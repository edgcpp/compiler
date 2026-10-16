# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The process serving reviewers: an ``AcknowledgPublisher`` driven by
``SubprocessPublisher`` over stdin/stdout (see ``publisher`` for the
messages).

It answers reviewers' requests by reading pull request content, so the
service runs it sandboxed, away from its credentials, database, and clones.
'''

import asyncio
import json
import logging
import os
import sys
import uuid

from pathlib import Path
from typing import Any, BinaryIO, Optional

from edgacknowledg.forge.publisher import (
  MESSAGE_LIMIT,
  AcknowledgPublisher,
  PublisherUnavailableError,
  decode_review,
  decode_section,
)
from edgacknowledg.forge.sandbox import Sandbox

_log = logging.getLogger('edgacknowledg.forge.publisher_worker')


class _Output:
  '''Writes messages to the parent, one JSON object per line.'''

  def __init__(self, stream: BinaryIO) -> None:
    self._stream = stream

  def send(self, value: dict[str, Any]) -> None:
    self._stream.write((json.dumps(value) + '\n').encode())
    self._stream.flush()


class _WorkerPublisher(AcknowledgPublisher):
  '''Also reports the AcknowlEDG connection to the parent.'''

  def __init__(self, output: _Output, **kwargs: Any) -> None:
    super().__init__(**kwargs)
    self._output = output

  def connected(self) -> None:
    super().connected()
    self._output.send({'event': 'connected'})

  def disconnected(self) -> None:
    super().disconnected()
    self._output.send({'event': 'disconnected'})


async def _handle(publisher: AcknowledgPublisher,
                  request: dict[str, Any]) -> None:
  operation = request.get('op')
  if operation == 'sync':
    await publisher.sync(
      decode_review(request['review']),
      [decode_section(section) for section in request['sections']]
    )
  elif operation == 'remove':
    await publisher.remove(uuid.UUID(request['review_id']))
  else:
    raise ValueError(f"unknown operation: {operation!r}")


async def _serve_requests(reader: asyncio.StreamReader,
                          publisher: AcknowledgPublisher,
                          output: _Output) -> None:
  '''Answer requests, one at a time, until the parent closes stdin.'''
  while True:
    line = await reader.readline()
    if line == b'':
      return
    request = json.loads(line)
    reply: dict[str, Any] = {'id': request.get('id')}
    try:
      await _handle(publisher, request)
      reply['ok'] = True
    except PublisherUnavailableError as ex:
      reply.update(error = 'unavailable', message = str(ex))
    except Exception as ex:
      _log.exception('request %s failed', request.get('op'))
      reply.update(error = 'failed', message = f"{type(ex).__name__}: {ex}")
    output.send(reply)


async def _run(output: _Output) -> None:
  loop = asyncio.get_running_loop()
  reader = asyncio.StreamReader(limit = MESSAGE_LIMIT)
  await loop.connect_read_pipe(
    lambda: asyncio.StreamReaderProtocol(reader),
    sys.stdin
  )
  settings = json.loads(await reader.readline())
  sandbox_settings = settings['sandbox']
  tools_dir: Optional[str] = sandbox_settings['tools_dir']
  publisher = _WorkerPublisher(
    output,
    api_host = settings['api_host'],
    api_port = int(settings['api_port']),
    api_key = settings['api_key'],
    ui_host = settings['ui_host'],
    ui_port = int(settings['ui_port']),
    sandbox = Sandbox(
      enabled = bool(sandbox_settings['enabled']),
      tools_dir = Path(tools_dir) if tools_dir is not None else None,
      bwrap = sandbox_settings['bwrap']
    ),
    reviews_dir = Path(settings['reviews_dir'])
  )
  del settings

  publisher_task = asyncio.create_task(publisher.run())
  try:
    await _serve_requests(reader, publisher, output)
  finally:
    publisher_task.cancel()
    await asyncio.gather(publisher_task, return_exceptions = True)


def main() -> None:
  # Messages go out on a private copy of stdout; anything else printing to
  # stdout (the client libraries print) ends up on stderr, with the logs.
  output = _Output(os.fdopen(os.dup(sys.stdout.fileno()), 'wb'))
  os.dup2(sys.stderr.fileno(), sys.stdout.fileno())
  logging.basicConfig(
    level = logging.INFO,
    format = '%(levelname)s %(name)s (publisher worker): %(message)s'
  )
  try:
    asyncio.run(_run(output))
  except KeyboardInterrupt:
    pass


if __name__ == '__main__':
  main()
