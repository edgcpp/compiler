# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Keeping one connection to a server in another sandbox container
(``ForgeSandboxClient``), whichever protocol the server speaks: each client
of one, ``RunnerClient`` (``runner.client``) and ``PublisherClient``
(``publisher.client``), has one, and serves its protocol over each
connection it makes.

While it runs (``start`` until ``close``), it keeps one connection to its
server, and never gives up on it: the websockets library reconnects
whenever it goes, or can't be made, backing off exponentially. Connecting
failing in a way retrying may not fix (e.g. being refused) is logged, and
retried all the same: the server may yet be fixed.
'''

import asyncio
import logging

from typing import Awaitable, Callable, Generator, Optional

from websockets.asyncio.client import ClientConnection, connect
from websockets.client import backoff, process_exception
from websockets.exceptions import InvalidURI
from websockets.uri import parse_uri

_log = logging.getLogger(__name__)

# Seconds to wait before connecting again, so that a connection that keeps
# going at once (e.g. while serving it fails) doesn't spin.
RECONNECT_DELAY = 1.0


class ServiceUnavailableError(Exception):
  '''Raised when a server in another sandbox container can't take a request
  right now (retry later).
  '''


class ForgeSandboxClient:
  '''Keeps a connection to the server at ``url`` (``peer`` names it, for
  messages), while it runs; a connection takes at most ``open_timeout``
  seconds to open, and takes messages of up to ``max_size`` bytes.

  ``serve`` serves each connection until it goes (raising has it go, to
  connect again); requests wait for one (``wait_connected``), or check
  there is one (``connected``).

  Raises ``ValueError`` if ``url`` isn't a WebSocket URL: retrying
  wouldn't fix that.
  '''

  def __init__(self,
               url: str,
               *,
               peer: str,
               serve: Callable[[ClientConnection], Awaitable[None]],
               open_timeout: float,
               max_size: int) -> None:
    try:
      parse_uri(url)
    except (InvalidURI, ValueError) as ex:
      raise ValueError(
        f"the URL of {peer}, {url!r}, isn't valid: {ex}"
      ) from ex
    self.url = url
    self._peer = peer
    self._serve = serve
    self._open_timeout = open_timeout
    self._max_size = max_size
    # The task keeping the connection, while it runs; the connection, and
    # whether there is one, are its alone to change.
    self._running: Optional[asyncio.Task[None]] = None
    self._connection: Optional[ClientConnection] = None
    # Set while connected, or once closed.
    self._connected = asyncio.Event()
    self._closed = False

  def start(self) -> None:
    '''Start keeping the connection (until ``close``).'''
    if self._running is None:
      self._closed = False
      self._connected.clear()
      self._running = asyncio.create_task(self._keep_connected())

  async def close(self) -> None:
    '''Close the connection, and stop keeping it; requests waiting for it
    fail.
    '''
    self._closed = True
    try:
      if self._running is not None:
        self._running.cancel()
        await asyncio.gather(self._running, return_exceptions = True)
        self._running = None
    finally:
      self._connected.set()

  @property
  def connection(self) -> Optional[ClientConnection]:
    '''The connection being served, if any.'''
    return self._connection

  @property
  def connected(self) -> bool:
    '''Whether there is a connection now.'''
    return (not self._closed and self._connection is not None and
            self._connected.is_set())

  async def wait_connected(self) -> ClientConnection:
    '''The connection, once there is one; raises
    ``ServiceUnavailableError`` if closed first.
    '''
    while True:
      await self._connected.wait()
      if self._closed:
        raise ServiceUnavailableError(
          f"the client of {self._peer} at {self.url} is closed"
        )
      # It may have gone since the event was set: then wait for the next.
      connection = self._connection
      if connection is not None and self._connected.is_set():
        return connection

  def drop(self, connection: ClientConnection) -> None:
    '''Send ``connection`` nothing more, and have it go (to connect
    again).
    '''
    if self._connection is connection:
      self._connection = None
      self._connected.clear()
    connection.transport.abort()

  # Connection

  @staticmethod
  def _reconnect_delays() -> Generator[float, None, None]:
    '''Seconds to wait between attempts to connect.'''
    return backoff()

  def _retry(self, ex: Exception) -> Optional[Exception]:
    '''Have websockets retry connecting, whatever the error.'''
    if process_exception(ex) is not None:
      _log.warning("can't connect to %s at %s (%s); retrying", self._peer,
                   self.url, ex)
    return None

  async def _keep_connected(self) -> None:
    '''Keep a connection to the server, until cancelled.'''
    async for connection in connect(
        self.url,
        open_timeout = self._open_timeout,
        max_size = self._max_size,
        process_exception = self._retry,
        reconnect_delays = self._reconnect_delays):
      self._connection = connection
      self._connected.set()
      try:
        await self._serve(connection)
      except Exception:
        _log.exception('serving the connection to %s at %s failed',
                       self._peer, self.url)
      finally:
        self._connected.clear()
        self._connection = None
        connection.transport.abort()
        await connection.close()
      _log.warning('lost %s at %s; reconnecting', self._peer, self.url)
      await asyncio.sleep(RECONNECT_DELAY)
