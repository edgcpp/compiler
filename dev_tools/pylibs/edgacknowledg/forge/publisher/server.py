# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The publisher server (``PublisherServer``): serves the service's
publisher client over a WebSocket, in the publisher container (or, without
containers, in the service's process; see ``local``).

It answers reviewers by reading pull request content, so it runs away from
the service's credentials, database, and clones: it sees the review trees,
read-only, and holds no secret but the AcknowlEDG API key, which the client
sends. It answers each reviewer's request in its process pool (``pool``),
so no request can stall it; the pool processes send the commands run for
reviewers to the tools runner, so a tools runner they can't reach fails
only the requests running one.

Each connection gets a publisher of its own (an ``AcknowledgPublisher``,
with the settings the client sends first), which lasts as long as the
connection: when the connection goes, so do the publisher and everything it
served, and the client serves it all again on its next connection. A new
connection replaces the current one. See ``protocol`` for the messages.
'''

import argparse
import asyncio
import json
import logging
import os
import sys
import uuid

from pathlib import Path
from typing import Any, Optional

from websockets.asyncio.server import Server, ServerConnection, serve
from websockets.exceptions import ConnectionClosed

from edgacknowledg.forge.config import ENV_PREFIX, ConfigError
from edgacknowledg.forge.publisher import PublisherUnavailableError
from edgacknowledg.forge.publisher.acknowledg import AcknowledgPublisher
from edgacknowledg.forge.publisher.pool import (
  DEFAULT_PROCESSES,
  DEFAULT_REQUEST_TIMEOUT_S,
  ProcessPool,
)
from edgacknowledg.forge.publisher.protocol import (
  MESSAGE_LIMIT,
  decode_review,
  decode_section,
)

_log = logging.getLogger(__name__)


class _ReportingPublisher(AcknowledgPublisher):
  '''Also reports its AcknowlEDG connection to the client.'''

  def __init__(self,
               outgoing: 'asyncio.Queue[dict[str, Any]]',
               **kwargs: Any) -> None:
    super().__init__(**kwargs)
    self._outgoing = outgoing

  def connected(self) -> None:
    super().connected()
    self._outgoing.put_nowait({'event': 'connected'})

  def disconnected(self) -> None:
    super().disconnected()
    self._outgoing.put_nowait({'event': 'disconnected'})


async def _handle(publisher: AcknowledgPublisher,
                  request: dict[str, Any]) -> None:
  operation = request.get('op')
  if operation == 'sync':
    await publisher.sync(
      decode_review(request['review']),
      [decode_section(section) for section in request['sections']]
    )
  elif operation == 'remove':
    await publisher.remove(uuid.UUID(request['review_id']))
  else:
    raise ValueError(f"unknown operation: {operation!r}")


class _Session:
  '''One connection, and the publisher serving it.'''

  def __init__(self,
               connection: ServerConnection,
               server: 'PublisherServer') -> None:
    self.connection = connection
    self._server = server
    # Everything sent to the client, in order: answers and events.
    self._outgoing: asyncio.Queue[dict[str, Any]] = asyncio.Queue()

  async def run(self) -> None:
    settings = json.loads(await self.connection.recv())
    publisher = _ReportingPublisher(
      self._outgoing,
      api_host = settings['api_host'],
      api_port = int(settings['api_port']),
      api_key = settings['api_key'],
      ui_host = settings['ui_host'],
      ui_port = int(settings['ui_port']),
      pool = self._server.pool,
      reviews_dir = Path(settings['reviews_dir'])
    )
    del settings
    tasks = [
      asyncio.create_task(publisher.run()),
      asyncio.create_task(self._send()),
      asyncio.create_task(self._serve_requests(publisher)),
    ]
    try:
      # Until the connection goes (or the publisher fails).
      await asyncio.wait(tasks, return_when = asyncio.FIRST_COMPLETED)
    finally:
      for task in tasks:
        task.cancel()
      await asyncio.gather(*tasks, return_exceptions = True)

  async def _serve_requests(self, publisher: AcknowledgPublisher) -> None:
    '''Answer requests, one at a time, until the client goes.'''
    try:
      async for message in self.connection:
        request = json.loads(message)
        answer: dict[str, Any]
        try:
          await _handle(publisher, request)
          answer = {'ok': True}
        except PublisherUnavailableError as ex:
          answer = {'error': 'unavailable', 'message': str(ex)}
        except Exception as ex:
          _log.exception('request %s failed', request.get('op'))
          answer = {'error': 'failed', 'message': f"{type(ex).__name__}: {ex}"}
        self._outgoing.put_nowait(answer)
    except ConnectionClosed:
      pass

  async def _send(self) -> None:
    try:
      while True:
        await self.connection.send(json.dumps(await self._outgoing.get()))
    except ConnectionClosed:
      pass


class PublisherServer:
  '''Serves a publisher to each connection from the client, answering its
  reviewers in a pool of ``processes`` processes, whose commands run in the
  tools runner at ``tools_service_url``.
  '''

  def __init__(self,
               *,
               tools_service_url: str,
               processes: int = DEFAULT_PROCESSES,
               request_timeout: float = DEFAULT_REQUEST_TIMEOUT_S) -> None:
    # Shared by every connection's publisher, for the server's life.
    self.pool = ProcessPool(
      tools_service_url = tools_service_url,
      processes = processes,
      request_timeout = request_timeout
    )
    self._current: Optional[_Session] = None

  async def start(self, host: str, port: int) -> Server:
    '''Start the pool, and serving (port 0: any free one); close the
    server, then ``close``, to stop.
    '''
    await self.pool.start()
    try:
      return await serve(self._handle, host, port, max_size = MESSAGE_LIMIT)
    except BaseException:
      await self.pool.close()
      raise

  async def close(self) -> None:
    '''Stop the pool.'''
    await self.pool.close()

  async def serve(self, host: str, port: int) -> None:
    try:
      async with await self.start(host, port) as server:
        await server.serve_forever()
    finally:
      await self.close()

  def drop_connection(self) -> None:
    '''Drop the current connection, if any (the client reconnects).'''
    if self._current is not None:
      self._current.connection.transport.abort()

  async def _handle(self, connection: ServerConnection) -> None:
    if self._current is not None:
      _log.warning('a new connection replaces the current one')
      self.drop_connection()
    session = _Session(connection, self)
    self._current = session
    _log.info('serving a publisher for a new connection')
    try:
      await session.run()
    finally:
      if self._current is session:
        self._current = None
      _log.info('the connection, and its publisher, are gone')


def main() -> None:
  '''The publisher container's entry point.'''
  parser = argparse.ArgumentParser(
    description = 'Serve a publisher to each connection from the service.'
  )
  parser.add_argument(
    '--host',
    default = '127.0.0.1',
    help = 'the address to listen on (default: loopback only; in a '
           'container, its network\'s)'
  )
  parser.add_argument('--port', type = int, required = True)
  parser.add_argument(
    '--processes',
    type = int,
    default = DEFAULT_PROCESSES,
    help = 'pool processes answering reviewers (default: %(default)s)'
  )
  parser.add_argument(
    '--request-timeout',
    type = float,
    default = DEFAULT_REQUEST_TIMEOUT_S,
    help = 'seconds a pool process may take to answer a request, before '
           'it is replaced (default: %(default)s)'
  )
  args = parser.parse_args()
  logging.basicConfig(
    level = logging.INFO,
    format = '%(levelname)s %(name)s: %(message)s'
  )
  tools_service_url = os.environ.get(
    f"{ENV_PREFIX}TOOLS_SERVICE_URL", ''
  ).strip()
  if not tools_service_url.startswith('ws://'):
    print(f"configuration error: {ENV_PREFIX}TOOLS_SERVICE_URL must be a "
          f"ws:// URL", file = sys.stderr)
    sys.exit(2)
  server = PublisherServer(
    tools_service_url = tools_service_url,
    processes = args.processes,
    request_timeout = args.request_timeout
  )
  try:
    asyncio.run(server.serve(args.host, args.port))
  except KeyboardInterrupt:
    pass
