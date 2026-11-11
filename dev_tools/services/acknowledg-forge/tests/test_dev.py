# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The development launcher's anonymous-memory credentials and lifetime.'''

import os
import select
import signal
import stat
import subprocess
import sys
import textwrap

from pathlib import Path
from typing import Mapping

import pytest

from edgacknowledg.forge import dev
from edgacknowledg.forge.config import (
  ConfigError,
  ForgeServiceConfig,
  read_secret,
)

from conftest import make_tools_dir

pytestmark = pytest.mark.skipif(
  not hasattr(os, 'memfd_create') or not Path('/proc/self/fd').is_dir(),
  reason = 'needs Linux memfd and /proc'
)


def test_memory_credentials(tmp_path: Path) -> None:
  directory = tmp_path / 'credentials'
  values = {'forge-token': b'synthetic-secret'}
  with dev.memory_credentials(directory, values) as credentials:
    assert stat.S_IMODE(directory.stat().st_mode) == 0o700
    link = credentials / 'forge-token'
    assert link.is_symlink()
    assert stat.S_IMODE(link.stat().st_mode) == 0o600
    assert read_secret('forge-token', credentials) == 'synthetic-secret'
    assert read_secret('forge-token', credentials) == 'synthetic-secret'
    assert read_secret('webhook-secret', credentials) is None
    with pytest.raises(PermissionError):
      link.write_bytes(b'changed')
    # The child inherits no descriptor: it reopens the parent-held memfd.
    child = subprocess.run(
      [
        sys.executable, '-c',
        'from pathlib import Path; import os; '
        'p = Path(os.environ["CREDENTIALS_DIRECTORY"]) / "forge-token"; '
        'assert len(p.read_bytes()) == 16; '
        'assert len(p.read_bytes()) == 16; '
        'assert not any("synthetic-secret" in v for v in os.environ.values())',
      ],
      env = {'CREDENTIALS_DIRECTORY': str(credentials)}, close_fds = True,
      capture_output = True
    )
    assert child.returncode == 0, child.stderr
    # Only the link is on disk; its target identifies a deleted memfd.
    assert os.readlink(link).startswith(f"/proc/{os.getpid()}/fd/")
    target = os.readlink(os.readlink(link))
    assert 'memfd:forge-credential' in target
    assert list(directory.iterdir()) == [link]
  assert not directory.exists()
  with pytest.raises(FileNotFoundError):
    link.read_bytes()


def test_failed_launch_cleans_credentials(tmp_path: Path) -> None:
  directory = tmp_path / 'credentials'
  with pytest.raises(FileNotFoundError):
    with dev.memory_credentials(
      directory, {'forge-token': b'synthetic-secret'}
    ):
      dev.supervise(['/nonexistent/forge-command'], {})
  assert not directory.exists()


def test_caller_directory_is_not_deleted(tmp_path: Path) -> None:
  directory = tmp_path / 'credentials'
  directory.mkdir()
  marker = directory / 'existing'
  marker.write_text('keep')
  with pytest.raises(FileExistsError):
    with dev.memory_credentials(directory, {}):
      pass
  assert marker.read_text() == 'keep'


@pytest.mark.parametrize('sig', [signal.SIGTERM, signal.SIGINT, signal.SIGHUP])
@pytest.mark.parametrize('ignores_shutdown', [False, True])
def test_shutdown_reaps_child_and_removes_credentials(
                                             tmp_path: Path,
                                             sig: int,
                                             ignores_shutdown: bool) -> None:
  # The child announces readiness only after reading the anonymous secret.
  original = set(tmp_path.iterdir())
  program = textwrap.dedent('''
    import os, sys, tempfile
    from pathlib import Path
    from edgacknowledg.forge import dev
    from edgacknowledg.forge.dev import memory_credentials, supervise
    dev._SHUTDOWN_GRACE_PERIOD = 0.2
    with tempfile.TemporaryDirectory(dir = sys.argv[1]) as root:
      with memory_credentials(Path(root) / 'credentials',
                              {'forge-token': b'synthetic-secret'}
                              ) as directory:
        code = (
          'import os, signal, time; from pathlib import Path; '
          + (
            '[signal.signal(s, signal.SIG_IGN) for s in '
            '(signal.SIGTERM, signal.SIGINT, signal.SIGHUP)]; '
            if sys.argv[2] == 'True' else ''
          ) +
          'assert len((Path(os.environ["CREDENTIALS_DIRECTORY"]) / '
          '"forge-token").read_bytes()) == 16; '
          'print(os.getpid(), flush=True); time.sleep(60)'
        )
        status = supervise([sys.executable, '-c', code],
                           {'CREDENTIALS_DIRECTORY': str(directory)})
    sys.exit(status)
  ''')
  process = subprocess.Popen(
    [sys.executable, '-c', program, str(tmp_path), str(ignores_shutdown)],
    stdout = subprocess.PIPE,
    stderr = subprocess.PIPE,
    start_new_session = True
  )
  try:
    assert process.stdout is not None
    readable, _, _ = select.select([process.stdout], [], [], 10)
    assert readable, 'launcher did not start its child'
    line = process.stdout.readline()
    assert line.strip().isdigit(), (line, process.communicate(timeout = 5))
    child_pid = int(line)
    process.send_signal(sig)
    stdout, stderr = process.communicate(timeout = 10)
    expected = signal.SIGKILL if ignores_shutdown else sig
    assert process.returncode == 128 + expected, (stdout, stderr)
    assert set(tmp_path.iterdir()) == original
    assert not Path(f"/proc/{child_pid}").exists()
    assert b'synthetic-secret' not in stdout + stderr
  finally:
    if process.poll() is None:
      process.kill()
      process.communicate(timeout = 10)


def test_generated_toml_contains_no_credentials(tmp_path: Path) -> None:
  tools = make_tools_dir(tmp_path / 'tools')
  path = tmp_path / 'development.toml'
  path.write_text(dev.development_settings(tmp_path / 'state', tools))
  config = ForgeServiceConfig.from_file(path)
  assert config.unsandboxed and not config.comments
  assert config.repositories == ('edgcpp/compiler',)
  assert not hasattr(config, 'forge_token')


def test_launcher_owns_secret_lifetime(tmp_path: Path,
                                       monkeypatch: pytest.MonkeyPatch,
                                       capsys: pytest.CaptureFixture[str]
                                       ) -> None:
  monkeypatch.delenv('ACKNOWLEDG_FORGE_SETTINGS_FILE')
  monkeypatch.delenv('CREDENTIALS_DIRECTORY', raising = False)
  monkeypatch.setattr(sys, 'argv', ['forge-dev', '--use-gh-token'])
  monkeypatch.setenv('GH_TOKEN', 'synthetic-secret')
  monkeypatch.setenv('GITHUB_TOKEN', 'synthetic-secret')
  choices = []
  roots = []

  def token(use_gh: bool) -> bytes:
    choices.append(use_gh)
    return b'synthetic-secret'

  def child(command: list[str], environ: Mapping[str, str]) -> int:
    assert command == [sys.executable, '-m', 'acknowledg_forge_github']
    credentials = Path(environ['CREDENTIALS_DIRECTORY'])
    roots.append(credentials.parent)
    assert read_secret('forge-token', credentials) == 'synthetic-secret'
    config = ForgeServiceConfig.from_file(
      Path(environ['ACKNOWLEDG_FORGE_SETTINGS_FILE'])
    )
    assert config.unsandboxed and not config.comments
    assert not any('synthetic-secret' in v for v in environ.values())
    assert not any('synthetic-secret' in p.read_text()
                   for p in credentials.parent.rglob('*')
                   if p.is_file() and not p.is_symlink())
    return 7

  monkeypatch.setattr(dev, '_token', token)
  monkeypatch.setattr(dev, 'supervise', child)
  assert dev.main() == 7
  assert choices == [True]
  assert roots and not roots[0].exists()
  captured = capsys.readouterr()
  assert 'synthetic-secret' not in captured.out + captured.err


def test_interrupted_startup_cleans_credentials(tmp_path: Path) -> None:
  directory = tmp_path / 'credentials'
  with pytest.raises(dev._Termination):
    with dev._startup_signals():
      with dev.memory_credentials(
        directory, {'forge-token': b'synthetic-secret'}
      ):
        os.kill(os.getpid(), signal.SIGTERM)
  assert not directory.exists()


def test_launcher_preserves_caller_credentials(tmp_path: Path,
                                               monkeypatch: pytest.MonkeyPatch
                                               ) -> None:
  directory = tmp_path / 'existing'
  directory.mkdir(mode = 0o700)
  marker = directory / 'forge-token'
  marker.write_text('synthetic-secret')
  # Unrelated entries must not be opened or prevent startup.
  (directory / 'unrelated-directory').mkdir()
  os.mkfifo(directory / 'unrelated-fifo')
  monkeypatch.setenv('CREDENTIALS_DIRECTORY', str(directory))
  monkeypatch.setattr(sys, 'argv', ['forge-dev'])
  monkeypatch.setattr(dev, 'supervise', lambda command, environ: 0)
  assert dev.main() == 0
  assert marker.read_text() == 'synthetic-secret'


def test_credential_probe_rejects_unreadable_credential(
                                                      tmp_path: Path) -> None:
  (tmp_path / 'forge-token').mkdir()
  with pytest.raises(ConfigError, match = 'child cannot read'):
    dev._check_credentials(tmp_path)


def test_credential_probe_times_out(tmp_path: Path,
                                  monkeypatch: pytest.MonkeyPatch) -> None:
  os.mkfifo(tmp_path / 'forge-token')
  monkeypatch.setattr(dev, '_CREDENTIAL_PROBE_TIMEOUT', 0.1)
  with pytest.raises(ConfigError, match = 'access check timed out'):
    dev._check_credentials(tmp_path)


@pytest.mark.parametrize('kind', [
  'missing',
  'file',
  'no-token',
  'empty-token',
])
def test_launcher_rejects_invalid_credentials(
                                  tmp_path: Path,
                                  monkeypatch: pytest.MonkeyPatch,
                                  capsys: pytest.CaptureFixture[str],
                                  kind: str) -> None:
  directory = tmp_path / 'credentials'
  if kind == 'file':
    directory.write_text('not a directory', encoding = 'utf-8')
  elif kind != 'missing':
    directory.mkdir()
    if kind == 'empty-token':
      (directory / 'forge-token').write_bytes(b'\r\n')
  monkeypatch.setenv('CREDENTIALS_DIRECTORY', str(directory))
  monkeypatch.setattr(sys, 'argv', ['forge-dev'])

  def unexpected_child(command: list[str], environ: Mapping[str, str]) -> int:
    pytest.fail('Forge must not start with invalid credentials')

  monkeypatch.setattr(dev, 'supervise', unexpected_child)
  assert dev.main() == 2
  error = capsys.readouterr().err
  if kind in ('missing', 'file'):
    assert f"credentials path is not a directory: {directory}" in error
  else:
    assert 'forge-token credential is required' in error


def test_no_memory_file_support_has_no_disk_fallback(
                               monkeypatch: pytest.MonkeyPatch,
                               capsys: pytest.CaptureFixture[str]) -> None:
  monkeypatch.delattr(os, 'memfd_create')
  monkeypatch.setattr(sys, 'argv', ['forge-dev'])
  assert dev.main() == 2
  assert 'require Linux memfd' in capsys.readouterr().err
