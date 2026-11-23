#!/bin/bash
set -e

CPFE_PATH="${CPFE_BIN:-.tmp/build_tests/bin/cpfe}"
if [ ! -f "$CPFE_PATH" ]; then
    CPFE_PATH="build/test_llvm_enabled/bin/cpfe"
fi
if [ ! -f "$CPFE_PATH" ]; then
    echo "FAIL: $CPFE_PATH not found."
    exit 1
fi

TEST_C=".tmp/empty_main.c"
TEST_LL=".tmp/empty_main.ll"
mkdir -p .tmp

cat << 'C_EOF' > "$TEST_C"
int main() {
    return 0;
}
C_EOF

echo "Running $CPFE_PATH on empty_main.c..."
"$CPFE_PATH" --gen_llvm_file_name "$TEST_LL" "$TEST_C" || {
    echo "FAIL: cpfe execution failed."
    exit 1
}

if [ ! -f "$TEST_LL" ]; then
    echo "FAIL: Output file $TEST_LL was not generated."
    exit 1
fi

if ! grep -q "^target datalayout = " "$TEST_LL"; then
    echo "FAIL: Missing target datalayout in output."
    exit 1
fi

if ! grep -q "^define .* @main(" "$TEST_LL"; then
    echo "FAIL: Missing definition for main in output."
    exit 1
fi

echo "PASS: Output generation tests passed."
rm -f "$TEST_C" "$TEST_LL"
