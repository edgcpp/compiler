# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''SQLAlchemy models for acknowledg-forge state.

Only portable column types are used so the same models work on SQLite and
PostgreSQL. Schema changes go through Alembic (``migrations/``).
'''

import uuid

from datetime import datetime, timezone
from typing import Any, Optional

from sqlalchemy import (
  JSON,
  DateTime,
  ForeignKey,
  Integer,
  String,
  Text,
  TypeDecorator,
  UniqueConstraint,
  Uuid,
)
from sqlalchemy.engine import Dialect
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class UTCDateTime(TypeDecorator[datetime]):
  '''A timezone-aware UTC datetime on every backend.

  SQLite has no timezone support, so values are stored as naive UTC and
  made aware again when loaded.
  '''
  impl = DateTime
  cache_ok = True

  def process_bind_param(self,
                         value: Optional[datetime],
                         dialect: Dialect) -> Optional[datetime]:
    if value is None:
      return None
    if value.tzinfo is None:
      raise ValueError('naive datetimes are not accepted')
    return value.astimezone(timezone.utc).replace(tzinfo = None)

  def process_result_value(self,
                           value: Optional[datetime],
                           dialect: Dialect) -> Optional[datetime]:
    if value is None:
      return None
    return value.replace(tzinfo = timezone.utc)


class Base(DeclarativeBase):
  pass


class PullRequest(Base):
  '''A pull request the service knows about, open or not.'''
  __tablename__ = 'pull_requests'
  __table_args__ = (UniqueConstraint('forge', 'repository', 'number'),)

  id: Mapped[int] = mapped_column(Integer, primary_key = True)
  forge: Mapped[str] = mapped_column(String(32))
  repository: Mapped[str] = mapped_column(String(255))
  number: Mapped[int] = mapped_column(Integer)
  # Assigned once; reused if the pull request is reopened.
  review_id: Mapped[uuid.UUID] = mapped_column(Uuid, unique = True)
  state: Mapped[str] = mapped_column(String(16))
  title: Mapped[str] = mapped_column(Text)
  author: Mapped[str] = mapped_column(String(255))
  url: Mapped[str] = mapped_column(Text)
  base_ref: Mapped[str] = mapped_column(String(255))
  base_sha: Mapped[str] = mapped_column(String(64))
  head_sha: Mapped[str] = mapped_column(String(64))
  fetch_ref: Mapped[str] = mapped_column(String(255))
  forge_updated_at: Mapped[datetime] = mapped_column(UTCDateTime)
  head_changed_at: Mapped[datetime] = mapped_column(UTCDateTime)
  # When the pull request counts as settled even without a finished CI run.
  settle_deadline: Mapped[datetime] = mapped_column(UTCDateTime)
  # Bumped by ingestion on every material change; the pull request needs
  # processing while generation > processed_generation.
  generation: Mapped[int] = mapped_column(Integer, default = 1)
  processed_generation: Mapped[int] = mapped_column(Integer, default = 0)
  error_count: Mapped[int] = mapped_column(Integer, default = 0)
  next_attempt_at: Mapped[Optional[datetime]] = mapped_column(
    UTCDateTime,
    nullable = True
  )
  last_error: Mapped[Optional[str]] = mapped_column(Text, nullable = True)


class CIRun(Base):
  '''The latest tracked CI run attempt for a pull request's head.'''
  __tablename__ = 'ci_runs'

  pull_request_id: Mapped[int] = mapped_column(
    ForeignKey('pull_requests.id', ondelete = 'CASCADE'),
    primary_key = True
  )
  run_id: Mapped[str] = mapped_column(String(64))
  attempt: Mapped[int] = mapped_column(Integer)
  head_sha: Mapped[str] = mapped_column(String(64))
  status: Mapped[str] = mapped_column(String(16))
  conclusion: Mapped[Optional[str]] = mapped_column(
    String(16),
    nullable = True
  )
  completed_at: Mapped[Optional[datetime]] = mapped_column(
    UTCDateTime,
    nullable = True
  )
  url: Mapped[str] = mapped_column(Text)
  # [{name, id, size, digest, expired}, ...]
  artifacts: Mapped[list[dict[str, Any]]] = mapped_column(JSON)


class Section(Base):
  '''A review section currently being served for a pull request.'''
  __tablename__ = 'sections'
  __table_args__ = (UniqueConstraint('pull_request_id', 'slug'),)

  id: Mapped[int] = mapped_column(Integer, primary_key = True)
  pull_request_id: Mapped[int] = mapped_column(
    ForeignKey('pull_requests.id', ondelete = 'CASCADE')
  )
  kind: Mapped[str] = mapped_column(String(32))
  name: Mapped[str] = mapped_column(String(255))
  slug: Mapped[str] = mapped_column(String(255))
  # Identifies the inputs the section was built from; a section is rebuilt
  # when its desired source key changes.
  source_key: Mapped[str] = mapped_column(Text)
  # Identifies what the section shows; drives comment decisions.
  content_fingerprint: Mapped[str] = mapped_column(String(128))
  # Directory the section serves, and the test or bench home it is under.
  root: Mapped[str] = mapped_column(Text)
  home: Mapped[str] = mapped_column(Text)
  # Section-specific extras (e.g. the bench baseline directory and tag).
  details: Mapped[dict[str, Any]] = mapped_column(JSON, default = dict)


class Announcement(Base):
  '''What the service last announced in a pull request comment.'''
  __tablename__ = 'announcements'

  pull_request_id: Mapped[int] = mapped_column(
    ForeignKey('pull_requests.id', ondelete = 'CASCADE'),
    primary_key = True
  )
  # Served-state fingerprint and {section name: content fingerprint} as of
  # the last comment (or adoption, which sets them without commenting).
  fingerprint: Mapped[Optional[str]] = mapped_column(
    String(128),
    nullable = True
  )
  sections: Mapped[dict[str, str]] = mapped_column(JSON, default = dict)
  # Set before posting so a crash mid-post can be reconciled on retry.
  pending_fingerprint: Mapped[Optional[str]] = mapped_column(
    String(128),
    nullable = True
  )
  last_comment_id: Mapped[Optional[str]] = mapped_column(
    String(64),
    nullable = True
  )
  last_posted_at: Mapped[Optional[datetime]] = mapped_column(
    UTCDateTime,
    nullable = True
  )
