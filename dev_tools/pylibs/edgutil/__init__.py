# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import asyncio
import difflib
import functools
import json
import os
import platform
import re
import shutil
import signal
import sys
import tempfile
import traceback

from datetime import datetime
from pathlib import Path
from typing import (
  Any,
  BinaryIO,
  Callable,
  Coroutine,
  Dict,
  Iterable,
  List,
  Optional,
  Set,
  Tuple,
  cast
)

def _is_project_dir(src_dir: Path) -> bool:
  '''Perform a quick check to see if this directory looks like a project
  checkout.'''
  if not (src_dir / '.git').exists():
    return False
  if not (src_dir / 'src' / 'cfe.c').exists():
    return False
  if not (src_dir / 'dev_tools').exists():
    return False
  return True

@functools.lru_cache(maxsize = None)
def _find_project_dir(dir: Path) -> Optional[Path]:
  for candidate in [dir, *dir.parents]:
    if _is_project_dir(candidate):
      return candidate
  return None

def try_find_mono_repo(search_start: Optional[Path] = None) -> Optional[Path]:
  '''Locate the EDG monorepo root, or return None if none is found.

  Still exits if both the search start and EDG_PRIMARY_SRC_DIR resolve to
  different monorepo roots (ambiguous configuration).
  '''
  if search_start is None:
    search_start = Path.cwd()

  cwd_src_dir = _find_project_dir(search_start)
  env_src_dir = (
    _find_project_dir(Path(os.environ['EDG_PRIMARY_SRC_DIR']))
    if 'EDG_PRIMARY_SRC_DIR' in os.environ else None
  )
  src_dir = env_src_dir or cwd_src_dir

  if not src_dir:
    return None

  if (cwd_src_dir is not None and env_src_dir is not None and
      cwd_src_dir != env_src_dir):
    eprint(
      'In a compiler source directory but EDG_PRIMARY_SRC_DIR is set to a '
      'different compiler source directory:\n'
      f"  {env_src_dir}\n"
      'Please unset EDG_PRIMARY_SRC_DIR or change directory to resolve '
      'the ambiguity.'
    )
    sys.exit(1)

  return src_dir

def find_mono_repo_or_exit(search_start: Optional[Path] = None) -> Path:
  src_dir = try_find_mono_repo(search_start)
  if not src_dir:
    eprint('Not in an EDG compiler checkout and no default found!')
    sys.exit(1)
  return src_dir

class MissingRequiredToolException(Exception):
  '''Raised when a required external tool is not found on PATH.'''

  def __init__(self, tool_name: str) -> None:
    self.tool_name = tool_name
    super().__init__(f"{tool_name} tool is required, but was not found")

class RequiredToolPath:
  '''Lazy PATH lookup for a required external tool.

  Construction only caches ``shutil.which``; it does not exit or raise, so
  importing modules that define ``TOOL_PATH_* = RequiredToolPath(...)`` still
  allows ``--help`` when the tool is missing. Call ``exit_if_not_found``
  (CLI) or convert to ``str`` (raises ``MissingRequiredToolException`` if
  missing) before using the path.
  '''

  def __init__(self, tool_name: str) -> None:
    self._tool_name = tool_name
    self._tool_path: Optional[str] = shutil.which(tool_name)

  def exit_if_not_found(self) -> str:
    '''Return the tool path, or print an error and exit.'''
    if self._tool_path is None:
      eprint(str(MissingRequiredToolException(self._tool_name)))
      sys.exit(1)
    return self._tool_path

  def __str__(self) -> str:
    if self._tool_path is None:
      raise MissingRequiredToolException(self._tool_name)
    return self._tool_path

def python_gil_enabled() -> bool:
  '''Return True when this interpreter is running with the GIL held.'''
  if sys.version_info >= (3, 13):
    return sys._is_gil_enabled()
  return False

class GILAvoidanceProcessException(Exception):
  '''Raised when a GIL-avoidance helper subprocess exits unsuccessfully.

  These processes exist specifically to move GIL-bound work (e.g. regex
  normalization, tar compression) into a separate interpreter so concurrent
  Python work can progress.
  '''

  def __init__(
      self,
      command: List[str],
      returncode: int,
      *,
      stderr: str = '') -> None:
    self.command = list(command)
    self.returncode = returncode
    self.stderr = stderr
    message = (
      f"GIL-avoidance process failed (exit {returncode}): "
      f"{' '.join(command)}"
    )
    if stderr:
      message = f"{message}\n{stderr}"
    super().__init__(message)

def is_docker_preferred(mono_repo_dir: Path) -> bool:
  dev_pref_file = mono_repo_dir / 'dev-pref.json'
  preferred_env = None

  if dev_pref_file.exists():
    with open(dev_pref_file) as file_handle:
      dev_prefs = json.load(file_handle)
      if 'env_type' in dev_prefs:
        preferred_env = dev_prefs['env_type']

  return preferred_env is None or preferred_env == 'docker'

class CurrentWorkingDirectorySwap:
  '''Temporary enter a current working directory.

  Swaps to the specified destination directory when used via a "with as"
  statement. Swaps back upon exit.
  '''

  def __init__(self, dest_dir: Path) -> None:
    self.original_cwd = os.getcwd()
    self.dest_dir = dest_dir

  def __enter__(self) -> 'CurrentWorkingDirectorySwap':
    os.chdir(self.dest_dir)
    return self

  def __exit__(self, _type, _value, _tb):
    os.chdir(self.original_cwd)

def eprint(msg: str) -> None:
  print(msg, file = sys.stderr)

def indent_text_block(text_block: str, n_times: int = 1) -> str:
  indent = '  ' * n_times
  return re.sub(r'^(.)', f"{indent}\\1", text_block, flags = re.MULTILINE)

def format_overlong_lines(overlong_lines: Dict[str, Any],
                          heading_indent: str = '') -> List[str]:
  '''Format the overlong_lines result of edg-check-file-changes ({line number:
  [first context line number, context lines]}) for display.

  Each overlong line gets a heading (indented by heading_indent) and its
  context between two 79-column rules.  The context isn't indented, so the
  columns line up with the rules.
  '''
  report_lines = []
  for line_no, (line_start, lines) in sorted(overlong_lines.items(),
                                             key = lambda item: int(item[0])):
    line_end = line_start + (len(lines) - 1)
    report_lines.append(f"{heading_indent}Line {line_no} too long:")
    report_lines.append(f" {line_start} -> {line_end} ".center(79, '='))
    report_lines += [line.rstrip('\n') for line in lines]
    report_lines.append('=' * 79)
  return report_lines

def is_env_set(env_var: str) -> bool:
  '''Return True if the given environment variable is set to a non-zero
  value.'''
  if env_var not in os.environ:
    return False
  var_value = os.environ[env_var]
  if not var_value or var_value == '0':
    return False
  return True

def print_env_var(env_var: str) -> None:
  var_value = os.environ.get(env_var) or ''
  print(f"Using \"{env_var}\": \"{var_value}\"")

def get_application_files_dir(app_name: str) -> Path:
  platform_name = platform.system()
  if platform_name == 'Linux':
    if 'XDG_CONFIG_HOME' in os.environ:
      return Path(os.environ['XDG_CONFIG_HOME'], app_name)
    return Path(os.environ['HOME'], '.config', app_name)
  elif platform_name == 'Darwin':
    return Path(os.environ['HOME'], 'Library', 'Preferences', app_name)
  elif platform_name == 'Windows':
    return Path(os.environ['APPDATA'], app_name)
  else:
    print(f"Unsupported platform: {platform_name}.")
    sys.exit(1)

_DESCRITPION_KEY_REGEX = re.compile(r'^\s*//([a-z_]+):\s*(.*)\s*$')

def _collect_directive_line(directive_map: Dict[str, List[str]],
                            directive_line: str) -> bool:
  '''Process and collect the directive line's key and value into the given
  map.

  This function simply multi-maps valid looking EDG C++ directives textually.

  If the line doesn't match the textual pattern of an EDG C++ directive, the
  function returns False; otherwise it returns True.
  '''
  re_match = _DESCRITPION_KEY_REGEX.match(directive_line)
  if re_match is None:
    return False

  key = re_match.group(1)
  value = re_match.group(2)

  if key not in directive_map:
    directive_map[key] = []
  directive_map[key].append(value)
  return True

def collect_cpp_line_directives(file_path: Path) -> Dict[str, List[str]]:
  '''Process and collect the directive EDG C++ directive block at the start
  of the given file.

  EDG C++ directives are those C++-style comments at the start of a C++
  source file that match _DESCRITPION_KEY_REGEX.

  The returned value is a mapping from the description keys to all values with
  that key.
  '''
  open_file = open(
    file_path,
    'r',
    encoding = 'utf-8',
    errors = 'backslashreplace'
  )
  with open_file as file_handle:
    directive_map: Dict[str, List[str]] = {}
    started_collection = False
    for line in file_handle:
      collected = _collect_directive_line(directive_map, line)
      if started_collection and not collected:
        break
      if collected:
        started_collection = True

    return directive_map

def split_leading_cpp_line_directives(
                   lines: List[str]) -> Tuple[Dict[str, List[str]], List[str]]:
  '''Split the EDG C++ directive block off the start of lines.

  Unlike collect_cpp_line_directives, only the unbroken block of directive
  lines starting at the very first line is collected, so directive-looking
  lines later on (e.g., in an example) are left alone.

  Returns the directive map and the lines that follow the block.
  '''
  directive_map: Dict[str, List[str]] = {}
  line_index = 0
  while (line_index < len(lines) and
         _collect_directive_line(directive_map, lines[line_index])):
    line_index += 1

  return directive_map, lines[line_index:]

def replace_cpp_line_directives(file_path: Path,
                                directive_lines: Iterable[str]) -> None:
  '''Replace the leading EDG C++ directive block in file_path.

  directive_lines are written at the start of the file (each line is given a
  trailing newline if missing). Existing leading //key:value directive lines
  are dropped; all subsequent content is preserved.
  '''
  with tempfile.NamedTemporaryFile(
    'w',
    encoding = 'utf-8'
  ) as tmp_file_handle:
    # Prepend the new directive lines.
    for line in directive_lines:
      if line.endswith('\n'):
        tmp_file_handle.write(line)
      else:
        tmp_file_handle.write(line)
        tmp_file_handle.write('\n')
    # Collect and discard the existing directive lines, then
    # append the remaining file contents line by line.
    with open(
      file_path,
      'r',
      encoding = 'utf-8',
      errors = 'backslashreplace'
    ) as file_handle:
      directive_map: Dict[str, List[str]] = {}
      collecting_lines = True
      for line in file_handle:
        if collecting_lines:
          if _collect_directive_line(directive_map, line):
            continue
          collecting_lines = False
        tmp_file_handle.write(line)
    # Flush the tmp file and then replace the original file with the tmp file.
    tmp_file_handle.flush()
    shutil.copy2(tmp_file_handle.name, file_path)

class AsyncLoopFatalError(Exception):
  '''Raised to ask ``run_asyncio_loop`` to cancel remaining tasks and stop.'''
  pass

# The helpers below depend on the Python version, which cannot change at run
# time, so each is declared once per supported API and selected at import.

if sys.version_info >= (3, 7):
  def get_running_event_loop() -> asyncio.AbstractEventLoop:
    '''Return the running event loop (asyncio.get_running_loop is 3.7+).'''
    return asyncio.get_running_loop()
else:
  def get_running_event_loop() -> asyncio.AbstractEventLoop:
    '''Return the running event loop (asyncio.get_running_loop is 3.7+).'''
    return asyncio.get_event_loop()

if sys.version_info >= (3, 9):
  def is_path_relative_to(path: Path, other: Path) -> bool:
    '''Return whether path is equal to or located under other.'''
    return path.is_relative_to(other)
else:
  def is_path_relative_to(path: Path, other: Path) -> bool:
    '''Return whether path is equal to or located under other.

    Path.is_relative_to is only available in Python 3.9+.
    '''
    try:
      result = Path(os.path.commonpath([str(path), str(other)])) == other
    except ValueError:
      # Mixing absolute and relative paths (or differing drives).
      result = False
    return result

if sys.version_info >= (3, 9):
  async def async_to_thread(func: Callable[..., Any],
                            *args: Any,
                            **kwargs: Any) -> Any:
    '''Run func(*args, **kwargs) in a worker thread and await its result.'''
    return await asyncio.to_thread(func, *args, **kwargs)
else:
  async def async_to_thread(func: Callable[..., Any],
                            *args: Any,
                            **kwargs: Any) -> Any:
    '''Run func(*args, **kwargs) in a worker thread and await its result.

    asyncio.to_thread is only available in Python 3.9+.
    '''
    return await get_running_event_loop().run_in_executor(
      None, functools.partial(func, *args, **kwargs)
    )

if sys.version_info >= (3, 7):
  def _active_asyncio_tasks(
      loop: asyncio.AbstractEventLoop) -> Set[asyncio.Task]:
    # asyncio.Task.all_tasks was deprecated in 3.7 in favor of
    # asyncio.all_tasks, and removed in 3.9.
    return asyncio.all_tasks(loop)
else:
  def _active_asyncio_tasks(
      loop: asyncio.AbstractEventLoop) -> Set[asyncio.Task]:
    return asyncio.Task.all_tasks(loop)  # type: ignore[attr-defined]

def _stop_asyncio_loop(loop: asyncio.AbstractEventLoop) -> None:
  for task in _active_asyncio_tasks(loop):
    task.cancel()

@functools.lru_cache(maxsize = None)
def lazy_init_event_loop() -> asyncio.AbstractEventLoop:
  '''Upon first execution, create a new event loop and install it as the main
  event loop.  Upon subsequent executions return the cached event loop value.

  This provides an event loop abstraction that works from Python 3.6 through
  at least Python 3.14 (latest at the time of writing).

  Guaranteed (one time) replacement of the event loop is the chosen
  implementation strategy as retrieving and modifying the running event loop
  has proven non-portable between different Python versions and Linux
  distribution builds of Python.
  '''
  loop = asyncio.new_event_loop()
  asyncio.set_event_loop(loop)
  return loop

def _install_asyncio_signal_handlers(loop: asyncio.AbstractEventLoop) -> None:
  '''Configure SIGINT/SIGTERM to cancel active tasks on ``loop``.'''
  for signame in {'SIGINT', 'SIGTERM'}:
    loop.add_signal_handler(
      getattr(signal, signame),
      functools.partial(_stop_asyncio_loop, loop)
    )

def run_asyncio_loop(
       task_consumer: Callable[[asyncio.AbstractEventLoop], Coroutine],
       *,
       install_signal_handlers: bool = True) -> Any:
  '''Run an asyncio loop.  If run at the root level, this function will
  configure a new event loop, optionally with signal handlers configured to
  terminate tasks upon receiving SIGINT or SIGTERM signals.

  task_consumer is a lambda function consuming the created loop and returning
  the initial async function asyncio.

  If that task raises ``AsyncLoopFatalError``, remaining tasks are cancelled
  the same way a SIGINT/SIGTERM would cancel them, those cancelled tasks are
  awaited, and the error is re-raised to the caller.

  Pass ``install_signal_handlers=False`` when the caller already registers
  process-level handlers and must not have them replaced by the event loop.
  '''
  loop = lazy_init_event_loop()
  if install_signal_handlers:
    _install_asyncio_signal_handlers(loop)

  async def run_task_consumer() -> Any:
    try:
      return await task_consumer(loop)
    except AsyncLoopFatalError:
      _stop_asyncio_loop(loop)
      raise

  return loop.run_until_complete(
    asyncio.shield(loop.create_task(run_task_consumer()))
  )

def handle_proc_cancel(proc: Optional[asyncio.subprocess.Process]) -> bool:
  '''Terminate the given process prematurely.

  If the process was already dead or shutting down, return False; otherwise,
  return True.
  '''
  # Cleanup the process if it's been spawned and is still alive (represented
  # by the asyncio process as the absence of a return code).
  if proc is not None and proc.returncode is None:
    try:
      # As the process was started in a new session (i.e., a new process group)
      # we can safely and effectively terminate it via said process group.
      proc_group = os.getpgid(proc.pid)
      os.killpg(proc_group, signal.SIGTERM)
      return True
    except ProcessLookupError:
      # In some cases the process terminates while we're trying to retrieve the
      # process group to terminate. Silently swallow the exception.
      pass
  return False

class AsyncJobLoop:
  def __init__(self, loop: asyncio.AbstractEventLoop, *, max_jobs: int,
               max_update_interval = 1) -> None:
    self.loop = loop
    self.max_jobs = max_jobs
    self.active_tasks: List[asyncio.Task] = []
    self.max_update_interval = max_update_interval

  def add_task(self, new_async_task: Coroutine) -> None:
    self.active_tasks.append(self.loop.create_task(new_async_task))

  def cancel_active_tasks(self) -> None:
    for task in self.active_tasks:
      task.cancel()

  async def run_supplier(
              self, supplier_fn: Callable[[], Optional[Coroutine]],
              update_callback_fn: Callable[[Set[asyncio.Task]], bool]) -> None:
    '''Run supplied jobs provided by the supplier function.

    The supplier function should return new async function calls until there
    are no more jobs to execute, at which point it should return None to
    indicate there are no more jobs remaining.

    The update callback function will be called whenever at least one job
    finishes or the max update interval has been exceeded. It should accept a
    set of complete tasks (composed of the tasks completed since the last
    call). The update function should then return True if it would like to
    prematurely stop processing; otherwise, it should return False to continue
    processing.

    If any completed job raised an exception (other than cancellation), its
    traceback is printed and processing continues with the remaining jobs.
    '''
    while True:
      try:
        # While there's headroom for new tasks, check if the supplier has
        # any new jobs.
        while len(self.active_tasks) < self.max_jobs:
          new_async_task = supplier_fn()
          if new_async_task is None:
            break

          self.add_task(new_async_task)

        # All tasks have finished, end the loop
        if len(self.active_tasks) == 0:
          break

        # Wait for a test to finish (or a 1 second timeout) so that the
        # update_callback isn't starved.
        done, pending = await asyncio.wait(
          self.active_tasks,
          timeout = self.max_update_interval,
          return_when = asyncio.FIRST_COMPLETED
        )

        # Retrieve exceptions from completed tasks. Leaving them unread
        # yields opaque "Task exception was never retrieved" warnings and
        # can hide the failure for an extended period.
        for task in done:
          if task.cancelled():
            continue
          exc = task.exception()
          if exc is not None:
            eprint(
              f"Async job failed: {type(exc).__name__}: {exc}"
            )
            traceback.print_exception(
              type(exc),
              exc,
              exc.__traceback__,
              file = sys.stderr
            )

        if self.max_jobs > 0 and update_callback_fn(done):
          # Accept no new tasks, and cancel any currently running tests.
          self.max_jobs = 0
          self.cancel_active_tasks()

        self.active_tasks = list(pending)
      except asyncio.CancelledError:
        # It's assumed other tasks have also raised a cancellation exception,
        # so reducing our target quantity of jobs to 0 is sufficient.
        self.max_jobs = 0

class AsyncStatusReporter:
  def __init__(self, status_line_fn: Callable[[], str]) -> None:
    self.status_line_fn = status_line_fn
    self.status_line_length = 0

  def is_status_line_enabled(self) -> bool:
    # We should only have a status line if we're running interactively.
    return sys.stdout.isatty()

  def _print_status_line(self) -> None:
    sys.stdout.write('\r')
    sys.stdout.flush()
    status_line = self.status_line_fn()

    # Use an extra two spaces to prevent some cases where terminal input
    # offsets the line, resulting in overwrite past what the program would
    # otherwise consider the end of the status line.
    self.status_line_length = len(status_line) + 2
    sys.stdout.write(status_line)
    sys.stdout.write('  ')
    sys.stdout.flush()

  def print_status_line(self) -> None:
    if not self.is_status_line_enabled():
      return

    self._print_status_line()

  def cleanup_status_line(self) -> None:
    if not self.is_status_line_enabled():
      return

    self._print_status_line()
    sys.stdout.write('\n')

  def print(self, string: str, end = '\n') -> None:
    # Setup to overwrite the status line if one is present. This is
    # particularly important for piped stdout as we don't want to pollute the
    # output stream with a bunch of nonsensical return characters.
    if self.is_status_line_enabled():
      # Write the info, replacing the status line, and adding the new text
      sys.stdout.write(f"\r{' ' * self.status_line_length}\r{string}{end}")
      sys.stdout.flush()

      # Add a new status line
      self.print_status_line()
    else:
      try:
        sys.stdout.write(string)
        sys.stdout.flush()
      except BrokenPipeError:
        # Ignore broken pipe errors, the user has likely sent us SIGINT or
        # SIGTERM and our output is piped. Redirect the output to devnull to
        # avoid further broken pipe errors.
        devnull = os.open(os.devnull, os.O_WRONLY)
        os.dup2(devnull, sys.stdout.fileno())

# The timestamp format used for various "runs" (namely test and benchmarks
# "runs").
RUN_TIMESTAMP_FORMAT = '%Y.%m.%d-%H.%M.%S'
RUN_TIMESTAMP_REGEX = re.compile(
  r'[0-9]{4}.[0-9]{2}.[0-9]{2}-[0-9]{2}.[0-9]{2}.[0-9]{2}'
)

def sorted_runs_in_dir(dir: Path) -> List[Path]:
  '''Return a list of the directories matching the RUN_TIMESTAMP_FORMAT in
  the given directory.  The most recent runs will be first in the list.
  '''
  dirs = [
    d for d in dir.iterdir()
    if d.is_dir() and RUN_TIMESTAMP_REGEX.match(d.name)
  ]
  dirs.sort(
    key = lambda d: datetime.strptime(d.name, RUN_TIMESTAMP_FORMAT),
    reverse = True
  )
  return dirs


# ---------------------------------------------------------------------------
# Unified file diffs (async `diff -u`, with difflib fallback)
# ---------------------------------------------------------------------------

def _write_unified_diff_with_difflib(from_file: Path,
                                     to_file: Path,
                                     out_handle: BinaryIO,
                                     from_label: str,
                                     to_label: str) -> bool:
  '''Fallback unified diff via difflib when the diff binary is unavailable.

  Writes into out_handle. Returns True when a diff was produced.
  '''
  with open(from_file, 'r', errors = 'replace') as from_handle:
    from_lines = from_handle.readlines()
  with open(to_file, 'r', errors = 'replace') as to_handle:
    to_lines = to_handle.readlines()

  diff_lines = list(difflib.unified_diff(
    from_lines,
    to_lines,
    fromfile = from_label,
    tofile = to_label
  ))
  if len(diff_lines) == 0:
    return False

  for line in diff_lines:
    out_handle.write(line.encode('utf-8', errors = 'replace'))
  return True

_DIFF_CMD = shutil.which('diff')

async def _write_unified_diff_with_diff_cmd(
    from_file: Path,
    to_file: Path,
    out_handle: BinaryIO,
    from_label: str,
    to_label: str,
    env: Optional[Dict[str, str]]) -> bool:
  '''Primary diff implementation via diff binary.

  Writes into out_handle. Returns True when a diff was produced.
  '''
  assert _DIFF_CMD is not None
  process = await asyncio.create_subprocess_exec(
    _DIFF_CMD,
    '-u',
    '--label',
    from_label,
    str(from_file),
    '--label',
    to_label,
    str(to_file),
    env = env,
    stdout = out_handle,
    stderr = asyncio.subprocess.STDOUT
  )
  return_code = await process.wait()
  return return_code != 0

class UnifiedDiffStream:
  '''Async context manager yielding a readable unified-diff stream.

  Prefers an async `diff -u` subprocess when available; otherwise falls back
  to difflib. Owns a temporary directory for the diff file for the duration of
  the context (similar to ``edgtest.ExecutionTempDir``).

  ``async with`` yields the open binary stream, or ``None`` when the inputs
  match.
  '''

  def __init__(
      self,
      from_file: Path,
      to_file: Path,
      *,
      from_label: str,
      to_label: str,
      env: Optional[Dict[str, str]] = None) -> None:
    self._from_file = from_file
    self._to_file = to_file
    self._from_label = from_label
    self._to_label = to_label
    self._env = env
    self._temp_dir: Optional[Path] = None
    self._out_handle: Optional[BinaryIO] = None

  async def __aenter__(self) -> Optional[BinaryIO]:
    self._temp_dir = Path(tempfile.mkdtemp())
    self._out_handle = open(self._temp_dir / 'diff', 'w+b')

    if _DIFF_CMD is None:
      has_diff = await async_to_thread(
        _write_unified_diff_with_difflib,
        self._from_file,
        self._to_file,
        self._out_handle,
        self._from_label,
        self._to_label
      )
    else:
      has_diff = await _write_unified_diff_with_diff_cmd(
        self._from_file,
        self._to_file,
        self._out_handle,
        self._from_label,
        self._to_label,
        self._env
      )

    if not has_diff:
      return None

    self._out_handle.seek(0)
    return self._out_handle

  async def __aexit__(self, _type: Any, _value: Any, _tb: Any) -> None:
    if self._out_handle is not None:
      self._out_handle.close()
      self._out_handle = None
    if self._temp_dir is not None:
      shutil.rmtree(self._temp_dir)
      self._temp_dir = None
