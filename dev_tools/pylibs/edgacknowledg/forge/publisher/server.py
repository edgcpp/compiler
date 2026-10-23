# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The publisher server (``create_publisher``): serves the service's
publisher client over a WebSocket at ``/`` (and health checks; see
``serving``), in the publisher container (or, without containers, in the
service's process; see ``local``).

It answers reviewers by reading pull request content, so it runs away from
the service's credentials, database, and clones: it sees the review trees,
read-only, and holds no secret but the AcknowlEDG API key, which the client
sends. It answers each reviewer's request in its process pool (``pool``),
so no request can stall it; the pool processes send the commands run for
reviewers to the tools runner, so a tools runner they can't reach fails
only the requests running one.

Connections only queue what to publish (``acknowledg.Wanted``); one
publishing task (``acknowledg.publish``, with the settings a client sends
first) publishes it, whenever it is connected to AcknowlEDG. What it
publishes outlasts any connection; a client that connects serves every
review stored in the service's database (again). See ``protocol`` for the
messages.
'''

import argparse
import asyncio
import json
import logging
import os
import sys
import uuid

from dataclasses import dataclass
from pathlib import Path
from typing import Any, AsyncIterator, Optional

from quart import Blueprint, websocket

from edgacknowledg.forge import serving
from edgacknowledg.forge.config import ENV_PREFIX, ConfigError
from edgacknowledg.forge.publisher.acknowledg import (
  Delist,
  Prune,
  Sync,
  Wanted,
  publish,
)
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


@dataclass(frozen = True)
class _Settings:
  '''What a client sends first: the publisher's settings.'''
  api_host: str
  api_port: int
  api_key: Optional[str]
  reviews_dir: Path


def _decode_settings(value: Any) -> _Settings:
  return _Settings(
    api_host = str(value['api_host']),
    api_port = int(value['api_port']),
    api_key = None if value['api_key'] is None else str(value['api_key']),
    reviews_dir = Path(value['reviews_dir'])
  )


def _answer(wanted: Wanted, request: dict[str, Any]) -> dict[str, Any]:
  '''Queue what ``request`` asks to publish; its answer.'''
  answer: dict[str, Any] = {'ok': True}
  try:
    operation = request.get('op')
    if operation == 'sync':
      wanted.put_nowait(Sync(
        decode_review(request['review']),
        [decode_section(section) for section in request['sections']]
      ))
    elif operation == 'delist':
      wanted.put_nowait(Delist(uuid.UUID(request['review_id'])))
    elif operation == 'prune':
      wanted.put_nowait(Prune(frozenset(
        uuid.UUID(value) for value in request['keep']
      )))
    else:
      raise ValueError(f"unknown operation: {operation!r}")
  except Exception as ex:
    _log.exception('request %s failed', request.get('op'))
    answer = {'error': 'failed', 'message': f"{type(ex).__name__}: {ex}"}
  return answer


async def _keep_publishing(settings: 'asyncio.Queue[_Settings]',
                           wanted: Wanted,
                           pool: ProcessPool) -> None:
  '''Publish what ``wanted`` asks for with the latest ``settings`` a client
  sent, until cancelled: from the first ones on; again with new ones, or
  should publishing fail (what it published goes with it; the clients
  serve it all again on connecting).
  '''
  current = await settings.get()
  while True:
    publishing = asyncio.create_task(publish(
      wanted,
      api_host = current.api_host,
      api_port = current.api_port,
      api_key = current.api_key,
      pool = pool,
      reviews_dir = current.reviews_dir
    ))
    changing = asyncio.create_task(settings.get())
    try:
      done, _ = await asyncio.wait({publishing, changing},
                                   return_when = asyncio.FIRST_COMPLETED)
    finally:
      for task in (publishing, changing):
        task.cancel()
      await asyncio.gather(publishing, changing, return_exceptions = True)
    if changing in done:
      _log.warning('restarting the publisher with new settings')
      current = changing.result()
    else:
      _log.error('the publisher stopped (%r); restarting it',
                 publishing.exception())
      await asyncio.sleep(1)


def create_publisher(*,
                     tools_service_url: str,
                     processes: int = DEFAULT_PROCESSES,
                     request_timeout: float = DEFAULT_REQUEST_TIMEOUT_S
                     ) -> Blueprint:
  '''The publisher server, publishing for its clients over a WebSocket at
  ``/``, answering reviewers in a pool of ``processes`` processes, whose
  commands run in the tools runner at ``tools_service_url``; both for as
  long as the app serves. Serve it with ``serving``, taking messages of up
  to ``MESSAGE_LIMIT``.
  '''
  blueprint = Blueprint('publisher', __name__)
  pool = ProcessPool(
    tools_service_url = tools_service_url,
    processes = processes,
    request_timeout = request_timeout
  )
  wanted: Wanted = asyncio.Queue()
  # The settings clients send, when they differ from the last ones.
  settings: asyncio.Queue[_Settings] = asyncio.Queue()
  latest_settings: Optional[_Settings] = None

  @blueprint.while_app_serving
  async def run_publishing() -> AsyncIterator[None]:
    await pool.start()
    publishing = asyncio.create_task(_keep_publishing(settings, wanted, pool))
    try:
      yield
    finally:
      publishing.cancel()
      await asyncio.gather(publishing, return_exceptions = True)
      await pool.close()

  @blueprint.websocket('/')
  async def serve_connection() -> None:
    '''Queue what the connection's client asks to publish, until the
    connection goes (Quart cancels this).
    '''
    nonlocal latest_settings
    client_settings = _decode_settings(json.loads(await websocket.receive()))
    if client_settings != latest_settings:
      latest_settings = client_settings
      settings.put_nowait(client_settings)
    _log.info('a client connected')
    try:
      while True:
        request = json.loads(await websocket.receive())
        await websocket.send(json.dumps(_answer(wanted, request)))
    finally:
      _log.info('a client disconnected')

  return blueprint


def main() -> None:
  '''The publisher container's entry point.'''
  parser = argparse.ArgumentParser(
    description = 'Publish for the service, over its connections.'
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
  publisher = create_publisher(
    tools_service_url = tools_service_url,
    processes = args.processes,
    request_timeout = args.request_timeout
  )
  try:
    asyncio.run(serving.serve(
      publisher,
      host = args.host,
      port = args.port,
      max_message_size = MESSAGE_LIMIT
    ))
  except KeyboardInterrupt:
    pass
