#!/usr/bin/bash

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Prepare what an acknowledg-forge deployment's containers need before
# `docker compose up` (run as root by the service's unit, ExecStartPre; see
# README.md):
#
#   prepare-runtime.sh <runtime-dir> <state-dir> <account>
#
# - The credentials systemd decrypted ($CREDENTIALS_DIRECTORY, readable by
#   root only) are copied to <runtime-dir>/credentials for <account>, whom
#   the containers run as. <runtime-dir> is the unit's RuntimeDirectory=, in
#   /run: memory, removed when the unit stops.
# - <state-dir>'s reviews/ and build-staging/ exist and belong to <account>
#   (compose.yaml binds them separately, and won't create them).

set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 <runtime-dir> <state-dir> <account>" >&2
  exit 2
fi
runtime_dir=$1
state_dir=$2
account=$3
group=$(id -g "${account}")

umask 077

install -d -m 0700 -o "${account}" -g "${group}" "${runtime_dir}/credentials"
if [[ -n "${CREDENTIALS_DIRECTORY:-}" ]]; then
  for credential in "${CREDENTIALS_DIRECTORY}"/*; do
    [[ -f "${credential}" ]] || continue
    install -m 0400 -o "${account}" -g "${group}" "${credential}" \
      "${runtime_dir}/credentials/"
  done
fi

# Read-only to the service's container too.
chmod 0500 "${runtime_dir}/credentials"

install -d -m 0700 -o "${account}" -g "${group}" \
  "${state_dir}/reviews" "${state_dir}/build-staging"
