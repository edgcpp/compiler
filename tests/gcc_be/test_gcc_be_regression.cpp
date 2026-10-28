/**
 * @file test_gcc_be_regression.cpp
 * @brief Regression test suite for the GCC backend (`gcc_gen_be`).
 * @details Verifies error enum percolation and regression invariants.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_error.h"
#include <cstdio>

/**
 * @brief Executes regression checks for error code string lookup.
 * @details Ensures `GCC_GEN_BE_SUCCESS` resolves to a valid non-empty string.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_regression_checks(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    for (int code = (int)GCC_GEN_BE_SUCCESS; code <= (int)GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED; ++code) {
        const char *msg = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_error_string((gcc_gen_be_error_t)code, &msg));
        if (!msg || msg[0] == '\0') return GCC_GEN_BE_ERROR_INTERNAL;
    }

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_regression.cpp`.
 * @details Runs `run_regression_checks` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_regression_checks(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
