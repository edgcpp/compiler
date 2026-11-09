# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Tests for the commit policy: edgchanges.check_commit_message (shared with
the pre-push hook) and edg-check-commit-messages.

Run with: python3 -m unittest discover -s dev_tools/tests
(with dev_tools/pylibs on PYTHONPATH).
'''

import os
import subprocess
import sys
import tempfile
import unittest

import edgchanges

from pathlib import Path

DEV_TOOLS_DIR = Path(__file__).resolve().parents[1]
MESSAGES_TOOL_PATH = DEV_TOOLS_DIR / 'bin' / 'edg-check-commit-messages'

class CommitMessageTests(unittest.TestCase):
  '''The commit message policy, shared by the pre-push hook and
  edg-check-commit-messages.'''

  def test_valid_messages(self) -> None:
    for message in (
      'Fix thing [GH #213]',
      'Fix thing [EDGcpfe/29032]',
      'Fix bug [EDGcpfe/29055,EDGcpfe/28999]',
      'Fix bug [GH #1, GH #2]',
      'Mixed [nope] and [GH #7]',
      'No issue []',
      'Drop [[always_inline]] on setjmp [GH #176]',
      'Fix thing [GH #1]\n',
      # git revert and Reapply subjects, nested ones included.
      'Revert "Fix thing [GH #1]"\n\nThis reverts commit 0123abcd.',
      'Reapply "Fix thing [EDGcpfe/28806]"',
      'Revert "Revert "Fix thing [GH #1]""',
      'Revert "Fix thing []"',
      'Fix thing [GH #1]\n\nBody mentions [nope]',
      # Trailing newlines (git show adds some) don't change the verdict.
      'Fix thing [GH #1]\n  ',
      'Fix thing [GH #1]\n  \nBody',
    ):
      with self.subTest(message = message):
        self.assertIsNone(edgchanges.check_commit_message(message))
        self.assertIsNone(
          edgchanges.check_commit_message(message.rstrip('\n'))
        )

  def test_invalid_messages(self) -> None:
    for message in (
      '',
      'No tag at all',
      'Bad ref [GH 213]',
      'Comma junk [,,]',
      'Run together [GH #1GH #2]',
      'Unclosed [GH #1',
      'Last wins [GH #7] but [nope]',
      'Tag not at the end [GH #1] of the subject',
      'Wrapped subject\n[GH #1]',
      # Trailing whitespace on the subject isn't tolerated.
      'Fix thing [GH #1] ',
      'Fix thing [GH #1] \n\nBody',
      'Revert "Fix thing [GH #1]" ',
      'Revert "Fix thing"',
      'Revert "Fix thing [GH 1]"',
      'Revert "Fix thing [GH #1] and more"',
      'Wrapped subject [GH #1]\ncontinued',
      'No tag here\n\nBut the body has [GH #1]',
      # Anything but \n that splitlines treats as a line break, anywhere.
      'Fix thing [GH #1]\r\n\r\nBody',
      'Fix thing [GH #1]\n\nBody\rmore',
      'Fix\x0cthing [GH #1]',
      'Fix thing [GH #1]\n\nBody\u2028more',
    ):
      with self.subTest(message = message):
        self.assertIsNotNone(edgchanges.check_commit_message(message))
        self.assertIsNotNone(
          edgchanges.check_commit_message(message.rstrip('\n'))
        )

  def test_tool_reports_only_violations(self) -> None:
    with tempfile.TemporaryDirectory() as tmp_dir_str:
      git = ['git', '-C', tmp_dir_str]
      subprocess.run(git + ['init', '-q'], check = True)
      for subject in ('Good [GH #1]', 'Bad one', 'Also good []\n\nBody'):
        subprocess.run(
          git + ['-c', 'user.name=t', '-c', 'user.email=t@example.com',
                 'commit', '-q', '--allow-empty', '-m', subject],
          check = True
        )
      def run_tool(*args: str) -> 'subprocess.CompletedProcess[str]':
        return subprocess.run(
          [sys.executable, str(MESSAGES_TOOL_PATH), *args],
          cwd = tmp_dir_str,
          capture_output = True,
          text = True,
          env = dict(os.environ, PYTHONPATH = str(DEV_TOOLS_DIR / 'pylibs'))
        )

      result = run_tool('HEAD')
      self.assertEqual(result.returncode, 1)
      self.assertIn('Bad one', result.stdout)
      self.assertNotIn('Good', result.stdout)

      result = run_tool('HEAD~1..HEAD')
      self.assertEqual(result.returncode, 0)
      self.assertEqual(result.stdout, '')

      # A merge commit is reported even with a good subject, unless merges
      # are allowed.
      commit = ['-c', 'user.name=t', '-c', 'user.email=t@example.com']
      subprocess.run(git + ['checkout', '-q', '-b', 'side', 'HEAD~1'],
                     check = True)
      subprocess.run(
        git + commit + ['commit', '-q', '--allow-empty', '-m', 'Side [GH #2]'],
        check = True
      )
      subprocess.run(git + ['checkout', '-q', '-'], check = True)
      subprocess.run(
        git + commit + ['merge', '-q', '--no-ff', 'side',
                        '-m', 'Merge side [GH #2]'],
        check = True
      )
      result = run_tool('HEAD~1..HEAD')
      self.assertEqual(result.returncode, 1)
      self.assertIn('Merge side', result.stdout)
      self.assertIn('merge commits are not allowed', result.stdout)

      result = run_tool('--allow-merges', 'HEAD~1..HEAD')
      self.assertEqual(result.returncode, 0)
      self.assertEqual(result.stdout, '')

  def test_tool_hostile_messages(self) -> None:
    '''Messages containing the characters a naive parser might use as
    delimiters can't hide a violation or break the tool.'''
    with tempfile.TemporaryDirectory() as tmp_dir_str:
      git = ['git', '-C', tmp_dir_str]
      subprocess.run(git + ['init', '-q'], check = True)
      tree = subprocess.run(
        git + ['hash-object', '-t', 'tree', '-w', '--stdin'],
        input = b'', capture_output = True, check = True
      ).stdout.decode().strip()
      ident = b'author t <t@e.com> 0 +0000\ncommitter t <t@e.com> 0 +0000\n'
      parent = ''
      for message in (
        b'Wrapped [GH #1]\x01\ncontinued',
        b'Fake\x01deadbeef\x00\x00Good [GH #1]\x01',
        b'Embedded NUL [GH #1]\n\nbody\x00\x01more',
      ):
        content = b'tree ' + tree.encode() + b'\n'
        if parent != '':
          content += b'parent ' + parent.encode() + b'\n'
        content += ident + b'\n' + message
        parent = subprocess.run(
          [
            *git,
            'hash-object',
            '-t',
            'commit',
            '-w',
            '--literally',
            '--stdin'
          ],
          input = content, capture_output = True, check = True
        ).stdout.decode().strip()
      result = subprocess.run(
        [sys.executable, str(MESSAGES_TOOL_PATH), parent],
        cwd = tmp_dir_str,
        capture_output = True,
        text = True,
        env = dict(os.environ, PYTHONPATH = str(DEV_TOOLS_DIR / 'pylibs'))
      )
      self.assertEqual(result.returncode, 1)
      self.assertEqual(result.stderr, '')
      # Only the first and second commits violate the policy (the wrapped
      # subject, and the fake-record one that doesn't end in a tag).
      self.assertEqual(result.stdout.count('\n  '), 2)
      self.assertIn('single line', result.stdout)
