# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Handling of untrusted input: archives, paths, and the sandbox.'''

import io
import os
import stat
import tarfile
import zipfile

from pathlib import Path, PurePosixPath

import pytest

from edgacknowledg.forge.sandbox import (
  RunnerClient,
  Sandbox,
  SandboxUnavailableError,
)
from edgacknowledg.forge.workspace import (
  UnsafeArchiveError,
  WorkspaceError,
  changes_patterns,
  ensure_real_directory,
  extract_artifact,
  failures_patterns,
  patterns_for_test,
)

def _zip(path: Path, entries: dict[str, bytes]) -> Path:
  with zipfile.ZipFile(path, 'w') as archive:
    for name, data in entries.items():
      archive.writestr(name, data)
  return path


@pytest.mark.parametrize('name', [
  '../escape.txt',
  '/absolute.txt',
  'a/../../escape.txt',
  'a\\..\\escape.txt',
])
def test_extract_rejects_escaping_paths(tmp_path: Path, name: str) -> None:
  archive = _zip(tmp_path / 'a.zip', {name: b'x'})
  destination = tmp_path / 'out'
  destination.mkdir()
  with pytest.raises(UnsafeArchiveError):
    extract_artifact(archive, destination, max_bytes = 1024)
  assert not (tmp_path / 'escape.txt').exists()


def test_extract_rejects_symlinks(tmp_path: Path) -> None:
  path = tmp_path / 'a.zip'
  with zipfile.ZipFile(path, 'w') as archive:
    info = zipfile.ZipInfo('link')
    info.external_attr = (stat.S_IFLNK | 0o777) << 16
    archive.writestr(info, '/etc/passwd')
  destination = tmp_path / 'out'
  destination.mkdir()
  with pytest.raises(UnsafeArchiveError):
    extract_artifact(path, destination, max_bytes = 1024)


def test_extract_caps_total_size(tmp_path: Path) -> None:
  # Highly compressible, so the zip itself is tiny.
  archive = _zip(tmp_path / 'a.zip', {'big.txt': b'0' * 100_000})
  destination = tmp_path / 'out'
  destination.mkdir()
  with pytest.raises(UnsafeArchiveError):
    extract_artifact(archive, destination, max_bytes = 10_000)


def test_extract_caps_inner_tarballs(tmp_path: Path) -> None:
  payload = io.BytesIO()
  with tarfile.open(fileobj = payload, mode = 'w:gz') as tarball:
    data = b'0' * 100_000
    info = tarfile.TarInfo('output.txt')
    info.size = len(data)
    tarball.addfile(info, io.BytesIO(data))
  archive = _zip(tmp_path / 'a.zip', {'cfg/t.rt.tar.gz': payload.getvalue()})
  destination = tmp_path / 'out'
  destination.mkdir()
  with pytest.raises(UnsafeArchiveError):
    extract_artifact(archive, destination, max_bytes = 10_000)


def test_extract_valid_artifact(tmp_path: Path) -> None:
  archive = _zip(tmp_path / 'a.zip', {
    'ci-meta.json': b'{}',
    'cfg/changes.elist': b'suite/a.sft.cpp\n',
  })
  destination = tmp_path / 'out'
  destination.mkdir()
  extract_artifact(archive, destination, max_bytes = 10_000)
  assert (destination / 'cfg' / 'changes.elist').read_bytes() == (
    b'suite/a.sft.cpp\n'
  )


def test_ensure_real_directory_refuses_symlinks(tmp_path: Path) -> None:
  outside = tmp_path / 'outside'
  outside.mkdir()
  root = tmp_path / 'worktree'
  (root / 'tests').mkdir(parents = True)
  # What a pull request could commit in place of tests/runs.
  os.symlink(outside, root / 'tests' / 'runs')
  with pytest.raises(WorkspaceError):
    ensure_real_directory(root, PurePosixPath('tests', 'runs', 'ci-1'))
  assert list(outside.iterdir()) == []


def test_test_patterns() -> None:
  assert patterns_for_test(PurePosixPath('tests/tests/s/d/a.sft.cpp')) == [
    '/tests/tests/s/d/a.sft.cpp',
    '/tests/tests/s/d/.a.sft.rto/',
    '/tests/tests/s/d/.a.rto/',
  ]
  # Multi-file tests own their directory.
  mft_test = PurePosixPath('tests/tests/cwg/x/test.mft.cpp')
  assert patterns_for_test(mft_test) == ['/tests/tests/cwg/x/']
  # Pattern characters in names are escaped.
  assert patterns_for_test(PurePosixPath('tests/tests/s/[x].sft.c'))[0] == (
    '/tests/tests/s/\\[x\\].sft.c'
  )


def test_changes_patterns_cover_recordings_and_expectations() -> None:
  diff = '\n'.join([
    'diff --git a/tests/tests/s/.a.rto/default.1.1.txt '
    'b/tests/tests/s/.a.rto/default.1.1.txt',
    '--- a/tests/tests/s/.a.rto/default.1.1.txt',
    '+++ b/tests/tests/s/.a.rto/default.1.1.txt',
    '-old',
    '+new',
    'diff --git a/tests/expectations/edg_x86_64/other.log '
    'b/tests/expectations/edg_x86_64/other.log',
    '--- a/tests/expectations/edg_x86_64/other.log',
    '+++ b/tests/expectations/edg_x86_64/other.log',
    '-dir/b.sft.cpp: PASS',
    '+dir/b.sft.cpp: FAIL',
  ])
  patterns = changes_patterns(diff)
  assert '/tests/tests/s/.a.rto/' in patterns
  assert '/tests/tests/s/a.*' in patterns
  assert '/tests/tests/other/dir/b.sft.cpp' in patterns
  # Tooling comes from the frozen tools directory, never the checkout.
  assert not any(p.startswith('/dev_tools/') for p in patterns)


def test_failures_patterns_ignore_escaping_ids() -> None:
  patterns = failures_patterns(['s/a.sft.cpp', '../../etc/passwd'])
  assert '/tests/tests/s/a.sft.cpp' in patterns
  assert not any('etc' in pattern for pattern in patterns)


async def test_sandbox_requires_its_runners(tmp_path: Path,
                                            unreachable_url: str) -> None:
  # A sandbox runs nothing itself: without a runner, it refuses.
  sandbox = Sandbox()
  with pytest.raises(SandboxUnavailableError, match = 'no runner'):
    await sandbox.check()
  with pytest.raises(SandboxUnavailableError, match = 'tools runner'):
    sandbox.context_for(tmp_path)
  with pytest.raises(SandboxUnavailableError, match = 'builder runner'):
    await sandbox.run(['true'], timeout = 5)

  # One that doesn't answer fails the check.
  unreachable = Sandbox(builder = RunnerClient(unreachable_url))
  with pytest.raises(SandboxUnavailableError, match = "didn't answer"):
    await unreachable.check()
