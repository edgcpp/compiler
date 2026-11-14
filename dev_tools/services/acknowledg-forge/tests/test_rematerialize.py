# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The real edg-rematerialize-test-run (the other tests use a stub) produces
the run the service serves as the "Test - Changes" section.
'''

import os
import subprocess
import sys

from pathlib import Path, PurePosixPath
from typing import Any

import pytest

import edgtest
import edgutil

# (Not 'from ... import TestSourceHandler', which pytest would collect.)
import edgacknowledg.client.test as test_provider

from edgacknowledg import RequestTags, message
from edgacknowledg.client.shared import SandboxContext

from test_symlinks import FakeChannel

DEV_TOOLS = Path(__file__).resolve().parents[3]
REMATERIALIZE = DEV_TOOLS / 'bin' / 'edg-rematerialize-test-run'


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
  ).stdout


def write_files(root: Path, files: dict[str, str]) -> None:
  for name, content in files.items():
    (root / name).parent.mkdir(parents = True, exist_ok = True)
    (root / name).write_text(content)


def test_rematerialize_writes_changes_elist(tmp_path: Path) -> None:
  repo = tmp_path / 'repo'
  repo.mkdir()
  git(repo, 'init', '--quiet', '--initial-branch=main')
  write_files(repo, {
    'tests/.edgy/config.json': '{}\n',
    'tests/expectations/edg_x86_64/suite.log': (
      'b.sft.cpp:cp:-DTEST_NUMBER=1:FAIL:\n'
    ),
    'tests/expectations/edg_x86_64_cp/suite.log': '',
    'tests/tests/suite/a.sft.cpp': '//type:cp\nint a;\n',
    'tests/tests/suite/.a.rto/default.1.1.txt': (
      'cpfe -DTEST_NUMBER=1 Test_name.c\nold output\n'
    ),
    'tests/tests/suite/b.sft.cpp': '//type:cp\nint b;\n',
    'tests/tests/suite/d.sft.cpp': '//type:cp\nint d;\n',
    'tests/tests/suite/.d.rto/default.1.1.txt': (
      'cpfe -DTEST_NUMBER=1 Test_name.c\nd output\n'
    ),
    'tests/tests/suite/unchanged.sft.cpp': '//type:cp\nint u;\n',
  })
  git(repo, 'add', '-A')
  git(repo, 'commit', '--quiet', '-m', 'base')

  write_files(repo, {
    # (a) An existing test's recording changes.
    'tests/tests/suite/.a.rto/default.1.1.txt': (
      'cpfe -DTEST_NUMBER=1 Test_name.c\nnew output\n'
    ),
    # (b) A new test, with its recordings.
    'tests/tests/suite/c.sft.cpp': '//type:cp\nint c;\n',
    'tests/tests/suite/.c.rto/default.1.1.txt': (
      'cpfe -DTEST_NUMBER=1 Test_name.c\nc output\n'
    ),
    # A new test with no output (its recording is just the command line).
    'tests/tests/suite/e.sft.cpp': '//type:cp\nint e;\n',
    'tests/tests/suite/.e.rto/default.1.1.txt': (
      'cpfe -DTEST_NUMBER=1 Test_name.c\n'
    ),
    # (c) An expectation changes (b now passes).
    'tests/expectations/edg_x86_64/suite.log': '',
    # A configuration specific recording is added.
    'tests/tests/suite/.d.rto/edg_x86_64_cp.1.1.txt': (
      'cpfe -DTEST_NUMBER=1 Test_name.c\nd cp output\n'
    ),
  })
  git(repo, 'add', '-A')
  git(repo, 'commit', '--quiet', '-m', 'change')
  diff_file = tmp_path / 'pr.diff'
  diff_file.write_text(git(
    repo, 'diff', '--no-renames', '--no-color', 'HEAD~1', 'HEAD', '--', 'tests'
  ))
  # The tool runs against the base, as the service does.
  git(repo, 'checkout', '--quiet', 'HEAD~1')

  runs_dir = tmp_path / 'runs'
  timestamp = '2026.01.02-03.04.05'
  env = dict(os.environ)
  env['EDG_TEST_HOME'] = str(repo / 'tests')
  env['PYTHONPATH'] = str(DEV_TOOLS / 'pylibs')
  env.pop('RUN_TEST_DEFAULT_CONFIG', None)
  result = subprocess.run(
    [sys.executable, str(REMATERIALIZE),
     '--runs-dir', str(runs_dir),
     '--run-timestamp', timestamp,
     str(diff_file)],
    cwd = repo,
    env = env,
    capture_output = True,
    text = True
  )
  assert result.returncode == 0, result.stdout + result.stderr

  run_dir = runs_dir / timestamp
  changes_files = edgtest.find_changes_files(run_dir)
  assert sorted(changes_files) == ['edg_x86_64', 'edg_x86_64_cp']
  assert changes_files['edg_x86_64'] == (
    run_dir / 'edg_x86_64' / 'changes.elist'
  )
  assert changes_files['edg_x86_64'].read_text() == (
    'suite/a.sft.cpp\n'
    'suite/b.sft.cpp\n'
    'suite/c.sft.cpp\n'
    'suite/e.sft.cpp\n'
  )
  # The configuration inherits the default recordings and expectations, so
  # their changes are its changes too (unchanged.sft.cpp is in neither).
  assert changes_files['edg_x86_64_cp'].read_text() == (
    'suite/a.sft.cpp\n'
    'suite/b.sft.cpp\n'
    'suite/c.sft.cpp\n'
    'suite/d.sft.cpp\n'
    'suite/e.sft.cpp\n'
  )

  # New tests are reported as edgy reports a test without recordings: no
  # previous output (not mismatched output), diffed against nothing.
  elog = (run_dir / 'edg_x86_64' / 'changes.elog').read_text().splitlines()
  assert '+ suite/a.sft.cpp cp:-DTEST_NUMBER=1:PASS--OUTPUT MISMATCH:' in elog
  assert (
    '+ suite/c.sft.cpp cp:-DTEST_NUMBER=1:PASS--NO PREVIOUS OUTPUT:' in elog
  )
  assert (
    '+ suite/e.sft.cpp cp:-DTEST_NUMBER=1:PASS--NO PREVIOUS OUTPUT:' in elog
  )

  def static_diff(test: str) -> str:
    rt_path = (run_dir / 'edg_x86_64' / test).with_suffix(
      edgtest.runtest.RUN_TEST_OUTPUT_SUFFIX
    )
    with edgtest.runtest.RunTestOutput(rt_path) as rt_output:
      text = ''.join(rt_output.combined_static_diff())
      # As pieces (for display), the same, header lines fitting in one.
      for limit in (len('--- Expected Output\n'), 1000):
        assert ''.join(
          piece for piece, _ in rt_output.iter_static_diff_pieces(limit)
        ) == text
        assert ''.join(
          piece for piece, _ in rt_output.iter_raw_output_pieces(limit)
        ) == ''.join(rt_output.combined_raw_output())
      return text

  c_diff = static_diff('suite/c.sft.cpp')
  assert '# cpfe -DTEST_NUMBER=1 Test_name.c\n' in c_diff
  assert '+c output\n' in c_diff
  # As with edgy, empty output against no recording is no difference.
  assert static_diff('suite/e.sft.cpp') == ''

  # edgy-review finds each listed test's .rt.tar from the changes files.
  found = sorted(
    path.relative_to(run_dir).as_posix()
    for path in edgtest.walk_rt_file_directory(
      run_dir,
      diff_algorithm = edgtest.DiffDiscMode.EDGY_CHANGES,
      do_replays_dir_detection = True
    )
  )
  assert found == sorted(
    (PurePosixPath(config) / test).with_suffix(
      edgtest.runtest.RUN_TEST_OUTPUT_SUFFIX
    ).as_posix()
    for config, changes_file in changes_files.items()
    for test in changes_file.read_text().split()
  )


def _rematerialize_change(tmp_path: Path,
                          change: dict[str, str]) -> tuple[
                            subprocess.CompletedProcess[str], Path]:
  '''Run the tool on the diff ``change`` makes to a small test home; return
  its result and the run's directory.
  '''
  repo = tmp_path / 'repo'
  repo.mkdir()
  git(repo, 'init', '--quiet', '--initial-branch=main')
  write_files(repo, {
    'tests/.edgy/config.json': '{}\n',
    'tests/expectations/edg_x86_64/suite.log': '',
    'tests/expectations/edg_x86_64_cp/suite.log': '',
    'tests/tests/suite/a.sft.cpp': '//type:cp\nint a;\n',
    'tests/tests/suite/.a.rto/default.1.1.txt': (
      'cpfe -DTEST_NUMBER=1 Test_name.c\nold output\n'
    ),
  })
  git(repo, 'add', '-A')
  git(repo, 'commit', '--quiet', '-m', 'base')
  write_files(repo, change)
  git(repo, 'add', '-A')
  git(repo, 'commit', '--quiet', '-m', 'change')
  diff_file = tmp_path / 'pr.diff'
  diff_file.write_text(git(
    repo, 'diff', '--no-renames', '--no-color', 'HEAD~1', 'HEAD', '--', 'tests'
  ))
  git(repo, 'checkout', '--quiet', 'HEAD~1')

  runs_dir = tmp_path / 'runs'
  timestamp = '2026.01.02-03.04.05'
  env = dict(os.environ)
  env['EDG_TEST_HOME'] = str(repo / 'tests')
  env['PYTHONPATH'] = str(DEV_TOOLS / 'pylibs')
  env.pop('RUN_TEST_DEFAULT_CONFIG', None)
  result = subprocess.run(
    [sys.executable, str(REMATERIALIZE),
     '--runs-dir', str(runs_dir),
     '--run-timestamp', timestamp,
     str(diff_file)],
    cwd = repo,
    env = env,
    capture_output = True,
    text = True
  )
  return result, runs_dir / timestamp


def test_rematerialize_passes_huge_lines_through(tmp_path: Path) -> None:
  # Far longer than a line is ever held whole; written (and patched) in
  # full.
  huge = 'h' * (5 * edgutil.MAX_LINE_CHARS)
  result, run_dir = _rematerialize_change(tmp_path, {
    'tests/tests/suite/.a.rto/default.1.1.txt': (
      f"cpfe -DTEST_NUMBER=1 Test_name.c\n{huge}\n"
    ),
  })
  assert result.returncode == 0, result.stdout + result.stderr
  rt_path = (run_dir / 'edg_x86_64' / 'suite/a.sft.cpp').with_suffix(
    edgtest.runtest.RUN_TEST_OUTPUT_SUFFIX
  )
  with edgtest.runtest.RunTestOutput(rt_path) as rt_output:
    static_diff = ''.join(rt_output.combined_static_diff())
  assert f"+{huge}\n" in static_diff


def test_rematerialize_refuses_huge_expectation_lines(tmp_path: Path) -> None:
  result, _ = _rematerialize_change(tmp_path, {
    'tests/expectations/edg_x86_64/suite.log': (
      'a.sft.cpp:cp:-DTEST_NUMBER=1:FAIL:' +
      'r' * edgutil.MAX_LINE_CHARS + '\n'
    ),
  })
  assert result.returncode == 1
  assert (
    'tests/expectations/edg_x86_64/suite.log: a changed line is longer than'
  ) in result.stderr


async def test_curr_diff_is_shown_and_compared(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
  # Emulate a non-UTF-8 locale for text reads without an explicit encoding.
  def locale_open(file: Any, mode: str = 'r', **kwargs: Any) -> Any:
    if 'b' not in mode:
      kwargs.setdefault('encoding', 'ascii')
    return open(file, mode, **kwargs)

  monkeypatch.setattr(test_provider, 'open', locale_open, raising = False)
  result, run_dir = _rematerialize_change(tmp_path, {
    'tests/tests/suite/.a.rto/default.1.1.txt': (
      'cpfe -DTEST_NUMBER=1 Test_name.c\nnew output\n'
    ),
    # Brings the other configuration into the run (which inherits the
    # recording's change).
    'tests/expectations/edg_x86_64_cp/suite.log': (
      'a.sft.cpp:cp:-DTEST_NUMBER=1:FAIL:\n'
    ),
  })
  assert result.returncode == 0, result.stdout + result.stderr
  context = SandboxContext(tmp_path)
  handler = test_provider.TestSourceHandler(
    'Test',
    'test',
    context,
    context.subpath(tmp_path / 'repo' / 'tests'),
    context.subpath(run_dir)
  )

  async def show_curr_diff(
      configs: tuple[str, ...] = ('edg_x86_64', 'edg_x86_64_cp')
      ) -> dict[str, Any]:
    channel = FakeChannel(message(
      RequestTags.TEST_SHOW_CURR_DIFF,
      value = {
        'id': '(edg_x86_64) suite/a.sft.cpp',
        'configs': list(configs),
        'reviewer_id': 'r',
      }
    ))
    await handler.handle_request(channel)  # type: ignore[arg-type]
    [reply] = channel.sent
    response: dict[str, Any] = reply['v']['client_response']
    return response

  # Observed against the run's (the pull request's) recordings, which it
  # was made from: no difference.
  response = await show_curr_diff()
  assert response['content'] == ''
  assert 'divergent_configs' not in response

  # Recordings changed since: the difference is shown, the same for both
  # configurations (the other uses the default recording).
  recordings = run_dir / 'patched-tests' / 'suite' / '.a.rto'
  (recordings / 'default.1.1.txt').write_text(
    'cpfe -DTEST_NUMBER=1 Test_name.c\nnewer café output\n',
    encoding = 'utf-8'
  )
  response = await show_curr_diff()
  assert response['content'] == (
    '--- Expected Output\n'
    '+++ Observed Output\n'
    '# cpfe -DTEST_NUMBER=1 Test_name.c\n'
    '@@ -1 +1 @@\n'
    '-newer café output\n'
    '+new output\n'
  )
  assert 'divergent_configs' not in response

  # A recording of its own makes the other configuration's differ.
  (recordings / 'edg_x86_64_cp.1.1.txt').write_text(
    'cpfe -DTEST_NUMBER=1 Test_name.c\nnew output\n'
  )
  response = await show_curr_diff()
  assert '-newer café output\n+new output' in response['content']
  assert response['divergent_configs'] == ['edg_x86_64_cp']

  compared: list[Path] = []
  original_cmp = test_provider.filecmp.cmp

  def cmp(first: Path, second: Path, *, shallow: bool) -> bool:
    compared.append(second)
    return original_cmp(first, second, shallow = shallow)

  monkeypatch.setattr(test_provider.filecmp, 'cmp', cmp)
  await show_curr_diff(
    ('edg_x86_64', 'edg_x86_64_cp', 'edg_x86_64_cp')
  )
  assert len(compared) == 2
  assert compared[0] != compared[1]
