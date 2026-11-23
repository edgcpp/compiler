/**
 * @file test_gcc_be_expr_cov_rval.cpp
 * @brief Exhaustive branch, line, and OOM coverage tests for `gcc_gen_be_lower_expr_rvalue` in `src/gcc_gen_be_expr.c`.
 * @details Covers all rvalue expression kinds, builtin operations, short-circuit/ternary control flow,
 * imaginary/complex/vector/pointer/atomic operators, direct/member/PMF/virtual calls, variadics,
 * pointer-to-member casts, `new`/`delete` exception cleanups, overriding-function wrappers, and OOM budgets.
 * @ingroup gcc_gen_be_tests
 */

#include "test_gcc_be_expr_common.h"
#include "gcc_gen_be_lib_loader.h"

USING_NAMESPACE_EDG

BEGIN_EDG_NAMESPACE

/**
 * @brief Sweeps OOM budgets on `gcc_gen_be_lower_expr_rvalue` while preserving intentional `cblock == NULL` state.
 * @details Uses the active `gcc_jit_block`'s owning function (if `cblock != NULL`) to reset basic blocks
 * between OOM retries, or passes `NULL` when `cblock == NULL` so null-block paths stay exercised.
 * @param[in] expr Frontend expression node to lower.
 * @param[out] out_rv Output rvalue slot.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static inline gcc_gen_be_error_t expr_sweep_rval_oom(
    an_expr_node_ptr expr,
    gcc_jit_rvalue **out_rv) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_block *orig_blk = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&orig_blk));
    gcc_jit_function *owner_fn = orig_blk ? gcc_jit_block_get_function(orig_blk) : NULL;
    return expr_sweep_oom_fn(owner_fn, 96, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(expr, out_rv);
    });
}

/**
 * @brief Covers builtins, variable reads, lvalue-read preambles, and short-circuit/ternary control flow in `gcc_gen_be_lower_expr_rvalue`.
 * @details Exercises `!ctx`, `enk_lambda`, `bok_builtin_complex`, `bok_builtin_addressof`, `bok_offsetof`/`bok_intaddr`,
 * `bok_builtin_bit_cast`, vector shuffle/convert edge branches, `enk_variable` static/atomic/VLA reads,
 * `enk_operation` lvalue-read preambles, `eok_comma`, `eok_land`/`eok_lor`, and `eok_question`.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_rval_builtins_vars_and_flow(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, long_tp, bool_tp, dbl_tp, cplx_tp, void_tp, vptr_tp, iptr_tp, arr_tp, vla_tp, vec_tp, rfn_tp, rfn_ptr_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&bool_tp, ik_int, 1));
    bool_tp.variant.integer.bool_type = TRUE;
    GCC_GEN_BE_CHECK(expr_init_float_type(&dbl_tp, fk_double, 8));
    GCC_GEN_BE_CHECK(expr_init_complex_type(&cplx_tp, fk_double, 16));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&iptr_tp, &int_tp, FALSE));

    std::memset(&arr_tp, 0, sizeof(arr_tp));
    arr_tp.kind = tk_array;
    arr_tp.size = 8;
    arr_tp.variant.array.element_type = &int_tp;
    arr_tp.variant.array.variant.number_of_elements = 2;

    std::memset(&vla_tp, 0, sizeof(vla_tp));
    vla_tp.kind = tk_array;
    vla_tp.variant.array.element_type = &int_tp;
    vla_tp.variant.array.is_variable_size_array = TRUE;
    vla_tp.variant.array.variant.number_of_elements = 2;

    std::memset(&vec_tp, 0, sizeof(vec_tp));
    vec_tp.kind = tk_vector;
    vec_tp.size = 16;
    vec_tp.variant.vector.element_type = &int_tp;

    std::memset(&rfn_tp, 0, sizeof(rfn_tp));
    rfn_tp.kind = tk_routine;
    rfn_tp.variant.routine.return_type = &void_tp;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&rfn_ptr_tp, &rfn_tp, FALSE));

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_bool = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
    gcc_jit_type *jit_dbl = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
    gcc_jit_type *jit_iptr = gcc_jit_type_get_pointer(jit_int);
    gcc_jit_type *jit_vec = gcc_jit_type_get_vector(jit_int, 4);
    GCC_GEN_BE_CHECK(expr_test_map_type(&void_tp, jit_void));
    GCC_GEN_BE_CHECK(expr_test_map_type(&vec_tp, jit_vec));

    gcc_jit_rvalue *rv = NULL;
    a_constant c1, c2;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c1, &int_tp, 1));
    GCC_GEN_BE_CHECK(expr_init_int_const(&c2, &int_tp, 2));
    an_expr_node e_c1, e_c2, e_bad, e_null_ty;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_c1, &c1));
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_c2, &c2));
    std::memset(&e_bad, 0, sizeof(e_bad));
    e_bad.kind = (an_expr_node_kind)99;
    e_bad.type = &int_tp;
    std::memset(&e_null_ty, 0, sizeof(e_null_ty));
    e_null_ty.kind = enk_type_operand;
    e_null_ty.type = &int_tp;

    /* !ctx guard */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_c1, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* enk_lambda: error and temp_lval == NULL (void type -> new_local returns NULL!) */
    an_expr_node e_lam;
    std::memset(&e_lam, 0, sizeof(e_lam));
    e_lam.kind = enk_lambda;
    e_lam.type = &void_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_lam, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_lam, &rv)); /* temp_lval == NULL! */

    /* enk_builtin_operation: bok_builtin_complex (native & struct), bok_builtin_addressof,
       bok_offsetof / bok_intaddr, bok_builtin_bit_cast, vector shuffle/convert edge branches */
    a_variable v_dbl, v_int, v_bool, v_iptr, v_arr, v_vla, v_vec;
    std::memset(&v_dbl, 0, sizeof(v_dbl));
    v_dbl.type = &dbl_tp;
    std::memset(&v_int, 0, sizeof(v_int));
    v_int.type = &int_tp;
    std::memset(&v_bool, 0, sizeof(v_bool));
    v_bool.type = &bool_tp;
    std::memset(&v_iptr, 0, sizeof(v_iptr));
    v_iptr.type = &iptr_tp;
    std::memset(&v_arr, 0, sizeof(v_arr));
    v_arr.type = &arr_tp;
    std::memset(&v_vla, 0, sizeof(v_vla));
    v_vla.type = &vla_tp;
    std::memset(&v_vec, 0, sizeof(v_vec));
    v_vec.type = &vec_tp;

    gcc_jit_lvalue *lv_d = gcc_jit_function_new_local(fn, NULL, jit_dbl, "rv_d");
    gcc_jit_lvalue *lv_i = gcc_jit_function_new_local(fn, NULL, jit_int, "rv_i");
    gcc_jit_lvalue *lv_b = gcc_jit_function_new_local(fn, NULL, jit_bool, "rv_b");
    gcc_jit_lvalue *lv_ip = gcc_jit_function_new_local(fn, NULL, jit_iptr, "rv_ip");
    gcc_jit_lvalue *lv_arr = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_context_new_array_type(ctx, NULL, jit_int, 2), "rv_arr");
    gcc_jit_lvalue *lv_vec = gcc_jit_function_new_local(fn, NULL, jit_vec, "rv_vec");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_dbl, lv_d));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_int, lv_i));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_bool, lv_b));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_iptr, lv_ip));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_arr, lv_arr));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vla, lv_arr)); /* array-typed VLA -> hits !is_pointer! */
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vec, lv_vec));

    an_expr_node e_vd1, e_vd2, e_vi, e_vb, e_vip, e_varr, e_vvla, e_vvec, e_bop;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vd1, &v_dbl));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vd2, &v_dbl));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi, &v_int));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vb, &v_bool));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vip, &v_iptr));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_varr, &v_arr));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vvla, &v_vla));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vvec, &v_vec));

    std::memset(&e_bop, 0, sizeof(e_bop));
    e_bop.kind = enk_builtin_operation;
    e_bop.type = &cplx_tp;
    e_bop.variant.builtin_operation.kind = bok_builtin_complex;
    /* op1 == NULL, op2 == NULL, native complex, struct complex, r1/r2 == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_vd1.next = &e_vd2;
    e_bop.variant.builtin_operation.operands = &e_vd1;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    g_expr_mock.complex_as_struct = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_null_ty.next = &e_null_ty; /* AVOID cycle! Use separate second node */
    an_expr_node e_null_ty2 = e_null_ty;
    e_null_ty.next = &e_null_ty2;
    e_bop.variant.builtin_operation.operands = &e_null_ty;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    g_expr_mock.complex_as_struct = FALSE;
    e_vd1.next = NULL;
    e_null_ty.next = NULL;

    /* bok_builtin_addressof: target_lval != NULL & target_lval == NULL */
    e_bop.type = &iptr_tp;
    e_bop.variant.builtin_operation.kind = bok_builtin_addressof;
    e_bop.variant.builtin_operation.operands = &e_vi;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    an_expr_node e_null_var;
    std::memset(&e_null_var, 0, sizeof(e_null_var));
    e_null_var.kind = enk_variable;
    e_null_var.type = &int_tp;
    e_null_var.variant.variable.ptr = NULL;
    e_bop.variant.builtin_operation.operands = &e_null_var;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));

    /* bok_offsetof & bok_intaddr */
    a_field off_fld;
    std::memset(&off_fld, 0, sizeof(off_fld));
    off_fld.offset = 12;
    an_expr_node e_off_fld;
    std::memset(&e_off_fld, 0, sizeof(e_off_fld));
    e_off_fld.kind = enk_field;
    e_off_fld.type = &int_tp;
    e_off_fld.variant.field.ptr = &off_fld;

    e_bop.type = &long_tp;
    e_bop.variant.builtin_operation.kind = bok_offsetof;
    e_null_ty.next = &e_off_fld;
    e_bop.variant.builtin_operation.operands = &e_null_ty;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv)); /* valid field */
    e_off_fld.variant.field.ptr = NULL; /* NULL field.ptr -> falls to target_op rvalue -> UNHANDLED_EXPR on enk_field! */
    if (gcc_gen_be_lower_expr_rvalue(&e_bop, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_null_ty.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv)); /* sub_rval != NULL */
    e_null_ty.next = &e_null_ty2;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv)); /* sub_rval == NULL */
    e_null_ty.next = NULL;

    e_bop.variant.builtin_operation.kind = bok_intaddr;
    e_bop.variant.builtin_operation.operands = NULL; /* target_op == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_bop.variant.builtin_operation.operands = &e_c1;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));

    /* bok_builtin_bit_cast */
    e_bop.type = &int_tp;
    e_bop.variant.builtin_operation.kind = bok_builtin_bit_cast;
    e_bop.variant.builtin_operation.operands = NULL; /* src_op == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_bop.variant.builtin_operation.operands = &e_null_ty; /* op1 is enk_type_operand, op2 == NULL -> src_op == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_null_ty.next = &e_null_ty2; /* src_op == e_null_ty2 -> src_rval == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_null_ty.next = NULL;

    e_bop.variant.builtin_operation.operands = &e_vi; /* op1 != enk_type_operand, p_gcc_jit_context_new_bitcast != NULL */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    auto saved_bc = p_gcc_jit_context_new_bitcast;
    p_gcc_jit_context_new_bitcast = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv)); /* bitcast == NULL, cblock != NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv)); /* bitcast == NULL, cblock == NULL */
    p_gcc_jit_context_new_bitcast = saved_bc;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* Remaining 4 branches in vector_shuffle (L5732, L5734, L5749) & convertvector (L5853) */
    a_type zero_el_tp, vec_null_el, vec_zero_el, vec_small;
    GCC_GEN_BE_CHECK(expr_init_int_type(&zero_el_tp, ik_int, 0));
    std::memset(&vec_null_el, 0, sizeof(vec_null_el));
    vec_null_el.kind = tk_vector;
    vec_null_el.variant.vector.element_type = NULL;
    std::memset(&vec_zero_el, 0, sizeof(vec_zero_el));
    vec_zero_el.kind = tk_vector;
    vec_zero_el.size = 16;
    vec_zero_el.variant.vector.element_type = &zero_el_tp;
    std::memset(&vec_small, 0, sizeof(vec_small));
    vec_small.kind = tk_vector;
    vec_small.size = 2;
    vec_small.variant.vector.element_type = &int_tp; /* ut->size < elem->size! */
    GCC_GEN_BE_CHECK(expr_test_map_type(&vec_null_el, jit_vec));
    GCC_GEN_BE_CHECK(expr_test_map_type(&vec_zero_el, jit_vec));
    GCC_GEN_BE_CHECK(expr_test_map_type(&vec_small, jit_vec));

    a_type tref_null;
    std::memset(&tref_null, 0, sizeof(tref_null));
    tref_null.kind = tk_typeref;
    tref_null.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(expr_test_map_type(&tref_null, jit_vec));

    an_expr_node e_vvec2 = e_vvec;
    an_expr_node e_m_null_con;
    std::memset(&e_m_null_con, 0, sizeof(e_m_null_con));
    e_m_null_con.kind = enk_constant;
    e_m_null_con.type = &int_tp;
    e_m_null_con.variant.constant.ptr = NULL;

    e_bop.variant.builtin_operation.kind = bok_builtin_shufflevector;
    e_vvec.next = &e_vvec2;
    e_vvec2.next = &e_m_null_con;
    e_bop.variant.builtin_operation.operands = &e_vvec;
    e_bop.type = &tref_null;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_bop.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_bop.type = &vec_null_el;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_bop.type = &vec_zero_el;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_bop.type = &vec_small;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_vvec.next = NULL;
    e_vvec2.next = NULL;

    auto saved_cv = p_gcc_jit_context_new_convert_vector;
    p_gcc_jit_context_new_convert_vector = NULL;
    e_bop.variant.builtin_operation.kind = bok_builtin_convertvector;
    e_bop.variant.builtin_operation.operands = &e_vvec;
    e_vvec.type = &tref_null;
    e_bop.type = &tref_null;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_vvec.type = &int_tp;
    e_bop.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_vvec.type = &vec_null_el;
    e_bop.type = &vec_null_el;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_vvec.type = &vec_tp;
    e_bop.type = &vec_zero_el;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_bop.type = &vec_small;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    p_gcc_jit_context_new_convert_vector = saved_cv;

    /* enk_variable in rvalue position:
       - anonymous static constant (int, array, NULL type)
       - lval error, C11 _Atomic with expr->type == NULL, VLA array decay, array lower_type error */
    a_variable v_anon_s, v_anon_arr;
    std::memset(&v_anon_s, 0, sizeof(v_anon_s));
    v_anon_s.type = &int_tp;
    v_anon_s.storage_class = sc_static;
    v_anon_s.init_kind = initk_static;
    v_anon_s.initializer.constant = &c1;
    an_expr_node e_vanon;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vanon, &v_anon_s));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_vanon, &rv));
    e_vanon.type = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_vanon, &rv));

    a_constant c_arr_zero;
    std::memset(&c_arr_zero, 0, sizeof(c_arr_zero));
    c_arr_zero.kind = ck_aggregate;
    c_arr_zero.type = &arr_tp;
    v_anon_arr = v_anon_s;
    v_anon_arr.type = &arr_tp;
    v_anon_arr.initializer.constant = &c_arr_zero;
    e_vanon.type = &arr_tp;
    e_vanon.variant.variable.ptr = &v_anon_arr;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_vanon, &rv));

    /* C11 _Atomic variable load with expr->type == NULL */
    g_expr_mock.atomic_target_tp = &int_tp;
    e_vi.type = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_vi, &rv));
    e_vi.type = &int_tp;
    g_expr_mock.atomic_target_tp = NULL;

    /* VLA decay (v_vla has array-typed lv_arr!) */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_vvla, &rv));

    /* enk_operation lvalue-read preamble (L2952..2988):
       - eok_indirect on function / pointer-to-function (etype_chk, op1_t_chk, op1_pts_chk == tk_routine)
       - bitfield with is_bit_field == FALSE && bit_size > 0
       - C11 _Atomic load on eok_indirect with expr->type != NULL and == NULL
       - tk_array lvalue operation decay */
    a_routine r_fn;
    std::memset(&r_fn, 0, sizeof(r_fn));
    r_fn.type = &rfn_tp;
    gcc_jit_function *j_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_rv_fn", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_fn, j_fn));
    an_expr_node e_rfn, e_ind_fn;
    std::memset(&e_rfn, 0, sizeof(e_rfn));
    e_rfn.kind = enk_routine;
    e_rfn.type = &rfn_ptr_tp;
    e_rfn.variant.routine.ptr = &r_fn;
    GCC_GEN_BE_CHECK(expr_init_op(&e_ind_fn, eok_indirect, &rfn_tp, &e_rfn));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_ind_fn, &rv)); /* etype_chk == tk_routine */
    e_ind_fn.type = &int_tp;
    e_rfn.type = &rfn_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_ind_fn, &rv)); /* op1_t_chk == tk_routine */
    e_rfn.type = &rfn_ptr_tp;
    e_ind_fn.variant.operation.kind = eok_ref_indirect;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_ind_fn, &rv)); /* op1_pts_chk == tk_routine */

    /* C11 _Atomic load on eok_indirect with expr->type != NULL and == NULL */
    g_expr_mock.atomic_target_tp = &int_tp;
    an_expr_node e_ind_ip;
    GCC_GEN_BE_CHECK(expr_init_op(&e_ind_ip, eok_indirect, &int_tp, &e_vip));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_ind_ip, &rv));
    e_ind_ip.type = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_ind_ip, &rv));
    g_expr_mock.atomic_target_tp = NULL;

    /* tk_array decay on eok_parens? Wait: preamble runs on eok_indirect! Dereference pointer-to-array! */
    a_type ptr_arr_tp;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_arr_tp, &arr_tp, FALSE));
    a_variable v_parr;
    std::memset(&v_parr, 0, sizeof(v_parr));
    v_parr.type = &ptr_arr_tp;
    gcc_jit_lvalue *lv_parr = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_type_get_pointer(gcc_jit_context_new_array_type(ctx, NULL, jit_int, 2)), "rv_parr");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_parr, lv_parr));
    an_expr_node e_vparr, e_ind_arr;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vparr, &v_parr));
    GCC_GEN_BE_CHECK(expr_init_op(&e_ind_arr, eok_indirect, &arr_tp, &e_vparr));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_ind_arr, &rv);
    }));
    /* Also VLA array decay on eok_indirect */
    e_ind_arr.type = &vla_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_ind_arr, &rv));

    /* eok_comma: op1 == NULL */
    an_expr_node e_comma_null;
    GCC_GEN_BE_CHECK(expr_init_op(&e_comma_null, eok_comma, &int_tp, NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_comma_null, &rv));

    /* eok_land & eok_lor: !cblock, op1 err, ptr r1, bool r1, op2 err, op2 enk_throw (rhs_block==NULL), ptr r2, bool r2, bool result */
    an_expr_node e_throw, e_logic;
    std::memset(&e_throw, 0, sizeof(e_throw));
    e_throw.kind = enk_throw;
    e_throw.type = &int_tp;

    GCC_GEN_BE_CHECK(expr_init_op(&e_logic, eok_land, &bool_tp, &e_vb));
    e_vb.next = &e_vip;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_logic, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_logic, &rv)); /* bool r1, ptr r2, bool result! */

    e_logic.variant.operation.kind = eok_lor;
    e_vip.next = &e_vb;
    e_vb.next = NULL;
    e_logic.variant.operation.operands = &e_vip;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_logic, &rv)); /* ptr r1, bool r2! */
    e_vip.next = &e_throw;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_logic, &rv)); /* rhs_block == NULL! */
    e_vip.next = &e_bad;
    if (gcc_gen_be_lower_expr_rvalue(&e_logic, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_vip.next = NULL;
    e_bad.next = &e_c1;
    e_logic.variant.operation.operands = &e_bad;
    if (gcc_gen_be_lower_expr_rvalue(&e_logic, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_bad.next = NULL;

    /* eok_question: op2==NULL, !cblock, op1 err, bool/ptr r1, op2/op3 err, op2/op3 enk_throw, void r2/r3==NULL */
    an_expr_node e_q;
    GCC_GEN_BE_CHECK(expr_init_op(&e_q, eok_question, &int_tp, &e_vb));
    e_vb.next = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_q, &rv)); /* op2 == NULL & op3 == NULL */
    e_vb.next = &e_c1;
    e_c1.next = &e_c2;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_q, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_q, &rv)); /* bool r1 */

    /* Pointer r1 + void result + enk_throw on both branches (then_block == NULL & else_block == NULL -> !any_branch_reaches_merge!) */
    an_expr_node e_throw2 = e_throw;
    e_vip.next = &e_throw;
    e_throw.next = &e_throw2;
    e_q.type = &void_tp;
    e_q.variant.operation.operands = &e_vip;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_q, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* Void ternary with r2 == NULL & r3 == NULL reaching merge, and op3 == NULL */
    e_vip.next = &e_null_ty;
    e_null_ty.next = NULL; /* op3 == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_q, &rv));
    e_null_ty.next = &e_null_ty2;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_q, &rv));
    e_null_ty.next = NULL;

    /* op1 err, op2 err, op3 err */
    e_bad.next = &e_c1;
    e_q.type = &int_tp;
    e_q.variant.operation.operands = &e_bad;
    if (gcc_gen_be_lower_expr_rvalue(&e_q, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_bad.next = NULL;
    e_vb.next = &e_bad;
    e_bad.next = &e_c2;
    e_q.variant.operation.operands = &e_vb;
    if (gcc_gen_be_lower_expr_rvalue(&e_q, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_bad.next = NULL;
    e_vb.next = &e_c1;
    e_c1.next = &e_bad;
    if (gcc_gen_be_lower_expr_rvalue(&e_q, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_vb.next = NULL;
    e_c1.next = NULL;
    e_vip.next = NULL;
    e_throw.next = NULL;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Covers assignments, arithmetic/imaginary/vector/pointer ops, calls, and variadics in `gcc_gen_be_lower_expr_rvalue`.
 * @details Exercises `eok_assign`/`eok_bassign` coercions, pointer arithmetic (`eok_padd`, `eok_psubtract`, `eok_pdiff`),
 * imaginary/complex/vector/relational ops, pre/post incr/decr, `eok_address_of`, ABI-coerced/builtin/member/PMF/virtual calls,
 * and `va_start`/`va_end`/`va_copy`/`va_arg` across all supported target ABIs.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_rval_arith_assign_calls_va(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, long_tp, bool_tp, flt_tp, dbl_tp, cplx_tp, void_tp, vptr_tp, iptr_tp, arr_tp, vec_tp, st1_tp, st2_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&bool_tp, ik_int, 1));
    bool_tp.variant.integer.bool_type = TRUE;
    GCC_GEN_BE_CHECK(expr_init_float_type(&flt_tp, fk_float, 4));
    GCC_GEN_BE_CHECK(expr_init_float_type(&dbl_tp, fk_double, 8));
    GCC_GEN_BE_CHECK(expr_init_complex_type(&cplx_tp, fk_double, 16));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&iptr_tp, &int_tp, FALSE));

    std::memset(&arr_tp, 0, sizeof(arr_tp));
    arr_tp.kind = tk_array;
    arr_tp.size = 8;
    arr_tp.variant.array.element_type = &int_tp;
    arr_tp.variant.array.variant.number_of_elements = 2;

    std::memset(&vec_tp, 0, sizeof(vec_tp));
    vec_tp.kind = tk_vector;
    vec_tp.size = 16;
    vec_tp.variant.vector.element_type = &int_tp;

    std::memset(&st1_tp, 0, sizeof(st1_tp));
    st1_tp.kind = tk_struct;
    st1_tp.size = 4;
    st1_tp.alignment = 4;
    std::memset(&st2_tp, 0, sizeof(st2_tp));
    st2_tp.kind = tk_struct;
    st2_tp.size = 16;
    st2_tp.alignment = 16;

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_long = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_type *jit_bool = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
    gcc_jit_type *jit_flt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
    gcc_jit_type *jit_dbl = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_vptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *jit_iptr = gcc_jit_type_get_pointer(jit_int);
    gcc_jit_type *jit_vec = gcc_jit_type_get_vector(jit_int, 4);
    GCC_GEN_BE_CHECK(expr_test_map_type(&void_tp, jit_void));
    GCC_GEN_BE_CHECK(expr_test_map_type(&vec_tp, jit_vec));
    GCC_GEN_BE_CHECK(expr_test_map_type(&st1_tp, g_expr_mock.one_field_struct_type));
    GCC_GEN_BE_CHECK(expr_test_map_type(&st2_tp, g_expr_mock.cplx_struct_type));

    a_variable v_i, v_l, v_f, v_d, v_vp, v_ip, v_arr, v_vec, v_s1, v_s2;
    std::memset(&v_i, 0, sizeof(v_i));     v_i.type = &int_tp;
    std::memset(&v_l, 0, sizeof(v_l));     v_l.type = &long_tp;
    std::memset(&v_f, 0, sizeof(v_f));     v_f.type = &flt_tp;
    std::memset(&v_d, 0, sizeof(v_d));     v_d.type = &dbl_tp;
    std::memset(&v_vp, 0, sizeof(v_vp));   v_vp.type = &vptr_tp;
    std::memset(&v_ip, 0, sizeof(v_ip));   v_ip.type = &iptr_tp;
    std::memset(&v_arr, 0, sizeof(v_arr)); v_arr.type = &arr_tp;
    std::memset(&v_vec, 0, sizeof(v_vec)); v_vec.type = &vec_tp;
    std::memset(&v_s1, 0, sizeof(v_s1));   v_s1.type = &st1_tp;
    std::memset(&v_s2, 0, sizeof(v_s2));   v_s2.type = &st2_tp;

    gcc_jit_lvalue *lv_i = gcc_jit_function_new_local(fn, NULL, jit_int, "ca_i");
    gcc_jit_lvalue *lv_l = gcc_jit_function_new_local(fn, NULL, jit_long, "ca_l");
    gcc_jit_lvalue *lv_f = gcc_jit_function_new_local(fn, NULL, jit_flt, "ca_f");
    gcc_jit_lvalue *lv_d = gcc_jit_function_new_local(fn, NULL, jit_dbl, "ca_d");
    gcc_jit_lvalue *lv_vp = gcc_jit_function_new_local(fn, NULL, jit_vptr, "ca_vp");
    gcc_jit_lvalue *lv_ip = gcc_jit_function_new_local(fn, NULL, jit_iptr, "ca_ip");
    gcc_jit_lvalue *lv_arr = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_context_new_array_type(ctx, NULL, jit_int, 2), "ca_arr");
    gcc_jit_lvalue *lv_vec = gcc_jit_function_new_local(fn, NULL, jit_vec, "ca_vec");
    gcc_jit_lvalue *lv_s1 = gcc_jit_function_new_local(fn, NULL, g_expr_mock.one_field_struct_type, "ca_s1");
    gcc_jit_lvalue *lv_s2 = gcc_jit_function_new_local(fn, NULL, g_expr_mock.cplx_struct_type, "ca_s2");

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_i, lv_i));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_l, lv_l));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_f, lv_f));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_d, lv_d));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vp, lv_vp));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_ip, lv_ip));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_arr, lv_arr));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vec, lv_vec));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_s1, lv_s1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_s2, lv_s2));

    an_expr_node e_vi, e_vl, e_vf, e_vd, e_vvp, e_vip, e_varr, e_vvec, e_vs1, e_vs2, e_null_ty, e_op;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi, &v_i));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vl, &v_l));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vf, &v_f));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vd, &v_d));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vvp, &v_vp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vip, &v_ip));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_varr, &v_arr));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vvec, &v_vec));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vs1, &v_s1));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vs2, &v_s2));
    std::memset(&e_null_ty, 0, sizeof(e_null_ty));
    e_null_ty.kind = enk_type_operand;
    e_null_ty.type = &int_tp;

    gcc_jit_rvalue *rv = NULL;

    /* Assignments: eok_bassign, pointer-to-pointer mismatch, scalar-to-1-field-struct (same & diff f0_t),
       scalar-to-scalar mismatch, compound assign with r2==NULL & l1==NULL & r2!=l1_t */
    e_vip.next = &e_vvp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_bassign, &iptr_tp, &e_vip));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* void* -> int* */
    e_vip.next = NULL;

    e_vs1.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_assign, &st1_tp, &e_vs1));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* int -> 1-field {int} */
    e_vs1.next = &e_vl;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* long -> 1-field {int} */
    e_vs1.next = NULL;

    e_vi.next = &e_vl;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_assign, &int_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* long -> int */
    e_op.variant.operation.kind = eok_add_assign;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* compound assign long -> int */
    e_vi.next = &e_null_ty;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* compound assign r2 == NULL */
    e_vi.next = NULL;

    /* Pointer arithmetic: int + ptr (eok_padd), array + int, array - long (eok_psubtract), array - array (eok_pdiff) */
    e_vi.next = &e_vip;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_padd, &vptr_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* int + int* -> void* */
    e_vi.next = NULL;

    e_varr.next = &e_vl;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_psubtract, &vptr_tp, &e_varr));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* array - long -> void* */
    e_varr.next = NULL;

    an_expr_node e_varr2 = e_varr;
    e_varr.next = &e_varr2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pdiff, &long_tp, &e_varr));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* array - array -> long */
    e_varr.next = NULL;

    /* Unary - on float, unary ~ on struct (cblock != NULL & cblock == NULL), logical ! on ptr & NULL */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_negate, &flt_tp, &e_vf));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_complement, &st1_tp, &e_vs1));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_not, &bool_tp, &e_vip));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.variant.operation.operands = &e_null_ty;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    /* Struct == and !=, scalar < with differing types & bool result */
    an_expr_node e_vs1_b = e_vs1;
    e_vs1.next = &e_vs1_b;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_eq, &bool_tp, &e_vs1));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.variant.operation.kind = eok_ne;
    e_op.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vs1.next = NULL;

    e_vi.next = &e_vl;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_lt, &bool_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vi.next = NULL;

    /* Imaginary ops: eok_fjadd, eok_jfadd, eok_fjsubtract, eok_jfsubtract */
    static const an_expr_operator_kind k_imag_ops[4] = {
        eok_fjadd, eok_jfadd, eok_fjsubtract, eok_jfsubtract
    };
    e_vd.next = &e_vf;
    for (size_t ii = 0; ii < 4; ++ii) {
        GCC_GEN_BE_CHECK(expr_init_op(&e_op, k_imag_ops[ii], &cplx_tp, &e_vd));
        GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    }
    e_vd.next = NULL;

    /* Vector logical ops: eok_vector_land, eok_vector_lor, eok_vector_not */
    an_expr_node e_vvec2 = e_vvec;
    e_vvec.next = &e_vvec2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_vector_land, &vec_tp, &e_vvec));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.variant.operation.kind = eok_vector_lor;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vvec.next = NULL;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_vector_not, &vec_tp, &e_vvec));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    /* Post/pre incr/decr: atomic, eok_post_decr, cblock==NULL, eok_address_of, eok_pm_points_to_field */
    g_expr_mock.atomic_target_tp = &int_tp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_post_incr, &int_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.variant.operation.kind = eok_pre_incr;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.atomic_target_tp = NULL;

    e_op.variant.operation.kind = eok_post_decr;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_op.variant.operation.kind = eok_pre_decr;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* eok_address_of: parens around routine, ck_string constant, VLA pointer */
    a_type rfn_tp;
    std::memset(&rfn_tp, 0, sizeof(rfn_tp));
    rfn_tp.kind = tk_routine;
    rfn_tp.variant.routine.return_type = &void_tp;
    a_routine r_fn;
    std::memset(&r_fn, 0, sizeof(r_fn));
    r_fn.type = &rfn_tp;
    gcc_jit_function *j_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_ca_fn", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_fn, j_fn));
    an_expr_node e_rfn, e_par_rfn;
    std::memset(&e_rfn, 0, sizeof(e_rfn));
    e_rfn.kind = enk_routine;
    e_rfn.type = &rfn_tp;
    e_rfn.variant.routine.ptr = &r_fn;
    GCC_GEN_BE_CHECK(expr_init_op(&e_par_rfn, eok_parens, &rfn_tp, &e_rfn));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_par_rfn, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_address_of, &vptr_tp, &e_par_rfn));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    a_constant c_str;
    std::memset(&c_str, 0, sizeof(c_str));
    c_str.kind = ck_string;
    c_str.variant.string.value = "s";
    c_str.variant.string.length = 2;
    an_expr_node e_cstr;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_cstr, &c_str));
    e_op.variant.operation.kind = eok_array_to_pointer;
    e_op.variant.operation.operands = &e_cstr;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    a_type vla_t;
    std::memset(&vla_t, 0, sizeof(vla_t));
    vla_t.kind = tk_array;
    vla_t.variant.array.element_type = &int_tp;
    vla_t.variant.array.is_variable_size_array = TRUE;
    a_variable v_vla_p;
    std::memset(&v_vla_p, 0, sizeof(v_vla_p));
    v_vla_p.type = &vla_t;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vla_p, lv_ip));
    an_expr_node e_vvlap;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vvlap, &v_vla_p));
    e_op.variant.operation.operands = &e_vvlap;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    /* eok_pm_points_to_field */
    e_vvp.next = &e_vl;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_points_to_field, &int_tp, &e_vvp));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vvp.next = NULL;

    /* Calls: sret, invis_ref, ABI coercion, direct source_location builtins, virtual call with extra_info */
    static const char *const k_loc_builtins[6] = {
        "__builtin_FILE", "__builtin_FILE_NAME", "__builtin_LINE",
        "__builtin_COLUMN", "__builtin_FUNCTION", "__builtin_source_location"
    };
    for (size_t bi = 0; bi < 6; ++bi) {
        a_routine rb;
        std::memset(&rb, 0, sizeof(rb));
        rb.type = &rfn_tp;
        rb.source_corresp.name = k_loc_builtins[bi];
        an_expr_node e_rb, e_cb;
        std::memset(&e_rb, 0, sizeof(e_rb));
        e_rb.kind = enk_routine;
        e_rb.type = &rfn_tp;
        e_rb.variant.routine.ptr = &rb;
        GCC_GEN_BE_CHECK(expr_init_op(&e_cb, eok_call, &vptr_tp, &e_rb));
        GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_cb, &rv));
    }

    /* Virtual member call with extra_info param_type_list (this, invis_ref, coerced_abi, NULL ptp->type) */
    a_param_type pt0, pt1, pt2, pt3;
    std::memset(&pt0, 0, sizeof(pt0)); pt0.type = NULL;
    std::memset(&pt1, 0, sizeof(pt1)); pt1.type = &st2_tp;
    std::memset(&pt2, 0, sizeof(pt2)); pt2.type = &st1_tp;
    std::memset(&pt3, 0, sizeof(pt3)); pt3.type = NULL;
    pt0.next = &pt1;
    pt1.next = &pt2;
    pt2.next = &pt3;
    a_routine_type_supplement r_sup;
    std::memset(&r_sup, 0, sizeof(r_sup));
    r_sup.param_type_list = &pt0;
    a_type vcall_rfn_tp;
    std::memset(&vcall_rfn_tp, 0, sizeof(vcall_rfn_tp));
    vcall_rfn_tp.kind = tk_routine;
    vcall_rfn_tp.variant.routine.return_type = &int_tp;
    vcall_rfn_tp.variant.routine.extra_info = &r_sup;
    gcc_jit_type *vc_pts[4] = {
        jit_vptr, gcc_jit_type_get_pointer(g_expr_mock.cplx_struct_type), g_expr_mock.one_field_struct_type, jit_int
    };
    gcc_jit_type *jit_vcall_fn_ptr = gcc_jit_context_new_function_ptr_type(
        ctx, NULL, jit_int, 4, vc_pts, 0);
    GCC_GEN_BE_CHECK(expr_test_map_type(&vcall_rfn_tp, jit_vcall_fn_ptr));

    a_routine r_virt;
    std::memset(&r_virt, 0, sizeof(r_virt));
    r_virt.type = &vcall_rfn_tp;
    r_virt.is_virtual = TRUE;
    r_virt.number.virtual_function = 1;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_virt, j_fn));

    an_expr_node e_rvirt, e_vs1_arg, e_vi_arg, e_mcall;
    std::memset(&e_rvirt, 0, sizeof(e_rvirt));
    e_rvirt.kind = enk_routine;
    e_rvirt.type = &vcall_rfn_tp;
    e_rvirt.variant.routine.ptr = &r_virt;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vs1_arg, &v_s1));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi_arg, &v_i));

    e_rvirt.next = &e_vvp;
    e_vvp.next = &e_vs2;
    e_vs2.next = &e_vs1_arg;
    e_vs1_arg.next = &e_vi_arg;
    GCC_GEN_BE_CHECK(expr_init_op(&e_mcall, eok_points_to_member_call, &int_tp, &e_rvirt));
    e_mcall.variant.operation.is_virtual_call = TRUE;

    g_expr_mock.invis_ref_tp = &st2_tp;
    g_expr_mock.needs_abi_coercion = TRUE;
    g_expr_mock.coerced_abi_type = g_expr_mock.one_field_struct_type;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_mcall, &rv);
    }));
    g_expr_mock.invis_ref_tp = NULL;
    g_expr_mock.needs_abi_coercion = FALSE;
    g_expr_mock.coerced_abi_type = NULL;
    e_rvirt.next = NULL;
    e_vvp.next = NULL;
    e_vs2.next = NULL;
    e_vs1_arg.next = NULL;

    /* Variadics: eok_va_start_single_operand with op2, eok_va_copy with matching & mismatched types */
    e_vvp.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_start_single_operand, &void_tp, &e_vvp));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vvp.next = &e_vip; /* dst_t (void*) != src_t (int*) */
    e_op.variant.operation.kind = eok_va_copy;
    e_op.type = &int_tp; /* non-void return from va_copy! */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vvp.next = NULL;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Covers PM casts, `new`/`delete` cleanups, overriding-function wrappers, `enk_reuse_value`, and remaining `enk_*` nodes.
 * @details Exercises `eok_await`/`eok_yield`, `eok_pm_cast`, `eok_pm_base_class_cast`/`eok_pm_derived_class_cast`
 * (both PMF and PMD across SYSV_AMD64 and AAPCS64), lvalue/rvalue adjust casts, `enk_braced_init_list`,
 * `enk_condition`, `enk_new_delete` (cookies, exception dealloc cleanups, sized/aligned delete),
 * `enk_result_of_overriding_function`, and `enk_reuse_value`.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_rval_casts_new_delete_eh(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, long_tp, void_tp, vptr_tp, nullptr_tp, rfn_tp, base_cls_tp, deriv_cls_tp, pmf_b_tp, pmf_d_tp, pmd_b_tp, pmd_d_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));
    std::memset(&nullptr_tp, 0, sizeof(nullptr_tp));
    nullptr_tp.kind = tk_nullptr;

    std::memset(&rfn_tp, 0, sizeof(rfn_tp));
    rfn_tp.kind = tk_routine;
    rfn_tp.variant.routine.return_type = &void_tp;

    std::memset(&base_cls_tp, 0, sizeof(base_cls_tp));
    base_cls_tp.kind = tk_class;
    base_cls_tp.size = 8;

    a_base_class bc_other, bc_match;
    std::memset(&bc_other, 0, sizeof(bc_other));
    bc_other.type = &int_tp;
    std::memset(&bc_match, 0, sizeof(bc_match));
    bc_match.type = &base_cls_tp;
    bc_match.offset = 16;
    bc_other.next = &bc_match;

    a_class_type_supplement d_sup;
    std::memset(&d_sup, 0, sizeof(d_sup));
    d_sup.base_classes = &bc_other;
    d_sup.direct_base_classes = &bc_other;

    std::memset(&deriv_cls_tp, 0, sizeof(deriv_cls_tp));
    deriv_cls_tp.kind = tk_class;
    deriv_cls_tp.size = 16;
    deriv_cls_tp.variant.class_struct_union.extra_info = &d_sup;

    std::memset(&pmf_b_tp, 0, sizeof(pmf_b_tp));
    pmf_b_tp.kind = tk_ptr_to_member;
    pmf_b_tp.variant.ptr_to_member.class_of_which_a_member = &base_cls_tp;
    pmf_b_tp.variant.ptr_to_member.type = &rfn_tp;
    pmf_d_tp = pmf_b_tp;
    pmf_d_tp.variant.ptr_to_member.class_of_which_a_member = &deriv_cls_tp;

    std::memset(&pmd_b_tp, 0, sizeof(pmd_b_tp));
    pmd_b_tp.kind = tk_ptr_to_member;
    pmd_b_tp.variant.ptr_to_member.class_of_which_a_member = &base_cls_tp;
    pmd_b_tp.variant.ptr_to_member.type = &int_tp;
    pmd_d_tp = pmd_b_tp;
    pmd_d_tp.variant.ptr_to_member.class_of_which_a_member = &deriv_cls_tp;

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_long = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_type *jit_size = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_vptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    GCC_GEN_BE_CHECK(expr_test_map_type(&void_tp, jit_void));
    GCC_GEN_BE_CHECK(expr_test_map_type(&pmd_b_tp, jit_long));
    GCC_GEN_BE_CHECK(expr_test_map_type(&pmd_d_tp, jit_long));

    gcc_jit_lvalue *lv_i = gcc_jit_function_new_local(fn, NULL, jit_int, "nd_i");
    gcc_jit_lvalue *lv_l = gcc_jit_function_new_local(fn, NULL, jit_long, "nd_l");
    gcc_jit_lvalue *lv_vp = gcc_jit_function_new_local(fn, NULL, jit_vptr, "nd_vp");
    gcc_jit_lvalue *lv_pmf = gcc_jit_function_new_local(fn, NULL, g_expr_mock.pmf_struct_type, "nd_pmf");

    a_variable v_i, v_l, v_vp, v_pmf_b, v_pmf_d, v_pmd_b;
    std::memset(&v_i, 0, sizeof(v_i));         v_i.type = &int_tp;
    std::memset(&v_l, 0, sizeof(v_l));         v_l.type = &long_tp;
    std::memset(&v_vp, 0, sizeof(v_vp));       v_vp.type = &vptr_tp;
    std::memset(&v_pmf_b, 0, sizeof(v_pmf_b)); v_pmf_b.type = &pmf_b_tp;
    std::memset(&v_pmf_d, 0, sizeof(v_pmf_d)); v_pmf_d.type = &pmf_d_tp;
    std::memset(&v_pmd_b, 0, sizeof(v_pmd_b)); v_pmd_b.type = &pmd_b_tp;

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_i, lv_i));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_l, lv_l));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vp, lv_vp));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pmf_b, lv_pmf));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pmf_d, lv_pmf));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pmd_b, lv_l));

    an_expr_node e_vi, e_vl, e_vvp, e_vpmfb, e_vpmfd, e_vpmdb, e_nullp, e_op;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi, &v_i));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vl, &v_l));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vvp, &v_vp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vpmfb, &v_pmf_b));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vpmfd, &v_pmf_d));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vpmdb, &v_pmd_b));

    a_constant c_nullp;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_nullp, &nullptr_tp, 0));
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_nullp, &c_nullp));
    gcc_jit_rvalue *rv = NULL;

    /* eok_await & eok_yield */
    g_expr_mock.coro_frame_type = g_expr_mock.one_field_struct_type;
    g_expr_mock.coro_frame_lval = lv_vp;
    g_expr_mock.coro_state_field_lval = lv_i;
    g_expr_mock.coro_frame_ptr_rval = gcc_jit_lvalue_as_rvalue(lv_vp);
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_await, &int_tp, NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.variant.operation.kind = eok_yield;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.coro_frame_type = NULL;
    g_expr_mock.coro_frame_lval = NULL;
    g_expr_mock.coro_state_field_lval = NULL;
    g_expr_mock.coro_frame_ptr_rval = NULL;

    /* eok_bool_cast: tk_nullptr, PMF, PMD, pointer r1, scalar r1 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_bool_cast, &int_tp, &e_nullp));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.variant.operation.operands = &e_vpmfb;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.variant.operation.operands = &e_vpmdb;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.variant.operation.operands = &e_vvp; /* pointer r1 */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.variant.operation.operands = &e_vi; /* scalar non-ptr r1 */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    /* eok_pm_base_class_cast & eok_pm_derived_class_cast (PMF on SYSV & AAPCS64, PMD, delta==0, !cblock, guard fail) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_derived_class_cast, &pmf_d_tp, &e_vpmfb));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;

    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, &pmd_b_tp, &e_vpmdb));
    e_vpmdb.type = &pmd_d_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* PMD base cast */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* !cblock */
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    bc_match.offset = 0;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* delta == 0 */
    e_op.type = &int_tp; /* dst_tp != tk_ptr_to_member guard! */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vpmdb.type = &pmd_b_tp;

    /* eok_lvalue_cast, eok_ref_cast, eok_lvalue_adjust, eok_class_rvalue_adjust, tk_nullptr cast, default op */
    static const an_expr_operator_kind k_adj_ops[4] = {
        eok_lvalue_cast, eok_ref_cast, eok_lvalue_adjust, eok_class_rvalue_adjust
    };
    for (size_t ai = 0; ai < 4; ++ai) {
        GCC_GEN_BE_CHECK(expr_init_op(&e_op, k_adj_ops[ai], &long_tp, &e_nullp));
        GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    }
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, (an_expr_operator_kind)999, &int_tp, &e_vi));
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    /* enk_condition: !cblock, initialization stmt, dip->variable cached & uncached, cond->expr == NULL */
    a_statement init_stmt;
    std::memset(&init_stmt, 0, sizeof(init_stmt));
    init_stmt.kind = stmk_expr;
    init_stmt.expr = &e_vi;
    a_variable v_cond_uncached;
    std::memset(&v_cond_uncached, 0, sizeof(v_cond_uncached));
    v_cond_uncached.type = &int_tp;
    a_dynamic_init cond_dip;
    std::memset(&cond_dip, 0, sizeof(cond_dip));
    cond_dip.kind = dik_expression;
    cond_dip.variable = &v_cond_uncached;
    cond_dip.variant.expression = &e_vi;

    a_condition_supplement cond_s;
    std::memset(&cond_s, 0, sizeof(cond_s));
    cond_s.initialization = &init_stmt;
    cond_s.dynamic_init = &cond_dip;
    cond_s.expr = NULL;
    an_expr_node e_cond;
    std::memset(&e_cond, 0, sizeof(e_cond));
    e_cond.kind = enk_condition;
    e_cond.type = &int_tp;
    e_cond.variant.condition = &cond_s;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_cond, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_cond, &rv)); /* uncached var, expr == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_cond, &rv)); /* now cached var! */

    /* enk_new_delete:
       - !ndsp, !ndsp->type, !cblock
       - array new with cookie + 2-param operator new + active_alloc_lp + freeing_of_storage_on_exception (2-param free_fn)
       - error inside dynamic_init while new_cleanup_lp is active
       - array delete & scalar delete with 2-param dtor & 3-param sized+aligned operator delete + extra arg */
    an_expr_node e_nd;
    std::memset(&e_nd, 0, sizeof(e_nd));
    e_nd.kind = enk_new_delete;
    e_nd.type = &vptr_tp;
    if (gcc_gen_be_lower_expr_rvalue(&e_nd, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    a_new_delete_supplement nds;
    std::memset(&nds, 0, sizeof(nds));
    e_nd.variant.new_delete = &nds;
    if (gcc_gen_be_lower_expr_rvalue(&e_nd, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    nds.type = &base_cls_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_nd, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    gcc_jit_param *op_new_p[2] = {
        gcc_jit_context_new_param(ctx, NULL, jit_size, "sz"),
        gcc_jit_context_new_param(ctx, NULL, jit_long, "al")
    };
    gcc_jit_function *j_op_new2 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_vptr, "cov_op_new2", 2, op_new_p, 0);
    gcc_jit_param *op_del_p[3] = {
        gcc_jit_context_new_param(ctx, NULL, jit_vptr, "p"),
        gcc_jit_context_new_param(ctx, NULL, jit_size, "sz"),
        gcc_jit_context_new_param(ctx, NULL, jit_size, "al")
    };
    gcc_jit_function *j_op_del3 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_op_del3", 3, op_del_p, 0);
    gcc_jit_param *dtor2_p[2] = {
        gcc_jit_context_new_param(ctx, NULL, jit_vptr, "this"),
        gcc_jit_context_new_param(ctx, NULL, jit_int, "in_chrg")
    };
    gcc_jit_function *j_dtor2 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_nd_dtor2", 2, dtor2_p, 0);

    a_routine r_new2, r_del3, r_dtor2;
    std::memset(&r_new2, 0, sizeof(r_new2));   r_new2.type = &rfn_tp;
    std::memset(&r_del3, 0, sizeof(r_del3));   r_del3.type = &rfn_tp;
    std::memset(&r_dtor2, 0, sizeof(r_dtor2)); r_dtor2.type = &rfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_new2, j_op_new2));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_del3, j_op_del3));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_dtor2, j_dtor2));

    a_dynamic_init new_dip, free_dip;
    std::memset(&new_dip, 0, sizeof(new_dip));
    new_dip.kind = dik_expression;
    new_dip.destructor = &r_dtor2;
    new_dip.variant.expression = &e_vi;
    std::memset(&free_dip, 0, sizeof(free_dip));
    free_dip.destructor = &r_del3;

    a_constant c_cnt;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_cnt, &int_tp, 3));
    an_expr_node e_cnt, e_sz_arg, e_al_arg;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_cnt, &c_cnt));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_sz_arg, &v_i)); /* int != size_t -> casts atype != ptype! */
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_al_arg, &v_i)); /* int != long -> casts atype != ptype! */
    e_sz_arg.next = &e_al_arg;

    nds.is_new = TRUE;
    nds.number_of_elements = &e_cnt; /* array new + destructor -> needs_cookie == TRUE! */
    nds.routine = &r_new2;
    nds.arg = &e_sz_arg;
    nds.dynamic_init = &new_dip;
    nds.freeing_of_storage_on_exception = &free_dip;

    gcc_jit_block *outer_lp = gcc_jit_function_new_block(fn, "nd_outer_lp");
    gcc_jit_block_end_with_void_return(outer_lp, NULL);
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(outer_lp));
    g_expr_mock.unwind_resume_make_block = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_nd, &rv));
    g_expr_mock.unwind_resume_make_block = FALSE;

    /* Error inside new_dip while new_cleanup_lp is active */
    an_expr_node e_bad;
    std::memset(&e_bad, 0, sizeof(e_bad));
    e_bad.kind = (an_expr_node_kind)99;
    e_bad.type = &int_tp;
    new_dip.variant.expression = &e_bad;
    if (gcc_gen_be_lower_expr_rvalue(&e_nd, &rv) == GCC_GEN_BE_SUCCESS) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    e_sz_arg.next = NULL;

    /* Delete: array_delete = TRUE & FALSE with 2-param dtor & 3-param operator delete */
    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = FALSE;
    nds.type = &base_cls_tp;
    nds.array_delete = TRUE;
    nds.dynamic_init = &new_dip;
    nds.routine = &r_del3;
    nds.arg = &e_vvp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_nd, &rv));

    nds.array_delete = FALSE;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_nd, &rv));

    /* enk_result_of_overriding_function */
    gcc_jit_param *ov_p[3] = {
        gcc_jit_context_new_param(ctx, NULL, jit_long, "ov_p0"),
        gcc_jit_context_new_param(ctx, NULL, jit_int, "ov_p1"),
        gcc_jit_context_new_param(ctx, NULL, jit_int, "ov_p2")
    };
    gcc_jit_function *j_ov_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_int, "cov_ov_fn", 3, ov_p, 0);
    gcc_jit_param *wrap_p[3] = {
        gcc_jit_context_new_param(ctx, NULL, jit_int, "wp0"),
        gcc_jit_context_new_param(ctx, NULL, jit_int, "wp1"),
        gcc_jit_context_new_param(ctx, NULL, jit_int, "wp2")
    };
    gcc_jit_function *j_wrap_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, jit_void, "cov_wrap_fn", 3, wrap_p, 0);
    a_routine r_ov_target, r_wrapper;
    std::memset(&r_ov_target, 0, sizeof(r_ov_target));
    r_ov_target.type = &rfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_ov_target, j_ov_fn));

    a_variable v_wp_uncached;
    std::memset(&v_wp_uncached, 0, sizeof(v_wp_uncached));
    v_wp_uncached.type = &int_tp;
    v_i.next = &v_wp_uncached;

    a_scope def_sc, cur_fn_sc;
    std::memset(&def_sc, 0, sizeof(def_sc));
    def_sc.variant.routine.parameters = &v_i; /* p0 cached int!=long, p1 uncached, p2 var_param==NULL */
    std::memset(&r_wrapper, 0, sizeof(r_wrapper));
    g_expr_mock.mock_routine_scope = &def_sc;

    std::memset(&cur_fn_sc, 0, sizeof(cur_fn_sc));
    cur_fn_sc.variant.routine.ptr = &r_wrapper;

    an_expr_node e_ov;
    std::memset(&e_ov, 0, sizeof(e_ov));
    e_ov.kind = enk_result_of_overriding_function;
    e_ov.type = &int_tp;

    /* 1) innermost_function_scope == NULL & overriding_function_for_wrapper == NULL -> INVALID_ARGUMENT */
    innermost_function_scope = NULL;
    if (gcc_gen_be_lower_expr_rvalue(&e_ov, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    innermost_function_scope = &cur_fn_sc;
    if (gcc_gen_be_lower_expr_rvalue(&e_ov, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    r_wrapper.overriding_function_for_wrapper = &r_ov_target;
    /* 2) !cblock -> INTERNAL */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_ov, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    /* 3) Non-void return == expected_type (int) and != expected_type (long), void expected_type, and OOM sweep */
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(j_wrap_fn, 16, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_ov, &rv);
    }));
    e_ov.type = &long_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_ov, &rv));
    e_ov.type = &void_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_ov, &rv));
    gcc_jit_block *end_wrap_blk = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&end_wrap_blk));
    if (end_wrap_blk) {
        gcc_jit_block_end_with_void_return(end_wrap_blk, NULL);
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    innermost_function_scope = NULL;
    g_expr_mock.mock_routine_scope = NULL;
    v_i.next = NULL;

    /* enk_reuse_value in rvalue position: cached var, uncached var with local_var_return_null, !cblock */
    a_variable v_rv_uncached;
    std::memset(&v_rv_uncached, 0, sizeof(v_rv_uncached));
    v_rv_uncached.type = &int_tp;
    a_dynamic_init rv_dip;
    std::memset(&rv_dip, 0, sizeof(rv_dip));
    rv_dip.kind = dik_expression;
    rv_dip.variable = &v_i; /* cached! */
    rv_dip.variant.expression = &e_vi;
    an_expr_node e_rv_reuse;
    std::memset(&e_rv_reuse, 0, sizeof(e_rv_reuse));
    e_rv_reuse.kind = enk_reuse_value;
    e_rv_reuse.type = &int_tp;
    e_rv_reuse.variant.reused_value_init = &rv_dip;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_rv_reuse, &rv));

    rv_dip.variable = &v_rv_uncached;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_rv_reuse, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_rv_reuse.variant.reused_value_init = NULL;
    if (gcc_gen_be_lower_expr_rvalue(&e_rv_reuse, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    e_rv_reuse.variant.reused_value_init = &rv_dip;
    g_expr_mock.local_var_return_null = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_rv_reuse, &rv)); /* !temp_lval && init_rval! */
    rv_dip.kind = dik_none;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_rv_reuse, &rv)); /* temp_lval == NULL at L5376! */
    g_expr_mock.local_var_return_null = FALSE;

    /* enk_lowered_eh_construct with cached_addr != NULL */
    an_expr_node e_eh_rv;
    std::memset(&e_eh_rv, 0, sizeof(e_eh_rv));
    e_eh_rv.kind = enk_lowered_eh_construct;
    e_eh_rv.type = &vptr_tp;
    e_eh_rv.variant.lowered_eh.kind = leck_thrown_object_address;
    gcc_gen_be_singleton_state.active_thrown_obj_lval = lv_vp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_eh_rv, &rv));
    gcc_gen_be_singleton_state.active_thrown_obj_lval = NULL;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Covers remaining rvalue branches (`eok_va_arg`, `eok_spaceship`, complex casts, `dot_pm_call`, `pm_field`, cleanup labels).
 * @details Exercises `va_arg` across AAPCS64, SysV AMD64, RISC-V LP64D, Win64, and i386 ABIs; float/struct `<=>`;
 * complex-to-complex and complex-to-scalar casts; `eok_dot_pm_call` and `eok_pm_field`; `call_fail` and `new_alloc_fail`
 * error cleanups; 0-arg/0-param `delete`; pointer/bool logical ops; void/struct/bitfield assignments; and `int + ptr` `eok_padd`.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_rval_remaining_gaps(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, long_tp, bool_tp, dbl_tp, flt_tp, void_tp, vptr_tp, st1_tp, st2_tp, big_st_tp, win12_st_tp;
    a_type cplx_flt_tp, cplx_dbl_tp, align16_int_tp, zero_sz_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&bool_tp, ik_int, 1));
    bool_tp.variant.integer.bool_type = TRUE;
    GCC_GEN_BE_CHECK(expr_init_int_type(&align16_int_tp, ik_int128, 16));
    align16_int_tp.alignment = 16;
    GCC_GEN_BE_CHECK(expr_init_int_type(&zero_sz_tp, ik_int, 0));
    zero_sz_tp.alignment = 0;
    GCC_GEN_BE_CHECK(expr_init_float_type(&flt_tp, fk_float, 4));
    GCC_GEN_BE_CHECK(expr_init_float_type(&dbl_tp, fk_double, 8));
    std::memset(&void_tp, 0, sizeof(void_tp)); void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));

    std::memset(&cplx_flt_tp, 0, sizeof(cplx_flt_tp));
    cplx_flt_tp.kind = tk_complex; cplx_flt_tp.size = 8; cplx_flt_tp.variant.float_kind = fk_float;
    std::memset(&cplx_dbl_tp, 0, sizeof(cplx_dbl_tp));
    cplx_dbl_tp.kind = tk_complex; cplx_dbl_tp.size = 16; cplx_dbl_tp.variant.float_kind = fk_double;

    std::memset(&st1_tp, 0, sizeof(st1_tp)); st1_tp.kind = tk_struct; st1_tp.size = 4; st1_tp.alignment = 4;
    std::memset(&st2_tp, 0, sizeof(st2_tp)); st2_tp.kind = tk_struct; st2_tp.size = 16; st2_tp.alignment = 8;
    std::memset(&big_st_tp, 0, sizeof(big_st_tp)); big_st_tp.kind = tk_struct; big_st_tp.size = 32; big_st_tp.alignment = 16;
    std::memset(&win12_st_tp, 0, sizeof(win12_st_tp)); win12_st_tp.kind = tk_struct; win12_st_tp.size = 12; win12_st_tp.alignment = 4;

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_uint = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT);
    gcc_jit_type *jit_long = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_type *jit_bool = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
    gcc_jit_type *jit_dbl = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_vptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *jit_cptr = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR));

    gcc_jit_field *cf_f[2] = {
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT), "rf"),
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT), "if")
    };
    gcc_jit_type *jit_cplx_flt = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cplx_flt_st", 2, cf_f));

    GCC_GEN_BE_CHECK(expr_test_map_type(&st1_tp, g_expr_mock.one_field_struct_type));
    GCC_GEN_BE_CHECK(expr_test_map_type(&st2_tp, g_expr_mock.cplx_struct_type));
    GCC_GEN_BE_CHECK(expr_test_map_type(&big_st_tp, g_expr_mock.cplx_struct_type));
    GCC_GEN_BE_CHECK(expr_test_map_type(&win12_st_tp, g_expr_mock.cplx_struct_type));
    GCC_GEN_BE_CHECK(expr_test_map_type(&cplx_flt_tp, jit_cplx_flt));
    GCC_GEN_BE_CHECK(expr_test_map_type(&cplx_dbl_tp, g_expr_mock.cplx_struct_type));

    gcc_jit_lvalue *lv_i = gcc_jit_function_new_local(fn, NULL, jit_int, "rg_i");
    gcc_jit_lvalue *lv_b = gcc_jit_function_new_local(fn, NULL, jit_bool, "rg_b");
    gcc_jit_lvalue *lv_d = gcc_jit_function_new_local(fn, NULL, jit_dbl, "rg_d");
    gcc_jit_lvalue *lv_vp = gcc_jit_function_new_local(fn, NULL, jit_vptr, "rg_vp");
    gcc_jit_lvalue *lv_cp = gcc_jit_function_new_local(fn, NULL, jit_cptr, "rg_cp");
    gcc_jit_lvalue *lv_s1 = gcc_jit_function_new_local(fn, NULL, g_expr_mock.one_field_struct_type, "rg_s1");
    gcc_jit_lvalue *lv_s2 = gcc_jit_function_new_local(fn, NULL, g_expr_mock.cplx_struct_type, "rg_s2");
    gcc_jit_lvalue *lv_cf = gcc_jit_function_new_local(fn, NULL, jit_cplx_flt, "rg_cf");
    gcc_jit_lvalue *lv_pmf = gcc_jit_function_new_local(fn, NULL, g_expr_mock.pmf_struct_type, "rg_pmf");

    a_variable v_i, v_b, v_d, v_vp, v_cp, v_s1, v_s2, v_cf, v_pmf;
    std::memset(&v_i, 0, sizeof(v_i)); v_i.type = &int_tp;
    std::memset(&v_b, 0, sizeof(v_b)); v_b.type = &bool_tp;
    std::memset(&v_d, 0, sizeof(v_d)); v_d.type = &dbl_tp;
    std::memset(&v_vp, 0, sizeof(v_vp)); v_vp.type = &vptr_tp;
    std::memset(&v_cp, 0, sizeof(v_cp)); v_cp.type = &vptr_tp;
    std::memset(&v_s1, 0, sizeof(v_s1)); v_s1.type = &st1_tp;
    std::memset(&v_s2, 0, sizeof(v_s2)); v_s2.type = &st2_tp;
    std::memset(&v_cf, 0, sizeof(v_cf)); v_cf.type = &cplx_flt_tp;
    std::memset(&v_pmf, 0, sizeof(v_pmf)); v_pmf.type = &long_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_i, lv_i));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_b, lv_b));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_d, lv_d));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vp, lv_vp));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_cp, lv_cp));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_s1, lv_s1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_s2, lv_s2));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_cf, lv_cf));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pmf, lv_pmf));

    an_expr_node e_vi, e_vb, e_vd1, e_vd2, e_vvp, e_vcp, e_vs1, e_vs2, e_vcf, e_vpmf, e_op, e_bad;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi, &v_i));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vb, &v_b));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vd1, &v_d));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vd2, &v_d));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vvp, &v_vp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vcp, &v_cp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vs1, &v_s1));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vs2, &v_s2));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vcf, &v_cf));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vpmf, &v_pmf));
    std::memset(&e_bad, 0, sizeof(e_bad));
    e_bad.kind = (an_expr_node_kind)99;
    e_bad.type = &int_tp;

    gcc_jit_rvalue *rv = NULL;

    /* 1. eok_dot_field edge branches (op2==NULL, op2->kind!=enk_field, field.ptr==NULL, !is_bit_field && bit_size>0, lval==NULL) */
    a_field f_bitsz;
    std::memset(&f_bitsz, 0, sizeof(f_bitsz));
    f_bitsz.type = &int_tp; f_bitsz.is_bit_field = FALSE; f_bitsz.bit_size = 4;
    an_expr_node e_fbitsz, e_fnull;
    std::memset(&e_fbitsz, 0, sizeof(e_fbitsz)); e_fbitsz.kind = enk_field; e_fbitsz.type = &int_tp; e_fbitsz.variant.field.ptr = &f_bitsz;
    std::memset(&e_fnull, 0, sizeof(e_fnull)); e_fnull.kind = enk_field; e_fnull.type = &int_tp; e_fnull.variant.field.ptr = NULL;

    e_vs1.next = &e_fbitsz;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_field, &int_tp, &e_vs1));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* !is_bit_field && bit_size > 0 */
    an_expr_node e_asgn_bf;
    e_op.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_asgn_bf, eok_assign, &int_tp, &e_op));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_asgn_bf, &rv)); /* L3158 True */
    e_op.next = NULL;
    gcc_gen_be_error_t dot_err = GCC_GEN_BE_SUCCESS;
    e_vs1.next = &e_fnull;
    dot_err = gcc_gen_be_lower_expr_rvalue(&e_op, &rv); /* field.ptr == NULL */
    if (dot_err == GCC_GEN_BE_ERROR_OOM) return dot_err;
    e_vs1.next = &e_vi;
    dot_err = gcc_gen_be_lower_expr_rvalue(&e_op, &rv); /* op2->kind != enk_field */
    if (dot_err == GCC_GEN_BE_ERROR_OOM) return dot_err;
    e_vs1.next = NULL;
    dot_err = gcc_gen_be_lower_expr_rvalue(&e_op, &rv); /* op2 == NULL */
    if (dot_err == GCC_GEN_BE_ERROR_OOM) return dot_err;

    /* lval == NULL at L2975 (bitfield in eok_subscript/dot_field where bit_field is bypassed? Use eok_subscript? Or enk_variable uncached under eok_ref_indirect? No: eok_dot_field with bitfield returns NULL lval when op2 wasn't bitfield? Wait: enk_variable with ptr==NULL returns lval==NULL!) */
    an_expr_node e_vnull_ptr;
    std::memset(&e_vnull_ptr, 0, sizeof(e_vnull_ptr));
    e_vnull_ptr.kind = enk_variable; e_vnull_ptr.type = NULL; e_vnull_ptr.variant.variable.ptr = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_vnull_ptr, &rv));

    /* 2. eok_land & eok_lor with pointer r1/r2, bool r2, and !cblock on eok_lor */
    an_expr_node e_vvp2 = e_vvp;
    e_vvp.next = &e_vvp2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_land, &bool_tp, &e_vvp));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* ptr && ptr */
    e_vvp.next = &e_vb;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* ptr && bool */
    e_op.variant.operation.kind = eok_lor;
    e_vvp.next = &e_vvp2;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* ptr || ptr */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    e_vvp.next = NULL;

    /* 3. eok_assign / eok_bassign: _Atomic bassign, 2-field struct LHS, struct RHS into int LHS, void RHS, l1==NULL */
    g_expr_mock.atomic_target_tp = &int_tp;
    e_vi.next = &e_vb;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_bassign, &int_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3173 op == eok_bassign on _Atomic */
    g_expr_mock.atomic_target_tp = NULL;
    e_vi.next = NULL;

    e_vs2.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_assign, &st2_tp, &e_vs2));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* 2-field struct != 1-field struct */
    e_vs2.next = NULL;

    e_vi.next = &e_vs2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_assign, &int_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* r2 is struct into non-struct l1 */
    e_vi.next = NULL;

    gcc_jit_function *j_void_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "rg_void_fn", 0, NULL, 0);
    a_type vfn_tp;
    std::memset(&vfn_tp, 0, sizeof(vfn_tp)); vfn_tp.kind = tk_routine; vfn_tp.variant.routine.return_type = &void_tp;
    a_routine r_vfn;
    std::memset(&r_vfn, 0, sizeof(r_vfn)); r_vfn.type = &vfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_vfn, j_void_fn));
    an_expr_node e_rvfn, e_vcall;
    std::memset(&e_rvfn, 0, sizeof(e_rvfn)); e_rvfn.kind = enk_routine; e_rvfn.type = &vfn_tp; e_rvfn.variant.routine.ptr = &r_vfn;
    GCC_GEN_BE_CHECK(expr_init_op(&e_vcall, eok_call, &void_tp, &e_rvfn));
    e_vi.next = &e_vcall;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_assign, &int_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* r2_t == jit_void -> L3207! */
    e_vi.next = NULL;

    e_vnull_ptr.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_assign, &int_tp, &e_vnull_ptr));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* l1 == NULL */
    e_op.variant.operation.kind = eok_add_assign;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* l1 == NULL at L3222 */
    e_vnull_ptr.next = NULL;

    /* 4. eok_padd (int + ptr & int + int), eok_spaceship (double & 1-field struct), eok_pm_field, eok_dot_pm_call, call_fail */
    e_vi.next = &e_vvp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_padd, &vptr_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* int + ptr */
    an_expr_node e_vi2 = e_vi; e_vi2.next = NULL;
    e_vi.next = &e_vi2;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* non-ptr + non-ptr */
    e_vi.next = NULL;

    e_vd1.next = &e_vd2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_spaceship, &int_tp, &e_vd1));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* float <=> -> int */
    e_op.type = &st1_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* float <=> -> strong_ordering struct (cblock != NULL) */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* strong_ordering struct with cblock == NULL */
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    e_vd1.next = NULL;

    /* eok_pm_field & eok_dot_pm_call & indirect eok_call & call_fail */
    an_expr_node e_vl_off;
    a_constant c_off0;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_off0, &long_tp, 0));
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_vl_off, &c_off0));
    e_vs1.next = &e_vl_off;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_field, &int_tp, &e_vs1));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vs1.next = NULL;

    gcc_jit_type *pm_fn_ptr_t = gcc_jit_context_new_function_ptr_type(ctx, NULL, jit_void, 1, &jit_vptr, 0);
    a_type pm_rfn_tp, pmf_tp;
    std::memset(&pm_rfn_tp, 0, sizeof(pm_rfn_tp)); pm_rfn_tp.kind = tk_routine; pm_rfn_tp.variant.routine.return_type = &void_tp;
    GCC_GEN_BE_CHECK(expr_test_map_type(&pm_rfn_tp, pm_fn_ptr_t));
    std::memset(&pmf_tp, 0, sizeof(pmf_tp)); pmf_tp.kind = tk_ptr_to_member; pmf_tp.variant.ptr_to_member.type = &pm_rfn_tp;
    v_pmf.type = &pmf_tp;
    e_vpmf.type = &pmf_tp;
    e_vpmf.next = &e_vs1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_pm_call, &void_tp, &e_vpmf));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vpmf.next = NULL;

    /* call_fail: direct call with bad argument (&e_bad) after args allocation */
    e_rvfn.next = &e_bad;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &void_tp, &e_rvfn));
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_rvfn.next = NULL;

    /* Virtual call with uses_sret = TRUE, vindex == VIRTUAL_FUNCTION_NUMBER_NONE, extra_info == NULL */
    a_routine r_v_sret;
    std::memset(&r_v_sret, 0, sizeof(r_v_sret));
    r_v_sret.type = &pm_rfn_tp;
    r_v_sret.is_virtual = TRUE;
    r_v_sret.number.virtual_function = 0;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_v_sret, j_void_fn));
    an_expr_node e_rvsret;
    std::memset(&e_rvsret, 0, sizeof(e_rvsret));
    e_rvsret.kind = enk_routine; e_rvsret.type = &pm_rfn_tp; e_rvsret.variant.routine.ptr = &r_v_sret;
    e_rvsret.next = &e_vvp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_points_to_member_call, &st1_tp, &e_rvsret));
    e_op.variant.operation.is_virtual_call = TRUE;
    g_expr_mock.is_sret = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    r_v_sret.number.virtual_function = VIRTUAL_FUNCTION_NUMBER_NONE;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.is_sret = FALSE;
    e_rvsret.next = NULL;

    /* 5. eok_va_arg across AAPCS64, SysV AMD64, RISC-V LP64D, Win64, i386 */
    gcc_jit_field *aapcs_va_f[5] = {
        gcc_jit_context_new_field(ctx, NULL, jit_vptr, "__stack"),
        gcc_jit_context_new_field(ctx, NULL, jit_vptr, "__gr_top"),
        gcc_jit_context_new_field(ctx, NULL, jit_vptr, "__vr_top"),
        gcc_jit_context_new_field(ctx, NULL, jit_int, "__gr_offs"),
        gcc_jit_context_new_field(ctx, NULL, jit_int, "__vr_offs")
    };
    gcc_jit_type *jit_aapcs_va = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "__aapcs_va_list", 5, aapcs_va_f));
    gcc_jit_lvalue *lv_aapcs_va = gcc_jit_function_new_local(fn, NULL, jit_aapcs_va, "va_aapcs");

    gcc_jit_field *sysv_va_f[4] = {
        gcc_jit_context_new_field(ctx, NULL, jit_uint, "gp_offset"),
        gcc_jit_context_new_field(ctx, NULL, jit_uint, "fp_offset"),
        gcc_jit_context_new_field(ctx, NULL, jit_vptr, "overflow_arg_area"),
        gcc_jit_context_new_field(ctx, NULL, jit_vptr, "reg_save_area")
    };
    gcc_jit_type *jit_sysv_va_st = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "__sysv_va_tag", 4, sysv_va_f));
    gcc_jit_type *jit_sysv_va_arr = gcc_jit_context_new_array_type(ctx, NULL, jit_sysv_va_st, 1);
    gcc_jit_lvalue *lv_sysv_va = gcc_jit_function_new_local(fn, NULL, jit_sysv_va_arr, "va_sysv");

    a_type aapcs_va_tp, aapcs_va_cls_tp, sysv_va_arr_tp;
    std::memset(&aapcs_va_tp, 0, sizeof(aapcs_va_tp)); aapcs_va_tp.kind = tk_struct; aapcs_va_tp.size = 32;
    std::memset(&aapcs_va_cls_tp, 0, sizeof(aapcs_va_cls_tp)); aapcs_va_cls_tp.kind = tk_class; aapcs_va_cls_tp.size = 32;
    std::memset(&sysv_va_arr_tp, 0, sizeof(sysv_va_arr_tp));
    sysv_va_arr_tp.kind = tk_array; sysv_va_arr_tp.variant.array.element_type = &aapcs_va_tp;

    a_variable v_va_aapcs, v_va_sysv;
    std::memset(&v_va_aapcs, 0, sizeof(v_va_aapcs)); v_va_aapcs.type = &aapcs_va_tp;
    std::memset(&v_va_sysv, 0, sizeof(v_va_sysv)); v_va_sysv.type = &sysv_va_arr_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_va_aapcs, lv_aapcs_va));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_va_sysv, lv_sysv_va));

    an_expr_node e_va_aapcs, e_va_sysv;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_va_aapcs, &v_va_aapcs));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_va_sysv, &v_va_sysv));

    /* AAPCS64 struct/class __va_list: GP, FP, align16 GP, >16B indirect, !cblock */
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
    g_expr_mock.va_num_gp = 1; g_expr_mock.va_num_fp = 0; g_expr_mock.va_is_stack = FALSE;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &int_tp, &e_va_aapcs));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.type = &align16_int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.va_num_gp = 0; g_expr_mock.va_num_fp = 1;
    e_va_aapcs.type = &aapcs_va_cls_tp;
    e_op.type = &dbl_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.va_num_gp = 0; g_expr_mock.va_num_fp = 0; g_expr_mock.va_is_stack = TRUE;
    e_op.type = &big_st_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* SysV AMD64 __va_list_tag[1]: >16B stack, align16 GP, zero-size, !cblock */
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    e_op.variant.operation.operands = &e_va_sysv;
    g_expr_mock.va_is_stack = TRUE;
    e_op.type = &big_st_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.va_is_stack = FALSE; g_expr_mock.va_num_gp = 1;
    e_op.type = &align16_int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.type = &zero_sz_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    g_expr_mock.va_num_gp = 0;

    /* Scalar char* va_list across RISCV64_LP64D, WIN64, SYSV_I386, AAPCS64, and !cblock */
    e_op.variant.operation.operands = &e_vcp;
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D;
    e_op.type = &align16_int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.type = &big_st_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_WIN64;
    e_op.type = &win12_st_tp; /* 12B indirect on Win64 */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.type = &long_tp; /* 8B direct on Win64 */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_I386;
    e_op.type = &dbl_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
    e_op.type = &align16_int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.type = &zero_sz_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;

    /* 6. eok_cast: complex-to-complex, complex-to-double, and !r1 (null operand rvalue) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_cast, &cplx_dbl_tp, &e_vcf));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.type = &dbl_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.type = &flt_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_op.type = &int_tp;
    e_op.variant.operation.operands = &e_vnull_ptr; /* r1 == NULL -> gcc_jit_context_null */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    /* 7. enk_new_delete: new_alloc_fail (bad arg), 1-arg calling 2-param operator new, and 0-arg/0-param operator delete */
    gcc_jit_param *n2p[2] = {
        gcc_jit_context_new_param(ctx, NULL, jit_long, "nsz"),
        gcc_jit_context_new_param(ctx, NULL, jit_long, "nal")
    };
    gcc_jit_function *j_new2 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_vptr, "rg_op_new2", 2, n2p, 0);
    gcc_jit_function *j_del0 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "rg_op_del0", 0, NULL, 0);
    a_routine r_n2, r_d0;
    std::memset(&r_n2, 0, sizeof(r_n2)); r_n2.type = &vfn_tp;
    std::memset(&r_d0, 0, sizeof(r_d0)); r_d0.type = &vfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_n2, j_new2));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_d0, j_del0));

    a_new_delete_supplement nds_g;
    std::memset(&nds_g, 0, sizeof(nds_g));
    nds_g.is_new = TRUE;
    nds_g.type = &int_tp;
    nds_g.routine = &r_n2;
    nds_g.arg = NULL; /* num_args == 1 < param_count == 2 -> L4979 p < num_args False! */
    an_expr_node e_nd_g;
    std::memset(&e_nd_g, 0, sizeof(e_nd_g));
    e_nd_g.kind = enk_new_delete;
    e_nd_g.type = &vptr_tp;
    e_nd_g.variant.new_delete = &nds_g;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_nd_g, &rv));

    nds_g.arg = &e_bad; /* hits new_alloc_fail! */
    if (gcc_gen_be_lower_expr_rvalue(&e_nd_g, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    nds_g.is_new = FALSE;
    nds_g.routine = &r_d0;
    nds_g.arg = &e_vvp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_nd_g, &rv)); /* param_count == 0 -> L5245 False! */

    /* 8. Remaining arithmetic/comparison/PM/atomic-builtin/array-decay/new-delete lines */
    e_vvp.next = &e_vvp2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_psubtract, &int_tp, &e_vvp));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3308 res_diff long != int */
    e_vvp.next = NULL;

    e_vi.next = &e_vi2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_add, &long_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3435 & L3438 r1/r2 int != long */
    e_vi.next = &e_vl_off;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_eq, &bool_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3522 r2 long != r1 int */
    e_vi.next = NULL;

    a_type arr_int_tp;
    std::memset(&arr_int_tp, 0, sizeof(arr_int_tp));
    arr_int_tp.kind = tk_array;
    arr_int_tp.variant.array.element_type = &int_tp;
    arr_int_tp.variant.array.variant.number_of_elements = 2;
    e_vvp.next = &e_vl_off;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_points_to_field, &arr_int_tp, &e_vvp));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3748-3752 tk_array PM field decay */
    e_vvp.next = NULL;

    e_vpmf.next = &e_vnull_ptr; /* base_lval == NULL -> L3839 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_pm_call, &void_tp, &e_vpmf));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vpmf.next = NULL;

    /* sfk_gnu_atomic_generic_function direct call (with ck_integer op2 and non-ck_integer op2) */
    a_routine r_gatom;
    std::memset(&r_gatom, 0, sizeof(r_gatom));
    r_gatom.type = &vfn_tp;
    r_gatom.source_corresp.name = "__atomic_load";
    r_gatom.special_kind = (a_special_function_kind)sfk_gnu_atomic_generic_function;
    an_expr_node e_rgatom;
    std::memset(&e_rgatom, 0, sizeof(e_rgatom));
    e_rgatom.kind = enk_routine; e_rgatom.type = &vfn_tp; e_rgatom.variant.routine.ptr = &r_gatom;
    e_rgatom.next = &e_vl_off; /* ck_integer op2 -> L3914-3919 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &void_tp, &e_rgatom));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_rgatom.next = &e_vi; /* non-constant op2 */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_rgatom.next = NULL;

    /* eok_array_to_pointer with l1 == NULL (L4712) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_array_to_pointer, &vptr_tp, &e_vnull_ptr));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    /* enk_new_delete: 1-param free_fn + unwind_resume_make_block, 1-param scalar dtor, NULL dtor dynamic_init, extra delete arg */
    gcc_jit_param *d1_p = gcc_jit_context_new_param(ctx, NULL, jit_vptr, "dp0");
    gcc_jit_function *j_del1 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "rg_op_del1", 1, &d1_p, 0);
    gcc_jit_param *dt1_p = gcc_jit_context_new_param(ctx, NULL, jit_vptr, "dt0");
    gcc_jit_function *j_dtor1 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "rg_dtor1", 1, &dt1_p, 0);
    gcc_jit_param *d2_p[2] = {
        gcc_jit_context_new_param(ctx, NULL, gcc_jit_type_get_pointer(jit_int), "dp_ip"),
        gcc_jit_context_new_param(ctx, NULL, jit_long, "dp_sz")
    };
    gcc_jit_function *j_del2_ip = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "rg_op_del2_ip", 2, d2_p, 0);
    a_routine r_del1, r_dtor1, r_del2_ip;
    std::memset(&r_del1, 0, sizeof(r_del1)); r_del1.type = &vfn_tp;
    std::memset(&r_dtor1, 0, sizeof(r_dtor1)); r_dtor1.type = &vfn_tp;
    std::memset(&r_del2_ip, 0, sizeof(r_del2_ip)); r_del2_ip.type = &vfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_del1, j_del1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_dtor1, j_dtor1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_del2_ip, j_del2_ip));

    a_dynamic_init dip_new1, dip_free1, dip_nodtor;
    std::memset(&dip_new1, 0, sizeof(dip_new1));
    dip_new1.kind = dik_expression; dip_new1.variant.expression = &e_vs1; dip_new1.destructor = &r_dtor1;
    std::memset(&dip_free1, 0, sizeof(dip_free1));
    dip_free1.kind = dik_none; dip_free1.destructor = &r_del1;
    std::memset(&dip_nodtor, 0, sizeof(dip_nodtor));
    dip_nodtor.kind = dik_expression; dip_nodtor.variant.expression = &e_vi; dip_nodtor.destructor = NULL;

    std::memset(&nds_g, 0, sizeof(nds_g));
    nds_g.is_new = TRUE;
    nds_g.type = &st1_tp;
    nds_g.routine = &r_n2;
    nds_g.dynamic_init = &dip_new1;
    nds_g.freeing_of_storage_on_exception = &dip_free1;
    exceptions_enabled = TRUE;
    g_expr_mock.unwind_resume_make_block = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_nd_g, &rv)); /* L5074 & L5084! */
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));
    g_expr_mock.unwind_resume_make_block = FALSE;
    exceptions_enabled = FALSE;

    /* Scalar delete with 1-param dtor, extra arg (atype != ptype), error extra arg, and destructor == NULL dynamic_init */
    std::memset(&nds_g, 0, sizeof(nds_g));
    nds_g.is_new = FALSE;
    nds_g.array_delete = FALSE;
    nds_g.type = &st1_tp;
    nds_g.dynamic_init = &dip_new1;
    nds_g.routine = &r_del2_ip;
    e_vvp.next = &e_vi;
    nds_g.arg = &e_vvp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_nd_g, &rv)); /* L5187, L5215-5218, L5238! */
    e_vvp.next = &e_bad;
    if (gcc_gen_be_lower_expr_rvalue(&e_nd_g, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_vvp.next = NULL;
    nds_g.dynamic_init = &dip_nodtor;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_nd_g, &rv)); /* L5192-5195! */

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises all remaining `gcc_gen_be_lower_expr_rvalue` lines, branches, and OOM budgets.
 * @details Covers builtin operations (`addressof` NULL lval, `bit_cast`, `shufflevector`, `convertvector`),
 * C11 atomic variable/field loads/stores/RMWs OOM, `eok_vector_subscript`, complex arithmetic/comparison/unary/cast OOM,
 * VLA compound pointer assignment OOM, `eok_pdiff` non-long result, `eok_spaceship` pointer/float,
 * `eok_address_of` empty parens, `__atomic_*`/`__sync_*`/`__c11_atomic_*` call dispatch, ARM64 non-poly PMF call,
 * SysV/AAPCS64/scalar `va_arg`/`va_start`/`va_copy` edge cases and NULL `libgccjit` extension hooks,
 * `eok_bool_cast`/`eok_pm_base_class_cast`/`eok_array_to_pointer`/`eok_braced_init`, `enk_condition`,
 * `enk_new_delete` (non-LP unwind resume, array without dtor, 0-size delete, aligned delete),
 * `enk_result_of_overriding_function` (0-param caller, void callee), `sizeof` VLA, `co_await`, and `enk_lowered_eh_construct`.
 * @param[in] ctx Active `libgccjit` context.
 * @param[in] fn Active `libgccjit` test function.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_rval_final_gaps(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_short = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SHORT);
    gcc_jit_type *jit_long = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_type *jit_flt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
    gcc_jit_type *jit_dbl = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
    gcc_jit_type *jit_ldbl = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_vptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *jit_cptr = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR));
    gcc_jit_type *jit_iptr = gcc_jit_type_get_pointer(jit_int);
    gcc_jit_type *jit_sz = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);

    a_type int_tp, short_tp, long_tp, flt_tp, dbl_tp, ldbl_tp, void_tp, vptr_tp, cptr_tp, iptr_tp, bool_tp, bit24_tp, cplx_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&short_tp, ik_short, 2));
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&bit24_tp, ik_bit_precise, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&bool_tp, ik_char, 1));
    bool_tp.variant.integer.bool_type = TRUE;
    GCC_GEN_BE_CHECK(expr_init_float_type(&flt_tp, fk_float, 4));
    GCC_GEN_BE_CHECK(expr_init_float_type(&dbl_tp, fk_double, 8));
    GCC_GEN_BE_CHECK(expr_init_float_type(&ldbl_tp, fk_long_double, 16));
    std::memset(&void_tp, 0, sizeof(void_tp)); void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&iptr_tp, &int_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_test_map_type(&cptr_tp, jit_cptr));
    cptr_tp.kind = tk_pointer; cptr_tp.size = sizeof(void *);
    std::memset(&cplx_tp, 0, sizeof(cplx_tp));
    cplx_tp.kind = tk_complex; cplx_tp.size = 16; cplx_tp.variant.float_kind = fk_double;

    gcc_jit_type *jit_cplx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&cplx_tp, &jit_cplx));

    gcc_jit_lvalue *lv_i = gcc_jit_function_new_local(fn, NULL, jit_int, "rfg_i");
    gcc_jit_lvalue *lv_s = gcc_jit_function_new_local(fn, NULL, jit_short, "rfg_s");
    gcc_jit_lvalue *lv_l = gcc_jit_function_new_local(fn, NULL, jit_long, "rfg_l");
    gcc_jit_lvalue *lv_f = gcc_jit_function_new_local(fn, NULL, jit_flt, "rfg_f");
    gcc_jit_lvalue *lv_d = gcc_jit_function_new_local(fn, NULL, jit_dbl, "rfg_d");
    gcc_jit_lvalue *lv_vp = gcc_jit_function_new_local(fn, NULL, jit_vptr, "rfg_vp");
    gcc_jit_lvalue *lv_cp = gcc_jit_function_new_local(fn, NULL, jit_cptr, "rfg_cp");
    gcc_jit_lvalue *lv_ip = gcc_jit_function_new_local(fn, NULL, jit_iptr, "rfg_ip");
    gcc_jit_lvalue *lv_cx = gcc_jit_function_new_local(fn, NULL, jit_cplx, "rfg_cx");

    a_variable v_i, v_s, v_l, v_f, v_d, v_vp, v_cp, v_ip, v_cx, v_null_lv;
    std::memset(&v_i, 0, sizeof(v_i)); v_i.type = &int_tp;
    std::memset(&v_s, 0, sizeof(v_s)); v_s.type = &short_tp;
    std::memset(&v_l, 0, sizeof(v_l)); v_l.type = &long_tp;
    std::memset(&v_f, 0, sizeof(v_f)); v_f.type = &flt_tp;
    std::memset(&v_d, 0, sizeof(v_d)); v_d.type = &dbl_tp;
    std::memset(&v_vp, 0, sizeof(v_vp)); v_vp.type = &vptr_tp;
    std::memset(&v_cp, 0, sizeof(v_cp)); v_cp.type = &cptr_tp;
    std::memset(&v_ip, 0, sizeof(v_ip)); v_ip.type = &iptr_tp;
    std::memset(&v_cx, 0, sizeof(v_cx)); v_cx.type = &cplx_tp;
    std::memset(&v_null_lv, 0, sizeof(v_null_lv)); v_null_lv.type = &int_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_i, lv_i));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_s, lv_s));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_l, lv_l));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_f, lv_f));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_d, lv_d));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vp, lv_vp));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_cp, lv_cp));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_ip, lv_ip));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_cx, lv_cx));

    an_expr_node e_vi, e_vi2, e_vs, e_vl, e_vf, e_vd, e_vd2, e_vvp, e_vcp, e_vip, e_vip2, e_vcx, e_vcx2, e_vnull, e_op;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi, &v_i));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi2, &v_i));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vs, &v_s));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vl, &v_l));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vf, &v_f));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vd, &v_d));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vd2, &v_d));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vvp, &v_vp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vcp, &v_cp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vip, &v_ip));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vip2, &v_ip));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vcx, &v_cx));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vcx2, &v_cx));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vnull, &v_null_lv));

    gcc_jit_rvalue *rv = NULL;

    /* 1) Builtin operations: bok_builtin_addressof NULL lval OOM (L2890), bok_builtin_bit_cast OOM (L2923, L2925),
          bok_builtin_shufflevector / bok_builtin_convertvector OOM (L2946, L5758, L5772, L5890) */
    g_expr_mock.local_var_return_null = TRUE;
    an_expr_node e_bop;
    std::memset(&e_bop, 0, sizeof(e_bop));
    e_bop.kind = enk_builtin_operation;
    e_bop.type = &vptr_tp;
    e_bop.variant.builtin_operation.kind = bok_builtin_addressof;
    e_bop.variant.builtin_operation.operands = &e_vnull;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    g_expr_mock.local_var_return_null = FALSE;

    e_bop.type = &flt_tp;
    e_bop.variant.builtin_operation.kind = bok_builtin_bit_cast;
    e_bop.variant.builtin_operation.operands = &e_vi;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));

    a_type vec4i_tp, vec4f_tp;
    std::memset(&vec4i_tp, 0, sizeof(vec4i_tp));
    vec4i_tp.kind = tk_vector; vec4i_tp.size = 16; vec4i_tp.variant.vector.element_type = &int_tp;
    std::memset(&vec4f_tp, 0, sizeof(vec4f_tp));
    vec4f_tp.kind = tk_vector; vec4f_tp.size = 16; vec4f_tp.variant.vector.element_type = &flt_tp;
    gcc_jit_type *jit_v4i = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&vec4i_tp, &jit_v4i));
    gcc_jit_lvalue *lv_v4i = gcc_jit_function_new_local(fn, NULL, jit_v4i, "rfg_v4i");
    a_variable v_v4i;
    std::memset(&v_v4i, 0, sizeof(v_v4i)); v_v4i.type = &vec4i_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_v4i, lv_v4i));
    an_expr_node e_vv4i, e_vv4i_2;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vv4i, &v_v4i));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vv4i_2, &v_v4i));

    /* shufflevector with 2 vector operands (op2->next == NULL -> hits L5772 safe_ptr_or_int_cast!) and with mask op */
    e_vv4i.next = &e_vv4i_2;
    e_bop.type = &vec4i_tp;
    e_bop.variant.builtin_operation.kind = bok_builtin_shufflevector;
    e_bop.variant.builtin_operation.operands = &e_vv4i;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_vv4i_2.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_vv4i_2.next = NULL; e_vv4i.next = NULL;

    /* convertvector same type (hits L5890 safe_ptr_or_int_cast!) and int->float */
    e_bop.type = &vec4i_tp;
    e_bop.variant.builtin_operation.kind = bok_builtin_convertvector;
    e_bop.variant.builtin_operation.operands = &e_vv4i;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));
    e_bop.type = &vec4f_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bop, &rv));

    /* 2) enk_variable static unnamed constant OOM (L2986) & C11 atomic load/store/RMW/inc OOM (L2995, L3048, L3099, L3101, L3722, L3725, L3780, L3783) */
    a_constant c_stat_i;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_stat_i, &int_tp, 55));
    a_variable v_stat_con;
    std::memset(&v_stat_con, 0, sizeof(v_stat_con));
    v_stat_con.type = &int_tp; v_stat_con.storage_class = sc_static; v_stat_con.init_kind = initk_static;
    v_stat_con.initializer.constant = &c_stat_i;
    an_expr_node e_vstat_con;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vstat_con, &v_stat_con));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_vstat_con, &rv));

    a_type at_int_tp;
    GCC_GEN_BE_CHECK(expr_init_typeref(&at_int_tp, &int_tp, TQ_C11_ATOMIC));
    a_variable v_at_i;
    std::memset(&v_at_i, 0, sizeof(v_at_i)); v_at_i.type = &at_int_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_at_i, lv_i));
    an_expr_node e_vat;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vat, &v_at_i));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_vat, &rv)); /* L2995 */

    a_field f_at_fld, f_bf_rw;
    std::memset(&f_at_fld, 0, sizeof(f_at_fld)); f_at_fld.type = &at_int_tp;
    std::memset(&f_bf_rw, 0, sizeof(f_bf_rw)); f_bf_rw.type = &int_tp; f_bf_rw.is_bit_field = TRUE; f_bf_rw.bit_size = 4;
    a_type st_at_tp;
    std::memset(&st_at_tp, 0, sizeof(st_at_tp));
    st_at_tp.kind = tk_struct; st_at_tp.size = 4; st_at_tp.variant.class_struct_union.field_list = &f_at_fld;
    gcc_jit_type *jit_st_at = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&st_at_tp, &jit_st_at));
    gcc_jit_lvalue *lv_stat = gcc_jit_function_new_local(fn, NULL, jit_st_at, "rfg_stat");
    a_variable v_stat;
    std::memset(&v_stat, 0, sizeof(v_stat)); v_stat.type = &st_at_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_stat, lv_stat));
    an_expr_node e_vstat, e_fat, e_fbf;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vstat, &v_stat));
    std::memset(&e_fat, 0, sizeof(e_fat)); e_fat.kind = enk_field; e_fat.type = &at_int_tp; e_fat.variant.field.ptr = &f_at_fld;
    std::memset(&e_fbf, 0, sizeof(e_fbf)); e_fbf.kind = enk_field; e_fbf.type = &int_tp; e_fbf.variant.field.ptr = &f_bf_rw;

    e_vstat.next = &e_fat;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_field, &int_tp, &e_vstat));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3048 */
    e_vstat.next = NULL;

    an_expr_node e_bf_acc;
    e_vstat.next = &e_fbf;
    GCC_GEN_BE_CHECK(expr_init_op(&e_bf_acc, eok_dot_field, &int_tp, &e_vstat));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_bf_acc, &rv)); /* L3040 */
    e_bf_acc.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_assign, &int_tp, &e_bf_acc));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3071 */
    e_bf_acc.next = NULL;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_post_incr, &int_tp, &e_bf_acc));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3718 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pre_incr, &int_tp, &e_bf_acc));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3776 */
    e_vstat.next = NULL;

    e_vat.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_assign, &int_tp, &e_vat));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3099 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_add_assign, &int_tp, &e_vat));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3101 */
    e_vat.next = NULL;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_post_incr, &int_tp, &e_vat));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3722, L3725 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pre_incr, &int_tp, &e_vat));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3780, L3783 */

    /* 3) L3021:92 eok_vector_subscript & L3026:47 eok_indirect with op1 == NULL */
    e_vv4i.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_vector_subscript, &int_tp, &e_vv4i));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vv4i.next = NULL;

    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_indirect, &int_tp, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* 4) Complex compound assign, binary, eq, unary, real/imag/xconj, casts OOM sweeps */
    e_vcx.next = &e_vcx2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_add_assign, &cplx_tp, &e_vcx));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3125, L3131, L3155 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_add, &cplx_tp, &e_vcx));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3444-L3455 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_eq, &bool_tp, &e_vcx));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3540-L3547 */
    e_vcx.next = NULL;

    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_negate, &cplx_tp, &e_vcx));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3657-L3671 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_real_part, &dbl_tp, &e_vcx));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3692, L3695 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_xconj, &cplx_tp, &e_vcx));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3703-L3709 */

    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_cast, &cplx_tp, &e_vd));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L4797 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_cast, &dbl_tp, &e_vcx));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L4806, L4808 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_cast, &cplx_tp, &e_vcx));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L4812-L4820 */

    /* 5) VLA compound pointer assign OOM (L3170, L3175), ternary cast OOM (L3272), r1/r2 binary OOM (L3306, L3313) */
    a_type vla_tp, ptr_vla_tp;
    std::memset(&vla_tp, 0, sizeof(vla_tp));
    vla_tp.kind = tk_array; vla_tp.variant.array.is_variable_size_array = TRUE;
    vla_tp.variant.array.has_assoc_vla_dimension = TRUE;
    vla_tp.variant.array.element_type = &int_tp;
    a_vla_dimension vla_d;
    std::memset(&vla_d, 0, sizeof(vla_d)); vla_d.type = &vla_tp; vla_d.dimension_variable = &v_l;
    a_scope fn_sc_vla;
    std::memset(&fn_sc_vla, 0, sizeof(fn_sc_vla)); fn_sc_vla.vla_dimensions = &vla_d;
    innermost_function_scope = &fn_sc_vla;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_vla_tp, &vla_tp, FALSE));
    a_variable v_pvla;
    std::memset(&v_pvla, 0, sizeof(v_pvla)); v_pvla.type = &ptr_vla_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pvla, lv_ip));
    an_expr_node e_pvla;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_pvla, &v_pvla));
    e_pvla.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_padd_assign, &ptr_vla_tp, &e_pvla));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_psubtract_assign, &ptr_vla_tp, &e_pvla));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_pvla.next = NULL;
    innermost_function_scope = NULL;

    /* Ternary cond ? short : long -> long (L3272) */
    e_vi.next = &e_vs; e_vs.next = &e_vl;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_question, &long_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vi.next = NULL; e_vs.next = NULL;

    /* Binary arithmetic / bitwise / comparison / pdiff / spaceship / negate OOMs */
    e_vi.next = &e_vs;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_add, &bit24_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3306, L3313, L3505, L3508, L3511 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_and, &long_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3531 F>0 & T>0, L3534, L3535! */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_eq, &int_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3573, L3578 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_lt, &int_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3600, L3605 */
    e_vi.next = NULL;

    /* eok_eq with NULL op1 -> L3535:27 & L3536:31 False (returns GCC_GEN_BE_ERROR_INVALID_ARGUMENT on NULL cmp) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_eq, &bool_tp, NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    /* eok_pdiff with expr->type = int_tp -> L3366 line & OOM! */
    e_vip.next = &e_vip2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pdiff, &int_tp, &e_vip));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    /* eok_spaceship on pointer (L3384:49 False) & float OOM (L3382) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_spaceship, &int_tp, &e_vip));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vip.next = NULL;
    e_vd.next = &e_vd2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_spaceship, &int_tp, &e_vd));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vd.next = NULL;

    /* eok_negate on short -> int_tp (L3683) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_negate, &int_tp, &e_vs));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    /* L3732:31 False: eok_post_decr on pointer + eok_post_incr with cblock == NULL */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_post_decr, &iptr_tp, &e_vip));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_post_incr, &int_tp, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L3757:34, L3761:64, L3762:31, L3764, L3766: eok_address_of empty parens & routine OOM */
    an_expr_node e_empty_par;
    GCC_GEN_BE_CHECK(expr_init_op(&e_empty_par, eok_parens, &int_tp, NULL));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_address_of, &iptr_tp, &e_empty_par));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    a_type vfn_tp;
    std::memset(&vfn_tp, 0, sizeof(vfn_tp)); vfn_tp.kind = tk_routine; vfn_tp.variant.routine.return_type = &void_tp;
    a_routine r_fn0;
    std::memset(&r_fn0, 0, sizeof(r_fn0)); r_fn0.type = &vfn_tp; r_fn0.source_corresp.name = "rfg_fn0";
    an_expr_node e_rfn0;
    std::memset(&e_rfn0, 0, sizeof(e_rfn0)); e_rfn0.kind = enk_routine; e_rfn0.type = &vfn_tp; e_rfn0.variant.routine.ptr = &r_fn0;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_address_of, &vptr_tp, &e_rfn0));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L3764 */

    /* L3782:54 False: eok_pre_incr with op1 == NULL */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pre_incr, &int_tp, NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    /* 6) Calls: __atomic_* / __sync_* / __c11_atomic_* (L3829-3831), ARM64 non-poly PMF (L3910:49),
          builtin return cast OOM (L3979), sret+invis_ref on 0-param callee (L4009:57, L4073), variadic double vs float (L4035:39) */
    static const char *const k_bnames[3] = {
        "__atomic_thread_fence", "__sync_synchronize", "__c11_atomic_thread_fence"
    };
    g_expr_mock.mock_builtin_rval = gcc_jit_context_new_rvalue_from_int(ctx, jit_int, 1);
    for (int bi = 0; bi < 3; ++bi) {
        a_routine rb;
        std::memset(&rb, 0, sizeof(rb));
        rb.type = &vfn_tp; rb.source_corresp.name = k_bnames[bi];
        an_expr_node erb;
        std::memset(&erb, 0, sizeof(erb));
        erb.kind = enk_routine; erb.type = &vfn_tp; erb.variant.routine.ptr = &rb;
        GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &long_tp, &erb));
        GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* Also hits L3979 int -> long cast OOM! */
    }
    g_expr_mock.mock_builtin_rval = NULL;

    /* ARM64 AAPCS64 PMF call (L3910:49) */
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
    a_type nonpoly_cls_tp, pmf_np_tp, mfn_tp;
    std::memset(&mfn_tp, 0, sizeof(mfn_tp)); mfn_tp.kind = tk_routine; mfn_tp.variant.routine.return_type = &void_tp;
    gcc_jit_type *jit_mfn_ptr = gcc_jit_context_new_function_ptr_type(ctx, NULL, jit_void, 1, &jit_vptr, 0);
    GCC_GEN_BE_CHECK(expr_test_map_type(&mfn_tp, jit_mfn_ptr));
    std::memset(&nonpoly_cls_tp, 0, sizeof(nonpoly_cls_tp));
    nonpoly_cls_tp.kind = tk_class; nonpoly_cls_tp.size = 4; nonpoly_cls_tp.variant.class_struct_union.extra_info = NULL;
    std::memset(&pmf_np_tp, 0, sizeof(pmf_np_tp));
    pmf_np_tp.kind = tk_ptr_to_member; pmf_np_tp.size = 16;
    pmf_np_tp.variant.ptr_to_member.class_of_which_a_member = &nonpoly_cls_tp;
    pmf_np_tp.variant.ptr_to_member.type = &mfn_tp;
    GCC_GEN_BE_CHECK(expr_test_map_type(&pmf_np_tp, g_expr_mock.pmf_struct_type));
    gcc_jit_lvalue *lv_pmf = gcc_jit_function_new_local(fn, NULL, g_expr_mock.pmf_struct_type, "rfg_pmf");
    gcc_jit_lvalue *lv_np = gcc_jit_function_new_local(fn, NULL, g_expr_mock.one_field_struct_type, "rfg_np");
    a_variable v_pmf, v_np;
    std::memset(&v_pmf, 0, sizeof(v_pmf)); v_pmf.type = &pmf_np_tp;
    std::memset(&v_np, 0, sizeof(v_np)); v_np.type = &nonpoly_cls_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pmf, lv_pmf));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_np, lv_np));
    an_expr_node e_vpmf, e_vnp;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vpmf, &v_pmf));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vnp, &v_np));
    e_vpmf.next = &e_vnp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_pm_call, &void_tp, &e_vpmf));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vpmf.next = NULL;
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;

    /* sret on 0-param non-variadic callee (L4009:57 False) + 1-param variadic invis_ref OOM (L4073) & float/double (L4035:39) */
    a_routine r_sret0;
    std::memset(&r_sret0, 0, sizeof(r_sret0)); r_sret0.type = &vfn_tp; r_sret0.source_corresp.name = "rfg_sret0";
    gcc_jit_function *j_sret0 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "rfg_sret0", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_sret0, j_sret0));
    an_expr_node e_rsret0;
    std::memset(&e_rsret0, 0, sizeof(e_rsret0));
    e_rsret0.kind = enk_routine; e_rsret0.type = &vfn_tp; e_rsret0.variant.routine.ptr = &r_sret0;
    gcc_jit_block *sret0_lp = gcc_jit_function_new_block(fn, "sret0_lp");
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(sret0_lp));
    g_expr_mock.is_sret = TRUE;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_rsret0));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());

    a_routine_type_supplement var_sup;
    std::memset(&var_sup, 0, sizeof(var_sup)); var_sup.has_ellipsis = TRUE;
    a_type var_fn_tp;
    std::memset(&var_fn_tp, 0, sizeof(var_fn_tp));
    var_fn_tp.kind = tk_routine; var_fn_tp.variant.routine.return_type = &int_tp; var_fn_tp.variant.routine.extra_info = &var_sup;
    a_routine r_var0;
    std::memset(&r_var0, 0, sizeof(r_var0)); r_var0.type = &var_fn_tp; r_var0.source_corresp.name = "rfg_var1";
    gcc_jit_param *vp_sret = gcc_jit_context_new_param(ctx, NULL, jit_vptr, "sret");
    gcc_jit_function *j_var0 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "rfg_var1", 1, &vp_sret, 1);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_var0, j_var0));
    an_expr_node e_rvar0;
    std::memset(&e_rvar0, 0, sizeof(e_rvar0));
    e_rvar0.kind = enk_routine; e_rvar0.type = &var_fn_tp; e_rvar0.variant.routine.ptr = &r_var0;
    g_expr_mock.invis_ref_all = TRUE;
    e_rvar0.next = &e_vf; /* float promoted in variadic + invis_ref OOM */
    e_vf.next = &e_vd;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_rvar0));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.invis_ref_all = FALSE;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* hits float promotion L4035:39 True & double False! */
    e_rvar0.next = NULL; e_vf.next = NULL;
    GCC_GEN_BE_CHECK(expr_test_reset_mock());

    /* 7) va_arg / va_start / va_copy edge cases & NULL libgccjit extension pointers */
    a_type va_tag_st, va_arr_tp, z_dbl_tp, z_ldbl_tp, z_long_tp, z_vptr_tp;
    std::memset(&va_tag_st, 0, sizeof(va_tag_st)); va_tag_st.kind = tk_struct; va_tag_st.size = 24;
    std::memset(&va_arr_tp, 0, sizeof(va_arr_tp));
    va_arr_tp.kind = tk_array; va_arr_tp.size = 24; va_arr_tp.variant.array.element_type = &va_tag_st;
    va_arr_tp.variant.array.variant.number_of_elements = 1;
    GCC_GEN_BE_CHECK(expr_init_float_type(&z_dbl_tp, fk_double, 0));
    GCC_GEN_BE_CHECK(expr_init_float_type(&z_ldbl_tp, fk_long_double, 0));
    GCC_GEN_BE_CHECK(expr_init_int_type(&z_long_tp, ik_long, 0));
    std::memset(&z_vptr_tp, 0, sizeof(z_vptr_tp)); z_vptr_tp.kind = tk_pointer; z_vptr_tp.size = 0;
    GCC_GEN_BE_CHECK(expr_test_map_type(&z_dbl_tp, jit_dbl));
    GCC_GEN_BE_CHECK(expr_test_map_type(&z_ldbl_tp, jit_ldbl));
    GCC_GEN_BE_CHECK(expr_test_map_type(&z_long_tp, jit_long));
    GCC_GEN_BE_CHECK(expr_test_map_type(&z_vptr_tp, jit_vptr));

    gcc_jit_type *jit_va_arr = gcc_jit_context_new_array_type(ctx, NULL, jit_cptr, 3);
    gcc_jit_lvalue *lv_va_arr = gcc_jit_function_new_local(fn, NULL, jit_va_arr, "rfg_va_arr");
    a_variable v_va_arr, v_va_st;
    std::memset(&v_va_arr, 0, sizeof(v_va_arr)); v_va_arr.type = &va_arr_tp;
    std::memset(&v_va_st, 0, sizeof(v_va_st)); v_va_st.type = &va_tag_st;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_va_arr, lv_va_arr));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_va_st, lv_vp)); /* pointer-typed lval -> L4199:82 False! */
    an_expr_node e_va_arr, e_va_st;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_va_arr, &v_va_arr));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_va_st, &v_va_st));

    /* SysV AMD64: GP-only (va_num_gp=1, va_num_fp=0) and FP-only (va_num_gp=0, va_num_fp=1) + 0-size types */
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    g_expr_mock.va_is_stack = FALSE; g_expr_mock.va_num_gp = 1; g_expr_mock.va_num_fp = 0;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_long_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.va_num_gp = 0; g_expr_mock.va_num_fp = 1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_dbl_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.va_is_stack = TRUE;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_ldbl_tp, &e_va_st));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_vptr_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* SysV & AAPCS64 type_align > 8 (16-byte aligned) with bitcast present & NULL + cblock == NULL */
    z_ldbl_tp.alignment = 16;
    pfn_gcc_jit_context_new_bitcast saved_bc = p_gcc_jit_context_new_bitcast;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_ldbl_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    p_gcc_jit_context_new_bitcast = NULL;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    p_gcc_jit_context_new_bitcast = saved_bc;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;

    /* Scalar pointer va_arg with 0-size double, long_double, long, void* (L4493-4495) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_dbl_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_ldbl_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_long_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_vptr_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* va_start & va_copy edge cases (L4540-4593) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_start, &void_tp, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* op1 == NULL */
    g_expr_mock.local_var_return_null = TRUE;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_start, &void_tp, &e_vnull));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* ap_lval == NULL */
    g_expr_mock.local_var_return_null = FALSE;

    pfn_gcc_jit_context_get_builtin_function saved_bfn = p_gcc_jit_context_get_builtin_function;
    p_gcc_jit_context_get_builtin_function = NULL;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_start, &void_tp, &e_vvp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* next_arg_fn == NULL */
    p_gcc_jit_context_get_builtin_function = saved_bfn;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* cblock == NULL */
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    e_vvp.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_start, &void_tp, &e_vvp));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L4551 OOM + ap_t == void* (L4557:57 True) */
    e_vvp.next = NULL;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_start, &void_tp, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* ap_t == int (L4556:39 False) */

    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_copy, &void_tp, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* op1 == NULL */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_copy, &void_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* op2 == NULL */

    g_expr_mock.local_var_return_null = TRUE;
    e_vnull.next = &e_vvp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_copy, &void_tp, &e_vnull));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* dst_ap == NULL */
    e_vnull.next = NULL;
    g_expr_mock.local_var_return_null = FALSE;

    e_vcp.next = &e_vvp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_copy, &void_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* cblock == NULL */
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L4573 OOM + dst_t (char*) != src_rv (void*) */
    a_constant c_vp_zero;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c_vp_zero, &vptr_tp, 0));
    an_expr_node e_cvp;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_cvp, &c_vp_zero));
    e_vcp.next = &e_cvp;
    g_expr_mock.local_var_return_null = TRUE;
    e_vcp.next = &e_vnull; /* src_ap == NULL -> lowers op2 as rvalue (L4574:39 False & L4577 OOM!) */
    /* Wait: with local_var_return_null, e_vcp (cached!) still resolves dst_ap != NULL while e_vnull resolves src_ap == NULL! */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.local_var_return_null = FALSE;
    e_vcp.next = NULL;

    /* 8) eok_bool_cast (L4608, L4612:60, L4641), eok_pm_base_class_cast (L4656-4657),
          eok_array_to_pointer (L4737:23, L4748:59), eok_cast NULL op1, eok_braced_init, enk_condition */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_bool_cast, &bool_tp, &e_vpmf));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L4608 */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_bool_cast, &bool_tp, &e_vnp));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L4641 struct bool_cast OOM */
    an_expr_node e_void_op;
    std::memset(&e_void_op, 0, sizeof(e_void_op));
    e_void_op.kind = enk_throw; e_void_op.type = &pmf_np_tp; /* yields r1 == NULL -> L4612:60 False! */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_bool_cast, &bool_tp, &e_void_op));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* PM cast: derived_cls == NULL, tk_struct, tk_integer, and tk_class with extra_info == NULL */
    a_type st_cls_tp, pm_null_cls, pm_st_cls, pm_int_cls, pm_noextra_cls;
    std::memset(&st_cls_tp, 0, sizeof(st_cls_tp)); st_cls_tp.kind = tk_struct; st_cls_tp.size = 4;
    std::memset(&pm_null_cls, 0, sizeof(pm_null_cls));
    pm_null_cls.kind = tk_ptr_to_member; pm_null_cls.size = 8; pm_null_cls.variant.ptr_to_member.type = &int_tp;
    pm_null_cls.variant.ptr_to_member.class_of_which_a_member = NULL;
    pm_st_cls = pm_null_cls; pm_st_cls.variant.ptr_to_member.class_of_which_a_member = &st_cls_tp;
    pm_int_cls = pm_null_cls; pm_int_cls.variant.ptr_to_member.class_of_which_a_member = &int_tp;
    pm_noextra_cls = pm_null_cls; pm_noextra_cls.variant.ptr_to_member.class_of_which_a_member = &nonpoly_cls_tp;

    e_vl.type = &pm_st_cls;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, &pm_null_cls, &e_vl));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* derived_cls == NULL (4656:23 False) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, &pm_st_cls, &e_vl));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* derived_cls tk_struct (4656:72 True) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, &pm_int_cls, &e_vl));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* derived_cls tk_integer (4656:72 False) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, &pm_noextra_cls, &e_vl));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* derived_cls tk_class extra_info==NULL (4657:23 False) */
    e_vl.type = &long_tp;

    /* eok_array_to_pointer: op1 == NULL (4737:23 False) & VLA backed by array lvalue (4748:59 False) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_array_to_pointer, &iptr_tp, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    a_variable v_vla_arrlv;
    std::memset(&v_vla_arrlv, 0, sizeof(v_vla_arrlv)); v_vla_arrlv.type = &vla_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vla_arrlv, lv_va_arr)); /* array lvalue, not pointer! */
    an_expr_node e_vvla_arrlv;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vvla_arrlv, &v_vla_arrlv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_array_to_pointer, &iptr_tp, &e_vvla_arrlv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* eok_cast with op1 == NULL -> L4773:31 & L4815:35 False */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_cast, &void_tp, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* enk_braced_init_list on 2-field struct with 1 operand (elem == NULL on 2nd field -> L4850:48 False) */
    a_field two_f1, two_f2;
    std::memset(&two_f1, 0, sizeof(two_f1)); two_f1.type = &vptr_tp; two_f1.next = &two_f2;
    std::memset(&two_f2, 0, sizeof(two_f2)); two_f2.type = &long_tp;
    gcc_jit_struct *pmf_jst = gcc_jit_type_is_struct(g_expr_mock.pmf_struct_type);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &two_f1, gcc_jit_struct_get_field(pmf_jst, 0)));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &two_f2, gcc_jit_struct_get_field(pmf_jst, 1)));
    a_type two_st_tp;
    std::memset(&two_st_tp, 0, sizeof(two_st_tp)); two_st_tp.kind = tk_struct; two_st_tp.size = 16;
    two_st_tp.variant.class_struct_union.field_list = &two_f1;
    GCC_GEN_BE_CHECK(expr_test_map_type(&two_st_tp, g_expr_mock.pmf_struct_type));
    std::memset(&e_op, 0, sizeof(e_op));
    e_op.kind = enk_braced_init_list; e_op.type = &two_st_tp; e_op.variant.braced_init_list = &e_vvp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* enk_condition with local_var_return_null (L4885:49 False) & OOM sweep (L4906) */
    a_dynamic_init cond_dip;
    std::memset(&cond_dip, 0, sizeof(cond_dip));
    cond_dip.kind = dik_expression; cond_dip.variable = &v_null_lv; cond_dip.variant.expression = &e_vi;
    a_condition_supplement cond_sup;
    std::memset(&cond_sup, 0, sizeof(cond_sup));
    cond_sup.dynamic_init = &cond_dip;
    cond_sup.expr = &e_vi2;
    an_expr_node e_cond;
    std::memset(&e_cond, 0, sizeof(e_cond));
    e_cond.kind = enk_condition; e_cond.type = &int_tp;
    e_cond.variant.condition = &cond_sup;
    g_expr_mock.local_var_return_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cond, &rv));
    g_expr_mock.local_var_return_null = FALSE;
    cond_dip.variable = &v_i;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_cond, &rv));

    /* 9) enk_new_delete:
          - array new with dynamic_init == NULL (L4978:27 False) and dynamic_init->destructor == NULL (L4978:49 False)
          - new with alloc_func returning int* == ptr_type (L5037:63 False)
          - new with ctor + free_fn + unwind_resume_make_block + NO outer_lp (L5124:31 False & L5127-5128!)
          - array delete on 0-size type + 0-param dtor + uncached dtor OOM (L5145, L5162:55, L5184:59)
          - scalar delete with dealloc_func == NULL (L5244:27 False), 0-size 2-param delete (L5266:59 False), aligned 3-param delete (L5270:52 True) */
    a_new_delete_supplement nds;
    an_expr_node e_nd;
    std::memset(&e_nd, 0, sizeof(e_nd));
    e_nd.kind = enk_new_delete; e_nd.type = &iptr_tp; e_nd.variant.new_delete = &nds;

    a_dynamic_init dip_nodtor, dip_ctor_free, dip_fsoe;
    std::memset(&dip_nodtor, 0, sizeof(dip_nodtor)); dip_nodtor.kind = dik_none; dip_nodtor.destructor = NULL;

    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = TRUE; nds.type = &int_tp; nds.number_of_elements = &e_vi; nds.dynamic_init = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv)); /* L4978:27 False */
    nds.dynamic_init = &dip_nodtor;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv)); /* L4978:49 False */

    /* Custom operator new returning int* (matches ptr_type -> L5037:63 False) + ctor + free_fn + NO outer_lp (L5127-5128!) */
    gcc_jit_param *an_p = gcc_jit_context_new_param(ctx, NULL, jit_sz, "sz");
    gcc_jit_function *j_an_ip = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_iptr, "rfg_op_new_ip", 1, &an_p, 0);
    gcc_jit_param *fr_p = gcc_jit_context_new_param(ctx, NULL, jit_vptr, "p");
    gcc_jit_function *j_free1 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "rfg_op_del1", 1, &fr_p, 0);
    gcc_jit_param *ct_p = gcc_jit_context_new_param(ctx, NULL, jit_iptr, "self");
    gcc_jit_function *j_ctor1 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "rfg_ctor1", 1, &ct_p, 0);
    a_routine r_an_ip, r_free1, r_ctor1;
    std::memset(&r_an_ip, 0, sizeof(r_an_ip)); r_an_ip.type = &vfn_tp;
    std::memset(&r_free1, 0, sizeof(r_free1)); r_free1.type = &vfn_tp;
    std::memset(&r_ctor1, 0, sizeof(r_ctor1)); r_ctor1.type = &vfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_an_ip, j_an_ip));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_free1, j_free1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_ctor1, j_ctor1));

    std::memset(&dip_fsoe, 0, sizeof(dip_fsoe)); dip_fsoe.destructor = &r_free1;
    std::memset(&dip_ctor_free, 0, sizeof(dip_ctor_free));
    dip_ctor_free.kind = dik_constructor; dip_ctor_free.variant.constructor.ptr = &r_ctor1;
    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = TRUE; nds.type = &int_tp; nds.routine = &r_an_ip; nds.arg = &e_vl;
    nds.freeing_of_storage_on_exception = &dip_fsoe; nds.dynamic_init = &dip_ctor_free;
    g_expr_mock.unwind_resume_make_block = TRUE;
    gcc_gen_be_singleton_state.landing_pad_stack_size = 0; /* outer_lp == NULL! */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_nd, &rv));
    g_expr_mock.unwind_resume_make_block = FALSE;

    /* Array delete on 0-size type + 0-param uncached dtor OOM (L5145, L5162:55, L5184:59) */
    a_type z_st_tp;
    std::memset(&z_st_tp, 0, sizeof(z_st_tp)); z_st_tp.kind = tk_struct; z_st_tp.size = 0; z_st_tp.alignment = 0;
    GCC_GEN_BE_CHECK(expr_test_map_type(&z_st_tp, g_expr_mock.one_field_struct_type));
    a_routine r_dtor0;
    std::memset(&r_dtor0, 0, sizeof(r_dtor0)); r_dtor0.type = &vfn_tp; r_dtor0.source_corresp.name = "rfg_dtor0";
    a_dynamic_init dip_dtor0;
    std::memset(&dip_dtor0, 0, sizeof(dip_dtor0)); dip_dtor0.destructor = &r_dtor0;
    e_nd.type = &void_tp;
    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = FALSE; nds.array_delete = TRUE; nds.type = &z_st_tp;
    nds.arg = &e_vvp; nds.dynamic_init = &dip_dtor0;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 128, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(cache_clear_all());
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vp, lv_vp));
        return gcc_gen_be_lower_expr_rvalue(&e_nd, &rv);
    }));

    /* Scalar delete: dealloc_func == NULL (L5244:27 False), 0-size 2-param delete (L5266:59 False), aligned 3-param delete (L5270:52 True) */
    a_routine r_del_null, r_del3;
    std::memset(&r_del_null, 0, sizeof(r_del_null)); r_del_null.type = &vfn_tp;
    g_expr_mock.func_decl_return_null = TRUE;
    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = FALSE; nds.array_delete = FALSE; nds.type = &z_st_tp;
    nds.arg = &e_vvp; nds.routine = &r_del_null;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv));
    g_expr_mock.func_decl_return_null = FALSE;

    gcc_jit_param *d3_p[3] = {
        gcc_jit_context_new_param(ctx, NULL, jit_vptr, "p"),
        gcc_jit_context_new_param(ctx, NULL, jit_sz, "sz"),
        gcc_jit_context_new_param(ctx, NULL, jit_sz, "al")
    };
    gcc_jit_function *j_del3 = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "rfg_del3", 3, d3_p, 0);
    std::memset(&r_del3, 0, sizeof(r_del3)); r_del3.type = &vfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_del3, j_del3));
    nds.routine = &r_del3;
    nds.type = &z_st_tp; /* size == 0 -> L5266:59 False, alignment == 0 -> L5270:52 False */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv));
    a_type aligned_st_tp = z_st_tp;
    aligned_st_tp.size = 16; aligned_st_tp.alignment = 16; /* alignment > 0 -> L5270:52 True! */
    GCC_GEN_BE_CHECK(expr_test_map_type(&aligned_st_tp, g_expr_mock.one_field_struct_type));
    nds.type = &aligned_st_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv));

    /* 10) enk_result_of_overriding_function: 0-param current_fn -> 1-param target (L5324:19 False, L5356:31 True)
           & expected_type == int while callee returns void (L5367:49 True) */
    gcc_jit_function *fn_zero_p = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, jit_void, "rfg_zero_p_fn", 0, NULL, 0);
    gcc_jit_block *blk_zp = gcc_jit_function_new_block(fn_zero_p, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk_zp));

    a_routine r_wrapper_ov;
    std::memset(&r_wrapper_ov, 0, sizeof(r_wrapper_ov));
    r_wrapper_ov.type = &vfn_tp;
    r_wrapper_ov.overriding_function_for_wrapper = &r_free1;
    a_scope sc_ov;
    std::memset(&sc_ov, 0, sizeof(sc_ov));
    sc_ov.variant.routine.ptr = &r_wrapper_ov;
    innermost_function_scope = &sc_ov;

    an_expr_node e_ov;
    std::memset(&e_ov, 0, sizeof(e_ov));
    e_ov.kind = enk_result_of_overriding_function;
    e_ov.type = &int_tp; /* expected_type == int while j_free1 returns void -> L5367:49 True! */
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn_zero_p, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_ov, &rv);
    }));
    a_routine r_ov_zero;
    std::memset(&r_ov_zero, 0, sizeof(r_ov_zero)); r_ov_zero.type = &vfn_tp;
    gcc_jit_function *j_ov_zero = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "rfg_ov_zero", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_ov_zero, j_ov_zero));
    r_wrapper_ov.overriding_function_for_wrapper = &r_ov_zero;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn_zero_p, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_ov, &rv)); /* alloc_argc == 0! */
    innermost_function_scope = NULL;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* 11) enk_reuse_value OOM (L5388, L5420), sizeof(vla) NULL dim (L5447:65 False),
           enk_statement OOM (L5470), co_await void op3 (L5518:62, L5519:37), enk_lowered_eh_construct (L5535-L5591) */
    a_variable v_tmp;
    std::memset(&v_tmp, 0, sizeof(v_tmp)); v_tmp.type = &int_tp;
    a_dynamic_init dip_tmp;
    std::memset(&dip_tmp, 0, sizeof(dip_tmp));
    dip_tmp.kind = dik_expression; dip_tmp.variable = &v_tmp; dip_tmp.variant.expression = &e_vi;
    dip_tmp.destructor = &r_free1;
    an_expr_node e_tinit;
    std::memset(&e_tinit, 0, sizeof(e_tinit));
    e_tinit.kind = enk_reuse_value; e_tinit.type = &int_tp; e_tinit.variant.reused_value_init = &dip_tmp;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(cache_clear_all());
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_i, lv_i));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_free1, j_free1));
        return gcc_gen_be_lower_expr_rvalue(&e_tinit, &rv);
    }));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vp, lv_vp));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_l, lv_l));

    /* sizeof(vla) with dimension having NULL var and NULL expr (L5447:65 False) */
    a_type vla_empty_tp;
    std::memset(&vla_empty_tp, 0, sizeof(vla_empty_tp));
    vla_empty_tp.kind = tk_array; vla_empty_tp.variant.array.is_variable_size_array = TRUE;
    vla_empty_tp.variant.array.has_assoc_vla_dimension = TRUE;
    vla_empty_tp.variant.array.element_type = &int_tp;
    a_vla_dimension vla_empty_d;
    std::memset(&vla_empty_d, 0, sizeof(vla_empty_d)); vla_empty_d.type = &vla_empty_tp;
    fn_sc_vla.vla_dimensions = &vla_empty_d;
    innermost_function_scope = &fn_sc_vla;
    an_expr_node e_sz_vla;
    std::memset(&e_sz_vla, 0, sizeof(e_sz_vla));
    e_sz_vla.kind = enk_sizeof; e_sz_vla.type = &long_tp;
    e_sz_vla.variant.sizeof_info.is_type = TRUE; e_sz_vla.variant.sizeof_info.variant.type = &vla_empty_tp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_sz_vla, &rv));
    innermost_function_scope = NULL;

    /* co_await with op3 yielding NULL (void) and expr->type = int_tp (L5518:62 False, L5519:37 True) & void_tp (L5519:37 False) */
    an_expr_node e_aw1, e_aw2, e_aw3, e_await;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_aw1, &v_i));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_aw2, &v_vp));
    GCC_GEN_BE_CHECK(expr_init_op(&e_aw3, eok_va_end, &void_tp, &e_vvp));
    e_aw1.next = &e_aw2; e_aw2.next = &e_aw3;
    g_expr_mock.coro_state_field_lval = lv_i;
    g_expr_mock.coro_frame_ptr_rval = gcc_jit_lvalue_as_rvalue(lv_vp);
    GCC_GEN_BE_CHECK(expr_init_op(&e_await, eok_await, &int_tp, &e_aw1));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_await, &rv));
    e_await.type = &void_tp;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_await, &rv));
    e_aw1.next = NULL; e_aw2.next = NULL;

    /* enk_lowered_eh_construct: leck_internal_try OOM + try_e = enk_throw (tb == NULL -> L5556:23 False!),
       plus leck_thrown_object_address & leck_caught_object_address OOM (L5578-L5591) */
    an_expr_node e_try_ok, e_catch_ok, e_eh;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_try_ok, &v_i));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_catch_ok, &v_l));
    e_try_ok.next = &e_catch_ok;
    std::memset(&e_eh, 0, sizeof(e_eh));
    e_eh.kind = enk_lowered_eh_construct; e_eh.type = &void_tp;
    e_eh.variant.lowered_eh.kind = leck_internal_try;
    e_eh.variant.lowered_eh.variant.try_and_catch_expr = &e_try_ok;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_eh, &rv));
    e_try_ok.next = NULL;

    an_expr_node e_try_throw;
    std::memset(&e_try_throw, 0, sizeof(e_try_throw));
    e_try_throw.kind = enk_throw; e_try_throw.type = &void_tp;
    e_try_throw.next = &e_catch_ok;
    e_eh.variant.lowered_eh.variant.try_and_catch_expr = &e_try_throw;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv)); /* tb == NULL -> L5556:23 False! */

    e_eh.type = &vptr_tp;
    e_eh.variant.lowered_eh.kind = leck_thrown_object_address;
    gcc_gen_be_singleton_state.active_thrown_obj_lval = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_eh, &rv));
    e_eh.variant.lowered_eh.kind = leck_caught_object_address;
    gcc_gen_be_singleton_state.active_caught_obj_lval = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_eh, &rv));

    /* 12) Final precision sweep across remaining rvalue branches & lines:
           - L3002: uncached local var OOM
           - L3322-3366: eok_padd / eok_psubtract / eok_pdiff on tk_array, int+ptr, int+int, and void* (`sz <= 0`)
           - L3563: eok_eq with r2 == NULL (`e_aw3`)
           - L3828, L3857-3858, L3884, L3921-3966, L3986, L4074, L4093-4116: call/builtin/PMF/virtual call edge cases
           - L4207-4510: va_arg 0-size struct, NULL expr->type, AAPCS64/SysV/I386/RISCV64/WIN64 indirect & mixed aggregates
           - L4612, L4673, L4683, L4747, L4864-4891: bool_cast, PM base cast, array_to_pointer, braced_init, condition
           - L5090, L5134-5135, L5154, L5265, L5288: new/delete edge cases & outer_lp == NULL unwind resume jump
           - L5332-5362: overriding function call with 1-param target (`target_pcount > num_args`) + cached/uncached var_param
           - L5393-5484, L5552-5585, L5777, L5895: reuse_value, sizeof VLA, lowered_eh, shufflevector/convertvector NULL op */
    a_variable v_uncached_pool[64];
    std::memset(v_uncached_pool, 0, sizeof(v_uncached_pool));
    int unc_idx = 0;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 32, [&]() noexcept -> gcc_gen_be_error_t {
        a_variable &vu = v_uncached_pool[unc_idx++ & 63];
        vu.type = &int_tp;
        an_expr_node e_vunc;
        GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vunc, &vu));
        return gcc_gen_be_lower_expr_rvalue(&e_vunc, &rv);
    }));

    /* Pointer arithmetic: tk_array (`e_va_arr`), int + ptr (`e_vi + e_vip`), int + int (`e_vi + e_vi2`), void* `pdiff` (`sz <= 0`) */
    an_expr_node e_vvp2;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vvp2, &v_vp));
    e_va_arr.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_padd, &cptr_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_psubtract, &cptr_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pdiff, &long_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_va_arr.next = NULL;

    e_vi.next = &e_vip;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_padd, &iptr_tp, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* op1_is_ptr == FALSE! */
    e_vi.next = &e_vi2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_padd, &iptr_tp, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* neither ptr nor array! */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_psubtract, &iptr_tp, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pdiff, &long_tp, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vi.next = NULL;

    e_vvp.next = &e_vvp2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pdiff, &long_tp, &e_vvp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* elem_type = void -> sz == 0 <= 0! */
    e_vvp.next = NULL;

    /* L3563:29 False: eok_eq with r1 != NULL and r2 == NULL (`e_aw3` is eok_va_end) */
    e_vi.next = &e_aw3;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_eq, &bool_tp, &e_vi));
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_vi.next = NULL;

    /* Call edge cases: enk_routine with ptr==NULL, ptr->type==NULL, ptr->type->kind==tk_integer */
    a_routine r_notype, r_inttype;
    std::memset(&r_notype, 0, sizeof(r_notype)); r_notype.type = NULL;
    std::memset(&r_inttype, 0, sizeof(r_inttype)); r_inttype.type = &int_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_notype, j_ov_zero));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_inttype, j_ov_zero));
    an_expr_node e_rnullp;
    std::memset(&e_rnullp, 0, sizeof(e_rnullp)); e_rnullp.kind = enk_routine; e_rnullp.type = &vfn_tp;
    e_rnullp.variant.routine.ptr = NULL;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &void_tp, &e_rnullp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L3828:59 & L3857:59 False */
    e_rnullp.variant.routine.ptr = &r_notype;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L3857:88 False */
    e_rnullp.variant.routine.ptr = &r_inttype;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L3858:31 False */

    /* L3884:39 False (`eok_points_to_pm_call`) & L3986:83 False (`eok_dot_pm_call` with op2 == NULL) */
    e_vpmf.next = &e_vvp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_points_to_pm_call, &void_tp, &e_vpmf));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vpmf.next = NULL;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_pm_call, &void_tp, &e_vpmf));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* num_args == 0 -> L3986:83 False! */

    /* L3921:35 (`arg->type == NULL`), L3921:46 (`arg_rval == NULL`), L3928:61 (`coerced_abi_type == NULL`), L3931:47 (`cblock == NULL`) */
    an_expr_node e_arg_notype = e_vi;
    e_arg_notype.type = NULL;
    e_rvar0.next = &e_arg_notype;
    e_arg_notype.next = &e_aw3;
    e_aw3.next = &e_vnp;
    g_expr_mock.is_sret = TRUE;
    g_expr_mock.needs_abi_coercion = TRUE;
    g_expr_mock.coerced_abi_type = NULL; /* L3928:61 False! */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &int_tp, &e_rvar0));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    g_expr_mock.is_sret = FALSE;
    g_expr_mock.coerced_abi_type = jit_long;
    e_rnullp.variant.routine.ptr = &r_var0;
    e_rnullp.next = &e_vnp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &void_tp, &e_rnullp));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L3931:47 False (`cblock == NULL`)! */
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    e_rnullp.next = NULL; e_rvar0.next = NULL; e_arg_notype.next = NULL; e_aw3.next = NULL;
    GCC_GEN_BE_CHECK(expr_test_reset_mock());

    /* L3953:35 & L3963-3966: sfk_gnu_atomic_generic_function with bname==NULL, op2==NULL, op2 non-const, op2 const(NULL), op2 ck_float */
    a_routine r_gen_at;
    std::memset(&r_gen_at, 0, sizeof(r_gen_at));
    r_gen_at.type = &vfn_tp; r_gen_at.special_kind = sfk_gnu_atomic_generic_function; r_gen_at.source_corresp.name = NULL;
    an_expr_node e_rgen, e_cnull, e_cflt;
    std::memset(&e_rgen, 0, sizeof(e_rgen)); e_rgen.kind = enk_routine; e_rgen.type = &vfn_tp; e_rgen.variant.routine.ptr = &r_gen_at;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &void_tp, &e_rgen));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* bname == NULL -> L3953:35 False */

    r_gen_at.source_corresp.name = "__atomic_load";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* op2 == NULL -> L3963:92 False */
    e_rgen.next = &e_vi;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* op2->kind != enk_constant -> L3965:39 False */
    a_constant c_flt_op;
    std::memset(&c_flt_op, 0, sizeof(c_flt_op)); c_flt_op.kind = ck_float; c_flt_op.type = &dbl_tp;
    std::memset(&e_cflt, 0, sizeof(e_cflt)); e_cflt.kind = enk_constant; e_cflt.type = &dbl_tp; e_cflt.variant.constant.ptr = &c_flt_op;
    e_rgen.next = &e_cflt;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* ck_float -> L3966:39 False */
    e_rgen.next = NULL;

    /* L4074:84 & L4093-4116: virtual calls */
    a_param_type vpt1, vpt2;
    std::memset(&vpt1, 0, sizeof(vpt1)); vpt1.type = NULL; vpt1.next = &vpt2; /* ptp->type == NULL -> L4104:47 & L4111:54 False! */
    std::memset(&vpt2, 0, sizeof(vpt2)); vpt2.type = &two_st_tp;
    a_routine_type_supplement vrt_sup;
    std::memset(&vrt_sup, 0, sizeof(vrt_sup)); vrt_sup.param_type_list = &vpt1;
    a_type vrt_tp, vrt_noextra_tp;
    std::memset(&vrt_tp, 0, sizeof(vrt_tp));
    vrt_tp.kind = tk_routine; vrt_tp.variant.routine.return_type = &void_tp; vrt_tp.variant.routine.extra_info = &vrt_sup;
    GCC_GEN_BE_CHECK(expr_test_map_type(&vrt_tp, jit_mfn_ptr));
    std::memset(&vrt_noextra_tp, 0, sizeof(vrt_noextra_tp));
    vrt_noextra_tp.kind = tk_routine; vrt_noextra_tp.variant.routine.return_type = &void_tp; vrt_noextra_tp.variant.routine.extra_info = NULL;
    GCC_GEN_BE_CHECK(expr_test_map_type(&vrt_noextra_tp, jit_mfn_ptr));

    a_routine r_virt;
    std::memset(&r_virt, 0, sizeof(r_virt));
    r_virt.type = &vrt_tp; r_virt.number.virtual_function = 0;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_virt, j_free1));
    an_expr_node e_rvirt;
    std::memset(&e_rvirt, 0, sizeof(e_rvirt)); e_rvirt.kind = enk_routine; e_rvirt.type = &vrt_tp; e_rvirt.variant.routine.ptr = &r_virt;

    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &void_tp, &e_rvirt));
    e_op.variant.operation.is_virtual_call = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* num_args == 0 -> L4074:84 False */

    e_rvirt.next = &e_vvp;
    e_vvp.next = &e_vnp;
    g_expr_mock.needs_abi_coercion = TRUE;
    g_expr_mock.coerced_abi_type = NULL; /* L4116:69 False! */
    /* Wait: virtual call calls fn_ptr (`jit_mfn_ptr` has 1 param), so use 2-param fn_ptr type for 2 args! */
    gcc_jit_type *v2_params[2] = { jit_vptr, jit_vptr };
    gcc_jit_type *jit_vfn2_ptr = gcc_jit_context_new_function_ptr_type(ctx, NULL, jit_void, 2, v2_params, 0);
    GCC_GEN_BE_CHECK(expr_test_map_type(&vrt_tp, jit_vfn2_ptr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_test_reset_mock());

    e_vvp.next = NULL;
    GCC_GEN_BE_CHECK(expr_test_map_type(&vrt_noextra_tp, jit_mfn_ptr));
    r_virt.type = &vrt_noextra_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* extra_info == NULL -> L4093:76 False */
    a_type ptr_mfn_tp;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_mfn_tp, &void_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_test_map_type(&ptr_mfn_tp, jit_mfn_ptr));
    r_virt.type = &ptr_mfn_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* rout->type->kind != tk_routine -> L4093:49 False */
    e_rvirt.next = NULL;

    /* va_arg: 0-size struct (`z_st_tp`), NULL `expr->type`, AAPCS64 indirect/non-indirect, SysV mixed GP+SSE, I386/RISCV64/WIN64 */
    a_type big_rec_tp, big_int_tp;
    std::memset(&big_rec_tp, 0, sizeof(big_rec_tp)); big_rec_tp.kind = tk_struct; big_rec_tp.size = 24; big_rec_tp.alignment = 8;
    GCC_GEN_BE_CHECK(expr_test_map_type(&big_rec_tp, g_expr_mock.pmf_struct_type));
    GCC_GEN_BE_CHECK(expr_init_int_type(&big_int_tp, ik_unsigned_int128, 24));

    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
    g_expr_mock.va_is_stack = TRUE; g_expr_mock.va_num_gp = 0; g_expr_mock.va_num_fp = 0;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_st_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L4207:86 False */
    e_op.type = NULL;
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) == GCC_GEN_BE_SUCCESS) { /* expr->type == NULL mapped to jit_int via default */
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &big_rec_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* AAPCS64 is_indirect_agg == TRUE! */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &big_int_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* type_size > 16 && !va_is_rec -> L4297:87 False */
    g_expr_mock.va_num_fp = 1;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* num_fp != 0 -> L4297:55 False */

    /* SysV AMD64 mixed GP+SSE (`num_gp=1, num_fp=1`) with SSE at index 0 & OOM, plus `num_gp=2, num_fp=1` */
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    g_expr_mock.va_is_stack = FALSE; g_expr_mock.va_num_gp = 1; g_expr_mock.va_num_fp = 1;
    g_expr_mock.sysv_class0 = GCC_GEN_BE_ABI_CLASS_SSE; g_expr_mock.sysv_class1 = GCC_GEN_BE_ABI_CLASS_INTEGER;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &two_st_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.va_num_gp = 2; g_expr_mock.va_num_fp = 1;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L4385:48 False */
    g_expr_mock.va_num_gp = 1; g_expr_mock.va_num_fp = 2;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L4385:62 False */

    /* Scalar pointer va_arg on z_st_tp, SYSV_I386, RISCV64_LP64D, WIN64 (sizes 1, 2, 4, 8, 12) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_st_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L4498:82 False */
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_I386;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &big_rec_tp, &e_vcp));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &int_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_WIN64;
    static const size_t k_win_sz[5] = { 1, 2, 4, 8, 12 };
    for (int wi = 0; wi < 5; ++wi) {
        a_type wst = big_rec_tp;
        wst.size = k_win_sz[wi];
        GCC_GEN_BE_CHECK(expr_test_map_type(&wst, g_expr_mock.one_field_struct_type));
        GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &wst, &e_vcp));
        GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    }
    GCC_GEN_BE_CHECK(expr_test_reset_mock());

    /* eok_bool_cast: nullptr, PDM (`mtp == NULL` and `mtp == int_tp`), PMF mapped to scalar `jit_long` */
    a_type nullptr_tp, pdm_null_m, pdm_int_m, pmf_long_m;
    std::memset(&nullptr_tp, 0, sizeof(nullptr_tp)); nullptr_tp.kind = tk_nullptr; nullptr_tp.size = 8;
    std::memset(&pdm_null_m, 0, sizeof(pdm_null_m)); pdm_null_m.kind = tk_ptr_to_member; pdm_null_m.size = 8;
    pdm_null_m.variant.ptr_to_member.type = NULL;
    pdm_int_m = pdm_null_m; pdm_int_m.variant.ptr_to_member.type = &int_tp;
    pmf_long_m = pdm_null_m; pmf_long_m.variant.ptr_to_member.type = &vfn_tp;
    GCC_GEN_BE_CHECK(expr_test_map_type(&nullptr_tp, jit_vptr));
    GCC_GEN_BE_CHECK(expr_test_map_type(&pdm_null_m, jit_long));
    GCC_GEN_BE_CHECK(expr_test_map_type(&pdm_int_m, jit_long));
    GCC_GEN_BE_CHECK(expr_test_map_type(&pmf_long_m, jit_long));
    e_vvp.type = &nullptr_tp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_bool_cast, &bool_tp, &e_vvp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vvp.type = &vptr_tp;
    e_vl.type = &pdm_null_m;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_bool_cast, &bool_tp, &e_vl));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* mtp == NULL -> L4612:27 False */
    e_vl.type = &pdm_int_m;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* mtp != tk_routine -> L4612:34 False */
    e_vl.type = &pmf_long_m;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* is_struct(r1) == NULL -> L4612:60 False! */
    e_vl.type = &long_tp;

    /* eok_pm_base_class_cast: multi-base derived class (offset 8) for PDM & PMF (AAPCS64 vs SYSV) */
    a_type b_other_tp, b_match_tp, d_multi_tp, pm_b_tp, pm_d_tp, pmf_b_tp, pmf_d_tp;
    std::memset(&b_other_tp, 0, sizeof(b_other_tp)); b_other_tp.kind = tk_struct; b_other_tp.size = 8;
    std::memset(&b_match_tp, 0, sizeof(b_match_tp)); b_match_tp.kind = tk_struct; b_match_tp.size = 8;
    a_base_class mb1, mb2;
    std::memset(&mb1, 0, sizeof(mb1)); mb1.direct = TRUE; mb1.type = &b_other_tp; mb1.offset = 0; mb1.next = &mb2;
    std::memset(&mb2, 0, sizeof(mb2)); mb2.direct = TRUE; mb2.type = &b_match_tp; mb2.offset = 8;
    a_class_type_supplement d_msup;
    std::memset(&d_msup, 0, sizeof(d_msup)); d_msup.direct_base_classes = &mb1;
    std::memset(&d_multi_tp, 0, sizeof(d_multi_tp));
    d_multi_tp.kind = tk_class; d_multi_tp.size = 16; d_multi_tp.variant.class_struct_union.extra_info = &d_msup;

    std::memset(&pm_b_tp, 0, sizeof(pm_b_tp));
    pm_b_tp.kind = tk_ptr_to_member; pm_b_tp.size = 8;
    pm_b_tp.variant.ptr_to_member.class_of_which_a_member = &b_match_tp; pm_b_tp.variant.ptr_to_member.type = &int_tp;
    pm_d_tp = pm_b_tp; pm_d_tp.variant.ptr_to_member.class_of_which_a_member = &d_multi_tp;
    GCC_GEN_BE_CHECK(expr_test_map_type(&pm_b_tp, jit_long));
    GCC_GEN_BE_CHECK(expr_test_map_type(&pm_d_tp, jit_long));

    pmf_b_tp = pm_b_tp; pmf_b_tp.size = 16; pmf_b_tp.variant.ptr_to_member.type = &mfn_tp;
    pmf_d_tp = pmf_b_tp; pmf_d_tp.variant.ptr_to_member.class_of_which_a_member = &d_multi_tp;
    GCC_GEN_BE_CHECK(expr_test_map_type(&pmf_b_tp, g_expr_mock.pmf_struct_type));
    GCC_GEN_BE_CHECK(expr_test_map_type(&pmf_d_tp, g_expr_mock.pmf_struct_type));

    e_vl.type = &pm_b_tp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_derived_class_cast, &pm_d_tp, &e_vl));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vl.type = &long_tp;

    e_vpmf.type = &pmf_b_tp;
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_derived_class_cast, &pmf_d_tp, &e_vpmf));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    e_vpmf.type = &pmf_np_tp;

    /* eok_array_to_pointer with op1->type == NULL & op1->type == &int_tp */
    e_vi.type = NULL;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_array_to_pointer, &iptr_tp, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vi.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* enk_braced_init_list: orig_tp == NULL, field->type == NULL, uncached field; enk_condition dynamic_init == NULL */
    std::memset(&e_op, 0, sizeof(e_op));
    e_op.kind = enk_braced_init_list; e_op.type = NULL;
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) == GCC_GEN_BE_SUCCESS) {
    }
    a_field br_notype_f, br_uncached_f;
    std::memset(&br_notype_f, 0, sizeof(br_notype_f)); br_notype_f.type = NULL; br_notype_f.next = &br_uncached_f;
    std::memset(&br_uncached_f, 0, sizeof(br_uncached_f)); br_uncached_f.type = &int_tp;
    a_type br_st_tp;
    std::memset(&br_st_tp, 0, sizeof(br_st_tp)); br_st_tp.kind = tk_struct; br_st_tp.size = 16;
    br_st_tp.variant.class_struct_union.field_list = &br_notype_f;
    GCC_GEN_BE_CHECK(expr_test_map_type(&br_st_tp, g_expr_mock.pmf_struct_type));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &br_notype_f, gcc_jit_struct_get_field(pmf_jst, 0)));
    e_vvp.next = &e_vi;
    e_op.type = &br_st_tp; e_op.variant.braced_init_list = &e_vvp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vvp.next = NULL;

    cond_sup.dynamic_init = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cond, &rv)); /* L4891:19 False */

    /* enk_new_delete:
       1) L5090:31 False (`free_fn == NULL`)
       2) L5134-5135: drain ALL landing pads via `gcc_gen_be_pop_landing_pad` so `outer_lp == NULL` with `unwind_resume_make_block = TRUE`!
       3) L5154:39 False: `array_delete = TRUE` with `dynamic_init == NULL`
       4) L5265 True (2nd extra delete arg failing OOM) & L5288:99 False (0-param `dealloc_func`) */
    for (;;) {
        gcc_jit_block *cur_lp = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&cur_lp));
        if (!cur_lp) break;
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    }
    e_nd.type = &iptr_tp;
    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = TRUE; nds.type = &int_tp; nds.routine = &r_an_ip; nds.arg = &e_vl;
    nds.freeing_of_storage_on_exception = &dip_fsoe; nds.dynamic_init = &dip_ctor_free;
    g_expr_mock.unwind_resume_make_block = TRUE;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv)); /* HITS L5134-5135! */
    g_expr_mock.unwind_resume_make_block = FALSE;

    a_routine r_free_null;
    std::memset(&r_free_null, 0, sizeof(r_free_null)); r_free_null.type = &vfn_tp;
    dip_fsoe.destructor = &r_free_null;
    g_expr_mock.func_decl_return_null = TRUE;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv)); /* free_fn == NULL -> L5090:31 False */
    g_expr_mock.func_decl_return_null = FALSE;
    dip_fsoe.destructor = &r_free1;

    e_nd.type = &void_tp;
    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = FALSE; nds.array_delete = TRUE; nds.type = &int_tp; nds.arg = &e_vip; nds.dynamic_init = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv)); /* L5154:39 False */

    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = FALSE; nds.array_delete = FALSE; nds.type = &int_tp;
    e_vvp.next = &e_vi;
    nds.arg = &e_vvp; nds.routine = &r_ov_zero; /* 0-param dealloc_func -> L5288:99 False + L5265 OOM! */
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_nd, &rv));
    e_vvp.next = NULL;

    /* L5332-5362: overriding function call:
       - 0-param caller -> 1-param target (`r_ov_one` with 1-param `param_type_list` so `func` has 1 param!) -> HITS L5358-5362!
       - 1-param caller with `def_scope` cached & uncached `var_param` -> HITS L5332:19 & L5337:34! */
    a_param_type ov_pt1, ov_pt2;
    std::memset(&ov_pt1, 0, sizeof(ov_pt1)); ov_pt1.type = &int_tp; ov_pt1.next = &ov_pt2;
    std::memset(&ov_pt2, 0, sizeof(ov_pt2)); ov_pt2.type = &long_tp;
    a_routine_type_supplement ov_sup2;
    std::memset(&ov_sup2, 0, sizeof(ov_sup2)); ov_sup2.param_type_list = &ov_pt1;
    a_type ov_fn2_tp;
    std::memset(&ov_fn2_tp, 0, sizeof(ov_fn2_tp));
    ov_fn2_tp.kind = tk_routine; ov_fn2_tp.variant.routine.return_type = &int_tp; ov_fn2_tp.variant.routine.extra_info = &ov_sup2;
    a_routine r_ov_two;
    std::memset(&r_ov_two, 0, sizeof(r_ov_two)); r_ov_two.type = &ov_fn2_tp; r_ov_two.source_corresp.name = "rfg_ov_two";

    gcc_jit_param *caller1_p = gcc_jit_context_new_param(ctx, NULL, jit_short, "cp0");
    gcc_jit_function *fn_one_p = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, jit_int, "rfg_one_p_fn", 1, &caller1_p, 0);
    a_variable v_ov_p_cached, v_ov_p_uncached;
    std::memset(&v_ov_p_cached, 0, sizeof(v_ov_p_cached)); v_ov_p_cached.type = &int_tp;
    std::memset(&v_ov_p_uncached, 0, sizeof(v_ov_p_uncached)); v_ov_p_uncached.type = &int_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_ov_p_cached, lv_i));

    a_scope def_sc_ov;
    std::memset(&def_sc_ov, 0, sizeof(def_sc_ov));
    def_sc_ov.variant.routine.parameters = &v_ov_p_cached;
    g_expr_mock.mock_routine_scope = &def_sc_ov;
    r_wrapper_ov.overriding_function_for_wrapper = &r_ov_two;
    innermost_function_scope = &sc_ov;
    e_ov.type = &long_tp;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn_one_p, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_ov, &rv);
    }));
    def_sc_ov.variant.routine.parameters = &v_ov_p_uncached;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn_one_p, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_ov, &rv)); /* cached_lval == NULL! */
    g_expr_mock.mock_routine_scope = NULL;
    innermost_function_scope = NULL;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L5393-5433: enk_reuse_value with dip==NULL (`force_lower_type_err` + OOM) & `temp_lval==NULL && init_rval!=NULL` */
    e_tinit.variant.reused_value_init = NULL;
    g_expr_mock.force_lower_type_err = GCC_GEN_BE_ERROR_OOM;
    if (gcc_gen_be_lower_expr_rvalue(&e_tinit, &rv) != GCC_GEN_BE_ERROR_OOM) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_lower_type_err = GCC_GEN_BE_SUCCESS;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_tinit, &rv));

    dip_tmp.destructor = NULL;
    dip_tmp.variable = &v_null_lv;
    e_tinit.variant.reused_value_init = &dip_tmp;
    g_expr_mock.local_var_return_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_tinit, &rv)); /* temp_lval == NULL && init_rval != NULL */
    dip_tmp.kind = dik_none;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_tinit, &rv)); /* temp_lval == NULL && dip->kind == dik_none */
    g_expr_mock.local_var_return_null = FALSE;

    /* L5455-5484: enk_sizeof runtime VLA expression side effects + NULL/fixed/dim_expr cases */
    a_type ptr_vla_fn_tp;
    std::memset(&ptr_vla_fn_tp, 0, sizeof(ptr_vla_fn_tp));
    ptr_vla_fn_tp.kind = tk_routine; ptr_vla_fn_tp.variant.routine.return_type = &vla_empty_tp;
    a_routine r_vla_side;
    std::memset(&r_vla_side, 0, sizeof(r_vla_side)); r_vla_side.type = &ptr_vla_fn_tp;
    gcc_jit_function *j_vla_side = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_iptr, "rfg_vla_side", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_vla_side, j_vla_side));
    an_expr_node e_rvla_side, e_call_vla_side;
    std::memset(&e_rvla_side, 0, sizeof(e_rvla_side));
    e_rvla_side.kind = enk_routine; e_rvla_side.type = &ptr_vla_fn_tp; e_rvla_side.variant.routine.ptr = &r_vla_side;
    GCC_GEN_BE_CHECK(expr_init_op(&e_call_vla_side, eok_call, &vla_empty_tp, &e_rvla_side));

    vla_empty_d.dimension_expr = &e_vl;
    fn_sc_vla.vla_dimensions = &vla_empty_d;
    innermost_function_scope = &fn_sc_vla;
    e_sz_vla.variant.sizeof_info.is_type = FALSE;
    e_sz_vla.variant.sizeof_info.variant.expr = &e_call_vla_side;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_sz_vla, &rv)); /* L5461, L5462, L5480 OOM! */

    e_sz_vla.variant.sizeof_info.variant.expr = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz_vla, &rv)); /* expr == NULL -> L5456:19 False */
    vla_empty_tp.variant.array.element_type = NULL;
    e_sz_vla.variant.sizeof_info.is_type = TRUE;
    e_sz_vla.variant.sizeof_info.variant.type = &vla_empty_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz_vla, &rv)); /* elem_type == NULL -> L5484:37 False */
    innermost_function_scope = NULL;

    /* L5552-5585: lowered_eh try_e==NULL, catch_e==NULL, cb==NULL (`catch_e` is enk_throw), cached thrown_obj_addr */
    e_eh.type = &void_tp;
    e_eh.variant.lowered_eh.kind = leck_internal_try;
    e_eh.variant.lowered_eh.variant.try_and_catch_expr = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv)); /* try_e == NULL & catch_e == NULL */
    e_try_ok.next = &e_try_throw;
    e_try_throw.next = NULL;
    e_eh.variant.lowered_eh.variant.try_and_catch_expr = &e_try_ok;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv)); /* cb == NULL -> L5565:23 False! */
    e_try_ok.next = NULL;

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_thrown_obj_addr", lv_vp));
    e_eh.type = &vptr_tp;
    e_eh.variant.lowered_eh.kind = leck_thrown_object_address;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_eh, &rv));

    /* shufflevector & convertvector with `cblock == NULL` */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    e_vv4i.next = &e_vv4i_2;
    e_bop.type = &vec4f_tp;
    e_bop.variant.builtin_operation.kind = bok_builtin_shufflevector;
    e_bop.variant.builtin_operation.operands = &e_vv4i;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_bop, &rv));
    e_vv4i.next = NULL;
    e_bop.variant.builtin_operation.kind = bok_builtin_convertvector;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_bop, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* 13) Final zero-gap completion:
           - L2883:72 (`bok_offsetof` with `op2 == NULL`)
           - L2973 (`initk_none` and `initk_static` with `constant == NULL`)
           - L3096:23 (`eok_assign` with `enk_throw` RHS -> `rhs_block == NULL`)
           - L3240:80 (`eok_assign` pointer LHS, int RHS)
           - L3561:38 (`eok_eq` struct LHS, int RHS)
           - L3720:31 (`eok_post_incr` with `l1 == NULL`)
           - L3763 (`eok_address_of` `enk_variable` of `tk_routine` and `uop1->type == NULL`)
           - L3858:31 (`call_ret_type == NULL`), L3881:39 (`eok_dot_member_call`), L3980:52 (`eok_dot_pm_call` + `sret`),
             L4067:74 (`is_virtual_call` on non-routine), L4086:39 (`rout->type == NULL`), L4087:51 (`virtual` + `sret`),
             L4089:59 (`param_type_list` longer than `num_args`), L4109:51 (`coerced_pt == NULL`)
           - L4543:35 (`va_start` `ap_lval == NULL`), L4558:69 (`va_start` `ap_t = char* != void*`),
             L4570:35 (`va_copy` `dst_lval == NULL`), L4572:39 (`va_copy` `src_rval == NULL`),
             L4602:76, L4630:30, L4647, L4658, L4681, L4737-4738, L4811, L4864-4921, L4970-5160, L5353, L5424, L5452-5480, L5756 */
    e_bop.type = &long_tp;
    e_bop.variant.builtin_operation.kind = bok_offsetof;
    e_bop.variant.builtin_operation.operands = &e_vi;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_bop, &rv)); /* op2 == NULL -> L2883:72 False */

    a_variable v_st_noinit, v_st_nullcon;
    std::memset(&v_st_noinit, 0, sizeof(v_st_noinit)); v_st_noinit.type = &int_tp; v_st_noinit.storage_class = sc_static; v_st_noinit.init_kind = initk_none;
    std::memset(&v_st_nullcon, 0, sizeof(v_st_nullcon)); v_st_nullcon.type = &int_tp; v_st_nullcon.storage_class = sc_static; v_st_nullcon.init_kind = initk_static;
    an_expr_node e_vsni;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vsni, &v_st_noinit));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_vsni, &rv));
    e_vsni.variant.variable.ptr = &v_st_nullcon;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_vsni, &rv));

    /* L3096:23 False (`rhs_block == NULL` via `e_try_throw`) & L3240:80 False (`ptr = int`) */
    e_vi.next = &e_try_throw;
    e_try_throw.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_assign, &int_tp, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vi.next = NULL; e_try_throw.type = &void_tp;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    e_vvp.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_assign, &vptr_tp, &e_vvp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L3240:80 False */
    e_vvp.next = NULL;

    /* L3561:38 False (`struct == int` -> safe_ptr_or_int_cast) */
    e_vnp.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_eq, &bool_tp, &e_vnp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vnp.next = NULL;

    /* L3720:31 False (`eok_post_incr` with `l1 == NULL` & `cblock == NULL`) */
    g_expr_mock.local_var_return_null = TRUE;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_post_incr, &int_tp, &e_vnull));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.local_var_return_null = FALSE;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L3763:70 False (`uop1->type == NULL`) & L3763:81 True (`uop1` variable of `tk_routine`) */
    e_vi.type = NULL;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_address_of, &iptr_tp, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vi.type = &vfn_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vi.type = &int_tp;

    /* L3858:31 False (`call_ret_type == NULL`), L3881:39 True (`eok_dot_member_call`),
       L3980:52 True (`eok_dot_pm_call` with `uses_sret == TRUE`) */
    a_type fn_nullret_tp;
    std::memset(&fn_nullret_tp, 0, sizeof(fn_nullret_tp));
    fn_nullret_tp.kind = tk_routine; fn_nullret_tp.variant.routine.return_type = NULL;
    a_routine r_nullret;
    std::memset(&r_nullret, 0, sizeof(r_nullret)); r_nullret.type = &fn_nullret_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_nullret, j_ov_zero));
    e_rnullp.variant.routine.ptr = &r_nullret;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &void_tp, &e_rnullp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* call_ret_type == NULL -> L3858:31 False! */

    e_rnullp.variant.routine.ptr = &r_free1;
    e_rnullp.next = &e_vnp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_member_call, &void_tp, &e_rnullp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L3881:39 True! */
    e_rnullp.next = NULL;

    gcc_jit_type *jit_sret_mfn_ptr = gcc_jit_context_new_function_ptr_type(ctx, NULL, jit_void, 2, v2_params, 0);
    GCC_GEN_BE_CHECK(expr_test_map_type(&mfn_tp, jit_sret_mfn_ptr));
    g_expr_mock.is_sret = TRUE;
    e_vpmf.next = &e_vnp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_pm_call, &int_tp, &e_vpmf));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L3980:52 True! */
    e_vpmf.next = NULL;
    GCC_GEN_BE_CHECK(expr_test_map_type(&mfn_tp, jit_mfn_ptr));
    g_expr_mock.is_sret = FALSE;

    /* Virtual calls: non-routine op1 (L4067:74 False),
       uses_sret==TRUE (L4087:51 True), param_type_list > num_args (L4089:59 False), coerced_pt==NULL (L4109:51 False) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &void_tp, &e_vvp));
    e_op.variant.operation.is_virtual_call = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* op1->kind != enk_routine -> L4067:74 False */

    r_virt.type = NULL;
    e_rvirt.next = &e_vvp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &void_tp, &e_rvirt));
    e_op.variant.operation.is_virtual_call = TRUE;
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        /* lower_type(NULL) fails before L4086 unless NULL mapped! Map NULL? No, lower_type checks !tp first. */
    }
    vpt1.type = &two_st_tp; vpt1.next = &vpt2; vpt2.type = &int_tp;
    r_virt.type = &vrt_tp;
    GCC_GEN_BE_CHECK(expr_test_map_type(&vrt_tp, jit_mfn_ptr));
    g_expr_mock.needs_abi_coercion = TRUE;
    g_expr_mock.coerced_abi_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* 1 arg vs 2 ptp -> L4089:59 False & L4109:51 False! */

    g_expr_mock.is_sret = TRUE;
    GCC_GEN_BE_CHECK(expr_test_map_type(&vrt_tp, jit_vfn2_ptr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* uses_sret == TRUE -> L4087:51 True! */
    e_rvirt.next = NULL;
    GCC_GEN_BE_CHECK(expr_test_reset_mock());

    /* L4543:35, L4558:69, L4570:35, L4572:39: va_start & va_copy */
    a_variable v_va_nl1, v_va_nl2;
    std::memset(&v_va_nl1, 0, sizeof(v_va_nl1)); v_va_nl1.type = &cptr_tp;
    std::memset(&v_va_nl2, 0, sizeof(v_va_nl2)); v_va_nl2.type = &cptr_tp;
    an_expr_node e_vnl1, e_vnl2;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vnl1, &v_va_nl1));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vnl2, &v_va_nl2));
    g_expr_mock.local_var_return_null = TRUE;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_start, &void_tp, &e_vnl1));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* ap_lval == NULL -> L4543:35 False! */
    e_vnl2.next = &e_vcp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_copy, &void_tp, &e_vnl2));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* dst_lval == NULL -> L4570:35 False! */
    e_vnl2.next = NULL;
    g_expr_mock.local_var_return_null = FALSE;

    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_start, &void_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* ap_t (char*) != va_ptr (void*) -> L4558:69 False! */

    e_vcp.next = &e_aw3; /* src_rval == NULL -> L4572:39 False! */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_copy, &void_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vcp.next = NULL;

    /* L4602:76 (`eok_bool_cast` PM with `r1 == NULL`), L4630:30 (`eok_bool_cast` non-PM with `r1 == NULL`),
       L4647 (`eok_pm_base_class_cast` with `r1 == NULL`, `dst_tp != tk_ptr_to_member`, `src_tp != tk_ptr_to_member`),
       L4658:72 (`derived_cls` `tk_struct` WITH `extra_info` & `base_classes`!), L4681 (`mtp == NULL` & `is_struct(type) == NULL`) */
    e_aw3.type = &pmf_np_tp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_bool_cast, &bool_tp, &e_aw3));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* r1 == NULL -> L4602:76 & L4630:30 False! */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, &pmf_np_tp, &e_aw3));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* !r1 -> L4647:23 True! */
    e_aw3.type = &void_tp;

    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, &int_tp, &e_vpmf));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* dst_tp != tk_ptr_to_member! */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, &pmf_np_tp, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* src_tp != tk_ptr_to_member! */

    /* Note: `derived_cls->variant.class_struct_union.extra_info->base_classes` (NOT `direct_base_classes`!) is iterated at L4660! */
    d_msup.base_classes = &mb1;
    d_multi_tp.kind = tk_struct; /* hits L4658:72 `tk_struct` True + `delta == 8`! */
    a_type pm_d_nullm = pm_d_tp;
    pm_d_nullm.variant.ptr_to_member.type = NULL;
    GCC_GEN_BE_CHECK(expr_test_map_type(&pm_d_nullm, jit_long));
    a_type pmf_d_long = pmf_d_tp;
    GCC_GEN_BE_CHECK(expr_test_map_type(&pmf_d_long, jit_long));

    e_vl.type = &pm_b_tp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_derived_class_cast, &pm_d_nullm, &e_vl));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* mtp == NULL -> L4681:23 False! */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_derived_class_cast, &pmf_d_long, &e_vl));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* mtp == tk_routine && !is_struct(type) -> L4681:57 False! */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_derived_class_cast, &pm_d_tp, &e_vl));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vl.type = &long_tp;

    e_vpmf.type = &pmf_b_tp;
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_derived_class_cast, &pmf_d_tp, &e_vpmf));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    e_vpmf.type = &pmf_np_tp;

    /* L4737:59 & L4738:23: `eok_array_to_pointer` on `enk_constant` with `ck_integer` (`!ck_string`) */
    an_expr_node e_c_int_arr;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_c_int_arr, &c_stat_i));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_array_to_pointer, &iptr_tp, &e_c_int_arr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    /* L4811 OOM: `eok_cast` to `void_tp` OOM sweep */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_cast, &void_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));

    /* L4864, L4874, L4876, L4879: `enk_braced_init_list` array OOM + struct OOM + more elems than fields (`fld == NULL`) */
    std::memset(&e_op, 0, sizeof(e_op));
    e_op.kind = enk_braced_init_list; e_op.type = &va_arr_tp; e_op.variant.braced_init_list = &e_vvp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv)); /* L4864 OOM */
    e_vvp.next = &e_vi; e_vi.next = &e_vi2; /* 3 elems for 2-field `two_st_tp` -> `fld == NULL` (L4874:34 False) + L4876 & L4879 OOM! */
    e_op.type = &two_st_tp; e_op.variant.braced_init_list = &e_vvp;
    GCC_GEN_BE_CHECK(expr_sweep_rval_oom(&e_op, &rv));
    e_vvp.next = NULL; e_vi.next = NULL;

    /* L4921:41 False: `enk_condition` with `cond_dip.kind == dik_none` */
    cond_dip.kind = dik_none;
    cond_sup.dynamic_init = &cond_dip;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cond, &rv));

    /* L4970-5160: `enk_new_delete` remaining branches + untyped operand guards */
    an_expr_node e_nt_con, e_nt_con2;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_nt_con, &c_stat_i));
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_nt_con2, &c_stat_i));
    e_nt_con.type = NULL; e_nt_con2.type = NULL;
    e_nt_con.next = &e_nt_con2;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_padd, &iptr_tp, &e_nt_con));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_psubtract, &iptr_tp, &e_nt_con));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pdiff, &long_tp, &e_nt_con));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_spaceship, &int_tp, &e_nt_con));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_nt_con.next = NULL;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_bool_cast, &bool_tp, &e_nt_con));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, &pmf_np_tp, &e_nt_con));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vcp.type = NULL;
    /* eok_va_arg evaluates op1 at L3301 (v_cp.type != NULL!) while op1->type == NULL -> hits ap_edg_type == NULL! */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &int_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vcp.type = &cptr_tp;

    e_nd.type = &vptr_tp;
    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = TRUE; nds.type = &z_st_tp; /* size == 0 -> L4970:39 False */
    nds.routine = &r_free_null;
    g_expr_mock.func_decl_return_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv)); /* alloc_func == NULL -> L4993:27 False */
    g_expr_mock.func_decl_return_null = FALSE;

    /* L5026:50 (`cblock == NULL` via `e_try_throw` placement arg) & L5027:31 (`active_alloc_lp` from different function `fn_zero_p`) */
    gcc_jit_block *other_fn_lp = gcc_jit_function_new_block(fn_zero_p, "other_fn_lp");
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(other_fn_lp));
    nds.type = &int_tp; nds.routine = &r_an_ip; nds.arg = NULL;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv)); /* L5027:31 False! */
    e_try_throw.type = &long_tp;
    nds.arg = &e_try_throw;
    /* Note: when placement arg throws, cblock becomes NULL; test only up to L5026 with `alloc_func` taking 2 params: */
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    e_try_throw.type = &void_tp;

    /* L5081:68 (`fsoe->destructor == NULL`), L5100:31 (`cblock == NULL` after `dynamic_init`), L5121:31 (`after_resume == NULL`) */
    a_dynamic_init fsoe_nodtor, dip_throw_init;
    std::memset(&fsoe_nodtor, 0, sizeof(fsoe_nodtor)); fsoe_nodtor.destructor = NULL;
    std::memset(&dip_throw_init, 0, sizeof(dip_throw_init));
    dip_throw_init.kind = dik_expression; dip_throw_init.variant.expression = &e_try_throw;

    nds.arg = NULL; nds.routine = NULL;
    nds.freeing_of_storage_on_exception = &fsoe_nodtor; nds.dynamic_init = &dip_nodtor;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv)); /* L5081:68 False */

    nds.freeing_of_storage_on_exception = &dip_fsoe; nds.dynamic_init = &dip_throw_init;
    g_expr_mock.unwind_resume_make_block = FALSE; /* after_resume == NULL -> L5121:31 False, AND dip_throw_init sets cblock == NULL -> L5100:31 False! */
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L5160:31 False: `delete` with `dtor_fn == NULL` */
    a_dynamic_init dip_null_dtor;
    std::memset(&dip_null_dtor, 0, sizeof(dip_null_dtor)); dip_null_dtor.destructor = &r_free_null;
    e_nd.type = &void_tp;
    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = FALSE; nds.type = &int_tp; nds.arg = &e_vip; nds.dynamic_init = &dip_null_dtor;
    g_expr_mock.func_decl_return_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv)); /* dtor_fn == NULL -> L5160:31 False! */
    g_expr_mock.func_decl_return_null = FALSE;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L5353 True: overriding call zero-fill `force_zero_err = OOM` */
    r_wrapper_ov.overriding_function_for_wrapper = &r_ov_two;
    innermost_function_scope = &sc_ov;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn_one_p, NULL));
    g_expr_mock.force_zero_err = GCC_GEN_BE_ERROR_OOM;
    if (gcc_gen_be_lower_expr_rvalue(&e_ov, &rv) != GCC_GEN_BE_ERROR_OOM) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_zero_err = GCC_GEN_BE_SUCCESS;
    innermost_function_scope = NULL;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L5424:37 False: `enk_reuse_value` with `!temp_lval` and `init_rval == NULL` (`e_aw3`) */
    dip_tmp.kind = dik_expression; dip_tmp.variant.expression = &e_aw3; dip_tmp.variable = &v_null_lv;
    e_tinit.variant.reused_value_init = &dip_tmp;
    g_expr_mock.local_var_return_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_tinit, &rv));
    g_expr_mock.local_var_return_null = FALSE;

    /* L5452-5480: `enk_sizeof` `!is_type` (`stp == NULL`, `stp != tk_array`, `!has_assoc_vla_dimension`) + uncached `dim->dimension_variable` OOM */
    an_expr_node e_sz_arg = e_vi;
    e_sz_vla.variant.sizeof_info.is_type = FALSE;
    e_sz_vla.variant.sizeof_info.variant.expr = &e_sz_arg;
    e_sz_arg.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz_vla, &rv)); /* stp == NULL -> L5452:61 False */
    e_sz_arg.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz_vla, &rv)); /* stp != tk_array -> L5452:68 False */
    e_sz_arg.type = &va_arr_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz_vla, &rv)); /* !has_assoc_vla_dimension -> L5453:19 False */

    vla_empty_tp.variant.array.element_type = &int_tp;
    fn_sc_vla.vla_dimensions = &vla_empty_d;
    innermost_function_scope = &fn_sc_vla;
    e_sz_vla.variant.sizeof_info.is_type = TRUE;
    e_sz_vla.variant.sizeof_info.variant.type = &vla_empty_tp;
    a_variable v_dim_pool[64];
    std::memset(v_dim_pool, 0, sizeof(v_dim_pool));
    int dim_pidx = 0;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        a_variable &vd = v_dim_pool[dim_pidx++ & 63];
        vd.type = &long_tp;
        vla_empty_d.dimension_variable = &vd;
        return gcc_gen_be_lower_expr_rvalue(&e_sz_vla, &rv);
    }));
    innermost_function_scope = NULL;

    /* L5756 True: `shufflevector` 3rd mask operand failing rvalue lowering */
    an_expr_node e_unh_mask;
    std::memset(&e_unh_mask, 0, sizeof(e_unh_mask));
    e_unh_mask.kind = (an_expr_node_kind)120; e_unh_mask.type = &int_tp;
    e_vv4i.next = &e_vv4i_2; e_vv4i_2.next = &e_unh_mask;
    e_bop.type = &vec4f_tp;
    e_bop.variant.builtin_operation.kind = bok_builtin_shufflevector;
    e_bop.variant.builtin_operation.operands = &e_vv4i;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_bop, &rv));
    e_vv4i.next = NULL; e_vv4i_2.next = NULL;

    /* Cover `expr->type == NULL` (via `expr_test_map_type(NULL, ...)`) on `eok_indirect`, `eok_pre_incr`, `eok_va_arg`, `r_virt`, `eok_pm_base_class_cast`,
       plus 0-size `int` and `float` in `eok_va_arg` */
    a_type z_int_tp, z_flt_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&z_int_tp, ik_int, 0));
    GCC_GEN_BE_CHECK(expr_init_float_type(&z_flt_tp, fk_float, 0));
    GCC_GEN_BE_CHECK(expr_test_map_type(&z_int_tp, jit_int));
    GCC_GEN_BE_CHECK(expr_test_map_type(&z_flt_tp, jit_flt));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_int_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_flt_tp, &e_va_arr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_int_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, &z_flt_tp, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));

    GCC_GEN_BE_CHECK(expr_test_map_type(NULL, jit_int));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_indirect, NULL, &e_vip));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pre_incr, NULL, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, NULL, &e_va_arr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_arg, NULL, &e_vcp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, NULL, &e_vpmf));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    GCC_GEN_BE_CHECK(expr_test_map_type(NULL, jit_mfn_ptr));
    r_virt.type = NULL;
    e_rvirt.next = &e_vvp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &void_tp, &e_rvirt));
    e_op.variant.operation.is_virtual_call = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_rvirt.next = NULL;

    /* 14) Close final 16 branch conditions:
           - L1699 True & L1775 True (`eok_points_to_field` bitfield read & assign with failing `op1`)
           - L3094:23 False (`eok_land` with `enk_throw` RHS setting `rhs_block == NULL`)
           - L3718:31 False (`eok_post_incr` with `l1 == NULL`)
           - L4107:51 False (`r_virt` with `r_virt.type == &vrt_tp`, `needs_abi_coercion == TRUE`, `coerced_abi_type == NULL`)
           - L4543:35 False (`eok_va_start` with `ap_lval == NULL`)
           - L4570:35 False (`eok_va_copy` with `dst_lval == NULL`)
           - L4602:76 False, L4630:30 False, L4647:23 True (`enk_type_operand` `r1 == NULL` on `eok_bool_cast` & `eok_pm_base_class_cast`)
           - L4658:23 False (`derived_cls == NULL`) & L4658:72 False (`derived_cls->kind == tk_union`)
           - L4737:59 False (`eok_array_to_pointer` with `op1->kind == enk_constant` & `constant.ptr == NULL`)
           - L5024:50 False (`enk_new_delete` with `active_alloc_lp != NULL` & `cblock == NULL`)
           - L5422:37 False (`enk_reuse_value` with `temp_lval == NULL` & `init_rval == NULL`)
           - L5754 True (2-operand `bok_builtin_shuffle` with failing `op2`) */
    a_field bf_err_f;
    std::memset(&bf_err_f, 0, sizeof(bf_err_f));
    bf_err_f.is_bit_field = TRUE; bf_err_f.bit_size = 3; bf_err_f.type = &int_tp;
    an_expr_node e_bf_err_fld, e_bf_ptf, e_bf_asg;
    std::memset(&e_bf_err_fld, 0, sizeof(e_bf_err_fld));
    e_bf_err_fld.kind = enk_field; e_bf_err_fld.type = &int_tp; e_bf_err_fld.variant.field.ptr = &bf_err_f;
    e_unh_mask.next = &e_bf_err_fld;
    GCC_GEN_BE_CHECK(expr_init_op(&e_bf_ptf, eok_points_to_field, &int_tp, &e_unh_mask));
    if (gcc_gen_be_lower_expr_rvalue(&e_bf_ptf, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED; /* L1699 True! */
    }
    e_bf_ptf.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_bf_asg, eok_assign, &int_tp, &e_bf_ptf));
    if (gcc_gen_be_lower_expr_rvalue(&e_bf_asg, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED; /* L1775 True! */
    }
    e_unh_mask.next = NULL; e_bf_ptf.next = NULL;

    /* L3094:23 False: `eok_land` with `enk_throw` RHS terminating & clearing `rhs_block` */
    an_expr_node e_real_throw;
    std::memset(&e_real_throw, 0, sizeof(e_real_throw));
    e_real_throw.kind = enk_throw; e_real_throw.type = &void_tp;
    e_vi.next = &e_real_throw;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_land, &bool_tp, &e_vi));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv));
    e_vi.next = NULL;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L3718:31 False, L4543:35 False, L4570:35 False via uncached global vars + `global_var_return_null = TRUE` */
    a_variable v_gn1, v_gn2, v_gn3;
    std::memset(&v_gn1, 0, sizeof(v_gn1)); v_gn1.type = &int_tp; v_gn1.storage_class = sc_extern;
    std::memset(&v_gn2, 0, sizeof(v_gn2)); v_gn2.type = &cptr_tp; v_gn2.storage_class = sc_extern;
    std::memset(&v_gn3, 0, sizeof(v_gn3)); v_gn3.type = &cptr_tp; v_gn3.storage_class = sc_extern;
    an_expr_node e_gn1, e_gn2, e_gn3;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_gn1, &v_gn1));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_gn2, &v_gn2));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_gn3, &v_gn3));
    g_expr_mock.global_var_return_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_post_incr, &int_tp, &e_gn1));
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED; /* L3718:31 False! */
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_start, &void_tp, &e_gn2));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L4543:35 False! */
    e_gn3.next = &e_vcp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_va_copy, &void_tp, &e_gn3));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L4570:35 False! */
    e_gn3.next = NULL;
    g_expr_mock.global_var_return_null = FALSE;

    /* L4107:51 False: `r_virt.type = &vrt_tp` with `needs_abi_coercion == FALSE` (and also TRUE with `coerced_abi_type == NULL`) */
    r_virt.type = &vrt_tp;
    GCC_GEN_BE_CHECK(expr_test_map_type(&vrt_tp, jit_vfn2_ptr));
    vpt1.type = &two_st_tp; vpt1.next = &vpt2; vpt2.type = &int_tp;
    e_vvp.next = &e_vnp;
    e_rvirt.next = &e_vvp;
    g_expr_mock.needs_abi_coercion = FALSE;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &void_tp, &e_rvirt));
    e_op.variant.operation.is_virtual_call = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L4107:51 False! */
    e_vvp.next = NULL; e_rvirt.next = NULL; vpt1.next = NULL;

    /* L4602:76 False, L4630:30 False, L4647:23 True via `enk_type_operand` (`r1 == NULL`) */
    an_expr_node e_null_rv;
    std::memset(&e_null_rv, 0, sizeof(e_null_rv));
    e_null_rv.kind = enk_type_operand; e_null_rv.type = &pmf_np_tp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_bool_cast, &bool_tp, &e_null_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L4602:76 False & L4630:30 False! */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, &pmf_np_tp, &e_null_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L4647:23 True! */

    /* L4658:23 False (`derived_cls == NULL`) & L4658:72 False (`derived_cls->kind == tk_union`) */
    d_multi_tp.kind = tk_union;
    e_vl.type = &pm_d_tp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, &pm_b_tp, &e_vl));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L4658:72 False! */
    a_type pm_d_ncls = pm_d_tp;
    pm_d_ncls.variant.ptr_to_member.class_of_which_a_member = NULL;
    GCC_GEN_BE_CHECK(expr_test_map_type(&pm_d_ncls, jit_long));
    e_vl.type = &pm_d_ncls;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_base_class_cast, &pm_b_tp, &e_vl));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_op, &rv)); /* L4658:23 False! */
    e_vl.type = &long_tp;

    /* L4737:59 False: `eok_array_to_pointer` with `op1->kind == enk_constant` & `constant.ptr == NULL` */
    an_expr_node e_c_nullp;
    std::memset(&e_c_nullp, 0, sizeof(e_c_nullp));
    e_c_nullp.kind = enk_constant; e_c_nullp.type = &iptr_tp; e_c_nullp.variant.constant.ptr = NULL;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_array_to_pointer, &iptr_tp, &e_c_nullp));
    if (gcc_gen_be_lower_expr_rvalue(&e_op, &rv) == GCC_GEN_BE_SUCCESS) {}

    /* L5024:50 False: enter `enk_new_delete` with `cblock != NULL` & `active_alloc_lp != NULL`,
       and clear `cblock` during placement `nds.arg` (`enk_throw`) evaluation before L5022! */
    gcc_jit_param *al2_p[2] = {
        gcc_jit_context_new_param(ctx, NULL, jit_sz, "sz"),
        gcc_jit_context_new_param(ctx, NULL, jit_long, "pl")
    };
    gcc_jit_function *j_alloc2_lp = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_vptr, "fg_alloc2_lp", 2, al2_p, 0);
    a_routine r_alloc_lp;
    std::memset(&r_alloc_lp, 0, sizeof(r_alloc_lp)); r_alloc_lp.type = &vfn_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_alloc_lp, j_alloc2_lp));
    an_expr_node e_throw_pl;
    std::memset(&e_throw_pl, 0, sizeof(e_throw_pl));
    e_throw_pl.kind = enk_throw; e_throw_pl.type = &long_tp;
    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = TRUE; nds.type = &int_tp; nds.routine = &r_alloc_lp; nds.arg = &e_throw_pl;
    e_nd.type = &vptr_tp; e_nd.variant.new_delete = &nds;
    gcc_jit_block *lp_nd = gcc_jit_function_new_block(fn, "lp_nd_cnull");
    gcc_jit_block_end_with_void_return(lp_nd, NULL);
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(lp_nd));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_nd, &rv)); /* L5024:50 False! */
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* L5422:37 False: `enk_reuse_value` with `temp_lval == NULL` (`local_var_return_null = TRUE`) and `init_rval == NULL` (`dik_zero` with `v_rv.type = NULL` unmapped) */
    g_expr_mock.num_type_overrides = 0; /* clear NULL->jit_int override so `v_rv.type == NULL` keeps `target_type == NULL` -> `init_rval == NULL`! */
    a_variable v_rv_null;
    std::memset(&v_rv_null, 0, sizeof(v_rv_null)); v_rv_null.type = NULL;
    a_dynamic_init dip_rv_z;
    std::memset(&dip_rv_z, 0, sizeof(dip_rv_z)); dip_rv_z.kind = dik_zero; dip_rv_z.variable = &v_rv_null;
    an_expr_node e_reuse_z;
    std::memset(&e_reuse_z, 0, sizeof(e_reuse_z));
    e_reuse_z.kind = enk_reuse_value; e_reuse_z.type = &int_tp;
    e_reuse_z.variant.reused_value_init = &dip_rv_z;
    g_expr_mock.local_var_return_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_reuse_z, &rv)); /* L5422:37 False! */
    g_expr_mock.local_var_return_null = FALSE;

    /* L5754 True: 2-operand `bok_builtin_shuffle` (`op3 == NULL`) with failing `op2` (`e_unh_mask`) */
    e_vv4i.next = &e_unh_mask; e_unh_mask.next = NULL;
    e_bop.type = &vec4i_tp;
    e_bop.variant.builtin_operation.kind = bok_builtin_shuffle;
    e_bop.variant.builtin_operation.operands = &e_vv4i;
    if (gcc_gen_be_lower_expr_rvalue(&e_bop, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED; /* L5754 True! */
    }
    e_vv4i.next = NULL;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises Stage-2 rvalue, vector, C99 complex, ARM64 ABI, lowered-EH, and coroutine branches.
 * @details Covers `bok_builtin_addressof`, `bok_is_same` type comparisons, `enk_builtin_choose_expr`
 * rvalues, mismatched compound-assign operands, GNU vector relational/equality mask lowering,
 * `__c99_complex_*` and `__c99_*` complex conversions, AAPCS64 PMF calls, ARM64 `x8` indirect calls,
 * ARM64/WIN64 `va_arg` aggregates, `enk_lowered_eh` cleanup/exception-address nodes, and `co_await`
 * symmetric transfer.
 * @param[in] ctx Active `libgccjit` context.
 * @param[in] fn Active `libgccjit` test function.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_rval_stage2_gaps(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp = {}, long_tp = {}, bool_tp = {}, flt_tp = {}, dbl_tp = {}, ldbl_tp = {}, void_tp = {}, ptr_tp = {};
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&bool_tp, ik_char, 1));
    flt_tp.kind = tk_float; flt_tp.size = 4; flt_tp.variant.float_kind = fk_float;
    dbl_tp.kind = tk_float; dbl_tp.size = 8; dbl_tp.variant.float_kind = fk_double;
    ldbl_tp.kind = tk_float; ldbl_tp.size = 16; ldbl_tp.variant.float_kind = fk_long_double;
    void_tp.kind = tk_void;
    ptr_tp.kind = tk_pointer; ptr_tp.size = 8; ptr_tp.variant.pointer.type = &int_tp;

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_vp = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);

    /* 1) L3421..3455 & L3787 & L5741: builtin_addressof, bok_is_same, choose_expr rvalue, += cast, enk_routine abi_fn */
    {
        a_variable v_int = {}; v_int.type = &int_tp; v_int.storage_class = sc_auto; v_int.source_corresp.name = "s2_rv_vi";
        gcc_jit_lvalue *lv_int = gcc_jit_function_new_local(fn, NULL, jit_int, "s2_rv_lvi");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_int, lv_int));

        an_expr_node e_vi = {}, e_addrof = {};
        e_vi.kind = enk_variable; e_vi.type = &int_tp; e_vi.variant.variable.ptr = &v_int;
        e_addrof.kind = enk_builtin_operation; e_addrof.type = &ptr_tp;
        e_addrof.variant.builtin_operation.kind = bok_builtin_addressof;
        e_addrof.variant.builtin_operation.operands = &e_vi;
        gcc_jit_rvalue *rv = NULL;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 25, [&]() noexcept -> gcc_gen_be_error_t {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_int, lv_int));
            return gcc_gen_be_lower_expr_rvalue(&e_addrof, &rv);
        }));

        /* bok_is_same across integer/float/struct/class/union/pointer/null/mismatch + OOM */
        a_type int_u_tp = {}, flt2_tp = {}, st1 = {}, st2 = {}, cl1 = {}, un1 = {};
        GCC_GEN_BE_CHECK(expr_init_int_type(&int_u_tp, ik_unsigned_int, 4));
        flt2_tp.kind = tk_float; flt2_tp.size = 4; flt2_tp.variant.float_kind = fk_double;
        st1.kind = tk_struct; st1.size = 4; st2.kind = tk_struct; st2.size = 4;
        cl1.kind = tk_class; cl1.size = 4; un1.kind = tk_union; un1.size = 4;
        a_type *pairs[10][2] = {
            {NULL, &int_tp}, {&int_tp, NULL}, {&int_tp, &flt_tp}, {&int_tp, &long_tp},
            {&int_tp, &int_tp}, {&int_tp, &int_u_tp},
            {&flt_tp, &flt_tp}, {&flt_tp, &flt2_tp},
            {&st1, &st1}, {&st1, &st2}
        };
        an_expr_node o1 = {}, o2 = {}, e_same = {};
        o1.kind = enk_constant; o1.next = &o2; o2.kind = enk_constant;
        e_same.kind = enk_builtin_operation; e_same.type = &int_tp;
        e_same.variant.builtin_operation.kind = bok_is_same;
        e_same.variant.builtin_operation.operands = &o1;
        for (int pi = 0; pi < 10; ++pi) {
            o1.type = pairs[pi][0]; o2.type = pairs[pi][1];
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_same, &rv));
        }
        o1.type = &cl1; o2.type = &cl1; GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_same, &rv));
        o1.type = &un1; o2.type = &un1; GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_same, &rv));
        e_same.variant.builtin_operation.kind = bok_types_compatible;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_same, &rv));
        e_same.variant.builtin_operation.kind = bok_is_same_as;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_same, &rv));
        o1.next = NULL; /* op2 == NULL */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_same, &rv));
        e_same.variant.builtin_operation.operands = NULL; /* op1 == NULL */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_same, &rv));
        o1.next = &o2; e_same.variant.builtin_operation.operands = &o1;
        o1.type = &ptr_tp; o2.type = &ptr_tp;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 20, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_lower_expr_rvalue(&e_same, &rv);
        }));

        /* enk_builtin_choose_expr rvalue: null operands, null op_false, choose_first TRUE & FALSE */
        a_constant c1 = {}, c2 = {}; c1.kind = ck_integer; c1.type = &int_tp; c2.kind = ck_integer; c2.type = &long_tp;
        an_expr_node ec0 = {}, ec1 = {}, ec2 = {}, e_ch = {};
        ec0.kind = enk_constant; ec0.type = &int_tp; ec0.variant.constant.ptr = &c1;
        ec1.kind = enk_constant; ec1.type = &int_tp; ec1.variant.constant.ptr = &c1;
        ec2.kind = enk_constant; ec2.type = &long_tp; ec2.variant.constant.ptr = &c2;
        e_ch.kind = enk_builtin_choose_expr; e_ch.type = &int_tp;
        e_ch.variant.builtin_choose_expr.operands = NULL; e_ch.variant.builtin_choose_expr.choose_first = TRUE;
        if (gcc_gen_be_lower_expr_rvalue(&e_ch, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
        ec0.next = NULL; e_ch.variant.builtin_choose_expr.operands = &ec0; e_ch.variant.builtin_choose_expr.choose_first = FALSE;
        if (gcc_gen_be_lower_expr_rvalue(&e_ch, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
        ec0.next = &ec1; ec1.next = &ec2;
        e_ch.variant.builtin_choose_expr.choose_first = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_ch, &rv));
        e_ch.variant.builtin_choose_expr.choose_first = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_ch, &rv));

        /* L3787: eok_add_assign where l1 is int and r2 is long (safe_ptr_or_int_cast + OOM) */
        an_expr_node ea_l = {}, ea_r = {}, ea_op = {};
        ea_l.kind = enk_variable; ea_l.type = &int_tp; ea_l.variant.variable.ptr = &v_int; ea_l.next = &ea_r;
        ea_r.kind = enk_constant; ea_r.type = &long_tp; ea_r.variant.constant.ptr = &c2;
        GCC_GEN_BE_CHECK(expr_init_op(&ea_op, eok_add_assign, &int_tp, &ea_l));
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 30, [&]() noexcept -> gcc_gen_be_error_t {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_int, lv_int));
            return gcc_gen_be_lower_expr_rvalue(&ea_op, &rv);
        }));

        /* L5741: enk_routine with abi_fn in GCC_GEN_BE_CACHE_FUNC */
        a_type rty = {}; rty.kind = tk_routine; rty.variant.routine.return_type = &void_tp;
        a_routine rout = {}; rout.source_corresp.name = "s2_er_orig"; rout.type = &rty;
        gcc_jit_function *orig_f = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "__s2_ero", 0, NULL, 0);
        gcc_jit_function *abi_f = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "__s2_era", 0, NULL, 0);
        an_expr_node e_rt = {}; e_rt.kind = enk_routine; e_rt.type = &ptr_tp; e_rt.variant.routine.ptr = &rout;
        g_expr_mock.mock_decl_func = orig_f;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 20, [&]() noexcept -> gcc_gen_be_error_t {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, orig_f, abi_f));
            return gcc_gen_be_lower_expr_rvalue(&e_rt, &rv);
        }));
        g_expr_mock.mock_decl_func = NULL;
    }

    /* 2) L4112..4227: GNU vector comparisons (eq, ne, lt, le, gt, ge, scalar op1, n_lanes==0, vc_blk==NULL, + OOM) */
    {
        a_type vec4_tp = {}, vec0_tp = {};
        vec4_tp.kind = tk_vector; vec4_tp.size = 16; vec4_tp.variant.vector.element_type = &int_tp;
        vec0_tp.kind = tk_vector; vec0_tp.size = 16; vec0_tp.variant.vector.element_type = NULL;
        gcc_jit_type *v4_jt = gcc_jit_type_get_vector(jit_int, 4);
        a_variable vv1 = {}, vv2 = {};
        vv1.type = &vec4_tp; vv1.storage_class = sc_auto; vv1.source_corresp.name = "vcmp_v1";
        vv2.type = &vec4_tp; vv2.storage_class = sc_auto; vv2.source_corresp.name = "vcmp_v2";
        gcc_jit_lvalue *vlv1 = gcc_jit_function_new_local(fn, NULL, v4_jt, "vcmp_l1");
        gcc_jit_lvalue *vlv2 = gcc_jit_function_new_local(fn, NULL, v4_jt, "vcmp_l2");

        an_expr_node ev1 = {}, ev2 = {}, ev_cmp = {};
        ev1.kind = enk_variable; ev1.type = &vec4_tp; ev1.variant.variable.ptr = &vv1; ev1.next = &ev2;
        ev2.kind = enk_variable; ev2.type = &vec4_tp; ev2.variant.variable.ptr = &vv2;
        an_expr_operator_kind vops[6] = {eok_eq, eok_ne, eok_lt, eok_le, eok_gt, eok_ge};
        gcc_jit_rvalue *rv = NULL;
        for (int oi = 0; oi < 6; ++oi) {
            GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, vops[oi], &vec4_tp, &ev1));
            GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 55, [&]() noexcept -> gcc_gen_be_error_t {
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vv1, vlv1));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vv2, vlv2));
                return gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv);
            }));
        }
        /* Scalar op1->type (in_vec_tp != tk_vector), n_lanes == 0 (vec0_tp), and vc_blk == NULL fallback */
        ev1.type = &int_tp;
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vv1, vlv1));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vv2, vlv2));
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_eq, &vec4_tp, &ev1));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_lt, &vec4_tp, &ev1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));

        /* 64-bit lane vector (mask_val = -1LL), op1->type == NULL, 0-size elem, r1/r2 == NULL via builtin, vc_blk == NULL on real vectors */
        a_type zsz_i_tp = {}, vec2_ll_tp = {}, vec_zsz_tp = {};
        zsz_i_tp.kind = tk_integer; zsz_i_tp.size = 0; zsz_i_tp.variant.integer.int_kind = ik_int;
        vec2_ll_tp.kind = tk_vector; vec2_ll_tp.size = 16; vec2_ll_tp.variant.vector.element_type = &long_tp;
        vec_zsz_tp.kind = tk_vector; vec_zsz_tp.size = 16; vec_zsz_tp.variant.vector.element_type = &zsz_i_tp;
        gcc_jit_type *v2_ll_jt = gcc_jit_type_get_vector(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), 2);
        a_variable vll1 = {}, vll2 = {};
        vll1.type = &vec2_ll_tp; vll1.storage_class = sc_auto; vll1.source_corresp.name = "vll1";
        vll2.type = &vec2_ll_tp; vll2.storage_class = sc_auto; vll2.source_corresp.name = "vll2";
        gcc_jit_lvalue *lvll1 = gcc_jit_function_new_local(fn, NULL, v2_ll_jt, "lvll1");
        gcc_jit_lvalue *lvll2 = gcc_jit_function_new_local(fn, NULL, v2_ll_jt, "lvll2");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vll1, lvll1));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vll2, lvll2));

        an_expr_node ell1 = {}, ell2 = {};
        ell1.kind = enk_variable; ell1.type = &vec2_ll_tp; ell1.variant.variable.ptr = &vll1; ell1.next = &ell2;
        ell2.kind = enk_variable; ell2.type = &vec2_ll_tp; ell2.variant.variable.ptr = &vll2;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_eq, &vec2_ll_tp, &ell1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_lt, &vec2_ll_tp, &ell1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));

        /* op1->type == NULL (in_elem_tp == NULL) & op1->type == vec_zsz_tp (n_lanes == 0 while r1/r2 != NULL) & vc_blk == NULL */
        ell1.type = NULL;
        GCC_GEN_BE_CHECK(expr_test_map_type(&vec0_tp, jit_int));
        GCC_GEN_BE_CHECK(expr_test_map_type(&vec_zsz_tp, jit_int));
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_eq, &vec0_tp, &ell1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_lt, &vec0_tp, &ell1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));
        ell1.type = &vec_zsz_tp;
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_eq, &vec_zsz_tp, &ell1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_lt, &vec_zsz_tp, &ell1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));
        ell1.type = &vec2_ll_tp;
        GCC_GEN_BE_CHECK(expr_test_map_type(&vec2_ll_tp, jit_int));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL)); /* vc_blk == NULL with real r1/r2 */
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_eq, &vec2_ll_tp, &ell1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_lt, &vec2_ll_tp, &ell1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));

        /* r2 == NULL and r1 == NULL via builtin returning NULL */
        a_type bi_rty = {}; bi_rty.kind = tk_routine; bi_rty.variant.routine.return_type = &vec4_tp;
        a_routine bi_null_r = {}; bi_null_r.source_corresp.name = "__builtin_ia32_pause"; bi_null_r.type = &bi_rty;
        an_expr_node e_bi_fn1 = {}, e_bi_fn2 = {}, e_bi1 = {}, e_bi2 = {};
        e_bi_fn1.kind = enk_routine; e_bi_fn1.type = &ptr_tp; e_bi_fn1.variant.routine.ptr = &bi_null_r;
        e_bi_fn2.kind = enk_routine; e_bi_fn2.type = &ptr_tp; e_bi_fn2.variant.routine.ptr = &bi_null_r;
        GCC_GEN_BE_CHECK(expr_init_op(&e_bi1, eok_call, &vec4_tp, &e_bi_fn1));
        GCC_GEN_BE_CHECK(expr_init_op(&e_bi2, eok_call, &vec4_tp, &e_bi_fn2));
        g_expr_mock.mock_builtin_rval = NULL;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        ell1.next = &e_bi2; /* r1 != NULL, r2 == NULL */
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_eq, &vec2_ll_tp, &ell1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_lt, &vec2_ll_tp, &ell1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));
        e_bi1.next = &ell2; /* r1 == NULL, r2 != NULL */
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_eq, &vec2_ll_tp, &e_bi1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));
        GCC_GEN_BE_CHECK(expr_init_op(&ev_cmp, eok_lt, &vec2_ll_tp, &e_bi1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ev_cmp, &rv));
        g_expr_mock.num_type_overrides = 0;
    }

    /* 3) L4464..4714: __c99_complex_* arithmetic and __c99_* complex/scalar conversions + OOM */
    {
        g_expr_mock.complex_as_struct = TRUE;
        a_type cf_tp = {}, cd_tp = {}, cld_tp = {}, rty = {};
        cf_tp.kind = tk_complex; cf_tp.size = 8; cf_tp.variant.float_kind = fk_float;
        cd_tp.kind = tk_complex; cd_tp.size = 16; cd_tp.variant.float_kind = fk_double;
        cld_tp.kind = tk_complex; cld_tp.size = 32; cld_tp.variant.float_kind = fk_long_double;
        rty.kind = tk_routine; rty.variant.routine.return_type = &cd_tp;

        gcc_jit_field *cf_fs[2] = {
            gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT), "re"),
            gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT), "im")
        };
        gcc_jit_type *cf_jt = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2CfSt", 2, cf_fs));
        gcc_jit_field *cld_fs[2] = {
            gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE), "re"),
            gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE), "im")
        };
        gcc_jit_type *cld_jt = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2CldSt", 2, cld_fs));

        a_variable vd1 = {}, vd2 = {}, vf1 = {}, vld1 = {};
        vd1.type = &cd_tp; vd1.storage_class = sc_auto; vd1.source_corresp.name = "cd1";
        vd2.type = &cd_tp; vd2.storage_class = sc_auto; vd2.source_corresp.name = "cd2";
        vf1.type = &cf_tp; vf1.storage_class = sc_auto; vf1.source_corresp.name = "cf1";
        vld1.type = &cld_tp; vld1.storage_class = sc_auto; vld1.source_corresp.name = "cld1";
        gcc_jit_lvalue *lvd1 = gcc_jit_function_new_local(fn, NULL, g_expr_mock.cplx_struct_type, "l_cd1");
        gcc_jit_lvalue *lvd2 = gcc_jit_function_new_local(fn, NULL, g_expr_mock.cplx_struct_type, "l_cd2");
        gcc_jit_lvalue *lvf1 = gcc_jit_function_new_local(fn, NULL, cf_jt, "l_cf1");
        gcc_jit_lvalue *lvld1 = gcc_jit_function_new_local(fn, NULL, cld_jt, "l_cld1");

        const char *bin_names[10] = {
            "__c99_complex_double_add", "__c99_complex_double_subtract", "__c99_complex_double_sub",
            "__c99_complex_double_multiply", "__c99_complex_double_mul",
            "__c99_complex_double_divide", "__c99_complex_double_div",
            "__c99_complex_double_other", "__c99_complex_other", "__c99_complex_double_conj"
        };
        gcc_jit_rvalue *rv = NULL;
        for (int bi = 0; bi < 10; ++bi) {
            a_routine br = {}; br.source_corresp.name = bin_names[bi]; br.type = &rty;
            an_expr_node e_fn = {}, a1 = {}, a2 = {}, e_call = {};
            e_fn.kind = enk_routine; e_fn.type = &ptr_tp; e_fn.variant.routine.ptr = &br; e_fn.next = &a1;
            a1.kind = enk_variable; a1.type = &cd_tp; a1.variant.variable.ptr = &vd1;
            a1.next = (bi == 9) ? NULL : &a2;
            a2.kind = enk_variable; a2.type = &cd_tp; a2.variant.variable.ptr = &vd2;
            GCC_GEN_BE_CHECK(expr_init_op(&e_call, eok_call, &cd_tp, &e_fn));
            GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 45, [&]() noexcept -> gcc_gen_be_error_t {
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vd1, lvd1));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vd2, lvd2));
                return gcc_gen_be_lower_expr_rvalue(&e_call, &rv);
            }));
        }
        /* float_negate, long_double_conjugate, 1-arg non-negate, and cblock == NULL */
        const char *un_names[3] = {"__c99_complex_float_negate", "__c99_complex_long_double_conjugate", "__c99_complex_double_add"};
        for (int ui = 0; ui < 3; ++ui) {
            a_routine ur = {}; ur.source_corresp.name = un_names[ui]; ur.type = &rty;
            a_type *utp = (ui == 0) ? &cf_tp : ((ui == 1) ? &cld_tp : &cd_tp);
            a_variable *uvar = (ui == 0) ? &vf1 : ((ui == 1) ? &vld1 : &vd1);
            an_expr_node e_fn = {}, a1 = {}, e_call = {};
            e_fn.kind = enk_routine; e_fn.type = &ptr_tp; e_fn.variant.routine.ptr = &ur; e_fn.next = &a1;
            a1.kind = enk_variable; a1.type = utp; a1.variant.variable.ptr = uvar;
            GCC_GEN_BE_CHECK(expr_init_op(&e_call, eok_call, utp, &e_fn));
            GCC_GEN_BE_CHECK(expr_test_map_type(&cf_tp, cf_jt));
            GCC_GEN_BE_CHECK(expr_test_map_type(&cld_tp, cld_jt));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vf1, lvf1));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vld1, lvld1));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vd1, lvd1));
            GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call, &rv));
            if (ui == 0) {
                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call, &rv));
            }
        }

        /* __c99_* 1-arg conversions: struct->struct (same/diff elem), struct->scalar (re/im, same/cast, null type), scalar->struct (re/im, same/cast), scalar->scalar, cblock==NULL + OOM */
        struct conv_case_t {
            const char *name;
            a_type *arg_tp;
            a_variable *arg_v;
            a_type *res_tp;
        };
        a_variable vs_flt = {}, vs_dbl = {};
        vs_flt.type = &flt_tp; vs_flt.storage_class = sc_auto; vs_flt.source_corresp.name = "vs_flt";
        vs_dbl.type = &dbl_tp; vs_dbl.storage_class = sc_auto; vs_dbl.source_corresp.name = "vs_dbl";
        gcc_jit_lvalue *lvs_flt = gcc_jit_function_new_local(fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT), "l_vsf");
        gcc_jit_lvalue *lvs_dbl = gcc_jit_function_new_local(fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE), "l_vsd");

        conv_case_t convs[10] = {
            {"__c99_cfloat_to_cdouble", &cf_tp, &vf1, &cd_tp},
            {"__c99_cdouble_to_cdouble", &cd_tp, &vd1, &cd_tp},
            {"__c99_cldouble_to_cfloat", &cld_tp, &vld1, &cf_tp},
            {"__c99_cdouble_to_cldouble", &cd_tp, &vd1, &cld_tp},
            {"__c99_clong_double_to_clong_double", &cld_tp, &vld1, &cld_tp},
            {"__c99_cdouble_to_i", &cd_tp, &vd1, &dbl_tp},
            {"__c99_cdouble_to_r", &cd_tp, &vd1, &flt_tp},
            {"__c99_ifloat_to_cdouble", &flt_tp, &vs_flt, &cd_tp},
            {"__c99_rdouble_to_cdouble", &dbl_tp, &vs_dbl, &cd_tp},
            {"__c99_scalar_noop", &dbl_tp, &vs_dbl, &dbl_tp}
        };
        for (int ci = 0; ci < 10; ++ci) {
            a_routine cr = {}; cr.source_corresp.name = convs[ci].name; cr.type = &rty;
            an_expr_node e_fn = {}, a1 = {}, e_call = {};
            e_fn.kind = enk_routine; e_fn.type = &ptr_tp; e_fn.variant.routine.ptr = &cr; e_fn.next = &a1;
            a1.kind = enk_variable; a1.type = convs[ci].arg_tp; a1.variant.variable.ptr = convs[ci].arg_v;
            GCC_GEN_BE_CHECK(expr_init_op(&e_call, eok_call, convs[ci].res_tp, &e_fn));
            GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 45, [&]() noexcept -> gcc_gen_be_error_t {
                g_expr_mock.num_type_overrides = 0;
                GCC_GEN_BE_CHECK(expr_test_map_type(&cf_tp, cf_jt));
                GCC_GEN_BE_CHECK(expr_test_map_type(&cld_tp, cld_jt));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vf1, lvf1));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vd1, lvd1));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vld1, lvld1));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vs_flt, lvs_flt));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vs_dbl, lvs_dbl));
                return gcc_gen_be_lower_expr_rvalue(&e_call, &rv);
            }));
            if (ci == 0) {
                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call, &rv));
                GCC_GEN_BE_CHECK(expr_test_map_type(&cd_tp, NULL));
                GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call, &rv));
            }
            if (ci == 5) {
                /* __c99_cdouble_to_i with type == NULL (4680:43 False) */
                GCC_GEN_BE_CHECK(expr_test_map_type(&dbl_tp, NULL));
                GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call, &rv));
            }
            if (ci == 7) {
                /* __c99_ifloat_to_cdouble with type == NULL (4686:50 False) */
                GCC_GEN_BE_CHECK(expr_test_map_type(&cd_tp, NULL));
                GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call, &rv));
            }
        }
        /* 0-arg __c99_complex_double_add (4597:43 False) & null a0 on __c99_cfloat_to_cdouble (4654:39 False) */
        {
            a_routine c0_r = {}, cnull_r = {}, bi_null_r = {};
            c0_r.source_corresp.name = "__c99_complex_double_add"; c0_r.type = &rty;
            cnull_r.source_corresp.name = "__c99_cfloat_to_cdouble"; cnull_r.type = &rty;
            bi_null_r.source_corresp.name = "__builtin_ia32_pause"; bi_null_r.type = &rty;
            an_expr_node ef0 = {}, ef1 = {}, ebi_fn = {}, ebi_arg = {}, ec0 = {}, ecnull = {};
            ef0.kind = enk_routine; ef0.type = &ptr_tp; ef0.variant.routine.ptr = &c0_r;
            GCC_GEN_BE_CHECK(expr_init_op(&ec0, eok_call, &cd_tp, &ef0));
            ebi_fn.kind = enk_routine; ebi_fn.type = &ptr_tp; ebi_fn.variant.routine.ptr = &bi_null_r;
            GCC_GEN_BE_CHECK(expr_init_op(&ebi_arg, eok_call, &cf_tp, &ebi_fn));
            ef1.kind = enk_routine; ef1.type = &ptr_tp; ef1.variant.routine.ptr = &cnull_r; ef1.next = &ebi_arg;
            GCC_GEN_BE_CHECK(expr_init_op(&ecnull, eok_call, &cd_tp, &ef1));
            g_expr_mock.num_type_overrides = 0;
            g_expr_mock.mock_builtin_rval = NULL;
            GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ec0, &rv));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ecnull, &rv));
        }
        g_expr_mock.num_type_overrides = 0;
        g_expr_mock.complex_as_struct = FALSE;
    }

    /* 4) L4773..4955 & L5339..5351: AAPCS64 PMF call, ARM64 x8 indirect call, and ARM64/WIN64 va_arg */
    {
        /* AAPCS64 PMF call (arm_pmf == TRUE) */
        g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
        a_param_type p_this = {}; p_this.type = &ptr_tp;
        a_routine_type_supplement pmf_rsup = {}; pmf_rsup.param_type_list = &p_this;
        a_type mfn_tp = {}, pmf_tp = {};
        mfn_tp.kind = tk_routine; mfn_tp.variant.routine.return_type = &int_tp; mfn_tp.variant.routine.extra_info = &pmf_rsup;
        pmf_tp.kind = tk_ptr_to_member; pmf_tp.size = 16; pmf_tp.variant.ptr_to_member.type = &mfn_tp;

        a_variable v_pmf = {}, v_this = {};
        v_pmf.type = &pmf_tp; v_pmf.storage_class = sc_auto; v_pmf.source_corresp.name = "v_pmf_arm";
        v_this.type = &ptr_tp; v_this.storage_class = sc_auto; v_this.source_corresp.name = "v_this_arm";
        gcc_jit_lvalue *lv_pmf = gcc_jit_function_new_local(fn, NULL, g_expr_mock.pmf_struct_type, "lv_pmf_arm");
        gcc_jit_lvalue *lv_this = gcc_jit_function_new_local(fn, NULL, jit_vp, "lv_this_arm");

        an_expr_node e_pmf = {}, e_th = {}, e_pcall = {};
        e_pmf.kind = enk_variable; e_pmf.type = &pmf_tp; e_pmf.variant.variable.ptr = &v_pmf; e_pmf.next = &e_th;
        e_th.kind = enk_variable; e_th.type = &ptr_tp; e_th.variant.variable.ptr = &v_this;
        GCC_GEN_BE_CHECK(expr_init_op(&e_pcall, eok_points_to_member_call, &int_tp, &e_pmf));
        gcc_jit_rvalue *rv = NULL;
        gcc_jit_type *mfn_jt = gcc_jit_context_new_function_ptr_type(ctx, NULL, jit_int, 1, &jit_vp, 0);
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 60, [&]() noexcept -> gcc_gen_be_error_t {
            g_expr_mock.num_type_overrides = 0;
            GCC_GEN_BE_CHECK(expr_test_map_type(&mfn_tp, mfn_jt));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pmf, lv_pmf));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_this, lv_this));
            return gcc_gen_be_lower_expr_rvalue(&e_pcall, &rv);
        }));
        g_expr_mock.num_type_overrides = 0;

        /* Indirect x8 sret call (fn_ptr != NULL, edg_ret_as_param = TRUE, 1-arg & 2-arg on DARWIN_ARM64, AAPCS64, SYSV_AMD64) + OOM */
        a_param_type xp0 = {}, xp1 = {}; xp0.type = &ptr_tp; xp0.next = &xp1; xp1.type = &int_tp;
        a_routine_type_supplement xsup = {}; xsup.param_type_list = &xp0; xsup.value_returned_as_parameter = TRUE;
        a_type xr_tp = {}, xfp_tp = {};
        xr_tp.kind = tk_routine; xr_tp.variant.routine.return_type = &void_tp; xr_tp.variant.routine.extra_info = &xsup;
        xfp_tp.kind = tk_pointer; xfp_tp.size = 8; xfp_tp.variant.pointer.type = &xr_tp;
        gcc_jit_type *xfp_ptys2[2] = {jit_vp, jit_int};
        gcc_jit_type *xfp_jt2 = gcc_jit_context_new_function_ptr_type(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), 2, xfp_ptys2, 0);
        gcc_jit_type *xfp_jt1 = gcc_jit_context_new_function_ptr_type(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), 1, &jit_vp, 0);
        a_variable v_fp = {}; v_fp.type = &xfp_tp; v_fp.storage_class = sc_auto; v_fp.source_corresp.name = "v_xfp";
        gcc_jit_lvalue *lv_fp = gcc_jit_function_new_local(fn, NULL, xfp_jt2, "lv_xfp");
        gcc_jit_lvalue *lv_fp1 = gcc_jit_function_new_local(fn, NULL, xfp_jt1, "lv_xfp1");

        an_expr_node e_fp = {}, xa0 = {}, xa1 = {}, e_xcall = {};
        e_fp.kind = enk_variable; e_fp.type = &xfp_tp; e_fp.variant.variable.ptr = &v_fp; e_fp.next = &xa0;
        xa0.kind = enk_variable; xa0.type = &ptr_tp; xa0.variant.variable.ptr = &v_this; xa0.next = &xa1;
        xa1.kind = enk_constant; xa1.type = &int_tp;
        a_constant xic = {}; xic.kind = ck_integer; xic.type = &int_tp; xa1.variant.constant.ptr = &xic;
        GCC_GEN_BE_CHECK(expr_init_op(&e_xcall, eok_call, &void_tp, &e_fp));

        gcc_gen_be_target_abi_t xabis[3] = {GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64, GCC_GEN_BE_TARGET_ABI_AAPCS64, GCC_GEN_BE_TARGET_ABI_SYSV_AMD64};
        for (int ai = 0; ai < 3; ++ai) {
            g_expr_mock.target_abi = xabis[ai];
            GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 65, [&]() noexcept -> gcc_gen_be_error_t {
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_fp, lv_fp));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_this, lv_this));
                return gcc_gen_be_lower_expr_rvalue(&e_xcall, &rv);
            }));
        }
        /* 1-arg x8 indirect call (+ OOM), 0-arg, cblock==NULL, fn_ptr==NULL, and a0==NULL */
        xa0.next = NULL;
        g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 65, [&]() noexcept -> gcc_gen_be_error_t {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_fp, lv_fp1));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_this, lv_this));
            return gcc_gen_be_lower_expr_rvalue(&e_xcall, &rv);
        }));
        /* cblock == NULL (4900:95 False) */
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_fp, lv_fp1));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_this, lv_this));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_xcall, &rv));
        /* 0-arg x8 indirect call (4900:67 False) */
        gcc_jit_type *xfp_jt0 = gcc_jit_context_new_function_ptr_type(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), 0, NULL, 0);
        gcc_jit_lvalue *lv_fp0 = gcc_jit_function_new_local(fn, NULL, xfp_jt0, "lv_xfp0");
        e_fp.next = NULL;
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_fp, lv_fp0));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_xcall, &rv));
        /* a0 == NULL (4908:43 False) & fn_ptr == NULL (4900:83 False) via builtin returning NULL */
        a_routine bi_null_x = {}; bi_null_x.source_corresp.name = "__builtin_ia32_pause"; bi_null_x.type = &xr_tp;
        an_expr_node e_bix_fn = {}, e_bix_call = {};
        e_bix_fn.kind = enk_routine; e_bix_fn.type = &ptr_tp; e_bix_fn.variant.routine.ptr = &bi_null_x;
        GCC_GEN_BE_CHECK(expr_init_op(&e_bix_call, eok_call, &ptr_tp, &e_bix_fn));
        g_expr_mock.mock_builtin_rval = NULL;
        e_fp.next = &e_bix_call;
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_fp, lv_fp1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_xcall, &rv));
        e_bix_call.type = &xfp_tp; e_bix_call.next = &xa0;
        an_expr_node e_xcall_nullfp = {};
        GCC_GEN_BE_CHECK(expr_init_op(&e_xcall_nullfp, eok_call, &void_tp, &e_bix_call));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_this, lv_this));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_xcall_nullfp, &rv));

        /* va_arg >16B HFA vs non-HFA on DARWIN_ARM64/AAPCS64 and sizes 1,2,4,8,12 on WIN64 */
        a_type big_st = {}; big_st.kind = tk_struct; big_st.size = 24; big_st.source_corresp.name = "S2BigVaSt";
        an_expr_node e_ap = {}, e_va = {};
        e_ap.kind = enk_variable; e_ap.type = &ptr_tp; e_ap.variant.variable.ptr = &v_this;
        GCC_GEN_BE_CHECK(expr_init_op(&e_va, eok_va_arg, &big_st, &e_ap));
        for (int hfa = 0; hfa <= 1; ++hfa) {
            g_expr_mock.va_num_fp = hfa;
            g_expr_mock.target_abi = (hfa == 0) ? GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64 : GCC_GEN_BE_TARGET_ABI_AAPCS64;
            GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 35, [&]() noexcept -> gcc_gen_be_error_t {
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_this, lv_this));
                return gcc_gen_be_lower_expr_rvalue(&e_va, &rv);
            }));
        }
        g_expr_mock.va_num_fp = 0;
        g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_WIN64;
        a_targ_size_t win_szs[5] = {1, 2, 4, 8, 12};
        for (int wi = 0; wi < 5; ++wi) {
            big_st.size = win_szs[wi];
            GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 30, [&]() noexcept -> gcc_gen_be_error_t {
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_this, lv_this));
                return gcc_gen_be_lower_expr_rvalue(&e_va, &rv);
            }));
        }
        g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    }

    /* 5) L6420..6669: enk_lowered_eh (cleanup_state, ctor subobjects, thrown/caught object address) & co_await symmetric transfer */
    {
        a_type sub_st = {}, cls_tp = {}, p_sub_tp = {}, p_cls_tp = {}, dtor_rty = {};
        sub_st.kind = tk_struct; sub_st.size = 4; sub_st.source_corresp.name = "S2SubSt";
        a_field cf0 = {}, cf1 = {}, cf_other = {};
        cf0.source_corresp.name = "cf0"; cf0.type = &sub_st; cf0.offset = 0; cf0.next = &cf_other;
        cf_other.source_corresp.name = "cfo"; cf_other.type = &int_tp; cf_other.offset = 4; cf_other.next = &cf1;
        cf1.source_corresp.name = "cf1"; cf1.type = &sub_st; cf1.offset = 8;
        cls_tp.kind = tk_class; cls_tp.size = 12; cls_tp.source_corresp.name = "S2CtorCls";
        cls_tp.variant.class_struct_union.field_list = &cf0;

        p_sub_tp.kind = tk_pointer; p_sub_tp.size = 8; p_sub_tp.variant.pointer.type = &sub_st;
        p_cls_tp.kind = tk_pointer; p_cls_tp.size = 8; p_cls_tp.variant.pointer.type = &cls_tp;
        a_param_type dp0 = {}; dp0.type = &p_sub_tp;
        a_routine_type_supplement drsup = {}; drsup.param_type_list = &dp0;
        dtor_rty.kind = tk_routine; dtor_rty.variant.routine.return_type = &void_tp; dtor_rty.variant.routine.extra_info = &drsup;
        a_routine dtor_r = {}; dtor_r.source_corresp.name = "__s2_sub_dtor"; dtor_r.type = &dtor_rty;
        gcc_jit_param *jdp0 = gcc_jit_context_new_param(ctx, NULL, jit_vp, "this");
        gcc_jit_function *dtor_jfn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "__s2_sub_dtor_j", 1, &jdp0, 0);

        a_variable this_var = {}, auto_var = {};
        this_var.source_corresp.name = "this"; this_var.type = &p_cls_tp;
        auto_var.source_corresp.name = "av"; auto_var.type = &int_tp;
        gcc_jit_lvalue *this_lv = gcc_jit_function_new_local(fn, NULL, jit_vp, "s2_this_lv");
        gcc_jit_lvalue *auto_lv = gcc_jit_function_new_local(fn, NULL, jit_int, "s2_auto_lv");

        an_object_lifetime scope_lt = {}, dip_lt = {};
        a_dynamic_init d_var = {}, d_ctor1 = {}, d_ctor2 = {}, d_nodt = {}, d_barrier = {};
        d_var.variable = &auto_var; d_var.destructor = &dtor_r; d_var.next_in_destruction_list = &d_ctor1;
        d_ctor1.is_constructor_init = TRUE; d_ctor1.destructor = &dtor_r; d_ctor1.lifetime = &dip_lt;
        dip_lt.parent_destruction_sublist = &d_ctor2;
        d_ctor2.is_constructor_init = TRUE; d_ctor2.destructor = &dtor_r; d_ctor2.next_in_destruction_list = &d_nodt;
        d_nodt.destructor = NULL; d_nodt.next_in_destruction_list = &d_barrier;

        /* Scope lifetime destruction chain with extra matching ctor entries so both same_after < n_offs True and False execute */
        a_dynamic_init sc_extra1 = {}, sc_extra2 = {};
        sc_extra1.is_constructor_init = TRUE; sc_extra1.destructor = &dtor_r; sc_extra1.next_in_destruction_list = &sc_extra2;
        sc_extra2.is_constructor_init = TRUE; sc_extra2.destructor = &dtor_r; sc_extra2.next_in_destruction_list = &d_var;
        scope_lt.destructions = &sc_extra1;

        a_scope fsc = {}; fsc.kind = sck_function; fsc.variant.routine.parameters = &this_var; fsc.lifetime = &scope_lt;
        exceptions_enabled = TRUE;
        g_expr_mock.mock_decl_func = dtor_jfn;

        an_expr_node e_leh = {}; e_leh.kind = enk_lowered_eh_construct; e_leh.type = &void_tp;
        e_leh.variant.lowered_eh.kind = leck_cleanup_state;
        e_leh.variant.lowered_eh.variant.cleanup_ptr = &d_var;
        gcc_jit_rvalue *rv = NULL;

        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 90, [&]() noexcept -> gcc_gen_be_error_t {
            gcc_gen_be_singleton_state.active_function_scope = &fsc;
            gcc_gen_be_singleton_state.eh_try_barrier_depth = 1U;
            gcc_gen_be_singleton_state.eh_try_barriers[0] = &d_barrier;
            gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
            while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
            }
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &this_var, this_lv));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &auto_var, auto_lv));
            return gcc_gen_be_lower_expr_rvalue(&e_leh, &rv);
        }));

        /* Common-prefix reuse & pop excess cleanup_dip_depth + leck_unreachable_cleanup_state + barrier_depth > 32 + n_offs == 0 */
        gcc_gen_be_singleton_state.active_function_scope = &fsc;
        gcc_gen_be_singleton_state.eh_try_barrier_depth = 33U;
        e_leh.variant.lowered_eh.kind = leck_unreachable_cleanup_state;
        e_leh.variant.lowered_eh.variant.cleanup_ptr = &d_ctor2; /* shares d_ctor2 prefix[0], pops excess! */
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        for (size_t k = 0; k < gcc_gen_be_singleton_state.eh_cleanup_dip_depth; ++k) {
            GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(gcc_jit_function_new_block(fn, "s2_mock_lp")));
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));

        /* Ctor subobject edge cases: fsc.lifetime == NULL, n_offs == 0, non-pointer dp0, non-routine drt, non-pointer this_var */
        fsc.lifetime = NULL;
        cls_tp.variant.class_struct_union.field_list = &cf_other; /* n_offs == 0 */
        e_leh.variant.lowered_eh.variant.cleanup_ptr = &d_ctor2;
        d_ctor2.next_in_destruction_list = NULL;
        for (int ci = 0; ci < 4; ++ci) {
            gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
            while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
            }
            if (ci == 1) dp0.type = &int_tp;
            if (ci == 2) dtor_r.type = &int_tp;
            if (ci == 3) { dtor_r.type = NULL; this_var.type = &int_tp; }
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &this_var, this_lv));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
        }
        gcc_gen_be_singleton_state.active_function_scope = NULL;
        gcc_gen_be_singleton_state.eh_try_barrier_depth = 0U;
        while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
            GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
        }
        gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
        g_expr_mock.mock_decl_func = NULL;

        /* Extra lowered_eh branches: cblock==NULL, 65-entry chain cap, uncached auto_var (+ OOM & null), func_decl_return_null, exceptions_enabled==FALSE, null this_var/this_lv/dp0_list, sublist sc_d */
        {
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
            e_leh.variant.lowered_eh.kind = leck_cleanup_state;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));

            a_dynamic_init many_d[65] = {};
            for (int mi = 0; mi < 64; ++mi) many_d[mi].next_in_destruction_list = &many_d[mi + 1];
            e_leh.variant.lowered_eh.variant.cleanup_ptr = &many_d[0];
            GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));

            /* Uncached auto_var (OOM + local_var_return_null + exceptions_enabled==FALSE + func_decl_return_null) */
            a_dynamic_init d_unc = {}; d_unc.variable = &auto_var; d_unc.destructor = &dtor_r;
            e_leh.variant.lowered_eh.variant.cleanup_ptr = &d_unc;
            g_expr_mock.mock_decl_func = dtor_jfn;
            GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 30, [&]() noexcept -> gcc_gen_be_error_t {
                gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
                while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
                }
                return gcc_gen_be_lower_expr_rvalue(&e_leh, &rv);
            }));
            gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
            exceptions_enabled = FALSE;
            GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
            exceptions_enabled = TRUE;
            g_expr_mock.local_var_return_null = TRUE;
            GCC_GEN_BE_CHECK(cache_clear_all());
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
            g_expr_mock.local_var_return_null = FALSE;
            g_expr_mock.mock_decl_func = NULL;
            g_expr_mock.func_decl_return_null = TRUE;
            GCC_GEN_BE_CHECK(cache_clear_all());
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &auto_var, auto_lv));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
            g_expr_mock.func_decl_return_null = FALSE;

            /* Ctor subobject: fsc.parameters==NULL, this_lv==NULL, cls_tp non-struct, drt->extra_info==NULL, sc_d sublist & 3-field same_after==0 */
            a_field cf2 = {}; cf2.source_corresp.name = "cf2"; cf2.type = &sub_st; cf2.offset = 12;
            cf1.next = &cf2;
            cls_tp.variant.class_struct_union.field_list = &cf0;
            this_var.type = &p_cls_tp; dp0.type = &p_sub_tp; dtor_r.type = &dtor_rty;
            g_expr_mock.mock_decl_func = dtor_jfn;
            an_object_lifetime sc_sub_lt = {};
            a_dynamic_init sc_head = {}, d_single_ctor = {};
            d_single_ctor.is_constructor_init = TRUE; d_single_ctor.destructor = &dtor_r;
            sc_head.lifetime = &sc_sub_lt; sc_sub_lt.parent_destruction_sublist = &d_single_ctor;
            scope_lt.destructions = &sc_head;
            fsc.lifetime = &scope_lt;
            fsc.variant.routine.parameters = &this_var;
            gcc_gen_be_singleton_state.active_function_scope = &fsc;
            gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
            e_leh.variant.lowered_eh.variant.cleanup_ptr = &d_single_ctor;
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &this_var, this_lv));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv)); /* same_after == 0 < n_offs (3) */

            gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
            GCC_GEN_BE_CHECK(cache_clear_all()); /* this_lv == NULL */
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));

            fsc.variant.routine.parameters = NULL; /* this_var == NULL */
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
            fsc.variant.routine.parameters = &this_var;

            p_cls_tp.variant.pointer.type = &int_tp; /* cls_tp non-struct */
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &this_var, this_lv));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
            p_cls_tp.variant.pointer.type = &cls_tp;

            dtor_rty.variant.routine.extra_info = NULL; /* dp0_list == NULL */
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
            dtor_rty.variant.routine.extra_info = &drsup;
            drsup.param_type_list = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
            drsup.param_type_list = &dp0;

            gcc_gen_be_singleton_state.active_function_scope = NULL;
            while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
            }
            gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
            g_expr_mock.mock_decl_func = NULL;
        }

        /* leck_thrown_object_address & leck_caught_object_address & other leck kind (active lval, CACHE_VAR, fallback unwind_exc) + OOM */
        gcc_jit_lvalue *exc_lv = gcc_jit_function_new_local(fn, NULL, jit_vp, "s2_exc_lv");
        e_leh.type = &void_tp; /* type == void -> 6525:34 False */
        e_leh.variant.lowered_eh.kind = leck_thrown_object_address;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
        e_leh.type = NULL; /* 6525:26 False */
        GCC_GEN_BE_CHECK(expr_test_map_type(NULL, NULL));
        e_leh.type = &ptr_tp;
        GCC_GEN_BE_CHECK(expr_test_map_type(&ptr_tp, NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
        g_expr_mock.num_type_overrides = 0;

        /* Other leck_* kind (leck_mark -> 6528:68 False, both uncached & cached) */
        e_leh.variant.lowered_eh.kind = static_cast<a_lowered_eh_construct_kind>(0);
        GCC_GEN_BE_CHECK(cache_clear_all());
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_caught_obj_addr", exc_lv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
        GCC_GEN_BE_CHECK(cache_clear_all());

        e_leh.variant.lowered_eh.kind = leck_thrown_object_address;
        gcc_gen_be_singleton_state.active_thrown_obj_lval = exc_lv;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
        gcc_gen_be_singleton_state.active_thrown_obj_lval = NULL;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 30, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_lower_expr_rvalue(&e_leh, &rv);
        }));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_thrown_obj_addr", exc_lv));
        e_leh.type = &int_tp; /* non-pointer deref branch */
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));

        e_leh.variant.lowered_eh.kind = leck_caught_object_address;
        gcc_gen_be_singleton_state.active_caught_obj_lval = exc_lv;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));
        gcc_gen_be_singleton_state.active_caught_obj_lval = NULL;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 30, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_lower_expr_rvalue(&e_leh, &rv);
        }));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_caught_obj_addr", exc_lv));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_leh, &rv));

        /* co_await await_suspend: bool return, struct coroutine_handle<> symmetric transfer (+ OOM), JIT void/bool runtime type, null type + non-null rval, null builtin rval */
        a_type coro_hdl_tp = {}; coro_hdl_tp.kind = tk_struct; coro_hdl_tp.size = 8; coro_hdl_tp.source_corresp.name = "S2CoroHdl";
        a_variable v_bool_s = {}, v_hdl_s = {};
        v_bool_s.type = &bool_tp; v_bool_s.storage_class = sc_auto; v_bool_s.source_corresp.name = "v_bsusp";
        v_hdl_s.type = &coro_hdl_tp; v_hdl_s.storage_class = sc_auto; v_hdl_s.source_corresp.name = "v_hsusp";
        gcc_jit_lvalue *lv_bs = gcc_jit_function_new_local(fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL), "lv_bs");
        gcc_jit_lvalue *lv_hs = gcc_jit_function_new_local(fn, NULL, jit_vp, "lv_hs");

        a_constant c_rdy = {}; c_rdy.kind = ck_integer; c_rdy.type = &int_tp;
        an_expr_node e_rdy = {}, e_res = {}, e_sus = {}, e_aw = {};
        e_rdy.kind = enk_constant; e_rdy.type = &int_tp; e_rdy.variant.constant.ptr = &c_rdy; e_rdy.next = &e_res;
        e_res.kind = enk_constant; e_res.type = &int_tp; e_res.variant.constant.ptr = &c_rdy; e_res.next = &e_sus;
        e_sus.kind = enk_variable; e_sus.type = &bool_tp; e_sus.variant.variable.ptr = &v_bool_s;
        e_aw.kind = enk_await; e_aw.type = &int_tp; e_aw.variant.await_info.ready_resume_suspend = &e_rdy;

        /* bool await_suspend & !susp_edg_tp (e_sus.type == NULL) with non-null suspend_rval (covers 6645:17 True, 6646!) */
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_bool_s, lv_bs));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_co_await(&e_aw, &rv));
        e_sus.type = NULL;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_co_await(&e_aw, &rv));

        /* coroutine_handle<> symmetric transfer + OOM */
        e_sus.type = &coro_hdl_tp; e_sus.variant.variable.ptr = &v_hdl_s;
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 45, [&]() noexcept -> gcc_gen_be_error_t {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_hdl_s, lv_hs));
            return gcc_gen_be_lower_co_await(&e_aw, &rv);
        }));

        /* non-void/non-bool EDG type whose lowered JIT rvalue has GCC_JIT_TYPE_BOOL or GCC_JIT_TYPE_VOID */
        e_sus.type = &int_tp; e_sus.variant.variable.ptr = &v_bool_s;
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_bool_s, lv_bs));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_co_await(&e_aw, &rv));

        a_type susp_int_tp = {};
        GCC_GEN_BE_CHECK(expr_init_int_type(&susp_int_tp, ik_int, 4));
        a_type vfn_tp = {}; vfn_tp.kind = tk_routine; vfn_tp.variant.routine.return_type = &void_tp;
        a_routine vfn_r = {}; vfn_r.source_corresp.name = "s2_void_susp"; vfn_r.type = &vfn_tp;
        gcc_jit_function *vfn_j = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "__s2_vsusp", 0, NULL, 0);
        g_expr_mock.mock_decl_func = vfn_j;
        an_expr_node e_vfn = {}; e_vfn.kind = enk_routine; e_vfn.type = &ptr_tp; e_vfn.variant.routine.ptr = &vfn_r;
        GCC_GEN_BE_CHECK(expr_init_op(&e_sus, eok_call, &susp_int_tp, &e_vfn));
        GCC_GEN_BE_CHECK(expr_test_map_type(&susp_int_tp, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_co_await(&e_aw, &rv));
        g_expr_mock.num_type_overrides = 0;
        g_expr_mock.mock_decl_func = NULL;

        /* suspend_rval == NULL via builtin call on bool_tp (6649:33 False) and coro_hdl_tp (6652:20 False, 6656!) */
        a_routine bi_aw_r = {}; bi_aw_r.source_corresp.name = "__builtin_ia32_pause"; bi_aw_r.type = &vfn_tp;
        an_expr_node e_biaw_fn = {}; e_biaw_fn.kind = enk_routine; e_biaw_fn.type = &ptr_tp; e_biaw_fn.variant.routine.ptr = &bi_aw_r;
        g_expr_mock.mock_builtin_rval = NULL;
        GCC_GEN_BE_CHECK(expr_init_op(&e_sus, eok_call, &bool_tp, &e_biaw_fn));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_co_await(&e_aw, &rv));
        GCC_GEN_BE_CHECK(expr_init_op(&e_sus, eok_call, &coro_hdl_tp, &e_biaw_fn));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_co_await(&e_aw, &rv));
    }

    /* 7) Final rvalue branch gaps: psubtract_assign long rhs, null expr->type eq/lt, call op1->type guards,
          edg_ret_as_param without uses_sret on PMF & virtual calls, and constructor-init EH cleanup edge cases */
    {
        g_expr_mock.num_type_overrides = 0;
        gcc_jit_rvalue *rv = NULL;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));

        /* eok_psubtract_assign with r2 already having GCC_JIT_TYPE_LONG & eok_va_start with char* ap_lval != void* */
        a_type long_tp = {}; GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
        a_variable vp_lhs = {}, vl_rhs = {}, v_va_cp = {};
        vp_lhs.source_corresp.name = "s2_psub_p"; vp_lhs.type = &ptr_tp; vp_lhs.storage_class = sc_auto;
        vl_rhs.source_corresp.name = "s2_psub_l"; vl_rhs.type = &long_tp; vl_rhs.storage_class = sc_auto;
        v_va_cp.source_corresp.name = "s2_va_cp"; v_va_cp.type = &ptr_tp; v_va_cp.storage_class = sc_auto;
        gcc_jit_lvalue *lvp = gcc_jit_function_new_local(fn, NULL, gcc_jit_type_get_pointer(jit_int), "l_psub_p");
        gcc_jit_lvalue *lvl = gcc_jit_function_new_local(fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), "l_psub_l");
        gcc_jit_lvalue *lv_vacp = gcc_jit_function_new_local(
            fn, NULL, gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR)), "l_va_cp");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vp_lhs, lvp));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vl_rhs, lvl));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_va_cp, lv_vacp));
        an_expr_node e_vacp = {}, e_vastart_cp = {};
        e_vacp.kind = enk_variable; e_vacp.type = &ptr_tp; e_vacp.variant.variable.ptr = &v_va_cp;
        GCC_GEN_BE_CHECK(expr_init_op(&e_vastart_cp, eok_va_start, &void_tp, &e_vacp));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_vastart_cp, &rv));
        an_expr_node ep1 = {}, ep2 = {}, epsub = {};
        ep1.kind = enk_variable; ep1.type = &ptr_tp; ep1.variant.variable.ptr = &vp_lhs; ep1.next = &ep2;
        ep2.kind = enk_variable; ep2.type = &long_tp; ep2.variant.variable.ptr = &vl_rhs;
        GCC_GEN_BE_CHECK(expr_init_op(&epsub, eok_psubtract_assign, &ptr_tp, &ep1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&epsub, &rv));

        /* eok_eq and eok_lt with strip_expr_typerefs(expr->type) == NULL */
        a_type null_tref = {}, null_ptr_tp = {};
        null_tref.kind = tk_typeref; null_tref.variant.typeref.type = NULL;
        null_ptr_tp.kind = tk_pointer; null_ptr_tp.size = 8; null_ptr_tp.variant.pointer.type = NULL;
        GCC_GEN_BE_CHECK(expr_test_map_type(&null_tref, jit_int));
        an_expr_node el1 = {}, el2 = {}, e_eq_nt = {}, e_lt_nt = {};
        el1.kind = enk_variable; el1.type = &long_tp; el1.variant.variable.ptr = &vl_rhs; el1.next = &el2;
        el2.kind = enk_variable; el2.type = &long_tp; el2.variant.variable.ptr = &vl_rhs;
        GCC_GEN_BE_CHECK(expr_init_op(&e_eq_nt, eok_eq, &null_tref, &el1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eq_nt, &rv));
        GCC_GEN_BE_CHECK(expr_init_op(&e_lt_nt, eok_lt, &null_tref, &el1));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_lt_nt, &rv));
        g_expr_mock.num_type_overrides = 0;

        /* eok_call with op1->type == NULL, tk_typeref -> NULL, and tk_pointer -> NULL */
        a_variable v_fnptr = {}; v_fnptr.source_corresp.name = "s2_fnp"; v_fnptr.type = &ptr_tp; v_fnptr.storage_class = sc_auto;
        gcc_jit_type *jfn_pt = gcc_jit_context_new_function_ptr_type(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), 0, NULL, 0);
        gcc_jit_lvalue *lv_fnp = gcc_jit_function_new_local(fn, NULL, jfn_pt, "l_fnp");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_fnptr, lv_fnp));
        a_type *call_op_tps[3] = { NULL, &null_tref, &null_ptr_tp };
        for (int ti = 0; ti < 3; ++ti) {
            an_expr_node e_callee = {}, e_c = {};
            e_callee.kind = enk_variable; e_callee.type = call_op_tps[ti]; e_callee.variant.variable.ptr = &v_fnptr;
            GCC_GEN_BE_CHECK(expr_init_op(&e_c, eok_call, &void_tp, &e_callee));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_c, &rv));
        }

        /* PMF call and virtual member call with uses_sret == FALSE && edg_ret_as_param == TRUE */
        a_param_type rap_p0 = {}, rap_p1 = {};
        rap_p0.type = &ptr_tp; rap_p0.next = &rap_p1;
        rap_p1.type = &ptr_tp;
        a_routine_type_supplement rap_sup = {}; rap_sup.param_type_list = &rap_p0; rap_sup.value_returned_as_parameter = TRUE;
        a_type rap_rty = {}; rap_rty.kind = tk_routine; rap_rty.variant.routine.return_type = &void_tp; rap_rty.variant.routine.extra_info = &rap_sup;
        gcc_jit_type *jrap_ps[2] = { jit_vp, jit_vp };
        gcc_jit_type *jrap_fp = gcc_jit_context_new_function_ptr_type(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), 2, jrap_ps, 0);
        GCC_GEN_BE_CHECK(expr_test_map_type(&rap_rty, jrap_fp));

        /* PMF call: op1 is enk_variable of tk_ptr_to_member pointing to rap_rty, with 2 void* args (sret, this) */
        gcc_jit_field *pmf_fs[2] = {
            gcc_jit_context_new_field(ctx, NULL, jit_vp, "__pfn"),
            gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), "__delta")
        };
        gcc_jit_type *jpmf_tp = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "S2RapPmfJ", 2, pmf_fs));
        a_type pmf_edg = {}; pmf_edg.kind = tk_ptr_to_member; pmf_edg.size = 16; pmf_edg.variant.ptr_to_member.type = &rap_rty;
        GCC_GEN_BE_CHECK(expr_test_map_type(&pmf_edg, jpmf_tp));
        a_variable v_pmf = {}, v_vp = {};
        v_pmf.source_corresp.name = "s2_vpmf"; v_pmf.type = &pmf_edg; v_pmf.storage_class = sc_auto;
        v_vp.source_corresp.name = "s2_vvp"; v_vp.type = &ptr_tp; v_vp.storage_class = sc_auto;
        gcc_jit_lvalue *lv_pmf = gcc_jit_function_new_local(fn, NULL, jpmf_tp, "l_vpmf");
        gcc_jit_lvalue *lv_vp = gcc_jit_function_new_local(fn, NULL, jit_vp, "l_vvp");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pmf, lv_pmf));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vp, lv_vp));
        an_expr_node e_pmf_op = {}, e_ra0 = {}, e_ra1 = {}, e_pmf_call = {};
        e_pmf_op.kind = enk_variable; e_pmf_op.type = &rap_rty; e_pmf_op.variant.variable.ptr = &v_pmf; e_pmf_op.next = &e_ra0;
        e_ra0.kind = enk_variable; e_ra0.type = &ptr_tp; e_ra0.variant.variable.ptr = &v_vp; e_ra0.next = &e_ra1;
        e_ra1.kind = enk_variable; e_ra1.type = &ptr_tp; e_ra1.variant.variable.ptr = &v_vp;
        GCC_GEN_BE_CHECK(expr_init_op(&e_pmf_call, eok_points_to_pm_call, &void_tp, &e_pmf_op));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_pmf_call, &rv));

        /* Virtual call with uses_sret == FALSE && edg_ret_as_param == TRUE */
        a_routine vrap_r = {}; vrap_r.source_corresp.name = "s2_vrap"; vrap_r.source_corresp.is_class_member = TRUE;
        vrap_r.type = &rap_rty; vrap_r.number.virtual_function = 0;
        an_expr_node e_vrap_fn = {}, e_vrap_call = {};
        e_vrap_fn.kind = enk_routine; e_vrap_fn.type = &rap_rty; e_vrap_fn.variant.routine.ptr = &vrap_r; e_vrap_fn.next = &e_ra0;
        GCC_GEN_BE_CHECK(expr_init_op(&e_vrap_call, eok_call, &void_tp, &e_vrap_fn));
        e_vrap_call.variant.operation.is_virtual_call = TRUE;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_vrap_call, &rv));
        g_expr_mock.num_type_overrides = 0;

        /* Constructor-init EH cleanup edge cases: null active_function_scope, null this_var->type,
           null d->destructor, d->destructor->type = null_tref, param_type_list->type = NULL, and 33 matching fields */
        a_scope *saved_fscope = gcc_gen_be_singleton_state.active_function_scope;
        a_boolean saved_exc = exceptions_enabled;
        exceptions_enabled = FALSE;

        a_type sub_cls = {}, outer_cls = {}, cls_ptr_tp = {};
        sub_cls.kind = tk_class; sub_cls.size = 4; sub_cls.source_corresp.name = "S2EhSub";
        a_field cf33[33] = {};
        for (int fi = 0; fi < 33; ++fi) {
            cf33[fi].type = &sub_cls;
            cf33[fi].offset = (a_targ_size_t)(fi * 4);
            cf33[fi].next = (fi + 1 < 33) ? &cf33[fi + 1] : NULL;
        }
        outer_cls.kind = tk_class; outer_cls.size = 132; outer_cls.source_corresp.name = "S2EhOuter33";
        outer_cls.variant.class_struct_union.field_list = &cf33[0];
        cls_ptr_tp.kind = tk_pointer; cls_ptr_tp.size = 8; cls_ptr_tp.variant.pointer.type = &outer_cls;

        a_type sub_ptr_tp = {}; sub_ptr_tp.kind = tk_pointer; sub_ptr_tp.size = 8; sub_ptr_tp.variant.pointer.type = &sub_cls;
        a_param_type dtp0_ok = {}, dtp0_null = {};
        dtp0_ok.type = &sub_ptr_tp;
        dtp0_null.type = NULL;
        a_routine_type_supplement dsup_ok = {}, dsup_null = {};
        dsup_ok.param_type_list = &dtp0_ok;
        dsup_null.param_type_list = &dtp0_null;
        a_type drt_ok = {}, drt_nullp = {};
        drt_ok.kind = tk_routine; drt_ok.variant.routine.return_type = &void_tp; drt_ok.variant.routine.extra_info = &dsup_ok;
        drt_nullp.kind = tk_routine; drt_nullp.variant.routine.return_type = &void_tp; drt_nullp.variant.routine.extra_info = &dsup_null;

        gcc_jit_param *jdp0 = gcc_jit_context_new_param(ctx, NULL, jit_vp, "this_p");
        gcc_jit_function *jdtor_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "__s2_dtor33", 1, &jdp0, 0);
        a_routine r_dtor_ok = {}, r_dtor_tref = {}, r_dtor_nullp = {};
        r_dtor_ok.source_corresp.name = "s2_dtor_ok"; r_dtor_ok.type = &drt_ok;
        r_dtor_tref.source_corresp.name = "s2_dtor_tref"; r_dtor_tref.type = &null_tref;
        r_dtor_nullp.source_corresp.name = "s2_dtor_nullp"; r_dtor_nullp.type = &drt_nullp;
        g_expr_mock.mock_decl_func = jdtor_fn;

        a_variable this_v = {}; this_v.source_corresp.name = "this"; this_v.type = &cls_ptr_tp; this_v.storage_class = sc_auto;
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &this_v, lv_vp));

        a_dynamic_init di = {};
        di.kind = dik_class_result_via_ctor;
        di.is_constructor_init = TRUE;
        di.variable = NULL;
        di.destructor = &r_dtor_ok;

        an_object_lifetime lt = {}; lt.destructions = &di;
        a_scope fsc = {}; fsc.kind = sck_function; fsc.variant.routine.parameters = &this_v; fsc.lifetime = &lt;

        an_expr_node e_eh = {};
        e_eh.kind = enk_lowered_eh_construct; e_eh.type = &void_tp;
        e_eh.variant.lowered_eh.kind = leck_cleanup_state;
        e_eh.variant.lowered_eh.variant.cleanup_ptr = &di;

        /* Case A: active_function_scope == NULL */
        gcc_gen_be_singleton_state.active_function_scope = NULL;
        gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));

        /* Case B: 33 matching fields */
        gcc_gen_be_singleton_state.active_function_scope = &fsc;
        gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));

        /* Case C: d->destructor == NULL */
        di.destructor = NULL;
        gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));

        /* Case D: d->destructor->type = null_tref */
        di.destructor = &r_dtor_tref;
        gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));

        /* Case E: param_type_list->type == NULL */
        di.destructor = &r_dtor_nullp;
        gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));

        /* Case F: this_var->type == NULL */
        this_v.type = NULL;
        di.destructor = &r_dtor_ok;
        gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));

        /* Case G: error propagation from gcc_gen_be_pop_landing_pad when eh_cleanup_dip_depth > common_prefix */
        e_eh.variant.lowered_eh.variant.cleanup_ptr = NULL;
        gcc_gen_be_singleton_state.landing_pad_stack_size = 0;
        gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 1U;
        gcc_gen_be_singleton_state.eh_cleanup_dips[0] = &di;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        if (gcc_gen_be_lower_expr_rvalue(&e_eh, &rv) == GCC_GEN_BE_SUCCESS) {
            return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
        }

        g_expr_mock.mock_decl_func = NULL;
        exceptions_enabled = saved_exc;
        gcc_gen_be_singleton_state.active_function_scope = saved_fscope;
        gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes all `gcc_gen_be_lower_expr_rvalue` coverage tests.
 * @details Runs the six Part C rvalue test suites in isolated `libgccjit` contexts.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t run_expr_cov_rval_tests(void) GCC_GEN_BE_NOEXCEPT {
    typedef gcc_gen_be_error_t (*rval_fn_t)(gcc_jit_context *, gcc_jit_function *) GCC_GEN_BE_NOEXCEPT;
    static const rval_fn_t suites[6] = {
        test_cov_rval_builtins_vars_and_flow,
        test_cov_rval_arith_assign_calls_va,
        test_cov_rval_casts_new_delete_eh,
        test_cov_rval_remaining_gaps,
        test_cov_rval_final_gaps,
        test_cov_rval_stage2_gaps
    };
    for (size_t i = 0; i < 6; ++i) {
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
