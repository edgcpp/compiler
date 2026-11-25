# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Tests for the IL display filter (edgtest.il_filter).

Run with: python3 -m unittest discover -s dev_tools/tests
(with dev_tools/pylibs on PYTHONPATH).
'''

import unittest

from edgtest import il_filter

SOURCE_WITH_SPEC = '''\
extern int i;
static int j;

/* @filter
file-scope variable
  source_corresp/name, storage_class, declared_storage_class
 */
'''

IL_DUMP = '''\
file-scope variable@aaa
source_corresp:
  name:                  file-scope id-name@bbb: "i"
next:                    file-scope variable@ccc
type:                    file-scope type@ddd: int
storage_class:           sc_extern
declared_storage_class:  sc_unspecified

file-scope variable@ccc
source_corresp:
  name:                  file-scope id-name@eee: "j"
next:                    NULL
type:                    file-scope type@ddd: int
storage_class:           sc_static
declared_storage_class:  sc_static

file-scope type@ddd
next:                    NULL
kind:                    tk_integer
'''

FILTERED_IL_DUMP = '''\
file-scope variable#1
  name:                  file-scope id-name#1: "i"
storage_class:           sc_extern
declared_storage_class:  sc_unspecified

file-scope variable#2
  name:                  file-scope id-name#2: "j"
storage_class:           sc_static
declared_storage_class:  sc_static

'''


class ExtractFilterSpecTests(unittest.TestCase):
  '''The /* @filter ... */ comment extraction.'''

  def test_extract_spec(self) -> None:
    spec = il_filter.extract_filter_spec(SOURCE_WITH_SPEC)
    self.assertIsNotNone(spec)
    assert spec is not None
    self.assertEqual(
      il_filter.parse_filter_spec(spec)[0].fields,
      [
        'source_corresp/name',
        'storage_class',
        'declared_storage_class'
      ]
    )

  def test_no_spec(self) -> None:
    self.assertIsNone(il_filter.extract_filter_spec('int i;\n'))


class FilterIlDumpTests(unittest.TestCase):
  '''Selecting entity blocks and their requested fields.'''

  SPEC = '''\
file-scope variable
  source_corresp/name, storage_class, declared_storage_class
'''

  def test_filter_variables(self) -> None:
    self.assertEqual(
      il_filter.filter_il_dump(IL_DUMP, self.SPEC),
      FILTERED_IL_DUMP
    )

  def test_absent_optional_field_is_omitted(self) -> None:
    il_dump = '''\
file-scope variable@aaa
source_corresp:
  name:                  file-scope id-name@bbb: "i"
storage_class:           sc_extern
'''
    expected = '''\
file-scope variable#1
  name:                  file-scope id-name#1: "i"
storage_class:           sc_extern

'''
    self.assertEqual(il_filter.filter_il_dump(il_dump, self.SPEC), expected)


if __name__ == '__main__':
  unittest.main()
