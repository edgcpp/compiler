# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Publishing review sections to AcknowlEDG (``publish``), over one batch
session, from a queue of what to publish, and what to stop publishing
(``Wanted``), applied in order; the requests of reviewers go to the
process pool (``pool``), which answers them with the stock handlers. The
client library keeps the session connected, and serves what was published
while not connected once it is.
'''

import asyncio
import logging
import uuid

from dataclasses import dataclass
from pathlib import Path
from typing import AbstractSet, Optional, Protocol, Sequence, TypeAlias

from edgacknowledg.client import (
  DEFAULT_OPEN_TIMEOUT_S,
  BatchClient,
  BatchClientSession,
  ReviewClient,
)
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.publisher import ReviewIdentity
from edgacknowledg.forge.publisher.pool import ProcessPool, UnixSocketHandler
from edgacknowledg.forge.workspace import (
  WorkspaceError,
  require_real_directory_under,
)

_log = logging.getLogger(__name__)


class WantedItem(Protocol):
  '''Something wanted of what is published (``Sync``, ``Delist``,
  ``Prune``).
  '''

  async def apply_to(self, published: '_Published') -> None:
    '''Make ``published`` what is wanted.'''
    ...


@dataclass(frozen = True)
class Sync:
  '''Serve exactly ``sections`` for ``review`` (none: stop serving it).'''
  review: ReviewIdentity
  sections: list[SectionRecord]

  async def apply_to(self, published: '_Published') -> None:
    await published.sync(self.review, self.sections)


@dataclass(frozen = True)
class Delist:
  '''Stop serving a review.'''
  review_id: uuid.UUID

  async def apply_to(self, published: '_Published') -> None:
    await published.remove(self.review_id)


@dataclass(frozen = True)
class Prune:
  '''Stop serving every review but those in ``keep``.'''
  keep: frozenset[uuid.UUID]

  async def apply_to(self, published: '_Published') -> None:
    await published.prune(self.keep)


# What to publish, and to stop publishing, in order, for ``publish`` to
# apply.
Wanted: TypeAlias = 'asyncio.Queue[WantedItem]'


def _check_directories(section: SectionRecord, reviews_dir: Path) -> None:
  '''Require the section's directories to be real ones (no symlink
  anywhere below the reviews directory), the root and baselines inside the
  home; the handlers rely on that.
  '''
  directories = [section.home, section.root]
  if 'baseline_dir' in section.details:
    directories.append(Path(section.details['baseline_dir']))
  for directory in directories:
    require_real_directory_under(reviews_dir, directory)
  for directory in directories[1:]:
    require_real_directory_under(section.home, directory)


def _build_handler(section: SectionRecord,
                   pool: ProcessPool,
                   reviews_dir: Optional[Path]) -> UnixSocketHandler:
  if reviews_dir is not None:
    _check_directories(section, reviews_dir)
  return UnixSocketHandler(section, pool)


@dataclass
class _ServedSection:
  record: SectionRecord
  handler: UnixSocketHandler


class _Published:
  '''What ``session`` publishes, read-only, answering reviewers in
  ``pool``.

  The handlers there treat the trees as untrusted: they follow no
  symlinks, and with ``reviews_dir`` every section's directories must be
  real directories under it.
  '''

  def __init__(self,
               session: BatchClientSession,
               pool: ProcessPool,
               reviews_dir: Optional[Path]) -> None:
    self._session = session
    self._pool = pool
    self._reviews_dir = reviews_dir
    self._reviews: dict[uuid.UUID, ReviewClient] = {}
    self._served: dict[uuid.UUID, dict[str, _ServedSection]] = {}

  async def sync(self,
                 review: ReviewIdentity,
                 sections: Sequence[SectionRecord]) -> None:
    '''Serve exactly ``sections`` for ``review`` (none: stop serving it).'''
    if len(sections) == 0:
      await self.remove(review.review_id)
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
      try:
        handler = await asyncio.to_thread(
          _build_handler,
          section,
          self._pool,
          self._reviews_dir
        )
      except WorkspaceError as ex:
        # The service only sends real directories; this guards the
        # handlers should it ever not.
        _log.error('not serving section %s of review %s: %s', slug,
                   review.review_id, ex)
        continue
      await client_review.add_section(handler)
      served[slug] = _ServedSection(section, handler)

  async def prune(self, keep: AbstractSet[uuid.UUID]) -> None:
    '''Stop serving every review but those in ``keep``.'''
    for review_id in list(self._reviews):
      if review_id not in keep:
        await self.remove(review_id)

  async def remove(self, review_id: uuid.UUID) -> None:
    '''Stop serving a review.'''
    client_review = self._reviews.pop(review_id, None)
    self._served.pop(review_id, None)
    if client_review is not None:
      await self._session.remove_review(client_review)


class _Logging:
  '''Logs the batch session's connection.'''

  def connected(self) -> None:
    _log.info('connected to AcknowlEDG')

  def attempting_reconnect(self) -> None:
    pass

  def disconnected(self) -> None:
    _log.warning('lost the AcknowlEDG connection; reconnecting')


async def publish(wanted: Wanted,
                  *,
                  api_host: str,
                  api_port: int,
                  api_key: Optional[str],
                  pool: ProcessPool,
                  reviews_dir: Optional[Path] = None) -> None:
  '''Publish what ``wanted`` asks for, until cancelled.'''
  async with BatchClient(
      api_host = api_host,
      api_port = api_port,
      api_key = api_key,
      open_timeout = DEFAULT_OPEN_TIMEOUT_S,
      status_listener = _Logging()) as session:
    published = _Published(session, pool, reviews_dir)
    while True:
      item = await wanted.get()
      try:
        await item.apply_to(published)
      except Exception:
        # One review mustn't stop the rest.
        _log.exception('publishing failed; it is published again once it '
                       'changes')
