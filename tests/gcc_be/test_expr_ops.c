/**
 * @file test_expr_ops.c
 * @brief End-to-end C test for compound assignment operators (`+=`, `<<=`, `>>=`, `&=`, `^=`, `|=`).
 * @details Verifies arithmetic and bitwise compound assignment lowering in `gcc_gen_be_expr.c`.
 */

#include "test_common.h"

/**
 * @brief Executes sequential compound assignment assertions.
 * @details Exercises `+=`, `<<=`, `>>=`, `&=`, `^=`, and `|=` and verifies intermediate and final values.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_expr_ops_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    int a = 1, b = 2;
    a += b;
    ASSERT_EQ(a, 3);
    a <<= 1;
    ASSERT_EQ(a, 6);
    a >>= 1;
    ASSERT_EQ(a, 3);
    a &= 1;
    ASSERT_EQ(a, 1);
    a ^= 3;
    ASSERT_EQ(a, 2);
    a |= 4;
    ASSERT_EQ(a, 6);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_expr_ops.c`.
 * @details Invokes `run_expr_ops_test` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_expr_ops_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
