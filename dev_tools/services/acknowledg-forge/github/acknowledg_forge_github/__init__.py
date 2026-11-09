# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''acknowledg-forge for GitHub (and GitHub Enterprise Server).

Implements the ``edgacknowledg.forge`` protocols over the GitHub REST API:
pull requests, GitHub Actions workflow runs and artifacts, issue comments,
and ``pull_request`` / ``workflow_run`` webhooks.
'''

import hashlib
import hmac
import json

from datetime import datetime
from pathlib import Path
from typing import Any, Mapping, Optional, Sequence

import httpx

from edgacknowledg.forge import (
  ArtifactInfo,
  ArtifactTooLargeError,
  CIConclusion,
  CIRunInfo,
  CIRunTouched,
  CIStatus,
  ForgeError,
  ForgeEvent,
  PullRequestInfo,
  PullRequestState,
  PullRequestTouched,
)
from edgacknowledg.forge.http import ForgeHttpClient
from edgacknowledg.forge.workspace import basic_auth_header

DEFAULT_API_URL = 'https://api.github.com'
DEFAULT_WEB_URL = 'https://github.com'

_API_VERSION = '2022-11-28'

_CONCLUSIONS = {
  'success': CIConclusion.SUCCESS,
  'failure': CIConclusion.FAILURE,
  'timed_out': CIConclusion.FAILURE,
  'startup_failure': CIConclusion.FAILURE,
  'cancelled': CIConclusion.CANCELLED,
}


def _parse_time(value: str) -> datetime:
  return datetime.fromisoformat(value)


def web_url_for(api_url: str) -> str:
  '''The web (and git) base URL for an API URL: github.com's API lives on
  api.github.com; GitHub Enterprise Server's under <host>/api/v3.
  '''
  if api_url.rstrip('/') == DEFAULT_API_URL:
    return DEFAULT_WEB_URL
  return api_url.rstrip('/').removesuffix('/api/v3')


class GitHubClient(ForgeHttpClient):
  '''Authenticated GitHub REST access.'''

  def __init__(self,
               *,
               api_url: str = DEFAULT_API_URL,
               token: Optional[str],
               transport: Optional[httpx.AsyncBaseTransport] = None) -> None:
    headers = {
      'Accept': 'application/vnd.github+json',
      'X-GitHub-Api-Version': _API_VERSION,
    }
    if token:
      headers['Authorization'] = f"Bearer {token}"
    super().__init__(base_url = api_url, headers = headers,
                     transport = transport)
    self.token = token


class GitHubForge:
  '''``Forge`` over the GitHub REST API.'''

  name = 'github'

  def __init__(self,
               client: GitHubClient,
               *,
               workflow: Optional[str]) -> None:
    self._client = client
    self._workflow = workflow
    self._web_url = web_url_for(client.base_url)

  def clone_url(self, repository: str) -> str:
    return f"{self._web_url}/{repository}.git"

  def git_http_headers(self) -> Sequence[str]:
    if not self._client.token:
      return []
    return [basic_auth_header('x-access-token', self._client.token)]

  def _pull_request(self, repository: str, payload: Any) -> PullRequestInfo:
    if payload['state'] == 'open':
      state = PullRequestState.OPEN
    elif payload.get('merged_at'):
      state = PullRequestState.MERGED
    else:
      state = PullRequestState.CLOSED
    return PullRequestInfo(
      repository = repository,
      number = int(payload['number']),
      state = state,
      title = str(payload['title']),
      author = str((payload.get('user') or {}).get('login', '')),
      url = str(payload['html_url']),
      base_ref = str(payload['base']['ref']),
      base_sha = str(payload['base']['sha']),
      head_sha = str(payload['head']['sha']),
      fetch_ref = f"refs/pull/{int(payload['number'])}/head",
      updated_at = _parse_time(payload['updated_at']),
    )

  async def list_open_pull_requests(self,
                                    repository: str) -> list[PullRequestInfo]:
    payloads = await self._client.paginate(
      f"/repos/{repository}/pulls",
      {'state': 'open'}
    )
    return [self._pull_request(repository, payload) for payload in payloads]

  async def get_pull_request(self,
                             repository: str,
                             number: int) -> PullRequestInfo:
    payload = await self._client.get_json(
      f"/repos/{repository}/pulls/{number}"
    )
    return self._pull_request(repository, payload)

  async def latest_ci_run(self,
                          pull_request: PullRequestInfo
                          ) -> Optional[CIRunInfo]:
    if self._workflow is None:
      return None
    repository = pull_request.repository
    payload = await self._client.get_json(
      f"/repos/{repository}/actions/workflows/{self._workflow}/runs",
      {
        'head_sha': pull_request.head_sha,
        'event': 'pull_request',
        'per_page': 1,
      }
    )
    runs = payload.get('workflow_runs') or []
    if len(runs) == 0:
      return None
    run = runs[0]

    if run['status'] == 'completed':
      status = CIStatus.COMPLETED
      conclusion: Optional[CIConclusion] = _CONCLUSIONS.get(
        str(run.get('conclusion')),
        CIConclusion.OTHER
      )
      artifacts = tuple(
        ArtifactInfo(
          name = str(artifact['name']),
          id = str(artifact['id']),
          size = int(artifact.get('size_in_bytes', 0)),
          digest = artifact.get('digest'),
          expired = bool(artifact.get('expired', False)),
        )
        for artifact in await self._client.paginate(
          f"/repos/{repository}/actions/runs/{run['id']}/artifacts",
          key = 'artifacts'
        )
      )
    else:
      status = (
        CIStatus.RUNNING if run['status'] == 'in_progress'
        else CIStatus.PENDING
      )
      conclusion = None
      artifacts = ()
    return CIRunInfo(
      run_id = str(run['id']),
      attempt = int(run.get('run_attempt', 1)),
      head_sha = str(run['head_sha']),
      status = status,
      conclusion = conclusion,
      completed_at = (
        _parse_time(run['updated_at']) if status == CIStatus.COMPLETED
        else None
      ),
      url = str(run['html_url']),
      artifacts = artifacts,
    )

  async def download_artifact(self,
                              repository: str,
                              artifact: ArtifactInfo,
                              destination: Path,
                              *,
                              max_bytes: int) -> None:
    if artifact.size > max_bytes:
      raise ArtifactTooLargeError(
        f"artifact {artifact.name} is {artifact.size} bytes"
      )
    await self._client.download(
      f"/repos/{repository}/actions/artifacts/{artifact.id}/zip",
      destination,
      max_bytes = max_bytes
    )

  async def aclose(self) -> None:
    await self._client.aclose()


class GitHubWebhooks:
  '''``ForgeWebhooks`` for GitHub's signed deliveries.'''

  def __init__(self, secret: str) -> None:
    self._secret = secret.encode()

  def verify(self, headers: Mapping[str, str], body: bytes) -> bool:
    signature = headers.get('x-hub-signature-256', '')
    expected = 'sha256=' + hmac.new(
      self._secret,
      body,
      hashlib.sha256
    ).hexdigest()
    return hmac.compare_digest(signature.encode(), expected.encode())

  def parse(self,
            headers: Mapping[str, str],
            body: bytes) -> list[ForgeEvent]:
    event = headers.get('x-github-event')
    if event not in ('pull_request', 'workflow_run'):
      return []
    payload = json.loads(body)
    repository = str(payload['repository']['full_name'])
    if event == 'pull_request':
      return [PullRequestTouched(repository, int(payload['number']))]
    return [
      CIRunTouched(repository, str(payload['workflow_run']['head_sha']))
    ]


class GitHubCommenter:
  '''``ForgeCommenter`` posting issue comments as the token's account.'''

  def __init__(self, client: GitHubClient) -> None:
    self._client = client
    self._login: Optional[str] = None

  async def post_comment(self,
                         repository: str,
                         number: int,
                         body: str) -> str:
    response = await self._client.request(
      'POST',
      f"/repos/{repository}/issues/{number}/comments",
      json_body = {'body': body}
    )
    return str(response.json()['id'])

  async def _own_login(self) -> Optional[str]:
    if self._login is None:
      try:
        self._login = str((await self._client.get_json('/user'))['login'])
      except ForgeError:
        # E.g. a GitHub App installation token; match on the marker alone.
        self._login = ''
    return self._login or None

  async def find_comment(self,
                         repository: str,
                         number: int,
                         marker: str) -> Optional[str]:
    login = await self._own_login()
    for comment in await self._client.paginate(
        f"/repos/{repository}/issues/{number}/comments"):
      author = (comment.get('user') or {}).get('login')
      if login is not None and author != login:
        continue
      if marker in str(comment.get('body', '')):
        return str(comment['id'])
    return None
