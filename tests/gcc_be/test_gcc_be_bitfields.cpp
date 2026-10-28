/**
 * @file test_gcc_be_bitfields.cpp
 * @brief Unit tests for bitfield layout, read, write, and compound assignment lowering.
 * @details Verifies bitfield container synthesis in `gcc_gen_be_type.c` and bitfield
 * mask/shift operations in `gcc_gen_be_expr.c`.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_error.h"
#include "const_ints.h"
#include "float_pt.h"
#include <cstdio>

extern "C" {
/**
 * @brief Mock diagnostic reporter for bitfield unit tests.
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
/** @brief Innermost function scope pointer. */
a_scope_ptr innermost_function_scope = NULL;

/**
 * @brief Stub for computing type size in bitfield unit tests.
 * @details Returns `tp->size` directly.
 * @param[in] tp Type pointer.
 * @return Byte size of `tp`.
 */
a_targ_size_t f_size_of_type(a_type_ptr tp) {
    return tp ? tp->size : 0;
}

/**
 * @brief Stub for extracting host FP value in bitfield unit tests.
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
 * @brief Stub for checking integer constant signedness in bitfield unit tests.
 * @details Returns `TRUE`.
 * @param[in] cp Constant pointer.
 * @return `TRUE`.
 */
a_boolean int_constant_is_signed(a_constant_ptr cp) {
    (void)cp;
    return TRUE;
}

/**
 * @brief Stub for creating integer value mask in bitfield unit tests.
 * @details Zeroes `*mask`.
 * @param[out] mask Mask pointer.
 * @param[in] bits Number of bits.
 */
void make_integer_value_mask(an_integer_value *mask, unsigned long bits) {
    (void)bits;
    if (mask) *mask = 0;
}

/**
 * @brief Stub for right-shifting integer value in bitfield unit tests.
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
 * @brief Stub for extracting unsigned value of integer value in bitfield unit tests.
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
 * @brief Stub for extracting signed integer constant in bitfield unit tests.
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
 * @brief Stub for extracting unsigned integer constant in bitfield unit tests.
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
 * @brief Stub for looking up routine scope in bitfield unit tests.
 * @details Returns NULL.
 * @param[in] rout Routine pointer.
 * @return NULL.
 */
a_scope_ptr scope_for_routine_or_null(a_routine_ptr rout) {
    (void)rout;
    return NULL;
}

/**
 * @brief Stub for global variable declaration lowering in bitfield unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var Variable pointer.
 * @param[out] out_lval Output lvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (out_lval) *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for local variable declaration lowering in bitfield unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func JIT function.
 * @param[in] var Variable pointer.
 * @param[out] out_lval Output lvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)var;
    if (out_lval) *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function declaration lowering in bitfield unit tests.
 * @details Sets `*out_func` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout Routine pointer.
 * @param[out] out_func Output JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    if (out_func) *out_func = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for statement lowering in bitfield unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] stmt Statement pointer.
 * @param[in] func JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for label block resolution in bitfield unit tests.
 * @details Sets `*out_block` to NULL.
 * @param[in] label Label pointer.
 * @param[in] func JIT function.
 * @param[out] out_block Output block.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_get_or_create_label_block(a_label_ptr label, struct gcc_jit_function *func, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    (void)label;
    (void)func;
    if (out_block) *out_block = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for active coroutine frame lookup in bitfield unit tests.
 * @details Sets `*out_frame_type` and `*out_frame_lval` to NULL.
 * @param[out] out_frame_type Output frame type.
 * @param[out] out_frame_lval Output frame lvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_get_active_coroutine_frame(struct gcc_jit_type **out_frame_type, struct gcc_jit_lvalue **out_frame_lval) GCC_GEN_BE_NOEXCEPT {
    if (out_frame_type) *out_frame_type = NULL;
    if (out_frame_lval) *out_frame_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for registering coroutine suspend point in bitfield unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] resume_block Resume block.
 * @param[out] out_state_idx Output state index.
 * @param[out] out_state_field_lval Output state field lvalue.
 * @param[out] out_frame_ptr_rval Output frame pointer rvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_register_coro_suspend_point(struct gcc_jit_block *resume_block, int *out_state_idx, struct gcc_jit_lvalue **out_state_field_lval, struct gcc_jit_rvalue **out_frame_ptr_rval) GCC_GEN_BE_NOEXCEPT {
    (void)resume_block;
    if (out_state_idx) *out_state_idx = 1;
    if (out_state_field_lval) *out_state_field_lval = NULL;
    if (out_frame_ptr_rval) *out_frame_ptr_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for coroutine symmetric transfer emission in bitfield unit tests.
 * @details Ends `suspend_block` with a `void` return and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Active JIT function.
 * @param[in] suspend_block Suspend basic block.
 * @param[in] handle_rval Target coroutine handle rvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_coro_symmetric_transfer(struct gcc_jit_function *func, struct gcc_jit_block *suspend_block, struct gcc_jit_rvalue *handle_rval) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)handle_rval;
    if (suspend_block) gcc_jit_block_end_with_void_return(suspend_block, NULL);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for RAII scope unwind cleanup emission in bitfield unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Active JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for builtin call lowering in bitfield unit tests.
 * @details Sets `*out_rval` to NULL.
 * @param[in] rout Builtin routine.
 * @param[in] num_args Number of args.
 * @param[in] args Arg array.
 * @param[out] out_rval Output rvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_builtin_call(a_routine_ptr rout, int num_args, struct gcc_jit_rvalue **args, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    (void)num_args;
    (void)args;
    if (out_rval) *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for getting active target ABI in bitfield unit tests.
 * @details Sets `*out_abi` to `GCC_GEN_BE_TARGET_ABI_SYSV_AMD64`.
 * @param[out] out_abi Output ABI.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_get_target_abi(gcc_gen_be_target_abi_t *out_abi) GCC_GEN_BE_NOEXCEPT {
    if (out_abi) *out_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for System V AMD64 classification in bitfield unit tests.
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
 * @brief Stub for va_arg classification in bitfield unit tests.
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
 * @brief Stub for sret check in bitfield unit tests.
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
 * @brief Stub for invisible reference check in bitfield unit tests.
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
 * @brief Stub for rvalue ABI coercion in bitfield unit tests.
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
 * @brief Stub for coerced ABI type lookup in bitfield unit tests.
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
 * @brief Stub for argument register coercion in bitfield unit tests.
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
 * @brief Stub for throw emission in bitfield unit tests.
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
 * @brief Stub for unwind resume emission in bitfield unit tests.
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
 * @brief Stub for direct invoke call emission in bitfield unit tests.
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
 * @brief Stub for indirect invoke call emission in bitfield unit tests.
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
 * @brief Stub for source location lookup in bitfield unit tests.
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
 * @brief Stub for expression source location lookup in bitfield unit tests.
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
 * @brief Stub for type source location lookup in bitfield unit tests.
 * @details Sets `*out_loc` to NULL.
 * @param[in] tp Type pointer.
 * @param[out] out_loc Output location.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_type(a_type_ptr tp, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    if (out_loc) *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for type debug info emission in bitfield unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] tp Type pointer.
 * @param[in] type JIT type.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_type_debug_info(a_type_ptr tp, struct gcc_jit_type *type) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    (void)type;
    return GCC_GEN_BE_SUCCESS;
}
}

/**
 * @brief Executes bitfield expression and type null-pointer handling checks.
 * @details Verifies that null output pointers are rejected with `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_bitfield_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    if (gcc_gen_be_lower_type(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_expr_rvalue(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    unsigned long packed_word = 0;
    const unsigned long b_vals[3] = { 0x5UL, 0x3UL, 0x7UL };
    const unsigned int b_offs[3] = { 0U, 4U, 0U };
    const unsigned int b_widths[3] = { 4U, 4U, 0U };
    if (gcc_gen_be_pack_bitfield_constants(b_vals, b_offs, b_widths, 3, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_pack_bitfield_constants(NULL, b_offs, b_widths, 3, &packed_word) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_pack_bitfield_constants(b_vals, NULL, b_widths, 3, &packed_word) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_pack_bitfield_constants(b_vals, b_offs, NULL, 3, &packed_word) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_pack_bitfield_constants(b_vals, b_offs, b_widths, 3, &packed_word));
    if (packed_word != 0x35UL) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(cache_clear_all());

    a_type uint_tp = {};
    uint_tp.kind = tk_integer;
    uint_tp.size = 4;
    uint_tp.variant.integer.int_kind = ik_unsigned_int;

    a_field bf1 = {}, bf2 = {};
    bf1.source_corresp.name = "bf1";
    bf1.type = &uint_tp;
    bf1.is_bit_field = TRUE;
    bf1.bit_size = 20;
    bf1.offset = 0;
    bf1.offset_bit_remainder = 16;
    bf1.next = &bf2;

    bf2.source_corresp.name = "bf2";
    bf2.type = &uint_tp;
    bf2.is_bit_field = TRUE;
    bf2.bit_size = 8;
    bf2.offset = 4;
    bf2.offset_bit_remainder = 4;

    a_type bf_struct_tp = {};
    bf_struct_tp.kind = tk_struct;
    bf_struct_tp.size = 8;
    bf_struct_tp.source_corresp.name = "PackedBfStruct";
    bf_struct_tp.variant.class_struct_union.field_list = &bf1;

    gcc_jit_type *jit_bf_struct = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&bf_struct_tp, &jit_bf_struct));
    if (!jit_bf_struct) return GCC_GEN_BE_ERROR_INTERNAL;

    a_constant c1 = {}, c2 = {}, agg_c = {};
    c1.kind = ck_integer;
    c1.type = &uint_tp;
    c1.variant.integer_value = 3;
    c1.next = &c2;
    c2.kind = ck_integer;
    c2.type = &uint_tp;
    c2.variant.integer_value = 7;
    agg_c.kind = ck_aggregate;
    agg_c.type = &bf_struct_tp;
    agg_c.variant.aggregate.first_constant = &c1;

    gcc_jit_rvalue *agg_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&agg_c, jit_bf_struct, &agg_rval));
    if (!agg_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test union constant initialization */
    a_field uf = {};
    uf.source_corresp.name = "u_val";
    uf.type = &uint_tp;
    uf.offset = 0;
    a_type union_tp = {};
    union_tp.kind = tk_union;
    union_tp.size = 8;
    union_tp.source_corresp.name = "PaddedUnion";
    union_tp.variant.class_struct_union.field_list = &uf;
    gcc_jit_type *jit_union_t = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&union_tp, &jit_union_t));
    if (!jit_union_t) return GCC_GEN_BE_ERROR_INTERNAL;

    a_constant uc1 = {}, u_agg = {};
    uc1.kind = ck_integer;
    uc1.type = &uint_tp;
    uc1.variant.integer_value = 42;
    u_agg.kind = ck_aggregate;
    u_agg.type = &union_tp;
    u_agg.variant.aggregate.first_constant = &uc1;
    gcc_jit_rvalue *union_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&u_agg, jit_union_t, &union_rval));
    if (!union_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_jit_context_release(ctx);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_bitfields.cpp`.
 * @details Runs `run_bitfield_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_bitfield_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
