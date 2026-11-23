/**
 * @file test_gcc_be_expr_rval.cpp
 * @brief Unit tests for Part 1 of `gcc_gen_be_lower_expr_rvalue` (`gcc_gen_be_expr.c`).
 * @details Exercises constants, variables, temporaries, statement-expressions, short-circuit
 * logical/ternary/comma operators, scalar/bitfield/atomic assignments, pointer arithmetic,
 * spaceship, arithmetic/complex/vector/comparison binary operators, unary operators, pre/post
 * increment/decrement, address-of, and member/pointer-to-data-member accesses.
 * @ingroup gcc_gen_be_tests
 */

#include "test_gcc_be_expr_common.h"

USING_NAMESPACE_EDG

BEGIN_EDG_NAMESPACE

/**
 * @brief Tests `enk_constant`, `enk_variable`, `enk_temp_init`, `enk_statement`, comma, ternary, and logical ops.
 * @details Exercises atomic variable loads, statement-expressions, `eok_comma`, `eok_question`,
 * `eok_land`, `eok_lor`, scalar/bitfield/atomic `eok_assign` and compound assignments, and OOM sweeps.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_rval_basics_and_assign(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    (void)ctx;
    a_type int_tp, bool_tp, void_tp, ptr_tp, at_tp, cls_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&bool_tp, ik_int, 1));
    bool_tp.variant.integer.bool_type = TRUE;
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_tp, &int_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_typeref(&at_tp, &int_tp, TQ_C11_ATOMIC));

    a_field bf_f;
    std::memset(&bf_f, 0, sizeof(bf_f));
    bf_f.type = &int_tp;
    bf_f.is_bit_field = TRUE;
    bf_f.bit_size = 5;
    bf_f.offset = 0;
    std::memset(&cls_tp, 0, sizeof(cls_tp));
    cls_tp.kind = tk_struct;
    cls_tp.size = 4;
    cls_tp.alignment = 4;
    cls_tp.variant.class_struct_union.field_list = &bf_f;

    a_variable v_int, v_at, v_cls;
    std::memset(&v_int, 0, sizeof(v_int));
    v_int.type = &int_tp;
    v_int.storage_class = (a_storage_class)sc_auto;
    v_int.source_corresp.name = "rv_i";

    std::memset(&v_at, 0, sizeof(v_at));
    v_at.type = &at_tp;
    v_at.storage_class = (a_storage_class)sc_auto;
    v_at.source_corresp.name = "rv_at";

    std::memset(&v_cls, 0, sizeof(v_cls));
    v_cls.type = &cls_tp;
    v_cls.storage_class = (a_storage_class)sc_auto;
    v_cls.source_corresp.name = "rv_cls";

    an_expr_node e_vi, e_vat, e_vcls, e_c1, e_c2;
    a_constant c1, c2;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi, &v_int, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vat, &v_at, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vcls, &v_cls, &cls_tp));
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_c1, &c1, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_c2, &c2, &int_tp));
    GCC_GEN_BE_CHECK(expr_test_map_const_int(&c1, 7));
    GCC_GEN_BE_CHECK(expr_test_map_const_int(&c2, 3));

    gcc_jit_rvalue *rv = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_vi, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_vat, &rv));

    /* Statement expression (`enk_statement`) for non-void and void */
    a_statement st_inner, st_blk;
    std::memset(&st_inner, 0, sizeof(st_inner));
    std::memset(&st_blk, 0, sizeof(st_blk));
    st_inner.kind = (a_statement_kind)stmk_expr;
    st_inner.expr = &e_c1;
    st_blk.kind = (a_statement_kind)stmk_block;
    st_blk.variant.block.statements = &st_inner;

    an_expr_node e_stmt;
    std::memset(&e_stmt, 0, sizeof(e_stmt));
    e_stmt.kind = enk_statement;
    e_stmt.type = &int_tp;
    e_stmt.variant.statement = &st_blk;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 24, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_stmt, &rv);
    }));
    e_stmt.type = &void_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_stmt, &rv));

    /* Comma, ternary (value & void), logical && / || */
    an_expr_node e_op;
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_comma, &int_tp, &e_c1, &e_c2));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_question, &int_tp, &e_c1, &e_vi, &e_c2));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 24, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));

    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_question, &void_tp, &e_c1, &e_c2, &e_vi));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_land, &bool_tp, &e_c1, &e_c2));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 24, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_lor, &bool_tp, &e_c1, &e_c2));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* Assignments: scalar, compound, atomic store/rmw, bitfield assign & compound assign */
    const an_expr_operator_kind assign_ops[] = {
        eok_assign, eok_add_assign, eok_subtract_assign, eok_multiply_assign, eok_divide_assign,
        eok_remainder_assign, eok_shiftl_assign, eok_shiftr_assign, eok_and_assign, eok_or_assign, eok_xor_assign
    };
    for (size_t i = 0; i < sizeof(assign_ops) / sizeof(assign_ops[0]); ++i) {
        GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi, &v_int, &int_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, assign_ops[i], &int_tp, &e_vi, &e_c2));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    }

    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vat, &v_at, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_assign, &int_tp, &e_vat, &e_c2));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vat, &v_at, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_add_assign, &int_tp, &e_vat, &e_c2));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    an_expr_node e_bf_fld, e_bf_dot;
    std::memset(&e_bf_fld, 0, sizeof(e_bf_fld));
    e_bf_fld.kind = enk_field;
    e_bf_fld.variant.field.ptr = &bf_f;
    for (size_t i = 0; i < sizeof(assign_ops) / sizeof(assign_ops[0]); ++i) {
        GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vcls, &v_cls, &cls_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_bf_dot, eok_dot_field, &int_tp, &e_vcls, &e_bf_fld));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, assign_ops[i], &int_tp, &e_bf_dot, &e_c2));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests binary, complex, pointer, vector, unary, inc/dec, address-of, and PMD rvalues.
 * @details Covers `eok_padd`/`eok_psubtract`/`eok_pdiff`, `eok_spaceship`, complex `+`/`-`/`*`/`/`/`==`/`!=`
 * (both native and struct-lowered), unary `-`/`~`/`!`/`__real__`/`__imag__`, pre/post `++`/`--`,
 * `&&label`, `&Class::field`, `&Class::method`, and `.*`/`->*` data-member reads.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_rval_ops_unary_and_pm(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    (void)ctx;
    a_type int_tp, long_tp, bool_tp, ptr_tp, void_ptr_tp, void_tp, cplx_tp, vec_tp, cls_tp, pmd_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&bool_tp, ik_int, 1));
    bool_tp.variant.integer.bool_type = TRUE;
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_tp, &int_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&void_ptr_tp, &void_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_complex_type(&cplx_tp, fk_double, 16));

    std::memset(&vec_tp, 0, sizeof(vec_tp));
    vec_tp.kind = tk_vector;
    vec_tp.size = 16;
    vec_tp.alignment = 16;
    vec_tp.variant.vector.element_type = &int_tp;

    a_field f0;
    std::memset(&f0, 0, sizeof(f0));
    f0.type = &int_tp;
    f0.offset = 0;
    std::memset(&cls_tp, 0, sizeof(cls_tp));
    cls_tp.kind = tk_struct;
    cls_tp.size = 4;
    cls_tp.alignment = 4;
    cls_tp.variant.class_struct_union.field_list = &f0;

    std::memset(&pmd_tp, 0, sizeof(pmd_tp));
    pmd_tp.kind = tk_ptr_to_member;
    pmd_tp.size = 8;
    pmd_tp.variant.ptr_to_member.type = &int_tp;

    a_variable v_ptr, v_int, v_vec, v_cls, v_pmd;
    std::memset(&v_ptr, 0, sizeof(v_ptr));
    v_ptr.type = &ptr_tp;
    v_ptr.storage_class = (a_storage_class)sc_auto;
    v_ptr.source_corresp.name = "p_v";

    std::memset(&v_int, 0, sizeof(v_int));
    v_int.type = &int_tp;
    v_int.storage_class = (a_storage_class)sc_auto;
    v_int.source_corresp.name = "i_v";

    std::memset(&v_vec, 0, sizeof(v_vec));
    v_vec.type = &vec_tp;
    v_vec.storage_class = (a_storage_class)sc_auto;
    v_vec.source_corresp.name = "vec_v";

    std::memset(&v_cls, 0, sizeof(v_cls));
    v_cls.type = &cls_tp;
    v_cls.storage_class = (a_storage_class)sc_auto;
    v_cls.source_corresp.name = "cls_v";

    std::memset(&v_pmd, 0, sizeof(v_pmd));
    v_pmd.type = &pmd_tp;
    v_pmd.storage_class = (a_storage_class)sc_auto;
    v_pmd.source_corresp.name = "pmd_v";

    an_expr_node ep1, ep2, ei1, ei2, e_op;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ep1, &v_ptr, &ptr_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ep2, &v_ptr, &ptr_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ei1, &v_int, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ei2, &v_int, &int_tp));

    gcc_jit_rvalue *rv = NULL;

    /* Pointer arithmetic & spaceship */
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_padd, &ptr_tp, &ep1, &ei1));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ei1, &v_int, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ep1, &v_ptr, &ptr_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_padd, &ptr_tp, &ei1, &ep1));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ep1, &v_ptr, &ptr_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ei1, &v_int, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_psubtract, &ptr_tp, &ep1, &ei1));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ep1, &v_ptr, &ptr_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ep2, &v_ptr, &ptr_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_pdiff, &long_tp, &ep1, &ep2));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    GCC_GEN_BE_CHECK(expr_init_var_expr(&ei1, &v_int, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ei2, &v_int, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_spaceship, &int_tp, &ei1, &ei2));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* Scalar arithmetic, bitwise, shift, and comparison operators */
    const an_expr_operator_kind bin_ops[] = {
        eok_add, eok_subtract, eok_multiply, eok_divide, eok_remainder,
        eok_and, eok_or, eok_xor, eok_shiftl, eok_shiftr
    };
    for (size_t i = 0; i < sizeof(bin_ops) / sizeof(bin_ops[0]); ++i) {
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ei1, &v_int, &int_tp));
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ei2, &v_int, &int_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, bin_ops[i], &int_tp, &ei1, &ei2));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    }
    const an_expr_operator_kind cmp_ops[] = {
        eok_eq, eok_ne, eok_lt, eok_le, eok_gt, eok_ge
    };
    for (size_t i = 0; i < sizeof(cmp_ops) / sizeof(cmp_ops[0]); ++i) {
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ei1, &v_int, &int_tp));
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ei2, &v_int, &int_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, cmp_ops[i], &bool_tp, &ei1, &ei2));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    }

    /* Vector comparison (`eok_vector_eq`, `eok_vector_lt`, etc.) */
    an_expr_node ev1, ev2;
    const an_expr_operator_kind vec_cmp_ops[] = {
        eok_vector_eq, eok_vector_ne, eok_vector_lt, eok_vector_le, eok_vector_gt, eok_vector_ge
    };
    for (size_t i = 0; i < sizeof(vec_cmp_ops) / sizeof(vec_cmp_ops[0]); ++i) {
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ev1, &v_vec, &vec_tp));
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ev2, &v_vec, &vec_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, vec_cmp_ops[i], &vec_tp, &ev1, &ev2));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    }

    /* Complex arithmetic, equality, conjugate, real/imag (native & struct) */
    a_type dbl_tp;
    GCC_GEN_BE_CHECK(expr_init_float_type(&dbl_tp, fk_double, 8));
    a_variable v_cx_arr[2];
    std::memset(v_cx_arr, 0, sizeof(v_cx_arr));
    for (int mode = 0; mode < 2; ++mode) {
        g_expr_mock.complex_as_struct = (mode == 1) ? TRUE : FALSE;
        v_cx_arr[mode].type = &cplx_tp;
        v_cx_arr[mode].storage_class = (a_storage_class)sc_auto;
        v_cx_arr[mode].source_corresp.name = mode ? "cx_st" : "cx_nat";

        an_expr_node ecx1, ecx2;
        const an_expr_operator_kind cx_ops[] = { eok_add, eok_subtract, eok_multiply, eok_divide };
        for (size_t i = 0; i < 4; ++i) {
            GCC_GEN_BE_CHECK(expr_init_var_expr(&ecx1, &v_cx_arr[mode], &cplx_tp));
            GCC_GEN_BE_CHECK(expr_init_var_expr(&ecx2, &v_cx_arr[mode], &cplx_tp));
            GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, cx_ops[i], &cplx_tp, &ecx1, &ecx2));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
        }
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ecx1, &v_cx_arr[mode], &cplx_tp));
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ecx2, &v_cx_arr[mode], &cplx_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_eq, &bool_tp, &ecx1, &ecx2));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ecx1, &v_cx_arr[mode], &cplx_tp));
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ecx2, &v_cx_arr[mode], &cplx_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_ne, &bool_tp, &ecx1, &ecx2));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

        GCC_GEN_BE_CHECK(expr_init_var_expr(&ecx1, &v_cx_arr[mode], &cplx_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_negate, &cplx_tp, &ecx1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ecx1, &v_cx_arr[mode], &cplx_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_xconj, &cplx_tp, &ecx1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ecx1, &v_cx_arr[mode], &cplx_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_real_part, &dbl_tp, &ecx1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ecx1, &v_cx_arr[mode], &cplx_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_imag_part, &dbl_tp, &ecx1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    }
    g_expr_mock.complex_as_struct = FALSE;

    /* Unary +, -, ~, !, pre/post ++/-- (scalar & pointer) */
    const an_expr_operator_kind un_ops[] = {
        eok_unary_plus, eok_negate, eok_complement, eok_not,
        eok_post_incr, eok_post_decr, eok_pre_incr, eok_pre_decr
    };
    for (size_t i = 0; i < sizeof(un_ops) / sizeof(un_ops[0]); ++i) {
        GCC_GEN_BE_CHECK(expr_init_var_expr(&ei1, &v_int, &int_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, un_ops[i], (un_ops[i] == eok_not) ? &bool_tp : &int_tp, &ei1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    }
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ep1, &v_ptr, &ptr_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_post_incr, &ptr_tp, &ep1));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ep1, &v_ptr, &ptr_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_pre_decr, &ptr_tp, &ep1));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* Address of label (`ck_address` `abk_label`), address of variable, and `.*` PMD read */
    a_label lbl;
    std::memset(&lbl, 0, sizeof(lbl));
    a_constant c_lbl;
    std::memset(&c_lbl, 0, sizeof(c_lbl));
    c_lbl.kind = ck_address;
    c_lbl.type = &void_ptr_tp;
    c_lbl.variant.address.kind = (an_address_base_kind)abk_label;
    c_lbl.variant.address.variant.label = &lbl;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&c_lbl, NULL, &rv));

    an_expr_node e_cls, e_pmd;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ei1, &v_int, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_address_of, &ptr_tp, &ei1));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_cls, &v_cls, &cls_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_pmd, &v_pmd, &pmd_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_op, eok_pm_field, &int_tp, &e_cls, &e_pmd));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 24, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t run_expr_rval_part1_tests(void) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    gcc_jit_function *fn = NULL;
    gcc_jit_block *blk = NULL;
    GCC_GEN_BE_CHECK(expr_test_setup_fn(&ctx, &fn, &blk));

    gcc_gen_be_error_t err = test_rval_basics_and_assign(ctx, fn);
    if (err == GCC_GEN_BE_SUCCESS) {
        GCC_GEN_BE_CHECK(expr_test_reset_mock());
        err = test_rval_ops_unary_and_pm(ctx, fn);
    }

    GCC_GEN_BE_CHECK(expr_test_teardown_fn(ctx));
    return err;
}

END_EDG_NAMESPACE
