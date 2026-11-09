#!/bin/bash -e

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

arch=$(uname -m)

packages=()

# Add required packages for checkout to function
packages+=("nodejs")

# Add valgrind (cachegrind) for the CI benchmark job.
packages+=("valgrind")

# Add zstd for actions/cache, which compresses its archives with it (and
# otherwise falls back to gzip, which this image also lacks).
packages+=("zstd")

# Install the core packages (dnf fetches the package database on install,
# so forcing an update here, particularly for development is counter
# productive to image build time -- nearly doubling it on a MacBook Pro
# circa 2018).
dnf install -y "${packages[@]}"
