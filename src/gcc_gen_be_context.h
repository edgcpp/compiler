/**
 * @file gcc_gen_be_context.h
 * @brief Context and core state management for the GCC backend.
 *
 * This file declares the global state (such as the gcc_jit_context and current
 * block) and lifecycle functions for initializing and cleaning up the backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_CONTEXT_H
#define GCC_GEN_BE_CONTEXT_H

#include "gcc_gen_be_error.h"
#include <stddef.h>

/* Forward declarations for libgccjit types to avoid including the header in
 * every file that needs the context, though it may still be needed depending
 * on usage. */
typedef struct gcc_jit_context gcc_jit_context;
typedef struct gcc_jit_block gcc_jit_block;
struct gcc_jit_function;
struct gcc_jit_location;
struct gcc_jit_lvalue;
struct gcc_jit_rvalue;
struct gcc_jit_type;

#include "fe_common.h"

BEGIN_EDG_NAMESPACE

/**
 * @brief Retrieves the global libgccjit context.
 *
 * @param out_ctx Pointer to receive the gcc_jit_context pointer.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_context(gcc_jit_context **out_ctx) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Sets the global libgccjit context.
 *
 * @param ctx The gcc_jit_context to set as global.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_set_context(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Retrieves the currently active libgccjit block.
 *
 * @param out_block Pointer to receive the gcc_jit_block pointer.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_current_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Sets the currently active libgccjit block.
 *
 * @param block The gcc_jit_block to set as current.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_set_current_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT;

/**
 * @struct gcc_gen_be_context_t
 * @brief Encapsulates the global state for the GCC backend.
 *
 * This structure holds the primary libgccjit context, the current
 * active block, and various stacks for control flow and resource cleanup.
 */
typedef struct {
    /** @brief The path to the active translation unit. */
    const char *tu_path;
    
    /** @brief The identifier for the current module. */
    const char *module_id;

    /** @brief Stack of libgccjit blocks to jump to on 'break' in a loop. */
    gcc_jit_block **break_stack;
    
    /** @brief Number of entries in the break stack. */
    size_t break_stack_size;
    
    /** @brief Capacity of the break stack. */
    size_t break_stack_capacity;

    /** @brief Stack of libgccjit blocks to jump to on 'continue' in a loop. */
    gcc_jit_block **continue_stack;
    
    /** @brief Number of entries in the continue stack. */
    size_t continue_stack_size;
    
    /** @brief Capacity of the continue stack. */
    size_t continue_stack_capacity;

    /** @brief Stack of libgccjit blocks to jump to on 'break' in a switch. */
    gcc_jit_block **switch_exit_stack;
    
    /** @brief Number of entries in the switch exit stack. */
    size_t switch_exit_stack_size;
    
    /** @brief Capacity of the switch exit stack. */
    size_t switch_exit_stack_capacity;

    /** @brief Stack of libgccjit blocks for nested scope management. */
    gcc_jit_block **block_stack;
    
    /** @brief Number of entries in the nested block stack. */
    size_t block_stack_size;
    
    /** @brief Capacity of the nested block stack. */
    size_t block_stack_capacity;

    /** @brief Stack for RAII cleanups and destructor invocations. */
    void **cleanup_stack;
    
    /** @brief Number of entries in the cleanup stack. */
    size_t cleanup_stack_size;
    
    /** @brief Capacity of the cleanup stack. */
    size_t cleanup_stack_capacity;

    /** @brief Global static initialization function. */
    struct gcc_jit_function *global_ctor_func;
    
    /** @brief Global static initialization block. */
    struct gcc_jit_block *global_ctor_block;
    
    /** @brief Thread ID of the context owner to enforce thread-safety. */
    unsigned long thread_id;
} gcc_gen_be_context_t;

/**
 * @brief Retrieves the global GCC backend context object.
 *
 * @param out_state Pointer to receive the backend state object.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_state(gcc_gen_be_context_t **out_state) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Pushes a block onto the break stack.
 *
 * @param block The gcc_jit_block to jump to on 'break'.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_push_break_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Pops a block from the break stack.
 *
 * @param out_block Pointer to receive the popped block.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_pop_break_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Pushes a block onto the continue stack.
 *
 * @param block The gcc_jit_block to jump to on 'continue'.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_push_continue_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Pops a block from the continue stack.
 *
 * @param out_block Pointer to receive the popped block.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_pop_continue_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Pushes a block onto the switch exit stack.
 *
 * @param block The gcc_jit_block to jump to on 'break' in a switch.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_push_switch_exit_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Pops a block from the switch exit stack.
 *
 * @param out_block Pointer to receive the popped block.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_pop_switch_exit_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Verifies that the current thread owns the context.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_check_thread_safety(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Configures libgccjit context based on command line options.
 *
 * @param ctx The gcc_jit_context to configure.
 * @return A GCC_GEN_BE_SUCCESS on success, or an error code otherwise.
 */
extern gcc_gen_be_error_t gcc_gen_be_context_configure_options(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Performs early initialization of the backend, such as loading libraries.
 *
 * @return A GCC_GEN_BE_SUCCESS on success, or an error code otherwise.
 */
extern gcc_gen_be_error_t gcc_gen_be_early_init(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Initializes the main gcc_jit_context and sets default options.
 *
 * @return A GCC_GEN_BE_SUCCESS on success, or an error code otherwise.
 */
extern gcc_gen_be_error_t gcc_gen_be_init(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Cleans up the gcc_jit_context and associated caches.
 *
 * @return A GCC_GEN_BE_SUCCESS on success, or an error code otherwise.
 */
extern gcc_gen_be_error_t gcc_gen_be_cleanup(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Type for cleanup functions pushed to the RAII stack.
 *
 * @param data Opaque pointer passed during registration.
 */
typedef void (*gcc_gen_be_cleanup_func_t)(void *data);

/**
 * @brief Pushes a cleanup function to the RAII stack.
 *
 * @param func The function to execute during cleanup.
 * @param data Data pointer to pass to the cleanup function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_push_cleanup(gcc_gen_be_cleanup_func_t func, void *data) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Executes all pending cleanups in reverse order and clears the stack.
 *
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_execute_cleanups(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Pushes a block to a generic block stack.
 *
 * This function enables nested scope and block stack management by pushing a block
 * to the generic block stack for tracking currently active nested scopes.
 *
 * @param block The block to push onto the stack.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_push_block(gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Pops a block from the generic block stack.
 *
 * @param out_block Pointer to receive the popped block.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_pop_block(gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_CONTEXT_H */
