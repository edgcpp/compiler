/**
 * @file gcc_gen_be_expr.c
 * @brief Implementation of expression lowering for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_expr.h"
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
#include "gcc_gen_be_lib_loader.h"
#include <libgccjit.h>
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/** @brief Points to the local variable capturing the result of the current statement expression. */
struct gcc_jit_lvalue *current_stmt_expr_result_lval = NULL;

/**
 * @brief Checks if an EDG type is a complex floating-point type.
 * @details Strips typedefs and checks if the underlying type kind is tk_float with is_complex set.
 * @param[in] tp The EDG type pointer to inspect.
 * @param[out] out_is_complex Pointer to receive TRUE if complex type, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if out_is_complex is NULL.
 */
static gcc_gen_be_error_t check_is_complex_type(a_type_ptr tp, a_boolean *out_is_complex) GCC_GEN_BE_NOEXCEPT {
    if (!out_is_complex) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    while (tp && tp->kind == tk_typeref) {
        tp = tp->variant.typeref.type;
    }
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
 * @param[in] ctx The libgccjit context.
 * @param[in] loc The source location (may be NULL).
 * @param[in] func The enclosing libgccjit function.
 * @param[in] cblock The current basic block.
 * @param[in] val The complex rvalue.
 * @param[in] c_type The libgccjit complex type.
 * @param[in] elem_type The underlying floating-point element type.
 * @param[out] out_real Pointer to receive the real part rvalue.
 * @param[out] out_imag Pointer to receive the imaginary part rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on invalid arguments.
 */
static gcc_gen_be_error_t extract_complex_parts(
    gcc_jit_context *ctx,
    gcc_jit_location *loc,
    gcc_jit_function *func,
    gcc_jit_block *cblock,
    gcc_jit_rvalue *val,
    gcc_jit_type *c_type,
    gcc_jit_type *elem_type,
    gcc_jit_rvalue **out_real,
    gcc_jit_rvalue **out_imag) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !val || !c_type || !elem_type || !out_real || !out_imag) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    if (gcc_jit_type_is_struct(c_type)) {
        gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
        *out_real = gcc_jit_rvalue_access_field(val, NULL, gcc_jit_struct_get_field(s, 0));
        *out_imag = gcc_jit_rvalue_access_field(val, NULL, gcc_jit_struct_get_field(s, 1));
    } else {
        if (!func || !cblock) return GCC_GEN_BE_ERROR_INVALID_STATE;
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
                                GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
                            }
                            if (elem_err != GCC_GEN_BE_SUCCESS) return elem_err;
                            if (elem_cleanup_lp && elem_dip->destructor) {
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
                            if (elem_rval && cblock) {
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
                    a_type_ptr agg_type = con->type;
                    while (agg_type && agg_type->kind == tk_typeref) agg_type = agg_type->variant.typeref.type;
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
                                GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                                if (elem_rval && cblock) {
                                    gcc_jit_block_add_assignment(cblock, NULL, elem_lval, elem_rval);
                                }
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
                                    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                                    if (fld_rval && cblock) {
                                        gcc_jit_block_add_assignment(cblock, NULL, fld_lval, fld_rval);
                                    }
                                }
                            }
                            if (f) f = f->next;
                        }
                    } else {
                        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(con, target_type, &init_rval));
                        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                        if (init_rval && cblock) {
                            gcc_jit_block_add_assignment(cblock, NULL, target_lval, init_rval);
                        }
                    }
                    init_rval = gcc_jit_lvalue_as_rvalue(target_lval);
                } else if (con) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(con, target_type, &init_rval));
                    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                    if (target_lval && init_rval && cblock) {
                        gcc_jit_block_add_assignment(cblock, NULL, target_lval, init_rval);
                    }
                }
            }
            break;

        case dik_expression:
        case dik_class_result_via_ctor:
            if (dip->variant.expression) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(dip->variant.expression, &init_rval));
                GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                if (target_lval && init_rval && cblock) {
                    gcc_jit_block_add_assignment(cblock, NULL, target_lval, init_rval);
                }
            }
            break;

        case dik_bitwise_copy:
            if (dip->variant.bitwise_copy.source) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(dip->variant.bitwise_copy.source, &init_rval));
                GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                if (target_lval && init_rval && cblock) {
                    gcc_jit_block_add_assignment(cblock, NULL, target_lval, init_rval);
                }
            }
            break;

        case dik_constructor:
            {
                if (dip->variant.constructor.value_initialization && target_lval && target_type && cblock) {
                    gcc_jit_rvalue *zero_val = NULL;
                    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(edg_type, target_type, &zero_val));
                    if (zero_val) {
                        gcc_jit_block_add_assignment(cblock, NULL, target_lval, zero_val);
                    }
                }
                a_routine_ptr ctor = dip->variant.constructor.ptr;
                if (ctor) {
                    void *cached_ctor = NULL;
                    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, ctor, &cached_ctor));
                    gcc_jit_function *ctor_fn = (gcc_jit_function *)cached_ctor;
                    if (!ctor_fn) {
                        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(ctor, &ctor_fn));
                    }
                    if (ctor_fn && cblock) {
                        int num_args = target_lval ? 1 : 0;
                        for (an_expr_node_ptr a = dip->variant.constructor.args; a != NULL; a = a->next) {
                            num_args++;
                        }
                        int param_count = (int)gcc_jit_function_get_param_count(ctor_fn);
                        int alloc_args = num_args > param_count ? num_args : param_count;
                        gcc_jit_rvalue **args = NULL;
                        if (alloc_args > 0) {
                            args = (gcc_jit_rvalue **)malloc((size_t)alloc_args * sizeof(gcc_jit_rvalue *));
                            if (!args) return GCC_GEN_BE_ERROR_OOM;
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
                                if (vtt_err == GCC_GEN_BE_SUCCESS && vtt_lval) {
                                    args[idx++] = gcc_jit_lvalue_get_address(vtt_lval, NULL);
                                }
                            }
                            while (idx < param_count) {
                                gcc_jit_param *param = gcc_jit_function_get_param(ctor_fn, idx);
                                gcc_jit_type *ptype = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(param));
                                args[idx++] = gcc_jit_context_zero(ctx, ptype);
                            }
                            num_args = idx;
                            for (int p = 0; p < num_args && p < param_count; ++p) {
                                gcc_jit_param *param = gcc_jit_function_get_param(ctor_fn, p);
                                gcc_jit_type *ptype = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(param));
                                gcc_jit_type *atype = gcc_jit_rvalue_get_type(args[p]);
                                if (atype != ptype) {
                                    args[p] = gcc_jit_context_new_cast(ctx, NULL, args[p], ptype);
                                }
                            }
                        }
                        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                        if (cblock) {
                            gcc_jit_block *active_lp = NULL;
                            GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&active_lp));
                            if (active_lp &&
                                gcc_jit_block_get_function(active_lp) == gcc_jit_block_get_function(cblock)) {
                                gcc_jit_block *next_blk = NULL;
                                gcc_gen_be_error_t inv_err = gcc_gen_be_emit_invoke_call(
                                    ctx, cblock, ctor_fn, num_args, args, NULL, NULL, active_lp, &next_blk);
                                if (inv_err != GCC_GEN_BE_SUCCESS) {
                                    if (args) free(args);
                                    return inv_err;
                                }
                            } else {
                                gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, NULL, ctor_fn, num_args, args);
                                gcc_jit_block_add_eval(cblock, NULL, call);
                            }
                        }
                        if (args) free(args);
                    }
                }
                if (target_lval) {
                    init_rval = gcc_jit_lvalue_as_rvalue(target_lval);
                }
            }
            break;

        default:
            break;
    }

    if (dip->destructor && target_lval && !dip->variable) {
#ifndef EDG_GCC_BE_ISOLATED_TEST
        if (exceptions_enabled)
#endif
        {
            void *dtor_cached = NULL;
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, dip->destructor, &dtor_cached));
            gcc_jit_function *dtor_fn = (gcc_jit_function *)dtor_cached;
            if (!dtor_fn) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(dip->destructor, &dtor_fn));
            }
            if (dtor_fn) {
                gcc_gen_be_error_t reg_err = gcc_gen_be_register_cleanup(target_lval, dtor_fn);
                if (reg_err != GCC_GEN_BE_SUCCESS && reg_err != GCC_GEN_BE_ERROR_INTERNAL) {
                    return reg_err;
                }
                if (reg_err == GCC_GEN_BE_SUCCESS) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                    if (cblock) {
                        gcc_jit_function *cur_fn = gcc_jit_block_get_function(cblock);
                        if (cur_fn) {
                            extern gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
                            GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(cur_fn));
                        }
                    }
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
              if (gcc_gen_be_get_active_coroutine_frame(&coro_ft, &coro_fl) == GCC_GEN_BE_SUCCESS &&
                  coro_fl != NULL &&
                  cache_lookup(GCC_GEN_BE_CACHE_FIELD, expr, &cached_coro_fld) == GCC_GEN_BE_SUCCESS &&
                  cached_coro_fld != NULL) {
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
              void *cached = NULL;
              err = cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached);
              if (err != GCC_GEN_BE_SUCCESS) return err;

              if (!cached) {
                  void *cached_fld = NULL;
                  gcc_jit_type *coro_ft = NULL;
                  gcc_jit_lvalue *coro_flval = NULL;
                  if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, var, &cached_fld) == GCC_GEN_BE_SUCCESS &&
                      cached_fld != NULL &&
                      gcc_gen_be_get_active_coroutine_frame(&coro_ft, &coro_flval) == GCC_GEN_BE_SUCCESS &&
                      coro_flval != NULL) {
                      gcc_jit_lvalue *frame_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(coro_flval), NULL);
                      *out_lval = gcc_jit_lvalue_access_field(frame_deref, NULL, (gcc_jit_field *)cached_fld);
                      return GCC_GEN_BE_SUCCESS;
                  }
                  if (var && var->source_corresp.name) {
                      void *existing_sym = NULL;
                      if (cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)var->source_corresp.name, &existing_sym) == GCC_GEN_BE_SUCCESS &&
                          existing_sym != NULL && existing_sym != (void *)var) {
                          /* Check if existing_sym is already a gcc_jit_lvalue (from _ZTV/_ZTT) or another a_variable_ptr */
                          void *var_lval_check = NULL;
                          if (cache_lookup(GCC_GEN_BE_CACHE_VAR, existing_sym, &var_lval_check) == GCC_GEN_BE_SUCCESS && var_lval_check) {
                              GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, var_lval_check));
                              *out_lval = (gcc_jit_lvalue *)var_lval_check;
                              return GCC_GEN_BE_SUCCESS;
                          } else if (strncmp(var->source_corresp.name, "_ZTV", 4) == 0 ||
                                     strncmp(var->source_corresp.name, "_ZTT", 4) == 0 ||
                                     strncmp(var->source_corresp.name, "_ZTI", 4) == 0) {
                              GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, existing_sym));
                              *out_lval = (gcc_jit_lvalue *)existing_sym;
                              return GCC_GEN_BE_SUCCESS;
                          }
                      }
                  }
                  if (!var->source_corresp.name && var->storage_class == sc_static &&
                      var->init_kind == initk_static && var->initializer.constant) {
                      gcc_jit_block *cblock = NULL;
                      if (gcc_gen_be_get_current_block(&cblock) == GCC_GEN_BE_SUCCESS && cblock) {
                          gcc_jit_type *vtype = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(var->type, &vtype));
                          gcc_jit_lvalue *local_tmp = gcc_jit_function_new_local(
                              gcc_jit_block_get_function(cblock), NULL, vtype, "anon_const_lval");
                          gcc_jit_rvalue *init_rval = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(var->initializer.constant, vtype, &init_rval));
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          if (init_rval && cblock) {
                              gcc_jit_block_add_assignment(cblock, NULL, local_tmp, init_rval);
                          }
                          *out_lval = local_tmp;
                          return GCC_GEN_BE_SUCCESS;
                      }
                  }
                  gcc_jit_lvalue *lval = NULL;
                  err = gcc_gen_be_lower_global_variable_decl(var, &lval);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  *out_lval = lval;
              } else {
                  *out_lval = (gcc_jit_lvalue *)cached;
              }
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
                  if (cache_lookup(GCC_GEN_BE_CACHE_VAR, dip->variable, &cached) == GCC_GEN_BE_SUCCESS && cached) {
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
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (r1 && cblock && gcc_jit_rvalue_get_type(r1) != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                      gcc_jit_block_add_eval(cblock, loc, r1);
                  }
                  return gcc_gen_be_lower_expr_lvalue(op2, out_lval);
              } else if (op == eok_assign || op == eok_bassign || op == eok_add_assign || op == eok_subtract_assign ||
                         op == eok_multiply_assign || op == eok_divide_assign || op == eok_remainder_assign ||
                         op == eok_shiftl_assign || op == eok_shiftr_assign || op == eok_and_assign ||
                         op == eok_or_assign || op == eok_xor_assign || op == eok_padd_assign || op == eok_psubtract_assign) {
                  gcc_jit_rvalue *dummy = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(expr, &dummy));
                  return gcc_gen_be_lower_expr_lvalue(op1, out_lval);
                } else if (op == eok_dot_field || op == eok_points_to_field) {
                    if (op2 && op2->kind == enk_field) {
                        a_field_ptr f = op2->variant.field.ptr;
                        if (f->is_bit_field || f->bit_size > 0) {
                            *out_lval = NULL;
                            return GCC_GEN_BE_SUCCESS;
                        }
                        void *cached = NULL;
                        err = cache_lookup(GCC_GEN_BE_CACHE_FIELD, f, &cached);
                        
                        /* If field is not cached, it might be because the struct type hasn't been completely lowered in this scope yet, 
                           so force lowering the struct type of op1 */
                        if (err != GCC_GEN_BE_SUCCESS || !cached) {
                            gcc_jit_type *dummy = NULL;
                            err = gcc_gen_be_lower_type(op1->type, &dummy);
                            if (err != GCC_GEN_BE_SUCCESS) return err;
                            err = cache_lookup(GCC_GEN_BE_CACHE_FIELD, f, &cached);
                            if (err != GCC_GEN_BE_SUCCESS || !cached) {
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
                        err = gcc_gen_be_lower_type(expr->type, &dest_type);
                        if (err != GCC_GEN_BE_SUCCESS) return err;
                        
                        gcc_jit_type *dest_ptr_type = gcc_jit_type_get_pointer(dest_type);
                        gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, loc, byte_ptr, dest_ptr_type);
                        
                        *out_lval = gcc_jit_rvalue_dereference(typed_ptr, loc);
                        return GCC_GEN_BE_SUCCESS;
                    }
              } else if (op == eok_subscript) {
                  gcc_jit_rvalue *ptr = NULL;
                  gcc_jit_lvalue *arr_lval = NULL;
                  a_type_ptr op1_stripped = op1->type;
                  while (op1_stripped && op1_stripped->kind == tk_typeref) {
                      op1_stripped = op1_stripped->variant.typeref.type;
                  }
                  if (op1_stripped && op1_stripped->kind == tk_array) {
                      err = gcc_gen_be_lower_expr_lvalue(op1, &arr_lval);
                      if (err == GCC_GEN_BE_SUCCESS && arr_lval) {
                          gcc_jit_type *elem_type = NULL;
                          err = gcc_gen_be_lower_type(op1_stripped->variant.array.element_type, &elem_type);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                          ptr = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(arr_lval, NULL), elem_ptr_type);
                      }
                  }
                  if (!ptr) {
                      err = gcc_gen_be_lower_expr_rvalue(op1, &ptr);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  
                  gcc_jit_rvalue *idx = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op2, &idx);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  *out_lval = gcc_jit_context_new_array_access(ctx, loc, ptr, idx);
                  return GCC_GEN_BE_SUCCESS;
              } else if (op == eok_pre_incr || op == eok_pre_decr) {
                  gcc_jit_lvalue *l1 = NULL;
                  err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_type *type = NULL;
                  err = gcc_gen_be_lower_type(op1->type, &type);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, type, 1);
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (cblock) {
                      gcc_jit_block_add_assignment_op(cblock, NULL, l1, 
                          op == eok_pre_incr ? GCC_JIT_BINARY_OP_PLUS : GCC_JIT_BINARY_OP_MINUS, one);
                  }
                  *out_lval = l1;
                  return GCC_GEN_BE_SUCCESS;
              }
              *out_lval = NULL;
              return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_UNHANDLED_EXPR, "unhandled lvalue operation kind");
          }
          
      case enk_constant:
          {
              if (expr->variant.constant.ptr) {
                  gcc_jit_type *ctype = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &ctype));
                  gcc_jit_rvalue *crval = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(expr->variant.constant.ptr, ctype, &crval));
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

#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
      case enk_lowered_eh_construct:
          {
              gcc_jit_type *ltype = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &ltype));
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (cblock && ltype && ltype != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                  void *cached_addr = NULL;
                  if (expr->variant.lowered_eh.kind == leck_thrown_object_address) {
                      if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_thrown_obj_addr", &cached_addr) != GCC_GEN_BE_SUCCESS) {
                          cached_addr = NULL;
                      }
                  } else if (expr->variant.lowered_eh.kind == leck_caught_object_address) {
                      if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_caught_obj_addr", &cached_addr) != GCC_GEN_BE_SUCCESS) {
                          cached_addr = NULL;
                      }
                  }
                  if (cached_addr) {
                      gcc_jit_rvalue *exc_ptr = gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached_addr);
                      gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, NULL, exc_ptr, gcc_jit_type_get_pointer(ltype));
                      *out_lval = gcc_jit_rvalue_dereference(typed_ptr, NULL);
                      return GCC_GEN_BE_SUCCESS;
                  }
                  gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                  void *cached_eh_ptr = NULL;
                  if (cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__builtin_eh_pointer", &cached_eh_ptr) != GCC_GEN_BE_SUCCESS || !cached_eh_ptr) {
                      gcc_jit_param *p0 = gcc_jit_context_new_param(ctx, NULL, int_type, "p");
                      gcc_jit_function *eh_ptr_fn = gcc_jit_context_new_function(
                          ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__builtin_eh_pointer", 1, &p0, 0);
                      GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__builtin_eh_pointer", eh_ptr_fn));
                      cached_eh_ptr = eh_ptr_fn;
                  }
                  if (cached_eh_ptr) {
                      gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                      gcc_jit_rvalue *exc_ptr = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_eh_ptr, 1, &zero);
                      gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, NULL, exc_ptr, gcc_jit_type_get_pointer(ltype));
                      *out_lval = gcc_jit_rvalue_dereference(typed_ptr, NULL);
                      return GCC_GEN_BE_SUCCESS;
                  }
              }
              gcc_jit_rvalue *lrval = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(expr, &lrval));
              if (cblock && lrval && ltype) {
                  gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                  gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, ltype, "eh_lval");
                  gcc_jit_block_add_assignment(cblock, loc, tmp, lrval);
                  *out_lval = tmp;
                  return GCC_GEN_BE_SUCCESS;
              }
              *out_lval = NULL;
              return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_UNHANDLED_EXPR, "unhandled lowered_eh lvalue");
          }
#endif

      default:
          *out_lval = NULL;
          return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_UNHANDLED_EXPR, "unhandled lvalue expression node kind");
  }
    return GCC_GEN_BE_SUCCESS;
}



/**
 * @brief Safely casts or bitcasts an rvalue to a destination type.
 *
 * @param[in] ctx The libgccjit context.
 * @param[in] val The rvalue to cast.
 * @param[in] dest_type The target type.
 * @return The cast rvalue.
 */

/**
 * @brief Lowers a bitfield read operation.
 * @details Extracts and aligns a bitfield value.
 * @param[in] expr The expression to lower.
 * @param[out] out_rval The lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t gcc_gen_be_lower_bitfield_read(an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_expr(expr, &loc));

    an_expr_operator_kind op = expr->variant.operation.kind;
    an_expr_node_ptr op1 = expr->variant.operation.operands;
    an_expr_node_ptr op2 = op1 ? op1->next : NULL;
    if (!op1 || !op2 || op2->kind != enk_field || !op2->variant.field.ptr) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
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
    unsigned long unit_sz = (unsigned long)gcc_jit_type_get_size(word_type);
    if (unit_sz == 0) unit_sz = (unsigned long)size_of_type(f->type);
    if (unit_sz == 0) unit_sz = 4;
    unsigned long aligned_off = ((unsigned long)f->offset / unit_sz) * unit_sz;
    unsigned int total_bit_shift = (unsigned int)(((unsigned long)f->offset - aligned_off) * 8UL) + (unsigned int)f->offset_bit_remainder;

    gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_rvalue *offset = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)aligned_off);
    gcc_jit_lvalue *byte_lval = gcc_jit_context_new_array_access(ctx, loc, base_char_ptr, offset);
    gcc_jit_rvalue *byte_ptr = gcc_jit_lvalue_get_address(byte_lval, NULL);
    
    gcc_jit_type *word_ptr_type = gcc_jit_type_get_pointer(word_type);
    gcc_jit_rvalue *word_ptr = gcc_jit_context_new_cast(ctx, loc, byte_ptr, word_ptr_type);
    gcc_jit_rvalue *word_val = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(word_ptr, NULL));
    
    if (total_bit_shift > 0) {
        gcc_jit_rvalue *shift = gcc_jit_context_new_rvalue_from_long(ctx, word_type, (long)total_bit_shift);
        word_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, word_type, word_val, shift);
    }
    
    unsigned long mask_val = (f->bit_size >= sizeof(unsigned long) * 8) ? ~0UL : ((1UL << f->bit_size) - 1UL);
    gcc_jit_rvalue *mask = gcc_jit_context_new_rvalue_from_long(ctx, word_type, (long)mask_val);
    word_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, word_type, word_val, mask);
    
    if (f->bit_field_is_signed) {
        int word_bits = (int)size_of_type(f->type) * 8;
        if (word_bits == 0) word_bits = 32; /* Fallback */
        int shift_amt = word_bits - (int)f->bit_size;
        if (shift_amt > 0) {
            gcc_jit_rvalue *s_amt = gcc_jit_context_new_rvalue_from_long(ctx, word_type, (long)shift_amt);
            word_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, word_type, word_val, s_amt);
            word_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, word_type, word_val, s_amt);
        }
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
    if (!bf_expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_expr(bf_expr, &loc));

    an_expr_operator_kind op = bf_expr->variant.operation.kind;
    an_expr_node_ptr op1 = bf_expr->variant.operation.operands;
    an_expr_node_ptr op2 = op1 ? op1->next : NULL;
    if (!op1 || !op2 || op2->kind != enk_field || !op2->variant.field.ptr) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
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
    unsigned long unit_sz = (unsigned long)gcc_jit_type_get_size(word_type);
    if (unit_sz == 0) unit_sz = (unsigned long)size_of_type(f->type);
    if (unit_sz == 0) unit_sz = 4;
    unsigned long aligned_off = ((unsigned long)f->offset / unit_sz) * unit_sz;
    unsigned int total_bit_shift = (unsigned int)(((unsigned long)f->offset - aligned_off) * 8UL) + (unsigned int)f->offset_bit_remainder;

    gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_rvalue *offset = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)aligned_off);
    gcc_jit_lvalue *byte_lval = gcc_jit_context_new_array_access(ctx, loc, base_char_ptr, offset);
    gcc_jit_rvalue *byte_ptr = gcc_jit_lvalue_get_address(byte_lval, NULL);
    
    gcc_jit_type *word_ptr_type = gcc_jit_type_get_pointer(word_type);
    gcc_jit_rvalue *word_ptr = gcc_jit_context_new_cast(ctx, loc, byte_ptr, word_ptr_type);
    gcc_jit_lvalue *word_lval = gcc_jit_rvalue_dereference(word_ptr, NULL);
    
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
        switch (assign_op) {
            case eok_add_assign:
            case eok_pre_incr:
            case eok_post_incr: jit_op = GCC_JIT_BINARY_OP_PLUS; break;
            case eok_subtract_assign:
            case eok_pre_decr:
            case eok_post_decr: jit_op = GCC_JIT_BINARY_OP_MINUS; break;
            case eok_multiply_assign: jit_op = GCC_JIT_BINARY_OP_MULT; break;
            case eok_divide_assign: jit_op = GCC_JIT_BINARY_OP_DIVIDE; break;
            case eok_remainder_assign: jit_op = GCC_JIT_BINARY_OP_MODULO; break;
            case eok_shiftl_assign: jit_op = GCC_JIT_BINARY_OP_LSHIFT; break;
            case eok_shiftr_assign: jit_op = GCC_JIT_BINARY_OP_RSHIFT; break;
            case eok_and_assign: jit_op = GCC_JIT_BINARY_OP_BITWISE_AND; break;
            case eok_or_assign: jit_op = GCC_JIT_BINARY_OP_BITWISE_OR; break;
            case eok_xor_assign: jit_op = GCC_JIT_BINARY_OP_BITWISE_XOR; break;
            default: break;
        }
        rhs_val = gcc_jit_context_new_binary_op(ctx, loc, jit_op, word_type, current_bf_val, rhs_val);
    }
    
    unsigned long mask_val = (f->bit_size >= sizeof(unsigned long) * 8) ? ~0UL : ((1UL << f->bit_size) - 1UL);
    gcc_jit_rvalue *mask = gcc_jit_context_new_rvalue_from_long(ctx, word_type, (long)mask_val);
    rhs_val = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, word_type, rhs_val, mask);
    
    gcc_jit_rvalue *shifted_rhs = rhs_val;
    if (total_bit_shift > 0) {
        gcc_jit_rvalue *shift = gcc_jit_context_new_rvalue_from_long(ctx, word_type, (long)total_bit_shift);
        shifted_rhs = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, word_type, rhs_val, shift);
    }
    
    unsigned long clear_mask_val = ~(mask_val << total_bit_shift);
    gcc_jit_rvalue *clear_mask = gcc_jit_context_new_rvalue_from_long(ctx, word_type, (long)clear_mask_val);
    
    gcc_jit_rvalue *old_word = gcc_jit_lvalue_as_rvalue(word_lval);
    gcc_jit_rvalue *cleared_word = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, word_type, old_word, clear_mask);
    
    gcc_jit_rvalue *new_word = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_OR, word_type, cleared_word, shifted_rhs);
    if (cblock) {
        gcc_jit_block_add_assignment(cblock, loc, word_lval, new_word);
    }
    
    if (assign_op == eok_post_incr || assign_op == eok_post_decr) {
        *out_rval = current_bf_val;
        return GCC_GEN_BE_SUCCESS;
    }

    *out_rval = rhs_val;
    if (f->bit_field_is_signed) {
        int word_bits = (int)size_of_type(f->type) * 8;
        if (word_bits == 0) word_bits = 32;
        int shift_amt = word_bits - (int)f->bit_size;
        if (shift_amt > 0) {
            gcc_jit_rvalue *s_amt = gcc_jit_context_new_rvalue_from_long(ctx, word_type, (long)shift_amt);
            *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, word_type, *out_rval, s_amt);
            *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, word_type, *out_rval, s_amt);
        }
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Helper to cast a value between pointer and integer.
 * @details Performs a safe pointer-to-int or int-to-pointer cast.
 * @param[in] ctx The libgccjit context.
 * @param[in] val The value to cast.
 * @param[in] dest_type The destination type.
 * @param[out] out_rval The casted value.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t safe_ptr_or_int_cast(gcc_jit_context *ctx, gcc_jit_rvalue *val, gcc_jit_type *dest_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!val || !dest_type || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_type *src_type = gcc_jit_rvalue_get_type(val);
    if (src_type == dest_type) {
        *out_rval = val;
        return GCC_GEN_BE_SUCCESS;
    }
    if (gcc_jit_type_is_pointer(src_type) != NULL && gcc_jit_type_is_pointer(dest_type) != NULL) {
        *out_rval = gcc_jit_context_new_cast(ctx, NULL, val, dest_type);
        return GCC_GEN_BE_SUCCESS;
    }
    if (gcc_jit_type_is_pointer(dest_type) != NULL && gcc_jit_type_is_pointer(src_type) == NULL) {
        if (p_gcc_jit_context_new_bitcast) {
            gcc_jit_type *uptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG);
            gcc_jit_rvalue *uval = (src_type == uptr_type) ? val : gcc_jit_context_new_cast(ctx, NULL, val, uptr_type);
            *out_rval = p_gcc_jit_context_new_bitcast(ctx, NULL, uval, dest_type);
            return GCC_GEN_BE_SUCCESS;
        }
        *out_rval = gcc_jit_context_null(ctx, dest_type);
        return GCC_GEN_BE_SUCCESS;
    }
    if (gcc_jit_type_is_pointer(dest_type) == NULL && gcc_jit_type_is_pointer(src_type) != NULL) {
        if (p_gcc_jit_context_new_bitcast) {
            gcc_jit_type *uptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG);
            gcc_jit_rvalue *uval = p_gcc_jit_context_new_bitcast(ctx, NULL, val, uptr_type);
            *out_rval = (dest_type == uptr_type) ? uval : gcc_jit_context_new_cast(ctx, NULL, uval, dest_type);
            return GCC_GEN_BE_SUCCESS;
        }
    }
    if (gcc_jit_type_is_pointer(dest_type) == NULL && gcc_jit_type_is_pointer(src_type) == NULL &&
        gcc_jit_type_is_struct(dest_type) == NULL && gcc_jit_type_is_struct(src_type) == NULL) {
        *out_rval = gcc_jit_context_new_cast(ctx, NULL, val, dest_type);
        return GCC_GEN_BE_SUCCESS;
    }
    if (p_gcc_jit_context_new_bitcast &&
        gcc_jit_type_is_struct(src_type) == NULL &&
        gcc_jit_type_is_struct(dest_type) == NULL) {
        ssize_t s1 = gcc_jit_type_get_size(src_type);
        ssize_t s2 = gcc_jit_type_get_size(dest_type);
        if (s1 == s2 && s1 > 0) {
            *out_rval = p_gcc_jit_context_new_bitcast(ctx, NULL, val, dest_type);
            return GCC_GEN_BE_SUCCESS;
        }
    }
    *out_rval = gcc_jit_context_new_cast(ctx, NULL, val, dest_type);
    return GCC_GEN_BE_SUCCESS;
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
    if (!con || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

    gcc_jit_rvalue *base_rval = NULL;
    if (con->variant.address.kind == abk_variable) {
        a_variable_ptr var = con->variant.address.variant.variable;
        void *cached = NULL;
        gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached);
        gcc_jit_lvalue *lval = NULL;
        if (err == GCC_GEN_BE_SUCCESS && cached) {
            lval = (gcc_jit_lvalue *)cached;
        } else if (var) {
            extern gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;
            err = gcc_gen_be_lower_global_variable_decl(var, &lval);
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
            if (cache_lookup(GCC_GEN_BE_CACHE_FUNC, innermost_function_scope->variant.routine.ptr, &cached_fn) == GCC_GEN_BE_SUCCESS && cached_fn) {
                lbl_func = (gcc_jit_function *)cached_fn;
            }
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_get_or_create_label_block(con->variant.address.variant.label, lbl_func, &lbl_block));
        gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
        base_rval = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)(uintptr_t)con->variant.address.variant.label);
    } else if (con->variant.address.kind == abk_routine) {
        a_routine_ptr rout = con->variant.address.variant.routine;
        void *cached = NULL;
        gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, rout, &cached);
        gcc_jit_function *func = NULL;
        if (err == GCC_GEN_BE_SUCCESS && cached) {
            func = (gcc_jit_function *)cached;
        } else if (rout) {
            extern gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT;
            err = gcc_gen_be_lower_function_decl(rout, &func);
            if (err != GCC_GEN_BE_SUCCESS) func = NULL;
        }
        if (func) {
            base_rval = gcc_jit_function_get_address(func, NULL);
        }
    } else if (con->variant.address.kind == abk_constant || con->variant.address.kind == abk_temporary) {
        a_constant_ptr inner_con = con->variant.address.variant.constant;
        gcc_jit_type *inner_type = expected_type;
        if (inner_con && inner_con->type) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(inner_con->type, &inner_type));
        }
        gcc_jit_rvalue *inner_val = NULL;
        gcc_gen_be_error_t lower_err = gcc_gen_be_lower_constant_rvalue(inner_con, inner_type, &inner_val);
        if (lower_err != GCC_GEN_BE_SUCCESS) return lower_err;
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (cblock && inner_val && inner_type && con->variant.address.offset == 0) {
            gcc_jit_function *func = gcc_jit_block_get_function(cblock);
            gcc_jit_lvalue *tmp_lval = gcc_jit_function_new_local(func, NULL, inner_type, "abk_const_tmp");
            gcc_jit_block_add_assignment(cblock, NULL, tmp_lval, inner_val);
            base_rval = gcc_jit_lvalue_get_address(tmp_lval, NULL);
        } else if (inner_val && inner_type) {
            char gname[128];
            snprintf(gname, sizeof(gname), "__abk_const_global_%p", (void *)con);
            void *existing_g = NULL;
            gcc_jit_lvalue *g_lval = NULL;
            if (cache_lookup(GCC_GEN_BE_CACHE_VAR, con, &existing_g) == GCC_GEN_BE_SUCCESS && existing_g) {
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

    if (con->variant.address.offset != 0) {
        if (con->variant.address.kind == abk_variable && con->variant.address.variant.variable) {
            a_variable_ptr var = con->variant.address.variant.variable;
            a_type_ptr vtp = var->type;
            while (vtp && vtp->kind == tk_typeref) vtp = vtp->variant.typeref.type;
            void *cached = NULL;
            if (cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached) == GCC_GEN_BE_SUCCESS && cached) {
                gcc_jit_lvalue *var_lval = (gcc_jit_lvalue *)cached;
                gcc_jit_type *lval_type = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(var_lval));
                if (vtp && vtp->kind == tk_array && vtp->variant.array.element_type &&
                    lval_type && gcc_jit_type_is_pointer(lval_type) == NULL &&
                    gcc_jit_type_is_struct(lval_type) == NULL) {
                    long elem_sz = (long)size_of_type(vtp->variant.array.element_type);
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
                            (long)con->variant.address.offset < (long)fld->offset + (long)size_of_type(fld->type)) {
                            void *cached_fld = NULL;
                            if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, fld, &cached_fld) == GCC_GEN_BE_SUCCESS && cached_fld) {
                                gcc_jit_lvalue *fld_lval = gcc_jit_lvalue_access_field(var_lval, NULL, (gcc_jit_field *)cached_fld);
                                long rem_off = (long)con->variant.address.offset - (long)fld->offset;
                                a_type_ptr ftp = fld->type;
                                while (ftp && ftp->kind == tk_typeref) ftp = ftp->variant.typeref.type;
                                if (rem_off == 0) {
                                    return safe_ptr_or_int_cast(ctx, gcc_jit_lvalue_get_address(fld_lval, NULL), expected_type, out_rval);
                                } else if (ftp && ftp->kind == tk_array && ftp->variant.array.element_type) {
                                    long elem_sz = (long)size_of_type(ftp->variant.array.element_type);
                                    if (elem_sz > 0 && (rem_off % elem_sz) == 0) {
                                        gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                                        gcc_jit_rvalue *idx_rval = gcc_jit_context_new_rvalue_from_long(ctx, long_type, rem_off / elem_sz);
                                        gcc_jit_lvalue *elem_lval = gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(fld_lval), idx_rval);
                                        return safe_ptr_or_int_cast(ctx, gcc_jit_lvalue_get_address(elem_lval, NULL), expected_type, out_rval);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
        gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
        gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);

        gcc_jit_rvalue *byte_base = NULL;
        GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, base_rval, char_ptr_type, &byte_base));
        gcc_jit_rvalue *off_rval = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)con->variant.address.offset);
        gcc_jit_lvalue *elem_lval = gcc_jit_context_new_array_access(ctx, NULL, byte_base, off_rval);
        gcc_jit_rvalue *offset_addr = gcc_jit_lvalue_get_address(elem_lval, NULL);

        GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, offset_addr, expected_type, out_rval));
    } else {
        GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, base_rval, expected_type, out_rval));
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers an aggregate constant.
 *
 * @details Processes a constant aggregate into a gcc_jit_rvalue.
 *
 * @param[in] con The frontend constant.
 * @param[in] type The expected gcc_jit_type.
 * @param[out] out_rval Pointer to receive the resulting rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t lower_aggregate_constant(a_constant_ptr con, gcc_jit_type *type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    a_type_ptr con_tp = con ? con->type : NULL;
    while (con_tp && con_tp->kind == tk_typeref) con_tp = con_tp->variant.typeref.type;

    if (con_tp && con_tp->kind == tk_array) {
        size_t count = 0;
        for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
            if (c->kind == ck_designator) c = c->next;
            count++;
        }
        
        gcc_jit_rvalue **vals = NULL;
        if (count > 0) {
            vals = (gcc_jit_rvalue **)malloc(count * sizeof(gcc_jit_rvalue *));
            size_t idx = 0;
            for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
                if (c->kind == ck_designator) c = c->next;
                gcc_jit_type *elem_type = NULL;
                err = gcc_gen_be_lower_type(con_tp->variant.array.element_type, &elem_type);
                if (err == GCC_GEN_BE_SUCCESS) {
                    err = gcc_gen_be_lower_constant_rvalue(c, elem_type, &vals[idx]);
                    if (err != GCC_GEN_BE_SUCCESS) {
                        free(vals);
                        return err;
                    }
                } else {
                    vals[idx] = NULL;
                }
                idx++;
            }
        }
        *out_rval = gcc_jit_context_new_array_constructor(ctx, NULL, type, count, vals);
        if (vals) free(vals);
    } else {
        size_t count = 0;
        {
            a_field_ptr f_scan = NULL;
            if (con_tp && (con_tp->kind == tk_struct || con_tp->kind == tk_union || con_tp->kind == tk_class)) {
                f_scan = con_tp->variant.class_struct_union.field_list;
            }
            for (a_constant_ptr c = con->variant.aggregate.first_constant; c; c = c->next) {
                if (c->kind == ck_designator) {
                    if (c->variant.designator.is_field_designator && !c->variant.designator.is_generic) {
                        f_scan = c->variant.designator.variant.field;
                    }
                    c = c->next;
                }
                count++;
                if (f_scan) f_scan = f_scan->next;
            }
        }
        
        gcc_jit_field **fields = NULL;
        gcc_jit_rvalue **vals = NULL;
        gcc_jit_type *struct_type = type;
        if (con_tp && (con_tp->kind == tk_struct || con_tp->kind == tk_union || con_tp->kind == tk_class)) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(con_tp, &struct_type));
        }
        if (count > 0) {
            fields = (gcc_jit_field **)malloc(count * sizeof(gcc_jit_field *));
            vals = (gcc_jit_rvalue **)malloc(count * sizeof(gcc_jit_rvalue *));
            size_t idx = 0;
            a_base_class_ptr b_cur = NULL;
            a_field_ptr f = NULL;
            if (con_tp && (con_tp->kind == tk_struct || con_tp->kind == tk_union || con_tp->kind == tk_class)) {
                if (con_tp->variant.class_struct_union.extra_info) {
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
                }
                
                gcc_jit_field *jit_f = NULL;
                gcc_jit_type *f_type = NULL;
                a_boolean skip_unmapped_empty = FALSE;
                if (b_cur && val_c && val_c->kind == ck_aggregate) {
                    void *cached_bf = NULL;
                    if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, b_cur, &cached_bf) == GCC_GEN_BE_SUCCESS && cached_bf) {
                        jit_f = (gcc_jit_field *)cached_bf;
                    } else if (b_cur->is_optimized_empty_base) {
                        skip_unmapped_empty = TRUE;
                    }
                    if (b_cur->type) {
                        err = gcc_gen_be_lower_type(b_cur->type, &f_type);
                    }
                } else {
                    if (b_cur) {
                        b_cur = NULL;
                        if (con_tp && (con_tp->kind == tk_struct || con_tp->kind == tk_union || con_tp->kind == tk_class)) {
                            f = con_tp->variant.class_struct_union.field_list;
                        }
                    }
                    if (f) {
                        void *cached = NULL;
                        if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, f, &cached) == GCC_GEN_BE_SUCCESS && cached) {
                            jit_f = (gcc_jit_field *)cached;
                        } else if (f->is_optimized_empty_class) {
                            skip_unmapped_empty = TRUE;
                        }
                        err = gcc_gen_be_lower_type(f->type, &f_type);
                    }
                }
                if (skip_unmapped_empty) {
                    count--;
                    if (b_cur) {
                        b_cur = b_cur->next;
                        while (b_cur && b_cur->is_virtual) b_cur = b_cur->next;
                        if (!b_cur && con_tp && (con_tp->kind == tk_struct || con_tp->kind == tk_union || con_tp->kind == tk_class)) {
                            f = con_tp->variant.class_struct_union.field_list;
                        }
                    } else if (f) {
                        f = f->next;
                    }
                    continue;
                }
                if (!jit_f && val_c && val_c->kind == ck_aggregate && val_c->variant.aggregate.field_or_base.base != NULL) {
                    a_base_class_ptr desig_bcp = val_c->variant.aggregate.field_or_base.base;
                    void *cached_bf = NULL;
                    if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, desig_bcp, &cached_bf) == GCC_GEN_BE_SUCCESS && cached_bf) {
                        jit_f = (gcc_jit_field *)cached_bf;
                    }
                    if (jit_f && desig_bcp->type) {
                        err = gcc_gen_be_lower_type(desig_bcp->type, &f_type);
                    }
                }
                if (!jit_f && struct_type && gcc_jit_type_is_struct(struct_type) != NULL) {
                    gcc_jit_struct *st = gcc_jit_type_is_struct(struct_type);
                    if (idx < (size_t)gcc_jit_struct_get_field_count(st)) {
                        jit_f = gcc_jit_struct_get_field(st, idx);
                    }
                }
                if (!f_type && val_c && val_c->type) { err = gcc_gen_be_lower_type(val_c->type, &f_type); }
                if (!f_type && jit_f) {
                    /* Infer f_type when val_c has no explicit type */
                    f_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                }
                if (!f_type) f_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

                if (f && (f->is_bit_field || f->bit_size > 0) && val_c && val_c->kind == ck_integer) {
                    /* Pack all adjacent bitfield constants sharing the same container field `jit_f` */
                    unsigned long unit_sz = (unsigned long)gcc_jit_type_get_size(f_type);
                    if (unit_sz == 0) unit_sz = (unsigned long)size_of_type(f->type);
                    if (unit_sz == 0) unit_sz = 4;
                    if (unit_sz < 8UL &&
                        ((unsigned long)f->offset_bit_remainder + (unsigned long)f->bit_size) > (unit_sz * 8UL) &&
                        con_tp && (f->offset + 8UL) <= con_tp->size) {
                        f_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
                        unit_sz = 8UL;
                    }
                    unsigned long container_base_off = ((unsigned long)f->offset / unit_sz) * unit_sz;
                    unsigned long bf_vals[64];
                    unsigned int bf_shifts[64];
                    unsigned int bf_widths[64];
                    int bf_count = 0;
                    for (;;) {
                        a_boolean bf_ovf = FALSE;
                        unsigned long bf_val = (unsigned long)unsigned_value_of_integer_constant(val_c, &bf_ovf);
                        unsigned int bit_shift = (unsigned int)(((unsigned long)f->offset - container_base_off) * 8UL) +
                                                 (unsigned int)f->offset_bit_remainder;
                        if (bf_count < 64) {
                            bf_vals[bf_count] = bf_val;
                            bf_shifts[bf_count] = bit_shift;
                            bf_widths[bf_count] = (unsigned int)f->bit_size;
                            bf_count++;
                        }

                        a_constant_ptr next_c = c->next;
                        a_field_ptr next_f = f->next;
                        if (next_c && next_c->kind == ck_designator) {
                            if (next_c->variant.designator.is_field_designator && !next_c->variant.designator.is_generic) {
                                next_f = next_c->variant.designator.variant.field;
                            }
                            next_c = next_c->next;
                        }
                        void *next_cached = NULL;
                        if (next_f && (next_f->is_bit_field || next_f->bit_size > 0) && next_c && next_c->kind == ck_integer &&
                            cache_lookup(GCC_GEN_BE_CACHE_FIELD, next_f, &next_cached) == GCC_GEN_BE_SUCCESS &&
                            (gcc_jit_field *)next_cached == jit_f) {
                            c = (c->next && c->next->kind == ck_designator) ? c->next->next : c->next;
                            val_c = c;
                            f = next_f;
                            count--;
                        } else {
                            break;
                        }
                    }
                    unsigned long combined_word = 0UL;
                    GCC_GEN_BE_CHECK(gcc_gen_be_pack_bitfield_constants(bf_vals, bf_shifts, bf_widths, bf_count, &combined_word));
                    fields[idx] = jit_f;
                    vals[idx] = gcc_jit_context_new_rvalue_from_long(ctx, f_type, (long)combined_word);
                    idx++;
                    f = f->next;
                } else {
                    fields[idx] = jit_f;
                    a_constant_ptr inner_val_c = val_c;
                    while (inner_val_c && inner_val_c->kind == ck_aggregate &&
                           !b_cur && inner_val_c->variant.aggregate.field_or_base.base == NULL &&
                           inner_val_c->variant.aggregate.first_constant && !inner_val_c->variant.aggregate.first_constant->next &&
                           f_type && gcc_jit_type_is_pointer(f_type) != NULL) {
                        inner_val_c = inner_val_c->variant.aggregate.first_constant;
                    }
                    err = gcc_gen_be_lower_constant_rvalue(inner_val_c, f_type, &vals[idx]);
                    if (vals[idx] && f_type && gcc_jit_rvalue_get_type(vals[idx]) != f_type) {
                        if (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(vals[idx])) != NULL &&
                            gcc_jit_type_is_pointer(f_type) != NULL) {
                            vals[idx] = gcc_jit_context_new_cast(ctx, NULL, vals[idx], f_type);
                        } else if (gcc_jit_type_is_struct(f_type) != NULL &&
                                   gcc_jit_struct_get_field_count(gcc_jit_type_is_struct(f_type)) == 1) {
                            gcc_jit_struct *sub_st = gcc_jit_type_is_struct(f_type);
                            gcc_jit_field *sub_f0 = gcc_jit_struct_get_field(sub_st, 0);
                            if (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(vals[idx])) != NULL &&
                                con_tp && (con_tp->kind == tk_struct || con_tp->kind == tk_class)) {
                                a_field_ptr sub_edg_f = NULL;
                                if (b_cur && b_cur->type && (b_cur->type->kind == tk_struct || b_cur->type->kind == tk_class)) {
                                    sub_edg_f = b_cur->type->variant.class_struct_union.field_list;
                                } else if (f && f->type && (f->type->kind == tk_struct || f->type->kind == tk_class)) {
                                    sub_edg_f = f->type->variant.class_struct_union.field_list;
                                }
                                if (sub_edg_f && sub_edg_f->type) {
                                    gcc_jit_type *sub_f0_type = NULL;
                                    if (gcc_gen_be_lower_type(sub_edg_f->type, &sub_f0_type) == GCC_GEN_BE_SUCCESS &&
                                        sub_f0_type && gcc_jit_rvalue_get_type(vals[idx]) != sub_f0_type) {
                                        vals[idx] = gcc_jit_context_new_cast(ctx, NULL, vals[idx], sub_f0_type);
                                    }
                                }
                            }
                            vals[idx] = gcc_jit_context_new_struct_constructor(ctx, NULL, f_type, 1, &sub_f0, &vals[idx]);
                        } else if (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(vals[idx])) != NULL ||
                                   gcc_jit_type_is_pointer(f_type) != NULL) {
                            vals[idx] = gcc_jit_context_new_cast(ctx, NULL, vals[idx], f_type);
                        } else if (val_c && val_c->kind == ck_aggregate && val_c->variant.aggregate.first_constant &&
                                   !val_c->variant.aggregate.first_constant->next) {
                            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(val_c->variant.aggregate.first_constant, f_type, &vals[idx]));
                        }
                    }
                    idx++;
                    if (b_cur) {
                        b_cur = b_cur->next;
                        while (b_cur && b_cur->is_virtual) b_cur = b_cur->next;
                        if (!b_cur && con_tp && (con_tp->kind == tk_struct || con_tp->kind == tk_union || con_tp->kind == tk_class)) {
                            f = con_tp->variant.class_struct_union.field_list;
                        }
                    } else if (f) {
                        f = f->next;
                    }
                }
            }
        }
        
        if (con_tp && con_tp->kind == tk_union) {
            if (count > 0 && fields[0] && vals[0]) {
                *out_rval = gcc_jit_context_new_union_constructor(ctx, NULL, struct_type ? struct_type : type, fields[0], vals[0]);
            } else {
                err = gcc_gen_be_get_zero_rvalue(con_tp, struct_type ? struct_type : type, out_rval);
            }
        } else if ((con_tp && (con_tp->kind == tk_struct || con_tp->kind == tk_class)) ||
                   (!con_tp && struct_type && gcc_jit_type_is_struct(struct_type) != NULL)) {
            if (!struct_type) struct_type = type;
            if (struct_type && gcc_jit_type_is_struct(struct_type) != NULL) {
                gcc_jit_struct *st = gcc_jit_type_is_struct(struct_type);
                size_t st_fcount = (size_t)gcc_jit_struct_get_field_count(st);
                if (count > 0 && st_fcount == 1 && con_tp &&
                    (con_tp->kind == tk_struct || con_tp->kind == tk_class) &&
                    con_tp->variant.class_struct_union.extra_info &&
                    con_tp->variant.class_struct_union.extra_info->direct_base_classes &&
                    !con_tp->variant.class_struct_union.field_list) {
                    a_base_class_ptr b0 = con_tp->variant.class_struct_union.extra_info->direct_base_classes;
                    gcc_jit_type *b0_type = NULL;
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(b0->type, &b0_type));
                    if (b0_type && gcc_jit_type_is_struct(b0_type) != NULL &&
                        (size_t)gcc_jit_struct_get_field_count(gcc_jit_type_is_struct(b0_type)) == count &&
                        vals[0] && gcc_jit_rvalue_get_type(vals[0]) != b0_type) {
                        gcc_jit_struct *b0_st = gcc_jit_type_is_struct(b0_type);
                        for (size_t bi = 0; bi < count; ++bi) {
                            fields[bi] = gcc_jit_struct_get_field(b0_st, bi);
                        }
                        if (count == 1 && vals[0] &&
                            gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(vals[0])) != NULL &&
                            b0->type && (b0->type->kind == tk_struct || b0->type->kind == tk_class) &&
                            b0->type->variant.class_struct_union.field_list) {
                            a_field_ptr bf0 = b0->type->variant.class_struct_union.field_list;
                            if (bf0 && bf0->type) {
                                gcc_jit_type *bf0_type = NULL;
                                if (gcc_gen_be_lower_type(bf0->type, &bf0_type) == GCC_GEN_BE_SUCCESS &&
                                    bf0_type && gcc_jit_rvalue_get_type(vals[0]) != bf0_type) {
                                    vals[0] = gcc_jit_context_new_cast(ctx, NULL, vals[0], bf0_type);
                                }
                            }
                        }
                        gcc_jit_rvalue *b0_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, b0_type, count, fields, vals);
                        gcc_jit_field *outer_f0 = gcc_jit_struct_get_field(st, 0);
                        *out_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, struct_type, 1, &outer_f0, &b0_rval);
                        free(fields);
                        free(vals);
                        return GCC_GEN_BE_SUCCESS;
                    }
                }
            }
            gcc_jit_rvalue *ctor_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, struct_type, count, fields, vals);
            if (type && struct_type != type && gcc_jit_type_is_struct(type) != NULL) {
                gcc_jit_struct *outer_s = gcc_jit_type_is_struct(type);
                if (gcc_jit_struct_get_field_count(outer_s) == 1) {
                    gcc_jit_field *outer_f0 = gcc_jit_struct_get_field(outer_s, 0);
                    ctor_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, type, 1, &outer_f0, &ctor_rval);
                }
            }
            *out_rval = ctor_rval;
        } else {
            if (count == 1 && vals && vals[0]) {
                *out_rval = vals[0];
            } else {
                err = gcc_gen_be_get_zero_rvalue(con ? con->type : NULL, type, out_rval);
            }
        }
        
        if (fields) free(fields);
        if (vals) free(vals);
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!out_rval || !con) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    if (con->kind == ck_integer) {
        a_type_ptr ctype = con->type;
        while (ctype && ctype->kind == tk_typeref) ctype = ctype->variant.typeref.type;
#if INT128_EXTENSIONS_ALLOWED
        if (ctype && ctype->kind == tk_integer &&
            (ctype->variant.integer.int_kind == ik_int128 || ctype->variant.integer.int_kind == ik_unsigned_int128)) {
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
            gcc_jit_type *target_128 = expected_type ? expected_type : ulong_type;
            gcc_jit_rvalue *lo_rval = gcc_jit_context_new_rvalue_from_long(ctx, ulong_type, (long)lo_u);
            lo_rval = gcc_jit_context_new_cast(ctx, NULL, lo_rval, target_128);
            if (hi_u != 0) {
                gcc_jit_rvalue *hi_rval = gcc_jit_context_new_rvalue_from_long(ctx, ulong_type, (long)hi_u);
                hi_rval = gcc_jit_context_new_cast(ctx, NULL, hi_rval, target_128);
                gcc_jit_rvalue *shift_64 = gcc_jit_context_new_rvalue_from_int(ctx, target_128, 64);
                gcc_jit_rvalue *hi_shifted = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, target_128, hi_rval, shift_64);
                *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_OR, target_128, hi_shifted, lo_rval);
            } else {
                *out_rval = lo_rval;
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
        if (val == 0) {
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
        if (expected_type && gcc_jit_type_is_pointer(expected_type) != NULL) {
            gcc_jit_rvalue *str_lit = gcc_jit_context_new_string_literal(ctx, (const char *)con->variant.string.value);
            *out_rval = gcc_jit_context_new_cast(ctx, NULL, str_lit, expected_type);
        } else if (con->type && con->type->kind == tk_array) {
            a_type_ptr elem_type_ptr = con->type->variant.array.element_type;
            gcc_jit_type *elem_type = NULL;
            err = gcc_gen_be_lower_type(elem_type_ptr, &elem_type);
            long array_elems = (long)con->type->variant.array.variant.number_of_elements;
            
            size_t bytes_per_elem = 1;
            if (elem_type_ptr->kind == tk_integer) bytes_per_elem = (size_t)size_of_type(elem_type_ptr);
            
            size_t num_values = array_elems > 0 ? (size_t)array_elems : ((size_t)con->variant.string.length / bytes_per_elem);
            gcc_jit_rvalue **vals = (gcc_jit_rvalue **)malloc(num_values * sizeof(gcc_jit_rvalue *));
            
            const unsigned char *bytes = (const unsigned char *)con->variant.string.value;
            size_t str_len_bytes = (size_t)con->variant.string.length;
            
            for (size_t i = 0; i < num_values; ++i) {
                long long val = 0;
                size_t offset = i * bytes_per_elem;
                if (offset < str_len_bytes) {
                    for (size_t b = 0; b < bytes_per_elem && (offset + b) < str_len_bytes; ++b) {
                        val |= ((long long)bytes[offset + b]) << (b * 8);
                    }
                }
                vals[i] = gcc_jit_context_new_rvalue_from_long(ctx, elem_type, (long)val);
            }
            gcc_jit_type *arr_type = expected_type;
            if (!arr_type) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(con->type, &arr_type));
            }
            *out_rval = gcc_jit_context_new_array_constructor(ctx, NULL, arr_type, num_values, vals);
            free(vals);
        } else {
            *out_rval = gcc_jit_context_new_string_literal(ctx, (const char *)con->variant.string.value);
        }
    } else if (con->kind == ck_aggregate) {
        err = lower_aggregate_constant(con, expected_type, out_rval);
        if (err != GCC_GEN_BE_SUCCESS) *out_rval = NULL;
    } else if (con->kind == ck_address) {
        err = lower_address_constant(con, expected_type, out_rval);
        if (err != GCC_GEN_BE_SUCCESS) *out_rval = NULL;
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
              }
              GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(expr->type, res_type, out_rval));
              return GCC_GEN_BE_SUCCESS;
          }

      case enk_statement:
          {
              gcc_jit_type *res_type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &res_type));
              
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              
              gcc_jit_lvalue *res_lval = gcc_jit_function_new_local(func, NULL, res_type, "stmt_expr_res");
              
              extern struct gcc_jit_lvalue *current_stmt_expr_result_lval;
              struct gcc_jit_lvalue *old_res_lval = current_stmt_expr_result_lval;
              current_stmt_expr_result_lval = res_lval;
              
              extern gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(expr->variant.statement, func));
              
              current_stmt_expr_result_lval = old_res_lval;
              
              *out_rval = gcc_jit_lvalue_as_rvalue(res_lval);
              return GCC_GEN_BE_SUCCESS;
          }
      case enk_constant:
          {
              gcc_jit_type *type = NULL;
              err = gcc_gen_be_lower_type(expr->type, &type);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              return gcc_gen_be_lower_constant_rvalue(expr->variant.constant.ptr, type, out_rval);
          }
          
      case enk_variable:
          {
              a_variable_ptr var = expr->variant.variable.ptr;
              a_type_ptr etype = expr->type;
              while (etype && etype->kind == tk_typeref) etype = etype->variant.typeref.type;
              if (var && !var->source_corresp.name && var->storage_class == sc_static &&
                  var->init_kind == initk_static && var->initializer.constant &&
                  !(etype && etype->kind == tk_array)) {
                  gcc_jit_type *vtype = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &vtype));
                  return gcc_gen_be_lower_constant_rvalue(var->initializer.constant, vtype, out_rval);
              }
              gcc_jit_lvalue *lval = NULL;
              err = gcc_gen_be_lower_expr_lvalue(expr, &lval);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              if (lval) {
                  if (etype && etype->kind == tk_array) {
                      gcc_jit_type *elem_type = NULL;
                      err = gcc_gen_be_lower_type(etype->variant.array.element_type, &elem_type);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                      *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(lval, NULL), elem_ptr_type);
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
              err = gcc_gen_be_lower_type(expr->type, &type);
              if (err != GCC_GEN_BE_SUCCESS) return err;

              if (op == eok_dot_field || op == eok_points_to_field || op == eok_subscript || op == eok_indirect || op == eok_ref_indirect) {
                  if ((op == eok_dot_field || op == eok_points_to_field) &&
                      op2 && op2->kind == enk_field && op2->variant.field.ptr &&
                      (op2->variant.field.ptr->is_bit_field || op2->variant.field.ptr->bit_size > 0)) {
                      return gcc_gen_be_lower_bitfield_read(expr, out_rval);
                  }
                  gcc_jit_lvalue *lval = NULL;
                  err = gcc_gen_be_lower_expr_lvalue(expr, &lval);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  if (lval) {
                      a_type_ptr etype = expr->type;
                      while (etype && etype->kind == tk_typeref) etype = etype->variant.typeref.type;
                      if (etype && etype->kind == tk_array) {
                          gcc_jit_type *elem_type = NULL;
                          err = gcc_gen_be_lower_type(etype->variant.array.element_type, &elem_type);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                          *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(lval, NULL), elem_ptr_type);
                      } else {
                          *out_rval = gcc_jit_lvalue_as_rvalue(lval);
                      }
                      return GCC_GEN_BE_SUCCESS;
                  }
              }
              
              if (op == eok_comma) {
                  gcc_jit_rvalue *r1 = NULL;
                  if (op1) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op1, &r1));
                  }
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (r1 && cblock && gcc_jit_rvalue_get_type(r1) != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                      gcc_jit_block_add_eval(cblock, loc, r1);
                  }
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
                  err = gcc_gen_be_lower_expr_rvalue(op1, &r1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
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
                  if (op1 && op1->kind == enk_operation &&
                      (op1->variant.operation.kind == eok_dot_field || op1->variant.operation.kind == eok_points_to_field) &&
                      op1->variant.operation.operands && op1->variant.operation.operands->next &&
                      op1->variant.operation.operands->next->kind == enk_field &&
                      op1->variant.operation.operands->next->variant.field.ptr &&
                      (op1->variant.operation.operands->next->variant.field.ptr->is_bit_field ||
                       op1->variant.operation.operands->next->variant.field.ptr->bit_size > 0)) {
                      return gcc_gen_be_lower_bitfield_assignment(op1, op, op2, out_rval);
                  }
                  gcc_jit_lvalue *l1 = NULL;
                  err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_rvalue *r2 = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(op2, &r2);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (cblock) {
                      if (op == eok_assign || op == eok_bassign) {
                          if (l1 && r2) {
                              gcc_jit_type *l1_t = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(l1));
                              gcc_jit_type *r2_t = gcc_jit_rvalue_get_type(r2);
                              if (l1_t && r2_t && l1_t != r2_t) {
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
                          gcc_jit_rvalue *neg_r2 = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, gcc_jit_rvalue_get_type(r2), r2);
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, gcc_jit_lvalue_as_rvalue(l1), neg_r2);
                          gcc_jit_rvalue *new_ptr = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          gcc_jit_block_add_assignment(cblock, loc, l1, new_ptr);
                      } else {
                          enum gcc_jit_binary_op jit_op;
                          switch (op) {
                              case eok_add_assign: jit_op = GCC_JIT_BINARY_OP_PLUS; break;
                              case eok_subtract_assign: jit_op = GCC_JIT_BINARY_OP_MINUS; break;
                              case eok_multiply_assign: jit_op = GCC_JIT_BINARY_OP_MULT; break;
                              case eok_divide_assign: jit_op = GCC_JIT_BINARY_OP_DIVIDE; break;
                              case eok_remainder_assign: jit_op = GCC_JIT_BINARY_OP_MODULO; break;
                              case eok_shiftl_assign: jit_op = GCC_JIT_BINARY_OP_LSHIFT; break;
                              case eok_shiftr_assign: jit_op = GCC_JIT_BINARY_OP_RSHIFT; break;
                              case eok_and_assign: jit_op = GCC_JIT_BINARY_OP_BITWISE_AND; break;
                              case eok_or_assign: jit_op = GCC_JIT_BINARY_OP_BITWISE_OR; break;
                              case eok_xor_assign: jit_op = GCC_JIT_BINARY_OP_BITWISE_XOR; break;
                              default: return GCC_GEN_BE_ERROR_INTERNAL;
                          }
                          gcc_jit_block_add_assignment_op(cblock, NULL, l1, jit_op, r2);
                      }
                  }
                  *out_rval = gcc_jit_lvalue_as_rvalue(l1);
                  return GCC_GEN_BE_SUCCESS;
              }
              
              gcc_jit_rvalue *r1 = NULL;
              if (op1 && op != eok_address_of &&
                  !(op == eok_call || op == eok_dot_member_call || op == eok_points_to_member_call)) {
                  err = gcc_gen_be_lower_expr_rvalue(op1, &r1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
              } else if (op1 && (op == eok_call || op == eok_dot_member_call || op == eok_points_to_member_call) &&
                         op1->kind != enk_routine) {
                  err = gcc_gen_be_lower_expr_rvalue(op1, &r1);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
              }
              
              gcc_jit_rvalue *r2 = NULL;
              if (op2 && !(op == eok_call || op == eok_dot_member_call || op == eok_points_to_member_call)) {
                  err = gcc_gen_be_lower_expr_rvalue(op2, &r2);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
              }
              
              switch (op) {
                  case eok_dynamic_cast:
                      err = gcc_gen_be_lower_dynamic_cast(expr, out_rval);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      return GCC_GEN_BE_SUCCESS;
                  case eok_padd:
                      {
                          a_type_ptr op1_t = op1 ? op1->type : NULL;
                          while (op1_t && op1_t->kind == tk_typeref) op1_t = op1_t->variant.typeref.type;
                          a_boolean op1_is_ptr = op1_t && (op1_t->kind == tk_pointer || op1_t->kind == tk_array);
                          gcc_jit_rvalue *ptr_val = op1_is_ptr ? r1 : r2;
                          gcc_jit_rvalue *int_val = op1_is_ptr ? r2 : r1;
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, ptr_val, int_val);
                          *out_rval = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_psubtract:
                      {
                          gcc_jit_rvalue *neg_r2 = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, gcc_jit_rvalue_get_type(r2), r2);
                          gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, r1, neg_r2);
                          *out_rval = gcc_jit_lvalue_get_address(arr_acc, NULL);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_pdiff:
                      {
                          gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                          gcc_jit_rvalue *c1 = gcc_jit_context_new_cast(ctx, loc, r1, long_type);
                          gcc_jit_rvalue *c2 = gcc_jit_context_new_cast(ctx, loc, r2, long_type);
                          gcc_jit_rvalue *diff = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, long_type, c1, c2);
                          
                          a_type_ptr elem_type = op1->type->variant.pointer.type;
                          long sz = (long)size_of_type(elem_type);
                          gcc_jit_rvalue *sz_val = gcc_jit_context_new_rvalue_from_long(ctx, long_type, sz);
                          
                          *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_DIVIDE, long_type, diff, sz_val);
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

                          a_type_ptr op_t = op1->type;
                          while (op_t && op_t->kind == tk_typeref) op_t = op_t->variant.typeref.type;
                          if (op_t && (op_t->kind == tk_float)) {
                              gcc_jit_rvalue *cmp_ne = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, r1, r2);
                              gcc_jit_rvalue *cmp_unordered = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LOGICAL_AND, int_type,
                                  gcc_jit_context_new_cast(ctx, loc, cmp_ne, int_type),
                                  gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, int_type, res_val));

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
                  #if defined(LIBGCCJIT_HAVE_gcc_jit_context_new_bitcast)
                              *out_rval = gcc_jit_context_new_bitcast(ctx, NULL, schar_val, type);
                  #else
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
                  #endif
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
                              gcc_jit_type *c_type = NULL; err = gcc_gen_be_lower_type(expr->type, &c_type); if (err != GCC_GEN_BE_SUCCESS) return err;
                              gcc_jit_type *elem_type = NULL;
                              a_type_ptr op_t2 = expr->type;
while (op_t2 && op_t2->kind == tk_typeref) op_t2 = op_t2->variant.typeref.type;
if (op_t2->variant.float_kind == fk_float) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                              else if (op_t2->variant.float_kind == fk_long_double) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                              else elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                              
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              
                              gcc_jit_rvalue *a = NULL, *b = NULL, *c = NULL, *d = NULL;
                              GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, func, cblock, r1, c_type, elem_type, &a, &b));
                              GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, func, cblock, r2, c_type, elem_type, &c, &d));
                              
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
                              } else if (op == eok_divide) {
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
                              
                              if (gcc_jit_type_is_struct(c_type)) {
                                  gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                                  gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_res");
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), res_r);
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), res_i);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                              } else {
                                  gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_res");
                                  gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                                  gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                                  gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                                  
                                  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                  gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                                  gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                                  
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, zero), res_r);
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, one), res_i);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                              }
                              return GCC_GEN_BE_SUCCESS;
                          } else {
                              enum gcc_jit_binary_op jit_op = GCC_JIT_BINARY_OP_PLUS;
                              if (op == eok_subtract) jit_op = GCC_JIT_BINARY_OP_MINUS;
                              else if (op == eok_multiply) jit_op = GCC_JIT_BINARY_OP_MULT;
                              else if (op == eok_divide) jit_op = GCC_JIT_BINARY_OP_DIVIDE;
                              *out_rval = gcc_jit_context_new_binary_op(ctx, loc, jit_op, type, r1, r2);
                              return GCC_GEN_BE_SUCCESS;
                          }
                      }
                  case eok_remainder: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MODULO, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_shiftl: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_LSHIFT, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_shiftr: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_RSHIFT, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_comma:
                      {
                          gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          if (r1 && cblock) gcc_jit_block_add_eval(cblock, loc, r1);
                          *out_rval = r2;
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_and: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_or: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_OR, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_xor: *out_rval = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_XOR, type, r1, r2); return GCC_GEN_BE_SUCCESS;
                      
                  case eok_eq:
                  case eok_ne:
                      {
                          a_boolean op1_is_cplx = FALSE;
                          if (op1) GCC_GEN_BE_CHECK(check_is_complex_type(op1->type, &op1_is_cplx));
                          if (op1 && op1_is_cplx) {
                              gcc_jit_type *c_type = gcc_jit_rvalue_get_type(r1);
                              gcc_jit_type *elem_type = NULL;
                              a_type_ptr op1_t2 = op1->type;
while (op1_t2 && op1_t2->kind == tk_typeref) op1_t2 = op1_t2->variant.typeref.type;
if (op1_t2->variant.float_kind == fk_float) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                              else if (op1_t2->variant.float_kind == fk_long_double) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                              else elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                              
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              
                              gcc_jit_rvalue *a = NULL, *b = NULL, *c = NULL, *d = NULL;
                              GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, func, cblock, r1, c_type, elem_type, &a, &b));
                              GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, func, cblock, r2, c_type, elem_type, &c, &d));
                              
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
                          } else {
                              *out_rval = gcc_jit_context_new_comparison(ctx, loc, op == eok_eq ? GCC_JIT_COMPARISON_EQ : GCC_JIT_COMPARISON_NE, r1, r2);
                              return GCC_GEN_BE_SUCCESS;
                          }
                      }
                  case eok_lt: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_LT, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_le: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_LE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_gt: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_GT, r1, r2); return GCC_GEN_BE_SUCCESS;
                  case eok_ge: *out_rval = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_GE, r1, r2); return GCC_GEN_BE_SUCCESS;
                  
                  case eok_fjadd:
                  case eok_jfadd:
                  case eok_fjsubtract:
                  case eok_jfsubtract:
                      {
                          gcc_jit_type *c_type = NULL; err = gcc_gen_be_lower_type(expr->type, &c_type); if (err != GCC_GEN_BE_SUCCESS) return err;
                          gcc_jit_type *elem_type = NULL;
                          a_type_ptr op_t2 = expr->type;
while (op_t2 && op_t2->kind == tk_typeref) op_t2 = op_t2->variant.typeref.type;
if (op_t2->variant.float_kind == fk_float) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                          else if (op_t2->variant.float_kind == fk_long_double) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                          else elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                          
                          gcc_jit_rvalue *real_val = (op == eok_fjadd || op == eok_fjsubtract) ? r1 : r2;
                          gcc_jit_rvalue *imag_val = (op == eok_fjadd || op == eok_fjsubtract) ? r2 : r1;
                          
                          if (op == eok_fjsubtract) {
                              imag_val = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, imag_val);
                          } else if (op == eok_jfsubtract) {
                              real_val = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, real_val);
                          }
                          
                          gcc_jit_block *cblock = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                          
                          if (gcc_jit_type_is_struct(c_type)) {
                              gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                              gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                              gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), real_val);
                              gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), imag_val);
                              *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                          } else {
                              gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                              gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                              gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                              gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                              
                              gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                              gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                              gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                              
                              gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, zero), real_val);
                              gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, one), imag_val);
                              *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                          }
                          return GCC_GEN_BE_SUCCESS;
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
                              gcc_jit_type *c_type = NULL; err = gcc_gen_be_lower_type(expr->type, &c_type); if (err != GCC_GEN_BE_SUCCESS) return err;
                              gcc_jit_type *elem_type = NULL;
                              a_type_ptr op_t2 = expr->type;
while (op_t2 && op_t2->kind == tk_typeref) op_t2 = op_t2->variant.typeref.type;
if (op_t2->variant.float_kind == fk_float) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                              else if (op_t2->variant.float_kind == fk_long_double) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                              else elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                              
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              
                              gcc_jit_rvalue *a = NULL, *b = NULL;
                              GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, func, cblock, r1, c_type, elem_type, &a, &b));
                              
                              gcc_jit_rvalue *neg_a = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, a);
                              gcc_jit_rvalue *neg_b = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, b);
                              
                              if (gcc_jit_type_is_struct(c_type)) {
                                  gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                                  gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_res");
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), neg_a);
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), neg_b);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                              } else {
                                  gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_res");
                                  gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                                  gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                                  gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                                  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0)), neg_a);
                                  gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1)), neg_b);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                              }
                              return GCC_GEN_BE_SUCCESS;
                          } else {
                              *out_rval = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, type, r1);
                              return GCC_GEN_BE_SUCCESS;
                          }
                      }
                  case eok_complement: *out_rval = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_BITWISE_NEGATE, type, r1); return GCC_GEN_BE_SUCCESS;
                  case eok_not:
                      if (r1 && gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(r1)) != NULL) {
                          gcc_jit_rvalue *cmp = gcc_jit_context_new_comparison(
                              ctx, loc, GCC_JIT_COMPARISON_EQ, r1, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(r1)));
                          *out_rval = (gcc_jit_rvalue_get_type(cmp) == type) ? cmp : gcc_jit_context_new_cast(ctx, loc, cmp, type);
                      } else {
                          *out_rval = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, type, r1);
                      }
                      return GCC_GEN_BE_SUCCESS;

                  case eok_real_part:
                  case eok_imag_part:
                      {
                          gcc_jit_type *c_type = gcc_jit_rvalue_get_type(r1);
                          if (gcc_jit_type_is_struct(c_type)) {
                              gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                              gcc_jit_field *f = gcc_jit_struct_get_field(s, op == eok_real_part ? 0 : 1);
                              *out_rval = gcc_jit_rvalue_access_field(r1, NULL, f);
                          } else {
                              /* Fallback for native complex types (bitcast or array access workaround) */
                              gcc_jit_type *elem_type = type; /* The result type is the element type */
                              gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                              
                              /* We need to get the address of r1. If r1 is an lvalue-as-rvalue or similar, we might need a local */
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "cplx_tmp");
                              gcc_jit_block_add_assignment(cblock, loc, tmp, r1);
                              
                              gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                              gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                              
                              if (op == eok_imag_part) {
                                  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                  gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                                  gcc_jit_lvalue *arr_acc = gcc_jit_context_new_array_access(ctx, loc, elem_ptr, one);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(arr_acc);
                              } else {
                                  *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(elem_ptr, NULL));
                              }
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_xconj:
                      {
                          gcc_jit_type *c_type = gcc_jit_rvalue_get_type(r1);
                          gcc_jit_type *elem_type = NULL;
                          if (gcc_jit_type_is_struct(c_type)) {
                              gcc_jit_struct *s = gcc_jit_type_is_struct(c_type);
                              gcc_jit_field *f_real = gcc_jit_struct_get_field(s, 0);
                              gcc_jit_field *f_imag = gcc_jit_struct_get_field(s, 1);
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(op2->variant.field.ptr->type, &elem_type));
                              
                              gcc_jit_rvalue *real_val = gcc_jit_rvalue_access_field(r1, NULL, f_real);
                              gcc_jit_rvalue *imag_val = gcc_jit_rvalue_access_field(r1, NULL, f_imag);
                              gcc_jit_rvalue *neg_imag = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, imag_val);
                              
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "conj_tmp");
                              gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, f_real), real_val);
                              gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, f_imag), neg_imag);
                              *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                          } else {
                              /* If native complex type, libgccjit supports bitwise conjugate via MINUS? No, there is no conjugate unary op. */
                              /* We can negate the imaginary part via pointer as before */
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, c_type, "conj_tmp");
                              gcc_jit_block_add_assignment(cblock, loc, tmp, r1);
                              
                              gcc_jit_type *elem_ptr_type = NULL;
                              a_type_ptr op_t2 = expr->type;
while (op_t2 && op_t2->kind == tk_typeref) op_t2 = op_t2->variant.typeref.type;
if (op_t2->variant.float_kind == fk_float) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                              else if (op_t2->variant.float_kind == fk_long_double) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                              else elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                              
                              elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                              gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                              gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                              
                              gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                              gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1);
                              gcc_jit_lvalue *imag_lval = gcc_jit_context_new_array_access(ctx, loc, elem_ptr, one);
                              gcc_jit_rvalue *imag_rval = gcc_jit_lvalue_as_rvalue(imag_lval);
                              gcc_jit_rvalue *neg_imag = gcc_jit_context_new_unary_op(ctx, loc, GCC_JIT_UNARY_OP_MINUS, elem_type, imag_rval);
                              gcc_jit_block_add_assignment(cblock, loc, imag_lval, neg_imag);
                              
                              *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }


                  case eok_post_incr:
                  case eok_post_decr:
                      {
                          if (op1 && op1->kind == enk_operation &&
                              (op1->variant.operation.kind == eok_dot_field || op1->variant.operation.kind == eok_points_to_field) &&
                              op1->variant.operation.operands && op1->variant.operation.operands->next &&
                              op1->variant.operation.operands->next->kind == enk_field &&
                              op1->variant.operation.operands->next->variant.field.ptr &&
                              (op1->variant.operation.operands->next->variant.field.ptr->is_bit_field ||
                               op1->variant.operation.operands->next->variant.field.ptr->bit_size > 0)) {
                              return gcc_gen_be_lower_bitfield_assignment(op1, op, NULL, out_rval);
                          }
                          gcc_jit_lvalue *l1 = NULL;
                          err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          
                          gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
                          
                          gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                          gcc_jit_lvalue *temp = gcc_jit_function_new_local(func, NULL, type, "post_inc_temp");
                          
                          gcc_jit_block_add_assignment(cblock, loc, temp, gcc_jit_lvalue_as_rvalue(l1));
                          
                          gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, type, 1);
                          gcc_jit_block_add_assignment_op(cblock, NULL, l1, 
                              op == eok_post_incr ? GCC_JIT_BINARY_OP_PLUS : GCC_JIT_BINARY_OP_MINUS, one);
                              
                          *out_rval = gcc_jit_lvalue_as_rvalue(temp);
                          return GCC_GEN_BE_SUCCESS;
                      }

                  case eok_address_of:
                      {
                          gcc_jit_lvalue *l1 = NULL;
                          err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          *out_rval = gcc_jit_lvalue_get_address(l1, NULL);
                          return GCC_GEN_BE_SUCCESS;
                      }
                  case eok_indirect:
                  case eok_ref_indirect:
                  case eok_dot_field:
                  case eok_points_to_field:


          case eok_subscript:
                  case eok_pre_incr:
                  case eok_pre_decr:
                      {
                          if ((op == eok_pre_incr || op == eok_pre_decr) &&
                              op1 && op1->kind == enk_operation &&
                              (op1->variant.operation.kind == eok_dot_field || op1->variant.operation.kind == eok_points_to_field) &&
                              op1->variant.operation.operands && op1->variant.operation.operands->next &&
                              op1->variant.operation.operands->next->kind == enk_field &&
                              op1->variant.operation.operands->next->variant.field.ptr &&
                              (op1->variant.operation.operands->next->variant.field.ptr->is_bit_field ||
                               op1->variant.operation.operands->next->variant.field.ptr->bit_size > 0)) {
                              return gcc_gen_be_lower_bitfield_assignment(op1, op, NULL, out_rval);
                          }
                          gcc_jit_lvalue *lexpr = NULL;
                          err = gcc_gen_be_lower_expr_lvalue(expr, &lexpr);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          if (lexpr) {
                              a_type_ptr etype = expr->type;
                              while (etype && etype->kind == tk_typeref) etype = etype->variant.typeref.type;
                              if (etype && etype->kind == tk_array) {
                                  gcc_jit_type *elem_type = NULL;
                                  err = gcc_gen_be_lower_type(etype->variant.array.element_type, &elem_type);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;
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
                      {
                          gcc_jit_function *func = NULL;
                          gcc_jit_rvalue *fn_ptr = NULL;
                          int num_args = 0;
                          
                          int is_member = (op == eok_dot_member_call || op == eok_points_to_member_call);
                          
                          if (op1->kind == enk_routine) {
                              a_routine_ptr rout = op1->variant.routine.ptr;
                              void *cached = NULL;
                              err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, rout, &cached);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                              
                              if (!cached) {
                                  err = gcc_gen_be_lower_function_decl(rout, &func);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;
                              } else {
                                  func = (gcc_jit_function *)cached;
                              }
                          } else {
                              fn_ptr = r1;
                          }
                          
                          an_expr_node_ptr arg = op2;
                          while (arg) {
                              num_args++;
                              arg = arg->next;
                          }
                          
                          gcc_jit_rvalue **args = NULL;
                          a_boolean uses_sret = FALSE;
                          gcc_jit_lvalue *sret_lval = NULL;
                          a_type_ptr call_ret_type = expr->type;
                          if (op1->kind == enk_routine && op1->variant.routine.ptr && op1->variant.routine.ptr->type &&
                              op1->variant.routine.ptr->type->kind == tk_routine) {
                              call_ret_type = op1->variant.routine.ptr->type->variant.routine.return_type;
                          }
                          if (call_ret_type && (op1->kind != enk_routine || op1->variant.routine.ptr->variant.builtin_function_kind == bfk_none)) {
                              extern gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;
                              err = gcc_gen_be_is_returned_via_sret(call_ret_type, &uses_sret);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                              if (uses_sret) num_args++;
                          }

                          if (num_args > 0) {
                              args = (gcc_jit_rvalue **)malloc((size_t)num_args * sizeof(gcc_jit_rvalue *));
                              if (!args) return GCC_GEN_BE_ERROR_OOM;
                              
                              int i = 0;
                              if (uses_sret) {
                                  gcc_jit_block *cblock = NULL;
                                  err = gcc_gen_be_get_current_block(&cblock);
                                  if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                  gcc_jit_function *parent_func = gcc_jit_block_get_function(cblock);
                                  gcc_jit_type *sret_type = NULL;
                                  err = gcc_gen_be_lower_type(call_ret_type, &sret_type);
                                  if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                  sret_lval = gcc_jit_function_new_local(parent_func, NULL, sret_type, "sret_buffer");
                                  args[i++] = gcc_jit_lvalue_get_address(sret_lval, NULL);
                              }
                              
                              arg = op2;
                              if (is_member) {
                                  gcc_jit_rvalue *this_ptr = NULL;
                                  if (op == eok_dot_member_call) {
                                      gcc_jit_lvalue *base_lval = NULL;
                                      err = gcc_gen_be_lower_expr_lvalue(arg, &base_lval);
                                      if (err == GCC_GEN_BE_SUCCESS && base_lval) {
                                          this_ptr = gcc_jit_lvalue_get_address(base_lval, NULL);
                                      } else {
                                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(arg, &this_ptr));
                                      }
                                  } else {
                                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(arg, &this_ptr));
                                  }
                                  if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                  args[i++] = this_ptr;
                                  arg = arg->next;
                              }
                              
                              while (arg) {
                                  gcc_jit_rvalue *arg_rval = NULL;
                                  err = gcc_gen_be_lower_expr_rvalue(arg, &arg_rval);
                                  if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                  
                                  extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
                                  extern gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;
                                  extern gcc_gen_be_error_t gcc_gen_be_coerce_Arg_to_registers(
                                      gcc_jit_function *fn,
                                      gcc_jit_block *blk,
                                      a_type_ptr atype,
                                      gcc_jit_rvalue *src_rval,
                                      gcc_jit_rvalue **out_coerced) GCC_GEN_BE_NOEXCEPT;
                                  a_boolean is_invis_ref = FALSE;
                                  err = gcc_gen_be_is_passed_by_invisible_reference(arg->type, &is_invis_ref);
                                  if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                  if (is_invis_ref) {
                                      gcc_jit_block *cblock = NULL;
                                      err = gcc_gen_be_get_current_block(&cblock);
                                      if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                      gcc_jit_function *parent_func = gcc_jit_block_get_function(cblock);
                                      gcc_jit_type *arg_type = gcc_jit_rvalue_get_type(arg_rval);
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(parent_func, NULL, arg_type, "invis_ref");
                                      gcc_jit_block_add_assignment(cblock, loc, tmp, arg_rval);
                                      arg_rval = gcc_jit_lvalue_get_address(tmp, NULL);
                                  } else if (arg->type && arg_rval &&
                                             (op1->kind != enk_routine || op1->variant.routine.ptr->variant.builtin_function_kind == bfk_none)) {
                                      gcc_jit_type *coerced_arg_t = NULL;
                                      a_boolean needs_coercion = FALSE;
                                      if (gcc_gen_be_get_coerced_abi_type(arg->type, &coerced_arg_t, &needs_coercion) == GCC_GEN_BE_SUCCESS &&
                                          needs_coercion && coerced_arg_t) {
                                          gcc_jit_block *cblock = NULL;
                                          err = gcc_gen_be_get_current_block(&cblock);
                                          if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                          if (cblock) {
                                              gcc_jit_rvalue *coerced_arg = NULL;
                                              err = gcc_gen_be_coerce_Arg_to_registers(
                                                  gcc_jit_block_get_function(cblock), cblock, arg->type, arg_rval, &coerced_arg);
                                              if (err == GCC_GEN_BE_SUCCESS && coerced_arg) {
                                                  arg_rval = coerced_arg;
                                              }
                                          }
                                      }
                                  }
                                  
                                  args[i] = arg_rval;
                                  i++;
                                  arg = arg->next;
                              }
                          }
                          
                          if (op1->kind == enk_routine && op1->variant.routine.ptr->variant.builtin_function_kind != bfk_none) {
                              a_routine_ptr rout = op1->variant.routine.ptr;
                              err = gcc_gen_be_lower_builtin_call(rout, num_args, args, out_rval);
                              if (args) free(args);
                              return err;
                          }

                            if (is_member && op1->kind != enk_routine && num_args > 0) {
                                int this_arg_idx = uses_sret ? 1 : 0;
                                gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                                gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
                                gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
                                gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                                
                                gcc_jit_type *struct_type = gcc_jit_rvalue_get_type(fn_ptr);
                                gcc_jit_field *f_func_ptr = gcc_jit_struct_get_field(gcc_jit_type_is_struct(struct_type), 0);
                                gcc_jit_field *f_this_adj = gcc_jit_struct_get_field(gcc_jit_type_is_struct(struct_type), 1);
                                
                                gcc_jit_rvalue *func_ptr_val = gcc_jit_rvalue_access_field(fn_ptr, NULL, f_func_ptr);
                                gcc_jit_rvalue *this_adj_val = gcc_jit_rvalue_access_field(fn_ptr, NULL, f_this_adj);
                                
                                /* Adjust this: (char*)this + this_adj */
                                gcc_jit_rvalue *this_char_ptr = gcc_jit_context_new_cast(ctx, loc, args[this_arg_idx], char_ptr_type);
                                gcc_jit_lvalue *adjusted_this = gcc_jit_context_new_array_access(ctx, loc, this_char_ptr, this_adj_val);
                                gcc_jit_rvalue *new_this_ptr = gcc_jit_lvalue_get_address(adjusted_this, NULL);
                                args[this_arg_idx] = gcc_jit_context_new_cast(ctx, loc, new_this_ptr, gcc_jit_rvalue_get_type(args[this_arg_idx]));
                                
                                /* Test least significant bit of func_ptr: (long)func_ptr & 1 */
                                gcc_jit_rvalue *func_ptr_as_long = gcc_jit_context_new_cast(ctx, loc, func_ptr_val, long_type);
                                gcc_jit_rvalue *one_long = gcc_jit_context_new_rvalue_from_long(ctx, long_type, 1);
                                gcc_jit_rvalue *is_virtual = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_BITWISE_AND, long_type, func_ptr_as_long, one_long);
                                gcc_jit_rvalue *is_virtual_bool = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, is_virtual, gcc_jit_context_new_rvalue_from_long(ctx, long_type, 0));
                                
                                gcc_jit_block *cblock = NULL;
                                GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                                gcc_jit_function *parent_func = gcc_jit_block_get_function(cblock);
                                
                                gcc_jit_block *virt_block = gcc_jit_function_new_block(parent_func, "pm_virt");
                                gcc_jit_block *non_virt_block = gcc_jit_function_new_block(parent_func, "pm_non_virt");
                                gcc_jit_block *merge_block = gcc_jit_function_new_block(parent_func, "pm_merge");
                                
                                gcc_jit_block_end_with_conditional(cblock, loc, is_virtual_bool, virt_block, non_virt_block);
                                
                                a_type_ptr op1_type = op1->type;
                                while (op1_type && op1_type->kind == tk_typeref) op1_type = op1_type->variant.typeref.type;
                                
                                gcc_jit_type *member_fn_type = NULL;
                                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(op1_type->variant.ptr_to_member.type, &member_fn_type));
                                gcc_jit_type *fn_ptr_type = gcc_jit_type_get_pointer(member_fn_type);
                                
                                gcc_jit_lvalue *resolved_fn_ptr = gcc_jit_function_new_local(parent_func, NULL, fn_ptr_type, "resolved_fn_ptr");
                                
                                /* non-virtual: just cast func_ptr_val */
                                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(non_virt_block));
                                gcc_jit_block_add_assignment(non_virt_block, NULL, resolved_fn_ptr, gcc_jit_context_new_cast(ctx, loc, func_ptr_val, fn_ptr_type));
                                gcc_jit_block_end_with_jump(non_virt_block, NULL, merge_block);
                                
                                /* virtual: read vtable */
                                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(virt_block));
                                gcc_jit_type *vtable_ptr_type = gcc_jit_type_get_pointer(void_ptr_type);
                                gcc_jit_type *vtable_ptr_ptr_type = gcc_jit_type_get_pointer(vtable_ptr_type);
                                gcc_jit_rvalue *this_vptr_ptr = gcc_jit_context_new_cast(ctx, loc, new_this_ptr, vtable_ptr_ptr_type);
                                gcc_jit_rvalue *vptr = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(this_vptr_ptr, NULL));
                                
                                gcc_jit_rvalue *vtable_char_ptr = gcc_jit_context_new_cast(ctx, loc, vptr, char_ptr_type);
                                gcc_jit_rvalue *vfunc_offset = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MINUS, long_type, func_ptr_as_long, one_long);
                                gcc_jit_lvalue *vfunc_addr_lval = gcc_jit_context_new_array_access(ctx, loc, vtable_char_ptr, vfunc_offset);
                                gcc_jit_rvalue *vfunc_addr = gcc_jit_lvalue_get_address(vfunc_addr_lval, NULL);
                                
                                gcc_jit_rvalue *vfunc_void_ptr_ptr = gcc_jit_context_new_cast(ctx, loc, vfunc_addr, vtable_ptr_ptr_type);
                                gcc_jit_rvalue *vfunc_void_ptr = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(vfunc_void_ptr_ptr, NULL));
                                
                                gcc_jit_block_add_assignment(virt_block, NULL, resolved_fn_ptr, gcc_jit_context_new_cast(ctx, loc, vfunc_void_ptr, fn_ptr_type));
                                gcc_jit_block_end_with_jump(virt_block, NULL, merge_block);
                                
                                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
                                fn_ptr = gcc_jit_lvalue_as_rvalue(resolved_fn_ptr);
                                func = NULL;
                            }

                          if (expr->variant.operation.is_virtual_call && op1->kind == enk_routine && num_args > 0) {
                              a_routine_ptr rout = op1->variant.routine.ptr;
                              int vindex = rout->number.virtual_function;
                              if (vindex != VIRTUAL_FUNCTION_NUMBER_NONE) {
                                  int this_arg_idx = uses_sret ? 1 : 0;
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
                                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(rout->type, &fn_ptr_type));
                                  if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info) {
                                      int p_idx = uses_sret ? 1 : 0;
                                      for (a_param_type_ptr ptp = rout->type->variant.routine.extra_info->param_type_list;
                                           ptp != NULL && p_idx < num_args;
                                           ptp = ptp->next, ++p_idx) {
                                          gcc_jit_type *expected_pt = NULL;
                                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(ptp->type, &expected_pt));
                                          extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
                                          extern gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;
                                          a_boolean is_invis = FALSE;
                                          if (ptp->type && gcc_gen_be_is_passed_by_invisible_reference(ptp->type, &is_invis) == GCC_GEN_BE_SUCCESS && is_invis) {
                                              expected_pt = gcc_jit_type_get_pointer(gcc_jit_type_unqualified(expected_pt));
                                          } else if (ptp->type) {
                                              gcc_jit_type *coerced_pt = NULL;
                                              a_boolean needs_coercion = FALSE;
                                              if (gcc_gen_be_get_coerced_abi_type(ptp->type, &coerced_pt, &needs_coercion) == GCC_GEN_BE_SUCCESS &&
                                                  needs_coercion && coerced_pt) {
                                                  expected_pt = coerced_pt;
                                              }
                                          }
                                          if (expected_pt && args[p_idx] && gcc_jit_rvalue_get_type(args[p_idx]) != expected_pt) {
                                              if (gcc_jit_type_is_struct(expected_pt) != NULL ||
                                                  gcc_jit_type_is_struct(gcc_jit_rvalue_get_type(args[p_idx])) != NULL) {
                                                  extern gcc_gen_be_error_t gcc_gen_be_coerce_rvalue(
                                                      gcc_jit_function *fn,
                                                      gcc_jit_block *blk,
                                                      gcc_jit_rvalue *src_rval,
                                                      gcc_jit_type *dst_type,
                                                      gcc_jit_rvalue **out_coerced) GCC_GEN_BE_NOEXCEPT;
                                                  gcc_jit_block *cblock = NULL;
                                                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                                                  if (cblock) {
                                                      gcc_jit_rvalue *coerced_varg = NULL;
                                                      if (gcc_gen_be_coerce_rvalue(
                                                              gcc_jit_block_get_function(cblock), cblock, args[p_idx], expected_pt, &coerced_varg) == GCC_GEN_BE_SUCCESS &&
                                                          coerced_varg) {
                                                          args[p_idx] = coerced_varg;
                                                      }
                                                  }
                                              } else {
                                                  args[p_idx] = gcc_jit_context_new_cast(ctx, loc, args[p_idx], expected_pt);
                                              }
                                          }
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
                                  gcc_jit_type *arg_type = gcc_jit_rvalue_get_type(args[i]);
                                  if (arg_type != param_type) {
                                      if (gcc_jit_type_is_pointer(arg_type) != NULL && gcc_jit_type_is_pointer(param_type) != NULL) {
                                          args[i] = gcc_jit_context_new_cast(ctx, loc, args[i], param_type);
                                      } else if (gcc_jit_type_is_pointer(arg_type) == NULL && gcc_jit_type_is_pointer(param_type) == NULL &&
                                                 gcc_jit_type_is_struct(arg_type) == NULL && gcc_jit_type_is_struct(param_type) == NULL) {
                                          args[i] = gcc_jit_context_new_cast(ctx, loc, args[i], param_type);
                                      } else if (gcc_jit_type_is_struct(arg_type) != NULL || gcc_jit_type_is_struct(param_type) != NULL) {
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
                                              if (gcc_gen_be_coerce_rvalue(gcc_jit_block_get_function(cblock), cblock, args[i], param_type, &coerced_arg) == GCC_GEN_BE_SUCCESS && coerced_arg) {
                                                  args[i] = coerced_arg;
                                              }
                                          }
                                      } else if (p_gcc_jit_context_new_bitcast) {
                                          args[i] = p_gcc_jit_context_new_bitcast(ctx, NULL, args[i], param_type);
                                      } else {
                                          gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                                          gcc_jit_rvalue *vptr = gcc_jit_context_new_cast(ctx, loc, args[i], void_ptr_type);
                                          args[i] = gcc_jit_context_new_cast(ctx, loc, vptr, param_type);
                                      }
                                  }
                              }
                              gcc_jit_rvalue *call_rval = gcc_jit_context_new_call(ctx, loc, func, num_args, args);
                              gcc_jit_block *active_lp = NULL;
                              gcc_jit_block *invoke_cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&active_lp));
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&invoke_cblock));
                              if (active_lp && invoke_cblock &&
                                  gcc_jit_block_get_function(active_lp) != gcc_jit_block_get_function(invoke_cblock)) {
                                  active_lp = NULL;
                              }
                              if (uses_sret) {
                                  gcc_jit_block *cblock = NULL;
                                  err = gcc_gen_be_get_current_block(&cblock);
                                  if (err != GCC_GEN_BE_SUCCESS) { if (args) free(args); return err; }
                                  if (active_lp && cblock) {
                                      gcc_jit_block *next_blk = NULL;
                                      err = gcc_gen_be_emit_invoke_call(
                                          ctx, cblock, func, num_args, args, NULL, NULL, active_lp, &next_blk);
                                      if (err != GCC_GEN_BE_SUCCESS) { if (args) free(args); return err; }
                                  } else {
                                      gcc_jit_block_add_eval(cblock, loc, call_rval);
                                  }
                                  *out_rval = gcc_jit_lvalue_as_rvalue(sret_lval);
                              } else {
                                  if (active_lp && invoke_cblock) {
                                      gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                                      gcc_jit_type *ret_t = call_rval ? gcc_jit_rvalue_get_type(call_rval) : void_t;
                                      gcc_jit_lvalue *res_lval = NULL;
                                      if (ret_t && ret_t != void_t) {
                                          res_lval = gcc_jit_function_new_local(
                                              gcc_jit_block_get_function(invoke_cblock), loc, ret_t, "invoke_res");
                                      }
                                      gcc_jit_block *next_blk = NULL;
                                      err = gcc_gen_be_emit_invoke_call(
                                          ctx, invoke_cblock, func, num_args, args, res_lval, NULL, active_lp, &next_blk);
                                      if (err != GCC_GEN_BE_SUCCESS) { if (args) free(args); return err; }
                                      call_rval = res_lval
                                          ? gcc_jit_lvalue_as_rvalue(res_lval)
                                          : gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);
                                  }
                                  if (type && call_rval && type != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID) &&
                                      gcc_jit_rvalue_get_type(call_rval) != type) {
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
                                          if (gcc_gen_be_coerce_rvalue(gcc_jit_block_get_function(cblock), cblock, call_rval, type, &unpacked_ret) == GCC_GEN_BE_SUCCESS &&
                                              unpacked_ret) {
                                              call_rval = unpacked_ret;
                                          }
                                      }
                                  }
                                  *out_rval = call_rval;
                              }
                          } else if (fn_ptr) {
                              gcc_jit_rvalue *call_rval = gcc_jit_context_new_call_through_ptr(ctx, NULL, fn_ptr, num_args, args);
                              gcc_jit_block *active_lp = NULL;
                              gcc_jit_block *invoke_cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&active_lp));
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&invoke_cblock));
                              if (active_lp && invoke_cblock &&
                                  gcc_jit_block_get_function(active_lp) != gcc_jit_block_get_function(invoke_cblock)) {
                                  active_lp = NULL;
                              }
                              if (uses_sret && sret_lval) {
                                  gcc_jit_block *cblock = NULL;
                                  err = gcc_gen_be_get_current_block(&cblock);
                                  if (err != GCC_GEN_BE_SUCCESS) { if (args) free(args); return err; }
                                  if (active_lp && cblock) {
                                      gcc_jit_block *next_blk = NULL;
                                      err = gcc_gen_be_emit_invoke_call_through_ptr(
                                          ctx, cblock, fn_ptr, num_args, args, NULL, NULL, active_lp, &next_blk);
                                      if (err != GCC_GEN_BE_SUCCESS) { if (args) free(args); return err; }
                                  } else {
                                      gcc_jit_block_add_eval(cblock, loc, call_rval);
                                  }
                                  *out_rval = gcc_jit_lvalue_as_rvalue(sret_lval);
                              } else {
                                  if (active_lp && invoke_cblock) {
                                      gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                                      gcc_jit_type *ret_t = call_rval ? gcc_jit_rvalue_get_type(call_rval) : void_t;
                                      gcc_jit_lvalue *res_lval = NULL;
                                      if (ret_t && ret_t != void_t) {
                                          res_lval = gcc_jit_function_new_local(
                                              gcc_jit_block_get_function(invoke_cblock), loc, ret_t, "invoke_ptr_res");
                                      }
                                      gcc_jit_block *next_blk = NULL;
                                      err = gcc_gen_be_emit_invoke_call_through_ptr(
                                          ctx, invoke_cblock, fn_ptr, num_args, args, res_lval, NULL, active_lp, &next_blk);
                                      if (err != GCC_GEN_BE_SUCCESS) { if (args) free(args); return err; }
                                      call_rval = res_lval
                                          ? gcc_jit_lvalue_as_rvalue(res_lval)
                                          : gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);
                                  }
                                  if (type && call_rval && type != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID) &&
                                      gcc_jit_rvalue_get_type(call_rval) != type) {
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
                                          if (gcc_gen_be_coerce_rvalue(gcc_jit_block_get_function(cblock), cblock, call_rval, type, &unpacked_ret) == GCC_GEN_BE_SUCCESS &&
                                              unpacked_ret) {
                                              call_rval = unpacked_ret;
                                          }
                                      }
                                  }
                                  *out_rval = call_rval;
                              }
                          } else {
                              return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
                          }
                          
                          if (args) free(args);
                          return GCC_GEN_BE_SUCCESS;
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
                              err = gcc_gen_be_lower_expr_lvalue(op1, &ap_lval);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                              
                              gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                              gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
                              gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));

                              a_type_ptr ap_edg_type = op1 ? op1->type : NULL;
                              while (ap_edg_type && ap_edg_type->kind == tk_typeref) {
                                  ap_edg_type = ap_edg_type->variant.typeref.type;
                              }

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

                                  size_t type_size = (size_t)gcc_jit_type_get_size(type);
                                  if (type_size == 0 && expr->type) type_size = (size_t)size_of_type(expr->type);
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
                                      a_boolean is_indirect_agg = (num_fp == 0 && type_size > 16 &&
                                          expr->type && (expr->type->kind == tk_class || expr->type->kind == tk_struct || expr->type->kind == tk_union))
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

                                  if (!is_stack && (num_gp > 0 || num_fp > 0) && (num_gp == 0 || num_fp == 0)) {
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
                              size_t type_size = (size_t)gcc_jit_type_get_size(type);
                              if (type_size == 0 && expr->type) type_size = (size_t)size_of_type(expr->type);
                              size_t ptr_size = (size_t)gcc_jit_type_get_size(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR));
                              if (ptr_size == 0) ptr_size = 8;
                              size_t align = (target_abi == GCC_GEN_BE_TARGET_ABI_SYSV_I386) ? 4 : ptr_size;

                              a_boolean is_indirect_agg = FALSE;
                              if ((target_abi == GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D && type_size > 16) ||
                                  (target_abi == GCC_GEN_BE_TARGET_ABI_WIN64 && type_size > 8 &&
                                   type_size != 1 && type_size != 2 && type_size != 4 && type_size != 8)) {
                                  if (expr->type && (expr->type->kind == tk_class || expr->type->kind == tk_struct || expr->type->kind == tk_union)) {
                                      is_indirect_agg = TRUE;
                                  }
                              }

                              size_t slot_size = is_indirect_agg ? ptr_size : type_size;
                              size_t aligned_size = (slot_size + align - 1) & ~(align - 1);
                              
                              gcc_jit_rvalue *offset = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)aligned_size);
                              
                              /* Compute next ap: next_ap = ap_char_ptr + offset */
                              gcc_jit_lvalue *next_ap_lval = gcc_jit_context_new_array_access(ctx, loc, ap_char_ptr, offset);
                              gcc_jit_rvalue *next_ap = gcc_jit_lvalue_get_address(next_ap_lval, NULL);
                              
                              /* Save next_ap back to ap_lval */
                              gcc_jit_rvalue *next_ap_casted = gcc_jit_context_new_cast(ctx, loc, next_ap, gcc_jit_rvalue_get_type(ap_val));
                              if (cblock) {
                                  gcc_jit_block_add_assignment(cblock, loc, ap_lval, next_ap_casted);
                              }
                              
                              /* Cast original ap_val to type* (or type** if indirect aggregate) and dereference */
                              gcc_jit_type *type_ptr = gcc_jit_type_get_pointer(type);
                              if (is_indirect_agg) {
                                  gcc_jit_type *type_ptr_ptr = gcc_jit_type_get_pointer(type_ptr);
                                  gcc_jit_rvalue *ind_ptr = gcc_jit_lvalue_as_rvalue(
                                      gcc_jit_rvalue_dereference(
                                          gcc_jit_context_new_cast(ctx, loc, ap_val, type_ptr_ptr), loc));
                                  *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(ind_ptr, loc));
                              } else {
                                  gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, loc, ap_val, type_ptr);
                                  *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(typed_ptr, loc));
                              }
                              
                              return GCC_GEN_BE_SUCCESS;
                          }
                          const char *bname = NULL;
                          if (op == eok_va_start || op == eok_va_start_single_operand) {
                              bname = "__builtin_va_start";
                          } else if (op == eok_va_end) {
                              bname = "__builtin_va_end";
                          } else if (op == eok_va_copy) {
                              bname = "__builtin_va_copy";
                          }
                          
                          gcc_jit_function *bfunc = bname ? gcc_jit_context_get_builtin_function(ctx, bname) : NULL;
                          if (bfunc) {
                              int num_args = 0;
                              an_expr_node_ptr arg = op1;
                              while (arg) {
                                  num_args++;
                                  arg = arg->next;
                              }
                              
                              gcc_jit_rvalue **args = NULL;
                              if (num_args > 0) {
                                  args = (gcc_jit_rvalue **)malloc((size_t)num_args * sizeof(gcc_jit_rvalue *));
                                  if (!args) return GCC_GEN_BE_ERROR_OOM;
                                  int i = 0;
                                  arg = op1;
                                  while (arg) {
                                      err = gcc_gen_be_lower_expr_rvalue(arg, &args[i]);
                                      if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                                      i++;
                                      arg = arg->next;
                                  }
                              }
                              
                              *out_rval = gcc_jit_context_new_call(ctx, loc, bfunc, num_args, args);
                              if (args) free(args);
                              return GCC_GEN_BE_SUCCESS;
                          }
                          *out_rval = gcc_jit_context_zero(ctx, type);
                          return GCC_GEN_BE_SUCCESS;
                      }
                      
                  case eok_await:
                  case eok_yield:
                      return gcc_gen_be_lower_await_yield(expr, out_rval);
          case eok_bool_cast:
                      {
                          gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                          if (r1 && gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(r1)) != NULL) {
                              *out_rval = gcc_jit_context_new_comparison(
                                  ctx, loc, GCC_JIT_COMPARISON_NE, r1, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(r1)));
                          } else {
                              *out_rval = gcc_jit_context_new_cast(ctx, loc, r1, bool_type);
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }

                  case eok_array_to_pointer:
                      {
                          gcc_jit_lvalue *l1 = NULL;
                          err = gcc_gen_be_lower_expr_lvalue(op1, &l1);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          if (l1) {
                              *out_rval = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(l1, NULL), type);
                          } else {
                              *out_rval = gcc_jit_context_null(ctx, type);
                          }
                          return GCC_GEN_BE_SUCCESS;
                      }
                      
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
                              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                              
                              if (dest_is_complex && !src_is_complex) {
                                  /* Real to Complex */
                                  gcc_jit_type *elem_type = NULL;
                                  a_type_ptr op_t2 = expr->type;
while (op_t2 && op_t2->kind == tk_typeref) op_t2 = op_t2->variant.typeref.type;
if (op_t2->variant.float_kind == fk_float) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                                  else if (op_t2->variant.float_kind == fk_long_double) elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                                  else elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                                  
                                  gcc_jit_rvalue *real_part = gcc_jit_context_new_cast(ctx, loc, r1, elem_type);
                                  gcc_jit_rvalue *imag_part = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);
                                  imag_part = gcc_jit_context_new_cast(ctx, loc, imag_part, elem_type);
                                  
                                  if (gcc_jit_type_is_struct(type)) {
                                      gcc_jit_struct *s = gcc_jit_type_is_struct(type);
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, type, "cplx_res");
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), real_part);
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), imag_part);
                                      *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                                  } else {
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, type, "cplx_res");
                                      gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_type);
                                      gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                                      gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                                      gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0)), real_part);
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1)), imag_part);
                                      *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                                  }
                                  return GCC_GEN_BE_SUCCESS;
                              } else if (!dest_is_complex && src_is_complex) {
                                  /* Complex to Real */
                                  gcc_jit_type *src_elem_type = NULL;
                                  a_type_ptr op1_t2 = op1->type;
while (op1_t2 && op1_t2->kind == tk_typeref) op1_t2 = op1_t2->variant.typeref.type;
if (op1_t2->variant.float_kind == fk_float) src_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                                  else if (op1_t2->variant.float_kind == fk_long_double) src_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                                  else src_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                                  
                                  gcc_jit_rvalue *r = NULL, *i = NULL;
                                  GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, func, cblock, r1, gcc_jit_rvalue_get_type(r1), src_elem_type, &r, &i));
                                  *out_rval = gcc_jit_context_new_cast(ctx, loc, r, type);
                                  return GCC_GEN_BE_SUCCESS;
                              } else if (dest_is_complex && src_is_complex) {
                                  /* Complex to Complex */
                                  gcc_jit_type *src_elem_type = NULL;
                                  a_type_ptr op1_t2 = op1->type;
while (op1_t2 && op1_t2->kind == tk_typeref) op1_t2 = op1_t2->variant.typeref.type;
if (op1_t2->variant.float_kind == fk_float) src_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                                  else if (op1_t2->variant.float_kind == fk_long_double) src_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                                  else src_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                                  
                                  gcc_jit_type *dest_elem_type = NULL;
                                  a_type_ptr op_t2 = expr->type;
while (op_t2 && op_t2->kind == tk_typeref) op_t2 = op_t2->variant.typeref.type;
if (op_t2->variant.float_kind == fk_float) dest_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                                  else if (op_t2->variant.float_kind == fk_long_double) dest_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                                  else dest_elem_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                                  
                                  gcc_jit_rvalue *r = NULL, *i = NULL;
                                  GCC_GEN_BE_CHECK(extract_complex_parts(ctx, loc, func, cblock, r1, gcc_jit_rvalue_get_type(r1), src_elem_type, &r, &i));
                                  
                                  gcc_jit_rvalue *new_r = gcc_jit_context_new_cast(ctx, loc, r, dest_elem_type);
                                  gcc_jit_rvalue *new_i = gcc_jit_context_new_cast(ctx, loc, i, dest_elem_type);
                                  
                                  if (gcc_jit_type_is_struct(type)) {
                                      gcc_jit_struct *s = gcc_jit_type_is_struct(type);
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, type, "cplx_res");
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 0)), new_r);
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_lvalue_access_field(tmp, NULL, gcc_jit_struct_get_field(s, 1)), new_i);
                                      *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                                  } else {
                                      gcc_jit_lvalue *tmp = gcc_jit_function_new_local(func, NULL, type, "cplx_res");
                                      gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(dest_elem_type);
                                      gcc_jit_rvalue *tmp_addr = gcc_jit_lvalue_get_address(tmp, NULL);
                                      gcc_jit_rvalue *elem_ptr = gcc_jit_context_new_cast(ctx, loc, tmp_addr, elem_ptr_type);
                                      gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0)), new_r);
                                      gcc_jit_block_add_assignment(cblock, loc, gcc_jit_context_new_array_access(ctx, loc, elem_ptr, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 1)), new_i);
                                      *out_rval = gcc_jit_lvalue_as_rvalue(tmp);
                                  }
                                  return GCC_GEN_BE_SUCCESS;
                              }
                          }
                          if (type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              if (r1 && cblock && gcc_jit_rvalue_get_type(r1) != type) {
                                  gcc_jit_block_add_eval(cblock, loc, r1);
                              }
                              *out_rval = NULL;
                          } else if (r1) {
                              if (gcc_jit_rvalue_get_type(r1) == type) {
                                  *out_rval = r1;
                              } else {
                                  *out_rval = gcc_jit_context_new_cast(ctx, loc, r1, type);
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
              err = gcc_gen_be_get_context(&ctx);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              
              gcc_jit_type *expected_type = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &expected_type));
              
              gcc_jit_function *func = gcc_jit_block_get_function(cblock);
              gcc_jit_lvalue *temp_lval = gcc_jit_function_new_local(func, NULL, expected_type, "braced_init_temp");
              
              an_expr_node_ptr elem = expr->variant.braced_init_list;
              
              a_type_ptr orig_type = expr->type;
              while (orig_type && orig_type->kind == tk_typeref) orig_type = orig_type->variant.typeref.type;
              
              if (orig_type->kind == tk_array) {
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
              } else if (orig_type->kind == tk_class || orig_type->kind == tk_struct || orig_type->kind == tk_union) {
                  a_field_ptr fld = orig_type->variant.class_struct_union.field_list;
                  
                  while (elem && fld) {
                      gcc_jit_rvalue *elem_rval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(elem, &elem_rval));
                      
                      void *cached_field = NULL;
                      err = cache_lookup(GCC_GEN_BE_CACHE_FIELD, fld, &cached_field);
                      if (err == GCC_GEN_BE_SUCCESS && cached_field) {
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
          {
              err = gcc_gen_be_lower_typeid(expr, out_rval);
              if (err != GCC_GEN_BE_SUCCESS) return err;
          }
          break;

      case enk_routine:
          {
              a_routine_ptr rout = expr->variant.routine.ptr;
              void *cached = NULL;
              err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, rout, &cached);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              gcc_jit_function *f = (gcc_jit_function *)cached;
              if (!f) {
                  err = gcc_gen_be_lower_function_decl(rout, &f);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
              }
              if (f) *out_rval = gcc_jit_function_get_address(f, NULL);
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

                  if (ndsp->number_of_elements) {
                      gcc_jit_rvalue *elem_size = gcc_jit_context_new_sizeof(ctx, type);
                      gcc_jit_rvalue *elem_size_cast = gcc_jit_context_new_cast(ctx, loc, elem_size, size_type);
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
                      alloc_size = gcc_jit_context_new_sizeof(ctx, type);
                      alloc_size = gcc_jit_context_new_cast(ctx, loc, alloc_size, size_type);
                  }
                  
                  gcc_jit_rvalue *alloc_call = NULL;
                  if (ndsp->routine) {
                      void *cached = NULL;
                      err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, ndsp->routine, &cached);
                      gcc_jit_function *alloc_func = (gcc_jit_function *)cached;
                      if (!alloc_func) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(ndsp->routine, &alloc_func));
                      }
                      if (alloc_func) {
                          int num_args = 1;
                          an_expr_node_ptr arg = ndsp->arg;
                          while (arg) { num_args++; arg = arg->next; }
                          
                          gcc_jit_rvalue **args = (gcc_jit_rvalue **)malloc((size_t)num_args * sizeof(gcc_jit_rvalue *));
                          if (!args) return GCC_GEN_BE_ERROR_OOM;
                          args[0] = alloc_size;
                          
                          arg = ndsp->arg;
                          int i = 1;
                          while (arg) {
                              err = gcc_gen_be_lower_expr_rvalue(arg, &args[i]);
                              if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                              i++;
                              arg = arg->next;
                          }
                          int param_count = (int)gcc_jit_function_get_param_count(alloc_func);
                          for (int p = 0; p < num_args && p < param_count; ++p) {
                              gcc_jit_param *param = gcc_jit_function_get_param(alloc_func, p);
                              gcc_jit_type *ptype = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(param));
                              gcc_jit_type *atype = gcc_jit_rvalue_get_type(args[p]);
                              if (atype != ptype) {
                                  args[p] = gcc_jit_context_new_cast(ctx, loc, args[p], ptype);
                              }
                          }
                          
                          gcc_jit_block *active_alloc_lp = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&active_alloc_lp));
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          if (active_alloc_lp && cblock &&
                              gcc_jit_block_get_function(active_alloc_lp) == gcc_jit_block_get_function(cblock)) {
                              gcc_jit_lvalue *alloc_res_lval = gcc_jit_function_new_local(
                                  func, loc, void_ptr_type, "invoke_new_alloc");
                              gcc_jit_block *next_blk = NULL;
                              err = gcc_gen_be_emit_invoke_call(
                                  ctx, cblock, alloc_func, num_args, args, alloc_res_lval, NULL, active_alloc_lp, &next_blk);
                              if (err != GCC_GEN_BE_SUCCESS) { free(args); return err; }
                              cblock = next_blk;
                              alloc_call = gcc_jit_lvalue_as_rvalue(alloc_res_lval);
                          } else {
                              alloc_call = gcc_jit_context_new_call(ctx, loc, alloc_func, num_args, args);
                          }
                          free(args);
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
                      if (needs_cookie && num_elems_cast) {
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
                      if (new_cleanup_lp) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
                      }
                      if (err != GCC_GEN_BE_SUCCESS) {
                          if (free_fn && cblock) {
                              gcc_jit_rvalue *void_p = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_as_rvalue(allocated_ptr), void_ptr_type);
                              gcc_jit_block_add_eval(cblock, loc, gcc_jit_context_new_call(ctx, loc, free_fn, 1, &void_p));
                          }
                          return err;
                      }
                      if (new_cleanup_lp && free_fn) {
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
                              if (gcc_gen_be_get_active_landing_pad(&outer_lp) == GCC_GEN_BE_SUCCESS && outer_lp) {
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
                          void *dtor_cached = NULL;
                          GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, ndsp->dynamic_init->destructor, &dtor_cached));
                          gcc_jit_function *dtor_fn = (gcc_jit_function *)dtor_cached;
                          if (!dtor_fn) {
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(ndsp->dynamic_init->destructor, &dtor_fn));
                          }
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
                                  gcc_jit_rvalue *elem_sz_val = gcc_jit_context_new_cast(
                                      ctx, loc, gcc_jit_context_new_sizeof(ctx, type), size_type);
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
                      void *cached = NULL;
                      err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, ndsp->routine, &cached);
                      gcc_jit_function *dealloc_func = (gcc_jit_function *)cached;
                      if (!dealloc_func) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(ndsp->routine, &dealloc_func));
                      }
                      if (dealloc_func) {
                          /* Support C++14 sized deallocation and C++17 over-aligned deallocation */
                          int num_args = 0;
                          for (an_expr_node_ptr a = ndsp->arg; a != NULL; a = a->next) {
                              num_args++;
                          }
                          int param_count = (int)gcc_jit_function_get_param_count(dealloc_func);
                          int call_argc = num_args > param_count ? num_args : param_count;
                          if (call_argc < 1) call_argc = 1;
                          gcc_jit_rvalue **dargs = (gcc_jit_rvalue **)malloc((size_t)call_argc * sizeof(gcc_jit_rvalue *));
                          if (!dargs) return GCC_GEN_BE_ERROR_OOM;
                          int idx = 0;
                          dargs[idx++] = dealloc_ptr;
                          for (an_expr_node_ptr a = ndsp->arg ? ndsp->arg->next : NULL; a != NULL && idx < call_argc; a = a->next) {
                              err = gcc_gen_be_lower_expr_rvalue(a, &dargs[idx]);
                              if (err != GCC_GEN_BE_SUCCESS) { free(dargs); return err; }
                              idx++;
                          }
                          /* Fill in implicit size / alignment if dealloc_func expects more arguments (e.g. operator delete(void*, size_t)) */
                          while (idx < param_count) {
                              gcc_jit_param *param = gcc_jit_function_get_param(dealloc_func, idx);
                              gcc_jit_type *ptype = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(param));
                              if (idx == 1) {
                                  gcc_jit_rvalue *sz = dealloc_total_size ? dealloc_total_size : gcc_jit_context_new_sizeof(ctx, type);
                                  dargs[idx] = gcc_jit_context_new_cast(ctx, loc, sz, ptype);
                              } else {
                                  long align_val = ndsp->type ? (long)ndsp->type->alignment : 8L;
                                  dargs[idx] = gcc_jit_context_new_rvalue_from_long(ctx, ptype, align_val);
                              }
                              idx++;
                          }
                          for (int p = 0; p < idx && p < param_count; ++p) {
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
              err = gcc_gen_be_lower_function_decl(overriding_rout, &func);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
              gcc_jit_function *current_jit_func = gcc_jit_block_get_function(cblock);

              int num_args = (int)gcc_jit_function_get_param_count(current_jit_func);
              gcc_jit_rvalue **args = NULL;
              if (num_args > 0) {
                  args = (gcc_jit_rvalue **)malloc((size_t)num_args * sizeof(gcc_jit_rvalue *));
                  
                  a_scope_ptr def_scope = scope_for_routine_or_null(wrapper_rout);
                  a_variable_ptr var_param = def_scope ? def_scope->variant.routine.parameters : NULL;
                  
                  for (int i = 0; i < num_args; i++) {
                      if (var_param) {
                          void *cached_lval = NULL;
                          err = cache_lookup(GCC_GEN_BE_CACHE_VAR, var_param, &cached_lval);
                          if (err == GCC_GEN_BE_SUCCESS && cached_lval) {
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
              }
              
              gcc_jit_type *expected_type = NULL;
              err = gcc_gen_be_lower_type(expr->type, &expected_type);
              if (err != GCC_GEN_BE_SUCCESS) {
                  if (args) free(args);
                  return err;
              }
              
              int target_pcount = (int)gcc_jit_function_get_param_count(func);
              int call_argc = num_args < target_pcount ? num_args : target_pcount;
              for (int p = 0; p < call_argc; ++p) {
                  gcc_jit_type *pt = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(func, p)));
                  if (args[p] && gcc_jit_rvalue_get_type(args[p]) != pt) {
                      args[p] = gcc_jit_context_new_cast(ctx, loc, args[p], pt);
                  }
              }
              gcc_jit_rvalue *call_rval = gcc_jit_context_new_call(ctx, loc, func, call_argc, args);
              if (args) free(args);
              
              gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
              if (expected_type == void_type || gcc_jit_function_get_return_type(func) == void_type) {
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (cblock) {
                      gcc_jit_block_add_eval(cblock, loc, call_rval);
                  }
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
                  if (cache_lookup(GCC_GEN_BE_CACHE_VAR, dip->variable, &cached) == GCC_GEN_BE_SUCCESS && cached) {
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

#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
      case enk_lowered_eh_construct:
          {
              gcc_jit_type *type = NULL;
              err = gcc_gen_be_lower_type(expr->type, &type);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
              if (cblock && type && type != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                  void *cached_addr = NULL;
                  if (expr->variant.lowered_eh.kind == leck_thrown_object_address) {
                      if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_thrown_obj_addr", &cached_addr) != GCC_GEN_BE_SUCCESS) {
                          cached_addr = NULL;
                      }
                  } else if (expr->variant.lowered_eh.kind == leck_caught_object_address) {
                      if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_caught_obj_addr", &cached_addr) != GCC_GEN_BE_SUCCESS) {
                          cached_addr = NULL;
                      }
                  }
                  if (cached_addr) {
                      gcc_jit_rvalue *exc_ptr = gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached_addr);
                      if (gcc_jit_type_is_pointer(type) != NULL) {
                          *out_rval = gcc_jit_context_new_cast(ctx, NULL, exc_ptr, type);
                      } else {
                          gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, NULL, exc_ptr, gcc_jit_type_get_pointer(type));
                          *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(typed_ptr, NULL));
                      }
                      return GCC_GEN_BE_SUCCESS;
                  }
                  gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                  void *cached_eh_ptr = NULL;
                  if (cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__builtin_eh_pointer", &cached_eh_ptr) != GCC_GEN_BE_SUCCESS || !cached_eh_ptr) {
                      gcc_jit_param *p0 = gcc_jit_context_new_param(ctx, NULL, int_type, "p");
                      gcc_jit_function *eh_ptr_fn = gcc_jit_context_new_function(
                          ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__builtin_eh_pointer", 1, &p0, 0);
                      GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__builtin_eh_pointer", eh_ptr_fn));
                      cached_eh_ptr = eh_ptr_fn;
                  }
                  if (cached_eh_ptr) {
                      gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                      gcc_jit_rvalue *exc_ptr = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_eh_ptr, 1, &zero);
                      if (gcc_jit_type_is_pointer(type) != NULL) {
                          *out_rval = gcc_jit_context_new_cast(ctx, NULL, exc_ptr, type);
                      } else {
                          gcc_jit_rvalue *typed_ptr = gcc_jit_context_new_cast(ctx, NULL, exc_ptr, gcc_jit_type_get_pointer(type));
                          *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(typed_ptr, NULL));
                      }
                      return GCC_GEN_BE_SUCCESS;
                  }
              }
              return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
          }
#endif

      case enk_throw:
          {
              gcc_jit_type *type = NULL;
              err = gcc_gen_be_lower_type(expr->type, &type);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              gcc_jit_block *out_block = NULL;
              err = gcc_gen_be_emit_throw(expr, &out_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              
              return gcc_gen_be_get_zero_rvalue(expr->type, type, out_rval);
          }
          
      default:
          *out_rval = NULL;
          return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_UNHANDLED_EXPR, "unhandled expression node kind");
  }
    return GCC_GEN_BE_SUCCESS;
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

    an_expr_node_ptr rrs = expr->variant.await_info.ready_resume_suspend;
    an_expr_node_ptr await_ready = rrs;
    an_expr_node_ptr await_resume = rrs ? rrs->next : NULL;
    an_expr_node_ptr await_suspend = await_resume ? await_resume->next : NULL;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_jit_function *func = gcc_jit_block_get_function(cblock);

    if (expr->variant.await_info.operand) {
        gcc_jit_rvalue *op_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(expr->variant.await_info.operand, &op_rval));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (op_rval && cblock) {
            gcc_jit_block_add_eval(cblock, loc, op_rval);
        }
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
        gcc_jit_rvalue *suspend_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(await_suspend, &suspend_rval));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&suspend_block));
        if (suspend_rval) {
            gcc_jit_type *susp_type = gcc_jit_rvalue_get_type(suspend_rval);
            if (susp_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                gcc_jit_block_add_eval(suspend_block, loc, suspend_rval);
                gcc_jit_block_end_with_void_return(suspend_block, loc);
            } else if (susp_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL)) {
                gcc_jit_block *suspend_actual = gcc_jit_function_new_block(func, "suspend_actual");
                gcc_jit_block_end_with_conditional(suspend_block, loc, suspend_rval, suspend_actual, resume_block);
                gcc_jit_block_end_with_void_return(suspend_actual, loc);
            } else {
                /* Symmetric transfer returning std::coroutine_handle<> */
                GCC_GEN_BE_CHECK(gcc_gen_be_emit_coro_symmetric_transfer(func, suspend_block, suspend_rval));
            }
        } else {
            gcc_jit_block_end_with_jump(suspend_block, NULL, resume_block);
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
        if (p_gcc_jit_context_new_rvalue_vector_perm && v1 && v2 && mask_rval) {
            *out_rval = p_gcc_jit_context_new_rvalue_vector_perm(ctx, loc, v1, v2, mask_rval);
            return GCC_GEN_BE_SUCCESS;
        }
    } else {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(op2, &v2));
    }

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!cblock) {
        GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, v1, res_type, out_rval));
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_function *func = gcc_jit_block_get_function(cblock);
    gcc_jit_type *v1_type = gcc_jit_rvalue_get_type(v1);
    gcc_jit_lvalue *src1_tmp = gcc_jit_function_new_local(func, loc, v1_type, "shuf_v1");
    gcc_jit_lvalue *src2_tmp = gcc_jit_function_new_local(func, loc, gcc_jit_rvalue_get_type(v2 ? v2 : v1), "shuf_v2");
    gcc_jit_lvalue *dst_tmp = gcc_jit_function_new_local(func, loc, res_type, "shuf_dst");
    gcc_jit_block_add_assignment(cblock, loc, src1_tmp, v1);
    gcc_jit_block_add_assignment(cblock, loc, src2_tmp, v2 ? v2 : v1);

    a_type_ptr ut = expr->type;
    while (ut && ut->kind == tk_typeref) ut = ut->variant.typeref.type;
    int num_lanes = 4;
    gcc_jit_type *lane_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    if (ut && ut->kind == tk_vector) {
        if (ut->variant.vector.element_type) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(ut->variant.vector.element_type, &lane_type));
            if (ut->variant.vector.element_type->size > 0 && ut->size >= ut->variant.vector.element_type->size) {
                num_lanes = (int)(ut->size / ut->variant.vector.element_type->size);
            }
        }
    }
    if (num_lanes <= 0) num_lanes = 1;

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
        GCC_GEN_BE_CHECK(safe_ptr_or_int_cast(ctx, src_vec, res_type, out_rval));
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_function *func = gcc_jit_block_get_function(cblock);
    gcc_jit_lvalue *src_tmp = gcc_jit_function_new_local(func, loc, gcc_jit_rvalue_get_type(src_vec), "convvec_src");
    gcc_jit_lvalue *dst_tmp = gcc_jit_function_new_local(func, loc, res_type, "convvec_dst");
    gcc_jit_block_add_assignment(cblock, loc, src_tmp, src_vec);

    a_type_ptr src_ut = src_op->type;
    while (src_ut && src_ut->kind == tk_typeref) src_ut = src_ut->variant.typeref.type;
    a_type_ptr dst_ut = expr->type;
    while (dst_ut && dst_ut->kind == tk_typeref) dst_ut = dst_ut->variant.typeref.type;

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
    if (num_lanes <= 0) num_lanes = 1;

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
    a_type_ptr ut = con->type;
    while (ut && ut->kind == tk_typeref) ut = ut->variant.typeref.type;
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

    long double ld_val = (long double)fetch_host_fp_value(fk, &con->variant.float_value);
    double dbl_hi = (double)ld_val;
    gcc_jit_rvalue *hi_rval = gcc_jit_context_new_rvalue_from_double(ctx, flt_type, dbl_hi);

    if (fk == fk_long_double || fk == fk_float80 || fk == fk_float128 || fk == fk_std_float128) {
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
    gcc_jit_type *i64_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG);
    gcc_jit_rvalue *w_lhs = gcc_jit_context_new_cast(ctx, NULL, lhs, i64_type);
    gcc_jit_rvalue *w_rhs = gcc_jit_context_new_cast(ctx, NULL, rhs, i64_type);
    gcc_jit_rvalue *shift_rv = gcc_jit_context_new_rvalue_from_int(ctx, i64_type, frac_bits);

    gcc_jit_rvalue *wide_res = NULL;
    if (op_code == '+') {
        wide_res = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_PLUS, i64_type, w_lhs, w_rhs);
    } else if (op_code == '-') {
        wide_res = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, i64_type, w_lhs, w_rhs);
    } else if (op_code == '*') {
        gcc_jit_rvalue *prod = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, i64_type, w_lhs, w_rhs);
        wide_res = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_RSHIFT, i64_type, prod, shift_rv);
    } else if (op_code == '/') {
        gcc_jit_rvalue *scaled = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, i64_type, w_lhs, shift_rv);
        wide_res = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_DIVIDE, i64_type, scaled, w_rhs);
    } else {
        return GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW;
    }

    if (is_saturating) {
        long max_bound = (frac_bits < 31) ? ((1L << (frac_bits + 1)) - 1L) : 0x7fffffffL;
        long min_bound = -max_bound - 1L;
        gcc_jit_rvalue *max_rv = gcc_jit_context_new_rvalue_from_long(ctx, i64_type, max_bound);
        gcc_jit_rvalue *min_rv = gcc_jit_context_new_rvalue_from_long(ctx, i64_type, min_bound);
        gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
        gcc_jit_rvalue *gt_max = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_GT, wide_res, max_rv);
        gcc_jit_rvalue *le_max = gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, bool_t, gt_max);
        wide_res = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_PLUS, i64_type,
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, i64_type, max_rv, gcc_jit_context_new_cast(ctx, NULL, gt_max, i64_type)),
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, i64_type, wide_res, gcc_jit_context_new_cast(ctx, NULL, le_max, i64_type)));
        gcc_jit_rvalue *lt_min = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_LT, wide_res, min_rv);
        gcc_jit_rvalue *ge_min = gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, bool_t, lt_min);
        wide_res = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_PLUS, i64_type,
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, i64_type, min_rv, gcc_jit_context_new_cast(ctx, NULL, lt_min, i64_type)),
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, i64_type, wide_res, gcc_jit_context_new_cast(ctx, NULL, ge_min, i64_type)));
    }

    *out_rval = (res_type == i64_type) ? wide_res : gcc_jit_context_new_cast(ctx, NULL, wide_res, res_type);
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