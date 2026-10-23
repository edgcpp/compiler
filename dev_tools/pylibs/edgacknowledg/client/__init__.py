# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''AcknowlEDG client API: polymorphic reviews over batch or direct sockets.'''

import argparse
import asyncio
import os
import traceback
import uuid

from types import TracebackType

from typing import (
  Any,
  Dict,
  List,
  Optional,
  Tuple,
  Type,
)

try:
  from typing import Protocol
except ImportError:
  try:
    from typing_extensions import Protocol  # type: ignore
  except ImportError:
    Protocol = None  # type: ignore[misc,assignment]

from websockets import ConnectionClosed

from edgacknowledg import (
  IncompatibleProtocolVersion,
  ProtocolMessage,
  RequestTags,
  RichName,
  RoleOccupied,
  batch_event_message,
  decode_response,
  encode_message,
  message,
  ui_section_path_kind,
  unwrap_batch_event,
)
from edgacknowledg.client.shared import (
  BackgroundWorkerPool,
  Channel,
  IllegalFileAccessError,
  ReviewClientHandler,
  announce_section,
)
from edgutil import eprint, get_running_event_loop
from edgacknowledg.client.websocket import (
  ConnectionStatusListener,
  DEFAULT_OPEN_TIMEOUT_S,
  ResilientWebsocket,
  websocket_connect_kwargs,
)

try:
  from websockets.asyncio.client import ClientConnection  # type: ignore
except Exception:
  ClientConnection = Any  # type: ignore[misc,assignment]


# Re-export for callers that construct edit handlers via this package.
__all__ = [
  'AlreadyConnectedError',
  'BackgroundWorkerPool',
  'BatchClient',
  'BatchClientSession',
  'ClientError',
  'ConnectionStatusListener',
  'DEFAULT_API_HOST',
  'DEFAULT_API_PORT',
  'DEFAULT_OPEN_TIMEOUT_S',
  'DEFAULT_UI_HOST',
  'DEFAULT_UI_PORT',
  'DirectClient',
  'DirectClientSession',
  'NotConnectedError',
  'ReviewAlreadyRegisteredError',
  'ReviewClient',
  'SectionAlreadyRegisteredError',
  'SectionNotRegisteredError',
  'add_endpoint_arguments',
  'default_api_host',
  'default_api_port',
  'default_ui_host',
  'default_ui_port',
  'form_base_http_url',
  'form_section_http_url',
]


class ClientError(Exception):
  '''Base class for AcknowlEDG client API errors.'''


class AlreadyConnectedError(ClientError):
  '''Raised when ``connect()`` is called on an already-connected client.'''


class NotConnectedError(ClientError):
  '''Raised when an operation requires a connected client.'''


class ReviewAlreadyRegisteredError(ClientError):
  '''Raised when ``create_review()`` reuses an existing review id.'''

  def __init__(self, review_id: uuid.UUID) -> None:
    self.review_id = review_id
    super().__init__(f"review already registered: {review_id}")


class SectionAlreadyRegisteredError(ClientError):
  '''Raised when a section handler is registered or subscribed twice.'''

  def __init__(self,
               slug: str,
               subscription_type: Optional[str] = None) -> None:
    self.slug = slug
    self.subscription_type = subscription_type
    if subscription_type is None:
      detail = slug
    else:
      detail = f"{subscription_type}/{slug}"
    super().__init__(f"section already registered: {detail}")


class SectionNotRegisteredError(ClientError):
  '''Raised when removing a section that was never registered.'''

  def __init__(self, slug: str) -> None:
    self.slug = slug
    super().__init__(f"section not registered: {slug}")


def _form_batch_ws_url(api_host: str, api_port: int) -> str:
  '''Return the WebSocket URL for the ``/batch`` endpoint.'''
  if api_port == 80:
    return f"ws://{api_host}/batch"
  if api_port == 443:
    return f"wss://{api_host}/batch"
  return f"ws://{api_host}:{api_port}/batch"


def _form_base_ws_url(api_host: str, api_port: int) -> str:
  if api_port == 80:
    return f"ws://{api_host}"
  if api_port == 443:
    return f"wss://{api_host}"
  return f"ws://{api_host}:{api_port}"


def _form_direct_ws_url(api_host: str,
                        api_port: int,
                        review_id: uuid.UUID,
                        handler: ReviewClientHandler) -> str:
  '''Return the dedicated ``/source`` or ``/edit`` WebSocket URL.'''
  kind, role = handler.subscription_type.split('.', 1)
  role_path = 'source' if role == 'src' else 'edit'
  base = _form_base_ws_url(api_host, api_port)
  return f"{base}/{role_path}/{review_id}/{kind}/{handler.slug}"


ENV_API_HOST = 'EDG_ACKNOWLEDG_API_HOST'
ENV_API_PORT = 'EDG_ACKNOWLEDG_API_PORT'
ENV_UI_HOST = 'EDG_ACKNOWLEDG_UI_HOST'
ENV_UI_PORT = 'EDG_ACKNOWLEDG_UI_PORT'

DEFAULT_API_HOST = 'localhost'
DEFAULT_API_PORT = 2727
DEFAULT_UI_HOST = 'localhost'
DEFAULT_UI_PORT = 5173
# Used when the corresponding host is not localhost.
DEFAULT_REMOTE_API_PORT = 80
DEFAULT_REMOTE_UI_PORT = 80

# How long to wait for the server to acknowledge a batch subscribe or
# unsubscribe before giving up on it.
SUBSCRIBE_TIMEOUT_S = 30.0
UNSUBSCRIBE_TIMEOUT_S = 10.0


def default_api_host() -> str:
  '''Return ``EDG_ACKNOWLEDG_API_HOST``, or ``DEFAULT_API_HOST``.'''
  return os.environ.get(ENV_API_HOST, DEFAULT_API_HOST)


def default_api_port() -> int:
  '''Return ``EDG_ACKNOWLEDG_API_PORT`` as int, or a host-dependent default.

  Defaults to ``DEFAULT_API_PORT`` (2727) while the API host is still
  localhost; otherwise ``DEFAULT_REMOTE_API_PORT``.
  '''
  raw = os.environ.get(ENV_API_PORT)
  if raw is not None and raw != '':
    return int(raw)
  if default_api_host() == DEFAULT_API_HOST:
    return DEFAULT_API_PORT
  return DEFAULT_REMOTE_API_PORT


def default_ui_host() -> str:
  '''Return ``EDG_ACKNOWLEDG_UI_HOST``, or ``DEFAULT_UI_HOST``.'''
  return os.environ.get(ENV_UI_HOST, DEFAULT_UI_HOST)


def default_ui_port() -> int:
  '''Return ``EDG_ACKNOWLEDG_UI_PORT`` as int, or a host-dependent default.

  Defaults to ``DEFAULT_UI_PORT`` (5173) while the UI host is still
  localhost; otherwise ``DEFAULT_REMOTE_UI_PORT``.
  '''
  raw = os.environ.get(ENV_UI_PORT)
  if raw is not None and raw != '':
    return int(raw)
  if default_ui_host() == DEFAULT_UI_HOST:
    return DEFAULT_UI_PORT
  return DEFAULT_REMOTE_UI_PORT


def add_endpoint_arguments(parser: argparse.ArgumentParser) -> None:
  '''Add shared API/UI host and port CLI arguments.'''
  parser.add_argument(
    '--api-host',
    dest = 'api_hostname',
    default = default_api_host(),
    help = (
      f"the hostname to connect to (for the API, "
      f"default: {default_api_host()})"
    )
  )
  parser.add_argument(
    '--api-port',
    dest = 'api_port',
    default = default_api_port(),
    type = int,
    help = (
      f"the port to connect to (for the API, default: {default_api_port()})"
    )
  )
  parser.add_argument(
    '--ui-host',
    dest = 'ui_hostname',
    default = default_ui_host(),
    help = (
      f"the hostname to connect to (for the UI, "
      f"default: {default_ui_host()})"
    )
  )
  parser.add_argument(
    '--ui-port',
    dest = 'ui_port',
    default = default_ui_port(),
    type = int,
    help = (
      f"the port to connect to (for the UI, default: {default_ui_port()})"
    )
  )


def form_base_http_url(ui_host: str, ui_port: int) -> str:
  '''Return the base HTTP(S) URL for the AcknowlEDG UI.'''
  if ui_port == 80:
    return f"http://{ui_host}"
  if ui_port == 443:
    return f"https://{ui_host}"
  return f"http://{ui_host}:{ui_port}"


def form_section_http_url(ui_host: str,
                          ui_port: int,
                          review_id: uuid.UUID,
                          section_type: str,
                          slug: str) -> Optional[str]:
  '''Return the reviewer UI URL for a section, or ``None`` if it has no UI.'''
  path_kind = ui_section_path_kind(section_type)
  if path_kind is None:
    return None
  base = form_base_http_url(ui_host, ui_port)
  return f"{base}/review/{review_id}/{path_kind}/{slug}"


if Protocol is not None:
  class ReviewClient(Protocol):
    '''Public review API returned by ``create_review``.'''

    @property
    def review_id(self) -> uuid.UUID: ...

    @property
    def name(self) -> RichName: ...

    @property
    def timestamp(self) -> str: ...

    def sections(self) -> List[ReviewClientHandler]: ...

    async def add_section(self, handler: ReviewClientHandler) -> None: ...

    async def remove_section(self, handler: ReviewClientHandler) -> None: ...
else:
  ReviewClient = Any  # type: ignore[misc,assignment]


class _ClientReview:
  '''Shared review identity and section-handler registry.'''

  def __init__(self,
               review_id: uuid.UUID,
               name: RichName,
               timestamp: str) -> None:
    self._review_id = review_id
    self._name = name
    self._timestamp = timestamp
    self._handlers: List[ReviewClientHandler] = []

  @property
  def review_id(self) -> uuid.UUID:
    return self._review_id

  @property
  def name(self) -> RichName:
    return self._name

  @property
  def timestamp(self) -> str:
    return self._timestamp

  def register_handler(self, handler: ReviewClientHandler) -> None:
    '''Add ``handler`` to the registry; raise if already present.'''
    if handler in self._handlers:
      raise SectionAlreadyRegisteredError(handler.slug)
    self._handlers.append(handler)

  def unregister_handler(self, handler: ReviewClientHandler) -> None:
    '''Remove ``handler`` from the registry; raise if missing.'''
    if handler not in self._handlers:
      raise SectionNotRegisteredError(handler.slug)
    self._handlers.remove(handler)

  def discard_handler(self, handler: ReviewClientHandler) -> None:
    '''Remove ``handler`` from the registry if present.'''
    if handler in self._handlers:
      self._handlers.remove(handler)

  def sections(self) -> List[ReviewClientHandler]:
    return list(self._handlers)


class _SubscriptionChannel:
  '''Per-subscription send/recv facade over the multiplexed batch socket.'''

  def __init__(self,
               subscription_id: uuid.UUID,
               inbound: asyncio.Queue,
               client: 'BatchClientSession') -> None:
    self.subscription_id = subscription_id
    self._inbound = inbound
    self._client = client

  async def recv(self) -> ProtocolMessage:
    return await self._inbound.get()

  async def send(self, msg: ProtocolMessage) -> None:
    await self._client._send(
      batch_event_message(self.subscription_id, msg)
    )


class _DirectChannel:
  '''Send/recv facade that JSON-encodes protocol messages for a websocket.'''

  def __init__(self, websocket: ClientConnection) -> None:
    self._websocket = websocket

  async def recv(self) -> Any:
    return await self._websocket.recv()

  async def send(self, msg: ProtocolMessage) -> None:
    await self._websocket.send(encode_message(msg))


async def _send_reply_best_effort(channel: Channel,
                                  reply: ProtocolMessage) -> None:
  '''Send ``reply`` to a failed request, ignoring any failure but a lost
  connection (which the caller handles).
  '''
  try:
    await channel.send(reply)
  except (ConnectionClosed, NotConnectedError):
    raise
  except Exception:
    pass


async def _reply_to_failed_request(channel: Channel,
                                   handler: ReviewClientHandler) -> None:
  '''Answer a request whose handler raised, and log why.

  The section keeps serving afterwards: one bad request (which a reviewer
  can trigger) shouldn't take the whole section down.
  '''
  eprint(
    f"Request to section {handler.subscription_type}/{handler.slug} failed:"
  )
  traceback.print_exc()
  await _send_reply_best_effort(channel, message(RequestTags.INVALID_REQUEST))


async def _report_illegal_access(channel: Channel,
                                 handler: ReviewClientHandler,
                                 error: IllegalFileAccessError) -> None:
  '''Report the reviewer whose request asked for a file outside the
  section, so the server drops their connection. The section keeps serving
  everyone else.
  '''
  if error.reviewer_id is None:
    # Not traced to a reviewer; just a failed request.
    await _reply_to_failed_request(channel, handler)
  else:
    eprint(
      f"Reviewer {error.reviewer_id} of section "
      f"{handler.subscription_type}/{handler.slug} requested a file outside "
      f"it; having the server disconnect them: {error}"
    )
    await _send_reply_best_effort(channel, message(
      RequestTags.ILLEGAL_ACCESS,
      value = {'reviewer_id': error.reviewer_id}
    ))


async def _await_acknowledgement(future: asyncio.Future,
                                 timeout: float,
                                 action: str) -> Any:
  '''Wait for the server to acknowledge ``action``, up to ``timeout`` s.'''
  try:
    return await asyncio.wait_for(future, timeout)
  except asyncio.TimeoutError:
    raise ClientError(
      f"timed out waiting for the server to {action}"
    ) from None


class _BatchClientReview:
  '''A review served over a ``BatchClientSession`` ``/batch`` connection.'''

  def __init__(self,
               client: 'BatchClientSession',
               review_id: uuid.UUID,
               name: RichName,
               timestamp: str) -> None:
    self._client = client
    self._review = _ClientReview(review_id, name, timestamp)
    self._subscriptions: Dict[int, uuid.UUID] = {}
    self._subscription_tasks: Dict[uuid.UUID, asyncio.Task] = {}

  @property
  def review_id(self) -> uuid.UUID:
    return self._review.review_id

  @property
  def name(self) -> RichName:
    return self._review.name

  @property
  def timestamp(self) -> str:
    return self._review.timestamp

  def sections(self) -> List[ReviewClientHandler]:
    return self._review.sections()

  async def add_section(self, handler: ReviewClientHandler) -> None:
    '''Register handler and subscribe it on the batch socket.'''
    # Serialized with reconnect restores so a section added mid-restore is
    # subscribed exactly once.
    async with self._client._subscriptions_lock:
      self._review.register_handler(handler)
      try:
        await self.subscribe_section(handler)
      except Exception:
        self._review.discard_handler(handler)
        raise

  async def remove_section(self, handler: ReviewClientHandler) -> None:
    '''Unsubscribe, then unregister.'''
    async with self._client._subscriptions_lock:
      if handler not in self._review.sections():
        raise SectionNotRegisteredError(handler.slug)
      await self.unsubscribe_section(handler)
      self._review.discard_handler(handler)

  async def _run_handler_loop(self,
                              channel: _SubscriptionChannel,
                              handler: ReviewClientHandler) -> None:
    try:
      while True:
        try:
          await handler.handle_request(channel)
        except RoleOccupied:
          self._subscriptions.pop(id(handler), None)
          self._review.discard_handler(handler)
          return
        except (ConnectionClosed, NotConnectedError, asyncio.CancelledError):
          raise
        except IncompatibleProtocolVersion as ex:
          self._client.abort(ex)
          raise
        except IllegalFileAccessError as ex:
          await _report_illegal_access(channel, handler, ex)
        except Exception:
          await _reply_to_failed_request(channel, handler)
    finally:
      self._client._inbounds.pop(channel.subscription_id, None)
      self._subscription_tasks.pop(channel.subscription_id, None)

  async def _drop_local_subscriptions(self) -> None:
    '''Cancel handler tasks after the batch socket was lost.'''
    tasks = list(self._subscription_tasks.values())
    self._subscription_tasks.clear()
    self._subscriptions.clear()
    for task in tasks:
      task.cancel()
    for task in tasks:
      try:
        await task
      except asyncio.CancelledError:
        pass
      except Exception:
        pass

  async def _restore_subscriptions(self) -> None:
    '''Re-subscribe every registered section on a fresh batch socket.

    Sections already subscribed on this socket are skipped, and one section
    failing to subscribe doesn't keep the rest from being restored.
    '''
    for handler in self._review.sections():
      if id(handler) in self._subscriptions:
        continue
      try:
        await self.subscribe_section(handler)
      except (ConnectionClosed, NotConnectedError, asyncio.CancelledError):
        raise
      except Exception:
        eprint(
          f"Failed to restore section "
          f"{handler.subscription_type}/{handler.slug}:"
        )
        traceback.print_exc()

  async def subscribe_section(self, handler: ReviewClientHandler) -> None:
    handler_key = id(handler)
    if handler_key in self._subscriptions:
      raise SectionAlreadyRegisteredError(
        handler.slug,
        handler.subscription_type
      )

    subscription_id = await self._client._subscribe({
      'id': str(self.review_id),
      'type': handler.subscription_type,
      'slug': handler.slug,
    })
    channel = self._client._make_channel(subscription_id)
    await announce_section(channel, handler, self._review)
    loop = get_running_event_loop()
    task = loop.create_task(self._run_handler_loop(channel, handler))
    self._subscription_tasks[subscription_id] = task
    self._subscriptions[handler_key] = subscription_id

  async def unsubscribe_section(self, handler: ReviewClientHandler) -> None:
    subscription_id = self._subscriptions.pop(id(handler), None)
    if subscription_id is None:
      return

    task = self._subscription_tasks.pop(subscription_id, None)
    if task is not None:
      task.cancel()
      try:
        await task
      except asyncio.CancelledError:
        pass
      except Exception:
        pass
    self._client._inbounds.pop(subscription_id, None)
    try:
      await self._client._unsubscribe(subscription_id)
    except Exception:
      pass


class _FanInConnectionStatusListener:
  '''Aggregate per-socket status into one client-level listener.

  ``connected`` / ``disconnected`` fire when the live-socket count crosses
  zero; ``attempting_reconnect`` is forwarded whenever any socket reports it.
  '''

  def __init__(self,
               listener: Optional[ConnectionStatusListener]) -> None:
    self._listener = listener
    self._live = 0

  def connected(self) -> None:
    self._live += 1
    listener = self._listener
    if self._live == 1 and listener is not None:
      listener.connected()

  def attempting_reconnect(self) -> None:
    listener = self._listener
    if listener is not None:
      listener.attempting_reconnect()

  def disconnected(self) -> None:
    if self._live > 0:
      self._live -= 1
    listener = self._listener
    if self._live == 0 and listener is not None:
      listener.disconnected()


class _DirectClientReview:
  '''A review served over dedicated ``/source`` and ``/edit`` sockets.'''

  def __init__(self,
               client: 'DirectClientSession',
               review_id: uuid.UUID,
               name: RichName,
               timestamp: str) -> None:
    self._client = client
    self._review = _ClientReview(review_id, name, timestamp)
    self._section_tasks: Dict[int, asyncio.Task] = {}
    self._section_first_ready: Dict[int, asyncio.Event] = {}

  @property
  def review_id(self) -> uuid.UUID:
    return self._review.review_id

  @property
  def name(self) -> RichName:
    return self._review.name

  @property
  def timestamp(self) -> str:
    return self._review.timestamp

  def sections(self) -> List[ReviewClientHandler]:
    return self._review.sections()

  async def add_section(self, handler: ReviewClientHandler) -> None:
    '''Register handler and subscribe it on a dedicated socket.'''
    self._review.register_handler(handler)
    try:
      await self.subscribe_section(handler)
    except Exception:
      self._review.discard_handler(handler)
      raise

  async def remove_section(self, handler: ReviewClientHandler) -> None:
    '''Unsubscribe, then unregister.'''
    if handler not in self._review.sections():
      raise SectionNotRegisteredError(handler.slug)
    await self.unsubscribe_section(handler)
    self._review.discard_handler(handler)

  async def _run_handler_loop(self,
                              handler: ReviewClientHandler,
                              channel: _DirectChannel) -> None:
    while True:
      try:
        await handler.handle_request(channel)
      except (RoleOccupied, ConnectionClosed, asyncio.CancelledError,
              IncompatibleProtocolVersion):
        raise
      except IllegalFileAccessError as ex:
        await _report_illegal_access(channel, handler, ex)
      except Exception:
        await _reply_to_failed_request(channel, handler)

  def _signal_section_ready(self, handler_key: int) -> None:
    ready = self._section_first_ready.get(handler_key)
    if ready is not None and not ready.is_set():
      ready.set()

  async def _section_connection_loop(self,
                                     handler: ReviewClientHandler) -> None:
    '''Keep one section socket alive via ``ResilientWebsocket``.'''
    handler_key = id(handler)
    try:
      api_host, api_port = self._client._require_connected()
    except NotConnectedError:
      return

    url = _form_direct_ws_url(
      api_host,
      api_port,
      self.review_id,
      handler
    )
    resilient = ResilientWebsocket(
      url,
      websocket_connect_kwargs(self._client._api_key),
      open_timeout = self._client._open_timeout,
      status_listener = self._client._status_fan_in
    )
    try:
      async for websocket in resilient:
        if (self._client._closed or
            handler_key not in self._section_tasks):
          break

        channel = _DirectChannel(websocket)
        try:
          await announce_section(channel, handler, self._review)
          self._signal_section_ready(handler_key)
          await self._run_handler_loop(handler, channel)
        except RoleOccupied:
          self._section_tasks.pop(handler_key, None)
          self._review.discard_handler(handler)
          break
        except IncompatibleProtocolVersion as ex:
          self._client.abort(ex)
          break
        except ConnectionClosed:
          continue

        if (self._client._closed or
            handler_key not in self._section_tasks):
          break
    finally:
      self._signal_section_ready(handler_key)

  async def subscribe_section(self, handler: ReviewClientHandler) -> None:
    self._client._require_connected()
    handler_key = id(handler)
    if handler_key in self._section_tasks:
      raise SectionAlreadyRegisteredError(
        handler.slug,
        handler.subscription_type
      )

    ready = asyncio.Event()
    self._section_first_ready[handler_key] = ready
    loop = get_running_event_loop()
    # Register the key before starting so the loop treats this section as
    # wanted; unsubscribe pops the key to stop reconnecting.
    task = loop.create_task(self._section_connection_loop(handler))
    self._section_tasks[handler_key] = task
    try:
      await ready.wait()
      if handler_key not in self._section_tasks:
        raise NotConnectedError(
          f"section failed to connect: "
          f"{handler.subscription_type}/{handler.slug}"
        )
    finally:
      self._section_first_ready.pop(handler_key, None)

  async def unsubscribe_section(self, handler: ReviewClientHandler) -> None:
    handler_key = id(handler)
    task = self._section_tasks.pop(handler_key, None)
    ready = self._section_first_ready.pop(handler_key, None)
    if ready is not None and not ready.is_set():
      ready.set()
    if task is not None:
      task.cancel()
      try:
        await task
      except asyncio.CancelledError:
        pass
      except Exception:
        pass


class BatchClient:
  '''Async context manager for multiplexed ``/batch`` provider sessions.

  ``async with BatchClient(...) as session`` starts the connection and yields
  a ``BatchClientSession``. The ``with`` block waits until ``session.stop()``
  unless cancelled or an exception is raised.
  '''

  def __init__(self, *,
               api_host: str,
               api_port: int,
               api_key: Optional[str] = None,
               open_timeout: Optional[float] = DEFAULT_OPEN_TIMEOUT_S,
               status_listener: Optional[ConnectionStatusListener] = None
               ) -> None:
    self._api_host = api_host
    self._api_port = api_port
    self._api_key = api_key
    self._open_timeout = open_timeout
    self._status_listener = status_listener
    self._session: Optional['BatchClientSession'] = None

  async def __aenter__(self) -> 'BatchClientSession':
    self._session = BatchClientSession(
      status_listener = self._status_listener,
      api_host = self._api_host,
      api_port = self._api_port,
      api_key = self._api_key,
      open_timeout = self._open_timeout
    )
    await self._session.start()
    return self._session

  async def __aexit__(self,
                      exc_type: Optional[Type[BaseException]],
                      exc: Optional[BaseException],
                      tb: Optional[TracebackType]) -> bool:
    assert self._session is not None
    session = self._session
    try:
      if exc_type is None:
        await session.stopped()
    finally:
      await session.stop()
      self._session = None
    return False


class BatchClientSession:
  '''Live batch connection from ``async with BatchClient(...) as session``.

  Call ``stop()`` to end the managed context gracefully. Cancellation or an
  exception in the ``with`` body skips the wait and tears down immediately.
  '''

  def __init__(self, *,
               status_listener: Optional[ConnectionStatusListener],
               api_host: str,
               api_port: int,
               api_key: Optional[str],
               open_timeout: Optional[float]) -> None:
    self._status_listener = status_listener
    self._api_host = api_host
    self._api_port = api_port
    self._api_key = api_key
    self._open_timeout = open_timeout

    self._connection: Optional[ResilientWebsocket] = None
    self._connection_task: Optional[asyncio.Task] = None
    self._closed = False
    self._stopped: Optional[asyncio.Future] = None
    self._started = False

    self._pending_subscribe: Dict[
      Tuple[str, str, str],
      asyncio.Future
    ] = {}
    self._pending_unsubscribe: Dict[uuid.UUID, asyncio.Future] = {}
    self._inbounds: Dict[uuid.UUID, asyncio.Queue] = {}
    self._reviews: Dict[uuid.UUID, _BatchClientReview] = {}
    self._session_ready = asyncio.Event()
    # Held while sections are (re)subscribed or unsubscribed.
    self._subscriptions_lock = asyncio.Lock()

  def abort(self, error: BaseException) -> None:
    '''End the session; ``stopped()`` will raise ``error``.'''
    future = self._stopped
    if future is not None and not future.done():
      future.set_exception(error)

  async def stopped(self) -> None:
    '''Wait until ``stop()`` or ``abort()`` completes this session.'''
    future = self._stopped
    assert future is not None
    await future
    future.result()

  async def start(self) -> None:
    if self._started:
      raise AlreadyConnectedError()
    url = _form_batch_ws_url(self._api_host, self._api_port)
    connection = ResilientWebsocket(
      url,
      websocket_connect_kwargs(self._api_key),
      open_timeout = self._open_timeout,
      status_listener = self._status_listener
    )
    self._connection = connection
    self._closed = False
    self._stopped = get_running_event_loop().create_future()
    self._session_ready.clear()
    self._started = True
    self._connection_task = get_running_event_loop().create_task(
      self._connection_loop(connection)
    )
    try:
      await connection.wait_connected()
      await self._session_ready.wait()
    except ConnectionClosed as exc:
      await self.stop()
      raise NotConnectedError('failed to establish batch connection') from exc

  async def stop(self) -> None:
    '''Stop reconnecting, tear down reviews, and complete ``stopped()``.'''
    self._closed = True
    future = self._stopped
    if future is not None and not future.done():
      future.set_result(None)

    for review in list(self._reviews.values()):
      try:
        await self.remove_review(review)
      except Exception:
        pass

    connection = self._connection
    self._connection = None
    if connection is not None:
      await connection.stop()

    task = self._connection_task
    self._connection_task = None
    if task is not None:
      task.cancel()
      try:
        await task
      except asyncio.CancelledError:
        pass
      except Exception:
        pass

    self._fail_pending(NotConnectedError('batch client closed'))
    self._inbounds.clear()
    self._session_ready.clear()
    self._started = False

  def _fail_pending(self, error: BaseException) -> None:
    for future in list(self._pending_subscribe.values()):
      if not future.done():
        future.set_exception(error)
    self._pending_subscribe.clear()
    for future in list(self._pending_unsubscribe.values()):
      if not future.done():
        future.set_exception(error)
    self._pending_unsubscribe.clear()

  async def _drop_subscriptions(self) -> None:
    '''Tear down local subscription state after the batch socket drops.'''
    self._session_ready.clear()
    for review in list(self._reviews.values()):
      await review._drop_local_subscriptions()
    self._fail_pending(NotConnectedError('batch connection lost'))
    self._inbounds.clear()

  async def _restore_subscriptions(self) -> None:
    '''Re-establish every review section on a fresh batch socket.'''
    async with self._subscriptions_lock:
      for review in list(self._reviews.values()):
        await review._restore_subscriptions()

  async def _connection_loop(self,
                             connection: ResilientWebsocket) -> None:
    '''Reconnect the batch socket; restore sections on each session.'''
    async for websocket in connection:
      if self._closed:
        break
      try:
        await self._run_session(websocket)
      except ConnectionClosed:
        continue
      except IncompatibleProtocolVersion:
        break

  async def _run_session(self, websocket: ClientConnection) -> None:
    '''One live batch connection: restore sections, then demux until close.'''
    try:
      # Recv must run concurrently so subscribe replies can complete.
      recv_task = get_running_event_loop().create_task(
        self._recv_loop(websocket)
      )
      try:
        try:
          await self._restore_subscriptions()
        finally:
          # Unblock waiters even if restore partially failed.
          self._session_ready.set()
        await recv_task
      finally:
        if not recv_task.done():
          recv_task.cancel()
          try:
            await recv_task
          except asyncio.CancelledError:
            pass
          except Exception:
            pass
    finally:
      await self._drop_subscriptions()

  async def _send(self, frame: ProtocolMessage) -> None:
    connection = self._connection
    if connection is None or self._closed:
      raise NotConnectedError()
    try:
      await connection.send(encode_message(frame))
    except ConnectionClosed as exc:
      raise NotConnectedError('batch connection lost') from exc

  async def _recv_loop(self, websocket: ClientConnection) -> None:
    try:
      while True:
        tag, value, _section = decode_response(await websocket.recv())

        if tag == RequestTags.BATCH_SUBSCRIBE:
          if not isinstance(value, dict):
            continue
          try:
            subscription_id = uuid.UUID(str(value['subscription_id']))
          except (KeyError, ValueError, TypeError):
            continue
          # Bind inbound routing before completing the subscribe future so
          # early batch.event frames are not dropped.
          if subscription_id not in self._inbounds:
            self._inbounds[subscription_id] = asyncio.Queue()
          key = (
            str(value.get('id')),
            str(value.get('type')),
            str(value.get('slug', '')),
          )
          future = self._pending_subscribe.pop(key, None)
          if future is not None and not future.done():
            future.set_result(value)

        elif tag == RequestTags.BATCH_UNSUBSCRIBE:
          if not isinstance(value, dict):
            continue
          try:
            subscription_id = uuid.UUID(str(value['subscription_id']))
          except (KeyError, ValueError, TypeError):
            continue
          future = self._pending_unsubscribe.pop(subscription_id, None)
          if future is not None and not future.done():
            future.set_result(value)

        elif tag == RequestTags.BATCH_EVENT:
          try:
            subscription_id, nested = unwrap_batch_event(value)
          except (KeyError, ValueError, TypeError):
            continue
          inbound = self._inbounds.get(subscription_id)
          if inbound is not None:
            await inbound.put(nested)
    except asyncio.CancelledError:
      raise
    except ConnectionClosed:
      raise
    except IncompatibleProtocolVersion as ex:
      self._fail_pending(ex)
      self.abort(ex)
      raise
    except Exception:
      if not self._closed:
        raise

  async def _subscribe(self, payload: Dict[str, Any]) -> uuid.UUID:
    key = (
      str(payload['id']),
      str(payload['type']),
      str(payload.get('slug', '')),
    )
    loop = get_running_event_loop()
    future: asyncio.Future = loop.create_future()
    self._pending_subscribe[key] = future
    try:
      await self._send(message(RequestTags.BATCH_SUBSCRIBE, value = payload))
      echo = await _await_acknowledgement(
        future,
        SUBSCRIBE_TIMEOUT_S,
        f"subscribe {key[1]}/{key[2]}"
      )
    except Exception:
      self._pending_subscribe.pop(key, None)
      if not future.done():
        future.cancel()
      raise
    subscription_id = uuid.UUID(str(echo['subscription_id']))
    return subscription_id

  async def _unsubscribe(self, subscription_id: uuid.UUID) -> None:
    loop = get_running_event_loop()
    future: asyncio.Future = loop.create_future()
    self._pending_unsubscribe[subscription_id] = future
    try:
      await self._send(message(
        RequestTags.BATCH_UNSUBSCRIBE,
        value = { 'subscription_id': str(subscription_id) }
      ))
      await _await_acknowledgement(
        future,
        UNSUBSCRIBE_TIMEOUT_S,
        f"unsubscribe {subscription_id}"
      )
    except Exception:
      self._pending_unsubscribe.pop(subscription_id, None)
      if not future.done():
        future.cancel()
      raise

  def _make_channel(self, subscription_id: uuid.UUID) -> _SubscriptionChannel:
    inbound = self._inbounds.setdefault(subscription_id, asyncio.Queue())
    return _SubscriptionChannel(subscription_id, inbound, self)

  async def create_review(self,
                          review_id: uuid.UUID,
                          name: RichName,
                          timestamp: str) -> ReviewClient:
    '''Construct a review served over this session's batch transport.'''
    if review_id in self._reviews:
      raise ReviewAlreadyRegisteredError(review_id)
    review = _BatchClientReview(self, review_id, name, timestamp)
    self._reviews[review_id] = review
    return review

  async def remove_review(self, review: ReviewClient) -> None:
    if self._reviews.pop(review.review_id, None) is None:
      return

    for handler in list(review.sections()):
      try:
        await review.remove_section(handler)
      except Exception:
        pass


class DirectClient:
  '''Async context manager for dedicated ``/source`` and ``/edit`` sessions.

  ``async with DirectClient(...) as session`` yields a ``DirectClientSession``.
  The ``with`` block waits until ``session.stop()`` unless cancelled or an
  exception is raised.
  '''

  def __init__(self, *,
               api_host: str,
               api_port: int,
               api_key: Optional[str] = None,
               open_timeout: Optional[float] = DEFAULT_OPEN_TIMEOUT_S,
               status_listener: Optional[ConnectionStatusListener] = None
               ) -> None:
    self._api_host = api_host
    self._api_port = api_port
    self._api_key = api_key
    self._open_timeout = open_timeout
    self._status_listener = status_listener
    self._session: Optional['DirectClientSession'] = None

  async def __aenter__(self) -> 'DirectClientSession':
    self._session = DirectClientSession(
      status_listener = self._status_listener,
      api_host = self._api_host,
      api_port = self._api_port,
      api_key = self._api_key,
      open_timeout = self._open_timeout
    )
    await self._session.start()
    return self._session

  async def __aexit__(self,
                      exc_type: Optional[Type[BaseException]],
                      exc: Optional[BaseException],
                      tb: Optional[TracebackType]) -> bool:
    assert self._session is not None
    session = self._session
    try:
      if exc_type is None:
        await session.stopped()
    finally:
      await session.stop()
      self._session = None
    return False


class DirectClientSession:
  '''Live direct-socket session from ``async with DirectClient(...) as
  session``.

  Call ``stop()`` to end the managed context gracefully. Cancellation or an
  exception in the ``with`` body skips the wait and tears down immediately.
  '''

  def __init__(self, *,
               status_listener: Optional[ConnectionStatusListener],
               api_host: str,
               api_port: int,
               api_key: Optional[str],
               open_timeout: Optional[float]) -> None:
    self._api_host = api_host
    self._api_port = api_port
    self._api_key = api_key
    self._open_timeout = open_timeout
    self._started = False
    self._closed = False
    self._stopped: Optional[asyncio.Future] = None
    self._status_listener = status_listener
    self._status_fan_in = _FanInConnectionStatusListener(status_listener)
    self._reviews: Dict[uuid.UUID, _DirectClientReview] = {}

  def abort(self, error: BaseException) -> None:
    '''End the session; ``stopped()`` will raise ``error``.'''
    future = self._stopped
    if future is not None and not future.done():
      future.set_exception(error)

  async def stopped(self) -> None:
    '''Wait until ``stop()`` or ``abort()`` completes this session.'''
    future = self._stopped
    assert future is not None
    await future
    future.result()

  async def start(self) -> None:
    if self._started:
      raise AlreadyConnectedError()
    self._started = True
    self._closed = False
    self._stopped = get_running_event_loop().create_future()

  async def stop(self) -> None:
    '''Tear down reviews and complete ``stopped()``.'''
    self._closed = True
    future = self._stopped
    if future is not None and not future.done():
      future.set_result(None)
    for review in list(self._reviews.values()):
      try:
        await self.remove_review(review)
      except Exception:
        pass
    self._started = False

  def _require_connected(self) -> Tuple[str, int]:
    if not self._started or self._closed:
      raise NotConnectedError()
    return self._api_host, self._api_port

  async def create_review(self,
                          review_id: uuid.UUID,
                          name: RichName,
                          timestamp: str) -> ReviewClient:
    '''Construct a review served over this session's direct sockets.'''
    self._require_connected()
    if review_id in self._reviews:
      raise ReviewAlreadyRegisteredError(review_id)
    review = _DirectClientReview(self, review_id, name, timestamp)
    self._reviews[review_id] = review
    return review

  async def remove_review(self, review: ReviewClient) -> None:
    if self._reviews.pop(review.review_id, None) is None:
      return

    for handler in list(review.sections()):
      try:
        await review.remove_section(handler)
      except Exception:
        pass
