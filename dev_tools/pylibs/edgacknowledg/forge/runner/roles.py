# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''What a runner allows (``RunnerPolicy``), and each role's: the builder's
and the tools runner's, the same with or without containers.'''

import logging
import os
import shutil

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Optional

_log = logging.getLogger('edgacknowledg.forge.runner.roles')

DEFAULT_PATH = '/usr/local/bin:/usr/bin:/bin'


@dataclass(frozen = True)
class RunnerPolicy:
  '''What a runner allows.

  - ``commands``: allowed argument prefixes. The first element of each is
    resolved on ``search_path`` when the runner starts; a request may name
    it either way, and the resolved one runs.
  - ``roots``: every absolute path in the request (arguments after the
    allowed prefix, the working directory, and variable values) must lie
    under one of them.
  - ``env_names``: variables a request may set.
  - ``max_timeout``: seconds a command may run at most.
  '''
  commands: tuple[tuple[str, ...], ...]
  roots: tuple[Path, ...]
  env_names: frozenset[str]
  max_timeout: float
  tools_dir: Optional[Path] = None

  @property
  def search_path(self) -> str:
    if self.tools_dir is None:
      return DEFAULT_PATH
    return f"{self.tools_dir / 'bin'}:{DEFAULT_PATH}"

  def base_env(self) -> dict[str, str]:
    env = {'PATH': self.search_path, 'HOME': '/tmp', 'LC_ALL': 'C'}
    if self.tools_dir is not None:
      env['PYTHONPATH'] = str(self.tools_dir / 'pylibs')
    return env

  def resolved_commands(self) -> list[tuple[tuple[str, ...], str]]:
    '''Each allowed prefix and the program its first element resolves to;
    one that isn't installed is left out (and so refused).
    '''
    resolved = []
    for prefix in self.commands:
      program = shutil.which(prefix[0], path = self.search_path)
      if program is None:
        _log.warning('%s was not found; requests for it are refused',
                     prefix[0])
        continue
      resolved.append((prefix, os.path.abspath(program)))
    return resolved

  def describe(self) -> dict[str, Any]:
    return {
      'commands': [list(prefix) for prefix in self.commands],
      'roots': [str(root) for root in self.roots],
      'env': sorted(self.env_names),
      'max_timeout': self.max_timeout,
    }


# The deployment's state directory layout (see ``workspace.Workspaces``):
# the review trees, and where builds write.
REVIEWS_DIR = 'reviews'
BUILD_STAGING_DIR = 'build-staging'

# Seconds a command may run at most, by role (the service's own limits, e.g.
# ACKNOWLEDG_FORGE_REMATERIALIZE_TIMEOUT, apply below these).
BUILD_MAX_TIMEOUT_S = 60 * 60.0
TOOLS_MAX_TIMEOUT_S = 120.0


def builder_policy(*, home: Path, tools_dir: Path) -> RunnerPolicy:
  '''The builder's: rebuild a section's test run from the pull request's
  diff, reading the review trees and writing only build staging.
  '''
  return RunnerPolicy(
    commands = (
      ('python3', str(tools_dir / 'bin' / 'edg-rematerialize-test-run')),
    ),
    roots = (home / REVIEWS_DIR, home / BUILD_STAGING_DIR),
    env_names = frozenset({'EDG_TEST_HOME'}),
    max_timeout = BUILD_MAX_TIMEOUT_S,
    tools_dir = tools_dir
  )


def tools_policy(*, home: Path, tools_dir: Path) -> RunnerPolicy:
  '''The tools runner's: the commands reviewers' requests run, on the
  review trees.
  '''
  return RunnerPolicy(
    commands = (('ifc-dump',), ('cg_annotate',)),
    roots = (home / REVIEWS_DIR,),
    env_names = frozenset(),
    max_timeout = TOOLS_MAX_TIMEOUT_S,
    tools_dir = tools_dir
  )


ROLES: dict[str, Callable[..., RunnerPolicy]] = {
  'builder': builder_policy,
  'tools': tools_policy,
}
