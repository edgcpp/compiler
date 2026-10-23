# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Without containers (``ACKNOWLEDG_FORGE_UNSANDBOXED=1``, for local
development only): the servers the builder, tools, and publisher containers
run, served from the service's own process instead.

The service then works exactly as it does with containers: builds go to the
builder's runner server, publishing to the publisher server (and its pool
processes), whose reviewer commands go to the tools runner server, each over
a WebSocket, and the runners allow the same commands and paths
(``runner.roles``). What differs is only what the containers add. Here
every command, and the publisher, sees and can reach everything the service
can; and the servers listen on 127.0.0.1, where any process on the machine
can reach them, which the containers' networks otherwise prevent. So use it
on a single-user machine only.
'''

import contextlib

from dataclasses import dataclass
from pathlib import Path
from typing import AsyncIterator

from quart import Blueprint

from edgacknowledg.forge import serving
from edgacknowledg.forge.publisher.protocol import MESSAGE_LIMIT
from edgacknowledg.forge.publisher.server import create_publisher
from edgacknowledg.forge.runner.client import RunnerClient
from edgacknowledg.forge.runner.roles import ROLES
from edgacknowledg.forge.runner.server import MAX_REQUEST_BYTES, create_runner
from edgacknowledg.forge.serving import Listening

HOST = '127.0.0.1'


@contextlib.asynccontextmanager
async def _serving(server: Blueprint,
                   max_message_size: int) -> AsyncIterator[Listening]:
  listening = await serving.start(server, host = HOST, port = 0,
                                  max_message_size = max_message_size)
  try:
    yield listening
  finally:
    await listening.close()


@dataclass(frozen = True)
class LocalDeployment:
  builder: RunnerClient
  tools: RunnerClient
  publisher_service_url: str


@contextlib.asynccontextmanager
async def local_runner(role: str,
                       *,
                       home: Path,
                       tools_dir: Path) -> AsyncIterator[RunnerClient]:
  '''Serve ``role``'s runner server (see ``runner.roles``) for the
  deployment in ``home``; yield its client.
  '''
  runner = create_runner(ROLES[role](home = home, tools_dir = tools_dir))
  async with (_serving(runner, MAX_REQUEST_BYTES) as listening,
              RunnerClient(listening.url()) as client):
    yield client


@contextlib.asynccontextmanager
async def local_deployment(*,
                           home: Path,
                           tools_dir: Path) -> AsyncIterator[LocalDeployment]:
  '''Serve the builder and tools runner servers and the publisher server
  for the deployment in ``home``, until the context exits.
  '''
  async with contextlib.AsyncExitStack() as stack:
    builder = await stack.enter_async_context(
      local_runner('builder', home = home, tools_dir = tools_dir)
    )
    tools = await stack.enter_async_context(
      local_runner('tools', home = home, tools_dir = tools_dir)
    )
    # Its pool runs while it serves.
    listening = await stack.enter_async_context(_serving(
      create_publisher(tools_service_url = tools.url),
      MESSAGE_LIMIT
    ))
    yield LocalDeployment(
      builder = builder,
      tools = tools,
      publisher_service_url = listening.url()
    )
