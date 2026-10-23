# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''acknowledg-forge configuration, read from the environment.'''

import os

from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Optional

from edgacknowledg.client import (
  default_api_host,
  default_api_port,
  default_ui_host,
  default_ui_port,
)

ENV_PREFIX = 'ACKNOWLEDG_FORGE_'


class ConfigError(Exception):
  '''Raised for a missing or malformed setting.'''


def read_secret(credential_name: str,
                env_name: str,
                *,
                environ: Mapping[str, str] = os.environ) -> Optional[str]:
  '''Return a secret, preferring a systemd credential over the environment.

  Credentials (``LoadCredential=`` / ``LoadCredentialEncrypted=``) keep the
  secret out of the process environment; ``env_name`` is the fallback for
  local development.
  '''
  credentials_dir = environ.get('CREDENTIALS_DIRECTORY')
  if credentials_dir is not None:
    path = Path(credentials_dir, credential_name)
    if path.is_file():
      return path.read_text(encoding = 'utf-8').rstrip('\r\n')
  return environ.get(env_name)


def service_url(name: str,
               *,
               environ: Mapping[str, str] = os.environ) -> Optional[str]:
  '''The URL of the ``name`` service (another container's server) that
  ``ACKNOWLEDG_FORGE_<name>_SERVICE_URL`` sets, if any.
  '''
  url = environ.get(f"{ENV_PREFIX}{name}_SERVICE_URL", '').strip()
  if url == '':
    return None
  if not url.startswith('ws://'):
    raise ConfigError(f"{ENV_PREFIX}{name}_SERVICE_URL must be a ws:// URL")
  return url


def _environment_secrets(environ: Mapping[str, str],
                         names: dict[str, str]) -> tuple[str, ...]:
  '''The variables (of ``names``, credential name -> variable) a secret
  is actually read from, for lack of a credential.
  '''
  credentials_dir = environ.get('CREDENTIALS_DIRECTORY')
  return tuple(
    env_name for credential_name, env_name in names.items()
    if environ.get(env_name) is not None and (
      credentials_dir is None or
      not Path(credentials_dir, credential_name).is_file()
    )
  )


@dataclass(frozen = True)
class ForgeServiceConfig:
  '''Settings shared by every forge service.'''
  # Root for clones, worktrees, and (by default) the database.
  home: Path
  database_url: str
  repositories: tuple[str, ...]
  # Only pull requests targeting these branches are served (e.g. not ones
  # into feature branches). A pull request retargeted away from them has its
  # review removed, as if it were closed.
  base_branches: frozenset[str]
  # Forge API base URL override (e.g. GitHub Enterprise, self-hosted GitLab).
  api_url: Optional[str]
  # Seconds between polls; 0 polls once at startup only.
  poll_interval: float
  # 'host:port' to serve webhooks on; None runs no web server at all.
  webhook_bind: Optional[str]
  comments: bool
  # CI workflow (GitHub workflow file or id; GitLab: unused) whose runs are
  # tracked; None disables CI tracking.
  ci_workflow: Optional[str]
  # Seconds to wait for CI before a pull request counts as settled anyway.
  settle_timeout: float
  test_artifact: str
  bench_artifact: str
  max_artifact_bytes: int
  max_extracted_bytes: int
  max_concurrency: int
  rematerialize_timeout: float
  unsandboxed: bool
  # The builder's runner server and the publisher server, in their own
  # containers (see compose.yaml); required unless unsandboxed.
  builder_service_url: Optional[str]
  publisher_service_url: Optional[str]
  # A dev_tools-style directory (bin/ and pylibs/) with the EDG tooling the
  # service runs (edg-rematerialize-test-run, ifc-dump): a frozen, reviewed
  # copy (the container image embeds one), never the pull request's.
  tools_dir: Path
  forge_token: Optional[str]
  webhook_secret: Optional[str]
  acknowledg_api_host: str
  acknowledg_api_port: int
  acknowledg_api_key: Optional[str]
  acknowledg_ui_host: str
  acknowledg_ui_port: int
  # Secrets read from environment variables rather than credentials; any
  # process can read its own initial environment (/proc/self/environ).
  environment_secrets: tuple[str, ...] = ()

  @classmethod
  def from_environment(cls,
                       environ: Mapping[str, str] = os.environ
                       ) -> 'ForgeServiceConfig':
    def get(name: str) -> Optional[str]:
      value = environ.get(ENV_PREFIX + name)
      if value is None or value.strip() == '':
        return None
      return value.strip()

    def get_number(name: str, default: float) -> float:
      value = get(name)
      if value is None:
        return default
      try:
        number = float(value)
      except ValueError:
        raise ConfigError(f"{ENV_PREFIX}{name} must be a number") from None
      if number < 0:
        raise ConfigError(f"{ENV_PREFIX}{name} must not be negative")
      return number

    def get_flag(name: str) -> bool:
      value = get(name)
      if value is None:
        return False
      if value.lower() in ('1', 'true', 'yes', 'on'):
        return True
      if value.lower() in ('0', 'false', 'no', 'off'):
        return False
      raise ConfigError(f"{ENV_PREFIX}{name} must be a boolean (1/0)")

    def get_list(name: str) -> tuple[str, ...]:
      value = get(name)
      if value is None:
        return ()
      return tuple(item.strip() for item in value.split(',') if item.strip())

    home_value = get('HOME')
    if home_value is None:
      raise ConfigError(f"{ENV_PREFIX}HOME must be set")
    home = Path(home_value).absolute()

    repositories = get_list('REPOSITORIES')
    if len(repositories) == 0:
      raise ConfigError(f"{ENV_PREFIX}REPOSITORIES must list a repository")

    tools_value = get('TOOLS_DIR')
    if tools_value is None:
      raise ConfigError(f"{ENV_PREFIX}TOOLS_DIR must be set")
    tools_dir = Path(tools_value).absolute()
    if not (tools_dir / 'bin' / 'edg-rematerialize-test-run').is_file():
      raise ConfigError(
        f"{ENV_PREFIX}TOOLS_DIR ({tools_dir}) has no "
        f"bin/edg-rematerialize-test-run"
      )

    max_concurrency = int(get_number('MAX_CONCURRENCY', 2))
    if max_concurrency < 1:
      raise ConfigError(f"{ENV_PREFIX}MAX_CONCURRENCY must be at least 1")

    unsandboxed = get_flag('UNSANDBOXED')
    builder_service_url = service_url('BUILDER', environ = environ)
    publisher_service_url = service_url('PUBLISHER', environ = environ)
    if not unsandboxed and (builder_service_url is None or
                            publisher_service_url is None):
      raise ConfigError(
        f"{ENV_PREFIX}BUILDER_SERVICE_URL and "
        f"{ENV_PREFIX}PUBLISHER_SERVICE_URL must be set, as compose.yaml "
        f"does; for local development only, set {ENV_PREFIX}UNSANDBOXED=1 "
        f"instead"
      )

    return cls(
      home = home,
      # A credential, as a remote database's URL holds a password.
      database_url = (
        read_secret(
          'database-url',
          f"{ENV_PREFIX}DATABASE_URL",
          environ = environ
        ) or f"sqlite+aiosqlite:///{home / 'state.db'}"
      ),
      repositories = repositories,
      base_branches = frozenset(get_list('BASE_BRANCHES') or ('main',)),
      api_url = get('API_URL'),
      poll_interval = get_number('POLL_INTERVAL', 300),
      webhook_bind = get('WEBHOOK_BIND'),
      comments = get_flag('COMMENTS'),
      ci_workflow = get('CI_WORKFLOW'),
      settle_timeout = get_number('SETTLE_TIMEOUT', 2 * 60 * 60),
      test_artifact = get('TEST_ARTIFACT') or 'test-results-all',
      bench_artifact = get('BENCH_ARTIFACT') or 'bench-results',
      max_artifact_bytes = int(
        get_number('MAX_ARTIFACT_BYTES', 256 * 1024 * 1024)
      ),
      max_extracted_bytes = int(
        get_number('MAX_EXTRACTED_BYTES', 2 * 1024 * 1024 * 1024)
      ),
      max_concurrency = max_concurrency,
      rematerialize_timeout = get_number('REMATERIALIZE_TIMEOUT', 15 * 60),
      unsandboxed = unsandboxed,
      builder_service_url = builder_service_url,
      publisher_service_url = publisher_service_url,
      tools_dir = tools_dir,
      forge_token = read_secret(
        'forge-token',
        f"{ENV_PREFIX}TOKEN",
        environ = environ
      ),
      webhook_secret = read_secret(
        'webhook-secret',
        f"{ENV_PREFIX}WEBHOOK_SECRET",
        environ = environ
      ),
      acknowledg_api_host = default_api_host(),
      acknowledg_api_port = default_api_port(),
      acknowledg_api_key = read_secret(
        'acknowledg-api-key',
        'EDG_ACKNOWLEDG_API_KEY',
        environ = environ
      ),
      acknowledg_ui_host = default_ui_host(),
      acknowledg_ui_port = default_ui_port(),
      # (Not the database URL, which is only a secret if it holds a
      # password, and then the documentation says to use the credential.)
      environment_secrets = _environment_secrets(environ, {
        'forge-token': f"{ENV_PREFIX}TOKEN",
        'webhook-secret': f"{ENV_PREFIX}WEBHOOK_SECRET",
        'acknowledg-api-key': 'EDG_ACKNOWLEDG_API_KEY',
      }),
    )

  @property
  def ci_tracked(self) -> bool:
    return self.ci_workflow is not None
