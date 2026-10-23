# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Forge-agnostic core of acknowledg-forge.

acknowledg-forge keeps an AcknowlEDG review alive for every open pull request
on a code forge (GitHub, GitLab, ...). This module defines the forge-neutral
data the rest of the package works with, and the protocols a forge
implementation provides:

- ``Forge``: query pull requests and CI runs, download CI artifacts.
- ``ForgeWebhooks``: (optional) verify and parse webhook deliveries.
- ``ForgeCommenter``: (optional) leave comments on pull requests.

Nothing outside a forge implementation knows which forge is in use.

Requires Python 3.11+ (unlike the rest of ``edgacknowledg``, which keeps a
Python 3.6 floor); nothing outside this subpackage imports it.
'''

import enum

from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Mapping, Optional, Protocol, Sequence, Union


class PullRequestState(enum.StrEnum):
  OPEN = 'open'
  CLOSED = 'closed'
  MERGED = 'merged'


class CIStatus(enum.StrEnum):
  PENDING = 'pending'
  RUNNING = 'running'
  COMPLETED = 'completed'


class CIConclusion(enum.StrEnum):
  SUCCESS = 'success'
  FAILURE = 'failure'
  CANCELLED = 'cancelled'
  OTHER = 'other'


@dataclass(frozen = True)
class PullRequestInfo:
  '''A pull request (merge request, ...) as reported by the forge.'''
  # Forge-specific repository identifier, e.g. 'owner/name' or a GitLab
  # project path.
  repository: str
  number: int
  state: PullRequestState
  title: str
  author: str
  url: str
  # Branch the pull request targets, and its current commit.
  base_ref: str
  base_sha: str
  head_sha: str
  # Ref in the *base* repository that holds the head commit (e.g.
  # 'refs/pull/7/head'), so pull requests from forks need no extra remotes.
  fetch_ref: str
  updated_at: datetime


@dataclass(frozen = True)
class ArtifactInfo:
  '''A file produced by a CI run.'''
  name: str
  id: str
  size: int
  # Content digest reported by the forge, if any (e.g. 'sha256:...').
  digest: Optional[str]
  expired: bool


@dataclass(frozen = True)
class CIRunInfo:
  '''One attempt of the configured CI workflow for a commit.'''
  run_id: str
  attempt: int
  head_sha: str
  status: CIStatus
  conclusion: Optional[CIConclusion]
  completed_at: Optional[datetime]
  url: str
  artifacts: tuple[ArtifactInfo, ...]

  def artifact(self, name: str) -> Optional[ArtifactInfo]:
    '''Return the artifact called ``name``, if the run has one.'''
    for artifact in self.artifacts:
      if artifact.name == name:
        return artifact
    return None


@dataclass(frozen = True)
class PullRequestTouched:
  '''Something about a pull request may have changed.'''
  repository: str
  number: int


@dataclass(frozen = True)
class CIRunTouched:
  '''A CI run for the given commit may have changed.'''
  repository: str
  head_sha: str


# Webhook events carry only identity; the service re-queries the forge for
# the actual state, so a delivery's payload is never trusted.
ForgeEvent = Union[PullRequestTouched, CIRunTouched]


class ForgeError(Exception):
  '''Raised when a forge request fails.'''


class ArtifactTooLargeError(ForgeError):
  '''Raised when an artifact exceeds the configured size limit.'''


class Forge(Protocol):
  '''Read access to a forge.'''

  @property
  def name(self) -> str:
    '''Short, filesystem-safe forge name, e.g. 'github'.'''
    ...

  def clone_url(self, repository: str) -> str:
    ...

  def git_http_headers(self) -> Sequence[str]:
    '''HTTP headers (e.g. authorization) git needs to fetch from the forge.'''
    ...

  async def list_open_pull_requests(self,
                                    repository: str) -> list[PullRequestInfo]:
    ...

  async def get_pull_request(self,
                             repository: str,
                             number: int) -> PullRequestInfo:
    ...

  async def latest_ci_run(self,
                          pull_request: PullRequestInfo
                          ) -> Optional[CIRunInfo]:
    '''Return the newest attempt of the configured CI workflow for the pull
    request's head commit, or None if it hasn't run (or CI isn't tracked).
    '''
    ...

  async def download_artifact(self,
                              repository: str,
                              artifact: ArtifactInfo,
                              destination: Path,
                              *,
                              max_bytes: int) -> None:
    '''Download ``artifact`` as a zip archive to ``destination``.

    Raises ``ArtifactTooLargeError`` past ``max_bytes``.
    '''
    ...

  async def aclose(self) -> None:
    ...


class ForgeWebhooks(Protocol):
  '''Webhook support for a forge.'''

  def verify(self, headers: Mapping[str, str], body: bytes) -> bool:
    '''Return whether the delivery is authentic.'''
    ...

  def parse(self,
            headers: Mapping[str, str],
            body: bytes) -> list[ForgeEvent]:
    '''Return the events in an authentic delivery (possibly none).'''
    ...


class ForgeCommenter(Protocol):
  '''Comment access to a forge, for the service account.'''

  async def post_comment(self,
                         repository: str,
                         number: int,
                         body: str) -> str:
    '''Post a comment and return its forge id.'''
    ...

  async def find_comment(self,
                         repository: str,
                         number: int,
                         marker: str) -> Optional[str]:
    '''Return the id of a comment by the service account containing
    ``marker``, if there is one.
    '''
    ...
