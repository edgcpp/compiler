# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Test doubles for the forge, publisher, and commenter.'''

import shutil
import uuid

from dataclasses import replace
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Optional, Sequence

from edgacknowledg import RichName
from edgacknowledg.forge import (
  ArtifactInfo,
  CIRunInfo,
  PullRequestInfo,
  PullRequestState,
)
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.publisher import (
  PublisherUnavailableError,
  ReviewIdentity,
)

REPOSITORY = 'edg/compiler'


class Clock:
  '''A controllable UTC clock.'''

  def __init__(self) -> None:
    self.now = datetime(2026, 10, 1, 12, 0, tzinfo = timezone.utc)

  def __call__(self) -> datetime:
    return self.now

  def advance(self, seconds: float) -> None:
    self.now += timedelta(seconds = seconds)


class FakeForge:
  '''A forge whose state the test sets directly; git comes from a local
  origin repository.
  '''

  name = 'fake'

  def __init__(self, origin: Path) -> None:
    self.origin = origin
    self.pull_requests: dict[int, PullRequestInfo] = {}
    self.runs: dict[str, CIRunInfo] = {}
    self.artifact_files: dict[str, Path] = {}
    self.downloads = 0
    self.closed = False

  def set_pull_request(self, info: PullRequestInfo) -> None:
    self.pull_requests[info.number] = info

  def update_pull_request(self, number: int, **changes: object) -> None:
    self.pull_requests[number] = replace(
      self.pull_requests[number],
      **changes  # type: ignore[arg-type]
    )

  def clone_url(self, repository: str) -> str:
    return f"file://{self.origin}"

  def git_http_headers(self) -> Sequence[str]:
    return []

  async def list_open_pull_requests(self,
                                    repository: str) -> list[PullRequestInfo]:
    return [
      info for info in self.pull_requests.values()
      if info.state == PullRequestState.OPEN
    ]

  async def get_pull_request(self,
                             repository: str,
                             number: int) -> PullRequestInfo:
    return self.pull_requests[number]

  async def latest_ci_run(self,
                          pull_request: PullRequestInfo
                          ) -> Optional[CIRunInfo]:
    return self.runs.get(pull_request.head_sha)

  async def download_artifact(self,
                              repository: str,
                              artifact: ArtifactInfo,
                              destination: Path,
                              *,
                              max_bytes: int) -> None:
    self.downloads += 1
    shutil.copyfile(self.artifact_files[artifact.id], destination)

  async def aclose(self) -> None:
    self.closed = True


class FakePublisher:
  '''Records what would be served.'''

  def __init__(self) -> None:
    self.reviews: dict[uuid.UUID, list[SectionRecord]] = {}
    self.names: dict[uuid.UUID, RichName] = {}
    self.available = True
    self.sync_count = 0

  async def wait_connected(self) -> None:
    pass

  async def sync(self,
                 review: ReviewIdentity,
                 sections: Sequence[SectionRecord]) -> None:
    if not self.available:
      raise PublisherUnavailableError('fake outage')
    self.sync_count += 1
    for section in sections:
      assert section.root.is_dir(), section
    if len(sections) == 0:
      self.reviews.pop(review.review_id, None)
    else:
      self.reviews[review.review_id] = list(sections)
      self.names[review.review_id] = review.name

  async def remove(self, review_id: uuid.UUID) -> None:
    self.reviews.pop(review_id, None)

  def section_url(self,
                  review_id: uuid.UUID,
                  section: SectionRecord) -> Optional[str]:
    return f"http://ui.example/review/{review_id}/{section.slug}"

  def served_names(self, review_id: uuid.UUID) -> list[str]:
    return sorted(s.name for s in self.reviews.get(review_id, []))


class FakeCommenter:
  def __init__(self) -> None:
    self.comments: list[tuple[str, int, str]] = []
    self.fail_after_post = False

  async def post_comment(self,
                         repository: str,
                         number: int,
                         body: str) -> str:
    self.comments.append((repository, number, body))
    if self.fail_after_post:
      self.fail_after_post = False
      raise ConnectionError('lost the response after posting')
    return str(len(self.comments))

  async def find_comment(self,
                         repository: str,
                         number: int,
                         marker: str) -> Optional[str]:
    for index, (comment_repository, comment_number, body) in enumerate(
        self.comments):
      if (comment_repository, comment_number) == (repository, number) and (
          marker in body):
        return str(index + 1)
    return None

  def bodies(self) -> list[str]:
    return [body for _, _, body in self.comments]
