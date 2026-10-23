# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The publisher server's process pool: reviewers' requests answered in pool
processes, which are replaced when they miss their deadline or exit.
'''

import asyncio
import os
import signal
import time
import uuid

from dataclasses import dataclass
from pathlib import Path
from typing import Any, AsyncIterator

import pytest
import pytest_asyncio

from edgacknowledg import decode_response, encode_message, message
from edgacknowledg.client.shared import IllegalFileAccessError
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.local import local_runner
from edgacknowledg.forge.publisher import KIND_TEST_CHANGES
from edgacknowledg.forge.publisher import pool as pool_module
from edgacknowledg.forge.publisher.pool import (
  PoolError,
  ProcessPool,
  UnixSocketHandler,
)
from edgacknowledg.forge.runner.client import RunnerClient

from conftest import make_tools_dir

# Stands in for ifc-dump in the tools runner: hangs on files named slow*.
STUB_IFC_DUMP = '''#!/bin/sh
case "$1" in
  */slow*) sleep 60 ;;
esac
echo "dumped $(basename "$1")"
'''

REVIEWER = 'reviewer-1'


@dataclass
class _Env:
  section: SectionRecord
  tools: RunnerClient


@pytest_asyncio.fixture
async def env(tmp_path: Path) -> AsyncIterator[_Env]:
  home = tmp_path / 'home'
  tests_home = home / 'reviews' / str(uuid.uuid4()) / 'base' / 'tests'
  (tests_home / 'tests' / 'suite').mkdir(parents = True)
  for name in ('a.ifc', 'slow.ifc'):
    (tests_home / 'tests' / 'suite' / name).write_text('ifc\n')
  root = tests_home / 'runs' / 'r1'
  (root / 'edg_x86_64').mkdir(parents = True)
  (root / 'edg_x86_64' / 'changes.elist').write_text('suite/a.ifc\n')
  tools_dir = make_tools_dir(tmp_path / 'tools')
  ifc_dump = tools_dir / 'bin' / 'ifc-dump'
  ifc_dump.write_text(STUB_IFC_DUMP)
  ifc_dump.chmod(0o755)
  section = SectionRecord(
    kind = KIND_TEST_CHANGES,
    name = 'Test - Changes',
    slug = 'test-changes',
    source_key = 'k1',
    content_fingerprint = 'f1',
    root = root,
    home = tests_home,
  )
  async with local_runner('tools', home = home,
                          tools_dir = tools_dir) as tools:
    yield _Env(section, tools)


@pytest_asyncio.fixture
async def make_pool(env: _Env) -> AsyncIterator[Any]:
  pools: list[ProcessPool] = []

  async def make(processes: int = 2,
                 request_timeout: float = 30.0) -> ProcessPool:
    pool = ProcessPool(
      tools_service_url = env.tools.url,
      processes = processes,
      request_timeout = request_timeout
    )
    pools.append(pool)
    await pool.start()
    return pool

  yield make
  for pool in pools:
    await pool.close()


def _show_source(test_id: str) -> str:
  return encode_message(message(
    'test.src.show-src',
    value = {'id': test_id, 'assoc_file': None, 'reviewer_id': REVIEWER},
    section = 'test-changes'
  ))


def _content(answer: pool_module.Answer) -> str:
  [reply] = answer.replies
  tag, value, _ = decode_response(encode_message(reply))
  assert tag == 'test.src.show-src', reply
  return str(value['client_response']['content'])


async def test_requests_are_answered_in_the_pool(env: _Env,
                                                 make_pool: Any) -> None:
  pool = await make_pool()
  answer = await pool.answer(env.section, _show_source('suite/a.ifc'))
  assert answer.error is None
  assert 'dumped a.ifc' in _content(answer)
  assert os.getpid() not in pool.pids() and len(pool.pids()) == 2


async def test_a_slow_request_is_cut_off_alone(env: _Env,
                                               make_pool: Any) -> None:
  pool = await make_pool(processes = 2, request_timeout = 3.0)
  pids = pool.pids()
  # Round-robin: the slow request takes the first process...
  slow = asyncio.create_task(
    pool.answer(env.section, _show_source('suite/slow.ifc'))
  )
  await asyncio.sleep(0.5)
  # ...and the next one, on the second, isn't held up by it.
  started = time.monotonic()
  answer = await pool.answer(env.section, _show_source('suite/a.ifc'))
  assert 'dumped a.ifc' in _content(answer)
  assert time.monotonic() - started < 2.5
  with pytest.raises(PoolError, match = 'timed out'):
    await slow
  # Its process was replaced, and the replacement answers.
  answer = await pool.answer(env.section, _show_source('suite/a.ifc'))
  assert 'dumped a.ifc' in _content(answer)
  assert pool.pids()[0] != pids[0] and pool.pids()[1] == pids[1]


async def test_an_exited_process_is_replaced(env: _Env,
                                             make_pool: Any) -> None:
  pool = await make_pool(processes = 1)
  [pid] = pool.pids()
  # It dies while idle: the next request goes to its replacement.
  os.kill(pid, signal.SIGKILL)
  await asyncio.sleep(0.5)
  answer = await pool.answer(env.section, _show_source('suite/a.ifc'))
  assert 'dumped a.ifc' in _content(answer)
  [replacement] = pool.pids()
  assert replacement != pid

  # It dies while answering: that request fails; the next is answered.
  slow = asyncio.create_task(
    pool.answer(env.section, _show_source('suite/slow.ifc'))
  )
  await asyncio.sleep(0.5)
  os.kill(replacement, signal.SIGKILL)
  with pytest.raises(PoolError, match = 'exited'):
    await slow
  answer = await pool.answer(env.section, _show_source('suite/a.ifc'))
  assert 'dumped a.ifc' in _content(answer)


async def test_a_process_failing_to_start_backs_off(
    env: _Env,
    monkeypatch: pytest.MonkeyPatch) -> None:
  monkeypatch.setattr(pool_module, 'POOL_PROCESS_MODULE',
                      'edgacknowledg.forge.no_such_module')
  pool = ProcessPool(tools_service_url = env.tools.url, processes = 1)
  try:
    with pytest.raises(PoolError, match = 'failed to start'):
      await pool.start()
    # The next try waits; meanwhile requests fail at once.
    started = time.monotonic()
    with pytest.raises(PoolError, match = 'restarting'):
      await pool.answer(env.section, _show_source('suite/a.ifc'))
    assert time.monotonic() - started < 1.0
  finally:
    await pool.close()


class _Channel:
  '''One request in; the replies the stand-in handler relays, out.'''

  def __init__(self, request: str) -> None:
    self._request = request
    self.sent: list[Any] = []

  async def recv(self) -> str:
    return self._request

  async def send(self, msg: Any) -> None:
    self.sent.append(msg)


async def test_the_stand_in_handler_relays_and_raises(
    env: _Env,
    make_pool: Any) -> None:
  pool = await make_pool(processes = 1)
  handler = UnixSocketHandler(env.section, pool)
  assert (handler.subscription_type, handler.section_name, handler.slug) == (
    'test.src', 'Test - Changes', 'test-changes'
  )

  channel = _Channel(_show_source('suite/a.ifc'))
  await handler.handle_request(channel)  # type: ignore[arg-type]
  [reply] = channel.sent
  assert 'dumped a.ifc' in str(reply)

  # A file outside the section: the client library drops the reviewer.
  channel = _Channel(_show_source('../../../../etc/passwd'))
  with pytest.raises(IllegalFileAccessError) as caught:
    await handler.handle_request(channel)  # type: ignore[arg-type]
  assert caught.value.reviewer_id == REVIEWER

  # Any other failure: the client library sends its failure reply.
  broken = UnixSocketHandler(
    SectionRecord(**{**env.section.__dict__, 'kind': 'no-such-kind'}),
    pool
  )
  with pytest.raises(PoolError, match = 'failed'):
    await broken.handle_request(  # type: ignore[arg-type]
      _Channel(_show_source('suite/a.ifc'))
    )
