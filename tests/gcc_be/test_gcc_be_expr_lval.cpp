/**
 * @file test_gcc_be_expr_lval.cpp
 * @brief Unit tests for dynamic initializers, lvalues, bitfields, and constant rvalues (`gcc_gen_be_expr.c`).
 * @details Exercises `gcc_gen_be_lower_dynamic_init`, `gcc_gen_be_lower_expr_lvalue`,
 * `gcc_gen_be_lower_bitfield_read`, `gcc_gen_be_lower_bitfield_assignment`, aggregate/scalar
 * constant lowering, and `gcc_gen_be_lower_float_constant` across success, null/error, and OOM paths.
 * @ingroup gcc_gen_be_tests
 */

#include "test_gcc_be_expr_common.h"
#include "gcc_gen_be_lib_loader.h"

USING_NAMESPACE_EDG

BEGIN_EDG_NAMESPACE

/**
 * @brief Tests `gcc_gen_be_lower_dynamic_init` across all `a_dynamic_init_kind` variants.
 * @details Covers `dik_none`, `dik_zero`, `dik_constant`, `dik_expression`, `dik_constructor`,
 * `dik_class_result_via_ctor`, `dik_copy_to_agg`, `dik_init_component_list`, `dik_array_elt_repeat`,
 * `dik_nonconstant_aggregate`, destructor cleanup registration, and OOM sweeps.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_dynamic_init_variants(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, arr_tp, cls_tp, rtp, void_tp, ptr_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_tp, &void_tp, FALSE));

    std::memset(&arr_tp, 0, sizeof(arr_tp));
    arr_tp.kind = tk_array;
    arr_tp.size = 8;
    arr_tp.alignment = 4;
    arr_tp.variant.array.element_type = &int_tp;
    arr_tp.variant.array.variant.number_of_elements = 2;

    a_field fld0;
    std::memset(&fld0, 0, sizeof(fld0));
    fld0.type = &int_tp;
    fld0.offset = 0;

    a_class_type_supplement cls_extra;
    std::memset(&cls_extra, 0, sizeof(cls_extra));
    a_base_class vbase;
    std::memset(&vbase, 0, sizeof(vbase));
    vbase.is_virtual = TRUE;
    vbase.type = &int_tp;
    cls_extra.base_classes = &vbase;

    std::memset(&cls_tp, 0, sizeof(cls_tp));
    cls_tp.kind = tk_class;
    cls_tp.size = 4;
    cls_tp.alignment = 4;
    cls_tp.variant.class_struct_union.any_virtual_base_classes = TRUE;
    cls_tp.variant.class_struct_union.field_list = &fld0;
    cls_tp.variant.class_struct_union.extra_info = &cls_extra;

    std::memset(&rtp, 0, sizeof(rtp));
    rtp.kind = tk_routine;
    rtp.variant.routine.return_type = &void_tp;

    gcc_jit_type *jit_int = NULL;
    gcc_jit_type *jit_arr = NULL;
    gcc_jit_type *jit_cls = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&int_tp, &jit_int));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&arr_tp, &jit_arr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&cls_tp, &jit_cls));

    gcc_jit_lvalue *lv_int = gcc_jit_function_new_local(fn, NULL, jit_int, "di_int");
    gcc_jit_lvalue *lv_arr = gcc_jit_function_new_local(fn, NULL, jit_arr, "di_arr");
    gcc_jit_lvalue *lv_cls = gcc_jit_function_new_local(fn, NULL, jit_cls, "di_cls");
    gcc_jit_rvalue *out_rv = NULL;

    /* Invalid argument & NULL context checks */
    if (gcc_gen_be_lower_dynamic_init(NULL, lv_int, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(NULL, lv_int, &out_rv));

    a_dynamic_init di;
    std::memset(&di, 0, sizeof(di));
    di.kind = dik_none;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_dynamic_init(&di, lv_int, &out_rv) != GCC_GEN_BE_ERROR_INVALID_STATE) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(&di, lv_int, &out_rv));

    /* dik_zero: with target_lval and without target_lval */
    a_variable var_int;
    std::memset(&var_int, 0, sizeof(var_int));
    var_int.type = &int_tp;
    di.kind = dik_zero;
    di.variable = &var_int;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 16, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(&di, lv_int, &out_rv));
        return gcc_gen_be_lower_dynamic_init(&di, NULL, &out_rv);
    }));

    /* dik_constant */
    a_constant c1;
    an_expr_node ce1;
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&ce1, &c1, &int_tp));
    di.kind = dik_constant;
    di.variant.constant.ptr = &c1;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 16, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(&di, lv_int, &out_rv));
        return gcc_gen_be_lower_dynamic_init(&di, NULL, &out_rv);
    }));

    /* dik_expression + dik_bitwise_copy */
    di.kind = dik_expression;
    di.variant.expression = &ce1;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 16, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(&di, lv_int, &out_rv));
        return gcc_gen_be_lower_dynamic_init(&di, NULL, &out_rv);
    }));
    di.kind = dik_bitwise_copy;
    di.variant.bitwise_copy.source = &ce1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(&di, lv_int, &out_rv));

    /* dik_constructor & dik_class_result_via_ctor with VTT + extra params + invoke + destructor */
    a_routine ctor_r, dtor_r;
    std::memset(&ctor_r, 0, sizeof(ctor_r));
    std::memset(&dtor_r, 0, sizeof(dtor_r));
    ctor_r.type = &rtp;
    ctor_r.source_corresp.name = "MockCtor";
    ctor_r.special_kind = (a_special_function_kind)sfk_constructor;
    dtor_r.type = &rtp;
    dtor_r.source_corresp.name = "MockDtor";

    gcc_jit_param *cp[4] = {
        gcc_jit_context_new_param(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "this_p"),
        gcc_jit_context_new_param(ctx, NULL, jit_int, "a0"),
        gcc_jit_context_new_param(ctx, NULL, gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR)), "vtt_p"),
        gcc_jit_context_new_param(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "extra_ptr")
    };
    gcc_jit_function *ctor_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED,
        gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "MockCtor3", 4, cp, 0);
    gcc_jit_param *dp0 = gcc_jit_context_new_param(
        ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "this_d");
    gcc_jit_function *dtor_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED,
        gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "MockDtor1", 1, &dp0, 0);

    a_variable var_cls;
    std::memset(&var_cls, 0, sizeof(var_cls));
    var_cls.type = &cls_tp;

    std::memset(&di, 0, sizeof(di));
    di.kind = dik_constructor;
    di.variable = &var_cls;
    di.destructor = &dtor_r;
    di.variant.constructor.ptr = &ctor_r;
    di.variant.constructor.args = &ce1;

    g_expr_mock.mock_decl_func = ctor_fn;
    g_expr_mock.mock_decl_func2 = dtor_fn;

    /* Without scope (reg_err == INTERNAL), then with scope + active LP */
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 48, [&]() noexcept -> gcc_gen_be_error_t {
        g_expr_mock.func_decl_calls = 0;
        return gcc_gen_be_lower_dynamic_init(&di, lv_cls, &out_rv);
    }));

    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
    gcc_jit_block *lp_blk = gcc_jit_function_new_block(fn, "ctor_lp");
    gcc_jit_block_end_with_void_return(lp_blk, NULL);
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(lp_blk));
    g_expr_mock.func_decl_calls = 0;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    di.variant.constructor.args = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(&di, NULL, &out_rv));
    di.variant.constructor.args = &ce1;
    /* Second call in same scope hits `already_has_lp == TRUE` */
    g_expr_mock.func_decl_calls = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(&di, lv_cls, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));

    /* VTT error propagation & VTT null fallback */
    g_expr_mock.force_vtt_err = GCC_GEN_BE_ERROR_VTT_GENERATION_FAILED;
    g_expr_mock.func_decl_calls = 0;
    if (gcc_gen_be_lower_dynamic_init(&di, lv_cls, &out_rv) != GCC_GEN_BE_ERROR_VTT_GENERATION_FAILED) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    g_expr_mock.force_vtt_err = GCC_GEN_BE_SUCCESS;
    g_expr_mock.vtt_return_null = TRUE;
    g_expr_mock.func_decl_calls = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(&di, lv_cls, &out_rv));
    g_expr_mock.vtt_return_null = FALSE;
    g_expr_mock.mock_decl_func = NULL;
    g_expr_mock.mock_decl_func2 = NULL;

    /* ck_aggregate with ck_dynamic_init components & ck_init_repeat */
    a_dynamic_init elem_di;
    std::memset(&elem_di, 0, sizeof(elem_di));
    elem_di.kind = dik_constant;
    elem_di.variant.constant.ptr = &c1;

    a_constant dyn_c1, dyn_c2, agg_dyn_con, rep_con;
    std::memset(&dyn_c1, 0, sizeof(dyn_c1));
    std::memset(&dyn_c2, 0, sizeof(dyn_c2));
    dyn_c1.kind = ck_dynamic_init;
    dyn_c1.type = &int_tp;
    dyn_c1.variant.dynamic_init.ptr = &elem_di;
    dyn_c1.next = &dyn_c2;
    dyn_c2.kind = ck_integer;
    dyn_c2.type = &int_tp;

    std::memset(&agg_dyn_con, 0, sizeof(agg_dyn_con));
    agg_dyn_con.kind = ck_aggregate;
    agg_dyn_con.type = &arr_tp;
    agg_dyn_con.variant.aggregate.has_dynamic_init_component = TRUE;
    agg_dyn_con.variant.aggregate.first_constant = &dyn_c1;

    a_variable var_arr;
    std::memset(&var_arr, 0, sizeof(var_arr));
    var_arr.type = &arr_tp;
    std::memset(&di, 0, sizeof(di));
    di.kind = dik_nonconstant_aggregate;
    di.variable = &var_arr;
    di.variant.constant.ptr = &agg_dyn_con;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 24, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_dynamic_init(&di, lv_arr, &out_rv);
    }));

    dyn_c1.next = NULL;
    agg_dyn_con.type = &cls_tp;
    di.variable = &var_cls;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 24, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_dynamic_init(&di, lv_cls, &out_rv);
    }));

    std::memset(&rep_con, 0, sizeof(rep_con));
    rep_con.kind = ck_init_repeat;
    rep_con.type = &arr_tp;
    rep_con.variant.init_repeat.count = 2;
    rep_con.variant.init_repeat.constant = &dyn_c1;
    di.variable = &var_arr;
    di.variant.constant.ptr = &rep_con;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 24, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_dynamic_init(&di, lv_arr, &out_rv);
    }));

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests `gcc_gen_be_lower_expr_lvalue` and bitfield read/write helpers.
 * @details Covers variables (local/global/coroutine/incomplete-array), temporaries, reusable
 * values, constants (string/address/aggregate), dereferences, subscripts (including VLA),
 * struct/union/byte-offset/bitfield member accesses, compound literals, and error paths.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_lvalues_and_bitfields(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, uint_tp, u64_tp, s8_tp, ptr_int_tp, arr_tp, inc_arr_tp, cls_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&uint_tp, ik_unsigned_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&u64_tp, ik_unsigned_long_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&s8_tp, ik_signed_char, 1));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_int_tp, &int_tp, FALSE));

    std::memset(&arr_tp, 0, sizeof(arr_tp));
    arr_tp.kind = tk_array;
    arr_tp.size = 16;
    arr_tp.alignment = 4;
    arr_tp.variant.array.element_type = &int_tp;
    arr_tp.variant.array.variant.number_of_elements = 4;

    std::memset(&inc_arr_tp, 0, sizeof(inc_arr_tp));
    inc_arr_tp.kind = tk_array;
    inc_arr_tp.size = 0;
    inc_arr_tp.alignment = 4;
    inc_arr_tp.variant.array.element_type = &int_tp;

    a_field fld_norm, fld_bf_s, fld_bf_u;
    std::memset(&fld_norm, 0, sizeof(fld_norm));
    fld_norm.type = &int_tp;
    fld_norm.offset = 0;

    std::memset(&fld_bf_s, 0, sizeof(fld_bf_s));
    fld_bf_s.type = &int_tp;
    fld_bf_s.is_bit_field = TRUE;
    fld_bf_s.bit_size = 5;
    fld_bf_s.offset = 4;
    fld_bf_s.offset_bit_remainder = 3;
    fld_norm.next = &fld_bf_s;

    std::memset(&fld_bf_u, 0, sizeof(fld_bf_u));
    fld_bf_u.type = &uint_tp;
    fld_bf_u.is_bit_field = TRUE;
    fld_bf_u.bit_size = 7;
    fld_bf_u.offset = 8;
    fld_bf_u.offset_bit_remainder = 0;
    fld_bf_s.next = &fld_bf_u;

    std::memset(&cls_tp, 0, sizeof(cls_tp));
    cls_tp.kind = tk_struct;
    cls_tp.size = 16;
    cls_tp.alignment = 4;
    cls_tp.variant.class_struct_union.field_list = &fld_norm;

    gcc_jit_type *jit_cls = NULL;
    gcc_jit_type *jit_int = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&cls_tp, &jit_cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&int_tp, &jit_int));

    a_variable v_bf_cls;
    std::memset(&v_bf_cls, 0, sizeof(v_bf_cls));
    v_bf_cls.type = &cls_tp;
    v_bf_cls.storage_class = (a_storage_class)sc_auto;
    v_bf_cls.source_corresp.name = "bf_obj";

    an_expr_node e_bf_obj, e_bfs_f, e_bfu_f, e_bfs_dot, e_bfu_dot, e_bfs_asg, e_bfu_asg, e_bf_rhs;
    a_constant c_bf_rhs;
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_bf_rhs, &c_bf_rhs, &int_tp));
    std::memset(&e_bfs_f, 0, sizeof(e_bfs_f));
    e_bfs_f.kind = enk_field;
    e_bfs_f.variant.field.ptr = &fld_bf_s;
    std::memset(&e_bfu_f, 0, sizeof(e_bfu_f));
    e_bfu_f.kind = enk_field;
    e_bfu_f.variant.field.ptr = &fld_bf_u;

    /* Bitfield read & assignment (signed/unsigned) via rvalue lowering */
    gcc_jit_rvalue *bf_rv = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 24, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(expr_init_var_expr(&e_bf_obj, &v_bf_cls, &cls_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_bfs_dot, eok_dot_field, &int_tp, &e_bf_obj, &e_bfs_f));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_bfs_dot, &bf_rv));
        GCC_GEN_BE_CHECK(expr_init_var_expr(&e_bf_obj, &v_bf_cls, &cls_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_bfu_dot, eok_dot_field, &uint_tp, &e_bf_obj, &e_bfu_f));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_bfu_dot, &bf_rv));
        GCC_GEN_BE_CHECK(expr_init_var_expr(&e_bf_obj, &v_bf_cls, &cls_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_bfs_dot, eok_dot_field, &int_tp, &e_bf_obj, &e_bfs_f));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_bfs_asg, eok_assign, &int_tp, &e_bfs_dot, &e_bf_rhs));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_bfs_asg, &bf_rv));
        GCC_GEN_BE_CHECK(expr_init_var_expr(&e_bf_obj, &v_bf_cls, &cls_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_bfu_dot, eok_dot_field, &uint_tp, &e_bf_obj, &e_bfu_f));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_bfu_asg, eok_add_assign, &uint_tp, &e_bfu_dot, &e_bf_rhs));
        return gcc_gen_be_lower_expr_rvalue(&e_bfu_asg, &bf_rv);
    }));

    /* Variable lvalues: local, global, incomplete extern array, coroutine frame spill */
    a_variable v_loc, v_glob, v_inc;
    std::memset(&v_loc, 0, sizeof(v_loc));
    v_loc.type = &int_tp;
    v_loc.storage_class = (a_storage_class)sc_auto;
    v_loc.source_corresp.name = "v_loc";

    std::memset(&v_glob, 0, sizeof(v_glob));
    v_glob.type = &int_tp;
    v_glob.storage_class = (a_storage_class)sc_extern;
    v_glob.source_corresp.name = "v_glob";

    std::memset(&v_inc, 0, sizeof(v_inc));
    v_inc.type = &inc_arr_tp;
    v_inc.storage_class = (a_storage_class)sc_extern;
    v_inc.source_corresp.name = "v_inc_arr";

    an_expr_node e_var;
    gcc_jit_lvalue *out_lv = NULL;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_var, &v_loc, &int_tp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_var, &out_lv));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_var, &v_glob, &int_tp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_var, &out_lv));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_var, &v_inc, &inc_arr_tp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_var, &out_lv));

    /* Coroutine frame field access on local variable */
    gcc_jit_field *cf_fld = gcc_jit_context_new_field(ctx, NULL, jit_int, "coro_v");
    gcc_jit_struct *coro_st = gcc_jit_context_new_struct_type(ctx, NULL, "_coro_fr", 1, &cf_fld);
    gcc_jit_type *coro_ft = gcc_jit_struct_as_type(coro_st);
    gcc_jit_lvalue *coro_ptr_lv = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_type_get_pointer(coro_ft), "coro_fr_p");
    a_variable v_coro;
    std::memset(&v_coro, 0, sizeof(v_coro));
    v_coro.type = &int_tp;
    v_coro.storage_class = (a_storage_class)sc_auto;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &v_coro, cf_fld));
    g_expr_mock.coro_frame_type = coro_ft;
    g_expr_mock.coro_frame_lval = coro_ptr_lv;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_var, &v_coro, &int_tp));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_var, &out_lv));
    g_expr_mock.coro_frame_type = NULL;
    g_expr_mock.coro_frame_lval = NULL;

    /* enk_temp_init & enk_reuse_value lvalues */
    a_dynamic_init dip;
    std::memset(&dip, 0, sizeof(dip));
    dip.kind = dik_zero;
    dip.variable = &v_loc;
    an_expr_node e_tmp;
    std::memset(&e_tmp, 0, sizeof(e_tmp));
    e_tmp.kind = enk_temp_init;
    e_tmp.type = &int_tp;
    e_tmp.variant.init.dynamic_init = &dip;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_tmp, &out_lv));

    e_tmp.kind = enk_reuse_value;
    e_tmp.variant.reused_value_init = &dip;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_tmp, &out_lv));

    /* enk_constant lvalues: string literal, address of variable, anonymous aggregate */
    a_type char_tp, char6_arr_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&char_tp, ik_char, 1));
    std::memset(&char6_arr_tp, 0, sizeof(char6_arr_tp));
    char6_arr_tp.kind = tk_array;
    char6_arr_tp.size = 6;
    char6_arr_tp.alignment = 1;
    char6_arr_tp.variant.array.element_type = &char_tp;
    char6_arr_tp.variant.array.variant.number_of_elements = 6;

    a_constant c_str;
    std::memset(&c_str, 0, sizeof(c_str));
    c_str.kind = ck_string;
    c_str.type = &char6_arr_tp;
    c_str.variant.string.value = "hello";
    c_str.variant.string.length = 6;
    an_expr_node e_cstr;
    std::memset(&e_cstr, 0, sizeof(e_cstr));
    e_cstr.kind = enk_constant;
    e_cstr.type = &char6_arr_tp;
    e_cstr.variant.constant.ptr = &c_str;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_cstr, &out_lv));

    /* Subscript, indirect, dot_field, points_to_field, ternary, comma, paren, assign lvalues */
    a_variable v_arr_loc, v_cls_loc, v_ptr_loc;
    std::memset(&v_arr_loc, 0, sizeof(v_arr_loc));
    v_arr_loc.type = &arr_tp;
    v_arr_loc.storage_class = (a_storage_class)sc_auto;
    v_arr_loc.source_corresp.name = "arr_l";

    std::memset(&v_cls_loc, 0, sizeof(v_cls_loc));
    v_cls_loc.type = &cls_tp;
    v_cls_loc.storage_class = (a_storage_class)sc_auto;
    v_cls_loc.source_corresp.name = "cls_l";

    std::memset(&v_ptr_loc, 0, sizeof(v_ptr_loc));
    v_ptr_loc.type = &ptr_int_tp;
    v_ptr_loc.storage_class = (a_storage_class)sc_auto;
    v_ptr_loc.source_corresp.name = "ptr_l";

    an_expr_node e_arr_v, e_cls_v, e_ptr_v, e_idx, e_sub, e_ind, e_dot, e_fld_node, e_q, e_cm;
    a_constant c_idx;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_arr_v, &v_arr_loc, &arr_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_cls_v, &v_cls_loc, &cls_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_ptr_v, &v_ptr_loc, &ptr_int_tp));
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_idx, &c_idx, &int_tp));

    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_sub, eok_subscript, &int_tp, &e_arr_v, &e_idx));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_sub, &out_lv));

    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_ind, eok_indirect, &int_tp, &e_ptr_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_ind, &out_lv));

    std::memset(&e_fld_node, 0, sizeof(e_fld_node));
    e_fld_node.kind = enk_field;
    e_fld_node.variant.field.ptr = &fld_norm;
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_dot, eok_dot_field, &int_tp, &e_cls_v, &e_fld_node));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_dot, &out_lv));

    /* Unmapped field byte-offset fallback in lvalue access */
    a_field fld_unmapped;
    std::memset(&fld_unmapped, 0, sizeof(fld_unmapped));
    fld_unmapped.type = &int_tp;
    fld_unmapped.offset = 4;
    e_fld_node.variant.field.ptr = &fld_unmapped;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_dot, &out_lv));

    /* Ternary & comma lvalue */
    an_expr_node e_qcond, e_cm1, e_ind2;
    a_constant c_qcond, c_cm1;
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_qcond, &c_qcond, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_cm1, &c_cm1, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_ind2, eok_indirect, &int_tp, &e_ptr_v));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_q, eok_question, &int_tp, &e_qcond, &e_ind, &e_sub));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_q, &out_lv));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_cm, eok_comma, &int_tp, &e_cm1, &e_ind2));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(&e_cm, &out_lv));

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests `gcc_gen_be_lower_constant_rvalue` and `gcc_gen_be_lower_float_constant`.
 * @details Covers 64-bit & 128-bit integers, narrow/wide strings, floats, complex, address constants
 * (`abk_variable`, `abk_routine`, `abk_constant`, `abk_label`), pointer-to-member constants, and
 * struct/union/array/bitfield aggregate constants.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_constants_and_aggregates(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    (void)ctx;
    a_type int_tp, uint_tp, i128_tp, flt_tp, cplx_tp, ptr_tp, arr_tp, char_tp, str_arr_tp, wstr_arr_tp;
    a_type pmd_tp, pmf_tp, rtp, void_tp, bf_st_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&uint_tp, ik_unsigned_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&i128_tp, ik_int128, 16));
    GCC_GEN_BE_CHECK(expr_init_int_type(&char_tp, ik_char, 1));
    GCC_GEN_BE_CHECK(expr_init_float_type(&flt_tp, fk_double, 8));
    GCC_GEN_BE_CHECK(expr_init_complex_type(&cplx_tp, fk_double, 16));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_tp, &int_tp, FALSE));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    std::memset(&rtp, 0, sizeof(rtp));
    rtp.kind = tk_routine;
    rtp.variant.routine.return_type = &void_tp;

    std::memset(&arr_tp, 0, sizeof(arr_tp));
    arr_tp.kind = tk_array;
    arr_tp.size = 8;
    arr_tp.alignment = 4;
    arr_tp.variant.array.element_type = &int_tp;
    arr_tp.variant.array.variant.number_of_elements = 2;

    std::memset(&str_arr_tp, 0, sizeof(str_arr_tp));
    str_arr_tp.kind = tk_array;
    str_arr_tp.size = 4;
    str_arr_tp.alignment = 1;
    str_arr_tp.variant.array.element_type = &char_tp;
    str_arr_tp.variant.array.variant.number_of_elements = 4;

    std::memset(&wstr_arr_tp, 0, sizeof(wstr_arr_tp));
    wstr_arr_tp.kind = tk_array;
    wstr_arr_tp.size = 8;
    wstr_arr_tp.alignment = 4;
    wstr_arr_tp.variant.array.element_type = &int_tp;
    wstr_arr_tp.variant.array.variant.number_of_elements = 2;

    gcc_jit_rvalue *rv = NULL;

    /* 64-bit signed/unsigned + overflow + 128-bit integer constants */
    a_constant con;
    std::memset(&con, 0, sizeof(con));
    con.kind = ck_integer;
    con.type = &int_tp;
    g_expr_mock.signed_int_val = -77;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));
    g_expr_mock.signed_int_ovf = TRUE;
    g_expr_mock.unsigned_int_val = 0x80000001ULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));
    g_expr_mock.signed_int_ovf = FALSE;
    con.type = &uint_tp;
    g_expr_mock.int_is_signed = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));
    g_expr_mock.int_is_signed = TRUE;

    con.type = &i128_tp;
    g_expr_mock.int128_lo = 0x1122334455667788ULL;
    g_expr_mock.int128_hi = 0x99aabbccddeeff00ULL;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 16, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv);
    }));

    /* Float & complex constants */
    std::memset(&con, 0, sizeof(con));
    con.kind = ck_float;
    con.type = &flt_tp;
    g_expr_mock.host_fp_val = 3.25;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_float_constant(&con, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));

    std::memset(&con, 0, sizeof(con));
    con.kind = ck_complex;
    con.type = &cplx_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));
    g_expr_mock.complex_as_struct = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));
    g_expr_mock.complex_as_struct = FALSE;

    /* String constants (pointer decay, char array, wide char array) */
    const int wchars[2] = { 65, 0 };
    std::memset(&con, 0, sizeof(con));
    con.kind = ck_string;
    con.type = &ptr_tp;
    con.variant.string.value = "abc";
    con.variant.string.length = 4;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));
    con.type = &str_arr_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));
    con.type = &wstr_arr_tp;
    con.variant.string.value = (const char *)wchars;
    con.variant.string.length = sizeof(wchars);
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));

    /* Address constants: variable + offset, routine, backing constant */
    a_variable gv;
    std::memset(&gv, 0, sizeof(gv));
    gv.type = &int_tp;
    gv.storage_class = (a_storage_class)sc_extern;
    gv.source_corresp.name = "addr_glob";

    std::memset(&con, 0, sizeof(con));
    con.kind = ck_address;
    con.type = &ptr_tp;
    con.variant.address.kind = (an_address_base_kind)abk_variable;
    con.variant.address.variant.variable = &gv;
    con.variant.address.offset = 8;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));

    a_routine rfn;
    std::memset(&rfn, 0, sizeof(rfn));
    rfn.type = &rtp;
    rfn.source_corresp.name = "addr_fn";
    con.variant.address.kind = (an_address_base_kind)abk_routine;
    con.variant.address.variant.routine = &rfn;
    con.variant.address.offset = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));

    /* Pointer-to-data-member & pointer-to-member-function constants */
    a_field pmd_fld;
    std::memset(&pmd_fld, 0, sizeof(pmd_fld));
    pmd_fld.type = &int_tp;
    pmd_fld.offset = 12;

    std::memset(&pmd_tp, 0, sizeof(pmd_tp));
    pmd_tp.kind = tk_ptr_to_member;
    pmd_tp.size = 8;
    pmd_tp.variant.ptr_to_member.type = &int_tp;

    a_base_class pm_base;
    std::memset(&pm_base, 0, sizeof(pm_base));
    pm_base.offset = 8;

    std::memset(&con, 0, sizeof(con));
    con.kind = ck_ptr_to_member;
    con.type = &pmd_tp;
    con.variant.ptr_to_member.variant.field = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));
    con.variant.ptr_to_member.variant.field = &pmd_fld;
    con.variant.ptr_to_member.casting_base_class = &pm_base;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));

    std::memset(&pmf_tp, 0, sizeof(pmf_tp));
    pmf_tp.kind = tk_ptr_to_member;
    pmf_tp.size = 16;
    pmf_tp.variant.ptr_to_member.type = &rtp;
    con.type = &pmf_tp;
    con.variant.ptr_to_member.is_function_ptr = TRUE;
    con.variant.ptr_to_member.variant.routine = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));
    con.variant.ptr_to_member.variant.routine = &rfn;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));
    rfn.is_virtual = TRUE;
    rfn.number.virtual_function = 2;
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(&con, NULL, &rv));

    /* Struct aggregate constant with bitfields + array aggregate constant */
    a_field bf1, bf2;
    std::memset(&bf1, 0, sizeof(bf1));
    std::memset(&bf2, 0, sizeof(bf2));
    bf1.type = &uint_tp;
    bf1.is_bit_field = TRUE;
    bf1.bit_size = 3;
    bf1.offset = 0;
    bf1.offset_bit_remainder = 0;
    bf1.next = &bf2;
    bf2.type = &uint_tp;
    bf2.is_bit_field = TRUE;
    bf2.bit_size = 5;
    bf2.offset = 0;
    bf2.offset_bit_remainder = 3;

    std::memset(&bf_st_tp, 0, sizeof(bf_st_tp));
    bf_st_tp.kind = tk_struct;
    bf_st_tp.size = 4;
    bf_st_tp.alignment = 4;
    bf_st_tp.variant.class_struct_union.field_list = &bf1;

    gcc_jit_field *storage_jf = gcc_jit_context_new_field(
        ctx,
        NULL,
        gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T),
        "_bf_word");
    gcc_jit_type *bf_jit_st = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_bf_agg_st", 1, &storage_jf));
    GCC_GEN_BE_CHECK(expr_test_map_type(&bf_st_tp, bf_jit_st));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf1, storage_jf));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &bf2, storage_jf));

    a_constant sub_c1, sub_c2, agg_con;
    std::memset(&sub_c1, 0, sizeof(sub_c1));
    std::memset(&sub_c2, 0, sizeof(sub_c2));
    sub_c1.kind = ck_integer;
    sub_c1.type = &uint_tp;
    sub_c1.next = &sub_c2;
    sub_c2.kind = ck_integer;
    sub_c2.type = &uint_tp;
    GCC_GEN_BE_CHECK(expr_test_map_const_int(&sub_c1, 5));
    GCC_GEN_BE_CHECK(expr_test_map_const_int(&sub_c2, 11));

    std::memset(&agg_con, 0, sizeof(agg_con));
    agg_con.kind = ck_aggregate;
    agg_con.type = &bf_st_tp;
    agg_con.variant.aggregate.first_constant = &sub_c1;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 24, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_constant_rvalue(&agg_con, bf_jit_st, &rv);
    }));

    agg_con.type = &arr_tp;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 24, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_constant_rvalue(&agg_con, NULL, &rv);
    }));

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t run_expr_lval_and_init_tests(void) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    gcc_jit_function *fn = NULL;
    gcc_jit_block *blk = NULL;
    GCC_GEN_BE_CHECK(expr_test_setup_fn(&ctx, &fn, &blk));

    gcc_gen_be_error_t err = test_dynamic_init_variants(ctx, fn);
    if (err != GCC_GEN_BE_SUCCESS) std::fprintf(stderr, "lval step 1 failed: %d\n", (int)err);
    if (err == GCC_GEN_BE_SUCCESS) {
        GCC_GEN_BE_CHECK(expr_test_reset_mock());
        err = test_lvalues_and_bitfields(ctx, fn);
        if (err != GCC_GEN_BE_SUCCESS) std::fprintf(stderr, "lval step 2 failed: %d\n", (int)err);
    }
    if (err == GCC_GEN_BE_SUCCESS) {
        GCC_GEN_BE_CHECK(expr_test_reset_mock());
        err = test_constants_and_aggregates(ctx, fn);
        if (err != GCC_GEN_BE_SUCCESS) std::fprintf(stderr, "lval step 3 failed: %d\n", (int)err);
    }

    GCC_GEN_BE_CHECK(expr_test_teardown_fn(ctx));
    return err;
}

END_EDG_NAMESPACE
