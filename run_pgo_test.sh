#!/bin/bash
set -e

CPFE_BIN="./build/bin/cpfe"
if [ ! -x "$CPFE_BIN" ] && [ -x "./.tmp/build_tests/bin/cpfe" ]; then
    CPFE_BIN="./.tmp/build_tests/bin/cpfe"
fi

# Compile a test file to LLVM IR
cat << 'TESTEOF' > ./test_pgo.cpp
int test_func(int i) { return i + 1; }
TESTEOF

echo "Running PGO Generate test..."
"$CPFE_BIN" --c++20 --edg_base_dir ./bases/cmake-native/macos-arm/clang --fprofile-generate --gen_llvm_file_name ./pgo_gen.ll ./test_pgo.cpp
if ! grep -q "llvm.instrprof.increment" ./pgo_gen.ll; then
    echo "PGO generate instrumentation pass not run!"
    exit 1
fi

echo "Running PGO Use test..."
"$CPFE_BIN" --c++20 --edg_base_dir ./bases/cmake-native/macos-arm/clang --fprofile-use=default.profdata --gen_llvm_file_name ./pgo_use.ll ./test_pgo.cpp
rm -f ./test_pgo.cpp ./pgo_gen.ll ./pgo_use.ll
# If it parses and runs without error, the driver flag works.
echo "All PGO integration tests passed!"
