#!/bin/bash
set -e

# Run cmake with coverage flags
cmake -B build/coverage -S . -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_C_FLAGS="--coverage" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/coverage

# Run tests
cd build/coverage
ctest --output-on-failure
cd ../..

# Generate coverage report for src/llvm_gen_be*.cpp
mkdir -p coverage_report
lcov --capture --directory build/coverage --output-file coverage_report/coverage.info
lcov --extract coverage_report/coverage.info '*/src/llvm_gen_be*.cpp' --output-file coverage_report/llvm_be_coverage.info

# Check for 100% coverage
lcov --summary coverage_report/llvm_be_coverage.info > coverage_report/summary.txt
cat coverage_report/summary.txt

if ! grep -q "lines......: 100.0%" coverage_report/summary.txt; then
  echo "Error: Line coverage is not 100%"
  exit 1
fi

if ! grep -q "functions..: 100.0%" coverage_report/summary.txt; then
  echo "Error: Function coverage is not 100%"
  exit 1
fi

echo "Coverage is 100%!"
exit 0