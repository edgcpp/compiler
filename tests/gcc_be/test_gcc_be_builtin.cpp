/**
 * @file test_gcc_be_builtin.cpp
 * @brief Exhaustive unit tests for GCC, C11/C23, atomic, source_location, coroutine, sanitizer, and SIMD builtins (`gcc_gen_be_builtin.c`).
 * @details Exercises every public and internal path in `src/gcc_gen_be_builtin.c`, including deterministic
 * `cache_clear_all()` OOM sweeps across all helpers.
 * @ingroup gcc_gen_be_tests
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for builtin unit tests.
 * @details Returns the error code passed to it without terminating.
 * @param[in] err The error code.
 * @param[in] msg Optional diagnostic message.
 * @return The input error code `err`.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char *msg) GCC_GEN_BE_NOEXCEPT {
    return msg ? err : err;
}
}

USING_NAMESPACE_EDG

/** @brief Path buffer containing both forward slash and backslash separators for `__builtin_FILE_NAME`. */
static const char g_mock_bs_path[] = { '/', 'd', 'i', 'r', static_cast<char>(92), 'f', '.', 'c', '\0' };
/** @brief Configurable short file name returned by `conv_seq_to_file_and_line`. */
static const char *g_mock_raw_fname = "builtin_test.cpp";
/** @brief Configurable full file path returned by `conv_seq_to_file_and_line`. */
static const char *g_mock_full_fname = g_mock_bs_path;
/** @brief Configurable active coroutine frame type returned by `gcc_gen_be_get_active_coroutine_frame`. */
static gcc_jit_type *g_mock_coro_frame_type = NULL;
/** @brief When `TRUE`, `gcc_gen_be_lower_type` outputs `NULL` while returning `GCC_GEN_BE_SUCCESS`. */
static a_boolean g_lower_type_returns_null = FALSE;

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
 * @brief Stub for computing type size in builtin unit tests.
 * @details Returns `tp->size` directly.
 * @param[in] tp The frontend type pointer.
 * @return Byte size of `tp`.
 */
a_targ_size_t f_size_of_type(a_type_ptr tp) {
    while (tp && tp->kind == tk_typeref) tp = tp->variant.typeref.type;
    return tp ? tp->size : 0;
}

/**
 * @brief Stub for converting sequence number to file and line in builtin unit tests.
 * @details Populates `g_mock_raw_fname`, `g_mock_full_fname`, and line 42.
 * @param[in] seq Sequence number.
 * @param[out] file_name Output file name.
 * @param[out] full_name Output full path.
 * @param[out] line_number Output line number.
 * @param[out] at_end_of_source Output end-of-source flag.
 * @return NULL.
 */
a_source_file_ptr conv_seq_to_file_and_line(
    a_seq_number seq,
    a_const_char **file_name,
    a_const_char **full_name,
    a_line_number *line_number,
    a_boolean *at_end_of_source) {
    if (file_name) *file_name = g_mock_raw_fname;
    if (full_name) *full_name = g_mock_full_fname;
    if (line_number) *line_number = (a_line_number)(seq ? 42 : 0);
    if (at_end_of_source) *at_end_of_source = FALSE;
    return nullptr;
}

/**
 * @brief Stub for extracting signed integer constant in builtin unit tests.
 * @details Reads the first `long` word of `cp->variant.integer_value`.
 * @param[in] cp Constant pointer.
 * @param[out] err Overflow flag.
 * @return Extracted integer value.
 */
a_host_large_integer value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    if (err) *err = FALSE;
    if (!cp) return 0;
    long v = 0;
    std::memcpy(&v, &cp->variant.integer_value, sizeof(long) < sizeof(cp->variant.integer_value) ? sizeof(long) : sizeof(cp->variant.integer_value));
    return (a_host_large_integer)v;
}

/**
 * @brief Stub for type lowering in builtin unit tests.
 * @details Steps OOM budget and maps integer, float, pointer, vector, struct, and void types to `libgccjit`.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_type Pointer to receive the lowered `gcc_jit_type`.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_lower_type_returns_null || !tp) {
        *out_type = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;
    while (tp && tp->kind == tk_typeref) tp = tp->variant.typeref.type;
    if (!tp || tp->kind == tk_void) {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    } else if (tp->kind == tk_float) {
        if (tp->size == 4) *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
        else if (tp->size == 8) *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
        else *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
    } else if (tp->kind == tk_pointer) {
        gcc_jit_type *pt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        if (tp->variant.pointer.type && tp->variant.pointer.type->kind != tk_void) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.pointer.type, &pt));
        }
        *out_type = pt ? gcc_jit_type_get_pointer(pt) : gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    } else if (tp->kind == tk_vector) {
        void *cached_vt = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_TYPE, tp, &cached_vt));
        if (cached_vt) {
            *out_type = (gcc_jit_type *)cached_vt;
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_type *et = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
        if (tp->variant.vector.element_type) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.vector.element_type, &et));
        }
        size_t lanes = (tp->variant.vector.element_type && tp->variant.vector.element_type->size > 0 && tp->size >= tp->variant.vector.element_type->size)
            ? (size_t)(tp->size / tp->variant.vector.element_type->size)
            : 4U;
        *out_type = gcc_jit_type_get_vector(et, lanes);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, *out_type));
    } else if (tp->kind == tk_struct || tp->kind == tk_class) {
        gcc_jit_field *f = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "v");
        *out_type = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "__bt_st", 1, &f));
    } else if (tp->size == 8) {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG);
    } else if (tp->size == 16) {
        *out_type = gcc_jit_context_get_int_type(ctx, 16, 0);
    } else if (tp->size == 2) {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SHORT);
    } else if (tp->size == 1) {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
    } else {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for active coroutine frame lookup in builtin unit tests.
 * @details Steps OOM budget and outputs `g_mock_coro_frame_type`.
 * @param[out] out_frame_type Pointer to receive the active coroutine frame type.
 * @param[out] out_frame_lval Pointer to receive the active coroutine frame lvalue.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_active_coroutine_frame(struct gcc_jit_type **out_frame_type, struct gcc_jit_lvalue **out_frame_lval) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_frame_type) *out_frame_type = g_mock_coro_frame_type;
    if (out_frame_lval) *out_frame_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}
END_EDG_NAMESPACE

/**
 * @brief Mock `gcc_jit_context_new_sizeof` callback for `__builtin_coro_size` struct frame testing.
 * @param[in] ctx Active context.
 * @param[in] tp Target `gcc_jit_type`.
 * @return Constant `size_t` 128 rvalue.
 */
static gcc_jit_rvalue *mock_new_sizeof(gcc_jit_context *ctx, gcc_jit_type *tp) noexcept {
    return tp ? gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T), 128) : nullptr;
}

/**
 * @brief Mock `gcc_jit_context_new_rvalue_vector_perm` callback for `__builtin_shufflevector`.
 * @param[in] ctx Active context.
 * @param[in] loc Location.
 * @param[in] v1 First vector.
 * @param[in] v2 Second vector.
 * @param[in] mask Mask vector.
 * @return `v1`.
 */
static gcc_jit_rvalue *mock_vec_perm(
    gcc_jit_context *ctx,
    gcc_jit_location *loc,
    gcc_jit_rvalue *v1,
    gcc_jit_rvalue *v2,
    gcc_jit_rvalue *mask) noexcept {
    return (ctx && !loc && v2 && mask) ? v1 : v1;
}

/**
 * @brief Mock `gcc_jit_context_new_bitcast` callback for vector/scalar bitcast testing when `cblock == NULL`.
 * @param[in] ctx Active context.
 * @param[in] loc Location.
 * @param[in] rval Input rvalue.
 * @param[in] dst_t Destination type.
 * @return Zero of `dst_t` or `rval`.
 */
static gcc_jit_rvalue *mock_bitcast(
    gcc_jit_context *ctx,
    gcc_jit_location *loc,
    gcc_jit_rvalue *rval,
    gcc_jit_type *dst_t) noexcept {
    if (!ctx || loc || !rval) return rval;
    return dst_t ? rval : rval;
}

/**
 * @brief Mock `gcc_jit_context_new_struct_constructor` callback for `__builtin_source_location`.
 * @param[in] ctx Active context.
 * @param[in] loc Location.
 * @param[in] tp Struct type.
 * @param[in] nval Field count.
 * @param[in] fields Fields array.
 * @param[in] vals Values array.
 * @return Zeroed struct local/rvalue via `gcc_jit_context_new_array_constructor` workaround or first value.
 */
static gcc_jit_rvalue *mock_struct_ctor(
    gcc_jit_context *ctx,
    gcc_jit_location *loc,
    gcc_jit_type *tp,
    size_t nval,
    gcc_jit_field **fields,
    gcc_jit_rvalue **vals) noexcept {
    if (p_gcc_jit_context_new_struct_constructor && p_gcc_jit_context_new_struct_constructor != &mock_struct_ctor) {
        return p_gcc_jit_context_new_struct_constructor(ctx, loc, tp, nval, fields, vals);
    }
    return vals[0];
}

/**
 * @brief Exercises standalone helper APIs: `bit_cast`, `validate_simd_immediate`, `eval_builtin_object_size`,
 * `clear_padding`, and `source_location`.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] fn Active `gcc_jit_function`.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
static gcc_gen_be_error_t test_helper_apis(gcc_jit_context *ctx, gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_rvalue *rv = NULL;
    gcc_jit_type *i32_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *f32_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
    gcc_jit_type *void_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *vptr_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_rvalue *i32_rv = gcc_jit_context_new_rvalue_from_int(ctx, i32_jt, 7);
    gcc_jit_rvalue *ptr_rv = gcc_jit_context_null(ctx, vptr_jt);

    a_type t_i32 = {}, t_f32 = {}, t_i64 = {}, t_ptr = {}, t_st = {}, t_void = {};
    t_i32.kind = tk_integer; t_i32.size = 4;
    t_f32.kind = tk_float; t_f32.size = 4;
    t_i64.kind = tk_integer; t_i64.size = 8;
    t_ptr.kind = tk_pointer; t_ptr.size = 8; t_ptr.variant.pointer.type = &t_i32;
    t_st.kind = tk_struct; t_st.size = 4;
    t_void.kind = tk_void; t_void.size = 0;

    /* 1. gcc_gen_be_lower_builtin_bit_cast */
    if (gcc_gen_be_lower_builtin_bit_cast(&t_f32, &t_i32, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_builtin_bit_cast(&t_f32, &t_i32, i32_rv, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_builtin_bit_cast(&t_i64, &t_i32, i32_rv, &rv) != GCC_GEN_BE_ERROR_TYPE_MISMATCH) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_builtin_bit_cast(&t_f32, &t_i32, i32_rv, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* dst_type == NULL, dst_t == src_t, dst_t == NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(NULL, NULL, i32_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_i32, &t_i32, i32_rv, &rv));
    g_lower_type_returns_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_f32, &t_i32, i32_rv, &rv));
    g_lower_type_returns_null = FALSE;

    /* With active cblock != NULL (stack memory punning) and 0-sized void dst_t */
    gcc_jit_block *blk = gcc_jit_function_new_block(fn, "bc_blk");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_f32, &t_i32, i32_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_void, NULL, i32_rv, &rv));
    gcc_jit_rvalue *void_call_rv = gcc_jit_context_new_call(
        ctx, NULL, gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_jt, "__void_src_fn", 0, NULL, 0), 0, NULL);
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_f32, NULL, void_call_rv, &rv));

    /* With cblock == NULL and p_gcc_jit_context_new_bitcast non-NULL & NULL across integer, float, pointer, struct, size mismatch, 0-size */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    auto saved_bc = p_gcc_jit_context_new_bitcast;
    p_gcc_jit_context_new_bitcast = &mock_bitcast;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_f32, &t_i32, i32_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_i32, &t_f32, gcc_jit_context_new_rvalue_from_double(ctx, f32_jt, 1.0), &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_ptr, NULL, gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG), 0), &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_st, &t_i32, i32_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_i64, NULL, i32_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_f32, NULL, void_call_rv, &rv));
    {
        a_type t_f64 = {}, t_f128 = {}, t_zint = {};
        t_f64.kind = tk_float; t_f64.size = 8;
        t_f128.kind = tk_float; t_f128.size = 16;
        t_zint.kind = tk_integer; t_zint.size = 0;
        gcc_jit_rvalue *f64_c = gcc_jit_context_new_rvalue_from_double(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE), 1.0);
        gcc_jit_rvalue *ld_c = gcc_jit_context_new_rvalue_from_double(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE), 1.0);
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_i64, &t_f64, f64_c, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_i32, NULL, ld_c, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_f32, &t_i32, void_call_rv, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_zint, NULL, void_call_rv, &rv));
    }
    p_gcc_jit_context_new_bitcast = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&t_f32, &t_i32, i32_rv, &rv));
    p_gcc_jit_context_new_bitcast = saved_bc;

    for (int b = 0; b < 20; ++b) {
        GCC_GEN_BE_CHECK(cache_clear_all());
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_builtin_bit_cast(&t_f32, &t_i32, i32_rv, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* 2. gcc_gen_be_validate_simd_immediate */
    a_boolean is_valid = FALSE;
    if (gcc_gen_be_validate_simd_immediate(NULL, NULL, 0, 7, &is_valid) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_validate_simd_immediate("imm", NULL, 0, 7, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_validate_simd_immediate("imm", NULL, 0, 7, &is_valid) != GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    an_expr_node imm_e = {};
    imm_e.kind = enk_variable;
    if (gcc_gen_be_validate_simd_immediate("imm", &imm_e, 0, 7, &is_valid) != GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    imm_e.kind = enk_constant;
    imm_e.variant.constant.ptr = NULL;
    if (gcc_gen_be_validate_simd_immediate("imm", &imm_e, 0, 7, &is_valid) != GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    a_constant imm_c = {};
    imm_c.kind = ck_float;
    imm_e.variant.constant.ptr = &imm_c;
    if (gcc_gen_be_validate_simd_immediate("imm", &imm_e, 0, 7, &is_valid) != GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    imm_c.kind = ck_integer;
    long v_neg = -1, v_big = 99, v_ok = 3;
    std::memcpy(&imm_c.variant.integer_value, &v_neg, sizeof(long));
    if (gcc_gen_be_validate_simd_immediate("imm", &imm_e, 0, 7, &is_valid) != GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    std::memcpy(&imm_c.variant.integer_value, &v_big, sizeof(long));
    if (gcc_gen_be_validate_simd_immediate("imm", &imm_e, 0, 7, &is_valid) != GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    std::memcpy(&imm_c.variant.integer_value, &v_ok, sizeof(long));
    GCC_GEN_BE_CHECK(gcc_gen_be_validate_simd_immediate("imm", &imm_e, 0, 7, &is_valid));
    if (!is_valid) return GCC_GEN_BE_ERROR_INTERNAL;

    /* 3. gcc_gen_be_eval_builtin_object_size */
    size_t bos = 0;
    if (gcc_gen_be_eval_builtin_object_size(NULL, 0, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_eval_builtin_object_size(NULL, 0, &bos));
    GCC_GEN_BE_CHECK(gcc_gen_be_eval_builtin_object_size(&t_i32, 2, &bos));

    a_type tref_p = {}, ptr_null = {}, tref_pt = {}, pt_tp = {};
    ptr_null.kind = tk_pointer; ptr_null.variant.pointer.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_eval_builtin_object_size(&ptr_null, 0, &bos));

    tref_pt.kind = tk_typeref; tref_pt.variant.typeref.type = NULL;
    t_ptr.variant.pointer.type = &tref_pt;
    tref_p.kind = tk_typeref; tref_p.variant.typeref.type = &t_ptr;
    GCC_GEN_BE_CHECK(gcc_gen_be_eval_builtin_object_size(&tref_p, 0, &bos));

    a_type_kind bos_kinds[] = { tk_struct, tk_class, tk_union, tk_array, tk_integer };
    for (size_t k = 0; k < sizeof(bos_kinds) / sizeof(bos_kinds[0]); ++k) {
        pt_tp = {};
        pt_tp.kind = bos_kinds[k];
        pt_tp.size = (k == 0) ? 0 : 24;
        tref_pt.variant.typeref.type = &pt_tp;
        GCC_GEN_BE_CHECK(gcc_gen_be_eval_builtin_object_size(&tref_p, 0, &bos));
    }
    pt_tp.kind = tk_struct; pt_tp.size = 16;
    GCC_GEN_BE_CHECK(gcc_gen_be_eval_builtin_object_size(&tref_p, 0, &bos));
    tref_p.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_eval_builtin_object_size(&tref_p, 0, &bos));
    tref_p.variant.typeref.type = &t_ptr;
    tref_pt.variant.typeref.type = &pt_tp;
    for (int b = 0; b < 10; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_eval_builtin_object_size(&tref_p, 0, &bos);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* 4. gcc_gen_be_lower_builtin_clear_padding */
    if (gcc_gen_be_lower_builtin_clear_padding(&t_ptr, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_builtin_clear_padding(&t_ptr, ptr_rv, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_builtin_clear_padding(&t_ptr, ptr_rv, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    a_field fld1 = {}, fld2 = {}, fld3 = {};
    a_type f_u8 = {};
    f_u8.kind = tk_integer; f_u8.size = 1;
    fld1.offset = 0; fld1.type = &f_u8; fld1.next = &fld2;
    fld2.offset = 0; fld2.type = NULL; fld2.next = &fld3; /* f_off + f_sz <= prev_end & f->type == NULL */
    fld3.offset = 4; fld3.type = &f_u8; fld3.next = NULL; /* f_off > prev_end (1..3) + tail padding (5..7) + f_off >= total_sz test */

    a_type pad_st = {}, pad_cls = {}, pad_zero = {};
    pad_st.kind = tk_struct; pad_st.size = 8; pad_st.variant.class_struct_union.field_list = &fld1;
    pad_cls.kind = tk_class; pad_cls.size = 3; pad_cls.variant.class_struct_union.field_list = &fld3; /* f_off (4) >= total_sz (3) */
    pad_zero.kind = tk_struct; pad_zero.size = 0;

    tref_pt.variant.typeref.type = &pad_st;
    t_ptr.variant.pointer.type = &tref_pt;
    tref_p.variant.typeref.type = &t_ptr;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_clear_padding(&tref_p, ptr_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_clear_padding(&tref_p, ptr_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_clear_padding(&pad_cls, ptr_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_clear_padding(&pad_zero, ptr_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_clear_padding(&t_i32, ptr_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_clear_padding(NULL, ptr_rv, &rv));
    tref_pt.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_clear_padding(&tref_p, ptr_rv, &rv));
    tref_p.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_clear_padding(&tref_p, ptr_rv, &rv));

    for (int b = 0; b < 10; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_builtin_clear_padding(&pad_st, ptr_rv, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* 5. gcc_gen_be_lower_source_location_builtin */
    if (gcc_gen_be_lower_source_location_builtin(NULL, NULL, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_source_location_builtin("__builtin_FILE", NULL, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_source_location_builtin("__builtin_FILE", NULL, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    a_source_position pos0 = {}, pos1 = {};
    pos0.seq = 0; pos0.column = 5;
    pos1.seq = 10; pos1.column = 9;

    gcc_gen_be_singleton_state.active_routine_name = "my_func";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_source_location_builtin("__builtin_FILE", NULL, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_source_location_builtin("__builtin_FILE", &pos0, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_source_location_builtin("__builtin_FILE_NAME", &pos1, NULL, &rv));
    g_mock_full_fname = "";
    g_mock_raw_fname = "C:\\dir\\raw_only.cpp";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_source_location_builtin("__builtin_FILE_NAME", &pos1, NULL, &rv));
    g_mock_full_fname = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_source_location_builtin("__builtin_FILE_NAME", &pos1, NULL, &rv));
    g_mock_raw_fname = NULL;
    gcc_gen_be_singleton_state.active_routine_name = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_source_location_builtin("__builtin_FUNCTION", &pos1, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_source_location_builtin("__builtin_LINE", &pos1, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_source_location_builtin("__builtin_COLUMN", &pos1, &t_i32, &rv));

    /* __builtin_source_location with struct ctor non-NULL & NULL, pointer/non-pointer/null result_type */
    auto saved_sc = p_gcc_jit_context_new_struct_constructor;
    if (!p_gcc_jit_context_new_struct_constructor) {
        p_gcc_jit_context_new_struct_constructor = &mock_struct_ctor;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_source_location_builtin("__builtin_source_location", &pos1, &t_ptr, &rv));
    p_gcc_jit_context_new_struct_constructor = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_source_location_builtin("__builtin_source_location", &pos1, &t_i32, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_source_location_builtin("__builtin_source_location", &pos1, NULL, &rv));
    g_lower_type_returns_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_source_location_builtin("__builtin_source_location", &pos1, &t_ptr, &rv));
    g_lower_type_returns_null = FALSE;
    p_gcc_jit_context_new_struct_constructor = saved_sc;

    if (gcc_gen_be_lower_source_location_builtin("__builtin_unknown_loc", &pos1, NULL, &rv) != GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    for (int b = 0; b < 10; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_source_location_builtin("__builtin_LINE", &pos1, &t_i32, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 10; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_source_location_builtin("__builtin_source_location", &pos1, &t_ptr, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    g_mock_raw_fname = "builtin_test.cpp";
    g_mock_full_fname = "/src/builtin_test.cpp";
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises atomic, C11 atomic, C23 `<stdbit.h>`, math/bitwise, coroutine, SIMD, and sanitizer builtins.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] fn Active `gcc_jit_function`.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
static gcc_gen_be_error_t test_all_builtin_categories(gcc_jit_context *ctx, gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_rvalue *rv = NULL;
    gcc_jit_type *i32_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *u64_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG);
    gcc_jit_type *u128_jt = gcc_jit_context_get_int_type(ctx, 16, 0);
    gcc_jit_type *f64_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
    gcc_jit_type *f32_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
    gcc_jit_type *v4f_jt = gcc_jit_type_get_vector(f32_jt, 4);
    gcc_jit_type *v4i_jt = gcc_jit_type_get_vector(i32_jt, 4);
    gcc_jit_type *vptr_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *void_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);

    gcc_jit_rvalue *i32_rv = gcc_jit_context_new_rvalue_from_int(ctx, i32_jt, 3);
    gcc_jit_rvalue *u64_rv = gcc_jit_context_new_rvalue_from_long(ctx, u64_jt, 5);
    gcc_jit_rvalue *u128_rv = gcc_jit_context_new_rvalue_from_int(ctx, u128_jt, 9);
    gcc_jit_rvalue *f64_rv = gcc_jit_context_new_rvalue_from_double(ctx, f64_jt, 2.5);
    gcc_jit_rvalue *ptr_rv = gcc_jit_context_null(ctx, vptr_jt);
    gcc_jit_rvalue *f32_lanes[4] = {
        gcc_jit_context_new_rvalue_from_double(ctx, f32_jt, 1.0),
        gcc_jit_context_new_rvalue_from_double(ctx, f32_jt, 2.0),
        gcc_jit_context_new_rvalue_from_double(ctx, f32_jt, 3.0),
        gcc_jit_context_new_rvalue_from_double(ctx, f32_jt, 4.0)
    };
    gcc_jit_rvalue *i32_lanes[4] = { i32_rv, i32_rv, i32_rv, i32_rv };
    gcc_jit_rvalue *v4f_rv = gcc_jit_context_new_rvalue_from_vector(ctx, NULL, v4f_jt, 4, f32_lanes);
    gcc_jit_rvalue *v4i_rv = gcc_jit_context_new_rvalue_from_vector(ctx, NULL, v4i_jt, 4, i32_lanes);

    gcc_jit_block *blk = gcc_jit_function_new_block(fn, "all_builtins_blk");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));

    /* Unnamed builtin routine dispatch */
    a_routine rout = {};
    if (gcc_gen_be_lower_builtin_call(&rout, 0, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    rout.special_kind = sfk_constructor;
    if (gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    rout.special_kind = sfk_none;
    rout.variant.builtin_function_kind = (a_builtin_function_kind)1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));

    /* ctx == NULL across __c11_atomic_, __atomic_, __builtin_coro_, simd, sanitizer, and math */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    const char *null_ctx_names[] = {
        "__c11_atomic_load", "__atomic_load_n", "__builtin_coro_done", "_mm_add_ps", "__asan_init", "__builtin_sin"
    };
    for (size_t i = 0; i < sizeof(null_ctx_names) / sizeof(null_ctx_names[0]); ++i) {
        rout.source_corresp.name = null_ctx_names[i];
        if (gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* 1. get_builtin variations: p_gcc_jit_context_get_builtin_function NULL vs valid, libc_substrs, libc_prefixes, cached hit */
    const char *gb_names[] = {
        "__builtin_va_start", "__builtin_char_memchr", "__builtin_strlen", "__builtin_nonexistent_xyz", "custom_libc_fn"
    };
    gcc_jit_rvalue *one_arg[2] = { ptr_rv, i32_rv };
    for (size_t i = 0; i < sizeof(gb_names) / sizeof(gb_names[0]); ++i) {
        rout.source_corresp.name = gb_names[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, one_arg, &rv));
        /* Second call hits cached_imp! */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, one_arg, &rv));
    }
    auto saved_gbf = p_gcc_jit_context_get_builtin_function;
    p_gcc_jit_context_get_builtin_function = NULL;
    rout.source_corresp.name = "__builtin_fabs";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &f64_rv, &rv));
    p_gcc_jit_context_get_builtin_function = saved_gbf;

    /* 2. Atomic & __c11_atomic_* builtins */
    a_type t_i32 = {}, t_u64 = {}, t_u128 = {}, t_odd24 = {}, t_zero = {}, tref_e = {}, t_ptr = {}, tref_p = {};
    t_i32.kind = tk_integer; t_i32.size = 4;
    t_u64.kind = tk_integer; t_u64.size = 8;
    t_u128.kind = tk_integer; t_u128.size = 16;
    t_odd24.kind = tk_struct; t_odd24.size = 24;
    t_zero.kind = tk_void; t_zero.size = 0;
    tref_e.kind = tk_typeref; tref_e.variant.typeref.type = &t_i32;
    t_ptr.kind = tk_pointer; t_ptr.size = 8; t_ptr.variant.pointer.type = &tref_e;
    tref_p.kind = tk_typeref; tref_p.variant.typeref.type = &t_ptr;

    a_param_type p0 = {}, p1 = {};
    a_routine_type_supplement rts = {};
    a_type rtp = {};
    rtp.kind = tk_routine; rtp.variant.routine.extra_info = &rts;

    /* __atomic_always_lock_free / __atomic_is_lock_free */
    rout.type = &t_zero; /* non-tk_routine rout->type */
    rout.source_corresp.name = "__atomic_always_lock_free";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv));
    rout.type = &rtp;
    rtp.variant.routine.extra_info = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv));
    rtp.variant.routine.extra_info = &rts;
    rout.source_corresp.name = "__atomic_is_lock_free";
    rts.param_type_list = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv));
    p0.type = &t_u64; p0.next = NULL; rts.param_type_list = &p0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv));
    p0.next = &p1; p1.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv));
    p1.type = &t_i32;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv));
    p1.type = &tref_p;
    long lf_widths[] = { 1, 2, 4, 8, 16, 24, 0 };
    for (size_t wi = 0; wi < sizeof(lf_widths) / sizeof(lf_widths[0]); ++wi) {
        a_type wt = {}; wt.kind = tk_integer; wt.size = (a_targ_size_t)lf_widths[wi];
        tref_e.variant.typeref.type = &wt;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv));
    }
    tref_e.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv));
    t_ptr.variant.pointer.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv));
    tref_p.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv));
    tref_p.variant.typeref.type = &t_ptr;
    t_ptr.variant.pointer.type = &tref_e;
    tref_e.variant.typeref.type = &t_i32;

    /* Alignment failure on odd return_type and odd param_type_list, plus non-pointer p0 & null pointer.type */
    p0.type = &t_i32; p0.next = NULL;
    rtp.variant.routine.return_type = NULL;
    rout.source_corresp.name = "__atomic_load_n";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, one_arg, &rv));
    p0.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, one_arg, &rv));
    t_ptr.variant.pointer.type = NULL;
    p0.type = &t_ptr;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, one_arg, &rv));
    t_ptr.variant.pointer.type = &tref_e;
    p0.type = &tref_p;
    tref_p.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, one_arg, &rv));
    tref_p.variant.typeref.type = &t_ptr;

    rtp.variant.routine.return_type = &t_odd24;
    if (gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv) != GCC_GEN_BE_ERROR_ATOMIC_ALIGNMENT_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    rtp.variant.routine.return_type = &t_zero;
    tref_e.variant.typeref.type = &t_odd24;
    if (gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv) != GCC_GEN_BE_ERROR_ATOMIC_ALIGNMENT_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    tref_e.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, one_arg, &rv));

    /* Return type NULL from lower_type, void atomics, width inference from return_type / args[1] / ret_type, 16-byte atomics */
    g_lower_type_returns_null = TRUE;
    rtp.variant.routine.return_type = &t_i32;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, one_arg, &rv));
    g_lower_type_returns_null = FALSE;

    rout.type = NULL;
    rout.source_corresp.name = "__atomic_clear";
    if (gcc_gen_be_lower_builtin_call(&rout, -1, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_builtin_call(&rout, 2, NULL, &rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));

    gcc_jit_rvalue *void_rv = gcc_jit_context_new_call(
        ctx, NULL, gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_jt, "__void_b_fn", 0, NULL, 0), 0, NULL);

    gcc_jit_rvalue *at_args8[10] = { ptr_rv, i32_rv, i32_rv, i32_rv, i32_rv, i32_rv, i32_rv, i32_rv, i32_rv, i32_rv };
    rout.source_corresp.name = "__atomic_store_n";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, at_args8, &rv));
    at_args8[1] = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, at_args8, &rv));
    at_args8[1] = void_rv; /* a1_sz == 0 -> !a1_ok */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, at_args8, &rv));
    at_args8[1] = u128_rv;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, at_args8, &rv));
    rout.source_corresp.name = "__atomic_load_n";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, one_arg, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 10, at_args8, &rv));
    /* r_sz == 0 on __atomic_load_n when ret_type is 0-sized struct */
    {
        a_type zero_st = {}; zero_st.kind = tk_struct; zero_st.size = 0;
        rout.type = &rtp; rts.param_type_list = NULL; rtp.variant.routine.return_type = &zero_st;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, one_arg, &rv));
        rout.type = NULL;
    }
    /* Generic __atomic_load with <3 args falls through; >=3 args forwards to gcc_gen_be_lower_generic_atomic_call */
    rout.source_corresp.name = "__atomic_load";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, one_arg, &rv));
    gcc_jit_rvalue *gen_at_args[3] = { ptr_rv, ptr_rv, i32_rv };
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, gen_at_args, &rv));

    /* __sync_fetch_and_add_4 exercises strncmp(name, "__sync_", 7) == 0 dispatch */
    rout.source_corresp.name = "__sync_fetch_and_add_4";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, gen_at_args, &rv));

    /* Sized mapping for exchange_n, compare_exchange_n, fetch_add, fetch_add_4, test_and_set, and 16-byte load_n (0 & 2 args) */
    rout.type = &rtp;
    rts.param_type_list = NULL;
    rtp.variant.routine.return_type = &t_i32;
    const char *at_ops[] = {
        "__atomic_exchange_n", "__atomic_compare_exchange_n", "__atomic_fetch_add", "__atomic_fetch_add_4", "__atomic_test_and_set"
    };
    at_args8[1] = i32_rv;
    for (size_t i = 0; i < sizeof(at_ops) / sizeof(at_ops[0]); ++i) {
        rout.source_corresp.name = at_ops[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, at_args8, &rv));
    }
    rtp.variant.routine.return_type = &t_u128;
    rout.source_corresp.name = "__atomic_load_n";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, at_args8, &rv));
    rout.source_corresp.name = "__atomic_test_and_set";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));

    /* Argument coercion in lower_atomic_builtin: int->ptr (i32 & u64, cblock!=NULL & cblock==NULL), ptr->int (i32 & u64, cblock!=NULL & NULL), int->u64 */
    {
        gcc_jit_type *cp_pts[3] = { vptr_jt, u64_jt, i32_jt };
        gcc_jit_param *cp_ps[3] = {
            gcc_jit_context_new_param(ctx, NULL, cp_pts[0], "a0"),
            gcc_jit_context_new_param(ctx, NULL, cp_pts[1], "a1"),
            gcc_jit_context_new_param(ctx, NULL, cp_pts[2], "a2")
        };
        gcc_jit_function *cp_fn = gcc_jit_context_new_function(
            ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, i32_jt, "__atomic_custom_coerced", 3, cp_ps, 0);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)".imp:__atomic_custom_coerced", cp_fn));

        rout.type = NULL;
        rout.source_corresp.name = "__atomic_custom_coerced";
        gcc_jit_rvalue *ca1[3] = { i32_rv, ptr_rv, u64_rv };
        gcc_jit_rvalue *ca2[3] = { u64_rv, ptr_rv, NULL };
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, ca1, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, ca2, &rv));
        gcc_jit_block *saved_b = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_b));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, ca1, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_b));

        gcc_jit_rvalue *ca3[3] = { ptr_rv, u64_rv, ptr_rv };
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, ca3, &rv));
    }

    /* __c11_atomic_init, compare_exchange_strong/weak, load, store, exchange, fetch_add */
    gcc_jit_rvalue *c11_args[5] = { ptr_rv, ptr_rv, i32_rv, i32_rv, i32_rv };
    rout.type = &rtp;
    rts.param_type_list = &p0;
    p0.type = &t_ptr;
    tref_e.variant.typeref.type = &t_i32;
    rtp.variant.routine.return_type = &t_i32;

    rout.source_corresp.name = "__c11_atomic_init";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, c11_args, &rv));
    {
        gcc_jit_block *saved_b = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_b));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, c11_args, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_b));
    }
    gcc_jit_rvalue *c11_null0[2] = { NULL, i32_rv };
    gcc_jit_rvalue *c11_null1[2] = { ptr_rv, NULL };
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, c11_null0, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, c11_null1, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, c11_args, &rv));

    rout.source_corresp.name = "__c11_atomic_compare_exchange_strong";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 5, c11_args, &rv));
    rout.source_corresp.name = "__c11_atomic_compare_exchange_weak";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 5, c11_args, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, c11_args, &rv));
    rout.source_corresp.name = "__c11_atomic_load";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, c11_args, &rv));
    rout.source_corresp.name = "__c11_atomic_store";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, c11_args, &rv));
    rout.source_corresp.name = "__c11_atomic_exchange";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, c11_args, &rv));
    rout.source_corresp.name = "__c11_atomic_fetch_add";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, c11_args, &rv));

    /* 3. C23 `<stdbit.h>` & type-generic `*g` & `bswap128` */
    rout.type = NULL;
    rout.source_corresp.name = "__builtin_bswap128";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &u128_rv, &rv));

    {
        gcc_jit_block *saved_b = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_b));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        rout.source_corresp.name = "__builtin_clzg";
        if (gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_b));
    }

    rout.source_corresp.name = "__builtin_stdc_rotate_left";
    if (gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv) != GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_jit_rvalue *rot_null[2] = { i32_rv, NULL };
    if (gcc_gen_be_lower_builtin_call(&rout, 2, rot_null, &rv) != GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH) return GCC_GEN_BE_ERROR_INTERNAL;

    const char *bit_names[] = {
        "__builtin_popcountg", "__builtin_stdc_count_ones", "__builtin_stdc_count_zeros",
        "__builtin_parityg", "__builtin_stdc_has_single_bit",
        "__builtin_stdc_rotate_left", "__builtin_stdc_rotate_right",
        "__builtin_stdc_bit_floor", "__builtin_stdc_bit_ceil",
        "__builtin_clzg", "__builtin_ctzg", "__builtin_ffsg",
        "__builtin_stdc_leading_zeros", "__builtin_stdc_leading_ones",
        "__builtin_stdc_trailing_zeros", "__builtin_stdc_trailing_ones",
        "__builtin_stdc_first_leading_one", "__builtin_stdc_first_leading_zero",
        "__builtin_stdc_first_trailing_one", "__builtin_stdc_first_trailing_zero",
        "__builtin_stdc_bit_width"
    };
    gcc_jit_rvalue *bit_args2[2] = { i32_rv, i32_rv };
    for (size_t i = 0; i < sizeof(bit_names) / sizeof(bit_names[0]); ++i) {
        rout.source_corresp.name = bit_names[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, bit_args2, &rv));
    }
    rout.source_corresp.name = "__builtin_clzg";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, rot_null, &rv));
    p0.type = &t_u128; rts.param_type_list = &p0; rout.type = &rtp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &u128_rv, &rv));
    p0.type = &t_zero; /* first_param->type->size == 0 */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
    p0.type = NULL; /* first_param->type == NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
    rout.type = NULL;
    rout.source_corresp.name = "__builtin_popcountg";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &void_rv, &rv));

    /* 4. Math, vector reduce, fpclassify, shuffle/convertvector, fortified _chk, types_compatible_p, SEH */
    rout.source_corresp.name = "__builtin_FILE";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    rout.type = &rtp; rtp.variant.routine.return_type = &t_ptr;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    rout.type = NULL;

    rout.source_corresp.name = "__builtin_addressof";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    gcc_jit_rvalue *null_arg1[1] = { NULL };
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_arg1, &rv));
    rout.source_corresp.name = "__addressof";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));

    /* __builtin_reduce_* */
    a_type vec_tp = {}, vec_tiny = {}, vec_noel = {}, vec_zsz = {}, tref_v = {};
    vec_tp.kind = tk_vector; vec_tp.size = 16; vec_tp.variant.vector.element_type = &t_i32;
    vec_tiny.kind = tk_vector; vec_tiny.size = 2; vec_tiny.variant.vector.element_type = &t_i32;
    vec_noel.kind = tk_vector; vec_noel.size = 16; vec_noel.variant.vector.element_type = NULL;
    vec_zsz.kind = tk_vector; vec_zsz.size = 16; vec_zsz.variant.vector.element_type = &t_zero;
    tref_v.kind = tk_typeref; tref_v.variant.typeref.type = &vec_tp;
    p0.type = &tref_v; rts.param_type_list = &p0; rtp.variant.routine.return_type = &t_i32;
    rout.type = &rtp;
    const char *red_names[] = { "__builtin_reduce_or", "__builtin_reduce_and", "__builtin_reduce_add", "__builtin_reduce_xor" };
    for (size_t i = 0; i < 4; ++i) {
        rout.source_corresp.name = red_names[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4i_rv, &rv));
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_arg1, &rv));
    tref_v.variant.typeref.type = &vec_tiny;
    g_lower_type_returns_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4i_rv, &rv));
    g_lower_type_returns_null = FALSE;
    tref_v.variant.typeref.type = &vec_noel;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4i_rv, &rv));
    tref_v.variant.typeref.type = &vec_zsz;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4i_rv, &rv));
    tref_v.variant.typeref.type = &t_i32;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4i_rv, &rv));
    tref_v.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4i_rv, &rv));
    p0.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4i_rv, &rv));
    rout.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4i_rv, &rv));
    {
        gcc_jit_block *saved_b = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_b));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4i_rv, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_b));
    }

    /* isnan / isfinite / isinf / isnormal / fpclassify / trap / unreachable */
    const char *fp_preds[] = { "__builtin_isnan", "__builtin_isfinite", "__builtin_isinf", "__builtin_isnormal" };
    for (size_t i = 0; i < 4; ++i) {
        rout.source_corresp.name = fp_preds[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &f64_rv, &rv));
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_arg1, &rv));

    rout.source_corresp.name = "__builtin_assume";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
    gcc_jit_rvalue *addc_args[4] = { i32_rv, i32_rv, i32_rv, ptr_rv };
    rout.source_corresp.name = "__builtin_addcll";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 4, addc_args, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, addc_args, &rv));
    addc_args[0] = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 4, addc_args, &rv));
    addc_args[0] = i32_rv;
    {
        gcc_jit_block *saved_b_addc = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_b_addc));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 4, addc_args, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_b_addc));
    }
    rout.source_corresp.name = "__builtin_subcll";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 4, addc_args, &rv));
    rout.source_corresp.name = "__builtin_unreachable";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    rout.source_corresp.name = "__builtin_trap";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));

    gcc_jit_rvalue *fpc_args[6] = { i32_rv, i32_rv, i32_rv, i32_rv, i32_rv, f64_rv };
    rout.source_corresp.name = "__builtin_fpclassify";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 6, fpc_args, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, fpc_args, &rv));
    fpc_args[5] = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 6, fpc_args, &rv));
    fpc_args[5] = f64_rv;
    {
        gcc_jit_block *saved_b = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_b));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 6, fpc_args, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_b));
    }

    /* launder / assume_aligned / expect / frame_address / return_address / dwarf_cfa */
    const char *pass_names[] = {
        "__builtin_launder", "__builtin_assume_aligned", "__builtin_expect",
        "__builtin_expect_with_probability", "__builtin_extract_return_addr"
    };
    for (size_t i = 0; i < 5; ++i) {
        rout.source_corresp.name = pass_names[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));
    }
    rout.source_corresp.name = "__builtin_launder";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_arg1, &rv));

    rout.source_corresp.name = "__builtin_frame_address";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
    gcc_jit_rvalue *null_fa[1] = { NULL };
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_fa, &rv));
    rout.source_corresp.name = "__builtin_return_address";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
    rout.source_corresp.name = "__builtin_dwarf_cfa";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));

    /* shufflevector / shuffle / convertvector / is_constant_evaluated / va_arg_pack / object_size / clear_padding */
    gcc_jit_rvalue *shuf_args[3] = { v4f_rv, v4f_rv, v4i_rv };
    auto saved_vp = p_gcc_jit_context_new_rvalue_vector_perm;
    p_gcc_jit_context_new_rvalue_vector_perm = &mock_vec_perm;
    rout.source_corresp.name = "__builtin_shufflevector";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, shuf_args, &rv));
    shuf_args[2] = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, shuf_args, &rv));
    p_gcc_jit_context_new_rvalue_vector_perm = NULL;
    rout.source_corresp.name = "__builtin_shuffle";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, shuf_args, &rv));
    shuf_args[1] = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, shuf_args, &rv));
    shuf_args[0] = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, shuf_args, &rv));
    shuf_args[0] = v4f_rv; shuf_args[1] = v4f_rv; shuf_args[2] = v4i_rv;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, shuf_args, &rv));
    p_gcc_jit_context_new_rvalue_vector_perm = saved_vp;

    rout.source_corresp.name = "__builtin_convertvector";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_arg1, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4f_rv, &rv));
    rout.type = &rtp; rtp.variant.routine.return_type = &t_i32; /* non-vector return_type */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4f_rv, &rv));
    vec_tp.variant.vector.element_type = &t_i32;
    rtp.variant.routine.return_type = &vec_tp;
    auto saved_bc2 = p_gcc_jit_context_new_bitcast;
    p_gcc_jit_context_new_bitcast = &mock_bitcast;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4f_rv, &rv));
    p_gcc_jit_context_new_bitcast = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4f_rv, &rv));
    p_gcc_jit_context_new_bitcast = saved_bc2;

    rout.type = NULL;
    rout.source_corresp.name = "__builtin_is_constant_evaluated";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    rout.source_corresp.name = "__builtin_constant_p";
    rout.compiler_generated = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
    rout.compiler_generated = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
    rout.compiler_generated = FALSE;

    rout.source_corresp.name = "__builtin_va_arg_pack";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    rout.source_corresp.name = "__builtin_va_arg_pack_len";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, bit_args2, &rv));

    rout.source_corresp.name = "__builtin_dynamic_object_size";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));
    rout.type = &rtp; p0.type = &t_ptr; rts.param_type_list = &p0;
    rout.source_corresp.name = "__builtin_object_size";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));
    rout.source_corresp.name = "__builtin_clear_padding";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_arg1, &rv));
    rout.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));

    /* Fortified __builtin___*_chk: vsnprintf_chk (6, 4, 2 args), snprintf_chk (4, 5, 6, 2 args), >=64 base_len, empty base_len, 0 args, null arg */
    gcc_jit_rvalue *fmt_s = gcc_jit_context_new_string_literal(ctx, "%s");
    gcc_jit_rvalue *vsn_a[6] = { ptr_rv, u64_rv, i32_rv, u64_rv, fmt_s, ptr_rv };
    rout.source_corresp.name = "__builtin___vsnprintf_chk";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 6, vsn_a, &rv));
    vsn_a[5] = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 6, vsn_a, &rv));
    vsn_a[3] = fmt_s;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 4, vsn_a, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vsn_a, &rv));
    vsn_a[3] = u64_rv; vsn_a[5] = i32_rv;
    rout.source_corresp.name = "__builtin___snprintf_chk";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 5, vsn_a, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 6, vsn_a, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vsn_a, &rv));
    rout.source_corresp.name = "__builtin___memcpy_chk";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    gcc_jit_rvalue *null_chk[2] = { NULL, ptr_rv };
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, null_chk, &rv));
    rout.source_corresp.name = "__builtin____chk";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));
    rout.source_corresp.name = "__builtin___aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa_chk";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));
    rout.source_corresp.name = "__builtin___no_check_suffix";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));

    /* __builtin_classify_type across every tk_* kind */
    rout.source_corresp.name = "__builtin_classify_type";
    rout.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    rout.type = &rtp;
    p0.type = NULL; rts.param_type_list = &p0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    {
        a_type ct = {}, cref = {};
        cref.kind = tk_typeref; cref.variant.typeref.type = NULL;
        p0.type = &cref;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
        cref.variant.typeref.type = &ct;
        a_type_kind ckinds[] = {
            tk_void, tk_ptr_to_member, tk_float, tk_complex, tk_routine,
            tk_struct, tk_class, tk_union, tk_array, tk_vector, tk_nullptr
        };
        for (size_t k = 0; k < sizeof(ckinds) / sizeof(ckinds[0]); ++k) {
            ct = {}; ct.kind = ckinds[k]; ct.size = 4;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
        }
        ct = {}; ct.kind = tk_integer; ct.size = 1; ct.variant.integer.bool_type = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
        ct = {}; ct.kind = tk_integer; ct.size = 4; ct.variant.integer.enum_type = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
        ct = {}; ct.kind = tk_integer; ct.size = 1;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
        ct = {}; ct.kind = tk_integer; ct.size = 4;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
        ct = {}; ct.kind = tk_pointer; ct.size = 8; ct.variant.pointer.is_reference = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
        ct.variant.pointer.is_reference = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    }
    rout.type = NULL;

    /* __builtin_types_compatible_p, __builtin_offsetof, __builtin_choose_expr, __builtin_bit_cast, SEH cached & uncached */
    rout.source_corresp.name = "__builtin_types_compatible_p";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    gcc_jit_rvalue *tc_same[2] = { i32_rv, i32_rv };
    gcc_jit_rvalue *tc_diff[2] = { i32_rv, u64_rv };
    gcc_jit_rvalue *tc_null0[2] = { NULL, i32_rv };
    gcc_jit_rvalue *tc_null1[2] = { i32_rv, NULL };
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, tc_same, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, tc_diff, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, tc_null0, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, tc_null1, &rv));
    p0.type = &tref_e; p0.next = &p1; p1.type = &tref_e; tref_e.variant.typeref.type = &t_i32;
    rts.param_type_list = &p0; rout.type = &rtp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    p1.type = &t_u64;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    a_type f32_tp = {}; f32_tp.kind = tk_float; f32_tp.size = 4;
    p1.type = &f32_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    tref_e.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    a_type tref_e2 = {}; tref_e2.kind = tk_typeref; tref_e2.variant.typeref.type = NULL;
    tref_e.variant.typeref.type = &t_i32; p1.type = &tref_e2;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    p1.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    p0.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    p0.next = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    rout.type = NULL;

    rout.source_corresp.name = "__builtin_offsetof";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_arg1, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));

    gcc_jit_rvalue *ce_a[3] = { i32_rv, i32_rv, u64_rv };
    rout.source_corresp.name = "__builtin_choose_expr";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, ce_a, &rv));
    ce_a[1] = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, ce_a, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, ce_a, &rv));

    rout.source_corresp.name = "__builtin_bit_cast";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_arg1, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
    rout.type = &rtp; rtp.variant.routine.return_type = &f32_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
    rout.type = NULL;

    /* SEH builtins (uncached & cached, all 9 alias names) */
    const char *seh_names[] = {
        "_abnormal_termination", "__builtin_eh_AbnormalTermination", "AbnormalTermination",
        "_exception_code", "GetExceptionCode", "__exception_code",
        "_exception_info", "GetExceptionInformation", "__exception_info"
    };
    for (size_t i = 0; i < 9; ++i) {
        rout.source_corresp.name = seh_names[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    }
    gcc_jit_lvalue *seh_abn = gcc_jit_function_new_local(fn, NULL, i32_jt, "seh_abn");
    gcc_jit_lvalue *seh_cod = gcc_jit_function_new_local(fn, NULL, u64_jt, "seh_cod");
    gcc_jit_lvalue *seh_inf = gcc_jit_function_new_local(fn, NULL, vptr_jt, "seh_inf");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_abnormal_termination", seh_abn));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_exception_code", seh_cod));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_exception_info", seh_inf));
    rout.source_corresp.name = "_abnormal_termination";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    rout.source_corresp.name = "_exception_code";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    rout.source_corresp.name = "_exception_info";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));

    /* Fallback math call: coercion when param type differs, NULL arg, 0 args, variadic pcount < safe_args, null lower_type */
    {
        gcc_jit_param *mp = gcc_jit_context_new_param(ctx, NULL, u64_jt, "mp0");
        gcc_jit_function *mfn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, i32_jt, "my_coerced_math", 1, &mp, 0);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)".imp:my_coerced_math", mfn));
        rout.source_corresp.name = "my_coerced_math";
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &u64_rv, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_arg1, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));

        gcc_jit_function *vfn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, i32_jt, "my_var_math", 0, NULL, 1);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)".imp:my_var_math", vfn));
        rout.source_corresp.name = "my_var_math";
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));

        rout.type = &rtp; rtp.variant.routine.return_type = &t_i32;
        g_lower_type_returns_null = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
        g_lower_type_returns_null = FALSE;
        rout.type = NULL;
    }

    /* 5. Coroutine builtins */
    const char *coro_names[] = {
        "__builtin_coro_resume", "__builtin_coro_destroy", "__builtin_coro_done",
        "__builtin_coro_frame", "__builtin_coro_free", "__builtin_coro_noop", "__builtin_coro_alloc"
    };
    for (size_t i = 0; i < sizeof(coro_names) / sizeof(coro_names[0]); ++i) {
        rout.source_corresp.name = coro_names[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));
    }
    rout.source_corresp.name = "__builtin_coro_noop";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_arg1, &rv));

    gcc_jit_rvalue *prom_args[3] = { ptr_rv, i32_rv, i32_rv };
    rout.source_corresp.name = "__builtin_coro_promise";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, prom_args, &rv));
    {
        gcc_jit_block *saved_b = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_b));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, prom_args, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_b));
    }
    prom_args[2] = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, prom_args, &rv));
    prom_args[2] = i32_rv;

    /* __builtin_coro_size: NULL frame, scalar int frame (fsz > 0), struct frame with mock sizeof */
    auto saved_sz = p_gcc_jit_context_new_sizeof;
    p_gcc_jit_context_new_sizeof = &mock_new_sizeof;
    rout.source_corresp.name = "__builtin_coro_size";
    g_mock_coro_frame_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    g_mock_coro_frame_type = u64_jt;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    gcc_jit_field *cf = gcc_jit_context_new_field(ctx, NULL, vptr_jt, "r");
    g_mock_coro_frame_type = gcc_jit_struct_as_type(gcc_jit_context_new_struct_type(ctx, NULL, "__coro_sz_st", 1, &cf));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    g_mock_coro_frame_type = NULL;
    p_gcc_jit_context_new_sizeof = saved_sz;

    /* 6. SIMD & Sanitizer builtins */
    /* Native vector binary op fast-path on _mm_add_ps (pre-seed cached v4f_jt on &vec_tp!) */
    a_type vec_f32_tp = {};
    vec_f32_tp.kind = tk_vector; vec_f32_tp.size = 16; vec_f32_tp.variant.vector.element_type = &f32_tp;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, &vec_f32_tp, v4f_jt));
    rout.type = &rtp;
    rtp.variant.routine.return_type = &vec_f32_tp;
    gcc_jit_rvalue *v4f_pair[2] = { v4f_rv, v4f_rv };
    gcc_jit_rvalue *v4f_mism0[2] = { v4i_rv, v4f_rv };
    gcc_jit_rvalue *v4f_mism1[2] = { v4f_rv, v4i_rv };
    gcc_jit_rvalue *v4f_n0[2] = { NULL, v4f_rv };
    gcc_jit_rvalue *v4f_n1[2] = { v4f_rv, NULL };
    rout.source_corresp.name = "_mm_add_ps";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, v4f_pair, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, v4f_mism0, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, v4f_mism1, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, v4f_n0, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, v4f_n1, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4f_rv, &rv));
    rtp.variant.routine.return_type = &t_i32;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, v4f_pair, &rv));
    rtp.variant.routine.return_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, v4f_pair, &rv));
    rtp.variant.routine.return_type = &vec_f32_tp;

    /* Mapped intrinsic with mapped_bin_op < 0 (`_mm_loadu_ps`) */
    rout.source_corresp.name = "_mm_loadu_ps";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));

    /* lower_type returning NULL in lower_simd_builtin */
    g_lower_type_returns_null = TRUE;
    rout.source_corresp.name = "_mm_add_ps";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, v4f_pair, &rv));
    g_lower_type_returns_null = FALSE;
    rout.type = NULL;

    /* Unmapped _mm_*, _mm256_*, _mm512_*, NEON vadd*, sv*, handled simd (`vdup_n_s32`), simd_err (`_mm_custom_zero` 0 args), 0-arg sv_custom_zero, NULL arg */
    const char *simd_names[] = {
        "_mm_custom_xyz", "_mm256_custom_xyz", "_mm512_custom_xyz", "vadd_custom_xyz", "sv_custom_xyz"
    };
    for (size_t i = 0; i < sizeof(simd_names) / sizeof(simd_names[0]); ++i) {
        rout.source_corresp.name = simd_names[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &v4f_rv, &rv));
    }
    rout.source_corresp.name = "_mm_custom_zero";
    if (gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv) != GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    rout.source_corresp.name = "sv_custom_zero";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));
    gcc_jit_rvalue *null_simd[1] = { NULL };
    rout.source_corresp.name = "sv_custom_xyz";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_simd, &rv));

    /* Handled by gcc_gen_be_try_lower_simd_builtin (`vdup_n_s32`) */
    rout.type = &rtp; vec_tp.variant.vector.element_type = &t_i32; rtp.variant.routine.return_type = &vec_tp;
    rout.source_corresp.name = "vdup_n_s32";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &i32_rv, &rv));
    rout.type = NULL;

    /* Sanitizer hooks */
    rout.source_corresp.name = "__asan_report_load4";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &ptr_rv, &rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, null_simd, &rv));
    rout.source_corresp.name = "__ubsan_handle_add_overflow";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rv));

    /* 7. Deterministic `cache_clear_all()` OOM sweeps across all categories! */
    struct oom_case_t {
        /** @brief Builtin name to sweep. */
        const char *bname;
        /** @brief Argument count. */
        int argc;
        /** @brief Argument vector. */
        gcc_jit_rvalue **argv;
        /** @brief Optional routine type. */
        a_type_ptr rtype;
    };
    a_param_type oom_p0 = {}, oom_p1 = {};
    oom_p0.type = &tref_p; oom_p0.next = &oom_p1;
    oom_p1.type = &tref_p; oom_p1.next = NULL;
    tref_p.variant.typeref.type = &t_ptr;
    t_ptr.variant.pointer.type = &tref_e;
    tref_e.variant.typeref.type = &t_u128;
    rts.param_type_list = &oom_p0;
    rtp.variant.routine.return_type = &t_u128;

    a_type rtp_vec = {};
    a_param_type vp_p0 = {};
    vp_p0.type = &tref_v; tref_v.variant.typeref.type = &vec_tp; vec_tp.variant.vector.element_type = &t_i32;
    a_routine_type_supplement rts_vec = {};
    rts_vec.param_type_list = &vp_p0;
    rtp_vec.kind = tk_routine; rtp_vec.variant.routine.return_type = &vec_tp; rtp_vec.variant.routine.extra_info = &rts_vec;

    oom_case_t oom_cases[] = {
        { "__atomic_is_lock_free", 2, at_args8, &rtp },
        { "__atomic_load", 3, gen_at_args, &rtp },
        { "__atomic_load_n", 1, &ptr_rv, &rtp },         /* 16B atomic params alloc */
        { "__atomic_load_n", 1, &ptr_rv, NULL },         /* 0-param r_sz path */
        { "__atomic_store_n", 2, at_args8, NULL },       /* 4B atomic a1_sz + get_builtin */
        { "__c11_atomic_init", 2, c11_args, NULL },
        { "__c11_atomic_compare_exchange_strong", 5, c11_args, &rtp },
        { "__builtin_FILE", 0, NULL, &rtp },
        { "__builtin_bswap128", 1, &u128_rv, NULL },
        { "__builtin_stdc_bit_floor", 1, &i32_rv, NULL },
        { "__builtin_stdc_leading_zeros", 1, &i32_rv, NULL },
        { "__builtin_reduce_add", 1, &v4i_rv, &rtp_vec },
        { "__builtin_isnan", 1, &f64_rv, NULL },
        { "__builtin_launder", 1, &ptr_rv, NULL },
        { "__builtin_unreachable", 0, NULL, NULL },
        { "__builtin_addcll", 4, fpc_args, NULL },
        { "__builtin_fpclassify", 6, fpc_args, NULL },
        { "__builtin_frame_address", 1, &i32_rv, NULL },
        { "__builtin_convertvector", 1, &v4f_rv, &rtp_vec },
        { "__builtin_object_size", 1, &ptr_rv, &rtp },
        { "__builtin_clear_padding", 1, &ptr_rv, &rtp },
        { "__builtin_classify_type", 0, NULL, &rtp },
        { "__builtin_types_compatible_p", 0, NULL, &rtp },
        { "__builtin_bit_cast", 1, &u128_rv, &rtp },
        { "__builtin___vsnprintf_chk", 6, vsn_a, NULL },
        { "__builtin___snprintf_chk", 6, vsn_a, NULL },
        { "__builtin___memcpy_chk", 4, vsn_a, NULL },
        { "_abnormal_termination", 0, NULL, NULL },
        { "_exception_code", 0, NULL, NULL },
        { "_exception_info", 0, NULL, NULL },
        { "custom_math_oom", 2, tc_diff, &rtp },
        { "__builtin_coro_promise", 3, prom_args, NULL },
        { "__builtin_coro_noop", 0, NULL, NULL },
        { "__builtin_coro_size", 0, NULL, NULL },
        { "_mm_custom_oom", 2, v4f_pair, &rtp_vec },
        { "sv_custom_oom", 2, v4f_pair, &rtp_vec },
        { "vdup_n_s32", 1, &i32_rv, &rtp_vec },
        { "__asan_report_store8", 1, &ptr_rv, NULL }
    };
    vsn_a[5] = ptr_rv;
    for (size_t ci = 0; ci < sizeof(oom_cases) / sizeof(oom_cases[0]); ++ci) {
        for (int b = 0; b < 60; ++b) {
            gcc_jit_block *iter_blk = gcc_jit_function_new_block(fn, "bt_oom_blk");
            GCC_GEN_BE_CHECK(cache_clear_all());
            /* Pre-register custom_math_oom with 2 params so coerced_args malloc is also swept! */
            if (std::strcmp(oom_cases[ci].bname, "custom_math_oom") == 0) {
                gcc_jit_param *ps2[2] = {
                    gcc_jit_context_new_param(ctx, NULL, u64_jt, "p0"),
                    gcc_jit_context_new_param(ctx, NULL, u64_jt, "p1")
                };
                gcc_jit_function *cmf = gcc_jit_context_new_function(
                    ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, i32_jt, "custom_math_oom", 2, ps2, 0);
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)".imp:custom_math_oom", cmf));
            }
            /* Also test custom_unregistered_oom without pre-registration so get_builtin params alloc is swept! */
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(iter_blk));
            if (std::strcmp(oom_cases[ci].bname, "__builtin_coro_size") == 0) {
                g_mock_coro_frame_type = u64_jt;
            }
            rout.source_corresp.name = oom_cases[ci].bname;
            rout.type = oom_cases[ci].rtype;
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t e = gcc_gen_be_lower_builtin_call(&rout, oom_cases[ci].argc, oom_cases[ci].argv, &rv);
            g_gcc_be_oom_countdown = -1;
            g_mock_coro_frame_type = NULL;
            if (e == GCC_GEN_BE_SUCCESS) break;
        }
    }
    for (int b = 0; b < 40; ++b) {
        GCC_GEN_BE_CHECK(cache_clear_all());
        rout.source_corresp.name = "unregistered_libc_oom";
        rout.type = NULL;
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_builtin_call(&rout, 2, tc_diff, &rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* Stage-2 builtin coverage: __builtin_signbit (float/double/long double), struct->int atomic arg coercion,
       struct-returning atomic with cblock == NULL, and __builtin_operator_new with NULL arg */
    {
        gcc_jit_type *flt_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
        gcc_jit_type *dbl_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
        gcc_jit_type *ldbl_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
        gcc_jit_rvalue *flt_v = gcc_jit_context_new_rvalue_from_double(ctx, flt_t, -1.0);
        gcc_jit_rvalue *dbl_v = gcc_jit_context_new_rvalue_from_double(ctx, dbl_t, -1.0);
        gcc_jit_rvalue *ldbl_v = gcc_jit_context_new_rvalue_from_double(ctx, ldbl_t, -1.0);

        a_type int_ret_tp = {}; int_ret_tp.kind = tk_integer; int_ret_tp.size = 4;
        a_type sign_rty = {}; sign_rty.kind = tk_routine; sign_rty.variant.routine.return_type = &int_ret_tp;
        a_routine sign_r = {}; sign_r.source_corresp.name = "__builtin_signbit"; sign_r.type = &sign_rty;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&sign_r, 1, &flt_v, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&sign_r, 1, &dbl_v, &rv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&sign_r, 1, &ldbl_v, &rv));

        /* Struct rvalue argument coerced into integer parameter in atomic helper (486:68 True, cblock != NULL & cblock == NULL)
           + struct return type when cblock == NULL (508:13 False) */
        gcc_jit_field *sf0 = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "v");
        gcc_jit_struct *st4_s = gcc_jit_context_new_struct_type(ctx, NULL, "AtomSt4", 1, &sf0);
        gcc_jit_type *st4_jt = gcc_jit_struct_as_type(st4_s);
        gcc_jit_rvalue *st4_rv = gcc_jit_context_new_struct_constructor(ctx, NULL, st4_jt, 0, NULL, NULL);
        gcc_jit_rvalue *ptr_st4 = gcc_jit_context_null(ctx, gcc_jit_type_get_pointer(st4_jt));
        gcc_jit_rvalue *ord_rv = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);

        a_field edg_sf = {}; edg_sf.type = &int_ret_tp;
        a_type edg_st4 = {}; edg_st4.kind = tk_struct; edg_st4.size = 4; edg_st4.variant.class_struct_union.field_list = &edg_sf;
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, &edg_st4, st4_jt));
        a_type atom_xchg_rty = {}; atom_xchg_rty.kind = tk_routine; atom_xchg_rty.variant.routine.return_type = &edg_st4;
        a_routine atom_xchg_r = {}; atom_xchg_r.source_corresp.name = "__atomic_exchange_n"; atom_xchg_r.type = &atom_xchg_rty;
        gcc_jit_rvalue *ax_args[3] = {ptr_st4, st4_rv, ord_rv};

        gcc_jit_block *cblk_bi = gcc_jit_function_new_block(fn, "bi_atom_st_blk");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cblk_bi));
        for (int b = 0; b < 80; ++b) {
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t ae = gcc_gen_be_lower_builtin_call(&atom_xchg_r, 3, ax_args, &rv);
            g_gcc_be_oom_countdown = -1;
            if (ae == GCC_GEN_BE_SUCCESS) break;
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&atom_xchg_r, 3, ax_args, &rv));

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&atom_xchg_r, 3, ax_args, &rv));

        /* __builtin_operator_new with NULL f_args[0] (1424:21 False) */
        a_type vp_tp = {}; vp_tp.kind = tk_pointer; vp_tp.size = 8;
        a_type opnew_rty = {}; opnew_rty.kind = tk_routine; opnew_rty.variant.routine.return_type = &vp_tp;
        a_routine opnew_r = {}; opnew_r.source_corresp.name = "__builtin_operator_new"; opnew_r.type = &opnew_rty;
        gcc_jit_rvalue *null_farg[1] = {NULL};
        /* Note args[0] == NULL triggers f_args[0] == NULL! Wait: check if args == NULL or args[0] == NULL is allowed: pass 2 args where args[0] is size_t and args[1] is NULL (`__builtin_operator_delete` 2-arg)! */
        a_type void_tp_bi = {}; void_tp_bi.kind = tk_void;
        a_type opdel_rty = {}; opdel_rty.kind = tk_routine; opdel_rty.variant.routine.return_type = &void_tp_bi;
        a_routine opdel_r = {}; opdel_r.source_corresp.name = "__builtin_operator_delete"; opdel_r.type = &opdel_rty;
        gcc_jit_rvalue *del_null_args[2] = {NULL, ord_rv};
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&opdel_r, 2, del_null_args, &rv));
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes all builtin lowering unit tests.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t run_builtin_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_rvalue *rval = NULL;
    if (gcc_gen_be_lower_builtin_call(NULL, 0, NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    GCC_GEN_BE_CHECK(load_libgccjit_posix());
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "builtin_test_fn", 0, NULL, 0);

    GCC_GEN_BE_CHECK(test_helper_apis(ctx, fn));
    GCC_GEN_BE_CHECK(test_all_builtin_categories(ctx, fn));

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    gcc_jit_context_release(ctx);
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_builtin.cpp`.
 * @details Runs `run_builtin_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_builtin_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
