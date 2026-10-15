# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Publishing review sections to AcknowlEDG over one batch connection.'''

import asyncio
import logging
import uuid

from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Protocol, Sequence

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
from edgacknowledg.forge.sandbox import Sandbox

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
  '''Serves sections with the stock AcknowlEDG handlers, read-only.'''

  def __init__(self,
               *,
               api_host: str,
               api_port: int,
               api_key: Optional[str],
               ui_host: str,
               ui_port: int,
               sandbox: Sandbox) -> None:
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

  def _build_handler(self, section: SectionRecord) -> ReviewClientHandler:
    # Commands run on reviewers' behalf (ifc-dump, cg_annotate) only see the
    # section's tree, read-only.
    command_prefix = self._sandbox.prefix(read_only = [section.home])
    if section.kind in (KIND_TEST_CHANGES, KIND_TEST_FAILURES):
      return TestSourceHandler(
        section.name,
        section.slug,
        section.home,
        section.root,
        command_prefix = command_prefix
      )
    if section.kind == KIND_BENCH:
      session = BenchReviewSession(
        bench_home = section.home,
        run_dir = section.root,
        baseline_dir = Path(section.details['baseline_dir']),
        tag = section.details['tag'],
        command_prefix = command_prefix
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
