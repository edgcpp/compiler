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
import uuid

from pathlib import Path
from typing import Any, AsyncIterator, Optional

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
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.local import LocalDeployment, local_deployment
from edgacknowledg.forge.publisher import (
  KIND_TEST_CHANGES,
  PublisherUnavailableError,
  ReviewIdentity,
)
from edgacknowledg.forge.publisher.client import PublisherClient
from edgacknowledg.forge.publisher.protocol import (
  decode_review,
  encode_review,
)
from edgacknowledg.forge.publisher.server import PublisherServer

from conftest import make_tools_dir
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


def _url(listening: Any) -> str:
  return f"ws://127.0.0.1:{next(iter(listening.sockets)).getsockname()[1]}"


def _client(url: str,
            reviews: Path,
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
    **kwargs
  )


@pytest.mark.parametrize('ending', ['server drops it', 'client drops it'])
async def test_publishing_survives_reconnecting(deployment: _Deployment,
                                                server: Server,
                                                ending: str) -> None:
  local, reviews, section = deployment
  publisher = _client(local.publisher_service_url, reviews,
                      api_port = server.port, restart_delay = 0.1)
  task = asyncio.create_task(publisher.run())
  try:
    await asyncio.wait_for(publisher.wait_connected(), 30)
    review = ReviewIdentity(uuid.uuid4(), NAME, '')
    await publisher.sync(review, [section])
    await _wait_for_sources(server.port, review.review_id, ['test-changes'])
    value = await _show_source(server.port, review.review_id)
    assert 'int a;' in str(value)

    # The connection goes (the server's side, or the client gave up on
    # it): its publisher, and what it served, go with it, and come back
    # with the next connection's.
    if ending == 'server drops it':
      local.publisher_server.drop_connection()
    else:
      connection = publisher._connection
      assert connection is not None
      connection.abort()
    await _wait_for_sources(server.port, review.review_id, [])
    await asyncio.wait_for(publisher.wait_connected(), 30)
    await _wait_for_sources(server.port, review.review_id, ['test-changes'])

    await publisher.remove(review.review_id)
    await _wait_for_sources(server.port, review.review_id, [])
  finally:
    task.cancel()
    await asyncio.gather(task, return_exceptions = True)


async def test_publishing_needs_no_tools_runner(
    tmp_path: Path,
    unreachable_port: int,
    unreachable_url: str) -> None:
  reviews, _ = _layout(tmp_path)
  # Only requests running a command need the tools runner; the connection
  # is served without it.
  publisher_server = PublisherServer(tools_service_url = unreachable_url,
                                     processes = 1)
  listening = await publisher_server.start('127.0.0.1', 0)
  client = _client(_url(listening), reviews, api_port = unreachable_port)
  connection = asyncio.create_task(client._run_connection())
  try:
    for _ in range(100):
      if client._connection is not None:
        break
      await asyncio.sleep(0.05)
    await asyncio.wait_for(client.remove(uuid.uuid4()), 30)
    assert not connection.done()
  finally:
    connection.cancel()
    await asyncio.gather(connection, return_exceptions = True)
    listening.close()
    await listening.wait_closed()
    await publisher_server.close()


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
    unreachable_port: int,
    unreachable_url: str) -> None:
  reviews, section = _layout(tmp_path)
  publisher = _client(unreachable_url, reviews, api_port = unreachable_port,
                      request_timeout = 0.1)
  connection = _SilentConnection()
  publisher._connection = connection  # type: ignore[assignment]
  publisher._connected.set()
  review = ReviewIdentity(uuid.uuid4(), NAME, '')
  with pytest.raises(PublisherUnavailableError, match = 'stopped answering'):
    await publisher.sync(review, [section])
  assert connection.aborted

  # Its late answer isn't taken for the next request's: nothing more is
  # sent on it.
  publisher._handle_message(b'{"ok": true}\n')
  with pytest.raises(PublisherUnavailableError, match = 'not connected'):
    await publisher.sync(review, [section])
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
