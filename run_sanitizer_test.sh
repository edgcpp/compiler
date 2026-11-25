#!/bin/bash
set -e

CPFE_BIN="./build/bin/cpfe"
if [ ! -x "$CPFE_BIN" ] && [ -x "./.tmp/build_tests/bin/cpfe" ]; then
    CPFE_BIN="./.tmp/build_tests/bin/cpfe"
fi

# Compile a test file to LLVM IR
cat << 'TESTEOF' > ./test_sanitizer.cpp
int test_func(int i) {
    return i + 1;
}
TESTEOF

echo "Running ASan test..."
"$CPFE_BIN" --c++20 --edg_base_dir ./bases/cmake-native/macos-arm/clang --fsanitize address --gen_llvm_file_name ./asan.ll ./test_sanitizer.cpp
if ! grep -q "sanitize_address" ./asan.ll; then
    echo "ASan attribute not found!"
    exit 1
fi

echo "Running TSan test..."
"$CPFE_BIN" --c++20 --edg_base_dir ./bases/cmake-native/macos-arm/clang --fsanitize thread --gen_llvm_file_name ./tsan.ll ./test_sanitizer.cpp
if ! grep -q "sanitize_thread" ./tsan.ll; then
    echo "TSan attribute not found!"
    exit 1
fi

echo "Running MSan test..."
"$CPFE_BIN" --c++20 --edg_base_dir ./bases/cmake-native/macos-arm/clang --llvm_target x86_64-unknown-linux-gnu --fsanitize memory --gen_llvm_file_name ./msan.ll ./test_sanitizer.cpp
if ! grep -q "sanitize_memory" ./msan.ll; then
    echo "MSan attribute not found!"
    exit 1
fi

echo "Running UBSan test..."
"$CPFE_BIN" --c++20 --edg_base_dir ./bases/cmake-native/macos-arm/clang --fsanitize undefined --gen_llvm_file_name ./ubsan.ll ./test_sanitizer.cpp
if ! grep -q "\"ubsan\"" ./ubsan.ll; then
    echo "UBSan pass or its effects not found!"
    exit 1
fi

rm -f ./test_sanitizer.cpp ./asan.ll ./tsan.ll ./msan.ll ./ubsan.ll
echo "All sanitizer integration tests passed!"
