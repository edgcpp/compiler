# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The GitLab adapter against a mocked REST API (v4).'''

import json

from datetime import datetime, timezone
from http import HTTPStatus
from pathlib import Path
from typing import Any, Callable

import httpx

from edgacknowledg.forge import (
  ArtifactInfo,
  CIConclusion,
  CIRunTouched,
  CIStatus,
  PullRequestState,
  PullRequestTouched,
)

from acknowledg_forge_gitlab import (
  GitLabClient,
  GitLabCommenter,
  GitLabForge,
  GitLabWebhooks,
  web_url_for,
)

PROJECT = 'edg/front-end'
ENCODED = 'edg%2Ffront-end'
API = 'https://gitlab.example/api/v4'
NOW = datetime(2026, 10, 1, 12, 0, tzinfo = timezone.utc)


def _merge_request(iid: int, state: str = 'opened') -> dict[str, Any]:
  return {
    'iid': iid,
    'state': state,
    'title': f"MR {iid}",
    'author': {'username': 'author'},
    'web_url': f"https://gitlab.example/{PROJECT}/-/merge_requests/{iid}",
    'target_branch': 'main',
    'sha': 'h' * 40,
    'diff_refs': {'base_sha': 'b' * 40},
    'updated_at': '2026-10-01T12:00:00.000Z',
  }


def _forge(handler: Callable[[httpx.Request], httpx.Response],
           pipeline: str = 'ci') -> GitLabForge:
  client = GitLabClient(
    api_url = API,
    token = 'glpat-secret',
    transport = httpx.MockTransport(handler)
  )
  return GitLabForge(
    client,
    pipeline = pipeline,
    artifact_jobs = ('test-results-all', 'bench-results'),
    clock = lambda: NOW
  )


def test_web_url_for() -> None:
  assert web_url_for(API) == 'https://gitlab.example'


async def test_merge_requests_map_states_and_paths() -> None:
  def handler(request: httpx.Request) -> httpx.Response:
    assert request.headers['private-token'] == 'glpat-secret'
    # Project paths are URL-encoded into one path segment.
    path = request.url.raw_path.decode().split('?')[0]
    if path == f"/api/v4/projects/{ENCODED}/merge_requests":
      assert request.url.params['state'] == 'opened'
      return httpx.Response(HTTPStatus.OK, json = [_merge_request(1)])
    if path == f"/api/v4/projects/{ENCODED}/merge_requests/2":
      return httpx.Response(HTTPStatus.OK, json = _merge_request(2, 'merged'))
    if path == f"/api/v4/projects/{ENCODED}/merge_requests/3":
      return httpx.Response(HTTPStatus.OK, json = _merge_request(3, 'locked'))
    return httpx.Response(HTTPStatus.NOT_FOUND)

  forge = _forge(handler)
  [first] = await forge.list_open_pull_requests(PROJECT)
  assert first.fetch_ref == 'refs/merge-requests/1/head'
  assert first.base_ref == 'main' and first.head_sha == 'h' * 40
  assert (await forge.get_pull_request(PROJECT, 2)).state == (
    PullRequestState.MERGED
  )
  assert (await forge.get_pull_request(PROJECT, 3)).state == (
    PullRequestState.CLOSED
  )
  assert forge.clone_url(PROJECT) == f"https://gitlab.example/{PROJECT}.git"
  await forge.aclose()


async def test_pipeline_with_job_artifacts() -> None:
  def handler(request: httpx.Request) -> httpx.Response:
    path = request.url.raw_path.decode().split('?')[0]
    if path == f"/api/v4/projects/{ENCODED}/pipelines":
      assert request.url.params['sha'] == 'h' * 40
      assert request.url.params['name'] == 'ci'
      return httpx.Response(HTTPStatus.OK, json = [{
        'id': 900, 'sha': 'h' * 40, 'status': 'failed',
        'updated_at': '2026-10-01T13:00:00Z',
        'web_url': 'https://gitlab.example/pipelines/900',
      }])
    if path == f"/api/v4/projects/{ENCODED}/pipelines/900/jobs":
      return httpx.Response(HTTPStatus.OK, json = [
        {'id': 1, 'name': 'build', 'artifacts': [
          {'file_type': 'archive', 'size': 5},
        ]},
        {'id': 2, 'name': 'test-results-all',
         'artifacts_expire_at': '2026-10-10T00:00:00Z', 'artifacts': [
           {'file_type': 'archive', 'size': 10},
           {'file_type': 'trace', 'size': 99},
         ]},
        {'id': 3, 'name': 'bench-results',
         'artifacts_expire_at': '2026-09-01T00:00:00Z', 'artifacts': [
           {'file_type': 'archive', 'size': 20},
         ]},
      ])
    return httpx.Response(HTTPStatus.NOT_FOUND)

  forge = _forge(handler)
  pull = forge._merge_request(PROJECT, _merge_request(1))
  run = await forge.latest_ci_run(pull)
  assert run is not None
  assert run.status == CIStatus.COMPLETED
  assert run.conclusion == CIConclusion.FAILURE
  assert [a.name for a in run.artifacts] == [
    'test-results-all', 'bench-results'
  ]
  tests = run.artifact('test-results-all')
  assert tests is not None and tests.id == '2' and tests.size == 10
  assert not tests.expired
  bench = run.artifact('bench-results')
  assert bench is not None and bench.expired


async def test_running_pipeline_has_no_artifacts() -> None:
  def handler(request: httpx.Request) -> httpx.Response:
    assert 'name' not in request.url.params
    return httpx.Response(HTTPStatus.OK, json = [{
      'id': 901, 'sha': 'h' * 40, 'status': 'running',
      'updated_at': '2026-10-01T13:00:00Z', 'web_url': 'u',
    }])

  forge = _forge(handler, pipeline = '*')
  run = await forge.latest_ci_run(
    forge._merge_request(PROJECT, _merge_request(1))
  )
  assert run is not None and run.status == CIStatus.RUNNING
  assert run.artifacts == ()


async def test_job_artifact_download(tmp_path: Path) -> None:
  def handler(request: httpx.Request) -> httpx.Response:
    path = request.url.raw_path.decode()
    assert path == f"/api/v4/projects/{ENCODED}/jobs/2/artifacts"
    return httpx.Response(HTTPStatus.OK, content = b'PK-zip')

  forge = _forge(handler)
  destination = tmp_path / 'a.zip'
  await forge.download_artifact(
    PROJECT,
    ArtifactInfo('test-results-all', '2', 6, None, False),
    destination,
    max_bytes = 100
  )
  assert destination.read_bytes() == b'PK-zip'


def test_webhooks_verify_and_parse() -> None:
  webhooks = GitLabWebhooks('hook-secret')
  assert webhooks.verify({'x-gitlab-token': 'hook-secret'}, b'')
  assert not webhooks.verify({'x-gitlab-token': 'nope'}, b'')
  assert not webhooks.verify({}, b'')

  merge_body = json.dumps({
    'project': {'path_with_namespace': PROJECT},
    'object_attributes': {'iid': 4},
  }).encode()
  assert webhooks.parse({'x-gitlab-event': 'Merge Request Hook'},
                        merge_body) == [PullRequestTouched(PROJECT, 4)]
  pipeline_body = json.dumps({
    'project': {'path_with_namespace': PROJECT},
    'object_attributes': {'sha': 'h' * 40},
    'merge_request': {'iid': 4},
  }).encode()
  assert webhooks.parse({'x-gitlab-event': 'Pipeline Hook'},
                        pipeline_body) == [
    CIRunTouched(PROJECT, 'h' * 40),
    PullRequestTouched(PROJECT, 4),
  ]
  assert webhooks.parse({'x-gitlab-event': 'Push Hook'}, b'{}') == []


async def test_commenter_uses_merge_request_notes() -> None:
  def handler(request: httpx.Request) -> httpx.Response:
    path = request.url.raw_path.decode().split('?')[0]
    if path == '/api/v4/user':
      return httpx.Response(
        HTTPStatus.OK,
        json = {'username': 'acknowledg-bot'}
      )
    if path == f"/api/v4/projects/{ENCODED}/merge_requests/4/notes":
      if request.method == 'POST':
        assert json.loads(request.content) == {'body': 'hi'}
        return httpx.Response(HTTPStatus.CREATED, json = {'id': 7})
      return httpx.Response(HTTPStatus.OK, json = [
        {'id': 5, 'author': {'username': 'other'}, 'body': 'MARK'},
        {'id': 6, 'author': {'username': 'acknowledg-bot'}, 'body': 'MARK'},
      ])
    return httpx.Response(HTTPStatus.NOT_FOUND)

  client = GitLabClient(
    api_url = API,
    token = 't',
    transport = httpx.MockTransport(handler)
  )
  commenter = GitLabCommenter(client)
  assert await commenter.post_comment(PROJECT, 4, 'hi') == '7'
  assert await commenter.find_comment(PROJECT, 4, 'MARK') == '6'
