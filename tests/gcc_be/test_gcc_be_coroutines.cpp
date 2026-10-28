/**
 * @file test_gcc_be_coroutines.cpp
 * @brief Unit tests for C++20 coroutine lowering in the GCC backend.
 * @details Tests `gcc_gen_be_compute_coro_frame_layout`, `gcc_gen_be_lower_coroutine_frame`,
 * `gcc_gen_be_lower_coroutine_ramp`, `gcc_gen_be_lower_coroutine_dispatchers`,
 * `gcc_gen_be_register_coro_suspend_point`, `gcc_gen_be_lower_await_yield`, and null-pointer fault-injection paths.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_decl.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for coroutine unit tests.
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
/** @brief Active test coroutine descriptor returned by `get_coroutine_descr`. */
static a_coroutine_descr_ptr active_test_coro_descr = NULL;
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
 * @brief Stub for constant rvalue lowering in coroutine unit tests.
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
 * @details Returns NULL unless the routine has an attached scope with coroutine_descr.
 * @param[in] rout The frontend routine pointer.
 * @return Coroutine descriptor pointer or NULL.
 */
a_coroutine_descr_ptr get_coroutine_descr(a_routine_ptr rout) {
    (void)rout;
    return active_test_coro_descr;
}

/**
 * @brief Stub for unsigned integer constant extraction in coroutine unit tests.
 * @details Sets `*err` to `FALSE` and returns 0.
 * @param[in] cp The frontend constant pointer.
 * @param[out] err Pointer to receive overflow flag.
 * @return Extracted unsigned integer value.
 */
a_host_large_unsigned unsigned_value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    (void)cp;
    if (err) *err = FALSE;
    return 0;
}

/**
 * @brief Stub for signed integer constant extraction in coroutine unit tests.
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
 * @brief Stub for type lowering in coroutine unit tests.
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
 * @brief Stub for zero rvalue creation in coroutine unit tests.
 * @details Creates a zero int rvalue for testing purposes.
 * @param[in] edg_type The frontend type pointer.
 * @param[in] jit_type The lowered JIT type.
 * @param[out] out_rval Pointer to receive the zero rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_zero_rvalue(a_type_ptr edg_type, struct gcc_jit_type *jit_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)edg_type;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    *out_rval = gcc_jit_context_zero(ctx, jit_type ? jit_type : gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function declaration lowering in coroutine unit tests.
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
 * @brief Stub for local variable declaration lowering in coroutine unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func The active JIT function.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered local variable lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for global variable declaration lowering in coroutine unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered global variable lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for dynamic initialization lowering in coroutine unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] dip The dynamic initialization pointer.
 * @param[in] target_lval The destination lvalue.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_init(a_dynamic_init_ptr dip, struct gcc_jit_lvalue *target_lval, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)dip;
    (void)target_lval;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression rvalue lowering in coroutine unit tests.
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
 * @brief Stub for expression lvalue lowering in coroutine unit tests.
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
 * @brief Stub for source location lookup in coroutine unit tests.
 * @details Sets `*out_loc` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] pos The frontend source position.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location(a_source_position *pos, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)pos;
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for variable source location lookup in coroutine unit tests.
 * @details Sets `*out_loc` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_var(a_variable_ptr var, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for statement source location lookup in coroutine unit tests.
 * @details Sets `*out_loc` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] stmt The frontend statement pointer.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_stmt(a_statement_ptr stmt, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for lexical block debug info emission in coroutine unit tests.
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
 * @brief Stub for variable debug info emission in coroutine unit tests.
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
 * @brief Stub for try block lowering in coroutine unit tests.
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
 * @brief Stub for scope unwind cleanup in coroutine unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func The active JIT function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for SEH `.pdata` / `.xdata` unwind table emission in coroutine unit tests.
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

/**
 * @brief Stub for dynamic_cast lowering in coroutine unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] expr The frontend expression pointer.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_cast(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for typeid lowering in coroutine unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] expr The frontend expression pointer.
 * @param[out] out_lval Pointer to receive the lowered lvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_typeid(an_expr_node_ptr expr, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for builtin call lowering in coroutine unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout The builtin routine pointer.
 * @param[in] num_args Number of arguments.
 * @param[in] args Argument rvalues array.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_builtin_call(a_routine_ptr rout, int num_args, struct gcc_jit_rvalue **args, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    (void)num_args;
    (void)args;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for rvalue ABI coercion in coroutine unit tests.
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
}

/**
 * @brief Runs all C++20 coroutine lowering unit tests.
 * @details Exercises `gcc_gen_be_compute_coro_frame_layout`, `gcc_gen_be_lower_coroutine_frame`,
 * `gcc_gen_be_lower_coroutine_ramp`, `gcc_gen_be_lower_coroutine_dispatchers`, and `gcc_gen_be_register_coro_suspend_point`.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_coroutine_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_type *frame_type = NULL;
    gcc_jit_struct *frame_struct = NULL;
    size_t frame_size = 0;
    gcc_jit_function *resume_fn = NULL;
    gcc_jit_function *destroy_fn = NULL;

    if (gcc_gen_be_compute_coro_frame_layout(NULL, NULL, &frame_struct, &frame_size) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_frame(NULL, &frame_type) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_frame((a_routine_ptr)0x1234, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_ramp(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_dispatchers(NULL, &resume_fn, &destroy_fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_dispatchers((a_routine_ptr)0x1234, NULL, &destroy_fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_dispatchers((a_routine_ptr)0x1234, &resume_fn, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(cache_clear_all());

    a_routine rout;
    std::memset(&rout, 0, sizeof(rout));
    a_scope scope;
    std::memset(&scope, 0, sizeof(scope));
    a_type int_tp;
    std::memset(&int_tp, 0, sizeof(int_tp));
    int_tp.kind = tk_integer;
    int_tp.size = 4;

    a_variable local_var;
    std::memset(&local_var, 0, sizeof(local_var));
    local_var.type = &int_tp;
    scope.nonstatic_variables = &local_var;

    a_variable lt_temp_var;
    std::memset(&lt_temp_var, 0, sizeof(lt_temp_var));
    lt_temp_var.type = &int_tp;
    a_dynamic_init lt_dtor;
    std::memset(&lt_dtor, 0, sizeof(lt_dtor));
    lt_dtor.variable = &lt_temp_var;
    an_object_lifetime scope_lt;
    std::memset(&scope_lt, 0, sizeof(scope_lt));
    scope_lt.destructions = &lt_dtor;
    scope.lifetime = &scope_lt;

    an_expr_node inner_temp_expr, outer_op_expr, await_temp_expr, await_node_expr;
    std::memset(&inner_temp_expr, 0, sizeof(inner_temp_expr));
    std::memset(&outer_op_expr, 0, sizeof(outer_op_expr));
    std::memset(&await_temp_expr, 0, sizeof(await_temp_expr));
    std::memset(&await_node_expr, 0, sizeof(await_node_expr));
    inner_temp_expr.kind = enk_temp_init;
    inner_temp_expr.type = &int_tp;
    await_temp_expr.kind = enk_temp_init;
    await_temp_expr.type = &int_tp;
    await_node_expr.kind = enk_await;
    await_node_expr.type = &int_tp;
    await_node_expr.variant.await_info.operand = &await_temp_expr;
    outer_op_expr.kind = enk_operation;
    outer_op_expr.type = &int_tp;
    outer_op_expr.variant.operation.operands = &inner_temp_expr;
    inner_temp_expr.next = &await_node_expr;

    /* Build nested statements (if, constexpr_if, while, for, range_based_for, switch, try_block, init) */
    a_statement expr_stmt, if_stmt, ce_if_stmt, while_stmt, for_stmt, rfor_stmt, sw_stmt, try_stmt, init_stmt, blk_stmt;
    std::memset(&expr_stmt, 0, sizeof(expr_stmt));
    std::memset(&if_stmt, 0, sizeof(if_stmt));
    std::memset(&ce_if_stmt, 0, sizeof(ce_if_stmt));
    std::memset(&while_stmt, 0, sizeof(while_stmt));
    std::memset(&for_stmt, 0, sizeof(for_stmt));
    std::memset(&rfor_stmt, 0, sizeof(rfor_stmt));
    std::memset(&sw_stmt, 0, sizeof(sw_stmt));
    std::memset(&try_stmt, 0, sizeof(try_stmt));
    std::memset(&init_stmt, 0, sizeof(init_stmt));
    std::memset(&blk_stmt, 0, sizeof(blk_stmt));

    expr_stmt.kind = stmk_expr;
    expr_stmt.expr = &outer_op_expr;

    if_stmt.kind = stmk_if;
    if_stmt.variant.if_stmt.then_statement = &expr_stmt;

    a_constexpr_if ce_info;
    std::memset(&ce_info, 0, sizeof(ce_info));
    ce_info.then_statement = &if_stmt;
    ce_if_stmt.kind = stmk_constexpr_if;
    ce_if_stmt.variant.constexpr_if = &ce_info;

    while_stmt.kind = stmk_while;
    while_stmt.variant.loop_statement = &ce_if_stmt;

    a_for_loop fl_info;
    std::memset(&fl_info, 0, sizeof(fl_info));
    fl_info.initialization = &expr_stmt;
    fl_info.increment = &outer_op_expr;
    for_stmt.kind = stmk_for;
    for_stmt.variant.for_loop.extra_info = &fl_info;
    for_stmt.variant.for_loop.statement = &while_stmt;

    rfor_stmt.kind = stmk_range_based_for;
    rfor_stmt.variant.range_based_for_loop.statement = &for_stmt;

    sw_stmt.kind = stmk_switch;
    sw_stmt.variant.switch_stmt.body_statement = &rfor_stmt;

    a_handler catch_h;
    std::memset(&catch_h, 0, sizeof(catch_h));
    catch_h.statement = &expr_stmt;
    a_try_supplement tbs;
    std::memset(&tbs, 0, sizeof(tbs));
    tbs.statement = &sw_stmt;
    tbs.handlers = &catch_h;
    try_stmt.kind = stmk_try_block;
    try_stmt.variant.try_block = &tbs;

    a_dynamic_init d_init;
    std::memset(&d_init, 0, sizeof(d_init));
    d_init.kind = dik_expression;
    d_init.variant.expression = &outer_op_expr;
    init_stmt.kind = stmk_init;
    init_stmt.variant.dynamic_init = &d_init;
    try_stmt.next = &init_stmt;

    blk_stmt.kind = stmk_block;
    blk_stmt.variant.block.statements = &try_stmt;
    scope.assoc_block = &blk_stmt;

    size_t exact_sz = 0;
    size_t exact_align = 0;
    if (gcc_gen_be_compute_coro_frame_size_and_align(NULL, &scope, &exact_sz, &exact_align) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_compute_coro_frame_size_and_align(&rout, &scope, NULL, &exact_align) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_compute_coro_frame_size_and_align(&rout, &scope, &exact_sz, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_size_and_align(&rout, &scope, &exact_sz, &exact_align));
    if (exact_sz < 64U || exact_align < sizeof(void *)) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&rout, &scope, &frame_struct, &frame_size));
    if (!frame_struct || frame_size == 0) return GCC_GEN_BE_ERROR_INTERNAL;
    void *spilled_temp_fld = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, &inner_temp_expr, &spilled_temp_fld));
    if (!spilled_temp_fld) return GCC_GEN_BE_ERROR_INTERNAL;
    void *spilled_await_fld = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, &await_temp_expr, &spilled_await_fld));
    if (!spilled_await_fld) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_frame(&rout, &frame_type));
    if (!frame_type) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *ramp_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, "coro_ramp_test", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&rout, ramp_fn));

    /* Test HALO stack elision (opt_level >= 2, no custom new_routine/alloc_failure_gro_call)
       and unhandled_exception / final_suspend / multi-step suspend point registration */
    a_coroutine_descr full_descr;
    std::memset(&full_descr, 0, sizeof(full_descr));
    a_variable prom_var, init_await_var, handle_var;
    std::memset(&prom_var, 0, sizeof(prom_var));
    std::memset(&init_await_var, 0, sizeof(init_await_var));
    std::memset(&handle_var, 0, sizeof(handle_var));
    prom_var.type = &int_tp;
    init_await_var.type = &int_tp;
    handle_var.type = &int_tp;
    an_expr_node ue_expr, fs_expr, is_expr, gro_expr;
    std::memset(&ue_expr, 0, sizeof(ue_expr));
    std::memset(&fs_expr, 0, sizeof(fs_expr));
    std::memset(&is_expr, 0, sizeof(is_expr));
    std::memset(&gro_expr, 0, sizeof(gro_expr));
    full_descr.promise = &prom_var;
    full_descr.init_await_resume = &init_await_var;
    full_descr.handle = &handle_var;
    full_descr.unhandled_exception_call = &ue_expr;
    full_descr.final_suspend_call = &fs_expr;
    full_descr.initial_suspend_call = &is_expr;
    full_descr.get_return_object_call = &gro_expr;

    active_test_coro_descr = &full_descr;
    gcc_be_opt_level = 2;
    gcc_jit_function *halo_ramp_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, "coro_halo_ramp_test", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&rout, halo_ramp_fn));
    gcc_be_opt_level = 0;
    active_test_coro_descr = NULL;

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_dispatchers(&rout, &resume_fn, &destroy_fn));
    if (!resume_fn || !destroy_fn) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test over-aligned coroutine frame allocation and deallocation (align > 16) */
    a_type overaligned_tp;
    std::memset(&overaligned_tp, 0, sizeof(overaligned_tp));
    overaligned_tp.kind = tk_struct;
    overaligned_tp.size = 64;
    overaligned_tp.alignment = 64;
    a_variable overaligned_prom;
    std::memset(&overaligned_prom, 0, sizeof(overaligned_prom));
    overaligned_prom.type = &overaligned_tp;
    a_coroutine_descr overaligned_descr;
    std::memset(&overaligned_descr, 0, sizeof(overaligned_descr));
    overaligned_descr.promise = &overaligned_prom;
    active_test_coro_descr = &overaligned_descr;

    a_routine overaligned_rout;
    std::memset(&overaligned_rout, 0, sizeof(overaligned_rout));
    gcc_jit_function *oa_ramp = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, "coro_oa_ramp", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&overaligned_rout, oa_ramp));
    active_test_coro_descr = NULL;

    /* Test symmetric transfer helper */
    gcc_jit_function *sym_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, "coro_sym_transfer_test", 0, NULL, 0);
    gcc_jit_block *sym_blk_voidp = gcc_jit_function_new_block(sym_fn, "sym_voidp");
    gcc_jit_block *sym_blk_intp = gcc_jit_function_new_block(sym_fn, "sym_intp");
    gcc_jit_block *sym_blk_val = gcc_jit_function_new_block(sym_fn, "sym_val");
    gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *int_ptr_t = gcc_jit_type_get_pointer(int_t);
    gcc_jit_rvalue *null_vp = gcc_jit_context_null(ctx, void_ptr_t);
    gcc_jit_rvalue *null_ip = gcc_jit_context_null(ctx, int_ptr_t);
    gcc_jit_rvalue *zero_int = gcc_jit_context_zero(ctx, int_t);

    if (gcc_gen_be_emit_coro_symmetric_transfer(NULL, sym_blk_voidp, null_vp) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_coro_symmetric_transfer(sym_fn, NULL, null_vp) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_coro_symmetric_transfer(sym_fn, sym_blk_voidp, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_coro_symmetric_transfer(sym_fn, sym_blk_voidp, null_vp));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_coro_symmetric_transfer(sym_fn, sym_blk_intp, null_ip));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_coro_symmetric_transfer(sym_fn, sym_blk_val, zero_int));

    gcc_jit_type *active_ft = NULL;
    gcc_jit_lvalue *active_fl = NULL;
    if (gcc_gen_be_get_active_coroutine_frame(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_active_coroutine_frame(&active_ft, &active_fl));

    int state_idx = 0;
    gcc_jit_lvalue *state_lval = NULL;
    gcc_jit_rvalue *frame_addr = NULL;
    if (gcc_gen_be_register_coro_suspend_point(NULL, &state_idx, &state_lval, &frame_addr) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    /* Test coroutine HALO eligibility and noop_coroutine singleton */
    a_boolean can_elide = FALSE;
    if (gcc_gen_be_try_coro_halo(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_coro_halo(NULL, &can_elide));
    if (can_elide != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;

    a_coroutine_descr halo_descr;
    std::memset(&halo_descr, 0, sizeof(halo_descr));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_coro_halo(&halo_descr, &can_elide));
    if (can_elide != TRUE) return GCC_GEN_BE_ERROR_INTERNAL;

    halo_descr.new_routine = &rout;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_coro_halo(&halo_descr, &can_elide));
    if (can_elide != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;

    halo_descr.new_routine = NULL;
    an_expr_node dummy_gro;
    std::memset(&dummy_gro, 0, sizeof(dummy_gro));
    halo_descr.alloc_failure_gro_call = &dummy_gro;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_coro_halo(&halo_descr, &can_elide));
    if (can_elide != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;

    halo_descr.alloc_failure_gro_call = NULL;
    a_variable escaping_promise;
    std::memset(&escaping_promise, 0, sizeof(escaping_promise));
    escaping_promise.address_taken = TRUE;
    halo_descr.promise = &escaping_promise;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_coro_halo(&halo_descr, &can_elide));
    if (can_elide != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_lvalue *noop_frame = NULL;
    if (gcc_gen_be_get_noop_coroutine_frame(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_get_noop_coroutine_frame(&noop_frame) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_noop_coroutine_frame(&noop_frame));
    if (!noop_frame) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_jit_lvalue *noop_frame_cached = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_noop_coroutine_frame(&noop_frame_cached));
    if (noop_frame_cached != noop_frame) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_jit_context_release(ctx);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for the coroutine unit test executable.
 * @details Runs `run_coroutine_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_coroutine_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}

