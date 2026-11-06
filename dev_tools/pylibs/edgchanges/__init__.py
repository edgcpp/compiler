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
those the target Changes file already uses, which it accepts anyway), and
"//correction: true" marks the entry as a correction (see below).  Then comes
the tag block, one or more comma-separated "GH #N" tags in brackets (it may
wrap onto several lines), the summary line, a blank line, and the
description.  Lines are at most MAX_LINE_LENGTH columns, not counting trailing
whitespace (which is stripped when the entry is collected).

A correction replaces entries already collected into the current (newest,
still open) version of its Changes file: when it's collected, every entry
there with one of its tags is deleted, and it's added like any other entry
(dated the day it's collected).  Corrections don't reach across releases, so
each of its tags must be on an entry in the current version, and every tag of
those entries must be on the correction (see find_corrected_entries).

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
CORRECTION_DIRECTIVE = 'correction'
CORRECTION_DIRECTIVE_VALUE = 'true'
KNOWN_DIRECTIVES = (ALLOWED_WORDS_DIRECTIVE, CORRECTION_DIRECTIVE)
PENDING_CHANGES_DIR_NAME = 'pending_changes'
CHANGES_FILE_NAME = 'Changes'

MAX_LINE_LENGTH = 79
# The date ("10/21/26") is left-justified in this many columns before the
# tags.
DATE_WIDTH = 8

TAG_REGEX = re.compile(r'^GH #[0-9]+$')

# An open version is "Version 7.1"; a released one is
# "Version 7.0, September 28, 2026".
VERSION_HEADER_REGEX = re.compile(
  r'^Version ([0-9]+\.[0-9]+)(, [A-Z][a-z]+ [0-9]{1,2}, [0-9]{4})?$'
)
SEPARATOR_REGEX = re.compile(r'^-+$')

# The first line of an entry in a Changes file: the date and the start of the
# tag block.
_COLLECTED_ENTRY_START_REGEX = re.compile(
  r'^[0-9]{1,2}/[0-9]{1,2}/[0-9]{2} +\['
)

_CORRECTION_DIRECTIVE_LINE_REGEX = re.compile(
  r'^\s*//' + CORRECTION_DIRECTIVE + ':'
)

# The key of a directive line (see edgutil.split_leading_cpp_line_directives).
_DIRECTIVE_KEY_REGEX = re.compile(r'^\s*//([a-z_]+):')

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
               body_lines: List[str], *, is_correction: bool = False) -> None:
    self.path = path
    self.tags = tags
    self.summary = summary
    self.body_lines = body_lines
    self.is_correction = is_correction

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

def _split_tags(tag_block_lines: List[str]) -> List[str]:
  '''Return the tags in the tag block on tag_block_lines (anything before its
  '[' is ignored).'''
  tag_text = ' '.join(line.strip() for line in tag_block_lines)
  tag_text = tag_text[tag_text.index('[') + 1:tag_text.index(']')]
  return [tag.strip() for tag in tag_text.split(',')]

def _check_directive_keys(lines: List[str]) -> List[FormatError]:
  '''Return the problems with the keys of an entry's directives, given its
  directive lines.

  An unknown key is most likely a misspelled one, and ignoring it would be
  harmful: an entry with a misspelled //correction: would be collected as an
  ordinary entry, beside the entry it was meant to replace.
  '''
  errors = []
  for line_no, line in enumerate(lines, start = 1):
    key_match = _DIRECTIVE_KEY_REGEX.match(line)
    if key_match is not None and key_match.group(1) not in KNOWN_DIRECTIVES:
      known_text = ', '.join(f"//{key}:" for key in KNOWN_DIRECTIVES)
      errors.append(
        FormatError(
          line_no,
          f"unknown directive //{key_match.group(1)}: (the directives are "
          f"{known_text})"
        )
      )
  return errors

def _check_correction_directive(lines: List[str],
                                values: List[str]) -> List[FormatError]:
  '''Return the problems with an entry's //correction: directives (whose
  values are values), given the lines of the entry.'''
  line_nos = [
    line_no for line_no, line in enumerate(lines, start = 1)
    if _CORRECTION_DIRECTIVE_LINE_REGEX.match(line) is not None
  ]
  errors = []
  for line_no, value in zip(line_nos, values):
    if value.strip() != CORRECTION_DIRECTIVE_VALUE:
      errors.append(
        FormatError(
          line_no,
          f"invalid //{CORRECTION_DIRECTIVE}: value '{value.strip()}'; use "
          f"'//{CORRECTION_DIRECTIVE}: {CORRECTION_DIRECTIVE_VALUE}'"
        )
      )
  for line_no in line_nos[1:]:
    errors.append(
      FormatError(line_no, f"//{CORRECTION_DIRECTIVE}: is repeated")
    )
  return errors

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

  directive_map, remaining_lines = (
    edgutil.split_leading_cpp_line_directives(lines)
  )
  directive_lines = lines[:len(lines) - len(remaining_lines)]
  errors += _check_directive_keys(directive_lines)
  correction_values = directive_map.get(CORRECTION_DIRECTIVE, [])
  errors += _check_correction_directive(directive_lines, correction_values)
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

  tags = _split_tags(remaining_lines[:close_index + 1])
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
    remaining_lines[summary_index + 2:],
    is_correction = len(correction_values) != 0
  ), []

def find_newest_version_header(lines: List[str]) -> Optional[int]:
  '''Return the index of the newest Version header line of a Changes file.

  Body text can also start with "Version", so a header must be the first line
  of the file or directly follow a separator line.
  '''
  for line_index, line in enumerate(lines):
    follows_separator = (
      line_index == 0 or
      SEPARATOR_REGEX.match(lines[line_index - 1]) is not None
    )
    if follows_separator and VERSION_HEADER_REGEX.match(line) is not None:
      return line_index
  return None

# An entry in a Changes file: its lines are [start, end) (including the blank
# lines after it).
CollectedEntry = NamedTuple(
  'CollectedEntry',
  [('start', int), ('end', int), ('tags', List[str]), ('summary', str)]
)

def read_current_entries(lines: List[str]) -> Optional[List[CollectedEntry]]:
  '''Return the entries in the newest version of the Changes file whose lines
  are lines, or None if that version is released (or there is none).'''
  header_index = find_newest_version_header(lines)
  if header_index is None:
    return None
  header_match = VERSION_HEADER_REGEX.match(lines[header_index])
  assert header_match is not None
  if header_match.group(2) is not None:
    return None

  # The section ends at the separator before the next Version header.
  section_end = next(
    (
      index for index in range(header_index + 1, len(lines) - 1)
      if SEPARATOR_REGEX.match(lines[index]) is not None and
         VERSION_HEADER_REGEX.match(lines[index + 1]) is not None
    ),
    len(lines)
  )
  # Entries are normally separated by two blank lines, but not always (a
  # hand edit may have lost them), so only the first line is relied on.
  starts = [
    index for index in range(header_index + 1, section_end)
    if _COLLECTED_ENTRY_START_REGEX.match(lines[index]) is not None
  ]

  entries = []
  for start, end in zip(starts, starts[1:] + [section_end]):
    close_index = _find_tag_block_end(lines[start:end])
    if close_index is None:
      continue
    summary_index = start + close_index + 1
    entries.append(
      CollectedEntry(
        start,
        end,
        _split_tags(lines[start:summary_index]),
        lines[summary_index] if summary_index < end else ''
      )
    )
  return entries

# Added to the errors for a correction naming a tag the current version
# doesn't have, which is often one fixed in a prior release.
_PRIOR_RELEASE_HINT = (
  'correction: corrections can\'t reach across releases; a problem with an '
  'issue fixed in a prior release is usually a new issue, with an ordinary '
  'entry that references the old issue and the release it was fixed in'
)

def find_corrected_entries(changes_lines: List[str], tags: List[str]
                           ) -> Tuple[List[CollectedEntry], List[str]]:
  '''Return the entries a correction with tags replaces in the Changes file
  whose lines are changes_lines, and the reasons it can't (if any).

  It replaces the entries in the newest version with any of its tags.  Each
  of its tags must be on one of them (corrections don't reach across
  releases), and it must have all of their tags.
  '''
  current_entries = read_current_entries(changes_lines)
  if current_entries is None:
    return [], [
      'a correction must replace entries in the current version, but the '
      'newest version of the Changes file is released',
      _PRIOR_RELEASE_HINT
    ]

  tag_set = set(tags)
  corrected_entries = [
    entry for entry in current_entries if not tag_set.isdisjoint(entry.tags)
  ]
  corrected_tags = {tag for entry in corrected_entries for tag in entry.tags}
  errors = [
    f"correction: no entry in the current version has the tag '{tag}'"
    for tag in tags if tag not in corrected_tags
  ]
  if len(errors) != 0:
    errors.append(_PRIOR_RELEASE_HINT)
  for entry in corrected_entries:
    missing_tags = [tag for tag in entry.tags if tag not in tag_set]
    if len(missing_tags) != 0:
      errors.append(
        f"correction: the entry it replaces, \"{entry.summary}\", also has "
        f"{', '.join(repr(tag) for tag in missing_tags)}; add "
        f"{'it' if len(missing_tags) == 1 else 'them'} to the tag block"
      )
  return corrected_entries, errors

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
