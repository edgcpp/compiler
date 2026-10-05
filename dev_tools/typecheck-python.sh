#!/usr/bin/bash

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Usage: typecheck-python.sh [--legacy]
#
# --legacy  Skip the AcknowlEDG server (no 3.6 client-side floor) and intended
#           for typechecking against older CPython (e.g. 3.6 via CI).

standard_type_checks=()
strict_type_checks=()
legacy=0

for arg in "$@"; do
  case "${arg}" in
    --legacy)
      legacy=1
      ;;
    -h|--help)
      echo "Usage: $0 [--legacy]" >&2
      exit 0
      ;;
    *)
      echo "error: unknown argument: ${arg}" >&2
      echo "Usage: $0 [--legacy]" >&2
      exit 2
      ;;
  esac
done

function check_types {
  if [[ ! -e $1 ]]; then
    echo "warning: skipping missing path for typecheck: $1" >&2
    return
  fi
  standard_type_checks+=("$(realpath $1)")
}

function check_types_strict {
  if [[ ! -e $1 ]]; then
    echo "warning: skipping missing path for typecheck: $1" >&2
    return
  fi
  strict_type_checks+=("$(realpath $1)")
}

pushd bin/ > /dev/null
  check_types build-edg-env
  check_types clone-test-here
  check_types clone-test-to-tmp
  check_types_strict edg-acknowledg-cli
  check_types_strict edg-bench
  check_types_strict edg-bench-review
  check_types_strict edg-bench-run-delta
  check_types_strict edg-bench-run-save
  check_types_strict edg-check-policy
  check_types edg-cmakedef
  check_types_strict edg-docker-bench
  check_types_strict edg-docker-test
  check_types edg-examine-test
  check_types edg-exec
  check_types edg-modify-test
  check_types_strict edg-pack-cpfe-ce
  check_types_strict edg-run-test
  check_types edg-scrape-compiler
  check_types_strict edg-test-run-diff
  check_types_strict edg-test-run-sonar
  check_types edg-walk-tests
  check_types_strict edgy
  check_types_strict edgy-reduce-race
  check_types_strict edgy-replay-blame
  check_types_strict edgy-review
  check_types gen-ifc-map
  check_types ifc-qt
popd > /dev/null

pushd pylibs/ > /dev/null
  pushd edgacknowledg/ > /dev/null
    check_types_strict __init__.py
    pushd client/ > /dev/null
      check_types_strict __init__.py
      check_types_strict shared.py
      check_types_strict websocket.py
      pushd bench/ > /dev/null
        check_types_strict __init__.py
      popd > /dev/null
      pushd test/ > /dev/null
        check_types_strict __init__.py
      popd > /dev/null
    popd > /dev/null
  popd > /dev/null
  pushd edgbench/ > /dev/null
    check_types_strict __init__.py
  popd > /dev/null
  pushd edgcppgen/ > /dev/null
    check_types __init__.py
  popd > /dev/null
  pushd edgdocker/ > /dev/null
    check_types __init__.py
  popd > /dev/null
  pushd edggpp/ > /dev/null
    check_types __init__.py
  popd > /dev/null
  pushd edgifc/ > /dev/null
    check_types __init__.py
    pushd decoration > /dev/null
      check_types __init__.py
    popd > /dev/null
  popd > /dev/null
  pushd edgpack/ > /dev/null
    check_types __init__.py
  popd > /dev/null
  pushd edgpolicy/ > /dev/null
    check_types_strict __init__.py
  popd > /dev/null
  pushd edgshell/ > /dev/null
    check_types __init__.py
  popd > /dev/null
  pushd edgtest/ > /dev/null
    check_types_strict __init__.py
    pushd runtest/ > /dev/null
      check_types_strict __init__.py
    popd > /dev/null
  popd > /dev/null
  pushd edgtools/ > /dev/null
    check_types __init__.py
  popd > /dev/null
  pushd edgutil/ > /dev/null
    check_types __init__.py
  popd > /dev/null
popd > /dev/null

if [[ ${legacy} -eq 0 ]]; then
  pushd services/acknowledg/ > /dev/null
    pushd server/ > /dev/null
      check_types_strict acknowledg_server
    popd > /dev/null
  popd > /dev/null
else
  echo 'legacy: skipping AcknowlEDG server typecheck'
fi

echo 'Running checks...'
mypy_python_version_args=()
if [[ -n "${EDG_MYPY_PYTHON_VERSION:-}" ]]; then
  mypy_python_version_args+=(--python-version "${EDG_MYPY_PYTHON_VERSION}")
fi

mypy --no-namespace-packages \
     --scripts-are-modules   \
     --strict-equality       \
     "${mypy_python_version_args[@]}" \
     "${standard_type_checks[@]}"
standard_status=$?

echo 'Running strict checks...'
mypy --no-namespace-packages  \
     --scripts-are-modules    \
     --strict-equality        \
     --disallow-untyped-defs  \
     --disallow-untyped-calls \
     "${mypy_python_version_args[@]}" \
     "${strict_type_checks[@]}"
strict_status=$?

if [[ ${standard_status} -ne 0 || ${strict_status} -ne 0 ]]; then
  exit 1
fi
exit 0
