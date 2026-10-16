# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import asyncio
import re

import edgdocker
import edgutil

from datetime import datetime, timedelta
from pathlib import Path
from typing import Dict, FrozenSet, Set

_TARGET_CONFIGURATION_DEFINE_REGEX = re.compile(
  r'^#define\s+TARGET_CONFIGURATION_\d+\s+(\S+)\s*$',
  re.MULTILINE
)
# The legacy / default host target is a valid --target name but is not
# numbered as TARGET_CONFIGURATION_<N>.
_LEGACY_TARGET_CONFIGURATION_NAME_REGEX = re.compile(
  r'^#define\s+LEGACY_TARGET_CONFIGURATION_NAME\s+"([^"]*)"',
  re.MULTILINE
)

def _check_config_enables_macro(config_dump: str, macro: str) -> bool:
  '''True if the dumped configuration defines macro as enabled (1 or TRUE).

  A macro that the dump reports as "not defined" is treated as disabled.
  '''
  return re.search(
    rf'#define[ \t]+{re.escape(macro)}[ \t]+(1|TRUE)\s',
    config_dump
  ) is not None

# Target modes whose command-line options are only accepted by front ends
# built with the corresponding configuration macro.
MICROSOFT_MODE = 'microsoft'
GNU_MODE = 'gnu'

_TARGET_MODE_CONFIG_MACROS = {
  MICROSOFT_MODE: 'MICROSOFT_EXTENSIONS_ALLOWED',
  GNU_MODE: 'GNU_EXTENSIONS_ALLOWED'
}

def _parse_target_modes(config_dump: str) -> FrozenSet[str]:
  '''Collect the target modes (e.g., MICROSOFT_MODE) this build supports.'''
  return frozenset(
    mode for mode, macro in _TARGET_MODE_CONFIG_MACROS.items()
    if _check_config_enables_macro(config_dump, macro)
  )

def _parse_target_configurations(config_dump: str) -> FrozenSet[str]:
  '''Collect --target names supported by this front-end build.'''
  targets: Set[str] = set(
    _TARGET_CONFIGURATION_DEFINE_REGEX.findall(config_dump)
  )
  legacy_match = _LEGACY_TARGET_CONFIGURATION_NAME_REGEX.search(config_dump)
  if legacy_match is not None and legacy_match.group(1):
    targets.add(legacy_match.group(1))
  return frozenset(targets)

class BuildConstants:
  def __init__(self, version: str, build_timestamp: str,
               config_dump: str,
               target_configurations: FrozenSet[str],
               target_modes: FrozenSet[str]) -> None:
    self.version = version
    self.build_timestamp = build_timestamp
    self.config_dump = config_dump
    self.target_configurations = target_configurations
    self.target_modes = target_modes

async def load_build_constants(compiler_bin: Path) -> BuildConstants:
  config_dump_proc = await asyncio.create_subprocess_exec(
    str(compiler_bin), '--dump_config',
    stdout = asyncio.subprocess.PIPE,
    stderr = asyncio.subprocess.STDOUT
  )
  version_dump_proc = await asyncio.create_subprocess_exec(
    str(compiler_bin), '--version',
    stdout = asyncio.subprocess.PIPE,
    stderr = asyncio.subprocess.STDOUT
  )

  config_dump_encoded, _ = await config_dump_proc.communicate()
  version_dump_encoded, _ = await version_dump_proc.communicate()

  version_info_match = re.search(
    r'version (.*) \((.*)\)',
    version_dump_encoded.decode()
  )
  assert version_info_match is not None

  version_number = version_info_match.group(1)
  build_date = version_info_match.group(2)
  config_dump = config_dump_encoded.decode()
  return BuildConstants(
    version_number,
    build_date,
    config_dump,
    _parse_target_configurations(config_dump),
    _parse_target_modes(config_dump)
  )

_REGEX_REPLACEMENTS = {
  # IL-style address annotations.
  r'@[0-9a-f]{3,}': '@XXXXXXXX',
  r'@0x[0-9a-f]{3,}': '@XXXXXXXX',
  # IL display of a name that is not in the file scope memory region.
  r'\*\*possible non-file-scope ptr\*\* \(0x[0-9a-f]+\)':
      '**possible non-file-scope ptr** (0xXX)',
  # Linker section offsets that move with codegen layout; keep the .text
  # marker so symbol-blame changes remain visible.
  r'\.text\+0x[0-9a-f]+': '.text+0xXX',
  r'__T[0-9]{7,}': '__TXXXXXXXXX',
  r'.*/cpfe-cp-gen-be: line [0-9]+:\s+[0-9]+ Aborted.*': 'Aborted'
}
_ESCAPED_REGEX_REPLACEMENTS = {
  re.escape(k): v for k, v in {
   'AddressSanitizer:DEADLYSIGNAL': 'Segmentation fault',
   ' (core dumped)': '',
   '"./cpfe-cp-gen-be/Test_name.c"': '"Test_name.c"'
  }.items()
}
GLOBAL_REGEX_REPLACEMENTS = {
  **_REGEX_REPLACEMENTS,
  **_ESCAPED_REGEX_REPLACEMENTS
}

# Stack / high canonical user addresses that vary across hosts and ASLR.
# Match both %p (0x-prefixed) and %x (bare) forms.  The 0x form must be listed
# first so it wins over the bare form.
_STACK_ADDR_REPLACEMENTS = (
  (r'0x(?:7fffffff[0-9a-f]+|ffff[0-9a-f]+)', '0xSTACK'),
  (r'(?<![0-9a-fx])(?:7fffffff[0-9a-f]+|ffff[0-9a-f]+)', 'STACK'),
)

class Normalizer:
  def __init__(self, *, normalize_il: bool, stack_addr_cleanup: bool = False,
               env: Dict[str, str],
               build_constants: BuildConstants) -> None:
    self.replacements = list(GLOBAL_REGEX_REPLACEMENTS.items())
    self.replacements.extend([
      (
        re.escape(f"{env['TMPDIR']}/"),
        ''
      ),
      (
        re.escape(env['RUN_TEST_CURR_DIR']),
        Path(env['RUN_TEST_CURR_DIR']).name
      )
    ])
    mono_repo_dir = edgutil.try_find_mono_repo(
      Path(env['RUN_TEST_CURR_DIR'])
    )
    if mono_repo_dir is not None:
      self.replacements.append((
        re.escape(mono_repo_dir.as_posix()),
        '/edg/workspace'
      ))

    if stack_addr_cleanup:
      self.replacements.extend(_STACK_ADDR_REPLACEMENTS)

    if normalize_il:
      # Detect the compiler version string.
      self.replacements.append(
        (
          f"compiler_version:.*?: \"{re.escape(build_constants.version)}\"",
          'compiler_version:        IL COMPILER VERSION'
        ),
      )
      # Detect the time_of_compilation string and expect something either
      # today or yesterday.
      curr_date = datetime.now()
      self.replacements.append(
        (
          f"time_of_compilation:.*?: \".*?{curr_date.day}.*?"
          f"{curr_date.year}.*?\"",
          'time_of_compilation:     IL BUILD TIME'
        ),
      )
      hr_ago = curr_date - timedelta(hours = 1)
      if curr_date.date() != hr_ago.date():
        # Enable the check for yesterday since an hour ago was a different
        # calendar day.
        self.replacements.append(
          (
            f"time_of_compilation:.*?: \".*?{hr_ago.day}.*?"
            f"{hr_ago.year}.*?\"",
            'time_of_compilation:     IL BUILD TIME'
          ),
        )

    replacement_groups = [f"({r})" for r, _ in self.replacements]
    self.normalization_regex = re.compile(f"{'|'.join(replacement_groups)}")

  def process_line(self, line: str) -> str:
    return self.normalization_regex.sub(
      lambda re_match: self.replacements[re_match.lastindex - 1][1],
      line
    )
