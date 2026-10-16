#!/bin/bash
set -e

if [ "$RUN_LLVM_LINK_TESTS" != "1" ]; then
    echo "Skipping output generation execution test. Set RUN_LLVM_LINK_TESTS=1 to run."
    exit 0
fi

CPFE_PATH="build/test_llvm_enabled/bin/cpfe"
if [ ! -f "$CPFE_PATH" ]; then
    echo "FAIL: $CPFE_PATH not found."
    exit 1
fi

TEST_C="tests/empty_main.c"
# By default, without --gen_llvm_file_name, if it's activated via --llvm, it might use empty_main.ll
# But since we only have --gen_llvm_file_name, we test if it outputs to the given path and format.
TEST_LL="tests/empty_main.ll"

cat << 'C_EOF' > "$TEST_C"
int main() {
    return 0;
}
C_EOF

echo "Running $CPFE_PATH on empty_main.c..."
$CPFE_PATH --gen_llvm_file_name "$TEST_LL" "$TEST_C" || {
    echo "FAIL: cpfe execution failed."
    exit 1
}

if [ ! -f "$TEST_LL" ]; then
    echo "FAIL: Output file $TEST_LL was not generated."
    exit 1
fi

# Verify structural match for .ll
if ! grep -q "^target datalayout = " "$TEST_LL"; then
    echo "FAIL: Missing target datalayout in output."
    exit 1
fi

if ! grep -q "^define .* @main(" "$TEST_LL"; then
    echo "FAIL: Missing definition for main in output."
    exit 1
fi

echo "PASS: Output generation tests passed."

rm "$TEST_C" "$TEST_LL"
