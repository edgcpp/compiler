/**
 * @file gcc_gen_be_simd.c
 * @brief Implementation of target SIMD vector construction and x86 SSE/AVX & ARM NEON builtins.
 * @details Lowers vector constructors via `gcc_jit_context_new_rvalue_from_vector` (with memory-lane
 * fallback when absent), scalar broadcasts, lane extract/insert, element-wise arithmetic/bitwise/min/max,
 * fused multiply-add, horizontal reductions, and unaligned vector loads/stores across x86 and ARM64.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_simd.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_lib_loader.h"
#include <libgccjit.h>
#include <string.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

gcc_gen_be_error_t gcc_gen_be_build_vector_rvalue(
    struct gcc_jit_type *vec_type,
    struct gcc_jit_type *lane_type,
    size_t num_lanes,
    struct gcc_jit_rvalue **elements,
    struct gcc_jit_rvalue **out_vec) GCC_GEN_BE_NOEXCEPT {
    if (!vec_type || !lane_type || num_lanes == 0 || !elements || !out_vec) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_vec = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }

    void *raw_buf = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(num_lanes * sizeof(gcc_jit_rvalue *), &raw_buf));
    gcc_jit_rvalue **coerced = (gcc_jit_rvalue **)raw_buf;
    for (size_t i = 0; i < num_lanes; ++i) {
        if (!elements[i]) {
            free(coerced);
            return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        }
        coerced[i] = (gcc_jit_rvalue_get_type(elements[i]) == lane_type)
            ? elements[i]
            : gcc_jit_context_new_cast(ctx, NULL, elements[i], lane_type);
    }

    if (p_gcc_jit_context_new_rvalue_from_vector) {
        *out_vec = p_gcc_jit_context_new_rvalue_from_vector(ctx, NULL, vec_type, num_lanes, coerced);
        free(coerced);
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_block *cblock = NULL;
    gcc_gen_be_error_t g_simd_err = gcc_gen_be_get_current_block(&cblock);
    if (g_simd_err != GCC_GEN_BE_SUCCESS) {
        free(coerced);
        return g_simd_err;
    }
    if (!cblock) {
        free(coerced);
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }
    gcc_jit_function *fn = gcc_jit_block_get_function(cblock);
    gcc_jit_lvalue *vec_slot = gcc_jit_function_new_local(fn, NULL, vec_type, "__simd_vec_init");
    gcc_jit_rvalue *lane_ptr = gcc_jit_context_new_cast(
        ctx, NULL, gcc_jit_lvalue_get_address(vec_slot, NULL), gcc_jit_type_get_pointer(lane_type));
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    for (size_t i = 0; i < num_lanes; ++i) {
        gcc_jit_rvalue *idx = gcc_jit_context_new_rvalue_from_int(ctx, int_t, (int)i);
        gcc_jit_block_add_assignment(
            cblock, NULL, gcc_jit_context_new_array_access(ctx, NULL, lane_ptr, idx), coerced[i]);
    }
    *out_vec = gcc_jit_lvalue_as_rvalue(vec_slot);
    free(coerced);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_splat_vector_rvalue(
    struct gcc_jit_type *vec_type,
    struct gcc_jit_type *lane_type,
    size_t num_lanes,
    struct gcc_jit_rvalue *lane_val,
    struct gcc_jit_rvalue **out_vec) GCC_GEN_BE_NOEXCEPT {
    if (!vec_type || !lane_type || num_lanes == 0 || !lane_val || !out_vec) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    void *raw_lanes = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(num_lanes * sizeof(gcc_jit_rvalue *), &raw_lanes));
    gcc_jit_rvalue **lanes = (gcc_jit_rvalue **)raw_lanes;
    for (size_t i = 0; i < num_lanes; ++i) {
        lanes[i] = lane_val;
    }
    gcc_gen_be_error_t err = gcc_gen_be_build_vector_rvalue(vec_type, lane_type, num_lanes, lanes, out_vec);
    free(lanes);
    return err;
}

/**
 * @brief Extracts vector geometry (`vec_type`, `lane_type`, `num_lanes`) from an EDG vector type.
 * @details Unwraps `tk_typeref` nodes and queries `tp->variant.vector.element_type` and `tp->size`.
 * @param[in] edg_vec_type Frontend `a_type_ptr` (may be NULL).
 * @param[in] fallback_jit_type Fallback `gcc_jit_type` when `edg_vec_type` is NULL.
 * @param[out] out_vec_type Pointer receiving lowered vector `gcc_jit_type`.
 * @param[out] out_lane_type Pointer receiving lowered lane `gcc_jit_type`.
 * @param[out] out_num_lanes Pointer receiving number of lanes (`>= 1`).
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t resolve_simd_vector_geometry(
    a_type_ptr edg_vec_type,
    gcc_jit_type *fallback_jit_type,
    gcc_jit_type **out_vec_type,
    gcc_jit_type **out_lane_type,
    size_t *out_num_lanes) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    while (edg_vec_type && edg_vec_type->kind == tk_typeref) {
        edg_vec_type = edg_vec_type->variant.typeref.type;
    }
    if (edg_vec_type && edg_vec_type->kind == tk_vector && edg_vec_type->variant.vector.element_type) {
        a_type_ptr elem_tp = edg_vec_type->variant.vector.element_type;
        while (elem_tp && elem_tp->kind == tk_typeref) elem_tp = elem_tp->variant.typeref.type;
        if (elem_tp) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(edg_vec_type, out_vec_type));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(elem_tp, out_lane_type));
            *out_num_lanes = (elem_tp->size > 0 && edg_vec_type->size >= elem_tp->size)
                ? (size_t)(edg_vec_type->size / elem_tp->size)
                : 4U;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    gcc_jit_type *flt_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
    *out_lane_type = flt_t;
    *out_num_lanes = 4U;
    *out_vec_type = fallback_jit_type;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers a lane-by-lane min/max, FMA, or horizontal reduction across SIMD vectors in memory.
 * @details Spill-loads vector operands into local slots and computes exact lane-wise results, packing
 * the resulting lanes via `gcc_gen_be_build_vector_rvalue` (or returning scalar accumulator for `addv`).
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] cblock Active `gcc_jit_block`.
 * @param[in] vec_type Lowered vector `gcc_jit_type`.
 * @param[in] lane_type Lowered scalar lane `gcc_jit_type`.
 * @param[in] num_lanes Number of scalar lanes in `vec_type`.
 * @param[in] mode Operation selector (`0` = min, `1` = max, `2` = fma, `3` = horizontal sum `addv`).
 * @param[in] args Input vector argument array.
 * @param[out] out_rval Pointer receiving the lowered vector or scalar result.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t lower_lane_wise_simd_op(
    gcc_jit_context *ctx,
    gcc_jit_block *cblock,
    gcc_jit_type *vec_type,
    gcc_jit_type *lane_type,
    size_t num_lanes,
    int mode,
    gcc_jit_rvalue **args,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_jit_function *fn = gcc_jit_block_get_function(cblock);
    gcc_jit_type *lane_ptr_t = gcc_jit_type_get_pointer(lane_type);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

    gcc_jit_lvalue *s0 = gcc_jit_function_new_local(fn, NULL, gcc_jit_rvalue_get_type(args[0]), "__simd_s0");
    gcc_jit_block_add_assignment(cblock, NULL, s0, args[0]);
    gcc_jit_rvalue *p0 = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(s0, NULL), lane_ptr_t);

    if (mode == 3) {
        gcc_jit_rvalue *acc = NULL;
        for (size_t i = 0; i < num_lanes; ++i) {
            gcc_jit_rvalue *idx = gcc_jit_context_new_rvalue_from_int(ctx, int_t, (int)i);
            gcc_jit_rvalue *elt = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, NULL, p0, idx));
            acc = (i == 0)
                ? elt
                : gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_PLUS, lane_type, acc, elt);
        }
        *out_rval = acc;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_lvalue *s1 = gcc_jit_function_new_local(fn, NULL, gcc_jit_rvalue_get_type(args[1]), "__simd_s1");
    gcc_jit_block_add_assignment(cblock, NULL, s1, args[1]);
    gcc_jit_rvalue *p1 = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(s1, NULL), lane_ptr_t);

    gcc_jit_rvalue *p2 = NULL;
    if (mode == 2) {
        gcc_jit_lvalue *s2 = gcc_jit_function_new_local(fn, NULL, gcc_jit_rvalue_get_type(args[2]), "__simd_s2");
        gcc_jit_block_add_assignment(cblock, NULL, s2, args[2]);
        p2 = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(s2, NULL), lane_ptr_t);
    }

    void *raw_out = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(num_lanes * sizeof(gcc_jit_rvalue *), &raw_out));
    gcc_jit_rvalue **lanes = (gcc_jit_rvalue **)raw_out;
    for (size_t i = 0; i < num_lanes; ++i) {
        gcc_jit_rvalue *idx = gcc_jit_context_new_rvalue_from_int(ctx, int_t, (int)i);
        gcc_jit_rvalue *a = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, NULL, p0, idx));
        gcc_jit_rvalue *b = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, NULL, p1, idx));
        if (mode == 2) {
            gcc_jit_rvalue *c = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, NULL, p2, idx));
            gcc_jit_rvalue *prod = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, lane_type, a, b);
            lanes[i] = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_PLUS, lane_type, prod, c);
        } else {
            gcc_jit_lvalue *l_res = gcc_jit_function_new_local(fn, NULL, lane_type, "__simd_minmax_lane");
            gcc_jit_block *t_blk = gcc_jit_function_new_block(fn, "simd_mm_then");
            gcc_jit_block *e_blk = gcc_jit_function_new_block(fn, "simd_mm_else");
            gcc_jit_block *m_blk = gcc_jit_function_new_block(fn, "simd_mm_merge");
            enum gcc_jit_comparison cmp = (mode == 0) ? GCC_JIT_COMPARISON_LT : GCC_JIT_COMPARISON_GT;
            gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, cmp, a, b);
            gcc_jit_block_end_with_conditional(cblock, NULL, cond, t_blk, e_blk);
            gcc_jit_block_add_assignment(t_blk, NULL, l_res, a);
            gcc_jit_block_end_with_jump(t_blk, NULL, m_blk);
            gcc_jit_block_add_assignment(e_blk, NULL, l_res, b);
            gcc_jit_block_end_with_jump(e_blk, NULL, m_blk);
            cblock = m_blk;
            lanes[i] = gcc_jit_lvalue_as_rvalue(l_res);
        }
    }
    gcc_gen_be_error_t g_lw_err = gcc_gen_be_set_current_block(cblock);
    gcc_gen_be_error_t g_bv_err = gcc_gen_be_build_vector_rvalue(vec_type, lane_type, num_lanes, lanes, out_rval);
    free(lanes);
    return (g_bv_err != GCC_GEN_BE_SUCCESS) ? g_bv_err : g_lw_err;
}

gcc_gen_be_error_t gcc_gen_be_try_lower_simd_builtin(
    const char *name,
    a_routine_ptr rout,
    int num_args,
    struct gcc_jit_rvalue **args,
    a_boolean *out_handled,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!name || !out_handled || !out_rval) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_handled = FALSE;
    *out_rval = NULL;

    a_boolean is_ia32 = (strncmp(name, "__builtin_ia32_", 15) == 0) ? TRUE : FALSE;
    a_boolean is_neon = (strncmp(name, "__builtin_neon_", 15) == 0) ? TRUE : FALSE;
    if (!is_ia32 && !is_neon) {
        return GCC_GEN_BE_SUCCESS;
    }
    if (num_args <= 0 || !args) {
        return GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED;
    }

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!ctx || !cblock) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }

    const char *op_name = name + 15;
    a_type_ptr ret_edg_type = (rout && rout->type && rout->type->kind == tk_routine)
        ? rout->type->variant.routine.return_type
        : NULL;
    gcc_jit_type *vec_type = NULL;
    gcc_jit_type *lane_type = NULL;
    size_t num_lanes = 4U;
    GCC_GEN_BE_CHECK(resolve_simd_vector_geometry(
        ret_edg_type, gcc_jit_rvalue_get_type(args[0]), &vec_type, &lane_type, &num_lanes));

    /* Scalar splat / duplicate (__builtin_neon_vdup_n*, __builtin_ia32_vec_set_v* broadcast) */
    if ((strncmp(op_name, "vdup_n", 6) == 0 || strncmp(op_name, "vbroadcast", 10) == 0) && num_args == 1) {
        *out_handled = TRUE;
        return gcc_gen_be_splat_vector_rvalue(vec_type, lane_type, num_lanes, args[0], out_rval);
    }

    /* Lane extraction (__builtin_ia32_vec_ext_*, __builtin_neon_vget_lane*) */
    if ((strncmp(op_name, "vec_ext_", 8) == 0 || strncmp(op_name, "vget_lane", 9) == 0 ||
         strncmp(op_name, "vgetq_lane", 10) == 0) && num_args >= 2 &&
        gcc_jit_type_is_integral(gcc_jit_rvalue_get_type(args[1]))) {
        gcc_jit_function *fn = gcc_jit_block_get_function(cblock);
        gcc_jit_lvalue *slot = gcc_jit_function_new_local(fn, NULL, gcc_jit_rvalue_get_type(args[0]), "__simd_ext");
        gcc_jit_block_add_assignment(cblock, NULL, slot, args[0]);
        gcc_jit_rvalue *lptr = gcc_jit_context_new_cast(
            ctx, NULL, gcc_jit_lvalue_get_address(slot, NULL), gcc_jit_type_get_pointer(lane_type));
        gcc_jit_rvalue *idx = gcc_jit_context_new_cast(
            ctx, NULL, args[1], gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
        *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, NULL, lptr, idx));
        *out_handled = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    /* Lane insertion (__builtin_ia32_vec_set_*, __builtin_neon_vset_lane*) */
    if ((strncmp(op_name, "vec_set_", 8) == 0 || strncmp(op_name, "vset_lane", 9) == 0 ||
         strncmp(op_name, "vsetq_lane", 10) == 0) && num_args >= 3 &&
        gcc_jit_type_is_integral(gcc_jit_rvalue_get_type(args[2]))) {
        gcc_jit_rvalue *vec_arg = is_neon ? args[1] : args[0];
        gcc_jit_rvalue *val_arg = is_neon ? args[0] : args[1];
        gcc_jit_rvalue *idx_arg = args[2];
        gcc_jit_type *v_t = gcc_jit_rvalue_get_type(vec_arg);
        gcc_jit_function *fn = gcc_jit_block_get_function(cblock);
        gcc_jit_lvalue *slot = gcc_jit_function_new_local(fn, NULL, v_t, "__simd_set");
        gcc_jit_block_add_assignment(cblock, NULL, slot, vec_arg);
        gcc_jit_rvalue *lptr = gcc_jit_context_new_cast(
            ctx, NULL, gcc_jit_lvalue_get_address(slot, NULL), gcc_jit_type_get_pointer(lane_type));
        gcc_jit_rvalue *idx = gcc_jit_context_new_cast(
            ctx, NULL, idx_arg, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
        gcc_jit_rvalue *cval = (gcc_jit_rvalue_get_type(val_arg) == lane_type)
            ? val_arg
            : gcc_jit_context_new_cast(ctx, NULL, val_arg, lane_type);
        gcc_jit_block_add_assignment(cblock, NULL, gcc_jit_context_new_array_access(ctx, NULL, lptr, idx), cval);
        *out_rval = gcc_jit_lvalue_as_rvalue(slot);
        *out_handled = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    /* Unaligned vector load (__builtin_ia32_loadu*, __builtin_neon_vld1*) */
    if ((strncmp(op_name, "loadu", 5) == 0 || strncmp(op_name, "loaddqu", 7) == 0 ||
         strncmp(op_name, "vld1", 4) == 0) &&
        gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(args[0])) != NULL) {
        gcc_jit_rvalue *vptr = gcc_jit_context_new_cast(ctx, NULL, args[0], gcc_jit_type_get_pointer(vec_type));
        *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(vptr, NULL));
        *out_handled = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    /* Unaligned vector store (__builtin_ia32_storeu*, __builtin_neon_vst1*) */
    if ((strncmp(op_name, "storeu", 6) == 0 || strncmp(op_name, "storedqu", 8) == 0 ||
         strncmp(op_name, "vst1", 4) == 0) && num_args >= 2 &&
        gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(args[0])) != NULL) {
        gcc_jit_type *val_vtype = gcc_jit_rvalue_get_type(args[1]);
        gcc_jit_rvalue *vptr = gcc_jit_context_new_cast(ctx, NULL, args[0], gcc_jit_type_get_pointer(val_vtype));
        gcc_jit_block_add_assignment(cblock, NULL, gcc_jit_rvalue_dereference(vptr, NULL), args[1]);
        *out_handled = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    /* Element-wise binary arithmetic & bitwise operations */
    if (num_args == 2) {
        gcc_jit_type *v_t = gcc_jit_rvalue_get_type(args[0]);
        if (strncmp(op_name, "add", 3) == 0 || strncmp(op_name, "padd", 4) == 0 || strncmp(op_name, "vadd", 4) == 0) {
            *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_PLUS, v_t, args[0], args[1]);
            *out_handled = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
        if (strncmp(op_name, "sub", 3) == 0 || strncmp(op_name, "psub", 4) == 0 || strncmp(op_name, "vsub", 4) == 0) {
            *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, v_t, args[0], args[1]);
            *out_handled = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
        if (strncmp(op_name, "mul", 3) == 0 || strncmp(op_name, "pmulld", 6) == 0 || strncmp(op_name, "vmul", 4) == 0) {
            *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, v_t, args[0], args[1]);
            *out_handled = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
        if (strncmp(op_name, "div", 3) == 0 || strncmp(op_name, "vdiv", 4) == 0) {
            *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_DIVIDE, v_t, args[0], args[1]);
            *out_handled = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
        if (strncmp(op_name, "pandn", 5) == 0 || strncmp(op_name, "andnp", 5) == 0) {
            gcc_jit_rvalue *inv = gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_BITWISE_NEGATE, v_t, args[0]);
            *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_AND, v_t, inv, args[1]);
            *out_handled = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
        if (strncmp(op_name, "pand", 4) == 0 || strncmp(op_name, "andp", 4) == 0 || strncmp(op_name, "vand", 4) == 0) {
            *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_AND, v_t, args[0], args[1]);
            *out_handled = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
        if (strncmp(op_name, "por", 3) == 0 || strncmp(op_name, "orp", 3) == 0 || strncmp(op_name, "vorr", 4) == 0) {
            *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_OR, v_t, args[0], args[1]);
            *out_handled = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
        if (strncmp(op_name, "pxor", 4) == 0 || strncmp(op_name, "xorp", 4) == 0 || strncmp(op_name, "veor", 4) == 0) {
            *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_XOR, v_t, args[0], args[1]);
            *out_handled = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
        if (strncmp(op_name, "min", 3) == 0 || strncmp(op_name, "pmin", 4) == 0 || strncmp(op_name, "vmin", 4) == 0) {
            *out_handled = TRUE;
            return lower_lane_wise_simd_op(ctx, cblock, v_t, lane_type, num_lanes, 0, args, out_rval);
        }
        if (strncmp(op_name, "max", 3) == 0 || strncmp(op_name, "pmax", 4) == 0 || strncmp(op_name, "vmax", 4) == 0) {
            *out_handled = TRUE;
            return lower_lane_wise_simd_op(ctx, cblock, v_t, lane_type, num_lanes, 1, args, out_rval);
        }
    }

    /* Fused multiply-add (__builtin_ia32_vfmadd*, __builtin_neon_vfma*) */
    if ((strncmp(op_name, "vfmadd", 6) == 0 || strncmp(op_name, "vfma", 4) == 0) && num_args == 3) {
        *out_handled = TRUE;
        gcc_jit_rvalue *fma_args[3] = {
            is_neon ? args[1] : args[0],
            is_neon ? args[2] : args[1],
            is_neon ? args[0] : args[2]
        };
        return lower_lane_wise_simd_op(
            ctx, cblock, gcc_jit_rvalue_get_type(args[0]), lane_type, num_lanes, 2, fma_args, out_rval);
    }

    /* Horizontal vector reduction (__builtin_neon_vaddv*) */
    if (strncmp(op_name, "vaddv", 5) == 0 && num_args == 1) {
        *out_handled = TRUE;
        return lower_lane_wise_simd_op(
            ctx, cblock, gcc_jit_rvalue_get_type(args[0]), lane_type, num_lanes, 3, args, out_rval);
    }

    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
