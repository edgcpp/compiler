# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Pending Changes entries: the *.txt files in each pending_changes/
directory, which edg-collect-changes moves into the Changes file beside it
(the nightly collect-changes workflow lists the directories).

An entry looks like:

  //allowed_words: reflectable memoized
  [GH #335]
  Type transformations of qualified function types

  Previously, ...

The leading //key: value directives (EDG C++ line directive syntax) are
optional; //allowed_words: lists words the spell check should accept (beyond
those the target Changes file already uses, which it accepts anyway).  Then
comes the tag block, one or more comma-separated "GH #N" tags in brackets (it
may wrap onto several lines), the summary line, a blank line, and the
description.  Lines are at most MAX_LINE_LENGTH columns, not counting trailing
whitespace (which is stripped when the entry is collected).

The format is checked by edg-check-file-changes (which the pre-push hook and
edg-collect-changes --check run) and relied on by edg-collect-changes.
'''

import datetime
import re

import edgutil

from pathlib import Path
from typing import Iterable, List, NamedTuple, Optional, Set, Tuple

FRAGMENT_SUFFIX = '.txt'
ALLOWED_WORDS_DIRECTIVE = 'allowed_words'
PENDING_CHANGES_DIR_NAME = 'pending_changes'
CHANGES_FILE_NAME = 'Changes'

MAX_LINE_LENGTH = 79
# The date ("10/21/26") is left-justified in this many columns before the
# tags.
DATE_WIDTH = 8

TAG_REGEX = re.compile(r'^GH #[0-9]+$')

# The internal issue tracker form of a tag; accepted in commit messages but
# not in Changes entries.
EDG_TAG_REGEX = re.compile(r'^EDG[cfjp]+fe/[0-9]+$')

# The bracketed group at the end of a commit subject, which may be followed by
# the closing quotes of a git revert subject (Revert "...", nested to any
# depth); see check_commit_message.
_SUBJECT_TAGS_REGEX = re.compile(r'\[([^\[\]]*)\]"*$')

# A problem with an entry's format; line_no is None for the whole file.
FormatError = NamedTuple(
  'FormatError',
  [('line_no', Optional[int]), ('message', str)]
)

def is_fragment_path(path_str: str) -> bool:
  '''Return True if path_str names a pending Changes entry: a *.txt file in a
  pending_changes/ directory.'''
  path = Path(path_str)
  return (path.suffix == FRAGMENT_SUFFIX and
          path.parent.name == PENDING_CHANGES_DIR_NAME)

def _is_subject_tag(tag: str) -> bool:
  return (TAG_REGEX.match(tag) is not None or
          EDG_TAG_REGEX.match(tag) is not None)

def check_commit_message(message: str) -> Optional[str]:
  '''Return a description of the problem with a commit message, or None if
  it's acceptable.

  The subject is the first line, which must stand alone (be followed by a
  blank line, if anything) and end with a bracketed group that is either empty
  (for work with no issue) or a comma-separated list of tags: "GH #N" (as in a
  Changes entry) or the internal form "EDGcpfe/N".  Earlier bracketed text
  (e.g. "[[always_inline]]") is not checked.  Nothing in the message may break
  a line other than \n.'''
  # Keep the history friendly to simple line-based tooling: only \n may break
  # a line (git's rule), so splitlines and split('\n') agree.  This also
  # rejects CRLF, which git's default cleanup never leaves in a message.
  # The trailing newlines are dropped because only split yields their empty
  # final element.
  text = message.rstrip('\n')
  lines = text.split('\n')
  if text != '' and lines != text.splitlines():
    return ('message must not contain line-break characters other than'
            ' newline (e.g. carriage return)')

  subject = lines[0]
  # Like git, count a whitespace-only line as blank.
  if len(lines) > 1 and lines[1].strip() != '':
    return 'subject must be a single line; wrap the body after a blank line'

  match = _SUBJECT_TAGS_REGEX.search(subject)
  if match is None:
    return "subject must end with a tag list (use e.g. '[GH #123]', or '[]')"

  group = match.group(1)
  if group != '' and not all(
      _is_subject_tag(tag.strip()) for tag in group.split(',')):
    return f"invalid tag list '[{group}]'; use e.g. '[GH #123]', or '[]'"

  return None

def target_changes_path(fragment_path: Path) -> Path:
  '''Return the Changes file the entry at fragment_path will be collected
  into (the one beside its pending_changes/ directory).'''
  return fragment_path.parent.parent / CHANGES_FILE_NAME

def describe_error(display_path: str, error: FormatError) -> str:
  '''Return error as a "path:line: message" string.'''
  if error.line_no is None:
    return f"{display_path}: {error.message}"
  return f"{display_path}:{error.line_no}: {error.message}"

def allowed_words_hint(display_path: str, words: Iterable[str]) -> List[str]:
  '''Return the lines telling the user how to accept words an entry's spell
  check rejected.'''
  return [
    'If these words are correct, accept them by adding this line at the very',
    f"top of {display_path}:",
    f"  //{ALLOWED_WORDS_DIRECTIVE}: {' '.join(words)}"
  ]

def format_date(date: datetime.date) -> str:
  '''Format date as m/d/yy, the format used throughout the Changes files.'''
  return f"{date.month}/{date.day}/{date.year % 100:02d}"

def format_tag_lines(date_text: str, tags: List[str]) -> List[str]:
  '''Return the first line(s) of an entry: the date padded to DATE_WIDTH
  columns, a space, and the bracketed tags.

  Tags are packed onto each line as long as they fit in MAX_LINE_LENGTH
  columns; continuation lines line the tags up under the first one.
  '''
  first_line_prefix = f"{date_text:<{DATE_WIDTH}} ["
  continuation_prefix = ' ' * len(first_line_prefix)

  lines = []
  current_line = first_line_prefix
  line_has_tag = False
  for tag_index, tag in enumerate(tags):
    is_last_tag = tag_index == len(tags) - 1
    piece = tag + (']' if is_last_tag else ',')
    if line_has_tag and len(current_line) + len(piece) > MAX_LINE_LENGTH:
      lines.append(current_line)
      current_line = continuation_prefix
    current_line += piece
    line_has_tag = True
  lines.append(current_line)

  return lines

class ChangesEntry:
  '''A parsed pending Changes entry.'''

  def __init__(self, path: Path, tags: List[str], summary: str,
               body_lines: List[str]) -> None:
    self.path = path
    self.tags = tags
    self.summary = summary
    self.body_lines = body_lines

  def sort_key(self) -> Tuple[str, str, str]:
    return (self.summary.casefold(), self.summary, self.path.name)

  def format(self, date_text: str) -> List[str]:
    '''Return the lines of this entry as it appears in a Changes file.'''
    return (
      format_tag_lines(date_text, self.tags) +
      [self.summary, ''] +
      self.body_lines
    )

def _find_tag_block_end(lines: List[str]) -> Optional[int]:
  '''Return the index of the line closing the tag block that starts lines.'''
  return next(
    (index for index, line in enumerate(lines) if ']' in line),
    None
  )

def parse_fragment(path: Path, *, check_line_length: bool = True
                   ) -> Tuple[Optional[ChangesEntry], List[FormatError]]:
  '''Parse the pending Changes entry at path.

  Returns the entry (None if it's malformed) and the format errors found.
  Line lengths are checked only if check_line_length (edg-check-file-changes
  has its own check); trailing whitespace never counts.
  '''
  # The lines keep their newlines; everything below strips them anyway.
  try:
    with open(path, 'r', encoding = 'utf-8') as handle:
      lines = handle.readlines()
  except UnicodeDecodeError as ex:
    return None, [FormatError(None, f"not valid UTF-8 ({ex})")]

  errors = []
  for line_no, line in enumerate(lines, start = 1):
    if '\t' in line:
      errors.append(FormatError(line_no, 'tab character; use spaces'))
    if check_line_length and len(line.rstrip()) > MAX_LINE_LENGTH:
      errors.append(
        FormatError(line_no, f"line longer than {MAX_LINE_LENGTH} columns")
      )

  _, remaining_lines = edgutil.split_leading_cpp_line_directives(lines)
  first_line_no = len(lines) - len(remaining_lines) + 1
  remaining_lines = [line.rstrip() for line in remaining_lines]
  while remaining_lines and remaining_lines[-1] == '':
    remaining_lines.pop()

  if not remaining_lines or not remaining_lines[0].startswith('['):
    errors.append(
      FormatError(first_line_no, "expected the tag block, e.g. '[GH #123]'")
    )
    return None, errors

  close_index = _find_tag_block_end(remaining_lines)
  if close_index is None:
    errors.append(FormatError(first_line_no, "tag block has no closing ']'"))
    return None, errors
  if not remaining_lines[close_index].endswith(']'):
    errors.append(
      FormatError(
        first_line_no + close_index,
        "text after the tag block's ']'"
      )
    )
    return None, errors

  tag_text = ' '.join(
    line.strip() for line in remaining_lines[:close_index + 1]
  )[1:-1]
  tags = [tag.strip() for tag in tag_text.split(',')]
  if tags == ['']:
    errors.append(
      FormatError(first_line_no, "empty tag block; use e.g. '[GH #123]'")
    )
  else:
    seen_tags = set()
    for tag in tags:
      if TAG_REGEX.match(tag) is None:
        errors.append(
          FormatError(first_line_no, f"invalid tag '{tag}'; use 'GH #N'")
        )
      elif tag in seen_tags:
        errors.append(FormatError(first_line_no, f"tag '{tag}' is repeated"))
      seen_tags.add(tag)

  summary_index = close_index + 1
  summary_line_no = first_line_no + summary_index
  if (summary_index >= len(remaining_lines) or
      remaining_lines[summary_index] == ''):
    errors.append(
      FormatError(summary_line_no, 'expected the summary line after the tags')
    )
  elif remaining_lines[summary_index][0].isspace():
    errors.append(
      FormatError(summary_line_no, 'summary line must not be indented')
    )
  elif (summary_index + 2 >= len(remaining_lines) or
        remaining_lines[summary_index + 1] != '' or
        remaining_lines[summary_index + 2] == ''):
    errors.append(
      FormatError(
        summary_line_no + 1,
        'expected one blank line and then the description after the summary'
      )
    )

  if len(errors) != 0:
    return None, errors

  return ChangesEntry(
    path,
    tags,
    remaining_lines[summary_index],
    remaining_lines[summary_index + 2:]
  ), []

def read_prose(path: Path) -> Tuple[Set[str], List[str]]:
  '''Return the words an entry's //allowed_words: directives accept, and the
  lines of the entry to spell check (everything after the directives and the
  tag block).'''
  with open(path, 'r', encoding = 'utf-8', errors = 'replace') as handle:
    lines = [line.rstrip('\n') for line in handle]
  directive_map, remaining_lines = (
    edgutil.split_leading_cpp_line_directives(lines)
  )

  allowed_words: Set[str] = set()
  for value in directive_map.get(ALLOWED_WORDS_DIRECTIVE, []):
    allowed_words.update(value.split())

  if len(remaining_lines) != 0 and remaining_lines[0].startswith('['):
    close_index = _find_tag_block_end(remaining_lines)
    if close_index is None:
      close_index = len(remaining_lines) - 1
    remaining_lines = remaining_lines[close_index + 1:]

  return allowed_words, remaining_lines
