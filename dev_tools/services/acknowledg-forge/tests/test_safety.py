# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Handling of untrusted input: archives, paths, and the sandbox.'''

import asyncio
import io
import os
import signal
import stat
import tarfile
import zipfile

from pathlib import Path, PurePosixPath

import pytest

import edgtest

from edgacknowledg.client.test import _run_test_output
from edgacknowledg.forge import processor
from edgacknowledg.forge.processor import _changes_test_ids, _read_ci_metadata
from edgacknowledg.forge.sandbox import Sandbox, SandboxUnavailableError
from edgacknowledg.forge.workspace import (
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


def test_extract_counts_inner_tarballs_compressed_size(tmp_path: Path) -> None:
  payload = io.BytesIO()
  with tarfile.open(fileobj = payload, mode = 'w:gz') as tarball:
    data = b'0' * 100_000
    info = tarfile.TarInfo('output.txt')
    info.size = len(data)
    tarball.addfile(info, io.BytesIO(data))
  archive = _zip(tmp_path / 'a.zip', {'cfg/t.rt.tar.gz': payload.getvalue()})
  destination = tmp_path / 'out'
  destination.mkdir()
  compressed_size = len(payload.getvalue())
  extract_artifact(archive, destination, max_bytes = compressed_size)
  assert (destination / 'cfg' / 't.rt.tar.gz').read_bytes() == payload.getvalue()

  smaller = tmp_path / 'smaller'
  smaller.mkdir()
  with pytest.raises(UnsafeArchiveError,
                     match = 'artifact exceeds the extracted size limit'):
    extract_artifact(archive, smaller, max_bytes = compressed_size - 1)


def test_run_output_checks_inner_members_on_read(tmp_path: Path) -> None:
  path = tmp_path / 'a.rt.tar.gz'
  with tarfile.open(path, 'w:gz') as archive:
    archive.addfile(tarfile.TarInfo('a.txt'), io.BytesIO())
    archive.addfile(tarfile.TarInfo('b.txt'), io.BytesIO())
  with _run_test_output(path, None, None) as output:
    assert output.member_names == {'a.txt', 'b.txt'}
  with pytest.raises(ValueError, match = 'too many entries'):
    with _run_test_output(path, 1, None):
      pass
  with pytest.raises(ValueError, match = 'too many entries'):
    with edgtest.runtest.RunTestOutput(path, max_entries = 1):
      pass

  with tarfile.open(path, 'w:gz') as archive:
    member = tarfile.TarInfo('a.txt')
    member.size = 100
    archive.addfile(member, io.BytesIO(b'x' * 100))
  with pytest.raises(ValueError, match = 'too large'):
    with edgtest.runtest.RunTestOutput(path, max_member_bytes = 10):
      pass

  with tarfile.open(path, 'w:gz') as archive:
    member = tarfile.TarInfo('a.txt')
    member.type = tarfile.SYMTYPE
    member.linkname = 'elsewhere'
    archive.addfile(member)
  with pytest.raises(edgtest.runtest.RunTestOutputLinkError):
    with edgtest.runtest.RunTestOutput(path, max_entries = 2):
      pass


def test_extract_caps_entries(tmp_path: Path) -> None:
  destination = tmp_path / 'out'
  destination.mkdir()
  archive = _zip(tmp_path / 'a.zip', {f"{n}.txt": b'' for n in range(4)})
  with pytest.raises(UnsafeArchiveError, match = 'more than 3 entries'):
    extract_artifact(
      archive,
      destination,
      max_bytes = 10_000,
      max_entries = 3
    )

  # Inner tarballs' entries are checked when opened for review.
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
  extract_artifact(
    archive,
    destination,
    max_bytes = 10_000,
    max_entries = 3
  )


def test_extract_caps_the_central_directory(tmp_path: Path) -> None:
  # zipfile would read it all before the entries could be counted.
  archive = _zip(tmp_path / 'a.zip', {f"{n}.txt": b'' for n in range(10)})
  size = _central_directory_size(archive)
  assert size is not None
  destination = tmp_path / 'out'
  destination.mkdir()
  with pytest.raises(UnsafeArchiveError, match = 'central directory'):
    extract_artifact(
      archive,
      destination,
      max_bytes = 10_000,
      max_directory_bytes = size - 1
    )
  assert list(destination.iterdir()) == []
  extract_artifact(
    archive,
    destination,
    max_bytes = 10_000,
    max_directory_bytes = size
  )


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
  # Starting as an end record does, but too short to be one.
  for size in (15, 17, 21):
    short = tmp_path / f"short-{size}.zip"
    short.write_bytes(b'PK\x05\x06' + b'\0' * (size - 4))
    assert _central_directory_size(short) is None


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
  # Recordings are where the test runner keeps them: named for the part of
  # the test's name before its first dot.
  assert patterns_for_test(PurePosixPath('tests/tests/s/d/a.sft.cpp')) == [
    '/tests/tests/s/d/a.sft.cpp',
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


def test_test_patterns_are_linear_in_the_path() -> None:
  # Two, however many dots its name has.
  name = '.'.join(['x'] * 2100) + '.sft.cpp'
  assert patterns_for_test(PurePosixPath('tests/tests/s', name)) == [
    f"/tests/tests/s/{name}",
    '/tests/tests/s/.x.rto/',
  ]


def test_changes_patterns_skip_suites_no_suite_may_be_named() -> None:
  # A suite is in the patterns of every test its expectation file names.
  def expectations(suite: str) -> str:
    log = f"tests/expectations/edg_x86_64/{suite}.log"
    return '\n'.join([
      f"diff --git a/{log} b/{log}",
      f"--- a/{log}",
      f"+++ b/{log}",
      '+a.sft.cpp: PASS',
    ])

  patterns = changes_patterns(expectations('cwg'))
  assert '/tests/tests/cwg/a.sft.cpp' in patterns
  for suite in ['s' * 65, 'a.b', 'a b']:
    assert changes_patterns(expectations(suite)) == [
      '/tests/.edgy/', '/tests/expectations/',
    ]


def test_patterns_are_capped() -> None:
  cap = 100
  # Repeats aren't counted.
  failures_patterns(['s/a.sft.cpp'] * (cap + 1), max_patterns = cap)
  with pytest.raises(WorkspaceError, match = 'sparse-checkout patterns'):
    failures_patterns(
      [f"s/{n}.sft.cpp" for n in range(cap)],
      max_patterns = cap
    )
  diff = '\n'.join([
    'diff --git a/tests/expectations/x/s.log b/tests/expectations/x/s.log',
    *(f"+{n}.sft.cpp: FAIL" for n in range(cap)),
  ])
  with pytest.raises(WorkspaceError, match = 'sparse-checkout patterns'):
    changes_patterns(diff, max_patterns = cap)
  # Within a higher cap.
  changes_patterns(diff, max_patterns = 3 * cap)


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
  # One that would never stop is stopped (letting it clean up).
  process = await asyncio.create_subprocess_exec(
    'yes',
    stdout = asyncio.subprocess.PIPE,
    stderr = asyncio.subprocess.PIPE
  )
  assert await _communicate_bounded(process, 1024 * 1024) is None
  assert process.returncode == -signal.SIGTERM


async def test_cancelled_git_output_reads_stop_git() -> None:
  # As when the service stops while git still has output to write: left
  # writing to a pipe no one reads, it would never exit.
  process = await asyncio.create_subprocess_exec(
    'yes',
    stdout = asyncio.subprocess.PIPE,
    stderr = asyncio.subprocess.PIPE
  )
  reading = asyncio.create_task(_communicate_bounded(process, 1 << 40))
  await asyncio.sleep(0.2)
  reading.cancel()
  await asyncio.wait_for(asyncio.gather(reading, return_exceptions = True), 10)
  assert process.returncode == -signal.SIGTERM


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
  # Within the limit as read, not as JSON (six bytes for each U+FFFD).
  garbled = tmp_path / 'c.elist'
  garbled.write_bytes(b'\xff' * 5)
  with pytest.raises(WorkspaceError, match = 'over 20 bytes as JSON'):
    _changes_test_ids([garbled])

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


async def test_sandbox_requires_its_runners(tmp_path: Path) -> None:
  # A sandbox runs nothing itself: without a runner, it refuses.
  sandbox = Sandbox()
  with pytest.raises(SandboxUnavailableError, match = 'no runner'):
    await sandbox.check()
  with pytest.raises(SandboxUnavailableError, match = 'tools runner'):
    sandbox.context_for(tmp_path)
  with pytest.raises(SandboxUnavailableError, match = 'builder runner'):
    await sandbox.run(['true'], timeout = 5)
