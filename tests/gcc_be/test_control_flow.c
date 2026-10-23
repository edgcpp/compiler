/**
 * @file test_control_flow.c
 * @brief End-to-end C test for loops, `break`/`continue`, and GNU C computed `goto *` dispatch tables.
 * @details Verifies `for` loop control flow, static arrays of label addresses (`&&lbl`), label address
 * comparisons, and multi-site `goto *dispatch[idx]` indirect jumps.
 */

#include "test_common.h"

/**
 * @brief Executes loop and computed-goto state transitions and verifies the final accumulator sum.
 * @details Tests `break`, `continue`, static label address tables, and indirect jumps.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_control_flow_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    int i;
    int sum = 0;
    for (i = 0; i < 10; ++i) {
        if (i % 2 == 0) continue;
        if (i == 7) break;
        sum += i;
    }
    ASSERT_EQ(sum, (1 + 3 + 5));

    static const void *dispatch[] = { &&lbl_step1, &&lbl_step2, &&lbl_done };
    ASSERT_NE(&&lbl_step1, &&lbl_step2);
    int idx = 0;
    goto *dispatch[idx];

lbl_step1:
    sum += 10;
    idx = 1;
    goto *dispatch[idx];

lbl_step2:
    sum += 20;
    idx = 2;
    goto *dispatch[idx];

lbl_done:
    ASSERT_EQ(sum, 39);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_control_flow.c`.
 * @details Invokes `run_control_flow_test` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_control_flow_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
