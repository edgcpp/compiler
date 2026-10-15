# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Run commands over untrusted pull request content in other containers.

Commands that read pull request content never run in the service's
container (or the publisher's): they run in a runner container (a runner
server, ``runner.server``) that sees only the review trees, read-only (and,
for builds, a staging directory to write), with no network, no
capabilities, and a cleared environment, so neither the service's secrets
nor the rest of the host are reachable from them. Their /tmp is a bounded
tmpfs.

- Builds (``run``) go to the builder runner, which only the service
  reaches.
- Reviewer commands (through the ``SandboxContext`` of ``context_for``)
  go to the tools runner, which only the publisher reaches.

Without containers (local development only), the same runners serve from
the service's process (``local``): commands go the same way, but nothing
confines them.
'''

from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Optional, Sequence

from edgacknowledg.client.shared import SandboxContext
from edgacknowledg.forge.runner.client import RunnerClient, RunnerError

__all__ = [
  'TMPFS_SIZE',
  'CommandResult',
  'RunnerClient',
  'Sandbox',
  'SandboxUnavailableError',
]

# Bytes a runner container's /tmp may hold (compose.yaml mounts a tmpfs this
# size). It's memory; bounding it keeps untrusted content (copied there to
# be diffed, say) from exhausting the host's.
TMPFS_SIZE = 512 * 1024 * 1024


class SandboxUnavailableError(Exception):
  '''Raised when sandboxing is required but its runners can't be used.'''


@dataclass(frozen = True)
class CommandResult:
  '''How a build ended, with its output as text.'''
  returncode: int
  output: str
  timed_out: bool

  @property
  def succeeded(self) -> bool:
    return self.returncode == 0 and not self.timed_out


class Sandbox:
  '''Runs sandboxed commands through this process's runner clients:
  ``builder`` (the service's) and ``tools`` (the publisher's).
  '''

  def __init__(self,
               *,
               builder: Optional[RunnerClient] = None,
               tools: Optional[RunnerClient] = None) -> None:
    self._builder = builder
    self._tools = tools
    # What each runner serves, once checked.
    self._roots: dict[RunnerClient, tuple[Path, ...]] = {}

  @property
  def builder(self) -> Optional[RunnerClient]:
    return self._builder

  @property
  def tools(self) -> Optional[RunnerClient]:
    return self._tools

  async def check(self) -> None:
    '''Raise ``SandboxUnavailableError`` unless this process's runners
    answer.
    '''
    clients = [c for c in (self._builder, self._tools) if c is not None]
    if len(clients) == 0:
      raise SandboxUnavailableError('no runner is configured')
    for client in clients:
      try:
        description = await client.describe()
      except RunnerError as ex:
        raise SandboxUnavailableError(str(ex)) from ex
      self._roots[client] = tuple(
        Path(root) for root in description.get('roots', ())
      )

  def _require(self,
               client: Optional[RunnerClient],
               name: str,
               paths: Sequence[Path]) -> RunnerClient:
    '''``client``, whose runner must serve every one of ``paths``.'''
    if client is None:
      raise SandboxUnavailableError(f"no {name} runner is configured")
    roots = self._roots.get(client)
    if roots is not None:
      for path in paths:
        if not any(path.is_relative_to(root) for root in roots):
          raise SandboxUnavailableError(
            f"the {name} runner doesn't serve {path}"
          )
    return client

  def context_for(self, *roots: Path) -> SandboxContext:
    '''The context for serving the pull request content under ``roots``:
    no symlink in it is followed, and commands run on it run in the tools
    runner, which sees only the review trees, read-only.
    '''
    context = SandboxContext(*roots, allow_symlinks = False)
    tools = self._require(self._tools, 'tools', context.roots)
    return SandboxContext(
      *context.roots,
      allow_symlinks = False,
      command_runner = tools.run_command
    )

  async def run(self,
                command: Sequence[str],
                *,
                read_only: Sequence[Path] = (),
                writable: Sequence[Path] = (),
                env: Mapping[str, str] = {},
                cwd: Optional[Path] = None,
                timeout: float) -> CommandResult:
    '''Run ``command`` in the builder runner (stdin closed, stderr merged
    into the output). It may read ``read_only`` and write ``writable``,
    which the runner must serve.
    '''
    builder = self._require(
      self._builder,
      'builder',
      [*read_only, *writable, *([cwd] if cwd is not None else [])]
    )
    try:
      result = await builder.run(
        command,
        cwd = cwd,
        env = env,
        timeout = timeout
      )
    except RunnerError as ex:
      return CommandResult(returncode = -1, output = str(ex),
                           timed_out = False)
    return CommandResult(
      returncode = result.returncode,
      output = result.output.decode(errors = 'replace'),
      timed_out = result.timed_out
    )
