#!/bin/bash
set -e

if [ "$RUN_LLVM_LINK_TESTS" != "1" ]; then
    echo "Skipping expressions execution test. Set RUN_LLVM_LINK_TESTS=1 to run."
    exit 0
fi

CPFE_PATH="build/test_llvm_enabled/bin/cpfe"
if [ ! -f "$CPFE_PATH" ]; then
    echo "FAIL: $CPFE_PATH not found."
    exit 1
fi

TEST_C="tests/expressions_test.c"
TEST_LL="tests/expressions_test.ll"

cat << 'C_EOF' > "$TEST_C"
struct Nested {
    int field;
};
struct Outer {
    struct Nested array[10];
};

int math_test(int a, unsigned int b) {
    int x = a + 1;
    unsigned int y = b / 2;
    int z = a / 2;
    return x + y + z;
}

int gep_test(struct Outer* o) {
    return o->array[5].field;
}
C_EOF

echo "Running $CPFE_PATH on expressions_test.c..."
$CPFE_PATH --gen_llvm_file_name "$TEST_LL" "$TEST_C" || {
    echo "FAIL: cpfe execution failed."
    exit 1
}

# Check signed vs unsigned div
if ! grep -q "udiv" "$TEST_LL"; then
    echo "FAIL: Missing unsigned division (udiv)."
    exit 1
fi

if ! grep -q "sdiv" "$TEST_LL"; then
    echo "FAIL: Missing signed division (sdiv)."
    exit 1
fi

# Check GEP for nested access
if ! grep -q "getelementptr" "$TEST_LL"; then
    echo "FAIL: Missing getelementptr instruction for struct/array access."
    exit 1
fi

echo "PASS: Expression translation tests passed."

rm "$TEST_C" "$TEST_LL"
