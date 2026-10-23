/**
 * @file test_inline_asm.c
 * @brief End-to-end C test for extended inline assembly (`__asm__`) output constraint lowering.
 * @details Verifies register output constraint (`"=r"`) lowering across AArch64 and x86_64 targets.
 */

#include "test_common.h"

/**
 * @brief Executes an inline assembly instruction writing `1` to `out` and verifies `out == 1`.
 * @details Tests `"=r"` output operand binding in `gcc_gen_be_stmt.c`.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_inline_asm_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    int out = 0;
#if defined(__aarch64__)
    __asm__("mov %w0, 1" : "=r"(out));
#elif defined(__x86_64__)
    __asm__("mov $1, %0" : "=r"(out));
#else
    out = 1;
#endif
    ASSERT_EQ(out, 1);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_inline_asm.c`.
 * @details Invokes `run_inline_asm_test` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_inline_asm_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
