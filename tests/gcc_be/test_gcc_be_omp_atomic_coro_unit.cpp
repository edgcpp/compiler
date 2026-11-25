/**
 * @file test_gcc_be_omp_atomic_coro_unit.cpp
 * @brief Unit & JIT-execution tests for `_Decimal128`, `_Sat long _Accum`, atomics, SIMD, coroutines, and OpenMP.
 * @details Exercises both success and `gcc_gen_be_error_t` error-return paths across
 * `gcc_gen_be_type.c` (`_Decimal128` bit-127 negation/`fabs`, 64-bit `__bid_fix*di`, 64-bit `_Sat long _Accum`,
 * `_BitInt(256)` struct lowering & unsupported widths), `gcc_gen_be_atomic.c`, `gcc_gen_be_simd.c`,
 * `gcc_gen_be_coro.c`, and `gcc_gen_be_omp.c`.
 * @ingroup gcc_gen_be_tests
 */

#include "gcc_gen_be_type.h"
#include "gcc_gen_be_atomic.h"
#include "gcc_gen_be_simd.h"
#include "gcc_gen_be_coro.h"
#include "gcc_gen_be_omp.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_lib_loader.h"
#include "test_common.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <libgccjit.h>

extern "C" {
/**
 * @brief Diagnostic reporter stub returning `err` directly in unit tests.
 * @param[in] err Error status code.
 * @param[in] msg Diagnostic text.
 * @return `err`.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char *msg) GCC_GEN_BE_NOEXCEPT {
    if (msg != nullptr && msg[0] == '\0') return err;
    return err;
}
}

USING_NAMESPACE_EDG

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
/** @brief Primary source file name stub. */
a_const_char *primary_source_file_name = "test_omp_atomic_coro.cpp";
/** @brief Active C++ dialect stub. */
a_C_dialect C_dialect = C_dialect_cplusplus;
/** @brief GCC compatibility mode stub. */
a_boolean gcc_mode = TRUE;
/** @brief Innermost function scope stub. */
a_scope_ptr innermost_function_scope = NULL;
/** @brief Configurable mock source file path for `conv_seq_to_file_and_line`. */
static const char *g_mock_seq_file_path = NULL;
/** @brief Optional override for `file_name` when testing empty/null `full_name` fallback. */
static const char *g_mock_short_fn = NULL;
/** @brief When `TRUE`, `gcc_gen_be_lower_statement` terminates the current block and sets it to `NULL`. */
static a_boolean g_terminate_in_lower_stmt = FALSE;

/**
 * @brief Stub mapping sequence numbers to configurable source file and line in unit tests.
 * @details Returns `g_mock_seq_file_path` (or `g_mock_short_fn` for `file_name`) and casts `seq` directly to `*line_number`.
 * @param[in] seq Source sequence number.
 * @param[out] file_name Short file name output pointer.
 * @param[out] full_name Full path output pointer.
 * @param[out] line_number Physical line number output pointer.
 * @param[out] at_end_of_source End-of-source flag output pointer.
 * @return NULL source file pointer.
 */
a_source_file_ptr conv_seq_to_file_and_line(
    a_seq_number seq,
    a_const_char **file_name,
    a_const_char **full_name,
    a_line_number *line_number,
    a_boolean *at_end_of_source) {
    if (file_name) *file_name = g_mock_short_fn ? g_mock_short_fn : g_mock_seq_file_path;
    if (full_name) *full_name = g_mock_seq_file_path;
    if (line_number) *line_number = (a_line_number)seq;
    if (at_end_of_source) *at_end_of_source = FALSE;
    return NULL;
}

/**
 * @brief Stub for getting source location from type in unit tests.
 * @details Sets `*out_loc` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] tp Frontend type pointer.
 * @param[out] out_loc Output location pointer.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_type(a_type_ptr tp, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = tp ? nullptr : nullptr;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for emitting type debug info in unit tests.
 * @param[in] tp Frontend type pointer.
 * @param[in] type Lowered JIT type pointer.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t gcc_gen_be_emit_type_debug_info(a_type_ptr tp, struct gcc_jit_type *type) GCC_GEN_BE_NOEXCEPT {
    return (tp || type) ? GCC_GEN_BE_SUCCESS : GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for lowering constant rvalues in OpenMP/atomic/coroutine unit tests.
 * @details Steps OOM budget and produces a constant `1` rvalue of `expected_type` (or `int`).
 * @param[in] con Frontend constant pointer.
 * @param[in] expected_type Expected JIT type (may be NULL).
 * @param[out] out_rval Pointer receiving the lowered constant rvalue.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(
    a_constant_ptr con,
    struct gcc_jit_type *expected_type,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *ty = expected_type
        ? expected_type
        : gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    long val = con ? 1L : 1L;
    *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, ty, val);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for checking sret return in unit tests.
 * @param[in] type Frontend type pointer.
 * @param[out] out_is_sret Output sret flag.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT {
    if (out_is_sret) *out_is_sret = type ? FALSE : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for checking invisible-reference parameter passing in unit tests.
 * @param[in] type Frontend type pointer.
 * @param[out] out_is_ref Output invisible-ref flag.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    if (out_is_ref) *out_is_ref = type ? FALSE : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for coerced ABI type lookup in unit tests.
 * @param[in] type Frontend type pointer.
 * @param[out] out_coerced_type Output coerced JIT type.
 * @param[out] out_needs_coercion Output coercion flag.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT {
    if (out_coerced_type) *out_coerced_type = type ? nullptr : nullptr;
    if (out_needs_coercion) *out_needs_coercion = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for lowering function declarations referenced by function-pointer types in unit tests.
 * @param[in] rout Frontend routine pointer.
 * @param[out] out_func Output JIT function.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    if (out_func) *out_func = rout ? nullptr : nullptr;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub computing `tp->size` in unit tests.
 * @param[in] tp Frontend type pointer.
 * @return `tp->size` or `0`.
 */
a_targ_size_t f_size_of_type(a_type_ptr tp) {
    return tp ? tp->size : 0;
}

/**
 * @brief Stub checking nonreal types in unit tests.
 * @param[in] tp Frontend type pointer.
 * @return `FALSE`.
 */
a_boolean type_is_nonreal(a_type_ptr tp) {
    return tp ? FALSE : FALSE;
}

/**
 * @brief Stub lowering expression rvalues in OpenMP/atomic unit tests.
 * @param[in] expr Expression node.
 * @param[out] out_rval Output rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    if (expr->type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &t));
    }
    int cval = (expr->kind == enk_constant) ? 8 : 1;
    *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, t, cval);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub lowering expression lvalues in OpenMP/atomic unit tests.
 * @param[in] expr Expression node.
 * @param[out] out_lval Output lvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (expr->kind == enk_variable && expr->variant.variable.ptr) {
        void *lv = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, expr->variant.variable.ptr, &lv));
        *out_lval = static_cast<gcc_jit_lvalue *>(lv);
        return GCC_GEN_BE_SUCCESS;
    }
    return GCC_GEN_BE_ERROR_UNHANDLED_EXPR;
}

/**
 * @brief Stub lowering statements inside OpenMP unit tests while dispatching nested OpenMP pragmas.
 * @param[in] stmt Statement node.
 * @param[in] func Enclosing `gcc_jit_function`.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_terminate_in_lower_stmt) {
        gcc_jit_block *cb = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cb));
        if (cb) {
            gcc_jit_block_end_with_void_return(cb, NULL);
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        }
        return GCC_GEN_BE_SUCCESS;
    }
    while (stmt) {
        a_boolean handled = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(stmt, func, &handled));
        if (!handled && stmt->kind == stmk_block && stmt->variant.block.statements) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(stmt->variant.block.statements, func));
        }
        stmt = stmt->next;
    }
    return GCC_GEN_BE_SUCCESS;
}
END_EDG_NAMESPACE

namespace {

/**
 * @brief JIT-compiles and natively executes `_Decimal128` bit-127 negation/`fabs` and 64-bit `_Sat long _Accum`.
 * @details Verifies `_Decimal128` unary `-` flips bit 127 (high 64-bit word bit 63, NOT low word bit 63!)
 * and 64-bit `_Sat long _Accum` (`Q32.31`) multiplication and saturating addition clamp at `INT64_MAX`/`INT64_MIN`.
 * @param[out] out_ok Pointer receiving `1` on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t test_decimal128_and_sat_accum_jit(int *out_ok) noexcept {
    *out_ok = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_init());
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

    /* Error path checks on decimal & fixed-point helpers */
    gcc_jit_type *bad_df = NULL;
    ASSERT_EQ(gcc_gen_be_lower_decimal_float_type(12, &bad_df), GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED);
    ASSERT_EQ(gcc_gen_be_lower_decimal_float_type(16, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);

    gcc_jit_type *d128_t = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(16, &d128_t));
    ASSERT_TRUE(d128_t != NULL);

    /* 64-bit decimal->int conversions (`__bid_fixtddi` / `__bid_fixunstddi`) */
    gcc_jit_rvalue *d128_zero = gcc_jit_context_new_rvalue_from_int(ctx, d128_t, 0);
    gcc_jit_rvalue *fix64_s = NULL;
    gcc_jit_rvalue *fix64_u = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_int_sized(16, 8, FALSE, d128_zero, &fix64_s));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_int_sized(16, 8, TRUE, d128_zero, &fix64_u));
    ASSERT_TRUE(fix64_s != NULL && fix64_u != NULL);

    /* Build native JIT function: `unsigned __int128 jit_neg_d128(unsigned __int128 x)` */
    gcc_jit_param *p_d128 = gcc_jit_context_new_param(ctx, NULL, d128_t, "x");
    gcc_jit_function *fn_neg = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, d128_t, "jit_neg_d128", 1, &p_d128, 0);
    gcc_jit_block *b_neg = gcc_jit_function_new_block(fn_neg, "entry");
    gcc_jit_rvalue *neg_rv = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_unary_op(16, '-', gcc_jit_param_as_rvalue(p_d128), &neg_rv));
    gcc_jit_block_end_with_return(b_neg, NULL, neg_rv);

    /* Build native JIT function: `int64_t jit_sat_accum_add(int64_t a, int64_t b)` */
    gcc_jit_type *i64_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT64_T);
    gcc_jit_param *pa[2] = {
        gcc_jit_context_new_param(ctx, NULL, i64_t, "a"),
        gcc_jit_context_new_param(ctx, NULL, i64_t, "b")
    };
    gcc_jit_function *fn_sat = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, i64_t, "jit_sat_accum_add", 2, pa, 0);
    gcc_jit_block *b_sat = gcc_jit_function_new_block(fn_sat, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(b_sat));
    gcc_jit_rvalue *sat_rv = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_binary_op(
        8, 31, FALSE, TRUE, '+', gcc_jit_param_as_rvalue(pa[0]), gcc_jit_param_as_rvalue(pa[1]), &sat_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&b_sat));
    gcc_jit_block_end_with_return(b_sat, NULL, sat_rv);

    /* Build native JIT function: `int64_t jit_sat_accum_mul(int64_t a, int64_t b)` (128-bit intermediate!) */
    gcc_jit_param *pm[2] = {
        gcc_jit_context_new_param(ctx, NULL, i64_t, "a"),
        gcc_jit_context_new_param(ctx, NULL, i64_t, "b")
    };
    gcc_jit_function *fn_mul = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, i64_t, "jit_sat_accum_mul", 2, pm, 0);
    gcc_jit_block *b_mul = gcc_jit_function_new_block(fn_mul, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(b_mul));
    gcc_jit_rvalue *mul_rv = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_binary_op(
        8, 31, FALSE, TRUE, '*', gcc_jit_param_as_rvalue(pm[0]), gcc_jit_param_as_rvalue(pm[1]), &mul_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&b_mul));
    gcc_jit_block_end_with_return(b_mul, NULL, mul_rv);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));

    const char *so_path = "/tmp/edg_gcc_be_dec128_sat.so";
    std::remove(so_path);
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_command_line_option
    gcc_jit_context_add_command_line_option(ctx, "-fPIC");
#endif
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
    gcc_jit_context_add_driver_option(ctx, "-fno-use-linker-plugin");
#if defined(__APPLE__)
    gcc_jit_context_add_driver_option(ctx, "-nodefaultlibs");
    gcc_jit_context_add_driver_option(ctx, "-lSystem");
#endif
#endif
    gcc_jit_context_compile_to_file(ctx, GCC_JIT_OUTPUT_KIND_DYNAMIC_LIBRARY, so_path);
    ASSERT_TRUE(gcc_jit_context_get_first_error(ctx) == NULL);
    void *dl_h = dlopen(so_path, RTLD_NOW);
    ASSERT_TRUE(dl_h != NULL);

    typedef unsigned __int128 (*neg_fn_t)(unsigned __int128);
    typedef int64_t (*sat_fn_t)(int64_t, int64_t);
    neg_fn_t neg_fn = reinterpret_cast<neg_fn_t>(dlsym(dl_h, "jit_neg_d128"));
    sat_fn_t sat_add = reinterpret_cast<sat_fn_t>(dlsym(dl_h, "jit_sat_accum_add"));
    sat_fn_t sat_mul = reinterpret_cast<sat_fn_t>(dlsym(dl_h, "jit_sat_accum_mul"));
    ASSERT_TRUE(neg_fn != NULL && sat_add != NULL && sat_mul != NULL);

    /* Verify _Decimal128 `-` flips bit 127 and leaves low 64-bit word (including bit 63!) untouched! */
    unsigned __int128 in_d128 = (static_cast<unsigned __int128>(0x3040000000000000ULL) << 64) | 0x8000000000000055ULL;
    unsigned __int128 out_d128 = neg_fn(in_d128);
    uint64_t out_hi = static_cast<uint64_t>(out_d128 >> 64);
    uint64_t out_lo = static_cast<uint64_t>(out_d128);
    ASSERT_EQ(out_hi, 0xB040000000000000ULL);
    ASSERT_EQ(out_lo, 0x8000000000000055ULL);

    /* Verify 64-bit _Sat long _Accum saturation & 128-bit Q32.31 multiplication */
    ASSERT_EQ(sat_add(0x6000000000000000LL, 0x5000000000000000LL), INT64_MAX);
    ASSERT_EQ(sat_add(-0x6000000000000000LL, -0x5000000000000000LL), INT64_MIN);
    /* 3.0 * 2.5 = 7.5 in Q32.31 (1.0 == 1LL << 31) */
    int64_t q_three = 3LL << 31;
    int64_t q_two_half = (2LL << 31) + (1LL << 30);
    ASSERT_EQ(sat_mul(q_three, q_two_half), (7LL << 31) + (1LL << 30));

    dlclose(dl_h);
    std::remove(so_path);
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exhaustively exercises `src/gcc_gen_be_coro.c` across all AST shapes and OOM steps.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t test_exhaustive_coro_layout(gcc_jit_context *ctx) noexcept {
    unsigned long fbytes = 0;
    unsigned long falign = 0;
    ASSERT_EQ(gcc_gen_be_compute_coro_frame_layout(NULL, NULL, NULL, NULL, &falign), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_compute_coro_frame_layout(NULL, NULL, NULL, &fbytes, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);

    /* Types: typeref -> null, zero-size/align, and over-aligned (>4096) */
    a_type tref_null = {};
    tref_null.kind = tk_typeref;
    tref_null.variant.typeref.type = NULL;

    a_type zero_tp = {};
    zero_tp.kind = tk_integer;
    zero_tp.size = 0;
    zero_tp.alignment = 0;

    a_type huge_align_tp = {};
    huge_align_tp.kind = tk_struct;
    huge_align_tp.size = 13;
    huge_align_tp.alignment = 8192;

    a_type tref_huge = {};
    tref_huge.kind = tk_typeref;
    tref_huge.variant.typeref.type = &huge_align_tp;

    /* Promise & parameter_copies */
    a_variable prom_v = {};
    prom_v.type = &tref_null;
    a_variable pcopy_null_tp = {};
    pcopy_null_tp.type = NULL;
    a_variable pcopy_zero = {};
    pcopy_zero.type = &zero_tp;
    pcopy_null_tp.next = &pcopy_zero;

    a_coroutine_descr descr = {};
    descr.promise = &prom_v;
    descr.parameter_copies = &pcopy_null_tp;

    /* Expressions: temp_init (null type, non-null with dik_expression, non-null with dik_zero, non-null null dinit), operation, await, yield, constant */
    a_dynamic_init d_expr = {};
    d_expr.kind = dik_expression;
    an_expr_node leaf_tmp = {};
    leaf_tmp.kind = enk_temp_init;
    leaf_tmp.type = &tref_huge;
    d_expr.variant.expression = &leaf_tmp;

    a_dynamic_init d_non_expr = {};
    d_non_expr.kind = dik_zero;

    an_expr_node e_tmp1 = {};
    e_tmp1.kind = enk_temp_init;
    e_tmp1.type = &zero_tp;
    e_tmp1.variant.init.dynamic_init = &d_expr;

    an_expr_node e_tmp2 = {};
    e_tmp2.kind = enk_temp_init;
    e_tmp2.type = &zero_tp;
    e_tmp2.variant.init.dynamic_init = &d_non_expr;

    an_expr_node e_tmp_notp = {};
    e_tmp_notp.kind = enk_temp_init;
    e_tmp_notp.type = NULL;

    an_expr_node e_op = {};
    e_op.kind = enk_operation;
    e_op.variant.operation.operands = &e_tmp1;

    an_expr_node e_await = {};
    e_await.kind = enk_await;
    e_await.variant.await_info.operand = &e_tmp2;
    e_await.variant.await_info.ready_resume_suspend = &e_tmp_notp;

    an_expr_node e_yield = {};
    e_yield.kind = enk_yield;
    e_yield.variant.await_info.operand = &leaf_tmp;
    e_yield.variant.await_info.ready_resume_suspend = NULL;

    an_expr_node e_const = {};
    e_const.kind = enk_constant;

    e_op.next = &e_await;
    e_await.next = &e_yield;
    e_yield.next = &e_const;

    /* Statements: block, if, if_consteval, if_not_consteval, while, end_test_while, for (with & without extra_info), switch, try_block (with & without try_block), init (expr, non-expr, null), empty */
    a_statement s_leaf = {};
    s_leaf.kind = stmk_empty;
    s_leaf.expr = &e_op;

    a_statement s_if1 = {}, s_if2 = {}, s_if3 = {};
    s_if1.kind = stmk_if;
    s_if1.variant.if_stmt.then_statement = &s_leaf;
    s_if2.kind = stmk_if_consteval;
    s_if3.kind = stmk_if_not_consteval;
    s_if1.next = &s_if2;
    s_if2.next = &s_if3;

    a_statement s_wh1 = {}, s_wh2 = {};
    s_wh1.kind = stmk_while;
    s_wh1.variant.loop_statement = &s_leaf;
    s_wh2.kind = stmk_end_test_while;
    s_if3.next = &s_wh1;
    s_wh1.next = &s_wh2;

    a_for_loop fl_extra = {};
    fl_extra.initialization = &s_leaf;
    fl_extra.increment = &leaf_tmp;
    a_statement s_for1 = {}, s_for2 = {};
    s_for1.kind = stmk_for;
    s_for1.variant.for_loop.extra_info = &fl_extra;
    s_for1.variant.for_loop.statement = &s_leaf;
    s_for2.kind = stmk_for;
    s_for2.variant.for_loop.extra_info = NULL;
    s_wh2.next = &s_for1;
    s_for1.next = &s_for2;

    a_statement s_sw = {};
    s_sw.kind = stmk_switch;
    s_sw.variant.switch_stmt.body_statement = &s_leaf;
    s_for2.next = &s_sw;

    a_handler h1 = {};
    h1.statement = &s_leaf;
    a_try_supplement tsup = {};
    tsup.statement = &s_leaf;
    tsup.handlers = &h1;
    a_statement s_try1 = {}, s_try2 = {};
    s_try1.kind = stmk_try_block;
    s_try1.variant.try_block = &tsup;
    s_try2.kind = stmk_try_block;
    s_try2.variant.try_block = NULL;
    s_sw.next = &s_try1;
    s_try1.next = &s_try2;

    a_statement s_init1 = {}, s_init2 = {}, s_init3 = {};
    s_init1.kind = stmk_init;
    s_init1.variant.dynamic_init = &d_expr;
    s_init2.kind = stmk_init;
    s_init2.variant.dynamic_init = &d_non_expr;
    s_init3.kind = stmk_init;
    s_init3.variant.dynamic_init = NULL;
    s_try2.next = &s_init1;
    s_init1.next = &s_init2;
    s_init2.next = &s_init3;

    a_statement s_blk = {};
    s_blk.kind = stmk_block;
    s_blk.variant.block.statements = &s_if1;

    /* Scopes & variables */
    a_variable nsv1 = {}, nsv2 = {};
    nsv1.type = NULL;
    nsv2.type = &zero_tp;
    nsv1.next = &nsv2;

    a_variable v_static = {}, v_extern = {}, v_notp = {}, v_auto = {};
    v_static.type = &zero_tp;
    v_static.storage_class = sc_static;
    v_extern.type = &zero_tp;
    v_extern.storage_class = sc_extern;
    v_notp.type = NULL;
    v_auto.type = &tref_huge;
    v_auto.storage_class = sc_unspecified;
    v_static.next = &v_extern;
    v_extern.next = &v_notp;
    v_notp.next = &v_auto;

    a_scope sub_scope = {};
    sub_scope.assoc_block = NULL;
    sub_scope.nonstatic_variables = &nsv2;
    sub_scope.variables = &nsv2; /* variables == nonstatic_variables branch */

    a_scope top_scope = {};
    top_scope.assoc_block = &s_blk;
    top_scope.nonstatic_variables = &nsv1;
    top_scope.variables = &v_static;
    top_scope.scopes = &sub_scope;

    /* Frame types: struct type, small int type, large unaligned array type, large aligned array type, and void type (jit_sz <= 0) */
    gcc_jit_struct *j_struct = gcc_jit_context_new_struct_type(ctx, NULL, "coro_ft_s", 0, NULL);
    gcc_jit_type *ft_struct = gcc_jit_struct_as_type(j_struct);
    gcc_jit_type *char_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
    gcc_jit_type *ft_small = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *ft_void = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *ft_big_odd = gcc_jit_context_new_array_type(ctx, NULL, char_t, 20003);
    gcc_jit_type *ft_big_aligned = gcc_jit_context_new_array_type(ctx, NULL, char_t, 24576);

    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(NULL, NULL, NULL, &fbytes, &falign));
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&descr, &top_scope, ft_struct, &fbytes, &falign));
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&descr, &top_scope, ft_small, &fbytes, &falign));
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&descr, &top_scope, ft_void, &fbytes, &falign));
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&descr, &top_scope, ft_big_odd, &fbytes, &falign));
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&descr, &top_scope, ft_big_aligned, &fbytes, &falign));

    /* Also test descr with null promise, promise with null type, null parameter_copies, and already-aligned offset */
    a_coroutine_descr d_empty = {};
    a_variable prom_notp = {};
    prom_notp.type = NULL;
    d_empty.promise = &prom_notp;
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&d_empty, NULL, NULL, &fbytes, &falign));
    d_empty.promise = NULL;
    a_type align16_tp = {};
    align16_tp.kind = tk_integer;
    align16_tp.size = 4; /* 16 + 8 + 4 + 1 + (pad 3) + 4 = 36 -> + 12 = 48 % 16 == 0! */
    align16_tp.alignment = 4;
    a_variable v12 = {};
    a_type tp12 = {};
    tp12.kind = tk_struct;
    tp12.size = 12;
    tp12.alignment = 4;
    v12.type = &tp12;
    a_scope sc_exact = {};
    sc_exact.nonstatic_variables = &v12;
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&d_empty, &sc_exact, NULL, &fbytes, &falign));

    /* OOM countdown sweep across entire coro layout call tree */
    for (int b = 0; b < 160; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_compute_coro_frame_layout(&descr, &top_scope, ft_big_odd, &fbytes, &falign);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Mock `gcc_jit_context_new_rvalue_from_vector` callback returning a zero vector rvalue.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] loc Optional location.
 * @param[in] vec_type Vector `gcc_jit_type`.
 * @param[in] num_elements Lane count.
 * @param[in] elements Lane array.
 * @return Zero vector rvalue of `vec_type`.
 */
gcc_jit_rvalue *mock_new_rvalue_from_vector(
    gcc_jit_context *ctx,
    gcc_jit_location *loc,
    gcc_jit_type *vec_type,
    size_t num_elements,
    gcc_jit_rvalue **elements) noexcept {
    if (!ctx || !vec_type || num_elements == 0 || !elements) return loc ? nullptr : nullptr;
    return gcc_jit_context_zero(ctx, vec_type);
}

/**
 * @brief Exhaustively exercises `src/gcc_gen_be_simd.c` across all vector builders, opcodes, and OOM steps.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] test_fn Active `gcc_jit_function`.
 * @param[in] blk Active `gcc_jit_block`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t test_exhaustive_simd_api(
    gcc_jit_context *ctx,
    gcc_jit_function *test_fn,
    gcc_jit_block *blk) noexcept {
    gcc_jit_type *flt_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *v4f_t = gcc_jit_type_get_vector(flt_t, 4);
    gcc_jit_type *v4i_t = gcc_jit_type_get_vector(int_t, 4);
    gcc_jit_rvalue *f2 = gcc_jit_context_new_rvalue_from_double(ctx, flt_t, 2.0);
    gcc_jit_rvalue *i3 = gcc_jit_context_new_rvalue_from_int(ctx, int_t, 3);
    gcc_jit_rvalue *out_v = NULL;

    /* 1. gcc_gen_be_build_vector_rvalue & gcc_gen_be_splat_vector_rvalue invalid args & missing context/block */
    gcc_jit_rvalue *lanes_ok[4] = { f2, i3, f2, f2 }; /* mixes flt_t and int_t (cast branch!) */
    gcc_jit_rvalue *lanes_bad[4] = { f2, NULL, f2, f2 };
    ASSERT_EQ(gcc_gen_be_build_vector_rvalue(NULL, flt_t, 4, lanes_ok, &out_v), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_build_vector_rvalue(v4f_t, NULL, 4, lanes_ok, &out_v), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_build_vector_rvalue(v4f_t, flt_t, 0, lanes_ok, &out_v), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_build_vector_rvalue(v4f_t, flt_t, 4, NULL, &out_v), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_build_vector_rvalue(v4f_t, flt_t, 4, lanes_ok, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_build_vector_rvalue(v4f_t, flt_t, 4, lanes_bad, &out_v), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);

    ASSERT_EQ(gcc_gen_be_splat_vector_rvalue(NULL, flt_t, 4, f2, &out_v), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_splat_vector_rvalue(v4f_t, NULL, 4, f2, &out_v), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_splat_vector_rvalue(v4f_t, flt_t, 0, f2, &out_v), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_splat_vector_rvalue(v4f_t, flt_t, 4, NULL, &out_v), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_splat_vector_rvalue(v4f_t, flt_t, 4, f2, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    ASSERT_EQ(gcc_gen_be_build_vector_rvalue(v4f_t, flt_t, 4, lanes_ok, &out_v), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* Test both p_gcc_jit_context_new_rvalue_from_vector != NULL and == NULL memory-slot fallback */
    pfn_gcc_jit_context_new_rvalue_from_vector saved_vec_pfn = p_gcc_jit_context_new_rvalue_from_vector;
    p_gcc_jit_context_new_rvalue_from_vector = &mock_new_rvalue_from_vector;
    GCC_GEN_BE_CHECK(gcc_gen_be_build_vector_rvalue(v4f_t, flt_t, 4, lanes_ok, &out_v));
    p_gcc_jit_context_new_rvalue_from_vector = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    ASSERT_EQ(gcc_gen_be_build_vector_rvalue(v4f_t, flt_t, 4, lanes_ok, &out_v), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));
    for (int b = 0; b < 10; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_splat_vector_rvalue(v4f_t, flt_t, 4, f2, &out_v);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    p_gcc_jit_context_new_rvalue_from_vector = saved_vec_pfn;

    /* 2. gcc_gen_be_try_lower_simd_builtin validation & context/block state checks */
    a_boolean handled = FALSE;
    ASSERT_EQ(gcc_gen_be_try_lower_simd_builtin(NULL, NULL, 0, NULL, &handled, &out_v), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_try_lower_simd_builtin("foo", NULL, 0, NULL, NULL, &out_v), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_try_lower_simd_builtin("foo", NULL, 0, NULL, &handled, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_clz", NULL, 1, &i3, &handled, &out_v));
    ASSERT_EQ(handled, FALSE);
    ASSERT_EQ(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_addps", NULL, 0, &f2, &handled, &out_v), GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED);
    ASSERT_EQ(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vaddq", NULL, 1, NULL, &handled, &out_v), GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED);

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    ASSERT_EQ(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_addps", NULL, 1, &f2, &handled, &out_v), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    ASSERT_EQ(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_addps", NULL, 1, &f2, &handled, &out_v), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));

    /* Vector arguments & pointer argument */
    gcc_jit_lvalue *vf_loc = gcc_jit_function_new_local(test_fn, NULL, v4f_t, "vf");
    gcc_jit_lvalue *vi_loc = gcc_jit_function_new_local(test_fn, NULL, v4i_t, "vi");
    gcc_jit_rvalue *vf_rv = gcc_jit_lvalue_as_rvalue(vf_loc);
    gcc_jit_rvalue *vi_rv = gcc_jit_lvalue_as_rvalue(vi_loc);
    gcc_jit_rvalue *ptr_rv = gcc_jit_lvalue_get_address(vf_loc, NULL);

    /* EDG vector return type with typeref chain (both normal size and small size < elem_tp->size) */
    a_type flt_edg = {};
    flt_edg.kind = tk_float;
    flt_edg.size = 4;
    flt_edg.variant.float_kind = fk_float;
    a_type flt_tref = {};
    flt_tref.kind = tk_typeref;
    flt_tref.variant.typeref.type = &flt_edg;

    a_type vec_edg = {};
    vec_edg.kind = tk_vector;
    vec_edg.size = 16;
    vec_edg.variant.vector.element_type = &flt_tref;
    a_type vec_tref = {};
    vec_tref.kind = tk_typeref;
    vec_tref.variant.typeref.type = &vec_edg;

    a_type rout_tp = {};
    rout_tp.kind = tk_routine;
    rout_tp.variant.routine.return_type = &vec_tref;
    a_routine simd_rout = {};
    simd_rout.type = &rout_tp;

    /* Test resolve_simd_vector_geometry with elem_tp.size == 0, vec_edg.size < elem_tp->size, typeref->null, null element_type */
    flt_edg.size = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vdup_n_f32", &simd_rout, 1, &f2, &handled, &out_v));
    flt_edg.size = 4;
    vec_edg.size = 2;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vdup_n_f32", &simd_rout, 1, &f2, &handled, &out_v));
    vec_edg.size = 16;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_vbroadcastss", &simd_rout, 1, &f2, &handled, &out_v));

    /* elem_tp is typeref -> NULL */
    flt_tref.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vdup_n_f32", &simd_rout, 1, &vf_rv, &handled, &out_v));
    flt_tref.variant.typeref.type = &flt_edg;

    /* edg_vec_type is typeref -> NULL */
    vec_tref.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vdup_n_f32", &simd_rout, 1, &vf_rv, &handled, &out_v));
    vec_tref.variant.typeref.type = &vec_edg;

    a_type vec_no_elem = {};
    vec_no_elem.kind = tk_vector;
    vec_no_elem.variant.vector.element_type = NULL;
    rout_tp.variant.routine.return_type = &vec_no_elem;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vdup_n_f32", &simd_rout, 1, &vf_rv, &handled, &out_v));

    /* return_type is non-vector (tk_float) -> hits edg_vec_type->kind == tk_vector == False! */
    rout_tp.variant.routine.return_type = &flt_edg;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vdup_n_f32", &simd_rout, 1, &vf_rv, &handled, &out_v));

    /* Fallback with fallback_jit_type == NULL? No, args[0] always has a type, let's test rout with non-routine type & null type */
    a_routine rout_nontp = {};
    rout_nontp.type = &flt_edg;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_unknown_op", &rout_nontp, 1, &vf_rv, &handled, &out_v));
    rout_nontp.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_vdup_n_bad_arity", &rout_nontp, 2, lanes_ok, &handled, &out_v));
    rout_tp.variant.routine.return_type = &vec_tref;

    /* Lane extract: vec_ext_, vget_lane, vgetq_lane, plus bad arity & non-integral index */
    gcc_jit_rvalue *ext_ok[2] = { vf_rv, i3 };
    gcc_jit_rvalue *ext_bad_idx[2] = { vf_rv, f2 };
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_vec_ext_v4sf", &simd_rout, 2, ext_ok, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vget_lane_f32", &simd_rout, 2, ext_ok, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vgetq_lane_f32", &simd_rout, 2, ext_ok, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vgetq_lane_f32", &simd_rout, 1, ext_ok, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vgetq_lane_f32", &simd_rout, 2, ext_bad_idx, &handled, &out_v));

    /* Lane insert: ia32 vec_set_ (same lane type & cast lane type) and neon vset_lane / vsetq_lane, plus bad arity & non-integral index */
    gcc_jit_rvalue *set_ia32_same[3] = { vf_rv, f2, i3 };
    gcc_jit_rvalue *set_ia32_cast[3] = { vf_rv, i3, i3 };
    gcc_jit_rvalue *set_neon[3] = { f2, vf_rv, i3 };
    gcc_jit_rvalue *set_bad_idx[3] = { vf_rv, f2, f2 };
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_vec_set_v4sf", &simd_rout, 3, set_ia32_same, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_vec_set_v4sf", &simd_rout, 3, set_ia32_cast, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vset_lane_f32", &simd_rout, 3, set_neon, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vsetq_lane_f32", &simd_rout, 3, set_neon, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vsetq_lane_f32", &simd_rout, 2, set_neon, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vsetq_lane_f32", &simd_rout, 3, set_bad_idx, &handled, &out_v));

    /* Unaligned load & store + non-pointer / bad arity fallbacks */
    gcc_jit_rvalue *store_ok[2] = { ptr_rv, vf_rv };
    gcc_jit_rvalue *store_bad[2] = { f2, vf_rv };
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_loadups", &simd_rout, 1, &ptr_rv, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_loaddqu", &simd_rout, 1, &ptr_rv, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vld1q_f32", &simd_rout, 1, &ptr_rv, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vld1q_f32", &simd_rout, 1, &f2, &handled, &out_v));

    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_storeups", &simd_rout, 2, store_ok, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_storedqu", &simd_rout, 2, store_ok, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vst1q_f32", &simd_rout, 2, store_ok, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vst1q_f32", &simd_rout, 1, store_ok, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vst1q_f32", &simd_rout, 2, store_bad, &handled, &out_v));

    /* All 2-arg binary arithmetic, bitwise, and min/max prefixes */
    gcc_jit_rvalue *pair_f[2] = { vf_rv, vf_rv };
    gcc_jit_rvalue *pair_i[2] = { vi_rv, vi_rv };
    const char *bin_float_ops[] = {
        "__builtin_ia32_addps", "__builtin_neon_vaddq_f32",
        "__builtin_ia32_subps", "__builtin_neon_vsubq_f32",
        "__builtin_ia32_mulps", "__builtin_neon_vmulq_f32",
        "__builtin_ia32_divps", "__builtin_neon_vdivq_f32",
        "__builtin_ia32_minps", "__builtin_ia32_pminsd", "__builtin_neon_vminq_f32",
        "__builtin_ia32_maxps", "__builtin_ia32_pmaxsd", "__builtin_neon_vmaxq_f32",
        "__builtin_ia32_unhandled_bin2"
    };
    for (size_t k = 0; k < sizeof(bin_float_ops) / sizeof(bin_float_ops[0]); ++k) {
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin(bin_float_ops[k], &simd_rout, 2, pair_f, &handled, &out_v));
    }
    const char *bin_int_ops[] = {
        "__builtin_ia32_paddd", "__builtin_ia32_psubd", "__builtin_ia32_pmulld",
        "__builtin_ia32_pandn", "__builtin_ia32_andnps",
        "__builtin_ia32_pand", "__builtin_ia32_andps", "__builtin_neon_vandq_s32",
        "__builtin_ia32_por", "__builtin_ia32_orps", "__builtin_neon_vorrq_s32",
        "__builtin_ia32_pxor", "__builtin_ia32_xorps", "__builtin_neon_veorq_s32"
    };
    for (size_t k = 0; k < sizeof(bin_int_ops) / sizeof(bin_int_ops[0]); ++k) {
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin(bin_int_ops[k], NULL, 2, pair_i, &handled, &out_v));
    }

    /* FMA (ia32 & neon + wrong arity) and horizontal reduction vaddv (+ wrong arity) */
    gcc_jit_rvalue *fma_f[3] = { vf_rv, vf_rv, vf_rv };
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_vfmaddps", &simd_rout, 3, fma_f, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vfmaq_f32", &simd_rout, 3, fma_f, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vfmaq_f32", &simd_rout, 2, fma_f, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vaddvq_f32", &simd_rout, 1, &vf_rv, &handled, &out_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_simd_builtin("__builtin_neon_vaddvq_f32", &simd_rout, 3, fma_f, &handled, &out_v));

    /* OOM countdown sweeps across min/max, FMA, vaddv, and geometry resolution */
    for (int b = 0; b < 40; ++b) {
        gcc_jit_block *step_blk = gcc_jit_function_new_block(test_fn, "simd_oom_blk");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(step_blk));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_minps", &simd_rout, 2, pair_f, &handled, &out_v);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 40; ++b) {
        gcc_jit_block *step_blk = gcc_jit_function_new_block(test_fn, "simd_fma_oom_blk");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(step_blk));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_try_lower_simd_builtin("__builtin_ia32_vfmaddps", &simd_rout, 3, fma_f, &handled, &out_v);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exhaustively exercises `src/gcc_gen_be_atomic.c` across all C11 `_Atomic(T)`, RMW, generic, and OOM paths.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] test_fn Active `gcc_jit_function`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t test_exhaustive_atomic_api(
    gcc_jit_context *ctx,
    gcc_jit_function *test_fn) noexcept {
    /* 1. gcc_gen_be_is_c11_atomic_type */
    a_boolean is_at = FALSE;
    ASSERT_EQ(gcc_gen_be_is_c11_atomic_type(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    GCC_GEN_BE_CHECK(gcc_gen_be_is_c11_atomic_type(NULL, &is_at));
    ASSERT_EQ(is_at, FALSE);

    a_type u8_tp = {}, u16_tp = {}, u32_tp = {}, u64_tp = {}, f32_tp = {}, st16_tp = {};
    u8_tp.kind = tk_integer; u8_tp.size = 1; u8_tp.variant.integer.int_kind = ik_unsigned_char;
    u16_tp.kind = tk_integer; u16_tp.size = 2; u16_tp.variant.integer.int_kind = ik_unsigned_short;
    u32_tp.kind = tk_integer; u32_tp.size = 4; u32_tp.variant.integer.int_kind = ik_unsigned_int;
    u64_tp.kind = tk_integer; u64_tp.size = 8; u64_tp.variant.integer.int_kind = ik_unsigned_long_long;
    f32_tp.kind = tk_float; f32_tp.size = 4; f32_tp.variant.float_kind = fk_float;
    st16_tp.kind = tk_struct; st16_tp.size = 16;

    a_type at_inner = {}, at_outer = {}, tref_bad = {};
    at_inner.kind = tk_typeref; at_inner.variant.typeref.qualifiers = TQ_C11_ATOMIC; at_inner.variant.typeref.type = &u32_tp;
    at_outer.kind = tk_typeref; at_outer.variant.typeref.qualifiers = TQ_CONST; at_outer.variant.typeref.type = &at_inner;
    tref_bad.kind = tk_typeref; tref_bad.variant.typeref.type = NULL;

    GCC_GEN_BE_CHECK(gcc_gen_be_is_c11_atomic_type(&at_outer, &is_at));
    ASSERT_EQ(is_at, TRUE);
    GCC_GEN_BE_CHECK(gcc_gen_be_is_c11_atomic_type(&u32_tp, &is_at));
    ASSERT_EQ(is_at, FALSE);

    /* Locals for 1/2/4/8/16-byte & float & pointer targets */
    gcc_jit_type *ju8 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT8_T);
    gcc_jit_type *ju16 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT16_T);
    gcc_jit_type *ju32 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T);
    gcc_jit_type *ju64 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
    gcc_jit_type *jf32 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
    gcc_jit_type *ji32 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jp32 = gcc_jit_type_get_pointer(ji32);
    gcc_jit_type *jst16 = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&st16_tp, &jst16));

    gcc_jit_lvalue *lv8 = gcc_jit_function_new_local(test_fn, NULL, ju8, "at8");
    gcc_jit_lvalue *lv16 = gcc_jit_function_new_local(test_fn, NULL, ju16, "at16");
    gcc_jit_lvalue *lv32 = gcc_jit_function_new_local(test_fn, NULL, ju32, "at32");
    gcc_jit_lvalue *lv64 = gcc_jit_function_new_local(test_fn, NULL, ju64, "at64");
    gcc_jit_lvalue *lvf = gcc_jit_function_new_local(test_fn, NULL, jf32, "atf");
    gcc_jit_lvalue *lvp = gcc_jit_function_new_local(test_fn, NULL, jp32, "atp");
    gcc_jit_lvalue *lvst = gcc_jit_function_new_local(test_fn, NULL, jst16, "atst");

    gcc_jit_rvalue *rv_i1 = gcc_jit_context_new_rvalue_from_int(ctx, ji32, 1);
    gcc_jit_rvalue *rv_u32 = gcc_jit_context_new_rvalue_from_int(ctx, ju32, 7);
    gcc_jit_rvalue *rv_f32 = gcc_jit_context_new_rvalue_from_double(ctx, jf32, 3.5);
    gcc_jit_rvalue *rv_st = gcc_jit_lvalue_as_rvalue(lvst);
    gcc_jit_rvalue *out_rv = NULL;

    gcc_jit_block *blk = gcc_jit_function_new_block(test_fn, "atomic_entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));

    /* 2. C11 load & store validation, state errors, typeref->null, 1/2/4/8/float/16B, cache hit, OOM */
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_load(NULL, lv32, &out_rv), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_load(&u32_tp, NULL, &out_rv), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_load(&u32_tp, lv32, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_load(&tref_bad, lv32, &out_rv), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);

    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_store(NULL, lv32, rv_u32, &out_rv), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_store(&u32_tp, NULL, rv_u32, &out_rv), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_store(&u32_tp, lv32, NULL, &out_rv), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_store(&u32_tp, lv32, rv_u32, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_store(&tref_bad, lv32, rv_u32, &out_rv), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_load(&u32_tp, lv32, &out_rv), GCC_GEN_BE_ERROR_INVALID_STATE);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_store(&u32_tp, lv32, rv_u32, &out_rv), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_load(&u32_tp, lv32, &out_rv), GCC_GEN_BE_ERROR_INVALID_STATE);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_store(&u32_tp, lv32, rv_u32, &out_rv), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_load(&u8_tp, lv8, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_load(&u16_tp, lv16, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_load(&at_outer, lv32, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_load(&u64_tp, lv64, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_load(&f32_tp, lvf, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_load(&st16_tp, lvst, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_load(&st16_tp, lvst, &out_rv)); /* cached libatomic fn */

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_store(&u8_tp, lv8, rv_i1, &out_rv)); /* cast rhs */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_store(&u32_tp, lv32, rv_u32, &out_rv)); /* exact rhs */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_store(&f32_tp, lvf, rv_f32, &out_rv)); /* float reinterpret */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_store(&st16_tp, lvst, rv_st, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_store(&st16_tp, lvst, rv_st, &out_rv));

    for (int b = 0; b < 25; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_FUNC));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_c11_atomic_load(&f32_tp, lvf, &out_rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 25; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_FUNC));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_c11_atomic_load(&st16_tp, lvst, &out_rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 25; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_FUNC));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_c11_atomic_store(&f32_tp, lvf, rv_f32, &out_rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 25; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_FUNC));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_c11_atomic_store(&st16_tp, lvst, rv_st, &out_rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* 3. C11 RMW across integer, pointer, float, mul/div, pre/post, invalid op, non-lockfree, and OOM */
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_rmw(eok_add_assign, NULL, lv32, rv_u32, &out_rv), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_rmw(eok_add_assign, &u32_tp, NULL, rv_u32, &out_rv), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_rmw(eok_add_assign, &u32_tp, lv32, rv_u32, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_rmw(eok_assign, &u32_tp, lv32, rv_u32, &out_rv), GCC_GEN_BE_ERROR_UNSUPPORTED);
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_rmw(eok_add_assign, &st16_tp, lvst, rv_u32, &out_rv), GCC_GEN_BE_ERROR_ATOMIC_ALIGNMENT_FAILED);

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_rmw(eok_add_assign, &u32_tp, lv32, rv_u32, &out_rv), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    ASSERT_EQ(gcc_gen_be_lower_c11_atomic_rmw(eok_add_assign, &u32_tp, lv32, rv_u32, &out_rv), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));

    const an_expr_operator_kind int_rmw_ops[] = {
        eok_add_assign, eok_pre_incr, eok_post_incr,
        eok_subtract_assign, eok_pre_decr, eok_post_decr,
        eok_and_assign, eok_or_assign, eok_xor_assign
    };
    for (size_t k = 0; k < sizeof(int_rmw_ops) / sizeof(int_rmw_ops[0]); ++k) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_rmw(int_rmw_ops[k], &u32_tp, lv32, (k % 2 == 0) ? rv_u32 : NULL, &out_rv));
    }
    /* Signed int32 target so delta_word (uint32) != p1_t or reinterpret_atomic_bits casts */
    a_type s32_tp = {};
    s32_tp.kind = tk_integer; s32_tp.size = 4; s32_tp.variant.integer.int_kind = ik_int;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_rmw(eok_add_assign, &s32_tp, lv32, rv_i1, &out_rv));

    /* Pointer RMW: typeref pointee, null pointee, zero-size pointee, with & without rhs_rval */
    a_type ptee_zero = {};
    ptee_zero.kind = tk_integer; ptee_zero.size = 0;
    a_type ptee_tref = {};
    ptee_tref.kind = tk_typeref; ptee_tref.variant.typeref.type = &s32_tp;
    a_type ptr_edg = {};
    ptr_edg.kind = tk_pointer; ptr_edg.size = 8; ptr_edg.variant.pointer.type = &ptee_tref;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_rmw(eok_padd_assign, &ptr_edg, lvp, rv_i1, &out_rv));
    ptr_edg.variant.pointer.type = &ptee_zero;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_rmw(eok_psubtract_assign, &ptr_edg, lvp, NULL, &out_rv));
    ptr_edg.variant.pointer.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_rmw(eok_pre_incr, &ptr_edg, lvp, NULL, &out_rv));

    /* CAS loop RMW: multiply, divide, float post_incr (returns_old == TRUE, rhs == NULL), float add (exact rhs & cast rhs) */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_rmw(eok_multiply_assign, &u32_tp, lv32, rv_u32, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_rmw(eok_divide_assign, &u32_tp, lv32, rv_i1, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_rmw(eok_post_incr, &f32_tp, lvf, NULL, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_c11_atomic_rmw(eok_add_assign, &f32_tp, lvf, rv_f32, &out_rv));

    for (int b = 0; b < 25; ++b) {
        gcc_jit_block *rb = gcc_jit_function_new_block(test_fn, "rmw_int_oom");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(rb));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_c11_atomic_rmw(eok_add_assign, &s32_tp, lv32, rv_i1, &out_rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 25; ++b) {
        gcc_jit_block *rb = gcc_jit_function_new_block(test_fn, "rmw_cas_oom");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(rb));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_c11_atomic_rmw(eok_multiply_assign, &f32_tp, lvf, rv_f32, &out_rv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* 4. Generic __atomic_* calls & infer_routine_pointee_size */
    gcc_jit_block *gblk = gcc_jit_function_new_block(test_fn, "generic_atomic_blk");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gblk));

    gcc_jit_rvalue *p_addr = gcc_jit_lvalue_get_address(lv32, NULL);
    gcc_jit_rvalue *gargs3[3] = { p_addr, p_addr, rv_i1 };
    gcc_jit_rvalue *gargs4_sz[4] = { rv_i1, p_addr, p_addr, rv_i1 };
    gcc_jit_rvalue *gargs4_ex[4] = { p_addr, p_addr, p_addr, rv_i1 };
    gcc_jit_rvalue *gargs5_ex[5] = { rv_i1, p_addr, p_addr, p_addr, rv_i1 };
    gcc_jit_rvalue *gargs6_cas[6] = { p_addr, p_addr, p_addr, rv_i1, rv_i1, rv_i1 };
    gcc_jit_rvalue *gargs6_casn[6] = { p_addr, p_addr, rv_u32, rv_i1, rv_i1, rv_i1 };
    gcc_jit_rvalue *gargs7_cas[7] = { rv_i1, p_addr, p_addr, p_addr, rv_i1, rv_i1, rv_i1 };
    gcc_jit_rvalue *gargs7_casn[7] = { rv_i1, p_addr, p_addr, rv_u32, rv_i1, rv_i1, rv_i1 };

    ASSERT_EQ(gcc_gen_be_lower_generic_atomic_call(NULL, NULL, 3, gargs3, 4, &out_rv), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_generic_atomic_call("__atomic_load", NULL, 2, gargs3, 4, &out_rv), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_generic_atomic_call("__atomic_load", NULL, 3, NULL, 4, &out_rv), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_generic_atomic_call("__atomic_load", NULL, 3, gargs3, 4, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_lower_generic_atomic_call("__atomic_bogus", NULL, 3, gargs3, 4, &out_rv), GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN);

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    ASSERT_EQ(gcc_gen_be_lower_generic_atomic_call("__atomic_load", NULL, 3, gargs3, 4, &out_rv), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* Bad arities */
    ASSERT_EQ(gcc_gen_be_lower_generic_atomic_call("__atomic_load", NULL, 5, gargs5_ex, 4, &out_rv), GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH);
    ASSERT_EQ(gcc_gen_be_lower_generic_atomic_call("__atomic_store", NULL, 5, gargs5_ex, 4, &out_rv), GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH);
    ASSERT_EQ(gcc_gen_be_lower_generic_atomic_call("__atomic_exchange", NULL, 3, gargs3, 4, &out_rv), GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH);
    ASSERT_EQ(gcc_gen_be_lower_generic_atomic_call("__atomic_compare_exchange", NULL, 5, gargs5_ex, 4, &out_rv), GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH);

    /* Routine descriptors exercising every branch of infer_routine_pointee_size */
    a_param_type p0 = {}, p1 = {};
    a_routine_type_supplement rsup = {};
    a_type rtp = {};
    rtp.kind = tk_routine;
    rtp.variant.routine.extra_info = &rsup;
    a_routine gro = {};
    gro.type = &rtp;

    /* rout == NULL, rout->type == NULL, rout->type->kind != tk_routine, extra_info == NULL, param_type_list == NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_load", NULL, 3, gargs3, 0, &out_rv));
    gro.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_load", &gro, 4, gargs4_sz, 0, &out_rv));
    gro.type = &u32_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_store", &gro, 3, gargs3, 0, &out_rv));
    gro.type = &rtp;
    rtp.variant.routine.extra_info = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_store", &gro, 4, gargs4_sz, 0, &out_rv));
    rtp.variant.routine.extra_info = &rsup;
    rsup.param_type_list = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_exchange", &gro, 4, gargs4_ex, 0, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_exchange", &gro, 5, gargs5_ex, 0, &out_rv));

    /* p->type == NULL, p->type typeref->non-pointer, pointer->NULL, pointer->typeref->NULL, pointer->typeref->u32_tp */
    rsup.param_type_list = &p0;
    p0.type = NULL;
    p0.next = &p1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_load", &gro, 3, gargs3, 0, &out_rv));
    a_type pt_nonptr_ref = {};
    pt_nonptr_ref.kind = tk_typeref; pt_nonptr_ref.variant.typeref.type = &u32_tp;
    p1.type = &pt_nonptr_ref;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_load", &gro, 4, gargs4_sz, 0, &out_rv));
    p1.type = &tref_bad;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_load", &gro, 4, gargs4_sz, 0, &out_rv));

    a_type ptr_null_elem = {};
    ptr_null_elem.kind = tk_pointer; ptr_null_elem.variant.pointer.type = NULL;
    p1.type = &ptr_null_elem;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_load", &gro, 4, gargs4_sz, 0, &out_rv));

    ptr_null_elem.variant.pointer.type = &tref_bad;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_load", &gro, 4, gargs4_sz, 0, &out_rv));

    a_type elem_ref = {};
    elem_ref.kind = tk_typeref; elem_ref.variant.typeref.type = &u32_tp;
    ptr_null_elem.variant.pointer.type = &elem_ref;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_load", &gro, 4, gargs4_sz, 0, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_store", &gro, 4, gargs4_sz, 0, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_exchange", &gro, 5, gargs5_ex, 0, &out_rv));

    /* Explicit lockfree (4B) and libatomic (16B) with cblock != NULL and cblock == NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_load", NULL, 3, gargs3, 4, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_load", NULL, 3, gargs3, 16, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_store", NULL, 3, gargs3, 4, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_store", NULL, 3, gargs3, 16, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_exchange", NULL, 4, gargs4_ex, 4, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_exchange", NULL, 4, gargs4_ex, 16, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_compare_exchange", NULL, 6, gargs6_cas, 4, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_compare_exchange", NULL, 6, gargs6_cas, 0, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_compare_exchange", NULL, 7, gargs7_cas, 16, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_compare_exchange_n", NULL, 6, gargs6_casn, 0, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_compare_exchange_n", NULL, 7, gargs7_casn, 16, &out_rv));

    /* cblock == NULL fallback paths */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_load", NULL, 3, gargs3, 4, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_exchange", NULL, 4, gargs4_ex, 4, &out_rv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_generic_atomic_call("__atomic_compare_exchange", NULL, 6, gargs6_cas, 4, &out_rv));
    ASSERT_EQ(gcc_gen_be_lower_generic_atomic_call("__atomic_compare_exchange_n", NULL, 6, gargs6_casn, 4, &out_rv), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gblk));

    /* OOM sweeps across all 4 generic atomic operations */
    const char *gen_names[5] = {
        "__atomic_load", "__atomic_store", "__atomic_exchange",
        "__atomic_compare_exchange", "__atomic_compare_exchange_n"
    };
    const int gen_arities[5] = { 4, 4, 5, 7, 6 };
    gcc_jit_rvalue **gen_argvecs[5] = { gargs4_sz, gargs4_sz, gargs5_ex, gargs7_cas, gargs6_casn };
    for (int k = 0; k < 5; ++k) {
        for (int sz_mode = 0; sz_mode < 2; ++sz_mode) {
            unsigned long sz_arg = (sz_mode == 0) ? 0UL : 16UL;
            for (int b = 0; b < 20; ++b) {
                GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_FUNC));
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t e = gcc_gen_be_lower_generic_atomic_call(gen_names[k], &gro, gen_arities[k], gen_argvecs[k], sz_arg, &out_rv);
                g_gcc_be_oom_countdown = -1;
                if (e == GCC_GEN_BE_SUCCESS) break;
            }
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Builds a canonical `stmk_for` AST node for OpenMP loop unit tests.
 * @param[out] f_stmt Destination `stmk_for` statement.
 * @param[out] f_info Destination `a_for_loop`.
 * @param[out] init_s Destination initialization statement.
 * @param[out] dip Destination `a_dynamic_init`.
 * @param[out] lb_e Destination lower-bound expression.
 * @param[out] ub_e Destination upper-bound expression.
 * @param[out] iv_e Destination IV expression in loop condition.
 * @param[out] cond_e Destination comparison expression.
 * @param[out] inc_e Destination increment expression.
 * @param[out] iv_inc_e Destination IV expression in increment.
 * @param[in] iv_var Induction variable pointer.
 * @param[in] cmp_op Loop comparison operator (`eok_lt`, `eok_le`, `eok_gt`, `eok_ge`, `eok_ne`).
 * @param[in] inc_op Loop increment operator (`eok_post_incr`, `eok_post_decr`, `eok_add_assign`, etc.).
 * @param[in] step_e Optional step expression for `+=`/`-=` (may be NULL).
 * @param[in] body_s Loop body statement.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t init_canonical_for_ast(
    a_statement *f_stmt,
    a_for_loop *f_info,
    a_statement *init_s,
    a_dynamic_init *dip,
    an_expr_node *lb_e,
    an_expr_node *ub_e,
    an_expr_node *iv_e,
    an_expr_node *cond_e,
    an_expr_node *inc_e,
    an_expr_node *iv_inc_e,
    a_variable_ptr iv_var,
    an_expr_operator_kind cmp_op,
    an_expr_operator_kind inc_op,
    an_expr_node_ptr step_e,
    a_statement_ptr body_s) noexcept {
    *f_stmt = {};
    *f_info = {};
    *init_s = {};
    *dip = {};
    *lb_e = {};
    *ub_e = {};
    *iv_e = {};
    *cond_e = {};
    *inc_e = {};
    *iv_inc_e = {};

    lb_e->kind = enk_constant;
    lb_e->type = iv_var->type;
    ub_e->kind = enk_constant;
    ub_e->type = iv_var->type;
    iv_e->kind = enk_variable;
    iv_e->type = iv_var->type;
    iv_e->variant.variable.ptr = iv_var;
    iv_e->next = ub_e;

    cond_e->kind = enk_operation;
    cond_e->variant.operation.kind = cmp_op;
    cond_e->variant.operation.operands = iv_e;

    dip->kind = dik_expression;
    dip->variable = iv_var;
    dip->variant.expression = lb_e;
    init_s->kind = stmk_init;
    init_s->variant.dynamic_init = dip;

    iv_inc_e->kind = enk_variable;
    iv_inc_e->type = iv_var->type;
    iv_inc_e->variant.variable.ptr = iv_var;
    iv_inc_e->next = step_e;
    inc_e->kind = enk_operation;
    inc_e->variant.operation.kind = inc_op;
    inc_e->variant.operation.operands = iv_inc_e;

    f_info->initialization = init_s;
    f_info->increment = inc_e;
    f_stmt->kind = stmk_for;
    f_stmt->expr = cond_e;
    f_stmt->variant.for_loop.extra_info = f_info;
    f_stmt->variant.for_loop.statement = body_s;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exhaustively exercises `src/gcc_gen_be_omp.c` across all constructs, clauses, source scanning, and OOM paths.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] test_fn Active parent `gcc_jit_function`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t test_exhaustive_omp_api(gcc_jit_context *ctx, gcc_jit_function *test_fn) noexcept {
    gcc_jit_block *blk = gcc_jit_function_new_block(test_fn, "omp_suite_entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));

    /* 1. Argument validation & locate_omp_payload / bind_omp_pragma / resize / dedup */
    a_boolean handled = FALSE;
    a_statement empty_s = {};
    empty_s.kind = stmk_empty;
    ASSERT_EQ(gcc_gen_be_bind_omp_pragma(NULL, NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(NULL, test_fn, &handled), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&empty_s, NULL, &handled), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&empty_s, test_fn, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(gcc_gen_be_flush_trailing_omp_pragmas(NULL, NULL), GCC_GEN_BE_ERROR_INVALID_ARGUMENT);

    /* Count == 0 early returns */
    gcc_gen_be_singleton_state.omp_pragma_count = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&empty_s, test_fn, &handled));
    ASSERT_EQ(handled, FALSE);
    GCC_GEN_BE_CHECK(gcc_gen_be_flush_trailing_omp_pragmas(NULL, test_fn));
    a_scope dummy_scope = {};
    GCC_GEN_BE_CHECK(gcc_gen_be_flush_trailing_omp_pragmas(&dummy_scope, test_fn));

    /* Non-OpenMP pragma strings ignored cleanly */
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_s, NULL, "   "));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_s, NULL, "#  ident"));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_s, NULL, "# pragma pack(1)"));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_s, NULL, "pragma_not omp"));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_s, NULL, "omp_not"));
    ASSERT_EQ(gcc_gen_be_singleton_state.omp_pragma_count, 0U);

    /* Duplicate binding deduplication & >16 entries dynamic realloc + OOM */
    g_gcc_be_oom_countdown = 1;
    ASSERT_EQ(gcc_gen_be_bind_omp_pragma(&empty_s, NULL, "#  pragma   omp   barrier"), GCC_GEN_BE_ERROR_OOM);
    g_gcc_be_oom_countdown = -1;

    a_statement many_stmts[20];
    std::memset(many_stmts, 0, sizeof(many_stmts));
    for (int i = 0; i < 20; ++i) {
        many_stmts[i].kind = stmk_empty;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&many_stmts[i], NULL, "pragma omp barrier"));
    }
    /* Exact duplicate ignored */
    size_t prev_cnt = gcc_gen_be_singleton_state.omp_pragma_count;
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&many_stmts[0], NULL, "pragma omp barrier"));
    ASSERT_EQ(gcc_gen_be_singleton_state.omp_pragma_count, prev_cnt);

    /* Consume many_stmts[0] via barrier on stmk_empty, then re-bind same text (hits arr[i].consumed == TRUE in dedup loop) */
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&many_stmts[0], test_fn, &handled));
    ASSERT_EQ(handled, TRUE);
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&many_stmts[0], NULL, "pragma omp barrier"));
    /* Unmatched statement */
    a_statement unmatched_s = {};
    unmatched_s.kind = stmk_empty;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&unmatched_s, test_fn, &handled));
    ASSERT_EQ(handled, FALSE);

    /* Empty `"omp"` directive payload & unknown construct errors */
    a_statement empty_omp_s = {}, bad_omp_s = {};
    empty_omp_s.kind = stmk_empty;
    bad_omp_s.kind = stmk_empty;
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_omp_s, NULL, "omp"));
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&empty_omp_s, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_omp_s, NULL, "#pragma omp unknown_construct"));
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_omp_s, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

    /* Barrier & taskwait on stmk_empty AND on non-empty statement (which emits call and continues loop!), plus trailing scope flush */
    a_statement tw_empty = {}, bw_nonempty = {};
    tw_empty.kind = stmk_empty;
    bw_nonempty.kind = stmk_expr;
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&tw_empty, NULL, "omp taskwait"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&tw_empty, test_fn, &handled));
    ASSERT_EQ(handled, TRUE);

    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bw_nonempty, NULL, "#pragma omp barrier"));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bw_nonempty, NULL, "#pragma omp taskwait"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bw_nonempty, test_fn, &handled));
    ASSERT_EQ(handled, FALSE);

    a_scope scope_a = {}, scope_b = {};
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(NULL, &scope_a, "#pragma omp barrier"));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(NULL, &scope_a, "#pragma omp taskwait"));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(NULL, &scope_a, "#pragma omp simd"));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(NULL, &scope_b, "#pragma omp barrier"));
    GCC_GEN_BE_CHECK(gcc_gen_be_flush_trailing_omp_pragmas(&scope_a, test_fn));
    /* Second flush hits arr[i].consumed == TRUE */
    GCC_GEN_BE_CHECK(gcc_gen_be_flush_trailing_omp_pragmas(&scope_a, test_fn));

    /* 2. Source-line scanner (`read_preceding_omp_line`, `scan_omp_pragmas_in_stmt_tree`, `register_omp_pragmas_in_scope`) */
    GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(NULL));
    const char *omp_src = "/tmp/edg_omp_scan_unit.c";
    FILE *fp = std::fopen(omp_src, "wb");
    ASSERT_TRUE(fp != NULL);
    /* Line 1: normal C
       Line 2: `#pragma omp barrier\r\n`
       Line 3: `   // comment\n`
       Line 4: `   \t  \r\n`
       Line 5: stmt on line 5 (claims `#pragma omp barrier` from line 2!)
       Line 6: `/\n` (1-char slash, NOT `//`!)
       Line 7: stmt on line 7 (sees `/` -> not omp)
       Line 8: `omp barrier\n` (omp payload without `#`!)
       Line 9: stmt on line 9
       Line 10: >511 char `#pragma omp parallel ...` line!
       Line 11: stmt on line 11 */
    std::fprintf(fp, "int x = 0;\n");
    std::fprintf(fp, "  #pragma omp barrier  \r\n");
    std::fprintf(fp, "  // single line comment\n");
    std::fprintf(fp, "   \r\n");
    std::fprintf(fp, "/a\n");
    std::fprintf(fp, "/\n");
    std::fprintf(fp, "x++;\n");
    std::fprintf(fp, "omp barrier\n");
    std::fprintf(fp, "x++;\n");
    char huge_omp_line[540];
    std::memset(huge_omp_line, 'x', sizeof(huge_omp_line));
    std::memcpy(huge_omp_line, "#pragma omp parallel private(", 29);
    huge_omp_line[537] = ')';
    huge_omp_line[538] = '\n';
    huge_omp_line[539] = '\0';
    std::fwrite(huge_omp_line, 1, 539, fp);
    std::fprintf(fp, "x++;\n");
    std::fclose(fp);

    a_statement s_seq0 = {}, s_line1 = {}, s_line5 = {}, s_line5_dup = {}, s_line7 = {}, s_line9 = {}, s_line11 = {}, s_eof = {};
    s_seq0.kind = stmk_empty; s_seq0.position.seq = 0; s_seq0.next = &s_line1;
    s_line1.kind = stmk_empty; s_line1.position.seq = 1; s_line1.next = &s_line5;
    s_line5.kind = stmk_block; s_line5.position.seq = 5; s_line5.variant.block.statements = &s_line5_dup; s_line5.next = &s_line7;
    s_line5_dup.kind = stmk_empty; s_line5_dup.position.seq = 5;
    s_line7.kind = stmk_for; s_line7.position.seq = 7; s_line7.variant.for_loop.statement = &s_line9; s_line7.next = &s_line11;
    s_line9.kind = stmk_while; s_line9.position.seq = 9; s_line9.variant.loop_statement = &s_eof;
    s_eof.kind = stmk_end_test_while; s_eof.position.seq = 50; s_eof.variant.loop_statement = NULL;
    a_statement s_then = {}, s_else = {};
    s_then.kind = stmk_empty; s_then.position.seq = 5;
    s_else.kind = stmk_empty; s_else.position.seq = 7;
    s_line11.kind = stmk_if; s_line11.position.seq = 11;
    s_line11.variant.if_stmt.then_statement = &s_then;
    s_line11.variant.if_stmt.else_statement = &s_else;

    a_pragma pr_null = {}, pr_stmt_null = {}, pr_stmt_ok = {}, pr_trail = {};
    pr_null.pragma_text = NULL; pr_null.next = &pr_stmt_null;
    pr_stmt_null.pragma_text = "#pragma omp barrier"; pr_stmt_null.entity.kind = iek_statement; pr_stmt_null.entity.ptr = NULL; pr_stmt_null.next = &pr_stmt_ok;
    pr_stmt_ok.pragma_text = "#pragma omp barrier"; pr_stmt_ok.entity.kind = iek_statement; pr_stmt_ok.entity.ptr = reinterpret_cast<char *>(&s_seq0); pr_stmt_ok.next = &pr_trail;
    pr_trail.pragma_text = "#pragma omp taskwait"; pr_trail.entity.kind = iek_none;

    a_scope child_sc = {}, reg_sc = {};
    reg_sc.pragmas = &pr_null;
    reg_sc.assoc_block = &s_seq0;
    reg_sc.scopes = &child_sc;

    /* Test openmp_enabled == FALSE and assoc_block == NULL first */
    a_scope bare_sc = {};
    gcc_gen_be_singleton_state.openmp_enabled = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&bare_sc));

    /* Test full_fn == "" falling back to short_fn = omp_src */
    g_mock_seq_file_path = "";
    g_mock_short_fn = omp_src;
    GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&reg_sc));

    /* Test src_path == "" (empty file_path in read_preceding_omp_line), NULL src_path, and nonexistent file */
    a_statement s_probe = {};
    s_probe.kind = stmk_empty;
    s_probe.position.seq = 3;
    a_scope probe_sc = {};
    probe_sc.assoc_block = &s_probe;
    gcc_gen_be_singleton_state.openmp_enabled = TRUE;

    g_mock_seq_file_path = "";
    g_mock_short_fn = "";
    GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&probe_sc));

    g_mock_seq_file_path = NULL;
    g_mock_short_fn = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&probe_sc));

    g_mock_seq_file_path = "/nonexistent_edg_omp_file.c";
    GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&probe_sc));

    /* Test >256 distinct claimed lines in scan_omp_pragmas_in_stmt_tree */
    a_statement many_lines[260];
    std::memset(many_lines, 0, sizeof(many_lines));
    for (int i = 0; i < 260; ++i) {
        many_lines[i].kind = stmk_empty;
        many_lines[i].position.seq = (a_seq_number)(i + 2);
        many_lines[i].next = (i + 1 < 260) ? &many_lines[i + 1] : NULL;
    }
    probe_sc.assoc_block = &many_lines[0];
    GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&probe_sc));
    g_mock_seq_file_path = NULL;
    std::remove(omp_src);

    /* 3. Prepare local variables (int, uint64, float, typeref, static, extern, unbound, unnamed, 50 vars for cap) */
    a_type i32_t = {}, i8_t = {}, i16_t = {}, i64_t = {}, u64_t = {}, u64_ul_t = {}, u64_ui_t = {}, u64_bp_t = {}, f64_t = {}, big16_t = {};
    a_type tref_i32 = {}, tref_u64 = {}, tref_null_t = {};
    i32_t.kind = tk_integer; i32_t.size = 4; i32_t.variant.integer.int_kind = ik_int;
    i8_t.kind = tk_integer; i8_t.size = 1; i8_t.variant.integer.int_kind = ik_signed_char;
    i16_t.kind = tk_integer; i16_t.size = 2; i16_t.variant.integer.int_kind = ik_short;
    i64_t.kind = tk_integer; i64_t.size = 8; i64_t.variant.integer.int_kind = ik_long_long;
    u64_t.kind = tk_integer; u64_t.size = 8; u64_t.variant.integer.int_kind = ik_unsigned_long_long;
    u64_ul_t.kind = tk_integer; u64_ul_t.size = 8; u64_ul_t.variant.integer.int_kind = ik_unsigned_long;
    u64_ui_t.kind = tk_integer; u64_ui_t.size = 8; u64_ui_t.variant.integer.int_kind = ik_unsigned_int;
    an_integer_type_supplement bp64_sup = {}; bp64_sup.bit_width = 64;
    u64_bp_t.kind = tk_integer; u64_bp_t.size = 8; u64_bp_t.variant.integer.int_kind = ik_unsigned_bit_precise;
    u64_bp_t.variant.integer.extra_info = &bp64_sup;
    f64_t.kind = tk_float; f64_t.size = 8; f64_t.variant.float_kind = fk_double;
    big16_t.kind = tk_integer; big16_t.size = 16; big16_t.variant.integer.int_kind = ik_int128;
    tref_i32.kind = tk_typeref; tref_i32.size = 4; tref_i32.variant.typeref.type = &i32_t;
    tref_u64.kind = tk_typeref; tref_u64.size = 8; tref_u64.variant.typeref.type = &u64_t;
    tref_null_t.kind = tk_typeref; tref_null_t.variant.typeref.type = NULL;

    gcc_jit_type *jit_i32 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_i8 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIGNED_CHAR);
    gcc_jit_type *jit_i16 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SHORT);
    gcc_jit_type *jit_i64 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG);
    gcc_jit_type *jit_u64 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
    gcc_jit_type *jit_f64 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);

    a_variable v_sh = {}, v_pr = {}, v_fp = {}, v_lp = {}, v_red_i = {}, v_red_f = {}, v_anon = {};
    a_variable v_stat = {}, v_ext = {}, v_unbound = {};
    v_sh.source_corresp.name = "v_sh"; v_sh.type = &tref_i32;
    v_pr.source_corresp.name = "v_pr"; v_pr.type = &i32_t;
    v_fp.source_corresp.name = "v_fp"; v_fp.type = &i32_t;
    v_lp.source_corresp.name = "v_lp"; v_lp.type = &i32_t;
    v_red_i.source_corresp.name = "v_red_i"; v_red_i.type = &tref_i32;
    v_red_f.source_corresp.name = "v_red_f"; v_red_f.type = &f64_t;
    v_anon.source_corresp.name = NULL; v_anon.type = &i32_t;
    v_stat.source_corresp.name = "v_stat"; v_stat.type = &i32_t; v_stat.storage_class = sc_static;
    v_ext.source_corresp.name = "v_ext"; v_ext.type = &i32_t; v_ext.storage_class = sc_extern;
    v_unbound.source_corresp.name = "v_unbound"; v_unbound.type = &i32_t;

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_sh, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_sh")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pr, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_pr")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_fp, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_fp")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_lp, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_lp")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_red_i, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_ri")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_red_f, gcc_jit_function_new_local(test_fn, NULL, jit_f64, "lv_rf")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_anon, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_an")));

    /* Build rich body exercising all collect_vars_in_expr / collect_vars_in_stmt branches (including 50 vars > 48 cap!) */
    a_variable extra_vars[50];
    an_expr_node extra_exprs[50];
    std::memset(extra_vars, 0, sizeof(extra_vars));
    std::memset(extra_exprs, 0, sizeof(extra_exprs));
    for (int i = 0; i < 50; ++i) {
        extra_vars[i].type = &i32_t;
        extra_vars[i].source_corresp.name = "ex_v";
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &extra_vars[i], gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_ex")));
        extra_exprs[i].kind = enk_variable;
        extra_exprs[i].type = &i32_t;
        extra_exprs[i].variant.variable.ptr = &extra_vars[i];
        extra_exprs[i].next = (i + 1 < 50) ? &extra_exprs[i + 1] : NULL;
    }

    an_expr_node e_null_v = {}, e_stat = {}, e_ext = {}, e_unb = {}, e_sh1 = {}, e_sh2 = {};
    an_expr_node e_pr = {}, e_fp = {}, e_lp = {}, e_ri = {}, e_rf = {}, e_an = {};
    an_expr_node e_op = {}, e_cond_null = {}, e_cond_ok = {}, e_ti_null = {}, e_ti_zero = {}, e_ti_expr = {}, e_const = {};
    a_condition_supplement cond_obj = {};
    a_dynamic_init di_zero = {}, di_expr = {};

    e_null_v.kind = enk_variable; e_null_v.variant.variable.ptr = NULL; e_null_v.next = &e_stat;
    e_stat.kind = enk_variable; e_stat.variant.variable.ptr = &v_stat; e_stat.next = &e_ext;
    e_ext.kind = enk_variable; e_ext.variant.variable.ptr = &v_ext; e_ext.next = &e_unb;
    e_unb.kind = enk_variable; e_unb.variant.variable.ptr = &v_unbound; e_unb.next = &e_sh1;
    e_sh1.kind = enk_variable; e_sh1.variant.variable.ptr = &v_sh; e_sh1.next = &e_sh2;
    e_sh2.kind = enk_variable; e_sh2.variant.variable.ptr = &v_sh; e_sh2.next = &e_pr; /* duplicate seen==TRUE */
    e_pr.kind = enk_variable; e_pr.variant.variable.ptr = &v_pr; e_pr.next = &e_fp;
    e_fp.kind = enk_variable; e_fp.variant.variable.ptr = &v_fp; e_fp.next = &e_lp;
    e_lp.kind = enk_variable; e_lp.variant.variable.ptr = &v_lp; e_lp.next = &e_ri;
    e_ri.kind = enk_variable; e_ri.variant.variable.ptr = &v_red_i; e_ri.next = &e_rf;
    e_rf.kind = enk_variable; e_rf.variant.variable.ptr = &v_red_f; e_rf.next = &e_an;
    e_an.kind = enk_variable; e_an.variant.variable.ptr = &v_anon; e_an.next = &e_op;

    e_const.kind = enk_constant;
    e_op.kind = enk_operation; e_op.variant.operation.operands = &e_const; e_op.next = &e_cond_null;
    e_cond_null.kind = enk_condition; e_cond_null.variant.condition = NULL; e_cond_null.next = &e_cond_ok;
    cond_obj.expr = &e_const;
    e_cond_ok.kind = enk_condition; e_cond_ok.variant.condition = &cond_obj; e_cond_ok.next = &e_ti_null;
    e_ti_null.kind = enk_temp_init; e_ti_null.variant.init.dynamic_init = NULL; e_ti_null.next = &e_ti_zero;
    di_zero.kind = dik_zero;
    e_ti_zero.kind = enk_temp_init; e_ti_zero.variant.init.dynamic_init = &di_zero; e_ti_zero.next = &e_ti_expr;
    di_expr.kind = dik_expression; di_expr.variant.expression = &extra_exprs[0];
    e_ti_expr.kind = enk_temp_init; e_ti_expr.variant.init.dynamic_init = &di_expr;

    a_statement bs_if = {}, bs_while = {}, bs_do = {}, bs_for_noex = {}, bs_for_ex = {};
    a_statement bs_init_null = {}, bs_init_z = {}, bs_init_e = {}, bs_expr = {};
    a_for_loop dummy_fex = {};
    bs_if.kind = stmk_if; bs_if.expr = &e_null_v; bs_if.variant.if_stmt.then_statement = NULL; bs_if.variant.if_stmt.else_statement = NULL; bs_if.next = &bs_while;
    bs_while.kind = stmk_while; bs_while.expr = &e_const; bs_while.next = &bs_do;
    bs_do.kind = stmk_end_test_while; bs_do.expr = &e_const; bs_do.next = &bs_for_noex;
    bs_for_noex.kind = stmk_for; bs_for_noex.variant.for_loop.extra_info = NULL; bs_for_noex.next = &bs_for_ex;
    bs_for_ex.kind = stmk_for; bs_for_ex.variant.for_loop.extra_info = &dummy_fex; bs_for_ex.next = &bs_init_null;
    bs_init_null.kind = stmk_init; bs_init_null.variant.dynamic_init = NULL; bs_init_null.next = &bs_init_z;
    bs_init_z.kind = stmk_init; bs_init_z.variant.dynamic_init = &di_zero; bs_init_z.next = &bs_init_e;
    bs_init_e.kind = stmk_init; bs_init_e.variant.dynamic_init = &di_expr; bs_init_e.next = &bs_expr;
    bs_expr.kind = stmk_expr; bs_expr.expr = &e_const;

    a_statement rich_blk = {};
    rich_blk.kind = stmk_block;
    rich_blk.variant.block.statements = &bs_if;

    /* 4. Outlined `parallel` & `task` regions across all clauses, >32 clause vars, >63-char ident, and all reduction ops! */
    const char *red_ops[] = {
        "+", "-", "*", "&", "|", "^", "&&", "||", "min", "max", "minx", "maxx", "custom_op"
    };
    for (size_t ri = 0; ri < sizeof(red_ops) / sizeof(red_ops[0]); ++ri) {
        char ptxt[512];
        std::snprintf(
            ptxt, sizeof(ptxt),
            "#pragma omp parallel num_threads(4) shared(v_sh) private(v_pr, +) firstprivate(v_fp) lastprivate(v_lp) "
            "reduction(%s: v_red_i, v_red_f) proc_bind((close)) untied",
            red_ops[ri]);
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&rich_blk, NULL, ptxt));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&rich_blk, test_fn, &handled));
        ASSERT_EQ(handled, TRUE);
    }

    /* Clause parser edge cases: missing `(`, unclosed `(`, >32 clause vars, >63-char ident, duplicate var override,
       `if(0)`, `if(false)`, `if((1))`, unclosed `if((0`, `num_threads(0)`, `num_threads` no `(`, non-block parallel stmt,
       0 captured fields (`__dummy`), `task` with `if(0)` and `if(1)`, and `out_tail == NULL`! */
    a_statement empty_par_blk = {};
    empty_par_blk.kind = stmk_empty;
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(
        &empty_par_blk, NULL,
        "#pragma omp parallel , + num_threads num_threads(0) if(false) private x reduction y schedule z "
        "very_long_identifier_exceeding_sixty_four_characters_in_scan_omp_ident_test_1234567890((a))"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&empty_par_blk, test_fn, &handled));

    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_par_blk, NULL, "#pragma omp parallel if((0)) depend((in:x"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&empty_par_blk, test_fn, &handled));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_par_blk, NULL, "#pragma omp parallel if((1"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&empty_par_blk, test_fn, &handled));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_par_blk, NULL, "#pragma omp parallel private(a[0], b"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&empty_par_blk, test_fn, &handled));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_par_blk, NULL, "#pragma omp parallel reduction(+ a, b"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&empty_par_blk, test_fn, &handled));

    /* >32 clause vars + duplicate var update */
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(
        &rich_blk, NULL,
        "#pragma omp parallel firstprivate(v_fp) lastprivate(v_fp) "
        "private(c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15,c16,c17,c18,c19,"
        "c20,c21,c22,c23,c24,c25,c26,c27,c28,c29,c30,c31,c32,c33,c34)"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&rich_blk, test_fn, &handled));

    /* Task with `if(0)` and `if(1)` and pure `private(v_pr)` (ctx_field == NULL in caller loop) */
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp task if(0) private(v_pr) shared(v_sh)"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&rich_blk, NULL, "#pragma omp task"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&rich_blk, test_fn, &handled));

    /* Outlined region where body terminates block (`out_tail == NULL`) */
    g_terminate_in_lower_stmt = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&rich_blk, NULL, "#pragma omp parallel reduction(+:v_red_i)"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&rich_blk, test_fn, &handled));
    g_terminate_in_lower_stmt = FALSE;

    /* 5. Work-sharing `#pragma omp for`, `#pragma omp simd`, and `#pragma omp parallel for`! */
    a_variable iv0 = {}, iv1 = {}, iv_u64 = {}, iv_ul = {}, iv_ui = {}, iv_ubp = {}, iv_nullt = {}, iv_unbound = {};
    iv0.source_corresp.name = "iv0"; iv0.type = &tref_i32;
    iv1.source_corresp.name = "iv1"; iv1.type = &i64_t;
    iv_u64.source_corresp.name = "iv_u64"; iv_u64.type = &tref_u64;
    iv_ul.source_corresp.name = "iv_ul"; iv_ul.type = &u64_ul_t;
    iv_ui.source_corresp.name = "iv_ui"; iv_ui.type = &u64_ui_t;
    iv_ubp.source_corresp.name = "iv_ubp"; iv_ubp.type = &u64_bp_t;
    iv_nullt.source_corresp.name = NULL; iv_nullt.type = &f64_t;
    iv_unbound.source_corresp.name = "iv_unbound"; iv_unbound.type = &i32_t;

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &iv0, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_iv0")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &iv1, gcc_jit_function_new_local(test_fn, NULL, jit_i64, "lv_iv1")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &iv_u64, gcc_jit_function_new_local(test_fn, NULL, jit_u64, "lv_ivu64")));

    a_statement f0 = {}, f1 = {}, wrap_blk = {}, init0 = {}, init1 = {};
    a_for_loop fi0 = {}, fi1 = {};
    a_dynamic_init di0 = {}, di1 = {};
    an_expr_node lb0 = {}, ub0 = {}, ive0 = {}, cond0 = {}, inc0 = {}, ivinc0 = {}, step0 = {};
    an_expr_node lb1 = {}, ub1 = {}, ive1 = {}, cond1 = {}, inc1 = {}, ivinc1 = {};
    step0.kind = enk_constant; step0.type = &i32_t;

    GCC_GEN_BE_CHECK(init_canonical_for_ast(
        &f1, &fi1, &init1, &di1, &lb1, &ub1, &ive1, &cond1, &inc1, &ivinc1,
        &iv1, eok_ge, eok_pre_decr, NULL, &rich_blk));
    wrap_blk.kind = stmk_block;
    wrap_blk.variant.block.statements = &f1;
    GCC_GEN_BE_CHECK(init_canonical_for_ast(
        &f0, &fi0, &init0, &di0, &lb0, &ub0, &ive0, &cond0, &inc0, &ivinc0,
        &iv0, eok_lt, eok_add_assign, &step0, &wrap_blk));

    /* Schedules: static, dynamic, guided, runtime (signed & unsigned 64-bit), monotonic/nonmonotonic, chunk, nowait, collapse(2) */
    const char *for_pragmas[] = {
        "#pragma omp for collapse(2) schedule(static, 2) private(v_pr) firstprivate(v_fp) lastprivate(v_lp, iv0) shared(v_sh) reduction(+:v_red_i)",
        "#pragma omp for schedule(monotonic: dynamic, 4) nowait",
        "#pragma omp for schedule(nonmonotonic : guided, 0) nowait",
        "#pragma omp for schedule(dynamic) nowait",
        "#pragma omp for schedule(runtime) nowait",
        "#pragma omp for schedule(auto) nowait",
        "#pragma omp for schedule(unknown_sched",
        "#pragma omp simd linear(iv0)",
        "#pragma omp parallel for collapse(2) lastprivate(iv0) reduction(min:v_red_i)",
        "#pragma omp parallel for collapse(2)"
    };
    for (size_t pi = 0; pi < sizeof(for_pragmas) / sizeof(for_pragmas[0]); ++pi) {
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&f0, NULL, for_pragmas[pi]));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&f0, test_fn, &handled));
    }

    /* Unsigned 64-bit GOMP_loop_ull_*_start / next across runtime and non-runtime schedules, plus ul / ui / bit_precise / f64 / unbound IVs */
    a_variable *ull_ivs[] = { &iv_u64, &iv_ul, &iv_ui, &iv_ubp, &iv_nullt, &iv_unbound };
    for (size_t ui = 0; ui < sizeof(ull_ivs) / sizeof(ull_ivs[0]); ++ui) {
        a_statement fu = {}, initu = {};
        a_for_loop fiu = {};
        a_dynamic_init diu = {};
        an_expr_node lbu = {}, ubu = {}, iveu = {}, condu = {}, incu = {}, ivincu = {};
        GCC_GEN_BE_CHECK(init_canonical_for_ast(
            &fu, &fiu, &initu, &diu, &lbu, &ubu, &iveu, &condu, &incu, &ivincu,
            ull_ivs[ui], eok_le, eok_post_incr, NULL, NULL));
        const char *ull_p = (ui % 2 == 0)
            ? "#pragma omp for schedule(runtime) lastprivate(iv_unbound) nowait"
            : "#pragma omp for schedule(guided, 2) nowait";
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fu, NULL, ull_p));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fu, test_fn, &handled));
    }

    /* Additional canonical `for` forms: `stmk_block` init, `eok_assign` / `eok_bassign` init, `eok_cast` around cond,
       `-=`, `= iv + step`, `= iv - step`, `post_decr`, non-op inc, and `after_body == NULL`! */
    {
        a_statement fc = {}, init_blk = {}, init_expr = {};
        a_for_loop fic = {};
        an_expr_node lhs_v = {}, lb_c = {}, assign_e = {}, cast_c = {}, cond_c = {}, ive_c = {}, ub_c = {};
        an_expr_node inc_c = {}, iv_l = {}, rhs_add = {}, iv_r = {}, step_r = {};
        lhs_v.kind = enk_variable; lhs_v.type = &i32_t; lhs_v.variant.variable.ptr = &iv0;
        lb_c.kind = enk_constant; lb_c.type = &i32_t;
        lhs_v.next = &lb_c;
        assign_e.kind = enk_operation; assign_e.variant.operation.kind = eok_bassign; assign_e.variant.operation.operands = &lhs_v;
        init_expr.kind = stmk_expr; init_expr.expr = &assign_e;
        init_blk.kind = stmk_block; init_blk.variant.block.statements = &init_expr;

        ive_c.kind = enk_variable; ive_c.type = &i32_t; ive_c.variant.variable.ptr = &iv0;
        ub_c.kind = enk_constant; ub_c.type = &i32_t;
        ive_c.next = &ub_c;
        cond_c.kind = enk_operation; cond_c.variant.operation.kind = eok_gt; cond_c.variant.operation.operands = &ive_c;
        cast_c.kind = enk_operation; cast_c.variant.operation.kind = eok_cast; cast_c.variant.operation.operands = &cond_c;

        iv_r.kind = enk_variable; iv_r.type = &i32_t; iv_r.variant.variable.ptr = &iv0;
        step_r.kind = enk_constant; step_r.type = &i32_t;
        iv_r.next = &step_r;
        rhs_add.kind = enk_operation; rhs_add.variant.operation.kind = eok_subtract; rhs_add.variant.operation.operands = &iv_r;
        iv_l.kind = enk_variable; iv_l.type = &i32_t; iv_l.variant.variable.ptr = &iv0;
        iv_l.next = &rhs_add;
        inc_c.kind = enk_operation; inc_c.variant.operation.kind = eok_assign; inc_c.variant.operation.operands = &iv_l;

        fic.initialization = &init_blk;
        fic.increment = &inc_c;
        fc.kind = stmk_for;
        fc.expr = &cast_c;
        fc.variant.for_loop.extra_info = &fic;
        fc.variant.for_loop.statement = &bs_expr;

        g_terminate_in_lower_stmt = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for nowait"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));
        g_terminate_in_lower_stmt = FALSE;

        /* Test `= iv + step`, `-=`, `post_decr`, `bassign` with `eok_multiply`, `operands == NULL`, and `increment == NULL` */
        rhs_add.variant.operation.kind = eok_add;
        assign_e.variant.operation.kind = eok_assign;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for nowait"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));

        rhs_add.variant.operation.kind = eok_multiply;
        inc_c.variant.operation.kind = eok_bassign;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for nowait"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));

        rhs_add.variant.operation.kind = eok_add;
        rhs_add.variant.operation.operands = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for nowait"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));

        rhs_add.variant.operation.kind = eok_subtract;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for nowait"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));

        inc_c.variant.operation.kind = eok_subtract_assign;
        iv_l.next = &step_r;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for nowait"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));

        inc_c.variant.operation.kind = eok_post_decr;
        iv_l.next = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for nowait"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));

        fic.increment = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for nowait"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));

        /* Invalid canonical loop rejection paths (including collapse(10) hitting K > 8 clamp & parallel for inner break) */
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for collapse(10)"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp parallel for collapse(10)"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        /* Non-for, null extra_info, null expr, bad init_s, bad cond operands */
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_s, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&empty_s, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        a_statement bad_f = {};
        bad_f.kind = stmk_for;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);
        bad_f.variant.for_loop.extra_info = &fic;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        bad_f.expr = &e_const; /* non-enk_operation cond */
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        an_expr_node c_no_ops = {}, c_one_op = {};
        c_no_ops.kind = enk_operation;
        bad_f.expr = &c_no_ops;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);
        c_one_op.kind = enk_variable; c_one_op.variant.variable.ptr = &iv0;
        c_no_ops.variant.operation.operands = &c_one_op;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        /* Bad init variations: NULL init_s, stmk_init with NULL dynamic_init, dik_zero with NULL lb_expr,
           stmk_expr with NULL expr / non-op / non-assign / NULL operands / non-var lhs, empty & multi-stmt stmk_block,
           eok_cast with NULL operands, non-op inc, inc with NULL operands, assign inc with NULL rhs / non-op rhs,
           and NULL-unwrapped typeref IV! */
        bad_f.expr = &cond_c;
        fic.initialization = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        fic.initialization = &bs_init_null;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        a_dynamic_init di_var_nolb = {};
        di_var_nolb.kind = dik_zero;
        di_var_nolb.variable = &iv0;
        a_statement init_var_nolb = {};
        init_var_nolb.kind = stmk_init;
        init_var_nolb.variant.dynamic_init = &di_var_nolb;
        fic.initialization = &init_var_nolb;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        an_expr_node cast_null = {};
        cast_null.kind = enk_operation;
        cast_null.variant.operation.kind = eok_cast;
        cast_null.variant.operation.operands = NULL;
        bad_f.expr = &cast_null;
        fic.initialization = &init_expr;
        lhs_v.kind = enk_variable;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        bad_f.expr = &cond_c;
        init_expr.expr = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        init_expr.expr = &e_const;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        assign_e.variant.operation.kind = eok_add;
        init_expr.expr = &assign_e;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        assign_e.variant.operation.kind = eok_assign;
        assign_e.variant.operation.operands = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        assign_e.variant.operation.operands = &lhs_v;
        lhs_v.kind = enk_constant;
        fic.initialization = &init_expr;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);
        init_blk.variant.block.statements = NULL;
        fic.initialization = &init_blk;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bad_f, NULL, "#pragma omp for"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bad_f, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        /* Valid fc with non-op inc, inc with NULL operands, assign inc with NULL rhs / non-op rhs,
           collapse(2) with NULL body / empty block / 2-stmt block, parallel for simd, collapse(0),
           schedule(static , 2), num_threads( 2 ), reduction( + : a[0]), reduction(), reduction(+:a[0 */
        lhs_v.kind = enk_variable;
        fic.initialization = &init_expr;
        fic.increment = &e_const;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp parallel for simd collapse(0) num_threads( 2 ) schedule(static , 2)"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));

        inc_c.variant.operation.kind = eok_post_incr;
        inc_c.variant.operation.operands = NULL;
        fic.increment = &inc_c;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for reduction( + : a[0]) reduction() reduction(+:a[0"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));

        inc_c.variant.operation.kind = eok_assign;
        inc_c.variant.operation.operands = &iv_l;
        iv_l.next = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for nowait"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));

        /* Stage-2 OpenMP block-wrapped for loops (stmk_decl/stmk_empty + stmk_init + stmk_for) & dik_constant lower bound */
        {
            a_constant lb_c0 = {};
            lb_c0.kind = ck_integer;
            a_dynamic_init di_const_lb = {};
            di_const_lb.kind = dik_constant;
            di_const_lb.variable = &iv0;
            di_const_lb.variant.constant.ptr = &lb_c0;

            /* 1) Block-wrapped init_s: stmk_decl -> stmk_empty -> stmk_init(dik_constant), and trailing stmt in outer block */
            a_statement in_decl = {}, in_emp = {}, in_cinit = {}, in_blk_wrap = {};
            in_decl.kind = stmk_decl; in_decl.next = &in_emp;
            in_emp.kind = stmk_empty; in_emp.next = &in_cinit;
            in_cinit.kind = stmk_init; in_cinit.variant.dynamic_init = &di_const_lb;
            in_blk_wrap.kind = stmk_block; in_blk_wrap.variant.block.statements = &in_decl;

            fic.initialization = &in_blk_wrap;
            GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for"));
            GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));

            /* Also cover dik_constant lb_const OOM / constant failure at L1226! */
            for (int b = 0; b < 35; ++b) {
                GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for"));
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t oe = gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled);
                g_gcc_be_oom_countdown = -1;
                if (oe == GCC_GEN_BE_SUCCESS) break;
            }

            /* 2) Outer block wrapping stmk_decl -> stmk_empty -> stmk_init -> stmk_for (with fic.initialization == NULL -> ext_init_s!)
                  plus scan_omp_pragmas_in_stmt_tree coverage (fs->position.seq != 0 and fs->position.seq == 0) */
            fic.initialization = NULL;
            fc.position.seq = 502;
            a_statement out_decl = {}, out_emp = {}, out_init = {}, out_for = {}, out_trail = {}, out_blk = {};
            out_decl.kind = stmk_decl; out_decl.next = &out_emp;
            out_emp.kind = stmk_empty; out_emp.next = &out_init;
            out_init.kind = stmk_init; out_init.variant.dynamic_init = &di_const_lb; out_init.position.seq = 501; out_init.next = &out_for;
            out_for = fc; out_for.position.seq = 502; out_for.next = NULL;
            out_blk.kind = stmk_block; out_blk.variant.block.statements = &out_decl;

            a_scope omp_sc = {};
            omp_sc.assoc_block = &out_blk;
            GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&omp_sc));
            out_init.position.seq = 0; /* fs->position.seq == 0 -> uses fs->next->position.seq (360:27 False)! */
            GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&omp_sc));

            /* Lower out_blk directly (hits 1045:22 T, 1048:18..82 T!) */
            GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&out_blk, NULL, "#pragma omp for"));
            GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&out_blk, test_fn, &handled));

            /* Edge cases on outer block: stmk_init with s->next == NULL, s->next->kind != stmk_for, s->next->next != NULL, and stmk_for with s->next != NULL */
            out_init.next = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&omp_sc));
            GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&out_blk, NULL, "#pragma omp for"));
            ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&out_blk, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

            out_trail.kind = stmk_empty;
            out_init.next = &out_trail; /* s->next->kind != stmk_for */
            GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&omp_sc));
            GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&out_blk, NULL, "#pragma omp for"));
            ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&out_blk, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

            out_init.next = &out_for;
            out_for.next = &out_trail; /* s->next->next != NULL */
            GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&omp_sc));
            GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&out_blk, NULL, "#pragma omp for"));
            ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&out_blk, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

            /* Outer block with stmk_for followed by trailing statement (1053:41 False) and empty outer block (354:56 F, 356:20 F, 359:17 F, 1069:16 F) */
            out_emp.next = &out_for; /* out_for.next == &out_trail -> 1053:41 False! */
            GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&out_blk, NULL, "#pragma omp for"));
            ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&out_blk, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

            out_emp.next = NULL; /* while(fs) reaches fs == NULL -> 356:20 F, 359:17 F! */
            GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&omp_sc));
            out_blk.variant.block.statements = NULL; /* 354:56 F! */
            GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(&omp_sc));

            in_emp.next = NULL; /* 1069:16 F! */
            fic.initialization = &in_blk_wrap;
            GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for"));
            ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

            /* dik_constant with NULL iv_var -> out_lvl->lb_const != NULL while iv_var == NULL (covers 1096:9 T & cond with lb_const != NULL!) */
            in_emp.next = &in_cinit;
            di_const_lb.variable = &iv0;
            fc.expr = &e_const; /* cond->kind != enk_operation while lb_const != NULL -> 1096:30 T, 1096:51 F! */
            GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for"));
            ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);
            fc.expr = &cond_c;
            fic.initialization = &init_expr;
        }

        iv_l.next = &e_const;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for nowait"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled));

        /* collapse(2) with NULL body, empty stmk_block body, and 2-stmt stmk_block body */
        fc.variant.for_loop.statement = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for collapse(2)"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        a_statement empty_b = {}, two_b = {}, two_s1 = {}, two_s2 = {};
        empty_b.kind = stmk_block; empty_b.variant.block.statements = NULL;
        fc.variant.for_loop.statement = &empty_b;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for collapse(2)"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);

        two_s1.kind = stmk_empty; two_s2.kind = stmk_empty; two_s1.next = &two_s2;
        two_b.kind = stmk_block; two_b.variant.block.statements = &two_s1;
        fc.variant.for_loop.statement = &two_b;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fc, NULL, "#pragma omp for collapse(2)"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&fc, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);
    }

    /* 6. Synchronization & Atomic constructs (`critical`, `critical(name)`, `single`, `master`, `masked`, `atomic`) */
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp critical"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp critical ( my_lock ) , "));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled));
    /* Second call hits cached_lock != NULL; unclosed `critical(my_lock` hits `*p != ')'`! */
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp critical(my_lock"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled));

    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp single"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp single nowait"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled));

    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp master"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp masked filter(2)"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled));

    /* Terminated body inside critical(name), single, and master */
    g_terminate_in_lower_stmt = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp critical(my_lock)"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled));
    gcc_jit_block *res_blk = gcc_jit_function_new_block(test_fn, "omp_resume_1");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(res_blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp single nowait"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp master"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled));
    g_terminate_in_lower_stmt = FALSE;

    /* `#pragma omp atomic` (`update`, `write`, `read`, `capture`, 1/2/4/8/16B, NULL type, unary & binary RMW) */
    a_variable av1 = {}, av2 = {}, av4 = {}, av8 = {}, av16 = {}, av_nt = {};
    av1.type = &i8_t; av2.type = &i16_t; av4.type = &tref_i32; av8.type = &i64_t; av16.type = &big16_t; av_nt.type = NULL;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &av1, gcc_jit_function_new_local(test_fn, NULL, jit_i8, "av1")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &av2, gcc_jit_function_new_local(test_fn, NULL, jit_i16, "av2")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &av4, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "av4")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &av8, gcc_jit_function_new_local(test_fn, NULL, jit_i64, "av8")));

    const an_expr_operator_kind rmw_ops[] = {
        eok_add_assign, eok_subtract_assign, eok_and_assign, eok_or_assign, eok_xor_assign,
        eok_pre_incr, eok_post_incr, eok_pre_decr, eok_post_decr
    };
    a_variable *atom_vars[] = { &av1, &av2, &av4, &av8, &av16, &av_nt };
    for (size_t ai = 0; ai < sizeof(rmw_ops) / sizeof(rmw_ops[0]); ++ai) {
        a_variable *av = atom_vars[ai % 6];
        an_expr_node a_lhs = {}, a_rhs = {}, a_op = {};
        a_lhs.kind = enk_variable; a_lhs.type = av->type; a_lhs.variant.variable.ptr = av;
        a_rhs.kind = enk_constant; a_rhs.type = av->type;
        if (ai < 5) a_lhs.next = &a_rhs;
        a_op.kind = enk_operation; a_op.variant.operation.kind = rmw_ops[ai]; a_op.variant.operation.operands = &a_lhs;
        a_statement a_st = {};
        a_st.kind = stmk_expr; a_st.expr = &a_op;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&a_st, NULL, "#pragma omp atomic update"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&a_st, test_fn, &handled));
    }
    /* Atomic write (1/2/4/8/16B & NULL type, `eok_assign` and `eok_bassign`), atomic read, atomic capture, and non-expr fallback */
    for (size_t wi = 0; wi < 6; ++wi) {
        a_variable *av = atom_vars[wi];
        an_expr_node a_lhs = {}, a_rhs = {}, a_op = {};
        a_lhs.kind = enk_variable; a_lhs.type = av->type; a_lhs.variant.variable.ptr = av;
        a_rhs.kind = enk_constant; a_rhs.type = av->type;
        a_lhs.next = &a_rhs;
        a_op.kind = enk_operation;
        a_op.variant.operation.kind = (wi % 2 == 0) ? eok_assign : eok_bassign;
        a_op.variant.operation.operands = &a_lhs;
        a_statement a_st = {};
        a_st.kind = stmk_expr; a_st.expr = &a_op;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&a_st, NULL, "#pragma omp atomic write"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&a_st, test_fn, &handled));
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&rich_blk, NULL, "#pragma omp atomic read"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&rich_blk, test_fn, &handled));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp atomic capture seq_cst"));
    GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled));
    {
        a_statement a_null_e = {}, a_op_st = {};
        an_expr_node a_op = {}, a1_e = {}, a2_e = {};
        a_null_e.kind = stmk_expr; a_null_e.expr = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&a_null_e, NULL, "#pragma omp atomic update"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&a_null_e, test_fn, &handled));

        a_op.kind = enk_operation; a_op.variant.operation.kind = eok_multiply; a_op.variant.operation.operands = NULL;
        a_op_st.kind = stmk_expr; a_op_st.expr = &a_op;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&a_op_st, NULL, "#pragma omp atomic update"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&a_op_st, test_fn, &handled));
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&a_op_st, NULL, "#pragma omp atomic write"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&a_op_st, test_fn, &handled));

        a1_e.kind = enk_variable; a1_e.type = &i32_t; a1_e.variant.variable.ptr = &av4;
        a_op.variant.operation.operands = &a1_e;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&a_op_st, NULL, "#pragma omp atomic update"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&a_op_st, test_fn, &handled));
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&a_op_st, NULL, "#pragma omp atomic write"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&a_op_st, test_fn, &handled));

        a2_e.kind = enk_constant; a2_e.type = &i32_t;
        a1_e.next = &a2_e;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&a_op_st, NULL, "#pragma omp atomic write"));
        GCC_GEN_BE_CHECK(gcc_gen_be_try_lower_omp_statement(&a_op_st, test_fn, &handled));
    }

    /* Reduction & canonical IV with tk_typeref -> NULL (`tref_null_t`) */
    {
        a_variable v_rnt = {}, iv_rnt = {};
        v_rnt.source_corresp.name = "v_rnt"; v_rnt.type = &tref_null_t;
        iv_rnt.source_corresp.name = "iv_rnt"; iv_rnt.type = &tref_null_t;
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_rnt, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_rnt")));
        an_expr_node ernt = {};
        ernt.kind = enk_variable; ernt.type = &i32_t; ernt.variant.variable.ptr = &v_rnt;
        a_statement srnt = {};
        srnt.kind = stmk_expr; srnt.expr = &ernt;
        /* Note: v_rnt->type lowers to void (invalid local), so test make_omp_reduction_identity via tref_null_t on valid local? */
        tref_null_t.variant.typeref.type = NULL;
        /* When lowering v_rnt->type in gcc_gen_be_lower_type(&tref_null_t), it produces void, so instead set v_rnt.type = NULL?
           Wait: gcc_gen_be_lower_type(NULL) returns out_type = NULL! Instead test iv_rnt where gcc_gen_be_lower_type(&tref_null_t) returns void_t: */
        a_statement fn_st = {}, in_st = {};
        a_for_loop fi_st = {};
        a_dynamic_init di_st = {};
        an_expr_node lb_s = {}, ub_s = {}, iv_s = {}, cd_s = {}, ic_s = {}, ivi_s = {};
        GCC_GEN_BE_CHECK(init_canonical_for_ast(
            &fn_st, &fi_st, &in_st, &di_st, &lb_s, &ub_s, &iv_s, &cd_s, &ic_s, &ivi_s,
            &iv_rnt, eok_lt, eok_post_incr, NULL, NULL));
        /* extract_canonical_omp_for_level runs inside parallel for loop_ivs scan before lower_omp_for_workshare! */
        fi_st.initialization = &in_st;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fn_st, NULL, "#pragma omp parallel for collapse(2)"));
        ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&fn_st, test_fn, &handled), GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED);
    }

    /* 7. Invalid state (`ctx == NULL` or `cblock == NULL`) across void call, workshare, outlined, and sync constructs */
    gcc_jit_block *saved_cb = gcc_jit_function_new_block(test_fn, "omp_sec7_blk");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_cb));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_s, NULL, "#pragma omp barrier"));
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&empty_s, test_fn, &handled), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&f0, NULL, "#pragma omp for"));
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&f0, test_fn, &handled), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&rich_blk, NULL, "#pragma omp parallel"));
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&rich_blk, test_fn, &handled), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp critical"));
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled), GCC_GEN_BE_ERROR_INVALID_STATE);

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&empty_s, NULL, "#pragma omp barrier"));
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&empty_s, test_fn, &handled), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&f0, NULL, "#pragma omp for"));
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&f0, test_fn, &handled), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&rich_blk, NULL, "#pragma omp parallel"));
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&rich_blk, test_fn, &handled), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&bs_expr, NULL, "#pragma omp critical"));
    ASSERT_EQ(gcc_gen_be_try_lower_omp_statement(&bs_expr, test_fn, &handled), GCC_GEN_BE_ERROR_INVALID_STATE);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_cb));

    /* 8. Deterministic OOM sweeps using compact ASTs so every single helper and step is reached! */
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_VAR));
    an_expr_node ce1 = {}, ce2 = {}, ce3 = {}, ce4 = {}, ce5 = {}, ce6 = {};
    ce1.kind = enk_variable; ce1.type = &tref_i32; ce1.variant.variable.ptr = &v_sh; ce1.next = &ce2;
    ce2.kind = enk_variable; ce2.type = &i32_t; ce2.variant.variable.ptr = &v_pr; ce2.next = &ce3;
    ce3.kind = enk_variable; ce3.type = &i32_t; ce3.variant.variable.ptr = &v_fp; ce3.next = &ce4;
    ce4.kind = enk_variable; ce4.type = &i32_t; ce4.variant.variable.ptr = &v_lp; ce4.next = &ce5;
    ce5.kind = enk_variable; ce5.type = &tref_i32; ce5.variant.variable.ptr = &v_red_i; ce5.next = &ce6;
    ce6.kind = enk_variable; ce6.type = &f64_t; ce6.variant.variable.ptr = &v_red_f;
    a_statement compact_expr_s = {}, compact_blk = {};
    compact_expr_s.kind = stmk_expr; compact_expr_s.expr = &ce1;
    compact_blk.kind = stmk_block; compact_blk.variant.block.statements = &compact_expr_s;
    f1.variant.for_loop.statement = &compact_blk;

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_sh, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_sh2")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pr, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_pr2")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_fp, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_fp2")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_lp, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_lp2")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_red_i, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_ri2")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_red_f, gcc_jit_function_new_local(test_fn, NULL, jit_f64, "lv_rf2")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &iv0, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_iv02")));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &iv1, gcc_jit_function_new_local(test_fn, NULL, jit_i64, "lv_iv12")));

    const char *oom_pragmas[] = {
        "#pragma omp parallel shared(v_sh) private(v_pr) firstprivate(v_fp) lastprivate(v_lp) reduction(+:v_red_i) reduction(min:v_red_f)",
        "#pragma omp task firstprivate(v_fp)",
        "#pragma omp for collapse(2) schedule(static, 2) private(v_pr) firstprivate(v_fp) lastprivate(v_lp, iv0) reduction(+:v_red_i)",
        "#pragma omp for schedule(runtime) nowait",
        "#pragma omp critical(oom_lock)",
        "#pragma omp critical",
        "#pragma omp single",
        "#pragma omp master",
        "#pragma omp taskwait",
        "#pragma omp for schedule(monotonic: dynamic, 2) nowait"
    };
    for (size_t oi = 0; oi < sizeof(oom_pragmas) / sizeof(oom_pragmas[0]); ++oi) {
        a_statement_ptr target = (oi == 2 || oi == 3 || oi == 9) ? &f0 : ((oi == 1) ? &compact_expr_s : &compact_blk);
        for (int b = 0; b < 400; ++b) {
            saved_cb = gcc_jit_function_new_block(test_fn, "omp_oom_iter_blk");
            free(gcc_gen_be_singleton_state.break_stack);
            gcc_gen_be_singleton_state.break_stack = NULL;
            gcc_gen_be_singleton_state.break_stack_size = 0;
            gcc_gen_be_singleton_state.break_stack_capacity = 0;
            free(gcc_gen_be_singleton_state.continue_stack);
            gcc_gen_be_singleton_state.continue_stack = NULL;
            gcc_gen_be_singleton_state.continue_stack_size = 0;
            gcc_gen_be_singleton_state.continue_stack_capacity = 0;
            GCC_GEN_BE_CHECK(cache_clear_all());
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_sh, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_sh2")));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_pr, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_pr2")));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_fp, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_fp2")));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_lp, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_lp2")));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_red_i, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_ri2")));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &v_red_f, gcc_jit_function_new_local(test_fn, NULL, jit_f64, "lv_rf2")));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &iv0, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "lv_iv02")));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &iv1, gcc_jit_function_new_local(test_fn, NULL, jit_i64, "lv_iv12")));
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_cb));
            GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(target, NULL, oom_pragmas[oi]));
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t e = gcc_gen_be_try_lower_omp_statement(target, test_fn, &handled);
            g_gcc_be_oom_countdown = -1;
            if (e == GCC_GEN_BE_SUCCESS) break;
        }
    }

    /* Also run OOM sweep on rich_blk `collect_vars_in_stmt_list` (`stmk_if`, `stmk_while`, `stmk_for`, `stmk_init`) */
    di_expr.variant.expression = &e_const;
    a_statement rich_then = {}, rich_else = {}, fex_init = {};
    rich_then.kind = stmk_empty; rich_else.kind = stmk_empty; fex_init.kind = stmk_empty;
    dummy_fex.initialization = &fex_init;
    dummy_fex.increment = &e_const;
    bs_if.variant.if_stmt.then_statement = &rich_then;
    bs_if.variant.if_stmt.else_statement = &rich_else;
    for (int b = 0; b < 200; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_STRING));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_cb));
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&rich_blk, NULL, "#pragma omp parallel"));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_try_lower_omp_statement(&rich_blk, test_fn, &handled);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* OOM sweeps on unsigned 64-bit runtime/static schedules, atomic update/write, register_in_scope, and flush_trailing */
    {
        a_statement fu = {}, initu = {};
        a_for_loop fiu = {};
        a_dynamic_init diu = {};
        an_expr_node lbu = {}, ubu = {}, iveu = {}, condu = {}, incu = {}, ivincu = {};
        GCC_GEN_BE_CHECK(init_canonical_for_ast(
            &fu, &fiu, &initu, &diu, &lbu, &ubu, &iveu, &condu, &incu, &ivincu,
            &iv_u64, eok_lt, eok_post_incr, NULL, NULL));
        for (int m = 0; m < 2; ++m) {
            const char *up = (m == 0) ? "#pragma omp for schedule(runtime)" : "#pragma omp for schedule(static, 2)";
            for (int b = 0; b < 200; ++b) {
                GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_STRING));
                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_cb));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &iv_u64, gcc_jit_function_new_local(test_fn, NULL, jit_u64, "lv_u64r")));
                GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&fu, NULL, up));
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t e = gcc_gen_be_try_lower_omp_statement(&fu, test_fn, &handled);
                g_gcc_be_oom_countdown = -1;
                if (e == GCC_GEN_BE_SUCCESS) break;
            }
        }

        an_expr_node a_lhs = {}, a_rhs = {}, a_op = {};
        a_lhs.kind = enk_variable; a_lhs.type = &i32_t; a_lhs.variant.variable.ptr = &av4;
        a_rhs.kind = enk_constant; a_rhs.type = &i32_t;
        a_lhs.next = &a_rhs;
        a_op.kind = enk_operation; a_op.variant.operation.kind = eok_add_assign; a_op.variant.operation.operands = &a_lhs;
        a_statement a_st = {};
        a_st.kind = stmk_expr; a_st.expr = &a_op;
        for (int m = 0; m < 3; ++m) {
            a_op.variant.operation.kind = (m == 1) ? eok_assign : eok_add_assign;
            const char *ap = (m == 0) ? "#pragma omp atomic update" : ((m == 1) ? "#pragma omp atomic write" : "#pragma omp atomic capture");
            for (int b = 0; b < 100; ++b) {
                GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_STRING));
                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_cb));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &av4, gcc_jit_function_new_local(test_fn, NULL, jit_i32, "av4r")));
                GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&a_st, NULL, ap));
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t e = gcc_gen_be_try_lower_omp_statement(&a_st, test_fn, &handled);
                g_gcc_be_oom_countdown = -1;
                if (e == GCC_GEN_BE_SUCCESS) break;
            }
        }

        for (int b = 0; b < 50; ++b) {
            GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_STRING));
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_cb));
            GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(NULL, &scope_a, "#pragma omp barrier"));
            GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(NULL, &scope_a, "#pragma omp taskwait"));
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t e = gcc_gen_be_flush_trailing_omp_pragmas(&scope_a, test_fn);
            g_gcc_be_oom_countdown = -1;
            if (e == GCC_GEN_BE_SUCCESS) break;
        }

        FILE *fp2 = std::fopen(omp_src, "wb");
        if (fp2) {
            std::fprintf(fp2, "int x;\n#pragma omp barrier\nx++;\n#pragma omp barrier\nx++;\n");
            std::fclose(fp2);
        }
        g_mock_seq_file_path = omp_src;
        g_mock_short_fn = NULL;
        s_line5.position.seq = 3;
        s_then.position.seq = 15;
        s_else.position.seq = 16;
        child_sc.pragmas = &pr_trail;
        reg_sc.assoc_block = &s_seq0;
        for (int b = 0; b < 100; ++b) {
            gcc_gen_be_singleton_state.omp_pragma_count = 0;
            gcc_gen_be_singleton_state.openmp_enabled = TRUE;
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t e = gcc_gen_be_register_omp_pragmas_in_scope(&reg_sc);
            g_gcc_be_oom_countdown = -1;
            if (e == GCC_GEN_BE_SUCCESS) break;
        }
        g_mock_seq_file_path = NULL;
        std::remove(omp_src);
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests `_BitInt(256)` multi-word struct lowering, `_BitInt` error bounds, coro layout, atomics, SIMD, and OpenMP.
 * @param[out] out_ok Pointer receiving `1` on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t test_coro_atomic_simd_omp_api(int *out_ok) noexcept {
    *out_ok = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_init());
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

    /* 1. _BitInt(256) multi-word struct & _BitInt(1024) unsupported error */
    an_integer_type_supplement bp256_sup = {};
    bp256_sup.bit_width = 256;
    a_type bp256_tp = {};
    bp256_tp.kind = tk_integer;
    bp256_tp.size = 32;
    bp256_tp.variant.integer.int_kind = ik_bit_precise;
    bp256_tp.variant.integer.extra_info = &bp256_sup;
    gcc_jit_type *bp256_jt = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&bp256_tp, &bp256_jt));
    ASSERT_TRUE(bp256_jt != NULL && gcc_jit_type_is_struct(bp256_jt) != NULL);

    an_integer_type_supplement bp_bad_sup = {};
    bp_bad_sup.bit_width = 1024;
    a_type bp_bad_tp = {};
    bp_bad_tp.kind = tk_integer;
    bp_bad_tp.size = 128;
    bp_bad_tp.variant.integer.int_kind = ik_bit_precise;
    bp_bad_tp.variant.integer.extra_info = &bp_bad_sup;
    gcc_jit_type *bp_bad_jt = NULL;
    ASSERT_EQ(gcc_gen_be_lower_type(&bp_bad_tp, &bp_bad_jt), GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED);

    /* 2. Exhaustive Coroutine frame layout calculation */
    GCC_GEN_BE_CHECK(test_exhaustive_coro_layout(ctx));

    /* 3. Exhaustive SIMD splat/build/opcode & error branches */
    gcc_jit_function *test_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "unit_fn", 0, NULL, 0);
    gcc_jit_block *blk = gcc_jit_function_new_block(test_fn, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));
    GCC_GEN_BE_CHECK(test_exhaustive_simd_api(ctx, test_fn, blk));
    GCC_GEN_BE_CHECK(test_exhaustive_atomic_api(ctx, test_fn));

    /* 4. Exhaustive OpenMP directive binding, lowering, source scanning, and OOM sweeps */
    GCC_GEN_BE_CHECK(test_exhaustive_omp_api(ctx, test_fn));

    /* Deterministic OOM injection test on gcc_gen_be_alloc */
    g_gcc_be_oom_countdown = 0;
    void *oom_ptr = NULL;
    ASSERT_EQ(gcc_gen_be_alloc(64, &oom_ptr), GCC_GEN_BE_ERROR_OOM);
    g_gcc_be_oom_countdown = -1;

    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

} // namespace

/**
 * @brief Entry point for `test_gcc_be_omp_atomic_coro_unit.cpp`.
 * @details Runs `_Decimal128`/`_Sat _Accum` JIT tests and coroutine/atomic/SIMD/OpenMP unit tests.
 * @return `0` on success, `1` on failure.
 */
int main() noexcept {
    int ok1 = 0;
    int ok2 = 0;
    if (test_decimal128_and_sat_accum_jit(&ok1) != GCC_GEN_BE_SUCCESS || !ok1) return 1;
    if (test_coro_atomic_simd_omp_api(&ok2) != GCC_GEN_BE_SUCCESS || !ok2) return 1;
    return 0;
}
