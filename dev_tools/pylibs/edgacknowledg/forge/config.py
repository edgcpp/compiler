# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Typed TOML settings, command-line wiring, and service-only credentials.'''

import math
import os
import re
import tomllib

from dataclasses import dataclass, field, fields, replace
from pathlib import Path
from typing import Any, Mapping, Optional, Sequence
from urllib.parse import urlsplit

ENV_PREFIX = 'ACKNOWLEDG_FORGE_'
SETTINGS_VARIABLE = ENV_PREFIX + 'SETTINGS_FILE'

_LEGACY_ENVIRONMENT_VARIABLES = frozenset((
  'EDG_ACKNOWLEDG_API_HOST',
  'EDG_ACKNOWLEDG_API_PORT',
  'EDG_ACKNOWLEDG_UI_HOST',
  'EDG_ACKNOWLEDG_UI_PORT',
  'EDG_ACKNOWLEDG_API_KEY',
))

# The defaults of the limits on what a pull request and its CI artifacts
# may make the service do (``workspace``), which the settings may raise for
# large ones, given the memory (and time) they take.
#
# The most sparse-checkout patterns a checkout may have: matching costs
# patterns x paths, and they are built from pull request content. Their
# total size must also stay within a small multiple of the diff's (see
# DEFAULT_MAX_DIFF_BYTES): a test gets at most two patterns, each about as
# long as its path, and the only text that is part of many patterns, a
# suite's name, is short.
DEFAULT_MAX_PATTERNS = 20_000
# The largest diff (of tests/, in bytes) a pull request may have: it is
# kept with the review's rematerialized run (on disk, never in memory
# whole), and the builder applies all of it.
DEFAULT_MAX_DIFF_BYTES = 512 * 1024 * 1024
# The most entries an artifact zip may have, and the largest its central
# directory may be: zipfile reads all of that directory into memory, before
# we can count. CI's test results reach about 170,000 entries, of about 106
# bytes each there.
DEFAULT_MAX_ARTIFACT_ENTRIES = 500_000
DEFAULT_MAX_ARTIFACT_DIRECTORY_BYTES = 64 * 1024 * 1024
DEFAULT_MAX_RT_ENTRIES = 10_000
DEFAULT_MAX_RT_MEMBER_BYTES = 64 * 1024 * 1024


class ConfigError(Exception):
  '''Raised for a missing or malformed setting; never includes its value.'''


def check_environment(environ: Mapping[str, str]) -> None:
  '''Reject removed application variables without disclosing their values.'''
  removed = sorted(
    name for name in environ
    if (
      name.startswith(ENV_PREFIX) and name != SETTINGS_VARIABLE
    ) or name in _LEGACY_ENVIRONMENT_VARIABLES
  )
  if len(removed) != 0:
    raise ConfigError(
      'removed environment settings: ' + ', '.join(removed) +
      '; move application settings to lowercase TOML keys and secrets to '
      'CREDENTIALS_DIRECTORY'
    )


def settings_path(environ: Optional[Mapping[str, str]] = None) -> Path:
  environ = os.environ if environ is None else environ
  check_environment(environ)
  filename = environ.get(SETTINGS_VARIABLE, '').strip()
  if not filename:
    raise ConfigError(f"{SETTINGS_VARIABLE} must name a TOML file")
  return Path(filename).absolute()


def read_secret(name: str, directory: Optional[Path]) -> Optional[str]:
  '''Read a credential, including a development launcher's memfd link.'''
  if directory is None:
    return None
  try:
    value = (directory / name).read_text(encoding = 'utf-8').rstrip('\r\n')
  except FileNotFoundError:
    return None
  except (OSError, UnicodeError):
    raise ConfigError(f"cannot read credential {name}") from None
  return value or None


@dataclass(frozen = True)
class ForgeCredentials:
  '''Service-only secrets, kept separate from shared application settings.'''
  forge_token: Optional[str] = field(default = None, repr = False)
  webhook_secret: Optional[str] = field(default = None, repr = False)
  acknowledg_api_key: Optional[str] = field(default = None, repr = False)
  database_password: Optional[str] = field(default = None, repr = False)

  @classmethod
  def from_directory(cls, directory: Optional[Path]) -> 'ForgeCredentials':
    return cls(
      forge_token = read_secret('forge-token', directory),
      webhook_secret = read_secret('webhook-secret', directory),
      acknowledg_api_key = read_secret('acknowledg-api-key', directory),
      database_password = read_secret('database-password', directory)
    )


def check_service_bind(value: str) -> None:
  '''Validate the service's host:port listener address.'''
  host, _, port = value.rpartition(':')
  if (not host or not port.isascii() or not port.isdecimal() or
      len(port) > 5 or not 0 <= int(port) <= 65535):
    raise ConfigError('service_bind must be host:port with a valid port')


def check_service_url(value: str, key: str) -> str:
  '''A container's server: ``ws://`` only, within the deployment.'''
  url = urlsplit(value)
  if url.scheme != 'ws' or not url.netloc:
    raise ConfigError(f"{key} must be a ws:// URL")
  if url.path not in ('', '/'):
    raise ConfigError(f"{key} must have an empty path or '/'")
  return value


# A DNS name or IPv4 address, nothing that could change a URL's shape.
_HOST_NAME = re.compile(r'[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?')
_LOOPBACK_HOSTS = frozenset(('localhost', '127.0.0.1', '::1'))
# The AcknowlEDG client selects TLS (wss://, https://) by this port alone.
TLS_PORT = 443


@dataclass(frozen = True)
class SqliteDatabaseConfig:
  '''Non-secret SQLite connection settings.'''
  path: Path


@dataclass(frozen = True)
class PostgresqlUnixDatabaseConfig:
  '''Non-secret PostgreSQL connection settings for a Unix socket.'''
  socket_directory: Path
  name: str
  username: str
  port: int = 5432


@dataclass(frozen = True)
class PostgresqlTcpDatabaseConfig:
  '''Non-secret PostgreSQL connection settings for TCP.'''
  host: str
  name: str
  username: str
  port: int = 5432
  require_tls: bool = True


DatabaseConfig = (
  SqliteDatabaseConfig |
  PostgresqlUnixDatabaseConfig |
  PostgresqlTcpDatabaseConfig
)


@dataclass(frozen = True)
class ForgeServiceConfig:
  '''Non-secret application settings shared by the service and workers.'''
  home: Path
  tools_dir: Path
  repositories: tuple[str, ...]
  database: DatabaseConfig
  base_branches: frozenset[str] = frozenset(('main',))
  api_url: Optional[str] = None
  poll_interval: float = 300
  webhooks: bool = False
  comments: bool = False
  ci_workflow: Optional[str] = None
  settle_timeout: float = 2 * 60 * 60
  test_artifact: str = 'test-results-all'
  bench_artifact: str = 'bench-results'
  max_artifact_bytes: int = 256 * 1024 * 1024
  max_extracted_bytes: int = 2 * 1024 * 1024 * 1024
  max_artifact_entries: int = DEFAULT_MAX_ARTIFACT_ENTRIES
  max_artifact_directory_bytes: int = DEFAULT_MAX_ARTIFACT_DIRECTORY_BYTES
  max_rt_entries: int = DEFAULT_MAX_RT_ENTRIES
  max_rt_member_bytes: int = DEFAULT_MAX_RT_MEMBER_BYTES
  max_diff_bytes: int = DEFAULT_MAX_DIFF_BYTES
  max_sparse_patterns: int = DEFAULT_MAX_PATTERNS
  max_concurrency: int = 2
  rematerialize_timeout: float = 15 * 60
  unsandboxed: bool = False
  publisher_processes: int = 4
  publisher_request_timeout: float = 150
  acknowledg_api_host: str = 'localhost'
  acknowledg_api_port: int = 2727
  acknowledg_ui_host: str = 'localhost'
  acknowledg_ui_port: int = 5173
  # The deployment's wiring, which keeps each server to its one network.
  # compose.yaml sets it on the service's command line (``with_wiring``);
  # the TOML can't, so a settings change can't widen a listener.
  service_bind: Optional[str] = None
  builder_service_url: Optional[str] = None
  publisher_service_url: Optional[str] = None

  @staticmethod
  def from_file(path: Optional[Path] = None) -> 'ForgeServiceConfig':
    '''Read typed shared TOML settings without accessing credentials.

    Each service validates the settings required for its own role.
    '''
    path = settings_path() if path is None else path.absolute()
    try:
      with path.open('rb') as stream:
        data = tomllib.load(stream)
    except (OSError, ValueError):
      # TOML parser exceptions can quote the input, which may contain secrets.
      raise ConfigError(
        f"cannot read settings {path}: invalid or unreadable TOML"
      ) from None
    try:
      return _decode_settings(data, path)
    except ConfigError as ex:
      raise ConfigError(f"{path}: {ex}") from None

  def with_wiring(self, *,
                  service_bind: Optional[str] = None,
                  builder_service_url: Optional[str] = None,
                  publisher_service_url: Optional[str] = None
                  ) -> 'ForgeServiceConfig':
    '''These settings with the service's command-line wiring.'''
    if service_bind is not None:
      check_service_bind(service_bind)
    for key, value in (('builder_service_url', builder_service_url),
                       ('publisher_service_url', publisher_service_url)):
      if value is not None:
        check_service_url(value, key)
    return replace(
      self,
      service_bind = service_bind,
      builder_service_url = builder_service_url,
      publisher_service_url = publisher_service_url
    )

  @property
  def ci_tracked(self) -> bool:
    return self.ci_workflow is not None


def check_credential_transport(config: ForgeServiceConfig,
                               credentials: 'ForgeCredentials') -> None:
  '''Refuse to send the AcknowlEDG API key off this machine without TLS.'''
  if (credentials.acknowledg_api_key is not None and
      config.acknowledg_api_host not in _LOOPBACK_HOSTS and
      config.acknowledg_api_port != TLS_PORT):
    raise ConfigError(
      f"the acknowledg-api-key credential is only sent over TLS: set "
      f"acknowledg_api_port = {TLS_PORT} for a host other than localhost"
    )


_PATH_KEYS = frozenset(('home', 'tools_dir'))
_LIST_KEYS = frozenset(('repositories', 'base_branches'))
_FLAG_KEYS = frozenset(('webhooks', 'comments', 'unsandboxed'))
_NUMBER_KEYS = frozenset((
  'poll_interval',
  'settle_timeout',
  'rematerialize_timeout',
  'publisher_request_timeout',
))
_INT_KEYS = frozenset((
  'max_artifact_bytes',
  'max_extracted_bytes',
  'max_artifact_entries',
  'max_artifact_directory_bytes',
  'max_rt_entries',
  'max_rt_member_bytes',
  'max_diff_bytes',
  'max_sparse_patterns',
  'max_concurrency',
  'publisher_processes',
  'acknowledg_api_port',
  'acknowledg_ui_port',
))
_WIRING_KEYS = frozenset((
  'service_bind',
  'builder_service_url',
  'publisher_service_url',
))
# Former wiring settings, now command-line arguments of each server.
_REMOVED_WIRING_KEYS = frozenset((
  'builder_bind',
  'publisher_bind',
  'tools_bind',
  'tools_service_url',
))


def _check_api_url(value: str) -> None:
  '''The forge token is sent here: HTTPS, or HTTP to this machine only.

  Errors never quote the URL, which could hold userinfo.
  '''
  try:
    url = urlsplit(value)
    hostname = url.hostname
    has_userinfo = url.username is not None or url.password is not None
    # Only for its ValueError on a malformed port.
    _ = url.port
  except ValueError:
    raise ConfigError('api_url is not a valid URL') from None
  if url.scheme not in ('https', 'http') or not hostname:
    raise ConfigError('api_url must be an https:// URL')
  if url.scheme == 'http' and hostname not in _LOOPBACK_HOSTS:
    raise ConfigError(
      'api_url must use https:// for a host other than localhost'
    )
  if has_userinfo or url.query or url.fragment:
    raise ConfigError(
      'api_url must not hold credentials, a query, or a fragment; put the '
      'token in the forge-token credential'
    )


def _decode_settings(data: dict[str, Any], path: Path) -> ForgeServiceConfig:
  allowed = {f.name for f in fields(ForgeServiceConfig)} - _WIRING_KEYS
  values: dict[str, Any] = {}
  for key, value in data.items():
    if key in _WIRING_KEYS or key in _REMOVED_WIRING_KEYS:
      raise ConfigError(
        f"{key} is deployment wiring, set on the command line (compose.yaml), "
        'not in settings'
      )
    if key not in allowed:
      raise ConfigError(f"unknown or secret setting {key}")
    if key == 'database':
      # Decode after home, which supplies SQLite's default path.
      continue
    if key in _LIST_KEYS:
      if (not isinstance(value, list) or
          not all(isinstance(v, str) and v.strip() for v in value)):
        raise ConfigError(f"{key} must be an array of nonempty strings")
      values[key] = (
        frozenset(value) if key == 'base_branches'
        else tuple(value)
      )
    elif key in _FLAG_KEYS:
      if type(value) is not bool:
        raise ConfigError(f"{key} must be a boolean")
      values[key] = value
    elif key in _INT_KEYS or key in _NUMBER_KEYS:
      types = (int,) if key in _INT_KEYS else (int, float)
      if type(value) not in types or value < 0 or (
          isinstance(value, float) and not math.isfinite(value)):
        raise ConfigError(
          f"{key} must be a nonnegative finite " +
          ('integer' if key in _INT_KEYS else 'number')
        )
      values[key] = value
    else:
      if not isinstance(value, str) or not value.strip():
        raise ConfigError(f"{key} must be a nonempty string")
      if key in _PATH_KEYS:
        directory = Path(value)
        values[key] = (path.parent / directory).absolute()
      else:
        values[key] = value
  for key in ('home', 'tools_dir', 'repositories'):
    if key not in values or not values[key]:
      raise ConfigError(f"{key} is required")
  tool = values['tools_dir'] / 'bin' / 'edg-rematerialize-test-run'
  if not tool.is_file():
    raise ConfigError('tools_dir has no bin/edg-rematerialize-test-run')
  # The local AcknowlEDG's ports are the defaults; any other host needs its
  # port stated, since the port alone selects TLS (never a silent 80).
  for kind, local_port in (('api', 2727), ('ui', 5173)):
    host_key = f"acknowledg_{kind}_host"
    port_key = f"acknowledg_{kind}_port"
    host = values.get(host_key, 'localhost')
    if _HOST_NAME.fullmatch(host) is None:
      raise ConfigError(
        f"{host_key} must be a host name, without a scheme, port, or path"
      )
    if port_key not in values:
      if host != 'localhost':
        raise ConfigError(
          f"{port_key} is required for a host other than localhost "
          f"({TLS_PORT} for TLS)"
        )
      values[port_key] = local_port
  if 'api_url' in values:
    _check_api_url(values['api_url'])
  values['database'] = _decode_database(
    data.get('database', {}), path.parent, values['home'] / 'state.db'
  )
  config = ForgeServiceConfig(**values)
  for key in ('max_concurrency', 'publisher_processes'):
    if getattr(config, key) < 1:
      raise ConfigError(f"{key} must be at least 1")
  if config.publisher_request_timeout <= 0:
    raise ConfigError('publisher_request_timeout must be positive')
  for key in ('acknowledg_api_port', 'acknowledg_ui_port'):
    if not 1 <= getattr(config, key) <= 65535:
      raise ConfigError(f"{key} must be between 1 and 65535")
  return config


def _validate_database_fields(data: dict[str, Any],
                              keys: Sequence[str]) -> None:
  for key, value in data.items():
    if key not in keys:
      raise ConfigError(f"unknown or unsupported database setting {key}")
    if key == 'port':
      if type(value) is not int or not 1 <= value <= 65535:
        raise ConfigError('database.port must be an integer from 1 to 65535')
    elif key == 'require_tls':
      if type(value) is not bool:
        raise ConfigError('database.require_tls must be a boolean')
    elif not isinstance(value, str) or not value.strip():
      raise ConfigError(f"database.{key} must be a nonempty string")


def _decode_database(data: Any, directory: Path,
                      default_path: Path) -> DatabaseConfig:
  if not isinstance(data, dict):
    raise ConfigError('database must be a table')
  backend = data.get('backend', 'sqlite')
  if backend not in ('sqlite', 'postgresql-unix', 'postgresql-tcp'):
    raise ConfigError(
      'database.backend must be sqlite, postgresql-unix, or postgresql-tcp'
    )
  if backend == 'sqlite':
    _validate_database_fields(data, ('backend', 'path'))
    filename = data.get('path')
    if filename is None:
      path = default_path
    elif filename == ':memory:':
      path = Path(':memory:')
    else:
      path = (directory / filename).absolute()
    return SqliteDatabaseConfig(path = path)
  elif backend == 'postgresql-unix':
    _validate_database_fields(
      data,
      (
        'backend',
        'socket_directory',
        'port',
        'name',
        'username',
      )
    )
    for key in ('socket_directory', 'name', 'username'):
      if key not in data:
        raise ConfigError(f"database.{key} is required for postgresql-unix")
    socket_directory = Path(data['socket_directory'])
    if not socket_directory.is_absolute():
      raise ConfigError('database.socket_directory must be an absolute path')
    return PostgresqlUnixDatabaseConfig(
      socket_directory = socket_directory,
      port = data.get('port', 5432),
      name = data['name'],
      username = data['username']
    )
  else:
    _validate_database_fields(
      data,
      (
        'backend',
        'host',
        'port',
        'name',
        'username',
        'require_tls',
      )
    )
    for key in ('host', 'name', 'username'):
      if key not in data:
        raise ConfigError(f"database.{key} is required for postgresql-tcp")
    if '/' in data['host']:
      raise ConfigError('database.host must be a TCP host name or IP address')
    return PostgresqlTcpDatabaseConfig(
      host = data['host'],
      port = data.get('port', 5432),
      name = data['name'],
      username = data['username'],
      require_tls = data.get('require_tls', True)
    )
