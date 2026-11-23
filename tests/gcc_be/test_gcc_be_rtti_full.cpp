/**
 * @file test_gcc_be_rtti_full.cpp
 * @brief Comprehensive unit and OOM fault-injection tests for Itanium C++ ABI RTTI, VTables, VTC, VTT, and Thunks.
 * @details Exercises mangling of nested namespaces/classes/functions/blocks, templates, packs,
 * function/array/vector/complex/float/bit-precise/ptr-to-member/lambda types, CV-qualifiers,
 * substitutions (`S_`, `S0_`, `S10_`), secondary vtables, Construction Virtual Tables (`_ZTC`),
 * VTTs (`_ZTT`), `_ZTv`/`_ZTh`/`_ZTch`/`_ZTcv` thunks, `dynamic_cast`, `typeid`, `_ZTI`/`_ZTS`
 * descriptors (LP64 and `-m32`), and deterministic OOM error percolation across all paths.
 * @ingroup gcc_gen_be_tests
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
#include <cstdlib>
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

/** @brief Configurable integer constant return value for `value_of_integer_constant`. */
static a_host_large_integer g_mock_int_const_val = 0;
/** @brief Configurable invisible-reference return flag for return/parameter types. */
static a_boolean g_mock_invis_ref = FALSE;
/** @brief Countdown until `gcc_gen_be_is_passed_by_invisible_reference` fails (`< 0` = disabled). */
static int g_mock_invis_ref_fail_countdown = -1;
/** @brief When TRUE, `gcc_gen_be_lower_type` returns `GCC_JIT_TYPE_VOID` for `tk_void`. */
static a_boolean g_mock_lower_void_for_tk_void = FALSE;
/** @brief Countdown until `gcc_gen_be_lower_type` fails (`< 0` = disabled). */
static int g_mock_lower_type_fail_countdown = -1;
/** @brief When TRUE, `gcc_gen_be_lower_function_decl` sets `*out_func = NULL`. */
static a_boolean g_mock_lower_fn_null = FALSE;
/** @brief When TRUE, `gcc_gen_be_lower_function_decl` returns `GCC_GEN_BE_ERROR_OOM`. */
static a_boolean g_mock_lower_fn_fail = FALSE;
/** @brief Optional pre-created lvalue returned by `gcc_gen_be_lower_expr_lvalue`. */
static gcc_jit_lvalue *g_mock_expr_lval = NULL;
/** @brief When TRUE, `gcc_gen_be_lower_expr_lvalue` returns `GCC_GEN_BE_ERROR_OOM`. */
static a_boolean g_mock_expr_lval_fail = FALSE;
/** @brief When TRUE, `gcc_gen_be_lower_expr_rvalue` returns `GCC_GEN_BE_ERROR_OOM`. */
static a_boolean g_mock_expr_rval_fail = FALSE;
/** @brief Optional pre-created global lvalue returned by `gcc_gen_be_lower_global_variable_decl`. */
static gcc_jit_lvalue *g_mock_global_var_lval = NULL;
/** @brief Counter used to generate unique imported mock function names. */
static int g_mock_fn_seq = 0;
/** @brief When TRUE, `gcc_gen_be_lower_function_decl` registers an `abi_fn` entry in `GCC_GEN_BE_CACHE_FUNC`. */
static a_boolean g_mock_bind_abi_fn_on_lower = FALSE;

BEGIN_EDG_NAMESPACE
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
 * @details Sets `*err` to `FALSE` and returns `g_mock_int_const_val`.
 * @param[in] cp The frontend constant pointer.
 * @param[out] err Pointer to receive overflow flag.
 * @return Extracted signed integer value.
 */
a_host_large_integer value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    (void)cp;
    if (err) *err = FALSE;
    return g_mock_int_const_val;
}

/**
 * @brief Stub for invisible reference check in RTTI unit tests.
 * @details Supports deterministic failure countdown and configurable `g_mock_invis_ref` result.
 * @param[in] type The frontend type pointer.
 * @param[out] out_is_ref Pointer to receive invisible reference flag.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an injected error.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (!out_is_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_invis_ref_fail_countdown >= 0) {
        if (g_mock_invis_ref_fail_countdown == 0) {
            g_mock_invis_ref_fail_countdown = -1;
            return GCC_GEN_BE_ERROR_OOM;
        }
        g_mock_invis_ref_fail_countdown--;
    }
    *out_is_ref = g_mock_invis_ref;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for type lowering in RTTI unit tests.
 * @details Lowers types to `void` (when `g_mock_lower_void_for_tk_void` and `tk_void`) or `void*`.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_type Pointer to receive the lowered `gcc_jit_type`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an injected error.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_lower_type_fail_countdown >= 0) {
        if (g_mock_lower_type_fail_countdown == 0) {
            g_mock_lower_type_fail_countdown = -1;
            return GCC_GEN_BE_ERROR_OOM;
        }
        g_mock_lower_type_fail_countdown--;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    a_type_ptr cur = tp;
    while (cur && cur->kind == tk_typeref) cur = cur->variant.typeref.type;
    if (g_mock_lower_void_for_tk_void && cur && cur->kind == tk_void) {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    } else {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function declaration lowering in RTTI unit tests.
 * @details Creates an imported function matching the parameter count of `rout` (including sret).
 * @param[in] rout The frontend routine pointer.
 * @param[out] out_func Pointer to receive the lowered function.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an injected error.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_lower_fn_fail) return GCC_GEN_BE_ERROR_OOM;
    if (g_mock_lower_fn_null) {
        *out_func = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *vp = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *ret_jt = vp;
    if (g_mock_invis_ref) {
        ret_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    } else if (g_mock_lower_void_for_tk_void && rout && rout->type && rout->type->kind == tk_routine) {
        a_type_ptr rtp = rout->type->variant.routine.return_type;
        while (rtp && rtp->kind == tk_typeref) rtp = rtp->variant.typeref.type;
        if (rtp && rtp->kind == tk_void) {
            ret_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        }
    }
    int nparams = g_mock_invis_ref ? 1 : 0;
    int is_var = 0;
    if (rout && rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info) {
        is_var = rout->type->variant.routine.extra_info->has_ellipsis ? 1 : 0;
        for (a_param_type_ptr p = rout->type->variant.routine.extra_info->param_type_list; p != NULL; p = p->next) {
            nparams++;
        }
    } else if (nparams == 0) {
        nparams = 1;
    }
    gcc_jit_param *plist[8];
    for (int i = 0; i < nparams && i < 8; ++i) {
        gcc_jit_type *pt = (g_mock_invis_ref && i >= 1) ? gcc_jit_type_get_pointer(vp) : vp;
        char pn[16];
        std::snprintf(pn, sizeof(pn), "mp%d", i);
        plist[i] = gcc_jit_context_new_param(ctx, NULL, pt, pn);
    }
    char fname[64];
    std::snprintf(fname, sizeof(fname), "_ZN7Derived4funcEv_%d", g_mock_fn_seq++);
    *out_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_jt, fname, nparams, plist, is_var);
    if (g_mock_bind_abi_fn_on_lower && *out_func) {
        gcc_jit_param *aplist[8];
        for (int i = 0; i < nparams && i < 8; ++i) {
            gcc_jit_type *pt = (g_mock_invis_ref && i >= 1) ? gcc_jit_type_get_pointer(vp) : vp;
            char apn[16];
            std::snprintf(apn, sizeof(apn), "ap%d", i);
            aplist[i] = gcc_jit_context_new_param(ctx, NULL, pt, apn);
        }
        char aname[64];
        std::snprintf(aname, sizeof(aname), "_ZN7Derived4funcEv_abi_%d", g_mock_fn_seq++);
        gcc_jit_function *afn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_jt, aname, nparams, aplist, is_var);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, *out_func, afn));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression rvalue lowering in RTTI unit tests.
 * @details Returns a null `void*` JIT rvalue or fails when `g_mock_expr_rval_fail` is set.
 * @param[in] expr The frontend expression pointer.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an injected error.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_expr_rval_fail) return GCC_GEN_BE_ERROR_OOM;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    *out_rval = gcc_jit_context_null(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR));
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression lvalue lowering in RTTI unit tests.
 * @details Returns `g_mock_expr_lval` or fails when `g_mock_expr_lval_fail` is set.
 * @param[in] expr The frontend expression pointer.
 * @param[out] out_lval Pointer to receive the lowered lvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an injected error.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_expr_lval_fail) return GCC_GEN_BE_ERROR_OOM;
    *out_lval = g_mock_expr_lval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for global variable declaration lowering in RTTI unit tests.
 * @details Returns `g_mock_global_var_lval` to exercise `tp->typeinfo_var` lowering paths.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered global variable lvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an OOM error.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_lval = g_mock_global_var_lval;
    return GCC_GEN_BE_SUCCESS;
}
END_EDG_NAMESPACE

/**
 * @brief Sweeps deterministic OOM budgets (`0..max_budget`) over a callable test operation.
 * @details Clears backend caches before each iteration and ensures every budget either returns
 * `GCC_GEN_BE_ERROR_OOM` or completes with `GCC_GEN_BE_SUCCESS`.
 * @tparam Fn Callable returning `gcc_gen_be_error_t` with `noexcept`.
 * @param[in] max_budget Maximum OOM budget to sweep.
 * @param[in] fn Operation under test.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INTERNAL` if an unexpected error occurs.
 */
template <typename Fn>
static gcc_gen_be_error_t sweep_oom_budgets(int max_budget, Fn fn) GCC_GEN_BE_NOEXCEPT {
    int limit = max_budget > 5000 ? max_budget : 5000;
    for (int b = 0; b <= limit; ++b) {
        g_gcc_be_oom_countdown = -1;
        GCC_GEN_BE_CHECK(cache_clear_all());
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t err = fn();
        g_gcc_be_oom_countdown = -1;
        if (err == GCC_GEN_BE_SUCCESS) {
            return GCC_GEN_BE_SUCCESS;
        }
        if (err != GCC_GEN_BE_ERROR_OOM) {
            std::fprintf(stderr, "sweep_oom_budgets unexpected err=%d at b=%d\n", (int)err, b);
            return GCC_GEN_BE_ERROR_INTERNAL;
        }
    }
    std::fprintf(stderr, "sweep_oom_budgets exceeded limit=%d\n", limit);
    return GCC_GEN_BE_ERROR_INTERNAL;
}

/**
 * @brief Exercises all Itanium C++ ABI type, template, scope, expression, and substitution mangling paths.
 * @details Covers primitive/extended integers, `_BitInt`, enums, floats, complex, vectors, arrays,
 * pointers/references, routines, pointers-to-members, `std::*` abbreviations, lambdas, templates/packs,
 * enclosing scopes, dynamic buffer resizing, overflow checks, and base-36 substitution tokens up to `S10_`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_mangling_types_and_exprs(void) GCC_GEN_BE_NOEXCEPT {
    char buf[1024];
    char *alloc_str = NULL;

    /* Invalid argument and overflow checks */
    if (gcc_gen_be_mangle_type_name_alloc(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name_alloc(NULL, &alloc_str));
    if (!alloc_str || std::strcmp(alloc_str, "v") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    std::free(alloc_str);

    a_type int_tp = {};
    int_tp.kind = tk_integer;
    int_tp.size = 4;
    int_tp.variant.integer.int_kind = ik_int;

    if (gcc_gen_be_mangle_type_name(&int_tp, NULL, 16) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mangle_type_name(&int_tp, buf, 0) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mangle_type_name(&int_tp, buf, 1) != GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_mangle_with_substitutions(&int_tp, TRUE, NULL, 16) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mangle_with_substitutions(&int_tp, TRUE, buf, 0) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mangle_with_substitutions(&int_tp, TRUE, buf, 1) != GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Canonical typeinfo mangler hook variants */
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo = [](a_type_ptr) noexcept -> char * { return NULL; };
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&int_tp, buf, sizeof(buf)));
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo = [](a_type_ptr) noexcept -> char * { return const_cast<char *>("not_zti"); };
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&int_tp, buf, sizeof(buf)));
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo = [](a_type_ptr) noexcept -> char * { return const_cast<char *>("_ZTI"); };
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&int_tp, buf, sizeof(buf)));
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo = [](a_type_ptr) noexcept -> char * { return const_cast<char *>("_ZTI6CanonT"); };
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&int_tp, buf, sizeof(buf)));
    if (std::strcmp(buf, "6CanonT") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(10, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_type_name(&int_tp, buf, sizeof(buf));
    }) );
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo = NULL;

    /* Typerefs: nullptr_t, restrict/volatile/const qualifiers, and dangling NULL typeref */
    a_type tref_nullptr = {}, tref_dangling = {}, tref_cvr = {};
    tref_nullptr.kind = tk_typeref;
    tref_nullptr.source_corresp.name = "nullptr_t";
    tref_nullptr.variant.typeref.type = &int_tp;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_type_name(&tref_nullptr, buf, sizeof(buf));
    }) );
    if (std::strcmp(buf, "Dn") != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    tref_nullptr.source_corresp.name = "other_typedef";
    tref_dangling.kind = tk_typeref;
    tref_dangling.variant.typeref.qualifiers = TQ_CONST | TQ_VOLATILE | TQ_RESTRICT;
    tref_dangling.variant.typeref.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&tref_dangling, buf, sizeof(buf)));
    if (std::strcmp(buf, "rVKv") != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    tref_cvr.kind = tk_typeref;
    tref_cvr.variant.typeref.qualifiers = TQ_CONST | TQ_VOLATILE | TQ_RESTRICT;
    tref_cvr.variant.typeref.type = &tref_nullptr;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&tref_cvr, buf, sizeof(buf)));
    if (std::strcmp(buf, "rVKi") != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Void, nullptr, bool, all integer kinds, bit-precise, and unknown default kind */
    a_type void_tp = {}, nullptr_tp = {}, bool_tp = {}, tpl_param_tp = {};
    void_tp.kind = tk_void;
    nullptr_tp.kind = tk_nullptr;
    bool_tp.kind = tk_integer;
    bool_tp.variant.integer.bool_type = TRUE;
    tpl_param_tp.kind = tk_template_param;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&void_tp, buf, sizeof(buf)));
    GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_type_name(&nullptr_tp, buf, sizeof(buf));
    }) );
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&bool_tp, buf, sizeof(buf)));
    GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_type_name(&tpl_param_tp, buf, sizeof(buf));
    }) );

    const an_integer_kind int_kinds[] = {
        ik_char, ik_signed_char, ik_unsigned_char, ik_short, ik_unsigned_short,
        ik_int, ik_unsigned_int, ik_long, ik_unsigned_long, ik_long_long, ik_unsigned_long_long,
#if INT128_EXTENSIONS_ALLOWED
        ik_int128, ik_unsigned_int128,
#endif
        (an_integer_kind)99
    };
    for (size_t i = 0; i < sizeof(int_kinds) / sizeof(int_kinds[0]); ++i) {
        a_type it = {};
        it.kind = tk_integer;
        it.size = 8;
        it.variant.integer.int_kind = int_kinds[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&it, buf, sizeof(buf)));
    }

    an_integer_type_supplement bp_supp_zero = {}, bp_supp_24 = {};
    bp_supp_zero.bit_width = 0;
    bp_supp_24.bit_width = 24;
    a_type bp_tp = {};
    bp_tp.kind = tk_integer;
    bp_tp.size = 4;
    bp_tp.variant.integer.int_kind = ik_bit_precise;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&bp_tp, buf, sizeof(buf)));
    bp_tp.variant.integer.extra_info = &bp_supp_zero;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&bp_tp, buf, sizeof(buf)));
    bp_tp.variant.integer.extra_info = &bp_supp_24;
    bp_tp.variant.integer.int_kind = ik_unsigned_bit_precise;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&bp_tp, buf, sizeof(buf)));
    if (std::strcmp(buf, "DU24_") != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Enclosing scopes: file, std (root vs file-parent vs nested), named/empty/null ns, class, function, block, enum */
    a_scope file_sc = {}, std_root_sc = {}, std_file_sc = {}, std_nested_sc = {};
    a_scope named_ns_sc = {}, empty_ns_sc = {}, null_ns_sc = {}, noassoc_ns_sc = {};
    a_scope cls_sc = {}, empty_cls_sc = {}, null_cls_sc = {}, noassoc_cls_sc = {};
    a_scope fn_sc = {}, empty_fn_sc = {}, null_fn_sc = {}, noassoc_fn_sc = {};
    a_scope blk1_sc = {}, blk3_sc = {}, enum_sc = {};

    a_namespace ns_std = {}, ns_named = {}, ns_empty = {}, ns_null = {};
    ns_std.source_corresp.name = "std";
    ns_named.source_corresp.name = "my_ns";
    ns_empty.source_corresp.name = "";
    ns_null.source_corresp.name = NULL;

    file_sc.kind = sck_file;
    std_root_sc.kind = sck_namespace; std_root_sc.variant.assoc_namespace = &ns_std; std_root_sc.parent = NULL;
    std_file_sc.kind = sck_namespace; std_file_sc.variant.assoc_namespace = &ns_std; std_file_sc.parent = &file_sc;
    named_ns_sc.kind = sck_namespace; named_ns_sc.variant.assoc_namespace = &ns_named; named_ns_sc.parent = &file_sc;
    std_nested_sc.kind = sck_namespace; std_nested_sc.variant.assoc_namespace = &ns_std; std_nested_sc.parent = &named_ns_sc;
    empty_ns_sc.kind = sck_namespace; empty_ns_sc.variant.assoc_namespace = &ns_empty; empty_ns_sc.parent = &std_nested_sc;
    null_ns_sc.kind = sck_namespace; null_ns_sc.variant.assoc_namespace = &ns_null; null_ns_sc.parent = &empty_ns_sc;
    noassoc_ns_sc.kind = sck_namespace; noassoc_ns_sc.variant.assoc_namespace = NULL; noassoc_ns_sc.parent = &null_ns_sc;

    a_type sc_cls_named = {}, sc_cls_empty = {}, sc_cls_null = {};
    sc_cls_named.kind = tk_class; sc_cls_named.source_corresp.name = "OuterCls";
    sc_cls_empty.kind = tk_class; sc_cls_empty.source_corresp.name = "";
    sc_cls_null.kind = tk_class; sc_cls_null.source_corresp.name = NULL;
    cls_sc.kind = sck_class_struct_union; cls_sc.variant.assoc_type = &sc_cls_named; cls_sc.parent = &noassoc_ns_sc;
    empty_cls_sc.kind = sck_class_struct_union; empty_cls_sc.variant.assoc_type = &sc_cls_empty; empty_cls_sc.parent = &cls_sc;
    null_cls_sc.kind = sck_class_struct_union; null_cls_sc.variant.assoc_type = &sc_cls_null; null_cls_sc.parent = &empty_cls_sc;
    noassoc_cls_sc.kind = sck_class_struct_union; noassoc_cls_sc.variant.assoc_type = NULL; noassoc_cls_sc.parent = &null_cls_sc;

    a_routine sc_fn_named = {}, sc_fn_empty = {}, sc_fn_null = {};
    sc_fn_named.source_corresp.name = "enclosingFn";
    sc_fn_empty.source_corresp.name = "";
    sc_fn_null.source_corresp.name = NULL;
    fn_sc.kind = sck_function; fn_sc.variant.routine.ptr = &sc_fn_named; fn_sc.parent = &noassoc_cls_sc;
    empty_fn_sc.kind = sck_function; empty_fn_sc.variant.routine.ptr = &sc_fn_empty; empty_fn_sc.parent = &fn_sc;
    null_fn_sc.kind = sck_function; null_fn_sc.variant.routine.ptr = &sc_fn_null; null_fn_sc.parent = &empty_fn_sc;
    noassoc_fn_sc.kind = sck_function; noassoc_fn_sc.variant.routine.ptr = NULL; noassoc_fn_sc.parent = &null_fn_sc;

    blk1_sc.kind = sck_block; blk1_sc.number = 1; blk1_sc.parent = &noassoc_fn_sc;
    blk3_sc.kind = sck_block; blk3_sc.number = 3; blk3_sc.parent = &blk1_sc;
    enum_sc.kind = sck_enum; enum_sc.parent = &blk3_sc;

    /* Enum types: top-level/file-scope/nested, named/empty/null */
    a_type enum_tp = {};
    enum_tp.kind = tk_integer;
    enum_tp.variant.integer.enum_type = TRUE;
    enum_tp.source_corresp.name = "MyEnum";
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&enum_tp, buf, sizeof(buf)));
    enum_tp.source_corresp.parent_scope = &file_sc;
    enum_tp.source_corresp.name = "";
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&enum_tp, buf, sizeof(buf)));
    enum_tp.source_corresp.parent_scope = &std_root_sc;
    enum_tp.source_corresp.name = NULL;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(25, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_type_name(&enum_tp, buf, sizeof(buf));
    }) );
    enum_tp.source_corresp.parent_scope = &std_file_sc;
    enum_tp.source_corresp.name = "StdEnum";
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&enum_tp, buf, sizeof(buf)));
    enum_tp.source_corresp.parent_scope = &enum_sc;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(45, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_type_name(&enum_tp, buf, sizeof(buf));
    }) );

    /* Complex and float kinds */
    const a_float_kind complex_kinds[] = {
        fk_float, fk_double, fk_long_double, fk_float80,
        fk_float128, fk_std_float128, fk_float16, fk_std_float16, (a_float_kind)99
    };
    for (size_t i = 0; i < sizeof(complex_kinds) / sizeof(complex_kinds[0]); ++i) {
        a_type ct = {};
        ct.kind = tk_complex;
        ct.variant.float_kind = complex_kinds[i];
        GCC_GEN_BE_CHECK( sweep_oom_budgets(15, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_mangle_type_name(&ct, buf, sizeof(buf));
        }) );
    }
    const a_float_kind float_kinds[] = {
        fk_float, fk_double, fk_long_double, fk_float80,
        fk_float128, fk_std_float128, fk_float16, fk_std_float16, fk_std_bfloat16, (a_float_kind)99
    };
    for (size_t i = 0; i < sizeof(float_kinds) / sizeof(float_kinds[0]); ++i) {
        a_type ft = {};
        ft.kind = tk_float;
        ft.variant.float_kind = float_kinds[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&ft, buf, sizeof(buf)));
    }

    /* Vectors, pointers/references, arrays, pointers-to-members */
    a_type zero_sz_tp = {}, vec_tp = {}, ptr_tp = {}, lref_tp = {}, rref_tp = {}, arr_tp = {}, ptm_tp = {};
    zero_sz_tp.kind = tk_integer; zero_sz_tp.size = 0; zero_sz_tp.variant.integer.int_kind = ik_int;
    vec_tp.kind = tk_vector; vec_tp.size = 16; vec_tp.variant.vector.element_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&vec_tp, buf, sizeof(buf)));
    vec_tp.variant.vector.element_type = &zero_sz_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&vec_tp, buf, sizeof(buf)));
    vec_tp.variant.vector.element_type = &int_tp;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_type_name(&vec_tp, buf, sizeof(buf));
    }) );

    ptr_tp.kind = tk_pointer; ptr_tp.variant.pointer.type = &int_tp;
    lref_tp.kind = tk_pointer; lref_tp.variant.pointer.is_reference = TRUE; lref_tp.variant.pointer.type = &int_tp;
    rref_tp.kind = tk_pointer; rref_tp.variant.pointer.is_reference = TRUE; rref_tp.variant.pointer.is_rvalue_reference = TRUE; rref_tp.variant.pointer.type = &int_tp;
    arr_tp.kind = tk_array; arr_tp.variant.array.variant.number_of_elements = 8; arr_tp.variant.array.element_type = &int_tp;
    ptm_tp.kind = tk_ptr_to_member; ptm_tp.variant.ptr_to_member.class_of_which_a_member = &sc_cls_named; ptm_tp.variant.ptr_to_member.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&ptr_tp, buf, sizeof(buf)));
    GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_type_name(&lref_tp, buf, sizeof(buf));
    }) );
    GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_type_name(&rref_tp, buf, sizeof(buf));
    }) );
    GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_type_name(&arr_tp, buf, sizeof(buf));
    }) );
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&ptm_tp, buf, sizeof(buf)));

    /* Routine types: rtsp NULL, CVR + ref-qualifiers + noexcept + ellipsis */
    a_type fn_tp = {};
    fn_tp.kind = tk_routine;
    fn_tp.variant.routine.return_type = &void_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&fn_tp, buf, sizeof(buf)));

    an_exception_specification exc_noexcept = {}, exc_throw_any = {}, exc_false = {};
    exc_noexcept.is_noexcept = TRUE; exc_noexcept.throw_any = FALSE;
    exc_throw_any.is_noexcept = TRUE; exc_throw_any.throw_any = TRUE;
    exc_false.is_noexcept = FALSE;

    a_param_type fp1 = {}, fp2 = {};
    fp1.type = &int_tp; fp1.next = &fp2;
    fp2.type = &bool_tp;
    a_routine_type_supplement fn_supp = {};
    fn_tp.variant.routine.extra_info = &fn_supp;
    fn_supp.qualifiers = TQ_CONST | TQ_VOLATILE | TQ_RESTRICT;
    fn_supp.ref_qualifiers = (a_ref_qualifier_kind)rqk_lvalue;
    fn_supp.exception_specification = &exc_noexcept;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(30, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_type_name(&fn_tp, buf, sizeof(buf));
    }) );

    fn_supp.ref_qualifiers = (a_ref_qualifier_kind)rqk_rvalue;
    fn_supp.exception_specification = &exc_throw_any;
    fn_supp.param_type_list = &fp1;
    fn_supp.has_ellipsis = TRUE;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(35, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_type_name(&fn_tp, buf, sizeof(buf));
    }) );

    fn_supp.qualifiers = TQ_NONE;
    fn_supp.ref_qualifiers = (a_ref_qualifier_kind)rqk_default;
    fn_supp.exception_specification = &exc_false;
    fn_supp.has_ellipsis = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&fn_tp, buf, sizeof(buf)));

    /* Standard library abbreviations (std::allocator, basic_string, string, istream, ostream, iostream, non-abbrev) */
    const char *std_names[] = { "allocator", "basic_string", "string", "istream", "ostream", "iostream", "vector" };
    for (size_t i = 0; i < sizeof(std_names) / sizeof(std_names[0]); ++i) {
        a_type st = {};
        st.kind = tk_class;
        st.source_corresp.name = std_names[i];
        st.source_corresp.parent_scope = (i % 2 == 0) ? &std_root_sc : &std_file_sc;
        GCC_GEN_BE_CHECK( sweep_oom_budgets(25, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_mangle_type_name(&st, buf, sizeof(buf));
        }) );
    }
    {
        /* std scope edge cases: nested std, unnamed in std, noassoc_ns, null_ns, named_ns */
        a_type st_edge = {};
        st_edge.kind = tk_class;
        st_edge.source_corresp.name = "string";
        st_edge.source_corresp.parent_scope = &std_nested_sc;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&st_edge, buf, sizeof(buf)));
        st_edge.source_corresp.parent_scope = &noassoc_ns_sc;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&st_edge, buf, sizeof(buf)));
        st_edge.source_corresp.parent_scope = &null_ns_sc;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&st_edge, buf, sizeof(buf)));
        st_edge.source_corresp.parent_scope = &std_root_sc;
        st_edge.source_corresp.name = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&st_edge, buf, sizeof(buf)));
    }

    /* Unnamed top-level & nested classes, empty name, and unmangled_name_or_mangled_encoding */
    {
        a_type un_top = {}, un_empty = {};
        un_top.kind = tk_struct;
        un_top.source_corresp.name = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&un_top, buf, sizeof(buf)));
        if (std::strcmp(buf, "11unnamed_cls") != 0) return GCC_GEN_BE_ERROR_INTERNAL;

        un_empty.kind = tk_union;
        un_empty.source_corresp.name = "";
        un_empty.source_corresp.parent_scope = &named_ns_sc;
        un_empty.source_corresp.unmangled_name_or_mangled_encoding = const_cast<char *>("");
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&un_empty, buf, sizeof(buf)));
        un_empty.source_corresp.unmangled_name_or_mangled_encoding = const_cast<char *>("OrigUnion");
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&un_empty, buf, sizeof(buf)));
    }

    /* Lambda closure classes across all operator() shapes */
    {
        a_class_type_supplement lam_supp = {};
        lam_supp.is_lambda_closure_class = TRUE;
        a_type lam_tp = {};
        lam_tp.kind = tk_class;
        lam_tp.variant.class_struct_union.extra_info = &lam_supp;
        /* 1. assoc_scope == NULL */
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&lam_tp, buf, sizeof(buf)));
        /* 2. assoc_scope->routines == NULL */
        a_scope lam_sc = {};
        lam_supp.assoc_scope = &lam_sc;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&lam_tp, buf, sizeof(buf)));
        /* 3. non-operator routine, malformed operator routines, 1-param non-static (this-only), multi-param non-static, static */
        a_routine lr_nonop = {}, lr_notype = {}, lr_nonrout = {}, lr_noextra = {}, lr_noparam = {}, lr_thisonly = {}, lr_multi = {}, lr_static = {};
        a_type nonrout_tp = {}; nonrout_tp.kind = tk_integer;
        a_type rt_noextra = {}; rt_noextra.kind = tk_routine;
        a_routine_type_supplement rts_noparam = {}, rts_thisonly = {}, rts_multi = {};
        a_param_type p_this = {}, p_a = {}, p_b = {};
        p_this.type = &ptr_tp;
        p_a.type = &int_tp; p_a.next = &p_b;
        p_b.type = &bool_tp;
        rts_thisonly.param_type_list = &p_this;
        a_type rt_noparam = {}, rt_thisonly = {}, rt_multi = {};
        rt_noparam.kind = tk_routine; rt_noparam.variant.routine.extra_info = &rts_noparam;
        rt_thisonly.kind = tk_routine; rt_thisonly.variant.routine.extra_info = &rts_thisonly;
        rt_multi.kind = tk_routine; rt_multi.variant.routine.extra_info = &rts_multi;

        lr_nonop.special_kind = (a_special_function_kind)sfk_none; lr_nonop.next = &lr_notype;
        lr_notype.special_kind = (a_special_function_kind)sfk_operator; lr_notype.type = NULL; lr_notype.next = &lr_nonrout;
        lr_nonrout.special_kind = (a_special_function_kind)sfk_operator; lr_nonrout.type = &nonrout_tp; lr_nonrout.next = &lr_noextra;
        lr_noextra.special_kind = (a_special_function_kind)sfk_operator; lr_noextra.type = &rt_noextra; lr_noextra.next = &lr_noparam;
        lr_noparam.special_kind = (a_special_function_kind)sfk_operator; lr_noparam.type = &rt_noparam;

        lam_sc.routines = &lr_nonop;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&lam_tp, buf, sizeof(buf)));

        lr_thisonly.special_kind = (a_special_function_kind)sfk_operator; lr_thisonly.type = &rt_thisonly;
        lam_sc.routines = &lr_thisonly;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&lam_tp, buf, sizeof(buf)));

        p_this.next = &p_a;
        rts_multi.param_type_list = &p_this;
        lr_multi.special_kind = (a_special_function_kind)sfk_operator; lr_multi.type = &rt_multi;
        lam_sc.routines = &lr_multi;
        GCC_GEN_BE_CHECK( sweep_oom_budgets(40, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_mangle_type_name(&lam_tp, buf, sizeof(buf));
        }) );

        lr_static.special_kind = (a_special_function_kind)sfk_operator; lr_static.storage_class = (a_storage_class)sc_static; lr_static.type = &rt_multi;
        lam_sc.routines = &lr_static;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&lam_tp, buf, sizeof(buf)));
    }

    /* Template arguments: packs, type (null & valid), nontype (array bound +/-, null const, int +/-, address routine/var/other, float), template-template, other */
    {
        a_routine addr_rout = {}, addr_rout_noname = {};
        addr_rout.source_corresp.name = "tplTargetFn";
        a_variable addr_var = {}, addr_var_noname = {};
        addr_var.source_corresp.name = "tplTargetVar";
        a_template tt_named = {}, tt_noname = {};
        tt_named.source_corresp.name = "MyContainer";

        a_constant c_int_pos = {}, c_int_notype = {}, c_addr_r = {}, c_addr_r_noname = {}, c_addr_r_null = {};
        a_constant c_addr_v = {}, c_addr_v_noname = {}, c_addr_v_null = {}, c_addr_other = {}, c_float = {};
        c_int_pos.kind = (a_constant_repr_kind)ck_integer; c_int_pos.type = &int_tp;
        c_int_notype.kind = (a_constant_repr_kind)ck_integer; c_int_notype.type = NULL;
        c_addr_r.kind = (a_constant_repr_kind)ck_address; c_addr_r.type = &ptr_tp; c_addr_r.variant.address.kind = (an_address_base_kind)abk_routine; c_addr_r.variant.address.variant.routine = &addr_rout;
        c_addr_r_noname.kind = (a_constant_repr_kind)ck_address; c_addr_r_noname.variant.address.kind = (an_address_base_kind)abk_routine; c_addr_r_noname.variant.address.variant.routine = &addr_rout_noname;
        c_addr_r_null.kind = (a_constant_repr_kind)ck_address; c_addr_r_null.variant.address.kind = (an_address_base_kind)abk_routine;
        c_addr_v.kind = (a_constant_repr_kind)ck_address; c_addr_v.type = &ptr_tp; c_addr_v.variant.address.kind = (an_address_base_kind)abk_variable; c_addr_v.variant.address.variant.variable = &addr_var;
        c_addr_v_noname.kind = (a_constant_repr_kind)ck_address; c_addr_v_noname.variant.address.kind = (an_address_base_kind)abk_variable; c_addr_v_noname.variant.address.variant.variable = &addr_var_noname;
        c_addr_v_null.kind = (a_constant_repr_kind)ck_address; c_addr_v_null.variant.address.kind = (an_address_base_kind)abk_variable;
        c_addr_other.kind = (a_constant_repr_kind)ck_address; c_addr_other.variant.address.kind = (an_address_base_kind)abk_constant;
        c_float.kind = (a_constant_repr_kind)ck_float; c_float.type = &int_tp;

        a_template_arg ta[18] = {};
        ta[0].kind = (a_templ_arg_kind)tak_start_of_pack_expansion;
        ta[1].kind = (a_templ_arg_kind)tak_type; ta[1].variant.type = &int_tp; ta[1].is_pack_element = TRUE;
        ta[2].kind = (a_templ_arg_kind)tak_type; ta[2].variant.type = NULL; ta[2].is_pack_element = TRUE;
        ta[3].kind = (a_templ_arg_kind)tak_nontype; ta[3].is_array_bound_of_unknown_type = TRUE; ta[3].variant.integer_value = -5;
        ta[4].kind = (a_templ_arg_kind)tak_nontype; ta[4].is_array_bound_of_unknown_type = TRUE; ta[4].variant.integer_value = 7;
        ta[5].kind = (a_templ_arg_kind)tak_nontype; ta[5].variant.constant = NULL;
        ta[6].kind = (a_templ_arg_kind)tak_nontype; ta[6].variant.constant = &c_int_pos;
        ta[7].kind = (a_templ_arg_kind)tak_nontype; ta[7].variant.constant = &c_int_notype;
        ta[8].kind = (a_templ_arg_kind)tak_nontype; ta[8].variant.constant = &c_addr_r;
        ta[9].kind = (a_templ_arg_kind)tak_nontype; ta[9].variant.constant = &c_addr_r_noname;
        ta[10].kind = (a_templ_arg_kind)tak_nontype; ta[10].variant.constant = &c_addr_r_null;
        ta[11].kind = (a_templ_arg_kind)tak_nontype; ta[11].variant.constant = &c_addr_v;
        ta[12].kind = (a_templ_arg_kind)tak_nontype; ta[12].variant.constant = &c_addr_v_noname;
        ta[13].kind = (a_templ_arg_kind)tak_nontype; ta[13].variant.constant = &c_addr_v_null;
        ta[14].kind = (a_templ_arg_kind)tak_nontype; ta[14].variant.constant = &c_addr_other;
        ta[15].kind = (a_templ_arg_kind)tak_nontype; ta[15].variant.constant = &c_float;
        ta[16].kind = (a_templ_arg_kind)tak_template; ta[16].variant.templ.ptr = &tt_named;
        ta[17].kind = (a_templ_arg_kind)tak_template; ta[17].variant.templ.ptr = &tt_noname; ta[17].is_pack_element = TRUE;
        for (int i = 0; i < 17; ++i) ta[i].next = &ta[i + 1];

        a_template_arg ta_null_tt = {}, ta_expr = {};
        ta_null_tt.kind = (a_templ_arg_kind)tak_template; ta_null_tt.variant.templ.ptr = NULL; ta_null_tt.next = &ta_expr;
        ta_expr.kind = (a_templ_arg_kind)99;

        a_class_type_supplement tpl_cls_supp = {};
        tpl_cls_supp.template_arg_list = &ta[0];
        a_type tpl_cls = {};
        tpl_cls.kind = tk_class;
        tpl_cls.source_corresp.name = "MegaTemplateClassWithAVeryLongIdentifierToForceMultipleDynamicBufferReallocationsDuringMangling";
        tpl_cls.source_corresp.parent_scope = &std_root_sc; /* std::<tpl> has targs != NULL so bypasses std abbrev */
        tpl_cls.variant.class_struct_union.extra_info = &tpl_cls_supp;

        g_mock_int_const_val = 42;
        GCC_GEN_BE_CHECK( sweep_oom_budgets(180, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_mangle_type_name(&tpl_cls, buf, sizeof(buf));
        }) );
        g_mock_int_const_val = -42;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&tpl_cls, buf, sizeof(buf)));
        g_mock_int_const_val = 0;

        tpl_cls_supp.template_arg_list = &ta_null_tt;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&tpl_cls, buf, sizeof(buf)));
    }

    /* 40 distinct classes repeated twice -> exercises active_subst_table growth (0->32->64) and base-36 S_ / S0_ .. S12_ */
    {
        char cnames[40][16];
        a_type classes[40] = {};
        a_param_type params[80] = {};
        for (int i = 0; i < 40; ++i) {
            std::snprintf(cnames[i], sizeof(cnames[i]), "SubstCls%02d", i);
            classes[i].kind = tk_class;
            classes[i].source_corresp.name = cnames[i];
            params[i].type = &classes[i];
            params[i].next = &params[i + 1];
            params[40 + i].type = &classes[i];
            params[40 + i].next = (i + 1 < 40) ? &params[40 + i + 1] : NULL;
        }
        a_routine_type_supplement big_rts = {};
        big_rts.param_type_list = &params[0];
        a_type big_fn = {};
        big_fn.kind = tk_routine;
        big_fn.variant.routine.return_type = &void_tp;
        big_fn.variant.routine.extra_info = &big_rts;

        GCC_GEN_BE_CHECK( sweep_oom_budgets(350, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_mangle_with_substitutions(&big_fn, TRUE, buf, sizeof(buf));
        }) );
        if (std::strstr(buf, "S10_") == NULL) return GCC_GEN_BE_ERROR_INTERNAL;
        /* Also test reset_table == FALSE OOM sweep */
        GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_mangle_with_substitutions(&classes[39], FALSE, buf, sizeof(buf));
        }) );
    }

    /* Expression mangling: invalid args, overflow, constants (typed/untyped, int +/-, non-int), sizeof(T), all ops, DT fallback */
    {
        char ebuf[256];
        an_expr_node e_con = {};
        e_con.kind = enk_constant;
        if (gcc_gen_be_mangle_expr(NULL, ebuf, sizeof(ebuf)) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
        if (gcc_gen_be_mangle_expr(&e_con, NULL, sizeof(ebuf)) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
        if (gcc_gen_be_mangle_expr(&e_con, ebuf, 0) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
        /* enk_constant with ptr == NULL and expr->type == NULL -> DTiE */
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(&e_con, ebuf, sizeof(ebuf)));
        if (std::strcmp(ebuf, "DTiE") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
        if (gcc_gen_be_mangle_expr(&e_con, ebuf, 2) != GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW) return GCC_GEN_BE_ERROR_INTERNAL;

        a_constant c1 = {}, c_notype = {}, c_flt = {};
        c1.kind = (a_constant_repr_kind)ck_integer; c1.type = &int_tp;
        c_notype.kind = (a_constant_repr_kind)ck_integer; c_notype.type = NULL;
        c_flt.kind = (a_constant_repr_kind)ck_float; c_flt.type = &int_tp;

        e_con.variant.constant.ptr = &c1;
        g_mock_int_const_val = -9;
        GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_mangle_expr(&e_con, ebuf, sizeof(ebuf));
        }) );
        if (std::strcmp(ebuf, "Lin9E") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
        g_mock_int_const_val = 9;
        e_con.variant.constant.ptr = &c_notype;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(&e_con, ebuf, sizeof(ebuf)));
        e_con.variant.constant.ptr = &c_flt;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(&e_con, ebuf, sizeof(ebuf)));
        g_mock_int_const_val = 0;

        an_expr_node e_top = {};
        e_top.kind = enk_type_operand;
        e_top.type = &int_tp;
        /* type_operand.type == NULL falls through to DT<expr->type>E */
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(&e_top, ebuf, sizeof(ebuf)));
        e_top.variant.type_operand.type = &int_tp;
        GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_mangle_expr(&e_top, ebuf, sizeof(ebuf));
        }) );

        const an_expr_operator_kind ops[] = {
            eok_add, eok_subtract, eok_multiply, eok_divide, eok_negate, eok_not, (an_expr_operator_kind)99
        };
        an_expr_node lhs = {}, rhs = {}, bin = {};
        lhs.kind = enk_type_operand; lhs.variant.type_operand.type = &int_tp;
        rhs.kind = enk_type_operand; rhs.variant.type_operand.type = &bool_tp;
        bin.kind = enk_operation;
        /* op1 == NULL */
        bin.variant.operation.kind = eok_add;
        bin.variant.operation.operands = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(&bin, ebuf, sizeof(ebuf)));
        /* Binary and unary across all operator kinds */
        for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); ++i) {
            lhs.next = &rhs;
            bin.variant.operation.kind = ops[i];
            bin.variant.operation.operands = &lhs;
            GCC_GEN_BE_CHECK( sweep_oom_budgets(25, [&]() noexcept -> gcc_gen_be_error_t {
                return gcc_gen_be_mangle_expr(&bin, ebuf, sizeof(ebuf));
            }) );
        }
        lhs.next = NULL;
        bin.variant.operation.kind = eok_subtract;
        bin.variant.operation.operands = &lhs;
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(&bin, ebuf, sizeof(ebuf)));

        /* OOM sweep on DT fallback */
        e_con.variant.constant.ptr = NULL;
        e_con.type = &int_tp;
        GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_mangle_expr(&e_con, ebuf, sizeof(ebuf));
        }) );
        e_con.type = NULL;
        GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_mangle_expr(&e_con, ebuf, sizeof(ebuf));
        }) );
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises all Itanium C++ ABI function, operator, ctor/dtor clone, module, and member mangling paths.
 * @details Covers all 39 operator codes, literal operators (`operator""` with/without suffix), conversion
 * operators, canonical mangler variant rewriting (`C0`/`C1`/`C2`/`D0`/`D1`/`D2` before `E`/`I`/`B`),
 * C++20 module attachment (`W<mod>`), template functions, CVR/ref-qualified methods, and OOM sweeps.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_mangling_functions(void) GCC_GEN_BE_NOEXCEPT {
    char buf[512];
    char *alloc_str = NULL;

    a_type int_tp = {}, void_tp = {}, cls_tp = {};
    int_tp.kind = tk_integer; int_tp.size = 4; int_tp.variant.integer.int_kind = ik_int;
    void_tp.kind = tk_void;
    cls_tp.kind = tk_class; cls_tp.source_corresp.name = "Widget";

    a_routine rout = {};
    rout.source_corresp.name = "plainFn";

    if (gcc_gen_be_mangle_function_name_alloc(NULL, NULL, &alloc_str) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mangle_function_name_alloc(&rout, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mangle_function_name(NULL, NULL, buf, sizeof(buf)) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mangle_function_name(&rout, NULL, NULL, sizeof(buf)) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mangle_function_name(&rout, NULL, buf, 0) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mangle_function_name(&rout, NULL, buf, 3) != GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Canonical function mangler & clone_canonical_fn_with_variant */
    gcc_gen_be_singleton_state.canonical_mangle_func = [](a_routine_ptr) noexcept -> char * { return NULL; };
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, "C2", buf, sizeof(buf)));
    gcc_gen_be_singleton_state.canonical_mangle_func = [](a_routine_ptr) noexcept -> char * { return const_cast<char *>(""); };
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, "C2", buf, sizeof(buf)));

    const char *canon_inputs[] = {
        "_Z", "_ZN6WidgetC1Ev", "_ZN6WidgetC0IifEEv", "_ZN6WidgetC2B3tagEv",
        "_ZN6WidgetD0Ev", "_ZN6WidgetD1Ev", "_ZN6WidgetD2Ev", "_ZN6Widget3runEv"
    };
    static const char *s_cur_canon = NULL;
    gcc_gen_be_singleton_state.canonical_mangle_func = [](a_routine_ptr) noexcept -> char * {
        return const_cast<char *>(s_cur_canon);
    };
    for (size_t i = 0; i < sizeof(canon_inputs) / sizeof(canon_inputs[0]); ++i) {
        s_cur_canon = canon_inputs[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf)));
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, "X", buf, sizeof(buf)));
        GCC_GEN_BE_CHECK( sweep_oom_budgets(10, [&]() noexcept -> gcc_gen_be_error_t {
            return gcc_gen_be_mangle_function_name(&rout, "D0", buf, sizeof(buf));
        }) );
    }
    gcc_gen_be_singleton_state.canonical_mangle_func = NULL;

    /* unmangled_name_or_mangled_encoding and raw_name starting with _Z */
    rout.source_corresp.unmangled_name_or_mangled_encoding = const_cast<char *>("not_mangled");
    rout.source_corresp.name = "_ZN6WidgetC1Ev";
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, "C2", buf, sizeof(buf)));
    if (std::strcmp(buf, "_ZN6WidgetC2Ev") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    rout.source_corresp.unmangled_name_or_mangled_encoding = const_cast<char *>("_ZN6WidgetD1Ev");
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, "D2", buf, sizeof(buf)));
    if (std::strcmp(buf, "_ZN6WidgetD2Ev") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    rout.source_corresp.unmangled_name_or_mangled_encoding = NULL;

    /* Complete operator table + spaces + unknown operator + literal operators */
    const char *ops[] = {
        "operator new", "operator   new[]", "operator delete", "operator delete[]",
        "operator co_await", "operator()", "operator[]", "operator->*", "operator->",
        "operator==", "operator!=", "operator<=>", "operator<=", "operator>=",
        "operator<<=", "operator>>=", "operator<<", "operator>>", "operator<", "operator>",
        "operator+=", "operator-=", "operator*=", "operator/=", "operator%=",
        "operator&=", "operator|=", "operator^=", "operator++", "operator--",
        "operator&&", "operator||", "operator+", "operator-", "operator*", "operator/",
        "operator%", "operator&", "operator|", "operator^", "operator~", "operator!",
        "operator,", "operator=", "operator?", R"(operator"" _km)", R"(operator"")"
    };
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); ++i) {
        rout.source_corresp.name = ops[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf)));
    }

    /* Special kinds: constructor, destructor, conversion (null type, non-routine type, routine type), anonfn */
    a_param_type p_this = {}, p_arg = {};
    p_this.type = &cls_tp; p_this.next = &p_arg;
    p_arg.type = &int_tp;
    a_routine_type_supplement rts = {};
    rts.param_type_list = &p_this;
    rts.qualifiers = TQ_CONST | TQ_VOLATILE | TQ_RESTRICT;
    rts.ref_qualifiers = (a_ref_qualifier_kind)rqk_lvalue;
    rts.has_ellipsis = TRUE;
    a_type rtype = {};
    rtype.kind = tk_routine;
    rtype.variant.routine.return_type = &int_tp;
    rtype.variant.routine.extra_info = &rts;

    a_scope cls_sc = {}, ns_sc = {}, file_sc = {};
    a_namespace ns = {}; ns.source_corresp.name = "pkg";
    file_sc.kind = sck_file;
    cls_sc.kind = sck_class_struct_union; cls_sc.variant.assoc_type = &cls_tp; cls_sc.parent = &file_sc;
    ns_sc.kind = sck_namespace; ns_sc.variant.assoc_namespace = &ns; ns_sc.parent = &file_sc;

    a_template_arg fn_ta = {};
    fn_ta.kind = (a_templ_arg_kind)tak_type;
    fn_ta.variant.type = &int_tp;

    /* Nested member function with CVR + lvalue/rvalue/default ref-qualifiers + template args + ellipsis */
    rout = {};
    rout.source_corresp.name = "method";
    rout.source_corresp.parent_scope = &cls_sc;
    rout.type = &rtype;
    rout.is_template_function = TRUE;
    rout.template_arg_list = &fn_ta;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(60, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_function_name(&rout, "", buf, sizeof(buf));
    }) );

    rts.ref_qualifiers = (a_ref_qualifier_kind)rqk_rvalue;
    rts.has_ellipsis = FALSE;
    rout.is_template_function = TRUE;
    rout.template_arg_list = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, "C2", buf, sizeof(buf)));

    rts.qualifiers = TQ_NONE;
    rts.ref_qualifiers = (a_ref_qualifier_kind)rqk_default;
    rout.special_kind = (a_special_function_kind)sfk_constructor;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf)));
    rout.special_kind = (a_special_function_kind)sfk_destructor;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf)));

    /* Conversion operator with routine type, non-routine type, and NULL type (also nested with rtsp == NULL) */
    rout.special_kind = (a_special_function_kind)sfk_conversion;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(30, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf));
    }) );
    rout.type = &int_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf)));
    rout.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf)));

    /* Static class method, 0-arg variadic (!ptp && rtsp->has_ellipsis), namespace fn, anonfn, module variants */
    rout = {};
    rout.source_corresp.parent_scope = &cls_sc;
    rout.storage_class = (a_storage_class)sc_static;
    rout.type = &rtype;
    rout.source_corresp.name = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf)));

    rts.param_type_list = NULL;
    rts.has_ellipsis = TRUE;
    rout.source_corresp.parent_scope = &ns_sc;
    rout.source_corresp.name = "";
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf)));

    a_module mod = {};
    rout = {};
    rout.source_corresp.name = "modFn";
    rout.source_corresp.module_info = &mod;
    /* exported == TRUE */
    rout.source_corresp.is_exported = TRUE;
    mod.kind = mk_unit; mod.variant.unit.name = "m";
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf)));
    /* kind != mk_unit */
    rout.source_corresp.is_exported = FALSE;
    mod.kind = mk_none;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf)));
    /* unit.name == NULL & "" & valid */
    mod.kind = mk_unit; mod.variant.unit.name = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf)));
    mod.variant.unit.name = "";
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf)));
    mod.variant.unit.name = "my_mod";
    GCC_GEN_BE_CHECK( sweep_oom_budgets(25, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_function_name(&rout, NULL, buf, sizeof(buf));
    }) );

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises VTable (`_ZTV`), Construction VTable (`_ZTC`), VTT (`_ZTT`), and Thunk emission.
 * @details Covers missing-context errors, `ctsp == NULL`, primary/secondary/virtual vtables, cache hits,
 * anonymous linkage, pure/deleted/thunked/cached/null virtual functions, `__SO__` subobject VTT skip,
 * sret/invis-ref/variadic/void/non-void thunks, and deterministic OOM + mock failure percolation.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_vtables_vtt_and_thunks(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_lvalue *out_lv = NULL;
    gcc_jit_function *out_fn = NULL;
    gcc_jit_rvalue *out_rv = NULL;

    a_type int_tp = {}, void_tp = {}, ptr_tp = {};
    int_tp.kind = tk_integer; int_tp.size = 4; int_tp.variant.integer.int_kind = ik_int;
    void_tp.kind = tk_void;
    ptr_tp.kind = tk_pointer; ptr_tp.size = 8; ptr_tp.variant.pointer.type = &int_tp;

    /* Test INVALID_STATE when JIT context is NULL across all 6 JIT entry points */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    a_type bare_cls = {}; bare_cls.kind = tk_class;
    a_routine bare_rout = {};
    an_expr_node bare_op = {}, bare_dc = {}, bare_ti = {};
    bare_dc.type = &ptr_tp; bare_dc.variant.operation.operands = &bare_op;
    if (gcc_gen_be_emit_vtable(&bare_cls, &out_lv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_vtt(&bare_cls, &out_lv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_thunk(&bare_rout, &out_fn) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_dynamic_cast(&bare_dc, &out_rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_typeid(&bare_ti, &out_rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_type_descriptor(&int_tp, &out_lv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* Invalid class / thunk routine shapes */
    if (gcc_gen_be_emit_vtable(&bare_cls, &out_lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_vtt(&bare_cls, &out_lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_vtt(&int_tp, &out_lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    a_type tref_null = {}, tref_cls = {};
    tref_null.kind = tk_typeref; tref_null.variant.typeref.type = NULL;
    if (gcc_gen_be_emit_vtt(&tref_null, &out_lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_emit_thunk(&bare_rout, &out_fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    bare_rout.type = &int_tp;
    if (gcc_gen_be_emit_thunk(&bare_rout, &out_fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    a_type rt_noextra = {}; rt_noextra.kind = tk_routine;
    bare_rout.type = &rt_noextra;
    if (gcc_gen_be_emit_thunk(&bare_rout, &out_fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    a_routine_type_supplement rts_noparam = {};
    rt_noextra.variant.routine.extra_info = &rts_noparam;
    if (gcc_gen_be_emit_thunk(&bare_rout, &out_fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Thunk matrix: target_encoding == "_Z", non-_Z, sret, invis_ref param, variadic, anonymous, void return, cache hit */
    a_type tref_ptr = {}; tref_ptr.kind = tk_typeref; tref_ptr.variant.typeref.type = &ptr_tp;
    a_param_type tp0 = {}, tp1 = {};
    tp0.type = &tref_ptr; tp0.next = &tp1;
    tp1.type = &tref_ptr;
    a_routine_type_supplement trts = {};
    trts.param_type_list = &tp0;
    a_type trtype = {};
    trtype.kind = tk_routine;
    trtype.variant.routine.return_type = &tref_ptr;
    trtype.variant.routine.extra_info = &trts;

    a_namespace anon_ns = {}; anon_ns.source_corresp.name = NULL;
    a_scope anon_sc = {}; anon_sc.kind = sck_namespace; anon_sc.variant.assoc_namespace = &anon_ns;

    a_routine tr = {};
    tr.type = &trtype;
    tr.source_corresp.name = "_Z"; /* len 2 -> target_encoding stays "_Z" */
    tr.delta = 8; tr.return_delta = 0; tr.vcall_index = 0; tr.vbase_index = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_thunk(&tr, &out_fn));
    /* Second call hits cached_thunk! */
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_thunk(&tr, &out_fn));

    /* Non-_Z canonical name + positive/negative combinations + anonymous scope + variadic + sret + invis_ref */
    gcc_gen_be_singleton_state.canonical_mangle_func = [](a_routine_ptr) noexcept -> char * {
        return const_cast<char *>("raw_c_target");
    };
    tr.source_corresp.parent_scope = &anon_sc;
    trts.has_ellipsis = TRUE;
    g_mock_invis_ref = TRUE;
    tr.delta = 16; tr.return_delta = 0; tr.vcall_index = 32; tr.vbase_index = 0;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(40, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_emit_thunk(&tr, &out_fn);
    }) );
    g_mock_invis_ref = FALSE;
    tr.delta = 16; tr.return_delta = 24; tr.vcall_index = 32; tr.vbase_index = 0;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(40, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_emit_thunk(&tr, &out_fn);
    }) );
    trts.has_ellipsis = FALSE;
    tr.source_corresp.parent_scope = NULL;
    gcc_gen_be_singleton_state.canonical_mangle_func = NULL;

    /* Void-returning thunk + mock failure injections in emit_thunk */
    a_type trtype_void = {};
    trtype_void.kind = tk_routine;
    trtype_void.variant.routine.return_type = &void_tp;
    trtype_void.variant.routine.extra_info = &trts;
    tr.type = &trtype_void;
    tr.source_corresp.name = "_ZN7Derived5vfuncEi";
    tr.delta = 0; tr.return_delta = 0; tr.vcall_index = 16; tr.vbase_index = 0;
    g_mock_lower_void_for_tk_void = TRUE;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(40, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_emit_thunk(&tr, &out_fn);
    }) );
    g_mock_lower_void_for_tk_void = FALSE;

    /* Covariant return thunk with both vbase_index != 0 and return_delta != 0 */
    tr.type = &trtype;
    tr.delta = 8; tr.return_delta = 16; tr.vcall_index = 0; tr.vbase_index = 24;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(40, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_emit_thunk(&tr, &out_fn);
    }) );
    tr.delta = 8; tr.return_delta = -16; tr.vcall_index = 0; tr.vbase_index = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_thunk(&tr, &out_fn));

    /* Mock error percolation inside gcc_gen_be_emit_thunk */
    tr.delta = 40;
    GCC_GEN_BE_CHECK(cache_clear_all());
    g_mock_lower_type_fail_countdown = 0; /* fail return type lowering */
    if (gcc_gen_be_emit_thunk(&tr, &out_fn) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_lower_type_fail_countdown = 1; /* fail param 0 type lowering */
    if (gcc_gen_be_emit_thunk(&tr, &out_fn) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_invis_ref_fail_countdown = 0; /* fail return invis_ref */
    if (gcc_gen_be_emit_thunk(&tr, &out_fn) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_invis_ref_fail_countdown = 1; /* fail param invis_ref */
    if (gcc_gen_be_emit_thunk(&tr, &out_fn) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_lower_fn_fail = TRUE; /* fail gcc_gen_be_lower_function_decl */
    if (gcc_gen_be_emit_thunk(&tr, &out_fn) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_lower_fn_fail = FALSE;

    /* Rich class hierarchy for gcc_gen_be_emit_vtable and gcc_gen_be_emit_vtt */
    /* Class 1: empty polymorphic class with VIRTUAL_FUNCTION_NUMBER_NONE, no assoc_scope, direct_base_classes only, anonymous */
    a_class_type_supplement empty_supp = {}, b0_supp = {};
    empty_supp.highest_virtual_function_number = VIRTUAL_FUNCTION_NUMBER_NONE;
    a_type b0_tp = {}, empty_cls = {};
    b0_tp.kind = tk_struct; b0_tp.size = 8; b0_tp.source_corresp.name = "DirectZeroBase"; b0_tp.variant.class_struct_union.extra_info = &b0_supp;
    a_base_class db0 = {};
    db0.type = &b0_tp; db0.direct = TRUE; db0.is_virtual = TRUE; db0.offset = 0; db0.has_public_derivation = TRUE;
    empty_supp.direct_base_classes = &db0;
    empty_cls.kind = tk_struct; empty_cls.size = 8; empty_cls.source_corresp.name = "AnonEmptyPoly";
    empty_cls.source_corresp.parent_scope = &anon_sc;
    empty_cls.variant.class_struct_union.extra_info = &empty_supp;

    tref_cls.kind = tk_typeref; tref_cls.variant.typeref.type = &empty_cls;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(80, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_emit_vtt(&tref_cls, &out_lv);
    }) );
    /* Repeat emit_vtable -> hits CACHE_VAR fast path */
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtable(&empty_cls, &out_lv));
    /* Repeat emit_vtt -> hits CACHE_STRING fast path */
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtt(&tref_cls, &out_lv));

    /* Pre-seeded CACHE_STRING + CACHE_VAR indirection for vtable and vtt, plus pre-cached vtable with missing CTC in VTT */
    empty_cls.source_corresp.parent_scope = NULL;
    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *arr8_t = gcc_jit_context_new_array_type(ctx, NULL, vp_t, 8);
    int dummy_key_vt = 1, dummy_key_vtt = 2;
    gcc_jit_lvalue *pre_vt = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, arr8_t, "_ZTV13AnonEmptyPoly_pre");
    gcc_jit_lvalue *pre_vtt = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, arr8_t, "_ZTT13AnonEmptyPoly_pre");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTV13AnonEmptyPoly", &dummy_key_vt));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &dummy_key_vt, pre_vt));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtable(&empty_cls, &out_lv));
    if (out_lv != pre_vt) return GCC_GEN_BE_ERROR_INTERNAL;
    /* Now empty_cls is in CACHE_VAR, but db0 is NOT in CACHE_VAR -> emit_vtt hits cached_ctc == NULL branch! */
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtt(&empty_cls, &out_lv));
    /* Also test CACHE_STRING hit without secondary CACHE_VAR entry on both vtable and vtt */
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTV13AnonEmptyPoly", pre_vt));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtable(&empty_cls, &out_lv));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTT13AnonEmptyPoly", &dummy_key_vtt));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &dummy_key_vtt, pre_vtt));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtt(&empty_cls, &out_lv));
    if (out_lv != pre_vtt) return GCC_GEN_BE_ERROR_INTERNAL;

    /* __SO__ subobject VTT early exit */
    a_class_type_supplement so_supp = {};
    a_type so_cls = {};
    so_cls.kind = tk_class; so_cls.source_corresp.name = "Sub__SO__Type"; so_cls.variant.class_struct_union.extra_info = &so_supp;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtt(&so_cls, &out_lv));
    if (out_lv != NULL) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Canonical vtable/ctc/vtt mangler callbacks (NULL, empty, non-empty, and existing_ctc hit) */
    gcc_gen_be_singleton_state.canonical_mangle_vtbl = [](a_type_ptr, a_base_class_ptr, a_base_class_ptr) noexcept -> char * { return const_cast<char *>(""); };
    gcc_gen_be_singleton_state.canonical_mangle_vtt = [](a_type_ptr) noexcept -> char * { return const_cast<char *>(""); };
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtt(&empty_cls, &out_lv));

    gcc_gen_be_singleton_state.canonical_mangle_vtbl = [](a_type_ptr, a_base_class_ptr b, a_base_class_ptr) noexcept -> char * {
        return const_cast<char *>(b ? "_ZTC_canon" : "_ZTV_canon");
    };
    gcc_gen_be_singleton_state.canonical_mangle_vtt = [](a_type_ptr) noexcept -> char * {
        return const_cast<char *>("_ZTT_canon");
    };
    GCC_GEN_BE_CHECK( sweep_oom_budgets(80, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_emit_vtt(&empty_cls, &out_lv);
    }) );
    gcc_gen_be_singleton_state.canonical_mangle_vtbl = NULL;
    gcc_gen_be_singleton_state.canonical_mangle_vtt = NULL;

    /* Class 2: Full complex polymorphic hierarchy exercising EVERY routine & secondary vtable branch!
     * - 9 virtual slots (0..8), plus non-virtual and out-of-range routines:
     *   slot 0: pure_virtual #1
     *   slot 1: pure_virtual #2 (hits !pure_virt_fn == FALSE)
     *   slot 2: is_deleted #1
     *   slot 3: is_deleted #2 (hits !deleted_virt_fn == FALSE)
     *   slot 4: thunked with delta != 0 & return_delta != 0
     *   slot 5: thunked with vbase_index != 0
     *   slot 6: thunked with vcall_index != 0
     *   slot 7: normal virtual pre-cached in CACHE_FUNC
     *   slot 8: normal virtual uncached; plus slot 9 missing (filled by __cxa_pure_virtual loop!)
     * - Bases: indirect non-virtual (!is_virtual && !direct), direct non-virtual at offset 8, virtual at offset 16, direct at offset 0
     */
    a_routine r_nonvirt = {}, r_oor = {}, r_pv1 = {}, r_pv2 = {}, r_del1 = {}, r_del2 = {};
    a_routine r_th_cov = {}, r_th_vb = {}, r_th_vc = {}, r_th_d = {}, r_cached = {}, r_uncached = {}, r_target = {};
    r_target.source_corresp.name = "_ZN4Full6targetEv"; r_target.type = &trtype;

    r_nonvirt.number.virtual_function = VIRTUAL_FUNCTION_NUMBER_NONE; r_nonvirt.next = &r_oor;
    r_oor.number.virtual_function = 99; r_oor.next = &r_pv1;
    r_pv1.number.virtual_function = 0; r_pv1.pure_virtual = TRUE; r_pv1.next = &r_pv2;
    r_pv2.number.virtual_function = 1; r_pv2.pure_virtual = TRUE; r_pv2.next = &r_del1;
    r_del1.number.virtual_function = 2; r_del1.is_deleted = TRUE; r_del1.next = &r_del2;
    r_del2.number.virtual_function = 3; r_del2.is_deleted = TRUE; r_del2.next = &r_th_cov;

    r_th_cov.number.virtual_function = 4; r_th_cov.type = &trtype; r_th_cov.overriding_function_for_wrapper = &r_target;
    r_th_cov.delta = -8; r_th_cov.return_delta = 8; r_th_cov.next = &r_th_vb;

    r_th_vb.number.virtual_function = 5; r_th_vb.type = &trtype; r_th_vb.overriding_function_for_wrapper = &r_target;
    r_th_vb.vbase_index = -16; r_th_vb.next = &r_th_vc;

    r_th_vc.number.virtual_function = 6; r_th_vc.type = &trtype; r_th_vc.overriding_function_for_wrapper = &r_target;
    r_th_vc.vcall_index = -24; r_th_vc.next = &r_th_d;

    r_th_d.number.virtual_function = 7; r_th_d.type = &trtype; r_th_d.overriding_function_for_wrapper = &r_target;
    r_th_d.delta = -16; r_th_d.next = &r_cached;

    r_cached.number.virtual_function = 8; r_cached.type = &trtype; r_cached.source_corresp.name = "_ZN4Full6cachedEv"; r_cached.next = &r_uncached;
    r_uncached.number.virtual_function = 9; r_uncached.type = &trtype; r_uncached.source_corresp.name = "_ZN4Full8uncachedEv";

    a_scope full_sc = {};
    full_sc.routines = &r_nonvirt;

    a_base_class b_indir = {}, b_dir8 = {}, b_virt16 = {}, b_dir0 = {};
    b_indir.type = &b0_tp; b_indir.direct = FALSE; b_indir.is_virtual = FALSE; b_indir.next = &b_dir8;
    b_dir8.type = &b0_tp; b_dir8.direct = TRUE; b_dir8.is_virtual = FALSE; b_dir8.offset = 8; b_dir8.has_public_derivation = TRUE; b_dir8.next = &b_virt16;
    b_virt16.type = &b0_tp; b_virt16.direct = FALSE; b_virt16.is_virtual = TRUE; b_virt16.offset = 16; b_virt16.has_public_derivation = TRUE; b_virt16.next = &b_dir0;
    b_dir0.type = &b0_tp; b_dir0.direct = TRUE; b_dir0.is_virtual = FALSE; b_dir0.offset = 0; b_dir0.has_public_derivation = TRUE;

    a_class_type_supplement full_supp = {};
    full_supp.highest_virtual_function_number = 10; /* slot 10 left NULL -> filled by loop */
    full_supp.assoc_scope = &full_sc;
    full_supp.base_classes = &b_indir;
    full_supp.direct_base_classes = &b_dir8;

    a_type full_cls = {};
    full_cls.kind = tk_class; full_cls.size = 32; full_cls.source_corresp.name = "FullPoly";
    full_cls.variant.class_struct_union.extra_info = &full_supp;

    gcc_jit_param *dummy_p = gcc_jit_context_new_param(ctx, NULL, vp_t, "p");
    gcc_jit_function *pre_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, vp_t, "_ZN4Full6cachedEv", 1, &dummy_p, 0);

    GCC_GEN_BE_CHECK( sweep_oom_budgets(350, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &r_cached, pre_fn));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTC8FullPoly0_14DirectZeroBase", pre_vt));
        return gcc_gen_be_emit_vtt(&full_cls, &out_lv);
    }) );

    /* Also exercise g_mock_lower_fn_null == TRUE (so fn == NULL -> slot filled when pure_virt_fn is initially NULL!) */
    a_routine r_nullfn = {};
    r_nullfn.number.virtual_function = 0; r_nullfn.type = &trtype; r_nullfn.source_corresp.name = "_ZN4Null2fnEv";
    a_scope nullfn_sc = {}; nullfn_sc.routines = &r_nullfn;
    a_class_type_supplement nullfn_supp = {};
    nullfn_supp.highest_virtual_function_number = 0;
    nullfn_supp.assoc_scope = &nullfn_sc;
    a_type nullfn_cls = {};
    nullfn_cls.kind = tk_class; nullfn_cls.size = 8; nullfn_cls.source_corresp.name = "NullFnCls";
    nullfn_cls.variant.class_struct_union.extra_info = &nullfn_supp;

    GCC_GEN_BE_CHECK(cache_clear_all());
    g_mock_lower_fn_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtable(&nullfn_cls, &out_lv));
    g_mock_lower_fn_null = FALSE;

    GCC_GEN_BE_CHECK(cache_clear_all());
    g_mock_lower_fn_fail = TRUE;
    if (gcc_gen_be_emit_vtable(&nullfn_cls, &out_lv) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_lower_fn_fail = FALSE;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises `dynamic_cast`, `typeid`, and `_ZTI`/`_ZTS` type descriptor emission (LP64 & `-m32`).
 * @details Covers `dynamic_cast<void*>`, pointer/reference `dynamic_cast`, all `src2dst` hint cases,
 * static & dynamic `typeid` (lvalue vs rvalue, null block), and all Itanium RTTI descriptor shapes
 * (`__fundamental`, `__enum`, `__array`, `__function`, `__pointer`, `__pointer_to_member`,
 * `__class`, `__si_class`, and `__vmi_class` under both 64-bit and `-m32`), plus OOM sweeps.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_dynamic_cast_typeid_and_descriptors(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_rvalue *out_rv = NULL;
    gcc_jit_lvalue *out_lv = NULL;

    gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *host_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "rtti_test_host", 0, NULL, 0);
    gcc_jit_lvalue *local_obj = gcc_jit_function_new_local(host_fn, NULL, vp_t, "local_obj");

    a_type void_tp = {}, int_tp = {}, enum_tp = {}, flt_tp = {}, arr_tp = {}, fn_tp = {};
    void_tp.kind = tk_void;
    int_tp.kind = tk_integer; int_tp.size = 4; int_tp.variant.integer.int_kind = ik_int;
    enum_tp.kind = tk_integer; enum_tp.size = 4; enum_tp.variant.integer.enum_type = TRUE; enum_tp.source_corresp.name = "Color";
    flt_tp.kind = tk_float; flt_tp.size = 8; flt_tp.variant.float_kind = fk_double;
    arr_tp.kind = tk_array; arr_tp.variant.array.variant.number_of_elements = 4; arr_tp.variant.array.element_type = &int_tp;
    fn_tp.kind = tk_routine; fn_tp.variant.routine.return_type = &void_tp;

    a_type tref_void = {}, ptr_void = {}, tref_ptr_void = {}, ptr_null = {};
    tref_void.kind = tk_typeref; tref_void.variant.typeref.type = &void_tp;
    ptr_void.kind = tk_pointer; ptr_void.size = 8; ptr_void.variant.pointer.type = &tref_void;
    tref_ptr_void.kind = tk_typeref; tref_ptr_void.variant.typeref.type = &ptr_void;
    ptr_null.kind = tk_pointer; ptr_null.size = 8; ptr_null.variant.pointer.type = NULL;

    /* Invalid dynamic_cast args */
    an_expr_node dc_expr = {}, dc_src = {};
    if (gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    dc_expr.type = &tref_ptr_void;
    if (gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    dc_src.type = &ptr_null;
    dc_expr.variant.operation.operands = &dc_src;

    /* dynamic_cast<void*> with NULL block -> INVALID_STATE, rvalue failure, then valid block + OOM sweep */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_expr_rval_fail = TRUE;
    if (gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_expr_rval_fail = FALSE;

    GCC_GEN_BE_CHECK( sweep_oom_budgets(25, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_jit_block *blk = gcc_jit_function_new_block(host_fn, "dc_void_blk");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));
        return gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv);
    }) );

    /* Reference dynamic_cast & compute_dynamic_cast_hint edge cases */
    a_type cls_noctsp = {}, cls_priv = {}, base_priv = {};
    cls_noctsp.kind = tk_struct; cls_noctsp.source_corresp.name = "NoCtsp";
    base_priv.kind = tk_class; base_priv.source_corresp.name = "PrivBase";
    a_base_class bc_priv = {}, bc_other = {};
    bc_priv.type = &base_priv; bc_priv.direct = TRUE; bc_priv.offset = 0; bc_priv.has_public_derivation = FALSE; bc_priv.next_direct = &bc_other;
    bc_other.type = &cls_noctsp; bc_other.direct = TRUE; bc_other.offset = 8; bc_other.has_public_derivation = TRUE;
    a_class_type_supplement priv_supp = {};
    priv_supp.direct_base_classes = &bc_priv; /* base_classes == NULL -> uses direct_base_classes! */
    cls_priv.kind = tk_class; cls_priv.source_corresp.name = "PrivDerived"; cls_priv.variant.class_struct_union.extra_info = &priv_supp;

    a_type tref_base_priv = {}, ptr_tref_base = {}, ref_cls_priv = {}, tref_ref_priv = {}, ptr_noctsp = {}, ptr_int = {};
    tref_base_priv.kind = tk_typeref; tref_base_priv.variant.typeref.type = &base_priv;
    ptr_tref_base.kind = tk_pointer; ptr_tref_base.variant.pointer.type = &tref_base_priv;
    tref_ref_priv.kind = tk_typeref; tref_ref_priv.variant.typeref.type = &cls_priv;
    ref_cls_priv.kind = tk_pointer; ref_cls_priv.variant.pointer.is_reference = TRUE; ref_cls_priv.variant.pointer.type = &tref_ref_priv;
    ptr_noctsp.kind = tk_pointer; ptr_noctsp.variant.pointer.type = &cls_noctsp;
    ptr_int.kind = tk_pointer; ptr_int.variant.pointer.type = &int_tp;

    /* Reference dynamic_cast with NULL cblock -> INVALID_STATE */
    dc_src.type = &ptr_tref_base;
    dc_expr.type = &ref_cls_priv;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Valid block: run twice (first imports __dynamic_cast & __cxa_bad_cast, second hits CACHE_FUNC for both!) + OOM sweep */
    GCC_GEN_BE_CHECK( sweep_oom_budgets(120, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_jit_block *blk = gcc_jit_function_new_block(host_fn, "dc_ref_blk");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv));
        return gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv);
    }) );

    /* Hint edge cases: unrelated class (match_count == 0), ctsp == NULL, non-class dst, non-pointer src/dst */
    dc_src.type = &int_tp; dc_expr.type = &ptr_noctsp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv));
    dc_src.type = &ptr_noctsp; dc_expr.type = &ptr_int;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv));
    dc_src.type = &ptr_int; dc_expr.type = &cls_priv;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv));

    /* gcc_gen_be_lower_typeid: invalid args, dynamic (NULL operand, NULL block, lval != NULL, lval == NULL, cached __cxa_bad_typeid), static */
    an_expr_node ti_expr = {}, ti_op = {}, ti_arg = {};
    if (gcc_gen_be_lower_typeid(&ti_expr, &out_rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    ti_op.kind = enk_constant;
    ti_expr.variant.typeid_info.type_with_opt_expr = &ti_op;
    if (gcc_gen_be_lower_typeid(&ti_expr, &out_rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    ti_op.kind = enk_type_operand;
    ti_op.variant.type_operand.type = NULL;
    if (gcc_gen_be_lower_typeid(&ti_expr, &out_rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    ti_op.variant.type_operand.type = &cls_priv;
    ti_expr.variant.typeid_info.is_dynamic = TRUE;
    if (gcc_gen_be_lower_typeid(&ti_expr, &out_rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    ti_op.next = &ti_arg;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_typeid(&ti_expr, &out_rv) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_block *ti_blk = gcc_jit_function_new_block(host_fn, "ti_err_blk");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(ti_blk));
    g_mock_expr_lval_fail = TRUE;
    if (gcc_gen_be_lower_typeid(&ti_expr, &out_rv) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_expr_lval_fail = FALSE;
    g_mock_expr_rval_fail = TRUE;
    if (gcc_gen_be_lower_typeid(&ti_expr, &out_rv) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_expr_rval_fail = FALSE;

    GCC_GEN_BE_CHECK( sweep_oom_budgets(40, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_jit_block *blk = gcc_jit_function_new_block(host_fn, "ti_dyn_blk");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));
        g_mock_expr_lval = local_obj;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_typeid(&ti_expr, &out_rv));
        g_mock_expr_lval = NULL;
        return gcc_gen_be_lower_typeid(&ti_expr, &out_rv);
    }) );

    /* gcc_gen_be_emit_type_descriptor: reference/typeref unwrapping, typeinfo_var, canonical hooks, cache paths, all RTTI kinds */
    a_type ref_to_null = {}, tref_ref_null = {};
    tref_ref_null.kind = tk_typeref; tref_ref_null.variant.typeref.type = NULL;
    ref_to_null.kind = tk_pointer; ref_to_null.variant.pointer.is_reference = TRUE; ref_to_null.variant.pointer.type = &tref_ref_null;
    if (gcc_gen_be_emit_type_descriptor(&ref_to_null, &out_lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    /* tp->typeinfo_var path (returning NULL and non-NULL) */
    a_variable mock_ti_var = {};
    a_type tp_with_var = {};
    tp_with_var.kind = tk_class; tp_with_var.source_corresp.name = "HasTypeinfoVar"; tp_with_var.typeinfo_var = &mock_ti_var;
    g_mock_global_var_lval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&tp_with_var, &out_lv));

    /* Fundamental, enum, array, function, anonymous class, union */
    a_namespace anon_ns = {}; anon_ns.source_corresp.name = NULL;
    a_scope anon_sc = {}; anon_sc.kind = sck_namespace; anon_sc.variant.assoc_namespace = &anon_ns;
    a_type anon_union = {};
    anon_union.kind = tk_union; anon_union.source_corresp.name = "AnonU"; anon_union.source_corresp.parent_scope = &anon_sc;

    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&flt_tp, &out_lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&enum_tp, &out_lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&arr_tp, &out_lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&fn_tp, &out_lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&anon_union, &out_lv));

    /* Canonical typeinfo / typeinfo_str hooks (NULL, "", non-_ZTS, "_ZTS", valid) + pre-seeded cache paths */
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo_str = [](a_type_ptr) noexcept -> char * { return const_cast<char *>(""); };
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo = [](a_type_ptr) noexcept -> char * { return const_cast<char *>(""); };
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&flt_tp, &out_lv));

    gcc_gen_be_singleton_state.canonical_mangle_typeinfo_str = [](a_type_ptr) noexcept -> char * { return const_cast<char *>("raw_zts_str"); };
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo = [](a_type_ptr) noexcept -> char * { return const_cast<char *>("_ZTI_canon_d"); };
    GCC_GEN_BE_CHECK( sweep_oom_budgets(35, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_emit_type_descriptor(&flt_tp, &out_lv);
    }) );
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo_str = [](a_type_ptr) noexcept -> char * { return const_cast<char *>("_ZTS"); };
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&flt_tp, &out_lv));
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo_str = NULL;
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo = NULL;

    /* Pre-seeded CACHE_STRING -> CACHE_VAR indirection for _ZTI, _ZTS, and _ZTVN10__cxxabiv1... */
    gcc_jit_type *arr8_t = gcc_jit_context_new_array_type(ctx, NULL, vp_t, 8);
    gcc_jit_lvalue *pre_zti = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, arr8_t, "pre_zti_g");
    gcc_jit_lvalue *pre_zts = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, arr8_t, "pre_zts_g");
    gcc_jit_lvalue *pre_tv = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, arr8_t, "pre_tv_g");
    int k_zti = 11, k_zts = 22, k_tv = 33;

    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTId", &k_zti));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &k_zti, pre_zti));
    GCC_GEN_BE_CHECK( sweep_oom_budgets(15, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTId", &k_zti));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &k_zti, pre_zti));
        return gcc_gen_be_emit_type_descriptor(&flt_tp, &out_lv);
    }) );
    /* Also CACHE_STRING hit on _ZTId WITHOUT CACHE_VAR mapping */
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTId", pre_zti));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&flt_tp, &out_lv));

    /* CACHE_STRING hit on _ZTSd (with & without CACHE_VAR) and tv_sym (with & without CACHE_VAR) */
    GCC_GEN_BE_CHECK( sweep_oom_budgets(30, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTSd", &k_zts));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &k_zts, pre_zts));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTVN10__cxxabiv123__fundamental_type_infoE", &k_tv));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &k_tv, pre_tv));
        return gcc_gen_be_emit_type_descriptor(&flt_tp, &out_lv);
    }) );
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTSd", pre_zts));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTVN10__cxxabiv123__fundamental_type_infoE", &k_tv));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&flt_tp, &out_lv));

    /* Pointer & pointer-to-member descriptors: CVR flags, unqualified, NULL pointee, NULL/non-NULL sub-descriptors */
    a_type cvr_int = {}, ptr_cvr = {}, ptr_unqual = {}, ptr_var_null = {};
    cvr_int.kind = tk_typeref; cvr_int.variant.typeref.qualifiers = TQ_CONST | TQ_VOLATILE | TQ_RESTRICT; cvr_int.variant.typeref.type = &int_tp;
    ptr_cvr.kind = tk_pointer; ptr_cvr.variant.pointer.type = &cvr_int;
    ptr_unqual.kind = tk_pointer; ptr_unqual.variant.pointer.type = &int_tp;
    ptr_var_null.kind = tk_pointer; ptr_var_null.variant.pointer.type = &tp_with_var;

    GCC_GEN_BE_CHECK( sweep_oom_budgets(40, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_emit_type_descriptor(&ptr_cvr, &out_lv);
    }) );
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&ptr_unqual, &out_lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&ptr_null, &out_lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&ptr_var_null, &out_lv));

    a_type ptm_full = {}, ptm_empty = {}, ptm_var_null = {};
    ptm_full.kind = tk_ptr_to_member; ptm_full.variant.ptr_to_member.type = &int_tp; ptm_full.variant.ptr_to_member.class_of_which_a_member = &cls_noctsp;
    ptm_empty.kind = tk_ptr_to_member;
    ptm_var_null.kind = tk_ptr_to_member; ptm_var_null.variant.ptr_to_member.type = &tp_with_var; ptm_var_null.variant.ptr_to_member.class_of_which_a_member = &tp_with_var;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(60, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_emit_type_descriptor(&ptm_full, &out_lv);
    }) );
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&ptm_empty, &out_lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&ptm_var_null, &out_lv));

    /* __si_class_type_info (with valid base_desc and NULL base_desc) */
    a_base_class si_b1 = {}, si_bnull = {};
    si_b1.type = &cls_noctsp; si_b1.direct = TRUE; si_b1.is_virtual = FALSE; si_b1.offset = 0; si_b1.has_public_derivation = TRUE;
    si_bnull.type = &tp_with_var; si_bnull.direct = TRUE; si_bnull.is_virtual = FALSE; si_bnull.offset = 0; si_bnull.has_public_derivation = TRUE;
    a_class_type_supplement si_supp1 = {}, si_supp2 = {};
    si_supp1.direct_base_classes = &si_b1;
    si_supp2.direct_base_classes = &si_bnull;
    a_type si_cls1 = {}, si_cls2 = {};
    si_cls1.kind = tk_class; si_cls1.source_corresp.name = "SiDerived1"; si_cls1.variant.class_struct_union.extra_info = &si_supp1;
    si_cls2.kind = tk_class; si_cls2.source_corresp.name = "SiDerived2"; si_cls2.variant.class_struct_union.extra_info = &si_supp2;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(45, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_emit_type_descriptor(&si_cls1, &out_lv);
    }) );
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&si_cls2, &out_lv));

    /* __vmi_class_type_info:
     * 1) vmi_flags == 0 (2 distinct non-virtual bases, offset_flags == 0 on private offset-0 base, NULL vb->type, NULL vb_desc)
     *    tested under BOTH LP64 (gcc_be_m32 == FALSE) and ILP32 (gcc_be_m32 == TRUE)!
     * 2) vmi_flags == 0x3 (virtual base + repeated non-virtual base in base_classes) under LP64 and ILP32!
     */
    a_base_class vb_priv0 = {}, vb_nulltp = {}, vb_nulldesc = {};
    vb_priv0.type = &cls_noctsp; vb_priv0.direct = TRUE; vb_priv0.is_virtual = FALSE; vb_priv0.offset = 0; vb_priv0.has_public_derivation = FALSE; vb_priv0.next_direct = &vb_nulltp;
    vb_nulltp.type = NULL; vb_nulltp.direct = TRUE; vb_nulltp.is_virtual = FALSE; vb_nulltp.offset = 8; vb_nulltp.has_public_derivation = TRUE; vb_nulltp.next_direct = &vb_nulldesc;
    vb_nulldesc.type = &tp_with_var; vb_nulldesc.direct = TRUE; vb_nulldesc.is_virtual = FALSE; vb_nulldesc.offset = 16; vb_nulldesc.has_public_derivation = TRUE;
    a_class_type_supplement vmi0_supp = {};
    vmi0_supp.direct_base_classes = &vb_priv0; /* base_classes == NULL, vmi_flags == 0 */
    a_type vmi0_cls = {};
    vmi0_cls.kind = tk_class; vmi0_cls.source_corresp.name = "VmiZeroFlags"; vmi0_cls.variant.class_struct_union.extra_info = &vmi0_supp;

    GCC_GEN_BE_CHECK( sweep_oom_budgets(60, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_emit_type_descriptor(&vmi0_cls, &out_lv);
    }) );
    gcc_be_m32 = TRUE;
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&vmi0_cls, &out_lv));
    gcc_be_m32 = FALSE;

    a_base_class vmi_b1 = {}, vmi_b2_virt = {}, vmi_b3_dup = {};
    vmi_b1.type = &cls_noctsp; vmi_b1.direct = TRUE; vmi_b1.is_virtual = FALSE; vmi_b1.offset = 0; vmi_b1.has_public_derivation = TRUE;
    vmi_b1.next = &vmi_b2_virt; vmi_b1.next_direct = &vmi_b2_virt;
    vmi_b2_virt.type = &base_priv; vmi_b2_virt.direct = TRUE; vmi_b2_virt.is_virtual = TRUE; vmi_b2_virt.offset = 8; vmi_b2_virt.has_public_derivation = TRUE;
    vmi_b2_virt.next = &vmi_b3_dup; vmi_b2_virt.next_direct = NULL;
    vmi_b3_dup.type = &cls_noctsp; vmi_b3_dup.direct = FALSE; vmi_b3_dup.is_virtual = FALSE; vmi_b3_dup.offset = 16;

    a_class_type_supplement vmi3_supp = {};
    vmi3_supp.direct_base_classes = &vmi_b1;
    vmi3_supp.base_classes = &vmi_b1;
    a_type vmi3_cls = {};
    vmi3_cls.kind = tk_class; vmi3_cls.source_corresp.name = "VmiDiamondRepeat";
    vmi3_cls.variant.class_struct_union.any_virtual_base_classes = TRUE;
    vmi3_cls.variant.class_struct_union.extra_info = &vmi3_supp;

    gcc_be_m32 = TRUE;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(60, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_emit_type_descriptor(&vmi3_cls, &out_lv);
    }) );
    gcc_be_m32 = FALSE;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises remaining edge-case branches in mangling, vtables, thunks, dynamic_cast, and descriptors.
 * @details Covers file-parented classes/functions, lambda loop exhaustion, canonical mangler NULL returns,
 * `CACHE_STRING` OOM sweeps, `use_all_bases == FALSE` indirect skips, `p_gcc_jit_rvalue_set_bool_require_tail_call == NULL`,
 * `vcall_thunk` / `covariant_return_thunk` edge args, all `compute_dynamic_cast_hint` paths, and private single-base VMI.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_remaining_edge_branches(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    char buf[512];
    gcc_jit_lvalue *out_lv = NULL;
    gcc_jit_function *out_fn = NULL;
    gcc_jit_rvalue *out_rv = NULL;

    a_scope file_sc = {};
    file_sc.kind = sck_file;
    a_type int_tp = {};
    int_tp.kind = tk_integer; int_tp.size = 4; int_tp.variant.integer.int_kind = ik_int;

    /* 1. tk_class with file-scope parent */
    a_type file_cls = {};
    file_cls.kind = tk_class;
    file_cls.source_corresp.name = "FileScopedCls";
    file_cls.source_corresp.parent_scope = &file_sc;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&file_cls, buf, sizeof(buf)));

    /* 2. Lambda closure class whose routines list exhausts without matching operator() */
    a_routine nonop_r = {};
    nonop_r.special_kind = (a_special_function_kind)sfk_none;
    a_scope lam_sc = {};
    lam_sc.routines = &nonop_r;
    a_class_type_supplement lam_supp = {};
    lam_supp.is_lambda_closure_class = TRUE;
    lam_supp.assoc_scope = &lam_sc;
    a_type lam_tp = {};
    lam_tp.kind = tk_class;
    lam_tp.variant.class_struct_union.extra_info = &lam_supp;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name(&lam_tp, buf, sizeof(buf)));

    /* 3. clone_canonical_fn_with_variant with C3 and C1X */
    a_routine fn_rout = {};
    fn_rout.source_corresp.name = "_ZN6WidgetC3Ev";
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&fn_rout, "C2", buf, sizeof(buf)));
    fn_rout.source_corresp.name = "_ZN6WidgetC1XEv";
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&fn_rout, "C2", buf, sizeof(buf)));

    /* 4. Function mangling: file-scope parent, !ptp && rtsp && !has_ellipsis, and 2-param OOM sweep */
    a_routine_type_supplement rts0 = {}, rts2 = {};
    rts0.param_type_list = NULL;
    rts0.has_ellipsis = FALSE;
    a_type rt0 = {};
    rt0.kind = tk_routine; rt0.variant.routine.return_type = &int_tp; rt0.variant.routine.extra_info = &rts0;
    fn_rout = {};
    fn_rout.source_corresp.name = "fileScopedFn";
    fn_rout.source_corresp.parent_scope = &file_sc;
    fn_rout.type = &rt0;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(&fn_rout, NULL, buf, sizeof(buf)));

    a_param_type fp1 = {}, fp2 = {};
    fp1.type = &int_tp; fp1.next = &fp2;
    fp2.type = &file_cls;
    rts2.param_type_list = &fp1;
    a_type rt2 = {};
    rt2.kind = tk_routine; rt2.variant.routine.return_type = &int_tp; rt2.variant.routine.extra_info = &rts2;
    fn_rout.type = &rt2;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(30, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_mangle_function_name(&fn_rout, NULL, buf, sizeof(buf));
    }) );

    /* 5. Canonical mangler callbacks returning NULL */
    gcc_gen_be_singleton_state.canonical_mangle_vtbl = [](a_type_ptr, a_base_class_ptr, a_base_class_ptr) noexcept -> char * { return NULL; };
    gcc_gen_be_singleton_state.canonical_mangle_vtt = [](a_type_ptr) noexcept -> char * { return NULL; };
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo_str = [](a_type_ptr) noexcept -> char * { return NULL; };
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo = [](a_type_ptr) noexcept -> char * { return NULL; };

    /* 6. Class with num_funcs=1, assoc_scope=NULL, base_classes=NULL, direct_base_classes: (!is_virtual && !direct) -> virtual offset 8 */
    a_class_type_supplement b_supp = {}, c_supp = {};
    a_type base_cls = {}, no_assoc_cls = {};
    base_cls.kind = tk_class; base_cls.size = 8; base_cls.source_corresp.name = "EdgeBase"; base_cls.variant.class_struct_union.extra_info = &b_supp;
    a_base_class bc_skip = {}, bc_v8 = {};
    bc_skip.type = &base_cls; bc_skip.direct = FALSE; bc_skip.is_virtual = FALSE; bc_skip.next_direct = &bc_v8;
    bc_v8.type = &base_cls; bc_v8.direct = TRUE; bc_v8.is_virtual = TRUE; bc_v8.offset = 8; bc_v8.has_public_derivation = TRUE;
    c_supp.highest_virtual_function_number = 0;
    c_supp.assoc_scope = NULL;
    c_supp.direct_base_classes = &bc_skip;
    no_assoc_cls.kind = tk_class; no_assoc_cls.size = 16; no_assoc_cls.source_corresp.name = "NoAssocPoly";
    no_assoc_cls.variant.class_struct_union.extra_info = &c_supp;

    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtt(&no_assoc_cls, &out_lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&base_cls, &out_lv));

    gcc_gen_be_singleton_state.canonical_mangle_vtbl = NULL;
    gcc_gen_be_singleton_state.canonical_mangle_vtt = NULL;
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo_str = NULL;
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo = NULL;

    /* 7. OOM sweeps on CACHE_STRING hit paths in emit_vtable (lines 1217, 1224) and emit_vtt (line 1618) */
    gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *arr8_t = gcc_jit_context_new_array_type(ctx, NULL, vp_t, 8);
    gcc_jit_lvalue *pre_vt = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, arr8_t, "_ZTV11NoAssocPoly_pre");
    gcc_jit_lvalue *pre_vtt = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, arr8_t, "_ZTT11NoAssocPoly_pre");
    int k_vt = 77, k_vtt = 88;
    GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTV11NoAssocPoly", &k_vt));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &k_vt, pre_vt));
        return gcc_gen_be_emit_vtable(&no_assoc_cls, &out_lv);
    }) );
    GCC_GEN_BE_CHECK( sweep_oom_budgets(20, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTT11NoAssocPoly", &k_vtt));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &k_vtt, pre_vtt));
        return gcc_gen_be_emit_vtt(&no_assoc_cls, &out_lv);
    }) );

    /* 8. Vtable routines: (delta==0 && vcall_index==0 && return_delta!=0) and (overriding != NULL && all 4 offsets == 0) */
    a_param_type tp_null = {};
    tp_null.type = NULL; /* p_orig == NULL in emit_thunk */
    a_routine_type_supplement rts_null = {};
    rts_null.param_type_list = &tp_null;
    a_type rt_null = {};
    rt_null.kind = tk_routine;
    rt_null.variant.routine.return_type = NULL; /* orig_ret_type == NULL in emit_thunk */
    rt_null.variant.routine.extra_info = &rts_null;

    a_routine r_tgt = {}, r_retd_only = {}, r_zero_wrap = {};
    r_tgt.source_corresp.name = "_ZN4Edge3tgtEv"; r_tgt.type = &rt_null;
    r_retd_only.number.virtual_function = 0; r_retd_only.type = &rt_null; r_retd_only.overriding_function_for_wrapper = &r_tgt;
    r_retd_only.return_delta = 8; r_retd_only.next = &r_zero_wrap;
    r_zero_wrap.number.virtual_function = 1; r_zero_wrap.type = &rt_null; r_zero_wrap.overriding_function_for_wrapper = &r_tgt;
    r_zero_wrap.source_corresp.name = "_ZN4Edge4zeroEv";

    a_scope edge_sc = {}; edge_sc.routines = &r_retd_only;
    a_class_type_supplement edge_supp = {};
    edge_supp.highest_virtual_function_number = 1;
    edge_supp.assoc_scope = &edge_sc;
    a_type edge_cls = {};
    edge_cls.kind = tk_class; edge_cls.size = 8; edge_cls.source_corresp.name = "EdgeWrapCls";
    edge_cls.variant.class_struct_union.extra_info = &edge_supp;
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtable(&edge_cls, &out_lv));

    /* 9. Thunk tail-call NULL, vcall_thunk & covariant_return_thunk edge cases */
    p_gcc_jit_rvalue_set_bool_require_tail_call = NULL;
    r_tgt.delta = 8;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_thunk(&r_tgt, &out_fn));
    r_tgt.delta = 0;

    if (gcc_gen_be_emit_vcall_thunk(&base_cls, NULL, &r_tgt, 0, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_covariant_return_thunk(&r_tgt, 0, 0, FALSE, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    /* vbase == NULL, vcall_offset != 0, rout->vcall_index == 0 -> sets rout->vcall_index */
    r_tgt.vcall_index = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vcall_thunk(&base_cls, NULL, &r_tgt, 24, &out_fn));
    /* vbase == NULL, vcall_offset != 0, rout->vcall_index != 0 */
    r_tgt.vcall_index = 16;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vcall_thunk(&base_cls, NULL, &r_tgt, 24, &out_fn));

    /* 10. compute_dynamic_cast_hint & lower_dynamic_cast & emit_type_descriptor remaining branches */
    a_type tref_null = {}, ptr_to_tref_null = {};
    tref_null.kind = tk_typeref; tref_null.variant.typeref.type = NULL;
    ptr_to_tref_null.kind = tk_pointer; ptr_to_tref_null.variant.pointer.type = &tref_null;

    /* emit_type_descriptor(&tref_null) -> tp becomes NULL at 2416/2417 -> INVALID_ARGUMENT */
    if (gcc_gen_be_emit_type_descriptor(&tref_null, &out_lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    an_expr_node dc_op = {}, dc_expr = {};
    dc_op.kind = enk_operation;
    dc_expr.kind = enk_operation;
    dc_expr.variant.operation.operands = &dc_op;

    /* dest_type_stripped becomes NULL (2108/2112), then src_typeid fails cleanly when dc_op.type == NULL */
    dc_op.type = NULL;
    dc_expr.type = &tref_null;
    if (gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    /* pointee becomes NULL via tref_null (2114/2115), then dst_typeid returns INVALID_ARGUMENT */
    a_type ptr_base = {}, ptr_null = {};
    ptr_base.kind = tk_pointer; ptr_base.variant.pointer.type = &base_cls;
    ptr_null.kind = tk_pointer; ptr_null.variant.pointer.type = NULL;
    dc_op.type = &ptr_base;
    dc_expr.type = &ptr_to_tref_null;
    if (gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    /* With ptr_null (pointer.type == NULL), emit_type_descriptor succeeds and compute_dynamic_cast_hint sees d == NULL and s == NULL! */
    dc_op.type = &ptr_base;
    dc_expr.type = &ptr_null;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv));

    dc_op.type = &ptr_null;
    dc_expr.type = &ptr_base;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv));

    /* Hint with s = typeref->ptr->base_cls, d = typeref->ptr->hint_cls with base_classes != NULL:
     * 1) single public non-virtual base (matched_off = 8)
     * 2) single virtual base (-2)
     * 3) duplicate base (match_count == 2 -> -2)
     */
    a_type base2_cls = {}, hint_cls = {}, tref_ptr_base = {}, ptr_base2 = {}, ptr_hint = {}, tref_ptr_hint = {};
    a_class_type_supplement b2_supp = {}, hint_supp = {};
    base2_cls.kind = tk_class; base2_cls.source_corresp.name = "HintBase2"; base2_cls.variant.class_struct_union.extra_info = &b2_supp;
    a_base_class hb1 = {}, hb2 = {}, hb_dup = {};
    hb1.type = &base_cls; hb1.direct = TRUE; hb1.is_virtual = FALSE; hb1.has_public_derivation = TRUE; hb1.offset = 8;
    hb1.next = &hb2; hb1.next_direct = &hb2;
    hb2.type = &base2_cls; hb2.direct = TRUE; hb2.is_virtual = TRUE; hb2.has_public_derivation = TRUE; hb2.offset = 16;

    hint_supp.direct_base_classes = &hb1;
    hint_supp.base_classes = &hb1;
    hint_cls.kind = tk_class; hint_cls.source_corresp.name = "HintDerived"; hint_cls.variant.class_struct_union.extra_info = &hint_supp;

    tref_ptr_base.kind = tk_typeref; tref_ptr_base.variant.typeref.type = &ptr_base;
    ptr_base2.kind = tk_pointer; ptr_base2.variant.pointer.type = &base2_cls;
    ptr_hint.kind = tk_pointer; ptr_hint.variant.pointer.type = &hint_cls;
    tref_ptr_hint.kind = tk_typeref; tref_ptr_hint.variant.typeref.type = &ptr_hint;

    dc_op.type = &tref_ptr_base;
    dc_expr.type = &tref_ptr_hint;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv)); /* single public non-virtual -> hint 8 */

    dc_op.type = &ptr_base2;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv)); /* single virtual -> hint -2 */

    hb_dup.type = &base_cls; hb_dup.direct = TRUE; hb_dup.has_public_derivation = TRUE; hb_dup.offset = 24;
    hb2.next = &hb_dup; hb2.next_direct = &hb_dup;
    dc_op.type = &tref_ptr_base;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_cast(&dc_expr, &out_rv)); /* duplicate -> hint -2 */

    /* 11. Single private direct non-virtual base at offset 0 -> VMI descriptor (2639:83 False) */
    a_base_class single_priv = {};
    single_priv.type = &base_cls; single_priv.direct = TRUE; single_priv.is_virtual = FALSE; single_priv.offset = 0; single_priv.has_public_derivation = FALSE;
    a_class_type_supplement sp_supp = {};
    sp_supp.direct_base_classes = &single_priv;
    a_type sp_cls = {};
    sp_cls.kind = tk_class; sp_cls.source_corresp.name = "SinglePrivBaseCls"; sp_cls.variant.class_struct_union.extra_info = &sp_supp;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(&sp_cls, &out_lv));

    /* 12. Cover abi_fn resolution on both cached (1350:25 True) and freshly-lowered (1361:29 True) vtable slots */
    {
        gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_function *cached_vfn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, vp_t, "_ZN9AbiFnTest2v1Ev", 0, NULL, 0);
        gcc_jit_function *cached_abi = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, vp_t, "_ZN9AbiFnTest2v1Ev_abi", 0, NULL, 0);
        a_routine vr_cached = {}, vr_fresh = {};
        vr_cached.is_virtual = TRUE; vr_cached.number.virtual_function = 1; vr_cached.source_corresp.name = "v1"; vr_cached.next = &vr_fresh;
        vr_fresh.is_virtual = TRUE; vr_fresh.number.virtual_function = 2; vr_fresh.source_corresp.name = "v2";
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &vr_cached, cached_vfn));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, cached_vfn, cached_abi));

        a_scope ab_sc = {}; ab_sc.kind = sck_class_struct_union; ab_sc.routines = &vr_cached;
        a_class_type_supplement ab_supp = {}; ab_supp.assoc_scope = &ab_sc; ab_supp.highest_virtual_function_number = 2;
        a_type ab_cls = {}; ab_cls.kind = tk_class; ab_cls.size = 8; ab_cls.source_corresp.name = "AbiFnVtblCls";
        ab_cls.variant.class_struct_union.any_virtual_functions = TRUE;
        ab_cls.variant.class_struct_union.extra_info = &ab_supp;

        g_mock_bind_abi_fn_on_lower = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtable(&ab_cls, &out_lv));
        g_mock_bind_abi_fn_on_lower = FALSE;
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Runs all RTTI, VTable, VTT, Thunk, and OOM unit tests.
 * @details Orchestrates mangling, VTable/VTT/thunk, and dynamic_cast/typeid/descriptor test suites.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
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

    gcc_gen_be_error_t perr = test_mangling_types_and_exprs();
    if (perr != GCC_GEN_BE_SUCCESS) { std::fprintf(stderr, "phase 1 failed: %d\n", (int)perr); return perr; }
    perr = test_mangling_functions();
    if (perr != GCC_GEN_BE_SUCCESS) { std::fprintf(stderr, "phase 2 failed: %d\n", (int)perr); return perr; }

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(cache_clear_all());

    p_gcc_jit_rvalue_set_bool_require_tail_call = [](struct gcc_jit_rvalue *c, int req) noexcept {
        (void)c;
        (void)req;
    };
    perr = test_vtables_vtt_and_thunks(ctx);
    p_gcc_jit_rvalue_set_bool_require_tail_call = NULL;
    if (perr != GCC_GEN_BE_SUCCESS) { std::fprintf(stderr, "phase 3 failed: %d\n", (int)perr); return perr; }

    perr = test_dynamic_cast_typeid_and_descriptors(ctx);
    if (perr != GCC_GEN_BE_SUCCESS) { std::fprintf(stderr, "phase 4 failed: %d\n", (int)perr); return perr; }

    perr = test_remaining_edge_branches(ctx);
    if (perr != GCC_GEN_BE_SUCCESS) { std::fprintf(stderr, "phase 5 failed: %d\n", (int)perr); return perr; }

    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for the RTTI full unit test executable.
 * @details Runs `run_rtti_full_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_rtti_full_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
