#!/usr/bin/bash

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Install an uploaded AcknowlEDG server wheel on the deployment host and
# restart its systemd unit, but only when the wheel's contents differ from
# what is already deployed. Run on the host by the acknowledg-deploy workflow
# (piped over SSH with `bash -s`):
#
#   deploy-remote.sh <server-path> <service> <wheel-name> <fingerprint>
#
# <server-path> must contain a `venv/` with the server installed into it and
# an `incoming/` directory holding <wheel-name>. <fingerprint> identifies the
# wheel's contents (see the workflow). The fingerprint is recorded only after
# a successful restart, so a failed deploy is retried by the next run.

set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 <server-path> <service> <wheel-name> <fingerprint>" >&2
  exit 2
fi

server_path=$1
service=$2
wheel_name=$3
fingerprint=$4

wheel="${server_path}/incoming/${wheel_name}"
fingerprint_file="${server_path}/deployed.fingerprint"
pip="${server_path}/venv/bin/pip"
wheels_to_keep=5

if [[ ! -f "${wheel}" ]]; then
  echo "error: uploaded wheel not found" >&2
  exit 1
fi
if [[ ! -x "${pip}" ]]; then
  echo "error: no venv found in the server path" >&2
  exit 1
fi

deployed=""
if [[ -f "${fingerprint_file}" ]]; then
  deployed=$(<"${fingerprint_file}")
fi

if [[ "${deployed}" == "${fingerprint}" ]]; then
  echo "Server unchanged (${fingerprint:0:12}); skipping install and restart."
  rm -f -- "${wheel}"
  exit 0
fi

echo "Installing server ${fingerprint:0:12} (was ${deployed:0:12})."
# The package version is static, so force our own package to be replaced,
# then let pip pick up any new or changed dependencies.
"${pip}" install --quiet --force-reinstall --no-deps "${wheel}"
"${pip}" install --quiet "${wheel}"

echo "Restarting ${service}."
sudo -n systemctl restart "${service}"
# A simple-type unit reports active immediately, so give the server a moment
# to fail on startup (bad import, port in use) before checking.
sleep 5
if ! systemctl is-active --quiet "${service}"; then
  echo "error: ${service} is not active after restart" >&2
  systemctl status --no-pager --lines=20 "${service}" >&2 || true
  exit 1
fi

# Keep recent wheels for manual rollback. Wheel file names don't change
# between builds (static version), so each gets its own directory.
archive="${server_path}/wheels/$(date -u +'%Y.%m.%d-%H.%M.%S')-${fingerprint:0:12}"
mkdir -p -- "${archive}"
mv -- "${wheel}" "${archive}/"
find "${server_path}/wheels" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' \
  | sort -r | tail -n +$((wheels_to_keep + 1)) \
  | while read -r old; do rm -rf -- "${server_path}/wheels/${old}"; done

printf '%s\n' "${fingerprint}" >"${fingerprint_file}"
echo "Server deployed and ${service} restarted."
