# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Handling of untrusted input: archives, paths, and sandboxed commands.'''

import io
import os
import shutil
import stat
import tarfile
import zipfile

from pathlib import Path, PurePosixPath

import pytest

from edgacknowledg.forge.sandbox import Sandbox
from edgacknowledg.forge.workspace import (
  UnsafeArchiveError,
  WorkspaceError,
  changes_patterns,
  ensure_real_directory,
  extract_artifact,
  failures_patterns,
  patterns_for_test,
)

needs_bwrap = pytest.mark.skipif(
  shutil.which('bwrap') is None,
  reason = 'bubblewrap is not installed'
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


@needs_bwrap
async def test_sandbox_confines_writes_and_network(tmp_path: Path) -> None:
  readable = tmp_path / 'readable'
  readable.mkdir()
  (readable / 'input.txt').write_text('data\n')
  writable = tmp_path / 'writable'
  writable.mkdir()
  secret = tmp_path / 'secret.txt'
  secret.write_text('token\n')

  sandbox = Sandbox(enabled = True)
  await sandbox.check()
  script = (
    f"cat {readable}/input.txt > {writable}/copy.txt; "
    f"echo x > {readable}/tampered.txt 2>/dev/null && echo WROTE-RO; "
    f"cat {secret} 2>/dev/null && echo READ-SECRET; "
    f"echo \"env=$EDG_SECRET\""
  )
  os.environ['EDG_SECRET'] = 'leaked'
  try:
    result = await sandbox.run(
      ['sh', '-c', script],
      read_only = [readable],
      writable = [writable],
      timeout = 30
    )
  finally:
    del os.environ['EDG_SECRET']
  assert result.succeeded, result.output
  assert (writable / 'copy.txt').read_text() == 'data\n'
  assert 'WROTE-RO' not in result.output
  assert 'READ-SECRET' not in result.output
  assert 'env=\n' in result.output


async def test_sandbox_timeout_kills(tmp_path: Path) -> None:
  sandbox = Sandbox(enabled = shutil.which('bwrap') is not None)
  result = await sandbox.run(['sleep', '30'], timeout = 0.5)
  assert result.timed_out and not result.succeeded


@needs_bwrap
async def test_sandbox_provides_frozen_tools(tmp_path: Path) -> None:
  tools = tmp_path / 'tools'
  (tools / 'bin').mkdir(parents = True)
  (tools / 'pylibs' / 'frozenlib').mkdir(parents = True)
  (tools / 'pylibs' / 'frozenlib' / '__init__.py').write_text("NAME = 'ok'\n")
  script = tools / 'bin' / 'frozen-tool'
  script.write_text(
    '#!/usr/bin/env python3\nimport frozenlib\nprint(frozenlib.NAME)\n'
  )
  script.chmod(0o755)
  sandbox = Sandbox(enabled = True, tools_dir = tools)
  # Found on PATH, importing from the frozen pylibs, without binding the
  # tools explicitly.
  result = await sandbox.run(['frozen-tool'], timeout = 30)
  assert result.succeeded and result.output == 'ok\n', result.output
  # ...and read-only.
  result = await sandbox.run(['touch', str(script)], timeout = 30)
  assert not result.succeeded
