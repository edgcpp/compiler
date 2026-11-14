# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Connection components remain separate through SQLAlchemy engine creation.'''

import ssl

from pathlib import Path
from typing import Any, Optional

import pytest

from sqlalchemy.engine import URL
from sqlalchemy.exc import ArgumentError

from edgacknowledg.forge.config import (
  DatabaseConfig,
  PostgresqlTcpDatabaseConfig,
  PostgresqlUnixDatabaseConfig,
  SqliteDatabaseConfig,
)
from edgacknowledg.forge.db import DatabaseConfigError, ForgeStore
from edgacknowledg.forge.db import store as store_module

from conftest import (
  _database_settings,
  _reset_database,
  make_tools_dir,
  write_settings,
)


async def test_postgresql_components_are_not_parsed_as_a_url(
                                      monkeypatch: pytest.MonkeyPatch) -> None:
  config = PostgresqlUnixDatabaseConfig(
    socket_directory = Path('/run/postgresql'),
    port = 5433,
    name = 'forge?name#fragment',
    username = 'user:@/%'
  )
  password = 'synthetic-secret:@/?#%'
  captured = []

  class EngineCreated(Exception):
    pass

  def engine(url: URL, **options: Any) -> None:
    captured.append(url)
    raise EngineCreated

  monkeypatch.setattr(store_module, 'create_async_engine', engine)
  with pytest.raises(EngineCreated):
    await ForgeStore.open(config, password = password)
  url = captured[0]
  assert isinstance(url, URL)
  assert url.drivername == 'postgresql+asyncpg'
  assert url.username == config.username
  assert url.password == password
  assert url.host == str(config.socket_directory)
  assert url.port == config.port
  assert url.database == config.name
  assert not url.query
  # Exercise the actual asyncpg dialect's argument construction, too.
  _, options = url.get_dialect()().create_connect_args(url)
  assert options['password'] == password
  assert options['host'] == '/run/postgresql'
  assert options['database'] == config.name


async def test_sqlite_path_is_literal(tmp_path: Path) -> None:
  path = tmp_path / 'state?mode=ro#fragment.db'
  store = await ForgeStore.open(SqliteDatabaseConfig(path = path))
  try:
    assert path.is_file()
    assert not (tmp_path / 'state').exists()
  finally:
    await store.aclose()


@pytest.mark.parametrize('password', [None, 'synthetic-secret'])
@pytest.mark.parametrize('reset', [False, True])
@pytest.mark.parametrize('backend', ['postgresql-tcp', 'postgresql-unix'])
@pytest.mark.parametrize('require_tls', [True, False])
async def test_postgresql_reset_and_store_connection_options(
                                               monkeypatch: pytest.MonkeyPatch,
                                               password: Optional[str],
                                               reset: bool,
                                               backend: str,
                                               require_tls: bool) -> None:
  config: DatabaseConfig
  if backend == 'postgresql-unix':
    config = PostgresqlUnixDatabaseConfig(
      socket_directory = Path('/run/postgresql'),
      port = 5432,
      name = 'forge',
      username = 'forge'
    )
  else:
    config = PostgresqlTcpDatabaseConfig(
      host = 'db.internal',
      port = 5432,
      name = 'forge',
      username = 'forge',
      require_tls = require_tls
    )
  captured = []

  class EngineCreated(Exception):
    pass

  def engine(url: URL, **options: Any) -> None:
    _, arguments = url.get_dialect()().create_connect_args(url)
    arguments.update(options['connect_args'])
    captured.append(arguments)
    raise EngineCreated

  monkeypatch.setattr(store_module, 'create_async_engine', engine)
  with pytest.raises(EngineCreated):
    if reset:
      await _reset_database(config, password = password)
    else:
      await ForgeStore.open(config, password = password)
  arguments = captured[0]
  assert arguments['host'] == (
    '/run/postgresql' if backend == 'postgresql-unix' else 'db.internal'
  )
  assert arguments.get('password') == password
  tls = arguments['ssl']
  if require_tls and backend == 'postgresql-tcp':
    assert isinstance(tls, ssl.SSLContext)
    assert tls.verify_mode == ssl.CERT_REQUIRED
    assert tls.check_hostname
  else:
    assert tls is False


async def test_engine_configuration_errors_do_not_expose_passwords(
                                      monkeypatch: pytest.MonkeyPatch) -> None:
  config = PostgresqlTcpDatabaseConfig(
    host = 'db.internal',
    port = 5432,
    name = 'forge',
    username = 'forge'
  )

  def engine(url: URL, **options: Any) -> None:
    raise ArgumentError('synthetic-secret')

  monkeypatch.setattr(store_module, 'create_async_engine', engine)
  with pytest.raises(DatabaseConfigError) as failure:
    await ForgeStore.open(config, password = 'synthetic-secret')
  assert str(failure.value) == (
    'database connection settings are invalid or unsupported'
  )


async def test_sqlite_does_not_ignore_a_database_password(
                                                tmp_path: Path) -> None:
  with pytest.raises(DatabaseConfigError, match = 'accepts no'):
    await ForgeStore.open(
      SqliteDatabaseConfig(path = tmp_path / 'state.db'),
      password = 'synthetic-secret'
    )
  assert not (tmp_path / 'state.db').exists()


@pytest.mark.parametrize('require_tls', [True, False])
def test_database_fixture_loads_toml_and_separate_credentials(
                                                    tmp_path: Path,
                                                    require_tls: bool) -> None:
  settings = tmp_path / 'database.toml'
  write_settings(
    settings,
    {
      'home': str(tmp_path / 'home'),
      'tools_dir': str(make_tools_dir(tmp_path / 'tools')),
      'repositories': ['edg/compiler'],
      'database': {
        'backend': 'postgresql-tcp',
        'host': 'db.internal',
        'port': 5433,
        'name': 'forge',
        'username': 'forge',
        'require_tls': require_tls,
      },
    }
  )
  directory = tmp_path / 'credentials'
  directory.mkdir()
  (directory / 'database-password').write_text('synthetic-secret\n')
  database, credentials = _database_settings(str(settings), str(directory))
  assert database == PostgresqlTcpDatabaseConfig(
    host = 'db.internal',
    port = 5433,
    name = 'forge',
    username = 'forge',
    require_tls = require_tls
  )
  assert credentials.database_password == 'synthetic-secret'


def test_database_fixture_defaults_to_in_memory_sqlite() -> None:
  database, credentials = _database_settings(None, None)
  assert database == SqliteDatabaseConfig(path = Path(':memory:'))
  assert credentials.database_password is None


async def test_reset_database_uses_configured_sqlite_path(
                                                       tmp_path: Path) -> None:
  from sqlalchemy import inspect

  from edgacknowledg.forge.db import create_database_engine

  config = SqliteDatabaseConfig(path = tmp_path / 'state.db')
  store = await ForgeStore.open(config)
  await store.aclose()
  await _reset_database(config)
  engine = create_database_engine(config)
  try:
    async with engine.connect() as connection:
      tables = await connection.run_sync(
        lambda c: inspect(c).get_table_names()
      )
    assert tables == []
  finally:
    await engine.dispose()
  store = await ForgeStore.open(config)
  await store.aclose()
