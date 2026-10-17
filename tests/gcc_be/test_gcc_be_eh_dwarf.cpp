/**
 * @file test_gcc_be_eh_dwarf.cpp
 * @brief Unit tests for DWARF zero-cost exception unwinding, LSDA tables, and landing pads.
 * @details Tests `gcc_gen_be_begin_function_eh`, `gcc_gen_be_end_function_eh`,
 * `gcc_gen_be_emit_unwind_resume`, `gcc_gen_be_register_personality_routine`,
 * `gcc_gen_be_emit_lsda_call_site_table`, `gcc_gen_be_enforce_exception_spec`,
 * `gcc_gen_be_probe_eh_builtins`, `gcc_gen_be_lower_try_block`, and `gcc_gen_be_lower_routine_body`.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_decl.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for DWARF EH tests.
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
/** @brief GCC compatibility mode flag. */
a_boolean gcc_mode = TRUE;
/** @brief Mock IL header. */
an_il_header il_header;

/**
 * @brief Stub for computing non-typeref type size in DWARF EH unit tests.
 * @details Returns `tp->size` if `tp` is non-NULL, or 0 otherwise.
 * @param[in] tp The frontend type pointer.
 * @return The size of `tp` in bytes.
 */
a_targ_size_t f_size_of_type(a_type_ptr tp) {
    return tp ? tp->size : 0;
}

/**
 * @brief Stub for extracting signed integer value from a constant in DWARF EH unit tests.
 * @details Sets `*err` to `FALSE` and returns 0.
 * @param[in] cp The frontend constant pointer.
 * @param[out] err Pointer to receive overflow flag.
 * @return Extracted signed integer value.
 */
a_host_large_integer value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    (void)cp;
    if (err) *err = FALSE;
    return 0;
}

/**
 * @brief Stub for local variable declaration lowering in DWARF EH unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func The active JIT function.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered local variable lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    if (func) {
        gcc_jit_context *ctx = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
        if (ctx) {
            *out_lval = gcc_jit_function_new_local(func, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "catch_param_stub");
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for type lowering in DWARF EH unit tests.
 * @details Lowers any type to `int` for testing purposes.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_type Pointer to receive the lowered `gcc_jit_type`.
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

/**
 * @brief Stub for invisible reference check in DWARF EH unit tests.
 * @details Always sets `*out_is_invis_ref` to `FALSE`.
 * @param[in] type The frontend type pointer.
 * @param[out] out_is_invis_ref Pointer to receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_invis_ref) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (!out_is_invis_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_invis_ref = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for global variable declaration lowering in DWARF EH unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for variable lowering in DWARF EH unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_variable(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function declaration lowering in DWARF EH unit tests.
 * @details Sets `*out_func` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout The frontend routine pointer.
 * @param[out] out_func Pointer to receive the lowered function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_func = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for dynamic initialization lowering in DWARF EH unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] dip The dynamic initialization pointer.
 * @param[in] dest_lval The destination lvalue.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_init(a_dynamic_init_ptr dip, struct gcc_jit_lvalue *dest_lval, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)dip;
    (void)dest_lval;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression rvalue lowering in DWARF EH unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] expr The expression node pointer.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression lvalue lowering in DWARF EH unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] expr The expression node pointer.
 * @param[out] out_lval Pointer to receive the lowered lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for statement lowering in DWARF EH unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] stmt The statement pointer.
 * @param[in] func The active JIT function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}
}

/**
 * @brief Runs all DWARF EH unit tests.
 * @details Exercises builtin probing, personality registration, unwind resume, LSDA tables,
 * exception specification enforcement, and bare rethrow lowering.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_eh_dwarf_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_declare_eh_symbols(ctx));

    a_boolean has_builtins = FALSE;
    if (gcc_gen_be_probe_eh_builtins(NULL, &has_builtins) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_probe_eh_builtins(ctx, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_eh_builtins(ctx, &has_builtins));

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, "eh_test_fn", 0, NULL, 0);

    gcc_jit_function *pers_fn = NULL;
    if (gcc_gen_be_register_personality_routine(NULL, &pers_fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_register_personality_routine(fn, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_register_personality_routine(fn, &pers_fn));
    if (!pers_fn) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_block *entry = gcc_jit_function_new_block(fn, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(entry));
    GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(fn));

    gcc_jit_rvalue *null_exc = gcc_jit_context_null(ctx, void_ptr_type);
    gcc_jit_block *after_resume = NULL;
    if (gcc_gen_be_emit_unwind_resume(NULL, &after_resume) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_unwind_resume(null_exc, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(null_exc, &after_resume));
    if (!after_resume) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(fn));

    /* Test nested try block with typed catch + catch(...) action chain and @TType encoding */
    a_type int_catch_type;
    std::memset(&int_catch_type, 0, sizeof(int_catch_type));
    int_catch_type.kind = tk_integer;
    int_catch_type.size = 4;

    a_variable catch_param;
    std::memset(&catch_param, 0, sizeof(catch_param));
    catch_param.type = &int_catch_type;

    a_statement handler1_body, handler2_body, try_inner_body;
    std::memset(&handler1_body, 0, sizeof(handler1_body));
    std::memset(&handler2_body, 0, sizeof(handler2_body));
    std::memset(&try_inner_body, 0, sizeof(try_inner_body));
    handler1_body.kind = stmk_empty;
    handler2_body.kind = stmk_empty;
    try_inner_body.kind = stmk_empty;

    a_handler h1, h2;
    std::memset(&h1, 0, sizeof(h1));
    std::memset(&h2, 0, sizeof(h2));
    h1.parameter = &catch_param;
    h1.statement = &handler1_body;
    h1.next = &h2;
    h2.parameter = NULL; /* catch (...) */
    h2.statement = &handler2_body;

    a_try_supplement try_supp;
    std::memset(&try_supp, 0, sizeof(try_supp));
    try_supp.statement = &try_inner_body;
    try_supp.handlers = &h1;

    a_statement try_stmt;
    std::memset(&try_stmt, 0, sizeof(try_stmt));
    try_stmt.kind = stmk_try_block;
    try_stmt.variant.try_block = &try_supp;

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&try_stmt, fn));

    gcc_jit_lvalue *lsda_global = NULL;
    if (gcc_gen_be_emit_lsda_call_site_table(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_lsda_call_site_table(NULL, &lsda_global));
    if (!lsda_global) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test PIC/PIE and shared LSDA @TType encodings */
    gcc_be_fPIC = TRUE;
    gcc_jit_lvalue *lsda_pic = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_lsda_call_site_table(NULL, &lsda_pic));
    if (!lsda_pic) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_be_fPIC = FALSE;
    gcc_be_shared = TRUE;
    gcc_jit_lvalue *lsda_shared = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_lsda_call_site_table(NULL, &lsda_shared));
    if (!lsda_shared) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_be_shared = FALSE;

    /* Test gcc_gen_be_emit_invoke_call and gcc_gen_be_emit_invoke_call_through_ptr */
    gcc_jit_block *inv_blk = gcc_jit_function_new_block(fn, "inv_blk");
    gcc_jit_block *inv_lp = gcc_jit_function_new_block(fn, "inv_lp");
    gcc_jit_block *inv_next = NULL;
    if (gcc_gen_be_emit_invoke_call(NULL, inv_blk, pers_fn, 0, NULL, NULL, NULL, inv_lp, &inv_next) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    if (gcc_gen_be_emit_invoke_call_through_ptr(NULL, inv_blk, null_exc, 0, NULL, NULL, NULL, inv_lp, &inv_next) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    gcc_jit_function *callee_void = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "callee_void", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call(ctx, inv_blk, callee_void, 0, NULL, NULL, NULL, inv_lp, &inv_next));
    if (!inv_next) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_type *callee_ptr_t = gcc_jit_context_new_function_ptr_type(ctx, NULL, void_type, 0, NULL, 0);
    gcc_jit_rvalue *callee_ptr = gcc_jit_context_null(ctx, callee_ptr_t);
    gcc_jit_block *inv_ptr_next = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call_through_ptr(ctx, inv_next, callee_ptr, 0, NULL, NULL, NULL, inv_lp, &inv_ptr_next));
    if (!inv_ptr_next) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test invoke with non-NULL result_lval and optional p_gcc_jit_block_end_with_invoke hook */
    gcc_jit_type *int_ret_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_function *callee_int = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_ret_t, "callee_int", 0, NULL, 0);
    gcc_jit_type *callee_int_ptr_t = gcc_jit_context_new_function_ptr_type(ctx, NULL, int_ret_t, 0, NULL, 0);
    gcc_jit_rvalue *callee_int_ptr = gcc_jit_context_null(ctx, callee_int_ptr_t);
    gcc_jit_lvalue *inv_res_lval = gcc_jit_function_new_local(fn, NULL, int_ret_t, "inv_res_lval");
    p_gcc_jit_block_end_with_invoke = [](struct gcc_jit_block *b, struct gcc_jit_location *l, struct gcc_jit_rvalue *c, struct gcc_jit_block *nd, struct gcc_jit_block *ud) {
        (void)l; (void)c; (void)ud;
        gcc_jit_block_end_with_jump(b, NULL, nd);
    };
    p_gcc_jit_block_add_try_catch = [](struct gcc_jit_block *tb, struct gcc_jit_location *l, struct gcc_jit_block *cb) {
        (void)tb; (void)l; (void)cb;
    };
    p_gcc_jit_block_add_cleanup = [](struct gcc_jit_block *b, struct gcc_jit_location *l, struct gcc_jit_block *cb) {
        (void)b; (void)l; (void)cb;
    };
    p_gcc_jit_context_add_top_level_asm = [](struct gcc_jit_context *c, struct gcc_jit_location *l, const char *s) {
        (void)c; (void)l; (void)s;
    };
    gcc_jit_block *inv_hook_next1 = NULL;
    gcc_jit_block *inv_hook_next2 = NULL;
    gcc_jit_block *inv_hook_next3 = NULL;
    gcc_jit_block *inv_hook_next4 = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call(ctx, inv_ptr_next, callee_void, 0, NULL, NULL, NULL, inv_lp, &inv_hook_next1));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call(ctx, inv_hook_next1, callee_int, 0, NULL, inv_res_lval, NULL, inv_lp, &inv_hook_next2));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call_through_ptr(ctx, inv_hook_next2, callee_ptr, 0, NULL, NULL, NULL, inv_lp, &inv_hook_next3));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call_through_ptr(ctx, inv_hook_next3, callee_int_ptr, 0, NULL, inv_res_lval, NULL, inv_lp, &inv_hook_next4));
    p_gcc_jit_block_end_with_invoke = NULL;

    an_exception_specification spec;
    std::memset(&spec, 0, sizeof(spec));
    spec.is_noexcept = TRUE;
    spec.throw_any = FALSE;

    gcc_jit_block *lp_noexcept = gcc_jit_function_new_block(fn, "lp_noexcept");
    gcc_jit_block *end_noexcept = gcc_jit_function_new_block(fn, "end_noexcept");
    if (gcc_gen_be_emit_noexcept_filter(NULL, lp_noexcept, end_noexcept) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_noexcept_filter(fn, NULL, end_noexcept) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_noexcept_filter(fn, lp_noexcept, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_enforce_exception_spec(NULL, &spec, lp_noexcept, end_noexcept) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_enforce_exception_spec(fn, &spec, lp_noexcept, end_noexcept));

    gcc_jit_block *outer_lp = gcc_jit_function_new_block(fn, "outer_lp");
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(outer_lp));

    an_expr_node rethrow_expr;
    std::memset(&rethrow_expr, 0, sizeof(rethrow_expr));
    rethrow_expr.kind = enk_throw;
    rethrow_expr.variant.throw_info = NULL;

    gcc_jit_block *after_throw = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&rethrow_expr, &after_throw));
    if (!after_throw) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test constructor-initialized throw object with __cxa_free_exception cleanup LP */
    a_type exc_obj_type;
    std::memset(&exc_obj_type, 0, sizeof(exc_obj_type));
    exc_obj_type.kind = tk_integer;
    exc_obj_type.size = 4;

    a_dynamic_init ctor_dip;
    std::memset(&ctor_dip, 0, sizeof(ctor_dip));
    ctor_dip.kind = dik_constructor;

    a_throw_supplement throw_supp;
    std::memset(&throw_supp, 0, sizeof(throw_supp));
    throw_supp.type = &exc_obj_type;
    throw_supp.dynamic_init = &ctor_dip;

    an_expr_node ctor_throw_expr;
    std::memset(&ctor_throw_expr, 0, sizeof(ctor_throw_expr));
    ctor_throw_expr.kind = enk_throw;
    ctor_throw_expr.variant.throw_info = &throw_supp;

    gcc_jit_block *after_ctor_throw = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ctor_throw_expr, &after_ctor_throw));
    if (!after_ctor_throw) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test ULEB128 and SLEB128 encoders */
    unsigned char leb_buf[16];
    size_t leb_len = 0;
    if (gcc_gen_be_encode_uleb128(0, NULL, sizeof(leb_buf), &leb_len) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_encode_uleb128(0, leb_buf, sizeof(leb_buf), NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_encode_uleb128(300, leb_buf, 1, &leb_len) != GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_uleb128(0, leb_buf, sizeof(leb_buf), &leb_len));
    if (leb_len != 1 || leb_buf[0] != 0x00) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_uleb128(624485UL, leb_buf, sizeof(leb_buf), &leb_len));
    if (leb_len != 3 || leb_buf[0] != 0xe5 || leb_buf[1] != 0x8e || leb_buf[2] != 0x26) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_encode_sleb128(0, NULL, sizeof(leb_buf), &leb_len) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_encode_sleb128(0, leb_buf, sizeof(leb_buf), NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_encode_sleb128(300, leb_buf, 1, &leb_len) != GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_sleb128(0, leb_buf, sizeof(leb_buf), &leb_len));
    if (leb_len != 1 || leb_buf[0] != 0x00) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_sleb128(-1, leb_buf, sizeof(leb_buf), &leb_len));
    if (leb_len != 1 || leb_buf[0] != 0x7f) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_sleb128(-123456L, leb_buf, sizeof(leb_buf), &leb_len));
    if (leb_len != 3 || leb_buf[0] != 0xc0 || leb_buf[1] != 0xbb || leb_buf[2] != 0x78) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_sleb128(128L, leb_buf, sizeof(leb_buf), &leb_len));
    if (leb_len != 2 || leb_buf[0] != 0x80 || leb_buf[1] != 0x01) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test native EH probing */
    a_boolean native_eh_supp = FALSE;
    if (gcc_gen_be_probe_native_eh_support(NULL, &native_eh_supp) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_probe_native_eh_support(ctx, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_native_eh_support(ctx, &native_eh_supp));

    a_boolean has_inv = FALSE;
    a_boolean has_lp = FALSE;
    if (gcc_gen_be_resolve_eh_entrypoints(NULL, &has_inv, &has_lp) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_resolve_eh_entrypoints(ctx, &has_inv, &has_lp));

    /* Test LSDA directive formatter, chained cleanup actions, and ARM EHABI unwind tables */
    char dir_buf[512];
    size_t dir_len = 0;
    if (gcc_gen_be_format_lsda_asm_directives(1, 0, 0, 1, NULL, sizeof(dir_buf), &dir_len) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_lsda_asm_directives(1, 0, 0, 1, dir_buf, sizeof(dir_buf), NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_lsda_asm_directives(-1, 0, 0, 1, dir_buf, sizeof(dir_buf), &dir_len) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_lsda_asm_directives(1, 0, 0, 1, dir_buf, 8, &dir_len) != GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_format_lsda_asm_directives(1, 0, 0, 1, dir_buf, sizeof(dir_buf), &dir_len));
    if (dir_len == 0 || std::strstr(dir_buf, ".cfi_lsda") == NULL || std::strstr(dir_buf, ".uleb128") == NULL) return GCC_GEN_BE_ERROR_INTERNAL;

    int chained_idx = 0;
    if (gcc_gen_be_chain_cleanup_action(0, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_chain_cleanup_action(0, &chained_idx));
    if (chained_idx <= 0) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_chain_cleanup_action(chained_idx, &chained_idx));
    if (chained_idx <= 0) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Emit scope unwind cleanup while an active catch region is at the head of eh_state.regions */
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(fn));

    gcc_jit_lvalue *arm_extab_pr0 = NULL;
    gcc_jit_lvalue *arm_extab_pr1 = NULL;
    if (gcc_gen_be_emit_arm_ehabi_tables(NULL, TRUE, &arm_extab_pr0) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_arm_ehabi_tables(fn, TRUE, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_arm_ehabi_tables(fn, TRUE, &arm_extab_pr0));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_arm_ehabi_tables(fn, FALSE, &arm_extab_pr1));
    if (!arm_extab_pr0 || !arm_extab_pr1) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test multi-scope Windows x64 SEH .pdata / .xdata emission with active eh_state.regions */
    gcc_jit_lvalue *seh_xdata_multi_exc = NULL;
    gcc_jit_lvalue *seh_xdata_multi_fin = NULL;
    if (gcc_gen_be_emit_seh_unwind_tables(NULL, TRUE, &seh_xdata_multi_exc) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_seh_unwind_tables(fn, TRUE, &seh_xdata_multi_exc));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_seh_unwind_tables(fn, FALSE, &seh_xdata_multi_fin));
    if (!seh_xdata_multi_exc || !seh_xdata_multi_fin) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test selector filter dispatch */
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_rvalue *sel_rval = gcc_jit_context_new_rvalue_from_int(ctx, int_t, 1);
    gcc_jit_block *sel_disp = gcc_jit_function_new_block(fn, "sel_disp");
    gcc_jit_block *sel_match = gcc_jit_function_new_block(fn, "sel_match");
    gcc_jit_block *sel_next1 = NULL;
    gcc_jit_block *sel_next2 = NULL;
    gcc_jit_block *sel_next3 = NULL;
    gcc_jit_block *sel_next4 = NULL;
    if (gcc_gen_be_emit_selector_filter_dispatch(NULL, sel_disp, sel_rval, 1, sel_match, &sel_next1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_selector_filter_dispatch(fn, sel_disp, sel_rval, 1, sel_match, &sel_next1));
    if (!sel_next1) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_selector_filter_dispatch(fn, sel_next1, sel_rval, -1, sel_match, &sel_next2));
    if (!sel_next2) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_selector_filter_dispatch(fn, sel_next2, sel_rval, 0, sel_match, &sel_next3));
    if (!sel_next3) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_selector_filter_dispatch(fn, sel_next3, null_exc, 1, sel_match, &sel_next4));
    if (!sel_next4) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test partial array construction cleanup and subobject unwind cleanup */
    gcc_jit_type *size_t_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_lvalue *arr_base_lval = gcc_jit_function_new_local(fn, NULL, void_ptr_type, "arr_base");
    gcc_jit_lvalue *arr_idx_lval = gcc_jit_function_new_local(fn, NULL, size_t_t, "arr_idx");
    a_routine elem_dtor_rout;
    std::memset(&elem_dtor_rout, 0, sizeof(elem_dtor_rout));
    elem_dtor_rout.source_corresp.name = "_ZN4ElemD1Ev";
    gcc_jit_block *arr_cleanup_lp = NULL;
    if (gcc_gen_be_emit_partial_array_cleanup(NULL, arr_base_lval, arr_idx_lval, &exc_obj_type, &elem_dtor_rout, &arr_cleanup_lp) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_partial_array_cleanup(fn, arr_base_lval, arr_idx_lval, &exc_obj_type, &elem_dtor_rout, &arr_cleanup_lp));
    if (!arr_cleanup_lp) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_block *subobj_lp0 = NULL;
    gcc_jit_block *subobj_lp8 = NULL;
    if (gcc_gen_be_emit_subobject_unwind_cleanup(NULL, null_exc, 0, &elem_dtor_rout, &subobj_lp0) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subobject_unwind_cleanup(fn, null_exc, 0, &elem_dtor_rout, &subobj_lp0));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subobject_unwind_cleanup(fn, null_exc, 8, &elem_dtor_rout, &subobj_lp8));
    if (!subobj_lp0 || !subobj_lp8) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    gcc_jit_block *subobj_lp_no_outer = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subobject_unwind_cleanup(fn, null_exc, 0, &elem_dtor_rout, &subobj_lp_no_outer));
    if (!subobj_lp_no_outer) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&try_stmt, fn));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(fn));
    gcc_jit_lvalue *lsda_with_asm = NULL;
    gcc_jit_lvalue *seh_with_asm = NULL;
    gcc_jit_lvalue *arm_with_asm = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_lsda_call_site_table(NULL, &lsda_with_asm));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_seh_unwind_tables(fn, TRUE, &seh_with_asm));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_arm_ehabi_tables(fn, TRUE, &arm_with_asm));
    p_gcc_jit_block_add_try_catch = NULL;
    p_gcc_jit_block_add_cleanup = NULL;
    p_gcc_jit_context_add_top_level_asm = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(fn));

    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_jit_context_release(ctx);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for the DWARF EH unit test executable.
 * @details Runs `run_eh_dwarf_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_eh_dwarf_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}


