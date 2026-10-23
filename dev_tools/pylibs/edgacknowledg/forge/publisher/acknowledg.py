# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''``AcknowledgPublisher``: serves review sections over one AcknowlEDG batch
connection. The publisher server runs one per connection; the requests of
reviewers go to its process pool (``pool``), which answers them with the
stock handlers.
'''

import asyncio
import logging
import uuid

from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Sequence

from edgacknowledg.client import (
  DEFAULT_OPEN_TIMEOUT_S,
  BatchClientSession,
  ClientError,
  NotConnectedError,
  ReviewClient,
)
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.publisher import (
  PublisherUnavailableError,
  ReviewIdentity,
  section_url,
)
from edgacknowledg.forge.publisher.pool import ProcessPool, UnixSocketHandler
from edgacknowledg.forge.workspace import require_real_directory_under

_log = logging.getLogger(__name__)


@dataclass
class _ServedSection:
  record: SectionRecord
  handler: UnixSocketHandler


class AcknowledgPublisher:
  '''Serves sections, read-only, answering reviewers in ``pool``.

  The handlers there treat the trees as untrusted: they follow no
  symlinks, and with ``reviews_dir`` every section's directories must be
  real directories under it.
  '''

  def __init__(self,
               *,
               api_host: str,
               api_port: int,
               api_key: Optional[str],
               ui_host: str,
               ui_port: int,
               pool: ProcessPool,
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
    self._pool = pool
    self._reviews_dir = reviews_dir
    self._connected = asyncio.Event()
    self._lock = asyncio.Lock()
    self._reviews: dict[uuid.UUID, ReviewClient] = {}
    self._served: dict[uuid.UUID, dict[str, _ServedSection]] = {}

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
    return section_url(self._ui_host, self._ui_port, review_id, section)

  def _check_directories(self, section: SectionRecord) -> None:
    '''Require the section's directories to be real ones (no symlink
    anywhere below the reviews directory), the root and baselines inside
    the home; the handlers rely on that.
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

  def _build_handler(self, section: SectionRecord) -> UnixSocketHandler:
    self._check_directories(section)
    return UnixSocketHandler(section, self._pool)

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
