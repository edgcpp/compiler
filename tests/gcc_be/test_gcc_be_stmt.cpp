/**
 * @file test_gcc_be_stmt.cpp
 * @brief Comprehensive unit and OOM fault-injection tests for statement and coroutine lowering (`gcc_gen_be_stmt.c`).
 * @details Exercises label/switch-case resolution, computed `goto *`, static local `__cxa_guard_*` initialization,
 * SEH `__try`/`__except`/`__finally` (including abnormal exit cleanups), lexical blocks/RAII/OMP/VLA stack scopes,
 * `if`/`constexpr if`/`switch`/loops, inline/function-body assembly, `return`/`goto`, and all C++20 coroutine
 * frame/ramp/dispatcher/HALO/noop/symmetric-transfer lowering paths under deterministic OOM sweeps.
 * @ingroup gcc_gen_be_tests
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_omp.h"
#include "gcc_gen_be_lib_loader.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for statement unit tests.
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

/** @brief Active coroutine descriptor returned by `get_coroutine_descr`. */
static a_coroutine_descr_ptr g_mock_coro_descr = NULL;
/** @brief When TRUE, `gcc_gen_be_lower_type` sets `*out_type = NULL`. */
static a_boolean g_mock_lower_type_null = FALSE;
/** @brief Optional pre-created struct `gcc_jit_type *` returned by `gcc_gen_be_lower_type` for `tk_struct`. */
static gcc_jit_type *g_mock_struct_jit_type = NULL;
/** @brief When TRUE, `gcc_gen_be_get_zero_rvalue` sets `*out_rval = NULL`. */
static a_boolean g_mock_zero_rval_null = FALSE;
/** @brief When TRUE, `gcc_gen_be_lower_function_decl` sets `*out_func = NULL`. */
static a_boolean g_mock_lower_fn_null = FALSE;
/** @brief Parameter count for imported functions synthesized by `gcc_gen_be_lower_function_decl`. */
static int g_mock_fn_param_count = 1;
/** @brief When TRUE, `gcc_gen_be_lower_function_decl` synthesizes a 0-parameter function. */
static a_boolean g_mock_fn_zero_params = FALSE;
/** @brief When TRUE, parameter 1 of synthesized mock functions uses `int` instead of `void*`. */
static a_boolean g_mock_fn_p1_int = FALSE;
/** @brief Optional target expression node triggering `g_mock_alt_expr_rval` in `gcc_gen_be_lower_expr_rvalue`. */
static an_expr_node_ptr g_mock_alt_expr_node = NULL;
/** @brief Alternate rvalue returned when `expr == g_mock_alt_expr_node`. */
static gcc_jit_rvalue *g_mock_alt_expr_rval = NULL;
/** @brief When TRUE, `gcc_gen_be_lower_expr_rvalue` clears `current_block` only when `expr == g_mock_alt_expr_node`. */
static a_boolean g_mock_alt_expr_clear_block = FALSE;
/** @brief When TRUE, `gcc_gen_be_lower_try_block` registers 258 suspend points across DIP depths 1 and 65. */
static a_boolean g_mock_try_many_suspends = FALSE;
/** @brief When TRUE, `gcc_gen_be_lower_local_variable_decl` / `global` sets `*out_lval = NULL`. */
static a_boolean g_mock_var_decl_null = FALSE;
/** @brief When TRUE, `gcc_gen_be_lower_dynamic_init` returns `GCC_GEN_BE_ERROR_OOM`. */
static a_boolean g_mock_dyn_init_fail = FALSE;
/** @brief When TRUE, `gcc_gen_be_lower_dynamic_init` clears the current basic block to NULL. */
static a_boolean g_mock_dyn_init_clear_block = FALSE;
/** @brief Configurable rvalue returned by `gcc_gen_be_lower_expr_rvalue`. */
static gcc_jit_rvalue *g_mock_expr_rval = NULL;
/** @brief When TRUE, `gcc_gen_be_lower_expr_rvalue` clears the current basic block to NULL. */
static a_boolean g_mock_expr_clear_block = FALSE;
/** @brief Configurable lvalue returned by `gcc_gen_be_lower_expr_lvalue`. */
static gcc_jit_lvalue *g_mock_expr_lval = NULL;
/** @brief When TRUE, `gcc_gen_be_lower_constant_rvalue` sets `*out_rval = NULL`. */
static a_boolean g_mock_const_rval_null = FALSE;
/** @brief When TRUE, location helpers return a non-NULL `gcc_jit_location *`. */
static a_boolean g_mock_emit_loc = FALSE;
/** @brief When TRUE, `gcc_gen_be_coerce_rvalue` sets `*out_rval = NULL`. */
static a_boolean g_mock_coerce_null = FALSE;
/** @brief Optional explicit rvalue returned by `gcc_gen_be_coerce_rvalue`. */
static gcc_jit_rvalue *g_mock_coerced_rval = NULL;
/** @brief Sequence counter for unique mock global/function names. */
static int g_mock_seq = 0;

BEGIN_EDG_NAMESPACE
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
/** @brief Mock asm operand constraint character lookup table for non-raw asm configurations. */
char asm_operand_constraint_letters[aoc_last + 1] = {'@', ',', 'r', 'm'};
#endif
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
/** @brief Innermost function scope pointer. */
a_scope_ptr innermost_function_scope = NULL;
/** @brief Current statement expression result lvalue pointer. */
struct gcc_jit_lvalue *current_stmt_expr_result_lval = NULL;
/** @brief Named register names table sized to `anr_last + 1`. */
a_const_char *named_register_names[anr_last + 1] = { "invalid", "memory" };
/** @brief Mock IL header. */
an_il_header il_header;

/**
 * @brief Stub mapping sequence numbers to source file and line in statement unit tests.
 * @details Populates empty/default outputs and returns NULL.
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
    if (file_name) *file_name = NULL;
    if (full_name) *full_name = NULL;
    if (line_number) *line_number = (a_line_number)seq;
    if (at_end_of_source) *at_end_of_source = FALSE;
    return NULL;
}

/**
 * @brief Stub for constant rvalue lowering in statement unit tests.
 * @details Returns NULL when `g_mock_const_rval_null` is set, otherwise an integer constant of `expected_type`.
 * @param[in] con The frontend constant pointer.
 * @param[in] expected_type The expected JIT type.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, struct gcc_jit_type *expected_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_const_rval_null) {
        *out_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    long val = con ? (long)(((uintptr_t)con >> 3) & 0x3f) + 1L : 1L;
    *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, expected_type, val);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for retrieving coroutine descriptor from a routine in unit tests.
 * @details Returns `g_mock_coro_descr`.
 * @param[in] rout The frontend routine pointer.
 * @return Configured `g_mock_coro_descr`.
 */
a_coroutine_descr_ptr get_coroutine_descr(a_routine_ptr rout) {
    (void)rout;
    return g_mock_coro_descr;
}

/**
 * @brief Stub for looking up routine scope in statement unit tests.
 * @details Synthesizes a minimal function `a_scope` containing `stmk_coroutine` when `g_mock_coro_descr` is active.
 * @param[in] rout The frontend routine pointer.
 * @return Synthetic scope pointer when `g_mock_coro_descr != NULL`, or NULL otherwise.
 */
a_scope_ptr scope_for_routine(a_routine_ptr rout) {
    (void)rout;
    if (!g_mock_coro_descr) return NULL;
    static a_statement s_coro_stmt;
    static a_scope s_coro_scope;
    memset(&s_coro_stmt, 0, sizeof(s_coro_stmt));
    memset(&s_coro_scope, 0, sizeof(s_coro_scope));
    s_coro_stmt.kind = stmk_coroutine;
    s_coro_stmt.variant.coroutine.descr = g_mock_coro_descr;
    s_coro_scope.kind = sck_function;
    s_coro_scope.assoc_block = &s_coro_stmt;
    return &s_coro_scope;
}

/**
 * @brief Stub for unsigned integer constant extraction in statement unit tests.
 * @details Sets `*err` to `FALSE` and returns 0.
 * @param[in] cp The frontend constant pointer.
 * @param[out] err Pointer to receive overflow flag.
 * @return 0.
 */
a_host_large_unsigned unsigned_value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    (void)cp;
    if (err) *err = FALSE;
    return 0;
}

/**
 * @brief Stub for signed integer constant extraction in statement unit tests.
 * @details Sets `*err` to `FALSE` and returns 0.
 * @param[in] cp The frontend constant pointer.
 * @param[out] err Pointer to receive overflow flag.
 * @return 0.
 */
a_host_large_integer value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    (void)cp;
    if (err) *err = FALSE;
    return 0;
}

/**
 * @brief Stub for type lowering in statement unit tests.
 * @details Lowers `tk_struct` to `g_mock_struct_jit_type` (when set), `tk_pointer`/`tk_array` to `void*`, and scalars to `int`.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_type Pointer to receive the lowered `gcc_jit_type`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_lower_type_null) {
        *out_type = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    a_type_ptr cur = tp;
    while (cur && cur->kind == tk_typeref) cur = cur->variant.typeref.type;
    if (cur && cur->kind == tk_struct && g_mock_struct_jit_type) {
        *out_type = g_mock_struct_jit_type;
    } else if (cur && (cur->kind == tk_pointer || cur->kind == tk_array)) {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    } else {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for zero rvalue creation in statement unit tests.
 * @details Synthesizes a valid zero/null/struct-zero rvalue matching `jit_type`, or NULL if `g_mock_zero_rval_null`.
 * @param[in] edg_type The frontend type pointer.
 * @param[in] jit_type The lowered JIT type.
 * @param[out] out_rval Pointer to receive the zero rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_zero_rvalue(a_type_ptr edg_type, struct gcc_jit_type *jit_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)edg_type;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_zero_rval_null) {
        *out_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (gcc_jit_type_is_pointer(jit_type) != NULL) {
        *out_rval = gcc_jit_context_null(ctx, jit_type);
    } else if (gcc_jit_type_is_struct(jit_type) != NULL) {
        char gname[64];
        std::snprintf(gname, sizeof(gname), "__mock_zero_st_%d", g_mock_seq++);
        gcc_jit_lvalue *zg = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, jit_type, gname);
        *out_rval = gcc_jit_lvalue_as_rvalue(zg);
    } else {
        *out_rval = gcc_jit_context_zero(ctx, jit_type);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function declaration lowering in statement unit tests.
 * @details Synthesizes an imported function with `g_mock_fn_param_count` parameters, or NULL if `g_mock_lower_fn_null`.
 * @param[in] rout The frontend routine pointer.
 * @param[out] out_func Pointer to receive the lowered function.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_lower_fn_null) {
        *out_func = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *vp = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *sz_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    a_boolean is_new = (rout && rout->source_corresp.name && std::strcmp(rout->source_corresp.name, "op_new") == 0) ? TRUE : FALSE;
    gcc_jit_type *ret_t = is_new ? vp : void_t;
    int pc = g_mock_fn_zero_params ? 0 : ((g_mock_fn_param_count > 0) ? g_mock_fn_param_count : 1);
    gcc_jit_param *params[8];
    for (int i = 0; i < pc && i < 8; ++i) {
        char pname[16];
        std::snprintf(pname, sizeof(pname), "p%d", i);
        gcc_jit_type *pt = (is_new && i == 0) ? sz_t : ((i == 1 && g_mock_fn_p1_int) ? gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT) : vp);
        params[i] = gcc_jit_context_new_param(ctx, NULL, pt, pname);
    }
    char fname[64];
    std::snprintf(fname, sizeof(fname), "__mock_fn_%d", g_mock_seq++);
    *out_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_t, fname, pc, params, 0);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for local variable declaration lowering in statement unit tests.
 * @details Allocates an `int` local in `func` (and caches it) unless `g_mock_var_decl_null` is set.
 * @param[in] func The active JIT function.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered local variable lvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_var_decl_null || !func) {
        *out_lval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    void *cached = NULL;
    if (var) {
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached));
        if (cached) {
            *out_lval = (gcc_jit_lvalue *)cached;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_lvalue *lv = gcc_jit_function_new_local(
        func, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT),
        (var && var->source_corresp.name) ? var->source_corresp.name : "mock_local");
    if (var) {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, lv));
    }
    *out_lval = lv;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for global variable declaration lowering in statement unit tests.
 * @details Allocates an `int` global (and caches it) unless `g_mock_var_decl_null` is set.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered global variable lvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_var_decl_null) {
        *out_lval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    char gname[64];
    std::snprintf(gname, sizeof(gname), "__mock_glob_%d", g_mock_seq++);
    gcc_jit_lvalue *gv = gcc_jit_context_new_global(
        ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), gname);
    if (var) {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, gv));
    }
    *out_lval = gv;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for dynamic initialization lowering in statement unit tests.
 * @details Supports optional failure (`g_mock_dyn_init_fail`) and block termination (`g_mock_dyn_init_clear_block`).
 * @param[in] dip The dynamic initialization pointer.
 * @param[in] target_lval The destination lvalue.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_init(a_dynamic_init_ptr dip, struct gcc_jit_lvalue *target_lval, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)dip;
    (void)target_lval;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_dyn_init_fail) return GCC_GEN_BE_ERROR_OOM;
    if (g_mock_dyn_init_clear_block) {
        int saved_oom = g_gcc_be_oom_countdown;
        g_gcc_be_oom_countdown = -1;
        gcc_jit_block *cb = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cb));
        if (cb) {
            gcc_jit_block_end_with_void_return(cb, NULL);
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        g_gcc_be_oom_countdown = saved_oom;
    }
    *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for RAII scope unwind cleanup emission in stmt unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Active JIT function.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    return gcc_gen_be_step_oom_budget();
}

/** @brief When TRUE, `gcc_gen_be_lower_expr_rvalue` registers a coroutine suspend point in the current block's function. */
static a_boolean g_mock_register_suspend_in_expr = FALSE;

/**
 * @brief Stub for expression rvalue lowering in statement unit tests.
 * @details Returns `g_mock_expr_rval` and optionally terminates/clears the current block (`g_mock_expr_clear_block`).
 * @param[in] expr The expression node pointer.
 * @param[out] out_rval Pointer to receive the lowered rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_alt_expr_node && expr == g_mock_alt_expr_node) {
        if (g_mock_alt_expr_clear_block) {
            int saved_oom = g_gcc_be_oom_countdown;
            g_gcc_be_oom_countdown = -1;
            gcc_jit_block *cb = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cb));
            if (cb) gcc_jit_block_end_with_void_return(cb, NULL);
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
            g_gcc_be_oom_countdown = saved_oom;
        }
        *out_rval = g_mock_alt_expr_rval;
        return GCC_GEN_BE_SUCCESS;
    }
    if (g_mock_register_suspend_in_expr) {
        int saved_oom = g_gcc_be_oom_countdown;
        g_gcc_be_oom_countdown = -1;
        gcc_jit_block *cb = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cb));
        if (cb) {
            gcc_jit_function *fn = gcc_jit_block_get_function(cb);
            gcc_jit_block *rb = gcc_jit_function_new_block(fn, "expr_susp_res");
            gcc_jit_block_end_with_void_return(rb, NULL);
            int sidx = 0;
            GCC_GEN_BE_CHECK(gcc_gen_be_register_coro_suspend_point(rb, &sidx, NULL, NULL));
        }
        g_gcc_be_oom_countdown = saved_oom;
    }
    if (g_mock_expr_clear_block) {
        int saved_oom = g_gcc_be_oom_countdown;
        g_gcc_be_oom_countdown = -1;
        gcc_jit_block *cb = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cb));
        if (cb) {
            gcc_jit_block_end_with_void_return(cb, NULL);
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        g_gcc_be_oom_countdown = saved_oom;
    }
    *out_rval = g_mock_expr_rval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression lvalue lowering in statement unit tests.
 * @details Returns `g_mock_expr_lval`.
 * @param[in] expr The expression node pointer.
 * @param[out] out_lval Pointer to receive the lowered lvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_lval = g_mock_expr_lval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for source location lookup in statement unit tests.
 * @details Returns a synthetic location when `g_mock_emit_loc` is TRUE, otherwise NULL.
 * @param[in] pos The frontend source position.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location(a_source_position *pos, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)pos;
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_emit_loc) {
        gcc_jit_context *ctx = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
        *out_loc = ctx ? gcc_jit_context_new_location(ctx, "stmt_test.cpp", 10, 1) : NULL;
    } else {
        *out_loc = NULL;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for variable source location lookup in statement unit tests.
 * @details Delegates to `gcc_gen_be_get_location`.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_var(a_variable_ptr var, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    return gcc_gen_be_get_location(var ? &var->source_corresp.decl_position : NULL, out_loc);
}

/**
 * @brief Stub for statement source location lookup in statement unit tests.
 * @details Delegates to `gcc_gen_be_get_location`.
 * @param[in] stmt The frontend statement pointer.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_stmt(a_statement_ptr stmt, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    return gcc_gen_be_get_location(stmt ? &stmt->position : NULL, out_loc);
}

/**
 * @brief Stub for lexical block debug info emission in statement unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] stmt The frontend statement pointer.
 * @param[in] block The JIT basic block.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_emit_block_debug_info(a_statement_ptr stmt, struct gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    (void)block;
    return gcc_gen_be_step_oom_budget();
}

/**
 * @brief Stub for variable debug info emission in statement unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var The frontend variable pointer.
 * @param[in] lval The JIT lvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, struct gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    (void)lval;
    return gcc_gen_be_step_oom_budget();
}

/**
 * @brief Stub for try block lowering in statement unit tests.
 * @details Steps OOM budget and, when inside a coroutine dispatcher with an active frame,
 * registers two coroutine suspension points (with and without an active lexical scope) to exercise
 * resume/destroy switch generation and `emit_coro_scope_dtors`.
 * @param[in] stmt The frontend try statement pointer.
 * @param[in] func The active JIT function.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_try_block(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (func && gcc_gen_be_singleton_state.active_coro_frame_lval && gcc_gen_be_singleton_state.active_coro_state_field) {
        int sidx = 0;
        if (g_mock_try_many_suspends) {
            int saved_oom = g_gcc_be_oom_countdown;
            g_gcc_be_oom_countdown = -1;
            gcc_jit_block *rb_d1 = gcc_jit_function_new_block(func, "coro_try_d1");
            gcc_jit_block_end_with_void_return(rb_d1, NULL);
            GCC_GEN_BE_CHECK(gcc_gen_be_register_coro_suspend_point(rb_d1, &sidx, NULL, NULL));
            size_t saved_dd = gcc_gen_be_singleton_state.eh_cleanup_dip_depth;
            gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 65U;
            for (int i = 0; i < 257; ++i) {
                char bnm[32]; std::snprintf(bnm, sizeof(bnm), "coro_try_cap_%d", i);
                gcc_jit_block *rb_cap = gcc_jit_function_new_block(func, bnm);
                gcc_jit_block_end_with_void_return(rb_cap, NULL);
                GCC_GEN_BE_CHECK(gcc_gen_be_register_coro_suspend_point(rb_cap, &sidx, NULL, NULL));
            }
            gcc_gen_be_singleton_state.eh_cleanup_dip_depth = saved_dd;
            g_gcc_be_oom_countdown = saved_oom;
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_lvalue *slv = NULL;
        gcc_jit_rvalue *sfr = NULL;
        gcc_jit_block *rb1 = gcc_jit_function_new_block(func, "coro_try_res_1");
        gcc_jit_block_end_with_void_return(rb1, NULL);
        GCC_GEN_BE_CHECK(gcc_gen_be_register_coro_suspend_point(rb1, &sidx, &slv, &sfr));
        a_scope_ptr saved_sc = gcc_gen_be_singleton_state.active_coro_current_scope;
        gcc_gen_be_singleton_state.active_coro_current_scope = NULL;
        gcc_jit_block *rb2 = gcc_jit_function_new_block(func, "coro_try_res_2");
        gcc_jit_block_end_with_void_return(rb2, NULL);
        GCC_GEN_BE_CHECK(gcc_gen_be_register_coro_suspend_point(rb2, &sidx, NULL, NULL));
        gcc_gen_be_singleton_state.active_coro_current_scope = saved_sc;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for invisible reference ABI check in statement unit tests.
 * @details Sets `*out_is_ref` to `FALSE` and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] type The frontend type pointer.
 * @param[out] out_is_ref Pointer to receive the boolean result.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (!out_is_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_is_ref = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for rvalue ABI coercion in statement unit tests.
 * @details Returns NULL when `g_mock_coerce_null`, `g_mock_coerced_rval` when set, or zero/cast of `dst_type`.
 * @param[in] func Active JIT function.
 * @param[in] block Active JIT block.
 * @param[in] src_rval Source rvalue.
 * @param[in] dst_type Destination JIT type.
 * @param[out] out_rval Pointer to receive the coerced rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_coerce_rvalue(
    struct gcc_jit_function *func,
    struct gcc_jit_block *block,
    struct gcc_jit_rvalue *src_rval,
    struct gcc_jit_type *dst_type,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)block;
    (void)src_rval;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_coerce_null) {
        *out_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    if (g_mock_coerced_rval) {
        *out_rval = g_mock_coerced_rval;
        return GCC_GEN_BE_SUCCESS;
    }
    return gcc_gen_be_get_zero_rvalue(NULL, dst_type, out_rval);
}

/**
 * @brief Stub for SEH `.pdata` / `.xdata` unwind table emission in statement unit tests.
 * @details Steps OOM budget, sets `*out_xdata_global` to NULL, and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Active JIT function.
 * @param[in] has_except_handler Whether the scope is `__except` (`TRUE`) or `__finally` (`FALSE`).
 * @param[out] out_xdata_global Optional pointer to receive the `.xdata` global.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_emit_seh_unwind_tables(
    struct gcc_jit_function *func,
    a_boolean has_except_handler,
    struct gcc_jit_lvalue **out_xdata_global) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)has_except_handler;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_xdata_global) *out_xdata_global = NULL;
    return GCC_GEN_BE_SUCCESS;
}

extern gcc_gen_be_error_t gcc_gen_be_begin_function_stmts(void) GCC_GEN_BE_NOEXCEPT;
extern gcc_gen_be_error_t gcc_gen_be_end_function_stmts(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
END_EDG_NAMESPACE

/**
 * @brief Sweeps deterministic OOM budgets (`0..max_budget`) over a callable test operation.
 * @details Clears backend caches before each iteration and ensures every budget either returns
 * `GCC_GEN_BE_ERROR_OOM` or completes with `GCC_GEN_BE_SUCCESS`.
 * @tparam Fn Callable returning `gcc_gen_be_error_t` with `noexcept`.
 * @param[in] max_budget Hint budget cap (clamped to at least 5000).
 * @param[in] fn Operation under test.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INTERNAL` on unexpected error.
 */
template <typename Fn>
static gcc_gen_be_error_t sweep_oom_stmt(int max_budget, Fn fn) GCC_GEN_BE_NOEXCEPT {
    int limit = max_budget > 5000 ? max_budget : 5000;
    for (int b = 0; b <= limit; ++b) {
        g_gcc_be_oom_countdown = -1;
        while (gcc_gen_be_singleton_state.scope_stack_size > 0) {
            size_t top = --gcc_gen_be_singleton_state.scope_stack_size;
            std::free(gcc_gen_be_singleton_state.scope_stack[top]);
        }
        gcc_gen_be_singleton_state.landing_pad_stack_size = 0;
        GCC_GEN_BE_CHECK(cache_clear_all());
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t err = fn();
        g_gcc_be_oom_countdown = -1;
        if (err == GCC_GEN_BE_SUCCESS) {
            return GCC_GEN_BE_SUCCESS;
        }
        if (err != GCC_GEN_BE_ERROR_OOM) {
            std::fprintf(stderr, "sweep_oom_stmt unexpected err=%d at b=%d ", (int)err, b);
            return GCC_GEN_BE_ERROR_INTERNAL;
        }
    }
    std::fprintf(stderr, "sweep_oom_stmt exceeded limit=%d ", limit);
    return GCC_GEN_BE_ERROR_INTERNAL;
}

/**
 * @brief Tests label resolution, computed `goto *`, switch-case blocks, and `__cxa_guard_*` static local initialization.
 * @details Exercises `computed_goto_labels` growth (`0->16->32`), void vs non-void `finalize_computed_goto`,
 * bitcast vs coerce computed goto targets, and static/thread_local guard acquire/release/abort paths.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_labels_computed_goto_and_guards(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *long_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);

    gcc_jit_function *fn_void = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "cgoto_void_fn", 0, NULL, 0);
    gcc_jit_function *fn_int = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "cgoto_int_fn", 0, NULL, 0);

    gcc_jit_block *out_blk = NULL;
    if (gcc_gen_be_get_or_create_label_block(NULL, fn_void, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_or_create_label_block(NULL, fn_void, &out_blk) != GCC_GEN_BE_ERROR_LABEL_RESOLUTION_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    /* func == NULL with cblock == NULL -> LABEL_RESOLUTION_FAILED; with cblock != NULL -> resolves func from cblock! */
    a_label lbl_cblk = {}, lbl_brk = {}, lbl_cont = {}, lbl_anon = {};
    lbl_cblk.source_corresp.name = "named_lbl";
    lbl_brk.break_label = TRUE;
    lbl_cont.continue_label = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_get_or_create_label_block(&lbl_cblk, NULL, &out_blk) != GCC_GEN_BE_ERROR_LABEL_RESOLUTION_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_block *entry_v = gcc_jit_function_new_block(fn_void, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(entry_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_stmts());
    GCC_GEN_BE_CHECK(gcc_gen_be_end_function_stmts(fn_void)); /* dispatcher_block == NULL fast path */
    GCC_GEN_BE_CHECK(gcc_gen_be_finalize_computed_goto(fn_void)); /* dispatcher_block == NULL fast path */

    /* 20 labels -> grows computed_goto_labels 0 -> 16 -> 32, plus cached lookup with & without prior entry */
    a_label many_lbls[20] = {};
    GCC_GEN_BE_CHECK( sweep_oom_stmt(150, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_stmts());
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(entry_v));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_or_create_label_block(&lbl_cblk, NULL, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_or_create_label_block(&lbl_cblk, fn_void, &out_blk)); /* cached + already in list */
        GCC_GEN_BE_CHECK(gcc_gen_be_get_or_create_label_block(&lbl_brk, fn_void, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_or_create_label_block(&lbl_cont, fn_void, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_or_create_label_block(&lbl_anon, fn_void, &out_blk));
        for (int i = 0; i < 20; ++i) {
            GCC_GEN_BE_CHECK(gcc_gen_be_get_or_create_label_block(&many_lbls[i], fn_void, &out_blk));
        }
        /* Reset count without clearing cache -> cached label NOT in computed_goto_labels! */
        GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_stmts());
        GCC_GEN_BE_CHECK(gcc_gen_be_get_or_create_label_block(&lbl_cblk, fn_void, &out_blk));
        return GCC_GEN_BE_SUCCESS;
    }) );

    /* Switch case blocks: case_value != NULL, NULL, cached */
    a_constant sc_con = {};
    a_switch_case_entry sce_val = {}, sce_def = {};
    sce_val.case_value = &sc_con;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(20, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_get_switch_case_block(fn_void, &sce_val, &out_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_switch_case_block(fn_void, &sce_val, &out_blk));
        return gcc_gen_be_get_switch_case_block(fn_void, &sce_def, &out_blk);
    }) );

    /* stmk_assigned_goto variants (NULL ptr, long ptr, int ptr, void* with bitcast & with coerce) + finalize on void & int fns */
    an_expr_node ag_expr = {};
    a_statement ag_stmt = {};
    ag_stmt.kind = stmk_assigned_goto;
    ag_stmt.expr = &ag_expr;

    GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_stmts());
    /* 0 labels in computed_goto_labels_count + NULL ptr + finalize on void fn (also test ctx == NULL -> INVALID_STATE) */
    gcc_jit_block *b_ag0 = gcc_jit_function_new_block(fn_void, "ag0");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(b_ag0));
    g_mock_expr_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&ag_stmt, fn_void));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_finalize_computed_goto(fn_void) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_end_function_stmts(fn_void));

    /* Non-void fn with labels, testing long, int, pointer+bitcast, pointer+coerce, and OOM sweep */
    GCC_GEN_BE_CHECK( sweep_oom_stmt(80, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_stmts());
        GCC_GEN_BE_CHECK(gcc_gen_be_get_or_create_label_block(&lbl_cblk, fn_int, &out_blk));

        gcc_jit_block *b1 = gcc_jit_function_new_block(fn_int, "ag_long");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(b1));
        g_mock_expr_rval = gcc_jit_context_zero(ctx, long_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&ag_stmt, fn_int));

        gcc_jit_block *b2 = gcc_jit_function_new_block(fn_int, "ag_int");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(b2));
        g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&ag_stmt, fn_int));

        gcc_jit_block *b3 = gcc_jit_function_new_block(fn_int, "ag_ptr_bc");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(b3));
        g_mock_expr_rval = gcc_jit_context_null(ctx, vp_t);
        p_gcc_jit_context_new_bitcast = [](gcc_jit_context *c, gcc_jit_location *, gcc_jit_rvalue *, gcc_jit_type *dt) noexcept -> gcc_jit_rvalue * {
            return gcc_jit_context_zero(c, dt);
        };
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&ag_stmt, fn_int));
        p_gcc_jit_context_new_bitcast = NULL;

        gcc_jit_block *b4 = gcc_jit_function_new_block(fn_int, "ag_ptr_coerce");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(b4));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&ag_stmt, fn_int));
        g_mock_expr_rval = NULL;

        return gcc_gen_be_end_function_stmts(fn_int);
    }) );

    /* Also test finalize_computed_goto failing inside get_label_block when a NULL label is in computed_goto_labels */
    GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_stmts());
    gcc_jit_block *b_ag_err = gcc_jit_function_new_block(fn_int, "ag_err");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(b_ag_err));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&ag_stmt, fn_int));
    gcc_gen_be_singleton_state.computed_goto_labels[0] = &lbl_cblk;
    gcc_gen_be_singleton_state.computed_goto_labels_count = 1;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(25, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_jit_block *bt = gcc_jit_function_new_block(fn_int, "ag_swp");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(bt));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&ag_stmt, fn_int));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_or_create_label_block(&lbl_cblk, fn_int, &out_blk));
        return gcc_gen_be_finalize_computed_goto(fn_int);
    }) );

    /* Static local & thread_local guard initialization: named & unnamed, cached guard & __cxa_guard_*, clear_block, abort on error */
    a_variable svar_named = {}, svar_anon = {};
    svar_named.source_corresp.name = "static_local_x";
    svar_named.storage_class = (a_storage_class)sc_static;
    svar_anon.source_corresp.name = NULL;
    svar_anon.is_thread_local = TRUE;

    a_dynamic_init s_dip1 = {}, s_dip2 = {};
    s_dip1.kind = (a_dynamic_init_kind)dik_expression;
    s_dip1.variable = &svar_named;
    s_dip2.kind = (a_dynamic_init_kind)dik_expression;
    s_dip2.variable = &svar_anon;

    a_statement st_init1 = {}, st_init2 = {};
    st_init1.kind = stmk_init; st_init1.variant.dynamic_init = &s_dip1;
    st_init2.kind = stmk_init; st_init2.variant.dynamic_init = &s_dip2;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(60, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_jit_block *bi = gcc_jit_function_new_block(fn_void, "guard_entry");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(bi));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&st_init1, fn_void));
        /* Second call on st_init1 hits cached guard AND cached __cxa_guard_* functions! */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&st_init1, fn_void));
        g_mock_dyn_init_clear_block = TRUE;
        gcc_gen_be_error_t e = gcc_gen_be_lower_statement(&st_init2, fn_void);
        g_mock_dyn_init_clear_block = FALSE;
        return e;
    }) );

    /* Dynamic init failure inside static guard -> emits __cxa_guard_abort call and returns error */
    gcc_jit_block *bi_fail = gcc_jit_function_new_block(fn_void, "guard_fail_entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(bi_fail));
    g_mock_dyn_init_fail = TRUE;
    if (gcc_gen_be_lower_statement(&st_init1, fn_void) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_dyn_init_fail = FALSE;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests control-flow statements, blocks, RAII/OMP/VLA scopes, SEH, `return`/`goto`, and inline assembly.
 * @details Covers `stmt_has_label_or_case`, `lower_block_stmt_inner`, `stmk_return` ABI coercions/fallbacks,
 * `stmk_if`/`constexpr_if`, `stmk_switch`, loops, `stmk_decl`/`stmk_init`/`stmk_vla_decl`, `stmk_asm`, and SEH.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_control_flow_blocks_and_asm(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
    gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *ip_t = gcc_jit_type_get_pointer(int_t);
    gcc_jit_field *sf0 = gcc_jit_context_new_field(ctx, NULL, int_t, "x");
    gcc_jit_struct *st_j = gcc_jit_context_new_struct_type(ctx, NULL, "MockRetStruct", 1, &sf0);
    gcc_jit_type *st_t = gcc_jit_struct_as_type(st_j);
    g_mock_struct_jit_type = st_t;

    gcc_jit_function *fn_void = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "cf_void_fn", 0, NULL, 0);
    gcc_jit_function *fn_int0 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "cf_int0_fn", 0, NULL, 0);
    gcc_jit_param *p_int = gcc_jit_context_new_param(ctx, NULL, int_t, "pi");
    gcc_jit_function *fn_int_pi = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "cf_int_pi", 1, &p_int, 0);
    gcc_jit_param *p_vp = gcc_jit_context_new_param(ctx, NULL, vp_t, "pvp");
    gcc_jit_function *fn_int_pvp = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "cf_int_pvp", 1, &p_vp, 0);
    gcc_jit_param *p_ip = gcc_jit_context_new_param(ctx, NULL, ip_t, "pip");
    gcc_jit_function *fn_vp_pip = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, vp_t, "cf_vp_pip", 1, &p_ip, 0);
    gcc_jit_function *fn_st = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, st_t, "cf_st_fn", 0, NULL, 0);

    /* ctx == NULL on gcc_gen_be_lower_statement */
    a_statement empty_s = {}; empty_s.kind = stmk_empty;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_statement(&empty_s, fn_void) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* Unreachable control-flow scanner (`!current_block`) + loc attachment (`g_mock_emit_loc = TRUE`) */
    a_label lbl1 = {}, lbl2 = {};
    lbl1.source_corresp.name = "u_lbl1";
    lbl2.source_corresp.name = "u_lbl2";
    a_switch_case_entry sce1 = {}, sce2 = {};
    a_statement s_lbl1 = {}, s_lbl2 = {}, s_case1 = {}, s_case2 = {}, s_unreach_expr = {};
    s_lbl1.kind = stmk_label; s_lbl1.variant.label.ptr = &lbl1;
    s_lbl2.kind = stmk_label; s_lbl2.variant.label.ptr = &lbl2;
    s_case1.kind = stmk_switch_case; s_case1.variant.switch_case.extra_info = &sce1;
    s_case2.kind = stmk_switch_case; s_case2.variant.switch_case.extra_info = &sce2;
    s_unreach_expr.kind = stmk_expr;

    /* Nested trees with and without labels for stmt_has_label_or_case */
    a_statement u_blk_inner = {}, u_if_then = {}, u_if_else = {}, u_while = {}, u_for = {}, u_sw = {};
    u_blk_inner.kind = stmk_block; u_blk_inner.variant.block.statements = &s_unreach_expr;
    u_if_then.kind = stmk_if; u_if_then.variant.if_stmt.then_statement = &s_lbl2;
    u_if_else.kind = stmk_if_consteval; u_if_else.variant.if_stmt.then_statement = &u_blk_inner; u_if_else.variant.if_stmt.else_statement = &s_case2;
    u_while.kind = stmk_while; u_while.variant.loop_statement = &s_unreach_expr;
    u_for.kind = stmk_for; u_for.variant.for_loop.statement = &u_while;
    a_switch_stmt_descr u_ssdp = {};
    u_sw.kind = stmk_switch; u_sw.variant.switch_stmt.extra_info = &u_ssdp; u_sw.variant.switch_stmt.body_statement = &u_for;

    g_mock_emit_loc = TRUE;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(60, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_jit_function *ufn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "u_cf_fn", 0, NULL, 0);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_lbl1, ufn)); /* !current_block + stmk_label -> reachable! */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_lbl2, ufn)); /* current_block != NULL + stmk_label! */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_case1, ufn)); /* current_block != NULL + stmk_switch_case! */
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_case2, ufn)); /* !current_block + stmk_switch_case! */
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&u_sw, ufn)); /* has_target == FALSE -> skipped! */
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        s_lbl2.variant.label.ptr = &lbl1;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&u_if_then, ufn)); /* has_target == TRUE in then! */
        s_lbl2.variant.label.ptr = &lbl2;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        return gcc_gen_be_lower_statement(&u_if_else, ufn); /* has_target == TRUE in else! */
    }) );
    /* Also test stmt_has_label_or_case finding label inside block -> second statement */
    a_label lbl3 = {}; lbl3.source_corresp.name = "u_lbl3";
    a_statement b_first = {}, b_second = {}, b_wrap = {}, w_has_lbl = {};
    b_first.kind = stmk_empty; b_first.next = &b_second;
    b_second.kind = stmk_label; b_second.variant.label.ptr = &lbl3;
    b_wrap.kind = stmk_block; b_wrap.variant.block.statements = &b_first;
    w_has_lbl.kind = stmk_end_test_while; w_has_lbl.variant.loop_statement = &b_wrap;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&w_has_lbl, fn_void));
    g_mock_emit_loc = FALSE;

    /* OpenMP pragma hook: omp_pragma_count > 0 with matching (omp_handled == TRUE) and non-matching (FALSE) stmt */
    a_statement omp_match_s = {}, omp_nomatch_s = {};
    omp_match_s.kind = stmk_block; omp_match_s.position.seq = 55; omp_match_s.next = &omp_nomatch_s;
    omp_nomatch_s.kind = stmk_empty; omp_nomatch_s.position.seq = 99;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(40, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_gen_be_singleton_state.omp_pragma_count = 0;
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&omp_match_s, NULL, "#pragma omp master"));
        GCC_GEN_BE_CHECK(gcc_gen_be_bind_omp_pragma(&omp_nomatch_s, NULL, "#pragma omp barrier"));
        gcc_jit_block *omp_blk = gcc_jit_function_new_block(fn_void, "omp_blk");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(omp_blk));
        return gcc_gen_be_lower_statement(&omp_match_s, fn_void);
    }) );
    gcc_gen_be_singleton_state.omp_pragma_count = 0;

    /* stmk_block: assoc_scope with local_static_variable_inits, VLA, params, unnamed-static, nonstatic, RAII destructions, and extra_info->lifetime */
    auto mock_builtin_fn = [](gcc_jit_context *c, const char *bname) noexcept -> gcc_jit_function * {
        gcc_jit_type *vpt = gcc_jit_context_get_type(c, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_type *vt = gcc_jit_context_get_type(c, GCC_JIT_TYPE_VOID);
        if (std::strcmp(bname, "__builtin_stack_save") == 0) {
            return gcc_jit_context_new_function(c, NULL, GCC_JIT_FUNCTION_IMPORTED, vpt, "__mock_stack_save", 0, NULL, 0);
        }
        if (std::strcmp(bname, "__builtin_stack_restore") == 0) {
            gcc_jit_param *p = gcc_jit_context_new_param(c, NULL, vpt, "sp");
            return gcc_jit_context_new_function(c, NULL, GCC_JIT_FUNCTION_IMPORTED, vt, "__mock_stack_restore", 1, &p, 0);
        }
        if (std::strcmp(bname, "__builtin_alloca") == 0) {
            gcc_jit_param *p = gcc_jit_context_new_param(c, NULL, gcc_jit_context_get_type(c, GCC_JIT_TYPE_INT), "sz");
            return gcc_jit_context_new_function(c, NULL, GCC_JIT_FUNCTION_IMPORTED, vpt, "__mock_alloca_int", 1, &p, 0);
        }
        return gcc_jit_context_get_builtin_function(c, bname);
    };
    p_gcc_jit_context_get_builtin_function = mock_builtin_fn;
    a_variable v_lsv = {}, v_param = {}, v_vla1 = {}, v_ustatic = {}, v_norm = {}, v_ns_param = {}, v_ns_vla = {}, v_ns_norm = {}, v_raii = {};
    v_lsv.init_kind = (an_init_kind)initk_function_local;
    a_local_static_variable_init lsvi1 = {}, lsvi_null = {};
    lsvi1.variable = &v_lsv; lsvi1.init_kind = (an_init_kind)initk_zero; lsvi1.next = &lsvi_null;
    v_param.is_parameter = TRUE; v_param.next = &v_vla1;
    v_vla1.has_variably_modified_type = TRUE; v_vla1.next = &v_ustatic;
    v_ustatic.storage_class = (a_storage_class)sc_static; v_ustatic.source_corresp.name = NULL; v_ustatic.init_kind = (an_init_kind)initk_static; v_ustatic.next = &v_norm;
    v_norm.source_corresp.name = "sv_norm";
    v_ns_param.is_parameter = TRUE; v_ns_param.next = &v_ns_vla;
    v_ns_vla.has_variably_modified_type = TRUE; v_ns_vla.next = &v_ns_norm;
    v_ns_norm.source_corresp.name = "nsv_norm";
    v_raii.source_corresp.name = "raii_var";

    a_routine raii_dtor = {}; raii_dtor.source_corresp.name = "raii_dtor";
    a_dynamic_init d_raii = {}, d_skip = {};
    d_raii.variable = &v_raii; d_raii.destructor = &raii_dtor; d_raii.next_in_destruction_list = &d_skip;
    an_object_lifetime blk_lt = {};
    blk_lt.destructions = &d_raii;

    a_scope blk_sc = {};
    blk_sc.local_static_variable_inits = &lsvi1;
    blk_sc.variables = &v_param;
    blk_sc.nonstatic_variables = &v_ns_param;
    blk_sc.lifetime = &blk_lt;

    a_block bsupp_sc = {}, bsupp_lt = {};
    bsupp_sc.assoc_scope = &blk_sc;
    bsupp_lt.lifetime = &blk_lt;

    a_statement blk_stmt1 = {}, blk_stmt2 = {};
    blk_stmt1.kind = stmk_block; blk_stmt1.variant.block.extra_info = &bsupp_sc; blk_stmt1.variant.block.statements = &empty_s;
    blk_stmt2.kind = stmk_block; blk_stmt2.variant.block.extra_info = &bsupp_lt;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(80, [&]() noexcept -> gcc_gen_be_error_t {
        v_lsv.init_kind = (an_init_kind)initk_function_local;
        gcc_jit_block *bb = gcc_jit_function_new_block(fn_void, "blk_test");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(bb));
        exceptions_enabled = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&blk_stmt1, fn_void));
        exceptions_enabled = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&blk_stmt2, fn_void));
        exceptions_enabled = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }) );
    /* Also test d_fn == NULL (any_cleanup_reg == FALSE) and current_block == NULL inside block */
    g_mock_lower_fn_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&blk_stmt1, fn_void));
    g_mock_lower_fn_null = FALSE;

    /* stmk_expr & stmk_stmt_expr_result */
    gcc_jit_lvalue *se_loc = gcc_jit_function_new_local(fn_void, NULL, int_t, "se_res");
    an_expr_node dummy_e = {};
    a_dynamic_init se_dip = {};
    a_statement s_expr = {}, s_ser1 = {}, s_ser2 = {}, s_ser3 = {};
    s_expr.kind = stmk_expr; s_expr.expr = &dummy_e;
    s_ser1.kind = stmk_stmt_expr_result; s_ser1.expr = &dummy_e;
    s_ser2.kind = stmk_stmt_expr_result; s_ser2.variant.stmt_expr_result.dynamic_init = &se_dip;
    s_ser3.kind = stmk_stmt_expr_result;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(40, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_jit_block *be = gcc_jit_function_new_block(fn_void, "expr_ser");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(be));
        g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_expr, fn_void));
        current_stmt_expr_result_lval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ser1, fn_void));
        current_stmt_expr_result_lval = se_loc;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ser1, fn_void));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ser2, fn_void));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ser3, fn_void));
        g_mock_expr_clear_block = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ser1, fn_void));
        g_mock_expr_clear_block = FALSE;
        current_stmt_expr_result_lval = NULL;
        g_mock_expr_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }) );

    /* stmk_return matrix (with expr & without expr, sret, void, 0-param, matching param, pointer-cast param, mismatched param, struct coerce, scalar cast) */
    a_type tref_u = {}, union_tp = {}, int_edg_tp = {};
    union_tp.kind = tk_union;
    tref_u.kind = tk_typeref; tref_u.variant.typeref.type = &union_tp;
    int_edg_tp.kind = tk_integer;
    an_expr_node ret_e = {}; ret_e.type = &tref_u;
    a_statement s_ret_e = {}, s_ret_noe = {};
    s_ret_e.kind = stmk_return; s_ret_e.expr = &ret_e;
    s_ret_noe.kind = stmk_return;

    gcc_jit_param *sret_p = gcc_jit_context_new_param(ctx, NULL, ip_t, "sret_ptr");
    gcc_jit_function *fn_sret = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "cf_sret_fn", 1, &sret_p, 0);

    GCC_GEN_BE_CHECK( sweep_oom_stmt(120, [&]() noexcept -> gcc_gen_be_error_t {
        /* 1. sret cached + rval != NULL */
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, fn_sret, sret_p));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_sret, "r1")));
        g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_sret));

        /* 2. void return fn with expr & without expr */
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_void, "r2")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_void));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_void, "r3")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_noe, fn_void));

        /* 3. rval == NULL on 0-param, matching-param, pointer-cast-param, mismatched-param (both with & without expr!) */
        g_mock_expr_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int0, "r4")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_int0));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int_pi, "r5")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_int_pi));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_vp_pip, "r6")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_vp_pip));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int_pvp, "r7")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_int_pvp));

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int0, "r8")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_noe, fn_int0));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int_pi, "r9")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_noe, fn_int_pi));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_vp_pip, "r10")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_noe, fn_vp_pip));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int_pvp, "r11")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_noe, fn_int_pvp));

        /* 4. rval != NULL: exact match, struct coerce (non-NULL & NULL coerced_ret), and scalar cast */
        g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int0, "r12")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_int0));

        ret_e.type = &tref_u;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_st, "r13")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_st));

        g_mock_coerce_null = TRUE;
        g_mock_expr_rval = gcc_jit_context_zero(ctx, bool_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int0, "r14")));
        /* With union_tp, coerce returns NULL -> rval stays bool -> wait: fn_int0 expects int, so test coerce_null when rval ALREADY matches fn_st? No, rval != fn_ret_t! Use g_mock_coerced_rval = zero(int_t) vs scalar cast! */
        g_mock_coerce_null = FALSE;
        ret_e.type = &int_edg_tp;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_int0)); /* scalar cast bool -> int */
        g_mock_expr_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }) );

    /* stmk_goto (with goto_lt != NULL, NULL + primary_scope == NULL, NULL + primary_scope != NULL) */
    a_scope prim_sc = {};
    a_statement s_goto = {};
    s_goto.kind = stmk_goto; s_goto.variant.label.ptr = &lbl1;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(35, [&]() noexcept -> gcc_gen_be_error_t {
        il_header.primary_scope = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_void, "g1")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_goto, fn_void));
        il_header.primary_scope = &prim_sc;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_void, "g2")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_goto, fn_void));
        il_header.primary_scope = NULL;
        return GCC_GEN_BE_SUCCESS;
    }) );

    /* stmk_if & stmk_constexpr_if (pointer cond, bool cond, int cond, NULL expr, empty & terminating branches) */
    a_constexpr_if cif = {};
    a_statement s_if = {}, s_cif1 = {}, s_cif2 = {};
    s_if.kind = stmk_if; s_if.expr = &dummy_e; s_if.variant.if_stmt.then_statement = &s_ret_noe; s_if.variant.if_stmt.else_statement = &s_ret_noe;
    cif.then_statement = &empty_s; cif.else_statement = &empty_s;
    s_cif1.kind = stmk_constexpr_if; s_cif1.variant.constexpr_if = &cif;
    a_constexpr_if cif_empty = {};
    s_cif2.kind = stmk_constexpr_if; s_cif2.expr = &dummy_e; s_cif2.variant.constexpr_if = &cif_empty;
    a_constexpr_if cif_true = {}, cif_false = {};
    cif_true.value_known = TRUE; cif_true.value = TRUE; cif_true.then_statement = &empty_s;
    cif_false.value_known = TRUE; cif_false.value = FALSE; cif_false.else_statement = NULL;
    a_statement s_cif_null = {}, s_cif_true = {}, s_cif_false = {};
    s_cif_null.kind = stmk_constexpr_if; s_cif_null.variant.constexpr_if = NULL;
    s_cif_true.kind = stmk_constexpr_if; s_cif_true.variant.constexpr_if = &cif_true;
    s_cif_false.kind = stmk_constexpr_if; s_cif_false.variant.constexpr_if = &cif_false;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(70, [&]() noexcept -> gcc_gen_be_error_t {
        /* Both branches return -> merge_block skipped (`else` sets current_block = NULL)! */
        g_mock_expr_rval = gcc_jit_context_null(ctx, vp_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_void, "if1")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_if, fn_void));

        g_mock_expr_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_void, "if2")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_cif_null, fn_void));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_cif_true, fn_void));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_cif_false, fn_void));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_cif1, fn_void));

        g_mock_expr_rval = gcc_jit_context_zero(ctx, bool_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_cif2, fn_void));
        g_mock_expr_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }) );
    /* Also regular stmk_if with NULL then/else */
    a_statement s_if_nulls = {}; s_if_nulls.kind = stmk_if_not_consteval;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_void, "if3")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_if_nulls, fn_void));

    /* stmk_switch: ssdp == NULL, range cases, NULL/non-int case_value, with & without default_case */
    a_statement s_sw_bad = {}; s_sw_bad.kind = stmk_switch;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&empty_s, fn_void));
    if (gcc_gen_be_lower_statement(&s_sw_bad, fn_void) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    a_constant c_lo = {}, c_hi = {}, c_flt = {};
    c_lo.kind = (a_constant_repr_kind)ck_integer;
    c_hi.kind = (a_constant_repr_kind)ck_integer;
    c_flt.kind = (a_constant_repr_kind)ck_float;
    a_switch_case_entry sc1 = {}, sc2 = {}, sc_d = {};
    sc1.case_value = &c_lo; sc1.range_end = &c_hi; sc1.next = &sc2;
    sc2.case_value = &c_flt; sc2.range_end = &c_flt; sc2.next = &sc_d;
    a_switch_stmt_descr sw_d1 = {}, sw_d2 = {};
    sw_d1.cases = &sc1; sw_d1.default_case = &sc_d;
    sw_d2.cases = NULL; sw_d2.default_case = NULL;

    a_statement sw_body_fall = {}, s_sw1 = {}, s_sw2 = {};
    sw_body_fall.kind = stmk_switch_case; sw_body_fall.variant.switch_case.extra_info = &sc1;
    s_sw1.kind = stmk_switch; s_sw1.expr = &dummy_e; s_sw1.variant.switch_stmt.extra_info = &sw_d1; s_sw1.variant.switch_stmt.body_statement = &sw_body_fall;
    s_sw2.kind = stmk_switch; s_sw2.variant.switch_stmt.extra_info = &sw_d2; s_sw2.variant.switch_stmt.body_statement = &sw_body_fall;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(60, [&]() noexcept -> gcc_gen_be_error_t {
        g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_void, "sw1")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_sw1, fn_void));
        g_mock_expr_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_void, "sw2")));
        return gcc_gen_be_lower_statement(&s_sw2, fn_void);
    }) );

    /* Loops: stmk_end_test_while, stmk_while, stmk_for, stmk_try_block, stmk_range_based_for, stmk_for_each */
    a_for_loop fl_info = {};
    fl_info.initialization = &empty_s;
    fl_info.increment = &dummy_e;
    a_statement s_dow = {}, s_dow_ret = {}, s_wh = {}, s_for = {}, s_try = {}, s_rfor = {}, s_feach = {};
    s_dow.kind = stmk_end_test_while; s_dow.expr = &dummy_e; s_dow.variant.loop_statement = &empty_s;
    s_dow_ret.kind = stmk_end_test_while; s_dow_ret.variant.loop_statement = &s_ret_noe;
    s_wh.kind = stmk_while; s_wh.expr = &dummy_e; s_wh.variant.loop_statement = &empty_s;
    s_for.kind = stmk_for; s_for.expr = &dummy_e; s_for.variant.for_loop.extra_info = &fl_info; s_for.variant.for_loop.statement = &empty_s;
    s_try.kind = stmk_try_block;
    s_rfor.kind = stmk_range_based_for; s_rfor.variant.range_based_for_loop.statement = &empty_s;
    s_feach.kind = stmk_for_each; s_feach.variant.for_each_loop.statement = &empty_s;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(80, [&]() noexcept -> gcc_gen_be_error_t {
        g_mock_expr_rval = gcc_jit_context_null(ctx, vp_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_void, "lp1")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_dow, fn_void));
        g_mock_expr_rval = gcc_jit_context_zero(ctx, bool_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_wh, fn_void));
        g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_for, fn_void));
        g_mock_expr_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_dow, fn_void));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_wh, fn_void));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_for, fn_void));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_try, fn_void));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_rfor, fn_void));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_feach, fn_void));
        return gcc_gen_be_lower_statement(&s_dow_ret, fn_void);
    }) );

    /* VLA statements & gcc_gen_be_lower_vla_decl + stack_save/restore */
    gcc_jit_lvalue *saved_sp = NULL;
    gcc_jit_block *vla_blk = gcc_jit_function_new_block(fn_void, "vla_blk");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_emit_stack_save(fn_void, vla_blk, &saved_sp) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_stack_restore(fn_void, vla_blk, se_loc) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_vla_decl(fn_void, &v_vla1, &saved_sp) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    p_gcc_jit_context_get_builtin_function = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_stack_save(fn_void, vla_blk, &saved_sp));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_stack_restore(fn_void, vla_blk, se_loc));
    p_gcc_jit_context_get_builtin_function = mock_builtin_fn;

    a_variable dim_var1 = {}; dim_var1.source_corresp.name = "vla_dim_n";
    a_type vla_elem_tp = {}, vla_fixed_arr = {}, vla_dyn_arr = {}, tref_vla = {};
    vla_elem_tp.kind = tk_integer; vla_elem_tp.size = 0; vla_elem_tp.alignment = 1; /* elem_bytes == 0 -> 1, align == 1 */
    vla_fixed_arr.kind = tk_array; vla_fixed_arr.variant.array.variant.number_of_elements = 4; vla_fixed_arr.variant.array.element_type = &vla_elem_tp;
    vla_dyn_arr.kind = tk_array; vla_dyn_arr.variant.array.has_assoc_vla_dimension = TRUE; vla_dyn_arr.variant.array.element_type = &vla_fixed_arr;
    tref_vla.kind = tk_typeref; tref_vla.variant.typeref.type = &vla_dyn_arr;

    a_vla_dimension vd_orig = {}, vd_proxy = {}, vd_other = {};
    vd_orig.type = &vla_dyn_arr; vd_orig.dimension_variable = &dim_var1; vd_orig.dimension_expr = &dummy_e;
    vd_proxy.type = &vla_dyn_arr; vd_proxy.original_dimension = &vd_orig;
    vd_other.type = &vla_fixed_arr; vd_other.next = &vd_proxy;

    a_scope vla_fn_sc = {};
    vla_fn_sc.vla_dimensions = &vd_other;
    innermost_function_scope = &vla_fn_sc;

    a_variable vla_var = {};
    vla_var.type = &tref_vla;
    g_mock_lower_type_null = TRUE;
    if (gcc_gen_be_lower_vla_decl(fn_void, &vla_var, &saved_sp) != GCC_GEN_BE_ERROR_VLA_ALLOCATION_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_lower_type_null = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    if (gcc_gen_be_lower_vla_decl(fn_void, &vla_var, &saved_sp) != GCC_GEN_BE_ERROR_VLA_ALLOCATION_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    a_statement s_set_vla = {}, s_vla_decl = {};
    s_set_vla.kind = stmk_set_vla_size; s_set_vla.variant.vla_dimension = &vd_orig; s_set_vla.next = &s_vla_decl;
    s_vla_decl.kind = stmk_vla_decl; s_vla_decl.variant.vla.variant.variable = &vla_var;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(60, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(vla_blk));
        g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
        vla_elem_tp.alignment = 1;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_set_vla, fn_void));
        /* Second call hits cached dim_var1 AND aligned > 1 with & without bitcast! */
        vla_elem_tp.size = 4; vla_elem_tp.alignment = 16;
        p_gcc_jit_context_new_bitcast = [](gcc_jit_context *c, gcc_jit_location *, gcc_jit_rvalue *, gcc_jit_type *dt) noexcept -> gcc_jit_rvalue * {
            return gcc_jit_context_zero(c, dt);
        };
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_vla_decl(fn_void, &vla_var, &saved_sp));
        p_gcc_jit_context_new_bitcast = NULL;
        g_mock_coerced_rval = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_vla_decl(fn_void, &vla_var, &saved_sp));
        g_mock_coerced_rval = NULL;
        g_mock_expr_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }) );
    innermost_function_scope = NULL;

    /* stmk_decl & stmk_init (dik_none static/zero, local dynamic init) */
    a_variable dv_local = {}, dv_ext = {}, dv_coro = {};
    dv_local.source_corresp.name = "decl_loc";
    dv_ext.storage_class = (a_storage_class)sc_extern;
    dv_coro.source_corresp.name = "decl_coro";
    an_il_entity_list_entry de1 = {}, de2 = {}, de3 = {}, de_type = {};
    de1.entity.kind = (an_il_entry_kind)iek_variable; de1.entity.ptr = reinterpret_cast<char *>(&dv_local); de1.next = &de2;
    de2.entity.kind = (an_il_entry_kind)iek_variable; de2.entity.ptr = reinterpret_cast<char *>(&dv_ext); de2.next = &de3;
    de3.entity.kind = (an_il_entry_kind)iek_variable; de3.entity.ptr = reinterpret_cast<char *>(&dv_coro); de3.next = &de_type;
    de_type.entity.kind = (an_il_entry_kind)iek_type;

    a_statement s_decl = {}; s_decl.kind = stmk_decl; s_decl.variant.decl.entities = &de1;
    if (gcc_gen_be_lower_statement(&s_decl, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    a_constant init_c = {}; init_c.kind = (a_constant_repr_kind)ck_integer;
    a_variable iv_c = {}, iv_z = {}, iv_dyn = {};
    iv_c.init_kind = (an_init_kind)initk_static; iv_c.initializer.constant = &init_c; iv_c.source_corresp.name = "iv_c";
    iv_z.init_kind = (an_init_kind)initk_zero; iv_z.source_corresp.name = "iv_z";
    iv_dyn.source_corresp.name = "iv_dyn";
    a_dynamic_init dip_c = {}, dip_z = {}, dip_dyn = {};
    dip_c.kind = (a_dynamic_init_kind)dik_none; dip_c.variable = &iv_c;
    dip_z.kind = (a_dynamic_init_kind)dik_none; dip_z.variable = &iv_z;
    dip_dyn.kind = (a_dynamic_init_kind)dik_expression; dip_dyn.variable = &iv_dyn;
    a_statement si_c = {}, si_z = {}, si_dyn = {};
    si_c.kind = stmk_init; si_c.variant.dynamic_init = &dip_c; si_c.next = &si_z;
    si_z.kind = stmk_init; si_z.variant.dynamic_init = &dip_z; si_z.next = &si_dyn;
    si_dyn.kind = stmk_init; si_dyn.variant.dynamic_init = &dip_dyn;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(60, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(vla_blk));
        gcc_gen_be_singleton_state.active_coro_frame_lval = se_loc;
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &dv_coro, se_loc));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_decl, fn_void));
        gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
        return gcc_gen_be_lower_statement(&si_c, fn_void);
    }) );

    /* stmk_asm_func_body, stmk_asm, and unhandled default statement */
    a_constant asm_str_c = {};
    asm_str_c.kind = (a_constant_repr_kind)ck_string;
    asm_str_c.variant.string.value = "nop";
    an_asm_operand aop_rw = {}, aop_w = {}, aop_in = {};
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
    aop_rw.name = const_cast<char *>("out0"); aop_rw.is_output_operand = TRUE; aop_rw.constraints_string = "+r"; aop_rw.expression = &dummy_e; aop_rw.next = &aop_w;
    aop_w.name = const_cast<char *>("out1"); aop_w.is_output_operand = TRUE; aop_w.constraints_string = "=r"; aop_w.expression = &dummy_e; aop_w.next = &aop_in;
    aop_in.name = const_cast<char *>("in0"); aop_in.is_output_operand = FALSE; aop_in.constraints_string = "r"; aop_in.expression = &dummy_e;
#else
    asm_operand_constraint_letters[(int)aoc_reg_integer] = 'r';
    an_asm_operand_constraint aoc_r = {};
    aoc_r.kind = aoc_reg_integer;
    an_asm_operand_constraint aoc_long[64] = {};
    for (int ci = 0; ci < 64; ++ci) {
        aoc_long[ci].kind = aoc_reg_integer;
        aoc_long[ci].next = (ci + 1 < 64) ? &aoc_long[ci + 1] : NULL;
    }
    aop_rw.name = const_cast<char *>("out0"); aop_rw.modifiers = (an_asm_operand_modifier)(aom_output | aom_input); aop_rw.constraints = &aoc_r; aop_rw.expression = &dummy_e; aop_rw.next = &aop_w;
    aop_w.name = const_cast<char *>("out1"); aop_w.modifiers = aom_output; aop_w.constraints = &aoc_r; aop_w.expression = &dummy_e; aop_w.next = &aop_in;
    aop_in.name = const_cast<char *>("in0"); aop_in.modifiers = aom_input; aop_in.constraints = &aoc_long[0]; aop_in.expression = &dummy_e;
#endif

    a_named_register_list clob1 = {}, clob2 = {}, clob3 = {};
    clob1.reg = (a_named_register)anr_memory; clob1.next = &clob2;
    clob2.reg = (a_named_register)anr_invalid; clob2.next = &clob3;
    clob3.reg = (a_named_register)((int)anr_last - 1);
    if ((int)anr_last > 2) {
        named_register_names[(int)anr_last - 1] = "r0";
    }

    an_asm_entry aent = {};
    aent.asm_string = &asm_str_c;
    aent.is_volatile = TRUE;
    aent.operands = &aop_rw;
    aent.clobbers = &clob1;

    a_statement s_asm = {}, s_asm_body = {}, s_bad_kind = {};
    s_asm.kind = stmk_asm; s_asm.variant.asm_entry = &aent;
    s_asm_body.kind = stmk_asm_func_body; s_asm_body.variant.asm_func_body = "ret";
    s_bad_kind.kind = (a_statement_kind)250;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(40, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(vla_blk));
        g_mock_expr_lval = se_loc;
        g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_asm, fn_void));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_asm_body, fn_void));
        g_mock_expr_lval = NULL;
        g_mock_expr_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }) );
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(vla_blk));
    if (gcc_gen_be_lower_statement(&s_bad_kind, fn_void) != GCC_GEN_BE_ERROR_UNHANDLED_STMT) return GCC_GEN_BE_ERROR_INTERNAL;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests all C++20 coroutine lowering functions (`frame`, `ramp`, `dispatchers`, `HALO`, `noop`, `symmetric_transfer`, `stmk_coroutine`).
 * @details Exercises `scan_stmt_for_coro_temps`, `accumulate_coro_stmt_temp_sizes`, >16-field frame reallocations,
 * `emit_coro_scope_dtors`, `map_coro_scope_vars_to_frame`, HALO stack frames, over-aligned operator new/delete,
 * `unhandled_exception` / `final_suspend` paths, and deterministic OOM sweeps across all coroutine entry points.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_coroutines_and_oom(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *ip_t = gcc_jit_type_get_pointer(int_t);

    /* Active coroutine frame & suspend-point registration (including >8 capacity doubling and OOM sweeps) */
    gcc_jit_type *out_ft = NULL;
    gcc_jit_lvalue *out_fl = NULL;
    gcc_jit_rvalue *out_fr = NULL;
    int state_idx = 0;
    if (gcc_gen_be_get_active_coroutine_frame(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_active_coroutine_frame(&out_ft, &out_fl));
    if (gcc_gen_be_register_coro_suspend_point(NULL, NULL, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_register_coro_suspend_point(NULL, &state_idx, &out_fl, &out_fr) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_function *dummy_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "coro_susp_host", 0, NULL, 0);
    gcc_jit_block *rblk = gcc_jit_function_new_block(dummy_fn, "rblk");
    /* Inactive coroutine -> state_idx == 0 */
    GCC_GEN_BE_CHECK(gcc_gen_be_register_coro_suspend_point(rblk, &state_idx, &out_fl, &out_fr));

    /* Rich coroutine scope & AST exercising ALL statement/expression temporary scanners & accumulators */
    a_type overaligned_tp = {}, small_tp = {}, tref_oa = {};
    overaligned_tp.kind = tk_struct; overaligned_tp.size = 32; overaligned_tp.alignment = 32;
    small_tp.kind = tk_integer; small_tp.size = 2; small_tp.alignment = 0;
    tref_oa.kind = tk_typeref; tref_oa.variant.typeref.type = &overaligned_tp;

    an_expr_node te_inner = {}, te_main = {}, te_op = {}, te_await = {};
    a_dynamic_init te_dip = {};
    te_inner.kind = enk_temp_init; te_inner.type = &small_tp;
    te_dip.kind = (a_dynamic_init_kind)dik_expression; te_dip.variant.expression = &te_inner;
    te_main.kind = enk_temp_init; te_main.type = &tref_oa; te_main.variant.init.dynamic_init = &te_dip; te_main.next = &te_op;
    te_op.kind = enk_operation; te_op.variant.operation.operands = &te_inner; te_op.next = &te_await;
    te_await.kind = enk_await; te_await.variant.await_info.operand = &te_inner; te_await.variant.await_info.ready_resume_suspend = &te_inner;

    a_statement cs_empty = {}, cs_blk = {}, cs_if = {}, cs_cif = {}, cs_wh = {}, cs_for = {}, cs_rfor = {}, cs_sw = {}, cs_try = {}, cs_init = {};
    cs_empty.kind = stmk_empty; cs_empty.expr = &te_main;
    cs_blk.kind = stmk_block; cs_blk.variant.block.statements = &cs_empty; cs_blk.next = &cs_if;
    cs_if.kind = stmk_if; cs_if.variant.if_stmt.then_statement = &cs_empty; cs_if.variant.if_stmt.else_statement = &cs_empty; cs_if.next = &cs_cif;
    a_constexpr_if coro_cif = {}; coro_cif.then_statement = &cs_empty; coro_cif.else_statement = &cs_empty;
    cs_cif.kind = stmk_constexpr_if; cs_cif.variant.constexpr_if = &coro_cif; cs_cif.next = &cs_wh;
    cs_wh.kind = stmk_while; cs_wh.variant.loop_statement = &cs_empty; cs_wh.next = &cs_for;
    a_for_loop coro_fl = {}; coro_fl.initialization = &cs_empty; coro_fl.increment = &te_inner;
    cs_for.kind = stmk_for; cs_for.variant.for_loop.extra_info = &coro_fl; cs_for.variant.for_loop.statement = &cs_empty; cs_for.next = &cs_rfor;
    cs_rfor.kind = stmk_range_based_for; cs_rfor.variant.range_based_for_loop.statement = &cs_empty; cs_rfor.next = &cs_sw;
    a_switch_stmt_descr coro_swd = {};
    cs_sw.kind = stmk_switch; cs_sw.variant.switch_stmt.extra_info = &coro_swd; cs_sw.variant.switch_stmt.body_statement = &cs_empty; cs_sw.next = &cs_try;
    a_handler coro_h = {}; coro_h.statement = &cs_empty;
    a_try_supplement coro_ts = {}; coro_ts.statement = &cs_empty; coro_ts.handlers = &coro_h;
    cs_try.kind = stmk_try_block; cs_try.variant.try_block = &coro_ts; cs_try.next = &cs_init;
    cs_init.kind = stmk_init; cs_init.variant.dynamic_init = &te_dip;

    /* 18 copied parameters -> forces capacity 16 -> 32 realloc in both stmk_coroutine and compute_coro_frame_layout! */
    a_variable prom_v = {}, iaw_v = {}, hdl_v = {}, pcopies[18] = {}, fparam_v = {}, nsv_v = {}, sv_v = {}, ltv_v = {};
    prom_v.type = &tref_oa; prom_v.init_kind = (an_init_kind)initk_dynamic;
    a_routine prom_dtor_r = {}, ltv_dtor_r = {}, custom_new_r = {}, custom_del_r = {};
    prom_dtor_r.source_corresp.name = "prom_dtor";
    ltv_dtor_r.source_corresp.name = "ltv_dtor";
    custom_new_r.source_corresp.name = "op_new";
    custom_del_r.source_corresp.name = "op_del";

    a_dynamic_init prom_di = {}, ltv_di = {};
    prom_di.destructor = &prom_dtor_r;
    prom_v.initializer.dynamic = &prom_di;
    iaw_v.type = &small_tp;
    hdl_v.type = &small_tp;
    for (int i = 0; i < 18; ++i) {
        pcopies[i].type = &small_tp;
        pcopies[i].next = (i + 1 < 18) ? &pcopies[i + 1] : &prom_v;
    }
    prom_v.next = &iaw_v;
    iaw_v.next = &hdl_v;

    fparam_v.type = &small_tp; fparam_v.is_parameter = TRUE;
    nsv_v.type = &small_tp;
    sv_v.type = &small_tp;
    ltv_v.type = &small_tp;
    ltv_di.variable = &ltv_v; ltv_di.destructor = &ltv_dtor_r;
    an_object_lifetime coro_lt = {}; coro_lt.destructions = &ltv_di;

    a_scope child_coro_sc = {}, coro_def_sc = {};
    child_coro_sc.lifetime = &coro_lt;
    coro_def_sc.kind = sck_function;
    coro_def_sc.assoc_block = &cs_blk;
    coro_def_sc.variant.routine.parameters = &fparam_v;
    coro_def_sc.nonstatic_variables = &nsv_v;
    coro_def_sc.variables = &sv_v;
    coro_def_sc.lifetime = &coro_lt;
    coro_def_sc.scopes = &child_coro_sc;

    a_function_def_descr fdd_table[2] = {};
    fdd_table[1].scope = &coro_def_sc;
    il_header.function_def_table = fdd_table;

    an_expr_node gro_e = {}, fail_gro_e = {}, isusp_e = {}, fsusp_e = {}, uexc_e = {};
    a_label fsusp_lbl = {}; fsusp_lbl.source_corresp.name = "final_suspend_lbl";
    a_coroutine_descr cdescr = {};
    cdescr.promise = &prom_v;
    cdescr.init_await_resume = &iaw_v;
    cdescr.handle = &hdl_v;
    cdescr.parameter_copies = &pcopies[0];
    cdescr.get_return_object_call = &gro_e;
    cdescr.initial_suspend_call = &isusp_e;
    cdescr.final_suspend_call = &fsusp_e;
    cdescr.unhandled_exception_call = &uexc_e;
    g_mock_coro_descr = &cdescr;

    a_routine coro_rout = {};
    coro_rout.function_def_number = 1;

    /* 1. Frame size/align, layout, and lower_coroutine_frame (including invalid args, cached struct & cached non-struct!) */
    size_t fsz = 0, fal = 0;
    gcc_jit_struct *fst = NULL;
    if (gcc_gen_be_compute_coro_frame_size_and_align(NULL, NULL, &fsz, &fal) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_compute_coro_frame_layout(&coro_rout, NULL, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(250, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_coroutine_frame(&coro_rout, &out_ft);
    }) );
    /* Cached struct hit */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_frame(&coro_rout, &out_ft));
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&coro_rout, &coro_def_sc, &fst, &fsz));
    /* Cached non-struct type -> compute_coro_frame_layout returns NULL struct -> lower_coroutine_frame returns ERROR_INTERNAL */
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, &coro_rout, int_t));
    if (gcc_gen_be_lower_coroutine_frame(&coro_rout, &out_ft) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;

    /* 2. Ramp & Dispatchers across HALO, custom new/delete, over-aligned, standard _Znwm/_ZdlPv, sret, final_suspend_label, and suspend dtors! */
    /* Replace assoc_block with a lightweight block that registers 9 suspend points + co_return during dispatcher lowering! */
    a_statement cret_stmt = {};
    cret_stmt.kind = stmk_coroutine_return;
    cret_stmt.expr = &gro_e;
    coro_def_sc.assoc_block = &cret_stmt;

    gcc_jit_param *rp0 = gcc_jit_context_new_param(ctx, NULL, int_t, "rp0");
    gcc_jit_function *ramp_int = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "coro_ramp_int", 1, &rp0, 0);
    gcc_jit_function *ramp_void = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "coro_ramp_void", 0, NULL, 0);

    /* 2a: Over-aligned frame (>16B) + unhandled_exception + final_suspend_call + OOM sweep! */
    gcc_be_opt_level = 0;
    g_mock_expr_rval = gcc_jit_context_null(ctx, vp_t); /* triggers coerce_rvalue on ramp_int! */
    GCC_GEN_BE_CHECK( sweep_oom_stmt(400, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_coroutine_ramp(&coro_rout, ramp_int);
    }) );

    /* 2b: HALO stack elision (gcc_be_opt_level = 2) + sret_cached + final_suspend_label cached! */
    gcc_be_opt_level = 2;
    cdescr.final_suspend_label = &fsusp_lbl;
    gcc_jit_param *ramp_sret_p = gcc_jit_context_new_param(ctx, NULL, ip_t, "sret_out");
    gcc_jit_function *ramp_sret = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "coro_ramp_sret", 1, &ramp_sret_p, 0);
    g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
    GCC_GEN_BE_CHECK( sweep_oom_stmt(350, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, ramp_sret, ramp_sret_p));
        return gcc_gen_be_lower_coroutine_ramp(&coro_rout, ramp_sret);
    }) );
    gcc_be_opt_level = 0;
    cdescr.final_suspend_label = NULL;

    /* 2c: Custom operator new (3 params > ramp 0 params) + alloc_failure_gro_call + custom delete + <=16B standard alignment! */
    overaligned_tp.size = 8; overaligned_tp.alignment = 8;
    cdescr.new_routine = &custom_new_r;
    cdescr.delete_routine = &custom_del_r;
    cdescr.alloc_failure_gro_call = &fail_gro_e;
    g_mock_fn_param_count = 3;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(350, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_coroutine_ramp(&coro_rout, ramp_void);
    }) );
    /* Standard <=16B _Znwm / _ZdlPv (new_routine == NULL, delete_routine == NULL, align == 8) + alloc_failure on non-void ramp! */
    cdescr.new_routine = NULL;
    cdescr.delete_routine = NULL;
    g_mock_fn_param_count = 1;
    g_mock_expr_rval = NULL;
    gcc_jit_function *ramp_int2 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "coro_ramp_int2", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&coro_rout, ramp_int2));
    cdescr.alloc_failure_gro_call = NULL;

    /* 2d: Dispatcher with registered suspend points + emit_coro_scope_dtors (with child scope, non-cached & null dtor_fn, and OOM sweep)! */
    a_dynamic_init d_unmapped = {}, d_no_dtor = {};
    a_variable unmapped_v = {};
    d_unmapped.variable = &unmapped_v; d_unmapped.destructor = &ltv_dtor_r; d_unmapped.next_in_destruction_list = &d_no_dtor;
    ltv_di.next_in_destruction_list = &d_unmapped;
    a_statement s_coro_try = {}, body_lbl_fs = {};
    s_coro_try.kind = stmk_try_block; s_coro_try.next = &body_lbl_fs;
    body_lbl_fs.kind = stmk_label; body_lbl_fs.variant.label.ptr = &fsusp_lbl;
    coro_def_sc.assoc_block = &s_coro_try;
    cdescr.final_suspend_label = &fsusp_lbl;
    gcc_jit_function *res_fn = NULL, *dest_fn = NULL;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(400, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_gen_be_singleton_state.active_coro_current_scope = &coro_def_sc;
        gcc_gen_be_error_t derr = gcc_gen_be_lower_coroutine_dispatchers(&coro_rout, &res_fn, &dest_fn);
        gcc_gen_be_singleton_state.active_coro_current_scope = NULL;
        return derr;
    }) );
    /* Also run with g_mock_lower_fn_null = TRUE so dtor_fn == NULL and prom_dtor == NULL branches are hit! */
    g_mock_lower_fn_null = TRUE;
    gcc_gen_be_singleton_state.active_coro_current_scope = &coro_def_sc;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_dispatchers(&coro_rout, &res_fn, &dest_fn));
    gcc_gen_be_singleton_state.active_coro_current_scope = NULL;
    g_mock_lower_fn_null = FALSE;
    ltv_di.next_in_destruction_list = NULL;
    cdescr.final_suspend_label = NULL;

    /* Directly test register_coro_suspend_point (>8 points, partial NULL out-ptrs, state_field == NULL, ctx == NULL) */
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_frame(&coro_rout, &out_ft));
    gcc_jit_lvalue *active_loc = gcc_jit_function_new_local(dummy_fn, NULL, gcc_jit_type_get_pointer(out_ft), "af");
    gcc_gen_be_singleton_state.active_coro_frame_lval = active_loc;
    gcc_gen_be_singleton_state.active_coro_frame_type = out_ft;
    gcc_gen_be_singleton_state.active_coro_state_field = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_register_coro_suspend_point(rblk, &state_idx, NULL, NULL)); /* !active_coro_state_field */
    gcc_gen_be_singleton_state.active_coro_state_field = gcc_jit_struct_get_field(gcc_jit_type_is_struct(out_ft), 3);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_register_coro_suspend_point(rblk, &state_idx, &out_fl, &out_fr) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_active_coroutine_frame(&out_ft, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_active_coroutine_frame(NULL, &out_fl));

    GCC_GEN_BE_CHECK( sweep_oom_stmt(40, [&]() noexcept -> gcc_gen_be_error_t {
        if (gcc_gen_be_singleton_state.active_coro_resume_blocks) {
            std::free(gcc_gen_be_singleton_state.active_coro_resume_blocks);
            gcc_gen_be_singleton_state.active_coro_resume_blocks = NULL;
        }
        if (gcc_gen_be_singleton_state.active_coro_suspend_scopes) {
            std::free(gcc_gen_be_singleton_state.active_coro_suspend_scopes);
            gcc_gen_be_singleton_state.active_coro_suspend_scopes = NULL;
        }
        gcc_gen_be_singleton_state.active_coro_resume_blocks_count = 0;
        gcc_gen_be_singleton_state.active_coro_resume_blocks_capacity = 0;
        for (int i = 0; i < 10; ++i) {
            GCC_GEN_BE_CHECK(gcc_gen_be_register_coro_suspend_point(rblk, &state_idx, &out_fl, &out_fr));
        }
        return GCC_GEN_BE_SUCCESS;
    }) );
    std::free(gcc_gen_be_singleton_state.active_coro_resume_blocks);
    std::free(gcc_gen_be_singleton_state.active_coro_suspend_scopes);
    gcc_gen_be_singleton_state.active_coro_resume_blocks = NULL;
    gcc_gen_be_singleton_state.active_coro_suspend_scopes = NULL;
    gcc_gen_be_singleton_state.active_coro_resume_blocks_count = 0;
    gcc_gen_be_singleton_state.active_coro_resume_blocks_capacity = 0;
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
    gcc_gen_be_singleton_state.active_coro_frame_type = NULL;
    gcc_gen_be_singleton_state.active_coro_state_field = NULL;
    gcc_gen_be_singleton_state.active_coro_current_scope = NULL;

    /* 3. stmk_coroutine (in-function lowering with fresh param, dynamic promise, custom new/del, OOM sweep) */
    a_statement st_coro = {};
    st_coro.kind = stmk_coroutine;
    st_coro.variant.coroutine.descr = &cdescr;
    cdescr.new_routine = &custom_new_r;
    cdescr.delete_routine = &custom_del_r;
    cdescr.alloc_failure_gro_call = &fail_gro_e;
    g_mock_fn_param_count = 3; /* p=1 < ramp_pcount(1), p=2 >= ramp_pcount(1)! */
    g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
    innermost_function_scope = &coro_def_sc;
    coro_def_sc.assoc_block = &cs_blk;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(450, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
        gcc_jit_param *cf_p0 = gcc_jit_context_new_param(ctx, NULL, vp_t, "cf_p0");
        gcc_jit_function *cf = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "in_fn_coro", 1, &cf_p0, 0);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(cf, "entry")));
        return gcc_gen_be_lower_statement(&st_coro, cf);
    }) );
    /* Second stmk_coroutine while active_coro_frame_lval != NULL -> immediate break! */
    gcc_jit_block *already_blk = gcc_jit_function_new_block(dummy_fn, "already_coro");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(already_blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&st_coro, dummy_fn));
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
    innermost_function_scope = NULL;
    g_mock_fn_param_count = 1;
    g_mock_expr_rval = NULL;

    /* stmk_coroutine with default _Znwm / _ZdlPv, void fn, zero-init promise, and descr == NULL */
    cdescr.new_routine = NULL;
    cdescr.delete_routine = NULL;
    prom_v.init_kind = (an_init_kind)initk_zero;
    g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
    gcc_jit_function *cf_v = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "in_fn_coro_v", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(cf_v, "entry")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&st_coro, cf_v));
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
    g_mock_expr_rval = NULL;

    /* Also stmk_coroutine on int fn with fail_gro_rval == NULL (1340-1342), zero_prom == NULL (1405), and NULL descr fields */
    gcc_jit_function *cf_int_fg = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "in_fn_coro_fg", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(cf_int_fg, "entry")));
    g_mock_zero_rval_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&st_coro, cf_int_fg));
    g_mock_zero_rval_null = FALSE;
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;

    st_coro.variant.coroutine.descr = NULL;
    gcc_jit_function *cf_null_d = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "in_fn_coro_nd", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(cf_null_d, "entry")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&st_coro, cf_null_d));
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;

    /* 4. HALO, noop_coroutine_frame, and symmetric_transfer */
    a_boolean can_elide = FALSE;
    if (gcc_gen_be_try_coro_halo(&cdescr, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_coro_halo(NULL, &can_elide));
    cdescr.alloc_failure_gro_call = NULL;
    hdl_v.address_taken = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_coro_halo(&cdescr, &can_elide));
    hdl_v.address_taken = FALSE;
    prom_v.address_taken = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_coro_halo(&cdescr, &can_elide));
    prom_v.address_taken = FALSE;
    cdescr.handle = NULL;
    cdescr.promise = NULL;
    cdescr.alloc_failure_gro_call = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_coro_halo(&cdescr, &can_elide));

    if (gcc_gen_be_get_noop_coroutine_frame(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_get_noop_coroutine_frame(&out_fl) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK( sweep_oom_stmt(20, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_get_noop_coroutine_frame(&out_fl));
        return gcc_gen_be_get_noop_coroutine_frame(&out_fl); /* cached hit */
    }) );

    if (gcc_gen_be_emit_coro_symmetric_transfer(NULL, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_coro_symmetric_transfer(dummy_fn, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_coro_symmetric_transfer(dummy_fn, rblk, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_emit_coro_symmetric_transfer(dummy_fn, rblk, gcc_jit_context_null(ctx, vp_t)) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    p_gcc_jit_rvalue_set_bool_require_tail_call = [](gcc_jit_rvalue *, int) noexcept {};
    GCC_GEN_BE_CHECK( sweep_oom_stmt(30, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_coro_symmetric_transfer(
            dummy_fn, gcc_jit_function_new_block(dummy_fn, "st_vp"), gcc_jit_context_null(ctx, vp_t)));
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_coro_symmetric_transfer(
            dummy_fn, gcc_jit_function_new_block(dummy_fn, "st_ip"), gcc_jit_context_null(ctx, ip_t)));
        return gcc_gen_be_emit_coro_symmetric_transfer(
            dummy_fn, gcc_jit_function_new_block(dummy_fn, "st_val"), gcc_jit_context_zero(ctx, int_t));
    }) );
    p_gcc_jit_rvalue_set_bool_require_tail_call = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_coro_symmetric_transfer(
        dummy_fn, gcc_jit_function_new_block(dummy_fn, "st_notc"), gcc_jit_context_null(ctx, vp_t)));

    g_mock_coro_descr = NULL;
    il_header.function_def_table = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises remaining edge branches across SEH, guards, control flow, VLA, inline asm, and coroutines.
 * @details Covers standalone `__except` setjmp synthesis, nested `__finally`, `p_gcc_jit_lvalue_add_string_attribute`,
 * pointer loop conditions, `map_coro_scope_vars_to_frame` promise/handle skips, `stmk_coroutine_return` fallbacks,
 * `ramp` parameter forwarding (`p - 1 < ramp_pcount`), and NULL-descriptor ramp/dispatcher/layout paths.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_stmt_edge_branches(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
    gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *ip_t = gcc_jit_type_get_pointer(int_t);

    gcc_jit_function *fn_v = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "edge_void_fn", 0, NULL, 0);
    gcc_jit_param *p_i = gcc_jit_context_new_param(ctx, NULL, int_t, "pi");
    gcc_jit_function *fn_vp_pi = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, vp_t, "edge_vp_pi", 1, &p_i, 0);
    gcc_jit_param *p_vp0 = gcc_jit_context_new_param(ctx, NULL, vp_t, "pvp0");
    gcc_jit_function *fn_int_pvp = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "edge_int_pvp", 1, &p_vp0, 0);

    a_statement s_ret = {}, s_ret_e = {}, s_goto = {}, s_bad = {};
    an_expr_node e_node = {};
    a_label lbl = {}; lbl.source_corresp.name = "edge_lbl";
    s_ret.kind = stmk_return;
    s_ret_e.kind = stmk_return; s_ret_e.expr = &e_node;
    s_goto.kind = stmk_goto; s_goto.variant.label.ptr = &lbl;
    s_bad.kind = (a_statement_kind)251;

    /* 1. Guard attribute callback + SEH edge cases (standalone __except before setjmp cached, nested __finally, multi-entry seh cleanup stack) */
    p_gcc_jit_lvalue_add_string_attribute = [](gcc_jit_lvalue *, enum gcc_jit_variable_attribute, const char *) noexcept {};
    a_variable sv_attr = {}; sv_attr.storage_class = (a_storage_class)sc_static; sv_attr.source_corresp.name = "sv_attr";
    a_dynamic_init di_attr = {}; di_attr.kind = (a_dynamic_init_kind)dik_expression; di_attr.variable = &sv_attr;
    a_statement st_attr = {}; st_attr.kind = stmk_init; st_attr.variant.dynamic_init = &di_attr;
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "g_attr")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&st_attr, fn_v));
    p_gcc_jit_lvalue_add_string_attribute = NULL;

#if MICROSOFT_EXTENSIONS_ALLOWED
    a_statement seh_null = {}; seh_null.kind = stmk_microsoft_try;
    if (gcc_gen_be_lower_microsoft_try(&seh_null, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_microsoft_try(&seh_null, fn_v) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* Standalone __except right after cache_clear_all (synthesizes setjmp, guarded=NULL, cleanup=NULL, filter_res=NULL, outer_seh_lp=NULL) */
    a_microsoft_try_supplement ex_s1 = {}, ex_s2 = {}, fin_inner = {}, fin_outer = {}, fin_ret = {};
    ex_s1.except_expr = &e_node;
    a_statement st_ex1 = {}; st_ex1.kind = stmk_microsoft_try; st_ex1.variant.microsoft_try = &ex_s1;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(40, [&]() noexcept -> gcc_gen_be_error_t {
        g_mock_expr_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "ex1")));
        return gcc_gen_be_lower_microsoft_try(&st_ex1, fn_v);
    }) );
    /* __except with filter_res of int_type and cleanup_statement terminating (after_handler == NULL) */
    ex_s2.except_expr = &e_node; ex_s2.cleanup_statement = &s_ret;
    a_statement st_ex2 = {}; st_ex2.kind = stmk_microsoft_try; st_ex2.variant.microsoft_try = &ex_s2;
    g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "ex2")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_microsoft_try(&st_ex2, fn_v));
    g_mock_expr_rval = NULL;

    /* Nested __finally inside outer __finally (guarded=NULL, cleanup=NULL -> outer_seh_lp != NULL on inner!) + terminating guarded/cleanup */
    a_statement st_fin_in = {}, st_fin_out = {}, st_fin_ret = {};
    st_fin_in.kind = stmk_microsoft_try; st_fin_in.variant.microsoft_try = &fin_inner;
    fin_outer.guarded_statement = &st_fin_in;
    st_fin_out.kind = stmk_microsoft_try; st_fin_out.variant.microsoft_try = &fin_outer;
    fin_ret.guarded_statement = &s_ret; fin_ret.cleanup_statement = &s_ret;
    st_fin_ret.kind = stmk_microsoft_try; st_fin_ret.variant.microsoft_try = &fin_ret;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(60, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "fin_nest")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_microsoft_try(&st_fin_out, fn_v));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "fin_ret")));
        return gcc_gen_be_lower_microsoft_try(&st_fin_ret, fn_v);
    }) );

    /* 2-entry active_seh_finally_stack where entry 1 terminates block -> entry 2 sees cblock == NULL, AND stmk_return/goto see current_block == NULL! */
    gcc_gen_be_seh_finally_entry_t fe2 = {}, fe1 = {};
    fe2.abnormal_flag_lval = NULL; fe2.cleanup_statement = NULL; fe2.prev = NULL;
    fe1.abnormal_flag_lval = NULL; fe1.cleanup_statement = &s_ret; fe1.prev = &fe2;
    gcc_gen_be_singleton_state.active_seh_finally_stack = &fe1;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "seh_abn1")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "seh_abn2")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "seh_abn3")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_goto, fn_v));
    /* Also cleanup_statement failing -> restores active_seh_finally_stack */
    fe1.cleanup_statement = &s_bad;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "seh_abn_err")));
    if (gcc_gen_be_lower_statement(&s_ret, fn_v) != GCC_GEN_BE_ERROR_UNHANDLED_STMT) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_gen_be_singleton_state.active_seh_finally_stack = NULL;
#endif

    /* 2. stmt_has_label_or_case (NULL child, stmk_constexpr_if, stmk_if_not_consteval) & lower_statement_decl edge cases */
    a_statement u_cif = {}, u_ifnc = {};
    u_cif.kind = stmk_constexpr_if;
    u_ifnc.kind = stmk_if_not_consteval;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&u_cif, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&u_ifnc, fn_v));

    a_variable v_par = {}, v_tl = {}, v_uncached_coro = {};
    v_par.is_parameter = TRUE;
    v_tl.is_thread_local = TRUE;
    an_il_entity_list_entry le1 = {}, le2 = {}, le3 = {};
    le1.entity.kind = (an_il_entry_kind)iek_variable; le1.entity.ptr = reinterpret_cast<char *>(&v_par); le1.next = &le2;
    le2.entity.kind = (an_il_entry_kind)iek_variable; le2.entity.ptr = reinterpret_cast<char *>(&v_tl); le2.next = &le3;
    le3.entity.kind = (an_il_entry_kind)iek_variable; le3.entity.ptr = reinterpret_cast<char *>(&v_uncached_coro);
    a_statement s_decl2 = {}; s_decl2.kind = stmk_decl; s_decl2.variant.decl.entities = &le1;
    gcc_jit_lvalue *dummy_lv = gcc_jit_function_new_local(fn_v, NULL, int_t, "dlv");
    gcc_gen_be_singleton_state.active_coro_frame_lval = dummy_lv;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "decl2")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_decl2, fn_v));
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;

    /* 3. lower_block_stmt_inner edge cases */
    a_variable b_sv1 = {}, b_sv2 = {}, b_vla = {};
    b_sv1.storage_class = (a_storage_class)sc_static; b_sv1.source_corresp.name = "named_st"; b_sv1.next = &b_sv2;
    b_sv2.storage_class = (a_storage_class)sc_static; b_sv2.source_corresp.name = NULL; b_sv2.init_kind = (an_init_kind)initk_zero; b_sv2.next = &b_vla;
    b_vla.has_variably_modified_type = TRUE;
    a_routine dtor_r = {}; dtor_r.source_corresp.name = "dtor";
    a_dynamic_init d_novar = {}, d_nulllv = {};
    d_novar.destructor = &dtor_r; d_novar.variable = NULL; d_novar.next_in_destruction_list = &d_nulllv;
    d_nulllv.destructor = &dtor_r; d_nulllv.variable = &b_sv1;
    an_object_lifetime lt_empty = {}, lt_edge = {};
    lt_edge.destructions = &d_novar;
    a_scope bsc_edge = {}; bsc_edge.variables = &b_sv1; bsc_edge.lifetime = &lt_edge;
    a_block blk_e1 = {}, blk_e2 = {};
    blk_e1.assoc_scope = &bsc_edge;
    blk_e2.lifetime = &lt_empty;
    a_statement sb1 = {}, sb2 = {};
    sb1.kind = stmk_block; sb1.variant.block.extra_info = &blk_e1; sb1.variant.block.statements = &s_ret; /* terminates -> current_block == NULL at vla_saved_stack! */
    sb2.kind = stmk_block; sb2.variant.block.extra_info = &blk_e2;
    g_mock_var_decl_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "sb1")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&sb1, fn_v));
    g_mock_var_decl_null = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "sb2")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&sb2, fn_v));

    /* 4. Control-flow, return, goto, switch, loops, VLA, init, asm micro-branches */
    /* stmk_expr with clear_block, stmk_stmt_expr_result with res_val == NULL */
    a_statement s_ex = {}, s_ser = {};
    s_ex.kind = stmk_expr; s_ex.expr = &e_node;
    s_ser.kind = stmk_stmt_expr_result; s_ser.expr = &e_node;
    g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
    g_mock_expr_clear_block = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "ex_cb")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ex, fn_v));
    g_mock_expr_clear_block = FALSE;
    g_mock_expr_rval = NULL;
    current_stmt_expr_result_lval = dummy_lv;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "ser_null")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ser, fn_v));
    current_stmt_expr_result_lval = NULL;

    /* stmk_return: sret with rval == NULL; fn_vp_pi (ret pointer, param int); rv_t struct; expr_ut NULL / tk_struct / tk_class */
    gcc_jit_param *sret_p2 = gcc_jit_context_new_param(ctx, NULL, ip_t, "sp2");
    gcc_jit_function *fn_sret2 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "sret2", 1, &sret_p2, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, fn_sret2, sret_p2));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_sret2, "sr_null")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_sret2));

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_vp_pi, "rvp1")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_vp_pi));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_vp_pi, "rvp2")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret, fn_vp_pi));

    a_type st_edg = {}, cls_edg = {};
    st_edg.kind = tk_struct; cls_edg.kind = tk_class;
    /* rv_t is struct (g_mock_struct_jit_type), fn_ret_t is int */
    gcc_jit_lvalue *st_lv = gcc_jit_function_new_local(fn_int_pvp, NULL, g_mock_struct_jit_type, "stlv");
    g_mock_expr_rval = gcc_jit_lvalue_as_rvalue(st_lv);
    e_node.type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int_pvp, "rst1")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_int_pvp));
    g_mock_expr_rval = gcc_jit_context_zero(ctx, bool_t);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int_pvp, "rst_nullut")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_int_pvp));
    e_node.type = &st_edg;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int_pvp, "rst2")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_int_pvp));
    e_node.type = &cls_edg;
    g_mock_coerce_null = TRUE;
    g_mock_expr_rval = gcc_jit_context_null(ctx, vp_t);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int_pvp, "rst3")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_int_pvp)); /* hits if (coerced_ret) False */
    g_mock_coerce_null = FALSE;
    g_mock_expr_rval = NULL;
    e_node.type = NULL;

    /* stmk_goto with active scope having non-NULL lifetime (goto_lt != NULL) */
    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(&lt_empty));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "gt_lt")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_goto, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));

    /* stmk_if: asymmetric branch termination (then returns / else falls through, and vice versa) */
    a_statement empty_s = {}; empty_s.kind = stmk_empty;
    a_statement sif_a = {}, sif_b = {};
    sif_a.kind = stmk_if; sif_a.variant.if_stmt.then_statement = &s_ret; sif_a.variant.if_stmt.else_statement = &empty_s;
    sif_b.kind = stmk_if; sif_b.variant.if_stmt.then_statement = &empty_s; sif_b.variant.if_stmt.else_statement = &s_ret;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "ifa")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&sif_a, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&sif_b, fn_v));

    /* stmk_switch: NULL case_value/range_end, default-only OOM sweep (cases == NULL), and terminating body without default */
    a_switch_case_entry sc_nulls = {}, sc_def_only = {};
    a_switch_stmt_descr sw_nulls = {}, sw_def_only = {}, sw_term = {};
    sw_nulls.cases = &sc_nulls;
    sw_def_only.cases = &sc_def_only; sw_def_only.default_case = &sc_def_only;
    a_statement s_sw_n = {}, s_sw_do = {}, s_sw_t = {};
    s_sw_n.kind = stmk_switch; s_sw_n.variant.switch_stmt.extra_info = &sw_nulls; s_sw_n.variant.switch_stmt.body_statement = &s_ret;
    s_sw_do.kind = stmk_switch; s_sw_do.variant.switch_stmt.extra_info = &sw_def_only;
    s_sw_t.kind = stmk_switch; s_sw_t.variant.switch_stmt.extra_info = &sw_term; s_sw_t.variant.switch_stmt.body_statement = &s_ret;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(25, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "sw_do")));
        return gcc_gen_be_lower_statement(&s_sw_do, fn_v);
    }) );
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "sw_nt")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_sw_n, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "sw_t")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_sw_t, fn_v));

    /* Loops: bool & pointer conditions, clear_block in cond/increment, terminating init/body, NULL bodies */
    a_for_loop fl_empty = {}, fl_term_init = {}, fl_cb_inc = {};
    fl_term_init.initialization = &s_ret;
    fl_cb_inc.increment = &e_node;
    a_statement s_dow = {}, s_wh = {}, s_for1 = {}, s_for2 = {}, s_for3 = {}, s_rfor = {}, s_fe = {};
    s_dow.kind = stmk_end_test_while; s_dow.expr = &e_node; s_dow.variant.loop_statement = &empty_s;
    s_wh.kind = stmk_while; s_wh.expr = &e_node; s_wh.variant.loop_statement = &s_ret;
    s_for1.kind = stmk_for; s_for1.expr = &e_node; s_for1.variant.for_loop.extra_info = NULL; s_for1.variant.for_loop.statement = &s_ret;
    s_for2.kind = stmk_for; s_for2.expr = &e_node; s_for2.variant.for_loop.extra_info = &fl_empty;
    s_for3.kind = stmk_for; s_for3.expr = &e_node; s_for3.variant.for_loop.extra_info = &fl_term_init;
    s_rfor.kind = stmk_range_based_for;
    s_fe.kind = stmk_for_each;

    g_mock_expr_rval = gcc_jit_context_zero(ctx, bool_t);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "lp_b")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_dow, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_for2, fn_v));

    g_mock_expr_rval = gcc_jit_context_null(ctx, vp_t);
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_wh, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_for1, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_for3, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_rfor, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_fe, fn_v));

    g_mock_expr_clear_block = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_dow, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_wh, fn_v));
    s_for2.variant.for_loop.extra_info = &fl_cb_inc;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_for2, fn_v));
    g_mock_expr_clear_block = FALSE;
    s_for2.expr = NULL;
    g_mock_expr_clear_block = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_for2, fn_v)); /* hits step_block == NULL in increment! */
    g_mock_expr_clear_block = FALSE;
    g_mock_expr_rval = NULL;

    /* VLA edge cases */
    gcc_jit_lvalue *vla_out = NULL;
    gcc_jit_block *vblk = gcc_jit_function_new_block(fn_v, "vblk");
    if (gcc_gen_be_emit_stack_save(fn_v, NULL, &vla_out) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_stack_save(fn_v, vblk, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_stack_restore(fn_v, NULL, dummy_lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_stack_restore(fn_v, vblk, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_vla_decl(NULL, &b_vla, &vla_out) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_vla_decl(fn_v, NULL, &vla_out) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_vla_decl(fn_v, &b_vla, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    a_vla_dimension vd_novar = {};
    a_statement svs1 = {}, svs2 = {}, svs3 = {}, svd1 = {}, svd2 = {};
    svs1.kind = stmk_set_vla_size; svs1.variant.vla_dimension = NULL;
    svs2.kind = stmk_set_vla_size; svs2.variant.vla_dimension = &vd_novar;
    vd_novar.dimension_variable = &b_vla;
    svs3.kind = stmk_set_vla_size; svs3.variant.vla_dimension = &vd_novar;
    svd1.kind = stmk_vla_decl; svd1.variant.vla.variant.variable = NULL;
    svd2.kind = stmk_vla_decl; svd2.variant.vla.variant.variable = &b_vla; svd2.variant.vla.is_typedef_decl = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(vblk));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&svs1, fn_v));
    vd_novar.dimension_variable = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&svs2, fn_v));
    vd_novar.dimension_variable = &b_vla;
    g_mock_expr_rval = NULL; /* ignore == NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&svs3, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&svd1, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&svd2, fn_v));

    /* lower_vla_decl: named var, innermost_function_scope == NULL, missing dim, dim_lval == NULL, 0-len array, typeref -> NULL element */
    a_type vt_tref_null = {}, vt_arr0 = {}, vt_arr_vla = {};
    vt_tref_null.kind = tk_typeref; vt_tref_null.variant.typeref.type = NULL;
    vt_arr0.kind = tk_array; vt_arr0.variant.array.variant.number_of_elements = 0; vt_arr0.variant.array.element_type = &vt_tref_null;
    vt_arr_vla.kind = tk_array; vt_arr_vla.variant.array.has_assoc_vla_dimension = TRUE; vt_arr_vla.variant.array.element_type = &vt_arr0;
    a_variable vla_named = {}; vla_named.source_corresp.name = "named_vla"; vla_named.type = &vt_arr_vla;
    innermost_function_scope = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vla_decl(fn_v, &vla_named, &vla_out));
    a_scope empty_vla_sc = {};
    innermost_function_scope = &empty_vla_sc;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vla_decl(fn_v, &vla_named, &vla_out)); /* dim == NULL */
    a_vla_dimension vd_unlinked = {}; vd_unlinked.type = &vt_arr_vla;
    empty_vla_sc.vla_dimensions = &vd_unlinked;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vla_decl(fn_v, &vla_named, &vla_out)); /* !dim->dimension_variable && !original_dimension */
    vd_unlinked.dimension_variable = &b_vla;
    g_mock_var_decl_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vla_decl(fn_v, &vla_named, &vla_out)); /* dim_lval == NULL */
    g_mock_var_decl_null = FALSE;
    innermost_function_scope = NULL;

    /* stmk_init edge cases: dip == NULL, var == NULL, coro promise skip, unnamed initk_static, var_lvalue == NULL, null constant/rvals,asm edge cases */
    a_variable prom_skip_v = {}, ustat_v = {}, nocon_v = {}, other_ik_v = {};
    ustat_v.storage_class = (a_storage_class)sc_static; ustat_v.source_corresp.name = NULL; ustat_v.init_kind = (an_init_kind)initk_static;
    nocon_v.init_kind = (an_init_kind)initk_static; nocon_v.initializer.constant = NULL;
    other_ik_v.init_kind = (an_init_kind)initk_none;
    a_coroutine_descr cd_init = {}; cd_init.promise = &prom_skip_v;
    a_dynamic_init di_novar = {}, di_prom = {}, di_ustat = {}, di_nocon = {}, di_other = {};
    di_prom.variable = &prom_skip_v;
    di_ustat.kind = (a_dynamic_init_kind)dik_none; di_ustat.variable = &ustat_v;
    di_nocon.kind = (a_dynamic_init_kind)dik_none; di_nocon.variable = &nocon_v;
    di_other.kind = (a_dynamic_init_kind)dik_none; di_other.variable = &other_ik_v;

    a_statement si0 = {}, si1 = {}, si2 = {}, si3 = {}, si4 = {}, si5 = {};
    si0.kind = stmk_init; si0.variant.dynamic_init = NULL;
    si1.kind = stmk_init; si1.variant.dynamic_init = &di_novar;
    si2.kind = stmk_init; si2.variant.dynamic_init = &di_prom;
    si3.kind = stmk_init; si3.variant.dynamic_init = &di_ustat;
    si4.kind = stmk_init; si4.variant.dynamic_init = &di_nocon;
    si5.kind = stmk_init; si5.variant.dynamic_init = &di_other;

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&si0, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&si1, fn_v));
    gcc_gen_be_singleton_state.active_coro_frame_lval = dummy_lv;
    gcc_gen_be_singleton_state.active_coro_descr = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&si3, fn_v)); /* active_coro_descr == NULL */
    gcc_gen_be_singleton_state.active_coro_descr = &cd_init;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&si2, fn_v)); /* promise skip break! */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&si3, fn_v)); /* non-promise + unnamed static dik_none */
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
    gcc_gen_be_singleton_state.active_coro_descr = NULL;

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&si4, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&si5, fn_v));

    /* Pre-cached static var in CACHE_VAR + const_rval_null + zero_rval_null + var_lvalue == NULL */
    a_constant dummy_c = {};
    ustat_v.initializer.constant = &dummy_c;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &ustat_v, dummy_lv));
    g_mock_const_rval_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&si3, fn_v));
    g_mock_const_rval_null = FALSE;

    a_variable z_v = {}; z_v.init_kind = (an_init_kind)initk_zero;
    a_dynamic_init di_z = {}; di_z.kind = (a_dynamic_init_kind)dik_none; di_z.variable = &z_v;
    a_statement si_z = {}; si_z.kind = stmk_init; si_z.variant.dynamic_init = &di_z;
    g_mock_zero_rval_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&si_z, fn_v));
    g_mock_zero_rval_null = FALSE;

    a_variable null_lv_v = {};
    a_dynamic_init di_nlv = {}; di_nlv.variable = &null_lv_v;
    a_statement si_nlv = {}; si_nlv.kind = stmk_init; si_nlv.variant.dynamic_init = &di_nlv;
    g_mock_var_decl_null = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&si_nlv, fn_v));
    g_mock_var_decl_null = FALSE;

    /* stmk_asm (NULL entry, NULL/non-string asm_string, NULL lval/rval) & stmk_asm_func_body (NULL str) */
    a_constant non_str_c = {}; non_str_c.kind = (a_constant_repr_kind)ck_integer;
    an_asm_operand op_out_nl = {}, op_in_nr = {};
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
    op_out_nl.is_output_operand = TRUE; op_out_nl.constraints_string = "=r"; op_out_nl.expression = &e_node; op_out_nl.next = &op_in_nr;
    op_in_nr.is_output_operand = FALSE; op_in_nr.constraints_string = "r"; op_in_nr.expression = &e_node;
#else
    an_asm_operand_constraint aoc_nl = {};
    aoc_nl.kind = aoc_reg_integer;
    op_out_nl.modifiers = aom_output; op_out_nl.constraints = &aoc_nl; op_out_nl.expression = &e_node; op_out_nl.next = &op_in_nr;
    op_in_nr.modifiers = aom_input; op_in_nr.constraints = &aoc_nl; op_in_nr.expression = &e_node;
#endif
    an_asm_entry ae1 = {}, ae2 = {};
    ae1.asm_string = NULL; ae1.operands = &op_out_nl;
    ae2.asm_string = &non_str_c;
    a_statement sa0 = {}, sa1 = {}, sa2 = {}, sab0 = {};
    sa0.kind = stmk_asm; sa0.variant.asm_entry = NULL;
    sa1.kind = stmk_asm; sa1.variant.asm_entry = &ae1;
    sa2.kind = stmk_asm; sa2.variant.asm_entry = &ae2;
    sab0.kind = stmk_asm_func_body; sab0.variant.asm_func_body = NULL;
    g_mock_expr_lval = NULL; g_mock_expr_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&sa0, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&sa1, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&sa2, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&sab0, fn_v));

    /* 5. Coroutine edge branches: map_coro_scope_vars_to_frame skips, stmk_label, stmk_coroutine_return, temp scanners, layout, ramp, dispatchers */
    a_type zero_sz_tp = {}; zero_sz_tp.kind = tk_integer; zero_sz_tp.size = 0;
    an_expr_node te_notp = {}, te_nodip = {}, te_nondik = {}, te_yld = {};
    a_dynamic_init di_none_k = {}; di_none_k.kind = (a_dynamic_init_kind)dik_none;
    te_notp.kind = enk_temp_init; te_notp.type = NULL; te_notp.next = &te_nodip;
    te_nodip.kind = enk_temp_init; te_nodip.type = &zero_sz_tp; te_nodip.variant.init.dynamic_init = NULL; te_nodip.next = &te_nondik;
    te_nondik.kind = enk_temp_init; te_nondik.type = &zero_sz_tp; te_nondik.variant.init.dynamic_init = &di_none_k; te_nondik.next = &te_yld;
    te_yld.kind = enk_yield;

    a_statement cs1 = {}, cs2 = {}, cs3 = {}, cs4 = {}, cs5 = {}, cs6 = {}, cs7 = {}, cs8 = {};
    cs1.kind = stmk_if_consteval; cs1.expr = &te_notp; cs1.next = &cs2;
    cs2.kind = stmk_if_not_consteval; cs2.next = &cs3;
    cs3.kind = stmk_constexpr_if; cs3.variant.constexpr_if = NULL; cs3.next = &cs4;
    cs4.kind = stmk_end_test_while; cs4.next = &cs5;
    cs5.kind = stmk_for; cs5.variant.for_loop.extra_info = NULL; cs5.next = &cs6;
    cs6.kind = stmk_try_block; cs6.variant.try_block = NULL; cs6.next = &cs7;
    cs7.kind = stmk_init; cs7.variant.dynamic_init = NULL; cs7.next = &cs8;
    cs8.kind = stmk_init; cs8.variant.dynamic_init = &di_none_k;

    a_variable v_pr = {}, v_ia = {}, v_hd = {}, v_notp = {}, v_pflag = {};
    v_pflag.is_parameter = TRUE; v_pflag.next = &v_notp;
    a_dynamic_init d1 = {}, d2 = {}, d3 = {}, d4 = {}, d5 = {};
    d1.variable = NULL; d1.next_in_destruction_list = &d2;
    d2.variable = &v_pr; d2.next_in_destruction_list = &d3;
    d3.variable = &v_ia; d3.next_in_destruction_list = &d4;
    d4.variable = &v_hd; d4.next_in_destruction_list = &d5;
    d5.variable = &v_notp;
    an_object_lifetime lt_coro_skips = {}, lt_coro_empty = {};
    lt_coro_skips.destructions = &d1;

    a_scope sc_c1 = {}, sc_c2 = {}, sc_top = {};
    sc_c1.lifetime = NULL; sc_c1.next = &sc_c2;
    sc_c2.lifetime = &lt_coro_empty;
    sc_top.kind = sck_function;
    sc_top.assoc_block = &cs1;
    sc_top.variant.routine.parameters = &v_notp;
    sc_top.nonstatic_variables = &v_pr;
    v_pr.next = &v_ia; v_ia.next = &v_hd; v_hd.next = &v_notp;
    sc_top.variables = &v_pr;
    sc_top.lifetime = &lt_coro_skips;
    sc_top.scopes = &sc_c1;

    a_coroutine_descr cd_edge = {};
    cd_edge.promise = &v_pr;
    cd_edge.init_await_resume = &v_ia;
    cd_edge.handle = &v_hd;
    cd_edge.parameter_copies = &v_notp;
    g_mock_coro_descr = &cd_edge;

    a_routine r_edge = {};
    size_t out_sz = 0;
    gcc_jit_struct *out_st = NULL;
    /* compute_coro_frame_layout: only out_frame_struct, only out_frame_size, ctx == NULL, g_mock_lower_type_null, cached partial out-ptrs */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_compute_coro_frame_layout(&r_edge, &sc_top, &out_st, &out_sz) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_ramp(&r_edge, fn_v) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_jit_function *rf = NULL, *df = NULL;
    if (gcc_gen_be_lower_coroutine_dispatchers(&r_edge, &rf, &df) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_ramp(&r_edge, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_dispatchers(&r_edge, NULL, &df) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_dispatchers(&r_edge, &rf, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    v_pr.type = &zero_sz_tp;
    g_mock_lower_type_null = TRUE;
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&r_edge, &sc_top, &out_st, NULL)); /* !promise_type + append jit_type == NULL + out_frame_size == NULL */
    g_mock_lower_type_null = FALSE;
    v_pr.type = NULL;

    /* Cached layout with out_frame_struct == NULL, out_frame_size == NULL, and void_t (sz == 0) */
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&r_edge, &sc_top, NULL, &out_sz));
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&r_edge, &sc_top, &out_st, NULL));
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, &r_edge, void_t));
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&r_edge, NULL, NULL, &out_sz));
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(&r_edge, &sc_top, NULL, &out_sz)); /* uncached with out_frame_struct == NULL */

    /* Also test variables = &v_pflag -> &v_pr -> &v_ia -> &v_hd -> &v_notp in collect/accumulate/map_coro_scope_vars_to_frame */
    v_pflag.next = &v_pr;
    sc_top.variables = &v_pflag;
    a_function_def_descr fdd2[2] = {};
    fdd2[1].scope = &sc_top;
    il_header.function_def_table = fdd2;
    r_edge.function_def_number = 1;
    a_statement try_susp_s = {}; try_susp_s.kind = stmk_try_block;
    sc_top.assoc_block = &try_susp_s;
    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_gen_be_singleton_state.active_coro_current_scope = &sc_top;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_dispatchers(&r_edge, &rf, &df));
    gcc_gen_be_singleton_state.active_coro_current_scope = NULL;

    /* NULL descr ramp & dispatchers (prom_f_type == void_ptr_type, ret_obj == NULL -> lines 3268-3270, active_susp_scope == NULL -> 3521 False!) */
    g_mock_coro_descr = NULL;
    il_header.function_def_table = NULL;
    r_edge.function_def_number = 0;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(120, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_jit_function *r_int_nd = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "ramp_nd_int", 0, NULL, 0);
        return gcc_gen_be_lower_coroutine_ramp(&r_edge, r_int_nd);
    }) );
    /* Dispatcher with NULL def_scope & NULL active_coro_current_scope + suspend points -> active_susp_scope == NULL! */
    a_scope tmp_try_sc = {}; tmp_try_sc.assoc_block = &try_susp_s;
    fdd2[1].scope = &tmp_try_sc;
    il_header.function_def_table = fdd2;
    r_edge.function_def_number = 1;
    /* Also test ramp with custom new_routine (pc=1 and pc=2 on 1-param ramp -> lines 3050-3051!), zero_prom==NULL, gro matching ret_type, void gro */
    a_routine op_new_r = {}; op_new_r.source_corresp.name = "op_new";
    an_expr_node gro2 = {}, fg2 = {}, is2 = {}, fs2 = {}, ue2 = {};
    a_variable pr2 = {}, hd2 = {};
    pr2.type = &zero_sz_tp; pr2.init_kind = (an_init_kind)initk_dynamic; pr2.initializer.dynamic = NULL;
    cd_edge = {};
    cd_edge.new_routine = &op_new_r;
    cd_edge.promise = &pr2;
    cd_edge.handle = &hd2; /* hd2.type == NULL! */
    cd_edge.parameter_copies = &pr2;
    cd_edge.alloc_failure_gro_call = &fg2;
    cd_edge.get_return_object_call = &gro2;
    cd_edge.initial_suspend_call = &is2;
    cd_edge.body_generated = TRUE;
    cd_edge.unhandled_exception_call = &ue2;
    cd_edge.final_suspend_call = &fs2;
    g_mock_coro_descr = &cd_edge;

    /* pc=2 on 1-param ramp -> p=1 < ramp_pcount(1) hits lines 3050-3051! Plus gro_rval matching int_t, zero_prom == NULL */
    g_mock_fn_param_count = 2;
    g_mock_zero_rval_null = TRUE;
    g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
    gcc_jit_param *rp_1 = gcc_jit_context_new_param(ctx, NULL, vp_t, "rp1");
    gcc_jit_function *ramp_p1 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "ramp_p1", 1, &rp_1, 0);
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&r_edge, ramp_p1));
    g_mock_zero_rval_null = FALSE;

    /* pc=1 on void ramp with g_mock_expr_rval = void call rvalue (hits void ue_rval / fs_rval & void ramp gro_rval!) */
    g_mock_fn_param_count = 1;
    gcc_jit_function *void_callee = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_t, "__void_callee", 0, NULL, 0);
    g_mock_expr_rval = gcc_jit_context_new_call(ctx, NULL, void_callee, 0, NULL);
    gcc_jit_function *ramp_v2 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "ramp_v2", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&r_edge, ramp_v2));

    /* g_mock_expr_clear_block = TRUE during ramp & dispatcher (clears entry / res_state0 / coro_eh_lp!) */
    cd_edge.alloc_failure_gro_call = NULL;
    cd_edge.body_generated = FALSE;
    g_mock_expr_rval = NULL;
    g_mock_expr_clear_block = TRUE;
    gcc_jit_function *ramp_cb = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "ramp_cb", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&r_edge, ramp_cb));
    a_label fsl2 = {}; fsl2.source_corresp.name = "fsl2";
    a_statement s_fsl2 = {}; s_fsl2.kind = stmk_label; s_fsl2.variant.label.ptr = &fsl2;
    tmp_try_sc.assoc_block = &s_fsl2;
    cd_edge.final_suspend_label = &fsl2;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_dispatchers(&r_edge, &rf, &df));
    g_mock_expr_clear_block = FALSE;

    /* stmk_label & stmk_coroutine_return edge cases (lines 1122-1124, 1476-1513) */
    a_statement s_cr_noexpr = {}, s_cr_expr = {}, s_lbl_other = {};
    a_label other_lbl = {}; other_lbl.source_corresp.name = "other_l";
    s_cr_noexpr.kind = stmk_coroutine_return;
    s_cr_expr.kind = stmk_coroutine_return; s_cr_expr.expr = &e_node;
    s_lbl_other.kind = stmk_label; s_lbl_other.variant.label.ptr = &other_lbl;

    /* stmk_label with active_coro_descr != NULL: non-matching label, NULL frame_lval, NULL frame_type, int_t frame_type */
    gcc_gen_be_singleton_state.active_coro_descr = &cd_edge;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "cl1")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_lbl_other, fn_v));
    s_lbl_other.variant.label.ptr = &fsl2;
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_lbl_other, fn_v));
    gcc_gen_be_singleton_state.active_coro_frame_lval = dummy_lv;
    gcc_gen_be_singleton_state.active_coro_frame_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_lbl_other, fn_v));
    gcc_gen_be_singleton_state.active_coro_frame_type = int_t;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_lbl_other, fn_v));

    /* stmk_coroutine_return: expr == NULL, neither final_suspend_label nor final_suspend_call (lines 1511-1513!), clear_block */
    cd_edge.final_suspend_label = NULL;
    cd_edge.final_suspend_call = NULL;
    gcc_jit_type *coro_ft = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_frame(&r_edge, &coro_ft));
    gcc_jit_lvalue *coro_flv = gcc_jit_function_new_local(fn_v, NULL, gcc_jit_type_get_pointer(coro_ft), "cflv");
    gcc_gen_be_singleton_state.active_coro_frame_lval = coro_flv;
    gcc_gen_be_singleton_state.active_coro_frame_type = int_t; /* non-struct active_coro_frame_type! */
    gcc_gen_be_singleton_state.active_coro_state_field = gcc_jit_struct_get_field(gcc_jit_type_is_struct(coro_ft), 3);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "cr1")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_cr_noexpr, fn_v));

    /* Clear block inside stmt->expr and inside final_suspend_call WITH non-NULL rval so rval && cblock evaluates cblock == NULL! */
    g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
    g_mock_expr_clear_block = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "cr2")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_cr_expr, fn_v));
    cd_edge.final_suspend_call = &fs2;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "cr3")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_cr_noexpr, fn_v));
    /* Also with final_suspend_label != NULL and cblock cleared by stmt->expr! */
    cd_edge.final_suspend_label = &fsl2;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "cr4")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_cr_expr, fn_v));
    g_mock_expr_clear_block = FALSE;
    g_mock_expr_rval = NULL;

    /* stmk_coroutine_return with NULL frame_lval, NULL state_field, NULL frame_type, NULL descr */
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
    gcc_gen_be_singleton_state.active_coro_descr = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "cr5")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_cr_noexpr, fn_v));
    gcc_gen_be_singleton_state.active_coro_frame_lval = coro_flv;
    gcc_gen_be_singleton_state.active_coro_state_field = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "cr6")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_cr_noexpr, fn_v));
    gcc_gen_be_singleton_state.active_coro_state_field = gcc_jit_struct_get_field(gcc_jit_type_is_struct(coro_ft), 3);
    gcc_gen_be_singleton_state.active_coro_frame_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "cr7")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_cr_noexpr, fn_v));

    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
    gcc_gen_be_singleton_state.active_coro_frame_type = NULL;
    gcc_gen_be_singleton_state.active_coro_state_field = NULL;
    gcc_gen_be_singleton_state.active_coro_descr = NULL;

    /* Final micro-branches:
     * - seh cleanup with abnormal_flag_lval != NULL & cleanup_statement == NULL (94:23 False)
     * - get_active_coroutine_frame OOM sweep (340)
     * - stmk_decl with active_coro_frame_lval == NULL (811:21 False)
     * - stmk_block with bscope->pragmas ("#pragma omp barrier") under OOM sweep (1154, 1052)
     * - stmk_label with ptr == NULL (1121:39 False)
     * - stmk_stmt_expr_result with res_val != NULL & cblock == NULL (1187:35 False)
     * - stmk_return with scalar rv_t/fn_ret_t & expr_ut == tk_union (1564:111 True)
     * - stmk_goto with label.lifetime != NULL (1627:27 False)
     * - stmk_while with expr == NULL (1844:23 False)
     * - stmk_for with loop_info == NULL & non-terminating body (1915:27 False)
     * - stmk_init with unnamed static dik_expression, thread_local dik_none, unnamed static initk_zero dik_none (1990-1999)
     * - lower_vla_decl with curr_type->alignment == 0 (2384:45 False)
     */
    gcc_gen_be_seh_finally_entry_t fe_flag_only = {};
    fe_flag_only.abnormal_flag_lval = dummy_lv;
    fe_flag_only.cleanup_statement = NULL;
    gcc_gen_be_singleton_state.active_seh_finally_stack = &fe_flag_only;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "seh_fo")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret, fn_v));
    gcc_gen_be_singleton_state.active_seh_finally_stack = NULL;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(10, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_get_active_coroutine_frame(&coro_ft, &coro_flv);
    }) );

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "decl_nocoro")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_decl2, fn_v));

    a_pragma scope_prag = {};
    scope_prag.pragma_text = "#pragma omp barrier";
    a_scope omp_bscope = {};
    omp_bscope.pragmas = &scope_prag;
    a_block omp_binfo = {};
    omp_binfo.assoc_scope = &omp_bscope;
    a_statement omp_bstmt = {};
    omp_bstmt.kind = stmk_block;
    omp_bstmt.variant.block.extra_info = &omp_binfo;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(40, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_gen_be_singleton_state.omp_pragma_count = 0;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "omp_bs")));
        return gcc_gen_be_lower_statement(&omp_bstmt, fn_v);
    }) );
    gcc_gen_be_singleton_state.omp_pragma_count = 0;

    a_statement s_lbl_null = {}; s_lbl_null.kind = stmk_label; s_lbl_null.variant.label.ptr = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "lbl_null")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_lbl_null, fn_v));

    current_stmt_expr_result_lval = dummy_lv;
    g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
    g_mock_expr_clear_block = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "ser_cb")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ser, fn_v));
    g_mock_expr_clear_block = FALSE;
    g_mock_expr_rval = NULL;
    current_stmt_expr_result_lval = NULL;

    a_type union_edg = {}; union_edg.kind = tk_union;
    e_node.type = &union_edg;
    g_mock_expr_rval = gcc_jit_context_zero(ctx, bool_t);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_int_pvp, "ret_u")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ret_e, fn_int_pvp));
    g_mock_expr_rval = NULL;
    e_node.type = NULL;

    s_goto.variant.label.lifetime = &lt_empty;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "gt_lt2")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_goto, fn_v));
    s_goto.variant.label.lifetime = NULL;

    a_statement s_wh_noexpr = {}, s_for_noinfo = {};
    s_wh_noexpr.kind = stmk_while; s_wh_noexpr.variant.loop_statement = &empty_s;
    s_for_noinfo.kind = stmk_for; s_for_noinfo.variant.for_loop.statement = &empty_s;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "lp_ne")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_wh_noexpr, fn_v));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_for_noinfo, fn_v));

    a_variable v_us_expr = {}, v_tl_none = {}, v_us_zero = {};
    v_us_expr.storage_class = (a_storage_class)sc_static; v_us_expr.source_corresp.name = NULL; v_us_expr.init_kind = (an_init_kind)initk_static;
    v_tl_none.is_thread_local = TRUE; v_tl_none.source_corresp.name = "tl_none";
    v_us_zero.storage_class = (a_storage_class)sc_static; v_us_zero.source_corresp.name = NULL; v_us_zero.init_kind = (an_init_kind)initk_zero;
    a_dynamic_init di_use = {}, di_tln = {}, di_usz = {};
    di_use.kind = (a_dynamic_init_kind)dik_expression; di_use.variable = &v_us_expr;
    di_tln.kind = (a_dynamic_init_kind)dik_none; di_tln.variable = &v_tl_none;
    di_usz.kind = (a_dynamic_init_kind)dik_none; di_usz.variable = &v_us_zero;
    a_statement si_use = {}, si_tln = {}, si_usz = {};
    si_use.kind = stmk_init; si_use.variant.dynamic_init = &di_use; si_use.next = &si_tln;
    si_tln.kind = stmk_init; si_tln.variant.dynamic_init = &di_tln; si_tln.next = &si_usz;
    si_usz.kind = stmk_init; si_usz.variant.dynamic_init = &di_usz;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&si_use, fn_v));

    a_type vla_align0_el = {}, vla_align0_arr = {};
    vla_align0_el.kind = tk_integer; vla_align0_el.size = 4; vla_align0_el.alignment = 0;
    vla_align0_arr.kind = tk_array; vla_align0_arr.variant.array.variant.number_of_elements = 2; vla_align0_arr.variant.array.element_type = &vla_align0_el;
    a_variable vla_a0_var = {}; vla_a0_var.type = &vla_align0_arr;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "vla_a0")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_vla_decl(fn_v, &vla_a0_var, &vla_out));

    /* Coroutine final micro-branches:
     * 1) Empty non-NULL descr (cd_empty) in stmk_coroutine, ramp, and dispatchers + fno!=0 with table==NULL
     * 2) stmk_coroutine with null pcopy->type, null ptype, pc=1 new_routine, null promise->type, dynamic promise with null initializer & null destructor & null prom_dtor, clear_block with non-null rval, OOM on fail_gro/zero_prom
     * 3) Ramp & dispatchers with non-NULL rval + clear_block, void ue_rval with final_suspend_label, OOM on body_err without coro_eh_lp, active_susp_scope == NULL via g_mock_register_suspend_in_expr, d->variable == NULL in emit_coro_scope_dtors
     */
    a_coroutine_descr cd_empty = {};
    a_statement st_c_empty = {}; st_c_empty.kind = stmk_coroutine; st_c_empty.variant.coroutine.descr = &cd_empty;
    gcc_jit_function *cf_emp = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "cf_emp", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(cf_emp, "entry")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&st_c_empty, cf_emp));
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;

    g_mock_coro_descr = &cd_empty;
    il_header.function_def_table = NULL;
    r_edge.function_def_number = 1; /* fno != 0 && table == NULL! */
    gcc_jit_function *ramp_emp = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "ramp_emp", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&r_edge, ramp_emp));

    /* NULL descr with sc_top having nonstatic/variables/destructions so map_coro_scope_vars_to_frame sees active_coro_descr == NULL! */
    g_mock_coro_descr = NULL;
    d1.destructor = &dtor_r; /* d1.variable == NULL && d1.destructor != NULL -> 284:34 False in emit_coro_scope_dtors! */
    fdd2[1].scope = &sc_top;
    il_header.function_def_table = fdd2;
    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_gen_be_singleton_state.active_coro_current_scope = &sc_top;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_dispatchers(&r_edge, &rf, &df));
    gcc_gen_be_singleton_state.active_coro_current_scope = NULL;

    /* OOM sweep on lower_coroutine_dispatchers when unhandled_exception_call == NULL (hits 3411 GCC_GEN_BE_CHECK(body_err)) + assoc_block == NULL (3395:22 False) */
    g_mock_coro_descr = &cd_empty;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(120, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_coroutine_dispatchers(&r_edge, &rf, &df);
    }) );
    sc_top.assoc_block = NULL;
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_dispatchers(&r_edge, &rf, &df));

    /* active_susp_scope == NULL (def_scope == NULL & active_coro_current_scope == NULL) via g_mock_register_suspend_in_expr */
    il_header.function_def_table = NULL;
    r_edge.function_def_number = 0;
    cd_empty.initial_suspend_call = &is2;
    g_mock_register_suspend_in_expr = TRUE;
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_dispatchers(&r_edge, &rf, &df));
    g_mock_register_suspend_in_expr = FALSE;
    cd_empty.initial_suspend_call = NULL;

    /* stmk_coroutine edge matrix */
    a_variable pc_null_tp = {}, pc_valid_tp = {}, pr_edge = {}, hd_null_tp = {};
    pc_null_tp.type = NULL; pc_null_tp.next = &pc_valid_tp;
    pc_valid_tp.type = &zero_sz_tp;
    pr_edge.type = NULL; pr_edge.init_kind = (an_init_kind)initk_dynamic; pr_edge.initializer.dynamic = NULL;
    a_dynamic_init pr_di_nodtor = {}; pr_di_nodtor.destructor = NULL;
    a_coroutine_descr cd_sc = {};
    cd_sc.promise = &pr_edge;
    cd_sc.parameter_copies = &pc_null_tp;
    cd_sc.new_routine = &op_new_r;
    cd_sc.alloc_failure_gro_call = &fg2;
    cd_sc.handle = &hd_null_tp;
    cd_sc.initial_suspend_call = &is2;
    a_statement st_c_edge = {}; st_c_edge.kind = stmk_coroutine; st_c_edge.variant.coroutine.descr = &cd_sc;

    /* 1) void fn with fail_gro_rval == NULL (1336:31 False), g_mock_fn_param_count = 1 (1293:34 False), g_mock_lower_type_null = TRUE (ptype == NULL & cached_fld == NULL!) */
    g_mock_fn_param_count = 1;
    g_mock_lower_type_null = TRUE;
    g_mock_expr_rval = NULL;
    gcc_jit_function *cf_e1 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "cf_e1", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(cf_e1, "entry")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&st_c_edge, cf_e1));
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
    g_mock_lower_type_null = FALSE;

    /* 2) pr_edge with pr_di_nodtor (destructor == NULL -> 1429:62 False), then with destructor & g_mock_lower_fn_null (prom_dtor == NULL -> 1432:27 False) + non-NULL init_susp_rval & clear_block (1455:45 False) */
    pr_edge.initializer.dynamic = &pr_di_nodtor;
    g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
    g_mock_expr_clear_block = TRUE;
    gcc_jit_function *cf_e2 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "cf_e2", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(cf_e2, "entry")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&st_c_edge, cf_e2));
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
    g_mock_expr_clear_block = FALSE;
    g_mock_expr_rval = NULL;

    pr_di_nodtor.destructor = &dtor_r;
    cd_sc.new_routine = NULL;
    cd_sc.alloc_failure_gro_call = NULL;
    g_mock_lower_fn_null = TRUE;
    gcc_jit_function *cf_e3 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "cf_e3", 0, NULL, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(cf_e3, "entry")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&st_c_edge, cf_e3));
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
    g_mock_lower_fn_null = FALSE;

    /* 3) OOM sweep on stmk_coroutine with int return & fail_gro_rval == NULL & initk_zero promise (hits 1340 & 1403 OOM branches!) */
    cd_sc.alloc_failure_gro_call = &fg2;
    pr_edge.type = &zero_sz_tp;
    pr_edge.init_kind = (an_init_kind)initk_zero;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(80, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;
        gcc_jit_function *cf_swp = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "cf_swp", 0, NULL, 0);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(cf_swp, "entry")));
        return gcc_gen_be_lower_statement(&st_c_edge, cf_swp);
    }) );
    gcc_gen_be_singleton_state.active_coro_frame_lval = NULL;

    /* 4) Ramp & Dispatchers final branches: OOM on 3087 (fail_gro == NULL on int ramp), 0-param ramp with pcopy/pvar (p_idx >= ramp_pcount), sret/int/void ramp with non-NULL gro_rval & clear_block, void ue_rval with final_suspend_label, prom_di_nodtor in dispatchers */
    g_mock_coro_descr = &cd_edge;
    cd_edge.alloc_failure_gro_call = &fg2;
    cd_edge.new_routine = NULL;
    pr2.initializer.dynamic = &pr_di_nodtor;
    pr_di_nodtor.destructor = NULL; /* 3532:48 False! */
    g_mock_expr_rval = NULL;
    GCC_GEN_BE_CHECK( sweep_oom_stmt(120, [&]() noexcept -> gcc_gen_be_error_t {
        gcc_jit_function *rf_fg = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "rf_fg", 0, NULL, 0);
        return gcc_gen_be_lower_coroutine_ramp(&r_edge, rf_fg);
    }) );
    /* void ramp with fail_gro == NULL -> 3083:17 False! */
    gcc_jit_function *rf_fg_v = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "rf_fg_v", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&r_edge, rf_fg_v));
    cd_edge.alloc_failure_gro_call = NULL;

    /* 0-param ramp with pcopy & pvar -> p_idx(0) >= ramp_pcount(0) -> 3158:21 & 3178:21 False! Plus void ue_rval with final_suspend_label (&fsl2 -> 3424:42 False)! */
    a_variable pcopy_extra = {}, pvar_extra = {};
    pcopy_extra.type = &zero_sz_tp;
    pvar_extra.type = &zero_sz_tp;
    cd_edge.parameter_copies = &pcopy_extra;
    tmp_try_sc.variant.routine.parameters = &pvar_extra;
    tmp_try_sc.assoc_block = &s_fsl2;
    cd_edge.final_suspend_label = &fsl2;
    fdd2[1].scope = &tmp_try_sc;
    il_header.function_def_table = fdd2;
    r_edge.function_def_number = 1;
    g_mock_expr_rval = gcc_jit_context_new_call(ctx, NULL, void_callee, 0, NULL);
    gcc_jit_function *rf_0p = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "rf_0p", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&r_edge, rf_0p));

    /* Non-NULL gro_rval / ue_rval / fs_rval / is_rval WITH g_mock_expr_clear_block = TRUE across sret, int, and void ramps + dispatchers with & without final_suspend_label! */
    g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
    g_mock_expr_clear_block = TRUE;
    /* a) sret ramp with non-NULL gro_rval & entry == NULL (3219:44 False) + dispatchers with final_suspend_label (3379:24, 3424:28, 3427:17 False) */
    gcc_jit_param *sp_cb = gcc_jit_context_new_param(ctx, NULL, ip_t, "sp_cb");
    gcc_jit_function *rf_sret_cb = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "rf_sret_cb", 1, &sp_cb, 0);
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, rf_sret_cb, sp_cb));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&r_edge, rf_sret_cb));
    /* b) int ramp with non-NULL gro_rval & entry == NULL (3225:33 False) + dispatchers without final_suspend_label (3444:28, 3447:17, 3462:28 False) */
    cd_edge.final_suspend_label = NULL;
    tmp_try_sc.assoc_block = NULL;
    gcc_jit_function *rf_int_cb = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, int_t, "rf_int_cb", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&r_edge, rf_int_cb));
    /* c) void ramp with non-NULL gro_rval & entry == NULL (3241:36 False) */
    gcc_jit_function *rf_void_cb = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "rf_void_cb", 0, NULL, 0);
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&r_edge, rf_void_cb));
    g_mock_expr_clear_block = FALSE;

    /* Also sret ramp with gro_rval == NULL (3219:32 False), uncached pcopy/pvar (type == NULL -> 3158:21 & 3178:21 False), stmk_expr with rval == NULL (1173:23 False), and coro_eh_lp cleared when initial_suspend_call == NULL (3424:28, 3427:17, 3444:28, 3447:17 False)! */
    g_mock_expr_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_v, "ex_nullrv")));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&s_ex, fn_v));

    pcopy_extra.type = NULL;
    pvar_extra.type = NULL;
    gcc_jit_param *sp_ng = gcc_jit_context_new_param(ctx, NULL, ip_t, "sp_ng");
    gcc_jit_function *rf_sret_ng = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "rf_sret_ng", 1, &sp_ng, 0);
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, rf_sret_ng, sp_ng));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&r_edge, rf_sret_ng));

    cd_edge.initial_suspend_call = NULL;
    g_mock_expr_rval = gcc_jit_context_zero(ctx, int_t);
    g_mock_expr_clear_block = TRUE;
    tmp_try_sc.assoc_block = &s_fsl2;
    cd_edge.final_suspend_label = &fsl2;
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_dispatchers(&r_edge, &rf, &df));
    tmp_try_sc.assoc_block = NULL;
    cd_edge.final_suspend_label = NULL;
    GCC_GEN_BE_CHECK(cache_clear_all());
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_dispatchers(&r_edge, &rf, &df));
    g_mock_expr_clear_block = FALSE;
    g_mock_expr_rval = NULL;

    g_mock_coro_descr = NULL;
    il_header.function_def_table = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes statement lowering null-pointer validation, SEH, control-flow, and coroutine test suites.
 * @details Orchestrates all sub-suites and verifies clean resource teardown via `gcc_gen_be_cleanup`.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t run_stmt_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_block *block = NULL;
    gcc_jit_type *type = NULL;
    gcc_jit_function *func = NULL;
    gcc_jit_lvalue *lval = NULL;

    if (gcc_gen_be_get_label_block(NULL, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_switch_case_block(NULL, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_label_block(NULL, NULL, &block));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_switch_case_block(NULL, NULL, &block));
    if (gcc_gen_be_lower_statement(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_finalize_computed_goto(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_microsoft_try(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_stack_save(NULL, NULL, &lval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_stack_restore(NULL, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_frame(NULL, &type) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_ramp(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_coroutine_dispatchers(NULL, &func, &func) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

#if MICROSOFT_EXTENSIONS_ALLOWED
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *seh_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "seh_test_fn", 0, NULL, 0);

    /* mtsp == NULL early exit + full SEH nested __try/__finally + __try/__except + early return under OOM sweep! */
    a_statement null_mtsp_stmt = {};
    null_mtsp_stmt.kind = stmk_microsoft_try;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_microsoft_try(&null_mtsp_stmt, seh_fn));

    a_statement empty_guarded = {}; empty_guarded.kind = stmk_empty;
    a_statement empty_cleanup = {}; empty_cleanup.kind = stmk_empty;
    a_statement early_ret_stmt = {}; early_ret_stmt.kind = stmk_return;
    an_expr_node filter_expr = {};
    a_microsoft_try_supplement inner_except_supp = {};
    inner_except_supp.guarded_statement = &early_ret_stmt;
    inner_except_supp.cleanup_statement = &empty_cleanup;
    inner_except_supp.except_expr = &filter_expr;
    a_statement inner_except_stmt = {};
    inner_except_stmt.kind = stmk_microsoft_try;
    inner_except_stmt.variant.microsoft_try = &inner_except_supp;

    a_microsoft_try_supplement finally_supp = {};
    finally_supp.guarded_statement = &inner_except_stmt;
    finally_supp.cleanup_statement = &empty_cleanup;
    a_statement finally_stmt = {};
    finally_stmt.kind = stmk_microsoft_try;
    finally_stmt.variant.microsoft_try = &finally_supp;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(120, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(seh_fn, "seh_entry")));
        g_mock_expr_rval = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&finally_stmt, seh_fn));
        /* Second call hits cached setjmp and NULL filter_res */
        g_mock_expr_rval = NULL;
        finally_supp.guarded_statement = &empty_guarded;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_microsoft_try(&finally_stmt, seh_fn));
        finally_supp.guarded_statement = &inner_except_stmt;
        return GCC_GEN_BE_SUCCESS;
    }) );
#endif

    GCC_GEN_BE_CHECK(test_labels_computed_goto_and_guards(ctx));
    GCC_GEN_BE_CHECK(test_control_flow_blocks_and_asm(ctx));
    GCC_GEN_BE_CHECK(test_coroutines_and_oom(ctx));
    GCC_GEN_BE_CHECK(test_stmt_edge_branches(ctx));

    /* Test inline assembly constraint normalization + OOM sweep */
    char out_cstr[32];
    char tied_cstr[16];
    a_boolean is_rw = FALSE;
    if (gcc_gen_be_normalize_asm_constraints("+r", 0, NULL, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("+r", 0, out_cstr, 0, tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("+r", 0, out_cstr, sizeof(out_cstr), NULL, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("+r", 0, out_cstr, sizeof(out_cstr), tied_cstr, 0, &is_rw) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("+r", 0, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("+r", -1, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints(NULL, 0, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("", 0, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("r", 0, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("+", 0, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("+r", 0, out_cstr, 2, tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("+r", 12, out_cstr, sizeof(out_cstr), tied_cstr, 2, &is_rw) != GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_normalize_asm_constraints("=r", 0, out_cstr, 2, tied_cstr, sizeof(tied_cstr), &is_rw) != GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK( sweep_oom_stmt(10, [&]() noexcept -> gcc_gen_be_error_t {
        GCC_GEN_BE_CHECK(gcc_gen_be_normalize_asm_constraints("+r", 2, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw));
        return gcc_gen_be_normalize_asm_constraints("=&r", 1, out_cstr, sizeof(out_cstr), tied_cstr, sizeof(tied_cstr), &is_rw);
    }) );

    /* Stage-2 stmt coverage: goto_lt C++/C_mode branches, stmk_if struct/pointer cond, and cleanup_routine + OOM */
    {
        gcc_jit_context *s2_st_ctx = gcc_jit_context_acquire();
        GCC_GEN_BE_CHECK(gcc_gen_be_set_context(s2_st_ctx));
        gcc_jit_context *ctx = s2_st_ctx;
        gcc_jit_type *vt_s2 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_type *it_s2 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        gcc_jit_function *fn_s2 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, "stmt_s2_fn", 0, NULL, 0);

        /* 1) stmk_goto with il_header.primary_scope != NULL across C++ (!C_mode()) and C (C_mode() with goto_lt NULL & non-NULL) */
        a_scope pri_s2 = {}; pri_s2.kind = sck_file;
        a_label lbl_s2 = {};
        an_object_lifetime lt_s2 = {};
        a_statement goto_s2 = {};
        goto_s2.kind = stmk_goto;
        goto_s2.variant.label.ptr = &lbl_s2;

        il_header.primary_scope = &pri_s2;
        C_dialect = C_dialect_cplusplus;
        goto_s2.variant.label.lifetime = &lt_s2;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_goto_cpp")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&goto_s2, fn_s2));

        C_dialect = C_dialect_ANSI;
        goto_s2.variant.label.lifetime = &lt_s2; /* C_mode() && goto_lt != NULL -> 1640:42 False! */
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_goto_c_nonnull")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&goto_s2, fn_s2));

        goto_s2.variant.label.lifetime = NULL; /* C_mode() && goto_lt == NULL -> 1640:42 True! */
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_goto_c_null")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&goto_s2, fn_s2));
        C_dialect = C_dialect_cplusplus;
        il_header.primary_scope = NULL;

        /* 2) stmk_if with struct condition rvalue (1696:15 True, 0-field struct & 1-field struct) */
        gcc_jit_struct *st0_s = gcc_jit_context_new_struct_type(ctx, NULL, "IfCondSt0", 0, NULL);
        gcc_jit_field *st1_f = gcc_jit_context_new_field(ctx, NULL, it_s2, "v");
        gcc_jit_struct *st1_s = gcc_jit_context_new_struct_type(ctx, NULL, "IfCondSt1", 1, &st1_f);
        an_expr_node if_ce = {}; if_ce.kind = enk_constant;
        a_statement emp_then = {}; emp_then.kind = stmk_empty;
        a_statement if_s2 = {}; if_s2.kind = stmk_if; if_s2.expr = &if_ce; if_s2.variant.if_stmt.then_statement = &emp_then;

        g_mock_expr_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, gcc_jit_struct_as_type(st0_s), 0, NULL, NULL);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_if_st0")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&if_s2, fn_s2));

        g_mock_expr_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, gcc_jit_struct_as_type(st1_s), 0, NULL, NULL);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_if_st1")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&if_s2, fn_s2));
        g_mock_expr_rval = NULL;

        /* 3) stmk_init with dip->variable->cleanup_routine (auto, static, thread_local, null cl_fn, valid cl_fn + OOM) */
        a_type cl_v_tp = {}; cl_v_tp.kind = tk_integer; cl_v_tp.size = 4;
        a_routine cl_rout = {}; cl_rout.source_corresp.name = "__s2_gnu_cleanup";

        a_variable cv_static = {}, cv_tls = {}, cv_auto = {};
        cv_static.source_corresp.name = "cv_st"; cv_static.type = &cl_v_tp; cv_static.storage_class = sc_static; cv_static.cleanup_routine = &cl_rout;
        cv_tls.source_corresp.name = "cv_tl"; cv_tls.type = &cl_v_tp; cv_tls.is_thread_local = TRUE; cv_tls.cleanup_routine = &cl_rout;
        cv_auto.source_corresp.name = "cv_au"; cv_auto.type = &cl_v_tp; cv_auto.cleanup_routine = &cl_rout;
        gcc_jit_lvalue *lv_st = gcc_jit_function_new_local(fn_s2, NULL, it_s2, "lv_st");
        gcc_jit_lvalue *lv_tl = gcc_jit_function_new_local(fn_s2, NULL, it_s2, "lv_tl");
        gcc_jit_lvalue *lv_au = gcc_jit_function_new_local(fn_s2, NULL, it_s2, "lv_au");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &cv_static, lv_st));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &cv_tls, lv_tl));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &cv_auto, lv_au));

        a_dynamic_init di_cl = {}; di_cl.kind = dik_zero;
        a_statement init_cl = {}; init_cl.kind = stmk_init; init_cl.variant.dynamic_init = &di_cl;

        GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
        di_cl.variable = &cv_static;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_cl_st")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&init_cl, fn_s2));

        di_cl.variable = &cv_tls;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_cl_tl")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&init_cl, fn_s2));

        di_cl.variable = &cv_auto;
        g_mock_lower_fn_null = TRUE; /* cl_fn == NULL -> 2104:35 False */
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_cl_nullfn")));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&init_cl, fn_s2));
        g_mock_lower_fn_null = FALSE;

        GCC_GEN_BE_CHECK( sweep_oom_stmt(15, [&]() noexcept -> gcc_gen_be_error_t {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &cv_auto, lv_au));
            GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_cl_oom")));
            return gcc_gen_be_lower_statement(&init_cl, fn_s2);
        }) );

        /* 4) stmk_set_vla_size edge cases (1965..1986) and uncached dim->dimension_variable OOM in gcc_gen_be_lower_vla_decl (2408) */
        {
            a_type vla_el_tp = {}, vla_arr_tp = {};
            vla_el_tp.kind = tk_integer; vla_el_tp.size = 4;
            vla_arr_tp.kind = tk_array; vla_arr_tp.variant.array.is_variable_size_array = TRUE;
            vla_arr_tp.variant.array.has_assoc_vla_dimension = TRUE; vla_arr_tp.variant.array.element_type = &vla_el_tp;
            a_variable vla_dv = {}, vla_var_decl = {};
            vla_dv.source_corresp.name = "vla_dv_s2"; vla_dv.type = &vla_el_tp;
            vla_var_decl.source_corresp.name = "vla_arr_s2"; vla_var_decl.type = &vla_arr_tp;
            an_expr_node vla_de = {}; vla_de.kind = enk_constant; vla_de.type = &vla_el_tp;
            a_vla_dimension vdim = {}; vdim.dimension_expr = &vla_de; vdim.dimension_variable = &vla_dv; vdim.type = &vla_arr_tp;
            a_statement vla_st = {}; vla_st.kind = stmk_set_vla_size; vla_st.variant.vla_dimension = &vdim;

            /* dim_rv == NULL (1965:27 False) */
            g_mock_expr_rval = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_vla0")));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&vla_st, fn_s2));

            /* dim_rv != NULL (int != size_t -> 1982:57 False), func == NULL with uncached dv (1974:46 False, 1980:31 False) */
            gcc_jit_rvalue *int_dim_rv = gcc_jit_context_new_rvalue_from_int(s2_st_ctx, it_s2, 8);
            g_mock_expr_rval = int_dim_rv;
            GCC_GEN_BE_CHECK(cache_clear_all());
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&vla_st, NULL));

            /* dim->dimension_variable == NULL: func == NULL (1977:38 False) and func != NULL (1977:38 True, 1978..1979) */
            vdim.dimension_variable = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&vla_st, NULL));
            vdim.type = NULL; /* 1986:35 False */
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_vla1")));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&vla_st, fn_s2));

            /* cblock == NULL after dimension_expr lowering with valid dim_lv (1980:41 False) */
            vdim.type = &vla_arr_tp;
            g_mock_expr_clear_block = TRUE;
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_vla_clb")));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&vla_st, fn_s2));
            g_mock_expr_clear_block = FALSE;
            vdim.dimension_variable = &vla_dv;
            GCC_GEN_BE_CHECK(cache_clear_all());
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_vla2")));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(&vla_st, fn_s2));
            g_mock_expr_rval = NULL;

            /* Uncached dim->dimension_variable OOM in gcc_gen_be_lower_vla_decl (line 2408) */
            a_scope vla_fsc = {}; vla_fsc.kind = sck_function; vla_fsc.vla_dimensions = &vdim;
            innermost_function_scope = &vla_fsc;
            GCC_GEN_BE_CHECK( sweep_oom_stmt(60, [&]() noexcept -> gcc_gen_be_error_t {
                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(gcc_jit_function_new_block(fn_s2, "s2_vla_decl_oom")));
                gcc_jit_lvalue *saved_sp = NULL;
                return gcc_gen_be_lower_vla_decl(fn_s2, &vla_var_decl, &saved_sp);
            }) );
            innermost_function_scope = NULL;
        }

        /* 5) Coroutine descriptor search, ramp sret/edg_ret_as_param/parameters, and destroy DIP + delete_routine (2850..3853) */
        {
            g_mock_coro_descr = NULL;
            gcc_jit_type *vp_s2 = gcc_jit_context_get_type(s2_st_ctx, GCC_JIT_TYPE_VOID_PTR);
            gcc_jit_type *ip_s2 = gcc_jit_type_get_pointer(it_s2);
            gcc_jit_type *lt_s2 = gcc_jit_context_get_type(s2_st_ctx, GCC_JIT_TYPE_LONG);

            a_type cr_void = {}, cr_int = {}, cr_ptr = {}, cr_rty = {}, cr_rty_nosupp = {}, cr_tref = {};
            cr_void.kind = tk_void;
            cr_int.kind = tk_integer; cr_int.size = 4;
            cr_ptr.kind = tk_pointer; cr_ptr.size = 8; cr_ptr.variant.pointer.type = &cr_int;
            a_param_type cr_p0 = {}; cr_p0.type = &cr_ptr;
            a_routine_type_supplement cr_supp = {}; cr_supp.param_type_list = &cr_p0; cr_supp.value_returned_as_parameter = TRUE;
            cr_rty.kind = tk_routine; cr_rty.variant.routine.return_type = &cr_void; cr_rty.variant.routine.extra_info = &cr_supp;
            cr_rty_nosupp.kind = tk_routine; cr_rty_nosupp.variant.routine.return_type = &cr_void; cr_rty_nosupp.variant.routine.extra_info = NULL;
            cr_tref.kind = tk_typeref; cr_tref.variant.typeref.type = &cr_rty;

            a_variable cp_prom = {}, cp_pvar1 = {}, cp_pvar2 = {}, cp_lv_mapped = {}, cp_lv_unmapped = {};
            cp_prom.type = &cr_int; cp_prom.source_corresp.name = "cp_prom";
            cp_pvar1.type = &cr_int; cp_pvar1.source_corresp.name = "cp_pv1"; cp_pvar1.next = &cp_pvar2;
            cp_pvar2.type = &cr_int; cp_pvar2.source_corresp.name = "cp_pv2";
            cp_lv_mapped.type = &cr_int; cp_lv_mapped.source_corresp.name = "cp_lvm";

            a_routine del_rout = {}, dt_rout = {};
            del_rout.source_corresp.name = "op_del_coro";
            dt_rout.source_corresp.name = "dtor_coro_var";

            a_coroutine_descr cdescr = {};
            cdescr.promise = &cp_prom;
            cdescr.delete_routine = &del_rout;

            /* Nested stmt list: stmk_expr -> stmk_block(empty) -> stmk_try_block(null) -> stmk_try_block(no-coro) -> stmk_try_block(block->coro) */
            a_statement cs_expr = {}, cs_blk_emp = {}, cs_try_null = {}, cs_inner_nocoro = {}, cs_try_nocoro = {}, cs_coro = {}, cs_inner_coro = {}, cs_try_coro = {}, cs_root_blk = {};
            cs_expr.kind = stmk_expr; cs_expr.next = &cs_blk_emp;
            cs_blk_emp.kind = stmk_block; cs_blk_emp.variant.block.statements = NULL; cs_blk_emp.next = &cs_try_null;
            cs_try_null.kind = static_cast<a_statement_kind>(stmk_try_block); cs_try_null.variant.try_block = NULL; cs_try_null.next = &cs_try_nocoro;
            cs_inner_nocoro.kind = stmk_expr;
            a_try_supplement tbs_nocoro = {}; tbs_nocoro.statement = &cs_inner_nocoro;
            cs_try_nocoro.kind = static_cast<a_statement_kind>(stmk_try_block); cs_try_nocoro.variant.try_block = &tbs_nocoro; cs_try_nocoro.next = &cs_try_coro;
            cs_coro.kind = stmk_coroutine; cs_coro.variant.coroutine.descr = &cdescr;
            cs_inner_coro.kind = stmk_block; cs_inner_coro.variant.block.statements = &cs_coro;
            a_try_supplement tbs_coro = {}; tbs_coro.statement = &cs_inner_coro;
            cs_try_coro.kind = static_cast<a_statement_kind>(stmk_try_block); cs_try_coro.variant.try_block = &tbs_coro;
            cs_root_blk.kind = stmk_block; cs_root_blk.variant.block.statements = &cs_expr;

            a_scope cfsc = {}; cfsc.kind = sck_function; cfsc.nonstatic_variables = &cp_lv_mapped;
            cfsc.variant.routine.parameters = &cp_pvar1;
            a_function_def_descr cfdd[2] = {}; cfdd[1].scope = &cfsc;

            a_routine crout = {}; crout.source_corresp.name = "s2_coro_r"; crout.type = &cr_tref; crout.function_def_number = 1; crout.is_coroutine = TRUE;
            size_t fsz = 0, faln = 0;
            /* function_def_table == NULL (2892:77 False) and assoc_block == NULL (2895:19 False) */
            il_header.function_def_table = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_size_and_align(&crout, &cfsc, &fsz, &faln));
            il_header.function_def_table = cfdd;
            cfsc.assoc_block = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_size_and_align(&crout, &cfsc, &fsz, &faln));
            /* Block with no stmk_coroutine (find_coro_descr_in_stmt returns NULL) */
            cs_try_nocoro.next = NULL;
            cfsc.assoc_block = &cs_root_blk;
            GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_size_and_align(&crout, &cfsc, &fsz, &faln));
            cs_try_nocoro.next = &cs_try_coro;

            /* OOM sweeps on compute_coro_frame_size_and_align, lower_coroutine_frame, lower_coroutine_ramp, lower_coroutine_dispatchers */
            GCC_GEN_BE_CHECK( sweep_oom_stmt(25, [&]() noexcept -> gcc_gen_be_error_t {
                return gcc_gen_be_compute_coro_frame_size_and_align(&crout, &cfsc, &fsz, &faln);
            }) );
            GCC_GEN_BE_CHECK( sweep_oom_stmt(25, [&]() noexcept -> gcc_gen_be_error_t {
                gcc_jit_type *fty = NULL;
                return gcc_gen_be_lower_coroutine_frame(&crout, &fty);
            }) );

            /* Ramp with extra_info == NULL (3243:13 False) & uncached def_scope parameter copies (3307..3310 + OOM) */
            crout.type = &cr_rty_nosupp;
            GCC_GEN_BE_CHECK( sweep_oom_stmt(60, [&]() noexcept -> gcc_gen_be_error_t {
                cfsc.variant.routine.parameters = NULL;
                gcc_jit_type *pre_fty = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_frame(&crout, &pre_fty));
                cfsc.variant.routine.parameters = &cp_pvar1;
                gcc_jit_param *rp_2[2] = {
                    gcc_jit_context_new_param(s2_st_ctx, NULL, ip_s2, "rp0"),
                    gcc_jit_context_new_param(s2_st_ctx, NULL, it_s2, "rp1")
                };
                char rnm[32]; std::snprintf(rnm, sizeof(rnm), "s2_ramp_ns_%d", g_mock_seq++);
                gcc_jit_function *rf = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, rnm, 1, rp_2, 0);
                return gcc_gen_be_lower_coroutine_ramp(&crout, rf);
            }) );
            crout.type = &cr_tref;

            /* Ramp GRO paths: (a) sret_cached != NULL with long gro_rval (3353..3358: coerce ok, coerce null, coerce OOM) */
            an_expr_node gro_c1 = {}, gro_c2 = {}, gro_call = {}, gro_callee = {}, gro_arg = {};
            gro_callee.kind = enk_routine; gro_callee.type = &cr_ptr; gro_callee.next = &gro_arg;
            gro_arg.kind = enk_variable; gro_arg.type = &cr_ptr; gro_arg.variant.variable.ptr = &cp_prom;
            gro_call.kind = enk_operation; gro_call.type = &cr_void; gro_call.variant.operation.kind = eok_call; gro_call.variant.operation.operands = &gro_callee;
            gro_c1.kind = enk_constant; gro_c1.type = &cr_int; gro_c1.next = &gro_call;
            gro_c2.kind = enk_operation; gro_c2.type = &cr_void; gro_c2.variant.operation.kind = eok_comma; gro_c2.variant.operation.operands = &gro_c1;
            cdescr.get_return_object_call = &gro_c2;

            gcc_jit_rvalue *long_rv = gcc_jit_context_new_rvalue_from_long(s2_st_ctx, lt_s2, 9L);
            gcc_jit_rvalue *int_rv = gcc_jit_context_new_rvalue_from_int(s2_st_ctx, it_s2, 5);
            gcc_jit_function *void_helper_fn = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, vt_s2, "__s2_void_gro", 0, NULL, 0);
            gcc_jit_rvalue *void_rv = gcc_jit_context_new_call(s2_st_ctx, NULL, void_helper_fn, 0, NULL);
            gcc_jit_rvalue *vptr_rv = gcc_jit_context_null(s2_st_ctx, vp_s2);

            g_mock_expr_rval = long_rv;
            GCC_GEN_BE_CHECK( sweep_oom_stmt(80, [&]() noexcept -> gcc_gen_be_error_t {
                gcc_jit_param *sp = gcc_jit_context_new_param(s2_st_ctx, NULL, ip_s2, "__sret");
                char rnm[32]; std::snprintf(rnm, sizeof(rnm), "s2_ramp_sc_%d", g_mock_seq++);
                gcc_jit_function *rf = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, rnm, 1, &sp, 0);
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, rf, sp));
                return gcc_gen_be_lower_coroutine_ramp(&crout, rf);
            }) );
            g_mock_coerce_null = TRUE;
            {
                gcc_jit_param *sp = gcc_jit_context_new_param(s2_st_ctx, NULL, ip_s2, "__sret");
                gcc_jit_function *rf = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, "s2_ramp_sc_nullc", 1, &sp, 0);
                GCC_GEN_BE_CHECK(cache_clear_all());
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, rf, sp));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&crout, rf));
            }
            g_mock_coerce_null = FALSE;

            /* (b) edg_ret_as_param == TRUE, sret_cached == NULL: int_rv, long_rv (+ coerce null + OOM), 0-param ramp, entry==NULL */
            g_mock_expr_rval = int_rv;
            {
                gcc_jit_param *sp = gcc_jit_context_new_param(s2_st_ctx, NULL, ip_s2, "__sret");
                gcc_jit_function *rf1 = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, "s2_ramp_er_int", 1, &sp, 0);
                gcc_jit_function *rf0 = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, "s2_ramp_er_0p", 0, NULL, 0);
                GCC_GEN_BE_CHECK(cache_clear_all());
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&crout, rf1));
                GCC_GEN_BE_CHECK(cache_clear_all());
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&crout, rf0));
                g_mock_expr_clear_block = TRUE;
                gcc_jit_param *spb = gcc_jit_context_new_param(s2_st_ctx, NULL, ip_s2, "__sret");
                gcc_jit_function *rfb = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, "s2_ramp_er_clb", 1, &spb, 0);
                GCC_GEN_BE_CHECK(cache_clear_all());
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&crout, rfb));
                g_mock_expr_clear_block = FALSE;
            }
            g_mock_expr_rval = long_rv;
            GCC_GEN_BE_CHECK( sweep_oom_stmt(80, [&]() noexcept -> gcc_gen_be_error_t {
                gcc_jit_param *sp = gcc_jit_context_new_param(s2_st_ctx, NULL, ip_s2, "__sret");
                char rnm[32]; std::snprintf(rnm, sizeof(rnm), "s2_ramp_er_l_%d", g_mock_seq++);
                gcc_jit_function *rf = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, rnm, 1, &sp, 0);
                return gcc_gen_be_lower_coroutine_ramp(&crout, rf);
            }) );
            g_mock_coerce_null = TRUE;
            {
                gcc_jit_param *sp = gcc_jit_context_new_param(s2_st_ctx, NULL, ip_s2, "__sret");
                gcc_jit_function *rf = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, "s2_ramp_er_lnull", 1, &sp, 0);
                GCC_GEN_BE_CHECK(cache_clear_all());
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&crout, rf));
            }
            g_mock_coerce_null = FALSE;

            /* (c) void_rv & NULL gro_rval on edg_ret_as_param / sret_cached: comma unwrap, eok_call tmp_ptr copy + OOM + edge cases */
            g_mock_expr_rval = void_rv;
            g_mock_alt_expr_node = &gro_arg;
            g_mock_alt_expr_rval = vptr_rv;
            GCC_GEN_BE_CHECK( sweep_oom_stmt(160, [&]() noexcept -> gcc_gen_be_error_t {
                gcc_jit_param *sp = gcc_jit_context_new_param(s2_st_ctx, NULL, ip_s2, "__sret");
                char rnm[32]; std::snprintf(rnm, sizeof(rnm), "s2_ramp_vgro_%d", g_mock_seq++);
                gcc_jit_function *rf = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, rnm, 1, &sp, 0);
                return gcc_gen_be_lower_coroutine_ramp(&crout, rf);
            }) );
            {
                /* sret_cached != NULL with NULL gro_rval reaches 3380 (3380:39 True), gro_tmp_ptr == NULL, inner entry == NULL, 0-param, non-call, short comma/call */
                g_mock_expr_rval = NULL;
                gcc_jit_param *sp1 = gcc_jit_context_new_param(s2_st_ctx, NULL, ip_s2, "__sret");
                gcc_jit_function *rf_scv = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, "s2_ramp_sc_vgro", 1, &sp1, 0);
                GCC_GEN_BE_CHECK(cache_clear_all());
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, rf_scv, sp1));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&crout, rf_scv));
                g_mock_expr_rval = void_rv;

                gcc_jit_function *rf_0pv = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, "s2_ramp_0p_vgro", 0, NULL, 0);
                GCC_GEN_BE_CHECK(cache_clear_all());
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&crout, rf_0pv));

                g_mock_alt_expr_rval = NULL; /* 3398:25 False */
                gcc_jit_param *sp2 = gcc_jit_context_new_param(s2_st_ctx, NULL, ip_s2, "__sret");
                gcc_jit_function *rf_nt = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, "s2_ramp_vgro_nt", 1, &sp2, 0);
                GCC_GEN_BE_CHECK(cache_clear_all());
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&crout, rf_nt));

                g_mock_alt_expr_rval = vptr_rv;
                g_mock_alt_expr_clear_block = TRUE; /* 3398:40 False */
                gcc_jit_param *sp3 = gcc_jit_context_new_param(s2_st_ctx, NULL, ip_s2, "__sret");
                gcc_jit_function *rf_cb = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, "s2_ramp_vgro_cb", 1, &sp3, 0);
                GCC_GEN_BE_CHECK(cache_clear_all());
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&crout, rf_cb));
                g_mock_alt_expr_clear_block = FALSE;

                /* Comma with operands==NULL, comma with operands->next==NULL, call with operands==NULL, call with next==NULL, non-call */
                an_expr_node bad_comma1 = {}, bad_comma2 = {}, bad_call1 = {}, bad_call2 = {}, non_call = {};
                bad_comma1.kind = enk_operation; bad_comma1.variant.operation.kind = eok_comma; bad_comma1.variant.operation.operands = NULL;
                bad_comma2.kind = enk_operation; bad_comma2.variant.operation.kind = eok_comma; bad_comma2.variant.operation.operands = &non_call;
                bad_call1.kind = enk_operation; bad_call1.variant.operation.kind = eok_call; bad_call1.variant.operation.operands = NULL;
                an_expr_node lone_callee = {}; lone_callee.kind = enk_routine;
                bad_call2.kind = enk_operation; bad_call2.variant.operation.kind = eok_call; bad_call2.variant.operation.operands = &lone_callee;
                non_call.kind = enk_constant;
                an_expr_node *gro_variants[5] = {&bad_comma1, &bad_comma2, &bad_call1, &bad_call2, &non_call};
                g_mock_expr_rval = NULL; /* 3377:21 False */
                for (int vi = 0; vi < 5; ++vi) {
                    cdescr.get_return_object_call = gro_variants[vi];
                    gcc_jit_param *spv = gcc_jit_context_new_param(s2_st_ctx, NULL, ip_s2, "__sret");
                    char vnm[32]; std::snprintf(vnm, sizeof(vnm), "s2_ramp_gv_%d", vi);
                    gcc_jit_function *rfv = gcc_jit_context_new_function(s2_st_ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, vt_s2, vnm, 1, &spv, 0);
                    GCC_GEN_BE_CHECK(cache_clear_all());
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_ramp(&crout, rfv));
                }
            }
            g_mock_alt_expr_node = NULL;
            g_mock_alt_expr_rval = NULL;
            g_mock_expr_rval = NULL;
            cdescr.get_return_object_call = NULL;

            /* (d) Destroy-block has_primary_il_scope DIP destructors (0/1/2p-ptr/2p-int/null-fn + OOM) & 2/3-param delete_routine */
            an_object_lifetime clt_sub = {}, clt_null = {};
            a_dynamic_init cd_nodt = {}, cd_novar = {}, cd_prom = {}, cd_unmap = {}, cd_map1 = {}, cd_map2 = {};
            cd_nodt.destructor = NULL; cd_nodt.variable = &cp_lv_mapped; cd_nodt.next_in_destruction_list = &cd_novar;
            cd_novar.destructor = &dt_rout; cd_novar.variable = NULL; cd_novar.next_in_destruction_list = &cd_prom;
            cd_prom.destructor = &dt_rout; cd_prom.variable = &cp_prom; cd_prom.next_in_destruction_list = &cd_unmap;
            cd_unmap.destructor = &dt_rout; cd_unmap.variable = &cp_lv_unmapped; cd_unmap.next_in_destruction_list = &cd_map1;
            cd_map1.destructor = &dt_rout; cd_map1.variable = &cp_lv_mapped; cd_map1.lifetime = &clt_sub;
            clt_sub.parent_destruction_sublist = &cd_map2;
            cd_map2.destructor = &dt_rout; cd_map2.variable = &cp_lv_mapped; cd_map2.lifetime = NULL;

            gcc_gen_be_singleton_state.has_primary_il_scope = TRUE;
            gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 1U;
            gcc_gen_be_singleton_state.eh_cleanup_dips[0] = &cd_nodt;

            /* 258 suspends + 2-param p1=void* dtor + 2-param delete_routine + OOM */
            g_mock_try_many_suspends = TRUE;
            g_mock_fn_param_count = 2;
            g_mock_fn_p1_int = FALSE;
            GCC_GEN_BE_CHECK( sweep_oom_stmt(120, [&]() noexcept -> gcc_gen_be_error_t {
                g_mock_try_many_suspends = FALSE; /* keep OOM sweep fast (2 suspends) */
                gcc_jit_type *fty = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_frame(&crout, &fty));
                gcc_jit_function *rfn = NULL, *dfn = NULL;
                return gcc_gen_be_lower_coroutine_dispatchers(&crout, &rfn, &dfn);
            }) );
            /* Also run once with g_mock_try_many_suspends = TRUE (covers >= 256 cap & ddepth == 65U), pcount=3 delete, p1_int=TRUE, pcount=1, pcount=0, null dtor_fn */
            for (int mode = 0; mode < 5; ++mode) {
                GCC_GEN_BE_CHECK(cache_clear_all());
                g_mock_try_many_suspends = (mode == 0) ? TRUE : FALSE;
                g_mock_fn_zero_params = (mode == 1) ? TRUE : FALSE;
                g_mock_lower_fn_null = (mode == 2) ? TRUE : FALSE;
                cdescr.delete_routine = (mode == 2) ? NULL : &del_rout;
                g_mock_fn_param_count = (mode == 3) ? 3 : ((mode == 4) ? 2 : 1);
                g_mock_fn_p1_int = (mode == 4) ? TRUE : FALSE;
                gcc_jit_type *fty = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_frame(&crout, &fty));
                gcc_jit_function *rfn = NULL, *dfn = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_dispatchers(&crout, &rfn, &dfn));
            }
            g_mock_try_many_suspends = FALSE;
            g_mock_fn_zero_params = FALSE;
            g_mock_lower_fn_null = FALSE;
            g_mock_fn_param_count = 1;
            g_mock_fn_p1_int = FALSE;
            gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 0U;
            gcc_gen_be_singleton_state.has_primary_il_scope = FALSE;
            il_header.function_def_table = NULL;
        }
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_stmt.cpp`.
 * @details Runs `run_stmt_unit_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_stmt_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
