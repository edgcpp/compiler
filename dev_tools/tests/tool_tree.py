# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Helper for tests that run edg-check-file-changes: gives it a private copy
of its packages beside an allowlist of the test's choosing, so the results
don't depend on the production spelling allowlist.'''

import json
import shutil

from pathlib import Path
from typing import Any

PYLIBS_DIR = Path(__file__).resolve().parents[1] / 'pylibs'

# The pylibs packages the tool imports.
TOOL_PYLIBS = ('edgchanges', 'edgspelling', 'edgtools', 'edgutil')

def make_tool_tree(tmp_dir: Path, allowlist: Any) -> Path:
  '''Copy the packages the tool needs into tmp_dir/dev_tools/pylibs, beside
  an allowlist file holding allowlist (JSON text, or a value to dump).  The
  tool finds its allowlist relative to the packages, so this gives it one of
  our own.  Packages already copied are kept; the allowlist is rewritten.
  Return the pylibs directory.'''
  pylibs_dir = tmp_dir / 'dev_tools' / 'pylibs'
  for package in TOOL_PYLIBS:
    if not (pylibs_dir / package).exists():
      shutil.copytree(
        PYLIBS_DIR / package,
        pylibs_dir / package,
        ignore = shutil.ignore_patterns('__pycache__')
      )
  (tmp_dir / 'dev_tools' / 'spelling_allowlist.json').write_text(
    allowlist if isinstance(allowlist, str) else json.dumps(allowlist)
  )
  return pylibs_dir
