# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Symlinks (and other non-regular files) in pull requests, CI artifacts,
and sandbox output must never let reviewers read outside the review.
'''

import io
import json
import os
import shutil
import tarfile
import uuid

from pathlib import Path, PurePosixPath
from typing import Any, Optional, Sequence

import pytest

from edgacknowledg import RequestTags, encode_message, message
from edgacknowledg.client.bench import (
  BenchReviewSession,
  collect_all_benchmarks,
  read_program_totals,
  resolve_baseline_dir,
)
from edgacknowledg.client.shared import (
  DEFAULT_MAX_FILE_SIZE,
  CompletedCommand,
  ConfinedPathComponent,
  FileTooLargeError,
  SandboxContext,
)
# (Not 'from ... import TestSourceHandler', which pytest would collect.)
import edgacknowledg.client.shared as shared
import edgacknowledg.client.test as test_provider

from edgacknowledg.client.test import (
  IllegalFileAccessError,
  _test_if_any_output_exists,
)
from edgacknowledg.forge.db import ForgeStore, SectionRecord
from edgtest.runtest import RunTestOutput, RunTestOutputLinkError
from edgacknowledg.forge.publisher import KIND_BENCH, KIND_TEST_FAILURES
from edgacknowledg.forge.publisher.acknowledg import AcknowledgPublisher
from edgacknowledg.forge.publisher.pool import ProcessPool
from edgacknowledg.forge.workspace import (
  WorkspaceError,
  Workspaces,
  check_plain_tree,
  require_real_directory,
)

from conftest import (
  STUB_REMATERIALIZE,
  Harness,
  Origin,
  git,
  make_config,
  pull_request_info,
)
from fakes import REPOSITORY, Clock, FakeForge

SECRET = 'forge-token-value\n'


@pytest.fixture
def secret(tmp_path: Path) -> Path:
  '''Stands in for a credential (or the database) outside every review.'''
  path = tmp_path / 'credentials' / 'forge-token'
  path.parent.mkdir()
  path.write_text(SECRET)
  return path


def _push_with_link(origin: Origin,
                    number: int,
                    link: str,
                    target: str) -> str:
  '''A pull request replacing (or adding) ``link`` with a symlink.'''
  git(origin.path, 'checkout', '--quiet', '-B', f"pr-{number}",
      origin.main_sha)
  path = origin.path / link
  path.parent.mkdir(parents = True, exist_ok = True)
  if path.is_dir() and not path.is_symlink():
    shutil.rmtree(path)
  else:
    path.unlink(missing_ok = True)
  os.symlink(target, path)
  git(origin.path, 'add', '--all')
  git(origin.path, 'commit', '--quiet', '-m', f"pull request {number}")
  git(origin.path, 'checkout', '--quiet', '--detach')
  sha = git(origin.path, 'rev-parse', 'HEAD')
  origin.set_ref(f"refs/pull/{number}/head", sha)
  return sha


# Workspaces


def test_check_plain_tree(tmp_path: Path, secret: Path) -> None:
  root = tmp_path / 'tree'
  (root / 'dir').mkdir(parents = True)
  (root / 'dir' / 'file').write_text('x')
  check_plain_tree(root)

  os.symlink(secret, root / 'dir' / 'link')
  with pytest.raises(WorkspaceError):
    check_plain_tree(root)
  (root / 'dir' / 'link').unlink()

  os.mkfifo(root / 'fifo')
  with pytest.raises(WorkspaceError):
    check_plain_tree(root)
  (root / 'fifo').unlink()

  os.link(root / 'dir' / 'file', root / 'hard')
  check_plain_tree(root)
  with pytest.raises(WorkspaceError):
    check_plain_tree(root, allow_hard_links = False)


def test_require_real_directory(tmp_path: Path) -> None:
  (tmp_path / 'a' / 'b').mkdir(parents = True)
  os.symlink(tmp_path / 'a', tmp_path / 'link')
  assert require_real_directory(tmp_path, PurePosixPath('a', 'b')) == (
    tmp_path / 'a' / 'b'
  )
  for relative in ('link/b', 'a/missing', 'a/../a'):
    with pytest.raises(WorkspaceError):
      require_real_directory(tmp_path, PurePosixPath(relative))
  assert not (tmp_path / 'a' / 'missing').exists()


async def test_checkout_neutralizes_committed_symlinks(tmp_path: Path,
                                                       origin: Origin,
                                                       forge: FakeForge,
                                                       secret: Path) -> None:
  link = 'tests/tests/suite/.a.rto/default.1.1.txt'
  sha = _push_with_link(origin, 1, link, str(secret))
  workspaces = Workspaces(tmp_path / 'home', forge)
  info = pull_request_info(1, sha, origin.main_sha)
  head, _ = await workspaces.fetch_pull_request(info)
  worktree = await workspaces.checkout(info, 'r1', 'head', head, ['/tests/'])

  # A plain file holding the link's target, not the secret.
  checked_out = worktree / link
  assert not checked_out.is_symlink()
  assert checked_out.read_text() == str(secret)
  config = git(
    workspaces.clone_dir(REPOSITORY), 'config', '--get', 'core.symlinks'
  )
  assert config == 'false'

  # A worktree that somehow holds a link anyway is refused.
  os.symlink(secret.parent, worktree / 'tests' / 'planted')
  with pytest.raises(WorkspaceError):
    await workspaces.checkout(info, 'r1', 'head', head, ['/tests/'])


# Processing


async def test_links_in_sandbox_output_are_not_served(tmp_path: Path,
                                                      origin: Origin,
                                                      forge: FakeForge,
                                                      store: ForgeStore,
                                                      clock: Clock,
                                                      secret: Path) -> None:
  config = make_config(tmp_path / 'home')
  # A rematerializer that (as a patch applying a symlink diff could) leaves
  # a link to a service file in its output.
  (config.tools_dir / 'bin' / 'edg-rematerialize-test-run').write_text(
    STUB_REMATERIALIZE +
    f"os.symlink({str(secret)!r}, config_dir / 'changes.elog')\n"
  )
  harness = Harness(store, forge, config, clock)
  await harness.start()
  head = origin.push_pull_request(1, {
    'tests/tests/suite/.a.rto/default.1.1.txt': 'new output\n',
  })
  forge.set_pull_request(pull_request_info(1, head, origin.main_sha))
  await harness.step()

  record = await store.find_pull_request('fake', REPOSITORY, 1)
  assert record is not None
  assert harness.publisher.served_names(record.review_id) == []
  # Nothing of it is left behind to be served later.
  runs = tmp_path / 'home' / 'reviews' / str(record.review_id) / 'base'
  assert list((runs / 'tests' / 'runs').iterdir()) == []


async def test_linked_bench_baselines_are_not_served(tmp_path: Path,
                                                     origin: Origin,
                                                     forge: FakeForge,
                                                     store: ForgeStore,
                                                     clock: Clock) -> None:
  from conftest import ci_run
  from edgacknowledg.forge import ArtifactInfo, CIConclusion
  import zipfile

  config = make_config(tmp_path / 'home')
  harness = Harness(store, forge, config, clock)
  await harness.start()
  # The baselines directory is a link (to another review, say).
  head = _push_with_link(
    origin, 1, 'benchmarks/baselines/project/release-c', '../../../..'
  )
  forge.set_pull_request(pull_request_info(1, head, origin.main_sha))
  artifact = tmp_path / 'bench.zip'
  with zipfile.ZipFile(artifact, 'w') as archive:
    archive.writestr('c/x.bnch.cgout', 'summary: 1100\n')
  forge.artifact_files['b1'] = artifact
  forge.runs[head] = ci_run(
    head,
    conclusion = CIConclusion.SUCCESS,
    artifacts = (ArtifactInfo('bench-results', 'b1', 100, None, False),)
  )
  await harness.step()
  record = await store.find_pull_request('fake', REPOSITORY, 1)
  assert record is not None
  assert harness.publisher.served_names(record.review_id) == []


# Providers


class FakeChannel:
  '''Delivers one request and collects the replies.'''

  def __init__(self, request: Any) -> None:
    self._request = encode_message(request)
    self.sent: list[Any] = []

  async def recv(self) -> str:
    return self._request

  async def send(self, reply: Any) -> None:
    self.sent.append(reply)


def _test_tree(tmp_path: Path, secret: Path) -> tuple[Path, Path]:
  home = tmp_path / 'review' / 'tests'
  suite = home / 'tests' / 'suite'
  suite.mkdir(parents = True)
  (suite / 'a.sft.cpp').write_text('int a;\n')
  os.symlink('a.sft.cpp', suite / 'inside.sft.cpp')
  os.symlink(secret, suite / 'escape.sft.cpp')
  root = home / 'runs' / 'r1'
  (root / 'edg_x86_64').mkdir(parents = True)
  (root / 'edg_x86_64' / 'changes.elist').write_text('suite/a.sft.cpp\n')
  return home, root


async def _show_source(handler: test_provider.TestSourceHandler,
                       test_id: str) -> Optional[str]:
  channel = FakeChannel(message(
    RequestTags.TEST_SHOW_SOURCE,
    value = {'id': test_id, 'assoc_file': None, 'reviewer_id': 'r'}
  ))
  await handler.handle_request(channel)  # type: ignore[arg-type]
  [reply] = channel.sent
  content: Optional[str] = reply['v']['client_response'].get('content')
  return content


async def test_show_source_symlinks(tmp_path: Path, secret: Path) -> None:
  home, root = _test_tree(tmp_path, secret)
  trusted, untrusted = (
    test_provider.TestSourceHandler(
      'Test',
      'test',
      context,
      context.subpath(home),
      context.subpath(root)
    )
    for context in (SandboxContext(home),
                    SandboxContext(home, allow_symlinks = False))
  )

  assert await _show_source(trusted, 'suite/a.sft.cpp') == 'int a;\n'
  assert await _show_source(untrusted, 'suite/a.sft.cpp') == 'int a;\n'
  # A local review may use links within its tree...
  assert await _show_source(trusted, 'suite/inside.sft.cpp') == 'int a;\n'
  # ...but no review may read outside it, and an untrusted one may not
  # follow links at all.
  for handler, test_id in ((trusted, 'suite/escape.sft.cpp'),
                           (untrusted, 'suite/escape.sft.cpp'),
                           (untrusted, 'suite/inside.sft.cpp')):
    with pytest.raises(IllegalFileAccessError) as refused:
      await _show_source(handler, test_id)
    # Naming the reviewer, for the server to disconnect.
    assert refused.value.reviewer_id == 'r'


async def test_illegal_access_is_reported(tmp_path: Path,
                                          secret: Path) -> None:
  from edgacknowledg.client import (
    _reply_to_failed_request,
    _report_illegal_access,
  )

  home, root = _test_tree(tmp_path, secret)
  context = SandboxContext(home, allow_symlinks = False)
  handler = test_provider.TestSourceHandler(
    'Test',
    'test',
    context,
    context.subpath(home),
    context.subpath(root)
  )

  def channel() -> FakeChannel:
    return FakeChannel(message(RequestTags.INVALID_REQUEST))

  # The reviewer is reported, for the server to disconnect...
  reported = channel()
  error = IllegalFileAccessError('outside')
  error.reviewer_id = 'reviewer-1'
  await _report_illegal_access(reported, handler, error)  # type: ignore
  assert reported.sent == [
    message(RequestTags.ILLEGAL_ACCESS, value = {'reviewer_id': 'reviewer-1'})
  ]

  # ...unless it can't be traced to one, like any other failure.
  for send in (
      lambda c: _report_illegal_access(  # type: ignore
        c, handler, IllegalFileAccessError('outside')),
      lambda c: _reply_to_failed_request(c, handler)):  # type: ignore
    invalid = channel()
    try:
      raise ValueError('broken')
    except ValueError:
      await send(invalid)
    assert invalid.sent == [message(RequestTags.INVALID_REQUEST)]


def test_confined_path(tmp_path: Path, secret: Path) -> None:
  root = tmp_path / 'tree'
  (root / 'dir').mkdir(parents = True)
  (root / 'dir' / 'file').write_text('x')
  os.symlink('file', root / 'dir' / 'inside')
  os.symlink(secret, root / 'dir' / 'escape')
  os.symlink(root / 'dir', root / 'linked-dir')
  os.mkfifo(root / 'fifo')
  trusted = SandboxContext(root).subpath(root)
  untrusted = SandboxContext(root, allow_symlinks = False).subpath(root)

  for tree in (trusted, untrusted):
    # Like pathlib.Path...
    file = tree / 'dir' / 'file'
    assert file == tree.joinpath('dir', 'file')
    assert (file.name, file.parent, file.path) == (
      'file', tree / 'dir', root / 'dir' / 'file'
    )
    assert str(file.relative) == 'dir/file'
    assert file.read_text() == 'x' and file.is_file() and not file.is_dir()
    assert tree.subpath(root / 'dir' / 'file') == file
    # Missing files are valid; reading them fails as usual.
    missing = tree / 'dir' / 'missing'
    assert not missing.exists()
    with pytest.raises(FileNotFoundError):
      missing.read_text()
    # ...but nothing can point outside the tree, however it's spelled.
    for escape in (lambda: tree / '..' / 'credentials',
                   lambda: tree / str(secret),
                   lambda: tree.parent,
                   lambda: tree.subpath(secret),
                   lambda: tree / 'dir' / 'escape',
                   lambda: (tree / 'dir').with_name('..')):
      with pytest.raises(IllegalFileAccessError):
        escape()

  # Links staying inside are fine in a trusted tree only.
  assert (trusted / 'linked-dir' / 'inside').read_text() == 'x'
  for components in (('dir', 'inside'), ('linked-dir', 'file'), ('fifo',)):
    with pytest.raises(IllegalFileAccessError):
      untrusted.joinpath(*components)

  # Walking an untrusted tree never yields anything but directories and
  # regular files.
  walked = {
    str((directory / name).relative)
    for directory, dirs, files in untrusted.walk()
    for name in [*dirs, *files]
  }
  assert walked == {'dir', 'dir/file'}

  # Even a link planted after validation isn't followed.
  late = untrusted / 'dir' / 'late'
  os.symlink('file', late.path)
  # (It exists, as an entry, but is no file to read.)
  assert late.exists() and not late.is_file()
  with pytest.raises(IllegalFileAccessError):
    late.read_text()


def test_confined_path_component(tmp_path: Path) -> None:
  (tmp_path / 'run' / 'c').mkdir(parents = True)
  (tmp_path / 'run' / 'c' / 'x.cgout').write_text('run\n')
  (tmp_path / 'base' / 'c').mkdir(parents = True)
  (tmp_path / 'base' / 'c' / 'x.cgout').write_text('base\n')
  context = SandboxContext(tmp_path)
  run = context.subpath(tmp_path / 'run')
  base = context.subpath(tmp_path / 'base')

  component = (run / 'c' / 'x.cgout').relative_to(run)
  assert str(component) == 'c/x.cgout' and component.name == 'x.cgout'
  assert component == (base / 'c' / 'x.cgout').relative_to(base)
  # It carries across trees...
  assert (base / component).read_text() == 'base\n'
  assert base.joinpath('.', component) == base / 'c' / 'x.cgout'
  # ...but is no path to anything else.
  with pytest.raises(TypeError):
    Path('/') / component  # type: ignore[operator]
  with pytest.raises(TypeError):
    open(component)  # type: ignore[call-overload]
  with pytest.raises(TypeError):
    run / Path('c')  # type: ignore[operator]
  for invalid in (['..'], ['a/b'], ['']):
    with pytest.raises(ValueError):
      ConfinedPathComponent(invalid)
  with pytest.raises(ValueError):
    run.relative_to(base)


def test_recordings_are_validated(tmp_path: Path, secret: Path) -> None:
  home, _ = _test_tree(tmp_path, secret)
  recordings = home / 'tests' / 'suite' / '.a.rto'
  recordings.mkdir()
  env = {'EDG_CONFIG': 'edg_x86_64', 'RUN_TEST_DEFAULT_CONFIG': 'edg_x86_64'}
  trusted = SandboxContext(home).subpath(recordings)
  untrusted = SandboxContext(home, allow_symlinks = False).subpath(recordings)

  # What a pull request recording a link to a credential amounts to: before
  # this was checked, the 'current diff' showed the credential.
  os.symlink(secret, recordings / 'default.1.1.txt')
  for tree in (trusted, untrusted):
    with pytest.raises(IllegalFileAccessError):
      _test_if_any_output_exists(tree, env, 1, 1)

  (recordings / 'default.1.1.txt').unlink()
  (recordings / 'real.txt').write_text('output\n')
  os.symlink('real.txt', recordings / 'default.1.1.txt')
  assert _test_if_any_output_exists(trusted, env, 1, 1) == (
    trusted / 'default.1.1.txt'
  )
  with pytest.raises(IllegalFileAccessError):
    _test_if_any_output_exists(untrusted, env, 1, 1)


def test_bench_reads_are_confined(tmp_path: Path) -> None:
  bench_home = tmp_path / 'benchmarks'
  baseline = bench_home / 'baselines' / 'project' / 'release-c'
  run = bench_home / 'runs' / 'ci-1' / 'c'
  baseline.mkdir(parents = True)
  run.mkdir(parents = True)
  inside = baseline / 'real.cgout'
  inside.write_text('events: Ir\nsummary: 1234\n')
  os.symlink(inside, baseline / 'x.bnch.cgout')
  (run / 'x.bnch.cgout').write_text('events: Ir\nsummary: 1100\n')
  (run / 'y.bnch.cgout').write_text('events: Ir\nsummary: 1100\n')
  outside = tmp_path / 'outside.cgout'
  outside.write_text('events: Ir\nsummary: 999\n')
  os.symlink(outside, baseline / 'y.bnch.cgout')

  # A link staying in the baselines works locally; one leaving them never
  # does, and a pull request's baselines may not use links at all.
  trusted = SandboxContext(baseline).subpath(baseline)
  assert read_program_totals(trusted / 'x.bnch.cgout') == 1234
  with pytest.raises(IllegalFileAccessError):
    read_program_totals(trusted / 'y.bnch.cgout')
  # Collecting fails outright on any file it may not read.
  with pytest.raises(IllegalFileAccessError):
    collect_all_benchmarks(SandboxContext(run).subpath(run), trusted)
  (baseline / 'y.bnch.cgout').unlink()
  local = collect_all_benchmarks(SandboxContext(run).subpath(run), trusted)
  assert [str(bm.rel_benchmark_path) for bm in local] == ['x.bnch.cgout']
  with pytest.raises(IllegalFileAccessError):
    collect_all_benchmarks(
      SandboxContext(run, allow_symlinks = False).subpath(run),
      SandboxContext(baseline, allow_symlinks = False).subpath(baseline)
    )


def test_file_size_limit(tmp_path: Path) -> None:
  (tmp_path / 'fits').write_text('0123456789')
  (tmp_path / 'too-large').write_text('0123456789A')
  for allow_symlinks in (True, False):
    context = SandboxContext(
      tmp_path,
      allow_symlinks = allow_symlinks,
      max_file_size = 10
    )
    # The limit carries over to derived paths.
    root = context.subpath(tmp_path)
    assert (root / 'fits').read_text() == '0123456789'
    with pytest.raises(FileTooLargeError):
      (root / 'too-large').read_bytes()
    unlimited = SandboxContext(
      tmp_path,
      allow_symlinks = allow_symlinks,
      max_file_size = None
    )
    assert unlimited.subpath(tmp_path / 'too-large').read_text() == (
      '0123456789A'
    )
  assert SandboxContext(tmp_path).max_file_size == DEFAULT_MAX_FILE_SIZE


async def test_show_source_reads_only_what_is_shown(
    tmp_path: Path,
    secret: Path,
    monkeypatch: pytest.MonkeyPatch) -> None:
  home, root = _test_tree(tmp_path, secret)
  (home / 'tests' / 'suite' / 'big.sft.cpp').write_text('int x;\n' * 1000)
  monkeypatch.setattr(test_provider, 'MAX_DISPLAY_CONTENT_SIZE', 64)
  monkeypatch.setattr(shared, 'MAX_DISPLAY_CONTENT_SIZE', 64)
  reads: list[int] = []
  real_open = shared.ConfinedPath.open

  def open_recording_reads(self: shared.ConfinedPath,
                           *args: Any,
                           **kwargs: Any) -> Any:
    handle = real_open(self, *args, **kwargs)
    real_read = handle.read

    def read(size: int = -1) -> Any:
      reads.append(size)
      return real_read(size)

    handle.read = read
    return handle

  monkeypatch.setattr(shared.ConfinedPath, 'open', open_recording_reads)
  context = SandboxContext(home, allow_symlinks = False)
  handler = test_provider.TestSourceHandler(
    'Test',
    'test',
    context,
    context.subpath(home),
    context.subpath(root)
  )
  channel = FakeChannel(message(
    RequestTags.TEST_SHOW_SOURCE,
    value = {'id': 'suite/big.sft.cpp', 'assoc_file': None,
             'reviewer_id': 'r'}
  ))
  await handler.handle_request(channel)  # type: ignore[arg-type]
  [reply] = channel.sent
  response = reply['v']['client_response']
  assert response['truncated'] and len(response['content']) <= 64
  assert response['content'].startswith('int x;\n')
  assert reads == [65]


async def test_sandbox_context_commands(tmp_path: Path) -> None:
  (tmp_path / 'home').mkdir()
  (tmp_path / 'home' / 'file').write_text('contents\n')
  ran: list[tuple[Sequence[str], float]] = []

  async def runner(args: Sequence[str], timeout: float) -> CompletedCommand:
    ran.append((args, timeout))
    return CompletedCommand(0, b'remote\n', False)

  # A context's commands go to its runner...
  context = SandboxContext(
    tmp_path / 'home',
    allow_symlinks = False,
    command_runner = runner,
    command_timeout = 5
  )
  file = context.subpath(tmp_path / 'home' / 'file')
  result = await context.run_command('tool', '--flag', str(file))
  assert result.output == b'remote\n'
  assert ran == [(('tool', '--flag', str(tmp_path / 'home' / 'file')), 5)]

  # ...or, by default, run here, within its timeout.
  local = SandboxContext(tmp_path, command_timeout = 0.5)
  assert local.allow_symlinks
  assert await local.run_command('cat', str(file)) == (0, b'contents\n',
                                                       False)
  assert (await local.run_command('sleep', '30')).timed_out


def test_permitted_roots(tmp_path: Path, secret: Path) -> None:
  home = tmp_path / 'home'
  (home / 'runs' / 'r1').mkdir(parents = True)
  (home / 'data').write_text('home data\n')
  run = tmp_path / 'downloads' / 'run'
  run.mkdir(parents = True)
  (run / 'output').write_text('output\n')
  os.symlink(home / 'data', run / 'into-home')
  # A run outside the home must be one of the roots...
  with pytest.raises(IllegalFileAccessError):
    SandboxContext(home).subpath(run / 'output')
  permitted = SandboxContext(home, run)
  assert permitted.roots == (home, run)
  assert permitted.subpath(run / 'output').read_text() == 'output\n'
  # ...one inside it (or a duplicate) is folded into it, in any order.
  assert SandboxContext(home / 'runs' / 'r1', home, home).roots == (home,)
  assert SandboxContext(run, home / 'runs', home).roots == (run, home)
  with pytest.raises(ValueError):
    SandboxContext()

  # Each path is confined to its own root: a link from the run into the
  # home (or anywhere else) is refused, as is '..' out of the run.
  output = permitted.subpath(run / 'output')
  assert output.tree_root == permitted.subpath(run)
  for escape in (lambda: permitted.subpath(run / 'into-home'),
                 lambda: output.parent.parent,
                 lambda: output.tree_root / '..' / '..' / 'home' / 'data'):
    with pytest.raises(IllegalFileAccessError):
      escape()
  assert permitted.subpath(home / 'data').read_text() == 'home data\n'


def test_publisher_requires_real_section_directories(
    tmp_path: Path,
    secret: Path,
    unreachable_port: int,
    unreachable_url: str) -> None:
  reviews = tmp_path / 'home' / 'reviews'
  worktree = reviews / str(uuid.uuid4()) / 'head'
  (worktree / 'benchmarks' / 'runs' / 'ci-1' / 'c').mkdir(parents = True)
  publisher = AcknowledgPublisher(
    api_host = '127.0.0.1',
    api_port = unreachable_port,
    api_key = None,
    ui_host = 'ui.example',
    ui_port = 443,
    # Never started: no request is answered here.
    pool = ProcessPool(tools_service_url = unreachable_url),
    reviews_dir = reviews
  )

  # A test home that is a link (as if a pull request committed 'tests').
  os.symlink(secret.parent, worktree / 'tests')
  with pytest.raises(WorkspaceError):
    publisher._build_handler(SectionRecord(
      kind = KIND_TEST_FAILURES,
      name = 'Test - Failures',
      slug = 'test-failures',
      source_key = 'k',
      content_fingerprint = 'f',
      root = worktree / 'tests' / 'runs' / 'ci-1',
      home = worktree / 'tests',
    ))

  # Baselines outside the bench home.
  (tmp_path / 'elsewhere').mkdir()
  with pytest.raises(WorkspaceError):
    publisher._build_handler(SectionRecord(
      kind = KIND_BENCH,
      name = 'Bench - C',
      slug = 'bench-c',
      source_key = 'k',
      content_fingerprint = 'f',
      root = worktree / 'benchmarks' / 'runs' / 'ci-1' / 'c',
      home = worktree / 'benchmarks',
      details = {
        'baseline_dir': str(tmp_path / 'elsewhere'),
        'tag': 'release-c',
      },
    ))


def test_bench_session_trees(tmp_path: Path) -> None:
  bench_home = tmp_path / 'benchmarks'
  baseline = bench_home / 'baselines' / 'project' / 'release-c'
  baseline.mkdir(parents = True)
  (baseline / 'x.bnch.cgout').write_text('events: Ir\nsummary: 1000\n')
  # A local run kept elsewhere (edg-bench-review --root).
  elsewhere = tmp_path / 'downloads' / 'run'
  elsewhere.mkdir(parents = True)
  (elsewhere / 'x.bnch.cgout').write_text('events: Ir\nsummary: 1100\n')

  # Not without permission (as edg-bench-review --root gives)...
  for context in (SandboxContext(bench_home),
                  SandboxContext(bench_home, allow_symlinks = False)):
    with pytest.raises(IllegalFileAccessError):
      context.subpath(elsewhere)
  # ...but with it.
  context = SandboxContext(bench_home, elsewhere)
  local = BenchReviewSession(
    context = context,
    run_dir = context.subpath(elsewhere),
    baseline_dir = resolve_baseline_dir(
      context.subpath(bench_home),
      'release-c'
    ),
    tag = 'release-c'
  )
  assert local.benchmark('x.bnch.cgout') is not None
  assert local.baseline_dir == context.subpath(baseline)
  # A tag can't lead out of the bench home.
  with pytest.raises(IllegalFileAccessError):
    resolve_baseline_dir(context.subpath(bench_home), '@../../../downloads')


async def test_test_run_outside_home(tmp_path: Path, secret: Path) -> None:
  home, _ = _test_tree(tmp_path, secret)
  # A run kept elsewhere (edgy-review --root).
  run = tmp_path / 'downloads' / 'run'
  (run / 'edg_x86_64').mkdir(parents = True)
  (run / 'edg_x86_64' / 'changes.elist').write_text('suite/a.sft.cpp\n')
  with pytest.raises(IllegalFileAccessError):
    SandboxContext(home).subpath(run)
  context = SandboxContext(home, run)
  handler = test_provider.TestSourceHandler(
    'Test',
    'test',
    context,
    context.subpath(home),
    context.subpath(run)
  )
  assert await _show_source(handler, 'suite/a.sft.cpp') == 'int a;\n'


def _rt_with_links(path: Path, *, link_metadata: bool) -> None:
  '''A run artifact whose command output is a symlink, the matches a hard
  link, and (optionally) metadata.json a link too.
  '''
  metadata = json.dumps({
    'cases': [{'commands': [{'command': 'cc'}], 'matches': {}}]
  }).encode()
  with tarfile.open(path, 'w:gz') as archive:
    member = tarfile.TarInfo('real.json')
    member.size = len(metadata)
    archive.addfile(member, io.BytesIO(metadata))
    for name, link_type, target in [
      ('metadata.json', tarfile.LNKTYPE if link_metadata else None,
       'real.json'),
      ('cases/1/1/output.txt', tarfile.SYMTYPE, '../../../real.json'),
      ('cases/1/matches/output.txt', tarfile.LNKTYPE, 'real.json'),
    ]:
      member = tarfile.TarInfo(name)
      if link_type is None:
        member.size = len(metadata)
        archive.addfile(member, io.BytesIO(metadata))
        continue
      member.type = link_type
      member.linkname = target
      archive.addfile(member)


def test_run_output_refuses_link_members(tmp_path: Path) -> None:
  rt = tmp_path / 'a.rt.tar.gz'
  _rt_with_links(rt, link_metadata = False)
  with RunTestOutput(rt) as output:
    assert output.metadata['cases'][0]['commands'] == [{'command': 'cc'}]
    with pytest.raises(RunTestOutputLinkError):
      output.command_output(1, 1)
    with pytest.raises(RunTestOutputLinkError):
      output.extract_command_output(1, 1, tmp_path / 'out')
    with pytest.raises(RunTestOutputLinkError):
      output.extract_case_matches(1, tmp_path / 'matches')
    with pytest.raises(RunTestOutputLinkError):
      output.combined_raw_output()
    with pytest.raises(FileNotFoundError):
      output.command_output(1, 2)

  _rt_with_links(rt, link_metadata = True)
  with RunTestOutput(rt) as output:
    with pytest.raises(RunTestOutputLinkError):
      output.metadata
