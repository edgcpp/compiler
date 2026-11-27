# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Filter an EDG ``--il_display`` dump to selected entities and fields.

Reads a filter specification and a full IL dump, emits matching entity
blocks (separated by blank lines) with only the requested fields. Raw
pointer suffixes (``@0x…``) are replaced by per-kind enumeration
(``type#1``, ``routine#2``, …) in order of first appearance in the
output; unnamed-entity references (``<unnamed@0x…>``) are enumerated the
same way.

Filter specification format::

    file-scope base-class
      type, derived_class, direct
    file-scope routine[special_kind==sfk_deduction_guide]
      source_corresp/name, type, special_kind

Each rule is a scope and kind (``file-scope routine``), optional
bracketed field conditions (``field==value`` or a bare ``field`` to
require the field's presence, comma-separated), and an indented
comma-separated field list. Lines starting with ``#`` are comments.
Field lists may span several indented lines. A condition on a repeated
field (e.g. the two ``kind:`` lines of a ``tk_typeref`` entry) matches
when any occurrence has the requested value. Multiple condition
alternatives may be given, separated by ``||``: comma-separated
conditions within one alternative are all required, and the rule matches
when any alternative holds, e.g.
``file-scope routine[builtin_function_kind ||
source_corresp/decl_position.seq]``.

Field paths: top-level IL labels are written as-is (including dots:
``position.seq``). Members of a section use ``section/label`` (e.g.
``source_corresp/name``); sections are either colon-terminated openers
(``source_corresp:``) or bare labels with indented contents (e.g.
``expr_range``, so ``expr_range/start.seq``). Selected fields are copied
in IL block order; continuation lines are preserved. Absent optional
fields are omitted; repeated fields emit every occurrence.

Tests select this filter with ``//filter:edg-il-filter`` and embed the
specification in an ``/* @filter ... */`` comment in the test source
(see ``extract_filter_spec``).

This module is a library; ``dev_tools/bin/edg-il-filter`` is its only
command-line entry point.
'''

import re
from typing import Dict, List, Match, Optional, Tuple

# Start of an IL entity block, e.g. file-scope routine@60f1989dce30
ENTITY_HEADER_RE = re.compile(
  r'^([\w-]+) ([\w-]+)(?:@([0-9a-f]+))?$'
)
# Reference embedded in a field value, e.g. file-scope type@60f1989dbd38
IL_REF_RE = re.compile(
  r'\b(file|func)-scope ([\w-]+)@([0-9a-f]+)'
)
# Unnamed-entity reference in display text, e.g. <unnamed@60f1989dbd38>
UNNAMED_REF_RE = re.compile(
  r'<unnamed@([0-9a-f]+)>'
)
# Separates section from member in filter field paths (source_corresp/name)
PATH_SEP = '/'

# Matches a /* @filter ... */ comment embedding a filter specification in
# a test source file.
FILTER_COMMENT_RE = re.compile(r'/\*\s*@filter\b(.*?)\*/', re.DOTALL)


def extract_filter_spec(source_text: str) -> Optional[str]:
  '''Return the filter spec from a ``/* @filter ... */`` comment, if any.'''
  match = FILTER_COMMENT_RE.search(source_text)
  if match is None:
    return None
  return match.group(1)


class EntityRule:
  '''One entity selector from the filter specification.'''

  def __init__(
    self, scope: str, kind: str,
    condition_groups: Optional[List[Dict[str, Optional[str]]]] = None,
    fields: Optional[List[str]] = None
  ) -> None:
    self.scope = scope
    self.kind = kind
    # Alternatives (``||``-separated in the specification); each maps a
    # field path to the required value, or None to just require the field's
    # presence.
    self.condition_groups = (
      condition_groups if condition_groups is not None else []
    )
    self.fields = fields if fields is not None else []


def parse_filter_spec(text: str) -> List[EntityRule]:
  '''Parse filter specification text into a list of entity rules.'''
  rules: List[EntityRule] = []
  for raw_line in text.splitlines():
    line = raw_line.rstrip()
    if not line or line.lstrip().startswith('#'):
      continue
    if line.startswith(' ') or line.startswith('\t'):
      if not rules:
        raise ValueError('field list before any entity rule')
      for part in line.split(','):
        part = part.strip()
        if part:
          rules[-1].fields.append(part)
      continue
    scope, rest = line.split(None, 1)
    kind_part, _, cond_part = rest.partition('[')
    kind = kind_part.strip()
    condition_groups: List[Dict[str, Optional[str]]] = []
    if cond_part:
      if not cond_part.endswith(']'):
        raise ValueError(f'unclosed condition in rule: {line}')
      cond_text = cond_part[:-1]
      for group_text in cond_text.split('||'):
        conditions: Dict[str, Optional[str]] = {}
        for item in group_text.split(','):
          item = item.strip()
          if not item:
            continue
          key, sep, value = item.partition('==')
          key = key.strip()
          if not key:
            raise ValueError(f'bad condition {item!r} in {line}')
          conditions[key] = value.strip() if sep else None
        if conditions:
          condition_groups.append(conditions)
    rules.append(EntityRule(scope = scope, kind = kind,
                            condition_groups = condition_groups))
  return rules


def split_il_blocks(lines: List[str]) -> List[List[str]]:
  '''Split IL dump lines into entity blocks (one list of lines per entity).'''
  blocks: List[List[str]] = []
  current: List[str] = []
  for line in lines:
    if ENTITY_HEADER_RE.match(line):
      if current:
        blocks.append(current)
      current = [line]
    elif current:
      current.append(line)
  if current:
    blocks.append(current)
  return blocks


def parse_entity_header(header_line: str) -> Tuple[str, str, Optional[str]]:
  '''Return (scope, kind, address) from an entity header line.'''
  match = ENTITY_HEADER_RE.match(header_line)
  if not match:
    raise ValueError(f'not an entity header: {header_line!r}')
  return match.group(1), match.group(2), match.group(3)


def _line_indent(line: str) -> int:
  '''Return the indentation column of a line.'''
  return len(line) - len(line.lstrip())


def _line_opens_section(
  block: List[str], index: int, line: str
) -> bool:
  '''True if a label-only line is followed by more-indented content.

  Section openers either end with a colon (``source_corresp:``) or are
  bare labels with an indented body (``expr_range``).
  '''
  stripped = line.strip()
  if not stripped:
    return False
  if ':' in stripped:
    _label, _, value = stripped.partition(':')
    if value.strip():
      return False
  line_indent = _line_indent(line)
  for next_line in block[index + 1:]:
    if not next_line.strip():
      continue
    return _line_indent(next_line) > line_indent
  return False


def _continuation_lines(
  lines: List[str], start: int, key_indent: int
) -> List[str]:
  '''Collect indented continuation lines following a label with no value.'''
  out: List[str] = []
  for line in lines[start:]:
    if not line.strip():
      break
    if _line_indent(line) <= key_indent:
      break
    out.append(line)
  return out


class FieldOccurrence:
  '''One occurrence of a field within an entity block.'''

  def __init__(self, path: str, lines: List[str]) -> None:
    self.path = path
    self.lines = lines


def scan_block_fields(block: List[str]) -> List[FieldOccurrence]:
  '''Split an entity block into its field occurrences.

  Paths chain through enclosing sections (``source_corresp/name``);
  repeated labels yield one occurrence each, in block order.  Section
  openers are reported both as a field whose value is the whole
  indented body (e.g. a plain ``template_arg_list`` path) and as a
  section prefix for the member paths within that body.
  '''
  occurrences: List[FieldOccurrence] = []
  section_stack: List[Tuple[str, int]] = []
  for index in range(1, len(block)):
    line = block[index]
    stripped = line.strip()
    if not stripped:
      continue
    indent = _line_indent(line)
    while section_stack and indent <= section_stack[-1][1]:
      section_stack.pop()
    if ':' in stripped:
      label = stripped.split(':', 1)[0]
    else:
      label = stripped
    if not label:
      continue
    path = PATH_SEP.join([name for name, _ in section_stack] + [label])
    if _line_opens_section(block, index, line):
      occurrences.append(
        FieldOccurrence(
          path = path,
          lines = [line] + _continuation_lines(block, index + 1, indent),
        )
      )
      section_stack.append((label, indent))
      continue
    if ':' not in stripped:
      # A bare-word line that opens nothing (e.g. a wrapped value); not
      # a field.
      continue
    occurrences.append(FieldOccurrence(path = path, lines = [line]))
  return occurrences


def _single_line_value(field_lines: List[str]) -> str:
  '''Extract the value string from a field's display lines (for conditions).'''
  first = field_lines[0]
  if ':' in first:
    value = first.split(':', 1)[1].strip()
    if value:
      return value
  for line in field_lines[1:]:
    stripped = line.strip()
    if stripped:
      return stripped
  return ''


def _conditions_hold(
  conditions: Dict[str, Optional[str]], occurrences: List[FieldOccurrence]
) -> bool:
  '''True when all conditions of one alternative hold for the block.

  A ``field==value`` condition is satisfied when any occurrence of the
  field has the value; a bare ``field`` condition just requires the
  field to be present.
  '''
  for key, want in conditions.items():
    got = [occ for occ in occurrences if occ.path == key]
    if want is None:
      if not got:
        return False
    elif not any(_single_line_value(occ.lines) == want for occ in got):
      return False
  return True


def rule_matches_block(
  rule: EntityRule, block: List[str], occurrences: List[FieldOccurrence]
) -> bool:
  '''True if the block's header and optional conditions satisfy the rule.

  Condition alternatives (``||``-separated in the specification) are
  tried in order; the rule matches when any alternative's conditions all
  hold.  A rule without conditions matches every block of its scope
  and kind.
  '''
  scope, kind, _addr = parse_entity_header(block[0])
  if scope != rule.scope or kind != rule.kind:
    return False
  if not rule.condition_groups:
    return True
  return any(_conditions_hold(conditions, occurrences)
             for conditions in rule.condition_groups)


def extract_requested_fields(
  occurrences: List[FieldOccurrence], field_paths: List[str]
) -> List[str]:
  '''Extract selected field occurrences in IL block order.

  Every occurrence of a requested field is emitted; absent optional
  fields are omitted.
  '''
  want = set(field_paths)
  out: List[str] = []
  for occ in occurrences:
    if occ.path in want:
      out.extend(occ.lines)
  return out


class AddressMap:
  '''Map raw IL addresses to per-kind enumeration numbers.

  Each entity scope and kind (file-scope type, func-scope routine, …)
  gets its own 1-based counter; unnamed entities have their own
  counter.  Numbers are assigned on first reference in output order,
  so the same address always maps to the same number.
  '''

  def __init__(self) -> None:
    self._next: Dict[Tuple[str, str], int] = {}
    self._addr: Dict[Tuple[str, str, str], int] = {}

  def number(self, scope: str, kind: str, addr: str) -> int:
    '''Return the enumeration number for (scope, kind, addr).'''
    # Accept scope with or without the "-scope" suffix.
    if scope.endswith('-scope'):
      scope = scope[:-len('-scope')]
    key = (scope, kind, addr)
    if key not in self._addr:
      counter = (scope, kind)
      self._next[counter] = self._next.get(counter, 0) + 1
      self._addr[key] = self._next[counter]
    return self._addr[key]

  def substitute(self, text: str) -> str:
    '''Replace raw entity references in text with enumerated ones.'''
    def repl(match: Match[str]) -> str:
      scope, kind, addr = match.group(1), match.group(2), match.group(3)
      num = self.number(scope, kind, addr)
      return f'{scope}-scope {kind}#{num}'

    def repl_unnamed(match: Match[str]) -> str:
      num = self.number('', 'unnamed', match.group(1))
      return f'<unnamed#{num}>'

    text = IL_REF_RE.sub(repl, text)
    return UNNAMED_REF_RE.sub(repl_unnamed, text)


def format_entity_header(
  block: List[str], addr_map: AddressMap
) -> str:
  '''Format the block header with an enumerated address (scope kind#N).'''
  scope, kind, addr = parse_entity_header(block[0])
  assert addr is not None
  num = addr_map.number(scope, kind, addr)
  return f'{scope} {kind}#{num}'


def filter_il_dump(
  il_text: str, spec_text: str
) -> str:
  '''Apply the filter specification to a full IL dump; return filtered text.'''
  rules = parse_filter_spec(spec_text)
  lines = il_text.splitlines()
  blocks = split_il_blocks(lines)
  addr_map = AddressMap()
  output_blocks: List[str] = []

  for block in blocks:
    occurrences = scan_block_fields(block)
    for rule in rules:
      if rule_matches_block(rule, block, occurrences):
        header = format_entity_header(block, addr_map)
        body_lines = extract_requested_fields(occurrences, rule.fields)
        body = [addr_map.substitute(line) for line in body_lines]
        output_blocks.append('\n'.join([header] + body))
        break

  if not output_blocks:
    return ''
  return '\n\n'.join(output_blocks) + '\n\n'
