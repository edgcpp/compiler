# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# This script is executed by build-edg-env before creation of the Docker image
# to copy in the documentation's requirements.txt file.

import os
import shutil

import edgtools

from pathlib import Path

def copy_requirements_file() -> None:
  '''Copy doc/requirements.txt for use by the Dockerfile.'''
  doc_dir = edgtools.get_tools_dir().parent / 'doc'
  shutil.copy(doc_dir / 'requirements.txt', Path(os.getcwd()))

def main() -> None:
  copy_requirements_file()

main()
