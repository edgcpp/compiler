# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Tests for pruning a test run's .rt.tar files (edgtest.prune_run_archives)
and edg-prune-test-run.

Run with: python3 -m unittest discover -s dev_tools/tests
(with dev_tools/pylibs on PYTHONPATH).
'''

import os
import subprocess
import sys
import tempfile
import unittest

from pathlib import Path
from typing import List

import edgtest

from edgtest import runtest

DEV_TOOLS_DIR = Path(__file__).resolve().parents[1]
PRUNE_TOOL_PATH = DEV_TOOLS_DIR / 'bin' / 'edg-prune-test-run'

SUFFIX = runtest.RUN_TEST_OUTPUT_SUFFIX

def _touch(path: Path) -> Path:
  path.parent.mkdir(parents = True, exist_ok = True)
  path.write_bytes(b'')
  return path

def _files(root: Path) -> List[str]:
  return sorted(
    Path(directory, name).relative_to(root).as_posix()
    for directory, _, names in os.walk(root)
    for name in names
  )

class PruneRunArchivesTests(unittest.TestCase):

  def setUp(self) -> None:
    temp_dir = tempfile.TemporaryDirectory()
    self.addCleanup(temp_dir.cleanup)
    self.run_dir = Path(temp_dir.name) / 'run'
    # One configuration whose changes.elist names two tests of one suite
    # (and one without results), with every test's results...
    config = self.run_dir / 'edg_x86_64'
    _touch(config / 'changes.elist').write_text(
      'suite/a.sft.cpp\nsuite/dir/b.sft.c\nsuite/none.sft.cpp\n\n'
    )
    for name in ('changes.elog', 'executed.elist', 'suite-suite.elog',
                 'suite-suite.log', 'suite-other.elog', 'suite-other.log'):
      _touch(config / name)
    for name in ('suite/a.sft', 'suite/dir/b.sft', 'suite/c.sft',
                 'other/a.sft'):
      _touch(config / f"{name}{SUFFIX}")
    # ...one without a changes.elist...
    config = self.run_dir / 'edg_x86_32'
    for name in ('executed.elist', 'suite-suite.elog', 'suite-suite.log',
                 f"suite/a.sft{SUFFIX}"):
      _touch(config / name)
    # ...and the run's replays (removed whole).
    _touch(self.run_dir / 'replays' / f"suite/c.sft{SUFFIX}")

  def test_keeps_only_what_changes_elist_names(self) -> None:
    changes_file = self.run_dir / 'edg_x86_64' / 'changes.elist'
    read = list(edgtest._scan_changes_file(changes_file))

    self.assertEqual(edgtest.prune_run_archives(self.run_dir), (2, 4, 2))
    # Suites with no changes go whole, and unnamed results with them.
    self.assertEqual(_files(self.run_dir), [
      'edg_x86_32/executed.elist',
      'edg_x86_64/changes.elist',
      'edg_x86_64/changes.elog',
      'edg_x86_64/executed.elist',
      'edg_x86_64/suite-suite.elog',
      'edg_x86_64/suite-suite.log',
      'edg_x86_64/suite/a.sft' + SUFFIX,
      'edg_x86_64/suite/dir/b.sft' + SUFFIX,
    ])
    # A review reads what it read before.
    self.assertEqual(list(edgtest._scan_changes_file(changes_file)), read)

    # Pruning again changes nothing.
    self.assertEqual(edgtest.prune_run_archives(self.run_dir), (2, 0, 0))

  def test_keeps_nothing_without_changes_elist(self) -> None:
    (self.run_dir / 'edg_x86_64' / 'changes.elist').unlink()
    self.assertEqual(edgtest.prune_run_archives(self.run_dir), (0, 6, 3))
    self.assertEqual(_files(self.run_dir), [
      'edg_x86_32/executed.elist',
      'edg_x86_64/changes.elog',
      'edg_x86_64/executed.elist',
    ])

  def test_command(self) -> None:
    env = dict(os.environ)
    env['PYTHONPATH'] = str(DEV_TOOLS_DIR / 'pylibs')
    result = subprocess.run(
      [sys.executable, str(PRUNE_TOOL_PATH), str(self.run_dir)],
      capture_output = True,
      text = True,
      env = env
    )
    self.assertEqual(result.returncode, 0, result.stderr)
    self.assertIn('Kept 2', result.stdout)
    self.assertIn('removed 4 others, and 2 suites', result.stdout)

    missing = subprocess.run(
      [sys.executable, str(PRUNE_TOOL_PATH), str(self.run_dir / 'missing')],
      capture_output = True,
      text = True,
      env = env
    )
    self.assertEqual(missing.returncode, 1)

if __name__ == '__main__':
  unittest.main()
