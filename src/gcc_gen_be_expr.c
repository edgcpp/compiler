/**
 * @file gcc_gen_be_expr.c
 * @brief Implementation of expression lowering for the GCC backend.
 * @details Lowers EDG IL expressions, constants, aggregates, bitfields, pointers-to-members,
 * C11 `_Atomic(T)` operations, `source_location` intrinsics, exception throws, and C++20
 * coroutine `co_await`/`co_yield` expressions into `libgccjit` rvalues and lvalues.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_atomic.h"
#include "gcc_gen_be_simd.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_decl.h"
#include <libgccjit.h>
#include "gcc_gen_be_lib_loader.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/** @brief Points to the local variable capturing the result of the current statement expression. */
struct gcc_jit_lvalue *current_stmt_expr_result_lval = NULL;

static gcc_gen_be_error_t safe_ptr_or_int_cast(
    gcc_jit_context *ctx,
    gcc_jit_rvalue *val,
    gcc_jit_type *dest_type,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Strips `tk_typeref` qualifiers/typedefs from a frontend type pointer.
 * @details Walks `tp->variant.typeref.type` until a non-`tk_typeref` type or `NULL` is reached.
 * @param[in] tp Frontend type pointer (may be NULL).
 * @param[out] out_tp Non-null pointer receiving the stripped `a_type_ptr`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t strip_expr_typerefs(
    a_type_ptr tp,
    a_type_ptr *out_tp) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    while (tp != NULL && tp->kind == tk_typeref) {
        tp = tp->variant.typeref.type;
    }
    *out_tp = tp;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Determines whether a frontend type is a record (`tk_struct`, `tk_class`, or `tk_union`).
 * @details Strips `tk_typeref` wrappers and sets `*out_is_record` to `TRUE` for aggregate record kinds.
 * @param[in] tp Frontend type pointer (may be NULL).
 * @param[out] out_is_record Non-null pointer receiving `TRUE` if `tp` is a record type, else `FALSE`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t check_is_record_type(
    a_type_ptr tp,
    a_boolean *out_is_record) GCC_GEN_BE_NOEXCEPT {
    a_type_ptr ut = NULL;
    GCC_GEN_BE_CHECK(strip_expr_typerefs(tp, &ut));
    *out_is_record = (ut != NULL && (ut->kind == tk_struct || ut->kind == tk_class || ut->kind == tk_union))
        ? TRUE
        : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies call-family and pointer-to-member-field operator kinds.
 * @details Deduplicates multi-way operator checks across rvalue pre-evaluation and call dispatch.
 * @param[in] op Frontend expression operator kind.
 * @param[out] out_is_call Non-null pointer receiving `TRUE` for call/member-call/PMF-call operators.
 * @param[out] out_is_member_call Non-null pointer receiving `TRUE` for member-call and PMF-call operators.
 * @param[out] out_is_pm_field Non-null pointer receiving `TRUE` for `.*` and `->*` field operators.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t classify_call_or_pm_op(
    an_expr_operator_kind op,
    a_boolean *out_is_call,
    a_boolean *out_is_member_call,
    a_boolean *out_is_pm_field) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    a_boolean is_mem = (op == eok_dot_member_call || op == eok_points_to_member_call ||
                        op == eok_dot_pm_call || op == eok_points_to_pm_call) ? TRUE : FALSE;
    *out_is_member_call = is_mem;
    *out_is_call = (op == eok_call || is_mem) ? TRUE : FALSE;
    *out_is_pm_field = (op == eok_pm_field || op == eok_pm_points_to_field) ? TRUE : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Determines whether an expression node is a direct bitfield member access (`s.bf` or `p->bf`).
 * @details Checks `enk_operation` with `eok_dot_field`/`eok_points_to_field` targeting a bitfield `enk_field`.
 * @param[in] e Candidate frontend expression node (may be NULL).
 * @param[out] out_is_bf Non-null pointer receiving `TRUE` when `e` accesses a bitfield, else `FALSE`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t check_is_bitfield_access_op(
    an_expr_node_ptr e,
    a_boolean *out_is_bf) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_is_bf = FALSE;
    if (!e || e->kind != enk_operation) return GCC_GEN_BE_SUCCESS;
    an_expr_operator_kind ek = e->variant.operation.kind;
    if (ek != eok_dot_field && ek != eok_points_to_field) return GCC_GEN_BE_SUCCESS;
    an_expr_node_ptr lhs = e->variant.operation.operands;
    an_expr_node_ptr rhs = lhs ? lhs->next : NULL;
    if (!rhs || rhs->kind != enk_field || !rhs->variant.field.ptr) return GCC_GEN_BE_SUCCESS;
    if (rhs->variant.field.ptr->is_bit_field || rhs->variant.field.ptr->bit_size > 0) {
        *out_is_bf = TRUE;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Evaluates an optional `libgccjit` rvalue inside the active basic block.
 * @details Queries `gcc_gen_be_get_current_block` and emits `gcc_jit_block_add_eval` when
 * `rv` and `cblock` are non-NULL (including `void`-returning call rvalues).
 * @param[in] loc Optional source location.
 * @param[in] rv Candidate rvalue to evaluate (may be NULL).
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t emit_optional_eval(
    gcc_jit_location *loc,
    gcc_jit_rvalue *rv) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (rv && cblock) {
        gcc_jit_block_add_eval(cblock, loc, rv);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits an assignment in the active basic block when destination, source, and block are non-NULL.
 * @details Queries `gcc_gen_be_get_current_block` and calls `gcc_jit_block_add_assignment`.
 * @param[in] loc Optional source location.
 * @param[in] dst Destination lvalue (may be NULL).
 * @param[in] src Source rvalue (may be NULL).
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t assign_if_active_block(
    gcc_jit_location *loc,
    gcc_jit_lvalue *dst,
    gcc_jit_rvalue *src) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (dst && src && cblock) {
        gcc_jit_block_add_assignment(cblock, loc, dst, src);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Resolves the scalar element `libgccjit` type (`float`, `double`, or `long double`) of a `_Complex` type.
 * @details Strips `tk_typeref` wrappers and maps `fk_float`, `fk_long_double`, or default `double`.
 * @param[in] ctx Non-null active `libgccjit` context.
 * @param[in] tp Frontend `_Complex` type pointer.
 * @param[out] out_elem_type Non-null pointer receiving the element `gcc_jit_type *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t get_complex_elem_jit_type(
    gcc_jit_context *ctx,
    a_type_ptr tp,
    gcc_jit_type **out_elem_type) GCC_GEN_BE_NOEXCEPT {
    a_type_ptr ut = NULL;
    GCC_GEN_BE_CHECK(strip_expr_typerefs(tp, &ut));
    if (ut->variant.float_kind == fk_float) {
        *out_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
        return GCC_GEN_BE_SUCCESS;
    }
    if (ut->variant.float_kind == fk_long_double) {
        *out_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
        return GCC_GEN_BE_SUCCESS;
    }
    *out_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Maps an arithmetic/bitwise compound-assignment or increment/decrement EDG operator to `gcc_jit_binary_op`.
 * @details Supports `+=`, `++`, `--`, `-=`, `*=`, `/=`, `%=`, `<<=`, `>>=`, `&=`, `|=`, and `^=` without relying on partial `switch` coverage.
 * @param[in] op Compound-assignment or increment/decrement operator kind.
 * @param[out] out_jit_op Non-null pointer receiving the mapped `enum gcc_jit_binary_op`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OUT_OF_MEMORY`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t map_compound_assign_binary_op(
    an_expr_operator_kind op,
    enum gcc_jit_binary_op *out_jit_op) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (op == eok_add_assign || op == eok_pre_incr || op == eok_post_incr) {
        *out_jit_op = GCC_JIT_BINARY_OP_PLUS;
    } else if (op == eok_subtract_assign || op == eok_pre_decr || op == eok_post_decr) {
        *out_jit_op = GCC_JIT_BINARY_OP_MINUS;
    } else if (op == eok_multiply_assign) {
        *out_jit_op = GCC_JIT_BINARY_OP_MULT;
    } else if (op == eok_divide_assign) {
        *out_jit_op = GCC_JIT_BINARY_OP_DIVIDE;
    } else if (op == eok_remainder_assign) {
        *out_jit_op = GCC_JIT_BINARY_OP_MODULO;
    } else if (op == eok_shiftl_assign) {
        *out_jit_op = GCC_JIT_BINARY_OP_LSHIFT;
    } else if (op == eok_shiftr_assign) {
        *out_jit_op = GCC_JIT_BINARY_OP_RSHIFT;
    } else if (op == eok_and_assign) {
        *out_jit_op = GCC_JIT_BINARY_OP_BITWISE_AND;
    } else if (op == eok_or_assign) {
        *out_jit_op = GCC_JIT_BINARY_OP_BITWISE_OR;
    } else {
        *out_jit_op = GCC_JIT_BINARY_OP_BITWISE_XOR;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Materializes a `_Complex` rvalue from real and imaginary scalar components.
 * @details Handles both 2-field `struct { real, imag }` complex representations and native `GCC_JIT_TYPE_COMPLEX_*`.
 * @param[in] ctx Non-null active `libgccjit` context.
 * @param[in] loc Optional source location.
 * @param[in] cblock Active `gcc_jit_block *` (must be non-NULL).
 * @param[in] cplx_type Lowered `_Complex` `gcc_jit_type *`.
 * @param[in] elem_type Lowered real/imaginary element `gcc_jit_type *`.
 * @param[in] re Real component rvalue.
 * @param[in] im Imaginary component rvalue.
 * @param[out] out_rval Non-null pointer receiving the constructed `_Complex` rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t construct_complex_from_parts(
    gcc_jit_context *ctx,
    gcc_jit_location *loc,
    gcc_jit_block *cblock,
    gcc_jit_type *cplx_type,
    gcc_jit_type *elem_type,
    gcc_jit_rvalue *re,
    gcc_jit_rvalue *im,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (!cblock) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }
    gcc_jit_function *func = gcc_jit_block_get_function(cblock);
    gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, cplx_type, "cplx_res");
    gcc_jit_struct *s = gcc_jit_type_is_struct(cplx_type);
    if (s != NULL && gcc_jit_struct_get_field_count(s) >= 2U) {
        gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), re);
        gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), im);
    } else {
        gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
        gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(tmp, NULL), elem_ptr_type);
        gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        gcc_jit_block_add_assignment(
            cblock, loc,
            gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0)),
            re);
        gcc_jit_block_add_assignment(
            cblock, loc,
            gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1)),
            im);
    }
    *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Looks up a lowered `gcc_jit_function *` in `GCC_GEN_BE_CACHE_FUNC`, declaring it on cache miss.
 * @details Returns `*out_fn = NULL` when `rout` is `NULL` or declaration lowering yields `NULL`.
 * @param[in] rout Frontend routine pointer (may be NULL).
 * @param[out] out_fn Non-null pointer receiving the resolved `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t resolve_cached_or_decl_func(
    a_routine_ptr rout,
    gcc_jit_function **out_fn) GCC_GEN_BE_NOEXCEPT {
    *out_fn = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (!rout) return GCC_GEN_BE_SUCCESS;
    void *cached = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, rout, &cached));
    gcc_jit_function *fn = (gcc_jit_function *)cached;
    if (!fn) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(rout, &fn));
    }
    *out_fn = fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a call to `__edg_gcc_eh_get_unwind_info` in `cblock` and returns the active exception object pointer.
 * @details Caches the imported `__edg_gcc_eh_get_unwind_info` declaration in `GCC_GEN_BE_CACHE_FUNC`.
 * @param[in] ctx Non-null active `libgccjit` context.
 * @param[in] cblock Non-null active basic block.
 * @param[out] out_exc_ptr Non-null pointer receiving the `void *` exception object rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t emit_eh_unwind_exc_rvalue(
    gcc_jit_context *ctx,
    gcc_jit_block *cblock,
    gcc_jit_rvalue **out_exc_ptr) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_function *cur_fn = gcc_jit_block_get_function(cblock);
    gcc_jit_lvalue *tmp_exc = gcc_jit_function_new_local(cur_fn, NULL, void_ptr_type, "__eh_unwind_exc");
    gcc_jit_lvalue *tmp_ti = gcc_jit_function_new_local(cur_fn, NULL, void_ptr_type, "__eh_unwind_ti");
    void *cached_get_info = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__edg_gcc_eh_get_unwind_info", &cached_get_info));
    if (!cached_get_info) {
        gcc_jit_type *vpp_t = gcc_jit_type_get_pointer(void_ptr_type);
        gcc_jit_param *ip[2];
        ip[0] = gcc_jit_context_new_param(ctx, NULL, vpp_t, "out_exc");
        ip[1] = gcc_jit_context_new_param(ctx, NULL, vpp_t, "out_ti");
        gcc_jit_function *info_fn = gcc_jit_context_new_function(
            ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "__edg_gcc_eh_get_unwind_info", 2, ip, 0);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__edg_gcc_eh_get_unwind_info", info_fn));
        cached_get_info = info_fn;
    }
    gcc_jit_rvalue *ia[2] = {
        gcc_jit_lvalue_get_address(tmp_exc, NULL),
        gcc_jit_lvalue_get_address(tmp_ti, NULL)
    };
    gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_get_info, 2, ia));
    *out_exc_ptr = gcc_jit_lvalue_as_rvalue(tmp_exc);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Reconciles a call argument rvalue with a formal parameter's lowered `libgccjit` type.
 * @details Handles pointer casts, scalar casts, ABI struct register coercion, and pointer-integer bitcasts.
 * @param[in] ctx Non-null active `libgccjit` context.
 * @param[in] loc Optional source location.
 * @param[in] src_arg Lowered source argument rvalue (may be NULL).
 * @param[in] param_type Expected parameter `gcc_jit_type *` (may be NULL).
 * @param[out] out_arg Non-null pointer receiving the reconciled `gcc_jit_rvalue *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t reconcile_call_arg_rvalue(
    gcc_jit_context *ctx,
    gcc_jit_location *loc,
    gcc_jit_rvalue *src_arg,
    gcc_jit_type *param_type,
    gcc_jit_rvalue **out_arg) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_arg = src_arg;
    if (!src_arg || !param_type) return GCC_GEN_BE_SUCCESS;
    gcc_jit_type *arg_type = gcc_jit_rvalue_get_type(src_arg);
    if (arg_type == param_type) return GCC_GEN_BE_SUCCESS;

    if (gcc_jit_type_is_pointer(arg_type) != NULL && gcc_jit_type_is_pointer(param_type) != NULL) {
        *out_arg = gcc_jit_context_new_cast(ctx, loc, src_arg, param_type);
        return GCC_GEN_BE_SUCCESS;
    }
    if (gcc_jit_type_is_struct(arg_type) != NULL || gcc_jit_type_is_struct(param_type) != NULL) {
        extern gcc_gen_be_error_t gcc_gen_be_coerce_rvalue(
            gcc_jit_function *fn,
            gcc_jit_block *blk,
            gcc_jit_rvalue *src_rval,
            gcc_jit_type *dst_type,
            gcc_jit_rvalue **out_coerced) GCC_GEN_BE_NOEXCEPT;
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (cblock) {
            gcc_jit_rvalue *coerced_arg = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_coerce_rvalue(
                gcc_jit_block_get_function(cblock), cblock, src_arg, param_type, &coerced_arg));
            if (coerced_arg) {
                *out_arg = coerced_arg;
            }
        }
        return GCC_GEN_BE_SUCCESS;
    }
    if (gcc_jit_type_is_pointer(arg_type) == NULL && gcc_jit_type_is_pointer(param_type) == NULL) {
        return safe_ptr_or_int_cast(ctx, src_arg, param_type, out_arg);
    }
    if (p_gcc_jit_context_new_bitcast) {
        *out_arg = p_gcc_jit_context_new_bitcast(ctx, NULL, src_arg, param_type);
    } else {
        gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_rvalue *vptr = gcc_jit_context_new_cast(ctx, loc, src_arg, void_ptr_type);
        *out_arg = gcc_jit_context_new_cast(ctx, loc, vptr, param_type);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a direct or indirect call (including EH landing-pad invoke, `sret`, and ABI return unpacking).
 * @details Shared tail of `eok_call` for both direct `gcc_jit_function *` and indirect `fn_ptr` callees.
 * @param[in] ctx Non-null active `libgccjit` context.
 * @param[in] loc Optional call-site location.
 * @param[in] func Direct callee function (NULL when calling via `fn_ptr`).
 * @param[in] fn_ptr Indirect callee function pointer (used when `func == NULL`).
 * @param[in] num_args Number of lowered call arguments.
 * @param[in] args Lowered call argument array.
 * @param[in] uses_sret `TRUE` when return aggregate is passed via hidden `sret_lval`.
 * @param[in] sret_lval Destination `sret` local lvalue when `uses_sret` is `TRUE`.
 * @param[in] expected_ret_type Expected lowered expression return type.
 * @param[out] out_rval Non-null pointer receiving the call result rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t finish_direct_or_indirect_call(
    gcc_jit_context *ctx,
    gcc_jit_location *loc,
    gcc_jit_function *func,
    gcc_jit_rvalue *fn_ptr,
    int num_args,
    gcc_jit_rvalue **args,
    a_boolean uses_sret,
    gcc_jit_lvalue *sret_lval,
    gcc_jit_type *expected_ret_type,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_rvalue *call_rval = func
        ? gcc_jit_context_new_call(ctx, loc, func, num_args, args)
        : gcc_jit_context_new_call_through_ptr(ctx, NULL, fn_ptr, num_args, args);
    gcc_jit_block *active_lp = NULL;
    gcc_jit_block *invoke_cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&active_lp));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&invoke_cblock));
    if (active_lp && invoke_cblock &&
        gcc_jit_block_get_function(active_lp) != gcc_jit_block_get_function(invoke_cblock)) {
        active_lp = NULL;
    }
    if (uses_sret) {
        if (active_lp && invoke_cblock) {
            gcc_jit_block *next_blk = NULL;
            if (func) {
                GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call(
                    ctx, invoke_cblock, func, num_args, args, NULL, NULL, active_lp, &next_blk));
            } else {
                GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call_through_ptr(
                    ctx, invoke_cblock, fn_ptr, num_args, args, NULL, NULL, active_lp, &next_blk));
            }
        } else if (invoke_cblock) {
            gcc_jit_block_add_eval(invoke_cblock, loc, call_rval);
        }
        *out_rval = gcc_jit_lvalue_as_rvalue(sret_lval);
        return GCC_GEN_BE_SUCCESS;
    }

    if (active_lp && invoke_cblock) {
        gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_type *ret_t = gcc_jit_rvalue_get_type(call_rval);
        gcc_jit_lvalue *res_lval = (ret_t != void_t)
            ? gcc_jit_function_new_local(gcc_jit_block_get_function(invoke_cblock), loc, ret_t, "invoke_res")
            : NULL;
        gcc_jit_block *next_blk = NULL;
        if (func) {
            GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call(
                ctx, invoke_cblock, func, num_args, args, res_lval, NULL, active_lp, &next_blk));
        } else {
            GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call_through_ptr(
                ctx, invoke_cblock, fn_ptr, num_args, args, res_lval, NULL, active_lp, &next_blk));
        }
        call_rval = res_lval
            ? gcc_jit_lvalue_as_rvalue(res_lval)
            : gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);
    }

    if (call_rval && expected_ret_type &&
        expected_ret_type != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID) &&
        gcc_jit_rvalue_get_type(call_rval) != expected_ret_type) {
        extern gcc_gen_be_error_t gcc_gen_be_coerce_rvalue(
            gcc_jit_function *fn,
            gcc_jit_block *blk,
            gcc_jit_rvalue *src_rval,
            gcc_jit_type *dst_type,
            gcc_jit_rvalue **out_coerced) GCC_GEN_BE_NOEXCEPT;
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (cblock) {
            gcc_jit_rvalue *unpacked_ret = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_coerce_rvalue(
                gcc_jit_block_get_function(cblock), cblock, call_rval, expected_ret_type, &unpacked_ret));
            if (unpacked_ret) {
                call_rval = unpacked_ret;
            }
        }
    }
    *out_rval = call_rval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Determines whether an lvalue expression designates a C11 `_Atomic(T)` object.
 * @details Inspects `lval_expr->type` as well as the underlying variable, struct field, or pointer
 * pointee type (because C11 lvalue conversion strips `TQ_C11_ATOMIC` from `expr->type` on rvalue reads).
 * @param[in] lval_expr Frontend lvalue expression node (may be NULL).
 * @param[out] out_atomic_type Pointer receiving the `_Atomic(T)` type when atomic, or `NULL` otherwise.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t resolve_lvalue_c11_atomic_type(
    an_expr_node_ptr lval_expr,
    a_type_ptr *out_atomic_type) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_atomic_type = NULL;

    a_boolean is_at = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_c11_atomic_type(lval_expr->type, &is_at));
    if (is_at) {
        *out_atomic_type = lval_expr->type;
        return GCC_GEN_BE_SUCCESS;
    }
    if (lval_expr->kind == enk_variable && lval_expr->variant.variable.ptr) {
        a_type_ptr vt = lval_expr->variant.variable.ptr->type;
        GCC_GEN_BE_CHECK(gcc_gen_be_is_c11_atomic_type(vt, &is_at));
        if (is_at) {
            *out_atomic_type = vt;
            return GCC_GEN_BE_SUCCESS;
        }
    } else if (lval_expr->kind == enk_operation) {
        an_expr_operator_kind k = lval_expr->variant.operation.kind;
        an_expr_node_ptr a1 = lval_expr->variant.operation.operands;
        an_expr_node_ptr a2 = a1 ? a1->next : NULL;
        if ((k == eok_dot_field || k == eok_points_to_field) &&
            a2 && a2->kind == enk_field && a2->variant.field.ptr) {
            a_type_ptr ft = a2->variant.field.ptr->type;
            GCC_GEN_BE_CHECK(gcc_gen_be_is_c11_atomic_type(ft, &is_at));
            if (is_at) {
                *out_atomic_type = ft;
                return GCC_GEN_BE_SUCCESS;
            }
        } else if ((k == eok_indirect || k == eok_ref_indirect) && a1 && a1->type) {
            a_type_ptr pt = NULL;
            GCC_GEN_BE_CHECK(strip_expr_typerefs(a1->type, &pt));
            if (pt->kind == tk_pointer && pt->variant.pointer.type) {
                GCC_GEN_BE_CHECK(gcc_gen_be_is_c11_atomic_type(pt->variant.pointer.type, &is_at));
                if (is_at) {
                    *out_atomic_type = pt->variant.pointer.type;
                    return GCC_GEN_BE_SUCCESS;
                }
            }
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Checks if an EDG type is a complex floating-point type.
 * @details Strips typedefs and checks if the underlying type kind is tk_complex.
 * @param[in] tp The EDG type pointer to inspect.
 * @param[out] out_is_complex Non-null pointer to receive TRUE if complex type, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_OOM.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t check_is_complex_type(a_type_ptr tp, a_boolean *out_is_complex) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(strip_expr_typerefs(tp, &tp));
#ifdef EDG_GCC_BE_ISOLATED_TEST
    *out_is_complex = (tp && tp->kind == tk_float && tp->variant.float_supplement.is_complex) ? TRUE : FALSE;
#else
    *out_is_complex = (tp && tp->kind == tk_complex) ? TRUE : FALSE;
#endif
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Extracts the real and imaginary components of a lowered complex rvalue.
 * @details Supports both struct-represented complex types and native array-indexed complex types.
 * @param[in] ctx Non-null libgccjit context.
 * @param[in] loc The source location (may be NULL).
 * @param[in] cblock The current basic block (required for native complex types).
 * @param[in] val The complex rvalue.
 * @param[in] c_type The libgccjit complex type.
 * @param[in] elem_type Non-null underlying floating-point element type.
 * @param[out] out_real Non-null pointer to receive the real part rvalue.
 * @param[out] out_imag Non-null pointer to receive the imaginary part rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on invalid arguments/state.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t extract_complex_parts(
    gcc_jit_context *ctx,
    gcc_jit_location *loc,
    gcc_jit_block *cblock,
    gcc_jit_rvalue *val,
    gcc_jit_type *c_type,
    gcc_jit_type *elem_type,
    gcc_jit_rvalue **out_real,
    gcc_jit_rvalue **out_imag) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (!val) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    if (gcc_jit_type_is_struct(c_type) && gcc_jit_struct_get_field_count(gcc_jit_type_is_struct(c_type)) >= 2U) {
        gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
        *out_real = gcc_jit_rvalue_access_field(val, NULL, gcc_jit_struct_get_field(s, 0));
        *out_imag = gcc_jit_rvalue_access_field(val, NULL, gcc_jit_struct_get_field(s, 1));
    } else {
        if (!cblock) return GCC_GEN_BE_ERROR_INVALID_STATE;
        gcc_jit_function *func = gcc_jit_block_get_function(cblock);
        gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
        gcc_jit_block_add_assignment(cblock, loc, tmp, val);
        gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
        gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
        gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
        gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
        gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
        *out_real = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, elem_ptr, zero));
        *out_imag = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, elem_ptr, one));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers an EDG dynamic initialization (`a_dynamic_init_ptr`) into a target lvalue and/or rvalue.
 *
 * @details Handles `dik_zero`, `dik_constant`, `dik_expression`, `dik_class_result_via_ctor`,
 * `dik_constructor`, `dik_nonconstant_aggregate`, `dik_bitwise_copy`, `dik_lambda`, array master
 * initialization loops (`ck_init_repeat` / `ck_dynamic_init`), and destructor cleanup registration.
 *
 * @param[in] dip The frontend dynamic initialization descriptor.
 * @param[in] target_lval The destination lvalue to initialize (may be NULL if only computing rvalue).
 * @param[out] out_rval Pointer to receive the resulting rvalue (must not be NULL).
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_init(a_dynamic_init_ptr dip, struct gcc_jit_lvalue *target_lval, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    if (!dip) return GCC_GEN_BE_SUCCESS;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));

    gcc_jit_type *target_type = NULL;
    a_type_ptr edg_type = dip->variable ? dip->variable->type : NULL;
    if (target_lval) {
        target_type = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(target_lval));
    } else if (edg_type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(edg_type, &target_type));
    }

    gcc_jit_rvalue *init_rval = NULL;

    switch (dip->kind) {
        case dik_none:
            break;

        case dik_zero:
            if (target_type) {
                GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(edg_type, target_type, &init_rval));
            }
            if (target_lval && init_rval && cblock) {
                gcc_jit_block_add_assignment(cblock, NULL, target_lval, init_rval);
            }
            break;

        case dik_constant:
        case dik_nonconstant_aggregate:
        case dik_lambda:
            {
                a_constant_ptr con = dip->variant.constant.ptr;
                if (con && con->kind == ck_init_repeat && target_lval && cblock) {
                    /* Master array element initialization loop */
                    gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                    a_constant_ptr elem_con = con->variant.init_repeat.constant;
                    long count = (long)con->variant.init_repeat.count;
                    if (count > 0 && elem_con) {
                        gcc_jit_type *idx_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                        gcc_jit_lvalue *idx_var = gcc_jit_function_new_local(func, NULL, idx_type, "arr_init_idx");
                        gcc_jit_block_add_assignment(cblock, NULL, idx_var, gcc_jit_context_new_rvalue_from_long(ctx, idx_type, 0));

                        gcc_jit_block *loop_cond = gcc_jit_function_new_block(func, "arr_init_cond");
                        gcc_jit_block *loop_body = gcc_jit_function_new_block(func, "arr_init_body");
                        gcc_jit_block *loop_end = gcc_jit_function_new_block(func, "arr_init_end");

                        gcc_jit_block_end_with_jump(cblock, NULL, loop_cond);

                        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(loop_cond));
                        gcc_jit_rvalue *curr_idx = gcc_jit_lvalue_as_rvalue(idx_var);
                        gcc_jit_rvalue *limit = gcc_jit_context_new_rvalue_from_long(ctx, idx_type, count);
                        gcc_jit_rvalue *cmp = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_LT, curr_idx, limit);
                        gcc_jit_block_end_with_conditional(loop_cond, NULL, cmp, loop_body, loop_end);

                        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(loop_body));
                        gcc_jit_lvalue *elem_lval = gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(target_lval), curr_idx);
                        if (elem_con->kind == ck_dynamic_init && elem_con->variant.dynamic_init.ptr) {
                            a_dynamic_init_ptr elem_dip = elem_con->variant.dynamic_init.ptr;
                            gcc_jit_block *elem_cleanup_lp = NULL;
                            if (elem_dip->destructor) {
                                elem_cleanup_lp = gcc_jit_function_new_block(func, "arr_init_eh_cleanup");
                                GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(elem_cleanup_lp));
                            }
                            gcc_jit_rvalue *elem_rval = NULL;
                            gcc_gen_be_error_t elem_err = gcc_gen_be_lower_dynamic_init(elem_dip, elem_lval, &elem_rval);
                            if (elem_cleanup_lp) {
                                if (elem_err != GCC_GEN_BE_SUCCESS) {
                                    --gcc_gen_be_singleton_state.landing_pad_stack_size;
                                    return elem_err;
                                }
                                GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
                            } else {
                                GCC_GEN_BE_CHECK(elem_err);
                            }
                            if (elem_cleanup_lp) {
                                gcc_jit_block *saved_block = NULL;
                                GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_block));
                                gcc_jit_function *elem_dtor_fn = NULL;
                                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(elem_dip->destructor, &elem_dtor_fn));
                                if (elem_dtor_fn) {
                                    gcc_jit_block *dtor_cond = gcc_jit_function_new_block(func, "arr_eh_dtor_cond");
                                    gcc_jit_block *dtor_body = gcc_jit_function_new_block(func, "arr_eh_dtor_body");
                                    gcc_jit_block *dtor_done = gcc_jit_function_new_block(func, "arr_eh_dtor_done");
                                    gcc_jit_block_end_with_jump(elem_cleanup_lp, NULL, dtor_cond);
                                    gcc_jit_rvalue *zero_idx = gcc_jit_context_new_rvalue_from_long(ctx, idx_type, 0);
                                    gcc_jit_rvalue *one_idx = gcc_jit_context_new_rvalue_from_long(ctx, idx_type, 1);
                                    gcc_jit_rvalue *has_prev = gcc_jit_context_new_comparison(
                                        ctx, NULL, GCC_JIT_COMPARISON_GT, gcc_jit_lvalue_as_rvalue(idx_var), zero_idx);
                                    gcc_jit_block_end_with_conditional(dtor_cond, NULL, has_prev, dtor_body, dtor_done);
                                    gcc_jit_block_add_assignment_op(dtor_body, NULL, idx_var, GCC_JIT_BINARY_OP_MINUS, one_idx);
                                    gcc_jit_lvalue *prev_elem = gcc_jit_context_new_array_access(
                                        ctx, NULL, gcc_jit_lvalue_as_rvalue(target_lval), gcc_jit_lvalue_as_rvalue(idx_var));
                                    gcc_jit_rvalue *prev_addr = gcc_jit_lvalue_get_address(prev_elem, NULL);
                                    int dtor_pcount = (int)gcc_jit_function_get_param_count(elem_dtor_fn);
                                    if (dtor_pcount > 0) {
                                        gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(elem_dtor_fn, 0)));
                                        gcc_jit_rvalue *d_args[2];
                                        d_args[0] = gcc_jit_context_new_cast(ctx, NULL, prev_addr, p0_t);
                                        if (dtor_pcount >= 2) {
                                            gcc_jit_type *p1_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(elem_dtor_fn, 1)));
                                            d_args[1] = gcc_jit_context_new_rvalue_from_int(ctx, p1_t, 2);
                                            gcc_jit_block_add_eval(dtor_body, NULL, gcc_jit_context_new_call(ctx, NULL, elem_dtor_fn, 2, d_args));
                                        } else {
                                            gcc_jit_block_add_eval(dtor_body, NULL, gcc_jit_context_new_call(ctx, NULL, elem_dtor_fn, 1, d_args));
                                        }
                                    }
                                    gcc_jit_block_end_with_jump(dtor_body, NULL, dtor_cond);
                                    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(dtor_done));
                                    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                                    gcc_jit_block *after_resume = NULL;
                                    GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(gcc_jit_context_null(ctx, void_ptr_type), &after_resume));
                                    if (after_resume) {
                                        gcc_jit_block_end_with_jump(after_resume, NULL, loop_end);
                                    }
                                } else {
                                    gcc_jit_block_end_with_jump(elem_cleanup_lp, NULL, loop_end);
                                }
                                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_block));
                            }
                        } else {
                            gcc_jit_rvalue *elem_rval = NULL;
                            gcc_jit_type *elem_jit_type = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(elem_lval));
                            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(elem_con, elem_jit_type, &elem_rval));
                            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                            if (elem_rval) {
                                gcc_jit_block_add_assignment(cblock, NULL, elem_lval, elem_rval);
                            }
                        }

                        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                        if (cblock) {
                            gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_long(ctx, idx_type, 1);
                            gcc_jit_block_add_assignment_op(cblock, NULL, idx_var, GCC_JIT_BINARY_OP_PLUS, one);
                            gcc_jit_block_end_with_jump(cblock, NULL, loop_cond);
                        }

                        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(loop_end));
                        cblock = loop_end;
                    }
                    init_rval = gcc_jit_lvalue_as_rvalue(target_lval);
                } else if (con && con->kind == ck_aggregate && con->variant.aggregate.has_dynamic_init_component && target_lval && cblock) {
                    /* Member-by-member or element-by-element initialization with ck_dynamic_init components */
                    a_type_ptr agg_type = NULL;
                    GCC_GEN_BE_CHECK(strip_expr_typerefs(con->type, &agg_type));
                    if (agg_type && agg_type->kind == tk_array) {
                        long idx = 0;
                        gcc_jit_type *idx_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                        for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
                            if (c->kind == ck_designator) {
                                if (!c->variant.designator.is_field_designator && !c->variant.designator.is_generic) {
#ifdef EDG_GCC_BE_ISOLATED_TEST
                                    idx = (long)c->variant.designator.variant.index;
#else
                                    idx = (long)c->variant.designator.variant.array_element;
#endif
                                }
                                c = c->next;
                            }
                            if (!c) break;
                            gcc_jit_rvalue *idx_rval = gcc_jit_context_new_rvalue_from_long(ctx, idx_type, idx);
                            gcc_jit_lvalue *elem_lval = gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(target_lval), idx_rval);
                            if (c->kind == ck_dynamic_init && c->variant.dynamic_init.ptr) {
                                gcc_jit_rvalue *elem_rval = NULL;
                                GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(c->variant.dynamic_init.ptr, elem_lval, &elem_rval));
                            } else {
                                gcc_jit_rvalue *elem_rval = NULL;
                                gcc_jit_type *elem_jit_type = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(elem_lval));
                                GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(c, elem_jit_type, &elem_rval));
                                GCC_GEN_BE_CHECK(assign_if_active_block(NULL, elem_lval, elem_rval));
                            }
                            idx++;
                        }
                    } else if (agg_type && (agg_type->kind == tk_struct || agg_type->kind == tk_class || agg_type->kind == tk_union)) {
                        a_field_ptr f = agg_type->variant.class_struct_union.field_list;
                        for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
                            if (c->kind == ck_designator) {
                                if (c->variant.designator.is_field_designator && !c->variant.designator.is_generic) {
                                    f = c->variant.designator.variant.field;
                                }
                                c = c->next;
                            }
                            if (!c) break;
                            gcc_jit_field *jit_f = NULL;
                            if (f) {
                                void *cached_f = NULL;
                                GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, f, &cached_f));
                                jit_f = (gcc_jit_field *)cached_f;
                            }
                            if (jit_f) {
                                gcc_jit_lvalue *fld_lval = gcc_jit_lvalue_access_field(target_lval, NULL, jit_f);
                                if (c->kind == ck_dynamic_init && c->variant.dynamic_init.ptr) {
                                    gcc_jit_rvalue *fld_rval = NULL;
                                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(c->variant.dynamic_init.ptr, fld_lval, &fld_rval));
                                } else {
                                    gcc_jit_rvalue *fld_rval = NULL;
                                    gcc_jit_type *fld_jit_type = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(fld_lval));
                                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(c, fld_jit_type, &fld_rval));
                                    GCC_GEN_BE_CHECK(assign_if_active_block(NULL, fld_lval, fld_rval));
                                }
                            }
                            if (f) f = f->next;
                        }
                    } else {
                        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(con, target_type, &init_rval));
                        GCC_GEN_BE_CHECK(assign_if_active_block(NULL, target_lval, init_rval));
                    }
                    init_rval = gcc_jit_lvalue_as_rvalue(target_lval);
                } else if (con) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(con, target_type, &init_rval));
                    GCC_GEN_BE_CHECK(assign_if_active_block(NULL, target_lval, init_rval));
                }
            }
            break;

        case dik_expression:
        case dik_class_result_via_ctor:
            if (dip->variant.expression) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(dip->variant.expression, &init_rval));
                GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                if (target_lval && init_rval && cblock) {
                    gcc_jit_type *tl_t = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(target_lval));
                    if (gcc_jit_rvalue_get_type(init_rval) != tl_t &&
                        gcc_jit_type_is_struct(tl_t) == NULL &&
                        gcc_jit_type_is_struct(gcc_jit_rvalue_get_type(init_rval)) == NULL) {
                        GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, init_rval, tl_t, &init_rval));
                    }
                    gcc_jit_block_add_assignment(cblock, NULL, target_lval, init_rval);
                }
            }
            break;

        case dik_bitwise_copy:
            if (dip->variant.bitwise_copy.source) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(dip->variant.bitwise_copy.source, &init_rval));
                GCC_GEN_BE_CHECK(assign_if_active_block(NULL, target_lval, init_rval));
            }
            break;

        case dik_constructor:
            {
                if (dip->variant.constructor.value_initialization && target_lval && cblock) {
                    gcc_jit_rvalue *zero_val = NULL;
                    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(edg_type, target_type, &zero_val));
                    gcc_jit_block_add_assignment(cblock, NULL, target_lval, zero_val);
                }
                a_routine_ptr ctor = dip->variant.constructor.ptr;
                if (ctor) {
                    gcc_jit_function *ctor_fn = NULL;
                    GCC_GEN_BE_CHECK(resolve_cached_or_decl_func(ctor, &ctor_fn));
                    if (ctor_fn && cblock) {
                        int num_args = target_lval ? 1 : 0;
                        for (an_expr_node_ptr a = dip->variant.constructor.args; a != NULL; a = a->next) {
                            num_args++;
                        }
                        int param_count = (int)gcc_jit_function_get_param_count(ctor_fn);
                        int alloc_args = num_args > param_count ? num_args : param_count;
                        gcc_jit_rvalue **args = NULL;
                        if (alloc_args > 0) {
                            GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)alloc_args * sizeof(gcc_jit_rvalue *), (void **)&args));
                            int idx = 0;
                            if (target_lval) {
                                args[idx++] = gcc_jit_lvalue_get_address(target_lval, NULL);
                            }
                            for (an_expr_node_ptr a = dip->variant.constructor.args; a != NULL; a = a->next) {
                                gcc_gen_be_error_t err = gcc_gen_be_lower_expr_rvalue(a, &args[idx]);
                                if (err != GCC_GEN_BE_SUCCESS) {
                                    free(args);
                                    return err;
                                }
                                idx++;
                            }
                            /* If calling a base-object constructor (__base_ctor) of a class with virtual bases
                               that expects an implicit VTT pointer parameter, supply the VTT sub-pointer. */
                            if (idx < param_count && edg_type &&
                                (edg_type->kind == tk_class || edg_type->kind == tk_struct) &&
                                edg_type->variant.class_struct_union.any_virtual_base_classes) {
                                extern gcc_gen_be_error_t gcc_gen_be_emit_vtt(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT;
                                gcc_jit_lvalue *vtt_lval = NULL;
                                gcc_gen_be_error_t vtt_err = gcc_gen_be_emit_vtt(edg_type, &vtt_lval);
                                if (vtt_err != GCC_GEN_BE_SUCCESS) {
                                    free(args);
                                    return vtt_err;
                                }
                                if (vtt_lval) {
                                    args[idx++] = gcc_jit_lvalue_get_address(vtt_lval, NULL);
                                }
                            }
                            while (idx < param_count) {
                                gcc_jit_param *param = gcc_jit_function_get_param(ctor_fn, idx);
                                gcc_jit_type *ptype = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(param));
                                gcc_jit_rvalue *zarg = NULL;
                                gcc_gen_be_error_t zerr = gcc_gen_be_get_zero_rvalue(NULL, ptype, &zarg);
                                if (zerr != GCC_GEN_BE_SUCCESS) {
                                    free(args);
                                    return zerr;
                                }
                                args[idx++] = zarg;
                            }
                            num_args = idx;
                            for (int p = 0; p < param_count; ++p) {
                                gcc_jit_param *param = gcc_jit_function_get_param(ctor_fn, p);
                                gcc_jit_type *ptype = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(param));
                                gcc_jit_type *atype = gcc_jit_rvalue_get_type(args[p]);
                                if (atype != ptype) {
                                    args[p] = gcc_jit_context_new_cast(ctx, NULL, args[p], ptype);
                                }
                            }
                        }
                        gcc_gen_be_error_t berr = gcc_gen_be_get_current_block(&cblock);
                        if (berr != GCC_GEN_BE_SUCCESS) {
                            free(args);
                            return berr;
                        }
                        if (cblock) {
                            gcc_jit_block *active_lp = NULL;
                            berr = gcc_gen_be_get_active_landing_pad(&active_lp);
                            if (berr != GCC_GEN_BE_SUCCESS) {
                                free(args);
                                return berr;
                            }
                            if (active_lp &&
                                gcc_jit_block_get_function(active_lp) == gcc_jit_block_get_function(cblock)) {
                                gcc_jit_block *next_blk = NULL;
                                gcc_gen_be_error_t inv_err = gcc_gen_be_emit_invoke_call(
                                    ctx, cblock, ctor_fn, num_args, args, NULL, NULL, active_lp, &next_blk);
                                if (inv_err != GCC_GEN_BE_SUCCESS) {
                                    free(args);
                                    return inv_err;
                                }
                            } else {
                                gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, NULL, ctor_fn, num_args, args);
                                gcc_jit_block_add_eval(cblock, NULL, call);
                            }
                        }
                        free(args);
                    }
                }
                if (target_lval) {
                    init_rval = gcc_jit_lvalue_as_rvalue(target_lval);
                }
                break;
            }

        default:
            break;
    }

    if (dip->destructor && target_lval && !dip->variable) {
        gcc_jit_function *dtor_fn = NULL;
        GCC_GEN_BE_CHECK(resolve_cached_or_decl_func(dip->destructor, &dtor_fn));
        if (dtor_fn) {
            gcc_gen_be_error_t reg_err = gcc_gen_be_register_cleanup(target_lval, dtor_fn);
            if (reg_err != GCC_GEN_BE_SUCCESS && reg_err != GCC_GEN_BE_ERROR_INTERNAL) {
                return reg_err;
            }
            if (reg_err == GCC_GEN_BE_SUCCESS
#ifndef EDG_GCC_BE_ISOLATED_TEST
                && exceptions_enabled
#endif
            ) {
                GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                if (cblock) {
                    extern gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
                    GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(gcc_jit_block_get_function(cblock)));
                }
            }
        }
    }

    if (!init_rval && target_lval) {
        init_rval = gcc_jit_lvalue_as_rvalue(target_lval);
    }

    *out_rval = init_rval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Scales an array/pointer index by any runtime VLA sub-array dimensions of `elem_tp`.
 * @details For multi-dimensional VLAs (`T vla[rows][cols]`), indexing `vla[r]` advances by
 * `r * cols` leaf elements across the flat stack allocation.
 * @param[in] ctx Active `libgccjit` context.
 * @param[in] loc Optional source location.
 * @param[in] elem_tp Pointee or element EDG type of the subscripted/offset array or pointer.
 * @param[in] idx Raw index rvalue.
 * @param[out] out_scaled Pointer receiving the stride-scaled index rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t scale_vla_subarray_index(
    gcc_jit_context *ctx,
    gcc_jit_location *loc,
    a_type_ptr elem_tp,
    gcc_jit_rvalue *idx,
    gcc_jit_rvalue **out_scaled) GCC_GEN_BE_NOEXCEPT {
    *out_scaled = idx;
    GCC_GEN_BE_CHECK(strip_expr_typerefs(elem_tp, &elem_tp));
    if (!idx || !elem_tp || elem_tp->kind != tk_array || !elem_tp->variant.array.is_variable_size_array) {
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_type *idx_t = gcc_jit_rvalue_get_type(idx);
    gcc_jit_rvalue *scaled = idx;
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    gcc_jit_function *cur_fn = cblock ? gcc_jit_block_get_function(cblock) : NULL;
    a_type_ptr cur = elem_tp;
    while (cur && cur->kind == tk_array && cur->variant.array.is_variable_size_array) {
        void *cached_tdim = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, cur, &cached_tdim));
        if (cached_tdim != NULL) {
            gcc_jit_rvalue *dv = gcc_jit_context_new_cast(
                ctx, loc, gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached_tdim), idx_t);
            scaled = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, idx_t, scaled, dv);
        } else if (cur->variant.array.has_assoc_vla_dimension && innermost_function_scope) {
            a_vla_dimension_ptr dim = innermost_function_scope->vla_dimensions;
            while (dim && dim->type != cur) dim = dim->next;
            while (dim && !dim->dimension_variable && dim->original_dimension) {
                dim = dim->original_dimension;
            }
            if (dim && dim->dimension_variable) {
                void *cached_d = NULL;
                GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, dim->dimension_variable, &cached_d));
                gcc_jit_lvalue *dim_lv = (gcc_jit_lvalue *)cached_d;
                if (!dim_lv && cur_fn) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(cur_fn, dim->dimension_variable, &dim_lv));
                }
                if (dim_lv) {
                    gcc_jit_rvalue *dv = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_as_rvalue(dim_lv), idx_t);
                    scaled = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, idx_t, scaled, dv);
                }
            }
        } else if (cur->variant.array.variant.number_of_elements > 0) {
            gcc_jit_rvalue *dv = gcc_jit_context_new_rvalue_from_long(
                ctx, idx_t, (long)cur->variant.array.variant.number_of_elements);
            scaled = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, idx_t, scaled, dv);
        }
        GCC_GEN_BE_CHECK(strip_expr_typerefs(cur->variant.array.element_type, &cur));
    }
    *out_scaled = scaled;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
  if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_lval = NULL;
  if (!expr) return GCC_GEN_BE_SUCCESS;
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  gcc_gen_be_error_t err;

  gcc_jit_location *loc = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_expr(expr, &loc));

  switch (expr->kind) {

      case enk_temp_init:
      case enk_lambda:
          {
              gcc_jit_type *temp_type = NULL;
              err = gcc_gen_be_lower_type(expr->type, &temp_type);
              if (err != GCC_GEN_BE_SUCCESS) return err;

              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;

              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              gcc_jit_lvalue *temp_lval = NULL;
              gcc_jit_type *coro_ft = NULL;
              gcc_jit_lvalue *coro_fl = NULL;
              void *cached_coro_fld = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_active_coroutine_frame(&coro_ft, &coro_fl));
              if (coro_fl != NULL) {
                  GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, expr, &cached_coro_fld));
              }
              if (cached_coro_fld != NULL) {
                  gcc_jit_lvalue *frame_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(coro_fl), loc);
                  temp_lval = gcc_jit_lvalue_access_field(frame_deref, loc, (gcc_jit_field *)cached_coro_fld);
              } else {
                  temp_lval = gcc_jit_function_new_local(func, NULL, temp_type, "temp_init");
              }

              a_dynamic_init_ptr dip = expr->variant.init.dynamic_init;
              if (dip && dip->kind != dik_none) {
                  gcc_jit_rvalue *init_rval = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(dip, temp_lval, &init_rval));
              }

              *out_lval = temp_lval;
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_variable:
          {
              a_variable_ptr var = expr->variant.variable.ptr;
              if (!var) {
                  return GCC_GEN_BE_SUCCESS;
              }
              void *cached = NULL;
              GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached));

              if (!cached) {
                  void *cached_fld = NULL;
                  gcc_jit_type *coro_ft = NULL;
                  gcc_jit_lvalue *coro_flval = NULL;
                  GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, var, &cached_fld));
                  if (cached_fld != NULL) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_active_coroutine_frame(&coro_ft, &coro_flval));
                  }
                  if (coro_flval != NULL) {
                      gcc_jit_lvalue *frame_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(coro_flval), NULL);
                      *out_lval = gcc_jit_lvalue_access_field(frame_deref, NULL, (gcc_jit_field *)cached_fld);
                      return GCC_GEN_BE_SUCCESS;
                  }
                  if (var->source_corresp.name) {
                      void *existing_sym = NULL;
                      GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)var->source_corresp.name, &existing_sym));
                      if (existing_sym != NULL && existing_sym != (void *)var) {
                          /* Check if existing_sym is already a gcc_jit_lvalue (from _ZTV/_ZTT) or another a_variable_ptr */
                          void *var_lval_check = NULL;
                          GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, existing_sym, &var_lval_check));
                          if (var_lval_check) {
                              GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, var_lval_check));
                              *out_lval = (gcc_jit_lvalue *)var_lval_check;
                              return GCC_GEN_BE_SUCCESS;
                          } else if (strncmp(var->source_corresp.name, "_ZTV", 4) == 0 ||
                                     strncmp(var->source_corresp.name, "_ZTT", 4) == 0 ||
                                     strncmp(var->source_corresp.name, "_ZTI", 4) == 0 ||
                                     strncmp(var->source_corresp.name, "_ZZ", 3) == 0) {
                              GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, existing_sym));
                              *out_lval = (gcc_jit_lvalue *)existing_sym;
                              return GCC_GEN_BE_SUCCESS;
                          }
                      }
                  }
                  if (!var->source_corresp.name && var->storage_class == sc_static &&
                      var->init_kind == initk_static && var->initializer.constant) {
                      gcc_jit_block *cblock = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                      if (cblock) {
                          gcc_jit_type *vtype = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(var->type, &vtype));
                          gcc_jit_lvalue *local_tmp = gcc_jit_function_new_local(
                              gcc_jit_block_get_function(cblock), NULL, vtype, "anon_const_lval");
                          gcc_jit_rvalue *init_rval = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(var->initializer.constant, vtype, &init_rval));
                          if (init_rval) {
                              gcc_jit_block_add_assignment(cblock, NULL, local_tmp, init_rval);
                          }
                          *out_lval = local_tmp;
                          return GCC_GEN_BE_SUCCESS;
                      }
                  }
                  gcc_jit_lvalue *lval = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(var, &lval));
                  *out_lval = lval;
              } else {
                  *out_lval = (gcc_jit_lvalue *)cached;
              }
#if defined(__APPLE__) && !defined(EDG_GCC_BE_ISOLATED_TEST)
              if (var->is_thread_local) {
                  void *cached_tls_fn = NULL;
                  GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, var, &cached_tls_fn));
                  gcc_jit_block *tls_cblk = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&tls_cblk));
                  if (cached_tls_fn && tls_cblk) {
                      gcc_jit_rvalue *tls_ptr = gcc_jit_context_new_call(
                          ctx, loc, (gcc_jit_function *)cached_tls_fn, 0, NULL);
                      *out_lval = gcc_jit_rvalue_dereference(tls_ptr, loc);
                  }
              }
#endif
              return GCC_GEN_BE_SUCCESS;
          }
          
      case enk_object_lifetime:
          {
              return gcc_gen_be_lower_expr_lvalue(expr->variant.object_lifetime.expr, out_lval);
          }

      case enk_condition:
          {
              return gcc_gen_be_lower_expr_lvalue(expr->variant.condition->expr, out_lval);
          }

      case enk_reuse_value:
          {
              a_dynamic_init_ptr dip = expr->variant.reused_value_init;
              if (dip && dip->variable) {
                  void *cached = NULL;
                  GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, dip->variable, &cached));
                  if (cached) {
                      *out_lval = (gcc_jit_lvalue *)cached;
                      return GCC_GEN_BE_SUCCESS;
                  }
              }
              gcc_jit_type *temp_type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &temp_type));
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              gcc_jit_lvalue *temp_lval = gcc_jit_function_new_local(func, NULL, temp_type, "reused_lval");
              if (dip && dip->variable) {
                  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, dip->variable, temp_lval));
              }
              if (dip && dip->kind != dik_none) {
                  gcc_jit_rvalue *init_rval = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(dip, temp_lval, &init_rval));
              }
              *out_lval = temp_lval;
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_operation:
          {
              an_expr_operator_kind op = expr->variant.operation.kind;
              an_expr_node_ptr op1 = expr->variant.operation.operands;
              an_expr_node_ptr op2 = op1 ? op1->next : NULL;
              
              if (op == eok_indirect || op == eok_ref_indirect) {
                  gcc_jit_rvalue *ptr = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op1, &ptr);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  *out_lval = gcc_jit_rvalue_dereference(ptr, loc);
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_parens) {
                  return gcc_gen_be_lower_expr_lvalue(op1, out_lval);
              } else if (op == eok_lvalue_adjust || op == eok_lvalue_cast || op == eok_ref_cast ||
                         op == eok_base_class_cast || op == eok_derived_class_cast) {
                  gcc_jit_lvalue *sub_lval = NULL;
                  err = gcc_gen_be_lower_expr_lvalue(op1, &sub_lval);
                  if (err != GCC_GEN_BE_SUCCESS || !sub_lval) return err;
                  gcc_jit_type *target_type = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &target_type));
                  if (!target_type || gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(sub_lval)) == target_type) {
                      *out_lval = sub_lval;
                  } else {
                      gcc_jit_rvalue *addr = gcc_jit_lvalue_get_address(sub_lval, loc);
                      gcc_jit_rvalue *cast_addr = gcc_jit_context_new_cast(ctx, loc, addr, gcc_jit_type_get_pointer(target_type));
                      *out_lval = gcc_jit_rvalue_dereference(cast_addr, loc);
                  }
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_comma) {
                  gcc_jit_rvalue *r1 = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &r1));
                  GCC_GEN_BE_CHECK(emit_optional_eval(loc, r1));
                  return gcc_gen_be_lower_expr_lvalue(op2, out_lval);
              } else if (op == eok_assign || op == eok_bassign || op == eok_add_assign || op == eok_subtract_assign ||
                         op == eok_multiply_assign || op == eok_divide_assign || op == eok_remainder_assign ||
                         op == eok_shiftl_assign || op == eok_shiftr_assign || op == eok_and_assign ||
                         op == eok_or_assign || op == eok_xor_assign || op == eok_padd_assign || op == eok_psubtract_assign) {
                  gcc_jit_rvalue *dummy = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(expr, &dummy));
                  return gcc_gen_be_lower_expr_lvalue(op1, out_lval);
                } else if (op == eok_dot_field || op == eok_points_to_field) {
                    if (op2 && op2->kind == enk_field && op2->variant.field.ptr) {
                        a_field_ptr f = op2->variant.field.ptr;
                        if (f->is_bit_field || f->bit_size > 0) {
                            *out_lval = NULL;
                            return GCC_GEN_BE_SUCCESS;
                        }
                        void *cached = NULL;
                        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, f, &cached));
                        
                        /* If field is not cached, it might be because the struct type hasn't been completely lowered in this scope yet, 
                           so force lowering the struct type of op1 */
                        if (!cached) {
                            gcc_jit_type *dummy = NULL;
                            err = gcc_gen_be_lower_type(op1->type, &dummy);
                            if (err != GCC_GEN_BE_SUCCESS) return err;
                            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, f, &cached));
                            if (!cached) {
                                /* Optimized empty base or [[no_unique_address]] field: compute byte offset pointer */
                                gcc_jit_rvalue *base_ptr = NULL;
                                if (op == eok_dot_field) {
                                    gcc_jit_lvalue *base = NULL;
                                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &base));
                                    base_ptr = gcc_jit_lvalue_get_address(base, loc);
                                } else {
                                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &base_ptr));
                                }
                                gcc_jit_type *ftype = NULL;
                                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(f->type, &ftype));
                                if (f->offset > 0) {
                                    gcc_jit_type *uchar_ptr = gcc_jit_type_get_pointer(
                                        gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR));
                                    gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                                    base_ptr = gcc_jit_lvalue_get_address(
                                        gcc_jit_context_new_array_access(
                                            ctx, loc, gcc_jit_context_new_cast(ctx, loc, base_ptr, uchar_ptr),
                                            gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)f->offset)),
                                        loc);
                                }
                                *out_lval = gcc_jit_rvalue_dereference(
                                    gcc_jit_context_new_cast(ctx, loc, base_ptr, gcc_jit_type_get_pointer(ftype)), loc);
                                return GCC_GEN_BE_SUCCESS;
                            }
                        }
                        
                        gcc_jit_field *field = (gcc_jit_field *)cached;
                        
                        if (op == eok_dot_field) {
                            gcc_jit_lvalue *base = NULL;
                            err = gcc_gen_be_lower_expr_lvalue(op1, &base);
                            if (err != GCC_GEN_BE_SUCCESS) return err;
                            *out_lval = gcc_jit_lvalue_access_field(base, loc, field);
                        } else {
                            gcc_jit_rvalue *ptr = NULL;
                            err = gcc_gen_be_lower_expr_rvalue(op1, &ptr);
                            if (err != GCC_GEN_BE_SUCCESS) return err;
                            *out_lval = gcc_jit_rvalue_dereference_field(ptr, loc, field);
                        }
                        if (*out_lval && f->type) {
                            gcc_jit_type *expected_ft = NULL;
                            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(f->type, &expected_ft));
                            gcc_jit_type *actual_ft = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(*out_lval));
                            if (expected_ft && expected_ft != actual_ft &&
                                gcc_jit_type_is_struct(expected_ft) != NULL &&
                                gcc_jit_type_is_struct(actual_ft) == NULL) {
                                gcc_jit_rvalue *raw_addr = gcc_jit_lvalue_get_address(*out_lval, loc);
                                gcc_jit_rvalue *typed_addr = gcc_jit_context_new_cast(
                                    ctx, loc, raw_addr, gcc_jit_type_get_pointer(expected_ft));
                                *out_lval = gcc_jit_rvalue_dereference(typed_addr, loc);
                            }
                        }
                        return GCC_GEN_BE_SUCCESS;
                    } else {
                        /* Pointer-to-data-member with offset */
                        gcc_jit_rvalue *offset_rval = NULL;
                        err = gcc_gen_be_lower_expr_rvalue(op2, &offset_rval);
                        if (err != GCC_GEN_BE_SUCCESS) return err;

                        gcc_jit_rvalue *base_rval = NULL;
                        if (op == eok_dot_field) {
                            gcc_jit_lvalue *base_lval = NULL;
                            err = gcc_gen_be_lower_expr_lvalue(op1, &base_lval);
                            if (err != GCC_GEN_BE_SUCCESS) return err;
                            base_rval = gcc_jit_lvalue_get_address(base_lval, NULL);
                        } else {
                            err = gcc_gen_be_lower_expr_rvalue(op1, &base_rval);
                            if (err != GCC_GEN_BE_SUCCESS) return err;
                        }

                        gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
                        gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
                        
                        gcc_jit_rvalue *base_char_ptr = gcc_jit_context_new_cast(ctx, loc, base_rval, char_ptr_type);
                        gcc_jit_lvalue *arr_access = gcc_jit_context_new_array_access(ctx, loc, base_char_ptr, offset_rval);
                        gcc_jit_rvalue *byte_ptr = gcc_jit_lvalue_get_address(arr_access, NULL);
                        
                        gcc_jit_type *dest_type = NULL;
                        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &dest_type));
                        
                        gcc_jit_type *dest_ptr_type = gcc_jit_type_get_pointer(dest_type);
                        gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, loc, byte_ptr, dest_ptr_type);
                        
                        *out_lval = gcc_jit_rvalue_dereference(typed_ptr, loc);
                        return GCC_GEN_BE_SUCCESS;
                    }
              } else if (op == eok_subscript || op == eok_vector_subscript) {
                  gcc_jit_rvalue *ptr = NULL;
                  gcc_jit_lvalue *arr_lval = NULL;
                  a_type_ptr op1_stripped = NULL;
                  GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &op1_stripped));
                  if (op1_stripped && op1_stripped->kind == tk_array) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &arr_lval));
                      if (arr_lval) {
                          gcc_jit_type *elem_type = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(op1_stripped->variant.array.element_type, &elem_type));
                          gcc_jit_type *elem_ptr_type = (op1_stripped->variant.array.element_type &&
                              op1_stripped->variant.array.element_type->kind == tk_array &&
                              op1_stripped->variant.array.element_type->variant.array.is_variable_size_array)
                              ? elem_type
                              : gcc_jit_type_get_pointer(elem_type);
                          gcc_jit_rvalue *arr_rv = gcc_jit_lvalue_as_rvalue(arr_lval);
                          gcc_jit_rvalue *raw_base = (op1_stripped->variant.array.is_variable_size_array &&
                              gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(arr_rv)) != NULL)
                              ? arr_rv
                              : gcc_jit_lvalue_get_address(arr_lval, NULL);
                          ptr = gcc_jit_context_new_cast(ctx, loc, raw_base, elem_ptr_type);
                      }
                  } else if (op == eok_vector_subscript || (op1_stripped && op1_stripped->kind == tk_vector)) {
                      gcc_jit_type *elem_type = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &elem_type));
                      if (op1->is_lvalue) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &arr_lval));
                      }
                      if (arr_lval) {
                          ptr = gcc_jit_context_new_cast(
                              ctx, loc, gcc_jit_lvalue_get_address(arr_lval, NULL), gcc_jit_type_get_pointer(elem_type));
                      } else {
                          gcc_jit_rvalue *vr = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &vr));
                          gcc_jit_block *cblk = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblk));
                          if (cblk && vr) {
                              gcc_jit_lvalue *vslot = gcc_jit_function_new_local(
                                  gcc_jit_block_get_function(cblk), loc, gcc_jit_rvalue_get_type(vr), "__vec_sub_slot");
                              gcc_jit_block_add_assignment(cblk, loc, vslot, vr);
                              ptr = gcc_jit_context_new_cast(
                                  ctx, loc, gcc_jit_lvalue_get_address(vslot, NULL), gcc_jit_type_get_pointer(elem_type));
                          }
                      }
                  }
                  if (!ptr) {
                      err = gcc_gen_be_lower_expr_rvalue(op1, &ptr);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  
                  gcc_jit_rvalue *idx = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op2, &idx);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  a_type_ptr sub_elem_tp = (op1_stripped && op1_stripped->kind == tk_array)
                      ? op1_stripped->variant.array.element_type
                      : (op1_stripped && op1_stripped->kind == tk_pointer ? op1_stripped->variant.pointer.type : NULL);
                  GCC_GEN_BE_CHECK(scale_vla_subarray_index(ctx, loc, sub_elem_tp, idx, &idx));
                  
                  *out_lval = gcc_jit_context_new_array_access(ctx, loc, ptr, idx);
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_pm_field || op == eok_pm_points_to_field) {
                  gcc_jit_rvalue *obj_ptr = NULL;
                  if (op == eok_pm_field) {
                      gcc_jit_lvalue *obj_lval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &obj_lval));
                      if (!obj_lval) return GCC_GEN_BE_ERROR_INTERNAL;
                      obj_ptr = gcc_jit_lvalue_get_address(obj_lval, loc);
                  } else {
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &obj_ptr));
                  }
                  gcc_jit_rvalue *pdm_off = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op2, &pdm_off));
                  gcc_jit_type *uchar_ptr_t = gcc_jit_type_get_pointer(
                      gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR));
                  gcc_jit_rvalue *byte_base = gcc_jit_context_new_cast(ctx, loc, obj_ptr, uchar_ptr_t);
                  gcc_jit_lvalue *byte_elem = gcc_jit_context_new_array_access(ctx, loc, byte_base, pdm_off);
                  gcc_jit_type *field_t = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &field_t));
                  gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(
                      ctx, loc, gcc_jit_lvalue_get_address(byte_elem, loc), gcc_jit_type_get_pointer(field_t));
                  *out_lval = gcc_jit_rvalue_dereference(typed_ptr, loc);
                  return GCC_GEN_BE_SUCCESS;
              } else if ((op == eok_pre_incr || op == eok_pre_decr) && op1) {
                  gcc_jit_lvalue *l1 = NULL;
                  err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_type *type = NULL;
                  err = gcc_gen_be_lower_type(op1->type, &type);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (cblock) {
                      if (gcc_jit_type_is_pointer(type) != NULL) {
                          gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                          gcc_jit_rvalue *delta = gcc_jit_context_new_rvalue_from_int(
                              ctx, int_type, op == eok_pre_incr ? 1 : -1);
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(
                              ctx, loc, gcc_jit_lvalue_as_rvalue(l1), delta);
                          gcc_jit_rvalue *new_ptr = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          gcc_jit_block_add_assignment(cblock, loc, l1, new_ptr);
                      } else {
                          gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, type, 1);
                          gcc_jit_block_add_assignment_op(cblock, NULL, l1, 
                              op == eok_pre_incr ? GCC_JIT_BINARY_OP_PLUS : GCC_JIT_BINARY_OP_MINUS, one);
                      }
                  }
                  *out_lval = l1;
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_question && op1 && op2 && op2->next) {
                  an_expr_node_ptr op3 = op2->next;
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (cblock) {
                      gcc_jit_function *qfn = gcc_jit_block_get_function(cblock);
                      gcc_jit_type *res_t = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &res_t));
                      gcc_jit_type *res_ptr_t = gcc_jit_type_get_pointer(res_t);
                      gcc_jit_lvalue *addr_slot = gcc_jit_function_new_local(qfn, loc, res_ptr_t, "__q_lval_addr");
                      gcc_jit_rvalue *cond_rv = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &cond_rv));
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                      gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                      if (gcc_jit_rvalue_get_type(cond_rv) != bool_t) {
                          cond_rv = gcc_jit_context_new_comparison(
                              ctx, loc, GCC_JIT_COMPARISON_NE, cond_rv,
                              gcc_jit_context_zero(ctx, gcc_jit_rvalue_get_type(cond_rv)));
                      }
                      gcc_jit_block *t_blk = gcc_jit_function_new_block(qfn, "q_lval_true");
                      gcc_jit_block *f_blk = gcc_jit_function_new_block(qfn, "q_lval_false");
                      gcc_jit_block *m_blk = gcc_jit_function_new_block(qfn, "q_lval_merge");
                      gcc_jit_block_end_with_conditional(cblock, loc, cond_rv, t_blk, f_blk);

                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(t_blk));
                      gcc_jit_lvalue *lv2 = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op2, &lv2));
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&t_blk));
                      gcc_jit_block_add_assignment(
                          t_blk, loc, addr_slot,
                          gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(lv2, loc), res_ptr_t));
                      gcc_jit_block_end_with_jump(t_blk, loc, m_blk);

                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(f_blk));
                      gcc_jit_lvalue *lv3 = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op3, &lv3));
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&f_blk));
                      gcc_jit_block_add_assignment(
                          f_blk, loc, addr_slot,
                          gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(lv3, loc), res_ptr_t));
                      gcc_jit_block_end_with_jump(f_blk, loc, m_blk);

                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(m_blk));
                      *out_lval = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(addr_slot), loc);
                      return GCC_GEN_BE_SUCCESS;
                  }
              } else if (op == eok_dot_static || op == eok_points_to_static) {
                  if (op1) {
                      gcc_jit_rvalue *r1_eval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &r1_eval));
                      GCC_GEN_BE_CHECK(emit_optional_eval(loc, r1_eval));
                  }
                  return gcc_gen_be_lower_expr_lvalue(op2, out_lval);
              } else if (op == eok_call || op == eok_dot_member_call || op == eok_points_to_member_call ||
                         op == eok_dot_pm_call || op == eok_points_to_pm_call || op == eok_cast) {
                  gcc_jit_rvalue *rv = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(expr, &rv));
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (rv && cblock) {
                      a_type_ptr etp = NULL;
                      GCC_GEN_BE_CHECK(strip_expr_typerefs(expr->type, &etp));
                      if (etp && etp->kind == tk_pointer && etp->variant.pointer.is_reference) {
                          *out_lval = gcc_jit_rvalue_dereference(rv, loc);
                      } else {
                          gcc_jit_function *mfn = gcc_jit_block_get_function(cblock);
                          gcc_jit_lvalue *mat = gcc_jit_function_new_local(
                              mfn, loc, gcc_jit_rvalue_get_type(rv), "__mat_lval");
                          gcc_jit_block_add_assignment(cblock, loc, mat, rv);
                          *out_lval = mat;
                      }
                      return GCC_GEN_BE_SUCCESS;
                  }
              }
              *out_lval = NULL;
              return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_UNHANDLED_EXPR, "unhandled lvalue operation kind");
          }

      case enk_typeid:
          {
              gcc_jit_rvalue *ti_ptr = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_typeid(expr, &ti_ptr));
              *out_lval = gcc_jit_rvalue_dereference(ti_ptr, loc);
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_statement:
      case enk_builtin_operation:
      case enk_new_delete:
      case enk_await:
      case enk_yield:
      case enk_braced_init_list:
          {
              gcc_jit_rvalue *rv = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(expr, &rv));
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              a_type_ptr etp = NULL;
              GCC_GEN_BE_CHECK(strip_expr_typerefs(expr->type, &etp));
              if (rv && etp && etp->kind == tk_pointer && etp->variant.pointer.is_reference) {
                  *out_lval = gcc_jit_rvalue_dereference(rv, loc);
                  return GCC_GEN_BE_SUCCESS;
              }
              if (rv && cblock) {
                  gcc_jit_function *mfn = gcc_jit_block_get_function(cblock);
                  gcc_jit_lvalue *mat = gcc_jit_function_new_local(
                      mfn, loc, gcc_jit_rvalue_get_type(rv), "__mat_node_lval");
                  gcc_jit_block_add_assignment(cblock, loc, mat, rv);
                  *out_lval = mat;
                  return GCC_GEN_BE_SUCCESS;
              }
              return GCC_GEN_BE_ERROR_INVALID_STATE;
          }
          
      case enk_constant:
          {
              if (expr->variant.constant.ptr) {
                  a_constant_ptr con = expr->variant.constant.ptr;
                  gcc_jit_type *ctype = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &ctype));
                  if (con->kind == ck_string && ctype) {
                      void *existing_str_g = NULL;
                      GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, con, &existing_str_g));
                      if (existing_str_g) {
                          *out_lval = (gcc_jit_lvalue *)existing_str_g;
                          return GCC_GEN_BE_SUCCESS;
                      }
                      char gname[128];
                      snprintf(gname, sizeof(gname), "__str_lit_global_%p", (void *)con);
                      gcc_jit_lvalue *g_lval = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, ctype, gname);
                      if (con->variant.string.length > 0 && con->variant.string.value) {
                          gcc_jit_global_set_initializer(g_lval, con->variant.string.value, (size_t)con->variant.string.length);
                      }
                      GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, con, g_lval));
                      *out_lval = g_lval;
                      return GCC_GEN_BE_SUCCESS;
                  }
                  gcc_jit_rvalue *crval = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(con, ctype, &crval));
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (cblock && crval && ctype) {
                      gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, ctype, "const_lval");
                      gcc_jit_block_add_assignment(cblock, loc, tmp, crval);
                      *out_lval = tmp;
                      return GCC_GEN_BE_SUCCESS;
                  }
              }
              *out_lval = NULL;
              return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_UNHANDLED_EXPR, "unhandled constant lvalue");
          }

      default:
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
          if (expr->kind == enk_lowered_eh_construct) {
              gcc_jit_type *ltype = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &ltype));
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (cblock && ltype && ltype != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                  void *cached_addr = NULL;
                  if (expr->variant.lowered_eh.kind == leck_thrown_object_address) {
                      cached_addr = gcc_gen_be_singleton_state.active_thrown_obj_lval;
                      if (!cached_addr) {
                          GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_thrown_obj_addr", &cached_addr));
                      }
                  } else if (expr->variant.lowered_eh.kind == leck_caught_object_address) {
                      cached_addr = gcc_gen_be_singleton_state.active_caught_obj_lval;
                      if (!cached_addr) {
                          GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_caught_obj_addr", &cached_addr));
                      }
                  }
                  gcc_jit_rvalue *exc_ptr = NULL;
                  if (cached_addr) {
                      exc_ptr = gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached_addr);
                  } else {
                      GCC_GEN_BE_CHECK(emit_eh_unwind_exc_rvalue(ctx, cblock, &exc_ptr));
                  }
                  gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, NULL, exc_ptr, gcc_jit_type_get_pointer(ltype));
                  *out_lval = gcc_jit_rvalue_dereference(typed_ptr, NULL);
                  return GCC_GEN_BE_SUCCESS;
              }
              gcc_jit_rvalue *lrval = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(expr, &lrval));
              *out_lval = NULL;
              return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_UNHANDLED_EXPR, "unhandled lowered_eh lvalue");
          }
#endif
          if (expr->kind == enk_builtin_choose_expr) {
              an_expr_node_ptr op_cond = expr->variant.builtin_choose_expr.operands;
              an_expr_node_ptr op_true = op_cond ? op_cond->next : NULL;
              an_expr_node_ptr op_false = op_true ? op_true->next : NULL;
              an_expr_node_ptr chosen = expr->variant.builtin_choose_expr.choose_first ? op_true : op_false;
              if (!chosen) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
              return gcc_gen_be_lower_expr_lvalue(chosen, out_lval);
          }
          *out_lval = NULL;
          return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_UNHANDLED_EXPR, "unhandled lvalue expression node kind");
  }
}

/**
 * @brief Lowers a bitfield read operation.
 * @details Extracts and aligns a bitfield value.
 * @param[in] expr Non-null bitfield expression to lower.
 * @param[out] out_rval Non-null pointer to receive the lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t gcc_gen_be_lower_bitfield_read(an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    *out_rval = NULL;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_expr(expr, &loc));

    an_expr_operator_kind op = expr->variant.operation.kind;
    an_expr_node_ptr op1 = expr->variant.operation.operands;
    an_expr_node_ptr op2 = op1->next;
    a_field_ptr f = op2->variant.field.ptr;
    
    gcc_jit_rvalue *base_rval = NULL;
    if (op == eok_dot_field) {
        gcc_jit_lvalue *base_lval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &base_lval));
        if (!base_lval) return GCC_GEN_BE_ERROR_INTERNAL;
        base_rval = gcc_jit_lvalue_get_address(base_lval, NULL);
    } else {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &base_rval));
    }
    
    gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
    gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
    gcc_jit_rvalue *base_char_ptr = gcc_jit_context_new_cast(ctx, loc, base_rval, char_ptr_type);

    gcc_jit_type *word_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(f->type, &word_type));
    gcc_jit_type *u64_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
    gcc_jit_type *i64_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG);
    gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);

    unsigned int rem = (unsigned int)f->offset_bit_remainder;
    unsigned int width = (unsigned int)f->bit_size;
    if (width == 0U) {
        *out_rval = gcc_jit_context_zero(ctx, word_type);
        return GCC_GEN_BE_SUCCESS;
    }
    if (width > 64U) width = 64U;
    unsigned int num_bytes = (rem + width + 7U) / 8U;

    gcc_jit_rvalue *acc_u64 = gcc_jit_context_zero(ctx, u64_type);
    for (unsigned int b = 0U; b < num_bytes; ++b) {
        gcc_jit_rvalue *b_off = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)((unsigned long)f->offset + (unsigned long)b));
        gcc_jit_lvalue *b_lval = gcc_jit_context_new_array_access(ctx, loc, base_char_ptr, b_off);
        gcc_jit_rvalue *b_u64 = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_as_rvalue(b_lval), u64_type);
        if (b == 0U) {
            if (rem > 0U) {
                gcc_jit_rvalue *rsh = gcc_jit_context_new_rvalue_from_int(ctx, u64_type, (int)rem);
                b_u64 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, u64_type, b_u64, rsh);
            }
        } else {
            unsigned int lsh_bits = (b * 8U) - rem;
            gcc_jit_rvalue *lsh = gcc_jit_context_new_rvalue_from_int(ctx, u64_type, (int)lsh_bits);
            b_u64 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, u64_type, b_u64, lsh);
        }
        acc_u64 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_OR, u64_type, acc_u64, b_u64);
    }

    if (width < 64U) {
        gcc_jit_rvalue *one_u64 = gcc_jit_context_one(ctx, u64_type);
        gcc_jit_rvalue *w_sh = gcc_jit_context_new_rvalue_from_int(ctx, u64_type, (int)width);
        gcc_jit_rvalue *mask_u64 = gcc_jit_context_new_binary_op(
            ctx, loc, GCC_JIT_BINARY_OP_MINUS, u64_type,
            gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, u64_type, one_u64, w_sh),
            one_u64);
        acc_u64 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, u64_type, acc_u64, mask_u64);
    }

    gcc_jit_rvalue *word_val = acc_u64;
    if (f->bit_field_is_signed) {
        gcc_jit_rvalue *acc_i64 = gcc_jit_context_new_cast(ctx, loc, acc_u64, i64_type);
        int shift_amt = 64 - (int)width;
        if (shift_amt > 0) {
            gcc_jit_rvalue *s_amt = gcc_jit_context_new_rvalue_from_int(ctx, i64_type, shift_amt);
            acc_i64 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, i64_type, acc_i64, s_amt);
            acc_i64 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, i64_type, acc_i64, s_amt);
        }
        word_val = (word_type == i64_type) ? acc_i64 : gcc_jit_context_new_cast(ctx, loc, acc_i64, word_type);
    } else if (word_type != u64_type) {
        word_val = gcc_jit_context_new_cast(ctx, loc, acc_u64, word_type);
    }
    
    *out_rval = word_val;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers a bitfield assignment operation.
 * @details Reads, masks, shifts, and stores a new bitfield value.
 * @param[in] bf_expr The bitfield expression.
 * @param[in] assign_op The assignment operator.
 * @param[in] rhs_expr The RHS expression.
 * @param[out] out_rval The lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t gcc_gen_be_lower_bitfield_assignment(an_expr_node_ptr bf_expr, an_expr_operator_kind assign_op, an_expr_node_ptr rhs_expr, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    *out_rval = NULL;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_expr(bf_expr, &loc));

    an_expr_operator_kind op = bf_expr->variant.operation.kind;
    an_expr_node_ptr op1 = bf_expr->variant.operation.operands;
    an_expr_node_ptr op2 = op1->next;
    a_field_ptr f = op2->variant.field.ptr;
    
    gcc_jit_rvalue *base_rval = NULL;
    if (op == eok_dot_field) {
        gcc_jit_lvalue *base_lval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &base_lval));
        if (!base_lval) return GCC_GEN_BE_ERROR_INTERNAL;
        base_rval = gcc_jit_lvalue_get_address(base_lval, NULL);
    } else {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &base_rval));
    }
    
    gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
    gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
    gcc_jit_rvalue *base_char_ptr = gcc_jit_context_new_cast(ctx, loc, base_rval, char_ptr_type);

    gcc_jit_type *word_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(f->type, &word_type));
    gcc_jit_type *u64_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
    gcc_jit_type *i64_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG);
    gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));

    gcc_jit_rvalue *rhs_val = NULL;
    if (rhs_expr) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(rhs_expr, &rhs_val));
        rhs_val = gcc_jit_context_new_cast(ctx, loc, rhs_val, word_type);
    } else {
        rhs_val = gcc_jit_context_new_rvalue_from_int(ctx, word_type, 1);
    }

    gcc_jit_rvalue *current_bf_val = NULL;
    if (assign_op != eok_assign && assign_op != eok_bassign) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_bitfield_read(bf_expr, &current_bf_val));
        enum gcc_jit_binary_op jit_op = GCC_JIT_BINARY_OP_PLUS;
        GCC_GEN_BE_CHECK(map_compound_assign_binary_op(assign_op, &jit_op));
        rhs_val = gcc_jit_context_new_binary_op(ctx, loc, jit_op, word_type, current_bf_val, rhs_val);
    }

    unsigned int rem = (unsigned int)f->offset_bit_remainder;
    unsigned int width = (unsigned int)f->bit_size;
    if (width > 64U) width = 64U;

    gcc_jit_rvalue *rhs_u64 = (word_type == u64_type) ? rhs_val : gcc_jit_context_new_cast(ctx, loc, rhs_val, u64_type);
    if (width > 0U && width < 64U) {
        gcc_jit_rvalue *one_u64 = gcc_jit_context_one(ctx, u64_type);
        gcc_jit_rvalue *w_sh = gcc_jit_context_new_rvalue_from_int(ctx, u64_type, (int)width);
        gcc_jit_rvalue *mask_u64 = gcc_jit_context_new_binary_op(
            ctx, loc, GCC_JIT_BINARY_OP_MINUS, u64_type,
            gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, u64_type, one_u64, w_sh),
            one_u64);
        rhs_u64 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, u64_type, rhs_u64, mask_u64);
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (cblock && width > 0U) {
        unsigned int num_bytes = (rem + width + 7U) / 8U;
        gcc_jit_function *cur_fn = gcc_jit_block_get_function(cblock);
        gcc_jit_lvalue *val_slot = gcc_jit_function_new_local(cur_fn, loc, u64_type, "bf_write_u64");
        gcc_jit_block_add_assignment(cblock, loc, val_slot, rhs_u64);
        rhs_u64 = gcc_jit_lvalue_as_rvalue(val_slot);

        for (unsigned int b = 0U; b < num_bytes; ++b) {
            unsigned int bit_lo = (b == 0U) ? rem : 0U;
            unsigned int rem_plus_w = rem + width;
            unsigned int byte_end = (b + 1U) * 8U;
            unsigned int bit_hi = (rem_plus_w < byte_end) ? (rem_plus_w - (b * 8U)) : 8U;
            unsigned int byte_mask_u = (((1U << (bit_hi - bit_lo)) - 1U) << bit_lo) & 0xFFU;

            gcc_jit_rvalue *b_off = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)((unsigned long)f->offset + (unsigned long)b));
            gcc_jit_lvalue *b_lval = gcc_jit_context_new_array_access(ctx, loc, base_char_ptr, b_off);
            gcc_jit_rvalue *part_u64 = rhs_u64;
            if (b == 0U) {
                if (rem > 0U) {
                    part_u64 = gcc_jit_context_new_binary_op(
                        ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, u64_type, part_u64,
                        gcc_jit_context_new_rvalue_from_int(ctx, u64_type, (int)rem));
                }
            } else {
                unsigned int rsh_bits = (b * 8U) - rem;
                part_u64 = gcc_jit_context_new_binary_op(
                    ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, u64_type, part_u64,
                    gcc_jit_context_new_rvalue_from_int(ctx, u64_type, (int)rsh_bits));
            }
            gcc_jit_rvalue *part_u8 = gcc_jit_context_new_cast(ctx, loc, part_u64, char_type);
            gcc_jit_rvalue *ins_u8 = gcc_jit_context_new_binary_op(
                ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, char_type, part_u8,
                gcc_jit_context_new_rvalue_from_int(ctx, char_type, (int)byte_mask_u));
            if (byte_mask_u == 0xFFU) {
                gcc_jit_block_add_assignment(cblock, loc, b_lval, ins_u8);
            } else {
                gcc_jit_rvalue *old_u8 = gcc_jit_lvalue_as_rvalue(b_lval);
                gcc_jit_rvalue *clr_u8 = gcc_jit_context_new_binary_op(
                    ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, char_type, old_u8,
                    gcc_jit_context_new_rvalue_from_int(ctx, char_type, (int)((~byte_mask_u) & 0xFFU)));
                gcc_jit_rvalue *new_u8 = gcc_jit_context_new_binary_op(
                    ctx, loc, GCC_JIT_BINARY_OP_BITWISE_OR, char_type, clr_u8, ins_u8);
                gcc_jit_block_add_assignment(cblock, loc, b_lval, new_u8);
            }
        }
    }

    if (assign_op == eok_post_incr || assign_op == eok_post_decr) {
        *out_rval = current_bf_val;
        return GCC_GEN_BE_SUCCESS;
    }

    if (f->bit_field_is_signed) {
        gcc_jit_rvalue *res_i64 = gcc_jit_context_new_cast(ctx, loc, rhs_u64, i64_type);
        int shift_amt = 64 - (int)width;
        if (shift_amt > 0) {
            gcc_jit_rvalue *s_amt = gcc_jit_context_new_rvalue_from_int(ctx, i64_type, shift_amt);
            res_i64 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, i64_type, res_i64, s_amt);
            res_i64 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, i64_type, res_i64, s_amt);
        }
        *out_rval = (word_type == i64_type) ? res_i64 : gcc_jit_context_new_cast(ctx, loc, res_i64, word_type);
    } else {
        *out_rval = (word_type == u64_type) ? rhs_u64 : gcc_jit_context_new_cast(ctx, loc, rhs_u64, word_type);
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Helper to cast a value between pointer, integer, floating-point, or vector types.
 * @details Performs a safe pointer/scalar cast or vector bitcast (`gcc_jit_context_new_bitcast`).
 * @param[in] ctx The libgccjit context.
 * @param[in] val The value to cast.
 * @param[in] dest_type The destination type.
 * @param[out] out_rval The casted value.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t safe_ptr_or_int_cast(gcc_jit_context *ctx, gcc_jit_rvalue *val, gcc_jit_type *dest_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (!val || !dest_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_type *src_type = gcc_jit_rvalue_get_type(val);
    if (src_type == dest_type) {
        *out_rval = val;
        return GCC_GEN_BE_SUCCESS;
    }
    if (gcc_jit_type_is_pointer(src_type) != NULL && gcc_jit_type_is_pointer(dest_type) != NULL) {
        *out_rval = gcc_jit_context_new_cast(ctx, NULL, val, dest_type);
        return GCC_GEN_BE_SUCCESS;
    }
    if (gcc_jit_type_is_pointer(dest_type) != NULL) {
        gcc_jit_type *uptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG);
        gcc_jit_rvalue *uval = (src_type == uptr_type) ? val : gcc_jit_context_new_cast(ctx, NULL, val, uptr_type);
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (cblock) {
            gcc_jit_function *func = gcc_jit_block_get_function(cblock);
            gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, uptr_type, "int2ptr_tmp");
            gcc_jit_block_add_assignment(cblock, NULL, tmp, uval);
            gcc_jit_rvalue *addr = gcc_jit_lvalue_get_address(tmp, NULL);
            gcc_jit_rvalue *dptr = gcc_jit_context_new_cast(ctx, NULL, addr, gcc_jit_type_get_pointer(dest_type));
            *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(dptr, NULL));
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_type *char_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
        gcc_jit_type *char_ptr_t = gcc_jit_type_get_pointer(char_t);
        gcc_jit_rvalue *null_cp = gcc_jit_context_null(ctx, char_ptr_t);
        gcc_jit_lvalue *elem = gcc_jit_context_new_array_access(ctx, NULL, null_cp, uval);
        gcc_jit_rvalue *addr = gcc_jit_lvalue_get_address(elem, NULL);
        *out_rval = (dest_type == char_ptr_t) ? addr : gcc_jit_context_new_cast(ctx, NULL, addr, dest_type);
        return GCC_GEN_BE_SUCCESS;
    }
    if (gcc_jit_type_is_pointer(src_type) != NULL) {
        static const enum gcc_jit_types k_int_scalars[] = {
            GCC_JIT_TYPE_BOOL, GCC_JIT_TYPE_CHAR, GCC_JIT_TYPE_SIGNED_CHAR, GCC_JIT_TYPE_UNSIGNED_CHAR,
            GCC_JIT_TYPE_SHORT, GCC_JIT_TYPE_UNSIGNED_SHORT, GCC_JIT_TYPE_INT, GCC_JIT_TYPE_UNSIGNED_INT,
            GCC_JIT_TYPE_LONG, GCC_JIT_TYPE_UNSIGNED_LONG, GCC_JIT_TYPE_LONG_LONG, GCC_JIT_TYPE_UNSIGNED_LONG_LONG,
            GCC_JIT_TYPE_FLOAT, GCC_JIT_TYPE_DOUBLE, GCC_JIT_TYPE_LONG_DOUBLE,
            GCC_JIT_TYPE_UINT8_T, GCC_JIT_TYPE_UINT16_T, GCC_JIT_TYPE_UINT32_T, GCC_JIT_TYPE_UINT64_T, GCC_JIT_TYPE_UINT128_T,
            GCC_JIT_TYPE_INT8_T, GCC_JIT_TYPE_INT16_T, GCC_JIT_TYPE_INT32_T, GCC_JIT_TYPE_INT64_T, GCC_JIT_TYPE_INT128_T,
            GCC_JIT_TYPE_SIZE_T
        };
        a_boolean dst_is_scalar = FALSE;
        for (size_t si = 0; si < sizeof(k_int_scalars) / sizeof(k_int_scalars[0]); ++si) {
            if (dest_type == gcc_jit_context_get_type(ctx, k_int_scalars[si])) {
                dst_is_scalar = TRUE;
                break;
            }
        }
        if (!dst_is_scalar && dest_type != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
            gcc_jit_rvalue *aptr = gcc_jit_context_new_cast(ctx, NULL, val, gcc_jit_type_get_pointer(dest_type));
            *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(aptr, NULL));
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_type *uptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG);
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (cblock) {
            gcc_jit_function *func = gcc_jit_block_get_function(cblock);
            gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, src_type, "ptr2int_tmp");
            gcc_jit_block_add_assignment(cblock, NULL, tmp, val);
            gcc_jit_rvalue *addr = gcc_jit_lvalue_get_address(tmp, NULL);
            gcc_jit_rvalue *uptr = gcc_jit_context_new_cast(ctx, NULL, addr, gcc_jit_type_get_pointer(uptr_type));
            gcc_jit_rvalue *uval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(uptr, NULL));
            *out_rval = (dest_type == uptr_type) ? uval : gcc_jit_context_new_cast(ctx, NULL, uval, dest_type);
            return GCC_GEN_BE_SUCCESS;
        }
        if (p_gcc_jit_context_new_bitcast) {
            gcc_jit_rvalue *uval = p_gcc_jit_context_new_bitcast(ctx, NULL, val, uptr_type);
            *out_rval = (dest_type == uptr_type) ? uval : gcc_jit_context_new_cast(ctx, NULL, uval, dest_type);
            return GCC_GEN_BE_SUCCESS;
        }
        return gcc_gen_be_get_zero_rvalue(NULL, dest_type, out_rval);
    }
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    if (dest_type == void_t) {
        *out_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    if (src_type == void_t) {
        gcc_jit_block *vblk = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&vblk));
        if (vblk) {
            gcc_jit_block_add_eval(vblk, NULL, val);
        }
        return gcc_gen_be_get_zero_rvalue(NULL, dest_type, out_rval);
    }
    if (gcc_jit_type_is_struct(dest_type) == NULL && gcc_jit_type_is_struct(src_type) == NULL) {
        static const enum gcc_jit_types k_scalars[] = {
            GCC_JIT_TYPE_BOOL, GCC_JIT_TYPE_CHAR, GCC_JIT_TYPE_SIGNED_CHAR, GCC_JIT_TYPE_UNSIGNED_CHAR,
            GCC_JIT_TYPE_SHORT, GCC_JIT_TYPE_UNSIGNED_SHORT, GCC_JIT_TYPE_INT, GCC_JIT_TYPE_UNSIGNED_INT,
            GCC_JIT_TYPE_LONG, GCC_JIT_TYPE_UNSIGNED_LONG, GCC_JIT_TYPE_LONG_LONG, GCC_JIT_TYPE_UNSIGNED_LONG_LONG,
            GCC_JIT_TYPE_FLOAT, GCC_JIT_TYPE_DOUBLE, GCC_JIT_TYPE_LONG_DOUBLE,
            GCC_JIT_TYPE_UINT8_T, GCC_JIT_TYPE_UINT16_T, GCC_JIT_TYPE_UINT32_T, GCC_JIT_TYPE_UINT64_T, GCC_JIT_TYPE_UINT128_T,
            GCC_JIT_TYPE_INT8_T, GCC_JIT_TYPE_INT16_T, GCC_JIT_TYPE_INT32_T, GCC_JIT_TYPE_INT64_T, GCC_JIT_TYPE_INT128_T,
            GCC_JIT_TYPE_SIZE_T
        };
        a_boolean src_scalar = FALSE;
        a_boolean dst_scalar = FALSE;
        for (size_t si = 0; si < sizeof(k_scalars) / sizeof(k_scalars[0]); ++si) {
            gcc_jit_type *st = gcc_jit_context_get_type(ctx, k_scalars[si]);
            if (src_type == st) src_scalar = TRUE;
            if (dest_type == st) dst_scalar = TRUE;
        }
        static const int k_widths[] = { 1, 2, 4, 8, 16 };
        for (size_t wi = 0; wi < 5; ++wi) {
            for (int sg = 0; sg <= 1; ++sg) {
                gcc_jit_type *it = gcc_jit_context_get_int_type(ctx, k_widths[wi], sg);
                if (src_type == it) src_scalar = TRUE;
                if (dest_type == it) dst_scalar = TRUE;
            }
        }
        if (!src_scalar || !dst_scalar) {
            gcc_jit_block *cblk = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblk));
            if (cblk) {
                gcc_jit_function *cfn = gcc_jit_block_get_function(cblk);
                gcc_jit_type *buf_t = gcc_jit_context_new_array_type(
                    ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR), 64);
                gcc_jit_lvalue *bslot = gcc_jit_function_new_local(cfn, NULL, buf_t, "__vec_cast_buf");
                gcc_jit_block_add_assignment(cblk, NULL, bslot, gcc_jit_context_new_array_constructor(ctx, NULL, buf_t, 0, NULL));
                gcc_jit_rvalue *sp = gcc_jit_context_new_cast(
                    ctx, NULL, gcc_jit_lvalue_get_address(bslot, NULL), gcc_jit_type_get_pointer(src_type));
                gcc_jit_block_add_assignment(cblk, NULL, gcc_jit_rvalue_dereference(sp, NULL), val);
                gcc_jit_rvalue *dp = gcc_jit_context_new_cast(
                    ctx, NULL, gcc_jit_lvalue_get_address(bslot, NULL), gcc_jit_type_get_pointer(dest_type));
                *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(dp, NULL));
                return GCC_GEN_BE_SUCCESS;
            }
            if (p_gcc_jit_context_new_bitcast) {
                *out_rval = p_gcc_jit_context_new_bitcast(ctx, NULL, val, dest_type);
                return GCC_GEN_BE_SUCCESS;
            }
        }
        *out_rval = gcc_jit_context_new_cast(ctx, NULL, val, dest_type);
        return GCC_GEN_BE_SUCCESS;
    }
    return gcc_gen_be_get_zero_rvalue(NULL, dest_type, out_rval);
}

/**
 * @brief Lowers an address constant.
 *
 * @details Processes a constant address expression into a gcc_jit_rvalue,
 * including non-zero byte offsets on variables, functions, labels, and nested aggregate subobjects.
 *
 * @param[in] con The frontend constant.
 * @param[in] expected_type The expected gcc_jit_type.
 * @param[out] out_rval Pointer to receive the resulting rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t lower_address_constant(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    *out_rval = NULL;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!expected_type) {
        expected_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    }

    gcc_jit_rvalue *base_rval = NULL;
    if (con->variant.address.kind == abk_variable) {
        a_variable_ptr var = con->variant.address.variant.variable;
        void *cached = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached));
        gcc_jit_lvalue *lval = NULL;
        if (cached) {
            lval = (gcc_jit_lvalue *)cached;
        } else if (var) {
            extern gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;
            gcc_gen_be_error_t err = gcc_gen_be_lower_global_variable_decl(var, &lval);
            if (err != GCC_GEN_BE_SUCCESS) lval = NULL;
        }
        if (lval) {
            base_rval = gcc_jit_lvalue_get_address(lval, NULL);
        }
    } else if (con->variant.address.kind == abk_label) {
        gcc_jit_block *lbl_block = NULL;
        gcc_jit_function *lbl_func = NULL;
        if (innermost_function_scope && innermost_function_scope->variant.routine.ptr) {
            void *cached_fn = NULL;
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, innermost_function_scope->variant.routine.ptr, &cached_fn));
            if (cached_fn) {
                lbl_func = (gcc_jit_function *)cached_fn;
            }
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_get_or_create_label_block(con->variant.address.variant.label, lbl_func, &lbl_block));
        gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
        base_rval = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)(uintptr_t)con->variant.address.variant.label);
    } else if (con->variant.address.kind == abk_routine) {
        a_routine_ptr rout = con->variant.address.variant.routine;
        gcc_jit_function *func = NULL;
        if (rout) {
            if (resolve_cached_or_decl_func(rout, &func) != GCC_GEN_BE_SUCCESS) {
                func = NULL;
            }
        }
        if (func) {
            void *abi_fn = NULL;
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, func, &abi_fn));
            if (abi_fn) func = (gcc_jit_function *)abi_fn;
            base_rval = gcc_jit_function_get_address(func, NULL);
        }
    } else if (con->variant.address.kind == abk_constant || con->variant.address.kind == abk_temporary) {
        a_constant_ptr inner_con = con->variant.address.variant.constant;
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (inner_con && inner_con->kind == ck_string &&
            gcc_jit_type_is_pointer(expected_type) != NULL) {
            gcc_jit_type *const_char_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CONST_CHAR_PTR);
            if (expected_type == const_char_ptr_t && con->variant.address.offset == 0) {
                *out_rval = gcc_jit_context_new_string_literal(ctx, (const char *)inner_con->variant.string.value);
                return GCC_GEN_BE_SUCCESS;
            }
            gcc_jit_type *char_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
            gcc_jit_type *uchar_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
            gcc_jit_type *schar_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIGNED_CHAR);
            gcc_jit_type *elem_t = char_t;
            if (expected_type == gcc_jit_type_get_pointer(uchar_t)) elem_t = uchar_t;
            else if (expected_type == gcc_jit_type_get_pointer(schar_t)) elem_t = schar_t;
            size_t slen = (size_t)inner_con->variant.string.length;
            if (slen == 0) slen = 1;
            gcc_jit_type *arr_t = gcc_jit_context_new_array_type(ctx, NULL, elem_t, (int)slen);
            char gname[128];
            snprintf(gname, sizeof(gname), "__abk_str_global_%p", (void *)con);
            void *existing_g = NULL;
            gcc_jit_lvalue *g_lval = NULL;
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, con, &existing_g));
            if (existing_g) {
                g_lval = (gcc_jit_lvalue *)existing_g;
            } else {
                g_lval = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, arr_t, gname);
                if (p_gcc_jit_lvalue_set_alignment) {
                    p_gcc_jit_lvalue_set_alignment(g_lval, 8U);
                }
                if (inner_con->variant.string.length > 0 && inner_con->variant.string.value) {
                    gcc_jit_global_set_initializer(g_lval, inner_con->variant.string.value, (size_t)inner_con->variant.string.length);
                }
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, con, g_lval));
            }
            gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
            gcc_jit_rvalue *idx_rval = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)con->variant.address.offset);
            gcc_jit_lvalue *elem_lval = gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(g_lval), idx_rval);
            gcc_jit_rvalue *str_ptr = gcc_jit_lvalue_get_address(elem_lval, NULL);
            if (gcc_jit_rvalue_get_type(str_ptr) != expected_type) {
                str_ptr = gcc_jit_context_new_cast(ctx, NULL, str_ptr, expected_type);
            }
            *out_rval = str_ptr;
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_type *inner_type = expected_type;
        if (inner_con && inner_con->type) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(inner_con->type, &inner_type));
        }
        gcc_jit_rvalue *inner_val = NULL;
        gcc_gen_be_error_t lower_err = gcc_gen_be_lower_constant_rvalue(inner_con, inner_type, &inner_val);
        if (lower_err != GCC_GEN_BE_SUCCESS) return lower_err;
        if (con->variant.address.kind == abk_temporary && cblock && inner_val && con->variant.address.offset == 0) {
            gcc_jit_function *func = gcc_jit_block_get_function(cblock);
            gcc_jit_lvalue *tmp_lval = gcc_jit_function_new_local(func, NULL, inner_type, "abk_const_tmp");
            gcc_jit_block_add_assignment(cblock, NULL, tmp_lval, inner_val);
            base_rval = gcc_jit_lvalue_get_address(tmp_lval, NULL);
        } else if (inner_val) {
            char gname[128];
            snprintf(gname, sizeof(gname), "__abk_const_global_%p", (void *)con);
            void *existing_g = NULL;
            gcc_jit_lvalue *g_lval = NULL;
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, con, &existing_g));
            if (existing_g) {
                g_lval = (gcc_jit_lvalue *)existing_g;
            } else {
                g_lval = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, inner_type, gname);
                gcc_jit_global_set_initializer_rvalue(g_lval, inner_val);
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, con, g_lval));
            }
            base_rval = gcc_jit_lvalue_get_address(g_lval, NULL);
        } else {
            base_rval = inner_val;
        }
    }
    
    if (!base_rval) {
        return gcc_gen_be_get_zero_rvalue(con->type, expected_type, out_rval);
    }

    if (con->variant.address.offset != 0 ||
        gcc_jit_rvalue_get_type(base_rval) != expected_type) {
        if (con->variant.address.kind == abk_variable) {
            a_variable_ptr var = con->variant.address.variant.variable;
            a_type_ptr vtp = NULL;
            GCC_GEN_BE_CHECK(strip_expr_typerefs(var->type, &vtp));
            void *cached = NULL;
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached));
            gcc_jit_lvalue *var_lval = (gcc_jit_lvalue *)cached;
            gcc_jit_type *lval_type = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(var_lval));
            if (vtp && vtp->kind == tk_array && vtp->variant.array.element_type &&
                gcc_jit_type_is_pointer(lval_type) == NULL &&
                gcc_jit_type_is_struct(lval_type) == NULL) {
                long elem_sz = (long)f_size_of_type(vtp->variant.array.element_type);
                long total_sz = (long)vtp->size;
                long num_elems = (long)vtp->variant.array.variant.number_of_elements;
                if (var->source_corresp.name &&
                    (strncmp(var->source_corresp.name, "_ZTV", 4) == 0 ||
                     strncmp(var->source_corresp.name, "_ZTT", 4) == 0 ||
                     strncmp(var->source_corresp.name, "_ZTI", 4) == 0)) {
                    elem_sz = (long)sizeof(void *);
                } else if (total_sz > 0 && num_elems > 0 && (total_sz / num_elems) > 0) {
                    elem_sz = total_sz / num_elems;
                }
                if (elem_sz > 0 && ((long)con->variant.address.offset % elem_sz) == 0) {
                    gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                    long elem_idx = (long)con->variant.address.offset / elem_sz;
                    gcc_jit_rvalue *idx_rval = gcc_jit_context_new_rvalue_from_long(ctx, long_type, elem_idx);
                    gcc_jit_lvalue *elem_lval = gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(var_lval), idx_rval);
                    gcc_jit_rvalue *elem_addr = gcc_jit_lvalue_get_address(elem_lval, NULL);
                    return safe_ptr_or_int_cast(ctx, elem_addr, expected_type, out_rval);
                }
            } else if (vtp && (vtp->kind == tk_struct || vtp->kind == tk_class)) {
                for (a_field_ptr fld = vtp->variant.class_struct_union.field_list; fld != NULL; fld = fld->next) {
                    if ((long)fld->offset <= (long)con->variant.address.offset &&
                        (long)con->variant.address.offset < (long)fld->offset + (long)f_size_of_type(fld->type)) {
                        void *cached_fld = NULL;
                        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, fld, &cached_fld));
                        if (cached_fld) {
                            gcc_jit_lvalue *fld_lval = gcc_jit_lvalue_access_field(var_lval, NULL, (gcc_jit_field *)cached_fld);
                            long rem_off = (long)con->variant.address.offset - (long)fld->offset;
                            a_type_ptr ftp = NULL;
                            GCC_GEN_BE_CHECK(strip_expr_typerefs(fld->type, &ftp));
                            if (ftp->kind == tk_array && ftp->variant.array.element_type) {
                                long elem_sz = (long)f_size_of_type(ftp->variant.array.element_type);
                                if (elem_sz > 0 && (rem_off % elem_sz) == 0) {
                                    gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                                    gcc_jit_rvalue *idx_rval = gcc_jit_context_new_rvalue_from_long(ctx, long_type, rem_off / elem_sz);
                                    gcc_jit_lvalue *elem_lval = gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(fld_lval), idx_rval);
                                    return safe_ptr_or_int_cast(ctx, gcc_jit_lvalue_get_address(elem_lval, NULL), expected_type, out_rval);
                                }
                            }
                            if (rem_off == 0) {
                                return safe_ptr_or_int_cast(ctx, gcc_jit_lvalue_get_address(fld_lval, NULL), expected_type, out_rval);
                            }
                        }
                    }
                }
            }
        }
        if (con->variant.address.offset == 0) {
            return safe_ptr_or_int_cast(ctx, base_rval, expected_type, out_rval);
        }
        gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
        gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
        gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);

        gcc_jit_rvalue *byte_base = NULL;
        GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, base_rval, char_ptr_type, &byte_base));
        gcc_jit_rvalue *off_rval = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)con->variant.address.offset);
        gcc_jit_lvalue *elem_lval = gcc_jit_context_new_array_access(ctx, NULL, byte_base, off_rval);
        gcc_jit_rvalue *offset_addr = gcc_jit_lvalue_get_address(elem_lval, NULL);

        return safe_ptr_or_int_cast(ctx, offset_addr, expected_type, out_rval);
    }
    return safe_ptr_or_int_cast(ctx, base_rval, expected_type, out_rval);
}

/**
 * @brief Populates `fields` and `vals` and constructs the struct/union constant rvalue for `lower_aggregate_constant`.
 * @details Preserves designated initializers, empty-base elision, bitfield packing, and derived-to-base wrapping while
 * the caller owns and frees `fields` and `vals`.
 * @param[in] ctx Non-null `gcc_jit_context`.
 * @param[in] con Non-null `ck_aggregate` frontend constant.
 * @param[in] con_tp Unwrapped `a_type_ptr` of `con` (may be NULL).
 * @param[in] type Expected `gcc_jit_type *` (may be NULL).
 * @param[in] struct_type Lowered `gcc_jit_type *` of `con_tp` (may be NULL).
 * @param[in] count Number of aggregate constant slots (`> 0` when `fields`/`vals` are allocated).
 * @param[in,out] fields Pre-allocated `gcc_jit_field *` array of length `count` (or NULL if `count == 0`).
 * @param[in,out] vals Pre-allocated `gcc_jit_rvalue *` array of length `count` (or NULL if `count == 0`).
 * @param[out] out_rval Non-null pointer to receive the lowered aggregate rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t lower_struct_aggregate_constant_inner(
    gcc_jit_context *ctx,
    a_constant_ptr con,
    a_type_ptr con_tp,
    gcc_jit_type *type,
    gcc_jit_type *struct_type,
    size_t count,
    gcc_jit_field **fields,
    gcc_jit_rvalue **vals,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (count > 0) {
        size_t idx = 0;
        unsigned char last_bf_byte_val = 0U;
        a_base_class_ptr b_cur = NULL;
        a_field_ptr f = NULL;
        if (con_tp && (con_tp->kind == tk_struct || con_tp->kind == tk_union || con_tp->kind == tk_class)) {
            a_boolean has_lowered_base_fields = FALSE;
            for (a_field_ptr sf = con_tp->variant.class_struct_union.field_list; sf != NULL; sf = sf->next) {
                if (sf->source_corresp.name && strncmp(sf->source_corresp.name, "__b_", 4) == 0) {
                    has_lowered_base_fields = TRUE;
                    break;
                }
#ifndef EDG_GCC_BE_ISOLATED_TEST
                if (sf->is_lowered_base_class) {
                    has_lowered_base_fields = TRUE;
                    break;
                }
#endif
            }
            if (!has_lowered_base_fields && con_tp->variant.class_struct_union.extra_info) {
                b_cur = con_tp->variant.class_struct_union.extra_info->direct_base_classes;
                while (b_cur && b_cur->is_virtual) b_cur = b_cur->next;
            }
            if (!b_cur) {
                f = con_tp->variant.class_struct_union.field_list;
            }
        }

        for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
            a_constant_ptr val_c = c;
            if (c->kind == ck_designator) {
                if (c->variant.designator.is_field_designator && !c->variant.designator.is_generic) {
                    b_cur = NULL;
                    f = c->variant.designator.variant.field;
                }
                c = c->next;
                val_c = c;
                if (!val_c) break;
            }
        retry_subobject:
            while (!b_cur && f && (f->is_bit_field || f->bit_size > 0) &&
                   (f->bit_size == 0 || f->source_corresp.name == NULL)) {
                f = f->next;
            }

            gcc_jit_field *jit_f = NULL;
            gcc_jit_type *f_type = NULL;
            a_type_ptr sub_tp = NULL;
            a_boolean skip_unmapped_empty = FALSE;
            if (b_cur && val_c->kind == ck_aggregate) {
                void *cached_bf = NULL;
                GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, b_cur, &cached_bf));
                if (cached_bf) {
                    jit_f = (gcc_jit_field *)cached_bf;
                } else if (b_cur->is_optimized_empty_base) {
                    skip_unmapped_empty = TRUE;
                }
                sub_tp = b_cur->type;
                if (b_cur->type) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(b_cur->type, &f_type));
                }
            } else {
                if (b_cur) {
                    b_cur = NULL;
                    f = con_tp->variant.class_struct_union.field_list;
                }
                if (f) {
                    void *cached = NULL;
                    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, f, &cached));
                    if (cached) {
                        jit_f = (gcc_jit_field *)cached;
                    } else if (f->is_optimized_empty_class) {
                        skip_unmapped_empty = TRUE;
                    }
                    sub_tp = f->type;
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(f->type, &f_type));
                }
            }
            if (skip_unmapped_empty) {
                a_type_ptr sub_ut = NULL;
                a_type_ptr val_ut = NULL;
                GCC_GEN_BE_CHECK(strip_expr_typerefs(sub_tp, &sub_ut));
                GCC_GEN_BE_CHECK(strip_expr_typerefs(val_c->type, &val_ut));
                if (b_cur) {
                    b_cur = b_cur->next;
                    while (b_cur && b_cur->is_virtual) b_cur = b_cur->next;
                    if (!b_cur) {
                        f = con_tp->variant.class_struct_union.field_list;
                    }
                } else {
                    f = f->next;
                }
                if (val_ut != sub_ut) {
                    goto retry_subobject;
                }
                count--;
                continue;
            }
            if (!jit_f && val_c->kind == ck_aggregate && val_c->variant.aggregate.field_or_base.base != NULL) {
                a_base_class_ptr desig_bcp = val_c->variant.aggregate.field_or_base.base;
                void *cached_bf = NULL;
                GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, desig_bcp, &cached_bf));
                if (cached_bf) {
                    jit_f = (gcc_jit_field *)cached_bf;
                    if (desig_bcp->type) {
                        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(desig_bcp->type, &f_type));
                    }
                }
            }
            if (!jit_f && struct_type && gcc_jit_type_is_struct(struct_type) != NULL) {
                gcc_jit_struct *st = gcc_jit_type_is_struct(struct_type);
                if (idx < (size_t)gcc_jit_struct_get_field_count(st)) {
                    jit_f = gcc_jit_struct_get_field(st, idx);
                }
            }
            if (!f_type && val_c->type) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(val_c->type, &f_type));
            }
            if (!f_type && jit_f) {
                f_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
            }
            if (!f_type) f_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

            if (f && (f->is_bit_field || f->bit_size > 0) && val_c->kind == ck_integer) {
                void *cached_bf_ft = NULL;
                if (jit_f) {
                    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_TYPE, jit_f, &cached_bf_ft));
                    if (cached_bf_ft) {
                        f_type = (gcc_jit_type *)cached_bf_ft;
                    }
                }
                gcc_jit_type *uchar_jit_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
                a_boolean is_byte_bf_container = (f_type == uchar_jit_t && struct_type && gcc_jit_type_is_struct(struct_type) != NULL) ? TRUE : FALSE;
                unsigned long unit_sz = is_byte_bf_container ? 1UL : (unsigned long)f_size_of_type(f->type);
                if (unit_sz == 0) unit_sz = 4;
                if (!cached_bf_ft && !is_byte_bf_container && unit_sz < 8UL &&
                    ((unsigned long)f->offset_bit_remainder + (unsigned long)f->bit_size) > (unit_sz * 8UL) &&
                    con_tp && (f->offset + 8UL) <= con_tp->size) {
                    f_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
                    unit_sz = 8UL;
                }
                unsigned long container_base_off = is_byte_bf_container
                    ? (unsigned long)f->offset
                    : (((unsigned long)f->offset / unit_sz) * unit_sz);
                size_t bf_cap = count + 16U;
                unsigned long *bf_vals = NULL;
                unsigned int *bf_shifts = NULL;
                unsigned int *bf_widths = NULL;
                int bf_count = 0;
                unsigned int max_bit_end = 0U;
                GCC_GEN_BE_CHECK(gcc_gen_be_alloc(bf_cap * sizeof(unsigned long), (void **)&bf_vals));
                gcc_gen_be_error_t bf_err = GCC_GEN_BE_SUCCESS;
                GCC_GEN_BE_CHECK_CLEANUP(
                    gcc_gen_be_alloc(bf_cap * sizeof(unsigned int), (void **)&bf_shifts), bf_err, bf_pack_fail);
                GCC_GEN_BE_CHECK_CLEANUP(
                    gcc_gen_be_alloc(bf_cap * sizeof(unsigned int), (void **)&bf_widths), bf_err, bf_pack_fail);
                for (;;) {
                    a_boolean bf_ovf = FALSE;
                    unsigned long bf_val = 0UL;
#ifndef EDG_GCC_BE_ISOLATED_TEST
                    if (int_constant_is_signed(val_c)) {
                        bf_val = (unsigned long)(long)value_of_integer_constant(val_c, &bf_ovf);
                    } else {
                        bf_val = (unsigned long)unsigned_value_of_integer_constant(val_c, &bf_ovf);
                    }
#else
                    bf_val = (unsigned long)unsigned_value_of_integer_constant(val_c, &bf_ovf);
#endif
                    unsigned int bit_shift = (unsigned int)(((unsigned long)f->offset - container_base_off) * 8UL) +
                                             (unsigned int)f->offset_bit_remainder;
                    bf_vals[bf_count] = bf_val;
                    bf_shifts[bf_count] = bit_shift;
                    bf_widths[bf_count] = (unsigned int)f->bit_size;
                    if (bit_shift + (unsigned int)f->bit_size > max_bit_end) {
                        max_bit_end = bit_shift + (unsigned int)f->bit_size;
                    }
                    bf_count++;

                    a_constant_ptr next_c = c->next;
                    a_field_ptr next_f = f->next;
                    if (next_c && next_c->kind == ck_designator) {
                        if (next_c->variant.designator.is_field_designator && !next_c->variant.designator.is_generic) {
                            next_f = next_c->variant.designator.variant.field;
                        }
                        next_c = next_c->next;
                    } else {
                        while (next_f && (next_f->is_bit_field || next_f->bit_size > 0) &&
                               (next_f->bit_size == 0 || next_f->source_corresp.name == NULL)) {
                            next_f = next_f->next;
                        }
                    }
                    void *next_cached = NULL;
                    if (next_f && (next_f->is_bit_field || next_f->bit_size > 0) && next_c && next_c->kind == ck_integer) {
                        GCC_GEN_BE_CHECK_CLEANUP(
                            cache_lookup(GCC_GEN_BE_CACHE_FIELD, next_f, &next_cached), bf_err, bf_pack_fail);
                    }
                    if (next_cached != NULL && (gcc_jit_field *)next_cached == jit_f) {
                        c = (c->next->kind == ck_designator) ? c->next->next : c->next;
                        val_c = c;
                        f = next_f;
                        count--;
                    } else {
                        break;
                    }
                }
                goto bf_pack_ok;
            bf_pack_fail:;
                free(bf_widths);
                free(bf_shifts);
                free(bf_vals);
                return bf_err;
            bf_pack_ok:
                if (is_byte_bf_container) {
                    unsigned char bf_bytes[32];
                    memset(bf_bytes, 0, sizeof(bf_bytes));
                    for (int bi = 0; bi < bf_count; ++bi) {
                        unsigned int w = bf_widths[bi];
                        unsigned int sh = bf_shifts[bi];
                        unsigned long v = bf_vals[bi];
                        for (unsigned int bit_i = 0U; bit_i < w; ++bit_i) {
                            if ((v & (1UL << bit_i)) != 0UL) {
                                unsigned int dst_bit = sh + bit_i;
                                bf_bytes[dst_bit / 8U] |= (unsigned char)(1U << (dst_bit % 8U));
                            }
                        }
                    }
                    free(bf_widths);
                    free(bf_shifts);
                    free(bf_vals);
                    gcc_jit_struct *bf_st = gcc_jit_type_is_struct(struct_type);
                    size_t bf_st_fc = gcc_jit_struct_get_field_count(bf_st);
                    size_t start_fi = bf_st_fc;
                    for (size_t fi = 0; fi < bf_st_fc; ++fi) {
                        if (gcc_jit_struct_get_field(bf_st, fi) == jit_f) {
                            start_fi = fi;
                            break;
                        }
                    }
                    unsigned int span_bytes = (max_bit_end + 7U) / 8U;
                    for (unsigned int b = 0U; b < span_bytes && (start_fi + b) < bf_st_fc; ++b) {
                        gcc_jit_field *target_f = gcc_jit_struct_get_field(bf_st, start_fi + b);
                        if (idx > 0U && fields[idx - 1U] == target_f) {
                            unsigned char merged_b = (unsigned char)(last_bf_byte_val | bf_bytes[b]);
                            vals[idx - 1U] = gcc_jit_context_new_rvalue_from_int(ctx, uchar_jit_t, (int)merged_b);
                            last_bf_byte_val = merged_b;
                        } else {
                            fields[idx] = target_f;
                            vals[idx] = gcc_jit_context_new_rvalue_from_int(ctx, uchar_jit_t, (int)bf_bytes[b]);
                            last_bf_byte_val = bf_bytes[b];
                            idx++;
                        }
                    }
                    f = f->next;
                } else {
                    unsigned long combined_word = 0UL;
                    bf_err = gcc_gen_be_pack_bitfield_constants(bf_vals, bf_shifts, bf_widths, bf_count, &combined_word);
                    free(bf_widths);
                    free(bf_shifts);
                    free(bf_vals);
                    GCC_GEN_BE_CHECK(bf_err);
                    fields[idx] = jit_f;
                    vals[idx] = gcc_jit_context_new_rvalue_from_long(ctx, f_type, (long)combined_word);
                    idx++;
                    f = f->next;
                }
            } else {
                fields[idx] = jit_f;
                if (struct_type && gcc_jit_type_is_struct(struct_type) != NULL && jit_f != NULL) {
                    gcc_jit_rvalue *z_st = gcc_jit_context_new_struct_constructor(ctx, NULL, struct_type, 0, NULL, NULL);
                    gcc_jit_rvalue *z_fa = gcc_jit_rvalue_access_field(z_st, NULL, jit_f);
                    if (z_fa) {
                        f_type = gcc_jit_rvalue_get_type(z_fa);
                    }
                }
                a_constant_ptr inner_val_c = val_c;
                while (inner_val_c->kind == ck_aggregate &&
                       !b_cur && inner_val_c->variant.aggregate.field_or_base.base == NULL &&
                       inner_val_c->variant.aggregate.first_constant && !inner_val_c->variant.aggregate.first_constant->next &&
                       gcc_jit_type_is_pointer(f_type) != NULL) {
                    inner_val_c = inner_val_c->variant.aggregate.first_constant;
                }
                a_type_ptr f_arr_tp = NULL;
                if (f && f->type) {
                    GCC_GEN_BE_CHECK(strip_expr_typerefs(f->type, &f_arr_tp));
                }
                if (inner_val_c->kind == ck_string && f_arr_tp && f_arr_tp->kind == tk_array) {
                    a_type_ptr s_etp = NULL;
                    GCC_GEN_BE_CHECK(strip_expr_typerefs(f_arr_tp->variant.array.element_type, &s_etp));
                    gcc_jit_type *s_ejt = NULL;
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(s_etp, &s_ejt));
                    size_t bpe = (s_etp && s_etp->kind == tk_integer && f_size_of_type(s_etp) > 0)
                        ? (size_t)f_size_of_type(s_etp) : 1U;
                    size_t n_el = (size_t)f_arr_tp->variant.array.variant.number_of_elements;
                    if (n_el == 0U) n_el = ((size_t)inner_val_c->variant.string.length / bpe);
                    if (n_el == 0U) n_el = 1U;
                    gcc_jit_rvalue **s_vals = NULL;
                    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(n_el * sizeof(gcc_jit_rvalue *), (void **)&s_vals));
                    const unsigned char *s_bytes = (const unsigned char *)inner_val_c->variant.string.value;
                    size_t s_len = (size_t)inner_val_c->variant.string.length;
                    for (size_t si = 0; si < n_el; ++si) {
                        long long ch_v = 0;
                        size_t off_b = si * bpe;
                        if (s_bytes && off_b < s_len) {
                            for (size_t bi = 0; bi < bpe && (off_b + bi) < s_len; ++bi) {
                                ch_v |= ((long long)s_bytes[off_b + bi]) << (bi * 8);
                            }
                        }
                        s_vals[si] = gcc_jit_context_new_rvalue_from_long(ctx, s_ejt, (long)ch_v);
                    }
                    vals[idx] = gcc_jit_context_new_array_constructor(ctx, NULL, f_type, n_el, s_vals);
                    free(s_vals);
                } else {
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(inner_val_c, f_type, &vals[idx]));
                }
                if (vals[idx] && gcc_jit_rvalue_get_type(vals[idx]) != f_type) {
                    if (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(vals[idx])) != NULL &&
                        gcc_jit_type_is_pointer(f_type) != NULL) {
                        vals[idx] = gcc_jit_context_new_cast(ctx, NULL, vals[idx], f_type);
                    } else if (gcc_jit_type_is_struct(f_type) != NULL &&
                               gcc_jit_struct_get_field_count(gcc_jit_type_is_struct(f_type)) == 1) {
                        gcc_jit_struct *sub_st = gcc_jit_type_is_struct(f_type);
                        gcc_jit_field *sub_f0 = gcc_jit_struct_get_field(sub_st, 0);
                        a_boolean con_is_rec = FALSE;
                        GCC_GEN_BE_CHECK(check_is_record_type(con_tp, &con_is_rec));
                        if (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(vals[idx])) != NULL && con_is_rec) {
                            a_field_ptr sub_edg_f = NULL;
                            a_boolean mem_is_rec = FALSE;
                            a_type_ptr mem_tp = b_cur ? b_cur->type : (f ? f->type : NULL);
                            GCC_GEN_BE_CHECK(check_is_record_type(mem_tp, &mem_is_rec));
                            if (mem_is_rec) {
                                sub_edg_f = mem_tp->variant.class_struct_union.field_list;
                            }
                            if (sub_edg_f && sub_edg_f->type) {
                                gcc_jit_type *sub_f0_type = NULL;
                                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(sub_edg_f->type, &sub_f0_type));
                                if (gcc_jit_rvalue_get_type(vals[idx]) != sub_f0_type) {
                                    vals[idx] = gcc_jit_context_new_cast(ctx, NULL, vals[idx], sub_f0_type);
                                }
                            }
                        }
                        vals[idx] = gcc_jit_context_new_struct_constructor(ctx, NULL, f_type, 1, &sub_f0, &vals[idx]);
                    } else if (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(vals[idx])) != NULL ||
                               gcc_jit_type_is_pointer(f_type) != NULL) {
                        GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, vals[idx], f_type, &vals[idx]));
                    } else if (val_c->kind == ck_aggregate && val_c->variant.aggregate.first_constant &&
                               !val_c->variant.aggregate.first_constant->next) {
                        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(val_c->variant.aggregate.first_constant, f_type, &vals[idx]));
                    }
                }
                idx++;
                if (b_cur) {
                    b_cur = b_cur->next;
                    while (b_cur && b_cur->is_virtual) b_cur = b_cur->next;
                    if (!b_cur) {
                        f = con_tp->variant.class_struct_union.field_list;
                    }
                } else if (f) {
                    f = f->next;
                }
            }
        }
        count = idx;
    }

    if (con_tp && con_tp->kind == tk_union) {
        if (count > 0 && fields[0] && vals[0]) {
            *out_rval = gcc_jit_context_new_union_constructor(ctx, NULL, struct_type ? struct_type : type, fields[0], vals[0]);
            return GCC_GEN_BE_SUCCESS;
        }
        return gcc_gen_be_get_zero_rvalue(con_tp, struct_type ? struct_type : type, out_rval);
    }
    if ((con_tp && (con_tp->kind == tk_struct || con_tp->kind == tk_class)) ||
        (!con_tp && struct_type && gcc_jit_type_is_struct(struct_type) != NULL)) {
        if (!struct_type) struct_type = type;
        if (struct_type && gcc_jit_type_is_struct(struct_type) != NULL) {
            gcc_jit_struct *st = gcc_jit_type_is_struct(struct_type);
            size_t st_fcount = (size_t)gcc_jit_struct_get_field_count(st);
            if (count > 0 && st_fcount == 1 && con_tp &&
                con_tp->variant.class_struct_union.extra_info &&
                con_tp->variant.class_struct_union.extra_info->direct_base_classes &&
                !con_tp->variant.class_struct_union.field_list) {
                gcc_jit_field *outer_f0 = gcc_jit_struct_get_field(st, 0);
                gcc_jit_rvalue *z_outer = gcc_jit_context_new_struct_constructor(ctx, NULL, struct_type, 0, NULL, NULL);
                gcc_jit_rvalue *z_f0 = gcc_jit_rvalue_access_field(z_outer, NULL, outer_f0);
                gcc_jit_type *b0_type = gcc_jit_rvalue_get_type(z_f0);
                if (gcc_jit_type_is_struct(b0_type) != NULL &&
                    (size_t)gcc_jit_struct_get_field_count(gcc_jit_type_is_struct(b0_type)) == count &&
                    vals[0] && gcc_jit_rvalue_get_type(vals[0]) != b0_type) {
                    gcc_jit_struct *b0_st = gcc_jit_type_is_struct(b0_type);
                    for (size_t bi = 0; bi < count; ++bi) {
                        fields[bi] = gcc_jit_struct_get_field(b0_st, bi);
                    }
                    gcc_jit_rvalue *b0_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, b0_type, count, fields, vals);
                    *out_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, struct_type, 1, &outer_f0, &b0_rval);
                    return GCC_GEN_BE_SUCCESS;
                }
            }
        }
        if (struct_type && gcc_jit_type_is_struct(struct_type) != NULL && count > 0) {
            gcc_jit_rvalue *zero_probe = gcc_jit_context_new_struct_constructor(ctx, NULL, struct_type, 0, NULL, NULL);
            a_boolean needs_trimmed_lval_init = FALSE;
            for (size_t ci = 0; ci < count; ++ci) {
                if (!fields[ci] || !vals[ci]) continue;
                gcc_jit_type *exact_ft = gcc_jit_rvalue_get_type(
                    gcc_jit_rvalue_access_field(zero_probe, NULL, fields[ci]));
                gcc_jit_type *cur_vt = gcc_jit_rvalue_get_type(vals[ci]);
                if (cur_vt != exact_ft &&
                    gcc_jit_type_is_struct(cur_vt) != NULL) {
                    needs_trimmed_lval_init = TRUE;
                    break;
                }
            }
            if (needs_trimmed_lval_init) {
                gcc_jit_block *agg_blk = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&agg_blk));
                if (agg_blk) {
                    gcc_jit_function *agg_fn = gcc_jit_block_get_function(agg_blk);
                    gcc_jit_lvalue *agg_tmp = gcc_jit_function_new_local(agg_fn, NULL, struct_type, "agg_trim_tmp");
                    gcc_jit_block_add_assignment(agg_blk, NULL, agg_tmp, zero_probe);
                    for (size_t ci = 0; ci < count; ++ci) {
                        if (!fields[ci] || !vals[ci]) continue;
                        gcc_jit_lvalue *fld_lv = gcc_jit_lvalue_access_field(agg_tmp, NULL, fields[ci]);
                        gcc_jit_type *exact_ft = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(fld_lv));
                        gcc_jit_type *cur_vt = gcc_jit_rvalue_get_type(vals[ci]);
                        if (cur_vt == exact_ft) {
                            gcc_jit_block_add_assignment(agg_blk, NULL, fld_lv, vals[ci]);
                        } else if (gcc_jit_type_is_struct(cur_vt) != NULL) {
                            gcc_jit_rvalue *fld_addr = gcc_jit_lvalue_get_address(fld_lv, NULL);
                            gcc_jit_rvalue *sub_ptr = gcc_jit_context_new_cast(
                                ctx, NULL, fld_addr, gcc_jit_type_get_pointer(cur_vt));
                            gcc_jit_block_add_assignment(agg_blk, NULL, gcc_jit_rvalue_dereference(sub_ptr, NULL), vals[ci]);
                        } else {
                            gcc_jit_rvalue *coerced_v = NULL;
                            GCC_GEN_BE_CHECK(gcc_gen_be_coerce_rvalue(agg_fn, agg_blk, vals[ci], exact_ft, &coerced_v));
                            if (coerced_v) {
                                gcc_jit_block_add_assignment(agg_blk, NULL, fld_lv, coerced_v);
                            }
                        }
                    }
                    *out_rval = gcc_jit_lvalue_as_rvalue(agg_tmp);
                    return GCC_GEN_BE_SUCCESS;
                }
            }
        }
        gcc_jit_rvalue *ctor_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, struct_type, count, fields, vals);
        if (type && struct_type != type && gcc_jit_type_is_struct(type) != NULL) {
            gcc_jit_struct *outer_s = gcc_jit_type_is_struct(type);
            size_t outer_fc = (size_t)gcc_jit_struct_get_field_count(outer_s);
            gcc_jit_rvalue *z_out = gcc_jit_context_new_struct_constructor(ctx, NULL, type, 0, NULL, NULL);
            if (outer_fc == 1) {
                gcc_jit_field *outer_f0 = gcc_jit_struct_get_field(outer_s, 0);
                gcc_jit_rvalue *z_of0 = gcc_jit_rvalue_access_field(z_out, NULL, outer_f0);
                if (gcc_jit_rvalue_get_type(z_of0) == struct_type) {
                    ctor_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, type, 1, &outer_f0, &ctor_rval);
                    *out_rval = ctor_rval;
                    return GCC_GEN_BE_SUCCESS;
                }
            }
            gcc_jit_struct *built_s = gcc_jit_type_is_struct(struct_type);
            size_t built_fc = (size_t)gcc_jit_struct_get_field_count(built_s);
            size_t rep_fc = (outer_fc < built_fc) ? outer_fc : built_fc;
            if (rep_fc > 0U) {
                gcc_jit_field **rep_fs = NULL;
                gcc_jit_rvalue **rep_vs = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_alloc(rep_fc * sizeof(gcc_jit_field *), (void **)&rep_fs));
                gcc_gen_be_error_t rp_err = gcc_gen_be_alloc(rep_fc * sizeof(gcc_jit_rvalue *), (void **)&rep_vs);
                if (rp_err != GCC_GEN_BE_SUCCESS) { free(rep_fs); return rp_err; }
                for (size_t ri = 0; ri < rep_fc; ++ri) {
                    rep_fs[ri] = gcc_jit_struct_get_field(outer_s, ri);
                    rep_vs[ri] = gcc_jit_rvalue_access_field(ctor_rval, NULL, gcc_jit_struct_get_field(built_s, ri));
                }
                ctor_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, type, rep_fc, rep_fs, rep_vs);
                free(rep_vs);
                free(rep_fs);
            } else {
                ctor_rval = z_out;
            }
        }
        *out_rval = ctor_rval;
        return GCC_GEN_BE_SUCCESS;
    }
    if (count == 1 && vals[0]) {
        *out_rval = vals[0];
        return GCC_GEN_BE_SUCCESS;
    }
    return gcc_gen_be_get_zero_rvalue(con->type, type, out_rval);
}

/**
 * @brief Lowers an aggregate constant.
 *
 * @details Processes a constant aggregate into a gcc_jit_rvalue.
 *
 * @param[in] con Non-null frontend constant.
 * @param[in] type The expected gcc_jit_type.
 * @param[out] out_rval Non-null pointer to receive the resulting rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_aggregate_constant(a_constant_ptr con, gcc_jit_type *type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

    a_type_ptr con_tp = NULL;
    GCC_GEN_BE_CHECK(strip_expr_typerefs(con->type, &con_tp));

#if GNU_VECTOR_TYPES_ALLOWED
    if (con_tp && con_tp->kind == tk_vector) {
        gcc_jit_type *vec_type = type;
        a_type_ptr elem_edg_tp = con_tp->variant.vector.element_type;
        a_type_ptr raw_elem_tp = NULL;
        GCC_GEN_BE_CHECK(strip_expr_typerefs(elem_edg_tp, &raw_elem_tp));
        gcc_jit_type *elem_jit_tp = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(elem_edg_tp, &elem_jit_tp));
        size_t elem_sz = raw_elem_tp ? (size_t)raw_elem_tp->size : 0U;
        size_t n_lanes = (elem_sz > 0) ? ((size_t)con_tp->size / elem_sz) : 0;
        gcc_jit_block *vec_cblk = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&vec_cblk));
        if (n_lanes > 0 && (p_gcc_jit_context_new_rvalue_from_vector != NULL || vec_cblk != NULL)) {
            gcc_jit_rvalue **lane_vals = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_alloc(n_lanes * sizeof(gcc_jit_rvalue *), (void **)&lane_vals));
            a_constant_ptr c = con->variant.aggregate.first_constant;
            for (size_t li = 0; li < n_lanes; ++li) {
                while (c && c->kind == ck_designator) c = c->next;
                gcc_gen_be_error_t lerr = GCC_GEN_BE_SUCCESS;
                if (c) {
                    lerr = gcc_gen_be_lower_constant_rvalue(c, elem_jit_tp, &lane_vals[li]);
                    c = c->next;
                } else {
                    lerr = gcc_gen_be_get_zero_rvalue(elem_edg_tp, elem_jit_tp, &lane_vals[li]);
                }
                if (lerr != GCC_GEN_BE_SUCCESS) {
                    free(lane_vals);
                    return lerr;
                }
            }
            gcc_gen_be_error_t berr = gcc_gen_be_build_vector_rvalue(vec_type, elem_jit_tp, n_lanes, lane_vals, out_rval);
            free(lane_vals);
            return berr;
        }
    }
#endif

    if (con_tp && con_tp->kind == tk_array) {
        size_t count = 0;
        size_t max_pos = 0;
        size_t scan_pos = 0;
        a_boolean has_arr_desig = FALSE;
        for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
            if (c->kind == ck_designator) {
                if (!c->variant.designator.is_field_designator && !c->variant.designator.is_generic) {
#ifdef EDG_GCC_BE_ISOLATED_TEST
                    scan_pos = (size_t)c->variant.designator.variant.index;
#else
                    scan_pos = (size_t)c->variant.designator.variant.array_element;
#endif
                    has_arr_desig = TRUE;
                }
                c = c->next;
                if (!c) break;
            }
            size_t step = (c->kind == ck_init_repeat) ? (size_t)c->variant.init_repeat.count : 1U;
            if (scan_pos + step > max_pos) max_pos = scan_pos + step;
            scan_pos += step;
            count += step;
        }
        size_t alloc_len = count;
        if (has_arr_desig) {
            size_t decl_len = (size_t)con_tp->variant.array.variant.number_of_elements;
            alloc_len = (decl_len > max_pos) ? decl_len : max_pos;
        }

        gcc_jit_rvalue **vals = NULL;
        if (alloc_len > 0) {
            GCC_GEN_BE_CHECK(gcc_gen_be_alloc(alloc_len * sizeof(gcc_jit_rvalue *), (void **)&vals));
            gcc_jit_type *elem_type = NULL;
            gcc_gen_be_error_t err = gcc_gen_be_lower_type(con_tp->variant.array.element_type, &elem_type);
            if (err != GCC_GEN_BE_SUCCESS) {
                free(vals);
                return err;
            }
            if (has_arr_desig) {
                for (size_t zi = 0; zi < alloc_len; ++zi) {
                    err = gcc_gen_be_get_zero_rvalue(con_tp->variant.array.element_type, elem_type, &vals[zi]);
                    if (err != GCC_GEN_BE_SUCCESS) {
                        free(vals);
                        return err;
                    }
                }
            }
            size_t idx = 0;
            for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
                if (c->kind == ck_designator) {
                    if (!c->variant.designator.is_field_designator && !c->variant.designator.is_generic) {
#ifdef EDG_GCC_BE_ISOLATED_TEST
                        idx = (size_t)c->variant.designator.variant.index;
#else
                        idx = (size_t)c->variant.designator.variant.array_element;
#endif
                    }
                    c = c->next;
                    if (!c) break;
                }
                if (c->kind == ck_init_repeat) {
                    size_t rep_n = (size_t)c->variant.init_repeat.count;
                    gcc_jit_rvalue *rep_rv = NULL;
                    err = gcc_gen_be_lower_constant_rvalue(c->variant.init_repeat.constant, elem_type, &rep_rv);
                    if (err != GCC_GEN_BE_SUCCESS) {
                        free(vals);
                        return err;
                    }
                    for (size_t ri = 0; ri < rep_n; ++ri) {
                        vals[idx++] = rep_rv;
                    }
                } else {
                    err = gcc_gen_be_lower_constant_rvalue(c, elem_type, &vals[idx]);
                    if (err != GCC_GEN_BE_SUCCESS) {
                        free(vals);
                        return err;
                    }
                    idx++;
                }
            }
        }
        *out_rval = gcc_jit_context_new_array_constructor(ctx, NULL, type, alloc_len, vals);
        free(vals);
        return GCC_GEN_BE_SUCCESS;
    }

    size_t count = 0;
    for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
        if (c->kind == ck_designator) {
            c = c->next;
            if (!c) break;
        }
        count++;
    }

    gcc_jit_type *struct_type = type;
    if (con_tp && (con_tp->kind == tk_struct || con_tp->kind == tk_union || con_tp->kind == tk_class)) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(con_tp, &struct_type));
    }
    gcc_jit_field **fields = NULL;
    gcc_jit_rvalue **vals = NULL;
    if (count > 0) {
        size_t st_fc_extra = 32U;
        if (struct_type && gcc_jit_type_is_struct(struct_type) != NULL) {
            st_fc_extra += gcc_jit_struct_get_field_count(gcc_jit_type_is_struct(struct_type));
        }
        size_t alloc_cap = count + st_fc_extra;
        GCC_GEN_BE_CHECK(gcc_gen_be_alloc(alloc_cap * sizeof(gcc_jit_field *), (void **)&fields));
        gcc_gen_be_error_t aerr = gcc_gen_be_alloc(alloc_cap * sizeof(gcc_jit_rvalue *), (void **)&vals);
        if (aerr != GCC_GEN_BE_SUCCESS) {
            free(fields);
            return aerr;
        }
    }
    gcc_gen_be_error_t ierr = lower_struct_aggregate_constant_inner(
        ctx, con, con_tp, type, struct_type, count, fields, vals, out_rval);
    free(fields);
    free(vals);
    return ierr;
}

gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!out_rval || !con) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;
    if (!expected_type && con->type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(con->type, &expected_type));
    }

    if (con->kind == ck_integer) {
        a_type_ptr ctype = NULL;
        GCC_GEN_BE_CHECK(strip_expr_typerefs(con->type, &ctype));
#if INT128_EXTENSIONS_ALLOWED
        if (ctype && ctype->kind == tk_integer &&
            (ctype->variant.integer.int_kind == ik_int128 ||
             ctype->variant.integer.int_kind == ik_unsigned_int128 ||
             (ctype->size == 16 &&
              (ctype->variant.integer.int_kind == ik_bit_precise ||
               ctype->variant.integer.int_kind == ik_unsigned_bit_precise)))) {
            an_integer_value lo_val = con->variant.integer_value;
            an_integer_value hi_val = con->variant.integer_value;
            an_integer_value mask64;
            make_integer_value_mask(&mask64, 64);
            and_integer_values(&lo_val, &mask64);
            shift_right_integer_value(&hi_val, 64, /*is_signed=*/FALSE, /*sign_extend=*/FALSE);
            and_integer_values(&hi_val, &mask64);
            a_boolean lo_ovf = FALSE;
            a_boolean hi_ovf = FALSE;
            unsigned long lo_u = (unsigned long)unsigned_value_of_integer_value(&lo_val, FALSE, &lo_ovf);
            unsigned long hi_u = (unsigned long)unsigned_value_of_integer_value(&hi_val, FALSE, &hi_ovf);
            gcc_jit_type *ulong_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG);
            gcc_jit_type *arith_128 = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(ctype, &arith_128));
            gcc_jit_rvalue *lo_rval = gcc_jit_context_new_rvalue_from_long(ctx, ulong_type, (long)lo_u);
            lo_rval = gcc_jit_context_new_cast(ctx, NULL, lo_rval, arith_128);
            gcc_jit_rvalue *res_128 = lo_rval;
            if (hi_u != 0) {
                gcc_jit_rvalue *hi_rval = gcc_jit_context_new_rvalue_from_long(ctx, ulong_type, (long)hi_u);
                hi_rval = gcc_jit_context_new_cast(ctx, NULL, hi_rval, arith_128);
                gcc_jit_rvalue *shift_64 = gcc_jit_context_new_rvalue_from_int(ctx, arith_128, 64);
                gcc_jit_rvalue *hi_shifted = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, arith_128, hi_rval, shift_64);
                res_128 = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_OR, arith_128, hi_shifted, lo_rval);
            }
            if (expected_type && expected_type != arith_128) {
                if (lo_u == 0 && hi_u == 0 && gcc_jit_type_is_pointer(expected_type) != NULL) {
                    *out_rval = gcc_jit_context_null(ctx, expected_type);
                } else {
                    GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, res_128, expected_type, out_rval));
                }
            } else {
                *out_rval = res_128;
            }
            return GCC_GEN_BE_SUCCESS;
        }
#endif
        a_boolean local_err = FALSE;
        long val = 0;
        if (!int_constant_is_signed(con)) {
            val = (long)unsigned_value_of_integer_constant(con, &local_err);
        } else {
            val = (long)value_of_integer_constant(con, &local_err);
        }
        a_boolean is_direct_int_type = expected_type != NULL &&
            (!ctype || ctype->kind == tk_integer) &&
            gcc_jit_type_is_pointer(expected_type) == NULL &&
            gcc_jit_type_is_struct(expected_type) == NULL &&
            expected_type != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID) &&
            expected_type != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT) &&
            expected_type != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE) &&
            expected_type != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
        if (is_direct_int_type) {
            *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, expected_type, val);
        } else if (val == 0 && expected_type && gcc_jit_type_is_pointer(expected_type) != NULL) {
            *out_rval = gcc_jit_context_null(ctx, expected_type);
        } else if (val == 0) {
            gcc_gen_be_error_t cast_err = safe_ptr_or_int_cast(ctx, gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), 0), expected_type, out_rval);
            if (cast_err != GCC_GEN_BE_SUCCESS || !*out_rval) {
                GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(con->type, expected_type, out_rval));
            }
        } else {
            gcc_jit_rvalue *num = gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), val);
            GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, num, expected_type, out_rval));
        }
    } else if (con->kind == ck_float) {
        return gcc_gen_be_lower_float_constant(con, expected_type, out_rval);
    } else if (con->kind == ck_string) {
        a_type_ptr str_con_tp = NULL;
        GCC_GEN_BE_CHECK(strip_expr_typerefs(con->type, &str_con_tp));
        if (expected_type && gcc_jit_type_is_pointer(expected_type) != NULL) {
            gcc_jit_rvalue *str_lit = gcc_jit_context_new_string_literal(ctx, (const char *)con->variant.string.value);
            *out_rval = gcc_jit_context_new_cast(ctx, NULL, str_lit, expected_type);
        } else if (str_con_tp && str_con_tp->kind == tk_array) {
            a_type_ptr elem_type_ptr = NULL;
            GCC_GEN_BE_CHECK(strip_expr_typerefs(str_con_tp->variant.array.element_type, &elem_type_ptr));
            gcc_jit_type *elem_type = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(elem_type_ptr, &elem_type));
            long array_elems = (long)str_con_tp->variant.array.variant.number_of_elements;

            size_t bytes_per_elem = 1;
            if (elem_type_ptr && elem_type_ptr->kind == tk_integer) {
                bytes_per_elem = (size_t)f_size_of_type(elem_type_ptr);
                if (bytes_per_elem == 0) bytes_per_elem = 1;
            }

            size_t num_values = array_elems > 0 ? (size_t)array_elems : ((size_t)con->variant.string.length / bytes_per_elem);
            if (num_values == 0) num_values = 1;
            gcc_jit_rvalue **vals = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_alloc(num_values * sizeof(gcc_jit_rvalue *), (void **)&vals));

            const unsigned char *bytes = (const unsigned char *)con->variant.string.value;
            size_t str_len_bytes = (size_t)con->variant.string.length;

            for (size_t i = 0; i < num_values; ++i) {
                long long val = 0;
                size_t offset = i * bytes_per_elem;
                if (bytes && offset < str_len_bytes) {
                    for (size_t b = 0; b < bytes_per_elem && (offset + b) < str_len_bytes; ++b) {
                        val |= ((long long)bytes[offset + b]) << (b * 8);
                    }
                }
                vals[i] = gcc_jit_context_new_rvalue_from_long(ctx, elem_type, (long)val);
            }
            *out_rval = gcc_jit_context_new_array_constructor(ctx, NULL, expected_type, num_values, vals);
            free(vals);
        } else {
            *out_rval = gcc_jit_context_new_string_literal(ctx, (const char *)con->variant.string.value);
        }
    } else if (con->kind == ck_aggregate) {
        return lower_aggregate_constant(con, expected_type, out_rval);
    } else if (con->kind == ck_address) {
        return lower_address_constant(con, expected_type, out_rval);
    } else if (con->kind == ck_ptr_to_member) {
        gcc_jit_type *pm_type = expected_type;
        long base_delta = 0L;
        if (con->variant.ptr_to_member.casting_base_class) {
            long boff = (long)con->variant.ptr_to_member.casting_base_class->offset;
            base_delta = con->variant.ptr_to_member.cast_to_base ? -boff : boff;
        }
        if (!con->variant.ptr_to_member.is_function_ptr) {
            a_field_ptr fld = con->variant.ptr_to_member.variant.field;
            gcc_jit_type *pdm_t = (pm_type && !gcc_jit_type_is_struct(pm_type))
                ? pm_type
                : gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
            long off_val = fld ? ((long)fld->offset + base_delta) : -1L;
            *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, pdm_t, off_val);
            return GCC_GEN_BE_SUCCESS;
        }
        if (!pm_type || !gcc_jit_type_is_struct(pm_type)) {
            return gcc_gen_be_get_zero_rvalue(con->type, expected_type, out_rval);
        }
        gcc_jit_struct *pm_st = gcc_jit_type_is_struct(pm_type);
        gcc_jit_field *f_ptr = gcc_jit_struct_get_field(pm_st, 0);
        gcc_jit_field *f_adj = gcc_jit_struct_get_field(pm_st, 1);
        gcc_jit_field *flds[2] = { f_ptr, f_adj };
        gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_type *long_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
        a_routine_ptr rout = con->variant.ptr_to_member.variant.routine;
        if (!rout) {
            gcc_jit_rvalue *vals[2] = {
                gcc_jit_context_null(ctx, vp_t),
                gcc_jit_context_new_rvalue_from_long(ctx, long_t, 0L)
            };
            *out_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, pm_type, 2, flds, vals);
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_gen_be_target_abi_t active_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_target_abi(&active_abi));
        a_boolean arm_pmf = (active_abi == GCC_GEN_BE_TARGET_ABI_AAPCS64) ? TRUE : FALSE;

        gcc_jit_rvalue *ptr_val = NULL;
        long adj_val = base_delta;
        if (rout->is_virtual && rout->number.virtual_function != VIRTUAL_FUNCTION_NUMBER_NONE) {
            long vbyte_off = (long)rout->number.virtual_function * (long)sizeof(void *);
            if (arm_pmf) {
                ptr_val = (vbyte_off == 0L)
                    ? gcc_jit_context_null(ctx, vp_t)
                    : gcc_jit_context_new_rvalue_from_ptr(ctx, vp_t, (void *)(uintptr_t)vbyte_off);
                adj_val = (base_delta << 1) | 1L;
            } else {
                ptr_val = gcc_jit_context_new_rvalue_from_ptr(ctx, vp_t, (void *)(uintptr_t)(vbyte_off + 1L));
                adj_val = base_delta;
            }
        } else {
            gcc_jit_function *fn = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(rout, &fn));
            void *abi_fn = NULL;
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, fn, &abi_fn));
            if (abi_fn) fn = (gcc_jit_function *)abi_fn;
            ptr_val = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(fn, NULL), vp_t);
            adj_val = arm_pmf ? (base_delta << 1) : base_delta;
        }
        gcc_jit_rvalue *vals[2] = {
            ptr_val,
            gcc_jit_context_new_rvalue_from_long(ctx, long_t, adj_val)
        };
        *out_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, pm_type, 2, flds, vals);
        return GCC_GEN_BE_SUCCESS;
    } else {
        return gcc_gen_be_get_zero_rvalue(con->type, expected_type, out_rval);
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
  if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_rval = NULL;
  if (!expr) return GCC_GEN_BE_SUCCESS;
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  gcc_gen_be_error_t err;

  gcc_jit_location *loc = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_expr(expr, &loc));

  switch (expr->kind) {

      case enk_temp_init:
      case enk_lambda:
          {
              gcc_jit_lvalue *temp_lval = NULL;
              err = gcc_gen_be_lower_expr_lvalue(expr, &temp_lval);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              if (temp_lval) {
                  *out_rval = gcc_jit_lvalue_as_rvalue(temp_lval);
              } else {
                  *out_rval = NULL;
              }
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_builtin_operation:
          {
              a_builtin_operation_kind bok = expr->variant.builtin_operation.kind;
              gcc_jit_type *res_type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &res_type));
              an_expr_node_ptr op1 = expr->variant.builtin_operation.operands;
              an_expr_node_ptr op2 = op1 ? op1->next : NULL;

              if (bok == bok_builtin_complex) {
                  an_expr_node_ptr r_expr = op1;
                  an_expr_node_ptr i_expr = op2;
                  
                  gcc_jit_rvalue *r_val = NULL;
                  gcc_jit_rvalue *i_val = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(r_expr, &r_val));
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(i_expr, &i_val));
                  
                  gcc_jit_type *c_type = res_type;
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  
                  if (gcc_jit_type_is_struct(c_type)) {
                      gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), r_val);
                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), i_val);
                      *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                  } else {
                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                      gcc_jit_type *elem_type = gcc_jit_rvalue_get_type(r_val);
                      gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                      gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                      gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                      
                      gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                      gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                      gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                      
                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, zero), r_val);
                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, one), i_val);
                      
                      *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                  }
                  return GCC_GEN_BE_SUCCESS;
              } else if (bok == bok_builtin_addressof) {
                  gcc_jit_lvalue *target_lval = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &target_lval));
                  if (target_lval) {
                      *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(target_lval, loc), res_type);
                  } else {
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(expr->type, res_type, out_rval));
                  }
                  return GCC_GEN_BE_SUCCESS;
              } else if (bok == bok_offsetof || bok == bok_intaddr) {
                  an_expr_node_ptr target_op = (bok == bok_offsetof && op2) ? op2 : op1;
                  if (target_op && target_op->kind == enk_field && target_op->variant.field.ptr) {
                      long f_off = (long)target_op->variant.field.ptr->offset;
                      *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, res_type, f_off);
                      return GCC_GEN_BE_SUCCESS;
                  }
                  if (target_op) {
                      gcc_jit_rvalue *sub_rval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(target_op, &sub_rval));
                      if (sub_rval) {
                          GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, sub_rval, res_type, out_rval));
                          return GCC_GEN_BE_SUCCESS;
                      }
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(expr->type, res_type, out_rval));
                  return GCC_GEN_BE_SUCCESS;
              } else if (bok == bok_builtin_bit_cast) {
                  an_expr_node_ptr src_op = (op1 && op1->kind == enk_type_operand) ? op2 : op1;
                  gcc_jit_rvalue *src_rval = NULL;
                  if (src_op) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(src_op, &src_rval));
                  }
                  if (src_rval) {
                      if (p_gcc_jit_context_new_bitcast) {
                          *out_rval = p_gcc_jit_context_new_bitcast(ctx, loc, src_rval, res_type);
                          return GCC_GEN_BE_SUCCESS;
                      }
                      gcc_jit_block *cblock = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                      if (cblock) {
                          gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                          gcc_jit_lvalue *src_tmp = gcc_jit_function_new_local(func, loc, gcc_jit_rvalue_get_type(src_rval), "bitcast_src");
                          gcc_jit_block_add_assignment(cblock, loc, src_tmp, src_rval);
                          gcc_jit_rvalue *src_addr = gcc_jit_lvalue_get_address(src_tmp, loc);
                          gcc_jit_rvalue *dst_ptr = gcc_jit_context_new_cast(ctx, loc, src_addr, gcc_jit_type_get_pointer(res_type));
                          *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(dst_ptr, loc));
                          return GCC_GEN_BE_SUCCESS;
                      }
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(expr->type, res_type, out_rval));
                  return GCC_GEN_BE_SUCCESS;
              } else if (bok == bok_builtin_shuffle || bok == bok_builtin_shufflevector) {
                  return gcc_gen_be_lower_vector_shuffle(expr, out_rval);
              } else if (bok == bok_builtin_convertvector) {
                  return gcc_gen_be_lower_convertvector(expr, out_rval);
              } else if ((bok == bok_types_compatible || bok == bok_is_same || bok == bok_is_same_as) && op1 && op2) {
                  a_type_ptr t1 = NULL;
                  a_type_ptr t2 = NULL;
                  GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &t1));
                  GCC_GEN_BE_CHECK(strip_expr_typerefs(op2->type, &t2));
                  a_boolean compat = (t1 && t2 && t1->kind == t2->kind && t1->size == t2->size) ? TRUE : FALSE;
                  if (compat && t1->kind == tk_integer) {
                      compat = (t1->variant.integer.int_kind == t2->variant.integer.int_kind) ? TRUE : FALSE;
                  } else if (compat && t1->kind == tk_float) {
                      compat = (t1->variant.float_kind == t2->variant.float_kind) ? TRUE : FALSE;
                  } else if (compat && (t1->kind == tk_struct || t1->kind == tk_class || t1->kind == tk_union)) {
                      compat = (t1 == t2) ? TRUE : FALSE;
                  }
                  *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, res_type, compat ? 1 : 0);
                  return GCC_GEN_BE_SUCCESS;
              }
              GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(expr->type, res_type, out_rval));
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_builtin_choose_expr:
          {
              an_expr_node_ptr op_cond = expr->variant.builtin_choose_expr.operands;
              an_expr_node_ptr op_true = op_cond ? op_cond->next : NULL;
              an_expr_node_ptr op_false = op_true ? op_true->next : NULL;
              an_expr_node_ptr chosen = expr->variant.builtin_choose_expr.choose_first ? op_true : op_false;
              if (!chosen) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
              return gcc_gen_be_lower_expr_rvalue(chosen, out_rval);
          }

      case enk_statement:
          {
              gcc_jit_type *res_type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &res_type));
              
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              
              a_boolean is_void_stmt_expr = (!res_type || res_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID))
                  ? TRUE : FALSE;
              gcc_jit_lvalue *res_lval = is_void_stmt_expr
                  ? NULL
                  : gcc_jit_function_new_local(func, NULL, res_type, "stmt_expr_res");
              
              extern struct gcc_jit_lvalue *current_stmt_expr_result_lval;
              struct gcc_jit_lvalue *old_res_lval = current_stmt_expr_result_lval;
              current_stmt_expr_result_lval = res_lval;
              
              extern gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
              gcc_gen_be_error_t st_err = gcc_gen_be_lower_statement(expr->variant.statement, func);
              current_stmt_expr_result_lval = old_res_lval;
              GCC_GEN_BE_CHECK(st_err);
              
              *out_rval = res_lval ? gcc_jit_lvalue_as_rvalue(res_lval) : NULL;
              return GCC_GEN_BE_SUCCESS;
          }
      case enk_constant:
          {
              gcc_jit_type *type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &type));
              return gcc_gen_be_lower_constant_rvalue(expr->variant.constant.ptr, type, out_rval);
          }
          
      case enk_variable:
          {
              a_variable_ptr var = expr->variant.variable.ptr;
              a_type_ptr etype = NULL;
              GCC_GEN_BE_CHECK(strip_expr_typerefs(expr->type, &etype));
              if (var && !var->source_corresp.name && var->storage_class == sc_static &&
                  var->init_kind == initk_static && var->initializer.constant &&
                  !(etype && etype->kind == tk_array)) {
                  gcc_jit_type *vtype = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &vtype));
                  return gcc_gen_be_lower_constant_rvalue(var->initializer.constant, vtype, out_rval);
              }
              gcc_jit_lvalue *lval = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(expr, &lval));
              if (lval) {
                  a_type_ptr atomic_tp = NULL;
                  GCC_GEN_BE_CHECK(resolve_lvalue_c11_atomic_type(expr, &atomic_tp));
                  if (atomic_tp) {
                      return gcc_gen_be_lower_c11_atomic_load(expr->type ? expr->type : atomic_tp, lval, out_rval);
                  }
                  if (etype && etype->kind == tk_array) {
                      gcc_jit_rvalue *lv_rv = gcc_jit_lvalue_as_rvalue(lval);
                      if (etype->variant.array.is_variable_size_array &&
                          gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(lv_rv)) != NULL) {
                          *out_rval = lv_rv;
                      } else {
                          gcc_jit_type *elem_type = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(etype->variant.array.element_type, &elem_type));
                          gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                          *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(lval, NULL), elem_ptr_type);
                      }
                  } else {
                      *out_rval = gcc_jit_lvalue_as_rvalue(lval);
                  }
              }
              return GCC_GEN_BE_SUCCESS;
          }
          
      case enk_operation:
          {
              an_expr_operator_kind op = expr->variant.operation.kind;
              an_expr_node_ptr op1 = expr->variant.operation.operands;
              an_expr_node_ptr op2 = op1 ? op1->next : NULL;
              
              gcc_jit_type *type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &type));

              if (op == eok_dot_field || op == eok_points_to_field || op == eok_subscript || op == eok_vector_subscript || op == eok_indirect || op == eok_ref_indirect) {
                  a_type_ptr etype_chk = NULL;
                  a_type_ptr op1_t_chk = NULL;
                  a_type_ptr op1_pts_chk = NULL;
                  GCC_GEN_BE_CHECK(strip_expr_typerefs(expr->type, &etype_chk));
                  GCC_GEN_BE_CHECK(strip_expr_typerefs(op1 ? op1->type : NULL, &op1_t_chk));
                  GCC_GEN_BE_CHECK(strip_expr_typerefs(
                      (op1_t_chk && op1_t_chk->kind == tk_pointer) ? op1_t_chk->variant.pointer.type : NULL,
                      &op1_pts_chk));
                  if ((op == eok_indirect || op == eok_ref_indirect) &&
                      ((etype_chk && etype_chk->kind == tk_routine) ||
                       (op1_t_chk && op1_t_chk->kind == tk_routine) ||
                       (op1_pts_chk && op1_pts_chk->kind == tk_routine))) {
                      return gcc_gen_be_lower_expr_rvalue(op1, out_rval);
                  }
                  a_boolean self_is_bf = FALSE;
                  GCC_GEN_BE_CHECK(check_is_bitfield_access_op(expr, &self_is_bf));
                  if (self_is_bf) {
                      return gcc_gen_be_lower_bitfield_read(expr, out_rval);
                  }
                  gcc_jit_lvalue *lval = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(expr, &lval));
                  if (lval) {
                      a_type_ptr atomic_tp = NULL;
                      GCC_GEN_BE_CHECK(resolve_lvalue_c11_atomic_type(expr, &atomic_tp));
                      if (atomic_tp) {
                          return gcc_gen_be_lower_c11_atomic_load(expr->type, lval, out_rval);
                      }
                      if (etype_chk && etype_chk->kind == tk_array) {
                          gcc_jit_type *elem_type = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(etype_chk->variant.array.element_type, &elem_type));
                          gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                          *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(lval, NULL), elem_ptr_type);
                      } else {
                          *out_rval = gcc_jit_lvalue_as_rvalue(lval);
                      }
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
              
              if (op == eok_comma) {
                  gcc_jit_rvalue *r1 = NULL;
                  if (op1) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &r1));
                  }
                  GCC_GEN_BE_CHECK(emit_optional_eval(loc, r1));
                  return gcc_gen_be_lower_expr_rvalue(op2, out_rval);
              }
              if (op == eok_land) {
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  gcc_jit_lvalue *res = gcc_jit_function_new_local(func, NULL, bool_type, "land_res");
                  
                  gcc_jit_block *rhs_block = gcc_jit_function_new_block(func, "land_rhs");
                  gcc_jit_block *false_block = gcc_jit_function_new_block(func, "land_false");
                  gcc_jit_block *merge_block = gcc_jit_function_new_block(func, "land_merge");
                  
                  gcc_jit_rvalue *r1 = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &r1));
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_rvalue *r1_bool = (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(r1)) != NULL)
                      ? gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, r1, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(r1)))
                      : ((gcc_jit_rvalue_get_type(r1) == bool_type) ? r1 : gcc_jit_context_new_cast(ctx, loc, r1, bool_type));
                  gcc_jit_block_end_with_conditional(cblock, loc, r1_bool, rhs_block, false_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(false_block));
                  gcc_jit_block_add_assignment(false_block, NULL, res, gcc_jit_context_new_rvalue_from_int(ctx, bool_type, 0));
                  gcc_jit_block_end_with_jump(false_block, loc, merge_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(rhs_block));
                  gcc_jit_rvalue *r2 = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op2, &r2));
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&rhs_block));
                  if (rhs_block) {
                      gcc_jit_rvalue *r2_bool = (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(r2)) != NULL)
                          ? gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, r2, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(r2)))
                          : ((gcc_jit_rvalue_get_type(r2) == bool_type) ? r2 : gcc_jit_context_new_cast(ctx, loc, r2, bool_type));
                      gcc_jit_block_add_assignment(rhs_block, NULL, res, r2_bool);
                      gcc_jit_block_end_with_jump(rhs_block, loc, merge_block);
                  }
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
                  *out_rval = (gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(res)) == type)
                      ? gcc_jit_lvalue_as_rvalue(res)
                      : gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_as_rvalue(res), type);
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_lor) {
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  gcc_jit_lvalue *res = gcc_jit_function_new_local(func, NULL, bool_type, "lor_res");
                  
                  gcc_jit_block *true_block = gcc_jit_function_new_block(func, "lor_true");
                  gcc_jit_block *rhs_block = gcc_jit_function_new_block(func, "lor_rhs");
                  gcc_jit_block *merge_block = gcc_jit_function_new_block(func, "lor_merge");
                  
                  gcc_jit_rvalue *r1 = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op1, &r1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_rvalue *r1_bool = (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(r1)) != NULL)
                      ? gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, r1, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(r1)))
                      : ((gcc_jit_rvalue_get_type(r1) == bool_type) ? r1 : gcc_jit_context_new_cast(ctx, loc, r1, bool_type));
                  gcc_jit_block_end_with_conditional(cblock, loc, r1_bool, true_block, rhs_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(true_block));
                  gcc_jit_block_add_assignment(true_block, NULL, res, gcc_jit_context_new_rvalue_from_int(ctx, bool_type, 1));
                  gcc_jit_block_end_with_jump(true_block, loc, merge_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(rhs_block));
                  gcc_jit_rvalue *r2 = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op2, &r2);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&rhs_block));
                  if (rhs_block) {
                      gcc_jit_rvalue *r2_bool = (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(r2)) != NULL)
                          ? gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, r2, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(r2)))
                          : ((gcc_jit_rvalue_get_type(r2) == bool_type) ? r2 : gcc_jit_context_new_cast(ctx, loc, r2, bool_type));
                      gcc_jit_block_add_assignment(rhs_block, NULL, res, r2_bool);
                      gcc_jit_block_end_with_jump(rhs_block, loc, merge_block);
                  }
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
                  *out_rval = (gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(res)) == type)
                      ? gcc_jit_lvalue_as_rvalue(res)
                      : gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_as_rvalue(res), type);
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_question) {
                  an_expr_node_ptr op3 = op2 ? op2->next : NULL;
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
                  
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  gcc_jit_block *then_block = gcc_jit_function_new_block(func, "ternary_then");
                  gcc_jit_block *else_block = gcc_jit_function_new_block(func, "ternary_else");
                  gcc_jit_block *merge_block = gcc_jit_function_new_block(func, "ternary_merge");
                  
                  gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                  a_boolean is_void_ternary = (type == void_type) ? TRUE : FALSE;
                  gcc_jit_lvalue *res = is_void_ternary ? NULL : gcc_jit_function_new_local(func, NULL, type, "ternary_res");
                  
                  gcc_jit_rvalue *r1 = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op1, &r1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  gcc_jit_type *r1_type = gcc_jit_rvalue_get_type(r1);
                  gcc_jit_rvalue *cond_bool = NULL;
                  if (r1_type == bool_type) {
                      cond_bool = r1;
                  } else if (gcc_jit_type_is_pointer(r1_type) != NULL) {
                      cond_bool = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, r1, gcc_jit_context_null(ctx, r1_type));
                  } else {
                      cond_bool = gcc_jit_context_new_cast(ctx, loc, r1, bool_type);
                  }
                  gcc_jit_block_end_with_conditional(cblock, loc, cond_bool, then_block, else_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(then_block));
                  gcc_jit_rvalue *r2 = NULL;
                  if (op2) { err = gcc_gen_be_lower_expr_rvalue(op2, &r2); if (err != GCC_GEN_BE_SUCCESS) return err; }
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&then_block));
                  if (then_block) {
                      if (r2 && res) gcc_jit_block_add_assignment(then_block, NULL, res, r2);
                      else if (r2) gcc_jit_block_add_eval(then_block, NULL, r2);
                      gcc_jit_block_end_with_jump(then_block, NULL, merge_block);
                  }
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(else_block));
                  gcc_jit_rvalue *r3 = NULL;
                  if (op3) { err = gcc_gen_be_lower_expr_rvalue(op3, &r3); if (err != GCC_GEN_BE_SUCCESS) return err; }
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&else_block));
                  if (else_block) {
                      if (r3 && res) gcc_jit_block_add_assignment(else_block, NULL, res, r3);
                      else if (r3) gcc_jit_block_add_eval(else_block, NULL, r3);
                      gcc_jit_block_end_with_jump(else_block, NULL, merge_block);
                  }
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
                  *out_rval = res ? gcc_jit_lvalue_as_rvalue(res) : NULL;
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_assign || op == eok_bassign || op == eok_add_assign || op == eok_subtract_assign || op == eok_multiply_assign || op == eok_divide_assign || op == eok_remainder_assign || op == eok_shiftl_assign || op == eok_shiftr_assign || op == eok_and_assign || op == eok_or_assign || op == eok_xor_assign || op == eok_padd_assign || op == eok_psubtract_assign) {
                  a_boolean lhs_is_bf = FALSE;
                  GCC_GEN_BE_CHECK(check_is_bitfield_access_op(op1, &lhs_is_bf));
                  if (lhs_is_bf) {
                      return gcc_gen_be_lower_bitfield_assignment(op1, op, op2, out_rval);
                  }
                  gcc_jit_lvalue *l1 = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &l1));
                  
                  gcc_jit_rvalue *r2 = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op2, &r2));

                  if (l1 && r2) {
                      a_type_ptr atomic_lhs_tp = NULL;
                      GCC_GEN_BE_CHECK(resolve_lvalue_c11_atomic_type(op1, &atomic_lhs_tp));
                      if (atomic_lhs_tp) {
                          if (op == eok_assign || op == eok_bassign) {
                              return gcc_gen_be_lower_c11_atomic_store(atomic_lhs_tp, l1, r2, out_rval);
                          }
                          return gcc_gen_be_lower_c11_atomic_rmw(op, atomic_lhs_tp, l1, r2, out_rval);
                      }
                  }
                  
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (cblock) {
                      if (op == eok_assign || op == eok_bassign) {
                          if (l1 && r2) {
                              gcc_jit_type *l1_t = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(l1));
                              gcc_jit_type *r2_t = gcc_jit_rvalue_get_type(r2);
                              if (l1_t != r2_t) {
                                  if (gcc_jit_type_is_pointer(l1_t) != NULL && gcc_jit_type_is_pointer(r2_t) != NULL) {
                                      r2 = gcc_jit_context_new_cast(ctx, loc, r2, l1_t);
                                  } else if (gcc_jit_type_is_struct(l1_t) != NULL &&
                                             gcc_jit_struct_get_field_count(gcc_jit_type_is_struct(l1_t)) == 1) {
                                      gcc_jit_field *f0 = gcc_jit_struct_get_field(gcc_jit_type_is_struct(l1_t), 0);
                                      gcc_jit_lvalue *l1_f0 = gcc_jit_lvalue_access_field(l1, loc, f0);
                                      gcc_jit_type *f0_t = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(l1_f0));
                                      if (f0_t == r2_t) {
                                          l1 = l1_f0;
                                      }
                                  } else if (gcc_jit_type_is_struct(l1_t) == NULL &&
                                             gcc_jit_type_is_struct(r2_t) == NULL &&
                                             r2_t != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                                      GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, r2, l1_t, &r2));
                                  }
                              }
                              if (r2_t != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                                  gcc_jit_block_add_assignment(cblock, loc, l1, r2);
                              } else {
                                  gcc_jit_block_add_eval(cblock, loc, r2);
                              }
                          }
                      } else if (op == eok_padd_assign) {
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, gcc_jit_lvalue_as_rvalue(l1), r2);
                          gcc_jit_rvalue *new_ptr = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          gcc_jit_block_add_assignment(cblock, loc, l1, new_ptr);
                      } else if (op == eok_psubtract_assign) {
                          gcc_jit_type *ptrdiff_t_jit = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                          gcc_jit_rvalue *signed_r2 = (gcc_jit_rvalue_get_type(r2) == ptrdiff_t_jit)
                              ? r2
                              : gcc_jit_context_new_cast(ctx, loc, r2, ptrdiff_t_jit);
                          gcc_jit_rvalue *neg_r2 = gcc_jit_context_new_unary_op(
                              ctx, loc, GCC_JIT_UNARY_OP_MINUS, ptrdiff_t_jit, signed_r2);
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, gcc_jit_lvalue_as_rvalue(l1), neg_r2);
                          gcc_jit_rvalue *new_ptr = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          gcc_jit_block_add_assignment(cblock, loc, l1, new_ptr);
                      } else {
                          enum gcc_jit_binary_op jit_op = GCC_JIT_BINARY_OP_PLUS;
                          GCC_GEN_BE_CHECK(map_compound_assign_binary_op(op, &jit_op));
                          if (l1 && r2) {
                              gcc_jit_type *l1_t = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(l1));
                              if (gcc_jit_rvalue_get_type(r2) != l1_t) {
                                  GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, r2, l1_t, &r2));
                              }
                          }
                          gcc_jit_block_add_assignment_op(cblock, NULL, l1, jit_op, r2);
                      }
                  }
                  *out_rval = gcc_jit_lvalue_as_rvalue(l1);
                  return GCC_GEN_BE_SUCCESS;
              }
              
              a_boolean is_call_op = FALSE;
              a_boolean is_member_call_op = FALSE;
              a_boolean is_pm_field_op = FALSE;
              GCC_GEN_BE_CHECK(classify_call_or_pm_op(op, &is_call_op, &is_member_call_op, &is_pm_field_op));

              gcc_jit_rvalue *r1 = NULL;
              if (op1 && op != eok_address_of &&
                  op != eok_pre_incr && op != eok_pre_decr &&
                  op != eok_post_incr && op != eok_post_decr &&
                  !is_call_op && !is_pm_field_op) {
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &r1));
              } else if (op1 && is_call_op && op1->kind != enk_routine) {
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &r1));
              }

              gcc_jit_rvalue *r2 = NULL;
              if (op2 && !is_call_op && !is_pm_field_op) {
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op2, &r2));
              }
              
              switch (op) {
                  case eok_dynamic_cast:
                      return gcc_gen_be_lower_dynamic_cast(expr, out_rval);
                  case eok_padd:
                      {
                          a_type_ptr op1_t = NULL;
                          a_type_ptr op2_t = NULL;
                          GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &op1_t));
                          GCC_GEN_BE_CHECK(strip_expr_typerefs(op2->type, &op2_t));
                          a_boolean op1_is_ptr = op1_t && (op1_t->kind == tk_pointer || op1_t->kind == tk_array);
                          a_type_ptr ptr_edg_t = op1_is_ptr ? op1_t : op2_t;
                          a_type_ptr elem_edg_t = ptr_edg_t
                              ? (ptr_edg_t->kind == tk_array ? ptr_edg_t->variant.array.element_type
                                 : (ptr_edg_t->kind == tk_pointer ? ptr_edg_t->variant.pointer.type : NULL))
                              : NULL;
                          gcc_jit_rvalue *ptr_val = op1_is_ptr ? r1 : r2;
                          gcc_jit_rvalue *int_val = op1_is_ptr ? r2 : r1;
                          GCC_GEN_BE_CHECK(scale_vla_subarray_index(ctx, loc, elem_edg_t, int_val, &int_val));
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, ptr_val, int_val);
                          *out_rval = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_psubtract:
                      {
                          a_type_ptr op1_t = NULL;
                          GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &op1_t));
                          a_type_ptr elem_edg_t = op1_t
                              ? (op1_t->kind == tk_array ? op1_t->variant.array.element_type
                                 : (op1_t->kind == tk_pointer ? op1_t->variant.pointer.type : NULL))
                              : NULL;
                          gcc_jit_type *ptrdiff_t_jit = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                          gcc_jit_rvalue *signed_r2 = (gcc_jit_rvalue_get_type(r2) == ptrdiff_t_jit)
                              ? r2
                              : gcc_jit_context_new_cast(ctx, loc, r2, ptrdiff_t_jit);
                          gcc_jit_rvalue *neg_r2 = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, ptrdiff_t_jit, signed_r2);
                          GCC_GEN_BE_CHECK(scale_vla_subarray_index(ctx, loc, elem_edg_t, neg_r2, &neg_r2));
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, r1, neg_r2);
                          *out_rval = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_pdiff:
                      {
                          gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                          gcc_jit_rvalue *c1 = NULL;
                          gcc_jit_rvalue *c2 = NULL;
                          GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, r1, long_type, &c1));
                          GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, r2, long_type, &c2));
                          gcc_jit_rvalue *diff = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, long_type, c1, c2);
                          
                          a_type_ptr op1_t = NULL;
                          GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &op1_t));
                          a_type_ptr elem_type = op1_t
                              ? (op1_t->kind == tk_array ? op1_t->variant.array.element_type
                                 : (op1_t->kind == tk_pointer ? op1_t->variant.pointer.type : NULL))
                              : NULL;
                          long sz = elem_type ? (long)f_size_of_type(elem_type) : 1L;
                          if (sz <= 0) sz = 1L;
                          gcc_jit_rvalue *sz_val = gcc_jit_context_new_rvalue_from_long(ctx, long_type, sz);
                          
                          gcc_jit_rvalue *res_diff = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_DIVIDE, long_type, diff, sz_val);
                          if (type != long_type) {
                              GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, res_diff, type, out_rval));
                          } else {
                              *out_rval = res_diff;
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_spaceship:
                      {
                          gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                          gcc_jit_rvalue *cmp_lt = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_LT, r1, r2);
                          gcc_jit_rvalue *cmp_gt = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_GT, r1, r2);
                          gcc_jit_rvalue *lt_val = gcc_jit_context_new_cast(ctx, loc, cmp_lt, int_type);
                          gcc_jit_rvalue *gt_val = gcc_jit_context_new_cast(ctx, loc, cmp_gt, int_type);
                          gcc_jit_rvalue *res_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, int_type, gt_val, lt_val);

                          a_type_ptr op_t = NULL;
                          GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &op_t));
                          if (op_t && op_t->kind == tk_float) {
                              gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                              gcc_jit_rvalue *cmp_ne = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, r1, r2);
                              gcc_jit_rvalue *cmp_unordered = gcc_jit_context_new_cast(
                                  ctx, loc,
                                  gcc_jit_context_new_binary_op(
                                      ctx, loc, GCC_JIT_BINARY_OP_LOGICAL_AND, bool_t, cmp_ne,
                                      gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, bool_t, res_val)),
                                  int_type);

                              /* If unordered (not equal and not < and not >), return -127 */
                              gcc_jit_rvalue *unord_val = gcc_jit_context_new_rvalue_from_int(ctx, int_type, -127);

                              /* We want a ternary operator or just if unordered ? -127 : res_val */
                              /* In libgccjit, there's no ternary rvalue yet in some older versions, so let's do bitwise or arithmetic:
                                 res = unord ? -127 : res_val;
                                 res = (unord * -127) + ((1 - unord) * res_val)
                                 Wait, unord is 1 or 0 */
                              gcc_jit_rvalue *unord_term = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, int_type, cmp_unordered, unord_val);
                              gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                              gcc_jit_rvalue *not_unord = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, int_type, one, cmp_unordered);
                              gcc_jit_rvalue *res_term = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, int_type, not_unord, res_val);
                              res_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, int_type, unord_term, res_term);
                          }

                          /* `type` is expected to be the comparison category struct. We can cast the int to signed char, then bitcast to struct. */
                          gcc_jit_type *schar_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIGNED_CHAR);
                          gcc_jit_rvalue *schar_val = gcc_jit_context_new_cast(ctx, loc, res_val, schar_type);

                          if (gcc_jit_type_is_struct(type)) {
                              /* Create a temporary variable of the struct type and write the byte */
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              if (cblock) {
                                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                                  gcc_jit_lvalue *tmp_lval = gcc_jit_function_new_local(func, NULL, type, "spaceship_tmp");
                                  gcc_jit_type *schar_ptr_type = gcc_jit_type_get_pointer(schar_type);
                                  gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp_lval, NULL);
                                  gcc_jit_rvalue *tmp_schar_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, schar_ptr_type);
                                  gcc_jit_lvalue *tmp_schar_lval = gcc_jit_rvalue_dereference(tmp_schar_ptr, NULL);
                                  gcc_jit_block_add_assignment(cblock, loc, tmp_schar_lval, schar_val);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(tmp_lval);
                              } else {
                                  *out_rval = schar_val; /* fallback */
                              }
                          } else {
                              *out_rval = gcc_jit_context_new_cast(ctx, loc, schar_val, type);
                          }

                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_add:
                  case eok_subtract:
                  case eok_multiply:
                  case eok_divide:
                      {
                          a_boolean is_cplx = FALSE;
                          GCC_GEN_BE_CHECK(check_is_complex_type(expr->type, &is_cplx));
                          if (is_cplx) {
                              gcc_jit_type *c_type = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &c_type));
                              gcc_jit_type *elem_type = NULL;
                              GCC_GEN_BE_CHECK(get_complex_elem_jit_type(ctx, expr->type, &elem_type));
                              
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              
                              gcc_jit_rvalue *a = NULL, *b = NULL, *c = NULL, *d = NULL;
                              GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, cblock, r1, c_type, elem_type, &a, &b));
                              GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, cblock, r2, c_type, elem_type, &c, &d));
                              
                              gcc_jit_rvalue *res_r = NULL;
                              gcc_jit_rvalue *res_i = NULL;
                              
                              if (op == eok_add) {
                                  res_r = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_type, a, c);
                                  res_i = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_type, b, d);
                              } else if (op == eok_subtract) {
                                  res_r = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, elem_type, a, c);
                                  res_i = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, elem_type, b, d);
                              } else if (op == eok_multiply) {
                                  /* (a+bi)(c+di) = (ac-bd) + (ad+bc)i */
                                  gcc_jit_rvalue *ac = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, a, c);
                                  gcc_jit_rvalue *bd = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, b, d);
                                  res_r = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, elem_type, ac, bd);
                                  
                                  gcc_jit_rvalue *ad = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, a, d);
                                  gcc_jit_rvalue *bc = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, b, c);
                                  res_i = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_type, ad, bc);
                              } else {
                                  /* standard algorithm: (a+bi)/(c+di) = (ac+bd)/(c^2+d^2) + (bc-ad)/(c^2+d^2) i */
                                  gcc_jit_rvalue *ac = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, a, c);
                                  gcc_jit_rvalue *bd = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, b, d);
                                  gcc_jit_rvalue *num_r = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_type, ac, bd);
                                  
                                  gcc_jit_rvalue *bc = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, b, c);
                                  gcc_jit_rvalue *ad = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, a, d);
                                  gcc_jit_rvalue *num_i = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, elem_type, bc, ad);
                                  
                                  gcc_jit_rvalue *c2 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, c, c);
                                  gcc_jit_rvalue *d2 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_type, d, d);
                                  gcc_jit_rvalue *den = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_type, c2, d2);
                                  
                                  res_r = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_DIVIDE, elem_type, num_r, den);
                                  res_i = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_DIVIDE, elem_type, num_i, den);
                              }
                              
                              return construct_complex_from_parts(ctx, loc, cblock, c_type, elem_type, res_r, res_i, out_rval);
                          } else {
                              enum gcc_jit_binary_op jit_op = GCC_JIT_BINARY_OP_PLUS;
                              if (op == eok_subtract) jit_op = GCC_JIT_BINARY_OP_MINUS;
                              else if (op == eok_multiply) jit_op = GCC_JIT_BINARY_OP_MULT;
                              else if (op == eok_divide) jit_op = GCC_JIT_BINARY_OP_DIVIDE;
                              if (gcc_jit_rvalue_get_type(r1) != type) {
                                  GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, r1, type, &r1));
                              }
                              if (gcc_jit_rvalue_get_type(r2) != type) {
                                  GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, r2, type, &r2));
                              }
                              *out_rval = gcc_jit_context_new_binary_op(ctx, loc, jit_op, type, r1, r2);
                              return gcc_gen_be_normalize_bitint_rvalue(expr->type, *out_rval, out_rval);
                          }
                      }
                  case eok_remainder:
                  case eok_shiftl:
                  case eok_shiftr:
                  case eok_and:
                  case eok_or:
                  case eok_xor:
                      {
                          enum gcc_jit_binary_op jit_op = GCC_JIT_BINARY_OP_MODULO;
                          if (op == eok_shiftl) jit_op = GCC_JIT_BINARY_OP_LSHIFT;
                          else if (op == eok_shiftr) jit_op = GCC_JIT_BINARY_OP_RSHIFT;
                          else if (op == eok_and) jit_op = GCC_JIT_BINARY_OP_BITWISE_AND;
                          else if (op == eok_or) jit_op = GCC_JIT_BINARY_OP_BITWISE_OR;
                          else if (op == eok_xor) jit_op = GCC_JIT_BINARY_OP_BITWISE_XOR;
                          if (gcc_jit_rvalue_get_type(r1) != type) {
                              GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, r1, type, &r1));
                          }
                          if (gcc_jit_rvalue_get_type(r2) != type) {
                              GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, r2, type, &r2));
                          }
                          *out_rval = gcc_jit_context_new_binary_op(ctx, loc, jit_op, type, r1, r2);
                          return gcc_gen_be_normalize_bitint_rvalue(expr->type, *out_rval, out_rval);
                      }

                  case eok_eq:
                  case eok_ne:
                      {
                          a_boolean op1_is_cplx = FALSE;
                          if (op1) GCC_GEN_BE_CHECK(check_is_complex_type(op1->type, &op1_is_cplx));
                          if (op1 && op1_is_cplx) {
                              gcc_jit_type *c_type = gcc_jit_rvalue_get_type(r1);
                              gcc_jit_type *elem_type = NULL;
                              GCC_GEN_BE_CHECK(get_complex_elem_jit_type(ctx, op1->type, &elem_type));
                              
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              
                              gcc_jit_rvalue *a = NULL, *b = NULL, *c = NULL, *d = NULL;
                              GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, cblock, r1, c_type, elem_type, &a, &b));
                              GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, cblock, r2, c_type, elem_type, &c, &d));
                              
                              gcc_jit_rvalue *r_eq = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_EQ, a, c);
                              gcc_jit_rvalue *i_eq = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_EQ, b, d);
                              
                              gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                              gcc_jit_rvalue *r_eq_b = gcc_jit_context_new_cast(ctx, loc, r_eq, bool_type);
                              gcc_jit_rvalue *i_eq_b = gcc_jit_context_new_cast(ctx, loc, i_eq, bool_type);
                              
                              gcc_jit_rvalue *both_eq = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LOGICAL_AND, bool_type, r_eq_b, i_eq_b);
                              
                              if (op == eok_eq) {
                                  *out_rval = gcc_jit_context_new_cast(ctx, loc, both_eq, type);
                              } else {
                                  gcc_jit_rvalue *neq_val = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, bool_type, both_eq);
                                  *out_rval = gcc_jit_context_new_cast(ctx, loc, neq_val, type);
                              }
                              return GCC_GEN_BE_SUCCESS;
                          } else if (r1 && r2 && gcc_jit_type_is_struct(gcc_jit_rvalue_get_type(r1)) != NULL &&
                                     gcc_jit_type_is_struct(gcc_jit_rvalue_get_type(r2)) != NULL) {
                              gcc_jit_struct *s1 = gcc_jit_type_is_struct(gcc_jit_rvalue_get_type(r1));
                              gcc_jit_struct *s2 = gcc_jit_type_is_struct(gcc_jit_rvalue_get_type(r2));
                              gcc_jit_rvalue *fp1 = gcc_jit_rvalue_access_field(r1, loc, gcc_jit_struct_get_field(s1, 0));
                              gcc_jit_rvalue *adj1 = gcc_jit_rvalue_access_field(r1, loc, gcc_jit_struct_get_field(s1, 1));
                              gcc_jit_rvalue *fp2 = gcc_jit_rvalue_access_field(r2, loc, gcc_jit_struct_get_field(s2, 0));
                              gcc_jit_rvalue *adj2 = gcc_jit_rvalue_access_field(r2, loc, gcc_jit_struct_get_field(s2, 1));
                              gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                              gcc_jit_rvalue *fp_eq = gcc_jit_context_new_cast(
                                  ctx, loc, gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_EQ, fp1, fp2), bool_t);
                              gcc_jit_rvalue *adj_eq = gcc_jit_context_new_cast(
                                  ctx, loc, gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_EQ, adj1, adj2), bool_t);
                              gcc_jit_rvalue *both_eq = gcc_jit_context_new_binary_op(
                                  ctx, loc, GCC_JIT_BINARY_OP_LOGICAL_AND, bool_t, fp_eq, adj_eq);
                              if (op == eok_ne) {
                                  both_eq = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, bool_t, both_eq);
                              }
                              *out_rval = (type != bool_t) ? gcc_jit_context_new_cast(ctx, loc, both_eq, type) : both_eq;
                              return GCC_GEN_BE_SUCCESS;
                          } else {
#if GNU_VECTOR_TYPES_ALLOWED
                              a_type_ptr vec_res_tp = NULL;
                              GCC_GEN_BE_CHECK(strip_expr_typerefs(expr->type, &vec_res_tp));
                              if (vec_res_tp && vec_res_tp->kind == tk_vector) {
                                  a_type_ptr in_vec_tp = NULL;
                                  GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &in_vec_tp));
                                  a_type_ptr out_elem_tp = NULL;
                                  GCC_GEN_BE_CHECK(strip_expr_typerefs(vec_res_tp->variant.vector.element_type, &out_elem_tp));
                                  a_type_ptr in_elem_tp = (in_vec_tp && in_vec_tp->kind == tk_vector)
                                      ? in_vec_tp->variant.vector.element_type
                                      : out_elem_tp;
                                  GCC_GEN_BE_CHECK(strip_expr_typerefs(in_elem_tp, &in_elem_tp));
                                  size_t n_lanes = (out_elem_tp && out_elem_tp->size > 0)
                                      ? ((size_t)vec_res_tp->size / (size_t)out_elem_tp->size)
                                      : 0;
                                  gcc_jit_block *vc_blk = NULL;
                                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&vc_blk));
                                  if (n_lanes > 0 && vc_blk) {
                                      gcc_jit_function *vc_fn = gcc_jit_block_get_function(vc_blk);
                                      gcc_jit_type *in_lane_t = NULL;
                                      gcc_jit_type *out_lane_t = NULL;
                                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(in_elem_tp, &in_lane_t));
                                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(out_elem_tp, &out_lane_t));
                                      gcc_jit_lvalue *va_slot = gcc_jit_function_new_local(vc_fn, loc, gcc_jit_rvalue_get_type(r1), "vcmp_a");
                                      gcc_jit_lvalue *vb_slot = gcc_jit_function_new_local(vc_fn, loc, gcc_jit_rvalue_get_type(r2), "vcmp_b");
                                      gcc_jit_block_add_assignment(vc_blk, loc, va_slot, r1);
                                      gcc_jit_block_add_assignment(vc_blk, loc, vb_slot, r2);
                                      gcc_jit_type *in_lane_pt = gcc_jit_type_get_pointer(in_lane_t);
                                      gcc_jit_rvalue *pa = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(va_slot, loc), in_lane_pt);
                                      gcc_jit_rvalue *pb = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(vb_slot, loc), in_lane_pt);
                                      gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                      gcc_jit_rvalue **lanes = NULL;
                                      GCC_GEN_BE_CHECK(gcc_gen_be_alloc(n_lanes * sizeof(gcc_jit_rvalue *), (void **)&lanes));
                                      gcc_jit_rvalue *all_ones = gcc_jit_context_new_rvalue_from_int(ctx, out_lane_t, -1);
                                      enum gcc_jit_comparison vcmp = (op == eok_eq) ? GCC_JIT_COMPARISON_EQ : GCC_JIT_COMPARISON_NE;
                                      for (size_t li = 0; li < n_lanes; ++li) {
                                          gcc_jit_rvalue *idx_rv = gcc_jit_context_new_rvalue_from_int(ctx, int_t, (int)li);
                                          gcc_jit_rvalue *la = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, pa, idx_rv));
                                          gcc_jit_rvalue *lb = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, pb, idx_rv));
                                          gcc_jit_rvalue *cond_b = gcc_jit_context_new_comparison(ctx, loc, vcmp, la, lb);
                                          gcc_jit_rvalue *mask_i = gcc_jit_context_new_cast(ctx, loc, cond_b, out_lane_t);
                                          lanes[li] = gcc_jit_context_new_binary_op(
                                              ctx, loc, GCC_JIT_BINARY_OP_MULT, out_lane_t, mask_i, all_ones);
                                      }
                                      gcc_gen_be_error_t verr = gcc_gen_be_build_vector_rvalue(type, out_lane_t, n_lanes, lanes, out_rval);
                                      free(lanes);
                                      return verr;
                                  }
                              }
#endif
                              if (r1 && r2 && gcc_jit_rvalue_get_type(r1) != gcc_jit_rvalue_get_type(r2)) {
                                  GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, r2, gcc_jit_rvalue_get_type(r1), &r2));
                              }
                              gcc_jit_rvalue *cmp = gcc_jit_context_new_comparison(ctx, loc, op == eok_eq ? GCC_JIT_COMPARISON_EQ : GCC_JIT_COMPARISON_NE, r1, r2);
                              if (gcc_jit_rvalue_get_type(cmp) != type) {
                                  GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, cmp, type, out_rval));
                              } else {
                                  *out_rval = cmp;
                              }
                              return GCC_GEN_BE_SUCCESS;
                          }
                      }
                  case eok_lt:
                  case eok_le:
                  case eok_gt:
                  case eok_ge:
                      {
                          enum gcc_jit_comparison cmp_op = GCC_JIT_COMPARISON_LT;
                          if (op == eok_le) cmp_op = GCC_JIT_COMPARISON_LE;
                          else if (op == eok_gt) cmp_op = GCC_JIT_COMPARISON_GT;
                          else if (op == eok_ge) cmp_op = GCC_JIT_COMPARISON_GE;
#if GNU_VECTOR_TYPES_ALLOWED
                          a_type_ptr vec_res_tp = NULL;
                          GCC_GEN_BE_CHECK(strip_expr_typerefs(expr->type, &vec_res_tp));
                          if (vec_res_tp && vec_res_tp->kind == tk_vector) {
                              a_type_ptr in_vec_tp = NULL;
                              GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &in_vec_tp));
                              a_type_ptr out_elem_tp = NULL;
                              GCC_GEN_BE_CHECK(strip_expr_typerefs(vec_res_tp->variant.vector.element_type, &out_elem_tp));
                              a_type_ptr in_elem_tp = (in_vec_tp && in_vec_tp->kind == tk_vector)
                                  ? in_vec_tp->variant.vector.element_type
                                  : out_elem_tp;
                              GCC_GEN_BE_CHECK(strip_expr_typerefs(in_elem_tp, &in_elem_tp));
                              size_t n_lanes = (out_elem_tp && out_elem_tp->size > 0)
                                  ? ((size_t)vec_res_tp->size / (size_t)out_elem_tp->size)
                                  : 0;
                              gcc_jit_block *vc_blk = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&vc_blk));
                              if (n_lanes > 0 && vc_blk) {
                                  gcc_jit_function *vc_fn = gcc_jit_block_get_function(vc_blk);
                                  gcc_jit_type *in_lane_t = NULL;
                                  gcc_jit_type *out_lane_t = NULL;
                                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(in_elem_tp, &in_lane_t));
                                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(out_elem_tp, &out_lane_t));
                                  gcc_jit_lvalue *va_slot = gcc_jit_function_new_local(vc_fn, loc, gcc_jit_rvalue_get_type(r1), "vcmp_a");
                                  gcc_jit_lvalue *vb_slot = gcc_jit_function_new_local(vc_fn, loc, gcc_jit_rvalue_get_type(r2), "vcmp_b");
                                  gcc_jit_block_add_assignment(vc_blk, loc, va_slot, r1);
                                  gcc_jit_block_add_assignment(vc_blk, loc, vb_slot, r2);
                                  gcc_jit_type *in_lane_pt = gcc_jit_type_get_pointer(in_lane_t);
                                  gcc_jit_rvalue *pa = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(va_slot, loc), in_lane_pt);
                                  gcc_jit_rvalue *pb = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(vb_slot, loc), in_lane_pt);
                                  gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                  gcc_jit_rvalue **lanes = NULL;
                                  GCC_GEN_BE_CHECK(gcc_gen_be_alloc(n_lanes * sizeof(gcc_jit_rvalue *), (void **)&lanes));
                                  gcc_jit_rvalue *all_ones = gcc_jit_context_new_rvalue_from_int(ctx, out_lane_t, -1);
                                  for (size_t li = 0; li < n_lanes; ++li) {
                                      gcc_jit_rvalue *idx_rv = gcc_jit_context_new_rvalue_from_int(ctx, int_t, (int)li);
                                      gcc_jit_rvalue *la = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, pa, idx_rv));
                                      gcc_jit_rvalue *lb = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, pb, idx_rv));
                                      gcc_jit_rvalue *cond_b = gcc_jit_context_new_comparison(ctx, loc, cmp_op, la, lb);
                                      gcc_jit_rvalue *mask_i = gcc_jit_context_new_cast(ctx, loc, cond_b, out_lane_t);
                                      lanes[li] = gcc_jit_context_new_binary_op(
                                          ctx, loc, GCC_JIT_BINARY_OP_MULT, out_lane_t, mask_i, all_ones);
                                  }
                                  gcc_gen_be_error_t verr = gcc_gen_be_build_vector_rvalue(type, out_lane_t, n_lanes, lanes, out_rval);
                                  free(lanes);
                                  return verr;
                              }
                          }
#endif
                          if (gcc_jit_rvalue_get_type(r1) != gcc_jit_rvalue_get_type(r2)) {
                              GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, r2, gcc_jit_rvalue_get_type(r1), &r2));
                          }
                          gcc_jit_rvalue *cmp = gcc_jit_context_new_comparison(ctx, loc, cmp_op, r1, r2);
                          if (gcc_jit_rvalue_get_type(cmp) != type) {
                              GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, cmp, type, out_rval));
                          } else {
                              *out_rval = cmp;
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }
                  
                  case eok_fjadd:
                  case eok_jfadd:
                  case eok_fjsubtract:
                  case eok_jfsubtract:
                      {
                          gcc_jit_type *c_type = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &c_type));
                          gcc_jit_type *elem_type = NULL;
                          GCC_GEN_BE_CHECK(get_complex_elem_jit_type(ctx, expr->type, &elem_type));
                          
                          gcc_jit_rvalue *real_val = (op == eok_fjadd || op == eok_fjsubtract) ? r1 : r2;
                          gcc_jit_rvalue *imag_val = (op == eok_fjadd || op == eok_fjsubtract) ? r2 : r1;
                          
                          if (op == eok_fjsubtract) {
                              imag_val = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, imag_val);
                          } else if (op == eok_jfsubtract) {
                              real_val = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, real_val);
                          }
                          
                          gcc_jit_block *cblock = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          return construct_complex_from_parts(ctx, loc, cblock, c_type, elem_type, real_val, imag_val, out_rval);
                      }

                  case eok_vector_eq: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_EQ, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_ne: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_lt: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_LT, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_le: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_LE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_gt: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_GT, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_ge: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_GE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_land: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LOGICAL_AND, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_vector_lor: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LOGICAL_OR, type, r1, r2); return GCC_GEN_BE_SUCCESS;

                  case eok_vector_not: *out_rval = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, type, r1); return GCC_GEN_BE_SUCCESS;
                  case eok_negate:
                      {
                          a_boolean is_cplx = FALSE;
                          GCC_GEN_BE_CHECK(check_is_complex_type(expr->type, &is_cplx));
                          if (is_cplx) {
                              gcc_jit_type *c_type = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &c_type));
                              gcc_jit_type *elem_type = NULL;
                              GCC_GEN_BE_CHECK(get_complex_elem_jit_type(ctx, expr->type, &elem_type));
                              
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              
                              gcc_jit_rvalue *a = NULL, *b = NULL;
                              GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, cblock, r1, c_type, elem_type, &a, &b));
                              
                              gcc_jit_rvalue *neg_a = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, a);
                              gcc_jit_rvalue *neg_b = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, b);
                              
                              return construct_complex_from_parts(ctx, loc, cblock, c_type, elem_type, neg_a, neg_b, out_rval);
                          } else {
                              *out_rval = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, type, r1);
                              return GCC_GEN_BE_SUCCESS;
                          }
                      }
                  case eok_unary_plus: *out_rval = r1; return GCC_GEN_BE_SUCCESS;
                  case eok_complement: *out_rval = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_BITWISE_NEGATE, type, r1); return GCC_GEN_BE_SUCCESS;
                  case eok_not:
                      if (r1 && gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(r1)) != NULL) {
                          gcc_jit_rvalue *cmp = gcc_jit_context_new_comparison(
                              ctx, loc, GCC_JIT_COMPARISON_EQ, r1, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(r1)));
                          *out_rval = (gcc_jit_rvalue_get_type(cmp) == type) ? cmp : gcc_jit_context_new_cast(ctx, loc, cmp, type);
                      } else {
                          gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                          gcc_jit_rvalue *neg_b = gcc_jit_context_new_unary_op(
                              ctx, loc, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, bool_t, r1);
                          *out_rval = gcc_jit_context_new_cast(ctx, loc, neg_b, type);
                      }
                      return GCC_GEN_BE_SUCCESS;

                  case eok_real_part:
                  case eok_imag_part:
                      {
                          gcc_jit_block *cblock = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          gcc_jit_type *c_type = gcc_jit_rvalue_get_type(r1);
                          gcc_jit_rvalue *re_val = NULL, *im_val = NULL;
                          GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, cblock, r1, c_type, type, &re_val, &im_val));
                          *out_rval = (op == eok_real_part) ? re_val : im_val;
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_xconj:
                      {
                          gcc_jit_type *c_type = gcc_jit_rvalue_get_type(r1);
                          gcc_jit_type *elem_type = NULL;
                          GCC_GEN_BE_CHECK(get_complex_elem_jit_type(ctx, expr->type, &elem_type));
                          gcc_jit_block *cblock = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          gcc_jit_rvalue *re_val = NULL, *im_val = NULL;
                          GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, cblock, r1, c_type, elem_type, &re_val, &im_val));
                          gcc_jit_rvalue *neg_im = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, im_val);
                          return construct_complex_from_parts(ctx, loc, cblock, c_type, elem_type, re_val, neg_im, out_rval);
                      }


                  case eok_post_incr:
                  case eok_post_decr:
                      {
                          a_boolean post_is_bf = FALSE;
                          GCC_GEN_BE_CHECK(check_is_bitfield_access_op(op1, &post_is_bf));
                          if (post_is_bf) {
                              return gcc_gen_be_lower_bitfield_assignment(op1, op, NULL, out_rval);
                          }
                          gcc_jit_lvalue *l1 = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &l1));
                          if (l1) {
                              a_type_ptr atomic_post_tp = NULL;
                              GCC_GEN_BE_CHECK(resolve_lvalue_c11_atomic_type(op1, &atomic_post_tp));
                              if (atomic_post_tp) {
                                  return gcc_gen_be_lower_c11_atomic_rmw(op, atomic_post_tp, l1, NULL, out_rval);
                              }
                          }
                          
                          gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
                          
                          gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                          gcc_jit_lvalue *temp = gcc_jit_function_new_local(func, NULL, type, "post_inc_temp");
                          
                          gcc_jit_block_add_assignment(cblock, loc, temp, gcc_jit_lvalue_as_rvalue(l1));
                          
                          if (gcc_jit_type_is_pointer(type) != NULL) {
                              gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                              gcc_jit_rvalue *delta = gcc_jit_context_new_rvalue_from_int(
                                  ctx, int_type, op == eok_post_incr ? 1 : -1);
                              gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(
                                  ctx, loc, gcc_jit_lvalue_as_rvalue(l1), delta);
                              gcc_jit_rvalue *new_ptr = gcc_jit_lvalue_get_address(arr_acc, NULL);
                              gcc_jit_block_add_assignment(cblock, loc, l1, new_ptr);
                          } else {
                              gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, type, 1);
                              gcc_jit_block_add_assignment_op(cblock, NULL, l1, 
                                  op == eok_post_incr ? GCC_JIT_BINARY_OP_PLUS : GCC_JIT_BINARY_OP_MINUS, one);
                          }
                              
                          *out_rval = gcc_jit_lvalue_as_rvalue(temp);
                          return GCC_GEN_BE_SUCCESS;
                      }

                  case eok_address_of:
                      {
                          an_expr_node_ptr uop1 = op1;
                          while (uop1 && uop1->kind == enk_operation && uop1->variant.operation.kind == eok_parens) {
                              uop1 = uop1->variant.operation.operands;
                          }
                          a_type_ptr uop1_tp = NULL;
                          GCC_GEN_BE_CHECK(strip_expr_typerefs(uop1 ? uop1->type : NULL, &uop1_tp));
                          if (uop1 && (uop1->kind == enk_routine || (uop1_tp && uop1_tp->kind == tk_routine))) {
                              return gcc_gen_be_lower_expr_rvalue(uop1, out_rval);
                          }
                          gcc_jit_lvalue *l1 = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &l1));
                          *out_rval = gcc_jit_lvalue_get_address(l1, NULL);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_pm_field:
                  case eok_pm_points_to_field:
                  case eok_pre_incr:
                  case eok_pre_decr:
                      {
                          if (op == eok_pre_incr || op == eok_pre_decr) {
                              a_boolean pre_is_bf = FALSE;
                              GCC_GEN_BE_CHECK(check_is_bitfield_access_op(op1, &pre_is_bf));
                              if (pre_is_bf) {
                                  return gcc_gen_be_lower_bitfield_assignment(op1, op, NULL, out_rval);
                              }
                          }
                          if ((op == eok_pre_incr || op == eok_pre_decr) && op1) {
                              a_type_ptr atomic_pre_tp = NULL;
                              GCC_GEN_BE_CHECK(resolve_lvalue_c11_atomic_type(op1, &atomic_pre_tp));
                              if (atomic_pre_tp) {
                                  gcc_jit_lvalue *alval = NULL;
                                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &alval));
                                  return gcc_gen_be_lower_c11_atomic_rmw(op, atomic_pre_tp, alval, NULL, out_rval);
                              }
                          }
                          gcc_jit_lvalue *lexpr = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(expr, &lexpr));
                          if (lexpr) {
                              a_type_ptr etype = NULL;
                              GCC_GEN_BE_CHECK(strip_expr_typerefs(expr->type, &etype));
                              if (etype && etype->kind == tk_array) {
                                  gcc_jit_type *elem_type = NULL;
                                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(etype->variant.array.element_type, &elem_type));
                                  gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                                  *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(lexpr, NULL), elem_ptr_type);
                              } else {
                                  *out_rval = gcc_jit_lvalue_as_rvalue(lexpr);
                              }
                          } else {
                              *out_rval = NULL;
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }
                      
                  case eok_call:
                  case eok_dot_member_call:
                  case eok_points_to_member_call:
                  case eok_dot_pm_call:
                  case eok_points_to_pm_call:
                      {
                          if (!op1) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
                          gcc_jit_function *func = NULL;
                          gcc_jit_rvalue *fn_ptr = NULL;
                          int num_args = 0;

                          int is_member = (int)is_member_call_op;
                          
                          a_boolean is_direct_builtin = FALSE;
                          if (op1->kind == enk_routine && op1->variant.routine.ptr) {
                              a_routine_ptr brout = op1->variant.routine.ptr;
                              const char *rname = brout->source_corresp.name;
                              if (brout->special_kind == sfk_gnu_atomic_generic_function ||
                                  (brout->special_kind == sfk_none && brout->variant.builtin_function_kind != bfk_none) ||
                                  (rname && (strncmp(rname, "__builtin_", 10) == 0 ||
                                             strncmp(rname, "__atomic_", 9) == 0 ||
                                             strncmp(rname, "__sync_", 7) == 0 ||
                                             strncmp(rname, "__c11_atomic_", 13) == 0 ||
                                             strncmp(rname, "__c99_", 6) == 0))) {
                                  is_direct_builtin = TRUE;
                              }
                          }

                          if (op1->kind == enk_routine && !is_direct_builtin) {
                              GCC_GEN_BE_CHECK(resolve_cached_or_decl_func(op1->variant.routine.ptr, &func));
                          } else if (op1->kind != enk_routine) {
                              fn_ptr = r1;
                          }
                          
                          an_expr_node_ptr arg = op2;
                          while (arg) {
                              num_args++;
                              arg = arg->next;
                          }
                          
                          gcc_jit_rvalue **args = NULL;
                          a_boolean uses_sret = FALSE;
                          a_boolean edg_ret_as_param = FALSE;
                          gcc_jit_lvalue *sret_lval = NULL;
                          a_type_ptr call_ret_type = expr->type;
                          if (op1->type) {
                              a_type_ptr ctp = NULL;
                              GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &ctp));
                              if (ctp && ctp->kind == tk_pointer) {
                                  GCC_GEN_BE_CHECK(strip_expr_typerefs(ctp->variant.pointer.type, &ctp));
                              }
                              if (ctp && ctp->kind == tk_routine) {
                                  call_ret_type = ctp->variant.routine.return_type;
#if DO_IL_LOWERING
                                  if (ctp->variant.routine.extra_info &&
                                      ctp->variant.routine.extra_info->value_returned_as_parameter) {
                                      edg_ret_as_param = TRUE;
                                  }
#endif
                              }
                          }
                          if (op1->kind == enk_routine && op1->variant.routine.ptr && op1->variant.routine.ptr->type &&
                              op1->variant.routine.ptr->type->kind == tk_routine) {
                              call_ret_type = op1->variant.routine.ptr->type->variant.routine.return_type;
#if DO_IL_LOWERING
                              if (op1->variant.routine.ptr->type->variant.routine.extra_info &&
                                  op1->variant.routine.ptr->type->variant.routine.extra_info->value_returned_as_parameter) {
                                  edg_ret_as_param = TRUE;
                              }
#endif
                          }
                          if (!edg_ret_as_param && call_ret_type && !is_direct_builtin) {
                              extern gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;
                              GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(call_ret_type, &uses_sret));
                              if (uses_sret) num_args++;
                          }

                          if (num_args > 0) {
                              GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)num_args * sizeof(gcc_jit_rvalue *), (void **)&args));

                              int i = 0;
                              if (uses_sret) {
                                  gcc_jit_block *cblock = NULL;
                                  GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_get_current_block(&cblock), err, call_fail);
                                  gcc_jit_function *parent_func = gcc_jit_block_get_function(cblock);
                                  gcc_jit_type *sret_type = NULL;
                                  GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_lower_type(call_ret_type, &sret_type), err, call_fail);
                                  sret_lval = gcc_jit_function_new_local(parent_func, NULL, sret_type, "sret_buffer");
                                  args[i++] = gcc_jit_lvalue_get_address(sret_lval, NULL);
                              }

                              arg = op2;
                              if (is_member) {
                                  gcc_jit_rvalue *this_ptr = NULL;
                                  if (op == eok_dot_member_call || op == eok_dot_pm_call) {
                                      gcc_jit_lvalue *base_lval = NULL;
                                      GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_lower_expr_lvalue(arg, &base_lval), err, call_fail);
                                      if (base_lval) {
                                          this_ptr = gcc_jit_lvalue_get_address(base_lval, NULL);
                                      } else {
                                          GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_lower_expr_rvalue(arg, &this_ptr), err, call_fail);
                                      }
                                  } else {
                                      GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_lower_expr_rvalue(arg, &this_ptr), err, call_fail);
                                  }
                                  args[i++] = this_ptr;
                                  arg = arg->next;
                              }
                              
                              while (arg) {
                                  gcc_jit_rvalue *arg_rval = NULL;
                                  GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_lower_expr_rvalue(arg, &arg_rval), err, call_fail);
                                  
                                  extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
                                  extern gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;
                                  extern gcc_gen_be_error_t gcc_gen_be_coerce_Arg_to_registers(
                                      gcc_jit_function *fn,
                                      gcc_jit_block *blk,
                                      a_type_ptr atype,
                                      gcc_jit_rvalue *src_rval,
                                      gcc_jit_rvalue **out_coerced) GCC_GEN_BE_NOEXCEPT;
                                  a_boolean is_invis_ref = FALSE;
                                  GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_is_passed_by_invisible_reference(arg->type, &is_invis_ref), err, call_fail);
                                  if (is_invis_ref) {
                                      gcc_jit_block *cblock = NULL;
                                      GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_get_current_block(&cblock), err, call_fail);
                                      gcc_jit_function *parent_func = gcc_jit_block_get_function(cblock);
                                      gcc_jit_type *arg_type = gcc_jit_rvalue_get_type(arg_rval);
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(parent_func, NULL, arg_type, "invis_ref");
                                      gcc_jit_block_add_assignment(cblock, loc, tmp, arg_rval);
                                      arg_rval = gcc_jit_lvalue_get_address(tmp, NULL);
                                  } else if (arg->type && arg_rval &&
                                             (op1->kind != enk_routine || op1->variant.routine.ptr->variant.builtin_function_kind == bfk_none)) {
                                      gcc_jit_type *coerced_arg_t = NULL;
                                      a_boolean needs_coercion = FALSE;
                                      GCC_GEN_BE_CHECK_CLEANUP(
                                          gcc_gen_be_get_coerced_abi_type(arg->type, &coerced_arg_t, &needs_coercion),
                                          err, call_fail);
                                      if (needs_coercion && coerced_arg_t) {
                                          gcc_jit_block *cblock = NULL;
                                          GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_get_current_block(&cblock), err, call_fail);
                                          if (cblock) {
                                              GCC_GEN_BE_CHECK_CLEANUP(
                                                  gcc_gen_be_coerce_Arg_to_registers(
                                                      gcc_jit_block_get_function(cblock), cblock, arg->type, arg_rval, &arg_rval),
                                                  err, call_fail);
                                          }
                                      }
                                  }
                                  
                                  args[i] = arg_rval;
                                  i++;
                                  arg = arg->next;
                              }
                          }
                          
                          if (is_direct_builtin) {
                              a_routine_ptr rout = op1->variant.routine.ptr;
                              const char *bname = rout->source_corresp.name;
                              if (bname && strncmp(bname, "__c99_complex_", 14) == 0 && num_args >= 1) {
                                  gcc_jit_type *elem_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                                  const char *op_suf = NULL;
                                  if (strncmp(bname + 14, "float_", 6) == 0) {
                                      elem_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                                      op_suf = bname + 20;
                                  } else if (strncmp(bname + 14, "double_", 7) == 0) {
                                      elem_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                                      op_suf = bname + 21;
                                  } else if (strncmp(bname + 14, "long_double_", 12) == 0) {
                                      elem_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                                      op_suf = bname + 26;
                                  }
                                  gcc_jit_block *cblock = NULL;
                                  GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_get_current_block(&cblock), err, c99_cplx_done);
                                  if (op_suf && cblock) {
                                      gcc_jit_rvalue *ar = NULL;
                                      gcc_jit_rvalue *ai = NULL;
                                      GCC_GEN_BE_CHECK_CLEANUP(
                                          extract_complex_parts(ctx, loc, cblock, args[0], gcc_jit_rvalue_get_type(args[0]), elem_t, &ar, &ai),
                                          err, c99_cplx_done);
                                      gcc_jit_rvalue *rr = ar;
                                      gcc_jit_rvalue *ri = ai;
                                      if (strcmp(op_suf, "negate") == 0) {
                                          rr = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_t, ar);
                                          ri = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_t, ai);
                                      } else if (strcmp(op_suf, "conj") == 0 || strcmp(op_suf, "conjugate") == 0) {
                                          ri = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_t, ai);
                                      } else if (num_args >= 2) {
                                          gcc_jit_rvalue *br = NULL;
                                          gcc_jit_rvalue *bi = NULL;
                                          GCC_GEN_BE_CHECK_CLEANUP(
                                              extract_complex_parts(ctx, loc, cblock, args[1], gcc_jit_rvalue_get_type(args[1]), elem_t, &br, &bi),
                                              err, c99_cplx_done);
                                          if (strcmp(op_suf, "add") == 0) {
                                              rr = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_t, ar, br);
                                              ri = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_t, ai, bi);
                                          } else if (strcmp(op_suf, "subtract") == 0 || strcmp(op_suf, "sub") == 0) {
                                              rr = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, elem_t, ar, br);
                                              ri = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, elem_t, ai, bi);
                                          } else if (strcmp(op_suf, "multiply") == 0 || strcmp(op_suf, "mul") == 0) {
                                              gcc_jit_rvalue *ac = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_t, ar, br);
                                              gcc_jit_rvalue *bd = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_t, ai, bi);
                                              gcc_jit_rvalue *ad = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_t, ar, bi);
                                              gcc_jit_rvalue *bc = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_t, ai, br);
                                              rr = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, elem_t, ac, bd);
                                              ri = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_t, ad, bc);
                                          } else if (strcmp(op_suf, "divide") == 0 || strcmp(op_suf, "div") == 0) {
                                              gcc_jit_rvalue *ac = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_t, ar, br);
                                              gcc_jit_rvalue *bd = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_t, ai, bi);
                                              gcc_jit_rvalue *bc = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_t, ai, br);
                                              gcc_jit_rvalue *ad = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_t, ar, bi);
                                              gcc_jit_rvalue *c2 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_t, br, br);
                                              gcc_jit_rvalue *d2 = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, elem_t, bi, bi);
                                              gcc_jit_rvalue *den = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_t, c2, d2);
                                              gcc_jit_rvalue *nr = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, elem_t, ac, bd);
                                              gcc_jit_rvalue *ni = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, elem_t, bc, ad);
                                              rr = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_DIVIDE, elem_t, nr, den);
                                              ri = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_DIVIDE, elem_t, ni, den);
                                          }
                                      }
                                      err = construct_complex_from_parts(ctx, loc, cblock, type, elem_t, rr, ri, out_rval);
                                  }
                              c99_cplx_done:;
                                  free(args);
                                  return err;
                              }
                              if (bname && strncmp(bname, "__c99_", 6) == 0 && num_args == 1 && args[0]) {
                                  gcc_jit_block *cblock = NULL;
                                  GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_get_current_block(&cblock), err, c99_conv_done);
                                  if (cblock) {
                                      gcc_jit_type *arg_t = gcc_jit_rvalue_get_type(args[0]);
                                      const char *to_pos = strstr(bname, "_to_");
                                      const char *dst_part = to_pos ? (to_pos + 4) : bname;
                                      gcc_jit_type *elem_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                                      if (strstr(dst_part, "cfloat") != NULL) {
                                          elem_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                                      } else if (strstr(dst_part, "cldouble") != NULL || strstr(dst_part, "clong_double") != NULL) {
                                          elem_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                                      }
                                      if (gcc_jit_type_is_struct(arg_t) != NULL) {
                                          gcc_jit_type *src_elem_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                                          if (strncmp(bname + 6, "cfloat_", 7) == 0) {
                                              src_elem_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                                          } else if (strncmp(bname + 6, "cldouble_", 9) == 0 ||
                                                     strncmp(bname + 6, "clong_double_", 13) == 0) {
                                              src_elem_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                                          }
                                          gcc_jit_rvalue *ar = NULL;
                                          gcc_jit_rvalue *ai = NULL;
                                          GCC_GEN_BE_CHECK_CLEANUP(
                                              extract_complex_parts(ctx, loc, cblock, args[0], arg_t, src_elem_t, &ar, &ai),
                                              err, c99_conv_done);
                                          if (type && gcc_jit_type_is_struct(type) != NULL) {
                                              gcc_jit_rvalue *cr = (src_elem_t == elem_t) ? ar : gcc_jit_context_new_cast(ctx, loc, ar, elem_t);
                                              gcc_jit_rvalue *ci = (src_elem_t == elem_t) ? ai : gcc_jit_context_new_cast(ctx, loc, ai, elem_t);
                                              err = construct_complex_from_parts(ctx, loc, cblock, type, elem_t, cr, ci, out_rval);
                                          } else if (type) {
                                              gcc_jit_rvalue *part = (strstr(bname, "_to_i") != NULL) ? ai : ar;
                                              *out_rval = (gcc_jit_rvalue_get_type(part) == type)
                                                  ? part
                                                  : gcc_jit_context_new_cast(ctx, loc, part, type);
                                          }
                                      } else if (type && gcc_jit_type_is_struct(type) != NULL) {
                                          gcc_jit_rvalue *sc = (arg_t == elem_t)
                                              ? args[0]
                                              : gcc_jit_context_new_cast(ctx, loc, args[0], elem_t);
                                          gcc_jit_rvalue *zr = gcc_jit_context_zero(ctx, elem_t);
                                          a_boolean from_imag = (strncmp(bname, "__c99_i", 7) == 0) ? TRUE : FALSE;
                                          err = construct_complex_from_parts(
                                              ctx, loc, cblock, type, elem_t, from_imag ? zr : sc, from_imag ? sc : zr, out_rval);
                                      }
                                  }
                              c99_conv_done:;
                                  free(args);
                                  return err;
                              }
                              if (bname && (strcmp(bname, "__builtin_FILE") == 0 ||
                                            strcmp(bname, "__builtin_FILE_NAME") == 0 ||
                                            strcmp(bname, "__builtin_LINE") == 0 ||
                                            strcmp(bname, "__builtin_COLUMN") == 0 ||
                                            strcmp(bname, "__builtin_FUNCTION") == 0 ||
                                            strcmp(bname, "__builtin_source_location") == 0)) {
                                  free(args);
                                  return gcc_gen_be_lower_source_location_builtin(
                                      bname, &expr->expr_range.start, expr->type, out_rval);
                              }
                              if (rout->special_kind == sfk_gnu_atomic_generic_function && bname) {
                                  unsigned long explicit_sz = 0UL;
                                  if (op2 && op2->kind == enk_constant &&
                                      op2->variant.constant.ptr->kind == ck_integer) {
                                      a_boolean ovf = FALSE;
                                      explicit_sz = (unsigned long)unsigned_value_of_integer_constant(
                                          op2->variant.constant.ptr, &ovf);
                                  }
                                  err = gcc_gen_be_lower_generic_atomic_call(
                                      bname, rout, num_args, args, explicit_sz, out_rval);
                              } else {
                                  err = gcc_gen_be_lower_builtin_call(rout, num_args, args, out_rval);
                              }
                              free(args);
                              GCC_GEN_BE_CHECK(err);
                              if (*out_rval &&
                                  type != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID) &&
                                  gcc_jit_rvalue_get_type(*out_rval) != type) {
                                  GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, *out_rval, type, out_rval));
                              }
                              return GCC_GEN_BE_SUCCESS;
                          }

                            if (is_member && op1->kind != enk_routine && num_args > 0) {
                                int this_arg_idx = (uses_sret || edg_ret_as_param) ? 1 : 0;
                                gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                                gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
                                gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
                                gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                                
                                gcc_jit_type *struct_type = gcc_jit_rvalue_get_type(fn_ptr);
                                gcc_jit_field *f_func_ptr = gcc_jit_struct_get_field(gcc_jit_type_is_struct(struct_type), 0);
                                gcc_jit_field *f_this_adj = gcc_jit_struct_get_field(gcc_jit_type_is_struct(struct_type), 1);
                                
                                gcc_jit_rvalue *func_ptr_val = gcc_jit_rvalue_access_field(fn_ptr, NULL, f_func_ptr);
                                gcc_jit_rvalue *this_adj_val = gcc_jit_rvalue_access_field(fn_ptr, NULL, f_this_adj);
                                
                                gcc_gen_be_target_abi_t pmf_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
                                GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_get_target_abi(&pmf_abi), err, call_fail);
                                a_boolean arm_pmf = (pmf_abi == GCC_GEN_BE_TARGET_ABI_AAPCS64) ? TRUE : FALSE;

                                gcc_jit_rvalue *one_long = gcc_jit_context_new_rvalue_from_long(ctx, long_type, 1);
                                gcc_jit_rvalue *func_ptr_as_long = NULL;
                                GCC_GEN_BE_CHECK_CLEANUP(safe_ptr_or_int_cast(ctx, func_ptr_val, long_type, &func_ptr_as_long), err, call_fail);

                                gcc_jit_rvalue *actual_this_adj = arm_pmf
                                    ? gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, long_type, this_adj_val, one_long)
                                    : this_adj_val;

                                /* Adjust this: (char*)this + actual_this_adj */
                                gcc_jit_rvalue *this_char_ptr = gcc_jit_context_new_cast(ctx, loc, args[this_arg_idx], char_ptr_type);
                                gcc_jit_lvalue *adjusted_this = gcc_jit_context_new_array_access(ctx, loc, this_char_ptr, actual_this_adj);
                                gcc_jit_rvalue *new_this_ptr = gcc_jit_lvalue_get_address(adjusted_this, NULL);
                                args[this_arg_idx] = gcc_jit_context_new_cast(ctx, loc, new_this_ptr, gcc_jit_rvalue_get_type(args[this_arg_idx]));
                                
                                /* Test virtual bit: (this_adj & 1) on ARM64 vs ((long)func_ptr & 1) on x86_64 */
                                gcc_jit_rvalue *virt_carrier = arm_pmf ? this_adj_val : func_ptr_as_long;
                                gcc_jit_rvalue *is_virtual = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, long_type, virt_carrier, one_long);
                                gcc_jit_rvalue *is_virtual_bool = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, is_virtual, gcc_jit_context_new_rvalue_from_long(ctx, long_type, 0));
                                
                                gcc_jit_block *cblock = NULL;
                                GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_get_current_block(&cblock), err, call_fail);
                                gcc_jit_function *parent_func = gcc_jit_block_get_function(cblock);
                                
                                gcc_jit_block *virt_block = gcc_jit_function_new_block(parent_func, "pm_virt");
                                gcc_jit_block *non_virt_block = gcc_jit_function_new_block(parent_func, "pm_non_virt");
                                gcc_jit_block *merge_block = gcc_jit_function_new_block(parent_func, "pm_merge");
                                
                                gcc_jit_block_end_with_conditional(cblock, loc, is_virtual_bool, virt_block, non_virt_block);
                                
                                a_type_ptr op1_type = NULL;
                                GCC_GEN_BE_CHECK_CLEANUP(strip_expr_typerefs(op1->type, &op1_type), err, call_fail);
                                
                                gcc_jit_type *member_fn_type = NULL;
                                GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_lower_type(op1_type->variant.ptr_to_member.type, &member_fn_type), err, call_fail);
                                gcc_jit_type *fn_ptr_type = (gcc_jit_type_is_pointer(member_fn_type) != NULL)
                                    ? member_fn_type
                                    : gcc_jit_type_get_pointer(member_fn_type);
                                
                                gcc_jit_lvalue *resolved_fn_ptr = gcc_jit_function_new_local(parent_func, NULL, fn_ptr_type, "resolved_fn_ptr");
                                
                                /* non-virtual: just cast func_ptr_val */
                                GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_set_current_block(non_virt_block), err, call_fail);
                                gcc_jit_block_add_assignment(non_virt_block, NULL, resolved_fn_ptr, gcc_jit_context_new_cast(ctx, loc, func_ptr_val, fn_ptr_type));
                                gcc_jit_block_end_with_jump(non_virt_block, NULL, merge_block);
                                
                                /* virtual: read vtable (`func_ptr` directly on ARM64, `func_ptr - 1` on x86_64) */
                                GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_set_current_block(virt_block), err, call_fail);
                                gcc_jit_type *vtable_ptr_type = gcc_jit_type_get_pointer(void_ptr_type);
                                gcc_jit_type *vtable_ptr_ptr_type = gcc_jit_type_get_pointer(vtable_ptr_type);
                                gcc_jit_rvalue *this_vptr_ptr = gcc_jit_context_new_cast(ctx, loc, new_this_ptr, vtable_ptr_ptr_type);
                                gcc_jit_rvalue *vptr = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(this_vptr_ptr, NULL));
                                
                                gcc_jit_rvalue *vtable_char_ptr = gcc_jit_context_new_cast(ctx, loc, vptr, char_ptr_type);
                                gcc_jit_rvalue *vfunc_offset = arm_pmf
                                    ? func_ptr_as_long
                                    : gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, long_type, func_ptr_as_long, one_long);
                                gcc_jit_lvalue *vfunc_addr_lval = gcc_jit_context_new_array_access(ctx, loc, vtable_char_ptr, vfunc_offset);
                                gcc_jit_rvalue *vfunc_addr = gcc_jit_lvalue_get_address(vfunc_addr_lval, NULL);
                                
                                gcc_jit_rvalue *vfunc_void_ptr_ptr = gcc_jit_context_new_cast(ctx, loc, vfunc_addr, vtable_ptr_ptr_type);
                                gcc_jit_rvalue *vfunc_void_ptr = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(vfunc_void_ptr_ptr, NULL));
                                
                                gcc_jit_block_add_assignment(virt_block, NULL, resolved_fn_ptr, gcc_jit_context_new_cast(ctx, loc, vfunc_void_ptr, fn_ptr_type));
                                gcc_jit_block_end_with_jump(virt_block, NULL, merge_block);
                                
                                GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_set_current_block(merge_block), err, call_fail);
                                fn_ptr = gcc_jit_lvalue_as_rvalue(resolved_fn_ptr);
                                func = NULL;
                            }

                          if (expr->variant.operation.is_virtual_call && op1->kind == enk_routine && num_args > 0) {
                              a_routine_ptr rout = op1->variant.routine.ptr;
                              int vindex = rout->number.virtual_function;
                              if (vindex != VIRTUAL_FUNCTION_NUMBER_NONE) {
                                  int this_arg_idx = (uses_sret || edg_ret_as_param) ? 1 : 0;
                                  gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                                  gcc_jit_type *vtable_ptr_type = gcc_jit_type_get_pointer(void_ptr_type);
                                  gcc_jit_type *vtable_ptr_ptr_type = gcc_jit_type_get_pointer(vtable_ptr_type);
                                  
                                  gcc_jit_rvalue *this_ptr_casted = gcc_jit_context_new_cast(ctx, loc, args[this_arg_idx], vtable_ptr_ptr_type);
                                  gcc_jit_lvalue *vptr_lval = gcc_jit_rvalue_dereference(this_ptr_casted, NULL);
                                  gcc_jit_rvalue *vptr = gcc_jit_lvalue_as_rvalue(vptr_lval);
                                  
                                  gcc_jit_rvalue *idx_rval = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 2 + vindex);
                                  gcc_jit_lvalue *fn_ptr_lval = gcc_jit_context_new_array_access(ctx, loc, vptr, idx_rval);
                                  gcc_jit_rvalue *raw_fn_ptr = gcc_jit_lvalue_as_rvalue(fn_ptr_lval);
                                  
                                  gcc_jit_type *fn_ptr_type = NULL;
                                  GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_lower_type(rout->type, &fn_ptr_type), err, call_fail);
                                  if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info) {
                                      int p_idx = (uses_sret || edg_ret_as_param) ? 1 : 0;
                                      for (a_param_type_ptr ptp = rout->type->variant.routine.extra_info->param_type_list;
                                           ptp != NULL && p_idx < num_args;
                                           ptp = ptp->next, ++p_idx) {
                                          gcc_jit_type *expected_pt = NULL;
                                          GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_lower_type(ptp->type, &expected_pt), err, call_fail);
                                          extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
                                          extern gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;
                                          a_boolean is_invis = FALSE;
                                          if (ptp->type) {
                                              GCC_GEN_BE_CHECK_CLEANUP(
                                                  gcc_gen_be_is_passed_by_invisible_reference(ptp->type, &is_invis),
                                                  err, call_fail);
                                          }
                                          if (is_invis) {
                                              expected_pt = gcc_jit_type_get_pointer(gcc_jit_type_unqualified(expected_pt));
                                          } else if (ptp->type) {
                                              gcc_jit_type *coerced_pt = NULL;
                                              a_boolean needs_coercion = FALSE;
                                              GCC_GEN_BE_CHECK_CLEANUP(
                                                  gcc_gen_be_get_coerced_abi_type(ptp->type, &coerced_pt, &needs_coercion),
                                                  err, call_fail);
                                              if (needs_coercion && coerced_pt) {
                                                  expected_pt = coerced_pt;
                                              }
                                          }
                                          GCC_GEN_BE_CHECK_CLEANUP(
                                              reconcile_call_arg_rvalue(ctx, loc, args[p_idx], expected_pt, &args[p_idx]),
                                              err, call_fail);
                                      }
                                  }
                                  fn_ptr = gcc_jit_context_new_cast(ctx, loc, raw_fn_ptr, fn_ptr_type);
                                  func = NULL;
                              }
                          }

                          if (func) {
                              int param_count = (int)gcc_jit_function_get_param_count(func);
                              for (int i = 0; i < num_args && i < param_count; ++i) {
                                  gcc_jit_param *param = gcc_jit_function_get_param(func, i);
                                  gcc_jit_type *param_type = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(param));
                                  GCC_GEN_BE_CHECK_CLEANUP(
                                      reconcile_call_arg_rvalue(ctx, loc, args[i], param_type, &args[i]),
                                      err, call_fail);
                              }
                              GCC_GEN_BE_CHECK_CLEANUP(
                                  finish_direct_or_indirect_call(
                                      ctx, loc, func, NULL, num_args, args, uses_sret, sret_lval, type, out_rval),
                                  err, call_fail);
                          } else if (fn_ptr) {
#if DO_IL_LOWERING && defined(__aarch64__) && defined(LIBGCCJIT_HAVE_ASM_STATEMENTS)
                              if ((uses_sret || edg_ret_as_param) && num_args >= 1) {
                                  gcc_gen_be_target_abi_t icall_abi = GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64;
                                  GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_get_target_abi(&icall_abi), err, call_fail);
                                  if (icall_abi == GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64 ||
                                      icall_abi == GCC_GEN_BE_TARGET_ABI_AAPCS64) {
                                      static unsigned long icall_x8_seq = 0UL;
                                      char icall_name[64];
                                      snprintf(icall_name, sizeof(icall_name), "__edg_x8_icall_%lu", ++icall_x8_seq);
                                      gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                                      gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                                      gcc_jit_param **iparams = NULL;
                                      GCC_GEN_BE_CHECK_CLEANUP(
                                          gcc_gen_be_alloc((size_t)(num_args + 1) * sizeof(gcc_jit_param *), (void **)&iparams),
                                          err, call_fail);
                                      iparams[0] = gcc_jit_context_new_param(ctx, loc, vp_t, "target_fn");
                                      for (int ai = 0; ai < num_args; ++ai) {
                                          char pnm[32];
                                          snprintf(pnm, sizeof(pnm), "p%d", ai);
                                          iparams[ai + 1] = gcc_jit_context_new_param(
                                              ctx, loc, gcc_jit_rvalue_get_type(args[ai]), pnm);
                                      }
                                      gcc_jit_function *icall_fn = gcc_jit_context_new_function(
                                          ctx, loc, GCC_JIT_FUNCTION_IMPORTED, void_t, icall_name, num_args + 1, iparams, 0);
                                      free(iparams);
                                      char asm_buf[512];
#if defined(__APPLE__) && defined(__MACH__)
                                      snprintf(
                                          asm_buf, sizeof(asm_buf),
                                          ".text\n.p2align 2\n_%s:\n"
                                          "  mov x16, x0\n  mov x8, x1\n  mov x0, x2\n  mov x1, x3\n"
                                          "  mov x2, x4\n  mov x3, x5\n  mov x4, x6\n  mov x5, x7\n"
                                          "  ldr x6, [sp]\n  br x16\n",
                                          icall_name);
#else
                                      snprintf(
                                          asm_buf, sizeof(asm_buf),
                                          ".text\n.p2align 2\n%s:\n"
                                          "  mov x16, x0\n  mov x8, x1\n  mov x0, x2\n  mov x1, x3\n"
                                          "  mov x2, x4\n  mov x3, x5\n  mov x4, x6\n  mov x5, x7\n"
                                          "  ldr x6, [sp]\n  br x16\n",
                                          icall_name);
#endif
                                      gcc_jit_context_add_top_level_asm(ctx, NULL, asm_buf);
                                      gcc_jit_rvalue **iargs = NULL;
                                      GCC_GEN_BE_CHECK_CLEANUP(
                                          gcc_gen_be_alloc((size_t)(num_args + 1) * sizeof(gcc_jit_rvalue *), (void **)&iargs),
                                          err, call_fail);
                                      iargs[0] = gcc_jit_context_new_cast(ctx, loc, fn_ptr, vp_t);
                                      for (int ai = 0; ai < num_args; ++ai) {
                                          iargs[ai + 1] = args[ai];
                                      }
                                      free(args);
                                      args = iargs;
                                      num_args += 1;
                                      func = icall_fn;
                                      fn_ptr = NULL;
                                  }
                              }
#endif
                              GCC_GEN_BE_CHECK_CLEANUP(
                                  finish_direct_or_indirect_call(
                                      ctx, loc, func, fn_ptr, num_args, args, uses_sret, sret_lval, type, out_rval),
                                  err, call_fail);
                          } else {
                              free(args);
                              return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
                          }

                          free(args);
                          return GCC_GEN_BE_SUCCESS;
                      call_fail:;
                          free(args);
                          return err;
                      }
                      
                  case eok_va_start:
                  case eok_va_start_single_operand:
                  case eok_va_arg:
                  case eok_va_end:
                  case eok_va_copy:
                      {
                          if (op == eok_va_arg) {
                              /* Architecture-aware va_arg lowering:
                                 Check if ap is a struct/array (e.g., System V AMD64 __va_list_tag[1] or AAPCS64 Linux struct __va_list)
                                 or a scalar pointer (e.g., Darwin ARM64 char*, Windows x64 char*). */
                              gcc_jit_lvalue *ap_lval = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &ap_lval));
                              
                              gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                              gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
                              gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));

                              a_type_ptr ap_edg_type = NULL;
                              GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &ap_edg_type));

                              if (ap_edg_type && (ap_edg_type->kind == tk_array || ap_edg_type->kind == tk_struct || ap_edg_type->kind == tk_class)) {
                                  /* System V AMD64 (__va_list_tag[1]) and Linux AAPCS64 (struct __va_list) state-machine lowering:
                                     - SysV AMD64 layout: { unsigned int gp_offset (0); unsigned int fp_offset (4); void *overflow_arg_area (8); void *reg_save_area (16); }
                                     - Linux AAPCS64 layout: { void *__stack (0); void *__gr_top (8); void *__vr_top (16); int __gr_offs (24); int __vr_offs (28); } */
                                  gcc_gen_be_target_abi_t target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
                                  GCC_GEN_BE_CHECK(gcc_gen_be_get_target_abi(&target_abi));

                                  a_boolean is_stack = FALSE;
                                  int num_gp = 0;
                                  int num_fp = 0;
                                  if (expr->type) {
                                      GCC_GEN_BE_CHECK(gcc_gen_be_classify_va_arg(expr->type, &is_stack, &num_gp, &num_fp));
                                  }

                                  gcc_jit_type *uint_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT);
                                  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                  gcc_jit_type *uint_ptr_type = gcc_jit_type_get_pointer(uint_type);
                                  gcc_jit_type *int_ptr_type = gcc_jit_type_get_pointer(int_type);
                                  gcc_jit_type *char_ptr_ptr_type = gcc_jit_type_get_pointer(char_ptr_type);
                                  gcc_jit_type *type_ptr = gcc_jit_type_get_pointer(type);

                                  gcc_jit_rvalue *ap_base = gcc_jit_context_new_cast(
                                      ctx, loc, gcc_jit_lvalue_get_address(ap_lval, loc), char_ptr_type);

                                  size_t type_size = expr->type ? (size_t)f_size_of_type(expr->type) : 0;
                                  if (type_size == 0 && gcc_jit_type_is_pointer(type) == NULL && gcc_jit_type_is_struct(type) == NULL) {
                                      type_size = (type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT) ||
                                                   type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT)) ? 4 : 8;
                                  }
                                  if (type_size == 0) type_size = 8;
                                  size_t type_align = expr->type ? (size_t)expr->type->alignment : 8;
                                  if (type_align < 8) type_align = 8;
                                  size_t aligned_size = (type_size + 7) & ~(size_t)7;

                                  if (!cblock) {
                                      gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, loc, ap_base, type_ptr);
                                      *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(typed_ptr, loc));
                                      return GCC_GEN_BE_SUCCESS;
                                  }

                                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                                  gcc_jit_lvalue *va_res = gcc_jit_function_new_local(func, loc, type, "va_arg_res");
                                  gcc_jit_block *reg_blk = gcc_jit_function_new_block(func, "va_arg_reg");
                                  gcc_jit_block *ovf_blk = gcc_jit_function_new_block(func, "va_arg_overflow");
                                  gcc_jit_block *merge_blk = gcc_jit_function_new_block(func, "va_arg_merge");

                                  if (target_abi == GCC_GEN_BE_TARGET_ABI_AAPCS64) {
                                      /* Linux AAPCS64 struct __va_list */
                                      gcc_jit_lvalue *stack_slot = gcc_jit_rvalue_dereference(
                                          gcc_jit_context_new_cast(ctx, loc, ap_base, char_ptr_ptr_type), loc);
                                      if (!is_stack && (num_gp > 0 || num_fp > 0)) {
                                          long top_off = (num_fp > 0) ? 16L : 8L;
                                          long offs_off = (num_fp > 0) ? 28L : 24L;
                                          int step = (num_fp > 0) ? (num_fp * 16) : (num_gp * 8);

                                          gcc_jit_rvalue *offs_addr = gcc_jit_lvalue_get_address(
                                              gcc_jit_context_new_array_access(
                                                  ctx, loc, ap_base,
                                                  gcc_jit_context_new_rvalue_from_long(ctx, long_type, offs_off)), loc);
                                          gcc_jit_lvalue *offs_lval = gcc_jit_rvalue_dereference(
                                              gcc_jit_context_new_cast(ctx, loc, offs_addr, int_ptr_type), loc);
                                          gcc_jit_rvalue *offs_val = gcc_jit_lvalue_as_rvalue(offs_lval);
                                          gcc_jit_rvalue *zero_int = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                                          gcc_jit_rvalue *in_regs = gcc_jit_context_new_comparison(
                                              ctx, loc, GCC_JIT_COMPARISON_LT, offs_val, zero_int);
                                          gcc_jit_block_end_with_conditional(cblock, loc, in_regs, reg_blk, ovf_blk);

                                          /* reg_blk */
                                          gcc_jit_rvalue *top_addr = gcc_jit_lvalue_get_address(
                                              gcc_jit_context_new_array_access(
                                                  ctx, loc, ap_base,
                                                  gcc_jit_context_new_rvalue_from_long(ctx, long_type, top_off)), loc);
                                          gcc_jit_rvalue *top_ptr = gcc_jit_lvalue_as_rvalue(
                                              gcc_jit_rvalue_dereference(
                                                  gcc_jit_context_new_cast(ctx, loc, top_addr, char_ptr_ptr_type), loc));
                                          gcc_jit_rvalue *reg_elem_addr = gcc_jit_lvalue_get_address(
                                              gcc_jit_context_new_array_access(
                                                  ctx, loc, top_ptr,
                                                  gcc_jit_context_new_cast(ctx, loc, offs_val, long_type)), loc);
                                          gcc_jit_rvalue *new_offs = gcc_jit_context_new_binary_op(
                                              ctx, loc, GCC_JIT_BINARY_OP_PLUS, int_type, offs_val,
                                              gcc_jit_context_new_rvalue_from_int(ctx, int_type, step));
                                          gcc_jit_block_add_assignment(reg_blk, loc, offs_lval, new_offs);
                                          gcc_jit_rvalue * use_reg = gcc_jit_context_new_comparison(
                                              ctx, loc, GCC_JIT_COMPARISON_LE, new_offs, zero_int);
                                          gcc_jit_block *reg_load_blk = gcc_jit_function_new_block(func, "va_aapcs_reg_load");
                                          gcc_jit_block_end_with_conditional(reg_blk, loc, use_reg, reg_load_blk, ovf_blk);
                                          gcc_jit_block_add_assignment(
                                              reg_load_blk, loc, va_res,
                                              gcc_jit_lvalue_as_rvalue(
                                                  gcc_jit_rvalue_dereference(
                                                      gcc_jit_context_new_cast(ctx, loc, reg_elem_addr, type_ptr), loc)));
                                          gcc_jit_block_end_with_jump(reg_load_blk, loc, merge_blk);
                                      } else {
                                          gcc_jit_block_end_with_jump(cblock, loc, ovf_blk);
                                      }
                                      /* ovf_blk */
                                      gcc_jit_rvalue *cur_stack = gcc_jit_lvalue_as_rvalue(stack_slot);
                                      if (type_align > 8) {
                                          gcc_jit_rvalue *align_m1 = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)(type_align - 1));
                                          gcc_jit_rvalue *neg_align = gcc_jit_context_new_rvalue_from_long(ctx, long_type, -((long)type_align));
                                          gcc_jit_rvalue *cur_int = p_gcc_jit_context_new_bitcast
                                              ? p_gcc_jit_context_new_bitcast(ctx, loc, cur_stack, long_type)
                                              : gcc_jit_context_new_cast(ctx, loc, cur_stack, long_type);
                                          gcc_jit_rvalue *aligned_int = gcc_jit_context_new_binary_op(
                                              ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, long_type,
                                              gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, long_type, cur_int, align_m1),
                                              neg_align);
                                          cur_stack = p_gcc_jit_context_new_bitcast
                                              ? p_gcc_jit_context_new_bitcast(ctx, loc, aligned_int, char_ptr_type)
                                              : gcc_jit_context_new_cast(ctx, loc, aligned_int, char_ptr_type);
                                      }
                                      a_boolean va_is_rec = FALSE;
                                      GCC_GEN_BE_CHECK(check_is_record_type(expr->type, &va_is_rec));
                                      a_boolean is_indirect_agg = (num_fp == 0 && type_size > 16 && va_is_rec)
                                          ? TRUE : FALSE;
                                      if (is_indirect_agg) {
                                          gcc_jit_type *type_ptr_ptr = gcc_jit_type_get_pointer(type_ptr);
                                          gcc_jit_rvalue *ind_ptr = gcc_jit_lvalue_as_rvalue(
                                              gcc_jit_rvalue_dereference(
                                                  gcc_jit_context_new_cast(ctx, loc, cur_stack, type_ptr_ptr), loc));
                                          gcc_jit_block_add_assignment(
                                              ovf_blk, loc, va_res,
                                              gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(ind_ptr, loc)));
                                          aligned_size = 8;
                                      } else {
                                          gcc_jit_block_add_assignment(
                                              ovf_blk, loc, va_res,
                                              gcc_jit_lvalue_as_rvalue(
                                                  gcc_jit_rvalue_dereference(
                                                      gcc_jit_context_new_cast(ctx, loc, cur_stack, type_ptr), loc)));
                                      }
                                      gcc_jit_rvalue *next_stack = gcc_jit_lvalue_get_address(
                                          gcc_jit_context_new_array_access(
                                              ctx, loc, cur_stack,
                                              gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)aligned_size)), loc);
                                      gcc_jit_block_add_assignment(ovf_blk, loc, stack_slot, next_stack);
                                      gcc_jit_block_end_with_jump(ovf_blk, loc, merge_blk);

                                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_blk));
                                      *out_rval = gcc_jit_lvalue_as_rvalue(va_res);
                                      return GCC_GEN_BE_SUCCESS;
                                  }

                                  /* System V AMD64 __va_list_tag state machine */
                                  gcc_jit_rvalue *ovf_field_addr = gcc_jit_lvalue_get_address(
                                      gcc_jit_context_new_array_access(
                                          ctx, loc, ap_base,
                                          gcc_jit_context_new_rvalue_from_long(ctx, long_type, 8L)), loc);
                                  gcc_jit_lvalue *ovf_slot = gcc_jit_rvalue_dereference(
                                      gcc_jit_context_new_cast(ctx, loc, ovf_field_addr, char_ptr_ptr_type), loc);

                                  gcc_jit_rvalue *reg_save_field_addr = gcc_jit_lvalue_get_address(
                                      gcc_jit_context_new_array_access(
                                          ctx, loc, ap_base,
                                          gcc_jit_context_new_rvalue_from_long(ctx, long_type, 16L)), loc);
                                  gcc_jit_lvalue *reg_save_slot = gcc_jit_rvalue_dereference(
                                      gcc_jit_context_new_cast(ctx, loc, reg_save_field_addr, char_ptr_ptr_type), loc);

                                  if (!is_stack && ((num_gp > 0) != (num_fp > 0))) {
                                      long off_field_byte = (num_fp > 0) ? 4L : 0L;
                                      int limit = (num_fp > 0) ? (176 - num_fp * 16) : (48 - num_gp * 8);
                                      int step = (num_fp > 0) ? (num_fp * 16) : (num_gp * 8);

                                      gcc_jit_rvalue *off_addr = gcc_jit_lvalue_get_address(
                                          gcc_jit_context_new_array_access(
                                              ctx, loc, ap_base,
                                              gcc_jit_context_new_rvalue_from_long(ctx, long_type, off_field_byte)), loc);
                                      gcc_jit_lvalue *off_lval = gcc_jit_rvalue_dereference(
                                          gcc_jit_context_new_cast(ctx, loc, off_addr, uint_ptr_type), loc);
                                      gcc_jit_rvalue *off_val = gcc_jit_lvalue_as_rvalue(off_lval);
                                      gcc_jit_rvalue *limit_val = gcc_jit_context_new_rvalue_from_int(ctx, uint_type, limit);
                                      gcc_jit_rvalue *fits_in_regs = gcc_jit_context_new_comparison(
                                          ctx, loc, GCC_JIT_COMPARISON_LE, off_val, limit_val);
                                      gcc_jit_block_end_with_conditional(cblock, loc, fits_in_regs, reg_blk, ovf_blk);

                                      /* reg_blk: load from reg_save_area + off_val, advance off_val += step */
                                      gcc_jit_rvalue *reg_save_base = gcc_jit_lvalue_as_rvalue(reg_save_slot);
                                      gcc_jit_rvalue *reg_addr = gcc_jit_lvalue_get_address(
                                          gcc_jit_context_new_array_access(
                                              ctx, loc, reg_save_base,
                                              gcc_jit_context_new_cast(ctx, loc, off_val, long_type)), loc);
                                      gcc_jit_block_add_assignment(
                                          reg_blk, loc, va_res,
                                          gcc_jit_lvalue_as_rvalue(
                                              gcc_jit_rvalue_dereference(
                                                  gcc_jit_context_new_cast(ctx, loc, reg_addr, type_ptr), loc)));
                                      gcc_jit_rvalue *next_off = gcc_jit_context_new_binary_op(
                                          ctx, loc, GCC_JIT_BINARY_OP_PLUS, uint_type, off_val,
                                          gcc_jit_context_new_rvalue_from_int(ctx, uint_type, step));
                                      gcc_jit_block_add_assignment(reg_blk, loc, off_lval, next_off);
                                      gcc_jit_block_end_with_jump(reg_blk, loc, merge_blk);
                                  } else if (!is_stack && num_gp == 1 && num_fp == 1) {
                                      /* Mixed INTEGER + SSE 16-byte aggregate spanning both gp_offset and fp_offset */
                                      gcc_gen_be_abi_class_t sysv_classes[2];
                                      GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(expr->type, sysv_classes));

                                      gcc_jit_rvalue *gp_off_addr = gcc_jit_lvalue_get_address(
                                          gcc_jit_context_new_array_access(
                                              ctx, loc, ap_base,
                                              gcc_jit_context_new_rvalue_from_long(ctx, long_type, 0L)), loc);
                                      gcc_jit_lvalue *gp_off_lval = gcc_jit_rvalue_dereference(
                                          gcc_jit_context_new_cast(ctx, loc, gp_off_addr, uint_ptr_type), loc);
                                      gcc_jit_rvalue *gp_off_val = gcc_jit_lvalue_as_rvalue(gp_off_lval);

                                      gcc_jit_rvalue *fp_off_addr = gcc_jit_lvalue_get_address(
                                          gcc_jit_context_new_array_access(
                                              ctx, loc, ap_base,
                                              gcc_jit_context_new_rvalue_from_long(ctx, long_type, 4L)), loc);
                                      gcc_jit_lvalue *fp_off_lval = gcc_jit_rvalue_dereference(
                                          gcc_jit_context_new_cast(ctx, loc, fp_off_addr, uint_ptr_type), loc);
                                      gcc_jit_rvalue *fp_off_val = gcc_jit_lvalue_as_rvalue(fp_off_lval);

                                      gcc_jit_rvalue *gp_fits = gcc_jit_context_new_comparison(
                                          ctx, loc, GCC_JIT_COMPARISON_LE, gp_off_val,
                                          gcc_jit_context_new_rvalue_from_int(ctx, uint_type, 40));
                                      gcc_jit_rvalue *fp_fits = gcc_jit_context_new_comparison(
                                          ctx, loc, GCC_JIT_COMPARISON_LE, fp_off_val,
                                          gcc_jit_context_new_rvalue_from_int(ctx, uint_type, 160));
                                      gcc_jit_block *check_fp_blk = gcc_jit_function_new_block(func, "va_mixed_check_fp");
                                      gcc_jit_block_end_with_conditional(cblock, loc, gp_fits, check_fp_blk, ovf_blk);
                                      gcc_jit_block_end_with_conditional(check_fp_blk, loc, fp_fits, reg_blk, ovf_blk);

                                      /* reg_blk: copy GP eightbyte and FP eightbyte into va_res */
                                      gcc_jit_type *u64_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
                                      gcc_jit_type *u64_ptr_t = gcc_jit_type_get_pointer(u64_t);
                                      gcc_jit_rvalue *res_u64_ptr = gcc_jit_context_new_cast(
                                          ctx, loc, gcc_jit_lvalue_get_address(va_res, loc), u64_ptr_t);
                                      gcc_jit_rvalue *reg_save_base = gcc_jit_lvalue_as_rvalue(reg_save_slot);

                                      gcc_jit_rvalue *gp_src_addr = gcc_jit_lvalue_get_address(
                                          gcc_jit_context_new_array_access(
                                              ctx, loc, reg_save_base,
                                              gcc_jit_context_new_cast(ctx, loc, gp_off_val, long_type)), loc);
                                      gcc_jit_rvalue *fp_src_addr = gcc_jit_lvalue_get_address(
                                          gcc_jit_context_new_array_access(
                                              ctx, loc, reg_save_base,
                                              gcc_jit_context_new_cast(ctx, loc, fp_off_val, long_type)), loc);

                                      int gp_eb_idx = (sysv_classes[0] == GCC_GEN_BE_ABI_CLASS_INTEGER) ? 0 : 1;
                                      int fp_eb_idx = 1 - gp_eb_idx;

                                      gcc_jit_lvalue *dst_gp = gcc_jit_context_new_array_access(
                                          ctx, loc, res_u64_ptr,
                                          gcc_jit_context_new_rvalue_from_int(ctx, int_type, gp_eb_idx));
                                      gcc_jit_lvalue *dst_fp = gcc_jit_context_new_array_access(
                                          ctx, loc, res_u64_ptr,
                                          gcc_jit_context_new_rvalue_from_int(ctx, int_type, fp_eb_idx));

                                      gcc_jit_block_add_assignment(
                                          reg_blk, loc, dst_gp,
                                          gcc_jit_lvalue_as_rvalue(
                                              gcc_jit_rvalue_dereference(
                                                  gcc_jit_context_new_cast(ctx, loc, gp_src_addr, u64_ptr_t), loc)));
                                      gcc_jit_block_add_assignment(
                                          reg_blk, loc, dst_fp,
                                          gcc_jit_lvalue_as_rvalue(
                                              gcc_jit_rvalue_dereference(
                                                  gcc_jit_context_new_cast(ctx, loc, fp_src_addr, u64_ptr_t), loc)));

                                      gcc_jit_block_add_assignment(
                                          reg_blk, loc, gp_off_lval,
                                          gcc_jit_context_new_binary_op(
                                              ctx, loc, GCC_JIT_BINARY_OP_PLUS, uint_type, gp_off_val,
                                              gcc_jit_context_new_rvalue_from_int(ctx, uint_type, 8)));
                                      gcc_jit_block_add_assignment(
                                          reg_blk, loc, fp_off_lval,
                                          gcc_jit_context_new_binary_op(
                                              ctx, loc, GCC_JIT_BINARY_OP_PLUS, uint_type, fp_off_val,
                                              gcc_jit_context_new_rvalue_from_int(ctx, uint_type, 16)));
                                      gcc_jit_block_end_with_jump(reg_blk, loc, merge_blk);
                                  } else {
                                      gcc_jit_block_end_with_jump(cblock, loc, ovf_blk);
                                  }

                                  /* ovf_blk: load from overflow_arg_area, advance overflow_arg_area += aligned_size */
                                  gcc_jit_rvalue *cur_ovf = gcc_jit_lvalue_as_rvalue(ovf_slot);
                                  if (type_align > 8) {
                                      gcc_jit_rvalue *align_m1 = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)(type_align - 1));
                                      gcc_jit_rvalue *neg_align = gcc_jit_context_new_rvalue_from_long(ctx, long_type, -((long)type_align));
                                      gcc_jit_rvalue *cur_int = p_gcc_jit_context_new_bitcast
                                          ? p_gcc_jit_context_new_bitcast(ctx, loc, cur_ovf, long_type)
                                          : gcc_jit_context_new_cast(ctx, loc, cur_ovf, long_type);
                                      gcc_jit_rvalue *aligned_int = gcc_jit_context_new_binary_op(
                                          ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, long_type,
                                          gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, long_type, cur_int, align_m1),
                                          neg_align);
                                      cur_ovf = p_gcc_jit_context_new_bitcast
                                          ? p_gcc_jit_context_new_bitcast(ctx, loc, aligned_int, char_ptr_type)
                                          : gcc_jit_context_new_cast(ctx, loc, aligned_int, char_ptr_type);
                                  }
                                  gcc_jit_block_add_assignment(
                                      ovf_blk, loc, va_res,
                                      gcc_jit_lvalue_as_rvalue(
                                          gcc_jit_rvalue_dereference(
                                              gcc_jit_context_new_cast(ctx, loc, cur_ovf, type_ptr), loc)));
                                  gcc_jit_rvalue *next_ovf = gcc_jit_lvalue_get_address(
                                      gcc_jit_context_new_array_access(
                                          ctx, loc, cur_ovf,
                                          gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)aligned_size)), loc);
                                  gcc_jit_block_add_assignment(ovf_blk, loc, ovf_slot, next_ovf);
                                  gcc_jit_block_end_with_jump(ovf_blk, loc, merge_blk);

                                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_blk));
                                  *out_rval = gcc_jit_lvalue_as_rvalue(va_res);
                                  return GCC_GEN_BE_SUCCESS;
                              }
                              
                              /* Darwin ARM64 / Windows x64 / RISC-V LP64D / i386 scalar pointer va_list */
                              gcc_gen_be_target_abi_t target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_target_abi(&target_abi));

                              gcc_jit_rvalue *ap_val = gcc_jit_lvalue_as_rvalue(ap_lval);
                              gcc_jit_rvalue *ap_char_ptr = gcc_jit_context_new_cast(ctx, loc, ap_val, char_ptr_type);
                              
                              /* size of type aligned to pointer size */
                              size_t type_size = expr->type ? (size_t)f_size_of_type(expr->type) : 0;
                              if (type_size == 0 && gcc_jit_type_is_pointer(type) == NULL && gcc_jit_type_is_struct(type) == NULL) {
                                  type_size = (type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT) ||
                                               type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT)) ? 4 : 8;
                              }
                              if (type_size == 0) type_size = 8;
                              size_t ptr_size = (target_abi == GCC_GEN_BE_TARGET_ABI_SYSV_I386) ? 4 : 8;
                              size_t align = (target_abi == GCC_GEN_BE_TARGET_ABI_SYSV_I386) ? 4 : ptr_size;

                              a_boolean is_indirect_agg = FALSE;
                              if ((target_abi == GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64 ||
                                   target_abi == GCC_GEN_BE_TARGET_ABI_AAPCS64) && type_size > 16) {
                                  a_boolean is_hfa = FALSE;
                                  GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(expr->type, &is_hfa));
                                  if (!is_hfa) {
                                      GCC_GEN_BE_CHECK(check_is_record_type(expr->type, &is_indirect_agg));
                                  }
                              } else if ((target_abi == GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D && type_size > 16) ||
                                         (target_abi == GCC_GEN_BE_TARGET_ABI_WIN64 &&
                                          type_size != 1 && type_size != 2 && type_size != 4 && type_size != 8)) {
                                  GCC_GEN_BE_CHECK(check_is_record_type(expr->type, &is_indirect_agg));
                              }

                              size_t slot_size = is_indirect_agg ? ptr_size : type_size;
                              size_t aligned_size = (slot_size + align - 1) & ~(align - 1);
                              
                              gcc_jit_rvalue *offset = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)aligned_size);
                              
                              /* Compute next ap: next_ap = ap_char_ptr + offset */
                              gcc_jit_lvalue *next_ap_lval = gcc_jit_context_new_array_access(ctx, loc, ap_char_ptr, offset);
                              gcc_jit_rvalue *next_ap = gcc_jit_lvalue_get_address(next_ap_lval, NULL);
                              gcc_jit_rvalue *next_ap_casted = gcc_jit_context_new_cast(ctx, loc, next_ap, gcc_jit_rvalue_get_type(ap_val));
                              
                              /* Cast original ap_val to type* (or type** if indirect aggregate) and dereference before updating ap_lval */
                              gcc_jit_type *type_ptr = gcc_jit_type_get_pointer(type);
                              gcc_jit_rvalue *deref_val = NULL;
                              if (is_indirect_agg) {
                                  gcc_jit_type *type_ptr_ptr = gcc_jit_type_get_pointer(type_ptr);
                                  gcc_jit_rvalue *ind_ptr = gcc_jit_lvalue_as_rvalue(
                                      gcc_jit_rvalue_dereference(
                                          gcc_jit_context_new_cast(ctx, loc, ap_val, type_ptr_ptr), loc));
                                  deref_val = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(ind_ptr, loc));
                              } else {
                                  gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, loc, ap_val, type_ptr);
                                  deref_val = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(typed_ptr, loc));
                              }
                              if (cblock) {
                                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                                  gcc_jit_lvalue *va_res = gcc_jit_function_new_local(func, loc, type, "va_arg_scalar_res");
                                  gcc_jit_block_add_assignment(cblock, loc, va_res, deref_val);
                                  gcc_jit_block_add_assignment(cblock, loc, ap_lval, next_ap_casted);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(va_res);
                              } else {
                                  *out_rval = deref_val;
                              }
                              
                              return GCC_GEN_BE_SUCCESS;
                          }
                          if ((op == eok_va_start || op == eok_va_start_single_operand) && op1) {
                              gcc_jit_lvalue *ap_lval = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &ap_lval));
                              if (ap_lval) {
                                  gcc_jit_block *cblock = NULL;
                                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                                  gcc_jit_function *next_arg_fn = p_gcc_jit_context_get_builtin_function
                                      ? p_gcc_jit_context_get_builtin_function(ctx, "__builtin_next_arg")
                                      : NULL;
                                  if (cblock && next_arg_fn) {
                                      gcc_jit_rvalue *last_arg = NULL;
                                      if (op2) {
                                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op2, &last_arg));
                                      }
                                      gcc_jit_rvalue *va_ptr = gcc_jit_context_new_call(
                                          ctx, loc, next_arg_fn, last_arg ? 1 : 0, last_arg ? &last_arg : NULL);
                                      gcc_jit_type *ap_t = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(ap_lval));
                                      if (gcc_jit_type_is_pointer(ap_t) != NULL) {
                                          gcc_jit_rvalue *cast_va = (gcc_jit_rvalue_get_type(va_ptr) == ap_t)
                                              ? va_ptr
                                              : gcc_jit_context_new_cast(ctx, loc, va_ptr, ap_t);
                                          gcc_jit_block_add_assignment(cblock, loc, ap_lval, cast_va);
                                      }
                                  }
                              }
                          }
                          if (op == eok_va_copy && op1 && op2) {
                              gcc_jit_lvalue *dst_lval = NULL;
                              gcc_jit_rvalue *src_rval = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(op1, &dst_lval));
                              if (dst_lval) {
                                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op2, &src_rval));
                                  if (src_rval) {
                                      gcc_jit_block *cblock = NULL;
                                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                                      if (cblock) {
                                          gcc_jit_type *dst_t = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(dst_lval));
                                          gcc_jit_type *src_t = gcc_jit_rvalue_get_type(src_rval);
                                          if (dst_t == src_t) {
                                              gcc_jit_block_add_assignment(cblock, loc, dst_lval, src_rval);
                                          }
                                      }
                                  }
                              }
                          }
                          if (type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                              *out_rval = NULL;
                              return GCC_GEN_BE_SUCCESS;
                          }
                          return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
                      }
                      
                  case eok_await:
                  case eok_yield:
                      return gcc_gen_be_lower_await_yield(expr, out_rval);
          case eok_bool_cast:
              {
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  a_type_ptr src_tp = NULL;
                  GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &src_tp));
                  if (src_tp && src_tp->kind == tk_nullptr) {
                      *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, bool_type, 0);
                  } else if (src_tp && src_tp->kind == tk_ptr_to_member && r1) {
                      a_type_ptr mtp = NULL;
                      GCC_GEN_BE_CHECK(strip_expr_typerefs(src_tp->variant.ptr_to_member.type, &mtp));
                      if (mtp && mtp->kind == tk_routine && gcc_jit_type_is_struct(gcc_jit_rvalue_get_type(r1))) {
                          gcc_jit_struct *pm_st = gcc_jit_type_is_struct(gcc_jit_rvalue_get_type(r1));
                          gcc_jit_rvalue *fp = gcc_jit_rvalue_access_field(r1, loc, gcc_jit_struct_get_field(pm_st, 0));
                          gcc_jit_rvalue *adj = gcc_jit_rvalue_access_field(r1, loc, gcc_jit_struct_get_field(pm_st, 1));
                          gcc_jit_type *long_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                          gcc_jit_rvalue *fp_nz = gcc_jit_context_new_cast(
                              ctx, loc,
                              gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, fp, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(fp))),
                              bool_type);
                          gcc_jit_rvalue *adj_bit0 = gcc_jit_context_new_binary_op(
                              ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, long_t, adj,
                              gcc_jit_context_new_rvalue_from_long(ctx, long_t, 1L));
                          gcc_jit_rvalue *adj_nz = gcc_jit_context_new_cast(
                              ctx, loc,
                              gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, adj_bit0, gcc_jit_context_new_rvalue_from_long(ctx, long_t, 0L)),
                              bool_type);
                          *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LOGICAL_OR, bool_type, fp_nz, adj_nz);
                      } else {
                          gcc_jit_type *pdm_t = gcc_jit_rvalue_get_type(r1);
                          gcc_jit_rvalue *neg_one = gcc_jit_context_new_rvalue_from_long(ctx, pdm_t, -1L);
                          *out_rval = gcc_jit_context_new_cast(
                              ctx, loc,
                              gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, r1, neg_one),
                              bool_type);
                      }
                  } else if (r1 && gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(r1)) != NULL) {
                      *out_rval = gcc_jit_context_new_comparison(
                          ctx, loc, GCC_JIT_COMPARISON_NE, r1, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(r1)));
                  } else {
                      *out_rval = gcc_jit_context_new_cast(ctx, loc, r1, bool_type);
                  }
                  return GCC_GEN_BE_SUCCESS;
              }

          case eok_pm_base_class_cast:
          case eok_pm_derived_class_cast:
              {
                  /* Preserve null (-1L for PDM, {NULL, 0} for PMF) while applying direct base subobject offset */
                  a_type_ptr dst_tp = NULL;
                  a_type_ptr src_tp = NULL;
                  GCC_GEN_BE_CHECK(strip_expr_typerefs(expr->type, &dst_tp));
                  GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &src_tp));
                  if (!r1 || !dst_tp || dst_tp->kind != tk_ptr_to_member || !src_tp || src_tp->kind != tk_ptr_to_member) {
                      return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
                  }
                  a_type_ptr src_cls = NULL;
                  a_type_ptr dst_cls = NULL;
                  GCC_GEN_BE_CHECK(strip_expr_typerefs(src_tp->variant.ptr_to_member.class_of_which_a_member, &src_cls));
                  GCC_GEN_BE_CHECK(strip_expr_typerefs(dst_tp->variant.ptr_to_member.class_of_which_a_member, &dst_cls));

                  long delta = 0L;
                  a_type_ptr derived_cls = (op == eok_pm_derived_class_cast) ? dst_cls : src_cls;
                  a_type_ptr base_cls = (op == eok_pm_derived_class_cast) ? src_cls : dst_cls;
                  if (derived_cls && (derived_cls->kind == tk_class || derived_cls->kind == tk_struct) &&
                      derived_cls->variant.class_struct_union.extra_info) {
                      for (a_base_class_ptr bc = derived_cls->variant.class_struct_union.extra_info->base_classes;
                           bc != NULL; bc = bc->next) {
                          if (bc->type == base_cls) {
                              delta = (op == eok_pm_derived_class_cast) ? (long)bc->offset : -((long)bc->offset);
                              break;
                          }
                      }
                  }
                  if (delta == 0L) {
                      *out_rval = r1;
                      return GCC_GEN_BE_SUCCESS;
                  }
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (!cblock) {
                      *out_rval = r1;
                      return GCC_GEN_BE_SUCCESS;
                  }
                  gcc_jit_function *parent_fn = gcc_jit_block_get_function(cblock);
                  a_type_ptr mtp = NULL;
                  GCC_GEN_BE_CHECK(strip_expr_typerefs(dst_tp->variant.ptr_to_member.type, &mtp));
                  if (mtp && mtp->kind == tk_routine && gcc_jit_type_is_struct(type)) {
                      gcc_jit_struct *pm_st = gcc_jit_type_is_struct(type);
                      gcc_jit_field *f_ptr = gcc_jit_struct_get_field(pm_st, 0);
                      gcc_jit_field *f_adj = gcc_jit_struct_get_field(pm_st, 1);
                      gcc_jit_lvalue *res_pm = gcc_jit_function_new_local(parent_fn, loc, type, "pmf_cast_res");
                      gcc_jit_block_add_assignment(cblock, loc, res_pm, r1);
                      gcc_jit_rvalue *fp = gcc_jit_lvalue_as_rvalue(gcc_jit_lvalue_access_field(res_pm, loc, f_ptr));
                      gcc_jit_rvalue *adj = gcc_jit_lvalue_as_rvalue(gcc_jit_lvalue_access_field(res_pm, loc, f_adj));
                      gcc_jit_type *long_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                      gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                      gcc_jit_rvalue *fp_nz = gcc_jit_context_new_cast(
                          ctx, loc,
                          gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, fp, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(fp))),
                          bool_t);
                      gcc_jit_rvalue *adj_b0 = gcc_jit_context_new_binary_op(
                          ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, long_t, adj, gcc_jit_context_new_rvalue_from_long(ctx, long_t, 1L));
                      gcc_jit_rvalue *adj_nz = gcc_jit_context_new_cast(
                          ctx, loc,
                          gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, adj_b0, gcc_jit_context_new_rvalue_from_long(ctx, long_t, 0L)),
                          bool_t);
                      gcc_jit_rvalue *non_null = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LOGICAL_OR, bool_t, fp_nz, adj_nz);
                      gcc_gen_be_target_abi_t pmf_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_target_abi(&pmf_abi));
                      long scaled_delta = (pmf_abi == GCC_GEN_BE_TARGET_ABI_AAPCS64) ? (delta << 1) : delta;
                      gcc_jit_block *adj_blk = gcc_jit_function_new_block(parent_fn, "pmf_cast_adj");
                      gcc_jit_block *done_blk = gcc_jit_function_new_block(parent_fn, "pmf_cast_done");
                      gcc_jit_block_end_with_conditional(cblock, loc, non_null, adj_blk, done_blk);
                      gcc_jit_block_add_assignment_op(
                          adj_blk, loc,
                          gcc_jit_lvalue_access_field(res_pm, loc, f_adj),
                          GCC_JIT_BINARY_OP_PLUS,
                          gcc_jit_context_new_rvalue_from_long(ctx, long_t, scaled_delta));
                      gcc_jit_block_end_with_jump(adj_blk, loc, done_blk);
                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(done_blk));
                      *out_rval = gcc_jit_lvalue_as_rvalue(res_pm);
                  } else {
                      gcc_jit_lvalue *res_pdm = gcc_jit_function_new_local(parent_fn, loc, type, "pdm_cast_res");
                      gcc_jit_block_add_assignment(cblock, loc, res_pdm, gcc_jit_context_new_cast(ctx, loc, r1, type));
                      gcc_jit_rvalue *neg_one = gcc_jit_context_new_rvalue_from_long(ctx, type, -1L);
                      gcc_jit_rvalue *non_null = gcc_jit_context_new_comparison(
                          ctx, loc, GCC_JIT_COMPARISON_NE, gcc_jit_lvalue_as_rvalue(res_pdm), neg_one);
                      gcc_jit_block *adj_blk = gcc_jit_function_new_block(parent_fn, "pdm_cast_adj");
                      gcc_jit_block *done_blk = gcc_jit_function_new_block(parent_fn, "pdm_cast_done");
                      gcc_jit_block_end_with_conditional(cblock, loc, non_null, adj_blk, done_blk);
                      gcc_jit_block_add_assignment_op(
                          adj_blk, loc, res_pdm, GCC_JIT_BINARY_OP_PLUS,
                          gcc_jit_context_new_rvalue_from_long(ctx, type, delta));
                      gcc_jit_block_end_with_jump(adj_blk, loc, done_blk);
                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(done_blk));
                      *out_rval = gcc_jit_lvalue_as_rvalue(res_pdm);
                  }
                  return GCC_GEN_BE_SUCCESS;
              }

          case eok_array_to_pointer:
              {
                  if (op1 && op1->kind == enk_constant &&
                      op1->variant.constant.ptr->kind == ck_string) {
                      return gcc_gen_be_lower_constant_rvalue(op1->variant.constant.ptr, type, out_rval);
                  }
                  gcc_jit_lvalue *l1 = NULL;
                  err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  if (l1) {
                      a_type_ptr op1_tp = NULL;
                      GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &op1_tp));
                              gcc_jit_rvalue *l1_rv = gcc_jit_lvalue_as_rvalue(l1);
                              gcc_jit_rvalue *decayed = (op1_tp && op1_tp->kind == tk_array &&
                                  op1_tp->variant.array.is_variable_size_array &&
                                  gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(l1_rv)) != NULL)
                                  ? l1_rv
                                  : gcc_jit_lvalue_get_address(l1, NULL);
                              *out_rval = gcc_jit_context_new_cast(ctx, loc, decayed, type);
                          } else {
                              *out_rval = gcc_jit_context_null(ctx, type);
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }
                      
                  case eok_parens:
                      *out_rval = r1;
                      return GCC_GEN_BE_SUCCESS;

                  case eok_lvalue_cast:
                  case eok_ref_cast:
                  case eok_lvalue_adjust:
                  case eok_class_rvalue_adjust:
                  case eok_cast:
                      {
                          a_boolean dest_is_complex = FALSE;
                          a_boolean src_is_complex = FALSE;
                          GCC_GEN_BE_CHECK(check_is_complex_type(expr->type, &dest_is_complex));
                          if (op1) GCC_GEN_BE_CHECK(check_is_complex_type(op1->type, &src_is_complex));
                          
                          if (dest_is_complex || src_is_complex) {
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              
                              if (dest_is_complex && !src_is_complex) {
                                  /* Real to Complex */
                                  gcc_jit_type *elem_type = NULL;
                                  GCC_GEN_BE_CHECK(get_complex_elem_jit_type(ctx, expr->type, &elem_type));
                                  gcc_jit_rvalue *real_part = gcc_jit_context_new_cast(ctx, loc, r1, elem_type);
                                  gcc_jit_rvalue *imag_part = gcc_jit_context_new_cast(
                                      ctx, loc,
                                      gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0),
                                      elem_type);
                                  return construct_complex_from_parts(ctx, loc, cblock, type, elem_type, real_part, imag_part, out_rval);
                              } else if (!dest_is_complex) {
                                  /* Complex to Real */
                                  gcc_jit_type *src_elem_type = NULL;
                                  GCC_GEN_BE_CHECK(get_complex_elem_jit_type(ctx, op1->type, &src_elem_type));
                                  gcc_jit_rvalue *r = NULL, *i = NULL;
                                  GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, cblock, r1, gcc_jit_rvalue_get_type(r1), src_elem_type, &r, &i));
                                  *out_rval = gcc_jit_context_new_cast(ctx, loc, r, type);
                                  return GCC_GEN_BE_SUCCESS;
                              } else {
                                  /* Complex to Complex */
                                  gcc_jit_type *src_elem_type = NULL;
                                  GCC_GEN_BE_CHECK(get_complex_elem_jit_type(ctx, op1->type, &src_elem_type));
                                  gcc_jit_type *dest_elem_type = NULL;
                                  GCC_GEN_BE_CHECK(get_complex_elem_jit_type(ctx, expr->type, &dest_elem_type));
                                  gcc_jit_rvalue *r = NULL, *i = NULL;
                                  GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, cblock, r1, gcc_jit_rvalue_get_type(r1), src_elem_type, &r, &i));
                                  gcc_jit_rvalue *new_r = gcc_jit_context_new_cast(ctx, loc, r, dest_elem_type);
                                  gcc_jit_rvalue *new_i = gcc_jit_context_new_cast(ctx, loc, i, dest_elem_type);
                                  return construct_complex_from_parts(ctx, loc, cblock, type, dest_elem_type, new_r, new_i, out_rval);
                              }
                          }
                          if (type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                              GCC_GEN_BE_CHECK(emit_optional_eval(loc, r1));
                              *out_rval = NULL;
                          } else if (r1) {
                              a_type_ptr s_tp = NULL;
                              GCC_GEN_BE_CHECK(strip_expr_typerefs(op1->type, &s_tp));
                              if (s_tp && s_tp->kind == tk_nullptr) {
                                  GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval));
                              } else if (gcc_jit_rvalue_get_type(r1) == type) {
                                  GCC_GEN_BE_CHECK(gcc_gen_be_normalize_bitint_rvalue(expr->type, r1, out_rval));
                              } else {
                                  GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, r1, type, out_rval));
                                  GCC_GEN_BE_CHECK(gcc_gen_be_normalize_bitint_rvalue(expr->type, *out_rval, out_rval));
                              }
                          } else {
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval));
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }
                      
                  default:
                      *out_rval = NULL;
                      return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_UNHANDLED_EXPR, "unhandled expression operation kind");
              }
          }


      case enk_braced_init_list:
          {
              *out_rval = NULL;
              
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              
              gcc_jit_type *expected_type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &expected_type));
              
              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              gcc_jit_lvalue *temp_lval = gcc_jit_function_new_local(func, NULL, expected_type, "braced_init_temp");
              
              an_expr_node_ptr elem = expr->variant.braced_init_list;
              
              a_type_ptr orig_type = NULL;
              GCC_GEN_BE_CHECK(strip_expr_typerefs(expr->type, &orig_type));
              a_boolean braced_is_rec = FALSE;
              GCC_GEN_BE_CHECK(check_is_record_type(orig_type, &braced_is_rec));
              
              if (orig_type && orig_type->kind == tk_array) {
                  int idx = 0;
                  gcc_jit_type *idx_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                  while (elem) {
                      gcc_jit_rvalue *elem_rval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(elem, &elem_rval));
                      gcc_jit_rvalue *idx_rval = gcc_jit_context_new_rvalue_from_int(ctx, idx_type, idx);
                      gcc_jit_lvalue *arr_elem = gcc_jit_context_new_array_access(ctx, loc, gcc_jit_lvalue_as_rvalue(temp_lval), idx_rval);
                      gcc_jit_block_add_assignment(cblock, loc, arr_elem, elem_rval);
                      idx++;
                      elem = elem->next;
                  }
              } else if (braced_is_rec) {
                  a_field_ptr fld = orig_type->variant.class_struct_union.field_list;
                  
                  while (elem && fld) {
                      gcc_jit_rvalue *elem_rval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(elem, &elem_rval));
                      
                      void *cached_field = NULL;
                      GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, fld, &cached_field));
                      if (cached_field) {
                          gcc_jit_lvalue *field_lval = gcc_jit_lvalue_access_field(temp_lval, NULL, (gcc_jit_field *)cached_field);
                          gcc_jit_block_add_assignment(cblock, loc, field_lval, elem_rval);
                      }
                      
                      elem = elem->next;
                      fld = fld->next;
                  }
              }
              
              *out_rval = gcc_jit_lvalue_as_rvalue(temp_lval);
              return GCC_GEN_BE_SUCCESS;
          }


          
      case enk_typeid:
          return gcc_gen_be_lower_typeid(expr, out_rval);

      case enk_routine:
          {
              gcc_jit_function *f = NULL;
              GCC_GEN_BE_CHECK(resolve_cached_or_decl_func(expr->variant.routine.ptr, &f));
              if (f) {
                  void *abi_fn = NULL;
                  GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, f, &abi_fn));
                  if (abi_fn) f = (gcc_jit_function *)abi_fn;
                  *out_rval = gcc_jit_function_get_address(f, NULL);
              }
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_condition:
          {
              a_condition_supplement_ptr cond = expr->variant.condition;
              
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              
              if (cond->initialization) {
                  extern gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(cond->initialization, func));
              }
              
              if (cond->dynamic_init && cond->dynamic_init->kind != dik_none) {
                  a_dynamic_init_ptr dip = cond->dynamic_init;
                  if (dip->variable) {
                      void *cached = NULL;
                      GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, dip->variable, &cached));
                      gcc_jit_lvalue *var_lval = (gcc_jit_lvalue *)cached;
                      if (!var_lval) {
                          extern gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(gcc_jit_function *func, a_variable_ptr var, gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, dip->variable, &var_lval));
                      }
                      
                      gcc_jit_rvalue *init_rval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(dip, var_lval, &init_rval));
                  }
              }
              
              if (cond->expr) {
                  return gcc_gen_be_lower_expr_rvalue(cond->expr, out_rval);
              } else {
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, bool_type, 1);
                  return GCC_GEN_BE_SUCCESS;
              }
          }

      case enk_new_delete:
          {
              a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
              if (!ndsp || !ndsp->type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
              gcc_jit_type *type = NULL;
              err = gcc_gen_be_lower_type(ndsp->type, &type);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
              
              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              gcc_jit_type *size_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
              gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
              gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
              gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
              
              if (ndsp->is_new) {
                  gcc_jit_rvalue *alloc_size = NULL;
                  gcc_jit_rvalue *num_elems_cast = NULL;
                  a_boolean needs_cookie = FALSE;
                  size_t cookie_size = sizeof(size_t);

                  size_t elem_bytes = (ndsp->type->size > 0) ? (size_t)ndsp->type->size : 1U;
                  gcc_jit_rvalue *elem_size_cast = gcc_jit_context_new_rvalue_from_long(ctx, size_type, (long)elem_bytes);
                  if (ndsp->number_of_elements) {
                      gcc_jit_rvalue *num_elems = NULL;
                      err = gcc_gen_be_lower_expr_rvalue(ndsp->number_of_elements, &num_elems);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      num_elems_cast = gcc_jit_context_new_cast(ctx, loc, num_elems, size_type);
                      alloc_size = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, size_type, num_elems_cast, elem_size_cast);
                      
                      /* Add Itanium C++ ABI array cookie if element type has a destructor */
                      if (ndsp->dynamic_init && ndsp->dynamic_init->destructor) {
                          needs_cookie = TRUE;
                          gcc_jit_rvalue *cookie_rval = gcc_jit_context_new_rvalue_from_long(ctx, size_type, (long)cookie_size);
                          alloc_size = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_PLUS, size_type, alloc_size, cookie_rval);
                      }
                  } else {
                      alloc_size = elem_size_cast;
                  }
                  
                  gcc_jit_rvalue *alloc_call = NULL;
                  if (ndsp->routine) {
                      gcc_jit_function *alloc_func = NULL;
                      GCC_GEN_BE_CHECK(resolve_cached_or_decl_func(ndsp->routine, &alloc_func));
                      if (alloc_func) {
                          int num_args = 1;
                          an_expr_node_ptr arg = ndsp->arg;
                          while (arg) { num_args++; arg = arg->next; }

                          gcc_jit_rvalue **args = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)num_args * sizeof(gcc_jit_rvalue *), (void **)&args));
                          args[0] = alloc_size;

                          arg = ndsp->arg;
                          int i = 1;
                          int param_count = 0;
                          gcc_jit_block *active_alloc_lp = NULL;
                          while (arg) {
                              GCC_GEN_BE_CHECK_CLEANUP(
                                  gcc_gen_be_lower_expr_rvalue(arg, &args[i]), err, new_alloc_fail);
                              i++;
                              arg = arg->next;
                          }
                          param_count = (int)gcc_jit_function_get_param_count(alloc_func);
                          for (int p = 0; p < num_args && p < param_count; ++p) {
                              gcc_jit_param *param = gcc_jit_function_get_param(alloc_func, p);
                              gcc_jit_type *ptype = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(param));
                              gcc_jit_type *atype = gcc_jit_rvalue_get_type(args[p]);
                              if (atype != ptype) {
                                  args[p] = gcc_jit_context_new_cast(ctx, loc, args[p], ptype);
                              }
                          }

                          GCC_GEN_BE_CHECK_CLEANUP(
                              gcc_gen_be_get_active_landing_pad(&active_alloc_lp), err, new_alloc_fail);
                          GCC_GEN_BE_CHECK_CLEANUP(
                              gcc_gen_be_get_current_block(&cblock), err, new_alloc_fail);
                          if (active_alloc_lp && cblock &&
                              gcc_jit_block_get_function(active_alloc_lp) == gcc_jit_block_get_function(cblock)) {
                              gcc_jit_lvalue *alloc_res_lval = gcc_jit_function_new_local(
                                  func, loc, void_ptr_type, "invoke_new_alloc");
                              gcc_jit_block *next_blk = NULL;
                              GCC_GEN_BE_CHECK_CLEANUP(
                                  gcc_gen_be_emit_invoke_call(
                                      ctx, cblock, alloc_func, num_args, args, alloc_res_lval, NULL, active_alloc_lp, &next_blk),
                                  err, new_alloc_fail);
                              cblock = next_blk;
                              alloc_call = gcc_jit_lvalue_as_rvalue(alloc_res_lval);
                          } else {
                              alloc_call = gcc_jit_context_new_call(ctx, loc, alloc_func, num_args, args);
                          }
                          free(args);
                          goto new_alloc_ok;
                      new_alloc_fail:;
                          free(args);
                          return err;
                      new_alloc_ok:
                          ;
                      }
                  } else {
                      /* Fallback to system malloc if routine is missing and not lowered */
                      gcc_jit_param *malloc_param = gcc_jit_context_new_param(ctx, NULL, size_type, "size");
                      gcc_jit_function *malloc_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "malloc", 1, &malloc_param, 0);
                      alloc_call = gcc_jit_context_new_call(ctx, loc, malloc_func, 1, &alloc_size);
                  }

                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_type *ptr_type = gcc_jit_type_get_pointer(type);
                  gcc_jit_lvalue *allocated_ptr = gcc_jit_function_new_local(func, NULL, ptr_type, "new_ptr");
                  if (alloc_call) {
                      if (needs_cookie) {
                          gcc_jit_lvalue *raw_ptr = gcc_jit_function_new_local(func, NULL, void_ptr_type, "raw_new_ptr");
                          gcc_jit_block_add_assignment(cblock, loc, raw_ptr, alloc_call);
                          gcc_jit_type *size_ptr_type = gcc_jit_type_get_pointer(size_type);
                          gcc_jit_rvalue *cookie_ptr = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_as_rvalue(raw_ptr), size_ptr_type);
                          gcc_jit_block_add_assignment(cblock, loc, gcc_jit_rvalue_dereference(cookie_ptr, NULL), num_elems_cast);
                          gcc_jit_rvalue *raw_char = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_as_rvalue(raw_ptr), char_ptr_type);
                          gcc_jit_rvalue *cookie_off = gcc_jit_context_new_rvalue_from_long(ctx, size_type, (long)cookie_size);
                          gcc_jit_rvalue *data_addr = gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, loc, raw_char, cookie_off), NULL);
                          gcc_jit_block_add_assignment(cblock, loc, allocated_ptr, gcc_jit_context_new_cast(ctx, loc, data_addr, ptr_type));
                      } else {
                          gcc_jit_block_add_assignment(cblock, loc, allocated_ptr, gcc_jit_context_new_cast(ctx, loc, alloc_call, ptr_type));
                      }
                  } else {
                      gcc_jit_block_add_assignment(cblock, loc, allocated_ptr, gcc_jit_context_null(ctx, ptr_type));
                  }

                  if (ndsp->dynamic_init) {
                      /* Dereference the pointer to get the lvalue for the new object */
                      gcc_jit_lvalue *obj_lval = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(allocated_ptr), NULL);
                      gcc_jit_block *new_cleanup_lp = NULL;
                      gcc_jit_function *free_fn = NULL;
                      if (ndsp->freeing_of_storage_on_exception && ndsp->freeing_of_storage_on_exception->destructor) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(ndsp->freeing_of_storage_on_exception->destructor, &free_fn));
                          if (free_fn) {
                              new_cleanup_lp = gcc_jit_function_new_block(func, "new_eh_free_storage_lp");
                              GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(new_cleanup_lp));
                          }
                      }
                      gcc_jit_rvalue *init_rval = NULL;
                      err = gcc_gen_be_lower_dynamic_init(ndsp->dynamic_init, obj_lval, &init_rval);
                      if (err != GCC_GEN_BE_SUCCESS) {
                          if (new_cleanup_lp) {
                              --gcc_gen_be_singleton_state.landing_pad_stack_size;
                          }
                          return err;
                      }
                      if (new_cleanup_lp) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
                          gcc_jit_block *cont_after_new = gcc_jit_function_new_block(func, "after_new_init");
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          if (cblock) {
                              gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                              gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                              gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_EQ, zero, zero);
                              gcc_jit_block_end_with_conditional(cblock, loc, cond, cont_after_new, new_cleanup_lp);
                          }
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(new_cleanup_lp));
                          gcc_jit_rvalue *void_p = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_as_rvalue(allocated_ptr), void_ptr_type);
                          int free_pcount = (int)gcc_jit_function_get_param_count(free_fn);
                          if (free_pcount >= 2) {
                              gcc_jit_rvalue *f_args[2];
                              gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(free_fn, 0)));
                              gcc_jit_type *p1_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(free_fn, 1)));
                              f_args[0] = gcc_jit_context_new_cast(ctx, loc, void_p, p0_t);
                              f_args[1] = gcc_jit_context_new_cast(ctx, loc, alloc_size, p1_t);
                              gcc_jit_block_add_eval(new_cleanup_lp, loc, gcc_jit_context_new_call(ctx, loc, free_fn, 2, f_args));
                          } else {
                              gcc_jit_block_add_eval(new_cleanup_lp, loc, gcc_jit_context_new_call(ctx, loc, free_fn, 1, &void_p));
                          }
                          gcc_jit_block *after_resume = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(gcc_jit_context_null(ctx, void_ptr_type), &after_resume));
                          if (after_resume) {
                              gcc_jit_block *outer_lp = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&outer_lp));
                              if (outer_lp) {
                                  gcc_jit_block_end_with_jump(after_resume, loc, outer_lp);
                              } else {
                                  gcc_jit_block_end_with_jump(after_resume, loc, cont_after_new);
                              }
                          }
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cont_after_new));
                      }
                  }
                  
                  *out_rval = gcc_jit_lvalue_as_rvalue(allocated_ptr);
              } else {
                  /* Delete */
                  gcc_jit_rvalue *ptr = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(ndsp->arg, &ptr));
                  
                  gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
                  gcc_jit_rvalue *ptr_as_void = gcc_jit_context_new_cast(ctx, loc, ptr, void_ptr_type);
                  gcc_jit_rvalue *is_not_null = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, ptr_as_void, null_ptr);
                  
                  gcc_jit_block *delete_block = gcc_jit_function_new_block(func, "delete_not_null");
                  gcc_jit_block *end_block = gcc_jit_function_new_block(func, "delete_end");
                  
                  gcc_jit_block_end_with_conditional(cblock, loc, is_not_null, delete_block, end_block);
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(delete_block));
                  
                  gcc_jit_rvalue *dealloc_ptr = ptr;
                  gcc_jit_rvalue *dealloc_total_size = NULL;
                  if (ndsp->dynamic_init) {
                      /* Invoke destructor or dynamic destruction before deallocating storage */
                      gcc_jit_type *ptr_type = gcc_jit_type_get_pointer(type);
                      gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, loc, ptr, ptr_type);
                      gcc_jit_lvalue *obj_lval = gcc_jit_rvalue_dereference(typed_ptr, NULL);
                      if (ndsp->dynamic_init->destructor) {
                          gcc_jit_function *dtor_fn = NULL;
                          GCC_GEN_BE_CHECK(resolve_cached_or_decl_func(ndsp->dynamic_init->destructor, &dtor_fn));
                          if (dtor_fn) {
                              int pcount = (int)gcc_jit_function_get_param_count(dtor_fn);
                              gcc_jit_type *p0_type = pcount > 0
                                  ? gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(dtor_fn, 0)))
                                  : void_ptr_type;
#ifdef EDG_GCC_BE_ISOLATED_TEST
                              if (ndsp->is_array) {
#else
                              if (ndsp->array_delete) {
#endif
                                  /* Array delete[]: read Itanium ABI cookie at (char*)ptr - sizeof(size_t),
                                     destroy elements in reverse order from count down to 1, and adjust dealloc_ptr */
                                  size_t cookie_sz = sizeof(size_t);
                                  gcc_jit_rvalue *data_char_ptr = gcc_jit_context_new_cast(ctx, loc, ptr, char_ptr_type);
                                  gcc_jit_rvalue *neg_cookie = gcc_jit_context_new_rvalue_from_long(
                                      ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), -((long)cookie_sz));
                                  gcc_jit_rvalue *cookie_raw_addr = gcc_jit_lvalue_get_address(
                                      gcc_jit_context_new_array_access(ctx, loc, data_char_ptr, neg_cookie), loc);
                                  dealloc_ptr = gcc_jit_context_new_cast(ctx, loc, cookie_raw_addr, void_ptr_type);

                                  gcc_jit_type *size_ptr_type = gcc_jit_type_get_pointer(size_type);
                                  gcc_jit_rvalue *elem_count_val = gcc_jit_lvalue_as_rvalue(
                                      gcc_jit_rvalue_dereference(
                                          gcc_jit_context_new_cast(ctx, loc, cookie_raw_addr, size_ptr_type), loc));
                                  size_t del_elem_bytes = (ndsp->type->size > 0) ? (size_t)ndsp->type->size : 1U;
                                  gcc_jit_rvalue *elem_sz_val = gcc_jit_context_new_rvalue_from_long(
                                      ctx, size_type, (long)del_elem_bytes);
                                  gcc_jit_rvalue *cookie_sz_val = gcc_jit_context_new_rvalue_from_long(ctx, size_type, (long)cookie_sz);
                                  dealloc_total_size = gcc_jit_context_new_binary_op(
                                      ctx, loc, GCC_JIT_BINARY_OP_PLUS, size_type,
                                      gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, size_type, elem_count_val, elem_sz_val),
                                      cookie_sz_val);

                                  gcc_jit_lvalue *idx_var = gcc_jit_function_new_local(func, loc, size_type, "del_arr_idx");
                                  gcc_jit_block_add_assignment(delete_block, loc, idx_var, elem_count_val);
                                  gcc_jit_block *loop_cond = gcc_jit_function_new_block(func, "del_arr_cond");
                                  gcc_jit_block *loop_body = gcc_jit_function_new_block(func, "del_arr_body");
                                  gcc_jit_block *loop_after = gcc_jit_function_new_block(func, "del_arr_after");
                                  gcc_jit_block_end_with_jump(delete_block, loc, loop_cond);

                                  gcc_jit_rvalue *zero_sz = gcc_jit_context_new_rvalue_from_long(ctx, size_type, 0);
                                  gcc_jit_rvalue *one_sz = gcc_jit_context_new_rvalue_from_long(ctx, size_type, 1);
                                  gcc_jit_rvalue *has_more = gcc_jit_context_new_comparison(
                                      ctx, loc, GCC_JIT_COMPARISON_GT, gcc_jit_lvalue_as_rvalue(idx_var), zero_sz);
                                  gcc_jit_block_end_with_conditional(loop_cond, loc, has_more, loop_body, loop_after);

                                  gcc_jit_block_add_assignment_op(loop_body, loc, idx_var, GCC_JIT_BINARY_OP_MINUS, one_sz);
                                  gcc_jit_lvalue *elem_slot = gcc_jit_context_new_array_access(
                                      ctx, loc, typed_ptr, gcc_jit_lvalue_as_rvalue(idx_var));
                                  gcc_jit_rvalue *elem_addr = gcc_jit_lvalue_get_address(elem_slot, loc);
                                  gcc_jit_rvalue *dtor_args[2];
                                  dtor_args[0] = gcc_jit_context_new_cast(ctx, loc, elem_addr, p0_type);
                                  if (pcount >= 2) {
                                      gcc_jit_type *p1_type = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(dtor_fn, 1)));
                                      dtor_args[1] = gcc_jit_context_new_rvalue_from_int(ctx, p1_type, 2);
                                      gcc_jit_block_add_eval(loop_body, loc, gcc_jit_context_new_call(ctx, loc, dtor_fn, 2, dtor_args));
                                  } else {
                                      gcc_jit_block_add_eval(loop_body, loc, gcc_jit_context_new_call(ctx, loc, dtor_fn, 1, dtor_args));
                                  }
                                  gcc_jit_block_end_with_jump(loop_body, loc, loop_cond);
                                  delete_block = loop_after;
                                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(delete_block));
                              } else {
                                  gcc_jit_rvalue *dtor_args[2];
                                  dtor_args[0] = gcc_jit_context_new_cast(ctx, loc, ptr, p0_type);
                                  if (pcount >= 2) {
                                      gcc_jit_type *p1_type = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(dtor_fn, 1)));
                                      dtor_args[1] = gcc_jit_context_new_rvalue_from_int(ctx, p1_type, 2);
                                      gcc_jit_block_add_eval(delete_block, loc, gcc_jit_context_new_call(ctx, loc, dtor_fn, 2, dtor_args));
                                  } else {
                                      gcc_jit_block_add_eval(delete_block, loc, gcc_jit_context_new_call(ctx, loc, dtor_fn, 1, dtor_args));
                                  }
                              }
                          }
                      } else {
                          gcc_jit_rvalue *dummy_rval = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(ndsp->dynamic_init, obj_lval, &dummy_rval));
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&delete_block));
                      }
                  }
                  
                  if (ndsp->routine) {
                      gcc_jit_function *dealloc_func = NULL;
                      GCC_GEN_BE_CHECK(resolve_cached_or_decl_func(ndsp->routine, &dealloc_func));
                      if (dealloc_func) {
                          /* Support C++14 sized deallocation and C++17 over-aligned deallocation */
                          int num_args = 0;
                          for (an_expr_node_ptr a = ndsp->arg; a != NULL; a = a->next) {
                              num_args++;
                          }
                          int param_count = (int)gcc_jit_function_get_param_count(dealloc_func);
                          int call_argc = num_args > param_count ? num_args : param_count;
                          gcc_jit_rvalue **dargs = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)call_argc * sizeof(gcc_jit_rvalue *), (void **)&dargs));
                          int idx = 0;
                          dargs[idx++] = dealloc_ptr;
                          for (an_expr_node_ptr a = ndsp->arg->next; a != NULL; a = a->next) {
                              err = gcc_gen_be_lower_expr_rvalue(a, &dargs[idx]);
                              if (err != GCC_GEN_BE_SUCCESS) { free(dargs); return err; }
                              idx++;
                          }
                          /* Fill in implicit size / alignment if dealloc_func expects more arguments (e.g. operator delete(void*, size_t)) */
                          while (idx < param_count) {
                              gcc_jit_param *param = gcc_jit_function_get_param(dealloc_func, idx);
                              gcc_jit_type *ptype = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(param));
                              if (idx == 1) {
                                  size_t del_type_bytes = (ndsp->type->size > 0) ? (size_t)ndsp->type->size : 1U;
                                  gcc_jit_rvalue *sz = dealloc_total_size ? dealloc_total_size : gcc_jit_context_new_rvalue_from_long(ctx, size_type, (long)del_type_bytes);
                                  dargs[idx] = gcc_jit_context_new_cast(ctx, loc, sz, ptype);
                              } else {
                                  long align_val = (ndsp->type->alignment > 0) ? (long)ndsp->type->alignment : 8L;
                                  dargs[idx] = gcc_jit_context_new_rvalue_from_long(ctx, ptype, align_val);
                              }
                              idx++;
                          }
                          for (int p = 0; p < param_count; ++p) {
                              gcc_jit_param *param = gcc_jit_function_get_param(dealloc_func, p);
                              gcc_jit_type *ptype = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(param));
                              gcc_jit_type *atype = gcc_jit_rvalue_get_type(dargs[p]);
                              if (atype != ptype) {
                                  dargs[p] = gcc_jit_context_new_cast(ctx, loc, dargs[p], ptype);
                              }
                          }
                          gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, loc, dealloc_func, param_count > 0 ? param_count : idx, dargs);
                          gcc_jit_block_add_eval(delete_block, NULL, call);
                          free(dargs);
                      }
                  } else {
                      /* Fallback to system free */
                      gcc_jit_param *free_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
                      gcc_jit_function *free_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "free", 1, &free_param, 0);
                      gcc_jit_rvalue *free_arg = gcc_jit_context_new_cast(ctx, loc, dealloc_ptr, void_ptr_type);
                      gcc_jit_block_add_eval(delete_block, NULL, gcc_jit_context_new_call(ctx, loc, free_func, 1, &free_arg));
                  }

                  gcc_jit_block_end_with_jump(delete_block, NULL, end_block);
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(end_block));

                  *out_rval = gcc_jit_context_null(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR));
              }
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_result_of_overriding_function:
          {
              a_routine_ptr wrapper_rout = innermost_function_scope ? innermost_function_scope->variant.routine.ptr : NULL;
              if (!wrapper_rout || !wrapper_rout->overriding_function_for_wrapper) {
                  return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
              }
              a_routine_ptr overriding_rout = wrapper_rout->overriding_function_for_wrapper;

              gcc_jit_function *func = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(overriding_rout, &func));

              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
              gcc_jit_function *current_jit_func = gcc_jit_block_get_function(cblock);

              gcc_jit_type *expected_type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &expected_type));

              int num_args = (int)gcc_jit_function_get_param_count(current_jit_func);
              int target_pcount = (int)gcc_jit_function_get_param_count(func);
              int alloc_argc = num_args > target_pcount ? num_args : target_pcount;
              gcc_jit_rvalue **args = NULL;
              if (alloc_argc > 0) {
                  GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)alloc_argc * sizeof(gcc_jit_rvalue *), (void **)&args));

                  a_scope_ptr def_scope = scope_for_routine_or_null(wrapper_rout);
                  a_variable_ptr var_param = def_scope ? def_scope->variant.routine.parameters : NULL;

                  for (int i = 0; i < num_args; i++) {
                      GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_step_oom_budget(), err, ov_call_fail);
                      if (var_param) {
                          void *cached_lval = NULL;
                          GCC_GEN_BE_CHECK_CLEANUP(
                              cache_lookup(GCC_GEN_BE_CACHE_VAR, var_param, &cached_lval), err, ov_call_fail);
                          if (cached_lval) {
                              args[i] = gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached_lval);
                          } else {
                              gcc_jit_param *param = gcc_jit_function_get_param(current_jit_func, i);
                              args[i] = gcc_jit_param_as_rvalue(param);
                          }
                          var_param = var_param->next;
                      } else {
                          gcc_jit_param *param = gcc_jit_function_get_param(current_jit_func, i);
                          args[i] = gcc_jit_param_as_rvalue(param);
                      }
                  }
                  for (int p = num_args; p < target_pcount; ++p) {
                      gcc_jit_type *pt = gcc_jit_rvalue_get_type(
                          gcc_jit_param_as_rvalue(gcc_jit_function_get_param(func, p)));
                      GCC_GEN_BE_CHECK_CLEANUP(
                          gcc_gen_be_get_zero_rvalue(NULL, pt, &args[p]), err, ov_call_fail);
                  }
              }
              goto ov_call_ok;
          ov_call_fail:;
              free(args);
              return err;
          ov_call_ok:
              for (int p = 0; p < target_pcount; ++p) {
                  gcc_jit_type *pt = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(func, p)));
                  if (gcc_jit_rvalue_get_type(args[p]) != pt) {
                      args[p] = gcc_jit_context_new_cast(ctx, loc, args[p], pt);
                  }
              }
              gcc_jit_rvalue *call_rval = gcc_jit_context_new_call(ctx, loc, func, target_pcount, args);
              free(args);
              
              gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
              if (expected_type == void_type || gcc_jit_function_get_return_type(func) == void_type) {
                  gcc_jit_block_add_eval(cblock, loc, call_rval);
                  *out_rval = NULL;
              } else if (gcc_jit_rvalue_get_type(call_rval) == expected_type) {
                  *out_rval = call_rval;
              } else {
                  *out_rval = gcc_jit_context_new_cast(ctx, loc, call_rval, expected_type);
              }
              return GCC_GEN_BE_SUCCESS;
          }

          
      case enk_await:
          return gcc_gen_be_lower_co_await(expr, out_rval);
      case enk_yield:
          return gcc_gen_be_lower_co_yield(expr, out_rval);

      case enk_object_lifetime:
          {
              return gcc_gen_be_lower_expr_rvalue(expr->variant.object_lifetime.expr, out_rval);
          }

      case enk_reuse_value:
          {
              a_dynamic_init_ptr dip = expr->variant.reused_value_init;
              if (dip && dip->variable) {
                  void *cached = NULL;
                  GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, dip->variable, &cached));
                  if (cached) {
                      *out_rval = gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached);
                      return GCC_GEN_BE_SUCCESS;
                  }
              }
              gcc_jit_lvalue *temp_lval = NULL;
              if (dip && dip->variable) {
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, dip->variable, &temp_lval));
              } else {
                  gcc_jit_type *temp_type = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &temp_type));
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  temp_lval = gcc_jit_function_new_local(func, NULL, temp_type, "reused_rval");
              }
              if (dip && dip->kind != dik_none) {
                  gcc_jit_rvalue *init_rval = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(dip, temp_lval, &init_rval));
                  if (!temp_lval && init_rval) {
                      *out_rval = init_rval;
                      return GCC_GEN_BE_SUCCESS;
                  }
              }
              if (temp_lval) {
                  *out_rval = gcc_jit_lvalue_as_rvalue(temp_lval);
              }
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_sizeof:
      case enk_alignof:
      case enk_datasizeof:
          {
              gcc_jit_type *res_t = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &res_t));
              a_type_ptr raw_stp = expr->variant.sizeof_info.is_type
                  ? expr->variant.sizeof_info.variant.type
                  : (expr->variant.sizeof_info.variant.expr ? expr->variant.sizeof_info.variant.expr->type : NULL);
              a_type_ptr stp = NULL;
              GCC_GEN_BE_CHECK(strip_expr_typerefs(raw_stp, &stp));
              if (expr->kind == enk_alignof) {
                  long al = (stp && stp->alignment > 0) ? (long)stp->alignment : 1L;
                  *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, res_t, al);
                  return GCC_GEN_BE_SUCCESS;
              }
              if (expr->kind == enk_sizeof && !expr->variant.sizeof_info.is_type &&
                  expr->variant.sizeof_info.variant.expr && stp && stp->kind == tk_array &&
                  stp->variant.array.has_assoc_vla_dimension) {
                  /* Evaluate VLA operand expression if it has runtime side effects (e.g. sizeof(*vla_fn())) */
                  an_expr_node_ptr op_e = expr->variant.sizeof_info.variant.expr;
                  if (op_e->kind != enk_variable) {
                      gcc_jit_rvalue *side_rv = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op_e, &side_rv));
                      GCC_GEN_BE_CHECK(emit_optional_eval(loc, side_rv));
                  }
              }
              if (expr->kind == enk_sizeof && stp && stp->kind == tk_array) {
                  gcc_jit_rvalue *total_elems = gcc_jit_context_new_rvalue_from_long(ctx, res_t, 1L);
                  a_type_ptr cur = stp;
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  gcc_jit_function *cur_fn = cblock ? gcc_jit_block_get_function(cblock) : NULL;
                  while (cur && cur->kind == tk_array) {
                      if (cur->variant.array.has_assoc_vla_dimension && innermost_function_scope) {
                          a_vla_dimension_ptr dim = innermost_function_scope->vla_dimensions;
                          while (dim && dim->type != cur) dim = dim->next;
                          while (dim && !dim->dimension_variable && dim->original_dimension) {
                              dim = dim->original_dimension;
                          }
                          if (dim && dim->dimension_variable) {
                              void *cached_d = NULL;
                              GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, dim->dimension_variable, &cached_d));
                              gcc_jit_lvalue *dim_lv = (gcc_jit_lvalue *)cached_d;
                              if (!dim_lv && cur_fn) {
                                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(cur_fn, dim->dimension_variable, &dim_lv));
                              }
                              if (dim_lv) {
                                  gcc_jit_rvalue *dv = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_as_rvalue(dim_lv), res_t);
                                  total_elems = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, res_t, total_elems, dv);
                              }
                          }
                      } else {
                          gcc_jit_rvalue *dv = gcc_jit_context_new_rvalue_from_long(
                              ctx, res_t, (long)cur->variant.array.variant.number_of_elements);
                          total_elems = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, res_t, total_elems, dv);
                      }
                      GCC_GEN_BE_CHECK(strip_expr_typerefs(cur->variant.array.element_type, &cur));
                  }
                  long elem_sz = cur ? (long)cur->size : 1L;
                  gcc_jit_rvalue *esz_rv = gcc_jit_context_new_rvalue_from_long(ctx, res_t, elem_sz);
                  *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, res_t, total_elems, esz_rv);
                  return GCC_GEN_BE_SUCCESS;
              }
              long byte_sz = stp ? (long)stp->size : 0L;
              *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, res_t, byte_sz);
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_type_operand:
          {
              *out_rval = NULL;
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_throw:
          {
              gcc_jit_type *type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &type));

              gcc_jit_block *out_block = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(expr, &out_block));

              return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
          }

      default:
#if VLA_DEALLOCATIONS_IN_IL
          if (expr->kind == enk_vla_dealloc) {
              *out_rval = NULL;
              return GCC_GEN_BE_SUCCESS;
          }
#endif
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
          if (expr->kind == enk_lowered_eh_construct) {
              gcc_jit_type *type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &type));
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (expr->variant.lowered_eh.kind == leck_internal_try && cblock) {
                  an_expr_node_ptr try_e = expr->variant.lowered_eh.variant.try_and_catch_expr;
                  an_expr_node_ptr catch_e = try_e ? try_e->next : NULL;
                  gcc_jit_function *cur_fn = gcc_jit_block_get_function(cblock);
                  gcc_jit_block *catch_lp = gcc_jit_function_new_block(cur_fn, "internal_try_lp");
                  gcc_jit_block *done_blk = gcc_jit_function_new_block(cur_fn, "internal_try_done");

                  GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(catch_lp));
                  gcc_jit_rvalue *try_rv = NULL;
                  gcc_gen_be_error_t terr = try_e
                      ? gcc_gen_be_lower_expr_rvalue(try_e, &try_rv)
                      : GCC_GEN_BE_SUCCESS;
                  if (terr != GCC_GEN_BE_SUCCESS) {
                      --gcc_gen_be_singleton_state.landing_pad_stack_size;
                      return terr;
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());

                  gcc_jit_block *tb = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&tb));
                  if (tb) {
                      if (try_rv) {
                          gcc_jit_block_add_eval(tb, loc, try_rv);
                      }
                      gcc_jit_block_end_with_jump(tb, loc, done_blk);
                  }

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(catch_lp));
                  if (catch_e) {
                      gcc_jit_rvalue *catch_rv = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(catch_e, &catch_rv));
                      GCC_GEN_BE_CHECK(emit_optional_eval(loc, catch_rv));
                  }
                  gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                  gcc_jit_block *after_res = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(gcc_jit_context_null(ctx, void_ptr_type), &after_res));
                  if (after_res) {
                      gcc_jit_block_end_with_jump(after_res, loc, done_blk);
                  }

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(done_blk));
                  *out_rval = NULL;
                  return GCC_GEN_BE_SUCCESS;
              }
              if ((expr->variant.lowered_eh.kind == leck_cleanup_state ||
                   expr->variant.lowered_eh.kind == leck_unreachable_cleanup_state) && cblock) {
                  gcc_gen_be_context_t *be_st = &gcc_gen_be_singleton_state;
                  a_dynamic_init_ptr target_dip = expr->variant.lowered_eh.variant.cleanup_ptr;
                  a_dynamic_init_ptr barrier_dip = (be_st->eh_try_barrier_depth > 0 && be_st->eh_try_barrier_depth <= 32U)
                      ? be_st->eh_try_barriers[be_st->eh_try_barrier_depth - 1]
                      : NULL;
                  a_dynamic_init_ptr chain_rev[64];
                  size_t chain_len = 0;
                  for (a_dynamic_init_ptr cur_d = target_dip;
                       cur_d != NULL && cur_d != barrier_dip && chain_len < 64U;
                       cur_d = cur_d->next_in_destruction_list
                           ? cur_d->next_in_destruction_list
                           : (cur_d->lifetime ? cur_d->lifetime->parent_destruction_sublist : NULL)) {
                      chain_rev[chain_len++] = cur_d;
                  }
                  a_dynamic_init_ptr desired[64];
                  for (size_t di = 0; di < chain_len; ++di) {
                      desired[di] = chain_rev[chain_len - 1 - di];
                  }
                  size_t common_prefix = 0;
                  while (common_prefix < be_st->eh_cleanup_dip_depth &&
                         common_prefix < chain_len &&
                         be_st->eh_cleanup_dips[common_prefix] == desired[common_prefix]) {
                      common_prefix++;
                  }
                  while (be_st->eh_cleanup_dip_depth > common_prefix) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
                      be_st->eh_cleanup_dip_depth--;
                  }
                  gcc_jit_function *cur_fn = gcc_jit_block_get_function(cblock);
                  for (size_t di = common_prefix; di < chain_len; ++di) {
                      a_dynamic_init_ptr d = desired[di];
                      gcc_jit_lvalue *obj_lv = NULL;
                      gcc_jit_function *dtor_fn = NULL;
                      if (d->destructor) {
                          gcc_jit_block *saved_b = cblock;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(d->destructor, &dtor_fn));
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_b));
                      }
                      if (d->variable) {
                          void *cached_v = NULL;
                          GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, d->variable, &cached_v));
                          obj_lv = (gcc_jit_lvalue *)cached_v;
                      } else if (d->is_constructor_init && be_st->active_function_scope &&
                                 be_st->active_function_scope->variant.routine.parameters) {
                          a_variable_ptr this_var = be_st->active_function_scope->variant.routine.parameters;
                          void *cached_this = NULL;
                          GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, this_var, &cached_this));
                          gcc_jit_lvalue *this_lv = (gcc_jit_lvalue *)cached_this;
                          a_type_ptr this_pt = NULL;
                          GCC_GEN_BE_CHECK(strip_expr_typerefs(this_var->type, &this_pt));
                          a_type_ptr cls_tp = (this_pt && this_pt->kind == tk_pointer) ? this_pt->variant.pointer.type : NULL;
                          GCC_GEN_BE_CHECK(strip_expr_typerefs(cls_tp, &cls_tp));
                          a_type_ptr dtor_sub_tp = NULL;
                          if (d->destructor && d->destructor->type) {
                              a_type_ptr drt = NULL;
                              GCC_GEN_BE_CHECK(strip_expr_typerefs(d->destructor->type, &drt));
                              if (drt && drt->kind == tk_routine && drt->variant.routine.extra_info &&
                                  drt->variant.routine.extra_info->param_type_list) {
                                  a_type_ptr dp0 = NULL;
                                  GCC_GEN_BE_CHECK(strip_expr_typerefs(drt->variant.routine.extra_info->param_type_list->type, &dp0));
                                  if (dp0 && dp0->kind == tk_pointer) {
                                      GCC_GEN_BE_CHECK(strip_expr_typerefs(dp0->variant.pointer.type, &dtor_sub_tp));
                                  }
                              }
                          }
                          if (this_lv && cls_tp && (cls_tp->kind == tk_class || cls_tp->kind == tk_struct) && dtor_sub_tp) {
                              unsigned int same_after = 0U;
                              for (a_dynamic_init_ptr sc = be_st->active_function_scope->lifetime
                                       ? be_st->active_function_scope->lifetime->destructions
                                       : NULL;
                                   sc != NULL && sc != d;
                                   sc = sc->next_in_destruction_list) {
                                  if (sc->is_constructor_init && sc->destructor == d->destructor) {
                                      same_after++;
                                  }
                              }
                              unsigned long sub_offs[32];
                              unsigned int n_offs = 0U;
                              for (a_field_ptr cf = cls_tp->variant.class_struct_union.field_list;
                                   cf != NULL && n_offs < 32U;
                                   cf = cf->next) {
                                  a_type_ptr cft = NULL;
                                  GCC_GEN_BE_CHECK(strip_expr_typerefs(cf->type, &cft));
                                  if (cft == dtor_sub_tp) {
                                      sub_offs[n_offs++] = (unsigned long)cf->offset;
                                  }
                              }
                              unsigned long chosen_off = 0UL;
                              if (n_offs > 0U) {
                                  unsigned int pick = (same_after < n_offs) ? (n_offs - 1U - same_after) : 0U;
                                  chosen_off = sub_offs[pick];
                              }
                              gcc_jit_type *char_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
                              gcc_jit_type *char_pt = gcc_jit_type_get_pointer(char_t);
                              gcc_jit_rvalue *this_rv = gcc_jit_lvalue_as_rvalue(this_lv);
                              gcc_jit_rvalue *base_cp = gcc_jit_context_new_cast(ctx, loc, this_rv, char_pt);
                              gcc_jit_rvalue *off_rv = gcc_jit_context_new_rvalue_from_long(
                                  ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), (long)chosen_off);
                              obj_lv = gcc_jit_context_new_array_access(ctx, loc, base_cp, off_rv);
                          }
                      }
                      if (obj_lv && dtor_fn && exceptions_enabled) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_emit_single_dtor_unwind_lp(cur_fn, obj_lv, dtor_fn));
                          be_st->eh_cleanup_dips[be_st->eh_cleanup_dip_depth++] = d;
                      }
                  }
                  *out_rval = NULL;
                  return GCC_GEN_BE_SUCCESS;
              }
              if (cblock && type && type != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                  void *cached_addr = NULL;
                  if (expr->variant.lowered_eh.kind == leck_thrown_object_address) {
                      cached_addr = gcc_gen_be_singleton_state.active_thrown_obj_lval;
                      if (!cached_addr) {
                          GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_thrown_obj_addr", &cached_addr));
                      }
                  } else if (expr->variant.lowered_eh.kind == leck_caught_object_address) {
                      cached_addr = gcc_gen_be_singleton_state.active_caught_obj_lval;
                      if (!cached_addr) {
                          GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_caught_obj_addr", &cached_addr));
                      }
                  }
                  gcc_jit_rvalue *exc_ptr = NULL;
                  if (cached_addr) {
                      exc_ptr = gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached_addr);
                  } else {
                      GCC_GEN_BE_CHECK(emit_eh_unwind_exc_rvalue(ctx, cblock, &exc_ptr));
                  }
                  if (gcc_jit_type_is_pointer(type) != NULL) {
                      *out_rval = gcc_jit_context_new_cast(ctx, NULL, exc_ptr, type);
                  } else {
                      gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, NULL, exc_ptr, gcc_jit_type_get_pointer(type));
                      *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(typed_ptr, NULL));
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
              return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
          }
#endif
          *out_rval = NULL;
          return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_UNHANDLED_EXPR, "unhandled expression node kind");
  }
}

gcc_gen_be_error_t gcc_gen_be_lower_co_await(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    return gcc_gen_be_lower_await_yield(expr, out_rval);
}

gcc_gen_be_error_t gcc_gen_be_lower_co_yield(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    return gcc_gen_be_lower_await_yield(expr, out_rval);
}

gcc_gen_be_error_t gcc_gen_be_lower_await_yield(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&expr->expr_range.start, &loc));

    gcc_jit_type *type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &type));

    a_boolean is_op_form = (expr->kind == enk_operation) ? TRUE : FALSE;
    an_expr_node_ptr rrs = is_op_form ? NULL : expr->variant.await_info.ready_resume_suspend;
    an_expr_node_ptr await_op = is_op_form ? expr->variant.operation.operands : expr->variant.await_info.operand;
    an_expr_node_ptr await_ready = rrs;
    an_expr_node_ptr await_resume = rrs ? rrs->next : NULL;
    an_expr_node_ptr await_suspend = await_resume ? await_resume->next : NULL;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_jit_function *func = gcc_jit_block_get_function(cblock);

    if (await_op) {
        gcc_jit_rvalue *op_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(await_op, &op_rval));
        GCC_GEN_BE_CHECK(emit_optional_eval(loc, op_rval));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    }

    gcc_jit_block *suspend_block = gcc_jit_function_new_block(func, "await_suspend");
    gcc_jit_block *resume_block = gcc_jit_function_new_block(func, "await_resume");

    gcc_jit_rvalue *is_ready = NULL;
    if (await_ready) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(await_ready, &is_ready));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    }
    if (!is_ready) {
        is_ready = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL), 1);
    } else {
        is_ready = gcc_jit_context_new_cast(ctx, loc, is_ready, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL));
    }

    gcc_jit_block_end_with_conditional(cblock, loc, is_ready, resume_block, suspend_block);

    int suspend_state_idx = 0;
    gcc_jit_lvalue *suspend_state_lval = NULL;
    gcc_jit_rvalue *coro_frame_addr = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_register_coro_suspend_point(
        resume_block, &suspend_state_idx, &suspend_state_lval, &coro_frame_addr));

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(suspend_block));
    if (suspend_state_lval && suspend_state_idx > 0) {
        gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        gcc_jit_block_add_assignment(
            suspend_block, loc, suspend_state_lval,
            gcc_jit_context_new_rvalue_from_int(ctx, int_type, suspend_state_idx));
    }
    if (await_suspend) {
        a_type_ptr susp_edg_tp = NULL;
        GCC_GEN_BE_CHECK(strip_expr_typerefs(await_suspend->type, &susp_edg_tp));
        gcc_jit_rvalue *suspend_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(await_suspend, &suspend_rval));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&suspend_block));
        a_boolean is_void_susp = (!susp_edg_tp || susp_edg_tp->kind == tk_void) ? TRUE : FALSE;
        a_boolean is_bool_susp = (susp_edg_tp && susp_edg_tp->kind == tk_integer && susp_edg_tp->size == 1U) ? TRUE : FALSE;
        if (suspend_rval && !is_void_susp && !is_bool_susp) {
            gcc_jit_type *susp_type = gcc_jit_rvalue_get_type(suspend_rval);
            if (susp_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                is_void_susp = TRUE;
            } else if (susp_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL)) {
                is_bool_susp = TRUE;
            }
        }
        if (is_void_susp) {
            if (suspend_rval) {
                gcc_jit_block_add_eval(suspend_block, loc, suspend_rval);
            }
            gcc_jit_block_end_with_void_return(suspend_block, loc);
        } else if (is_bool_susp && suspend_rval) {
            gcc_jit_rvalue *bool_susp = gcc_jit_context_new_cast(
                ctx, loc, suspend_rval, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL));
            gcc_jit_block *suspend_actual = gcc_jit_function_new_block(func, "suspend_actual");
            gcc_jit_block_end_with_conditional(suspend_block, loc, bool_susp, suspend_actual, resume_block);
            gcc_jit_block_end_with_void_return(suspend_actual, loc);
        } else if (suspend_rval) {
            /* Symmetric transfer returning std::coroutine_handle<> */
            GCC_GEN_BE_CHECK(gcc_gen_be_emit_coro_symmetric_transfer(func, suspend_block, suspend_rval));
        } else {
            gcc_jit_block_end_with_void_return(suspend_block, loc);
        }
    } else {
        gcc_jit_block_end_with_jump(suspend_block, NULL, resume_block);
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(resume_block));
    gcc_jit_rvalue *resume_rval = NULL;
    if (await_resume) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(await_resume, &resume_rval));
    }

    if (resume_rval) {
        *out_rval = resume_rval;
    } else if (expr->type && expr->type->kind != tk_void) {
        *out_rval = gcc_jit_context_zero(ctx, type);
    } else {
        *out_rval = NULL;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_vector_shuffle(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&expr->expr_range.start, &loc));

    gcc_jit_type *res_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &res_type));
    if (!res_type) return GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED;

    an_expr_node_ptr op1 = expr->variant.builtin_operation.operands;
    an_expr_node_ptr op2 = op1 ? op1->next : NULL;
    an_expr_node_ptr op3 = op2 ? op2->next : NULL;
    if (!op1 || !op2) return GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED;

    gcc_jit_rvalue *v1 = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &v1));
    if (!v1) return GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED;

    gcc_jit_rvalue *v2 = v1;
    gcc_jit_rvalue *mask_rval = NULL;
    if (expr->variant.builtin_operation.kind == bok_builtin_shuffle) {
        if (op3) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op2, &v2));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op3, &mask_rval));
        } else {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op2, &mask_rval));
            v2 = v1;
        }
        if (p_gcc_jit_context_new_rvalue_vector_perm && v2 && mask_rval) {
            *out_rval = p_gcc_jit_context_new_rvalue_vector_perm(ctx, loc, v1, v2, mask_rval);
            return GCC_GEN_BE_SUCCESS;
        }
    } else {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op2, &v2));
    }

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!cblock) {
        return safe_ptr_or_int_cast(ctx, v1, res_type, out_rval);
    }

    gcc_jit_function *func = gcc_jit_block_get_function(cblock);
    gcc_jit_type *v1_type = gcc_jit_rvalue_get_type(v1);
    gcc_jit_rvalue *eff_v2 = v2 ? v2 : v1;
    gcc_jit_lvalue *src1_tmp = gcc_jit_function_new_local(func, loc, v1_type, "shuf_v1");
    gcc_jit_lvalue *src2_tmp = gcc_jit_function_new_local(func, loc, gcc_jit_rvalue_get_type(eff_v2), "shuf_v2");
    gcc_jit_lvalue *dst_tmp = gcc_jit_function_new_local(func, loc, res_type, "shuf_dst");
    gcc_jit_block_add_assignment(cblock, loc, src1_tmp, v1);
    gcc_jit_block_add_assignment(cblock, loc, src2_tmp, eff_v2);

    a_type_ptr ut = NULL;
    GCC_GEN_BE_CHECK(strip_expr_typerefs(expr->type, &ut));
    int num_lanes = 4;
    gcc_jit_type *lane_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    if (ut && ut->kind == tk_vector && ut->variant.vector.element_type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(ut->variant.vector.element_type, &lane_type));
        if (ut->variant.vector.element_type->size > 0 && ut->size >= ut->variant.vector.element_type->size) {
            num_lanes = (int)(ut->size / ut->variant.vector.element_type->size);
        }
    }

    gcc_jit_type *lane_ptr_type = gcc_jit_type_get_pointer(lane_type);
    gcc_jit_rvalue *src1_ptr = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(src1_tmp, loc), lane_ptr_type);
    gcc_jit_rvalue *src2_ptr = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(src2_tmp, loc), lane_ptr_type);
    gcc_jit_rvalue *dst_ptr = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(dst_tmp, loc), lane_ptr_type);
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

    if (expr->variant.builtin_operation.kind == bok_builtin_shufflevector) {
        an_expr_node_ptr m_op = op3;
        for (int i = 0; i < num_lanes; ++i) {
            long mask_idx = i;
            if (m_op && m_op->kind == enk_constant && m_op->variant.constant.ptr) {
                a_boolean ovf = FALSE;
                mask_idx = (long)value_of_integer_constant(m_op->variant.constant.ptr, &ovf);
            }
            if (mask_idx < 0) mask_idx = 0;
            int total_lanes = 2 * num_lanes;
            int eff_idx = (int)(mask_idx % total_lanes);
            gcc_jit_rvalue *base_ptr = (eff_idx < num_lanes) ? src1_ptr : src2_ptr;
            int lane_off = (eff_idx < num_lanes) ? eff_idx : (eff_idx - num_lanes);
            gcc_jit_rvalue *lane_val = gcc_jit_lvalue_as_rvalue(
                gcc_jit_context_new_array_access(ctx, loc, base_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, lane_off)));
            gcc_jit_lvalue *dst_lane = gcc_jit_context_new_array_access(
                ctx, loc, dst_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, i));
            gcc_jit_block_add_assignment(cblock, loc, dst_lane, gcc_jit_context_new_cast(ctx, loc, lane_val, lane_type));
            if (m_op) m_op = m_op->next;
        }
    } else if (mask_rval) {
        gcc_jit_lvalue *mask_tmp = gcc_jit_function_new_local(func, loc, gcc_jit_rvalue_get_type(mask_rval), "shuf_mask");
        gcc_jit_block_add_assignment(cblock, loc, mask_tmp, mask_rval);
        gcc_jit_rvalue *mask_ptr = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(mask_tmp, loc), gcc_jit_type_get_pointer(int_type));
        int mod_n = op3 ? (2 * num_lanes) : num_lanes;
        gcc_jit_rvalue *mod_rval = gcc_jit_context_new_rvalue_from_int(ctx, int_type, mod_n);
        gcc_jit_rvalue *n_rval = gcc_jit_context_new_rvalue_from_int(ctx, int_type, num_lanes);
        for (int i = 0; i < num_lanes; ++i) {
            gcc_jit_rvalue *idx_i = gcc_jit_context_new_rvalue_from_int(ctx, int_type, i);
            gcc_jit_rvalue *raw_m = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, mask_ptr, idx_i));
            gcc_jit_rvalue *m_mod = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MODULO, int_type, raw_m, mod_rval);
            gcc_jit_rvalue *from_v1 = gcc_jit_lvalue_as_rvalue(
                gcc_jit_context_new_array_access(
                    ctx, loc, src1_ptr,
                    gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MODULO, int_type, m_mod, n_rval)));
            gcc_jit_rvalue *from_v2 = gcc_jit_lvalue_as_rvalue(
                gcc_jit_context_new_array_access(
                    ctx, loc, src2_ptr,
                    gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MODULO, int_type, m_mod, n_rval)));
            gcc_jit_rvalue *in_first = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_LT, m_mod, n_rval);
            gcc_jit_rvalue *cond_int = gcc_jit_context_new_cast(ctx, loc, in_first, lane_type);
            gcc_jit_rvalue *inv_cond = gcc_jit_context_new_cast(
                ctx, loc, gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL), in_first),
                lane_type);
            gcc_jit_rvalue *sel_val = gcc_jit_context_new_binary_op(
                ctx, loc, GCC_JIT_BINARY_OP_PLUS, lane_type,
                gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, lane_type, from_v1, cond_int),
                gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, lane_type, from_v2, inv_cond));
            gcc_jit_lvalue *dst_lane = gcc_jit_context_new_array_access(ctx, loc, dst_ptr, idx_i);
            gcc_jit_block_add_assignment(cblock, loc, dst_lane, sel_val);
        }
    }

    *out_rval = gcc_jit_lvalue_as_rvalue(dst_tmp);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_convertvector(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&expr->expr_range.start, &loc));

    gcc_jit_type *res_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &res_type));
    if (!res_type) return GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED;

    an_expr_node_ptr op1 = expr->variant.builtin_operation.operands;
    an_expr_node_ptr src_op = (op1 && op1->kind == enk_type_operand) ? op1->next : op1;
    if (!src_op) return GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED;

    gcc_jit_rvalue *src_vec = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(src_op, &src_vec));
    if (!src_vec) return GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED;

    if (p_gcc_jit_context_new_convert_vector) {
        *out_rval = p_gcc_jit_context_new_convert_vector(ctx, loc, src_vec, res_type);
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!cblock) {
        return safe_ptr_or_int_cast(ctx, src_vec, res_type, out_rval);
    }

    gcc_jit_function *func = gcc_jit_block_get_function(cblock);
    gcc_jit_lvalue *src_tmp = gcc_jit_function_new_local(func, loc, gcc_jit_rvalue_get_type(src_vec), "convvec_src");
    gcc_jit_lvalue *dst_tmp = gcc_jit_function_new_local(func, loc, res_type, "convvec_dst");
    gcc_jit_block_add_assignment(cblock, loc, src_tmp, src_vec);

    a_type_ptr src_ut = NULL;
    GCC_GEN_BE_CHECK(strip_expr_typerefs(src_op->type, &src_ut));
    a_type_ptr dst_ut = NULL;
    GCC_GEN_BE_CHECK(strip_expr_typerefs(expr->type, &dst_ut));

    gcc_jit_type *src_lane_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *dst_lane_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    int num_lanes = 4;
    if (src_ut && src_ut->kind == tk_vector && src_ut->variant.vector.element_type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(src_ut->variant.vector.element_type, &src_lane_t));
    }
    if (dst_ut && dst_ut->kind == tk_vector && dst_ut->variant.vector.element_type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(dst_ut->variant.vector.element_type, &dst_lane_t));
        if (dst_ut->variant.vector.element_type->size > 0 && dst_ut->size >= dst_ut->variant.vector.element_type->size) {
            num_lanes = (int)(dst_ut->size / dst_ut->variant.vector.element_type->size);
        }
    }

    gcc_jit_rvalue *src_ptr = gcc_jit_context_new_cast(
        ctx, loc, gcc_jit_lvalue_get_address(src_tmp, loc), gcc_jit_type_get_pointer(src_lane_t));
    gcc_jit_rvalue *dst_ptr = gcc_jit_context_new_cast(
        ctx, loc, gcc_jit_lvalue_get_address(dst_tmp, loc), gcc_jit_type_get_pointer(dst_lane_t));
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

    for (int i = 0; i < num_lanes; ++i) {
        gcc_jit_rvalue *idx = gcc_jit_context_new_rvalue_from_int(ctx, int_type, i);
        gcc_jit_rvalue *src_lane = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, loc, src_ptr, idx));
        gcc_jit_rvalue *conv_lane = gcc_jit_context_new_cast(ctx, loc, src_lane, dst_lane_t);
        gcc_jit_lvalue *dst_lane = gcc_jit_context_new_array_access(ctx, loc, dst_ptr, idx);
        gcc_jit_block_add_assignment(cblock, loc, dst_lane, conv_lane);
    }

    *out_rval = gcc_jit_lvalue_as_rvalue(dst_tmp);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_float_constant(a_constant_ptr con, struct gcc_jit_type *expected_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!con || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    if (con->kind != ck_float) return GCC_GEN_BE_ERROR_FLOAT_PRECISION_FAILED;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    a_float_kind fk = fk_double;
    a_type_ptr ut = NULL;
    GCC_GEN_BE_CHECK(strip_expr_typerefs(con->type, &ut));
    if (ut && ut->kind == tk_float) {
        fk = ut->variant.float_kind;
    }

    gcc_jit_type *flt_type = expected_type;
    if (!flt_type) {
        if (ut) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(ut, &flt_type));
        }
        if (!flt_type) {
            flt_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
        }
    }

#if USE_SOFTFLOAT
    a_host_fp_value host_fp = fetch_host_fp_value(fk, &con->variant.float_value);
    edg_softfloat64_t f64_hi = f128M_to_f64(&host_fp);
    double dbl_hi = 0.0;
    memcpy(&dbl_hi, &f64_hi.v, sizeof(double));
#else
    long double ld_val = (long double)fetch_host_fp_value(fk, &con->variant.float_value);
    double dbl_hi = (double)ld_val;
#endif
    gcc_jit_rvalue *hi_rval = gcc_jit_context_new_rvalue_from_double(ctx, flt_type, dbl_hi);

#if (LDBL_MANT_DIG > DBL_MANT_DIG) && !USE_SOFTFLOAT
    if ((fk == fk_long_double || fk == fk_float80 || fk == fk_float128 || fk == fk_std_float128) &&
        isfinite(ld_val)) {
        long double rem1 = ld_val - (long double)dbl_hi;
        if (rem1 != 0.0L) {
            double dbl_mid = (double)rem1;
            gcc_jit_rvalue *mid_rval = gcc_jit_context_new_rvalue_from_double(ctx, flt_type, dbl_mid);
            gcc_jit_rvalue *sum = gcc_jit_context_new_binary_op(
                ctx, NULL, GCC_JIT_BINARY_OP_PLUS, flt_type, hi_rval, mid_rval);
            long double rem2 = rem1 - (long double)dbl_mid;
            if (rem2 != 0.0L) {
                gcc_jit_rvalue *lo_rval = gcc_jit_context_new_rvalue_from_double(ctx, flt_type, (double)rem2);
                sum = gcc_jit_context_new_binary_op(
                    ctx, NULL, GCC_JIT_BINARY_OP_PLUS, flt_type, sum, lo_rval);
            }
            *out_rval = sum;
            return GCC_GEN_BE_SUCCESS;
        }
    }
#endif

    *out_rval = hi_rval;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_decimal_float_op(
    int op_code,
    int byte_width,
    struct gcc_jit_rvalue *lhs,
    struct gcc_jit_rvalue *rhs,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!lhs || !rhs || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    const char *mode_suffix = NULL;
    if (byte_width == 4) mode_suffix = "sd3";
    else if (byte_width == 8) mode_suffix = "dd3";
    else if (byte_width == 16) mode_suffix = "td3";
    else return GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED;

    const char *op_prefix = NULL;
    if (op_code == '+') op_prefix = "add";
    else if (op_code == '-') op_prefix = "sub";
    else if (op_code == '*') op_prefix = "mul";
    else if (op_code == '/') op_prefix = "div";
    else return GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED;

    int abi_ov = gcc_gen_be_singleton_state.active_target_abi_override;
    const char *df_prefix = (abi_ov == ((int)GCC_GEN_BE_TARGET_ABI_PPC64_ELFV2 + 1) ||
                             abi_ov == ((int)GCC_GEN_BE_TARGET_ABI_S390X + 1))
        ? "__dpd_"
        : "__bid_";
    char helper_name[64];
    snprintf(helper_name, sizeof(helper_name), "%s%s%s", df_prefix, op_prefix, mode_suffix);

    gcc_jit_type *dec_type = gcc_jit_rvalue_get_type(lhs);
    gcc_jit_param *params[2];
    params[0] = gcc_jit_context_new_param(ctx, NULL, dec_type, "a");
    params[1] = gcc_jit_context_new_param(ctx, NULL, dec_type, "b");
    gcc_jit_function *helper_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, dec_type, helper_name, 2, params, 0);
    gcc_jit_rvalue *call_args[2] = { lhs, rhs };
    *out_rval = gcc_jit_context_new_call(ctx, NULL, helper_fn, 2, call_args);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_fixed_point_op(
    int op_code,
    int frac_bits,
    a_boolean is_saturating,
    struct gcc_jit_rvalue *lhs,
    struct gcc_jit_rvalue *rhs,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!lhs || !rhs || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    if (frac_bits <= 0 || frac_bits >= 63) {
        return GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW;
    }

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_type *res_type = gcc_jit_rvalue_get_type(lhs);
    gcc_jit_type *wide_type = (frac_bits >= 31)
        ? gcc_jit_context_get_int_type(ctx, 16, 1)
        : gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG);
    gcc_jit_rvalue *w_lhs = gcc_jit_context_new_cast(ctx, NULL, lhs, wide_type);
    gcc_jit_rvalue *w_rhs = gcc_jit_context_new_cast(ctx, NULL, rhs, wide_type);
    gcc_jit_rvalue *shift_rv = gcc_jit_context_new_rvalue_from_int(ctx, wide_type, frac_bits);

    gcc_jit_rvalue *wide_res = NULL;
    if (op_code == '+') {
        wide_res = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_PLUS, wide_type, w_lhs, w_rhs);
    } else if (op_code == '-') {
        wide_res = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, wide_type, w_lhs, w_rhs);
    } else if (op_code == '*') {
        gcc_jit_rvalue *prod = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, wide_type, w_lhs, w_rhs);
        wide_res = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_RSHIFT, wide_type, prod, shift_rv);
    } else if (op_code == '/') {
        gcc_jit_rvalue *scaled = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, wide_type, w_lhs, shift_rv);
        wide_res = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_DIVIDE, wide_type, scaled, w_rhs);
    } else {
        return GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW;
    }

    if (is_saturating) {
        long max_bound = (frac_bits < 31) ? ((1L << (frac_bits + 1)) - 1L) : 0x7fffffffffffffffL;
        long min_bound = -max_bound - 1L;
        gcc_jit_rvalue *max_rv = gcc_jit_context_new_rvalue_from_long(ctx, wide_type, max_bound);
        gcc_jit_rvalue *min_rv = gcc_jit_context_new_rvalue_from_long(ctx, wide_type, min_bound);
        gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
        gcc_jit_rvalue *gt_max = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_GT, wide_res, max_rv);
        gcc_jit_rvalue *le_max = gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, bool_t, gt_max);
        wide_res = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_PLUS, wide_type,
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, wide_type, max_rv, gcc_jit_context_new_cast(ctx, NULL, gt_max, wide_type)),
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, wide_type, wide_res, gcc_jit_context_new_cast(ctx, NULL, le_max, wide_type)));
        gcc_jit_rvalue *lt_min = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_LT, wide_res, min_rv);
        gcc_jit_rvalue *ge_min = gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, bool_t, lt_min);
        wide_res = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_PLUS, wide_type,
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, wide_type, min_rv, gcc_jit_context_new_cast(ctx, NULL, lt_min, wide_type)),
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, wide_type, wide_res, gcc_jit_context_new_cast(ctx, NULL, ge_min, wide_type)));
    }

    *out_rval = (res_type == wide_type) ? wide_res : gcc_jit_context_new_cast(ctx, NULL, wide_res, res_type);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_pack_bitfield_constants(
    const unsigned long *bit_values,
    const unsigned int *bit_offsets,
    const unsigned int *bit_widths,
    int count,
    unsigned long *out_packed_word) GCC_GEN_BE_NOEXCEPT {
    if (!out_packed_word || count < 0 || (count > 0 && (!bit_values || !bit_offsets || !bit_widths))) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    unsigned long packed = 0UL;
    for (int i = 0; i < count; ++i) {
        unsigned int w = bit_widths[i];
        unsigned int sh = bit_offsets[i];
        if (w == 0 || sh >= 64U) continue;
        unsigned long mask = (w >= 64U) ? ~0UL : ((1UL << w) - 1UL);
        packed |= ((bit_values[i] & mask) << sh);
    }
    *out_packed_word = packed;
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */