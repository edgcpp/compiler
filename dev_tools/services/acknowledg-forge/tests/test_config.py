# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Typed TOML settings, migration errors, and credentials-only secrets.'''


from pathlib import Path
from typing import Any

import pytest

from edgacknowledg.forge.config import (
  ConfigError,
  DatabaseConfig,
  ForgeCredentials,
  ForgeServiceConfig,
  PostgresqlTcpDatabaseConfig,
  PostgresqlUnixDatabaseConfig,
  SqliteDatabaseConfig,
  check_credential_transport,
  check_environment,
  settings_path,
)
from edgacknowledg.forge.publisher.pool import _process_env

from conftest import make_tools_dir, write_settings


def settings_file(root: Path, **overrides: Any) -> Path:
  path = root / 'config.toml'
  values = {
    'home': str(root / 'home'),
    'tools_dir': str(make_tools_dir(root / 'tools')),
    'repositories': ['edg/compiler'],
    'unsandboxed': True,
    **overrides,
  }
  write_settings(path, values)
  return path


def test_native_settings_and_defaults(tmp_path: Path) -> None:
  path = settings_file(tmp_path)
  defaults = ForgeServiceConfig.from_file(path)
  assert (defaults.max_rt_entries, defaults.max_rt_member_bytes) == (
    10_000, 64 * 1024 * 1024
  )
  assert defaults.base_branches == frozenset(('main',))
  assert defaults.database == SqliteDatabaseConfig(
    path = tmp_path / 'home' / 'state.db'
  )
  assert defaults.acknowledg_api_port == 2727
  path = settings_file(
    tmp_path, max_rt_entries = 12,
    max_rt_member_bytes = 1024, comments = True,
    poll_interval = 1.5,
    base_branches = ['main', 'release'],
    test_artifact = 'literal ${NAME}',
    acknowledg_api_host = 'api.example',
    acknowledg_api_port = 443,
    api_url = 'https://ghe.example/api/v3'
  )
  config = ForgeServiceConfig.from_file(path)
  assert (config.max_rt_entries, config.max_rt_member_bytes) == (12, 1024)
  assert config.comments and config.poll_interval == 1.5
  assert config.base_branches == frozenset(('main', 'release'))
  assert config.test_artifact == 'literal ${NAME}'
  assert config.acknowledg_api_port == 443
  # A local forge (e.g. a test double) may use plain HTTP.
  path = settings_file(tmp_path, api_url = 'http://127.0.0.1:8080/api/v3')
  assert ForgeServiceConfig.from_file(path).api_url.startswith('http://')


def test_postgresql_settings(tmp_path: Path) -> None:
  config = ForgeServiceConfig.from_file(
    settings_file(
      tmp_path,
      database = {
        'backend': 'postgresql-tcp',
        'host': 'db.internal',
        'name': 'acknowledg_forge',
        'username': 'acknowledg_forge',
      }
    )
  )
  assert config.database == PostgresqlTcpDatabaseConfig(
    host = 'db.internal',
    port = 5432,
    name = 'acknowledg_forge',
    username = 'acknowledg_forge'
  )
  assert not hasattr(config.database, 'password')


def test_passwordless_postgresql_socket(tmp_path: Path) -> None:
  config = ForgeServiceConfig.from_file(
    settings_file(
      tmp_path,
      database = {
        'backend': 'postgresql-unix',
        'socket_directory': '/run/postgresql',
        'name': 'acknowledg_forge',
        'username': 'acknowledg-forge',
      }
    )
  )
  credentials = ForgeCredentials.from_directory(tmp_path)
  assert credentials.database_password is None
  assert isinstance(config.database, PostgresqlUnixDatabaseConfig)
  assert config.database.socket_directory == Path('/run/postgresql')
  assert config.database.port == 5432
  assert not hasattr(config.database, 'host')
  assert not hasattr(config.database, 'require_tls')


@pytest.mark.parametrize('require_tls', ['true', 1, 0])
def test_postgresql_tls_requires_boolean(tmp_path: Path,
                                         require_tls: Any) -> None:
  with pytest.raises(ConfigError, match = 'database.require_tls'):
    ForgeServiceConfig.from_file(
      settings_file(
        tmp_path,
        database = {
          'backend': 'postgresql-tcp',
          'host': 'db.internal',
          'name': 'forge',
          'username': 'forge',
          'require_tls': require_tls,
        }
      )
    )


@pytest.mark.parametrize(
  'fields',
  [
    {},
    {'socket_directory': 'relative/path'},
    {'socket_directory': ''},
    {'socket_directory': 1},
    {'socket_directory': '/run/postgresql', 'host': 'localhost'},
    {'socket_directory': '/run/postgresql', 'require_tls': True},
    {'socket_directory': '/run/postgresql', 'require_tls': False},
    {'socket_directory': '/run/postgresql', 'port': 0},
    {'socket_directory': '/run/postgresql', 'name': ''},
    {'socket_directory': '/run/postgresql', 'username': ''},
  ]
)
def test_postgresql_unix_schema(tmp_path: Path,
                                fields: dict[str, Any]) -> None:
  with pytest.raises(ConfigError, match = 'database'):
    ForgeServiceConfig.from_file(
      settings_file(
        tmp_path,
        database = {
          'backend': 'postgresql-unix',
          'name': 'forge',
          'username': 'forge',
          **fields,
        }
      )
    )


@pytest.mark.parametrize(
  'fields',
  [
    {'host': '/run/postgresql'},
    {'socket_directory': '/run/postgresql'},
  ]
)
def test_postgresql_tcp_rejects_socket_settings(tmp_path: Path,
                                               fields: dict[str, Any]) -> None:
  with pytest.raises(ConfigError, match = 'database'):
    ForgeServiceConfig.from_file(
      settings_file(
        tmp_path,
        database = {
          'backend': 'postgresql-tcp',
          'host': 'db.internal',
          'name': 'forge',
          'username': 'forge',
          **fields,
        }
      )
    )


@pytest.mark.parametrize('key, value', [
  ('max_rt_entries', -1), ('max_rt_entries', 1.5),
  ('max_rt_entries', True), ('max_rt_entries', '12'),
  ('max_rt_member_bytes', 'no'), ('comments', 'true'),
  ('repositories', 'edg/compiler'), ('repositories', []),
  ('base_branches', [1]), ('poll_interval', float('inf')),
  ('max_concurrency', 0), ('publisher_processes', 0),
  ('publisher_request_timeout', 0), ('acknowledg_api_port', 65536),
  ('webhook_bind', 'localhost:1'),
  ('forge_token', 'synthetic-secret'), ('webhook_secret', 'synthetic-secret'),
  ('acknowledg_api_key', 'synthetic-secret'),
  # Wiring is compose.yaml's, so settings can't widen a listener.
  ('service_bind', '0.0.0.0:2728'), ('builder_service_url', 'ws://b:1'),
  ('publisher_service_url', 'ws://p:1'), ('publisher_bind', '0.0.0.0:2731'),
  ('builder_bind', '0.0.0.0:2730'), ('tools_bind', '0.0.0.0:2732'),
  ('tools_service_url', 'ws://tools:2732'),
  # A host other than localhost needs its port (which selects TLS) stated.
  ('acknowledg_api_host', 'api.example'),
  ('acknowledg_ui_host', 'ui.example'),
  ('acknowledg_api_host', 'evil.example/x?'),
  ('acknowledg_api_host', 'wss://api.example'),
  ('acknowledg_ui_host', 'ui.example:443'),
  ('api_url', 'http://ghe.example/api/v3'),
  ('api_url', 'ftp://ghe.example/api/v3'),
  ('api_url', 'https://user:synthetic-secret@ghe.example/api/v3'),
  ('api_url', 'https://ghe.example/api/v3?token=synthetic-secret'),
  ('api_url', 'https://ghe.example:synthetic-secret/api/v3'),
  ('api_url', 'synthetic-secret'),
])
def test_invalid_settings(tmp_path: Path, key: str, value: Any) -> None:
  path = settings_file(tmp_path)
  # JSON's Infinity is not TOML, so write native TOML for this case.
  if isinstance(value, float) and value == float('inf'):
    with path.open('a') as stream:
      stream.write(f"{key} = inf\n")
  else:
    path = settings_file(tmp_path, **{key: value})
  with pytest.raises(ConfigError) as failure:
    ForgeServiceConfig.from_file(path)
  assert str(path) in str(failure.value)
  assert 'synthetic-secret' not in str(failure.value)


def test_missing_and_malformed_settings(tmp_path: Path) -> None:
  path = tmp_path / 'missing.toml'
  with pytest.raises(ConfigError, match = 'cannot read settings'):
    ForgeServiceConfig.from_file(path)
  path.write_text('bad = "synthetic-secret')
  with pytest.raises(ConfigError) as failure:
    ForgeServiceConfig.from_file(path)
  assert 'synthetic-secret' not in str(failure.value)
  path.write_text('max_rt_entries = 12\n')
  with pytest.raises(ConfigError, match = 'home is required'):
    ForgeServiceConfig.from_file(path)


@pytest.mark.parametrize('name', [
  'ACKNOWLEDG_FORGE_MAX_RT_ENTRIES', 'ACKNOWLEDG_FORGE_HOME',
  'ACKNOWLEDG_FORGE_TOOLS_SERVICE_URL', 'ACKNOWLEDG_FORGE_TOKEN',
  'ACKNOWLEDG_FORGE_WEBHOOK_SECRET', 'ACKNOWLEDG_FORGE_DATABASE_URL',
  'EDG_ACKNOWLEDG_API_KEY', 'EDG_ACKNOWLEDG_API_HOST',
  'EDG_ACKNOWLEDG_API_PORT', 'EDG_ACKNOWLEDG_UI_HOST',
  'EDG_ACKNOWLEDG_UI_PORT',
])
def test_removed_environment(name: str) -> None:
  with pytest.raises(ConfigError) as failure:
    check_environment({name: 'synthetic-secret'})
  assert name in str(failure.value)
  assert 'synthetic-secret' not in str(failure.value)
  assert 'TOML' in str(failure.value)


def test_bootstrap_and_worker_environment(tmp_path: Path,
                                          monkeypatch: pytest.MonkeyPatch
                                          ) -> None:
  with pytest.raises(ConfigError, match = 'must name a TOML file'):
    settings_path({})
  path = settings_file(tmp_path)
  monkeypatch.setenv('ACKNOWLEDG_FORGE_SETTINGS_FILE', str(path))
  monkeypatch.setenv('CREDENTIALS_DIRECTORY', '/private/credentials')
  worker = _process_env()
  assert worker['ACKNOWLEDG_FORGE_SETTINGS_FILE'] == str(path)
  assert 'CREDENTIALS_DIRECTORY' not in worker
  assert not any(name.startswith('EDG_ACKNOWLEDG_') for name in worker)
  assert ForgeServiceConfig.from_file().max_rt_entries == 10_000


def test_credentials_are_service_only(tmp_path: Path) -> None:
  path = settings_file(
    tmp_path, database = {
      'backend': 'postgresql-tcp', 'host': 'db.internal',
      'name': 'acknowledg_forge', 'username': 'acknowledg_forge',
    }
  )
  credentials = tmp_path / 'credentials'
  credentials.mkdir(mode = 0o700)
  for name in ('forge-token', 'webhook-secret', 'acknowledg-api-key'):
    (credentials / name).write_text('synthetic-secret\n')
  (credentials / 'database-password').write_text('synthetic-secret\n')
  secrets = ForgeCredentials.from_directory(credentials)
  assert secrets.forge_token == secrets.webhook_secret == 'synthetic-secret'
  assert secrets.acknowledg_api_key == 'synthetic-secret'
  assert secrets.database_password == 'synthetic-secret'
  plain = ForgeServiceConfig.from_file(path)
  assert not hasattr(plain, 'forge_token')
  assert not hasattr(plain, 'webhook_secret')
  assert not hasattr(plain, 'acknowledg_api_key')
  assert isinstance(plain.database, PostgresqlTcpDatabaseConfig)
  assert plain.database.host == 'db.internal'
  assert not hasattr(plain.database, 'password')
  assert 'synthetic-secret' not in repr(secrets)
  assert ForgeCredentials.from_directory(None) == ForgeCredentials()
  (credentials / 'forge-token').write_bytes(b'\xff')
  with pytest.raises(
    ConfigError,
    match = 'cannot read credential forge-token'
  ):
    ForgeCredentials.from_directory(credentials)


@pytest.mark.parametrize('wiring', [
  {'service_bind': 'bad'}, {'service_bind': 'host:²'},
  {'builder_service_url': 'http://builder:2730'},
  {'publisher_service_url': 'wss://publisher:2731'},
  {'builder_service_url': 'ws:builder'},
  {'builder_service_url': 'ws:///missing-host'},
  {'builder_service_url': 'ws://builder:2730/missing'},
  {'publisher_service_url': 'ws://publisher:2731/missing'},
])
def test_invalid_wiring(tmp_path: Path, wiring: dict[str, str]) -> None:
  config = ForgeServiceConfig.from_file(settings_file(tmp_path))
  with pytest.raises(ConfigError, match = next(iter(wiring))):
    config.with_wiring(**wiring)


@pytest.mark.parametrize('path', ['', '/'])
def test_root_service_urls(tmp_path: Path, path: str) -> None:
  config = ForgeServiceConfig.from_file(settings_file(tmp_path))
  builder = f"ws://builder:2730{path}"
  publisher = f"ws://publisher:2731{path}"
  wired = config.with_wiring(
    builder_service_url = builder,
    publisher_service_url = publisher
  )
  assert wired.builder_service_url == builder
  assert wired.publisher_service_url == publisher


@pytest.mark.parametrize('host, port, key, allowed', [
  ('localhost', 2727, 'synthetic-secret', True),
  ('api.example', 443, 'synthetic-secret', True),
  ('api.example', 80, None, True),
  ('api.example', 80, 'synthetic-secret', False),
  ('api.example', 2727, 'synthetic-secret', False),
])
def test_api_key_is_only_sent_over_tls(tmp_path: Path, host: str, port: int,
                                       key: str | None,
                                       allowed: bool) -> None:
  config = ForgeServiceConfig.from_file(
    settings_file(
      tmp_path, acknowledg_api_host = host, acknowledg_api_port = port
    )
  )
  credentials = ForgeCredentials(acknowledg_api_key = key)
  if allowed:
    check_credential_transport(config, credentials)
  else:
    with pytest.raises(ConfigError, match = 'only sent over TLS') as failure:
      check_credential_transport(config, credentials)
    assert 'synthetic-secret' not in str(failure.value)


async def test_service_refuses_the_api_key_without_tls(tmp_path: Path) -> None:
  from edgacknowledg.forge import service
  from fakes import FakeForge

  config = ForgeServiceConfig.from_file(
    settings_file(
      tmp_path, acknowledg_api_host = 'api.example', acknowledg_api_port = 80
    )
  )
  with pytest.raises(ConfigError, match = 'only sent over TLS'):
    await service.run_service(
      config, ForgeCredentials(acknowledg_api_key = 'synthetic-secret'),
      service.ForgeComponents(FakeForge(tmp_path))
    )


@pytest.mark.parametrize('wiring', [
  {}, {'builder_service_url': 'ws://builder:2730'},
  {'publisher_service_url': 'ws://publisher:2731'},
])
async def test_sandboxed_service_needs_endpoints(tmp_path: Path,
                                                wiring: dict[str, str]
                                                ) -> None:
  from edgacknowledg.forge import service
  from fakes import FakeForge

  path = settings_file(tmp_path, unsandboxed = False)
  # Shared loading succeeds for roles that do not need these endpoints.
  config = ForgeServiceConfig.from_file(path).with_wiring(**wiring)
  with pytest.raises(ConfigError, match = '--builder-service-url'):
    await service.run_service(
      config, ForgeCredentials(), service.ForgeComponents(FakeForge(tmp_path))
    )


@pytest.mark.parametrize('forge', ['github', 'gitlab'])
async def test_forge_builders_receive_credentials_separately(tmp_path: Path,
                                                             forge: str
                                                             ) -> None:
  import importlib

  module = importlib.import_module(f"acknowledg_forge_{forge}.__main__")
  config = ForgeServiceConfig.from_file(
    settings_file(
      tmp_path, comments = True, webhooks = True
    )
  ).with_wiring(service_bind = '127.0.0.1:0')
  credentials = ForgeCredentials(
    forge_token = 'synthetic-token',
    webhook_secret = 'synthetic-webhook'
  )
  components = module.build(config, credentials)
  try:
    assert components.webhooks is not None
    assert components.commenter is not None
    assert not hasattr(config, 'forge_token')
  finally:
    await components.forge.aclose()
  anonymous = module.build(config, ForgeCredentials())
  try:
    assert anonymous.webhooks is anonymous.commenter is None
  finally:
    await anonymous.forge.aclose()


@pytest.mark.parametrize('password', [None, 'synthetic-secret'])
async def test_database_password_is_passed_separately(
                          tmp_path: Path, monkeypatch: pytest.MonkeyPatch,
                          password: str | None) -> None:
  from edgacknowledg.forge import service
  from fakes import FakeForge

  config = ForgeServiceConfig.from_file(settings_file(tmp_path))
  credentials = ForgeCredentials(database_password = password)
  opened = []

  class DatabaseOpened(Exception):
    pass

  async def check(sandbox: object) -> None:
    pass

  async def open_database(database: DatabaseConfig,
                          *, password: str | None = None) -> None:
    opened.append((database, password))
    # Stop before constructing the rest of the service.
    raise DatabaseOpened

  monkeypatch.setattr(service.Sandbox, 'check', check)
  monkeypatch.setattr(service.ForgeStore, 'open', open_database)
  with pytest.raises(DatabaseOpened):
    await service._run_components(
      config, credentials, service.ForgeComponents(FakeForge(tmp_path)),
      None, 'unused'
    )
  assert opened == [(config.database, password)]
  assert isinstance(config.database, SqliteDatabaseConfig)
  assert config.database.path == tmp_path / 'home' / 'state.db'


@pytest.mark.parametrize(
  'database',
  [
    'not-a-table',
    {'backend': 'mysql'},
    {'backend': True},
    {'path': ''},
    {'path': 1},
    {'password': 'synthetic-secret'},
    {'query': {'password': 'synthetic-secret'}},
    {'connect_args': {'password': 'synthetic-secret'}},
    {'backend': 'sqlite', 'host': 'db.internal'},
    {'backend': 'postgresql-tcp', 'path': 'state.db'},
    {'backend': 'postgresql-tcp'},
    {'backend': 'postgresql-tcp', 'host': 'db.internal'},
    {'backend': 'postgresql-tcp', 'host': 'db.internal', 'name': 'forge'},
    {
      'backend': 'postgresql-tcp',
      'host': '',
      'name': 'forge',
      'username': 'forge',
    },
    {
      'backend': 'postgresql-tcp',
      'host': 'db',
      'name': 'forge',
      'username': 'forge',
      'port': True,
    },
    {
      'backend': 'postgresql-tcp',
      'host': 'db',
      'name': 'forge',
      'username': 'forge',
      'port': '5432',
    },
    {
      'backend': 'postgresql-tcp',
      'host': 'db',
      'name': 'forge',
      'username': 'forge',
      'port': 65536,
    },
  ]
)
def test_database_schema_rejects_unsupported_fields(tmp_path: Path,
                                                    database: Any) -> None:
  with pytest.raises(ConfigError) as failure:
    ForgeServiceConfig.from_file(settings_file(tmp_path, database = database))
  assert 'database' in str(failure.value)
  assert 'synthetic-secret' not in str(failure.value)


def test_sqlite_paths(tmp_path: Path) -> None:
  path = settings_file(tmp_path)
  with path.open('a') as stream:
    stream.write('[database]\nbackend = "sqlite"\npath = "local.db"\n')
  assert ForgeServiceConfig.from_file(path).database == SqliteDatabaseConfig(
    path = tmp_path / 'local.db'
  )
  path = settings_file(tmp_path, database = {'path': ':memory:'})
  assert ForgeServiceConfig.from_file(path).database == SqliteDatabaseConfig(
    path = Path(':memory:')
  )
