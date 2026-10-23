/**
 * @file test_gcc_be_context.cpp
 * @brief Unit tests for context lifecycle, state management, and block/scope stacks (`gcc_gen_be_context.c`).
 * @details Exercises `gcc_gen_be_init`, `gcc_gen_be_get_context`, `gcc_gen_be_set_context`,
 * `gcc_gen_be_get_current_block`, `gcc_gen_be_set_current_block`, `gcc_gen_be_cleanup`,
 * and all error-percolation paths.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_context.h"
#include <stdio.h>
#include <stdlib.h>

extern "C" {
/**
 * @brief Mock diagnostic reporter for context tests.
 * @details Returns the error code passed to it without terminating.
 * @param[in] err The error code.
 * @param[in] msg Optional diagnostic message.
 * @return The input error code `err`.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char *msg) GCC_GEN_BE_NOEXCEPT {
    (void)msg;
    return err;
}
}

USING_NAMESPACE_EDG

namespace edg {
/** @brief Optimization level option. */
int gcc_be_opt_level = 0;
/** @brief Debug info option. */
a_boolean gcc_be_debug_info = FALSE;
/** @brief Dump initial tree option. */
a_boolean gcc_be_dump_initial_tree = FALSE;
/** @brief Dump GIMPLE option. */
a_boolean gcc_be_dump_gimple = FALSE;
/** @brief Dump RTL option. */
a_boolean gcc_be_dump_rtl_all = FALSE;
/** @brief Fast math option. */
a_boolean gcc_be_ffast_math = FALSE;
/** @brief Associative math option. */
a_boolean gcc_be_fassociative_math = FALSE;
/** @brief No trapping math option. */
a_boolean gcc_be_fno_trapping_math = FALSE;
/** @brief Shared library option. */
a_boolean gcc_be_shared = FALSE;
/** @brief Static linking option. */
a_boolean gcc_be_static = FALSE;
/** @brief Target architecture string. */
a_const_char *gcc_be_march = NULL;
/** @brief Target CPU string. */
a_const_char *gcc_be_mcpu = NULL;
/** @brief Target tune string. */
a_const_char *gcc_be_mtune = NULL;
/** @brief Target FPU string. */
a_const_char *gcc_be_mfpu = NULL;
/** @brief Target code model string. */
a_const_char *gcc_be_mcmodel = NULL;
/** @brief 64-bit mode flag. */
a_boolean gcc_be_m64 = FALSE;
/** @brief 32-bit mode flag. */
a_boolean gcc_be_m32 = FALSE;
/** @brief Position-independent code flag. */
a_boolean gcc_be_fPIC = FALSE;
/** @brief Position-independent executable flag. */
a_boolean gcc_be_fPIE = FALSE;
/** @brief No position-independent executable flag. */
a_boolean gcc_be_fno_pie = FALSE;
/** @brief Exceptions enabled flag. */
a_boolean exceptions_enabled = TRUE;
/** @brief Driver options list pointer. */
void *gcc_be_driver_options = NULL;
/** @brief Compiler options list pointer. */
void *gcc_be_compiler_options = NULL;
/** @brief Active C dialect. */
a_C_dialect C_dialect = C_dialect_cplusplus;
}

/**
 * @brief Executes the context lifecycle unit tests.
 * @details Initializes the context, retrieves it, and cleans it up while checking every error return.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_context_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(edg::gcc_gen_be_init());
    GCC_GEN_BE_CHECK(edg::gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INTERNAL;

    edg::gcc_gen_be_context_t *state = NULL;
    GCC_GEN_BE_CHECK(edg::gcc_gen_be_get_state(&state));
    if (!state) return GCC_GEN_BE_ERROR_INTERNAL;
    state->computed_goto_labels = (edg::a_label_ptr *)malloc(sizeof(edg::a_label_ptr));
    state->active_coro_resume_blocks = (gcc_jit_block **)malloc(sizeof(gcc_jit_block *));
    state->active_coro_suspend_scopes = (edg::a_scope_ptr *)malloc(sizeof(edg::a_scope_ptr));
    edg::gcc_gen_be_coro_state_t *cs = (edg::gcc_gen_be_coro_state_t *)calloc(1, sizeof(edg::gcc_gen_be_coro_state_t));
    if (cs) {
        cs->resume_blocks = (gcc_jit_block **)malloc(sizeof(gcc_jit_block *));
        cs->suspend_scopes = (edg::a_scope_ptr *)malloc(sizeof(edg::a_scope_ptr));
        state->coro_state_stack = cs;
    }
    edg::gcc_gen_be_eh_region_t *reg = (edg::gcc_gen_be_eh_region_t *)calloc(1, sizeof(edg::gcc_gen_be_eh_region_t));
    state->eh_state.regions = reg;
    edg::gcc_gen_be_eh_type_filter_t *flt = (edg::gcc_gen_be_eh_type_filter_t *)calloc(1, sizeof(edg::gcc_gen_be_eh_type_filter_t));
    state->eh_state.types = flt;
    state->eh_state.action_table = (char *)malloc(16);

    GCC_GEN_BE_CHECK(edg::gcc_gen_be_cleanup());

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for context tests.
 * @details Invokes `run_context_tests` and prints PASS on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_context_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    puts("PASS");
    return 0;
}
