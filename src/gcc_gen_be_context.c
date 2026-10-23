/**
 * @file gcc_gen_be_context.c
 * @brief Implementation of context and core state management.
 * @details Manages libgccjit context initialization, option propagation, control-flow stacks, and cleanup.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "fe_common.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_cache.h"
#include "cmd_line.h"
#include <libgccjit.h>
#ifdef _WIN32
#include <windows.h>
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
    if (gcc_be_opt_level == 3) gcc_jit_context_add_command_line_option(ctx, "-O3");
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

    if (gcc_be_fassociative_math) gcc_jit_context_add_command_line_option(ctx, "-fassociative-math");
    if (gcc_be_fno_trapping_math) gcc_jit_context_add_command_line_option(ctx, "-fno-trapping-math");

    if (gcc_be_m64) gcc_jit_context_add_command_line_option(ctx, "-m64");
    if (gcc_be_m32) gcc_jit_context_add_command_line_option(ctx, "-m32");

#endif

    if (gcc_be_shared) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
        gcc_jit_context_add_driver_option(ctx, "-shared");
#endif
    }
    if (gcc_be_static) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
        gcc_jit_context_add_driver_option(ctx, "-static");
#endif
    }
    
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
    gcc_jit_ctx = gcc_jit_context_acquire();
    if (!gcc_jit_ctx) {
        return GCC_GEN_BE_ERROR_OOM;
    }

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
    backend_state.active_subst_table.count = 0;
    
    return GCC_GEN_BE_SUCCESS;
}





/**
 * @brief Gets the current thread ID.
 * @details Queries the OS thread identifier and writes it to out_tid.
 * @param[out] out_tid Pointer to receive the thread ID as an unsigned long.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if out_tid is NULL.
 */
static gcc_gen_be_error_t get_current_thread_id(unsigned long *out_tid) GCC_GEN_BE_NOEXCEPT {
    if (!out_tid) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
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
        void *new_stack = realloc(*stack, new_cap * sizeof(gcc_jit_block *));
        if (!new_stack) return GCC_GEN_BE_ERROR_OOM;
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
        void *new_stack = realloc(backend_state.cleanup_stack, new_cap * sizeof(gcc_gen_be_cleanup_entry_t));
        if (!new_stack) return GCC_GEN_BE_ERROR_OOM;
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
        if (entries[backend_state.cleanup_stack_size].func) {
            GCC_GEN_BE_CHECK(entries[backend_state.cleanup_stack_size].func(entries[backend_state.cleanup_stack_size].data));
        }
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
    /** @brief Optional SjLj frame lvalue registered in this scope. */
    struct gcc_jit_lvalue *sjlj_frame;
} gcc_gen_be_scope_t;

gcc_gen_be_error_t gcc_gen_be_push_scope(an_object_lifetime_ptr lifetime) GCC_GEN_BE_NOEXCEPT {
    if (backend_state.scope_stack_size >= backend_state.scope_stack_capacity) {
        size_t new_cap = (backend_state.scope_stack_capacity == 0) ? 16 : (backend_state.scope_stack_capacity * 2);
        void *new_stack = realloc(backend_state.scope_stack, new_cap * sizeof(gcc_gen_be_scope_t *));
        if (!new_stack) return GCC_GEN_BE_ERROR_OOM;
        backend_state.scope_stack = (struct gcc_gen_be_scope **)new_stack;
        backend_state.scope_stack_capacity = new_cap;
    }
    
    gcc_gen_be_scope_t *new_scope = (gcc_gen_be_scope_t *)malloc(sizeof(gcc_gen_be_scope_t));
    if (!new_scope) return GCC_GEN_BE_ERROR_OOM;
    new_scope->lifetime = lifetime;
    new_scope->cleanups = NULL;
    new_scope->sjlj_frame = NULL;
    
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

    gcc_gen_be_cleanup_t *cleanup = (gcc_gen_be_cleanup_t *)malloc(sizeof(gcc_gen_be_cleanup_t));
    if (!cleanup) return GCC_GEN_BE_ERROR_OOM;
    
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

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));

    /* Traverse from top down to (but not including) the scope with target_lifetime */
    for (size_t i = backend_state.scope_stack_size; i > 0; --i) {
        gcc_gen_be_scope_t *scope = (gcc_gen_be_scope_t *)backend_state.scope_stack[i - 1];
        
        /* If we hit the target lifetime or its ancestor, we stop. 
           Wait, a target_lifetime might not exactly match our scope's lifetime pointer if EDG creates
           multiple overlapping ones. But typically, the innermost lifetime in goto/break EXACTLY matches
           one of the active block scopes. */
        /* To be safe, we also check if we reached the target. */
        
        if (target_lifetime != NULL) {
            /* Check if scope->lifetime is target_lifetime or an ancestor. 
               We just check equality for now. */
            an_object_lifetime_ptr curr = scope->lifetime;
            int found = 0;
            while (curr) {
                if (curr == target_lifetime) {
                    found = 1;
                    break;
                }
                curr = curr->parent_lifetime;
            }
            if (found) break; /* stop traversing */
        }
        
        if (scope->sjlj_frame) {
            void *cached_chain = NULL;
            void *cached_eh_t = NULL;
            if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_eh_chain", &cached_chain) == GCC_GEN_BE_SUCCESS && cached_chain &&
                cache_lookup(GCC_GEN_BE_CACHE_TYPE, (void *)"__eh_frame", &cached_eh_t) == GCC_GEN_BE_SUCCESS && cached_eh_t) {
                gcc_jit_struct *eh_st = gcc_jit_type_is_struct((gcc_jit_type *)cached_eh_t);
                if (eh_st) {
                    gcc_jit_field *f_next = gcc_jit_struct_get_field(eh_st, 0);
                    gcc_jit_rvalue *next_val = gcc_jit_lvalue_as_rvalue(
                        gcc_jit_lvalue_access_field(scope->sjlj_frame, NULL, f_next));
                    gcc_jit_block_add_assignment(block, NULL, (gcc_jit_lvalue *)cached_chain, next_val);
                }
            }
        }
#if !defined(EDG_GCC_BE_ISOLATED_TEST) && DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
        if (target_lifetime == (an_object_lifetime_ptr)(uintptr_t)1) {
            /* Normal stmk_return / normal scope exit: EDG already emitted explicit destructor calls */
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
                gcc_jit_rvalue *arg = gcc_jit_context_new_cast(ctx, NULL, var_addr, void_ptr_type);
                gcc_jit_block_add_eval(block, NULL, gcc_jit_context_new_call(ctx, NULL, cleanup->dtor_func, 1, &arg));
            }
            cleanup = cleanup->next;
        }
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Pops the current cleanup scope.
 * @details Decrements the scope depth and cleans up the scope structure.
 * @param[in] block The block to emit cleanups into before popping.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_pop_scope(struct gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    if (backend_state.scope_stack_size == 0) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT; /* Underflow */
    }
    
    gcc_gen_be_scope_t *top_scope = (gcc_gen_be_scope_t *)backend_state.scope_stack[backend_state.scope_stack_size - 1];
    
    /* Emit cleanups into block if provided */
    if (block) {
        gcc_jit_context *ctx = NULL;
        if (gcc_gen_be_get_context(&ctx) == GCC_GEN_BE_SUCCESS) {
            if (top_scope->sjlj_frame) {
                void *cached_chain = NULL;
                void *cached_eh_t = NULL;
                if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_eh_chain", &cached_chain) == GCC_GEN_BE_SUCCESS && cached_chain &&
                    cache_lookup(GCC_GEN_BE_CACHE_TYPE, (void *)"__eh_frame", &cached_eh_t) == GCC_GEN_BE_SUCCESS && cached_eh_t) {
                    gcc_jit_struct *eh_st = gcc_jit_type_is_struct((gcc_jit_type *)cached_eh_t);
                    if (eh_st) {
                        gcc_jit_field *f_next = gcc_jit_struct_get_field(eh_st, 0);
                        gcc_jit_rvalue *next_val = gcc_jit_lvalue_as_rvalue(
                            gcc_jit_lvalue_access_field(top_scope->sjlj_frame, NULL, f_next));
                        gcc_jit_block_add_assignment(block, NULL, (gcc_jit_lvalue *)cached_chain, next_val);
                    }
                }
            }
            gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
            gcc_gen_be_cleanup_t *cleanup = top_scope->cleanups;
#if !defined(EDG_GCC_BE_ISOLATED_TEST) && DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
            if (top_scope->lifetime != NULL) {
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
                    gcc_jit_rvalue *arg = gcc_jit_context_new_cast(ctx, NULL, var_addr, void_ptr_type);
                    gcc_jit_block_add_eval(block, NULL, gcc_jit_context_new_call(ctx, NULL, cleanup->dtor_func, 1, &arg));
                }
                cleanup = cleanup->next;
            }
        }
    }
    
    gcc_gen_be_cleanup_t *cleanup = top_scope->cleanups;
    while (cleanup) {
        gcc_gen_be_cleanup_t *next = cleanup->next;
        free(cleanup);
        cleanup = next;
    }
    
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_GUARD, top_scope, NULL));
    backend_state.scope_stack_size--;
    free(top_scope);
    
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
    if (backend_state.landing_pad_stack_size >= backend_state.landing_pad_stack_capacity) {
        size_t new_cap = (backend_state.landing_pad_stack_capacity == 0) ? 16 : (backend_state.landing_pad_stack_capacity * 2);
        gcc_jit_block **new_stack = (gcc_jit_block **)realloc(
            backend_state.landing_pad_stack, new_cap * sizeof(gcc_jit_block *));
        if (!new_stack) return GCC_GEN_BE_ERROR_OOM;
        backend_state.landing_pad_stack = new_stack;
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
    if (backend_state.landing_pad_stack_size == 0) {
        *out_lp_block = NULL;
    } else {
        *out_lp_block = backend_state.landing_pad_stack[backend_state.landing_pad_stack_size - 1];
    }
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
