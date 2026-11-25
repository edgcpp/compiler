/**
 * @file test_noexcept_raii.cpp
 * @brief End-to-end `-fno-exceptions` (`--no_exceptions`) C++ RAII scope destructor test.
 * @details Verifies that when exceptions are disabled (`-fno-exceptions`), local C++ destructors
 * still execute in exact reverse construction order on normal block exit, `return`, `break`,
 * `continue`, and `goto` across nested lexical scopes.
 * @ingroup gcc_gen_be_tests
 */

#include "test_common.h"

namespace {

/**
 * @brief RAII scope guard recording destruction order when `-fno-exceptions` is active.
 */
struct ScopeRecorder {
    /** @brief Pointer to decimal trace integer. */
    long *trace;
    /** @brief Digit `[1..9]` appended on destruction. */
    int id;

    /**
     * @brief Constructs `ScopeRecorder`.
     * @param[in] t Pointer to trace accumulator.
     * @param[in] i Identifier digit.
     */
    ScopeRecorder(long *t, int i) noexcept : trace(t), id(i) {}

    /**
     * @brief Appends `id` to `*trace` on scope exit.
     */
    ~ScopeRecorder() noexcept {
        if (trace) {
            *trace = (*trace * 10L) + (long)id;
        }
    }
};

/**
 * @brief Helper testing early `return` from nested scopes under `-fno-exceptions`.
 * @details Constructs `r1(1)` in outer scope and `r2(2)`, `r3(3)` in inner scope before `return`.
 * @param[in,out] trace Destruction trace accumulator.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t helper_early_return(long *trace) noexcept {
    ScopeRecorder r1(trace, 1);
    {
        ScopeRecorder r2(trace, 2);
        ScopeRecorder r3(trace, 3);
        return GCC_GEN_BE_SUCCESS;
    }
}

/**
 * @brief Tests block exit, `return`, `break`, and `goto` RAII cleanups under `-fno-exceptions`.
 * @details Verifies exact reverse construction order across all four control-flow exits.
 * @param[out] out_ok Pointer receiving `1` when all assertions pass.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
gcc_gen_be_error_t run_noexcept_raii_tests(int *out_ok) noexcept {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    /* 1. Normal lexical block exit */
    long blk_trace = 0;
    {
        ScopeRecorder a(&blk_trace, 1);
        {
            ScopeRecorder b(&blk_trace, 2);
            ScopeRecorder c(&blk_trace, 3);
        }
    }
    ASSERT_EQ(blk_trace, 321L);

    /* 2. Early `return` from nested scope */
    long ret_trace = 0;
    GCC_GEN_BE_CHECK(helper_early_return(&ret_trace));
    ASSERT_EQ(ret_trace, 321L);

    /* 3. Loop `break` */
    long brk_trace = 0;
    for (int i = 0; i < 4; ++i) {
        ScopeRecorder lp(&brk_trace, i + 4);
        if (i == 1) {
            ScopeRecorder inner(&brk_trace, 9);
            break;
        }
    }
    /* Iteration 0: ~lp(4) -> 4; Iteration 1: ~inner(9) then ~lp(5) -> 495 */
    ASSERT_EQ(brk_trace, 495L);

    /* 4. Forward `goto` exiting nested scope */
    long goto_trace = 0;
    {
        ScopeRecorder outer_g(&goto_trace, 6);
        {
            ScopeRecorder inner_g1(&goto_trace, 7);
            ScopeRecorder inner_g2(&goto_trace, 8);
            goto after_inner;
        }
    after_inner:
        ASSERT_EQ(goto_trace, 87L);
    }
    ASSERT_EQ(goto_trace, 876L);

    /* 5. Loop `continue` exiting nested scope */
    long cont_trace = 0;
    for (int j = 0; j < 2; ++j) {
        ScopeRecorder outer_c(&cont_trace, j + 1);
        if (j == 0) {
            ScopeRecorder inner_c(&cont_trace, 9);
            continue;
        }
    }
    /* j=0: ~inner_c(9) then ~outer_c(1) -> 91; j=1: ~outer_c(2) -> 912 */
    ASSERT_EQ(cont_trace, 912L);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

} // namespace

/**
 * @brief Entry point for `test_noexcept_raii.cpp`.
 * @details Runs `run_noexcept_raii_tests` and returns `0` on success.
 * @return `0` on success, `1` on failure.
 */
int main() noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_noexcept_raii_tests(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok == 1) ? 0 : 1;
}
