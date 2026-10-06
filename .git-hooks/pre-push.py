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
# All of the policy itself lives in edg-check-policy, which CI and users at a
# shell run too.  This hook only arranges for a usable terminal and then asks
# that tool to review everything that has not been pushed yet.
#

import platform
import shutil
import subprocess
import sys

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

TOOL_PATH_CHECK_POLICY = shutil.which('edg-check-policy')

def terminal_input():
  '''Open the terminal to hand to the checker as its standard input.

  git supplies the refs being pushed on this hook's stdin, so the checker
  cannot simply inherit it and still read the user's answers.  On Windows
  the relaunch above has already arranged a standalone console, whose stdin
  is usable as it is, so that is inherited instead.
  '''
  if platform.system() == 'Windows':
    return None

  try:
    return open('/dev/tty')
  except OSError:
    # Pushing with no terminal attached, so there is nobody to prompt.  The
    # checker reports its findings without asking rather than reading the
    # refs above as if they were answers.
    return subprocess.DEVNULL

def main():
  if TOOL_PATH_CHECK_POLICY is None:
    print(
      'edg-check-policy tool is required, but was not found.\n'
      'Has dev-init.py been run, and is dev_tools/bin on PATH?',
      file = sys.stderr
    )
    sys.exit(1)

  completed_process = subprocess.run(
    [
      TOOL_PATH_CHECK_POLICY,
      '--unpushed',
      '--interactive'
    ],
    stdin = terminal_input()
  )

  sys.exit(completed_process.returncode)

main()
