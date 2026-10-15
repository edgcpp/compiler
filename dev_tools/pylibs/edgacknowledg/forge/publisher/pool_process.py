# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''A pool process: answers reviewers' requests for the publisher server,
one at a time, over the socket whose file descriptor is its argument (see
``pool`` for the messages).

For each request it builds the section's handler (the stock providers from
``edgacknowledg.client``), has it answer the request, and drops it. The
handlers read pull request content without following links, only under the
section's tree, and run their commands (ifc-dump, cg_annotate) in the
tools runner that ``ACKNOWLEDG_FORGE_TOOLS_SERVICE_URL`` names.
'''

import argparse
import asyncio
import json
import logging
import os
import socket

from pathlib import Path
from typing import Any

from edgacknowledg import ProtocolMessage
from edgacknowledg.client.bench import BenchReviewSession, BenchSourceHandler
from edgacknowledg.client.shared import (
  BackgroundWorkerPool,
  IllegalFileAccessError,
  ReviewClientHandler,
)
from edgacknowledg.client.test import TestSourceHandler
from edgacknowledg.forge.config import ENV_PREFIX
from edgacknowledg.forge.db import SectionRecord
from edgacknowledg.forge.publisher import (
  KIND_BENCH,
  KIND_TEST_CHANGES,
  KIND_TEST_FAILURES,
)
from edgacknowledg.forge.publisher.protocol import MESSAGE_LIMIT, decode_section
from edgacknowledg.forge.runner.client import RunnerClient
from edgacknowledg.forge.sandbox import Sandbox

_log = logging.getLogger('edgacknowledg.forge.publisher.pool_process')

# Parallel cg_annotate runs in one process (it answers one request at a
# time, so this only matters to a request running several).
_BENCH_WORKERS = 2


def build_handler(section: SectionRecord,
                  sandbox: Sandbox,
                  bench_workers: BackgroundWorkerPool) -> ReviewClientHandler:
  '''The stock handler serving ``section``. Files are read without following
  links, and commands run on reviewers' behalf (ifc-dump, cg_annotate) only
  see the review trees, read-only.
  '''
  context = sandbox.context_for(section.home)
  if section.kind in (KIND_TEST_CHANGES, KIND_TEST_FAILURES):
    return TestSourceHandler(
      section.name,
      section.slug,
      context,
      context.subpath(section.home),
      context.subpath(section.root)
    )
  if section.kind == KIND_BENCH:
    session = BenchReviewSession(
      context = context,
      run_dir = context.subpath(section.root),
      baseline_dir = context.subpath(Path(section.details['baseline_dir'])),
      tag = section.details['tag']
    )
    return BenchSourceHandler(section.name, section.slug, session,
                              bench_workers)
  raise ValueError(f"unknown section kind: {section.kind}")


class _OneRequestChannel:
  '''Hands a handler one request, and keeps what it sends back.'''

  def __init__(self, request: Any) -> None:
    self._request = request
    self._received = False
    self.replies: list[ProtocolMessage] = []

  async def recv(self) -> Any:
    if self._received:
      raise RuntimeError('a handler asked for a second request')
    self._received = True
    return self._request

  async def send(self, msg: ProtocolMessage) -> None:
    self.replies.append(msg)


async def answer(request: dict[str, Any],
                 sandbox: Sandbox,
                 bench_workers: BackgroundWorkerPool) -> dict[str, Any]:
  '''Answer one request from the server.'''
  channel = _OneRequestChannel(request['request'])
  try:
    section = decode_section(request['section'])
    handler = build_handler(section, sandbox, bench_workers)
    await handler.handle_request(channel)  # type: ignore[arg-type]
  except IllegalFileAccessError as ex:
    return {
      'replies': channel.replies,
      'error': 'illegal-access',
      'message': str(ex),
      'reviewer_id': ex.reviewer_id,
    }
  except Exception as ex:
    _log.exception('a request failed')
    return {
      'replies': channel.replies,
      'error': 'failed',
      'message': f"{type(ex).__name__}: {ex}",
    }
  return {'replies': channel.replies}


async def _run(channel_fd: int) -> None:
  sock = socket.socket(fileno = channel_fd)
  # Commands run for reviewers must not reach the server.
  sock.set_inheritable(False)
  messages, channel = await asyncio.open_unix_connection(
    sock = sock,
    limit = MESSAGE_LIMIT
  )
  # One connection to the tools runner, for every command this process
  # runs.
  tools = RunnerClient(os.environ.get(f"{ENV_PREFIX}TOOLS_SERVICE_URL", ''))
  sandbox = Sandbox(tools = tools)
  bench_workers = BackgroundWorkerPool(_BENCH_WORKERS)
  try:
    channel.write(b'{"ready": true}\n')
    await channel.drain()
    while True:
      line = await messages.readline()
      if line == b'':
        return
      reply = await answer(json.loads(line), sandbox, bench_workers)
      channel.write((json.dumps(reply) + '\n').encode())
      await channel.drain()
  finally:
    channel.close()
    await tools.close()


def main() -> None:
  parser = argparse.ArgumentParser(
    description = 'Answer reviewers\' requests for the publisher server.'
  )
  parser.add_argument(
    'channel_fd',
    type = int,
    help = 'the inherited socket to the server'
  )
  args = parser.parse_args()
  logging.basicConfig(
    level = logging.INFO,
    format = '%(levelname)s %(name)s (pool process): %(message)s'
  )
  try:
    asyncio.run(_run(args.channel_fd))
  except KeyboardInterrupt:
    pass


if __name__ == '__main__':
  main()
