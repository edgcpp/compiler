# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The process pool: where the publisher server answers reviewers.

Answering a reviewer reads (and parses, diffs, decompresses) pull request
content, which can take long, or forever, and would stall the server's
event loop, and with it every other reviewer and the AcknowlEDG connection.
So the server keeps only section management, and answers each request in a
pool process (``pool_process``):

- ``AcknowledgPublisher`` registers a ``UnixSocketHandler`` per section,
  which stands in for the section's handler: it passes each request, with
  the section it is for, to the pool, and relays the replies.
- ``ProcessPool`` keeps a fixed number of pool processes, each with one end
  of a socket pair as its channel, and sends each request to the next one
  in turn (round-robin). A process answers one request at a time; requests
  for a busy one wait their turn.
- A request has a deadline, counted from when its process starts on it. A
  process that misses it is killed (with whatever it started) and replaced;
  so is one that exits. Either way the request fails, and the client
  library answers the reviewer with its failure reply.

Messages on a channel are JSON objects, one per line: a process first says
``{"ready": true}``; then the server sends ``{"section": ..., "request":
...}``, and the process answers ``{"replies": [...]}``, plus ``"error":
"illegal-access" | "failed"`` (and ``"message"``, ``"reviewer_id"``) if the
handler raised. A process that exits before it is ready has failed to
start, and is retried after a delay, doubling from 1 s up to 60 s; until
then, requests for it fail at once.
'''

import asyncio
import importlib.util
import json
import logging
import os
import signal
import socket
import sys
import time

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional

from edgacknowledg import ProtocolMessage
from edgacknowledg.client.shared import Channel, IllegalFileAccessError
from edgacknowledg.forge.config import ENV_PREFIX
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.publisher import SUBSCRIPTION_TYPES
from edgacknowledg.forge.publisher.protocol import MESSAGE_LIMIT, encode_section

_log = logging.getLogger(__name__)

POOL_PROCESS_MODULE = 'edgacknowledg.forge.publisher.pool_process'

DEFAULT_PROCESSES = 4
# Seconds a pool process may take to answer one request. Longer than a
# command run for a reviewer may take (the tools runner's limit, 2 min).
DEFAULT_REQUEST_TIMEOUT_S = 150.0

# Seconds a process may take to be ready.
_READY_TIMEOUT_S = 60.0
_MAX_RESTART_DELAY_S = 60.0


class PoolError(Exception):
  '''A request the pool couldn't answer: its process missed the deadline,
  exited, or isn't running.
  '''


def python_paths() -> list[Path]:
  '''The directories this interpreter loads code from (which a pool process
  needs too): its installation, the virtual environment, ``sys.path``, and
  wherever the EDG packages actually live (e.g. an editable install's
  sources).
  '''
  paths = [Path(sys.prefix), Path(sys.base_prefix)]
  paths += [Path(entry) for entry in sys.path if entry != '']
  for name in ('edgacknowledg', 'edgtest', 'edgutil'):
    spec = importlib.util.find_spec(name)
    if spec is not None and spec.submodule_search_locations is not None:
      paths += [Path(location).parent
                for location in spec.submodule_search_locations]
  result: list[Path] = []
  for path in paths:
    path = path.absolute()
    if path.is_dir() and path not in result:
      result.append(path)
  return result


def _process_env(tools_service_url: str) -> dict[str, str]:
  '''A pool process's whole environment: nothing of the server's but
  ``PATH``, and the tools runner to use.
  '''
  return {
    'PATH': os.environ.get('PATH', '/usr/local/bin:/usr/bin:/bin'),
    'HOME': '/tmp',
    'PYTHONUTF8': '1',
    'PYTHONDONTWRITEBYTECODE': '1',
    'PYTHONPATH': os.pathsep.join(str(path) for path in python_paths()),
    f"{ENV_PREFIX}TOOLS_SERVICE_URL": tools_service_url,
  }


@dataclass
class Answer:
  '''A pool process's answer to one request.'''
  replies: list[ProtocolMessage]
  error: Optional[str] = None
  message: str = ''
  reviewer_id: Optional[str] = None


@dataclass
class _Slot:
  '''One place in the pool, and the process filling it.'''
  index: int
  # One request at a time; waiting requests queue here, in order.
  lock: asyncio.Lock = field(default_factory = asyncio.Lock)
  process: Optional[asyncio.subprocess.Process] = None
  messages: Optional[asyncio.StreamReader] = None
  channel: Optional[asyncio.StreamWriter] = None
  # Starts that failed in a row, and when the next may be tried.
  failed_starts: int = 0
  not_before: float = 0.0

  def running(self) -> bool:
    return self.process is not None and self.process.returncode is None


class ProcessPool:
  '''``processes`` pool processes, whose reviewer commands run in the
  tools runner at ``tools_service_url``, each answering a request within
  ``request_timeout`` seconds or being replaced.
  '''

  def __init__(self,
               *,
               tools_service_url: str,
               processes: int = DEFAULT_PROCESSES,
               request_timeout: float = DEFAULT_REQUEST_TIMEOUT_S) -> None:
    if processes < 1:
      raise ValueError('a pool needs at least one process')
    self._tools_service_url = tools_service_url
    self._request_timeout = request_timeout
    self._slots = [_Slot(index) for index in range(processes)]
    self._next = 0

  async def start(self) -> None:
    '''Start every process; raise ``PoolError`` if one fails to.'''
    for slot in self._slots:
      await self._start(slot)

  async def close(self) -> None:
    '''Stop every process.'''
    for slot in self._slots:
      await self._stop(slot)

  def pids(self) -> list[int]:
    return [slot.process.pid for slot in self._slots
            if slot.process is not None and slot.running()]

  async def answer(self,
                   section: SectionRecord,
                   request: Any) -> Answer:
    '''Have the next process (round-robin) answer ``request`` for
    ``section``; raise ``PoolError`` if it can't.
    '''
    slot = self._slots[self._next]
    self._next = (self._next + 1) % len(self._slots)
    if isinstance(request, bytes):
      request = request.decode()
    async with slot.lock:
      if not slot.running():
        await self._replace(slot)
      assert slot.channel is not None and slot.messages is not None
      try:
        slot.channel.write(
          (json.dumps({
            'section': encode_section(section),
            'request': request,
          }) + '\n').encode()
        )
        await slot.channel.drain()
        line = await asyncio.wait_for(slot.messages.readline(),
                                      self._request_timeout)
      except asyncio.TimeoutError:
        _log.error('pool process %d missed the %g s deadline; replacing it',
                   slot.index, self._request_timeout)
        await self._stop(slot)
        # Not a failure to start: replace it at once.
        slot.process = None
        raise PoolError('the request timed out') from None
      except (BrokenPipeError, ConnectionResetError, ValueError) as ex:
        await self._stop(slot)
        raise PoolError(f"pool process {slot.index} failed: {ex}") from ex
      if line == b'':
        returncode = await self._stop(slot)
        _log.error('pool process %d exited (%s) while answering',
                   slot.index, returncode)
        raise PoolError('the pool process exited')
      value = json.loads(line)
      return Answer(
        replies = list(value.get('replies', [])),
        error = value.get('error'),
        message = str(value.get('message', '')),
        reviewer_id = value.get('reviewer_id')
      )

  async def _replace(self, slot: _Slot) -> None:
    '''Replace ``slot``'s process, which isn't running.'''
    if slot.process is not None:
      returncode = await self._stop(slot)
      slot.process = None
      _log.warning('pool process %d exited (%s); replacing it',
                   slot.index, returncode)
    if time.monotonic() < slot.not_before:
      raise PoolError(f"pool process {slot.index} is restarting")
    await self._start(slot)

  async def _start(self, slot: _Slot) -> None:
    '''Start ``slot``'s process and wait until it is ready; raise
    ``PoolError`` (and delay the next try) if it isn't.
    '''
    parent_end, process_end = socket.socketpair()
    try:
      process = await asyncio.create_subprocess_exec(
        sys.executable, '-m', POOL_PROCESS_MODULE, str(process_end.fileno()),
        stdin = asyncio.subprocess.DEVNULL,
        env = _process_env(self._tools_service_url),
        pass_fds = (process_end.fileno(),),
        start_new_session = True
      )
    except BaseException:
      parent_end.close()
      raise
    finally:
      process_end.close()
    slot.messages, slot.channel = await asyncio.open_unix_connection(
      sock = parent_end,
      limit = MESSAGE_LIMIT
    )
    slot.process = process
    try:
      ready = await asyncio.wait_for(slot.messages.readline(),
                                     _READY_TIMEOUT_S)
    except asyncio.TimeoutError:
      ready = b''
    if ready.strip() != b'{"ready": true}':
      returncode = await self._stop(slot)
      slot.process = None
      slot.failed_starts += 1
      delay = min(2.0 ** (slot.failed_starts - 1), _MAX_RESTART_DELAY_S)
      slot.not_before = time.monotonic() + delay
      _log.error('pool process %d failed to start (%s); trying again in '
                 '%g s', slot.index, returncode, delay)
      raise PoolError(f"pool process {slot.index} failed to start")
    slot.failed_starts = 0
    _log.info('started pool process %d (pid %d)', slot.index, process.pid)

  async def _stop(self, slot: _Slot) -> Optional[int]:
    '''Kill ``slot``'s process and whatever it started; return how it
    exited.
    '''
    process = slot.process
    if slot.channel is not None:
      slot.channel.close()
    slot.channel = None
    slot.messages = None
    if process is None:
      return None
    if process.returncode is None:
      try:
        os.killpg(process.pid, signal.SIGKILL)
      except (ProcessLookupError, PermissionError):
        pass
    return await process.wait()


class UnixSocketHandler:
  '''Stands in, in the publisher server, for ``section``'s handler: answers
  each request through ``pool``.
  '''

  def __init__(self, section: SectionRecord, pool: ProcessPool) -> None:
    self._section = section
    self._pool = pool

  @property
  def section(self) -> SectionRecord:
    return self._section

  @property
  def subscription_type(self) -> str:
    return SUBSCRIPTION_TYPES[self._section.kind]

  @property
  def section_name(self) -> str:
    return self._section.name

  @property
  def slug(self) -> str:
    return self._section.slug

  async def handle_request(self, channel: Channel) -> None:
    request = await channel.recv()
    answer = await self._pool.answer(self._section, request)
    for reply in answer.replies:
      await channel.send(reply)
    if answer.error == 'illegal-access':
      error = IllegalFileAccessError(answer.message)
      error.reviewer_id = answer.reviewer_id
      raise error
    if answer.error is not None:
      raise PoolError(f"the request failed: {answer.message}")
