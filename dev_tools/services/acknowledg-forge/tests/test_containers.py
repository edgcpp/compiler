# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The deployment's containers (compose.yaml): what each one sees and can
reach, and that each client reaches its server: the service the builder's
runner server and the publisher server, the publisher the tools runner
server.

Needs Docker with Compose, and the image to test in
ACKNOWLEDG_FORGE_TEST_IMAGE (e.g. ghcr.io/edgcpp/acknowledg-forge:latest
after build-edg-env acknowledg-forge); skipped otherwise. The service
container runs a placeholder instead of the service, so probes can run in
it.
'''

import json
import os
import shutil
import socket
import subprocess
import time
import uuid

from pathlib import Path
from typing import Iterator

import pytest

from edgacknowledg.forge.sandbox import TMPFS_SIZE

IMAGE = os.environ.get('ACKNOWLEDG_FORGE_TEST_IMAGE')
COMPOSE_FILE = Path(__file__).resolve().parent.parent / 'compose.yaml'
PYTHON = '/edg/acknowledg-forge/github/bin/python'
STATE = '/var/lib/acknowledg-forge'
CONTAINERS = ('service', 'builder', 'publisher', 'tools')

pytestmark = pytest.mark.skipif(
  IMAGE is None or shutil.which('docker') is None,
  reason = 'needs Docker and ACKNOWLEDG_FORGE_TEST_IMAGE'
)

# The service container's command, so probes can run in it.
_OVERRIDE = '''services:
  service:
    command: ["sleep", "infinity"]
'''


class Deployment:
  def __init__(self, root: Path) -> None:
    self.root = root
    self.project = f"acknowledg-forge-test-{uuid.uuid4().hex[:8]}"
    self.state = root / 'state'
    self.env_file = root / 'compose.env'
    self.override = root / 'override.yaml'

  def compose(self, *args: str, check: bool = True,
              timeout: float = 300) -> 'subprocess.CompletedProcess[str]':
    return subprocess.run(
      [
        'docker', 'compose',
        '--project-name', self.project,
        '--file', str(COMPOSE_FILE),
        '--file', str(self.override),
        '--env-file', str(self.env_file),
        *args,
      ],
      capture_output = True,
      text = True,
      check = check,
      timeout = timeout
    )

  def run(self, container: str, script: str,
          timeout: float = 60) -> 'subprocess.CompletedProcess[str]':
    '''Run ``script`` (sh) in ``container``.'''
    return self.compose('exec', '-T', container, 'sh', '-c', script,
                        check = False, timeout = timeout)

  def python(self, container: str, code: str,
             timeout: float = 120) -> 'subprocess.CompletedProcess[str]':
    return self.compose('exec', '-T', container, PYTHON, '-c', code,
                        check = False, timeout = timeout)

  def processes(self, container: str) -> str:
    return self.compose('top', container).stdout


@pytest.fixture(scope = 'module')
def deployment(tmp_path_factory: pytest.TempPathFactory
               ) -> Iterator[Deployment]:
  root = tmp_path_factory.mktemp('deployment')
  deployment = Deployment(root)
  state = deployment.state
  for directory in ('reviews', 'build-staging', 'repos'):
    (state / directory).mkdir(parents = True)
  (state / 'state.db').write_text('database\n')
  (state / 'repos' / 'clone-file').write_text('clone\n')
  review = state / 'reviews' / 'r1' / 'base' / 'tests'
  review.mkdir(parents = True)
  (review / 'a.sft.cpp').write_text('int a;\n')
  credentials = root / 'credentials'
  credentials.mkdir()
  (credentials / 'forge-token').write_text('forge-token-value\n')
  settings = root / 'settings.env'
  settings.write_text('ACKNOWLEDG_FORGE_REPOSITORIES=edg/compiler\n')
  deployment.env_file.write_text(
    f"IMAGE={IMAGE}\n"
    f"FORGE=github\n"
    f"FORGE_UID={os.getuid()}\n"
    f"FORGE_GID={os.getgid()}\n"
    f"STATE_DIR={state}\n"
    f"SETTINGS_FILE={settings}\n"
    f"CREDENTIALS_DIR={credentials}\n"
    # Any free port: tests may run beside a deployment.
    f"WEBHOOK_PORT=0\n"
  )
  deployment.override.write_text(_OVERRIDE)
  deployment.compose('up', '--detach', '--wait', '--wait-timeout', '120')
  try:
    yield deployment
  finally:
    logs = deployment.compose('logs', check = False).stdout
    deployment.compose('down', '--volumes', '--timeout', '5', check = False)
    print(logs[-20000:])


# What each container is


@pytest.mark.parametrize('container', CONTAINERS)
def test_containers_are_unprivileged(deployment: Deployment,
                                     container: str) -> None:
  result = deployment.run(container, (
    'id -u; '
    'grep CapEff /proc/self/status; '
    'unshare -U true 2>/dev/null && echo UNSHARED; '
    'touch /root-fs 2>/dev/null && echo WROTE-ROOT-FS; '
    'df --block-size=1 --output=size /tmp | tail -n 1'
  ))
  lines = result.stdout.split()
  assert lines[0] == str(os.getuid()), result
  assert lines[1:3] == ['CapEff:', '0000000000000000'], result
  assert 'UNSHARED' not in result.stdout
  assert 'WROTE-ROOT-FS' not in result.stdout
  assert int(lines[-1]) == TMPFS_SIZE, result


@pytest.mark.parametrize('container', ('builder', 'publisher', 'tools'))
def test_only_the_service_sees_its_state(deployment: Deployment,
                                         container: str) -> None:
  result = deployment.run(container, (
    f"cat {STATE}/state.db {STATE}/repos/clone-file 2>/dev/null; "
    'cat /run/credentials/forge-token 2>/dev/null; '
    f"cat {STATE}/reviews/r1/base/tests/a.sft.cpp; "
    f"touch {STATE}/reviews/r1/tampered 2>/dev/null && echo WROTE-REVIEWS; "
    f"ls {STATE}"
  ))
  assert 'int a;' in result.stdout, result
  assert 'database' not in result.stdout
  assert 'clone' not in result.stdout
  assert 'forge-token-value' not in result.stdout
  assert 'WROTE-REVIEWS' not in result.stdout
  expected = {'reviews', 'build-staging'} if container == 'builder' else {
    'reviews'
  }
  assert set(result.stdout.split('\n', 1)[1].split()) == expected, result


def test_the_builder_writes_only_staging(deployment: Deployment) -> None:
  result = deployment.run('builder', (
    f"echo built > {STATE}/build-staging/probe && echo WROTE-STAGING"
  ))
  assert 'WROTE-STAGING' in result.stdout, result
  assert (deployment.state / 'build-staging' / 'probe').read_text() == (
    'built\n'
  )


def _reach(deployment: Deployment, container: str, host: str,
           port: int) -> bool:
  result = deployment.python(container, (
    'import socket\n'
    'try:\n'
    f"  socket.create_connection(({host!r}, {port}), 3).close()\n"
    '  print("REACHED")\n'
    'except OSError as ex:\n'
    '  print(ex)\n'
  ))
  return 'REACHED' in result.stdout


def test_networks_pair_containers(deployment: Deployment) -> None:
  # Each pair, and nothing else.
  assert _reach(deployment, 'service', 'builder', 2730)
  assert _reach(deployment, 'service', 'publisher', 2731)
  assert _reach(deployment, 'publisher', 'tools', 2732)
  assert not _reach(deployment, 'service', 'tools', 2732)
  assert not _reach(deployment, 'builder', 'publisher', 2731)
  assert not _reach(deployment, 'builder', 'tools', 2732)
  assert not _reach(deployment, 'tools', 'builder', 2730)
  assert not _reach(deployment, 'tools', 'publisher', 2731)
  # The runners have no way out.
  for container in ('builder', 'tools'):
    assert not _reach(deployment, container, '1.1.1.1', 53)


def test_webhooks_bound_to_the_frontend_reach_only_the_host(
    deployment: Deployment) -> None:
  # As the service does with ACKNOWLEDG_FORGE_WEBHOOK_BIND=
  # service-frontend:2728 (a stand-in server; the service isn't running).
  deployment.compose(
    'exec', '--detach', 'service', PYTHON, '-m', 'http.server',
    '--bind', 'service-frontend', '--directory', '/tmp', '2728'
  )
  published = deployment.compose('port', 'service', '2728').stdout.strip()
  host, _, port = published.rpartition(':')
  # Published for the host's reverse proxy only, never beyond the host.
  assert host == '127.0.0.1', published
  deadline = time.monotonic() + 30
  while True:
    try:
      socket.create_connection((host, int(port)), 3).close()
      break
    except OSError:
      assert time.monotonic() < deadline, published
      time.sleep(0.5)
  for container in ('builder', 'publisher'):
    assert not _reach(deployment, container, 'service', 2728)


# Through the runners


def test_the_service_builds_through_the_builder(
    deployment: Deployment) -> None:
  result = deployment.python('service', (
    'import asyncio, json\n'
    'from edgacknowledg.forge.sandbox import RunnerClient, Sandbox\n'
    'async def main():\n'
    "  sandbox = Sandbox(builder = RunnerClient('ws://builder:2730'))\n"
    '  await sandbox.check()\n'
    '  allowed = await sandbox.run(\n'
    "    ['python3', '/edg/tools/bin/edg-rematerialize-test-run', '--help'],\n"
    '    timeout = 60)\n'
    "  refused = await sandbox.run(['sh', '-c', 'id'], timeout = 60)\n"
    '  print(json.dumps([allowed.returncode, allowed.output,\n'
    '                    refused.returncode, refused.output]))\n'
    'asyncio.run(main())\n'
  ))
  allowed_code, allowed_output, refused_code, refused_output = json.loads(
    result.stdout.strip().splitlines()[-1]
  )
  assert allowed_code == 0 and 'usage' in allowed_output, result
  assert refused_code != 0 and 'not allowed' in refused_output, result


def test_the_publisher_runs_tools_through_its_runner(
    deployment: Deployment) -> None:
  # As a pool process does: one long-lived connection to the tools runner.
  result = deployment.python('publisher', (
    'import asyncio, json\n'
    'from edgacknowledg.forge.runner.client import RunnerClient, '
    'RunnerError\n'
    'async def main():\n'
    "  client = RunnerClient('ws://tools:2732')\n"
    '  codes = []\n'
    "  for argv in (['ifc-dump', '--help'], ['cg_annotate', '--version']):\n"
    '    codes.append((await client.run(argv, timeout = 60)).returncode)\n'
    '  try:\n'
    "    await client.run(['sh', '-c', 'id'], timeout = 60)\n"
    "    codes.append('ran')\n"
    '  except RunnerError as ex:\n'
    '    codes.append(str(ex))\n'
    '  await client.close()\n'
    '  print(json.dumps(codes))\n'
    'asyncio.run(main())\n'
  ))
  ifc_dump, cg_annotate, refused = json.loads(
    result.stdout.strip().splitlines()[-1]
  )
  assert (ifc_dump, cg_annotate) == (0, 0), result
  # Only its allowed commands.
  assert 'not allowed' in refused, result


def test_the_publisher_serves_each_connection_itself(
    deployment: Deployment) -> None:
  # The service connects (here: a probe), and the publisher server answers
  # its requests, with a publisher of its own for the connection; its pool
  # processes stand ready for reviewers.
  result = deployment.python('service', (
    'import asyncio, json, uuid\n'
    'from websockets.asyncio.client import connect\n'
    'async def main():\n'
    "  async with connect('ws://publisher:2731') as connection:\n"
    '    await connection.send(json.dumps({\n'
    "      'api_host': 'acknowledg.invalid', 'api_port': 2727,\n"
    "      'api_key': None, 'ui_host': 'ui.invalid', 'ui_port': 443,\n"
    f"      'reviews_dir': '{STATE}/reviews'}}))\n"
    '    await connection.send(json.dumps({\n'
    "      'op': 'remove', 'review_id': str(uuid.uuid4())}))\n"
    '    while True:\n'
    '      answer = json.loads(await connection.recv())\n'
    "      if 'event' not in answer:\n"
    '        print(json.dumps(answer))\n'
    '        return\n'
    'asyncio.run(main())\n'
  ))
  assert result.stdout.strip().splitlines()[-1] == '{"ok": true}', result
  # The server and its pool processes (besides the container's init).
  processes = [
    line for line in deployment.processes('publisher').splitlines()[1:]
    if line.strip() != '' and 'docker-init' not in line
  ]
  pool = [line for line in processes
          if 'edgacknowledg.forge.publisher.pool_process' in line]
  assert len(pool) == 4, processes
  assert len(processes) == 5, processes
