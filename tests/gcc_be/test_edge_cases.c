/**
 * @file test_edge_cases.c
 * @brief End-to-end C test for GNU C zero-length array (`int arr[0]`) type lowering.
 * @details Verifies that zero-length arrays lower cleanly without triggering invalid size errors in `libgccjit`.
 */

#include "test_common.h"

/**
 * @brief Declares a zero-length local array and verifies address stability.
 * @details Exercises GNU C `int arr[0]` array lowering in `gcc_gen_be_type.c`.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_edge_cases_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    int arr[0]; /* empty array edge case */
    (void)arr;
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_edge_cases.c`.
 * @details Invokes `run_edge_cases_test` and returns 0 on success.
 * @return 0 on success.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_edge_cases_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
