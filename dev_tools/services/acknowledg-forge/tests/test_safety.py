# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Handling of untrusted input: archives, paths, and the sandbox.'''

import asyncio
import io
import os
import stat
import tarfile
import zipfile

from pathlib import Path, PurePosixPath

import pytest

from edgacknowledg.forge import processor, workspace
from edgacknowledg.forge.processor import _changes_test_ids, _read_ci_metadata
from edgacknowledg.forge.sandbox import (
  RunnerClient,
  Sandbox,
  SandboxUnavailableError,
)
from edgacknowledg.forge.workspace import (
  MAX_PATTERNS,
  UnsafeArchiveError,
  WorkspaceError,
  _central_directory_size,
  _communicate_bounded,
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


def test_extract_caps_entries(tmp_path: Path,
                              monkeypatch: pytest.MonkeyPatch) -> None:
  monkeypatch.setattr(workspace, 'MAX_ARTIFACT_ENTRIES', 3)
  destination = tmp_path / 'out'
  destination.mkdir()
  archive = _zip(tmp_path / 'a.zip', {f"{n}.txt": b'' for n in range(4)})
  with pytest.raises(UnsafeArchiveError, match = 'more than 3 entries'):
    extract_artifact(archive, destination, max_bytes = 10_000)

  # Inner tarballs' entries count too.
  payload = io.BytesIO()
  with tarfile.open(fileobj = payload, mode = 'w:gz') as tarball:
    for n in range(2):
      tarball.addfile(tarfile.TarInfo(f"{n}.txt"), io.BytesIO())
  archive = _zip(tmp_path / 'b.zip', {
    'a.txt': b'',
    't.rt.tar.gz': payload.getvalue(),
  })
  destination = tmp_path / 'out2'
  destination.mkdir()
  with pytest.raises(UnsafeArchiveError, match = 'more than 3 entries'):
    extract_artifact(archive, destination, max_bytes = 10_000)


def test_extract_caps_the_central_directory(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch) -> None:
  # zipfile would read it all before the entries could be counted.
  archive = _zip(tmp_path / 'a.zip', {f"{n}.txt": b'' for n in range(10)})
  size = _central_directory_size(archive)
  assert size is not None
  monkeypatch.setattr(workspace, 'MAX_CENTRAL_DIRECTORY_BYTES', size - 1)
  destination = tmp_path / 'out'
  destination.mkdir()
  with pytest.raises(UnsafeArchiveError, match = 'central directory'):
    extract_artifact(archive, destination, max_bytes = 10_000)
  assert list(destination.iterdir()) == []


def _zipfile_directory_size(path: Path) -> int:
  with open(path, 'rb') as handle:
    end_record = zipfile._EndRecData(handle)  # type: ignore[attr-defined]
  return int(end_record[zipfile._ECD_SIZE])  # type: ignore[attr-defined]


def test_central_directory_size_matches_zipfiles(tmp_path: Path) -> None:
  plain = _zip(tmp_path / 'plain.zip', {'a.txt': b'a', 'b/c.txt': b'c'})
  commented = tmp_path / 'commented.zip'
  with zipfile.ZipFile(commented, 'w') as archive:
    archive.writestr('a.txt', b'a')
    # Holding what looks like another end record.
    archive.comment = b'PK\x05\x06' + b'x' * 1000
  # Too many entries for the plain end record: a ZIP64 one.
  zip64 = _zip(tmp_path / 'zip64.zip', {f"{n}": b'' for n in range(0x10000)})
  for path in (plain, commented, zip64):
    assert _central_directory_size(path) == _zipfile_directory_size(path)
  (tmp_path / 'not.zip').write_bytes(b'not a zip')
  assert _central_directory_size(tmp_path / 'not.zip') is None


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


def test_long_test_names_need_no_patterns() -> None:
  # No file has such a name; its patterns would be quadratic in its length.
  name = '.'.join(['x'] * 200) + '.sft.cpp'
  assert patterns_for_test(PurePosixPath('tests/tests/s', name)) == []


def test_patterns_are_capped() -> None:
  # Repeats aren't counted.
  failures_patterns(['s/a.sft.cpp'] * (MAX_PATTERNS + 1))
  with pytest.raises(WorkspaceError, match = 'sparse-checkout patterns'):
    failures_patterns([f"s/{n}.sft.cpp" for n in range(MAX_PATTERNS)])
  diff = '\n'.join([
    'diff --git a/tests/expectations/x/s.log b/tests/expectations/x/s.log',
    *(f"+{n}.sft.cpp: FAIL" for n in range(MAX_PATTERNS)),
  ])
  with pytest.raises(WorkspaceError, match = 'sparse-checkout patterns'):
    changes_patterns(diff)


async def test_git_output_can_be_bounded() -> None:
  async def run(*argv: str, limit: int) -> object:
    process = await asyncio.create_subprocess_exec(
      *argv,
      stdout = asyncio.subprocess.PIPE,
      stderr = asyncio.subprocess.PIPE
    )
    result = await _communicate_bounded(process, limit)
    assert process.returncode is not None
    return result

  assert await run('sh', '-c', 'printf out; printf err >&2',
                   limit = 3) == (b'out', b'err')
  # One that would never stop is stopped.
  assert await run('yes', limit = 1024 * 1024) is None


def test_ci_reads_are_capped(tmp_path: Path,
                             monkeypatch: pytest.MonkeyPatch) -> None:
  monkeypatch.setattr(processor, 'MAX_TEST_IDS_BYTES', 20)
  first = tmp_path / 'a.elist'
  first.write_text('s/a.sft.cpp\n\n')
  second = tmp_path / 'b.elist'
  second.write_text('s/b.sft.cpp\n')
  assert _changes_test_ids([first]) == ['s/a.sft.cpp']
  # Within the limit together, not each.
  with pytest.raises(WorkspaceError, match = 'over 20 bytes'):
    _changes_test_ids([first, second])

  monkeypatch.setattr(processor, 'MAX_CI_METADATA_BYTES', 30)
  metadata = tmp_path / 'ci-meta.json'
  assert _read_ci_metadata(metadata) == {}
  metadata.write_text('{"tested_sha": "abc"}')
  assert _read_ci_metadata(metadata) == {'tested_sha': 'abc'}
  metadata.write_text('[1]')
  assert _read_ci_metadata(metadata) == {}
  metadata.write_text('{"tested_sha": "' + 'a' * 40 + '"}')
  with pytest.raises(WorkspaceError, match = 'over 30 bytes'):
    _read_ci_metadata(metadata)


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
  async with RunnerClient(unreachable_url, open_timeout = 0.5) as client:
    unreachable = Sandbox(builder = client)
    with pytest.raises(SandboxUnavailableError, match = "didn't answer"):
      await unreachable.check()
