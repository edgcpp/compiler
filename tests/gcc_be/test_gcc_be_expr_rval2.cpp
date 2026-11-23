/**
 * @file test_gcc_be_expr_rval2.cpp
 * @brief Unit tests for Part 2 of `gcc_gen_be_lower_expr_rvalue` and vector/coro helpers (`gcc_gen_be_expr.c`).
 * @details Exercises direct/indirect/member/virtual/PMF/builtin/invoke calls, `va_arg`/`va_start`/
 * `va_copy`/`va_end` across SysV AMD64 / AAPCS64 / Win64 / RISCV64 / i386, scalar/aggregate/base/PM
 * casts, braced-init lists, `typeid`, `condition`, `new`/`delete`, overriding-function wrappers,
 * `co_await`/`co_yield`, `reuse_value`, `sizeof`/`alignof` (including VLAs), `throw`, lowered EH
 * constructs, `gcc_gen_be_lower_vector_shuffle`, and `gcc_gen_be_lower_convertvector`.
 * @ingroup gcc_gen_be_tests
 */

#include "test_gcc_be_expr_common.h"
#include "gcc_gen_be_lib_loader.h"

USING_NAMESPACE_EDG

BEGIN_EDG_NAMESPACE

/**
 * @brief Tests calls, variadic macros (`va_*`), casts, and braced initializer lists.
 * @details Covers `eok_call`, `eok_dot_pm_call`, virtual calls, invoke calls, `va_arg` across all
 * 5 target ABIs, `eok_bool_cast`, `eok_pm_base_class_cast`, `eok_pm_derived_class_cast`,
 * `eok_array_to_pointer`, `eok_cast` / `eok_base_class_cast` / `eok_dynamic_cast`, and `enk_braced_init_list`.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_calls_va_and_casts(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, dbl_tp, bool_tp, void_tp, ptr_tp, void_ptr_tp, rtp, pmf_tp, cls_tp, arr_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_float_type(&dbl_tp, fk_double, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&bool_tp, ik_int, 1));
    bool_tp.variant.integer.bool_type = TRUE;
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&ptr_tp, &int_tp, FALSE));
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&void_ptr_tp, &void_tp, FALSE));

    a_field f0;
    std::memset(&f0, 0, sizeof(f0));
    f0.type = &int_tp;
    f0.offset = 0;
    std::memset(&cls_tp, 0, sizeof(cls_tp));
    cls_tp.kind = tk_struct;
    cls_tp.size = 16;
    cls_tp.alignment = 8;
    cls_tp.variant.class_struct_union.field_list = &f0;

    std::memset(&arr_tp, 0, sizeof(arr_tp));
    arr_tp.kind = tk_array;
    arr_tp.size = 8;
    arr_tp.alignment = 4;
    arr_tp.variant.array.element_type = &int_tp;
    arr_tp.variant.array.variant.number_of_elements = 2;

    a_param_type p_int;
    std::memset(&p_int, 0, sizeof(p_int));
    p_int.type = &int_tp;
    a_routine_type_supplement rtsp;
    std::memset(&rtsp, 0, sizeof(rtsp));
    rtsp.param_type_list = &p_int;
    std::memset(&rtp, 0, sizeof(rtp));
    rtp.kind = tk_routine;
    rtp.variant.routine.return_type = &int_tp;
    rtp.variant.routine.extra_info = &rtsp;

    std::memset(&pmf_tp, 0, sizeof(pmf_tp));
    pmf_tp.kind = tk_ptr_to_member;
    pmf_tp.size = 16;
    pmf_tp.variant.ptr_to_member.type = &rtp;

    a_routine callee_r;
    std::memset(&callee_r, 0, sizeof(callee_r));
    callee_r.type = &rtp;
    callee_r.source_corresp.name = "callee_fn";

    an_expr_node e_rout, e_arg, e_call;
    a_constant c_arg;
    std::memset(&e_rout, 0, sizeof(e_rout));
    e_rout.kind = enk_routine;
    e_rout.type = &rtp;
    e_rout.variant.routine.ptr = &callee_r;
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_arg, &c_arg, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_call, eok_call, &int_tp, &e_rout, &e_arg));

    gcc_jit_rvalue *rv = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 32, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_call, &rv);
    }));

    /* Invoke call with active landing pad */
    gcc_jit_block *lp_blk = gcc_jit_function_new_block(fn, "call_lp");
    gcc_jit_block_end_with_void_return(lp_blk, NULL);
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(lp_blk));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_call, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());

    /* PMF call (`eok_dot_pm_call`) on SysV AMD64 & AAPCS64 */
    a_variable v_cls, v_pmf;
    std::memset(&v_cls, 0, sizeof(v_cls));
    v_cls.type = &cls_tp;
    v_cls.storage_class = (a_storage_class)sc_auto;
    v_cls.source_corresp.name = "pm_obj";

    std::memset(&v_pmf, 0, sizeof(v_pmf));
    v_pmf.type = &pmf_tp;
    v_pmf.storage_class = (a_storage_class)sc_auto;
    v_pmf.source_corresp.name = "pm_fn";

    an_expr_node e_obj, e_pmf, e_pm_call;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_obj, &v_cls, &cls_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_pmf, &v_pmf, &pmf_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_pm_call, eok_dot_pm_call, &int_tp, &e_obj, &e_pmf));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 32, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_pm_call, &rv);
    }));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_pm_call, &rv));
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;

    /* `va_arg`, `va_start`, `va_copy`, `va_end` across SysV AMD64, Win64, RISCV64, i386 */
    gcc_jit_field *sysv_f[4] = {
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT), "gp_offset"),
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT), "fp_offset"),
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "overflow_arg_area"),
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "reg_save_area")
    };
    gcc_jit_type *sysv_va_t = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_sysv_va", 4, sysv_f));

    a_type va_edg_tp;
    std::memset(&va_edg_tp, 0, sizeof(va_edg_tp));
    va_edg_tp.kind = tk_struct;
    va_edg_tp.size = 24;
    GCC_GEN_BE_CHECK(expr_test_map_type(&va_edg_tp, sysv_va_t));

    a_variable v_ap, v_ap2;
    std::memset(&v_ap, 0, sizeof(v_ap));
    v_ap.type = &va_edg_tp;
    v_ap.storage_class = (a_storage_class)sc_auto;
    v_ap.source_corresp.name = "ap1";
    std::memset(&v_ap2, 0, sizeof(v_ap2));
    v_ap2.type = &va_edg_tp;
    v_ap2.storage_class = (a_storage_class)sc_auto;
    v_ap2.source_corresp.name = "ap2";

    an_expr_node e_ap, e_ap2, e_va;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_ap, &v_ap, &va_edg_tp));
    g_expr_mock.va_is_stack = FALSE;
    g_expr_mock.va_num_gp = 1;
    g_expr_mock.va_num_fp = 0;
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_va, eok_va_arg, &int_tp, &e_ap));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_va, &rv));

    g_expr_mock.va_num_gp = 1;
    g_expr_mock.va_num_fp = 1;
    g_expr_mock.sysv_class0 = GCC_GEN_BE_ABI_CLASS_INTEGER;
    g_expr_mock.sysv_class1 = GCC_GEN_BE_ABI_CLASS_SSE;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_ap, &v_ap, &va_edg_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_va, eok_va_arg, &cls_tp, &e_ap));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_va, &rv));

    /* Win64 / RISCV64 / i386 pointer-based va_list */
    a_variable v_ap_ptr;
    std::memset(&v_ap_ptr, 0, sizeof(v_ap_ptr));
    v_ap_ptr.type = &void_ptr_tp;
    v_ap_ptr.storage_class = (a_storage_class)sc_auto;
    v_ap_ptr.source_corresp.name = "ap_ptr";

    const gcc_gen_be_target_abi_t ptr_abis[3] = {
        GCC_GEN_BE_TARGET_ABI_WIN64,
        GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D,
        GCC_GEN_BE_TARGET_ABI_SYSV_I386
    };
    for (int i = 0; i < 3; ++i) {
        g_expr_mock.target_abi = ptr_abis[i];
        GCC_GEN_BE_CHECK(expr_init_var_expr(&e_ap, &v_ap_ptr, &void_ptr_tp));
        GCC_GEN_BE_CHECK(expr_init_op_expr(&e_va, eok_va_arg, &int_tp, &e_ap));
        GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_va, &rv));
    }
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;

    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_ap, &v_ap, &va_edg_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_va, eok_va_start, &void_tp, &e_ap));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_va, &rv));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_ap, &v_ap, &va_edg_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_ap2, &v_ap2, &va_edg_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_va, eok_va_copy, &void_tp, &e_ap, &e_ap2));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_va, &rv));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_ap, &v_ap, &va_edg_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_va, eok_va_end, &void_tp, &e_ap));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_va, &rv));

    /* `eok_bool_cast`, `eok_cast`, `eok_dynamic_cast`, `eok_array_to_pointer`, `enk_braced_init_list` */
    an_expr_node e_cast;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_pmf, &v_pmf, &pmf_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_cast, eok_bool_cast, &bool_tp, &e_pmf));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast, &rv));

    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_arg, &c_arg, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_cast, eok_cast, &dbl_tp, &e_arg));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast, &rv));

    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_cast, eok_dynamic_cast, &void_ptr_tp, &e_arg));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast, &rv));

    a_variable v_arr;
    std::memset(&v_arr, 0, sizeof(v_arr));
    v_arr.type = &arr_tp;
    v_arr.storage_class = (a_storage_class)sc_auto;
    v_arr.source_corresp.name = "arr_v2";
    an_expr_node e_arr;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_arr, &v_arr, &arr_tp));
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_cast, eok_array_to_pointer, &ptr_tp, &e_arr));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_cast, &rv));

    an_expr_node e_braced, e_b1;
    a_constant cb1;
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_b1, &cb1, &int_tp));
    std::memset(&e_braced, 0, sizeof(e_braced));
    e_braced.kind = enk_braced_init_list;
    e_braced.type = &arr_tp;
    e_braced.variant.braced_init_list = &e_b1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_braced, &rv));
    e_braced.type = &cls_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_braced, &rv));

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests remaining `enk_*` rvalue nodes, coroutines, lowered EH, and vector shuffle/convert.
 * @details Covers `enk_typeid`, `enk_routine`, `enk_condition`, `enk_new_delete`,
 * `enk_result_of_overriding_function`, `enk_await`/`enk_yield`, `enk_object_lifetime`,
 * `enk_reuse_value`, `enk_sizeof`/`enk_alignof`/`enk_datasizeof`, `enk_type_operand`,
 * `enk_throw`, `enk_vla_dealloc`, `enk_lowered_eh_construct`, `gcc_gen_be_lower_vector_shuffle`,
 * and `gcc_gen_be_lower_convertvector`.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_remaining_nodes_and_vectors(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, size_tp, bool_tp, void_tp, void_ptr_tp, vec_tp, fvec_tp, flt_tp, cls_tp, rtp, arr_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&size_tp, ik_unsigned_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&bool_tp, ik_int, 1));
    bool_tp.variant.integer.bool_type = TRUE;
    GCC_GEN_BE_CHECK(expr_init_float_type(&flt_tp, fk_float, 4));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&void_ptr_tp, &void_tp, FALSE));

    std::memset(&vec_tp, 0, sizeof(vec_tp));
    vec_tp.kind = tk_vector;
    vec_tp.size = 16;
    vec_tp.alignment = 16;
    vec_tp.variant.vector.element_type = &int_tp;

    std::memset(&fvec_tp, 0, sizeof(fvec_tp));
    fvec_tp.kind = tk_vector;
    fvec_tp.size = 16;
    fvec_tp.alignment = 16;
    fvec_tp.variant.vector.element_type = &flt_tp;

    a_field f0;
    std::memset(&f0, 0, sizeof(f0));
    f0.type = &int_tp;
    std::memset(&cls_tp, 0, sizeof(cls_tp));
    cls_tp.kind = tk_class;
    cls_tp.size = 4;
    cls_tp.alignment = 4;
    cls_tp.variant.class_struct_union.field_list = &f0;

    std::memset(&rtp, 0, sizeof(rtp));
    rtp.kind = tk_routine;
    rtp.variant.routine.return_type = &void_tp;

    std::memset(&arr_tp, 0, sizeof(arr_tp));
    arr_tp.kind = tk_array;
    arr_tp.size = 16;
    arr_tp.alignment = 4;
    arr_tp.variant.array.element_type = &int_tp;
    arr_tp.variant.array.variant.number_of_elements = 4;

    gcc_jit_rvalue *rv = NULL;

    /* `enk_typeid`, `enk_routine`, `enk_condition`, `enk_type_operand`, `enk_object_lifetime` */
    an_expr_node e_node, e_c1;
    a_constant c1;
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_c1, &c1, &int_tp));
    GCC_GEN_BE_CHECK(expr_test_map_const_int(&c1, 4));

    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_typeid;
    e_node.type = &void_ptr_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));

    a_routine rout1;
    std::memset(&rout1, 0, sizeof(rout1));
    rout1.type = &rtp;
    rout1.source_corresp.name = "r_sym";
    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_routine;
    e_node.type = &void_ptr_tp;
    e_node.variant.routine.ptr = &rout1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));

    a_dynamic_init cond_di;
    std::memset(&cond_di, 0, sizeof(cond_di));
    cond_di.kind = dik_expression;
    cond_di.variant.expression = &e_c1;
    a_condition_supplement cond_sup;
    std::memset(&cond_sup, 0, sizeof(cond_sup));
    cond_sup.dynamic_init = &cond_di;
    cond_sup.expr = &e_c1;
    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_condition;
    e_node.type = &bool_tp;
    e_node.variant.condition = &cond_sup;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));

    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_type_operand;
    e_node.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));

    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_object_lifetime;
    e_node.type = &int_tp;
    e_node.variant.object_lifetime.expr = &e_c1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));

    /* `enk_new_delete` (new array with cookie/ctor + delete array with cookie/dtor) */
    a_routine op_new_r, op_del_r, dtor_r;
    std::memset(&op_new_r, 0, sizeof(op_new_r));
    std::memset(&op_del_r, 0, sizeof(op_del_r));
    std::memset(&dtor_r, 0, sizeof(dtor_r));
    op_new_r.source_corresp.name = "mock_op_new";
    op_del_r.source_corresp.name = "mock_op_del";
    dtor_r.source_corresp.name = "mock_arr_dtor";

    gcc_jit_param *np0 = gcc_jit_context_new_param(
        ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T), "sz");
    gcc_jit_function *new_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED,
        gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "mock_op_new", 1, &np0, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &op_new_r, new_fn));

    gcc_jit_param *dp0 = gcc_jit_context_new_param(
        ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "p");
    gcc_jit_function *del_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED,
        gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "mock_op_del", 1, &dp0, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &op_del_r, del_fn));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &dtor_r, del_fn));

    cond_di.destructor = &dtor_r;
    a_new_delete_supplement nds;
    std::memset(&nds, 0, sizeof(nds));
    nds.is_new = TRUE;
    nds.array_delete = TRUE;
    nds.type = &cls_tp;
    nds.routine = &op_new_r;
    nds.arg = &e_c1;
    nds.number_of_elements = &e_c1;
    nds.dynamic_init = &cond_di;

    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_new_delete;
    e_node.type = &void_ptr_tp;
    e_node.variant.new_delete = &nds;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 32, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_node, &rv);
    }));

    a_variable v_del_p;
    std::memset(&v_del_p, 0, sizeof(v_del_p));
    v_del_p.type = &void_ptr_tp;
    v_del_p.storage_class = (a_storage_class)sc_auto;
    v_del_p.source_corresp.name = "del_p";
    an_expr_node e_del_p;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_del_p, &v_del_p, &void_ptr_tp));
    nds.is_new = FALSE;
    nds.routine = &op_del_r;
    nds.arg = &e_del_p;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 32, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_node, &rv);
    }));
    cond_di.destructor = NULL;

    /* `enk_result_of_overriding_function` */
    a_routine wrap_r, over_r;
    std::memset(&wrap_r, 0, sizeof(wrap_r));
    std::memset(&over_r, 0, sizeof(over_r));
    over_r.type = &rtp;
    over_r.source_corresp.name = "over_fn";
    wrap_r.overriding_function_for_wrapper = &over_r;
    a_scope fn_sc;
    std::memset(&fn_sc, 0, sizeof(fn_sc));
    fn_sc.variant.routine.ptr = &wrap_r;
    innermost_function_scope = &fn_sc;
    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_result_of_overriding_function;
    e_node.type = &void_tp;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 24, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_node, &rv);
    }));
    innermost_function_scope = NULL;

    /* `enk_await` / `enk_yield` / `gcc_gen_be_lower_await_yield` (void, bool, symmetric transfer) */
    an_expr_node e_rdy, e_res, e_susp;
    a_constant cr, cs;
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_rdy, &cr, &bool_tp));
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_res, &cs, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_susp, &v_del_p, &void_ptr_tp));
    e_rdy.next = &e_res;
    e_res.next = &e_susp;

    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_await;
    e_node.type = &int_tp;
    e_node.variant.await_info.operand = &e_c1;
    e_node.variant.await_info.ready_resume_suspend = &e_rdy;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 32, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_rvalue(&e_node, &rv);
    }));
    e_node.kind = enk_yield;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));

    /* `enk_reuse_value`, `enk_sizeof`, `enk_alignof`, `enk_throw`, `enk_vla_dealloc`, `enk_lowered_eh_construct` */
    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_reuse_value;
    e_node.type = &int_tp;
    e_node.variant.reused_value_init = &cond_di;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));

    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_alignof;
    e_node.type = &size_tp;
    e_node.variant.sizeof_info.is_type = TRUE;
    e_node.variant.sizeof_info.variant.type = &arr_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));
    e_node.kind = enk_sizeof;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));
    e_node.kind = enk_datasizeof;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));

    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_throw;
    e_node.type = &void_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));

#if VLA_DEALLOCATIONS_IN_IL
    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_vla_dealloc;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));
#endif

#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_lowered_eh_construct;
    e_node.type = &void_tp;
    e_node.variant.lowered_eh.kind = (a_lowered_eh_construct_kind)leck_internal_try;
    e_c1.next = &e_res;
    e_res.next = NULL;
    e_node.variant.lowered_eh.variant.try_and_catch_expr = &e_c1;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));
    e_c1.next = NULL;

    e_node.type = &void_ptr_tp;
    e_node.variant.lowered_eh.kind = (a_lowered_eh_construct_kind)leck_thrown_object_address;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));
#endif

    /* Unhandled expression node diagnostic */
    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = (an_expr_node_kind)250;
    if (gcc_gen_be_lower_expr_rvalue(&e_node, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }

    /* `gcc_gen_be_lower_vector_shuffle` & `gcc_gen_be_lower_convertvector` (native + scalar fallback) */
    a_variable vv1, vv2, vvm;
    std::memset(&vv1, 0, sizeof(vv1));
    vv1.type = &vec_tp;
    vv1.storage_class = (a_storage_class)sc_auto;
    vv1.source_corresp.name = "sv1";
    std::memset(&vv2, 0, sizeof(vv2));
    vv2.type = &vec_tp;
    vv2.storage_class = (a_storage_class)sc_auto;
    vv2.source_corresp.name = "sv2";
    std::memset(&vvm, 0, sizeof(vvm));
    vvm.type = &vec_tp;
    vvm.storage_class = (a_storage_class)sc_auto;
    vvm.source_corresp.name = "svm";

    an_expr_node ev1, ev2, evm, em0, em1;
    a_constant cm0, cm1;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ev1, &vv1, &vec_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ev2, &vv2, &vec_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&evm, &vvm, &vec_tp));
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&em0, &cm0, &int_tp));
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&em1, &cm1, &int_tp));
    GCC_GEN_BE_CHECK(expr_test_map_const_int(&cm0, -1));
    GCC_GEN_BE_CHECK(expr_test_map_const_int(&cm1, 5));

    ev1.next = &ev2;
    ev2.next = &evm;
    evm.next = NULL;
    std::memset(&e_node, 0, sizeof(e_node));
    e_node.kind = enk_builtin_operation;
    e_node.type = &vec_tp;
    e_node.variant.builtin_operation.kind = (a_builtin_operation_kind)bok_builtin_shuffle;
    e_node.variant.builtin_operation.operands = &ev1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));

    pfn_gcc_jit_context_new_rvalue_vector_perm saved_perm = p_gcc_jit_context_new_rvalue_vector_perm;
    p_gcc_jit_context_new_rvalue_vector_perm = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 32, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_vector_shuffle(&e_node, &rv);
    }));
    p_gcc_jit_context_new_rvalue_vector_perm = saved_perm;

    ev1.next = &ev2;
    ev2.next = &em0;
    em0.next = &em1;
    em1.next = NULL;
    e_node.variant.builtin_operation.kind = (a_builtin_operation_kind)bok_builtin_shufflevector;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));

    an_expr_node etp_op;
    std::memset(&etp_op, 0, sizeof(etp_op));
    etp_op.kind = enk_type_operand;
    etp_op.next = &ev1;
    ev1.next = NULL;
    e_node.type = &fvec_tp;
    e_node.variant.builtin_operation.kind = (a_builtin_operation_kind)bok_builtin_convertvector;
    e_node.variant.builtin_operation.operands = &etp_op;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_node, &rv));

    pfn_gcc_jit_context_new_convert_vector saved_conv = p_gcc_jit_context_new_convert_vector;
    p_gcc_jit_context_new_convert_vector = NULL;
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 32, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_convertvector(&e_node, &rv);
    }));
    p_gcc_jit_context_new_convert_vector = saved_conv;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Mock callback for `p_gcc_jit_context_new_convert_vector` on hosts lacking that symbol.
 * @details Returns a zeroed destination vector rvalue of `type`.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] loc Optional `gcc_jit_location *`.
 * @param[in] vec Source vector rvalue.
 * @param[in] type Destination vector `gcc_jit_type *`.
 * @return Constructed destination vector rvalue.
 */
static struct gcc_jit_rvalue *mock_convert_vector_cb(
    struct gcc_jit_context *ctx,
    struct gcc_jit_location *loc,
    struct gcc_jit_rvalue *vec,
    struct gcc_jit_type *type) noexcept {
    (void)loc;
    (void)vec;
    gcc_jit_type *f_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
    gcc_jit_rvalue *z = gcc_jit_context_new_rvalue_from_double(ctx, f_t, 0.0);
    gcc_jit_rvalue *lanes[4] = { z, z, z, z };
    return gcc_jit_context_new_rvalue_from_vector(ctx, NULL, type, 4, lanes);
}

/**
 * @brief Exercises all remaining edge/error branches in lines 5550-6150 of `gcc_gen_be_expr.c`.
 * @details Covers `enk_sizeof`/`enk_alignof` VLA/null/side-effect branches, `enk_lowered_eh_construct`
 * pointer vs value & unwind/null-block branches, `co_await`/`co_yield`/`await_yield` void/bool/null
 * suspension branches, `vector_shuffle` unary/null-block/bad-arg branches, `convertvector` callback &
 * fallback branches, and `float_constant` error/fallback branches.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] fn Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t test_pass_a_tail_coverage(
    gcc_jit_context *ctx,
    gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    a_type int_tp, size_tp, bool_tp, void_tp, void_ptr_tp, vec_tp, fvec_tp, flt_tp;
    a_type zero_align_tp, vla_tp, vla_null_elem_tp, bad_vec_tp, zero_sz_int_tp, zero_vec_tp;
    GCC_GEN_BE_CHECK(expr_init_int_type(&int_tp, ik_int, 4));
    GCC_GEN_BE_CHECK(expr_init_int_type(&size_tp, ik_unsigned_long, 8));
    GCC_GEN_BE_CHECK(expr_init_int_type(&bool_tp, ik_int, 1));
    bool_tp.variant.integer.bool_type = TRUE;
    GCC_GEN_BE_CHECK(expr_init_float_type(&flt_tp, fk_float, 4));
    std::memset(&void_tp, 0, sizeof(void_tp));
    void_tp.kind = tk_void;
    GCC_GEN_BE_CHECK(expr_init_ptr_type(&void_ptr_tp, &void_tp, FALSE));

    GCC_GEN_BE_CHECK(expr_init_int_type(&zero_align_tp, ik_int, 4));
    zero_align_tp.alignment = 0;

    std::memset(&vla_tp, 0, sizeof(vla_tp));
    vla_tp.kind = tk_array;
    vla_tp.size = 0;
    vla_tp.alignment = 4;
    vla_tp.variant.array.element_type = &int_tp;
    vla_tp.variant.array.has_assoc_vla_dimension = TRUE;

    std::memset(&vla_null_elem_tp, 0, sizeof(vla_null_elem_tp));
    vla_null_elem_tp.kind = tk_array;
    vla_null_elem_tp.variant.array.element_type = NULL;
    vla_null_elem_tp.variant.array.variant.number_of_elements = 3;

    std::memset(&vec_tp, 0, sizeof(vec_tp));
    vec_tp.kind = tk_vector;
    vec_tp.size = 16;
    vec_tp.variant.vector.element_type = &int_tp;

    std::memset(&fvec_tp, 0, sizeof(fvec_tp));
    fvec_tp.kind = tk_vector;
    fvec_tp.size = 16;
    fvec_tp.variant.vector.element_type = &flt_tp;

    std::memset(&bad_vec_tp, 0, sizeof(bad_vec_tp));
    bad_vec_tp.kind = tk_vector;
    bad_vec_tp.size = 16;
    bad_vec_tp.variant.vector.element_type = NULL;

    GCC_GEN_BE_CHECK(expr_init_int_type(&zero_sz_int_tp, ik_int, 0));
    std::memset(&zero_vec_tp, 0, sizeof(zero_vec_tp));
    zero_vec_tp.kind = tk_vector;
    zero_vec_tp.size = 0;
    zero_vec_tp.variant.vector.element_type = &zero_sz_int_tp;

    gcc_jit_rvalue *rv = NULL;
    an_expr_node e_sz, e_c1;
    a_constant c1;
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_c1, &c1, &zero_align_tp));

    /* alignof: expr == NULL (stp == NULL), alignment == 0 */
    std::memset(&e_sz, 0, sizeof(e_sz));
    e_sz.kind = enk_alignof;
    e_sz.type = &size_tp;
    e_sz.variant.sizeof_info.is_type = FALSE;
    e_sz.variant.sizeof_info.variant.expr = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));
    e_sz.variant.sizeof_info.variant.expr = &e_c1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));

    /* sizeof: expr == NULL (byte_sz == 0), scalar type, array with NULL element, VLA side-effect + dim chains */
    e_sz.kind = enk_sizeof;
    e_sz.variant.sizeof_info.variant.expr = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));
    e_sz.variant.sizeof_info.is_type = TRUE;
    e_sz.variant.sizeof_info.variant.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));
    e_sz.variant.sizeof_info.variant.type = &vla_null_elem_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));

    /* VLA sizeof with non-variable operand (side-effect eval), cblock == NULL, original_dimension chain, dim_lv uncached & null */
    an_expr_node e_vla_expr;
    std::memset(&e_vla_expr, 0, sizeof(e_vla_expr));
    e_vla_expr.kind = enk_constant;
    e_vla_expr.type = &vla_tp;
    e_vla_expr.variant.constant.ptr = &c1;

    a_variable v_dim;
    std::memset(&v_dim, 0, sizeof(v_dim));
    v_dim.type = &size_tp;
    v_dim.storage_class = (a_storage_class)sc_auto;
    v_dim.source_corresp.name = "vla_d0";

    a_vla_dimension dim_other, dim_orig_null, dim_orig_real, dim_head;
    std::memset(&dim_other, 0, sizeof(dim_other));
    std::memset(&dim_orig_null, 0, sizeof(dim_orig_null));
    std::memset(&dim_orig_real, 0, sizeof(dim_orig_real));
    std::memset(&dim_head, 0, sizeof(dim_head));
    dim_other.type = &int_tp;
    dim_other.next = &dim_head;
    dim_orig_real.type = &vla_tp;
    dim_orig_real.dimension_variable = &v_dim;
    dim_orig_null.type = &vla_tp;
    dim_orig_null.original_dimension = NULL;
    dim_head.type = &vla_tp;
    dim_head.original_dimension = &dim_orig_real;

    a_scope vla_sc;
    std::memset(&vla_sc, 0, sizeof(vla_sc));
    vla_sc.vla_dimensions = &dim_other;

    e_sz.variant.sizeof_info.is_type = FALSE;
    e_sz.variant.sizeof_info.variant.expr = &e_vla_expr;

    /* 1) innermost_function_scope == NULL & cblock == NULL */
    innermost_function_scope = NULL;
    gcc_jit_block *saved_blk = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));
    /* 2) With scope, cblock == NULL (cur_fn == NULL) */
    innermost_function_scope = &vla_sc;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_blk));
    /* 3) With scope & cur_fn, local_var_return_null == TRUE (dim_lv == NULL) */
    g_expr_mock.local_var_return_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));
    g_expr_mock.local_var_return_null = FALSE;
    /* 4) With scope & cur_fn, uncached v_dim lowered then cached on second call */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));
    /* 5) dim with original_dimension == NULL & dimension_variable == NULL, and dim not found */
    dim_head.original_dimension = &dim_orig_null;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));
    vla_sc.vla_dimensions = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));
    /* 6) Variable VLA operand (`op_e->kind == enk_variable`) */
    a_variable v_vla_arr;
    std::memset(&v_vla_arr, 0, sizeof(v_vla_arr));
    v_vla_arr.type = &vla_tp;
    v_vla_arr.storage_class = (a_storage_class)sc_auto;
    an_expr_node e_vla_var;
    GCC_GEN_BE_CHECK(expr_init_var_expr(&e_vla_var, &v_vla_arr, &vla_tp));
    e_sz.variant.sizeof_info.variant.expr = &e_vla_var;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_sz, &rv));
    innermost_function_scope = NULL;

#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    /* `enk_lowered_eh_construct` complete branch sweep */
    an_expr_node e_eh, e_bad, e_void_op, e_thr_op;
    std::memset(&e_bad, 0, sizeof(e_bad));
    e_bad.kind = (an_expr_node_kind)251;
    std::memset(&e_void_op, 0, sizeof(e_void_op));
    e_void_op.kind = enk_type_operand;
    e_void_op.type = &void_tp;
    std::memset(&e_thr_op, 0, sizeof(e_thr_op));
    e_thr_op.kind = enk_throw;
    e_thr_op.type = &void_tp;

    std::memset(&e_eh, 0, sizeof(e_eh));
    e_eh.kind = enk_lowered_eh_construct;
    e_eh.type = &void_tp;
    e_eh.variant.lowered_eh.kind = (a_lowered_eh_construct_kind)leck_internal_try;
    /* cblock == NULL -> falls through to get_zero_rvalue */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    /* try_e == NULL, catch_e == NULL, unwind_resume_make_block == TRUE */
    g_expr_mock.unwind_resume_make_block = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));
    g_expr_mock.unwind_resume_make_block = FALSE;
    /* try_e error (`terr != GCC_GEN_BE_SUCCESS`) */
    e_eh.variant.lowered_eh.variant.try_and_catch_expr = &e_bad;
    if (gcc_gen_be_lower_expr_rvalue(&e_eh, &rv) != GCC_GEN_BE_ERROR_UNHANDLED_EXPR) {
        return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    }
    /* try_e clears current block (`tb == NULL`) & catch_e has void rval */
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    e_void_op.next = NULL;
    e_eh.variant.lowered_eh.variant.try_and_catch_expr = &e_void_op;
    /* Custom throw inside try_e that terminates block so tb == NULL: use enk_lowered_eh_construct nested or unwind */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));

    /* Caught/thrown object address: non-pointer type, cached vs uncached, unknown leck kind, void type */
    gcc_jit_lvalue *caught_lv = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "caught_slot");
    e_eh.type = &int_tp;
    e_eh.variant.lowered_eh.kind = (a_lowered_eh_construct_kind)leck_caught_object_address;
    /* Uncached -> calls __edg_gcc_eh_get_unwind_info (already cached from earlier test!) with non-pointer type */
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));
    /* Cached via active_caught_obj_lval with non-pointer type AND pointer type */
    gcc_gen_be_singleton_state.active_caught_obj_lval = caught_lv;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));
    e_eh.type = &void_ptr_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));
    gcc_gen_be_singleton_state.active_caught_obj_lval = NULL;
    /* Other leck kind (neither thrown nor caught) + type == NULL */
    e_eh.variant.lowered_eh.kind = (a_lowered_eh_construct_kind)99;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));
    e_eh.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(&e_eh, &rv));
#endif

    /* `co_await` / `co_yield` / `await_yield` full edge & suspension branches */
    an_expr_node e_aw;
    std::memset(&e_aw, 0, sizeof(e_aw));
    e_aw.kind = enk_await;
    e_aw.type = &int_tp;
    if (gcc_gen_be_lower_co_await(NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    if (gcc_gen_be_lower_co_await(&e_aw, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    if (gcc_gen_be_lower_co_yield(NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    if (gcc_gen_be_lower_co_yield(&e_aw, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    if (gcc_gen_be_lower_await_yield(NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    if (gcc_gen_be_lower_await_yield(&e_aw, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_await_yield(&e_aw, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_await_yield(&e_aw, &rv) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;

    /* rrs == NULL, operand == NULL, non-void expr->type (hits `!is_ready`, `await_suspend == NULL`, `expr->type->kind != tk_void`) */
    gcc_jit_lvalue *st_lv = gcc_jit_function_new_local(
        fn, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "coro_st_fld");
    g_expr_mock.coro_state_field_lval = st_lv;
    g_expr_mock.coro_suspend_idx = 3;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_co_await(&e_aw, &rv));

    /* coro_suspend_idx == 0 and expr->type == &void_tp / NULL */
    g_expr_mock.coro_suspend_idx = 0;
    e_aw.type = &void_tp;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_co_yield(&e_aw, &rv));
    e_aw.type = NULL;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_await_yield(&e_aw, &rv));

    /* await_suspend returning bool, void call rval, and NULL rval (`enk_type_operand`) */
    a_routine void_susp_r;
    a_type void_rtp;
    std::memset(&void_rtp, 0, sizeof(void_rtp));
    void_rtp.kind = tk_routine;
    void_rtp.variant.routine.return_type = &void_tp;
    std::memset(&void_susp_r, 0, sizeof(void_susp_r));
    void_susp_r.type = &void_rtp;
    void_susp_r.source_corresp.name = "void_susp_fn";

    an_expr_node e_rdy_b, e_res_v, e_susp_b, e_susp_vcall, e_susp_rout, e_susp_null;
    a_constant crb;
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_rdy_b, &crb, &bool_tp));
    std::memset(&e_res_v, 0, sizeof(e_res_v));
    e_res_v.kind = enk_type_operand;
    e_res_v.type = &void_tp;
    GCC_GEN_BE_CHECK(expr_init_int_const_expr(&e_susp_b, &crb, &bool_tp));
    e_rdy_b.next = &e_res_v;
    e_res_v.next = &e_susp_b;
    e_aw.type = &void_tp;
    e_aw.variant.await_info.ready_resume_suspend = &e_rdy_b;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_await_yield(&e_aw, &rv));

    std::memset(&e_susp_rout, 0, sizeof(e_susp_rout));
    e_susp_rout.kind = enk_routine;
    e_susp_rout.type = &void_rtp;
    e_susp_rout.variant.routine.ptr = &void_susp_r;
    GCC_GEN_BE_CHECK(expr_init_op_expr(&e_susp_vcall, eok_call, &void_tp, &e_susp_rout));
    e_res_v.next = &e_susp_vcall;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_await_yield(&e_aw, &rv));

    std::memset(&e_susp_null, 0, sizeof(e_susp_null));
    e_susp_null.kind = enk_type_operand;
    e_res_v.next = &e_susp_null;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_await_yield(&e_aw, &rv));
    g_expr_mock.coro_state_field_lval = NULL;
    g_expr_mock.coro_suspend_idx = 1;

    /* `gcc_gen_be_lower_vector_shuffle` & `gcc_gen_be_lower_convertvector` full edge sweep */
    an_expr_node e_shuf, ev1, ev2, enull_op;
    a_variable vv1, vv2;
    std::memset(&vv1, 0, sizeof(vv1));
    vv1.type = &vec_tp;
    vv1.storage_class = (a_storage_class)sc_auto;
    vv1.source_corresp.name = "pa_sv1";
    std::memset(&vv2, 0, sizeof(vv2));
    vv2.type = &vec_tp;
    vv2.storage_class = (a_storage_class)sc_auto;
    vv2.source_corresp.name = "pa_sv2";
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ev1, &vv1, &vec_tp));
    GCC_GEN_BE_CHECK(expr_init_var_expr(&ev2, &vv2, &vec_tp));
    std::memset(&enull_op, 0, sizeof(enull_op));
    enull_op.kind = enk_type_operand;

    std::memset(&e_shuf, 0, sizeof(e_shuf));
    e_shuf.kind = enk_builtin_operation;
    e_shuf.type = &vec_tp;
    e_shuf.variant.builtin_operation.kind = (a_builtin_operation_kind)bok_builtin_shuffle;

    if (gcc_gen_be_lower_vector_shuffle(NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    if (gcc_gen_be_lower_vector_shuffle(&e_shuf, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* res_type == NULL, op1 == NULL, op2 == NULL, v1 == NULL */
    e_shuf.type = NULL;
    if (gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv) != GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    e_shuf.type = &vec_tp;
    e_shuf.variant.builtin_operation.operands = NULL;
    if (gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv) != GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    ev1.next = NULL;
    e_shuf.variant.builtin_operation.operands = &ev1;
    if (gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv) != GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    enull_op.next = &ev2;
    ev2.next = NULL;
    e_shuf.variant.builtin_operation.operands = &enull_op;
    if (gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv) != GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;

    /* 2-operand bok_builtin_shuffle (with perm, without perm, mask_rval == NULL, v2 == NULL, cblock == NULL) */
    ev1.next = &ev2;
    ev2.next = NULL;
    e_shuf.variant.builtin_operation.operands = &ev1;
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv));

    pfn_gcc_jit_context_new_rvalue_vector_perm saved_perm = p_gcc_jit_context_new_rvalue_vector_perm;
    p_gcc_jit_context_new_rvalue_vector_perm = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv));
    /* cblock == NULL fallback */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
    p_gcc_jit_context_new_rvalue_vector_perm = saved_perm;

    /* mask_rval == NULL & v2 == NULL with perm non-NULL */
    ev1.next = &enull_op;
    enull_op.next = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv));
    ev1.next = &enull_op;
    enull_op.next = &ev2;
    ev2.next = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv));

    /* shufflevector with v2 == NULL, non-vector / bad_vec_tp / zero_vec_tp, and non-constant mask */
    e_shuf.variant.builtin_operation.kind = (a_builtin_operation_kind)bok_builtin_shufflevector;
    ev1.next = &enull_op;
    enull_op.next = &ev2;
    ev2.next = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv));

    gcc_jit_type *jit_vec = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&vec_tp, &jit_vec));
    GCC_GEN_BE_CHECK(expr_test_map_type(&bad_vec_tp, jit_vec));
    GCC_GEN_BE_CHECK(expr_test_map_type(&zero_vec_tp, jit_vec));
    e_shuf.type = &bad_vec_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv));
    e_shuf.type = &zero_vec_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv));
    GCC_GEN_BE_CHECK(expr_test_map_type(&int_tp, jit_vec));
    e_shuf.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vector_shuffle(&e_shuf, &rv));
    GCC_GEN_BE_CHECK(expr_test_reset_mock());

    /* `convertvector`: invalid args, NULL ctx, NULL res_type, NULL op1, direct op1, NULL src_vec, callback, NULL cblock, non-vec types */
    an_expr_node e_cv;
    std::memset(&e_cv, 0, sizeof(e_cv));
    e_cv.kind = enk_builtin_operation;
    e_cv.type = &fvec_tp;
    e_cv.variant.builtin_operation.kind = (a_builtin_operation_kind)bok_builtin_convertvector;

    if (gcc_gen_be_lower_convertvector(NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    if (gcc_gen_be_lower_convertvector(&e_cv, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_convertvector(&e_cv, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    e_cv.type = NULL;
    if (gcc_gen_be_lower_convertvector(&e_cv, &rv) != GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    e_cv.type = &fvec_tp;
    e_cv.variant.builtin_operation.operands = NULL;
    if (gcc_gen_be_lower_convertvector(&e_cv, &rv) != GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    enull_op.next = NULL;
    e_cv.variant.builtin_operation.operands = &enull_op;
    if (gcc_gen_be_lower_convertvector(&e_cv, &rv) != GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    an_expr_node e_tp2;
    std::memset(&e_tp2, 0, sizeof(e_tp2));
    e_tp2.kind = enk_type_operand;
    enull_op.next = &e_tp2;
    if (gcc_gen_be_lower_convertvector(&e_cv, &rv) != GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;

    /* Direct op1 (not enk_type_operand) + mock p_gcc_jit_context_new_convert_vector + cblock == NULL + non-vec types */
    ev1.next = NULL;
    e_cv.variant.builtin_operation.operands = &ev1;
    pfn_gcc_jit_context_new_convert_vector orig_cv = p_gcc_jit_context_new_convert_vector;
    p_gcc_jit_context_new_convert_vector = &mock_convert_vector_cb;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_convertvector(&e_cv, &rv));
    p_gcc_jit_context_new_convert_vector = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_convertvector(&e_cv, &rv));
    GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));

    gcc_jit_type *jit_fvec = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&fvec_tp, &jit_fvec));
    GCC_GEN_BE_CHECK(expr_test_map_type(&bad_vec_tp, jit_fvec));
    GCC_GEN_BE_CHECK(expr_test_map_type(&zero_vec_tp, jit_fvec));
    ev1.type = &bad_vec_tp;
    e_cv.type = &bad_vec_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_convertvector(&e_cv, &rv));
    ev1.type = NULL;
    e_cv.type = &zero_vec_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_convertvector(&e_cv, &rv));
    GCC_GEN_BE_CHECK(expr_test_map_type(&int_tp, jit_fvec));
    ev1.type = &int_tp;
    e_cv.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_convertvector(&e_cv, &rv));
    p_gcc_jit_context_new_convert_vector = orig_cv;
    GCC_GEN_BE_CHECK(expr_test_reset_mock());

    /* `gcc_gen_be_lower_float_constant`: invalid args, non-float kind, NULL ctx, NULL con->type, lowered NULL flt_type */
    a_constant cf;
    std::memset(&cf, 0, sizeof(cf));
    cf.kind = ck_integer;
    if (gcc_gen_be_lower_float_constant(NULL, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    if (gcc_gen_be_lower_float_constant(&cf, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    if (gcc_gen_be_lower_float_constant(&cf, NULL, &rv) != GCC_GEN_BE_ERROR_FLOAT_PRECISION_FAILED) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    cf.kind = ck_float;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_float_constant(&cf, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    cf.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_float_constant(&cf, NULL, &rv));
    cf.type = &int_tp;
    GCC_GEN_BE_CHECK(expr_test_map_type(&int_tp, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_float_constant(&cf, NULL, &rv));
    GCC_GEN_BE_CHECK(expr_test_reset_mock());

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t run_expr_rval_part2_tests(void) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    gcc_jit_function *fn = NULL;
    gcc_jit_block *blk = NULL;
    GCC_GEN_BE_CHECK(expr_test_setup_fn(&ctx, &fn, &blk));

    gcc_gen_be_error_t err = test_calls_va_and_casts(ctx, fn);
    if (err == GCC_GEN_BE_SUCCESS) {
        GCC_GEN_BE_CHECK(expr_test_reset_mock());
        err = test_remaining_nodes_and_vectors(ctx, fn);
    }
    if (err == GCC_GEN_BE_SUCCESS) {
        GCC_GEN_BE_CHECK(expr_test_reset_mock());
        err = test_pass_a_tail_coverage(ctx, fn);
    }

    GCC_GEN_BE_CHECK(expr_test_teardown_fn(ctx));
    return err;
}

END_EDG_NAMESPACE
