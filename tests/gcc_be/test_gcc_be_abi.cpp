/**
 * @file test_gcc_be_abi.cpp
 * @brief Exhaustive unit tests for ABI classification, attributes, coercion, and register allocation (`gcc_gen_be_abi.c`).
 * @details Exercises every function, target ABI, aggregate shape, calling convention/attribute hook,
 * register exhaustion edge case, and deterministic OOM step in `src/gcc_gen_be_abi.c`.
 * @ingroup gcc_gen_be_tests
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for ABI unit tests.
 * @param[in] err The error code.
 * @param[in] msg Optional diagnostic message.
 * @return The input error code `err`.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char *msg) GCC_GEN_BE_NOEXCEPT {
    return msg ? err : err;
}
}

USING_NAMESPACE_EDG

/** @brief Controls whether `gcc_gen_be_lower_type` returns `*out_type = NULL` (testing `!agg_type` failure). */
static bool g_lower_type_return_null = false;
/** @brief Controls `is_trivially_copyable_type` stub return value. */
static a_boolean g_trivially_copyable = TRUE;
/** @brief Counter of attribute callback invocations. */
static int g_abi_attr_hits = 0;

BEGIN_EDG_NAMESPACE
/** @brief Optimization level. */
int gcc_be_opt_level = 0;
/** @brief Debug info flag. */
a_boolean gcc_be_debug_info = FALSE;
/** @brief Dump initial tree flag. */
a_boolean gcc_be_dump_initial_tree = FALSE;
/** @brief Dump gimple flag. */
a_boolean gcc_be_dump_gimple = FALSE;
/** @brief Dump RTL flag. */
a_boolean gcc_be_dump_rtl_all = FALSE;
/** @brief Fast math flag. */
a_boolean gcc_be_ffast_math = FALSE;
/** @brief Associative math flag. */
a_boolean gcc_be_fassociative_math = FALSE;
/** @brief No trapping math flag. */
a_boolean gcc_be_fno_trapping_math = FALSE;
/** @brief Shared library flag. */
a_boolean gcc_be_shared = FALSE;
/** @brief Static linking flag. */
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
/** @brief PIC flag. */
a_boolean gcc_be_fPIC = FALSE;
/** @brief PIE flag. */
a_boolean gcc_be_fPIE = FALSE;
/** @brief No PIE flag. */
a_boolean gcc_be_fno_pie = FALSE;
/** @brief Exceptions enabled flag. */
a_boolean exceptions_enabled = TRUE;
/** @brief Active C dialect. */
a_C_dialect C_dialect = C_dialect_cplusplus;
/** @brief GCC compatibility mode flag. */
a_boolean gcc_mode = TRUE;

/**
 * @brief Stub for computing type size in ABI unit tests.
 * @param[in] tp The frontend type pointer.
 * @return Byte size of `tp`.
 */
a_targ_size_t f_size_of_type(a_type_ptr tp) {
    return tp ? tp->size : 0;
}

/**
 * @brief Stub for checking trivial copyability in ABI unit tests.
 * @param[in] tp The frontend type pointer.
 * @return `g_trivially_copyable`.
 */
a_boolean is_trivially_copyable_type(a_type_ptr tp) {
    return tp ? g_trivially_copyable : TRUE;
}

/**
 * @brief Stub for type lowering in ABI unit tests.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_type Pointer to receive the lowered `gcc_jit_type`.
 * @return GCC_GEN_BE_SUCCESS on success, or OOM on injected fault.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_lower_type_return_null) {
        *out_type = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx || !tp) {
        *out_type = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    return GCC_GEN_BE_SUCCESS;
}
END_EDG_NAMESPACE

/**
 * @brief Mock `gcc_jit_function_add_attribute` callback.
 * @param[in] fn Function handle.
 * @param[in] attr Attribute kind.
 */
static void mock_abi_fn_attr(gcc_jit_function *fn, enum gcc_jit_fn_attribute attr) noexcept {
    if (fn && (int)attr >= 0) ++g_abi_attr_hits;
}

/**
 * @brief Mock `gcc_jit_function_add_string_attribute` callback.
 * @param[in] fn Function handle.
 * @param[in] attr Attribute kind.
 * @param[in] val Attribute string.
 */
static void mock_abi_fn_str_attr(gcc_jit_function *fn, enum gcc_jit_fn_attribute attr, const char *val) noexcept {
    if (fn && (int)attr >= 0 && val) ++g_abi_attr_hits;
}

/**
 * @brief Mock `gcc_jit_lvalue_set_tls_model` callback.
 * @param[in] lv Lvalue handle.
 * @param[in] model TLS model.
 */
static void mock_abi_tls(gcc_jit_lvalue *lv, enum gcc_jit_tls_model model) noexcept {
    if (lv && (int)model >= 0) ++g_abi_attr_hits;
}

/**
 * @brief Mock `gcc_jit_lvalue_set_alignment` callback.
 * @param[in] lv Lvalue handle.
 * @param[in] bytes Alignment bytes.
 */
static void mock_abi_align(gcc_jit_lvalue *lv, unsigned bytes) noexcept {
    if (lv && bytes > 0U) ++g_abi_attr_hits;
}

/**
 * @brief Mock `gcc_jit_lvalue_set_link_section` callback.
 * @param[in] lv Lvalue handle.
 * @param[in] sec Section name.
 */
static void mock_abi_sec(gcc_jit_lvalue *lv, const char *sec) noexcept {
    if (lv && sec) ++g_abi_attr_hits;
}

/**
 * @brief Mock `gcc_jit_lvalue_add_string_attribute` callback.
 * @param[in] lv Lvalue handle.
 * @param[in] attr Variable attribute kind.
 * @param[in] val Attribute string.
 */
static void mock_abi_var_str_attr(gcc_jit_lvalue *lv, enum gcc_jit_variable_attribute attr, const char *val) noexcept {
    if (lv && (int)attr >= 0 && val) ++g_abi_attr_hits;
}

/**
 * @brief Exercises function, struct, and variable attribute helpers across all branches.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_abi_attributes(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_type *it = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, it, "afn", 0, NULL, 0);
    gcc_jit_struct *st = gcc_jit_context_new_opaque_struct(ctx, NULL, "ast");
    gcc_jit_lvalue *lv = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, it, "agv");

    a_routine rout = {};
    a_type cls_tp = {};
    a_variable var = {};

    if (gcc_gen_be_apply_function_attributes(NULL, fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_apply_function_attributes(&rout, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_apply_struct_attributes(NULL, st) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_apply_struct_attributes(&cls_tp, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_apply_variable_attributes(NULL, lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_apply_variable_attributes(&var, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    an_attribute fa[7] = {};
    const an_attribute_kind fkinds[7] = { ak_always_inline, ak_cold, ak_noinline, ak_pure, ak_const, ak_used, ak_align };
    for (int i = 0; i < 7; ++i) {
        fa[i].kind = fkinds[i];
        fa[i].next = (i + 1 < 7) ? &fa[i + 1] : NULL;
    }
    rout.source_corresp.attributes = &fa[0];

    a_routine_type_supplement rsup = {};
    a_type rtp = {};
    rtp.kind = tk_routine;
    rtp.variant.routine.extra_info = &rsup;

    /* Test with pfn == NULL and pfn != NULL, rout->type == NULL, non-routine, null extra_info, and all calling conventions */
    p_gcc_jit_function_add_attribute = NULL;
    p_gcc_jit_function_add_string_attribute = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(&rout, fn));
    rout.type = &cls_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(&rout, fn));
    rout.type = &rtp;
    rtp.variant.routine.extra_info = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(&rout, fn));
    rtp.variant.routine.extra_info = &rsup;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(&rout, fn));

    p_gcc_jit_function_add_attribute = &mock_abi_fn_attr;
    p_gcc_jit_function_add_string_attribute = &mock_abi_fn_str_attr;
    const a_calling_convention ccs[] = { cc_default, cc_stdcall, cc_fastcall, cc_thiscall, cc_vectorcall };
    for (size_t i = 0; i < sizeof(ccs) / sizeof(ccs[0]); ++i) {
        rsup.calling_convention = ccs[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(&rout, fn));
    }

    /* Struct attributes: ak_align, ak_packed, and other */
    an_attribute sa[3] = {};
    sa[0].kind = ak_align; sa[0].next = &sa[1];
    sa[1].kind = ak_packed; sa[1].next = &sa[2];
    sa[2].kind = ak_cold;
    cls_tp.source_corresp.attributes = &sa[0];
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_struct_attributes(&cls_tp, st));

    /* Variable attributes: TLS, alignment, section (valid, null arg, non-const arg, non-string const), visibility (first & duplicate) */
    a_constant c_str = {}, c_int = {};
    c_str.kind = ck_string; c_str.variant.string.value = ".my_sec";
    c_int.kind = ck_integer;
    an_attribute_arg a_valid = {}, a_nonconst = {}, a_nonstring = {};
    a_valid.kind = aak_constant; a_valid.variant.constant = &c_str;
    a_nonconst.kind = aak_type;
    a_nonstring.kind = aak_constant; a_nonstring.variant.constant = &c_int;

    an_attribute va[9] = {};
    va[0].kind = ak_section; va[0].arguments = NULL; va[0].next = &va[1];
    va[1].kind = ak_section; va[1].arguments = &a_nonconst; va[1].next = &va[2];
    va[2].kind = ak_section; va[2].arguments = &a_nonstring; va[2].next = &va[3];
    va[3].kind = ak_section; va[3].arguments = &a_valid; va[3].next = &va[4];
    va[4].kind = ak_visibility; va[4].arguments = NULL; va[4].next = &va[5];
    va[5].kind = ak_visibility; va[5].arguments = &a_nonconst; va[5].next = &va[6];
    va[6].kind = ak_visibility; va[6].arguments = &a_nonstring; va[6].next = &va[7];
    va[7].kind = ak_visibility; va[7].arguments = &a_valid; va[7].next = &va[8];
    va[8].kind = ak_visibility; va[8].arguments = &a_valid; /* second visibility skipped (vis_applied == TRUE) */

    var.source_corresp.attributes = &va[0];
    p_gcc_jit_lvalue_set_tls_model = NULL;
    p_gcc_jit_lvalue_set_alignment = NULL;
    p_gcc_jit_lvalue_set_link_section = NULL;
    p_gcc_jit_lvalue_add_string_attribute = NULL;
    var.is_thread_local = TRUE;
    var.alignment = 16;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_variable_attributes(&var, lv));

    p_gcc_jit_lvalue_set_tls_model = &mock_abi_tls;
    p_gcc_jit_lvalue_set_alignment = &mock_abi_align;
    p_gcc_jit_lvalue_set_link_section = &mock_abi_sec;
    p_gcc_jit_lvalue_add_string_attribute = &mock_abi_var_str_attr;
    var.is_thread_local = FALSE;
    var.alignment = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_variable_attributes(&var, lv));
    var.is_thread_local = TRUE;
    var.alignment = 16;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_variable_attributes(&var, lv));

    /* Function ak_visibility + ELF_visibility and variable ELF_visibility branches */
    an_attribute fva[5] = {};
    fva[0].kind = ak_visibility; fva[0].arguments = NULL; fva[0].next = &fva[1];
    fva[1].kind = ak_visibility; fva[1].arguments = &a_nonconst; fva[1].next = &fva[2];
    fva[2].kind = ak_visibility; fva[2].arguments = &a_nonstring; fva[2].next = &fva[3];
    fva[3].kind = ak_visibility; fva[3].arguments = &a_valid; fva[3].next = &fva[4];
    fva[4].kind = ak_visibility; fva[4].arguments = &a_valid;
    rout.source_corresp.attributes = &fva[0];
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(&rout, fn));
    p_gcc_jit_function_add_string_attribute = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(&rout, fn));
    p_gcc_jit_function_add_string_attribute = &mock_abi_fn_str_attr;
#if GNU_EXTENSIONS_ALLOWED && GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    rout.source_corresp.attributes = NULL;
    var.source_corresp.attributes = NULL;
    const an_ELF_visibility_kind ev_list[] = { evk_default, evk_protected, evk_hidden };
    for (size_t i = 0; i < 3; ++i) {
        rout.ELF_visibility = ev_list[i];
        var.ELF_visibility = ev_list[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(&rout, fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_apply_variable_attributes(&var, lv));
    }
    p_gcc_jit_function_add_string_attribute = NULL;
    p_gcc_jit_lvalue_add_string_attribute = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(&rout, fn));
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_variable_attributes(&var, lv));
#endif
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises SysV AMD64, AAPCS64, Win64, RISC-V, i386, PPC64LE, s390x, Darwin ARM64, AAPCS32, va_arg, coercion, and reg allocation.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_all_abi_classifiers_and_coercion(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_gen_be_abi_class_t cls[2];
    a_boolean b1 = FALSE, b2 = FALSE;
    int gp = 0, fp = 0;

    a_type tref_null = {};
    tref_null.kind = tk_typeref; tref_null.variant.typeref.type = NULL;

    /* Invalid NULL argument checks & dangling typeref checks across all classifiers */
    if (gcc_gen_be_classify_sysv_amd64(NULL, cls) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_sysv_amd64(&tref_null, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_aapcs64(NULL, &b1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_aapcs64(&tref_null, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_win64(NULL, &b1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_win64(&tref_null, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_riscv64(&tref_null, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_i386(&tref_null, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_ppc64le(&tref_null, &b1, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_ppc64le(&tref_null, NULL, &b2) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_s390x(&tref_null, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_darwin_arm64(NULL, &b1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_darwin_arm64(&tref_null, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_is_passed_by_invisible_reference(&tref_null, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_is_returned_via_sret(&tref_null, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_va_arg(NULL, &b1, &gp, &fp) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_va_arg(&tref_null, NULL, &gp, &fp) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&tref_null, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&tref_null, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_win64(&tref_null, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&tref_null, &b1, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_i386(&tref_null, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_ppc64le(&tref_null, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_s390x(&tref_null, &b1, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_darwin_arm64(&tref_null, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs32(&tref_null, &b1, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_va_arg(&tref_null, &b1, NULL, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_is_passed_by_invisible_reference(NULL, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(NULL, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_is_passed_by_invisible_reference(&tref_null, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(&tref_null, &b1));

    /* Auto-detected target ABI when override == 0 and out-of-range override > AAPCS32 + 1 */
    gcc_gen_be_singleton_state.active_target_abi_override = 0;
    gcc_gen_be_target_abi_t auto_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_target_abi(&auto_abi));
    gcc_gen_be_singleton_state.active_target_abi_override = 99;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_target_abi(&auto_abi));

    /* Scalar & vector & complex types */
    a_type t_i1 = {}, t_i2 = {}, t_i4 = {}, t_i8 = {}, t_i16 = {}, t_ptr = {};
    t_i1.kind = tk_integer; t_i1.size = 1;
    t_i2.kind = tk_integer; t_i2.size = 2;
    t_i4.kind = tk_integer; t_i4.size = 4;
    t_i8.kind = tk_integer; t_i8.size = 8;
    t_i16.kind = tk_integer; t_i16.size = 16;
    t_ptr.kind = tk_pointer; t_ptr.size = 8;

    a_type t_f4 = {}, t_f8 = {}, t_ld16 = {}, t_f80 = {}, t_f128 = {};
    t_f4.kind = tk_float; t_f4.size = 4; t_f4.variant.float_kind = fk_float;
    t_f8.kind = tk_float; t_f8.size = 8; t_f8.variant.float_kind = fk_double;
    t_ld16.kind = tk_float; t_ld16.size = 16; t_ld16.variant.float_kind = fk_long_double;
    t_f80.kind = tk_float; t_f80.size = 16; t_f80.variant.float_kind = fk_float80;
    t_f128.kind = tk_float; t_f128.size = 16; t_f128.variant.float_kind = fk_float128;

    a_type t_cf4 = {}, t_cld32 = {}, t_cf80_32 = {}, t_cld16 = {};
    t_cf4.kind = tk_complex; t_cf4.size = 8; t_cf4.variant.float_kind = fk_float;
    t_cld32.kind = tk_complex; t_cld32.size = 32; t_cld32.variant.float_kind = fk_long_double;
    t_cf80_32.kind = tk_complex; t_cf80_32.size = 32; t_cf80_32.variant.float_kind = fk_float80;
    t_cld16.kind = tk_complex; t_cld16.size = 16; t_cld16.variant.float_kind = fk_long_double;
    a_type t_cf80_16 = {};
    t_cf80_16.kind = tk_complex; t_cf80_16.size = 16; t_cf80_16.variant.float_kind = fk_float80;

    a_type t_v8 = {}, t_v16 = {}, t_v32 = {}, t_v64 = {}, t_v128 = {};
    t_v8.kind = tk_vector; t_v8.size = 8;
    t_v16.kind = tk_vector; t_v16.size = 16;
    t_v32.kind = tk_vector; t_v32.size = 32;
    t_v64.kind = tk_vector; t_v64.size = 64;
    t_v128.kind = tk_vector; t_v128.size = 128;

    /* 1. SysV AMD64 exhaustive classification: 0-size, >16 complex/AVX/struct, unions triggering X87/X87UP/COMPLEX_X87/SSEUP/MEMORY mergers */
    a_type st_empty0 = {}, st_only_zbf = {};
    st_empty0.kind = tk_struct; st_empty0.size = 0;
    a_field zbf = {};
    zbf.type = &t_i4; zbf.is_bit_field = TRUE; zbf.bit_size = 0;
    st_only_zbf.kind = tk_struct; st_only_zbf.size = 4; st_only_zbf.variant.class_struct_union.field_list = &zbf; /* classes[0] stays NO_CLASS -> becomes INTEGER! */

    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&st_empty0, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&st_only_zbf, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&t_cld32, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&t_cf80_32, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&t_v32, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&t_v64, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&t_v128, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&t_ld16, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&t_f80, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&t_cld16, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&t_cf80_16, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&t_cf4, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&t_i16, cls));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&t_v16, cls));

    /* Unions hitting every merge_amd64_classes branch:
       - SSE + SSE (same class a == b)
       - X87 + SSE -> MEMORY (`a == X87`)
       - SSE + X87 -> MEMORY (`b == X87`)
       - X87UP (at offset 8 via ld16, merged with SSE at offset 8 -> `a == X87UP` AND `b == X87UP`)
       - COMPLEX_X87 + SSE -> MEMORY (`a == COMPLEX_X87` AND `b == COMPLEX_X87`)
       - MEMORY + SSE (via misaligned field crossing 8B or bitfield >= 128b)
       - X87UP not preceded by X87 (`union { long double ld; int i; }`)
       - SSEUP not preceded by SSE (`union { v16 v; int i; }`) */
    a_field u_f1 = {}, u_f2 = {}, u_f3 = {};
    a_type u_mergers = {};
    u_mergers.kind = tk_union; u_mergers.size = 16; u_mergers.variant.class_struct_union.field_list = &u_f1;
    u_f1.next = &u_f2;

    /* Same class SSE + SSE */
    u_f1.type = &t_f8; u_f1.offset = 0; u_f2.type = &t_f4; u_f2.offset = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));
    /* a == X87, b == SSE -> MEMORY, then MEMORY + SSE -> a == MEMORY */
    u_f1.type = &t_ld16; u_f1.offset = 0; u_f2.type = &t_f8; u_f2.offset = 0; u_f2.next = &u_f3; u_f3.type = &t_f4; u_f3.offset = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));
    u_f2.next = NULL;
    /* a == SSE, b == X87 -> b == X87 */
    u_f1.type = &t_f8; u_f1.offset = 0; u_f2.type = &t_ld16; u_f2.offset = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));
    /* At offset 8: ld16 sets classes[1]=X87UP, then f8 at offset 8 merges (X87UP, SSE) -> a == X87UP */
    u_f1.type = &t_ld16; u_f1.offset = 0; u_f2.type = &t_f8; u_f2.offset = 8;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));
    /* Reverse: f8 at offset 8 sets classes[1]=SSE, then ld16 at offset 0 merges (SSE, X87UP) -> b == X87UP; also ld16 at offset 8 (`idx == 1`)! */
    u_f1.type = &t_f8; u_f1.offset = 8; u_f2.type = &t_ld16; u_f2.offset = 0; u_f2.next = &u_f3; u_f3.type = &t_ld16; u_f3.offset = 8;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));
    u_f2.next = NULL;
    /* a == COMPLEX_X87, b == SSE AND a == SSE, b == COMPLEX_X87 AND cld16 at offset 8 (`idx == 1`) */
    u_f1.type = &t_cld16; u_f1.offset = 0; u_f2.type = &t_f8; u_f2.offset = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));
    u_f1.type = &t_f8; u_f1.offset = 0; u_f2.type = &t_cld16; u_f2.offset = 0; u_f2.next = &u_f3; u_f3.type = &t_cld16; u_f3.offset = 8;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));
    /* b == MEMORY: classes[0]=SSE, then sub-union whose classes[1] becomes MEMORY? No, merge_amd64_classes `b` is only passed `INTEGER/SSE/SSEUP/X87/X87UP/COMPLEX_X87`, while `a` can be `MEMORY`! */
    u_f2.next = NULL;
    /* X87UP not preceded by X87: ld16 at 0 + int at 0 -> classes[0]=INTEGER, classes[1]=X87UP -> MEMORY! */
    u_f1.type = &t_ld16; u_f1.offset = 0; u_f2.type = &t_i4; u_f2.offset = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));
    /* classes[0]=SSE, classes[1]=MEMORY (ld16 at 0 + f8 at 8 -> classes[0]=X87, classes[1]=MEMORY; or f8 at 0 + union{ld16@0,f8@8}? No: f8 at 0 -> classes[0]=SSE, then at offset 8: ld16@0? Let's put f8@8 + ld16@8 -> classes[1]=merge(SSE, X87)=MEMORY while classes[0]=SSE!) */
    u_f1.type = &t_f8; u_f1.offset = 0; u_f2.type = &t_f8; u_f2.offset = 8; u_f2.next = &u_f3; u_f3.type = &t_ld16; u_f3.offset = 8;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));
    u_f2.next = NULL;
    /* SSEUP not preceded by SSE: v16 at 0 + i4 at 0 -> classes[0]=INTEGER, classes[1]=SSEUP -> converted to SSE! */
    u_f1.type = &t_v16; u_f1.offset = 0; u_f2.type = &t_i4; u_f2.offset = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));
    /* SSEUP + SSE -> reaches *out_class = SSE (line 172), AND INTEGER + SSE -> hits a == INTEGER! */
    u_f1.type = &t_v16; u_f1.offset = 0; u_f2.type = &t_f8; u_f2.offset = 8;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));
    u_f1.type = &t_i4; u_f1.offset = 0; u_f2.type = &t_f4; u_f2.offset = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));
    /* SSE + SSE (not SSEUP): f8 at 0 + f8 at 8 */
    u_f1.type = &t_f8; u_f1.offset = 0; u_f2.type = &t_f8; u_f2.offset = 8;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&u_mergers, cls));

    a_field f_cld16 = {}; f_cld16.type = &t_cld16;
    a_type st_cld16 = {}; st_cld16.kind = tk_struct; st_cld16.size = 16; st_cld16.variant.class_struct_union.field_list = &f_cld16;
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        if (gcc_gen_be_classify_sysv_amd64(&st_cld16, cls) == GCC_GEN_BE_SUCCESS) { g_gcc_be_oom_countdown = -1; break; }
        g_gcc_be_oom_countdown = -1;
    }

    a_type arr_tref_null = {};
    arr_tref_null.kind = tk_array; arr_tref_null.size = 4; arr_tref_null.variant.array.element_type = &tref_null; arr_tref_null.variant.array.variant.number_of_elements = 1;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&arr_tref_null, cls));

    /* Struct with bases (direct, indirect, null type), arrays, bitfields straddling 8B (`offset=7, bit_size=16`) & >= 128b */
    a_base_class bc_dir = {}, bc_indir = {}, bc_null = {}, bc_empty = {};
    bc_dir.direct = TRUE; bc_dir.type = &t_f4; bc_dir.offset = 0; bc_dir.next = &bc_indir;
    bc_indir.direct = FALSE; bc_indir.type = &t_f4; bc_indir.next = &bc_null;
    bc_null.direct = TRUE; bc_null.type = NULL; bc_null.next = &bc_empty;
    bc_empty.direct = TRUE; bc_empty.type = &st_empty0;

    a_type arr_f2 = {};
    arr_f2.kind = tk_array; arr_f2.size = 8; arr_f2.variant.array.element_type = &t_f4; arr_f2.variant.array.variant.number_of_elements = 2;

    a_field sf_arr = {}, sf_null = {}, sf_off16 = {}, sf_bf_past = {}, sf_bf_cross8 = {}, sf_bf_cross16 = {};
    sf_arr.type = &arr_f2; sf_arr.offset = 4; sf_arr.next = &sf_null;
    sf_null.type = &tref_null; sf_null.offset = 12; sf_null.next = &sf_off16;
    sf_off16.type = &t_i4; sf_off16.offset = 16; sf_off16.next = &sf_bf_cross8;
    sf_bf_cross8.type = &t_i4; sf_bf_cross8.is_bit_field = TRUE; sf_bf_cross8.offset = 7; sf_bf_cross8.bit_size = 16; sf_bf_cross8.next = &sf_bf_cross16;
    sf_bf_cross16.type = &t_i4; sf_bf_cross16.is_bit_field = TRUE; sf_bf_cross16.offset = 15; sf_bf_cross16.bit_size = 16; sf_bf_cross16.next = &sf_bf_past;
    sf_bf_past.type = &t_i4; sf_bf_past.is_bit_field = TRUE; sf_bf_past.offset = 16; sf_bf_past.bit_size = 4;

    a_class_type_supplement csup = {};
    csup.base_classes = &bc_dir;
    a_type st_nested = {};
    st_nested.kind = tk_class; st_nested.size = 16;
    st_nested.variant.class_struct_union.extra_info = &csup;
    st_nested.variant.class_struct_union.field_list = &sf_arr;

    for (int b = 0; b < 45; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_classify_sysv_amd64(&st_nested, cls);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* 2. HFA/HVA & flatten_riscv64 edge cases */
    /* HFA base failure, field failure, array len <= 0, array len > max, array elem failure, complex HFA, vector HFA (8 & 16 & size/kind mismatch & >max) */
    a_base_class hbase_bad = {}, hbase_ok = {};
    a_type st_int_base = {};
    st_int_base.kind = tk_struct; st_int_base.size = 4; st_int_base.variant.class_struct_union.field_list = &sf_off16; /* t_i4 -> non-HFA */
    hbase_bad.direct = TRUE; hbase_bad.type = &t_i4;
    a_class_type_supplement hsup_bad = {};
    hsup_bad.base_classes = &hbase_bad;
    a_type hst_bad_base = {};
    hst_bad_base.kind = tk_class; hst_bad_base.size = 8; hst_bad_base.variant.class_struct_union.extra_info = &hsup_bad;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&hst_bad_base, &b1));

    a_type tref_f4 = {};
    tref_f4.kind = tk_typeref; tref_f4.variant.typeref.type = &t_f4;
    hbase_ok.direct = TRUE; hbase_ok.type = &tref_f4; hbase_ok.next = &bc_indir;
    a_class_type_supplement hsup_ok = {};
    hsup_ok.base_classes = &hbase_ok;
    a_field hf_mismatch_sz = {}, hf_null = {};
    hf_mismatch_sz.type = &t_f8; /* f4 base + f8 field -> elem_size mismatch! */
    hf_null.type = &tref_null;
    a_type hst_mismatch = {};
    hst_mismatch.kind = tk_struct; hst_mismatch.size = 16;
    hst_mismatch.variant.class_struct_union.extra_info = &hsup_ok;
    hst_mismatch.variant.class_struct_union.field_list = &hf_mismatch_sz;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&hst_mismatch, &b1));
    hst_mismatch.variant.class_struct_union.field_list = &hf_null;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&hst_mismatch, &b1));

    /* Same size (16B) different float_kind (`fk_long_double` vs `fk_float128`) -> elem_kind mismatch! */
    a_field hf_ld1 = {}, hf_f128 = {};
    hf_ld1.type = &t_ld16; hf_ld1.next = &hf_f128;
    hf_f128.type = &t_f128;
    a_type hst_kind_mm = {};
    hst_kind_mm.kind = tk_struct; hst_kind_mm.size = 32; hst_kind_mm.variant.class_struct_union.field_list = &hf_ld1;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&hst_kind_mm, &b1));

    /* Complex HFA (2 x complex float = 4 floats <= 4; 3 x complex float = 6 > 4!) */
    a_type arr_cf2 = {}, arr_cf3 = {}, arr_zero = {}, arr_int2 = {};
    arr_cf2.kind = tk_array; arr_cf2.size = 16; arr_cf2.variant.array.element_type = &t_cf4; arr_cf2.variant.array.variant.number_of_elements = 2;
    arr_cf3.kind = tk_array; arr_cf3.size = 24; arr_cf3.variant.array.element_type = &t_cf4; arr_cf3.variant.array.variant.number_of_elements = 3;
    arr_zero.kind = tk_array; arr_zero.size = 0; arr_zero.variant.array.element_type = &t_f4; arr_zero.variant.array.variant.number_of_elements = 0;
    arr_int2.kind = tk_array; arr_int2.size = 8; arr_int2.variant.array.element_type = &t_i4; arr_int2.variant.array.variant.number_of_elements = 2;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&arr_cf2, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&arr_cf3, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&arr_zero, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&arr_int2, &b1));

    /* Short-vector HVA: 2 x v8, v8 + v16 mismatch, f8 + v8 mismatch (elem_size==8 == sz, elem_kind != kind!), and 5-field v8 struct (>4) */
    a_field vf1 = {}, vf2 = {}, vf3 = {}, vf4 = {}, vf5 = {};
    vf1.type = &t_v8; vf1.next = &vf2; vf2.type = &t_v8;
    a_type hva2 = {};
    hva2.kind = tk_struct; hva2.size = 16; hva2.variant.class_struct_union.field_list = &vf1;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&hva2, &b1));
    vf2.type = &t_v16;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&hva2, &b1));
    vf1.type = &t_f8; vf2.type = &t_v8; /* both 8 bytes, different kind! */
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&hva2, &b1));
    vf1.type = &t_v8; vf2.type = &t_v8; vf2.next = &vf3; vf3.type = &t_v8; vf3.next = &vf4; vf4.type = &t_v8; vf4.next = &vf5; vf5.type = &t_v8;
    hva2.size = 40;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&hva2, &b1));
    /* 2 x v16 HVA (valid 16-byte vector HVA!) + nested 5-element array inside struct (`arr_len > max_elements` with `arr_len > 0`!) + empty 1-byte C++ struct (`count == 0`!) */
    vf1.type = &t_v16; vf2.type = &t_v16; vf2.next = NULL; hva2.size = 32;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&hva2, &b1));
    /* 32-byte vector inside struct -> hits t->kind == tk_vector && (sz == 8 || sz == 16) == False! */
    a_field vf_32 = {}; vf_32.type = &t_v32;
    a_type hva_v32 = {}; hva_v32.kind = tk_struct; hva_v32.size = 32; hva_v32.variant.class_struct_union.field_list = &vf_32;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&hva_v32, &b1));
    vf1.type = &t_v8; vf2.type = &t_v8; hva2.size = 16;

    a_type arr_f5 = {}, st_empty_cpp1 = {};
    arr_f5.kind = tk_array; arr_f5.size = 20; arr_f5.variant.array.element_type = &t_f4; arr_f5.variant.array.variant.number_of_elements = 5;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&arr_f5, &b1));
    st_empty_cpp1.kind = tk_struct; st_empty_cpp1.size = 1; /* field_list == NULL -> valid==TRUE, count==0! */
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(&st_empty_cpp1, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_ppc64le(&st_empty_cpp1, &b1, &b2));

    for (int b = 0; b < 15; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_classify_aapcs64(&hst_mismatch, &b1);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* Win64 sizes 1, 2, 4, 8, and 3 */
    a_type t_i3 = {}; t_i3.kind = tk_struct; t_i3.size = 3;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_win64(&t_i1, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_win64(&t_i2, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_win64(&t_i4, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_win64(&t_i8, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_win64(&t_i3, &b1));

    /* RISC-V flatten: base, zero-bitfield, arrays (0, 2, 3), pointer leaf, >8B float/int, 3+ fields, non-struct */
    a_field rvf_zbf = {}, rvf_ptr = {}, rvf_f4 = {}, rvf_ld = {}, rvf_i16 = {};
    rvf_zbf.type = &t_i4; rvf_zbf.is_bit_field = TRUE; rvf_zbf.bit_size = 0; rvf_zbf.next = &rvf_ptr;
    rvf_ptr.type = &t_ptr; rvf_ptr.next = NULL;
    a_type rv_base_ptr_st = {};
    rv_base_ptr_st.kind = tk_class; rv_base_ptr_st.size = 16;
    rv_base_ptr_st.variant.class_struct_union.extra_info = &hsup_ok; /* f4 base + ptr field -> fp=1, int=1! */
    rv_base_ptr_st.variant.class_struct_union.field_list = &rvf_zbf;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&t_f4, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&t_f4, &b1, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&t_ld16, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&rv_base_ptr_st, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&rv_base_ptr_st, &b1, NULL));

    a_field rvf_arr2 = {}, rvf_arr0 = {};
    rvf_arr2.type = &arr_f2;
    rvf_arr0.type = &arr_zero;
    a_type rv_arr2_st = {}, rv_arr0_st = {}, rv_3fld_st = {}, rv_bigleaf_st = {};
    rv_arr2_st.kind = tk_struct; rv_arr2_st.size = 8; rv_arr2_st.variant.class_struct_union.field_list = &rvf_arr2;
    rv_arr0_st.kind = tk_struct; rv_arr0_st.size = 4; rv_arr0_st.variant.class_struct_union.field_list = &rvf_arr0;
    rvf_f4.type = &t_f4; rvf_f4.next = &rvf_ld; rvf_ld.type = &t_ld16; rvf_ld.next = &rvf_i16; rvf_i16.type = &t_i16;
    rv_3fld_st.kind = tk_struct; rv_3fld_st.size = 16; rv_3fld_st.variant.class_struct_union.field_list = &vf1;
    vf1.next = &vf2; vf2.next = &vf3;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&rv_arr2_st, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&rv_arr0_st, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&rv_3fld_st, &b1, &b2));
    vf2.next = NULL;
    rvf_ld.next = NULL;
    rv_bigleaf_st.kind = tk_struct; rv_bigleaf_st.size = 16; rv_bigleaf_st.variant.class_struct_union.field_list = &rvf_f4;
    for (int b = 0; b < 10; ++b) {
        g_gcc_be_oom_countdown = b;
        if (gcc_gen_be_classify_riscv64(&rv_arr2_st, &b1, &b2) == GCC_GEN_BE_SUCCESS) { g_gcc_be_oom_countdown = -1; break; }
        g_gcc_be_oom_countdown = -1;
    }

    /* 2-field { float, v8 } struct + tref_null/nonzero-bf/arr_f5/16B-int structs on RISC-V */
    a_field rvf_fv1 = {}, rvf_fv2 = {}, rvf_nzbf = {}, rvf_tnull = {}, rvf_a5 = {}, rvf_only_i16 = {};
    rvf_fv1.type = &t_f4; rvf_fv1.next = &rvf_fv2; rvf_fv2.type = &t_v8;
    a_type rv_fv_st = {};
    rv_fv_st.kind = tk_struct; rv_fv_st.size = 16; rv_fv_st.variant.class_struct_union.field_list = &rvf_fv1;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&rv_fv_st, &b1, &b2));

    rvf_nzbf.type = &t_i4; rvf_nzbf.is_bit_field = TRUE; rvf_nzbf.bit_size = 4; rvf_nzbf.next = &rvf_tnull;
    rvf_tnull.type = &tref_null;
    a_type rv_nzbf_st = {}, rv_a5_st = {}, rv_i16_st = {};
    rv_nzbf_st.kind = tk_struct; rv_nzbf_st.size = 8; rv_nzbf_st.variant.class_struct_union.field_list = &rvf_nzbf;
    rvf_a5.type = &arr_f5;
    rv_a5_st.kind = tk_struct; rv_a5_st.size = 16; rv_a5_st.variant.class_struct_union.field_list = &rvf_a5;
    rvf_only_i16.type = &t_i16;
    rv_i16_st.kind = tk_struct; rv_i16_st.size = 16; rv_i16_st.variant.class_struct_union.field_list = &rvf_only_i16;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&rv_nzbf_st, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&rv_a5_st, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&rv_i16_st, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&rv_bigleaf_st, &b1, &b2));

    for (int b = 0; b < 15; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_classify_riscv64(&rv_base_ptr_st, &b1, &b2);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* i386, PPC64LE (>16 non-HFA sret & scalar), s390x (1/2/4/8, 1-field float struct with & without out_is_fp_reg, 1-field int struct), Darwin ARM64, AAPCS32 */
    a_type u_24 = {}, st_1f4 = {}, st_1i4 = {};
    u_24.kind = tk_union; u_24.size = 24;
    a_field one_f4 = {}, one_i4 = {};
    one_f4.type = &t_f4;
    one_i4.type = &t_i4;
    st_1f4.kind = tk_struct; st_1f4.size = 4; st_1f4.variant.class_struct_union.field_list = &one_f4;
    st_1i4.kind = tk_class; st_1i4.size = 4; st_1i4.variant.class_struct_union.field_list = &one_i4;

    GCC_GEN_BE_CHECK(gcc_gen_be_classify_i386(&t_i4, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_i386(&u_24, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_ppc64le(&t_i4, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_ppc64le(&u_24, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_ppc64le(&arr_zero, &b1, &b2));
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_classify_ppc64le(&st_1f4, &b1, &b2);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    a_field two_i4_a = {}, two_i4_b = {};
    two_i4_a.type = &t_i4; two_i4_a.next = &two_i4_b; two_i4_b.type = &t_i4; two_i4_b.offset = 4;
    a_type st_2i4 = {};
    st_2i4.kind = tk_struct; st_2i4.size = 8; st_2i4.variant.class_struct_union.field_list = &two_i4_a;

    GCC_GEN_BE_CHECK(gcc_gen_be_classify_s390x(&t_i1, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_s390x(&t_i2, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_s390x(&t_i8, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_s390x(&st_2i4, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_s390x(&st_1f4, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_s390x(&st_1f4, &b1, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_s390x(&st_1i4, &b1, &b2));
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_classify_s390x(&st_1f4, &b1, &b2);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_classify_darwin_arm64(&t_i4, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_darwin_arm64(&arr_f5, &b1)); /* >16B non-HFA array */
    a_type cls_24 = {}, st_24 = {};
    cls_24.kind = tk_class; cls_24.size = 24;
    st_24.kind = tk_struct; st_24.size = 24;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_darwin_arm64(&cls_24, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_darwin_arm64(&st_24, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_darwin_arm64(&u_24, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_darwin_arm64(&st_1f4, &b1));
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_classify_darwin_arm64(&st_1f4, &b1);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs32(&t_i4, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs32(&t_i8, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs32(&st_1f4, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs32(&u_24, &b1, NULL));

    /* 3. invisible_reference, sret, and va_arg across ALL target ABIs + OOM sweeps */
    a_type vbase_cls = {};
    vbase_cls.kind = tk_class; vbase_cls.size = 4; vbase_cls.variant.class_struct_union.any_virtual_base_classes = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_passed_by_invisible_reference(&t_i4, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(&t_i4, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_is_passed_by_invisible_reference(&vbase_cls, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(&vbase_cls, &b1));
    g_trivially_copyable = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_passed_by_invisible_reference(&st_1i4, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(&st_1i4, &b1));
    g_trivially_copyable = TRUE;

    const gcc_gen_be_target_abi_t all_abis[] = {
        GCC_GEN_BE_TARGET_ABI_SYSV_AMD64,
        GCC_GEN_BE_TARGET_ABI_AAPCS64,
        GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64,
        GCC_GEN_BE_TARGET_ABI_WIN64,
        GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D,
        GCC_GEN_BE_TARGET_ABI_SYSV_I386,
        GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2,
        GCC_GEN_BE_TARGET_ABI_S390X,
        GCC_GEN_BE_TARGET_ABI_AAPCS32
    };
    a_type tref_st1i4 = {};
    tref_st1i4.kind = tk_typeref; tref_st1i4.variant.typeref.type = &st_1i4;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_passed_by_invisible_reference(&tref_st1i4, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(&tref_st1i4, &b1));

    a_type *probe_types[] = { &st_1f4, &st_1i4, &u_24, &t_f4, &t_v16, &t_cf4, &t_ld16, &t_cld32, &t_i8, &t_i16 };
    for (size_t ai = 0; ai < sizeof(all_abis) / sizeof(all_abis[0]); ++ai) {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(all_abis[ai]));
        for (size_t ti = 0; ti < sizeof(probe_types) / sizeof(probe_types[0]); ++ti) {
            for (int b = 0; b < 15; ++b) {
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t e = gcc_gen_be_is_passed_by_invisible_reference(probe_types[ti], &b1);
                g_gcc_be_oom_countdown = -1;
                if (e == GCC_GEN_BE_SUCCESS) break;
            }
            for (int b = 0; b < 15; ++b) {
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t e = gcc_gen_be_is_returned_via_sret(probe_types[ti], &b1);
                g_gcc_be_oom_countdown = -1;
                if (e == GCC_GEN_BE_SUCCESS) break;
            }
            GCC_GEN_BE_CHECK(gcc_gen_be_classify_va_arg(probe_types[ti], &b1, NULL, NULL));
            for (int b = 0; b < 15; ++b) {
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t e = gcc_gen_be_classify_va_arg(probe_types[ti], &b1, &gp, &fp);
                g_gcc_be_oom_countdown = -1;
                if (e == GCC_GEN_BE_SUCCESS) break;
            }
        }
    }

    /* 4. Coerced ABI types (aapcs64, riscv64, ppc64le, get_coerced_abi_type, coerce_rvalue, pack/unpack) */
    gcc_jit_type *ct = NULL;
    if (gcc_gen_be_coerced_aapcs64_type(&st_1f4, NULL, &b1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_aapcs64_type(&st_1f4, &ct, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_riscv64_type(&st_1f4, NULL, &b1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_riscv64_type(&st_1f4, &ct, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_ppc64le_type(&st_1f4, NULL, &b1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_ppc64le_type(&st_1f4, &ct, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_coerced_abi_type(&st_1f4, NULL, &b1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_coerced_abi_type(&st_1f4, &ct, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_type *vt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *it = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, vt, "cf", 0, NULL, 0);
    gcc_jit_block *blk = gcc_jit_function_new_block(fn, "cb");
    gcc_jit_rvalue *irv = gcc_jit_context_new_rvalue_from_int(ctx, it, 1);
    gcc_jit_rvalue *orv = NULL;
    gcc_jit_lvalue *olv = NULL;

    /* Missing context == NULL checks */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_coerced_aapcs64_type(&st_1f4, &ct, &b1) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_riscv64_type(&st_1f4, &ct, &b1) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_ppc64le_type(&st_1f4, &ct, &b1) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_coerced_abi_type(&st_1f4, &ct, &b1) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerce_rvalue(fn, blk, irv, it, &orv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* HVA (elem_kind >= 200), float (4), double (8), long double (16), GP 1-reg (4), GP 2-reg (16), >16 (24), >64/>128 (200), 0-size, scalar */
    a_field ld1_fld = {}; ld1_fld.type = &t_ld16;
    a_type hfa_ld = {}, st_16gp = {}, st_200 = {}, u_8 = {}, u_16 = {};
    hfa_ld.kind = tk_struct; hfa_ld.size = 16; hfa_ld.variant.class_struct_union.field_list = &ld1_fld;
    a_field i8_a = {}, i8_b = {};
    i8_a.type = &t_i8; i8_a.offset = 0; i8_a.next = &i8_b; i8_b.type = &t_i8; i8_b.offset = 8;
    st_16gp.kind = tk_struct; st_16gp.size = 16; st_16gp.variant.class_struct_union.field_list = &i8_a;
    st_200.kind = tk_struct; st_200.size = 200;
    u_8.kind = tk_union; u_8.size = 8; u_8.variant.class_struct_union.field_list = &i8_a;
    u_16.kind = tk_union; u_16.size = 16; u_16.variant.class_struct_union.field_list = &i8_a;

    a_type *coerce_shapes[] = {
        &t_i4, &st_empty0, &st_empty_cpp1, &st_200, &hva2, &st_1f4, &arr_cf2, &hfa_ld,
        &st_1i4, &st_16gp, &u_8, &u_16, &u_24, &rv_base_ptr_st, &rv_fv_st, &t_ld16, &t_cld32, &t_v32, &t_i1, &t_i2
    };
    const gcc_gen_be_target_abi_t coerce_abis[] = {
        GCC_GEN_BE_TARGET_ABI_SYSV_AMD64,
        GCC_GEN_BE_TARGET_ABI_AAPCS64,
        GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64,
        GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D,
        GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2,
        GCC_GEN_BE_TARGET_ABI_WIN64
    };
    for (size_t ai = 0; ai < sizeof(coerce_abis) / sizeof(coerce_abis[0]); ++ai) {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(coerce_abis[ai]));
        for (size_t si = 0; si < sizeof(coerce_shapes) / sizeof(coerce_shapes[0]); ++si) {
            for (int b = 0; b < 20; ++b) {
                GCC_GEN_BE_CHECK(cache_clear_all());
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t e = gcc_gen_be_get_coerced_abi_type(coerce_shapes[si], &ct, &b1);
                g_gcc_be_oom_countdown = -1;
                if (e == GCC_GEN_BE_SUCCESS) break;
            }
            /* Second lookup hits coerced cache when b1 == TRUE */
            GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(coerce_shapes[si], &ct, &b1));
        }
    }
    /* Win64 1-byte and 2-byte struct coercion */
    a_type st_1b = {}, st_2b = {};
    st_1b.kind = tk_struct; st_1b.size = 1;
    st_2b.kind = tk_class; st_2b.size = 2;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_WIN64));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(&st_1b, &ct, &b1));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(&st_2b, &ct, &b1));

    /* Direct OOM sweeps on aapcs64/riscv64/ppc64le scalar/oversize paths */
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        if (gcc_gen_be_coerced_aapcs64_type(&t_i4, &ct, &b1) == GCC_GEN_BE_SUCCESS) { g_gcc_be_oom_countdown = -1; break; }
        g_gcc_be_oom_countdown = -1;
    }
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        if (gcc_gen_be_coerced_aapcs64_type(&st_200, &ct, &b1) == GCC_GEN_BE_SUCCESS) { g_gcc_be_oom_countdown = -1; break; }
        g_gcc_be_oom_countdown = -1;
    }
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        if (gcc_gen_be_coerced_ppc64le_type(&t_i4, &ct, &b1) == GCC_GEN_BE_SUCCESS) { g_gcc_be_oom_countdown = -1; break; }
        g_gcc_be_oom_countdown = -1;
    }
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        if (gcc_gen_be_coerced_ppc64le_type(&st_200, &ct, &b1) == GCC_GEN_BE_SUCCESS) { g_gcc_be_oom_countdown = -1; break; }
        g_gcc_be_oom_countdown = -1;
    }

    /* Invalid args & OOM sweeps on coerce_rvalue, coerce_Arg_to_registers, unpack_coerced_Arg */
    if (gcc_gen_be_coerce_rvalue(fn, NULL, irv, it, &orv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerce_rvalue(fn, blk, NULL, it, &orv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerce_rvalue(fn, blk, irv, NULL, &orv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerce_rvalue(fn, blk, irv, it, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_coerce_Arg_to_registers(fn, NULL, &st_1i4, irv, &orv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerce_Arg_to_registers(fn, blk, NULL, irv, &orv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerce_Arg_to_registers(fn, blk, &st_1i4, NULL, &orv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerce_Arg_to_registers(fn, blk, &st_1i4, irv, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_unpack_coerced_Arg(fn, NULL, &st_1i4, irv, &olv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_unpack_coerced_Arg(fn, blk, NULL, irv, &olv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_unpack_coerced_Arg(fn, blk, &st_1i4, NULL, &olv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_unpack_coerced_Arg(fn, blk, &st_1i4, irv, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    g_lower_type_return_null = true;
    if (gcc_gen_be_unpack_coerced_Arg(fn, blk, &st_1i4, irv, &olv) != GCC_GEN_BE_ERROR_COERCION_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    g_lower_type_return_null = false;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_SYSV_AMD64));
    for (int b = 0; b < 15; ++b) {
        GCC_GEN_BE_CHECK(cache_clear_all());
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_coerce_Arg_to_registers(fn, blk, &st_16gp, irv, &orv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 10; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_unpack_coerced_Arg(fn, blk, &st_16gp, irv, &olv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* 5. Cumulative register allocation exhaustion & edge cases across all ABIs */
    gcc_gen_be_abi_reg_state_t rst = {};
    if (gcc_gen_be_init_abi_reg_state((gcc_gen_be_target_abi_t)-1, FALSE, &rst) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    for (size_t ai = 0; ai < sizeof(all_abis) / sizeof(all_abis[0]); ++ai) {
        GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(all_abis[ai], FALSE, &rst));
        GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(all_abis[ai], TRUE, &rst));
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &tref_null, &b1, &b2));
    g_trivially_copyable = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &st_1i4, &b1, &b2));
    g_trivially_copyable = TRUE;

    /* Non-trivial / vbase exhausted GP slots */
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_SYSV_I386, FALSE, &rst));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &vbase_cls, &b1, &b2));

    /* Win64 exhaustion */
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_WIN64, FALSE, &rst));
    for (int i = 0; i < 6; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &t_i8, &b1, &b2));
    }
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_allocate_abi_param_regs(&rst, &t_i8, &b1, &b2) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;

    /* AAPCS64 / Darwin ARM64 HFA fit & overflow, >16B class/array fit & overflow, GP 1-reg/2-reg fit & overflow, OOM */
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64, FALSE, &rst));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &cls_24, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &arr_f5, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &t_i4, &b1, &b2));
    for (int i = 0; i < 4; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &arr_cf2, &b1, &b2));
    }
    for (int i = 0; i < 5; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &st_16gp, &b1, &b2));
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &u_24, &b1, &b2)); /* gp already at limit! */
    for (int b = 0; b < 8; ++b) {
        GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_AAPCS64, FALSE, &rst));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_allocate_abi_param_regs(&rst, &arr_cf2, &b1, &b2);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* AAPCS32: 0-size (`words < 1`), non-even-odd, OOM */
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_AAPCS32, FALSE, &rst));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &st_empty0, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &t_i8, &b1, &b2));
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_allocate_abi_param_regs(&rst, &t_i4, &b1, &b2) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;

    /* RISC-V: >16B with exhausted GP, FP exhaustion -> falls back to GP, 2-reg GP, GP exhaustion, OOM */
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D, FALSE, &rst));
    for (int i = 0; i < 10; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &t_f8, &b1, &b2));
    }
    for (int i = 0; i < 5; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &st_16gp, &b1, &b2));
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &u_24, &b1, &b2));
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_allocate_abi_param_regs(&rst, &t_f8, &b1, &b2) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;

    /* PPC64LE: HFA exhaustion, 0-size (`needed_gp < 1`), GP exhaustion, OOM */
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2, FALSE, &rst));
    for (int i = 0; i < 6; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &arr_cf2, &b1, &b2));
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &st_empty0, &b1, &b2));
    for (int i = 0; i < 10; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &t_i8, &b1, &b2));
    }
    for (int b = 0; b < 8; ++b) {
        GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2, FALSE, &rst));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_allocate_abi_param_regs(&rst, &arr_cf2, &b1, &b2);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* SysV AMD64 param regs: MEMORY, X87, COMPLEX_X87, SSE+SSEUP (t_v16 -> classes[1]=SSEUP), FP exhaustion, OOM */
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_SYSV_AMD64, FALSE, &rst));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &u_24, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &t_ld16, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &t_cld32, &b1, &b2));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &t_v16, &b1, &b2));
    for (int i = 0; i < 9; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&rst, &t_f8, &b1, &b2));
    }
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_allocate_abi_param_regs(&rst, &t_f8, &b1, &b2) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes all ABI unit tests.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_abi_null_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    GCC_GEN_BE_CHECK(test_abi_attributes(ctx));
    GCC_GEN_BE_CHECK(test_all_abi_classifiers_and_coercion(ctx));

    /* Cover remaining Stage-2 visibility attribute branches in gcc_gen_be_abi.c */
    {
        gcc_jit_type *vt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_function *afn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt, "abi_vis_extra_fn", 0, NULL, 0);
        gcc_jit_lvalue *agv = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_EXPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "abi_vis_extra_gv");

        a_constant c_int_vis = {}, c_str_vis = {};
        c_int_vis.kind = ck_integer;
        c_str_vis.kind = ck_string;
        c_str_vis.variant.string.value = "hidden";

        an_attribute_arg aarg_nonconst = {}, aarg_intconst = {}, aarg_str = {};
        aarg_nonconst.kind = static_cast<an_attribute_arg_kind>(aak_type);
        aarg_intconst.kind = aak_constant; aarg_intconst.variant.constant = &c_int_vis;
        aarg_str.kind = aak_constant; aarg_str.variant.constant = &c_str_vis;

        an_attribute at_null_arg = {}, at_nonconst = {}, at_intconst = {}, at_valid = {};
        at_null_arg.kind = ak_visibility; at_null_arg.arguments = NULL; at_null_arg.next = &at_nonconst;
        at_nonconst.kind = ak_visibility; at_nonconst.arguments = &aarg_nonconst; at_nonconst.next = &at_intconst;
        at_intconst.kind = ak_visibility; at_intconst.arguments = &aarg_intconst; at_intconst.next = &at_valid;
        at_valid.kind = ak_visibility; at_valid.arguments = &aarg_str;

        a_routine vr = {};
        vr.function_def_number = 1;
        vr.storage_class = sc_unspecified;
        vr.source_corresp.attributes = &at_null_arg;

        /* 1) p_gcc_jit_function_add_string_attribute == NULL (covers 43:81 False & 77:46 False) */
        auto saved_fn_str = p_gcc_jit_function_add_string_attribute;
        p_gcc_jit_function_add_string_attribute = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(&vr, afn));

        /* 2) p_gcc_jit_function_add_string_attribute != NULL (covers 45:17 F, 45:24 F, 45:53 F, 45:53 T) + evk_protected (79:30 T) */
        p_gcc_jit_function_add_string_attribute = [](struct gcc_jit_function *, enum gcc_jit_fn_attribute, const char *) noexcept {};
        GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(&vr, afn));
        vr.source_corresp.attributes = NULL;
        vr.ELF_visibility = evk_protected;
        GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(&vr, afn));
        p_gcc_jit_function_add_string_attribute = saved_fn_str;

        /* 3) p_gcc_jit_lvalue_add_string_attribute == NULL and static variable on ak_visibility (covers 158:51 False) */
        a_variable vv = {};
        vv.storage_class = sc_static;
        vv.source_corresp.attributes = &at_valid;
        GCC_GEN_BE_CHECK(gcc_gen_be_apply_variable_attributes(&vv, agv));
        vv.storage_class = sc_unspecified;
        auto saved_lv_str = p_gcc_jit_lvalue_add_string_attribute;
        p_gcc_jit_lvalue_add_string_attribute = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_apply_variable_attributes(&vv, agv));
        p_gcc_jit_lvalue_add_string_attribute = saved_lv_str;
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_abi.cpp`.
 * @return 0 on success, non-zero on failure.
 */
int main(void) noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_abi_null_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
