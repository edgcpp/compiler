/**
 * @file test_gcc_be_stmt2.cpp
 * @brief Unit tests for VLA statement lowering and scope/block stack management (`gcc_gen_be_stmt.c`).
 * @details Exercises `stmk_vla_decl` lowering, `gcc_gen_be_push_block`, `gcc_gen_be_pop_block`,
 * `gcc_gen_be_push_scope`, and `gcc_gen_be_pop_scope`.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_builtin.h"
#include "const_ints.h"
#include "float_pt.h"
#include <libgccjit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
/** @brief Innermost function scope pointer. */
a_scope_ptr innermost_function_scope = NULL;
/** @brief Named register table. */
a_const_char *named_register_names[anr_last + 1] = { NULL };
/** @brief Mock IL header. */
an_il_header il_header;

/**
 * @brief Stub for computing type size in stmt2 unit tests.
 * @details Returns `tp->size` directly.
 * @param[in] tp Type pointer.
 * @return Byte size of `tp`.
 */
a_targ_size_t f_size_of_type(a_type_ptr tp) {
    return tp ? tp->size : 0;
}

/**
 * @brief Stub for extracting host FP value in stmt2 unit tests.
 * @details Returns 0.0.
 * @param[in] kind Float kind.
 * @param[in] val Internal float value.
 * @return 0.0.
 */
double fetch_host_fp_value(a_float_kind kind, an_internal_float_value *val) {
    (void)kind;
    (void)val;
    return 0.0;
}

/**
 * @brief Stub for checking integer constant signedness in stmt2 unit tests.
 * @details Returns `TRUE`.
 * @param[in] cp Constant pointer.
 * @return `TRUE`.
 */
a_boolean int_constant_is_signed(a_constant_ptr cp) {
    (void)cp;
    return TRUE;
}

/**
 * @brief Stub for creating integer value mask in stmt2 unit tests.
 * @details Zeroes `*mask`.
 * @param[out] mask Mask pointer.
 * @param[in] bits Number of bits.
 */
void make_integer_value_mask(an_integer_value *mask, unsigned long bits) {
    (void)bits;
    if (mask) *mask = 0;
}

/**
 * @brief Stub for right-shifting integer value in stmt2 unit tests.
 * @details No-op.
 * @param[in,out] val Integer value pointer.
 * @param[in] shift Shift count.
 * @param[in] is_signed Signedness flag.
 * @param[in] sign_extend Sign extension flag.
 */
void shift_right_integer_value(an_integer_value *val, int shift, a_boolean is_signed, a_boolean sign_extend) {
    (void)val;
    (void)shift;
    (void)is_signed;
    (void)sign_extend;
}

/**
 * @brief Stub for extracting unsigned value of integer value in stmt2 unit tests.
 * @details Sets `*ovflo` to `FALSE` and returns 0.
 * @param[in] val Integer value pointer.
 * @param[in] is_signed Signedness flag.
 * @param[out] ovflo Overflow flag.
 * @return 0.
 */
a_host_large_unsigned unsigned_value_of_integer_value(an_integer_value *val, a_boolean is_signed, a_boolean *ovflo) {
    (void)val;
    (void)is_signed;
    if (ovflo) *ovflo = FALSE;
    return 0;
}

/**
 * @brief Stub for extracting signed integer constant in stmt2 unit tests.
 * @details Sets `*err` to `FALSE` and returns 0.
 * @param[in] cp Constant pointer.
 * @param[out] err Overflow flag.
 * @return 0.
 */
a_host_large_integer value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    (void)cp;
    if (err) *err = FALSE;
    return 0;
}

/**
 * @brief Stub for extracting unsigned integer constant in stmt2 unit tests.
 * @details Sets `*err` to `FALSE` and returns 0.
 * @param[in] cp Constant pointer.
 * @param[out] err Overflow flag.
 * @return 0.
 */
a_host_large_unsigned unsigned_value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    (void)cp;
    if (err) *err = FALSE;
    return 0;
}

/**
 * @brief Stub for looking up routine scope in stmt2 unit tests.
 * @details Returns NULL.
 * @param[in] rout Routine pointer.
 * @return NULL.
 */
a_scope_ptr scope_for_routine_or_null(a_routine_ptr rout) {
    (void)rout;
    return NULL;
}

/**
 * @brief Stub for retrieving coroutine descriptor in stmt2 unit tests.
 * @details Returns NULL.
 * @param[in] rout Routine pointer.
 * @return NULL.
 */
a_coroutine_descr_ptr get_coroutine_descr(a_routine_ptr rout) {
    (void)rout;
    return NULL;
}

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
 * @brief Stub for local variable declaration lowering.
 * @details Allocates a dummy local in `func` and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func The active JIT function.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered local lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    *out_lval = func ? gcc_jit_function_new_local(func, NULL, int_t, "vla_local") : NULL;
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
 * @param[in] num_args Number of arguments.
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

/**
 * @brief Stub for zero rvalue creation in stmt2 unit tests.
 * @details Creates a zero int rvalue.
 * @param[in] edg_type Frontend type.
 * @param[in] jit_type Lowered JIT type.
 * @param[out] out_rval Output zero rvalue.
 * @return GCC_GEN_BE_SUCCESS.
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
 * @brief Stub for getting active target ABI in stmt2 unit tests.
 * @details Sets `*out_abi` to `GCC_GEN_BE_TARGET_ABI_SYSV_AMD64`.
 * @param[out] out_abi Output ABI.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_get_target_abi(gcc_gen_be_target_abi_t *out_abi) GCC_GEN_BE_NOEXCEPT {
    if (out_abi) *out_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for System V AMD64 classification in stmt2 unit tests.
 * @details Sets both classes to `GCC_GEN_BE_ABI_CLASS_INTEGER`.
 * @param[in] type Type pointer.
 * @param[out] classes Class array.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_classify_sysv_amd64(a_type_ptr type, gcc_gen_be_abi_class_t classes[2]) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (classes) {
        classes[0] = GCC_GEN_BE_ABI_CLASS_INTEGER;
        classes[1] = GCC_GEN_BE_ABI_CLASS_NO_CLASS;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for va_arg classification in stmt2 unit tests.
 * @details Sets `*out_is_stack` to `TRUE`.
 * @param[in] type Type pointer.
 * @param[out] out_is_stack Stack flag.
 * @param[out] out_num_gp GP register count.
 * @param[out] out_num_fp FP register count.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_classify_va_arg(a_type_ptr type, a_boolean *out_is_stack, int *out_num_gp, int *out_num_fp) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (out_is_stack) *out_is_stack = TRUE;
    if (out_num_gp) *out_num_gp = 0;
    if (out_num_fp) *out_num_fp = 0;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for sret check in stmt2 unit tests.
 * @details Sets `*out_is_sret` to `FALSE`.
 * @param[in] type Type pointer.
 * @param[out] out_is_sret Output boolean.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (out_is_sret) *out_is_sret = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for invisible reference check in stmt2 unit tests.
 * @details Sets `*out_is_ref` to `FALSE`.
 * @param[in] type Type pointer.
 * @param[out] out_is_ref Output boolean.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (out_is_ref) *out_is_ref = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for rvalue ABI coercion in stmt2 unit tests.
 * @details Forwards `src_rval` to `*out_rval`.
 * @param[in] func JIT function.
 * @param[in] block JIT block.
 * @param[in] src_rval Source rvalue.
 * @param[in] dst_type Destination JIT type.
 * @param[out] out_rval Output coerced rvalue.
 * @return GCC_GEN_BE_SUCCESS.
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
 * @brief Stub for coerced ABI type lookup in stmt2 unit tests.
 * @details Sets `*out_coerced_type` to NULL and `*out_needs_coercion` to `FALSE`.
 * @param[in] type Type pointer.
 * @param[out] out_coerced_type Output coerced type.
 * @param[out] out_needs_coercion Output boolean.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (out_coerced_type) *out_coerced_type = NULL;
    if (out_needs_coercion) *out_needs_coercion = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for argument register coercion in stmt2 unit tests.
 * @details Forwards `src_rval` to `*out_rval`.
 * @param[in] func JIT function.
 * @param[in] block JIT block.
 * @param[in] arg_type Argument frontend type.
 * @param[in] src_rval Source argument rvalue.
 * @param[out] out_rval Output coerced argument rvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_coerce_Arg_to_registers(
    struct gcc_jit_function *func,
    struct gcc_jit_block *block,
    a_type_ptr arg_type,
    struct gcc_jit_rvalue *src_rval,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)block;
    (void)arg_type;
    if (out_rval) *out_rval = src_rval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for try block lowering in stmt2 unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] stmt Try statement.
 * @param[in] func JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_try_block(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for throw emission in stmt2 unit tests.
 * @details Sets `*out_block` to NULL.
 * @param[in] expr Throw expression.
 * @param[out] out_block Output block.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_throw(an_expr_node_ptr expr, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (out_block) *out_block = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for unwind resume emission in stmt2 unit tests.
 * @details Sets `*out_block` to NULL.
 * @param[in] exc_ptr Exception pointer rvalue.
 * @param[out] out_block Output block.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_unwind_resume(struct gcc_jit_rvalue *exc_ptr, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    (void)exc_ptr;
    if (out_block) *out_block = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for direct invoke call emission in stmt2 unit tests.
 * @details Sets `*out_next_block` to `normal_dest` or `block`.
 * @param[in] ctx Active JIT context.
 * @param[in] block Current basic block.
 * @param[in] fn Target function.
 * @param[in] num_args Argument count.
 * @param[in] args Argument array.
 * @param[in] result_lval Result lvalue.
 * @param[in] normal_dest Normal destination block.
 * @param[in] unwind_dest Unwind destination block.
 * @param[out] out_next_block Pointer to receive next block.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_invoke_call(
    struct gcc_jit_context *ctx,
    struct gcc_jit_block *block,
    struct gcc_jit_function *fn,
    int num_args,
    struct gcc_jit_rvalue **args,
    struct gcc_jit_lvalue *result_lval,
    struct gcc_jit_block *normal_dest,
    struct gcc_jit_block *unwind_dest,
    struct gcc_jit_block **out_next_block) GCC_GEN_BE_NOEXCEPT {
    (void)ctx;
    (void)fn;
    (void)num_args;
    (void)args;
    (void)result_lval;
    (void)unwind_dest;
    if (!out_next_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_next_block = normal_dest ? normal_dest : block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for indirect invoke call emission in stmt2 unit tests.
 * @details Sets `*out_next_block` to `normal_dest` or `block`.
 * @param[in] ctx Active JIT context.
 * @param[in] block Current basic block.
 * @param[in] fn_ptr Target function pointer.
 * @param[in] num_args Argument count.
 * @param[in] args Argument array.
 * @param[in] result_lval Result lvalue.
 * @param[in] normal_dest Normal destination block.
 * @param[in] unwind_dest Unwind destination block.
 * @param[out] out_next_block Pointer to receive next block.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_invoke_call_through_ptr(
    struct gcc_jit_context *ctx,
    struct gcc_jit_block *block,
    struct gcc_jit_rvalue *fn_ptr,
    int num_args,
    struct gcc_jit_rvalue **args,
    struct gcc_jit_lvalue *result_lval,
    struct gcc_jit_block *normal_dest,
    struct gcc_jit_block *unwind_dest,
    struct gcc_jit_block **out_next_block) GCC_GEN_BE_NOEXCEPT {
    (void)ctx;
    (void)fn_ptr;
    (void)num_args;
    (void)args;
    (void)result_lval;
    (void)unwind_dest;
    if (!out_next_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_next_block = normal_dest ? normal_dest : block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for RAII scope unwind cleanup emission in stmt2 unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Active JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for SEH `.pdata` / `.xdata` unwind table emission in stmt2 unit tests.
 * @details Sets `*out_xdata_global` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Active JIT function.
 * @param[in] has_except_handler Whether the scope is `__except` (`TRUE`) or `__finally` (`FALSE`).
 * @param[out] out_xdata_global Optional pointer to receive the `.xdata` global.
 * @return GCC_GEN_BE_SUCCESS.
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
 * @brief Stub for source location lookup in stmt2 unit tests.
 * @details Sets `*out_loc` to NULL.
 * @param[in] pos Source position.
 * @param[out] out_loc Output location.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_get_location(a_source_position *pos, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)pos;
    if (out_loc) *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for variable source location lookup in stmt2 unit tests.
 * @details Sets `*out_loc` to NULL.
 * @param[in] var Variable pointer.
 * @param[out] out_loc Output location.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_var(a_variable_ptr var, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (out_loc) *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression source location lookup in stmt2 unit tests.
 * @details Sets `*out_loc` to NULL.
 * @param[in] expr Expression pointer.
 * @param[out] out_loc Output location.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_expr(an_expr_node_ptr expr, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (out_loc) *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for statement source location lookup in stmt2 unit tests.
 * @details Sets `*out_loc` to NULL.
 * @param[in] stmt Statement pointer.
 * @param[out] out_loc Output location.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_stmt(a_statement_ptr stmt, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    if (out_loc) *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for lexical block debug info emission in stmt2 unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] stmt Statement pointer.
 * @param[in] block JIT block.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_block_debug_info(a_statement_ptr stmt, struct gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    (void)block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for variable debug info emission in stmt2 unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var Variable pointer.
 * @param[in] lval JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, struct gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    (void)lval;
    return GCC_GEN_BE_SUCCESS;
}
}

extern "C" {
/**
 * @brief Mock diagnostic reporter for statement unit tests.
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
 * @brief Executes the VLA statement lowering test.
 * @details Creates a synthetic `stmk_vla_decl` statement and lowers it while checking all error returns.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_stmt2_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    a_variable *var = (a_variable *)calloc(1, sizeof(a_variable));
    if (!var) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_OOM;
    }
    var->storage_class = sc_auto;

    a_statement *stmt = (a_statement *)calloc(1, sizeof(a_statement));
    if (!stmt) {
        free(var);
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_OOM;
    }
    stmt->kind = stmk_vla_decl;
    stmt->variant.vla.is_typedef_decl = FALSE;
    stmt->variant.vla.variant.variable = var;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *dummy_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, "dummy", 0, NULL, 0);
    gcc_jit_block *block = gcc_jit_function_new_block(dummy_func, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(block));
    GCC_GEN_BE_CHECK(gcc_gen_be_push_block(block));
    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(stmt, dummy_func));

    GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(block));
    gcc_jit_block *popped = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_block(&popped));

    free(stmt);
    free(var);
    gcc_jit_context_release(ctx);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for statement lowering tests.
 * @details Runs `run_stmt2_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_stmt2_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    puts("PASS");
    return 0;
}
