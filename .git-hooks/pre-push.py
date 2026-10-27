#!/usr/bin/env python3

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#
# This hook is called by git-push[1] and can be used to prevent a push from
# taking place. The hook is called with two parameters which provide the name
# and location of the destination remote, if a named remote is not being used
# both values will be the same.
#
# Information about what is to be pushed is provided on the hook's standard
# input with lines of the form:
#
# <local ref> SP <local object name> SP <remote ref> SP <remote object name> LF
#
# For instance, if the command git push origin master:foreign were run the hook
# would receive a line like the following:
#
# refs/heads/master 67890 refs/heads/foreign 12345
#
# although the full object name would be supplied. If the foreign ref does not
# yet exist the <remote object name> will be the all-zeroes object name. If a
# ref is to be deleted, the <local ref> will be supplied as (delete) and the
# <local object name> will be the all-zeroes object name. If the local commit
# was specified by something other than a name which could be expanded (such as
# HEAD~, or an object name) it will be supplied as it was originally given.
#
# If this hook exits with a non-zero status, git push will abort without
# pushing anything. Information about why the push is rejected may be sent to
# the user by writing to standard error.
#

import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

# Git invokes this hook with the ambient environment, which does not
# necessarily have PYTHONPATH configured (e.g., on Windows).  Locate
# "dev_tools/pylibs" relative to this script so "edgutil" can be imported.
sys.path.insert(
  0, str(Path(__file__).resolve().parents[1] / 'dev_tools' / 'pylibs')
)

import edgchanges
import edgutil

if platform.system() == 'Windows':
  # The standard input of this hook carries the push ref list supplied by
  # git, so it cannot be used to prompt the user.  When a console is
  # attached, open the console input device directly ("CONIN$" is the
  # Windows counterpart of "/dev/tty") so that all prompts and output stay
  # in the user's current terminal.
  try:
    sys.stdin = open('CONIN$', 'r')
  except OSError:
    # No console is attached (e.g., a GUI git client or a pseudo terminal
    # such as mintty).  Relaunch in a standalone console window so that the
    # user can still respond to interactive prompts.
    delegate_proc = subprocess.run(
      [
        sys.executable,
        __file__
      ],
      creationflags = subprocess.CREATE_NEW_CONSOLE,
      env = {**os.environ, 'EDG_PREPUSH_DELEGATE': '1'}
    )
    sys.exit(delegate_proc.returncode)
else:
  # On Unix-like systems we can regain access to terminal input using /dev/tty.
  sys.stdin = open('/dev/tty')


#
# File paths that begin with the prefixes listed here are excluded from file
# level checking.
#
EXCLUDES_PATH_PREFIXES = [
  # Don't check tests as they're not necessarily "EDG-style".
  'tests/',
  # Don't check the generated builtin files as they're very large and there's
  # really nothing the user can do but accept them.
  'dev_tools/builtins/builtins_',
  # Don't check GitHub workflow yml files; long lines are all but required.
  '.github/workflows/',
  # Don't check the error_msg.txt file.
  'src/error_msg.txt',
  # Don't check AGENT files.
  'AGENTS.md',
  '.agents/'
]

def _is_excluded_file_path(path_str: str) -> bool:
  return any(
    path_str.startswith(prefix) for prefix in EXCLUDES_PATH_PREFIXES
  )

TOOL_PATH_GIT = shutil.which('git')

def run_git(args, *, check = True, text = True):
  '''Run a git command and return the completed process.'''
  return subprocess.run(
    [TOOL_PATH_GIT, *args],
    check = check,
    capture_output = True,
    text = text
  )

def git_output(args):
  '''Run a git command and return stripped stdout.'''
  return run_git(args).stdout.strip()

def print_indented(text, prefix):
  '''Print each line of text with the given prefix.'''
  if not text:
    return

  for line in text.splitlines():
    print(f"{prefix}{line}")

def git_show_blob(revision_path: str) -> bytes:
  '''Return the blob contents for revision:path, or b'' if missing.'''
  completed_process = run_git(
    ['show', revision_path], check = False, text = False
  )
  if completed_process.returncode != 0:
    return b''
  return completed_process.stdout

class PrePushChecker:
  '''Runs EDG pre-push policy checks against new commits and changed files.'''

  def __init__(self):
    self.errors = 0
    self.abort = False

  def confirm_okay(self):
    '''Ask the user whether a policy warning is acceptable.

    When stdout is a terminal, prompt on /dev/tty for y/n/q. Otherwise record
    an error without prompting (e.g. non-interactive pushes).
    '''
    if not sys.stdout.isatty():
      self.errors = 1
      return

    tries = 0
    while True:
      response = input('    Okay? [ynq] ').lower()

      if response == 'y':
        return
      if response == 'n':
        self.errors = 1
        return
      if response == 'q':
        self.errors = 1
        self.abort = True
        return

      print(
        "Please answer 'y' (to accept), 'n' (to continue check) or "
        "'q' (to quit)."
      )
      tries += 1
      if tries >= 10:
        print('Too many attempts.')
        self.errors = 1
        self.abort = True
        return

  def reject(self):
    '''Record a problem that can't be accepted, without asking.'''
    print('    Not allowed: CI would reject this; fix it and push again.')
    self.errors = 1

  def check_commits(self):
    '''Walk commits until one already on a remote branch is found.

    As commits are walked, check to make sure they match formatting guidance.

    Returns the oldest new commit hash, or None if there are no new commits
    (or if a commit subject fails the PR-number policy, in which case an
    error is recorded and the check sequence is aborted).
    '''
    first_new_commit = None

    commits = git_output(['log', '--pretty=%H']).splitlines()
    for commit in commits:
      # Break if the remote already has this commit.
      contains = run_git(
        ['branch', '-r', '--contains', commit], check = False
      ).stdout.strip()
      if contains:
        break

      print('New commit:')
      print('')
      show_output = git_output(['show', '--no-patch', commit])
      print_indented(show_output, '  ')
      print('')

      # Read bytes: text mode would turn any \r into \n, hiding it from the
      # check.  Don't strip: that would also remove \r, \x0c, etc. at the ends
      # of the message.
      message = run_git(
        ['show', '--no-patch', '--format=%B', commit], text = False
      ).stdout.decode('utf-8', errors = 'replace')
      problem = edgchanges.check_commit_message(message)
      if problem is not None:
        print(f"  [POLICY] Commit message: {problem}.")
        self.errors = 1
        self.abort = True
        return None

      first_new_commit = commit

    return first_new_commit

  def check_changed_files(self, first_new_commit):
    '''Run per-file policy checks for files changed since
    first_new_commit~1.'''
    remote_head = f"{first_new_commit}~1"
    print('Checking changed files:')

    changed_files = git_output(
      ['diff', '--no-commit-id', '--name-only', '-r', f"{remote_head}..HEAD"]
    ).splitlines()

    mono_repo_dir = edgutil.find_mono_repo_or_exit()
    containing_tmp_dir = mono_repo_dir / '.tmp'

    with tempfile.TemporaryDirectory(dir = containing_tmp_dir) as tmp_dir_str:
      input_spec = {}
      for file_path_str in changed_files:
        if _is_excluded_file_path(file_path_str):
          continue

        file_path = Path(file_path_str)
        named_tmp_args = {
          'prefix': file_path.stem,
          'suffix': file_path.suffix,
          'dir': tmp_dir_str,
          'delete': False
        }
        with tempfile.NamedTemporaryFile(**named_tmp_args) as file_handle:
          file_handle.write(git_show_blob(f"HEAD:{file_path_str}"))
          new_file = (
            Path(file_handle.name).relative_to(mono_repo_dir).as_posix()
          )
        with tempfile.NamedTemporaryFile(**named_tmp_args) as file_handle:
          file_handle.write(git_show_blob(f"{remote_head}:{file_path_str}"))
          old_file = (
            Path(file_handle.name).relative_to(mono_repo_dir).as_posix()
          )

        input_spec[file_path_str] = [old_file, new_file]

      tmp_dir = Path(tmp_dir_str)
      input_file = tmp_dir / 'input.json'
      output_file = tmp_dir / 'output.json'
      with open(input_file, 'w') as file_handle:
        json.dump(input_spec, file_handle)

      use_docker = (
        shutil.which('docker') is not None and
        edgutil.is_docker_preferred(mono_repo_dir)
      )
      if use_docker:
        file_change_check_proc = subprocess.run(
          [
            shutil.which('edg-exec'),
            '--',
            'edg-check-file-changes',
            str(input_file.relative_to(mono_repo_dir).as_posix()),
            str(output_file.relative_to(mono_repo_dir).as_posix())
          ],
          cwd = mono_repo_dir,
          stdin = subprocess.DEVNULL
        )
      else:
        file_change_check_proc = subprocess.run(
          [
            shutil.which('edg-check-file-changes'),
            str(input_file),
            str(output_file)
          ],
          cwd = mono_repo_dir,
          stdin = subprocess.DEVNULL
        )

      if file_change_check_proc.returncode != 0:
        # Record the failure and return (rather than exiting) so that the
        # status propagates through checker.run() and main(), allowing the
        # delegate window pause to run.
        self.errors = 1
        return

      with open(output_file, 'r') as file_handle:
        results = json.load(file_handle)

      for file_path_str, file_results in results.items():
        if self.abort:
          break
        print(f"  Checking file: {file_path_str}")

        # A pending Changes entry with any of these problems fails the
        # changes-fragments CI check, so there's no accepting them here.
        is_changes_entry = edgchanges.is_fragment_path(file_path_str)
        confirm_okay = (
          self.reject if is_changes_entry else self.confirm_okay
        )

        format_errors = [
          edgchanges.FormatError(*error)
          for error in file_results['format_errors']
        ]
        if len(format_errors) != 0:
          print('    Badly formatted Changes entry:')
          for error in format_errors:
            print(f"    - {edgchanges.describe_error(file_path_str, error)}")
          confirm_okay()
          if self.abort:
            break

        misspellings = file_results['misspellings']
        if misspellings is None:
          print('    Spell check failed:')
          if not use_docker:
            print('      docker not used.')
          print('      aspell not found or missing dictionary.')
        elif len(misspellings) != 0:
          print('    New misspelled words:')
          for misspelling in misspellings:
            print(f"    - {misspelling}")
          if is_changes_entry:
            hint_lines = edgchanges.allowed_words_hint(
              file_path_str,
              misspellings
            )
            print_indented('\n'.join(hint_lines), '    ')
          confirm_okay()
          if self.abort:
            break

        typos = file_results['typos']
        if len(typos) != 0:
          print('    New typos:')
          for typo in typos:
            print(f"    - {typo}")
          confirm_okay()
          if self.abort:
            break

        coding_errors = file_results['coding_errors']
        if len(coding_errors) != 0:
          endif_hits = coding_errors['missing_endifs']
          if len(endif_hits) != 0:
            print(f"    Missing comment on #endif:")
            print_indented('\n'.join(endif_hits), '    ')
            self.confirm_okay()
            if self.abort:
              break

          else_hits = coding_errors['missing_elses']
          if len(else_hits) != 0:
            print(f"    Missing comment on #else:")
            print_indented('\n'.join(else_hits), '    ')
            self.confirm_okay()
            if self.abort:
              break

          closing_comments = coding_errors['closing_comments']
          if len(closing_comments) != 0:
            print_indented('\n'.join(closing_comments), '    ')
            self.confirm_okay()
            if self.abort:
              break

        overlong_lines = file_results['overlong_lines']
        if len(overlong_lines) != 0:
          for line in edgutil.format_overlong_lines(overlong_lines, '    '):
            print(line)
          confirm_okay()
          if self.abort:
            break

  def run(self) -> int:
    '''Execute the full pre-push check sequence.  Return the exit status.'''
    first_new_commit = self.check_commits()

    # Skip per file checks if we didn't actually detect a new commit.
    if not self.abort and first_new_commit is not None:
      self.check_changed_files(first_new_commit)

    return 0 if (self.errors == 0 and not self.abort) else 1

def pause_delegate_window() -> None:
  '''When running as a relaunch delegate, wait for the user before the
  standalone console window closes so that failure output can be read.'''
  if os.environ.get('EDG_PREPUSH_DELEGATE') != '1':
    return

  try:
    input('Push checks failed; press Enter to close this window. ')
  except EOFError:
    pass

def main():
  checker = PrePushChecker()
  returncode = checker.run()
  if returncode != 0:
    pause_delegate_window()
  sys.exit(returncode)

main()
