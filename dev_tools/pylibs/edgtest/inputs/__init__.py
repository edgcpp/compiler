# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''The files of a test home that a run of one configuration can read.

A run with a given configuration reads every test source, but only some of the
per-configuration files:

  - recordings (in a test's .<bare>.rto/ directory): the configuration's own
    recordings, falling back to the default configuration's "default."
    recordings (see edgtest.runtest.get_recording_possible_names);
  - expectations/<config>/ and flakey-tests/<config>/: the configuration's own
    directory and the default configuration's (see edgtest.expectations).

hash_test_inputs hashes just those files, so that, e.g., re-recording a test
for one configuration doesn't change the hash for the others.  It reads git
trees only (never file contents), so it also works in a blob-less clone.

Keep this in sync with how edgy selects these files.
'''

import hashlib
import shutil
import subprocess

from pathlib import Path, PurePosixPath
from typing import Set

from edgtest import runtest

TOOL_PATH_GIT = shutil.which('git')

_PER_CONFIG_DIRS = ('expectations', 'flakey-tests')

def is_input_for_config(path: PurePosixPath,
                        config: str, default_config: str) -> bool:
  '''Return whether a run of config can read the file at path (relative to the
  test home), where default_config is the default configuration.'''
  parent = path.parent.name
  if parent.startswith('.') and parent.endswith('.rto'):
    # Only the default configuration's recordings are named "default."; its
    # own name is never consulted.
    prefixes = ['default.']
    if config != default_config:
      prefixes.append(f"{config}.")
    return path.name.startswith(tuple(prefixes))
  if len(path.parts) >= 3 and path.parts[0] in _PER_CONFIG_DIRS:
    return path.parts[1] in (config, default_config)
  return True

def hash_test_inputs(test_home: Path, config: str, *,
                     rev: str = 'HEAD') -> str:
  '''Return a SHA-256 hex digest of the files in test_home (as of rev) that a
  run of config can read.

  The digest covers each such file's mode, blob hash, and path, plus the name
  of every per-configuration directory (edgy requires each configured one to
  exist, so removing another configuration's directory changes the digest).'''
  assert TOOL_PATH_GIT is not None, 'git is required'
  default_config = runtest.get_default_config_name()
  listing = subprocess.run(
    [TOOL_PATH_GIT, 'ls-tree', '-r', '-z', rev, '--', '.'],
    cwd = test_home,
    stdout = subprocess.PIPE,
    check = True
  ).stdout

  digest = hashlib.sha256()
  per_config_dirs: Set[str] = set()
  for entry in listing.split(b'\0'):
    if not entry:
      continue
    # Each entry is "<mode> <type> <object>\t<path>", with the path relative
    # to test_home.
    info, _, raw_path = entry.partition(b'\t')
    path = PurePosixPath(raw_path.decode('utf-8', errors = 'surrogateescape'))
    if len(path.parts) >= 3 and path.parts[0] in _PER_CONFIG_DIRS:
      per_config_dirs.add(f"{path.parts[0]}/{path.parts[1]}")
    if is_input_for_config(path, config, default_config):
      digest.update(info + b'\t' + raw_path + b'\0')
  for name in sorted(per_config_dirs):
    digest.update(f"dir\t{name}\0".encode('utf-8', errors = 'surrogateescape'))
  return digest.hexdigest()
