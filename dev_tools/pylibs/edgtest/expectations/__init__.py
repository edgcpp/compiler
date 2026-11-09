# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Inheritance of test expectations from the default configuration.

The expectations of a non-default configuration extend the expectations of the
default configuration (see edgtest.runtest.get_default_config_name).  Lines
are matched by the key edgtest.runtest.parse_test_status returns (the test
path, the test type, and the case options), and for a given key:

  - the configuration's own line wins, otherwise
  - the default configuration's line applies, otherwise
  - the test is expected to pass.

Because "no line" means "inherit", a configuration that passes a test the
default configuration fails records an explicit PASS line.  Explicit PASS
lines only exist in the files: the effective expectations (see
overlay_expectations) never contain them.
'''

from typing import Dict, Iterable, List, Tuple

from edgtest import runtest

_PASS_STATUS = 'PASS'

class _KeyedLine:
  def __init__(self, line: str, full_status: str) -> None:
    self.line = line
    self.full_status = full_status

  def is_pass(self) -> bool:
    return self.full_status == _PASS_STATUS

def _key_line(line: str) -> Tuple[str, _KeyedLine]:
  '''Return the key and keyed form of a status line.

  A line that doesn't parse is its own key and its own status, so it is kept
  as written (the diagnostics for malformed lines are elsewhere).
  '''
  try:
    key, parsed_status = runtest.parse_test_status(line)
    return (key, _KeyedLine(line, parsed_status.full_status))
  except Exception:
    return (line, _KeyedLine(line, line))

def _key_lines(lines: Iterable[str]) -> Dict[str, _KeyedLine]:
  keyed_lines: Dict[str, _KeyedLine] = {}
  for line in lines:
    stripped_line = line.strip()
    if len(stripped_line) == 0:
      continue
    key, keyed_line = _key_line(stripped_line)
    keyed_lines[key] = keyed_line
  return keyed_lines

def get_expectation_key(line: str) -> str:
  '''Return the key (see the module description) of an expectation line.'''
  key, _ = _key_line(line.strip())
  return key

def make_explicit_pass_line(key: str) -> str:
  '''Return the line that overrides an inherited failure with a pass.'''
  return f"{key}:{_PASS_STATUS}:"

def overlay_expectations(parent_lines: Iterable[str],
                         child_lines: Iterable[str]) -> List[str]:
  '''Return the effective expectation lines of a child configuration.

  A child line replaces the parent line with the same key.  Explicit PASS
  lines are dropped from the result, as an expectation of PASS is the same as
  no expectation.
  '''
  merged_lines = _key_lines(parent_lines)
  merged_lines.update(_key_lines(child_lines))
  return [
    keyed_line.line for keyed_line in merged_lines.values()
    if not keyed_line.is_pass()
  ]

def compute_expectation_delta(
                parent_lines: Iterable[str],
                effective_lines: Iterable[str]) -> List[str]:
  '''Return the lines a child configuration's file needs to record
  effective_lines (its effective expectations) given the parent's lines.

  Lines whose status matches the parent's are omitted (they are inherited), a
  line is kept where the status differs or the parent has none, and an
  explicit PASS line is emitted where the parent expects a failure and the
  child expects a pass.
  '''
  parent = {
    key: keyed_line for key, keyed_line in _key_lines(parent_lines).items()
    if not keyed_line.is_pass()
  }
  effective = {
    key: keyed_line for key, keyed_line in _key_lines(effective_lines).items()
    if not keyed_line.is_pass()
  }

  delta_lines: List[str] = []
  for key in [*effective, *(key for key in parent if key not in effective)]:
    parent_line = parent.get(key)
    effective_line = effective.get(key)
    if effective_line is None:
      # The parent expects a failure and the child expects a pass.
      delta_lines.append(make_explicit_pass_line(key))
    elif (parent_line is None or
          parent_line.full_status != effective_line.full_status):
      delta_lines.append(effective_line.line)

  return delta_lines
