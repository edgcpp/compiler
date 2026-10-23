# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Database semantics, retries, announcements, and webhooks.'''

import hashlib
import hmac
import json
import shutil
import zipfile

from http import HTTPStatus
from pathlib import Path

import pytest

from edgacknowledg.forge import (
  ArtifactInfo,
  CIConclusion,
  PullRequestTouched,
  CIRunTouched,
)
from edgacknowledg.forge.config import ConfigError
from edgacknowledg.forge.db import ForgeStore
from edgacknowledg.forge.serving import HEALTH_PATH
from edgacknowledg.forge.serving import create_app
from edgacknowledg.forge.webhooks import EventQueue, create_receiver

from conftest import (
  Harness,
  Origin,
  ci_run,
  make_config,
  make_test_artifact,
  pull_request_info,
)
from fakes import REPOSITORY, Clock, FakeForge

PR_FILES = {
  'tests/tests/suite/.a.rto/default.1.1.txt': 'new output\n',
}


async def test_generation_tracks_material_changes(store: ForgeStore,
                                                  clock: Clock) -> None:
  info = pull_request_info(1, 'a' * 40, 'b' * 40, clock = clock)
  assert await store.upsert_pull_request(
    'fake', info, now = clock(), settle_timeout = 60
  )
  assert not await store.upsert_pull_request(
    'fake', info, now = clock(), settle_timeout = 60
  )
  # The base branch moving isn't material.
  moved = pull_request_info(1, 'a' * 40, 'c' * 40, clock = clock)
  assert not await store.upsert_pull_request(
    'fake', moved, now = clock(), settle_timeout = 60
  )
  record = await store.find_pull_request('fake', REPOSITORY, 1)
  assert record is not None and record.generation == 1

  # Processing that started at generation 1 doesn't hide a later change.
  pushed = pull_request_info(1, 'd' * 40, 'c' * 40, clock = clock)
  assert await store.upsert_pull_request(
    'fake', pushed, now = clock(), settle_timeout = 60
  )
  await store.mark_processed(record.id, 1)
  record = await store.get_pull_request(record.id)
  assert record is not None and record.is_dirty


async def test_failures_back_off_then_give_up(store: ForgeStore,
                                              clock: Clock) -> None:
  info = pull_request_info(1, 'a' * 40, 'b' * 40, clock = clock)
  await store.upsert_pull_request(
    'fake', info, now = clock(), settle_timeout = 60
  )
  record = await store.find_pull_request('fake', REPOSITORY, 1)
  assert record is not None

  async def attempt() -> bool:
    return await store.begin_attempt(
      record.id, record.generation,
      now = clock(), max_attempts = 3, base_delay = 60, max_delay = 3600
    )

  # An attempt counts before it starts, so one that never finishes (the
  # service killed part-way) counts too, and isn't retried at once.
  assert not await attempt()
  assert await store.due_pull_requests('fake', clock()) == []
  record = await store.get_pull_request(record.id)
  assert record is not None and record.error_count == 1
  assert record.last_error is not None
  assert 'never finished' in record.last_error

  # One taken back (not the pull request's fault) doesn't count.
  before = record
  assert not await attempt()
  await store.refund_attempt(before)
  record = await store.get_pull_request(record.id)
  assert record is not None and record.error_count == 1

  # Failed ones count, then it gives up.
  for _ in range(2):
    assert not await attempt()
    await store.mark_failed(record.id, record.generation, 'boom')
  record = await store.get_pull_request(record.id)
  assert record is not None and record.last_error == 'boom'
  assert await attempt()
  assert await store.due_pull_requests('fake', clock()) == []

  # A new push makes it due again immediately.
  pushed = pull_request_info(1, 'c' * 40, 'b' * 40, clock = clock)
  await store.upsert_pull_request(
    'fake', pushed, now = clock(), settle_timeout = 60
  )
  assert len(await store.due_pull_requests('fake', clock())) == 1


async def test_publisher_outage_defers_without_counting(
                                                tmp_path: Path,
                                                origin: Origin,
                                                forge: FakeForge,
                                                store: ForgeStore,
                                                clock: Clock) -> None:
  harness = Harness(store, forge, make_config(tmp_path / 'home'), clock)
  await harness.start()
  head = origin.push_pull_request(1, PR_FILES)
  forge.set_pull_request(pull_request_info(1, head, origin.main_sha))
  harness.publisher.available = False
  await harness.step()
  record = await store.find_pull_request('fake', REPOSITORY, 1)
  assert record is not None and record.is_dirty and record.error_count == 0
  # Stored all the same: the database leads, and the publisher client
  # serves what it stores once it can.
  assert [s.name for s in await store.sections(record.id)] == [
    'Test - Changes'
  ]

  harness.publisher.available = True
  await harness.step()
  assert harness.publisher.served_names(record.review_id) == [
    'Test - Changes'
  ]


async def test_connecting_stops_serving_what_is_gone(tmp_path: Path,
                                                    origin: Origin,
                                                    forge: FakeForge,
                                                    store: ForgeStore,
                                                    clock: Clock) -> None:
  config = make_config(tmp_path / 'home')
  harness = Harness(store, forge, config, clock)
  await harness.start()
  head = origin.push_pull_request(1, PR_FILES)
  forge.set_pull_request(pull_request_info(1, head, origin.main_sha))
  await harness.step()
  record = await store.find_pull_request('fake', REPOSITORY, 1)
  assert record is not None
  assert harness.publisher.served_names(record.review_id) == [
    'Test - Changes'
  ]

  # Its files go while the service is down; the publisher, which outlasts
  # it, still serves them until the service connects again.
  [section] = harness.publisher.reviews[record.review_id]
  shutil.rmtree(section.root)
  restarted = Harness(store, forge, config, clock,
                      publisher = harness.publisher,
                      commenter = harness.commenter)
  await restarted.start()
  assert restarted.publisher.served_names(record.review_id) == []
  record = await store.find_pull_request('fake', REPOSITORY, 1)
  assert record is not None and record.is_dirty

  # Rebuilt, it is served again.
  await restarted.step()
  assert restarted.publisher.served_names(record.review_id) == [
    'Test - Changes'
  ]


async def test_settle_deadline_announces_without_ci(tmp_path: Path,
                                                    origin: Origin,
                                                    forge: FakeForge,
                                                    store: ForgeStore,
                                                    clock: Clock) -> None:
  config = make_config(tmp_path / 'home', SETTLE_TIMEOUT = '600')
  harness = Harness(store, forge, config, clock)
  await harness.start()
  head = origin.push_pull_request(1, PR_FILES)
  forge.set_pull_request(pull_request_info(1, head, origin.main_sha))
  await harness.step()
  assert harness.commenter.comments == []

  # No new event at all; the deadline passing is enough.
  clock.advance(601)
  await harness.announcer.sweep()
  assert len(harness.commenter.comments) == 1


async def test_comment_not_duplicated_after_crash(tmp_path: Path,
                                                  origin: Origin,
                                                  forge: FakeForge,
                                                  store: ForgeStore,
                                                  clock: Clock) -> None:
  config = make_config(tmp_path / 'home', CI_WORKFLOW = '')
  harness = Harness(store, forge, config, clock)
  await harness.start()
  head = origin.push_pull_request(1, PR_FILES)
  forge.set_pull_request(pull_request_info(1, head, origin.main_sha))

  # The comment is posted but the response is lost.
  harness.commenter.fail_after_post = True
  await harness.step()
  assert len(harness.commenter.comments) == 1

  # The retry finds the posted comment instead of posting another.
  await harness.announcer.sweep()
  assert len(harness.commenter.comments) == 1
  record = await store.find_pull_request('fake', REPOSITORY, 1)
  assert record is not None
  announcement = await store.announcement(record.id)
  assert announcement is not None and announcement.last_comment_id == '1'
  assert announcement.pending_fingerprint is None


async def test_fresh_database_adopts_existing_pull_requests(
                                                tmp_path: Path,
                                                origin: Origin,
                                                forge: FakeForge,
                                                store: ForgeStore,
                                                clock: Clock) -> None:
  config = make_config(tmp_path / 'home', CI_WORKFLOW = '')
  head1 = origin.push_pull_request(1, PR_FILES)
  forge.set_pull_request(pull_request_info(1, head1, origin.main_sha,
                                           clock = clock))
  clock.advance(60)

  harness = Harness(store, forge, config, clock)
  await harness.start()
  await harness.step()
  # Already open (and last updated) before the service started: adopted.
  assert harness.commenter.comments == []

  # Activity after the start is announced as usual.
  clock.advance(60)
  head2 = origin.push_pull_request(2, PR_FILES)
  forge.set_pull_request(pull_request_info(2, head2, origin.main_sha,
                                           clock = clock))
  await harness.step()
  assert len(harness.commenter.comments) == 1
  assert harness.commenter.comments[0][1] == 2


async def test_rerun_with_same_run_id_replaces_failures(
                                                tmp_path: Path,
                                                origin: Origin,
                                                forge: FakeForge,
                                                store: ForgeStore,
                                                clock: Clock) -> None:
  harness = Harness(store, forge, make_config(tmp_path / 'home'), clock)
  await harness.start()
  head = origin.push_pull_request(1, PR_FILES)
  forge.set_pull_request(pull_request_info(1, head, origin.main_sha))
  zip1 = tmp_path / 'one.zip'
  make_test_artifact(zip1, head, ['suite/b.sft.cpp'])
  forge.artifact_files['a1'] = zip1
  forge.runs[head] = ci_run(head, artifacts = (
    ArtifactInfo('test-results-all', 'a1', 1, None, False),
  ))
  await harness.step()
  record = await store.find_pull_request('fake', REPOSITORY, 1)
  assert record is not None
  assert 'Test - Failures' in harness.publisher.served_names(record.review_id)

  # Re-run attempt 2 of the same run passes.
  forge.runs[head] = ci_run(
    head, attempt = 2, conclusion = CIConclusion.SUCCESS,
    artifacts = (ArtifactInfo('test-results-all', 'a1', 1, None, False),)
  )
  await harness.step()
  assert harness.publisher.served_names(record.review_id) == [
    'Test - Changes'
  ]


async def test_event_queue_deduplicates() -> None:
  queue = EventQueue()
  queue.put(PullRequestTouched(REPOSITORY, 1))
  queue.put(PullRequestTouched(REPOSITORY, 1))
  queue.put(CIRunTouched(REPOSITORY, 'a' * 40))
  assert queue._queue.qsize() == 2


class _SignedWebhooks:
  '''A minimal ForgeWebhooks: HMAC-SHA256 of the body.'''

  def __init__(self, secret: bytes) -> None:
    self._secret = secret

  def verify(self, headers: dict[str, str], body: bytes) -> bool:
    expected = hmac.new(self._secret, body, hashlib.sha256).hexdigest()
    return hmac.compare_digest(headers.get('x-signature', ''), expected)

  def parse(self, headers: dict[str, str], body: bytes) -> list[object]:
    payload = json.loads(body)
    return [PullRequestTouched(payload['repository'], payload['number'])]


async def test_webhook_endpoint_verifies_and_queues() -> None:
  queue = EventQueue()
  app = create_app(create_receiver(
    _SignedWebhooks(b'secret'),  # type: ignore[arg-type]
    queue
  ))
  client = app.test_client()
  body = json.dumps({'repository': REPOSITORY, 'number': 3}).encode()

  response = await client.post(
    '/webhook', data = body, headers = {'X-Signature': 'bogus'}
  )
  assert response.status_code == HTTPStatus.UNAUTHORIZED
  assert queue._queue.qsize() == 0

  signature = hmac.new(b'secret', body, hashlib.sha256).hexdigest()
  response = await client.post(
    '/webhook', data = body, headers = {'X-Signature': signature}
  )
  assert response.status_code == HTTPStatus.ACCEPTED
  assert queue._queue.get_nowait() == PullRequestTouched(REPOSITORY, 3)

  response = await client.get(HEALTH_PATH)
  assert response.status_code == HTTPStatus.OK


async def test_app_without_webhooks_answers_only_health_checks() -> None:
  # As the service serves it without ACKNOWLEDG_FORGE_WEBHOOKS.
  client = create_app().test_client()
  response = await client.get(HEALTH_PATH)
  assert response.status_code == HTTPStatus.OK
  assert await response.get_data(as_text = True) == 'ok\n'
  response = await client.post('/webhook', data = b'{}')
  assert response.status_code == HTTPStatus.NOT_FOUND


def test_webhooks_need_the_service_bind(tmp_path: Path) -> None:
  config = make_config(tmp_path / 'home', SERVICE_BIND = '127.0.0.1:0',
                       WEBHOOKS = '1')
  assert config.service_bind == '127.0.0.1:0' and config.webhooks
  config = make_config(tmp_path / 'home', SERVICE_BIND = '127.0.0.1:0')
  assert not config.webhooks
  with pytest.raises(ConfigError, match = 'SERVICE_BIND is not'):
    make_config(tmp_path / 'home', WEBHOOKS = '1')
  # The old setting isn't silently ignored.
  with pytest.raises(ConfigError, match = 'WEBHOOK_BIND is gone'):
    make_config(tmp_path / 'home', WEBHOOK_BIND = '127.0.0.1:0')


async def test_bench_sections_from_ci_artifact(tmp_path: Path,
                                               origin: Origin,
                                               forge: FakeForge,
                                               store: ForgeStore,
                                               clock: Clock) -> None:
  harness = Harness(store, forge, make_config(tmp_path / 'home'), clock)
  await harness.start()
  head = origin.push_pull_request(1, {'README': 'no test changes\n'})
  forge.set_pull_request(pull_request_info(1, head, origin.main_sha))

  bench_zip = tmp_path / 'bench.zip'
  with zipfile.ZipFile(bench_zip, 'w') as archive:
    archive.writestr('ci-meta.json', json.dumps({'tested_sha': head}))
    archive.writestr('c/x.bnch.cgout', 'summary: 990\n')
    archive.writestr('cpp/x.bnch.cgout', 'summary: 1010\n')
  forge.artifact_files['b1'] = bench_zip
  forge.runs[head] = ci_run(head, conclusion = CIConclusion.SUCCESS,
                            artifacts = (
    ArtifactInfo('bench-results', 'b1', 1, 'sha256:bbb', False),
  ))
  await harness.step()
  record = await store.find_pull_request('fake', REPOSITORY, 1)
  assert record is not None
  # The origin only has a release-c baseline, so only C is served.
  [bench] = harness.publisher.reviews[record.review_id]
  assert bench.name == 'Bench - C'
  assert (bench.root / 'x.bnch.cgout').is_file()
  assert Path(bench.details['baseline_dir'], 'x.bnch.cgout').is_file()
  assert bench.root.is_relative_to(bench.home)

  # Nothing changed: the artifact isn't downloaded again.
  downloads = forge.downloads
  await store.mark_dirty(record.id)
  await harness.step()
  assert forge.downloads == downloads
