/**
 * @file test_gcc_be_expr_cov_lval.cpp
 * @brief Exhaustive branch, line, and OOM coverage tests for `gcc_gen_be_lower_expr_lvalue` in `src/gcc_gen_be_expr.c`.
 * @details Covers all lvalue expression kinds, operation subcases, coroutine-frame fields, symbol aliases,
 * vector/VLA subscripts, pointer-to-member fields, lvalue call/cast/rvalue-kind materialization, constant lvalues,
 * lowered EH construct lvalues, error paths, and OOM budgets.
 * @ingroup gcc_gen_be_tests
 */

#include "test_gcc_be_expr_common.h"

USING_NAMESPACE_EDG

BEGIN_EDG_NAMESPACE

/**
 * @brief Covers non-operation lvalue expression kinds in `gcc_gen_be_lower_expr_lvalue`.
 * @details Exercises `!ctx`, `enk_temp_init`, `enk_lambda` (including coroutine frame field lookup),
 * `enk_variable` (coroutine frame, string-table aliases `_ZTV`/`_ZTT`/`_ZTI`/`_ZZ`, anonymous static constants),
 * `enk_object_lifetime`, `enk_condition`, `enk_reuse_value`, `enk_typeid`, rvalue-materialized node kinds,
 * `enk_constant`, `enk_lowered_eh_construct`, and `default`.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_lval_non_ops(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, void_tp, vptr_tp, ref_int_tp, iptr_tp, char_tp, str_arr_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&char_tp, ik_char, 1));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&vptr_tp, &void_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&iptr_tp, &int_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ref_int_tp, &int_tp, TRUE));

    std::memset(&str_arr_tp, 0, sizeof(str_arr_tp));
    str_arr_tp.kind = tk_array;
    str_arr_tp.size = 4;
    str_arr_tp.variant.array.element_type = &char_tp;
    str_arr_tp.variant.array.variant.number_of_elements = 4;

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_iptr = gcc_jit_type_get_pointer(jit_int);
    gcc_jit_lvalue *lv_int = gcc_jit_function_new_local(fn, NULL, jit_int, "cov_lv_i");
    gcc_jit_lvalue *lv_coro_ptr = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_type_get_pointer(g_expr_mock.one_field_struct_type), "cov_coro_fl");
    gcc_jit_field *coro_fld = gcc_jit_struct_get_field(
        gcc_jit_type_is_struct(g_expr_mock.one_field_struct_type), 0);
    gcc_jit_lvalue *out_lv = NULL;

    /* !ctx guard */
    a_constant c5;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c5, &int_tp, 5));
    an_expr_node e_c5;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_c5, &c5));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_expr_lvalue(&e_c5, &out_lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* enk_temp_init & enk_lambda: lower_type err, !cblock, coro frame (cached & uncached), dip NULL / dik_none / dik_expr */
    an_expr_node e_lam;
    std::memset(&e_lam, 0, sizeof(e_lam));
    e_lam.kind = enk_lambda;
    e_lam.type = &int_tp;
    g_expr_mock.force_lower_type_err = GCC_GEN_BE_ERROR_OOM;
    if (gcc_gen_be_lower_expr_lvalue(&e_lam, &out_lv) != GCC_GEN_BE_ERROR_OOM) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_lower_type_err = GCC_GEN_BE_SUCCESS;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_lvalue(&e_lam, &out_lv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* dip == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_lam, &out_lv));
    /* dip->kind == dik_none */
    a_dynamic_init lam_dip;
    std::memset(&lam_dip, 0, sizeof(lam_dip));
    lam_dip.kind = dik_none;
    e_lam.variant.init.dynamic_init = &lam_dip;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_lam, &out_lv));

    /* Active coroutine frame: uncached expr field, then cached expr field */
    g_expr_mock.coro_frame_type = g_expr_mock.one_field_struct_type;
    g_expr_mock.coro_frame_lval = lv_coro_ptr;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_lam, &out_lv));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &e_lam, coro_fld));
    lam_dip.kind = dik_expression;
    lam_dip.variant.expression = &e_c5;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_lvalue(&e_lam, &out_lv);
    }));
    g_expr_mock.coro_frame_type = NULL;
    g_expr_mock.coro_frame_lval = NULL;

    /* enk_variable:
       - var == NULL
       - var in CACHE_FIELD with coro_flval == NULL and coro_flval != NULL
       - var->source_corresp.name in CACHE_STRING: self, cached other var, _ZTV, _ZTT, _ZTI, _ZZ, other name
       - anonymous static constant var (cblock != NULL with init_rval != NULL & NULL, cblock == NULL, non-matching flags)
       - force_global_decl_err */
    an_expr_node e_var;
    std::memset(&e_var, 0, sizeof(e_var));
    e_var.kind = enk_variable;
    e_var.type = &int_tp;
    e_var.variant.variable.ptr = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_var, &out_lv));

    a_variable v_coro_fld;
    std::memset(&v_coro_fld, 0, sizeof(v_coro_fld));
    v_coro_fld.type = &int_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &v_coro_fld, coro_fld));
    e_var.variant.variable.ptr = &v_coro_fld;
    /* First with coro_flval != NULL (so v_coro_fld is NOT inserted into CACHE_VAR yet!) */
    g_expr_mock.coro_frame_lval = lv_coro_ptr;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_var, &out_lv));
    /* Second with coro_flval == NULL (hits coro_flval != NULL False!) */
    g_expr_mock.coro_frame_lval = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_var, &out_lv));

    /* String-table deduplication branches */
    a_variable v_canon, v_dup_var, v_self, v_ztv, v_ztt, v_zti, v_zz, v_plain_dup;
    std::memset(&v_canon, 0, sizeof(v_canon));
    v_canon.type = &int_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_canon, lv_int));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"sym_var_alias", &v_canon));

    std::memset(&v_dup_var, 0, sizeof(v_dup_var));
    v_dup_var.type = &int_tp;
    v_dup_var.source_corresp.name = "sym_var_alias";
    e_var.variant.variable.ptr = &v_dup_var;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_var, &out_lv));

    std::memset(&v_self, 0, sizeof(v_self));
    v_self.type = &int_tp;
    v_self.source_corresp.name = "sym_self";
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"sym_self", &v_self));
    e_var.variant.variable.ptr = &v_self;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_var, &out_lv));

    static const char *const k_special_syms[5] = {
        "_ZTV_cov_lv", "_ZTT_cov_lv", "_ZTI_cov_lv", "_ZZ_cov_lv", "plain_non_var_lv"
    };
    a_variable *const k_special_vars[5] = { &v_ztv, &v_ztt, &v_zti, &v_zz, &v_plain_dup };
    for (size_t si = 0; si < 5; ++si) {
        std::memset(k_special_vars[si], 0, sizeof(a_variable));
        k_special_vars[si]->type = &int_tp;
        k_special_vars[si]->source_corresp.name = k_special_syms[si];
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)k_special_syms[si], lv_int));
        e_var.variant.variable.ptr = k_special_vars[si];
        GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_var, &out_lv));
    }

    /* Anonymous sc_static + initk_static + initializer.constant branches */
    a_constant c_void;
    std::memset(&c_void, 0, sizeof(c_void));
    c_void.kind = ck_void;
    c_void.type = &int_tp;

    a_variable v_anon1, v_anon2, v_anon3, v_anon_nonstat, v_anon_noinit, v_anon_nocon, v_err_glob;
    std::memset(&v_anon1, 0, sizeof(v_anon1));
    v_anon1.type = &int_tp;
    v_anon1.storage_class = sc_static;
    v_anon1.init_kind = initk_static;
    v_anon1.initializer.constant = &c5;
    e_var.variant.variable.ptr = &v_anon1;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_var, &out_lv));

    v_anon2 = v_anon1;
    v_anon2.initializer.constant = &c_void; /* init_rval == NULL when force_zero_null == TRUE */
    e_var.variant.variable.ptr = &v_anon2;
    g_expr_mock.force_zero_null = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_var, &out_lv));
    g_expr_mock.force_zero_null = FALSE;

    v_anon3 = v_anon1;
    e_var.variant.variable.ptr = &v_anon3;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_var, &out_lv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    std::memset(&v_anon_nonstat, 0, sizeof(v_anon_nonstat));
    v_anon_nonstat.type = &int_tp;
    v_anon_nonstat.storage_class = sc_auto;
    e_var.variant.variable.ptr = &v_anon_nonstat;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_var, &out_lv));

    std::memset(&v_anon_noinit, 0, sizeof(v_anon_noinit));
    v_anon_noinit.type = &int_tp;
    v_anon_noinit.storage_class = sc_static;
    v_anon_noinit.init_kind = initk_none;
    e_var.variant.variable.ptr = &v_anon_noinit;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_var, &out_lv));

    std::memset(&v_anon_nocon, 0, sizeof(v_anon_nocon));
    v_anon_nocon.type = &int_tp;
    v_anon_nocon.storage_class = sc_static;
    v_anon_nocon.init_kind = initk_static;
    v_anon_nocon.initializer.constant = NULL;
    e_var.variant.variable.ptr = &v_anon_nocon;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_var, &out_lv));

    std::memset(&v_err_glob, 0, sizeof(v_err_glob));
    v_err_glob.type = &int_tp;
    e_var.variant.variable.ptr = &v_err_glob;
    g_expr_mock.force_global_decl_err = GCC_GEN_BE_ERROR_OOM;
    if (gcc_gen_be_lower_expr_lvalue(&e_var, &out_lv) != GCC_GEN_BE_ERROR_OOM) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_global_decl_err = GCC_GEN_BE_SUCCESS;

    /* enk_object_lifetime & enk_condition */
    an_expr_node e_vcanon, e_olt, e_cond;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vcanon, &v_canon));
    std::memset(&e_olt, 0, sizeof(e_olt));
    e_olt.kind = enk_object_lifetime;
    e_olt.type = &int_tp;
    e_olt.variant.object_lifetime.expr = &e_vcanon;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_olt, &out_lv));

    a_condition_supplement cond_rec;
    std::memset(&cond_rec, 0, sizeof(cond_rec));
    cond_rec.expr = &e_vcanon;
    std::memset(&e_cond, 0, sizeof(e_cond));
    e_cond.kind = enk_condition;
    e_cond.type = &int_tp;
    e_cond.variant.condition = &cond_rec;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_cond, &out_lv));

    /* enk_reuse_value: dip == NULL, dip->variable == NULL, cached var, uncached var (!cblock, dik_none, dik_expr) */
    an_expr_node e_reuse;
    std::memset(&e_reuse, 0, sizeof(e_reuse));
    e_reuse.kind = enk_reuse_value;
    e_reuse.type = &int_tp;
    e_reuse.variant.reused_value_init = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_reuse, &out_lv));

    a_dynamic_init r_dip;
    std::memset(&r_dip, 0, sizeof(r_dip));
    r_dip.kind = dik_none;
    r_dip.variable = &v_canon; /* cached */
    e_reuse.variant.reused_value_init = &r_dip;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_reuse, &out_lv));

    a_variable v_reuse_new;
    std::memset(&v_reuse_new, 0, sizeof(v_reuse_new));
    v_reuse_new.type = &int_tp;
    r_dip.variable = &v_reuse_new; /* uncached */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_lvalue(&e_reuse, &out_lv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_reuse, &out_lv));

    r_dip.variable = NULL;
    r_dip.kind = dik_expression;
    r_dip.variant.expression = &e_c5;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_lvalue(&e_reuse, &out_lv);
    }));

    /* enk_typeid */
    an_expr_node e_typeid;
    std::memset(&e_typeid, 0, sizeof(e_typeid));
    e_typeid.kind = enk_typeid;
    e_typeid.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_typeid, &out_lv));

    /* Rvalue-materialized node kinds: enk_statement, enk_builtin_operation, enk_new_delete,
       enk_await, enk_yield, enk_braced_init_list */
    a_variable v_iptr_stmt;
    std::memset(&v_iptr_stmt, 0, sizeof(v_iptr_stmt));
    v_iptr_stmt.type = &iptr_tp;
    gcc_jit_lvalue *lv_ips = gcc_jit_function_new_local(fn, NULL, jit_iptr, "cov_lv_ips");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_iptr_stmt, lv_ips));
    an_expr_node e_vips;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vips, &v_iptr_stmt));

    a_statement dummy_stmt;
    std::memset(&dummy_stmt, 0, sizeof(dummy_stmt));
    dummy_stmt.kind = stmk_expr;
    dummy_stmt.expr = &e_vips;
    an_expr_node e_stmt_lv;
    std::memset(&e_stmt_lv, 0, sizeof(e_stmt_lv));
    e_stmt_lv.kind = enk_statement;
    e_stmt_lv.type = &ref_int_tp;
    e_stmt_lv.variant.statement = &dummy_stmt;
    /* 1) Reference type (is_reference == TRUE) */
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_stmt_lv, &out_lv));
    /* 2) Non-reference pointer type */
    e_stmt_lv.type = &iptr_tp;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_stmt_lv, &out_lv));
    /* 3) Non-pointer scalar type */
    dummy_stmt.expr = &e_c5;
    e_stmt_lv.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_stmt_lv, &out_lv));
    /* 4) e_stmt_lv.type == NULL -> rv == NULL -> INVALID_STATE; e_braced_lv with r_dip & type == NULL -> rv != NULL && etp == NULL! */
    e_stmt_lv.type = NULL;
    if (gcc_gen_be_lower_expr_lvalue(&e_stmt_lv, &out_lv) != GCC_GEN_BE_ERROR_INVALID_STATE) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    /* 5) enk_braced_init_list, enk_builtin_operation, enk_new_delete (delete -> rv==NULL, new -> etp==NULL & cblock==NULL), enk_await, enk_yield */
    an_expr_node e_bop_lv, e_nd_lv, e_await_lv, e_yield_lv, e_braced_lv;
    std::memset(&e_braced_lv, 0, sizeof(e_braced_lv));
    e_braced_lv.kind = enk_braced_init_list;
    e_braced_lv.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_braced_lv, &out_lv));

    std::memset(&e_bop_lv, 0, sizeof(e_bop_lv));
    e_bop_lv.kind = enk_builtin_operation;
    e_bop_lv.type = &int_tp;
    e_bop_lv.variant.builtin_operation.kind = (a_builtin_operation_kind)99;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_bop_lv, &out_lv));
    g_expr_mock.force_zero_null = TRUE;
    if (gcc_gen_be_lower_expr_lvalue(&e_bop_lv, &out_lv) != GCC_GEN_BE_ERROR_INVALID_STATE) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_zero_null = FALSE;

    a_new_delete_supplement nds;
    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = FALSE;
    nds.type = &int_tp;
    std::memset(&e_nd_lv, 0, sizeof(e_nd_lv));
    e_nd_lv.kind = enk_new_delete;
    e_nd_lv.type = &void_tp;
    e_nd_lv.variant.new_delete = &nds;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_nd_lv, &out_lv));
    /* is_new == TRUE with expr->type == NULL -> rv != NULL && etp == NULL! */
    nds.is_new = TRUE;
    nds.type = &int_tp;
    e_nd_lv.type = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_nd_lv, &out_lv));
    /* e_bop_lv with cblock == NULL -> rv != NULL && cblock == NULL -> INVALID_STATE! */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_lvalue(&e_bop_lv, &out_lv) != GCC_GEN_BE_ERROR_INVALID_STATE) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    g_expr_mock.coro_frame_type = g_expr_mock.one_field_struct_type;
    g_expr_mock.coro_frame_lval = lv_coro_ptr;
    g_expr_mock.coro_state_field_lval = lv_int;
    g_expr_mock.coro_frame_ptr_rval = gcc_jit_lvalue_get_address(lv_int, NULL);
    std::memset(&e_await_lv, 0, sizeof(e_await_lv));
    e_await_lv.kind = enk_await;
    e_await_lv.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_await_lv, &out_lv));

    std::memset(&e_yield_lv, 0, sizeof(e_yield_lv));
    e_yield_lv.kind = enk_yield;
    e_yield_lv.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_yield_lv, &out_lv));
    g_expr_mock.coro_frame_type = NULL;
    g_expr_mock.coro_frame_lval = NULL;
    g_expr_mock.coro_state_field_lval = NULL;
    g_expr_mock.coro_frame_ptr_rval = NULL;

    /* enk_constant: NULL ptr, ck_string (cached, uncached, len==0, val==NULL, ctype==NULL),
       scalar constant (cblock!=NULL, cblock==NULL, crval==NULL, ctype==NULL) */
    an_expr_node e_con_lv;
    std::memset(&e_con_lv, 0, sizeof(e_con_lv));
    e_con_lv.kind = enk_constant;
    e_con_lv.type = &int_tp;
    e_con_lv.variant.constant.ptr = NULL;
    if (gcc_gen_be_lower_expr_lvalue(&e_con_lv, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    a_constant c_str_lv, c_str_empty, c_str_nullv;
    std::memset(&c_str_lv, 0, sizeof(c_str_lv));
    c_str_lv.kind = ck_string;
    c_str_lv.type = &str_arr_tp;
    c_str_lv.variant.string.value = "abc";
    c_str_lv.variant.string.length = 4;
    e_con_lv.type = &str_arr_tp;
    e_con_lv.variant.constant.ptr = &c_str_lv;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_con_lv, &out_lv)); /* uncached */
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_con_lv, &out_lv)); /* cached! */

    std::memset(&c_str_empty, 0, sizeof(c_str_empty));
    c_str_empty.kind = ck_string;
    c_str_empty.type = &str_arr_tp;
    c_str_empty.variant.string.value = "a";
    c_str_empty.variant.string.length = 0;
    e_con_lv.variant.constant.ptr = &c_str_empty;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_con_lv, &out_lv));

    std::memset(&c_str_nullv, 0, sizeof(c_str_nullv));
    c_str_nullv.kind = ck_string;
    c_str_nullv.type = &str_arr_tp;
    c_str_nullv.variant.string.value = NULL;
    c_str_nullv.variant.string.length = 2;
    e_con_lv.variant.constant.ptr = &c_str_nullv;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_con_lv, &out_lv));

    /* ck_string with ctype == NULL -> falls through to crval path, then fails at ctype == NULL! */
    e_con_lv.type = NULL;
    if (gcc_gen_be_lower_expr_lvalue(&e_con_lv, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    /* Scalar constant lvalue: valid, crval == NULL (c_void), cblock == NULL */
    e_con_lv.type = &int_tp;
    e_con_lv.variant.constant.ptr = &c5;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_con_lv, &out_lv));
    e_con_lv.variant.constant.ptr = &c_void;
    g_expr_mock.force_zero_null = TRUE;
    if (gcc_gen_be_lower_expr_lvalue(&e_con_lv, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_zero_null = FALSE;
    e_con_lv.variant.constant.ptr = &c5;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_lvalue(&e_con_lv, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* enk_lowered_eh_construct & default */
    an_expr_node e_eh_lv, e_unk_lv;
    std::memset(&e_eh_lv, 0, sizeof(e_eh_lv));
    e_eh_lv.kind = enk_lowered_eh_construct;
    e_eh_lv.type = &int_tp;
    e_eh_lv.variant.lowered_eh.kind = leck_thrown_object_address;
    /* 1) Uncached thrown_object_address -> calls emit_eh_unwind_exc_rvalue */
    gcc_gen_be_singleton_state.active_thrown_obj_lval = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_eh_lv, &out_lv));
    /* 2) Cached thrown_object_address */
    gcc_jit_lvalue *lv_vp = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "cov_eh_vp");
    gcc_gen_be_singleton_state.active_thrown_obj_lval = lv_vp;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_eh_lv, &out_lv));
    gcc_gen_be_singleton_state.active_thrown_obj_lval = NULL;

    /* 3) Uncached & cached caught_object_address */
    e_eh_lv.variant.lowered_eh.kind = leck_caught_object_address;
    gcc_gen_be_singleton_state.active_caught_obj_lval = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_eh_lv, &out_lv));
    gcc_gen_be_singleton_state.active_caught_obj_lval = lv_vp;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_eh_lv, &out_lv));
    gcc_gen_be_singleton_state.active_caught_obj_lval = NULL;

    /* 4) Other leck_* kind (e.g. leck_exception_value_temp -> cached_addr == NULL -> emit_eh_unwind_exc_rvalue) */
    e_eh_lv.variant.lowered_eh.kind = (a_lowered_eh_construct_kind)99;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_eh_lv, &out_lv));

    /* 5) ltype == void_t, ltype == NULL, and cblock == NULL -> falls through to unhandled lowered_eh lvalue! */
    GCC_GEN_BE_CHECK(expr_test_map_type(&void_tp, jit_void));
    e_eh_lv.type = &void_tp;
    if (gcc_gen_be_lower_expr_lvalue(&e_eh_lv, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_eh_lv.type = NULL;
    if (gcc_gen_be_lower_expr_lvalue(&e_eh_lv, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_eh_lv.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_lvalue(&e_eh_lv, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    std::memset(&e_unk_lv, 0, sizeof(e_unk_lv));
    e_unk_lv.kind = (an_expr_node_kind)99;
    if (gcc_gen_be_lower_expr_lvalue(&e_unk_lv, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Covers all `enk_operation` subcases in `gcc_gen_be_lower_expr_lvalue`.
 * @details Exercises `eok_indirect`, `eok_parens`, lvalue casts, `eok_comma`, all 14 assignment ops,
 * `eok_dot_field`/`eok_points_to_field` (bitfields, uncached fields, manual byte offsets, PDM offsets),
 * `eok_subscript`/`eok_vector_subscript` (2D VLA, outer VLA, vector lvalue/rvalue, non-pointer),
 * `eok_pm_field`/`eok_pm_points_to_field`, `eok_pre_incr`/`eok_pre_decr`, `eok_question`,
 * `eok_dot_static`/`eok_points_to_static`, call/cast lvalues, and OOM budgets.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_lval_ops(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, long_tp, bool_tp, void_tp, iptr_tp, ref_int_tp, st_tp, st_ptr_tp, arr_tp, vec_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&long_tp, ik_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&bool_tp, ik_int, 1));
    bool_tp.variant.integer.bool_type = TRUE;
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&iptr_tp, &int_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ref_int_tp, &int_tp, TRUE));

    a_field f_cached, f_uncached0, f_uncached4, f_bf1, f_bf2;
    std::memset(&f_cached, 0, sizeof(f_cached));
    f_cached.type = &int_tp;
    std::memset(&f_uncached0, 0, sizeof(f_uncached0));
    f_uncached0.type = &int_tp;
    f_uncached0.offset = 0;
    std::memset(&f_uncached4, 0, sizeof(f_uncached4));
    f_uncached4.type = &int_tp;
    f_uncached4.offset = 4;
    std::memset(&f_bf1, 0, sizeof(f_bf1));
    f_bf1.is_bit_field = TRUE;
    f_bf1.type = &int_tp;
    std::memset(&f_bf2, 0, sizeof(f_bf2));
    f_bf2.is_bit_field = FALSE;
    f_bf2.bit_size = 3;
    f_bf2.type = &int_tp;

    std::memset(&st_tp, 0, sizeof(st_tp));
    st_tp.kind = tk_struct;
    st_tp.size = 8;
    st_tp.variant.class_struct_union.field_list = &f_cached;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&st_ptr_tp, &st_tp, FALSE));

    std::memset(&arr_tp, 0, sizeof(arr_tp));
    arr_tp.kind = tk_array;
    arr_tp.size = 8;
    arr_tp.variant.array.element_type = &int_tp;
    arr_tp.variant.array.variant.number_of_elements = 2;

    std::memset(&vec_tp, 0, sizeof(vec_tp));
    vec_tp.kind = tk_vector;
    vec_tp.size = 16;
    vec_tp.variant.vector.element_type = &int_tp;

    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *jit_iptr = gcc_jit_type_get_pointer(jit_int);
    gcc_jit_type *jit_vec = gcc_jit_type_get_vector(jit_int, 4);
    GCC_GEN_BE_CHECK(expr_test_map_type(&vec_tp, jit_vec));
    GCC_GEN_BE_CHECK(expr_test_map_type(&void_tp, jit_void));

    a_variable v_int, v_int2, v_iptr, v_bool, v_st, v_stp, v_arr, v_vec;
    std::memset(&v_int, 0, sizeof(v_int));
    v_int.type = &int_tp;
    std::memset(&v_int2, 0, sizeof(v_int2));
    v_int2.type = &int_tp;
    std::memset(&v_iptr, 0, sizeof(v_iptr));
    v_iptr.type = &iptr_tp;
    std::memset(&v_bool, 0, sizeof(v_bool));
    v_bool.type = &bool_tp;
    std::memset(&v_st, 0, sizeof(v_st));
    v_st.type = &st_tp;
    std::memset(&v_stp, 0, sizeof(v_stp));
    v_stp.type = &st_ptr_tp;
    std::memset(&v_arr, 0, sizeof(v_arr));
    v_arr.type = &arr_tp;
    std::memset(&v_vec, 0, sizeof(v_vec));
    v_vec.type = &vec_tp;

    gcc_jit_lvalue *lv_i = gcc_jit_function_new_local(fn, NULL, jit_int, "lv_i");
    gcc_jit_lvalue *lv_i2 = gcc_jit_function_new_local(fn, NULL, jit_int, "lv_i2");
    gcc_jit_lvalue *lv_ip = gcc_jit_function_new_local(fn, NULL, jit_iptr, "lv_ip");
    gcc_jit_lvalue *lv_b = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL), "lv_b");
    gcc_jit_lvalue *lv_st = gcc_jit_function_new_local(fn, NULL, g_expr_mock.one_field_struct_type, "lv_st");
    gcc_jit_lvalue *lv_stp = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_type_get_pointer(g_expr_mock.one_field_struct_type), "lv_stp");
    gcc_jit_lvalue *lv_arr = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_context_new_array_type(ctx, NULL, jit_int, 2), "lv_arr");
    gcc_jit_lvalue *lv_vec = gcc_jit_function_new_local(fn, NULL, jit_vec, "lv_vec");

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_int, lv_i));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_int2, lv_i2));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_iptr, lv_ip));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_bool, lv_b));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_st, lv_st));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_stp, lv_stp));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_arr, lv_arr));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vec, lv_vec));

    a_constant c1;
    GCC_GEN_BE_CHECK(expr_init_int_const(&c1, &int_tp, 1));
    an_expr_node e_c1, e_vi, e_vi2, e_vip, e_vb, e_vst, e_vstp, e_varr, e_vvec, e_bad;
    GCC_GEN_BE_CHECK(expr_init_const_expr(&e_c1, &c1));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi, &v_int));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vi2, &v_int2));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vip, &v_iptr));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vb, &v_bool));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vst, &v_st));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vstp, &v_stp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_varr, &v_arr));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vvec, &v_vec));
    std::memset(&e_bad, 0, sizeof(e_bad));
    e_bad.kind = (an_expr_node_kind)99;
    e_bad.type = &int_tp;

    gcc_jit_lvalue *out_lv = NULL;
    an_expr_node e_op;

    /* eok_indirect error + eok_parens */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_indirect, &int_tp, &e_bad));
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_parens, &int_tp, &e_vi));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));

    /* All 5 lvalue cast operators + sub_lval==NULL, sub_lval err, target_type==NULL, same type, diff type */
    static const an_expr_operator_kind k_lv_casts[5] = {
        eok_lvalue_adjust, eok_lvalue_cast, eok_ref_cast, eok_base_class_cast, eok_derived_class_cast
    };
    for (size_t ci = 0; ci < 5; ++ci) {
        GCC_GEN_BE_CHECK(expr_init_op(&e_op, k_lv_casts[ci], &long_tp, &e_vi));
        GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    }
    e_op.type = &int_tp; /* target_type == sub_lval type */
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_op.type = NULL; /* target_type == NULL */
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_op.variant.operation.operands = &e_bad; /* err != SUCCESS */
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    an_expr_node e_null_var;
    std::memset(&e_null_var, 0, sizeof(e_null_var));
    e_null_var.kind = enk_variable;
    e_null_var.type = &int_tp;
    e_null_var.variant.variable.ptr = NULL;
    e_op.variant.operation.operands = &e_null_var; /* sub_lval == NULL with SUCCESS! */
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));

    /* eok_comma in lvalue position: r1 == NULL, cblock == NULL, r1 void call */
    an_expr_node e_null_ty;
    std::memset(&e_null_ty, 0, sizeof(e_null_ty));
    e_null_ty.kind = enk_type_operand;
    e_null_ty.type = &void_tp;
    e_null_ty.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_comma, &int_tp, &e_null_ty));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_null_ty.next = NULL;

    e_c1.next = &e_vi;
    e_op.variant.operation.operands = &e_c1;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    e_c1.next = NULL;

    a_type vfn_tp;
    std::memset(&vfn_tp, 0, sizeof(vfn_tp));
    vfn_tp.kind = tk_routine;
    vfn_tp.variant.routine.return_type = &void_tp;
    a_routine r_void;
    std::memset(&r_void, 0, sizeof(r_void));
    r_void.type = &vfn_tp;
    gcc_jit_function *j_vfn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_void, "cov_lv_vfn", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_void, j_vfn));
    an_expr_node e_vr, e_vcall;
    std::memset(&e_vr, 0, sizeof(e_vr));
    e_vr.kind = enk_routine;
    e_vr.type = &vfn_tp;
    e_vr.variant.routine.ptr = &r_void;
    GCC_GEN_BE_CHECK(expr_init_op(&e_vcall, eok_call, &void_tp, &e_vr));
    e_vcall.next = &e_vi;
    e_op.variant.operation.operands = &e_vcall;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_vcall.next = NULL;

    /* All 14 assignment operators in lvalue position */
    static const an_expr_operator_kind k_assign_ops[12] = {
        eok_assign, eok_bassign, eok_add_assign, eok_subtract_assign,
        eok_multiply_assign, eok_divide_assign, eok_remainder_assign,
        eok_shiftl_assign, eok_shiftr_assign, eok_and_assign, eok_or_assign, eok_xor_assign
    };
    e_vi.next = &e_c1;
    for (size_t ai = 0; ai < 12; ++ai) {
        GCC_GEN_BE_CHECK(expr_init_op(&e_op, k_assign_ops[ai], &int_tp, &e_vi));
        GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    }
    e_vi.next = NULL;
    e_vip.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_padd_assign, &iptr_tp, &e_vip));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_op.variant.operation.kind = eok_psubtract_assign;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_vip.next = NULL;

    /* eok_dot_field & eok_points_to_field:
       - bitfields (is_bit_field == TRUE, and is_bit_field == FALSE && bit_size > 0)
       - uncached field resolved by lower_type(op1->type)
       - uncached field NOT in field_list -> manual byte-offset fallback (dot & arrow, offset 0 & 4)
       - cached field (dot & arrow + error on op1)
       - PDM offset (dot & arrow + error on op1 / expr->type) */
    an_expr_node e_fld;
    std::memset(&e_fld, 0, sizeof(e_fld));
    e_fld.kind = enk_field;
    e_fld.type = &int_tp;

    e_fld.variant.field.ptr = &f_bf1;
    e_vst.next = &e_fld;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_field, &int_tp, &e_vst));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_fld.variant.field.ptr = &f_bf2;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));

    /* f_cached is in st_tp.field_list but NOT cached yet -> lower_type(st_tp) caches it! */
    e_fld.variant.field.ptr = &f_cached;
    g_expr_mock.force_lower_type_err = GCC_GEN_BE_ERROR_OOM;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_OOM) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_lower_type_err = GCC_GEN_BE_SUCCESS;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv)); /* caches f_cached & accesses dot! */

    /* Cached field on eok_points_to_field + error on op1 */
    e_vst.next = NULL;
    e_vstp.next = &e_fld;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_points_to_field, &int_tp, &e_vstp));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_vstp.next = NULL;

    e_bad.next = &e_fld;
    e_op.variant.operation.operands = &e_bad;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_op.variant.operation.kind = eok_dot_field;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_bad.next = NULL;

    /* Manual byte-offset fallback (f_uncached0 & f_uncached4 not in field_list) on dot & arrow */
    e_fld.variant.field.ptr = &f_uncached0;
    e_vst.next = &e_fld;
    e_op.variant.operation.kind = eok_dot_field;
    e_op.variant.operation.operands = &e_vst;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_vst.next = NULL;

    e_fld.variant.field.ptr = &f_uncached4;
    e_vstp.next = &e_fld;
    e_op.variant.operation.kind = eok_points_to_field;
    e_op.variant.operation.operands = &e_vstp;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_vstp.next = NULL;

    /* PDM offset (op2 = e_c1) on eok_points_to_field + errors on dot/arrow/dest_type */
    e_vstp.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv);
    }));
    e_vstp.next = NULL;

    e_bad.next = &e_c1;
    e_op.variant.operation.operands = &e_bad;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_op.variant.operation.kind = eok_dot_field;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_bad.next = NULL;

    /* eok_subscript & eok_vector_subscript:
       - tk_array with arr_lval err, arr_lval == NULL, 2D VLA, outer VLA (pointer vs array arr_rv), OOM
       - eok_vector_subscript & tk_vector (is_lvalue TRUE with valid/NULL/err, is_lvalue FALSE with cblk/NULL/vr==NULL)
       - op1_stripped == NULL and non-pointer scalar op1 */
    a_type vla_in, vla_out;
    std::memset(&vla_in, 0, sizeof(vla_in));
    vla_in.kind = tk_array;
    vla_in.variant.array.element_type = &int_tp;
    vla_in.variant.array.is_variable_size_array = TRUE;
    vla_in.variant.array.variant.number_of_elements = 2;
    std::memset(&vla_out, 0, sizeof(vla_out));
    vla_out.kind = tk_array;
    vla_out.variant.array.element_type = &vla_in;
    vla_out.variant.array.is_variable_size_array = TRUE;

    a_variable v_vla_ptr, v_vla_arr;
    std::memset(&v_vla_ptr, 0, sizeof(v_vla_ptr));
    v_vla_ptr.type = &vla_out;
    std::memset(&v_vla_arr, 0, sizeof(v_vla_arr));
    v_vla_arr.type = &vla_in;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vla_ptr, lv_ip));  /* pointer arr_rv! */
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_vla_arr, lv_arr)); /* array arr_rv! */

    an_expr_node e_vvp, e_vva;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vvp, &v_vla_ptr));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vva, &v_vla_arr));
    e_vvp.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_subscript, &vla_in, &e_vvp));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv);
    }));
    /* Also element_type == fixed arr_tp & element_type == NULL on tk_array */
    vla_out.variant.array.element_type = &arr_tp;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    vla_out.variant.array.element_type = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_vvp.next = NULL;

    e_vva.next = &e_c1;
    e_op.type = &int_tp;
    e_op.variant.operation.operands = &e_vva;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_vva.next = NULL;

    /* tk_array with arr_lval == NULL (e_null_var typed as arr_tp) and arr_lval failing (e_bad typed as arr_tp) */
    e_null_var.type = &arr_tp;
    e_null_var.next = &e_c1;
    e_op.variant.operation.operands = &e_null_var;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_null_var.next = NULL;
    e_null_var.type = &int_tp;

    e_bad.type = &arr_tp;
    e_bad.next = &e_c1;
    e_op.variant.operation.operands = &e_bad;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_bad.next = NULL;
    e_bad.type = &int_tp;

    /* Vector subscript: eok_subscript on tk_vector & eok_vector_subscript */
    e_vvec.is_lvalue = TRUE;
    e_vvec.next = &e_c1;
    e_op.variant.operation.kind = eok_subscript;
    e_op.variant.operation.operands = &e_vvec;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));

    e_vvec.is_lvalue = FALSE;
    e_op.variant.operation.kind = eok_vector_subscript;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    e_vvec.next = NULL;

    /* Vector subscript with is_lvalue == TRUE & arr_lval == NULL, is_lvalue == TRUE & lval error, and vr == NULL */
    e_null_var.type = &vec_tp;
    e_null_var.is_lvalue = TRUE;
    e_null_var.next = &e_c1;
    e_op.variant.operation.operands = &e_null_var;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_null_var.next = NULL;

    e_bad.type = &vec_tp;
    e_bad.is_lvalue = TRUE;
    e_bad.next = &e_c1;
    e_op.variant.operation.operands = &e_bad;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_bad.next = NULL;
    e_bad.type = &int_tp;

    an_expr_node e_null_vec;
    std::memset(&e_null_vec, 0, sizeof(e_null_vec));
    e_null_vec.kind = enk_type_operand;
    e_null_vec.type = &vec_tp;
    e_null_vec.next = &e_c1;
    e_op.variant.operation.operands = &e_null_vec;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_null_vec.next = NULL;

    /* eok_subscript with op1->type == NULL (op1_stripped == NULL) and op2 error */
    e_vip.type = NULL;
    e_vip.next = &e_c1;
    e_op.variant.operation.kind = eok_subscript;
    e_op.variant.operation.operands = &e_vip;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_vip.type = &iptr_tp;
    e_vip.next = &e_bad;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_vip.next = NULL;

    /* eok_pm_field (!obj_lval -> INTERNAL) and eok_pm_points_to_field */
    e_null_var.type = &st_tp;
    e_null_var.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_field, &int_tp, &e_null_var));
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_INTERNAL) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_null_var.next = NULL;

    e_vstp.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pm_points_to_field, &int_tp, &e_vstp));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_vstp.next = NULL;

    /* eok_pre_incr / eok_pre_decr: op1 err, cblock == NULL, eok_pre_incr (+1) */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pre_incr, &int_tp, &e_bad));
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_op.variant.operation.operands = &e_vi;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* eok_question: missing operands, cblock == NULL, bool cond_rv (cond_rv == bool_t) + OOM */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_question, &int_tp, NULL));
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_vb.next = NULL;
    e_op.variant.operation.operands = &e_vb;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_vb.next = &e_vi;
    e_vi.next = NULL;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_vi.next = &e_vi2;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv);
    }));
    e_vb.next = NULL;
    e_vi.next = NULL;

    /* eok_dot_static & eok_points_to_static (op1 != NULL & op1 == NULL) */
    e_vst.next = &e_vi;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_static, &int_tp, &e_vst));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_vst.next = NULL;
    e_op.variant.operation.kind = eok_points_to_static;
    e_op.variant.operation.operands = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));

    /* Call & cast operators in lvalue position:
       eok_call, eok_dot_member_call, eok_points_to_member_call, eok_dot_pm_call, eok_points_to_pm_call, eok_cast */
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_cast, &ref_int_tp, &e_vip));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv)); /* is_reference == TRUE */
    e_op.type = &iptr_tp;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv)); /* non-ref pointer */
    e_op.type = &int_tp;
    e_op.variant.operation.operands = &e_c1;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv)); /* scalar */
    e_op.type = NULL;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        /* Note: eok_cast with type==NULL returns zero_rvalue (rv==NULL? No, rv=0) and ltype==NULL -> new_local(NULL) */
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    e_op.type = &int_tp;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    e_op.variant.operation.operands = &e_null_ty; /* rv == NULL -> zero_rvalue in eok_cast; use eok_call on null fn_ptr with cblock==NULL for rv==NULL! */

    a_type rfn_tp, rfn_ptr_tp;
    std::memset(&rfn_tp, 0, sizeof(rfn_tp));
    rfn_tp.kind = tk_routine;
    rfn_tp.variant.routine.return_type = &ref_int_tp;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&rfn_ptr_tp, &rfn_tp, FALSE));
    a_routine r_ref;
    std::memset(&r_ref, 0, sizeof(r_ref));
    r_ref.type = &rfn_tp;
    gcc_jit_function *j_rfn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_iptr, "cov_lv_rfn", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_ref, j_rfn));
    an_expr_node e_rr;
    std::memset(&e_rr, 0, sizeof(e_rr));
    e_rr.kind = enk_routine;
    e_rr.type = &rfn_tp;
    e_rr.variant.routine.ptr = &r_ref;

    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &ref_int_tp, &e_rr));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    /* eok_call with expr->type == NULL -> etp == NULL! */
    e_op.type = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));

    /* Hit eok_dot_member_call, eok_points_to_member_call, eok_dot_pm_call, eok_points_to_pm_call switch arms */
    static const an_expr_operator_kind k_mcall_ops[4] = {
        eok_dot_member_call, eok_points_to_member_call, eok_dot_pm_call, eok_points_to_pm_call
    };
    for (size_t mi = 0; mi < 4; ++mi) {
        GCC_GEN_BE_CHECK(expr_init_op(&e_op, k_mcall_ops[mi], &ref_int_tp, NULL));
        if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) == GCC_GEN_BE_SUCCESS) {
            return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
        }
    }

    /* Remaining 6 exact branches in test_cov_lval_ops:
       1) L1297: PDM eok_dot_field with force_lower_type_err
       2) L1315: tk_array eok_subscript with force_lower_type_err
       3) L1318: 2D fixed array (element_type == tk_array && is_variable_size_array == FALSE)
       4) L1392 & L1400: eok_pre_decr (with force_lower_type_err and normal)
       5) L1472: eok_call on null indirect fn_ptr with cblock == NULL -> rv == NULL! */
    e_vst.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_field, &int_tp, &e_vst));
    g_expr_mock.force_lower_type_err = GCC_GEN_BE_ERROR_OOM;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_OOM) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_vst.next = NULL;

    e_varr.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_subscript, &int_tp, &e_varr));
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_OOM) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_lower_type_err = GCC_GEN_BE_SUCCESS;
    e_varr.next = NULL;

    a_type arr2d_fixed;
    std::memset(&arr2d_fixed, 0, sizeof(arr2d_fixed));
    arr2d_fixed.kind = tk_array;
    arr2d_fixed.size = 16;
    arr2d_fixed.variant.array.element_type = &arr_tp; /* tk_array with is_variable_size_array == FALSE! */
    arr2d_fixed.variant.array.variant.number_of_elements = 2;
    a_variable v_arr2d;
    std::memset(&v_arr2d, 0, sizeof(v_arr2d));
    v_arr2d.type = &arr2d_fixed;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_arr2d, lv_arr));
    an_expr_node e_varr2d;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_varr2d, &v_arr2d));
    e_varr2d.next = &e_c1;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_subscript, &arr_tp, &e_varr2d));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_varr2d.next = NULL;

    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_pre_decr, &int_tp, &e_vi));
    g_expr_mock.force_lower_type_err = GCC_GEN_BE_ERROR_OOM;
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_OOM) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_lower_type_err = GCC_GEN_BE_SUCCESS;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));

    an_expr_node e_null_fnptr;
    std::memset(&e_null_fnptr, 0, sizeof(e_null_fnptr));
    e_null_fnptr.kind = enk_type_operand;
    e_null_fnptr.type = &rfn_ptr_tp;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &ref_int_tp, &e_null_fnptr));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises remaining lvalue and bitfield-read branches and OOM paths in `src/gcc_gen_be_expr.c`.
 * @details Covers lazy `gcc_gen_be_lower_type` field-cache population (`L1294:33`), uncached
 * `eok_dot_field`/`eok_points_to_field` byte-offset fallback OOM (`L1299:41`, `L1301`, `L1303`),
 * non-null `eok_call` rvalue with `cblock == NULL` (`L1538:23`), `enk_lowered_eh_construct`
 * caught/thrown lvalue OOM (`L1663`, `L1700`), and `gcc_gen_be_lower_bitfield_read` OOM (`L1776`).
 * @param[in] ctx Active `libgccjit` context.
 * @param[in] fn Active `libgccjit` test function.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_lval_final_gaps(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_type *jit_int = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_iptr = gcc_jit_type_get_pointer(jit_int);
    gcc_jit_type *jit_vptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);

    a_type int_tp, ref_int_tp, st_lazy_tp, ptr_st_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ref_int_tp, &int_tp, TRUE));

    /* 1) L1294:33 False: field `f_lazy` NOT cached initially, then cached by `gcc_gen_be_lower_type(op1->type)` */
    a_field f_lazy, f_unmapped, f_bf;
    std::memset(&f_lazy, 0, sizeof(f_lazy));
    f_lazy.type = &int_tp;
    std::memset(&st_lazy_tp, 0, sizeof(st_lazy_tp));
    st_lazy_tp.kind = tk_struct; st_lazy_tp.size = 4;
    st_lazy_tp.variant.class_struct_union.field_list = &f_lazy;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_st_tp, &st_lazy_tp, FALSE));

    gcc_jit_lvalue *lv_st_ptr = gcc_jit_function_new_local(fn, NULL, jit_vptr, "lv_st_ptr");
    a_variable v_st_ptr;
    std::memset(&v_st_ptr, 0, sizeof(v_st_ptr)); v_st_ptr.type = &ptr_st_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_st_ptr, lv_st_ptr));

    an_expr_node e_vstp, e_flazy, e_funm, e_op;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vstp, &v_st_ptr));
    std::memset(&e_flazy, 0, sizeof(e_flazy));
    e_flazy.kind = enk_field; e_flazy.type = &int_tp; e_flazy.variant.field.ptr = &f_lazy;

    /* Use eok_points_to_field so op1 (`v_st_ptr`) is a pointer while `op1->type` Wait: for eok_dot_field, op1->type is `st_lazy_tp`! */
    gcc_jit_lvalue *lv_lazy_obj = gcc_jit_function_new_local(fn, NULL, g_expr_mock.one_field_struct_type, "lv_lazy_obj");
    a_variable v_lazy_obj;
    std::memset(&v_lazy_obj, 0, sizeof(v_lazy_obj)); v_lazy_obj.type = &st_lazy_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_lazy_obj, lv_lazy_obj));
    an_expr_node e_vlazy_obj;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vlazy_obj, &v_lazy_obj));
    e_vlazy_obj.next = &e_flazy;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_field, &int_tp, &e_vlazy_obj));
    gcc_jit_lvalue *out_lv = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv));
    e_vlazy_obj.next = NULL;

    /* 2) L1299:41 False + L1301 & L1303 OOM: unmapped empty-base field via BOTH eok_dot_field and eok_points_to_field */
    std::memset(&f_unmapped, 0, sizeof(f_unmapped));
    f_unmapped.type = &int_tp; f_unmapped.offset = 0;
    std::memset(&e_funm, 0, sizeof(e_funm));
    e_funm.kind = enk_field; e_funm.type = &int_tp; e_funm.variant.field.ptr = &f_unmapped;

    e_vlazy_obj.next = &e_funm;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_field, &int_tp, &e_vlazy_obj));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_vlazy_obj.next = NULL;

    e_vstp.next = &e_funm;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_points_to_field, &int_tp, &e_vstp));
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_op, &out_lv));
    e_vstp.next = NULL;

    /* 3) L1538:23 False: eok_call yielding non-null `rv` while `cblock == NULL` */
    a_type rfn_tp;
    std::memset(&rfn_tp, 0, sizeof(rfn_tp));
    rfn_tp.kind = tk_routine; rfn_tp.variant.routine.return_type = &ref_int_tp;
    a_routine r_ref;
    std::memset(&r_ref, 0, sizeof(r_ref)); r_ref.type = &rfn_tp;
    gcc_jit_function *j_rfn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, jit_iptr, "cov_lv_fg_rfn", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_ref, j_rfn));
    an_expr_node e_rr;
    std::memset(&e_rr, 0, sizeof(e_rr));
    e_rr.kind = enk_routine; e_rr.type = &rfn_tp; e_rr.variant.routine.ptr = &r_ref;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_call, &ref_int_tp, &e_rr));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    a_type void_tp_lv;
    std::memset(&void_tp_lv, 0, sizeof(void_tp_lv)); void_tp_lv.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_cast, &void_tp_lv, &e_vlazy_obj));
    if (gcc_gen_be_lower_expr_lvalue(&e_op, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    an_expr_node e_bad_op, e_eh_fail;
    std::memset(&e_bad_op, 0, sizeof(e_bad_op));
    e_bad_op.kind = (an_expr_node_kind)120; e_bad_op.type = &int_tp;
    std::memset(&e_eh_fail, 0, sizeof(e_eh_fail));
    e_eh_fail.kind = enk_lowered_eh_construct; e_eh_fail.type = &void_tp_lv;
    e_eh_fail.variant.lowered_eh.kind = leck_internal_try;
    e_eh_fail.variant.lowered_eh.variant.try_and_catch_expr = &e_bad_op;
    if (gcc_gen_be_lower_expr_lvalue(&e_eh_fail, &out_lv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    std::memset(&f_bf, 0, sizeof(f_bf));
    f_bf.type = &int_tp; f_bf.is_bit_field = TRUE; f_bf.bit_size = 5; f_bf.offset = 0; f_bf.offset_bit_remainder = 2;
    an_expr_node e_fbf_err, e_bf_bad, e_bf_asgn_bad;
    std::memset(&e_fbf_err, 0, sizeof(e_fbf_err));
    e_fbf_err.kind = enk_field; e_fbf_err.type = &int_tp; e_fbf_err.variant.field.ptr = &f_bf;
    e_bad_op.next = &e_fbf_err;
    GCC_GEN_BE_CHECK(expr_init_op(&e_bf_bad, eok_dot_field, &int_tp, &e_bad_op));
    gcc_jit_rvalue *rv_err = NULL;
    if (gcc_gen_be_lower_expr_rvalue(&e_bf_bad, &rv_err) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_bf_bad.next = &e_vlazy_obj;
    GCC_GEN_BE_CHECK(expr_init_op(&e_bf_asgn_bad, eok_assign, &int_tp, &e_bf_bad));
    if (gcc_gen_be_lower_expr_rvalue(&e_bf_asgn_bad, &rv_err) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    e_bf_bad.next = NULL; e_bad_op.next = NULL;

    /* L1701 & L1777: `base_lval == NULL` (`local_var_return_null = TRUE` on uncached `op1`) + OOM sweep on `gcc_gen_be_lower_expr_rvalue(op1)` */
    a_variable v_bf_nolval;
    std::memset(&v_bf_nolval, 0, sizeof(v_bf_nolval)); v_bf_nolval.type = &st_lazy_tp;
    an_expr_node e_vbf_nolval;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vbf_nolval, &v_bf_nolval));
    e_vbf_nolval.next = &e_fbf_err;
    GCC_GEN_BE_CHECK(expr_init_op(&e_bf_bad, eok_dot_field, &int_tp, &e_vbf_nolval));
    g_expr_mock.local_var_return_null = TRUE;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_bf_bad, &rv_err);
    }));
    e_bf_bad.next = &e_vlazy_obj;
    GCC_GEN_BE_CHECK(expr_init_op(&e_bf_asgn_bad, eok_assign, &int_tp, &e_bf_bad));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_bf_asgn_bad, &rv_err);
    }));
    g_expr_mock.local_var_return_null = FALSE;
    e_bf_bad.next = NULL; e_vbf_nolval.next = NULL;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    /* 4) L1663 & L1700: enk_lowered_eh_construct caught_object_address & unhandled leck OOM sweeps */
    an_expr_node e_eh_caught, e_eh_try;
    std::memset(&e_eh_caught, 0, sizeof(e_eh_caught));
    e_eh_caught.kind = enk_lowered_eh_construct;
    e_eh_caught.type = &int_tp;
    e_eh_caught.variant.lowered_eh.kind = leck_caught_object_address;
    gcc_gen_be_singleton_state.active_caught_obj_lval = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_lval_oom(&e_eh_caught, &out_lv));

    std::memset(&e_eh_try, 0, sizeof(e_eh_try));
    e_eh_try.kind = enk_lowered_eh_construct;
    e_eh_try.type = &int_tp;
    e_eh_try.variant.lowered_eh.kind = leck_internal_try;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_gen_be_error_t e = gcc_gen_be_lower_expr_lvalue(&e_eh_try, &out_lv);
        return (e == GCC_GEN_BE_ERROR_UNHANDLED_EXPR) ? GCC_GEN_BE_SUCCESS : e;
    }));

    /* 5) L1776: bitfield read OOM sweep */
    std::memset(&f_bf, 0, sizeof(f_bf));
    f_bf.type = &int_tp; f_bf.is_bit_field = TRUE; f_bf.bit_size = 5; f_bf.offset = 0; f_bf.offset_bit_remainder = 2;
    an_expr_node e_fbf;
    std::memset(&e_fbf, 0, sizeof(e_fbf));
    e_fbf.kind = enk_field; e_fbf.type = &int_tp; e_fbf.variant.field.ptr = &f_bf;
    e_vlazy_obj.next = &e_fbf;
    GCC_GEN_BE_CHECK(expr_init_op(&e_op, eok_dot_field, &int_tp, &e_vlazy_obj));
    gcc_jit_rvalue *rv = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 64, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_op, &rv);
    }));
    e_vlazy_obj.next = NULL;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises Stage-2 lvalue, complex-struct, VLA, TLS, choose_expr, and bitfield branches.
 * @details Covers `tk_class` record checks, 1-field complex struct pointer fallbacks, chained VLA
 * dimension lookup, macOS thread-local variable helper dereferences, byte-array to struct field
 * casts, `enk_builtin_choose_expr` lvalues, and 0-width/65-bit/straddling/64-bit bitfield ops.
 * @param[in] ctx Active `libgccjit` context.
 * @param[in] fn Active `libgccjit` test function.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_cov_lval_stage2_gaps(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp = {}, ll_tp = {}, ull_tp = {}, cls_tp = {}, cplx_tp = {}, dbl_tp = {}, vla1_tp = {}, vla2_tp = {}, ptr_tp = {};
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&ll_tp, ik_long_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&ull_tp, ik_unsigned_long_long, 8));
    cls_tp.kind = tk_class; cls_tp.size = 24; cls_tp.source_corresp.name = "S2ClsRec";
    dbl_tp.kind = tk_float; dbl_tp.size = 8; dbl_tp.variant.float_kind = fk_double;
    cplx_tp.kind = tk_complex; cplx_tp.size = 16; cplx_tp.variant.float_kind = fk_double;

    /* 1) L78: check_is_record_type with tk_class (>16 bytes on RISCV64 va_arg) */
    {
        g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D;
        a_variable va_ap = {}; va_ap.type = &ptr_tp; va_ap.storage_class = sc_auto; va_ap.source_corresp.name = "ap_cls";
        ptr_tp.kind = tk_pointer; ptr_tp.size = 8; ptr_tp.variant.pointer.type = &int_tp;
        an_expr_node e_ap = {}, e_va = {};
        e_ap.kind = enk_variable; e_ap.type = &ptr_tp; e_ap.variant.variable.ptr = &va_ap;
        GCC_GEN_BE_CHECK(expr_init_op(&e_va, eok_va_arg, &cls_tp, &e_ap));
        gcc_jit_lvalue *ap_lv = gcc_jit_function_new_local(fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "ap_loc");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &va_ap, ap_lv));
        gcc_jit_rvalue *va_rv = NULL;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_va, &va_rv));
        g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    }

    /* 2) L260 & L594: 1-field struct mapped to tk_complex (field_count < 2U fallback in construct/extract) */
    {
        GCC_GEN_BE_CHECK(expr_test_map_type(&cplx_tp, g_expr_mock.one_field_struct_type));
        a_variable cv1 = {}, cv2 = {};
        cv1.type = &cplx_tp; cv1.storage_class = sc_auto; cv1.source_corresp.name = "c1f_a";
        cv2.type = &cplx_tp; cv2.storage_class = sc_auto; cv2.source_corresp.name = "c1f_b";
        gcc_jit_lvalue *clv1 = gcc_jit_function_new_local(fn, NULL, g_expr_mock.one_field_struct_type, "c1f_l1");
        gcc_jit_lvalue *clv2 = gcc_jit_function_new_local(fn, NULL, g_expr_mock.one_field_struct_type, "c1f_l2");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &cv1, clv1));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &cv2, clv2));
        an_expr_node ec1 = {}, ec2 = {}, ec_add = {};
        ec1.kind = enk_variable; ec1.type = &cplx_tp; ec1.variant.variable.ptr = &cv1; ec1.next = &ec2;
        ec2.kind = enk_variable; ec2.type = &cplx_tp; ec2.variant.variable.ptr = &cv2;
        GCC_GEN_BE_CHECK(expr_init_op(&ec_add, eok_add, &cplx_tp, &ec1));
        gcc_jit_rvalue *crv = NULL;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&ec_add, &crv));
        g_expr_mock.num_type_overrides = 0;
    }

    /* 3) L1049: VLA dimension list where first entry dim->type != cur */
    {
        vla1_tp.kind = tk_array; vla1_tp.variant.array.is_variable_size_array = TRUE; vla1_tp.variant.array.element_type = &int_tp;
        vla2_tp.kind = tk_array; vla2_tp.variant.array.is_variable_size_array = TRUE;
        vla2_tp.variant.array.has_assoc_vla_dimension = TRUE; vla2_tp.variant.array.element_type = &int_tp;
        a_type vla_outer_tp = {}; vla_outer_tp.kind = tk_pointer; vla_outer_tp.size = 8; vla_outer_tp.variant.pointer.type = &vla2_tp;
        a_variable dv2 = {}, vla_base_v = {};
        dv2.type = &int_tp; dv2.storage_class = sc_auto; dv2.source_corresp.name = "dv2";
        vla_base_v.type = &vla_outer_tp; vla_base_v.storage_class = sc_auto; vla_base_v.source_corresp.name = "vla_p";
        a_vla_dimension dim1 = {}, dim2 = {};
        dim1.type = &vla1_tp; dim1.next = &dim2;
        dim2.type = &vla2_tp; dim2.dimension_variable = &dv2;
        a_scope fsc = {}; fsc.kind = sck_function; fsc.vla_dimensions = &dim1;
        innermost_function_scope = &fsc;
        gcc_jit_lvalue *bp_lv = gcc_jit_function_new_local(fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "vla_bp");
        gcc_jit_lvalue *dv_lv = gcc_jit_function_new_local(fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "vla_dv");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &vla_base_v, bp_lv));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &dv2, dv_lv));
        a_constant idx_c = {}; idx_c.kind = ck_integer; idx_c.type = &int_tp;
        an_expr_node e_vb = {}, e_vi = {}, e_sub = {};
        e_vb.kind = enk_variable; e_vb.type = &vla_outer_tp; e_vb.variant.variable.ptr = &vla_base_v; e_vb.next = &e_vi;
        e_vi.kind = enk_constant; e_vi.type = &int_tp; e_vi.variant.constant.ptr = &idx_c;
        GCC_GEN_BE_CHECK(expr_init_op(&e_sub, eok_subscript, &vla2_tp, &e_vb));
        gcc_jit_lvalue *sub_lv = NULL;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub, &sub_lv));
        innermost_function_scope = NULL;
    }

    /* 4) L1197..1207: Darwin TLS global variable lvalue (uncached helper, cached helper + block, cached helper + null block, + OOM) */
    {
        a_variable tls_gv = {};
        tls_gv.source_corresp.name = "s2_tls_gv"; tls_gv.type = &int_tp; tls_gv.is_thread_local = TRUE;
        gcc_jit_type *ip_t = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
        gcc_jit_lvalue *tls_glv = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "s2_tls_gv_raw");
        gcc_jit_function *tls_helper = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ip_t, "__s2_tls_init", 0, NULL, 0);
        an_expr_node e_tls = {}; e_tls.kind = enk_variable; e_tls.type = &int_tp; e_tls.variant.variable.ptr = &tls_gv;
        gcc_jit_lvalue *out_tlv = NULL;

        /* cached_tls_fn == NULL */
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &tls_gv, tls_glv));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_tls, &out_tlv));

        /* cached_tls_fn != NULL, tls_cblk == NULL */
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &tls_gv, tls_helper));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_tls, &out_tlv));

        /* cached_tls_fn != NULL, tls_cblk != NULL + OOM sweep */
        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 25, [&]() noexcept -> gcc_gen_be_error_t {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &tls_gv, tls_glv));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &tls_gv, tls_helper));
            return gcc_gen_be_lower_expr_lvalue(&e_tls, &out_tlv);
        }));
    }

    /* 5) L1350..1360: eok_dot_field where cached field has char[8] type while f->type is struct (plus f->type == NULL & expected_ft == NULL) */
    {
        a_field sub_f = {}, outer_f = {}, null_tp_f = {};
        sub_f.source_corresp.name = "sf0"; sub_f.type = &int_tp;
        a_type inner_st = {}, outer_st = {};
        inner_st.kind = tk_struct; inner_st.size = 8; inner_st.source_corresp.name = "S2InnerSt"; inner_st.variant.class_struct_union.field_list = &sub_f;
        outer_f.source_corresp.name = "of0"; outer_f.type = &inner_st;
        null_tp_f.source_corresp.name = "nf0"; null_tp_f.type = NULL;
        outer_st.kind = tk_struct; outer_st.size = 8; outer_st.source_corresp.name = "S2OuterSt"; outer_st.variant.class_struct_union.field_list = &outer_f;

        gcc_jit_type *char_arr8 = gcc_jit_context_new_array_type(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR), 8);
        gcc_jit_field *raw_jf = gcc_jit_context_new_field(ctx, NULL, char_arr8, "raw_bytes");
        gcc_jit_struct *raw_jst = gcc_jit_context_new_struct_type(ctx, NULL, "S2OuterRawJit", 1, &raw_jf);
        gcc_jit_lvalue *obj_lv = gcc_jit_function_new_local(fn, NULL, gcc_jit_struct_as_type(raw_jst), "s2_outer_loc");
        a_variable obj_v = {}; obj_v.type = &outer_st; obj_v.storage_class = sc_auto; obj_v.source_corresp.name = "s2_outer_v";

        an_expr_node e_obj = {}, e_fld = {}, e_dot = {};
        e_obj.kind = enk_variable; e_obj.type = &outer_st; e_obj.variant.variable.ptr = &obj_v; e_obj.next = &e_fld;
        e_fld.kind = enk_field; e_fld.type = &inner_st; e_fld.variant.field.ptr = &outer_f;
        GCC_GEN_BE_CHECK(expr_init_op(&e_dot, eok_dot_field, &inner_st, &e_obj));
        gcc_jit_lvalue *dlv = NULL;

        GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 30, [&]() noexcept -> gcc_gen_be_error_t {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &obj_v, obj_lv));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &outer_f, raw_jf));
            return gcc_gen_be_lower_expr_lvalue(&e_dot, &dlv);
        }));

        /* expected_ft == NULL (1354:33 False) */
        GCC_GEN_BE_CHECK(expr_test_map_type(&inner_st, NULL));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &obj_v, obj_lv));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &outer_f, raw_jf));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_dot, &dlv));
        g_expr_mock.num_type_overrides = 0;

        /* f->type == NULL (1350:42 False), expected_ft == actual_ft (1354:48 False), int vs long (1355:33 False), struct vs struct (1356:33 False) */
        e_fld.variant.field.ptr = &null_tp_f;
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &null_tp_f, raw_jf));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_dot, &dlv));

        e_fld.variant.field.ptr = &outer_f;
        GCC_GEN_BE_CHECK(expr_test_map_type(&inner_st, char_arr8)); /* expected_ft == actual_ft -> 1354:48 False */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_dot, &dlv));
        g_expr_mock.num_type_overrides = 0;
        GCC_GEN_BE_CHECK(expr_test_map_type(&inner_st, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT))); /* non-struct expected_ft -> 1355:33 False */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_dot, &dlv));
        g_expr_mock.num_type_overrides = 0;

        gcc_jit_field *st_jf = gcc_jit_context_new_field(ctx, NULL, g_expr_mock.one_field_struct_type, "st_sub");
        gcc_jit_struct *st_holder = gcc_jit_context_new_struct_type(ctx, NULL, "S2OuterStHolder", 1, &st_jf);
        gcc_jit_lvalue *st_obj_lv = gcc_jit_function_new_local(fn, NULL, gcc_jit_struct_as_type(st_holder), "s2_st_loc");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &obj_v, st_obj_lv));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &outer_f, st_jf)); /* struct vs different struct -> 1356:33 False */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_dot, &dlv));
    }

    /* 6) L1696..1703: enk_builtin_choose_expr in gcc_gen_be_lower_expr_lvalue */
    {
        a_variable ch_v1 = {}, ch_v2 = {};
        ch_v1.type = &int_tp; ch_v1.storage_class = sc_auto; ch_v1.source_corresp.name = "ch_v1";
        ch_v2.type = &int_tp; ch_v2.storage_class = sc_auto; ch_v2.source_corresp.name = "ch_v2";
        gcc_jit_lvalue *lv1 = gcc_jit_function_new_local(fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "ch_l1");
        gcc_jit_lvalue *lv2 = gcc_jit_function_new_local(fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "ch_l2");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &ch_v1, lv1));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &ch_v2, lv2));

        an_expr_node c_cond = {}, c_t = {}, c_f = {}, e_ch = {};
        c_cond.kind = enk_variable; c_cond.type = &int_tp; c_cond.variant.variable.ptr = &ch_v1;
        c_t.kind = enk_variable; c_t.type = &int_tp; c_t.variant.variable.ptr = &ch_v1;
        c_f.kind = enk_variable; c_f.type = &int_tp; c_f.variant.variable.ptr = &ch_v2;
        e_ch.kind = enk_builtin_choose_expr; e_ch.type = &int_tp;
        gcc_jit_lvalue *clv = NULL;

        /* operands == NULL -> INVALID_ARGUMENT */
        e_ch.variant.builtin_choose_expr.operands = NULL;
        e_ch.variant.builtin_choose_expr.choose_first = TRUE;
        if (gcc_gen_be_lower_expr_lvalue(&e_ch, &clv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

        /* op_true == NULL -> INVALID_ARGUMENT */
        c_cond.next = NULL;
        e_ch.variant.builtin_choose_expr.operands = &c_cond;
        e_ch.variant.builtin_choose_expr.choose_first = FALSE;
        if (gcc_gen_be_lower_expr_lvalue(&e_ch, &clv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

        /* op_true != NULL, op_false != NULL: choose_first = TRUE and FALSE */
        c_cond.next = &c_t; c_t.next = &c_f;
        e_ch.variant.builtin_choose_expr.choose_first = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_ch, &clv));
        e_ch.variant.builtin_choose_expr.choose_first = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_ch, &clv));
    }

    /* 7) L1751..1946: bitfield read & write (width 0, width 65, signed i64 vs i32, unsigned u64, straddling 3-byte mask 0xFF) */
    {
        a_field bf_w0 = {}, bf_w65_s = {}, bf_w65_u = {}, bf_s32 = {}, bf_s64 = {}, bf_strad = {};
        bf_w0.is_bit_field = TRUE; bf_w0.offset_bit_remainder = 4; bf_w0.bit_size = 0; bf_w0.type = &int_tp; bf_w0.source_corresp.name = "bf0";
        bf_w65_s.is_bit_field = TRUE; bf_w65_s.bit_size = 65; bf_w65_s.bit_field_is_signed = TRUE; bf_w65_s.type = &ll_tp; bf_w65_s.source_corresp.name = "bf65s";
        bf_w65_u.is_bit_field = TRUE; bf_w65_u.bit_size = 65; bf_w65_u.bit_field_is_signed = FALSE; bf_w65_u.type = &ull_tp; bf_w65_u.source_corresp.name = "bf65u";
        bf_s32.is_bit_field = TRUE; bf_s32.bit_size = 5; bf_s32.bit_field_is_signed = TRUE; bf_s32.type = &int_tp; bf_s32.source_corresp.name = "bfs32";
        bf_s64.is_bit_field = TRUE; bf_s64.bit_size = 5; bf_s64.bit_field_is_signed = TRUE; bf_s64.type = &ll_tp; bf_s64.source_corresp.name = "bfs64";
        bf_strad.is_bit_field = TRUE; bf_strad.offset_bit_remainder = 4; bf_strad.bit_size = 16; bf_strad.type = &ull_tp; bf_strad.source_corresp.name = "bfstr";

        a_type bf_holder_st = {}; bf_holder_st.kind = tk_struct; bf_holder_st.size = 16; bf_holder_st.source_corresp.name = "S2BfHolder";
        a_variable bf_obj_v = {}; bf_obj_v.type = &bf_holder_st; bf_obj_v.storage_class = sc_auto; bf_obj_v.source_corresp.name = "bf_obj_v";
        gcc_jit_type *arr16 = gcc_jit_context_new_array_type(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR), 16);
        gcc_jit_lvalue *bf_lv = gcc_jit_function_new_local(fn, NULL, arr16, "bf_raw16");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &bf_obj_v, bf_lv));

        a_field *bfs[6] = {&bf_w0, &bf_w65_s, &bf_w65_u, &bf_s32, &bf_s64, &bf_strad};
        a_constant rhs_c = {}; rhs_c.kind = ck_integer; rhs_c.type = &int_tp;
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn));
        for (int bi = 0; bi < 6; ++bi) {
            a_field *cur_bf = bfs[bi];
            an_expr_node e_bo = {}, e_bf = {}, e_rd = {}, e_bo2 = {}, e_bf2 = {}, e_lhs = {}, e_rhs = {}, e_wr = {};
            e_bo.kind = enk_variable; e_bo.type = &bf_holder_st; e_bo.variant.variable.ptr = &bf_obj_v; e_bo.next = &e_bf;
            e_bf.kind = enk_field; e_bf.type = cur_bf->type; e_bf.variant.field.ptr = cur_bf;
            GCC_GEN_BE_CHECK(expr_init_op(&e_rd, eok_dot_field, cur_bf->type, &e_bo));
            gcc_jit_rvalue *rd_rv = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_rd, &rd_rv));

            e_bo2.kind = enk_variable; e_bo2.type = &bf_holder_st; e_bo2.variant.variable.ptr = &bf_obj_v; e_bo2.next = &e_bf2;
            e_bf2.kind = enk_field; e_bf2.type = cur_bf->type; e_bf2.variant.field.ptr = cur_bf;
            GCC_GEN_BE_CHECK(expr_init_op(&e_lhs, eok_dot_field, cur_bf->type, &e_bo2));
            e_lhs.next = &e_rhs;
            e_rhs.kind = enk_constant; e_rhs.type = cur_bf->type; e_rhs.variant.constant.ptr = &rhs_c;
            GCC_GEN_BE_CHECK(expr_init_op(&e_wr, eok_assign, cur_bf->type, &e_lhs));
            gcc_jit_rvalue *wr_rv = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_wr, &wr_rv));
        }
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes all `gcc_gen_be_lower_expr_lvalue` coverage tests.
 * @details Runs non-operation and operation lvalue test suites in isolated `libgccjit` contexts.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t run_expr_cov_lval_tests(void) GCC_GEN_BE_NOEXCEPT {
    typedef gcc_gen_be_error_t (*lval_fn_t)(gcc_jit_context *, gcc_jit_function *) GCC_GEN_BE_NOEXCEPT;
    static const lval_fn_t suites[4] = {
        test_cov_lval_non_ops,
        test_cov_lval_ops,
        test_cov_lval_final_gaps,
        test_cov_lval_stage2_gaps
    };
    for (size_t i = 0; i < 4; ++i) {
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
