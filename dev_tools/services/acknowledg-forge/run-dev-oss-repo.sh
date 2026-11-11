#!/usr/bin/bash

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Run the public-repository demo with TOML and anonymous-memory credentials.
# The Python launcher owns cleanup and signal forwarding. No token is put
# in the shell environment, arguments, or a disk-backed temporary file.

set -euo pipefail
cd "$(dirname "$0")/github"
exec uv run python -m edgacknowledg.forge.dev "$@"
