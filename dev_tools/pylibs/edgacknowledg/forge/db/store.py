# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Database access for acknowledg-forge.

Every query lives here. Callers get immutable records rather than ORM
objects, so nothing lazy-loads outside a session and the rest of the service
doesn't depend on SQLAlchemy.
'''

import uuid

from dataclasses import dataclass, field
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any, Optional, Sequence

from sqlalchemy import delete, event, func, select
from sqlalchemy.engine import Connection, make_url
from sqlalchemy.exc import IntegrityError
from sqlalchemy.ext.asyncio import (
  AsyncEngine,
  AsyncSession,
  async_sessionmaker,
  create_async_engine,
)
from sqlalchemy.pool import StaticPool

from edgacknowledg.forge import (
  ArtifactInfo,
  CIConclusion,
  CIRunInfo,
  CIStatus,
  PullRequestInfo,
  PullRequestState,
)
from edgacknowledg.forge.db.models import (
  Announcement,
  CIRun,
  PullRequest,
  Section,
)

_MIGRATIONS_DIR = Path(__file__).parent / 'migrations'


@dataclass(frozen = True)
class PullRequestRecord:
  id: int
  forge: str
  review_id: uuid.UUID
  info: PullRequestInfo
  head_changed_at: datetime
  settle_deadline: datetime
  generation: int
  processed_generation: int
  error_count: int
  next_attempt_at: Optional[datetime]
  last_error: Optional[str]

  @property
  def is_dirty(self) -> bool:
    return self.generation > self.processed_generation


@dataclass(frozen = True)
class SectionRecord:
  kind: str
  name: str
  slug: str
  source_key: str
  content_fingerprint: str
  root: Path
  home: Path
  details: dict[str, Any] = field(default_factory = dict)


@dataclass(frozen = True)
class AnnouncementRecord:
  fingerprint: Optional[str]
  sections: dict[str, str]
  pending_fingerprint: Optional[str]
  last_comment_id: Optional[str]
  last_posted_at: Optional[datetime]


def _pull_request_record(row: PullRequest) -> PullRequestRecord:
  return PullRequestRecord(
    id = row.id,
    forge = row.forge,
    review_id = row.review_id,
    info = PullRequestInfo(
      repository = row.repository,
      number = row.number,
      state = PullRequestState(row.state),
      title = row.title,
      author = row.author,
      url = row.url,
      base_ref = row.base_ref,
      base_sha = row.base_sha,
      head_sha = row.head_sha,
      fetch_ref = row.fetch_ref,
      updated_at = row.forge_updated_at,
    ),
    head_changed_at = row.head_changed_at,
    settle_deadline = row.settle_deadline,
    generation = row.generation,
    processed_generation = row.processed_generation,
    error_count = row.error_count,
    next_attempt_at = row.next_attempt_at,
    last_error = row.last_error,
  )


def _artifacts_to_json(artifacts: Sequence[ArtifactInfo]
                       ) -> list[dict[str, Any]]:
  return [
    {
      'name': artifact.name,
      'id': artifact.id,
      'size': artifact.size,
      'digest': artifact.digest,
      'expired': artifact.expired,
    }
    for artifact in artifacts
  ]


def _ci_run_info(row: CIRun) -> CIRunInfo:
  return CIRunInfo(
    run_id = row.run_id,
    attempt = row.attempt,
    head_sha = row.head_sha,
    status = CIStatus(row.status),
    conclusion = (
      CIConclusion(row.conclusion) if row.conclusion is not None else None
    ),
    completed_at = row.completed_at,
    url = row.url,
    artifacts = tuple(
      ArtifactInfo(
        name = item['name'],
        id = item['id'],
        size = item['size'],
        digest = item['digest'],
        expired = item['expired'],
      )
      for item in row.artifacts
    ),
  )


def _section_record(row: Section) -> SectionRecord:
  return SectionRecord(
    kind = row.kind,
    name = row.name,
    slug = row.slug,
    source_key = row.source_key,
    content_fingerprint = row.content_fingerprint,
    root = Path(row.root),
    home = Path(row.home),
    details = dict(row.details),
  )


def _bump_generation(row: PullRequest) -> None:
  '''Mark ``row`` as needing processing, retrying immediately.'''
  row.generation += 1
  row.error_count = 0
  row.next_attempt_at = None
  row.last_error = None


def _run_migrations(connection: Connection) -> None:
  # Alembic is only needed at startup; keep it out of module import.
  from alembic import command
  from alembic.config import Config

  config = Config()
  config.set_main_option('script_location', str(_MIGRATIONS_DIR))
  config.attributes['connection'] = connection
  command.upgrade(config, 'head')


class ForgeStore:
  '''Async access to the acknowledg-forge database.'''

  def __init__(self, engine: AsyncEngine) -> None:
    self._engine = engine
    self._sessions = async_sessionmaker(engine, expire_on_commit = False)

  @classmethod
  async def open(cls, url: str) -> 'ForgeStore':
    '''Connect to ``url`` and bring its schema up to date.'''
    parsed_url = make_url(url)
    is_sqlite = parsed_url.get_backend_name() == 'sqlite'
    engine_kwargs: dict[str, Any] = {}
    if is_sqlite and parsed_url.database in (None, '', ':memory:'):
      # An in-memory database exists per connection; share one.
      engine_kwargs['poolclass'] = StaticPool
    engine = create_async_engine(url, **engine_kwargs)

    if is_sqlite:
      def configure_sqlite(dbapi_connection: Any, _record: Any) -> None:
        cursor = dbapi_connection.cursor()
        cursor.execute('PRAGMA foreign_keys = ON')
        cursor.execute('PRAGMA journal_mode = WAL')
        cursor.execute('PRAGMA busy_timeout = 10000')
        cursor.close()

      event.listen(engine.sync_engine, 'connect', configure_sqlite)

    async with engine.begin() as connection:
      await connection.run_sync(_run_migrations)
    return cls(engine)

  async def aclose(self) -> None:
    await self._engine.dispose()

  def _session(self) -> AsyncSession:
    return self._sessions()

  async def _find_row(self,
                      session: AsyncSession,
                      forge: str,
                      repository: str,
                      number: int) -> Optional[PullRequest]:
    result = await session.execute(
      select(PullRequest)
        .where(PullRequest.forge == forge)
        .where(PullRequest.repository == repository)
        .where(PullRequest.number == number)
        .with_for_update()
    )
    return result.scalar_one_or_none()

  async def upsert_pull_request(self,
                                forge: str,
                                info: PullRequestInfo,
                                *,
                                now: datetime,
                                settle_timeout: float) -> bool:
    '''Record the forge's view of a pull request.

    Returns whether anything material changed (and the pull request now
    needs processing). A moving base branch alone isn't material: the
    merge-base is recomputed whenever the pull request is processed anyway,
    and treating it as material would reprocess every pull request on every
    push to the base branch.
    '''
    try:
      return await self._upsert_pull_request(forge, info, now, settle_timeout)
    except IntegrityError:
      # Another task inserted the same pull request first; update it.
      return await self._upsert_pull_request(forge, info, now, settle_timeout)

  async def _upsert_pull_request(self,
                                 forge: str,
                                 info: PullRequestInfo,
                                 now: datetime,
                                 settle_timeout: float) -> bool:
    settle_deadline = now + timedelta(seconds = settle_timeout)
    async with self._session() as session, session.begin():
      row = await self._find_row(session, forge, info.repository, info.number)
      if row is None:
        session.add(PullRequest(
          forge = forge,
          repository = info.repository,
          number = info.number,
          review_id = uuid.uuid4(),
          state = info.state.value,
          title = info.title,
          author = info.author,
          url = info.url,
          base_ref = info.base_ref,
          base_sha = info.base_sha,
          head_sha = info.head_sha,
          fetch_ref = info.fetch_ref,
          forge_updated_at = info.updated_at,
          head_changed_at = now,
          settle_deadline = settle_deadline,
          generation = 1,
          processed_generation = 0,
          error_count = 0,
        ))
        return True

      material = (
        row.state != info.state.value or
        row.head_sha != info.head_sha or
        row.base_ref != info.base_ref or
        row.fetch_ref != info.fetch_ref
      )
      if row.head_sha != info.head_sha:
        row.head_changed_at = now
        row.settle_deadline = settle_deadline
      row.state = info.state.value
      row.title = info.title
      row.author = info.author
      row.url = info.url
      row.base_ref = info.base_ref
      row.base_sha = info.base_sha
      row.head_sha = info.head_sha
      row.fetch_ref = info.fetch_ref
      row.forge_updated_at = info.updated_at
      if material:
        _bump_generation(row)
      return material

  async def upsert_ci_run(self,
                          pull_request_id: int,
                          run: CIRunInfo) -> bool:
    '''Record the latest CI run attempt; return whether it changed.'''
    artifacts = _artifacts_to_json(run.artifacts)
    conclusion = run.conclusion.value if run.conclusion is not None else None
    async with self._session() as session, session.begin():
      row = await session.get(CIRun, pull_request_id)
      if row is not None and (
          row.run_id == run.run_id and
          row.attempt == run.attempt and
          row.head_sha == run.head_sha and
          row.status == run.status.value and
          row.conclusion == conclusion and
          row.artifacts == artifacts):
        return False

      if row is None:
        row = CIRun(pull_request_id = pull_request_id)
        session.add(row)
      row.run_id = run.run_id
      row.attempt = run.attempt
      row.head_sha = run.head_sha
      row.status = run.status.value
      row.conclusion = conclusion
      row.completed_at = run.completed_at
      row.url = run.url
      row.artifacts = artifacts

      pull_request = await session.get(PullRequest, pull_request_id)
      assert pull_request is not None
      _bump_generation(pull_request)
      return True

  async def get_pull_request(self,
                             pull_request_id: int
                             ) -> Optional[PullRequestRecord]:
    async with self._session() as session:
      row = await session.get(PullRequest, pull_request_id)
      return _pull_request_record(row) if row is not None else None

  async def find_pull_request(self,
                              forge: str,
                              repository: str,
                              number: int) -> Optional[PullRequestRecord]:
    async with self._session() as session:
      result = await session.execute(
        select(PullRequest)
          .where(PullRequest.forge == forge)
          .where(PullRequest.repository == repository)
          .where(PullRequest.number == number)
      )
      row = result.scalar_one_or_none()
      return _pull_request_record(row) if row is not None else None

  async def open_pull_requests(self,
                               forge: str,
                               repository: Optional[str] = None
                               ) -> list[PullRequestRecord]:
    statement = (
      select(PullRequest)
        .where(PullRequest.forge == forge)
        .where(PullRequest.state == PullRequestState.OPEN.value)
        .order_by(PullRequest.id)
    )
    if repository is not None:
      statement = statement.where(PullRequest.repository == repository)
    async with self._session() as session:
      result = await session.execute(statement)
      return [_pull_request_record(row) for row in result.scalars()]

  async def open_pull_requests_with_head(self,
                                         forge: str,
                                         repository: str,
                                         head_sha: str
                                         ) -> list[PullRequestRecord]:
    async with self._session() as session:
      result = await session.execute(
        select(PullRequest)
          .where(PullRequest.forge == forge)
          .where(PullRequest.repository == repository)
          .where(PullRequest.head_sha == head_sha)
          .where(PullRequest.state == PullRequestState.OPEN.value)
      )
      return [_pull_request_record(row) for row in result.scalars()]

  async def open_pull_request_count(self, forge: str, repository: str) -> int:
    async with self._session() as session:
      result = await session.execute(
        select(func.count())
          .select_from(PullRequest)
          .where(PullRequest.forge == forge)
          .where(PullRequest.repository == repository)
          .where(PullRequest.state == PullRequestState.OPEN.value)
      )
      return int(result.scalar_one())

  async def due_pull_requests(self,
                              forge: str,
                              now: datetime) -> list[PullRequestRecord]:
    '''Pull requests needing processing whose retry delay (if any) is over.'''
    async with self._session() as session:
      result = await session.execute(
        select(PullRequest)
          .where(PullRequest.forge == forge)
          .where(PullRequest.generation > PullRequest.processed_generation)
          .where(
            PullRequest.next_attempt_at.is_(None) |
            (PullRequest.next_attempt_at <= now)
          )
          .order_by(PullRequest.id)
      )
      return [_pull_request_record(row) for row in result.scalars()]

  async def ci_run(self, pull_request_id: int) -> Optional[CIRunInfo]:
    async with self._session() as session:
      row = await session.get(CIRun, pull_request_id)
      return _ci_run_info(row) if row is not None else None

  async def mark_dirty(self, pull_request_id: int) -> None:
    async with self._session() as session, session.begin():
      row = await session.get(PullRequest, pull_request_id)
      if row is not None:
        _bump_generation(row)

  async def mark_processed(self,
                           pull_request_id: int,
                           generation: int) -> None:
    '''Record that processing as of ``generation`` succeeded.

    If ingestion bumped the generation meanwhile, the pull request stays
    dirty and is processed again.
    '''
    async with self._session() as session, session.begin():
      row = await session.get(PullRequest, pull_request_id)
      if row is None:
        return
      row.processed_generation = max(row.processed_generation, generation)
      row.error_count = 0
      row.next_attempt_at = None
      row.last_error = None

  async def mark_failed(self,
                        pull_request_id: int,
                        generation: int,
                        error: str,
                        *,
                        now: datetime,
                        max_attempts: int,
                        base_delay: float,
                        max_delay: float) -> bool:
    '''Record a processing failure and schedule a retry with exponential
    backoff. After ``max_attempts`` the service gives up until something
    about the pull request changes. Returns whether it gave up.
    '''
    async with self._session() as session, session.begin():
      row = await session.get(PullRequest, pull_request_id)
      if row is None:
        return True
      row.last_error = error
      if row.generation != generation:
        # Changed while processing; retry right away with the new state.
        return False
      row.error_count += 1
      if row.error_count >= max_attempts:
        row.processed_generation = generation
        row.next_attempt_at = None
        return True
      delay = min(max_delay, base_delay * 2 ** (row.error_count - 1))
      row.next_attempt_at = now + timedelta(seconds = delay)
      return False

  async def sections(self, pull_request_id: int) -> list[SectionRecord]:
    async with self._session() as session:
      result = await session.execute(
        select(Section)
          .where(Section.pull_request_id == pull_request_id)
          .order_by(Section.slug)
      )
      return [_section_record(row) for row in result.scalars()]

  async def replace_sections(self,
                             pull_request_id: int,
                             sections: Sequence[SectionRecord]) -> None:
    async with self._session() as session, session.begin():
      await session.execute(
        delete(Section).where(Section.pull_request_id == pull_request_id)
      )
      for section in sections:
        session.add(Section(
          pull_request_id = pull_request_id,
          kind = section.kind,
          name = section.name,
          slug = section.slug,
          source_key = section.source_key,
          content_fingerprint = section.content_fingerprint,
          root = str(section.root),
          home = str(section.home),
          details = dict(section.details),
        ))

  async def announcement(self,
                         pull_request_id: int) -> Optional[AnnouncementRecord]:
    async with self._session() as session:
      row = await session.get(Announcement, pull_request_id)
      if row is None:
        return None
      return AnnouncementRecord(
        fingerprint = row.fingerprint,
        sections = dict(row.sections),
        pending_fingerprint = row.pending_fingerprint,
        last_comment_id = row.last_comment_id,
        last_posted_at = row.last_posted_at,
      )

  async def has_announcements(self, forge: str) -> bool:
    async with self._session() as session:
      result = await session.execute(
        select(func.count())
          .select_from(Announcement)
          .join(PullRequest, PullRequest.id == Announcement.pull_request_id)
          .where(PullRequest.forge == forge)
      )
      return int(result.scalar_one()) > 0

  async def set_pending_announcement(self,
                                     pull_request_id: int,
                                     fingerprint: str) -> None:
    async with self._session() as session, session.begin():
      row = await session.get(Announcement, pull_request_id)
      if row is None:
        row = Announcement(pull_request_id = pull_request_id, sections = {})
        session.add(row)
      row.pending_fingerprint = fingerprint

  async def record_announcement(self,
                                pull_request_id: int,
                                fingerprint: str,
                                sections: dict[str, str],
                                *,
                                comment_id: Optional[str],
                                posted_at: Optional[datetime]) -> None:
    '''Record ``fingerprint`` as announced. With no ``comment_id`` the state
    is adopted silently (e.g. on a fresh database).
    '''
    async with self._session() as session, session.begin():
      row = await session.get(Announcement, pull_request_id)
      if row is None:
        row = Announcement(pull_request_id = pull_request_id)
        session.add(row)
      row.fingerprint = fingerprint
      row.sections = dict(sections)
      row.pending_fingerprint = None
      if comment_id is not None:
        row.last_comment_id = comment_id
        row.last_posted_at = posted_at
