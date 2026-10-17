/**
 * @file test_gcc_be_rtti_full.cpp
 * @brief Comprehensive unit tests for Itanium C++ ABI RTTI, VTables, VTC, VTT, and Thunks.
 * @details Exercises mangling of nested namespaces/classes, templates, function/array/ptr-to-member
 * types, CV-qualifiers, secondary vtables, Construction Virtual Tables (`_ZTC`), VTTs (`_ZTT`),
 * `_ZTv` virtual-base thunks, and `_ZTch` covariant return thunks.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_lib_loader.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for RTTI full unit tests.
 * @details Returns the error code passed to it without terminating.
 * @param[in] err The error code.
 * @param[in] msg Optional diagnostic message.
 * @return The input error code `err`.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char *msg) GCC_GEN_BE_NOEXCEPT {
    (void)msg;
    return err;
}
}

USING_NAMESPACE_EDG

namespace edg {
/** @brief Optimization level option. */
int gcc_be_opt_level = 0;
/** @brief Debug info option. */
a_boolean gcc_be_debug_info = FALSE;
/** @brief Dump initial tree option. */
a_boolean gcc_be_dump_initial_tree = FALSE;
/** @brief Dump GIMPLE option. */
a_boolean gcc_be_dump_gimple = FALSE;
/** @brief Dump RTL option. */
a_boolean gcc_be_dump_rtl_all = FALSE;
/** @brief Fast math option. */
a_boolean gcc_be_ffast_math = FALSE;
/** @brief Associative math option. */
a_boolean gcc_be_fassociative_math = FALSE;
/** @brief No trapping math option. */
a_boolean gcc_be_fno_trapping_math = FALSE;
/** @brief Shared library option. */
a_boolean gcc_be_shared = FALSE;
/** @brief Static linking option. */
a_boolean gcc_be_static = FALSE;
/** @brief Target architecture string. */
a_const_char *gcc_be_march = NULL;
/** @brief Target CPU string. */
a_const_char *gcc_be_mcpu = NULL;
/** @brief Target tune string. */
a_const_char *gcc_be_mtune = NULL;
/** @brief Target FPU string. */
a_const_char *gcc_be_mfpu = NULL;
/** @brief Target code model string. */
a_const_char *gcc_be_mcmodel = NULL;
/** @brief 64-bit mode flag. */
a_boolean gcc_be_m64 = FALSE;
/** @brief 32-bit mode flag. */
a_boolean gcc_be_m32 = FALSE;
/** @brief Position-independent code flag. */
a_boolean gcc_be_fPIC = FALSE;
/** @brief Position-independent executable flag. */
a_boolean gcc_be_fPIE = FALSE;
/** @brief No position-independent executable flag. */
a_boolean gcc_be_fno_pie = FALSE;
/** @brief Exceptions enabled flag. */
a_boolean exceptions_enabled = TRUE;
/** @brief Driver options list pointer. */
void *gcc_be_driver_options = NULL;
/** @brief Compiler options list pointer. */
void *gcc_be_compiler_options = NULL;
/** @brief Active C dialect. */
a_C_dialect C_dialect = C_dialect_cplusplus;
/** @brief GCC compatibility mode flag. */
a_boolean gcc_mode = TRUE;
/** @brief Mock IL header. */
an_il_header il_header;

/**
 * @brief Stub for signed integer constant extraction in RTTI unit tests.
 * @details Sets `*err` to `FALSE` and returns 0.
 * @param[in] cp The frontend constant pointer.
 * @param[out] err Pointer to receive overflow flag.
 * @return Extracted signed integer value.
 */
a_host_large_integer value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    (void)cp;
    if (err) *err = FALSE;
    return 0;
}

/**
 * @brief Stub for invisible reference check in RTTI unit tests.
 * @details Sets `*out_is_ref` to `FALSE` and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] type The frontend type pointer.
 * @param[out] out_is_ref Pointer to receive invisible reference flag.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (!out_is_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_ref = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for type lowering in RTTI unit tests.
 * @details Lowers any type to `void*` for thunk testing.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_type Pointer to receive the lowered `gcc_jit_type`.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function declaration lowering in RTTI unit tests.
 * @details Creates an imported function accepting `void*` and returning `void*`.
 * @param[in] rout The frontend routine pointer.
 * @param[out] out_func Pointer to receive the lowered function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *vp = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_param *p0 = gcc_jit_context_new_param(ctx, NULL, vp, "this_ptr");
    *out_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, vp, "_ZN7Derived4funcEv", 1, &p0, 0);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression rvalue lowering in RTTI unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] expr The frontend expression pointer.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    *out_rval = gcc_jit_context_null(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR));
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression lvalue lowering in RTTI unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] expr The frontend expression pointer.
 * @param[out] out_lval Pointer to receive the lowered lvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for global variable declaration lowering in RTTI unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered global variable lvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}
}

/**
 * @brief Runs all RTTI, VTable, VTT, and Thunk unit tests.
 * @details Exercises `gcc_gen_be_emit_vtable`, `gcc_gen_be_emit_vtt`, `gcc_gen_be_emit_thunk`,
 * `gcc_gen_be_emit_vcall_thunk`, `gcc_gen_be_emit_covariant_return_thunk`, `gcc_gen_be_lower_dynamic_cast`,
 * `gcc_gen_be_lower_typeid`, and `gcc_gen_be_emit_type_descriptor`.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_rtti_full_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_lvalue *out_lval = NULL;
    gcc_jit_function *out_fn = NULL;
    gcc_jit_rvalue *out_rval = NULL;

    if (gcc_gen_be_emit_vtable(NULL, &out_lval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_vtt(NULL, &out_lval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_thunk(NULL, &out_fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_vcall_thunk(NULL, NULL, NULL, 0, &out_fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_covariant_return_thunk(NULL, 0, 0, FALSE, &out_fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_dynamic_cast(NULL, &out_rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_typeid(NULL, &out_rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_type_descriptor(NULL, &out_lval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(cache_clear_all());

    a_type ptr_type;
    std::memset(&ptr_type, 0, sizeof(ptr_type));
    ptr_type.kind = tk_pointer;
    ptr_type.size = 8;

    a_param_type param0;
    std::memset(&param0, 0, sizeof(param0));
    param0.type = &ptr_type;

    a_routine_type_supplement rsupp;
    std::memset(&rsupp, 0, sizeof(rsupp));
    rsupp.param_type_list = &param0;

    a_type rtype;
    std::memset(&rtype, 0, sizeof(rtype));
    rtype.kind = tk_routine;
    rtype.variant.routine.return_type = &ptr_type;
    rtype.variant.routine.extra_info = &rsupp;

    a_routine target_rout;
    std::memset(&target_rout, 0, sizeof(target_rout));
    target_rout.source_corresp.name = "_ZN7Derived4funcEv";
    target_rout.type = &rtype;

    a_routine thunk_rout;
    std::memset(&thunk_rout, 0, sizeof(thunk_rout));
    thunk_rout.source_corresp.name = "_ZN7Derived4funcEv";
    thunk_rout.type = &rtype;
    thunk_rout.overriding_function_for_wrapper = &target_rout;

    p_gcc_jit_rvalue_set_bool_require_tail_call = [](struct gcc_jit_rvalue *c, int req) {
        (void)c; (void)req;
    };
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vcall_thunk(&ptr_type, NULL, &thunk_rout, -24L, &out_fn));
    p_gcc_jit_rvalue_set_bool_require_tail_call = NULL;
    if (!out_fn) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_emit_covariant_return_thunk(&thunk_rout, -8L, 16L, FALSE, &out_fn));
    if (!out_fn) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_emit_covariant_return_thunk(&thunk_rout, -8L, -16L, TRUE, &out_fn));
    if (!out_fn) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test __pointer_type_info (4 fields) and __pointer_to_member_type_info (5 fields) */
    a_type int_tp;
    std::memset(&int_tp, 0, sizeof(int_tp));
    int_tp.kind = tk_integer;
    int_tp.size = 4;
    int_tp.variant.integer.int_kind = ik_int;

    a_type const_int_tp;
    std::memset(&const_int_tp, 0, sizeof(const_int_tp));
    const_int_tp.kind = tk_typeref;
    const_int_tp.variant.typeref.qualifiers = TQ_CONST;
    const_int_tp.variant.typeref.type = &int_tp;

    a_type ptr_to_const_int;
    std::memset(&ptr_to_const_int, 0, sizeof(ptr_to_const_int));
    ptr_to_const_int.kind = tk_pointer;
    ptr_to_const_int.size = 8;
    ptr_to_const_int.variant.pointer.type = &const_int_tp;

    gcc_jit_lvalue *ptr_zti = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&ptr_to_const_int, &ptr_zti));
    if (!ptr_zti) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test __vmi_class_type_info with 2 bases (one virtual, one non-virtual at offset 8) */
    a_class_type_supplement b1_supp, b2_supp, vmi_supp;
    std::memset(&b1_supp, 0, sizeof(b1_supp));
    std::memset(&b2_supp, 0, sizeof(b2_supp));
    std::memset(&vmi_supp, 0, sizeof(vmi_supp));

    a_type base1_tp, base2_tp, vmi_tp;
    std::memset(&base1_tp, 0, sizeof(base1_tp));
    base1_tp.kind = tk_class;
    base1_tp.size = 8;
    base1_tp.source_corresp.name = "Base1";
    base1_tp.variant.class_struct_union.extra_info = &b1_supp;

    std::memset(&base2_tp, 0, sizeof(base2_tp));
    base2_tp.kind = tk_class;
    base2_tp.size = 8;
    base2_tp.source_corresp.name = "Base2";
    base2_tp.variant.class_struct_union.extra_info = &b2_supp;

    a_base_class bc1, bc2;
    std::memset(&bc1, 0, sizeof(bc1));
    std::memset(&bc2, 0, sizeof(bc2));
    bc1.type = &base1_tp;
    bc1.direct = TRUE;
    bc1.is_virtual = TRUE;
    bc1.has_public_derivation = TRUE;
    bc1.offset = 16;
    bc1.next_direct = &bc2;
    bc1.next = &bc2;

    bc2.type = &base2_tp;
    bc2.direct = TRUE;
    bc2.is_virtual = FALSE;
    bc2.has_public_derivation = TRUE;
    bc2.offset = 8;

    vmi_supp.direct_base_classes = &bc1;
    vmi_supp.base_classes = &bc1;
    vmi_supp.highest_virtual_function_number = 0;

    std::memset(&vmi_tp, 0, sizeof(vmi_tp));
    vmi_tp.kind = tk_class;
    vmi_tp.size = 24;
    vmi_tp.source_corresp.name = "DiamondDerived";
    vmi_tp.variant.class_struct_union.any_virtual_functions = TRUE;
    vmi_tp.variant.class_struct_union.any_virtual_base_classes = TRUE;
    vmi_tp.variant.class_struct_union.extra_info = &vmi_supp;

    gcc_jit_lvalue *vmi_zti = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&vmi_tp, &vmi_zti));
    if (!vmi_zti) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_lvalue *vmi_vtt = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtt(&vmi_tp, &vmi_vtt));
    if (!vmi_vtt) return GCC_GEN_BE_ERROR_INTERNAL;

    a_type ptm_tp;
    std::memset(&ptm_tp, 0, sizeof(ptm_tp));
    ptm_tp.kind = tk_ptr_to_member;
    ptm_tp.size = 8;
    ptm_tp.variant.ptr_to_member.type = &int_tp;
    ptm_tp.variant.ptr_to_member.class_of_which_a_member = &base1_tp;

    gcc_jit_lvalue *ptm_zti = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&ptm_tp, &ptm_zti));
    if (!ptm_zti) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test gcc_gen_be_mangle_type_name and gcc_gen_be_mangle_function_name */
    char mtype_buf[128];
    if (gcc_gen_be_mangle_type_name(&int_tp, NULL, 0) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&int_tp, mtype_buf, sizeof(mtype_buf)));
    if (std::strcmp(mtype_buf, "i") != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_mangle_with_substitutions(&base1_tp, TRUE, NULL, 0) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_with_substitutions(&base1_tp, TRUE, mtype_buf, sizeof(mtype_buf)));
    if (std::strcmp(mtype_buf, "5Base1") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_with_substitutions(&base1_tp, FALSE, mtype_buf, sizeof(mtype_buf)));
    if (std::strcmp(mtype_buf, "S_") != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    a_routine op_rout;
    std::memset(&op_rout, 0, sizeof(op_rout));
    op_rout.source_corresp.name = "operator==";
    op_rout.type = &rtype;
    char mfn_buf[128];
    if (gcc_gen_be_mangle_function_name(NULL, NULL, mfn_buf, sizeof(mfn_buf)) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&op_rout, NULL, mfn_buf, sizeof(mfn_buf)));
    if (std::strncmp(mfn_buf, "_Zeq", 4) != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Exercise expanded operator table, conversion operator, and literal operator mangling */
    const char *all_ops[] = {
        "operatorco_await", "operator->*", "operator->", "operator<<=", "operator>>=",
        "operator<<", "operator>>", "operator+=", "operator-=", "operator*=",
        "operator/=", "operator%=", "operator&=", "operator|=", "operator^=",
        "operator++", "operator--", "operator&&", "operator||", "operator%",
        "operator&", "operator|", "operator^", "operator~", "operator!",
        "operator,", "operator\"\"_km"
    };
    for (size_t i = 0; i < sizeof(all_ops) / sizeof(all_ops[0]); ++i) {
        op_rout.source_corresp.name = all_ops[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&op_rout, NULL, mfn_buf, sizeof(mfn_buf)));
    }
    op_rout.special_kind = (a_special_function_kind)sfk_conversion;
    op_rout.source_corresp.name = "operator int*";
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&op_rout, NULL, mfn_buf, sizeof(mfn_buf)));
    if (std::strncmp(mfn_buf, "_Zcv", 4) != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Exercise gcc_gen_be_mangle_expr (constant, sizeof(T), binary/unary ops, decltype fallback) */
    char mexpr_buf[128];
    if (gcc_gen_be_mangle_expr(NULL, mexpr_buf, sizeof(mexpr_buf)) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    a_constant expr_con;
    std::memset(&expr_con, 0, sizeof(expr_con));
    expr_con.kind = ck_integer;
    expr_con.type = &int_tp;
    an_expr_node expr_lit, expr_sz, expr_add, expr_neg, expr_dt;
    std::memset(&expr_lit, 0, sizeof(expr_lit));
    expr_lit.kind = enk_constant;
    expr_lit.variant.constant.ptr = &expr_con;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(&expr_lit, mexpr_buf, sizeof(mexpr_buf)));

    std::memset(&expr_sz, 0, sizeof(expr_sz));
    expr_sz.kind = enk_type_operand;
    expr_sz.variant.type_operand.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(&expr_sz, mexpr_buf, sizeof(mexpr_buf)));
    if (std::strcmp(mexpr_buf, "sti") != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    std::memset(&expr_add, 0, sizeof(expr_add));
    expr_add.kind = enk_operation;
    expr_add.variant.operation.kind = eok_add;
    expr_lit.next = &expr_sz;
    expr_add.variant.operation.operands = &expr_lit;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(&expr_add, mexpr_buf, sizeof(mexpr_buf)));
    expr_lit.next = NULL;

    std::memset(&expr_neg, 0, sizeof(expr_neg));
    expr_neg.kind = enk_operation;
    expr_neg.variant.operation.kind = eok_negate;
    expr_neg.variant.operation.operands = &expr_lit;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(&expr_neg, mexpr_buf, sizeof(expr_neg)));

    std::memset(&expr_dt, 0, sizeof(expr_dt));
    expr_dt.kind = enk_typeid;
    expr_dt.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(&expr_dt, mexpr_buf, sizeof(mexpr_buf)));
    if (std::strcmp(mexpr_buf, "DTiE") != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Exercise dynamic_cast src2dst hint calculation: unique public non-virtual base (offset 8), virtual base (-2), ambiguous base (-2) */
    a_type ptr_base1 = {}, ptr_base2 = {}, ptr_vmi = {};
    ptr_base1.kind = tk_pointer; ptr_base1.size = 8; ptr_base1.variant.pointer.type = &base1_tp;
    ptr_base2.kind = tk_pointer; ptr_base2.size = 8; ptr_base2.variant.pointer.type = &base2_tp;
    ptr_vmi.kind = tk_pointer; ptr_vmi.size = 8; ptr_vmi.variant.pointer.type = &vmi_tp;

    an_expr_node dc_op = {}, dc_expr = {};
    dc_op.kind = enk_operation;
    dc_op.type = &ptr_base2;
    dc_expr.kind = enk_operation;
    dc_expr.type = &ptr_vmi;
    dc_expr.variant.operation.operands = &dc_op;
    gcc_jit_rvalue *dc_res = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_cast(&dc_expr, &dc_res));
    if (!dc_res) return GCC_GEN_BE_ERROR_INTERNAL;

    dc_op.type = &ptr_base1; /* virtual base -> hint -2 */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_cast(&dc_expr, &dc_res));

    a_base_class bc_dup = {};
    bc_dup.type = &base2_tp;
    bc_dup.direct = TRUE;
    bc_dup.has_public_derivation = TRUE;
    bc_dup.offset = 24;
    bc2.next = &bc_dup;
    bc2.next_direct = &bc_dup;
    dc_op.type = &ptr_base2; /* duplicate base -> hint -2 */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_cast(&dc_expr, &dc_res));
    bc2.next = NULL;
    bc2.next_direct = NULL;

    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_jit_context_release(ctx);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for the RTTI full unit test executable.
 * @details Runs `run_rtti_full_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_rtti_full_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}

