#!/bin/bash
set -e

cat << 'TESTEOF' > ./test_sanitizer.cpp
int test_func(int i) {
    int arr[5] = {0};
    return arr[i];
}
TESTEOF

echo "Running ASan test..."
./build/bin/cpfe --c++20 --edg_base_dir ./bases/cmake-native/macos-arm/clang --fsanitize address --gen_llvm_file_name ./asan.ll ./test_sanitizer.cpp
if ! grep -q "sanitize_address" ./asan.ll; then
    echo "ASan attribute not found!"
    exit 1
fi
if ! grep -q "AddressSanitizer" ./asan.ll && ! grep -q "asan" ./asan.ll; then
    echo "ASan instrumentation passes not run!"
    exit 1
fi

echo "Running TSan test..."
./build/bin/cpfe --c++20 --edg_base_dir ./bases/cmake-native/macos-arm/clang --fsanitize thread --gen_llvm_file_name ./tsan.ll ./test_sanitizer.cpp
if ! grep -q "sanitize_thread" ./tsan.ll; then
    echo "TSan attribute not found!"
    exit 1
fi

echo "Running MSan test..."
./build/bin/cpfe --c++20 --edg_base_dir ./bases/cmake-native/macos-arm/clang --fsanitize memory --gen_llvm_file_name ./msan.ll ./test_sanitizer.cpp
if ! grep -q "sanitize_memory" ./msan.ll; then
    echo "MSan attribute not found!"
    exit 1
fi

echo "Running UBSan test..."
./build/bin/cpfe --c++20 --edg_base_dir ./bases/cmake-native/macos-arm/clang --fsanitize undefined --gen_llvm_file_name ./ubsan.ll ./test_sanitizer.cpp
if ! grep -q "BoundsChecking" ./ubsan.ll && ! grep -q "trap" ./ubsan.ll; then
    echo "UBSan pass (BoundsChecking) or its effects not found!"
    exit 1
fi

echo "All sanitizer integration tests passed!"
