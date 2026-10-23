#!/bin/bash -e

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

packages=()

# The service itself and its git workspaces.
packages+=("python3")
packages+=("git-core")

# Used (in the builder container) by edg-rematerialize-test-run.
packages+=("patch")
packages+=("diffutils")

# cg_annotate, for benchmark sections (Fedora packages it separately).
packages+=("valgrind")
packages+=("valgrind-scripts")

dnf install -y --setopt=install_weak_deps=False "${packages[@]}"
dnf clean all
