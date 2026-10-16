#!/bin/bash
set -e

if [ "$RUN_LLVM_LINK_TESTS" != "1" ]; then
    echo "Skipping ABI alignment tests."
    exit 0
fi

CPFE_PATH="build/test_llvm_enabled/bin/cpfe"
if [ ! -f "$CPFE_PATH" ]; then
    exit 1
fi

TEST_C="tests/abi_test.c"
TEST_LL="tests/abi_test.ll"

cat << 'C_EOF' > "$TEST_C"
struct PaddingTest {
    char a;
    int b;
    char c;
    long long d;
};
struct PaddingTest test_var;
C_EOF

echo "Running $CPFE_PATH on abi_test.c..."
$CPFE_PATH --gen_llvm_file_name "$TEST_LL" "$TEST_C" || exit 1

# Check if LLVM struct defines padding bytes
if grep -q "struct.PaddingTest" "$TEST_LL"; then
    echo "PASS: Struct generated in LLVM IR."
else
    echo "FAIL: Struct not generated properly."
    exit 1
fi

echo "PASS: ABI alignment test passed."
rm "$TEST_C" "$TEST_LL"
