/**
 * @file gcc_gen_be_context.h
 * @brief Context and core state management for the GCC backend.
 * @details Declares the per-translation-unit singleton state (`gcc_gen_be_context_t`),
 * control-flow/scope/cleanup stacks, dynamic buffers, and lifecycle APIs.
 * @ingroup gcc_gen_be
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
/** @brief gcc_jit_context member. */
typedef struct gcc_jit_context gcc_jit_context;
/** @brief gcc_jit_block member. */
typedef struct gcc_jit_block gcc_jit_block;
struct gcc_jit_function;
struct gcc_jit_location;
struct gcc_jit_lvalue;
struct gcc_jit_rvalue;
struct gcc_jit_type;
struct gcc_jit_field;

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
 * @brief Tracks Itanium C++ ABI substitution candidates (`S_`, `S0_`, ...).
 * @details Dynamically resizes heap-allocated candidate strings during a mangling pass.
 */
typedef struct gcc_gen_be_mangle_subst_table {
    /** @brief Dynamically allocated array of heap-allocated mangled entity strings. */
    char **entries;
    /** @brief Number of entries currently stored. */
    int count;
    /** @brief Allocated capacity of `entries`. */
    int capacity;
} gcc_gen_be_mangle_subst_table_t;

/**
 * @brief Represents an active `__try` / `__finally` scope on the SEH finally stack.
 * @details Tracks the `cleanup_statement` and the `_abnormal_termination()` flag local variable
 * so early exits (`return`, `goto`, `break`, `continue`) out of `__try` execute `__finally` with
 * `_abnormal_termination() == 1`.
 */
typedef struct gcc_gen_be_seh_finally_entry {
    /** @brief The frontend `__finally` cleanup statement AST node. */
    a_statement_ptr cleanup_statement;
    /** @brief Local `int` lvalue holding the `_abnormal_termination()` flag (0 normal, 1 abnormal). */
    struct gcc_jit_lvalue *abnormal_flag_lval;
    /** @brief Enclosing `__try` / `__finally` scope on the stack. */
    struct gcc_gen_be_seh_finally_entry *prev;
} gcc_gen_be_seh_finally_entry_t;

/**
 * @brief Encapsulates per-coroutine lowering state for reentrant/nested coroutine lowering.
 * @details Stores the active coroutine frame local variable, frame struct type, state/promise fields,
 * coroutine descriptor, suspension point counter, and dynamic array of resume blocks.
 */
typedef struct gcc_gen_be_coro_state {
    /** @brief Active coroutine suspension state index counter. */
    int state_index;
    /** @brief Active coroutine frame pointer local variable. */
    struct gcc_jit_lvalue *frame_lval;
    /** @brief Active coroutine frame struct type. */
    struct gcc_jit_type *frame_type;
    /** @brief Active coroutine `__suspend_index` field. */
    struct gcc_jit_field *state_field;
    /** @brief Active coroutine `__promise` field. */
    struct gcc_jit_field *promise_field;
    /** @brief Active coroutine descriptor. */
    a_coroutine_descr_ptr descr;
    /** @brief Dynamic array of resume target blocks indexed by suspension point (1..N). */
    struct gcc_jit_block **resume_blocks;
    /** @brief Dynamic array of active lexical scopes at each suspension point (1..N). */
    a_scope_ptr *suspend_scopes;
    /** @brief Number of registered coroutine resume blocks. */
    int resume_count;
    /** @brief Capacity of `resume_blocks`. */
    int resume_capacity;
    /** @brief Pointer to the enclosing coroutine state on the stack. */
    struct gcc_gen_be_coro_state *prev;
} gcc_gen_be_coro_state_t;

/** @brief EH region descriptor for LSDA call-site generation. */
typedef struct gcc_gen_be_eh_region {
    /** @brief Try ID. */
    int try_id;
    /** @brief LP ID. */
    int landing_pad_id;
    /** @brief Action table index. */
    int action_table_index;
    /** @brief Next region. */
    struct gcc_gen_be_eh_region *next;
} gcc_gen_be_eh_region_t;

/** @brief EH type filter entry for LSDA @TType generation. */
typedef struct gcc_gen_be_eh_type_filter {
    /** @brief Type. */
    a_type_ptr type;
    /** @brief Type ID. */
    int type_id;
    /** @brief Next filter. */
    struct gcc_gen_be_eh_type_filter *next;
} gcc_gen_be_eh_type_filter_t;

/** @brief Per-function exception handling state structure. */
typedef struct gcc_gen_be_eh_state {
    /** @brief Active `gcc_jit_function` being lowered. */
    struct gcc_jit_function *current_func;
    /** @brief Current function ID. */
    int current_func_id;
    /** @brief Next try ID. */
    int next_try_id;
    /** @brief Next action ID. */
    int next_action_id;
    /** @brief Next type ID. */
    int next_type_id;
    /** @brief Whether the landing-pad start label has been emitted. */
    a_boolean lp_start_emitted;
    /** @brief Nesting depth of active try blocks. */
    int active_try_depth;
    /** @brief Linked list of EH regions in the current function. */
    gcc_gen_be_eh_region_t *regions;
    /** @brief Linked list of EH catch type filters in the current function. */
    gcc_gen_be_eh_type_filter_t *types;
    /** @brief Dynamically allocated LSDA action table buffer. */
    char *action_table;
    /** @brief Action table length in bytes. */
    int action_table_len;
    /** @brief Action table capacity in bytes. */
    int action_table_cap;
    /** @brief Enclosing EH state for nested function lowering. */
    struct gcc_gen_be_eh_state *prev;
} gcc_gen_be_eh_state_t;

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

    /** @brief Stack of active exception landing pad blocks. */
    gcc_jit_block **landing_pad_stack;

    /** @brief Number of entries in the landing pad stack. */
    size_t landing_pad_stack_size;

    /** @brief Capacity of the landing pad stack. */
    size_t landing_pad_stack_capacity;

    /** @brief Stack for lexical scopes containing local variables. */
    struct gcc_gen_be_scope **scope_stack;
    
    /** @brief Number of entries in the scope stack. */
    size_t scope_stack_size;
    
    /** @brief Capacity of the scope stack. */
    size_t scope_stack_capacity;

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

    /** @brief Active statement-expression (`({ ... })`) result lvalue slot. */
    struct gcc_jit_lvalue *stmt_expr_result_lval;

    /** @brief Computed-goto dispatcher block for the active function. */
    struct gcc_jit_block *computed_goto_dispatch_block;

    /** @brief Computed-goto target address local variable for the active function. */
    struct gcc_jit_lvalue *computed_goto_target_lval;

    /** @brief Dynamic array of address-taken labels for computed gotos in the active function. */
    a_label_ptr *computed_goto_labels;

    /** @brief Number of collected computed goto labels. */
    int computed_goto_labels_count;

    /** @brief Capacity of the collected computed goto labels array. */
    int computed_goto_labels_capacity;

    /** @brief Stack of active `__try` / `__finally` scopes in the current function. */
    gcc_gen_be_seh_finally_entry_t *active_seh_finally_stack;

    /** @brief Active coroutine suspension state index counter. */
    int active_coro_state_index;

    /** @brief Active coroutine frame `gcc_jit_type *`. */
    struct gcc_jit_type *active_coro_frame_type;

    /** @brief Active coroutine frame pointer local variable (`gcc_jit_lvalue *`). */
    struct gcc_jit_lvalue *active_coro_frame_lval;

    /** @brief Active coroutine `__suspend_index` field pointer. */
    struct gcc_jit_field *active_coro_state_field;

    /** @brief Active coroutine `__promise` field pointer. */
    struct gcc_jit_field *active_coro_promise_field;

    /** @brief Active coroutine descriptor for the current coroutine function. */
    a_coroutine_descr_ptr active_coro_descr;

    /** @brief Dynamic array of resume target blocks indexed by suspension point (1..N). */
    struct gcc_jit_block **active_coro_resume_blocks;

    /** @brief Dynamic array of active lexical scopes at each suspension point (1..N). */
    a_scope_ptr *active_coro_suspend_scopes;

    /** @brief Active lexical scope during coroutine body statement lowering. */
    a_scope_ptr active_coro_current_scope;

    /** @brief Number of registered coroutine resume blocks. */
    int active_coro_resume_blocks_count;

    /** @brief Capacity of `active_coro_resume_blocks`. */
    int active_coro_resume_blocks_capacity;

    /** @brief Stack of saved enclosing coroutine states for nested/lambda coroutines. */
    gcc_gen_be_coro_state_t *coro_state_stack;

    /** @brief Active per-function exception handling state (`eh_state`). */
    gcc_gen_be_eh_state_t eh_state;

    /** @brief Opaque pointer to the active per-function exception handling state (`eh_state`). */
    void *eh_state_ptr;

    /** @brief Active thrown exception buffer local lvalue (`void *`) during `throw` initialization. */
    struct gcc_jit_lvalue *active_thrown_obj_lval;

    /** @brief Active base-adjusted caught object local lvalue (`void *`) inside a `catch` handler. */
    struct gcc_jit_lvalue *active_caught_obj_lval;

    /** @brief Global function counter for landing pad uniqueness. */
    int global_func_counter;

    /** @brief Active Itanium C++ ABI substitution table for mangling. */
    gcc_gen_be_mangle_subst_table_t active_subst_table;

    /** @brief Dynamically allocated queue of deferred opaque structs (`deferred_struct_entry_t *`). */
    void *deferred_structs;

    /** @brief Number of active entries in `deferred_structs`. */
    int deferred_structs_count;

    /** @brief Allocated capacity of `deferred_structs`. */
    int deferred_structs_capacity;

    /** @brief Dynamically allocated table of recorded DWARF DIE entries (`gcc_gen_be_dwarf_die_entry_t *`). */
    void *tu_dwarf_die_table;

    /** @brief Number of active entries in `tu_dwarf_die_table`. */
    int tu_dwarf_die_count;

    /** @brief Allocated capacity of `tu_dwarf_die_table`. */
    int tu_dwarf_die_capacity;

    /** @brief Dynamically allocated cache of visited `a_scope_ptr` pointers during recursive scope traversal. */
    void **visited_scopes;

    /** @brief Number of visited scopes recorded in `visited_scopes`. */
    size_t visited_scopes_count;

    /** @brief Allocated capacity of `visited_scopes`. */
    size_t visited_scopes_capacity;

    /** @brief Monotonically increasing translation-unit sequence counter for unique internal init symbols. */
    unsigned long tu_sequence_id;

    /** @brief TRUE when lowering a full front-end IL translation unit (`il_header.primary_scope != NULL`). */
    a_boolean has_primary_il_scope;

    /** @brief Active EDG function scope (`def_scope`) during function body lowering. */
    a_scope_ptr active_function_scope;

    /** @brief Stack of active lowered EH `a_dynamic_init_ptr` entries above the innermost `try` barrier. */
    a_dynamic_init_ptr eh_cleanup_dips[64];

    /** @brief Current number of entries in `eh_cleanup_dips`. */
    size_t eh_cleanup_dip_depth;

    /** @brief Stack of enclosing `try`-block lowered EH `a_dynamic_init_ptr` barriers. */
    a_dynamic_init_ptr eh_try_barriers[32];

    /** @brief Current depth of `eh_try_barriers`. */
    size_t eh_try_barrier_depth;

    /** @brief Size optimization level (0 = none, 1 = -Os, 2 = -Oz). */
    int opt_size_level;

    /** @brief Debug info verbosity level (0 = none, 1 = line tables, 2 = -g, 3 = -g3). */
    int debug_level;

    /** @brief LTO mode (0 = none, 1 = full -flto, 2 = thin -flto). */
    int lto_mode;

    /** @brief Default symbol visibility (0 = default, 1 = hidden, 2 = protected, 3 = internal). */
    int symbol_visibility;

    /** @brief Emit each function into its own section (-ffunction-sections). */
    a_boolean function_sections;

    /** @brief Emit each data item into its own section (-fdata-sections). */
    a_boolean data_sections;

    /** @brief Enable OpenMP lowering and libgomp linking (-fopenmp). */
    a_boolean openmp_enabled;

    /** @brief TRUE when non-lock-free atomics were emitted and `-latomic` should be linked. */
    a_boolean needs_libatomic;

    /** @brief Unmangled name of the routine currently being lowered (for `__builtin_FUNCTION`). */
    const char *active_routine_name;

    /** @brief Dynamically allocated table of OpenMP statement-pragma associations (`gcc_gen_be_omp_pragma_entry_t *`). */
    void *omp_pragma_entries;

    /** @brief Number of active entries in `omp_pragma_entries`. */
    size_t omp_pragma_count;

    /** @brief Allocated capacity of `omp_pragma_entries`. */
    size_t omp_pragma_capacity;

    /** @brief Monotonically increasing counter for outlined OpenMP callback functions. */
    unsigned long omp_outlined_counter;

    /** @brief Enable POSIX threads (-pthread). */
    a_boolean pthread_enabled;

    /** @brief Bitmask of active `edg_be_sanitizer_flag_t` sanitizers. */
    unsigned int sanitizers;

    /** @brief Dynamically allocated array of `-L` library search directory strings. */
    char **driver_lib_dirs;

    /** @brief Number of entries in `driver_lib_dirs`. */
    size_t driver_lib_dirs_count;

    /** @brief Dynamically allocated array of `-l` library name strings. */
    char **driver_libs;

    /** @brief Number of entries in `driver_libs`. */
    size_t driver_libs_count;

    /** @brief Dynamically allocated array of `-Wl,*` / raw linker passthrough flags. */
    char **driver_ldflags;

    /** @brief Number of entries in `driver_ldflags`. */
    size_t driver_ldflags_count;

    /** @brief Optional canonical EDG function mangler hook (`get_mangled_function_name`). */
    char *(*canonical_mangle_func)(a_routine_ptr rout);

    /** @brief Optional canonical EDG variable mangler hook (`get_mangled_variable_name`). */
    a_const_char *(*canonical_mangle_var)(a_variable_ptr var);

    /** @brief Optional canonical EDG vtable/construction-vtable mangler hook (`mangled_vtbl_name`). */
    char *(*canonical_mangle_vtbl)(a_type_ptr class_type, a_base_class_ptr bcp, a_base_class_ptr ctor_bcp);

    /** @brief Optional canonical EDG typeinfo mangler hook (`mangled_typeinfo_name`). */
    char *(*canonical_mangle_typeinfo)(a_type_ptr tp);

    /** @brief Optional canonical EDG typeinfo-string mangler hook (`mangled_typeinfo_string_name`). */
    char *(*canonical_mangle_typeinfo_str)(a_type_ptr tp);

    /** @brief Optional canonical EDG VTT mangler hook (`mangled_virtual_table_table_name`). */
    char *(*canonical_mangle_vtt)(a_type_ptr tp);

    /** @brief Optional target ABI override (-1 when using host default, or `gcc_gen_be_target_abi_t`). */
    int active_target_abi_override;
    
    /** @brief Thread ID of the context owner to enforce thread-safety. */
    unsigned long thread_id;
} gcc_gen_be_context_t;

/** @brief Singleton backend context instance encapsulating all per-TU lowering state. */
extern gcc_gen_be_context_t gcc_gen_be_singleton_state;

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
 * @param[in] data Opaque pointer passed during registration.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
typedef gcc_gen_be_error_t (*gcc_gen_be_cleanup_func_t)(void *data);

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

/**
 * @brief Pushes a new lexical scope for variable shadowing onto the scope stack.
 *
 * @param[in] lifetime The scope block to push.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_push_scope(an_object_lifetime_ptr lifetime) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Pops the current lexical scope from the scope stack.
 *
 * @param[in] block The block.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_pop_scope(struct gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Registers a local variable destructor cleanup on the innermost scope stack frame.
 * @details Records `(var, dtor_func)` so lexical exit, `return`, `break`, `continue`, `goto`,
 * and exception unwind emit destructor calls in reverse construction order.
 * @param[in] var The local `gcc_jit_lvalue` to destroy.
 * @param[in] dtor_func The destructor `gcc_jit_function` taking `&var`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_register_cleanup(struct gcc_jit_lvalue *var, struct gcc_jit_function *dtor_func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Prepends a variable cleanup (`__attribute__((cleanup))`) to the front of the innermost scope's LIFO list.
 * @details Used when registering cleanups in forward statement execution order so destruction runs LIFO.
 * @param[in] var Non-null variable lvalue to pass by address to `dtor_func`.
 * @param[in] dtor_func Non-null cleanup routine `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t gcc_gen_be_prepend_cleanup(
    struct gcc_jit_lvalue *var,
    struct gcc_jit_function *dtor_func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits pending scope destructor calls down to `target_lifetime` into `block`.
 * @details Iterates active scope frames from innermost outward until `target_lifetime` is reached,
 * invoking each registered destructor in LIFO order.
 * @param[in] target_lifetime The target enclosing `an_object_lifetime_ptr` (NULL for full function exit).
 * @param[in,out] block The `gcc_jit_block` into which destructor calls are emitted.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_cleanups(an_object_lifetime_ptr target_lifetime, struct gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Determines whether an EDG scope is enclosed by an anonymous namespace or function-local scope.
 * @details Walks `scope->parent` upward; sets `*out_is_anon = TRUE` if any enclosing namespace has
 * internal linkage (unnamed namespace) or if `scope` is inside a function/block scope.
 * @param[in] scope The starting `a_scope_ptr` (may be NULL).
 * @param[out] out_is_anon Pointer to receive `TRUE` if internal linkage is required, `FALSE` otherwise.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_is_anon` is NULL.
 */
extern gcc_gen_be_error_t gcc_gen_be_is_scope_anonymous(
    a_scope_ptr scope,
    a_boolean *out_is_anon) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Marks an exported global variable (`_ZTV*`, `_ZTI*`, `_ZTS*`, `_ZTT*`, `_ZTC*`, `inline` var) as weak/linkonce.
 * @details Uses `p_gcc_jit_lvalue_add_attribute` when supported by `libgccjit`, and falls back to
 * emitting target-appropriate top-level assembler directives (`.weak <sym>` on ELF,
 * `.weak_definition _<sym>` on Mach-O, or COFF `.linkonce discard`) deduplicated per context.
 * @param[in,out] ctx The active `gcc_jit_context`.
 * @param[in,out] global The exported `gcc_jit_lvalue` global variable.
 * @param[in] symbol_name The mangled symbol name of `global`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_mark_global_vague_linkage(
    struct gcc_jit_context *ctx,
    struct gcc_jit_lvalue *global,
    const char *symbol_name) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Marks an exported vague-linkage function (`inline`, template, thunk, `C2`/`D2`/`D0`) as weak/linkonce.
 * @details Uses `p_gcc_jit_function_add_attribute(..., GCC_JIT_FN_ATTRIBUTE_WEAK)` when available,
 * falling back to target-appropriate top-level assembler weak directives on older `libgccjit`.
 * @param[in,out] ctx The active `gcc_jit_context`.
 * @param[in,out] func The exported `gcc_jit_function`.
 * @param[in] symbol_name The mangled symbol name of `func`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_mark_function_vague_linkage(
    struct gcc_jit_context *ctx,
    struct gcc_jit_function *func,
    const char *symbol_name) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Pushes an exception landing pad block onto the active landing pad stack.
 * @details Used for nested `try` blocks and cleanup landing pads.
 * @param[in] lp_block The landing pad `gcc_jit_block` to push.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_push_landing_pad(gcc_jit_block *lp_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Pops the top exception landing pad block from the active landing pad stack.
 * @details Restores the enclosing landing pad after leaving a `try` or cleanup region.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code if the stack is empty.
 */
extern gcc_gen_be_error_t gcc_gen_be_pop_landing_pad(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Retrieves the currently active exception landing pad block, if any.
 * @details Outputs the top block on the landing pad stack or `NULL` if none is active.
 * @param[out] out_lp_block Pointer to receive the active landing pad block.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_active_landing_pad(gcc_jit_block **out_lp_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Records that the innermost cleanup scope pushed a landing pad onto `landing_pad_stack`.
 * @details Ensures `gcc_gen_be_pop_scope` automatically pops the scope's landing pad when exiting.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INTERNAL` if `scope_stack` is empty.
 */
extern gcc_gen_be_error_t gcc_gen_be_record_scope_unwind_lp(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Queries whether the innermost cleanup scope already owns an active unwind landing pad.
 * @details Prevents duplicate landing-pad creation when multiple variables register cleanups in one scope.
 * @param[out] out_has_lp Pointer receiving `TRUE` if the innermost scope already recorded an unwind LP.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_has_scope_unwind_lp(a_boolean *out_has_lp) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_CONTEXT_H */
