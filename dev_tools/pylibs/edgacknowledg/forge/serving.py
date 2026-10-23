# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Serving every container's server the same way: a Quart app of the
server's blueprints, and the shared ``health`` one (``create_app``), run on
Hypercorn (``start``, ``serve``). The blueprints are the service's
(``webhooks``), the runner's (``runner.server``), and the publisher's
(``publisher.server``), whose WebSockets serve their client at ``/``.

``health`` answers a plain HTTP ``GET`` of ``HEALTH_PATH``, for
compose.yaml's healthchecks. For the WebSocket servers it is a route of its
own, so a probe never opens a connection.
'''

import asyncio
import contextlib
import socket

from http import HTTPStatus
from typing import Any, AsyncIterator, Optional

from hypercorn.asyncio import serve as hypercorn_serve
from hypercorn.config import Config
from quart import Blueprint, Quart

HEALTH_PATH = '/healthz'

# Seconds ``Listening.close`` waits for Hypercorn to stop, before stopping
# it anyway: its shutdown waits for every open connection, so a client that
# never ends its own would hold it up.
_CLOSE_TIMEOUT = 5.0

health = Blueprint('health', __name__)


@health.get(HEALTH_PATH)
async def healthz() -> Any:
  return 'ok\n', HTTPStatus.OK


def create_app(*blueprints: Blueprint) -> Quart:
  '''An app of ``blueprints``, answering health checks too.'''
  app = Quart(__name__)
  app.register_blueprint(health)
  for blueprint in blueprints:
    app.register_blueprint(blueprint)
  return app


class Listening:
  '''An app being served, until ``close``.'''

  def __init__(self,
               host: str,
               port: int,
               task: 'asyncio.Task[None]',
               stop: asyncio.Event) -> None:
    self.host = host
    self.port = port
    self._task = task
    self._stop = stop

  def url(self, scheme: str = 'ws') -> str:
    return f"{scheme}://{self.host}:{self.port}"

  async def wait(self) -> None:
    '''Until serving stops (with ``close``, or failing).'''
    await asyncio.shield(self._task)

  async def close(self) -> None:
    self._stop.set()
    try:
      await asyncio.wait_for(asyncio.shield(self._task), _CLOSE_TIMEOUT)
    except asyncio.TimeoutError:
      self._task.cancel()
      with contextlib.suppress(asyncio.CancelledError):
        await self._task


async def start(*blueprints: Blueprint,
                host: str,
                port: int,
                max_message_size: Optional[int] = None) -> Listening:
  '''Serve the app of ``blueprints`` on ``host`` and ``port`` (0: any free
  one, hence the socket made here: Hypercorn doesn't tell which), taking
  WebSocket messages of up to ``max_message_size`` bytes (default:
  Hypercorn's). Started (its blueprints' ``before_app_serving`` and
  ``while_app_serving``, e.g. the publisher's pool) and listening on
  return, so clients may connect at once.
  '''
  app = create_app(*blueprints)
  started = asyncio.Event()

  # Last, after the blueprints' own.
  @app.while_serving
  async def report_started() -> AsyncIterator[None]:
    started.set()
    yield

  family = socket.getaddrinfo(host, port, type = socket.SOCK_STREAM)[0][0]
  sock = socket.create_server((host, port), family = family)
  port = sock.getsockname()[1]
  config = Config()
  # Hypercorn takes the socket (and closes it).
  config.bind = [f"fd://{sock.detach()}"]
  if max_message_size is not None:
    config.websocket_max_message_size = max_message_size
  config.accesslog = None
  # Log through the process's logging setup only (not twice).
  config.errorlog = None
  # Without a shutdown trigger Hypercorn installs its own SIGINT/SIGTERM
  # handlers and shuts down alone; the caller stops it instead.
  stop = asyncio.Event()
  task = asyncio.create_task(hypercorn_serve(
    app,  # type: ignore[arg-type]
    config,
    shutdown_trigger = stop.wait
  ))
  # Until started, or failing to (e.g. Hypercorn's LifespanFailureError).
  starting = asyncio.create_task(started.wait())
  await asyncio.wait([starting, task], return_when = asyncio.FIRST_COMPLETED)
  starting.cancel()
  if task.done():
    task.result()
  return Listening(host, port, task, stop)


async def serve(*blueprints: Blueprint,
                host: str,
                port: int,
                max_message_size: Optional[int] = None) -> None:
  '''Serve the app of ``blueprints`` (as ``start``) until cancelled.'''
  listening = await start(*blueprints, host = host, port = port,
                          max_message_size = max_message_size)
  try:
    await listening.wait()
  finally:
    await listening.close()
