# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import getpass
import os
import platform
import re
import shutil
import socket
import subprocess
import sys

import edgtools
import edgutil

from functools import lru_cache
from pathlib import Path, PurePath, PurePosixPath
from typing import Dict, List, Optional

_DOCKER_DIR = edgtools.get_tools_dir() / 'docker'
_DEFAULT_SKEL = _DOCKER_DIR / 'skel'

DEFAULT_BUILD_DIR = 'build/gcc'
DEFAULT_BUILD_PRESET = 'linux-gcc-debug'
DEFAULT_EDG_BASE = 'bases/docker/dev-env/gcc'

DEFAULT_IMAGE = 'dev-env'
DEFAULT_IMAGE_PINNED_SHA = (
  '1c1cf78d322b7e310b4d35a114f279221b7725b6f3e28939a5212502f07f7f91'
)

IMAGE_PINS = {
  DEFAULT_IMAGE: DEFAULT_IMAGE_PINNED_SHA
}

# This should be a complete list of all executable EDG docker environments.
#
# i.e., This should be a list of every subdirectory in $EDG_TOOLS/docker that
# has a Dockerfile that's expected to be executed.
EXECUTABLE_IMAGES = ('dev-env', 'legacy-dev-env', 'sphinx-env')

# This should be a complete list of all buildable EDG docker images.
#
# i.e., This should be a list of every subdirectory in $EDG_TOOLS/docker that
# has a Dockerfile.
BUILDABLE_IMAGES = (
  *EXECUTABLE_IMAGES,
  'build-base',
  'native-tools',
  'legacy-native-tools',
  'softfloat',
  'ci-runner',
  'acknowledg-forge'
)

# The registry namespace EDG images are published under (the GitHub
# organization's container registry).
IMAGE_NAMESPACE = 'ghcr.io/edgcpp'

def get_image_repository(image_name: str) -> str:
  '''Return the registry repository an internal image is published to.'''
  return f"{IMAGE_NAMESPACE}/{image_name}"

def get_docker_file_dir(image_name: str) -> Path:
  '''Return the expected directory of the Dockerfile for an image name.'''
  return _DOCKER_DIR / image_name

def get_docker_file(image_name: str) -> Path:
  '''Return the expected path of the Dockerfile for an image name.'''
  return get_docker_file_dir(image_name) / 'Dockerfile'

def get_base_image(docker_file_path: Path) -> Optional[str]:
  '''Scan for the base image of the given Dockerfile.

  These are grammatically 'FROM' <label> '\n'. If an image has multiple, only
  the first will be returned. This may need reconsidered if EDG moves towards
  more advanced docker use cases.
  '''
  with open(docker_file_path, 'r') as docker_file:
    match = re.search(r'FROM (.*:.*)', docker_file.read())
    if match:
      return match.group(1)
  return None

def use_image_pinning() -> bool:
  return not edgutil.is_env_set('EDG_DOCKER_LOCAL_IMAGES')

def get_latest_image_tag(image_name: str, *,
                         respect_pins: bool = use_image_pinning()) -> str:
  '''Return the tag used for the given internal image name.

  If respect_pins is TRUE images with pinned SHA tags will be returned with
  their pinned name If you want the generic latest tag, use respect_pins =
  False.  The default value is controlled by use_image_pinning(), for uses
  that expect a particular format, the caller should explicitly specify.'''
  repository = get_image_repository(image_name)
  if respect_pins and image_name in IMAGE_PINS:
    # This is an officially maintained image synchronized between developers.
    return f"{repository}@sha256:{IMAGE_PINS[image_name]}"
  else:
    return f"{repository}:latest"

def has_image_tagged(image_name: str) -> bool:
  '''Return TRUE if there is a docker image already build on this system.'''
  process = subprocess.run(
    [
      'docker',
      'image',
      'inspect',
      get_latest_image_tag(image_name)
    ],
    stdout = subprocess.DEVNULL,
    stderr = subprocess.DEVNULL
  )
  return process.returncode == 0

def ensure_edg_docker_image_present(image_name: str) -> None:
  if has_image_tagged(image_name):
    return

  if use_image_pinning() and image_name in IMAGE_PINS:
    # This is an officially maintained image synchronized between developers.
    process = subprocess.run(
      [
        'docker',
        'pull',
        get_latest_image_tag(image_name, respect_pins = True)
      ]
    )
    if process.returncode != 0:
      sys.exit(1)
  else:
    process = subprocess.run(
      [
        'build-edg-env',
        image_name
      ]
    )
    if process.returncode != 0:
      sys.exit(1)

# FIXME: Replace with @cache when moving to Python 3.9
@lru_cache(maxsize = None)
def is_running_in_docker() -> bool:
  '''Return TRUE if this script is running inside of a docker container.'''
  return os.path.exists('/.dockerenv')

def terminate_if_running_in_docker() -> None:
  '''Terminate the program if it's running in a docker container.'''
  if not is_running_in_docker():
    return

  print('This command cannot be run from within a docker container.')
  sys.exit(1)

# FIXME: Replace with @cache when moving to Python 3.9
@lru_cache(maxsize = None)
def _enumerate_host_path_mapping() -> Dict[Path, PurePath]:
  mapping: Dict[Path, PurePath] = {}
  idx = 0
  while f"EDG_DOCKER_PATH_{idx}" in os.environ:
    docker_path = Path(os.environ[f"EDG_DOCKER_PATH_{idx}"])
    host_path = PurePath(os.environ[f"EDG_DOCKER_PATH_{idx}_V"])
    mapping[docker_path] = host_path
    idx += 1
  return mapping

def should_display_docker_host_paths() -> bool:
  if not is_running_in_docker():
    return False
  return edgutil.is_env_set('EDG_DOCKER_HOST_PATHS')

@lru_cache(maxsize = 128)
def _render_path_internal(path: Path) -> PurePath:
  # This may seem expensive, but as the _enumerate_host_path_mapping() is
  # cached, this is actually relatively cheap.  That compounds with the caching
  # of this function.
  mapping = _enumerate_host_path_mapping()
  if path in mapping:
    return mapping[path]
  if path == path.parent:
    return PurePath(path)
  return _render_path_internal(path.parent) / path.name

def render_docker_path(path: Path) -> str:
  if not path.is_absolute() or not should_display_docker_host_paths():
    return str(path)
  if os.environ['EDG_DOCKER_HOST_PATHS'] == '2':
    return f"{path} ({_render_path_internal(path)})"
  return str(_render_path_internal(path))

def render_user_name() -> str:
  '''Return the host user name, even when running inside a container.

  Docker launches pass ``EDG_USER_NAME`` from the host; fall back to
  ``getpass.getuser()`` when that is unset.
  '''
  if 'EDG_USER_NAME' in os.environ:
    return os.environ['EDG_USER_NAME']
  return getpass.getuser()

def _add_env_value(docker_args: List[str], key: str, value: str) -> None:
  docker_args.extend([
    '--env',
    f"{key}={value}"
  ])

def _expose_env_value(docker_args: List[str], key: str) -> None:
  if key in os.environ:
    _add_env_value(docker_args, key, os.environ[key])

def _add_volume_mapping(docker_args: List[str], mapped_dirs: List[Path],
                        host_dir: Path, container_dir: str, *,
                        modifier = '') -> None:
  # Sanitize the directory, resolving symlinks.
  host_dir = host_dir.resolve()
  # Expose the real path inside of the container for display purposes.
  # This follows the pattern of:
  #
  #  EDG_DOCKER_PATH_0="<real host path>"
  #  EDG_DOCKER_PATH_0_V="<container path>"
  #  EDG_DOCKER_PATH_1="<real host path>"
  #  EDG_DOCKER_PATH_1_V="<container path>"
  #  ...
  #  EDG_DOCKER_PATH_N="<real host path>"
  #  EDG_DOCKER_PATH_N_V="<container path>"
  #
  mapping_idx = len(mapped_dirs)
  _add_env_value(
    docker_args,
    f"EDG_DOCKER_PATH_{mapping_idx}",
    container_dir
  )
  _add_env_value(
    docker_args,
    f"EDG_DOCKER_PATH_{mapping_idx}_V",
    str(host_dir)
  )
  # Add this to the list of mapped directories.
  mapped_dirs.append(host_dir)
  # Add the command arguments for mounting/mapping this directory.
  docker_args.extend([
    '--volume',
    f"{host_dir}:{container_dir}{modifier}"
  ])

def _check_parents(mapped_dirs: List[Path], workspace_dir: Path) -> None:
  '''This function is used to ensure the mapped directories don't overlap.
  This can be valid (from a Docker perspective), but it's an advanced use case
  and can be a dangerous mistake (so we don't permit it).
  '''
  # Resolve the workspace so that a relative path cannot bypass the drive
  # comparison below (and "normcase" so that Windows drive letters that
  # differ only in case compare equal).
  workspace_dir = workspace_dir.resolve()
  workspace_drive = os.path.normcase(os.path.splitdrive(str(workspace_dir))[0])
  for mapped_dir in mapped_dirs:
    mapped_drive = os.path.normcase(os.path.splitdrive(str(mapped_dir))[0])
    if mapped_drive != workspace_drive:
      # The two paths are on different drives (which can only happen on
      # Windows), so neither path can contain the other and the check below
      # does not apply.
      continue
    common_dir = Path(os.path.commonpath([mapped_dir, workspace_dir]))
    if common_dir == workspace_dir:
      print('Current workspace is the parent of a mapped dir:')
      print(f"  {mapped_dir}")
      sys.exit(1)
    if common_dir == mapped_dir:
      print('Current workspace is a subdirectory of a mapped dir:')
      print(f"  {mapped_dir}")
      sys.exit(1)

def _process_volume_information(docker_args: List[str],
                                workspace_dir: Path) -> None:
  '''This function is used to add (and sanity check) the arguments to handle
  volume mapping on the host.
  '''
  mapped_dirs: List[Path] = []

  config_dir = (
    edgutil.get_application_files_dir('edgdocker') / 'profiles' / 'default'
  )
  if not config_dir.exists():
    shutil.copytree(_DEFAULT_SKEL, config_dir)

  _add_volume_mapping(docker_args, mapped_dirs, config_dir, '/home/dev')

  # Check that the above volumes are not in conflict with the requested
  # workspace directory.
  _check_parents(mapped_dirs, workspace_dir)

  # Add the volume mapping for the workspace directory (now that we've
  # checked to make sure this isn't problematic).
  _add_volume_mapping(docker_args, mapped_dirs, workspace_dir,
                      '/edg/workspace')

def _check_x11_support() -> bool:
  if 'DISPLAY' not in os.environ:
    print('The DISPLAY environment variable not set, X11 integration failed.')
    return False
  return True

def _find_xauth_cookie(display: str) -> str:
  '''This function searches for and returns the XAUTH cookie.

  The cookie is found via the command:

  $> xauth list $DISPLAY
  lin6.edg.com/unix:1  MIT-MAGIC-COOKIE-1  7e0136a508b8eac4def04b62407e7dcf
  #ffff##:1  MIT-MAGIC-COOKIE-1  7e0136a508b8eac4def04b62407e7dcf

  The cookie in this case (given a system with a hostname of lin6.edg.com) is
  "7e0136a508b8eac4def04b62407e7dcf".
  '''
  # Execute the command do some initial output processing.
  xauth_list_cmd = ['xauth', 'list']
  # This only works reliably on Linux
  if platform.system() == 'Linux':
    xauth_list_cmd.append(display)
  xauth_output = subprocess.check_output(xauth_list_cmd).decode('utf-8')
  display_keys = xauth_output.split('\n')

  # Figure out what the system hostname is to find the best match.
  system_hostname = socket.gethostname()
  for display_key in display_keys:
    # Check for a matching hostname
    if display_key.startswith(system_hostname):
      # Pull apart the 3 pieces based on the double spaces.
      return display_key.split('  ')[2]

  print('X11 support requested, but no viable X11 auth cookie could be found.')
  if platform.system() == 'Darwin':
    print('Please ensure:')
    print('- XQuartz is running.')
    print('- XQuartz is configured to authenticate connections.')
  sys.exit(1)

def _expose_x11(docker_args: List[str]) -> None:
  display = os.environ['DISPLAY']
  # Workaround XQuartz's unusual implementation (for MacOS support)
  if platform.system() == 'Darwin':
    display = f"host.docker.internal:0"

  # Expose the display variable.
  _add_env_value(docker_args, 'DISPLAY', display)

  # Resolve the desired cookie, then construct an XAUTH token that matches the
  # display.
  cookie = _find_xauth_cookie(display)
  _add_env_value(
    docker_args, 'XAUTH_TOKEN',
    f"{display}  MIT-MAGIC-COOKIE-1  {cookie}"
  )

  # Docker must use the system networking, make that happen now.
  #
  # The alternative is set X11UseLocalhost to no. However, (per my
  # understanding) this is dangerous as it exposes the XServer of connecting
  # clients to other connecting clients. e.g., if Bob and Jim connect to the
  # Server, Bob could display programs on Jim's screen.
  docker_args.extend(['--net', 'host'])

def _get_user_id() -> int:
  '''This function is used to retrieve the user ID for mapping the container to
  the local user. On Linux this is important for correctly mapping the file
  system information. On Windows and MacOS this is not a concern, and always
  returns a sane default value of "1000" for the container user ID.
  '''
  if platform.system() != 'Linux':
    return 1000
  return os.getuid()

def _get_group_id() -> int:
  '''This function is used to retrieve the group ID for mapping the container
  to the local user's group. On Linux this is important for correctly mapping
  the file system information. On Windows and MacOS this is not a concern, and
  always returns a sane default value of "1000" for the container group ID.
  '''
  if platform.system() != 'Linux':
    return 1000
  return os.getgid()

def exec_docker_cmd(image_name: str, workspace_dir: Path,
                    container_working_dir: PurePosixPath, command_string: str,
                    *, enable_x11: bool = False, memory_limit: str = '',
                    platform_specifier: Optional[str] = None,
                    enable_core_dumps: bool = False,
                    dry_run: bool = False, verbose: bool = False) -> None:
  docker_args = [
    'docker',
    'run',
    # Automatically destroy the container when its task is done.
    '--rm',
    # Don't persist the container's output; it is already streamed to the
    # attached terminal.
    '--log-driver',
    'none',
    # The container needs privileges for various things, e.g. setarch
    '--privileged',
    # Set a sane hostname.
    '--hostname',
    'edg',
    # Setup an init process that reaps orphaned children to prevent zombie
    # process build up.
    '--init',
    # Pass along information about what user the container should run as.
    '--env',
    f"EDG_USER_ID={_get_user_id()}",
    '--env',
    f"EDG_USER_GROUP_ID={_get_group_id()}",
    '--env',
    f"EDG_USER_NAME={render_user_name()}"
  ]

  if edgutil.is_interactive_terminal():
    # Make the container interactive.
    docker_args.append('-it')
  else:
    # Make the container available to stdin (useful for stdin/stdout based
    # IPC).
    docker_args.append('-i')

  # Set the working directory inside of the container.
  docker_args.extend([
    '--workdir',
    str(container_working_dir)
  ])

  if platform_specifier is not None:
    docker_args.extend(['--platform', platform_specifier])

  # Setup the volume (i.e., directory) mappings.
  _process_volume_information(docker_args, workspace_dir)

  # Handle X11 display socket forwarding if the requirements are met.
  if enable_x11 and _check_x11_support():
    _expose_x11(docker_args)

  if len(memory_limit) > 0:
    docker_args.append('--memory')
    docker_args.append(memory_limit)

  if not enable_core_dumps:
    docker_args.append('--ulimit')
    docker_args.append('core=0')

  # If relevant, attempt to forward an adapted version of EDG_BASE that rewords
  # the path for the container; otherwise, fallback to the default EDG_BASE for
  # docker containers.
  if 'EDG_BASE' in os.environ:
    container_workspace_dir = PurePosixPath('/', 'edg', 'workspace')
    host_base_path = Path(os.environ['EDG_BASE'])
    if edgutil.is_path_relative_to(host_base_path, workspace_dir):
      rel_base_path = host_base_path.relative_to(workspace_dir)
      base_path = container_workspace_dir.joinpath(rel_base_path)
      _add_env_value(docker_args, 'EDG_BASE', str(base_path))
    elif not host_base_path.is_absolute():
      base_path = container_workspace_dir.joinpath(host_base_path)
      _add_env_value(docker_args, 'EDG_BASE', str(base_path))
  else:
    _add_env_value(
      docker_args,
      'EDG_BASE',
      f"/edg/workspace/{DEFAULT_EDG_BASE}"
    )

  # Directly map the following environment variables.
  _expose_env_value(docker_args, 'EDG_DIFF_DISCOVERY_MODE')
  _expose_env_value(docker_args, 'EDGY_MY_TESTS')
  _expose_env_value(docker_args, 'EDG_TEST_NO_COPY_CPFE')
  _expose_env_value(docker_args, 'EDG_TEST_RUNS_KEPT')
  _expose_env_value(docker_args, 'EDG_BENCH_RUNS_KEPT')
  _expose_env_value(docker_args, 'EDG_DOCKER_HOST_PATHS')
  _expose_env_value(docker_args, 'EDG_ACKNOWLEDG_UI_HOST')
  _expose_env_value(docker_args, 'EDG_ACKNOWLEDG_API_HOST')
  _expose_env_value(docker_args, 'EDG_ACKNOWLEDG_UI_PORT')
  _expose_env_value(docker_args, 'EDG_ACKNOWLEDG_API_PORT')
  _expose_env_value(docker_args, 'EDG_ACKNOWLEDG_API_KEY')

  # Put the final touches on the command, and forward the command string to a
  # bash prompt.
  docker_args.extend([
    get_latest_image_tag(image_name),
    'edg-container-exec',
    command_string
  ])

  # Execute the docker command, replacing this process.
  if verbose or dry_run:
    print(' '.join(docker_args))

  # If this is a dry run, terminate early, don't run the command.
  if dry_run:
    return

  if platform.system() == 'Windows':
    # Windows does not properly handle the execvp. Instead, keep this process
    # around, and simply forward on stdin, stdout, and stderr.  Forward the
    # exit status of the container so that callers observe failures.
    completed_proc = subprocess.run(
      docker_args,
      stdin = sys.stdin,
      stdout = sys.stdout,
      stderr = sys.stderr
    )
    sys.exit(completed_proc.returncode)
  else:
    # Execute the docker command, replacing the current process.
    os.execvp('docker', docker_args)

