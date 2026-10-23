# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import json
import re
import sys
import uuid

from collections.abc import Mapping
from datetime import datetime, timezone
from typing import Any, Dict, FrozenSet, List, Optional, Tuple, Union
from urllib.parse import urlsplit

# If you change these values, make sure to update the JavaScript client code.
#
# Tag namespaces:
#   test.src.*   — test source provider / reviewer test requests
#   bench.src.*  — benchmark source provider / reviewer bench requests
#   test.edit.*  — test editor provider / reviewer edit requests
#   bench.edit.* — benchmark editor provider / reviewer edit requests
#
# This class does not use the StrEnum type because of incompatibilities with
# Python 3.6.
class RequestTags:
  INVALID_REQUEST = 'invalid'
  OUTDATED_PROTOCOL = 'outdated-proto'
  ROLE_OCCUPIED = 'role-occupied'
  # A reviewer asked a provider for a file it may not read: provider ->
  # server (naming the reviewer), which closes that reviewer's connection.
  ILLEGAL_ACCESS = 'illegal-access'
  META_SECTION_NAME = 'meta.section.name'
  META_REVIEW_NAME = 'meta.review.name'
  META_REVIEW_TIMESTAMP = 'meta.review.timestamp'
  LIST_REVIEWS = 'list-reviews'
  LIST_REVIEWS_PATCH = 'list-reviews-patch'
  CONNECTION_STATE = 'review.connection-state'

  # Presence errors (not part of provider reply namespaces).
  TEST_MISSING_SOURCE = 'test.missing-src'
  TEST_MISSING_EDITOR = 'test.missing-edit'
  BENCH_MISSING_SOURCE = 'bench.missing-src'
  BENCH_MISSING_EDITOR = 'bench.missing-edit'

  # Test source namespace.
  TEST_LIST_STATUSES = 'test.src.list-statuses'
  TEST_LIST_ASSOC_FILES = 'test.src.list-assoc-files'
  TEST_SHOW_DIFF = 'test.src.show-diff'
  TEST_SHOW_CURR_DIFF = 'test.src.show-diff-curr'
  TEST_SHOW_RAW_OUTPUT = 'test.src.show-output'
  TEST_SHOW_SOURCE = 'test.src.show-src'

  # Test editor namespace.
  TEST_UPDATE_DIFF = 'test.edit.update-diff'

  # Benchmark source namespace.
  BENCH_LIST_STATUSES = 'bench.src.list-statuses'
  BENCH_SHOW_ANNOTATE = 'bench.src.show-annotate'

  # Benchmark editor namespace.
  BENCH_UPDATE_BASELINE = 'bench.edit.update-baseline'

  # Batch multiplexing.
  BATCH_SUBSCRIBE = 'batch.subscribe'
  BATCH_UNSUBSCRIBE = 'batch.unsubscribe'
  BATCH_EVENT = 'batch.event'

TAG_PREFIX_TEST_SRC = 'test.src'
TAG_PREFIX_BENCH_SRC = 'bench.src'
TAG_PREFIX_TEST_EDIT = 'test.edit'
TAG_PREFIX_BENCH_EDIT = 'bench.edit'

BATCH_SUBSCRIPTION_TYPES = frozenset({
  'test.src',
  'test.edit',
  'bench.src',
  'bench.edit',
  'review'
})

# Section types that have a dedicated reviewer UI route, mapped to the path
# segment used under ``/review/{id}/...``. Edit subscriptions share the UI
# page of their source counterpart.
UI_SECTION_PATH_KINDS = {
  TAG_PREFIX_TEST_SRC: 'tests',
  TAG_PREFIX_BENCH_SRC: 'benchmarks',
}

SECTION_SLUG_PATTERN = re.compile(r'^[a-z0-9]+(?:-[a-z0-9]+)*$')

def ui_section_path_kind(section_type: str) -> Optional[str]:
  '''Return the reviewer UI path kind for ``section_type``, or ``None``.

  Recognized UI section types map to ``tests`` or ``benchmarks``.
  '''
  return UI_SECTION_PATH_KINDS.get(section_type)

def is_ui_section_type(section_type: str) -> bool:
  '''Return whether ``section_type`` has a dedicated reviewer UI route.'''
  return section_type in UI_SECTION_PATH_KINDS

def request_tags_with_prefix(prefix: str) -> FrozenSet[str]:
  '''Return all RequestTags values that start with ``prefix``.'''
  return frozenset(
    value
    for key, value in vars(RequestTags).items()
    if (not key.startswith('_') and
        isinstance(value, str) and
        value.startswith(f"{prefix}."))
  )

ALLOWED_TEST_SOURCE_TAGS = request_tags_with_prefix(TAG_PREFIX_TEST_SRC)
ALLOWED_BENCH_SOURCE_TAGS = request_tags_with_prefix(TAG_PREFIX_BENCH_SRC)
ALLOWED_TEST_EDIT_TAGS = request_tags_with_prefix(TAG_PREFIX_TEST_EDIT)
ALLOWED_BENCH_EDIT_TAGS = request_tags_with_prefix(TAG_PREFIX_BENCH_EDIT)
ALLOWED_REVIEWER_REQUEST_TAGS = (
  ALLOWED_TEST_SOURCE_TAGS |
  ALLOWED_BENCH_SOURCE_TAGS |
  ALLOWED_TEST_EDIT_TAGS |
  ALLOWED_BENCH_EDIT_TAGS
)

PROTOCOL_REVISION = 19

# 10MiB
MAX_MESSAGE_SIZE = 10485760
# 8MiB
MAX_DISPLAY_CONTENT_SIZE = 8388608

WebSocketsMessageType = Union[str, bytes]
ProtocolMessage = Dict[str, Any]
DecodableMessage = Union[WebSocketsMessageType, Mapping[str, Any]]

def review_display_name(name: Optional[str] = None) -> str:
  '''Return the review-list title.

  ``name`` is used as-is when provided so callers can supply a fully custom
  title. Otherwise the default ``Review opened by {render_user_name()}``
  is returned.
  '''
  if name:
    return name
  from edgdocker import render_user_name
  return f"Review opened by {render_user_name()}"

# A review's name is rich text: segments composed in order, each either
# {'type': 'text', 'text': ...} or {'type': 'link', 'url': ..., 'text': ...}
# (a link's text is optional; its URL is shown without it). See PROTOCOL.md.
RichName = List[Dict[str, str]]

MAX_RICH_NAME_SEGMENTS = 32
# Characters, across every segment's text and URL.
MAX_RICH_NAME_LENGTH = 4096

_RICH_SEGMENT_FIELDS = {
  'text': ({'type', 'text'}, {'type', 'text'}),
  'link': ({'type', 'url'}, {'type', 'url', 'text'}),
}

def rich_text(text: str) -> Dict[str, str]:
  return {'type': 'text', 'text': text}

def rich_link(url: str, text: Optional[str] = None) -> Dict[str, str]:
  segment = {'type': 'link', 'url': url}
  if text is not None:
    segment['text'] = text
  return segment

def _is_web_url(url: str) -> bool:
  try:
    parts = urlsplit(url)
  except ValueError:
    return False
  return parts.scheme in ('http', 'https') and parts.netloc != ''

def is_valid_rich_name(value: Any) -> bool:
  '''Return whether ``value`` is a well-formed rich name: a non-empty list
  of known segments with only their fields, all non-empty strings, and
  links only to http(s) URLs, within the size limits.
  '''
  if not isinstance(value, list):
    return False
  if len(value) == 0 or len(value) > MAX_RICH_NAME_SEGMENTS:
    return False
  length = 0
  for segment in value:
    if not isinstance(segment, dict):
      return False
    segment_type = segment.get('type')
    if segment_type not in _RICH_SEGMENT_FIELDS:
      return False
    required, allowed = _RICH_SEGMENT_FIELDS[segment_type]
    keys = set(segment)
    if not (required <= keys <= allowed):
      return False
    for key, field in segment.items():
      if not isinstance(field, str) or field == '':
        return False
      if key != 'type':
        length += len(field)
    if segment['type'] == 'link' and not _is_web_url(segment['url']):
      return False
  return length <= MAX_RICH_NAME_LENGTH

def rich_name_text(name: RichName) -> str:
  '''Return ``name`` as plain text (e.g. for terminal output).'''
  return ''.join(
    segment['text'] if segment.get('text') else segment.get('url', '')
    for segment in name
  )

def utc_now_iso() -> str:
  '''Return the current UTC time as an ISO-8601 timestamp.'''
  return datetime.now(timezone.utc).isoformat()

def is_iso_timestamp(value: str) -> bool:
  '''Return whether ``value`` is a parseable ISO-8601 datetime string.'''
  try:
    if sys.version_info >= (3, 7):
      datetime.fromisoformat(value)
    else:
      # Python 3.6: fromisoformat is unavailable. Accept the shapes produced
      # by utc_now_iso() (isoformat with optional fractional seconds + offset).
      normalized = value
      if normalized.endswith('Z'):
        normalized = normalized[:-1] + '+00:00'
      # Split off a trailing +HH:MM / -HH:MM offset when present.
      if (len(normalized) >= 6 and normalized[-6] in '+-' and
          normalized[-3] == ':'):
        normalized = normalized[:-6]
      normalized = normalized.replace(' ', 'T', 1)
      if '.' in normalized:
        datetime.strptime(normalized, '%Y-%m-%dT%H:%M:%S.%f')
      else:
        datetime.strptime(normalized, '%Y-%m-%dT%H:%M:%S')
  except ValueError:
    return False
  return True

def section_slug(display_name: str) -> str:
  '''Derive a URL slug from a section display name.

  Lowercases the name, rewrites ``c++`` to ``cpp`` so ``C`` and ``C++`` do
  not collapse to the same slug, and replaces each run of characters
  outside [a-z0-9] with a single dash.
  '''
  lowered = display_name.lower().replace('c++', 'cpp')
  slug = re.sub(r'[^a-z0-9]+', '-', lowered).strip('-')
  return slug

def is_valid_section_slug(slug: str) -> bool:
  return bool(SECTION_SLUG_PATTERN.fullmatch(slug))

class Serializer(json.JSONEncoder):
  def default(self, obj: Any) -> str:
    if isinstance(obj, uuid.UUID):
      return str(obj)
    return json.JSONEncoder.default(self, obj)

def message(tag: str, *,
            value: Any = None,
            section: Optional[str] = None) -> ProtocolMessage:
  '''Build a protocol message object (revision is added by ``encode_message``).

  ``section`` (wire field ``s``) is the focused review section slug, used by
  reviewer requests to select among multiple source/edit providers.
  '''
  payload: ProtocolMessage = {
    't': tag,
    'v': value,
  }
  if section is not None:
    payload['s'] = section
  return payload

def encode_message(payload: Mapping[str, Any]) -> str:
  '''Serialize a protocol message, attaching ``PROTOCOL_REVISION``.'''
  envelope = dict(payload)
  envelope['r'] = PROTOCOL_REVISION
  return json.dumps(envelope, cls = Serializer)

def outdated_protocol_message() -> ProtocolMessage:
  # This is a simple message that's delivered to trigger outdated protocol
  # processing.
  return message(RequestTags.OUTDATED_PROTOCOL)

def role_occupied_message() -> ProtocolMessage:
  return message(RequestTags.ROLE_OCCUPIED)

def batch_event_message(subscription_id: uuid.UUID,
                        nested_message: ProtocolMessage) -> ProtocolMessage:
  '''Wrap a nested protocol message in ``batch.event``.

  The nested message must not carry ``r``; revision lives on the outer
  envelope produced by ``encode_message``.
  '''
  return message(
    RequestTags.BATCH_EVENT,
    value = {
      'subscription_id': subscription_id,
      'message': nested_message
    }
  )

def unwrap_batch_event(
    value: Any
    ) -> Tuple[uuid.UUID, ProtocolMessage]:
  '''Return ``(subscription_id, nested_message)`` from a ``batch.event`` value.

  The nested message omits ``r``; pass it to ``decode_response``, which treats
  a missing revision as the current protocol revision.
  '''
  if not isinstance(value, dict):
    raise ValueError('batch.event value must be an object')
  subscription_id = uuid.UUID(str(value['subscription_id']))
  nested = value.get('message')
  if not isinstance(nested, dict):
    raise ValueError('batch.event message must be an object')
  return (subscription_id, dict(nested))

def _parse_envelope(response_message: DecodableMessage) -> ProtocolMessage:
  if isinstance(response_message, Mapping):
    response_json = dict(response_message)
    revision = response_json.get('r', PROTOCOL_REVISION)
  else:
    if isinstance(response_message, bytes):
      response_message = response_message.decode()
    response_json = json.loads(response_message)
    revision = response_json.get('r')

  if revision != PROTOCOL_REVISION:
    raise IncompatibleProtocolVersion()
  return response_json

def decode_response(
    response_message: DecodableMessage
    ) -> Tuple[str, Any, Optional[str]]:
  '''Return ``(tag, value, section_slug)`` from a protocol envelope.'''
  response_json = _parse_envelope(response_message)

  tag = response_json['t']
  value = response_json['v'] if 'v' in response_json else None
  if tag == RequestTags.ROLE_OCCUPIED:
    raise RoleOccupied()

  return (tag, value, response_json.get('s'))

class IncompatibleProtocolVersion(Exception):
  pass

class UnexpectedResponseTag(Exception):
  pass

class RoleOccupied(Exception):
  '''A provider is already connected for this review UUID and section slug.'''
  pass

def expect_response(response_message: DecodableMessage, tag: str) -> Any:
  response_json = _parse_envelope(response_message)
  if response_json['t'] != tag:
    raise UnexpectedResponseTag()
  return response_json['v']
