# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''AcknowlEDG benchmark-review provider package.'''

import asyncio
import os
import re
import shutil
import subprocess
import sys
import traceback

from pathlib import Path
from typing import (
  Any,
  Dict,
  List,
  Optional,
  Sequence,
)

from edgacknowledg import (
  ProtocolMessage,
  RequestTags,
  WebSocketsMessageType,
  message,
  decode_response,
)
from edgacknowledg.client.shared import (
  BackgroundWorkerPool,
  Channel,
  trunc_display_content,
)
import edgutil
from edgutil import eprint

TOOL_PATH_CG_ANNOTATE = edgutil.RequiredToolPath('cg_annotate')

DEFAULT_THRESHOLD_SLOW = 0.01
DEFAULT_THRESHOLD_FAST = 0.01

# Cachegrind may list only Ir, or Ir plus cache-sim events. Always take Ir
# (the first summary count).
_REGEX_CGOUT_SUMMARY = re.compile(r'^summary: ([0-9]+)(?: [0-9]+)*\n$')


def _compute_percent_change(baseline_ops: int, run_ops: int) -> float:
  return ((baseline_ops - run_ops) / max(1, baseline_ops)) * 100


class BenchmarkMetadata:
  def __init__(self, rel_benchmark_path: Path, baseline_ops: int,
               run_ops: int) -> None:
    self.rel_benchmark_path = rel_benchmark_path
    self.baseline_ops = baseline_ops
    self.run_ops = run_ops

  def is_faster(self) -> bool:
    return self.run_ops <= self.baseline_ops

  def percent_change(self) -> float:
    return _compute_percent_change(self.baseline_ops, self.run_ops)

  def to_status_dict(self) -> Dict[str, Any]:
    return {
      'id': str(self.rel_benchmark_path),
      'baseline_ops': self.baseline_ops,
      'run_ops': self.run_ops
    }


def read_program_totals(file_path: Path) -> Optional[int]:
  if not file_path.exists():
    return None

  with open(file_path, 'rb') as file_handle:
    try:
      # Advance to the last character of the file.
      file_handle.seek(-2, os.SEEK_END)

      # Check up to the last 10 lines.
      for _ in range(10):
        while file_handle.read(1) != b'\n':
          file_handle.seek(-2, os.SEEK_CUR)

        curr_pos = file_handle.tell()
        last_line = file_handle.readline().decode()
        re_match = _REGEX_CGOUT_SUMMARY.match(last_line)
        if re_match is not None:
          return int(re_match.group(1))

        file_handle.seek(curr_pos - 2, os.SEEK_SET)
    except OSError:
      pass

    return None


def collect_benchmark_metadata(rel_benchmark_path: Path, run_dir: Path,
                               baseline_dir: Path
                               ) -> Optional[BenchmarkMetadata]:
  baseline_file = baseline_dir / rel_benchmark_path
  run_file = run_dir / rel_benchmark_path

  baseline_ops = read_program_totals(baseline_file)
  run_ops = read_program_totals(run_file)

  if baseline_ops is None or run_ops is None:
    return None

  return BenchmarkMetadata(
    rel_benchmark_path,
    baseline_ops,
    run_ops
  )


def resolve_baseline_dir(bench_home: Path, tag: str) -> Path:
  if tag.startswith('@'):
    return bench_home / 'baselines' / 'user' / tag[1:]
  return bench_home / 'baselines' / 'project' / tag


def collect_all_benchmarks(run_dir: Path,
                           baseline_dir: Path) -> List[BenchmarkMetadata]:
  benchmark_metadatas: List[BenchmarkMetadata] = []
  for root, dirs, files in os.walk(run_dir):
    for file_name in files:
      file_path = Path(root, file_name)
      if file_path.suffix != '.cgout':
        continue

      rel_benchmark_path = file_path.relative_to(run_dir)
      benchmark_metadata = collect_benchmark_metadata(
        rel_benchmark_path,
        run_dir,
        baseline_dir
      )
      if benchmark_metadata is None:
        continue
      benchmark_metadatas.append(benchmark_metadata)
  return benchmark_metadatas


def get_cg_annotate_output(baseline_path: Path,
                           run_path: Path,
                           *,
                           command_prefix: Sequence[str] = ()) -> str:
  '''Return the ``cg_annotate`` diff of ``run_path`` against
  ``baseline_path``, with ``command_prefix`` (e.g. a sandbox launcher)
  prepended to the command.
  '''
  proc = subprocess.run(
    [
      *command_prefix,
      str(TOOL_PATH_CG_ANNOTATE),
      '--no-annotate',
      '--diff',
      str(baseline_path),
      str(run_path)
    ],
    stdout = subprocess.PIPE,
    stderr = subprocess.STDOUT,
    check = False
  )
  return proc.stdout.decode(errors = 'replace')


def build_statuses_payload(benchmark_metadatas: List[BenchmarkMetadata],
                           threshold_slow: float,
                           threshold_fast: float,
                           baseline_tag: str) -> Dict[str, Any]:
  def is_within_reporting_threshold(benchmark: BenchmarkMetadata) -> bool:
    if benchmark.is_faster():
      return benchmark.percent_change() >= threshold_fast
    return abs(benchmark.percent_change()) >= threshold_slow

  total_baseline_ops = 0
  total_run_ops = 0
  for benchmark_metadata in benchmark_metadatas:
    total_baseline_ops += benchmark_metadata.baseline_ops
    total_run_ops += benchmark_metadata.run_ops

  # Order is decided by the UI; keep a stable path order on the wire.
  reported = [
    bm.to_status_dict()
    for bm in sorted(
      benchmark_metadatas,
      key = lambda bm: str(bm.rel_benchmark_path)
    )
    if is_within_reporting_threshold(bm)
  ]

  return {
    'summary': {
      'baseline': baseline_tag,
      'baseline_ops': total_baseline_ops,
      'run_ops': total_run_ops
    },
    'benchmarks': reported
  }


class BenchReviewSession:
  '''Shared source/editor state for one benchmark review section.'''

  def __init__(self, *,
               bench_home: Path,
               run_dir: Path,
               baseline_dir: Path,
               tag: str,
               threshold_slow: float = DEFAULT_THRESHOLD_SLOW,
               threshold_fast: float = DEFAULT_THRESHOLD_FAST,
               command_prefix: Sequence[str] = ()) -> None:
    self.bench_home = bench_home
    self.run_dir = run_dir
    self.baseline_dir = baseline_dir
    self.tag = tag
    self.threshold_slow = threshold_slow
    self.threshold_fast = threshold_fast
    # Prepended to external commands run on the benchmark files.
    self.command_prefix = tuple(command_prefix)
    self.statuses_payload: Dict[str, Any] = {}
    self.metadata_by_id: Dict[str, BenchmarkMetadata] = {}
    self.refresh_statuses()

  def refresh_statuses(self) -> None:
    all_benchmarks = collect_all_benchmarks(self.run_dir, self.baseline_dir)
    self.statuses_payload = build_statuses_payload(
      all_benchmarks,
      self.threshold_slow,
      self.threshold_fast,
      self.tag
    )
    self.metadata_by_id = {
      str(bm.rel_benchmark_path): bm for bm in all_benchmarks
    }

  def update_baseline(self, bench_id: str) -> bool:
    '''Copy the selected run ``.cgout`` into this session's baseline tag.'''
    metadata = self.metadata_by_id.get(bench_id)
    if metadata is None:
      return False

    run_file = self.run_dir / metadata.rel_benchmark_path
    baseline_file = self.baseline_dir / metadata.rel_benchmark_path
    if not run_file.exists():
      return False

    baseline_file.parent.mkdir(parents = True, exist_ok = True)
    shutil.copy2(run_file, baseline_file)
    print(
      f"Updated baseline for {bench_id} under tag {self.tag}:\n"
      f"  {run_file} -> {baseline_file}"
    )
    self.refresh_statuses()
    return True

async def run_bench_source_handler(channel: Channel,
                                   session: BenchReviewSession,
                                   worker_pool: BackgroundWorkerPool
                                   ) -> None:
  '''Handle one benchmark source request on ``channel``.'''
  request_tag, request_value, _section = decode_response(await channel.recv())

  if request_value is None or 'reviewer_id' not in request_value:
    await channel.send(message(RequestTags.INVALID_REQUEST))
    return

  reviewer_id = request_value['reviewer_id']

  try:
    if request_tag == RequestTags.BENCH_LIST_STATUSES:
      await channel.send(message(
        RequestTags.BENCH_LIST_STATUSES,
        value = {
          'reviewer_id': reviewer_id,
          'client_response': session.statuses_payload
        }
      ))
    elif request_tag == RequestTags.BENCH_SHOW_ANNOTATE:
      bench_id = request_value['id']
      metadata = session.metadata_by_id.get(bench_id)
      if metadata is None:
        await channel.send(message(
          RequestTags.BENCH_SHOW_ANNOTATE,
          value = {
            'reviewer_id': reviewer_id,
            'client_response': {
              'id': bench_id,
              'content': None
            }
          }
        ))
        return

      baseline_path = session.baseline_dir / metadata.rel_benchmark_path
      run_path = session.run_dir / metadata.rel_benchmark_path

      async def annotate_job() -> str:
        return await edgutil.async_to_thread(
          get_cg_annotate_output,
          baseline_path,
          run_path,
          command_prefix = session.command_prefix
        )

      annotate_text = await worker_pool.submit(annotate_job)
      await channel.send(message(
        RequestTags.BENCH_SHOW_ANNOTATE,
        value = {
          'reviewer_id': reviewer_id,
          'client_response': {
            'id': bench_id,
            **trunc_display_content(annotate_text)
          }
        }
      ))
    else:
      await channel.send(message(
        RequestTags.INVALID_REQUEST,
        value = { 'reviewer_id': reviewer_id }
      ))
  except Exception as ex:
    if sys.version_info >= (3, 11):
      ex.add_note(f"request triggered by reviewer: \"{reviewer_id}\"")
    raise ex


async def handle_bench_edit_request(session: BenchReviewSession,
                                    request: WebSocketsMessageType
                                    ) -> ProtocolMessage:
  '''Handle one benchmark edit request; return a protocol reply message.'''
  request_tag, request_value, _section = decode_response(request)

  if request_tag == RequestTags.BENCH_UPDATE_BASELINE:
    bench_id = request_value['id']
    reviewer_id = request_value['reviewer_id']
    try:
      success = session.update_baseline(bench_id)
      return message(
        RequestTags.BENCH_UPDATE_BASELINE,
        value = {
          'id': bench_id,
          'reviewer_id': reviewer_id,
          'success': success
        }
      )
    except Exception as ex:
      if sys.version_info >= (3, 10):
        traceback.print_exception(ex)
      else:
        eprint(str(ex))
      return message(
        RequestTags.BENCH_UPDATE_BASELINE,
        value = {
          'id': bench_id,
          'reviewer_id': reviewer_id,
          'success': False
        }
      )

  return message(RequestTags.INVALID_REQUEST)


class BenchSourceHandler:
  def __init__(self,
               section_name: str,
               slug: str,
               session: BenchReviewSession,
               worker_pool: BackgroundWorkerPool) -> None:
    self._section_name = section_name
    self._slug = slug
    self._session = session
    self._worker_pool = worker_pool

  @property
  def subscription_type(self) -> str:
    return 'bench.src'

  @property
  def section_name(self) -> str:
    return self._section_name

  @property
  def slug(self) -> str:
    return self._slug


  async def handle_request(self, channel: Channel) -> None:
    await run_bench_source_handler(
      channel,
      self._session,
      self._worker_pool
    )


class BenchEditHandler:
  def __init__(self,
               section_name: str,
               slug: str,
               session: BenchReviewSession,
               worker_pool: BackgroundWorkerPool) -> None:
    self._section_name = section_name
    self._slug = slug
    self._session = session
    self._worker_pool = worker_pool

  @property
  def subscription_type(self) -> str:
    return 'bench.edit'

  @property
  def section_name(self) -> str:
    return self._section_name

  @property
  def slug(self) -> str:
    return self._slug


  async def handle_request(self, channel: Channel) -> None:
    request = await channel.recv()

    async def edit_job() -> None:
      reply = await handle_bench_edit_request(self._session, request)
      await channel.send(reply)

    await self._worker_pool.submit(edit_job)
