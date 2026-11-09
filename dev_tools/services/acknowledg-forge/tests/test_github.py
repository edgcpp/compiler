# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The GitHub adapter against a mocked REST API.'''

import hashlib
import hmac
import json

from http import HTTPStatus
from pathlib import Path
from typing import Any, Callable

import httpx
import pytest

from edgacknowledg.forge import (
  ArtifactInfo,
  ArtifactTooLargeError,
  CIConclusion,
  CIRunTouched,
  CIStatus,
  PullRequestState,
  PullRequestTouched,
)

from acknowledg_forge_github import (
  GitHubClient,
  GitHubCommenter,
  GitHubForge,
  GitHubWebhooks,
  web_url_for,
)

REPO = 'edg/compiler'
API = 'https://api.github.com'


def _pull(number: int, *, state: str = 'open',
          merged_at: Any = None) -> dict[str, Any]:
  return {
    'number': number,
    'state': state,
    'merged_at': merged_at,
    'title': f"PR {number}",
    'user': {'login': 'author'},
    'html_url': f"https://github.com/{REPO}/pull/{number}",
    'base': {'ref': 'main', 'sha': 'b' * 40},
    'head': {'sha': 'h' * 40},
    'updated_at': '2026-10-01T12:00:00Z',
  }


def _client(handler: Callable[[httpx.Request], httpx.Response],
            token: str = 'secret-token') -> GitHubClient:
  return GitHubClient(
    api_url = API,
    token = token,
    transport = httpx.MockTransport(handler)
  )


def test_web_url_for() -> None:
  assert web_url_for(API) == 'https://github.com'
  assert web_url_for('https://ghe.example/api/v3') == 'https://ghe.example'


async def test_pull_requests_paginate_and_map_states() -> None:
  def handler(request: httpx.Request) -> httpx.Response:
    assert request.headers['authorization'] == 'Bearer secret-token'
    if request.url.path == f"/repos/{REPO}/pulls":
      if request.url.params.get('page') == '2':
        return httpx.Response(HTTPStatus.OK, json = [_pull(2)])
      return httpx.Response(
        HTTPStatus.OK,
        json = [_pull(1)],
        headers = {
          'link': f"<{API}/repos/{REPO}/pulls?state=open&page=2>; rel=\"next\""
        }
      )
    if request.url.path == f"/repos/{REPO}/pulls/3":
      return httpx.Response(HTTPStatus.OK, json = _pull(
        3, state = 'closed', merged_at = '2026-10-02T00:00:00Z'
      ))
    if request.url.path == f"/repos/{REPO}/pulls/4":
      return httpx.Response(HTTPStatus.OK, json = _pull(4, state = 'closed'))
    return httpx.Response(HTTPStatus.NOT_FOUND)

  forge = GitHubForge(_client(handler), workflow = 'ci.yml')
  pulls = await forge.list_open_pull_requests(REPO)
  assert [p.number for p in pulls] == [1, 2]
  assert pulls[0].fetch_ref == 'refs/pull/1/head'
  assert pulls[0].updated_at.tzinfo is not None
  assert (await forge.get_pull_request(REPO, 3)).state == (
    PullRequestState.MERGED
  )
  assert (await forge.get_pull_request(REPO, 4)).state == (
    PullRequestState.CLOSED
  )
  assert forge.clone_url(REPO) == f"https://github.com/{REPO}.git"
  assert forge.git_http_headers()[0].startswith('Authorization: Basic ')
  await forge.aclose()


async def test_latest_ci_run_with_artifacts() -> None:
  def handler(request: httpx.Request) -> httpx.Response:
    path = request.url.path
    if path == f"/repos/{REPO}/actions/workflows/ci.yml/runs":
      assert request.url.params['head_sha'] == 'h' * 40
      assert request.url.params['event'] == 'pull_request'
      return httpx.Response(HTTPStatus.OK, json = {'workflow_runs': [{
        'id': 77, 'run_attempt': 2, 'head_sha': 'h' * 40,
        'status': 'completed', 'conclusion': 'failure',
        'updated_at': '2026-10-01T13:00:00Z',
        'html_url': 'https://github.com/runs/77',
      }]})
    if path == f"/repos/{REPO}/actions/runs/77/artifacts":
      return httpx.Response(HTTPStatus.OK, json = {'artifacts': [{
        'id': 5, 'name': 'test-results-all', 'size_in_bytes': 10,
        'digest': 'sha256:abc', 'expired': False,
      }]})
    return httpx.Response(HTTPStatus.NOT_FOUND)

  forge = GitHubForge(_client(handler), workflow = 'ci.yml')
  pull = forge._pull_request(REPO, _pull(1))
  run = await forge.latest_ci_run(pull)
  assert run is not None
  assert (run.run_id, run.attempt) == ('77', 2)
  assert run.status == CIStatus.COMPLETED
  assert run.conclusion == CIConclusion.FAILURE
  artifact = run.artifact('test-results-all')
  assert artifact is not None and artifact.digest == 'sha256:abc'

  untracked = GitHubForge(_client(handler), workflow = None)
  assert await untracked.latest_ci_run(pull) is None


async def test_artifact_download_does_not_leak_token(tmp_path: Path) -> None:
  def handler(request: httpx.Request) -> httpx.Response:
    if request.url.host == 'api.github.com':
      return httpx.Response(
        HTTPStatus.FOUND,
        headers = {'location': 'https://blobs.example/artifact.zip?sig=1'}
      )
    assert 'authorization' not in request.headers
    return httpx.Response(HTTPStatus.OK, content = b'PK' + b'0' * 100)

  forge = GitHubForge(_client(handler), workflow = 'ci.yml')
  artifact = ArtifactInfo('test-results-all', '5', 102, None, False)
  destination = tmp_path / 'a.zip'
  await forge.download_artifact(REPO, artifact, destination, max_bytes = 1000)
  assert destination.stat().st_size == 102

  with pytest.raises(ArtifactTooLargeError):
    await forge.download_artifact(
      REPO,
      ArtifactInfo('test-results-all', '5', 10, None, False),
      tmp_path / 'b.zip',
      max_bytes = 50
    )


async def test_rate_limit_is_waited_out(monkeypatch: pytest.MonkeyPatch
                                        ) -> None:
  sleeps: list[float] = []

  async def fake_sleep(seconds: float) -> None:
    sleeps.append(seconds)

  monkeypatch.setattr('asyncio.sleep', fake_sleep)
  calls = 0

  def handler(request: httpx.Request) -> httpx.Response:
    nonlocal calls
    calls += 1
    if calls == 1:
      return httpx.Response(
        HTTPStatus.FORBIDDEN,
        headers = {'retry-after': '7'}
      )
    return httpx.Response(HTTPStatus.OK, json = _pull(1))

  forge = GitHubForge(_client(handler), workflow = None)
  assert (await forge.get_pull_request(REPO, 1)).number == 1
  assert sleeps == [7.0]


def test_webhooks_verify_and_parse() -> None:
  webhooks = GitHubWebhooks('hook-secret')
  body = json.dumps({
    'number': 9,
    'repository': {'full_name': REPO},
  }).encode()
  signature = 'sha256=' + hmac.new(
    b'hook-secret', body, hashlib.sha256
  ).hexdigest()
  assert webhooks.verify({'x-hub-signature-256': signature}, body)
  assert not webhooks.verify({'x-hub-signature-256': 'sha256=00'}, body)
  assert not webhooks.verify({}, body)

  assert webhooks.parse({'x-github-event': 'pull_request'}, body) == [
    PullRequestTouched(REPO, 9)
  ]
  run_body = json.dumps({
    'workflow_run': {'head_sha': 'h' * 40},
    'repository': {'full_name': REPO},
  }).encode()
  assert webhooks.parse({'x-github-event': 'workflow_run'}, run_body) == [
    CIRunTouched(REPO, 'h' * 40)
  ]
  assert webhooks.parse({'x-github-event': 'ping'}, b'{}') == []


async def test_commenter_finds_only_its_own_marked_comment() -> None:
  posted: list[Any] = []

  def handler(request: httpx.Request) -> httpx.Response:
    path = request.url.path
    if path == '/user':
      return httpx.Response(HTTPStatus.OK, json = {'login': 'acknowledg-bot'})
    if path == f"/repos/{REPO}/issues/1/comments":
      if request.method == 'POST':
        posted.append(json.loads(request.content))
        return httpx.Response(HTTPStatus.CREATED, json = {'id': 42})
      return httpx.Response(HTTPStatus.OK, json = [
        {'id': 1, 'user': {'login': 'someone'}, 'body': 'MARK'},
        {'id': 2, 'user': {'login': 'acknowledg-bot'}, 'body': 'x MARK y'},
      ])
    return httpx.Response(HTTPStatus.NOT_FOUND)

  commenter = GitHubCommenter(_client(handler))
  assert await commenter.post_comment(REPO, 1, 'hello') == '42'
  assert posted == [{'body': 'hello'}]
  assert await commenter.find_comment(REPO, 1, 'MARK') == '2'
  assert await commenter.find_comment(REPO, 1, 'OTHER') is None
