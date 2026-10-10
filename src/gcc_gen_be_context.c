/**
 * @file gcc_gen_be_context.c
 * @brief Implementation of context and core state management.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "fe_common.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_cache.h"
#include "cmd_line.h"
#include <libgccjit.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

static unsigned long get_current_thread_id(void);

static gcc_jit_context *gcc_jit_ctx = NULL;
static gcc_jit_block *current_block = NULL;
static gcc_gen_be_context_t backend_state = {0};

/**
 * @brief Retrieves the global GCC backend context object.
 *
 * @param out_state Pointer to receive the backend state object.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_get_state(gcc_gen_be_context_t **out_state) GCC_GEN_BE_NOEXCEPT {
    if (!out_state) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_state = &backend_state;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves the currently active libgccjit context.
 *
 * @param out_ctx Pointer to receive the gcc_jit_context pointer.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_get_context(gcc_jit_context **out_ctx) GCC_GEN_BE_NOEXCEPT {
    if (!out_ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ctx = gcc_jit_ctx;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Sets the currently active libgccjit context.
 *
 * @param ctx The gcc_jit_context to set as active.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_set_context(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_ctx = ctx;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves the currently active libgccjit block.
 *
 * @param out_block Pointer to receive the gcc_jit_block pointer.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_get_current_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = current_block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Sets the currently active libgccjit block.
 *
 * @param block The gcc_jit_block to set as active.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_set_current_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    current_block = block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Performs early initialization of the GCC backend.
 *
 * Loads the libgccjit dynamic library for the current platform.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an appropriate error code.
 */
gcc_gen_be_error_t gcc_gen_be_early_init(void) GCC_GEN_BE_NOEXCEPT {
    gcc_gen_be_error_t err;
#if defined(_WIN32)
    err = load_libgccjit_windows();
#elif defined(__unix__) || defined(__APPLE__) || defined(__FreeBSD__)
    err = load_libgccjit_posix();
#else
    err = GCC_GEN_BE_SUCCESS;
#endif
    return err;
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
    if (gcc_be_fPIC || gcc_be_fPIE) {
        gcc_jit_context_add_command_line_option(ctx, gcc_be_fPIE ? "-fPIE" : "-fPIC");
    }

    /* Bind trace logfile if diagnostics are on and tracing is requested */
    /* Wait, EDG doesn't have a direct flag for trace logfile in our added options, 
       but we can use standard output or a file if requested. For now, leave empty or 
       check if we want to trace. */
    
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

    backend_state.thread_id = get_current_thread_id();

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

    if (backend_state.cleanup_stack) {
        free(backend_state.cleanup_stack);
        backend_state.cleanup_stack = NULL;
        backend_state.cleanup_stack_size = 0;
        backend_state.cleanup_stack_capacity = 0;
    }
    
    return GCC_GEN_BE_SUCCESS;
}





#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <stdint.h>
#endif
#include <stdlib.h>

/**
 * @brief Gets the current thread ID.
 * @return The thread ID as an unsigned long.
 */
static unsigned long get_current_thread_id(void) {
#ifdef _WIN32
    return (unsigned long)GetCurrentThreadId();
#elif defined(__APPLE__)
    uint64_t tid;
    pthread_threadid_np(NULL, &tid);
    return (unsigned long)tid;
#else
    return (unsigned long)pthread_self();
#endif
}

/**
 * @brief Helper to push to a generic block stack.
 */
static gcc_gen_be_error_t push_to_block_stack(gcc_jit_block ***stack, size_t *size, size_t *capacity, gcc_jit_block *block) {
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
 */
static gcc_gen_be_error_t pop_from_block_stack(gcc_jit_block ***stack, size_t *size, gcc_jit_block **out_block) {
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
    if (backend_state.thread_id != get_current_thread_id()) {
        return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_INTERNAL, "Thread safety violation in gcc_gen_be context");
    }
    return GCC_GEN_BE_SUCCESS;
}

typedef void (*gcc_gen_be_cleanup_func_t)(void *);

typedef struct {
    gcc_gen_be_cleanup_func_t func;
    void *data;
} gcc_gen_be_cleanup_entry_t;

/**
 * @brief Pushes a cleanup function to the RAII stack.
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
 */
gcc_gen_be_error_t gcc_gen_be_execute_cleanups(void) GCC_GEN_BE_NOEXCEPT {
    gcc_gen_be_cleanup_entry_t *entries = (gcc_gen_be_cleanup_entry_t *)backend_state.cleanup_stack;
    while (backend_state.cleanup_stack_size > 0) {
        backend_state.cleanup_stack_size--;
        if (entries[backend_state.cleanup_stack_size].func) {
            entries[backend_state.cleanup_stack_size].func(entries[backend_state.cleanup_stack_size].data);
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
