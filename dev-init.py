#!/usr/bin/env python3

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# This script is designed to be a cross platform friendly project setup
# utility.
#
# It is not allowed to depend on anything other than the Python standard
# library.

import argparse
import asyncio
import inspect
import json
import os
import platform
import re
import shutil
import sys

from enum import Enum
from pathlib import Path
from typing import Any, Awaitable, List, Optional, Tuple

# This is a bit of a sniff test to determine if the script should
# require user input before exiting.
#
if platform.system() == 'Windows':
  # Use the kernel API to detect if this script is running in a standalone
  # console window (if so, there will be only one processes at this point).
  import ctypes

  # WinDLL is Windows-only; stubs omit it when the analysis platform is not
  # Windows.
  kernel32 = ctypes.WinDLL(  # type: ignore[attr-defined]
    'kernel32', use_last_error = True
  )
  process_array = (ctypes.c_uint * 2)()
  num_processes = kernel32.GetConsoleProcessList(process_array, 2)
  SCRIPT_HAS_DEDICATED_WINDOW = num_processes == 1
else:
  SCRIPT_HAS_DEDICATED_WINDOW = False

class InitStatus(Enum):
  OK        = 1
  FAIL      = 2
  ATTENTION = 3

COLOR_SUPPORTED = not os.environ.get('NO_COLOR')

def color_green(text: str) -> str:
  if COLOR_SUPPORTED:
    return f"\033[32m{text}\033[0m"
  else:
    return text

def color_red(text: str) -> str:
  if COLOR_SUPPORTED:
    return f"\033[31m{text}\033[0m"
  else:
    return text

def color_yellow(text: str) -> str:
  if COLOR_SUPPORTED:
    return f"\033[33m{text}\033[0m"
  else:
    return text

def init_status_to_str(status: InitStatus) -> str:
  if status == InitStatus.OK:
    return f"[{color_green('OKAY')}]"
  if status == InitStatus.FAIL:
    return f"[{color_red('FAIL')}]"
  if status == InitStatus.ATTENTION:
    return f"[{color_yellow('ALRT')}]"
  return '[????]'

class InitResult:
  def __init__(self, task: str) -> None:
    self.status = InitStatus.OK
    self.task = task
    self.failure_commentary: List[Tuple[str, bool]] = []
    self.tool_output: Optional[str] = None
    self.sub_results: List['InitResult'] = []

  def add_failure_commentary(self, commentary: str,
                             *, allow_wrap = True) -> None:
    self.failure_commentary.append((commentary, allow_wrap))

  def add_sub_task(self, task: str) -> 'InitResult':
    sub_task = InitResult(task)
    self.sub_results.append(sub_task)
    return sub_task

PROJECT_ROOT = Path(__file__).parent

TOOL_PATH_CMAKE = shutil.which('cmake')
TOOL_PATH_NINJA = shutil.which('ninja')
TOOL_PATH_DOCKER = shutil.which('docker')

async def require_tool(tool_name: str, tool_path: Optional[str], *,
                       fail_okay = False) -> InitResult:
  result = InitResult(f"Verify tool install: {tool_name}")

  if not tool_path:
    result.status = InitStatus.ATTENTION if fail_okay else InitStatus.FAIL

    if fail_okay:
      result.add_failure_commentary(
        f"{tool_name} is not required, but highly recommended"
      )

  return result

async def build_dev_tools() -> InitResult:
  result = InitResult('Build: dev_tools')

  # Configure CMake for the dev_tools.
  configure_cmake = result.add_sub_task('Configure Build')

  assert TOOL_PATH_CMAKE is not None

  env = os.environ.copy()
  if TOOL_PATH_NINJA:
    env['CMAKE_GENERATOR'] = 'Ninja'

  configure_proc = await asyncio.create_subprocess_exec(
    TOOL_PATH_CMAKE,
    '-DCMAKE_BUILD_TYPE=Release',
    # Explicitly specify the install prefix in case configuration fails.  If
    # that happens, CMake (at the time of writing) does not respect the logic
    # in dev_tools/CMakeLists.txt for configuring the default prefix under
    # CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT; instead, it reverts the
    # install prefix to the default location.
    f"-DCMAKE_INSTALL_PREFIX={PROJECT_ROOT / 'dev_tools'}",
    '-B', 'build',
    '.',
    env = env,
    cwd = PROJECT_ROOT / 'dev_tools',
    stdout = asyncio.subprocess.PIPE,
    stderr = asyncio.subprocess.STDOUT
  )
  stdout, _ = await configure_proc.communicate()
  configure_cmake.tool_output = stdout.decode()
  if configure_proc.returncode != 0:
    result.status = InitStatus.FAIL
    configure_cmake.status = InitStatus.FAIL
    return result

  # Build the dev_tools.
  build_cmake = result.add_sub_task('Run Build')
  build_proc = await asyncio.create_subprocess_exec(
    TOOL_PATH_CMAKE,
    '--build', 'build',
    '--target', 'install',
    cwd = PROJECT_ROOT / 'dev_tools',
    stdout = asyncio.subprocess.PIPE,
    stderr = asyncio.subprocess.STDOUT
  )
  stdout, _ = await build_proc.communicate()
  build_cmake.tool_output = stdout.decode()
  if build_proc.returncode != 0:
    result.status = InitStatus.FAIL
    build_cmake.status = InitStatus.FAIL
    return result

  return result

def _install_git_hook(parent_result: InitResult, hook_name: str) -> None:
  parent_result.add_sub_task(f"Install: {hook_name} hook")

  prepush_script_path = PROJECT_ROOT / '.git-hooks' / f"{hook_name}.py"
  prepush_hook_path = PROJECT_ROOT / '.git' / 'hooks' / hook_name

  if platform.system() == 'Windows':
    script_drive_letter = prepush_script_path.drive.replace(':', '')
    script_path_parts = [script_drive_letter, *prepush_script_path.parts[1:]]

    # On Windows, create an execution shim.
    with open(prepush_hook_path, 'w') as file_handle:
      script_path_str = f"/{'/'.join(script_path_parts)}"
      file_handle.write(f"#!/bin/sh\nexec python.exe {script_path_str}\n")
  else:
    # On Unix, just directly dispatch to the Python script.
    if prepush_hook_path.exists():
      # Unlink if it already exists.
      prepush_hook_path.unlink()
    prepush_hook_path.symlink_to(prepush_script_path)

async def install_git_hooks() -> InitResult:
  result = InitResult('Install git hooks')

  _install_git_hook(result, 'pre-push')

  return result

async def check_edg_python() -> InitResult:
  result = InitResult('Check: EDG Python tools & libs available')

  # Find the test script.
  tools_bin_name = ('win-bin' if platform.system() == 'Windows' else 'bin')
  find_edgy = result.add_sub_task(f"Find tools in dev_tools/{tools_bin_name}")

  edgy_path = shutil.which('edgy')

  script_path = PROJECT_ROOT / 'dev_tools' / tools_bin_name

  if not edgy_path:
    result.status = InitStatus.FAIL
    find_edgy.status = InitStatus.FAIL

    if platform.system() == 'Windows':
      find_edgy.add_failure_commentary(
        'Open the Windows environment variable settings:'
      )
      find_edgy.add_failure_commentary(
        '  "C:\\Windows\\system32\\rundll32.exe" '
        'sysdm.cpl,EditEnvironmentVariables',
        allow_wrap = False
      )
      find_edgy.add_failure_commentary(
        'Please edit or create the Path environment variable '
        '(normally prefer a "User variable").'
      )
      find_edgy.add_failure_commentary('Add the following to the variable:')
      find_edgy.add_failure_commentary(f"  {script_path}", allow_wrap = False)
      find_edgy.add_failure_commentary(
        'then open a new terminal window or tab.'
      )
    else:
      if not shutil.which('direnv'):
        find_edgy.add_failure_commentary(
          'Consider installing direnv; otherwise, add:'
        )
        find_edgy.add_failure_commentary(str(script_path), allow_wrap = False)
        find_edgy.add_failure_commentary('to your PATH.')
      else:
        find_edgy.add_failure_commentary(
          'direnv is installed, did you type "direnv allow"?'
        )

    return result

  script_name = ('edgy.bat' if platform.system() == 'Windows' else 'edgy')
  expected_edgy_path = script_path / script_name
  if expected_edgy_path != Path(edgy_path).resolve():
    result.status = InitStatus.FAIL
    find_edgy.status = InitStatus.FAIL

    find_edgy.add_failure_commentary(f"Expected to find {script_name} at:")
    find_edgy.add_failure_commentary(
      str(expected_edgy_path), allow_wrap = False
    )
    find_edgy.add_failure_commentary(f"Instead found {script_name} at:")
    find_edgy.add_failure_commentary(edgy_path, allow_wrap = False)
    find_edgy.add_failure_commentary('Please update the path')

    return result

  find_edg_tools = result.add_sub_task('Find libraries in dev_tools/pylibs')
  pylibs_path = PROJECT_ROOT / 'dev_tools' / 'pylibs'

  try:
    import edgtools  # type: ignore[import-not-found,import]
  except ImportError:
    result.status = InitStatus.FAIL
    find_edg_tools.status = InitStatus.FAIL

    if platform.system() == 'Windows':
      find_edg_tools.add_failure_commentary(
        'Open the Windows environment variable settings:'
      )
      find_edg_tools.add_failure_commentary(
        '  "C:\\Windows\\system32\\rundll32.exe" '
        'sysdm.cpl,EditEnvironmentVariables',
        allow_wrap = False
      )
      find_edg_tools.add_failure_commentary(
        'Please edit or create the PYTHONPATH environment variable '
        '(normally prefer a "User variable").'
      )
      find_edg_tools.add_failure_commentary(
        'Add the following to the variable:'
      )
      find_edg_tools.add_failure_commentary(
        f"  {pylibs_path}",
        allow_wrap = False
      )
      find_edg_tools.add_failure_commentary(
        'then open a new terminal window or tab.'
      )
    else:
      if shutil.which('direnv'):
        find_edg_tools.add_failure_commentary(
          'Consider installing direnv; otherwise, add:'
        )
        find_edg_tools.add_failure_commentary(
          str(pylibs_path), allow_wrap = False
        )
        find_edg_tools.add_failure_commentary('to your PYTHONPATH.')
      else:
        find_edg_tools.add_failure_commentary(
          'direnv is installed, did you type "direnv allow"?'
        )

    return result

  expected_edg_tools = pylibs_path / 'edgtools'
  edgtools_source = inspect.getsourcefile(edgtools)
  assert edgtools_source is not None
  actual_edg_tools = Path(edgtools_source).parent
  if expected_edg_tools != actual_edg_tools:
    result.status = InitStatus.FAIL
    find_edg_tools.status = InitStatus.FAIL

    find_edgy.add_failure_commentary(f"Expected to find edgtools at:")
    find_edgy.add_failure_commentary(
      str(expected_edg_tools),
      allow_wrap = False
    )
    find_edgy.add_failure_commentary(f"Instead found edgtools at:")
    find_edgy.add_failure_commentary(str(actual_edg_tools), allow_wrap = False)
    find_edgy.add_failure_commentary('Please update the path')

    return result

  return result

_WHITESPACE_SPLIT_REGEX = re.compile(r'\s+')

def _wrap_message(msg: str, available_space: int) -> List[str]:
  words = _WHITESPACE_SPLIT_REGEX.split(msg)

  lines = []
  cur_line = words[0]
  words = words[1:]

  for word in words:
    new_line_len = len(word) + 1 + len(cur_line)
    if new_line_len > available_space:
      lines.append(cur_line)
      cur_line = word
      continue
    cur_line += f" {word}"

  if len(cur_line) != 0:
    lines.append(cur_line)

  return lines

def print_init_result(result: InitResult, terminal_width: int,
                      indent_level: int = 0) -> None:
  # Calculate the opener length manually to avoid interference from
  # color codes:
  #
  #   (2 * indent_level) + status string length (6) + 1 space
  #
  opener_len = 2 * indent_level + 6 + 1
  opener = f"{'  ' * indent_level}{init_status_to_str(result.status)} "
  print(opener, end = '')

  # Calculate how much space is available for wrapped messages.
  available_space = terminal_width - opener_len

  for idx, msg_line in enumerate(_wrap_message(result.task, available_space)):
    if idx == 0:
      print(msg_line)
    else:
      print(f"{' ' * opener_len}{msg_line}")

  if result.status != InitStatus.OK:
    for commentary, allow_wrap in result.failure_commentary:
      print('')
      if allow_wrap:
        for msg_line in _wrap_message(commentary, available_space):
          print(f"{' ' * opener_len}{msg_line}")
      else:
        print(f"{' ' * opener_len}{commentary}")
    if result.tool_output:
      print('=' * terminal_width)
      print(result.tool_output)
      print('=' * terminal_width)
    for sub_result in result.sub_results:
      print_init_result(sub_result, terminal_width, indent_level + 1)

def exit_program(exit_code: int) -> None:
  if SCRIPT_HAS_DEDICATED_WINDOW:
    input('Press Enter to continue...')
  sys.exit(exit_code)

async def run_group(*tasks: Awaitable[InitResult]) -> None:
  init_results = await asyncio.gather(*tasks)

  terminal_width, _ = shutil.get_terminal_size()
  exit_at_end = False
  for init_result in init_results:
    if init_result.status == InitStatus.FAIL:
      exit_at_end = True
    print_init_result(init_result, terminal_width)
  if exit_at_end:
    exit_program(1)

dev_pref_file = PROJECT_ROOT / 'dev-pref.json'

def load_dev_pref() -> Any:
  # Load or construct a default map for the dev prefs.
  if dev_pref_file.exists():
    with open(dev_pref_file) as file_handle:
     prefs = json.load(file_handle)
  else:
    prefs = {}

  # Apply defaults if not set.
  if 'env_type' not in prefs:
    prefs['env_type'] = 'docker'

  return prefs

def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
    description = (
      'Quickly configure and setup the development environment.'
    )
  )
  parser.add_argument(
    'env_type',
    choices = ['docker', 'native'],
    nargs = '?',
    help = 'the environment type (defaults to docker)'
  )
  return parser.parse_args()

async def main() -> None:
  args = parse_args()
  dev_prefs = load_dev_pref()

  # Look for command line overrides.
  if args.env_type is not None:
    dev_prefs['env_type'] = args.env_type

  if dev_prefs['env_type'] == 'docker':
    await run_group(
      require_tool('docker', TOOL_PATH_DOCKER)
    )
    await run_group(
      check_edg_python()
    )
  else:
    await run_group(
      require_tool('cmake', TOOL_PATH_CMAKE),
      require_tool('ninja', TOOL_PATH_NINJA, fail_okay = True)
    )
    await run_group(
      build_dev_tools(),
      check_edg_python()
    )
  await run_group(
    install_git_hooks()
  )

  # Environment configured, save the dev_prefs.
  with open(dev_pref_file, 'w') as file_handle:
    json.dump(dev_prefs, file_handle)

loop = asyncio.new_event_loop()
loop.run_until_complete(main())

exit_program(0)
