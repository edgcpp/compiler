/**
 * @file test_gcc_be_type.cpp
 * @brief Exhaustive unit tests for type lowering, zero rvalues, decimal floats, fixed-point, and `_BitInt` (`gcc_gen_be_type.c`).
 * @details Exercises every scalar, pointer, array/VLA, struct/class/union/bitfield/base/deferred-cycle,
 * routine/sret/coerced, pointer-to-member, vector, complex/imaginary, decimal float, fixed-point,
 * and `_BitInt` path plus deterministic OOM sweeps in `src/gcc_gen_be_type.c`.
 * @ingroup gcc_gen_be_tests
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for type unit tests.
 * @param[in] err The error code.
 * @param[in] msg Optional diagnostic message.
 * @return The input error code `err`.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char *msg) GCC_GEN_BE_NOEXCEPT {
    return msg ? err : err;
}
}

USING_NAMESPACE_EDG

/** @brief Controls `gcc_gen_be_is_returned_via_sret` stub output. */
static a_boolean g_stub_sret = FALSE;
/** @brief Controls `gcc_gen_be_is_passed_by_invisible_reference` stub output. */
static a_boolean g_stub_invis_ref = FALSE;
/** @brief Controls `gcc_gen_be_get_coerced_abi_type` stub output. */
static a_boolean g_stub_coerced = FALSE;
/** @brief Coerced `gcc_jit_type *` returned when `g_stub_coerced` is TRUE. */
static gcc_jit_type *g_stub_coerced_type = NULL;

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

/**
 * @brief Stub for getting source location from type in type unit tests.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or OOM on injected fault.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_type(a_type_ptr tp, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return tp ? GCC_GEN_BE_ERROR_INVALID_ARGUMENT : GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for emitting type debug info in type unit tests.
 * @param[in] tp The frontend type pointer.
 * @param[in] type The JIT type pointer.
 * @return GCC_GEN_BE_SUCCESS on success, or OOM on injected fault.
 */
gcc_gen_be_error_t gcc_gen_be_emit_type_debug_info(a_type_ptr tp, struct gcc_jit_type *type) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return (tp || type) ? GCC_GEN_BE_SUCCESS : GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for checking sret return in type unit tests.
 * @param[in] type The frontend type pointer.
 * @param[out] out_is_sret Pointer to receive the sret flag.
 * @return GCC_GEN_BE_SUCCESS on success, or OOM on injected fault.
 */
gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_is_sret) *out_is_sret = type ? g_stub_sret : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for checking invisible-reference parameter passing in type unit tests.
 * @param[in] type The frontend type pointer.
 * @param[out] out_is_ref Pointer to receive the invisible-reference flag.
 * @return GCC_GEN_BE_SUCCESS on success, or OOM on injected fault.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_is_ref) *out_is_ref = type ? g_stub_invis_ref : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for coerced ABI type lookup in type unit tests.
 * @param[in] type The frontend type pointer.
 * @param[out] out_coerced_type Pointer to receive the coerced JIT type.
 * @param[out] out_needs_coercion Pointer to receive the coercion flag.
 * @return GCC_GEN_BE_SUCCESS on success, or OOM on injected fault.
 */
gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(
    a_type_ptr type,
    struct gcc_jit_type **out_coerced_type,
    a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_coerced_type) *out_coerced_type = type ? g_stub_coerced_type : NULL;
    if (out_needs_coercion) *out_needs_coercion = type ? g_stub_coerced : FALSE;
    return GCC_GEN_BE_SUCCESS;
}
END_EDG_NAMESPACE

/**
 * @brief Mock `gcc_jit_context_get_int_type` returning NULL when `g_mock_int_type_null` is true.
 */
static bool g_mock_int_type_null = false;

/**
 * @brief Mock `gcc_jit_context_get_int_type` callback for testing NULL fallback paths.
 * @param[in] ctx Active context.
 * @param[in] num_bytes Byte width.
 * @param[in] is_signed Signedness flag.
 * @return `gcc_jit_type *` or `nullptr`.
 */
static gcc_jit_type *mock_get_int_type(gcc_jit_context *ctx, int num_bytes, int is_signed) noexcept {
    if (g_mock_int_type_null) return nullptr;
    return gcc_jit_context_get_int_type(ctx, num_bytes, is_signed);
}

/**
 * @brief Mock `gcc_jit_context_new_rvalue_from_vector` callback for vector zero-rvalue testing.
 * @param[in] ctx Active context.
 * @param[in] loc Optional location.
 * @param[in] vec_type Vector type.
 * @param[in] num_elements Lane count.
 * @param[in] elements Lane array.
 * @return Zero vector rvalue.
 */
static gcc_jit_rvalue *mock_type_vec_rval(
    gcc_jit_context *ctx,
    gcc_jit_location *loc,
    gcc_jit_type *vec_type,
    size_t num_elements,
    gcc_jit_rvalue **elements) noexcept {
    if (!ctx || !vec_type || num_elements == 0 || !elements) return loc ? nullptr : nullptr;
    return gcc_jit_context_zero(ctx, vec_type);
}

/**
 * @brief Exercises all scalar, integer, `_BitInt`, fixed-point, float, complex, pointer, array, routine, vector, and zero-rvalue paths.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_scalars_and_derived_types(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_type *jt = NULL;
    gcc_jit_rvalue *rv = NULL;

    /* Missing context error paths */
    a_type dummy_int = {};
    dummy_int.kind = tk_integer; dummy_int.size = 4; dummy_int.variant.integer.int_kind = ik_int;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_type(&dummy_int, &jt) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_zero_rvalue(&dummy_int, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_type(4, &jt) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* Void, nullptr, bool, all integer kinds, int128 (with pfn non-null, returning null, and pfn == NULL) */
    a_type t_void = {}, t_nullp = {}, t_bool = {}, t_err = {}, t_unk = {};
    t_void.kind = tk_void;
    t_nullp.kind = tk_nullptr;
    t_bool.kind = tk_integer; t_bool.variant.integer.bool_type = TRUE;
    t_err.kind = tk_error;
    t_unk.kind = (a_type_kind)250;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&t_void, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&t_nullp, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&t_bool, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&t_err, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&t_unk, &jt));

    const an_integer_kind iks[] = {
        ik_char, ik_signed_char, ik_unsigned_char, ik_short, ik_unsigned_short,
        ik_int, ik_unsigned_int, ik_long, ik_unsigned_long, ik_long_long, ik_unsigned_long_long,
        ik_int128, ik_unsigned_int128, (an_integer_kind)250
    };
    p_gcc_jit_context_get_int_type = &mock_get_int_type;
    g_mock_int_type_null = false;
    for (size_t i = 0; i < sizeof(iks) / sizeof(iks[0]); ++i) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
        a_type ti = {};
        ti.kind = tk_integer;
        ti.variant.integer.int_kind = iks[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&ti, &jt));
    }
    g_mock_int_type_null = true;
    a_type ti128_s = {}, ti128_u = {};
    ti128_s.kind = tk_integer; ti128_s.variant.integer.int_kind = ik_int128;
    ti128_u.kind = tk_integer; ti128_u.variant.integer.int_kind = ik_unsigned_int128;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&ti128_s, &jt));
    p_gcc_jit_context_get_int_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&ti128_u, &jt));

    /* _BitInt across all widths (0, signed 1, unsigned 1, 8, 12, 24, 48, 96, 256, 1024) & fallback when p_gcc_jit_context_get_int_type == NULL */
    const unsigned long bp_widths[] = { 1, 8, 12, 24, 48, 96, 256 };
    for (int sgn = 0; sgn <= 1; ++sgn) {
        for (int pfn_mode = 0; pfn_mode <= 2; ++pfn_mode) {
            p_gcc_jit_context_get_int_type = (pfn_mode == 0) ? NULL : &mock_get_int_type;
            g_mock_int_type_null = (pfn_mode == 2);
            for (size_t k = 0; k < sizeof(bp_widths) / sizeof(bp_widths[0]); ++k) {
                GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
                an_integer_type_supplement bsup = {};
                bsup.bit_width = (bp_widths[k] <= 128UL)
                    ? static_cast<decltype(bsup.bit_width)>(bp_widths[k])
                    : 0;
                a_type tbp = {};
                tbp.kind = tk_integer;
                tbp.size = (bp_widths[k] > 128UL) ? (a_targ_size_t)(bp_widths[k] / 8UL) : 0;
                tbp.variant.integer.int_kind = sgn ? ik_bit_precise : ik_unsigned_bit_precise;
                tbp.variant.integer.extra_info = &bsup;
                gcc_gen_be_error_t e = gcc_gen_be_lower_type(&tbp, &jt);
                if (sgn && bp_widths[k] == 1) {
                    if (e != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;
                } else if (bp_widths[k] == 96 && pfn_mode != 1) {
                    if (e != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;
                } else {
                    GCC_GEN_BE_CHECK(e);
                }
            }
        }
    }
    p_gcc_jit_context_get_int_type = &mock_get_int_type;
    g_mock_int_type_null = false;

    /* _BitInt with extra_info == NULL (uses tp->size * 8; size == 0 -> unsupported, size == 128 -> 1024b unsupported, size == 4 -> 32b) */
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
    a_type tbp_nosup0 = {}, tbp_nosup4 = {}, tbp_nosup1024 = {};
    tbp_nosup0.kind = tk_integer; tbp_nosup0.size = 0; tbp_nosup0.variant.integer.int_kind = ik_unsigned_bit_precise;
    tbp_nosup1024.kind = tk_integer; tbp_nosup1024.size = 128; tbp_nosup1024.variant.integer.int_kind = ik_bit_precise;
    if (gcc_gen_be_lower_type(&tbp_nosup0, &jt) != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_type(&tbp_nosup1024, &jt) != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;
    tbp_nosup4.kind = tk_integer; tbp_nosup4.size = 4; tbp_nosup4.variant.integer.int_kind = ik_bit_precise;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&tbp_nosup4, &jt));

#if FIXED_POINT_ALLOWED
    /* Fixed-point sizes 1, 2, 4, 8 signed & unsigned */
    const a_targ_size_t fp_sizes[4] = { 1, 2, 4, 8 };
    for (int u = 0; u <= 1; ++u) {
        for (int s = 0; s < 4; ++s) {
            GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
            a_type tfp = {};
            tfp.kind = tk_fixed_point;
            tfp.size = fp_sizes[s];
            tfp.variant.fixed_point.is_unsigned = u ? TRUE : FALSE;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&tfp, &jt));
        }
    }
#endif

    /* All float, complex, and imaginary kinds */
    const a_float_kind fks[] = {
        fk_float, fk_double, fk_long_double, fk_std_bfloat16,
        fk_float16, fk_fp16, fk_std_float16, fk_float32x, fk_std_float32,
        fk_float64x, fk_std_float64, fk_float80, fk_float128, fk_std_float128,
        (a_float_kind)250
    };
    for (size_t i = 0; i < sizeof(fks) / sizeof(fks[0]); ++i) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
        a_type tf = {}, tc = {}, tim = {};
        tf.kind = tk_float; tf.variant.float_kind = fks[i];
        tc.kind = tk_complex; tc.variant.float_kind = fks[i]; tc.size = 32;
        tim.kind = tk_imaginary; tim.variant.float_kind = fks[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&tf, &jt));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&tc, &jt));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&tim, &jt));
    }
    const a_targ_size_t ic_sizes[5] = { 2, 4, 8, 16, 24 };
    for (int i = 0; i < 5; ++i) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
        a_type tic = {};
        tic.kind = tk_complex; tic.variant.float_kind = (a_float_kind)250; tic.size = ic_sizes[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&tic, &jt));
    }

    /* Typeref, pointer (null, void, int, routine, VLA), arrays (fixed, 1D/2D VLA, null VLA leaf, dependent, incomplete) */
    a_type tref_int = {};
    tref_int.kind = tk_typeref; tref_int.variant.typeref.type = &dummy_int;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&tref_int, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&tref_int, &jt)); /* cached */

    a_type tref_to_null = {};
    tref_to_null.kind = tk_typeref; tref_to_null.variant.typeref.type = NULL;

    a_type vla_leaf_null = {}, vla_tref_null = {}, vla_inner = {}, vla_tref = {}, vla_outer = {}, arr_fix = {}, arr_dep = {}, arr_inc = {};
    vla_leaf_null.kind = tk_array; vla_leaf_null.variant.array.is_variable_size_array = TRUE; vla_leaf_null.variant.array.element_type = NULL;
    vla_tref_null.kind = tk_array; vla_tref_null.variant.array.is_variable_size_array = TRUE; vla_tref_null.variant.array.element_type = &tref_to_null;
    vla_inner.kind = tk_array; vla_inner.variant.array.is_variable_size_array = TRUE; vla_inner.variant.array.element_type = &tref_int;
    vla_tref.kind = tk_typeref; vla_tref.variant.typeref.type = &vla_inner;
    vla_outer.kind = tk_array; vla_outer.variant.array.is_variable_size_array = TRUE; vla_outer.variant.array.element_type = &vla_tref;
    arr_fix.kind = tk_array; arr_fix.variant.array.element_type = &dummy_int; arr_fix.variant.array.variant.number_of_elements = 4;
    arr_dep.kind = tk_array; arr_dep.variant.array.element_type = &dummy_int; arr_dep.variant.array.is_template_dependent_size_array = TRUE;
    arr_inc.kind = tk_array; arr_inc.variant.array.element_type = &dummy_int; arr_inc.variant.array.variant.number_of_elements = 0;

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&vla_leaf_null, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&vla_tref_null, &jt));
    for (int b = 0; b < 10; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_type(&vla_outer, &jt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 10; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_type(&arr_fix, &jt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 10; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_type(&arr_inc, &jt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    a_type vla_of_fixarr = {};
    vla_of_fixarr.kind = tk_array; vla_of_fixarr.variant.array.is_variable_size_array = TRUE; vla_of_fixarr.variant.array.element_type = &arr_fix;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&vla_of_fixarr, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&arr_dep, &jt));

    a_routine_type_supplement rsup0 = {};
    a_type rtp0 = {};
    rtp0.kind = tk_routine; rtp0.variant.routine.return_type = &t_void; rtp0.variant.routine.extra_info = &rsup0;

    a_type st_top_target = {};
    st_top_target.kind = tk_struct; st_top_target.size = 4;

    a_type p_null = {}, p_void = {}, p_int = {}, p_rout = {}, p_vla = {}, p_fixarr = {}, p_st_top = {};
    p_null.kind = tk_pointer; p_null.variant.pointer.type = NULL;
    p_void.kind = tk_pointer; p_void.variant.pointer.type = &t_void;
    p_int.kind = tk_pointer; p_int.variant.pointer.type = &tref_int;
    p_rout.kind = tk_pointer; p_rout.variant.pointer.type = &rtp0;
    p_vla.kind = tk_pointer; p_vla.variant.pointer.type = &vla_inner;
    p_fixarr.kind = tk_pointer; p_fixarr.variant.pointer.type = &arr_fix;
    p_st_top.kind = tk_pointer; p_st_top.variant.pointer.type = &st_top_target;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&p_null, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&p_void, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&p_int, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&p_rout, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&p_vla, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&p_fixarr, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&p_st_top, &jt));

    /* Routine types: null return, sret return, coerced return, 0 params, invis_ref param, coerced param, null ptype, variadic, OOM */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&rtp0, &jt));

    a_param_type pm1 = {}, pm2 = {};
    pm1.type = &tref_int; pm1.next = &pm2;
    pm2.type = NULL;
    a_routine_type_supplement rsup_var = {};
    rsup_var.param_type_list = &pm1;
    rsup_var.has_ellipsis = TRUE;

    a_type rtp_nullret = {}, rtp_sret = {}, rtp_coerced = {}, rtp_invis = {};
    rtp_nullret.kind = tk_routine; rtp_nullret.variant.routine.return_type = NULL; rtp_nullret.variant.routine.extra_info = &rsup0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&rtp_nullret, &jt));

    g_stub_sret = TRUE;
    rtp_sret.kind = tk_routine; rtp_sret.variant.routine.return_type = &dummy_int; rtp_sret.variant.routine.extra_info = &rsup_var;
    for (int b = 0; b < 25; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_type(&rtp_sret, &jt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    g_stub_sret = FALSE;

    g_stub_invis_ref = TRUE;
    rtp_invis.kind = tk_routine; rtp_invis.variant.routine.return_type = &dummy_int; rtp_invis.variant.routine.extra_info = &rsup_var;
    for (int b = 0; b < 25; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_type(&rtp_invis, &jt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    g_stub_invis_ref = FALSE;

    g_stub_coerced = TRUE;
    g_stub_coerced_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
    rtp_coerced.kind = tk_routine; rtp_coerced.variant.routine.return_type = &dummy_int; rtp_coerced.variant.routine.extra_info = &rsup_var;
    for (int b = 0; b < 25; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_type(&rtp_coerced, &jt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    /* Coerced flag TRUE with coerced_type == NULL */
    a_type rtp_coerced_null = {};
    rtp_coerced_null.kind = tk_routine; rtp_coerced_null.variant.routine.return_type = &dummy_int; rtp_coerced_null.variant.routine.extra_info = &rsup_var;
    g_stub_coerced_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&rtp_coerced_null, &jt));
    g_stub_coerced = FALSE;

    /* Pointer to member (routine via typeref, data via typeref, null member type) & vectors (bool, int, null elem, huge >64 lanes) */
    a_type rtp_tref = {};
    rtp_tref.kind = tk_typeref; rtp_tref.variant.typeref.type = &rtp0;
    a_type pm_fn = {}, pm_data = {}, pm_null = {};
    pm_fn.kind = tk_ptr_to_member; pm_fn.variant.ptr_to_member.type = &rtp_tref;
    pm_data.kind = tk_ptr_to_member; pm_data.variant.ptr_to_member.type = &tref_int;
    pm_null.kind = tk_ptr_to_member; pm_null.variant.ptr_to_member.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&pm_fn, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&pm_data, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&pm_null, &jt));

    t_bool.size = 1;
    a_type zero_elem_int = {};
    zero_elem_int.kind = tk_integer; zero_elem_int.size = 0; zero_elem_int.variant.integer.int_kind = ik_int;
    a_type vec_bool = {}, vec_int = {}, vec_null = {}, vec_128 = {}, vec_zerosz_elem = {}, vec_undersized = {};
    vec_bool.kind = tk_vector; vec_bool.size = 4; vec_bool.variant.vector.element_type = &t_bool;
    vec_int.kind = tk_vector; vec_int.size = 16; vec_int.variant.vector.element_type = &dummy_int;
    vec_null.kind = tk_vector; vec_null.size = 0; vec_null.variant.vector.element_type = NULL;
    vec_128.kind = tk_vector; vec_128.size = 512; vec_128.variant.vector.element_type = &dummy_int; /* 128 > 64 lanes */
    vec_zerosz_elem.kind = tk_vector; vec_zerosz_elem.size = 16; vec_zerosz_elem.variant.vector.element_type = &zero_elem_int;
    vec_undersized.kind = tk_vector; vec_undersized.size = 2; vec_undersized.variant.vector.element_type = &dummy_int;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&vec_bool, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&vec_int, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&vec_null, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&vec_128, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&vec_zerosz_elem, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&vec_undersized, &jt));

    /* gcc_gen_be_get_zero_rvalue across all branches + OOM */
    if (gcc_gen_be_get_zero_rvalue(NULL, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(NULL, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), &rv));
    gcc_jit_field *z_fld = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "zf");
    gcc_jit_struct *empty_js = gcc_jit_context_new_struct_type(ctx, NULL, "z_st", 1, &z_fld);
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(NULL, gcc_jit_struct_as_type(empty_js), &rv));

    a_type st_simple = {};
    st_simple.kind = tk_struct; st_simple.size = 1;
    a_type cls_simple = {};
    cls_simple.kind = tk_class; cls_simple.size = 1;
    a_type tref_nullp = {};
    tref_nullp.kind = tk_typeref; tref_nullp.variant.typeref.type = &t_nullp;

    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&tref_nullp, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&p_int, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&pm_fn, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&pm_data, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&pm_null, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&st_simple, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&cls_simple, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&arr_fix, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&t_void, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&dummy_int, NULL, &rv));

    p_gcc_jit_context_new_rvalue_from_vector = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&vec_bool, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&vec_null, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&vec_zerosz_elem, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&vec_undersized, NULL, &rv));
    p_gcc_jit_context_new_rvalue_from_vector = &mock_type_vec_rval;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&vec_int, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&vec_128, NULL, &rv));
    p_gcc_jit_context_new_rvalue_from_vector = NULL;

    /* Also pass non-null jit_type so lines 1003, 1009, 1029, 1033, 1040 hit OOM directly! */
    for (int b = 0; b < 10; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_get_zero_rvalue(&p_int, NULL, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    gcc_jit_type * dummy_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    a_type *z_targets[] = { &p_int, &pm_fn, &st_simple, &arr_fix, &vec_int };
    for (size_t z = 0; z < 5; ++z) {
        for (int b = 0; b < 15; ++b) {
            GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t e = gcc_gen_be_get_zero_rvalue(z_targets[z], dummy_jt, &rv);
            g_gcc_be_oom_countdown = -1;
            if (e == GCC_GEN_BE_SUCCESS) break;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises complex struct/class/union layout, bases, bitfield containers/extensions, anonymous members, and >64 deferred struct cycles.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_complex_structs_bases_bitfields_and_deferred(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_type *jt = NULL;
    if (!ctx) return GCC_GEN_BE_ERROR_INTERNAL;

    a_type u32_t = {}, zero_sz_t = {};
    u32_t.kind = tk_integer; u32_t.size = 4; u32_t.variant.integer.int_kind = ik_unsigned_int;
    zero_sz_t.kind = tk_integer; zero_sz_t.size = 0; zero_sz_t.variant.integer.int_kind = ik_unsigned_int;

    /* 1. Base classes: empty optimized, zero-size packed, virtual base nvsize < full_bsize, nvsize == 0, nvsize == full_bsize, overlapping next entry, full size, unnamed */
    a_class_type_supplement base_v_supp = {}, base_v0_supp = {}, base_vfull_supp = {};
    base_v_supp.size_without_virtual_base_classes = 4;
    base_v0_supp.size_without_virtual_base_classes = 0;
    base_vfull_supp.size_without_virtual_base_classes = 8;
    a_type base_v_tp = {}, base_v0_tp = {}, base_vfull_tp = {}, base_zero_tp = {}, base_unnamed_tp = {}, base_full_tp = {};
    base_v_tp.kind = tk_class; base_v_tp.size = 8; base_v_tp.source_corresp.name = "VBase";
    base_v_tp.variant.class_struct_union.extra_info = &base_v_supp;
    base_v0_tp.kind = tk_struct; base_v0_tp.size = 8; base_v0_tp.variant.class_struct_union.extra_info = &base_v0_supp;
    base_vfull_tp.kind = tk_struct; base_vfull_tp.size = 8; base_vfull_tp.variant.class_struct_union.extra_info = &base_vfull_supp;
    base_zero_tp.kind = tk_struct; base_zero_tp.size = 0; base_zero_tp.source_corresp.name = "ZeroBase";
    base_unnamed_tp.kind = tk_struct; base_unnamed_tp.size = 8; base_unnamed_tp.source_corresp.name = NULL;
    base_full_tp.kind = tk_integer; base_full_tp.size = 4; base_full_tp.variant.integer.int_kind = ik_int; /* non-class base */

    a_base_class b_opt = {}, b_zero = {}, b_nv = {}, b_nv0 = {}, b_nvfull = {}, b_overlap = {}, b_full = {}, b_virt_indir = {}, b_virt_dir = {};
    b_opt.type = &base_v_tp; b_opt.is_optimized_empty_base = TRUE; b_opt.offset = 0; b_opt.next_direct = &b_zero;
    b_zero.type = &base_zero_tp; b_zero.offset = 0; b_zero.next_direct = &b_nv;
    b_nv.type = &base_v_tp; b_nv.offset = 4; b_nv.next_direct = &b_nv0; /* pad 0..4, bsize 4 < full 8 */
    b_nv0.type = &base_v0_tp; b_nv0.offset = 8; b_nv0.next_direct = &b_nvfull;
    b_nvfull.type = &base_vfull_tp; b_nvfull.offset = 16; b_nvfull.next_direct = &b_overlap;
    b_overlap.type = &base_unnamed_tp; b_overlap.offset = 24; b_overlap.next_direct = &b_full; /* next entry at 26 -> bsize = 2 < 8 */
    b_full.type = &base_full_tp; b_full.offset = 26; b_full.next_direct = NULL;

    a_base_class b_nonvirt_in_all = {};
    b_nonvirt_in_all.type = &base_v_tp; b_nonvirt_in_all.is_virtual = FALSE;
    b_virt_dir.type = &base_v_tp; b_virt_dir.is_virtual = TRUE; b_virt_dir.direct = TRUE; b_virt_dir.next = &b_virt_indir;
    b_virt_indir.type = &base_unnamed_tp; b_virt_indir.is_virtual = TRUE; b_virt_indir.direct = FALSE; b_virt_indir.offset = 40; b_virt_indir.next = &b_nonvirt_in_all;

    /* Also test base-only struct (last entry is base -> k + 1 < num_entries == False) & bitfield followed by base (!entries[m].is_base == False) */
    a_base_class b_single = {};
    b_single.type = &base_full_tp; b_single.offset = 4;
    a_class_type_supplement single_base_supp = {};
    single_base_supp.direct_base_classes = &b_single;
    a_field bf_before_base = {};
    bf_before_base.type = &u32_t; bf_before_base.is_bit_field = TRUE; bf_before_base.offset = 0; bf_before_base.bit_size = 5;
    a_type bf_then_base_st = {};
    bf_then_base_st.kind = tk_struct; bf_then_base_st.size = 8;
    bf_then_base_st.variant.class_struct_union.field_list = &bf_before_base;
    bf_then_base_st.variant.class_struct_union.extra_info = &single_base_supp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&bf_then_base_st, &jt));

    a_class_type_supplement derived_supp = {};
    derived_supp.direct_base_classes = &b_opt;
    derived_supp.base_classes = &b_virt_dir;

    a_type tref_u32 = {};
    tref_u32.kind = tk_typeref; tref_u32.variant.typeref.type = &u32_t;
    a_type ptr_to_union_in_struct = {};
    a_type u_target = {};
    u_target.kind = tk_union; u_target.size = 4;
    a_field u_f_off4 = {};
    u_f_off4.type = &u32_t; u_f_off4.offset = 4;
    u_target.variant.class_struct_union.field_list = &u_f_off4;
    ptr_to_union_in_struct.kind = tk_pointer; ptr_to_union_in_struct.size = 8; ptr_to_union_in_struct.variant.pointer.type = &u_target;

    /* Unsorted fields + null f->type + typeref f->type + pointer-to-union inside struct */
    a_field f_late = {}, f_early = {}, f_emp = {}, f_nulltp = {}, f_uptr = {};
    f_late.type = &tref_u32; f_late.offset = 36; f_late.source_corresp.name = NULL; f_late.next = &f_early;
    f_early.type = &u32_t; f_early.offset = 32; f_early.source_corresp.name = "early_f"; f_early.next = &f_emp;
    f_emp.type = &u32_t; f_emp.offset = 32; f_emp.is_optimized_empty_class = TRUE; f_emp.next = &f_nulltp;
    f_nulltp.type = NULL; f_nulltp.offset = 48; f_nulltp.source_corresp.name = "void_fld"; f_nulltp.next = &f_uptr;
    f_uptr.type = &ptr_to_union_in_struct; f_uptr.offset = 48; f_uptr.source_corresp.name = "uptr_fld";

    a_type derived_tp = {};
    derived_tp.kind = tk_class;
    derived_tp.size = 64; /* triggers __tail_pad */
    derived_tp.source_corresp.name = "ComplexDerived";
    derived_tp.variant.class_struct_union.field_list = &f_late;
    derived_tp.variant.class_struct_union.extra_info = &derived_supp;

    /* Note void field isn't valid in libgccjit struct_set_fields, keep f_nulltp out of derived_tp and test null edg_type_byte_size via typeref->NULL! */
    a_type tref_null_sz = {};
    tref_null_sz.kind = tk_typeref; tref_null_sz.variant.typeref.type = NULL;
    f_emp.next = &f_uptr;

    for (int b = 0; b < 150; ++b) {
        GCC_GEN_BE_CHECK(cache_clear_all());
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_type(&derived_tp, &jt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* Plain 1-field struct with extra_info == NULL */
    a_field plain_f = {};
    plain_f.type = &u32_t; plain_f.source_corresp.name = "pf";
    a_type plain_st = {};
    plain_st.kind = tk_struct; plain_st.size = 4; plain_st.variant.class_struct_union.field_list = &plain_f;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&plain_st, &jt));

    /* 2. Lowered __b_ base fields (extra_info != NULL, extra_info == NULL, union with __b_) & anonymous parent union/struct/class/int/null */
    a_field anon_sub1 = {}, anon_sub2 = {};
    anon_sub1.type = &u32_t; anon_sub1.source_corresp.name = "u_a"; anon_sub1.next = &anon_sub2;
    anon_sub2.type = &u32_t; anon_sub2.source_corresp.name = "u_b";
    a_type anon_u_tp = {}, anon_s_tp = {}, anon_c_tp = {};
    anon_u_tp.kind = tk_union; anon_u_tp.size = 4; anon_u_tp.source_corresp.name = "AnonU";
    anon_u_tp.variant.class_struct_union.field_list = &anon_sub1;
    anon_s_tp.kind = tk_struct; anon_s_tp.size = 4; anon_s_tp.variant.class_struct_union.field_list = &plain_f;
    anon_c_tp.kind = tk_class; anon_c_tp.size = 4; anon_c_tp.variant.class_struct_union.field_list = &plain_f;
    a_type anon_u_tref = {};
    anon_u_tref.kind = tk_typeref; anon_u_tref.variant.typeref.type = &anon_u_tp;

    a_field fb_base = {}, fb_anon = {}, fb_anon_s = {}, fb_anon_c = {}, fb_anon_int = {};
    fb_base.source_corresp.name = "__b_Base"; fb_base.type = &u32_t; fb_base.offset = 4; fb_base.next = &fb_anon;
    fb_anon.source_corresp.name = "__anon_u"; fb_anon.type = &anon_u_tref; fb_anon.offset = 8; fb_anon.is_anonymous_parent_object = TRUE; fb_anon.next = &fb_anon_s;
    fb_anon_s.source_corresp.name = "__anon_s"; fb_anon_s.type = &anon_s_tp; fb_anon_s.offset = 12; fb_anon_s.is_anonymous_parent_object = TRUE; fb_anon_s.next = &fb_anon_c;
    fb_anon_c.source_corresp.name = "__anon_c"; fb_anon_c.type = &anon_c_tp; fb_anon_c.offset = 16; fb_anon_c.is_anonymous_parent_object = TRUE; fb_anon_c.next = &fb_anon_int;
    fb_anon_int.source_corresp.name = "__anon_i"; fb_anon_int.type = &u32_t; fb_anon_int.offset = 20; fb_anon_int.is_anonymous_parent_object = TRUE;

    a_class_type_supplement lb_supp1 = {}, lb_supp2 = {};
    lb_supp1.base_classes = &b_nv;
    lb_supp2.direct_base_classes = &b_opt;

    a_type lb_cls1 = {}, lb_cls2 = {}, lb_cls_noextra = {}, lb_union = {};
    lb_cls1.kind = tk_struct; lb_cls1.size = 24; lb_cls1.variant.class_struct_union.field_list = &fb_base;
    lb_cls1.variant.class_struct_union.extra_info = &lb_supp1;
    lb_cls2.kind = tk_class; lb_cls2.size = 24; lb_cls2.variant.class_struct_union.field_list = &fb_base;
    lb_cls2.variant.class_struct_union.extra_info = &lb_supp2;
    lb_cls_noextra.kind = tk_struct; lb_cls_noextra.size = 24; lb_cls_noextra.variant.class_struct_union.field_list = &fb_base;
    lb_union.kind = tk_union; lb_union.size = 24; lb_union.variant.class_struct_union.field_list = &fb_base;

    for (int b = 0; b < 80; ++b) {
        GCC_GEN_BE_CHECK(cache_clear_all());
        /* Lower anon_u_tp first, then clear ONLY CACHE_FIELD and pre-seed anon_sub2 so anon_sub1 is uncached and anon_sub2 is cached! */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&anon_u_tp, &jt));
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_FIELD));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &anon_sub2, (void *)0x1));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_type(&lb_cls1, &jt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&lb_cls2, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&lb_cls_noextra, &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&lb_union, &jt));

    /* 3. Bitfields: is_bit_field==FALSE && bit_size>0, container_already_emitted, max_bf_end expansion,
          pointer/struct bitfield type (unit_sz==0), req 1/2/4/8/unaligned fallback, wide bitfield + realloc, and union bitfields */
    a_type ptr_bf_t = {};
    ptr_bf_t.kind = tk_pointer; ptr_bf_t.size = 0; ptr_bf_t.variant.pointer.type = &u32_t;
    a_field bf_z0 = {}, bf_1b = {}, bf_1b_hi = {}, bf_same = {}, bf_z1 = {}, bf_2b = {}, bf_4b = {}, bf_8b = {}, bf_unalign = {}, bf_huge = {}, bf_end_reg = {};
    /* At offset 0: bf_z0 (0w), bf_1b (rem=1, sz=2 -> f_end=1), bf_1b_hi (is_bit_field=FALSE, bit_size=6, offset=1 -> nbf_end=2 > f_end=1 -> max_bf_end=2!), bf_same (offset=0, rem=4, sz=3 -> f_end=1 <= current_offset=2 -> container_already_emitted == TRUE!) */
    bf_same.type = &zero_sz_t; bf_same.is_bit_field = TRUE; bf_same.offset = 0; bf_same.offset_bit_remainder = 4; bf_same.bit_size = 3; bf_same.next = &bf_z0;
    bf_z0.type = &u32_t; bf_z0.is_bit_field = TRUE; bf_z0.offset = 0; bf_z0.offset_bit_remainder = 0; bf_z0.bit_size = 0; bf_z0.next = &bf_1b;
    bf_1b.type = &ptr_bf_t; bf_1b.is_bit_field = TRUE; bf_1b.offset = 0; bf_1b.offset_bit_remainder = 1; bf_1b.bit_size = 2; bf_1b.next = &bf_1b_hi;
    bf_1b_hi.type = &tref_null_sz; bf_1b_hi.is_bit_field = FALSE; bf_1b_hi.offset = 1; bf_1b_hi.bit_size = 6; bf_1b_hi.next = &bf_z1;
    bf_z1.type = &u32_t; bf_z1.is_bit_field = TRUE; bf_z1.offset = 2; bf_z1.bit_size = 0;

    a_field reg1 = {}, reg2 = {}, reg3 = {}, reg4 = {}, reg0 = {};
    a_type ch_t = {};
    ch_t.kind = tk_integer; ch_t.size = 1; ch_t.variant.integer.int_kind = ik_char;
    bf_z1.next = &reg0;
    reg0.type = &ch_t; reg0.offset = 2; reg0.next = &reg1;
    /* Group at offset 3..4 -> req=1, avail=1 -> UNSIGNED_CHAR! */
    a_field bf_only1 = {};
    reg0.next = &bf_only1;
    bf_only1.type = &zero_sz_t; bf_only1.is_bit_field = TRUE; bf_only1.offset = 3; bf_only1.bit_size = 4; bf_only1.next = &reg1;
    reg1.type = &ch_t; reg1.offset = 4; reg1.next = &bf_unalign;
    /* Group at offset 5..7 -> req=2, avail=3, offset%2!=0 -> fallback UNSIGNED_CHAR + 1 ext byte! */
    bf_unalign.type = &u32_t; bf_unalign.is_bit_field = TRUE; bf_unalign.offset = 5; bf_unalign.bit_size = 14; bf_unalign.next = &reg2;
    reg2.type = &ch_t; reg2.offset = 7; reg2.next = &bf_4b;
    /* Group at offset 8..12 -> unit_sz=1 < req=4, avail=4, offset%4==0 -> UNSIGNED_INT! */
    bf_4b.type = &ch_t; bf_4b.is_bit_field = TRUE; bf_4b.offset = 8; bf_4b.bit_size = 28; bf_4b.next = &reg4;
    reg4.type = &u32_t; reg4.offset = 12; reg4.next = &bf_8b;
    /* Group at offset 16..24 -> unit_sz=1 < req=8, avail=8, offset%8==0 -> UNSIGNED_LONG_LONG! */
    bf_8b.type = &ch_t; bf_8b.is_bit_field = TRUE; bf_8b.offset = 16; bf_8b.bit_size = 60; bf_8b.next = &bf_2b;
    /* Group at offset 24..26 -> req=2, avail=2, offset%2==0 -> UNSIGNED_SHORT! */
    bf_2b.type = &zero_sz_t; bf_2b.is_bit_field = TRUE; bf_2b.offset = 24; bf_2b.bit_size = 12; bf_2b.next = &reg3;
    reg3.type = &u32_t; reg3.offset = 28; reg3.next = &bf_huge;
    /* Group at offset 32..62 -> 240-bit bitfield -> req=30 > 8 -> UNSIGNED_CHAR + 29 ext bytes -> triggers max_fields realloc! */
    bf_huge.type = &u32_t; bf_huge.is_bit_field = TRUE; bf_huge.offset = 32; bf_huge.bit_size = 240; bf_huge.next = &bf_end_reg;
    bf_end_reg.type = &u32_t; bf_end_reg.offset = 64;

    a_type bf_master = {};
    bf_master.kind = tk_struct;
    bf_master.size = 68;
    bf_master.variant.class_struct_union.field_list = &bf_same;

    for (int b = 0; b < 350; ++b) {
        GCC_GEN_BE_CHECK(cache_clear_all());
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_type(&bf_master, &jt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&bf_master, &jt));

    a_type empty_s = {};
    empty_s.kind = tk_struct; empty_s.size = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&empty_s, &jt));

    a_field bf_tref_null_only = {}, bf8_only = {};
    bf_tref_null_only.type = &tref_null_sz; bf_tref_null_only.is_bit_field = TRUE; bf_tref_null_only.bit_size = 4;
    a_type bf_tref_null_st = {};
    bf_tref_null_st.kind = tk_struct; bf_tref_null_st.size = 4; bf_tref_null_st.variant.class_struct_union.field_list = &bf_tref_null_only;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&bf_tref_null_st, &jt));

    bf8_only.type = &empty_s; bf8_only.is_bit_field = TRUE; bf8_only.offset = 0; bf8_only.bit_size = 60;
    a_type bf8_only_st = {};
    bf8_only_st.kind = tk_struct; bf8_only_st.size = 8; bf8_only_st.variant.class_struct_union.field_list = &bf8_only;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&bf8_only_st, &jt));

    /* Additional bitfield edge cases:
       - 0-byte struct as bitfield type (unit_sz == 0 && gcc_jit_type_is_struct != NULL)
       - 8-byte u64 bitfield (unit_sz == 8 -> unit_sz < 8 == False)
       - 4-byte straddling bitfield in 4-byte struct (f->offset + 8 <= tp->size == False)
       - exact 4-byte aligned bitfield (unit_sz==4, avail==4, offset%4==0, req==1 -> keeps unit_sz=4!)
       - req=2 with avail=1 (327:54 == False)
       - req=4 with avail=8 at offset=2 (330:70 == False)
       - req=6 with avail=8 at offset=4 (333:70 == False) */
    a_type u64_t = {};
    u64_t.kind = tk_integer; u64_t.size = 8; u64_t.variant.integer.int_kind = ik_unsigned_long_long;

    a_field bf_st0 = {}, bf_u64 = {}, bf_keep4 = {}, bf_r2a1 = {}, pad_r2 = {}, bf_r4off2 = {}, pad_r4 = {}, bf_r6off4 = {};
    bf_keep4.type = &u32_t; bf_keep4.is_bit_field = TRUE; bf_keep4.offset = 0; bf_keep4.bit_size = 4; bf_keep4.next = &bf_u64;
    bf_u64.type = &u64_t; bf_u64.is_bit_field = TRUE; bf_u64.offset = 8; bf_u64.bit_size = 4; bf_u64.next = &bf_st0;
    bf_st0.type = &empty_s; bf_st0.is_bit_field = TRUE; bf_st0.offset = 16; bf_st0.bit_size = 4; bf_st0.next = &bf_r2a1;
    bf_r2a1.type = &u32_t; bf_r2a1.is_bit_field = TRUE; bf_r2a1.offset = 17; bf_r2a1.bit_size = 12; bf_r2a1.next = &pad_r2;
    pad_r2.type = &ch_t; pad_r2.offset = 18; pad_r2.next = &bf_r4off2; /* limit=18 -> avail=1 < req=2 */
    /* After pad_r2 at 18..19, add 1 pad to 20? No, offset 22: 22 % 4 == 2 != 0, limit = 30 -> avail = 8 >= 4, req = 4! */
    bf_r4off2.type = &u32_t; bf_r4off2.is_bit_field = TRUE; bf_r4off2.offset = 22; bf_r4off2.bit_size = 28; bf_r4off2.next = &pad_r4;
    pad_r4.type = &u32_t; pad_r4.offset = 32; pad_r4.next = &bf_r6off4; /* current becomes 36; at 36: 36 % 8 == 4 != 0, tp->size = 44 -> avail = 8 >= 8, req = 6! */
    bf_r6off4.type = &u32_t; bf_r6off4.is_bit_field = TRUE; bf_r6off4.offset = 36; bf_r6off4.bit_size = 44;

    a_type bf_extra_st = {};
    bf_extra_st.kind = tk_struct; bf_extra_st.size = 44; bf_extra_st.variant.class_struct_union.field_list = &bf_keep4;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&bf_extra_st, &jt));

    a_field bf_strad_small = {};
    bf_strad_small.type = &u32_t; bf_strad_small.is_bit_field = TRUE; bf_strad_small.offset = 0; bf_strad_small.offset_bit_remainder = 20; bf_strad_small.bit_size = 20;
    a_type bf_strad_small_st = {};
    bf_strad_small_st.kind = tk_struct; bf_strad_small_st.size = 6; bf_strad_small_st.variant.class_struct_union.field_list = &bf_strad_small;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&bf_strad_small_st, &jt));

    /* Struct bitfield with limit_offset <= current_offset */
    a_field bf_st_fld = {}, bf_past_sz = {};
    bf_st_fld.type = &plain_st; bf_st_fld.is_bit_field = TRUE; bf_st_fld.offset = 0; bf_st_fld.bit_size = 4; bf_st_fld.next = &bf_past_sz;
    bf_past_sz.type = &u32_t; bf_past_sz.is_bit_field = TRUE; bf_past_sz.offset = 4; bf_past_sz.bit_size = 24;
    a_type bf_edge_st = {};
    bf_edge_st.kind = tk_struct; bf_edge_st.size = 4; bf_edge_st.variant.class_struct_union.field_list = &bf_st_fld;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&bf_edge_st, &jt));

    /* Cached struct lookup when deferred_structs_count == 0 (hits 488:27 == False!) */
    gcc_gen_be_singleton_state.deferred_structs_count = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&bf_edge_st, &jt));

    /* Union with bitfield + OOM on union cache_insert */
    a_field ubf = {};
    ubf.type = &u32_t; ubf.is_bit_field = TRUE; ubf.bit_size = 5;
    a_type u_bf_tp = {};
    u_bf_tp.kind = tk_union; u_bf_tp.size = 4; u_bf_tp.variant.class_struct_union.field_list = &ubf;
    for (int b = 0; b < 60; ++b) {
        GCC_GEN_BE_CHECK(cache_clear_all());
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_type(&u_bf_tp, &jt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* 4. Deferred struct cycle (>64 pointer-referenced structs + distinct 2nd pointer type + by-value access to already-queued deferred struct) */
    a_type def_nodes[68];
    a_type def_ptrs[68];
    a_field def_flds[68];
    memset(def_nodes, 0, sizeof(def_nodes));
    memset(def_ptrs, 0, sizeof(def_ptrs));
    memset(def_flds, 0, sizeof(def_flds));
    for (int i = 0; i < 68; ++i) {
        def_nodes[i].kind = (i % 3 == 0) ? tk_class : tk_struct;
        def_nodes[i].size = 8;
        def_nodes[i].source_corresp.name = (i % 2 == 0) ? "DefNode" : NULL;
        int nxt = (i + 1) % 68;
        def_ptrs[i].kind = tk_pointer;
        def_ptrs[i].size = 8;
        def_ptrs[i].variant.pointer.type = &def_nodes[nxt];
        def_flds[i].type = &def_ptrs[i];
        def_nodes[i].variant.class_struct_union.field_list = &def_flds[i];
    }
    a_type distinct_ptr_to_1 = {};
    distinct_ptr_to_1.kind = tk_pointer; distinct_ptr_to_1.size = 8; distinct_ptr_to_1.variant.pointer.type = &def_nodes[1];

    a_field ptr_to_5 = {}, dup_ptr_fld = {}, byval_5 = {};
    ptr_to_5.type = &def_ptrs[4];
    ptr_to_5.offset = 8;
    dup_ptr_fld.type = &distinct_ptr_to_1; /* distinct a_type pointing to already-cached def_nodes[1] -> hits !elem == False! */
    dup_ptr_fld.offset = 16;
    byval_5.type = &def_nodes[5];
    byval_5.offset = 24;
    def_flds[0].next = &ptr_to_5;
    ptr_to_5.next = &dup_ptr_fld;
    dup_ptr_fld.next = &byval_5;
    def_nodes[0].size = 32;

    for (int b = 0; b < 450; ++b) {
        GCC_GEN_BE_CHECK(cache_clear_all());
        gcc_gen_be_singleton_state.deferred_structs_count = 0;
        if (gcc_gen_be_singleton_state.deferred_structs) {
            free(gcc_gen_be_singleton_state.deferred_structs);
            gcc_gen_be_singleton_state.deferred_structs = NULL;
            gcc_gen_be_singleton_state.deferred_structs_capacity = 0;
        }
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_type(&def_nodes[0], &jt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_gen_be_singleton_state.deferred_structs_count = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&def_nodes[0], &jt));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&def_nodes[0], &jt)); /* cached completed struct */

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises remaining decimal float (128-bit, pfn NULL, OOM), fixed-point (64-bit signed/unsigned saturating), and `_BitInt` normalization branches.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_decimal_fixed_and_bitint_norm(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_type *df32_t = NULL, *df64_t = NULL, *df128_t = NULL;
    p_gcc_jit_context_get_int_type = &mock_get_int_type;
    g_mock_int_type_null = false;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(4, &df32_t));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(8, &df64_t));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(16, &df128_t));
    p_gcc_jit_context_get_int_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(16, &df128_t));
    p_gcc_jit_context_get_int_type = &mock_get_int_type;

    gcc_jit_rvalue *v32 = gcc_jit_context_new_rvalue_from_int(ctx, df32_t, 0);
    gcc_jit_rvalue *v64 = gcc_jit_context_new_rvalue_from_int(ctx, df64_t, 0);
    gcc_jit_rvalue *v128 = gcc_jit_context_new_rvalue_from_int(ctx, df128_t, 0);
    gcc_jit_rvalue *rv = NULL;

    /* Invalid args & 128-bit modes + OOM sweeps */
    if (gcc_gen_be_lower_decimal_float_binary_op(4, '+', v32, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_binary_op(4, '+', v32, v32, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_binary_op(16, '+', v128, v128, &rv));

    if (gcc_gen_be_lower_decimal_float_comparison(4, "eq", NULL, v32, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_comparison(4, "eq", v32, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_comparison(4, "eq", v32, v32, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_comparison(16, "other", v128, v128, &rv));

    if (gcc_gen_be_lower_decimal_float_cast(4, 8, v32, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_cast(16, 8, v128, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_cast(8, 16, v64, &rv));

    if (gcc_gen_be_lower_decimal_to_binary_float(4, 4, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_to_binary_float(4, 4, v32, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_binary_float(16, 8, v128, &rv));

    if (gcc_gen_be_lower_decimal_to_int_sized(4, 4, FALSE, v32, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_to_int_sized(4, 2, FALSE, v32, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_int_sized(16, 8, FALSE, v128, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_int_sized(16, 8, TRUE, v128, &rv));

    if (gcc_gen_be_lower_int_to_decimal(4, FALSE, 4, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_int_to_decimal(4, FALSE, 4, v32, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_int_to_decimal(4, TRUE, 4, v32, &rv));

    if (gcc_gen_be_lower_binary_float_to_decimal(4, 4, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_binary_float_to_decimal(4, 4, v32, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_lower_decimal_float_unary_op(4, '-', v32, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_unary_op(16, '-', v128, &rv));

    /* OOM sweeps on all decimal helpers */
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_decimal_float_binary_op(4, '+', v32, v32, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_decimal_float_comparison(4, "eq", v32, v32, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_decimal_float_cast(4, 8, v32, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_decimal_to_binary_float(4, 8, v32, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_decimal_to_int_sized(4, 8, FALSE, v32, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_int_to_decimal(4, FALSE, 4, v32, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_binary_float_to_decimal(4, 8, v32, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_decimal_float_unary_op(4, '-', v32, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* Fixed-point invalid args, negative frac_bits, 8-byte signed/unsigned saturating (pfn non-null & null), OOM */
    if (gcc_gen_be_lower_fixed_point_binary_op(4, 8, FALSE, FALSE, '+', NULL, v32, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_binary_op(4, 8, FALSE, FALSE, '+', v32, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_binary_op(4, 8, FALSE, FALSE, '+', v32, v32, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_binary_op(1, 4, FALSE, FALSE, '+', v32, v32, &rv) != GCC_GEN_BE_SUCCESS) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_binary_op(4, -1, FALSE, FALSE, '+', v32, v32, &rv) != GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Missing ctx == NULL on decimal/fixed/bitint helpers */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_decimal_float_binary_op(4, '+', v32, v32, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_comparison(4, "eq", v32, v32, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_cast(4, 8, v32, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_to_binary_float(4, 8, v32, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_to_int_sized(4, 4, FALSE, v32, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_int_to_decimal(4, FALSE, 4, v32, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_binary_float_to_decimal(4, 8, v32, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_unary_op(4, '-', v32, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_binary_op(4, 8, FALSE, FALSE, '+', v32, v32, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_binary_op(8, 31, FALSE, TRUE, '+', v64, v64, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_binary_op(8, 31, TRUE, TRUE, '+', v64, v64, &rv));
    p_gcc_jit_context_get_int_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_binary_op(8, 31, FALSE, TRUE, '+', v64, v64, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_binary_op(8, 31, TRUE, TRUE, '+', v64, v64, &rv));
    p_gcc_jit_context_get_int_type = &mock_get_int_type;
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_lower_fixed_point_binary_op(4, 8, FALSE, FALSE, '+', v32, v32, &rv) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;

    /* gcc_gen_be_normalize_bitint_rvalue across all branches */
    if (gcc_gen_be_normalize_bitint_rvalue(NULL, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_bitint_rvalue(NULL, v32, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_normalize_bitint_rvalue(NULL, v32, &rv));

    a_type non_int = {}, ord_int = {};
    non_int.kind = tk_float;
    ord_int.kind = tk_integer; ord_int.variant.integer.int_kind = ik_int;
    GCC_GEN_BE_CHECK(gcc_gen_be_normalize_bitint_rvalue(&non_int, v32, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_normalize_bitint_rvalue(&ord_int, v32, &rv));

    const unsigned long norm_widths[] = { 0, 8, 16, 32, 64, 128, 256, 5, 12, 24, 48, 96 };
    for (int sgn = 0; sgn <= 1; ++sgn) {
        for (size_t k = 0; k < sizeof(norm_widths) / sizeof(norm_widths[0]); ++k) {
            an_integer_type_supplement nsup = {};
            nsup.bit_width = (norm_widths[k] <= 128UL)
                ? static_cast<decltype(nsup.bit_width)>(norm_widths[k])
                : 0;
            a_type nbp = {}, ntref = {};
            nbp.kind = tk_integer;
            nbp.size = (norm_widths[k] > 128UL) ? (a_targ_size_t)(norm_widths[k] / 8UL) : 0;
            nbp.variant.integer.int_kind = sgn ? ik_bit_precise : ik_unsigned_bit_precise;
            nbp.variant.integer.extra_info = (norm_widths[k] == 0) ? NULL : &nsup;
            ntref.kind = tk_typeref; ntref.variant.typeref.type = &nbp;
            gcc_jit_rvalue *in_v = (norm_widths[k] == 96) ? v128 : ((norm_widths[k] == 48) ? v64 : v32);
            GCC_GEN_BE_CHECK(gcc_gen_be_normalize_bitint_rvalue(&ntref, in_v, &rv));
        }
    }
    an_integer_type_supplement nsup24 = {};
    nsup24.bit_width = 24;
    a_type nbp24 = {};
    nbp24.kind = tk_integer; nbp24.variant.integer.int_kind = ik_bit_precise; nbp24.variant.integer.extra_info = &nsup24;
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_normalize_bitint_rvalue(&nbp24, v32, &rv) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes all type lowering unit tests.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_type_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_type *type = NULL;
    if (gcc_gen_be_lower_type(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_type(NULL, &type) != GCC_GEN_BE_SUCCESS) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_type(4, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    if (gcc_gen_be_lower_decimal_float_type(7, &type) != GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    const char df_ops[4] = { '+', '-', '*', '/' };
    gcc_jit_type *df32_t = NULL, *df64_t = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(4, &df32_t));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(8, &df64_t));
    gcc_jit_rvalue *df32_zero = gcc_jit_context_new_rvalue_from_int(ctx, df32_t, 0);
    gcc_jit_rvalue *df64_zero = gcc_jit_context_new_rvalue_from_int(ctx, df64_t, 0);
    gcc_jit_rvalue *rval = NULL;

    if (gcc_gen_be_lower_decimal_float_binary_op(4, '+', NULL, df32_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_binary_op(4, '?', df32_zero, df32_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    for (int i = 0; i < 4; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_binary_op(4, df_ops[i], df32_zero, df32_zero, &rval));
    }
    gcc_gen_be_singleton_state.active_target_abi_override = (int)GCC_GEN_BE_TARGET_ABI_PPC64_ELFV2 + 1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_binary_op(8, '+', df64_zero, df64_zero, &rval));
    gcc_gen_be_singleton_state.active_target_abi_override = (int)GCC_GEN_BE_TARGET_ABI_S390X + 1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_binary_op(4, '-', df32_zero, df32_zero, &rval));
    gcc_gen_be_singleton_state.active_target_abi_override = 0;

    if (gcc_gen_be_lower_decimal_float_unary_op(4, '-', NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_unary_op(4, '?', df32_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_unary_op(4, '-', df32_zero, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_unary_op(8, 'a', df64_zero, &rval));

    if (gcc_gen_be_lower_decimal_float_comparison(4, NULL, df32_zero, df32_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    const char *df_cmps[6] = { "eq", "ne", "lt", "le", "gt", "ge" };
    for (int i = 0; i < 6; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_comparison(4, df_cmps[i], df32_zero, df32_zero, &rval));
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_comparison(8, "eq", df64_zero, df64_zero, &rval));

    if (gcc_gen_be_lower_decimal_float_cast(4, 8, NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_cast(4, 4, df32_zero, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_cast(4, 8, df32_zero, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_cast(8, 4, df64_zero, &rval));

    if (gcc_gen_be_lower_decimal_to_binary_float(4, 3, df32_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_binary_float(4, 8, df32_zero, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_binary_float(8, 4, df64_zero, &rval));

    if (gcc_gen_be_lower_decimal_to_int(4, FALSE, NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_int(4, FALSE, df32_zero, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_int(8, TRUE, df64_zero, &rval));

    gcc_jit_type *int_jit_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_rvalue *int_zero = gcc_jit_context_new_rvalue_from_int(ctx, int_jit_t, 0);
    if (gcc_gen_be_lower_int_to_decimal(2, FALSE, 4, int_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_int_to_decimal(4, FALSE, 4, int_zero, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_int_to_decimal(8, TRUE, 8, df64_zero, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_int_to_decimal(8, FALSE, 16, df64_zero, &rval));

    gcc_jit_type *flt_jit_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
    gcc_jit_rvalue *flt_zero = gcc_jit_context_new_rvalue_from_double(ctx, flt_jit_t, 0.0);
    if (gcc_gen_be_lower_binary_float_to_decimal(3, 4, flt_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_binary_float_to_decimal(4, 8, flt_zero, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_binary_float_to_decimal(8, 4, flt_zero, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_binary_float_to_decimal(8, 16, flt_zero, &rval));

    if (gcc_gen_be_lower_fixed_point_binary_op(3, 8, FALSE, FALSE, '+', int_zero, int_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_binary_op(4, 32, FALSE, FALSE, '+', int_zero, int_zero, &rval) != GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_binary_op(4, 15, FALSE, FALSE, '?', int_zero, int_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    for (int i = 0; i < 4; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_binary_op(2, 7, FALSE, TRUE, df_ops[i], int_zero, int_zero, &rval));
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_binary_op(4, 16, TRUE, TRUE, '+', int_zero, int_zero, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_binary_op(8, 31, FALSE, FALSE, '*', df64_zero, df64_zero, &rval));

    a_type uint_tp = {};
    uint_tp.kind = tk_integer; uint_tp.size = 4; uint_tp.variant.integer.int_kind = ik_unsigned_int;
    a_field straddle_bf = {};
    straddle_bf.source_corresp.name = "straddle_bf";
    straddle_bf.type = &uint_tp;
    straddle_bf.is_bit_field = TRUE;
    straddle_bf.bit_size = 24;
    straddle_bf.offset = 0;
    straddle_bf.offset_bit_remainder = 16;
    a_type bf_struct_tp = {};
    bf_struct_tp.kind = tk_struct; bf_struct_tp.size = 8; bf_struct_tp.source_corresp.name = "StraddleBfStruct";
    bf_struct_tp.variant.class_struct_union.field_list = &straddle_bf;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&bf_struct_tp, &type));

    /* Stage-2 type coverage: >8B bitfield runs (`max_all_bf_end != max_bf_end` for req=1,2,4,8),
       tk_routine with extra_info == NULL, and union zero-rvalue construction */
    {
        const unsigned bf_bits[4] = {8U, 16U, 32U, 64U};
        for (int bi = 0; bi < 4; ++bi) {
            /* Unaligned u64 container (`size = 8`, `offset = 5`, `5 % 8 != 0`) forces `unit_sz = 0` -> lines 335..344,
               while trailing bitfield at `offset = 14` (`14 - 5 = 9 > 8`) makes `max_all_bf_end = 15 > max_bf_end`! */
            a_type u64_bf_t = {}; u64_bf_t.kind = tk_integer; u64_bf_t.size = 8; u64_bf_t.alignment = 8; u64_bf_t.variant.integer.int_kind = ik_unsigned_long_long;
            a_field bf_first = {}, bf_far = {};
            bf_first.source_corresp.name = "bf5"; bf_first.type = &u64_bf_t; bf_first.is_bit_field = TRUE;
            bf_first.offset = 5; bf_first.bit_size = bf_bits[bi]; bf_first.next = &bf_far;
            bf_far.source_corresp.name = "bf14"; bf_far.type = &uint_tp; bf_far.is_bit_field = TRUE;
            bf_far.offset = 14; bf_far.bit_size = 8;
            a_type long_bf_st = {};
            char st_nm[32]; std::snprintf(st_nm, sizeof(st_nm), "LongBfRun_%d", bi);
            long_bf_st.kind = tk_struct; long_bf_st.size = 16; long_bf_st.source_corresp.name = st_nm;
            long_bf_st.variant.class_struct_union.field_list = &bf_first;
            gcc_jit_type *lbf_jt = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&long_bf_st, &lbf_jt));
        }

        /* tk_routine with extra_info == NULL (902:14 False) */
        a_type rout_null_supp = {};
        rout_null_supp.kind = tk_routine;
        rout_null_supp.variant.routine.return_type = &uint_tp;
        rout_null_supp.variant.routine.extra_info = NULL;
        gcc_jit_type *rns_jt = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&rout_null_supp, &rns_jt));

        /* gcc_gen_be_get_zero_rvalue on tk_union (1093:26 True + OOM) and stripped==NULL with cached union jit_type (1131:17 True) */
        gcc_jit_type *dummy_vptr_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        a_field uf0 = {}; uf0.source_corresp.name = "u0"; uf0.type = &uint_tp;
        for (int b = 0; b < 10; ++b) {
            GCC_GEN_BE_CHECK(cache_clear_all());
            a_type u_edg_b = {};
            char unm[32]; std::snprintf(unm, sizeof(unm), "ZeroRvalUnion_%d", b);
            u_edg_b.kind = tk_union; u_edg_b.size = 4; u_edg_b.source_corresp.name = unm;
            u_edg_b.variant.class_struct_union.field_list = &uf0;
            gcc_jit_rvalue *uz_b = NULL;
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t ue = gcc_gen_be_get_zero_rvalue(&u_edg_b, dummy_vptr_jt, &uz_b);
            g_gcc_be_oom_countdown = -1;
            if (ue == GCC_GEN_BE_SUCCESS) break;
        }
        /* Non-bitfield field with typeref->NULL (401:60 False) followed by invalid _BitInt(0) field so libgccjit never sees void field */
        a_type tref_null_fld_tp = {}; tref_null_fld_tp.kind = tk_typeref; tref_null_fld_tp.variant.typeref.type = NULL;
        a_type bad_bp_fld_tp = {}; bad_bp_fld_tp.kind = tk_integer; bad_bp_fld_tp.size = 0; bad_bp_fld_tp.variant.integer.int_kind = ik_bit_precise;
        a_field f_tref_null_reg = {}, f_bad_bp_after = {};
        f_tref_null_reg.source_corresp.name = "tnf"; f_tref_null_reg.type = &tref_null_fld_tp; f_tref_null_reg.offset = 0; f_tref_null_reg.next = &f_bad_bp_after;
        f_bad_bp_after.source_corresp.name = "badbp"; f_bad_bp_after.type = &bad_bp_fld_tp; f_bad_bp_after.offset = 4;
        a_type st_tref_null_reg = {}; st_tref_null_reg.kind = tk_struct; st_tref_null_reg.size = 8; st_tref_null_reg.source_corresp.name = "TrefNullRegSt";
        st_tref_null_reg.variant.class_struct_union.field_list = &f_tref_null_reg;
        gcc_jit_type *tnr_jt = NULL;
        GCC_GEN_BE_CHECK(cache_clear_all());
        gcc_gen_be_singleton_state.deferred_structs_count = 0;
        if (gcc_gen_be_lower_type(&st_tref_null_reg, &tnr_jt) != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;
        gcc_gen_be_singleton_state.deferred_structs_count = 0;
        GCC_GEN_BE_CHECK(cache_clear_all());

        a_type ch1_t = {}; ch1_t.kind = tk_integer; ch1_t.size = 1; ch1_t.variant.integer.int_kind = ik_char;
        a_type u64_unalign_t = {}; u64_unalign_t.kind = tk_integer; u64_unalign_t.size = 8; u64_unalign_t.alignment = 8; u64_unalign_t.variant.integer.int_kind = ik_unsigned_long_long;
        a_field ext_pad0 = {}, ext_bf0 = {}, ext_bf1 = {};
        ext_pad0.source_corresp.name = "epad0"; ext_pad0.type = &ch1_t; ext_pad0.offset = 0; ext_pad0.next = &ext_bf0;
        ext_bf0.source_corresp.name = "ebf0"; ext_bf0.type = &u64_unalign_t; ext_bf0.is_bit_field = TRUE; ext_bf0.offset = 1; ext_bf0.bit_size = 56; ext_bf0.next = &ext_bf1;
        ext_bf1.source_corresp.name = "ebf1"; ext_bf1.type = &uint_tp; ext_bf1.is_bit_field = TRUE; ext_bf1.offset = 2; ext_bf1.bit_size = 8;
        for (int b = 0; b < 100; ++b) {
            GCC_GEN_BE_CHECK(cache_clear_all());
            gcc_gen_be_singleton_state.deferred_structs_count = 0;
            a_type ext_bf_st = {};
            char ebnm[32]; std::snprintf(ebnm, sizeof(ebnm), "ExtBfReallocSt_%d", b);
            ext_bf_st.kind = tk_struct; ext_bf_st.size = 16; ext_bf_st.source_corresp.name = ebnm;
            ext_bf_st.variant.class_struct_union.field_list = &ext_pad0;
            gcc_jit_type *eb_jt = NULL;
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t ebe = gcc_gen_be_lower_type(&ext_bf_st, &eb_jt);
            g_gcc_be_oom_countdown = -1;
            if (ebe == GCC_GEN_BE_SUCCESS) break;
        }
        a_type u_edg = {}; u_edg.kind = tk_union; u_edg.size = 4; u_edg.source_corresp.name = "ZeroRvalUnionMain";
        u_edg.variant.class_struct_union.field_list = &uf0;
        gcc_jit_type *u_jt = NULL;
        gcc_jit_rvalue *uz_rv = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&u_edg, &u_jt));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(&u_edg, u_jt, &uz_rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(NULL, u_jt, &uz_rv));
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_FIELD));
    }

    GCC_GEN_BE_CHECK(test_scalars_and_derived_types(ctx));
    GCC_GEN_BE_CHECK(test_complex_structs_bases_bitfields_and_deferred(ctx));
    GCC_GEN_BE_CHECK(test_decimal_fixed_and_bitint_norm(ctx));

    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_type.cpp`.
 * @return 0 on success, non-zero on failure.
 */
int main(void) noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_type_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
