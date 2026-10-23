# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Shared AcknowlEDG client types used by test and bench providers.'''

import asyncio
import errno
import os
import signal
import stat

from pathlib import Path
from typing import (
  Any,
  Awaitable,
  Callable,
  Dict,
  List,
  IO,
  Iterator,
  NamedTuple,
  Optional,
  Sequence,
  Tuple,
  Union,
)

try:
  from typing import Protocol
except ImportError:
  try:
    from typing_extensions import Protocol  # type: ignore
  except ImportError:
    Protocol = None  # type: ignore[misc,assignment]

from edgacknowledg import (
  MAX_DISPLAY_CONTENT_SIZE,
  ProtocolMessage,
  RequestTags,
  RichName,
  message,
)


if Protocol is not None:
  class Channel(Protocol):
    async def recv(self) -> Any: ...
    async def send(self, msg: ProtocolMessage) -> None: ...


  class ReviewIdentity(Protocol):
    @property
    def name(self) -> RichName: ...
    @property
    def timestamp(self) -> str: ...


  class ReviewClientHandler(Protocol):
    '''Structural provider for one review section subscription.'''

    @property
    def subscription_type(self) -> str:
      '''One of: test.src, test.edit, bench.src, bench.edit'''
      ...

    @property
    def section_name(self) -> str:
      ...

    @property
    def slug(self) -> str:
      ...

    async def handle_request(self, channel: Channel) -> None:
      '''Process one inbound request on ``channel``.'''
      ...
else:
  Channel = Any  # type: ignore[misc,assignment]
  ReviewIdentity = Any  # type: ignore[misc,assignment]
  ReviewClientHandler = Any  # type: ignore[misc,assignment]


from edgutil import get_running_event_loop, is_path_relative_to

CoroFactory = Callable[[], Awaitable[Any]]


class BackgroundWorkerPool:
  '''Async worker pool used to regulate parallel background work.'''

  def __init__(self, max_workers: Optional[int] = None) -> None:
    cpu_count = os.cpu_count()
    self._max_workers = max_workers if max_workers is not None else (
      cpu_count if cpu_count is not None else 1
    )
    # Queue element type: Tuple[CoroFactory, asyncio.Future]
    self._queue = asyncio.Queue(self._max_workers)  # type: asyncio.Queue
    self._workers = []  # type: List[asyncio.Task]
    self._started = False

  def ensure_started(self) -> None:
    if self._started:
      return
    self._started = True
    loop = get_running_event_loop()
    for _ in range(self._max_workers):
      self._workers.append(loop.create_task(self._worker()))

  async def _worker(self) -> None:
    while True:
      coro_factory, future = await self._queue.get()
      try:
        result = await coro_factory()
        if not future.done():
          future.set_result(result)
      except Exception as ex:
        if not future.done():
          future.set_exception(ex)
      finally:
        self._queue.task_done()

  async def submit(self, coro_factory: CoroFactory) -> Any:
    '''Run ``coro_factory()`` on a pool worker and return its result.'''
    self.ensure_started()
    loop = get_running_event_loop()
    future = loop.create_future()  # type: asyncio.Future
    await self._queue.put((coro_factory, future))
    return await future


async def announce_section(channel: Channel,
                           handler: ReviewClientHandler,
                           review: ReviewIdentity) -> None:
  '''Send optional section/review identity messages on a new subscription.'''
  await channel.send(message(
    RequestTags.META_SECTION_NAME,
    value = handler.section_name
  ))
  await channel.send(message(
    RequestTags.META_REVIEW_NAME,
    value = review.name
  ))
  await channel.send(message(
    RequestTags.META_REVIEW_TIMESTAMP,
    value = review.timestamp
  ))


def trunc_display_content(content: str) -> Dict[str, Union[str, bool]]:
  '''Truncate ``content`` for UI display; may set ``truncated``.'''
  max_content_size = MAX_DISPLAY_CONTENT_SIZE
  if len(content) > max_content_size:
    # Attempt to truncate at line boundaries.
    #
    # To save time, find the newline closest to the boundary before walking
    # the file size back.
    last_newline_permitted = content.find('\n', max_content_size)
    if last_newline_permitted != -1:
      content = content[:last_newline_permitted]
    # Now walk the file size back.
    while len(content) > max_content_size:
      last_new_line_idx = content.rfind('\n')
      if last_new_line_idx != -1:
        content = content[:(last_new_line_idx - 1)]
      else:
        # If there are no more newlines, resort to truncating within
        # the line.
        content = content[:(max_content_size - 4)]
        content += '...'
    return {
      'truncated': True,
      'content': content
    }
  return { 'content': content }



class IllegalFileAccessError(Exception):
  '''Raised when a request asks for a file outside the allowed tree.

  Providers record the requesting reviewer in ``reviewer_id``, so the
  request loop can report them (``RequestTags.ILLEGAL_ACCESS``) and the
  server can drop their connection.
  '''

  def __init__(self, *args: Any) -> None:
    super().__init__(*args)
    self.reviewer_id: Optional[str] = None


class FileTooLargeError(Exception):
  '''Raised when a request would read a file larger than its context
  allows. Unlike ``IllegalFileAccessError`` the request was legitimate;
  it just fails.
  '''


# Bytes a file read for a reviewer may have, by default: generous for
# sources, recordings, and logs, but bounded, so a large file (say, in a
# pull request) can't exhaust the memory of whoever serves it.
DEFAULT_MAX_FILE_SIZE = 64 * 1024 * 1024


class ConfinedPathComponent:
  '''A relative path between two ``ConfinedPath``s (``relative_to``), to be
  joined onto ``ConfinedPath``s (``/``, ``joinpath``) and nothing else: it
  is neither a ``PurePath`` nor ``os.PathLike``, so no other path API
  takes it. Its string form (``str``, ``as_posix``) is for display and
  identifiers.

  It never holds '..', '.', or an absolute part, but joining it still
  validates the result like any other ``ConfinedPath``.
  '''

  def __init__(self, parts: Sequence[str]) -> None:
    for part in parts:
      if part in ('', '.', '..') or '/' in part or '\0' in part:
        raise ValueError(f"invalid path component: {part!r}")
    self._parts = tuple(parts)

  @property
  def parts(self) -> Tuple[str, ...]:
    return self._parts

  @property
  def name(self) -> str:
    return self._parts[-1] if len(self._parts) != 0 else ''

  def as_posix(self) -> str:
    return '/'.join(self._parts) if len(self._parts) != 0 else '.'

  def __str__(self) -> str:
    return self.as_posix()

  def __repr__(self) -> str:
    return f"ConfinedPathComponent({self.as_posix()!r})"

  def __eq__(self, other: object) -> bool:
    if not isinstance(other, ConfinedPathComponent):
      return NotImplemented
    return self._parts == other._parts

  def __lt__(self, other: 'ConfinedPathComponent') -> bool:
    return self._parts < other._parts

  def __hash__(self) -> int:
    return hash(self._parts)


class ConfinedPath:
  '''A path confined to a tree: it behaves much like a ``pathlib.Path``
  (``/``, ``joinpath``, ``parent``, ``name``, ``exists``, ``open``, ...),
  but every ``ConfinedPath`` is validated when it is made, so one can't
  point outside its tree; ``IllegalFileAccessError`` is raised instead.
  Providers use it for every path a request influences.

  A path is valid when it is lexically under the tree's root (no '..') and:

  - if symlinks are allowed, it is still under the root once
    canonicalized (links resolved), so links may only stay inside;
  - otherwise, no component below the root is a symlink or anything but a
    directory or regular file. Its ``open``, ``exists`` and the like never
    follow a link either, even one that appeared after the path was
    validated.

  A missing path is valid; reading it fails as usual. The root itself is
  trusted: whoever makes the tree vouches for it. Opening a file larger
  than ``max_file_size`` (if any) raises ``FileTooLargeError``.

  Its tree and policy are given when it is made (usually by
  ``SandboxContext.subpath``, from one of the context's roots and its
  policy); paths derived from it share them.

  Unlike ``pathlib.Path`` it is not ``os.PathLike``, so it can't be handed
  to ``open`` or a library by accident: such uses take ``.path``
  explicitly (where the checks above end).
  '''

  def __init__(self,
               root: Path,
               path: Path,
               *,
               allow_symlinks: bool,
               max_file_size: Optional[int]) -> None:
    '''``path``, confined to the tree at ``root``; usually made by
    ``SandboxContext.subpath``.
    '''
    self._root = root
    self._path = path
    self._allow_symlinks = allow_symlinks
    self._max_file_size = max_file_size
    self._validate()

  def _derive(self, path: Path) -> 'ConfinedPath':
    return ConfinedPath(self._root, path,
                        allow_symlinks = self._allow_symlinks,
                        max_file_size = self._max_file_size)

  def _illegal(self, path: Optional[Path] = None) -> IllegalFileAccessError:
    return IllegalFileAccessError(
      f"A request was made attempting to retrieve the file: "
      f"\"{path if path is not None else self._path}\"!"
    )

  def _relative_parts(self, path: Path) -> List[str]:
    try:
      parts = list(path.relative_to(self._root).parts)
    except ValueError:
      raise self._illegal(path) from None
    if any(part in ('', '.', '..') for part in parts):
      raise self._illegal(path)
    return parts

  def _validate(self) -> None:
    parts = self._relative_parts(self._path)
    if self._allow_symlinks:
      if not is_path_relative_to(self._path.resolve(), self._root.resolve()):
        raise self._illegal()
      return
    current = self._root
    for part in parts:
      current = current / part
      try:
        mode = os.lstat(current).st_mode
      except FileNotFoundError:
        return
      if not (stat.S_ISDIR(mode) or stat.S_ISREG(mode)):
        raise self._illegal()

  # Deriving paths (each validated)

  def __truediv__(self,
                  other: Union[str, ConfinedPathComponent]) -> 'ConfinedPath':
    return self.joinpath(other)

  def joinpath(self,
               *others: Union[str, ConfinedPathComponent]) -> 'ConfinedPath':
    '''Join ``others``: strings (from requests, say) or components taken
    from other ``ConfinedPath``s.
    '''
    parts: List[str] = []
    for other in others:
      if isinstance(other, ConfinedPathComponent):
        parts += other.parts
      elif isinstance(other, str):
        parts.append(other)
      else:
        raise TypeError(f"can't join {type(other).__name__} to a "
                        f"ConfinedPath")
    return self._derive(self._path.joinpath(*parts))

  def relative_to(self, other: 'ConfinedPath') -> ConfinedPathComponent:
    '''This path relative to ``other``, which it must lie under.'''
    try:
      relative = self._path.relative_to(other._path)
    except ValueError:
      raise ValueError(f"{self!r} is not under {other!r}") from None
    return ConfinedPathComponent(relative.parts)

  def subpath(self, path: Path) -> 'ConfinedPath':
    '''``path`` (a plain path claimed to be in this path's tree),
    confined.
    '''
    return self._derive(path)

  @property
  def parent(self) -> 'ConfinedPath':
    if self._path == self._root:
      raise self._illegal(self._path.parent)
    return self._derive(self._path.parent)

  def with_name(self, name: str) -> 'ConfinedPath':
    return self._derive(self._path.with_name(name))

  def with_suffix(self, suffix: str) -> 'ConfinedPath':
    return self._derive(self._path.with_suffix(suffix))

  # Properties

  @property
  def path(self) -> Path:
    '''The plain path, for commands and libraries; using it bypasses
    the checks reading through this object makes.
    '''
    return self._path

  @property
  def tree_root(self) -> 'ConfinedPath':
    '''The root of this path's tree.'''
    return self._derive(self._root)

  @property
  def relative(self) -> ConfinedPathComponent:
    '''This path relative to its tree's root.'''
    return self.relative_to(self.tree_root)

  @property
  def name(self) -> str:
    return self._path.name

  @property
  def suffix(self) -> str:
    return self._path.suffix

  @property
  def suffixes(self) -> List[str]:
    return self._path.suffixes

  @property
  def stem(self) -> str:
    return self._path.stem

  def __str__(self) -> str:
    return str(self._path)

  def __repr__(self) -> str:
    links = 'links inside' if self._allow_symlinks else 'no links'
    return (f"ConfinedPath({str(self._path)!r} in {str(self._root)!r}, "
            f"{links})")

  def __eq__(self, other: object) -> bool:
    if not isinstance(other, ConfinedPath):
      return NotImplemented
    return self._key() == other._key()

  def __hash__(self) -> int:
    return hash(self._key())

  def _key(self) -> Tuple[Path, Path, bool, Optional[int]]:
    return (self._root, self._path, self._allow_symlinks,
            self._max_file_size)

  # File system queries (never following a link in an untrusted tree)

  def _mode(self) -> Optional[int]:
    try:
      if self._allow_symlinks:
        return os.stat(self._path).st_mode
      return os.lstat(self._path).st_mode
    except (FileNotFoundError, NotADirectoryError):
      return None

  def exists(self) -> bool:
    return self._mode() is not None

  def is_file(self) -> bool:
    mode = self._mode()
    return mode is not None and stat.S_ISREG(mode)

  def is_dir(self) -> bool:
    mode = self._mode()
    return mode is not None and stat.S_ISDIR(mode)

  def walk(self) -> Iterator[Tuple['ConfinedPath', List[str], List[str]]]:
    '''Like ``os.walk`` (top-down, not following links), yielding each
    directory as a ``ConfinedPath``. Directory names may be pruned in place.
    In an untrusted tree, entries that are neither directories nor regular
    files are left out.
    '''
    def has_type(directory: str, name: str, is_type: Any) -> bool:
      try:
        return bool(is_type(os.lstat(os.path.join(directory, name)).st_mode))
      except FileNotFoundError:
        return False

    self._validate()
    for directory, dirs, files in os.walk(self._path):
      if not self._allow_symlinks:
        dirs[:] = [d for d in dirs if has_type(directory, d, stat.S_ISDIR)]
        files[:] = [f for f in files
                    if has_type(directory, f, stat.S_ISREG)]
      yield self._derive(Path(directory)), dirs, files

  # Reading

  def open(self, mode: str = 'r', **kwargs: Any) -> IO[Any]:
    '''Open for reading (``mode`` 'r' or 'rb', other arguments as for
    ``open``).
    '''
    if mode not in ('r', 'rb'):
      raise ValueError(f"unsupported mode: {mode!r}")
    self._validate()
    if self._allow_symlinks:
      handle = open(self._path, mode, **kwargs)
    else:
      handle = self._open_without_links(mode, **kwargs)
    try:
      # The size of what was opened; the trees served don't change while
      # they're read.
      size = os.fstat(handle.fileno()).st_size
      if self._max_file_size is not None and size > self._max_file_size:
        raise FileTooLargeError(
          f"\"{self._path}\" is too large to read ({size} bytes; the "
          f"limit is {self._max_file_size})"
        )
    except BaseException:
      handle.close()
      raise
    return handle

  def read_text(self, **kwargs: Any) -> str:
    with self.open('r', **kwargs) as handle:
      return str(handle.read())

  def read_bytes(self) -> bytes:
    with self.open('rb') as handle:
      return bytes(handle.read())

  def _open_without_links(self, mode: str, **kwargs: Any) -> IO[Any]:
    '''Open this path for reading without following a link anywhere
    along it, in a tree that doesn't allow links.

    ``_validate`` already checked the path, but a plain ``open`` would
    resolve it all over again: a component replaced by a link after the
    check (or a FIFO put in place of the file) would be followed (or
    waited on). So the open enforces the rules itself, one component at a
    time, and checks what it actually opened. (``openat2`` with
    ``RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS`` does the same in one call,
    but Python has no wrapper for it.)
    '''
    parts = self._relative_parts(self._path)
    # The root itself is a directory; there's no file to open.
    if len(parts) == 0:
      raise self._illegal()

    # O_NOFOLLOW only applies to the last component of the name it's given,
    # so every name opened below is a single component, relative to the
    # directory opened before it (dir_fd); none is resolved through a link.
    # O_CLOEXEC keeps the descriptors from leaking into commands we run.
    flags = os.O_RDONLY | os.O_NOFOLLOW | getattr(os, 'O_CLOEXEC', 0)
    dir_fd = os.open(str(self._root), flags | os.O_DIRECTORY)
    try:
      # Descend through the directories. O_DIRECTORY with O_NOFOLLOW refuses
      # a link to a directory (ELOOP, or ENOTDIR on some systems) and
      # anything else that isn't a directory (ENOTDIR).
      for part in parts[:-1]:
        next_fd = os.open(part, flags | os.O_DIRECTORY, dir_fd = dir_fd)
        os.close(dir_fd)
        dir_fd = next_fd
      # Open the file itself. O_NOFOLLOW refuses a link (ELOOP), and
      # O_NONBLOCK makes opening a FIFO return at once instead of waiting
      # for a writer (it's then refused below, like any non-regular file).
      fd = os.open(parts[-1], flags | os.O_NONBLOCK, dir_fd = dir_fd)
    except OSError as ex:
      # A link (or a non-directory where a directory should be) is an
      # attempt to leave the tree; report it like any other. Anything else
      # (e.g. a missing file) is an ordinary error.
      if ex.errno in (errno.ELOOP, errno.ENOTDIR):
        raise self._illegal() from ex
      raise
    finally:
      os.close(dir_fd)

    try:
      # Check what was actually opened (not what _validate saw): only a
      # regular file may be read, never a FIFO, device, or socket.
      if not stat.S_ISREG(os.fstat(fd).st_mode):
        raise self._illegal()
      # Regular files don't block anyway; restore normal reads.
      os.set_blocking(fd, True)
      # The file object takes ownership of the descriptor.
      return open(fd, mode, **kwargs)
    except BaseException:
      os.close(fd)
      raise


# Seconds an external command run for a reviewer may take, by default.
DEFAULT_COMMAND_TIMEOUT_S = 120.0


class CompletedCommand(NamedTuple):
  '''How an external command ended: its exit status (negative for a
  signal), its output (stdout and stderr together), and whether it was
  killed for taking too long.
  '''
  returncode: int
  output: bytes
  timed_out: bool


# Runs a command (an argument vector) within a timeout (seconds).
CommandRunner = Callable[[Sequence[str], float], Awaitable[CompletedCommand]]


async def run_command_locally(args: Sequence[str],
                              timeout: float) -> CompletedCommand:
  '''Run ``args`` here, as a subprocess (stdin closed, stderr merged into
  the output), killing it and whatever it started after ``timeout``
  seconds, or if the caller gives up on it.
  '''
  process = await asyncio.create_subprocess_exec(
    *args,
    stdin = asyncio.subprocess.DEVNULL,
    stdout = asyncio.subprocess.PIPE,
    stderr = asyncio.subprocess.STDOUT,
    start_new_session = True
  )

  def kill() -> None:
    try:
      os.killpg(process.pid, signal.SIGKILL)
    except (ProcessLookupError, PermissionError):
      pass

  try:
    stdout, _ = await asyncio.wait_for(process.communicate(), timeout)
  except asyncio.TimeoutError:
    kill()
    stdout, _ = await process.communicate()
    return CompletedCommand(-1, stdout, True)
  except BaseException:
    kill()
    await process.wait()
    raise
  assert process.returncode is not None
  return CompletedCommand(process.returncode, stdout, False)


class SandboxContext:
  '''A security context: the trees a provider may read, whether links in
  them may be followed, and how to run external commands on them.
  Providers make ``ConfinedPath``s with one (``subpath``), reach every file
  through them, and run every command through the context itself
  (``run_command``), which they are handed alongside the paths.

  - ``roots``: the trees that may be read, e.g. a test home and a run
    directory given with --root that lies elsewhere. None is special; a
    root inside another (lexically) is folded into it, and every path is
    confined to the one root holding it.
  - ``allow_symlinks``: a trusted tree (a local review) may use links, as
    long as they stay inside their root. A tree with someone else's content
    (e.g. a pull request served by acknowledg-forge) may not: no link is
    followed.
  - ``command_runner``: runs every command on the trees (e.g. in a sandbox
    that only shows the command the trees, read-only; it must cover every
    root). By default, commands run here (``run_command_locally``).
  - ``command_timeout``: seconds such a command may run.
  - ``max_file_size``: bytes a file read may have (None for no limit).
  '''

  def __init__(self,
               *roots: Path,
               allow_symlinks: bool = True,
               command_runner: Optional[CommandRunner] = None,
               command_timeout: float = DEFAULT_COMMAND_TIMEOUT_S,
               max_file_size: Optional[int] = DEFAULT_MAX_FILE_SIZE) -> None:
    if len(roots) == 0:
      raise ValueError('a sandbox context needs at least one root')
    reduced: List[Path] = []
    for root in roots:
      if not any(is_path_relative_to(root, other) for other in reduced):
        reduced = [other for other in reduced
                   if not is_path_relative_to(other, root)]
        reduced.append(root)
    self._roots = tuple(reduced)
    self._allow_symlinks = allow_symlinks
    self._command_runner = command_runner or run_command_locally
    self._command_timeout = command_timeout
    self._max_file_size = max_file_size

  @property
  def roots(self) -> Tuple[Path, ...]:
    '''The (reduced) roots, as plain paths (where the checks end).'''
    return self._roots

  @property
  def allow_symlinks(self) -> bool:
    return self._allow_symlinks

  @property
  def command_timeout(self) -> float:
    return self._command_timeout

  @property
  def max_file_size(self) -> Optional[int]:
    return self._max_file_size

  def subpath(self, path: Path) -> ConfinedPath:
    '''``path`` (a plain path claimed to be in one of the roots),
    confined to that root.
    '''
    for root in self._roots:
      if is_path_relative_to(path, root):
        return ConfinedPath(root, path,
                            allow_symlinks = self._allow_symlinks,
                            max_file_size = self._max_file_size)
    raise IllegalFileAccessError(
      f"A request was made attempting to retrieve the file: \"{path}\"!"
    )

  async def run_command(self, *args: str) -> CompletedCommand:
    '''Run ``args`` in this context, within its ``command_timeout``. Files
    should come from ``ConfinedPath``s (so they were validated); the
    command only sees those under this context's roots.
    '''
    return await self._command_runner(args, self._command_timeout)
