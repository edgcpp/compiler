#!/bin/bash
set -e

if [ "$RUN_LLVM_LINK_TESTS" != "1" ]; then
    echo "Skipping LLVM execution tests. Set RUN_LLVM_LINK_TESTS=1 to run."
    exit 0
fi

CPFE_PATH="build/coverage/bin/cpfe"
if [ ! -f "$CPFE_PATH" ]; then
    echo "FAIL: $CPFE_PATH not found."
    exit 1
fi

if ! command -v lli >/dev/null 2>&1; then
    echo "FAIL: lli (LLVM interpreter) not found on PATH."
    exit 1
fi

if ! command -v gcc >/dev/null 2>&1; then
    echo "FAIL: gcc not found on PATH."
    exit 1
fi

TEST_DIR="tests/execution_suite"
mkdir -p "$TEST_DIR"

# Basic execution test
cat << 'C_EOF' > "$TEST_DIR/exec_test.c"
#include <stdio.h>
int main() {
    int sum = 0;
    for (int i = 0; i < 10; ++i) {
        sum += i;
    }
    printf("Sum: %d\n", sum);
    return 0;
}
C_EOF

echo "Compiling with GCC..."
gcc "$TEST_DIR/exec_test.c" -o "$TEST_DIR/exec_test.gcc.out"
"$TEST_DIR/exec_test.gcc.out" > "$TEST_DIR/gcc.stdout"

echo "Compiling with cpfe (LLVM)..."
$CPFE_PATH --gen_llvm_file_name "$TEST_DIR/exec_test.ll" "$TEST_DIR/exec_test.c"

echo "Executing with lli..."
lli "$TEST_DIR/exec_test.ll" > "$TEST_DIR/lli.stdout"

if ! diff -u "$TEST_DIR/gcc.stdout" "$TEST_DIR/lli.stdout"; then
    echo "FAIL: Execution outputs differ."
    exit 1
fi

echo "PASS: Execution test passed! stdout matches baseline."
rm -rf "$TEST_DIR"
