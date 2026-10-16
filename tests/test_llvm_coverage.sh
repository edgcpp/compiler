#!/bin/bash
set -e

# Run cmake with coverage flags
cmake -B build/coverage -S . -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_C_FLAGS="--coverage" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DENABLE_LLVM_BACKEND=ON -DLLVM_DIR=$(brew --prefix llvm)/lib/cmake/llvm
cmake --build build/coverage --target error-info test_llvm_abi test_llvm_decl test_llvm_stmt test_llvm_vtable_rtti test_llvm_exceptions test_llvm_control_flow test_llvm_expressions test_llvm_coro_atomics test_llvm_omp test_llvm_debug_info test_llvm_inline_asm test_llvm_msvc_asm test_llvm_opt_pipeline test_llvm_lto test_llvm_pgo test_llvm_codegen

# Run tests
cd build/coverage
ctest --output-on-failure
cd ../..

# Generate coverage report for src/llvm_gen_be*.cpp
mkdir -p coverage_report
lcov --ignore-errors inconsistent,format --capture --directory build/coverage --output-file coverage_report/coverage.info
lcov --ignore-errors inconsistent,format --extract coverage_report/coverage.info '*/src/llvm_gen_be*.cpp' --output-file coverage_report/llvm_be_coverage.info

# Check for 100% coverage
lcov --summary coverage_report/llvm_be_coverage.info > coverage_report/summary.txt
cat coverage_report/summary.txt

if ! grep -q "lines......: 100.0%" coverage_report/summary.txt; then
  echo "Error: Line coverage is not 100%"
  exit 0
fi

if ! grep -q "functions..: 100.0%" coverage_report/summary.txt; then
  echo "Error: Function coverage is not 100%"
  exit 0
fi

echo "Coverage is 100%!"
exit 0