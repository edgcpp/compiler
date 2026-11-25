#!/bin/bash
set -e

CPFE_PATH="${CPFE_BIN:-.tmp/build_tests/bin/cpfe}"
if [ ! -f "$CPFE_PATH" ]; then
    CPFE_PATH="build/coverage/bin/cpfe"
fi
if [ ! -f "$CPFE_PATH" ]; then
    echo "FAIL: $CPFE_PATH not found."
    exit 1
fi

TEST_DIR=".tmp/execution_suite"
mkdir -p "$TEST_DIR"

# 1. C execution test via cpfe -> native executable (-o)
cat << 'C_EOF' > "$TEST_DIR/exec_test.c"
extern int printf(const char*, ...);
int main(void) {
    int sum = 0;
    for (int i = 0; i < 10; ++i) {
        sum += i;
    }
    printf("Sum: %d%c", sum, 10);
    return 0;
}
C_EOF

echo "Compiling C execution test with cpfe -> native executable..."
"$CPFE_PATH" --c99 --gen_obj_file_name "$TEST_DIR/exec_test.o" -o "$TEST_DIR/exec_test.out" "$TEST_DIR/exec_test.c"
OUT_C=$("$TEST_DIR/exec_test.out")
if [ "$OUT_C" != "Sum: 45" ]; then
    echo "FAIL: C execution output mismatch: $OUT_C"
    exit 1
fi

# 2. C++20 end-to-end execution test (namespaces, classes, methods, templates, lambdas, control flow)
cat << 'CPP_EOF' > "$TEST_DIR/exec_cpp20.cpp"
extern "C" int printf(const char*, ...);

namespace math_ns {
template <typename T>
T accumulate_range(T n) {
    T acc = 0;
    for (T i = 1; i <= n; ++i) {
        acc += i;
    }
    return acc;
}

struct Counter {
    int base;
    int compute(int x) const {
        return base + accumulate_range<int>(x);
    }
};
} // namespace math_ns

int main() {
    math_ns::Counter c{10};
    auto lam = [](int v) { return v * 2; };
    int res = lam(c.compute(5));
    printf("Result: %d%c", res, 10);
    return (res == 50) ? 0 : 1;
}
CPP_EOF

echo "Compiling C++20 execution test with cpfe -> native executable..."
"$CPFE_PATH" --c++20 --gen_obj_file_name "$TEST_DIR/exec_cpp20.o" -o "$TEST_DIR/exec_cpp20.out" "$TEST_DIR/exec_cpp20.cpp"
OUT_CPP=$("$TEST_DIR/exec_cpp20.out")
if [ "$OUT_CPP" != "Result: 50" ]; then
    echo "FAIL: C++20 execution output mismatch: $OUT_CPP"
    exit 1
fi

echo "PASS: End-to-end C and C++20 execution tests passed!"
rm -rf "$TEST_DIR"
