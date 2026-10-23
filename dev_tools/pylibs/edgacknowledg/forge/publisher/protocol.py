# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The messages between the publisher client (in the service) and the
publisher server: JSON objects, one per WebSocket text message.

The client sends one initialization message (settings and the API key,
kept out of any command line or environment), then requests, one at a
time:

- ``{'op': 'sync', 'review', 'sections'}`` serves exactly a review's
  sections; with none, it stops serving the review.
- ``{'op': 'delist', 'review_id'}`` stops serving a review whose sections
  are still stored in the database, as its files are about to go.
- ``{'op': 'prune', 'keep': [review_id, ...]}`` stops serving every other
  review. What the server publishes outlasts a connection, so on
  connecting, after serving every review stored in the service's
  database, the client sends this, so that nothing else (e.g. what an
  earlier service served) stays published.

The server answers each ({'ok': true} or {'error': 'failed', 'message'})
once it has queued what to publish, whether or not it is connected to
AcknowlEDG: it publishes what is queued once it is.
'''

import uuid

from pathlib import Path
from typing import Any

from edgacknowledg import is_valid_rich_name
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.publisher import ReviewIdentity

# Longest message (a sync of every section of a review is small; this is a
# sanity limit).
MESSAGE_LIMIT = 16 * 1024 * 1024


def encode_review(review: ReviewIdentity) -> dict[str, Any]:
  return {
    'review_id': str(review.review_id),
    'name': review.name,
    'timestamp': review.timestamp,
  }


def decode_review(value: dict[str, Any]) -> ReviewIdentity:
  name = value['name']
  if not is_valid_rich_name(name):
    raise ValueError(f"invalid review name: {name!r}")
  return ReviewIdentity(
    review_id = uuid.UUID(value['review_id']),
    name = name,
    timestamp = str(value['timestamp'])
  )


def encode_section(section: SectionRecord) -> dict[str, Any]:
  return {
    'kind': section.kind,
    'name': section.name,
    'slug': section.slug,
    'source_key': section.source_key,
    'content_fingerprint': section.content_fingerprint,
    'root': str(section.root),
    'home': str(section.home),
    'details': section.details,
  }


def decode_section(value: dict[str, Any]) -> SectionRecord:
  return SectionRecord(
    kind = str(value['kind']),
    name = str(value['name']),
    slug = str(value['slug']),
    source_key = str(value['source_key']),
    content_fingerprint = str(value['content_fingerprint']),
    root = Path(value['root']),
    home = Path(value['home']),
    details = dict(value['details']),
  )
