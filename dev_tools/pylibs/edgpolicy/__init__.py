# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Shared definitions for the project's commit and file change policies.

The pre-push hook (fast local feedback) and the policy CI workflow
(enforcement) both read their rules from here so the two cannot drift
apart.  Nothing in this module prompts, prints, or exits; callers decide
how to report what they are told.
'''

import json
import re
import shutil
import subprocess
import tempfile

from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple

import edgutil

#
# File paths that begin with the prefixes listed here are excluded from file
# level checking.
#
EXCLUDED_PATH_PREFIXES = (
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
)

#
# The widest a line may be, measured in columns after tab expansion.
#
MAX_LINE_COLUMNS = 79

#
# A commit subject must carry a bracketed tag naming the work it belongs to.
# A tag holds one or more comma separated references, each of which is either
# a GitHub issue ("GH #123") or a legacy internal PR ("EDGcpfe/29032").
#
SUBJECT_REFERENCE_REGEX = re.compile(r'GH #[0-9]+|EDG[cfjp]+fe/[0-9]+')

BRACKETED_TAG_REGEX = re.compile(r'\[([^\[\]]*)\]')

SUBJECT_POLICY_DESCRIPTION = (
  'Commit subjects must end with a bracketed tag naming at least one '
  'GitHub issue or legacy PR, for example "[GH #213]", '
  '"[EDGcpfe/29032]", or "[GH #213,GH #214]".'
)

#
# Whether a commit subject that names no issue is a hard failure.
#
# Commits that belong to no issue have historically carried an empty "[]"
# to satisfy the rule.  Whether to keep requiring that is an open policy
# question for the maintainers, so the check currently reports without
# failing.  Setting this to True makes it binding again; the pre-push hook
# and the policy CI job both read this one flag, so there is nothing else
# to change.
#
ENFORCE_COMMIT_SUBJECT_POLICY = False

def is_excluded_path(path_str: str) -> bool:
  '''Return True when a repository path is exempt from file level checks.'''
  return any(
    path_str.startswith(prefix) for prefix in EXCLUDED_PATH_PREFIXES
  )

def is_text_blob(data: bytes) -> bool:
  '''Return True when a blob can be checked as UTF-8 text.'''
  if b'\x00' in data:
    return False

  try:
    data.decode('utf-8')
  except UnicodeDecodeError:
    return False

  return True

def _is_reference_list(tag_body: str) -> bool:
  '''Return True when a tag's contents are a comma separated reference
  list holding at least one reference.'''
  if not tag_body.strip():
    return False

  return all(
    SUBJECT_REFERENCE_REGEX.fullmatch(reference.strip()) is not None
    for reference in tag_body.split(',')
  )

def commit_subject_error(subject: str) -> Optional[str]:
  '''Return a description of why a commit subject is rejected, or None.

  An empty tag ("[]") is not a reference list and so does not satisfy the
  policy, even though earlier revisions of the pre-push hook accepted it.
  '''
  tags = BRACKETED_TAG_REGEX.findall(subject)
  if not tags:
    return "Commit subject has no bracketed tag."

  if not any(_is_reference_list(tag) for tag in tags):
    if any(not tag.strip() for tag in tags):
      return "Commit subject has an empty tag ('[]')."
    return "Commit subject's tag is not a valid reference list."

  return None

class GitRepository:
  '''Minimal read-only git helper scoped to one checkout.'''

  def __init__(self, repo_dir: Path) -> None:
    self.repo_dir = repo_dir
    self._tool_path = edgutil.RequiredToolPath('git')

  def run(
    self,
    args: Sequence[str],
    check: bool = True,
    text: bool = True
  ) -> 'subprocess.CompletedProcess[Any]':
    '''Run a git command in the repository and return the result.

    Capture is spelled out rather than using capture_output, and decoding
    with universal_newlines rather than text, because both shorthands
    postdate the Python 3.6 floor this tooling still supports.
    '''
    return subprocess.run(
      [str(self._tool_path), *args],
      cwd = str(self.repo_dir),
      check = check,
      stdout = subprocess.PIPE,
      stderr = subprocess.PIPE,
      universal_newlines = text
    )

  def output(self, args: Sequence[str]) -> str:
    '''Run a git command and return its stripped stdout.'''
    return str(self.run(args).stdout).strip()

  def lines(self, args: Sequence[str]) -> List[str]:
    '''Run a git command and return its stdout split into lines.'''
    return self.output(args).splitlines()

  def show_blob(self, revision_path: str) -> bytes:
    '''Return the blob contents for revision:path, or b'' if missing.'''
    completed_process = self.run(
      ['show', revision_path], check = False, text = False
    )
    if completed_process.returncode != 0:
      return b''
    return bytes(completed_process.stdout)

  def subject_of(self, commit: str) -> str:
    '''Return the subject line of a commit.'''
    return self.output(['show', '--no-patch', '--format=%s', commit])

  def changed_files(self, base_rev: str, head_rev: str) -> List[str]:
    '''Return paths modified between two revisions, ignoring deletions.'''
    return self.lines([
      'diff',
      '--name-only',
      '--diff-filter=d',
      f"{base_rev}..{head_rev}"
    ])

def collect_change_spec(
  repository: GitRepository,
  base_rev: str,
  head_rev: str,
  tmp_dir: Path
) -> Dict[str, List[str]]:
  '''Materialize both sides of every changed file under tmp_dir.

  Returns the mapping that edg-check-file-changes consumes: repository path
  to a [old_file, new_file] pair, each given relative to the repository so
  the checker can run inside or outside a container.

  Binary files are left out.  The checks behind this all read their input
  as UTF-8 text, and handing them an image makes the checker raise part way
  through and drop the file from its own results, which reads as a silent
  pass.  Excluding them here keeps that from happening.
  '''
  change_spec = {}

  for path_str in repository.changed_files(base_rev, head_rev):
    if is_excluded_path(path_str):
      continue

    blobs = [
      repository.show_blob(f"{revision}:{path_str}")
      for revision in (base_rev, head_rev)
    ]
    if not all(is_text_blob(blob) for blob in blobs):
      continue

    file_path = Path(path_str)

    def materialize(data: bytes) -> str:
      '''Write one side of the change to tmp_dir, returning its path.'''
      with tempfile.NamedTemporaryFile(
        prefix = file_path.stem,
        suffix = file_path.suffix,
        dir = str(tmp_dir),
        delete = False
      ) as file_handle:
        file_handle.write(data)
        return (
          Path(file_handle.name).relative_to(repository.repo_dir).as_posix()
        )

    change_spec[path_str] = [materialize(blob) for blob in blobs]

  return change_spec

def run_file_checks(
  repository: GitRepository,
  change_spec: Dict[str, List[str]],
  tmp_dir: Path,
  use_docker: bool = False
) -> Optional[Dict[str, Any]]:
  '''Run edg-check-file-changes over a change spec.

  Returns the checker's results, or None if the checker itself failed.
  '''
  input_file = tmp_dir / 'input.json'
  output_file = tmp_dir / 'output.json'

  with open(input_file, 'w') as file_handle:
    json.dump(change_spec, file_handle)

  def repo_relative(path: Path) -> str:
    return path.relative_to(repository.repo_dir).as_posix()

  if use_docker:
    command = [
      str(edgutil.RequiredToolPath('edg-exec')),
      '--',
      'edg-check-file-changes',
      repo_relative(input_file),
      repo_relative(output_file)
    ]
  else:
    command = [
      str(edgutil.RequiredToolPath('edg-check-file-changes')),
      str(input_file),
      str(output_file)
    ]

  completed_process = subprocess.run(
    command,
    cwd = str(repository.repo_dir),
    stdin = subprocess.DEVNULL
  )
  if completed_process.returncode != 0:
    return None

  with open(output_file, 'r') as file_handle:
    results = json.load(file_handle)

  return dict(results)

def should_use_docker(repo_dir: Path) -> bool:
  '''Return True when file checks should run inside the dev container.'''
  return (
    shutil.which('docker') is not None and
    edgutil.is_docker_preferred(repo_dir)
  )

def summarize_file_result(
  file_result: Dict[str, Any]
) -> List[Tuple[str, str]]:
  '''Flatten one file's checker output into (kind, message) findings.

  A None misspelling list means spell checking was unavailable, which is
  reported as its own kind so callers can treat it as a warning rather
  than a violation.
  '''
  findings = []

  misspellings = file_result['misspellings']
  if misspellings is None:
    findings.append(
      ('spelling-unavailable', 'aspell not found or missing dictionary')
    )
  else:
    for misspelling in misspellings:
      findings.append(('misspelling', f"new misspelled word: {misspelling}"))

  for typo in file_result['typos']:
    findings.append(('typo', f"repeated-word typo: {typo.strip()}"))

  coding_errors = file_result['coding_errors']
  if coding_errors:
    for hit in coding_errors['missing_endifs']:
      findings.append(('coding-error', f"#endif without a comment: {hit}"))
    for hit in coding_errors['missing_elses']:
      findings.append(('coding-error', f"#else without a comment: {hit}"))
    for hit in coding_errors['closing_comments']:
      findings.append(('coding-error', f"closing comment: {hit.strip()}"))

  for line_no in sorted(file_result['overlong_lines'], key = int):
    findings.append(
      (
        'overlong-line',
        f"line {line_no} exceeds {MAX_LINE_COLUMNS} columns"
      )
    )

  return findings
