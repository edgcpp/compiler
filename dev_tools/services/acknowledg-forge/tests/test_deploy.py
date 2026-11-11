# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The deploy helper (deploy-remote.py): it runs as root on a caller's
behalf, so its argument checks matter most.
'''

import importlib.util
import os
import stat
import sys

from pathlib import Path
from types import ModuleType

import pytest


def _load_deploy() -> ModuleType:
  path = Path(__file__).parent.parent / 'deploy-remote.py'
  spec = importlib.util.spec_from_file_location('deploy_remote', path)
  assert spec is not None and spec.loader is not None
  module = importlib.util.module_from_spec(spec)
  sys.modules['deploy_remote'] = module
  spec.loader.exec_module(module)
  return module


deploy = _load_deploy()

DIGEST = 'sha256:' + '0123456789abcdef' * 4
IMAGE = f"ghcr.io/edgcpp/acknowledg-forge@{DIGEST}"


def test_service_and_image_names() -> None:
  deploy.check_service('acknowledg-forge-github')
  for service in ('acknowledg-forge-', 'acknowledg-forge-GitHub',
                  'other-github', 'acknowledg-forge-a/b',
                  'acknowledg-forge-a\n'):
    with pytest.raises(deploy.DeployError):
      deploy.check_service(service)

  deploy.check_image(IMAGE)
  for image in (
    'ghcr.io/edgcpp/acknowledg-forge:latest',
    f"ghcr.io/other/acknowledg-forge@{DIGEST}",
    f"ghcrXio/edgcpp/acknowledg-forge@{DIGEST}",
    f"{IMAGE}0",
    f"{IMAGE}\n",
    IMAGE.upper(),
  ):
    with pytest.raises(deploy.DeployError):
      deploy.check_image(image)


def test_deploy_env(tmp_path: Path) -> None:
  content = deploy.deploy_env_content(IMAGE)
  assert content.splitlines()[1:] == [f"IMAGE={IMAGE}"]

  deploy_env = (
    tmp_path / 'acknowledg-forge' / 'acknowledg-forge-github.deploy.env'
  )
  assert deploy.write_if_changed(deploy_env, content)
  assert deploy_env.read_text() == content
  assert stat.S_IMODE(deploy_env.stat().st_mode) == 0o644
  assert stat.S_IMODE(deploy_env.parent.stat().st_mode) == 0o755
  assert not deploy.write_if_changed(deploy_env, content)
  assert deploy.write_if_changed(deploy_env, content + '\n')
  # Nothing is left behind.
  assert os.listdir(deploy_env.parent) == [deploy_env.name]


def test_main_checks_usage_and_root(capsys: pytest.CaptureFixture[str],
                                    monkeypatch: pytest.MonkeyPatch) -> None:
  monkeypatch.setattr(sys, 'argv', ['deploy', 'acknowledg-forge-github'])
  with pytest.raises(SystemExit) as exit_info:
    deploy.main()
  assert exit_info.value.code == 2
  assert 'usage:' in capsys.readouterr().err
  if os.geteuid() != 0:
    monkeypatch.setattr(sys, 'argv', [
      'deploy', 'acknowledg-forge-github', IMAGE
    ])
    assert deploy.main() == 1
    assert 'must run as root' in capsys.readouterr().err
