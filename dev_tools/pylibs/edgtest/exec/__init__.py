# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import asyncio
import io
import os
import re
import shlex
import shutil
import sys
import tempfile

import edgtest
import edgutil

from enum import IntEnum
from pathlib import Path, PurePath
from typing import (
  Any,
  Awaitable,
  Callable,
  Dict,
  FrozenSet,
  List,
  Optional,
  Tuple
)

from edgtest import runtest
from edgtest.filter import (
  BuildConstants,
  GNU_MODE,
  MICROSOFT_MODE,
  Normalizer as OutputNormalizer
)

_TOOL_PATH_ECCP = shutil.which('eccp')
_TOOL_PATH_NORMALIZE = edgutil.RequiredToolPath('edg-normalize-test-output')

# Output larger than this is worth offloading to a separate process when the
# GIL is held (regex work in a thread would still serialize on the GIL).
_NONTRIVIAL_NORMALIZE_OUTPUT_BYTES = 4096

# Match --target NAME or --target=NAME in joined option strings.
_TARGET_OPTION_REGEX = re.compile(
  r'(?:^|(?<=\s))--target(?:=|\s+)(\S+)'
)

# Match, per target mode, the options that are only accepted by front ends
# built to support that mode (including abbreviations and --no_ forms) in
# joined option strings.  The Microsoft options are --microsoft...; the GNU
# options are --gnu..., --g++, --gcc..., and --clang....
_MODE_OPTION_REGEXES = {
  MICROSOFT_MODE: re.compile(
    r'(^|(?<=\s))--(no_)?microsoft\w*'
  ),
  GNU_MODE: re.compile(
    r'(^|(?<=\s))--(no_)?(gnu\w*|g\+\+|gcc\w*|clang\w*)'
  )
}

def get_output_filename(env: Dict[str, str],
                        case_number: int,
                        command_number: int) -> str:
  assert case_number >= 1, "case numbers start at 1"
  assert command_number >= 1, "command numbers start at 1"
  active_config = env['EDG_CONFIG']
  default_config = runtest.get_default_config_name(env)
  if active_config == default_config:
    return f"default.{case_number}.{command_number}.txt"
  else:
    return f"{active_config}.{case_number}.{command_number}.txt"

def get_matches_filename(env: Dict[str, str], case_number: int) -> str:
  '''Return the case-scoped match-report recording name.'''
  assert case_number >= 1, "case numbers start at 1"
  active_config = env['EDG_CONFIG']
  default_config = runtest.get_default_config_name(env)
  if active_config == default_config:
    return f"default.{case_number}.matches.txt"
  return f"{active_config}.{case_number}.matches.txt"

def find_exisiting_output_file(recording_dir: Path,
                               output_filename: str,
                               case_number: int,
                               command_number: int) -> Optional[Path]:
  specialized_case_file = recording_dir / output_filename
  if specialized_case_file.exists():
    return specialized_case_file

  default_case_file = (
    recording_dir / f"default.{case_number}.{command_number}.txt"
  )
  if default_case_file.exists():
    return default_case_file

  return None

def find_existing_matches_file(recording_dir: Path,
                               matches_filename: str,
                               case_number: int) -> Optional[Path]:
  specialized_case_file = recording_dir / matches_filename
  if specialized_case_file.exists():
    return specialized_case_file

  default_case_file = recording_dir / f"default.{case_number}.matches.txt"
  if default_case_file.exists():
    return default_case_file

  return None

def get_command(test_descript: runtest.TestDescription,
                test_type: runtest.TestType) -> Tuple[List[str], str]:
  if test_descript.script:
    return ([test_descript.script], test_descript.script)

  if runtest.is_front_end_test_type(test_type):
    return ([_TOOL_PATH_ECCP, '--cpfe_only'], 'fe_only')
  if runtest.is_link_test_type(test_type):
    return ([_TOOL_PATH_ECCP], 'eccp')
  return ([_TOOL_PATH_ECCP, '-c'], 'cpfe')

class OutputComparison(IntEnum):
  EQUAL        = 0
  MISMATCH     = 1
  PREV_MISSING = 3

def read_output_command_line(output_file: Path) -> str:
  '''Return the first line of a recording (the command), or "".'''
  with open(output_file, 'rb') as file_handle:
    first_line = file_handle.readline()
  return first_line.decode('utf-8', errors = 'replace').rstrip('\r\n')

def write_recording_file(dest: Path, command: str, body_file: Path) -> None:
  '''Write a recording as command line + observed body content.'''
  dest.parent.mkdir(parents = True, exist_ok = True)
  with open(dest, 'wb') as out_handle, open(body_file, 'rb') as body_handle:
    out_handle.write(f"{command}\n".encode('utf-8'))
    shutil.copyfileobj(body_handle, out_handle)

async def check_output_mismatch(
    old_output_file: Optional[Path],
    new_output_file: Path,
    env: Dict[str, str],
    change_diff_path: Path) -> Tuple[OutputComparison, Optional[str]]:
  '''Compare expected recording vs observed body output.

  Recordings start with a command line that is omitted for comparison;
  observed output is body-only. On mismatch write a unified diff to
  change_diff_path and return (comparison, sha256 hex of the diff bytes).
  On equality, do not create change_diff_path and return (EQUAL, None) —
  or (PREV_MISSING, None) when no expected recording exists and content
  matches empty.
  '''
  diff_hash = await edgtest.runtest.write_content_diff(
    old_output_file,
    new_output_file,
    change_diff_path,
    env = env,
    skip_expected_command_line = True
  )
  if old_output_file is None:
    return OutputComparison.PREV_MISSING, diff_hash
  if diff_hash is None:
    return OutputComparison.EQUAL, None
  return OutputComparison.MISMATCH, diff_hash

def get_test_status_by_code(return_code: int, *,
                            expect_failure: bool,
                            expect_catastrophe: bool) -> str:
  if return_code == 129:
    return 'BADC'
  if return_code > 127:
    return 'ABORT'
  if return_code > 2:
    if expect_catastrophe:
      return 'PASS'
    else:
      return 'CATASTROPHE'
  if expect_catastrophe and return_code != 2:
    return 'FAIL'

  if expect_failure:
    return 'FAIL' if return_code == 0 else 'PASS'
  else:
    return 'FAIL' if return_code != 0 else 'PASS'

def _get_base_status(test_type: runtest.TestType, compile_return_code: int,
                     exec_return_code: Optional[int]) -> str:
  expect_catastrophe = runtest.is_catastrophe_test_type(test_type)
  expect_failure = not runtest.is_positive_test_type(test_type)

  if not runtest.is_executed_test_type(test_type):
    return get_test_status_by_code(
      compile_return_code,
      expect_failure = expect_failure,
      expect_catastrophe = expect_catastrophe
    )

  if exec_return_code is None:
    return get_test_status_by_code(
      compile_return_code,
      expect_failure = False,
      expect_catastrophe = False
    )

  if exec_return_code > 127:
    if test_type == runtest.TestType.RUNTIME_ABORT:
      return 'PASS'
    else:
      return 'RUNTIME ABORT'

  if expect_failure:
    return 'FAIL' if exec_return_code == 0 else 'PASS'
  else:
    return 'FAIL' if exec_return_code != 0 else 'PASS'

def _get_supplementary_status(test_type: runtest.TestType,
                              compile_return_code: int,
                              exec_return_code: Optional[int], *,
                              missing_matches: bool = False) -> str:
  if runtest.is_executed_test_type(test_type):
    if exec_return_code is None:
      return '(COMPILE)'
  return ''

def get_test_status(test_type: runtest.TestType, compile_return_code: int,
                    exec_return_code: Optional[int], *,
                    missing_matches: bool = False) -> str:
  base_status = _get_base_status(
    test_type,
    compile_return_code,
    exec_return_code
  )
  supplementary_status = _get_supplementary_status(
    test_type,
    compile_return_code,
    exec_return_code,
    missing_matches = missing_matches
  )
  if base_status == 'PASS' and missing_matches:
    return 'FAIL(REGEX)'
  return f"{base_status}{supplementary_status}"

def _str_for_output_cmp_result(output_cmp_result: OutputComparison) -> str:
  if output_cmp_result == OutputComparison.EQUAL:
    return ''
  if output_cmp_result == OutputComparison.MISMATCH:
    return '--OUTPUT MISMATCH'
  return '--NO PREVIOUS OUTPUT'

def make_status_message_with_explicit_status(test_path: Path,
                                             test_type: runtest.TestType,
                                             test_remark: str,
                                             case_tag: str,
                                             status: str) -> str:
  test_type_str = runtest.test_type_to_string(test_type)
  return f"{test_path}:{test_type_str}:{case_tag}:{status}:{test_remark}"

def make_requirements_not_met_status(test_path: Path,
                                     test_type: runtest.TestType,
                                     test_remark: str,
                                     case_tag: str = '') -> str:
  '''Status line for a test or case whose build requirements are unmet.'''
  return make_status_message_with_explicit_status(
    test_path,
    test_type,
    test_remark,
    case_tag,
    'REQUIREMENTS NOT MET'
  )

def make_skipped_status(test_path: Path,
                        test_type: runtest.TestType,
                        test_remark: str,
                        case_tag: str = '') -> str:
  '''Status line for a case marked SKIP_TEST.'''
  return make_status_message_with_explicit_status(
    test_path,
    test_type,
    test_remark,
    case_tag,
    'SKIPPED'
  )

def _case_options_tag(case_number: int, cmd_case_opts: List[str]) -> str:
  '''Format the case variant tag used in status messages.'''
  return ' '.join([
    f"-DTEST_NUMBER={case_number}",
    *cmd_case_opts
  ])

def make_status_fields(test_path: Path, test_type: runtest.TestType,
                       test_remark: str,
                       compile_return_code: int,
                       exec_return_code: Optional[int],
                       cmd_case_options: List[str], case_number: int, *,
                       output_cmp_result: OutputComparison,
                       missing_matches: bool) -> Tuple[str, Dict[str, str]]:
  '''Return (colon status string, per-case status fields for metadata).

  Per-case fields are type/variant/result only. Test path and remark are
  test-level and stored at the top of metadata.json by finalize_rt_tar.
  '''
  status = get_test_status(
    test_type,
    compile_return_code,
    exec_return_code,
    missing_matches = missing_matches
  )
  mismatch_text = _str_for_output_cmp_result(output_cmp_result)
  options_str = _case_options_tag(case_number, cmd_case_options)
  result = f"{status}{mismatch_text}"
  test_type_str = runtest.test_type_to_string(test_type)
  status_message = make_status_message_with_explicit_status(
    test_path,
    test_type,
    test_remark,
    options_str,
    result
  )
  return status_message, {
    'type': test_type_str,
    'variant': options_str,
    'result': result
  }

def make_status_message(test_path: Path, test_type: runtest.TestType,
                        test_remark: str,
                        compile_return_code: int,
                        exec_return_code: Optional[int],
                        cmd_case_options: List[str], case_number: int, *,
                        output_cmp_result: OutputComparison,
                        missing_matches: bool) -> str:
  status_message, _ = make_status_fields(
    test_path,
    test_type,
    test_remark,
    compile_return_code,
    exec_return_code,
    cmd_case_options,
    case_number,
    output_cmp_result = output_cmp_result,
    missing_matches = missing_matches
  )
  return status_message

DRIVER_DEBUG_REGEX = re.compile(r'^driver debug: (.*)\n$')
_ENV_VAR_REGEX = re.compile(r'\$([A-Z0-9_]+)')
_IL_DISP_REGEX = re.compile(r'--il( |_d)')

def expand_vars(arg: str, env: Dict[str, str]) -> str:
  result = arg
  for a_match in _ENV_VAR_REGEX.finditer(arg):
    var_name = a_match.group(1)
    if var_name in env:
      result = result.replace(f"${var_name}", env[var_name])
  return result

async def invoke_recorded_process(
                    base_command: List[str], command_args: List[str],
                    working_dir: Path, env: Dict[str, str],
                    build_constants: BuildConstants,
                    output_file_handle, *,
                    filter_command: Optional[str] = None, debug: bool,
                    driver_debug: bool = False,
                    injected_command_args: List[str] = None) -> int:
  # If in driver debug mode, inject an additional argument to for driver.
  injected_command_args = (
    injected_command_args.copy() if injected_command_args else []
  )
  if driver_debug:
    injected_command_args.append('--verbose_driver_debug')

  # Expand the command line arguments.
  cmd_line = [*base_command, *injected_command_args, *command_args]
  cmd_line[:] = [expand_vars(x, env) for x in cmd_line]

  if debug:
    print(f"Recording command: {shlex.join(cmd_line)}")

  with tempfile.TemporaryFile(mode='w+b') as test_process_output:
    # Launch the tested process.
    tested_process = await asyncio.create_subprocess_exec(
      *cmd_line,
      cwd = working_dir,
      env = env,
      stdout = asyncio.subprocess.PIPE,
      stderr = asyncio.subprocess.STDOUT,
      limit = sys.maxsize
    )

    # Now (if necessary) perform driver debug mode post processing on the file
    # output to extract the driver debug output from the output stream.
    async for raw_line in tested_process.stdout:
      if driver_debug:
        line = raw_line.decode()
        match_object = DRIVER_DEBUG_REGEX.match(line)
        if match_object is not None:
          match_content = match_object.group(1)
          print(match_content)
          continue
      test_process_output.write(raw_line)

    # Make sure the process terminates.
    await tested_process.wait()

    # Make sure we're at the start of the file.
    test_process_output.seek(0)

    # The test_process_output is now ready for filtering (regardless of whether
    # this is just the pipe from the process output or it is now a true
    # file). Launch the filter process (if needed) and ensure only filtered
    # output is written.
    if filter_command == 'noop':
      # Pass output through unchanged (avoids pathological regex cases).
      if debug:
        print("Applying filter: noop")
      shutil.copyfileobj(test_process_output, output_file_handle)
    elif filter_command and filter_command != 'normalize_test_output':
      filter_cmd_line = ['/usr/bin/bash', '-c', filter_command]
      if debug:
        print(f"Applying filter: {shlex.join(filter_cmd_line)}")
      filter_process = await asyncio.create_subprocess_exec(
        *filter_cmd_line,
        cwd = working_dir,
        env = env,
        stdin = test_process_output,
        stdout = output_file_handle,
        stderr = asyncio.subprocess.STDOUT
      )
      await filter_process.wait()
    else:
      # No filtering command was used, use the builtin filtering.
      # Generate a command args string for easy replacement rule generation.
      command_args_str = ' '.join(command_args)
      normalize_il = _IL_DISP_REGEX.search(command_args_str) is not None

      test_process_output.seek(0, io.SEEK_END)
      output_nbytes = test_process_output.tell()
      test_process_output.seek(0)

      # Under a GIL-bound interpreter, heavy regex normalization in a worker
      # thread still serializes on the GIL.  For non-trivial output, run the
      # normalizer in a separate process instead so other tests can progress.
      use_normalize_process = (
        edgutil.python_gil_enabled()
        and output_nbytes >= _NONTRIVIAL_NORMALIZE_OUTPUT_BYTES
      )

      normalize_cmd = [str(_TOOL_PATH_NORMALIZE)]
      if normalize_il:
        normalize_cmd.append('--il')
      if debug:
        print(f"Applying filter: {shlex.join(normalize_cmd)}")

      if use_normalize_process:
        test_process_output.flush()
        filter_process = await asyncio.create_subprocess_exec(
          *normalize_cmd,
          cwd = working_dir,
          env = env,
          stdin = test_process_output,
          stdout = output_file_handle,
          stderr = asyncio.subprocess.PIPE
        )
        _, stderr_data = await filter_process.communicate()
        if filter_process.returncode != 0:
          raise edgutil.GILAvoidanceProcessException(
            normalize_cmd,
            filter_process.returncode,
            stderr = stderr_data.decode('utf-8', errors = 'replace')
          )
      else:
        def background_process_lines():
          normalizer = OutputNormalizer(
            normalize_il = normalize_il,
            env = env,
            build_constants = build_constants
          )

          for line in test_process_output:
            try:
              output_file_handle.write(
                normalizer.process_line(line.decode()).encode()
              )
            except ValueError:
              # If UTF decoding fails, revert to a binary line write.
              output_file_handle.write(line)

        await edgutil.async_to_thread(background_process_lines)

  return tested_process.returncode

async def invoke_tested_process(test_descript: runtest.TestDescription,
                                test_type: runtest.TestType,
                                command_args: List[str],
                                injected_command_args: List[str],
                                working_dir: Path,
                                env: Dict[str, str],
                                build_constants: BuildConstants,
                                output_file_handle, *,
                                debug: bool,
                                driver_debug: bool = False,
                                faster_mode: bool = False
                                ) -> Tuple[int, str]:
  '''Run a tested process; write body output only (no command line).

  Returns (return_code, command_args_str).
  '''
  exec_cmd, generic_command = get_command(test_descript, test_type)

  command_args_str = ' '.join([generic_command, *command_args])

  injected_command_args = (
    injected_command_args.copy() if injected_command_args else []
  )
  injected_command_args.append('--no_wrap_diagnostics')
  if faster_mode:
    injected_command_args.append('--set_flag=no_very_expensive_checking')

  # If --il_display is used, checking pragma variance between faster and
  # non-faster modes can cause output divergence.  Do not inject checking
  # pragmas in --il_display tests.
  if _IL_DISP_REGEX.search(command_args_str) is not None:
    injected_command_args.append('--set_flag=no_checking_pragmas')

  return_code = await invoke_recorded_process(
    exec_cmd,
    command_args,
    working_dir,
    env,
    build_constants,
    output_file_handle,
    filter_command = test_descript.filter,
    debug = debug,
    driver_debug = driver_debug,
    injected_command_args = injected_command_args
  )
  return return_code, command_args_str

def get_output_file_path(output_dir: Path, test_path: Path) -> Path:
  return output_dir / edgtest.runtest.rt_tar_name_for_stem(test_path.stem)

def verify_regex_matches(body_files: List[Path],
                         match_expressions: List[str],
                         matches_file: Path) -> bool:
  '''Apply match_expressions to the joined command bodies.

  Writes a human-readable match report to matches_file (body only; no
  leading command line). Returns True if any expression has no match.
  '''
  any_missing = False
  test_output_parts: List[str] = []
  for body_file in body_files:
    with open(
      body_file,
      'r',
      encoding = 'utf-8',
      errors = 'replace'
    ) as body_handle:
      test_output_parts.append(body_handle.read())
  test_output = ''.join(test_output_parts)

  matches_file.parent.mkdir(parents = True, exist_ok = True)
  with open(
    matches_file,
    'w',
    encoding = 'utf-8',
    errors = 'replace'
  ) as matches_handle:
    for expr in match_expressions:
      expr_matches = re.finditer(
        expr,
        test_output,
        re.MULTILINE
      )
      match_found = False
      for expr_match in expr_matches:
        if not match_found:
          match_found = True
          matches_handle.write(f"Matches for \"{expr}\":\n")
        matches_handle.write(f"  <{'-' * 77}\n")
        for match_line in expr_match.group(0).split('\n'):
          matches_handle.write(f"  {match_line}\n")
        matches_handle.write(f"  {'-' * 77}>\n")

      if not match_found:
        any_missing = True
        matches_handle.write(f"No matches for \"{expr}\"!\n")

  return any_missing

def aggregate_output_comparison(
    results: List[OutputComparison]) -> OutputComparison:
  '''Combine per-command comparisons into a case-level result.'''
  if any(result == OutputComparison.MISMATCH for result in results):
    return OutputComparison.MISMATCH
  if any(result == OutputComparison.PREV_MISSING for result in results):
    return OutputComparison.PREV_MISSING
  return OutputComparison.EQUAL

def update_command_recording(
    *,
    case_number: int,
    command_number: int,
    command_str: str,
    body_file: Path,
    test_recordings: Path,
    env: Dict[str, str],
    rt_contents: runtest.RtTarContents,
    command_body_files: List[Path],
    command_metadata: List[Dict[str, Any]]) -> None:
  '''Write the updated recording and register the body in rt_contents.'''
  write_recording_file(
    test_recordings / get_output_filename(env, case_number, command_number),
    command_str,
    body_file
  )
  rt_contents.add_case_file(
    body_file,
    case_number,
    str(command_number),
    'output.txt'
  )
  command_body_files.append(body_file)
  command_metadata.append({
    'command': command_str,
    'diff_hash': None
  })

async def process_command_mismatch(
    *,
    case_number: int,
    command_number: int,
    command_str: str,
    body_file: Path,
    test_recordings: Path,
    env: Dict[str, str],
    rt_contents: runtest.RtTarContents,
    command_body_files: List[Path],
    command_metadata: List[Dict[str, Any]]
    ) -> OutputComparison:
  '''Diff observed body vs recording; register members in rt_contents.'''
  output_filename = get_output_filename(env, case_number, command_number)
  old_output_file = find_exisiting_output_file(
    test_recordings,
    output_filename,
    case_number,
    command_number
  )
  change_diff_path = body_file.parent / 'change.diff'
  output_cmp_result, diff_hash = await check_output_mismatch(
    old_output_file,
    body_file,
    env,
    change_diff_path
  )
  rt_contents.add_case_file(
    body_file,
    case_number,
    str(command_number),
    'output.txt'
  )
  command_body_files.append(body_file)
  if change_diff_path.exists():
    rt_contents.add_case_file(
      change_diff_path,
      case_number,
      str(command_number),
      'change.diff'
    )

  command_metadata.append({
    'command': command_str,
    'diff_hash': diff_hash
  })
  return output_cmp_result

def update_matches_recording(
    *,
    case_number: int,
    matches_file: Path,
    test_recordings: Path,
    env: Dict[str, str],
    rt_contents: runtest.RtTarContents,
    matches_metadata: Dict[str, Any]) -> None:
  '''Copy the observed match report into the recordings directory.'''
  dest = test_recordings / get_matches_filename(env, case_number)
  dest.parent.mkdir(parents = True, exist_ok = True)
  shutil.copyfile(matches_file, dest)
  rt_contents.add_case_file(
    matches_file,
    case_number,
    'matches',
    'output.txt'
  )
  matches_metadata['diff_hash'] = None

async def process_matches_mismatch(
    *,
    case_number: int,
    matches_file: Path,
    test_recordings: Path,
    env: Dict[str, str],
    rt_contents: runtest.RtTarContents,
    matches_metadata: Dict[str, Any]
    ) -> OutputComparison:
  '''Diff observed match report vs recording; register matches members.'''
  matches_filename = get_matches_filename(env, case_number)
  old_matches_file = find_existing_matches_file(
    test_recordings,
    matches_filename,
    case_number
  )
  change_diff_path = matches_file.parent / 'change.diff'
  diff_hash = await edgtest.runtest.write_content_diff(
    old_matches_file,
    matches_file,
    change_diff_path,
    env = env,
    skip_expected_command_line = False
  )
  rt_contents.add_case_file(
    matches_file,
    case_number,
    'matches',
    'output.txt'
  )
  if change_diff_path.exists():
    rt_contents.add_case_file(
      change_diff_path,
      case_number,
      'matches',
      'change.diff'
    )

  if old_matches_file is None:
    output_cmp_result = OutputComparison.PREV_MISSING
  elif diff_hash is None:
    output_cmp_result = OutputComparison.EQUAL
  else:
    output_cmp_result = OutputComparison.MISMATCH
  matches_metadata['diff_hash'] = diff_hash
  return output_cmp_result

async def run_test_with_options(test_descript: runtest.TestDescription,
                                test_path: Path, test_recordings: Path,
                                output_dir: Path, working_dir: Path,
                                env: Dict[str, str],
                                build_constants: BuildConstants,
                                test_type: runtest.TestType,
                                cmd_options: List[str],
                                injected_cmd_options: List[str],
                                cmd_case_options: List[str],
                                case_number: int, *,
                                case_artifact_dir: Path,
                                rt_contents: runtest.RtTarContents,
                                rt_metadata_cases: List[Dict[str, Any]],
                                debug: bool,
                                driver_debug: bool,
                                faster_mode: bool,
                                update_recording: bool,
                                quiet_results: bool) -> Optional[str]:
  common_cmd_args = []
  common_cmd_args.append(f"-DTEST_NUMBER={case_number}")
  common_cmd_args.extend(cmd_options)
  common_cmd_args.extend(cmd_case_options)

  command_number = 0
  command_body_files: List[Path] = []
  command_metadata: List[Dict[str, Any]] = []
  command_comparisons: List[OutputComparison] = []
  matches_metadata: Optional[Dict[str, Any]] = None
  compile_return_code = 0
  exec_return_code: Optional[int] = None

  async def finish_command(command_str: str,
                           body_file: Path) -> None:
    if update_recording:
      update_command_recording(
        case_number = case_number,
        command_number = command_number,
        command_str = command_str,
        body_file = body_file,
        test_recordings = test_recordings,
        env = env,
        rt_contents = rt_contents,
        command_body_files = command_body_files,
        command_metadata = command_metadata
      )
      command_comparisons.append(OutputComparison.EQUAL)
    else:
      comparison = await process_command_mismatch(
        case_number = case_number,
        command_number = command_number,
        command_str = command_str,
        body_file = body_file,
        test_recordings = test_recordings,
        env = env,
        rt_contents = rt_contents,
        command_body_files = command_body_files,
        command_metadata = command_metadata
      )
      command_comparisons.append(comparison)

  async def record_command_as_case(
      run_command: Callable[[Any], Awaitable[Tuple[int, str]]]
      ) -> int:
    '''Record one command case around a caller-provided process invocation.

    Opens an output file and passes the writable handle to ``run_command``,
    which must write process output and return ``(return_code, command_str)``.
    Diff/recording is applied afterward; the file is registered in
    rt_contents as cases/<N>/<C>/output.txt.
    '''
    nonlocal command_number
    command_number += 1
    cmd_dir = case_artifact_dir / str(command_number)
    cmd_dir.mkdir(parents = True, exist_ok = True)
    body_file = cmd_dir / 'output.txt'
    with open(body_file, 'wb') as output_file_handle:
      return_code, command_str = await run_command(output_file_handle)
      output_file_handle.flush()
    await finish_command(command_str, body_file)
    return return_code

  async def run_tested_process(
      output_file_handle: Any,
      cmd_args: List[str]) -> Tuple[int, str]:
    return await invoke_tested_process(
      test_descript,
      test_type,
      cmd_args,
      injected_cmd_options,
      working_dir,
      env,
      build_constants,
      output_file_handle,
      debug = debug,
      driver_debug = driver_debug,
      faster_mode = faster_mode
    )

  async def run_executed_process(
      output_file_handle: Any,
      cmd_args: List[str]) -> Tuple[int, str]:
    return_code = await invoke_recorded_process(
      cmd_args[:1],
      cmd_args[1:],
      working_dir,
      env,
      build_constants,
      output_file_handle,
      filter_command = test_descript.filter,
      debug = debug
    )
    return return_code, ' '.join(cmd_args)

  used_header_unit_args = []
  for header_unit_file in test_descript.header_unit_files:
    rt_mod_file = f"{header_unit_file}.rtmod"
    cmd_args = common_cmd_args.copy()
    cmd_args.append(f"--create_header_unit={rt_mod_file}")
    cmd_args.append(header_unit_file)

    await record_command_as_case(
      lambda h, cmd_args = cmd_args: run_tested_process(h, cmd_args)
    )

    used_header_unit_args.insert(0, '--header_unit')
    used_header_unit_args.insert(1, f"{header_unit_file}={rt_mod_file}")

  module_obj_files = []
  for module_file in test_descript.module_files:
    cmd_args = common_cmd_args.copy()

    MFT = runtest.TestModuleFileType
    if module_file.type == MFT.INTERFACE:
      cmd_args.append('--module_interface')
    elif module_file.type == MFT.OBJECT_ONLY:
      cmd_args.append('-c')
    elif module_file.type == MFT.INTERNAL_AS_IFC:
      cmd_args.append('--module_internal_partition')
    cmd_args.append(module_file.file_name)

    obj_file_name = str(PurePath(module_file.file_name).with_suffix('.o'))
    module_obj_files.append(obj_file_name)

    await record_command_as_case(
      lambda h, cmd_args = cmd_args: run_tested_process(h, cmd_args)
    )

  command_args = common_cmd_args.copy()
  command_args.extend(used_header_unit_args)

  # Do not include the module objects if this is not a linking or runtime
  # test.  This avoids spurious link errors for objects that aren't expected
  # to exist.
  if runtest.is_link_test_type(test_type):
    command_args.extend(module_obj_files)

  command_args.append(test_descript.name)
  for source_file in test_descript.extra_compiled_source_files():
    command_args.append(source_file)

  compile_return_code = await record_command_as_case(
    lambda h, cmd_args = command_args: run_tested_process(h, cmd_args)
  )

  if compile_return_code == 0 and runtest.is_executed_test_type(test_type):
    exec_cmd_args = [
      './a.out',
      *shlex.split(test_descript.execution_args)
    ]
    exec_return_code = await record_command_as_case(
      lambda h, cmd_args = exec_cmd_args: run_executed_process(h, cmd_args)
    )

  missing_matches = False
  if len(test_descript.match_regex) != 0:
    # Moving this to its own thread ensures that the overall calling program
    # isn't fully blocked.  In free-threaded Python builds, this should also
    # be significantly more performant for test suites with heavy regex use.
    matches_file = case_artifact_dir / 'matches' / 'output.txt'
    missing_matches = await edgutil.async_to_thread(
      verify_regex_matches,
      command_body_files,
      test_descript.match_regex,
      matches_file
    )
    matches_metadata = {}
    if update_recording:
      update_matches_recording(
        case_number = case_number,
        matches_file = matches_file,
        test_recordings = test_recordings,
        env = env,
        rt_contents = rt_contents,
        matches_metadata = matches_metadata
      )
      command_comparisons.append(OutputComparison.EQUAL)
    else:
      comparison = await process_matches_mismatch(
        case_number = case_number,
        matches_file = matches_file,
        test_recordings = test_recordings,
        env = env,
        rt_contents = rt_contents,
        matches_metadata = matches_metadata
      )
      command_comparisons.append(comparison)

  assert command_number >= 1
  output_cmp_result = aggregate_output_comparison(command_comparisons)

  status_message, status_fields = make_status_fields(
    test_path,
    test_type,
    test_descript.remark,
    compile_return_code,
    exec_return_code,
    cmd_case_options,
    case_number,
    output_cmp_result = output_cmp_result,
    missing_matches = missing_matches
  )

  case_entry: Dict[str, Any] = {
    'status': status_fields,
    'commands': command_metadata
  }
  if matches_metadata is not None:
    case_entry['matches'] = matches_metadata
  rt_metadata_cases.append(case_entry)

  # Check to see if test result should be silenced.
  if quiet_results:
    status = get_test_status(
      test_type,
      compile_return_code,
      exec_return_code,
      missing_matches = missing_matches
    )
    if output_cmp_result == OutputComparison.EQUAL and status == 'PASS':
      return None

  return status_message

def meets_requirements(config_dump: str,
                       requirements: List[str]) -> bool:
  for requirement in requirements:
    if requirement not in config_dump:
      return False
  return True

def _case_requests_unsupported_target(
    cmd_opts: List[str],
    cmd_case_opts: List[str],
    target_configurations: FrozenSet[str]
) -> bool:
  '''True if any --target in base/case options is missing from the build.'''
  options_str = ' '.join([*cmd_opts, *cmd_case_opts])
  for target in _TARGET_OPTION_REGEX.findall(options_str):
    if target not in target_configurations:
      return True
  return False

def _case_requests_unsupported_mode(
    cmd_opts: List[str],
    cmd_case_opts: List[str],
    target_modes: FrozenSet[str]
) -> bool:
  '''True if any base/case option needs a target mode the build lacks.'''
  options_str = ' '.join([*cmd_opts, *cmd_case_opts])
  for mode, option_regex in _MODE_OPTION_REGEXES.items():
    if mode not in target_modes and option_regex.search(options_str):
      return True
  return False

async def run_test(test_path: Path, *, top_dir: Path, output_dir: Path,
                   env: Dict[str, str], build_constants: BuildConstants,
                   faster_mode: bool, update_recording: bool, debug_mode: bool,
                   driver_debug_mode: bool,
                   quiet_results: bool = True) -> List[str]:
  try:
    test_descript = runtest.load_description(test_path, top_dir = top_dir)

    test_recordings = edgtest.runtest.get_assoc_recordings_path(test_path)
    if update_recording:
      test_recordings.mkdir(parents = True, exist_ok = True)

    # Set up a test output directory. The test output directory is relative to
    # the output directory matching the test file's position relative to the
    # top dir. In other words:
    #
    #   top_dir / a / b / Test.c
    #
    # should become:
    #
    #   output_dir / a / b / Test.rt.tar
    #
    test_output_dir = output_dir / test_path.relative_to(top_dir).parent
    test_output_dir.mkdir(parents = True, exist_ok = True)

    # Make a copy of the env dict to make sure the input isn't being modified.
    env = dict(env)

    # Update RUN_TEST_CURR_DIR for variable substitution in command line
    # arguments.
    env['RUN_TEST_CURR_DIR'] = str(top_dir)

    # Enable assertion line number suppression if not already set.
    if 'EDG_SUPPRESS_ASSERTION_LINE_NUMBER' not in env:
      env['EDG_SUPPRESS_ASSERTION_LINE_NUMBER'] = '1'

    # If this test uses system includes, tell the edg_eccp_config to enable
    # whatever system headers are necessary.
    if test_descript.use_system_includes:
      env['EDG_USE_SYSTEM_HEADERS'] = '1'

    # Set EDG_TRANSLATION_UNIT_TAG to the test file name (path) so that when
    # testing daemon mode, the test case being executed on the current daemon
    # thread can be easily identified.
    env['EDG_TRANSLATION_UNIT_TAG'] = str(test_path)

    # Print information about how to clone the source files.
    if driver_debug_mode:
      print('Source file clone: ')
      if env.get('SHELL', '').endswith('fish'):
        print(f"cd (clone-test-to-tmp {test_path})")
      else:
        print(f"cd $(clone-test-to-tmp {test_path})")

    # Remove the existing artifact (if any).
    output_file_path = get_output_file_path(test_output_dir, test_path)
    output_file_path.unlink(missing_ok = True)

    test_results = []
    case_traversal = test_descript.traverse_cases()
    requirements_met = meets_requirements(
      build_constants.config_dump,
      test_descript.require
    )
    with tempfile.TemporaryDirectory() as tmp_dir_name:
      tmp_dir = Path(tmp_dir_name)
      rt_metadata_cases: List[Dict[str, Any]] = []
      rt_contents = runtest.RtTarContents()

      for case_number, (mode, cmd_opts, cmd_case_opts) in (
        enumerate(case_traversal, start = 1)
      ):
        # Create a copy of the env for this case.
        env = dict(env)

        if mode == runtest.TestType.SKIP_TEST:
          test_results.append(make_skipped_status(
            test_path,
            mode,
            test_descript.remark,
            _case_options_tag(case_number, cmd_case_opts)
          ))
          continue

        if not requirements_met:
          # Emit one REQUIREMENTS NOT MET line per case so expectations see
          # every case variant that would have run.
          test_results.append(make_requirements_not_met_status(
            test_path,
            mode,
            test_descript.remark,
            _case_options_tag(case_number, cmd_case_opts)
          ))
          continue

        if mode == runtest.TestType.BACK_END_CPP_CODEGEN:
          # If this is a test of the C++-generating back end; check that
          # this front end is actually a C++-generating build.
          if not meets_requirements(
            build_constants.config_dump,
            ['BACK_END_IS_CP_GEN_BE 1']
          ):
            test_results.append(make_requirements_not_met_status(
              test_path,
              mode,
              test_descript.remark,
              _case_options_tag(case_number, cmd_case_opts)
            ))
            continue

          # Tell cpfe-cp-gen-be to dump the generated code.
          env['EDG_TEST_DUMP_GENERATED_CODE'] = '1'

        if _case_requests_unsupported_target(
          cmd_opts,
          cmd_case_opts,
          build_constants.target_configurations
        ):
          # Case requests a --target that this front end was not built with.
          test_results.append(make_requirements_not_met_status(
            test_path,
            mode,
            test_descript.remark,
            _case_options_tag(case_number, cmd_case_opts)
          ))
          continue

        if _case_requests_unsupported_mode(
          cmd_opts,
          cmd_case_opts,
          build_constants.target_modes
        ):
          # Case requests a Microsoft or GNU mode that this front end was not
          # built to support.
          test_results.append(make_requirements_not_met_status(
            test_path,
            mode,
            test_descript.remark,
            _case_options_tag(case_number, cmd_case_opts)
          ))
          continue

        case_artifact_dir = tmp_dir / str(case_number)
        case_artifact_dir.mkdir()

        exec_tmp_dir = edgtest.ExecutionTempDir(
          test_path,
          keep_files = debug_mode
        )
        async with exec_tmp_dir as tmp_execution:
          working_dir = tmp_execution.temp_dir

          # Update TMPDIR so that scripts like normalize_test_output can
          # observe it.
          env['TMPDIR'] = str(working_dir)

          if test_descript.edg_header_pack:
            header_pack_dir = (
              top_dir / '.includes' / test_descript.edg_header_pack
            )
            injected_cmd_opts = [
              '--sys_include',
              str(header_pack_dir),
            ]
          else:
            injected_cmd_opts = []

          # Set the AddressSanitizer to use exit code 128 so ASan errors show
          # as ABORTs.
          env['ASAN_OPTIONS'] = f"exitcode=139:log_path={working_dir}/asan"

          status_message = await run_test_with_options(
            test_descript,
            test_path,
            test_recordings,
            test_output_dir,
            working_dir,
            env,
            build_constants,
            mode,
            cmd_opts,
            injected_cmd_opts,
            cmd_case_opts,
            case_number,
            case_artifact_dir = case_artifact_dir,
            rt_contents = rt_contents,
            rt_metadata_cases = rt_metadata_cases,
            debug = debug_mode,
            driver_debug = driver_debug_mode,
            faster_mode = faster_mode,
            update_recording = update_recording,
            quiet_results = quiet_results
          )
          if status_message is not None:
            test_results.append(status_message)
          if debug_mode:
            print(f"Temporary files kept at: {working_dir}")

      if len(rt_metadata_cases) > 0:
        # Metadata path is relative to top_dir's parent so the top directory
        # name (e.g. the suite) remains the first path component.
        await edgutil.async_to_thread(
          edgtest.runtest.finalize_rt_tar,
          rt_contents,
          rt_metadata_cases,
          output_file_path,
          path = test_path.relative_to(top_dir.parent).as_posix(),
          remark = test_descript.remark
        )

    if update_recording:
      runtest.dedupe_recording_directory(test_recordings, env = env)
  except Exception as e:
    if sys.version_info >= (3, 11):
      e.add_note(f"test file: {test_path}")
    raise

  return test_results
