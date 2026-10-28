/**
 * @file test_gcc_be_dtor.cpp
 * @brief Unit tests for global destructor registration (`__cxa_atexit` and `__cxa_thread_atexit`).
 * @details Tests `gcc_gen_be_emit_cxa_atexit` and `gcc_gen_be_emit_cxa_thread_atexit` in `gcc_gen_be_dtor.c`.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_dtor.h"
#include "gcc_gen_be_context.h"
#include <libgccjit.h>
#include <stdio.h>
#include <stdlib.h>

USING_NAMESPACE_EDG

namespace edg {
/** @brief Optimization level. */
int gcc_be_opt_level = 0;
/** @brief Debug info flag. */
a_boolean gcc_be_debug_info = FALSE;
/** @brief Dump initial tree flag. */
a_boolean gcc_be_dump_initial_tree = FALSE;
/** @brief Dump gimple flag. */
a_boolean gcc_be_dump_gimple = FALSE;
/** @brief Dump RTL flag. */
a_boolean gcc_be_dump_rtl_all = FALSE;
/** @brief Fast math flag. */
a_boolean gcc_be_ffast_math = FALSE;
/** @brief Associative math flag. */
a_boolean gcc_be_fassociative_math = FALSE;
/** @brief No trapping math flag. */
a_boolean gcc_be_fno_trapping_math = FALSE;
/** @brief Shared library flag. */
a_boolean gcc_be_shared = FALSE;
/** @brief Static linking flag. */
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
/** @brief PIC flag. */
a_boolean gcc_be_fPIC = FALSE;
/** @brief PIE flag. */
a_boolean gcc_be_fPIE = FALSE;
/** @brief No PIE flag. */
a_boolean gcc_be_fno_pie = FALSE;
/** @brief Exceptions enabled flag. */
a_boolean exceptions_enabled = TRUE;
/** @brief Active C dialect. */
a_C_dialect C_dialect = C_dialect_cplusplus;
/** @brief GCC compatibility mode flag. */
a_boolean gcc_mode = FALSE;
/** @brief Named register table. */
a_const_char *named_register_names[anr_last + 1] = { NULL };

/**
 * @brief Stub for global variable declaration lowering.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function declaration lowering.
 * @details Sets `*out_func` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout The frontend routine pointer.
 * @param[out] out_func Pointer to receive the lowered function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_func = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for builtin call lowering.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout The builtin routine pointer.
 * @param[in] num_args Number of call arguments.
 * @param[in] args Array of argument rvalues.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_builtin_call(a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    (void)num_args;
    (void)args;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for type lowering.
 * @details Populates `*out_type` with `GCC_JIT_TYPE_INT` from the active context.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_type Pointer to receive the lowered type.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    return GCC_GEN_BE_SUCCESS;
}
}

extern "C" {
/**
 * @brief Mock diagnostic reporter for dtor unit tests.
 * @details Returns the error code passed in.
 * @param[in] err Error code.
 * @param[in] msg Context message.
 * @return `err`.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char *msg) GCC_GEN_BE_NOEXCEPT {
    (void)msg;
    return err;
}
}

using namespace edg;

/**
 * @brief Executes the destructor registration unit tests.
 * @details Creates a `gcc_jit_context`, emits `__cxa_atexit` and `__cxa_thread_atexit` calls, and releases the context.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_dtor_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_lvalue *lval = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, int_type, "var");

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *dummy_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, "dummy", 0, NULL, 0);
    gcc_jit_block *block = gcc_jit_function_new_block(dummy_func, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(block));

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(void_type);
    gcc_jit_param *p1 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
    gcc_jit_function *dtor = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, "dtor", 1, &p1, 0);

    GCC_GEN_BE_CHECK(gcc_gen_be_emit_cxa_atexit(lval, dtor));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_cxa_thread_atexit(lval, dtor));

    gcc_jit_context_release(ctx);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for destructor tests.
 * @details Runs `run_dtor_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_dtor_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    puts("PASS");
    return 0;
}
