# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Resilient WebSocket connections with exponential backoff.'''

import asyncio
import random

from typing import (
  Any,
  AsyncIterator,
  Dict,
  Optional,
)

try:
  from typing import Protocol
except ImportError:
  try:
    from typing_extensions import Protocol  # type: ignore
  except ImportError:
    Protocol = None  # type: ignore[misc,assignment]

import websockets

try:
  from websockets.asyncio.client import (  # type: ignore
    ClientConnection,
    connect,
  )
  _LEGACY_WEBSOCKETS = False
except Exception:
  from websockets.client import connect  # type: ignore
  ClientConnection = Any  # type: ignore[misc,assignment]
  _LEGACY_WEBSOCKETS = True

from websockets import ConnectionClosed

DEFAULT_INITIAL_BACKOFF_S = 1.0
DEFAULT_MAX_BACKOFF_S = 60.0
# Extra random delay mixed into reconnect sleeps to avoid synchronized floods.
DEFAULT_RECONNECT_JITTER_S = 3.0
# Per-attempt TCP/handshake timeout for ``connect()``.
DEFAULT_OPEN_TIMEOUT_S = 10.0


def _connection_closed() -> ConnectionClosed:
  # websockets < 10.0 constructs with (code, reason); 10.0+ uses (rcvd, sent)
  # close frames (None = no frame). 1006 is abnormal closure, matching the
  # modern ``code`` property when ``rcvd`` is None.
  if int(websockets.__version__.split('.', 1)[0]) < 10:
    return ConnectionClosed(1006, '')  # type: ignore[call-arg,arg-type]
  return ConnectionClosed(None, None)  # type: ignore[call-arg,arg-type]


if Protocol is not None:
  class ConnectionStatusListener(Protocol):
    '''Callbacks for resilient websocket lifecycle changes.'''

    def connected(self) -> None:
      '''The socket is open and ready for traffic.'''

    def attempting_reconnect(self) -> None:
      '''A connection attempt or reconnect is in progress.'''

    def disconnected(self) -> None:
      '''The live socket was lost (or never established before giving up).'''
else:
  class ConnectionStatusListener:  # type: ignore[no-redef]
    '''Callbacks for resilient websocket lifecycle changes.'''

    def connected(self) -> None:
      pass

    def attempting_reconnect(self) -> None:
      pass

    def disconnected(self) -> None:
      pass


def websocket_connect_kwargs(api_key: Optional[str] = None
                             ) -> Dict[str, Any]:
  '''Return extra kwargs for websocket connect (API key header when set).'''
  if not api_key:
    return {}
  headers = { 'X-AcknowlEDG-API-Key': api_key }
  if _LEGACY_WEBSOCKETS:
    return { 'extra_headers': headers }
  return { 'additional_headers': headers }


def next_backoff(current: float,
                 *,
                 initial: float = DEFAULT_INITIAL_BACKOFF_S,
                 maximum: float = DEFAULT_MAX_BACKOFF_S) -> float:
  '''Return the next exponential backoff delay, capped at ``maximum``.'''
  if current <= 0:
    return initial
  return min(current * 2.0, maximum)


def reconnect_delay(backoff: float,
                    *,
                    jitter: float = DEFAULT_RECONNECT_JITTER_S) -> float:
  '''Return ``backoff`` plus a random delay in ``[0, jitter]`` seconds.'''
  if jitter <= 0:
    return backoff
  return backoff + random.uniform(0.0, jitter)


class ResilientWebsocket:
  '''Async iterator that reconnects with exponential backoff.

  Use like modern ``websockets.connect``::

      async for websocket in ResilientWebsocket(url, connect_kwargs):
        try:
          ...
        except ConnectionClosed:
          continue

  Opening failures sleep with exponential backoff plus random jitter. After a
  yielded connection ends, a short random delay runs before the next open so
  reconnect timers do not align across clients. Call ``stop()`` (or ``break``)
  to end reconnecting.
  '''

  def __init__(self,
               url: str,
               connect_kwargs: Optional[Dict[str, Any]] = None,
               *,
               initial_backoff: float = DEFAULT_INITIAL_BACKOFF_S,
               max_backoff: float = DEFAULT_MAX_BACKOFF_S,
               reconnect_jitter: float = DEFAULT_RECONNECT_JITTER_S,
               open_timeout: Optional[float] = DEFAULT_OPEN_TIMEOUT_S,
               status_listener: Optional[ConnectionStatusListener] = None
               ) -> None:
    self._url = url
    self._connect_kwargs = dict(connect_kwargs or {})
    self._initial_backoff = initial_backoff
    self._max_backoff = max_backoff
    self._reconnect_jitter = reconnect_jitter
    self._open_timeout = open_timeout
    self._status_listener = status_listener

    self._websocket = None  # type: Optional[ClientConnection]
    self._closed = False
    self._connected = asyncio.Event()
    self._wake = asyncio.Event()
    self._send_lock = asyncio.Lock()
    self._ever_connected = False

  @property
  def legacy(self) -> bool:
    return _LEGACY_WEBSOCKETS

  @property
  def closed(self) -> bool:
    return self._closed

  def __aiter__(self) -> AsyncIterator[ClientConnection]:
    return self._iterate()

  def _notify_connected(self) -> None:
    listener = self._status_listener
    if listener is not None:
      listener.connected()

  def _notify_attempting_reconnect(self) -> None:
    listener = self._status_listener
    if listener is not None:
      listener.attempting_reconnect()

  def _notify_disconnected(self) -> None:
    listener = self._status_listener
    if listener is not None:
      listener.disconnected()

  async def _sleep(self, delay: float) -> bool:
    '''Sleep ``delay`` seconds, or return early if ``stop()`` was called.

    Returns ``True`` if stopped, ``False`` if the full delay elapsed.
    '''
    if delay <= 0:
      return self._closed
    self._wake.clear()
    try:
      await asyncio.wait_for(self._wake.wait(), timeout = delay)
      return True
    except asyncio.TimeoutError:
      return False

  async def _connect_once(self) -> ClientConnection:
    '''Open one connection, applying ``open_timeout`` when set.'''
    connect_coro = connect(self._url, **self._connect_kwargs)
    if self._open_timeout is None:
      return await connect_coro
    return await asyncio.wait_for(connect_coro, timeout = self._open_timeout)

  async def _open(self) -> ClientConnection:
    backoff = self._initial_backoff
    attempted = False
    while not self._closed:
      if attempted:
        self._notify_attempting_reconnect()
      attempted = True
      try:
        return await self._connect_once()
      except asyncio.CancelledError:
        raise
      except Exception:
        if self._closed:
          break
        delay = reconnect_delay(
          backoff,
          jitter = self._reconnect_jitter
        )
        if await self._sleep(delay):
          break
        backoff = next_backoff(
          backoff,
          initial = self._initial_backoff,
          maximum = self._max_backoff
        )
    raise _connection_closed()

  async def _iterate(self) -> AsyncIterator[ClientConnection]:
    while not self._closed:
      try:
        websocket = await self._open()
      except ConnectionClosed:
        if self._ever_connected:
          self._notify_disconnected()
        return
      except asyncio.CancelledError:
        raise

      self._websocket = websocket
      self._ever_connected = True
      self._connected.set()
      self._notify_connected()
      try:
        yield websocket
      finally:
        self._websocket = None
        self._connected.clear()
        self._notify_disconnected()
        try:
          await websocket.close()
        except Exception:
          pass

      if self._closed:
        return

      # Desynchronize the next open after a drop / loop continue.
      self._notify_attempting_reconnect()
      delay = reconnect_delay(0.0, jitter = self._reconnect_jitter)
      if await self._sleep(delay):
        return

  async def wait_connected(self) -> None:
    '''Block until a live connection exists, or the socket was stopped.'''
    while not self._closed:
      if self._websocket is not None:
        return
      await self._connected.wait()
    raise _connection_closed()

  async def stop(self) -> None:
    '''Stop reconnecting and close any active connection.'''
    self._closed = True
    self._wake.set()
    websocket = self._websocket
    if websocket is not None:
      try:
        await websocket.close()
      except Exception:
        pass
    self._websocket = None
    # Wake any waiters so they observe ``closed``.
    self._connected.set()

  async def send(self, data: Any) -> None:
    '''Send on the current connection, waiting for reconnect if needed.'''
    await self.wait_connected()
    websocket = self._websocket
    if websocket is None:
      raise _connection_closed()
    async with self._send_lock:
      await websocket.send(data)

  async def send_if_connected(self, data: Any) -> None:
    '''Send on the current connection, if there is one; otherwise drop
    ``data``, for what doesn't outlast a connection.
    '''
    websocket = self._websocket
    if websocket is None:
      return
    async with self._send_lock:
      await websocket.send(data)
