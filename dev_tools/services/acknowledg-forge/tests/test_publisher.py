# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The publisher client and server: serving reviewers from the publisher
server, which can't see the service's secrets, with a publisher per
connection.

These run the publisher and tools runner servers locally, as the service
does without containers; test_containers.py checks them in their
containers.
'''

import asyncio
import dataclasses
import json
import urllib.request
import uuid

from pathlib import Path
from typing import Any, AsyncIterator, Callable, Optional

import pytest
import pytest_asyncio
import websockets

from edgacknowledg import (
  decode_response,
  encode_message,
  message,
  rich_link,
  rich_text,
)
from edgacknowledg.forge import serving
from edgacknowledg.forge.db import ForgeStore, SectionRecord
from edgacknowledg.forge.ingest import utc_now
from edgacknowledg.forge.local import LocalDeployment, local_deployment
from edgacknowledg.forge.publisher import (
  KIND_TEST_CHANGES,
  PublisherUnavailableError,
  ReviewIdentity,
)
from edgacknowledg.forge.publisher.client import PublisherClient
from edgacknowledg.forge.publisher.protocol import (
  MESSAGE_LIMIT,
  decode_review,
  encode_review,
)
from edgacknowledg.forge.publisher.server import create_publisher
from edgacknowledg.forge.serving import HEALTH_PATH, Listening

from conftest import make_tools_dir, pull_request_info
from fakes import REPOSITORY, FakeForge
from test_acknowledg_integration import (
  Server,
  _wait_for_sources,
  pytestmark,
  server,
)

__all__ = ['pytestmark', 'server']

NAME = [rich_text('edg/compiler#1: test')]


def _layout(tmp_path: Path) -> tuple[Path, SectionRecord]:
  '''A review's tree, and its section.'''
  reviews = tmp_path / 'home' / 'reviews'
  tests_home = reviews / str(uuid.uuid4()) / 'base' / 'tests'
  (tests_home / 'tests' / 'suite').mkdir(parents = True)
  (tests_home / 'tests' / 'suite' / 'a.sft.cpp').write_text('int a;\n')
  root = tests_home / 'runs' / 'r1'
  (root / 'edg_x86_64').mkdir(parents = True)
  (root / 'edg_x86_64' / 'changes.elist').write_text('suite/a.sft.cpp\n')
  return reviews, SectionRecord(
    kind = KIND_TEST_CHANGES,
    name = 'Test - Changes',
    slug = 'test-changes',
    source_key = 'k1',
    content_fingerprint = 'f1',
    root = root,
    home = tests_home,
  )


async def _show_source(port: int, review_id: uuid.UUID) -> Any:
  async with websockets.connect(
      f"ws://127.0.0.1:{port}/review/{review_id}") as reviewer:
    await reviewer.send(encode_message(message(
      'test.src.show-src',
      value = {'id': 'suite/a.sft.cpp', 'assoc_file': None},
      section = 'test-changes'
    )))
    while True:
      tag, value, _ = decode_response(
        await asyncio.wait_for(reviewer.recv(), 10)
      )
      if tag == 'test.src.show-src':
        return value


_Deployment = tuple[LocalDeployment, Path, SectionRecord]


@pytest_asyncio.fixture
async def deployment(tmp_path: Path) -> AsyncIterator[_Deployment]:
  reviews, section = _layout(tmp_path)
  async with local_deployment(
      home = tmp_path / 'home',
      tools_dir = make_tools_dir(tmp_path / 'tools')) as local:
    yield local, reviews, section


async def _store(store: ForgeStore,
                 number: int,
                 sections: list[SectionRecord]) -> uuid.UUID:
  '''Store pull request ``number`` with ``sections`` as served, as the
  processor does; its review.
  '''
  await store.upsert_pull_request(
    FakeForge.name,
    pull_request_info(number, 'a' * 40, 'b' * 40),
    now = utc_now(),
    settle_timeout = 0
  )
  record = await store.find_pull_request(FakeForge.name, REPOSITORY, number)
  assert record is not None
  await store.replace_sections(record.id, sections)
  return record.review_id


def _client(url: str,
            reviews: Path,
            store: ForgeStore,
            *,
            api_port: int,
            **kwargs: Any) -> PublisherClient:
  return PublisherClient(
    url = url,
    api_host = '127.0.0.1',
    api_port = api_port,
    api_key = None,
    ui_host = 'ui.example',
    ui_port = 443,
    reviews_dir = reviews,
    store = store,
    forge_name = FakeForge.name,
    **kwargs
  )


def _empty_sync() -> dict[str, Any]:
  '''A request serving nothing for a review nothing serves.'''
  return {
    'op': 'sync',
    'review': encode_review(ReviewIdentity(uuid.uuid4(), NAME, '')),
    'sections': [],
  }


async def _wait_for(condition: Callable[[], bool]) -> None:
  for _ in range(300):
    if condition():
      return
    await asyncio.sleep(0.05)
  raise AssertionError('timed out')


async def test_publishing_outlasts_reconnecting(deployment: _Deployment,
                                                server: Server,
                                                store: ForgeStore) -> None:
  local, reviews, section = deployment
  publisher = _client(local.publisher_service_url, reviews, store,
                      api_port = server.port)
  task = asyncio.create_task(publisher.run())
  try:
    await asyncio.wait_for(publisher.wait_connected(), 30)
    review_id = await _store(store, 1, [section])
    await publisher.sync_one(review_id)
    await _wait_for_sources(server.port, review_id, ['test-changes'])

    # The connection goes; what it published stays, and still does once
    # the client connects again.
    connection = publisher._connection
    assert connection is not None
    connection.abort()
    await _wait_for(lambda: publisher._connection is not connection)
    await _wait_for_sources(server.port, review_id, ['test-changes'])
    await asyncio.wait_for(publisher.wait_connected(), 30)
    await _wait_for_sources(server.port, review_id, ['test-changes'])
    value = await _show_source(server.port, review_id)
    assert 'int a;' in str(value)
  finally:
    task.cancel()
    await asyncio.gather(task, return_exceptions = True)


async def test_connecting_serves_what_is_stored(deployment: _Deployment,
                                               server: Server,
                                               store: ForgeStore) -> None:
  local, reviews, section = deployment
  served = await _store(store, 1, [section])
  # Its files are gone (as after a disk cleanup).
  gone = await _store(store, 2, [
    dataclasses.replace(section, root = section.root.parent / 'gone')
  ])
  for record in await store.open_pull_requests(FakeForge.name):
    await store.mark_processed(record.id, record.generation)

  publisher = _client(local.publisher_service_url, reviews, store,
                      api_port = server.port)
  task = asyncio.create_task(publisher.run())
  try:
    await asyncio.wait_for(publisher.wait_connected(), 30)
    await _wait_for_sources(server.port, served, ['test-changes'])
    await _wait_for_sources(server.port, gone, [])
    # Not served, but rebuilt.
    dirty = {
      record.review_id: record.is_dirty
      for record in await store.open_pull_requests(FakeForge.name)
    }
    assert dirty == {served: False, gone: True}
  finally:
    task.cancel()
    await asyncio.gather(task, return_exceptions = True)


async def test_a_new_service_drops_what_it_does_not_store(
    deployment: _Deployment,
    server: Server,
    store: ForgeStore) -> None:
  local, reviews, section = deployment
  earlier = _client(local.publisher_service_url, reviews, store,
                    api_port = server.port)
  task = asyncio.create_task(earlier.run())
  try:
    await asyncio.wait_for(earlier.wait_connected(), 30)
    review_id = await _store(store, 1, [section])
    await earlier.sync_one(review_id)
    await _wait_for_sources(server.port, review_id, ['test-changes'])
  finally:
    task.cancel()
    await asyncio.gather(task, return_exceptions = True)

  # The publisher still serves it, until a service whose database doesn't
  # store it connects.
  await _wait_for_sources(server.port, review_id, ['test-changes'])
  other_store = await ForgeStore.open('sqlite+aiosqlite://')
  later = _client(local.publisher_service_url, reviews, other_store,
                  api_port = server.port)
  task = asyncio.create_task(later.run())
  try:
    await asyncio.wait_for(later.wait_connected(), 30)
    await _wait_for_sources(server.port, review_id, [])
  finally:
    task.cancel()
    await asyncio.gather(task, return_exceptions = True)
    await other_store.aclose()


async def test_delisting_stops_serving_what_is_still_stored(
    deployment: _Deployment,
    server: Server,
    store: ForgeStore) -> None:
  local, reviews, section = deployment
  publisher = _client(local.publisher_service_url, reviews, store,
                      api_port = server.port)
  task = asyncio.create_task(publisher.run())
  try:
    await asyncio.wait_for(publisher.wait_connected(), 30)
    review_id = await _store(store, 1, [section])
    await publisher.sync_one(review_id)
    await _wait_for_sources(server.port, review_id, ['test-changes'])

    # As when tearing it down, before its files go.
    await publisher.delist(review_id)
    await _wait_for_sources(server.port, review_id, [])
    record = await store.find_pull_request_by_review(review_id)
    assert record is not None and len(await store.sections(record.id)) == 1
  finally:
    task.cancel()
    await asyncio.gather(task, return_exceptions = True)


async def test_connections_share_the_publisher(deployment: _Deployment,
                                               server: Server,
                                               store: ForgeStore) -> None:
  local, reviews, section = deployment
  publisher = _client(local.publisher_service_url, reviews, store,
                      api_port = server.port)
  task = asyncio.create_task(publisher.run())
  try:
    await asyncio.wait_for(publisher.wait_connected(), 30)
    review_id = await _store(store, 1, [section])
    await publisher.sync_one(review_id)
    await _wait_for_sources(server.port, review_id, ['test-changes'])

    # Another connection's requests go to the same publisher, and its
    # opening and closing change nothing it publishes.
    async with websockets.connect(local.publisher_service_url) as other:
      await other.send(json.dumps(publisher._init_message()))
      await other.send(json.dumps(_empty_sync()))
      answer = json.loads(await asyncio.wait_for(other.recv(), 30))
      assert answer == {'ok': True}
    await _wait_for_sources(server.port, review_id, ['test-changes'])
    await publisher.sync_one(await _store(store, 1, []))
    await _wait_for_sources(server.port, review_id, [])
  finally:
    task.cancel()
    await asyncio.gather(task, return_exceptions = True)


async def _start_publisher(tools_service_url: str) -> Listening:
  return await serving.start(
    create_publisher(tools_service_url = tools_service_url, processes = 1),
    host = '127.0.0.1',
    port = 0,
    max_message_size = MESSAGE_LIMIT
  )


async def test_publishing_needs_no_tools_runner(
    tmp_path: Path,
    store: ForgeStore,
    unreachable_port: int,
    unreachable_url: str) -> None:
  reviews, _ = _layout(tmp_path)
  # Only requests running a command need the tools runner; the connection
  # is served without it.
  listening = await _start_publisher(unreachable_url)
  client = _client(listening.url(), reviews, store,
                   api_port = unreachable_port)
  connection = asyncio.create_task(client.run())
  try:
    await asyncio.wait_for(client.wait_connected(), 30)
    await asyncio.wait_for(client.sync_one(await _store(store, 1, [])), 30)
    assert not connection.done()
  finally:
    connection.cancel()
    await asyncio.gather(connection, return_exceptions = True)
    await listening.close()


def _get(url: str) -> int:
  with urllib.request.urlopen(url, timeout = 10) as response:
    return int(response.status)


async def test_health_checks_leave_the_connection(
    tmp_path: Path,
    unreachable_port: int,
    unreachable_url: str) -> None:
  # A health check is a route of its own: it leaves the connection
  # serving.
  listening = await _start_publisher(unreachable_url)
  try:
    async with websockets.connect(listening.url()) as connection:
      await connection.send(json.dumps({
        'api_host': '127.0.0.1', 'api_port': unreachable_port,
        'api_key': None, 'ui_host': 'ui.invalid', 'ui_port': 443,
        'reviews_dir': str(tmp_path),
      }))
      health = listening.url('http') + HEALTH_PATH
      assert await asyncio.to_thread(_get, health) == 200
      # The connection still serves its publisher.
      await connection.send(json.dumps(_empty_sync()))
      answer = json.loads(await asyncio.wait_for(connection.recv(), 30))
      assert answer == {'ok': True}
  finally:
    await listening.close()


class _SilentConnection:
  '''Stands in for a connection whose server never answers.'''

  def __init__(self) -> None:
    self.sent: list[Any] = []
    self.aborted = False

  async def send(self, message: Any) -> None:
    self.sent.append(message)

  async def receive(self) -> Optional[bytes]:
    return None

  def abort(self) -> None:
    self.aborted = True

  async def close(self) -> None:
    pass


async def test_unanswered_request_drops_the_connection(
    tmp_path: Path,
    store: ForgeStore,
    unreachable_port: int,
    unreachable_url: str) -> None:
  reviews, section = _layout(tmp_path)
  publisher = _client(unreachable_url, reviews, store,
                      api_port = unreachable_port, request_timeout = 0.1)
  connection = _SilentConnection()
  publisher._connection = connection  # type: ignore[assignment]
  publisher._connected.set()
  review_id = await _store(store, 1, [section])
  with pytest.raises(PublisherUnavailableError, match = 'stopped answering'):
    await publisher.sync_one(review_id)
  assert connection.aborted

  # Its late answer isn't taken for the next request's: nothing more is
  # sent on it.
  publisher._handle_message(b'{"ok": true}\n')
  with pytest.raises(PublisherUnavailableError, match = 'not connected'):
    await publisher.sync_one(review_id)
  assert len(connection.sent) == 1


def test_review_names_survive_the_protocol() -> None:
  review = ReviewIdentity(
    uuid.uuid4(),
    [rich_link('https://forge.example/edg/compiler/pull/1', 'edg/compiler#1'),
     rich_text(': test')],
    '2026-01-02T03:04:05+00:00'
  )
  assert decode_review(encode_review(review)) == review

  # Rejected rather than shown as the repr of whatever it was.
  with pytest.raises(ValueError):
    decode_review({**encode_review(review), 'name': 'edg/compiler#1: test'})
