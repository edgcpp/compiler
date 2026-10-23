# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Cancelling AcknowlEDG client requests undoes them, as failing does;
losing the connection fails them.
'''

import asyncio
import uuid

from typing import Any

import pytest

from websockets.exceptions import ConnectionClosedError

from edgacknowledg import (
  RequestTags,
  decode_response,
  encode_message,
  message,
)
from edgacknowledg.client import (
  BatchClientSession,
  NotConnectedError,
  _BatchClientReview,
  _DirectClientReview,
)
from edgacknowledg.client.websocket import ResilientWebsocket


class _Unanswered:
  '''A batch connection the server never answers on.'''

  def __init__(self) -> None:
    self.sent: list[Any] = []
    self.sent_if_connected: list[Any] = []

  async def send(self, data: Any) -> None:
    self.sent.append(data)

  async def send_if_connected(self, data: Any) -> None:
    self.sent_if_connected.append(data)


class _Handler:
  subscription_type = 'test.src'
  slug = 'test-changes'
  section_name = 'Test - Changes'


def _session() -> BatchClientSession:
  session = BatchClientSession(
    status_listener = None,
    api_host = '127.0.0.1',
    api_port = 1,
    api_key = None,
    open_timeout = None
  )
  session._connection = _Unanswered()  # type: ignore[assignment]
  session._session_ready.set()
  return session


def _released(session: BatchClientSession) -> list[str]:
  '''The subscription ids ``session`` had the server end, unasked.'''
  connection = session._connection
  assert isinstance(connection, _Unanswered)
  released = []
  for data in connection.sent_if_connected:
    tag, value, _section = decode_response(data)
    assert tag == RequestTags.BATCH_UNSUBSCRIBE
    released.append(value['subscription_id'])
  return released


class _Replying:
  '''A batch socket that sends ``replies``, then nothing.'''

  def __init__(self, *replies: Any) -> None:
    self._replies = list(replies)

  async def recv(self) -> Any:
    if not self._replies:
      await asyncio.Event().wait()
    return self._replies.pop(0)


def _subscribed(payload: dict[str, Any], subscription_id: str) -> str:
  '''The server's answer to subscribing ``payload``.'''
  return encode_message(message(
    RequestTags.BATCH_SUBSCRIBE,
    value = {**payload, 'subscription_id': subscription_id}
  ))


async def _cancel_once_sent(session: BatchClientSession,
                            request: 'asyncio.Task[Any]') -> None:
  connection = session._connection
  assert isinstance(connection, _Unanswered)
  while len(connection.sent) == 0:
    await asyncio.sleep(0.01)
  request.cancel()
  await asyncio.gather(request, return_exceptions = True)
  assert request.cancelled()


async def test_cancelled_subscribes_are_not_awaited() -> None:
  session = _session()
  subscribing = asyncio.create_task(session._subscribe({
    'id': str(uuid.uuid4()), 'type': 'test.src', 'slug': 'test-changes',
  }))
  await _cancel_once_sent(session, subscribing)
  assert session._pending_subscribe == {}

  unsubscribing = asyncio.create_task(session._unsubscribe(uuid.uuid4()))
  session._connection = _Unanswered()  # type: ignore[assignment]
  await _cancel_once_sent(session, unsubscribing)
  assert session._pending_unsubscribe == {}


async def test_cancelled_batch_sections_are_not_added() -> None:
  session = _session()
  review = _BatchClientReview(session, uuid.uuid4(), [], '')
  adding = asyncio.create_task(review.add_section(_Handler()))  # type: ignore
  await _cancel_once_sent(session, adding)
  # Not left to be subscribed on reconnecting.
  assert review.sections() == []
  assert session._pending_subscribe == {}


class _DirectSession:
  def _require_connected(self) -> tuple[str, int]:
    return '127.0.0.1', 1


async def test_direct_sections_are_added_without_connecting() -> None:
  review = _DirectClientReview(
    _DirectSession(),  # type: ignore[arg-type]
    uuid.uuid4(),
    [],
    ''
  )
  connecting = asyncio.Event()
  stopped = asyncio.Event()

  async def connection_loop(handler: Any) -> None:
    connecting.set()
    try:
      await asyncio.Event().wait()
    finally:
      stopped.set()

  review._section_connection_loop = connection_loop  # type: ignore
  handler = _Handler()
  # Nothing to cancel it in: it doesn't wait for the section to connect.
  await asyncio.wait_for(review.add_section(handler), 5)  # type: ignore
  assert review.sections() == [handler]
  await asyncio.wait_for(connecting.wait(), 5)

  await review.remove_section(handler)  # type: ignore[arg-type]
  assert review.sections() == []
  assert review._section_tasks == {}
  assert stopped.is_set()


async def test_late_subscribes_are_released() -> None:
  session = _session()
  payload = {'id': str(uuid.uuid4()), 'type': 'test.src', 'slug': 'test'}
  subscribing = asyncio.create_task(session._subscribe(payload))
  await _cancel_once_sent(session, subscribing)

  # Answered after it was given up on.
  subscription_id = str(uuid.uuid4())
  receiving = asyncio.create_task(session._recv_loop(
    _Replying(_subscribed(payload, subscription_id))  # type: ignore
  ))
  async def released() -> None:
    while _released(session) == []:
      await asyncio.sleep(0.01)

  try:
    await asyncio.wait_for(released(), 5)
  except TimeoutError:
    pass
  receiving.cancel()
  await asyncio.gather(receiving, return_exceptions = True)
  assert _released(session) == [subscription_id]
  assert session._inbounds == {}


async def test_subscribes_answered_as_cancelled_are_released() -> None:
  session = _session()
  payload = {'id': str(uuid.uuid4()), 'type': 'test.src', 'slug': 'test'}
  subscribing = asyncio.create_task(session._subscribe(payload))
  connection = session._connection
  assert isinstance(connection, _Unanswered)
  while len(connection.sent) == 0:
    await asyncio.sleep(0.01)

  # Answered, then cancelled before it resumes to take the answer.
  subscription_id = str(uuid.uuid4())
  (future,) = session._pending_subscribe.values()
  future.set_result({**payload, 'subscription_id': subscription_id})
  subscribing.cancel()
  await asyncio.gather(subscribing, return_exceptions = True)
  assert subscribing.cancelled()
  assert _released(session) == [subscription_id]


class _Stalled(_Unanswered):
  '''A batch connection whose sends never finish (its socket full).'''

  async def send(self, data: Any) -> None:
    self.sent.append(data)
    await asyncio.Event().wait()


async def test_sections_cancelled_once_subscribed_are_released() -> None:
  session = _session()
  session._connection = _Stalled()  # type: ignore[assignment]
  review = _BatchClientReview(session, uuid.uuid4(), [], '')
  subscription_id = uuid.uuid4()

  async def subscribe(payload: dict[str, Any]) -> uuid.UUID:
    return subscription_id

  session._subscribe = subscribe  # type: ignore[method-assign]
  # Cancelled while announcing the section, after it was subscribed.
  adding = asyncio.create_task(review.add_section(_Handler()))  # type: ignore
  await _cancel_once_sent(session, adding)
  assert review.sections() == []
  assert _released(session) == [str(subscription_id)]
  assert session._inbounds == {}


class _Lost:
  '''A batch socket lost once ``connection`` has sent something.'''

  def __init__(self, connection: _Unanswered) -> None:
    self._connection = connection

  async def recv(self) -> Any:
    while len(self._connection.sent) == 0:
      await asyncio.sleep(0.01)
    raise ConnectionClosedError(None, None)


async def test_subscribes_wait_until_the_connection_goes() -> None:
  session = _session()
  connection = session._connection
  assert isinstance(connection, _Unanswered)
  subscribing = asyncio.create_task(session._subscribe({
    'id': str(uuid.uuid4()), 'type': 'test.src', 'slug': 'test-changes',
  }))
  # However long the server takes: no timeout...
  await asyncio.sleep(0.2)
  assert not subscribing.done()

  # ...but losing the connection fails it.
  receiving = asyncio.create_task(session._recv_loop(
    _Lost(connection)  # type: ignore[arg-type]
  ))
  with pytest.raises(NotConnectedError):
    await asyncio.wait_for(subscribing, 5)
  await asyncio.gather(receiving, return_exceptions = True)
  assert session._pending_subscribe == {}


async def test_restoring_ends_when_the_connection_goes() -> None:
  session = _session()
  connection = session._connection
  assert isinstance(connection, _Unanswered)
  review = await session.create_review(uuid.uuid4(), [], '')
  handler = _Handler()
  # Registered while not connected: restored on connecting.
  review._review.register_handler(handler)  # type: ignore
  session._session_ready.clear()

  # Lost while restoring it: the session ends, to reconnect, rather than
  # waiting for an answer that can't come (and says why).
  with pytest.raises(ConnectionClosedError):
    await asyncio.wait_for(
      session._run_session(_Lost(connection)),  # type: ignore[arg-type]
      5
    )
  assert review.sections() == [handler]
  assert session._pending_subscribe == {}


async def test_releases_are_dropped_while_disconnected() -> None:
  connection = ResilientWebsocket('ws://127.0.0.1:1', {})
  # Not left waiting for a connection: the server ended what it would end.
  await asyncio.wait_for(connection.send_if_connected('{}'), 5)
