# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The publisher: serving review sections to AcknowlEDG.

- ``Publisher`` is what the service publishes through (and tests fake).
- The publisher container runs the publisher server (``server``), which
  queues the service's requests for ``acknowledg.publish``: one AcknowlEDG
  batch session, and the handlers that answer reviewers by reading pull
  request content. Its commands go to the tools runner.
- The service reaches it with the publisher client (``client``), which
  implements ``Publisher``; ``protocol`` is the messages between them.
'''

import uuid

from dataclasses import dataclass
from typing import Optional, Protocol

from edgacknowledg import (
  MAX_RICH_NAME_LENGTH,
  RichName,
  rich_link,
  rich_text,
)
from edgacknowledg.client import form_section_http_url
from edgacknowledg.forge.db import PullRequestRecord, SectionRecord

# Section kinds and the AcknowlEDG subscription type serving each.
KIND_TEST_CHANGES = 'test.changes'
KIND_TEST_FAILURES = 'test.failures'
KIND_BENCH = 'bench'

SUBSCRIPTION_TYPES = {
  KIND_TEST_CHANGES: 'test.src',
  KIND_TEST_FAILURES: 'test.src',
  KIND_BENCH: 'bench.src',
}


# Ends a pull request title cut short to fit in its review's name.
TITLE_ELLIPSIS = '\u2026'


@dataclass(frozen = True)
class ReviewIdentity:
  review_id: uuid.UUID
  name: RichName
  timestamp: str


def review_identity(record: PullRequestRecord) -> ReviewIdentity:
  '''The review of a pull request, as published.'''
  info = record.info
  reference = f"{info.repository}#{info.number}"
  title = f" {info.title}"
  # A forge's titles can be longer than a review's name may be: cut the
  # title (never the link) to fit.
  room = MAX_RICH_NAME_LENGTH - len(info.url) - len(reference)
  if len(title) > room:
    kept = max(room - len(TITLE_ELLIPSIS), 0)
    title = title[:kept].rstrip() + TITLE_ELLIPSIS
  return ReviewIdentity(
    review_id = record.review_id,
    # The pull request reference links to it; the title is only text.
    name = [
      rich_link(info.url, reference),
      rich_text(title),
    ],
    timestamp = record.head_changed_at.isoformat()
  )


class Publisher(Protocol):
  '''Where review sections are served (AcknowlEDG, or a test double):
  what is stored in the service's database.
  '''

  async def sync_one(self, review_id: uuid.UUID) -> None:
    '''Serve what is now stored in the database for the review (none: stop
    serving it); once this returns, the publisher has queued it, and stops
    serving what was stored in the database before moments later. While
    the publisher can't take it, it returns all the same: the publisher
    serves what is stored on connecting.
    '''
    ...

  async def delist(self, review_id: uuid.UUID) -> None:
    '''Stop serving the review, though its sections are still stored in
    the database: their files are about to go (and, after them, what is
    stored in the database). Raises ``ServiceUnavailableError`` unless
    the publisher has queued it.
    '''
    ...

  def section_url(self,
                  review_id: uuid.UUID,
                  section: SectionRecord) -> Optional[str]:
    ...


def section_url(ui_host: str,
                ui_port: int,
                review_id: uuid.UUID,
                section: SectionRecord) -> Optional[str]:
  '''Where reviewers see ``section`` of ``review_id`` in the AcknowlEDG UI.'''
  return form_section_http_url(
    ui_host,
    ui_port,
    review_id,
    SUBSCRIPTION_TYPES[section.kind],
    section.slug
  )
