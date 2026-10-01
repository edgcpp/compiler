#!/bin/bash

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

packages=()

# Add make build
packages+=("make")

# Add Python setup dependencies
packages+=("python3-pip")

# Add sphinx latex dependencies for PDF generation
packages+=(
  "latexmk"
  "texlive-collection-latexrecommended"
  "texlive-collection-latexextra"
)

# Add required packages for checkout to function (in CI, see
# .github/workflows/documentation.yml)
packages+=("git" "nodejs")

# Add useful linux commands
packages+=("util-linux") # kill, runuser

# Install the core packages (dnf fetches the package database on install,
# so forcing an update here, particularly for development is counter
# productive to image build time -- nearly doubling it on a MacBook Pro
# circa 2018).
dnf install -y "${packages[@]}"
