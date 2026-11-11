# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Linux development launcher: credential bytes stay in anonymous memory.

Only symlinks to sealed parent-held memfds are placed in the temporary
credentials directory. The service can reopen them through /proc even
when uv or subprocess closes inherited descriptors.

Note that this does not isolate processes running as the same user or prevent
the kernel swapping memory.
'''

import argparse
import contextlib
import fcntl
import getpass
import json
import os
import signal
import subprocess
import sys
import tempfile
import time

from pathlib import Path
from typing import Iterator, Mapping, Optional

from edgacknowledg.forge.config import (
  ConfigError, SETTINGS_VARIABLE, check_environment,
)


# Linux UAPI (linux/fcntl.h). Some standalone Python builds omit these
# names from fcntl even though memfd_create and sealing are supported.
_F_ADD_SEALS = getattr(fcntl, 'F_ADD_SEALS', 1024 + 9)
_F_SEALS = (getattr(fcntl, 'F_SEAL_SEAL', 0x0001) |
            getattr(fcntl, 'F_SEAL_SHRINK', 0x0002) |
            getattr(fcntl, 'F_SEAL_GROW', 0x0004) |
            getattr(fcntl, 'F_SEAL_WRITE', 0x0008))
_CREDENTIAL_NAMES = (
  'forge-token',
  'webhook-secret',
  'acknowledg-api-key',
  'database-password',
)
_CREDENTIAL_PROBE_TIMEOUT = 5
_SHUTDOWN_GRACE_PERIOD = 10


@contextlib.contextmanager
def memory_credentials(directory: Path,
                       values: Mapping[str, bytes]) -> Iterator[Path]:
  '''Keep credentials readable until the caller's child has stopped.'''
  descriptors = []
  created = False
  try:
    directory.mkdir(mode = 0o700)
    created = True
    for name, value in values.items():
      if name not in _CREDENTIAL_NAMES:
        raise ValueError('unknown credential name')
      fd = os.memfd_create(
        'forge-credential',
        os.MFD_CLOEXEC | os.MFD_ALLOW_SEALING
      )
      descriptors.append(fd)
      os.fchmod(fd, 0o600)
      with os.fdopen(os.dup(fd), 'wb') as stream:
        stream.write(value)
      fcntl.fcntl(fd, _F_ADD_SEALS, _F_SEALS)
      (directory / name).symlink_to(f"/proc/{os.getpid()}/fd/{fd}")
    yield directory
  finally:
    for fd in descriptors:
      os.close(fd)
    # This directory is created by this context, never supplied by a user.
    if created:
      for path in directory.iterdir():
        path.unlink()
      directory.rmdir()


def supervise(command: list[str], environ: Mapping[str, str]) -> int:
  '''Forward shutdown signals and reap the child before closing secrets.'''
  child: Optional[subprocess.Popen[bytes]] = None
  received: Optional[int] = None
  shutdown_deadline: Optional[float] = None

  def forward(signum: int, frame: object) -> None:
    nonlocal received, shutdown_deadline
    received = signum
    if shutdown_deadline is None:
      shutdown_deadline = time.monotonic() + _SHUTDOWN_GRACE_PERIOD
    if child is not None:
      try:
        os.killpg(child.pid, signum)
      except ProcessLookupError:
        pass

  previous = {sig: signal.signal(sig, forward)
              for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}
  try:
    child = subprocess.Popen(
      command, env = dict(environ),
      start_new_session = True, close_fds = True
    )
    if received is not None:
      forward(received, None)
    while True:
      try:
        returncode = child.wait(timeout = 0.1)
        break
      except subprocess.TimeoutExpired:
        if (shutdown_deadline is not None and
            time.monotonic() >= shutdown_deadline):
          try:
            os.killpg(child.pid, signal.SIGKILL)
          except ProcessLookupError:
            pass
          returncode = child.wait()
          break
    return returncode if returncode >= 0 else 128 - returncode
  finally:
    if child is not None and child.poll() is None:
      os.killpg(child.pid, signal.SIGKILL)
      child.wait()
    for sig, handler in previous.items():
      signal.signal(sig, handler)


def development_settings(home: Path, tools_dir: Path) -> str:
  '''A non-secret TOML for the read-only public-repository demo.'''
  return (
    f"home = {json.dumps(str(home))}\n"
    f"tools_dir = {json.dumps(str(tools_dir))}\n"
    'repositories = ["edgcpp/compiler"]\n'
    'ci_workflow = "build-and-test-ci.yml"\n'
    'poll_interval = 60\n'
    'unsandboxed = true\n'
    'comments = false\n'
    'acknowledg_api_host = "localhost"\n'
    'acknowledg_api_port = 2727\n'
    'acknowledg_ui_host = "localhost"\n'
    'acknowledg_ui_port = 5173\n'
  )


def _token(use_gh: bool) -> bytes:
  if use_gh:
    try:
      result = subprocess.run(
        ['gh', 'auth', 'token'], check = True,
        stdout = subprocess.PIPE,
        stderr = subprocess.DEVNULL
      )
    except (OSError, subprocess.CalledProcessError):
      raise ConfigError(
        'cannot read the GitHub CLI token; run gh auth login'
      ) from None
    token = result.stdout.strip()
  else:
    # Refuse getpass's fallback to an echoed stdin read without a terminal.
    try:
      with open('/dev/tty', 'rb'):
        pass
    except OSError:
      raise ConfigError(
        'a terminal is required for the token prompt; use --use-gh-token '
        'or CREDENTIALS_DIRECTORY'
      ) from None
    token = getpass.getpass('Read-only GitHub token: ').encode('utf-8')
  if not token:
    raise ConfigError('the forge-token credential must not be empty')
  return token


class _Termination(Exception):
  def __init__(self, signum: int) -> None:
    self.signum = signum


@contextlib.contextmanager
def _startup_signals() -> Iterator[None]:
  '''Unwind temporary resources if interrupted before child supervision.'''
  def stop(signum: int, frame: object) -> None:
    raise _Termination(signum)

  previous = {sig: signal.signal(sig, stop)
              for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}
  try:
    yield
  finally:
    for sig, handler in previous.items():
      signal.signal(sig, handler)


def _check_credentials(directory: Path) -> None:
  '''Check child access to credentials, including parent-held memfds.'''
  if not directory.is_dir():
    raise ConfigError(f"credentials path is not a directory: {directory}")
  try:
    probe = subprocess.run(
      [
        sys.executable,
        '-c',
        'from pathlib import Path; import sys; '
        'root = Path(sys.argv[1]); '
        'values = {name: p.read_bytes() for name in sys.argv[2:] '
        'if (p := root / name).exists()}; '
        'sys.exit(0 if values.get("forge-token", b"").rstrip(b"\\r\\n") '
        'else 3)',
        str(directory),
        *_CREDENTIAL_NAMES,
      ],
      env = {},
      stdout = subprocess.DEVNULL,
      stderr = subprocess.DEVNULL,
      timeout = _CREDENTIAL_PROBE_TIMEOUT
    )
  except subprocess.TimeoutExpired:
    raise ConfigError('the credential access check timed out') from None
  if probe.returncode == 3:
    raise ConfigError(
      'the forge-token credential is required and must not be empty'
    )
  if probe.returncode != 0:
    raise ConfigError('the child cannot read the credentials directory')


def main() -> int:
  parser = argparse.ArgumentParser(description = __doc__)
  parser.add_argument(
    '--use-gh-token', action = 'store_true',
    help = 'use gh auth token instead of prompting'
  )
  args = parser.parse_args()
  try:
    check_environment(os.environ)
    if not hasattr(os, 'memfd_create') or not Path('/proc/self/fd').is_dir():
      raise ConfigError(
        'development credentials require Linux memfd and /proc'
      )
    if args.use_gh_token and os.environ.get('CREDENTIALS_DIRECTORY'):
      raise ConfigError('choose --use-gh-token or CREDENTIALS_DIRECTORY')
    # No secret bytes are written anywhere under this directory.
    with (_startup_signals(),
          tempfile.TemporaryDirectory(
            prefix = 'acknowledg-forge-dev-'
          ) as tmp):
      root = Path(tmp)
      # gh may use these as its source; Forge must not inherit them.
      environ = {k: v for k, v in os.environ.items() if k not in (
        'GH_TOKEN', 'GITHUB_TOKEN', 'GH_ENTERPRISE_TOKEN',
        'GITHUB_ENTERPRISE_TOKEN',
      )}
      if not environ.get(SETTINGS_VARIABLE):
        settings = root / 'settings.toml'
        tools_dir = Path(__file__).resolve().parents[3]
        settings.write_text(
          development_settings(root / 'state', tools_dir),
          encoding = 'utf-8'
        )
        environ[SETTINGS_VARIABLE] = str(settings)
      with contextlib.ExitStack() as stack:
        if not environ.get('CREDENTIALS_DIRECTORY'):
          credentials = stack.enter_context(
            memory_credentials(
              root / 'credentials', {'forge-token': _token(args.use_gh_token)}
            )
          )
          environ['CREDENTIALS_DIRECTORY'] = str(credentials)
        _check_credentials(Path(environ['CREDENTIALS_DIRECTORY']))
        return supervise(
          [sys.executable, '-m', 'acknowledg_forge_github'],
          environ
        )
  except (ConfigError, OSError) as ex:
    # OS errors here identify paths/operations, never credential contents.
    print(f"development launcher: {ex}", file = sys.stderr)
    return 2
  except _Termination as ex:
    return 128 + ex.signum
  except KeyboardInterrupt:
    return 130


if __name__ == '__main__':
  sys.exit(main())
