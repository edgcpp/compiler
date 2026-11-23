#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/.tmp/build_tests"
PROF_DIR="${ROOT_DIR}/.tmp/prof"

mkdir -p "${PROF_DIR}"
rm -f "${PROF_DIR}"/*.profraw "${PROF_DIR}/merged.profdata"

cmake -B "${BUILD_DIR}" -S "${ROOT_DIR}" -DENABLE_LLVM_BACKEND=ON -DBUILD_TESTING=ON -DEDG_LLVM_BE_COVERAGE=ON -DEDG_BUILD_LIB=OFF -DLLVM_DIR="$(brew --prefix llvm)/lib/cmake/llvm" > "${PROF_DIR}/cmake_cfg.log" 2>&1

cmake --build "${BUILD_DIR}" --parallel 8 --target check_doc_coverage error-info test_llvm_type_lowering test_llvm_const_evaluation test_llvm_expressions test_llvm_control_flow test_llvm_exceptions test_llvm_declarations test_llvm_abi test_llvm_debug_info test_llvm_codegen test_llvm_opt_pipeline test_llvm_pgo test_llvm_lto test_llvm_vtable_rtti test_llvm_coro_atomics test_llvm_inline_asm test_llvm_msvc_asm test_llvm_builtins test_llvm_target_intrinsics test_llvm_error test_llvm_orchestrator test_llvm_decl test_llvm_stmt test_llvm_omp test_llvm_datalayout test_llvm_type_translation test_llvm_verifier test_llvm_driver_flag test_llvm_fuzz test_llvm_build_config > "${PROF_DIR}/cmake_build.log" 2>&1

OBJ_ARGS=()
FIRST_BIN=""
for bin in "${BUILD_DIR}"/bin/test_llvm_*; do
  if [[ -x "${bin}" && ! -d "${bin}" ]]; then
    LLVM_PROFILE_FILE="${PROF_DIR}/$(basename "${bin}")_%p.profraw" "${bin}" > /dev/null
    if [[ -z "${FIRST_BIN}" ]]; then
      FIRST_BIN="${bin}"
    else
      OBJ_ARGS+=("-object=${bin}")
    fi
  fi
done

xcrun llvm-profdata merge -sparse "${PROF_DIR}"/*.profraw -o "${PROF_DIR}/merged.profdata"

REPORT_OUT="${PROF_DIR}/coverage_summary.txt"
xcrun llvm-cov report --instr-profile="${PROF_DIR}/merged.profdata" "${FIRST_BIN}" "${OBJ_ARGS[@]}" --sources "${ROOT_DIR}"/src/llvm_gen_be*.cpp "${ROOT_DIR}"/src/llvm_gen_be*.h > "${REPORT_OUT}"

cat "${REPORT_OUT}"

TOTAL_LINE="$(grep -E '^TOTAL\s+' "${REPORT_OUT}")"
if ! echo "${TOTAL_LINE}" | grep -qE '100\.00%.*100\.00%.*100\.00%.*100\.00%'; then
  echo "Error: Coverage is not 100.00% across regions, functions, lines, and branches!"
  exit 1
fi

echo "Coverage is 100%!"
exit 0
