/**
 * @file gcc_gen_be_builtin.c
 * @brief Implementation of Target SIMD Vector Intrinsics & GCC Builtins.
 *
 * This file maps compiler builtins (atomic, math, bitwise, overflow) to
 * libgccjit function calls or native operations. It also translates
 * x86 and ARM SIMD vector intrinsics into generic vector types and operations,
 * and lowers AddressSanitizer and UBSan instrumentation hooks.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_lib_loader.h"
#include <string.h>
#include <stdio.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Helper to resolve and cache a builtin function.
 *
 * @param ctx The libgccjit context.
 * @param name The builtin name (e.g. "__atomic_load_n").
 * @param ret_type The return type.
 * @param num_params Number of parameters.
 * @param param_types Array of parameter types.
 * @param is_variadic Whether it accepts variadic arguments.
 * @param[out] out_func Pointer to receive the JIT function.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t get_builtin(gcc_jit_context *ctx, const char *name, gcc_jit_type *ret_type, int num_params, gcc_jit_type **param_types, int is_variadic, gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_func = NULL;
    
    if (p_gcc_jit_context_get_builtin_function &&
        (strncmp(name, "__builtin_", 10) == 0 ||
         strncmp(name, "__sync_", 7) == 0 ||
         strncmp(name, "__atomic_", 9) == 0)) {
        gcc_jit_function *builtin = p_gcc_jit_context_get_builtin_function(ctx, name);
        if (builtin) {
            *out_func = builtin;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    
    /* Fallback to importing standard functions (e.g., libatomic, libm) */
    gcc_jit_param **params = NULL;
    if (num_params > 0) {
        params = (gcc_jit_param **)malloc((size_t)num_params * sizeof(gcc_jit_param *));
        for (int i = 0; i < num_params; i++) {
            params[i] = gcc_jit_context_new_param(ctx, NULL, param_types[i], "p");
        }
    }
    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, name, num_params, params, is_variadic);
    if (params) free(params);
    *out_func = fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers atomic memory builtins (__atomic_load_n, etc.).
 *
 * @details Resolves the atomic builtin function and emits a call with the lowered arguments.
 *
 * @param[in] name The builtin name.
 * @param[in] rout The routine pointer.
 * @param[in] num_args Number of arguments.
 * @param[in] args Array of lowered argument rvalues.
 * @param[out] out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_atomic_builtin(const char *name, a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
    if (strcmp(name, "__atomic_always_lock_free") == 0 || strcmp(name, "__atomic_is_lock_free") == 0) {
        int is_lf = 1;
        if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info &&
            rout->type->variant.routine.extra_info->param_type_list) {
            a_param_type_ptr p0 = rout->type->variant.routine.extra_info->param_type_list;
            a_param_type_ptr p1 = p0 ? p0->next : NULL;
            if (p1 && p1->type) {
                a_type_ptr pt = p1->type;
                while (pt && pt->kind == tk_typeref) pt = pt->variant.typeref.type;
                if (pt && pt->kind == tk_pointer && pt->variant.pointer.type) {
                    long ptsz = (long)size_of_type(pt->variant.pointer.type);
                    if (ptsz > 0 && ptsz != 1 && ptsz != 2 && ptsz != 4 && ptsz != 8 && ptsz != 16) {
                        is_lf = 0;
                    }
                }
            }
        }
        *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, bool_type, is_lf);
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_type *ret_type = NULL;
    if (strcmp(name, "__atomic_clear") == 0 || strcmp(name, "__sync_synchronize") == 0 ||
        strcmp(name, "__atomic_thread_fence") == 0 || strcmp(name, "__atomic_signal_fence") == 0) {
        ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    } else if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.return_type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(rout->type->variant.routine.return_type, &ret_type));
    } else {
        ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    }

    /* Validate atomic operand power-of-two size and natural alignment for _n intrinsics */
    a_boolean is_generic_libatomic_call = (
        strcmp(name, "__atomic_load") == 0 ||
        strcmp(name, "__atomic_store") == 0 ||
        strcmp(name, "__atomic_exchange") == 0 ||
        strcmp(name, "__atomic_compare_exchange") == 0) ? TRUE : FALSE;
    if (!is_generic_libatomic_call && rout->type && rout->type->kind == tk_routine) {
        if (rout->type->variant.routine.return_type) {
            long ret_sz = (long)size_of_type(rout->type->variant.routine.return_type);
            if (ret_sz > 0 && ret_sz != 1 && ret_sz != 2 && ret_sz != 4 && ret_sz != 8 && ret_sz != 16) {
                return GCC_GEN_BE_ERROR_ATOMIC_ALIGNMENT_FAILED;
            }
        }
        if (rout->type->variant.routine.extra_info &&
            rout->type->variant.routine.extra_info->param_type_list) {
            a_type_ptr p0 = rout->type->variant.routine.extra_info->param_type_list->type;
            while (p0 && p0->kind == tk_typeref) p0 = p0->variant.typeref.type;
            if (p0 && p0->kind == tk_pointer && p0->variant.pointer.type) {
                a_type_ptr elem_tp = p0->variant.pointer.type;
                while (elem_tp && elem_tp->kind == tk_typeref) elem_tp = elem_tp->variant.typeref.type;
                long elem_sz = elem_tp ? (long)size_of_type(elem_tp) : 0;
                if (elem_sz > 0 && elem_sz != 1 && elem_sz != 2 && elem_sz != 4 && elem_sz != 8 && elem_sz != 16) {
                    return GCC_GEN_BE_ERROR_ATOMIC_ALIGNMENT_FAILED;
                }
            }
        }
    }

    /* Map generic __atomic_load_n / __atomic_store_n / __atomic_exchange_n / __atomic_compare_exchange_n / __atomic_fetch_* on 16-byte (__int128) operands to sized _16 builtins */
    char sized_atomic_name[128];
    const char *resolved_name = name;
    a_boolean is_16byte_atomic = FALSE;
    if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.return_type &&
        size_of_type(rout->type->variant.routine.return_type) == 16) {
        is_16byte_atomic = TRUE;
    } else if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info &&
               rout->type->variant.routine.extra_info->param_type_list) {
        a_type_ptr p0 = rout->type->variant.routine.extra_info->param_type_list->type;
        while (p0 && p0->kind == tk_typeref) p0 = p0->variant.typeref.type;
        if (p0 && p0->kind == tk_pointer && p0->variant.pointer.type &&
            size_of_type(p0->variant.pointer.type) == 16) {
            is_16byte_atomic = TRUE;
        }
    }
    if (is_16byte_atomic) {
        if (strcmp(name, "__atomic_load_n") == 0 || strcmp(name, "__atomic_load") == 0) {
            snprintf(sized_atomic_name, sizeof(sized_atomic_name), "__atomic_load_16");
            resolved_name = sized_atomic_name;
        } else if (strcmp(name, "__atomic_store_n") == 0 || strcmp(name, "__atomic_store") == 0) {
            snprintf(sized_atomic_name, sizeof(sized_atomic_name), "__atomic_store_16");
            resolved_name = sized_atomic_name;
        } else if (strcmp(name, "__atomic_exchange_n") == 0 || strcmp(name, "__atomic_exchange") == 0) {
            snprintf(sized_atomic_name, sizeof(sized_atomic_name), "__atomic_exchange_16");
            resolved_name = sized_atomic_name;
        } else if (strcmp(name, "__atomic_compare_exchange_n") == 0 || strcmp(name, "__atomic_compare_exchange") == 0) {
            snprintf(sized_atomic_name, sizeof(sized_atomic_name), "__atomic_compare_exchange_16");
            resolved_name = sized_atomic_name;
        } else if (strncmp(name, "__atomic_fetch_", 15) == 0 && strstr(name, "_16") == NULL) {
            snprintf(sized_atomic_name, sizeof(sized_atomic_name), "%s_16", name);
            resolved_name = sized_atomic_name;
        } else if ((strncmp(name, "__atomic_add_fetch", 18) == 0 ||
                    strncmp(name, "__atomic_sub_fetch", 18) == 0 ||
                    strncmp(name, "__atomic_and_fetch", 18) == 0 ||
                    strncmp(name, "__atomic_xor_fetch", 18) == 0 ||
                    strncmp(name, "__atomic_or_fetch", 17) == 0 ||
                    strncmp(name, "__atomic_nand_fetch", 19) == 0) && strstr(name, "_16") == NULL) {
            snprintf(sized_atomic_name, sizeof(sized_atomic_name), "%s_16", name);
            resolved_name = sized_atomic_name;
        }
    }

    int safe_args = num_args < 0 ? 0 : num_args;
    gcc_jit_type **arg_types = NULL;
    if (safe_args > 0) {
        arg_types = (gcc_jit_type **)malloc((size_t)safe_args * sizeof(gcc_jit_type *));
        if (!arg_types) return GCC_GEN_BE_ERROR_OOM;
        for (int i = 0; i < safe_args; ++i) {
            arg_types[i] = args && args[i] ? gcc_jit_rvalue_get_type(args[i]) : gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        }
    }
    
    gcc_jit_function *fn = NULL;
    gcc_gen_be_error_t err = GCC_GEN_BE_SUCCESS;
    if (resolved_name != name) {
        gcc_jit_param **params = NULL;
        if (safe_args > 0) {
            params = (gcc_jit_param **)malloc((size_t)safe_args * sizeof(gcc_jit_param *));
            if (!params) {
                free(arg_types);
                return GCC_GEN_BE_ERROR_OOM;
            }
            for (int i = 0; i < safe_args; ++i) {
                char pname[32];
                snprintf(pname, sizeof(pname), "arg_%d", i);
                params[i] = gcc_jit_context_new_param(ctx, NULL, arg_types[i], pname);
            }
        }
        fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, resolved_name, safe_args, params, 0);
        if (params) free(params);
        if (arg_types) free(arg_types);
    } else {
        err = get_builtin(ctx, resolved_name, ret_type, safe_args, arg_types, 0, &fn);
        if (arg_types) free(arg_types);
        if (err != GCC_GEN_BE_SUCCESS) return err;
    }
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, safe_args, args);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers `__builtin_bit_cast(T, src)` to a bit-preserving reinterpretation.
 * @details Validates that `dst_type` and `src_type` have identical byte sizes and bitcasts `src_val` to the lowered destination type.
 * @param[in] dst_type Target EDG type.
 * @param[in] src_type Source EDG type.
 * @param[in] src_val Lowered source rvalue.
 * @param[out] out_rval Pointer to receive the bitcast rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_TYPE_MISMATCH if sizes differ.
 */
gcc_gen_be_error_t gcc_gen_be_lower_builtin_bit_cast(
    a_type_ptr dst_type,
    a_type_ptr src_type,
    gcc_jit_rvalue *src_val,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!src_val || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    if (dst_type && src_type && size_of_type(dst_type) != size_of_type(src_type)) {
        return GCC_GEN_BE_ERROR_TYPE_MISMATCH;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;
    gcc_jit_type *src_t = gcc_jit_rvalue_get_type(src_val);
    gcc_jit_type *dst_t = src_t;
    if (dst_type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(dst_type, &dst_t));
    }
    if (!dst_t || dst_t == src_t) {
        *out_rval = src_val;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (cblock && gcc_jit_type_get_size(src_t) > 0 && gcc_jit_type_get_size(dst_t) > 0) {
        gcc_jit_function *func = gcc_jit_block_get_function(cblock);
        gcc_jit_lvalue *src_tmp = gcc_jit_function_new_local(func, NULL, src_t, "bitcast_src");
        gcc_jit_block_add_assignment(cblock, NULL, src_tmp, src_val);
        gcc_jit_rvalue *src_addr = gcc_jit_lvalue_get_address(src_tmp, NULL);
        gcc_jit_rvalue *dst_ptr = gcc_jit_context_new_cast(ctx, NULL, src_addr, gcc_jit_type_get_pointer(dst_t));
        *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(dst_ptr, NULL));
    } else if (p_gcc_jit_context_new_bitcast &&
               dst_type && (dst_type->kind == tk_integer || dst_type->kind == tk_float || dst_type->kind == tk_pointer) &&
               gcc_jit_type_get_size(src_t) == gcc_jit_type_get_size(dst_t) &&
               gcc_jit_type_get_size(src_t) > 0) {
        *out_rval = p_gcc_jit_context_new_bitcast(ctx, NULL, src_val, dst_t);
    } else {
        *out_rval = src_val;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_validate_simd_immediate(
    const char *intrinsic_name,
    an_expr_node_ptr imm_expr,
    int min_val,
    int max_val,
    a_boolean *out_is_valid) GCC_GEN_BE_NOEXCEPT {
    if (!intrinsic_name || !out_is_valid) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_is_valid = FALSE;
    if (!imm_expr || imm_expr->kind != enk_constant || !imm_expr->variant.constant.ptr ||
        imm_expr->variant.constant.ptr->kind != ck_integer) {
        return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED, intrinsic_name);
    }
    long val = (long)imm_expr->variant.constant.ptr->variant.integer_value;
    if (val < (long)min_val || val > (long)max_val) {
        return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED, intrinsic_name);
    }
    *out_is_valid = TRUE;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_eval_builtin_object_size(
    a_type_ptr ptr_type,
    int bos_mode,
    size_t *out_bound) GCC_GEN_BE_NOEXCEPT {
    if (!out_bound) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_bound = ((bos_mode & 2) == 0) ? (size_t)-1 : 0U;

    a_type_ptr t = ptr_type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (t && t->kind == tk_pointer && t->variant.pointer.type) {
        a_type_ptr pointee = t->variant.pointer.type;
        while (pointee && pointee->kind == tk_typeref) pointee = pointee->variant.typeref.type;
        if (pointee && (pointee->kind == tk_struct || pointee->kind == tk_class ||
                        pointee->kind == tk_union || pointee->kind == tk_array)) {
            long sz = (long)size_of_type(pointee);
            if (sz > 0) {
                *out_bound = (size_t)sz;
            }
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_builtin_clear_padding(
    a_type_ptr ptr_type,
    gcc_jit_rvalue *ptr_val,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!ptr_val || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));

    a_type_ptr t = ptr_type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    a_type_ptr agg = (t && t->kind == tk_pointer) ? t->variant.pointer.type : t;
    while (agg && agg->kind == tk_typeref) agg = agg->variant.typeref.type;

    if (cblock && agg && (agg->kind == tk_struct || agg->kind == tk_class) && size_of_type(agg) > 0) {
        gcc_jit_type *uchar_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
        gcc_jit_type *uchar_ptr_t = gcc_jit_type_get_pointer(uchar_t);
        gcc_jit_type *size_t_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
        gcc_jit_rvalue *byte_ptr = gcc_jit_context_new_cast(ctx, NULL, ptr_val, uchar_ptr_t);
        gcc_jit_rvalue *zero_byte = gcc_jit_context_zero(ctx, uchar_t);

        long prev_end = 0;
        long total_sz = (long)size_of_type(agg);
        for (a_field_ptr f = agg->variant.class_struct_union.field_list; f != NULL; f = f->next) {
            long f_off = (long)f->offset;
            for (long p = prev_end; p < f_off && p < total_sz; ++p) {
                gcc_jit_rvalue *idx_rv = gcc_jit_context_new_rvalue_from_long(ctx, size_t_t, p);
                gcc_jit_lvalue *pad_lv = gcc_jit_context_new_array_access(ctx, NULL, byte_ptr, idx_rv);
                gcc_jit_block_add_assignment(cblock, NULL, pad_lv, zero_byte);
            }
            long f_sz = f->type ? (long)size_of_type(f->type) : 0;
            if (f_off + f_sz > prev_end) prev_end = f_off + f_sz;
        }
        for (long p = prev_end; p < total_sz; ++p) {
            gcc_jit_rvalue *idx_rv = gcc_jit_context_new_rvalue_from_long(ctx, size_t_t, p);
            gcc_jit_lvalue *pad_lv = gcc_jit_context_new_array_access(ctx, NULL, byte_ptr, idx_rv);
            gcc_jit_block_add_assignment(cblock, NULL, pad_lv, zero_byte);
        }
    }

    *out_rval = ptr_val;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers math, bitwise, and overflow builtins.
 *
 * @details Resolves standard GCC math/bitwise/overflow builtins and emits a call
 * supporting arbitrary argument counts without clamping.
 *
 * @param[in] name The builtin name.
 * @param[in] rout The routine pointer.
 * @param[in] num_args Number of arguments.
 * @param[in] args Array of lowered argument rvalues.
 * @param[out] out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_math_bitwise_builtin(const char *name, a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    if (strcmp(name, "__builtin_unreachable") == 0 || strcmp(name, "__builtin_trap") == 0) {
        gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_function *fn_unreach = NULL;
        GCC_GEN_BE_CHECK(get_builtin(ctx, name, void_t, 0, NULL, 0, &fn_unreach));
        *out_rval = gcc_jit_context_new_call(ctx, NULL, fn_unreach, 0, NULL);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_launder") == 0 || strcmp(name, "__builtin_assume_aligned") == 0 ||
        strcmp(name, "__builtin_expect") == 0 || strcmp(name, "__builtin_expect_with_probability") == 0 ||
        strcmp(name, "__builtin_extract_return_addr") == 0) {
        if (num_args > 0 && args && args[0]) {
            *out_rval = args[0];
            return GCC_GEN_BE_SUCCESS;
        }
    }
    if (strcmp(name, "__builtin_frame_address") == 0 || strcmp(name, "__builtin_return_address") == 0 ||
        strcmp(name, "__builtin_dwarf_cfa") == 0) {
        gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_type *uint_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT);
        gcc_jit_function *fn_addr = NULL;
        GCC_GEN_BE_CHECK(get_builtin(ctx, name, void_ptr_t, num_args > 0 ? 1 : 0, &uint_t, 0, &fn_addr));
        gcc_jit_rvalue *zero_u = gcc_jit_context_new_rvalue_from_int(ctx, uint_t, 0);
        gcc_jit_rvalue *call_arg = (num_args > 0 && args && args[0])
            ? gcc_jit_context_new_cast(ctx, NULL, args[0], uint_t)
            : zero_u;
        *out_rval = gcc_jit_context_new_call(ctx, NULL, fn_addr, num_args > 0 ? 1 : 0, &call_arg);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_shufflevector") == 0 || strcmp(name, "__builtin_shuffle") == 0) {
        if (num_args >= 2 && args && args[0] && args[1]) {
            if (p_gcc_jit_context_new_rvalue_vector_perm && num_args >= 3 && args[2]) {
                *out_rval = p_gcc_jit_context_new_rvalue_vector_perm(ctx, NULL, args[0], args[1], args[2]);
                return GCC_GEN_BE_SUCCESS;
            }
            *out_rval = args[0];
            return GCC_GEN_BE_SUCCESS;
        }
    }
    if (strcmp(name, "__builtin_convertvector") == 0 && num_args >= 1 && args && args[0]) {
        gcc_jit_type *src_t = gcc_jit_rvalue_get_type(args[0]);
        gcc_jit_type *dst_t = src_t;
        if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.return_type &&
            rout->type->variant.routine.return_type->kind == tk_vector) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(rout->type->variant.routine.return_type, &dst_t));
        }
        *out_rval = (dst_t == src_t)
            ? args[0]
            : (p_gcc_jit_context_new_bitcast
                ? p_gcc_jit_context_new_bitcast(ctx, NULL, args[0], dst_t)
                : args[0]);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_is_constant_evaluated") == 0 || strcmp(name, "__builtin_constant_p") == 0) {
        int is_const = (strcmp(name, "__builtin_constant_p") == 0 && rout->compiler_generated) ? 1 : 0;
        *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), is_const);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_va_arg_pack") == 0 || strcmp(name, "__builtin_va_arg_pack_len") == 0) {
        int pack_val = (strcmp(name, "__builtin_va_arg_pack_len") == 0 && num_args > 0) ? num_args : 0;
        *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), pack_val);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_object_size") == 0 || strcmp(name, "__builtin_dynamic_object_size") == 0) {
        a_type_ptr p0_type = (rout->type && rout->type->kind == tk_routine &&
                              rout->type->variant.routine.extra_info &&
                              rout->type->variant.routine.extra_info->param_type_list)
            ? rout->type->variant.routine.extra_info->param_type_list->type
            : NULL;
        size_t bos_bound = (size_t)-1;
        GCC_GEN_BE_CHECK(gcc_gen_be_eval_builtin_object_size(p0_type, 0, &bos_bound));
        gcc_jit_type *sz_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
        *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, sz_t, (long)bos_bound);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_clear_padding") == 0 && num_args >= 1 && args && args[0]) {
        a_type_ptr p0_type = (rout->type && rout->type->kind == tk_routine &&
                              rout->type->variant.routine.extra_info &&
                              rout->type->variant.routine.extra_info->param_type_list)
            ? rout->type->variant.routine.extra_info->param_type_list->type
            : NULL;
        return gcc_gen_be_lower_builtin_clear_padding(p0_type, args[0], out_rval);
    }
    if (strncmp(name, "__builtin___", 12) == 0 && strstr(name, "_chk") != NULL) {
        /* Fortified libc builtins (__builtin___memcpy_chk, __builtin___memset_chk, __builtin___strcpy_chk, etc.):
           forward the non-bos arguments to the underlying standard C library function. */
        const char *base_start = name + 12;
        const char *chk_pos = strstr(base_start, "_chk");
        size_t base_len = (size_t)(chk_pos - base_start);
        char libc_name[64];
        if (base_len > 0 && base_len < sizeof(libc_name)) {
            memcpy(libc_name, base_start, base_len);
            libc_name[base_len] = '\0';
            int forwarded_args = (num_args > 1) ? (num_args - 1) : num_args;
            if ((strcmp(libc_name, "snprintf") == 0 || strcmp(libc_name, "vsnprintf") == 0) && num_args >= 4) {
                /* __builtin___snprintf_chk(s, maxlen, flag, os, fmt, ...) -> snprintf(s, maxlen, fmt, ...) */
                gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                gcc_jit_type *char_ptr_t = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR));
                gcc_jit_type *const_char_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CONST_CHAR_PTR);
                gcc_jit_type *sz_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
                gcc_jit_type *sn_arg_types[3] = { char_ptr_t, sz_t, const_char_ptr_t };
                gcc_jit_function *sn_fn = NULL;
                GCC_GEN_BE_CHECK(get_builtin(ctx, "snprintf", int_t, 3, sn_arg_types, 1, &sn_fn));
                int extra_varargs = (num_args > 5) ? (num_args - 5) : 0;
                int total_sn_args = 3 + extra_varargs;
                gcc_jit_rvalue **sn_args = (gcc_jit_rvalue **)malloc((size_t)total_sn_args * sizeof(gcc_jit_rvalue *));
                if (!sn_args) return GCC_GEN_BE_ERROR_OOM;
                sn_args[0] = gcc_jit_context_new_cast(ctx, NULL, args[0], char_ptr_t);
                sn_args[1] = gcc_jit_context_new_cast(ctx, NULL, args[1], sz_t);
                sn_args[2] = gcc_jit_context_new_cast(ctx, NULL, args[4 < num_args ? 4 : (num_args - 1)], const_char_ptr_t);
                for (int v = 0; v < extra_varargs; ++v) {
                    sn_args[3 + v] = args[5 + v];
                }
                *out_rval = gcc_jit_context_new_call(ctx, NULL, sn_fn, total_sn_args, sn_args);
                free(sn_args);
                return GCC_GEN_BE_SUCCESS;
            }
            gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
            gcc_jit_type **f_types = NULL;
            if (forwarded_args > 0) {
                f_types = (gcc_jit_type **)malloc((size_t)forwarded_args * sizeof(gcc_jit_type *));
                if (!f_types) return GCC_GEN_BE_ERROR_OOM;
                for (int i = 0; i < forwarded_args; ++i) {
                    f_types[i] = (args && args[i]) ? gcc_jit_rvalue_get_type(args[i]) : void_ptr_t;
                }
            }
            gcc_jit_function *libc_fn = NULL;
            gcc_gen_be_error_t l_err = get_builtin(ctx, libc_name, void_ptr_t, forwarded_args, f_types, 0, &libc_fn);
            if (f_types) free(f_types);
            if (l_err != GCC_GEN_BE_SUCCESS) return l_err;
            *out_rval = gcc_jit_context_new_call(ctx, NULL, libc_fn, forwarded_args, args);
            return GCC_GEN_BE_SUCCESS;
        }
    }
    if (strcmp(name, "__builtin_classify_type") == 0) {
        int cls_code = 1;
        if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info &&
            rout->type->variant.routine.extra_info->param_type_list &&
            rout->type->variant.routine.extra_info->param_type_list->type) {
            a_type_ptr pt = rout->type->variant.routine.extra_info->param_type_list->type;
            while (pt && pt->kind == tk_typeref) pt = pt->variant.typeref.type;
            if (pt) {
                if (pt->kind == tk_void) cls_code = 0;
                else if (pt->kind == tk_integer) {
                    if (pt->variant.integer.bool_type) cls_code = 4;
                    else if (pt->variant.integer.enum_type) cls_code = 3;
                    else if (pt->size == 1) cls_code = 2;
                    else cls_code = 1;
                }
                else if (pt->kind == tk_pointer) cls_code = pt->variant.pointer.is_reference ? 6 : 5;
                else if (pt->kind == tk_ptr_to_member) cls_code = 7;
                else if (pt->kind == tk_float) cls_code = 8;
                else if (pt->kind == tk_complex) cls_code = 9;
                else if (pt->kind == tk_routine) cls_code = 10;
                else if (pt->kind == tk_struct || pt->kind == tk_class) cls_code = 12;
                else if (pt->kind == tk_union) cls_code = 13;
                else if (pt->kind == tk_array) cls_code = 14;
                else if (pt->kind == tk_vector) cls_code = 19;
            }
        }
        *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), cls_code);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_types_compatible_p") == 0) {
        int compat = 1;
        if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info &&
            rout->type->variant.routine.extra_info->param_type_list) {
            a_param_type_ptr p0 = rout->type->variant.routine.extra_info->param_type_list;
            a_param_type_ptr p1 = p0 ? p0->next : NULL;
            if (p0 && p1 && p0->type && p1->type) {
                a_type_ptr t0 = p0->type;
                a_type_ptr t1 = p1->type;
                while (t0 && t0->kind == tk_typeref) t0 = t0->variant.typeref.type;
                while (t1 && t1->kind == tk_typeref) t1 = t1->variant.typeref.type;
                if (t0 && t1 && (t0->kind != t1->kind || size_of_type(t0) != size_of_type(t1))) {
                    compat = 0;
                }
            }
        } else if (num_args >= 2 && args && args[0] && args[1]) {
            if (gcc_jit_rvalue_get_type(args[0]) != gcc_jit_rvalue_get_type(args[1])) {
                compat = 0;
            }
        }
        *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), compat);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_offsetof") == 0) {
        if (num_args >= 1 && args && args[num_args - 1]) {
            *out_rval = gcc_jit_context_new_cast(ctx, NULL, args[num_args - 1], gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T));
        } else {
            *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T), 0);
        }
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_choose_expr") == 0 && num_args >= 3 && args) {
        *out_rval = args[1] ? args[1] : args[2];
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_bit_cast") == 0 && num_args >= 1 && args && args[num_args - 1]) {
        a_type_ptr ret_tp = (rout->type && rout->type->kind == tk_routine)
            ? rout->type->variant.routine.return_type
            : NULL;
        return gcc_gen_be_lower_builtin_bit_cast(ret_tp, NULL, args[num_args - 1], out_rval);
    }
    if (strcmp(name, "_abnormal_termination") == 0 ||
        strcmp(name, "__builtin_eh_AbnormalTermination") == 0 ||
        strcmp(name, "AbnormalTermination") == 0) {
        void *abn_lval = NULL;
        if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_abnormal_termination", &abn_lval) == GCC_GEN_BE_SUCCESS && abn_lval) {
            *out_rval = gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)abn_lval);
        } else {
            *out_rval = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
        }
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "_exception_code") == 0 ||
        strcmp(name, "GetExceptionCode") == 0 ||
        strcmp(name, "__exception_code") == 0) {
        void *code_lval = NULL;
        if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_exception_code", &code_lval) == GCC_GEN_BE_SUCCESS && code_lval) {
            *out_rval = gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)code_lval);
        } else {
            *out_rval = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG));
        }
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "_exception_info") == 0 ||
        strcmp(name, "GetExceptionInformation") == 0 ||
        strcmp(name, "__exception_info") == 0) {
        void *info_lval = NULL;
        if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_exception_info", &info_lval) == GCC_GEN_BE_SUCCESS && info_lval) {
            *out_rval = gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)info_lval);
        } else {
            *out_rval = gcc_jit_context_null(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR));
        }
        return GCC_GEN_BE_SUCCESS;
    }
    
    gcc_jit_type *ret_type = NULL;
    if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.return_type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(rout->type->variant.routine.return_type, &ret_type));
    } else {
        ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    }
    
    int safe_args = num_args < 0 ? 0 : num_args;
    gcc_jit_type **arg_types = NULL;
    if (safe_args > 0) {
        arg_types = (gcc_jit_type **)malloc((size_t)safe_args * sizeof(gcc_jit_type *));
        if (!arg_types) return GCC_GEN_BE_ERROR_OOM;
        for (int i = 0; i < safe_args; ++i) {
            arg_types[i] = args && args[i] ? gcc_jit_rvalue_get_type(args[i]) : gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        }
    }
    
    gcc_jit_function *fn = NULL;
    gcc_gen_be_error_t err = get_builtin(ctx, name, ret_type, safe_args, arg_types, 0, &fn);
    if (arg_types) free(arg_types);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, safe_args, args);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry in the deterministic SIMD intrinsic mapping table.
 * @details Maps a frontend x86 SSE/AVX/AVX-512 or ARM NEON intrinsic name to its exact GCC target builtin name
 * and optional native `libgccjit` binary vector operator (`op_kind`, or `-1` if not a direct binary op).
 */
typedef struct gcc_gen_be_simd_entry {
    /** @brief Frontend intrinsic name (e.g. `_mm_add_ps`, `vaddq_f32`). */
    const char *intrinsic_name;
    /** @brief Target GCC builtin name (e.g. `__builtin_ia32_addps`, `__builtin_neon_vaddv4sf`). */
    const char *gcc_builtin_name;
    /** @brief Native `gcc_jit_binary_op` when lowerable as a vector binary op, or `-1`. */
    int binary_op;
} gcc_gen_be_simd_entry_t;

/** @brief Deterministic lookup table for x86 SSE/AVX/AVX-512 and ARM NEON intrinsics. */
static const gcc_gen_be_simd_entry_t simd_intrinsic_table[] = {
    { "_mm_add_ps", "__builtin_ia32_addps", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm_sub_ps", "__builtin_ia32_subps", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm_mul_ps", "__builtin_ia32_mulps", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm_div_ps", "__builtin_ia32_divps", (int)GCC_JIT_BINARY_OP_DIVIDE },
    { "_mm_add_pd", "__builtin_ia32_addpd", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm_sub_pd", "__builtin_ia32_subpd", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm_mul_pd", "__builtin_ia32_mulpd", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm_div_pd", "__builtin_ia32_divpd", (int)GCC_JIT_BINARY_OP_DIVIDE },
    { "_mm_add_epi32", "__builtin_ia32_paddd128", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm_sub_epi32", "__builtin_ia32_psubd128", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm_mullo_epi32", "__builtin_ia32_pmulld128", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm_and_si128", "__builtin_ia32_pand128", (int)GCC_JIT_BINARY_OP_BITWISE_AND },
    { "_mm_or_si128", "__builtin_ia32_por128", (int)GCC_JIT_BINARY_OP_BITWISE_OR },
    { "_mm_xor_si128", "__builtin_ia32_pxor128", (int)GCC_JIT_BINARY_OP_BITWISE_XOR },
    { "_mm256_add_ps", "__builtin_ia32_addps256", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm256_sub_ps", "__builtin_ia32_subps256", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm256_mul_ps", "__builtin_ia32_mulps256", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm256_div_ps", "__builtin_ia32_divps256", (int)GCC_JIT_BINARY_OP_DIVIDE },
    { "_mm256_add_pd", "__builtin_ia32_addpd256", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm256_sub_pd", "__builtin_ia32_subpd256", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm256_mul_pd", "__builtin_ia32_mulpd256", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm256_div_pd", "__builtin_ia32_divpd256", (int)GCC_JIT_BINARY_OP_DIVIDE },
    { "_mm512_add_ps", "__builtin_ia32_addps512_mask", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm512_sub_ps", "__builtin_ia32_subps512_mask", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm512_mul_ps", "__builtin_ia32_mulps512_mask", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm512_div_ps", "__builtin_ia32_divps512_mask", (int)GCC_JIT_BINARY_OP_DIVIDE },
    { "_mm512_add_pd", "__builtin_ia32_addpd512_mask", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm512_sub_pd", "__builtin_ia32_subpd512_mask", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm512_mul_pd", "__builtin_ia32_mulpd512_mask", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm512_div_pd", "__builtin_ia32_divpd512_mask", (int)GCC_JIT_BINARY_OP_DIVIDE },
    { "_mm512_mask_add_ps", "__builtin_ia32_addps512_mask", -1 },
    { "_mm512_maskz_add_ps", "__builtin_ia32_addps512_maskz", -1 },
    { "_mm_fmadd_ps", "__builtin_ia32_vfmaddps", -1 },
    { "_mm256_fmadd_ps", "__builtin_ia32_vfmaddps256", -1 },
    { "_mm512_fmadd_ps", "__builtin_ia32_vfmaddps512_mask", -1 },
    { "vaddq_f32", "__builtin_neon_vaddv4sf", (int)GCC_JIT_BINARY_OP_PLUS },
    { "vsubq_f32", "__builtin_neon_vsubv4sf", (int)GCC_JIT_BINARY_OP_MINUS },
    { "vmulq_f32", "__builtin_neon_vmulv4sf", (int)GCC_JIT_BINARY_OP_MULT },
    { "vaddq_s32", "__builtin_neon_vaddv4si", (int)GCC_JIT_BINARY_OP_PLUS },
    { "vsubq_s32", "__builtin_neon_vsubv4si", (int)GCC_JIT_BINARY_OP_MINUS },
    { "vmulq_s32", "__builtin_neon_vmulv4si", (int)GCC_JIT_BINARY_OP_MULT },
    { "vdotq_s32", "__builtin_neon_vdotsv4si", -1 },
    { "vfmaq_f32", "__builtin_neon_vfmav4sf", -1 },
    { "vpaddq_f32", "__builtin_neon_vpaddv4sf", -1 },
    { "_mm_min_ps", "__builtin_ia32_minps", -1 },
    { "_mm_max_ps", "__builtin_ia32_maxps", -1 },
    { "_mm_min_pd", "__builtin_ia32_minpd", -1 },
    { "_mm_max_pd", "__builtin_ia32_maxpd", -1 },
    { "_mm256_min_ps", "__builtin_ia32_minps256", -1 },
    { "_mm256_max_ps", "__builtin_ia32_maxps256", -1 },
    { "svadd_f32_z", "__builtin_sve_svadd_f32_z", -1 },
    { "svsub_f32_z", "__builtin_sve_svsub_f32_z", -1 },
    { "svmul_f32_z", "__builtin_sve_svmul_f32_z", -1 },
    { "svld1_f32", "__builtin_sve_svld1_f32", -1 },
    { "svst1_f32", "__builtin_sve_svst1_f32", -1 },
    { "svmla_f32_z", "__builtin_sve_svmla_f32_z", -1 },
    { "svptrue_b32", "__builtin_sve_svptrue_b32", -1 },
    { "svwhilelt_b32", "__builtin_sve_svwhilelt_b32", -1 },
    { "__riscv_vadd_vv_i32m1", "__builtin_rvv_vadd_vv_i32m1", -1 },
    { "__riscv_vsub_vv_i32m1", "__builtin_rvv_vsub_vv_i32m1", -1 },
    { "__riscv_vmul_vv_i32m1", "__builtin_rvv_vmul_vv_i32m1", -1 },
    { "__riscv_vle32_v_f32m1", "__builtin_rvv_vle32_v_f32m1", -1 },
    { "__riscv_vse32_v_f32m1", "__builtin_rvv_vse32_v_f32m1", -1 },
    { "__riscv_vfmacc_vv_f32m1", "__builtin_rvv_vfmacc_vv_f32m1", -1 },
    { "__riscv_vredsum_vs_i32m1_i32m1", "__builtin_rvv_vredsum_vs_i32m1_i32m1", -1 },
    { "__riscv_vsetvl_e32m1", "__builtin_rvv_vsetvl_e32m1", -1 },
    { "_mm_cmpeq_ps", "__builtin_ia32_cmpeqps", -1 },
    { "_mm_cmpgt_ps", "__builtin_ia32_cmpgtps", -1 },
    { "_mm_shuffle_epi8", "__builtin_ia32_pshufb128", -1 },
    { "_mm_blendv_ps", "__builtin_ia32_blendvps", -1 },
    { "_mm_unpacklo_ps", "__builtin_ia32_unpcklps", -1 },
    { "_mm_unpackhi_ps", "__builtin_ia32_unpckhps", -1 },
    { "_mm_load_ps", "__builtin_ia32_loadaps", -1 },
    { "_mm_loadu_ps", "__builtin_ia32_loadups", -1 },
    { "_mm_store_ps", "__builtin_ia32_storeaps", -1 },
    { "_mm_storeu_ps", "__builtin_ia32_storeups", -1 },
    { "_mm_loadu_si128", "__builtin_ia32_loaddqu", -1 },
    { "_mm_storeu_si128", "__builtin_ia32_storedqu", -1 },
    { "_mm_set1_ps", "__builtin_ia32_vec_init_v4sf", -1 },
    { "_mm_setzero_ps", "__builtin_ia32_xorps", -1 },
    { "_mm_hadd_ps", "__builtin_ia32_haddps", -1 },
    { "_mm_dp_ps", "__builtin_ia32_dpps", -1 },
    { "_mm_movemask_epi8", "__builtin_ia32_pmovmskb128", -1 },
    { "_mm_movemask_ps", "__builtin_ia32_movmskps", -1 },
    { "_mm256_permute2f128_ps", "__builtin_ia32_vperm2f128_ps256", -1 },
    { "_mm256_permute4x64_epi64", "__builtin_ia32_permdi256", -1 },
    { "_mm256_i32gather_epi32", "__builtin_ia32_gathersiv4si", -1 },
    { "_mm256_fmsub_ps", "__builtin_ia32_vfmsubps256", -1 },
    { "_mm256_fnmadd_ps", "__builtin_ia32_vfnmaddps256", -1 },
    { "_mm512_cmp_ps_mask", "__builtin_ia32_cmpps512_mask", -1 },
    { "_mm512_reduce_add_ps", "__builtin_ia32_reduce_add_v16sf", -1 },
    { "vld1q_f32", "__builtin_neon_vld1v4sf", -1 },
    { "vst1q_f32", "__builtin_neon_vst1v4sf", -1 },
    { "vld2q_f32", "__builtin_neon_vld2v4sf", -1 },
    { "vgetq_lane_s32", "__builtin_neon_vget_lanev4si", -1 },
    { "vsetq_lane_s32", "__builtin_neon_vset_lanev4si", -1 },
    { "vaddvq_f32", "__builtin_neon_vaddvv4sf", -1 },
    { "vmaxvq_f32", "__builtin_neon_vmaxvv4sf", -1 },
    { "vqtbl1q_u8", "__builtin_neon_vqtbl1v16qi", -1 },
    { NULL, NULL, -1 }
};

/**
 * @brief Lowers SIMD vector intrinsics for x86 (SSE/AVX/AVX-512) and ARM NEON.
 *
 * @details Translates x86 (`_mm_*`, `_mm256_*`, `_mm512_*`) and ARM NEON (`vadd*`, `vsub*`,
 * `vmul*`, `vld*`, `vst*`, `vdup*`, `vreinterpret*`) intrinsics via `simd_intrinsic_table` to
 * native vector operations or exact GCC target builtins, supporting arbitrary argument counts.
 *
 * @param[in] name The builtin name (e.g. "_mm_add_ps").
 * @param[in] rout The routine pointer.
 * @param[in] num_args Number of arguments.
 * @param[in] args Array of lowered argument rvalues.
 * @param[out] out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_simd_builtin(const char *name, a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;
    
    gcc_jit_type *ret_type = NULL;
    if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.return_type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(rout->type->variant.routine.return_type, &ret_type));
    } else {
        ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    }

    const char *mapped_gcc_name = NULL;
    int mapped_bin_op = -1;
    for (size_t i = 0; simd_intrinsic_table[i].intrinsic_name != NULL; ++i) {
        if (strcmp(name, simd_intrinsic_table[i].intrinsic_name) == 0) {
            mapped_gcc_name = simd_intrinsic_table[i].gcc_builtin_name;
            mapped_bin_op = simd_intrinsic_table[i].binary_op;
            break;
        }
    }

    if (mapped_bin_op >= 0 && num_args == 2 && args && args[0] && args[1] &&
        rout->type && rout->type->kind == tk_routine &&
        rout->type->variant.routine.return_type &&
        rout->type->variant.routine.return_type->kind == tk_vector &&
        gcc_jit_rvalue_get_type(args[0]) == ret_type &&
        gcc_jit_rvalue_get_type(args[1]) == ret_type) {
        *out_rval = gcc_jit_context_new_binary_op(
            ctx, NULL, (enum gcc_jit_binary_op)mapped_bin_op, ret_type, args[0], args[1]);
        return GCC_GEN_BE_SUCCESS;
    }
    
    int safe_args = num_args < 0 ? 0 : num_args;
    gcc_jit_type **arg_types = NULL;
    if (safe_args > 0) {
        arg_types = (gcc_jit_type **)malloc((size_t)safe_args * sizeof(gcc_jit_type *));
        if (!arg_types) return GCC_GEN_BE_ERROR_OOM;
        for (int i = 0; i < safe_args; ++i) {
            arg_types[i] = args && args[i] ? gcc_jit_rvalue_get_type(args[i]) : ret_type;
        }
    }
    
    char gcc_name[128];
    if (mapped_gcc_name) {
        snprintf(gcc_name, sizeof(gcc_name), "%s", mapped_gcc_name);
    } else if (strncmp(name, "_mm_", 4) == 0) {
        snprintf(gcc_name, sizeof(gcc_name), "__builtin_ia32%s", name + 3);
    } else if (strncmp(name, "_mm256_", 7) == 0) {
        snprintf(gcc_name, sizeof(gcc_name), "__builtin_ia32_%s256", name + 7);
    } else if (strncmp(name, "_mm512_", 7) == 0) {
        snprintf(gcc_name, sizeof(gcc_name), "__builtin_ia32_%s512", name + 7);
    } else if (strncmp(name, "vadd", 4) == 0 || strncmp(name, "vsub", 4) == 0 ||
               strncmp(name, "vmul", 4) == 0 || strncmp(name, "vfma", 4) == 0 ||
               strncmp(name, "vcmp", 4) == 0 || strncmp(name, "vbsl", 4) == 0 ||
               strncmp(name, "vext", 4) == 0 || strncmp(name, "vrev", 4) == 0 ||
               strncmp(name, "vget_low", 8) == 0 || strncmp(name, "vget_high", 9) == 0 ||
               strncmp(name, "vcombine", 8) == 0 || strncmp(name, "vld", 3) == 0 ||
               strncmp(name, "vst", 3) == 0 || strncmp(name, "vdup", 4) == 0 ||
               strncmp(name, "vreinterpret", 12) == 0) {
        snprintf(gcc_name, sizeof(gcc_name), "__builtin_neon%s", name + 1);
    } else {
        snprintf(gcc_name, sizeof(gcc_name), "%s", name);
    }

    gcc_jit_param **params = NULL;
    if (safe_args > 0) {
        params = (gcc_jit_param **)malloc((size_t)safe_args * sizeof(gcc_jit_param *));
        if (!params) {
            free(arg_types);
            return GCC_GEN_BE_ERROR_OOM;
        }
        for (int i = 0; i < safe_args; ++i) {
            char pname[32];
            snprintf(pname, sizeof(pname), "simd_p%d", i);
            params[i] = gcc_jit_context_new_param(ctx, NULL, arg_types[i], pname);
        }
    }
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, gcc_name, safe_args, params, 0);
    if (params) free(params);
    if (arg_types) free(arg_types);
    if (!fn) return GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN;
    
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, safe_args, args);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers sanitizer and profiling hooks.
 *
 * @details Emits calls to __asan_* and __ubsan_* instrumentation hooks.
 *
 * @param[in] name The builtin name.
 * @param[in] num_args Number of arguments.
 * @param[in] args Array of lowered argument rvalues.
 * @param[out] out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_sanitizer_hook(const char *name, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;
    
    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    
    int safe_args = num_args < 0 ? 0 : num_args;
    gcc_jit_type **arg_types = NULL;
    if (safe_args > 0) {
        arg_types = (gcc_jit_type **)malloc((size_t)safe_args * sizeof(gcc_jit_type *));
        if (!arg_types) return GCC_GEN_BE_ERROR_OOM;
        for (int i = 0; i < safe_args; ++i) {
            arg_types[i] = args && args[i] ? gcc_jit_rvalue_get_type(args[i]) : gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        }
    }
    
    gcc_jit_function *fn = NULL;
    gcc_gen_be_error_t err = get_builtin(ctx, name, void_type, safe_args, arg_types, 0, &fn);
    if (arg_types) free(arg_types);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, safe_args, args);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers C++20 `<coroutine>` GCC builtins (`__builtin_coro_*`).
 *
 * @details Implements `__builtin_coro_done`, `__builtin_coro_resume`,
 * `__builtin_coro_destroy`, `__builtin_coro_promise`, `__builtin_coro_size`,
 * `__builtin_coro_frame`, and `__builtin_coro_free` using the standard
 * coroutine frame header layout (`{ resume_fn, destroy_fn, promise_obj, state_index, ... }`).
 *
 * @param[in] name The coroutine builtin name.
 * @param[in] rout The routine pointer.
 * @param[in] num_args Number of arguments.
 * @param[in] args Array of lowered argument rvalues.
 * @param[out] out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_coro_builtin(const char *name, a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
    gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
    gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);

    gcc_jit_rvalue *addr_arg = (num_args > 0 && args && args[0])
        ? gcc_jit_context_new_cast(ctx, NULL, args[0], void_ptr_type)
        : gcc_jit_context_null(ctx, void_ptr_type);

    if (strcmp(name, "__builtin_coro_resume") == 0 || strcmp(name, "__builtin_coro_destroy") == 0) {
        int slot_idx = (strcmp(name, "__builtin_coro_destroy") == 0) ? 1 : 0;
        gcc_jit_type *fn_ptr_type = gcc_jit_context_new_function_ptr_type(ctx, NULL, void_type, 1, &void_ptr_type, 0);
        gcc_jit_type *fn_ptr_ptr_type = gcc_jit_type_get_pointer(fn_ptr_type);
        gcc_jit_rvalue *fn_slot_base = gcc_jit_context_new_cast(ctx, NULL, addr_arg, fn_ptr_ptr_type);
        gcc_jit_rvalue *idx_rval = gcc_jit_context_new_rvalue_from_int(
            ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), slot_idx);
        gcc_jit_lvalue *fn_lval = gcc_jit_context_new_array_access(ctx, NULL, fn_slot_base, idx_rval);
        gcc_jit_rvalue *fn_ptr = gcc_jit_lvalue_as_rvalue(fn_lval);
        *out_rval = gcc_jit_context_new_call_through_ptr(ctx, NULL, fn_ptr, 1, &addr_arg);
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__builtin_coro_done") == 0) {
        /* A coroutine at final suspend has its resume pointer cleared or state_index == -1.
           In GCC/Clang ABI, checking *(void**)addr == NULL is the standard __builtin_coro_done check;
           we also support fallback to lower_math_bitwise_builtin if needed. */
        gcc_jit_type *void_ptr_ptr_type = gcc_jit_type_get_pointer(void_ptr_type);
        gcc_jit_rvalue *resume_slot_ptr = gcc_jit_context_new_cast(ctx, NULL, addr_arg, void_ptr_ptr_type);
        gcc_jit_rvalue *resume_ptr_val = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(resume_slot_ptr, NULL));
        *out_rval = gcc_jit_context_new_comparison(
            ctx, NULL, GCC_JIT_COMPARISON_EQ, resume_ptr_val, gcc_jit_context_null(ctx, void_ptr_type));
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__builtin_coro_promise") == 0) {
        /* Promise is stored immediately after the two function pointers (2 * sizeof(void*)).
           When from_promise (arg 2) is non-zero, subtract offset; otherwise add offset. */
        long hdr_offset = (long)(2 * sizeof(void *));
        gcc_jit_rvalue *byte_ptr = gcc_jit_context_new_cast(ctx, NULL, addr_arg, char_ptr_type);
        gcc_jit_rvalue *pos_off = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, hdr_offset);
        gcc_jit_rvalue *neg_off = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, -hdr_offset);
        gcc_jit_rvalue *to_prom = gcc_jit_context_new_cast(
            ctx, NULL,
            gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, byte_ptr, pos_off), NULL),
            void_ptr_type);
        gcc_jit_rvalue *from_prom = gcc_jit_context_new_cast(
            ctx, NULL,
            gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, byte_ptr, neg_off), NULL),
            void_ptr_type);
        if (num_args >= 3 && args && args[2]) {
            gcc_jit_rvalue *zero_arg = gcc_jit_context_zero(ctx, gcc_jit_rvalue_get_type(args[2]));
            gcc_jit_rvalue *is_from_prom = gcc_jit_context_new_comparison(
                ctx, NULL, GCC_JIT_COMPARISON_NE, args[2], zero_arg);
            gcc_jit_block *cblock = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
            if (cblock) {
                gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                gcc_jit_lvalue *res_local = gcc_jit_function_new_local(func, NULL, void_ptr_type, "coro_prom_ptr");
                gcc_jit_block *then_blk = gcc_jit_function_new_block(func, "coro_from_prom");
                gcc_jit_block *else_blk = gcc_jit_function_new_block(func, "coro_to_prom");
                gcc_jit_block *merge_blk = gcc_jit_function_new_block(func, "coro_prom_merge");
                gcc_jit_block_end_with_conditional(cblock, NULL, is_from_prom, then_blk, else_blk);
                gcc_jit_block_add_assignment(then_blk, NULL, res_local, from_prom);
                gcc_jit_block_end_with_jump(then_blk, NULL, merge_blk);
                gcc_jit_block_add_assignment(else_blk, NULL, res_local, to_prom);
                gcc_jit_block_end_with_jump(else_blk, NULL, merge_blk);
                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_blk));
                *out_rval = gcc_jit_lvalue_as_rvalue(res_local);
                return GCC_GEN_BE_SUCCESS;
            }
        }
        *out_rval = to_prom;
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__builtin_coro_size") == 0) {
        gcc_jit_type *active_frame_type = NULL;
        if (gcc_gen_be_get_active_coroutine_frame(&active_frame_type, NULL) == GCC_GEN_BE_SUCCESS && active_frame_type) {
            ssize_t fsz = gcc_jit_type_get_size(active_frame_type);
            if (fsz > 0) {
                *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)fsz);
            } else {
                *out_rval = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_context_new_sizeof(ctx, active_frame_type), size_t_type);
            }
        } else {
            *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, 64);
        }
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__builtin_coro_frame") == 0 || strcmp(name, "__builtin_coro_free") == 0) {
        *out_rval = addr_arg;
        return GCC_GEN_BE_SUCCESS;
    }

    (void)bool_type;
    return lower_math_bitwise_builtin(name, rout, num_args, args, out_rval);
}

/**
 * @brief High-level dispatcher for lowering GCC builtins and intrinsics.
 *
 * @details Dispatches atomic, math/bitwise/overflow, SIMD, coroutine, and sanitizer builtins.
 *
 * @param[in] rout The routine entry of the builtin.
 * @param[in] num_args The number of arguments in the call.
 * @param[in] args Array of JIT rvalues for the arguments.
 * @param[out] out_rval Pointer to receive the generated JIT rvalue for the result.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_builtin_call(a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    const char *name = rout->source_corresp.name;
#ifdef EDG_GCC_BE_ISOLATED_TEST
    if (!name && rout->is_builtin_function) {
        name = "__builtin_generic";
    }
#else
    if (!name && rout->special_kind == sfk_none && rout->variant.builtin_function_kind != bfk_none) {
        name = "__builtin_generic";
    }
#endif
    if (!name) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    if (strncmp(name, "__atomic_", 9) == 0 || strncmp(name, "__sync_", 7) == 0) {
        return lower_atomic_builtin(name, rout, num_args, args, out_rval);
    } else if (strncmp(name, "__builtin_coro_", 15) == 0) {
        return lower_coro_builtin(name, rout, num_args, args, out_rval);
    } else if (strncmp(name, "__builtin_add_overflow", 22) == 0 ||
               strncmp(name, "__builtin_sub_overflow", 22) == 0 ||
               strncmp(name, "__builtin_mul_overflow", 22) == 0 ||
               strncmp(name, "__builtin_clz", 13) == 0 ||
               strncmp(name, "__builtin_ctz", 13) == 0 ||
               strncmp(name, "__builtin_popcount", 18) == 0) {
        return lower_math_bitwise_builtin(name, rout, num_args, args, out_rval);
    } else if (strncmp(name, "_mm_", 4) == 0 || strncmp(name, "_mm256_", 7) == 0 ||
               strncmp(name, "_mm512_", 7) == 0 || strncmp(name, "vadd", 4) == 0 ||
               strncmp(name, "vsub", 4) == 0 || strncmp(name, "vmul", 4) == 0 ||
               strncmp(name, "vfma", 4) == 0 || strncmp(name, "vcmp", 4) == 0 ||
               strncmp(name, "vbsl", 4) == 0 || strncmp(name, "vext", 4) == 0 ||
               strncmp(name, "vrev", 4) == 0 || strncmp(name, "vget_low", 8) == 0 ||
               strncmp(name, "vget_high", 9) == 0 || strncmp(name, "vcombine", 8) == 0 ||
               strncmp(name, "vld", 3) == 0 || strncmp(name, "vst", 3) == 0 ||
               strncmp(name, "vdup", 4) == 0 || strncmp(name, "vreinterpret", 12) == 0 ||
               strncmp(name, "vpadd", 5) == 0 || strncmp(name, "vdot", 4) == 0 ||
               strncmp(name, "vgetq_lane", 10) == 0 || strncmp(name, "vsetq_lane", 10) == 0 ||
               strncmp(name, "vaddvq", 6) == 0 || strncmp(name, "vmaxvq", 6) == 0 ||
               strncmp(name, "vqtbl", 5) == 0 ||
               strncmp(name, "sv", 2) == 0 || strncmp(name, "__riscv_v", 9) == 0) {
        return lower_simd_builtin(name, rout, num_args, args, out_rval);
    } else if (strncmp(name, "__asan_", 7) == 0 || strncmp(name, "__ubsan_", 8) == 0) {
        return lower_sanitizer_hook(name, num_args, args, out_rval);
    }

    /* Fallback */
    return lower_math_bitwise_builtin(name, rout, num_args, args, out_rval);
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
