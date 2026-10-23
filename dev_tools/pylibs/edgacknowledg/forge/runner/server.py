# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The runner server: runs commands over untrusted pull request content for
the one client its container shares a network with.

The builder and tools containers (``builder``, ``tools``) each run one,
with their role's policy (``roles``): the service's builds
(``edg-rematerialize-test-run``) in the builder, and the publisher's
reviewer commands (``ifc-dump``, ``cg_annotate``) in tools. Those
containers see only the review trees (and, for builds, a staging directory
to write), hold no secret, and have no network but the one they share with
their client (see compose.yaml). ``client`` is the client.

The client keeps one connection open, and sends requests on it (JSON text
messages), each with an ``id`` of its choosing that the answers carry:

- ``{"id", "op": "run", "argv": [...], "cwd", "env": {...}, "timeout"}``
  runs a command. Its output (stdout and stderr together) comes back as
  binary messages: the id (4 bytes, big-endian), then the bytes. Then comes
  ``{"id", "returncode", "timed_out"}``.
- ``{"id", "op": "cancel"}`` kills that command (with no answer).
- ``{"id", "op": "describe"}`` answers ``{"id", "description": {...}}``,
  what the server allows, for start-up checks.

A refused request is answered ``{"id", "error"}``. Commands run
concurrently; when the connection goes, every command it started is
killed.

Only allowed commands (each an argument prefix, e.g. ``python3
/edg/tools/bin/edg-rematerialize-test-run``) run, and only on paths under
the server's roots, with a cleared environment but for allowed variables.
The container is long-lived, so after each command its process group is
killed and, with ``--container``, once no command runs, every other stray
process in the container is killed and ``/tmp`` emptied.
'''

import argparse
import asyncio
import json
import logging
import os
import posixpath
import shlex
import shutil
import signal

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping, Optional, Sequence

from websockets.asyncio.server import Server, ServerConnection, serve
from websockets.exceptions import ConnectionClosed

from edgacknowledg.forge.runner.roles import ROLES, RunnerPolicy

_log = logging.getLogger('edgacknowledg.forge.runner.server')

# Largest request message (an argument vector and a few paths).
MAX_REQUEST_BYTES = 1024 * 1024
# Output is sent in chunks of at most this many bytes.
OUTPUT_CHUNK = 64 * 1024
# Bytes of a request id at the start of an output message.
ID_BYTES = 4


class RequestError(Exception):
  '''A request the server refuses; the message says why.'''


def _under_roots(value: str, roots: Sequence[Path]) -> bool:
  # Lexically: the container's mounts are the actual boundary, this only
  # keeps a client from naming anything else.
  normalized = posixpath.normpath(value)
  if normalized != value.rstrip('/') and value != '/':
    return False
  return any(
    normalized == str(root) or normalized.startswith(str(root) + '/')
    for root in roots
  )


def _names_program(name: str, prefix: str, resolved: str) -> bool:
  '''Whether a request's ``name`` names the program ``prefix`` resolved to:
  by name, or by any absolute path to it (the client may have found it on
  a search path of its own, e.g. through Fedora's /usr/sbin, which links to
  /usr/bin).
  '''
  if name in (prefix, resolved):
    return True
  return (os.path.isabs(name) and
          os.path.realpath(name) == os.path.realpath(resolved))


@dataclass(frozen = True)
class Job:
  argv: tuple[str, ...]
  cwd: Optional[str]
  env: dict[str, str]
  timeout: float


def check_request(request: Mapping[str, Any],
                  policy: RunnerPolicy,
                  commands: Sequence[tuple[tuple[str, ...], str]]) -> Job:
  '''The job ``request`` asks for, if ``policy`` allows it.'''
  argv = request.get('argv')
  if (not isinstance(argv, list) or len(argv) == 0 or
      not all(isinstance(arg, str) and '\0' not in arg for arg in argv)):
    raise RequestError('argv must be a non-empty list of strings')
  program: Optional[str] = None
  prefix_length = 0
  for prefix, resolved in commands:
    if (len(argv) >= len(prefix) and
        _names_program(argv[0], prefix[0], resolved) and
        tuple(argv[1:len(prefix)]) == prefix[1:]):
      program = resolved
      prefix_length = len(prefix)
      break
  if program is None:
    raise RequestError(f"{argv[0]!r} is not allowed here")
  for arg in argv[prefix_length:]:
    if arg.startswith('/') and not _under_roots(arg, policy.roots):
      raise RequestError(f"{arg!r} is outside what this runner serves")

  cwd = request.get('cwd')
  if cwd is not None and (
      not isinstance(cwd, str) or not _under_roots(cwd, policy.roots)):
    raise RequestError(f"working directory {cwd!r} is not allowed")

  env = request.get('env') or {}
  if not isinstance(env, dict):
    raise RequestError('env must be an object')
  for name, value in env.items():
    if name not in policy.env_names or not isinstance(value, str):
      raise RequestError(f"variable {name!r} is not allowed")
    if value.startswith('/') and not _under_roots(value, policy.roots):
      raise RequestError(f"variable {name!r} names a path outside the roots")

  timeout = request.get('timeout', policy.max_timeout)
  if not isinstance(timeout, (int, float)) or timeout <= 0:
    raise RequestError('timeout must be a positive number')
  return Job(
    argv = (program, *argv[1:]),
    cwd = cwd,
    env = {**policy.base_env(), **env},
    timeout = min(float(timeout), policy.max_timeout)
  )


def _kill_group(pid: int) -> None:
  try:
    os.killpg(pid, signal.SIGKILL)
  except (ProcessLookupError, PermissionError):
    pass


def stray_processes(proc_dir: Path = Path('/proc')) -> list[int]:
  '''Every process but this one and its ancestors (e.g. the container's
  init).
  '''
  keep = set()
  pid = os.getpid()
  while pid > 0 and pid not in keep:
    keep.add(pid)
    try:
      stat = (proc_dir / str(pid) / 'stat').read_text()
    except OSError:
      break
    # The parent follows the command name, which may hold anything.
    pid = int(stat.rsplit(')', 1)[1].split()[1])
  strays = []
  for entry in proc_dir.iterdir():
    if entry.name.isdigit() and int(entry.name) not in keep:
      strays.append(int(entry.name))
  return strays


def _empty_directory(path: Path) -> None:
  for entry in path.iterdir():
    try:
      if entry.is_dir() and not entry.is_symlink():
        shutil.rmtree(entry, ignore_errors = True)
      else:
        entry.unlink()
    except OSError:
      pass


class RunnerServer:
  '''Serves ``policy`` to its client.'''

  def __init__(self,
               policy: RunnerPolicy,
               *,
               container: bool = False) -> None:
    self._policy = policy
    self._container = container
    self._commands = policy.resolved_commands()
    self._active = 0

  async def start(self, host: str, port: int) -> Server:
    '''Start serving (port 0: any free one); close the server to stop.'''
    return await serve(
      self._handle,
      host,
      port,
      max_size = MAX_REQUEST_BYTES
    )

  async def serve(self, host: str, port: int) -> None:
    async with await self.start(host, port) as server:
      await server.serve_forever()

  async def _handle(self, connection: ServerConnection) -> None:
    '''Serve one connection's requests until it goes, then kill whatever
    it still runs.
    '''
    commands: dict[int, asyncio.Task[None]] = {}
    try:
      async for raw in connection:
        try:
          request = json.loads(raw)
        except ValueError:
          _log.warning('ignored a malformed request')
          continue
        request_id = request.get('id') if isinstance(request, dict) else None
        if (not isinstance(request_id, int) or
            not 0 <= request_id < 2 ** (8 * ID_BYTES)):
          _log.warning('ignored a request without a valid id')
          continue
        op = request.get('op')
        if op == 'cancel':
          command = commands.get(request_id)
          if command is not None:
            command.cancel()
          continue
        try:
          if op == 'describe':
            await connection.send(json.dumps({
              'id': request_id,
              'description': self._policy.describe(),
            }))
          elif op == 'run':
            job = check_request(request, self._policy, self._commands)
            command = asyncio.create_task(
              self._run(connection, request_id, job)
            )
            commands[request_id] = command

            def forget(_: 'asyncio.Task[None]',
                       request_id: int = request_id) -> None:
              commands.pop(request_id, None)

            command.add_done_callback(forget)
          else:
            raise RequestError(f"unknown operation: {op!r}")
        except RequestError as ex:
          _log.warning('refused a request: %s', ex)
          await connection.send(json.dumps({
            'id': request_id,
            'error': str(ex),
          }))
    except ConnectionClosed:
      pass
    finally:
      for command in list(commands.values()):
        command.cancel()
      await asyncio.gather(*commands.values(), return_exceptions = True)

  async def _run(self,
                 connection: ServerConnection,
                 request_id: int,
                 job: Job) -> None:
    '''Run ``job``, relaying its output, until it ends, times out, or is
    cancelled (by the client, or with its connection).
    '''
    _log.info('running %s', shlex.join(job.argv))
    self._active += 1
    try:
      try:
        process = await asyncio.create_subprocess_exec(
          *job.argv,
          stdin = asyncio.subprocess.DEVNULL,
          stdout = asyncio.subprocess.PIPE,
          stderr = asyncio.subprocess.STDOUT,
          cwd = job.cwd,
          env = job.env,
          start_new_session = True
        )
      except OSError as ex:
        await connection.send(json.dumps({
          'id': request_id,
          'error': f"can't run: {ex}",
        }))
        return
      try:
        timed_out = False
        try:
          await asyncio.wait_for(
            self._relay(connection, request_id, process),
            job.timeout
          )
        except asyncio.TimeoutError:
          timed_out = True
        # The command leads its own session; take its children down with
        # it.
        _kill_group(process.pid)
        returncode = await process.wait()
        await connection.send(json.dumps({
          'id': request_id,
          'returncode': -1 if timed_out else returncode,
          'timed_out': timed_out,
        }))
      finally:
        _kill_group(process.pid)
        if process.returncode is None:
          process.kill()
          await process.wait()
    except ConnectionClosed:
      pass
    finally:
      self._active -= 1
      if self._active == 0:
        self._clean_up()

  @staticmethod
  async def _relay(connection: ServerConnection,
                   request_id: int,
                   process: asyncio.subprocess.Process) -> None:
    assert process.stdout is not None
    prefix = request_id.to_bytes(ID_BYTES, 'big')
    while True:
      chunk = await process.stdout.read(OUTPUT_CHUNK)
      if chunk == b'':
        return
      await connection.send(prefix + chunk)

  def _clean_up(self) -> None:
    '''With no command running, leave nothing of one behind.'''
    if not self._container:
      return
    for pid in stray_processes():
      try:
        os.kill(pid, signal.SIGKILL)
      except (ProcessLookupError, PermissionError):
        pass
    _empty_directory(Path('/tmp'))


def main(role: str) -> None:
  '''Entry point of a runner container: serve ``role``'s runner.'''
  parser = argparse.ArgumentParser(
    description = f"Serve the {role} runner: run its allowed commands for "
                  f"its client."
  )
  parser.add_argument(
    '--home',
    type = Path,
    required = True,
    help = "the deployment's state directory, as mounted (only the "
           "role's part of it need be)"
  )
  parser.add_argument('--tools-dir', type = Path, required = True)
  parser.add_argument(
    '--host',
    default = '127.0.0.1',
    help = 'the address to listen on (default: loopback only; in a '
           'container, its network\'s)'
  )
  parser.add_argument('--port', type = int, required = True)
  parser.add_argument(
    '--container',
    action = 'store_true',
    help = 'between commands, kill every other process and empty /tmp '
           '(only inside a container of its own)'
  )
  args = parser.parse_args()
  logging.basicConfig(
    level = logging.INFO,
    format = '%(levelname)s %(name)s: %(message)s'
  )
  policy = ROLES[role](
    home = args.home.absolute(),
    tools_dir = args.tools_dir.absolute()
  )
  server = RunnerServer(policy, container = args.container)
  try:
    asyncio.run(server.serve(args.host, args.port))
  except KeyboardInterrupt:
    pass
