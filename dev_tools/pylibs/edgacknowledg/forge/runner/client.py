# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The runner client (``RunnerClient``): runs commands in a runner
container (see ``runner.server``), over one long-lived connection.

The service has one, for its builds; each of the publisher's pool processes
has one, for the commands its handlers run (through ``SandboxContext``). A
client connects when first used, and again when its connection goes (which
fails the commands in flight on it: the server kills them).
'''

import asyncio
import json
import logging

from pathlib import Path
from typing import Any, Callable, Mapping, Optional, Sequence

from websockets.asyncio.client import ClientConnection, connect
from websockets.exceptions import ConnectionClosed, InvalidHandshake

from edgacknowledg.client.shared import CompletedCommand
from edgacknowledg.forge.runner.server import ID_BYTES, OUTPUT_CHUNK

_log = logging.getLogger(__name__)

# Seconds to wait for the runner to answer beyond the command's own limit
# (it enforces that one).
_ANSWER_GRACE_S = 30.0
_OPEN_TIMEOUT_S = 10.0

__all__ = ['CompletedCommand', 'RunnerClient', 'RunnerError']


class RunnerError(Exception):
  '''The runner couldn't be reached, or refused the command.'''


class _Pending:
  '''A request awaiting its answer.'''

  def __init__(self, output: Optional[Callable[[bytes], None]]) -> None:
    self.answer: asyncio.Future[dict[str, Any]] = (
      asyncio.get_running_loop().create_future()
    )
    self.output = output


class RunnerClient:
  '''The client of the runner server at ``url``, e.g.
  ``ws://builder:2730``.
  '''

  def __init__(self, url: str) -> None:
    self.url = url
    self._connection: Optional[ClientConnection] = None
    self._reader: Optional[asyncio.Task[None]] = None
    self._connecting: Optional[asyncio.Lock] = None
    self._pending: dict[int, _Pending] = {}
    self._next_id = 0

  def __repr__(self) -> str:
    return f"RunnerClient({self.url!r})"

  async def describe(self) -> dict[str, Any]:
    '''What the runner allows.'''
    value = await self._request({'op': 'describe'},
                                timeout = _OPEN_TIMEOUT_S + _ANSWER_GRACE_S)
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
    to ``output`` as they come). Cancelling this kills the command.
    '''
    collected = bytearray()

    def collect(chunk: bytes) -> None:
      collected.extend(chunk)
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
      bytes(collected),
      bool(value['timed_out'])
    )

  async def run_command(self,
                        argv: Sequence[str],
                        timeout: float) -> CompletedCommand:
    '''``run``, as a ``SandboxContext`` command runner.'''
    return await self.run(argv, timeout = timeout)

  async def close(self) -> None:
    '''Close the connection (killing whatever it still runs).'''
    connection = self._connection
    if connection is not None:
      await connection.close()
    if self._reader is not None:
      await asyncio.gather(self._reader, return_exceptions = True)

  async def _connected(self) -> ClientConnection:
    if self._connecting is None:
      self._connecting = asyncio.Lock()
    async with self._connecting:
      if self._connection is None:
        try:
          connection = await connect(
            self.url,
            open_timeout = _OPEN_TIMEOUT_S,
            max_size = ID_BYTES + OUTPUT_CHUNK + 1024
          )
        except (OSError, InvalidHandshake, asyncio.TimeoutError) as ex:
          raise RunnerError(
            f"the runner at {self.url} didn't answer: {ex!r}"
          ) from ex
        self._connection = connection
        self._reader = asyncio.create_task(self._read(connection))
      return self._connection

  async def _read(self, connection: ClientConnection) -> None:
    '''Hand each message to the request it answers, until the connection
    goes; then fail what is still waiting.
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
    except Exception:
      _log.exception('reading from the runner at %s failed', self.url)
    finally:
      if self._connection is connection:
        self._connection = None
      for pending in self._pending.values():
        if not pending.answer.done():
          pending.answer.set_exception(
            RunnerError(f"lost the runner at {self.url}")
          )

  async def _request(self,
                     request: dict[str, Any],
                     *,
                     timeout: float,
                     output: Optional[Callable[[bytes], None]] = None
                     ) -> dict[str, Any]:
    '''Send ``request``; pass any output to ``output`` and return the final
    answer.
    '''
    connection = await self._connected()
    request_id = self._next_id
    self._next_id = (self._next_id + 1) % 2 ** (8 * ID_BYTES)
    pending = _Pending(output)
    self._pending[request_id] = pending
    try:
      try:
        await connection.send(json.dumps({'id': request_id, **request}))
        value = await asyncio.wait_for(asyncio.shield(pending.answer),
                                       timeout)
      except asyncio.TimeoutError:
        await self._cancel(connection, request_id)
        raise RunnerError(
          f"the runner at {self.url} didn't answer in time"
        ) from None
      except asyncio.CancelledError:
        # The caller gave up: kill the command.
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
