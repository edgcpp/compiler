# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# This script is executed by build-edg-env before creation of the Docker image
# to build the image's dependencies: the frozen tooling archive, and each
# acknowledg-forge service's wheel plus its locked requirements.

import os
import shutil
import subprocess
import sys
import tarfile

import edgtools
import edgutil

from pathlib import Path

FORGES = ('github', 'gitlab')


def create_scripts_and_libs_tar_file() -> None:
  '''Create scripts-and-libs.tar.gz (dev_tools/bin and dev_tools/pylibs).'''
  tools_dir = edgtools.get_tools_dir()
  tar_file_path = Path(os.getcwd()).joinpath('scripts-and-libs.tar.gz')

  with edgutil.CurrentWorkingDirectorySwap(tools_dir):
    with tarfile.open(tar_file_path, 'w:gz') as tar:
      tar.add('bin')
      tar.add('pylibs')


def build_services() -> None:
  '''Build each service's wheel and export its locked requirements (with
  hashes) into services/<forge>/.
  '''
  if shutil.which('uv') is None:
    edgutil.eprint('uv is required to build the acknowledg-forge image')
    sys.exit(1)
  services_dir = edgtools.get_tools_dir() / 'services' / 'acknowledg-forge'
  for forge in FORGES:
    output_dir = Path(os.getcwd(), 'services', forge)
    output_dir.mkdir(parents = True)
    service_dir = services_dir / forge
    # Remove the service's build/ directory first: uv builds the wheel with
    # the service's build backend, setuptools, which keeps that directory as
    # a cache between builds and copies sources into it without removing the
    # modules whose sources have since been deleted, so those stale modules
    # would be packaged into the wheel (and the image).
    shutil.rmtree(service_dir / 'build', ignore_errors = True)
    subprocess.run(
      ['uv', 'build', '--wheel', '--out-dir', str(output_dir)],
      cwd = service_dir,
      check = True
    )
    subprocess.run(
      [
        'uv', 'export', '--frozen', '--no-dev', '--no-emit-project',
        # PostgreSQL support (asyncpg), so the image works with either
        # database.
        '--extra', 'postgresql',
        '--format', 'requirements-txt',
        '--output-file', str(output_dir / 'requirements.txt'),
      ],
      cwd = service_dir,
      check = True
    )


def main() -> None:
  create_scripts_and_libs_tar_file()
  build_services()

main()
