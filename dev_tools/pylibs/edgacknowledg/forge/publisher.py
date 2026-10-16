# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Publishing review sections to AcknowlEDG over one batch connection.

The service publishes through ``SubprocessPublisher``: the handlers that
answer reviewers read pull request content, so they run in a separate,
sandboxed process (``publisher_worker``, wrapping ``AcknowledgPublisher``)
that sees only the review trees, read-only, and holds no secret but the
AcknowlEDG API key.
'''

import asyncio
import importlib.util
import json
import logging
import os
import sys
import time
import uuid

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional, Protocol, Sequence

from edgacknowledg.client import (
  DEFAULT_OPEN_TIMEOUT_S,
  BatchClientSession,
  ClientError,
  NotConnectedError,
  ReviewClient,
  form_section_http_url,
)
from edgacknowledg.client.bench import BenchReviewSession, BenchSourceHandler
from edgacknowledg.client.shared import (
  BackgroundWorkerPool,
  ReviewClientHandler,
)
from edgacknowledg.client.test import TestSourceHandler
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.sandbox import Sandbox, SandboxUnavailableError
from edgacknowledg.forge.workspace import require_real_directory_under

_log = logging.getLogger(__name__)

# Section kinds and the AcknowlEDG subscription type serving each.
KIND_TEST_CHANGES = 'test.changes'
KIND_TEST_FAILURES = 'test.failures'
KIND_BENCH = 'bench'

_SUBSCRIPTION_TYPES = {
  KIND_TEST_CHANGES: 'test.src',
  KIND_TEST_FAILURES: 'test.src',
  KIND_BENCH: 'bench.src',
}

# Parallel cg_annotate runs across all benchmark sections.
_BENCH_WORKERS = 2


class PublisherUnavailableError(Exception):
  '''Raised when AcknowlEDG can't be reached right now (retry later).'''


@dataclass(frozen = True)
class ReviewIdentity:
  review_id: uuid.UUID
  name: str
  timestamp: str


class Publisher(Protocol):
  '''Where review sections are served (AcknowlEDG, or a test double).'''

  async def wait_connected(self) -> None:
    ...

  async def sync(self,
                 review: ReviewIdentity,
                 sections: Sequence[SectionRecord]) -> None:
    '''Serve exactly ``sections`` for ``review``.'''
    ...

  async def remove(self, review_id: uuid.UUID) -> None:
    ...

  def section_url(self,
                  review_id: uuid.UUID,
                  section: SectionRecord) -> Optional[str]:
    ...


@dataclass
class _ServedSection:
  record: SectionRecord
  handler: ReviewClientHandler


class AcknowledgPublisher:
  '''Serves sections with the stock AcknowlEDG handlers, read-only.

  The handlers treat the trees as untrusted (``Sandbox.context_for``): they
  follow no symlinks, and with ``reviews_dir`` every section's directories
  must be real directories under it.
  '''

  def __init__(self,
               *,
               api_host: str,
               api_port: int,
               api_key: Optional[str],
               ui_host: str,
               ui_port: int,
               sandbox: Sandbox,
               reviews_dir: Optional[Path] = None) -> None:
    self._session = BatchClientSession(
      status_listener = self,
      api_host = api_host,
      api_port = api_port,
      api_key = api_key,
      open_timeout = DEFAULT_OPEN_TIMEOUT_S
    )
    self._ui_host = ui_host
    self._ui_port = ui_port
    self._sandbox = sandbox
    self._reviews_dir = reviews_dir
    self._connected = asyncio.Event()
    self._lock = asyncio.Lock()
    self._reviews: dict[uuid.UUID, ReviewClient] = {}
    self._served: dict[uuid.UUID, dict[str, _ServedSection]] = {}
    self._bench_workers = BackgroundWorkerPool(_BENCH_WORKERS)

  # ConnectionStatusListener
  def connected(self) -> None:
    _log.info('connected to AcknowlEDG')
    self._connected.set()

  def attempting_reconnect(self) -> None:
    pass

  def disconnected(self) -> None:
    _log.warning('lost the AcknowlEDG connection; reconnecting')
    self._connected.clear()

  async def run(self) -> None:
    '''Keep the batch connection up until cancelled.'''
    self._bench_workers.ensure_started()
    try:
      await self._session.start()
      await self._session.stopped()
    finally:
      await self._session.stop()

  async def wait_connected(self) -> None:
    await self._connected.wait()

  def _require_connected(self) -> None:
    if not self._connected.is_set():
      raise PublisherUnavailableError('not connected to AcknowlEDG')

  def section_url(self,
                  review_id: uuid.UUID,
                  section: SectionRecord) -> Optional[str]:
    return form_section_http_url(
      self._ui_host,
      self._ui_port,
      review_id,
      _SUBSCRIPTION_TYPES[section.kind],
      section.slug
    )

  def _check_directories(self, section: SectionRecord) -> None:
    '''Require the section's directories to be real ones (no symlink
    anywhere below the reviews directory), the root and baselines inside
    the home; bubblewrap and the handlers rely on that.
    '''
    if self._reviews_dir is None:
      return
    directories = [section.home, section.root]
    if 'baseline_dir' in section.details:
      directories.append(Path(section.details['baseline_dir']))
    for directory in directories:
      require_real_directory_under(self._reviews_dir, directory)
    for directory in directories[1:]:
      require_real_directory_under(section.home, directory)

  def _build_handler(self, section: SectionRecord) -> ReviewClientHandler:
    self._check_directories(section)
    # Files are read without following links, and commands run on reviewers'
    # behalf (ifc-dump, cg_annotate) only see the section's tree, read-only.
    context = self._sandbox.context_for(section.home)
    if section.kind in (KIND_TEST_CHANGES, KIND_TEST_FAILURES):
      return TestSourceHandler(
        section.name,
        section.slug,
        context.subpath(section.home),
        context.subpath(section.root)
      )
    if section.kind == KIND_BENCH:
      session = BenchReviewSession(
        run_dir = context.subpath(section.root),
        baseline_dir = context.subpath(Path(section.details['baseline_dir'])),
        tag = section.details['tag']
      )
      return BenchSourceHandler(
        section.name,
        section.slug,
        session,
        self._bench_workers
      )
    raise ValueError(f"unknown section kind: {section.kind}")

  async def sync(self,
                 review: ReviewIdentity,
                 sections: Sequence[SectionRecord]) -> None:
    async with self._lock:
      self._require_connected()
      try:
        await self._sync(review, sections)
      except (NotConnectedError, ClientError) as ex:
        raise PublisherUnavailableError(str(ex)) from ex

  async def _sync(self,
                  review: ReviewIdentity,
                  sections: Sequence[SectionRecord]) -> None:
    if len(sections) == 0:
      await self._remove(review.review_id)
      return

    client_review = self._reviews.get(review.review_id)
    if client_review is None:
      client_review = await self._session.create_review(
        review.review_id,
        review.name,
        review.timestamp
      )
      self._reviews[review.review_id] = client_review
    served = self._served.setdefault(review.review_id, {})
    registered = client_review.sections()
    desired = {section.slug: section for section in sections}

    for slug, current in list(served.items()):
      wanted = desired.get(slug)
      still_registered = any(h is current.handler for h in registered)
      if (wanted is not None and still_registered and
          wanted.name == current.record.name and
          wanted.source_key == current.record.source_key and
          wanted.root == current.record.root):
        continue
      if still_registered:
        await client_review.remove_section(current.handler)
      else:
        # Dropped by the client (e.g. another provider held the slot);
        # re-adding it below heals the section.
        _log.warning('section %s of review %s was dropped; re-adding',
                     slug, review.review_id)
      del served[slug]

    for slug, section in desired.items():
      if slug in served:
        continue
      handler = await asyncio.to_thread(self._build_handler, section)
      await client_review.add_section(handler)
      served[slug] = _ServedSection(section, handler)

  async def remove(self, review_id: uuid.UUID) -> None:
    async with self._lock:
      await self._remove(review_id)

  async def _remove(self, review_id: uuid.UUID) -> None:
    client_review = self._reviews.pop(review_id, None)
    self._served.pop(review_id, None)
    if client_review is not None:
      await self._session.remove_review(client_review)


# Messages between ``SubprocessPublisher`` and ``publisher_worker``: one JSON
# object per line. The parent sends one initialization message (settings and
# the API key, kept out of the worker's command line and environment), then
# requests ({'id', 'op': 'sync' | 'remove', ...}); the worker answers each
# ({'id', 'ok': true} or {'id', 'error': 'unavailable' | 'failed',
# 'message'}) and reports its connection ({'event': 'connected' |
# 'disconnected'}).

WORKER_MODULE = 'edgacknowledg.forge.publisher_worker'

# Longest message line (a sync of every section of a review is small; this
# is a sanity limit).
MESSAGE_LIMIT = 16 * 1024 * 1024


def encode_review(review: ReviewIdentity) -> dict[str, Any]:
  return {
    'review_id': str(review.review_id),
    'name': review.name,
    'timestamp': review.timestamp,
  }


def decode_review(value: dict[str, Any]) -> ReviewIdentity:
  return ReviewIdentity(
    review_id = uuid.UUID(value['review_id']),
    name = str(value['name']),
    timestamp = str(value['timestamp'])
  )


def encode_section(section: SectionRecord) -> dict[str, Any]:
  return {
    'kind': section.kind,
    'name': section.name,
    'slug': section.slug,
    'source_key': section.source_key,
    'content_fingerprint': section.content_fingerprint,
    'root': str(section.root),
    'home': str(section.home),
    'details': section.details,
  }


def decode_section(value: dict[str, Any]) -> SectionRecord:
  return SectionRecord(
    kind = str(value['kind']),
    name = str(value['name']),
    slug = str(value['slug']),
    source_key = str(value['source_key']),
    content_fingerprint = str(value['content_fingerprint']),
    root = Path(value['root']),
    home = Path(value['home']),
    details = dict(value['details']),
  )


def _python_paths() -> list[Path]:
  '''The directories this interpreter loads code from (which the worker
  needs to see): its installation, the virtual environment, ``sys.path``,
  and wherever the EDG packages actually live (e.g. an editable install's
  sources).
  '''
  paths = [Path(sys.prefix), Path(sys.base_prefix)]
  # Every link on the way to the interpreter must resolve in the sandbox
  # too (e.g. a virtual environment's bin/python, or a chain of them).
  executable = sys.executable
  for _ in range(40):
    paths.append(Path(executable).parent)
    if not os.path.islink(executable):
      break
    executable = os.path.join(
      os.path.dirname(executable),
      os.readlink(executable)
    )
  paths += [Path(entry) for entry in sys.path if entry != '']
  for name in ('edgacknowledg', 'edgtest', 'edgutil'):
    spec = importlib.util.find_spec(name)
    if spec is not None and spec.submodule_search_locations is not None:
      paths += [Path(location).parent
                for location in spec.submodule_search_locations]
  result: list[Path] = []
  for path in paths:
    path = path.absolute()
    if path.is_dir() and path not in result:
      result.append(path)
  return result


def _outermost(paths: Sequence[Path]) -> list[Path]:
  '''``paths`` without those inside another of them.'''
  return [
    path for path in paths
    if not any(other != path and path.is_relative_to(other)
               for other in paths)
  ]


class SubprocessPublisher:
  '''Publishes through a ``publisher_worker`` process it supervises.

  The worker runs under ``sandbox.service_prefix``: it sees ``reviews_dir``,
  the frozen tools, and the Python installation, all read-only, and has the
  network (for AcknowlEDG), but not the service's home, database,
  credentials, or clones. Should it exit, it is restarted and every
  review's sections are served again.
  '''

  def __init__(self,
               *,
               api_host: str,
               api_port: int,
               api_key: Optional[str],
               ui_host: str,
               ui_port: int,
               sandbox: Sandbox,
               reviews_dir: Path,
               home: Path,
               request_timeout: float = 15 * 60.0,
               restart_delay: float = 1.0,
               max_restart_delay: float = 60.0) -> None:
    self._api_host = api_host
    self._api_port = api_port
    self._api_key = api_key
    self._ui_host = ui_host
    self._ui_port = ui_port
    self._sandbox = sandbox
    self._reviews_dir = reviews_dir
    self._home = home
    self._request_timeout = request_timeout
    self._restart_delay = restart_delay
    self._max_restart_delay = max_restart_delay

    self._process: Optional[asyncio.subprocess.Process] = None
    self._connected = asyncio.Event()
    # Serializes requests, and replaying after a restart.
    self._lock = asyncio.Lock()
    self._next_id = 0
    self._pending: dict[int, asyncio.Future[None]] = {}
    # What each review should serve, to replay to a restarted worker.
    self._desired: dict[uuid.UUID,
                        tuple[ReviewIdentity, list[SectionRecord]]] = {}
    self._needs_replay = False
    self._replay_task: Optional[asyncio.Task[None]] = None

  # Publisher

  async def wait_connected(self) -> None:
    await self._connected.wait()

  def section_url(self,
                  review_id: uuid.UUID,
                  section: SectionRecord) -> Optional[str]:
    return form_section_http_url(
      self._ui_host,
      self._ui_port,
      review_id,
      _SUBSCRIPTION_TYPES[section.kind],
      section.slug
    )

  async def sync(self,
                 review: ReviewIdentity,
                 sections: Sequence[SectionRecord]) -> None:
    async with self._lock:
      if not self._connected.is_set():
        raise PublisherUnavailableError('not connected to AcknowlEDG')
      await self._sync(review, sections)

  async def remove(self, review_id: uuid.UUID) -> None:
    async with self._lock:
      # A restarted worker serves nothing for it either way.
      self._desired.pop(review_id, None)
      if self._process is not None:
        await self._request({'op': 'remove', 'review_id': str(review_id)})

  async def _sync(self,
                  review: ReviewIdentity,
                  sections: Sequence[SectionRecord]) -> None:
    await self._request({
      'op': 'sync',
      'review': encode_review(review),
      'sections': [encode_section(section) for section in sections],
    })
    if len(sections) == 0:
      self._desired.pop(review.review_id, None)
    else:
      self._desired[review.review_id] = (review, list(sections))

  # Worker requests

  async def _request(self, request: dict[str, Any]) -> None:
    process = self._process
    if process is None or process.stdin is None:
      raise PublisherUnavailableError('the publisher worker is not running')
    self._next_id += 1
    request_id = self._next_id
    future: asyncio.Future[None] = (
      asyncio.get_running_loop().create_future()
    )
    self._pending[request_id] = future
    try:
      line = json.dumps({'id': request_id, **request}) + '\n'
      try:
        process.stdin.write(line.encode())
        await process.stdin.drain()
      except (BrokenPipeError, ConnectionResetError) as ex:
        raise PublisherUnavailableError(
          f"the publisher worker went away: {ex}"
        ) from ex
      try:
        await asyncio.wait_for(asyncio.shield(future),
                               self._request_timeout)
      except asyncio.TimeoutError:
        _log.error('the publisher worker did not answer within %s s; '
                   'restarting it', self._request_timeout)
        self._kill(process)
        raise PublisherUnavailableError(
          'the publisher worker stopped answering'
        ) from None
    finally:
      self._pending.pop(request_id, None)

  def _handle_message(self, line: bytes) -> None:
    try:
      value = json.loads(line)
    except ValueError:
      _log.error('bad message from the publisher worker: %r', line[:200])
      return
    event = value.get('event')
    if event == 'connected':
      if self._replay_task is None or self._replay_task.done():
        self._replay_task = asyncio.create_task(self._on_connected())
      return
    if event == 'disconnected':
      self._connected.clear()
      return
    future = self._pending.get(value.get('id'))
    if future is None or future.done():
      return
    error = value.get('error')
    if error is None:
      future.set_result(None)
    elif error == 'unavailable':
      future.set_exception(
        PublisherUnavailableError(str(value.get('message')))
      )
    else:
      future.set_exception(
        RuntimeError(f"publisher worker: {value.get('message')}")
      )

  async def _on_connected(self) -> None:
    '''Once the worker is connected, serve again what a previous worker
    served; only then accept new requests.
    '''
    async with self._lock:
      if self._needs_replay:
        for review, sections in list(self._desired.values()):
          try:
            await self._sync(review, sections)
          except PublisherUnavailableError as ex:
            _log.warning('serving the reviews again failed (%s); will '
                         'retry on the next connection', ex)
            return
          except Exception:
            # Don't let one review hold up every other one; it is served
            # again once its pull request changes.
            _log.exception('serving review %s again failed; dropping it',
                           review.review_id)
            self._desired.pop(review.review_id, None)
        self._needs_replay = False
      _log.info('connected to AcknowlEDG')
      self._connected.set()

  # Supervision

  def _worker_command(self) -> list[str]:
    python_paths = _python_paths()
    command = [sys.executable, '-m', WORKER_MODULE]
    env = {
      'HOME': '/tmp',
      'PYTHONUTF8': '1',
      'PYTHONDONTWRITEBYTECODE': '1',
      'PYTHONPATH': os.pathsep.join(str(path) for path in python_paths),
    }
    if not self._sandbox.enabled:
      return command
    binds = _outermost([*python_paths, self._reviews_dir])
    for path in binds:
      # The worker must never see the service's own files.
      if path != self._reviews_dir and (self._home.is_relative_to(path) or
                                        path.is_relative_to(self._home)):
        raise SandboxUnavailableError(
          f"the publisher worker would see the service's home "
          f"({self._home}) through the Python path entry {path}"
        )
    return [
      *self._sandbox.service_prefix(read_only = binds, env = env),
      *command,
    ]

  def _worker_env(self) -> Optional[dict[str, str]]:
    if self._sandbox.enabled:
      # bubblewrap sets the worker's environment; it needs none itself.
      return {'PATH': os.environ.get('PATH', '/usr/bin:/bin')}
    return {
      'PATH': f"{self._sandbox.search_path}:{os.environ.get('PATH', '')}",
      'HOME': os.environ.get('HOME', '/tmp'),
      'PYTHONUTF8': '1',
      'PYTHONPATH': os.pathsep.join(str(path) for path in _python_paths()),
    }

  def _init_message(self) -> dict[str, Any]:
    return {
      'api_host': self._api_host,
      'api_port': self._api_port,
      'api_key': self._api_key,
      'ui_host': self._ui_host,
      'ui_port': self._ui_port,
      'reviews_dir': str(self._reviews_dir),
      'sandbox': {
        'enabled': self._sandbox.enabled,
        'tools_dir': (
          str(self._sandbox.tools_dir)
          if self._sandbox.tools_dir is not None else None
        ),
        'bwrap': self._sandbox.bwrap,
      },
    }

  @staticmethod
  def _kill(process: asyncio.subprocess.Process) -> None:
    if process.returncode is None:
      try:
        process.kill()
      except ProcessLookupError:
        pass

  async def run(self) -> None:
    '''Run the worker, restarting it whenever it exits, until cancelled.'''
    self._reviews_dir.mkdir(parents = True, exist_ok = True)
    delay = self._restart_delay
    while True:
      started = time.monotonic()
      try:
        await self._run_worker()
      except asyncio.CancelledError:
        raise
      except Exception:
        _log.exception('the publisher worker failed')
      if time.monotonic() - started > self._max_restart_delay:
        delay = self._restart_delay
      _log.warning('the publisher worker exited; restarting in %.0f s',
                   delay)
      await asyncio.sleep(delay)
      delay = min(delay * 2, self._max_restart_delay)

  async def _run_worker(self) -> None:
    process = await asyncio.create_subprocess_exec(
      *self._worker_command(),
      stdin = asyncio.subprocess.PIPE,
      stdout = asyncio.subprocess.PIPE,
      env = self._worker_env(),
      limit = MESSAGE_LIMIT
    )
    assert process.stdin is not None and process.stdout is not None
    self._process = process
    try:
      process.stdin.write(
        (json.dumps(self._init_message()) + '\n').encode()
      )
      await process.stdin.drain()
      while True:
        line = await process.stdout.readline()
        if line == b'':
          break
        self._handle_message(line)
    finally:
      self._process = None
      self._connected.clear()
      self._needs_replay = True
      for future in self._pending.values():
        if not future.done():
          future.set_exception(
            PublisherUnavailableError('the publisher worker exited')
          )
      self._kill(process)
      await process.wait()
      if self._replay_task is not None:
        self._replay_task.cancel()
