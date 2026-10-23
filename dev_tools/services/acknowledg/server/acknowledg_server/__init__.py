# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import asyncio
import hmac
import os
import uuid

from functools import lru_cache, wraps
from http import HTTPStatus
from typing import (
  Any, AsyncGenerator, Awaitable, Callable, Dict, Optional, TypeVar
)

from quart import Quart, abort, websocket  # type: ignore[import-not-found]
from serde import SerdeError, from_dict  # type: ignore[import-not-found]

from edgacknowledg import (
  ALLOWED_BENCH_EDIT_TAGS,
  ALLOWED_BENCH_SOURCE_TAGS,
  ALLOWED_REVIEWER_REQUEST_TAGS,
  ALLOWED_TEST_EDIT_TAGS,
  ALLOWED_TEST_SOURCE_TAGS,
  BATCH_SUBSCRIPTION_TYPES,
  IncompatibleProtocolVersion,
  ProtocolMessage,
  RequestTags,
  RichName,
  RoleOccupied,
  batch_event_message,
  decode_response,
  encode_message,
  is_iso_timestamp,
  is_valid_rich_name,
  is_valid_section_slug,
  message,
  outdated_protocol_message,
  rich_text,
  role_occupied_message,
  unwrap_batch_event
)

from acknowledg_server.reviewer_requests import (
  REVIEWER_REQUEST_VALUE_MODELS,
  BenchId,
  EmptyRequest,
  ShowCurrDiffRequest,
  ShowTestSourceRequest,
  TestCaseId,
  UpdateDiffRequest
)

app = Quart(__name__)

# IANA WebSocket Close Code Number Registry: 3000 Unauthorized
WS_CLOSE_UNAUTHORIZED = 3000
# A provider reported that the reviewer requested a file it may not read.
WS_CLOSE_ILLEGAL_ACCESS = 3001

F = TypeVar('F', bound = Callable[..., Any])

class Broker:
  def __init__(self) -> None:
    self.connections: set[asyncio.Queue[ProtocolMessage]] = set()

  async def publish(self, msg: ProtocolMessage) -> None:
    for connection in self.connections:
      await connection.put(msg)

  async def subscribe(self) -> AsyncGenerator[ProtocolMessage, None]:
    connection: asyncio.Queue[ProtocolMessage] = asyncio.Queue()
    self.connections.add(connection)
    try:
      while True:
        yield await connection.get()
    finally:
      self.connections.remove(connection)

class IdentifiedBroker:
  def __init__(self) -> None:
    self.connections: dict[uuid.UUID, asyncio.Queue[ProtocolMessage]] = {}

  async def publish(self, client_id: uuid.UUID, msg: ProtocolMessage) -> None:
    if client_id not in self.connections:
      return
    await self.connections[client_id].put(msg)

  async def publish_all(self, msg: ProtocolMessage) -> None:
    for connection in list(self.connections.values()):
      await connection.put(msg)

  async def subscribe(
                self,
                client_id: uuid.UUID) -> AsyncGenerator[ProtocolMessage, None]:
    assert client_id not in self.connections
    connection: asyncio.Queue[ProtocolMessage] = asyncio.Queue()
    self.connections[client_id] = connection

    try:
      while True:
        yield await connection.get()
    finally:
        del self.connections[client_id]

class SingleTargetBroker:
  def __init__(self) -> None:
    self.msg_queue: Optional[asyncio.Queue[ProtocolMessage]] = None

  def is_occupied(self) -> bool:
    return self.msg_queue is not None

  async def publish(self, msg: ProtocolMessage) -> None:
    assert self.msg_queue is not None
    await self.msg_queue.put(msg)

  def subscribe(self) -> AsyncGenerator[ProtocolMessage, None]:
    # Claim occupancy immediately. This is not `async def` so RoleOccupied is
    # raised at subscribe() rather than on first iteration of the generator.
    if self.msg_queue is not None:
      raise RoleOccupied()
    self.msg_queue = asyncio.Queue()

    async def _generate() -> AsyncGenerator[ProtocolMessage, None]:
      try:
        while True:
          assert self.msg_queue is not None
          yield await self.msg_queue.get()
      finally:
        self.msg_queue = None

    return _generate()

class SectionSlot:
  '''One slug-keyed provider slot (display name + broker).'''

  def __init__(self, section_name: str) -> None:
    self.section_name = section_name
    self.broker = SingleTargetBroker()

class NoTestSourceSocket(Exception):
  pass

class NoBenchSourceSocket(Exception):
  pass

class NoTestEditSocket(Exception):
  pass

class ReviewerExpelled(Exception):
  '''Ends a reviewer session that a provider reported for illegal access.'''
  pass

# Queued to a reviewer's session to end it (``ReviewerExpelled``); never
# sent. Such a reviewer gets no explanation: the request couldn't have come
# from the reviewer UI.
_EXPEL_REVIEWER: ProtocolMessage = {}

class NoBenchEditSocket(Exception):
  pass

API_KEY_CREDENTIAL = 'api-key'

@lru_cache(maxsize = None)
def _configured_api_key() -> Optional[str]:
  '''Return the API key protecting write-capable routes, if any.

  A systemd credential named ``api-key`` (``LoadCredential=`` or
  ``LoadCredentialEncrypted=``) takes precedence, so deployments can keep the
  key out of the process environment. Otherwise ``EDG_ACKNOWLEDG_API_KEY`` is
  used. The key is read once and cached.
  '''
  credentials_dir = os.environ.get('CREDENTIALS_DIRECTORY')
  if credentials_dir is not None:
    path = os.path.join(credentials_dir, API_KEY_CREDENTIAL)
    if os.path.isfile(path):
      with open(path, encoding = 'utf-8') as credential:
        return credential.read().rstrip('\r\n')
  return os.environ.get('EDG_ACKNOWLEDG_API_KEY')

def _extract_request_api_key() -> Optional[str]:
  return websocket.headers.get('X-AcknowlEDG-API-Key')

def require_api_key(func: F) -> F:
  '''Authenticate write-capable websocket routes with a shared API key.

  When no key is configured (see ``_configured_api_key``), authentication is
  skipped (local development). Otherwise the key must be supplied via the
  ``X-AcknowlEDG-API-Key`` header; it is compared in constant time.

  The websocket is accepted before authentication so it can be closed with an
  application close code on failure.
  '''
  @wraps(func)
  async def wrapper(*args: Any, **kwargs: Any) -> Any:
    await websocket.accept()
    expected = _configured_api_key()
    if expected is not None:
      provided = _extract_request_api_key()
      if provided is None or not hmac.compare_digest(
          provided.encode('utf-8'), expected.encode('utf-8')):
        await websocket.close(WS_CLOSE_UNAUTHORIZED)
        abort(HTTPStatus.UNAUTHORIZED)
    return await func(*args, **kwargs)
  return wrapper  # type: ignore[return-value]

class ReviewDescriptor:
  def __init__(self, run_id: uuid.UUID) -> None:
    self.run_id = run_id
    self._name: Optional[RichName] = None
    self.created_at: Optional[str] = None
    self._reviewer_brokers = IdentifiedBroker()
    self._test_editors: Dict[str, SectionSlot] = {}
    self._bench_editors: Dict[str, SectionSlot] = {}
    self._test_sources: Dict[str, SectionSlot] = {}
    self._bench_sources: Dict[str, SectionSlot] = {}

  def get_name(self) -> Optional[RichName]:
    return self._name

  async def set_name(self, name: RichName) -> None:
    self._name = name

  def display_name(self) -> RichName:
    '''The name shown to reviewers: the review's id until one is set.'''
    if self._name is None:
      return [rich_text(str(self.run_id))]
    return self._name

  def get_num_reviewers(self) -> int:
    return len(self._reviewer_brokers.connections)

  def connection_state(self) -> dict[str, Any]:
    sources = []
    for slug, slot in self._test_sources.items():
      if slot.broker.is_occupied():
        sources.append({
          'kind': 'test',
          'slug': slug,
          'name': slot.section_name
        })
    for slug, slot in self._bench_sources.items():
      if slot.broker.is_occupied():
        sources.append({
          'kind': 'bench',
          'slug': slug,
          'name': slot.section_name
        })

    editors = []
    for slug, slot in self._test_editors.items():
      if slot.broker.is_occupied():
        editors.append({ 'kind': 'test', 'slug': slug })
    for slug, slot in self._bench_editors.items():
      if slot.broker.is_occupied():
        editors.append({ 'kind': 'bench', 'slug': slug })

    return {
      'name': self.display_name(),
      'sources': sources,
      'editors': editors
    }

  def has_test_source(self) -> bool:
    return any(
      slot.broker.is_occupied() for slot in self._test_sources.values()
    )

  def has_bench_source(self) -> bool:
    return any(
      slot.broker.is_occupied() for slot in self._bench_sources.values()
    )

  def has_test_editor(self) -> bool:
    return any(
      slot.broker.is_occupied() for slot in self._test_editors.values()
    )

  def has_bench_editor(self) -> bool:
    return any(
      slot.broker.is_occupied() for slot in self._bench_editors.values()
    )

  async def is_dead(self) -> bool:
    if self.get_num_reviewers() != 0:
      return False
    if self.has_test_source():
      return False
    if self.has_bench_source():
      return False
    if self.has_test_editor():
      return False
    if self.has_bench_editor():
      return False
    return True

  async def subscribe_as_reviewer(
              self,
              reviewer_id: uuid.UUID) -> AsyncGenerator[ProtocolMessage, None]:
    subscription = self._reviewer_brokers.subscribe(reviewer_id)
    await notify_review_state_changed(self)
    try:
      async for msg in subscription:
        yield msg
    finally:
      await notify_review_state_changed(self)

  async def publish_to_reviewer(self, reviewer_id: uuid.UUID,
                                msg: ProtocolMessage) -> None:
    await self._reviewer_brokers.publish(reviewer_id, msg)

  async def broadcast_to_reviewers(self, msg: ProtocolMessage) -> None:
    await self._reviewer_brokers.publish_all(msg)

  async def expel_reviewer(self, reviewer_id: uuid.UUID) -> None:
    '''End the session of a reviewer that a provider reported for illegal
    access (``ReviewerExpelled``).
    '''
    await self.publish_to_reviewer(reviewer_id, _EXPEL_REVIEWER)

  def _subscribe_section_slot(
      self,
      slots: Dict[str, SectionSlot],
      slug: str) -> AsyncGenerator[ProtocolMessage, None]:
    # Intentionally not `async def`. The slot is inserted (and the broker
    # claims occupancy) when this function is called, before the returned
    # generator is iterated. That way RoleOccupied is raised in
    # `_run_provider_session` instead of inside the TaskGroup, where a plain
    # `except RoleOccupied` would miss it.
    if slug in slots:
      raise RoleOccupied()
    slot = SectionSlot('')
    slots[slug] = slot
    subscription = slot.broker.subscribe()

    async def _generate() -> AsyncGenerator[ProtocolMessage, None]:
      await notify_review_state_changed(self)
      try:
        async for msg in subscription:
          yield msg
      finally:
        del slots[slug]
        await notify_review_state_changed(self)

    return _generate()

  # These wrappers are plain functions so `_subscribe_section_slot` claims
  # occupancy at the call in `_run_provider_session`, not on first `async for`.
  def subscribe_as_test_editor(
      self,
      slug: str) -> AsyncGenerator[ProtocolMessage, None]:
    return self._subscribe_section_slot(self._test_editors, slug)

  def subscribe_as_bench_editor(
      self,
      slug: str) -> AsyncGenerator[ProtocolMessage, None]:
    return self._subscribe_section_slot(self._bench_editors, slug)

  def subscribe_as_test_source(
      self,
      slug: str) -> AsyncGenerator[ProtocolMessage, None]:
    return self._subscribe_section_slot(self._test_sources, slug)

  def subscribe_as_bench_source(
      self,
      slug: str) -> AsyncGenerator[ProtocolMessage, None]:
    return self._subscribe_section_slot(self._bench_sources, slug)

  def section_slot_for_slug(self, slug: str) -> Optional[SectionSlot]:
    for slots in (
      self._test_sources,
      self._bench_sources,
      self._test_editors,
      self._bench_editors
    ):
      slot = slots.get(slug)
      if slot is not None:
        return slot
    return None

  def _occupied_slot(self,
                     slots: Dict[str, SectionSlot],
                     slug: str) -> Optional[SectionSlot]:
    slot = slots.get(slug)
    if slot is None or not slot.broker.is_occupied():
      return None
    return slot

  async def send_to_test_source(self, slug: str,
                                tag: str, value: Any) -> None:
    slot = self._occupied_slot(self._test_sources, slug)
    if slot is None:
      raise NoTestSourceSocket()
    await slot.broker.publish(message(tag, value = value))

  async def send_to_bench_source(self, slug: str,
                                 tag: str, value: Any) -> None:
    slot = self._occupied_slot(self._bench_sources, slug)
    if slot is None:
      raise NoBenchSourceSocket()
    await slot.broker.publish(message(tag, value = value))

  async def send_test_statuses_to(self, slug: str,
                                  reviewer_id: uuid.UUID) -> None:
    await self.send_to_test_source(
      slug,
      RequestTags.TEST_LIST_STATUSES,
      { 'reviewer_id': reviewer_id }
    )

  async def send_test_diff_to(self, slug: str, reviewer_id: uuid.UUID,
                              test_id: str) -> None:
    await self.send_to_test_source(
      slug,
      RequestTags.TEST_SHOW_DIFF,
      { 'id': test_id, 'reviewer_id': reviewer_id }
    )

  async def send_curr_test_diff_to(self, slug: str, reviewer_id: uuid.UUID,
                                   test_id: str,
                                   configs: list[str]) -> None:
    await self.send_to_test_source(
      slug,
      RequestTags.TEST_SHOW_CURR_DIFF,
      {
        'id': test_id,
        'configs': configs,
        'reviewer_id': reviewer_id
      }
    )

  async def send_test_raw_output_to(self, slug: str, reviewer_id: uuid.UUID,
                                    test_id: str) -> None:
    await self.send_to_test_source(
      slug,
      RequestTags.TEST_SHOW_RAW_OUTPUT,
      { 'id': test_id, 'reviewer_id': reviewer_id }
    )

  async def send_list_of_assoc_files_to(self, slug: str,
                                        reviewer_id: uuid.UUID,
                                        test_id: str) -> None:
    await self.send_to_test_source(
      slug,
      RequestTags.TEST_LIST_ASSOC_FILES,
      { 'id': test_id, 'reviewer_id': reviewer_id }
    )

  async def send_test_source_to(self, slug: str, reviewer_id: uuid.UUID,
                                test_id: str,
                                assoc_file: Optional[str]) -> None:
    await self.send_to_test_source(
      slug,
      RequestTags.TEST_SHOW_SOURCE,
      {
        'id': test_id,
        'assoc_file': assoc_file,
        'reviewer_id': reviewer_id
      }
    )

  async def update_test_diff_for(self, slug: str, reviewer_id: uuid.UUID,
                                 test_id: str,
                                 configs: list[str]) -> None:
    slot = self._occupied_slot(self._test_editors, slug)
    if slot is None:
      raise NoTestEditSocket()
    await slot.broker.publish(message(
      RequestTags.TEST_UPDATE_DIFF,
      value = {
        'id': test_id,
        'configs': configs,
        'reviewer_id': reviewer_id
      }
    ))

  async def send_bench_statuses_to(self, slug: str,
                                   reviewer_id: uuid.UUID) -> None:
    await self.send_to_bench_source(
      slug,
      RequestTags.BENCH_LIST_STATUSES,
      { 'reviewer_id': reviewer_id }
    )

  async def send_bench_annotate_to(self, slug: str, reviewer_id: uuid.UUID,
                                   bench_id: str) -> None:
    await self.send_to_bench_source(
      slug,
      RequestTags.BENCH_SHOW_ANNOTATE,
      { 'id': bench_id, 'reviewer_id': reviewer_id }
    )

  async def update_bench_baseline_for(self, slug: str,
                                      reviewer_id: uuid.UUID,
                                      bench_id: str) -> None:
    slot = self._occupied_slot(self._bench_editors, slug)
    if slot is None:
      raise NoBenchEditSocket()
    await slot.broker.publish(message(
      RequestTags.BENCH_UPDATE_BASELINE,
      value = {
        'id': bench_id,
        'reviewer_id': reviewer_id
      }
    ))

review_listing_broker = Broker()
state_map: dict[uuid.UUID, ReviewDescriptor] = {}

def _form_review_entry(run_descr: ReviewDescriptor) -> dict[str, Any]:
  return {
    'id': run_descr.run_id,
    'name': run_descr.display_name(),
    'created_at': run_descr.created_at,
    'sockets': {
      'reviewer': run_descr.get_num_reviewers(),
      'test_editor': (1 if run_descr.has_test_editor() else 0),
      'bench_editor': (1 if run_descr.has_bench_editor() else 0),
      'test_source': (1 if run_descr.has_test_source() else 0),
      'bench_source': (1 if run_descr.has_bench_source() else 0)
    }
  }

def _form_review_list() -> list[Any]:
  return [
    _form_review_entry(run_descr)
    for run_descr in state_map.values()
  ]

async def publish_review_list_patch(
    *,
    upsert: Optional[list[dict[str, Any]]] = None,
    remove: Optional[list[uuid.UUID]] = None) -> None:
  '''Broadcast an incremental review-list update to listing clients.'''
  patch: dict[str, Any] = {}
  if upsert:
    patch['upsert'] = upsert
  if remove:
    patch['remove'] = remove
  if not patch:
    return
  await review_listing_broker.publish(
    message(RequestTags.LIST_REVIEWS_PATCH, value = patch)
  )

async def notify_review_state_changed(run_descr: ReviewDescriptor) -> None:
  await publish_review_list_patch(upsert = [ _form_review_entry(run_descr) ])
  await run_descr.broadcast_to_reviewers(message(
    RequestTags.CONNECTION_STATE,
    value = run_descr.connection_state()
  ))

async def remove_run_if_dead(run_id: uuid.UUID) -> None:
  global state_map

  if run_id not in state_map:
    return

  if await state_map[run_id].is_dead():
    del state_map[run_id]
    await publish_review_list_patch(remove = [ run_id ])

def _get_or_create_run(run_id: uuid.UUID) -> ReviewDescriptor:
  global state_map
  if run_id not in state_map:
    state_map[run_id] = ReviewDescriptor(run_id)
  return state_map[run_id]

def _edit_reply_value(request_value: Any) -> Any:
  reply = {
    'id': request_value['id'],
    'success': request_value['success']
  }
  if 'configs' in request_value:
    reply['configs'] = request_value['configs']
  return reply

def _source_reply_value(request_value: Any) -> Any:
  return request_value.get('client_response')

SendFn = Callable[[ProtocolMessage], Awaitable[None]]
RecvFn = Callable[[], Awaitable[Any]]

# Bound inbound messages per batch subscription (load control only).
_BATCH_INBOUND_QUEUE_SIZE = 32

class BatchInboundBroker:
  '''Queue that feeds a logical subscription's ``RecvFn``.'''

  def __init__(self) -> None:
    self._queue: asyncio.Queue[ProtocolMessage] = asyncio.Queue(
      maxsize = _BATCH_INBOUND_QUEUE_SIZE
    )

  async def publish(self, msg: ProtocolMessage) -> None:
    await self._queue.put(msg)

  async def receive(self) -> ProtocolMessage:
    return await self._queue.get()

async def _apply_provider_meta(
    run_descr: ReviewDescriptor,
    slug: str,
    request_tag: str,
    request_value: Any,
    *,
    send: SendFn) -> bool:
  '''Handle optional provider meta messages. Return True if consumed.'''
  if request_tag == RequestTags.META_SECTION_NAME:
    if not isinstance(request_value, str) or not request_value:
      await send(message(RequestTags.INVALID_REQUEST))
      return True
    slot = run_descr.section_slot_for_slug(slug)
    if slot is None:
      await send(message(RequestTags.INVALID_REQUEST))
      return True
    slot.section_name = request_value
    await notify_review_state_changed(run_descr)
    return True

  if request_tag == RequestTags.META_REVIEW_NAME:
    if not is_valid_rich_name(request_value):
      await send(message(RequestTags.INVALID_REQUEST))
      return True
    if run_descr.get_name() is None:
      await run_descr.set_name(request_value)
      await notify_review_state_changed(run_descr)
    return True

  if request_tag == RequestTags.META_REVIEW_TIMESTAMP:
    if not isinstance(request_value, str) or not request_value:
      await send(message(RequestTags.INVALID_REQUEST))
      return True
    if not is_iso_timestamp(request_value):
      await send(message(RequestTags.INVALID_REQUEST))
      return True
    if run_descr.created_at is None:
      run_descr.created_at = request_value
      await notify_review_state_changed(run_descr)
    return True

  return False

async def _bridge_subscription_to_send(
    subscription: AsyncGenerator[ProtocolMessage, None],
    send: SendFn) -> None:
  async for msg in subscription:
    await send(msg)

async def _receive_provider_replies(
    run_descr: ReviewDescriptor,
    *,
    section: str,
    allowed_tags: frozenset[str],
    reply_value_from: Callable[[Any], Any],
    send: SendFn,
    recv: RecvFn) -> None:
  while True:
    try:
      request_tag, request_value, _reply_section = decode_response(
        await recv()
      )

      if await _apply_provider_meta(
        run_descr,
        section,
        request_tag,
        request_value,
        send = send
      ):
        continue

      if request_tag == RequestTags.ILLEGAL_ACCESS:
        try:
          offender = uuid.UUID(str(request_value['reviewer_id']))
        except (KeyError, TypeError, ValueError):
          await send(message(RequestTags.INVALID_REQUEST))
          continue
        # Reviewers are per review, so a provider can only name its own.
        await run_descr.expel_reviewer(offender)
        continue

      if (request_tag not in allowed_tags or
          request_value is None or
          'reviewer_id' not in request_value):
        await send(message(RequestTags.INVALID_REQUEST))
        continue

      # Always stamp the provider socket's bound slug so clients cannot spoof
      # which section a reply belongs to.
      reviewer_id = uuid.UUID(request_value['reviewer_id'])
      await run_descr.publish_to_reviewer(
        reviewer_id,
        message(
          request_tag,
          value = reply_value_from(request_value),
          section = section
        )
      )
    except asyncio.CancelledError:
      break
    except:
      await send(message(RequestTags.INVALID_REQUEST))
      raise

async def _run_provider_session(
    run_id: uuid.UUID,
    slug: str,
    *,
    send: SendFn,
    recv: RecvFn,
    subscribe: Callable[
      [ReviewDescriptor], AsyncGenerator[ProtocolMessage, None]
    ],
    allowed_reply_tags: frozenset[str],
    reply_value_from: Callable[[Any], Any]) -> None:
  '''Run a source/edit provider session that bridges to reviewers.

  ``send`` / ``recv`` abstract the transport (dedicated websocket or batch
  event channel). Section display name, review name, and timestamp are
  optional messages handled while the session is running.
  '''
  if not is_valid_section_slug(slug):
    await send(message(RequestTags.INVALID_REQUEST))
    return

  run_descr = _get_or_create_run(run_id)
  try:
    async with asyncio.TaskGroup() as tg:
      tg.create_task(_bridge_subscription_to_send(
        subscribe(run_descr),
        send
      ))
      tg.create_task(_receive_provider_replies(
        run_descr,
        section = slug,
        allowed_tags = allowed_reply_tags,
        reply_value_from = reply_value_from,
        send = send,
        recv = recv
      ))
  except* RoleOccupied:
    await send(role_occupied_message())
  except* IncompatibleProtocolVersion:
    await send(outdated_protocol_message())
  finally:
    await remove_run_if_dead(run_id)

async def _dispatch_reviewer_request(
    run_descr: ReviewDescriptor,
    reviewer_id: uuid.UUID,
    request_tag: str,
    request_value: Any,
    section: Optional[str],
    *,
    send: SendFn) -> None:
  if request_tag not in ALLOWED_REVIEWER_REQUEST_TAGS:
    await send(message(RequestTags.INVALID_REQUEST))
    return

  if (not isinstance(section, str) or
      not is_valid_section_slug(section)):
    await send(message(RequestTags.INVALID_REQUEST))
    return

  value_model = REVIEWER_REQUEST_VALUE_MODELS[request_tag]
  try:
    if value_model is EmptyRequest and (
        request_value is None or request_value == {}
    ):
      parsed: Any = EmptyRequest()
    else:
      parsed = from_dict(value_model, request_value)
  except SerdeError:
    await send(message(RequestTags.INVALID_REQUEST))
    return

  match (request_tag, parsed):
    case (RequestTags.TEST_LIST_STATUSES, EmptyRequest()):
      await run_descr.send_test_statuses_to(section, reviewer_id)
    case (RequestTags.TEST_SHOW_DIFF, TestCaseId(value = test_id)):
      await run_descr.send_test_diff_to(section, reviewer_id, test_id)
    case (RequestTags.TEST_SHOW_CURR_DIFF,
          ShowCurrDiffRequest(id = test_id, configs = configs)):
      await run_descr.send_curr_test_diff_to(
        section,
        reviewer_id,
        test_id,
        configs
      )
    case (RequestTags.TEST_SHOW_RAW_OUTPUT, TestCaseId(value = test_id)):
      await run_descr.send_test_raw_output_to(section, reviewer_id, test_id)
    case (RequestTags.TEST_LIST_ASSOC_FILES, TestCaseId(value = test_id)):
      await run_descr.send_list_of_assoc_files_to(
        section,
        reviewer_id,
        test_id
      )
    case (RequestTags.TEST_SHOW_SOURCE,
          ShowTestSourceRequest(id = test_id, assoc_file = assoc_file)):
      await run_descr.send_test_source_to(
        section,
        reviewer_id,
        test_id,
        assoc_file
      )
    case (RequestTags.TEST_UPDATE_DIFF,
          UpdateDiffRequest(id = test_id, configs = configs)):
      await run_descr.update_test_diff_for(
        section,
        reviewer_id,
        test_id,
        configs
      )
    case (RequestTags.BENCH_LIST_STATUSES, EmptyRequest()):
      await run_descr.send_bench_statuses_to(section, reviewer_id)
    case (RequestTags.BENCH_SHOW_ANNOTATE, BenchId(value = bench_id)):
      await run_descr.send_bench_annotate_to(section, reviewer_id, bench_id)
    case (RequestTags.BENCH_UPDATE_BASELINE, BenchId(value = bench_id)):
      await run_descr.update_bench_baseline_for(
        section,
        reviewer_id,
        bench_id
      )
    case unreachable:
      raise AssertionError(unreachable)

async def _run_reviewer_session(
    run_descr: ReviewDescriptor,
    *,
    send: SendFn,
    recv: RecvFn) -> bool:
  '''Run a browser/reviewer session for one review.

  Return whether it ended because a provider reported the reviewer for
  illegal access; the caller then drops the connection, without telling the
  reviewer why.
  '''
  expelled = False
  try:
    reviewer_id = uuid.uuid4()

    async def push_outbound() -> None:
      await send(message(
        RequestTags.CONNECTION_STATE,
        value = run_descr.connection_state()
      ))
      async for msg in run_descr.subscribe_as_reviewer(reviewer_id):
        if msg is _EXPEL_REVIEWER:
          raise ReviewerExpelled()
        await send(msg)

    async def pull_inbound() -> None:
      while True:
        try:
          request_tag, request_value, section = decode_response(
            await recv()
          )
          await _dispatch_reviewer_request(
            run_descr,
            reviewer_id,
            request_tag,
            request_value,
            section,
            send = send
          )
        except asyncio.CancelledError:
          break
        except NoTestSourceSocket:
          await send(message(RequestTags.TEST_MISSING_SOURCE))
        except NoBenchSourceSocket:
          await send(message(RequestTags.BENCH_MISSING_SOURCE))
        except NoTestEditSocket:
          await send(message(RequestTags.TEST_MISSING_EDITOR))
        except NoBenchEditSocket:
          await send(message(RequestTags.BENCH_MISSING_EDITOR))
        except:
          await send(message(RequestTags.INVALID_REQUEST))
          raise

    async with asyncio.TaskGroup() as tg:
      tg.create_task(push_outbound())
      tg.create_task(pull_inbound())
  except* IncompatibleProtocolVersion:
    await send(outdated_protocol_message())
  except* ReviewerExpelled:
    expelled = True
  finally:
    await remove_run_if_dead(run_descr.run_id)
  return expelled

def _websocket_send() -> SendFn:
  async def send(msg: ProtocolMessage) -> None:
    await websocket.send(encode_message(msg))
  return send

def _websocket_recv() -> RecvFn:
  async def recv() -> str:
    return await websocket.receive()
  return recv

@app.websocket('/review')
async def reviews_handler() -> None:
  await websocket.send(encode_message(message(
    RequestTags.LIST_REVIEWS,
    value = _form_review_list()
  )))
  async for msg in review_listing_broker.subscribe():
    await websocket.send(encode_message(msg))

@app.websocket('/review/<uuid:run_id>')
async def review_handler(run_id: uuid.UUID) -> None:
  run_descr = _get_or_create_run(run_id)
  if await _run_reviewer_session(
      run_descr,
      send = _websocket_send(),
      recv = _websocket_recv()):
    await websocket.close(WS_CLOSE_ILLEGAL_ACCESS)

@app.websocket('/edit/<uuid:run_id>/test/<slug>')
@require_api_key
async def test_edit_handler(run_id: uuid.UUID, slug: str) -> None:
  await _run_provider_session(
    run_id,
    slug,
    send = _websocket_send(),
    recv = _websocket_recv(),
    subscribe = (
      lambda run_descr: run_descr.subscribe_as_test_editor(slug)
    ),
    allowed_reply_tags = ALLOWED_TEST_EDIT_TAGS,
    reply_value_from = _edit_reply_value
  )

@app.websocket('/edit/<uuid:run_id>/bench/<slug>')
@require_api_key
async def bench_edit_handler(run_id: uuid.UUID, slug: str) -> None:
  await _run_provider_session(
    run_id,
    slug,
    send = _websocket_send(),
    recv = _websocket_recv(),
    subscribe = (
      lambda run_descr: run_descr.subscribe_as_bench_editor(slug)
    ),
    allowed_reply_tags = ALLOWED_BENCH_EDIT_TAGS,
    reply_value_from = _edit_reply_value
  )

@app.websocket('/source/<uuid:run_id>/test/<slug>')
@require_api_key
async def test_source_handler(run_id: uuid.UUID, slug: str) -> None:
  await _run_provider_session(
    run_id,
    slug,
    send = _websocket_send(),
    recv = _websocket_recv(),
    subscribe = (
      lambda run_descr: run_descr.subscribe_as_test_source(slug)
    ),
    allowed_reply_tags = ALLOWED_TEST_SOURCE_TAGS,
    reply_value_from = _source_reply_value
  )

@app.websocket('/source/<uuid:run_id>/bench/<slug>')
@require_api_key
async def bench_source_handler(run_id: uuid.UUID, slug: str) -> None:
  await _run_provider_session(
    run_id,
    slug,
    send = _websocket_send(),
    recv = _websocket_recv(),
    subscribe = (
      lambda run_descr: run_descr.subscribe_as_bench_source(slug)
    ),
    allowed_reply_tags = ALLOWED_BENCH_SOURCE_TAGS,
    reply_value_from = _source_reply_value
  )

class _BatchSubscription:
  def __init__(self, subscription_id: uuid.UUID, task: asyncio.Task,
               inbound: BatchInboundBroker) -> None:
    self.subscription_id = subscription_id
    self.task = task
    self.inbound = inbound

def _provider_subscribe_for_type(
    sub_type: str,
    slug: str
    ) -> tuple[
      Callable[[ReviewDescriptor], AsyncGenerator[ProtocolMessage, None]],
      frozenset[str],
      Callable[[Any], Any]
    ]:
  match sub_type:
    case 'test.src':
      return (
        lambda run_descr: run_descr.subscribe_as_test_source(slug),
        ALLOWED_TEST_SOURCE_TAGS,
        _source_reply_value
      )
    case 'test.edit':
      return (
        lambda run_descr: run_descr.subscribe_as_test_editor(slug),
        ALLOWED_TEST_EDIT_TAGS,
        _edit_reply_value
      )
    case 'bench.src':
      return (
        lambda run_descr: run_descr.subscribe_as_bench_source(slug),
        ALLOWED_BENCH_SOURCE_TAGS,
        _source_reply_value
      )
    case 'bench.edit':
      return (
        lambda run_descr: run_descr.subscribe_as_bench_editor(slug),
        ALLOWED_BENCH_EDIT_TAGS,
        _edit_reply_value
      )
    case _:
      raise ValueError(f"not a provider subscription type: {sub_type}")

@app.websocket('/batch')
@require_api_key
async def batch_handler() -> None:
  subscriptions: Dict[uuid.UUID, _BatchSubscription] = {}
  # Set when a reviewer session on this socket is expelled for illegal
  # access: the whole socket is closed, every other subscription with it.
  expelled = asyncio.Event()

  async def receive() -> Any:
    '''The next frame; raise ``ReviewerExpelled`` once a reviewer session
    on this socket was expelled.
    '''
    frame = asyncio.ensure_future(websocket.receive())
    expulsion = asyncio.ensure_future(expelled.wait())
    try:
      await asyncio.wait(
        {frame, expulsion},
        return_when = asyncio.FIRST_COMPLETED
      )
    finally:
      for task in (frame, expulsion):
        task.cancel()
    if expelled.is_set():
      raise ReviewerExpelled()
    return frame.result()

  async def cancel_subscription(subscription_id: uuid.UUID) -> None:
    state = subscriptions.pop(subscription_id, None)
    if state is None:
      return
    state.task.cancel()
    try:
      await state.task
    except asyncio.CancelledError:
      pass
    except Exception:
      pass

  async def start_subscription(value: Any) -> None:
    if not isinstance(value, dict):
      await websocket.send(
        encode_message(message(RequestTags.INVALID_REQUEST))
      )
      return

    sub_type = value.get('type')
    if sub_type not in BATCH_SUBSCRIPTION_TYPES:
      await websocket.send(
        encode_message(message(RequestTags.INVALID_REQUEST))
      )
      return

    try:
      run_id = uuid.UUID(str(value['id']))
    except (KeyError, ValueError, TypeError):
      await websocket.send(
        encode_message(message(RequestTags.INVALID_REQUEST))
      )
      return

    subscription_id = uuid.uuid4()
    inbound = BatchInboundBroker()

    async def send(msg: ProtocolMessage) -> None:
      await websocket.send(encode_message(
        batch_event_message(subscription_id, msg)
      ))

    async def recv() -> ProtocolMessage:
      return await inbound.receive()

    async def run_session() -> None:
      try:
        if sub_type == 'review':
          run_descr = _get_or_create_run(run_id)
          if await _run_reviewer_session(run_descr, send = send, recv = recv):
            expelled.set()
          return

        slug = value.get('slug')
        if not isinstance(slug, str) or not is_valid_section_slug(slug):
          await send(message(RequestTags.INVALID_REQUEST))
          return

        subscribe, allowed_tags, reply_value_from = (
          _provider_subscribe_for_type(sub_type, slug)
        )

        await _run_provider_session(
          run_id,
          slug,
          send = send,
          recv = recv,
          subscribe = subscribe,
          allowed_reply_tags = allowed_tags,
          reply_value_from = reply_value_from
        )
      finally:
        subscriptions.pop(subscription_id, None)

    # Echo subscribe before starting the session so the client can bind
    # inbound routing before any batch.event traffic.
    echo = dict(value)
    echo['subscription_id'] = str(subscription_id)
    await websocket.send(encode_message(message(
      RequestTags.BATCH_SUBSCRIBE,
      value = echo
    )))

    task = asyncio.create_task(run_session())
    subscriptions[subscription_id] = _BatchSubscription(
      subscription_id,
      task,
      inbound
    )

  try:
    while True:
      try:
        request_tag, request_value, _section = decode_response(
          await receive()
        )
      except IncompatibleProtocolVersion:
        await websocket.send(encode_message(outdated_protocol_message()))
        return
      except ReviewerExpelled:
        await websocket.close(WS_CLOSE_ILLEGAL_ACCESS)
        return

      if request_tag == RequestTags.BATCH_SUBSCRIBE:
        await start_subscription(request_value)
      elif request_tag == RequestTags.BATCH_UNSUBSCRIBE:
        if not isinstance(request_value, dict):
          await websocket.send(
            encode_message(message(RequestTags.INVALID_REQUEST))
          )
          continue
        try:
          subscription_id = uuid.UUID(
            str(request_value['subscription_id'])
          )
        except (KeyError, ValueError, TypeError):
          await websocket.send(
            encode_message(message(RequestTags.INVALID_REQUEST))
          )
          continue
        if subscription_id not in subscriptions:
          await websocket.send(
            encode_message(message(RequestTags.INVALID_REQUEST))
          )
          continue
        await cancel_subscription(subscription_id)
        await websocket.send(encode_message(message(
          RequestTags.BATCH_UNSUBSCRIBE,
          value = { 'subscription_id': str(subscription_id) }
        )))
      elif request_tag == RequestTags.BATCH_EVENT:
        try:
          subscription_id, nested = unwrap_batch_event(request_value)
        except (KeyError, ValueError, TypeError):
          await websocket.send(
            encode_message(message(RequestTags.INVALID_REQUEST))
          )
          continue
        state = subscriptions.get(subscription_id)
        if state is None:
          await websocket.send(
            encode_message(message(RequestTags.INVALID_REQUEST))
          )
          continue
        await state.inbound.publish(nested)
      else:
        await websocket.send(
          encode_message(message(RequestTags.INVALID_REQUEST))
        )
  except asyncio.CancelledError:
    raise
  finally:
    for subscription_id in list(subscriptions.keys()):
      await cancel_subscription(subscription_id)
