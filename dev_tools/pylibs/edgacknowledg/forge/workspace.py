# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''On-disk workspaces: clones, worktrees, run directories, artifacts.

Disk use is kept small because pull requests may stay open for a long time:

- One bare, partial (blobless) clone per repository, shared by every review
  of it, fetching only the refs the open pull requests need.
- Per-review worktrees with sparse checkouts listing individual tests (a
  single test directory can hold thousands of unrelated tests), created
  only when a section needs them.
- Run directories are swapped in by rename and deleted once superseded.

Everything taken from a pull request or a CI artifact is untrusted:

- Clones check symlinks out as plain files holding the link's target
  (``core.symlinks=false``) and fsck what they fetch, so no worktree holds a
  link a pull request committed; ``check_plain_tree`` verifies that after
  every checkout, and checks what sandboxed commands write too.
- Paths are checked for symlinks before the service writes through them.
- Archives are size-capped and may not contain links or escaping paths.
'''

import asyncio
import base64
import logging
import os
import re
import secrets
import shutil
import stat
import tarfile
import urllib.parse
import zipfile

from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Iterable, Optional, Sequence

from edgacknowledg.forge import Forge, PullRequestInfo
from edgacknowledg.forge.runner.roles import BUILD_STAGING_DIR, REVIEWS_DIR

_log = logging.getLogger(__name__)

# Test file extensions (see edgtest.DEFAULT_TEST_EXTENSIONS).
_TEST_EXTENSIONS = ('sft.cpp', 'sft.c', 'mft.cpp', 'mft.c')

_DIFF_HEADER = re.compile(r'^diff --git a/(.*) b/(.*)$')

# Paths every review worktree needs: the test home's configuration and
# expectations (both small).
_TEST_HOME_PATTERNS = ('/tests/.edgy/', '/tests/expectations/')

BENCH_BASELINE_PATTERNS = ('/benchmarks/baselines/',)


class WorkspaceError(Exception):
  '''Raised when a workspace operation fails or input is unsafe.'''


class UnsafeArchiveError(WorkspaceError):
  '''Raised for an artifact that is too large or has unsafe entries.'''


@dataclass(frozen = True)
class GitResult:
  returncode: int
  stdout: str
  stderr: str


def _escape_pattern(path: str) -> str:
  '''Escape gitignore-style pattern characters in a literal path.'''
  return re.sub(r'([\\*?\[\]!#])', r'\\\1', path)


def _test_base_names(file_name: str) -> list[str]:
  '''Return the candidate base names of a test file (for its '.<base>.rto'
  recordings directory), e.g. 'a.b.sft.cpp' -> ['a.b.sft', 'a.b', 'a'].
  '''
  parts = file_name.split('.')
  return ['.'.join(parts[:count]) for count in range(len(parts) - 1, 0, -1)]


def patterns_for_test(test_path: PurePosixPath) -> list[str]:
  '''Sparse-checkout patterns for one test (a repository-relative path such
  as 'tests/tests/suite/dir/name.sft.cpp'): the test file and its recordings,
  or the whole directory of a multi-file test.
  '''
  directory = _escape_pattern(str(test_path.parent))
  if '.mft.' in test_path.name:
    return [f"/{directory}/"]
  patterns = [f"/{_escape_pattern(str(test_path))}"]
  for base_name in _test_base_names(test_path.name):
    patterns.append(f"/{directory}/.{_escape_pattern(base_name)}.rto/")
  return patterns


def _patterns_for_changed_test_file(path: PurePosixPath) -> list[str]:
  '''Patterns covering a changed file under tests/tests/ and the test(s) it
  belongs to (mirroring edg-rematerialize-test-run's lookup).
  '''
  for index, part in enumerate(path.parts):
    if part.startswith('.') and part.endswith('.rto'):
      # A recording: '<dir>/.<base>.rto/...' belongs to '<dir>/<base>.*'.
      test_dir = _escape_pattern(str(PurePosixPath(*path.parts[:index])))
      base_name = _escape_pattern(part[1:-len('.rto')])
      return [
        f"/{test_dir}/.{base_name}.rto/",
        f"/{test_dir}/{base_name}.*",
      ]
  if any(path.name.endswith(f".{ext}") for ext in _TEST_EXTENSIONS):
    return patterns_for_test(path)
  # An associated file of a multi-file test; those live in the test's own
  # directory.
  return [f"/{_escape_pattern(str(path.parent))}/"]


def changed_paths(diff: str) -> list[str]:
  '''Return the paths a ``git diff --no-renames`` output touches.'''
  paths = []
  for line in diff.splitlines():
    match = _DIFF_HEADER.match(line)
    if match is not None:
      paths.append(match.group(1))
  return paths


def changes_patterns(diff: str) -> list[str]:
  '''Sparse-checkout patterns for rematerializing ``diff`` (of tests/) on
  the base tree: the test home and every affected test.
  '''
  patterns = list(_TEST_HOME_PATTERNS)
  current_suite: Optional[str] = None
  for line in diff.splitlines():
    match = _DIFF_HEADER.match(line)
    if match is not None:
      path = PurePosixPath(match.group(1))
      current_suite = None
      if path.parts[:2] == ('tests', 'tests'):
        patterns += _patterns_for_changed_test_file(path)
      elif (path.parts[:2] == ('tests', 'expectations') and
            path.suffix == '.log'):
        # Expectation lines name tests relative to the suite.
        current_suite = path.stem
      continue
    if current_suite is None or line.startswith(('---', '+++')):
      continue
    if line.startswith(('-', '+')):
      test_name = line[1:].split(':')[0].strip()
      if test_name:
        patterns += patterns_for_test(
          PurePosixPath('tests', 'tests', current_suite, test_name)
        )
  return _unique(patterns)


def failures_patterns(test_ids: Iterable[str]) -> list[str]:
  '''Sparse-checkout patterns for serving CI results naming ``test_ids``
  ('<suite>/<path>', as in .elist files), plus bench baselines.
  '''
  patterns = [*_TEST_HOME_PATTERNS, *BENCH_BASELINE_PATTERNS]
  for test_id in test_ids:
    test_path = PurePosixPath('tests', 'tests', test_id)
    if '..' in test_path.parts or test_path.is_absolute():
      continue
    patterns += patterns_for_test(test_path)
  return _unique(patterns)


def _unique(items: Iterable[str]) -> list[str]:
  return list(dict.fromkeys(items))


def ensure_real_directory(root: Path, relative: PurePosixPath) -> Path:
  '''Create ``root/relative`` one component at a time, refusing to go
  through anything that isn't a real directory (e.g. a symlink a pull
  request committed in place of 'tests/runs').
  '''
  current = root
  for part in relative.parts:
    if part in ('', '.', '..'):
      raise WorkspaceError(f"invalid path component in {relative}")
    current = current / part
    try:
      mode = os.lstat(current).st_mode
    except FileNotFoundError:
      current.mkdir()
      continue
    if not stat.S_ISDIR(mode):
      raise WorkspaceError(f"{current} is not a real directory")
  return current


def require_real_directory(root: Path, relative: PurePosixPath) -> Path:
  '''Return ``root/relative`` after checking that every component of it
  below ``root`` is a real directory (not a symlink); create nothing.
  '''
  current = root
  for part in relative.parts:
    if part in ('', '.', '..'):
      raise WorkspaceError(f"invalid path component in {relative}")
    current = current / part
    try:
      mode = os.lstat(current).st_mode
    except FileNotFoundError:
      raise WorkspaceError(f"{current} does not exist") from None
    if not stat.S_ISDIR(mode):
      raise WorkspaceError(f"{current} is not a real directory")
  return current


def require_real_directory_under(root: Path, path: Path) -> Path:
  '''``require_real_directory`` for an absolute ``path`` that must lie
  (lexically) under ``root``.
  '''
  try:
    relative = path.relative_to(root)
  except ValueError:
    raise WorkspaceError(f"{path} is not under {root}") from None
  return require_real_directory(root, PurePosixPath(*relative.parts))


def check_plain_tree(root: Path, *, allow_hard_links: bool = True) -> None:
  '''Raise ``WorkspaceError`` unless everything under ``root`` is a real
  directory or a regular file: no symlinks, FIFOs, sockets, or devices
  (and, unless ``allow_hard_links``, no regular file with other links).

  Run over trees that untrusted content shaped (worktrees, sandbox output)
  before anything outside a sandbox reads them.
  '''
  def check(path: Path) -> None:
    info = os.lstat(path)
    if stat.S_ISDIR(info.st_mode):
      return
    if not stat.S_ISREG(info.st_mode):
      raise WorkspaceError(f"{path} is not a regular file or directory")
    if not allow_hard_links and info.st_nlink > 1:
      raise WorkspaceError(f"{path} is hard-linked")

  def raise_error(error: OSError) -> None:
    raise error

  check(root)
  for directory, dirs, files in os.walk(root, onerror = raise_error):
    for name in [*dirs, *files]:
      check(Path(directory, name))


def remove_tree(path: Path) -> None:
  '''Remove a directory tree (without following symlinks), if present.'''
  try:
    mode = os.lstat(path).st_mode
  except FileNotFoundError:
    return
  if stat.S_ISDIR(mode):
    shutil.rmtree(path)
  else:
    path.unlink()


class _ByteBudget:
  def __init__(self, limit: int) -> None:
    self.remaining = limit

  def spend(self, count: int) -> None:
    self.remaining -= count
    if self.remaining < 0:
      raise UnsafeArchiveError('artifact exceeds the extracted size limit')


def _check_inner_tarball(path: Path, budget: _ByteBudget) -> None:
  '''Charge a (gzipped) tarball's decompressed size to ``budget``; reviewers'
  requests decompress these, so they're bombs-in-waiting otherwise.
  '''
  try:
    with tarfile.open(path, 'r:*') as archive:
      for member in archive:
        if not (member.isfile() or member.isdir()):
          raise UnsafeArchiveError(f"{path.name} contains a link or device")
        budget.spend(member.size)
  except tarfile.TarError as ex:
    raise UnsafeArchiveError(f"{path.name} is not a valid tarball") from ex


def extract_artifact(zip_path: Path,
                     destination: Path,
                     *,
                     max_bytes: int) -> None:
  '''Safely extract a CI artifact zip into the (new, empty) ``destination``.

  Rejects absolute or escaping paths and symlinks, and stops once more than
  ``max_bytes`` would be written (counting the decompressed contents of
  inner tarballs too).
  '''
  budget = _ByteBudget(max_bytes)
  tarballs = []
  try:
    with zipfile.ZipFile(zip_path) as archive:
      for info in archive.infolist():
        name = info.filename
        member_path = PurePosixPath(name)
        if (member_path.is_absolute() or '\\' in name or
            any(part in ('..', '') for part in member_path.parts)):
          raise UnsafeArchiveError(f"unsafe path in artifact: {name!r}")
        if stat.S_ISLNK(info.external_attr >> 16):
          raise UnsafeArchiveError(f"symlink in artifact: {name!r}")

        if info.is_dir():
          ensure_real_directory(destination, member_path)
          continue
        parent = ensure_real_directory(destination, member_path.parent)
        target = parent / member_path.name
        with archive.open(info) as source, open(target, 'xb') as sink:
          while True:
            chunk = source.read(1024 * 1024)
            if not chunk:
              break
            budget.spend(len(chunk))
            sink.write(chunk)
        if target.name.endswith(('.tar.gz', '.tgz', '.tar')):
          tarballs.append(target)
  except zipfile.BadZipFile as ex:
    raise UnsafeArchiveError('artifact is not a valid zip archive') from ex

  for tarball in tarballs:
    _check_inner_tarball(tarball, budget)


class Workspaces:
  '''Clones and worktrees under ``home`` for one forge.'''

  def __init__(self, home: Path, forge: Forge) -> None:
    self._home = home
    self._forge = forge
    self._repos_dir = home / 'repos' / forge.name
    self._reviews_dir = home / REVIEWS_DIR
    self._locks: dict[str, asyncio.Lock] = {}

  @property
  def reviews_dir(self) -> Path:
    return self._reviews_dir

  @property
  def build_staging_dir(self) -> Path:
    '''Where builds write (the builder runner sees the review trees only
    read-only); what they write is checked, then moved into a review.
    '''
    return self._home / BUILD_STAGING_DIR

  def clone_dir(self, repository: str) -> Path:
    return self._repos_dir / f"{urllib.parse.quote(repository, safe = '')}.git"

  def review_dir(self, review_id: object) -> Path:
    return self._reviews_dir / str(review_id)

  def lock(self, repository: str) -> asyncio.Lock:
    '''The lock serializing git operations on ``repository``'s clone.'''
    return self._locks.setdefault(repository, asyncio.Lock())

  def _git_env(self) -> dict[str, str]:
    env = {
      'PATH': os.environ.get('PATH', '/usr/bin:/bin'),
      'HOME': str(self._home),
      'LC_ALL': 'C',
      'GIT_TERMINAL_PROMPT': '0',
      'GIT_CONFIG_NOSYSTEM': '1',
      'GIT_CONFIG_GLOBAL': os.devnull,
    }
    # Pass credentials through the environment (GIT_CONFIG_*) rather than
    # the command line, where other users could see them.
    headers = list(self._forge.git_http_headers())
    env['GIT_CONFIG_COUNT'] = str(len(headers))
    for index, header in enumerate(headers):
      env[f"GIT_CONFIG_KEY_{index}"] = 'http.extraHeader'
      env[f"GIT_CONFIG_VALUE_{index}"] = header
    return env

  async def git(self,
                *args: str,
                git_dir: Optional[Path] = None,
                cwd: Optional[Path] = None,
                stdin: Optional[str] = None,
                check: bool = True) -> GitResult:
    command = ['git']
    if git_dir is not None:
      command += ['--git-dir', str(git_dir)]
    command += list(args)
    process = await asyncio.create_subprocess_exec(
      *command,
      stdin = (
        asyncio.subprocess.PIPE if stdin is not None
        else asyncio.subprocess.DEVNULL
      ),
      stdout = asyncio.subprocess.PIPE,
      stderr = asyncio.subprocess.PIPE,
      cwd = cwd,
      env = self._git_env()
    )
    stdout, stderr = await process.communicate(
      stdin.encode() if stdin is not None else None
    )
    assert process.returncode is not None
    result = GitResult(
      process.returncode,
      stdout.decode(errors = 'replace'),
      stderr.decode(errors = 'replace')
    )
    if check and result.returncode != 0:
      raise WorkspaceError(
        f"git {' '.join(args)} failed ({result.returncode}): "
        f"{result.stderr.strip()}"
      )
    return result

  async def _ensure_clone(self, repository: str) -> Path:
    clone_dir = self.clone_dir(repository)
    if clone_dir.exists():
      return clone_dir
    _log.info('creating a clone of %s', repository)
    self._repos_dir.mkdir(parents = True, exist_ok = True)
    staging = clone_dir.with_name(
      f"{clone_dir.name}.tmp-{secrets.token_hex(4)}"
    )
    await self.git('init', '--quiet', '--bare', str(staging))
    for key, value in (
        ('core.repositoryformatversion', '1'),
        ('remote.origin.url', self._forge.clone_url(repository)),
        # Blobless: blobs are fetched on demand for the files a sparse
        # checkout or diff actually needs.
        ('remote.origin.promisor', 'true'),
        ('remote.origin.partialclonefilter', 'blob:none'),
        ('extensions.partialClone', 'origin'),
        # Each worktree keeps its own sparse-checkout configuration.
        ('extensions.worktreeConfig', 'true'),
        ('core.hooksPath', os.devnull),
        # Check symlinks out as plain files holding their target, so a pull
        # request can't plant a link for the service to read through.
        ('core.symlinks', 'false'),
        # Reject malformed trees ('..' or '.git' components and the like).
        ('transfer.fsckObjects', 'true'),
        ('fetch.fsckObjects', 'true'),
        ('gc.autoDetach', 'false')):
      await self.git('config', key, value, git_dir = staging)
    # With per-worktree configuration, core.bare must be per-worktree too,
    # or the linked worktrees inherit it and stop working (git-worktree(1)).
    await self.git('config', '--unset', 'core.bare', git_dir = staging)
    await self.git(
      'config', '--worktree', 'core.bare', 'true',
      git_dir = staging
    )
    staging.rename(clone_dir)
    return clone_dir

  async def fetch_pull_request(self, info: PullRequestInfo) -> tuple[str, str]:
    '''Fetch the pull request's head and base branch; return their SHAs.'''
    async with self.lock(info.repository):
      clone_dir = await self._ensure_clone(info.repository)
      refs = f"refs/forge/{info.number}"
      await self.git(
        'fetch', '--quiet', '--no-tags', '--filter=blob:none', 'origin',
        f"+{info.fetch_ref}:{refs}/head",
        f"+refs/heads/{info.base_ref}:{refs}/base",
        git_dir = clone_dir
      )
      head = await self.git('rev-parse', f"{refs}/head", git_dir = clone_dir)
      base = await self.git('rev-parse', f"{refs}/base", git_dir = clone_dir)
      return head.stdout.strip(), base.stdout.strip()

  async def fetch_commit(self, info: PullRequestInfo, sha: str) -> bool:
    '''Fetch an arbitrary commit (e.g. the merge commit CI tested); return
    whether the forge provided it.
    '''
    async with self.lock(info.repository):
      clone_dir = await self._ensure_clone(info.repository)
      exists = await self.git(
        'cat-file', '-e', f"{sha}^{{commit}}",
        git_dir = clone_dir,
        check = False
      )
      if exists.returncode == 0:
        return True
      result = await self.git(
        'fetch', '--quiet', '--no-tags', '--filter=blob:none', 'origin',
        f"+{sha}:refs/forge/{info.number}/tested",
        git_dir = clone_dir,
        check = False
      )
      return result.returncode == 0

  async def merge_base(self, repository: str, base: str, head: str) -> str:
    async with self.lock(repository):
      result = await self.git(
        'merge-base', base, head,
        git_dir = self.clone_dir(repository)
      )
      return result.stdout.strip()

  async def diff(self,
                 repository: str,
                 base: str,
                 head: str,
                 paths: Sequence[str]) -> str:
    async with self.lock(repository):
      result = await self.git(
        'diff', '--no-renames', '--no-color', '--no-ext-diff', '--no-textconv',
        base, head, '--', *paths,
        git_dir = self.clone_dir(repository)
      )
      return result.stdout

  async def checkout(self,
                     info: PullRequestInfo,
                     review_id: object,
                     name: str,
                     sha: str,
                     patterns: Sequence[str]) -> Path:
    '''Check ``sha`` out sparsely (only ``patterns``) into the review's
    ``name`` worktree, creating it if needed; return the worktree root.

    Untracked run directories in the worktree are left alone. The result
    holds only real directories and regular files (``WorkspaceError``
    otherwise).
    '''
    async with self.lock(info.repository):
      clone_dir = await self._ensure_clone(info.repository)
      review_dir = ensure_real_directory(
        self._reviews_dir.parent,
        PurePosixPath(self._reviews_dir.name, str(review_id))
      )
      worktree = review_dir / name
      if not os.path.lexists(worktree):
        await self.git(
          'worktree', 'add', '--quiet', '--no-checkout', '--detach',
          str(worktree), sha,
          git_dir = clone_dir
        )
      await self.git(
        'sparse-checkout', 'set', '--no-cone', '--stdin',
        cwd = worktree,
        stdin = ''.join(f"{pattern}\n" for pattern in patterns)
      )
      await self.git('reset', '--quiet', '--hard', sha, cwd = worktree)
      await asyncio.to_thread(check_plain_tree, worktree)
      return worktree

  async def remove_review(self,
                          info: PullRequestInfo,
                          review_id: object) -> None:
    '''Remove a review's worktrees and directory, and the pull request's
    fetched refs.
    '''
    async with self.lock(info.repository):
      clone_dir = self.clone_dir(info.repository)
      review_dir = self.review_dir(review_id)
      if clone_dir.exists():
        if review_dir.is_dir():
          for worktree in review_dir.iterdir():
            if (worktree / '.git').is_file():
              await self.git(
                'worktree', 'remove', '--force', '--force', str(worktree),
                git_dir = clone_dir,
                check = False
              )
        refs = await self.git(
          'for-each-ref', '--format=%(refname)',
          f"refs/forge/{info.number}/",
          git_dir = clone_dir
        )
        for ref in refs.stdout.split():
          await self.git('update-ref', '-d', ref, git_dir = clone_dir)
      remove_tree(review_dir)
      if clone_dir.exists():
        await self.git('worktree', 'prune', git_dir = clone_dir)

  async def collect_garbage(self, repository: str, open_count: int) -> None:
    '''Delete ``repository``'s clone once no open pull request uses it
    (``open_count`` comes from the database, so a pull request without a
    worktree yet still keeps it); otherwise let git tidy it up.
    '''
    async with self.lock(repository):
      clone_dir = self.clone_dir(repository)
      if not clone_dir.exists():
        return
      if open_count == 0:
        _log.info('removing the clone of %s (no open pull requests)',
                  repository)
        remove_tree(clone_dir)
        return
      await self.git('worktree', 'prune', git_dir = clone_dir)
      await self.git('gc', '--auto', '--quiet', git_dir = clone_dir)


def basic_auth_header(user: str, password: str) -> str:
  '''An HTTP Basic 'Authorization' header (as git's http.extraHeader).'''
  token = base64.b64encode(f"{user}:{password}".encode()).decode()
  return f"Authorization: Basic {token}"
