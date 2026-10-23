# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The publisher: serving review sections to AcknowlEDG.

- ``Publisher`` is what the service publishes through (and tests fake).
- The publisher container runs the publisher server (``server``), which
  serves the service's requests with an ``AcknowledgPublisher``
  (``acknowledg``): one AcknowlEDG connection, and the handlers that answer
  reviewers by reading pull request content. Its commands go to the tools
  runner.
- The service reaches it with the publisher client (``client``), which
  implements ``Publisher``; ``protocol`` is the messages between them.
'''

import uuid

from dataclasses import dataclass
from typing import Optional, Protocol, Sequence

from edgacknowledg import RichName
from edgacknowledg.client import form_section_http_url
from edgacknowledg.forge.db import SectionRecord

# Section kinds and the AcknowlEDG subscription type serving each.
KIND_TEST_CHANGES = 'test.changes'
KIND_TEST_FAILURES = 'test.failures'
KIND_BENCH = 'bench'

SUBSCRIPTION_TYPES = {
  KIND_TEST_CHANGES: 'test.src',
  KIND_TEST_FAILURES: 'test.src',
  KIND_BENCH: 'bench.src',
}


class PublisherUnavailableError(Exception):
  '''Raised when AcknowlEDG can't be reached right now (retry later).'''


@dataclass(frozen = True)
class ReviewIdentity:
  review_id: uuid.UUID
  name: RichName
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
