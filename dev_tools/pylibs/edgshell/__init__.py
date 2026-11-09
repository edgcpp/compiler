# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# A library providing an interface to various process provided options.
#
# With respect to compilers, if the EDG front end can perform the operation it
# is preferred, otherwise another compiler (potentially a remote one), is used.

import os
import platform
import re
import subprocess
import tempfile
import uuid

import edgutil

from pathlib import Path, PurePath
from typing import Any, List, Optional, Set

TOOL_PATH_GIT = edgutil.RequiredToolPath('git')

def read_process_output(args: List[str], *, cwd = None,
                        stderr = subprocess.STDOUT) -> str:
  '''A simplified function for running a process and grabbing the output.

  By default, the output includes what the process writes to standard error.
  Pass another value (e.g., subprocess.DEVNULL) to keep it out of the output
  when the output is going to be parsed.'''
  completed_process = subprocess.run(
    args,
    stdout = subprocess.PIPE,
    stderr = stderr,
    cwd = cwd,
    check = True
  )
  return completed_process.stdout.decode()

def preprocess(source_file_path: Path, preprocessed_file_path: Path, *,
               compiler_options: List[str],
               stderr = subprocess.DEVNULL) -> None:
  with open(preprocessed_file_path, 'w') as preprocessed_file_handle:
    process = subprocess.run(
      ['eccp', *compiler_options, '-E', str(source_file_path)],
      stdout = preprocessed_file_handle,
      stderr = stderr
    )
    process.check_returncode()

# def generate_predefined_macros(*, script_path: Path) -> str:
#   '''Run the make_predef_macro_table script with the default compiler for the
#   current operating system and return the output. As this is not normally in
#   the path, providing the script path is required.
#   '''
#   with tempfile.TemporaryDirectory() as tmp_dir:
#     with edgutil.CurrentWorkingDirectorySwap(tmp_dir.name):
#       cmd_args = [script_path]
#       if platform.name() == 'Darwin':
#         cmd_args.extend(['--clang', 'clang'])
#       process = subprocess.run(cmd_args)
#       process.check_returncode()
#       with open('predefined_macros.txt', 'r') as tmp_file:
#         return tmp_file.read()

def generate_ifcspec(ifc_file_path: Path, *,
                     stderr = subprocess.DEVNULL) -> None:
  # gen-ifcspec at the time of writing requires the current working directory
  # to be the containing folder for it to function properly.
  with edgutil.CurrentWorkingDirectorySwap(ifc_file_path.parent):
    process = subprocess.run(
      ['gen-ifcspec', ifc_file_path],
      stderr = stderr
    )
    process.check_returncode()

def is_dynamic_executable(exec_path: PurePath) -> bool:
  completed_process = subprocess.run(
    ['ldd', str(exec_path)],
    stdout = subprocess.DEVNULL,
    stderr = subprocess.DEVNULL
  )
  return completed_process.returncode == 0

def has_architectures(exec_path: PurePath, *architectures: str) -> bool:
  process_output = read_process_output(
    ['file', str(exec_path)]
  )
  for architecture in architectures:
    if f"(for architecture {architecture})" not in process_output:
      return False
  return True

def is_git_version_at_least(major_ver: int, minor_ver: int) -> bool:
  git_proc = subprocess.run(
    [str(TOOL_PATH_GIT), '--version'],
    stdout = subprocess.PIPE,
    stderr = subprocess.STDOUT
  )
  git_version_match = re.search(
    r'([0-9]+)\.([0-9]+)\.([0-9]+)',
    git_proc.stdout.decode()
  )

  # Version detection failed, assume no match.
  if git_version_match is None:
    return False

  git_ver_tuple = (
    int(git_version_match.group(1)),
    int(git_version_match.group(2))
  )
  return git_ver_tuple >= (major_ver, minor_ver)

def has_local_git_changes(*, cwd: Optional[Path] = None) -> bool:
  '''Return TRUE if there are non-committed changes.'''
  output = read_process_output(
    [
      str(TOOL_PATH_GIT),
      'status',
      '--porcelain'
    ],
    cwd = cwd
  )
  return len(output) > 0

def get_git_summary(commit: str, *, cwd: Optional[Path] = None) -> str:
  '''Read and return the summary of the given git commit.'''
  return read_process_output(
    [
      str(TOOL_PATH_GIT),
      'show',
      '--summary',
      commit
    ],
    cwd = cwd
  )

def has_git_commit_sha(commit: str, *, cwd: Optional[Path] = None) -> bool:
  '''Given a commit string (potentially something like "HEAD"), use git to
  determine if the commit exists in the current branch.
  '''
  process = subprocess.run(
    [
      str(TOOL_PATH_GIT),
      'branch',
      '--contains',
      commit
    ],
    stdout = subprocess.DEVNULL,
    stderr = subprocess.DEVNULL,
    cwd = cwd
  )
  return process.returncode == 0

def get_git_commit_sha(commit: str, *, cwd: Optional[Path] = None) -> str:
  '''Given a commit string (potentially something like "HEAD"), use git to
  resolve the exact commit SHA.
  '''
  return read_process_output(
    [
      str(TOOL_PATH_GIT),
      'rev-parse',
      commit
    ],
    cwd = cwd
  ).strip()

def get_git_commit_date(commit: str, *, cwd: Optional[Path] = None) -> str:
  '''Given a commit string (potentially something like "HEAD"), use git to
  resolve the date the commit was added to the git (as opposed to the date the
  commit was originally authored).
  '''
  return read_process_output(
    [
      str(TOOL_PATH_GIT),
      'show',
      '--no-patch',
      '--format=%cI',
      commit
    ],
    cwd = cwd
  ).strip()

def get_current_git_commit(*, cwd: Optional[Path] = None) -> str:
  '''Return the SHA identifying the current git HEAD commit.'''
  return get_git_commit_sha('HEAD', cwd = cwd)

def get_git_commit_subject(commit: str, *, cwd: Optional[Path] = None) -> str:
  '''Read and return the subject of the given git commit.'''
  return read_process_output(
    [
      str(TOOL_PATH_GIT),
      'show',
      '--no-patch',
      '--format=%s',
      commit
    ],
    cwd = cwd
  ).strip()

def make_user_git_commit(message: str, *,
                         author_name: Optional[str],
                         author_email: Optional[str],
                         committer_name: Optional[str],
                         committer_email: Optional[str],
                         cwd: Optional[Path] = None,
                         date: Optional[str] = None) -> None:
  subprocess.run(
    [
      str(TOOL_PATH_GIT),
      'add',
      '-A'
    ],
    stdout = subprocess.DEVNULL,
    cwd = cwd,
    check = True
  )
  commit_message_args = [
    str(TOOL_PATH_GIT),
    'commit'
  ]
  commit_message_env = {}
  if author_name:
    commit_message_env['GIT_AUTHOR_NAME'] = author_name
  if author_email:
    commit_message_env['GIT_AUTHOR_EMAIL'] = author_email
  if committer_name:
    commit_message_env['GIT_COMMITTER_NAME'] = committer_name
  if committer_email:
    commit_message_env['GIT_COMMITTER_EMAIL'] = committer_email
  if date:
    commit_message_args.extend([
      '--date',
      date
    ])
    commit_message_env['GIT_COMMITTER_DATE'] = date
  commit_message_args.extend([
    '-m',
    message
  ])
  subprocess.run(
    commit_message_args,
    stdout = subprocess.DEVNULL,
    env = commit_message_env,
    cwd = cwd,
    check = True
  )

def make_system_git_commit(message: str, *,
                           cwd: Optional[Path] = None,
                           date: Optional[str] = None) -> None:
  make_user_git_commit(
    message,
    cwd = cwd,
    date = date,
    committer_name = 'Edison Design Group',
    author_name = 'Edison Design Group',
    committer_email = 'info@edg.com',
    author_email = 'info@edg.com'
  )

def get_current_git_branch() -> str:
  '''Return the name of the current git branch.'''
  branches = read_process_output(
    [
      str(TOOL_PATH_GIT),
      'branch'
    ]
  )
  result = re.search(r'^\*\s+(.+)$', branches, re.MULTILINE)
  assert result is not None
  return result.group(1)

class TmpGitBranchInstance:
  def reset_to_commit(self, commit: str) -> None:
    subprocess.run(
      [
        str(TOOL_PATH_GIT),
        'reset',
        '--hard',
        commit
      ],
      stdout = subprocess.DEVNULL,
      check = True
    )

class TmpGitBranch:
  '''A class for creating a temporary git branch. The previous branch will be
  restored upon exit.
  '''

  def __init__(self) -> None:
    self.original_branch = get_current_git_branch()
    self.tmp_branch_name = f"tmp/{uuid.uuid4()}"

  def __enter__(self) -> 'TmpGitBranchInstance':
    subprocess.run(
      [
        str(TOOL_PATH_GIT),
        'checkout',
        '-b',
        self.tmp_branch_name
      ],
      stdout = subprocess.DEVNULL,
      stderr = subprocess.DEVNULL,
      check = True
    )
    return TmpGitBranchInstance()

  def __exit__(self, _type: Any, _value: Any, _tb: Any) -> None:
    subprocess.run(
      [
        str(TOOL_PATH_GIT),
        'checkout',
        self.original_branch
      ],
      stdout = subprocess.DEVNULL,
      stderr = subprocess.DEVNULL,
      check = True
    )
    subprocess.run(
      [
        str(TOOL_PATH_GIT),
        'branch',
        '-D',
        self.tmp_branch_name
      ],
      stdout = subprocess.DEVNULL,
      stderr = subprocess.DEVNULL,
      check = True
    )

def list_files_in_git_revision(commit: str, *,
                               cwd: Optional[Path] = None) -> List[str]:
  output = read_process_output(
    [
      str(TOOL_PATH_GIT),
      'ls-tree',
      '-r',
      '--name-only',
      commit
    ],
    cwd = cwd
  )
  file_list = output.split('\n')
  file_list = [f for f in file_list if len(f) > 0]
  return file_list

def list_binary_files_in_git_diff(revision_range: str, *,
                                  cwd: Optional[Path] = None) -> Set[str]:
  '''Return the paths git treats as binary in the diff of revision_range.

  Git decides by the content of the file (a NUL byte near its start) and by
  the .gitattributes of its path.  A file git treats as binary has "-" for
  both of its counts in "git diff --numstat".

  This is a best effort to avoid checking files that have no text.  If git
  fails, return the empty set (quietly) rather than failing the caller.
  '''
  try:
    output = read_process_output(
      [
        str(TOOL_PATH_GIT),
        'diff',
        '--numstat',
        '-z',
        '--no-renames',
        revision_range
      ],
      cwd = cwd,
      # Diagnostics (e.g., about .gitattributes) would corrupt the records.
      stderr = subprocess.DEVNULL
    )
  except subprocess.CalledProcessError:
    return set()
  binary_files = set()
  for entry in output.split('\0'):
    added, _, rest = entry.partition('\t')
    deleted, _, path = rest.partition('\t')
    if added == '-' and deleted == '-' and path:
      binary_files.add(path)
  return binary_files

def list_tags_in_repo(*, cwd: Optional[Path] = None) -> List[str]:
  output = read_process_output(
    [
      str(TOOL_PATH_GIT),
      'tag'
    ],
    cwd = cwd
  )
  tag_list = output.split('\n')
  tag_list = [f for f in tag_list if len(f) > 0]
  return tag_list

def list_commits_for_issues(issue_numbers: List[str]) -> List[str]:
  # Form a grep expression that matches all the PR numbers
  issue_numbers_grep_exp = '\\|'.join(issue_numbers)
  # Execute the command to find the matching commits from git
  output = read_process_output(
    [
      str(TOOL_PATH_GIT),
      'log',
      'origin/master',
      f"--grep={issue_numbers_grep_exp}",
      '--format=format:%H',
      '--reverse'
    ]
  )
  commit_list = output.split('\n')
  commit_list = [f for f in commit_list if len(f) > 0]
  return commit_list

def list_commits_since_commit(commit: Optional[str], *,
                              cwd: Optional[Path] = None) -> List[str]:
  '''Given a commit string (potentially something like "HEAD" or None if
  starting from the beginning), use git to return the list of commits since the
  given commit was made on the current branch.
  '''
  output = read_process_output(
    [
      str(TOOL_PATH_GIT),
      'log',
      'origin/master',
      '--format=format:%H',
      '--reverse',
      *([f"{commit}..HEAD"] if commit else [])
    ],
    cwd = cwd
  )
  commit_list = output.split('\n')
  commit_list = [f for f in commit_list if len(f) > 0]
  return commit_list

def run_git_restore(path_specs: List[str], commit: str, *, cwd = None) -> None:
  # Windows has a maximum argument length; the pathspec is thus provided
  # in a file to avoid hitting platform limitations when running on Windows.
  #
  # This same code is reused on all platforms simply to share code (and as
  # the overhead of a file vs command line arguments is minimal).
  with tempfile.TemporaryDirectory() as dir_name:
    temp_file_path = Path(dir_name, 'file_list.txt').resolve()
    with open(temp_file_path, 'w') as temp_file:
      for path_spec in path_specs:
        temp_file.write(f"{path_spec}\n")
    subprocess.run(
      [
        str(TOOL_PATH_GIT),
        'restore',
        '--source',
        commit,
        f"--pathspec-from-file={temp_file_path}"
      ],
      stdout = subprocess.DEVNULL,
      stderr = subprocess.DEVNULL,
      cwd = cwd,
      check = True
    )

def run_git_restore_src_files(commit: str, *, cwd = None) -> None:
  run_git_restore(
    [
      ':/src/*.c',
      ':/src/*.cpp',
      ':/src/*.h',
      ':/src/error_msg.txt'
    ],
    commit,
    cwd = cwd
  )
