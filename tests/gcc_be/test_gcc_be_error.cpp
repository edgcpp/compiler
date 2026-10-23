/**
 * @file test_gcc_be_error.cpp
 * @brief Unit tests for centralized error enum and diagnostic reporting (`gcc_gen_be_error.c`).
 * @details Tests every enumerator in `gcc_gen_be_error_t`, null pointer handling,
 * unknown error code fallbacks, and `gcc_gen_be_report_diagnostic`.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_error.h"
#include <cstdio>

/**
 * @brief Runs all error enumerator string and diagnostic reporting assertions.
 * @details Exercises every `gcc_gen_be_error_t` value in `gcc_gen_be_error_string` and `gcc_gen_be_report_diagnostic`.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_error_enum_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    if (gcc_gen_be_error_string(GCC_GEN_BE_SUCCESS, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    const char *msg = NULL;
    for (int code = (int)GCC_GEN_BE_SUCCESS; code <= (int)GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED + 1; ++code) {
        msg = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_error_string((gcc_gen_be_error_t)code, &msg));
        if (!msg) return GCC_GEN_BE_ERROR_INTERNAL;
    }

    if (gcc_gen_be_report_diagnostic(GCC_GEN_BE_SUCCESS, "ok") != GCC_GEN_BE_SUCCESS) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_INTERNAL, "ctx") != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_INTERNAL, NULL) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_INTERNAL, "") != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for the error enum unit test executable.
 * @details Runs `run_error_enum_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_error_enum_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}

