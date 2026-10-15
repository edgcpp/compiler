# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The runner server (and its client): what it allows, and how it runs
commands.

These run it as a local process; test_containers.py checks what its
containers add (mounts, no network, no capabilities).
'''

import asyncio
import os
import shutil
import socket
import subprocess
import time

from pathlib import Path
from typing import AsyncIterator

import pytest
import pytest_asyncio

from edgacknowledg.forge.runner.client import RunnerClient, RunnerError
from edgacknowledg.forge.runner.roles import (
  RunnerPolicy,
  builder_policy,
  tools_policy,
)
from edgacknowledg.forge.runner.server import (
  RequestError,
  RunnerServer,
  check_request,
  stray_processes,
)
from edgacknowledg.forge.sandbox import Sandbox, SandboxUnavailableError
from edgacknowledg.forge.workspace import Workspaces

from fakes import FakeForge


def _free_port() -> int:
  with socket.socket() as sock:
    sock.bind(('127.0.0.1', 0))
    return int(sock.getsockname()[1])


def _alive(pid: int) -> bool:
  try:
    state = Path(f"/proc/{pid}/stat").read_text().rsplit(')', 1)[1].split()[0]
  except (OSError, IndexError):
    return False
  return state not in ('Z', 'X')


def _policy(root: Path, **overrides: object) -> RunnerPolicy:
  values: dict[str, object] = {
    'commands': (('sh', '-c'), ('cat',), ('true',)),
    'roots': (root,),
    'env_names': frozenset({'EDG_TEST_HOME'}),
    'max_timeout': 30.0,
  }
  values.update(overrides)
  return RunnerPolicy(**values)  # type: ignore[arg-type]


class _Running:
  def __init__(self, root: Path, client: RunnerClient) -> None:
    self.root = root
    self.client = client


@pytest_asyncio.fixture
async def runner(tmp_path: Path) -> AsyncIterator[_Running]:
  root = tmp_path / 'reviews'
  root.mkdir()
  port = _free_port()
  server = RunnerServer(_policy(root))
  task = asyncio.create_task(server.serve('127.0.0.1', port))
  for _ in range(100):
    try:
      _, writer = await asyncio.open_connection('127.0.0.1', port)
      writer.close()
      break
    except OSError:
      await asyncio.sleep(0.05)
  yield _Running(root, RunnerClient(f"ws://127.0.0.1:{port}"))
  task.cancel()
  await asyncio.gather(task, return_exceptions = True)


# What's allowed


def _check(root: Path, **request: object) -> object:
  policy = _policy(root)
  return check_request(
    {'op': 'run', **request},
    policy,
    policy.resolved_commands()
  )


def test_only_allowed_commands_run(tmp_path: Path) -> None:
  sh = shutil.which('sh')
  assert sh is not None
  # By name or by the path it resolves to; the resolved one runs.
  for name in ('sh', os.path.abspath(sh)):
    job = _check(tmp_path, argv = [name, '-c', 'true'])
    assert job.argv[0] == os.path.abspath(sh)  # type: ignore[attr-defined]
  refused = [
    ['python3', '-c', 'pass'],
    # The whole prefix must match.
    ['sh', 'script.sh'],
    # A program that merely shares the name.
    [str(tmp_path / 'sh'), '-c', 'true'],
    [],
  ]
  for argv in refused:
    with pytest.raises(RequestError):
      _check(tmp_path, argv = argv)


def test_paths_must_be_under_the_roots(tmp_path: Path) -> None:
  inside = tmp_path / 'file'
  _check(tmp_path, argv = ['cat', str(inside)], cwd = str(tmp_path))
  refused = [
    {'argv': ['cat', '/etc/passwd']},
    {'argv': ['cat', f"{tmp_path}/../escape"]},
    {'argv': ['cat', f"{tmp_path}x/file"]},
    {'argv': ['true'], 'cwd': '/'},
    {'argv': ['true'], 'env': {'EDG_TEST_HOME': '/etc'}},
    {'argv': ['true'], 'env': {'LD_PRELOAD': str(inside)}},
  ]
  for request in refused:
    with pytest.raises(RequestError):
      _check(tmp_path, **request)


def test_timeouts_are_capped(tmp_path: Path) -> None:
  job = _check(tmp_path, argv = ['true'], timeout = 10 ** 6)
  assert job.timeout == 30.0  # type: ignore[attr-defined]
  with pytest.raises(RequestError):
    _check(tmp_path, argv = ['true'], timeout = -1)


# Running


async def test_relays_output_and_status(runner: _Running) -> None:
  (runner.root / 'input').write_text('data\n')
  result = await runner.client.run(
    ['sh', '-c', f"cat {runner.root}/input; echo err >&2; exit 3"],
    timeout = 10
  )
  assert result.output == b'data\nerr\n'
  assert result.returncode == 3 and not result.timed_out

  result = await runner.client.run(['cat', str(runner.root)],
                                   cwd = runner.root, timeout = 10)
  assert result.returncode != 0


async def test_environment_is_cleared(runner: _Running,
                                      monkeypatch: pytest.MonkeyPatch) -> None:
  monkeypatch.setenv('EDG_SECRET', 'leaked')
  result = await runner.client.run(
    ['sh', '-c', 'echo "secret=$EDG_SECRET home=$HOME test=$EDG_TEST_HOME"'],
    env = {'EDG_TEST_HOME': str(runner.root)},
    timeout = 10
  )
  assert result.output == f"secret= home=/tmp test={runner.root}\n".encode()


async def test_refusals_reach_the_client(runner: _Running) -> None:
  with pytest.raises(RunnerError, match = 'not allowed'):
    await runner.client.run(['python3', '-c', 'pass'], timeout = 5)
  description = await runner.client.describe()
  assert description['roots'] == [str(runner.root)]


async def test_timeout_kills_the_whole_group(runner: _Running) -> None:
  child_file = runner.root / 'child'
  result = await runner.client.run(
    ['sh', '-c', f"sleep 30 & echo $! > {child_file}; wait"],
    timeout = 1
  )
  assert result.timed_out
  child = int(child_file.read_text())
  for _ in range(50):
    if not _alive(child):
      break
    await asyncio.sleep(0.1)
  assert not _alive(child)


async def _started(pid_file: Path) -> int:
  for _ in range(100):
    if pid_file.is_file() and pid_file.read_text().strip() != '':
      return int(pid_file.read_text())
    await asyncio.sleep(0.05)
  raise AssertionError(f"{pid_file} was never written")


async def _gone(pid: int) -> bool:
  for _ in range(50):
    if not _alive(pid):
      return True
    await asyncio.sleep(0.1)
  return False


async def test_cancelling_a_run_kills_its_command(runner: _Running) -> None:
  pid_file = runner.root / 'pid'
  run = asyncio.create_task(runner.client.run(
    ['sh', '-c', f"echo $$ > {pid_file}; sleep 30"],
    timeout = 60
  ))
  command = await _started(pid_file)
  run.cancel()
  with pytest.raises(asyncio.CancelledError):
    await run
  assert await _gone(command)
  # The connection carries on.
  result = await runner.client.run(['true'], timeout = 10)
  assert result.returncode == 0


async def test_losing_the_connection_kills_its_commands(
    runner: _Running) -> None:
  pid_file = runner.root / 'pid'
  run = asyncio.create_task(runner.client.run(
    ['sh', '-c', f"echo $$ > {pid_file}; sleep 30"],
    timeout = 60
  ))
  command = await _started(pid_file)
  await runner.client.close()
  with pytest.raises(RunnerError, match = 'lost'):
    await run
  assert await _gone(command)
  # The next request connects again.
  result = await runner.client.run(['true'], timeout = 10)
  assert result.returncode == 0


async def test_one_connection_carries_every_command(runner: _Running) -> None:
  await runner.client.run(['true'], timeout = 10)
  connection = runner.client._connection
  assert connection is not None
  # Commands run concurrently over it...
  started = time.monotonic()
  results = await asyncio.gather(*(
    runner.client.run(['sh', '-c', f"sleep 1; echo {n}"], timeout = 10)
    for n in range(4)
  ))
  assert time.monotonic() - started < 3
  # ...each answer reaching its own request.
  assert [r.output for r in results] == [f"{n}\n".encode() for n in range(4)]
  assert runner.client._connection is connection


def test_strays_are_everything_but_the_runner_and_its_ancestors() -> None:
  child = subprocess.Popen(['sleep', '30'])
  try:
    strays = stray_processes()
    assert child.pid in strays
    assert os.getpid() not in strays
    assert os.getppid() not in strays
  finally:
    child.kill()
    child.wait()


# The sandbox, over runners


async def test_sandbox_builds_through_its_builder(runner: _Running) -> None:
  staging = runner.root / 'staging'
  staging.mkdir()
  sandbox = Sandbox(builder = runner.client)
  await sandbox.check()
  result = await sandbox.run(
    ['sh', '-c', f"echo built > {staging}/out"],
    read_only = [runner.root],
    writable = [staging],
    timeout = 10
  )
  assert result.succeeded, result.output
  assert (staging / 'out').read_text() == 'built\n'
  # Paths the builder doesn't serve are refused before anything runs.
  with pytest.raises(SandboxUnavailableError, match = "doesn't serve"):
    await sandbox.run(['true'], writable = [Path('/etc')], timeout = 10)


async def test_sandbox_commands_go_to_the_tools_runner(
    runner: _Running) -> None:
  (runner.root / 'file').write_text('contents\n')
  sandbox = Sandbox(tools = runner.client)
  await sandbox.check()
  context = sandbox.context_for(runner.root)
  result = await context.run_command('cat', str(runner.root / 'file'))
  assert result == (0, b'contents\n', False)
  with pytest.raises(SandboxUnavailableError, match = "doesn't serve"):
    sandbox.context_for(Path('/etc'))


# The roles


def test_roles_serve_the_workspace_layout(tmp_path: Path) -> None:
  home = tmp_path / 'home'
  tools_dir = tmp_path / 'tools'
  workspaces = Workspaces(home, FakeForge(tmp_path / 'origin'))
  builder = builder_policy(home = home, tools_dir = tools_dir)
  assert set(builder.roots) == {
    workspaces.reviews_dir,
    workspaces.build_staging_dir,
  }
  assert builder.commands == (
    ('python3', str(tools_dir / 'bin' / 'edg-rematerialize-test-run')),
  )
  tools = tools_policy(home = home, tools_dir = tools_dir)
  assert tools.roots == (workspaces.reviews_dir,)
  assert {prefix[0] for prefix in tools.commands} == {'ifc-dump', 'cg_annotate'}


def test_missing_programs_are_refused_not_fatal(tmp_path: Path) -> None:
  policy = _policy(tmp_path, commands = (('true',), ('no-such-program',)))
  resolved = policy.resolved_commands()
  assert [prefix for prefix, _ in resolved] == [('true',)]
  with pytest.raises(RequestError, match = 'not allowed'):
    check_request({'argv': ['no-such-program']}, policy, resolved)
