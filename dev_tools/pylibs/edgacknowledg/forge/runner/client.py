# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The runner client (``RunnerClient``): runs commands in a runner
container (see ``runner.server``), over one long-lived connection.

The service has one, for its builds; each of the publisher's pool processes
has one, for the commands its handlers run (through ``SandboxContext``).

While it runs (``async with``), a client keeps one connection to its
runner, which every command it runs shares: the websockets library
reconnects whenever it goes, backing off exponentially while it can't reach
the runner. Each request carries an ``id`` of the client's choosing, which
the runner's answer, and every chunk of the command's output, carry back,
so any number of commands run at once over it. A command waits for the
connection, within its own time limit; the connection going fails the
commands in flight on it, as the runner kills them.
'''

import asyncio
import json
import logging

from pathlib import Path
from types import TracebackType
from typing import Any, Callable, Mapping, Optional, Sequence, Type

from websockets.asyncio.client import ClientConnection, connect
from websockets.exceptions import ConnectionClosed

from edgacknowledg.client.shared import CompletedCommand
from edgacknowledg.forge.runner.server import ID_BYTES, OUTPUT_CHUNK

_log = logging.getLogger(__name__)

# Seconds to wait for the runner to answer beyond the command's own limit
# (it enforces that one).
_ANSWER_GRACE_S = 30.0
_OPEN_TIMEOUT_S = 10.0
# The most of a command's output ``run`` keeps (in bytes): its start, and
# (where its errors are) its last ``OUTPUT_TAIL_BYTES``.
MAX_OUTPUT_BYTES = 8 * 1024 * 1024
OUTPUT_TAIL_BYTES = 64 * 1024

__all__ = ['CompletedCommand', 'RunnerClient', 'RunnerError']


class RunnerError(Exception):
  '''The runner couldn't be reached, or refused the command.'''


class _Output:
  '''A command's output, kept within ``MAX_OUTPUT_BYTES``: what goes beyond
  is dropped from its middle.
  '''

  def __init__(self) -> None:
    self._head = bytearray()
    self._tail = bytearray()
    self._dropped = 0

  def add(self, chunk: bytes) -> None:
    room = MAX_OUTPUT_BYTES - OUTPUT_TAIL_BYTES - len(self._head)
    if room > 0:
      self._head += chunk[:room]
      chunk = chunk[room:]
    self._tail += chunk
    excess = len(self._tail) - OUTPUT_TAIL_BYTES
    if excess > 0:
      del self._tail[:excess]
      self._dropped += excess

  def value(self) -> bytes:
    if self._dropped == 0:
      return bytes(self._head + self._tail)
    return b''.join([
      self._head,
      f"\n[... {self._dropped} bytes of output dropped ...]\n".encode(),
      self._tail,
    ])


class _Pending:
  '''A request awaiting its answer.'''

  def __init__(self, output: Optional[Callable[[bytes], None]]) -> None:
    self.answer: asyncio.Future[dict[str, Any]] = (
      asyncio.get_running_loop().create_future()
    )
    self.output = output


class RunnerClient:
  '''The client of the runner server at ``url``, e.g.
  ``ws://builder:2730``, connected while it runs (``async with``, or
  ``start`` and ``close``); a connection takes at most ``open_timeout``
  seconds to open, and a runner that can't be reached fails ``describe``
  within it.
  '''

  def __init__(self,
               url: str,
               *,
               open_timeout: float = _OPEN_TIMEOUT_S) -> None:
    self.url = url
    self._open_timeout = open_timeout
    # The task keeping the connection, while the client runs; the
    # connection, and whether there is one, are its alone to change.
    self._running: Optional[asyncio.Task[None]] = None
    self._connection: Optional[ClientConnection] = None
    self._connected = asyncio.Event()
    # The requests sent on the connection, awaiting their answers, by id.
    self._pending: dict[int, _Pending] = {}
    self._next_id = 0

  def __repr__(self) -> str:
    return f"RunnerClient({self.url!r})"

  def start(self) -> None:
    '''Start keeping the connection (until ``close``).'''
    if self._running is None:
      self._running = asyncio.create_task(self._keep_connected())

  async def close(self) -> None:
    '''Close the connection (killing whatever it still runs), and stop
    keeping it.
    '''
    if self._running is not None:
      self._running.cancel()
      await asyncio.gather(self._running, return_exceptions = True)
      self._running = None

  async def __aenter__(self) -> 'RunnerClient':
    self.start()
    return self

  async def __aexit__(self,
                      exc_type: Optional[Type[BaseException]],
                      exc: Optional[BaseException],
                      tb: Optional[TracebackType]) -> None:
    await self.close()

  async def describe(self) -> dict[str, Any]:
    '''What the runner allows.'''
    value = await self._request({'op': 'describe'},
                                timeout = self._open_timeout)
    return dict(value['description'])

  async def run(self,
                argv: Sequence[str],
                *,
                cwd: Optional[Path] = None,
                env: Mapping[str, str] = {},
                timeout: float,
                output: Optional[Callable[[bytes], None]] = None
                ) -> CompletedCommand:
    '''Run ``argv`` in the runner (stdout and stderr together, also passed
    to ``output`` as they come); its output beyond ``MAX_OUTPUT_BYTES`` is
    dropped from the middle. Cancelling this kills the command.
    '''
    collected = _Output()

    def collect(chunk: bytes) -> None:
      collected.add(chunk)
      if output is not None:
        output(chunk)

    value = await self._request(
      {
        'op': 'run',
        'argv': list(argv),
        'cwd': str(cwd) if cwd is not None else None,
        'env': dict(env),
        'timeout': timeout,
      },
      timeout = timeout + _ANSWER_GRACE_S,
      output = collect
    )
    return CompletedCommand(
      int(value['returncode']),
      collected.value(),
      bool(value['timed_out'])
    )

  async def run_command(self,
                        argv: Sequence[str],
                        timeout: float) -> CompletedCommand:
    '''``run``, as a ``SandboxContext`` command runner.'''
    return await self.run(argv, timeout = timeout)

  async def _keep_connected(self) -> None:
    '''Keep a connection to the runner, until cancelled.'''
    async for connection in connect(
        self.url,
        open_timeout = self._open_timeout,
        max_size = ID_BYTES + OUTPUT_CHUNK + 1024):
      self._connection = connection
      self._connected.set()
      try:
        await self._read(connection)
      finally:
        self._connected.clear()
        self._connection = None
        for pending in self._pending.values():
          if not pending.answer.done():
            pending.answer.set_exception(
              RunnerError(f"lost the runner at {self.url}")
            )
      _log.warning('lost the runner at %s; reconnecting', self.url)

  async def _read(self, connection: ClientConnection) -> None:
    '''Hand each message to the request it answers, until the connection
    goes.
    '''
    try:
      async for message in connection:
        if isinstance(message, bytes):
          request_id = int.from_bytes(message[:ID_BYTES], 'big')
          pending = self._pending.get(request_id)
          if pending is not None and pending.output is not None:
            pending.output(message[ID_BYTES:])
          continue
        value = json.loads(message)
        pending = self._pending.get(value.get('id'))
        if pending is not None and not pending.answer.done():
          pending.answer.set_result(value)
    except ConnectionClosed:
      pass

  async def _request(self,
                     request: dict[str, Any],
                     *,
                     timeout: float,
                     output: Optional[Callable[[bytes], None]] = None
                     ) -> dict[str, Any]:
    '''Send ``request`` once connected; pass any output to ``output`` and
    return the final answer, all within ``timeout`` seconds.
    '''
    request_id = self._next_id
    self._next_id = (self._next_id + 1) % 2 ** (8 * ID_BYTES)
    pending = _Pending(output)
    connection: Optional[ClientConnection] = None
    try:
      async with asyncio.timeout(timeout):
        await self._connected.wait()
        connection = self._connection
        assert connection is not None
        self._pending[request_id] = pending
        await connection.send(json.dumps({'id': request_id, **request}))
        value = await asyncio.shield(pending.answer)
    except TimeoutError:
      if connection is not None:
        await self._cancel(connection, request_id)
      raise RunnerError(
        f"the runner at {self.url} didn't answer in time"
      ) from None
    except asyncio.CancelledError:
      # The caller gave up: kill the command.
      if connection is not None:
        await asyncio.shield(self._cancel(connection, request_id))
      raise
    except ConnectionClosed as ex:
      raise RunnerError(f"lost the runner at {self.url}: {ex}") from ex
    finally:
      self._pending.pop(request_id, None)
    if 'error' in value:
      raise RunnerError(f"the runner refused: {value['error']}")
    return value

  @staticmethod
  async def _cancel(connection: ClientConnection, request_id: int) -> None:
    try:
      await connection.send(json.dumps({'id': request_id, 'op': 'cancel'}))
    except ConnectionClosed:
      pass
