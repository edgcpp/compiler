# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# EDG Testing library specific to the "run_test format" itself. Anything not
# directly related to the format's serialization, deserialization, and/or
# interpretation should not be put here.

import asyncio
import filecmp
import functools
import hashlib
import io
import json
import os
import re
import shlex
import shutil
import sys
import tarfile
import tempfile

import edgutil

from enum import IntEnum
from collections import OrderedDict
from pathlib import Path
from typing import (
  Any, Callable, Dict, IO, Iterator, List, Mapping, NamedTuple, Optional,
  Pattern, Set, Tuple, cast
)

def get_default_config_name(env: Optional[Dict[str, str]] = None) -> str:
  env_to_use = env or os.environ
  if 'RUN_TEST_DEFAULT_CONFIG' in env_to_use:
    return env_to_use['RUN_TEST_DEFAULT_CONFIG']
  return 'edg_x86_64'

def get_parent_config_name(
                       config_name: str,
                       env: Optional[Dict[str, str]] = None) -> Optional[str]:
  '''Return the configuration config_name inherits expectations from.

  This is the default configuration, or None for the default configuration
  itself.
  '''
  default_config_name = get_default_config_name(env)
  if config_name == default_config_name:
    return None
  return default_config_name

def get_recording_possible_names(test_suite_env: Dict[str, str],
                                 case_number: int,
                                 command_number: int) -> List[str]:
  assert case_number >= 1, "case numbers start at 1"
  assert command_number >= 1, "command numbers start at 1"
  results = []

  env = cast(Dict[str, str], test_suite_env or os.environ)

  active_config = env['EDG_CONFIG']
  default_config = get_default_config_name(env)

  if active_config != default_config:
    results.append(f"{active_config}.{case_number}.{command_number}.txt")
  results.append(f"default.{case_number}.{command_number}.txt")
  return results

def get_matches_possible_names(test_suite_env: Dict[str, str],
                               case_number: int) -> List[str]:
  '''Return ordered candidate names for a case match-report recording.'''
  assert case_number >= 1, "case numbers start at 1"
  results = []

  env = cast(Dict[str, str], test_suite_env or os.environ)

  active_config = env['EDG_CONFIG']
  default_config = get_default_config_name(env)

  if active_config != default_config:
    results.append(f"{active_config}.{case_number}.matches.txt")
  results.append(f"default.{case_number}.matches.txt")
  return results

def get_bare_name(test_file_path: Path) -> str:
  '''Return the test name without the file extension.'''
  return test_file_path.name.split('.')[0]

def get_assoc_files_path(test_file_path: Path) -> Optional[Path]:
  '''Returns a relative path to the test's associated files.'''
  first_suffix = test_file_path.suffixes[0]

  if first_suffix == '.sft':
    return None
  if first_suffix == '.mft':
    return test_file_path.parent

  bare_name = get_bare_name(test_file_path)
  return test_file_path.parent / f".{bare_name}"

def is_valid_assoc_path(_dir: str, entry_name: str) -> bool:
  if entry_name[0] == '.':
    return False
  if '.mft.' in entry_name:
    return False
  return True

def get_assoc_recordings_path(test_file_path: Path) -> Path:
  '''Returns a relative path to the test's associated recordings.'''
  bare_name = get_bare_name(test_file_path)
  return test_file_path.parent / f".{bare_name}.rto"

class TestType(IntEnum):
  FRONT_END                 = 1
  FRONT_END_NEGATIVE        = 2
  FRONT_END_CATASTROPHE     = 3
  COMPILE                   = 4
  COMPILE_NEGATIVE          = 5
  COMPILE_CATASTROPHE       = 6
  COMPILE_AND_LINK          = 7
  COMPILE_AND_LINK_NEGATIVE = 8
  RUNTIME                   = 9
  RUNTIME_NEGATIVE          = 10
  RUNTIME_ABORT             = 11
  BACK_END_CPP_CODEGEN      = 12
  SKIP_TEST                 = 13

_TEST_TYPE_TO_STRING = {
  TestType.FRONT_END:                 'fp',
  TestType.FRONT_END_NEGATIVE:        'fn',
  TestType.FRONT_END_CATASTROPHE:     'fc',
  TestType.COMPILE:                   'cp',
  TestType.COMPILE_NEGATIVE:          'cn',
  TestType.COMPILE_CATASTROPHE:       'cc',
  TestType.COMPILE_AND_LINK:          'lp',
  TestType.COMPILE_AND_LINK_NEGATIVE: 'ln',
  TestType.RUNTIME:                   'rp',
  TestType.RUNTIME_NEGATIVE:          'rn',
  TestType.RUNTIME_ABORT:             'ra',
  TestType.BACK_END_CPP_CODEGEN:      'cppbe',
  TestType.SKIP_TEST:                 's'
}

_STRING_TO_TEST_TYPE = {v: k for k, v in _TEST_TYPE_TO_STRING.items()}

def test_type_to_string(run_test_type: TestType) -> str:
  return _TEST_TYPE_TO_STRING[run_test_type]

def test_type_from_string(string: str) -> TestType:
  return _STRING_TO_TEST_TYPE[string]

_POSITIVE_TEST_TYPES = (
  TestType.FRONT_END,
  TestType.BACK_END_CPP_CODEGEN,
  TestType.COMPILE,
  TestType.COMPILE_AND_LINK,
  TestType.RUNTIME
)

def is_positive_test_type(test_type: TestType) -> bool:
  return test_type in _POSITIVE_TEST_TYPES

_FRONT_END_TEST_TYPES = (
  TestType.FRONT_END,
  TestType.FRONT_END_NEGATIVE,
  TestType.FRONT_END_CATASTROPHE
)

def is_front_end_test_type(test_type: TestType) -> bool:
  return test_type in _FRONT_END_TEST_TYPES

_EXECUTED_TEST_TYPES = (
  TestType.RUNTIME,
  TestType.RUNTIME_NEGATIVE,
  TestType.RUNTIME_ABORT
)

def is_executed_test_type(test_type: TestType) -> bool:
  return test_type in _EXECUTED_TEST_TYPES

_LINK_TEST_TYPES = (
  TestType.COMPILE_AND_LINK,
  TestType.COMPILE_AND_LINK_NEGATIVE,
  *_EXECUTED_TEST_TYPES
)

def is_link_test_type(test_type: TestType) -> bool:
  return test_type in _LINK_TEST_TYPES

_CATASTROPHE_TEST_TYPES = (
  TestType.FRONT_END_CATASTROPHE,
  TestType.COMPILE_CATASTROPHE
)

def is_catastrophe_test_type(test_type: TestType) -> bool:
  return test_type in _CATASTROPHE_TEST_TYPES

def _append_option(original_str: str, new_option_str: str) -> str:
  '''Compose the given new option's string to the original option string value
  (if the option is not already present), returning the new string value.
  '''
  dest_str = original_str
  if new_option_str not in dest_str:
    if len(dest_str) > 0:
      dest_str += ' '
    dest_str += new_option_str
  return dest_str

def _remove_option_regex(original_str: str,
                         removal_regex: Pattern[str]) -> str:
  new_str = removal_regex.sub(' ', original_str)
  # Trim off any whitespace preceding or following the string.
  return new_str.strip()

_OVERRIDE_OPTIONS_REGEX_PART = '|'.join(_TEST_TYPE_TO_STRING.values())
_TEST_TYPE_OVERRIDE_OPTION_REGEX = re.compile(
  f'^(.*);({_OVERRIDE_OPTIONS_REGEX_PART})$'
)

class TestModuleFileType(IntEnum):
  # A module interface source file; it will be compiled with the
  # --module_interface flag to generate both an IFC and a obj file.
  INTERFACE       = 1
  # A object-only module source file; the IFC file is assumed to already be
  # present and the moudle will be compiled only for its obj file.
  OBJECT_ONLY     = 2
  # A module implementation unit file with a forced IFC production; this is an
  # unusual situation in which the module file will be compiled with the
  # --module_internal_partition flag to generate both an IFC and a obj file.
  INTERNAL_AS_IFC = 3

class TestModuleFileDescription:
  def __init__(self, file_name: str, *, type: TestModuleFileType) -> None:
    self.file_name = file_name
    self.type = type

  def __str__(self) -> str:
    if self.type == TestModuleFileType.INTERFACE:
      return self.file_name
    elif self.type == TestModuleFileType.OBJECT_ONLY:
      return f"obj:{self.file_name}"
    elif self.type == TestModuleFileType.INTERNAL_AS_IFC:
      return f"internal:{self.file_name}"
    assert False, f"unknown TestModuleFileType: {self.type}"

class TestDescription:
  def __init__(self) -> None:
    self.parent: Optional[TestDescription] = None
    self.name = 'Test_name.c'
    self.remark = ''
    self.test_type = TestType.COMPILE
    self.options: List[str] = []
    self.options_all = ''
    self.options_sep = ':'
    self.name = 'Test_name.c'
    self.num_cases = 1
    self.eccp_command = 'eccp'
    self.ulimit_options = ''
    self.linker_options = ''
    self.execution_args = ''
    self.script = ''
    self.require: List[str] = []
    self.match_regex: List[str] = []
    self.source_files: List[str] = []
    self.input_files: List[str] = []
    self.header_unit_files: List[str] = []
    self.module_files: List[TestModuleFileDescription] = []
    self.use_system_includes = False
    self.edg_header_pack = ''
    self.filter = 'normalize_test_output'
    self.extra: Dict[str, List[str]] = {}

  def add_to_option_at_idx(self, idx: int, new_option: str) -> None:
    '''Add the given option to the options section at the given index (if it
    doesn't already exist).
    '''
    self.options[idx] = _append_option(self.options[idx], new_option)

  def add_to_all_options(self, new_option: str) -> None:
    '''Add the given option to options_all (if it doesn't already exist).'''
    self.options_all = _append_option(self.options_all, new_option)

  def remove_option_all_cases_regex(self,
                                    removed_option: Pattern[str]) -> None:
    self.options = (
      [_remove_option_regex(opt, removed_option) for opt in self.options]
    )
    self.options_all = _remove_option_regex(
      self.options_all, removed_option
    )

  def remove_option_all_cases(self, removed_option: str) -> None:
    # Escape the string to remove for inclusion in a regular expression.
    escaped_str = re.escape(removed_option)
    removal_regex = re.compile(fr"\s*{escaped_str}\s*")
    self.remove_option_all_cases_regex(removal_regex)

  def _get_all_options_in_context(self) -> List[str]:
    '''Get all the options including parent options (as a shell processed
    array).
    '''
    options = []
    if self.parent is not None:
      options.extend(self.parent._get_all_options_in_context())
    options.extend(shlex.split(self.options_all))
    return options

  def traverse_cases(self) -> Iterator[Tuple[TestType, List[str], List[str]]]:
    base_options = self._get_all_options_in_context()

    cases_traversed = 0
    for options_str in self.options:
      match = _TEST_TYPE_OVERRIDE_OPTION_REGEX.match(options_str)
      if match is not None:
        test_type = test_type_from_string(match.group(2))
        case_options = shlex.split(match.group(1))
      else:
        test_type = self.test_type
        case_options = shlex.split(options_str)
      yield test_type, base_options, case_options
      cases_traversed += 1

    while cases_traversed < self.num_cases:
      yield self.test_type, base_options, []
      cases_traversed += 1

  def extra_compiled_source_files(self) -> List[str]:
    def is_c_file(file_name: str) -> bool:
      _, ext = os.path.splitext(file_name)
      return ext == '.c' or ext == '.C'

    return list(filter(is_c_file, self.source_files))

  def __str__(self) -> str:
    '''Provide a string conversion operator for debugging purposes.'''
    return description_to_string(self)

_DEFAULT_DESCRIPTION = TestDescription()

#
# The following logic is for serialization of TestDescriptions.
#
# Classes are used to store the "serialize" and "deserialize" functions, but no
# objects are actually initialized of their types; they are simply "convenient
# containers."
#

def _serializer_for_basic_gen(field_name: str) -> Any:
  '''Generate a simple serializer for the given field.'''
  default_value = getattr(_DEFAULT_DESCRIPTION, field_name)

  class Serializer:
    @staticmethod
    def serialize(test_descript: TestDescription) -> Iterator[str]:
      value = getattr(test_descript, field_name)
      if value != default_value:
        yield value

    @staticmethod
    def deserialize(test_descript: TestDescription, content: str) -> None:
      if len(content) != 0:
        setattr(test_descript, field_name, content)
      else:
        setattr(test_descript, field_name, default_value)

  return Serializer

def _serializer_for_bool_gen(field_name: str) -> Any:
  '''Generate a simple boolean serializer for the given field.'''
  default_value = getattr(_DEFAULT_DESCRIPTION, field_name)

  class Serializer:
    @staticmethod
    def serialize(test_descript: TestDescription) -> Iterator[str]:
      value = getattr(test_descript, field_name)
      if value != default_value:
        yield value

    @staticmethod
    def deserialize(test_descript: TestDescription, content: str) -> None:
      lower_content = content.lower()
      if lower_content == 'true':
        setattr(test_descript, field_name, True)
      elif lower_content == 'false':
        setattr(test_descript, field_name, False)
      elif len(lower_content) != 0:
        setattr(test_descript, field_name, bool(lower_content))
      else:
        setattr(test_descript, field_name, default_value)

  return Serializer

def _serializer_for_int_gen(field_name: str) -> Any:
  '''Generate a simple integer serializer for the given field.'''
  default_value = getattr(_DEFAULT_DESCRIPTION, field_name)

  class Serializer:
    @staticmethod
    def serialize(test_descript: TestDescription) -> Iterator[str]:
      value = getattr(test_descript, field_name)
      if value != default_value:
        yield value

    @staticmethod
    def deserialize(test_descript: TestDescription, content: str) -> None:
      if len(content) != 0:
        setattr(test_descript, field_name, int(content))
      else:
        setattr(test_descript, field_name, default_value)

  return Serializer

def _serializer_for_list_gen(field_name: str) -> Any:
  '''Generate a white space separated list serializer for the given field.'''
  default_value = getattr(_DEFAULT_DESCRIPTION, field_name)

  class Serializer:
    @staticmethod
    def serialize(test_descript: TestDescription) -> Iterator[str]:
      value = getattr(test_descript, field_name)
      if value != default_value:
        yield value

    @staticmethod
    def deserialize(test_descript: TestDescription, content: str) -> None:
      setattr(test_descript, field_name, content.split())

  return Serializer

class _TestTypeSerializer:
  '''A serializer for test type values.'''
  @staticmethod
  def serialize(test_descript: TestDescription) -> Iterator[str]:
    if test_descript.test_type != _DEFAULT_DESCRIPTION.test_type:
      yield test_type_to_string(test_descript.test_type)

  @staticmethod
  def deserialize(test_descript: TestDescription, content: str) -> None:
    content = content.strip()
    if len(content) != 0:
      test_descript.test_type = test_type_from_string(content)
    else:
      test_descript.test_type = _DEFAULT_DESCRIPTION.test_type

class _TestOptionsSerializer:
  '''A serializer for test option values.'''
  @staticmethod
  def serialize(test_descript: TestDescription) -> Iterator[str]:
    options_sep = test_descript.options_sep
    if test_descript.options != _DEFAULT_DESCRIPTION.options:
      yield options_sep.join(test_descript.options)

  @staticmethod
  def deserialize(test_descript: TestDescription, content: str) -> None:
    options_sep = test_descript.options_sep
    test_descript.options = content.split(options_sep)

class _RequirementSerializer:
  '''A serializer for requirement values.'''
  @staticmethod
  def serialize(test_descript: TestDescription) -> Iterator[str]:
    if test_descript.require != _DEFAULT_DESCRIPTION.require:
      for requirement in test_descript.require:
        yield requirement

  @staticmethod
  def deserialize(test_descript: TestDescription, content: str) -> None:
    test_descript.require.append(content)

class _MatchRegexSerializer:
  '''A serializer for match_regex values.'''
  @staticmethod
  def serialize(test_descript: TestDescription) -> Iterator[str]:
    if test_descript.match_regex != _DEFAULT_DESCRIPTION.match_regex:
      for requirement in test_descript.match_regex:
        yield requirement

  @staticmethod
  def deserialize(test_descript: TestDescription, content: str) -> None:
    test_descript.match_regex.append(content)

class _ModuleFilesSerializer:
  '''A serializer for module_files values.'''
  @staticmethod
  def serialize(test_descript: TestDescription) -> Iterator[str]:
    module_file_strs = [str(x) for x in test_descript.module_files]
    if module_file_strs:
      yield ' '.join(module_file_strs)

  @staticmethod
  def deserialize(test_descript: TestDescription, content: str) -> None:
    def to_mfd(module_file_str: str) -> TestModuleFileDescription:
      if module_file_str.startswith('obj:'):
        return TestModuleFileDescription(
          module_file_str[4:],
          type = TestModuleFileType.OBJECT_ONLY
        )
      elif module_file_str.startswith('internal:'):
        return TestModuleFileDescription(
          module_file_str[9:],
          type = TestModuleFileType.INTERNAL_AS_IFC
        )
      else:
        return TestModuleFileDescription(
          module_file_str,
          type = TestModuleFileType.INTERFACE
        )

    module_files = [to_mfd(x) for x in content.split() if len(x) != 0]
    test_descript.module_files.extend(module_files)

_DESCRIPTION_SERIALIZER_MAP = {
  'name': _serializer_for_basic_gen('name'),
  'remark': _serializer_for_basic_gen('remark'),
  'type': _TestTypeSerializer,
  'options': _TestOptionsSerializer,
  'options_all': _serializer_for_basic_gen('options_all'),
  'options_sep': _serializer_for_basic_gen('options_sep'),
  'name': _serializer_for_basic_gen('name'),
  'cases': _serializer_for_int_gen('num_cases'),
  'eccp_command': _serializer_for_basic_gen('eccp_command'),
  'ulimit': _serializer_for_basic_gen('ulimit_options'),
  'linker_options': _serializer_for_basic_gen('linker_options'),
  'execution_args': _serializer_for_basic_gen('execution_args'),
  'script': _serializer_for_basic_gen('script'),
  'require': _RequirementSerializer,
  'match_regex': _MatchRegexSerializer,
  'source_files': _serializer_for_list_gen('source_files'),
  'header_unit_files': _serializer_for_list_gen('header_unit_files'),
  'module_files': _ModuleFilesSerializer,
  'use_system_includes': _serializer_for_bool_gen('use_system_includes'),
  'edg_header_pack': _serializer_for_basic_gen('edg_header_pack'),
  'filter': _serializer_for_basic_gen('filter')
}

def _serialize_description(test_descript: TestDescription) -> Iterator[str]:
  '''For the given description, yield any directive lines specifying
  non-default values.
  '''
  for key, serializer in _DESCRIPTION_SERIALIZER_MAP.items():
    for content in serializer.serialize(test_descript):
      yield f"//{key}:{content}"
  if test_descript.extra != _DEFAULT_DESCRIPTION.extra:
    for key, values in test_descript.extra.items():
      for value in values:
        yield f"//{key}:{value}"

def description_to_string(test_descript: TestDescription) -> str:
  '''Convert the given description into its textual representation (returned as
  a string).'''
  str_parts = []
  for str_part in _serialize_description(test_descript):
    str_parts.append(str_part)
  str_parts.append('') # added just to ensure a newline is added to the end
  return '\n'.join(str_parts)

def replace_description(test_descript: TestDescription,
                        file_path: Path) -> None:
  '''Replace the test description at the given file path with the given test
  description.'''
  try:
    edgutil.replace_cpp_line_directives(
      file_path,
      _serialize_description(test_descript)
    )
  except Exception as e:
    if sys.version_info >= (3, 11):
      e.add_note(
        f"Failed to replace run test description in file: {file_path}"
      )
    raise

def _deserialize_unknown_key(
                           key: str) -> Callable[[TestDescription, str], None]:
  '''Deserialize the given unknown key preserving it in the "extra" section.'''
  def deserialize(test_descript: TestDescription, content: str) -> None:
    if key not in test_descript.extra:
      test_descript.extra[key] = []
    test_descript.extra[key].append(content)

  return deserialize

def _get_deserialize_fn(key: str) -> Callable[[TestDescription, str], None]:
  '''Return the appropriate deserialization function for the given key.'''
  if key in _DESCRIPTION_SERIALIZER_MAP:
    # This is a known option, return the appropriate pre-baked function.
    return _DESCRIPTION_SERIALIZER_MAP[key].deserialize
  else:
    # This is an unknown option, return a generic handler to preserve the data.
    return _deserialize_unknown_key(key)

def _do_pop_deserialize(test_descript: TestDescription,
                        directive_map: Dict[str, List[str]],
                        key: str) -> None:
  '''If the given key is in the description map, remove it, and process it
  immediately; otherwise do nothing.
  '''
  if key not in directive_map:
    return

  deserialize_fn = _get_deserialize_fn(key)
  for value in directive_map.pop(key):
    deserialize_fn(test_descript, value)

def _form_description_from_map(
                       directive_map: Dict[str, List[str]]) -> TestDescription:
  '''Using the textual multi-map of the directives describing the test, form a
  TestDescription object with semantically relevant processing.
  '''
  test_descript = TestDescription()

  # Pop options that are required for further parsing
  for key in ['options_sep']:
    _do_pop_deserialize(test_descript, directive_map, key)

  # Handle deserializing the remaining options
  for key, values in directive_map.items():
    deserialize_fn = _get_deserialize_fn(key)
    for value in values:
      deserialize_fn(test_descript, value)

  return test_descript

def load_single_description(file_path: Path) -> TestDescription:
  '''Process the file at the given path and return its TestDescription.

  Note load_description should be preferred for tests as it will also load
  contextual (i.e., "parent") descriptions.
  '''
  try:
    directive_map = edgutil.collect_cpp_line_directives(file_path)
    return _form_description_from_map(directive_map)
  except Exception as e:
    if sys.version_info >= (3, 11):
      e.add_note(
        f"Failed to load run test description from file: {file_path}"
      )
    raise

def _parent_path_or_none(path: Path) -> Optional[Path]:
  if path.parent == path:
    return None
  return path.parent

@functools.lru_cache(maxsize = 512)
def _load_shared_flags_file(path: Path,
                            top_dir: Path) -> Optional[TestDescription]:
  parent_path = _parent_path_or_none(path)
  abs_parent = _parent_path_or_none(top_dir)
  if parent_path is not None and parent_path != abs_parent:
    parent_descript = _load_shared_flags_file(parent_path, top_dir)
  else:
    parent_descript = None

  descript_path = path / '.run_test_flags'
  if descript_path.exists():
    curr_descript = load_single_description(descript_path)
    curr_descript.parent = parent_descript
    return curr_descript

  return parent_descript

def load_description(test_path: Path, *,
                     top_dir: Optional[Path] = None) -> TestDescription:
  '''Process the test file at the given path and return its TestDescription.

  top_dir is the path to the "top" directory which may contain a
  .run_test_flags file.
  '''
  test_descript = load_single_description(test_path)

  child = test_descript

  top_dir = top_dir or Path.cwd()

  assoc_files_path = get_assoc_files_path(test_path)
  if assoc_files_path is not None:
    assoc_descript_path = assoc_files_path / 'flags.txt'
    if assoc_descript_path.exists():
      assoc_descript = load_single_description(assoc_descript_path)
      child.parent = assoc_descript
      child = assoc_descript

  child.parent = _load_shared_flags_file(test_path.parent, top_dir)

  return test_descript

TEST_STATUS_SEVERITY = {
  'PASS': 0,
  'FAIL': 1,
  'BADC': 1,
  'CATASTROPHE': 2,
  'ABORT': 3,
  'RUNTIME ABORT': 3,
  'SKIPPED': 4,
  'REQUIREMENTS NOT MET': 4,
  'MISSING COMMAND': 4
}

def is_status_change_regressive(old_status: str, new_status: str) -> bool:
  old_severity = TEST_STATUS_SEVERITY[old_status]
  new_severity = TEST_STATUS_SEVERITY[new_status]
  return new_severity > old_severity

def is_status_change_improvement(old_status: str, new_status: str) -> bool:
  if old_status == new_status:
    return False
  return not is_status_change_regressive(old_status, new_status)

_SUPPORTED_SEVERITIES_REGEX_PART = '|'.join(TEST_STATUS_SEVERITY.keys())
_SUPPLEMENTARY_STATUS_REGEX_PART = r'\(COMPILE\)|\(REGEX\)|'
_TEST_STATUS_REGEX = re.compile(
  fr"(.+):(({_SUPPORTED_SEVERITIES_REGEX_PART})"
  fr"({_SUPPLEMENTARY_STATUS_REGEX_PART})(--.*|)):"
)
_TEST_STATUS_DIFF_REGEX = re.compile(
  fr"(.) (.+) (..+:.*):(({_SUPPORTED_SEVERITIES_REGEX_PART})"
  fr"({_SUPPLEMENTARY_STATUS_REGEX_PART})(--.*|)):(.*)"
)

class ParsedTestStatus:
  def __init__(self, full_status: str, normalized_status: str,
               status_line: str) -> None:
    self.full_status = full_status
    self.normalized_status = normalized_status
    self.status_line = status_line

  def is_output_mismatch(self) -> bool:
    return 'OUTPUT MISMATCH' in self.full_status

  def has_no_previous_output(self) -> bool:
    return 'NO PREVIOUS OUTPUT' in self.full_status

def parse_test_status(status_line: str) -> Tuple[str, ParsedTestStatus]:
  try:
    test_status_match = _TEST_STATUS_REGEX.match(status_line)

    assert test_status_match is not None

    # The first group is the key, the second group is the full status, the
    # third group is the normalized status (ignoring supplementary status
    # messages following "--" and "(COMPILE)" annotations).
    key = test_status_match.group(1)
    full_status = test_status_match.group(2)
    normalized_status = test_status_match.group(3)

    return (key, ParsedTestStatus(full_status, normalized_status, status_line))
  except Exception as e:
    if sys.version_info >= (3, 11):
      e.add_note(f"while parsing:\n{status_line}\n")
    raise

class ParsedTestStatusDiff:
  def __init__(self, qualifier: str, variant: str,
               status: ParsedTestStatus, remark: str) -> None:
    self.qualifier = qualifier
    self.variant = variant
    self.status = status
    self.remark = remark

def parse_test_status_diff(
                         status_line: str) -> Tuple[str, ParsedTestStatusDiff]:
  try:
    test_status_match = _TEST_STATUS_DIFF_REGEX.match(status_line)

    assert test_status_match is not None

    # The first group is the + or -, the second group is the test name (key),
    # the third group is the test variant information, the fourth group is the
    # full status, the fifth group is the normalized status (ignoring
    # supplementary status messages following "--" and "(COMPILE)"
    # annotations), and the eight group is the remark.
    test_qualifier = test_status_match.group(1)
    test_name = test_status_match.group(2)
    test_variant = test_status_match.group(3)
    full_status = test_status_match.group(4)
    normalized_status = test_status_match.group(5)
    test_remark = test_status_match.group(8)

    return (
      test_name,
      ParsedTestStatusDiff(
        test_qualifier,
        test_variant,
        ParsedTestStatus(full_status, normalized_status, status_line),
        test_remark
      )
    )
  except Exception as e:
    if sys.version_info >= (3, 11):
      e.add_note(f"while parsing:\n{status_line}\n")
    raise

def _group_files_in_list(file_list: List[str]) -> Tuple[List[str], List[str]]:
  '''Group the given list of file names into a tuple of two lists, (default
  files, other files).'''
  default_files = []
  other_files = []
  for filename in file_list:
    if filename.startswith('default'):
      default_files.append(filename)
    else:
      other_files.append(filename)
  return (default_files, other_files)

def _get_suffix_on_default_file(filename: str) -> str:
  '''Return the suffix of a default file.

  e.g. 'default.1.2.txt' becomes '.1.2.txt'
  '''
  assert filename.startswith('default')
  return filename[len('default'):]

def _remove_duplicates(recording_dir: Path, filenames: List[str]) -> None:
  '''Remove duplicates of any default files in the list of file names.'''
  # Split things between default and non-default outputs.
  default_files, other_files = _group_files_in_list(filenames)

  for default_file in default_files:
    file_suffix = _get_suffix_on_default_file(default_file)
    duplicates_to_remove = []
    for other_file in other_files:
      # Skip files that do not have the correct suffix.
      if not other_file.endswith(file_suffix):
        continue
      # If the files are not the same, there's definitely no duplicate to
      # remove.
      if not filecmp.cmp(
        recording_dir / default_file,
        recording_dir / other_file,
        shallow = False
      ):
        continue

      duplicates_to_remove.append(other_file)
    for file_to_remove in duplicates_to_remove:
      os.remove(recording_dir / file_to_remove)
      other_files.remove(file_to_remove)

def _map_file_to_file_size(file_size_to_filename: Dict[int, List[str]],
                           recording_dir: Path, filename: str) -> None:
  # Group the recordings by size.
  file_size = os.path.getsize(recording_dir / filename)
  if file_size not in file_size_to_filename:
    file_size_to_filename[file_size] = []
  file_size_to_filename[file_size].append(filename)

def dedupe_recording_directory(recording_dir: Path, *,
                               env: Optional[Dict[str, str]] = None) -> None:
  default_config_name = get_default_config_name(env)
  default_files, other_files = _group_files_in_list(os.listdir(recording_dir))

  # Check for the case where the previous default was config "A" and the config
  # is now "B".  In this situation there are "B.1.1.txt" files that should be
  # removed as the recording should now be under "default.1.1.txt".
  for default_file in default_files:
    file_suffix = _get_suffix_on_default_file(default_file)

    old_default_files = []

    for other_file in other_files:
      # Skip files that do not have the correct suffix.
      if not other_file.endswith(file_suffix):
        continue

      file_base_name = other_file[:-len(file_suffix)]
      if file_base_name == default_config_name:
        old_default_files.append(other_file)

    # Remove matching files.
    for old_default_file in old_default_files:
      os.remove(recording_dir / old_default_file)
      other_files.remove(old_default_file)

  # Map the file size to the file name of the remaining files.
  file_size_to_filename: Dict[int, List[str]] = {}
  for filename in default_files:
    _map_file_to_file_size(file_size_to_filename, recording_dir, filename)
  for filename in other_files:
    _map_file_to_file_size(file_size_to_filename, recording_dir, filename)

  # Compare files of equivalent size for duplicates.
  for filenames in file_size_to_filename.values():
    # Skip any file sizes that don't have the same filename.
    if len(filenames) < 2:
      continue

    _remove_duplicates(recording_dir, filenames)

# ---------------------------------------------------------------------------
# Structured .rt.tar[.gz] run-test artifacts
#
# Archive layout:
#   metadata.json
#   cases/<N>/<C>/output.txt   # N=case, C=command (both 1-based);
#                              # body content only (no leading command line)
#   cases/<N>/<C>/change.diff  # only when expected≠observed
#   cases/<N>/matches/output.txt  # match_regex report when directives used
#   cases/<N>/matches/change.diff # only when matches report expected≠observed
#
# metadata.json includes top-level path (relative to top_dir's parent, so the
# top directory name remains the first component) and remark, plus a "cases"
# array. Each case has per-case status (type/variant/result) and a "commands"
# array; each command has "command" + "diff_hash" used when rendering
# combined diffs as GNU-patch # comments. When match_regex was used, the case
# also has "matches" with "diff_hash" for the case-level matches report.
#
# When the interpreter can both compress and decompress gzip tar members, the
# on-disk suffix is .rt.tar.gz (modes w:gz / r:gz); otherwise .rt.tar.
# ---------------------------------------------------------------------------

def _tarfile_supports_gz() -> bool:
  '''True if gzip compression/decompression is available for tar archives.'''
  try:
    from gzip import GzipFile  # noqa: F401
  except ImportError:
    return False
  return True

RT_TAR_GZ_SUPPORTED = _tarfile_supports_gz()

RUN_TEST_OUTPUT_SUFFIX = (
  '.rt.tar.gz' if RT_TAR_GZ_SUPPORTED else '.rt.tar'
)
RT_METADATA_VERSION = 4

def rt_tar_name_for_stem(stem: str) -> str:
  return f"{stem}{RUN_TEST_OUTPUT_SUFFIX}"

def is_rt_tar_path(path: Path) -> bool:
  return path.name.endswith(RUN_TEST_OUTPUT_SUFFIX)

def rt_tar_stem_name(path: Path) -> str:
  '''Return the file stem with the run-test suffix removed.

  e.g. foo.sft.rt.tar.gz -> foo.sft, foo.sft.rt.tar -> foo.sft.
  '''
  name = path.name
  if name.endswith(RUN_TEST_OUTPUT_SUFFIX):
    return name[:-len(RUN_TEST_OUTPUT_SUFFIX)]
  return path.stem

def hash_diff_bytes(data: bytes) -> str:
  return hashlib.sha256(data).hexdigest()

def hash_diff_text(text: str) -> str:
  return hash_diff_bytes(text.encode('utf-8', errors = 'replace'))

def hash_diff_lines(lines: List[str]) -> str:
  return hash_diff_text(''.join(lines))

def _unlink_missing_ok(path: Path) -> None:
  '''Remove ``path`` if it exists (``Path.unlink(missing_ok=True)`` on
  3.8+).'''
  if sys.version_info >= (3, 8):
    path.unlink(missing_ok = True)
  elif path.exists():
    path.unlink()

def read_combined_diff_hash(rt_file_path: Path) -> Optional[str]:
  '''Return metadata.combined_diff_hash, or None when there is no diff.'''
  with RunTestOutput(rt_file_path) as rt_output:
    return rt_output.metadata.get('combined_diff_hash')

def grouping_combined_diff_hash(rt_file_path: Path) -> str:
  '''Return a combined diff hash suitable for grouping configs.

  Missing .rt.tar files use the sentinel "missing". Archives with no
  change.diff members use the SHA-256 of empty content so they still group.
  '''
  if not rt_file_path.exists():
    return 'missing'
  combined = read_combined_diff_hash(rt_file_path)
  if combined is None:
    return hash_diff_bytes(b'')
  return combined

class DiffHashGroup(NamedTuple):
  '''Configs that share one combined_diff_hash.'''
  diff_hash: str
  configs: List[str]

def group_configs_by_diff_hash(
    config_diff_hashes: Mapping[str, str]) -> List[DiffHashGroup]:
  '''Group configuration names that share the same combined diff hash.

  config_diff_hashes maps config_name -> diff_hash. Configs within each group
  are sorted and groups are ordered by their first config name.
  '''
  groups_by_hash: Dict[str, List[str]] = OrderedDict()
  for config_name, diff_hash in config_diff_hashes.items():
    if diff_hash not in groups_by_hash:
      groups_by_hash[diff_hash] = []
    if config_name not in groups_by_hash[diff_hash]:
      groups_by_hash[diff_hash].append(config_name)

  groups = [
    DiffHashGroup(diff_hash, sorted(configs))
    for diff_hash, configs in groups_by_hash.items()
  ]
  groups.sort(key = lambda group: group.configs[0])
  return groups

def group_tests_by_diff_hash(
    test_config_diff_hashes: Mapping[str, Mapping[str, str]]
    ) -> List[Tuple[str, List[DiffHashGroup]]]:
  '''Return (test_id, config groups) pairs sorted by test name.

  test_config_diff_hashes maps test_id -> (config_name -> diff_hash). Configs
  within each test are grouped via group_configs_by_diff_hash.
  '''
  return [
    (test_id, group_configs_by_diff_hash(config_hashes))
    for test_id, config_hashes in sorted(test_config_diff_hashes.items())
  ]

def _compute_combined_diff_hash(diff_hashes: List[str]) -> Optional[str]:
  '''Hash the concatenation of per-command diff hashes, or None if empty.'''
  if len(diff_hashes) == 0:
    return None

  hasher = hashlib.sha256()
  for diff_hash in diff_hashes:
    hasher.update(diff_hash.encode('utf-8', errors = 'replace'))
  return hasher.hexdigest()

def copy_recording_for_diff(src: Path, dst: Path) -> None:
  '''Copy a recording for content diffing, omitting the command line.'''
  with open(src, 'rb') as in_handle, open(dst, 'wb') as out_handle:
    in_handle.readline()
    shutil.copyfileobj(in_handle, out_handle)

async def write_unified_diff(
    expected_file: Path,
    observed_file: Path,
    dest_diff_path: Path,
    *,
    env: Optional[Mapping[str, str]] = None) -> Optional[str]:
  '''Write an Expected/Observed unified diff and return its sha256.

  Removes dest_diff_path when the inputs match.
  '''
  async with edgutil.UnifiedDiffStream(
    expected_file,
    observed_file,
    from_label = 'Expected Output',
    to_label = 'Observed Output',
    env = dict(env) if env is not None else None
  ) as stream:
    if stream is None:
      _unlink_missing_ok(dest_diff_path)
      return None

    hasher = hashlib.sha256()
    while True:
      chunk = stream.read(65536)
      if not chunk:
        break
      hasher.update(chunk)

    stream.seek(0)
    dest_diff_path.parent.mkdir(parents = True, exist_ok = True)
    with open(dest_diff_path, 'wb') as dest_handle:
      shutil.copyfileobj(stream, dest_handle)
    return hasher.hexdigest()

async def write_content_diff(
    expected_file: Optional[Path],
    observed_file: Path,
    dest_diff_path: Path,
    *,
    env: Optional[Mapping[str, str]] = None,
    skip_expected_command_line: bool = True) -> Optional[str]:
  '''Diff expected recording vs observed body content.

  Observed output is body-only (no leading command). When
  skip_expected_command_line is True, the expected recording's first line
  (command) is omitted before comparison. expected_file None means empty
  expected content; an empty expected file is only materialized when a diff
  must be written. Returns the diff hash on mismatch, or None when equal
  (and removes dest_diff_path).
  '''
  # No expected recording and empty observed content: nothing to write.
  if expected_file is None and observed_file.stat().st_size == 0:
    _unlink_missing_ok(dest_diff_path)
    return None

  if expected_file is not None and not skip_expected_command_line:
    return await write_unified_diff(
      expected_file,
      observed_file,
      dest_diff_path,
      env = env
    )

  # Observed is already body-only; only the recording needs a temp strip
  # (or an empty expected when the recording is missing).
  with tempfile.TemporaryDirectory() as tmp_dir_name:
    expected_for_diff = Path(tmp_dir_name) / 'expected'
    if expected_file is None:
      expected_for_diff.touch()
    else:
      copy_recording_for_diff(expected_file, expected_for_diff)

    return await write_unified_diff(
      expected_for_diff,
      observed_file,
      dest_diff_path,
      env = env
    )

def case_command_from_metadata(command: Dict[str, Any]) -> str:
  '''Return the command string used for # annotations.'''
  return command.get('command') or ''

def split_unified_diff_headers(
    lines: List[str]) -> Tuple[List[str], List[str]]:
  '''Split a unified diff into (---/+++ header lines, remainder).'''
  if (len(lines) >= 2 and
      lines[0].startswith('---') and
      lines[1].startswith('+++')):
    return lines[:2], lines[2:]
  return [], lines

def rewrite_unified_diff_labels(
    lines: List[str],
    from_label: str,
    to_label: str) -> List[str]:
  '''Replace ---/+++ header labels; leave the body unchanged.'''
  header, body = split_unified_diff_headers(lines)
  if len(header) == 0:
    return list(lines)
  return [f"--- {from_label}\n", f"+++ {to_label}\n", *body]

def case_matches_annotation(case_number: int) -> str:
  '''Return the # comment text used for case-level matches diffs.'''
  return f"Matches for case {case_number}"

def format_collapsed_case_diffs(
    command_diff_entries: List[Tuple[Dict[str, Any], List[str]]],
    *,
    from_label: Optional[str] = None,
    to_label: Optional[str] = None,
    collapse: bool = True) -> List[str]:
  '''Render command diffs with GNU-patch # comments.

  When collapse is True, commands that share the same metadata diff_hash are
  emitted once (---/+++ headers, then a # comment per command, then the hunk
  body). When collapse is False, each command's full diff is emitted
  separately. When from_label/to_label are set, those replace the stored
  Expected/Observed header labels.

  Entries may use a synthetic ``command`` string such as
  ``Matches for case N`` for case-level matches diffs.
  '''
  if len(command_diff_entries) == 0:
    return []

  def emit_one(commands: List[Dict[str, Any]],
               diff_lines: List[str]) -> List[str]:
    if from_label is not None and to_label is not None:
      diff_lines = rewrite_unified_diff_labels(
        diff_lines,
        from_label,
        to_label
      )
    header, body = split_unified_diff_headers(diff_lines)
    result: List[str] = []
    result.extend(header)
    for command in commands:
      result.append(f"# {case_command_from_metadata(command)}\n")
    result.extend(body)
    return result

  if not collapse:
    result: List[str] = []
    for command, diff_lines in command_diff_entries:
      result.extend(emit_one([command], diff_lines))
    return result

  # First-seen order of unique diff bodies (keyed by stored hash).
  groups: Dict[str, Dict[str, Any]] = {}
  for command, diff_lines in command_diff_entries:
    group_key = command.get('diff_hash') or hash_diff_lines(diff_lines)
    if group_key not in groups:
      groups[group_key] = {
        'commands': [],
        'diff_lines': diff_lines
      }
    groups[group_key]['commands'].append(command)

  result = []
  for group in groups.values():
    result.extend(emit_one(group['commands'], group['diff_lines']))
  return result

class RtTarContents:
  '''Manifest of filesystem files to pack into a .rt.tar[.gz].

  Source paths need not match their archive layout; each entry records an
  explicit tar member path (e.g. cases/1/2/output.txt).
  '''

  def __init__(self) -> None:
    self._members: List[Tuple[Path, str]] = []

  def add_file(self, source: Path, arcname: str) -> None:
    '''Register source to be archived at arcname (POSIX path in the tar).'''
    assert not arcname.startswith('/'), arcname
    assert '\\' not in arcname, arcname
    self._members.append((source, arcname))

  def add_case_file(self,
                    source: Path,
                    case_number: int,
                    *path_parts: str) -> None:
    '''Register source as cases/<case_number>/<path_parts...>.'''
    assert case_number >= 1, "case numbers start at 1"
    assert len(path_parts) >= 1, "case file needs a relative path"
    for part in path_parts:
      assert part and '/' not in part and '\\' not in part, part
    arcname = '/'.join(('cases', str(case_number), *path_parts))
    self.add_file(source, arcname)

  def pack(self, metadata: Dict[str, Any], dest_path: Path) -> None:
    '''Write metadata.json and all registered members to dest_path.'''
    dest_path.parent.mkdir(parents = True, exist_ok = True)
    metadata_bytes = (
      json.dumps(metadata, indent = 2, sort_keys = True).encode('utf-8') +
      b'\n'
    )
    with tempfile.TemporaryDirectory() as tmp_dir_str:
      tmp_path = Path(tmp_dir_str) / 'archive.partial'
      with tarfile.open(
        tmp_path,
        'w:gz' if RT_TAR_GZ_SUPPORTED else 'w',
        format = tarfile.PAX_FORMAT
      ) as tar:
        metadata_info = tarfile.TarInfo(name = 'metadata.json')
        metadata_info.size = len(metadata_bytes)
        tar.addfile(metadata_info, io.BytesIO(metadata_bytes))
        for source, arcname in self._members:
          tar.add(source, arcname = arcname)
      shutil.copy2(tmp_path, dest_path)

def finalize_rt_tar(contents: RtTarContents,
                    cases: List[Dict[str, Any]],
                    dest_path: Path,
                    *,
                    path: str,
                    remark: str = '') -> None:
  '''Compute combined_diff_hash and pack contents into dest_path.

  path should be relative to top_dir's parent (including the top directory
  name as the first component). remark is the test-level remark shared across
  cases. combined_diff_hash is derived from the per-command and optional
  per-case matches diff_hash values.

  Callers that need to keep an asyncio event loop responsive should run this
  via ``edgutil.async_to_thread``.
  '''
  diff_hashes: List[str] = []
  for case in cases:
    for command in case.get('commands', []):
      diff_hash = command.get('diff_hash')
      if diff_hash is not None:
        diff_hashes.append(diff_hash)
    matches = case.get('matches')
    if matches is not None:
      diff_hash = matches.get('diff_hash')
      if diff_hash is not None:
        diff_hashes.append(diff_hash)

  metadata = {
    'version': RT_METADATA_VERSION,
    'path': path,
    'remark': remark,
    'cases': cases,
    'combined_diff_hash': _compute_combined_diff_hash(diff_hashes)
  }
  contents.pack(metadata, dest_path)

class RunTestOutputLinkError(ValueError):
  '''Raised when a run artifact member that is read is a link.'''

def _extract_tar_member(tar: tarfile.TarFile,
                        member_name: str) -> IO[bytes]:
  '''Open a run artifact member, which must be a regular file.

  tarfile resolves a symlink or hard link member to another member of the
  archive (never the file system), but links have no place in a run
  artifact, may alias members, and can form cycles that exhaust the stack;
  so they're refused (RunTestOutputLinkError) rather than followed.
  '''
  try:
    member = tar.getmember(member_name)
  except KeyError:
    raise FileNotFoundError(member_name) from None
  if member.issym() or member.islnk():
    raise RunTestOutputLinkError(
      f"{member_name} is a link to {member.linkname}"
    )
  extracted = tar.extractfile(member)
  if extracted is None:
    raise FileNotFoundError(member_name)
  return extracted

def _read_tar_lines_member(tar: tarfile.TarFile,
                           member_name: str) -> List[str]:
  '''Read a tar member as a list of lines (keepends=True).'''
  extracted = _extract_tar_member(tar, member_name)
  with io.TextIOWrapper(
    extracted,
    encoding = 'utf-8',
    errors = 'replace'
  ) as text_handle:
    return text_handle.readlines()

class RunTestOutput:
  '''Context manager for reading a structured .rt.tar run artifact once.'''

  def __init__(self, path: Path) -> None:
    self.path = path
    self._tar: Optional[tarfile.TarFile] = None
    self._metadata: Optional[Dict[str, Any]] = None
    self._member_names: Optional[Set[str]] = None

  def __enter__(self) -> 'RunTestOutput':
    self._tar = tarfile.open(
      self.path,
      'r:gz' if RT_TAR_GZ_SUPPORTED else 'r',
      format = tarfile.PAX_FORMAT
    )
    return self

  def __exit__(self, _type: Any, _value: Any, _tb: Any) -> None:
    if self._tar is not None:
      self._tar.close()
      self._tar = None

  def _require_tar(self) -> tarfile.TarFile:
    if self._tar is None:
      raise RuntimeError(
        'RunTestOutput must be used as a context manager'
      )
    return self._tar

  def _read_lines(self, member_name: str) -> List[str]:
    return _read_tar_lines_member(self._require_tar(), member_name)

  @property
  def metadata(self) -> Dict[str, Any]:
    if self._metadata is None:
      extracted = _extract_tar_member(self._require_tar(), 'metadata.json')
      with io.TextIOWrapper(
        extracted,
        encoding = 'utf-8',
        errors = 'replace'
      ) as text_handle:
        self._metadata = json.load(text_handle)
    return self._metadata

  @property
  def member_names(self) -> Set[str]:
    if self._member_names is None:
      self._member_names = set(self._require_tar().getnames())
    return self._member_names

  def command_output(self,
                     case_number: int,
                     command_number: int) -> List[str]:
    '''Return cases/<N>/<C>/output.txt lines.'''
    return self._read_lines(
      f"cases/{case_number}/{command_number}/output.txt"
    )

  def extract_command_output(self,
                             case_number: int,
                             command_number: int,
                             dest: Path) -> None:
    '''Stream cases/<N>/<C>/output.txt to dest.'''
    member = f"cases/{case_number}/{command_number}/output.txt"
    extracted = _extract_tar_member(self._require_tar(), member)
    with open(dest, 'wb') as out_handle:
      shutil.copyfileobj(extracted, out_handle)

  def command_diff(self,
                   case_number: int,
                   command_number: int) -> Optional[List[str]]:
    '''Return cases/<N>/<C>/change.diff lines, or None if absent.'''
    member = f"cases/{case_number}/{command_number}/change.diff"
    if member not in self.member_names:
      return None
    return self._read_lines(member)

  def case_matches(self, case_number: int) -> Optional[List[str]]:
    '''Return cases/<N>/matches/output.txt lines, or None if absent.'''
    member = f"cases/{case_number}/matches/output.txt"
    if member not in self.member_names:
      return None
    return self._read_lines(member)

  def extract_case_matches(self, case_number: int, dest: Path) -> None:
    '''Stream cases/<N>/matches/output.txt to dest.'''
    member = f"cases/{case_number}/matches/output.txt"
    extracted = _extract_tar_member(self._require_tar(), member)
    with open(dest, 'wb') as out_handle:
      shutil.copyfileobj(extracted, out_handle)

  def case_matches_diff(self, case_number: int) -> Optional[List[str]]:
    '''Return cases/<N>/matches/change.diff lines, or None if absent.'''
    member = f"cases/{case_number}/matches/change.diff"
    if member not in self.member_names:
      return None
    return self._read_lines(member)

  def iter_command_outputs(self) -> Iterator[List[str]]:
    '''Yield command output.txt lines in case then command order.'''
    for case_number, case in enumerate(self.metadata.get('cases', []),
                                       start = 1):
      for command_number, _command in enumerate(
        case.get('commands', []),
        start = 1
      ):
        yield self.command_output(case_number, command_number)

  def iter_command_diffs(self) -> Iterator[List[str]]:
    '''Yield change.diff lines for commands/matches that have a diff.'''
    for _entry, diff_lines in self.iter_command_diff_entries():
      yield diff_lines

  def iter_command_diff_entries(
      self) -> Iterator[Tuple[Dict[str, Any], List[str]]]:
    '''Yield (annotation metadata, change.diff lines) for diffs.

    Command diffs use the stored command metadata. Matches diffs use a
    synthetic command string ``Matches for case N``.
    '''
    for case_number, case in enumerate(self.metadata.get('cases', []),
                                       start = 1):
      for command_number, command in enumerate(
        case.get('commands', []),
        start = 1
      ):
        diff_lines = self.command_diff(case_number, command_number)
        if diff_lines is not None:
          yield command, diff_lines

      matches = case.get('matches')
      if matches is None:
        continue
      matches_diff_lines = self.case_matches_diff(case_number)
      if matches_diff_lines is None:
        continue
      matches_entry = {
        'command': case_matches_annotation(case_number),
        'diff_hash': matches.get('diff_hash')
      }
      yield matches_entry, matches_diff_lines

  def combined_static_diff(self,
                           *,
                           from_label: Optional[str] = None,
                           to_label: Optional[str] = None,
                           collapse: bool = True) -> List[str]:
    '''Combined static diffs with # command / matches comments.

    When collapse is True (default), equal diff bodies are merged.
    '''
    return format_collapsed_case_diffs(
      list(self.iter_command_diff_entries()),
      from_label = from_label,
      to_label = to_label,
      collapse = collapse
    )

  def combined_raw_output(self) -> List[str]:
    '''Concatenate command lines from metadata with each command's body.

    Reinjects cases[].commands[].command before each output.txt so review
    "Raw Output" matches the historical command+body presentation, with each
    command prefixed by ``# ``. When a matches report is present, appends it
    after the case's commands with a ``# Matches for case N`` header line.
    '''
    result: List[str] = []
    for case_number, case in enumerate(self.metadata.get('cases', []),
                                       start = 1):
      for command_number, command in enumerate(
        case.get('commands', []),
        start = 1
      ):
        command_str = case_command_from_metadata(command)
        if command_str:
          result.append(f"# {command_str}\n")
        result.extend(self.command_output(case_number, command_number))

      matches_lines = self.case_matches(case_number)
      if matches_lines is not None:
        result.append(f"# {case_matches_annotation(case_number)}\n")
        result.extend(matches_lines)
    return result
