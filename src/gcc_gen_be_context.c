/**
 * @file gcc_gen_be_context.c
 * @brief Implementation of context and core state management.
 * @details Manages libgccjit context initialization, option propagation, control-flow stacks, and cleanup.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "fe_common.h"
#include <libgccjit.h>
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_lib_loader.h"
#include "cmd_line.h"
#ifdef _WIN32
#if !defined(_AMD64_) && (defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__))
#define _AMD64_
#elif !defined(_ARM64_) && (defined(_M_ARM64) || defined(__aarch64__))
#define _ARM64_
#elif !defined(_X86_) && (defined(_M_IX86) || defined(__i386__))
#define _X86_
#elif !defined(_ARM_) && (defined(_M_ARM) || defined(__arm__))
#define _ARM_
#endif
#include <windef.h>
#include <processthreadsapi.h>
#else
#include <pthread.h>
#include <stdint.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

static gcc_gen_be_error_t get_current_thread_id(unsigned long *out_tid) GCC_GEN_BE_NOEXCEPT;

/** @brief Global libgccjit context pointer. */
static gcc_jit_context *gcc_jit_ctx = NULL;

/** @brief The currently active libgccjit block. */
static gcc_jit_block *current_block = NULL;

/** @brief Global tracking state for the GCC backend context. */
gcc_gen_be_context_t gcc_gen_be_singleton_state = {0};
/** @brief Alias for the singleton backend state inside `gcc_gen_be_context.c`. */
#define backend_state gcc_gen_be_singleton_state

/**
 * @brief Retrieves the global GCC backend context object.
 * @details Populates out_state with the address of the singleton backend_state structure.
 * @param[out] out_state Pointer to receive the backend state object.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_get_state(gcc_gen_be_context_t **out_state) GCC_GEN_BE_NOEXCEPT {
    if (!out_state) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_state = &backend_state;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves the currently active libgccjit context.
 * @details Populates out_ctx with the active gcc_jit_context pointer.
 * @param[out] out_ctx Pointer to receive the gcc_jit_context pointer.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_get_context(gcc_jit_context **out_ctx) GCC_GEN_BE_NOEXCEPT {
    if (!out_ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_ctx = gcc_jit_ctx;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Sets the currently active libgccjit context.
 * @details Updates the global gcc_jit_ctx pointer.
 * @param[in] ctx The gcc_jit_context to set as active.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_set_context(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_ctx = ctx;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves the currently active libgccjit block.
 * @details Populates out_block with the active gcc_jit_block pointer.
 * @param[out] out_block Pointer to receive the gcc_jit_block pointer.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_get_current_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_block = current_block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Sets the currently active libgccjit block.
 * @details Updates the global current_block pointer.
 * @param[in] block The gcc_jit_block to set as active.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_set_current_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    current_block = block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Configures libgccjit context based on command line options.
 *
 * Applies optimization level, debug info flags, and other options
 * parsed from the EDG command line.
 *
 * @param ctx The gcc_jit_context to configure.
 * @return GCC_GEN_BE_SUCCESS on success, or an appropriate error code.
 */
gcc_gen_be_error_t gcc_gen_be_context_configure_options(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    if (!ctx) return GCC_GEN_BE_ERROR_NULL_POINTER;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());

    /* Set optimization level */
    gcc_jit_context_set_int_option(ctx, GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, gcc_be_opt_level);

    /* Set debug info */
    gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DEBUGINFO, gcc_be_debug_info ? 1 : 0);

    /* Set dump options */
    gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DUMP_INITIAL_TREE, gcc_be_dump_initial_tree ? 1 : 0);
    gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DUMP_INITIAL_GIMPLE, gcc_be_dump_gimple ? 1 : 0);

    /* Position independent code is not exposed as a direct jit option, 
       but can be added via command line arguments to the driver. */
    
    if (gcc_be_dump_rtl_all) {
#ifdef GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE
        gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE, 1);
#endif
    }
    
    
    if (exceptions_enabled) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_command_line_option
        gcc_jit_context_add_command_line_option(ctx, "-fexceptions");
        gcc_jit_context_add_command_line_option(ctx, "-funwind-tables");
        gcc_jit_context_add_command_line_option(ctx, "-fasynchronous-unwind-tables");
#endif
    }

    if (gcc_be_ffast_math) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_command_line_option
        gcc_jit_context_add_command_line_option(ctx, "-ffast-math");
#endif
    }
    
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_command_line_option
    /* Optimization levels */
    if (backend_state.opt_size_level >= 2) gcc_jit_context_add_command_line_option(ctx, "-Oz");
    else if (backend_state.opt_size_level == 1) gcc_jit_context_add_command_line_option(ctx, "-Os");
    else if (gcc_be_opt_level == 3) gcc_jit_context_add_command_line_option(ctx, "-O3");
    else if (gcc_be_opt_level == 2) gcc_jit_context_add_command_line_option(ctx, "-O2");
    else if (gcc_be_opt_level == 1) gcc_jit_context_add_command_line_option(ctx, "-O1");
    else if (gcc_be_opt_level == 0) gcc_jit_context_add_command_line_option(ctx, "-O0");
    
    /* Target tuning */
    if (gcc_be_march) { char buf[128]; snprintf(buf, sizeof(buf), "-march=%s", gcc_be_march); gcc_jit_context_add_command_line_option(ctx, buf); }
    if (gcc_be_mcpu) { char buf[128]; snprintf(buf, sizeof(buf), "-mcpu=%s", gcc_be_mcpu); gcc_jit_context_add_command_line_option(ctx, buf); }
    if (gcc_be_mtune) { char buf[128]; snprintf(buf, sizeof(buf), "-mtune=%s", gcc_be_mtune); gcc_jit_context_add_command_line_option(ctx, buf); }
    if (gcc_be_mfpu) { char buf[128]; snprintf(buf, sizeof(buf), "-mfpu=%s", gcc_be_mfpu); gcc_jit_context_add_command_line_option(ctx, buf); }

    /* Code models */
    if (gcc_be_mcmodel) { char buf[128]; snprintf(buf, sizeof(buf), "-mcmodel=%s", gcc_be_mcmodel); gcc_jit_context_add_command_line_option(ctx, buf); }

    /* -fPIC / -fPIE */
    if (gcc_be_fPIC) gcc_jit_context_add_command_line_option(ctx, "-fPIC");
    if (gcc_be_fPIE) gcc_jit_context_add_command_line_option(ctx, "-fPIE");
    if (gcc_be_fno_pie) gcc_jit_context_add_command_line_option(ctx, "-fno-pie");

    if (backend_state.lto_mode != 0) gcc_jit_context_add_command_line_option(ctx, "-flto");
    if (backend_state.symbol_visibility == 1) gcc_jit_context_add_command_line_option(ctx, "-fvisibility=hidden");
    else if (backend_state.symbol_visibility == 2) gcc_jit_context_add_command_line_option(ctx, "-fvisibility=protected");
    else if (backend_state.symbol_visibility == 3) gcc_jit_context_add_command_line_option(ctx, "-fvisibility=internal");

    /* Disable GCC type-based strict-aliasing assumptions because backend lowering of unions,
       bitfields, and C++ subobjects accesses memory via reinterpreted pointer casts. */
    gcc_jit_context_add_command_line_option(ctx, "-fno-strict-aliasing");
#if defined(__APPLE__)
    gcc_jit_context_add_command_line_option(ctx, "-fno-common");
    gcc_jit_context_add_command_line_option(ctx, "-fno-zero-initialized-in-bss");
    gcc_jit_context_add_command_line_option(ctx, "-falign-functions=4");
    gcc_jit_context_add_command_line_option(ctx, "-falign-loops=4");
    gcc_jit_context_add_command_line_option(ctx, "-falign-jumps=4");
    gcc_jit_context_add_command_line_option(ctx, "-falign-labels=4");
#endif

    if (backend_state.function_sections) gcc_jit_context_add_command_line_option(ctx, "-ffunction-sections");
    if (backend_state.data_sections) gcc_jit_context_add_command_line_option(ctx, "-fdata-sections");
    if (backend_state.pthread_enabled) gcc_jit_context_add_command_line_option(ctx, "-pthread");

    if (backend_state.sanitizers & (1U << 0)) gcc_jit_context_add_command_line_option(ctx, "-fsanitize=address");
    if (backend_state.sanitizers & (1U << 1)) gcc_jit_context_add_command_line_option(ctx, "-fsanitize=undefined");
    if (backend_state.sanitizers & (1U << 2)) gcc_jit_context_add_command_line_option(ctx, "-fsanitize=thread");
    if (backend_state.sanitizers & (1U << 4)) gcc_jit_context_add_command_line_option(ctx, "-fsanitize=leak");

    if (gcc_be_fassociative_math) gcc_jit_context_add_command_line_option(ctx, "-fassociative-math");
    if (gcc_be_fno_trapping_math) gcc_jit_context_add_command_line_option(ctx, "-fno-trapping-math");

    if (gcc_be_m64) gcc_jit_context_add_command_line_option(ctx, "-m64");
    if (gcc_be_m32) gcc_jit_context_add_command_line_option(ctx, "-m32");

#endif

#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
    if (gcc_be_shared) gcc_jit_context_add_driver_option(ctx, "-shared");
    if (gcc_be_static) gcc_jit_context_add_driver_option(ctx, "-static");
    if (gcc_be_fPIE)   gcc_jit_context_add_driver_option(ctx, "-pie");
    if (backend_state.lto_mode != 0) gcc_jit_context_add_driver_option(ctx, "-flto");
    if (backend_state.openmp_enabled) {
        gcc_jit_context_add_driver_option(ctx, "-fopenmp");
        gcc_jit_context_add_driver_option(ctx, "-lgomp");
    }
    if (backend_state.pthread_enabled) gcc_jit_context_add_driver_option(ctx, "-pthread");
    if (backend_state.sanitizers & (1U << 0)) gcc_jit_context_add_driver_option(ctx, "-fsanitize=address");
    if (backend_state.sanitizers & (1U << 1)) gcc_jit_context_add_driver_option(ctx, "-fsanitize=undefined");
    if (backend_state.sanitizers & (1U << 2)) gcc_jit_context_add_driver_option(ctx, "-fsanitize=thread");
    if (backend_state.sanitizers & (1U << 4)) gcc_jit_context_add_driver_option(ctx, "-fsanitize=leak");

    for (size_t i = 0; i < backend_state.driver_lib_dirs_count; ++i) {
        if (backend_state.driver_lib_dirs[i]) {
            size_t lcap = strlen(backend_state.driver_lib_dirs[i]) + 8;
            void *raw_lopt = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_alloc(lcap, &raw_lopt));
            char *lopt = (char *)raw_lopt;
            snprintf(lopt, lcap, "-L%s", backend_state.driver_lib_dirs[i]);
            gcc_jit_context_add_driver_option(ctx, lopt);
            free(lopt);
        }
    }
    for (size_t i = 0; i < backend_state.driver_libs_count; ++i) {
        if (backend_state.driver_libs[i]) {
            size_t lcap = strlen(backend_state.driver_libs[i]) + 8;
            void *raw_lopt = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_alloc(lcap, &raw_lopt));
            char *lopt = (char *)raw_lopt;
            snprintf(lopt, lcap, "-l%s", backend_state.driver_libs[i]);
            gcc_jit_context_add_driver_option(ctx, lopt);
            free(lopt);
        }
    }
    for (size_t i = 0; i < backend_state.driver_ldflags_count; ++i) {
        const char *larg = backend_state.driver_ldflags[i];
        if (larg && larg[0] != '\0') {
            if (strncmp(larg, "-fuse-ld=", 9) == 0 ||
                strncmp(larg, "-F", 2) == 0 ||
                strncmp(larg, "-Wl,", 4) == 0 ||
                strcmp(larg, "-Xlinker") == 0) {
                gcc_jit_context_add_driver_option(ctx, larg);
            } else {
                gcc_jit_context_add_driver_option(ctx, "-Xlinker");
                gcc_jit_context_add_driver_option(ctx, larg);
            }
        }
    }
#endif
    
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
#if defined(__APPLE__) && defined(__MACH__)
    gcc_jit_context_add_driver_option(ctx, "-L/opt/homebrew/opt/gcc/lib/gcc/current");
#endif
#endif

    if (C_dialect == C_dialect_cplusplus) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
#if defined(__APPLE__) && defined(__MACH__)
        gcc_jit_context_add_driver_option(ctx, "-lc++");
#else
        gcc_jit_context_add_driver_option(ctx, "-lstdc++");
#endif
#endif
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Initializes the main gcc_jit_context and its dependencies.
 *
 * Acquires a new libgccjit context and configures it.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an appropriate error code.
 */
gcc_gen_be_error_t gcc_gen_be_init(void) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_ctx = (gcc_gen_be_step_oom_budget() == GCC_GEN_BE_SUCCESS)
        ? gcc_jit_context_acquire()
        : NULL;
    if (!gcc_jit_ctx) {
        return GCC_GEN_BE_ERROR_OOM;
    }
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_set_bool_allow_unreachable_blocks
    gcc_jit_context_set_bool_allow_unreachable_blocks(gcc_jit_ctx, 1);
#endif

    GCC_GEN_BE_CHECK(gcc_gen_be_context_configure_options(gcc_jit_ctx));

    GCC_GEN_BE_CHECK(get_current_thread_id(&backend_state.thread_id));

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Cleans up the gcc_jit_context and associated caches.
 *
 * Releases the main context and clears internal backend caches.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an appropriate error code.
 */
gcc_gen_be_error_t gcc_gen_be_cleanup(void) GCC_GEN_BE_NOEXCEPT {
    if (gcc_jit_ctx) {
        gcc_jit_context_release(gcc_jit_ctx);
        gcc_jit_ctx = NULL;
    }
    
    GCC_GEN_BE_CHECK(cache_clear_all());
    
    if (backend_state.break_stack) {
        free(backend_state.break_stack);
        backend_state.break_stack = NULL;
        backend_state.break_stack_size = 0;
        backend_state.break_stack_capacity = 0;
    }

    if (backend_state.continue_stack) {
        free(backend_state.continue_stack);
        backend_state.continue_stack = NULL;
        backend_state.continue_stack_size = 0;
        backend_state.continue_stack_capacity = 0;
    }

    if (backend_state.switch_exit_stack) {
        free(backend_state.switch_exit_stack);
        backend_state.switch_exit_stack = NULL;
        backend_state.switch_exit_stack_size = 0;
        backend_state.switch_exit_stack_capacity = 0;
    }

    if (backend_state.block_stack) {
        free(backend_state.block_stack);
        backend_state.block_stack = NULL;
        backend_state.block_stack_size = 0;
        backend_state.block_stack_capacity = 0;
    }

    if (backend_state.landing_pad_stack) {
        free(backend_state.landing_pad_stack);
        backend_state.landing_pad_stack = NULL;
        backend_state.landing_pad_stack_size = 0;
        backend_state.landing_pad_stack_capacity = 0;
    }

    if (backend_state.cleanup_stack) {
        free(backend_state.cleanup_stack);
        backend_state.cleanup_stack = NULL;
        backend_state.cleanup_stack_size = 0;
        backend_state.cleanup_stack_capacity = 0;
    }
    
    if (backend_state.scope_stack) {
        /* Any remaining scopes are leaked memory if not popped */
        for (size_t i = 0; i < backend_state.scope_stack_size; ++i) {
            free(backend_state.scope_stack[i]);
        }
        free(backend_state.scope_stack);
        backend_state.scope_stack = NULL;
        backend_state.scope_stack_size = 0;
        backend_state.scope_stack_capacity = 0;
    }

    backend_state.global_ctor_func = NULL;
    backend_state.global_ctor_block = NULL;
    backend_state.stmt_expr_result_lval = NULL;
    backend_state.computed_goto_dispatch_block = NULL;
    backend_state.computed_goto_target_lval = NULL;
    if (backend_state.computed_goto_labels) {
        free(backend_state.computed_goto_labels);
        backend_state.computed_goto_labels = NULL;
    }
    backend_state.computed_goto_labels_count = 0;
    backend_state.computed_goto_labels_capacity = 0;
    backend_state.active_seh_finally_stack = NULL;
    backend_state.active_coro_state_index = 0;
    backend_state.active_coro_frame_type = NULL;
    backend_state.active_coro_frame_lval = NULL;
    backend_state.active_coro_state_field = NULL;
    backend_state.active_coro_promise_field = NULL;
    backend_state.active_coro_descr = NULL;
    if (backend_state.active_coro_resume_blocks) {
        free(backend_state.active_coro_resume_blocks);
        backend_state.active_coro_resume_blocks = NULL;
    }
    if (backend_state.active_coro_suspend_scopes) {
        free(backend_state.active_coro_suspend_scopes);
        backend_state.active_coro_suspend_scopes = NULL;
    }
    backend_state.active_coro_current_scope = NULL;
    backend_state.active_coro_resume_blocks_count = 0;
    backend_state.active_coro_resume_blocks_capacity = 0;
    while (backend_state.coro_state_stack) {
        gcc_gen_be_coro_state_t *cs = backend_state.coro_state_stack;
        backend_state.coro_state_stack = cs->prev;
        if (cs->resume_blocks) free(cs->resume_blocks);
        if (cs->suspend_scopes) free(cs->suspend_scopes);
        free(cs);
    }
    while (backend_state.eh_state.regions) {
        gcc_gen_be_eh_region_t *r = backend_state.eh_state.regions;
        backend_state.eh_state.regions = r->next;
        free(r);
    }
    while (backend_state.eh_state.types) {
        gcc_gen_be_eh_type_filter_t *t = backend_state.eh_state.types;
        backend_state.eh_state.types = t->next;
        free(t);
    }
    if (backend_state.eh_state.action_table) {
        free(backend_state.eh_state.action_table);
        backend_state.eh_state.action_table = NULL;
    }
    backend_state.eh_state.action_table_len = 0;
    backend_state.eh_state.action_table_cap = 0;
    backend_state.eh_state.current_func = NULL;
    backend_state.eh_state_ptr = NULL;

    if (backend_state.active_subst_table.entries) {
        for (int i = 0; i < backend_state.active_subst_table.count; ++i) {
            if (backend_state.active_subst_table.entries[i]) {
                free(backend_state.active_subst_table.entries[i]);
            }
        }
        free(backend_state.active_subst_table.entries);
        backend_state.active_subst_table.entries = NULL;
    }
    backend_state.active_subst_table.count = 0;
    backend_state.active_subst_table.capacity = 0;

    if (backend_state.deferred_structs) {
        free(backend_state.deferred_structs);
        backend_state.deferred_structs = NULL;
    }
    backend_state.deferred_structs_count = 0;
    backend_state.deferred_structs_capacity = 0;

    if (backend_state.tu_dwarf_die_table) {
        free(backend_state.tu_dwarf_die_table);
        backend_state.tu_dwarf_die_table = NULL;
    }
    backend_state.tu_dwarf_die_count = 0;
    backend_state.tu_dwarf_die_capacity = 0;

    if (backend_state.visited_scopes) {
        free(backend_state.visited_scopes);
        backend_state.visited_scopes = NULL;
    }
    backend_state.visited_scopes_count = 0;
    backend_state.visited_scopes_capacity = 0;
    backend_state.has_primary_il_scope = FALSE;
    backend_state.needs_libatomic = FALSE;
    backend_state.active_routine_name = NULL;
    backend_state.active_thrown_obj_lval = NULL;
    backend_state.active_caught_obj_lval = NULL;
    if (backend_state.omp_pragma_entries) {
        free(backend_state.omp_pragma_entries);
        backend_state.omp_pragma_entries = NULL;
    }
    backend_state.omp_pragma_count = 0;
    backend_state.omp_pragma_capacity = 0;
    backend_state.omp_outlined_counter = 0;

    if (backend_state.driver_lib_dirs) {
        for (size_t i = 0; i < backend_state.driver_lib_dirs_count; ++i) {
            if (backend_state.driver_lib_dirs[i]) free(backend_state.driver_lib_dirs[i]);
        }
        free(backend_state.driver_lib_dirs);
        backend_state.driver_lib_dirs = NULL;
    }
    backend_state.driver_lib_dirs_count = 0;

    if (backend_state.driver_libs) {
        for (size_t i = 0; i < backend_state.driver_libs_count; ++i) {
            if (backend_state.driver_libs[i]) free(backend_state.driver_libs[i]);
        }
        free(backend_state.driver_libs);
        backend_state.driver_libs = NULL;
    }
    backend_state.driver_libs_count = 0;

    if (backend_state.driver_ldflags) {
        for (size_t i = 0; i < backend_state.driver_ldflags_count; ++i) {
            if (backend_state.driver_ldflags[i]) free(backend_state.driver_ldflags[i]);
        }
        free(backend_state.driver_ldflags);
        backend_state.driver_ldflags = NULL;
    }
    backend_state.driver_ldflags_count = 0;
    
    return GCC_GEN_BE_SUCCESS;
}





/**
 * @brief Gets the current thread ID.
 * @details Queries the OS thread identifier and writes it to out_tid.
 * @param[out] out_tid Pointer to receive the thread ID as an unsigned long.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if out_tid is NULL.
 */
static gcc_gen_be_error_t get_current_thread_id(unsigned long *out_tid) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
#ifdef _WIN32
    *out_tid = (unsigned long)GetCurrentThreadId();
#elif defined(__APPLE__)
    uint64_t tid = 0;
    pthread_threadid_np(NULL, &tid);
    *out_tid = (unsigned long)tid;
#else
    *out_tid = (unsigned long)pthread_self();
#endif
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Helper to push to a generic block stack.
 *
 * @details Resizes the given stack if necessary and appends the specified block.
 *
 * @param[in,out] stack Double pointer to the dynamically allocated array of block pointers.
 * @param[in,out] size Pointer to the current number of elements in the stack.
 * @param[in,out] capacity Pointer to the current capacity of the stack.
 * @param[in] block The gcc_jit_block to push onto the stack.
 * @return GCC_GEN_BE_SUCCESS on success, GCC_GEN_BE_ERROR_INVALID_ARGUMENT if block is null, or GCC_GEN_BE_ERROR_OOM.
 */
static gcc_gen_be_error_t push_to_block_stack(gcc_jit_block ***stack, size_t *size, size_t *capacity, gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    if (!block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (*size >= *capacity) {
        size_t new_cap = (*capacity == 0) ? 8 : (*capacity * 2);
        void *new_stack = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_realloc_buf(*stack, new_cap * sizeof(gcc_jit_block *), &new_stack));
        *stack = (gcc_jit_block **)new_stack;
        *capacity = new_cap;
    }
    (*stack)[(*size)++] = block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Helper to pop from a generic block stack.
 *
 * @details Removes the top element from the stack and returns it.
 *
 * @param[in,out] stack Double pointer to the dynamically allocated array of block pointers.
 * @param[in,out] size Pointer to the current number of elements in the stack.
 * @param[out] out_block Pointer to receive the popped gcc_jit_block.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT on underflow or null pointers.
 */
static gcc_gen_be_error_t pop_from_block_stack(gcc_jit_block ***stack, size_t *size, gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (*size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT; // Stack underflow
    *out_block = (*stack)[--(*size)];
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_push_break_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    return push_to_block_stack(&backend_state.break_stack, &backend_state.break_stack_size, &backend_state.break_stack_capacity, block);
}

gcc_gen_be_error_t gcc_gen_be_pop_break_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    return pop_from_block_stack(&backend_state.break_stack, &backend_state.break_stack_size, out_block);
}

gcc_gen_be_error_t gcc_gen_be_push_continue_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    return push_to_block_stack(&backend_state.continue_stack, &backend_state.continue_stack_size, &backend_state.continue_stack_capacity, block);
}

gcc_gen_be_error_t gcc_gen_be_pop_continue_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    return pop_from_block_stack(&backend_state.continue_stack, &backend_state.continue_stack_size, out_block);
}

gcc_gen_be_error_t gcc_gen_be_push_switch_exit_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    return push_to_block_stack(&backend_state.switch_exit_stack, &backend_state.switch_exit_stack_size, &backend_state.switch_exit_stack_capacity, block);
}

gcc_gen_be_error_t gcc_gen_be_pop_switch_exit_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    return pop_from_block_stack(&backend_state.switch_exit_stack, &backend_state.switch_exit_stack_size, out_block);
}

gcc_gen_be_error_t gcc_gen_be_push_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    return push_to_block_stack(&backend_state.block_stack, &backend_state.block_stack_size, &backend_state.block_stack_capacity, block);
}

gcc_gen_be_error_t gcc_gen_be_pop_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    return pop_from_block_stack(&backend_state.block_stack, &backend_state.block_stack_size, out_block);
}

gcc_gen_be_error_t gcc_gen_be_check_thread_safety(void) GCC_GEN_BE_NOEXCEPT {
    unsigned long current_tid = 0;
    GCC_GEN_BE_CHECK(get_current_thread_id(&current_tid));
    if (backend_state.thread_id != current_tid) {
        return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_INTERNAL, "Thread safety violation in gcc_gen_be context");
    }
    return GCC_GEN_BE_SUCCESS;
}

/** @brief A cleanup entry */
typedef struct {
    /** @brief Cleanup callback function. */
    gcc_gen_be_cleanup_func_t func;
    /** @brief Opaque data pointer for callback. */
    void *data;
} gcc_gen_be_cleanup_entry_t;

/**
 * @brief Pushes a cleanup function to the RAII stack.
 * @details Registers a callback to be invoked during cleanup execution.
 * @param[in] func The function to execute during cleanup.
 * @param[in] data Data pointer to pass to the cleanup function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_push_cleanup(gcc_gen_be_cleanup_func_t func, void *data) GCC_GEN_BE_NOEXCEPT {
    if (!func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    if (backend_state.cleanup_stack_size >= backend_state.cleanup_stack_capacity) {
        size_t new_cap = (backend_state.cleanup_stack_capacity == 0) ? 16 : (backend_state.cleanup_stack_capacity * 2);
        void *new_stack = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_realloc_buf(
            backend_state.cleanup_stack, new_cap * sizeof(gcc_gen_be_cleanup_entry_t), &new_stack));
        backend_state.cleanup_stack = (void **)new_stack;
        backend_state.cleanup_stack_capacity = new_cap;
    }

    gcc_gen_be_cleanup_entry_t *entries = (gcc_gen_be_cleanup_entry_t *)backend_state.cleanup_stack;
    entries[backend_state.cleanup_stack_size].func = func;
    entries[backend_state.cleanup_stack_size].data = data;
    backend_state.cleanup_stack_size++;

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes all pending cleanups in reverse order and clears the stack.
 * @details Pops each registered cleanup entry and percolates any error returned by the callback.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code from a cleanup callback.
 */
gcc_gen_be_error_t gcc_gen_be_execute_cleanups(void) GCC_GEN_BE_NOEXCEPT {
    gcc_gen_be_cleanup_entry_t *entries = (gcc_gen_be_cleanup_entry_t *)backend_state.cleanup_stack;
    while (backend_state.cleanup_stack_size > 0) {
        backend_state.cleanup_stack_size--;
        GCC_GEN_BE_CHECK(entries[backend_state.cleanup_stack_size].func(entries[backend_state.cleanup_stack_size].data));
    }
    return GCC_GEN_BE_SUCCESS;
}

/** @brief A scope entry */
/**
 * @brief Represents a single registered cleanup.
 */
typedef struct gcc_gen_be_cleanup {
    /** @brief The variable to clean up. */
    struct gcc_jit_lvalue *var;
    /** @brief The destructor function. */
    struct gcc_jit_function *dtor_func;
    /** @brief Next cleanup in list. */
    struct gcc_gen_be_cleanup *next;
} gcc_gen_be_cleanup_t;

/**
 * @brief Represents a block scope with cleanups.
 */
typedef struct gcc_gen_be_scope {
    /** @brief The frontend lifetime object. */
    an_object_lifetime_ptr lifetime;
    /** @brief List of cleanups registered in this scope. */
    gcc_gen_be_cleanup_t *cleanups;
    /** @brief Number of unwind landing pads pushed onto `landing_pad_stack` by this scope. */
    int pushed_lp_count;
} gcc_gen_be_scope_t;

gcc_gen_be_error_t gcc_gen_be_push_scope(an_object_lifetime_ptr lifetime) GCC_GEN_BE_NOEXCEPT {
    if (backend_state.scope_stack_size >= backend_state.scope_stack_capacity) {
        size_t new_cap = (backend_state.scope_stack_capacity == 0) ? 16 : (backend_state.scope_stack_capacity * 2);
        void *new_stack = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_realloc_buf(
            backend_state.scope_stack, new_cap * sizeof(gcc_gen_be_scope_t *), &new_stack));
        backend_state.scope_stack = (struct gcc_gen_be_scope **)new_stack;
        backend_state.scope_stack_capacity = new_cap;
    }
    
    void *raw_scope = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(sizeof(gcc_gen_be_scope_t), &raw_scope));
    gcc_gen_be_scope_t *new_scope = (gcc_gen_be_scope_t *)raw_scope;
    new_scope->lifetime = lifetime;
    new_scope->cleanups = NULL;
    new_scope->pushed_lp_count = 0;
    
    backend_state.scope_stack[backend_state.scope_stack_size++] = (struct gcc_gen_be_scope *)new_scope;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Registers a cleanup routine for a local variable.
 * @details Adds the specified destructor function to the current scope's cleanup list.
 * @param[in] var The variable to clean up.
 * @param[in] dtor_func The destructor function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_register_cleanup(struct gcc_jit_lvalue *var, struct gcc_jit_function *dtor_func) GCC_GEN_BE_NOEXCEPT {
    if (backend_state.scope_stack_size == 0) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_gen_be_scope_t *top_scope = (gcc_gen_be_scope_t *)backend_state.scope_stack[backend_state.scope_stack_size - 1];
    
    for (gcc_gen_be_cleanup_t *existing = top_scope->cleanups; existing != NULL; existing = existing->next) {
        if (existing->var == var && existing->dtor_func == dtor_func) {
            return GCC_GEN_BE_SUCCESS;
        }
    }

    void *raw_cleanup = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(sizeof(gcc_gen_be_cleanup_t), &raw_cleanup));
    gcc_gen_be_cleanup_t *cleanup = (gcc_gen_be_cleanup_t *)raw_cleanup;
    
    cleanup->var = var;
    cleanup->dtor_func = dtor_func;
    cleanup->next = NULL;
    if (!top_scope->cleanups) {
        top_scope->cleanups = cleanup;
    } else {
        gcc_gen_be_cleanup_t *tail = top_scope->cleanups;
        while (tail->next) tail = tail->next;
        tail->next = cleanup;
    }
    
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_prepend_cleanup(struct gcc_jit_lvalue *var, struct gcc_jit_function *dtor_func) GCC_GEN_BE_NOEXCEPT {
    if (!var || !dtor_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (backend_state.scope_stack_size == 0) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }
    gcc_gen_be_scope_t *top_scope = backend_state.scope_stack[backend_state.scope_stack_size - 1];
    void *raw_cleanup = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(sizeof(gcc_gen_be_cleanup_t), &raw_cleanup));
    gcc_gen_be_cleanup_t *cleanup = (gcc_gen_be_cleanup_t *)raw_cleanup;
    cleanup->var = var;
    cleanup->dtor_func = dtor_func;
    cleanup->next = top_scope->cleanups;
    top_scope->cleanups = cleanup;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits registered cleanups up to a target lifetime.
 * @details Emits destructor calls for all variables in scopes up to the target lifetime.
 * @param[in] target_lifetime The lifetime to stop emitting cleanups for (NULL for all).
 * @param[in] block The block to emit cleanups into.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_cleanups(an_object_lifetime_ptr target_lifetime, struct gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    if (!block) return GCC_GEN_BE_SUCCESS; /* Nothing to emit into */

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    /* Traverse from top down to (but not including) the scope with target_lifetime */
    for (size_t i = backend_state.scope_stack_size; i > 0; --i) {
        gcc_gen_be_scope_t *scope = (gcc_gen_be_scope_t *)backend_state.scope_stack[i - 1];
        
        /* If we hit the target lifetime or its ancestor, we stop. 
           Wait, a target_lifetime might not exactly match our scope's lifetime pointer if EDG creates
           multiple overlapping ones. But typically, the innermost lifetime in goto/break EXACTLY matches
           one of the active block scopes. */
        /* To be safe, we also check if we reached the target. */
        
        if (target_lifetime == (an_object_lifetime_ptr)(uintptr_t)2) {
            if (i < backend_state.scope_stack_size && scope->pushed_lp_count > 0) {
                break;
            }
        } else if (target_lifetime != NULL &&
            target_lifetime != (an_object_lifetime_ptr)(uintptr_t)1 &&
            scope->lifetime == target_lifetime) {
            break;
        }
        
#if !defined(EDG_GCC_BE_ISOLATED_TEST) && DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
        if (!C_mode() && target_lifetime == (an_object_lifetime_ptr)(uintptr_t)1 && scope->lifetime != NULL) {
            /* Normal stmk_return / lowered goto: EDG IL lowering already emitted explicit destructor calls */
            continue;
        }
#endif
        gcc_gen_be_cleanup_t *cleanup = scope->cleanups;
        while (cleanup) {
            int pcount = (int)gcc_jit_function_get_param_count(cleanup->dtor_func);
            gcc_jit_rvalue *var_addr = gcc_jit_lvalue_get_address(cleanup->var, NULL);
            if (pcount >= 2) {
                gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(cleanup->dtor_func, 0)));
                gcc_jit_type *p1_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(cleanup->dtor_func, 1)));
                gcc_jit_rvalue *d_args[2];
                d_args[0] = gcc_jit_context_new_cast(ctx, NULL, var_addr, p0_t);
                d_args[1] = gcc_jit_context_new_rvalue_from_int(ctx, p1_t, 2);
                gcc_jit_block_add_eval(block, NULL, gcc_jit_context_new_call(ctx, NULL, cleanup->dtor_func, 2, d_args));
            } else if (pcount == 1) {
                gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(cleanup->dtor_func, 0)));
                gcc_jit_rvalue *arg = gcc_jit_context_new_cast(ctx, NULL, var_addr, p0_t);
                gcc_jit_block_add_eval(block, NULL, gcc_jit_context_new_call(ctx, NULL, cleanup->dtor_func, 1, &arg));
            } else {
                gcc_jit_block_add_eval(block, NULL, gcc_jit_context_new_call(ctx, NULL, cleanup->dtor_func, 0, NULL));
            }
            cleanup = cleanup->next;
        }
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Pops the current cleanup scope.
 * @details Emits pending cleanups when `block` is provided, pops any scope-owned landing pads, and frees scope nodes.
 * @param[in] block The block to emit cleanups into before popping (or NULL).
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_pop_scope(struct gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    if (backend_state.scope_stack_size == 0) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT; /* Underflow */
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    
    gcc_gen_be_scope_t *top_scope = (gcc_gen_be_scope_t *)backend_state.scope_stack[backend_state.scope_stack_size - 1];
    while (top_scope->pushed_lp_count > 0) {
        if (backend_state.landing_pad_stack_size > 0) {
            GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
        }
        top_scope->pushed_lp_count--;
    }
    
    /* Emit cleanups into block if provided */
    if (block) {
        gcc_jit_context *ctx = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
        if (ctx) {
            gcc_gen_be_cleanup_t *cleanup = top_scope->cleanups;
#if !defined(EDG_GCC_BE_ISOLATED_TEST) && DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
            if (!C_mode() && top_scope->lifetime != NULL && backend_state.has_primary_il_scope) {
                cleanup = NULL;
            }
#endif
            while (cleanup) {
                int pcount = (int)gcc_jit_function_get_param_count(cleanup->dtor_func);
                gcc_jit_rvalue *var_addr = gcc_jit_lvalue_get_address(cleanup->var, NULL);
                if (pcount >= 2) {
                    gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(cleanup->dtor_func, 0)));
                    gcc_jit_type *p1_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(cleanup->dtor_func, 1)));
                    gcc_jit_rvalue *d_args[2];
                    d_args[0] = gcc_jit_context_new_cast(ctx, NULL, var_addr, p0_t);
                    d_args[1] = gcc_jit_context_new_rvalue_from_int(ctx, p1_t, 2);
                    gcc_jit_block_add_eval(block, NULL, gcc_jit_context_new_call(ctx, NULL, cleanup->dtor_func, 2, d_args));
                } else if (pcount == 1) {
                    gcc_jit_type *p0_t = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(cleanup->dtor_func, 0)));
                    gcc_jit_rvalue *arg = gcc_jit_context_new_cast(ctx, NULL, var_addr, p0_t);
                    gcc_jit_block_add_eval(block, NULL, gcc_jit_context_new_call(ctx, NULL, cleanup->dtor_func, 1, &arg));
                } else {
                    gcc_jit_block_add_eval(block, NULL, gcc_jit_context_new_call(ctx, NULL, cleanup->dtor_func, 0, NULL));
                }
                cleanup = cleanup->next;
            }
        }
    }
    
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_GUARD, top_scope, NULL));
    gcc_gen_be_cleanup_t *cleanup = top_scope->cleanups;
    while (cleanup) {
        gcc_gen_be_cleanup_t *next = cleanup->next;
        free(cleanup);
        cleanup = next;
    }
    
    backend_state.scope_stack_size--;
    free(top_scope);
    
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_record_scope_unwind_lp(void) GCC_GEN_BE_NOEXCEPT {
    if (backend_state.scope_stack_size == 0) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_gen_be_scope_t *top_scope =
        (gcc_gen_be_scope_t *)backend_state.scope_stack[backend_state.scope_stack_size - 1];
    top_scope->pushed_lp_count++;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_has_scope_unwind_lp(a_boolean *out_has_lp) GCC_GEN_BE_NOEXCEPT {
    if (!out_has_lp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_has_lp = FALSE;
    if (backend_state.scope_stack_size > 0) {
        gcc_gen_be_scope_t *top_scope =
            (gcc_gen_be_scope_t *)backend_state.scope_stack[backend_state.scope_stack_size - 1];
        if (top_scope->pushed_lp_count > 0) {
            *out_has_lp = TRUE;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Pushes an exception landing pad block onto the active landing pad stack.
 * @details Used for nested `try` blocks and cleanup landing pads.
 * @param[in] lp_block The landing pad `gcc_jit_block` to push.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_push_landing_pad(gcc_jit_block *lp_block) GCC_GEN_BE_NOEXCEPT {
    if (!lp_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (backend_state.landing_pad_stack_size >= backend_state.landing_pad_stack_capacity) {
        size_t new_cap = (backend_state.landing_pad_stack_capacity == 0) ? 16 : (backend_state.landing_pad_stack_capacity * 2);
        void *new_stack = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_realloc_buf(
            backend_state.landing_pad_stack, new_cap * sizeof(gcc_jit_block *), &new_stack));
        backend_state.landing_pad_stack = (gcc_jit_block **)new_stack;
        backend_state.landing_pad_stack_capacity = new_cap;
    }
    backend_state.landing_pad_stack[backend_state.landing_pad_stack_size++] = lp_block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Pops the top exception landing pad block from the active landing pad stack.
 * @details Restores the enclosing landing pad after leaving a `try` or cleanup region.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code if the stack is empty.
 */
gcc_gen_be_error_t gcc_gen_be_pop_landing_pad(void) GCC_GEN_BE_NOEXCEPT {
    if (backend_state.landing_pad_stack_size == 0) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    backend_state.landing_pad_stack_size--;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves the currently active exception landing pad block, if any.
 * @details Outputs the top block on the landing pad stack or `NULL` if none is active.
 * @param[out] out_lp_block Pointer to receive the active landing pad block.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_active_landing_pad(gcc_jit_block **out_lp_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_lp_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (backend_state.landing_pad_stack_size == 0) {
        *out_lp_block = NULL;
    } else {
        *out_lp_block = backend_state.landing_pad_stack[backend_state.landing_pad_stack_size - 1];
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a deduplicated top-level assembler weak symbol directive for `symbol_name`.
 * @details Formats `.weak_definition _<sym>` on Mach-O or `.weak <sym>` on ELF/MinGW and registers
 * a sentinel key in `GCC_GEN_BE_CACHE_STRING` so each symbol is emitted at most once per TU.
 * @param[in,out] ctx The active `gcc_jit_context`.
 * @param[in] symbol_name Non-empty mangled symbol name.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t emit_weak_asm_for_symbol(
    struct gcc_jit_context *ctx,
    const char *symbol_name) GCC_GEN_BE_NOEXCEPT {
    size_t slen = strlen(symbol_name);
    void *raw_key = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(slen + 16, &raw_key));
    char *key_buf = (char *)raw_key;
    snprintf(key_buf, slen + 16, ".weak:%s", symbol_name);

    void *already = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_STRING, key_buf, &already);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(key_buf);
        return err;
    }
    if (already) {
        free(key_buf);
        return GCC_GEN_BE_SUCCESS;
    }
    err = cache_insert(GCC_GEN_BE_CACHE_STRING, key_buf, (void *)1);
    free(key_buf);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    if (p_gcc_jit_context_add_top_level_asm) {
        void *raw_asm = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_alloc(slen + 64, &raw_asm));
        char *asm_buf = (char *)raw_asm;
#if defined(__APPLE__) && defined(__MACH__)
        snprintf(asm_buf, slen + 64, ".weak_definition _%s\n", symbol_name);
        p_gcc_jit_context_add_top_level_asm(ctx, NULL, asm_buf);
#elif !defined(_MSC_VER)
        snprintf(asm_buf, slen + 64, ".weak %s\n", symbol_name);
        p_gcc_jit_context_add_top_level_asm(ctx, NULL, asm_buf);
#endif
        free(asm_buf);
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_is_scope_anonymous(
    a_scope_ptr scope,
    a_boolean *out_is_anon) GCC_GEN_BE_NOEXCEPT {
    if (!out_is_anon) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_is_anon = FALSE;
    for (a_scope_ptr cur = scope; cur != NULL; cur = cur->parent) {
        if (cur->kind == sck_namespace && cur->variant.assoc_namespace) {
            a_namespace_ptr ns = cur->variant.assoc_namespace;
            if (ns->has_internal_linkage || !ns->source_corresp.name || ns->source_corresp.name[0] == '\0') {
                *out_is_anon = TRUE;
                return GCC_GEN_BE_SUCCESS;
            }
        } else if (cur->kind == sck_function || cur->kind == sck_block) {
            *out_is_anon = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_mark_global_vague_linkage(
    struct gcc_jit_context *ctx,
    struct gcc_jit_lvalue *global,
    const char *symbol_name) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !global || !symbol_name || symbol_name[0] == '\0') {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
#if defined(GCC_JIT_VARIABLE_ATTRIBUTE_WEAK)
    if (p_gcc_jit_lvalue_add_attribute) {
        p_gcc_jit_lvalue_add_attribute(global, GCC_JIT_VARIABLE_ATTRIBUTE_WEAK);
        return GCC_GEN_BE_SUCCESS;
    }
#endif
    return emit_weak_asm_for_symbol(ctx, symbol_name);
}

gcc_gen_be_error_t gcc_gen_be_mark_function_vague_linkage(
    struct gcc_jit_context *ctx,
    struct gcc_jit_function *func,
    const char *symbol_name) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !func || !symbol_name || symbol_name[0] == '\0') {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (p_gcc_jit_function_add_attribute) {
        p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_WEAK);
        return GCC_GEN_BE_SUCCESS;
    }
    return emit_weak_asm_for_symbol(ctx, symbol_name);
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
