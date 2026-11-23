#!/bin/bash
set -e

CPFE_PATH="${CPFE_BIN:-.tmp/build_tests/bin/cpfe}"
if [ ! -f "$CPFE_PATH" ]; then
    CPFE_PATH="build/bin/cpfe"
fi

echo "Running memcpy integration test..."
"$CPFE_PATH" --c99 --edg_base_dir include_c++ --gen_llvm_file_name .tmp/memcpy_test.ll tests/lit/llvm/memcpy_test.c
grep -q "@llvm.memcpy" .tmp/memcpy_test.ll
echo "memcpy integration test passed."

echo "Running bitfield integration test..."
"$CPFE_PATH" --c99 --edg_base_dir include_c++ --gen_llvm_file_name .tmp/bitfield_test.ll tests/lit/llvm/bitfield_test.c
grep -qE "and i(16|32)" .tmp/bitfield_test.ll
grep -qE "shl i(16|32)" .tmp/bitfield_test.ll
grep -qE "ashr i(16|32)" .tmp/bitfield_test.ll
grep -qE "or i(16|32)" .tmp/bitfield_test.ll
grep -qE "store i(16|32)" .tmp/bitfield_test.ll
echo "bitfield integration test passed."
rm -f .tmp/memcpy_test.ll .tmp/bitfield_test.ll
