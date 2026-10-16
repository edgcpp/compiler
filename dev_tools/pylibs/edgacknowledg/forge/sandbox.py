# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Run commands over untrusted pull request content under bubblewrap.

Sandboxed commands see a read-only /usr (plus the host's /bin, /lib*
layout), only the paths they are given, no network, no capabilities, and a
cleared environment, so neither the service's secrets nor the rest of the
host are reachable from them; nor can they create user namespaces of their
own (hence bubblewrap 0.8 or later, for --disable-userns).

The process serving reviewers (see ``publisher_worker``) runs sandboxed
too, through ``service_prefix``: like a command, but with the network (to
reach AcknowlEDG) and able to start sandboxed commands itself.

bubblewrap follows symlinks in the paths it binds, so every path given to
it must be one the service created or checked (``require_real_directory``).
'''

import asyncio
import os
import re
import shutil
import signal

from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Optional, Sequence

from edgacknowledg.client.shared import SandboxContext

# Top-level directories programs need, mirrored as the host has them
# (directories or, on merged-/usr systems, symlinks into /usr).
_SYSTEM_DIRS = ('/bin', '/sbin', '/lib', '/lib32', '/lib64', '/libx32')

_DEFAULT_PATH = '/usr/local/bin:/usr/bin:/bin'

# The oldest bubblewrap with --disable-userns (Ubuntu 24.04 and later, and
# current Fedora, ship newer ones).
MIN_BWRAP_VERSION = (0, 8)

_BWRAP_VERSION = re.compile(r'^bubblewrap (\d+)\.(\d+)', re.MULTILINE)

# What a networked process needs from /etc: name resolution and the
# certificates (and crypto policy) for TLS.
_NETWORK_ETC = (
  '/etc/resolv.conf',
  '/etc/hosts',
  '/etc/nsswitch.conf',
  '/etc/host.conf',
  '/etc/gai.conf',
  '/etc/ssl',
  '/etc/pki',
  '/etc/crypto-policies',
)


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
  def tools_dir(self) -> Optional[Path]:
    return self._tools_dir

  @property
  def bwrap(self) -> str:
    return self._bwrap

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
    result = await self._exec(
      [self._bwrap, '--version'],
      process_env = None,
      cwd = None,
      timeout = 30
    )
    match = _BWRAP_VERSION.search(result.output)
    version = (
      (int(match.group(1)), int(match.group(2))) if match is not None
      else None
    )
    if version is None or version < MIN_BWRAP_VERSION:
      found = (
        f"{version[0]}.{version[1]}" if version is not None
        else f"an unknown version ({result.output.strip()!r})"
      )
      raise SandboxUnavailableError(
        f"bubblewrap {MIN_BWRAP_VERSION[0]}.{MIN_BWRAP_VERSION[1]} or later "
        f"is required (for --disable-userns), but '{self._bwrap}' is {found}"
      )

    result = await self.run(['true'], timeout = 30)
    if not result.succeeded:
      raise SandboxUnavailableError(
        f"bubblewrap failed (are unprivileged user namespaces enabled?): "
        f"{result.output.strip()}"
      )

    # Commands for reviewers run sandboxed inside the sandboxed process
    # serving them.
    result = await self._exec(
      [*self.service_prefix(), *self.prefix(), 'true'],
      process_env = None,
      cwd = None,
      timeout = 30
    )
    if not result.succeeded:
      raise SandboxUnavailableError(
        f"bubblewrap can't run nested in itself (are nested user "
        f"namespaces allowed?): {result.output.strip()}"
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

    # (--disable-userns needs the user namespace unshared explicitly.)
    args = [self._bwrap, '--unshare-all', '--unshare-user', '--disable-userns']
    args += self._common_args(read_only = read_only)
    for path in writable:
      args += ['--bind', str(path), str(path)]
    args += self._env_args({**self._base_env(), **env})
    if cwd is not None:
      args += ['--chdir', str(cwd)]
    args.append('--')
    return args

  def context_for(self, *roots: Path) -> SandboxContext:
    '''The context for serving the pull request content under ``roots``:
    no symlink in it is followed, and commands run on it see only it,
    read-only.
    '''
    # Bind the roots as the context reduces them.
    context = SandboxContext(*roots, allow_symlinks = False)
    return SandboxContext(
      *context.roots,
      allow_symlinks = False,
      command_prefix = self.prefix(read_only = context.roots)
    )

  def service_prefix(self,
                     *,
                     read_only: Sequence[Path] = (),
                     env: Mapping[str, str] = {}) -> list[str]:
    '''Return the command prefix for a long-lived process that handles
    untrusted content on the network's behalf: it may use the network and
    start sandboxed commands (``prefix``) of its own, but otherwise sees only
    ``read_only``, read-only.

    Unlike ``prefix``, nothing is added to the environment but ``PATH``.
    '''
    if not self._enabled:
      return []

    args = [self._bwrap, '--unshare-all', '--share-net']
    args += self._common_args(read_only = read_only)
    for path in _NETWORK_ETC:
      args += ['--ro-bind-try', path, path]
    args += self._env_args(env)
    args.append('--')
    return args

  def _common_args(self, *, read_only: Sequence[Path]) -> list[str]:
    args = [
      '--die-with-parent',
      '--new-session',
      '--cap-drop', 'ALL',
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
    return args

  def _env_args(self, env: Mapping[str, str]) -> list[str]:
    args = ['--clearenv', '--setenv', 'PATH', self.search_path]
    for name, value in env.items():
      args += ['--setenv', name, value]
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
    return await self._exec(
      args,
      process_env = process_env,
      cwd = None if self._enabled else cwd,
      timeout = timeout
    )

  async def _exec(self,
                  args: Sequence[str],
                  *,
                  process_env: Optional[dict[str, str]],
                  cwd: Optional[Path],
                  timeout: float) -> CommandResult:
    process = await asyncio.create_subprocess_exec(
      *args,
      stdin = asyncio.subprocess.DEVNULL,
      stdout = asyncio.subprocess.PIPE,
      stderr = asyncio.subprocess.STDOUT,
      cwd = cwd,
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
