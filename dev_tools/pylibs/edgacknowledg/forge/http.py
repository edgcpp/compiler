# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''HTTP plumbing shared by forge REST clients.

Retries transient failures, waits out rate limits (``Retry-After`` or
``x-ratelimit-*`` / ``ratelimit-*`` reset headers), follows ``Link``
pagination, and downloads files without sending credentials to the storage
hosts downloads may redirect to.
'''

import asyncio
import logging
import time

from http import HTTPStatus
from pathlib import Path
from typing import Any, Mapping, Optional

import httpx

from edgacknowledg.forge import ArtifactTooLargeError, ForgeError

_log = logging.getLogger(__name__)

PAGE_SIZE = 100
# The largest API response read (decoded): far more than a page of
# PAGE_SIZE items; downloads have their own limit.
MAX_RESPONSE_BYTES = 32 * 1024 * 1024
MAX_RATE_LIMIT_WAIT = 15 * 60.0
MAX_ATTEMPTS = 4

_REDIRECTS = (
  HTTPStatus.MOVED_PERMANENTLY,
  HTTPStatus.FOUND,
  HTTPStatus.SEE_OTHER,
  HTTPStatus.TEMPORARY_REDIRECT,
  HTTPStatus.PERMANENT_REDIRECT,
)


def _rate_limit_wait(response: httpx.Response) -> Optional[float]:
  if response.status_code not in (HTTPStatus.FORBIDDEN,
                                  HTTPStatus.TOO_MANY_REQUESTS):
    return None
  retry_after = response.headers.get('retry-after')
  if retry_after is not None and retry_after.isdigit():
    return min(float(retry_after), MAX_RATE_LIMIT_WAIT)
  for prefix in ('x-ratelimit-', 'ratelimit-'):
    if response.headers.get(f"{prefix}remaining") == '0':
      reset = response.headers.get(f"{prefix}reset", '')
      if reset.isdigit():
        return min(max(1.0, float(reset) - time.time()), MAX_RATE_LIMIT_WAIT)
  return None


class ForgeHttpClient:
  '''An authenticated REST client for one forge API.'''

  def __init__(self,
               *,
               base_url: str,
               headers: Mapping[str, str],
               transport: Optional[httpx.AsyncBaseTransport] = None) -> None:
    self.base_url = base_url.rstrip('/')
    self._client = httpx.AsyncClient(
      base_url = self.base_url,
      headers = {'User-Agent': 'acknowledg-forge', **headers},
      timeout = httpx.Timeout(30.0, read = 120.0),
      transport = transport,
      follow_redirects = False
    )
    # Redirect targets (object storage) must not see the credentials.
    self._anonymous = httpx.AsyncClient(
      headers = {'User-Agent': 'acknowledg-forge'},
      timeout = httpx.Timeout(30.0, read = 300.0),
      transport = transport,
      follow_redirects = True
    )

  async def aclose(self) -> None:
    await self._client.aclose()
    await self._anonymous.aclose()

  async def request(self,
                    method: str,
                    path: str,
                    *,
                    params: Optional[Mapping[str, Any]] = None,
                    json_body: Optional[Any] = None) -> httpx.Response:
    '''Send a request, retrying transient failures and waiting out rate
    limits; raise ``ForgeError`` on failure, or for a response over
    ``MAX_RESPONSE_BYTES`` (read only that far).
    '''
    for attempt in range(MAX_ATTEMPTS):
      last_attempt = attempt + 1 == MAX_ATTEMPTS
      try:
        request = self._client.build_request(
          method,
          path,
          params = params,
          json = json_body
        )
        response = await _send_capped(self._client, request, method, path)
      except httpx.HTTPError as ex:
        if last_attempt:
          raise ForgeError(f"{method} {path} failed: {ex}") from ex
        await asyncio.sleep(2 ** attempt)
        continue

      wait = _rate_limit_wait(response)
      if wait is not None and not last_attempt:
        _log.warning('rate limited; waiting %.0f s', wait)
        await asyncio.sleep(wait)
        continue
      if (response.status_code >= HTTPStatus.INTERNAL_SERVER_ERROR and
          not last_attempt):
        await asyncio.sleep(2 ** attempt)
        continue
      if response.is_error or response.status_code in _REDIRECTS:
        raise ForgeError(
          f"{method} {path} failed: {response.status_code} "
          f"{response.text[:500]}"
        )
      return response
    raise ForgeError(f"{method} {path} failed after {MAX_ATTEMPTS} attempts")

  async def get_json(self,
                     path: str,
                     params: Optional[Mapping[str, Any]] = None) -> Any:
    return (await self.request('GET', path, params = params)).json()

  async def paginate(self,
                     path: str,
                     params: Optional[Mapping[str, Any]] = None,
                     *,
                     key: Optional[str] = None) -> list[Any]:
    '''Collect every page of a list endpoint (``key`` selects the list in
    wrapped responses such as ``{"workflow_runs": [...]}``).
    '''
    items: list[Any] = []
    next_path: Optional[str] = path
    next_params: Optional[Mapping[str, Any]] = {
      **(params or {}),
      'per_page': PAGE_SIZE,
    }
    while next_path is not None:
      response = await self.request('GET', next_path, params = next_params)
      payload = response.json()
      items += payload[key] if key is not None else payload
      # The next link is absolute and carries its own query.
      next_path = response.links.get('next', {}).get('url')
      next_params = None
    return items

  async def download(self,
                     path: str,
                     destination: Path,
                     *,
                     max_bytes: int) -> None:
    '''Stream ``path`` (served directly or via a redirect to storage) to
    ``destination``, failing past ``max_bytes``. Not retried here; callers
    retry the whole operation.
    '''
    try:
      async with self._client.stream('GET', path) as response:
        if response.status_code not in _REDIRECTS:
          if response.is_error:
            raise ForgeError(
              f"download of {path} failed: {response.status_code}"
            )
          await _write_capped(response, destination, max_bytes, path)
          return
        location = response.headers.get('location')
      if not location:
        raise ForgeError(f"redirect without a location for {path}")
      async with self._anonymous.stream('GET', location) as stream:
        if stream.is_error:
          raise ForgeError(f"download of {path} failed: {stream.status_code}")
        await _write_capped(stream, destination, max_bytes, path)
    except httpx.HTTPError as ex:
      raise ForgeError(f"download of {path} failed: {ex}") from ex


async def _send_capped(client: httpx.AsyncClient,
                       request: httpx.Request,
                       method: str,
                       path: str) -> httpx.Response:
  '''Send ``request``; return its response, its body read (decoded) as it
  comes, and only to ``MAX_RESPONSE_BYTES``: ``ForgeError`` beyond.
  '''
  response = await client.send(request, stream = True)
  try:
    body = bytearray()
    async for chunk in response.aiter_bytes():
      body += chunk
      if len(body) > MAX_RESPONSE_BYTES:
        raise ForgeError(
          f"{method} {path}: the response is over {MAX_RESPONSE_BYTES} bytes"
        )
  finally:
    await response.aclose()
  # The same response with its body in place; it is decoded already, so
  # without the headers that describe how it was sent.
  headers = [
    (name, value) for name, value in response.headers.multi_items()
    if name.lower() not in ('content-encoding', 'content-length',
                            'transfer-encoding')
  ]
  return httpx.Response(
    response.status_code,
    headers = headers,
    content = bytes(body),
    request = request,
    extensions = response.extensions
  )


async def _write_capped(stream: httpx.Response,
                        destination: Path,
                        max_bytes: int,
                        label: str) -> None:
  written = 0
  with open(destination, 'wb') as handle:
    async for chunk in stream.aiter_bytes():
      written += len(chunk)
      if written > max_bytes:
        raise ArtifactTooLargeError(
          f"{label} is larger than {max_bytes} bytes"
        )
      handle.write(chunk)
