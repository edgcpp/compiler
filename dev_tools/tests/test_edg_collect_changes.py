# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Tests for edg-collect-changes, edgchanges (the pending Changes entry
format), the entry checks in edg-check-file-changes, and the edgutil directive
helper they use.

Run with: python3 -m unittest discover -s dev_tools/tests
(with dev_tools/pylibs on PYTHONPATH).
'''

import contextlib
import datetime
import importlib.machinery
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

import edgchanges
import edgutil

from pathlib import Path
from typing import Any, Dict

DEV_TOOLS_DIR = Path(__file__).resolve().parents[1]
REPO_DIR = DEV_TOOLS_DIR.parent
COLLECT_TOOL_PATH = DEV_TOOLS_DIR / 'bin' / 'edg-collect-changes'
CHECK_TOOL_PATH = DEV_TOOLS_DIR / 'bin' / 'edg-check-file-changes'

def _load_collect_tool() -> Any:
  loader = importlib.machinery.SourceFileLoader(
    'edg_collect_changes',
    str(COLLECT_TOOL_PATH)
  )
  spec = importlib.util.spec_from_loader(loader.name, loader)
  assert spec is not None
  module = importlib.util.module_from_spec(spec)
  # Keep dev_tools/bin free of __pycache__.
  dont_write_bytecode = sys.dont_write_bytecode
  sys.dont_write_bytecode = True
  try:
    loader.exec_module(module)
  finally:
    sys.dont_write_bytecode = dont_write_bytecode
  return module

collect_tool = _load_collect_tool()

OPEN_CHANGES = '''Version 7.1

10/20/26 [GH #38]
Existing entry

Existing body.


-------------------------------------------------------------------------------
Version 7.0, September 28, 2026

9/1/26   [EDGcpfe/1]
Old entry

Version 6.9 of the front end is mentioned in this body.
'''

RELEASED_CHANGES = '''Version 7.0, September 28, 2026

No changes.


-------------------------------------------------------------------------------
Version 6.10, June 1, 2026

No changes.
'''

def _fragment(tags: str, summary: str, body: str = 'Body.') -> str:
  return f"{tags}\n{summary}\n\n{body}\n"

class FakeTree:
  '''A scratch tree of Changes files and pending_changes/ directories.'''

  def __init__(self, root: Path) -> None:
    self.root = root

  def add_changes(self, dir_name: str, text: str) -> Path:
    dir_path = self.root / dir_name
    (dir_path / 'pending_changes').mkdir(parents = True)
    (dir_path / 'pending_changes' / '.gitignore').write_text('# Keep.\n')
    changes_path = dir_path / 'Changes'
    changes_path.write_text(text, encoding = 'utf-8')
    return changes_path

  def add_fragment(self, dir_name: str, name: str, text: str) -> Path:
    path = self.root / dir_name / 'pending_changes' / name
    path.write_text(text, encoding = 'utf-8')
    return path

  def collect(self, dir_name: str = 'src',
              date: datetime.date = datetime.date(2026, 10, 21),
              version: Any = None, dry_run: bool = False) -> bool:
    with contextlib.redirect_stdout(io.StringIO()), \
         contextlib.redirect_stderr(io.StringIO()):
      return collect_tool.collect(
        self.root / dir_name / 'Changes',
        self.root / dir_name / 'pending_changes',
        date,
        version,
        dry_run = dry_run
      )

class TreeTestCase(unittest.TestCase):

  def setUp(self) -> None:
    tmp_dir = tempfile.TemporaryDirectory()
    self.addCleanup(tmp_dir.cleanup)
    self.tree = FakeTree(Path(tmp_dir.name))

class FormatTests(unittest.TestCase):

  def test_date_padding(self) -> None:
    for date, expected in [
      (datetime.date(2026, 1, 2), '1/2/26   [GH #1]'),
      (datetime.date(2026, 1, 21), '1/21/26  [GH #1]'),
      (datetime.date(2026, 10, 21), '10/21/26 [GH #1]')
    ]:
      date_text = edgchanges.format_date(date)
      self.assertEqual(
        edgchanges.format_tag_lines(date_text, ['GH #1']),
        [expected]
      )

  def test_wrap_matches_existing_changes(self) -> None:
    # src/Changes wraps long tag lists this way (see the 3/5/26 entry for
    # "Pragma processing corrections").
    tags = [
      'EDGcpfe/23609', 'EDGcpfe/23845', 'EDGcpfe/23927', 'EDGcpfe/24247',
      'EDGcpfe/27220', 'EDGcpfe/28636'
    ]
    self.assertEqual(
      edgchanges.format_tag_lines('3/5/26', tags),
      [
        '3/5/26   [EDGcpfe/23609,EDGcpfe/23845,EDGcpfe/23927,EDGcpfe/24247,',
        '          EDGcpfe/27220,EDGcpfe/28636]'
      ]
    )

  def test_wrap_exactly_at_limit(self) -> None:
    # 10 columns of prefix + 69 columns of tags fill the line exactly.
    tags = ['GH #1234567', 'GH #1234567', 'GH #1234567', 'GH #1234567',
            'GH #1234567', 'GH #1234']
    lines = edgchanges.format_tag_lines('10/21/26', tags)
    self.assertEqual(len(lines), 1)
    self.assertEqual(len(lines[0]), 79)

    lines = edgchanges.format_tag_lines('10/21/26', tags + ['GH #1'])
    self.assertEqual(
      lines,
      [
        '10/21/26 [GH #1234567,GH #1234567,GH #1234567,GH #1234567,'
        'GH #1234567,GH #1234,',
        '          GH #1]'
      ]
    )

class ParseTests(TreeTestCase):

  def _parse(self, text: str, check_line_length: bool = True) -> Any:
    self.tree.root.joinpath('x.txt').write_text(text, encoding = 'utf-8')
    return edgchanges.parse_fragment(
      self.tree.root / 'x.txt',
      check_line_length = check_line_length
    )

  def test_good_fragment(self) -> None:
    entry, errors = self._parse(
      '//allowed_words: frob  \n'
      '[GH #2,\n'
      '      GH #1]\n'
      'Summary  \n'
      '\n'
      'Body line.   \n'
      '\n'
      '  //not: a directive\n'
      '\n'
      '\n'
    )
    self.assertEqual(errors, [])
    self.assertEqual(entry.tags, ['GH #2', 'GH #1'])
    self.assertEqual(entry.summary, 'Summary')
    self.assertEqual(
      entry.body_lines,
      ['Body line.', '', '  //not: a directive']
    )

  def test_crlf(self) -> None:
    entry, errors = self._parse('[GH #1]\r\nSummary\r\n\r\nBody.\r\n')
    self.assertEqual(errors, [])
    self.assertEqual(entry.body_lines, ['Body.'])

  def _assert_error(self, text: str, expected: str) -> None:
    entry, errors = self._parse(text)
    self.assertIsNone(entry)
    self.assertTrue(
      any(expected in error.message for error in errors),
      f"{expected!r} not in {errors!r}"
    )

  def test_errors(self) -> None:
    self._assert_error('Summary\n\nBody.\n', 'expected the tag block')
    self._assert_error(_fragment('[]', 'Summary'), 'empty tag block')
    self._assert_error(
      _fragment('[EDGcpfe/1]', 'Summary'),
      "invalid tag 'EDGcpfe/1'"
    )
    self._assert_error(
      _fragment('[GH #1,GH #1]', 'Summary'),
      "tag 'GH #1' is repeated"
    )
    self._assert_error(_fragment('[GH #1', 'Summary'), "no closing ']'")
    self._assert_error(
      _fragment('[GH #1] Summary', 'More'),
      "text after the tag block's ']'"
    )
    self._assert_error('[GH #1]\n\nBody.\n', 'expected the summary line')
    self._assert_error(
      _fragment('[GH #1]', '  Summary'),
      'summary line must not be indented'
    )
    self._assert_error('[GH #1]\nSummary\nBody.\n', 'expected one blank line')
    self._assert_error('[GH #1]\nSummary\n', 'expected one blank line')
    self._assert_error(
      '[GH #1]\nSummary\n\n\nBody.\n',
      'expected one blank line'
    )
    self._assert_error(_fragment('[GH #1]', 'Sum\tmary'), 'tab character')
    self._assert_error(
      _fragment('[GH #1]', 'Summary', 'x' * 80),
      'line longer than 79 columns'
    )

  def test_line_length(self) -> None:
    entry, errors = self._parse(
      _fragment('[GH #1]', 'Summary', 'x' * 79 + '    ')
    )
    self.assertEqual(errors, [])

    entry, errors = self._parse(
      _fragment('[GH #1]', 'Summary', 'x' * 80),
      check_line_length = False
    )
    self.assertEqual(errors, [])

  def test_not_utf8(self) -> None:
    self.tree.root.joinpath('x.txt').write_bytes(
      b'[GH #1]\nSummary\n\n\xe9\n'
    )
    entry, errors = edgchanges.parse_fragment(self.tree.root / 'x.txt')
    self.assertIsNone(entry)
    self.assertIsNone(errors[0].line_no)
    self.assertIn('not valid UTF-8', errors[0].message)

class CollectTests(TreeTestCase):

  def test_open_section(self) -> None:
    changes_path = self.tree.add_changes('src', OPEN_CHANGES)
    zeta = self.tree.add_fragment(
      'src', 'gh-2.txt', _fragment('[GH #2]', 'zeta', 'Zeta body.  ')
    )
    alpha = self.tree.add_fragment(
      'src', 'gh-1.txt',
      '//allowed_words: frob\n' + _fragment('[GH #1]', 'Alpha')
    )

    self.assertTrue(self.tree.collect())

    self.assertEqual(
      changes_path.read_text(encoding = 'utf-8'),
      'Version 7.1\n'
      '\n'
      '10/21/26 [GH #1]\n'
      'Alpha\n'
      '\n'
      'Body.\n'
      '\n'
      '\n'
      '10/21/26 [GH #2]\n'
      'zeta\n'
      '\n'
      'Zeta body.\n'
      '\n'
      '\n' +
      OPEN_CHANGES[len('Version 7.1\n\n'):]
    )
    self.assertFalse(zeta.exists())
    self.assertFalse(alpha.exists())

  def test_matches_real_changes_layout(self) -> None:
    # Collecting the newest src/Changes entry, written as a pending entry,
    # into the file without it reproduces the file.
    real_text = (REPO_DIR / 'src' / 'Changes').read_text(encoding = 'utf-8')
    lines = real_text.split('\n')
    self.assertRegex(lines[0], r'^Version [0-9]+\.[0-9]+$')
    self.assertEqual(lines[1], '')
    next_entry_index = next(
      index for index in range(3, len(lines))
      if lines[index - 2:index] == ['', ''] and lines[index] != ''
    )
    entry_lines = lines[2:next_entry_index - 2]
    date_text, tags_text = entry_lines[0].split(' ', 1)
    if (any(line.startswith('          ') for line in entry_lines[1:2]) or
        '[GH #' not in tags_text):
      self.skipTest('newest src/Changes entry is not a single-line GH entry')

    changes_path = self.tree.add_changes(
      'src',
      '\n'.join(lines[:2] + lines[next_entry_index:])
    )
    self.tree.add_fragment(
      'src', 'newest.txt',
      '\n'.join([tags_text.strip()] + entry_lines[1:]) + '\n'
    )
    month, day, year = (int(part) for part in date_text.split('/'))
    self.assertTrue(
      self.tree.collect(date = datetime.date(2000 + year, month, day))
    )
    self.assertEqual(changes_path.read_text(encoding = 'utf-8'), real_text)

  def test_released_section(self) -> None:
    self.tree.add_changes('src', OPEN_CHANGES)
    changes_path = self.tree.add_changes('lib_src', RELEASED_CHANGES)
    self.tree.add_fragment(
      'lib_src', 'gh-5.txt', _fragment('[GH #5]', 'Library')
    )

    self.assertTrue(self.tree.collect('lib_src', version = '7.1'))

    self.assertEqual(
      changes_path.read_text(encoding = 'utf-8'),
      'Version 7.1\n'
      '\n'
      '10/21/26 [GH #5]\n'
      'Library\n'
      '\n'
      'Body.\n'
      '\n'
      '\n'
      f"{'-' * 79}\n" +
      RELEASED_CHANGES
    )

  def test_released_section_needs_version(self) -> None:
    self.tree.add_changes('lib_src', RELEASED_CHANGES)
    self.tree.add_fragment(
      'lib_src', 'gh-5.txt', _fragment('[GH #5]', 'Library')
    )
    with self.assertRaisesRegex(ValueError, 'use --version'):
      self.tree.collect('lib_src')

    self.assertTrue(self.tree.collect('lib_src', version = '8.0'))
    changes_path = self.tree.root / 'lib_src' / 'Changes'
    self.assertTrue(
      changes_path.read_text(encoding = 'utf-8').startswith('Version 8.0\n')
    )

  def test_read_open_version(self) -> None:
    # What --version-from reads.
    self.assertEqual(
      collect_tool.read_open_version(
        self.tree.add_changes('src', OPEN_CHANGES)
      ),
      '7.1'
    )
    self.assertIsNone(
      collect_tool.read_open_version(
        self.tree.add_changes('lib_src', RELEASED_CHANGES)
      )
    )

  def test_no_changes_replaced(self) -> None:
    changes_path = self.tree.add_changes(
      'src',
      'Version 7.1\n\nNo changes.\n\n\n' + RELEASED_CHANGES.split('\n', 5)[5]
    )
    self.tree.add_fragment('src', 'a.txt', _fragment('[GH #1]', 'A'))

    self.assertTrue(self.tree.collect())

    self.assertEqual(
      changes_path.read_text(encoding = 'utf-8'),
      'Version 7.1\n\n10/21/26 [GH #1]\nA\n\nBody.\n\n\n' +
      RELEASED_CHANGES.split('\n', 5)[5]
    )

  def test_header_needs_separator(self) -> None:
    # "Version 6.9 ..." in a body isn't a header, and neither is a version
    # line that doesn't follow a separator.
    lines = OPEN_CHANGES.split('\n')
    self.assertEqual(
      collect_tool._find_newest_version_header(lines[2:] + lines[:2]),
      lines[2:].index('Version 7.0, September 28, 2026')
    )
    self.assertIsNone(
      collect_tool._find_newest_version_header(['', 'Version 7.1'])
    )

  def test_sort_tie_break(self) -> None:
    changes_path = self.tree.add_changes('src', OPEN_CHANGES)
    self.tree.add_fragment('src', 'b.txt', _fragment('[GH #2]', 'Same'))
    self.tree.add_fragment('src', 'a.txt', _fragment('[GH #1]', 'Same'))
    self.tree.add_fragment('src', 'c.txt', _fragment('[GH #3]', 'same'))

    self.assertTrue(self.tree.collect())

    tag_lines = [
      line for line in changes_path.read_text(encoding = 'utf-8').split('\n')
      if line.startswith('10/21/26')
    ]
    self.assertEqual(
      tag_lines,
      ['10/21/26 [GH #1]', '10/21/26 [GH #2]', '10/21/26 [GH #3]']
    )

  def test_bad_fragment_left_in_place(self) -> None:
    changes_path = self.tree.add_changes('src', OPEN_CHANGES)
    good = self.tree.add_fragment(
      'src', 'good.txt', _fragment('[GH #1]', 'Good')
    )
    bad = self.tree.add_fragment(
      'src', 'bad.txt', _fragment('[EDGcpfe/1]', 'Bad')
    )

    self.assertFalse(self.tree.collect())

    self.assertFalse(good.exists())
    self.assertTrue(bad.exists())
    text = changes_path.read_text(encoding = 'utf-8')
    self.assertIn('Good', text)
    self.assertNotIn('Bad', text)

  def test_nothing_pending(self) -> None:
    changes_path = self.tree.add_changes('src', OPEN_CHANGES)
    self.tree.add_fragment('src', 'notes.md', 'Not an entry.\n')
    before = changes_path.stat().st_mtime_ns

    self.assertTrue(self.tree.collect())

    self.assertEqual(changes_path.stat().st_mtime_ns, before)
    self.assertEqual(changes_path.read_text(encoding = 'utf-8'), OPEN_CHANGES)

  def test_dry_run(self) -> None:
    changes_path = self.tree.add_changes('src', OPEN_CHANGES)
    fragment = self.tree.add_fragment(
      'src', 'a.txt', _fragment('[GH #1]', 'A')
    )

    self.assertTrue(self.tree.collect(dry_run = True))

    self.assertTrue(fragment.exists())
    self.assertEqual(changes_path.read_text(encoding = 'utf-8'), OPEN_CHANGES)

  def test_check_rejects_misnamed_file(self) -> None:
    self.tree.add_changes('src', OPEN_CHANGES)
    self.tree.add_fragment('src', 'notes.md', _fragment('[GH #1]', 'A'))
    with contextlib.redirect_stdout(io.StringIO()) as output:
      self.assertFalse(
        collect_tool.check(self.tree.root / 'src' / 'pending_changes')
      )
    self.assertIn('notes.md: not a pending Changes entry', output.getvalue())

class FragmentPathTests(unittest.TestCase):

  def test_is_fragment_path(self) -> None:
    self.assertTrue(edgchanges.is_fragment_path('src/pending_changes/a.txt'))
    self.assertTrue(
      edgchanges.is_fragment_path('include_c++/pending_changes/gh-1-x.txt')
    )
    self.assertFalse(edgchanges.is_fragment_path('src/pending_changes/a.md'))
    self.assertFalse(
      edgchanges.is_fragment_path('src/pending_changes/.gitignore')
    )
    self.assertFalse(edgchanges.is_fragment_path('src/error_msg.txt'))
    self.assertFalse(
      edgchanges.is_fragment_path('src/pending_changes/sub/a.txt')
    )

class DirectiveTests(unittest.TestCase):

  def test_leading_block_only(self) -> None:
    directive_map, remaining_lines = (
      edgutil.split_leading_cpp_line_directives([
        '//allowed_words: a b',
        '//allowed_words: c',
        '[GH #1]',
        '//allowed_words: d'
      ])
    )
    self.assertEqual(directive_map, {'allowed_words': ['a b', 'c']})
    self.assertEqual(remaining_lines, ['[GH #1]', '//allowed_words: d'])

  def test_no_block(self) -> None:
    directive_map, remaining_lines = (
      edgutil.split_leading_cpp_line_directives(['[GH #1]', '//x: y'])
    )
    self.assertEqual(directive_map, {})
    self.assertEqual(remaining_lines, ['[GH #1]', '//x: y'])

def _aspell_works() -> bool:
  aspell_path = shutil.which('aspell')
  if aspell_path is None:
    return False
  return subprocess.run(
    [aspell_path, 'list'],
    input = b'cat',
    stdout = subprocess.DEVNULL,
    stderr = subprocess.DEVNULL
  ).returncode == 0

class CheckFileChangesTests(unittest.TestCase):
  '''Changes entry rules in edg-check-file-changes.'''

  def setUp(self) -> None:
    tmp_dir = tempfile.TemporaryDirectory()
    self.addCleanup(tmp_dir.cleanup)
    self.tmp_dir = Path(tmp_dir.name)

  def _check(self, repo_path: str, text: str) -> Dict[str, Any]:
    # Like the pre-push hook's, the temporary files aren't in a
    # pending_changes/ directory; only the key is.
    old_file = self.tmp_dir / 'oldXYZ.txt'
    old_file.write_text('')
    new_file = self.tmp_dir / 'newXYZ.txt'
    new_file.write_text(text, encoding = 'utf-8')
    input_file = self.tmp_dir / 'input.json'
    output_file = self.tmp_dir / 'output.json'
    input_file.write_text(
      json.dumps({repo_path: [str(old_file), str(new_file)]})
    )
    # The checker looks for the entry's Changes file relative to its working
    # directory.
    subprocess.run(
      [sys.executable, str(CHECK_TOOL_PATH), str(input_file),
       str(output_file)],
      check = True,
      cwd = str(self.tmp_dir),
      env = dict(os.environ, PYTHONPATH = str(DEV_TOOLS_DIR / 'pylibs'))
    )
    return json.loads(output_file.read_text())[repo_path]

  def test_format_errors(self) -> None:
    results = self._check(
      'src/pending_changes/a.txt',
      '//allowed_words: x\n[EDGcpfe/1]\nSummary\n\nBody.\n'
    )
    self.assertEqual(
      results['format_errors'],
      [[2, "invalid tag 'EDGcpfe/1'; use 'GH #N'"]]
    )

    results = self._check(
      'src/pending_changes/a.txt',
      _fragment('[GH #1]', 'Summary')
    )
    self.assertEqual(results['format_errors'], [])

    # Other files have no format to check.
    results = self._check('notes.txt', 'Anything.\n')
    self.assertEqual(results['format_errors'], [])

  def test_trailing_whitespace_not_overlong(self) -> None:
    text = _fragment('[GH #1]', 'Summary', 'x' * 79 + '   ')
    results = self._check('src/pending_changes/a.txt', text)
    self.assertEqual(results['overlong_lines'], {})

    results = self._check('src/pending_changes/a.txt',
                          _fragment('[GH #1]', 'Summary', 'x' * 80))
    self.assertEqual(list(results['overlong_lines']), ['4'])

  @unittest.skipUnless(_aspell_works(), 'aspell is not available')
  def test_allowed_words(self) -> None:
    text = _fragment('[GH #1,\n GH #2]', 'Summary', 'A frobnicator lvalue.')
    results = self._check('src/pending_changes/a.txt', text)
    # The tag block isn't spell checked.
    self.assertEqual(results['misspellings'], ['frobnicator', 'lvalue'])

    results = self._check(
      'src/pending_changes/a.txt',
      '//allowed_words: frobnicator\n//allowed_words: lvalue\n' + text
    )
    self.assertEqual(results['misspellings'], [])

  @unittest.skipUnless(_aspell_works(), 'aspell is not available')
  def test_words_in_target_changes_file(self) -> None:
    # Words the target Changes file already uses aren't reported; other
    # Changes files don't count.
    (self.tmp_dir / 'src').mkdir()
    (self.tmp_dir / 'src' / 'Changes').write_text(
      'Version 7.1\n\nAn lvalue.\n'
    )
    (self.tmp_dir / 'util').mkdir()
    (self.tmp_dir / 'util' / 'Changes').write_text(
      'Version 7.1\n\nA frobnicator.\n'
    )
    text = _fragment('[GH #1]', 'Summary', 'A frobnicator lvalue.')

    results = self._check('src/pending_changes/a.txt', text)
    self.assertEqual(results['misspellings'], ['frobnicator'])

    results = self._check('util/pending_changes/a.txt', text)
    self.assertEqual(results['misspellings'], ['lvalue'])

  @unittest.skipUnless(_aspell_works(), 'aspell is not available')
  def test_plain_text_file_unchanged(self) -> None:
    # Outside pending_changes/, directives get no special treatment.
    results = self._check(
      'notes.txt',
      '//allowed_words: frobnicator\nA frobnicator.\n'
    )
    self.assertIn('frobnicator', results['misspellings'])

if __name__ == '__main__':
  unittest.main()
