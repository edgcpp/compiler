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

from websockets.asyncio.server import Server

from edgacknowledg.forge.publisher.server import PublisherServer
from edgacknowledg.forge.runner.client import RunnerClient
from edgacknowledg.forge.runner.roles import ROLES
from edgacknowledg.forge.runner.server import RunnerServer

HOST = '127.0.0.1'


def _url(server: Server) -> str:
  port = next(iter(server.sockets)).getsockname()[1]
  return f"ws://{HOST}:{port}"


@contextlib.asynccontextmanager
async def _serving(server: Server) -> AsyncIterator[Server]:
  try:
    yield server
  finally:
    server.close()
    await server.wait_closed()


@dataclass(frozen = True)
class LocalDeployment:
  builder: RunnerClient
  tools: RunnerClient
  publisher_service_url: str
  publisher_server: PublisherServer


@contextlib.asynccontextmanager
async def local_runner(role: str,
                       *,
                       home: Path,
                       tools_dir: Path) -> AsyncIterator[RunnerClient]:
  '''Serve ``role``'s runner server (see ``runner.roles``) for the
  deployment in ``home``; yield its client.
  '''
  server = RunnerServer(ROLES[role](home = home, tools_dir = tools_dir))
  async with _serving(await server.start(HOST, 0)) as listening:
    client = RunnerClient(_url(listening))
    try:
      yield client
    finally:
      await client.close()


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
    publisher_server = PublisherServer(tools_service_url = tools.url)
    # Its pool stops once it stops listening.
    stack.push_async_callback(publisher_server.close)
    listening = await stack.enter_async_context(
      _serving(await publisher_server.start(HOST, 0))
    )
    yield LocalDeployment(
      builder = builder,
      tools = tools,
      publisher_service_url = _url(listening),
      publisher_server = publisher_server
    )
