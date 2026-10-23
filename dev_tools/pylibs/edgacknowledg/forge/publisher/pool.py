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

- ``acknowledg.publish`` registers a ``UnixSocketHandler`` per section,
  which stands in for the section's handler: it passes each request, with
  the section it is for, to the pool, and relays the replies.
- ``ProcessPool`` keeps a fixed number of pool processes, each with one end
  of a socket pair as its channel. A process answers one request at a
  time, so requests queue, in order, and each process has a dispatcher
  task that takes the next one only once its process has answered the
  last: no process is given a request while busy with another.
- A request has a deadline, counted from when its process is given it (so
  never including another's work). A process that misses it is killed
  (with whatever it started) and replaced; so is one that exits. Either
  way the request fails, and the client library answers the reviewer with
  its failure reply.
- A request abandoned (cancelled, say, as its section is removed) before
  it is given to a process is skipped; one abandoned after is still
  answered (its process stays busy with it until then), and the answer
  dropped.

Messages on a channel are JSON objects, one per line: a process first says
``{"ready": true}``; then the server sends ``{"id", "section": ...,
"request": ...}``, and the process answers ``{"id", "replies": [...]}``,
plus ``"error": "illegal-access" | "failed"`` (and ``"message"``,
``"reviewer_id"``) if the handler raised; an answer to another id means
the two lost track, and the process is replaced. A process that exits
before it is ready has failed to start, and is retried after a delay,
doubling from 1 s up to 60 s; meanwhile, the other processes take the
requests, and while none can, requests fail at once.
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
from edgacknowledg.client.shared import (
  Channel,
  IllegalFileAccessError,
  reviewer_of,
)
from edgacknowledg.forge.config import ENV_PREFIX
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.publisher import SUBSCRIPTION_TYPES
from edgacknowledg.forge.publisher.protocol import (
  MESSAGE_LIMIT,
  encode_section,
)

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
  exited, or isn't running, or the handler failed. ``UnixSocketHandler``
  records the reviewer who made it, whom the failure reply is for.
  '''
  reviewer_id: Optional[str] = None


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
class _Job:
  '''A request waiting for, or being answered by, a pool process.'''
  id: int
  section: SectionRecord
  request: Any
  answered: 'asyncio.Future[dict[str, Any]]'


@dataclass
class _Slot:
  '''One place in the pool, and the process filling it.'''
  index: int
  process: Optional[asyncio.subprocess.Process] = None
  messages: Optional[asyncio.StreamReader] = None
  channel: Optional[asyncio.StreamWriter] = None
  # The task giving its process requests, one at a time.
  dispatcher: Optional['asyncio.Task[None]'] = None
  # Starts that failed in a row, and when the next may be tried.
  failed_starts: int = 0
  not_before: float = 0.0

  def running(self) -> bool:
    '''Whether its process can take a request (``_kill`` closes the
    channel at once, before the process is seen to exit).
    '''
    return (self.process is not None and self.process.returncode is None and
            self.channel is not None)

  def starting_later(self) -> bool:
    '''Whether its process failed to start, and isn't to be tried again
    yet.
    '''
    return not self.running() and time.monotonic() < self.not_before


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
    # Requests not yet given to a process, in order.
    self._jobs: asyncio.Queue[_Job] = asyncio.Queue()
    self._next_id = 0

  async def start(self) -> None:
    '''Start every process, and its dispatcher; raise ``PoolError`` if a
    process fails to start (its dispatcher tries again later).
    '''
    try:
      for slot in self._slots:
        await self._start(slot)
    finally:
      # Afterwards: a dispatcher starts its process itself, should it not
      # be running.
      for slot in self._slots:
        slot.dispatcher = asyncio.create_task(self._dispatch(slot))

  async def close(self) -> None:
    '''Stop every dispatcher and process; fail the requests still queued.'''
    for slot in self._slots:
      if slot.dispatcher is not None:
        slot.dispatcher.cancel()
        await asyncio.gather(slot.dispatcher, return_exceptions = True)
        slot.dispatcher = None
      await self._stop(slot)
    while not self._jobs.empty():
      job = self._jobs.get_nowait()
      if not job.answered.done():
        job.answered.set_exception(PoolError('the pool is closing'))

  def pids(self) -> list[int]:
    return [slot.process.pid for slot in self._slots
            if slot.process is not None and slot.running()]

  async def answer(self,
                   section: SectionRecord,
                   request: Any) -> Answer:
    '''Have the next free process answer ``request`` for ``section``;
    raise ``PoolError`` if it can't. Cancelling this abandons the request.
    '''
    if all(slot.starting_later() for slot in self._slots):
      raise PoolError('every pool process is restarting')
    if isinstance(request, bytes):
      request = request.decode()
    job = _Job(
      id = self._next_id,
      section = section,
      request = request,
      answered = asyncio.get_running_loop().create_future()
    )
    self._next_id += 1
    self._jobs.put_nowait(job)
    value = await job.answered
    return Answer(
      replies = list(value.get('replies', [])),
      error = value.get('error'),
      message = str(value.get('message', '')),
      reviewer_id = value.get('reviewer_id')
    )

  async def _dispatch(self, slot: _Slot) -> None:
    '''Give ``slot``'s process the queued requests, one at a time, each
    once it has answered the last, until cancelled.
    '''
    while True:
      # A running process first (replaced at once if it went), so it takes
      # requests only when it can answer them; while it can't start, the
      # others take them.
      if not slot.running():
        try:
          await self._replace(slot)
        except PoolError:
          await asyncio.sleep(max(slot.not_before - time.monotonic(), 0.1))
          continue
      job = await self._jobs.get()
      if job.answered.done():
        # Abandoned before its turn.
        continue
      try:
        value = await self._ask(slot, job)
      except PoolError as ex:
        # Its process is replaced before the next request (stopping one
        # that couldn't start does nothing).
        await self._stop(slot)
        if not job.answered.done():
          job.answered.set_exception(ex)
        continue
      except BaseException:
        if not job.answered.done():
          job.answered.set_exception(PoolError('the pool is closing'))
        raise
      # Unless abandoned since it was given.
      if not job.answered.done():
        job.answered.set_result(value)

  async def _ask(self, slot: _Slot, job: _Job) -> dict[str, Any]:
    '''Have ``slot``'s process, idle, answer ``job``; raise ``PoolError``
    if it can't (its process then at fault, unless it couldn't start).
    '''
    if not slot.running():
      await self._replace(slot)
    assert slot.channel is not None and slot.messages is not None
    try:
      slot.channel.write(
        (json.dumps({
          'id': job.id,
          'section': encode_section(job.section),
          'request': job.request,
        }) + '\n').encode()
      )
      await slot.channel.drain()
      line = await asyncio.wait_for(slot.messages.readline(),
                                    self._request_timeout)
      value = json.loads(line) if line != b'' else None
    except asyncio.TimeoutError:
      _log.error('pool process %d missed the %g s deadline', slot.index,
                 self._request_timeout)
      raise PoolError('the request timed out') from None
    except (BrokenPipeError, ConnectionResetError, ValueError) as ex:
      # ValueError: an oversized or malformed answer.
      raise PoolError(f"pool process {slot.index} failed: {ex}") from ex
    if value is None:
      _log.error('pool process %d exited while answering', slot.index)
      raise PoolError('the pool process exited')
    if not isinstance(value, dict) or value.get('id') != job.id:
      raise PoolError(f"pool process {slot.index} answered another request")
    return value

  async def _replace(self, slot: _Slot) -> None:
    '''Replace ``slot``'s process, which isn't running.'''
    if slot.process is not None:
      returncode = await self._stop(slot)
      slot.process = None
      _log.warning('pool process %d stopped (%s); replacing it',
                   slot.index, returncode)
    if time.monotonic() < slot.not_before:
      raise PoolError(f"pool process {slot.index} is restarting")
    await self._start(slot)

  async def _start(self, slot: _Slot) -> None:
    '''Start ``slot``'s process and wait until it is ready; raise
    ``PoolError`` (and delay the next try) if it isn't, whatever the
    reason (e.g. no process could be started at all).
    '''
    try:
      await self._launch(slot)
    except Exception as ex:
      returncode = await self._stop(slot)
      slot.process = None
      slot.failed_starts += 1
      delay = min(2.0 ** (slot.failed_starts - 1), _MAX_RESTART_DELAY_S)
      slot.not_before = time.monotonic() + delay
      _log.error('pool process %d failed to start (%s; exit status %s); '
                 'trying again in %g s', slot.index, ex, returncode, delay)
      raise PoolError(f"pool process {slot.index} failed to start") from ex
    slot.failed_starts = 0

  async def _launch(self, slot: _Slot) -> None:
    '''Start ``slot``'s process and wait until it is ready; raise if it
    isn't.
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
    # Recorded at once, so it is stopped whatever happens next.
    slot.process = process
    try:
      slot.messages, slot.channel = await asyncio.open_unix_connection(
        sock = parent_end,
        limit = MESSAGE_LIMIT
      )
      ready = await asyncio.wait_for(slot.messages.readline(),
                                     _READY_TIMEOUT_S)
    except asyncio.TimeoutError:
      ready = b''
    except BaseException:
      # Abandoned before it was ready: its ready line mustn't be taken for
      # a request's answer.
      self._kill(slot)
      raise
    if ready.strip() != b'{"ready": true}':
      raise PoolError('it never said it was ready')
    _log.info('started pool process %d (pid %d)', slot.index, process.pid)

  def _kill(self, slot: _Slot) -> None:
    '''Kill ``slot``'s process and whatever it started, and close its
    channel, without waiting for it to exit (``_stop`` waits).
    '''
    if slot.channel is not None:
      slot.channel.close()
    slot.channel = None
    slot.messages = None
    process = slot.process
    if process is not None and process.returncode is None:
      try:
        os.killpg(process.pid, signal.SIGKILL)
      except (ProcessLookupError, PermissionError):
        pass

  async def _stop(self, slot: _Slot) -> Optional[int]:
    '''Kill ``slot``'s process and whatever it started; return how it
    exited.
    '''
    self._kill(slot)
    process = slot.process
    if process is None:
      return None
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
    reviewer_id = reviewer_of(request)
    if reviewer_id is None:
      # The server's (e.g. its error for a reply it refused), which the
      # stock handlers don't answer: no need to bother a process.
      return
    try:
      answer = await self._pool.answer(self._section, request)
    except PoolError as ex:
      ex.reviewer_id = reviewer_id
      raise
    for reply in answer.replies:
      await channel.send(reply)
    if answer.error == 'illegal-access':
      illegal = IllegalFileAccessError(answer.message)
      illegal.reviewer_id = answer.reviewer_id
      raise illegal
    if answer.error is not None:
      failed = PoolError(f"the request failed: {answer.message}")
      failed.reviewer_id = reviewer_id
      raise failed
