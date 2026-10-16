# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Announcing (optional): one comment per meaningful change in what's served.

A pull request is commented on only when it has *settled* (CI for its head
has finished, the settle timeout passed, or CI isn't tracked), so the
sections one push produces are announced together. Comments are driven by
a fingerprint of the served sections' content, so restarts, rebuilds,
closing, and reopening with the same results post nothing.

Posting is idempotent: the fingerprint is recorded as pending first, and
each comment carries a hidden marker, so after a crash between posting and
recording, the existing comment is found instead of posting a duplicate.
'''

import hashlib
import json
import logging

from datetime import datetime
from typing import Callable, Optional, Sequence

from edgacknowledg.forge import (
  CIRunInfo,
  CIStatus,
  ForgeCommenter,
)
from edgacknowledg.forge.config import ForgeServiceConfig
from edgacknowledg.forge.db import ForgeStore, PullRequestRecord, SectionRecord
from edgacknowledg.forge.ingest import utc_now
from edgacknowledg.forge.publisher import Publisher

_log = logging.getLogger(__name__)


def served_fingerprint(sections: Sequence[SectionRecord]) -> str:
  '''Identify what a set of sections shows (names and content).'''
  pairs = sorted(
    (section.name, section.content_fingerprint) for section in sections
  )
  return hashlib.sha256(json.dumps(pairs).encode()).hexdigest()


def comment_marker(review_id: object, fingerprint: str) -> str:
  return f"<!-- acknowledg-forge review={review_id} fp={fingerprint} -->"


def render_comment(record: PullRequestRecord,
                   sections: Sequence[SectionRecord],
                   previous: dict[str, str],
                   ci_run: Optional[CIRunInfo],
                   marker: str,
                   section_url: Callable[[SectionRecord], Optional[str]]
                   ) -> str:
  '''Render an announcement. Only fixed text, commit hashes, and
  service-generated URLs go in; nothing a pull request author controls.
  '''
  lines = [
    marker,
    f"**AcknowlEDG review results updated** for "
    f"`{record.info.head_sha[:12]}`:",
    '',
  ]
  current_names = set()
  for section in sorted(sections, key = lambda s: s.name):
    current_names.add(section.name)
    url = section_url(section)
    label = f"[{section.name}]({url})" if url else section.name
    if section.name not in previous:
      change = 'new'
    elif previous[section.name] != section.content_fingerprint:
      change = 'updated'
    else:
      change = 'unchanged'
    lines.append(f"- {label} ({change})")
  for name in sorted(set(previous) - current_names):
    lines.append(f"- ~~{name}~~ (no longer applies)")
  if len(sections) == 0 and len(previous) == 0:
    lines.append('- No results to review.')

  if (ci_run is not None and ci_run.head_sha == record.info.head_sha and
      ci_run.status == CIStatus.COMPLETED and ci_run.conclusion is not None):
    lines += ['', f"CI [{ci_run.conclusion.value}]({ci_run.url})."]
  return '\n'.join(lines) + '\n'


class Announcer:
  '''Posts batched pull request comments when served results change.'''

  def __init__(self,
               *,
               store: ForgeStore,
               forge_name: str,
               commenter: ForgeCommenter,
               config: ForgeServiceConfig,
               publisher: Publisher,
               busy: Callable[[], frozenset[int]],
               clock: Callable[[], datetime] = utc_now) -> None:
    self._store = store
    self._forge_name = forge_name
    self._commenter = commenter
    self._config = config
    self._publisher = publisher
    self._busy = busy
    self._clock = clock
    self._adopt_before: Optional[datetime] = None

  async def start(self) -> None:
    '''On a fresh database, adopt (without commenting) the results of pull
    requests last updated before now, instead of announcing everything that
    was already open.
    '''
    if not await self._store.has_announcements(self._forge_name):
      self._adopt_before = self._clock()
      _log.info('fresh announcement state: adopting existing pull requests')

  async def sweep(self) -> None:
    '''Announce changes for every settled, fully processed pull request.'''
    busy = self._busy()
    for record in await self._store.open_pull_requests(self._forge_name):
      if (record.id in busy or record.is_dirty or
          record.info.base_ref not in self._config.base_branches):
        continue
      try:
        await self._maybe_announce(record)
      except Exception:
        _log.exception('announcing for %s#%d failed',
                       record.info.repository, record.info.number)

  async def _settled(self, record: PullRequestRecord) -> bool:
    if not self._config.ci_tracked:
      return True
    if self._clock() >= record.settle_deadline:
      return True
    run = await self._store.ci_run(record.id)
    return (
      run is not None and
      run.head_sha == record.info.head_sha and
      run.status == CIStatus.COMPLETED
    )

  async def _maybe_announce(self, record: PullRequestRecord) -> None:
    sections = await self._store.sections(record.id)
    fingerprint = served_fingerprint(sections)
    announcement = await self._store.announcement(record.id)
    if announcement is not None and announcement.fingerprint == fingerprint:
      return
    if announcement is None and len(sections) == 0:
      # Nothing has been served yet, so there's nothing to announce.
      return
    if not await self._settled(record):
      return

    section_map = {
      section.name: section.content_fingerprint for section in sections
    }
    info = record.info
    if (announcement is None and self._adopt_before is not None and
        info.updated_at < self._adopt_before):
      await self._store.record_announcement(
        record.id,
        fingerprint,
        section_map,
        comment_id = None,
        posted_at = None
      )
      return

    marker = comment_marker(record.review_id, fingerprint)
    comment_id = None
    if (announcement is not None and
        announcement.pending_fingerprint == fingerprint):
      # A previous attempt may have posted before failing to record it.
      comment_id = await self._commenter.find_comment(
        info.repository,
        info.number,
        marker
      )
    if comment_id is None:
      await self._store.set_pending_announcement(record.id, fingerprint)
      body = render_comment(
        record,
        sections,
        announcement.sections if announcement is not None else {},
        await self._store.ci_run(record.id),
        marker,
        lambda section: self._publisher.section_url(record.review_id, section)
      )
      comment_id = await self._commenter.post_comment(
        info.repository,
        info.number,
        body
      )
      _log.info('commented on %s#%d', info.repository, info.number)
    await self._store.record_announcement(
      record.id,
      fingerprint,
      section_map,
      comment_id = comment_id,
      posted_at = self._clock()
    )
