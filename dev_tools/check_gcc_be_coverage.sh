#!/usr/bin/env bash
set -euo pipefail

# Script to verify 100% test coverage for gcc_gen_be components
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_ROOT}"

echo "Checking GCC Backend test coverage..."
if ! python3 tests/gcc_be/check_coverage.py; then
    echo "Coverage check failed."
    exit 1
fi
exit 0
