# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''AcknowledgPublisher against a real AcknowlEDG server.'''

import asyncio
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

from edgacknowledg import decode_response, encode_message, message
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.publisher import (
  KIND_TEST_CHANGES,
  AcknowledgPublisher,
  ReviewIdentity,
)
from edgacknowledg.forge.sandbox import Sandbox

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


async def test_publish_heal_and_survive_bad_requests(
                                                  server: Server,
                                                  section: SectionRecord
                                                  ) -> None:
  publisher = AcknowledgPublisher(
    api_host = '127.0.0.1',
    api_port = server.port,
    api_key = None,
    ui_host = 'ui.example',
    ui_port = 443,
    sandbox = Sandbox(enabled = False)
  )
  task = asyncio.create_task(publisher.run())
  try:
    await asyncio.wait_for(publisher.wait_connected(), 30)
    review = ReviewIdentity(uuid.uuid4(), 'edg/compiler#1: test', '')
    await publisher.sync(review, [section])
    await _wait_for_sources(server.port, review.review_id, ['test-changes'])
    assert publisher.section_url(review.review_id, section) == (
      f"https://ui.example/review/{review.review_id}/tests/test-changes"
    )

    # A reviewer asking for a file outside the test home is refused (the
    # provider logs it; a bare 'invalid' can't be routed back to the
    # reviewer), and the section keeps answering afterwards.
    async with websockets.connect(
        f"ws://127.0.0.1:{server.port}/review/{review.review_id}"
        ) as reviewer:
      await reviewer.send(encode_message(message(
        'test.src.show-src',
        value = {'id': '../../../../etc/passwd', 'assoc_file': None},
        section = 'test-changes'
      )))
      await reviewer.send(encode_message(message(
        'test.src.list-statuses',
        section = 'test-changes'
      )))
      replies = []
      while 'test.src.list-statuses' not in replies:
        tag, value, _ = decode_response(
          await asyncio.wait_for(reviewer.recv(), 10)
        )
        replies.append(tag)
        assert tag != 'test.missing-src', replies
        assert 'root:' not in str(value)
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
