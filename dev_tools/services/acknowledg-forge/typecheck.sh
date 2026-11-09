#!/usr/bin/bash

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Typecheck acknowledg-forge (the shared edgacknowledg.forge package and each
# forge service) with the same strictness typecheck-python.sh uses, in the
# services' own environment (it needs their third-party packages, which the
# rest of dev_tools doesn't).

set -euo pipefail
cd "$(dirname "$0")"

exec uv run --project github --with mypy --with-editable gitlab \
  mypy --no-namespace-packages  \
       --scripts-are-modules    \
       --strict-equality        \
       --disallow-untyped-defs  \
       --disallow-untyped-calls \
       --follow-imports=silent  \
       "$(realpath ../../pylibs/edgacknowledg/forge)" \
       "$(realpath github/acknowledg_forge_github)" \
       "$(realpath gitlab/acknowledg_forge_gitlab)" \
       "$(realpath deploy-remote.py)"
