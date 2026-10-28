/**
 * @file test_short_circuit.c
 * @brief End-to-end C test for logical short-circuit (`||` and `&&`) evaluation.
 * @details Verifies that the right-hand side of `1 || (x = 1)` is never evaluated.
 */

#include "test_common.h"

/**
 * @brief Executes logical short-circuit evaluation checks.
 * @details Evaluates `1 || (x = 1)` and asserts `x == 0`.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_short_circuit_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    int x = 0;
    if (1 || (x = 1)) {}
    ASSERT_EQ(x, 0);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_short_circuit.c`.
 * @details Invokes `run_short_circuit_test` and returns 0 on success.
 * @return 0 on success, non-zero if short-circuit evaluation failed.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_short_circuit_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
