/**
 * @file test_scalars.c
 * @brief End-to-end C test for scalar integer and floating-point local variables and arithmetic.
 * @details Verifies scalar variable allocation, initialization, addition, and comparison in `gcc_gen_be`.
 */

#include "test_common.h"

/**
 * @brief Executes scalar integer and floating-point local variable tests.
 * @details Initializes scalar `int` and `float` locals and verifies `1 + 2 == 3`.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_scalars_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    int a = 1;
    int b = 2;
    float f = 3.14f;
    (void)f;
    ASSERT_EQ(a + b, 3);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_scalars.c`.
 * @details Invokes `run_scalars_test` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_scalars_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
