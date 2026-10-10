#!/bin/bash
set -e

if [ "$RUN_LLVM_LINK_TESTS" != "1" ]; then
    echo "Skipping functions execution test. Set RUN_LLVM_LINK_TESTS=1 to run."
    exit 0
fi

CPFE_PATH="build/test_llvm_enabled/bin/cpfe"
if [ ! -f "$CPFE_PATH" ]; then
    echo "FAIL: $CPFE_PATH not found."
    exit 1
fi

TEST_C="tests/functions_test.c"
TEST_LL="tests/functions_test.ll"

cat << 'C_EOF' > "$TEST_C"
void test_func(int my_arg) {
    int local_var = my_arg + 1;
}
C_EOF

echo "Running $CPFE_PATH on functions_test.c..."
$CPFE_PATH --gen_llvm_file_name "$TEST_LL" "$TEST_C" || {
    echo "FAIL: cpfe execution failed."
    exit 1
}

if ! grep -q "alloca" "$TEST_LL"; then
    echo "FAIL: Missing alloca instructions for local variables/arguments."
    exit 1
fi

if ! grep -q "store" "$TEST_LL"; then
    echo "FAIL: Missing store instruction to initialize argument alloca."
    exit 1
fi

echo "PASS: Function arguments and allocas generated correctly."

rm "$TEST_C" "$TEST_LL"
