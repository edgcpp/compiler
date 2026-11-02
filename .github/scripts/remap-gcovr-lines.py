#!/usr/bin/env python3

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Move the line numbers of a gcovr JSON report to where those lines are in
the current sources, e.g., when CI reuses the coverage report of an earlier
run whose build differed from this one only in line numbers:

  remap-gcovr-lines.py <old sources> <old report> <new report>

<old sources> is a directory holding a copy of the files the old report
covers, as they were when it was made (at the same paths relative to it as
the report's paths relative to the current directory, the gcovr root). Run
this in the environment gcovr ran in (its source encoding is the locale's).

Lines that git's diff of the old and current sources leaves alone move
exactly; lines in changed hunks go to the corresponding lines of the new hunk
(or its last line). The source line checksums are recomputed from the current
sources, as gcovr refuses to merge reports whose checksums for a line differ.
Exits with a nonzero status, without writing the new report, if a file of
the report is missing or the report has data this script doesn't know how to
move.
'''

import argparse
import hashlib
import json
import locale
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Callable, Dict, List, Tuple

# The keys of the report entries this script knows; an unknown key might
# hold a line number, so it makes the report unmappable.
FILE_KEYS = {'file', 'lines', 'functions'}
LINE_KEYS = {'line_number', 'function_name', 'block_ids', 'count',
             'branches', 'calls', 'gcovr/md5', 'gcovr/excluded'}
FUNCTION_KEYS = {'name', 'demangled_name', 'lineno', 'execution_count',
                 'blocks_percent', 'pos', 'gcovr/excluded'}

# A hunk of a "git diff -U0": (old start, old count, new start, new count).
Hunk = Tuple[int, int, int, int]

HUNK_HEADER = re.compile(rb'^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@')


class UnmappableError(Exception):
  '''The report can't be moved to the current sources.'''


def hunks(old: Path, new: Path) -> List[Hunk]:
  '''Return the hunks of the zero-context diff of two files.'''
  # "git diff --no-index" exits with 1 when the files differ.
  proc = subprocess.run(['git', 'diff', '--no-index', '--no-ext-diff',
                         '--no-color', '-U0', '--', str(old), str(new)],
                        stdout = subprocess.PIPE)
  if proc.returncode not in (0, 1):
    raise UnmappableError(f"can't diff {old} and {new}")
  result = []
  for line in proc.stdout.splitlines():
    m = HUNK_HEADER.match(line)
    if m:
      result.append((int(m[1]), int(m[2] or 1), int(m[3]), int(m[4] or 1)))
  return result


def line_mapper(file_hunks: List[Hunk]) -> Callable[[int], int]:
  '''Return a function that moves an old line number to the new file.'''
  def map_line(lineno: int) -> int:
    delta = 0
    for old_start, old_count, new_start, new_count in file_hunks:
      if old_count == 0:
        # An insertion after old_start.
        if lineno <= old_start:
          break
      elif lineno < old_start:
        break
      elif lineno < old_start + old_count:
        # A changed line: use the corresponding line of the new hunk (or its
        # last line); a deleted line goes to the line before the deletion.
        if new_count == 0:
          return max(new_start, 1)
        return new_start + min(lineno - old_start, new_count - 1)
      delta += new_count - old_count
    return lineno + delta
  return map_line


def check_keys(entry: Dict[str, Any], known: set, what: str) -> None:
  '''Raise UnmappableError if an entry has keys this script doesn't know.'''
  unknown = set(entry) - known
  if unknown:
    raise UnmappableError(f"unknown {what} keys: {sorted(unknown)}")


def remap_file(filecov: Dict[str, Any], map_line: Callable[[int], int],
               source_lines: List[bytes], encoding: str) -> None:
  '''Move the line numbers of one file's coverage in place.'''
  for linecov in filecov['lines']:
    check_keys(linecov, LINE_KEYS, 'line')
    lineno = map_line(linecov['line_number'])
    linecov['line_number'] = lineno
    if 'gcovr/md5' in linecov:
      # As gcovr computes it (see gcovr.utils.read_source_file).
      text = (source_lines[lineno - 1] if lineno <= len(source_lines)
              else b'/*EOF*/')
      linecov['gcovr/md5'] = hashlib.md5(
        text.decode(encoding, errors = 'replace').encode('UTF-8'),
        usedforsecurity = False).hexdigest()
  for funccov in filecov['functions']:
    check_keys(funccov, FUNCTION_KEYS, 'function')
    funccov['lineno'] = map_line(funccov['lineno'])
    if 'pos' in funccov:
      positions = []
      for pos in funccov['pos']:
        lineno, column = pos.split(':', 1)
        positions.append(f"{map_line(int(lineno))}:{column}")
      funccov['pos'] = positions


def remap_report(report: Dict[str, Any], old_sources: Path) -> None:
  '''Move the line numbers of a whole report in place.'''
  encoding = locale.getpreferredencoding()
  for filecov in report['files']:
    check_keys(filecov, FILE_KEYS, 'file')
    old = old_sources / filecov['file']
    new = Path(filecov['file'])
    try:
      old_text = old.read_bytes()
      new_text = new.read_bytes()
    except OSError as e:
      raise UnmappableError(str(e))
    if old_text != new_text:
      remap_file(filecov, line_mapper(hunks(old, new)),
                 new_text.splitlines(), encoding)


def main() -> int:
  parser = argparse.ArgumentParser(
    description = __doc__,
    formatter_class = argparse.RawDescriptionHelpFormatter)
  parser.add_argument('old_sources', type = Path)
  parser.add_argument('old_report')
  parser.add_argument('new_report')
  args = parser.parse_args()

  with open(args.old_report, encoding = 'utf-8') as f:
    report = json.load(f)
  try:
    remap_report(report, args.old_sources)
  except UnmappableError as e:
    print(f"remap-gcovr-lines: {e}", file = sys.stderr)
    return 1
  with open(args.new_report, 'w', encoding = 'utf-8') as f:
    json.dump(report, f)
  return 0


if __name__ == '__main__':
  sys.exit(main())
