# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The publisher client (``PublisherClient``): how the service publishes,
through the publisher server in the publisher container (see
``publisher.server``), over one long-lived connection (kept by a
``ForgeSandboxClient``).
'''

import asyncio
import json
import logging
import uuid

from pathlib import Path
from types import TracebackType
from typing import Any, Optional, Sequence, Type

from websockets.asyncio.client import ClientConnection
from websockets.exceptions import ConnectionClosed

from edgacknowledg.client import DEFAULT_OPEN_TIMEOUT_S
from edgacknowledg.forge.db import ForgeStore, SectionRecord
from edgacknowledg.forge.publisher import (
  ReviewIdentity,
  review_identity,
  section_url,
)
from edgacknowledg.forge.publisher.protocol import (
  MESSAGE_LIMIT,
  encode_review,
  encode_section,
)
from edgacknowledg.forge.sandbox_client import (
  ForgeSandboxClient,
  ServiceUnavailableError,
)

_log = logging.getLogger(__name__)


class PublisherClient:
  '''Publishes through the publisher server at ``url``, while it runs
  (``async with``, or ``start`` and ``close``).

  The server's publisher sees the review trees, read-only, and has the
  network (for AcknowlEDG), but not the service's home, database,
  credentials, or clones; its reviewer commands run in the tools
  container. It serves what is stored in the service's database
  (``store``) for ``forge_name``: when told a review's sections stored in
  the database changed (``sync_one``), and on every connection, every
  review stored in the database (``sync_all``). So telling it is only a
  shortcut: what it can't take then, it serves on connecting.
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
    self._link = ForgeSandboxClient(
      url,
      peer = 'the publisher server',
      serve = self._serve,
      open_timeout = DEFAULT_OPEN_TIMEOUT_S,
      max_size = MESSAGE_LIMIT
    )
    self._api_host = api_host
    self._api_port = api_port
    self._api_key = api_key
    self._ui_host = ui_host
    self._ui_port = ui_port
    self._reviews_dir = reviews_dir
    self._store = store
    self._forge_name = forge_name
    self._request_timeout = request_timeout
    # Serializes requests, for two reasons. Answers carry no request ID, so
    # at most one request may await (read) one. And sync_one and delist
    # must wait for sync_all to finish (even one a reconnect started), or
    # they could interleave with it: it would resend stale sections, or
    # prune a review sync_one just served.
    self._lock = asyncio.Lock()

  def start(self) -> None:
    '''Start keeping the connection; each one serves every review stored
    in the database before any request (``sync_all``).
    '''
    self._reviews_dir.mkdir(parents = True, exist_ok = True)
    self._link.start()

  async def close(self) -> None:
    '''Close the connection, and stop keeping it.'''
    await self._link.close()

  async def __aenter__(self) -> 'PublisherClient':
    self.start()
    return self

  async def __aexit__(self,
                      exc_type: Optional[Type[BaseException]],
                      exc: Optional[BaseException],
                      tb: Optional[TracebackType]) -> None:
    await self.close()

  async def wait_connected(self) -> None:
    '''Wait until connected (requests then wait for every review stored in
    the database to be served).
    '''
    await self._link.wait_connected()

  # Publisher

  def section_url(self,
                  review_id: uuid.UUID,
                  section: SectionRecord) -> Optional[str]:
    return section_url(self._ui_host, self._ui_port, review_id, section)

  async def sync_one(self, review_id: uuid.UUID) -> None:
    async with self._lock:
      if not self._link.connected:
        _log.info('not connected; serving %s on connecting', review_id)
        return
      record = await self._store.find_pull_request_by_review(review_id)
      if record is None:
        raise LookupError(f"no pull request has review {review_id}")
      try:
        await self._sync(review_identity(record),
                         await self._store.sections(record.id))
      except ServiceUnavailableError as ex:
        _log.warning('%s; serving %s on connecting', ex, review_id)

  async def delist(self, review_id: uuid.UUID) -> None:
    async with self._lock:
      if not self._link.connected:
        raise ServiceUnavailableError(
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
    data = json.dumps(request)
    if len(data) > MESSAGE_LIMIT:
      # The server would close the connection over it: it is this request
      # that fails, not the connection (nor every request after it).
      raise ValueError(
        f"the {request.get('op')} request is {len(data)} bytes, over the "
        f"publisher server's limit of {MESSAGE_LIMIT}"
      )
    connection = self._link.connection
    if connection is None:
      raise ServiceUnavailableError(
        'not connected to the publisher server'
      )
    await self._send(connection, data)
    try:
      message = await asyncio.wait_for(self._receive(connection),
                                       self._request_timeout)
    except asyncio.TimeoutError:
      _log.error('the publisher server did not answer within %s s; '
                 'reconnecting', self._request_timeout)
      # A late answer must not be taken for the next request's.
      self._link.drop(connection)
      raise ServiceUnavailableError(
        'the publisher server stopped answering'
      ) from None
    if message is None:
      raise ServiceUnavailableError('the publisher connection closed')

    try:
      answer = json.loads(message)
    except ValueError:
      answer = None
    if not isinstance(answer, dict):
      _log.error('bad answer from the publisher server: %r; reconnecting',
                 message[:200])
      # Out of step with the server: no later answer can be trusted.
      self._link.drop(connection)
      raise ServiceUnavailableError(
        'bad answer from the publisher server'
      )
    if answer.get('error') is not None:
      raise RuntimeError(f"publisher server: {answer.get('message')}")

  @staticmethod
  async def _send(connection: ClientConnection, message: str) -> None:
    try:
      await connection.send(message)
    except ConnectionClosed as ex:
      raise ServiceUnavailableError(
        f"the publisher server went away: {ex}"
      ) from ex

  @staticmethod
  async def _receive(connection: ClientConnection) -> Optional[bytes]:
    try:
      message = await connection.recv()
    except ConnectionClosed:
      return None
    return message.encode() if isinstance(message, str) else message

  async def sync_all(self, connection: ClientConnection) -> None:
    '''On ``connection``, serve every open pull request's sections stored in
    the database (the publisher may have restarted, losing what it served),
    and have it stop serving anything else (what it serves outlasts a
    connection, so it may still serve what an earlier service did); only
    then accept new requests. Those it can't serve (their files are gone,
    or the server rejected them) it leaves out of what it keeps serving,
    and marks to be rebuilt; since section fingerprints are content-derived,
    rebuilding yields the same state (and so no comment). Should anything
    else fail, the connection goes, to try again on the next.
    '''
    async with self._lock:
      await self._sync_all(connection)

  async def _sync_all(self, connection: ClientConnection) -> None:
    '''``sync_all``, for a caller holding the lock.'''
    try:
      served: list[str] = []
      for record in await self._store.open_pull_requests(self._forge_name):
        sections = await self._store.sections(record.id)
        if len(sections) == 0:
          continue
        try:
          if not all(section.root.is_dir() for section in sections):
            raise FileNotFoundError('its files are gone')
          await self._sync(review_identity(record), sections)
        except ServiceUnavailableError:
          raise
        except Exception as ex:
          _log.warning('rebuilding %s#%s: %s', record.info.repository,
                       record.info.number, ex)
          await self._store.mark_dirty(record.id)
          continue
        served.append(str(record.review_id))
      await self._request({'op': 'prune', 'keep': served})
    except ServiceUnavailableError as ex:
      _log.warning('serving the reviews failed (%s); will retry on the '
                   'next connection', ex)
      return
    except Exception:
      _log.exception('serving the reviews failed; reconnecting')
      self._link.drop(connection)
      return
    _log.info('connected to the publisher server')

  # Connection

  def _init_message(self) -> dict[str, Any]:
    return {
      'api_host': self._api_host,
      'api_port': self._api_port,
      'api_key': self._api_key,
      'reviews_dir': str(self._reviews_dir),
    }

  async def _serve(self, connection: ClientConnection) -> None:
    '''Serve ``connection`` until it goes: initialize it, then serve every
    review (``sync_all``), holding the lock throughout, so that requests
    come after. Requests read their own answers (the server sends nothing
    else); a request waiting on one when it goes fails.
    '''
    async with self._lock:
      try:
        await self._send(connection, json.dumps(self._init_message()))
      except ServiceUnavailableError as ex:
        _log.warning('the publisher server went away at once: %s', ex)
        return
      await self._sync_all(connection)
    await connection.wait_closed()
