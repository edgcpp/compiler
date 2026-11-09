# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Ingestion: the one path from forge state into the database.

Polling and webhooks both end up here, so however the service learns about
a pull request, the result is the same: the database is updated and, if
anything material changed, the processor is woken.
'''

import asyncio
import logging

from datetime import datetime, timezone
from typing import Callable

from edgacknowledg.forge import (
  Forge,
  PullRequestInfo,
  PullRequestState,
)
from edgacknowledg.forge.config import ForgeServiceConfig
from edgacknowledg.forge.db import ForgeStore, PullRequestRecord

_log = logging.getLogger(__name__)


def utc_now() -> datetime:
  return datetime.now(timezone.utc)


class Ingestor:
  '''Upserts forge state and wakes the processor on material changes.'''

  def __init__(self,
               store: ForgeStore,
               forge: Forge,
               config: ForgeServiceConfig,
               *,
               wake: asyncio.Event,
               clock: Callable[[], datetime] = utc_now) -> None:
    self._store = store
    self._forge = forge
    self._config = config
    self._wake = wake
    self._clock = clock

  async def ingest_pull_request(self, info: PullRequestInfo) -> None:
    changed = await self._store.upsert_pull_request(
      self._forge.name,
      info,
      now = self._clock(),
      settle_timeout = self._config.settle_timeout
    )
    if changed:
      _log.info(
        'pull request %s#%d changed (%s, head %s)',
        info.repository, info.number, info.state.value, info.head_sha[:12]
      )
      self._wake.set()
    if info.state == PullRequestState.OPEN:
      record = await self._store.find_pull_request(
        self._forge.name,
        info.repository,
        info.number
      )
      assert record is not None
      await self._refresh_ci(record)

  async def _refresh_ci(self, record: PullRequestRecord) -> None:
    if not self._config.ci_tracked:
      return
    run = await self._forge.latest_ci_run(record.info)
    if run is None:
      return
    if await self._store.upsert_ci_run(record.id, run):
      _log.info(
        'CI for %s#%d changed (run %s attempt %d: %s)',
        record.info.repository, record.info.number,
        run.run_id, run.attempt, run.status.value
      )
      self._wake.set()

  def _is_watched(self, repository: str) -> bool:
    if repository in self._config.repositories:
      return True
    # E.g. an organization-wide webhook delivering for other repositories.
    _log.debug('ignoring %s, which is not a configured repository',
               repository)
    return False

  async def refresh_pull_request(self, repository: str, number: int) -> None:
    '''Re-query one pull request (and its CI) from the forge.'''
    if not self._is_watched(repository):
      return
    info = await self._forge.get_pull_request(repository, number)
    await self.ingest_pull_request(info)

  async def refresh_ci_for_head(self, repository: str, head_sha: str) -> None:
    '''Re-query CI for the open pull requests whose head is ``head_sha``.'''
    if not self._is_watched(repository):
      return
    for record in await self._store.open_pull_requests_with_head(
        self._forge.name,
        repository,
        head_sha):
      await self._refresh_ci(record)

  async def sync_repository(self, repository: str) -> None:
    '''Bring every pull request in ``repository`` up to date.

    Open pull requests come from one listing; pull requests the database
    still thinks are open but the listing lacks were closed or merged in
    the meantime, so they're queried individually.
    '''
    open_infos = await self._forge.list_open_pull_requests(repository)
    open_numbers = {info.number for info in open_infos}
    for info in open_infos:
      await self.ingest_pull_request(info)
    for record in await self._store.open_pull_requests(
        self._forge.name,
        repository):
      if record.info.number not in open_numbers:
        await self.refresh_pull_request(repository, record.info.number)
