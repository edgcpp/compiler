#!/usr/bin/python3 -I

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Point an acknowledg-forge deployment at a new image and restart it.

Installed by the host's administrator as a root-owned helper (e.g.
/usr/local/libexec/acknowledg-forge-deploy) that the deploy user may run
with sudo, and nothing else as root:

  sudo /usr/local/libexec/acknowledg-forge-deploy \\
    <service> <image@sha256:...> <acknowledg-api-url> <acknowledg-ui-url>

The URLs are the public AcknowlEDG endpoints, e.g. wss://api.example and
https://ui.example (the same values the web client is built with).

The service is a systemd unit, /etc/systemd/system/<service>.service, that
runs the Docker Compose deployment (compose.yaml) with the settings in
CONFIG_DIR, all of which the administrator owns (see README.md), along with
its credentials. This script only manages one file there,
<service>.deploy.env, which sets the image (pinned by digest, from
IMAGE_REPOSITORY only) and the EDG_ACKNOWLEDG_{API,UI}_{HOST,PORT} the URLs
translate to, and restarts the service only when that file changes. Since
it runs as root on a caller's behalf, every argument is strictly
validated.

It uses only the standard library and runs isolated (python3 -I), so
nothing from the environment or the caller's directory can change it.
'''

import argparse
import os
import re
import subprocess
import sys
import tempfile
import time

from pathlib import Path
from typing import NamedTuple
from urllib.parse import urlsplit, urlunsplit

# The only repository images may come from.
IMAGE_REPOSITORY = 'ghcr.io/edgcpp/acknowledg-forge'

# The administrator's deployment configuration: compose.yaml, and each
# service's <service>.env and <service>.settings.
CONFIG_DIR = Path('/etc/acknowledg-forge')
# Where the administrator installs each service's unit.
UNITS_DIR = Path('/etc/systemd/system')
# Seconds to give the service to fail on startup (bad configuration,
# missing access) before checking that it's still up.
STARTUP_GRACE_S = 5

_SERVICE_NAME = re.compile(r'acknowledg-forge-[a-z0-9-]+')
_IMAGE = re.compile(re.escape(IMAGE_REPOSITORY) + r'@sha256:[0-9a-f]{64}')
_HOST_NAME = re.compile(r'[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?')
_PORT = re.compile(r'[0-9]{1,5}')

TLS_PORT = 443


class DeployError(Exception):
  '''A deployment that can't go ahead; the message says why.'''


class Endpoint(NamedTuple):
  host: str
  port: int


def parse_url(url: str, *, plain: str, tls: str, label: str) -> Endpoint:
  '''The host and port of ``url``, which must use the ``plain`` or ``tls``
  scheme (e.g. ws and wss), a host name, an optional port, and no path.

  The service picks TLS by port alone (443), so only URLs it can honor
  exactly are accepted: TLS on 443 and plain on any other port, never a
  silent downgrade.
  '''
  shape_error = DeployError(
    f"the AcknowlEDG {label} must be {plain}:// or {tls}:// followed by a "
    f"host name and optional port, with no path"
  )
  # urlsplit quietly drops some characters (tabs, newlines) and empty
  # parts ('?', '#'); anything it wouldn't reproduce exactly is refused.
  parts = urlsplit(url)
  if (url != urlunsplit(parts) or
      parts.scheme not in (plain, tls) or
      parts.path not in ('', '/') or
      parts.query != '' or
      parts.fragment != ''):
    raise shape_error
  host, colon, port_text = parts.netloc.partition(':')
  if _HOST_NAME.fullmatch(host) is None:
    raise shape_error
  if colon != '' and _PORT.fullmatch(port_text) is None:
    raise shape_error

  is_tls = parts.scheme == tls
  if colon == '':
    port = TLS_PORT if is_tls else 80
  else:
    port = int(port_text)
    if not 1 <= port <= 65535:
      raise DeployError(f"the AcknowlEDG {label} has an invalid port")
  if is_tls and port != TLS_PORT:
    raise DeployError(
      f"the AcknowlEDG {label} can only use {tls}:// on port {TLS_PORT}"
    )
  if not is_tls and port == TLS_PORT:
    raise DeployError(
      f"the AcknowlEDG {label} must use {tls}:// on port {TLS_PORT}"
    )
  return Endpoint(host, port)


def check_service(service: str) -> None:
  if _SERVICE_NAME.fullmatch(service) is None:
    raise DeployError('the service must be named acknowledg-forge-<name>')


def check_image(image: str) -> None:
  if _IMAGE.fullmatch(image) is None:
    raise DeployError(
      f"the image must be {IMAGE_REPOSITORY}, pinned by digest"
    )


def deploy_env_content(image: str, api: Endpoint, ui: Endpoint) -> str:
  '''The Compose variables pointing the deployment at ``image`` and the
  AcknowlEDG endpoints.
  '''
  return (
    '# Managed by the acknowledg-forge-deploy workflow; changes are '
    'overwritten.\n'
    f"IMAGE={image}\n"
    f"EDG_ACKNOWLEDG_API_HOST={api.host}\n"
    f"EDG_ACKNOWLEDG_API_PORT={api.port}\n"
    f"EDG_ACKNOWLEDG_UI_HOST={ui.host}\n"
    f"EDG_ACKNOWLEDG_UI_PORT={ui.port}\n"
  )


def write_if_changed(path: Path, content: str) -> bool:
  '''Atomically replace ``path`` with ``content`` (mode 0644, root-owned,
  so the service's account can't change what it runs), unless it already
  holds exactly that; return whether it changed.
  '''
  data = content.encode()
  try:
    if path.read_bytes() == data:
      return False
  except FileNotFoundError:
    pass
  path.parent.mkdir(mode = 0o755, exist_ok = True)
  path.parent.chmod(0o755)
  descriptor, temporary = tempfile.mkstemp(
    dir = path.parent,
    prefix = f".{path.name}."
  )
  try:
    with os.fdopen(descriptor, 'wb') as handle:
      handle.write(data)
    os.chmod(temporary, 0o644)
    os.replace(temporary, path)
  except BaseException:
    os.unlink(temporary)
    raise
  return True


def remove_old_images(current: str) -> None:
  '''Remove this repository's other images, except those still in use
  (another deployment's, say), which Docker refuses to remove. Only these:
  the host's other images aren't the deployment's to prune.
  '''
  listed = subprocess.run(
    [
      'docker', 'image', 'ls', '--no-trunc', '--format', '{{.ID}}',
      IMAGE_REPOSITORY,
    ],
    capture_output = True,
    text = True,
    check = True
  ).stdout.split()
  current_id = subprocess.run(
    ['docker', 'image', 'inspect', '--format', '{{.Id}}', current],
    capture_output = True,
    text = True,
    check = True
  ).stdout.strip()
  for image_id in sorted(set(listed) - {current_id}):
    subprocess.run(
      ['docker', 'image', 'rm', image_id],
      stdout = subprocess.DEVNULL,
      stderr = subprocess.DEVNULL
    )


def deploy(service: str, image: str, api_url: str, ui_url: str) -> None:
  if os.geteuid() != 0:
    raise DeployError('must run as root (via sudo)')
  check_service(service)
  check_image(image)
  api = parse_url(api_url, plain = 'ws', tls = 'wss', label = 'API URL')
  ui = parse_url(ui_url, plain = 'http', tls = 'https', label = 'UI URL')

  unit = UNITS_DIR / f"{service}.service"
  for required in (unit, CONFIG_DIR / 'compose.yaml'):
    if not required.is_file():
      raise DeployError(
        f"{required} is missing; set the service up first (see README.md)"
      )
  if not (CONFIG_DIR / f"{service}.env").is_file():
    raise DeployError(
      f"{CONFIG_DIR / f'{service}.env'} is missing; it holds the "
      f"deployment's variables (FORGE, STATE_DIR, ...), which don't belong "
      f"in {service}.deploy.env (this helper rewrites that); see README.md"
    )

  deploy_env = CONFIG_DIR / f"{service}.deploy.env"
  changed = write_if_changed(deploy_env, deploy_env_content(image, api, ui))

  service_unit = f"{service}.service"
  is_active = ['systemctl', 'is-active', '--quiet', service_unit]

  digest = image.partition('@')[2]
  print(f"Pulling {digest}.", flush = True)
  subprocess.run(
    ['docker', 'pull', '--quiet', image],
    stdout = subprocess.DEVNULL,
    check = True
  )

  if changed:
    print(f"Restarting {service}.", flush = True)
    subprocess.run(['systemctl', 'restart', service_unit], check = True)
  elif subprocess.run(is_active).returncode != 0:
    print(f"Starting {service}.", flush = True)
    subprocess.run(['systemctl', 'start', service_unit], check = True)
  else:
    print(f"Unchanged; {service} keeps running.", flush = True)

  time.sleep(STARTUP_GRACE_S)
  if subprocess.run(is_active).returncode != 0:
    # Show why, if the journal can say.
    subprocess.run(
      ['journalctl', '--unit', service_unit, '--lines', '30', '--no-pager'],
      stdout = sys.stderr
    )
    raise DeployError(f"{service} is not active")

  remove_old_images(image)
  print(f"Deployed {service} ({digest}).")


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
    description = (
      'Point an acknowledg-forge deployment at a new image and restart it '
      '(only if anything changed).'
    )
  )
  parser.add_argument(
    'service',
    help = 'the service (its unit), named acknowledg-forge-<name>'
  )
  parser.add_argument(
    'image',
    help = f"the image, {IMAGE_REPOSITORY}@sha256:<digest>"
  )
  parser.add_argument(
    'api_url',
    metavar = 'acknowledg-api-url',
    help = 'the public AcknowlEDG API, ws://<host>[:<port>] or wss://<host>'
  )
  parser.add_argument(
    'ui_url',
    metavar = 'acknowledg-ui-url',
    help = 'the public AcknowlEDG UI, http://<host>[:<port>] or '
           'https://<host>'
  )
  return parser.parse_args()


def main() -> int:
  args = parse_args()
  try:
    deploy(args.service, args.image, args.api_url, args.ui_url)
  except DeployError as ex:
    print(f"error: {ex}", file = sys.stderr)
    return 1
  except subprocess.CalledProcessError as ex:
    print(f"error: {' '.join(map(str, ex.cmd))} failed ({ex.returncode})",
          file = sys.stderr)
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())
