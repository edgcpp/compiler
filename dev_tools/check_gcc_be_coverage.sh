#!/usr/bin/env bash
# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_ROOT}"

BUILD_DIR="${EDG_GCC_BE_BUILD_DIR:-${PROJECT_ROOT}/build/gcc-agent}"
PROFRAW_DIR="${PROJECT_ROOT}/.tmp/agent-gcc/profraw"

mkdir -p "${PROFRAW_DIR}"
rm -f "${PROFRAW_DIR}"/*.profraw

if [[ ! -f "${BUILD_DIR}/build.ninja" && ! -f "${BUILD_DIR}/Makefile" ]]; then
  UNAME_S="$(uname -s)"
  UNAME_M="$(uname -m)"
  if [[ "${UNAME_S}" == "Darwin" ]]; then
    if [[ "${UNAME_M}" == "arm64" ]]; then
      MACRO_CONF="macos-arm-clang"
      RT_LIBS="macos_arm64"
    else
      MACRO_CONF="macos-x86-clang"
      RT_LIBS="macos_x86_64"
    fi
  else
    MACRO_CONF="linux-clang"
    RT_LIBS="linux_x86_64"
  fi
  CMAKE_ARGS=(
    -S "${PROJECT_ROOT}"
    -B "${BUILD_DIR}"
    -G Ninja
    -DCMAKE_BUILD_TYPE=Debug
    -DEDG_MACRO_CONF="${MACRO_CONF}"
    -DEDG_CPP_RT_LIBS="${RT_LIBS}"
    -DEDG_ENABLE_GCC_BACKEND=ON
    -DEDG_GCC_BE_ENABLE_COVERAGE=ON
  )
  cmake "${CMAKE_ARGS[@]}"
fi

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc || echo 4)"
cmake --build "${BUILD_DIR}" -j "${JOBS}"

find "${BUILD_DIR}" \( -name '*.gcda' -o -name '*.profraw' \) -delete 2>/dev/null || true

CTEST_ARGS=(
  --test-dir "${BUILD_DIR}/tests/gcc_be"
  -E "test_gcc_be_coverage|test_gcc_be_doc_coverage"
  --output-on-failure
)
LLVM_PROFILE_FILE="${PROFRAW_DIR}/gcc_be-%p-%m.profraw" ctest "${CTEST_ARGS[@]}"

echo "Checking GCC Backend 100% instrumented test coverage..."
python3 "${PROJECT_ROOT}/tests/gcc_be/check_coverage.py" --build-dir "${BUILD_DIR}"
