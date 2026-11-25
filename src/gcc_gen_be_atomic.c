/**
 * @file gcc_gen_be_atomic.c
 * @brief Implementation of C11 `_Atomic(T)` and generic `__atomic_*` / `libatomic` lowering.
 * @details Lowers C11 `TQ_C11_ATOMIC` loads, stores, and read-modify-write operations, as well as
 * generic `__atomic_load`, `__atomic_store`, `__atomic_exchange`, and `__atomic_compare_exchange`,
 * using hardware lock-free `libgccjit` builtins for 1/2/4/8-byte scalars and `libatomic` ABI calls
 * for non-lock-free or aggregate widths while writing `*expected` strictly on failure.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_atomic.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_lib_loader.h"
#include <libgccjit.h>
#include <stdio.h>
#include <string.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/** @brief Sequentially-consistent memory order constant (`__ATOMIC_SEQ_CST == 5`). */
#define GCC_BE_ATOMIC_SEQ_CST 5

/**
 * @brief Unwraps `tk_typeref` qualifiers to obtain the underlying unqualified EDG type and byte size.
 * @details Walks any chain of `tk_typeref` nodes and computes `size_of_type` on the unwrapped type.
 * @param[in] tp Frontend `a_type_ptr` (must be non-NULL).
 * @param[out] out_unwrapped Pointer receiving the unwrapped `a_type_ptr`.
 * @param[out] out_size Pointer receiving the byte width of `*out_unwrapped`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if arguments are invalid.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t unwrap_atomic_value_type(
    a_type_ptr tp,
    a_type_ptr *out_unwrapped,
    unsigned long *out_size) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    while (tp && tp->kind == tk_typeref) {
        tp = tp->variant.typeref.type;
    }
    if (!tp) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_unwrapped = tp;
    *out_size = (unsigned long)tp->size;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Maps a lock-free byte size (`1`, `2`, `4`, or `8`) to the exact unsigned integer `gcc_jit_type`.
 * @details Sets `*out_word_type` to `uint8_t`, `uint16_t`, `uint32_t`, or `uint64_t` for lock-free widths,
 * or `NULL` when `byte_size` is not one of `{1, 2, 4, 8}`.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] byte_size Candidate object size in bytes.
 * @param[out] out_word_type Non-null pointer receiving the unsigned integer word type (or NULL if non-lock-free).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on injected fault.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t get_lockfree_word_type(
    gcc_jit_context *ctx,
    unsigned long byte_size,
    gcc_jit_type **out_word_type) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    switch (byte_size) {
        case 1UL:
            *out_word_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT8_T);
            break;
        case 2UL:
            *out_word_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT16_T);
            break;
        case 4UL:
            *out_word_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T);
            break;
        case 8UL:
            *out_word_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
            break;
        default:
            *out_word_type = NULL;
            break;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Reinterprets a value of `src_rval` into `dst_type` of identical byte width (`1`, `2`, `4`, or `8`).
 * @details Uses integer/pointer casts when scalar-compatible and stack-slot reinterpretation for
 * floating-point or aggregate representations so bit patterns are preserved without UB.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] cblock Active `gcc_jit_block` for temporary stack reinterpretation when needed.
 * @param[in] src_rval Source rvalue to reinterpret.
 * @param[in] dst_type Destination `gcc_jit_type` of equal byte width.
 * @param[out] out_rval Non-null pointer receiving the reinterpreted rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t reinterpret_atomic_bits(
    gcc_jit_context *ctx,
    gcc_jit_block *cblock,
    gcc_jit_rvalue *src_rval,
    gcc_jit_type *dst_type,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_jit_type *src_type = gcc_jit_rvalue_get_type(src_rval);
    if (src_type == dst_type) {
        *out_rval = src_rval;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_function *fn = gcc_jit_block_get_function(cblock);
    gcc_jit_lvalue *slot = gcc_jit_function_new_local(fn, NULL, src_type, "__atomic_bitcast_slot");
    gcc_jit_block_add_assignment(cblock, NULL, slot, src_rval);
    gcc_jit_rvalue *slot_ptr = gcc_jit_context_new_cast(
        ctx, NULL, gcc_jit_lvalue_get_address(slot, NULL), gcc_jit_type_get_pointer(dst_type));
    *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(slot_ptr, NULL));
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Declares or fetches a cached `libatomic` external runtime function and marks `needs_libatomic`.
 * @details Registers `sym_name` in `GCC_GEN_BE_CACHE_FUNC` so repeated calls in the same TU reuse the
 * single `GCC_JIT_FUNCTION_IMPORTED` declaration.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] sym_name External `libatomic` symbol name (`"__atomic_*"`).
 * @param[in] ret_type Return `gcc_jit_type` of the runtime call.
 * @param[in] num_params Number of parameters in `param_types`.
 * @param[in] param_types Array of parameter `gcc_jit_type *` descriptors.
 * @param[out] out_fn Non-null pointer receiving the imported `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t get_or_declare_libatomic_fn(
    gcc_jit_context *ctx,
    const char *sym_name,
    gcc_jit_type *ret_type,
    int num_params,
    gcc_jit_type **param_types,
    gcc_jit_function **out_fn) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_gen_be_singleton_state.needs_libatomic = TRUE;
    void *cached = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)sym_name, &cached));
    if (cached) {
        *out_fn = (gcc_jit_function *)cached;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_param *params[8];
    for (int i = 0; i < num_params; ++i) {
        char pname[24];
        snprintf(pname, sizeof(pname), "p%d", i);
        params[i] = gcc_jit_context_new_param(ctx, NULL, param_types[i], pname);
    }
    char stub_name[64];
    const char *stem = sym_name + 2;
    snprintf(stub_name, sizeof(stub_name), "__edg_rt_%s", stem);
    char asm_stub[256];
#if defined(__APPLE__) && defined(__aarch64__)
    snprintf(asm_stub, sizeof(asm_stub),
        ".text\n.p2align 2\n_%s:\n b _%s\n", stub_name, sym_name);
#elif defined(__APPLE__) && defined(__x86_64__)
    snprintf(asm_stub, sizeof(asm_stub),
        ".text\n_%s:\n jmp _%s\n", stub_name, sym_name);
#elif defined(__aarch64__)
    snprintf(asm_stub, sizeof(asm_stub),
        ".text\n.p2align 2\n%s:\n b %s\n", stub_name, sym_name);
#elif defined(__x86_64__)
    snprintf(asm_stub, sizeof(asm_stub),
        ".text\n%s:\n jmp %s@PLT\n", stub_name, sym_name);
#else
    snprintf(asm_stub, sizeof(asm_stub),
        ".text\n%s:\n b %s\n", stub_name, sym_name);
#endif
    gcc_jit_context_add_top_level_asm(ctx, NULL, asm_stub);
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, stub_name, num_params, params, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)sym_name, fn));
    *out_fn = fn;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_is_c11_atomic_type(
    a_type_ptr tp,
    a_boolean *out_is_atomic) GCC_GEN_BE_NOEXCEPT {
    if (!out_is_atomic) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_is_atomic = FALSE;
    while (tp && tp->kind == tk_typeref) {
        if ((tp->variant.typeref.qualifiers & TQ_C11_ATOMIC) != 0) {
            *out_is_atomic = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
        tp = tp->variant.typeref.type;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_c11_atomic_load(
    a_type_ptr val_type,
    struct gcc_jit_lvalue *target_lval,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!val_type || !target_lval || !out_rval) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!ctx || !cblock) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }

    a_type_ptr unwrapped = NULL;
    unsigned long sz = 0UL;
    GCC_GEN_BE_CHECK(unwrap_atomic_value_type(val_type, &unwrapped, &sz));

    gcc_jit_type *jit_val_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(unwrapped, &jit_val_type));

    gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_rvalue *raw_addr = gcc_jit_lvalue_get_address(target_lval, NULL);
    gcc_jit_rvalue *seq_cst = gcc_jit_context_new_rvalue_from_int(ctx, int_t, GCC_BE_ATOMIC_SEQ_CST);

    gcc_jit_type *word_t = NULL;
    GCC_GEN_BE_CHECK(get_lockfree_word_type(ctx, sz, &word_t));
    if (word_t) {
        char bname[32];
        snprintf(bname, sizeof(bname), "__atomic_load_%lu", sz);
        gcc_jit_function *bfn = gcc_jit_context_get_builtin_function(ctx, bname);
        gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 0)));
        gcc_jit_rvalue *call_args[2] = {
            gcc_jit_context_new_cast(ctx, NULL, raw_addr, p0_t),
            seq_cst
        };
        gcc_jit_rvalue *loaded_word = gcc_jit_context_new_call(ctx, NULL, bfn, 2, call_args);
        return reinterpret_atomic_bits(ctx, cblock, loaded_word, jit_val_type, out_rval);
    }

    gcc_jit_function *cur_fn = gcc_jit_block_get_function(cblock);
    gcc_jit_lvalue *ret_slot = gcc_jit_function_new_local(cur_fn, NULL, jit_val_type, "__atomic_load_ret");
    gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *ptypes[4] = { size_t_type, void_ptr_t, void_ptr_t, int_t };
    gcc_jit_function *lib_fn = NULL;
    GCC_GEN_BE_CHECK(get_or_declare_libatomic_fn(ctx, "__atomic_load", void_t, 4, ptypes, &lib_fn));

    gcc_jit_rvalue *lib_args[4] = {
        gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)sz),
        gcc_jit_context_new_cast(ctx, NULL, raw_addr, void_ptr_t),
        gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(ret_slot, NULL), void_ptr_t),
        seq_cst
    };
    gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, lib_fn, 4, lib_args));
    *out_rval = gcc_jit_lvalue_as_rvalue(ret_slot);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_c11_atomic_store(
    a_type_ptr val_type,
    struct gcc_jit_lvalue *target_lval,
    struct gcc_jit_rvalue *rhs_rval,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!val_type || !target_lval || !rhs_rval || !out_rval) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!ctx || !cblock) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }

    a_type_ptr unwrapped = NULL;
    unsigned long sz = 0UL;
    GCC_GEN_BE_CHECK(unwrap_atomic_value_type(val_type, &unwrapped, &sz));

    gcc_jit_type *jit_val_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(unwrapped, &jit_val_type));

    gcc_jit_function *cur_fn = gcc_jit_block_get_function(cblock);
    gcc_jit_lvalue *val_slot = gcc_jit_function_new_local(cur_fn, NULL, jit_val_type, "__atomic_store_val");
    gcc_jit_rvalue *coerced_rhs = rhs_rval;
    if (gcc_jit_rvalue_get_type(rhs_rval) != jit_val_type) {
        coerced_rhs = gcc_jit_context_new_cast(ctx, NULL, rhs_rval, jit_val_type);
    }
    gcc_jit_block_add_assignment(cblock, NULL, val_slot, coerced_rhs);

    gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_rvalue *raw_addr = gcc_jit_lvalue_get_address(target_lval, NULL);
    gcc_jit_rvalue *seq_cst = gcc_jit_context_new_rvalue_from_int(ctx, int_t, GCC_BE_ATOMIC_SEQ_CST);

    gcc_jit_type *word_t = NULL;
    GCC_GEN_BE_CHECK(get_lockfree_word_type(ctx, sz, &word_t));
    if (word_t) {
        char bname[32];
        snprintf(bname, sizeof(bname), "__atomic_store_%lu", sz);
        gcc_jit_function *bfn = gcc_jit_context_get_builtin_function(ctx, bname);
        gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 0)));
        gcc_jit_type *p1_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 1)));
        gcc_jit_rvalue *word_val = NULL;
        GCC_GEN_BE_CHECK(reinterpret_atomic_bits(ctx, cblock, gcc_jit_lvalue_as_rvalue(val_slot), p1_t, &word_val));
        gcc_jit_rvalue *call_args[3] = {
            gcc_jit_context_new_cast(ctx, NULL, raw_addr, p0_t),
            word_val,
            seq_cst
        };
        gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, bfn, 3, call_args));
        *out_rval = gcc_jit_lvalue_as_rvalue(val_slot);
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *ptypes[4] = { size_t_type, void_ptr_t, void_ptr_t, int_t };
    gcc_jit_function *lib_fn = NULL;
    GCC_GEN_BE_CHECK(get_or_declare_libatomic_fn(ctx, "__atomic_store", void_t, 4, ptypes, &lib_fn));

    gcc_jit_rvalue *lib_args[4] = {
        gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)sz),
        gcc_jit_context_new_cast(ctx, NULL, raw_addr, void_ptr_t),
        gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(val_slot, NULL), void_ptr_t),
        seq_cst
    };
    gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, lib_fn, 4, lib_args));
    *out_rval = gcc_jit_lvalue_as_rvalue(val_slot);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Maps an EDG compound-assignment or increment/decrement operator to a lock-free `__atomic_*` op name.
 * @details Populates `*out_op_stem` (e.g. `"add"`, `"sub"`, `"and"`, `"or"`, `"xor"`) and `*out_returns_old`
 * (`TRUE` for `eok_post_incr` / `eok_post_decr`), or sets `*out_op_stem = NULL` when a CAS loop is required.
 * @param[in] op Frontend expression operator kind.
 * @param[out] out_op_stem Pointer receiving the builtin op stem (or NULL if CAS required).
 * @param[out] out_bin_op Pointer receiving the fallback `gcc_jit_binary_op` for CAS loops.
 * @param[out] out_returns_old Pointer receiving `TRUE` if the expression yields the pre-RMW value.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_UNSUPPORTED` for non-RMW operators.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t classify_atomic_rmw_op(
    an_expr_operator_kind op,
    const char **out_op_stem,
    enum gcc_jit_binary_op *out_bin_op,
    a_boolean *out_returns_old) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_op_stem = NULL;
    *out_bin_op = GCC_JIT_BINARY_OP_PLUS;
    *out_returns_old = (op == eok_post_incr || op == eok_post_decr) ? TRUE : FALSE;

    switch (op) {
        case eok_add_assign:
        case eok_padd_assign:
        case eok_pre_incr:
        case eok_post_incr:
            *out_op_stem = "add";
            *out_bin_op = GCC_JIT_BINARY_OP_PLUS;
            return GCC_GEN_BE_SUCCESS;
        case eok_subtract_assign:
        case eok_psubtract_assign:
        case eok_pre_decr:
        case eok_post_decr:
            *out_op_stem = "sub";
            *out_bin_op = GCC_JIT_BINARY_OP_MINUS;
            return GCC_GEN_BE_SUCCESS;
        case eok_and_assign:
            *out_op_stem = "and";
            *out_bin_op = GCC_JIT_BINARY_OP_BITWISE_AND;
            return GCC_GEN_BE_SUCCESS;
        case eok_or_assign:
            *out_op_stem = "or";
            *out_bin_op = GCC_JIT_BINARY_OP_BITWISE_OR;
            return GCC_GEN_BE_SUCCESS;
        case eok_xor_assign:
            *out_op_stem = "xor";
            *out_bin_op = GCC_JIT_BINARY_OP_BITWISE_XOR;
            return GCC_GEN_BE_SUCCESS;
        case eok_multiply_assign:
            *out_bin_op = GCC_JIT_BINARY_OP_MULT;
            return GCC_GEN_BE_SUCCESS;
        case eok_divide_assign:
            *out_bin_op = GCC_JIT_BINARY_OP_DIVIDE;
            return GCC_GEN_BE_SUCCESS;
        default:
            return GCC_GEN_BE_ERROR_UNSUPPORTED;
    }
}

gcc_gen_be_error_t gcc_gen_be_lower_c11_atomic_rmw(
    an_expr_operator_kind op,
    a_type_ptr val_type,
    struct gcc_jit_lvalue *target_lval,
    struct gcc_jit_rvalue *rhs_rval,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!val_type || !target_lval || !out_rval) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!ctx || !cblock) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }

    a_type_ptr unwrapped = NULL;
    unsigned long sz = 0UL;
    GCC_GEN_BE_CHECK(unwrap_atomic_value_type(val_type, &unwrapped, &sz));

    const char *op_stem = NULL;
    enum gcc_jit_binary_op bin_op = GCC_JIT_BINARY_OP_PLUS;
    a_boolean returns_old = FALSE;
    GCC_GEN_BE_CHECK(classify_atomic_rmw_op(op, &op_stem, &bin_op, &returns_old));

    gcc_jit_type *jit_val_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(unwrapped, &jit_val_type));
    gcc_jit_type *word_t = NULL;
    GCC_GEN_BE_CHECK(get_lockfree_word_type(ctx, sz, &word_t));
    if (!word_t) {
        return GCC_GEN_BE_ERROR_ATOMIC_ALIGNMENT_FAILED;
    }

    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_rvalue *raw_addr = gcc_jit_lvalue_get_address(target_lval, NULL);
    gcc_jit_rvalue *seq_cst = gcc_jit_context_new_rvalue_from_int(ctx, int_t, GCC_BE_ATOMIC_SEQ_CST);

    if (op_stem && (unwrapped->kind == tk_integer || unwrapped->kind == tk_pointer)) {
        gcc_jit_rvalue *delta_word = NULL;
        if (unwrapped->kind == tk_pointer) {
            unsigned long stride = 1UL;
            a_type_ptr pts_to = unwrapped->variant.pointer.type;
            while (pts_to && pts_to->kind == tk_typeref) pts_to = pts_to->variant.typeref.type;
            if (pts_to && pts_to->size > 0) stride = (unsigned long)pts_to->size;
            gcc_jit_rvalue *count_w = rhs_rval
                ? gcc_jit_context_new_cast(ctx, NULL, rhs_rval, word_t)
                : gcc_jit_context_new_rvalue_from_long(ctx, word_t, 1L);
            gcc_jit_rvalue *stride_w = gcc_jit_context_new_rvalue_from_long(ctx, word_t, (long)stride);
            delta_word = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, word_t, count_w, stride_w);
        } else {
            delta_word = rhs_rval
                ? gcc_jit_context_new_cast(ctx, NULL, rhs_rval, word_t)
                : gcc_jit_context_new_rvalue_from_long(ctx, word_t, 1L);
        }

        char bname[48];
        if (returns_old) {
            snprintf(bname, sizeof(bname), "__atomic_fetch_%s_%lu", op_stem, sz);
        } else {
            snprintf(bname, sizeof(bname), "__atomic_%s_fetch_%lu", op_stem, sz);
        }
        gcc_jit_function *bfn = gcc_jit_context_get_builtin_function(ctx, bname);
        gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 0)));
        gcc_jit_type *p1_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 1)));
        delta_word = gcc_jit_context_new_cast(ctx, NULL, delta_word, p1_t);
        gcc_jit_rvalue *rmw_args[3] = {
            gcc_jit_context_new_cast(ctx, NULL, raw_addr, p0_t),
            delta_word,
            seq_cst
        };
        gcc_jit_rvalue *res_word = gcc_jit_context_new_call(ctx, NULL, bfn, 3, rmw_args);
        return reinterpret_atomic_bits(ctx, cblock, res_word, jit_val_type, out_rval);
    }

    /* Floating-point or multiply/divide RMW: atomic compare-exchange loop */
    gcc_jit_function *cur_fn = gcc_jit_block_get_function(cblock);
    gcc_jit_rvalue *rhs_coerced = rhs_rval
        ? ((gcc_jit_rvalue_get_type(rhs_rval) == jit_val_type)
            ? rhs_rval
            : gcc_jit_context_new_cast(ctx, NULL, rhs_rval, jit_val_type))
        : gcc_jit_context_one(ctx, jit_val_type);
    gcc_jit_lvalue *rhs_slot = gcc_jit_function_new_local(cur_fn, NULL, jit_val_type, "__atomic_rmw_rhs");
    gcc_jit_block_add_assignment(cblock, NULL, rhs_slot, rhs_coerced);

    char load_name[32];
    snprintf(load_name, sizeof(load_name), "__atomic_load_%lu", sz);
    gcc_jit_function *load_fn = gcc_jit_context_get_builtin_function(ctx, load_name);
    gcc_jit_type *ld_p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(load_fn, 0)));
    char cas_name[48];
    snprintf(cas_name, sizeof(cas_name), "__atomic_compare_exchange_%lu", sz);
    gcc_jit_function *cas_fn = gcc_jit_context_get_builtin_function(ctx, cas_name);
    gcc_jit_type *cas_p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(cas_fn, 0)));
    gcc_jit_type *cas_p1_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(cas_fn, 1)));
    gcc_jit_type *cas_word_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(cas_fn, 2)));

    gcc_jit_lvalue *exp_word_slot = gcc_jit_function_new_local(cur_fn, NULL, cas_word_t, "__atomic_cas_exp");
    gcc_jit_lvalue *old_val_slot = gcc_jit_function_new_local(cur_fn, NULL, jit_val_type, "__atomic_cas_old");
    gcc_jit_lvalue *new_val_slot = gcc_jit_function_new_local(cur_fn, NULL, jit_val_type, "__atomic_cas_new");

    gcc_jit_rvalue *ld_args[2] = { gcc_jit_context_new_cast(ctx, NULL, raw_addr, ld_p0_t), seq_cst };
    gcc_jit_rvalue *ld_rv = gcc_jit_context_new_call(ctx, NULL, load_fn, 2, ld_args);
    gcc_jit_block_add_assignment(cblock, NULL, exp_word_slot, ld_rv);

    gcc_jit_block *loop_blk = gcc_jit_function_new_block(cur_fn, "atomic_rmw_cas_loop");
    gcc_jit_block *done_blk = gcc_jit_function_new_block(cur_fn, "atomic_rmw_cas_done");
    gcc_jit_block_end_with_jump(cblock, NULL, loop_blk);

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(loop_blk));
    gcc_jit_rvalue *cur_val = NULL;
    GCC_GEN_BE_CHECK(reinterpret_atomic_bits(ctx, loop_blk, gcc_jit_lvalue_as_rvalue(exp_word_slot), jit_val_type, &cur_val));
    gcc_jit_block_add_assignment(loop_blk, NULL, old_val_slot, cur_val);
    gcc_jit_rvalue *computed = gcc_jit_context_new_binary_op(
        ctx, NULL, bin_op, jit_val_type, gcc_jit_lvalue_as_rvalue(old_val_slot), gcc_jit_lvalue_as_rvalue(rhs_slot));
    gcc_jit_block_add_assignment(loop_blk, NULL, new_val_slot, computed);

    gcc_jit_rvalue *des_word = NULL;
    GCC_GEN_BE_CHECK(reinterpret_atomic_bits(ctx, loop_blk, gcc_jit_lvalue_as_rvalue(new_val_slot), cas_word_t, &des_word));

    gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
    gcc_jit_rvalue *cas_args[6] = {
        gcc_jit_context_new_cast(ctx, NULL, raw_addr, cas_p0_t),
        gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(exp_word_slot, NULL), cas_p1_t),
        des_word,
        gcc_jit_context_zero(ctx, bool_t),
        seq_cst,
        seq_cst
    };
    gcc_jit_rvalue *ok_rval = gcc_jit_context_new_call(ctx, NULL, cas_fn, 6, cas_args);
    gcc_jit_block_end_with_conditional(loop_blk, NULL, ok_rval, done_blk, loop_blk);

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(done_blk));
    *out_rval = returns_old ? gcc_jit_lvalue_as_rvalue(old_val_slot) : gcc_jit_lvalue_as_rvalue(new_val_slot);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Infers the pointee byte width from an EDG routine signature when `explicit_size_bytes == 0`.
 * @details Inspects the first pointer parameter in `rout->type->variant.routine.extra_info->param_type_list`.
 * @param[in] rout Frontend routine descriptor (may be NULL).
 * @param[in] has_size_prefix `TRUE` when parameter 0 is the front-end-inserted `size_t` parameter.
 * @param[out] out_size Pointer receiving the inferred pointee byte width (`0UL` when unknown).
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t infer_routine_pointee_size(
    a_routine_ptr rout,
    a_boolean has_size_prefix,
    unsigned long *out_size) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_size = 0UL;
    if (!rout || !rout->type || rout->type->kind != tk_routine || !rout->type->variant.routine.extra_info) {
        return GCC_GEN_BE_SUCCESS;
    }
    a_param_type_ptr p = rout->type->variant.routine.extra_info->param_type_list;
    if (has_size_prefix && p) {
        p = p->next;
    }
    if (!p || !p->type) {
        return GCC_GEN_BE_SUCCESS;
    }
    a_type_ptr pt = p->type;
    while (pt && pt->kind == tk_typeref) pt = pt->variant.typeref.type;
    if (!pt || pt->kind != tk_pointer || !pt->variant.pointer.type) {
        return GCC_GEN_BE_SUCCESS;
    }
    a_type_ptr elem = pt->variant.pointer.type;
    while (elem && elem->kind == tk_typeref) elem = elem->variant.typeref.type;
    *out_size = elem ? (unsigned long)elem->size : 0UL;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_generic_atomic_call(
    const char *name,
    a_routine_ptr rout,
    int num_args,
    struct gcc_jit_rvalue **args,
    unsigned long explicit_size_bytes,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!name || num_args < 3 || !args || !out_rval) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!ctx) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }

    gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);

    if (strcmp(name, "__atomic_load") == 0) {
        a_boolean has_sz = (num_args == 4) ? TRUE : FALSE;
        if (!has_sz && num_args != 3) return GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH;
        unsigned long sz = explicit_size_bytes;
        if (sz == 0UL) {
            GCC_GEN_BE_CHECK(infer_routine_pointee_size(rout, has_sz, &sz));
        }
        gcc_jit_rvalue *ptr_arg = args[has_sz ? 1 : 0];
        gcc_jit_rvalue *ret_arg = args[has_sz ? 2 : 1];
        gcc_jit_rvalue *mod_arg = gcc_jit_context_new_cast(ctx, NULL, args[has_sz ? 3 : 2], int_t);

        gcc_jit_type *word_t = NULL;
        GCC_GEN_BE_CHECK(get_lockfree_word_type(ctx, sz, &word_t));
        if (word_t && cblock) {
            char bname[32];
            snprintf(bname, sizeof(bname), "__atomic_load_%lu", sz);
            gcc_jit_function *bfn = gcc_jit_context_get_builtin_function(ctx, bname);
            gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 0)));
            gcc_jit_rvalue *bargs[2] = { gcc_jit_context_new_cast(ctx, NULL, ptr_arg, p0_t), mod_arg };
            gcc_jit_rvalue *wval = gcc_jit_context_new_call(ctx, NULL, bfn, 2, bargs);
            gcc_jit_type *ret_wt = gcc_jit_rvalue_get_type(wval);
            gcc_jit_rvalue *dst_ptr = gcc_jit_context_new_cast(ctx, NULL, ret_arg, gcc_jit_type_get_pointer(ret_wt));
            gcc_jit_block_add_assignment(cblock, NULL, gcc_jit_rvalue_dereference(dst_ptr, NULL), wval);
            *out_rval = gcc_jit_context_zero(ctx, int_t);
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_rvalue *sz_rval = has_sz
            ? gcc_jit_context_new_cast(ctx, NULL, args[0], size_t_type)
            : gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)(sz > 0 ? sz : 4UL));
        gcc_jit_type *ptypes[4] = { size_t_type, void_ptr_t, void_ptr_t, int_t };
        gcc_jit_function *lib_fn = NULL;
        GCC_GEN_BE_CHECK(get_or_declare_libatomic_fn(ctx, "__atomic_load", void_t, 4, ptypes, &lib_fn));
        gcc_jit_rvalue *largs[4] = {
            sz_rval,
            gcc_jit_context_new_cast(ctx, NULL, ptr_arg, void_ptr_t),
            gcc_jit_context_new_cast(ctx, NULL, ret_arg, void_ptr_t),
            mod_arg
        };
        *out_rval = gcc_jit_context_new_call(ctx, NULL, lib_fn, 4, largs);
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__atomic_store") == 0) {
        a_boolean has_sz = (num_args == 4) ? TRUE : FALSE;
        if (!has_sz && num_args != 3) return GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH;
        unsigned long sz = explicit_size_bytes;
        if (sz == 0UL) {
            GCC_GEN_BE_CHECK(infer_routine_pointee_size(rout, has_sz, &sz));
        }
        gcc_jit_rvalue *ptr_arg = args[has_sz ? 1 : 0];
        gcc_jit_rvalue *val_arg = args[has_sz ? 2 : 1];
        gcc_jit_rvalue *mod_arg = gcc_jit_context_new_cast(ctx, NULL, args[has_sz ? 3 : 2], int_t);

        gcc_jit_type *word_t = NULL;
        GCC_GEN_BE_CHECK(get_lockfree_word_type(ctx, sz, &word_t));
        if (word_t) {
            char bname[32];
            snprintf(bname, sizeof(bname), "__atomic_store_%lu", sz);
            gcc_jit_function *bfn = gcc_jit_context_get_builtin_function(ctx, bname);
            gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 0)));
            gcc_jit_type *p1_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 1)));
            gcc_jit_rvalue *src_ptr = gcc_jit_context_new_cast(ctx, NULL, val_arg, gcc_jit_type_get_pointer(p1_t));
            gcc_jit_rvalue *wval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(src_ptr, NULL));
            gcc_jit_rvalue *bargs[3] = { gcc_jit_context_new_cast(ctx, NULL, ptr_arg, p0_t), wval, mod_arg };
            *out_rval = gcc_jit_context_new_call(ctx, NULL, bfn, 3, bargs);
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_rvalue *sz_rval = has_sz
            ? gcc_jit_context_new_cast(ctx, NULL, args[0], size_t_type)
            : gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)(sz > 0 ? sz : 4UL));
        gcc_jit_type *ptypes[4] = { size_t_type, void_ptr_t, void_ptr_t, int_t };
        gcc_jit_function *lib_fn = NULL;
        GCC_GEN_BE_CHECK(get_or_declare_libatomic_fn(ctx, "__atomic_store", void_t, 4, ptypes, &lib_fn));
        gcc_jit_rvalue *largs[4] = {
            sz_rval,
            gcc_jit_context_new_cast(ctx, NULL, ptr_arg, void_ptr_t),
            gcc_jit_context_new_cast(ctx, NULL, val_arg, void_ptr_t),
            mod_arg
        };
        *out_rval = gcc_jit_context_new_call(ctx, NULL, lib_fn, 4, largs);
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__atomic_exchange") == 0) {
        a_boolean has_sz = (num_args == 5) ? TRUE : FALSE;
        if (!has_sz && num_args != 4) return GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH;
        unsigned long sz = explicit_size_bytes;
        if (sz == 0UL) {
            GCC_GEN_BE_CHECK(infer_routine_pointee_size(rout, has_sz, &sz));
        }
        gcc_jit_rvalue *ptr_arg = args[has_sz ? 1 : 0];
        gcc_jit_rvalue *val_arg = args[has_sz ? 2 : 1];
        gcc_jit_rvalue *ret_arg = args[has_sz ? 3 : 2];
        gcc_jit_rvalue *mod_arg = gcc_jit_context_new_cast(ctx, NULL, args[has_sz ? 4 : 3], int_t);

        gcc_jit_type *word_t = NULL;
        GCC_GEN_BE_CHECK(get_lockfree_word_type(ctx, sz, &word_t));
        if (word_t && cblock) {
            char bname[32];
            snprintf(bname, sizeof(bname), "__atomic_exchange_%lu", sz);
            gcc_jit_function *bfn = gcc_jit_context_get_builtin_function(ctx, bname);
            gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 0)));
            gcc_jit_type *p1_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 1)));
            gcc_jit_rvalue *src_ptr = gcc_jit_context_new_cast(ctx, NULL, val_arg, gcc_jit_type_get_pointer(p1_t));
            gcc_jit_rvalue *wval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(src_ptr, NULL));
            gcc_jit_rvalue *bargs[3] = { gcc_jit_context_new_cast(ctx, NULL, ptr_arg, p0_t), wval, mod_arg };
            gcc_jit_rvalue *old_w = gcc_jit_context_new_call(ctx, NULL, bfn, 3, bargs);
            gcc_jit_type *ret_wt = gcc_jit_rvalue_get_type(old_w);
            gcc_jit_rvalue *dst_ptr = gcc_jit_context_new_cast(ctx, NULL, ret_arg, gcc_jit_type_get_pointer(ret_wt));
            gcc_jit_block_add_assignment(cblock, NULL, gcc_jit_rvalue_dereference(dst_ptr, NULL), old_w);
            *out_rval = gcc_jit_context_zero(ctx, int_t);
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_rvalue *sz_rval = has_sz
            ? gcc_jit_context_new_cast(ctx, NULL, args[0], size_t_type)
            : gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)(sz > 0 ? sz : 4UL));
        gcc_jit_type *ptypes[5] = { size_t_type, void_ptr_t, void_ptr_t, void_ptr_t, int_t };
        gcc_jit_function *lib_fn = NULL;
        GCC_GEN_BE_CHECK(get_or_declare_libatomic_fn(ctx, "__atomic_exchange", void_t, 5, ptypes, &lib_fn));
        gcc_jit_rvalue *largs[5] = {
            sz_rval,
            gcc_jit_context_new_cast(ctx, NULL, ptr_arg, void_ptr_t),
            gcc_jit_context_new_cast(ctx, NULL, val_arg, void_ptr_t),
            gcc_jit_context_new_cast(ctx, NULL, ret_arg, void_ptr_t),
            mod_arg
        };
        *out_rval = gcc_jit_context_new_call(ctx, NULL, lib_fn, 5, largs);
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__atomic_compare_exchange") == 0 || strcmp(name, "__atomic_compare_exchange_n") == 0) {
        a_boolean is_by_val = (strcmp(name, "__atomic_compare_exchange_n") == 0) ? TRUE : FALSE;
        a_boolean has_sz = (num_args == 7) ? TRUE : FALSE;
        if (!has_sz && num_args != 6) return GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH;
        unsigned long sz = explicit_size_bytes;
        if (sz == 0UL) {
            GCC_GEN_BE_CHECK(infer_routine_pointee_size(rout, has_sz, &sz));
        }
        if (sz == 0UL && is_by_val) {
            sz = 4UL;
        }
        gcc_jit_rvalue *ptr_arg = args[has_sz ? 1 : 0];
        gcc_jit_rvalue *exp_arg = args[has_sz ? 2 : 1];
        gcc_jit_rvalue *des_arg = args[has_sz ? 3 : 2];
        gcc_jit_rvalue *weak_arg = gcc_jit_context_new_cast(ctx, NULL, args[has_sz ? 4 : 3], bool_t);
        gcc_jit_rvalue *succ_arg = gcc_jit_context_new_cast(ctx, NULL, args[has_sz ? 5 : 4], int_t);
        gcc_jit_rvalue *fail_arg = gcc_jit_context_new_cast(ctx, NULL, args[has_sz ? 6 : 5], int_t);

        gcc_jit_type *word_t = NULL;
        GCC_GEN_BE_CHECK(get_lockfree_word_type(ctx, sz, &word_t));
        if (word_t && (!is_by_val || cblock)) {
            char bname[48];
            snprintf(bname, sizeof(bname), "__atomic_compare_exchange_%lu", sz);
            gcc_jit_function *bfn = gcc_jit_context_get_builtin_function(ctx, bname);
            gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 0)));
            gcc_jit_type *p1_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 1)));
            gcc_jit_type *p2_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(bfn, 2)));
            gcc_jit_rvalue *des_word = NULL;
            if (is_by_val) {
                GCC_GEN_BE_CHECK(reinterpret_atomic_bits(ctx, cblock, des_arg, p2_t, &des_word));
            } else {
                gcc_jit_rvalue *des_ptr = gcc_jit_context_new_cast(ctx, NULL, des_arg, gcc_jit_type_get_pointer(p2_t));
                des_word = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(des_ptr, NULL));
            }
            gcc_jit_rvalue *bargs[6] = {
                gcc_jit_context_new_cast(ctx, NULL, ptr_arg, p0_t),
                gcc_jit_context_new_cast(ctx, NULL, exp_arg, p1_t),
                des_word,
                weak_arg,
                succ_arg,
                fail_arg
            };
            *out_rval = gcc_jit_context_new_call(ctx, NULL, bfn, 6, bargs);
            return GCC_GEN_BE_SUCCESS;
        }

        gcc_jit_rvalue *des_ptr_void = NULL;
        if (is_by_val) {
            if (!cblock) return GCC_GEN_BE_ERROR_INVALID_STATE;
            gcc_jit_function *cur_fn = gcc_jit_block_get_function(cblock);
            gcc_jit_lvalue *des_slot = gcc_jit_function_new_local(
                cur_fn, NULL, gcc_jit_rvalue_get_type(des_arg), "__atomic_cas_des_slot");
            gcc_jit_block_add_assignment(cblock, NULL, des_slot, des_arg);
            des_ptr_void = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(des_slot, NULL), void_ptr_t);
        } else {
            des_ptr_void = gcc_jit_context_new_cast(ctx, NULL, des_arg, void_ptr_t);
        }

        gcc_jit_rvalue *sz_rval = has_sz
            ? gcc_jit_context_new_cast(ctx, NULL, args[0], size_t_type)
            : gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)(sz > 0 ? sz : 16UL));
        gcc_jit_type *ptypes[6] = { size_t_type, void_ptr_t, void_ptr_t, void_ptr_t, int_t, int_t };
        gcc_jit_function *lib_fn = NULL;
        GCC_GEN_BE_CHECK(get_or_declare_libatomic_fn(ctx, "__atomic_compare_exchange", bool_t, 6, ptypes, &lib_fn));
        gcc_jit_rvalue *largs[6] = {
            sz_rval,
            gcc_jit_context_new_cast(ctx, NULL, ptr_arg, void_ptr_t),
            gcc_jit_context_new_cast(ctx, NULL, exp_arg, void_ptr_t),
            des_ptr_void,
            succ_arg,
            fail_arg
        };
        *out_rval = gcc_jit_context_new_call(ctx, NULL, lib_fn, 6, largs);
        return GCC_GEN_BE_SUCCESS;
    }

    return GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
