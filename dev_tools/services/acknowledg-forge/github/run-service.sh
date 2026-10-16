#!/usr/bin/bash

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Run acknowledg-forge-github for local development. Configuration comes
# from the environment (see ../README.md); at least ACKNOWLEDG_FORGE_HOME and
# ACKNOWLEDG_FORGE_REPOSITORIES must be set.

exec uv run acknowledg-forge-github "$@"
