#!/bin/bash
set -e

# Only run if explicitly requested or in a CI environment
if [ "$RUN_LLVM_LINK_TESTS" != "1" ]; then
    echo "Skipping globals linkage execution test. Set RUN_LLVM_LINK_TESTS=1 to run."
    exit 0
fi

CPFE_PATH="build/test_llvm_enabled/bin/cpfe"
if [ ! -f "$CPFE_PATH" ]; then
    echo "FAIL: $CPFE_PATH not found."
    exit 1
fi

TEST_C="tests/globals_test.c"
TEST_LL="tests/globals_test.ll"

cat << 'C_EOF' > "$TEST_C"
static int my_static_var = 42;
extern int my_extern_var;
int my_uninit_global;
C_EOF

echo "Running $CPFE_PATH on globals_test.c..."
$CPFE_PATH --gen_llvm_file_name "$TEST_LL" "$TEST_C" || {
    echo "FAIL: cpfe execution failed."
    exit 1
}

# Now we verify the LLVM IR output
if ! grep -q "my_static_var = internal global" "$TEST_LL"; then
    echo "FAIL: my_static_var linkage incorrect or missing."
    exit 1
fi

if ! grep -q "my_extern_var = external global" "$TEST_LL"; then
    echo "FAIL: my_extern_var linkage incorrect or missing."
    exit 1
fi

if ! grep -q "my_uninit_global = global i32 0" "$TEST_LL" && ! grep -q "my_uninit_global = common global" "$TEST_LL"; then
    echo "FAIL: my_uninit_global linkage incorrect or missing."
    exit 1
fi

echo "PASS: Global declarations and linkage tests passed."

rm "$TEST_C" "$TEST_LL"
