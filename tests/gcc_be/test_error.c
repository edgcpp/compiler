/**
 * @file test_error.c
 * @brief C unit tests for centralized `gcc_gen_be_error_t` and diagnostic reporting (`gcc_gen_be_error.c`).
 * @details Verifies every enumerator in `gcc_gen_be_error_t`, null pointer checks,
 * unknown enumerator fallback, and `gcc_gen_be_report_diagnostic`.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_error.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Executes all error string and diagnostic reporting assertions in C mode.
 * @details Iterates over all defined `gcc_gen_be_error_t` values and verifies non-empty diagnostic strings.
 * @param[out] out_passed Pointer to receive 1 if all checks pass, 0 otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on failure.
 */
static gcc_gen_be_error_t verify_all_error_codes(int *out_passed) GCC_GEN_BE_NOEXCEPT {
    if (!out_passed) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_passed = 0;

    if (gcc_gen_be_error_string(GCC_GEN_BE_SUCCESS, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    const char *msg = NULL;
    for (int code = (int)GCC_GEN_BE_SUCCESS; code <= (int)GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED + 1; ++code) {
        msg = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_error_string((gcc_gen_be_error_t)code, &msg));
        if (!msg || msg[0] == '\0') {
            return GCC_GEN_BE_ERROR_INTERNAL;
        }
    }

    if (gcc_gen_be_report_diagnostic(GCC_GEN_BE_SUCCESS, "ok") != GCC_GEN_BE_SUCCESS) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    if (gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_DWARF_SCOPE_FAILED, "scope") != GCC_GEN_BE_ERROR_DWARF_SCOPE_FAILED) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    *out_passed = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_error.c`.
 * @details Runs `verify_all_error_codes` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int passed = 0;
    gcc_gen_be_error_t err = verify_all_error_codes(&passed);
    if (err != GCC_GEN_BE_SUCCESS || !passed) {
        return 1;
    }
    puts("PASS");
    return 0;
}
