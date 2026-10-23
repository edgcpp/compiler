# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Tests for the test suite names edgtest accepts (which edgy checks on
loading .edgy/config.json).

Run with: python3 -m unittest discover -s dev_tools/tests
(with dev_tools/pylibs on PYTHONPATH).
'''

import json
import unittest

import edgtest

from pathlib import Path

REPO_DIR = Path(__file__).resolve().parents[2]

class SuiteNameTests(unittest.TestCase):
  def test_valid_names(self) -> None:
    for name in ['cwg', 'edg_x86', 'cpp-23', 'A1', 's' * 64]:
      with self.subTest(name = name):
        self.assertTrue(edgtest.is_valid_suite_name(name))

  def test_invalid_names(self) -> None:
    for name in ['', 's' * 65, 'a.b', 'a/b', '..', 'a b', 'café',
                 'cwg\n']:
      with self.subTest(name = name):
        self.assertFalse(edgtest.is_valid_suite_name(name))

  def test_the_test_homes_suites_are_valid(self) -> None:
    config_path = REPO_DIR / 'tests' / '.edgy' / 'config.json'
    test_suites = json.loads(config_path.read_text())['test_suites']
    for name in [*test_suites['primary'], *test_suites['secondary']]:
      with self.subTest(name = name):
        self.assertTrue(edgtest.is_valid_suite_name(name))

if __name__ == '__main__':
  unittest.main()
