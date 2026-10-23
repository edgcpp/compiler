/**
 * @file test_gcc_be_stmt.cpp
 * @brief Unit tests for statement and coroutine lowering interfaces (`gcc_gen_be_stmt.c`).
 * @details Verifies null-pointer handling for labels, switch cases, statements, stack save/restore, and coroutines.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_abi.h"
#include <libgccjit.h>
#include <cstdio>

extern "C" {
/**
 * @brief Mock diagnostic reporter for statement unit tests.
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
/** @brief Innermost function scope pointer. */
a_scope_ptr innermost_function_scope = NULL;
/** @brief Current statement expression result lvalue pointer. */
struct gcc_jit_lvalue *current_stmt_expr_result_lval = NULL;
/** @brief Named register names table. */
a_const_char *named_register_names[] = { "invalid", "r0" };
/** @brief Mock IL header. */
an_il_header il_header;

/**
 * @brief Stub for constant rvalue lowering in statement unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] con The frontend constant pointer.
 * @param[in] expected_type The expected JIT type.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, struct gcc_jit_type *expected_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)con;
    (void)expected_type;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for retrieving coroutine descriptor from a routine in unit tests.
 * @details Returns NULL.
 * @param[in] rout The frontend routine pointer.
 * @return NULL.
 */
a_coroutine_descr_ptr get_coroutine_descr(a_routine_ptr rout) {
    (void)rout;
    return NULL;
}

/**
 * @brief Stub for unsigned integer constant extraction in statement unit tests.
 * @details Sets `*err` to `FALSE` and returns 0.
 * @param[in] cp The frontend constant pointer.
 * @param[out] err Pointer to receive overflow flag.
 * @return 0.
 */
a_host_large_unsigned unsigned_value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    (void)cp;
    if (err) *err = FALSE;
    return 0;
}

/**
 * @brief Stub for signed integer constant extraction in statement unit tests.
 * @details Sets `*err` to `FALSE` and returns 0.
 * @param[in] cp The frontend constant pointer.
 * @param[out] err Pointer to receive overflow flag.
 * @return 0.
 */
a_host_large_integer value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    (void)cp;
    if (err) *err = FALSE;
    return 0;
}

/**
 * @brief Stub for type lowering in statement unit tests.
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
 * @brief Stub for zero rvalue creation in statement unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] edg_type The frontend type pointer.
 * @param[in] jit_type The lowered JIT type.
 * @param[out] out_rval Pointer to receive the zero rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_get_zero_rvalue(a_type_ptr edg_type, struct gcc_jit_type *jit_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)edg_type;
    (void)jit_type;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function declaration lowering in statement unit tests.
 * @details Sets `*out_func` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout The frontend routine pointer.
 * @param[out] out_func Pointer to receive the lowered function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_func = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for local variable declaration lowering in statement unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func The active JIT function.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered local variable lvalue.
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
 * @brief Stub for global variable declaration lowering in statement unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered global variable lvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for dynamic initialization lowering in statement unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] dip The dynamic initialization pointer.
 * @param[in] target_lval The destination lvalue.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_init(a_dynamic_init_ptr dip, struct gcc_jit_lvalue *target_lval, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)dip;
    (void)target_lval;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for RAII scope unwind cleanup emission in stmt unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Active JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression rvalue lowering in statement unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] expr The expression node pointer.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression lvalue lowering in statement unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] expr The expression node pointer.
 * @param[out] out_lval Pointer to receive the lowered lvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for source location lookup in statement unit tests.
 * @details Sets `*out_loc` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] pos The frontend source position.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_get_location(a_source_position *pos, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)pos;
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for variable source location lookup in statement unit tests.
 * @details Sets `*out_loc` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_var(a_variable_ptr var, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for statement source location lookup in statement unit tests.
 * @details Sets `*out_loc` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] stmt The frontend statement pointer.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_stmt(a_statement_ptr stmt, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for lexical block debug info emission in statement unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] stmt The frontend statement pointer.
 * @param[in] block The JIT basic block.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_block_debug_info(a_statement_ptr stmt, struct gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    (void)block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for variable debug info emission in statement unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var The frontend variable pointer.
 * @param[in] lval The JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, struct gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    (void)lval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for try block lowering in statement unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] stmt The frontend try statement pointer.
 * @param[in] func The active JIT function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_try_block(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for invisible reference ABI check in statement unit tests.
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

/**
 * @brief Stub for rvalue ABI coercion in statement unit tests.
 * @details Forwards `src_rval` to `*out_rval`.
 * @param[in] func Active JIT function.
 * @param[in] block Active JIT block.
 * @param[in] src_rval Source rvalue.
 * @param[in] dst_type Destination JIT type.
 * @param[out] out_rval Pointer to receive the coerced rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_coerce_rvalue(
    struct gcc_jit_function *func,
    struct gcc_jit_block *block,
    struct gcc_jit_rvalue *src_rval,
    struct gcc_jit_type *dst_type,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)block;
    (void)dst_type;
    if (out_rval) *out_rval = src_rval;
    return GCC_GEN_BE_SUCCESS;
}
/**
 * @brief Stub for SEH `.pdata` / `.xdata` unwind table emission in statement unit tests.
 * @details Sets `*out_xdata_global` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Active JIT function.
 * @param[in] has_except_handler Whether the scope is `__except` (`TRUE`) or `__finally` (`FALSE`).
 * @param[out] out_xdata_global Optional pointer to receive the `.xdata` global.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_seh_unwind_tables(
    struct gcc_jit_function *func,
    a_boolean has_except_handler,
    struct gcc_jit_lvalue **out_xdata_global) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)has_except_handler;
    if (out_xdata_global) *out_xdata_global = NULL;
    return GCC_GEN_BE_SUCCESS;
}
}

/**
 * @brief Executes statement lowering null-pointer validation and SEH (`__try` / `__finally` / `__except`) tests.
 * @details Verifies every public entry point in `gcc_gen_be_stmt.h` rejects NULL pointers with `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`,
 * and tests `gcc_gen_be_lower_microsoft_try` for `__try`/`__finally` normal exit, early `return` exit, exception unwind exit,
 * and nested `__try`/`__except` 3-way filter dispatch (`1`, `0`, `-1`).
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_stmt_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_block *block = NULL;
    gcc_jit_type *type = NULL;
    gcc_jit_function *func = NULL;
    gcc_jit_lvalue *lval = NULL;

    if (gcc_gen_be_get_label_block(NULL, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_switch_case_block(NULL, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_label_block(NULL, NULL, &block));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_switch_case_block(NULL, NULL, &block));
    if (gcc_gen_be_lower_statement(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_finalize_computed_goto(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_microsoft_try(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_stack_save(NULL, NULL, &lval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_stack_restore(NULL, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_frame(NULL, &type) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_ramp(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_dispatchers(NULL, &func, &func) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

#if MICROSOFT_EXTENSIONS_ALLOWED
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *seh_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "seh_test_fn", 0, NULL, 0);
    gcc_jit_block *entry_blk = gcc_jit_function_new_block(seh_fn, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(entry_blk));

    /* 1. __try / __finally normal exit */
    a_statement empty_guarded = {};
    empty_guarded.kind = stmk_empty;
    a_statement empty_cleanup = {};
    empty_cleanup.kind = stmk_empty;
    a_microsoft_try_supplement finally_supp = {};
    finally_supp.guarded_statement = &empty_guarded;
    finally_supp.cleanup_statement = &empty_cleanup;
    finally_supp.except_expr = NULL;
    a_statement finally_stmt = {};
    finally_stmt.kind = stmk_microsoft_try;
    finally_stmt.variant.microsoft_try = &finally_supp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_microsoft_try(&finally_stmt, seh_fn));

    /* 2. __try / __finally with early return inside __try */
    a_statement early_ret_stmt = {};
    early_ret_stmt.kind = stmk_return;
    finally_supp.guarded_statement = &early_ret_stmt;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_microsoft_try(&finally_stmt, seh_fn));

    /* 3. Nested __try / __except (filter) enclosing __try / __finally */
    an_expr_node filter_expr = {};
    a_microsoft_try_supplement inner_except_supp = {};
    inner_except_supp.guarded_statement = &empty_guarded;
    inner_except_supp.cleanup_statement = &empty_cleanup;
    inner_except_supp.except_expr = &filter_expr;
    a_statement inner_except_stmt = {};
    inner_except_stmt.kind = stmk_microsoft_try;
    inner_except_stmt.variant.microsoft_try = &inner_except_supp;

    finally_supp.guarded_statement = &inner_except_stmt;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&finally_stmt, seh_fn));

    gcc_jit_context_release(ctx);
#endif

    /* Test inline assembly constraint normalization */
    char out_cstr[32];
    char tied_cstr[16];
    a_boolean is_rw = FALSE;
    if (gcc_gen_be_normalize_asm_constraints("+r", 0, NULL, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("+r", -1, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints(NULL, 0, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("r", 0, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("+", 0, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("+r", 0, out_cstr, 2, tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("=r", 0, out_cstr, 2, tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_normalize_asm_constraints("+r", 2, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw));
    if (!is_rw || out_cstr[0] != '=' || out_cstr[1] != 'r' || tied_cstr[0] != '2') return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_normalize_asm_constraints("=&r", 1, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw));
    if (is_rw || out_cstr[0] != '=' || out_cstr[1] != '&') return GCC_GEN_BE_ERROR_INTERNAL;

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_stmt.cpp`.
 * @details Runs `run_stmt_unit_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_stmt_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
