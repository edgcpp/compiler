# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Webhook receiver (optional): a Quart blueprint (``create_receiver``) of
the service's app (``serving``) feeding the ingestor.

Deliveries only identify what may have changed; the ingestor re-queries the
forge for the actual state. Events are queued and deduplicated so a burst of
deliveries can't fan out into unbounded forge API calls.
'''

import asyncio
import logging

from http import HTTPStatus
from typing import Any

from quart import Blueprint, request
from quart.blueprints import BlueprintSetupState

from edgacknowledg.forge import (
  CIRunTouched,
  ForgeEvent,
  ForgeWebhooks,
  PullRequestTouched,
)
from edgacknowledg.forge.ingest import Ingestor

_log = logging.getLogger(__name__)

# GitHub caps deliveries at 25 MiB.
MAX_DELIVERY_BYTES = 25 * 1024 * 1024


class EventQueue:
  '''Pending forge events, each queued at most once until handled.'''

  def __init__(self) -> None:
    self._queue: asyncio.Queue[ForgeEvent] = asyncio.Queue()
    self._pending: set[ForgeEvent] = set()

  def put(self, event: ForgeEvent) -> None:
    if event in self._pending:
      return
    self._pending.add(event)
    self._queue.put_nowait(event)

  async def run(self, ingestor: Ingestor) -> None:
    '''Handle events forever, one at a time.'''
    while True:
      event = await self._queue.get()
      # Drop it from the pending set first: a delivery arriving while this
      # one is handled must be handled again afterwards.
      self._pending.discard(event)
      try:
        if isinstance(event, PullRequestTouched):
          await ingestor.refresh_pull_request(event.repository, event.number)
        elif isinstance(event, CIRunTouched):
          await ingestor.refresh_ci_for_head(event.repository, event.head_sha)
      except asyncio.CancelledError:
        raise
      except Exception:
        # The next poll (or delivery) will catch up.
        _log.exception('failed to handle %s', event)


def create_receiver(webhooks: ForgeWebhooks,
                    events: EventQueue) -> Blueprint:
  '''The receiver, at ``/webhook``, for the service's app (``serving``):
  verifies and parses deliveries with ``webhooks``, and queues their events
  on ``events``.
  '''
  blueprint = Blueprint('webhooks', __name__)

  @blueprint.record_once
  def limit_deliveries(state: BlueprintSetupState) -> None:
    state.app.config['MAX_CONTENT_LENGTH'] = MAX_DELIVERY_BYTES

  @blueprint.post('/webhook')
  async def receive_webhook() -> Any:
    body = await request.get_data(cache = False, as_text = False)
    assert isinstance(body, bytes)
    headers = {key.lower(): value for key, value in request.headers.items()}
    if not webhooks.verify(headers, body):
      _log.warning('rejected a webhook delivery that failed verification')
      return 'verification failed\n', HTTPStatus.UNAUTHORIZED
    for event in webhooks.parse(headers, body):
      events.put(event)
    return '', HTTPStatus.ACCEPTED

  return blueprint
