/**
 * @file test_gcc_be_eh.cpp
 * @brief Unit tests for Exception Handling lowering (`gcc_gen_be_eh.c`).
 * @details Tests SjLj/DWARF initialization, basic try/catch validation, and error handling.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_context.h"
#include <libgccjit.h>
#include <cstdio>

extern "C" {
/**
 * @brief Mock diagnostic reporter for EH unit tests.
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
a_boolean gcc_mode = TRUE;

/**
 * @brief Stub for computing type size in EH unit tests.
 * @details Returns `tp->size` directly.
 * @param[in] tp The frontend type pointer.
 * @return Byte size of `tp`.
 */
a_targ_size_t f_size_of_type(a_type_ptr tp) {
    return tp ? tp->size : 0;
}

/**
 * @brief Stub for integer constant extraction in EH unit tests.
 * @details Sets `*err` to `FALSE` and returns 0.
 * @param[in] cp Constant pointer.
 * @param[out] err Overflow error flag.
 * @return Integer value of constant.
 */
a_host_large_integer value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    (void)cp;
    if (err) *err = FALSE;
    return 0;
}

/**
 * @brief Stub for type lowering in EH unit tests.
 * @details Sets `*out_type` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_type Pointer to receive the lowered `gcc_jit_type`.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_type = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression lvalue lowering in EH unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] expr The frontend expression pointer.
 * @param[out] out_lval Pointer to receive the lowered `gcc_jit_lvalue`.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression rvalue lowering in EH unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] expr The frontend expression pointer.
 * @param[out] out_rval Pointer to receive the lowered `gcc_jit_rvalue`.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for statement lowering in EH unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] stmt The frontend statement pointer.
 * @param[in] func The active JIT function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for dynamic initialization lowering in EH unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] dip Dynamic init pointer.
 * @param[in] target_lval Target lvalue.
 * @param[out] out_rval Output rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_init(a_dynamic_init_ptr dip, struct gcc_jit_lvalue *target_lval, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)dip;
    (void)target_lval;
    if (out_rval) *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function declaration lowering in EH unit tests.
 * @details Sets `*out_func` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout The frontend routine pointer.
 * @param[out] out_func Pointer to receive the lowered `gcc_jit_function`.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_func = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for global variable declaration lowering in EH unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered `gcc_jit_lvalue`.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for local variable declaration lowering in EH unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func The active JIT function.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered `gcc_jit_lvalue`.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for invisible reference ABI check in EH unit tests.
 * @details Sets `*out_is_ref` to `FALSE` and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] type The frontend type pointer.
 * @param[out] out_is_ref Pointer to receive the boolean result.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (!out_is_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_ref = FALSE;
    return GCC_GEN_BE_SUCCESS;
}
}

/**
 * @brief Executes EH null-pointer validation and symbol declaration tests.
 * @details Verifies `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` on NULL arguments and `GCC_GEN_BE_SUCCESS` on valid context.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_eh_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    if (gcc_gen_be_begin_function_eh(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_end_function_eh(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_declare_eh_symbols(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_throw(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_try_block(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_routine_body(NULL, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_seh_unwind_tables(NULL, TRUE, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_declare_eh_symbols(ctx));

    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *seh_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "seh_table_test", 0, NULL, 0);
    gcc_jit_lvalue *xdata_except = NULL;
    gcc_jit_lvalue *xdata_finally = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_seh_unwind_tables(seh_fn, TRUE, &xdata_except));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_seh_unwind_tables(seh_fn, FALSE, &xdata_finally));
    if (!xdata_except || !xdata_finally) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    gcc_jit_context_release(ctx);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_eh.cpp`.
 * @details Runs `run_eh_unit_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_eh_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
