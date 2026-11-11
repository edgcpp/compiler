#!/usr/bin/bash

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Run acknowledg-forge-gitlab with ACKNOWLEDG_FORGE_SETTINGS_FILE naming
# its TOML configuration and CREDENTIALS_DIRECTORY naming its credentials.

exec uv run acknowledg-forge-gitlab "$@"
