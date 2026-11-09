# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Tests for the per-configuration test inputs (edgtest.inputs) and
edg-test-inputs-hash.

Run with: python3 -m unittest discover -s dev_tools/tests
(with dev_tools/pylibs on PYTHONPATH).
'''

import os
import subprocess
import sys
import tempfile
import unittest

from pathlib import Path, PurePosixPath
from typing import Dict
from unittest import mock

from edgtest import inputs

DEV_TOOLS_DIR = Path(__file__).resolve().parents[1]
HASH_TOOL_PATH = DEV_TOOLS_DIR / 'bin' / 'edg-test-inputs-hash'

CONFIGS = ('edg_x86_64', 'edg_x86_64_cp', 'edg_x86_32', 'edg_x86_32_cp')

class IsInputForConfigTests(unittest.TestCase):
  '''The selection rule, file by file.'''

  def check(self, path: str, config: str, expected: bool) -> None:
    self.assertEqual(
      inputs.is_input_for_config(PurePosixPath(path), config, 'edg_x86_64'),
      expected,
      f"{path} for {config}"
    )

  def test_recordings(self) -> None:
    rto = 'tests/edg/.t.rto'
    for config in CONFIGS:
      self.check(f"{rto}/default.1.1.txt", config, True)
      self.check(f"{rto}/default.1.matches.txt", config, True)
    self.check(f"{rto}/edg_x86_32.1.1.txt", 'edg_x86_32', True)
    self.check(f"{rto}/edg_x86_32.1.1.txt", 'edg_x86_32_cp', False)
    self.check(f"{rto}/edg_x86_32_cp.1.1.txt", 'edg_x86_32', False)
    self.check(f"{rto}/edg_x86_32_cp.1.1.txt", 'edg_x86_64', False)
    # The default configuration only ever reads "default." recordings.
    self.check(f"{rto}/edg_x86_64.1.1.txt", 'edg_x86_64', False)

  def test_per_config_dirs(self) -> None:
    for top in ('expectations', 'flakey-tests'):
      self.check(f"{top}/edg_x86_64/edg.log", 'edg_x86_32', True)
      self.check(f"{top}/edg_x86_32/edg.log", 'edg_x86_32', True)
      self.check(f"{top}/edg_x86_32/edg.log", 'edg_x86_32_cp', False)
      self.check(f"{top}/edg_x86_32/edg.log", 'edg_x86_64', False)

  def test_shared_files(self) -> None:
    for config in CONFIGS:
      self.check('tests/edg/t.c', config, True)
      self.check('tests/edg/.t/input.h', config, True)
      self.check('.edgy/config.json', config, True)

class HashTestInputsTests(unittest.TestCase):
  '''hash_test_inputs on a git repository whose test home is tests/.'''

  def setUp(self) -> None:
    tmp_dir = tempfile.TemporaryDirectory()
    self.addCleanup(tmp_dir.cleanup)
    self.repo = Path(tmp_dir.name)
    self.test_home = self.repo / 'tests'
    self.git = ['git', '-C', str(self.repo)]
    subprocess.run(self.git + ['init', '-q'], check = True)
    self.write({
      '.edgy/config.json': '{}\n',
      'tests/edg/t.c': 'int main() { return 0; }\n',
      'tests/edg/.t.rto/default.1.1.txt': 'default\n',
      'tests/edg/.t.rto/edg_x86_32.1.1.txt': 'x86_32\n',
      'tests/edg/.t.rto/edg_x86_32_cp.1.1.txt': 'x86_32_cp\n',
      **{f"expectations/{config}/edg.log": f"{config}\n"
         for config in CONFIGS}
    })
    self.env = mock.patch.dict(os.environ)
    self.env.start()
    self.addCleanup(self.env.stop)
    os.environ.pop('RUN_TEST_DEFAULT_CONFIG', None)

  def write(self, files: Dict[str, str]) -> None:
    for name, content in files.items():
      path = self.test_home / name
      path.parent.mkdir(parents = True, exist_ok = True)
      path.write_text(content)
    self.commit()

  def remove(self, name: str) -> None:
    subprocess.run(self.git + ['rm', '-q', '-r', f"tests/{name}"],
                   check = True)
    self.commit()

  def commit(self) -> None:
    subprocess.run(self.git + ['add', '-A'], check = True)
    subprocess.run(
      self.git + ['-c', 'user.name=t', '-c', 'user.email=t@example.com',
                  'commit', '-q', '--allow-empty', '-m', 'Update'],
      check = True
    )

  def hashes(self) -> Dict[str, str]:
    return {
      config: inputs.hash_test_inputs(self.test_home, config)
      for config in CONFIGS
    }

  def assert_changed(self, before: Dict[str, str], *changed: str) -> None:
    after = self.hashes()
    for config in CONFIGS:
      if config in changed:
        self.assertNotEqual(before[config], after[config], config)
      else:
        self.assertEqual(before[config], after[config], config)

  def test_stable_and_distinct(self) -> None:
    before = self.hashes()
    self.assertEqual(before, self.hashes())
    self.assertEqual(len(set(before.values())), len(CONFIGS))

  def test_own_recording(self) -> None:
    before = self.hashes()
    self.write({'tests/edg/.t.rto/edg_x86_32_cp.1.1.txt': 'changed\n'})
    self.assert_changed(before, 'edg_x86_32_cp')

  def test_new_recording(self) -> None:
    before = self.hashes()
    self.write({'tests/edg/.t.rto/edg_x86_64_cp.2.1.txt': 'new\n'})
    self.assert_changed(before, 'edg_x86_64_cp')

  def test_default_recording(self) -> None:
    before = self.hashes()
    self.write({'tests/edg/.t.rto/default.1.1.txt': 'changed\n'})
    self.assert_changed(before, *CONFIGS)

  def test_test_source(self) -> None:
    before = self.hashes()
    self.write({'tests/edg/t.c': 'int main() { return 1; }\n'})
    self.assert_changed(before, *CONFIGS)

  def test_expectations(self) -> None:
    before = self.hashes()
    self.write({'expectations/edg_x86_32/edg.log': 'changed\n'})
    self.assert_changed(before, 'edg_x86_32')
    before = self.hashes()
    self.write({'expectations/edg_x86_64/edg.log': 'changed\n'})
    self.assert_changed(before, *CONFIGS)

  def test_removed_config_directory(self) -> None:
    # edgy requires every configured expectations directory to exist.
    before = self.hashes()
    self.remove('expectations/edg_x86_32')
    self.assert_changed(before, *CONFIGS)

  def test_default_config_override(self) -> None:
    before = self.hashes()
    os.environ['RUN_TEST_DEFAULT_CONFIG'] = 'edg_x86_32'
    after = self.hashes()
    # edg_x86_32 is now the default: it reads only "default." recordings and
    # its own expectations, and every configuration inherits from it.
    for config in CONFIGS:
      self.assertNotEqual(before[config], after[config], config)
    before = after
    self.write({'expectations/edg_x86_32/edg.log': 'changed\n'})
    self.assert_changed(before, *CONFIGS)

  def test_uncommitted_changes_ignored(self) -> None:
    before = self.hashes()
    (self.test_home / 'tests' / 'edg' / 't.c').write_text('uncommitted\n')
    self.assert_changed(before)

  def test_tool(self) -> None:
    result = subprocess.run(
      [sys.executable, str(HASH_TOOL_PATH), '--config', 'edg_x86_32'],
      capture_output = True,
      text = True,
      env = dict(
        os.environ,
        PYTHONPATH = str(DEV_TOOLS_DIR / 'pylibs'),
        EDG_TEST_HOME = str(self.test_home)
      )
    )
    self.assertEqual(result.returncode, 0, result.stderr)
    self.assertEqual(result.stdout.strip(),
                     inputs.hash_test_inputs(self.test_home, 'edg_x86_32'))

if __name__ == '__main__':
  unittest.main()
