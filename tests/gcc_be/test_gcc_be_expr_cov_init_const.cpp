/**
 * @file test_gcc_be_expr_cov_init_const.cpp
 * @brief Exhaustive branch, line, and OOM coverage tests for helpers, dynamic init, bitfields, and constants in `src/gcc_gen_be_expr.c`.
 * @details Exercises helper routines, `gcc_gen_be_lower_dynamic_init`, `scale_vla_subarray_index`,
 * `gcc_gen_be_lower_bitfield_read`, `gcc_gen_be_lower_bitfield_assignment`, `safe_ptr_or_int_cast`,
 * `lower_address_constant`, `lower_struct_aggregate_constant_inner`, `lower_aggregate_constant`,
 * `gcc_gen_be_lower_constant_rvalue`, and `gcc_gen_be_lower_fixed_point_op` in `src/gcc_gen_be_expr.c`.
 * @ingroup gcc_gen_be_tests
 */

#include "test_gcc_be_expr_common.h"
#include "gcc_gen_be_lib_loader.h"

USING_NAMESPACE_EDG

BEGIN_EDG_NAMESPACE

/**
 * @brief Covers helper functions in lines 70–555 and fixed-point wide-type fast path in `gcc_gen_be_expr.c`.
 * @details Exercises `emit_optional_eval`, `assign_if_active_block`, `get_complex_elem_jit_type`,
 * `construct_complex_from_parts`, `resolve_cached_or_decl_func`, `reconcile_call_arg_rvalue`,
 * `finish_direct_or_indirect_call`, `resolve_lvalue_c11_atomic_type`, `check_is_complex_type`,
 * `extract_complex_parts`, and `gcc_gen_be_lower_fixed_point_op` when `res_type == wide_type`.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_part_a_helpers(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, long_tp, dbl_tp, flt_tp, ldbl_tp, void_tp, vptr_tp, st_tp;
    a_type cplx_flt_tp, cplx_ldbl_tp, cplx_dbl_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    std::memset(&dbl_tp, 0, sizeof(dbl_tp));
    dbl_tp.kind = tk_float;
    dbl_tp.size = 8;
    dbl_tp.variant.float_kind = fk_double;
    std::memset(&flt_tp, 0, sizeof(flt_tp));
    flt_tp.kind = tk_float;
    flt_tp.size = 4;
    flt_tp.variant.float_kind = fk_float;
    std::memset(&ldbl_tp, 0, sizeof(ldbl_tp));
    ldbl_tp.kind = tk_float;
    ldbl_tp.size = 16;
    ldbl_tp.variant.float_kind = fk_long_double;
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));

    a_field st_f0;
    std::memset(&st_f0, 0, sizeof(st_f0));
    st_f0.type = &int_tp;
    std::memset(&st_tp, 0, sizeof(st_tp));
    st_tp.kind = tk_struct;
    st_tp.size = 4;
    st_tp.alignment = 4;
    st_tp.variant.class_struct_union.field_list = &st_f0;

    std::memset(&cplx_flt_tp, 0, sizeof(cplx_flt_tp));
    cplx_flt_tp.kind = tk_complex;
    cplx_flt_tp.size = 8;
    cplx_flt_tp.variant.float_kind = fk_float;
    std::memset(&cplx_ldbl_tp, 0, sizeof(cplx_ldbl_tp));
    cplx_ldbl_tp.kind = tk_complex;
    cplx_ldbl_tp.size = 32;
    cplx_ldbl_tp.variant.float_kind = fk_long_double;
    std::memset(&cplx_dbl_tp, 0, sizeof(cplx_dbl_tp));
    cplx_dbl_tp.kind = tk_complex;
    cplx_dbl_tp.size = 16;
    cplx_dbl_tp.variant.float_kind = fk_double;

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_long = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_vptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_rvalue *rv = NULL;

    /* Fixed-point wide_type == res_type branch (GCC_JIT_TYPE_INT128_T operand) */
    gcc_jit_type *i128_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT128_T);
    gcc_jit_rvalue *v128_a = gcc_jit_context_new_rvalue_from_int(ctx, i128_t, 16);
    gcc_jit_rvalue *v128_b = gcc_jit_context_new_rvalue_from_int(ctx, i128_t, 4);
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_op('+', 4, FALSE, v128_a, v128_b, &rv));

    /* emit_optional_eval: rv != NULL with cblock == NULL, and rv of void type */
    a_constant c1;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c1, &int_tp, 7));
    an_expr_node e_c1, e_c2, e_comma;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_c1, &c1));
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_c2, &c1));
    GCC_GEN_BE_CHECK(expr_init_op(&e_comma, eok_comma, &int_tp, &e_c1));
    e_c1.next = &e_c2;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_comma, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* Void-returning call inside comma op1 -> rv has void type in emit_optional_eval */
    a_routine void_rout;
    std::memset(&void_rout, 0, sizeof(void_rout));
    a_type void_fn_tp;
    std::memset(&void_fn_tp, 0, sizeof(void_fn_tp));
    void_fn_tp.kind = tk_routine;
    void_fn_tp.variant.routine.return_type = &void_tp;
    void_rout.type = &void_fn_tp;
    gcc_jit_function *j_void_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_void_fn", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &void_rout, j_void_fn));
    an_expr_node e_vrout, e_vcall;
    std::memset(&e_vrout, 0, sizeof(e_vrout));
    e_vrout.kind = enk_routine;
    e_vrout.type = &void_fn_tp;
    e_vrout.variant.routine.ptr = &void_rout;
    GCC_GEN_BE_CHECK(expr_init_op(&e_vcall, eok_call, &void_tp, &e_vrout));
    e_vcall.next = &e_c2;
    e_comma.variant.operation.operands = &e_vcall;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_comma, &rv));
    e_vcall.next = NULL;

    /* assign_if_active_block: src == NULL and cblock == NULL via eok_assign, plus enk_temp_init */
    a_variable tmp_var;
    std::memset(&tmp_var, 0, sizeof(tmp_var));
    tmp_var.type = &int_tp;
    gcc_jit_lvalue *tmp_lv = gcc_jit_function_new_local(fn, NULL, jit_int, "cov_tmp_lv");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &tmp_var, tmp_lv));
    an_expr_node e_null_op, e_tinit, e_tvar, e_tassign;
    std::memset(&e_null_op, 0, sizeof(e_null_op));
    e_null_op.kind = enk_type_operand;
    e_null_op.type = &void_tp;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_tvar, &tmp_var));
    e_tvar.next = &e_null_op;
    GCC_GEN_BE_CHECK(expr_init_op(&e_tassign, eok_assign, &int_tp, &e_tvar));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_tassign, &rv));
    e_tvar.next = &e_c1;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_tassign, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    e_tvar.next = NULL;

    a_dynamic_init t_dip;
    std::memset(&t_dip, 0, sizeof(t_dip));
    t_dip.kind = dik_expression;
    t_dip.variable = &tmp_var;
    t_dip.variant.expression = &e_c1;
    std::memset(&e_tinit, 0, sizeof(e_tinit));
    e_tinit.kind = enk_temp_init;
    e_tinit.type = &int_tp;
    e_tinit.variant.init.dynamic_init = &t_dip;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_tinit, &rv));

    /* Complex float & long double element types, NULL val in extract_complex_parts, NULL cblock */
    an_expr_node e_cast_cflt, e_cast_cldbl;
    GCC_GEN_BE_CHECK(expr_init_op(&e_cast_cflt, eok_cast, &cplx_flt_tp, &e_c1));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast_cflt, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_cast_cldbl, eok_cast, &cplx_ldbl_tp, &e_c1));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast_cldbl, &rv));

    /* construct_complex_from_parts with cblock == NULL -> INVALID_STATE */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_cast_cflt, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    /* extract_complex_parts: scalar-typed complex rvalue with cblock == NULL -> INVALID_STATE */
    GCC_GEN_BE_CHECK(expr_test_map_type(&cplx_dbl_tp, jit_int));
    a_variable cplx_scalar_var;
    std::memset(&cplx_scalar_var, 0, sizeof(cplx_scalar_var));
    cplx_scalar_var.type = &cplx_dbl_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &cplx_scalar_var, tmp_lv));
    an_expr_node e_csvar, e_cast_creal;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_csvar, &cplx_scalar_var));
    GCC_GEN_BE_CHECK(expr_init_op(&e_cast_creal, eok_cast, &dbl_tp, &e_csvar));
    if (gcc_gen_be_lower_expr_rvalue(&e_cast_creal, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    g_expr_mock.num_type_overrides = 0;

    /* extract_complex_parts with val == NULL (enk_type_operand with tk_complex) -> INVALID_ARGUMENT */
    an_expr_node e_null_cplx, e_cast_null_cplx;
    std::memset(&e_null_cplx, 0, sizeof(e_null_cplx));
    e_null_cplx.kind = enk_type_operand;
    e_null_cplx.type = &cplx_dbl_tp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_cast_null_cplx, eok_cast, &dbl_tp, &e_null_cplx));
    if (gcc_gen_be_lower_expr_rvalue(&e_cast_null_cplx, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    /* check_is_complex_type with tp == NULL (eok_add with NULL operand type) */
    an_expr_node e_notype1, e_notype2, e_add_nt;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_notype1, &c1));
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_notype2, &c1));
    e_notype1.type = NULL;
    e_notype1.next = &e_notype2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_add_nt, eok_add, &int_tp, &e_notype1));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_add_nt, &rv));
    e_notype1.next = NULL;

    /* resolve_cached_or_decl_func with rout == NULL */
    an_expr_node e_null_rout;
    std::memset(&e_null_rout, 0, sizeof(e_null_rout));
    e_null_rout.kind = enk_routine;
    e_null_rout.type = &vptr_tp;
    e_null_rout.variant.routine.ptr = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_null_rout, &rv));

    /* reconcile_call_arg_rvalue & finish_direct_or_indirect_call edge branches */
    gcc_jit_param *p_int = gcc_jit_context_new_param(ctx, NULL, jit_int, "pi");
    gcc_jit_function *fn_take_int = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_int, "cov_take_int", 1, &p_int, 0);
    gcc_jit_param *p_vptr = gcc_jit_context_new_param(ctx, NULL, jit_vptr, "pv");
    gcc_jit_function *fn_take_ptr = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, g_expr_mock.one_field_struct_type, "cov_take_ptr", 1, &p_vptr, 0);
    gcc_jit_param *p_st = gcc_jit_context_new_param(ctx, NULL, g_expr_mock.one_field_struct_type, "ps");
    gcc_jit_function *fn_take_st = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_int, "cov_take_st", 1, &p_st, 0);

    a_routine r_take_int, r_take_ptr, r_take_st;
    std::memset(&r_take_int, 0, sizeof(r_take_int));
    std::memset(&r_take_ptr, 0, sizeof(r_take_ptr));
    std::memset(&r_take_st, 0, sizeof(r_take_st));
    r_take_int.type = &void_fn_tp;
    r_take_ptr.type = &void_fn_tp;
    r_take_st.type = &void_fn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_take_int, fn_take_int));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_take_ptr, fn_take_ptr));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_take_st, fn_take_st));

    /* 1) src_arg == NULL */
    an_expr_node e_ri, e_call_i;
    std::memset(&e_ri, 0, sizeof(e_ri));
    e_ri.kind = enk_routine;
    e_ri.type = &void_fn_tp;
    e_ri.variant.routine.ptr = &r_take_int;
    e_ri.next = &e_null_op;
    GCC_GEN_BE_CHECK(expr_init_op(&e_call_i, eok_call, &int_tp, &e_ri));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_i, &rv));

    /* 2) pointer arg to int param (with bitcast != NULL and bitcast == NULL) */
    a_constant c_null_ptr;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_null_ptr, &vptr_tp, 0));
    an_expr_node e_ptr_arg;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_ptr_arg, &c_null_ptr));
    e_ri.next = &e_ptr_arg;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_i, &rv));
    auto saved_bitcast = p_gcc_jit_context_new_bitcast;
    p_gcc_jit_context_new_bitcast = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_i, &rv));

    /* 3) non-pointer (double) arg to pointer param (with bitcast == NULL and != NULL) */
    a_variable dbl_var;
    std::memset(&dbl_var, 0, sizeof(dbl_var));
    dbl_var.type = &dbl_tp;
    gcc_jit_lvalue *dbl_lv = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE), "cov_dbl");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &dbl_var, dbl_lv));
    an_expr_node e_dbl_arg, e_rp, e_call_p;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_dbl_arg, &dbl_var));
    std::memset(&e_rp, 0, sizeof(e_rp));
    e_rp.kind = enk_routine;
    e_rp.type = &void_fn_tp;
    e_rp.variant.routine.ptr = &r_take_ptr;
    e_rp.next = &e_dbl_arg;
    GCC_GEN_BE_CHECK(expr_init_op(&e_call_p, eok_call, &int_tp, &e_rp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_p, &rv));
    p_gcc_jit_context_new_bitcast = saved_bitcast;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_p, &rv));

    /* Also test struct return unpacking with cblock == NULL and expected_ret_type == NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_p, &rv));
    e_call_p.type = NULL;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_p, &rv));

    /* 4) scalar arg (int and void-call) to struct param, with cblock != NULL and cblock == NULL */
    an_expr_node e_rs, e_call_s;
    std::memset(&e_rs, 0, sizeof(e_rs));
    e_rs.kind = enk_routine;
    e_rs.type = &void_fn_tp;
    e_rs.variant.routine.ptr = &r_take_st;
    e_rs.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_call_s, eok_call, &int_tp, &e_rs));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_s, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_s, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    /* Struct rvalue passed to int-typed parameter with cblock == NULL & force_zero_null == TRUE
       -> exercises coerced_arg == NULL in reconcile_call_arg_rvalue and unpacked_ret == NULL in finish_direct_or_indirect_call! */
    a_variable st_var;
    std::memset(&st_var, 0, sizeof(st_var));
    st_var.type = &st_tp;
    gcc_jit_lvalue *st_lv = gcc_jit_function_new_local(fn, NULL, g_expr_mock.one_field_struct_type, "cov_st");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &st_var, st_lv));
    an_expr_node e_st_arg;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_st_arg, &st_var));

    e_ri.next = &e_st_arg;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_i, &rv));
    g_expr_mock.force_zero_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_i, &rv));
    e_call_p.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_p, &rv));
    g_expr_mock.force_zero_null = FALSE;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* finish_direct_or_indirect_call:
       - active_lp belonging to a DIFFERENT function
       - active_lp with indirect NULL fn_ptr (enk_type_operand)
       - indirect NULL fn_ptr with cblock == NULL */
    gcc_jit_function *other_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, jit_void, "cov_other_fn", 0, NULL, 0);
    gcc_jit_block *other_lp = gcc_jit_function_new_block(other_fn, "other_lp");
    gcc_jit_block_end_with_void_return(other_lp, NULL);
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(other_lp));
    e_ri.next = &e_c1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_i, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());

    gcc_jit_block *same_lp = gcc_jit_function_new_block(fn, "same_lp");
    gcc_jit_block_end_with_void_return(same_lp, NULL);
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(same_lp));
    /* Direct call with cblock == NULL while active_lp != NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_i, &rv));
    /* Indirect call with fn_ptr == NULL while active_lp != NULL & cblock != NULL */
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    a_type fn_ptr_tp;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&fn_ptr_tp, &void_fn_tp, FALSE));
    an_expr_node e_null_fnptr, e_ind_call;
    std::memset(&e_null_fnptr, 0, sizeof(e_null_fnptr));
    e_null_fnptr.kind = enk_type_operand;
    e_null_fnptr.type = &fn_ptr_tp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_ind_call, eok_call, &void_tp, &e_null_fnptr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_ind_call, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    /* Indirect call with fn_ptr == NULL and cblock == NULL (active_lp == NULL) */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_ind_call, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    e_ri.next = NULL;
    e_rp.next = NULL;
    e_rs.next = NULL;

    /* resolve_lvalue_c11_atomic_type branches via assignment / pre-incr on lvalues */
    a_type atomic_int_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&atomic_int_tp, ik_int, 4));
    g_expr_mock.atomic_target_tp = &atomic_int_tp;

    /* 1) enk_variable with ptr == NULL (exercises lval_expr->variant.variable.ptr == NULL) */
    an_expr_node e_var_null_ptr, e_assign_at;
    std::memset(&e_var_null_ptr, 0, sizeof(e_var_null_ptr));
    e_var_null_ptr.kind = enk_variable;
    e_var_null_ptr.type = &int_tp;
    e_var_null_ptr.variant.variable.ptr = NULL;
    e_var_null_ptr.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_assign_at, eok_assign, &int_tp, &e_var_null_ptr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_assign_at, &rv));
    e_var_null_ptr.next = NULL;

    /* 2) Non-variable, non-operation lvalue (enk_temp_init) */
    e_tinit.next = &e_c1;
    e_assign_at.variant.operation.operands = &e_tinit;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_assign_at, &rv));
    e_tinit.next = NULL;

    /* 3) Field lvalue with atomic field type, NULL field ptr, non-enk_field a2, NULL a2, NULL a1 */
    a_field at_fld;
    std::memset(&at_fld, 0, sizeof(at_fld));
    at_fld.type = &atomic_int_tp;
    gcc_jit_field *j_at_f = gcc_jit_struct_get_field(
        gcc_jit_type_is_struct(g_expr_mock.one_field_struct_type), 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &at_fld, j_at_f));
    an_expr_node e_st_var, e_at_fld, e_dot_at;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_st_var, &st_var));
    std::memset(&e_at_fld, 0, sizeof(e_at_fld));
    e_at_fld.kind = enk_field;
    e_at_fld.type = &atomic_int_tp;
    e_at_fld.variant.field.ptr = &at_fld;
    e_st_var.next = &e_at_fld;
    GCC_GEN_BE_CHECK(expr_init_op(&e_dot_at, eok_dot_field, &int_tp, &e_st_var));
    e_dot_at.next = &e_c1;
    e_assign_at.variant.operation.operands = &e_dot_at;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_assign_at, &rv));

    /* NULL field.ptr (-> UNHANDLED_EXPR), non-enk_field a2, NULL a2, NULL a1 */
    e_at_fld.variant.field.ptr = NULL;
    if (gcc_gen_be_lower_expr_rvalue(&e_assign_at, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_st_var.next = &e_c2;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_assign_at, &rv));
    e_st_var.next = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_assign_at, &rv));
    e_dot_at.variant.operation.operands = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_assign_at, &rv));
    e_dot_at.next = NULL;

    /* 4) eok_ref_indirect and eok_indirect on pointer-to-atomic, plus edge cases */
    a_type ptr_at_tp, ptr_null_elem_tp;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_at_tp, &atomic_int_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_null_elem_tp, NULL, FALSE));
    a_variable pat_var;
    std::memset(&pat_var, 0, sizeof(pat_var));
    pat_var.type = &ptr_at_tp;
    gcc_jit_lvalue *pat_lv = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_type_get_pointer(jit_int), "cov_pat");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &pat_var, pat_lv));
    an_expr_node e_pat_var, e_ref_ind;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_pat_var, &pat_var));
    GCC_GEN_BE_CHECK(expr_init_op(&e_ref_ind, eok_ref_indirect, &int_tp, &e_pat_var));
    e_ref_ind.next = &e_c1;
    e_assign_at.variant.operation.operands = &e_ref_ind;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_assign_at, &rv));

    /* a1->type non-pointer, pointer with NULL target, NULL a1->type, NULL a1 */
    e_pat_var.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_assign_at, &rv));
    e_pat_var.type = &ptr_null_elem_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_assign_at, &rv));
    e_pat_var.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_assign_at, &rv));
    e_ref_ind.variant.operation.operands = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_assign_at, &rv));
    e_ref_ind.next = NULL;
    g_expr_mock.atomic_target_tp = NULL;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Covers `gcc_gen_be_lower_dynamic_init` and `scale_vla_subarray_index` branches.
 * @details Exercises `dik_zero`, `dik_lambda`, `ck_init_repeat` (including element destructor
 * unwind cleanups with 0/1/2 parameters), `ck_aggregate` dynamic designators, `dik_constructor`
 * value-init/virtual-base/OOM/null-block paths, scope destructor registration, and VLA scaling.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_part_a_dynamic_init_and_vla(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, void_tp, vptr_tp, arr_tp, st_tp, union_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));

    std::memset(&arr_tp, 0, sizeof(arr_tp));
    arr_tp.kind = tk_array;
    arr_tp.size = 8;
    arr_tp.alignment = 4;
    arr_tp.variant.array.element_type = &int_tp;
    arr_tp.variant.array.variant.number_of_elements = 2;

    a_field f0, f1;
    std::memset(&f0, 0, sizeof(f0));
    std::memset(&f1, 0, sizeof(f1));
    f0.type = &int_tp;
    f1.type = &int_tp;
    f0.next = &f1;

    a_class_type_supplement st_extra;
    std::memset(&st_extra, 0, sizeof(st_extra));
    std::memset(&st_tp, 0, sizeof(st_tp));
    st_tp.kind = tk_struct;
    st_tp.size = 4;
    st_tp.alignment = 4;
    st_tp.variant.class_struct_union.any_virtual_base_classes = TRUE;
    st_tp.variant.class_struct_union.field_list = &f0;
    st_tp.variant.class_struct_union.extra_info = &st_extra;

    std::memset(&union_tp, 0, sizeof(union_tp));
    union_tp.kind = tk_union;
    union_tp.size = 4;
    union_tp.alignment = 4;
    union_tp.variant.class_struct_union.field_list = &f0;

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_vptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *jit_arr = gcc_jit_context_new_array_type(ctx, NULL, jit_int, 2);
    gcc_jit_field *uf0 = gcc_jit_context_new_field(ctx, NULL, jit_int, "u0");
    gcc_jit_type *jit_union = gcc_jit_context_new_union_type(ctx, NULL, "_cov_u", 1, &uf0);
    GCC_GEN_BE_CHECK(expr_test_map_type(&st_tp, g_expr_mock.one_field_struct_type));
    GCC_GEN_BE_CHECK(expr_test_map_type(&union_tp, jit_union));
    GCC_GEN_BE_CHECK(cache_insert(
        GCC_GEN_BE_CACHE_FIELD, &f0,
        gcc_jit_struct_get_field(gcc_jit_type_is_struct(g_expr_mock.one_field_struct_type), 0)));

    gcc_jit_lvalue *lv_int = gcc_jit_function_new_local(fn, NULL, jit_int, "cov_di_i");
    gcc_jit_lvalue *lv_arr = gcc_jit_function_new_local(fn, NULL, jit_arr, "cov_di_a");
    gcc_jit_lvalue *lv_st = gcc_jit_function_new_local(fn, NULL, g_expr_mock.one_field_struct_type, "cov_di_s");
    gcc_jit_lvalue *lv_u = gcc_jit_function_new_local(fn, NULL, jit_union, "cov_di_u");
    gcc_jit_rvalue *rv = NULL;

    /* dik_zero: target_type == NULL, init_rval == NULL (void), and cblock == NULL */
    a_dynamic_init dip;
    std::memset(&dip, 0, sizeof(dip));
    dip.kind = dik_zero;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, NULL, &rv));
    a_variable void_var;
    std::memset(&void_var, 0, sizeof(void_var));
    void_var.type = &void_tp;
    dip.variable = &void_var;
    GCC_GEN_BE_CHECK(expr_test_map_type(&void_tp, jit_void));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, NULL, &rv));
    a_variable int_var;
    std::memset(&int_var, 0, sizeof(int_var));
    int_var.type = &int_tp;
    dip.variable = &int_var;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_int, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* dik_lambda & dik_constant with NULL con, ck_init_repeat corner cases */
    dip.kind = dik_lambda;
    dip.variant.constant.ptr = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_int, &rv));

    a_constant c_int, c_rep;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_int, &int_tp, 5));
    std::memset(&c_rep, 0, sizeof(c_rep));
    c_rep.kind = ck_init_repeat;
    c_rep.type = &arr_tp;
    c_rep.variant.init_repeat.count = 2;
    c_rep.variant.init_repeat.constant = &c_int;
    dip.variant.constant.ptr = &c_rep;
    /* target_lval == NULL, cblock == NULL, count == 0, constant == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    c_rep.variant.init_repeat.count = 0;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));
    c_rep.variant.init_repeat.count = 2;
    c_rep.variant.init_repeat.constant = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));
    /* Non-ck_dynamic_init elem_con (c_int) */
    c_rep.variant.init_repeat.constant = &c_int;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));

    /* ck_dynamic_init with ptr == NULL */
    a_constant c_dyn_elem;
    std::memset(&c_dyn_elem, 0, sizeof(c_dyn_elem));
    c_dyn_elem.kind = ck_dynamic_init;
    c_dyn_elem.type = &int_tp;
    c_dyn_elem.variant.dynamic_init.ptr = NULL;
    c_rep.variant.init_repeat.constant = &c_dyn_elem;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));

    /* ck_init_repeat with elem_dip->destructor (0-param, 1-param, 2-param, uncached, NULL dtor_fn, error) */
    a_type rtp_v;
    std::memset(&rtp_v, 0, sizeof(rtp_v));
    rtp_v.kind = tk_routine;
    rtp_v.variant.routine.return_type = &void_tp;
    a_routine dtor0, dtor1, dtor2, dtor_uncached;
    std::memset(&dtor0, 0, sizeof(dtor0));
    std::memset(&dtor1, 0, sizeof(dtor1));
    std::memset(&dtor2, 0, sizeof(dtor2));
    std::memset(&dtor_uncached, 0, sizeof(dtor_uncached));
    dtor0.type = &rtp_v;
    dtor1.type = &rtp_v;
    dtor2.type = &rtp_v;
    dtor_uncached.type = &rtp_v;

    gcc_jit_function *j_dtor0 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_dtor0", 0, NULL, 0);
    gcc_jit_param *dp1 = gcc_jit_context_new_param(ctx, NULL, jit_vptr, "this");
    gcc_jit_function *j_dtor1 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_dtor1", 1, &dp1, 0);
    gcc_jit_param *dp2[2] = {
        gcc_jit_context_new_param(ctx, NULL, jit_vptr, "this"),
        gcc_jit_context_new_param(ctx, NULL, jit_int, "in_chrg")
    };
    gcc_jit_function *j_dtor2 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_dtor2", 2, dp2, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &dtor0, j_dtor0));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &dtor1, j_dtor1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &dtor2, j_dtor2));

    an_expr_node e_c5;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_c5, &c_int));
    a_dynamic_init elem_dip;
    std::memset(&elem_dip, 0, sizeof(elem_dip));
    elem_dip.kind = dik_expression;
    elem_dip.variable = &int_var;
    elem_dip.variant.expression = &e_c5;
    c_dyn_elem.variant.dynamic_init.ptr = &elem_dip;

    elem_dip.destructor = &dtor0;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));
    elem_dip.destructor = &dtor1;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));
    elem_dip.destructor = &dtor2;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));
    elem_dip.destructor = &dtor_uncached;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));
    g_expr_mock.func_decl_return_null = TRUE;
    a_routine dtor_null;
    std::memset(&dtor_null, 0, sizeof(dtor_null));
    dtor_null.type = &rtp_v;
    elem_dip.destructor = &dtor_null;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));
    g_expr_mock.func_decl_return_null = FALSE;

    /* Error inside elem_dip while elem_cleanup_lp is active */
    elem_dip.destructor = &dtor2;
    an_expr_node e_bad_elem;
    std::memset(&e_bad_elem, 0, sizeof(e_bad_elem));
    e_bad_elem.kind = (an_expr_node_kind)99;
    e_bad_elem.type = &int_tp;
    elem_dip.variant.expression = &e_bad_elem;
    if (gcc_gen_be_lower_dynamic_init(&dip, lv_arr, &rv) == GCC_GEN_BE_SUCCESS) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    /* Also test emit_unwind_resume with unwind_resume_make_block == TRUE (after_resume != NULL) */
    elem_dip.variant.expression = &e_c5;
    g_expr_mock.unwind_resume_make_block = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));
    g_expr_mock.unwind_resume_make_block = FALSE;

    /* ck_aggregate with has_dynamic_init_component == TRUE:
       - target_lval == NULL, cblock == NULL
       - tk_array with ck_designator (array, field, generic, trailing NULL), non-dynamic, NULL ptr
       - tk_union & tk_struct with ck_designator (field, non-field, generic, trailing NULL), uncached field
       - agg_type = tk_integer */
    a_constant c_desig_arr, c_desig_gen, c_desig_fld, c_desig_trail, c_agg_dyn;
    std::memset(&c_desig_arr, 0, sizeof(c_desig_arr));
    c_desig_arr.kind = ck_designator;
    c_desig_arr.variant.designator.is_field_designator = FALSE;
    c_desig_arr.variant.designator.is_generic = FALSE;
    c_desig_arr.variant.designator.variant.array_element = 1;
    std::memset(&c_desig_gen, 0, sizeof(c_desig_gen));
    c_desig_gen.kind = ck_designator;
    c_desig_gen.variant.designator.is_generic = TRUE;
    std::memset(&c_desig_fld, 0, sizeof(c_desig_fld));
    c_desig_fld.kind = ck_designator;
    c_desig_fld.variant.designator.is_field_designator = TRUE;
    c_desig_fld.variant.designator.variant.field = &f0;
    std::memset(&c_desig_trail, 0, sizeof(c_desig_trail));
    c_desig_trail.kind = ck_designator;

    a_constant c_dyn_null, c_int_copy;
    std::memset(&c_dyn_null, 0, sizeof(c_dyn_null));
    c_dyn_null.kind = ck_dynamic_init;
    c_dyn_null.type = &int_tp;
    c_dyn_null.variant.dynamic_init.ptr = NULL;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_int_copy, &int_tp, 9));

    c_desig_arr.next = &c_int_copy;
    c_int_copy.next = &c_desig_gen;
    c_desig_gen.next = &c_dyn_null;
    c_dyn_null.next = &c_desig_fld;
    c_desig_fld.next = &c_int;
    c_int.next = &c_desig_trail;
    c_desig_trail.next = NULL;

    std::memset(&c_agg_dyn, 0, sizeof(c_agg_dyn));
    c_agg_dyn.kind = ck_aggregate;
    c_agg_dyn.type = &arr_tp;
    c_agg_dyn.variant.aggregate.has_dynamic_init_component = TRUE;
    c_agg_dyn.variant.aggregate.first_constant = &c_desig_arr;
    dip.kind = dik_nonconstant_aggregate;
    dip.variable = &int_var;
    dip.variant.constant.ptr = &c_agg_dyn;
    /* target_lval == NULL & cblock == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    /* Array dynamic aggregate with designators */
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));

    /* Struct & union dynamic aggregate with designators, uncached f1, f == NULL */
    c_agg_dyn.type = &st_tp;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    c_agg_dyn.type = &union_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &f0, uf0));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_u, &rv));
    GCC_GEN_BE_CHECK(cache_insert(
        GCC_GEN_BE_CACHE_FIELD, &f0,
        gcc_jit_struct_get_field(gcc_jit_type_is_struct(g_expr_mock.one_field_struct_type), 0)));
    /* Struct with empty field_list -> f == NULL */
    a_type empty_st_tp;
    std::memset(&empty_st_tp, 0, sizeof(empty_st_tp));
    empty_st_tp.kind = tk_struct;
    empty_st_tp.size = 1;
    GCC_GEN_BE_CHECK(expr_test_map_type(&empty_st_tp, g_expr_mock.one_field_struct_type));
    c_agg_dyn.type = &empty_st_tp;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    /* agg_type == NULL and agg_type == tk_integer */
    c_agg_dyn.type = NULL;
    dip.variable = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_int, &rv));
    c_agg_dyn.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_int, &rv));
    c_desig_arr.next = NULL;
    c_int_copy.next = NULL;
    c_desig_gen.next = NULL;
    c_dyn_null.next = NULL;
    c_desig_fld.next = NULL;
    c_int.next = NULL;

    /* dik_class_result_via_ctor: NULL expr, NULL init_rval, NULL cblock, struct-to-struct */
    dip.kind = dik_class_result_via_ctor;
    dip.variable = &int_var;
    dip.variant.expression = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_int, &rv));
    an_expr_node e_null_ty;
    std::memset(&e_null_ty, 0, sizeof(e_null_ty));
    e_null_ty.kind = enk_type_operand;
    e_null_ty.type = &void_tp;
    dip.variant.expression = &e_null_ty;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_int, &rv));
    dip.variant.expression = &e_c5;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_int, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    /* Struct rvalue assigned to 1-field struct target_lval (hits is_struct != NULL false branch) */
    a_variable st_src_var;
    std::memset(&st_src_var, 0, sizeof(st_src_var));
    st_src_var.type = &st_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &st_src_var, lv_st));
    an_expr_node e_st_rv;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_st_rv, &st_src_var));
    dip.variant.expression = &e_st_rv;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));

    /* dik_bitwise_copy with NULL source */
    std::memset(&dip, 0, sizeof(dip));
    dip.kind = dik_bitwise_copy;
    dip.variant.bitwise_copy.source = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));

    /* dik_constructor: value_initialization, NULL ctor, NULL ctor_fn, NULL cblock,
       num_args > param_count, alloc_args == 0, tk_struct virtual base VTT + zero fill + OOM */
    std::memset(&dip, 0, sizeof(dip));
    dip.kind = dik_constructor;
    dip.variable = &st_src_var;
    dip.variant.constructor.value_initialization = TRUE;
    dip.variant.constructor.ptr = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* 0-param ctor with target_lval == NULL and num_args == 0 -> alloc_args == 0 */
    a_routine ctor0, ctor3, ctor_null_fn;
    std::memset(&ctor0, 0, sizeof(ctor0));
    std::memset(&ctor3, 0, sizeof(ctor3));
    std::memset(&ctor_null_fn, 0, sizeof(ctor_null_fn));
    ctor0.type = &rtp_v;
    ctor3.type = &rtp_v;
    ctor_null_fn.type = &rtp_v;
    gcc_jit_function *j_ctor0 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_ctor0", 0, NULL, 0);
    gcc_jit_param *cp3[3] = {
        gcc_jit_context_new_param(ctx, NULL, jit_vptr, "this"),
        gcc_jit_context_new_param(ctx, NULL, jit_vptr, "vtt"),
        gcc_jit_context_new_param(ctx, NULL, jit_int, "extra")
    };
    gcc_jit_function *j_ctor3 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_ctor3", 3, cp3, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &ctor0, j_ctor0));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &ctor3, j_ctor3));

    dip.variant.constructor.value_initialization = FALSE;
    dip.variant.constructor.ptr = &ctor0;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, NULL, &rv));
    /* num_args > param_count: pass target_lval + 1 arg to 0-param ctor */
    dip.variant.constructor.args = &e_c5;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    dip.variant.constructor.args = NULL;

    /* NULL ctor_fn and NULL cblock */
    g_expr_mock.func_decl_return_null = TRUE;
    dip.variant.constructor.ptr = &ctor_null_fn;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    g_expr_mock.func_decl_return_null = FALSE;
    dip.variant.constructor.ptr = &ctor0;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* tk_struct with any_virtual_base_classes + 3-param ctor + active landing pad + OOM sweep */
    gcc_jit_block *ctor_lp = gcc_jit_function_new_block(fn, "ctor_lp");
    gcc_jit_block_end_with_void_return(ctor_lp, NULL);
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(ctor_lp));
    dip.variant.constructor.ptr = &ctor3;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_dynamic_init(&dip, lv_st, &rv);
    }));
    /* active_lp belonging to another function */
    gcc_jit_function *other_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, jit_void, "cov_ctor_other_fn", 0, NULL, 0);
    gcc_jit_block *other_lp = gcc_jit_function_new_block(other_fn, "ctor_other_lp");
    gcc_jit_block_end_with_void_return(other_lp, NULL);
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(other_lp));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());

    /* Constructor argument that clears current block (enk_throw) -> cblock == NULL at L883 */
    an_expr_node e_throw_arg;
    std::memset(&e_throw_arg, 0, sizeof(e_throw_arg));
    e_throw_arg.kind = enk_throw;
    e_throw_arg.type = &int_tp;
    dip.variant.constructor.ptr = &ctor3;
    dip.variant.constructor.args = &e_throw_arg;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    dip.variant.constructor.args = NULL;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* default kind + dip->destructor registration (NULL dtor_fn, active scope with/without EH & cblock, OOM) */
    std::memset(&dip, 0, sizeof(dip));
    dip.kind = (a_dynamic_init_kind)99;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));

    dip.kind = dik_none;
    g_expr_mock.func_decl_return_null = TRUE;
    dip.destructor = &dtor_null;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    g_expr_mock.func_decl_return_null = FALSE;

    dip.destructor = &dtor1;
    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
    exceptions_enabled = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    exceptions_enabled = FALSE;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    exceptions_enabled = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_dynamic_init(&dip, lv_st, &rv);
    }));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));

    /* scale_vla_subarray_index edge branches via 2D VLA subscript */
    a_type vla_inner_tp, vla_outer_tp, vla_ptr_tp, long_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    std::memset(&vla_inner_tp, 0, sizeof(vla_inner_tp));
    vla_inner_tp.kind = tk_array;
    vla_inner_tp.variant.array.element_type = &int_tp;
    vla_inner_tp.variant.array.is_variable_size_array = TRUE;
    vla_inner_tp.variant.array.has_assoc_vla_dimension = TRUE;
    std::memset(&vla_outer_tp, 0, sizeof(vla_outer_tp));
    vla_outer_tp.kind = tk_array;
    vla_outer_tp.variant.array.element_type = &vla_inner_tp;
    vla_outer_tp.variant.array.is_variable_size_array = TRUE;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vla_ptr_tp, &vla_inner_tp, FALSE));

    a_variable vla_base_var, dim_var_uncached;
    std::memset(&vla_base_var, 0, sizeof(vla_base_var));
    vla_base_var.type = &vla_ptr_tp;
    gcc_jit_lvalue *vla_ptr_lv = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_type_get_pointer(jit_int), "cov_vla_ptr");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vla_base_var, vla_ptr_lv));
    std::memset(&dim_var_uncached, 0, sizeof(dim_var_uncached));
    dim_var_uncached.type = &long_tp;

    a_vla_dimension dim_orig, dim_alias;
    std::memset(&dim_orig, 0, sizeof(dim_orig));
    dim_orig.type = &vla_inner_tp;
    dim_orig.dimension_variable = &dim_var_uncached;
    std::memset(&dim_alias, 0, sizeof(dim_alias));
    dim_alias.type = &vla_inner_tp;
    dim_alias.dimension_variable = NULL;
    dim_alias.original_dimension = &dim_orig;

    a_scope fn_scope;
    std::memset(&fn_scope, 0, sizeof(fn_scope));
    fn_scope.kind = sck_function;
    fn_scope.vla_dimensions = &dim_alias;
    innermost_function_scope = &fn_scope;

    an_expr_node e_vla_base, e_idx, e_sub;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vla_base, &vla_base_var));
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_idx, &c_int));
    e_vla_base.next = &e_idx;
    GCC_GEN_BE_CHECK(expr_init_op(&e_sub, eok_subscript, &vla_inner_tp, &e_vla_base));

    /* 1) idx == NULL (void expr index) */
    e_vla_base.next = &e_null_ty;
    gcc_jit_lvalue *sub_lv = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub, &sub_lv));
    /* 2) elem_tp == NULL */
    e_vla_base.next = &e_idx;
    vla_ptr_tp.variant.pointer.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub, &sub_lv));
    vla_ptr_tp.variant.pointer.type = &vla_inner_tp;

    /* 3) cblock == NULL (cur_fn == NULL) with uncached dim_var_uncached -> dim_lv == NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub, &sub_lv));
    /* 4) cblock != NULL with uncached dim_var_uncached -> allocates local on cur_fn */
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub, &sub_lv));

    /* 5) dim chain with original_dimension == NULL & dimension_variable == NULL, and dim not found */
    dim_alias.original_dimension = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub, &sub_lv));
    fn_scope.vla_dimensions = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub, &sub_lv));

    /* 6) innermost_function_scope == NULL & has_assoc_vla_dimension == FALSE with number_of_elements > 0 and == 0 */
    innermost_function_scope = NULL;
    vla_inner_tp.variant.array.variant.number_of_elements = 4;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub, &sub_lv));
    vla_inner_tp.variant.array.has_assoc_vla_dimension = FALSE;
    vla_inner_tp.variant.array.variant.number_of_elements = 4;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub, &sub_lv));
    vla_inner_tp.variant.array.variant.number_of_elements = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub, &sub_lv));
    e_vla_base.next = NULL;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Covers bitfield read/assignment, `safe_ptr_or_int_cast`, address constants, and aggregate/scalar constants.
 * @details Exercises `gcc_gen_be_lower_bitfield_read`, `gcc_gen_be_lower_bitfield_assignment`,
 * `safe_ptr_or_int_cast`, `lower_address_constant`, `lower_struct_aggregate_constant_inner`,
 * `lower_aggregate_constant`, and `gcc_gen_be_lower_constant_rvalue` across all uncovered branches.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_part_a_bitfields_and_constants(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, uint_tp, zero_sz_tp, u64_tp, st_tp, st_ptr_tp, void_tp, vptr_tp, char_tp, cptr_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&uint_tp, ik_unsigned_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&zero_sz_tp, ik_int, 0));
    GCC_GEN_BE_CHECK(expr_init_int_type(&u64_tp, ik_unsigned_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&char_tp, ik_char, 1));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&cptr_tp, &char_tp, FALSE));

    a_field bf;
    std::memset(&bf, 0, sizeof(bf));
    bf.is_bit_field = TRUE;
    bf.bit_field_is_signed = TRUE;
    bf.bit_size = 5;
    bf.type = &zero_sz_tp;

    std::memset(&st_tp, 0, sizeof(st_tp));
    st_tp.kind = tk_struct;
    st_tp.size = 8;
    st_tp.alignment = 8;
    st_tp.variant.class_struct_union.field_list = &bf;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&st_ptr_tp, &st_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_test_map_type(&st_tp, g_expr_mock.one_field_struct_type));

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_ulong = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_vptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *jit_char = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
    gcc_jit_type *jit_cptr = gcc_jit_type_get_pointer(jit_char);

    a_variable st_var, st_ptr_var;
    std::memset(&st_var, 0, sizeof(st_var));
    st_var.type = &st_tp;
    std::memset(&st_ptr_var, 0, sizeof(st_ptr_var));
    st_ptr_var.type = &st_ptr_tp;
    gcc_jit_lvalue *lv_st = gcc_jit_function_new_local(fn, NULL, g_expr_mock.one_field_struct_type, "cov_bf_s");
    gcc_jit_lvalue *lv_sp = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_type_get_pointer(g_expr_mock.one_field_struct_type), "cov_bf_sp");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &st_var, lv_st));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &st_ptr_var, lv_sp));

    an_expr_node e_st, e_sp, e_bf, e_dot_bf, e_arrow_bf;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_st, &st_var));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_sp, &st_ptr_var));
    std::memset(&e_bf, 0, sizeof(e_bf));
    e_bf.kind = enk_field;
    e_bf.type = &int_tp;
    e_bf.variant.field.ptr = &bf;
    e_st.next = &e_bf;
    e_sp.next = &e_bf;
    GCC_GEN_BE_CHECK(expr_init_op(&e_dot_bf, eok_dot_field, &int_tp, &e_st));
    GCC_GEN_BE_CHECK(expr_init_op(&e_arrow_bf, eok_points_to_field, &int_tp, &e_sp));

    gcc_jit_rvalue *rv = NULL;
    /* Bitfield read: arrow (->), unit_sz == 0, signed bitfield with word_bits == 0 & shift_amt > 0 */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_arrow_bf, &rv));
    /* Bitfield read & assign with bit_size == 64 (mask_val = ~0UL and shift_amt <= 0) */
    bf.type = &u64_tp;
    bf.bit_size = 64;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_dot_bf, &rv));

    /* Bitfield pre/post incr & decr (rhs_expr == NULL, map_compound_assign_binary_op, post_incr/decr, NULL cblock) */
    bf.type = &zero_sz_tp;
    bf.bit_size = 5;
    an_expr_node e_bf_op;
    GCC_GEN_BE_CHECK(expr_init_op(&e_bf_op, eok_pre_incr, &int_tp, &e_arrow_bf));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_bf_op, &rv));
    e_bf_op.variant.operation.kind = eok_post_incr;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_bf_op, &rv));
    e_bf_op.variant.operation.kind = eok_pre_decr;
    bf.type = &u64_tp;
    bf.bit_size = 64;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_bf_op, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    e_bf_op.variant.operation.kind = eok_post_decr;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_bf_op, &rv));

    /* !base_lval in bitfield read & bitfield assignment (eok_dot_field with NULL-deref eok_indirect op1) */
    an_expr_node e_null_ptr_src, e_null_lval_base;
    std::memset(&e_null_ptr_src, 0, sizeof(e_null_ptr_src));
    e_null_ptr_src.kind = enk_type_operand;
    e_null_ptr_src.type = &st_ptr_tp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_null_lval_base, eok_indirect, &st_tp, &e_null_ptr_src));
    e_null_lval_base.next = &e_bf;
    e_dot_bf.variant.operation.operands = &e_null_lval_base;
    if (gcc_gen_be_lower_expr_rvalue(&e_dot_bf, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_bf_op.variant.operation.kind = eok_pre_incr;
    e_bf_op.variant.operation.operands = &e_dot_bf;
    if (gcc_gen_be_lower_expr_rvalue(&e_bf_op, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_null_lval_base.next = NULL;
    e_st.next = NULL;
    e_sp.next = NULL;

    /* safe_ptr_or_int_cast edge cases via eok_cast:
       - val == NULL or dest_type == NULL
       - int-to-char* with cblock == NULL (dest_type == char_ptr_t)
       - ptr-to-ulong with cblock == NULL (dest_type == uptr_type, bitcast != NULL and == NULL)
       - int-to-void and void-call-to-int (with cblock != NULL and cblock == NULL)
       - vector-to-vector with cblock == NULL and bitcast == NULL */
    a_constant c_seven;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_seven, &int_tp, 7));
    an_expr_node e_c7, e_cast;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_c7, &c_seven));
    GCC_GEN_BE_CHECK(expr_init_op(&e_cast, eok_cast, &cptr_tp, &e_c7));
    GCC_GEN_BE_CHECK(expr_test_map_type(&cptr_tp, jit_cptr));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast, &rv));

    a_type ulong_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&ulong_tp, ik_unsigned_long, 8));
    GCC_GEN_BE_CHECK(expr_test_map_type(&ulong_tp, jit_ulong));
    a_variable vptr_var;
    std::memset(&vptr_var, 0, sizeof(vptr_var));
    vptr_var.type = &vptr_tp;
    gcc_jit_lvalue *lv_vp = gcc_jit_function_new_local(fn, NULL, jit_vptr, "cov_vp");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vptr_var, lv_vp));
    an_expr_node e_vp;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vp, &vptr_var));
    e_cast.type = &ulong_tp;
    e_cast.variant.operation.operands = &e_vp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast, &rv));
    auto saved_bc = p_gcc_jit_context_new_bitcast;
    p_gcc_jit_context_new_bitcast = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast, &rv));
    /* Also ptr-to-int (dest_type != uptr_type) with bitcast == NULL */
    e_cast.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast, &rv));

    /* Vector-to-vector cast with cblock == NULL and p_gcc_jit_context_new_bitcast == NULL */
    gcc_jit_type *vec4i = gcc_jit_type_get_vector(jit_int, 4);
    gcc_jit_type *vec4f = gcc_jit_type_get_vector(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT), 4);
    a_type edg_v4i, edg_v4f;
    std::memset(&edg_v4i, 0, sizeof(edg_v4i));
    std::memset(&edg_v4f, 0, sizeof(edg_v4f));
    edg_v4i.kind = tk_vector;
    edg_v4f.kind = tk_vector;
    GCC_GEN_BE_CHECK(expr_test_map_type(&edg_v4i, vec4i));
    GCC_GEN_BE_CHECK(expr_test_map_type(&edg_v4f, vec4f));
    a_variable v4i_var;
    std::memset(&v4i_var, 0, sizeof(v4i_var));
    v4i_var.type = &edg_v4i;
    gcc_jit_lvalue *lv_v4i = gcc_jit_function_new_local(fn, NULL, vec4i, "cov_v4i");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v4i_var, lv_v4i));
    an_expr_node e_v4i;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_v4i, &v4i_var));
    e_cast.type = &edg_v4f;
    e_cast.variant.operation.operands = &e_v4i;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast, &rv));
    p_gcc_jit_context_new_bitcast = saved_bc;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* Void src_type cast to int (with cblock != NULL and cblock == NULL), and int cast to void */
    a_type vfn_tp;
    std::memset(&vfn_tp, 0, sizeof(vfn_tp));
    vfn_tp.kind = tk_routine;
    vfn_tp.variant.routine.return_type = &void_tp;
    a_routine vfn_r;
    std::memset(&vfn_r, 0, sizeof(vfn_r));
    vfn_r.type = &vfn_tp;
    gcc_jit_function *j_vfn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_vfn2", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &vfn_r, j_vfn));
    an_expr_node e_vr, e_vcall;
    std::memset(&e_vr, 0, sizeof(e_vr));
    e_vr.kind = enk_routine;
    e_vr.type = &vfn_tp;
    e_vr.variant.routine.ptr = &vfn_r;
    GCC_GEN_BE_CHECK(expr_init_op(&e_vcall, eok_call, &void_tp, &e_vr));
    GCC_GEN_BE_CHECK(expr_test_map_type(&void_tp, jit_void));
    e_cast.type = &int_tp;
    e_cast.variant.operation.operands = &e_vcall;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* Exercise safe_ptr_or_int_cast dest_type == void_t (via eok_eq with void result),
       val == NULL (via pointer-minus-pointer eok_subtract L3308), and dest_type == NULL (via ck_integer L2612)! */
    an_expr_node e_c7_b, e_eq_void;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_c7_b, &c_seven));
    e_c7.next = &e_c7_b;
    GCC_GEN_BE_CHECK(expr_init_op(&e_eq_void, eok_eq, &void_tp, &e_c7));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eq_void, &rv));
    e_c7.next = NULL;

    an_expr_node e_null_ptr_val, e_psub;
    std::memset(&e_null_ptr_val, 0, sizeof(e_null_ptr_val));
    e_null_ptr_val.kind = enk_type_operand;
    e_null_ptr_val.type = &cptr_tp;
    e_vp.type = &cptr_tp;
    e_null_ptr_val.next = &e_vp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_psub, eok_pdiff, &int_tp, &e_null_ptr_val));
    if (gcc_gen_be_lower_expr_rvalue(&e_psub, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_null_ptr_val.next = NULL;
    e_vp.type = &vptr_tp;

    a_constant c_addr_null_dest;
    std::memset(&c_addr_null_dest, 0, sizeof(c_addr_null_dest));
    c_addr_null_dest.kind = ck_address;
    c_addr_null_dest.type = NULL;
    c_addr_null_dest.variant.address.kind = abk_variable;
    c_addr_null_dest.variant.address.variant.variable = &vptr_var;
    c_addr_null_dest.variant.address.offset = 0;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr_null_dest, NULL, &rv));

    e_c7.next = &e_c7_b;
    e_cast.variant.operation.kind = eok_and;
    e_cast.type = NULL;
    e_cast.variant.operation.operands = &e_c7;
    if (gcc_gen_be_lower_expr_rvalue(&e_cast, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_c7.next = NULL;

    /* lower_address_constant & gcc_gen_be_lower_constant_rvalue branches */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_constant_rvalue(&c_seven, jit_int, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* Address constant:
       - expected_type == NULL and con->type == NULL
       - abk_variable with NULL var, decl error, decl NULL
       - abk_label with NULL innermost_function_scope->variant.routine.ptr & uncached routine
       - abk_routine with NULL rout, decl error, decl NULL
       - abk_constant & abk_temporary: string literal with const char*, uchar*, schar*, const uchar*, 0-len, NULL value
       - abk_temporary & abk_constant with non-string inner constant (cached & uncached, offset == 0 & != 0, error)
       - unknown abk kind -> !base_rval
       - non-zero offset on _ZTT/_ZTI array, zero-sized array, odd offset, tk_class field array with odd rem_off */
    a_constant c_addr;
    std::memset(&c_addr, 0, sizeof(c_addr));
    c_addr.kind = ck_address;
    c_addr.type = NULL;
    c_addr.variant.address.kind = (an_address_base_kind)99;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, NULL, &rv));

    c_addr.variant.address.kind = abk_variable;
    c_addr.variant.address.variant.variable = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, jit_vptr, &rv));
    a_variable uncached_var;
    std::memset(&uncached_var, 0, sizeof(uncached_var));
    uncached_var.type = &int_tp;
    c_addr.variant.address.variant.variable = &uncached_var;
    g_expr_mock.force_global_decl_err = GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, jit_vptr, &rv));
    g_expr_mock.force_global_decl_err = GCC_GEN_BE_SUCCESS;
    g_expr_mock.global_var_return_null = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, jit_vptr, &rv));
    g_expr_mock.global_var_return_null = FALSE;

    a_scope lbl_scope;
    std::memset(&lbl_scope, 0, sizeof(lbl_scope));
    a_routine uncached_fn_rout;
    std::memset(&uncached_fn_rout, 0, sizeof(uncached_fn_rout));
    c_addr.variant.address.kind = abk_label;
    innermost_function_scope = &lbl_scope;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, jit_vptr, &rv));
    lbl_scope.variant.routine.ptr = &uncached_fn_rout;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, jit_vptr, &rv));
    innermost_function_scope = NULL;

    c_addr.variant.address.kind = abk_routine;
    c_addr.variant.address.variant.routine = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, jit_vptr, &rv));
    a_routine uncached_r2;
    std::memset(&uncached_r2, 0, sizeof(uncached_r2));
    uncached_r2.type = &vfn_tp;
    c_addr.variant.address.variant.routine = &uncached_r2;
    g_expr_mock.force_func_decl_err = GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, jit_vptr, &rv));
    g_expr_mock.force_func_decl_err = GCC_GEN_BE_SUCCESS;
    g_expr_mock.func_decl_return_null = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, jit_vptr, &rv));
    g_expr_mock.func_decl_return_null = FALSE;

    /* String constants under abk_constant with various pointer element types & 0-len */
    a_constant c_str;
    std::memset(&c_str, 0, sizeof(c_str));
    c_str.kind = ck_string;
    c_str.variant.string.value = "hi";
    c_str.variant.string.length = 3;
    c_addr.variant.address.kind = abk_constant;
    c_addr.variant.address.variant.constant = &c_str;
    c_addr.variant.address.offset = 0;

    gcc_jit_type *const_cptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CONST_CHAR_PTR);
    gcc_jit_type *uchar_ptr_t = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR));
    gcc_jit_type *schar_ptr_t = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIGNED_CHAR));
    gcc_jit_type *cchar_ptr_t = gcc_jit_type_get_pointer(gcc_jit_type_get_const(jit_char));
    gcc_jit_type *cuchar_ptr_t = gcc_jit_type_get_pointer(
        gcc_jit_type_get_const(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR)));

    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, const_cptr_t, &rv));
    /* Offset != 0 with const_char_ptr_t -> exercises global char array path */
    c_addr.variant.address.offset = 1;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, const_cptr_t, &rv));
    /* Cached existing_g lookup hit on second call with same c_addr! */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, const_cptr_t, &rv));

    a_constant c_addr2, c_addr3, c_addr4, c_addr5, c_str_empty;
    c_addr2 = c_addr;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr2, uchar_ptr_t, &rv));
    c_addr3 = c_addr;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr3, schar_ptr_t, &rv));
    c_addr4 = c_addr;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr4, cchar_ptr_t, &rv));
    c_addr5 = c_addr;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr5, cuchar_ptr_t, &rv));

    std::memset(&c_str_empty, 0, sizeof(c_str_empty));
    c_str_empty.kind = ck_string;
    c_str_empty.variant.string.value = NULL;
    c_str_empty.variant.string.length = 0;
    a_constant c_addr_empty = c_addr;
    c_addr_empty.variant.address.variant.constant = &c_str_empty;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr_empty, jit_cptr, &rv));

    /* abk_temporary and abk_constant with non-string inner_con (int, NULL inner_con, NULL type, error) */
    a_constant c_addr_tmp;
    std::memset(&c_addr_tmp, 0, sizeof(c_addr_tmp));
    c_addr_tmp.kind = ck_address;
    c_addr_tmp.variant.address.kind = abk_temporary;
    c_addr_tmp.variant.address.variant.constant = NULL;
    if (gcc_gen_be_lower_constant_rvalue(&c_addr_tmp, jit_vptr, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    a_constant c_notype_int;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_notype_int, NULL, 0));
    c_addr_tmp.variant.address.variant.constant = &c_notype_int;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr_tmp, jit_vptr, &rv));

    c_addr_tmp.variant.address.variant.constant = &c_seven;
    c_addr_tmp.variant.address.offset = 0;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr_tmp, jit_vptr, &rv));
    /* cblock == NULL -> hits else if (inner_val && inner_type) global materialization + cache hit! */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    a_constant c_addr_con_int = c_addr_tmp;
    c_addr_con_int.variant.address.kind = abk_constant;
    c_addr_con_int.variant.address.offset = 4;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr_con_int, jit_vptr, &rv));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr_con_int, jit_vptr, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    /* Inner constant error propagation */
    a_constant c_bad_inner;
    std::memset(&c_bad_inner, 0, sizeof(c_bad_inner));
    c_bad_inner.kind = ck_void;
    c_bad_inner.type = &int_tp;
    c_addr_tmp.variant.address.variant.constant = &c_bad_inner;
    /* ck_void lowers to NULL inner_val -> hits inner_val == NULL false branches! */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr_tmp, jit_vptr, &rv));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        c_addr_tmp.variant.address.variant.constant = &c_seven;
        return gcc_gen_be_lower_constant_rvalue(&c_addr_tmp, jit_vptr, &rv);
    }));

    /* Non-zero offset on abk_variable:
       - _ZTT and _ZTI array globals, zero-size array, non-divisible offset, pointer/struct lval_type
       - tk_class with array field (divisible and non-divisible rem_off), uncached field, non-array/struct var */
    a_type arr2_tp, arr0_tp;
    std::memset(&arr2_tp, 0, sizeof(arr2_tp));
    arr2_tp.kind = tk_array;
    arr2_tp.size = 16;
    arr2_tp.variant.array.element_type = &u64_tp;
    arr2_tp.variant.array.variant.number_of_elements = 2;
    std::memset(&arr0_tp, 0, sizeof(arr0_tp));
    arr0_tp.kind = tk_array;
    arr0_tp.size = 0;
    arr0_tp.variant.array.element_type = &u64_tp;
    arr0_tp.variant.array.variant.number_of_elements = 0;

    gcc_jit_type *jit_arr2 = gcc_jit_context_new_array_type(ctx, NULL, jit_vptr, 2);
    a_variable v_ztt, v_zti, v_arr0, v_noname, v_ptr_arr;
    std::memset(&v_ztt, 0, sizeof(v_ztt));
    v_ztt.type = &arr2_tp;
    v_ztt.source_corresp.name = "_ZTT_cov";
    std::memset(&v_zti, 0, sizeof(v_zti));
    v_zti.type = &arr2_tp;
    v_zti.source_corresp.name = "_ZTI_cov";
    std::memset(&v_arr0, 0, sizeof(v_arr0));
    v_arr0.type = &arr0_tp;
    v_arr0.source_corresp.name = "plain_arr0";
    std::memset(&v_noname, 0, sizeof(v_noname));
    v_noname.type = &arr2_tp;
    v_noname.source_corresp.name = NULL;
    std::memset(&v_ptr_arr, 0, sizeof(v_ptr_arr));
    v_ptr_arr.type = &arr2_tp;

    gcc_jit_lvalue *g_ztt = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, jit_arr2, "_ZTT_cov");
    gcc_jit_lvalue *g_zti = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, jit_arr2, "_ZTI_cov");
    gcc_jit_lvalue *g_arr0 = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, jit_arr2, "plain_arr0");
    gcc_jit_lvalue *g_noname = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, jit_arr2, "noname_arr");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_ztt, g_ztt));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_zti, g_zti));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_arr0, g_arr0));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_noname, g_noname));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_ptr_arr, lv_vp));

    a_constant c_off;
    std::memset(&c_off, 0, sizeof(c_off));
    c_off.kind = ck_address;
    c_off.variant.address.kind = abk_variable;
    c_off.variant.address.offset = 8;
    c_off.variant.address.variant.variable = &v_ztt;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));
    c_off.variant.address.variant.variable = &v_zti;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));
    c_off.variant.address.variant.variable = &v_arr0;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));
    c_off.variant.address.variant.variable = &v_noname;
    c_off.variant.address.offset = 3; /* non-divisible offset */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));
    c_off.variant.address.variant.variable = &v_ptr_arr;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));

    /* tk_class variable with array field (divisible and non-divisible rem_off) and uncached field */
    a_field cls_arr_fld, cls_uncached_fld;
    std::memset(&cls_arr_fld, 0, sizeof(cls_arr_fld));
    cls_arr_fld.offset = 0;
    cls_arr_fld.type = &arr2_tp;
    std::memset(&cls_uncached_fld, 0, sizeof(cls_uncached_fld));
    cls_uncached_fld.offset = 16;
    cls_uncached_fld.type = &int_tp;
    cls_arr_fld.next = &cls_uncached_fld;

    a_type cls_arr_tp;
    std::memset(&cls_arr_tp, 0, sizeof(cls_arr_tp));
    cls_arr_tp.kind = tk_class;
    cls_arr_tp.size = 24;
    cls_arr_tp.variant.class_struct_union.field_list = &cls_arr_fld;
    gcc_jit_field *j_caf = gcc_jit_context_new_field(ctx, NULL, jit_arr2, "caf");
    gcc_jit_type *j_cls_arr = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cov_cls_arr", 1, &j_caf));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &cls_arr_fld, j_caf));
    a_variable v_cls_arr;
    std::memset(&v_cls_arr, 0, sizeof(v_cls_arr));
    v_cls_arr.type = &cls_arr_tp;
    gcc_jit_lvalue *g_cls_arr = gcc_jit_context_new_global(
        ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, j_cls_arr, "g_cls_arr");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_cls_arr, g_cls_arr));

    c_off.variant.address.variant.variable = &v_cls_arr;
    c_off.variant.address.offset = 8; /* divisible rem_off */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));
    c_off.variant.address.offset = 3; /* non-divisible rem_off */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));
    c_off.variant.address.offset = 16; /* hits uncached field */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));

    /* Integer constants: _BitInt(256) / unsigned _BitInt(128), expected_type == NULL on 128-bit,
       val == 0 on struct/float/double/long double/void expected_type, NULL ctype */
    a_type bitint128_tp, ubitint128_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&bitint128_tp, ik_bit_precise, 16));
    GCC_GEN_BE_CHECK(expr_init_int_type(&ubitint128_tp, ik_unsigned_bit_precise, 16));
    a_constant c_bi128, c_ubi128, c_zero_nt;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_bi128, &bitint128_tp, 42));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_ubi128, &ubitint128_tp, 42));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_bi128, NULL, &rv));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_ubi128, NULL, &rv));

    GCC_GEN_BE_CHECK(expr_init_int_const(&c_zero_nt, NULL, 0));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_zero_nt, jit_int, &rv));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_zero_nt, g_expr_mock.one_field_struct_type, &rv));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_zero_nt, jit_void, &rv));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(
        &c_zero_nt, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT), &rv));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(
        &c_zero_nt, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE), &rv));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(
        &c_zero_nt, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE), &rv));

    /* String constant into array type: 0-byte elem, 0-len string, non-array expected_type (!arr_type) */
    a_type str_arr_tp, zero_elem_arr_tp;
    std::memset(&str_arr_tp, 0, sizeof(str_arr_tp));
    str_arr_tp.kind = tk_array;
    str_arr_tp.variant.array.element_type = &char_tp;
    str_arr_tp.variant.array.variant.number_of_elements = 0;
    std::memset(&zero_elem_arr_tp, 0, sizeof(zero_elem_arr_tp));
    zero_elem_arr_tp.kind = tk_array;
    zero_elem_arr_tp.variant.array.element_type = &zero_sz_tp;
    zero_elem_arr_tp.variant.array.variant.number_of_elements = 2;

    a_constant c_str_arr;
    std::memset(&c_str_arr, 0, sizeof(c_str_arr));
    c_str_arr.kind = ck_string;
    c_str_arr.type = &str_arr_tp;
    c_str_arr.variant.string.value = NULL;
    c_str_arr.variant.string.length = 0;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_str_arr, jit_int, &rv));
    c_str_arr.type = &zero_elem_arr_tp;
    c_str_arr.variant.string.value = "a";
    c_str_arr.variant.string.length = 1;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_str_arr, jit_int, &rv));
    /* String constant with non-array con->type and non-pointer expected_type */
    c_str_arr.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_str_arr, jit_int, &rv));

    /* Pointer-to-member constant: cast_to_base == TRUE, NULL pm_type, virtual fn at vbyte_off == 0, ARM64 PMF */
    a_base_class pm_base;
    std::memset(&pm_base, 0, sizeof(pm_base));
    pm_base.offset = 16;
    a_field pm_fld;
    std::memset(&pm_fld, 0, sizeof(pm_fld));
    pm_fld.offset = 8;
    a_constant c_pm;
    std::memset(&c_pm, 0, sizeof(c_pm));
    c_pm.kind = ck_ptr_to_member;
    c_pm.type = NULL;
    c_pm.variant.ptr_to_member.is_function_ptr = FALSE;
    c_pm.variant.ptr_to_member.cast_to_base = TRUE;
    c_pm.variant.ptr_to_member.casting_base_class = &pm_base;
    c_pm.variant.ptr_to_member.variant.field = &pm_fld;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_pm, NULL, &rv));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_pm, g_expr_mock.pmf_struct_type, &rv));

    a_routine vfn0;
    std::memset(&vfn0, 0, sizeof(vfn0));
    vfn0.type = &vfn_tp;
    vfn0.is_virtual = TRUE;
    vfn0.number.virtual_function = 0;
    c_pm.variant.ptr_to_member.is_function_ptr = TRUE;
    c_pm.variant.ptr_to_member.variant.routine = &vfn0;
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_pm, jit_int, &rv));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_pm, g_expr_mock.pmf_struct_type, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Covers `lower_struct_aggregate_constant_inner` and `lower_aggregate_constant` edge cases.
 * @details Exercises virtual/empty bases, empty fields, field designators, bitfield packing across
 * designators and spanning 64-bit words, 1-field wrapper struct coercions, union empty init, and array designators.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_part_a_aggregates(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, zero_sz_tp, vptr_tp, void_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&zero_sz_tp, ik_int, 0));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_vptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_rvalue *rv = NULL;

    /* 1) Array aggregate constant with ck_designator (valid & trailing NULL) + OOM sweep */
    a_type arr_tp;
    std::memset(&arr_tp, 0, sizeof(arr_tp));
    arr_tp.kind = tk_array;
    arr_tp.size = 8;
    arr_tp.variant.array.element_type = &int_tp;
    arr_tp.variant.array.variant.number_of_elements = 2;
    gcc_jit_type *jit_arr = gcc_jit_context_new_array_type(ctx, NULL, jit_int, 2);

    a_constant c_d0, c_v0, c_d_trail, c_arr_agg;
    std::memset(&c_d0, 0, sizeof(c_d0));
    c_d0.kind = ck_designator;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_v0, &int_tp, 12));
    std::memset(&c_d_trail, 0, sizeof(c_d_trail));
    c_d_trail.kind = ck_designator;
    c_d0.next = &c_v0;
    c_v0.next = &c_d_trail;
    c_d_trail.next = NULL;

    std::memset(&c_arr_agg, 0, sizeof(c_arr_agg));
    c_arr_agg.kind = ck_aggregate;
    c_arr_agg.type = &arr_tp;
    c_arr_agg.variant.aggregate.first_constant = &c_d0;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_constant_rvalue(&c_arr_agg, jit_arr, &rv);
    }));

    /* 2) Empty union aggregate constant (count == 0 -> get_zero_rvalue) */
    a_type u_tp;
    std::memset(&u_tp, 0, sizeof(u_tp));
    u_tp.kind = tk_union;
    u_tp.size = 4;
    gcc_jit_field *uf = gcc_jit_context_new_field(ctx, NULL, jit_int, "uf");
    gcc_jit_type *jit_u = gcc_jit_context_new_union_type(ctx, NULL, "_cov_agg_u", 1, &uf);
    GCC_GEN_BE_CHECK(expr_test_map_type(&u_tp, jit_u));
    a_constant c_u_empty;
    std::memset(&c_u_empty, 0, sizeof(c_u_empty));
    c_u_empty.kind = ck_aggregate;
    c_u_empty.type = &u_tp;
    c_u_empty.variant.aggregate.first_constant = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_u_empty, NULL, &rv));

    /* 3) Struct aggregate with:
       - leading virtual base + optimized empty base + non-virtual base + trailing virtual base
       - optimized empty field
       - explicit field_or_base.base designator
       - fallback positional struct field when jit_f == NULL and f_type == NULL
       - spanning bitfield (offset_bit_remainder + bit_size > 32, con_tp->size >= 8) + designated next bitfield */
    a_type base_st_tp;
    std::memset(&base_st_tp, 0, sizeof(base_st_tp));
    base_st_tp.kind = tk_struct;
    base_st_tp.size = 4;
    GCC_GEN_BE_CHECK(expr_test_map_type(&base_st_tp, g_expr_mock.one_field_struct_type));

    a_base_class b_virt1, b_empty, b_virt2, b_real;
    std::memset(&b_virt1, 0, sizeof(b_virt1));
    b_virt1.is_virtual = TRUE;
    std::memset(&b_empty, 0, sizeof(b_empty));
    b_empty.is_optimized_empty_base = TRUE;
    b_empty.type = &base_st_tp;
    std::memset(&b_virt2, 0, sizeof(b_virt2));
    b_virt2.is_virtual = TRUE;
    std::memset(&b_real, 0, sizeof(b_real));
    b_real.type = &base_st_tp;

    b_virt1.next = &b_empty;
    b_empty.next = &b_virt2;
    b_virt2.next = NULL;

    a_field f_empty, f_real;
    std::memset(&f_empty, 0, sizeof(f_empty));
    f_empty.is_optimized_empty_class = TRUE;
    f_empty.type = &int_tp;
    std::memset(&f_real, 0, sizeof(f_real));
    f_real.type = &int_tp;
    f_empty.next = &f_real;

    a_class_type_supplement sup1;
    std::memset(&sup1, 0, sizeof(sup1));
    sup1.direct_base_classes = &b_virt1;

    a_type cls1_tp;
    std::memset(&cls1_tp, 0, sizeof(cls1_tp));
    cls1_tp.kind = tk_class;
    cls1_tp.size = 8;
    cls1_tp.variant.class_struct_union.extra_info = &sup1;
    cls1_tp.variant.class_struct_union.field_list = &f_empty;
    GCC_GEN_BE_CHECK(expr_test_map_type(&cls1_tp, g_expr_mock.one_field_struct_type));

    a_constant c_empty_base_agg, c_empty_fld_val, c_real_fld_val, c_cls1_agg;
    std::memset(&c_empty_base_agg, 0, sizeof(c_empty_base_agg));
    c_empty_base_agg.kind = ck_aggregate;
    c_empty_base_agg.type = &base_st_tp;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_empty_fld_val, &int_tp, 1));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_real_fld_val, &int_tp, 2));
    c_empty_base_agg.next = &c_empty_fld_val;
    c_empty_fld_val.next = &c_real_fld_val;

    std::memset(&c_cls1_agg, 0, sizeof(c_cls1_agg));
    c_cls1_agg.kind = ck_aggregate;
    c_cls1_agg.type = &cls1_tp;
    c_cls1_agg.variant.aggregate.first_constant = &c_empty_base_agg;
    /* Note: f_real is intentionally NOT in GCC_GEN_BE_CACHE_FIELD so positional struct_get_field fallback runs! */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_cls1_agg, g_expr_mock.one_field_struct_type, &rv));

    /* Test b_cur transitioning to scalar constant (if (b_cur) { b_cur = NULL; f = field_list; }),
       explicit field_or_base.base lookup, NULL con_tp with struct expected_type, and type != struct_type wrapper! */
    b_virt2.next = &b_real;
    c_empty_base_agg.next = &c_real_fld_val;
    c_real_fld_val.next = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_cls1_agg, g_expr_mock.one_field_struct_type, &rv));

    /* Explicit field_or_base.base when b_cur == NULL */
    sup1.direct_base_classes = NULL;
    cls1_tp.variant.class_struct_union.field_list = NULL;
    gcc_jit_field *j_f0 = gcc_jit_struct_get_field(
        gcc_jit_type_is_struct(g_expr_mock.one_field_struct_type), 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &b_real, j_f0));
    a_constant c_base_desig_agg;
    std::memset(&c_base_desig_agg, 0, sizeof(c_base_desig_agg));
    c_base_desig_agg.kind = ck_aggregate;
    c_base_desig_agg.type = &int_tp;
    c_base_desig_agg.variant.aggregate.field_or_base.base = &b_real;
    c_base_desig_agg.variant.aggregate.first_constant = &c_real_fld_val;
    c_cls1_agg.variant.aggregate.first_constant = &c_base_desig_agg;
    /* Pass a DIFFERENT outer 1-field struct `expected_type` to hit `type != struct_type` wrapper at L2436! */
    gcc_jit_field *outer_f = gcc_jit_context_new_field(ctx, NULL, g_expr_mock.one_field_struct_type, "inner");
    gcc_jit_type *outer_wrap_st = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cov_outer_wrap", 1, &outer_f));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_cls1_agg, outer_wrap_st, &rv));

    /* NULL con_tp on ck_aggregate with struct expected_type and scalar expected_type (L2396 & L2446) */
    a_constant c_notype_agg, c_val_nt;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_val_nt, NULL, 33));
    std::memset(&c_notype_agg, 0, sizeof(c_notype_agg));
    c_notype_agg.kind = ck_aggregate;
    c_notype_agg.type = NULL;
    c_notype_agg.variant.aggregate.first_constant = &c_val_nt;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_notype_agg, g_expr_mock.one_field_struct_type, &rv));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_notype_agg, jit_int, &rv));

    /* Derived-without-own-fields wrapping multi-field base_0 (lines 2401–2434) + pointer field cast */
    a_field bf_ptr, bf_i2;
    std::memset(&bf_ptr, 0, sizeof(bf_ptr));
    bf_ptr.type = &vptr_tp;
    std::memset(&bf_i2, 0, sizeof(bf_i2));
    bf_i2.type = &int_tp;
    gcc_jit_field *j_ptr_f = gcc_jit_context_new_field(ctx, NULL, gcc_jit_type_get_pointer(jit_int), "p");
    gcc_jit_type *j_ptr_wrap_st = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cov_ptr_st", 1, &j_ptr_f));
    a_type ptr_base_tp, derived_no_fld_tp;
    std::memset(&ptr_base_tp, 0, sizeof(ptr_base_tp));
    ptr_base_tp.kind = tk_struct;
    ptr_base_tp.size = 8;
    ptr_base_tp.variant.class_struct_union.field_list = &bf_ptr;
    GCC_GEN_BE_CHECK(expr_test_map_type(&ptr_base_tp, j_ptr_wrap_st));

    a_base_class d_b0;
    std::memset(&d_b0, 0, sizeof(d_b0));
    d_b0.type = &ptr_base_tp;
    a_class_type_supplement d_sup;
    std::memset(&d_sup, 0, sizeof(d_sup));
    d_sup.direct_base_classes = &d_b0;
    std::memset(&derived_no_fld_tp, 0, sizeof(derived_no_fld_tp));
    derived_no_fld_tp.kind = tk_class;
    derived_no_fld_tp.size = 8;
    derived_no_fld_tp.variant.class_struct_union.extra_info = &d_sup;
    derived_no_fld_tp.variant.class_struct_union.field_list = NULL;
    gcc_jit_field *j_db0_f = gcc_jit_context_new_field(ctx, NULL, j_ptr_wrap_st, "base");
    gcc_jit_type *j_derived_st = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cov_deriv_st", 1, &j_db0_f));
    GCC_GEN_BE_CHECK(expr_test_map_type(&derived_no_fld_tp, j_derived_st));

    a_constant c_null_vp, c_deriv_agg;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_null_vp, &vptr_tp, 0));
    std::memset(&c_deriv_agg, 0, sizeof(c_deriv_agg));
    c_deriv_agg.kind = ck_aggregate;
    c_deriv_agg.type = &derived_no_fld_tp;
    c_deriv_agg.variant.aggregate.first_constant = &c_null_vp;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_deriv_agg, j_derived_st, &rv));

    /* Also test field type coercion branches in lower_struct_aggregate_constant_inner (lines 2333–2372):
       - 1-element inner aggregate initializing pointer field (L2337)
       - pointer-to-pointer mismatch (L2342)
       - scalar/pointer initializing 1-field sub-struct field (L2345–2365)
       - int initializing pointer field via safe_ptr_or_int_cast (L2366)
       - 1-element aggregate initializing scalar field (L2369) */
    a_field mf0, mf1, mf2, mf3;
    std::memset(&mf0, 0, sizeof(mf0));
    std::memset(&mf1, 0, sizeof(mf1));
    std::memset(&mf2, 0, sizeof(mf2));
    std::memset(&mf3, 0, sizeof(mf3));
    a_type iptr_tp;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&iptr_tp, &int_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_test_map_type(&iptr_tp, gcc_jit_type_get_pointer(jit_int)));
    mf0.type = &iptr_tp;      /* pointer field */
    mf1.type = &ptr_base_tp;  /* 1-field struct field */
    mf2.type = &vptr_tp;      /* pointer field initialized from int */
    mf3.type = &int_tp;       /* scalar field initialized from 1-elem aggregate */
    mf0.next = &mf1;
    mf1.next = &mf2;
    mf2.next = &mf3;

    gcc_jit_field *j_mf[4] = {
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_type_get_pointer(jit_int), "mf0"),
        gcc_jit_context_new_field(ctx, NULL, j_ptr_wrap_st, "mf1"),
        gcc_jit_context_new_field(ctx, NULL, jit_vptr, "mf2"),
        gcc_jit_context_new_field(ctx, NULL, jit_int, "mf3")
    };
    gcc_jit_type *j_multi_st = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cov_multi_st", 4, j_mf));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &mf0, j_mf[0]));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &mf1, j_mf[1]));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &mf2, j_mf[2]));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &mf3, j_mf[3]));

    a_type multi_st_tp;
    std::memset(&multi_st_tp, 0, sizeof(multi_st_tp));
    multi_st_tp.kind = tk_struct;
    multi_st_tp.size = 32;
    multi_st_tp.variant.class_struct_union.field_list = &mf0;
    GCC_GEN_BE_CHECK(expr_test_map_type(&multi_st_tp, j_multi_st));

    a_constant c_vp0_a, c_inner_ptr_agg, c_vp0_b, c_int_for_ptr, c_scalar_in_agg, c_inner_sc_agg, c_multi_agg;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_vp0_a, &vptr_tp, 0));
    std::memset(&c_inner_ptr_agg, 0, sizeof(c_inner_ptr_agg));
    c_inner_ptr_agg.kind = ck_aggregate;
    c_inner_ptr_agg.type = &vptr_tp;
    c_inner_ptr_agg.variant.aggregate.first_constant = &c_vp0_a;

    GCC_GEN_BE_CHECK(expr_init_int_const(&c_vp0_b, &vptr_tp, 0));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_int_for_ptr, &int_tp, 16));
    a_type long_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_scalar_in_agg, &long_tp, 99));
    std::memset(&c_inner_sc_agg, 0, sizeof(c_inner_sc_agg));
    c_inner_sc_agg.kind = ck_aggregate;
    c_inner_sc_agg.type = &long_tp;
    c_inner_sc_agg.variant.aggregate.first_constant = &c_scalar_in_agg;

    c_inner_ptr_agg.next = &c_vp0_b;
    c_vp0_b.next = &c_int_for_ptr;
    c_int_for_ptr.next = &c_inner_sc_agg;

    std::memset(&c_multi_agg, 0, sizeof(c_multi_agg));
    c_multi_agg.kind = ck_aggregate;
    c_multi_agg.type = &multi_st_tp;
    c_multi_agg.variant.aggregate.first_constant = &c_inner_ptr_agg;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_multi_agg, j_multi_st, &rv));

    /* Spanning bitfield constant + designated second bitfield in same storage unit + OOM sweep */
    a_field sbf1, sbf2;
    std::memset(&sbf1, 0, sizeof(sbf1));
    sbf1.is_bit_field = TRUE;
    sbf1.offset = 0;
    sbf1.offset_bit_remainder = 20;
    sbf1.bit_size = 20; /* 20 + 20 = 40 > 32 -> upgrades to UINT64_T when con_tp->size >= 8! */
    sbf1.type = &zero_sz_tp;
    std::memset(&sbf2, 0, sizeof(sbf2));
    sbf2.is_bit_field = TRUE;
    sbf2.offset = 0;
    sbf2.offset_bit_remainder = 40;
    sbf2.bit_size = 4;
    sbf2.type = &int_tp;
    sbf1.next = &sbf2;

    gcc_jit_field *j_sbf_word = gcc_jit_context_new_field(
        ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T), "_bf_w64");
    gcc_jit_type *j_sbf_st = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cov_sbf_st", 1, &j_sbf_word));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sbf1, j_sbf_word));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sbf2, j_sbf_word));

    a_type sbf_st_tp;
    std::memset(&sbf_st_tp, 0, sizeof(sbf_st_tp));
    sbf_st_tp.kind = tk_struct;
    sbf_st_tp.size = 8;
    sbf_st_tp.variant.class_struct_union.field_list = &sbf1;
    GCC_GEN_BE_CHECK(expr_test_map_type(&sbf_st_tp, j_sbf_st));

    a_constant c_bf_d1, c_bf_v1, c_bf_d2, c_bf_v2, c_bf_d_end, c_sbf_agg;
    std::memset(&c_bf_d1, 0, sizeof(c_bf_d1));
    c_bf_d1.kind = ck_designator;
    c_bf_d1.variant.designator.is_field_designator = TRUE;
    c_bf_d1.variant.designator.variant.field = &sbf1;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_bf_v1, &int_tp, 3));
    std::memset(&c_bf_d2, 0, sizeof(c_bf_d2));
    c_bf_d2.kind = ck_designator;
    c_bf_d2.variant.designator.is_field_designator = TRUE;
    c_bf_d2.variant.designator.variant.field = &sbf2;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_bf_v2, &int_tp, 5));
    std::memset(&c_bf_d_end, 0, sizeof(c_bf_d_end));
    c_bf_d_end.kind = ck_designator;

    c_bf_d1.next = &c_bf_v1;
    c_bf_v1.next = &c_bf_d2;
    c_bf_d2.next = &c_bf_v2;
    c_bf_v2.next = &c_bf_d_end;

    std::memset(&c_sbf_agg, 0, sizeof(c_sbf_agg));
    c_sbf_agg.kind = ck_aggregate;
    c_sbf_agg.type = &sbf_st_tp;
    c_sbf_agg.variant.aggregate.first_constant = &c_bf_d1;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_constant_rvalue(&c_sbf_agg, j_sbf_st, &rv);
    }));

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Covers the remaining fine-grained compound-condition branches across Part A of `gcc_gen_be_expr.c`.
 * @details Exercises remaining short-circuit sides in `emit_optional_eval`, `reconcile_call_arg_rvalue`,
 * `finish_direct_or_indirect_call` (void return), `resolve_lvalue_c11_atomic_type` (via `eok_pre_incr`),
 * `gcc_gen_be_lower_dynamic_init`, `scale_vla_subarray_index`, `lower_address_constant`,
 * `lower_struct_aggregate_constant_inner`, `lower_aggregate_constant`, and `gcc_gen_be_lower_constant_rvalue`.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_part_a_tail(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, uint_tp, long_tp, void_tp, vptr_tp, iptr_tp, st_tp, cls_tp, u_tp, arr_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&uint_tp, ik_unsigned_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&iptr_tp, &int_tp, FALSE));

    a_field st_f0;
    std::memset(&st_f0, 0, sizeof(st_f0));
    st_f0.type = &vptr_tp;
    std::memset(&st_tp, 0, sizeof(st_tp));
    st_tp.kind = tk_struct;
    st_tp.size = 8;
    st_tp.variant.class_struct_union.field_list = &st_f0;

    std::memset(&cls_tp, 0, sizeof(cls_tp));
    cls_tp.kind = tk_class;
    cls_tp.size = 8;
    cls_tp.variant.class_struct_union.field_list = &st_f0;

    std::memset(&u_tp, 0, sizeof(u_tp));
    u_tp.kind = tk_union;
    u_tp.size = 4;

    std::memset(&arr_tp, 0, sizeof(arr_tp));
    arr_tp.kind = tk_array;
    arr_tp.size = 8;
    arr_tp.variant.array.element_type = &int_tp;
    arr_tp.variant.array.variant.number_of_elements = 2;

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_vptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *jit_iptr = gcc_jit_type_get_pointer(jit_int);
    gcc_jit_field *j_pf = gcc_jit_context_new_field(ctx, NULL, jit_iptr, "pf");
    gcc_jit_type *jit_ptr_st = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cov_tail_pst", 1, &j_pf));
    GCC_GEN_BE_CHECK(expr_test_map_type(&st_tp, jit_ptr_st));
    GCC_GEN_BE_CHECK(expr_test_map_type(&cls_tp, jit_ptr_st));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &st_f0, j_pf));

    gcc_jit_rvalue *rv = NULL;
    a_constant c1;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c1, &int_tp, 3));
    an_expr_node e_c1;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_c1, &c1));

    /* L80 (80:15): emit_optional_eval with rv != NULL and cblock == NULL via (void)3 */
    an_expr_node e_void_cast;
    GCC_GEN_BE_CHECK(expr_init_op(&e_void_cast, eok_cast, &void_tp, &e_c1));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_void_cast, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L498 (498:24): check_is_complex_type(NULL) via eok_cast with op1->type == NULL */
    an_expr_node e_nt, e_cast_nt;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_nt, &c1));
    e_nt.type = NULL;
    GCC_GEN_BE_CHECK(expr_init_op(&e_cast_nt, eok_cast, &int_tp, &e_nt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast_nt, &rv));

    /* L316 (316:17): reconcile_call_arg_rvalue with coerce_return_null == TRUE */
    a_type vfn_tp;
    std::memset(&vfn_tp, 0, sizeof(vfn_tp));
    vfn_tp.kind = tk_routine;
    vfn_tp.variant.routine.return_type = &void_tp;
    gcc_jit_param *p_st = gcc_jit_context_new_param(ctx, NULL, g_expr_mock.one_field_struct_type, "ps");
    gcc_jit_function *fn_st = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_tail_fn_st", 1, &p_st, 0);
    a_routine r_st;
    std::memset(&r_st, 0, sizeof(r_st));
    r_st.type = &vfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_st, fn_st));
    an_expr_node e_rst, e_call_st;
    std::memset(&e_rst, 0, sizeof(e_rst));
    e_rst.kind = enk_routine;
    e_rst.type = &vfn_tp;
    e_rst.variant.routine.ptr = &r_st;
    e_rst.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_call_st, eok_call, &void_tp, &e_rst));
    g_expr_mock.coerce_rval_return_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_st, &rv));
    g_expr_mock.coerce_rval_return_null = FALSE;

    /* L373–382: finish_direct_or_indirect_call with ret_type == VOID:
       - direct void call with active_lp != NULL & invoke_cblock != NULL (L375 True)
       - direct void call with active_lp != NULL & invoke_cblock == NULL (L373:26 False)
       - indirect void call through fn_ptr with active_lp != NULL & invoke_cblock != NULL (L375 False)
       - direct void call with active_lp == NULL & invoke_cblock == NULL (L382 False) */
    gcc_jit_block *lp = gcc_jit_function_new_block(fn, "tail_lp");
    gcc_jit_block_end_with_void_return(lp, NULL);
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(lp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_st, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call_st, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    a_type fn_ptr_tp;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&fn_ptr_tp, &vfn_tp, FALSE));
    gcc_jit_type *j_vfn_ptr = gcc_jit_context_new_function_ptr_type(ctx, NULL, jit_void, 0, NULL, 0);
    GCC_GEN_BE_CHECK(expr_test_map_type(&fn_ptr_tp, j_vfn_ptr));
    a_variable vfn_ptr_var;
    std::memset(&vfn_ptr_var, 0, sizeof(vfn_ptr_var));
    vfn_ptr_var.type = &fn_ptr_tp;
    gcc_jit_lvalue *lv_vfnp = gcc_jit_function_new_local(fn, NULL, j_vfn_ptr, "cov_vfnp");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vfn_ptr_var, lv_vfnp));
    an_expr_node e_vfnp, e_ind_vcall;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vfnp, &vfn_ptr_var));
    GCC_GEN_BE_CHECK(expr_init_op(&e_ind_vcall, eok_call, &void_tp, &e_vfnp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_ind_vcall, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_ind_vcall, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    e_rst.next = NULL;

    /* L462, L464, L471, L474: resolve_lvalue_c11_atomic_type via eok_pre_incr (runs even when l1 is NULL/PDM!) */
    an_expr_node e_op_lval, e_pre_inc, e_fld_null, e_ptr_base;
    std::memset(&e_fld_null, 0, sizeof(e_fld_null));
    e_fld_null.kind = enk_field;
    e_fld_null.type = &int_tp;
    e_fld_null.variant.field.ptr = NULL;

    a_variable st_var, ptr_var;
    std::memset(&st_var, 0, sizeof(st_var));
    st_var.type = &st_tp;
    gcc_jit_lvalue *lv_st = gcc_jit_function_new_local(fn, NULL, jit_ptr_st, "cov_tail_st");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &st_var, lv_st));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_ptr_base, &st_var));

    /* a1 == NULL on eok_dot_field */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op_lval, eok_dot_field, &int_tp, NULL));
    GCC_GEN_BE_CHECK(expr_init_op(&e_pre_inc, eok_pre_incr, &int_tp, &e_op_lval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_pre_inc, &rv));
    /* a1 != NULL, a2 == NULL on eok_dot_field */
    e_ptr_base.next = NULL;
    e_op_lval.variant.operation.operands = &e_ptr_base;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_pre_inc, &rv));
    /* a2 == enk_field with field.ptr == NULL (returns UNHANDLED_EXPR AFTER hitting L464:44 False!) */
    e_ptr_base.next = &e_fld_null;
    if (gcc_gen_be_lower_expr_rvalue(&e_pre_inc, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_ptr_base.next = NULL;

    /* eok_indirect with a1->type == NULL, a1->type non-pointer, and pointer with NULL target */
    a_type ptr_null_tgt;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_null_tgt, NULL, FALSE));
    std::memset(&ptr_var, 0, sizeof(ptr_var));
    ptr_var.type = &iptr_tp;
    gcc_jit_lvalue *lv_ip = gcc_jit_function_new_local(fn, NULL, jit_iptr, "cov_tail_ip");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &ptr_var, lv_ip));
    an_expr_node e_ip;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_ip, &ptr_var));
    e_op_lval.variant.operation.kind = eok_indirect;
    e_op_lval.variant.operation.operands = &e_ip;
    e_ip.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_pre_inc, &rv));
    e_ip.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_pre_inc, &rv));
    e_ip.type = &ptr_null_tgt;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_pre_inc, &rv));

    /* L592, L694, L700, L746, L760, L793, L810, L843–845, L911, L922 in dynamic_init */
    gcc_jit_lvalue *lv_int = gcc_jit_function_new_local(fn, NULL, jit_int, "cov_tail_i");
    gcc_jit_lvalue *lv_arr = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_context_new_array_type(ctx, NULL, jit_int, 2), "cov_tail_a");
    a_variable int_var;
    std::memset(&int_var, 0, sizeof(int_var));
    int_var.type = &int_tp;

    /* L592: dik_zero with target_lval != NULL and force_zero_null == TRUE (init_rval == NULL) */
    a_dynamic_init dip;
    std::memset(&dip, 0, sizeof(dip));
    dip.kind = dik_zero;
    dip.variable = &int_var;
    g_expr_mock.force_zero_null = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_int, &rv));
    g_expr_mock.force_zero_null = FALSE;

    /* L694 & L700: ck_init_repeat with ck_void (elem_rval == NULL) and enk_throw (cblock becomes NULL) */
    a_constant c_void_elem, c_dyn_throw, c_rep;
    std::memset(&c_void_elem, 0, sizeof(c_void_elem));
    c_void_elem.kind = ck_void;
    c_void_elem.type = &int_tp;
    std::memset(&c_rep, 0, sizeof(c_rep));
    c_rep.kind = ck_init_repeat;
    c_rep.type = &arr_tp;
    c_rep.variant.init_repeat.count = 2;
    c_rep.variant.init_repeat.constant = &c_void_elem;
    dip.kind = dik_constant;
    dip.variant.constant.ptr = &c_rep;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));

    an_expr_node e_throw;
    std::memset(&e_throw, 0, sizeof(e_throw));
    e_throw.kind = enk_throw;
    e_throw.type = &int_tp;
    a_dynamic_init throw_dip;
    std::memset(&throw_dip, 0, sizeof(throw_dip));
    throw_dip.kind = dik_expression;
    throw_dip.variable = &int_var;
    throw_dip.variant.expression = &e_throw;
    std::memset(&c_dyn_throw, 0, sizeof(c_dyn_throw));
    c_dyn_throw.kind = ck_dynamic_init;
    c_dyn_throw.type = &int_tp;
    c_dyn_throw.variant.dynamic_init.ptr = &throw_dip;
    c_rep.variant.init_repeat.constant = &c_dyn_throw;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    /* Also elem_rval != NULL with cblock cleared during expression (comma(throw, 3)) -> L694:46 False! */
    an_expr_node e_throw_comma;
    e_throw.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_throw_comma, eok_comma, &int_tp, &e_throw));
    throw_dip.variant.expression = &e_throw_comma;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_arr, &rv));
    e_throw.next = NULL;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L746 (is_field_designator && is_generic) and L760 (ck_dynamic_init with ptr == NULL when jit_f != NULL) */
    a_constant c_fg_desig, c_dyn_null_ptr, c_st_dyn_agg;
    std::memset(&c_fg_desig, 0, sizeof(c_fg_desig));
    c_fg_desig.kind = ck_designator;
    c_fg_desig.variant.designator.is_field_designator = TRUE;
    c_fg_desig.variant.designator.is_generic = TRUE;
    std::memset(&c_dyn_null_ptr, 0, sizeof(c_dyn_null_ptr));
    c_dyn_null_ptr.kind = ck_dynamic_init;
    c_dyn_null_ptr.type = &vptr_tp;
    c_dyn_null_ptr.variant.dynamic_init.ptr = NULL;
    c_fg_desig.next = &c_dyn_null_ptr;
    std::memset(&c_st_dyn_agg, 0, sizeof(c_st_dyn_agg));
    c_st_dyn_agg.kind = ck_aggregate;
    c_st_dyn_agg.type = &st_tp;
    c_st_dyn_agg.variant.aggregate.has_dynamic_init_component = TRUE;
    c_st_dyn_agg.variant.aggregate.first_constant = &c_fg_desig;
    a_variable st_v2;
    std::memset(&st_v2, 0, sizeof(st_v2));
    st_v2.type = &st_tp;
    dip.kind = dik_nonconstant_aggregate;
    dip.variable = &st_v2;
    dip.variant.constant.ptr = &c_st_dyn_agg;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));

    /* L793 (793:25 False): dik_expression assigning DIFFERENT struct init_rval to 1-field struct target_lval */
    gcc_jit_lvalue *lv_one = gcc_jit_function_new_local(fn, NULL, g_expr_mock.one_field_struct_type, "cov_one");
    an_expr_node e_st_src;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_st_src, &st_var));
    dip.kind = dik_expression;
    dip.variant.expression = &e_st_src;
    /* Note: incompatible struct assignment logs a libgccjit error on ctx; run and ignore libgccjit diag */
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_one, &rv));

    /* L810 (810:85 False) & L843–845: dik_constructor value_initialization & VTT condition branches */
    std::memset(&dip, 0, sizeof(dip));
    dip.kind = dik_constructor;
    dip.variable = &st_v2;
    dip.variant.constructor.value_initialization = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    dip.variable = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));

    gcc_jit_param *cp3[3] = {
        gcc_jit_context_new_param(ctx, NULL, jit_vptr, "this"),
        gcc_jit_context_new_param(ctx, NULL, jit_vptr, "vtt"),
        gcc_jit_context_new_param(ctx, NULL, jit_int, "extra")
    };
    gcc_jit_function *j_ctor3 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_tail_ctor3", 3, cp3, 0);
    a_routine ctor3;
    std::memset(&ctor3, 0, sizeof(ctor3));
    ctor3.type = &vfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &ctor3, j_ctor3));
    dip.variant.constructor.value_initialization = FALSE;
    dip.variant.constructor.ptr = &ctor3;
    /* 1) edg_type == NULL (dip.variable == NULL) -> L843:54 False */
    dip.variable = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    /* 2) edg_type == tk_union -> L844:64 False */
    a_variable u_var;
    std::memset(&u_var, 0, sizeof(u_var));
    u_var.type = &u_tp;
    dip.variable = &u_var;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    /* 3) edg_type == tk_struct with any_virtual_base_classes == FALSE -> L845:33 False, plus tk_class with TRUE */
    dip.variable = &st_v2;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));
    cls_tp.variant.class_struct_union.any_virtual_base_classes = TRUE;
    a_variable cls_var;
    std::memset(&cls_var, 0, sizeof(cls_var));
    cls_var.type = &cls_tp;
    dip.variable = &cls_var;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));

    /* L911 (default: break;) and L922 (reg_err != SUCCESS && reg_err != INTERNAL when dip->variable == NULL!) */
    std::memset(&dip, 0, sizeof(dip));
    dip.kind = (a_dynamic_init_kind)14;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip, lv_st, &rv));

    gcc_jit_param *dp1 = gcc_jit_context_new_param(ctx, NULL, jit_vptr, "this");
    gcc_jit_function *j_dtor1 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_tail_dtor1", 1, &dp1, 0);
    a_routine dtor1;
    std::memset(&dtor1, 0, sizeof(dtor1));
    dtor1.type = &vfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &dtor1, j_dtor1));
    dip.kind = dik_none;
    dip.variable = NULL; /* critical so !dip->variable at L917 is TRUE! */
    dip.destructor = &dtor1;
    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_dynamic_init(&dip, lv_st, &rv);
    }));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));

    /* L975: scale_vla_subarray_index where inner VLA has element_type == NULL, and fixed-size subarray element */
    a_type vla_sub_null, vla_sub_fixed, ptr_vla_null, ptr_vla_fixed;
    std::memset(&vla_sub_null, 0, sizeof(vla_sub_null));
    vla_sub_null.kind = tk_array;
    vla_sub_null.variant.array.element_type = NULL;
    vla_sub_null.variant.array.is_variable_size_array = TRUE;
    vla_sub_null.variant.array.variant.number_of_elements = 2;
    std::memset(&vla_sub_fixed, 0, sizeof(vla_sub_fixed));
    vla_sub_fixed.kind = tk_array;
    vla_sub_fixed.variant.array.element_type = &arr_tp; /* tk_array with is_variable_size_array == FALSE */
    vla_sub_fixed.variant.array.is_variable_size_array = TRUE;
    vla_sub_fixed.variant.array.variant.number_of_elements = 2;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_vla_null, &vla_sub_null, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_vla_fixed, &vla_sub_fixed, FALSE));

    a_variable v_vn, v_vf;
    std::memset(&v_vn, 0, sizeof(v_vn));
    v_vn.type = &ptr_vla_null;
    std::memset(&v_vf, 0, sizeof(v_vf));
    v_vf.type = &ptr_vla_fixed;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vn, lv_ip));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vf, lv_ip));
    an_expr_node e_vn, e_vf, e_sub_v;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vn, &v_vn));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vf, &v_vf));
    e_vn.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_sub_v, eok_subscript, &int_tp, &e_vn));
    gcc_jit_lvalue *sub_lv = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub_v, &sub_lv));
    e_vn.next = NULL;
    e_vf.next = &e_c1;
    e_sub_v.variant.operation.operands = &e_vf;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub_v, &sub_lv));
    e_vf.next = NULL;

    /* L1739 (eok_bassign on bitfield) & L1864 (safe_ptr_or_int_cast struct -> int) */
    a_field bf_f;
    std::memset(&bf_f, 0, sizeof(bf_f));
    bf_f.is_bit_field = TRUE;
    bf_f.bit_size = 4;
    bf_f.type = &uint_tp;
    an_expr_node e_bf_fld, e_bf_dot, e_bf_bassign;
    std::memset(&e_bf_fld, 0, sizeof(e_bf_fld));
    e_bf_fld.kind = enk_field;
    e_bf_fld.type = &uint_tp;
    e_bf_fld.variant.field.ptr = &bf_f;
    e_ptr_base.next = &e_bf_fld;
    GCC_GEN_BE_CHECK(expr_init_op(&e_bf_dot, eok_dot_field, &uint_tp, &e_ptr_base));
    e_bf_dot.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_bf_bassign, eok_bassign, &uint_tp, &e_bf_dot));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_bf_bassign, &rv));
    e_bf_dot.next = NULL;
    e_ptr_base.next = NULL;

    an_expr_node e_st_to_int;
    GCC_GEN_BE_CHECK(expr_init_op(&e_st_to_int, eok_cast, &int_tp, &e_st_src));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_st_to_int, &rv));

    /* lower_address_constant remaining short-circuit branches (L1942..L2094) */
    a_constant c_addr;
    std::memset(&c_addr, 0, sizeof(c_addr));
    c_addr.kind = ck_address;
    /* L1957 True: abk_label with CACHED innermost_function_scope->variant.routine.ptr */
    a_scope fn_sc;
    std::memset(&fn_sc, 0, sizeof(fn_sc));
    fn_sc.variant.routine.ptr = &r_st;
    innermost_function_scope = &fn_sc;
    c_addr.variant.address.kind = abk_label;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, jit_vptr, &rv));
    innermost_function_scope = NULL;

    /* L1980 False: ck_string under abk_constant with non-pointer expected_type (jit_int) */
    a_constant c_str;
    std::memset(&c_str, 0, sizeof(c_str));
    c_str.kind = ck_string;
    c_str.type = &arr_tp;
    c_str.variant.string.value = "x";
    c_str.variant.string.length = 2;
    c_addr.variant.address.kind = abk_constant;
    c_addr.variant.address.variant.constant = &c_str;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr, jit_int, &rv));

    /* L2005:61 False: ck_string with length > 0 and value == NULL */
    a_constant c_str_null_val, c_addr_snv;
    std::memset(&c_str_null_val, 0, sizeof(c_str_null_val));
    c_str_null_val.kind = ck_string;
    c_str_null_val.variant.string.value = NULL;
    c_str_null_val.variant.string.length = 2;
    c_addr_snv = c_addr;
    c_addr_snv.variant.address.offset = 1;
    c_addr_snv.variant.address.variant.constant = &c_str_null_val;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr_snv, jit_iptr, &rv));

    /* L2023 & L2028 & L2042: abk_temporary with cblock == NULL, inner_val == NULL, inner_type == NULL, offset != 0 */
    a_constant c_tmp_var;
    std::memset(&c_tmp_var, 0, sizeof(c_tmp_var));
    c_tmp_var.kind = ck_address;
    c_tmp_var.variant.address.kind = abk_temporary;
    c_tmp_var.variant.address.variant.constant = &c1;
    /* 1) abk_temporary with offset != 0 (L2023:96 False -> goes to L2028) */
    c_tmp_var.variant.address.offset = 4;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_tmp_var, jit_vptr, &rv));
    /* 2) abk_temporary with cblock == NULL (L2023:59 False) */
    c_tmp_var.variant.address.offset = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_tmp_var, jit_vptr, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    /* 3) inner_val != NULL && inner_type == NULL (expected_type == NULL, c_zero_nt has type == NULL) -> hits L2042! */
    a_constant c_z_nt;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_z_nt, NULL, 0));
    c_tmp_var.variant.address.variant.constant = &c_z_nt;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_tmp_var, NULL, &rv));

    /* L2052..L2094: non-zero offset on abk_routine (L2052 False), abk_variable with NULL vtp,
       tk_array with NULL element_type, struct lval_type on array var, total_sz>0 && num_elems==0,
       total_sz < num_elems ((total_sz/num_elems)==0), tk_struct var with non-array field,
       tk_class var with array field of NULL element_type, array field with 0-size element, and tk_integer var! */
    a_constant c_off;
    std::memset(&c_off, 0, sizeof(c_off));
    c_off.kind = ck_address;
    c_off.variant.address.offset = 4;
    c_off.variant.address.kind = abk_routine;
    c_off.variant.address.variant.routine = &r_st;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));

    c_off.variant.address.kind = abk_variable;
    /* Variable not in cache when lowered via abk_routine? No: insert variable, then clear cache? Wait:
       if global_var returns a global WITHOUT inserting into cache? Our mock inserts into cache, EXCEPT if we
       leave var->type == NULL (vtp == NULL)! */
    a_variable v_null_tp, v_arr_null_el, v_arr_st_lv, v_arr_num0, v_arr_tiny, v_int_off;
    std::memset(&v_null_tp, 0, sizeof(v_null_tp));
    v_null_tp.type = NULL;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_null_tp, lv_int));
    c_off.variant.address.variant.variable = &v_null_tp;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));

    a_type arr_null_el, arr_num0, arr_tiny, zero_int_tp;
    std::memset(&arr_null_el, 0, sizeof(arr_null_el));
    arr_null_el.kind = tk_array;
    arr_null_el.variant.array.element_type = NULL;
    std::memset(&arr_num0, 0, sizeof(arr_num0));
    arr_num0.kind = tk_array;
    arr_num0.size = 8;
    arr_num0.variant.array.element_type = &int_tp;
    arr_num0.variant.array.variant.number_of_elements = 0;
    std::memset(&arr_tiny, 0, sizeof(arr_tiny));
    arr_tiny.kind = tk_array;
    arr_tiny.size = 1;
    arr_tiny.variant.array.element_type = &int_tp;
    arr_tiny.variant.array.variant.number_of_elements = 4; /* 1 / 4 == 0! */
    GCC_GEN_BE_CHECK(expr_init_int_type(&zero_int_tp, ik_int, 0));
    a_type arr_zel;
    std::memset(&arr_zel, 0, sizeof(arr_zel));
    arr_zel.kind = tk_array;
    arr_zel.size = 0;
    arr_zel.variant.array.element_type = &zero_int_tp;
    arr_zel.variant.array.variant.number_of_elements = 2;

    std::memset(&v_arr_null_el, 0, sizeof(v_arr_null_el));
    v_arr_null_el.type = &arr_null_el;
    std::memset(&v_arr_st_lv, 0, sizeof(v_arr_st_lv));
    v_arr_st_lv.type = &arr_tp;
    std::memset(&v_arr_num0, 0, sizeof(v_arr_num0));
    v_arr_num0.type = &arr_num0;
    std::memset(&v_arr_tiny, 0, sizeof(v_arr_tiny));
    v_arr_tiny.type = &arr_tiny;
    std::memset(&v_int_off, 0, sizeof(v_int_off));
    v_int_off.type = &int_tp;

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_arr_null_el, lv_arr));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_arr_st_lv, lv_st)); /* struct lval_type! */
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_arr_num0, lv_arr));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_arr_tiny, lv_arr));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_int_off, lv_int));

    c_off.variant.address.variant.variable = &v_arr_null_el;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));
    c_off.variant.address.variant.variable = &v_arr_st_lv;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));
    c_off.variant.address.variant.variable = &v_arr_num0;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));
    c_off.variant.address.variant.variable = &v_arr_tiny;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));
    c_off.variant.address.variant.variable = &v_int_off;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));

    /* Struct variable with: NULL ftp, array field with NULL element_type, array field with 0-byte elem */
    a_field sf_null_tp, sf_arr_nel, sf_arr_zel;
    std::memset(&sf_null_tp, 0, sizeof(sf_null_tp));
    sf_null_tp.offset = 0;
    sf_null_tp.type = NULL;
    std::memset(&sf_arr_nel, 0, sizeof(sf_arr_nel));
    sf_arr_nel.offset = 4;
    sf_arr_nel.type = &arr_null_el;
    std::memset(&sf_arr_zel, 0, sizeof(sf_arr_zel));
    sf_arr_zel.offset = 8;
    sf_arr_zel.type = &arr_zel;
    sf_null_tp.next = &sf_arr_nel;
    sf_arr_nel.next = &sf_arr_zel;

    gcc_jit_field *j_sf[3] = {
        gcc_jit_context_new_field(ctx, NULL, jit_int, "sf0"),
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_new_array_type(ctx, NULL, jit_int, 2), "sf1"),
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_new_array_type(ctx, NULL, jit_int, 2), "sf2")
    };
    gcc_jit_type *j_sf_st = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cov_sf_st", 3, j_sf));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sf_null_tp, j_sf[0]));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sf_arr_nel, j_sf[1]));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sf_arr_zel, j_sf[2]));
    a_type sf_st_tp;
    std::memset(&sf_st_tp, 0, sizeof(sf_st_tp));
    sf_st_tp.kind = tk_struct;
    sf_st_tp.size = 24;
    sf_st_tp.variant.class_struct_union.field_list = &sf_null_tp;
    a_variable v_sf_st;
    std::memset(&v_sf_st, 0, sizeof(v_sf_st));
    v_sf_st.type = &sf_st_tp;
    gcc_jit_lvalue *lv_sf = gcc_jit_function_new_local(fn, NULL, j_sf_st, "cov_sf");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_sf_st, lv_sf));
    c_off.variant.address.variant.variable = &v_sf_st;
    c_off.variant.address.offset = 2; /* hits sf_null_tp (ftp == NULL) */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));
    c_off.variant.address.offset = 5; /* hits sf_arr_nel (ftp->variant.array.element_type == NULL) */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));
    c_off.variant.address.offset = 9; /* hits sf_arr_zel (elem_sz == 0) */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_off, jit_vptr, &rv));

    /* L2173..L2446: lower_struct_aggregate_constant_inner remaining branches:
       - pointer-to-pointer mismatch in struct field (L2342–2344)
       - pointer constant initializing 1-field base struct & 1-field sub-struct field (L2349–2364)
       - bitfield with is_bit_field == FALSE && bit_size > 0, non-integer bitfield val, unmapped jit_f on bitfield
       - union with NULL field / NULL val (L2389 False), union with struct_type == NULL (L2390/2393 False)
       - struct with struct_type == NULL & type != NULL (L2397 True), struct_type == NULL & type == NULL (L2398 False)
       - derived wrapping tk_class b0 with different pointer type (L2417:69 & L2424) */
    a_constant c_null_vptr, c_ptr_st_agg;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_null_vptr, &vptr_tp, 0)); /* void* constant into int* field -> L2342–2344! */
    std::memset(&c_ptr_st_agg, 0, sizeof(c_ptr_st_agg));
    c_ptr_st_agg.kind = ck_aggregate;
    c_ptr_st_agg.type = &st_tp;
    c_ptr_st_agg.variant.aggregate.first_constant = &c_null_vptr;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_ptr_st_agg, jit_ptr_st, &rv));

    /* Pointer constant initializing 1-field base (b_cur) and 1-field sub-struct field (f) -> L2349–2364! */
    a_base_class b_ptr_sub;
    std::memset(&b_ptr_sub, 0, sizeof(b_ptr_sub));
    b_ptr_sub.type = &cls_tp; /* tk_class base with 1 field of vptr_tp mapped to jit_iptr! */
    a_field f_ptr_sub;
    std::memset(&f_ptr_sub, 0, sizeof(f_ptr_sub));
    f_ptr_sub.type = &st_tp; /* tk_struct field with 1 field of vptr_tp mapped to jit_iptr! */
    a_class_type_supplement sup_ps;
    std::memset(&sup_ps, 0, sizeof(sup_ps));
    sup_ps.direct_base_classes = &b_ptr_sub;

    gcc_jit_field *j_ps_f[2] = {
        gcc_jit_context_new_field(ctx, NULL, jit_ptr_st, "base_ps"),
        gcc_jit_context_new_field(ctx, NULL, jit_ptr_st, "fld_ps")
    };
    gcc_jit_type *j_ps_outer = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cov_ps_outer", 2, j_ps_f));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &b_ptr_sub, j_ps_f[0]));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &f_ptr_sub, j_ps_f[1]));
    a_type ps_outer_tp;
    std::memset(&ps_outer_tp, 0, sizeof(ps_outer_tp));
    ps_outer_tp.kind = tk_class;
    ps_outer_tp.size = 16;
    ps_outer_tp.variant.class_struct_union.extra_info = &sup_ps;
    ps_outer_tp.variant.class_struct_union.field_list = &f_ptr_sub;
    GCC_GEN_BE_CHECK(expr_test_map_type(&ps_outer_tp, j_ps_outer));

    a_constant c_vp_a, c_vp_b, c_ps_agg;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_vp_a, &vptr_tp, 0));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_vp_b, &vptr_tp, 0));
    c_vp_a.next = &c_vp_b;
    std::memset(&c_ps_agg, 0, sizeof(c_ps_agg));
    c_ps_agg.kind = ck_aggregate;
    c_ps_agg.type = &ps_outer_tp;
    c_ps_agg.variant.aggregate.first_constant = &c_vp_a;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_ps_agg, j_ps_outer, &rv));
    /* Note: in c_ps_agg, st_f0.type was &vptr_tp (jit_vptr == vals[idx] type); now set st_f0.type = &iptr_tp so sub_f0_type != vals[idx] type (L2361)! */
    GCC_GEN_BE_CHECK(expr_test_map_type(&iptr_tp, jit_iptr));
    st_f0.type = &iptr_tp;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_ps_agg, j_ps_outer, &rv));
    c_vp_a.next = NULL;

    /* Derived-without-own-fields wrapping tk_class b0 where bf0_type != vals[0] type (L2417:69 & L2424)! */
    a_base_class d_cls_b0;
    std::memset(&d_cls_b0, 0, sizeof(d_cls_b0));
    d_cls_b0.type = &cls_tp; /* tk_class! */
    a_class_type_supplement d_cls_sup;
    std::memset(&d_cls_sup, 0, sizeof(d_cls_sup));
    d_cls_sup.direct_base_classes = &d_cls_b0;
    a_type deriv_cls_tp;
    std::memset(&deriv_cls_tp, 0, sizeof(deriv_cls_tp));
    deriv_cls_tp.kind = tk_class;
    deriv_cls_tp.size = 8;
    deriv_cls_tp.variant.class_struct_union.extra_info = &d_cls_sup;
    gcc_jit_field *j_dcb = gcc_jit_context_new_field(ctx, NULL, jit_ptr_st, "dcb");
    gcc_jit_type *j_deriv_cls = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cov_deriv_cls", 1, &j_dcb));
    GCC_GEN_BE_CHECK(expr_test_map_type(&deriv_cls_tp, j_deriv_cls));
    a_constant c_deriv_cls_agg;
    std::memset(&c_deriv_cls_agg, 0, sizeof(c_deriv_cls_agg));
    c_deriv_cls_agg.kind = ck_aggregate;
    c_deriv_cls_agg.type = &deriv_cls_tp;
    c_deriv_cls_agg.variant.aggregate.first_constant = &c_vp_a;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_deriv_cls_agg, j_deriv_cls, &rv));

    /* Bitfield with is_bit_field == FALSE && bit_size > 0 (L2250:42 & L2303:60),
       non-ck_integer val on bitfield (L2250:62 False), unmapped jit_f on bitfield (L2252 False),
       and multi-bitfield realloc OOM sweep (L2273, L2278, L2305)! */
    a_field bf_a, bf_b, bf_other;
    std::memset(&bf_a, 0, sizeof(bf_a));
    bf_a.is_bit_field = FALSE;
    bf_a.bit_size = 4;
    bf_a.type = &uint_tp;
    std::memset(&bf_b, 0, sizeof(bf_b));
    bf_b.is_bit_field = FALSE;
    bf_b.bit_size = 4;
    bf_b.offset_bit_remainder = 4;
    bf_b.type = &uint_tp;
    std::memset(&bf_other, 0, sizeof(bf_other));
    bf_other.is_bit_field = TRUE;
    bf_other.bit_size = 4;
    bf_other.offset = 4;
    bf_other.type = &uint_tp;
    bf_a.next = &bf_b;
    bf_b.next = &bf_other;

    gcc_jit_field *j_bfw[2] = {
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T), "w0"),
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T), "w1")
    };
    gcc_jit_type *j_bf2_st = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cov_bf2_st", 2, j_bfw));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf_a, j_bfw[0]));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf_b, j_bfw[0]));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf_other, j_bfw[1]));

    a_type bf2_st_tp;
    std::memset(&bf2_st_tp, 0, sizeof(bf2_st_tp));
    bf2_st_tp.kind = tk_struct;
    bf2_st_tp.size = 8;
    bf2_st_tp.variant.class_struct_union.field_list = &bf_a;
    GCC_GEN_BE_CHECK(expr_test_map_type(&bf2_st_tp, j_bf2_st));

    a_constant c_bfa, c_bfb, c_bfo, c_bf2_agg;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_bfa, &uint_tp, 1));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_bfb, &uint_tp, 2));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_bfo, &uint_tp, 3));
    c_bfa.next = &c_bfb;
    c_bfb.next = &c_bfo;
    std::memset(&c_bf2_agg, 0, sizeof(c_bf2_agg));
    c_bf2_agg.kind = ck_aggregate;
    c_bf2_agg.type = &bf2_st_tp;
    c_bf2_agg.variant.aggregate.first_constant = &c_bfa;
    /* OOM sweep hits bf_shifts alloc, bf_widths alloc, and multi-bitfield reallocs + next_cached != jit_f! */
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_constant_rvalue(&c_bf2_agg, j_bf2_st, &rv);
    }));
    c_bfa.next = NULL;
    c_bfb.next = NULL;

    /* Non-integer constant on bitfield (L2250:62 False) and unmapped jit_f on bitfield (L2252 False) */
    a_field bf_unmapped;
    std::memset(&bf_unmapped, 0, sizeof(bf_unmapped));
    bf_unmapped.is_bit_field = TRUE;
    bf_unmapped.bit_size = 3;
    bf_unmapped.type = &uint_tp;
    a_type bf_unmap_st_tp;
    std::memset(&bf_unmap_st_tp, 0, sizeof(bf_unmap_st_tp));
    bf_unmap_st_tp.kind = tk_struct;
    bf_unmap_st_tp.size = 4;
    bf_unmap_st_tp.variant.class_struct_union.field_list = &bf_unmapped;
    GCC_GEN_BE_CHECK(expr_test_map_type(&bf_unmap_st_tp, NULL)); /* struct_type == NULL -> jit_f == NULL! */
    c_bf2_agg.type = &bf_unmap_st_tp;
    c_bf2_agg.variant.aggregate.first_constant = &c_bfa;
    /* Passing j_bf2_st as expected_type also hits L2397 (!struct_type -> struct_type = type)! */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_bf2_agg, j_bf2_st, &rv));
    /* Passing NULL as expected_type hits L2398 False (struct_type == NULL)! */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_bf2_agg, NULL, &rv));

    c_bf2_agg.type = &bf2_st_tp;
    c_bf2_agg.variant.aggregate.first_constant = &c_void_elem; /* val_c->kind != ck_integer! */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_bf2_agg, j_bf2_st, &rv));

    /* Union with struct_type == NULL (mapped to NULL, passing jit_u as expected_type -> L2390/2393 False & L2389 False!) */
    gcc_jit_field *uf0 = gcc_jit_context_new_field(ctx, NULL, jit_int, "uf0");
    gcc_jit_type *jit_u = gcc_jit_context_new_union_type(ctx, NULL, "_cov_tail_u", 1, &uf0);
    GCC_GEN_BE_CHECK(expr_test_map_type(&u_tp, NULL));
    a_constant c_u_agg;
    std::memset(&c_u_agg, 0, sizeof(c_u_agg));
    c_u_agg.kind = ck_aggregate;
    c_u_agg.type = &u_tp;
    c_u_agg.variant.aggregate.first_constant = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_u_agg, jit_u, &rv));
    c_u_agg.variant.aggregate.first_constant = &c_void_elem; /* fields[0] == NULL && vals[0] == NULL! */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_u_agg, jit_u, &rv));
    u_tp.variant.class_struct_union.field_list = &st_f0;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &st_f0, uf0));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_u_agg, jit_u, &rv)); /* fields[0] != NULL && vals[0] == NULL! */
    c_u_agg.variant.aggregate.first_constant = &c1;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_u_agg, jit_u, &rv)); /* struct_type == NULL ? type! */

    /* L2496: error inside array lower_aggregate_constant element lowering */
    a_constant c_bad_el_addr, c_bad_arr_agg;
    std::memset(&c_bad_el_addr, 0, sizeof(c_bad_el_addr));
    c_bad_el_addr.kind = ck_address;
    c_bad_el_addr.variant.address.kind = abk_temporary;
    c_bad_el_addr.variant.address.variant.constant = NULL; /* returns INVALID_ARGUMENT! */
    std::memset(&c_bad_arr_agg, 0, sizeof(c_bad_arr_agg));
    c_bad_arr_agg.kind = ck_aggregate;
    c_bad_arr_agg.type = &arr_tp;
    c_bad_arr_agg.variant.aggregate.first_constant = &c_bad_el_addr;
    if (gcc_gen_be_lower_constant_rvalue(&c_bad_arr_agg, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    /* L2558 False (_BitInt(64) size 8 != 16 & non-bitint size 16), L2607 (val == 0 with expected_type == NULL),
       L2627 False (array of pointers ck_string), L2683/L2707/L2722 (ck_ptr_to_member edge branches) */
    a_type bi64_tp, custom16_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&bi64_tp, ik_bit_precise, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&custom16_tp, ik_long, 16));
    a_constant c_bi64, c_c16;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_bi64, &bi64_tp, 5));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_c16, &custom16_tp, 5));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_bi64, jit_int, &rv));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_c16, jit_int, &rv));

    /* L2607 True: val == 0 with expected_type == NULL -> safe_ptr_or_int_cast fails -> get_zero_rvalue(int_tp, NULL)! */
    a_constant c_zero_typed;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_zero_typed, &int_tp, 0));
    GCC_GEN_BE_CHECK(expr_test_map_type(&int_tp, NULL));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_zero_typed, NULL, &rv));
    g_expr_mock.num_type_overrides = 0;

    /* L2627 False: ck_string with tk_array of non-tk_integer (vptr_tp) and NULL element_type */
    a_type ptr_arr_tp;
    std::memset(&ptr_arr_tp, 0, sizeof(ptr_arr_tp));
    ptr_arr_tp.kind = tk_array;
    ptr_arr_tp.variant.array.element_type = &vptr_tp;
    ptr_arr_tp.variant.array.variant.number_of_elements = 2;
    c_str.type = &ptr_arr_tp;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_str, NULL, &rv));

    /* L2683 True (is_function_ptr == TRUE, rout == NULL, pm_type == jit_int / NULL),
       L2707:33 False (is_virtual == TRUE, number == VIRTUAL_FUNCTION_NUMBER_NONE),
       L2722 False (non-virtual function PMF with cast_to_base == TRUE on SYSV_AMD64 & AAPCS64)! */
    a_base_class pm_b;
    std::memset(&pm_b, 0, sizeof(pm_b));
    pm_b.offset = 8;
    a_routine r_vnone, r_nonvirt;
    std::memset(&r_vnone, 0, sizeof(r_vnone));
    r_vnone.type = &vfn_tp;
    r_vnone.is_virtual = TRUE;
    r_vnone.number.virtual_function = VIRTUAL_FUNCTION_NUMBER_NONE;
    std::memset(&r_nonvirt, 0, sizeof(r_nonvirt));
    r_nonvirt.type = &vfn_tp;
    r_nonvirt.is_virtual = FALSE;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_vnone, fn_st));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_nonvirt, fn_st));

    a_constant c_pm;
    std::memset(&c_pm, 0, sizeof(c_pm));
    c_pm.kind = ck_ptr_to_member;
    c_pm.variant.ptr_to_member.is_function_ptr = TRUE;
    c_pm.variant.ptr_to_member.variant.routine = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_pm, jit_int, &rv));

    c_pm.variant.ptr_to_member.cast_to_base = TRUE;
    c_pm.variant.ptr_to_member.casting_base_class = &pm_b;
    c_pm.variant.ptr_to_member.variant.routine = &r_vnone;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_pm, g_expr_mock.pmf_struct_type, &rv));

    c_pm.variant.ptr_to_member.variant.routine = &r_nonvirt;
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_pm, g_expr_mock.pmf_struct_type, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_pm, g_expr_mock.pmf_struct_type, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Covers remaining edge branches across helpers, `dynamic_init`, lvalue materialization, constants, and `fixed_point`.
 * @details Exercises `uses_sret` invoke/block-cleared paths, `unpacked_ret == NULL`, `resolve_lvalue_c11_atomic_type`
 * null guards, VLA constant loop null-element/null-block branches, `dik_expression` struct mismatch, unknown `dip->kind`,
 * 2D VLA subscripting, bitfield constant OOM cleanup, aggregate/union constant edge cases, 128-bit/string/PMF null-type
 * constants, and `gcc_gen_be_lower_fixed_point_op` when `res_type == wide_type`.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_part_a_and_b_remaining_gaps(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, long_tp, dbl_tp, void_tp, vptr_tp, st_tp, int128_tp, ptr_null_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&int128_tp, ik_int128, 16));
    GCC_GEN_BE_CHECK(expr_init_float_type(&dbl_tp, fk_double, 8));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));
    std::memset(&ptr_null_tp, 0, sizeof(ptr_null_tp));
    ptr_null_tp.kind = tk_pointer;
    ptr_null_tp.size = 8;
    ptr_null_tp.alignment = 8;
    ptr_null_tp.variant.pointer.type = NULL;

    std::memset(&st_tp, 0, sizeof(st_tp));
    st_tp.kind = tk_struct;
    st_tp.size = 4;
    st_tp.alignment = 4;

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_long = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_type *jit_llong = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_vptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    GCC_GEN_BE_CHECK(expr_test_map_type(&st_tp, g_expr_mock.one_field_struct_type));

    gcc_jit_lvalue *lv_i = gcc_jit_function_new_local(fn, NULL, jit_int, "gap_i");
    gcc_jit_lvalue *lv_st = gcc_jit_function_new_local(fn, NULL, g_expr_mock.one_field_struct_type, "gap_st");
    gcc_jit_lvalue *lv_vp = gcc_jit_function_new_local(fn, NULL, jit_vptr, "gap_vp");
    a_variable v_i, v_st, v_vp;
    std::memset(&v_i, 0, sizeof(v_i)); v_i.type = &int_tp;
    std::memset(&v_st, 0, sizeof(v_st)); v_st.type = &st_tp;
    std::memset(&v_vp, 0, sizeof(v_vp)); v_vp.type = &vptr_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_i, lv_i));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_st, lv_st));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vp, lv_vp));

    gcc_jit_rvalue *rv = NULL;
    gcc_jit_lvalue *lv = NULL;

    /* 1. uses_sret with active_lp (direct func & indirect fn_ptr) and with invoke_cblock == NULL */
    gcc_jit_type *sret_ptr_t = gcc_jit_type_get_pointer(g_expr_mock.one_field_struct_type);
    gcc_jit_param *sret_p1_only = gcc_jit_context_new_param(ctx, NULL, sret_ptr_t, "sret_out1");
    gcc_jit_param *sret_p[2] = {
        gcc_jit_context_new_param(ctx, NULL, sret_ptr_t, "sret_out2"),
        gcc_jit_context_new_param(ctx, NULL, jit_int, "sret_a0")
    };
    gcc_jit_function *j_sret1 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "gap_sret1", 1, &sret_p1_only, 0);
    gcc_jit_function *j_sret2 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "gap_sret2", 2, sret_p, 0);
    gcc_jit_type *sret_fn_ptr_t = gcc_jit_context_new_function_ptr_type(
        ctx, NULL, jit_void, 1, &sret_ptr_t, 0);
    gcc_jit_lvalue *lv_sret_fp = gcc_jit_function_new_local(fn, NULL, sret_fn_ptr_t, "gap_sret_fp");

    a_type sret_rfn_tp, sret_fp_tp;
    std::memset(&sret_rfn_tp, 0, sizeof(sret_rfn_tp));
    sret_rfn_tp.kind = tk_routine;
    sret_rfn_tp.variant.routine.return_type = &st_tp;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&sret_fp_tp, &sret_rfn_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_test_map_type(&sret_fp_tp, sret_fn_ptr_t));

    a_routine r_sret1, r_sret2;
    std::memset(&r_sret1, 0, sizeof(r_sret1)); r_sret1.type = &sret_rfn_tp;
    std::memset(&r_sret2, 0, sizeof(r_sret2)); r_sret2.type = &sret_rfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_sret1, j_sret1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_sret2, j_sret2));

    a_variable v_sret_fp;
    std::memset(&v_sret_fp, 0, sizeof(v_sret_fp));
    v_sret_fp.type = &sret_fp_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_sret_fp, lv_sret_fp));

    an_expr_node e_rsret1, e_rsret2, e_vsret_fp, e_sret_call;
    std::memset(&e_rsret1, 0, sizeof(e_rsret1));
    e_rsret1.kind = enk_routine; e_rsret1.type = &sret_rfn_tp; e_rsret1.variant.routine.ptr = &r_sret1;
    std::memset(&e_rsret2, 0, sizeof(e_rsret2));
    e_rsret2.kind = enk_routine; e_rsret2.type = &sret_rfn_tp; e_rsret2.variant.routine.ptr = &r_sret2;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vsret_fp, &v_sret_fp));

    gcc_jit_block *lp_blk = gcc_jit_function_new_block(fn, "gap_lp");
    gcc_jit_block_end_with_void_return(lp_blk, NULL);
    g_expr_mock.is_sret = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(lp_blk));
    GCC_GEN_BE_CHECK(expr_init_op(&e_sret_call, eok_call, &st_tp, &e_rsret1));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sret_call, &rv)); /* L375 True */
    e_sret_call.variant.operation.operands = &e_vsret_fp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sret_call, &rv)); /* L375 False */
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());

    /* uses_sret with invoke_cblock == NULL (throw expression arg clears cblock) */
    an_expr_node e_throw_arg;
    std::memset(&e_throw_arg, 0, sizeof(e_throw_arg));
    e_throw_arg.kind = enk_throw;
    e_throw_arg.type = &int_tp;
    e_rsret2.next = &e_throw_arg;
    e_sret_call.variant.operation.operands = &e_rsret2;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sret_call, &rv)); /* L382 False */
    g_expr_mock.is_sret = FALSE;
    e_rsret2.next = NULL;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* 2. unpacked_ret == NULL in finish_direct_or_indirect_call (L423 False) */
    gcc_jit_function *j_ret_long = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_long, "gap_ret_long", 0, NULL, 0);
    a_routine r_ret_long;
    std::memset(&r_ret_long, 0, sizeof(r_ret_long)); r_ret_long.type = &sret_rfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_ret_long, j_ret_long));
    e_rsret1.variant.routine.ptr = &r_ret_long;
    e_sret_call.variant.operation.operands = &e_rsret1;
    g_expr_mock.coerce_rval_return_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sret_call, &rv));
    g_expr_mock.coerce_rval_return_null = FALSE;

    /* 3. resolve_lvalue_c11_atomic_type null guards (L452, L471, L474) */
    an_expr_node e_null_var, e_ind, e_vi, e_vpnull, e_assign, e_vst;
    std::memset(&e_null_var, 0, sizeof(e_null_var));
    e_null_var.kind = enk_variable; e_null_var.type = &int_tp; e_null_var.variant.variable.ptr = NULL;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi, &v_i));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vst, &v_st));
    /* Trigger resolve_lvalue_c11_atomic_type via pre_incr on eok_indirect with NULL/non-ptr/ptr-to-NULL */
    GCC_GEN_BE_CHECK(expr_init_op(&e_ind, eok_indirect, &int_tp, NULL));
    an_expr_node e_pre;
    gcc_gen_be_error_t probe_err = GCC_GEN_BE_SUCCESS;
    GCC_GEN_BE_CHECK(expr_init_op(&e_pre, eok_pre_incr, &int_tp, &e_null_var));
    probe_err = gcc_gen_be_lower_expr_rvalue(&e_pre, &rv); /* L452 False */
    if (probe_err == GCC_GEN_BE_ERROR_OOM) return probe_err;
    e_pre.variant.operation.operands = &e_ind;
    probe_err = gcc_gen_be_lower_expr_rvalue(&e_pre, &rv); /* L471 False (op1 == NULL) */
    if (probe_err == GCC_GEN_BE_ERROR_OOM) return probe_err;
    e_ind.variant.operation.operands = &e_vi;
    probe_err = gcc_gen_be_lower_expr_rvalue(&e_pre, &rv); /* L474 1st False (op1->type not tk_pointer) */
    if (probe_err == GCC_GEN_BE_ERROR_OOM) return probe_err;
    e_vpnull = e_vi;
    e_vpnull.type = &ptr_null_tp;
    e_ind.variant.operation.operands = &e_vpnull;
    probe_err = gcc_gen_be_lower_expr_rvalue(&e_pre, &rv); /* L474 2nd False (pointer.type == NULL) */
    if (probe_err == GCC_GEN_BE_ERROR_OOM) return probe_err;

    /* 4. dik_constant ck_init_repeat loop with elem_rval == NULL and cblock == NULL (L694),
          dik_expression struct mismatch (L793), dik_constructor value_init with cblock == NULL (L810), and default dip->kind (L911) */
    a_type vla_tp;
    std::memset(&vla_tp, 0, sizeof(vla_tp));
    vla_tp.kind = tk_array;
    vla_tp.variant.array.element_type = &int_tp;
    vla_tp.variant.array.is_variable_size_array = TRUE;
    vla_tp.variant.array.has_assoc_vla_dimension = FALSE;
    vla_tp.variant.array.variant.number_of_elements = 1;
    a_variable v_vla;
    std::memset(&v_vla, 0, sizeof(v_vla)); v_vla.type = &vla_tp;

    a_dynamic_init dip_clr, dip_vla;
    std::memset(&dip_clr, 0, sizeof(dip_clr));
    dip_clr.kind = dik_expression;
    dip_clr.variant.expression = &e_throw_arg;
    a_field st_f0;
    std::memset(&st_f0, 0, sizeof(st_f0)); st_f0.type = &int_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &st_f0, gcc_jit_struct_get_field(gcc_jit_type_is_struct(g_expr_mock.one_field_struct_type), 0)));
    st_tp.variant.class_struct_union.field_list = &st_f0;

    a_constant c_dyn_clr, c_unk_elem, c_st_clr, c_rep;
    std::memset(&c_dyn_clr, 0, sizeof(c_dyn_clr));
    c_dyn_clr.kind = ck_dynamic_init; c_dyn_clr.type = &int_tp; c_dyn_clr.variant.dynamic_init.ptr = &dip_clr;
    std::memset(&c_unk_elem, 0, sizeof(c_unk_elem));
    c_unk_elem.kind = (a_constant_repr_kind)99; c_unk_elem.type = &int_tp;
    std::memset(&c_st_clr, 0, sizeof(c_st_clr));
    c_st_clr.kind = ck_aggregate; c_st_clr.type = &st_tp; c_st_clr.variant.aggregate.first_constant = &c_dyn_clr;
    std::memset(&c_rep, 0, sizeof(c_rep));
    c_rep.kind = ck_init_repeat;
    c_rep.variant.init_repeat.count = 1;
    c_rep.variant.init_repeat.constant = &c_unk_elem;

    gcc_jit_type *jit_st_arr1 = gcc_jit_context_new_array_type(ctx, NULL, g_expr_mock.one_field_struct_type, 1);
    gcc_jit_lvalue *lv_st_arr1 = gcc_jit_function_new_local(fn, NULL, jit_st_arr1, "gap_st_arr1");

    std::memset(&dip_vla, 0, sizeof(dip_vla));
    dip_vla.kind = dik_constant;
    dip_vla.variable = &v_vla;
    dip_vla.variant.constant.ptr = &c_rep;
    g_expr_mock.force_zero_null = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip_vla, lv_st_arr1, &rv)); /* elem_rval == NULL -> L694 1st False */
    g_expr_mock.force_zero_null = FALSE;

    c_rep.variant.init_repeat.constant = &c_st_clr;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip_vla, lv_st_arr1, &rv)); /* elem_rval != NULL && cblock == NULL -> L694 2nd False */
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L793 False: dik_expression with non-struct target_lval (int) and struct init_rval */
    a_dynamic_init dip_ex;
    std::memset(&dip_ex, 0, sizeof(dip_ex));
    dip_ex.kind = dik_expression;
    dip_ex.variant.expression = &e_vst;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip_ex, lv_i, &rv));

    /* L810 False: dik_constructor value_initialization with cblock == NULL */
    a_dynamic_init dip_ctor;
    std::memset(&dip_ctor, 0, sizeof(dip_ctor));
    dip_ctor.kind = dik_constructor;
    dip_ctor.variable = &v_i;
    dip_ctor.variant.constructor.value_initialization = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip_ctor, lv_i, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L911: unknown dip->kind (7 fits in 3-bit/4-bit bitfield without wrapping to 0..5) */
    dip_ctor.kind = (a_dynamic_init_kind)7;
    GCC_GEN_BE_CHECK(expr_sweep_dyn_init_oom(&dip_ctor, lv_i, &rv));

    /* 5. L1318 True: 2D VLA array subscript & L1472 False: enk_builtin_operation lval with cblock == NULL */
    a_type arr2d_vla_tp;
    std::memset(&arr2d_vla_tp, 0, sizeof(arr2d_vla_tp));
    arr2d_vla_tp.kind = tk_array;
    arr2d_vla_tp.variant.array.element_type = &vla_tp;
    arr2d_vla_tp.variant.array.is_variable_size_array = TRUE;
    GCC_GEN_BE_CHECK(expr_test_map_type(&vla_tp, jit_vptr));
    a_variable v_arr2d;
    std::memset(&v_arr2d, 0, sizeof(v_arr2d)); v_arr2d.type = &arr2d_vla_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_arr2d, lv_vp));
    an_expr_node e_varr2d, e_sub2d, e_c0;
    a_constant c_zero;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_zero, &int_tp, 0));
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_c0, &c_zero));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_varr2d, &v_arr2d));
    e_varr2d.next = &e_c0;
    GCC_GEN_BE_CHECK(expr_init_op(&e_sub2d, eok_subscript, &vla_tp, &e_varr2d));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub2d, &lv));
    e_varr2d.next = NULL;

    an_expr_node e_boff;
    std::memset(&e_boff, 0, sizeof(e_boff));
    e_boff.kind = enk_builtin_operation;
    e_boff.type = &long_tp;
    e_boff.variant.builtin_operation.kind = bok_intaddr;
    e_boff.variant.builtin_operation.operands = &e_c0;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_lvalue(&e_boff, &lv) != GCC_GEN_BE_ERROR_INVALID_STATE) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* 6. lower_address_constant: abk_temporary with inner_val==NULL and inner_type==NULL (L2015-L2035),
          uncached global abk_variable with offset != 0, and elem_sz == 0 array offset */
    a_constant c_addr_tmp, c_int_untyped;
    std::memset(&c_int_untyped, 0, sizeof(c_int_untyped));
    c_int_untyped.kind = ck_integer; c_int_untyped.type = NULL;
    GCC_GEN_BE_CHECK(expr_test_map_const_int(&c_int_untyped, 7));

    std::memset(&c_addr_tmp, 0, sizeof(c_addr_tmp));
    c_addr_tmp.kind = ck_address;
    c_addr_tmp.type = &vptr_tp;
    c_addr_tmp.variant.address.kind = abk_temporary;
    c_addr_tmp.variant.address.variant.constant = &c_unk_elem; /* inner_val == NULL */
    g_expr_mock.force_zero_null = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr_tmp, jit_vptr, &rv));
    g_expr_mock.force_zero_null = FALSE;
    c_addr_tmp.variant.address.variant.constant = &c_int_untyped;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr_tmp, NULL, &rv));

    a_variable v_glob_uncached;
    std::memset(&v_glob_uncached, 0, sizeof(v_glob_uncached));
    v_glob_uncached.type = &int_tp;
    g_expr_mock.mock_global_lval = lv_i;
    c_addr_tmp.variant.address.kind = abk_variable;
    c_addr_tmp.variant.address.variant.variable = &v_glob_uncached;
    c_addr_tmp.variant.address.offset = 4;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr_tmp, jit_vptr, &rv)); /* cached == NULL at L2050 */
    g_expr_mock.mock_global_lval = NULL;

    a_type zero_el_tp, zero_arr_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&zero_el_tp, ik_int, 0));
    std::memset(&zero_arr_tp, 0, sizeof(zero_arr_tp));
    zero_arr_tp.kind = tk_array;
    zero_arr_tp.size = 0;
    zero_arr_tp.variant.array.element_type = &zero_el_tp;
    gcc_jit_type *jit_arr4 = gcc_jit_context_new_array_type(ctx, NULL, jit_int, 2);
    gcc_jit_lvalue *lv_zarr = gcc_jit_function_new_local(fn, NULL, jit_arr4, "gap_zarr");
    a_variable v_zarr;
    std::memset(&v_zarr, 0, sizeof(v_zarr)); v_zarr.type = &zero_arr_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_zarr, lv_zarr));
    c_addr_tmp.variant.address.variant.variable = &v_zarr;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_addr_tmp, jit_vptr, &rv)); /* elem_sz == 0 */

    /* 7. Bitfield aggregate constant OOM sweep (hits bf_pack_fail!) + struct/union aggregate constant edges */
    gcc_jit_field *bf_jf = gcc_jit_context_new_field(ctx, NULL, jit_int, "bf_word");
    gcc_jit_struct *bf_st = gcc_jit_context_new_struct_type(ctx, NULL, "_gap_bf_st", 1, &bf_jf);
    gcc_jit_type *bf_st_t = gcc_jit_struct_as_type(bf_st);
    a_field bf1, bf2;
    std::memset(&bf1, 0, sizeof(bf1)); bf1.type = &int_tp; bf1.is_bit_field = TRUE; bf1.bit_size = 4;
    std::memset(&bf2, 0, sizeof(bf2)); bf2.type = &int_tp; bf2.is_bit_field = TRUE; bf2.bit_size = 4; bf2.offset_bit_remainder = 4;
    bf1.next = &bf2;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf1, bf_jf));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf2, bf_jf));
    a_type bf_edg_st;
    std::memset(&bf_edg_st, 0, sizeof(bf_edg_st));
    bf_edg_st.kind = tk_struct; bf_edg_st.size = 4; bf_edg_st.variant.class_struct_union.field_list = &bf1;
    GCC_GEN_BE_CHECK(expr_test_map_type(&bf_edg_st, bf_st_t));

    a_constant c_bf1, c_bf2, c_bf_agg;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_bf1, &int_tp, 1));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_bf2, &int_tp, 2));
    c_bf1.next = &c_bf2;
    std::memset(&c_bf_agg, 0, sizeof(c_bf_agg));
    c_bf_agg.kind = ck_aggregate; c_bf_agg.type = &bf_edg_st; c_bf_agg.variant.aggregate.first_constant = &c_bf1;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_constant_rvalue(&c_bf_agg, bf_st_t, &rv);
    }));

    /* Struct aggregate constant: ptr->ptr (L2350), base/field 1-field-struct coercion (L2357-2370), scalar->ptr (L2374), null vals[idx] */
    gcc_jit_field *mix_jf[6] = {
        gcc_jit_context_new_field(ctx, NULL, g_expr_mock.one_field_struct_type, "_base_0"),
        gcc_jit_context_new_field(ctx, NULL, jit_vptr, "mp_str"),
        gcc_jit_context_new_field(ctx, NULL, g_expr_mock.one_field_struct_type, "ms_from_long"),
        gcc_jit_context_new_field(ctx, NULL, jit_vptr, "mp_from_int"),
        gcc_jit_context_new_field(ctx, NULL, jit_int, "mi_from_st"),
        gcc_jit_context_new_field(ctx, NULL, jit_long, "ml_null")
    };
    gcc_jit_struct *mix_st = gcc_jit_context_new_struct_type(ctx, NULL, "_gap_mix_st", 6, mix_jf);
    gcc_jit_type *mix_st_t = gcc_jit_struct_as_type(mix_st);
    a_type null_mapped_tp;
    std::memset(&null_mapped_tp, 0, sizeof(null_mapped_tp));
    null_mapped_tp.kind = tk_integer;
    GCC_GEN_BE_CHECK(expr_test_map_type(&null_mapped_tp, NULL));

    /* Build 1-field struct field list on st_tp */
    a_field sf0;
    std::memset(&sf0, 0, sizeof(sf0)); sf0.type = &int_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sf0, gcc_jit_struct_get_field(gcc_jit_type_is_struct(g_expr_mock.one_field_struct_type), 0)));
    st_tp.variant.class_struct_union.field_list = &sf0;

    a_base_class mix_bc;
    std::memset(&mix_bc, 0, sizeof(mix_bc));
    mix_bc.type = &st_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &mix_bc, mix_jf[0]));
    a_class_type_supplement mix_sup;
    std::memset(&mix_sup, 0, sizeof(mix_sup));
    mix_sup.direct_base_classes = &mix_bc;

    a_field mf[5];
    std::memset(mf, 0, sizeof(mf));
    mf[0].type = &vptr_tp; mf[1].type = &st_tp; mf[2].type = &vptr_tp; mf[3].type = &int_tp; mf[4].type = &null_mapped_tp;
    for (int k = 0; k < 5; ++k) {
        if (k < 4) mf[k].next = &mf[k + 1];
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &mf[k], mix_jf[k + 1]));
    }
    a_type mix_edg_st;
    std::memset(&mix_edg_st, 0, sizeof(mix_edg_st));
    mix_edg_st.kind = tk_struct; mix_edg_st.size = 48;
    mix_edg_st.variant.class_struct_union.extra_info = &mix_sup;
    mix_edg_st.variant.class_struct_union.field_list = &mf[0];
    GCC_GEN_BE_CHECK(expr_test_map_type(&mix_edg_st, mix_st_t));

    gcc_jit_lvalue *lv_l = gcc_jit_function_new_local(fn, NULL, jit_long, "gap_l");
    a_variable v_l;
    std::memset(&v_l, 0, sizeof(v_l)); v_l.type = &long_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_l, lv_l));
    an_expr_node e_vl;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vl, &v_l));

    a_dynamic_init dip_long, dip_int, dip_none;
    std::memset(&dip_long, 0, sizeof(dip_long)); dip_long.kind = dik_expression; dip_long.variant.expression = &e_vl;
    std::memset(&dip_int, 0, sizeof(dip_int)); dip_int.kind = dik_expression; dip_int.variant.expression = &e_vi;
    std::memset(&dip_none, 0, sizeof(dip_none)); dip_none.kind = dik_none;

    gcc_jit_field *ujf[2] = {
        gcc_jit_context_new_field(ctx, NULL, jit_long, "ul"),
        gcc_jit_context_new_field(ctx, NULL, jit_int, "ui")
    };
    gcc_jit_type *u_jit_t = gcc_jit_context_new_union_type(ctx, NULL, "_gap_u", 2, ujf);
    a_field uf[2];
    std::memset(uf, 0, sizeof(uf));
    uf[0].type = &long_tp; uf[1].type = &int_tp; uf[0].next = &uf[1];
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &uf[0], ujf[0]));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &uf[1], ujf[1]));
    a_type u_edg_tp;
    std::memset(&u_edg_tp, 0, sizeof(u_edg_tp));
    u_edg_tp.kind = tk_union; u_edg_tp.size = 8; u_edg_tp.variant.class_struct_union.field_list = &uf[0];
    GCC_GEN_BE_CHECK(expr_test_map_type(&u_edg_tp, u_jit_t));

    a_constant c_sf_val, c_one_agg, c_base_sub;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_sf_val, &int_tp, 9));
    std::memset(&c_one_agg, 0, sizeof(c_one_agg));
    c_one_agg.kind = ck_aggregate; c_one_agg.type = &st_tp; c_one_agg.variant.aggregate.first_constant = &c_sf_val;
    std::memset(&c_base_sub, 0, sizeof(c_base_sub));
    c_base_sub.kind = ck_aggregate; c_base_sub.type = &u_edg_tp; c_base_sub.variant.aggregate.first_constant = &c_sf_val;

    a_constant mc_b, mc0, mc1, mc2, mc3, mc4, c_mix_agg;
    mc_b = c_base_sub;
    std::memset(&mc0, 0, sizeof(mc0)); mc0.kind = ck_string; mc0.type = &vptr_tp; mc0.variant.string.value = "x"; mc0.variant.string.length = 2;
    std::memset(&mc1, 0, sizeof(mc1)); mc1.kind = ck_dynamic_init; mc1.type = &long_tp; mc1.variant.dynamic_init.ptr = &dip_long;
    std::memset(&mc2, 0, sizeof(mc2)); mc2.kind = ck_dynamic_init; mc2.type = &int_tp; mc2.variant.dynamic_init.ptr = &dip_int;
    mc3 = c_one_agg;
    std::memset(&mc4, 0, sizeof(mc4)); mc4.kind = ck_dynamic_init; mc4.type = &null_mapped_tp; mc4.variant.dynamic_init.ptr = &dip_none;
    mc_b.next = &mc0; mc0.next = &mc1; mc1.next = &mc2; mc2.next = &mc3; mc3.next = &mc4;
    std::memset(&c_mix_agg, 0, sizeof(c_mix_agg));
    c_mix_agg.kind = ck_aggregate; c_mix_agg.type = &mix_edg_st; c_mix_agg.variant.aggregate.first_constant = &mc_b;
    g_expr_mock.force_zero_null = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_mix_agg, mix_st_t, &rv));
    g_expr_mock.force_zero_null = FALSE;

    /* Union aggregate constant with designator (both dangling and valid) and null u_val */
    a_constant c_desig, c_uval, c_u_agg;
    std::memset(&c_desig, 0, sizeof(c_desig));
    c_desig.kind = ck_designator;
    c_desig.variant.designator.is_field_designator = TRUE;
    c_desig.variant.designator.variant.field = &uf[1];
    std::memset(&c_u_agg, 0, sizeof(c_u_agg));
    c_u_agg.kind = ck_aggregate; c_u_agg.type = &u_edg_tp;
    c_u_agg.variant.aggregate.first_constant = &c_desig; /* dangling designator -> val_c == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_u_agg, u_jit_t, &rv));

    GCC_GEN_BE_CHECK(expr_init_int_const(&c_uval, &int_tp, 3));
    c_desig.next = &c_uval; /* designator -> uf[1] */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_u_agg, u_jit_t, &rv));
    c_u_agg.variant.aggregate.first_constant = &mc4; /* u_val == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_u_agg, u_jit_t, &rv));

    /* 8. 128-bit constant with expected_type == NULL (L2575), string array with elem_type_ptr == NULL (L2631),
          PMF constant with expected_type == NULL (L2679), and fixed_point with res_type == wide_type (L6058) */
    a_constant c128;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c128, &int128_tp, 1));
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c128, NULL, &rv));

    a_type str_arr_null_el;
    std::memset(&str_arr_null_el, 0, sizeof(str_arr_null_el));
    str_arr_null_el.kind = tk_array;
    str_arr_null_el.variant.array.element_type = NULL;
    str_arr_null_el.variant.array.variant.number_of_elements = 2;
    a_constant c_str_ne;
    std::memset(&c_str_ne, 0, sizeof(c_str_ne));
    c_str_ne.kind = ck_string; c_str_ne.type = &str_arr_null_el;
    c_str_ne.variant.string.value = "a"; c_str_ne.variant.string.length = 2;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_str_ne, jit_arr4, &rv));

    a_constant c_pmf_null;
    std::memset(&c_pmf_null, 0, sizeof(c_pmf_null));
    c_pmf_null.kind = ck_ptr_to_member;
    c_pmf_null.variant.ptr_to_member.is_function_ptr = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_pmf_null, NULL, &rv));

    gcc_jit_rvalue *ll_a = gcc_jit_context_new_rvalue_from_long(ctx, jit_llong, 16);
    gcc_jit_rvalue *ll_b = gcc_jit_context_new_rvalue_from_long(ctx, jit_llong, 8);
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_op('+', 4, FALSE, ll_a, ll_b, &rv));

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises all remaining helper, dynamic-init, VLA, cast, aggregate/address constant, and decimal/fixed-point OOM/branch cases.
 * @details Covers `check_is_record_type` (`NULL`/`tk_union`), `check_is_bitfield_access_op(NULL)`,
 * `coerce_call_arg_if_needed` and `finish_direct_or_indirect_call` OOM/error/NULL-block paths,
 * `resolve_lvalue_c11_atomic_type` field OOM, `dik_expression`/`dik_bitwise_copy`/`dik_constructor`/unknown `dip->kind`,
 * `scale_vla_subarray_index` OOM, `safe_ptr_or_int_cast` struct-to-scalar and void/vector OOM,
 * `lower_address_constant` temporary/array-field edge cases, `lower_struct_aggregate_constant_inner`
 * 1-field struct pointer wrapping, bitfield designator edge cases, single-base constructor fallbacks,
 * 128-bit/zero/string constants, and decimal/fixed-point OOM budgets.
 * @param[in] ctx Active `libgccjit` context.
 * @param[in] fn Active `libgccjit` test function.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_init_const_final_gaps(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_long = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_type *jit_dbl = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_vptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *jit_cptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CONST_CHAR_PTR);
    gcc_jit_type *jit_iptr = gcc_jit_type_get_pointer(jit_int);
    gcc_jit_type *jit_u32 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T);

    a_type char_tp, int_tp, long_tp, void_tp, vptr_tp, iptr_tp, cptr_tp, u128_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&char_tp, ik_char, 1));
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&u128_tp, ik_unsigned_int128, 16));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&iptr_tp, &int_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&cptr_tp, &char_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_test_map_type(&cptr_tp, jit_cptr));

    gcc_jit_rvalue *rv = NULL;
    gcc_jit_lvalue *lv_i = gcc_jit_function_new_local(fn, NULL, jit_int, "fg_i");
    gcc_jit_lvalue *lv_l = gcc_jit_function_new_local(fn, NULL, jit_long, "fg_l");
    a_variable v_i, v_l;
    std::memset(&v_i, 0, sizeof(v_i)); v_i.type = &int_tp;
    std::memset(&v_l, 0, sizeof(v_l)); v_l.type = &long_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_i, lv_i));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_l, lv_l));
    an_expr_node e_vi, e_vl, e_op;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi, &v_i));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vl, &v_l));

    /* L118: check_is_bitfield_access_op(NULL, ...) via eok_assign with NULL operands */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_assign, &int_tp, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* L377, L380, L439, L442, L445, L463, L466, L484, L487: coerce & invoke call paths + OOM */
    a_param_type pt_arg;
    std::memset(&pt_arg, 0, sizeof(pt_arg));
    pt_arg.type = &int_tp;
    a_routine_type_supplement rt_sup;
    std::memset(&rt_sup, 0, sizeof(rt_sup));
    rt_sup.param_type_list = &pt_arg;
    a_type fn_tp, fn_ptr_tp;
    std::memset(&fn_tp, 0, sizeof(fn_tp));
    fn_tp.kind = tk_routine;
    fn_tp.variant.routine.return_type = &int_tp;
    fn_tp.variant.routine.extra_info = &rt_sup;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&fn_ptr_tp, &fn_tp, FALSE));

    a_routine r_callee;
    std::memset(&r_callee, 0, sizeof(r_callee));
    r_callee.type = &fn_tp;
    r_callee.source_corresp.name = "fg_callee";
    gcc_jit_param *cp0 = gcc_jit_context_new_param(ctx, NULL, jit_long, "p0");
    gcc_jit_function *j_callee = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_long, "fg_callee", 1, &cp0, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_callee, j_callee));

    gcc_jit_param *sp0 = gcc_jit_context_new_param(ctx, NULL, jit_vptr, "sret");
    gcc_jit_param *sp1 = gcc_jit_context_new_param(ctx, NULL, jit_long, "p0");
    gcc_jit_param *s_params[2] = { sp0, sp1 };
    a_routine r_sret;
    std::memset(&r_sret, 0, sizeof(r_sret));
    r_sret.type = &fn_tp;
    r_sret.source_corresp.name = "fg_sret_callee";
    gcc_jit_function *j_sret = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "fg_sret_callee", 2, s_params, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_sret, j_sret));

    gcc_jit_lvalue *lv_fptr = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_function_get_address(j_callee, NULL)
            ? gcc_jit_rvalue_get_type(gcc_jit_function_get_address(j_callee, NULL)) : jit_vptr,
        "fg_fptr");
    gcc_jit_lvalue *lv_sret_fptr = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_rvalue_get_type(gcc_jit_function_get_address(j_sret, NULL)),
        "fg_sret_fptr");
    a_variable v_fptr, v_sret_fptr;
    std::memset(&v_fptr, 0, sizeof(v_fptr)); v_fptr.type = &fn_ptr_tp;
    std::memset(&v_sret_fptr, 0, sizeof(v_sret_fptr)); v_sret_fptr.type = &fn_ptr_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_fptr, lv_fptr));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_sret_fptr, lv_sret_fptr));

    an_expr_node e_rcallee, e_rsret, e_vfptr, e_vsfptr, e_arg1;
    std::memset(&e_rcallee, 0, sizeof(e_rcallee));
    e_rcallee.kind = enk_routine; e_rcallee.type = &fn_tp; e_rcallee.variant.routine.ptr = &r_callee;
    std::memset(&e_rsret, 0, sizeof(e_rsret));
    e_rsret.kind = enk_routine; e_rsret.type = &fn_tp; e_rsret.variant.routine.ptr = &r_sret;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vfptr, &v_fptr));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vsfptr, &v_sret_fptr));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_arg1, &v_i));

    /* Coerced call arg + coerced return value OOM & error */
    g_expr_mock.needs_abi_coercion = TRUE;
    g_expr_mock.coerced_abi_type = jit_long;
    e_rcallee.next = &e_arg1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_rcallee));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));
    g_expr_mock.force_coerce_rval_err = GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_coerce_rval_err = GCC_GEN_BE_SUCCESS;
    e_rcallee.next = NULL;
    GCC_GEN_BE_CHECK(expr_test_reset_mock());

    /* Invoke calls with active_lp: direct & indirect, sret & non-sret, OOM + invoke_cblock == NULL */
    gcc_jit_block *lp_blk = gcc_jit_function_new_block(fn, "fg_lp");
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(lp_blk));

    /* 439:26 False: active_lp != NULL and invoke_cblock == NULL with uses_sret */
    g_expr_mock.is_sret = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    e_rsret.next = &e_arg1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_rsret));
    /* sret requires cblock at sret slot creation unless sret_lval passed; test non-sret with cblock==NULL too */
    g_expr_mock.is_sret = FALSE;
    e_rcallee.next = &e_arg1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_rcallee));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* Direct non-sret invoke OOM + error (L463) */
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    g_expr_mock.force_invoke_err = GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_invoke_err = GCC_GEN_BE_SUCCESS;
    e_rcallee.next = NULL;

    /* Indirect non-sret invoke OOM + error (L466) */
    e_vfptr.next = &e_arg1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_vfptr));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    g_expr_mock.force_invoke_ptr_err = GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_invoke_ptr_err = GCC_GEN_BE_SUCCESS;
    e_vfptr.next = NULL;

    /* Direct sret invoke OOM + error (L442) */
    g_expr_mock.is_sret = TRUE;
    e_rsret.next = &e_arg1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_rsret));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    g_expr_mock.force_invoke_err = GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_invoke_err = GCC_GEN_BE_SUCCESS;
    e_rsret.next = NULL;

    /* Indirect sret invoke OOM + error (L445) */
    e_vsfptr.next = &e_arg1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_vsfptr));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    g_expr_mock.force_invoke_ptr_err = GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_invoke_ptr_err = GCC_GEN_BE_SUCCESS;
    e_vsfptr.next = NULL;

    /* Also hit L439:26 False on uses_sret: sret_lval supplied via dik_class_result_via_ctor/call when cblock becomes NULL during arg eval */
    an_expr_node e_throw_arg;
    std::memset(&e_throw_arg, 0, sizeof(e_throw_arg));
    e_throw_arg.kind = enk_throw;
    e_throw_arg.type = &int_tp;
    e_rsret.next = &e_throw_arg;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_rsret));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_rsret.next = NULL;

    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    GCC_GEN_BE_CHECK(expr_test_reset_mock());
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L532: resolve_lvalue_c11_atomic_type on eok_dot_field OOM sweep */
    a_field f_at;
    std::memset(&f_at, 0, sizeof(f_at));
    f_at.type = &int_tp;
    a_type st_at;
    std::memset(&st_at, 0, sizeof(st_at));
    st_at.kind = tk_struct; st_at.size = 4;
    st_at.variant.class_struct_union.field_list = &f_at;
    gcc_jit_type *jit_st_at = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&st_at, &jit_st_at));
    gcc_jit_lvalue *lv_st_at = gcc_jit_function_new_local(fn, NULL, jit_st_at, "fg_stat");
    a_variable v_stat;
    std::memset(&v_stat, 0, sizeof(v_stat)); v_stat.type = &st_at;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_stat, lv_st_at));
    an_expr_node e_vstat, e_fat;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vstat, &v_stat));
    std::memset(&e_fat, 0, sizeof(e_fat));
    e_fat.kind = enk_field; e_fat.type = &int_tp; e_fat.variant.field.ptr = &f_at;
    e_vstat.next = &e_fat;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_field, &int_tp, &e_vstat));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));
    e_vstat.next = NULL;

    /* L860, L869, L870, L876, L977: dynamic_init OOM & cblock==NULL & default switch arm */
    a_dynamic_init dip_cast, dip_bw, dip_vinit, dip_unk;
    std::memset(&dip_cast, 0, sizeof(dip_cast));
    dip_cast.kind = dik_expression;
    dip_cast.variant.expression = &e_vi;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_dynamic_init(&dip_cast, lv_l, &rv);
    }));

    std::memset(&dip_bw, 0, sizeof(dip_bw));
    dip_bw.kind = dik_bitwise_copy;
    dip_bw.variant.bitwise_copy.source = &e_vi;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_dynamic_init(&dip_bw, lv_i, &rv);
    }));

    std::memset(&dip_vinit, 0, sizeof(dip_vinit));
    dip_vinit.kind = dik_constructor;
    dip_vinit.variant.constructor.value_initialization = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(&dip_vinit, lv_i, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    std::memset(&dip_unk, 0, sizeof(dip_unk));
    dip_unk.kind = (a_dynamic_init_kind)99;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(&dip_unk, lv_i, &rv));

    /* L1050, L1053: scale_vla_subarray_index OOM with uncached dim_var AND dim_expr */
    a_variable v_vla_dim;
    std::memset(&v_vla_dim, 0, sizeof(v_vla_dim));
    v_vla_dim.type = &long_tp;
    a_type vla_sub_tp, ptr_vla_tp;
    std::memset(&vla_sub_tp, 0, sizeof(vla_sub_tp));
    vla_sub_tp.kind = tk_array;
    vla_sub_tp.variant.array.is_variable_size_array = TRUE;
    vla_sub_tp.variant.array.has_assoc_vla_dimension = TRUE;
    vla_sub_tp.variant.array.element_type = &int_tp;
    a_vla_dimension vla_d1;
    std::memset(&vla_d1, 0, sizeof(vla_d1));
    vla_d1.type = &vla_sub_tp;
    vla_d1.dimension_variable = &v_vla_dim;
    vla_d1.dimension_expr = &e_vl;
    a_scope fn_sc_vla;
    std::memset(&fn_sc_vla, 0, sizeof(fn_sc_vla));
    fn_sc_vla.vla_dimensions = &vla_d1;
    innermost_function_scope = &fn_sc_vla;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_vla_tp, &vla_sub_tp, FALSE));
    gcc_jit_lvalue *lv_pvla = gcc_jit_function_new_local(fn, NULL, jit_iptr, "fg_pvla");
    a_variable v_pvla;
    std::memset(&v_pvla, 0, sizeof(v_pvla)); v_pvla.type = &ptr_vla_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pvla, lv_pvla));
    an_expr_node e_pvla;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_pvla, &v_pvla));
    e_pvla.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_padd, &ptr_vla_tp, &e_pvla));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));
    e_pvla.next = NULL;
    innermost_function_scope = NULL;

    /* L1932, L1938:56, L1965: safe_ptr_or_int_cast void src OOM, struct-to-long cast, vector bitcast OOM */
    a_routine r_void_fn;
    std::memset(&r_void_fn, 0, sizeof(r_void_fn));
    a_type vfn_tp;
    std::memset(&vfn_tp, 0, sizeof(vfn_tp)); vfn_tp.kind = tk_routine; vfn_tp.variant.routine.return_type = &void_tp;
    r_void_fn.type = &vfn_tp; r_void_fn.source_corresp.name = "fg_vfn";
    gcc_jit_function *j_vfn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "fg_vfn", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_void_fn, j_vfn));
    an_expr_node e_rvfn, e_vcall;
    std::memset(&e_rvfn, 0, sizeof(e_rvfn));
    e_rvfn.kind = enk_routine; e_rvfn.type = &vfn_tp; e_rvfn.variant.routine.ptr = &r_void_fn;
    GCC_GEN_BE_CHECK(expr_init_op(&e_vcall, eok_call, &void_tp, &e_rvfn));
    /* bok_intaddr on void call rvalue triggers safe_ptr_or_int_cast(void_rval, jit_long) -> L1932! */
    a_routine r_vret_int;
    std::memset(&r_vret_int, 0, sizeof(r_vret_int));
    r_vret_int.type = &vfn_tp; r_vret_int.source_corresp.name = "fg_vfn2";
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_vret_int, j_vfn));
    e_rvfn.variant.routine.ptr = &r_vret_int;
    e_vcall.type = &int_tp; /* mismatch: j_vfn returns void_t, expr->type is int_tp -> safe_ptr_or_int_cast(void_t, int_t)! */
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_vcall, &rv);
    }));

    /* Struct-to-long cast (L1938:56 False) */
    a_type two_st_tp;
    std::memset(&two_st_tp, 0, sizeof(two_st_tp));
    two_st_tp.kind = tk_struct; two_st_tp.size = 16;
    GCC_GEN_BE_CHECK(expr_test_map_type(&two_st_tp, g_expr_mock.pmf_struct_type));
    gcc_jit_lvalue *lv_pmfst = gcc_jit_function_new_local(fn, NULL, g_expr_mock.pmf_struct_type, "fg_pmfst");
    a_variable v_pmfst;
    std::memset(&v_pmfst, 0, sizeof(v_pmfst)); v_pmfst.type = &two_st_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pmfst, lv_pmfst));
    an_expr_node e_vpmfst;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vpmfst, &v_pmfst));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_cast, &long_tp, &e_vpmfst));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* Vector-to-vector bitcast OOM (L1965) */
    a_type vec4i_tp, vec2l_tp;
    std::memset(&vec4i_tp, 0, sizeof(vec4i_tp));
    vec4i_tp.kind = tk_vector; vec4i_tp.size = 16; vec4i_tp.variant.vector.element_type = &int_tp;
    std::memset(&vec2l_tp, 0, sizeof(vec2l_tp));
    vec2l_tp.kind = tk_vector; vec2l_tp.size = 16; vec2l_tp.variant.vector.element_type = &long_tp;
    gcc_jit_type *jit_v4i = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&vec4i_tp, &jit_v4i));
    gcc_jit_lvalue *lv_v4i = gcc_jit_function_new_local(fn, NULL, jit_v4i, "fg_v4i");
    a_variable v_v4i;
    std::memset(&v_v4i, 0, sizeof(v_v4i)); v_v4i.type = &vec4i_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_v4i, lv_v4i));
    an_expr_node e_vv4i;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vv4i, &v_v4i));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_cast, &vec2l_tp, &e_vv4i));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));

    /* L2096:69, L2096:83, L2101:16, L2115: lower_address_constant abk_temporary / abk_constant */
    a_constant c_void_inner, c_addr_tmp, c_int_inner, c_addr_glob;
    std::memset(&c_void_inner, 0, sizeof(c_void_inner));
    c_void_inner.kind = ck_void; c_void_inner.type = &void_tp;
    g_expr_mock.force_zero_null = TRUE;
    std::memset(&c_addr_tmp, 0, sizeof(c_addr_tmp));
    c_addr_tmp.kind = ck_address; c_addr_tmp.type = &vptr_tp;
    c_addr_tmp.variant.address.kind = abk_temporary;
    c_addr_tmp.variant.address.variant.constant = &c_void_inner;
    c_addr_tmp.variant.address.offset = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_addr_tmp, jit_vptr, &rv));
    g_expr_mock.force_zero_null = FALSE;

    GCC_GEN_BE_CHECK(expr_init_int_const(&c_int_inner, &int_tp, 7));
    c_addr_tmp.variant.address.variant.constant = &c_int_inner;
    c_addr_tmp.variant.address.offset = 4; /* offset != 0 -> L2096:83 False! */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_addr_tmp, jit_vptr, &rv));

    /* Fresh abk_constant OOM sweep to hit L2115 cache_insert OOM */
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(cache_clear_all());
        std::memset(&c_addr_glob, 0, sizeof(c_addr_glob));
        c_addr_glob.kind = ck_address; c_addr_glob.type = &vptr_tp;
        c_addr_glob.variant.address.kind = abk_constant;
        c_addr_glob.variant.address.variant.constant = &c_int_inner;
        return gcc_gen_be_lower_constant_rvalue(&c_addr_glob, jit_vptr, &rv);
    }));

    /* L2168:61 & L2170:37: struct field array with NULL element_type and 0-sized element_type */
    a_type z_elem_tp, arr_no_elem_tp, arr_z_elem_tp, st_arr_f_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&z_elem_tp, ik_int, 0));
    std::memset(&arr_no_elem_tp, 0, sizeof(arr_no_elem_tp));
    arr_no_elem_tp.kind = tk_array; arr_no_elem_tp.size = 16; arr_no_elem_tp.variant.array.element_type = NULL;
    std::memset(&arr_z_elem_tp, 0, sizeof(arr_z_elem_tp));
    arr_z_elem_tp.kind = tk_array; arr_z_elem_tp.size = 16; arr_z_elem_tp.variant.array.element_type = &z_elem_tp;

    a_field sf_a1, sf_a2;
    std::memset(&sf_a1, 0, sizeof(sf_a1)); sf_a1.offset = 0; sf_a1.type = &arr_no_elem_tp; sf_a1.next = &sf_a2;
    std::memset(&sf_a2, 0, sizeof(sf_a2)); sf_a2.offset = 16; sf_a2.type = &arr_z_elem_tp;
    std::memset(&st_arr_f_tp, 0, sizeof(st_arr_f_tp));
    st_arr_f_tp.kind = tk_struct; st_arr_f_tp.size = 32;
    st_arr_f_tp.variant.class_struct_union.field_list = &sf_a1;
    gcc_jit_type *jit_st_arrf = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&st_arr_f_tp, &jit_st_arrf));
    gcc_jit_lvalue *g_st_arrf = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, jit_st_arrf, "fg_st_arrf");
    a_variable v_st_arrf;
    std::memset(&v_st_arrf, 0, sizeof(v_st_arrf)); v_st_arrf.type = &st_arr_f_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_st_arrf, g_st_arrf));

    a_constant c_addr_sf;
    std::memset(&c_addr_sf, 0, sizeof(c_addr_sf));
    c_addr_sf.kind = ck_address; c_addr_sf.type = &vptr_tp;
    c_addr_sf.variant.address.kind = abk_variable;
    c_addr_sf.variant.address.variant.variable = &v_st_arrf;
    c_addr_sf.variant.address.offset = 4; /* inside sf_a1 (0..16), rem_off=4, element_type == NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_addr_sf, jit_vptr, &rv));
    c_addr_sf.variant.address.offset = 20; /* inside sf_a2 (16..32), rem_off=4, elem_sz == 0 */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_addr_sf, jit_vptr, &rv));

    /* L2277:21 (f == NULL), L77:23 (con_tp == NULL), L77:87 (tk_union),
       L2327:25, L2332:21, L2332:31, L2361, L2367, L2402-2494 */
    a_type empty_st_tp;
    std::memset(&empty_st_tp, 0, sizeof(empty_st_tp));
    empty_st_tp.kind = tk_struct; empty_st_tp.size = 4;
    gcc_jit_type *jit_empty_st = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&empty_st_tp, &jit_empty_st));
    a_constant c_i1, c_agg_empty_f;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_i1, &int_tp, 1));
    std::memset(&c_agg_empty_f, 0, sizeof(c_agg_empty_f));
    c_agg_empty_f.kind = ck_aggregate; c_agg_empty_f.type = &empty_st_tp;
    c_agg_empty_f.variant.aggregate.first_constant = &c_i1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_empty_f, jit_empty_st, &rv));

    /* Bitfield packing branches: L2327:25 (cached_bf_ft == NULL), L2332:21 (con_tp == NULL), L2332:31 (size < offset+8),
       L2361:53 (!is_field_designator), L2361:79 (is_generic), L2367:60 (bit_size>0), L2367:85 (next_c==NULL), L2367:95 (ck_float) */
    gcc_jit_field *bf_jf0 = gcc_jit_context_new_field(ctx, NULL, jit_u32, "_bf_u32");
    gcc_jit_type *bf_st_jit = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_fg_bf_st", 1, &bf_jf0));
    a_field bf1, bf2, bf3;
    std::memset(&bf1, 0, sizeof(bf1));
    bf1.type = &int_tp; bf1.is_bit_field = TRUE; bf1.bit_size = 16; bf1.offset = 0; bf1.offset_bit_remainder = 24; bf1.next = &bf2;
    std::memset(&bf2, 0, sizeof(bf2));
    bf2.type = &int_tp; bf2.is_bit_field = FALSE; bf2.bit_size = 4; bf2.offset = 0; bf2.offset_bit_remainder = 4; bf2.next = &bf3;
    std::memset(&bf3, 0, sizeof(bf3));
    bf3.type = &int_tp; bf3.is_bit_field = TRUE; bf3.bit_size = 4; bf3.offset = 0; bf3.offset_bit_remainder = 8;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf1, bf_jf0)); /* cached_bf_ft NOT in CACHE_TYPE -> L2327:25 False */

    a_type bf_edg_small;
    std::memset(&bf_edg_small, 0, sizeof(bf_edg_small));
    bf_edg_small.kind = tk_struct; bf_edg_small.size = 4; /* (0 + 8) <= 4 is False -> L2332:31 False */
    bf_edg_small.variant.class_struct_union.field_list = &bf1;
    GCC_GEN_BE_CHECK(expr_test_map_type(&bf_edg_small, bf_st_jit));

    /* Case A: con_tp == NULL with field designator pointing to bf1 -> L2332:21 False + L77:23 False! */
    a_constant d_bf1, v_bf1, d_nonfld, v_bf2, d_gen, d_trail, c_flt_bf, c_agg_bf;
    std::memset(&d_bf1, 0, sizeof(d_bf1));
    d_bf1.kind = ck_designator; d_bf1.variant.designator.is_field_designator = TRUE; d_bf1.variant.designator.variant.field = &bf1;
    GCC_GEN_BE_CHECK(expr_init_int_const(&v_bf1, &int_tp, 3));
    d_bf1.next = &v_bf1;
    /* Chain non-field designator -> L2361:53 False */
    std::memset(&d_nonfld, 0, sizeof(d_nonfld));
    d_nonfld.kind = ck_designator; d_nonfld.variant.designator.is_field_designator = FALSE;
    GCC_GEN_BE_CHECK(expr_init_int_const(&v_bf2, &int_tp, 2));
    v_bf1.next = &d_nonfld; d_nonfld.next = &v_bf2;
    std::memset(&c_agg_bf, 0, sizeof(c_agg_bf));
    c_agg_bf.kind = ck_aggregate; c_agg_bf.type = NULL; /* con_tp == NULL! */
    c_agg_bf.variant.aggregate.first_constant = &d_bf1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_bf, bf_st_jit, &rv));

    /* Case B: con_tp = &bf_edg_small (size 4 -> L2332:31 False), generic designator (L2361:79 False) + ck_float (L2367:95 False) */
    std::memset(&d_gen, 0, sizeof(d_gen));
    d_gen.kind = ck_designator; d_gen.variant.designator.is_field_designator = TRUE; d_gen.variant.designator.is_generic = TRUE;
    std::memset(&c_flt_bf, 0, sizeof(c_flt_bf));
    c_flt_bf.kind = ck_float; c_flt_bf.type = &int_tp;
    c_agg_bf.type = &bf_edg_small;
    c_agg_bf.variant.aggregate.first_constant = &v_bf1;
    v_bf1.next = &d_gen; d_gen.next = &c_flt_bf; c_flt_bf.next = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_bf, bf_st_jit, &rv));

    /* Case C: trailing designator with next == NULL (L2367:85 False) */
    std::memset(&d_trail, 0, sizeof(d_trail));
    d_trail.kind = ck_designator; d_trail.variant.designator.is_field_designator = TRUE; d_trail.variant.designator.variant.field = &bf3;
    v_bf1.next = &d_trail; d_trail.next = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_bf, bf_st_jit, &rv));
    v_bf1.next = NULL;

    /* L77:87 (tk_union in check_is_record_type): union aggregate constant */
    a_field u_fld;
    std::memset(&u_fld, 0, sizeof(u_fld)); u_fld.type = &int_tp;
    a_type union_tp;
    std::memset(&union_tp, 0, sizeof(union_tp));
    union_tp.kind = tk_union; union_tp.size = 4; union_tp.variant.class_struct_union.field_list = &u_fld;
    gcc_jit_type *jit_union = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&union_tp, &jit_union));
    a_constant c_agg_u;
    std::memset(&c_agg_u, 0, sizeof(c_agg_u));
    c_agg_u.kind = ck_aggregate; c_agg_u.type = &union_tp; c_agg_u.variant.aggregate.first_constant = &c_i1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_u, jit_union, &rv));

    /* L2402-2445: 1-field struct pointer wrapping (via base & via field & non-record) + pointer/pointer & scalar/pointer casts */
    gcc_jit_field *wp_jf1 = gcc_jit_context_new_field(ctx, NULL, jit_iptr, "p");
    gcc_jit_type *jit_wrap_iptr = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_fg_wrap_ip", 1, &wp_jf1));
    gcc_jit_field *wp_jf2 = gcc_jit_context_new_field(ctx, NULL, jit_cptr, "cp");
    gcc_jit_type *jit_wrap_cptr = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_fg_wrap_cp", 1, &wp_jf2));

    a_field wf_ip, wf_cp, wf_notype;
    std::memset(&wf_ip, 0, sizeof(wf_ip)); wf_ip.type = &iptr_tp;
    std::memset(&wf_cp, 0, sizeof(wf_cp)); wf_cp.type = &cptr_tp;
    std::memset(&wf_notype, 0, sizeof(wf_notype)); wf_notype.type = NULL;

    a_type wrap_ip_tp, wrap_cp_tp, wrap_empty_tp, wrap_notype_tp;
    std::memset(&wrap_ip_tp, 0, sizeof(wrap_ip_tp));
    wrap_ip_tp.kind = tk_struct; wrap_ip_tp.size = 8; wrap_ip_tp.variant.class_struct_union.field_list = &wf_ip;
    GCC_GEN_BE_CHECK(expr_test_map_type(&wrap_ip_tp, jit_wrap_iptr));

    std::memset(&wrap_cp_tp, 0, sizeof(wrap_cp_tp));
    wrap_cp_tp.kind = tk_struct; wrap_cp_tp.size = 8; wrap_cp_tp.variant.class_struct_union.field_list = &wf_cp;
    GCC_GEN_BE_CHECK(expr_test_map_type(&wrap_cp_tp, jit_wrap_cptr));

    std::memset(&wrap_empty_tp, 0, sizeof(wrap_empty_tp));
    wrap_empty_tp.kind = tk_struct; wrap_empty_tp.size = 8; wrap_empty_tp.variant.class_struct_union.field_list = NULL;
    GCC_GEN_BE_CHECK(expr_test_map_type(&wrap_empty_tp, jit_wrap_iptr));

    std::memset(&wrap_notype_tp, 0, sizeof(wrap_notype_tp));
    wrap_notype_tp.kind = tk_struct; wrap_notype_tp.size = 8; wrap_notype_tp.variant.class_struct_union.field_list = &wf_notype;
    GCC_GEN_BE_CHECK(expr_test_map_type(&wrap_notype_tp, jit_wrap_iptr));

    /* Outer struct with base `wrap_ip_tp` AND fields `wrap_ip_tp`, `wrap_cp_tp`, `wrap_empty_tp`, `wrap_notype_tp`, `int_tp` (mapped to jit_wrap_iptr) */
    a_type fake_int_wrap_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&fake_int_wrap_tp, ik_int, 8));
    GCC_GEN_BE_CHECK(expr_test_map_type(&fake_int_wrap_tp, jit_wrap_iptr));

    gcc_jit_field *ow_jf[6] = {
        gcc_jit_context_new_field(ctx, NULL, jit_wrap_iptr, "b0"),
        gcc_jit_context_new_field(ctx, NULL, jit_wrap_iptr, "f0"),
        gcc_jit_context_new_field(ctx, NULL, jit_wrap_cptr, "f1"),
        gcc_jit_context_new_field(ctx, NULL, jit_wrap_iptr, "f2"),
        gcc_jit_context_new_field(ctx, NULL, jit_wrap_iptr, "f3"),
        gcc_jit_context_new_field(ctx, NULL, jit_wrap_iptr, "f4")
    };
    gcc_jit_type *jit_outer_wrap = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_fg_outer_wrap", 6, ow_jf));

    a_base_class ow_bc;
    std::memset(&ow_bc, 0, sizeof(ow_bc)); ow_bc.type = &wrap_ip_tp; ow_bc.direct = TRUE;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &ow_bc, ow_jf[0]));
    a_class_type_supplement ow_sup;
    std::memset(&ow_sup, 0, sizeof(ow_sup)); ow_sup.direct_base_classes = &ow_bc;

    a_field ow_f[5];
    std::memset(ow_f, 0, sizeof(ow_f));
    ow_f[0].type = &wrap_ip_tp; ow_f[1].type = &wrap_cp_tp; ow_f[2].type = &wrap_empty_tp;
    ow_f[3].type = &wrap_notype_tp; ow_f[4].type = &fake_int_wrap_tp;
    for (int k = 0; k < 5; ++k) {
        if (k < 4) ow_f[k].next = &ow_f[k + 1];
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &ow_f[k], ow_jf[k + 1]));
    }
    a_type outer_wrap_tp;
    std::memset(&outer_wrap_tp, 0, sizeof(outer_wrap_tp));
    outer_wrap_tp.kind = tk_struct; outer_wrap_tp.size = 48;
    outer_wrap_tp.variant.class_struct_union.extra_info = &ow_sup;
    outer_wrap_tp.variant.class_struct_union.field_list = &ow_f[0];
    GCC_GEN_BE_CHECK(expr_test_map_type(&outer_wrap_tp, jit_outer_wrap));

    a_constant c_str_els[6];
    for (int k = 0; k < 6; ++k) {
        std::memset(&c_str_els[k], 0, sizeof(c_str_els[k]));
        c_str_els[k].kind = ck_string;
        c_str_els[k].variant.string.value = "hi";
        c_str_els[k].variant.string.length = 3;
        if (k < 5) c_str_els[k].next = &c_str_els[k + 1];
    }
    /* Note: for b_cur (`ow_bc`), `b_cur` only matches `val_c->kind == ck_aggregate` unless we wrap c_str_els[0] in a 1-elem untyped ck_aggregate! */
    a_constant c_b0_agg;
    std::memset(&c_b0_agg, 0, sizeof(c_b0_agg));
    c_b0_agg.kind = ck_aggregate; c_b0_agg.type = NULL;
    c_b0_agg.variant.aggregate.first_constant = &c_str_els[0];
    c_str_els[0].next = NULL;
    c_b0_agg.next = &c_str_els[1];

    a_constant c_outer_wrap;
    std::memset(&c_outer_wrap, 0, sizeof(c_outer_wrap));
    c_outer_wrap.kind = ck_aggregate; c_outer_wrap.type = &outer_wrap_tp;
    c_outer_wrap.variant.aggregate.first_constant = &c_b0_agg;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_outer_wrap, jit_outer_wrap, &rv));

    /* Also hit con_is_rec == FALSE on 1-field struct pointer wrap (`con->type = NULL`) */
    a_constant c_str_single, c_untyped_wrap;
    std::memset(&c_str_single, 0, sizeof(c_str_single));
    c_str_single.kind = ck_string; c_str_single.variant.string.value = "s"; c_str_single.variant.string.length = 2;
    std::memset(&c_untyped_wrap, 0, sizeof(c_untyped_wrap));
    c_untyped_wrap.kind = ck_aggregate; c_untyped_wrap.type = NULL;
    c_untyped_wrap.variant.aggregate.first_constant = &c_str_single;
    gcc_jit_field *uw_jf = gcc_jit_context_new_field(ctx, NULL, jit_wrap_iptr, "u0");
    gcc_jit_type *jit_uw_outer = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_fg_uw_outer", 1, &uw_jf));
    c_str_single.type = &wrap_ip_tp; /* sets f_type = jit_wrap_iptr while con_tp == NULL! */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_untyped_wrap, jit_uw_outer, &rv));

    /* L2402, L2408-2410, L2435-2441:
       - pointer f_type (`int*`) with 1-elem untyped ck_aggregate (base!=NULL to prevent L2402 unwrap) yielding `const char*` -> L2410!
       - pointer f_type (`int*`) with empty ck_aggregate (L2402:24 False) and 2-elem ck_aggregate (L2402:66 False)
       - pointer f_type (`int*`) with 1-elem untyped ck_aggregate (base!=NULL) yielding `int` -> L2436-2437!
       - scalar f_type (`long`) with `ck_string` -> L2435 True (vals[idx] pointer, f_type long)!
       - scalar f_type (`long`) with empty ck_aggregate & 2-elem ck_aggregate -> L2438-2439 False! */
    gcc_jit_field *pm_jf[6] = {
        gcc_jit_context_new_field(ctx, NULL, jit_iptr, "p0"),
        gcc_jit_context_new_field(ctx, NULL, jit_iptr, "p1"),
        gcc_jit_context_new_field(ctx, NULL, jit_iptr, "p2"),
        gcc_jit_context_new_field(ctx, NULL, jit_iptr, "p3"),
        gcc_jit_context_new_field(ctx, NULL, jit_long, "l0"),
        gcc_jit_context_new_field(ctx, NULL, jit_long, "l1")
    };
    gcc_jit_type *jit_pm_st = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_fg_pm_st", 6, pm_jf));
    a_field pm_f[6];
    std::memset(pm_f, 0, sizeof(pm_f));
    pm_f[0].type = &iptr_tp; pm_f[1].type = &iptr_tp; pm_f[2].type = &iptr_tp;
    pm_f[3].type = &iptr_tp; pm_f[4].type = &long_tp; pm_f[5].type = &long_tp;
    for (int k = 0; k < 6; ++k) {
        if (k < 5) pm_f[k].next = &pm_f[k + 1];
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &pm_f[k], pm_jf[k]));
    }
    a_type pm_edg_st;
    std::memset(&pm_edg_st, 0, sizeof(pm_edg_st));
    pm_edg_st.kind = tk_struct; pm_edg_st.size = 48;
    pm_edg_st.variant.class_struct_union.field_list = &pm_f[0];
    GCC_GEN_BE_CHECK(expr_test_map_type(&pm_edg_st, jit_pm_st));

    a_constant c_cstr_leaf, c_int_leaf, c_two_a, c_two_b, ca0, ca1, ca2, ca3, ca4, ca5, c_pm_agg;
    std::memset(&c_cstr_leaf, 0, sizeof(c_cstr_leaf));
    c_cstr_leaf.kind = ck_string; c_cstr_leaf.type = &cptr_tp; c_cstr_leaf.variant.string.value = "p"; c_cstr_leaf.variant.string.length = 2;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_int_leaf, &int_tp, 5));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_two_a, &int_tp, 1));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_two_b, &int_tp, 2));
    c_two_a.next = &c_two_b;

    /* ca0: untyped ck_aggregate with base!=NULL & c_cstr_leaf -> returns const char* != int* -> hits L2410! */
    std::memset(&ca0, 0, sizeof(ca0));
    ca0.kind = ck_aggregate; ca0.variant.aggregate.field_or_base.base = &ow_bc; ca0.variant.aggregate.first_constant = &c_cstr_leaf;
    /* ca1: empty ck_aggregate for pointer field -> L2402:24 False */
    std::memset(&ca1, 0, sizeof(ca1));
    ca1.kind = ck_aggregate; ca1.type = &int_tp; ca1.variant.aggregate.first_constant = NULL;
    /* ca2: 2-element ck_aggregate for pointer field -> L2402:66 False */
    std::memset(&ca2, 0, sizeof(ca2));
    ca2.kind = ck_aggregate; ca2.type = &int_tp; ca2.variant.aggregate.first_constant = &c_two_a;
    /* ca3: untyped ck_aggregate with base!=NULL & c_int_leaf -> returns int != int* -> hits L2436-2437! */
    std::memset(&ca3, 0, sizeof(ca3));
    ca3.kind = ck_aggregate; ca3.variant.aggregate.field_or_base.base = &ow_bc; ca3.variant.aggregate.first_constant = &c_int_leaf;
    /* ca4: ck_string for long field -> vals[4] is const char*, f_type is long -> hits L2435 True! */
    std::memset(&ca4, 0, sizeof(ca4));
    ca4.kind = ck_string; ca4.variant.string.value = "x"; ca4.variant.string.length = 2;
    /* ca5: 2-element ck_aggregate for long field -> L2439 False; then also test empty ck_aggregate for long field -> L2438:51 False */
    std::memset(&ca5, 0, sizeof(ca5));
    ca5.kind = ck_aggregate; ca5.type = &int_tp; ca5.variant.aggregate.first_constant = &c_two_a;
    ca0.next = &ca1; ca1.next = &ca2; ca2.next = &ca3; ca3.next = &ca4; ca4.next = &ca5;

    std::memset(&c_pm_agg, 0, sizeof(c_pm_agg));
    c_pm_agg.kind = ck_aggregate; c_pm_agg.type = &pm_edg_st;
    c_pm_agg.variant.aggregate.first_constant = &ca0;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_pm_agg, jit_pm_st, &rv));

    ca5.variant.aggregate.first_constant = NULL; /* L2438:51 False */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_pm_agg, jit_pm_st, &rv));

    /* L2469:28 False: con_tp is tk_struct mapped to non-struct jit_int */
    a_type nonst_mapped_tp;
    std::memset(&nonst_mapped_tp, 0, sizeof(nonst_mapped_tp));
    nonst_mapped_tp.kind = tk_struct; nonst_mapped_tp.size = 4;
    GCC_GEN_BE_CHECK(expr_test_map_type(&nonst_mapped_tp, jit_int));
    a_constant c_nonst_agg;
    std::memset(&c_nonst_agg, 0, sizeof(c_nonst_agg));
    c_nonst_agg.kind = ck_aggregate; c_nonst_agg.type = &nonst_mapped_tp;
    c_nonst_agg.variant.aggregate.first_constant = &c_i1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_nonst_agg, jit_int, &rv));

    /* L2478-2494: single-base struct constructor edge cases */
    gcc_jit_field *sb2_f[2] = {
        gcc_jit_context_new_field(ctx, NULL, jit_int, "x"),
        gcc_jit_context_new_field(ctx, NULL, jit_long, "y")
    };
    gcc_jit_type *jit_sb2_base = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_fg_sb2_base", 2, sb2_f));
    gcc_jit_field *sb_outer_f = gcc_jit_context_new_field(ctx, NULL, jit_sb2_base, "base");
    gcc_jit_type *jit_sb_outer = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_fg_sb_outer", 1, &sb_outer_f));

    a_type sb_base_tp, sb_derived_tp;
    std::memset(&sb_base_tp, 0, sizeof(sb_base_tp));
    sb_base_tp.kind = tk_struct; sb_base_tp.size = 16;
    GCC_GEN_BE_CHECK(expr_test_map_type(&sb_base_tp, jit_sb2_base));

    a_base_class sb_bc;
    std::memset(&sb_bc, 0, sizeof(sb_bc)); sb_bc.type = &sb_base_tp; sb_bc.direct = TRUE;
    a_class_type_supplement sb_sup;
    std::memset(&sb_sup, 0, sizeof(sb_sup)); sb_sup.direct_base_classes = &sb_bc;
    std::memset(&sb_derived_tp, 0, sizeof(sb_derived_tp));
    sb_derived_tp.kind = tk_struct; sb_derived_tp.size = 16;
    sb_derived_tp.variant.class_struct_union.extra_info = &sb_sup;
    GCC_GEN_BE_CHECK(expr_test_map_type(&sb_derived_tp, jit_sb_outer));

    /* 1) count==2 with matching first field type jit_int (L2488:33 False) and NULL second val (L2485:29 False) */
    a_constant sb_c0, sb_c1, sb_agg;
    GCC_GEN_BE_CHECK(expr_init_int_const(&sb_c0, &int_tp, 10));
    std::memset(&sb_c1, 0, sizeof(sb_c1));
    sb_c1.kind = ck_void; sb_c1.type = &void_tp;
    sb_c0.next = &sb_c1;
    std::memset(&sb_agg, 0, sizeof(sb_agg));
    sb_agg.kind = ck_aggregate; sb_agg.type = &sb_derived_tp;
    sb_agg.variant.aggregate.first_constant = &sb_c0;
    g_expr_mock.force_zero_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&sb_agg, jit_sb_outer, &rv));
    /* 2) vals[0] == NULL with count==2 -> L2480:21 False */
    sb_c1.next = &sb_c0; sb_c0.next = NULL;
    sb_agg.variant.aggregate.first_constant = &sb_c1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&sb_agg, jit_sb_outer, &rv));
    g_expr_mock.force_zero_null = FALSE;
    sb_c1.next = NULL;

    /* 3) count==1 vs 2-field b0_type -> L2479:21 False */
    sb_agg.variant.aggregate.first_constant = &sb_c0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&sb_agg, jit_sb_outer, &rv));

    /* 4) vals[0] already has type b0_type (with 1-field b0_type) -> L2480:32 False */
    GCC_GEN_BE_CHECK(expr_test_map_type(&sb_base_tp, g_expr_mock.one_field_struct_type));
    a_constant sb_already_base;
    std::memset(&sb_already_base, 0, sizeof(sb_already_base));
    sb_already_base.kind = ck_aggregate; sb_already_base.type = &sb_base_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sb_bc, sb_outer_f));
    sb_agg.variant.aggregate.first_constant = &sb_already_base;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&sb_agg, jit_sb_outer, &rv));

    /* 5) b0_type mapped to jit_int (L2478:32 False) and NULL (L2478:21 False) */
    GCC_GEN_BE_CHECK(expr_test_map_type(&sb_base_tp, jit_int));
    sb_agg.variant.aggregate.first_constant = &sb_c0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&sb_agg, jit_sb_outer, &rv));
    GCC_GEN_BE_CHECK(expr_test_map_type(&sb_base_tp, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&sb_agg, jit_sb_outer, &rv));

    /* L2641:39 (128-bit ck_integer with expected_type == NULL),
       L2677:17 & L2677:51 (val==0 with expected_type = jit_dbl and jit_void),
       L2713 (ck_string array OOM sweep) */
    a_constant c_u128, c_zero_con, c_str_arr;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_u128, &u128_tp, 9));
    g_expr_mock.int128_lo = 9ULL; g_expr_mock.int128_hi = 0ULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_u128, NULL, &rv));

    GCC_GEN_BE_CHECK(expr_init_int_const(&c_zero_con, &int_tp, 0));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_zero_con, jit_dbl, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_zero_con, jit_void, &rv));

    a_type ch_tp, ch_arr_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&ch_tp, ik_char, 1));
    std::memset(&ch_arr_tp, 0, sizeof(ch_arr_tp));
    ch_arr_tp.kind = tk_array; ch_arr_tp.size = 4;
    ch_arr_tp.variant.array.element_type = &ch_tp;
    ch_arr_tp.variant.array.variant.number_of_elements = 4;
    gcc_jit_type *jit_ch_arr = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&ch_arr_tp, &jit_ch_arr));
    std::memset(&c_str_arr, 0, sizeof(c_str_arr));
    c_str_arr.kind = ck_string; c_str_arr.type = &ch_arr_tp;
    c_str_arr.variant.string.value = "ab"; c_str_arr.variant.string.length = 3;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_str_arr, jit_ch_arr, &rv));

    /* L5941, L5946, L5954, L5955, L6007, L6015, L6056: float, decimal & fixed-point OOM sweeps */
    a_type dec32_tp;
    GCC_GEN_BE_CHECK(expr_init_float_type(&dec32_tp, fk_double, 8));
    a_constant c_dec;
    std::memset(&c_dec, 0, sizeof(c_dec));
    c_dec.kind = ck_float; c_dec.type = &dec32_tp;
    g_expr_mock.host_fp_val = 1.5;
    GCC_GEN_BE_CHECK(expr_sweep_const_oom(&c_dec, NULL, &rv));

    gcc_jit_rvalue *u32_a = gcc_jit_context_new_rvalue_from_int(ctx, jit_u32, 10);
    gcc_jit_rvalue *u32_b = gcc_jit_context_new_rvalue_from_int(ctx, jit_u32, 5);
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 32, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_decimal_float_op('+', 4, u32_a, u32_b, &rv);
    }));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 32, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_fixed_point_op('+', 15, TRUE, u32_a, u32_b, &rv);
    }));

    /* Additional precision branches:
       - L977: 4-bit `dip->kind = (a_dynamic_init_kind)14` -> `default: break;`
       - L377, L380: `coerce_call_arg_if_needed` with `tk_struct` formal param + OOM & force_coerce_arg_err
       - L1932 & L1938:9: `safe_ptr_or_int_cast` with `void_t` rvalue OOM & int-to-struct cast
       - L2200: `lower_address_constant` non-zero offset with `expected_type == NULL`
       - L77:87: `tk_union` containing 1-field struct `wrap_ip_tp` initialized from `ck_string`
       - L2265:28, L2268:21, L2300:58, L2304:21, L2306:25, L2335:38, L2372:60, L2417:32, L2429
       - L5959: `gcc_gen_be_lower_float_constant` with `force_lower_type_err` */
    dip_unk.kind = (a_dynamic_init_kind)14;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(&dip_unk, lv_i, &rv));

    /* L377 & L380: `pt_arg.type = &st_at` (`tk_struct`) on `r_callee` */
    pt_arg.type = &st_at;
    g_expr_mock.needs_abi_coercion = TRUE;
    g_expr_mock.coerced_abi_type = jit_long;
    e_rcallee.next = &e_vstat;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_rcallee));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));
    e_rcallee.next = NULL;
    GCC_GEN_BE_CHECK(expr_test_reset_mock());

    /* L1932: void-typed `gcc_jit_rvalue *` (`new_call` on `j_vfn`) cast to `int_tp` via `mock_builtin_rval` */
    a_routine r_bvoid;
    std::memset(&r_bvoid, 0, sizeof(r_bvoid));
    r_bvoid.type = &vfn_tp; r_bvoid.source_corresp.name = "__builtin_void_mock";
    an_expr_node e_rbvoid;
    std::memset(&e_rbvoid, 0, sizeof(e_rbvoid));
    e_rbvoid.kind = enk_routine; e_rbvoid.type = &vfn_tp; e_rbvoid.variant.routine.ptr = &r_bvoid;
    g_expr_mock.mock_builtin_rval = gcc_jit_context_new_call(ctx, NULL, j_vfn, 0, NULL);
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_rbvoid));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));
    g_expr_mock.mock_builtin_rval = NULL;

    /* L1938:9 False: `int` cast to `two_st_tp` (`dest_type` is struct) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_cast, &two_st_tp, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* abk_variable with offset 1 and `con->type = NULL`, `expected_type = NULL` */
    c_addr_sf.type = NULL; c_addr_sf.variant.address.offset = 1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_addr_sf, NULL, &rv));

    /* L77:87 True: `tk_union` whose first field is 1-field struct `wrap_ip_tp` initialized from `ck_string` */
    a_field u_wrap_fld;
    std::memset(&u_wrap_fld, 0, sizeof(u_wrap_fld)); u_wrap_fld.type = &wrap_ip_tp;
    a_type u_wrap_tp;
    std::memset(&u_wrap_tp, 0, sizeof(u_wrap_tp));
    u_wrap_tp.kind = tk_union; u_wrap_tp.size = 8; u_wrap_tp.variant.class_struct_union.field_list = &u_wrap_fld;
    gcc_jit_field *uw_u_jf = gcc_jit_context_new_field(ctx, NULL, jit_wrap_iptr, "uw");
    gcc_jit_type *jit_u_wrap = gcc_jit_context_new_union_type(ctx, NULL, "_fg_u_wrap", 1, &uw_u_jf);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &u_wrap_fld, uw_u_jf));
    GCC_GEN_BE_CHECK(expr_test_map_type(&u_wrap_tp, jit_u_wrap));
    c_str_single.type = NULL;
    c_agg_u.type = &u_wrap_tp; c_agg_u.variant.aggregate.first_constant = &c_str_single;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_u, jit_u_wrap, &rv));

    /* L2429: pass `c_str_single` directly for `ow_bc` (`b_cur != NULL`) on `outer_wrap_tp` */
    c_str_single.next = &c_str_els[1];
    c_outer_wrap.variant.aggregate.first_constant = &c_str_single;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_outer_wrap, jit_outer_wrap, &rv));
    c_str_single.next = NULL;

    /* L2265:28 False (`is_optimized_empty_base` with non-aggregate `c_i1`) & L2268:21 False (`b_cur->type == NULL`) */
    a_base_class bc_opt, bc_nullt;
    std::memset(&bc_opt, 0, sizeof(bc_opt)); bc_opt.direct = TRUE; bc_opt.is_optimized_empty_base = TRUE; bc_opt.type = &empty_st_tp;
    bc_opt.next = &bc_nullt;
    std::memset(&bc_nullt, 0, sizeof(bc_nullt)); bc_nullt.direct = TRUE; bc_nullt.type = NULL;
    a_class_type_supplement sup_opt;
    std::memset(&sup_opt, 0, sizeof(sup_opt)); sup_opt.direct_base_classes = &bc_opt;
    a_type st_opt_tp;
    std::memset(&st_opt_tp, 0, sizeof(st_opt_tp));
    st_opt_tp.kind = tk_struct; st_opt_tp.size = 8; st_opt_tp.variant.class_struct_union.extra_info = &sup_opt;
    GCC_GEN_BE_CHECK(expr_test_map_type(&st_opt_tp, g_expr_mock.pmf_struct_type));
    a_constant c_opt1, c_opt2, c_agg_opt;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_opt1, &int_tp, 1));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_opt2, &int_tp, 2));
    c_opt1.next = &c_opt2;
    std::memset(&c_agg_opt, 0, sizeof(c_agg_opt));
    c_agg_opt.kind = ck_aggregate; c_agg_opt.type = &st_opt_tp; c_agg_opt.variant.aggregate.first_constant = &c_opt1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_opt, g_expr_mock.pmf_struct_type, &rv));

    /* L2300:58 False (unmapped ck_aggregate without base), L2304:21 False (uncached base), L2306:25 False (cached base with type==NULL),
       L2417:32 False (2-field struct `f_type` mismatch) */
    a_base_class bc_uncached, bc_cached_notype;
    std::memset(&bc_uncached, 0, sizeof(bc_uncached)); bc_uncached.type = &int_tp;
    std::memset(&bc_cached_notype, 0, sizeof(bc_cached_notype)); bc_cached_notype.type = NULL;
    gcc_jit_field *desig3_fs[3] = {
        gcc_jit_context_new_field(ctx, NULL, jit_int, "d0"),
        gcc_jit_context_new_field(ctx, NULL, jit_int, "d1"),
        gcc_jit_context_new_field(ctx, NULL, jit_int, "d2")
    };
    gcc_jit_type *jit_desig3 = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_fg_desig3", 3, desig3_fs));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bc_cached_notype, desig3_fs[2]));
    a_constant c_sub_nobase, c_sub_unc, c_sub_notype, c_agg_desig_bc;
    std::memset(&c_sub_nobase, 0, sizeof(c_sub_nobase)); c_sub_nobase.kind = ck_aggregate; c_sub_nobase.type = &int_tp;
    std::memset(&c_sub_unc, 0, sizeof(c_sub_unc)); c_sub_unc.kind = ck_aggregate; c_sub_unc.variant.aggregate.field_or_base.base = &bc_uncached;
    std::memset(&c_sub_notype, 0, sizeof(c_sub_notype)); c_sub_notype.kind = ck_aggregate; c_sub_notype.variant.aggregate.field_or_base.base = &bc_cached_notype;
    c_sub_nobase.next = &c_sub_unc; c_sub_unc.next = &c_sub_notype;
    std::memset(&c_agg_desig_bc, 0, sizeof(c_agg_desig_bc));
    c_agg_desig_bc.kind = ck_aggregate; c_agg_desig_bc.type = NULL;
    c_agg_desig_bc.variant.aggregate.first_constant = &c_sub_nobase;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_desig_bc, jit_desig3, &rv));

    /* L2417:32 False: field `f` has 2-field struct `two_st_tp` initialized with `ck_integer` */
    a_field f_2st;
    std::memset(&f_2st, 0, sizeof(f_2st)); f_2st.type = &two_st_tp;
    a_type outer_2st_tp;
    std::memset(&outer_2st_tp, 0, sizeof(outer_2st_tp));
    outer_2st_tp.kind = tk_struct; outer_2st_tp.size = 16; outer_2st_tp.variant.class_struct_union.field_list = &f_2st;
    gcc_jit_field *o2_jf = gcc_jit_context_new_field(ctx, NULL, g_expr_mock.pmf_struct_type, "m");
    gcc_jit_type *jit_o2 = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "_fg_o2", 1, &o2_jf));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &f_2st, o2_jf));
    GCC_GEN_BE_CHECK(expr_test_map_type(&outer_2st_tp, jit_o2));
    a_constant c_agg_o2;
    std::memset(&c_agg_o2, 0, sizeof(c_agg_o2));
    c_agg_o2.kind = ck_aggregate; c_agg_o2.type = &outer_2st_tp; c_agg_o2.variant.aggregate.first_constant = &c_i1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_o2, jit_o2, &rv));

    /* L2335:38 False (`unit_sz == 8UL`) & L2372:60 False (`next_f` non-bitfield `is_bit_field==FALSE && bit_size==0`) */
    a_field bf_64, f_plain_after;
    std::memset(&bf_64, 0, sizeof(bf_64));
    bf_64.type = &long_tp; bf_64.is_bit_field = TRUE; bf_64.bit_size = 8; bf_64.next = &f_plain_after;
    std::memset(&f_plain_after, 0, sizeof(f_plain_after));
    f_plain_after.type = &int_tp; f_plain_after.is_bit_field = FALSE; f_plain_after.bit_size = 0;
    a_type st_bf64;
    std::memset(&st_bf64, 0, sizeof(st_bf64));
    st_bf64.kind = tk_struct; st_bf64.size = 16; st_bf64.variant.class_struct_union.field_list = &bf_64;
    GCC_GEN_BE_CHECK(expr_test_map_type(&st_bf64, g_expr_mock.pmf_struct_type));
    c_agg_opt.type = &st_bf64;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_opt, g_expr_mock.pmf_struct_type, &rv));

    /* L5959 True: `c_dec` with `expected_type == NULL` and `force_lower_type_err` */
    a_type dec32_unc1 = dec32_tp, dec32_unc2 = dec32_tp;
    g_expr_mock.force_lower_type_err = GCC_GEN_BE_ERROR_OOM;
    c_dec.type = &dec32_unc1;
    if (gcc_gen_be_lower_constant_rvalue(&c_dec, NULL, &rv) != GCC_GEN_BE_ERROR_OOM) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    c_dec.type = &dec32_unc2;
    if (gcc_gen_be_lower_float_constant(&c_dec, NULL, &rv) != GCC_GEN_BE_ERROR_OOM) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_lower_type_err = GCC_GEN_BE_SUCCESS;

    /* L377 & L380: `reconcile_call_arg_rvalue` via `eok_call` passing struct `e_vstat` to `jit_long` param `j_callee`! */
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_callee, j_callee));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_stat, lv_st_at));
    g_expr_mock.needs_abi_coercion = FALSE;
    e_rcallee.next = &e_vstat;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_rcallee));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));
    g_expr_mock.force_coerce_rval_err = GCC_GEN_BE_ERROR_OOM;
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_OOM) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_coerce_rval_err = GCC_GEN_BE_SUCCESS;
    e_rcallee.next = NULL;

    /* L2678 True: `ck_integer` (`val == 0`) with `expected_type = jit_void` and `force_zero_err = OOM` */
    g_expr_mock.force_zero_err = GCC_GEN_BE_ERROR_OOM;
    if (gcc_gen_be_lower_constant_rvalue(&c_zero_con, jit_void, &rv) != GCC_GEN_BE_ERROR_OOM) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_zero_err = GCC_GEN_BE_SUCCESS;

    /* Close final 6 aggregate constant branch conditions:
       - L2412:32 False (`f_type` is 2-field struct `pmf_struct_type` != 1)
       - L2437:32 False (`val_c->kind == ck_integer` != `ck_aggregate`)
       - L2437:63 False (`val_c->kind == ck_aggregate` with `first_constant == NULL`)
       - L2438:32 False (`val_c->kind == ck_aggregate` with 2 constants `c_ba1 -> c_ba2`)
       - L2428:37 False (`vals[idx]` already has `sub_f0_type == jit_iptr`)
       - L2464:21 False (`con_tp != NULL` and `con_tp->kind == tk_integer` with `expected_type = one_field_struct_type`)
       - L2485:25 False (`b0->type == NULL` mapped via `expr_test_map_type(NULL, one_field_struct_type)`) */
    a_type two_fld_sub_tp, outer_2f_tp;
    std::memset(&two_fld_sub_tp, 0, sizeof(two_fld_sub_tp));
    two_fld_sub_tp.kind = tk_struct; two_fld_sub_tp.size = 16;
    GCC_GEN_BE_CHECK(expr_test_map_type(&two_fld_sub_tp, g_expr_mock.pmf_struct_type));
    a_field outer_2f_fld;
    std::memset(&outer_2f_fld, 0, sizeof(outer_2f_fld)); outer_2f_fld.type = &two_fld_sub_tp;
    gcc_jit_field *j_outer_2f_fld = gcc_jit_context_new_field(ctx, NULL, g_expr_mock.pmf_struct_type, "m2");
    gcc_jit_type *jit_outer_2f = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_fg_outer_2f", 1, &j_outer_2f_fld));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &outer_2f_fld, j_outer_2f_fld));
    std::memset(&outer_2f_tp, 0, sizeof(outer_2f_tp));
    outer_2f_tp.kind = tk_struct; outer_2f_tp.size = 16;
    outer_2f_tp.variant.class_struct_union.field_list = &outer_2f_fld;
    GCC_GEN_BE_CHECK(expr_test_map_type(&outer_2f_tp, jit_outer_2f));

    a_type dbl_tp;
    GCC_GEN_BE_CHECK(expr_init_float_type(&dbl_tp, fk_double, 8));
    a_constant c_outer_2f, c_sub_empty, c_sub_two, c_flt1_2f, c_flt2_2f, c_pdm_2f;
    std::memset(&c_flt1_2f, 0, sizeof(c_flt1_2f)); c_flt1_2f.kind = ck_float; c_flt1_2f.type = &dbl_tp;
    std::memset(&c_flt2_2f, 0, sizeof(c_flt2_2f)); c_flt2_2f.kind = ck_float; c_flt2_2f.type = &dbl_tp;
    std::memset(&c_pdm_2f, 0, sizeof(c_pdm_2f)); c_pdm_2f.kind = ck_ptr_to_member; c_pdm_2f.type = &long_tp;
    std::memset(&c_outer_2f, 0, sizeof(c_outer_2f));
    c_outer_2f.kind = ck_aggregate; c_outer_2f.type = &outer_2f_tp;
    c_outer_2f.variant.aggregate.first_constant = &c_pdm_2f;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_outer_2f, jit_outer_2f, &rv)); /* L2412:32 False & L2437:32 False! */

    std::memset(&c_sub_empty, 0, sizeof(c_sub_empty));
    c_sub_empty.kind = ck_aggregate; c_sub_empty.type = &empty_st_tp;
    c_outer_2f.variant.aggregate.first_constant = &c_sub_empty;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_outer_2f, jit_outer_2f, &rv)); /* L2437:63 False! */

    a_field cplx_f0, cplx_f1;
    std::memset(&cplx_f0, 0, sizeof(cplx_f0)); cplx_f0.type = &dbl_tp; cplx_f0.next = &cplx_f1;
    std::memset(&cplx_f1, 0, sizeof(cplx_f1)); cplx_f1.type = &dbl_tp;
    gcc_jit_struct *j_cplx_s = gcc_jit_type_is_struct(g_expr_mock.cplx_struct_type);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &cplx_f0, gcc_jit_struct_get_field(j_cplx_s, 0)));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &cplx_f1, gcc_jit_struct_get_field(j_cplx_s, 1)));
    a_type cplx_2f_tp;
    std::memset(&cplx_2f_tp, 0, sizeof(cplx_2f_tp));
    cplx_2f_tp.kind = tk_struct; cplx_2f_tp.size = 16;
    cplx_2f_tp.variant.class_struct_union.field_list = &cplx_f0;
    GCC_GEN_BE_CHECK(expr_test_map_type(&cplx_2f_tp, g_expr_mock.cplx_struct_type));

    std::memset(&c_sub_two, 0, sizeof(c_sub_two));
    c_sub_two.kind = ck_aggregate; c_sub_two.type = &cplx_2f_tp;
    c_flt1_2f.next = &c_flt2_2f; c_flt2_2f.next = NULL;
    c_sub_two.variant.aggregate.first_constant = &c_flt1_2f;
    c_outer_2f.variant.aggregate.first_constant = &c_sub_two;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_outer_2f, jit_outer_2f, &rv)); /* L2438:32 False! */
    c_flt1_2f.next = NULL;

    /* L2428:37 False: 1-field struct `wrap_ccp_tp` whose field lowers to `GCC_JIT_TYPE_CONST_CHAR_PTR`,
       initialized from `ck_string` (`vals[0]` has exact type `GCC_JIT_TYPE_CONST_CHAR_PTR == sub_f0_type`!) */
    gcc_jit_type *jit_ccptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CONST_CHAR_PTR);
    a_type ccptr_tp, wrap_ccp_tp, outer_ccp_tp;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ccptr_tp, &int_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_test_map_type(&ccptr_tp, jit_ccptr));
    a_field w_ccp_f, outer_ccp_f;
    std::memset(&w_ccp_f, 0, sizeof(w_ccp_f)); w_ccp_f.type = &ccptr_tp;
    gcc_jit_field *j_w_ccp_f = gcc_jit_context_new_field(ctx, NULL, jit_ccptr, "ccp");
    gcc_jit_type *jit_wrap_ccp = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_fg_wrap_ccp", 1, &j_w_ccp_f));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &w_ccp_f, j_w_ccp_f));
    std::memset(&wrap_ccp_tp, 0, sizeof(wrap_ccp_tp));
    wrap_ccp_tp.kind = tk_struct; wrap_ccp_tp.size = 8;
    wrap_ccp_tp.variant.class_struct_union.field_list = &w_ccp_f;
    GCC_GEN_BE_CHECK(expr_test_map_type(&wrap_ccp_tp, jit_wrap_ccp));

    std::memset(&outer_ccp_f, 0, sizeof(outer_ccp_f)); outer_ccp_f.type = &wrap_ccp_tp;
    gcc_jit_field *j_outer_ccp_f = gcc_jit_context_new_field(ctx, NULL, jit_wrap_ccp, "m_ccp");
    gcc_jit_type *jit_outer_ccp = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_fg_outer_ccp", 1, &j_outer_ccp_f));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &outer_ccp_f, j_outer_ccp_f));
    std::memset(&outer_ccp_tp, 0, sizeof(outer_ccp_tp));
    outer_ccp_tp.kind = tk_struct; outer_ccp_tp.size = 8;
    outer_ccp_tp.variant.class_struct_union.field_list = &outer_ccp_f;
    GCC_GEN_BE_CHECK(expr_test_map_type(&outer_ccp_tp, jit_outer_ccp));

    a_constant c_str_ccp, c_outer_ccp;
    std::memset(&c_str_ccp, 0, sizeof(c_str_ccp));
    c_str_ccp.kind = ck_string; c_str_ccp.variant.string.value = "ok"; c_str_ccp.variant.string.length = 3;
    std::memset(&c_outer_ccp, 0, sizeof(c_outer_ccp));
    c_outer_ccp.kind = ck_aggregate; c_outer_ccp.type = &outer_ccp_tp;
    c_outer_ccp.variant.aggregate.first_constant = &c_str_ccp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_outer_ccp, jit_outer_ccp, &rv)); /* L2428:37 False! */

    /* L2464:21 False: `con->type == NULL` (`!con_tp` True) and `expected_type == NULL` (`struct_type == NULL` -> L2464:21 False!) */
    a_constant c_agg_null_st;
    std::memset(&c_agg_null_st, 0, sizeof(c_agg_null_st));
    c_agg_null_st.kind = ck_aggregate; c_agg_null_st.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_null_st, NULL, &rv));

    /* L2262:28 False (`b_cur` uncached, `is_optimized_empty_base == FALSE`, `val_c == ck_aggregate`),
       L2265:21 False (`b_cur->type == NULL` with `val_c == ck_aggregate`),
       L2447:37 True (`b_cur->next` is virtual base `is_virtual == TRUE`) */
    a_base_class bc_nonopt, bc_virt_after, bc_null_agg;
    std::memset(&bc_nonopt, 0, sizeof(bc_nonopt)); bc_nonopt.direct = TRUE; bc_nonopt.is_optimized_empty_base = FALSE; bc_nonopt.type = &empty_st_tp;
    bc_nonopt.next = &bc_virt_after;
    std::memset(&bc_virt_after, 0, sizeof(bc_virt_after)); bc_virt_after.direct = TRUE; bc_virt_after.is_virtual = TRUE; bc_virt_after.type = &empty_st_tp;
    bc_virt_after.next = &bc_null_agg;
    std::memset(&bc_null_agg, 0, sizeof(bc_null_agg)); bc_null_agg.direct = TRUE; bc_null_agg.type = NULL;
    sup_opt.direct_base_classes = &bc_nonopt;
    a_constant c_ba1, c_ba2;
    std::memset(&c_ba1, 0, sizeof(c_ba1)); c_ba1.kind = ck_aggregate; c_ba1.type = &empty_st_tp;
    std::memset(&c_ba2, 0, sizeof(c_ba2)); c_ba2.kind = ck_aggregate; c_ba2.type = &empty_st_tp;
    c_ba1.next = &c_ba2;
    c_agg_opt.type = &st_opt_tp; c_agg_opt.variant.aggregate.first_constant = &c_ba1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_opt, g_expr_mock.pmf_struct_type, &rv));

    /* L2414:32 False (`f_type` is 2-field struct `g_expr_mock.pmf_struct_type` != `vals[idx]` `jit_int`),
       L2422:49 True (`b_cur` active with `c_b0_agg.type = &cptr_tp` returning `const char *`!),
       L2422:72 False (`f == NULL` past end of `field_list` on 1-field struct `f_type`),
       L2430:37 False (`vals[idx]` already matches `sub_f0_type = jit_cptr` via `wrap_cp_tp`!),
       L2439-2440 False (`val_c` `ck_integer` into `long`, empty `ck_aggregate` into `long`, 2-elem `ck_aggregate` into `long`),
       L2466:21 False & L2516:23 False (`con_tp = &cptr_tp` with `count == 2` and `count == 1 && vals[0] == NULL`) */
    c_str_els[0].type = &cptr_tp;
    c_b0_agg.type = &cptr_tp;
    c_b0_agg.variant.aggregate.first_constant = &c_str_els[0];
    c_b0_agg.next = &c_str_els[1];
    /* Note: c_str_els[2] feeds `ow_f[1]` (`wrap_cp_tp`, whose `sub_f0_type == jit_cptr == typeof(c_str_els[2])` -> L2430:37 False!) */
    c_outer_wrap.variant.aggregate.first_constant = &c_b0_agg;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_outer_wrap, jit_outer_wrap, &rv));

    /* L2422:72 False: struct with `field_list == NULL` (`b_cur == NULL`, `f == NULL`) whose JIT struct has 1 `jit_wrap_iptr` field */
    a_type st_nof_wrap_tp;
    std::memset(&st_nof_wrap_tp, 0, sizeof(st_nof_wrap_tp));
    st_nof_wrap_tp.kind = tk_struct; st_nof_wrap_tp.size = 8; st_nof_wrap_tp.variant.class_struct_union.field_list = NULL;
    GCC_GEN_BE_CHECK(expr_test_map_type(&st_nof_wrap_tp, jit_uw_outer));
    c_str_single.type = &wrap_ip_tp;
    a_constant c_nof_agg;
    std::memset(&c_nof_agg, 0, sizeof(c_nof_agg));
    c_nof_agg.kind = ck_aggregate; c_nof_agg.type = &st_nof_wrap_tp; c_nof_agg.variant.aggregate.first_constant = &c_str_single;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_nof_agg, jit_uw_outer, &rv));

    /* L2439-2440 False: `long` field initialized from (a) `ck_integer` `int` (`val_c->kind != ck_aggregate`),
       (b) empty `ck_aggregate` (`!first_constant`), (c) 2-elem `ck_aggregate` (`first_constant->next != NULL`)
       where inner `ck_aggregate` has `base != NULL` so L2402 doesn't unwrap it! */
    gcc_jit_field *l3_jf[3] = {
        gcc_jit_context_new_field(ctx, NULL, jit_long, "a"),
        gcc_jit_context_new_field(ctx, NULL, jit_long, "b"),
        gcc_jit_context_new_field(ctx, NULL, jit_long, "c")
    };
    gcc_jit_type *jit_l3 = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "_fg_l3", 3, l3_jf));
    a_field l3_f[3];
    std::memset(l3_f, 0, sizeof(l3_f));
    l3_f[0].type = &long_tp; l3_f[0].next = &l3_f[1];
    l3_f[1].type = &long_tp; l3_f[1].next = &l3_f[2];
    l3_f[2].type = &long_tp;
    for (int k = 0; k < 3; ++k) GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &l3_f[k], l3_jf[k]));
    a_type l3_st_tp;
    std::memset(&l3_st_tp, 0, sizeof(l3_st_tp)); l3_st_tp.kind = tk_struct; l3_st_tp.size = 24;
    l3_st_tp.variant.class_struct_union.field_list = &l3_f[0];
    GCC_GEN_BE_CHECK(expr_test_map_type(&l3_st_tp, jit_l3));

    /* Force `c_i1` to lower as `jit_int` (via 1-elem `cptr_tp`? No, via 1-elem `ck_aggregate` of `short_tp` with `base != NULL`!) */
    a_type short_c_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&short_c_tp, ik_short, 2));
    a_constant c_sh1, c_sh2, la0, la1, la2, c_l3_agg;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_sh1, &short_c_tp, 3));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_sh2, &short_c_tp, 4));
    /* la0: ck_float of float (`fk_float`) into `long` field -> `val_c->kind == ck_float != ck_aggregate` and `vals[0]` is `float != long` -> L2439:32 False! */
    a_type flt_c_tp;
    GCC_GEN_BE_CHECK(expr_init_float_type(&flt_c_tp, fk_float, 4));
    std::memset(&la0, 0, sizeof(la0)); la0.kind = ck_float; la0.type = &flt_c_tp;
    /* la1: empty `ck_aggregate` (`type = &flt_c_tp` -> `vals[1]` is `float != long`, `first_constant == NULL` -> L2439:63 False!) */
    std::memset(&la1, 0, sizeof(la1)); la1.kind = ck_aggregate; la1.type = &flt_c_tp; la1.variant.aggregate.first_constant = NULL;
    /* la2: 2-elem `ck_aggregate` (`type = &flt_c_tp`, `c_sh1 -> c_sh2` -> `vals[2]` is `float != long`, `!next` False -> L2440:32 False!) */
    c_sh1.next = &c_sh2;
    std::memset(&la2, 0, sizeof(la2)); la2.kind = ck_aggregate; la2.type = &flt_c_tp; la2.variant.aggregate.first_constant = &c_sh1;
    la0.next = &la1; la1.next = &la2;
    std::memset(&c_l3_agg, 0, sizeof(c_l3_agg));
    c_l3_agg.kind = ck_aggregate; c_l3_agg.type = &l3_st_tp; c_l3_agg.variant.aggregate.first_constant = &la0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_l3_agg, jit_l3, &rv));
    c_sh1.next = NULL;

    /* L2414:32 False: `f_type` is 2-field struct `g_expr_mock.pmf_struct_type` while `vals[idx]` is `float` (`la0`) */
    c_agg_o2.variant.aggregate.first_constant = &la0;
    la0.next = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_o2, jit_o2, &rv));

    /* L2459:39 False: `tk_union` with `vals[0] == NULL` (`ck_void` + `force_zero_null = TRUE`) */
    g_expr_mock.force_zero_null = TRUE;
    c_agg_u.type = &union_tp; c_agg_u.variant.aggregate.first_constant = &c_void_inner;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_agg_u, jit_union, &rv));
    /* Also L2516:23 False: `con_tp = &flt_c_tp` (`count == 1 && vals[0] == NULL`) */
    la1.variant.aggregate.first_constant = &c_void_inner;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&la1, jit_int, &rv));
    g_expr_mock.force_zero_null = FALSE;

    /* L2486-2490 False: single-base `sb_derived_tp` (`st_fcount == 1`, `b0_type = one_field_struct_type`, `count == 1`):
       - `vals[0]` non-pointer (`la0` float != `one_field_struct_type` -> L2486:25 False)
       - `vals[0]` pointer (`c_str_single`):
         a) `b0->type = NULL` mapped via `b0`? No, `b0->type = &flt_c_tp` mapped to `jit_wrap_iptr` (`b0->type->kind == tk_float` -> L2487:69 False)
         b) `b0->type = &wrap_empty_tp` (`field_list == NULL` -> L2488:25 False)
         c) `b0->type = &wrap_notype_tp` (`bf0->type == NULL` -> L2490:29 False)
         d) `b0->type` `tk_class` (`wrap_cls_tp` -> L2487:39 False, `tk_class` True) */
    GCC_GEN_BE_CHECK(expr_test_map_type(&sb_base_tp, g_expr_mock.one_field_struct_type));
    sb_bc.type = &sb_base_tp;
    sb_agg.variant.aggregate.first_constant = &la0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&sb_agg, jit_sb_outer, &rv)); /* L2486:25 False */

    c_str_single.type = NULL;
    sb_agg.variant.aggregate.first_constant = &c_str_single;
    a_type flt_as_wrap_tp = flt_c_tp;
    GCC_GEN_BE_CHECK(expr_test_map_type(&flt_as_wrap_tp, jit_wrap_iptr));
    sb_bc.type = &flt_as_wrap_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&sb_agg, jit_sb_outer, &rv)); /* L2487:69 False */
    sb_bc.type = &wrap_empty_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&sb_agg, jit_sb_outer, &rv)); /* L2488:25 False */
    sb_bc.type = &wrap_notype_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&sb_agg, jit_sb_outer, &rv)); /* L2490:29 False */
    a_type wrap_cls_tp = wrap_ip_tp;
    wrap_cls_tp.kind = tk_class;
    GCC_GEN_BE_CHECK(expr_test_map_type(&wrap_cls_tp, jit_wrap_iptr));
    sb_bc.type = &wrap_cls_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&sb_agg, jit_sb_outer, &rv)); /* tk_class! */

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises Stage-2 constant and aggregate initializer lowering branches in `src/gcc_gen_be_expr.c`.
 * @details Covers `abi_fn` function cache remapping on `abk_routine` and non-virtual PMFs, string literal
 * temporary alignment when `p_gcc_jit_lvalue_set_alignment` is NULL, `is_lowered_base_class`, unnamed
 * bitfields, word/byte-container bitfield packing, `char[]` string initializers, `ck_init_repeat` in
 * structs and arrays, union constant constructors, single-base/outer struct repacking, GNU vector
 * aggregate constants, and 128-bit integer constant casts.
 * @param[in] ctx Active `libgccjit` context.
 * @param[in] fn Active `libgccjit` test function.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_init_const_stage2_gaps(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp = {}, ch_tp = {}, uch_tp = {}, ull_tp = {}, i128_tp = {}, ptr_tp = {}, void_tp = {};
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&ch_tp, ik_char, 1));
    GCC_GEN_BE_CHECK(expr_init_int_type(&uch_tp, ik_unsigned_char, 1));
    GCC_GEN_BE_CHECK(expr_init_int_type(&ull_tp, ik_unsigned_long_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&i128_tp, ik_int128, 16));
    void_tp.kind = tk_void;
    ptr_tp.kind = tk_pointer; ptr_tp.size = 8; ptr_tp.variant.pointer.type = &ch_tp;

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_uch = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
    gcc_jit_type *jit_vp = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);

    /* 1) L2167..2168 & L3271: abk_routine and non-virtual ck_ptr_to_member with abi_fn in GCC_GEN_BE_CACHE_FUNC */
    {
        a_type rty = {}, pmf_tp = {};
        rty.kind = tk_routine; rty.variant.routine.return_type = &void_tp;
        pmf_tp.kind = tk_ptr_to_member; pmf_tp.size = 16; pmf_tp.variant.ptr_to_member.type = &rty;
        a_routine rout = {}; rout.source_corresp.name = "s2_abi_orig_fn"; rout.type = &rty;
        gcc_jit_function *orig_jfn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "__s2_orig", 0, NULL, 0);
        gcc_jit_function *abi_jfn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "__s2_abi", 0, NULL, 0);

        a_constant c_addr = {}, c_pmf = {};
        c_addr.kind = ck_address; c_addr.type = &ptr_tp; c_addr.variant.address.kind = abk_routine; c_addr.variant.address.variant.routine = &rout;
        c_pmf.kind = ck_ptr_to_member; c_pmf.type = &pmf_tp; c_pmf.variant.ptr_to_member.is_function_ptr = TRUE; c_pmf.variant.ptr_to_member.variant.routine = &rout;

        g_expr_mock.mock_decl_func = orig_jfn;
        gcc_jit_rvalue *rv = NULL;
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &rout, orig_jfn));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, orig_jfn, abi_jfn));
        for (int b = 0; b < 25; ++b) {
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t ce = gcc_gen_be_lower_constant_rvalue(&c_addr, jit_vp, &rv);
            g_gcc_be_oom_countdown = -1;
            if (ce != GCC_GEN_BE_SUCCESS && ce != GCC_GEN_BE_ERROR_OOM) return ce;
        }
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 25, [&]() noexcept -> gcc_gen_be_error_t {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &rout, orig_jfn));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, orig_jfn, abi_jfn));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_addr, jit_vp, &rv));
            return gcc_gen_be_lower_constant_rvalue(&c_pmf, g_expr_mock.pmf_struct_type, &rv);
        }));
        /* L2009:31 False: safe_ptr_or_int_cast with dest_type == GCC_JIT_TYPE_VOID */
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &rout, orig_jfn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_addr, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), &rv));
        g_expr_mock.mock_decl_func = NULL;
    }

    /* 2) L2200: abk_constant string literal offset != 0 with p_gcc_jit_lvalue_set_alignment == NULL */
    {
        auto saved_align_fn = p_gcc_jit_lvalue_set_alignment;
        p_gcc_jit_lvalue_set_alignment = NULL;
        a_constant c_str = {}, c_saddr = {};
        c_str.kind = ck_string; c_str.type = &ptr_tp; c_str.variant.string.value = "hello"; c_str.variant.string.length = 6;
        c_saddr.kind = ck_address; c_saddr.type = &ptr_tp; c_saddr.variant.address.kind = abk_constant;
        c_saddr.variant.address.variant.constant = &c_str; c_saddr.variant.address.offset = 1;
        gcc_jit_rvalue *rv = NULL;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_saddr, jit_vp, &rv));
        p_gcc_jit_lvalue_set_alignment = saved_align_fn;
    }

    /* 3) L2369..2372, L2397, L2448..2450: is_lowered_base_class, unnamed non-zero bitfield, and uncached base val_ut != sub_ut */
    {
        a_field f_lbc = {}; f_lbc.source_corresp.name = "lbc_f"; f_lbc.type = &int_tp; f_lbc.is_lowered_base_class = TRUE;
        a_type st_lbc = {}; st_lbc.kind = tk_struct; st_lbc.size = 4; st_lbc.source_corresp.name = "S2LbcSt"; st_lbc.variant.class_struct_union.field_list = &f_lbc;
        a_constant ci1 = {}, ca_lbc = {};
        ci1.kind = ck_integer; ci1.type = &int_tp;
        ca_lbc.kind = ck_aggregate; ca_lbc.type = &st_lbc; ca_lbc.variant.aggregate.first_constant = &ci1;
        gcc_jit_rvalue *rv = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&ca_lbc, NULL, &rv));

        /* Uncached direct base where val_c->type != b_cur->type advances b_cur to field_list */
        a_type base_st = {}, other_st = {}, der_st = {};
        a_field other_f = {}, der_f = {}, unnamed_bf = {};
        other_f.source_corresp.name = "of"; other_f.type = &int_tp;
        unnamed_bf.is_bit_field = TRUE; unnamed_bf.bit_size = 4; unnamed_bf.source_corresp.name = NULL; unnamed_bf.type = &int_tp; unnamed_bf.next = &der_f;
        der_f.source_corresp.name = "df"; der_f.type = &other_st;
        base_st.kind = tk_struct; base_st.size = 1; base_st.source_corresp.name = "S2EmptyBase";
        other_st.kind = tk_struct; other_st.size = 4; other_st.source_corresp.name = "S2OtherAgg"; other_st.variant.class_struct_union.field_list = &other_f;
        a_base_class bc = {}; bc.direct = TRUE; bc.type = &base_st;
        a_class_type_supplement csup = {}; csup.direct_base_classes = &bc;
        der_st.kind = tk_struct; der_st.size = 8; der_st.source_corresp.name = "S2DerMismatch";
        der_st.variant.class_struct_union.extra_info = &csup; der_st.variant.class_struct_union.field_list = &unnamed_bf;
        a_constant c_sub_int = {}, c_sub_agg = {}, c_der_agg = {};
        c_sub_int.kind = ck_integer; c_sub_int.type = &int_tp;
        c_sub_agg.kind = ck_aggregate; c_sub_agg.type = &other_st; c_sub_agg.variant.aggregate.first_constant = &c_sub_int;
        c_der_agg.kind = ck_aggregate; c_der_agg.type = &der_st; c_der_agg.variant.aggregate.first_constant = &c_sub_agg;
        gcc_jit_type *emp0_early = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2Emp0Early", 0, NULL));
        base_st.size = 0; /* empty base + 0-field struct -> skip_unmapped_empty == TRUE and val_ut != sub_ut -> retry_subobject! */
        GCC_GEN_BE_CHECK(expr_test_map_type(&der_st, emp0_early));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_der_agg, emp0_early, &rv));
        g_expr_mock.num_type_overrides = 0;

        /* L2470:42 True (!is_bit_field && bit_size > 0), L2472:21 False (jit_f == NULL), L2479:76 False (struct_type non-struct) */
        a_field f_bs_only = {}; f_bs_only.source_corresp.name = "fbso"; f_bs_only.type = &uch_tp; f_bs_only.is_bit_field = FALSE; f_bs_only.bit_size = 4;
        a_type st_bso = {}; st_bso.kind = tk_struct; st_bso.size = 1; st_bso.source_corresp.name = "S2BsoSt"; st_bso.variant.class_struct_union.field_list = &f_bs_only;
        a_constant c_bso = {}; c_bso.kind = ck_aggregate; c_bso.type = &st_bso; c_bso.variant.aggregate.first_constant = &ci1;
        GCC_GEN_BE_CHECK(expr_test_map_type(&st_bso, emp0_early)); /* jit_f == NULL */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_bso, emp0_early, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_bso, jit_uch, &rv)); /* struct_type == jit_uch (non-struct) */
        g_expr_mock.num_type_overrides = 0;
    }

    /* 4) L2479..2619: Word-container & byte-container bitfield constant packing + OOM sweeps */
    {
        /* (a) Word container: cached_bf_ft != NULL, straddling 4B->8B (con_tp->size 8 vs 4), unnamed/0-width skip, non-bf/non-int stop, OOM */
        a_field wf1 = {}, wf_z = {}, wf_un = {}, wf2 = {}, wf_reg = {};
        wf1.source_corresp.name = "wf1"; wf1.type = &int_tp; wf1.is_bit_field = TRUE; wf1.offset = 0; wf1.offset_bit_remainder = 28; wf1.bit_size = 8; wf1.next = &wf_z;
        wf_z.source_corresp.name = "wfz"; wf_z.type = &int_tp; wf_z.is_bit_field = TRUE; wf_z.bit_size = 0; wf_z.next = &wf_un;
        wf_un.source_corresp.name = NULL; wf_un.type = &int_tp; wf_un.is_bit_field = TRUE; wf_un.bit_size = 2; wf_un.next = &wf2;
        wf2.source_corresp.name = "wf2"; wf2.type = &int_tp; wf2.is_bit_field = TRUE; wf2.offset = 4; wf2.offset_bit_remainder = 6; wf2.bit_size = 4; wf2.next = &wf_reg;
        wf_reg.source_corresp.name = "wfr"; wf_reg.type = &int_tp; wf_reg.offset = 8;

        a_type wst8 = {}, wst4 = {};
        wst8.kind = tk_struct; wst8.size = 12; wst8.source_corresp.name = "S2WordBf8"; wst8.variant.class_struct_union.field_list = &wf1;
        wst4.kind = tk_struct; wst4.size = 4; wst4.source_corresp.name = "S2WordBf4"; wst4.variant.class_struct_union.field_list = &wf1;

        gcc_jit_field *wj_f0 = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG), "wj0");
        gcc_jit_field *wj_f1 = gcc_jit_context_new_field(ctx, NULL, jit_int, "wj1");
        gcc_jit_field *wj_arr[2] = {wj_f0, wj_f1};
        gcc_jit_struct *wj_st = gcc_jit_context_new_struct_type(ctx, NULL, "S2WordBfJit", 2, wj_arr);
        gcc_jit_type *wj_tp = gcc_jit_struct_as_type(wj_st);

        a_constant wc1 = {}, wc2 = {}, wc_flt = {}, wagg = {};
        wc1.kind = ck_integer; wc1.type = &int_tp; wc1.next = &wc2;
        wc2.kind = ck_integer; wc2.type = &int_tp; wc2.next = &wc_flt;
        wc_flt.kind = ck_float; wc_flt.type = &int_tp;
        wagg.kind = ck_aggregate; wagg.type = &wst8; wagg.variant.aggregate.first_constant = &wc1;

        gcc_jit_rvalue *rv = NULL;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 45, [&]() noexcept -> gcc_gen_be_error_t {
            g_expr_mock.num_type_overrides = 0;
            GCC_GEN_BE_CHECK(expr_test_map_type(&wst8, wj_tp));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &wf1, wj_f0));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &wf_reg, wj_f1));
            return gcc_gen_be_lower_constant_rvalue(&wagg, wj_tp, &rv);
        }));
        /* cached_bf_ft != NULL & wst4 (f->offset + 8 > con_tp->size) & non-ck_integer next_c */
        GCC_GEN_BE_CHECK(expr_test_map_type(&wst4, wj_tp));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &wf1, wj_f0));
        wagg.type = &wst4;
        wc1.next = &wc_flt;
        wf2.next = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&wagg, wj_tp, &rv));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, wj_f0, jit_int));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&wagg, wj_tp, &rv));
        g_expr_mock.num_type_overrides = 0;

        /* (b) Byte container: multi-byte pack, unnamed/0-width skip, bit_size==0 (span_bytes==0), >32B span, duplicate target_f, foreign jit_f, + OOM */
        gcc_jit_field *bj_fields[4];
        for (int bi = 0; bi < 4; ++bi) {
            char bnm[16]; std::snprintf(bnm, sizeof(bnm), "bj_%d", bi);
            bj_fields[bi] = gcc_jit_context_new_field(ctx, NULL, jit_uch, bnm);
        }
        gcc_jit_struct *bj_st = gcc_jit_context_new_struct_type(ctx, NULL, "S2ByteBfJit", 4, bj_fields);
        gcc_jit_type *bj_tp = gcc_jit_struct_as_type(bj_st);
        gcc_jit_field *foreign_jf = gcc_jit_context_new_field(ctx, NULL, jit_uch, "foreign_bf");
        gcc_jit_struct *dummy_fst = gcc_jit_context_new_struct_type(ctx, NULL, "S2ForeignSt", 1, &foreign_jf);
        if (!dummy_fst) return GCC_GEN_BE_ERROR_INTERNAL;

        a_field bf1 = {}, bf_z = {}, bf_un = {}, bf2 = {}, bf_sep = {}, bf_reg = {};
        bf1.source_corresp.name = "bf1"; bf1.type = &uch_tp; bf1.is_bit_field = TRUE; bf1.offset = 0; bf1.offset_bit_remainder = 4; bf1.bit_size = 12; bf1.next = &bf_z;
        bf_z.source_corresp.name = "bfz"; bf_z.type = &uch_tp; bf_z.is_bit_field = TRUE; bf_z.bit_size = 0; bf_z.next = &bf_un;
        bf_un.source_corresp.name = NULL; bf_un.type = &uch_tp; bf_un.is_bit_field = TRUE; bf_un.bit_size = 2; bf_un.next = &bf2;
        bf2.source_corresp.name = "bf2"; bf2.type = &uch_tp; bf2.is_bit_field = TRUE; bf2.offset = 2; bf2.offset_bit_remainder = 0; bf2.bit_size = 4; bf2.next = &bf_reg;
        bf_reg.source_corresp.name = "bfr"; bf_reg.type = &uch_tp; bf_reg.offset = 3; bf_reg.next = &bf_sep;
        bf_sep.source_corresp.name = "bfs"; bf_sep.type = &uch_tp; bf_sep.is_bit_field = TRUE; bf_sep.offset = 3; bf_sep.bit_size = 4;

        a_type bst = {}; bst.kind = tk_struct; bst.size = 4; bst.source_corresp.name = "S2ByteBfEdg"; bst.variant.class_struct_union.field_list = &bf1;
        a_constant bc1 = {}, bc2 = {}, bc3 = {}, bc4 = {}, bagg = {};
        bc1.kind = ck_integer; bc1.type = &int_tp; bc1.next = &bc2;
        bc2.kind = ck_integer; bc2.type = &int_tp; bc2.next = &bc3;
        bc3.kind = ck_integer; bc3.type = &uch_tp; bc3.next = &bc4;
        bc4.kind = ck_integer; bc4.type = &int_tp;
        bagg.kind = ck_aggregate; bagg.type = &bst; bagg.variant.aggregate.first_constant = &bc1;

        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 55, [&]() noexcept -> gcc_gen_be_error_t {
            g_expr_mock.num_type_overrides = 0;
            g_expr_mock.num_const_ints = 0;
            GCC_GEN_BE_CHECK(expr_test_map_const_int(&bc1, 0xABCL));
            GCC_GEN_BE_CHECK(expr_test_map_const_int(&bc2, 0x5L));
            GCC_GEN_BE_CHECK(expr_test_map_type(&bst, bj_tp));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf1, bj_fields[0]));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf_reg, bj_fields[3]));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf_sep, bj_fields[3])); /* duplicate target_f at idx - 1! */
            return gcc_gen_be_lower_constant_rvalue(&bagg, bj_tp, &rv);
        }));

        /* bit_size == 0 via designator (span_bytes == 0), >32B offset (dst_bit/8 >= 32 & span_bytes > 32), foreign_jf */
        a_field bf_des_z = {}, bf_hg0 = {}, bf_huge = {};
        bf_des_z.source_corresp.name = "bfdz"; bf_des_z.type = &uch_tp; bf_des_z.is_bit_field = TRUE; bf_des_z.bit_size = 0;
        bf_hg0.source_corresp.name = "bfh0"; bf_hg0.type = &uch_tp; bf_hg0.is_bit_field = TRUE; bf_hg0.offset = 0; bf_hg0.bit_size = 1; bf_hg0.next = &bf_huge;
        bf_huge.source_corresp.name = "bfhg"; bf_huge.type = &uch_tp; bf_huge.is_bit_field = TRUE; bf_huge.offset = 33; bf_huge.bit_size = 8;
        a_constant cd1 = {}, cv1 = {}, cd2 = {}, cv2 = {}, cv3 = {}, bagg2 = {};
        cd1.kind = ck_designator; cd1.variant.designator.is_field_designator = TRUE; cd1.variant.designator.variant.field = &bf_des_z; cd1.next = &cv1;
        cv1.kind = ck_integer; cv1.type = &int_tp; cv1.next = &cd2;
        cd2.kind = ck_designator; cd2.variant.designator.is_field_designator = TRUE; cd2.variant.designator.variant.field = &bf_hg0; cd2.next = &cv2;
        cv2.kind = ck_integer; cv2.type = &int_tp; cv2.next = &cv3;
        cv3.kind = ck_integer; cv3.type = &int_tp;
        bagg2.kind = ck_aggregate; bagg2.type = &bst; bagg2.variant.aggregate.first_constant = &cd1;
        GCC_GEN_BE_CHECK(expr_test_map_type(&bst, bj_tp));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf_des_z, bj_fields[0]));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf_hg0, foreign_jf));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&bagg2, bj_tp, &rv));
        /* Byte-container stop on next_c == NULL and next_c->kind == ck_float (2553:85 & 2553:95) */
        a_constant bc_end = {}, bc_f1 = {}, bc_flt2 = {}, bagg_stop = {};
        bc_end.kind = ck_integer; bc_end.type = &int_tp; bc_end.next = NULL;
        bc_f1.kind = ck_integer; bc_f1.type = &int_tp; bc_f1.next = &bc_flt2;
        bc_flt2.kind = ck_float; bc_flt2.type = &int_tp;
        bagg_stop.kind = ck_aggregate; bagg_stop.type = &bst; bagg_stop.variant.aggregate.first_constant = &bc_end;
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf1, bj_fields[0]));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf2, bj_fields[0]));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&bagg_stop, bj_tp, &rv));
        bagg_stop.variant.aggregate.first_constant = &bc_f1;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&bagg_stop, bj_tp, &rv));
        g_expr_mock.num_type_overrides = 0;
    }

    /* 5) L2636..2729: Direct ck_string in char[]/int[]/0-len struct fields, ck_init_repeat in struct, and tk_union constants */
    {
        a_type carr4 = {}, carr0 = {}, iarr2 = {}, farr2 = {}, flt_el_tp = {}, s_str_st = {};
        flt_el_tp.kind = tk_float; flt_el_tp.size = 4; flt_el_tp.variant.float_kind = fk_float;
        carr4.kind = tk_array; carr4.size = 4; carr4.variant.array.element_type = &ch_tp; carr4.variant.array.variant.number_of_elements = 4;
        carr0.kind = tk_array; carr0.size = 0; carr0.variant.array.element_type = &ch_tp; carr0.variant.array.variant.number_of_elements = 0;
        iarr2.kind = tk_array; iarr2.size = 8; iarr2.variant.array.element_type = &int_tp; iarr2.variant.array.variant.number_of_elements = 2;
        farr2.kind = tk_array; farr2.size = 8; farr2.variant.array.element_type = &flt_el_tp; farr2.variant.array.variant.number_of_elements = 0;
        a_field sf_a = {}, sf_b = {}, sf_c = {}, sf_d = {}, sf_e = {};
        sf_a.source_corresp.name = "sa"; sf_a.type = &carr4; sf_a.next = &sf_b;
        sf_b.source_corresp.name = "sb"; sf_b.type = &carr4; sf_b.next = &sf_c;
        sf_c.source_corresp.name = "sc"; sf_c.type = &carr0; sf_c.next = &sf_d;
        sf_d.source_corresp.name = "sd"; sf_d.type = &iarr2; sf_d.next = &sf_e;
        sf_e.source_corresp.name = "se"; sf_e.type = &farr2;
        s_str_st.kind = tk_struct; s_str_st.size = 24; s_str_st.source_corresp.name = "S2CharArrSt"; s_str_st.variant.class_struct_union.field_list = &sf_a;

        a_constant ds1 = {}, ds2 = {}, ds3 = {}, ds4 = {}, ds5 = {}, sagg = {};
        ds1.kind = ck_string; ds1.type = &carr4; ds1.variant.string.value = "hi"; ds1.variant.string.length = 2; ds1.next = &ds2;
        ds2.kind = ck_string; ds2.type = &carr4; ds2.variant.string.value = NULL; ds2.variant.string.length = 2; ds2.next = &ds3;
        ds3.kind = ck_string; ds3.type = &carr0; ds3.variant.string.value = ""; ds3.variant.string.length = 0; ds3.next = &ds4;
        ds4.kind = ck_string; ds4.type = &iarr2; ds4.variant.string.value = "abcdefgh"; ds4.variant.string.length = 5; ds4.next = &ds5;
        ds5.kind = ck_string; ds5.type = &farr2; ds5.variant.string.value = "xy"; ds5.variant.string.length = 2;
        sagg.kind = ck_aggregate; sagg.type = &s_str_st; sagg.variant.aggregate.first_constant = &ds1;

        gcc_jit_rvalue *rv = NULL;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 85, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_lower_constant_rvalue(&sagg, NULL, &rv);
        }));

        /* ck_init_repeat in struct (count > 1, count == 0, null rep_elem, and overflow past fields) + OOM */
        a_field rf1 = {}, rf2 = {};
        rf1.source_corresp.name = "rf1"; rf1.type = &int_tp; rf1.next = &rf2;
        rf2.source_corresp.name = "rf2"; rf2.type = &int_tp;
        a_type rst = {}; rst.kind = tk_struct; rst.size = 8; rst.source_corresp.name = "S2RepSt"; rst.variant.class_struct_union.field_list = &rf1;
        a_constant rc_elem = {}, rc_null_rep = {}, rc_rep0 = {}, rc_rep3 = {}, ragg = {};
        rc_elem.kind = ck_integer; rc_elem.type = &int_tp;
        rc_null_rep.kind = ck_init_repeat; rc_null_rep.variant.init_repeat.constant = NULL; rc_null_rep.variant.init_repeat.count = 1; rc_null_rep.next = &rc_rep0;
        rc_rep0.kind = ck_init_repeat; rc_rep0.variant.init_repeat.constant = &rc_elem; rc_rep0.variant.init_repeat.count = 0; rc_rep0.next = &rc_rep3;
        rc_rep3.kind = ck_init_repeat; rc_rep3.variant.init_repeat.constant = &rc_elem; rc_rep3.variant.init_repeat.count = 3;
        ragg.kind = ck_aggregate; ragg.type = &rst; ragg.variant.aggregate.first_constant = &rc_null_rep;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 35, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_lower_constant_rvalue(&ragg, NULL, &rv);
        }));

        /* tk_union constant: empty union, matching/mismatched field type, non-union struct_type (2717 False), vals[0]==NULL (2718 False) */
        a_field uf = {}; uf.source_corresp.name = "uf0"; uf.type = &ull_tp;
        a_type utp = {}; utp.kind = tk_union; utp.size = 8; utp.source_corresp.name = "S2ConstUnion"; utp.variant.class_struct_union.field_list = &uf;
        gcc_jit_field *ujf = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG), "ujf0");
        gcc_jit_type *ujt = gcc_jit_context_new_union_type(ctx, NULL, "S2ConstUnionJit", 1, &ujf);
        a_constant uc_emp = {}, uc_int = {}, uc_ull = {}, uc_des = {}, uagg = {};
        uc_emp.kind = ck_aggregate; uc_emp.type = &utp; uc_emp.variant.aggregate.first_constant = NULL;
        uc_int.kind = ck_integer; uc_int.type = &int_tp;
        uc_ull.kind = ck_integer; uc_ull.type = &ull_tp;
        uc_des.kind = ck_designator; uc_des.variant.designator.is_field_designator = TRUE; uc_des.variant.designator.variant.field = &uf;
        GCC_GEN_BE_CHECK(expr_test_map_type(&utp, ujt));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &uf, ujf));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&uc_emp, ujt, &rv));
        uagg.kind = ck_aggregate; uagg.type = &utp; uagg.variant.aggregate.first_constant = &uc_int;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&uagg, NULL, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&uagg, jit_int, &rv)); /* struct_type non-union -> 2717:34 False */
        uagg.variant.aggregate.first_constant = &uc_des;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&uagg, ujt, &rv)); /* vals[0] == NULL -> 2718:45 False */
        uagg.variant.aggregate.first_constant = &uc_ull;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&uagg, ujt, &rv));
        g_expr_mock.num_type_overrides = 0;
    }

    /* 6) L2735..2892: Single-base wrapper struct, needs_trimmed_lval_init, cur_vt != exact_ft struct/ptr repack + OOM */
    {
        /* Base struct B2 (2 int fields, non-empty EDG field_list so skip_unmapped_empty is FALSE!) */
        gcc_jit_field *b2_f0 = gcc_jit_context_new_field(ctx, NULL, jit_int, "b2_0");
        gcc_jit_field *b2_f1 = gcc_jit_context_new_field(ctx, NULL, jit_int, "b2_1");
        gcc_jit_field *b2_fs[2] = {b2_f0, b2_f1};
        gcc_jit_struct *b2_jst = gcc_jit_context_new_struct_type(ctx, NULL, "S2Base2Jit", 2, b2_fs);
        gcc_jit_type *b2_jtp = gcc_jit_struct_as_type(b2_jst);
        gcc_jit_field *der2_f0 = gcc_jit_context_new_field(ctx, NULL, b2_jtp, "__b_b2");
        gcc_jit_struct *der2_jst = gcc_jit_context_new_struct_type(ctx, NULL, "S2Der2Jit", 1, &der2_f0);
        gcc_jit_type *der2_jtp = gcc_jit_struct_as_type(der2_jst);

        a_field b2_ef0 = {}; b2_ef0.source_corresp.name = "bef0"; b2_ef0.type = &int_tp;
        a_type b2_edg = {}, der2_edg = {};
        b2_edg.kind = tk_struct; b2_edg.size = 8; b2_edg.source_corresp.name = "S2Base2Edg"; b2_edg.variant.class_struct_union.field_list = &b2_ef0;
        a_base_class bc2 = {}; bc2.direct = TRUE; bc2.type = &b2_edg;
        a_class_type_supplement dsup2 = {}; dsup2.direct_base_classes = &bc2;
        der2_edg.kind = tk_struct; der2_edg.size = 8; der2_edg.source_corresp.name = "S2Der2Edg";
        der2_edg.variant.class_struct_union.extra_info = &dsup2;

        a_constant c_v0 = {}, c_v1 = {}, c_d2 = {};
        c_v0.kind = ck_integer; c_v0.type = &int_tp; c_v0.next = &c_v1;
        c_v1.kind = ck_integer; c_v1.type = &int_tp;
        c_d2.kind = ck_aggregate; c_d2.type = &der2_edg; c_d2.variant.aggregate.first_constant = &c_v0;
        gcc_jit_rvalue *rv = NULL;
        GCC_GEN_BE_CHECK(expr_test_map_type(&der2_edg, der2_jtp));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_d2, der2_jtp, &rv));

        /* count == 1 with pointer-to-B1 dereference, already-matching B1 (2739:32 False), int val, non-record b0->type, non-struct b0_type */
        gcc_jit_field *b1_f0 = gcc_jit_context_new_field(ctx, NULL, jit_int, "b1_0");
        gcc_jit_struct *b1_jst = gcc_jit_context_new_struct_type(ctx, NULL, "S2Base1Jit", 1, &b1_f0);
        gcc_jit_type *b1_jtp = gcc_jit_struct_as_type(b1_jst);
        gcc_jit_field *der1_f0 = gcc_jit_context_new_field(ctx, NULL, b1_jtp, "__b_b1");
        gcc_jit_struct *der1_jst = gcc_jit_context_new_struct_type(ctx, NULL, "S2Der1Jit", 1, &der1_f0);
        gcc_jit_type *der1_jtp = gcc_jit_struct_as_type(der1_jst);

        a_variable b1_gv = {}; b1_gv.source_corresp.name = "s2_b1_gv"; b1_gv.type = &b2_edg;
        gcc_jit_lvalue *b1_glv = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, b1_jtp, "s2_b1_glv");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &b1_gv, b1_glv));
        a_constant c_ptr_b1 = {}, c_sub_b1 = {}, c_d1 = {};
        c_ptr_b1.kind = ck_address; c_ptr_b1.type = &ptr_tp; c_ptr_b1.variant.address.kind = abk_variable; c_ptr_b1.variant.address.variant.variable = &b1_gv;
        c_v0.next = NULL;
        c_sub_b1.kind = ck_aggregate; c_sub_b1.type = &b2_edg; c_sub_b1.variant.aggregate.first_constant = &c_v0;
        c_d1.kind = ck_aggregate; c_d1.type = &der2_edg; c_d1.variant.aggregate.first_constant = &c_ptr_b1;
        g_expr_mock.num_type_overrides = 0;
        GCC_GEN_BE_CHECK(expr_test_map_type(&der2_edg, der1_jtp));
        GCC_GEN_BE_CHECK(expr_test_map_type(&b2_edg, b1_jtp));
        GCC_GEN_BE_CHECK(expr_test_map_type(&ptr_tp, gcc_jit_type_get_pointer(b1_jtp)));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_d1, der1_jtp, &rv));
        c_d1.variant.aggregate.first_constant = &c_sub_b1; /* vals[0] already b1_jtp -> 2739:32 False! */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_d1, der1_jtp, &rv));
        c_d1.variant.aggregate.first_constant = &c_ptr_b1;
        bc2.type = &int_tp; /* 2746 False */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_d1, der1_jtp, &rv));
        bc2.type = &b2_edg;
        c_d1.variant.aggregate.first_constant = &c_v0; /* 2745 False */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_d1, der1_jtp, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_d1, der2_jtp, &rv));
        g_expr_mock.num_type_overrides = 0;
        GCC_GEN_BE_CHECK(expr_test_map_type(&der2_edg, g_expr_mock.one_field_struct_type));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_d1, g_expr_mock.one_field_struct_type, &rv));
        g_expr_mock.num_type_overrides = 0;

        /* needs_trimmed_lval_init (2773..2803) and cur_vt != exact_ft ptr/struct adaptation (2815..2846) + OOM */
        gcc_jit_type *carr4_t = gcc_jit_context_new_array_type(ctx, NULL, jit_uch, 4);
        gcc_jit_field *tf_0 = gcc_jit_context_new_field(ctx, NULL, carr4_t, "tf_raw");
        gcc_jit_field *tf_1 = gcc_jit_context_new_field(ctx, NULL, jit_int, "tf_same");
        gcc_jit_field *tf_2 = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), "tf_coerced");
        gcc_jit_field *tf_fs[3] = {tf_0, tf_1, tf_2};
        gcc_jit_type *trim_jtp = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2TrimJit", 3, tf_fs));

        a_field te_0 = {}, te_1 = {}, te_2 = {};
        te_0.source_corresp.name = "te0"; te_0.type = &b2_edg; te_0.next = &te_1;
        te_1.source_corresp.name = "te1"; te_1.type = &int_tp; te_1.next = &te_2;
        te_2.source_corresp.name = "te2"; te_2.type = &int_tp;
        a_type trim_edg = {}; trim_edg.kind = tk_struct; trim_edg.size = 16; trim_edg.source_corresp.name = "S2TrimEdg"; trim_edg.variant.class_struct_union.field_list = &te_0;
        a_constant tc_sub = {}, tc_i1 = {}, tc_i2 = {}, tc_agg = {};
        tc_sub.kind = ck_aggregate; tc_sub.type = &b2_edg; tc_sub.variant.aggregate.first_constant = &c_v0; tc_sub.next = &tc_i1;
        tc_i1.kind = ck_integer; tc_i1.type = &int_tp; tc_i1.next = &tc_i2;
        tc_i2.kind = ck_integer; tc_i2.type = &int_tp;
        tc_agg.kind = ck_aggregate; tc_agg.type = &trim_edg; tc_agg.variant.aggregate.first_constant = &tc_sub;

        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 55, [&]() noexcept -> gcc_gen_be_error_t {
            g_expr_mock.num_type_overrides = 0;
            GCC_GEN_BE_CHECK(expr_test_map_type(&b2_edg, b1_jtp));
            GCC_GEN_BE_CHECK(expr_test_map_type(&trim_edg, trim_jtp));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &te_0, tf_0));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &te_1, tf_1));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &te_2, tf_2));
            return gcc_gen_be_lower_constant_rvalue(&tc_agg, trim_jtp, &rv);
        }));
        /* coerce_rval_return_null == TRUE (2799:29 False) and agg_blk == NULL (2781:21 False) */
        g_expr_mock.coerce_rval_return_null = TRUE;
        g_expr_mock.num_type_overrides = 0;
        GCC_GEN_BE_CHECK(expr_test_map_type(&b2_edg, b1_jtp));
        GCC_GEN_BE_CHECK(expr_test_map_type(&trim_edg, trim_jtp));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &te_0, tf_0));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &te_1, tf_1));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &te_2, tf_2));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&tc_agg, trim_jtp, &rv));
        g_expr_mock.coerce_rval_return_null = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&tc_agg, trim_jtp, &rv));
        g_expr_mock.num_type_overrides = 0;

        /* 2815..2846: field 0 int->void*, field 1 2-field struct->{void*,int} -> 1-field struct / 3-field struct / 0-field struct + OOM */
        gcc_jit_field *emp0_sub_f = gcc_jit_context_new_field(ctx, NULL, jit_vp, "af_ptr");
        gcc_jit_type *emp0_jtp = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2Emp0Jit", 0, NULL));
        gcc_jit_field *alt2_f0 = gcc_jit_context_new_field(ctx, NULL, jit_vp, "a20");
        gcc_jit_field *alt2_f1 = gcc_jit_context_new_field(ctx, NULL, jit_int, "a21");
        gcc_jit_field *alt2_f2 = gcc_jit_context_new_field(ctx, NULL, jit_int, "a22");
        gcc_jit_field *alt3_fs[3] = {alt2_f0, alt2_f1, alt2_f2};
        gcc_jit_type *alt3_jtp = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2Alt3Jit", 3, alt3_fs));
        gcc_jit_field *af_fs[4] = {
            emp0_sub_f,
            gcc_jit_context_new_field(ctx, NULL, b1_jtp, "af_s1"),
            gcc_jit_context_new_field(ctx, NULL, alt3_jtp, "af_s3"),
            gcc_jit_context_new_field(ctx, NULL, emp0_jtp, "af_s0")
        };
        gcc_jit_type *adapt_jtp = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2AdaptJit", 4, af_fs));

        a_field ae_0 = {}, ae_1 = {}, ae_2 = {}, ae_3 = {};
        ae_0.source_corresp.name = "ae0"; ae_0.type = &int_tp; ae_0.next = &ae_1;
        ae_1.source_corresp.name = "ae1"; ae_1.type = &b2_edg; ae_1.next = &ae_2;
        ae_2.source_corresp.name = "ae2"; ae_2.type = &b2_edg; ae_2.next = &ae_3;
        ae_3.source_corresp.name = "ae3"; ae_3.type = &b2_edg;
        a_type adapt_edg = {}; adapt_edg.kind = tk_struct; adapt_edg.size = 32; adapt_edg.source_corresp.name = "S2AdaptEdg"; adapt_edg.variant.class_struct_union.field_list = &ae_0;
        c_v0.next = &c_v1;
        a_constant ac_p = {}, ac_s1 = {}, ac_s2 = {}, ac_s3 = {}, adapt_agg = {};
        ac_p.kind = ck_integer; ac_p.type = &int_tp; ac_p.next = &ac_s1;
        ac_s1.kind = ck_aggregate; ac_s1.type = &b2_edg; ac_s1.variant.aggregate.first_constant = &c_v0; ac_s1.next = &ac_s2;
        ac_s2.kind = ck_aggregate; ac_s2.type = &b2_edg; ac_s2.variant.aggregate.first_constant = &c_v0; ac_s2.next = &ac_s3;
        ac_s3.kind = ck_aggregate; ac_s3.type = &b2_edg; ac_s3.variant.aggregate.first_constant = &c_v0;
        adapt_agg.kind = ck_aggregate; adapt_agg.type = &adapt_edg; adapt_agg.variant.aggregate.first_constant = &ac_p;

        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 85, [&]() noexcept -> gcc_gen_be_error_t {
            g_expr_mock.num_type_overrides = 0;
            GCC_GEN_BE_CHECK(expr_test_map_type(&b2_edg, b2_jtp));
            GCC_GEN_BE_CHECK(expr_test_map_type(&adapt_edg, adapt_jtp));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &b2_ef0, b2_f0));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &ae_0, af_fs[0]));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &ae_1, af_fs[1]));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &ae_2, af_fs[2]));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &ae_3, af_fs[3]));
            return gcc_gen_be_lower_constant_rvalue(&adapt_agg, adapt_jtp, &rv);
        }));
        g_expr_mock.num_type_overrides = 0;

        /* struct_type != type: (i) outer_fc == 1 wrapping struct_type, (ii) outer_fc == 1 not wrapping, (iii) outer_fc 3 > built_fc, (iv) outer_fc == 0 */
        a_field f2_0 = {}, f2_1 = {};
        f2_0.source_corresp.name = "f20"; f2_0.type = &int_tp; f2_0.next = &f2_1;
        f2_1.source_corresp.name = "f21"; f2_1.type = &int_tp;
        a_type st2_edg = {}; st2_edg.kind = tk_struct; st2_edg.size = 8; st2_edg.source_corresp.name = "S2Repack2"; st2_edg.variant.class_struct_union.field_list = &f2_0;
        a_constant c_st2 = {}; c_st2.kind = ck_aggregate; c_st2.type = &st2_edg; c_st2.variant.aggregate.first_constant = &c_v0;

        GCC_GEN_BE_CHECK(expr_test_map_type(&st2_edg, b2_jtp));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &f2_0, b2_f0));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &f2_1, b2_f1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_st2, der2_jtp, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_st2, emp0_jtp, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_st2, alt3_jtp, &rv));
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 45, [&]() noexcept -> gcc_gen_be_error_t {
            g_expr_mock.num_type_overrides = 0;
            GCC_GEN_BE_CHECK(expr_test_map_type(&st2_edg, b2_jtp));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &f2_0, b2_f0));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &f2_1, b2_f1));
            return gcc_gen_be_lower_constant_rvalue(&c_st2, g_expr_mock.one_field_struct_type, &rv);
        }));
        g_expr_mock.num_type_overrides = 0;
    }

    /* 7) L2924..3134: GNU vector aggregate constant, array ck_init_repeat, and 128-bit integer cast */
    {
        a_type zsz_el_tp = {}, flt0_el_tp = {}, vec4_tp = {}, vec0_tp = {}, vec_f0_tp = {}, vec_null_el = {};
        zsz_el_tp.kind = tk_integer; zsz_el_tp.size = 0; zsz_el_tp.variant.integer.int_kind = ik_int;
        flt0_el_tp.kind = tk_float; flt0_el_tp.size = 0; flt0_el_tp.variant.float_kind = fk_float;
        vec4_tp.kind = tk_vector; vec4_tp.size = 16; vec4_tp.variant.vector.element_type = &int_tp;
        vec0_tp.kind = tk_vector; vec0_tp.size = 16; vec0_tp.variant.vector.element_type = &zsz_el_tp;
        vec_f0_tp.kind = tk_vector; vec_f0_tp.size = 16; vec_f0_tp.variant.vector.element_type = &flt0_el_tp;
        vec_null_el.kind = tk_vector; vec_null_el.size = 16; vec_null_el.variant.vector.element_type = NULL;

        a_constant vd = {}, vc0 = {}, vc1 = {}, vagg = {}, vagg0 = {}, vagg_f0 = {}, vagg_nel = {};
        vd.kind = ck_designator; vd.next = &vc0;
        vc0.kind = ck_integer; vc0.type = &int_tp; vc0.next = &vc1;
        vc1.kind = ck_integer; vc1.type = &int_tp;
        vagg.kind = ck_aggregate; vagg.type = &vec4_tp; vagg.variant.aggregate.first_constant = &vd;
        vagg0.kind = ck_aggregate; vagg0.type = &vec0_tp;
        vagg_f0.kind = ck_aggregate; vagg_f0.type = &vec_f0_tp;
        vagg_nel.kind = ck_aggregate; vagg_nel.type = &vec_null_el;

        gcc_jit_rvalue *rv = NULL;
        auto saved_vec_fn = p_gcc_jit_context_new_rvalue_from_vector;
        p_gcc_jit_context_new_rvalue_from_vector = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&vagg, jit_int, &rv));
        GCC_GEN_BE_CHECK(expr_test_map_type(&vec0_tp, jit_int));
        GCC_GEN_BE_CHECK(expr_test_map_type(&vec_f0_tp, jit_int));
        GCC_GEN_BE_CHECK(expr_test_map_type(&vec_null_el, jit_int));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&vagg0, jit_int, &rv)); /* vec_cblk != NULL && n_lanes == 0 -> 2948:28 False */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&vagg_f0, jit_int, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&vagg_nel, jit_int, &rv));
        g_expr_mock.num_type_overrides = 0;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 45, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_lower_constant_rvalue(&vagg, NULL, &rv);
        }));
        /* With p_gcc_jit_context_new_rvalue_from_vector != NULL (+ OOM for 2939 and n_lanes == 0 for 2938:39 False) */
        p_gcc_jit_context_new_rvalue_from_vector = saved_vec_fn;
        gcc_jit_type *v4_jt = gcc_jit_type_get_vector(jit_int, 4);
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 45, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_lower_constant_rvalue(&vagg, v4_jt, &rv);
        }));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&vagg0, jit_int, &rv));

        /* Array ck_init_repeat (null rep_elem, count = 0, count = 3 with 2-elem cap, + OOM) */
        a_type arr2_tp = {}; arr2_tp.kind = tk_array; arr2_tp.size = 8; arr2_tp.variant.array.element_type = &int_tp; arr2_tp.variant.array.variant.number_of_elements = 2;
        a_constant ac_el = {}, ac_null = {}, ac_r0 = {}, ac_r3 = {}, aagg = {};
        ac_el.kind = ck_integer; ac_el.type = &int_tp;
        ac_null.kind = ck_init_repeat; ac_null.variant.init_repeat.constant = NULL; ac_null.variant.init_repeat.count = 1; ac_null.next = &ac_r0;
        ac_r0.kind = ck_init_repeat; ac_r0.variant.init_repeat.constant = &ac_el; ac_r0.variant.init_repeat.count = 0; ac_r0.next = &ac_r3;
        ac_r3.kind = ck_init_repeat; ac_r3.variant.init_repeat.constant = &ac_el; ac_r3.variant.init_repeat.count = 3;
        aagg.kind = ck_aggregate; aagg.type = &arr2_tp; aagg.variant.aggregate.first_constant = &ac_null;
        if (gcc_gen_be_lower_constant_rvalue(&aagg, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
            return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
        }
        aagg.variant.aggregate.first_constant = &ac_r0;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 35, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_lower_constant_rvalue(&aagg, NULL, &rv);
        }));

        /* 128-bit integer constant with expected_type != t128, null expected_type, zero/nonzero lo/hi + OOM */
        a_constant c128 = {}; c128.kind = ck_integer; c128.type = &i128_tp;
        g_expr_mock.int128_lo = 1ULL; g_expr_mock.int128_hi = 2ULL;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 25, [&]() noexcept -> gcc_gen_be_error_t {
            g_expr_mock.shifted_128 = FALSE;
            return gcc_gen_be_lower_constant_rvalue(&c128, jit_vp, &rv);
        }));
        g_expr_mock.int128_lo = 0ULL; g_expr_mock.int128_hi = 0ULL;
        g_expr_mock.shifted_128 = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c128, jit_vp, &rv));
        g_expr_mock.shifted_128 = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c128, jit_int, &rv));
        g_expr_mock.int128_lo = 0ULL; g_expr_mock.int128_hi = 1ULL;
        g_expr_mock.shifted_128 = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c128, jit_vp, &rv));
        GCC_GEN_BE_CHECK(expr_test_map_type(&i128_tp, NULL));
        g_expr_mock.shifted_128 = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c128, NULL, &rv));
        g_expr_mock.num_type_overrides = 0;

        /* Remaining struct/bitfield/trimmed-lvalue constant branches */
        {
            /* 1) skip_unmapped_empty with val_ut != sub_ut -> goto retry_subobject */
            a_type emp_sub_tp = {}; emp_sub_tp.kind = tk_struct; emp_sub_tp.size = 1; emp_sub_tp.source_corresp.name = "S2EmpSub";
            a_field fe_emp = {}, fe_int = {};
            fe_emp.source_corresp.name = "fe_emp"; fe_emp.type = &emp_sub_tp; fe_emp.is_optimized_empty_class = TRUE; fe_emp.next = &fe_int;
            fe_int.source_corresp.name = "fe_int"; fe_int.type = &int_tp;
            a_type st_emp_retry = {}; st_emp_retry.kind = tk_struct; st_emp_retry.size = 4; st_emp_retry.source_corresp.name = "S2EmpRetry";
            st_emp_retry.variant.class_struct_union.field_list = &fe_emp;
            gcc_jit_field *jfe_int = gcc_jit_context_new_field(ctx, NULL, jit_int, "fe_int");
            gcc_jit_struct *jst_emp_retry = gcc_jit_context_new_struct_type(ctx, NULL, "S2EmpRetryJ", 1, &jfe_int);
            GCC_GEN_BE_CHECK(expr_test_map_type(&st_emp_retry, gcc_jit_struct_as_type(jst_emp_retry)));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &fe_int, jfe_int));
            a_constant ce_int = {}, ce_agg = {};
            ce_int.kind = ck_integer; ce_int.type = &int_tp;
            ce_agg.kind = ck_aggregate; ce_agg.type = &st_emp_retry; ce_agg.variant.aggregate.first_constant = &ce_int;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&ce_agg, NULL, &rv));
            g_expr_mock.num_type_overrides = 0;

            /* 2) Bitfield constant edge cases: union uchar bf, null struct_type uchar bf, uncached zero-width byte bf,
                  unit_sz == 0, straddling > con_tp->size, ck_designator (normal/generic/non-field), unnamed bf & diff jit_f */
            gcc_jit_type *jit_uch = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
            a_type uch_tp = {}; GCC_GEN_BE_CHECK(expr_init_int_type(&uch_tp, ik_unsigned_char, 1));
            a_type zsz_int_tp = {}; GCC_GEN_BE_CHECK(expr_init_int_type(&zsz_int_tp, ik_int, 0));

            a_field ubf_f = {}; ubf_f.source_corresp.name = "ubf"; ubf_f.type = &uch_tp; ubf_f.is_bit_field = TRUE; ubf_f.bit_size = 3;
            a_type ubf_un = {}; ubf_un.kind = tk_union; ubf_un.size = 1; ubf_un.source_corresp.name = "S2UbfUn";
            ubf_un.variant.class_struct_union.field_list = &ubf_f;
            gcc_jit_field *jubf_f = gcc_jit_context_new_field(ctx, NULL, jit_uch, "ubf");
            gcc_jit_type *jubf_un = gcc_jit_context_new_union_type(ctx, NULL, "S2UbfUnJ", 1, &jubf_f);
            GCC_GEN_BE_CHECK(expr_test_map_type(&ubf_un, jubf_un));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &ubf_f, jubf_f));
            a_constant cubf_v = {}, cubf_agg = {};
            cubf_v.kind = ck_integer; cubf_v.type = &uch_tp;
            cubf_agg.kind = ck_aggregate; cubf_agg.type = &ubf_un; cubf_agg.variant.aggregate.first_constant = &cubf_v;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&cubf_agg, jubf_un, &rv));
            GCC_GEN_BE_CHECK(expr_test_map_type(&ubf_un, NULL));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&cubf_agg, NULL, &rv));
            g_expr_mock.num_type_overrides = 0;

            /* Uncached byte-bf with bit_size == 0 (2482:63 False & max_bit_end == 0) */
            a_field zbf_f = {}; zbf_f.source_corresp.name = "zbf"; zbf_f.type = &uch_tp; zbf_f.is_bit_field = TRUE; zbf_f.bit_size = 0;
            a_type zbf_st = {}; zbf_st.kind = tk_struct; zbf_st.size = 1; zbf_st.source_corresp.name = "S2ZbfSt";
            zbf_st.variant.class_struct_union.field_list = &zbf_f;
            gcc_jit_field *jzbf_f = gcc_jit_context_new_field(ctx, NULL, jit_uch, "zbf");
            gcc_jit_type *jzbf_st = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2ZbfStJ", 1, &jzbf_f));
            GCC_GEN_BE_CHECK(expr_test_map_type(&zbf_st, jzbf_st));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &zbf_f, jzbf_f));
            a_constant czbf_agg = {}; czbf_agg.kind = ck_aggregate; czbf_agg.type = &zbf_st; czbf_agg.variant.aggregate.first_constant = &cubf_v;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&czbf_agg, jzbf_st, &rv));
            g_expr_mock.num_type_overrides = 0;

            /* unit_sz == 0, unit_sz == 8UL (2482:63 False), con_tp == NULL straddling (2484:21 False), & f->type == NULL (2629:26 False) */
            a_type ul8_tp = {}; GCC_GEN_BE_CHECK(expr_init_int_type(&ul8_tp, ik_unsigned_long, 8));
            a_field sbf_0 = {}, sbf_ul8 = {}, sbf_nulltp = {};
            sbf_0.source_corresp.name = "sbf0"; sbf_0.type = &zsz_int_tp; sbf_0.is_bit_field = TRUE; sbf_0.offset_bit_remainder = 20; sbf_0.bit_size = 20; sbf_0.next = &sbf_ul8;
            sbf_ul8.source_corresp.name = "sbful8"; sbf_ul8.type = &ul8_tp; sbf_ul8.is_bit_field = TRUE; sbf_ul8.offset = 8; sbf_ul8.bit_size = 4; sbf_ul8.next = &sbf_nulltp;
            sbf_nulltp.source_corresp.name = "sbfnt"; sbf_nulltp.type = NULL; sbf_nulltp.offset = 16;
            a_type sbf_st = {}; sbf_st.kind = tk_struct; sbf_st.size = 6; sbf_st.source_corresp.name = "S2Strad6";
            sbf_st.variant.class_struct_union.field_list = &sbf_0;
            gcc_jit_field *jsbf_fs[3] = {
                gcc_jit_context_new_field(ctx, NULL, jit_int, "sbf0"),
                gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG), "sbful8"),
                gcc_jit_context_new_field(ctx, NULL, jit_int, "sbfnt")
            };
            gcc_jit_type *jsbf_st = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2Strad6J", 3, jsbf_fs));
            GCC_GEN_BE_CHECK(expr_test_map_type(&sbf_st, jsbf_st));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sbf_0, jsbf_fs[0]));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sbf_ul8, jsbf_fs[1]));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sbf_nulltp, jsbf_fs[2]));
            a_constant csbf_v0 = {}, csbf_v1 = {}, csbf_v2 = {}, csbf_agg = {};
            csbf_v0.kind = ck_integer; csbf_v0.type = &int_tp; csbf_v0.next = &csbf_v1;
            csbf_v1.kind = ck_integer; csbf_v1.type = &ul8_tp; csbf_v1.next = &csbf_v2;
            csbf_v2.kind = ck_integer; csbf_v2.type = &int_tp;
            csbf_agg.kind = ck_aggregate; csbf_agg.type = &sbf_st; csbf_agg.variant.aggregate.first_constant = &csbf_v0;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&csbf_agg, jsbf_st, &rv));
            /* con_tp == NULL with ck_designator to straddling sbf_0 -> 2484:21 False */
            a_constant cdes_nt = {}, cval_nt = {}, cagg_nt = {};
            cdes_nt.kind = ck_designator; cdes_nt.variant.designator.is_field_designator = TRUE; cdes_nt.variant.designator.variant.field = &sbf_0; cdes_nt.next = &cval_nt;
            cval_nt.kind = ck_integer; cval_nt.type = &int_tp;
            cagg_nt.kind = ck_aggregate; cagg_nt.type = NULL; cagg_nt.variant.aggregate.first_constant = &cdes_nt;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&cagg_nt, jsbf_st, &rv));
            g_expr_mock.num_type_overrides = 0;

            /* ck_designator in bitfield packing (jump backwards for 2520:25 False & 2544:29 True, plus generic & array designators)
               and unnamed bitfield (is_bit_field=FALSE, bit_size>0) + cached different jit_f (2533:67, 2539:60, 2543:48) */
            a_field dbf_lo = {}, dbf_unm = {}, dbf_hi = {}, dbf_other = {}, dbf_norm = {};
            dbf_lo.source_corresp.name = "dbf_lo"; dbf_lo.type = &int_tp; dbf_lo.is_bit_field = TRUE; dbf_lo.offset_bit_remainder = 0; dbf_lo.bit_size = 2; dbf_lo.next = &dbf_unm;
            dbf_unm.source_corresp.name = NULL; dbf_unm.type = &int_tp; dbf_unm.is_bit_field = FALSE; dbf_unm.offset_bit_remainder = 2; dbf_unm.bit_size = 2; dbf_unm.next = &dbf_hi;
            dbf_hi.source_corresp.name = "dbf_hi"; dbf_hi.type = &int_tp; dbf_hi.is_bit_field = FALSE; dbf_hi.offset_bit_remainder = 4; dbf_hi.bit_size = 2; dbf_hi.next = &dbf_other;
            dbf_other.source_corresp.name = "dbf_other"; dbf_other.type = &int_tp; dbf_other.is_bit_field = FALSE; dbf_other.offset = 4; dbf_other.bit_size = 3; dbf_other.next = &dbf_norm;
            dbf_norm.source_corresp.name = "dbf_norm"; dbf_norm.type = &int_tp; dbf_norm.offset = 8;
            a_type dbf_st = {}; dbf_st.kind = tk_struct; dbf_st.size = 12; dbf_st.source_corresp.name = "S2DbfSt";
            dbf_st.variant.class_struct_union.field_list = &dbf_lo;
            gcc_jit_field *jdbf_fs[3] = {
                gcc_jit_context_new_field(ctx, NULL, jit_int, "w0"),
                gcc_jit_context_new_field(ctx, NULL, jit_int, "w1"),
                gcc_jit_context_new_field(ctx, NULL, jit_int, "w2")
            };
            gcc_jit_type *jdbf_st = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2DbfStJ", 3, jdbf_fs));
            GCC_GEN_BE_CHECK(expr_test_map_type(&dbf_st, jdbf_st));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &dbf_lo, jdbf_fs[0]));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &dbf_hi, jdbf_fs[0]));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &dbf_other, jdbf_fs[1]));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &dbf_norm, jdbf_fs[2]));

            /* Sequential: dbf_lo -> skips dbf_unm -> packs dbf_hi -> sees dbf_other != jdbf_fs[0] -> dbf_other sees dbf_norm non-bf */
            a_constant cd_seq0 = {}, cd_seq1 = {}, cd_seq2 = {}, cd_seq3 = {}, cd_seq_agg = {};
            cd_seq0.kind = ck_integer; cd_seq0.type = &int_tp; cd_seq0.next = &cd_seq1;
            cd_seq1.kind = ck_integer; cd_seq1.type = &int_tp; cd_seq1.next = &cd_seq2;
            cd_seq2.kind = ck_integer; cd_seq2.type = &int_tp; cd_seq2.next = &cd_seq3;
            cd_seq3.kind = ck_integer; cd_seq3.type = &int_tp;
            cd_seq_agg.kind = ck_aggregate; cd_seq_agg.type = &dbf_st; cd_seq_agg.variant.aggregate.first_constant = &cd_seq0;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&cd_seq_agg, jdbf_st, &rv));

            /* Designated: start at dbf_hi (end=6), then designator to dbf_lo (end=2 <= 6, same jit_f), then generic desig, then non-field desig */
            a_constant cd_d0 = {}, cd_v0 = {}, cd_d1 = {}, cd_v1 = {}, cd_dgen = {}, cd_v2 = {}, cd_darr = {}, cd_v3 = {}, cd_des_agg = {};
            cd_d0.kind = ck_designator; cd_d0.variant.designator.is_field_designator = TRUE; cd_d0.variant.designator.variant.field = &dbf_hi; cd_d0.next = &cd_v0;
            cd_v0.kind = ck_integer; cd_v0.type = &int_tp; cd_v0.next = &cd_d1;
            cd_d1.kind = ck_designator; cd_d1.variant.designator.is_field_designator = TRUE; cd_d1.variant.designator.variant.field = &dbf_lo; cd_d1.next = &cd_v1;
            cd_v1.kind = ck_integer; cd_v1.type = &int_tp; cd_v1.next = &cd_dgen;
            cd_dgen.kind = ck_designator; cd_dgen.variant.designator.is_field_designator = TRUE; cd_dgen.variant.designator.is_generic = TRUE; cd_dgen.variant.designator.variant.field = &dbf_other; cd_dgen.next = &cd_v2;
            cd_v2.kind = ck_integer; cd_v2.type = &int_tp; cd_v2.next = &cd_darr;
            cd_darr.kind = ck_designator; cd_darr.variant.designator.is_field_designator = FALSE; cd_darr.next = &cd_v3;
            cd_v3.kind = ck_integer; cd_v3.type = &int_tp;
            cd_des_agg.kind = ck_aggregate; cd_des_agg.type = &dbf_st; cd_des_agg.variant.aggregate.first_constant = &cd_d0;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&cd_des_agg, jdbf_st, &rv));
            g_expr_mock.num_type_overrides = 0;

            /* 3) ck_string array field with null element_type & zero-size integer element_type, plus 2-elem ck_aggregate mismatch */
            a_type arr_nullel = {}, arr_zint = {};
            arr_nullel.kind = tk_array; arr_nullel.size = 2; arr_nullel.variant.array.element_type = NULL; arr_nullel.variant.array.variant.number_of_elements = 2;
            arr_zint.kind = tk_array; arr_zint.size = 2; arr_zint.variant.array.element_type = &zsz_int_tp; arr_zint.variant.array.variant.number_of_elements = 2;
            a_field sf_ne = {}, sf_zi = {}, sf_mm = {};
            sf_ne.source_corresp.name = "sf_ne"; sf_ne.type = &arr_nullel; sf_ne.next = &sf_zi;
            sf_zi.source_corresp.name = "sf_zi"; sf_zi.type = &arr_zint; sf_zi.next = &sf_mm;
            sf_mm.source_corresp.name = "sf_mm"; sf_mm.type = &int_tp;
            a_type st_sedge = {}; st_sedge.kind = tk_struct; st_sedge.size = 16; st_sedge.source_corresp.name = "S2StrEdge";
            st_sedge.variant.class_struct_union.field_list = &sf_ne;
            gcc_jit_type *jit_dbl = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
            gcc_jit_field *jsf_edge[3] = {
                gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_new_array_type(ctx, NULL, jit_uch, 2), "sf_ne"),
                gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_new_array_type(ctx, NULL, jit_int, 2), "sf_zi"),
                gcc_jit_context_new_field(ctx, NULL, jit_dbl, "sf_mm")
            };
            gcc_jit_type *jst_sedge = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2StrEdgeJ", 3, jsf_edge));
            GCC_GEN_BE_CHECK(expr_test_map_type(&st_sedge, jst_sedge));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sf_ne, jsf_edge[0]));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sf_zi, jsf_edge[1]));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sf_mm, jsf_edge[2]));
            a_constant cs_ne = {}, cs_zi = {}, cs_m0 = {}, cs_m1 = {}, cs_magg = {}, cs_eagg = {};
            cs_ne.kind = ck_string; cs_ne.type = &arr_nullel; cs_ne.variant.string.value = "ab"; cs_ne.variant.string.length = 2; cs_ne.next = &cs_zi;
            cs_zi.kind = ck_string; cs_zi.type = &arr_zint; cs_zi.variant.string.value = "cd"; cs_zi.variant.string.length = 2; cs_zi.next = &cs_magg;
            cs_m0.kind = ck_integer; cs_m0.type = &int_tp; cs_m0.next = &cs_m1;
            cs_m1.kind = ck_integer; cs_m1.type = &int_tp;
            cs_magg.kind = ck_aggregate; cs_magg.type = &arr2_tp; cs_magg.variant.aggregate.first_constant = &cs_m0;
            cs_eagg.kind = ck_aggregate; cs_eagg.type = &st_sedge; cs_eagg.variant.aggregate.first_constant = &cs_ne;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&cs_eagg, jst_sedge, &rv));
            g_expr_mock.num_type_overrides = 0;

            /* 4) b0_st base-class 2-field repacking (count == 2, vals[0] juco_tp union != b0_type) */
            a_type sm2f_edg = {}, uco_edg = {};
            a_field sm2_f0 = {}, sm2_f1 = {}, uco_f0 = {};
            sm2_f0.source_corresp.name = "s0"; sm2_f0.type = &int_tp; sm2_f0.next = &sm2_f1;
            sm2_f1.source_corresp.name = "s1"; sm2_f1.type = &int_tp;
            sm2f_edg.kind = tk_struct; sm2f_edg.size = 8; sm2f_edg.source_corresp.name = "S2Sm2fE";
            sm2f_edg.variant.class_struct_union.field_list = &sm2_f0;
            uco_f0.source_corresp.name = "u0"; uco_f0.type = &int_tp;
            uco_edg.kind = tk_union; uco_edg.size = 4; uco_edg.source_corresp.name = "S2UcoE";
            uco_edg.variant.class_struct_union.field_list = &uco_f0;
            gcc_jit_field *juco_f0 = gcc_jit_context_new_field(ctx, NULL, jit_int, "u0");
            gcc_jit_type *juco_tp = gcc_jit_context_new_union_type(ctx, NULL, "S2UcoJ", 1, &juco_f0);
            gcc_jit_field *jb2d_fs[2] = {
                gcc_jit_context_new_field(ctx, NULL, juco_tp, "p0"),
                gcc_jit_context_new_field(ctx, NULL, juco_tp, "p1")
            };
            gcc_jit_type *jb2d_tp = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2B2UcoJ", 2, jb2d_fs));
            gcc_jit_field *jder2d_f0 = gcc_jit_context_new_field(ctx, NULL, jb2d_tp, "__b_u");
            gcc_jit_type *jder2d_tp = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2Der2UcoJ", 1, &jder2d_f0));
            a_type b2d_edg = {}, der2d_edg = {};
            b2d_edg.kind = tk_struct; b2d_edg.size = 8; b2d_edg.source_corresp.name = "S2B2UcoE";
            a_base_class bc2d = {}; bc2d.type = &b2d_edg;
            a_class_type_supplement dsup2d = {}; dsup2d.direct_base_classes = &bc2d;
            der2d_edg.kind = tk_struct; der2d_edg.size = 8; der2d_edg.source_corresp.name = "S2Der2UcoE";
            der2d_edg.variant.class_struct_union.extra_info = &dsup2d;
            GCC_GEN_BE_CHECK(expr_test_map_type(&uco_edg, juco_tp));
            GCC_GEN_BE_CHECK(expr_test_map_type(&b2d_edg, jb2d_tp));
            GCC_GEN_BE_CHECK(expr_test_map_type(&der2d_edg, jder2d_tp));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &uco_f0, juco_f0));
            a_constant ctr_i0 = {}, ctr_i1 = {}, cfl0 = {}, cfl1 = {}, cder2d = {};
            ctr_i0.kind = ck_integer; ctr_i0.type = &int_tp; ctr_i0.next = &ctr_i1;
            ctr_i1.kind = ck_integer; ctr_i1.type = &int_tp;
            cfl0.kind = ck_aggregate; cfl0.type = &uco_edg; cfl0.variant.aggregate.first_constant = &ctr_i0; cfl0.next = &cfl1;
            cfl1.kind = ck_aggregate; cfl1.type = &uco_edg; cfl1.variant.aggregate.first_constant = &ctr_i0;
            cder2d.kind = ck_aggregate; cder2d.type = &der2d_edg; cder2d.variant.aggregate.first_constant = &cfl0;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&cder2d, jder2d_tp, &rv));
            g_expr_mock.num_type_overrides = 0;

            /* 5) needs_trimmed_lval_init: sm2f_edg-vs-uchar[8], int-vs-int, union-vs-int (coerce_rvalue True/False + OOM),
                  null fields[ci], and agg_blk == NULL */
            gcc_jit_field *jsm2_fs[2] = {
                gcc_jit_context_new_field(ctx, NULL, jit_int, "s0"),
                gcc_jit_context_new_field(ctx, NULL, jit_int, "s1")
            };
            gcc_jit_type *jsm2_tp = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2Sm2fJ", 2, jsm2_fs));
            gcc_jit_field *jtr_fs[3] = {
                gcc_jit_context_new_field(ctx, NULL, jit_int, "f_eq_int"),
                gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_new_array_type(ctx, NULL, jit_uch, 8), "f_st_vs_arr"),
                gcc_jit_context_new_field(ctx, NULL, jit_int, "f_un_vs_int")
            };
            gcc_jit_type *jtr_tp = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2TrimFullJ", 3, jtr_fs));
            a_field tr_f0 = {}, tr_f1 = {}, tr_f2 = {}, tr_f3 = {};
            tr_f0.source_corresp.name = "f0"; tr_f0.type = &int_tp; tr_f0.next = &tr_f1;
            tr_f1.source_corresp.name = "f1"; tr_f1.type = &sm2f_edg; tr_f1.next = &tr_f2;
            tr_f2.source_corresp.name = "f2"; tr_f2.type = &uco_edg; tr_f2.next = &tr_f3;
            tr_f3.source_corresp.name = "f3"; tr_f3.type = &int_tp;
            a_type tr_edg = {}; tr_edg.kind = tk_struct; tr_edg.size = 24; tr_edg.source_corresp.name = "S2TrimFullE";
            tr_edg.variant.class_struct_union.field_list = &tr_f0;

            a_constant ctr_eq = {}, ctr_sub1 = {}, ctr_un = {}, ctr_ex = {}, ctr_agg = {};
            ctr_eq.kind = ck_integer; ctr_eq.type = &int_tp; ctr_eq.next = &ctr_sub1;
            ctr_sub1.kind = ck_aggregate; ctr_sub1.type = &sm2f_edg; ctr_sub1.variant.aggregate.first_constant = &ctr_i0; ctr_sub1.next = &ctr_un;
            ctr_un.kind = ck_aggregate; ctr_un.type = &uco_edg; ctr_un.variant.aggregate.first_constant = &ctr_i0; ctr_un.next = &ctr_ex;
            ctr_ex.kind = ck_integer; ctr_ex.type = &int_tp;
            ctr_agg.kind = ck_aggregate; ctr_agg.type = &tr_edg; ctr_agg.variant.aggregate.first_constant = &ctr_eq;

            GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 85, [&]() noexcept -> gcc_gen_be_error_t {
                g_expr_mock.num_type_overrides = 0;
                g_expr_mock.coerce_rval_return_null = FALSE;
                GCC_GEN_BE_CHECK(expr_test_map_type(&sm2f_edg, jsm2_tp));
                GCC_GEN_BE_CHECK(expr_test_map_type(&uco_edg, juco_tp));
                GCC_GEN_BE_CHECK(expr_test_map_type(&tr_edg, jtr_tp));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sm2_f0, jsm2_fs[0]));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &sm2_f1, jsm2_fs[1]));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &uco_f0, juco_f0));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &tr_f0, jtr_fs[0]));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &tr_f1, jtr_fs[1]));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &tr_f2, jtr_fs[2]));
                return gcc_gen_be_lower_constant_rvalue(&ctr_agg, jtr_tp, &rv);
            }));
            g_expr_mock.coerce_rval_return_null = TRUE;
            GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&ctr_agg, jtr_tp, &rv));
            g_expr_mock.coerce_rval_return_null = FALSE;
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&ctr_agg, jtr_tp, &rv));
            GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
            g_expr_mock.num_type_overrides = 0;
        }
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes all Part A coverage tests for `src/gcc_gen_be_expr.c`.
 * @details Sets up a clean `libgccjit` test function and runs helper, dynamic-init, bitfield, and constant suites.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t run_expr_cov_init_and_const_tests(void) GCC_GEN_BE_NOEXCEPT {
    typedef gcc_gen_be_error_t (*part_a_fn_t)(gcc_jit_context *, gcc_jit_function *) GCC_GEN_BE_NOEXCEPT;
    static const part_a_fn_t suites[8] = {
        test_cov_part_a_helpers,
        test_cov_part_a_dynamic_init_and_vla,
        test_cov_part_a_bitfields_and_constants,
        test_cov_part_a_aggregates,
        test_cov_part_a_tail,
        test_cov_part_a_and_b_remaining_gaps,
        test_cov_init_const_final_gaps,
        test_cov_init_const_stage2_gaps
    };
    for (size_t i = 0; i < 8; ++i) {
        gcc_jit_context *ctx = NULL;
        gcc_jit_function *fn = NULL;
        gcc_jit_block *blk = NULL;
        GCC_GEN_BE_CHECK(expr_test_setup_fn(&ctx, &fn, &blk));
        gcc_gen_be_error_t err = suites[i](ctx, fn);
        gcc_gen_be_error_t td_err = expr_test_teardown_fn(ctx);
        if (err != GCC_GEN_BE_SUCCESS) return err;
        GCC_GEN_BE_CHECK(td_err);
    }
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE
