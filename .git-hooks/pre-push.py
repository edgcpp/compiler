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
# The policy this hook applies is defined in the edgpolicy library, which the
# policy CI workflow also uses.  This hook exists to give fast local feedback;
# CI is what actually enforces the policy.
#

import platform
import subprocess
import sys
import tempfile

import edgpolicy
import edgutil

from pathlib import Path

if platform.system() == 'Windows':
  # On Windows we need to use the kernel API to detect if this script is
  # running in a standalone console window (if so, there will be only one
  # processes at this point).
  import ctypes

  kernel32 = ctypes.WinDLL('kernel32', use_last_error = True)
  process_array = (ctypes.c_uint * 2)()
  num_processes = kernel32.GetConsoleProcessList(process_array, 2)
  if num_processes != 1:
    # When not running in a standalone console window, the script relaunches
    # itself to make sure the user has an interactive shell.  Then it forwards
    # the exit code from the delegate process.
    delegate_proc = subprocess.run(
      [
        sys.executable,
        __file__
      ],
      creationflags = subprocess.CREATE_NEW_CONSOLE
    )
    sys.exit(delegate_proc.returncode)
else:
  # On Unix-like systems we can regain access to terminal input using /dev/tty.
  sys.stdin = open('/dev/tty')

#
# How each kind of finding reported by edgpolicy is introduced to the user.
#
FINDING_KIND_HEADINGS = {
  'misspelling': 'New misspelled words:',
  'typo': 'New typos:',
  'coding-error': 'Coding errors:',
  'overlong-line': 'Lines too long:'
}

def print_indented(text, prefix):
  '''Print each line of text with the given prefix.'''
  if not text:
    return

  for line in text.splitlines():
    print(f"{prefix}{line}")

class PrePushChecker:
  '''Runs EDG pre-push policy checks against new commits and changed files.'''

  def __init__(self):
    self.errors = 0
    self.repository = edgpolicy.GitRepository(edgutil.find_mono_repo_or_exit())

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
        sys.exit(1)

      print(
        "Please answer 'y' (to accept), 'n' (to continue check) or "
        "'q' (to quit)."
      )
      tries += 1
      if tries >= 10:
        print('Too many attempts.')
        sys.exit(1)

  def check_commits(self):
    '''Walk commits until one already on a remote branch is found.

    As commits are walked, check to make sure they match formatting guidance.

    Returns the oldest new commit hash, or None if there are no new commits.
    A commit subject that fails the policy aborts the push outright when
    edgpolicy says the rule is binding, and is otherwise offered for
    confirmation like any other finding.
    '''
    first_new_commit = None

    commits = self.repository.lines(['log', '--pretty=%H'])
    for commit in commits:
      # Break if the remote already has this commit.
      contains = self.repository.run(
        ['branch', '-r', '--contains', commit], check = False
      ).stdout.strip()
      if contains:
        break

      print('New commit:')
      print('')
      show_output = self.repository.output(['show', '--no-patch', commit])
      print_indented(show_output, '  ')
      print('')

      subject_error = edgpolicy.commit_subject_error(
        self.repository.subject_of(commit)
      )
      if subject_error is not None:
        print(f"  [POLICY] {subject_error}")
        print_indented(edgpolicy.SUBJECT_POLICY_DESCRIPTION, '  ')
        if edgpolicy.ENFORCE_COMMIT_SUBJECT_POLICY:
          sys.exit(1)
        self.confirm_okay()

      first_new_commit = commit

    return first_new_commit

  def report_findings(self, findings):
    '''Print findings grouped by kind, confirming once per group.'''
    for kind, heading in FINDING_KIND_HEADINGS.items():
      messages = [
        message for finding_kind, message in findings
        if finding_kind == kind
      ]
      if not messages:
        continue

      print(f"    {heading}")
      for message in messages:
        print(f"    - {message}")
      self.confirm_okay()

  def check_changed_files(self, first_new_commit):
    '''Run per-file policy checks for files changed since
    first_new_commit~1.'''
    base_rev = f"{first_new_commit}~1"
    print('Checking changed files:')

    containing_tmp_dir = self.repository.repo_dir / '.tmp'
    containing_tmp_dir.mkdir(exist_ok = True)

    with tempfile.TemporaryDirectory(
      dir = str(containing_tmp_dir)
    ) as tmp_dir_str:
      tmp_dir = Path(tmp_dir_str)

      change_spec = edgpolicy.collect_change_spec(
        self.repository, base_rev, 'HEAD', tmp_dir
      )

      use_docker = edgpolicy.should_use_docker(self.repository.repo_dir)
      results = edgpolicy.run_file_checks(
        self.repository, change_spec, tmp_dir, use_docker = use_docker
      )
      if results is None:
        sys.exit(1)

      for file_path_str in sorted(results):
        print(f"  Checking file: {file_path_str}")

        findings = edgpolicy.summarize_file_result(results[file_path_str])
        if any(kind == 'spelling-unavailable' for kind, _ in findings):
          print('    Spell check failed:')
          if not use_docker:
            print('      docker not used.')
          print('      aspell not found or missing dictionary.')

        self.report_findings(findings)

  def run(self) -> bool:
    '''Execute the full pre-push check sequence.'''
    first_new_commit = self.check_commits()

    # Skip per file checks if we didn't actually detect a new commit.
    if first_new_commit is not None:
      self.check_changed_files(first_new_commit)

    return self.errors == 0

def main():
  checker = PrePushChecker()
  sys.exit(0 if checker.run() else 1)

main()
