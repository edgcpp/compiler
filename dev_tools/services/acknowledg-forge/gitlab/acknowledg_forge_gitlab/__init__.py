# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''acknowledg-forge for GitLab (gitlab.com or self-managed).

Implements the ``edgacknowledg.forge`` protocols over the GitLab REST API
(v4): merge requests, pipelines and job artifacts, merge request notes, and
merge request / pipeline webhooks.

GitLab specifics:

- Repositories are project paths ('group/project').
- CI artifacts belong to jobs, so an artifact name (e.g.
  ``ACKNOWLEDG_FORGE_TEST_ARTIFACT``) names the *job* whose artifacts
  archive holds the results.
- ``ACKNOWLEDG_FORGE_CI_WORKFLOW`` names the pipeline to track (its
  ``workflow:name``), or '*' for the newest pipeline of any name.
'''

import hmac
import json
import urllib.parse

from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Mapping, Optional, Sequence

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

DEFAULT_API_URL = 'https://gitlab.com/api/v4'

ANY_PIPELINE = '*'

_COMPLETED = {
  'success': CIConclusion.SUCCESS,
  'failed': CIConclusion.FAILURE,
  'canceled': CIConclusion.CANCELLED,
  'skipped': CIConclusion.OTHER,
}


def _parse_time(value: str) -> datetime:
  return datetime.fromisoformat(value)


def _project(repository: str) -> str:
  '''The API path segment for a project path.'''
  return urllib.parse.quote(repository, safe = '')


def web_url_for(api_url: str) -> str:
  return api_url.rstrip('/').removesuffix('/api/v4')


class GitLabClient(ForgeHttpClient):
  '''Authenticated GitLab REST access.'''

  def __init__(self,
               *,
               api_url: str = DEFAULT_API_URL,
               token: Optional[str],
               transport: Optional[httpx.AsyncBaseTransport] = None) -> None:
    headers = {'PRIVATE-TOKEN': token} if token else {}
    super().__init__(base_url = api_url, headers = headers,
                     transport = transport)
    self.token = token


class GitLabForge:
  '''``Forge`` over the GitLab REST API.'''

  name = 'gitlab'

  def __init__(self,
               client: GitLabClient,
               *,
               pipeline: Optional[str],
               artifact_jobs: Sequence[str],
               clock: Optional[Callable[[], datetime]] = None) -> None:
    self._client = client
    self._pipeline = pipeline
    # Only these jobs' artifacts are reported.
    self._artifact_jobs = frozenset(artifact_jobs)
    self._web_url = web_url_for(client.base_url)
    self._clock = clock or (lambda: datetime.now(timezone.utc))

  def clone_url(self, repository: str) -> str:
    return f"{self._web_url}/{repository}.git"

  def git_http_headers(self) -> Sequence[str]:
    if not self._client.token:
      return []
    return [basic_auth_header('oauth2', self._client.token)]

  def _merge_request(self, repository: str, payload: Any) -> PullRequestInfo:
    state = {
      'opened': PullRequestState.OPEN,
      'merged': PullRequestState.MERGED,
    }.get(str(payload['state']), PullRequestState.CLOSED)
    iid = int(payload['iid'])
    return PullRequestInfo(
      repository = repository,
      number = iid,
      state = state,
      title = str(payload['title']),
      author = str((payload.get('author') or {}).get('username', '')),
      url = str(payload['web_url']),
      base_ref = str(payload['target_branch']),
      # Informational only (the service computes the merge-base itself);
      # list results don't include diff_refs.
      base_sha = str((payload.get('diff_refs') or {}).get('base_sha') or ''),
      head_sha = str(payload['sha']),
      fetch_ref = f"refs/merge-requests/{iid}/head",
      updated_at = _parse_time(payload['updated_at']),
    )

  async def list_open_pull_requests(self,
                                    repository: str) -> list[PullRequestInfo]:
    payloads = await self._client.paginate(
      f"/projects/{_project(repository)}/merge_requests",
      {'state': 'opened'}
    )
    return [self._merge_request(repository, payload) for payload in payloads]

  async def get_pull_request(self,
                             repository: str,
                             number: int) -> PullRequestInfo:
    payload = await self._client.get_json(
      f"/projects/{_project(repository)}/merge_requests/{number}"
    )
    return self._merge_request(repository, payload)

  async def latest_ci_run(self,
                          pull_request: PullRequestInfo
                          ) -> Optional[CIRunInfo]:
    if self._pipeline is None:
      return None
    project = _project(pull_request.repository)
    params: dict[str, Any] = {
      'sha': pull_request.head_sha,
      'order_by': 'id',
      'sort': 'desc',
      'per_page': 1,
    }
    if self._pipeline != ANY_PIPELINE:
      params['name'] = self._pipeline
    pipelines = await self._client.get_json(
      f"/projects/{project}/pipelines",
      params
    )
    if len(pipelines) == 0:
      return None
    pipeline = pipelines[0]

    status_name = str(pipeline['status'])
    artifacts: tuple[ArtifactInfo, ...] = ()
    if status_name in _COMPLETED:
      status = CIStatus.COMPLETED
      conclusion: Optional[CIConclusion] = _COMPLETED[status_name]
      artifacts = await self._job_artifacts(project, pipeline['id'])
    else:
      status = (
        CIStatus.RUNNING if status_name == 'running' else CIStatus.PENDING
      )
      conclusion = None
    return CIRunInfo(
      run_id = str(pipeline['id']),
      # Retried jobs get new ids (and so new artifact ids); the pipeline
      # itself has no attempt number.
      attempt = 1,
      head_sha = str(pipeline['sha']),
      status = status,
      conclusion = conclusion,
      completed_at = (
        _parse_time(pipeline['updated_at']) if status == CIStatus.COMPLETED
        else None
      ),
      url = str(pipeline['web_url']),
      artifacts = artifacts,
    )

  async def _job_artifacts(self,
                           project: str,
                           pipeline_id: object
                           ) -> tuple[ArtifactInfo, ...]:
    now = self._clock()
    artifacts = []
    for job in await self._client.paginate(
        f"/projects/{project}/pipelines/{pipeline_id}/jobs"):
      name = str(job['name'])
      if name not in self._artifact_jobs:
        continue
      archives = [
        item for item in job.get('artifacts') or []
        if item.get('file_type') == 'archive'
      ]
      if len(archives) == 0:
        continue
      expire_at = job.get('artifacts_expire_at')
      artifacts.append(ArtifactInfo(
        name = name,
        id = str(job['id']),
        size = sum(int(item.get('size') or 0) for item in archives),
        digest = None,
        expired = (
          expire_at is not None and _parse_time(expire_at) <= now
        ),
      ))
    return tuple(artifacts)

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
      f"/projects/{_project(repository)}/jobs/{artifact.id}/artifacts",
      destination,
      max_bytes = max_bytes
    )

  async def aclose(self) -> None:
    await self._client.aclose()


class GitLabWebhooks:
  '''``ForgeWebhooks`` for GitLab's secret-token deliveries.'''

  def __init__(self, secret: str) -> None:
    self._secret = secret.encode()

  def verify(self, headers: Mapping[str, str], body: bytes) -> bool:
    token = headers.get('x-gitlab-token', '').encode()
    return hmac.compare_digest(token, self._secret)

  def parse(self,
            headers: Mapping[str, str],
            body: bytes) -> list[ForgeEvent]:
    event = headers.get('x-gitlab-event')
    if event not in ('Merge Request Hook', 'Pipeline Hook'):
      return []
    payload = json.loads(body)
    repository = str(payload['project']['path_with_namespace'])
    attributes = payload.get('object_attributes') or {}
    if event == 'Merge Request Hook':
      return [PullRequestTouched(repository, int(attributes['iid']))]
    events: list[ForgeEvent] = [
      CIRunTouched(repository, str(attributes['sha']))
    ]
    merge_request = payload.get('merge_request')
    if merge_request:
      events.append(PullRequestTouched(repository, int(merge_request['iid'])))
    return events


class GitLabCommenter:
  '''``ForgeCommenter`` posting merge request notes as the token's user.'''

  def __init__(self, client: GitLabClient) -> None:
    self._client = client
    self._username: Optional[str] = None

  async def post_comment(self,
                         repository: str,
                         number: int,
                         body: str) -> str:
    response = await self._client.request(
      'POST',
      f"/projects/{_project(repository)}/merge_requests/{number}/notes",
      json_body = {'body': body}
    )
    return str(response.json()['id'])

  async def _own_username(self) -> Optional[str]:
    if self._username is None:
      try:
        self._username = str(
          (await self._client.get_json('/user'))['username']
        )
      except ForgeError:
        self._username = ''
    return self._username or None

  async def find_comment(self,
                         repository: str,
                         number: int,
                         marker: str) -> Optional[str]:
    username = await self._own_username()
    for note in await self._client.paginate(
        f"/projects/{_project(repository)}/merge_requests/{number}/notes",
        {'sort': 'asc'}):
      author = (note.get('author') or {}).get('username')
      if username is not None and author != username:
        continue
      if marker in str(note.get('body', '')):
        return str(note['id'])
    return None
