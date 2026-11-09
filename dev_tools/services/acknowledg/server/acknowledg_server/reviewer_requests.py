# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Serde models for reviewer -> server request payloads.

These models define the expected ``v`` (value) shape for each reviewer request
tag. Deserialization with ``deny_unknown_fields`` rejects junk at the review
boundary before anything is forwarded to a source/edit provider.
'''

from typing import List, Optional

from serde import serde  # type: ignore[import-not-found]

from edgacknowledg import (
  ALLOWED_REVIEWER_REQUEST_TAGS,
  MAX_SOURCE_RANGE_CONTEXT,
  RequestTags
)

@serde(deny_unknown_fields = True)
class EmptyRequest:
  '''Request with no value payload (``v`` is null/absent or ``{}``).'''
  pass

@serde(transparent = True)
class TestCaseId:
  '''Bare string identifying a configured test case.'''
  value: str

@serde(transparent = True)
class BenchId:
  '''Bare string identifying a benchmark.'''
  value: str

@serde(deny_unknown_fields = True)
class ShowTestSourceRequest:
  '''Request to display a test's primary source or an associated file.

  ``line`` and ``context`` (both or neither) restrict the reply to the lines
  ``line - context`` through ``line + context`` (``line`` is 1-based).
  '''
  id: str
  assoc_file: Optional[str] = None
  line: Optional[int] = None
  context: Optional[int] = None

  def is_valid_range(self) -> bool:
    if self.line is None and self.context is None:
      return True
    # Exact type checks: ``bool`` is an ``int`` subclass, so pyserde's
    # strict mode lets ``true`` through as 1.
    return (
      type(self.line) is int and type(self.context) is int and
      self.line >= 1 and 0 <= self.context <= MAX_SOURCE_RANGE_CONTEXT
    )

@serde(deny_unknown_fields = True)
class ShowCurrDiffRequest:
  '''Request a live curr-diff for a representative configured id.

  ``configs`` lists the full diff-group membership so the source can check
  whether sibling curr-diffs still match the representative.
  '''
  id: str
  configs: List[str]

@serde(deny_unknown_fields = True)
class UpdateDiffRequest:
  '''Request a recording update for one test across one or more configs.'''
  id: str
  configs: List[str]

# Map each allowed reviewer request tag to its value model.
REVIEWER_REQUEST_VALUE_MODELS: dict[str, type] = {
  RequestTags.TEST_LIST_STATUSES: EmptyRequest,
  RequestTags.TEST_SHOW_DIFF: TestCaseId,
  RequestTags.TEST_SHOW_CURR_DIFF: ShowCurrDiffRequest,
  RequestTags.TEST_SHOW_RAW_OUTPUT: TestCaseId,
  RequestTags.TEST_LIST_ASSOC_FILES: TestCaseId,
  RequestTags.TEST_SHOW_SOURCE: ShowTestSourceRequest,
  RequestTags.TEST_UPDATE_DIFF: UpdateDiffRequest,
  RequestTags.BENCH_LIST_STATUSES: EmptyRequest,
  RequestTags.BENCH_SHOW_ANNOTATE: BenchId,
  RequestTags.BENCH_UPDATE_BASELINE: BenchId,
}

assert set(REVIEWER_REQUEST_VALUE_MODELS) == ALLOWED_REVIEWER_REQUEST_TAGS
