/**
 * @file test_gcc_be_context.cpp
 * @brief Exhaustive unit tests for context lifecycle, options, stacks, scopes, cleanups, and linkage (`gcc_gen_be_context.c`).
 * @details Exercises every public and internal path in `src/gcc_gen_be_context.c`, including
 * command-line/driver option combinations, block/landing-pad/cleanup/scope dynamic resizing,
 * destructor arity emission (`0, 1, 2`), anonymous scope classification, weak linkage fallbacks,
 * thread-safety validation, and deterministic OOM fault-injection sweeps.
 * @ingroup gcc_gen_be_tests
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_lib_loader.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for context tests.
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
END_EDG_NAMESPACE

/** @brief Counter of cleanup callback invocations. */
static int g_cleanup_cb_hits = 0;

/**
 * @brief Succeeding RAII cleanup callback incrementing `g_cleanup_cb_hits`.
 * @param[in,out] data Optional `int *` incremented by 1.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
static gcc_gen_be_error_t cleanup_cb_ok(void *data) GCC_GEN_BE_NOEXCEPT {
    ++g_cleanup_cb_hits;
    if (data) ++(*static_cast<int *>(data));
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Failing RAII cleanup callback testing error percolation in `gcc_gen_be_execute_cleanups`.
 * @param[in] data Unused opaque pointer.
 * @return `GCC_GEN_BE_ERROR_INTERNAL`.
 */
static gcc_gen_be_error_t cleanup_cb_fail(void *data) GCC_GEN_BE_NOEXCEPT {
    return data ? GCC_GEN_BE_ERROR_INTERNAL : GCC_GEN_BE_ERROR_INTERNAL;
}

/**
 * @brief Mock `gcc_jit_lvalue_add_attribute` callback.
 * @param[in] lval Target lvalue.
 * @param[in] attr Variable attribute enum.
 */
static void mock_var_attr(gcc_jit_lvalue *lval, enum gcc_jit_variable_attribute attr) noexcept {
    if (lval && (int)attr >= 0) ++g_cleanup_cb_hits;
}

/**
 * @brief Mock `gcc_jit_function_add_attribute` callback.
 * @param[in] fn Target function.
 * @param[in] attr Function attribute enum.
 */
static void mock_fn_attr(gcc_jit_function *fn, enum gcc_jit_fn_attribute attr) noexcept {
    if (fn && (int)attr >= 0) ++g_cleanup_cb_hits;
}

/**
 * @brief Mock `gcc_jit_context_add_top_level_asm` callback.
 * @param[in] ctx Active context.
 * @param[in] loc Optional location.
 * @param[in] asm_chars Top-level assembler string.
 */
static void mock_top_asm(gcc_jit_context *ctx, gcc_jit_location *loc, const char *asm_chars) noexcept {
    if (ctx && !loc && asm_chars) ++g_cleanup_cb_hits;
}

/**
 * @brief Exercises all option combinations in `gcc_gen_be_context_configure_options` and `gcc_gen_be_init`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_options_and_init_cleanup(void) GCC_GEN_BE_NOEXCEPT {
    if (gcc_gen_be_get_state(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = 0;
    gcc_gen_be_context_t *oom_st = NULL;
    if (gcc_gen_be_get_state(&oom_st) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;
    if (gcc_gen_be_get_context(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_current_block(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_context_configure_options(NULL) != GCC_GEN_BE_ERROR_NULL_POINTER) return GCC_GEN_BE_ERROR_INTERNAL;

    /* OOM sweeps on init & configure_options */
    for (int b = 0; b < 6; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_init();
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) {
            GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
            break;
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_init());
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_gen_be_context_t *st = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_state(&st));

    /* Exercise every opt_size_level, gcc_be_opt_level, visibility, sanitizer, flag, and C vs C++ dialect */
    gcc_be_debug_info = TRUE;
    gcc_be_dump_initial_tree = TRUE;
    gcc_be_dump_gimple = TRUE;
    gcc_be_dump_rtl_all = TRUE;
    exceptions_enabled = FALSE;
    gcc_be_ffast_math = TRUE;
    gcc_be_march = "armv8-a";
    gcc_be_mcpu = "generic";
    gcc_be_mtune = "generic";
    gcc_be_mfpu = "neon";
    gcc_be_mcmodel = "small";
    gcc_be_fPIC = TRUE;
    gcc_be_fPIE = TRUE;
    gcc_be_fno_pie = TRUE;
    gcc_be_fassociative_math = TRUE;
    gcc_be_fno_trapping_math = TRUE;
    gcc_be_m64 = TRUE;
    gcc_be_m32 = TRUE;
    gcc_be_shared = TRUE;
    gcc_be_static = TRUE;
    C_dialect = C_dialect_ANSI;

    st->opt_size_level = 2;
    st->lto_mode = 1;
    st->symbol_visibility = 1;
    st->function_sections = TRUE;
    st->data_sections = TRUE;
    st->openmp_enabled = TRUE;
    st->pthread_enabled = TRUE;
    st->sanitizers = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 4);

    st->driver_lib_dirs = (char **)calloc(2, sizeof(char *));
    st->driver_lib_dirs[0] = strdup("/usr/lib");
    st->driver_lib_dirs[1] = NULL;
    st->driver_lib_dirs_count = 2;

    st->driver_libs = (char **)calloc(2, sizeof(char *));
    st->driver_libs[0] = strdup("m");
    st->driver_libs[1] = NULL;
    st->driver_libs_count = 2;

    st->driver_ldflags = (char **)calloc(2, sizeof(char *));
    st->driver_ldflags[0] = strdup("-Wl,-v");
    st->driver_ldflags[1] = NULL;
    st->driver_ldflags_count = 2;

    for (int b = 0; b < 6; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_context_configure_options(ctx);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    st->opt_size_level = 1;
    st->symbol_visibility = 2;
    GCC_GEN_BE_CHECK(gcc_gen_be_context_configure_options(ctx));

    st->opt_size_level = 0;
    st->symbol_visibility = 3;
    for (int opt = -1; opt <= 3; ++opt) {
        gcc_be_opt_level = opt;
        GCC_GEN_BE_CHECK(gcc_gen_be_context_configure_options(ctx));
    }

    /* Reset flags and populate every dynamic table in backend_state to verify gcc_gen_be_cleanup frees all */
    gcc_be_opt_level = 0;
    gcc_be_debug_info = FALSE;
    gcc_be_dump_initial_tree = FALSE;
    gcc_be_dump_gimple = FALSE;
    gcc_be_dump_rtl_all = FALSE;
    exceptions_enabled = TRUE;
    gcc_be_ffast_math = FALSE;
    gcc_be_march = NULL;
    gcc_be_mcpu = NULL;
    gcc_be_mtune = NULL;
    gcc_be_mfpu = NULL;
    gcc_be_mcmodel = NULL;
    gcc_be_fPIC = FALSE;
    gcc_be_fPIE = FALSE;
    gcc_be_fno_pie = FALSE;
    gcc_be_fassociative_math = FALSE;
    gcc_be_fno_trapping_math = FALSE;
    gcc_be_m64 = FALSE;
    gcc_be_m32 = FALSE;
    gcc_be_shared = FALSE;
    gcc_be_static = FALSE;
    C_dialect = C_dialect_cplusplus;
    st->lto_mode = 0;
    st->symbol_visibility = 0;
    st->function_sections = FALSE;
    st->data_sections = FALSE;
    st->openmp_enabled = FALSE;
    st->pthread_enabled = FALSE;
    st->sanitizers = 0U;

    gcc_jit_type *vt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, vt, "f", 0, NULL, 0);
    gcc_jit_block *blk = gcc_jit_function_new_block(fn, "b");

    GCC_GEN_BE_CHECK(gcc_gen_be_push_break_block(blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_push_continue_block(blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_push_switch_exit_block(blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_push_block(blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_push_cleanup(&cleanup_cb_ok, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));

    st->computed_goto_labels = (a_label_ptr *)malloc(sizeof(a_label_ptr));
    st->active_coro_resume_blocks = (gcc_jit_block **)malloc(sizeof(gcc_jit_block *));
    st->active_coro_suspend_scopes = (a_scope_ptr *)malloc(sizeof(a_scope_ptr));
    gcc_gen_be_coro_state_t *cs1 = (gcc_gen_be_coro_state_t *)calloc(1, sizeof(gcc_gen_be_coro_state_t));
    gcc_gen_be_coro_state_t *cs2 = (gcc_gen_be_coro_state_t *)calloc(1, sizeof(gcc_gen_be_coro_state_t));
    cs1->resume_blocks = (gcc_jit_block **)malloc(sizeof(gcc_jit_block *));
    cs1->suspend_scopes = (a_scope_ptr *)malloc(sizeof(a_scope_ptr));
    cs1->prev = cs2; /* cs2 has NULL resume_blocks & NULL suspend_scopes! */
    st->coro_state_stack = cs1;

    st->eh_state.regions = (gcc_gen_be_eh_region_t *)calloc(1, sizeof(gcc_gen_be_eh_region_t));
    st->eh_state.types = (gcc_gen_be_eh_type_filter_t *)calloc(1, sizeof(gcc_gen_be_eh_type_filter_t));
    st->eh_state.action_table = (char *)malloc(16);

    st->active_subst_table.entries = (char **)calloc(2, sizeof(char *));
    st->active_subst_table.entries[0] = strdup("S_");
    st->active_subst_table.entries[1] = NULL;
    st->active_subst_table.count = 2;

    st->deferred_structs = malloc(16);
    st->tu_dwarf_die_table = malloc(16);
    st->visited_scopes = (void **)malloc(sizeof(void *));
    st->omp_pragma_entries = malloc(16);

    /* OOM on cache_clear_all inside gcc_gen_be_cleanup, then clean completion */
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_cleanup() != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
    /* Second cleanup when all pointers are already NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises block stacks, thread safety, RAII cleanups, scopes, landing pads, and weak linkage.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_stacks_scopes_and_linkage(void) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_init());
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_gen_be_context_t *st = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_state(&st));

    gcc_jit_type *vt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *vpt = gcc_jit_type_get_pointer(vt);
    gcc_jit_type *it = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, vt, "f2", 0, NULL, 0);
    gcc_jit_block *blk = gcc_jit_function_new_block(fn, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));
    gcc_jit_block *got_blk = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&got_blk));
    if (got_blk != blk) return GCC_GEN_BE_ERROR_INTERNAL;

    /* 1. Block stacks: NULL, underflow, OOM, resize >8, pop */
    if (gcc_gen_be_push_break_block(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_pop_break_block(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_pop_break_block(&got_blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_push_break_block(blk) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;

    for (int i = 0; i < 10; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_push_break_block(blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_push_continue_block(blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_push_switch_exit_block(blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_push_block(blk));
    }
    for (int i = 0; i < 10; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_break_block(&got_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_continue_block(&got_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_switch_exit_block(&got_blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_block(&got_blk));
    }

    /* 2. Thread safety: matching tid, mismatched tid, and OOM */
    GCC_GEN_BE_CHECK(gcc_gen_be_check_thread_safety());
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_check_thread_safety() != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;
    unsigned long saved_tid = st->thread_id;
    st->thread_id = saved_tid ^ 0x123456UL;
    if (gcc_gen_be_check_thread_safety() != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
    st->thread_id = saved_tid;

    /* 3. RAII cleanup callbacks: NULL, OOM, resize >16, success, failure */
    if (gcc_gen_be_push_cleanup(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_push_cleanup(&cleanup_cb_ok, NULL) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;
    int cb_counter = 0;
    for (int i = 0; i < 18; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_push_cleanup(&cleanup_cb_ok, &cb_counter));
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_execute_cleanups());
    if (cb_counter != 18) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_push_cleanup(&cleanup_cb_fail, NULL));
    if (gcc_gen_be_execute_cleanups() != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;

    /* 4. Landing pad stack: NULL, underflow, OOM, resize >16, get_active, pop */
    gcc_jit_block *lp_out = NULL;
    if (gcc_gen_be_get_active_landing_pad(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&lp_out));
    if (lp_out != NULL) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_push_landing_pad(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_pop_landing_pad() != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    for (int b = 0; b < 2; ++b) {
        g_gcc_be_oom_countdown = b;
        if (gcc_gen_be_push_landing_pad(blk) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
        g_gcc_be_oom_countdown = -1;
    }

    for (int i = 0; i < 18; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(blk));
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&lp_out));
    if (lp_out != blk) return GCC_GEN_BE_ERROR_INTERNAL;
    for (int i = 0; i < 18; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    }

    /* 5. Scopes, destructors (0, 1, 2 params), target_lifetime sentinels, landing-pad unwind tracking */
    if (gcc_gen_be_register_cleanup(NULL, NULL) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_pop_scope(blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_record_scope_unwind_lp() != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_has_scope_unwind_lp(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    a_boolean has_lp = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_has_scope_unwind_lp(&has_lp));
    if (has_lp != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;

    /* OOM on push_scope realloc & alloc */
    for (int b = 0; b < 4; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_push_scope(NULL);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) {
            GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));
            break;
        }
    }
    for (int i = 0; i < 18; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
    }
    for (int i = 0; i < 18; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));
    }

    gcc_jit_lvalue *lv1 = gcc_jit_function_new_local(fn, NULL, it, "c_lv1");
    gcc_jit_lvalue *lv2 = gcc_jit_function_new_local(fn, NULL, it, "c_lv2");
    gcc_jit_lvalue *lv3 = gcc_jit_function_new_local(fn, NULL, it, "c_lv3");

    gcc_jit_function *dtor0 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, vt, "dt0", 0, NULL, 0);
    gcc_jit_param *dp0 = gcc_jit_context_new_param(ctx, NULL, vpt, "self");
    gcc_jit_function *dtor1 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, vt, "dt1", 1, &dp0, 0);
    gcc_jit_param *dp2[2] = {
        gcc_jit_context_new_param(ctx, NULL, vpt, "self"),
        gcc_jit_context_new_param(ctx, NULL, it, "vtt")
    };
    gcc_jit_function *dtor2 = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, vt, "dt2", 2, dp2, 0);

    an_object_lifetime life_a = {}, life_b = {};
    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(&life_a));
    GCC_GEN_BE_CHECK(gcc_gen_be_has_scope_unwind_lp(&has_lp));
    if (has_lp != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_record_scope_unwind_lp());
    GCC_GEN_BE_CHECK(gcc_gen_be_record_scope_unwind_lp()); /* 2nd lp count with only 1 on stack -> tests both branches! */
    GCC_GEN_BE_CHECK(gcc_gen_be_has_scope_unwind_lp(&has_lp));
    if (has_lp != TRUE) return GCC_GEN_BE_ERROR_INTERNAL;

    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_register_cleanup(lv1, dtor0) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;

    GCC_GEN_BE_CHECK(gcc_gen_be_register_cleanup(lv1, dtor0));
    GCC_GEN_BE_CHECK(gcc_gen_be_register_cleanup(lv1, dtor0)); /* duplicate ignored */
    GCC_GEN_BE_CHECK(gcc_gen_be_register_cleanup(lv1, dtor1)); /* same var, different dtor_func */
    GCC_GEN_BE_CHECK(gcc_gen_be_register_cleanup(lv2, dtor1));
    GCC_GEN_BE_CHECK(gcc_gen_be_register_cleanup(lv3, dtor2));

    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_register_cleanup(lv1, dtor0));
    GCC_GEN_BE_CHECK(gcc_gen_be_register_cleanup(lv2, dtor1));
    GCC_GEN_BE_CHECK(gcc_gen_be_register_cleanup(lv3, dtor2));

    GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(NULL, NULL));
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_emit_cleanups(NULL, blk) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;

    GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(NULL, blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(&life_a, blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(&life_b, blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups((an_object_lifetime_ptr)(uintptr_t)1, blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups((an_object_lifetime_ptr)(uintptr_t)2, blk));

    /* Also test target_lifetime == 2 when enclosing scope has pushed_lp_count == 0 */
    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(&life_b));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups((an_object_lifetime_ptr)(uintptr_t)2, blk));
    st->has_primary_il_scope = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(blk)); /* lifetime != NULL && has_primary_il_scope == TRUE */
    st->has_primary_il_scope = FALSE;

    /* Pop scope with OOM sweep (exercises pop_landing_pad OOM, get_context OOM, cache_insert OOM) */
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(blk)); /* NULL lifetime scope emits all 3 dtors (0, 1, 2) */
    /* Pop life_a (lifetime != NULL && has_primary_il_scope == FALSE, and pushed_lp_count == 2 with only 1 LP on stack!) */
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(blk));

    /* Test pop_scope when ctx == NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    for (int b = 0; b < 8; ++b) {
        while (st->scope_stack_size > 0) {
            GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));
        }
        while (st->landing_pad_stack_size > 0) {
            GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
        GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(blk));
        GCC_GEN_BE_CHECK(gcc_gen_be_record_scope_unwind_lp());
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_pop_scope(blk);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* 6. gcc_gen_be_is_scope_anonymous */
    a_boolean is_anon = FALSE;
    if (gcc_gen_be_is_scope_anonymous(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_scope_anonymous(NULL, &is_anon));
    if (is_anon != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;

    a_namespace ns_named = {}, ns_internal = {}, ns_noname = {}, ns_empty = {};
    ns_named.source_corresp.name = "pub_ns";
    ns_internal.source_corresp.name = "int_ns";
    ns_internal.has_internal_linkage = TRUE;
    ns_noname.source_corresp.name = NULL;
    ns_empty.source_corresp.name = "";

    a_scope sc_root = {}, sc_ns_null = {}, sc_ns_named = {}, sc_ns_int = {}, sc_ns_noname = {}, sc_ns_empty = {}, sc_fn = {}, sc_blk = {};
    sc_root.kind = sck_file;
    sc_ns_null.kind = sck_namespace; sc_ns_null.variant.assoc_namespace = NULL; sc_ns_null.parent = &sc_root;
    sc_ns_named.kind = sck_namespace; sc_ns_named.variant.assoc_namespace = &ns_named; sc_ns_named.parent = &sc_ns_null;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_scope_anonymous(&sc_ns_named, &is_anon));
    if (is_anon != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;

    sc_ns_int.kind = sck_namespace; sc_ns_int.variant.assoc_namespace = &ns_internal;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_scope_anonymous(&sc_ns_int, &is_anon));
    sc_ns_noname.kind = sck_namespace; sc_ns_noname.variant.assoc_namespace = &ns_noname;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_scope_anonymous(&sc_ns_noname, &is_anon));
    sc_ns_empty.kind = sck_namespace; sc_ns_empty.variant.assoc_namespace = &ns_empty;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_scope_anonymous(&sc_ns_empty, &is_anon));
    sc_fn.kind = sck_function;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_scope_anonymous(&sc_fn, &is_anon));
    sc_blk.kind = sck_block;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_scope_anonymous(&sc_blk, &is_anon));

    /* 7. Vague linkage (attribute path & top-level asm fallback + deduplication + OOM sweeps) */
    gcc_jit_lvalue *gvar = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_EXPORTED, it, "weak_gv");
    if (gcc_gen_be_mark_global_vague_linkage(NULL, gvar, "weak_gv") != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mark_global_vague_linkage(ctx, NULL, "weak_gv") != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mark_global_vague_linkage(ctx, gvar, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mark_global_vague_linkage(ctx, gvar, "") != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_mark_function_vague_linkage(NULL, fn, "f2") != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mark_function_vague_linkage(ctx, NULL, "f2") != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mark_function_vague_linkage(ctx, fn, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_mark_function_vague_linkage(ctx, fn, "") != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    p_gcc_jit_lvalue_add_attribute = &mock_var_attr;
    p_gcc_jit_function_add_attribute = &mock_fn_attr;
    GCC_GEN_BE_CHECK(gcc_gen_be_mark_global_vague_linkage(ctx, gvar, "weak_gv"));
    GCC_GEN_BE_CHECK(gcc_gen_be_mark_function_vague_linkage(ctx, fn, "f2"));

    p_gcc_jit_lvalue_add_attribute = NULL;
    p_gcc_jit_function_add_attribute = NULL;
    p_gcc_jit_context_add_top_level_asm = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mark_global_vague_linkage(ctx, gvar, "weak_no_asm"));
    GCC_GEN_BE_CHECK(gcc_gen_be_mark_global_vague_linkage(ctx, gvar, "weak_no_asm")); /* cache hit! */

    p_gcc_jit_context_add_top_level_asm = &mock_top_asm;
    for (int b = 0; b < 15; ++b) {
        GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_STRING));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_mark_function_vague_linkage(ctx, fn, "weak_with_asm");
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* 8. Cover gcc_gen_be_prepend_cleanup (null args, empty scope stack, OOM, and non-empty prepend) + driver_ldflags in configure_options */
    if (gcc_gen_be_prepend_cleanup(NULL, fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_prepend_cleanup(gvar, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_prepend_cleanup(gvar, fn) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_prepend_cleanup(gvar, fn) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;
    GCC_GEN_BE_CHECK(gcc_gen_be_prepend_cleanup(gvar, fn));

    char *ldflags_raw[7] = {
        NULL,
        strdup(""),
        strdup("-fuse-ld=lld"),
        strdup("-F/System/Library/Frameworks"),
        strdup("-Wl,-v"),
        strdup("-Xlinker"),
        strdup("--custom-ctx-flag")
    };
    void *raw_arr = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(sizeof(ldflags_raw), &raw_arr));
    memcpy(raw_arr, ldflags_raw, sizeof(ldflags_raw));
    gcc_gen_be_singleton_state.driver_ldflags = static_cast<char **>(raw_arr);
    gcc_gen_be_singleton_state.driver_ldflags_count = 7;
    GCC_GEN_BE_CHECK(gcc_gen_be_context_configure_options(ctx));

    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes the context lifecycle unit tests.
 * @details Runs `test_options_and_init_cleanup` and `test_stacks_scopes_and_linkage`.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_context_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    GCC_GEN_BE_CHECK(test_options_and_init_cleanup());
    GCC_GEN_BE_CHECK(test_stacks_scopes_and_linkage());
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for context tests.
 * @details Invokes `run_context_tests` and prints PASS on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_context_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    puts("PASS");
    return 0;
}
