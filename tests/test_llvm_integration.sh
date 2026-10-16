#!/bin/bash
set -e

echo "Running memcpy integration test..."
build/bin/cpfe --c99 --edg_base_dir include_c++ --gen_llvm_file_name tests/lit/llvm/memcpy_test.ll tests/lit/llvm/memcpy_test.c
grep -q "@llvm.memcpy" tests/lit/llvm/memcpy_test.ll
echo "memcpy integration test passed."

echo "Running bitfield integration test..."
build/bin/cpfe --c99 --edg_base_dir include_c++ --gen_llvm_file_name tests/lit/llvm/bitfield_test.ll tests/lit/llvm/bitfield_test.c
# Check read_b has shifts and masks
grep -q "and i32" tests/lit/llvm/bitfield_test.ll
grep -q "shl i32" tests/lit/llvm/bitfield_test.ll
grep -q "ashr i32" tests/lit/llvm/bitfield_test.ll
# Check write_b has or and store
grep -q "or i32" tests/lit/llvm/bitfield_test.ll
grep -q "store i32" tests/lit/llvm/bitfield_test.ll
echo "bitfield integration test passed."
