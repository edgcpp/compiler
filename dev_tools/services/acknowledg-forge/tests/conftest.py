# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Shared fixtures: an origin repository, service wiring, and fakes.'''

import asyncio
import io
import json
import os
import socket
import subprocess
import sys
import tarfile
import zipfile

from pathlib import Path
from typing import AsyncIterator, Iterator, Optional

import pytest
import pytest_asyncio

sys.path.insert(0, str(Path(__file__).parent))

from edgacknowledg.forge import (
  ArtifactInfo,
  CIConclusion,
  CIRunInfo,
  CIStatus,
  PullRequestInfo,
  PullRequestState,
)
from edgacknowledg.forge.announcer import Announcer
from edgacknowledg.forge.config import ForgeServiceConfig
from edgacknowledg.forge.db import ForgeStore
from edgacknowledg.forge.ingest import Ingestor
from edgacknowledg.forge.local import local_runner
from edgacknowledg.forge.poller import sync_all
from edgacknowledg.forge.processor import Processor
from edgacknowledg.forge.sandbox import RunnerClient, Sandbox
from edgacknowledg.forge.workspace import Workspaces

from fakes import (
  REPOSITORY,
  Clock,
  FakeCommenter,
  FakeForge,
  FakePublisher,
)

# Stands in for edg-rematerialize-test-run: records which files the sparse
# checkout made visible and lists the changed tests in changes.elist (unless
# tests/.edgy/no-elist exists, mimicking the real tool's current gap).
STUB_REMATERIALIZE = '''#!/usr/bin/env python3
import argparse, os, re
from pathlib import Path
parser = argparse.ArgumentParser()
parser.add_argument('--runs-dir', type = Path)
parser.add_argument('--run-timestamp')
parser.add_argument('diff_file')
args = parser.parse_args()
home = Path(os.environ['EDG_TEST_HOME'])
run = args.runs_dir / args.run_timestamp
config_dir = run / 'edg_x86_64'
config_dir.mkdir(parents = True)
diff = open(args.diff_file).read()
pattern = r'^diff --git a/tests/tests/(\\S+) b/'
tests = sorted(set(re.findall(pattern, diff, re.M)))
if not (home / '.edgy' / 'no-elist').exists():
  (config_dir / 'changes.elist').write_text(''.join(t + '\\n' for t in tests))
visible = sorted(
  str(p.relative_to(home)) for p in home.rglob('*')
  if p.is_file() and 'runs' not in p.relative_to(home).parts
)
(run / 'visible.txt').write_text('\\n'.join(visible) + '\\n')
'''


def git(cwd: Path, *args: str) -> str:
  return subprocess.run(
    ['git', *args],
    cwd = cwd,
    check = True,
    capture_output = True,
    text = True,
    env = {
      'PATH': '/usr/bin:/bin',
      'HOME': str(cwd),
      'GIT_CONFIG_NOSYSTEM': '1',
      'GIT_AUTHOR_NAME': 'Test',
      'GIT_AUTHOR_EMAIL': 'test@example.com',
      'GIT_COMMITTER_NAME': 'Test',
      'GIT_COMMITTER_EMAIL': 'test@example.com',
    }
  ).stdout.strip()


class Origin:
  '''A local "forge" repository with main and pull request refs.'''

  def __init__(self, path: Path) -> None:
    self.path = path
    path.mkdir()
    git(path, 'init', '--quiet', '--initial-branch=main')
    git(path, 'config', 'uploadpack.allowFilter', 'true')
    git(path, 'config', 'uploadpack.allowAnySHA1InWant', 'true')
    files = {
      'README': 'compiler\n',
      'src/cfe.c': 'int main(void) { return 0; }\n',
      'tests/.edgy/config.json': '{}\n',
      'tests/expectations/edg_x86_64/suite.log': 'a.sft.cpp: PASS\n',
      'tests/tests/suite/a.sft.cpp': 'int a;\n',
      'tests/tests/suite/.a.rto/default.1.1.txt': 'old output\n',
      'tests/tests/suite/b.sft.cpp': 'int b;\n',
      'tests/tests/suite/.b.rto/default.1.1.txt': 'b output\n',
      'benchmarks/baselines/project/release-c/x.bnch.cgout':
        'summary: 1000\n',
    }
    for index in range(40):
      files[f"tests/tests/big/t{index}.sft.cpp"] = f"int t{index};\n"
    self.main_sha = self.commit('main', files, 'initial')

  def commit(self,
             branch: str,
             files: dict[str, Optional[str]],
             message: str,
             parent: Optional[str] = None) -> str:
    '''Commit ``files`` (None deletes) on ``branch`` (from ``parent``).'''
    if parent is not None:
      git(self.path, 'checkout', '--quiet', '-B', branch, parent)
    else:
      git(self.path, 'checkout', '--quiet', '-B', branch)
    for name, content in files.items():
      path = self.path / name
      if content is None:
        path.unlink()
        continue
      path.parent.mkdir(parents = True, exist_ok = True)
      path.write_text(content)
    git(self.path, 'add', '--all')
    git(self.path, 'commit', '--quiet', '-m', message)
    git(self.path, 'checkout', '--quiet', '--detach')
    return git(self.path, 'rev-parse', 'HEAD')

  def set_ref(self, ref: str, sha: str) -> None:
    git(self.path, 'update-ref', ref, sha)

  def push_pull_request(self,
                        number: int,
                        files: dict[str, Optional[str]],
                        parent: Optional[str] = None) -> str:
    sha = self.commit(
      f"pr-{number}",
      files,
      f"pull request {number}",
      parent = parent or self.main_sha
    )
    self.set_ref(f"refs/pull/{number}/head", sha)
    return sha


def pull_request_info(number: int,
                      head_sha: str,
                      base_sha: str,
                      *,
                      state: PullRequestState = PullRequestState.OPEN,
                      clock: Optional[Clock] = None) -> PullRequestInfo:
  return PullRequestInfo(
    repository = REPOSITORY,
    number = number,
    state = state,
    title = f"Pull request {number}",
    author = 'someone',
    url = f"https://forge.example/{REPOSITORY}/pull/{number}",
    base_ref = 'main',
    base_sha = base_sha,
    head_sha = head_sha,
    fetch_ref = f"refs/pull/{number}/head",
    updated_at = (clock or Clock())(),
  )


def make_test_artifact(path: Path,
                       tested_sha: str,
                       tests: list[str]) -> None:
  '''A test-results-all style zip: ci-meta.json plus one config's results.'''
  with zipfile.ZipFile(path, 'w') as archive:
    archive.writestr('ci-meta.json', json.dumps({'tested_sha': tested_sha}))
    archive.writestr(
      'edg_x86_64/changes.elist',
      ''.join(f"{test}\n" for test in tests)
    )
    archive.writestr('edg_x86_64/changes.elog', 'FAIL\n')
    for test in tests:
      payload = io.BytesIO()
      with tarfile.open(fileobj = payload, mode = 'w:gz') as tarball:
        data = b'output\n'
        info = tarfile.TarInfo('output.txt')
        info.size = len(data)
        tarball.addfile(info, io.BytesIO(data))
      stem = test.rsplit('.', 1)[0]
      archive.writestr(f"edg_x86_64/{stem}.rt.tar.gz", payload.getvalue())


def ci_run(head_sha: str,
           *,
           run_id: str = '100',
           attempt: int = 1,
           status: CIStatus = CIStatus.COMPLETED,
           conclusion: Optional[CIConclusion] = CIConclusion.FAILURE,
           artifacts: tuple[ArtifactInfo, ...] = ()) -> CIRunInfo:
  return CIRunInfo(
    run_id = run_id,
    attempt = attempt,
    head_sha = head_sha,
    status = status,
    conclusion = conclusion if status == CIStatus.COMPLETED else None,
    completed_at = None,
    url = f"https://forge.example/runs/{run_id}",
    artifacts = artifacts,
  )


def make_tools_dir(path: Path) -> Path:
  '''A frozen-tools directory holding the stub rematerialize tool.'''
  (path / 'bin').mkdir(parents = True, exist_ok = True)
  (path / 'pylibs').mkdir(exist_ok = True)
  (path / 'bin' / 'edg-rematerialize-test-run').write_text(STUB_REMATERIALIZE)
  return path


# The builder runner the local_builder fixture serves for the test, which
# make_config points configurations at.
_BUILDER_SERVICE_URL = 'ACKNOWLEDG_FORGE_BUILDER_SERVICE_URL'


@pytest_asyncio.fixture(autouse = True)
async def local_builder(tmp_path: Path,
                        monkeypatch: pytest.MonkeyPatch
                        ) -> AsyncIterator[RunnerClient]:
  '''A builder runner server for this test's service home (``tmp_path /
  'home'``, as every test has), served locally as the service serves its
  own without containers; the containers are tested on their own
  (test_containers.py).
  '''
  async with local_runner(
      'builder',
      home = tmp_path / 'home',
      tools_dir = make_tools_dir(tmp_path / 'tools')) as client:
    monkeypatch.setenv(_BUILDER_SERVICE_URL, client.url)
    yield client


def make_config(home: Path, **overrides: str) -> ForgeServiceConfig:
  environ = {
    'ACKNOWLEDG_FORGE_HOME': str(home),
    'ACKNOWLEDG_FORGE_TOOLS_DIR': str(make_tools_dir(home.parent / 'tools')),
    'ACKNOWLEDG_FORGE_REPOSITORIES': REPOSITORY,
    'ACKNOWLEDG_FORGE_CI_WORKFLOW': 'ci.yml',
    'ACKNOWLEDG_FORGE_COMMENTS': '1',
    'ACKNOWLEDG_FORGE_UNSANDBOXED': '1',
  }
  if _BUILDER_SERVICE_URL in os.environ:
    environ[_BUILDER_SERVICE_URL] = os.environ[_BUILDER_SERVICE_URL]
  environ.update(
    {f"ACKNOWLEDG_FORGE_{key}": value for key, value in overrides.items()}
  )
  return ForgeServiceConfig.from_environment(environ)


class Harness:
  '''The service's components wired to fakes, driven step by step.'''

  def __init__(self,
               store: ForgeStore,
               forge: FakeForge,
               config: ForgeServiceConfig,
               clock: Clock,
               publisher: Optional[FakePublisher] = None,
               commenter: Optional[FakeCommenter] = None) -> None:
    self.store = store
    self.forge = forge
    self.config = config
    self.clock = clock
    self.publisher = publisher or FakePublisher()
    self.commenter = commenter or FakeCommenter()
    self.wake = asyncio.Event()
    config.home.mkdir(parents = True, exist_ok = True)
    self.sandbox = Sandbox(
      builder = (RunnerClient(config.builder_service_url)
                 if config.builder_service_url is not None else None)
    )
    self.workspaces = Workspaces(config.home, forge)
    self.ingestor = Ingestor(
      store,
      forge,
      config,
      wake = self.wake,
      clock = clock
    )
    self.processor = Processor(
      store = store,
      forge = forge,
      config = config,
      workspaces = self.workspaces,
      sandbox = self.sandbox,
      publisher = self.publisher,
      wake = self.wake,
      clock = clock
    )
    self.announcer = Announcer(
      store = store,
      forge_name = forge.name,
      commenter = self.commenter,
      config = config,
      publisher = self.publisher,
      busy = lambda: self.processor.busy_pull_requests,
      clock = clock
    )

  async def start(self) -> None:
    await self.announcer.start()
    await self.processor.restore()

  async def step(self) -> None:
    '''Poll, process until nothing is due, then announce.'''
    await sync_all(self.ingestor, self.config)
    for _ in range(10):
      await self.processor.start_due()
      if not self.processor.busy_pull_requests:
        break
      await self.processor.drain()
    await self.announcer.sweep()


@pytest.fixture
def unreachable_port() -> Iterator[int]:
  '''A local port nothing listens on, for the whole test: it is held (bound,
  never listening), so connecting to it is refused, and nothing else can
  take it meanwhile.
  '''
  with socket.socket() as held:
    held.bind(('127.0.0.1', 0))
    yield int(held.getsockname()[1])


@pytest.fixture
def unreachable_url(unreachable_port: int) -> str:
  '''A WebSocket URL nothing answers at (see ``unreachable_port``).'''
  return f"ws://127.0.0.1:{unreachable_port}"


@pytest.fixture
def clock() -> Clock:
  return Clock()


@pytest.fixture
def origin(tmp_path: Path) -> Origin:
  return Origin(tmp_path / 'origin')


@pytest.fixture
def forge(origin: Origin) -> FakeForge:
  return FakeForge(origin.path)


# Set to run the database tests against another database (e.g.
# postgresql+asyncpg://...); its schema is reset after each test.
TEST_DATABASE_URL = os.environ.get('ACKNOWLEDG_FORGE_TEST_DATABASE_URL')


async def _reset_database(url: str) -> None:
  from sqlalchemy import text
  from sqlalchemy.ext.asyncio import create_async_engine

  from edgacknowledg.forge.db.models import Base

  engine = create_async_engine(url)
  async with engine.begin() as connection:
    await connection.run_sync(Base.metadata.drop_all)
    await connection.execute(text('DROP TABLE IF EXISTS alembic_version'))
  await engine.dispose()


@pytest_asyncio.fixture
async def store() -> AsyncIterator[ForgeStore]:
  url = TEST_DATABASE_URL or 'sqlite+aiosqlite://'
  if TEST_DATABASE_URL:
    await _reset_database(url)
  store = await ForgeStore.open(url)
  yield store
  await store.aclose()
  if TEST_DATABASE_URL:
    await _reset_database(url)


@pytest.fixture
def home(tmp_path: Path) -> Iterator[Path]:
  yield tmp_path / 'home'
