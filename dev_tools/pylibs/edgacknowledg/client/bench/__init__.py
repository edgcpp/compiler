# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''AcknowlEDG benchmark-review provider package.'''

import asyncio
import os
import re
import shutil
import sys
import traceback

from typing import (
  Any,
  Dict,
  List,
  Optional,
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
  ConfinedPath,
  ConfinedPathComponent,
  IllegalFileAccessError,
  SandboxContext,
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
  def __init__(self, rel_benchmark_path: ConfinedPathComponent,
               baseline_ops: int, run_ops: int) -> None:
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


def read_program_totals(file_path: ConfinedPath) -> Optional[int]:
  '''Return the Ir total of the ``.cgout`` file ``file_path``, or None if
  it is missing or has none.
  '''
  if not file_path.exists():
    return None

  with file_path.open('rb') as file_handle:
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


def collect_benchmark_metadata(rel_benchmark_path: ConfinedPathComponent,
                               run_dir: ConfinedPath,
                               baseline_dir: ConfinedPath
                               ) -> Optional[BenchmarkMetadata]:
  baseline_ops = read_program_totals(baseline_dir / rel_benchmark_path)
  run_ops = read_program_totals(run_dir / rel_benchmark_path)

  if baseline_ops is None or run_ops is None:
    return None

  return BenchmarkMetadata(
    rel_benchmark_path,
    baseline_ops,
    run_ops
  )


def resolve_baseline_dir(bench_home: ConfinedPath, tag: str) -> ConfinedPath:
  '''The baselines of ``tag`` in ``bench_home``.'''
  if tag.startswith('@'):
    return bench_home / 'baselines' / 'user' / tag[1:]
  return bench_home / 'baselines' / 'project' / tag


def collect_all_benchmarks(run_dir: ConfinedPath,
                           baseline_dir: ConfinedPath
                           ) -> List[BenchmarkMetadata]:
  '''Collect every benchmark in ``run_dir`` with a baseline in
  ``baseline_dir``.
  '''
  benchmark_metadatas: List[BenchmarkMetadata] = []
  for root, dirs, files in run_dir.walk():
    for file_name in files:
      file_path = root / file_name
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


async def get_cg_annotate_output(context: SandboxContext,
                                 baseline_path: ConfinedPath,
                                 run_path: ConfinedPath) -> str:
  '''Return the ``cg_annotate`` diff of ``run_path`` against
  ``baseline_path``, run in ``context``.
  '''
  result = await context.run_command(
    str(TOOL_PATH_CG_ANNOTATE),
    '--no-annotate',
    '--diff',
    str(baseline_path),
    str(run_path)
  )
  if result.timed_out:
    return (
      f"cg_annotate timed out after {context.command_timeout:g} seconds\n"
    )
  return result.output.decode(errors = 'replace')


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
  '''Where one benchmark review section's runs and baselines are. The
  benchmarks' statuses are read from them each time a request needs them,
  not kept, so they always match the files.
  '''

  def __init__(self, *,
               context: SandboxContext,
               run_dir: ConfinedPath,
               baseline_dir: ConfinedPath,
               tag: str,
               threshold_slow: float = DEFAULT_THRESHOLD_SLOW,
               threshold_fast: float = DEFAULT_THRESHOLD_FAST) -> None:
    '''Every read and the baseline update go through ``run_dir`` and
    ``baseline_dir``; cg_annotate runs in ``context``.
    '''
    self.context = context
    self.run_dir = run_dir
    self.baseline_dir = baseline_dir
    self.tag = tag
    self.threshold_slow = threshold_slow
    self.threshold_fast = threshold_fast

  def statuses_payload(self) -> Dict[str, Any]:
    '''The statuses of every benchmark with a run and a baseline.'''
    return build_statuses_payload(
      collect_all_benchmarks(self.run_dir, self.baseline_dir),
      self.threshold_slow,
      self.threshold_fast,
      self.tag
    )

  def benchmark(self, bench_id: str) -> Optional[BenchmarkMetadata]:
    '''The benchmark ``bench_id`` names (its path in the run, as the
    statuses list it), if it has a run and a baseline. An id leading out of
    the run raises ``IllegalFileAccessError``.
    '''
    run_file = self.run_dir / bench_id
    if run_file.suffix != '.cgout':
      return None
    return collect_benchmark_metadata(
      run_file.relative_to(self.run_dir),
      self.run_dir,
      self.baseline_dir
    )

  def update_baseline(self, bench_id: str) -> bool:
    '''Copy the selected run ``.cgout`` into this session's baseline tag.'''
    metadata = self.benchmark(bench_id)
    if metadata is None:
      return False

    run_file = self.run_dir / metadata.rel_benchmark_path
    baseline_file = self.baseline_dir / metadata.rel_benchmark_path
    if not run_file.exists():
      return False

    baseline_file.path.parent.mkdir(parents = True, exist_ok = True)
    shutil.copy2(run_file.path, baseline_file.path)
    print(
      f"Updated baseline for {bench_id} under tag {self.tag}:\n"
      f"  {run_file} -> {baseline_file}"
    )
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
      statuses = await edgutil.async_to_thread(session.statuses_payload)
      await channel.send(message(
        RequestTags.BENCH_LIST_STATUSES,
        value = {
          'reviewer_id': reviewer_id,
          'client_response': statuses
        }
      ))
    elif request_tag == RequestTags.BENCH_SHOW_ANNOTATE:
      bench_id = request_value['id']
      metadata = await edgutil.async_to_thread(session.benchmark, bench_id)
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

      baseline_path = (
        session.baseline_dir / metadata.rel_benchmark_path
      )
      run_path = session.run_dir / metadata.rel_benchmark_path

      async def annotate_job() -> str:
        return await get_cg_annotate_output(
          session.context,
          baseline_path,
          run_path
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
    if isinstance(ex, IllegalFileAccessError):
      ex.reviewer_id = reviewer_id
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
