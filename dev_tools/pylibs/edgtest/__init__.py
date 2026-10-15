# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import asyncio
import os
import re
import shutil
import sys
import tempfile

import edgutil

from enum import Enum
from pathlib import Path, PurePosixPath
from typing import Any, Dict, Iterator, List, Set, Optional, Tuple

from edgtest import runtest

def _copytree_exist_ok(src: Path, dst: Path, **kwargs: Any) -> None:
  '''``shutil.copytree`` with ``dirs_exist_ok=True`` semantics (3.8+).'''
  if sys.version_info >= (3, 8):
    shutil.copytree(src, dst, dirs_exist_ok = True, **kwargs)
    return

  ignore = kwargs.get('ignore')
  if not dst.exists():
    shutil.copytree(src, dst, **kwargs)
    return

  names = os.listdir(src)
  ignored: Set[str] = set()
  if ignore is not None:
    ignored = set(ignore(str(src), names))
  for name in names:
    if name in ignored:
      continue
    src_name = src / name
    dst_name = dst / name
    if src_name.is_dir():
      _copytree_exist_ok(src_name, dst_name, **kwargs)
    else:
      shutil.copy2(src_name, dst_name)

def load_test_home_or_exit(*, verbose: bool = False) -> Path:
  # Verify EDG_TEST_HOME points at a directory.
  if 'EDG_TEST_HOME' in os.environ:
    test_home = Path(os.environ['EDG_TEST_HOME'])

    if not test_home.is_dir():
      edgutil.eprint(
        f"\"EDG_TEST_HOME\" is set ({test_home}) but is not a valid "
        'directory on this system.'
      )
      sys.exit(1)
  else:
    test_home = edgutil.find_mono_repo_or_exit() / 'tests'

  if not (test_home / '.edgy' / 'config.json').exists():
    edgutil.eprint(
      f"\"EDG_TEST_HOME\" is set to a valid directory ({test_home}), "
      'but it doesn\'t look like a checkout of the tests repo.\n'
      f"Should contain an \".edgy/config.json\" file."
    )
    sys.exit(1)

  # Print test home value in verbose mode.
  if verbose:
    if 'EDG_TEST_HOME' in os.environ:
      edgutil.print_env_var('EDG_TEST_HOME')

  return test_home

def get_test_runs_in(runs_dir: Path, *,
                     include_replays: bool = False) -> List[Path]:
  '''Get a list of all subdirectories of the test runs folder with the most
  recently created first. If include_replays is True replays will be included
  in the list in their correct chronological position.
  '''
  if not runs_dir.exists():
    return []

  sorted_run_dirs = edgutil.sorted_runs_in_dir(runs_dir)
  if not include_replays:
    return sorted_run_dirs

  combined_run_dirs = []
  for sorted_run_dir in sorted_run_dirs:
    replay_dir = sorted_run_dir / 'replays'
    if replay_dir.exists():
      sorted_replay_dirs = [
        d for d in replay_dir.iterdir() if d.is_dir() and d.name.isdigit()
      ]
      sorted_replay_dirs.sort(
        key = lambda d: int(d.name),
        reverse = True
      )
      combined_run_dirs.extend(sorted_replay_dirs)
    combined_run_dirs.append(sorted_run_dir)
  return combined_run_dirs

def get_test_runs(test_home: Path, *,
                  include_replays: bool = False) -> List[Path]:
  '''Get a list of all subdirectories of the test runs folder with the most
  recently created first. If include_replays is True replays will be included
  in the list in their correct chronological position.
  '''
  runs_dir = test_home / 'runs'
  return get_test_runs_in(runs_dir, include_replays = include_replays)

class TestRunInspection:
  '''This class contains information about a test comparison that's both
  computed and obtained from the user's command line arguments.
  '''
  def __init__(self, root_path: Optional[Path], num_runs_back: int,
               previous_run_dirs: List[Path]) -> None:
    self.root_path = root_path
    self.num_runs_back = num_runs_back
    self.previous_run_dirs = previous_run_dirs

  def is_previous_run_search(self) -> bool:
    return self.root_path is None

  def get_run_directory(self) -> Path:
    '''Return the path to walk for .rt.tar files.'''
    if self.is_previous_run_search():
      # Using -b flags and implicit root directory resolution.
      return self.previous_run_dirs[self.num_runs_back]
    else:
      # Using an explicit root directory.
      assert self.root_path is not None
      return self.root_path

def validate_inspection(inspection: TestRunInspection) -> None:
  '''This function validates the inspection before use (giving a user facing
  error if there is a problem).
  '''
  if inspection.is_previous_run_search():
    # Using -b flags and implicit root directory resolution.
    num_previous_runs = len(inspection.previous_run_dirs)
    if num_previous_runs == 0:
      # If there are no previous runs, handle the "default" (i.e., no -b flag)
      # case.
      edgutil.eprint('no previous runs are available')
      sys.exit(1)
    if inspection.num_runs_back >= num_previous_runs:
      # There are multiple runs available but not as many as the user
      # requested. Add one to the run count as no -b flags (i.e.,
      # inspection.num_runs_back) is still 1 run ago.
      edgutil.eprint(
        f"only {num_previous_runs} previous runs are available, "
        f"cannot examine {inspection.num_runs_back + 1} runs ago"
      )
      sys.exit(1)
  else:
    # Using an explicit root directory.
    if inspection.num_runs_back != 0:
      edgutil.eprint('-b cannot be combined with --root')
      sys.exit(1)

def _is_test_file(file_path: Path, test_file_extensions: Set[str]) -> bool:
  # Check to make sure the test isn't a "hidden file." (Typically this is
  # something like an emacs temporary file that didn't/hasn't been cleaned up).
  if file_path.name.startswith('.'):
    return False

  # Check test extension.
  file_extensions = ''.join(file_path.suffixes)
  if file_extensions not in test_file_extensions:
    return False

  return True

def _add_run_my_tests_env_vars(suite_env: Dict[str, str]) -> None:
  # Fallback to LC_ALL=C explicitly, as some tests don't work without this.
  if 'LC_ALL' not in suite_env:
    suite_env['LC_ALL'] = 'C'

  # If CDISP isn't present, some tests expect it to be set to the directory
  # containing CPFE.
  if 'CDISP' not in suite_env and 'CPFE' in suite_env:
    suite_env['CDISP'] = os.path.dirname(suite_env['CPFE'])

def load_test_suite_env(test_suite_dir: Path) -> Dict[str, str]:
  '''Return the environment tests in the given suite run with.

  Suites used to be able to customize this with a `.run_test_env` shell
  script, but sourcing it executes arbitrary code from the test tree (a
  problem when serving untrusted pull requests), and no suite uses one, so
  the environment is now always inherited.
  '''
  suite_env = os.environ.copy()

  if 'RUN_TEST_DEFAULT_CONFIG' not in suite_env:
    suite_env['RUN_TEST_DEFAULT_CONFIG'] = runtest.get_default_config_name()

  # Emulate "quirks" of the run_my_tests script.
  _add_run_my_tests_env_vars(suite_env)

  return suite_env

DEFAULT_TEST_EXTENSIONS = (
  {'sft.cpp', 'sft.c', 'mft.cpp', 'mft.c'}
)

def test_extensions_for_suite(test_home: Path,
                              suite_name: str) -> Set[str]:
  '''Return a list of file extensions that should be considered when searching
  for test files in the given suite.'''
  suite_path = test_home / 'tests' / suite_name
  suite_env = load_test_suite_env(suite_path)
  if 'RUN_TEST_SUFFIXES' in suite_env:
    return set(suite_env['RUN_TEST_SUFFIXES'].split(' '))
  return DEFAULT_TEST_EXTENSIONS

def walk_test_directory(
            directory: Path,
            extensions: Set[str] = DEFAULT_TEST_EXTENSIONS) -> Iterator[Path]:
  '''Traverse the file system tree of the given directory yielding any tests
  with matching file extensions.
  '''
  extensions_set = set(map(lambda ext: f".{ext}", extensions))
  for root, dirs, filenames in os.walk(directory):
    # Remove hidden directories
    dirs[:] = [d for d in dirs if not d.startswith('.')]

    for filename in filenames:
      file_path = Path(root, filename)
      if not _is_test_file(file_path, extensions_set):
        continue

      yield file_path

def _scan_changes_file(changes_file: Path) -> Iterator[Path]:
  '''Yield the .rt.tar file paths expected based on the contents of the given
  changes file.'''
  path_strs = []

  # Collect the list of expected rt.tar files.
  with open(changes_file, 'r') as file_handle:
    path_strs = file_handle.readlines()

  # Sort to make things deterministic.
  path_strs.sort()

  for path_str in path_strs:
    rt_file_path = (
      changes_file.parent /
      PurePosixPath(path_str).with_suffix(runtest.RUN_TEST_OUTPUT_SUFFIX)
    )
    if rt_file_path.exists():
      yield rt_file_path

class DiffDiscMode(Enum):
  # Display any change.
  ANY_CHANGE              = 1
  # Display only changes acknowledged by edgy.
  EDGY_CHANGES            = 2
  # Use the EDGY_CHANGES algorithm unless no changes files can be discoverd, in
  # which case revert to ANY_CHANGE (the default).
  ANY_UNLESS_EDGY_CHANGES = 3

def detect_diff_discovery_mode() -> DiffDiscMode:
  if 'EDG_DIFF_DISCOVERY_MODE' in os.environ:
    return DiffDiscMode[os.environ['EDG_DIFF_DISCOVERY_MODE']]
  return DiffDiscMode.ANY_UNLESS_EDGY_CHANGES

def _find_changes_file_in_dir(search_path: Path) -> Optional[Path]:
  for root, dirs, _ in os.walk(search_path):
    changes_path = Path(root, 'changes.elist')
    if changes_path.exists():
      return changes_path
  return None

def find_changes_files(run_dir: Path) -> Dict[str, Path]:
  '''Run the changes.elist files in the given run directory and return a
  mapping of the configuration name to the path for its changes file.
  '''
  changes_files: Dict[str, Path] = {}
  for dir_entry in os.listdir(run_dir):
    # Exclude the 'replays' directory if its at the top level.
    if dir_entry == 'replays':
      continue

    # Exclude the 'recording-updates' directory if its at the top level.
    if dir_entry == 'recording-updates':
      continue

    # Exclude the 'patched-tests' directory if its at the top level.
    if dir_entry == 'patched-tests':
      continue

    changes_file = _find_changes_file_in_dir(run_dir / dir_entry)
    if changes_file is None:
      continue
    changes_files[dir_entry] = changes_file
  return changes_files

def describe_rt_path(rt_file_path: Path, run_dir: Path) -> Tuple[str, str]:
  '''Return (relative_rt_path, config_name) for an .rt.tar under the run.

  The path is interpreted as run_dir/<config>/<relative_rt_path>.
  '''
  rel = rt_file_path.relative_to(run_dir)
  if len(rel.parts) < 2:
    raise ValueError(
      f"rt path is not under a config directory of {run_dir}: {rt_file_path}"
    )
  config_name = rel.parts[0]
  relative_rt_path = Path(*rel.parts[1:]).as_posix()
  return relative_rt_path, config_name

def walk_rt_file_directory(run_path: Path, *,
                           diff_algorithm: DiffDiscMode,
                           do_replays_dir_detection: bool) -> Iterator[Path]:
  '''Walk the given search path for any .rt.tar files encountered.  If
  do_replays_dir_detection is True, the traversal will exclude any top level
  'replays' directory if it exists.
  '''
  run_path_str = str(run_path)
  if diff_algorithm == DiffDiscMode.ANY_CHANGE:
    for root, dirs, file_names in os.walk(run_path):
      if do_replays_dir_detection and root == run_path_str:
        # Exclude the 'replays' directory if its at the top level
        dirs[:] = [d for d in dirs if d != 'replays']

      # Sort to make things deterministic
      dirs.sort()
      file_names.sort()

      for file_name in file_names:
        # Skip everything that isn't an .rt.tar file
        if not file_name.endswith(runtest.RUN_TEST_OUTPUT_SUFFIX):
          continue

        yield Path(root, file_name)
  elif (diff_algorithm == DiffDiscMode.EDGY_CHANGES or
        diff_algorithm == DiffDiscMode.ANY_UNLESS_EDGY_CHANGES):
    changes_files = find_changes_files(run_path)
    if len(changes_files) != 0:
      for changes_file_path in changes_files.values():
        for file_path in _scan_changes_file(changes_file_path):
          yield file_path
    elif diff_algorithm == DiffDiscMode.ANY_UNLESS_EDGY_CHANGES:
      fallback_file_walker = walk_rt_file_directory(
        run_path,
        diff_algorithm = DiffDiscMode.ANY_CHANGE,
        do_replays_dir_detection = do_replays_dir_detection
      )
      for file_path in fallback_file_walker:
        yield file_path
  else:
    assert False, "Unknown diff discovery mode"

DIRECTIVE_LINE_REGEX = re.compile(r'^//.*:.*$')
EDG_ESCAPE_REGEX = re.compile(r'\?{3}([0-9]{3})')

async def perform_test_post_process(src_path: Path, dest_path: Path) -> None:
  '''Create a copy of the test file at the given source path with
  post-processing completed.

  This includes removing "//" style directive comments and processing EDG test
  escape sequences into the corresponding character.
  '''
  with open(dest_path, 'w+b') as dest_file_handle:
    with open(src_path, 'r+b') as src_file_handle:
      for line in src_file_handle:
        try:
          safe_line = line.decode(encoding = 'utf-8')
          # Replace directive lines with empty lines.
          if DIRECTIVE_LINE_REGEX.match(safe_line):
            dest_file_handle.write('\n'.encode(encoding = 'utf-8'))
            continue
          # Replace EDG escape characters in reverse (to allow indexing
          # to resolve correctly).
          esc_matches = [m for m in EDG_ESCAPE_REGEX.finditer(safe_line)]
          for esc_match in reversed(esc_matches):
            repl_char = chr(int(esc_match.group(1)))
            safe_line = (
              safe_line[:esc_match.start()] +
              repl_char +
              safe_line[esc_match.end():]
            )
          dest_file_handle.write(safe_line.encode(encoding = 'utf-8'))
        except ValueError:
          # If UTF decoding fails, revert to a binary line write.
          dest_file_handle.write(line)

def _checkout_filter_files(dir: str, entry_list: List[str]) -> List[str]:
  return [e for e in entry_list if not runtest.is_valid_assoc_path(dir, e)]

def copy_assoc_files_to(test_path: Path, dest_dir: Path) -> None:
  '''Copy the test's associated files into dest_dir, if any.

  Preserves symlinks so that links to Local_file.c resolve after the test
  file is checked out into the destination.
  '''
  assoc_files_path = runtest.get_assoc_files_path(test_path)
  if assoc_files_path is None or not assoc_files_path.exists():
    return

  _copytree_exist_ok(
    assoc_files_path,
    dest_dir,
    ignore = _checkout_filter_files,
    symlinks = True
  )

def copy_raw_test_files(test_path: Path, dest_dir: Path) -> Path:
  if not dest_dir.exists():
    dest_dir.mkdir(parents = True, exist_ok = True)

  # Pull in the associated files if they exists.
  assoc_files_path = runtest.get_assoc_files_path(test_path)
  if assoc_files_path is not None:
    assert (
      assoc_files_path == test_path.parent and
      assoc_files_path.exists()
    )
  copy_assoc_files_to(test_path, dest_dir)

  # Pull in the recordings (a test without output may have none).
  test_recordings = runtest.get_assoc_recordings_path(test_path)
  if test_recordings.is_dir():
    _copytree_exist_ok(
      test_recordings,
      dest_dir / test_recordings.name
    )

  # Pull in the test file.
  new_test_file_path = dest_dir / test_path.name
  shutil.copy2(test_path, new_test_file_path)

  return new_test_file_path

async def checkout_at_directory(test_path: Path, checkout_dir: Path) -> Path:
  '''A function for cloning a test's file set into the given directory.

  This is used to construct the working directory for executing a test.

  The path to the checked out test file is returned.
  '''
  # Pull in the associated files if they exists.
  test_descript = runtest.load_single_description(test_path)
  copy_assoc_files_to(test_path, checkout_dir)
  # Pull in the original test file as Local_file.c
  test_file_checkout_path = checkout_dir / 'Local_file.c'
  shutil.copy2(test_path, test_file_checkout_path)
  # Create the post processed file with the appropriate name
  processed_file_checkout_path = checkout_dir / test_descript.name
  await perform_test_post_process(
    test_file_checkout_path,
    processed_file_checkout_path
  )
  return processed_file_checkout_path


def sync_checkout_at_directory(test_path: Path, checkout_dir: Path) -> Path:
  '''A function for cloning a test's file set into the given directory.

  This is used to construct the working directory for executing a test.

  The path to the checked out test file is returned.
  '''
  loop = edgutil.lazy_init_event_loop()
  future = asyncio.Future()  # type: asyncio.Future

  async def exec_async() -> None:
    future.set_result(await checkout_at_directory(test_path, checkout_dir))

  loop.run_until_complete(loop.create_task(exec_async()))
  return future.result()

class ExecutionTempDir:
  '''A class for creating a temporary directory for executing a test.'''

  def __init__(self, test_path: Path, *, keep_files: bool = False) -> None:
    self.original_cwd = os.getcwd()
    self.orig_test_path = test_path
    self.temp_dir = Path(tempfile.mkdtemp(prefix = test_path.name))
    self.test_path: Optional[Path] = None
    self._keep_files = keep_files

  def __enter__(self) -> 'ExecutionTempDir':
    self.test_path = sync_checkout_at_directory(
      self.orig_test_path,
      self.temp_dir
    )
    return self

  async def __aenter__(self) -> 'ExecutionTempDir':
    self.test_path = await checkout_at_directory(
      self.orig_test_path,
      self.temp_dir
    )
    return self

  def __exit__(self, _type: Any, _value: Any, _tb: Any) -> None:
    if not self._keep_files:
      shutil.rmtree(self.temp_dir)

  async def __aexit__(self, type: Any, value: Any, tb: Any) -> None:
    self.__exit__(type, value, tb)
