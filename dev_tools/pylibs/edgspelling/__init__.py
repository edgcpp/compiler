# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The hierarchical allowlist for the spell check.

The list lives in dev_tools/spelling_allowlist.json as a tree of nodes.  Each
node has these optional members:

  "words"  a sorted list of lowercase words allowed with any capitalization
  "names"  a sorted list of words (proper nouns, say) allowed with exactly the
           capitalization given
  "paths"  an object mapping a path component (a directory or file name) to a
           child node

For example:

  {
    "words": ["lvalue"],
    "names": ["EDG"],
    "paths": {
      "src": {
        "words": ["demangle"],
        "paths": {"templates.c": {"words": ["tokenize"]}}
      }
    }
  }

Each entry is a single word.  The spell check breaks identifiers into words at
underscores and camel case boundaries before it looks at them, so an entry
like "foo_bar" or "fooBar" would never match anything, and the Allowlist
class rejects one.  An identifier that preprocessing leaves whole, such as
"getXValue" (it has a one letter part), is a single word and is allowed.

Lists are sorted by code point, without duplicates.  The Allowlist class
validates the file when it is loaded, which fails if a list is out of order, a
word isn't lowercase, or a node is malformed.

A file's allowed words are the union of those of every node on the way down to
it, so a word allowed for a directory is allowed for everything in it.
'''

import json
import re

import edgtools

from pathlib import Path, PurePosixPath
from typing import Any, Dict, Match, Set

DEFAULT_ALLOWLIST_PATH = edgtools.get_tools_dir() / 'spelling_allowlist.json'

class AllowlistError(Exception):
  '''The allowlist file is malformed.'''

class Allowlist:
  '''The allowlist file, loaded and validated.

  Construction reads the file and raises AllowlistError if it isn't valid
  JSON or any node is malformed, out of order, or (for "words") not lowercase,
  so a bad file is found up front.  A missing file is an empty allowlist.
  '''

  def __init__(self, tree_path: Path = DEFAULT_ALLOWLIST_PATH) -> None:
    self._tree_path = tree_path
    self._tree: Dict[str, Any] = {}
    if tree_path.is_file():
      self._tree = self._read()
      self._validate()

  def words_for_path(self, repo_path: str) -> Set[str]:
    '''Return the lowercase words allowed, in any case, for repo_path (repo
    relative): the "words" of every node on the way down to it.'''
    return self._entries_for_path(repo_path, 'words')

  def names_for_path(self, repo_path: str) -> Set[str]:
    '''Return the names allowed, with exactly the capitalization given, for
    repo_path (repo relative): the "names" of every node on the way down to
    it.'''
    return self._entries_for_path(repo_path, 'names')

  def _entries_for_path(self, repo_path: str, key: str) -> Set[str]:
    node = self._tree
    entries = set(node.get(key, []))
    for component in PurePosixPath(repo_path).parts:
      node = node.get('paths', {}).get(component)
      if node is None:
        break
      entries.update(node.get(key, []))
    return entries

  def _read(self) -> Dict[str, Any]:
    try:
      with open(self._tree_path, 'r', encoding = 'utf-8') as file_handle:
        return json.load(file_handle)
    except (OSError, ValueError) as error:
      raise AllowlistError(f"{self._tree_path}: {error}") from error

  def _validate(self) -> None:
    '''Check every node of the tree.

    A node must be an object with only "words", "names" and "paths", the
    first two being sorted lists of strings (the words lowercase) and the last
    an object of nodes.  Raises AllowlistError for the first problem found.
    '''
    pending = [(str(self._tree_path), self._tree)]
    while pending:
      where, node = pending.pop()
      if not isinstance(node, dict):
        raise AllowlistError(f"{where}: expected an object")
      unknown_keys = set(node) - {'words', 'names', 'paths'}
      if unknown_keys:
        raise AllowlistError(f"{where}: unknown keys {sorted(unknown_keys)}")
      for key in ('words', 'names'):
        self._validate_entries(node.get(key, []), key, where)
      children = node.get('paths', {})
      if not isinstance(children, dict):
        raise AllowlistError(f"{where}: \"paths\" must be an object")
      for name, child in children.items():
        pending.append((f"{where}: {name}", child))

  @staticmethod
  def _validate_entries(entries: Any, key: str, where: str) -> None:
    '''Check that entries, the value of key in a node, is a list of strings
    that is sorted, has no duplicates, is lowercase (for "words"), and is a
    single word that preprocessing would leave whole.'''
    if not isinstance(entries, list) or not all(
      isinstance(entry, str) for entry in entries
    ):
      raise AllowlistError(f"{where}: \"{key}\" must be a list of strings")
    for previous, entry in zip(entries, entries[1:]):
      if entry == previous:
        raise AllowlistError(f"{where}: \"{key}\" lists \"{entry}\" twice")
      if entry < previous:
        raise AllowlistError(
          f"{where}: \"{key}\" is out of order: \"{entry}\" must come "
          f"before \"{previous}\" (entries are sorted by code point)"
        )
    if key == 'words':
      for entry in entries:
        if entry != entry.lower():
          raise AllowlistError(
            f"{where}: \"words\" are matched ignoring case, so \"{entry}\" "
            "must be lowercase (use \"names\" to require a capitalization)"
          )
    for entry in entries:
      if '_' in entry or preprocess(entry) != entry:
        raise AllowlistError(
          f"{where}: \"{key}\" entry \"{entry}\" would be split into "
          "words before it is checked, so it could never match; list its "
          "parts instead"
        )

# A lowercase letter or digit followed by an uppercase letter ("fooBar"), or
# the last capital of an acronym followed by a capitalized word ("HTTPServer").
_CAMEL_CASE_BOUNDARY_REGEX = re.compile(
  r'(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])'
)

_TOKEN_REGEX = re.compile(r'[A-Za-z0-9]+')

def _is_word_like(part: str) -> bool:
  '''Return whether part could be a word or an acronym: at least two
  characters, and either all capitals/digits or containing a vowel.'''
  return len(part) >= 2 and (
    re.fullmatch(r'[A-Z0-9]+', part) is not None
    or re.search(r'[aeiouyAEIOUY]', part) is not None
  )

def _split_token(token_match: Match[str]) -> str:
  token = token_match.group()
  parts = _CAMEL_CASE_BOUNDARY_REGEX.split(token)
  # Random looking data (base64, hashes) has one letter "humps", parts without
  # vowels, and short parts on average, so it isn't split.  Identifiers are:
  # either their parts all look like words or acronyms ("setUp", "ABIs"), or
  # they're long enough on average to be abbreviations like "Stmt" and "Msvc".
  if (len(parts) > 1
      and min(len(part) for part in parts) >= 2
      and (all(_is_word_like(part) for part in parts)
           or len(token) >= 3 * len(parts))):
    return ' '.join(parts)
  return token

def _split_camel_case(text: str) -> str:
  '''Return text with a space at each camel case boundary of its identifiers.

  The spell checker then sees "foo Bar" for "fooBar" (as it already sees "foo"
  and "bar" for "foo_bar"), so identifiers needn't be allowed whole.  A token
  that looks like random data rather than an identifier, such as "getXValue"
  or a base64 blob, is left whole.
  '''
  return _TOKEN_REGEX.sub(_split_token, text)

def preprocess(text: str) -> str:
  '''Return text prepared for the spell checker.

  Identifiers are broken into the words they are made of, so that
  "fooBar" is checked as "foo Bar" instead of being flagged whole.
  '''
  return _split_camel_case(text)
