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
import time
import uuid

from pathlib import Path
from typing import Any, Optional, Sequence

from websockets.asyncio.client import ClientConnection, connect
from websockets.exceptions import ConnectionClosed, InvalidHandshake

from edgacknowledg.client import DEFAULT_OPEN_TIMEOUT_S
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.publisher import (
  PublisherUnavailableError,
  ReviewIdentity,
  section_url,
)
from edgacknowledg.forge.publisher.protocol import (
  MESSAGE_LIMIT,
  encode_review,
  encode_section,
)

_log = logging.getLogger(__name__)


class _Connection:
  '''The client's end of a connection to the publisher server, which keeps
  one publisher for it, for as long as it lasts.
  '''

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
    # Dropping the connection makes the server drop its publisher.
    self._connection.transport.abort()

  async def close(self) -> None:
    await self._connection.close()


class PublisherClient:
  '''Publishes through the publisher server at ``url``.

  Each connection gets a publisher of its own on the server, which sees
  the review trees, read-only, and has the network (for AcknowlEDG), but
  not the service's home, database, credentials, or clones; its reviewer
  commands run in the tools container. Should the connection go, the
  client connects again and serves every review's sections again.
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
               request_timeout: float = 15 * 60.0,
               restart_delay: float = 1.0,
               max_restart_delay: float = 60.0) -> None:
    self._url = url
    self._api_host = api_host
    self._api_port = api_port
    self._api_key = api_key
    self._ui_host = ui_host
    self._ui_port = ui_port
    self._reviews_dir = reviews_dir
    self._request_timeout = request_timeout
    self._restart_delay = restart_delay
    self._max_restart_delay = max_restart_delay

    # The current connection, if any.
    self._connection: Optional[_Connection] = None
    self._connected = asyncio.Event()
    # Serializes requests (so at most one awaits a reply), and replaying
    # after reconnecting.
    self._lock = asyncio.Lock()
    self._reply: Optional[asyncio.Future[dict[str, Any]]] = None
    # What each review should serve, to replay after reconnecting.
    self._desired: dict[uuid.UUID,
                        tuple[ReviewIdentity, list[SectionRecord]]] = {}
    self._needs_replay = False
    self._replay_task: Optional[asyncio.Task[None]] = None

  # Publisher

  async def wait_connected(self) -> None:
    await self._connected.wait()

  def section_url(self,
                  review_id: uuid.UUID,
                  section: SectionRecord) -> Optional[str]:
    return section_url(self._ui_host, self._ui_port, review_id, section)

  async def sync(self,
                 review: ReviewIdentity,
                 sections: Sequence[SectionRecord]) -> None:
    async with self._lock:
      if not self._connected.is_set():
        raise PublisherUnavailableError('not connected to AcknowlEDG')
      await self._sync(review, sections)

  async def remove(self, review_id: uuid.UUID) -> None:
    async with self._lock:
      # A new connection's publisher serves nothing for it either way.
      self._desired.pop(review_id, None)
      if self._connection is not None:
        await self._request({'op': 'remove', 'review_id': str(review_id)})

  async def _sync(self,
                  review: ReviewIdentity,
                  sections: Sequence[SectionRecord]) -> None:
    await self._request({
      'op': 'sync',
      'review': encode_review(review),
      'sections': [encode_section(section) for section in sections],
    })
    if len(sections) == 0:
      self._desired.pop(review.review_id, None)
    else:
      self._desired[review.review_id] = (review, list(sections))

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
    if error == 'unavailable':
      raise PublisherUnavailableError(str(answer.get('message')))
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
    event = value.get('event')
    if event == 'connected':
      if self._replay_task is None or self._replay_task.done():
        self._replay_task = asyncio.create_task(self._on_connected())
    elif event == 'disconnected':
      self._connected.clear()
    elif self._reply is not None and not self._reply.done():
      self._reply.set_result(value)
    else:
      _log.error('unexpected answer from the publisher server: %r',
                 line[:200])

  async def _on_connected(self) -> None:
    '''Once the publisher is connected to AcknowlEDG, serve again what the
    last connection's served; only then accept new requests.
    '''
    async with self._lock:
      if self._needs_replay:
        for review, sections in list(self._desired.values()):
          try:
            await self._sync(review, sections)
          except PublisherUnavailableError as ex:
            _log.warning('serving the reviews again failed (%s); will '
                         'retry on the next connection', ex)
            return
          except Exception:
            # Don't let one review hold up every other one; it is served
            # again once its pull request changes.
            _log.exception('serving review %s again failed; dropping it',
                           review.review_id)
            self._desired.pop(review.review_id, None)
        self._needs_replay = False
      _log.info('connected to AcknowlEDG')
      self._connected.set()

  # Connection

  def _init_message(self) -> dict[str, Any]:
    return {
      'api_host': self._api_host,
      'api_port': self._api_port,
      'api_key': self._api_key,
      'ui_host': self._ui_host,
      'ui_port': self._ui_port,
      'reviews_dir': str(self._reviews_dir),
    }

  async def run(self) -> None:
    '''Stay connected to the publisher server, reconnecting whenever the
    connection goes, until cancelled.
    '''
    self._reviews_dir.mkdir(parents = True, exist_ok = True)
    delay = self._restart_delay
    while True:
      started = time.monotonic()
      try:
        await self._run_connection()
      except asyncio.CancelledError:
        raise
      except (OSError, ConnectionClosed, InvalidHandshake,
              asyncio.TimeoutError) as ex:
        _log.warning("couldn't reach the publisher server: %r", ex)
      except Exception:
        _log.exception('the publisher connection failed')
      if time.monotonic() - started > self._max_restart_delay:
        delay = self._restart_delay
      _log.warning('lost the publisher server; reconnecting in %.0f s',
                   delay)
      await asyncio.sleep(delay)
      delay = min(delay * 2, self._max_restart_delay)

  async def _open(self) -> _Connection:
    connection = await connect(
      self._url,
      max_size = MESSAGE_LIMIT,
      open_timeout = DEFAULT_OPEN_TIMEOUT_S
    )
    return _Connection(connection)

  async def _run_connection(self) -> None:
    connection = await self._open()
    self._connection = connection
    try:
      await connection.send(self._init_message())
      while self._connection is connection:
        message = await connection.receive()
        if message is None:
          break
        self._handle_message(message)
    finally:
      if self._connection is connection:
        self._connection = None
      self._connected.clear()
      self._needs_replay = True
      if self._reply is not None and not self._reply.done():
        self._reply.set_exception(
          PublisherUnavailableError('the publisher connection closed')
        )
      connection.abort()
      await connection.close()
      if self._replay_task is not None:
        self._replay_task.cancel()
