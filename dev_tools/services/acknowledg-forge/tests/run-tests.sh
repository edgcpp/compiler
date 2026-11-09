#!/usr/bin/bash

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Run the acknowledg-forge tests (shared core and every forge service).

set -euo pipefail
cd "$(dirname "$0")"
exec uv run --project ../github --group dev --extra postgresql \
  --with-editable ../gitlab \
  pytest "$@"
