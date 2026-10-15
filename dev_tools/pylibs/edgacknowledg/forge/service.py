# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Composition and entry point shared by every forge service.

A forge service supplies its ``Forge`` (and optionally ``ForgeWebhooks`` and
``ForgeCommenter``) via ``main``; this module runs the rest:

- the publisher client (serving reviewers happens in the publisher
  container),
- without containers (local development only), the servers the builder,
  tools, and publisher containers run (``local``),
- the processor (and, if enabled, the announcer after each sweep),
- the poller (always: at least the startup sync),
- the webhook receiver and its event worker (only if configured).
'''

import asyncio
import contextlib
import logging
import signal
import sys

from dataclasses import dataclass
from typing import Callable, Optional

from edgacknowledg.forge import Forge, ForgeCommenter, ForgeWebhooks
from edgacknowledg.forge.announcer import Announcer
from edgacknowledg.forge.config import ConfigError, ForgeServiceConfig
from edgacknowledg.forge.db import ForgeStore
from edgacknowledg.forge.ingest import Ingestor
from edgacknowledg.forge.local import local_deployment
from edgacknowledg.forge.poller import run_poller
from edgacknowledg.forge.processor import Processor
from edgacknowledg.forge.publisher.client import PublisherClient
from edgacknowledg.forge.sandbox import (
  RunnerClient,
  Sandbox,
  SandboxUnavailableError,
)
from edgacknowledg.forge.webhooks import EventQueue, create_app
from edgacknowledg.forge.workspace import Workspaces

_log = logging.getLogger(__name__)


@dataclass
class ForgeComponents:
  '''What a forge implementation provides.'''
  forge: Forge
  webhooks: Optional[ForgeWebhooks] = None
  commenter: Optional[ForgeCommenter] = None


async def _serve_webhooks(app: object, bind: str) -> None:
  # Only needed (and imported) when webhooks are enabled.
  from hypercorn.asyncio import serve
  from hypercorn.config import Config

  hypercorn_config = Config()
  hypercorn_config.bind = [bind]
  hypercorn_config.accesslog = None
  # Log through the service's logging setup only (not twice).
  hypercorn_config.errorlog = None
  # Without a shutdown trigger Hypercorn installs its own SIGINT/SIGTERM
  # handlers and shuts down alone; the service cancels it with everything
  # else instead.
  never = asyncio.Event()
  await serve(
    app,  # type: ignore[arg-type]
    hypercorn_config,
    shutdown_trigger = never.wait
  )


async def run_service(config: ForgeServiceConfig,
                      components: ForgeComponents) -> None:
  if config.webhook_bind is not None and components.webhooks is None:
    raise ConfigError(
      'ACKNOWLEDG_FORGE_WEBHOOK_BIND is set, but webhooks are not '
      'configured (is the webhook secret set?)'
    )
  if config.comments and components.commenter is None:
    raise ConfigError(
      'ACKNOWLEDG_FORGE_COMMENTS is set, but commenting is not configured '
      '(is the forge token set?)'
    )

  config.home.mkdir(parents = True, exist_ok = True)
  async with contextlib.AsyncExitStack() as stack:
    if config.unsandboxed:
      _log.warning('running without containers: nothing confines what reads '
                   'pull requests; for local development only')
      local = await stack.enter_async_context(
        local_deployment(home = config.home, tools_dir = config.tools_dir)
      )
      builder = local.builder
      publisher_service_url = local.publisher_service_url
    else:
      assert config.builder_service_url is not None
      assert config.publisher_service_url is not None
      builder = RunnerClient(config.builder_service_url)
      stack.push_async_callback(builder.close)
      publisher_service_url = config.publisher_service_url
      if len(config.environment_secrets) != 0:
        _log.warning('secrets come from the environment (%s); use '
                     'credentials in production',
                     ', '.join(config.environment_secrets))
    await _run_components(config, components, builder, publisher_service_url)


async def _run_components(config: ForgeServiceConfig,
                          components: ForgeComponents,
                          builder: RunnerClient,
                          publisher_service_url: str) -> None:
  forge = components.forge
  sandbox = Sandbox(builder = builder)
  await sandbox.check()

  store = await ForgeStore.open(config.database_url)
  try:
    wake = asyncio.Event()
    ingestor = Ingestor(store, forge, config, wake = wake)
    workspaces = Workspaces(config.home, forge)
    # Serving reviewers happens in the publisher container.
    publisher = PublisherClient(
      url = publisher_service_url,
      api_host = config.acknowledg_api_host,
      api_port = config.acknowledg_api_port,
      api_key = config.acknowledg_api_key,
      ui_host = config.acknowledg_ui_host,
      ui_port = config.acknowledg_ui_port,
      reviews_dir = workspaces.reviews_dir
    )

    processor: Optional[Processor] = None
    announcer: Optional[Announcer] = None
    if config.comments and components.commenter is not None:
      announcer = Announcer(
        store = store,
        forge_name = forge.name,
        commenter = components.commenter,
        config = config,
        publisher = publisher,
        busy = lambda: (
          processor.busy_pull_requests if processor is not None
          else frozenset()
        )
      )
      await announcer.start()
    processor = Processor(
      store = store,
      forge = forge,
      config = config,
      workspaces = workspaces,
      sandbox = sandbox,
      publisher = publisher,
      wake = wake,
      after_sweep = announcer.sweep if announcer is not None else None
    )

    async with asyncio.TaskGroup() as task_group:
      task_group.create_task(publisher.run())
      task_group.create_task(processor.run())
      task_group.create_task(run_poller(ingestor, config))
      if config.webhook_bind is not None:
        assert components.webhooks is not None
        events = EventQueue()
        task_group.create_task(events.run(ingestor))
        task_group.create_task(_serve_webhooks(
          create_app(components.webhooks, events),
          config.webhook_bind
        ))
        _log.info('receiving webhooks on %s', config.webhook_bind)
  finally:
    await forge.aclose()
    await store.aclose()


def main(build: Callable[[ForgeServiceConfig], ForgeComponents]) -> None:
  '''Entry point for a forge service: read the configuration, build the
  forge components with ``build``, and run until SIGINT/SIGTERM.
  '''
  logging.basicConfig(
    level = logging.INFO,
    format = '%(levelname)s %(name)s: %(message)s'
  )
  logging.getLogger('alembic').setLevel(logging.WARNING)
  try:
    config = ForgeServiceConfig.from_environment()
    components = build(config)
  except ConfigError as ex:
    print(f"configuration error: {ex}", file = sys.stderr)
    sys.exit(2)

  async def run() -> None:
    task = asyncio.current_task()
    assert task is not None
    loop = asyncio.get_running_loop()
    for signal_number in (signal.SIGINT, signal.SIGTERM):
      loop.add_signal_handler(signal_number, task.cancel)
    try:
      await run_service(config, components)
    except asyncio.CancelledError:
      _log.info('shutting down')

  try:
    asyncio.run(run())
  except (ConfigError, SandboxUnavailableError) as ex:
    print(f"error: {ex}", file = sys.stderr)
    sys.exit(2)
