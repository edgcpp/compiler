# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Processing: reconcile the database with workspaces and published reviews.

For every pull request that changed (see ``ForgeStore`` generations), work
out which sections it should have, build any that are missing or stale, and
publish exactly those. Closed pull requests (and ones no longer targeting a
served base branch) have their review and workspace removed.
'''

import asyncio
import hashlib
import json
import logging
import tempfile

from dataclasses import dataclass
from datetime import datetime
from pathlib import Path, PurePosixPath
from typing import Awaitable, Callable, Optional

import edgtest
import edgutil

from edgacknowledg import rich_link, rich_text, section_slug
from edgacknowledg.forge import (
  ArtifactInfo,
  CIConclusion,
  CIRunInfo,
  CIStatus,
  Forge,
  PullRequestState,
)
from edgacknowledg.forge.config import ForgeServiceConfig
from edgacknowledg.forge.db import ForgeStore, PullRequestRecord, SectionRecord
from edgacknowledg.forge.ingest import utc_now
from edgacknowledg.forge.publisher import (
  KIND_BENCH,
  KIND_TEST_CHANGES,
  KIND_TEST_FAILURES,
  Publisher,
  PublisherUnavailableError,
  ReviewIdentity,
)
from edgacknowledg.forge.sandbox import Sandbox
from edgacknowledg.forge.workspace import (
  BENCH_BASELINE_PATTERNS,
  WorkspaceError,
  Workspaces,
  changes_patterns,
  check_plain_tree,
  ensure_real_directory,
  extract_artifact,
  failures_patterns,
  remove_tree,
  require_real_directory,
)

_log = logging.getLogger(__name__)

CHANGES_NAME = 'Test - Changes'
FAILURES_NAME = 'Test - Failures'

# (artifact subdirectory, checked-in baseline tag, section name)
BENCH_SECTIONS = (
  ('c', 'release-c', 'Bench - C'),
  ('cpp', 'release-cpp', 'Bench - C++'),
)

CI_METADATA_FILE = 'ci-meta.json'

# Retries for a failing pull request: 1 min, 2 min, ... up to 1 h; give up
# after this many attempts until the pull request changes again.
MAX_ATTEMPTS = 8
RETRY_BASE_DELAY = 60.0
RETRY_MAX_DELAY = 60.0 * 60

SWEEP_INTERVAL = 60.0
MAINTENANCE_INTERVAL = 6 * 60.0 * 60


class ProcessingError(Exception):
  '''Raised when a pull request can't be processed (it will be retried).'''


def _sha256(data: bytes) -> str:
  return hashlib.sha256(data).hexdigest()


def _file_sha256(path: Path) -> str:
  digest = hashlib.sha256()
  with open(path, 'rb') as handle:
    for chunk in iter(lambda: handle.read(1024 * 1024), b''):
      digest.update(chunk)
  return digest.hexdigest()


def _reusable(section: Optional[SectionRecord],
              source_key: str) -> Optional[SectionRecord]:
  '''Return ``section`` if it was built from ``source_key`` and its files
  are still there.
  '''
  if section is None or section.source_key != source_key:
    return None
  if not section.root.is_dir():
    return None
  return section


def _changes_test_ids(changes_elist: Path) -> list[str]:
  with open(changes_elist, encoding = 'utf-8', errors = 'replace') as handle:
    return [line.strip() for line in handle if line.strip()]


@dataclass
class _StagedArtifact:
  '''A downloaded and extracted CI artifact, before it moves into place.'''
  directory: Path
  fingerprint: str
  tested_sha: Optional[str]


class Processor:
  '''Reconciles pull requests and announces changes (if enabled).'''

  def __init__(self,
               *,
               store: ForgeStore,
               forge: Forge,
               config: ForgeServiceConfig,
               workspaces: Workspaces,
               sandbox: Sandbox,
               publisher: Publisher,
               wake: asyncio.Event,
               after_sweep: Optional[Callable[[], Awaitable[None]]] = None,
               clock: Callable[[], datetime] = utc_now) -> None:
    self._store = store
    self._forge = forge
    self._config = config
    self._workspaces = workspaces
    self._sandbox = sandbox
    self._publisher = publisher
    self._wake = wake
    self._after_sweep = after_sweep
    self._clock = clock
    self._in_flight: dict[int, asyncio.Task[None]] = {}
    self._semaphore = asyncio.Semaphore(config.max_concurrency)

  @property
  def busy_pull_requests(self) -> frozenset[int]:
    return frozenset(self._in_flight)

  # Loop

  async def run(self) -> None:
    '''Restore what was being served, then process changes forever.'''
    await self._publisher.wait_connected()
    await self.restore()
    last_maintenance = self._clock()
    while True:
      await self._publisher.wait_connected()
      self._wake.clear()
      await self.start_due()
      if self._after_sweep is not None:
        await self._after_sweep()
      if ((self._clock() - last_maintenance).total_seconds() >=
          MAINTENANCE_INTERVAL):
        await self.maintain()
        last_maintenance = self._clock()
      try:
        await asyncio.wait_for(self._wake.wait(), SWEEP_INTERVAL)
      except asyncio.TimeoutError:
        pass

  async def start_due(self) -> None:
    '''Start processing every due pull request not already in progress.'''
    for record in await self._store.due_pull_requests(
        self._forge.name,
        self._clock()):
      if record.id in self._in_flight:
        continue
      task = asyncio.create_task(self._process_guarded(record.id))
      self._in_flight[record.id] = task

  async def drain(self) -> None:
    '''Wait until no pull request is being processed (for tests).'''
    while self._in_flight:
      await asyncio.gather(*self._in_flight.values(), return_exceptions = True)

  async def _process_guarded(self, pull_request_id: int) -> None:
    try:
      async with self._semaphore:
        record = await self._store.get_pull_request(pull_request_id)
        if record is None or not record.is_dirty:
          return
        generation = record.generation
        label = f"{record.info.repository}#{record.info.number}"
        try:
          await self.process(record)
        except PublisherUnavailableError as ex:
          # Not the pull request's fault; it stays dirty and is retried
          # once the connection is back.
          _log.warning('deferring %s: %s', label, ex)
          return
        except asyncio.CancelledError:
          raise
        except Exception as ex:
          _log.exception('processing %s failed', label)
          gave_up = await self._store.mark_failed(
            pull_request_id,
            generation,
            f"{type(ex).__name__}: {ex}",
            now = self._clock(),
            max_attempts = MAX_ATTEMPTS,
            base_delay = RETRY_BASE_DELAY,
            max_delay = RETRY_MAX_DELAY
          )
          if gave_up:
            _log.error('giving up on %s until it changes again', label)
          return
        await self._store.mark_processed(pull_request_id, generation)
        # It may have changed while it was processed.
        self._wake.set()
    finally:
      self._in_flight.pop(pull_request_id, None)

  async def restore(self) -> None:
    '''Re-serve what was served before a restart, without rebuilding.

    Pull requests whose files are gone are marked to be rebuilt instead;
    since section fingerprints are content-derived, rebuilding yields the
    same state (and so no comment).
    '''
    # Builds interrupted by the restart.
    staging_root = self._workspaces.build_staging_dir
    if staging_root.is_dir():
      for leftover in staging_root.iterdir():
        remove_tree(leftover)
    for record in await self._store.open_pull_requests(self._forge.name):
      sections = await self._store.sections(record.id)
      if len(sections) == 0:
        continue
      if all(section.root.is_dir() for section in sections):
        await self._publisher.sync(self._identity(record), sections)
      else:
        await self._store.mark_dirty(record.id)
    _log.info('restored previously served reviews')

  async def maintain(self) -> None:
    '''Periodic git housekeeping for every repository.'''
    for repository in self._config.repositories:
      try:
        await self._workspaces.collect_garbage(
          repository,
          await self._store.open_pull_request_count(
            self._forge.name,
            repository
          )
        )
      except Exception:
        _log.exception('maintenance of %s failed', repository)

  # Processing

  def _identity(self, record: PullRequestRecord) -> ReviewIdentity:
    info = record.info
    return ReviewIdentity(
      review_id = record.review_id,
      # The pull request reference links to it; the title is only text.
      name = [
        rich_link(info.url, f"{info.repository}#{info.number}"),
        rich_text(f" {info.title}"),
      ],
      timestamp = record.head_changed_at.isoformat()
    )

  async def process(self, record: PullRequestRecord) -> None:
    info = record.info
    if (info.state != PullRequestState.OPEN or
        info.base_ref not in self._config.base_branches):
      await self._tear_down(record)
      return

    current = {
      section.slug: section
      for section in await self._store.sections(record.id)
    }
    head_sha, base_tip = await self._workspaces.fetch_pull_request(info)

    desired: list[SectionRecord] = []
    changes = await self._changes_section(
      record,
      head_sha,
      base_tip,
      current.get(section_slug(CHANGES_NAME))
    )
    if changes is not None:
      desired.append(changes)
    desired += await self._ci_sections(record, head_sha, current)

    await self._publisher.sync(self._identity(record), desired)
    await self._store.replace_sections(record.id, desired)

    kept_run_dirs = {section.details.get('run_dir') for section in desired}
    for section in current.values():
      run_dir = section.details.get('run_dir')
      if run_dir is not None and run_dir not in kept_run_dirs:
        remove_tree(Path(run_dir))

  async def _tear_down(self, record: PullRequestRecord) -> None:
    '''Stop serving a pull request and free its disk space; no comment.'''
    info = record.info
    await self._publisher.remove(record.review_id)
    await self._workspaces.remove_review(info, record.review_id)
    await self._store.replace_sections(record.id, [])
    await self._workspaces.collect_garbage(
      info.repository,
      await self._store.open_pull_request_count(
        self._forge.name,
        info.repository
      )
    )
    _log.info('stopped serving %s#%d (%s)',
              info.repository, info.number, info.state.value)

  def _new_staging_dir(self, record: PullRequestRecord, label: str) -> Path:
    staging_root = ensure_real_directory(
      self._workspaces.reviews_dir.parent,
      PurePosixPath(
        self._workspaces.reviews_dir.name,
        str(record.review_id),
        'staging'
      )
    )
    return Path(tempfile.mkdtemp(prefix = f"{label}-", dir = staging_root))

  async def _changes_section(self,
                             record: PullRequestRecord,
                             head_sha: str,
                             base_tip: str,
                             current: Optional[SectionRecord]
                             ) -> Optional[SectionRecord]:
    '''Test - Changes: the pull request's expectation diffs, rematerialized
    on the merge-base by the frozen tooling (nothing from the pull request
    is executed; the diff is only data for 'patch').
    '''
    info = record.info
    merge_base = await self._workspaces.merge_base(
      info.repository,
      base_tip,
      head_sha
    )
    source_key = f"{merge_base}..{head_sha}"
    reused = _reusable(current, source_key)
    if reused is not None:
      return reused

    diff = await self._workspaces.diff(
      info.repository,
      merge_base,
      head_sha,
      ['tests/']
    )
    if diff.strip() == '':
      return None

    worktree = await self._workspaces.checkout(
      info,
      record.review_id,
      'base',
      merge_base,
      changes_patterns(diff)
    )
    runs_dir = ensure_real_directory(worktree, PurePosixPath('tests', 'runs'))
    run_dir = runs_dir / f"remat-{_sha256(source_key.encode())[:16]}"
    remove_tree(run_dir)

    # The builder sees the worktree read-only, so it writes elsewhere (on
    # the same file system, for the rename into place).
    staging_root = ensure_real_directory(
      self._workspaces.build_staging_dir.parent,
      PurePosixPath(self._workspaces.build_staging_dir.name)
    )
    staging = Path(tempfile.mkdtemp(
      prefix = f"{record.review_id}-{run_dir.name}-",
      dir = staging_root
    ))
    try:
      diff_file = staging / 'pull-request.diff'
      diff_file.write_text(diff, encoding = 'utf-8')
      timestamp = self._clock().strftime(edgutil.RUN_TIMESTAMP_FORMAT)
      result = await self._sandbox.run(
        [
          'python3',
          str(self._config.tools_dir / 'bin' / 'edg-rematerialize-test-run'),
          '--runs-dir', str(staging),
          '--run-timestamp', timestamp,
          str(diff_file),
        ],
        read_only = [worktree],
        writable = [staging],
        env = {'EDG_TEST_HOME': str(worktree / 'tests')},
        cwd = worktree,
        timeout = self._config.rematerialize_timeout
      )
      if not result.succeeded:
        reason = 'timed out' if result.timed_out else (
          f"exited with {result.returncode}"
        )
        raise ProcessingError(
          f"edg-rematerialize-test-run {reason}: "
          f"{result.output.strip()[-2000:]}"
        )
      # The builder saw only pull request content and the frozen tooling,
      # but what it wrote is read by the service and the publisher server:
      # it may hold only directories and regular files (no links to service
      # files).
      try:
        await asyncio.to_thread(
          check_plain_tree,
          staging,
          allow_hard_links = False
        )
      except WorkspaceError as ex:
        raise ProcessingError(
          f"edg-rematerialize-test-run wrote something unsafe: {ex}"
        ) from ex
      if len(edgtest.find_changes_files(staging / timestamp)) == 0:
        _log.warning(
          '%s#%d: edg-rematerialize-test-run produced no changes.elist; '
          'not serving %s', info.repository, info.number, CHANGES_NAME
        )
        remove_tree(staging)
        return None
      staging.rename(run_dir)
    except BaseException:
      remove_tree(staging)
      raise

    return SectionRecord(
      kind = KIND_TEST_CHANGES,
      name = CHANGES_NAME,
      slug = section_slug(CHANGES_NAME),
      source_key = source_key,
      content_fingerprint = _sha256(diff.encode()),
      root = run_dir / timestamp,
      home = worktree / 'tests',
      details = {'run_dir': str(run_dir)},
    )

  async def _stage_artifact(self,
                            record: PullRequestRecord,
                            artifact: ArtifactInfo,
                            label: str) -> _StagedArtifact:
    staging = self._new_staging_dir(record, label)
    try:
      zip_path = staging.with_name(f"{staging.name}.zip")
      try:
        await self._forge.download_artifact(
          record.info.repository,
          artifact,
          zip_path,
          max_bytes = self._config.max_artifact_bytes
        )
        fingerprint = artifact.digest or f"sha256:{_file_sha256(zip_path)}"
        await asyncio.to_thread(
          extract_artifact,
          zip_path,
          staging,
          max_bytes = self._config.max_extracted_bytes
        )
      finally:
        zip_path.unlink(missing_ok = True)

      tested_sha = None
      metadata_path = staging / CI_METADATA_FILE
      if metadata_path.is_file():
        metadata = json.loads(metadata_path.read_text(encoding = 'utf-8'))
        value = metadata.get('tested_sha')
        if isinstance(value, str) and len(value) in (40, 64):
          tested_sha = value
      return _StagedArtifact(staging, fingerprint, tested_sha)
    except BaseException:
      remove_tree(staging)
      raise

  async def _ci_sections(self,
                         record: PullRequestRecord,
                         head_sha: str,
                         current: dict[str, SectionRecord]
                         ) -> list[SectionRecord]:
    '''Test - Failures and Bench sections, from the latest completed CI run
    for the head commit.
    '''
    ci_slugs = [section_slug(FAILURES_NAME)] + [
      section_slug(name) for _, _, name in BENCH_SECTIONS
    ]
    previous = [
      current[slug] for slug in ci_slugs
      if slug in current and current[slug].root.is_dir()
    ]
    if not self._config.ci_tracked:
      return []
    run = await self._store.ci_run(record.id)
    if (run is None or run.head_sha != head_sha or
        run.status != CIStatus.COMPLETED):
      # Keep showing the previous results until the new run finishes.
      return previous

    run_key = f"{run.run_id}.{run.attempt}"
    failures_artifact = None
    if run.conclusion != CIConclusion.SUCCESS:
      failures_artifact = run.artifact(self._config.test_artifact)
    bench_artifact = run.artifact(self._config.bench_artifact)

    # Expired artifacts can't be downloaded again; keep what was built.
    failures = current.get(section_slug(FAILURES_NAME))
    if failures_artifact is not None and failures_artifact.expired:
      kept_failures: Optional[SectionRecord] = failures
      failures_artifact = None
    else:
      kept_failures = _reusable(
        failures,
        f"{run_key}:{failures_artifact.id}" if failures_artifact else ''
      )
    current_benches = [
      current[section_slug(name)]
      for _, _, name in BENCH_SECTIONS
      if section_slug(name) in current
    ]
    bench_key = f"{run_key}:{bench_artifact.id}" if bench_artifact else ''
    if bench_artifact is not None and bench_artifact.expired:
      kept_benches = current_benches
      bench_artifact = None
    else:
      kept_benches = [
        section for section in current_benches
        if _reusable(section, bench_key) is not None
      ]

    build_failures = failures_artifact is not None and kept_failures is None
    build_benches = bench_artifact is not None and len(kept_benches) == 0
    if not build_failures and not build_benches:
      result = [kept_failures] if kept_failures is not None else []
      return result + kept_benches

    return await self._build_ci_sections(
      record,
      head_sha,
      run,
      failures_artifact if build_failures else None,
      bench_artifact if build_benches else None,
      kept_failures,
      kept_benches
    )

  async def _build_ci_sections(self,
                               record: PullRequestRecord,
                               head_sha: str,
                               run: CIRunInfo,
                               failures_artifact: Optional[ArtifactInfo],
                               bench_artifact: Optional[ArtifactInfo],
                               kept_failures: Optional[SectionRecord],
                               kept_benches: list[SectionRecord]
                               ) -> list[SectionRecord]:
    info = record.info
    run_key = f"{run.run_id}.{run.attempt}"
    staged_failures = None
    staged_bench = None
    try:
      if failures_artifact is not None:
        staged_failures = await self._stage_artifact(
          record,
          failures_artifact,
          'failures'
        )
      if bench_artifact is not None:
        staged_bench = await self._stage_artifact(
          record,
          bench_artifact,
          'bench'
        )

      # The tests CI flagged, from the artifact's changes.elist files.
      failed_tests: list[str] = []
      changes_files = {}
      if staged_failures is not None:
        changes_files = edgtest.find_changes_files(staged_failures.directory)
        for changes_file in changes_files.values():
          failed_tests += _changes_test_ids(changes_file)
      if kept_failures is not None:
        failed_tests += kept_failures.details.get('tests', [])

      # Check out what CI tested (for pull requests, usually a merge
      # commit), falling back to the head if the forge no longer has it.
      tested_sha = head_sha
      if kept_failures is not None:
        tested_sha = kept_failures.details.get('tested_sha', head_sha)
      for staged in (staged_failures, staged_bench):
        if staged is not None and staged.tested_sha is not None:
          if await self._workspaces.fetch_commit(info, staged.tested_sha):
            tested_sha = staged.tested_sha
          else:
            _log.warning('%s#%d: CI-tested commit %s is unavailable; using '
                         'the head', info.repository, info.number,
                         staged.tested_sha[:12])
          break
      worktree = await self._workspaces.checkout(
        info,
        record.review_id,
        'head',
        tested_sha,
        failures_patterns(failed_tests)
      )

      sections: list[SectionRecord] = []
      if kept_failures is not None:
        sections.append(kept_failures)
      elif staged_failures is not None and len(changes_files) > 0:
        assert failures_artifact is not None
        runs_dir = ensure_real_directory(
          worktree,
          PurePosixPath('tests', 'runs')
        )
        run_dir = runs_dir / f"ci-{run.run_id}-{run.attempt}"
        remove_tree(run_dir)
        staged_failures.directory.rename(run_dir)
        sections.append(SectionRecord(
          kind = KIND_TEST_FAILURES,
          name = FAILURES_NAME,
          slug = section_slug(FAILURES_NAME),
          source_key = f"{run_key}:{failures_artifact.id}",
          content_fingerprint = staged_failures.fingerprint,
          root = run_dir,
          home = worktree / 'tests',
          details = {
            'run_dir': str(run_dir),
            'tests': sorted(set(failed_tests)),
            'tested_sha': tested_sha,
          },
        ))

      sections += kept_benches
      if staged_bench is not None:
        assert bench_artifact is not None
        sections += self._bench_sections(
          worktree,
          run,
          bench_artifact,
          staged_bench
        )
      return sections
    finally:
      for staged in (staged_failures, staged_bench):
        if staged is not None:
          remove_tree(staged.directory)

  def _bench_sections(self,
                      worktree: Path,
                      run: CIRunInfo,
                      artifact: ArtifactInfo,
                      staged: _StagedArtifact) -> list[SectionRecord]:
    bench_home = ensure_real_directory(worktree, PurePosixPath('benchmarks'))
    runs_dir = ensure_real_directory(bench_home, PurePosixPath('runs'))
    run_dir = runs_dir / f"ci-{run.run_id}-{run.attempt}"
    remove_tree(run_dir)
    staged.directory.rename(run_dir)

    sections = []
    for subdirectory, tag, name in BENCH_SECTIONS:
      root = run_dir / subdirectory
      try:
        # The baselines come from the pull request; never follow a link.
        baseline_dir = require_real_directory(
          bench_home,
          PurePosixPath('baselines', 'project', tag)
        )
        require_real_directory(run_dir, PurePosixPath(subdirectory))
      except WorkspaceError:
        continue
      sections.append(SectionRecord(
        kind = KIND_BENCH,
        name = name,
        slug = section_slug(name),
        source_key = f"{run.run_id}.{run.attempt}:{artifact.id}",
        content_fingerprint = f"{staged.fingerprint}/{subdirectory}",
        root = root,
        home = bench_home,
        details = {
          'run_dir': str(run_dir),
          'baseline_dir': str(baseline_dir),
          'tag': tag,
        },
      ))
    return sections
