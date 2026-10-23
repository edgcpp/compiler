# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''AcknowledgPublisher against a real AcknowlEDG server.'''

import asyncio
import json
import os
import shutil
import signal
import socket
import subprocess
import uuid

from pathlib import Path
from typing import Any, AsyncIterator, Iterator

import pytest
import pytest_asyncio
import websockets

from edgacknowledg import (
  PROTOCOL_REVISION,
  batch_event_message,
  decode_response,
  encode_message,
  message,
  rich_link,
  rich_text,
)
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.publisher import KIND_TEST_CHANGES, ReviewIdentity
from edgacknowledg.forge.publisher.acknowledg import AcknowledgPublisher
from edgacknowledg.forge.publisher.pool import ProcessPool

# services/acknowledg/server, relative to services/acknowledg-forge/tests.
SERVER_DIR = (
  Path(__file__).resolve().parent.parent.parent / 'acknowledg' / 'server'
)

pytestmark = pytest.mark.skipif(
  shutil.which('uv') is None or not SERVER_DIR.is_dir(),
  reason = 'needs uv and the AcknowlEDG server sources'
)


def _free_port() -> int:
  with socket.socket() as sock:
    sock.bind(('127.0.0.1', 0))
    return int(sock.getsockname()[1])


class Server:
  def __init__(self, port: int) -> None:
    self.port = port
    self.process: 'subprocess.Popen[bytes] | None' = None

  async def start(self) -> None:
    self.process = subprocess.Popen(
      [
        'uv', 'run', '--quiet', '--project', str(SERVER_DIR),
        'hypercorn', '--bind', f"127.0.0.1:{self.port}",
        'acknowledg_server:app',
      ],
      stdout = subprocess.DEVNULL,
      stderr = subprocess.DEVNULL,
      start_new_session = True
    )
    for _ in range(300):
      try:
        _, writer = await asyncio.open_connection('127.0.0.1', self.port)
        writer.close()
        return
      except OSError:
        await asyncio.sleep(0.1)
    raise RuntimeError('the AcknowlEDG server did not start')

  def stop(self) -> None:
    if self.process is not None:
      # Hypercorn waits for open websockets on SIGTERM; don't.
      os.killpg(self.process.pid, signal.SIGKILL)
      self.process.wait(timeout = 30)
      self.process = None


@pytest_asyncio.fixture
async def server() -> AsyncIterator[Server]:
  server = Server(_free_port())
  await server.start()
  yield server
  server.stop()


@pytest.fixture
def section(tmp_path: Path) -> Iterator[SectionRecord]:
  home = tmp_path / 'tests'
  (home / 'tests' / 'suite').mkdir(parents = True)
  (home / 'tests' / 'suite' / 'a.sft.cpp').write_text('int a;\n')
  root = home / 'runs' / 'r1'
  (root / 'edg_x86_64').mkdir(parents = True)
  (root / 'edg_x86_64' / 'changes.elist').write_text('suite/a.sft.cpp\n')
  yield SectionRecord(
    kind = KIND_TEST_CHANGES,
    name = 'Test - Changes',
    slug = 'test-changes',
    source_key = 'k1',
    content_fingerprint = 'f1',
    root = root,
    home = home,
  )


async def _sources(port: int, review_id: uuid.UUID) -> list[Any]:
  '''The section sources a new reviewer of ``review_id`` sees.'''
  async with websockets.connect(
      f"ws://127.0.0.1:{port}/review/{review_id}") as reviewer:
    while True:
      tag, value, _ = decode_response(
        await asyncio.wait_for(reviewer.recv(), 10)
      )
      if tag == 'review.connection-state':
        return [source['slug'] for source in value['sources']]


async def _wait_for_sources(port: int,
                            review_id: uuid.UUID,
                            expected: list[str]) -> None:
  for _ in range(100):
    try:
      if await _sources(port, review_id) == expected:
        return
    except (OSError, websockets.exceptions.WebSocketException):
      # The server may still be starting.
      pass
    await asyncio.sleep(0.1)
  assert await _sources(port, review_id) == expected


async def _names(port: int, review_id: uuid.UUID) -> tuple[Any, Any]:
  '''The name of ``review_id`` a new reviewer sees, and the one listed.'''
  async with websockets.connect(
      f"ws://127.0.0.1:{port}/review/{review_id}") as reviewer:
    tag = None
    while tag != 'review.connection-state':
      tag, state, _ = decode_response(
        await asyncio.wait_for(reviewer.recv(), 10)
      )
  async with websockets.connect(f"ws://127.0.0.1:{port}/review") as listing:
    tag, reviews, _ = decode_response(
      await asyncio.wait_for(listing.recv(), 10)
    )
    assert tag == 'list-reviews', tag
  listed = [
    review['name'] for review in reviews if review['id'] == str(review_id)
  ]
  assert len(listed) == 1, reviews
  return state['name'], listed[0]


async def test_publish_heal_and_survive_bad_requests(
                                                  server: Server,
                                                  section: SectionRecord,
                                                  unreachable_url: str
                                                  ) -> None:
  # No command runs here, so the tools runner is never reached.
  pool = ProcessPool(tools_service_url = unreachable_url, processes = 2)
  await pool.start()
  publisher = AcknowledgPublisher(
    api_host = '127.0.0.1',
    api_port = server.port,
    api_key = None,
    ui_host = 'ui.example',
    ui_port = 443,
    pool = pool
  )
  task = asyncio.create_task(publisher.run())
  try:
    await asyncio.wait_for(publisher.wait_connected(), 30)
    name = [
      rich_link('https://forge.example/edg/compiler/pull/1', 'edg/compiler#1'),
      rich_text(': test'),
    ]
    review = ReviewIdentity(uuid.uuid4(), name, '')
    await publisher.sync(review, [section])
    await _wait_for_sources(server.port, review.review_id, ['test-changes'])
    assert await _names(server.port, review.review_id) == (name, name)
    assert publisher.section_url(review.review_id, section) == (
      f"https://ui.example/review/{review.review_id}/tests/test-changes"
    )

    # A reviewer asking for a file outside the test home is reported by the
    # provider and disconnected by the server, without explanation...
    async with websockets.connect(
        f"ws://127.0.0.1:{server.port}/review/{review.review_id}"
        ) as reviewer:
      await reviewer.send(encode_message(message(
        'test.src.show-src',
        value = {'id': '../../../../etc/passwd', 'assoc_file': None},
        section = 'test-changes'
      )))
      replies = []
      with pytest.raises(websockets.exceptions.ConnectionClosed) as closed:
        while True:
          tag, value, _ = decode_response(
            await asyncio.wait_for(reviewer.recv(), 10)
          )
          replies.append(tag)
          assert 'root:' not in str(value)
      assert set(replies) == {'review.connection-state'}, replies
      assert closed.value.rcvd is not None
      assert closed.value.rcvd.code == 3001

    # A reviewer on a /batch socket takes the whole socket down with it.
    async with websockets.connect(
        f"ws://127.0.0.1:{server.port}/batch") as batch:
      await batch.send(encode_message(message(
        'batch.subscribe',
        value = {'type': 'review', 'id': str(review.review_id)}
      )))
      tag, value, _ = decode_response(
        await asyncio.wait_for(batch.recv(), 10)
      )
      assert tag == 'batch.subscribe', (tag, value)
      subscription_id = uuid.UUID(value['subscription_id'])
      await batch.send(encode_message(batch_event_message(
        subscription_id,
        message(
          'test.src.show-src',
          value = {'id': '../../../../etc/passwd', 'assoc_file': None},
          section = 'test-changes'
        )
      )))
      with pytest.raises(websockets.exceptions.ConnectionClosed) as closed:
        while True:
          frame = await asyncio.wait_for(batch.recv(), 10)
          assert 'root:' not in str(frame)
      assert closed.value.rcvd is not None
      assert closed.value.rcvd.code == 3001

    # ...while the section keeps serving everyone else.
    async with websockets.connect(
        f"ws://127.0.0.1:{server.port}/review/{review.review_id}"
        ) as reviewer:
      await reviewer.send(encode_message(message(
        'test.src.list-statuses',
        section = 'test-changes'
      )))
      tag = None
      while tag != 'test.src.list-statuses':
        tag, _, _ = decode_response(
          await asyncio.wait_for(reviewer.recv(), 10)
        )
        assert tag not in ('test.missing-src', 'illegal-access', 'invalid')
    await _wait_for_sources(server.port, review.review_id, ['test-changes'])

    # After the server restarts (losing all state), the section is
    # re-registered by the client library.
    server.stop()
    await server.start()
    await _wait_for_sources(server.port, review.review_id, ['test-changes'])

    await publisher.sync(review, [])
    await _wait_for_sources(server.port, review.review_id, [])
  finally:
    task.cancel()
    await asyncio.gather(task, return_exceptions = True)
    await pool.close()


async def test_reviewer_with_outdated_protocol_is_told(server: Server) -> None:
  async with websockets.connect(
      f"ws://127.0.0.1:{server.port}/review/{uuid.uuid4()}") as reviewer:
    await reviewer.send(json.dumps({
      't': 'test.src.list-statuses',
      'r': PROTOCOL_REVISION - 1,
      's': 'test-changes',
    }))
    tags = []
    while 'outdated-proto' not in tags:
      frame = json.loads(await asyncio.wait_for(reviewer.recv(), 10))
      tags.append(frame['t'])


async def test_review_names_are_rich_text(server: Server) -> None:
  review_id = uuid.uuid4()
  async with websockets.connect(
      f"ws://127.0.0.1:{server.port}/source/{review_id}/test/test-changes"
      ) as provider:
    # Only well-formed rich names are accepted.
    for bad_name in [
        'edg/compiler#1: test',
        [],
        [{'type': 'text', 'text': 'test', 'color': 'red'}],
        [{'type': 'link', 'url': 'javascript:alert(1)', 'text': 'test'}],
        [{'type': 'heading', 'text': 'test'}]]:
      await provider.send(encode_message(message(
        'meta.review.name',
        value = bad_name
      )))
      tag, _, _ = decode_response(await asyncio.wait_for(provider.recv(), 10))
      assert tag == 'invalid', bad_name

    # Unnamed, a review is named by its id.
    unnamed = [rich_text(str(review_id))]
    assert await _names(server.port, review_id) == (unnamed, unnamed)

    name = [rich_link('https://forge.example/edg/compiler/pull/1')]
    await provider.send(encode_message(message(
      'meta.review.name',
      value = name
    )))
    for _ in range(100):
      if await _names(server.port, review_id) == (name, name):
        break
      await asyncio.sleep(0.1)
    assert await _names(server.port, review_id) == (name, name)
