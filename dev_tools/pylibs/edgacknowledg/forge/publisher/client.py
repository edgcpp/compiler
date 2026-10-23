# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The publisher client (``PublisherClient``): how the service publishes,
through the publisher server in the publisher container (see
``publisher.server``).
'''

import asyncio
import json
import logging
import uuid

from pathlib import Path
from typing import Any, Optional, Sequence

from websockets.asyncio.client import ClientConnection, connect
from websockets.exceptions import ConnectionClosed

from edgacknowledg.client import DEFAULT_OPEN_TIMEOUT_S
from edgacknowledg.forge.db import ForgeStore, SectionRecord
from edgacknowledg.forge.publisher import (
  PublisherUnavailableError,
  ReviewIdentity,
  review_identity,
  section_url,
)
from edgacknowledg.forge.publisher.protocol import (
  MESSAGE_LIMIT,
  encode_review,
  encode_section,
)

_log = logging.getLogger(__name__)


class _Connection:
  '''The client's end of a connection to the publisher server.'''

  def __init__(self, connection: ClientConnection) -> None:
    self._connection = connection

  async def send(self, message: dict[str, Any]) -> None:
    try:
      await self._connection.send(json.dumps(message))
    except ConnectionClosed as ex:
      raise PublisherUnavailableError(
        f"the publisher server went away: {ex}"
      ) from ex

  async def receive(self) -> Optional[bytes]:
    try:
      message = await self._connection.recv()
    except ConnectionClosed:
      return None
    return message.encode() if isinstance(message, str) else message

  def abort(self) -> None:
    self._connection.transport.abort()

  async def close(self) -> None:
    await self._connection.close()


class PublisherClient:
  '''Publishes through the publisher server at ``url``.

  The server's publisher sees the review trees, read-only, and has the
  network (for AcknowlEDG), but not the service's home, database,
  credentials, or clones; its reviewer commands run in the tools
  container. It serves what is stored in the service's database
  (``store``) for ``forge_name``: when told a review's sections stored in
  the database changed (``sync_one``), and on every connection, every
  review stored in the database (``sync_all``).
  '''

  def __init__(self,
               *,
               url: str,
               api_host: str,
               api_port: int,
               api_key: Optional[str],
               ui_host: str,
               ui_port: int,
               reviews_dir: Path,
               store: ForgeStore,
               forge_name: str,
               request_timeout: float = 15 * 60.0) -> None:
    self._url = url
    self._api_host = api_host
    self._api_port = api_port
    self._api_key = api_key
    self._ui_host = ui_host
    self._ui_port = ui_port
    self._reviews_dir = reviews_dir
    self._store = store
    self._forge_name = forge_name
    self._request_timeout = request_timeout

    # The current connection, if any; set once it has served every review
    # stored in the database.
    self._connection: Optional[_Connection] = None
    self._connected = asyncio.Event()
    # Serializes requests (so at most one awaits a reply), and serving the
    # reviews stored in the database on connecting.
    self._lock = asyncio.Lock()
    self._reply: Optional[asyncio.Future[dict[str, Any]]] = None
    self._sync_all_task: Optional[asyncio.Task[None]] = None

  # Publisher

  async def wait_connected(self) -> None:
    await self._connected.wait()

  def section_url(self,
                  review_id: uuid.UUID,
                  section: SectionRecord) -> Optional[str]:
    return section_url(self._ui_host, self._ui_port, review_id, section)

  async def sync_one(self, review_id: uuid.UUID) -> None:
    async with self._lock:
      if not self._connected.is_set():
        raise PublisherUnavailableError(
          'not connected to the publisher server'
        )
      record = await self._store.find_pull_request_by_review(review_id)
      if record is None:
        raise LookupError(f"no pull request has review {review_id}")
      await self._sync(review_identity(record),
                       await self._store.sections(record.id))

  async def delist(self, review_id: uuid.UUID) -> None:
    async with self._lock:
      if not self._connected.is_set():
        raise PublisherUnavailableError(
          'not connected to the publisher server'
        )
      await self._request({'op': 'delist', 'review_id': str(review_id)})

  async def _sync(self,
                  review: ReviewIdentity,
                  sections: Sequence[SectionRecord]) -> None:
    await self._request({
      'op': 'sync',
      'review': encode_review(review),
      'sections': [encode_section(section) for section in sections],
    })

  # Requests

  async def _request(self, request: dict[str, Any]) -> None:
    '''Send ``request`` to the server and wait for its answer; the caller
    holds the lock.
    '''
    connection = self._connection
    if connection is None:
      raise PublisherUnavailableError(
        'not connected to the publisher server'
      )
    reply: asyncio.Future[dict[str, Any]] = (
      asyncio.get_running_loop().create_future()
    )
    self._reply = reply
    try:
      await connection.send(request)
      try:
        answer = await asyncio.wait_for(asyncio.shield(reply),
                                        self._request_timeout)
      except asyncio.TimeoutError:
        _log.error('the publisher server did not answer within %s s; '
                   'reconnecting', self._request_timeout)
        # Send it nothing more: a late answer must not be taken for the
        # next request's.
        self._connection = None
        connection.abort()
        raise PublisherUnavailableError(
          'the publisher server stopped answering'
        ) from None
    finally:
      self._reply = None

    error = answer.get('error')
    if error is not None:
      raise RuntimeError(f"publisher server: {answer.get('message')}")

  def _handle_message(self, line: bytes) -> None:
    try:
      value = json.loads(line)
    except ValueError:
      value = None
    if not isinstance(value, dict):
      _log.error('bad message from the publisher server: %r', line[:200])
      return
    if self._reply is not None and not self._reply.done():
      self._reply.set_result(value)
    else:
      _log.error('unexpected answer from the publisher server: %r',
                 line[:200])

  async def sync_all(self) -> None:
    '''On connecting, serve every open pull request's sections stored in
    the database (the publisher may have restarted, losing what it served),
    and have it stop serving anything else (what it serves outlasts a
    connection, so it may still serve what an earlier service did); only
    then accept new requests. Those it can't serve (their files are gone,
    or serving failed) it stops serving, and marks to be rebuilt; since
    section fingerprints are content-derived, rebuilding yields the same
    state (and so no comment).
    '''
    async with self._lock:
      try:
        served: list[str] = []
        for record in await self._store.open_pull_requests(self._forge_name):
          sections = await self._store.sections(record.id)
          if len(sections) == 0:
            continue
          review = review_identity(record)
          try:
            if not all(section.root.is_dir() for section in sections):
              raise FileNotFoundError('its files are gone')
            await self._sync(review, sections)
            served.append(str(review.review_id))
          except PublisherUnavailableError:
            raise
          except Exception as ex:
            _log.warning('rebuilding %s#%s: %s', record.info.repository,
                         record.info.number, ex)
            await self._sync(review, [])
            await self._store.mark_dirty(record.id)
        await self._request({'op': 'prune', 'keep': served})
      except PublisherUnavailableError as ex:
        _log.warning('serving the reviews failed (%s); will retry on the '
                     'next connection', ex)
        return
      _log.info('connected to the publisher server')
      self._connected.set()

  # Connection

  def _init_message(self) -> dict[str, Any]:
    return {
      'api_host': self._api_host,
      'api_port': self._api_port,
      'api_key': self._api_key,
      'reviews_dir': str(self._reviews_dir),
    }

  async def run(self) -> None:
    '''Stay connected to the publisher server, until cancelled: the
    websockets library reconnects whenever the connection goes, backing off
    exponentially while it can't reach the server.
    '''
    self._reviews_dir.mkdir(parents = True, exist_ok = True)
    async for connection in connect(
        self._url,
        max_size = MESSAGE_LIMIT,
        open_timeout = DEFAULT_OPEN_TIMEOUT_S):
      await self._run_connection(_Connection(connection))
      _log.warning('lost the publisher server; reconnecting')

  async def _run_connection(self, connection: _Connection) -> None:
    '''Serve ``connection`` until it goes.'''
    self._connection = connection
    try:
      await connection.send(self._init_message())
      self._sync_all_task = asyncio.create_task(self.sync_all())
      while self._connection is connection:
        message = await connection.receive()
        if message is None:
          break
        self._handle_message(message)
    finally:
      if self._connection is connection:
        self._connection = None
      self._connected.clear()
      if self._reply is not None and not self._reply.done():
        self._reply.set_exception(
          PublisherUnavailableError('the publisher connection closed')
        )
      connection.abort()
      await connection.close()
      if self._sync_all_task is not None:
        self._sync_all_task.cancel()
