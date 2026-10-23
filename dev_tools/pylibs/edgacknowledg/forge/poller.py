# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Polling: catch up at startup, then (optionally) keep checking.'''

import asyncio
import logging

from edgacknowledg.forge.config import ForgeServiceConfig
from edgacknowledg.forge.ingest import Ingestor

_log = logging.getLogger(__name__)


async def sync_all(ingestor: Ingestor, config: ForgeServiceConfig) -> None:
  '''Sync every configured repository once; failures are logged per
  repository so one bad repository doesn't stall the others.
  '''
  for repository in config.repositories:
    try:
      await ingestor.sync_repository(repository)
    except asyncio.CancelledError:
      raise
    except Exception:
      _log.exception('failed to sync %s', repository)


async def run_poller(ingestor: Ingestor, config: ForgeServiceConfig) -> None:
  '''Sync at startup, which picks up anything missed while the service was
  down, then every ``poll_interval`` seconds (never, if it's 0).
  '''
  while True:
    _log.info('syncing %d repositories', len(config.repositories))
    await sync_all(ingestor, config)
    if config.poll_interval <= 0:
      _log.info('polling disabled after the startup sync')
      return
    await asyncio.sleep(config.poll_interval)
