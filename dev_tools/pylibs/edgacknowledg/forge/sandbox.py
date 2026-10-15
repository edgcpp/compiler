# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Run commands over untrusted pull request content under bubblewrap.

Sandboxed commands see a read-only /usr (plus the host's /bin, /lib*
layout), only the paths they are given, no network, and a cleared
environment, so neither the service's secrets nor the rest of the host are
reachable from them.
'''

import asyncio
import os
import shutil
import signal

from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Optional, Sequence

# Top-level directories programs need, mirrored as the host has them
# (directories or, on merged-/usr systems, symlinks into /usr).
_SYSTEM_DIRS = ('/bin', '/sbin', '/lib', '/lib32', '/lib64', '/libx32')

_DEFAULT_PATH = '/usr/local/bin:/usr/bin:/bin'


class SandboxUnavailableError(Exception):
  '''Raised when sandboxing is required but bubblewrap can't be used.'''


@dataclass(frozen = True)
class CommandResult:
  returncode: int
  output: str
  timed_out: bool

  @property
  def succeeded(self) -> bool:
    return self.returncode == 0 and not self.timed_out


class Sandbox:
  '''Builds and runs (optionally) sandboxed commands.

  ``tools_dir`` is a dev_tools-style directory (bin/, pylibs/) whose tools
  sandboxed commands may run: it is bound read-only, and its bin/ and
  pylibs/ are put on PATH and PYTHONPATH. With ``enabled = False`` commands
  run directly; that is for local development only.
  '''

  def __init__(self,
               *,
               enabled: bool,
               tools_dir: Optional[Path] = None,
               bwrap: str = 'bwrap') -> None:
    self._enabled = enabled
    self._tools_dir = tools_dir
    self._bwrap = bwrap

  @property
  def enabled(self) -> bool:
    return self._enabled

  @property
  def search_path(self) -> str:
    if self._tools_dir is None:
      return _DEFAULT_PATH
    return f"{self._tools_dir / 'bin'}:{_DEFAULT_PATH}"

  def _base_env(self) -> dict[str, str]:
    env = {'HOME': '/tmp', 'LC_ALL': 'C'}
    if self._tools_dir is not None:
      env['PYTHONPATH'] = str(self._tools_dir / 'pylibs')
    return env

  async def check(self) -> None:
    '''Raise ``SandboxUnavailableError`` unless sandboxed commands work.'''
    if not self._enabled:
      return
    if shutil.which(self._bwrap) is None:
      raise SandboxUnavailableError(
        f"'{self._bwrap}' was not found; install bubblewrap (or, for local "
        f"development only, disable sandboxing)"
      )
    result = await self.run(['true'], timeout = 30)
    if not result.succeeded:
      raise SandboxUnavailableError(
        f"bubblewrap failed (are unprivileged user namespaces enabled?): "
        f"{result.output.strip()}"
      )

  def prefix(self,
             *,
             read_only: Sequence[Path] = (),
             writable: Sequence[Path] = (),
             env: Mapping[str, str] = {},
             cwd: Optional[Path] = None) -> list[str]:
    '''Return the command prefix that runs a command in the sandbox.

    Later binds take precedence, so a writable directory may be nested in a
    read-only one.
    '''
    if not self._enabled:
      return []

    args = [
      self._bwrap,
      '--unshare-all',
      '--die-with-parent',
      '--new-session',
      '--ro-bind', '/usr', '/usr',
      '--ro-bind-try', '/etc/alternatives', '/etc/alternatives',
      '--ro-bind-try', '/etc/ld.so.cache', '/etc/ld.so.cache',
      '--proc', '/proc',
      '--dev', '/dev',
      '--tmpfs', '/tmp',
    ]
    for system_dir in _SYSTEM_DIRS:
      if os.path.islink(system_dir):
        args += ['--symlink', os.readlink(system_dir), system_dir]
      elif os.path.isdir(system_dir):
        args += ['--ro-bind', system_dir, system_dir]
    tools = [self._tools_dir] if self._tools_dir is not None else []
    for path in [*tools, *read_only]:
      args += ['--ro-bind', str(path), str(path)]
    for path in writable:
      args += ['--bind', str(path), str(path)]

    args += ['--clearenv', '--setenv', 'PATH', self.search_path]
    for name, value in {**self._base_env(), **env}.items():
      args += ['--setenv', name, value]
    if cwd is not None:
      args += ['--chdir', str(cwd)]
    args.append('--')
    return args

  async def run(self,
                command: Sequence[str],
                *,
                read_only: Sequence[Path] = (),
                writable: Sequence[Path] = (),
                env: Mapping[str, str] = {},
                cwd: Optional[Path] = None,
                timeout: float) -> CommandResult:
    '''Run ``command`` (stdin closed, stderr merged into the output).'''
    if self._enabled:
      args = [
        *self.prefix(
          read_only = read_only,
          writable = writable,
          env = env,
          cwd = cwd
        ),
        *command
      ]
      process_env: Optional[dict[str, str]] = None
    else:
      args = list(command)
      process_env = {
        **self._base_env(),
        'PATH': f"{self.search_path}:{os.environ.get('PATH', '')}",
        'HOME': os.environ.get('HOME', '/tmp'),
        **env,
      }

    process = await asyncio.create_subprocess_exec(
      *args,
      stdin = asyncio.subprocess.DEVNULL,
      stdout = asyncio.subprocess.PIPE,
      stderr = asyncio.subprocess.STDOUT,
      cwd = None if self._enabled else cwd,
      env = process_env,
      start_new_session = True
    )
    try:
      stdout, _ = await asyncio.wait_for(process.communicate(), timeout)
    except asyncio.TimeoutError:
      # The command leads its own session; take its children down with it.
      try:
        os.killpg(process.pid, signal.SIGKILL)
      except ProcessLookupError:
        pass
      stdout, _ = await process.communicate()
      return CommandResult(
        returncode = -1,
        output = stdout.decode(errors = 'replace'),
        timed_out = True
      )
    assert process.returncode is not None
    return CommandResult(
      returncode = process.returncode,
      output = stdout.decode(errors = 'replace'),
      timed_out = False
    )
