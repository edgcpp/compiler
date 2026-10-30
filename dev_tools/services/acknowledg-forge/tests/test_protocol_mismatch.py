# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''An AcknowlEDG server speaking another protocol revision: the publisher
waits for an older one to be updated, and stops for a newer one.
'''

import asyncio
import json
import urllib.error
import urllib.request
import uuid

from pathlib import Path
from typing import Any, AsyncIterator, Optional

import pytest
import pytest_asyncio
import websockets

from websockets.asyncio.server import ServerConnection, serve

from edgacknowledg import (
  PROTOCOL_REVISION,
  IncompatibleProtocolVersion,
  RequestTags,
  encode_message,
  message,
)
from edgacknowledg import client as acknowledg_client
from edgacknowledg.client import BatchClientSession
from edgacknowledg.forge import serving
from edgacknowledg.forge.publisher import server as publisher_server
from edgacknowledg.forge.publisher.acknowledg import Wanted, publish


class _Server:
  '''A batch endpoint speaking ``revisions[n]`` on its ``n``th connection
  (the last one from then on): one speaking another revision says so at
  once, and closes; one speaking this revision accepts subscriptions.
  '''

  def __init__(self, *revisions: int) -> None:
    self.revisions = list(revisions)
    self.connections = 0
    self.subscribed: list[Any] = []
    self.port = 0

  async def handle(self, connection: ServerConnection) -> None:
    revision = self.revisions[min(self.connections,
                                  len(self.revisions) - 1)]
    self.connections += 1
    if revision != PROTOCOL_REVISION:
      await connection.send(json.dumps({
        't': RequestTags.OUTDATED_PROTOCOL, 'v': None, 'r': revision,
      }))
      return
    async for frame in connection:
      request = json.loads(frame)
      if request['t'] == RequestTags.BATCH_SUBSCRIBE:
        self.subscribed.append(request['v'])
        await connection.send(encode_message(message(
          RequestTags.BATCH_SUBSCRIBE,
          value = {**request['v'], 'subscription_id': str(uuid.uuid4())}
        )))


@pytest_asyncio.fixture
async def fast_retries(monkeypatch: pytest.MonkeyPatch) -> None:
  monkeypatch.setattr(acknowledg_client, 'SERVER_UPDATE_RETRY_S', 0.1)


async def _serving(server: _Server) -> Any:
  listening = await serve(server.handle, '127.0.0.1', 0)
  server.port = listening.sockets[0].getsockname()[1]
  return listening


class _Handler:
  subscription_type = 'test.src'
  slug = 'test-changes'
  section_name = 'Test - Changes'

  async def handle_request(self, channel: Any) -> None:
    await asyncio.Event().wait()


def _session(server: _Server) -> BatchClientSession:
  return BatchClientSession(
    status_listener = None,
    api_host = '127.0.0.1',
    api_port = server.port,
    api_key = None,
    open_timeout = 5,
    wait_for_server_update = True
  )


async def _wait_for(condition: Any) -> None:
  for _ in range(200):
    if condition():
      return
    await asyncio.sleep(0.05)
  raise AssertionError('timed out')


async def test_sessions_wait_for_older_servers(fast_retries: None) -> None:
  server = _Server(PROTOCOL_REVISION - 1, PROTOCOL_REVISION - 1,
                   PROTOCOL_REVISION)
  async with await _serving(server):
    session = _session(server)
    await session.start()
    try:
      review = await session.create_review(uuid.uuid4(), [], '')
      handler = _Handler()
      await review.add_section(handler)  # type: ignore[arg-type]
      # Kept while the server is older, and subscribed once it is updated.
      await _wait_for(lambda: len(server.subscribed) == 1)
      assert server.connections == 3
      assert review.sections() == [handler]
      assert server.subscribed[0]['slug'] == 'test-changes'
    finally:
      await session.stop()


async def test_sessions_stop_for_newer_servers(fast_retries: None) -> None:
  server = _Server(PROTOCOL_REVISION + 1)
  async with await _serving(server):
    session = _session(server)
    await session.start()
    try:
      with pytest.raises(IncompatibleProtocolVersion) as raised:
        await asyncio.wait_for(session.stopped(), 5)
      assert raised.value.peer_is_newer
      assert raised.value.revision == PROTOCOL_REVISION + 1
      await asyncio.sleep(0.5)
      assert server.connections == 1
    finally:
      await session.stop()


async def _publishing(server: _Server) -> 'asyncio.Task[None]':
  wanted: Wanted = asyncio.Queue()
  return asyncio.create_task(publish(
    wanted,
    api_host = '127.0.0.1',
    api_port = server.port,
    api_key = None,
    pool = None  # type: ignore[arg-type]
  ))


async def test_publishing_waits_for_older_servers(fast_retries: None) -> None:
  server = _Server(PROTOCOL_REVISION - 1)
  async with await _serving(server):
    publishing = await _publishing(server)
    try:
      await _wait_for(lambda: server.connections >= 3)
      assert not publishing.done()
    finally:
      publishing.cancel()
      await asyncio.gather(publishing, return_exceptions = True)


async def test_publishing_stops_for_newer_servers(fast_retries: None) -> None:
  server = _Server(PROTOCOL_REVISION + 1)
  async with await _serving(server):
    publishing = await _publishing(server)
    with pytest.raises(IncompatibleProtocolVersion):
      await asyncio.wait_for(publishing, 5)


def _health(url: str) -> tuple[int, str]:
  try:
    with urllib.request.urlopen(url, timeout = 10) as response:
      return int(response.status), response.read().decode()
  except urllib.error.HTTPError as ex:
    return ex.code, ex.read().decode()


async def test_the_publisher_stays_stopped_for_newer_servers(
    tmp_path: Path,
    unreachable_url: str,
    monkeypatch: pytest.MonkeyPatch) -> None:
  calls = 0

  async def newer(wanted: Wanted, **kwargs: Any) -> None:
    nonlocal calls
    calls += 1
    raise IncompatibleProtocolVersion(PROTOCOL_REVISION + 1)

  monkeypatch.setattr(publisher_server, 'publish', newer)
  listening = await serving.start(
    publisher_server.create_publisher(
      tools_service_url = unreachable_url,
      processes = 1
    ),
    host = '127.0.0.1',
    port = 0
  )
  try:
    health = listening.url('http') + serving.HEALTH_PATH
    assert await asyncio.to_thread(_health, health) == (200, 'ok\n')

    # A client's settings start publishing, which stops for good...
    async with websockets.connect(listening.url()) as connection:
      await connection.send(json.dumps({
        'api_host': '127.0.0.1', 'api_port': 1, 'api_key': None,
        'reviews_dir': str(tmp_path),
      }))
      for _ in range(100):
        status, body = await asyncio.to_thread(_health, health)
        if status != 200:
          break
        await asyncio.sleep(0.05)
    # ...and the publisher is unhealthy until it is updated.
    assert status == 503
    assert f"revision {PROTOCOL_REVISION + 1}" in body
    await asyncio.sleep(1.5)
    assert calls == 1
    assert (await asyncio.to_thread(_health, health))[0] == 503
  finally:
    await listening.close()


def test_mismatches_say_which_side_is_newer() -> None:
  older: Optional[int] = PROTOCOL_REVISION - 1
  assert not IncompatibleProtocolVersion(older).peer_is_newer
  assert IncompatibleProtocolVersion(PROTOCOL_REVISION + 1).peer_is_newer
  # Saying none it could be compared by: not newer.
  assert not IncompatibleProtocolVersion(None).peer_is_newer
  assert not IncompatibleProtocolVersion('20').peer_is_newer
