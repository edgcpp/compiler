/**
 * @file test_gcc_be_eh.cpp
 * @brief Exhaustive unit tests for Exception Handling lowering (`gcc_gen_be_eh.c`).
 * @details Exercises ULEB128/SLEB128 encoders, Itanium C++ ABI throw/rethrow/try/catch/noexcept/
 * dynamic-exception-spec lowering, LSDA/SEH/ARM-EHABI table synthesis, invoke trampolines,
 * partial array/subobject/scope unwind cleanups, optional `libgccjit` GIMPLE EH entrypoints,
 * and deterministic OOM fault injection across every function, line, region, and branch of `src/gcc_gen_be_eh.c`.
 * @ingroup gcc_gen_be_tests
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for EH unit tests.
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
/** @brief Primary source file name. */
a_const_char *primary_source_file_name = "test_eh.cpp";
/** @brief Innermost function scope pointer. */
a_scope_ptr innermost_function_scope = NULL;
/** @brief Memory region table pointer. */
a_mem_block_header_ptr *mem_region_table = NULL;
/** @brief Mock IL header. */
an_il_header il_header;

/** @brief Configured error returned by `gcc_gen_be_lower_statement` stub. */
static gcc_gen_be_error_t g_stmt_err = GCC_GEN_BE_SUCCESS;
/** @brief Statement pointer on which `gcc_gen_be_lower_statement` returns `g_stmt_err`. */
static a_statement_ptr g_fail_stmt = NULL;
/** @brief Flag instructing `gcc_gen_be_lower_statement` to clear `current_block`. */
static a_boolean g_stmt_clears_block = FALSE;
/** @brief Configured rvalue returned by `gcc_gen_be_lower_expr_rvalue` stub. */
static gcc_jit_rvalue *g_expr_rval = NULL;
/** @brief Configured error returned by `gcc_gen_be_lower_expr_rvalue` stub. */
static gcc_gen_be_error_t g_expr_err = GCC_GEN_BE_SUCCESS;
/** @brief Flag instructing `gcc_gen_be_lower_expr_rvalue` to clear `current_block`. */
static a_boolean g_expr_clears_block = FALSE;
/** @brief Configured error returned by `gcc_gen_be_lower_dynamic_init` stub. */
static gcc_gen_be_error_t g_dyn_err = GCC_GEN_BE_SUCCESS;
/** @brief Flag instructing `gcc_gen_be_lower_dynamic_init` to clear `current_block`. */
static a_boolean g_dyn_clears_block = FALSE;
/** @brief Mode for `gcc_gen_be_lower_function_decl` stub: -1 -> NULL, 0 -> 0 params, 1 -> 1 param, 2 -> 2 params (VTT). */
static int g_fn_param_mode = 1;
/** @brief Optional landing pad pushed inside `gcc_gen_be_lower_expr_rvalue` to test `splice_landing_pad_at` shift. */
static gcc_jit_block *g_push_lp_in_expr = NULL;
/** @brief Flag instructing `gcc_gen_be_lower_expr_rvalue` to pop all landing pads to test `splice_landing_pad_at` underflow. */
static a_boolean g_pop_all_lp_in_expr = FALSE;

/** @brief Flag instructing `gcc_gen_be_lower_statement` to push a temporary DIP cleanup landing pad inside a try body. */
static a_boolean g_push_dip_in_stmt = FALSE;

/**
 * @brief Stub for statement lowering in EH unit tests.
 * @details Steps OOM budget, optionally clears `current_block`, and returns `g_stmt_err` if `stmt == g_fail_stmt`.
 * @param[in] stmt Statement pointer.
 * @param[in] func JIT function.
 * @return `GCC_GEN_BE_SUCCESS` or configured error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_fail_stmt && stmt == g_fail_stmt) {
        return g_stmt_err;
    }
    if (g_push_dip_in_stmt && func) {
        GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(gcc_jit_function_new_block(func, "dip_try_lp")));
        gcc_gen_be_singleton_state.eh_cleanup_dip_depth++;
    }
    if (g_stmt_clears_block) {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression rvalue lowering in EH unit tests.
 * @details Steps OOM budget, optionally clears `current_block`, writes `g_expr_rval`, and returns `g_expr_err`.
 * @param[in] expr Expression pointer.
 * @param[out] out_rval Output rvalue.
 * @return `GCC_GEN_BE_SUCCESS` or configured error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_push_lp_in_expr) {
        GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(g_push_lp_in_expr));
    }
    if (g_pop_all_lp_in_expr) {
        while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
            GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
        }
    }
    if (g_expr_clears_block) {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    }
    if (out_rval) *out_rval = g_expr_rval;
    return g_expr_err;
}

/**
 * @brief Stub for dynamic init lowering in EH unit tests.
 * @details Steps OOM budget, optionally clears `current_block`, and returns `g_dyn_err`.
 * @param[in] dip Dynamic init pointer.
 * @param[in] target_lval Target lvalue.
 * @param[out] out_rval Output rvalue.
 * @return `GCC_GEN_BE_SUCCESS` or configured error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_init(a_dynamic_init_ptr dip, struct gcc_jit_lvalue *target_lval, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)dip;
    (void)target_lval;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_dyn_clears_block) {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    }
    if (out_rval) *out_rval = NULL;
    return g_dyn_err;
}

/**
 * @brief Stub for type lowering in EH unit tests.
 * @details Steps OOM budget and lowers pointer/reference types to `void*` and other types to `int`.
 * @param[in] tp Frontend type pointer.
 * @param[out] out_type Output JIT type.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    while (tp && tp->kind == tk_typeref) tp = tp->variant.typeref.type;
    if (out_type) {
        *out_type = (tp && tp->kind == tk_pointer)
            ? gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR)
            : gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for local variable lowering in EH unit tests.
 * @details Steps OOM budget and creates a stack local matching `var->type`.
 * @param[in] func JIT function.
 * @param[in] var Variable pointer.
 * @param[out] out_lval Output local lvalue.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_jit_type *vt = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(var ? var->type : NULL, &vt));
    if (out_lval) {
        *out_lval = gcc_jit_function_new_local(func, NULL, vt, "catch_param");
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for global variable lowering in EH unit tests.
 * @details Sets `*out_lval` to NULL.
 * @param[in] var Variable pointer.
 * @param[out] out_lval Output lvalue.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (out_lval) *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function declaration lowering in EH unit tests.
 * @details Steps OOM budget and synthesizes an imported function with `g_fn_param_mode` parameters (or NULL if `-1`).
 * @param[in] rout Routine pointer.
 * @param[out] out_func Output function.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (g_fn_param_mode < 0) {
        *out_func = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *vptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_param *ps[2] = { NULL, NULL };
    int cnt = g_fn_param_mode;
    for (int i = 0; i < cnt; ++i) {
        ps[i] = gcc_jit_context_new_param(ctx, NULL, vptr_t, (i == 0) ? "this_p" : "vtt_p");
    }
    static int seq = 0;
    char nm[64];
    std::snprintf(nm, sizeof(nm), "%s_%d", (rout && rout->source_corresp.name) ? rout->source_corresp.name : "mock_dtor", ++seq);
    *out_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_t, nm, cnt, cnt > 0 ? ps : NULL, 0);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for extracting signed integer value from a constant in EH unit tests.
 * @param[in] cp Constant pointer.
 * @param[out] err Overflow flag.
 * @return 0.
 */
a_host_large_integer value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    (void)cp;
    if (err) *err = FALSE;
    return 0;
}

/**
 * @brief Stub for expression lvalue lowering in EH unit tests.
 * @param[in] expr Expression pointer.
 * @param[out] out_lval Output lvalue.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (out_lval) *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for invisible reference check in EH unit tests.
 * @param[in] type Frontend type pointer.
 * @param[out] out_is_invis_ref Output boolean.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_invis_ref) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (!out_is_invis_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_invis_ref = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for COMDAT linkage application in EH unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] global Global lvalue.
 * @param[in] symbol_name Symbol name.
 * @param[in] is_readonly_data Readonly data flag.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_apply_comdat_linkage(
    struct gcc_jit_lvalue *global,
    const char *symbol_name,
    a_boolean is_readonly_data) GCC_GEN_BE_NOEXCEPT {
    (void)global;
    (void)symbol_name;
    (void)is_readonly_data;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}
END_EDG_NAMESPACE

/** @brief Toggle controlling whether `mock_eh_filter` returns a non-null `gcc_jit_rvalue *` or NULL. */
static a_boolean g_eh_filter_nonnull = TRUE;

/**
 * @brief Mock `gcc_jit_lvalue_set_link_section` callback.
 * @param[in] lval Target global lvalue.
 * @param[in] sec Section name.
 */
static void mock_set_link_section(gcc_jit_lvalue *lval, const char *sec) noexcept {
    (void)lval;
    (void)sec;
}

/**
 * @brief Mock `gcc_jit_context_add_top_level_asm` callback.
 * @param[in] ctx Target context.
 * @param[in] loc Source location.
 * @param[in] asm_chars Assembly string.
 */
static void mock_top_asm(gcc_jit_context *ctx, gcc_jit_location *loc, const char *asm_chars) noexcept {
    (void)ctx;
    (void)loc;
    (void)asm_chars;
}

/**
 * @brief Mock `gcc_jit_block_add_try_catch` callback.
 * @param[in] try_b Try block.
 * @param[in] loc Source location.
 * @param[in] catch_b Catch block.
 */
static void mock_add_try_catch(gcc_jit_block *try_b, gcc_jit_location *loc, gcc_jit_block *catch_b) noexcept {
    (void)try_b;
    (void)loc;
    (void)catch_b;
}

/**
 * @brief Mock `gcc_jit_block_add_cleanup` callback.
 * @param[in] blk Block.
 * @param[in] loc Source location.
 * @param[in] cln_b Cleanup block.
 */
static void mock_add_cleanup(gcc_jit_block *blk, gcc_jit_location *loc, gcc_jit_block *cln_b) noexcept {
    (void)blk;
    (void)loc;
    (void)cln_b;
}

/**
 * @brief Mock `gcc_jit_block_end_with_resx` callback.
 * @param[in] blk Block to terminate.
 * @param[in] loc Source location.
 * @param[in] region_idx Region index.
 */
static void mock_end_resx(gcc_jit_block *blk, gcc_jit_location *loc, int region_idx) noexcept {
    (void)loc;
    (void)region_idx;
    gcc_jit_block_end_with_void_return(blk, NULL);
}

/**
 * @brief Mock `gcc_jit_function_set_personality` callback.
 * @param[in] fn Function.
 * @param[in] pers Personality function.
 */
static void mock_set_pers(gcc_jit_function *fn, gcc_jit_function *pers) noexcept {
    (void)fn;
    (void)pers;
}

/**
 * @brief Mock `gcc_jit_block_add_landing_pad` callback.
 * @param[in] blk Landing pad block.
 * @param[in] loc Source location.
 * @param[in] pers Personality function.
 * @param[in] act_idx Action index.
 */
static void mock_add_lp(gcc_jit_block *blk, gcc_jit_location *loc, gcc_jit_function *pers, int act_idx) noexcept {
    (void)blk;
    (void)loc;
    (void)pers;
    (void)act_idx;
}

/**
 * @brief Mock `gcc_jit_context_new_eh_filter` callback.
 * @param[in] ctx Context.
 * @param[in] loc Source location.
 * @param[in] filter_idx Filter index.
 * @return Non-null `void*` null rvalue when `g_eh_filter_nonnull`, else NULL.
 */
static gcc_jit_rvalue *mock_eh_filter(gcc_jit_context *ctx, gcc_jit_location *loc, int filter_idx) noexcept {
    (void)loc;
    (void)filter_idx;
    if (!g_eh_filter_nonnull) return NULL;
    return gcc_jit_context_null(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR));
}

/**
 * @brief Mock `gcc_jit_block_end_with_invoke` callback.
 * @param[in] blk Calling block.
 * @param[in] loc Source location.
 * @param[in] call Call rvalue.
 * @param[in] normal_d Normal destination block.
 * @param[in] unwind_d Unwind destination block.
 */
static void mock_end_invoke(gcc_jit_block *blk, gcc_jit_location *loc, gcc_jit_rvalue *call, gcc_jit_block *normal_d, gcc_jit_block *unwind_d) noexcept {
    (void)loc;
    (void)call;
    (void)unwind_d;
    gcc_jit_block_end_with_jump(blk, NULL, normal_d);
}

/**
 * @brief Resets all optional `libgccjit` GIMPLE EH function pointers to NULL.
 * @details Clears all dynamically loaded EH callback pointers to exercise portable fallback paths.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
static gcc_gen_be_error_t clear_optional_eh_hooks(void) noexcept {
    p_gcc_jit_lvalue_set_link_section = NULL;
    p_gcc_jit_context_add_top_level_asm = NULL;
    p_gcc_jit_block_add_try_catch = NULL;
    p_gcc_jit_block_add_cleanup = NULL;
    p_gcc_jit_block_end_with_resx = NULL;
    p_gcc_jit_function_set_personality = NULL;
    p_gcc_jit_block_add_landing_pad = NULL;
    p_gcc_jit_context_new_eh_filter = NULL;
    p_gcc_jit_block_end_with_invoke = NULL;
    p_gcc_jit_context_get_builtin_function = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Enables all optional `libgccjit` GIMPLE EH function pointers with mock callbacks.
 * @details Installs mock GIMPLE EH callbacks to exercise native landing-pad, invoke, and resx paths.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
static gcc_gen_be_error_t enable_optional_eh_hooks(void) noexcept {
    p_gcc_jit_lvalue_set_link_section = &mock_set_link_section;
    p_gcc_jit_context_add_top_level_asm = &mock_top_asm;
    p_gcc_jit_block_add_try_catch = &mock_add_try_catch;
    p_gcc_jit_block_add_cleanup = &mock_add_cleanup;
    p_gcc_jit_block_end_with_resx = &mock_end_resx;
    p_gcc_jit_function_set_personality = &mock_set_pers;
    p_gcc_jit_block_add_landing_pad = &mock_add_lp;
    p_gcc_jit_context_new_eh_filter = &mock_eh_filter;
    p_gcc_jit_block_end_with_invoke = &mock_end_invoke;
    p_gcc_jit_context_get_builtin_function = &gcc_jit_context_get_builtin_function;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises ULEB128/SLEB128 encoders, LSDA asm directive formatting, and `append_action_byte` growth.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_leb128_and_lsda_format(void) noexcept {
    unsigned char buf[16];
    size_t len = 0;
    if (gcc_gen_be_encode_uleb128(0, NULL, sizeof(buf), &len) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_encode_uleb128(0, buf, sizeof(buf), NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_encode_uleb128(300UL, buf, 1, &len) != GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_uleb128(0UL, buf, sizeof(buf), &len));
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_uleb128(624485UL, buf, sizeof(buf), &len));

    if (gcc_gen_be_encode_sleb128(0, NULL, sizeof(buf), &len) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_encode_sleb128(0, buf, sizeof(buf), NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_encode_sleb128(-123456L, buf, 1, &len) != GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_sleb128(0L, buf, sizeof(buf), &len));
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_sleb128(-1L, buf, sizeof(buf), &len));
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_sleb128(64L, buf, sizeof(buf), &len));
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_sleb128(-65L, buf, sizeof(buf), &len));

    char asm_buf[512];
    if (gcc_gen_be_format_lsda_asm_directives(1, 1, 1, 0, NULL, sizeof(asm_buf), &len) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_lsda_asm_directives(1, 1, 1, 0, asm_buf, sizeof(asm_buf), NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_lsda_asm_directives(-1, 1, 1, 0, asm_buf, sizeof(asm_buf), &len) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_lsda_asm_directives(1, -1, 1, 0, asm_buf, sizeof(asm_buf), &len) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_lsda_asm_directives(1, 1, -1, 0, asm_buf, sizeof(asm_buf), &len) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_lsda_asm_directives(1, 1, 1, 0, asm_buf, 8, &len) != GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_be_fPIC = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_format_lsda_asm_directives(1, 1, 1, 0, asm_buf, sizeof(asm_buf), &len));
    gcc_be_fPIC = FALSE; gcc_be_fPIE = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_format_lsda_asm_directives(1, 1, 1, 2, asm_buf, sizeof(asm_buf), &len));
    gcc_be_fPIE = FALSE; gcc_be_shared = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_format_lsda_asm_directives(1, 1, 1, -1, asm_buf, sizeof(asm_buf), &len));
    gcc_be_shared = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_format_lsda_asm_directives(1, 1, 1, 1, asm_buf, sizeof(asm_buf), &len));

    if (gcc_gen_be_chain_cleanup_action(0, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    int chained = 0;
    /* Grow action_table past initial 256-byte capacity (>130 chained entries = >260 bytes!) */
    for (int i = 0; i < 140; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_chain_cleanup_action(i, &chained));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises `gcc_gen_be_emit_throw` across object throws, rethrows, initializers, D1/VTT dtors, and OOM.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_throws(gcc_jit_context *ctx) noexcept {
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *i32_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_rvalue *rv1 = gcc_jit_context_new_rvalue_from_int(ctx, i32_jt, 7);

    an_expr_node ex = {};
    gcc_jit_block *out_b = NULL;
    if (gcc_gen_be_emit_throw(NULL, &out_b) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_throw(&ex, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_emit_throw(&ex, &out_b) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* cblock == NULL && current_func == NULL -> INVALID_STATE */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_emit_throw(&ex, &out_b) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, "throw_test_fn", 0, NULL, 0);
    gcc_jit_function *fn2 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, "nested_eh_fn", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(fn));
    /* Nested function EH state push/pop (`eh_state.prev`) */
    GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(fn2));
    GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(fn2));

    /* cblock == NULL && current_func != NULL -> synthesizes unreachable_throw block; rethrow without & with active LP */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    /* Second rethrow hits cached __cxa_rethrow + active_lp */
    gcc_jit_block *lp = gcc_jit_function_new_block(fn, "outer_lp");
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(lp));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());

    /* Object throws: typeref type, NULL type, tsp->expr (rval, NULL rval, block cleared, error, with & without outer_lp) */
    a_type t_i32 = {}, t_tref = {}, t_cls = {}, t_vcls = {}, t_u = {};
    t_i32.kind = tk_integer; t_i32.size = 4; t_i32.variant.integer.int_kind = (an_integer_kind)ik_int;
    t_tref.kind = tk_typeref; t_tref.variant.typeref.type = &t_cls;
    a_class_type_supplement cls_ext = {}, vcls_ext = {};
    cls_ext.highest_virtual_function_number = VIRTUAL_FUNCTION_NUMBER_NONE;
    vcls_ext.highest_virtual_function_number = VIRTUAL_FUNCTION_NUMBER_NONE;
    a_scope dscope = {};
    a_routine r_other = {}, r_d2 = {}, r_d1 = {};
    r_other.special_kind = (a_special_function_kind)sfk_none; r_other.source_corresp.name = "noop"; r_other.next = &r_d2;
    r_d2.special_kind = (a_special_function_kind)sfk_destructor; r_d2.source_corresp.name = "_ZN3ClsD2Ev"; r_d2.source_corresp.unmangled_name_or_mangled_encoding = "plain"; r_d2.next = &r_d1;
    r_d1.special_kind = (a_special_function_kind)sfk_destructor; r_d1.source_corresp.name = "raw"; r_d1.source_corresp.unmangled_name_or_mangled_encoding = "_ZN3ClsD1Ev"; r_d1.next = NULL;
    a_routine r_dnull = {};
    r_dnull.special_kind = (a_special_function_kind)sfk_destructor; r_dnull.source_corresp.name = NULL; r_dnull.next = &r_other;
    dscope.routines = &r_dnull;
    cls_ext.assoc_scope = &dscope;
    t_cls.kind = tk_class; t_cls.size = 8; t_cls.source_corresp.name = "Cls"; t_cls.variant.class_struct_union.extra_info = &cls_ext;

    a_base_class bc_v = {}; bc_v.type = &t_cls; bc_v.is_virtual = TRUE;
    vcls_ext.base_classes = &bc_v;
    t_vcls.kind = tk_struct; t_vcls.size = 16; t_vcls.source_corresp.name = "VCls";
    t_vcls.variant.class_struct_union.any_virtual_base_classes = TRUE;
    t_vcls.variant.class_struct_union.extra_info = &vcls_ext;
    t_u.kind = tk_union; t_u.size = 4; t_u.source_corresp.name = "Uni";

    an_expr_node init_e = {};
    a_throw_supplement ts = {};
    ex.variant.throw_info = &ts;

    /* 1. tsp->expr without outer_lp (uncached __cxa_free_exception & __cxa_throw) and with outer_lp */
    ts.type = &t_tref; ts.expr = &init_e;
    g_expr_rval = rv1;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    g_expr_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(lp));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());

    /* tsp->expr clearing cblock, NULL ts.type error, and tsp->expr failing */
    ts.type = NULL;
    if (gcc_gen_be_emit_throw(&ex, &out_b) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    g_expr_clears_block = TRUE;
    ts.type = &t_i32;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn, "tb_clr")));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    g_expr_clears_block = FALSE;

    g_expr_err = GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_throw(&ex, &out_b) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
    g_expr_err = GCC_GEN_BE_SUCCESS;
    ts.expr = NULL;

    /* 2. tsp->dynamic_init: dik_expression (rval, NULL rval, block cleared, error), dik_constructor (outer_lp & no outer_lp, block cleared, error), dik_class_result_via_ctor */
    a_dynamic_init dip = {};
    ts.type = &t_i32; ts.dynamic_init = &dip;
    dip.kind = (a_dynamic_init_kind)dik_expression; dip.variant.expression = &init_e;
    g_expr_rval = rv1;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    g_expr_rval = rv1; g_expr_clears_block = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn, "tb_dclr")));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    g_expr_clears_block = FALSE; g_expr_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    g_expr_err = GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_throw(&ex, &out_b) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
    g_expr_err = GCC_GEN_BE_SUCCESS;

    dip.kind = (a_dynamic_init_kind)dik_constructor;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    dip.kind = (a_dynamic_init_kind)dik_class_result_via_ctor;
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(lp));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    g_dyn_err = GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_throw(&ex, &out_b) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
    g_dyn_err = GCC_GEN_BE_SUCCESS;
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    g_dyn_clears_block = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn, "tb_dc2")));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    g_dyn_clears_block = FALSE;
    ts.dynamic_init = NULL;

    /* 3. Throw destructors: D2->D1 lookup via throw_type assoc_scope, parent_scope, union/NULL extra_info, cached dtor, 2-param VTT wrapper (with non-null VTT & null VTT), and NULL dtor_fn */
    a_routine dtor_orphan = {}; dtor_orphan.special_kind = (a_special_function_kind)sfk_destructor;
    ts.type = &t_tref; ts.destructor = &dtor_orphan;
    g_fn_param_mode = 2; /* 2-param VTT dtor on non-vbase class -> vtt_lval == NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn, "tb_dtor1")));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));

    /* 2-param VTT dtor on virtual-base class -> vtt_lval != NULL */
    a_routine dtor_v = {}; dtor_v.special_kind = (a_special_function_kind)sfk_destructor; dtor_v.source_corresp.parent_scope = &dscope;
    dscope.routines = NULL;
    ts.type = &t_vcls; ts.destructor = NULL; dip.destructor = &dtor_v; dip.kind = (a_dynamic_init_kind)dik_none; ts.dynamic_init = &dip;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));

    /* Cached dtor_fn, tk_struct without extra_info, union throw_type, non-sfk_destructor throw_dtor, and g_fn_param_mode = -1 (NULL dtor_fn) */
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, &dtor_orphan, fn));
    a_type t_st_noext = {}; t_st_noext.kind = tk_struct; t_st_noext.size = 4; t_st_noext.source_corresp.name = "StNoExt";
    ts.type = &t_st_noext; ts.destructor = &dtor_orphan; ts.dynamic_init = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    ts.type = &t_u; ts.destructor = &dtor_orphan; ts.dynamic_init = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    a_routine dtor_un = {}, dtor_non = {};
    dtor_un.special_kind = (a_special_function_kind)sfk_destructor;
    g_fn_param_mode = -1;
    ts.type = &t_i32; ts.destructor = &dtor_un;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));
    dtor_non.special_kind = (a_special_function_kind)sfk_none;
    g_fn_param_mode = 1;
    ts.type = &t_i32; ts.destructor = &dtor_non;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&ex, &out_b));

    GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(fn));
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises `gcc_gen_be_lower_try_block`, `gcc_gen_be_lower_routine_body`, `gcc_gen_be_enforce_exception_spec`,
 * `gcc_gen_be_emit_noexcept_filter`, `gcc_gen_be_emit_scope_unwind_cleanup`, LSDA/SEH/ARM-EHABI tables,
 * invoke trampolines, selector dispatch, and partial array/subobject cleanups under both portable and native GIMPLE EH hooks.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_try_catch_specs_and_tables(gcc_jit_context *ctx) noexcept {
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *vptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *i32_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *i64_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_type *sz_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);

    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, "eh_main_fn", 0, NULL, 0);
    gcc_jit_block *entry = gcc_jit_function_new_block(fn, "entry");

    /* Invalid-arg & NULL-context checks across all public entrypoints */
    a_statement dummy_st = {};
    an_exception_specification dummy_spec = {};
    gcc_jit_lvalue *out_glob = NULL;
    gcc_jit_block *out_blk = NULL;
    gcc_jit_function *out_pfn = NULL;
    a_type t_i32 = {}, t_ptr_ref = {}, t_tref_ref = {};
    t_i32.kind = tk_integer; t_i32.size = 4; t_i32.variant.integer.int_kind = (an_integer_kind)ik_int;
    t_ptr_ref.kind = tk_pointer; t_ptr_ref.size = 8; t_ptr_ref.variant.pointer.type = &t_i32; t_ptr_ref.variant.pointer.is_reference = TRUE;
    t_tref_ref.kind = tk_typeref; t_tref_ref.variant.typeref.type = &t_ptr_ref;
    a_routine dummy_dtor = {}; dummy_dtor.source_corresp.name = "elem_dtor";

    if (gcc_gen_be_begin_function_eh(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_end_function_eh(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_declare_eh_symbols(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_try_block(NULL, fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_try_block(&dummy_st, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_unwind_resume(NULL, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_jit_rvalue *null_vp = gcc_jit_context_null(ctx, vptr_t);
    if (gcc_gen_be_emit_unwind_resume(null_vp, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_register_personality_routine(NULL, &out_pfn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_register_personality_routine(fn, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_lsda_call_site_table(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_seh_unwind_tables(NULL, TRUE, &out_glob) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_noexcept_filter(NULL, entry, entry) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_noexcept_filter(fn, NULL, entry) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_noexcept_filter(fn, entry, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_enforce_exception_spec(NULL, &dummy_spec, entry, entry) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_enforce_exception_spec(fn, NULL, entry, entry) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_enforce_exception_spec(fn, &dummy_spec, NULL, entry) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_enforce_exception_spec(fn, &dummy_spec, entry, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_routine_body(NULL, fn, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_routine_body(&dummy_st, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_scope_unwind_cleanup(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_arm_ehabi_tables(NULL, TRUE, &out_glob) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_arm_ehabi_tables(fn, TRUE, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_lvalue *loc_ptr = gcc_jit_function_new_local(fn, NULL, vptr_t, "lp_b");
    gcc_jit_lvalue *loc_idx = gcc_jit_function_new_local(fn, NULL, sz_jt, "lp_i");
    if (gcc_gen_be_emit_partial_array_cleanup(NULL, loc_ptr, loc_idx, &t_i32, &dummy_dtor, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_partial_array_cleanup(fn, NULL, loc_idx, &t_i32, &dummy_dtor, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_partial_array_cleanup(fn, loc_ptr, NULL, &t_i32, &dummy_dtor, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_partial_array_cleanup(fn, loc_ptr, loc_idx, NULL, &dummy_dtor, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_partial_array_cleanup(fn, loc_ptr, loc_idx, &t_i32, NULL, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_partial_array_cleanup(fn, loc_ptr, loc_idx, &t_i32, &dummy_dtor, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_emit_subobject_unwind_cleanup(NULL, null_vp, 0, &dummy_dtor, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_subobject_unwind_cleanup(fn, NULL, 0, &dummy_dtor, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_subobject_unwind_cleanup(fn, null_vp, 0, NULL, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_subobject_unwind_cleanup(fn, null_vp, 0, &dummy_dtor, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_rvalue *sel_int = gcc_jit_context_new_rvalue_from_int(ctx, i32_jt, 1);
    if (gcc_gen_be_emit_selector_filter_dispatch(NULL, entry, sel_int, 1, entry, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_selector_filter_dispatch(fn, NULL, sel_int, 1, entry, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_selector_filter_dispatch(fn, entry, NULL, 1, entry, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_selector_filter_dispatch(fn, entry, sel_int, 1, NULL, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_selector_filter_dispatch(fn, entry, sel_int, 1, entry, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_emit_invoke_call(NULL, entry, fn, 0, NULL, NULL, NULL, entry, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_invoke_call(ctx, NULL, fn, 0, NULL, NULL, NULL, entry, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_invoke_call(ctx, entry, NULL, 0, NULL, NULL, NULL, entry, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_invoke_call(ctx, entry, fn, 1, NULL, NULL, NULL, entry, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_invoke_call(ctx, entry, fn, 0, NULL, NULL, NULL, NULL, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_invoke_call(ctx, entry, fn, 0, NULL, NULL, NULL, entry, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_emit_invoke_call_through_ptr(NULL, entry, null_vp, 0, NULL, NULL, NULL, entry, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_invoke_call_through_ptr(ctx, NULL, null_vp, 0, NULL, NULL, NULL, entry, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_invoke_call_through_ptr(ctx, entry, NULL, 0, NULL, NULL, NULL, entry, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_invoke_call_through_ptr(ctx, entry, null_vp, 1, NULL, NULL, NULL, entry, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_invoke_call_through_ptr(ctx, entry, null_vp, 0, NULL, NULL, NULL, NULL, &out_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_invoke_call_through_ptr(ctx, entry, null_vp, 0, NULL, NULL, NULL, entry, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    /* NULL ctx checks on all entrypoints */
    a_scope sc_fn = {}; sc_fn.kind = sck_function;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_try_block(&dummy_st, fn) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_unwind_resume(null_vp, &out_blk) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_register_personality_routine(fn, &out_pfn) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_lsda_call_site_table(&sc_fn, &out_glob) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_seh_unwind_tables(fn, TRUE, &out_glob) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_noexcept_filter(fn, entry, entry) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_enforce_exception_spec(fn, &dummy_spec, entry, entry) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_routine_body(&dummy_st, fn, &dummy_spec) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_scope_unwind_cleanup(fn) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_selector_filter_dispatch(fn, entry, sel_int, 1, entry, &out_blk) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_partial_array_cleanup(fn, loc_ptr, loc_idx, &t_i32, &dummy_dtor, &out_blk) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_subobject_unwind_cleanup(fn, null_vp, 0, &dummy_dtor, &out_blk) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_arm_ehabi_tables(fn, TRUE, &out_glob) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* NULL cblock checks on emit_unwind_resume & emit_scope_unwind_cleanup */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_emit_unwind_resume(null_vp, &out_blk) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_scope_unwind_cleanup(fn) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;

    /* C_mode() personality vs C++ personality & cached personality */
    C_dialect = C_dialect_ANSI;
    GCC_GEN_BE_CHECK(gcc_gen_be_register_personality_routine(fn, &out_pfn));
    GCC_GEN_BE_CHECK(gcc_gen_be_register_personality_routine(fn, &out_pfn));
    C_dialect = C_dialect_cplusplus;

    /* Declare EH symbols (uncached & cached) */
    GCC_GEN_BE_CHECK(gcc_gen_be_declare_eh_symbols(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_declare_eh_symbols(ctx));
    GCC_GEN_BE_CHECK(cache_clear_all());

    /* Try/catch comprehensive test (both without and with native GIMPLE EH hooks) */
    for (int hook_mode = 0; hook_mode < 2; ++hook_mode) {
        GCC_GEN_BE_CHECK(cache_clear_all());
        if (hook_mode == 0) GCC_GEN_BE_CHECK(clear_optional_eh_hooks());
        else GCC_GEN_BE_CHECK(enable_optional_eh_hooks());
        g_eh_filter_nonnull = (hook_mode == 1) ? TRUE : FALSE;

        char fnm[32]; std::snprintf(fnm, sizeof(fnm), "try_fn_%d", hook_mode);
        gcc_jit_function *tfn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, fnm, 0, NULL, 0);
        GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(tfn));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(tfn, "t_entry")));

        /* Handlers: 1) ref param, 2) copy-ctor dynamic_init, 3) expr dynamic_init, 4) non-ref pointer by-value param (2nd distinct filter type!), 5) NULL-typed param, 6) catch(...) with NULL statement */
        a_type t_ptr_nonref = {}; t_ptr_nonref.kind = tk_pointer; t_ptr_nonref.size = 8; t_ptr_nonref.variant.pointer.type = &t_i32; t_ptr_nonref.variant.pointer.is_reference = FALSE;
        a_statement try_body = {}, h_body1 = {}, h_body2 = {};
        a_variable hv_ref = {}, hv_ctor = {}, hv_expr = {}, hv_val = {}, hv_nt = {};
        hv_ref.type = &t_tref_ref;
        hv_ctor.type = &t_i32;
        hv_expr.type = &t_i32;
        hv_val.type = &t_ptr_nonref;
        hv_nt.type = NULL;

        an_expr_node hexp = {};
        a_dynamic_init hdi_ctor = {}, hdi_expr = {};
        hdi_ctor.kind = (a_dynamic_init_kind)dik_constructor;
        hdi_expr.kind = (a_dynamic_init_kind)dik_expression; hdi_expr.variant.expression = &hexp;

        a_handler h1 = {}, h2 = {}, h3 = {}, h4 = {}, h5 = {}, h6 = {};
        h1.parameter = &hv_ref; h1.statement = &h_body1; h1.next = &h2;
        h2.parameter = &hv_ctor; h2.dynamic_init = &hdi_ctor; h2.statement = &h_body2; h2.next = &h3;
        h3.parameter = &hv_expr; h3.dynamic_init = &hdi_expr; h3.statement = NULL; h3.next = &h4;
        h4.parameter = &hv_val; h4.statement = &h_body1; h4.next = &h5;
        h5.parameter = &hv_nt; h5.statement = NULL; h5.next = &h6;
        h6.parameter = NULL; h6.statement = NULL; h6.next = NULL;

        a_try_supplement tsp = {};
        tsp.statement = &try_body;
        tsp.handlers = &h1;
        a_statement try_stmt = {};
        try_stmt.kind = (a_statement_kind)stmk_try_block;
        try_stmt.variant.try_block = &tsp;

        g_expr_rval = gcc_jit_context_new_rvalue_from_int(ctx, i32_jt, 9);
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&try_stmt, tfn));

        /* Nested try inside active outer landing pad + empty handlers + NULL tsp + cleared blocks */
        gcc_jit_block *outer_lp = gcc_jit_function_new_block(tfn, "outer_try_lp");
        GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(outer_lp));
        g_stmt_clears_block = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&try_stmt, tfn));
        g_stmt_clears_block = FALSE;

        a_try_supplement tsp_no_h = {}; tsp_no_h.statement = NULL; tsp_no_h.handlers = NULL;
        try_stmt.variant.try_block = &tsp_no_h;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&try_stmt, tfn));
        try_stmt.variant.try_block = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&try_stmt, tfn));
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());

        /* Try body error, handler copy_err, and handler body error */
        try_stmt.variant.try_block = &tsp;
        g_fail_stmt = &try_body; g_stmt_err = GCC_GEN_BE_ERROR_INTERNAL;
        if (gcc_gen_be_lower_try_block(&try_stmt, tfn) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
        g_fail_stmt = &h_body1;
        if (gcc_gen_be_lower_try_block(&try_stmt, tfn) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
        g_fail_stmt = NULL;

        h1.next = NULL; h2.next = NULL; tsp.handlers = &h2;
        hdi_ctor.kind = (a_dynamic_init_kind)dik_class_result_via_ctor;
        g_dyn_clears_block = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&try_stmt, tfn));
        g_dyn_clears_block = FALSE;
        g_dyn_err = GCC_GEN_BE_ERROR_INTERNAL;
        if (gcc_gen_be_lower_try_block(&try_stmt, tfn) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
        g_dyn_err = GCC_GEN_BE_SUCCESS;

        /* Also test p_gcc_jit_context_new_eh_filter == NULL and has_eh_builtins == FALSE while p_gcc_jit_block_add_landing_pad != NULL, plus dik_expression handler NULL/cleared/error */
        if (hook_mode == 1) {
            g_eh_filter_nonnull = FALSE;
            GCC_GEN_BE_CHECK(cache_clear_all());
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&try_stmt, tfn));
            p_gcc_jit_context_new_eh_filter = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&try_stmt, tfn));
            p_gcc_jit_context_get_builtin_function = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&try_stmt, tfn));
            GCC_GEN_BE_CHECK(enable_optional_eh_hooks());
        }
        {
            h3.next = NULL; tsp.handlers = &h3;
            g_expr_rval = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&try_stmt, tfn));
            g_expr_rval = sel_int; g_expr_clears_block = TRUE;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&try_stmt, tfn));
            g_expr_clears_block = FALSE; g_expr_err = GCC_GEN_BE_ERROR_INTERNAL;
            if (gcc_gen_be_lower_try_block(&try_stmt, tfn) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
            g_expr_err = GCC_GEN_BE_SUCCESS; g_expr_rval = NULL;
        }

        /* Routine body: NULL spec, throw_any spec, noexcept(true) spec, dynamic throw(int, redundant) spec (twice for cached __cxa_call_unexpected), noexcept(false) direct enforce_exception_spec, body error, cleared after_try */
        an_exception_specification spec_any = {}, spec_nx = {}, spec_nx_false = {}, spec_dyn = {};
        spec_any.throw_any = TRUE;
        spec_nx.is_noexcept = TRUE; spec_nx.throw_any = FALSE;
        spec_nx_false.is_noexcept = TRUE; spec_nx_false.throw_any = TRUE;
        an_exception_specification_type est1 = {}, est_red = {};
        est1.type = &t_i32; est1.redundant = FALSE; est1.next = &est_red;
        est_red.type = &t_i32; est_red.redundant = TRUE; est_red.next = NULL;
        spec_dyn.is_noexcept = FALSE; spec_dyn.throw_any = FALSE;
        spec_dyn.variant.exception_specification_type_list = &est1;

        GCC_GEN_BE_CHECK(gcc_gen_be_lower_routine_body(&try_body, tfn, NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_routine_body(&try_body, tfn, &spec_any));
        GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(tfn));

        /* Fresh function so lp_start_emitted == FALSE inside gcc_gen_be_lower_routine_body! */
        char rfnm[32]; std::snprintf(rfnm, sizeof(rfnm), "rbody_fn_%d", hook_mode);
        gcc_jit_function *rfn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, rfnm, 0, NULL, 0);
        GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(rfn));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(rfn, "rb_entry")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_routine_body(&try_body, rfn, &spec_nx));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_routine_body(&try_body, rfn, &spec_nx)); /* cached _ZSt9terminatev */
        g_stmt_clears_block = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_routine_body(&try_body, rfn, &spec_dyn));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_routine_body(&try_body, rfn, &spec_dyn)); /* hits cached __cxa_call_unexpected */
        g_stmt_clears_block = FALSE;
        {
            gcc_jit_block *nf_lp = gcc_jit_function_new_block(rfn, "nf_lp");
            gcc_jit_block *nf_end = gcc_jit_function_new_block(rfn, "nf_end");
            GCC_GEN_BE_CHECK(gcc_gen_be_enforce_exception_spec(rfn, &spec_nx_false, nf_lp, nf_end));
        }
        g_fail_stmt = &try_body; g_stmt_err = GCC_GEN_BE_ERROR_INTERNAL;
        if (gcc_gen_be_lower_routine_body(&try_body, rfn, &spec_nx) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
        g_fail_stmt = NULL;

        /* SEH & ARM EHABI tables (with & without active block, with & without regions, fPIC/shared) */
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_seh_unwind_tables(rfn, TRUE, &out_glob));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_seh_unwind_tables(rfn, FALSE, NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_arm_ehabi_tables(rfn, TRUE, &out_glob));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_arm_ehabi_tables(rfn, FALSE, &out_glob));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_seh_unwind_tables(rfn, FALSE, &out_glob));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_arm_ehabi_tables(rfn, TRUE, &out_glob));

        gcc_be_fPIC = (hook_mode == 0) ? TRUE : FALSE;
        gcc_be_shared = (hook_mode == 1) ? TRUE : FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(rfn));
        gcc_be_fPIC = FALSE; gcc_be_shared = FALSE;

        /* Scope unwind cleanup: exceptions_enabled == FALSE, fresh fn (reset_eh_state + lp_start_emitted), scope_stack_size > 0 + already_has_lp, outer_lp */
        char sfnm[32]; std::snprintf(sfnm, sizeof(sfnm), "sc_cln_fn_%d", hook_mode);
        gcc_jit_function *sfn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, sfnm, 0, NULL, 0);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(sfn, "sc_entry")));
        exceptions_enabled = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(sfn));
        exceptions_enabled = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(sfn)); /* scope_stack_size == 0 */
        GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(sfn));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(sfn)); /* already_has_lp == TRUE */
        /* Nested inner scope chains onto outer cleanup LP (`outer_lp != NULL` and `eh_state.regions != NULL`) */
        GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(sfn));
        gcc_jit_block *cur_sc_b = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cur_sc_b));
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(cur_sc_b));
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(cur_sc_b));
        GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(sfn));
    }
    GCC_GEN_BE_CHECK(clear_optional_eh_hooks());

    /* Scope-scanning LSDA synthesis (`scan_statements_for_lsda` across all statement kinds + empty regions SEH) */
    {
        gcc_jit_function *lfn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, "lsda_scan_fn", 0, NULL, 0);
        GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(lfn));
        /* Empty regions SEH table */
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_seh_unwind_tables(lfn, TRUE, &out_glob));

        a_variable hv1 = {}; hv1.type = &t_i32;
        a_statement leaf = {}; leaf.kind = (a_statement_kind)stmk_empty;
        a_handler lh1 = {}, lh2 = {};
        lh1.parameter = &hv1; lh1.statement = &leaf; lh1.next = &lh2;
        lh2.parameter = NULL; lh2.statement = NULL; lh2.next = NULL;
        a_try_supplement ltsp = {}; ltsp.statement = &leaf; ltsp.handlers = &lh1;

        a_statement st_try = {}, st_blk = {}, st_if = {}, st_wh = {}, st_do = {}, st_for = {}, st_sw = {}, st_rfor = {};
        a_for_loop fls = {}; fls.initialization = &leaf;
        st_try.kind = (a_statement_kind)stmk_try_block; st_try.variant.try_block = &ltsp; st_try.next = &st_blk;
        st_blk.kind = (a_statement_kind)stmk_block; st_blk.variant.block.statements = &leaf; st_blk.next = &st_if;
        st_if.kind = (a_statement_kind)stmk_if; st_if.variant.if_stmt.then_statement = &leaf; st_if.variant.if_stmt.else_statement = &leaf; st_if.next = &st_wh;
        st_wh.kind = (a_statement_kind)stmk_while; st_wh.variant.loop_statement = &leaf; st_wh.next = &st_do;
        st_do.kind = (a_statement_kind)stmk_end_test_while; st_do.variant.loop_statement = &leaf; st_do.next = &st_for;
        st_for.kind = (a_statement_kind)stmk_for; st_for.variant.for_loop.extra_info = &fls; st_for.variant.for_loop.statement = &leaf; st_for.next = &st_sw;
        st_sw.kind = (a_statement_kind)stmk_switch; st_sw.variant.switch_stmt.body_statement = &leaf; st_sw.next = &st_rfor;
        st_rfor.kind = (a_statement_kind)stmk_range_based_for; st_rfor.variant.for_loop.statement = &leaf; st_rfor.next = NULL;

        /* Also test empty sub-statements on each compound kind (including stmk_try_block NULL try_block & stmk_for non-null extra_info with NULL initialization) */
        a_for_loop fls_noinit = {}; fls_noinit.initialization = NULL;
        a_statement st_try0 = {}, st_blk0 = {}, st_if0 = {}, st_wh0 = {}, st_for0 = {}, st_for_ni = {}, st_sw0 = {}, st_rfor0 = {};
        st_rfor.next = &st_try0;
        st_try0.kind = (a_statement_kind)stmk_try_block; st_try0.variant.try_block = NULL; st_try0.next = &st_blk0;
        st_blk0.kind = (a_statement_kind)stmk_block; st_blk0.next = &st_if0;
        st_if0.kind = (a_statement_kind)stmk_if; st_if0.next = &st_wh0;
        st_wh0.kind = (a_statement_kind)stmk_while; st_wh0.next = &st_for0;
        st_for0.kind = (a_statement_kind)stmk_for; st_for0.next = &st_for_ni;
        st_for_ni.kind = (a_statement_kind)stmk_for; st_for_ni.variant.for_loop.extra_info = &fls_noinit; st_for_ni.next = &st_sw0;
        st_sw0.kind = (a_statement_kind)stmk_switch; st_sw0.next = &st_rfor0;
        st_rfor0.kind = (a_statement_kind)stmk_range_based_for; st_rfor0.next = NULL;

        a_scope lsc = {}, ns_sc = {};
        lsc.kind = sck_function; lsc.assoc_block = &st_try;
        ns_sc.kind = sck_namespace;
        p_gcc_jit_lvalue_set_link_section = &mock_set_link_section;
        gcc_be_fPIE = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_lsda_call_site_table(&lsc, &out_glob));
        gcc_be_fPIE = FALSE;
        /* Second call with non-null eh_state.regions skips scan_statements_for_lsda; also test non-function scope and NULL scope */
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_lsda_call_site_table(&lsc, &out_glob));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_lsda_call_site_table(&ns_sc, &out_glob));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_lsda_call_site_table(NULL, &out_glob));
        p_gcc_jit_lvalue_set_link_section = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(lfn));
        /* Now eh_state.regions == NULL and scope == NULL -> INVALID_ARGUMENT! */
        if (gcc_gen_be_emit_lsda_call_site_table(NULL, &out_glob) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

        /* Many regions (>35) so call_site_bytes > 128 (`cs_uleb_len > 1` and `ttype_end_offset > 128`)! */
        a_statement many_tries[40] = {};
        a_try_supplement many_tsps[40] = {};
        for (int i = 0; i < 40; ++i) {
            many_tries[i].kind = (a_statement_kind)stmk_try_block;
            many_tries[i].variant.try_block = &many_tsps[i];
            many_tries[i].next = (i + 1 < 40) ? &many_tries[i + 1] : NULL;
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(lfn));
        lsc.assoc_block = &many_tries[0];
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_lsda_call_site_table(&lsc, &out_glob));
        GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(lfn));
    }

    /* Invoke trampolines (portable & native `p_gcc_jit_block_end_with_invoke`), selector dispatch, and partial array / subobject cleanups */
    {
        gcc_jit_function *ifn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, "inv_test_fn", 0, NULL, 0);
        gcc_jit_param *ip0 = gcc_jit_context_new_param(ctx, NULL, i32_jt, "a0");
        gcc_jit_function *callee_i32 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, i32_jt, "callee_i32", 1, &ip0, 0);
        gcc_jit_function *callee_void = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_t, "callee_v", 0, NULL, 0);
        gcc_jit_type *fptr_t = gcc_jit_context_new_function_ptr_type(ctx, NULL, i32_jt, 1, &i32_jt, 0);
        gcc_jit_rvalue *fptr_rv = gcc_jit_context_null(ctx, fptr_t);
        gcc_jit_rvalue *arg0 = gcc_jit_context_new_rvalue_from_int(ctx, i32_jt, 5);
        gcc_jit_lvalue *res_i64 = gcc_jit_function_new_local(ifn, NULL, i64_jt, "res64");
        gcc_jit_lvalue *res_i32 = gcc_jit_function_new_local(ifn, NULL, i32_jt, "res32");
        gcc_jit_block *unw_b = gcc_jit_function_new_block(ifn, "inv_unw");

        /* Portable trampoline: 0 args void, 1 arg with cast result (i32 -> i64), indirect call with matching result */
        gcc_jit_block *b0 = gcc_jit_function_new_block(ifn, "ib0");
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call(ctx, b0, callee_void, 0, NULL, NULL, NULL, unw_b, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call(ctx, out_blk, callee_i32, 1, &arg0, res_i64, NULL, unw_b, &out_blk));
        gcc_jit_block *expl_norm = gcc_jit_function_new_block(ifn, "expl_norm");
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call_through_ptr(ctx, out_blk, fptr_rv, 1, &arg0, res_i32, expl_norm, unw_b, &out_blk));

        /* Native `p_gcc_jit_block_end_with_invoke` (direct & indirect, with & without result_lval) */
        p_gcc_jit_block_end_with_invoke = &mock_end_invoke;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call(ctx, out_blk, callee_i32, 1, &arg0, res_i32, NULL, unw_b, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call_through_ptr(ctx, out_blk, fptr_rv, 1, &arg0, res_i32, NULL, unw_b, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_invoke_call(ctx, out_blk, callee_void, 0, NULL, NULL, NULL, unw_b, &out_blk));
        p_gcc_jit_block_end_with_invoke = NULL;

        /* Selector filter dispatch: int (>0, <0, 0) and void* (0, !=0) */
        gcc_jit_block *mb = gcc_jit_function_new_block(ifn, "sel_m");
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_selector_filter_dispatch(ifn, out_blk, sel_int, 2, mb, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_selector_filter_dispatch(ifn, out_blk, sel_int, -1, mb, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_selector_filter_dispatch(ifn, out_blk, sel_int, 0, mb, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_selector_filter_dispatch(ifn, out_blk, null_vp, 0, mb, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_selector_filter_dispatch(ifn, out_blk, null_vp, 3, mb, &out_blk));

        /* Partial array & subobject unwind cleanups: 1-param dtor, 0-param dtor, NULL dtor, offset 0 & !=0, with & without outer_lp */
        gcc_jit_lvalue *arr_p = gcc_jit_function_new_local(ifn, NULL, vptr_t, "arr_p");
        gcc_jit_lvalue *arr_i = gcc_jit_function_new_local(ifn, NULL, sz_jt, "arr_i");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(out_blk));
        g_fn_param_mode = 1;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_partial_array_cleanup(ifn, arr_p, arr_i, &t_i32, &dummy_dtor, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_subobject_unwind_cleanup(ifn, null_vp, 16, &dummy_dtor, &out_blk));
        g_fn_param_mode = 0;
        GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(unw_b));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_partial_array_cleanup(ifn, arr_p, arr_i, &t_i32, &dummy_dtor, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_subobject_unwind_cleanup(ifn, null_vp, 0, &dummy_dtor, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
        g_fn_param_mode = -1;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_partial_array_cleanup(ifn, arr_p, arr_i, &t_i32, &dummy_dtor, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_subobject_unwind_cleanup(ifn, null_vp, 0, &dummy_dtor, &out_blk));
        g_fn_param_mode = 1;
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Runs deterministic OOM fault-injection sweeps across all EH functions in `src/gcc_gen_be_eh.c`.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_eh_oom_sweeps(gcc_jit_context *ctx) noexcept {
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *vptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *i32_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *sz_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_rvalue *rv1 = gcc_jit_context_new_rvalue_from_int(ctx, i32_jt, 1);
    gcc_jit_rvalue *null_vp = gcc_jit_context_null(ctx, vptr_t);

    a_type t_i32 = {}, t_ptr_ref = {}, t_cls = {};
    a_class_type_supplement oom_cls_ext = {};
    oom_cls_ext.highest_virtual_function_number = VIRTUAL_FUNCTION_NUMBER_NONE;
    t_cls.kind = tk_class; t_cls.size = 8; t_cls.source_corresp.name = "OomCls"; t_cls.variant.class_struct_union.extra_info = &oom_cls_ext;
    t_i32.kind = tk_integer; t_i32.size = 4; t_i32.variant.integer.int_kind = (an_integer_kind)ik_int;
    t_ptr_ref.kind = tk_pointer; t_ptr_ref.size = 8; t_ptr_ref.variant.pointer.type = &t_i32; t_ptr_ref.variant.pointer.is_reference = TRUE;
    a_routine dtor_r = {}; dtor_r.special_kind = (a_special_function_kind)sfk_destructor; dtor_r.source_corresp.name = "oom_dtor";
    an_expr_node init_e = {};

    for (int cfg = 0; cfg < 20; ++cfg) {
        for (int b = 0; b < 400; ++b) {
            GCC_GEN_BE_CHECK(cache_clear_all());
            GCC_GEN_BE_CHECK(clear_optional_eh_hooks());
            char fnm[32]; std::snprintf(fnm, sizeof(fnm), "oom_eh_fn_%d_%d", cfg, b);
            gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, fnm, 0, NULL, 0);
            gcc_jit_block *eb = gcc_jit_function_new_block(fn, "entry");
            if (cfg == 19) {
                while (gcc_gen_be_singleton_state.eh_state_ptr &&
                       static_cast<gcc_gen_be_eh_state_t *>(gcc_gen_be_singleton_state.eh_state_ptr)->current_func) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(
                        static_cast<gcc_gen_be_eh_state_t *>(gcc_gen_be_singleton_state.eh_state_ptr)->current_func));
                }
            } else {
                GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(fn));
            }
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(eb));

            gcc_gen_be_error_t e = GCC_GEN_BE_SUCCESS;
            gcc_jit_block *out_b = NULL;
            gcc_jit_lvalue *out_g = NULL;

            if (cfg == 0) {
                /* Throw with tsp->expr + 2-param VTT dtor */
                a_throw_supplement ts = {}; ts.type = &t_cls; ts.expr = &init_e; ts.destructor = &dtor_r;
                an_expr_node tx = {}; tx.variant.throw_info = &ts;
                g_expr_rval = rv1; g_fn_param_mode = 2;
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_throw(&tx, &out_b);
            } else if (cfg == 1) {
                /* Throw with dip dik_constructor + outer_lp */
                a_dynamic_init dip = {}; dip.kind = (a_dynamic_init_kind)dik_constructor;
                a_throw_supplement ts = {}; ts.type = &t_i32; ts.dynamic_init = &dip;
                an_expr_node tx = {}; tx.variant.throw_info = &ts;
                gcc_jit_block *olp = gcc_jit_function_new_block(fn, "olp");
                GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(olp));
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_throw(&tx, &out_b);
                g_gcc_be_oom_countdown = -1;
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
                if (e == GCC_GEN_BE_SUCCESS) break;
                continue;
            } else if (cfg == 2) {
                /* Rethrow + NULL cblock */
                an_expr_node tx = {};
                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_throw(&tx, &out_b);
            } else if (cfg == 3) {
                /* Try block with ref + copy-ctor + by-val handlers */
                a_statement tb = {}, hb = {};
                a_variable vr = {}, vc = {}, vv = {};
                vr.type = &t_ptr_ref; vc.type = &t_i32; vv.type = &t_i32;
                a_dynamic_init hd = {}; hd.kind = (a_dynamic_init_kind)dik_constructor;
                a_handler h1 = {}, h2 = {}, h3 = {};
                h1.parameter = &vr; h1.statement = &hb; h1.next = &h2;
                h2.parameter = &vc; h2.dynamic_init = &hd; h2.statement = NULL; h2.next = &h3;
                h3.parameter = &vv; h3.statement = NULL; h3.next = NULL;
                a_try_supplement tsp = {}; tsp.statement = &tb; tsp.handlers = &h1;
                a_statement ts = {}; ts.kind = (a_statement_kind)stmk_try_block; ts.variant.try_block = &tsp;
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_lower_try_block(&ts, fn);
            } else if (cfg == 4) {
                /* Try block with native EH hooks enabled */
                GCC_GEN_BE_CHECK(enable_optional_eh_hooks());
                g_eh_filter_nonnull = FALSE;
                a_statement tb = {};
                a_try_supplement tsp = {}; tsp.statement = &tb;
                a_statement ts = {}; ts.kind = (a_statement_kind)stmk_try_block; ts.variant.try_block = &tsp;
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_lower_try_block(&ts, fn);
            } else if (cfg == 5) {
                /* Routine body with dynamic exception specification + end_function_eh */
                a_statement rb = {};
                an_exception_specification_type est = {}; est.type = &t_i32;
                an_exception_specification spec = {}; spec.variant.exception_specification_type_list = &est;
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_lower_routine_body(&rb, fn, &spec);
                if (e == GCC_GEN_BE_SUCCESS) {
                    e = gcc_gen_be_end_function_eh(fn);
                }
            } else if (cfg == 6) {
                /* Routine body with noexcept(true) */
                a_statement rb = {};
                an_exception_specification spec = {}; spec.is_noexcept = TRUE;
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_lower_routine_body(&rb, fn, &spec);
            } else if (cfg == 7) {
                /* Scope unwind cleanup with active scope & chained region */
                a_statement rb = {};
                an_exception_specification spec = {}; spec.is_noexcept = TRUE;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_routine_body(&rb, fn, &spec));
                gcc_jit_function *fn_sc = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, "sc_sub", 0, NULL, 0);
                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_sc, "entry")));
                GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_scope_unwind_cleanup(fn);
                int rem_sc = g_gcc_be_oom_countdown;
                g_gcc_be_oom_countdown = -1;
                while (gcc_gen_be_singleton_state.scope_stack_size > 0) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));
                }
                while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
                }
                GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(fn));
                if (e == GCC_GEN_BE_SUCCESS && rem_sc >= 0) break;
                continue;
            } else if (cfg == 8) {
                /* Scope-scanning LSDA table */
                a_variable hv = {}; hv.type = &t_i32;
                a_statement leaf = {};
                a_handler lh = {}; lh.parameter = &hv; lh.statement = &leaf;
                a_try_supplement ltsp = {}; ltsp.statement = &leaf; ltsp.handlers = &lh;
                a_statement st_try = {}, st_blk = {}, st_if = {}, st_wh = {}, st_for = {}, st_sw = {}, st_rf = {};
                a_for_loop fls = {}; fls.initialization = &leaf;
                st_try.kind = (a_statement_kind)stmk_try_block; st_try.variant.try_block = &ltsp; st_try.next = &st_blk;
                st_blk.kind = (a_statement_kind)stmk_block; st_blk.variant.block.statements = &leaf; st_blk.next = &st_if;
                st_if.kind = (a_statement_kind)stmk_if; st_if.variant.if_stmt.then_statement = &leaf; st_if.variant.if_stmt.else_statement = &leaf; st_if.next = &st_wh;
                st_wh.kind = (a_statement_kind)stmk_while; st_wh.variant.loop_statement = &leaf; st_wh.next = &st_for;
                st_for.kind = (a_statement_kind)stmk_for; st_for.variant.for_loop.extra_info = &fls; st_for.variant.for_loop.statement = &leaf; st_for.next = &st_sw;
                st_sw.kind = (a_statement_kind)stmk_switch; st_sw.variant.switch_stmt.body_statement = &leaf; st_sw.next = &st_rf;
                st_rf.kind = (a_statement_kind)stmk_range_based_for; st_rf.variant.for_loop.statement = &leaf;
                a_scope lsc = {}; lsc.kind = sck_function; lsc.assoc_block = &st_try;
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_lsda_call_site_table(&lsc, &out_g);
            } else if (cfg == 9) {
                /* SEH unwind tables */
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_seh_unwind_tables(fn, TRUE, &out_g);
            } else if (cfg == 10) {
                /* ARM EHABI tables */
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_arm_ehabi_tables(fn, FALSE, &out_g);
            } else if (cfg == 11) {
                /* Invoke trampoline with args + result */
                gcc_jit_param *p0 = gcc_jit_context_new_param(ctx, NULL, i32_jt, "a0");
                gcc_jit_function *cf = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, i32_jt, "inv_c", 1, &p0, 0);
                gcc_jit_lvalue *rl = gcc_jit_function_new_local(fn, NULL, i32_jt, "rl");
                gcc_jit_block *ub = gcc_jit_function_new_block(fn, "ub");
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_invoke_call(ctx, eb, cf, 1, &rv1, rl, NULL, ub, &out_b);
            } else if (cfg == 12) {
                /* Partial array cleanup */
                gcc_jit_lvalue *ap = gcc_jit_function_new_local(fn, NULL, vptr_t, "ap");
                gcc_jit_lvalue *ai = gcc_jit_function_new_local(fn, NULL, sz_jt, "ai");
                g_fn_param_mode = 1;
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_partial_array_cleanup(fn, ap, ai, &t_i32, &dtor_r, &out_b);
            } else if (cfg == 13) {
                /* Subobject cleanup */
                g_fn_param_mode = 1;
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_subobject_unwind_cleanup(fn, null_vp, 8, &dtor_r, &out_b);
            } else if (cfg == 14) {
                /* Nested begin_function_eh + declare_eh_symbols */
                gcc_jit_function *fn2 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, "oom_nest", 0, NULL, 0);
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_begin_function_eh(fn2);
                if (e == GCC_GEN_BE_SUCCESS) {
                    e = gcc_gen_be_declare_eh_symbols(ctx);
                }
            } else if (cfg == 15) {
                /* Native end_with_invoke + end_with_resx OOM */
                GCC_GEN_BE_CHECK(enable_optional_eh_hooks());
                gcc_jit_block *ub = gcc_jit_function_new_block(fn, "ub2");
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_invoke_call(ctx, eb, fn, 0, NULL, NULL, NULL, ub, &out_b);
                if (e == GCC_GEN_BE_SUCCESS) {
                    e = gcc_gen_be_emit_unwind_resume(null_vp, &out_b);
                }
            } else if (cfg == 16) {
                /* Throw dik_expression + rethrow with active outer_lp */
                a_dynamic_init dip = {}; dip.kind = (a_dynamic_init_kind)dik_expression; dip.variant.expression = &init_e;
                a_throw_supplement ts = {}; ts.type = &t_i32; ts.dynamic_init = &dip;
                an_expr_node tx = {}; tx.variant.throw_info = &ts;
                an_expr_node rtx = {};
                g_expr_rval = rv1;
                gcc_jit_block *olp = gcc_jit_function_new_block(fn, "olp2");
                GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(olp));
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_throw(&tx, &out_b);
                if (e == GCC_GEN_BE_SUCCESS) {
                    e = gcc_gen_be_emit_throw(&rtx, &out_b);
                }
            } else if (cfg == 17) {
                /* Try block with single dik_expression handler + cleared handler block (hits 1056 & 1128) */
                a_statement tb = {}, hb1 = {};
                a_variable ve = {}; ve.type = &t_i32;
                a_dynamic_init hde = {}; hde.kind = (a_dynamic_init_kind)dik_expression; hde.variant.expression = &init_e;
                a_handler h1 = {};
                h1.parameter = &ve; h1.dynamic_init = &hde; h1.statement = &hb1; h1.next = NULL;
                a_try_supplement tsp = {}; tsp.statement = NULL; tsp.handlers = &h1;
                a_statement ts = {}; ts.kind = (a_statement_kind)stmk_try_block; ts.variant.try_block = &tsp;
                g_expr_rval = rv1;
                g_stmt_clears_block = TRUE;
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_lower_try_block(&ts, fn);
                g_stmt_clears_block = FALSE;
            } else if (cfg == 18) {
                /* selector_filter_dispatch + format_lsda_asm_directives */
                char abuf[512]; size_t alen = 0;
                gcc_jit_block *mb = gcc_jit_function_new_block(fn, "mb_oom");
                g_gcc_be_oom_countdown = b;
                e = gcc_gen_be_emit_selector_filter_dispatch(fn, eb, rv1, 1, mb, &out_b);
                if (e == GCC_GEN_BE_SUCCESS) {
                    e = gcc_gen_be_format_lsda_asm_directives(1, 1, 1, 1, abuf, sizeof(abuf), &alen);
                }
            } else {
                /* register_personality_routine + emit_scope_unwind_cleanup when eh_state.current_func == NULL + error-path pop OOM (905, 1074, 1117, 1245, 1831) */
                gcc_jit_function *pf_tmp = NULL;
                if (b < 15) {
                    g_gcc_be_oom_countdown = b;
                    e = gcc_gen_be_register_personality_routine(fn, &pf_tmp);
                    g_gcc_be_oom_countdown = -1;
                    GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(fn));
                    continue;
                }
                if (b < 30) {
                    g_gcc_be_oom_countdown = b - 15;
                    e = gcc_gen_be_emit_scope_unwind_cleanup(fn);
                    g_gcc_be_oom_countdown = -1;
                    GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(fn));
                    continue;
                }
                GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(fn));
                a_statement tb = {}, hb = {};
                a_variable vc = {}; vc.type = &t_i32;
                a_dynamic_init hdc = {}; hdc.kind = (a_dynamic_init_kind)dik_constructor;
                a_handler h1 = {}; h1.parameter = &vc; h1.dynamic_init = &hdc; h1.statement = &hb;
                a_try_supplement tsp = {}; tsp.statement = &tb; tsp.handlers = &h1;
                a_statement ts = {}; ts.kind = (a_statement_kind)stmk_try_block; ts.variant.try_block = &tsp;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&ts, fn));
                int sub = (b - 30) % 3;
                if (sub == 0) { g_fail_stmt = &tb; g_stmt_err = GCC_GEN_BE_ERROR_INTERNAL; }
                else if (sub == 1) { g_dyn_err = GCC_GEN_BE_ERROR_INTERNAL; }
                else { g_fail_stmt = &hb; g_stmt_err = GCC_GEN_BE_ERROR_INTERNAL; }
                g_gcc_be_oom_countdown = (b - 30) / 3;
                e = gcc_gen_be_lower_try_block(&ts, fn);
                g_fail_stmt = NULL; g_dyn_err = GCC_GEN_BE_SUCCESS;
                if ((b - 30) / 3 > 80) { e = GCC_GEN_BE_SUCCESS; g_gcc_be_oom_countdown = 1; }
            }
            int rem_budget = g_gcc_be_oom_countdown;
            g_gcc_be_oom_countdown = -1;
            g_fn_param_mode = 1;
            g_expr_rval = NULL;
            while (gcc_gen_be_singleton_state.scope_stack_size > 0) {
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));
            }
            while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
            }
            GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(fn));
            if (e == GCC_GEN_BE_SUCCESS && rem_budget >= 0) break;
        }
    }
    GCC_GEN_BE_CHECK(clear_optional_eh_hooks());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes all EH unit tests and OOM sweeps.
 * @param[out] out_ok Pointer receiving 1 on success, 0 on failure.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t run_eh_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    *out_ok = 0;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    GCC_GEN_BE_CHECK(test_leb128_and_lsda_format());
    /* Before any begin_function_eh runs (current_func_id == 0), exercise table_id fallback & register_personality_routine reset_eh_state! */
    {
        gcc_jit_type *vt0 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_function *f0 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt0, "pre_eh_fn", 0, NULL, 0);
        gcc_jit_lvalue *g0 = NULL;
        a_scope empty_fsc = {}; empty_fsc.kind = sck_function; empty_fsc.assoc_block = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_lsda_call_site_table(&empty_fsc, &g0));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_seh_unwind_tables(f0, FALSE, &g0));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_arm_ehabi_tables(f0, TRUE, &g0));
        gcc_jit_function *pf0 = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_register_personality_routine(f0, &pf0));
        GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(f0));
    }
    GCC_GEN_BE_CHECK(test_throws(ctx));
    GCC_GEN_BE_CHECK(test_try_catch_specs_and_tables(ctx));
    GCC_GEN_BE_CHECK(test_eh_oom_sweeps(ctx));

    /* Stage-2 EH coverage: splice_landing_pad_at shift & underflow, D1E throw dtor lookup,
       eh_try_barrier_stack push/pop/overflow, and gcc_gen_be_emit_single_dtor_unwind_lp */
    {
        gcc_jit_context *s2_eh_ctx = gcc_jit_context_acquire();
        GCC_GEN_BE_CHECK(gcc_gen_be_set_context(s2_eh_ctx));
        gcc_jit_context *ctx = s2_eh_ctx;
        gcc_jit_type *vt_s2 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_type *vp_s2 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_type *it_s2 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        gcc_jit_function *fs2 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, "eh_s2_fn", 0, NULL, 0);
        GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(fs2));

        /* 1) splice_landing_pad_at shift (398:26 True) & underflow (394:9 True) + D1E dtor lookup (609..633) */
        a_type thr_int_t = {}; thr_int_t.kind = tk_integer; thr_int_t.size = 4;
        a_type thr_tref_null = {}; thr_tref_null.kind = tk_typeref; thr_tref_null.variant.typeref.type = &thr_int_t;
        a_type thr_tref_cls = {}; thr_tref_cls.kind = tk_typeref;
        a_scope cls_sc_s2 = {}, fn_sc_s2 = {};
        cls_sc_s2.kind = sck_class_struct_union;
        fn_sc_s2.kind = sck_function;

        a_routine dtor_non_sfk = {}, dtor_null_nm = {}, dtor_non_z = {}, dtor_d2 = {}, dtor_d1 = {};
        dtor_non_sfk.special_kind = static_cast<a_special_function_kind>(sfk_none); dtor_non_sfk.source_corresp.name = "foo"; dtor_non_sfk.next = &dtor_null_nm;
        dtor_null_nm.special_kind = static_cast<a_special_function_kind>(sfk_destructor); dtor_null_nm.source_corresp.name = NULL; dtor_null_nm.next = &dtor_non_z;
        dtor_non_z.special_kind = static_cast<a_special_function_kind>(sfk_destructor); dtor_non_z.source_corresp.name = "not_d1"; dtor_non_z.source_corresp.unmangled_name_or_mangled_encoding = const_cast<char *>("plain"); dtor_non_z.next = &dtor_d2;
        dtor_d2.special_kind = static_cast<a_special_function_kind>(sfk_destructor); dtor_d2.source_corresp.name = "_ZN1CD2Ev"; dtor_d2.source_corresp.unmangled_name_or_mangled_encoding = const_cast<char *>("_ZN1CD2Ev"); dtor_d2.source_corresp.parent_scope = &cls_sc_s2; dtor_d2.next = &dtor_d1;
        dtor_d1.special_kind = static_cast<a_special_function_kind>(sfk_destructor); dtor_d1.source_corresp.name = "_ZN1CD1Ev"; dtor_d1.source_corresp.unmangled_name_or_mangled_encoding = const_cast<char *>("_ZN1CD1Ev");
        cls_sc_s2.routines = &dtor_non_sfk;

        an_expr_node thr_val_e = {}, thr_op_e = {};
        thr_val_e.kind = enk_constant; thr_val_e.type = &thr_int_t;
        a_throw_supplement ths = {};
        ths.type = &thr_int_t; /* non-class -> !dscope, falls back to throw_dtor->parent_scope == &cls_sc_s2! */
        ths.expr = &thr_val_e;
        ths.destructor = &dtor_d2;
        thr_op_e.kind = enk_throw; thr_op_e.variant.throw_info = &ths;

        g_expr_rval = gcc_jit_context_new_rvalue_from_int(ctx, it_s2, 5);
        gcc_jit_block *extra_lp = gcc_jit_function_new_block(fs2, "eh_s2_extra_lp");
        g_push_lp_in_expr = extra_lp;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fs2, "eh_s2_thr1")));
        gcc_jit_block *thr_blk = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&thr_op_e, &thr_blk));
        g_push_lp_in_expr = NULL;
        while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
            GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
        }

        /* Also test typeref->class with extra_info->assoc_scope, non-class parent_scope (`&fn_sc_s2`), and null parent_scope */
        a_class_type_supplement cts_s2 = {}; cts_s2.assoc_scope = &cls_sc_s2;
        a_type cls_t_s2 = {}; cls_t_s2.kind = tk_class; cls_t_s2.size = 4; cls_t_s2.source_corresp.name = "EhS2Cls";
        cls_t_s2.variant.class_struct_union.extra_info = &cts_s2;
        thr_tref_cls.variant.typeref.type = &cls_t_s2;
        ths.type = &thr_tref_cls;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fs2, "eh_s2_thr2")));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&thr_op_e, &thr_blk));

        /* tk_struct with extra_info == NULL & parent_scope == &fn_sc_s2 (`!= sck_class_struct_union`), then parent_scope == NULL */
        a_type st_no_ei = {}; st_no_ei.kind = tk_struct; st_no_ei.size = 4; st_no_ei.source_corresp.name = "EhS2StNoEi";
        ths.type = &st_no_ei;
        dtor_d2.source_corresp.parent_scope = &fn_sc_s2;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fs2, "eh_s2_thr3")));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&thr_op_e, &thr_blk));

        ths.type = &thr_tref_null;
        dtor_d2.source_corresp.parent_scope = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fs2, "eh_s2_thr4")));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_throw(&thr_op_e, &thr_blk));

        /* splice_landing_pad_at underflow (394:9 True -> INVALID_STATE) */
        g_pop_all_lp_in_expr = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fs2, "eh_s2_thr_uf")));
        if (gcc_gen_be_emit_throw(&thr_op_e, &thr_blk) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
        g_pop_all_lp_in_expr = FALSE;
        g_expr_rval = NULL;

        /* 2) eh_try_barrier_stack push/pop & >= 32 cap when eh_cleanup_dip_depth > 0 (956..988) */
        a_statement try_b_s2 = {}, h_b_s2 = {};
        a_handler h_s2 = {}; h_s2.statement = &h_b_s2;
        a_try_supplement tsp_s2 = {}; tsp_s2.statement = &try_b_s2; tsp_s2.handlers = &h_s2;
        a_statement ts_s2 = {}; ts_s2.kind = static_cast<a_statement_kind>(stmk_try_block); ts_s2.variant.try_block = &tsp_s2;

        gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 2U;
        gcc_gen_be_singleton_state.eh_try_barrier_depth = 0U;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fs2, "eh_s2_try1")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&ts_s2, fs2));

        gcc_gen_be_singleton_state.eh_try_barrier_depth = 32U; /* >= 32U cap branch! */
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fs2, "eh_s2_try2")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&ts_s2, fs2));

        /* Empty try_block with saved_dip_depth > 0 (covers 988..990) and g_push_dip_in_stmt OOM sweep (covers 971..974) */
        a_statement ts_empty_s2 = {}; ts_empty_s2.kind = static_cast<a_statement_kind>(stmk_try_block); ts_empty_s2.variant.try_block = NULL;
        gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 2U;
        gcc_gen_be_singleton_state.eh_try_barrier_depth = 0U;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fs2, "eh_s2_try_empty")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(&ts_empty_s2, fs2));

        g_push_dip_in_stmt = TRUE;
        for (int b = 0; b < 20; ++b) {
            while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
            }
            gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
            gcc_gen_be_singleton_state.eh_try_barrier_depth = 0U;
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fs2, "eh_s2_try_dip")));
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t te = gcc_gen_be_lower_try_block(&ts_s2, fs2);
            g_gcc_be_oom_countdown = -1;
            if (te == GCC_GEN_BE_SUCCESS) break;
        }
        g_push_dip_in_stmt = FALSE;
        gcc_gen_be_singleton_state.eh_try_barrier_depth = 0U;
        gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;

        /* Throw with tsp->expr == NULL and dip->kind == dik_constructor OOM sweep (covers line 574 splice_landing_pad_at OOM) */
        a_type thr_dip_tp = {}; thr_dip_tp.kind = tk_integer; thr_dip_tp.size = 4;
        a_dynamic_init thr_dip = {}; thr_dip.kind = dik_constructor;
        a_throw_supplement thr_dip_tsp = {}; thr_dip_tsp.type = &thr_dip_tp; thr_dip_tsp.dynamic_init = &thr_dip; thr_dip_tsp.expr = NULL;
        an_expr_node thr_dip_e = {}; thr_dip_e.kind = enk_throw; thr_dip_e.variant.throw_info = &thr_dip_tsp;
        for (int b = 0; b < 25; ++b) {
            while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
            }
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fs2, "eh_s2_thr_dip")));
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t de = gcc_gen_be_emit_throw(&thr_dip_e, &thr_blk);
            g_gcc_be_oom_countdown = -1;
            if (de == GCC_GEN_BE_SUCCESS) break;
        }

        /* 3) gcc_gen_be_emit_single_dtor_unwind_lp: null args, null ctx, null cblock, 0-param/1-param/2-int-param/2-ptr-param dtor, and OOM sweep */
        gcc_jit_lvalue *obj_lv = gcc_jit_function_new_local(fs2, NULL, it_s2, "s2_dtor_obj");
        gcc_jit_param *dp0 = gcc_jit_context_new_param(ctx, NULL, vp_s2, "this");
        gcc_jit_param *dp1_i = gcc_jit_context_new_param(ctx, NULL, it_s2, "chg");
        gcc_jit_param *dp0b = gcc_jit_context_new_param(ctx, NULL, vp_s2, "this");
        gcc_jit_param *dp1_p = gcc_jit_context_new_param(ctx, NULL, vp_s2, "vtt");
        gcc_jit_param *d2i_arr[2] = {dp0, dp1_i};
        gcc_jit_param *d2p_arr[2] = {dp0b, dp1_p};
        gcc_jit_function *dfn_0 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, vt_s2, "s2_dtor_0p", 0, NULL, 0);
        gcc_jit_function *dfn_2i = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, vt_s2, "s2_dtor_2ip", 2, d2i_arr, 0);
        gcc_jit_function *dfn_2p = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, vt_s2, "s2_dtor_2pp", 2, d2p_arr, 0);

        if (gcc_gen_be_emit_single_dtor_unwind_lp(NULL, obj_lv, dfn_0) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
        if (gcc_gen_be_emit_single_dtor_unwind_lp(fs2, NULL, dfn_0) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
        if (gcc_gen_be_emit_single_dtor_unwind_lp(fs2, obj_lv, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        if (gcc_gen_be_emit_single_dtor_unwind_lp(fs2, obj_lv, dfn_0) != GCC_GEN_BE_SUCCESS) return GCC_GEN_BE_ERROR_INTERNAL;

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fs2, "eh_s2_sdlp0")));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_single_dtor_unwind_lp(fs2, obj_lv, dfn_0));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_single_dtor_unwind_lp(fs2, obj_lv, dfn_2i));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_single_dtor_unwind_lp(fs2, obj_lv, dfn_2p));

        for (int b = 0; b < 25; ++b) {
            while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
            }
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fs2, "eh_s2_sdlp_oom")));
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t se = gcc_gen_be_emit_single_dtor_unwind_lp(fs2, obj_lv, dfn_2p);
            g_gcc_be_oom_countdown = -1;
            if (se == GCC_GEN_BE_SUCCESS) break;
        }
        while (gcc_gen_be_singleton_state.landing_pad_stack_size > 0) {
            GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(fs2));
        GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

        /* With context cleared by cleanup(), verify null-context INVALID_STATE path, then acquire final context */
        if (gcc_gen_be_emit_single_dtor_unwind_lp(fs2, obj_lv, dfn_0) == GCC_GEN_BE_SUCCESS) return GCC_GEN_BE_ERROR_INTERNAL;
        gcc_jit_context *end_eh_ctx = gcc_jit_context_acquire();
        GCC_GEN_BE_CHECK(gcc_gen_be_set_context(end_eh_ctx));
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_eh.cpp`.
 * @return 0 on success, non-zero on failure.
 */
int main(void) noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_eh_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
