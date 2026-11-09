# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Tests for the line length exemptions of edg-check-file-changes and for the
hierarchical spelling allowlist (edgspelling).

Run with: python3 -m unittest discover -s dev_tools/tests
(with dev_tools/pylibs on PYTHONPATH).
'''

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

import edgspelling
import edgshell

from tool_tree import PYLIBS_DIR, make_tool_tree

from pathlib import Path

DEV_TOOLS_DIR = Path(__file__).resolve().parents[1]
CHECK_TOOL_PATH = DEV_TOOLS_DIR / 'bin' / 'edg-check-file-changes'

LONG_LINE = 'x' * 100 + '\n'

def _run_check(tmp_dir: Path, name: str, content) -> dict:
  '''Run the tool on a new file called name; return its result entry.'''
  file_path = tmp_dir / name
  if isinstance(content, bytes):
    file_path.write_bytes(content)
  else:
    file_path.write_text(content)
  # The old version is empty, so everything in the new file is new.
  old_path = tmp_dir / ('old_' + name)
  old_path.write_text('')
  input_path = tmp_dir / 'input.json'
  output_path = tmp_dir / 'output.json'
  input_path.write_text(json.dumps({name: [str(old_path), str(file_path)]}))
  # An empty allowlist keeps the result independent of the shipped one.
  pylibs_dir = make_tool_tree(tmp_dir, {})
  subprocess.run(
    [sys.executable, str(CHECK_TOOL_PATH), str(input_path), str(output_path)],
    check = True,
    cwd = '/',
    env = {**os.environ, 'PYTHONPATH': str(pylibs_dir)}
  )
  return json.loads(output_path.read_text())[name]

def _run_tool(tmp_dir: Path, name: str, content: str, *,
              pylibs_dir: Path = PYLIBS_DIR) -> subprocess.CompletedProcess:
  '''Run the tool on a new file called name, with pylibs_dir as its
  packages; the result entry is in tmp_dir/output.json if it succeeded.'''
  file_path = tmp_dir / name
  file_path.write_text(content)
  # The old version is empty, so everything in the new file is new.
  old_path = tmp_dir / ('old_' + name)
  old_path.write_text('')
  input_path = tmp_dir / 'input.json'
  input_path.write_text(json.dumps({name: [str(old_path), str(file_path)]}))
  return subprocess.run(
    [sys.executable, str(CHECK_TOOL_PATH), str(input_path),
     str(tmp_dir / 'output.json')],
    cwd = '/',
    env = {**os.environ, 'PYTHONPATH': str(pylibs_dir)},
    capture_output = True,
    text = True
  )

@unittest.skipUnless(shutil.which('aspell'), 'aspell not installed')
class ToolAllowlistTests(unittest.TestCase):
  '''The tool's use of the allowlist: words ignore case, names don't.'''

  def setUp(self) -> None:
    self._tmp = tempfile.TemporaryDirectory()
    self.addCleanup(self._tmp.cleanup)
    self.tmp_dir = Path(self._tmp.name)

  def _misspellings(self, allowlist, content: str):
    pylibs_dir = make_tool_tree(self.tmp_dir, allowlist)
    result = _run_tool(
      self.tmp_dir, 'a.md', content, pylibs_dir = pylibs_dir
    )
    self.assertEqual(result.returncode, 0, result.stderr)
    output = json.loads((self.tmp_dir / 'output.json').read_text())
    return output['a.md']['misspellings']

  def test_unlisted_words_are_reported(self) -> None:
    self.assertEqual(
      self._misspellings({}, 'zxqvortle\n'), ['zxqvortle']
    )

  def test_words_match_in_any_case(self) -> None:
    self.assertEqual(
      self._misspellings(
        {'words': ['zxqvortle']}, 'zxqvortle Zxqvortle ZXQVORTLE\n'
      ),
      []
    )

  def test_names_match_exactly(self) -> None:
    self.assertEqual(
      self._misspellings(
        {'names': ['Zxqvortle']}, 'Zxqvortle zxqvortle ZXQVORTLE\n'
      ),
      ['ZXQVORTLE', 'zxqvortle']
    )

  def test_refuses_to_start_with_a_bad_allowlist(self) -> None:
    for allowlist in (
      '{"words": ["b", "a"]}',
      '{"words": ["A"]}',
      '{"names": ["b", "A"]}',
      '{',
    ):
      with self.subTest(allowlist = allowlist), \
           tempfile.TemporaryDirectory() as tmp_dir_str:
        tmp_dir = Path(tmp_dir_str)
        pylibs_dir = make_tool_tree(tmp_dir, allowlist)
        result = _run_tool(tmp_dir, 'a.md', 'hi\n', pylibs_dir = pylibs_dir)
        self.assertEqual(result.returncode, 1)
        self.assertIn('Invalid spelling allowlist', result.stderr)
        self.assertFalse((tmp_dir / 'output.json').exists())

class OverlongLineTests(unittest.TestCase):
  def setUp(self) -> None:
    self._tmp = tempfile.TemporaryDirectory()
    self.addCleanup(self._tmp.cleanup)
    self.tmp_dir = Path(self._tmp.name)

  def test_data_files_are_exempt(self) -> None:
    for name in ('a.json', 'a.yml', 'a.yaml'):
      with self.subTest(name = name):
        result = _run_check(self.tmp_dir, name, LONG_LINE)
        self.assertEqual(len(result['overlong_lines']), 0)

  def test_other_files_are_still_checked(self) -> None:
    for name in ('a.md', 'a.py'):
      with self.subTest(name = name):
        result = _run_check(self.tmp_dir, name, LONG_LINE)
        self.assertEqual(len(result['overlong_lines']), 1)

  def test_md_tables_are_exempt(self) -> None:
    content = MD_TABLE + 'Text.\n'
    result = _run_check(self.tmp_dir, 'a.md', content)
    self.assertEqual(result['overlong_lines'], {})

  def test_md_text_after_a_table_is_checked(self) -> None:
    content = MD_TABLE + LONG_LINE
    result = _run_check(self.tmp_dir, 'a.md', content)
    self.assertEqual(list(result['overlong_lines']), ['4'])
    _, context = result['overlong_lines']['4']
    # The table is still shown as context.
    self.assertEqual(context, MD_TABLE.splitlines(True)[-1:] + [LONG_LINE])

  def test_md_pipe_lines_without_a_delimiter_row_are_checked(self) -> None:
    result = _run_check(self.tmp_dir, 'a.md', '| ' + LONG_LINE)
    self.assertEqual(list(result['overlong_lines']), ['1'])

  def test_rst_grid_tables_are_exempt(self) -> None:
    content = RST_GRID_TABLE + '\nText.\n'
    result = _run_check(self.tmp_dir, 'a.rst', content)
    self.assertEqual(result['overlong_lines'], {})

  def test_rst_simple_tables_are_exempt(self) -> None:
    content = 'Title\n=====\n\n' + RST_SIMPLE_TABLE + '\n' + LONG_LINE
    result = _run_check(self.tmp_dir, 'a.rst', content)
    self.assertEqual(list(result['overlong_lines']), ['10'])

  def test_rst_line_blocks_are_exempt(self) -> None:
    content = (
      'Text.\n'
      '\n'
      '  | ' + LONG_LINE +
      '  |\n'
      '  | Short.\n'
      '      ' + LONG_LINE +
      '\n' +
      LONG_LINE
    )
    result = _run_check(self.tmp_dir, 'a.rst', content)
    self.assertEqual(list(result['overlong_lines']), ['8'])

  def test_rst_pipes_without_a_space_are_checked(self) -> None:
    result = _run_check(self.tmp_dir, 'a.rst', '|' + LONG_LINE)
    self.assertEqual(list(result['overlong_lines']), ['1'])

  def test_tables_in_other_files_are_checked(self) -> None:
    result = _run_check(self.tmp_dir, 'a.py', MD_TABLE + RST_GRID_TABLE)
    self.assertEqual(len(result['overlong_lines']), 2)

LONG_CELL = 'y' * 90

MD_TABLE = (
  '| Name | Description |\n'
  '| --- | :-: |\n'
  f"| a | {LONG_CELL} |\n"
)

RST_GRID_TABLE = (
  '+------+-------------+\n'
  '| Name | Description |\n'
  '+======+=============+\n'
  f"| a    | {LONG_CELL} |\n"
  '+------+-------------+\n'
)

RST_SIMPLE_TABLE = (
  '====  ===========\n'
  'Name  Description\n'
  '====  ===========\n'
  f"a     {LONG_CELL}\n"
  '====  ===========\n'
)

class BinaryFilesInDiffTests(unittest.TestCase):
  '''The pre-push hook skips the files git says are binary.'''

  @unittest.skipUnless(shutil.which('git'), 'git not installed')
  def test_matches_git(self) -> None:
    with tempfile.TemporaryDirectory() as tmp_dir_str:
      repo = Path(tmp_dir_str)

      def git(*args: str) -> str:
        return subprocess.run(
          ['git', '-c', 'user.name=t', '-c', 'user.email=t@example.com',
           '-c', 'commit.gpgsign=false', *args],
          cwd = repo, check = True, capture_output = True, text = True
        ).stdout

      git('init', '-q')
      (repo / 'base.txt').write_text('base\n')
      # Git warns about the negative pattern each time it reads this file.
      # The warning is written before the first record of the numstat output
      # (attr.dat, below), so it must not become part of that record.
      (repo / '.gitattributes').write_text('attr.dat binary\n!*.x binary\n')
      git('add', '.')
      git('commit', '-q', '--no-verify', '-m', 'base')
      (repo / 'text.md').write_text('words\n')
      (repo / 'data.bin').write_bytes(b'ab\0cd')
      (repo / 'héllo.bin').write_bytes(b'\0\0')
      (repo / 'attr.dat').write_text('looks like text\n')
      git('add', '.')
      git('commit', '-q', '--no-verify', '-m', 'new')
      self.assertEqual(
        edgshell.list_binary_files_in_git_diff('HEAD~1..HEAD', cwd = repo),
        {'data.bin', 'héllo.bin', 'attr.dat'}
      )

  @unittest.skipUnless(shutil.which('git'), 'git not installed')
  def test_git_failure_is_not_fatal(self) -> None:
    with tempfile.TemporaryDirectory() as tmp_dir_str:
      repo = Path(tmp_dir_str)
      subprocess.run(['git', 'init', '-q'], cwd = repo, check = True)
      self.assertEqual(
        edgshell.list_binary_files_in_git_diff('no-such-rev..HEAD',
                                               cwd = repo),
        set()
      )

class AllowlistTests(unittest.TestCase):
  def setUp(self) -> None:
    self._tmp = tempfile.TemporaryDirectory()
    self.addCleanup(self._tmp.cleanup)
    self.tree_path = Path(self._tmp.name) / 'words.json'

  def _write(self, tree) -> None:
    self.tree_path.write_text(
      tree if isinstance(tree, str) else json.dumps(tree)
    )

  def _words(self, repo_path: str):
    return edgspelling.Allowlist(self.tree_path).words_for_path(repo_path)

  def _names(self, repo_path: str):
    return edgspelling.Allowlist(self.tree_path).names_for_path(repo_path)

  def test_union_down_the_tree(self) -> None:
    self._write({
      'words': ['global'],
      'names': ['Root'],
      'paths': {
        'src': {
          'words': ['dirword'],
          'paths': {'a.c': {'words': ['fileword'], 'names': ['File']}}
        },
        'doc': {'words': ['docword']}
      }
    })
    self.assertEqual(self._words('README.md'), {'global'})
    self.assertEqual(self._words('src/b.c'), {'global', 'dirword'})
    self.assertEqual(
      self._words('src/a.c'), {'global', 'dirword', 'fileword'}
    )
    self.assertEqual(self._words('doc/x/y.rst'), {'global', 'docword'})
    self.assertEqual(self._names('README.md'), {'Root'})
    self.assertEqual(self._names('src/b.c'), {'Root'})
    self.assertEqual(self._names('src/a.c'), {'Root', 'File'})

  def test_names_are_per_path(self) -> None:
    self._write({'paths': {'src': {'names': ['Bjarne']}}})
    self.assertEqual(self._names('src/a.c'), {'Bjarne'})
    self.assertEqual(self._names('doc/a.rst'), set())

  def test_path_names_do_not_collide_with_keys(self) -> None:
    self._write({'paths': {'words': {'words': ['inner']}}})
    self.assertEqual(self._words('words/f.txt'), {'inner'})
    self.assertEqual(self._words('f.txt'), set())

  def test_missing_file_allows_nothing(self) -> None:
    self.assertEqual(self._words('src/a.c'), set())

  def test_malformed_files_raise(self) -> None:
    for content in (
      '{',
      '[]',
      '{"words": "x"}',
      '{"words": [1]}',
      '{"names": "x"}',
      '{"words": ["Mixed"]}',
      '{"paths": []}',
      '{"other": 1}',
      '{"paths": {"src": {"wrods": []}}}',
      # Out of order, or duplicated, at the root and deeper in the tree.
      '{"words": ["beta", "alpha"]}',
      '{"names": ["Beta", "Alpha"]}',
      '{"words": ["alpha", "alpha"]}',
      '{"names": ["Alpha", "Alpha"]}',
      '{"paths": {"src": {"paths": {"a.c": {"words": ["b", "a"]}}}}}',
      # Names sort by code point, so a capital comes before a lowercase.
      '{"names": ["alpha", "Beta"]}',
    ):
      with self.subTest(content = content):
        self._write(content)
        with self.assertRaises(edgspelling.AllowlistError):
          self._words('src/a.c')

  def test_sorted_lists_load(self) -> None:
    self._write({
      'words': ['alpha', 'beta', 'gamma'],
      'names': ['Alpha', 'Beta', 'alpha'],
    })
    edgspelling.Allowlist(self.tree_path)

  def test_errors_name_the_problem(self) -> None:
    for content, expected in (
      ('{"words": ["b", "a"]}', 'out of order'),
      ('{"words": ["a", "a"]}', 'twice'),
      ('{"words": ["A"]}', 'lowercase'),
      ('{"paths": {"src": {"words": ["b", "a"]}}}', ': src'),
    ):
      with self.subTest(content = content):
        self._write(content)
        with self.assertRaisesRegex(edgspelling.AllowlistError, expected):
          edgspelling.Allowlist(self.tree_path)

  def test_bad_file_is_refused_when_loaded(self) -> None:
    # Even a path no bad node is on fails: the whole file is validated.
    self._write({'paths': {'src': {'words': ['b', 'a']}}})
    with self.assertRaises(edgspelling.AllowlistError):
      self._words('doc/a.rst')

class PreprocessTests(unittest.TestCase):
  def test_splits_camel_case_identifiers(self) -> None:
    for text, expected in (
      ('fooBar', 'foo Bar'),
      ('FooBarBaz', 'Foo Bar Baz'),
      # Abbreviations without vowels are fine as parts.
      ('StmtBlock', 'Stmt Block'),
      ('setUp', 'set Up'),
      ('ABIs', 'AB Is'),
      ('WinCE', 'Win CE'),
      ('TraitMsvcDeclAttrs', 'Trait Msvc Decl Attrs'),
      ('HTTPServer', 'HTTP Server'),
      ('parseJSON2Text', 'parse JSON2 Text'),
      ('foo_barBaz', 'foo_bar Baz'),
      ('see fooBar, then BazQux.', 'see foo Bar, then Baz Qux.'),
    ):
      with self.subTest(text = text):
        self.assertEqual(edgspelling.preprocess(text), expected)

  def test_leaves_other_tokens_whole(self) -> None:
    for text in (
      'plain', 'Capitalized', 'ALLCAPS', 'snake_case',
      # One letter parts, or short parts on average: random looking data.
      'getXValue', 'QaBcDeFgH', 'aBcd', 'xBqzK', 'QaBcDeFgHiJk',
    ):
      with self.subTest(text = text):
        self.assertEqual(edgspelling.preprocess(text), text)

class AllowlistSplitEntryTests(unittest.TestCase):
  def _load(self, tree) -> None:
    with tempfile.TemporaryDirectory() as tmp_dir_str:
      path = Path(tmp_dir_str) / 'allowlist.json'
      path.write_text(json.dumps(tree), encoding = 'utf-8')
      edgspelling.Allowlist(path)

  def test_rejects_entries_that_preprocessing_splits(self) -> None:
    # "words" must be lowercase, so only an underscore can split one.
    for key, entry in (
      ('words', 'foo_bar'),
      ('names', 'fooBar'), ('names', 'HTTPServer'), ('names', 'foo_bar'),
    ):
      with self.subTest(key = key, entry = entry):
        with self.assertRaisesRegex(
          edgspelling.AllowlistError, 'would be split'
        ):
          self._load({key: [entry]})

  def test_accepts_identifiers_preprocessing_leaves_whole(self) -> None:
    self._load({'names': ['getXValue'], 'words': ['plain']})

class ShippedAllowlistTests(unittest.TestCase):
  def test_shipped_file_is_well_formed(self) -> None:
    # Raises AllowlistError if the file is malformed, a list is out of order,
    # or a word isn't lowercase, so a bad change to it fails CI.
    edgspelling.Allowlist()

@unittest.skipUnless(shutil.which('aspell'), 'aspell not installed')
class SpellingIntegrationTests(unittest.TestCase):
  def test_unknown_word_is_reported(self) -> None:
    with tempfile.TemporaryDirectory() as tmp_dir_str:
      result = _run_check(Path(tmp_dir_str), 'a.md', 'zxqvortle\n')
    self.assertIn('zxqvortle', result['misspellings'])

if __name__ == '__main__':
  unittest.main()
