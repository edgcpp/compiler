# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''SubprocessPublisher: serving reviewers from a separate, sandboxed
process that can't see the service's secrets.
'''

import asyncio
import shutil
import subprocess
import sys
import uuid

from pathlib import Path
from typing import Any

import pytest
import websockets

from edgacknowledg import decode_response, encode_message, message
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.publisher import (
  KIND_TEST_CHANGES,
  ReviewIdentity,
  SubprocessPublisher,
)
from edgacknowledg.forge.sandbox import Sandbox

from test_acknowledg_integration import (
  Server,
  _wait_for_sources,
  pytestmark,
  server,
)

__all__ = ['pytestmark', 'server']

API_KEY = 'acknowledg-api-key-value'


def _layout(tmp_path: Path) -> tuple[Path, Path, SectionRecord]:
  '''A service home with a database and clones, and one review in it.'''
  home = tmp_path / 'home'
  reviews = home / 'reviews'
  (home / 'repos').mkdir(parents = True)
  (home / 'repos' / 'clone-file').write_text('clone\n')
  (home / 'state.db').write_text('database\n')
  tests_home = reviews / str(uuid.uuid4()) / 'base' / 'tests'
  (tests_home / 'tests' / 'suite').mkdir(parents = True)
  (tests_home / 'tests' / 'suite' / 'a.sft.cpp').write_text('int a;\n')
  root = tests_home / 'runs' / 'r1'
  (root / 'edg_x86_64').mkdir(parents = True)
  (root / 'edg_x86_64' / 'changes.elist').write_text('suite/a.sft.cpp\n')
  return home, reviews, SectionRecord(
    kind = KIND_TEST_CHANGES,
    name = 'Test - Changes',
    slug = 'test-changes',
    source_key = 'k1',
    content_fingerprint = 'f1',
    root = root,
    home = tests_home,
  )


async def _sandbox() -> Sandbox:
  sandbox = Sandbox(enabled = shutil.which('bwrap') is not None)
  await sandbox.check()
  return sandbox


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


async def test_worker_serves_and_recovers(tmp_path: Path,
                                          server: Server) -> None:
  home, reviews, section = _layout(tmp_path)
  publisher = SubprocessPublisher(
    api_host = '127.0.0.1',
    api_port = server.port,
    api_key = None,
    ui_host = 'ui.example',
    ui_port = 443,
    sandbox = await _sandbox(),
    reviews_dir = reviews,
    home = home,
    restart_delay = 0.1
  )
  task = asyncio.create_task(publisher.run())
  try:
    await asyncio.wait_for(publisher.wait_connected(), 30)
    review = ReviewIdentity(uuid.uuid4(), 'edg/compiler#1: test', '')
    await publisher.sync(review, [section])
    await _wait_for_sources(server.port, review.review_id, ['test-changes'])
    value = await _show_source(server.port, review.review_id)
    assert 'int a;' in str(value)

    # The worker dies: its sections go with its connection, and come back
    # with the restarted worker.
    process = publisher._process
    assert process is not None
    process.kill()
    await _wait_for_sources(server.port, review.review_id, [])
    await asyncio.wait_for(publisher.wait_connected(), 30)
    await _wait_for_sources(server.port, review.review_id, ['test-changes'])

    await publisher.remove(review.review_id)
    await _wait_for_sources(server.port, review.review_id, [])
  finally:
    task.cancel()
    await asyncio.gather(task, return_exceptions = True)


@pytest.mark.skipif(shutil.which('bwrap') is None,
                    reason = 'bubblewrap is not installed')
async def test_worker_sees_only_reviews(tmp_path: Path) -> None:
  home, reviews, section = _layout(tmp_path)
  credential = tmp_path / 'credentials' / 'forge-token'
  credential.parent.mkdir()
  credential.write_text('forge-token-value\n')
  publisher = SubprocessPublisher(
    api_host = '127.0.0.1',
    api_port = 1,
    api_key = API_KEY,
    ui_host = 'ui.example',
    ui_port = 443,
    sandbox = await _sandbox(),
    reviews_dir = reviews,
    home = home
  )
  command = publisher._worker_command()
  # The API key goes over stdin, not the command line or environment.
  assert API_KEY not in ' '.join(command)

  # The worker's sandbox, running a probe instead of the worker.
  prefix = command[:command.index(sys.executable)]
  test_file = section.home / 'tests' / 'suite' / 'a.sft.cpp'
  probe = subprocess.run(
    [
      *prefix, 'sh', '-c',
      f"cat {home / 'state.db'} {home / 'repos' / 'clone-file'} "
      f"{credential} 2>/dev/null; "
      f"cat {test_file}; "
      f"touch {section.root / 'tampered'} 2>/dev/null && echo WROTE; "
      f"{sys.executable} -c 'import edgacknowledg.forge.publisher_worker' "
      f"&& echo IMPORTS-OK"
    ],
    capture_output = True,
    text = True,
    timeout = 60
  )
  assert 'int a;' in probe.stdout, probe
  assert 'IMPORTS-OK' in probe.stdout, probe
  assert 'database' not in probe.stdout
  assert 'clone' not in probe.stdout
  assert 'forge-token-value' not in probe.stdout
  assert 'WROTE' not in probe.stdout
