# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''End-to-end pull request lifecycle against a fake forge.'''

from pathlib import Path

from edgacknowledg import rich_link, rich_text
from edgacknowledg.forge import (
  ArtifactInfo,
  CIConclusion,
  CIStatus,
  PullRequestState,
)
from edgacknowledg.forge.db import ForgeStore

from conftest import (
  Harness,
  Origin,
  ci_run,
  make_config,
  make_test_artifact,
  pull_request_info,
)
from fakes import REPOSITORY, Clock, FakeForge

CHANGES = 'Test - Changes'
FAILURES = 'Test - Failures'

PR_FILES = {
  'tests/tests/suite/.a.rto/default.1.1.txt': 'new output\n',
  'tests/expectations/edg_x86_64/suite.log': 'a.sft.cpp: FAIL\n',
}


async def _review_id(store: ForgeStore, number: int) -> object:
  record = await store.find_pull_request('fake', REPOSITORY, number)
  assert record is not None
  return record.review_id


async def test_motivating_example(tmp_path: Path,
                                  origin: Origin,
                                  forge: FakeForge,
                                  store: ForgeStore,
                                  clock: Clock) -> None:
  config = make_config(tmp_path / 'home')
  harness = Harness(store, forge, config, clock)
  await harness.start()

  # A new pull request: Test - Changes is served; CI hasn't run, so the
  # pull request hasn't settled and nothing is announced yet.
  head1 = origin.push_pull_request(1, PR_FILES)
  info1 = pull_request_info(1, head1, origin.main_sha)
  forge.set_pull_request(info1)
  await harness.step()
  review_id = await _review_id(store, 1)
  assert harness.publisher.served_names(review_id) == [CHANGES]
  assert harness.commenter.comments == []

  # The review is named after the pull request, which it links to.
  assert harness.publisher.names[review_id] == [
    rich_link(info1.url, f"{REPOSITORY}#1"),
    rich_text(f" {info1.title}"),
  ]

  # The base worktree only has what's needed: the touched test, not its
  # neighbours or the big unrelated directory.
  changes = harness.publisher.reviews[review_id][0]
  visible = (changes.root / 'visible.txt').read_text().split()
  assert 'tests/suite/a.sft.cpp' in visible
  assert 'tests/suite/.a.rto/default.1.1.txt' in visible
  assert not any(path.startswith('tests/big/') for path in visible)
  assert 'expectations/edg_x86_64/suite.log' in visible

  # CI finishes with unexpected differences: Test - Failures is added, and
  # both sections are announced in a single comment.
  artifact_zip = tmp_path / 'artifact-1.zip'
  make_test_artifact(artifact_zip, head1, ['suite/b.sft.cpp'])
  forge.artifact_files['a1'] = artifact_zip
  forge.runs[head1] = ci_run(head1, artifacts = (
    ArtifactInfo('test-results-all', 'a1', 100, 'sha256:aaa', False),
  ))
  await harness.step()
  assert harness.publisher.served_names(review_id) == [CHANGES, FAILURES]
  assert len(harness.commenter.comments) == 1
  body = harness.commenter.bodies()[0]
  assert f"[{CHANGES}](" in body and f"[{FAILURES}](" in body
  assert 'CI [failure]' in body

  # The failures were extracted into the head worktree, which has the
  # flagged test checked out.
  failures = next(
    s for s in harness.publisher.reviews[review_id] if s.name == FAILURES
  )
  assert (failures.root / 'edg_x86_64' / 'changes.elist').is_file()
  assert (failures.home / 'tests' / 'suite' / 'b.sft.cpp').is_file()
  assert not (failures.home / 'tests' / 'big').exists()

  # The author pushes again: Test - Changes is rebuilt; while CI runs, the
  # old failures stay up and nothing is announced.
  head2 = origin.push_pull_request(
    1,
    {'tests/tests/suite/.a.rto/default.1.1.txt': 'newer output\n'},
    parent = head1
  )
  forge.update_pull_request(1, head_sha = head2)
  forge.runs[head2] = ci_run(head2, run_id = '101', status = CIStatus.RUNNING)
  await harness.step()
  assert harness.publisher.served_names(review_id) == [CHANGES, FAILURES]
  new_changes = next(
    s for s in harness.publisher.reviews[review_id] if s.name == CHANGES
  )
  assert new_changes.content_fingerprint != changes.content_fingerprint
  assert not changes.root.exists(), 'superseded run dir was removed'
  assert len(harness.commenter.comments) == 1

  # CI passes: failures are removed, announced in one more comment.
  forge.runs[head2] = ci_run(
    head2,
    run_id = '101',
    conclusion = CIConclusion.SUCCESS
  )
  await harness.step()
  assert harness.publisher.served_names(review_id) == [CHANGES]
  assert len(harness.commenter.comments) == 2
  body = harness.commenter.bodies()[1]
  assert f"~~{FAILURES}~~" in body and '(updated)' in body
  assert not failures.root.exists()

  # A restart restores the same state and says nothing.
  restarted = Harness(store, forge, config, clock, commenter =
                      harness.commenter)
  await restarted.start()
  assert restarted.publisher.served_names(review_id) == [CHANGES]
  await restarted.step()
  assert len(harness.commenter.comments) == 2

  # Merging tears everything down without a comment, including the clone
  # (no other open pull request needs it).
  forge.update_pull_request(1, state = PullRequestState.MERGED)
  await restarted.step()
  assert restarted.publisher.served_names(review_id) == []
  assert not restarted.workspaces.review_dir(review_id).exists()
  assert not restarted.workspaces.clone_dir(REPOSITORY).exists()
  assert len(harness.commenter.comments) == 2

  # Reopening reuses the review id; the same results mean no comment.
  forge.update_pull_request(1, state = PullRequestState.OPEN)
  await restarted.step()
  assert await _review_id(store, 1) == review_id
  assert restarted.publisher.served_names(review_id) == [CHANGES]
  assert len(harness.commenter.comments) == 2


async def test_clone_shared_until_last_pull_request_closes(
                                                tmp_path: Path,
                                                origin: Origin,
                                                forge: FakeForge,
                                                store: ForgeStore,
                                                clock: Clock) -> None:
  config = make_config(tmp_path / 'home')
  harness = Harness(store, forge, config, clock)
  await harness.start()

  head1 = origin.push_pull_request(1, PR_FILES)
  head2 = origin.push_pull_request(2, {
    'tests/tests/suite/.b.rto/default.1.1.txt': 'changed\n',
  })
  forge.set_pull_request(pull_request_info(1, head1, origin.main_sha))
  forge.set_pull_request(pull_request_info(2, head2, origin.main_sha))
  await harness.step()
  review1 = await _review_id(store, 1)
  review2 = await _review_id(store, 2)
  assert harness.publisher.served_names(review1) == [CHANGES]
  assert harness.publisher.served_names(review2) == [CHANGES]

  forge.update_pull_request(1, state = PullRequestState.CLOSED)
  await harness.step()
  assert not harness.workspaces.review_dir(review1).exists()
  assert harness.workspaces.review_dir(review2).exists()
  assert harness.workspaces.clone_dir(REPOSITORY).exists()

  forge.update_pull_request(2, state = PullRequestState.CLOSED)
  await harness.step()
  assert not harness.workspaces.clone_dir(REPOSITORY).exists()


async def test_open_pull_request_without_sections_keeps_clone(
                                                tmp_path: Path,
                                                origin: Origin,
                                                forge: FakeForge,
                                                store: ForgeStore,
                                                clock: Clock) -> None:
  config = make_config(tmp_path / 'home')
  harness = Harness(store, forge, config, clock)
  await harness.start()

  # Pull request 1 changes no tests, so it has no worktree at all.
  head1 = origin.push_pull_request(1, {'README': 'docs only\n'})
  head2 = origin.push_pull_request(2, PR_FILES)
  forge.set_pull_request(pull_request_info(1, head1, origin.main_sha))
  forge.set_pull_request(pull_request_info(2, head2, origin.main_sha))
  await harness.step()
  assert harness.publisher.served_names(await _review_id(store, 1)) == []

  forge.update_pull_request(2, state = PullRequestState.CLOSED)
  await harness.step()
  assert harness.workspaces.clone_dir(REPOSITORY).exists()


async def test_missing_elist_serves_no_changes_section(
                                                tmp_path: Path,
                                                origin: Origin,
                                                forge: FakeForge,
                                                store: ForgeStore,
                                                clock: Clock) -> None:
  config = make_config(tmp_path / 'home')
  harness = Harness(store, forge, config, clock)
  await harness.start()
  origin.main_sha = origin.commit(
    'main',
    {'tests/.edgy/no-elist': ''},
    'mimic the real tool',
    parent = origin.main_sha
  )
  head = origin.push_pull_request(1, PR_FILES)
  forge.set_pull_request(pull_request_info(1, head, origin.main_sha))
  await harness.step()
  review_id = await _review_id(store, 1)
  assert harness.publisher.served_names(review_id) == []
  record = await store.find_pull_request('fake', REPOSITORY, 1)
  assert record is not None and not record.is_dirty
