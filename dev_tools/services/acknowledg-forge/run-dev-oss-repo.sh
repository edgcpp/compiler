#!/usr/bin/bash

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Run a development acknowledg-forge-github against edgcpp/compiler, serving
# reviews to the local AcknowlEDG server (../acknowledg/server/
# run-server.sh) and client. It runs unsandboxed and never comments on pull
# requests, so ACKNOWLEDG_FORGE_TOKEN only needs to be a fine-grained personal
# access token with read-only access to public repositories (see README.md).
# Without it, the GitHub CLI's token (gh auth token) is used.
#
# State (clones, reviews, the database) is kept in a new temporary directory,
# removed when the service exits, unless ACKNOWLEDG_FORGE_HOME names one to
# keep.

set -euo pipefail

cd "$(dirname "$0")/github"
dev_tools="$(realpath ../../..)"

if [[ -z "${ACKNOWLEDG_FORGE_TOKEN:-}" ]]; then
  if ! command -v gh > /dev/null; then
    echo 'error: set ACKNOWLEDG_FORGE_TOKEN to a GitHub personal access' \
         'token (see README.md)' >&2
    exit 1
  fi
  if ! ACKNOWLEDG_FORGE_TOKEN="$(gh auth token 2>/dev/null)"; then
    echo 'error: set ACKNOWLEDG_FORGE_TOKEN to a GitHub personal access' \
         'token, or log in with gh auth login (see README.md)' >&2
    exit 1
  fi
  echo 'Using the GitHub CLI token (gh auth token).' >&2
fi
export ACKNOWLEDG_FORGE_TOKEN

if [[ -z "${ACKNOWLEDG_FORGE_HOME:-}" ]]; then
  ACKNOWLEDG_FORGE_HOME="$(mktemp -d -t acknowledg-forge-dev.XXXXXXXXXX)"
  trap 'rm -rf "${ACKNOWLEDG_FORGE_HOME}"' EXIT
fi
export ACKNOWLEDG_FORGE_HOME
echo "acknowledg-forge home: ${ACKNOWLEDG_FORGE_HOME}" >&2
export ACKNOWLEDG_FORGE_TOOLS_DIR="${dev_tools}"
export ACKNOWLEDG_FORGE_REPOSITORIES='edgcpp/compiler'
export ACKNOWLEDG_FORGE_CI_WORKFLOW='build-and-test-ci.yml'
export ACKNOWLEDG_FORGE_POLL_INTERVAL="${ACKNOWLEDG_FORGE_POLL_INTERVAL:-60}"
export ACKNOWLEDG_FORGE_UNSANDBOXED=1
export ACKNOWLEDG_FORGE_COMMENTS=0
unset ACKNOWLEDG_FORGE_API_URL ACKNOWLEDG_FORGE_WEBHOOK_BIND
unset ACKNOWLEDG_FORGE_DATABASE_URL

export EDG_ACKNOWLEDG_API_HOST='localhost'
export EDG_ACKNOWLEDG_API_PORT=2727
export EDG_ACKNOWLEDG_UI_HOST='localhost'
export EDG_ACKNOWLEDG_UI_PORT=5173

# Not exec, so the temporary home is removed afterwards.
uv run acknowledg-forge-github "$@"
