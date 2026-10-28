/**
 * @file gcc_gen_be_stmt.c
 * @brief Implementation of statement lowering for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_rtti.h"
#include "func_def.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_type.h"
#include "expr.h"
#include "il_def.h"
#include <libgccjit.h>
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE




/**
 * @brief Gets or creates a guard variable for thread-safe initialization.
 *
 * @details Looks up or creates a unique guard variable for the given static local.
 *
 * @param[in] ctx The libgccjit context.
 * @param[in] var The frontend variable representing the static local.
 * @param[out] out_guard Pointer to receive the lvalue of the guard.
 * @return GCC_GEN_BE_SUCCESS on success.
 */

/** @brief Context-backed array of collected computed-goto labels. */
#define computed_goto_labels (gcc_gen_be_singleton_state.computed_goto_labels)
/** @brief Context-backed number of collected computed-goto labels. */
#define computed_goto_labels_count (gcc_gen_be_singleton_state.computed_goto_labels_count)
/** @brief Context-backed capacity of the collected computed-goto labels array. */
#define computed_goto_labels_capacity (gcc_gen_be_singleton_state.computed_goto_labels_capacity)
/** @brief Context-backed basic block for dispatching computed gotos. */
#define computed_goto_dispatcher_block (gcc_gen_be_singleton_state.computed_goto_dispatch_block)
/** @brief Context-backed local variable storing the target address for computed gotos. */
#define computed_goto_target_lval (gcc_gen_be_singleton_state.computed_goto_target_lval)
/** @brief Context-backed stack of active `__try` / `__finally` scopes in the current function. */
#define active_seh_finally_stack (gcc_gen_be_singleton_state.active_seh_finally_stack)

/**
 * @brief Emits all active `__finally` cleanup blocks along an early-exit control-flow edge (`return` or `goto`).
 * @details Sets each active scope's `abnormal_flag_lval` to `1` and lowers its `cleanup_statement` before
 * completing the branch. Temporarily unlinks visited scopes to prevent recursive re-entry if `cleanup_statement`
 * itself contains control-flow statements.
 * @param[in] func The active `gcc_jit_function`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t emit_active_seh_finally_cleanups(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!active_seh_finally_stack || !func) return GCC_GEN_BE_SUCCESS;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

    gcc_gen_be_seh_finally_entry_t *saved_stack = active_seh_finally_stack;
    active_seh_finally_stack = NULL;

    for (gcc_gen_be_seh_finally_entry_t *curr = saved_stack; curr != NULL; curr = curr->prev) {
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (cblock && curr->abnormal_flag_lval) {
            gcc_jit_block_add_assignment(
                cblock, NULL, curr->abnormal_flag_lval,
                gcc_jit_context_one(ctx, int_type));
        }
        if (cblock && curr->cleanup_statement) {
            void *prev_abn = NULL;
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_abnormal_termination", &prev_abn));
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_abnormal_termination", curr->abnormal_flag_lval));
            gcc_gen_be_error_t err = gcc_gen_be_lower_statement(curr->cleanup_statement, func);
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_abnormal_termination", prev_abn));
            if (err != GCC_GEN_BE_SUCCESS) {
                active_seh_finally_stack = saved_stack;
                return err;
            }
        }
    }

    active_seh_finally_stack = saved_stack;
    return GCC_GEN_BE_SUCCESS;
}

/** @brief Context-backed active coroutine suspension state index counter. */
#define active_coro_state_index (gcc_gen_be_singleton_state.active_coro_state_index)
/** @brief Context-backed active coroutine frame pointer local variable. */
#define active_coro_frame_lval (gcc_gen_be_singleton_state.active_coro_frame_lval)
/** @brief Context-backed active coroutine frame struct type. */
#define active_coro_frame_type (gcc_gen_be_singleton_state.active_coro_frame_type)
/** @brief Context-backed active coroutine `__suspend_index` field. */
#define active_coro_state_field (gcc_gen_be_singleton_state.active_coro_state_field)
/** @brief Context-backed active coroutine `__promise` field. */
#define active_coro_promise_field (gcc_gen_be_singleton_state.active_coro_promise_field)
/** @brief Context-backed active coroutine descriptor for the current coroutine function. */
#define active_coro_descr (gcc_gen_be_singleton_state.active_coro_descr)
/** @brief Context-backed dynamic array of resume target blocks indexed by suspension point (1..N). */
#define active_coro_resume_blocks (gcc_gen_be_singleton_state.active_coro_resume_blocks)
/** @brief Context-backed dynamic array of active lexical scopes at each suspension point (1..N). */
#define active_coro_suspend_scopes (gcc_gen_be_singleton_state.active_coro_suspend_scopes)
/** @brief Context-backed active lexical scope during coroutine body statement lowering. */
#define active_coro_current_scope (gcc_gen_be_singleton_state.active_coro_current_scope)
/** @brief Context-backed number of registered coroutine resume blocks. */
#define active_coro_resume_blocks_count (gcc_gen_be_singleton_state.active_coro_resume_blocks_count)
/** @brief Context-backed capacity of `active_coro_resume_blocks`. */
#define active_coro_resume_blocks_capacity (gcc_gen_be_singleton_state.active_coro_resume_blocks_capacity)
/** @brief Context-backed stack of saved enclosing coroutine states for nested/lambda coroutines. */
#define coro_state_stack (gcc_gen_be_singleton_state.coro_state_stack)

/**
 * @brief Pushes the current coroutine lowering state onto `coro_state_stack`.
 * @details Preserves all `active_coro_*` globals and resets them for a newly entered coroutine.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_OOM on allocation failure.
 */
static gcc_gen_be_error_t push_coro_state(void) GCC_GEN_BE_NOEXCEPT {
    gcc_gen_be_coro_state_t *node = (gcc_gen_be_coro_state_t *)malloc(sizeof(gcc_gen_be_coro_state_t));
    if (!node) return GCC_GEN_BE_ERROR_OOM;
    node->state_index = active_coro_state_index;
    node->frame_lval = active_coro_frame_lval;
    node->frame_type = active_coro_frame_type;
    node->state_field = active_coro_state_field;
    node->promise_field = active_coro_promise_field;
    node->descr = active_coro_descr;
    node->resume_blocks = active_coro_resume_blocks;
    node->suspend_scopes = active_coro_suspend_scopes;
    node->resume_count = active_coro_resume_blocks_count;
    node->resume_capacity = active_coro_resume_blocks_capacity;
    node->prev = coro_state_stack;
    coro_state_stack = node;

    active_coro_state_index = 0;
    active_coro_frame_lval = NULL;
    active_coro_frame_type = NULL;
    active_coro_state_field = NULL;
    active_coro_promise_field = NULL;
    active_coro_descr = NULL;
    active_coro_resume_blocks = NULL;
    active_coro_suspend_scopes = NULL;
    active_coro_resume_blocks_count = 0;
    active_coro_resume_blocks_capacity = 0;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Pops and restores the enclosing coroutine lowering state from `coro_state_stack`.
 * @details Frees the current coroutine's dynamic `active_coro_resume_blocks` array and restores saved fields.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t pop_coro_state(void) GCC_GEN_BE_NOEXCEPT {
    if (active_coro_resume_blocks) {
        free(active_coro_resume_blocks);
        active_coro_resume_blocks = NULL;
    }
    if (active_coro_suspend_scopes) {
        free(active_coro_suspend_scopes);
        active_coro_suspend_scopes = NULL;
    }
    if (!coro_state_stack) return GCC_GEN_BE_SUCCESS;
    gcc_gen_be_coro_state_t *node = coro_state_stack;
    coro_state_stack = node->prev;
    active_coro_state_index = node->state_index;
    active_coro_frame_lval = node->frame_lval;
    active_coro_frame_type = node->frame_type;
    active_coro_state_field = node->state_field;
    active_coro_promise_field = node->promise_field;
    active_coro_descr = node->descr;
    active_coro_resume_blocks = node->resume_blocks;
    active_coro_suspend_scopes = node->suspend_scopes;
    active_coro_resume_blocks_count = node->resume_count;
    active_coro_resume_blocks_capacity = node->resume_capacity;
    free(node);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively binds all variables in `scope` and its nested sub-scopes (`scope->scopes`) to their coroutine frame field lvalues.
 * @details Looks up each variable in `GCC_GEN_BE_CACHE_FIELD` and inserts `gcc_jit_lvalue_access_field(frame_deref, NULL, fld)` into `GCC_GEN_BE_CACHE_VAR`.
 * @param[in] scope The EDG scope to traverse.
 * @param[in] frame_deref Dereferenced coroutine frame struct lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t map_coro_scope_vars_to_frame(
    a_scope_ptr scope,
    gcc_jit_lvalue *frame_deref) GCC_GEN_BE_NOEXCEPT {
    if (!scope || !frame_deref) return GCC_GEN_BE_SUCCESS;

    if (scope->kind == sck_function) {
        for (a_variable_ptr pvar = scope->variant.routine.parameters; pvar != NULL; pvar = pvar->next) {
            void *cached_fld = NULL;
            if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, pvar, &cached_fld) == GCC_GEN_BE_SUCCESS && cached_fld) {
                gcc_jit_lvalue *slot = gcc_jit_lvalue_access_field(frame_deref, NULL, (gcc_jit_field *)cached_fld);
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, pvar, slot));
            }
        }
    }
    for (a_variable_ptr nsv = scope->nonstatic_variables; nsv != NULL; nsv = nsv->next) {
        void *cached_fld = NULL;
        if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, nsv, &cached_fld) == GCC_GEN_BE_SUCCESS && cached_fld) {
            gcc_jit_lvalue *slot = gcc_jit_lvalue_access_field(frame_deref, NULL, (gcc_jit_field *)cached_fld);
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, nsv, slot));
        }
    }
    for (a_variable_ptr sv = scope->variables; sv != NULL; sv = sv->next) {
        if (!sv->is_parameter) {
            void *cached_fld = NULL;
            if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, sv, &cached_fld) == GCC_GEN_BE_SUCCESS && cached_fld) {
                gcc_jit_lvalue *slot = gcc_jit_lvalue_access_field(frame_deref, NULL, (gcc_jit_field *)cached_fld);
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, sv, slot));
            }
        }
    }
    if (scope->lifetime && scope->lifetime->destructions) {
        for (a_dynamic_init_ptr d = scope->lifetime->destructions; d != NULL; d = d->next_in_destruction_list) {
            if (d->variable) {
                void *cached_fld = NULL;
                if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, d->variable, &cached_fld) == GCC_GEN_BE_SUCCESS && cached_fld) {
                    gcc_jit_lvalue *slot = gcc_jit_lvalue_access_field(frame_deref, NULL, (gcc_jit_field *)cached_fld);
                    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, d->variable, slot));
                }
            }
        }
    }
    for (a_scope_ptr child = scope->scopes; child != NULL; child = child->next) {
        GCC_GEN_BE_CHECK(map_coro_scope_vars_to_frame(child, frame_deref));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively emits destructor calls for frame-promoted local variables in `scope` and its sub-scopes.
 * @details Inspects `scope->lifetime->destructions` in reverse construction order and invokes destructors
 * on corresponding frame field addresses in `block`.
 * @param[in] ctx The active libgccjit context.
 * @param[in] scope The EDG scope to inspect.
 * @param[in] frame_deref Dereferenced coroutine frame struct lvalue.
 * @param[in] block The basic block (`destroy_state_N`) where destructor calls are appended.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t emit_coro_scope_dtors(
    gcc_jit_context *ctx,
    a_scope_ptr scope,
    gcc_jit_lvalue *frame_deref,
    gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !scope || !frame_deref || !block) return GCC_GEN_BE_SUCCESS;

    for (a_scope_ptr child = scope->scopes; child != NULL; child = child->next) {
        GCC_GEN_BE_CHECK(emit_coro_scope_dtors(ctx, child, frame_deref, block));
    }
    if (scope->lifetime && scope->lifetime->destructions) {
        gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        int count = 0;
        for (a_dynamic_init_ptr d = scope->lifetime->destructions; d != NULL; d = d->next_in_destruction_list) {
            count++;
        }
        a_dynamic_init_ptr *dtor_list = (a_dynamic_init_ptr *)malloc((size_t)count * sizeof(a_dynamic_init_ptr));
        if (!dtor_list) return GCC_GEN_BE_ERROR_OOM;
        int idx = 0;
        for (a_dynamic_init_ptr d = scope->lifetime->destructions; d != NULL; d = d->next_in_destruction_list) {
            dtor_list[idx++] = d;
        }
        for (int i = count - 1; i >= 0; --i) {
            a_dynamic_init_ptr d = dtor_list[i];
            if (d->destructor && d->variable) {
                void *cached_fld = NULL;
                if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, d->variable, &cached_fld) == GCC_GEN_BE_SUCCESS && cached_fld) {
                    gcc_jit_function *dtor_fn = NULL;
                    gcc_gen_be_error_t err = gcc_gen_be_lower_function_decl(d->destructor, &dtor_fn);
                    if (err != GCC_GEN_BE_SUCCESS) {
                        free(dtor_list);
                        return err;
                    }
                    if (dtor_fn) {
                        gcc_jit_lvalue *var_slot = gcc_jit_lvalue_access_field(frame_deref, NULL, (gcc_jit_field *)cached_fld);
                        gcc_jit_rvalue *var_addr = gcc_jit_context_new_cast(
                            ctx, NULL, gcc_jit_lvalue_get_address(var_slot, NULL), void_ptr_type);
                        gcc_jit_block_add_eval(block, NULL, gcc_jit_context_new_call(ctx, NULL, dtor_fn, 1, &var_addr));
                    }
                }
            }
        }
        free(dtor_list);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Collects all local variables in a scope and sub-scopes into a struct fields array.
 * @details Recursively walks EDG frontend scope nodes to build a flat frame layout.
 * @param[in] scope The EDG scope pointer.
 * @param[in] ctx The libgccjit context.
 * @param[in,out] fields Pointer to array of gcc_jit_field pointers.
 * @param[in,out] num_fields Pointer to current number of fields.
 * @param[in,out] capacity Pointer to current capacity of the array.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on failure.
 */
static gcc_gen_be_error_t collect_coro_locals(
    a_scope_ptr scope,
    gcc_jit_context *ctx,
    gcc_jit_field ***fields,
    int *num_fields,
    int *capacity) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Retrieves the active coroutine frame struct type and frame pointer lvalue.
 * @details Used by `<coroutine>` intrinsics (`__builtin_coro_*`) and expression lowering.
 * @param[out] out_frame_type Pointer to receive the active coroutine frame `gcc_jit_type *` (or NULL).
 * @param[out] out_frame_lval Pointer to receive the active coroutine frame `gcc_jit_lvalue *` (or NULL).
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_get_active_coroutine_frame(
    struct gcc_jit_type **out_frame_type,
    struct gcc_jit_lvalue **out_frame_lval) GCC_GEN_BE_NOEXCEPT {
    if (!out_frame_type && !out_frame_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (out_frame_type) *out_frame_type = active_coro_frame_type;
    if (out_frame_lval) *out_frame_lval = active_coro_frame_lval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Registers a new suspension point (`co_await` or `co_yield`) in the active coroutine.
 * @details Increments the active coroutine suspension state counter, records `resume_block`
 * for the resume dispatcher switch table, and returns the assigned state index (`>= 1`).
 * @param[in] resume_block The basic block where execution resumes after suspension.
 * @param[out] out_state_idx Pointer to receive the assigned suspension state index (`0` if not in an active coroutine).
 * @param[out] out_state_lval Pointer to receive the lvalue of `frame->__suspend_index` (or NULL if not in an active coroutine).
 * @param[out] out_frame_ptr_rval Pointer to receive the `void *` address of the active coroutine frame (or NULL).
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_register_coro_suspend_point(
    struct gcc_jit_block *resume_block,
    int *out_state_idx,
    struct gcc_jit_lvalue **out_state_lval,
    struct gcc_jit_rvalue **out_frame_ptr_rval) GCC_GEN_BE_NOEXCEPT {
    if (!out_state_idx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_state_idx = 0;
    if (out_state_lval) *out_state_lval = NULL;
    if (out_frame_ptr_rval) *out_frame_ptr_rval = NULL;
    if (!resume_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    if (!active_coro_frame_lval || !active_coro_state_field) {
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    if (active_coro_resume_blocks_count >= active_coro_resume_blocks_capacity) {
        int new_cap = (active_coro_resume_blocks_capacity == 0) ? 8 : (active_coro_resume_blocks_capacity * 2);
        gcc_jit_block **new_arr = (gcc_jit_block **)realloc(
            active_coro_resume_blocks, (size_t)new_cap * sizeof(gcc_jit_block *));
        if (!new_arr) return GCC_GEN_BE_ERROR_OOM;
        active_coro_resume_blocks = new_arr;
        a_scope_ptr *new_scopes = (a_scope_ptr *)realloc(
            active_coro_suspend_scopes, (size_t)new_cap * sizeof(a_scope_ptr));
        if (!new_scopes) return GCC_GEN_BE_ERROR_OOM;
        active_coro_suspend_scopes = new_scopes;
        active_coro_resume_blocks_capacity = new_cap;
    }

    active_coro_suspend_scopes[active_coro_resume_blocks_count] = active_coro_current_scope;
    active_coro_resume_blocks[active_coro_resume_blocks_count++] = resume_block;
    active_coro_state_index = active_coro_resume_blocks_count;
    *out_state_idx = active_coro_state_index;

    if (out_state_lval) {
        *out_state_lval = gcc_jit_lvalue_access_field(
            gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(active_coro_frame_lval), NULL),
            NULL,
            active_coro_state_field);
    }
    if (out_frame_ptr_rval) {
        gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        *out_frame_ptr_rval = gcc_jit_context_new_cast(
            ctx, NULL, gcc_jit_lvalue_as_rvalue(active_coro_frame_lval), void_ptr_type);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Begins tracking statement generation for a function.
 * @details Resets global state for computed gotos.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_begin_function_stmts(void) GCC_GEN_BE_NOEXCEPT {
    computed_goto_labels_count = 0;
    computed_goto_dispatcher_block = NULL;
    computed_goto_target_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Finalizes the computed goto (`&&label` / `goto *ptr`) switch dispatcher for a function.
 * @details Synthesizes the switch dispatcher block over all address-taken labels in `func`,
 * supporting functions with multiple `goto *` statements, label address comparisons (`&&l1 == &&l2`),
 * and static array tables of label addresses.
 * @param[in] func The `gcc_jit_function` being finalized.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_finalize_computed_goto(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (!computed_goto_dispatcher_block) {
        computed_goto_labels_count = 0;
        return GCC_GEN_BE_SUCCESS;
    }
    
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    
    gcc_jit_block *default_block = gcc_jit_function_new_block(func, "computed_goto_default");
    gcc_jit_function *trap_func = gcc_jit_context_get_builtin_function(ctx, "__builtin_trap");
    if (trap_func) {
        gcc_jit_block_add_eval(default_block, NULL, gcc_jit_context_new_call(ctx, NULL, trap_func, 0, NULL));
    }
    gcc_jit_type *fn_ret_t = gcc_jit_function_get_return_type(func);
    if (fn_ret_t == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
        gcc_jit_block_end_with_void_return(default_block, NULL);
    } else {
        gcc_jit_rvalue *zero_ret = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(NULL, fn_ret_t, &zero_ret));
        gcc_jit_block_end_with_return(default_block, NULL, zero_ret);
    }
    
    gcc_jit_case **cases = NULL;
    if (computed_goto_labels_count > 0) {
        cases = (gcc_jit_case **)malloc((size_t)computed_goto_labels_count * sizeof(gcc_jit_case *));
        if (!cases) return GCC_GEN_BE_ERROR_OOM;
        gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
        for (int i = 0; i < computed_goto_labels_count; ++i) {
            a_label_ptr lbl = computed_goto_labels[i];
            gcc_jit_block *lbl_block = NULL;
            gcc_gen_be_error_t err = gcc_gen_be_get_label_block(func, lbl, &lbl_block);
            if (err != GCC_GEN_BE_SUCCESS) {
                free(cases);
                return err;
            }
            gcc_jit_rvalue *val = gcc_jit_context_new_rvalue_from_long(ctx, long_type, (long)(uintptr_t)lbl);
            cases[i] = gcc_jit_context_new_case(ctx, val, val, lbl_block);
        }
    }
    
    gcc_jit_rvalue *long_target = gcc_jit_lvalue_as_rvalue(computed_goto_target_lval);
    
    gcc_jit_block_end_with_switch(computed_goto_dispatcher_block, NULL, long_target, default_block, computed_goto_labels_count, cases);
    
    if (cases) free(cases);
    computed_goto_dispatcher_block = NULL;
    computed_goto_target_lval = NULL;
    computed_goto_labels_count = 0;
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Ends tracking statement generation for a function.
 * @details Synthesizes the computed goto dispatcher block if needed via `gcc_gen_be_finalize_computed_goto`.
 * @param[in] func The function being finalized.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_end_function_stmts(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!computed_goto_dispatcher_block) {
        computed_goto_labels_count = 0;
        return GCC_GEN_BE_SUCCESS;
    }
    return gcc_gen_be_finalize_computed_goto(func);
}

/**
 * @brief Gets or creates a guard variable for thread-safe initialization.
 * @details Looks up or creates a unique guard variable for the given static local.
 * @param[in] ctx The libgccjit context.
 * @param[in] var The frontend variable representing the static local.
 * @param[out] out_guard Pointer to receive the lvalue of the guard.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t gcc_gen_be_get_or_create_guard(gcc_jit_context *ctx, a_variable_ptr var, gcc_jit_lvalue **out_guard) GCC_GEN_BE_NOEXCEPT {
    if (!out_guard) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_guard = NULL;

    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_GUARD, var, &cached);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    if (cached) {
        *out_guard = (gcc_jit_lvalue *)cached;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_type *long_long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG);
    
    char guard_name[128];
    snprintf(guard_name, sizeof(guard_name), "_ZGV_local_guard_%p", (void *)var);

    struct gcc_jit_lvalue *guard = gcc_jit_context_new_global(
        ctx,
        NULL,
        GCC_JIT_GLOBAL_INTERNAL,
        long_long_type,
        guard_name
    );
    if (p_gcc_jit_lvalue_add_string_attribute) {
        p_gcc_jit_lvalue_add_string_attribute(guard, (enum gcc_jit_variable_attribute)0, "default");
    }

    err = cache_insert(GCC_GEN_BE_CACHE_GUARD, var, guard);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    *out_guard = guard;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves a __cxa_guard function.
 *
 * @details Finds or creates the declaration for the requested guard function.
 *
 * @param[in] ctx The libgccjit context.
 * @param[in] name The name of the function to retrieve.
 * @param[in] return_type The expected return type.
 * @param[out] out_func Pointer to receive the function pointer.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t get_cxa_guard_func(gcc_jit_context *ctx, const char *name, gcc_jit_type *return_type, gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)name, &cached);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    if (cached) {
        *out_func = (gcc_jit_function *)cached;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_type *guard_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG));
    gcc_jit_param *param = gcc_jit_context_new_param(ctx, NULL, guard_ptr_type, "g");
    
    gcc_jit_function *func = gcc_jit_context_new_function(
        ctx,
        NULL,
        GCC_JIT_FUNCTION_IMPORTED,
        return_type,
        name,
        1,
        &param,
        0
    );

    err = cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)name, func);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    *out_func = func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers a Microsoft Structured Exception Handling (`__try` / `__except` / `__finally`) statement (`stmk_microsoft_try`).
 * @details Supports abnormal termination tracking (`_abnormal_termination()`), early-exit `__finally`
 * execution (`return`, `break`, `continue`, `goto`), 3-way `__except` filter dispatch (`1`, `0`, `-1`),
 * `_exception_code()` / `_exception_info()` intrinsic bindings, and Win64 `.pdata`/`.xdata` table emission.
 * @param[in] stmt The `stmk_microsoft_try` statement node.
 * @param[in] func The active `gcc_jit_function`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_microsoft_try(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!stmt || !func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
#if MICROSOFT_EXTENSIONS_ALLOWED
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_stmt(stmt, &loc));

    a_microsoft_try_supplement_ptr mtsp = stmt->variant.microsoft_try;
    if (!mtsp) return GCC_GEN_BE_SUCCESS;

    extern gcc_gen_be_error_t gcc_gen_be_emit_seh_unwind_tables(
        struct gcc_jit_function *fn,
        a_boolean has_except_handler,
        struct gcc_jit_lvalue **out_xdata_global) GCC_GEN_BE_NOEXCEPT;
    gcc_jit_lvalue *seh_xdata = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_seh_unwind_tables(func, mtsp->except_expr != NULL ? TRUE : FALSE, &seh_xdata));

    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *ulong_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG);

    if (mtsp->except_expr) {
        /* __try / __except with 3-way filter dispatch:
           EXCEPTION_EXECUTE_HANDLER = 1, EXCEPTION_CONTINUE_SEARCH = 0, EXCEPTION_CONTINUE_EXECUTION = -1 */
        gcc_jit_block *guarded_block = gcc_jit_function_new_block(func, "seh_try_body");
        gcc_jit_block *filter_block = gcc_jit_function_new_block(func, "seh_except_filter");
        gcc_jit_block *handler_block = gcc_jit_function_new_block(func, "seh_except_handler");
        gcc_jit_block *search_block = gcc_jit_function_new_block(func, "seh_continue_search");
        gcc_jit_block *end_block = gcc_jit_function_new_block(func, "seh_end");

        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));

        /* Bind _exception_code() and _exception_info() local variables for filter and handler */
        gcc_jit_lvalue *exc_code_lval = gcc_jit_function_new_local(func, NULL, ulong_type, "__seh_exc_code");
        gcc_jit_lvalue *exc_info_lval = gcc_jit_function_new_local(func, NULL, void_ptr_type, "__seh_exc_info");
        gcc_jit_block_add_assignment(cblock, loc, exc_code_lval, gcc_jit_context_new_rvalue_from_long(ctx, ulong_type, 0xC0000005L));
        gcc_jit_block_add_assignment(cblock, loc, exc_info_lval, gcc_jit_context_null(ctx, void_ptr_type));

        void *prev_code = NULL;
        void *prev_info = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_exception_code", &prev_code));
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_exception_info", &prev_info));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_exception_code", exc_code_lval));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_exception_info", exc_info_lval));

        /* Wire setjmp/unwind dispatch so both guarded_block and filter_block are reachable */
        void *cached_setjmp = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", &cached_setjmp));
        if (!cached_setjmp) {
            gcc_jit_param *jmpbuf_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "env");
            gcc_jit_function *setjmp_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "setjmp", 1, &jmpbuf_param, 0);
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", setjmp_fn));
            cached_setjmp = setjmp_fn;
        }
        gcc_jit_type *jmpbuf_arr_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, 32);
        gcc_jit_lvalue *seh_jmpbuf = gcc_jit_function_new_local(func, NULL, jmpbuf_arr_type, "seh_jmpbuf");
        gcc_jit_rvalue *jmpbuf_ptr = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(seh_jmpbuf, NULL), void_ptr_type);
        gcc_jit_rvalue *setjmp_res = gcc_jit_context_new_call(ctx, loc, (gcc_jit_function *)cached_setjmp, 1, &jmpbuf_ptr);
        gcc_jit_rvalue *is_normal = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_EQ, setjmp_res, gcc_jit_context_zero(ctx, int_type));
        gcc_jit_block_end_with_conditional(cblock, loc, is_normal, guarded_block, filter_block);

        GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(filter_block));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(guarded_block));
        if (mtsp->guarded_statement) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(mtsp->guarded_statement, func));
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
        gcc_jit_block *after_guarded = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_guarded));
        if (after_guarded) {
            gcc_jit_block_end_with_jump(after_guarded, NULL, end_block);
        }

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(filter_block));
        /* Evaluate except_expr filter and dispatch (>0 handler, <0 continue execution, 0 continue search) */
        gcc_jit_rvalue *filter_res = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(mtsp->except_expr, &filter_res));
        if (!filter_res) {
            filter_res = gcc_jit_context_one(ctx, int_type);
        } else if (gcc_jit_rvalue_get_type(filter_res) != int_type) {
            filter_res = gcc_jit_context_new_cast(ctx, loc, filter_res, int_type);
        }
        gcc_jit_block *after_filter = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_filter));
        gcc_jit_rvalue *is_exec_handler = gcc_jit_context_new_comparison(
            ctx, loc, GCC_JIT_COMPARISON_GT, filter_res, gcc_jit_context_zero(ctx, int_type));
        gcc_jit_block_end_with_conditional(after_filter, loc, is_exec_handler, handler_block, search_block);

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(search_block));
        gcc_jit_block *outer_seh_lp = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&outer_seh_lp));
        gcc_jit_rvalue *is_cont_exec = gcc_jit_context_new_comparison(
            ctx, loc, GCC_JIT_COMPARISON_LT, filter_res, gcc_jit_context_zero(ctx, int_type));
        gcc_jit_block_end_with_conditional(
            search_block, loc, is_cont_exec, end_block, outer_seh_lp ? outer_seh_lp : end_block);

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(handler_block));
        if (mtsp->cleanup_statement) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(mtsp->cleanup_statement, func));
        }
        gcc_jit_block *after_handler = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_handler));
        if (after_handler) {
            gcc_jit_block_end_with_jump(after_handler, NULL, end_block);
        }

        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_exception_code", prev_code));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_exception_info", prev_info));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(end_block));
    } else {
        /* __try / __finally: maintain abnormal termination flag (0 on normal fallthrough, 1 on unwind/early exit) */
        gcc_jit_block *guarded_block = gcc_jit_function_new_block(func, "seh_finally_try");
        gcc_jit_block *unwind_finally_block = gcc_jit_function_new_block(func, "seh_finally_unwind_lp");
        gcc_jit_block *finally_block = gcc_jit_function_new_block(func, "seh_finally_cleanup");
        gcc_jit_block *end_block = gcc_jit_function_new_block(func, "seh_finally_end");

        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));

        gcc_jit_lvalue *abn_flag = gcc_jit_function_new_local(func, NULL, int_type, "__seh_abnormal_term");
        gcc_jit_block_add_assignment(cblock, loc, abn_flag, gcc_jit_context_zero(ctx, int_type));

        void *cached_setjmp = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", &cached_setjmp));
        if (!cached_setjmp) {
            gcc_jit_param *jmpbuf_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "env");
            gcc_jit_function *setjmp_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "setjmp", 1, &jmpbuf_param, 0);
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", setjmp_fn));
            cached_setjmp = setjmp_fn;
        }
        gcc_jit_type *jmpbuf_arr_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, 32);
        gcc_jit_lvalue *seh_jmpbuf = gcc_jit_function_new_local(func, NULL, jmpbuf_arr_type, "seh_finally_jmpbuf");
        gcc_jit_rvalue *jmpbuf_ptr = gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_get_address(seh_jmpbuf, NULL), void_ptr_type);
        gcc_jit_rvalue *setjmp_res = gcc_jit_context_new_call(ctx, loc, (gcc_jit_function *)cached_setjmp, 1, &jmpbuf_ptr);
        gcc_jit_rvalue *is_normal = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_EQ, setjmp_res, gcc_jit_context_zero(ctx, int_type));
        gcc_jit_block_end_with_conditional(cblock, loc, is_normal, guarded_block, unwind_finally_block);

        /* Push active __finally entry so return/goto inside __try run __finally with abn_flag = 1 */
        gcc_gen_be_seh_finally_entry_t finally_entry;
        finally_entry.cleanup_statement = mtsp->cleanup_statement;
        finally_entry.abnormal_flag_lval = abn_flag;
        finally_entry.prev = active_seh_finally_stack;
        active_seh_finally_stack = &finally_entry;

        GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(unwind_finally_block));
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(guarded_block));
        gcc_gen_be_error_t guarded_err = GCC_GEN_BE_SUCCESS;
        if (mtsp->guarded_statement) {
            guarded_err = gcc_gen_be_lower_statement(mtsp->guarded_statement, func);
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
        active_seh_finally_stack = finally_entry.prev;
        if (guarded_err != GCC_GEN_BE_SUCCESS) return guarded_err;

        gcc_jit_block *after_guarded = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_guarded));
        if (after_guarded) {
            gcc_jit_block_add_assignment(after_guarded, loc, abn_flag, gcc_jit_context_zero(ctx, int_type));
            gcc_jit_block_end_with_jump(after_guarded, NULL, finally_block);
        }

        /* Unwind path sets abn_flag = 1 and jumps to finally_block */
        gcc_jit_block_add_assignment(unwind_finally_block, loc, abn_flag, gcc_jit_context_one(ctx, int_type));
        gcc_jit_block_end_with_jump(unwind_finally_block, loc, finally_block);

        void *prev_abn = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_abnormal_termination", &prev_abn));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_abnormal_termination", abn_flag));

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(finally_block));
        if (mtsp->cleanup_statement) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(mtsp->cleanup_statement, func));
        }
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_abnormal_termination", prev_abn));

        gcc_jit_block *after_finally = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_finally));
        if (after_finally) {
            gcc_jit_block *outer_seh_lp = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&outer_seh_lp));
            if (outer_seh_lp) {
                gcc_jit_rvalue *is_abn = gcc_jit_context_new_comparison(
                    ctx, loc, GCC_JIT_COMPARISON_NE, gcc_jit_lvalue_as_rvalue(abn_flag), gcc_jit_context_zero(ctx, int_type));
                gcc_jit_block_end_with_conditional(after_finally, loc, is_abn, outer_seh_lp, end_block);
            } else {
                gcc_jit_block_end_with_jump(after_finally, loc, end_block);
            }
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(end_block));
    }
#endif
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers a declaration statement.
 * @details Processes local variable declarations in the statement entity list.
 * @param[in] stmt The frontend statement.
 * @param[in] func The containing gcc_jit_function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t gcc_gen_be_lower_statement_decl(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!stmt || !func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    
    an_il_entity_list_entry_ptr ent = stmt->variant.decl.entities;
    while (ent) {
        if (ent->entity.kind == iek_variable) {
            a_variable_ptr var = (a_variable_ptr)ent->entity.ptr;
            if (!var->is_parameter && var->storage_class != sc_extern && !var->is_thread_local) {
                struct gcc_jit_lvalue *local = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, var, &local));
            }
        }
        ent = ent->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_get_or_create_label_block(
    a_label_ptr label,
    struct gcc_jit_function *func,
    struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;
    if (!label) return GCC_GEN_BE_ERROR_LABEL_RESOLUTION_FAILED;

    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_LABEL, label, &cached);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    if (!cached) {
        if (!func) {
            gcc_jit_block *cblock = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
            if (cblock) func = gcc_jit_block_get_function(cblock);
        }
        if (!func) return GCC_GEN_BE_ERROR_LABEL_RESOLUTION_FAILED;

        const char *name = "label";
        if (label->source_corresp.name) {
            name = label->source_corresp.name;
        } else if (label->break_label) {
            name = "break";
        } else if (label->continue_label) {
            name = "continue";
        }
        gcc_jit_block *block = gcc_jit_function_new_block(func, name);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_LABEL, label, block));
        if (computed_goto_labels_count == computed_goto_labels_capacity) {
            computed_goto_labels_capacity = computed_goto_labels_capacity ? computed_goto_labels_capacity * 2 : 16;
            computed_goto_labels = (a_label_ptr *)realloc(
                computed_goto_labels, (size_t)computed_goto_labels_capacity * sizeof(a_label_ptr));
        }
        computed_goto_labels[computed_goto_labels_count++] = label;
        *out_block = block;
    } else {
        *out_block = (gcc_jit_block *)cached;
        a_boolean found_lbl = FALSE;
        for (int i = 0; i < computed_goto_labels_count; ++i) {
            if (computed_goto_labels[i] == label) {
                found_lbl = TRUE;
                break;
            }
        }
        if (!found_lbl) {
            if (computed_goto_labels_count == computed_goto_labels_capacity) {
                computed_goto_labels_capacity = computed_goto_labels_capacity ? computed_goto_labels_capacity * 2 : 16;
                computed_goto_labels = (a_label_ptr *)realloc(
                    computed_goto_labels, (size_t)computed_goto_labels_capacity * sizeof(a_label_ptr));
            }
            computed_goto_labels[computed_goto_labels_count++] = label;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_get_label_block(struct gcc_jit_function *func, a_label_ptr label, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;
    if (!label) return GCC_GEN_BE_SUCCESS;
    return gcc_gen_be_get_or_create_label_block(label, func, out_block);
}

gcc_gen_be_error_t gcc_gen_be_get_switch_case_block(struct gcc_jit_function *func, a_switch_case_entry_ptr scep, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;
    if (!scep) return GCC_GEN_BE_SUCCESS;
    
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_LABEL, scep, &cached);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    if (!cached) {
        gcc_jit_block *block = gcc_jit_function_new_block(func, scep->case_value ? "case" : "default");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_LABEL, scep, block));
        *out_block = block;
    } else {
        *out_block = (gcc_jit_block *)cached;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  gcc_gen_be_error_t err;

  while (stmt) {
      gcc_jit_block *current_block = NULL;
      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
      
      gcc_jit_location *loc = NULL;
      GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_stmt(stmt, &loc));
      if (loc && current_block) {
          /* Attach statement source location via a DWARF-anchored block comment
             without emitting redundant null-pointer evaluation instructions. */
          gcc_jit_block_add_comment(current_block, loc, "stmt");
      }
      if (!current_block) {
          if (stmt->kind == stmk_label) {
              gcc_jit_block *lbl_block = NULL;
              err = gcc_gen_be_get_label_block(func, stmt->variant.label.ptr, &lbl_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(lbl_block));
              current_block = lbl_block;
          } else if (stmt->kind == stmk_switch_case) {
              gcc_jit_block *case_block = NULL;
              err = gcc_gen_be_get_switch_case_block(func, stmt->variant.switch_case.extra_info, &case_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(case_block));
              current_block = case_block;
          } else {
              stmt = stmt->next;
              continue;
          }
      } else {
          if (stmt->kind == stmk_label) {
              gcc_jit_block *label_block = NULL;
              err = gcc_gen_be_get_label_block(func, stmt->variant.label.ptr, &label_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              gcc_jit_block_end_with_jump(current_block, loc, label_block);
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(label_block));
              current_block = label_block;
          } else if (stmt->kind == stmk_switch_case) {
              gcc_jit_block *case_block = NULL;
              err = gcc_gen_be_get_switch_case_block(func, stmt->variant.switch_case.extra_info, &case_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              gcc_jit_block_end_with_jump(current_block, loc, case_block);
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(case_block));
              current_block = case_block;
          }
      }

      switch (stmt->kind) {
          case stmk_block:
              {
                  an_object_lifetime_ptr lifetime = NULL;
                  a_scope_ptr bscope = NULL;
                  if (stmt->variant.block.extra_info) {
                      if (stmt->variant.block.extra_info->assoc_scope) {
                          bscope = stmt->variant.block.extra_info->assoc_scope;
                          lifetime = bscope->lifetime;
                      } else {
                          lifetime = stmt->variant.block.extra_info->lifetime;
                      }
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(lifetime));
                  gcc_jit_lvalue *vla_saved_stack = NULL;
                  a_boolean scope_has_vla = FALSE;
                  if (bscope) {
                      for (a_variable_ptr sv = bscope->variables; sv != NULL; sv = sv->next) {
                          if (!sv->is_parameter && sv->has_variably_modified_type) scope_has_vla = TRUE;
                      }
                      for (a_variable_ptr nsv = bscope->nonstatic_variables; nsv != NULL; nsv = nsv->next) {
                          if (!nsv->is_parameter && nsv->has_variably_modified_type) scope_has_vla = TRUE;
                      }
                  }
                  if (scope_has_vla && current_block) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_emit_stack_save(func, current_block, &vla_saved_stack));
                  }
                  if (bscope) {
#ifndef EDG_GCC_BE_ISOLATED_TEST
                      for (a_local_static_variable_init_ptr lsvip = bscope->local_static_variable_inits;
                           lsvip != NULL;
                           lsvip = lsvip->next) {
                          if (lsvip->variable && lsvip->variable->init_kind == initk_function_local) {
                              lsvip->variable->init_kind = lsvip->init_kind;
                              lsvip->variable->initializer = lsvip->initializer;
                          }
                      }
#endif
                      for (a_variable_ptr sv = bscope->variables; sv != NULL; sv = sv->next) {
                          if (!sv->is_parameter && !sv->has_variably_modified_type &&
                              !(sv->storage_class == sc_static && !sv->source_corresp.name && sv->init_kind == initk_static)) {
                              gcc_jit_lvalue *sv_lval = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, sv, &sv_lval));
                          }
                      }
                      for (a_variable_ptr nsv = bscope->nonstatic_variables; nsv != NULL; nsv = nsv->next) {
                          if (!nsv->is_parameter && !nsv->has_variably_modified_type) {
                              gcc_jit_lvalue *nsv_lval = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, nsv, &nsv_lval));
                          }
                      }
                  }
                  if (exceptions_enabled && lifetime && lifetime->destructions) {
                      a_boolean any_cleanup_reg = FALSE;
                      for (a_dynamic_init_ptr d = lifetime->destructions; d != NULL; d = d->next_in_destruction_list) {
                          if (d->destructor && d->variable) {
                              gcc_jit_lvalue *d_lval = NULL;
                              gcc_jit_function *d_fn = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, d->variable, &d_lval));
                              gcc_jit_block *saved_b = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_b));
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(d->destructor, &d_fn));
                              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_b));
                              if (d_lval && d_fn) {
                                  if (gcc_gen_be_register_cleanup(d_lval, d_fn) == GCC_GEN_BE_SUCCESS) {
                                      any_cleanup_reg = TRUE;
                                  }
                              }
                          }
                      }
                      if (any_cleanup_reg) {
                          extern gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT;
                          GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(func));
                      }
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  }
                  if (stmt->variant.block.statements) {
                      err = gcc_gen_be_lower_statement(stmt->variant.block.statements, func);
                      if (err != GCC_GEN_BE_SUCCESS) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));
                          return err;
                      }
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  if (vla_saved_stack && current_block) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_emit_stack_restore(func, current_block, vla_saved_stack));
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(current_block));
              }
              break;

          case stmk_expr:
              if (stmt->expr) {
                  gcc_jit_rvalue *rval = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &rval);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  if (rval && current_block) gcc_jit_block_add_eval(current_block, loc, rval);
              }
              break;

          case stmk_stmt_expr_result:
              {
                  extern struct gcc_jit_lvalue *current_stmt_expr_result_lval;
                  if (current_stmt_expr_result_lval) {
                      gcc_jit_rvalue *res_val = NULL;
                      if (stmt->expr) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(stmt->expr, &res_val));
                          if (res_val) {
                              gcc_jit_block *cblock = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                              if (cblock) {
                                  gcc_jit_block_add_assignment(cblock, loc, current_stmt_expr_result_lval, res_val);
                              }
                          }
                      } else if (stmt->variant.stmt_expr_result.dynamic_init) {
                          a_dynamic_init_ptr dip = stmt->variant.stmt_expr_result.dynamic_init;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(dip, current_stmt_expr_result_lval, &res_val));
                      }
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
          case stmk_coroutine:
              {
                  /* Coroutine frame allocation and dispatcher synthesis */
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  a_coroutine_descr_ptr descr = stmt->variant.coroutine.descr;
                  active_coro_descr = descr;
                  active_coro_state_index = 0;
                  active_coro_resume_blocks_count = 0;

                  /* Layout coroutine heap frame: resume fn, destroy fn, promise, state index, initial_await_resume_called, flags, and spilled locals */
                  gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                  gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);

                  gcc_jit_type *resume_func_type = gcc_jit_context_new_function_ptr_type(ctx, NULL, void_type, 1, &void_ptr_type, 0);
                  gcc_jit_type *promise_type = void_ptr_type;
                  if (descr && descr->promise && descr->promise->type) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(descr->promise->type, &promise_type));
                  }

                  gcc_jit_field *f_resume = gcc_jit_context_new_field(ctx, NULL, resume_func_type, "__resume_fn");
                  gcc_jit_field *f_destroy = gcc_jit_context_new_field(ctx, NULL, resume_func_type, "__destroy_fn");
                  gcc_jit_field *f_promise = gcc_jit_context_new_field(ctx, NULL, promise_type, "__promise");
                  gcc_jit_field *f_state = gcc_jit_context_new_field(ctx, NULL, int_type, "__suspend_index");
                  gcc_jit_field *f_init_await = gcc_jit_context_new_field(ctx, NULL, bool_type, "initial_await_resume_called");
                  gcc_jit_field *f_flags = gcc_jit_context_new_field(ctx, NULL, int_type, "__coro_flags");

                  int num_fields = 6;
                  int capacity = 16;
                  gcc_jit_field **frame_fields = (gcc_jit_field **)malloc((size_t)capacity * sizeof(gcc_jit_field *));
                  if (!frame_fields) return GCC_GEN_BE_ERROR_OOM;
                  frame_fields[0] = f_resume;
                  frame_fields[1] = f_destroy;
                  frame_fields[2] = f_promise;
                  frame_fields[3] = f_state;
                  frame_fields[4] = f_init_await;
                  frame_fields[5] = f_flags;

                  if (descr && descr->parameter_copies) {
                      for (a_variable_ptr pcopy = descr->parameter_copies; pcopy != NULL; pcopy = pcopy->next) {
                          if (pcopy->type) {
                              gcc_jit_type *ptype = NULL;
                              if (gcc_gen_be_lower_type(pcopy->type, &ptype) == GCC_GEN_BE_SUCCESS && ptype) {
                                  if (num_fields >= capacity) {
                                      capacity *= 2;
                                      gcc_jit_field **nf = (gcc_jit_field **)realloc(frame_fields, (size_t)capacity * sizeof(gcc_jit_field *));
                                      if (!nf) { free(frame_fields); return GCC_GEN_BE_ERROR_OOM; }
                                      frame_fields = nf;
                                  }
                                  char pname[128];
                                  snprintf(pname, sizeof(pname), "param_%p", (void *)pcopy);
                                  gcc_jit_field *pfld = gcc_jit_context_new_field(ctx, NULL, ptype, pname);
                                  frame_fields[num_fields++] = pfld;
                                  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, pcopy, pfld));
                              }
                          }
                      }
                  }

                  extern struct a_scope *innermost_function_scope;
                  if (innermost_function_scope) {
                      gcc_gen_be_error_t cerr = collect_coro_locals(innermost_function_scope, ctx, &frame_fields, &num_fields, &capacity);
                      if (cerr != GCC_GEN_BE_SUCCESS) { free(frame_fields); return cerr; }
                  }

                  gcc_jit_struct *frame_struct = gcc_jit_context_new_struct_type(ctx, NULL, "coroutine_frame", num_fields, frame_fields);
                  free(frame_fields);
                  gcc_jit_type *frame_struct_type = gcc_jit_struct_as_type(frame_struct);
                  gcc_jit_type *frame_struct_ptr_type = gcc_jit_type_get_pointer(frame_struct_type);
                  active_coro_frame_type = frame_struct_type;
                  active_coro_state_field = f_state;

                  /* Allocate frame on the heap using promise_type::operator new or global ::operator new (_Znwm) */
                  gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
                  gcc_jit_rvalue *frame_size = gcc_jit_context_new_cast(ctx, loc, gcc_jit_context_new_sizeof(ctx, frame_struct_type), size_t_type);
                  gcc_jit_rvalue *frame_ptr = NULL;
                  if (descr && descr->new_routine) {
                      gcc_jit_function *op_new_fn = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(descr->new_routine, &op_new_fn));
                      int new_pcount = (int)gcc_jit_function_get_param_count(op_new_fn);
                      int ramp_pcount = (int)gcc_jit_function_get_param_count(func);
                      int argc = new_pcount > 1 ? new_pcount : 1;
                      gcc_jit_rvalue **new_args = (gcc_jit_rvalue **)malloc((size_t)argc * sizeof(gcc_jit_rvalue *));
                      if (!new_args) return GCC_GEN_BE_ERROR_OOM;
                      new_args[0] = frame_size;
                      for (int p = 1; p < argc; ++p) {
                          gcc_jit_param *np = gcc_jit_function_get_param(op_new_fn, p);
                          gcc_jit_type *nptype = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(np));
                          if (p - 1 < ramp_pcount) {
                              gcc_jit_rvalue *rp_val = gcc_jit_param_as_rvalue(gcc_jit_function_get_param(func, p - 1));
                              new_args[p] = gcc_jit_context_new_cast(ctx, loc, rp_val, nptype);
                          } else {
                              new_args[p] = gcc_jit_context_zero(ctx, nptype);
                          }
                      }
                      frame_ptr = gcc_jit_context_new_call(ctx, loc, op_new_fn, argc, new_args);
                      free(new_args);
                  } else {
                      gcc_jit_param *op_new_param = gcc_jit_context_new_param(ctx, NULL, size_t_type, "size");
                      gcc_jit_function *op_new_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "_Znwm", 1, &op_new_param, 0);
                      frame_ptr = gcc_jit_context_new_call(ctx, loc, op_new_func, 1, &frame_size);
                  }

                  gcc_jit_lvalue *coro_frame = gcc_jit_function_new_local(func, NULL, frame_struct_ptr_type, "coro_frame");
                  active_coro_frame_lval = coro_frame;
                  gcc_jit_rvalue *cast_frame_ptr = gcc_jit_context_new_cast(ctx, NULL, frame_ptr, frame_struct_ptr_type);
                  gcc_jit_block_add_assignment(cblock, loc, coro_frame, cast_frame_ptr);

                  /* Handle get_return_object_on_allocation_failure() when present */
                  if (descr && descr->alloc_failure_gro_call) {
                      gcc_jit_block *alloc_ok_block = gcc_jit_function_new_block(func, "coro_alloc_ok");
                      gcc_jit_block *alloc_fail_block = gcc_jit_function_new_block(func, "coro_alloc_fail");
                      gcc_jit_rvalue *is_null = gcc_jit_context_new_comparison(
                          ctx, loc, GCC_JIT_COMPARISON_EQ,
                          gcc_jit_context_new_cast(ctx, loc, gcc_jit_lvalue_as_rvalue(coro_frame), void_ptr_type),
                          gcc_jit_context_null(ctx, void_ptr_type));
                      gcc_jit_block_end_with_conditional(cblock, loc, is_null, alloc_fail_block, alloc_ok_block);

                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(alloc_fail_block));
                      gcc_jit_rvalue *fail_gro_rval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(descr->alloc_failure_gro_call, &fail_gro_rval));
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&alloc_fail_block));
                      if (gcc_jit_function_get_return_type(func) == void_type) {
                          if (fail_gro_rval) gcc_jit_block_add_eval(alloc_fail_block, loc, fail_gro_rval);
                          gcc_jit_block_end_with_void_return(alloc_fail_block, loc);
                      } else {
                          if (!fail_gro_rval) fail_gro_rval = gcc_jit_context_zero(ctx, gcc_jit_function_get_return_type(func));
                          gcc_jit_block_end_with_return(alloc_fail_block, loc, fail_gro_rval);
                      }

                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(alloc_ok_block));
                      cblock = alloc_ok_block;
                  }

                  /* Map init_await_resume, handle, and nested scope variables to frame fields */
                  gcc_jit_lvalue *frame_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(coro_frame), NULL);
                  active_coro_promise_field = f_promise;
                  if (innermost_function_scope) {
                      GCC_GEN_BE_CHECK(map_coro_scope_vars_to_frame(innermost_function_scope, frame_deref));
                  }
                  gcc_jit_lvalue *init_await_field = gcc_jit_lvalue_access_field(frame_deref, NULL, f_init_await);
                  gcc_jit_block_add_assignment(cblock, loc, init_await_field, gcc_jit_context_new_rvalue_from_int(ctx, bool_type, 0));
                  gcc_jit_lvalue *flags_field = gcc_jit_lvalue_access_field(frame_deref, NULL, f_flags);
                  gcc_jit_block_add_assignment(cblock, loc, flags_field, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0));
                  if (descr && descr->init_await_resume) {
                      GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, descr->init_await_resume, init_await_field));
                  }
                  if (descr && descr->handle) {
                      gcc_jit_type *handle_type = void_ptr_type;
                      if (descr->handle->type) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(descr->handle->type, &handle_type));
                      }
                      gcc_jit_lvalue *handle_lval = gcc_jit_function_new_local(func, NULL, handle_type, "coro_handle");
                      GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, descr->handle, handle_lval));
                  }

                  /* Copy coroutine parameters into frame fields */
                  if (descr && descr->parameter_copies) {
                      int ramp_pcount = (int)gcc_jit_function_get_param_count(func);
                      int p_idx = 0;
                      for (a_variable_ptr pcopy = descr->parameter_copies; pcopy != NULL; pcopy = pcopy->next) {
                          void *cached_fld = NULL;
                          GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, pcopy, &cached_fld));
                          if (cached_fld) {
                              gcc_jit_field *pfld = (gcc_jit_field *)cached_fld;
                              gcc_jit_lvalue *param_slot = gcc_jit_lvalue_access_field(frame_deref, NULL, pfld);
                              if (p_idx < ramp_pcount) {
                                  gcc_jit_rvalue *rp_val = gcc_jit_param_as_rvalue(gcc_jit_function_get_param(func, p_idx));
                                  gcc_jit_block_add_assignment(
                                      cblock, loc, param_slot,
                                      gcc_jit_context_new_cast(ctx, loc, rp_val, gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(param_slot))));
                              }
                              GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, pcopy, param_slot));
                          }
                          p_idx++;
                      }
                  }

                  /* Construct promise object inside the coroutine frame (f_promise) */
                  gcc_jit_lvalue *promise_field = gcc_jit_lvalue_access_field(frame_deref, NULL, f_promise);
                  if (descr && descr->promise) {
                      GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, descr->promise, promise_field));
                      if (descr->promise->init_kind == initk_dynamic && descr->promise->initializer.dynamic) {
                          gcc_jit_rvalue *prom_init = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(descr->promise->initializer.dynamic, promise_field, &prom_init));
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                      } else {
                          gcc_jit_rvalue *zero_prom = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(descr->promise->type, promise_type, &zero_prom));
                          if (zero_prom) gcc_jit_block_add_assignment(cblock, loc, promise_field, zero_prom);
                      }
                  } else {
                      gcc_jit_rvalue *null_promise = gcc_jit_context_null(ctx, void_ptr_type);
                      gcc_jit_block_add_assignment(cblock, loc, promise_field, null_promise);
                  }

                  /* Synthesize resume and destroy function dispatchers: switch on state index */
                  gcc_jit_block *coro_resume = gcc_jit_function_new_block(func, "coro_resume_dispatcher");
                  gcc_jit_block *coro_destroy = gcc_jit_function_new_block(func, "coro_destroy_dispatcher");

                  gcc_jit_lvalue *state_field = gcc_jit_lvalue_access_field(frame_deref, NULL, f_state);
                  gcc_jit_block_add_assignment(cblock, loc, state_field, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0));

                  gcc_jit_rvalue *state_val = gcc_jit_lvalue_as_rvalue(state_field);
                  gcc_jit_block *state_0_block = gcc_jit_function_new_block(func, "coro_resume_state_0");
                  gcc_jit_case *case_0 = gcc_jit_context_new_case(
                      ctx,
                      gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0),
                      gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0),
                      state_0_block);
                  gcc_jit_block_end_with_switch(coro_resume, NULL, state_val, coro_destroy, 1, &case_0);

                  /* Run destructors for promise and captured variables in frame, then operator delete (_ZdlPv) */
                  if (descr && descr->promise && descr->promise->init_kind == initk_dynamic &&
                      descr->promise->initializer.dynamic && descr->promise->initializer.dynamic->destructor) {
                      gcc_jit_function *prom_dtor = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(descr->promise->initializer.dynamic->destructor, &prom_dtor));
                      if (prom_dtor) {
                          gcc_jit_rvalue *prom_addr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(promise_field, NULL), void_ptr_type);
                          gcc_jit_block_add_eval(coro_destroy, NULL, gcc_jit_context_new_call(ctx, NULL, prom_dtor, 1, &prom_addr));
                      }
                  }
                  gcc_jit_rvalue *void_frame_ptr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(coro_frame), void_ptr_type);
                  if (descr && descr->delete_routine) {
                      gcc_jit_function *op_del_fn = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(descr->delete_routine, &op_del_fn));
                      gcc_jit_block_add_eval(coro_destroy, NULL, gcc_jit_context_new_call(ctx, NULL, op_del_fn, 1, &void_frame_ptr));
                  } else {
                      gcc_jit_param *del_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
                      gcc_jit_function *op_del_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "_ZdlPv", 1, &del_param, 0);
                      gcc_jit_block_add_eval(coro_destroy, NULL, gcc_jit_context_new_call(ctx, NULL, op_del_func, 1, &void_frame_ptr));
                  }

                  /* Execute initial suspend (co_await promise.initial_suspend()) and continue in state_0_block */
                  if (descr && descr->initial_suspend_call) {
                      active_coro_state_index++;
                      gcc_jit_block_add_assignment(cblock, loc, state_field, gcc_jit_context_new_rvalue_from_int(ctx, int_type, active_coro_state_index));
                      gcc_jit_rvalue *init_susp_rval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(descr->initial_suspend_call, &init_susp_rval));
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                      if (init_susp_rval && cblock) {
                          gcc_jit_block_add_eval(cblock, loc, init_susp_rval);
                      }
                  }

                  gcc_jit_type *fn_ret_type = gcc_jit_function_get_return_type(func);
                  if (fn_ret_type == void_type) {
                      gcc_jit_block_end_with_void_return(coro_destroy, NULL);
                      gcc_jit_block_end_with_jump(state_0_block, NULL, coro_destroy);
                  } else {
                      gcc_jit_rvalue *zero_ret = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(NULL, fn_ret_type, &zero_ret));
                      gcc_jit_block_end_with_return(coro_destroy, NULL, zero_ret);
                      gcc_jit_block_end_with_jump(state_0_block, NULL, coro_destroy);
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
          case stmk_coroutine_return:
              {
                  gcc_jit_block *cblock = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                  if (stmt->expr && cblock) {
                      /* Evaluate return_value(expr) or return_void() expression lowered by EDG */
                      gcc_jit_rvalue *ret_expr_val = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(stmt->expr, &ret_expr_val));
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                      if (ret_expr_val && cblock) {
                          gcc_jit_block_add_eval(cblock, loc, ret_expr_val);
                      }
                  }
                  if (active_coro_frame_lval && active_coro_state_field && cblock) {
                      gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                      gcc_jit_lvalue *frame_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(active_coro_frame_lval), NULL);
                      gcc_jit_lvalue *state_field = gcc_jit_lvalue_access_field(frame_deref, NULL, active_coro_state_field);
                      gcc_jit_block_add_assignment(cblock, loc, state_field, gcc_jit_context_new_rvalue_from_int(ctx, int_type, -1));
                      if (active_coro_frame_type && gcc_jit_type_is_struct(active_coro_frame_type)) {
                          gcc_jit_field *res_fn_fld = gcc_jit_struct_get_field(gcc_jit_type_is_struct(active_coro_frame_type), 0);
                          gcc_jit_lvalue *res_fn_lval = gcc_jit_lvalue_access_field(frame_deref, NULL, res_fn_fld);
                          gcc_jit_type *res_fn_type = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(res_fn_lval));
                          gcc_jit_block_add_assignment(cblock, loc, res_fn_lval, gcc_jit_context_null(ctx, res_fn_type));
                      }
                  }
                  /* Transition to final_suspend if available */
                  if (active_coro_descr && active_coro_descr->final_suspend_label && cblock) {
                      gcc_jit_block *final_susp_block = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_label_block(func, active_coro_descr->final_suspend_label, &final_susp_block));
                      gcc_jit_block_end_with_jump(cblock, loc, final_susp_block);
                  } else if (active_coro_descr && active_coro_descr->final_suspend_call && cblock) {
                      active_coro_state_index++;
                      gcc_jit_rvalue *fs_rval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(active_coro_descr->final_suspend_call, &fs_rval));
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                      if (fs_rval && cblock) {
                          gcc_jit_block_add_eval(cblock, loc, fs_rval);
                      }
                      if (cblock) gcc_jit_block_end_with_void_return(cblock, loc);
                  } else if (cblock) {
                      gcc_jit_block_end_with_void_return(cblock, loc);
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
                  return GCC_GEN_BE_SUCCESS;
              }
          case stmk_return:
              if (stmt->expr) {
                  gcc_jit_rvalue *rval = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &rval);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  GCC_GEN_BE_CHECK(emit_active_seh_finally_cleanups(func));
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  if (current_block) {
                      void *sret_cached = NULL;
                      GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, func, &sret_cached));
                      if (sret_cached && rval) {
                          gcc_jit_param *sret_param = (gcc_jit_param *)sret_cached;
                          gcc_jit_lvalue *sret_dest = gcc_jit_rvalue_dereference(gcc_jit_param_as_rvalue(sret_param), loc);
                          gcc_jit_block_add_assignment(current_block, loc, sret_dest, rval);
#ifdef EDG_GCC_BE_ISOLATED_TEST
                          GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(NULL, current_block));
#else
                          GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups((an_object_lifetime_ptr)(uintptr_t)1, current_block));
#endif
                          gcc_jit_block_end_with_void_return(current_block, loc);
                      } else {
#ifdef EDG_GCC_BE_ISOLATED_TEST
                          GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(NULL, current_block));
#else
                          GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups((an_object_lifetime_ptr)(uintptr_t)1, current_block));
#endif
                          gcc_jit_type *fn_ret_t = gcc_jit_function_get_return_type(func);
                          if (fn_ret_t == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                              gcc_jit_block_end_with_void_return(current_block, loc);
                          } else {
                              if (!rval) {
                                  if (gcc_jit_function_get_param_count(func) > 0) {
                                      gcc_jit_rvalue *p0_val = gcc_jit_param_as_rvalue(gcc_jit_function_get_param(func, 0));
                                      if (gcc_jit_rvalue_get_type(p0_val) == fn_ret_t) {
                                          rval = p0_val;
                                      } else if (gcc_jit_type_is_pointer(fn_ret_t) != NULL &&
                                                 gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(p0_val)) != NULL) {
                                          rval = gcc_jit_context_new_cast(ctx, loc, p0_val, fn_ret_t);
                                      }
                                  }
                                  if (!rval) {
                                      GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(stmt->expr->type, fn_ret_t, &rval));
                                  }
                              } else if (gcc_jit_rvalue_get_type(rval) != fn_ret_t) {
                                  gcc_jit_type *rv_t = gcc_jit_rvalue_get_type(rval);
                                  a_type_ptr expr_ut = stmt->expr ? stmt->expr->type : NULL;
                                  while (expr_ut && expr_ut->kind == tk_typeref) expr_ut = expr_ut->variant.typeref.type;
                                  if (gcc_jit_type_is_struct(rv_t) != NULL || gcc_jit_type_is_struct(fn_ret_t) != NULL ||
                                      (expr_ut && (expr_ut->kind == tk_struct || expr_ut->kind == tk_class || expr_ut->kind == tk_union))) {
                                      extern gcc_gen_be_error_t gcc_gen_be_coerce_rvalue(
                                          gcc_jit_function *fn,
                                          gcc_jit_block *blk,
                                          gcc_jit_rvalue *src_rval,
                                          gcc_jit_type *dst_type,
                                          gcc_jit_rvalue **out_coerced) GCC_GEN_BE_NOEXCEPT;
                                      gcc_jit_rvalue *coerced_ret = NULL;
                                      if (gcc_gen_be_coerce_rvalue(func, current_block, rval, fn_ret_t, &coerced_ret) == GCC_GEN_BE_SUCCESS &&
                                          coerced_ret) {
                                          rval = coerced_ret;
                                      }
                                  } else {
                                      rval = gcc_jit_context_new_cast(ctx, loc, rval, fn_ret_t);
                                  }
                              }
                              gcc_jit_block_end_with_return(current_block, loc, rval);
                          }
                      }
                  }
              } else {
                  GCC_GEN_BE_CHECK(emit_active_seh_finally_cleanups(func));
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  if (current_block) {
#ifdef EDG_GCC_BE_ISOLATED_TEST
                      GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(NULL, current_block));
#else
                      GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups((an_object_lifetime_ptr)(uintptr_t)1, current_block));
#endif
                      gcc_jit_type *fn_ret_t = gcc_jit_function_get_return_type(func);
                      if (fn_ret_t == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                          gcc_jit_block_end_with_void_return(current_block, NULL);
                      } else {
                          gcc_jit_rvalue *ret_fallback = NULL;
                          if (gcc_jit_function_get_param_count(func) > 0) {
                              gcc_jit_rvalue *p0_val = gcc_jit_param_as_rvalue(gcc_jit_function_get_param(func, 0));
                              if (gcc_jit_rvalue_get_type(p0_val) == fn_ret_t) {
                                  ret_fallback = p0_val;
                              } else if (gcc_jit_type_is_pointer(fn_ret_t) != NULL &&
                                         gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(p0_val)) != NULL) {
                                  ret_fallback = gcc_jit_context_new_cast(ctx, loc, p0_val, fn_ret_t);
                              }
                          }
                          if (!ret_fallback) {
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(NULL, fn_ret_t, &ret_fallback));
                          }
                          gcc_jit_block_end_with_return(current_block, NULL, ret_fallback);
                      }
                  }
              }
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
              break;

          case stmk_goto:
              {
                  gcc_jit_block *target = NULL;
                  err = gcc_gen_be_get_label_block(func, stmt->variant.label.ptr, &target);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  GCC_GEN_BE_CHECK(emit_active_seh_finally_cleanups(func));
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  if (current_block) {
                      GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(stmt->variant.label.lifetime, current_block));
                      gcc_jit_block_end_with_jump(current_block, loc, target);
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
              }
              break;

#if GNU_EXTENSIONS_ALLOWED
          case stmk_assigned_goto:
              {
                  gcc_jit_rvalue *ptr = NULL;
#ifdef EDG_GCC_BE_ISOLATED_TEST
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(stmt->variant.assigned_goto.ptr, &ptr));
#else
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(stmt->expr, &ptr));
#endif
                  
                  gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
                  if (!computed_goto_dispatcher_block) {
                      computed_goto_dispatcher_block = gcc_jit_function_new_block(func, "computed_goto_dispatcher");
                      computed_goto_target_lval = gcc_jit_function_new_local(func, NULL, long_type, "computed_goto_target");
                  }
                  
                  gcc_jit_rvalue *ptr_long = NULL;
                  if (ptr) {
                      gcc_jit_type *ptr_t = gcc_jit_rvalue_get_type(ptr);
                      if (ptr_t == long_type) {
                          ptr_long = ptr;
                      } else if (gcc_jit_type_is_pointer(ptr_t) == NULL) {
                          ptr_long = gcc_jit_context_new_cast(ctx, NULL, ptr, long_type);
                      } else if (p_gcc_jit_context_new_bitcast) {
                          ptr_long = p_gcc_jit_context_new_bitcast(ctx, NULL, ptr, long_type);
                      } else {
                          extern gcc_gen_be_error_t gcc_gen_be_coerce_rvalue(
                              struct gcc_jit_function *fn,
                              struct gcc_jit_block *blk,
                              struct gcc_jit_rvalue *src_rval,
                              struct gcc_jit_type *dst_type,
                              struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;
                          GCC_GEN_BE_CHECK(gcc_gen_be_coerce_rvalue(func, current_block, ptr, long_type, &ptr_long));
                      }
                  } else {
                      ptr_long = gcc_jit_context_zero(ctx, long_type);
                  }
                  gcc_jit_block_add_assignment(current_block, loc, computed_goto_target_lval, ptr_long);
                  gcc_jit_block_end_with_jump(current_block, loc, computed_goto_dispatcher_block);
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
              }
              break;
#endif

          case stmk_if:
          case stmk_constexpr_if:
          case stmk_if_consteval:
          case stmk_if_not_consteval:
              {
                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &cond);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  if (!cond) cond = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  if (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(cond)) != NULL) {
                      cond = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, cond, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(cond)));
                  } else if (gcc_jit_rvalue_get_type(cond) != bool_type) {
                      cond = gcc_jit_context_new_cast(ctx, NULL, cond, bool_type);
                  }

                  gcc_jit_block *then_block = gcc_jit_function_new_block(func, "if_then");
                  gcc_jit_block *else_block = gcc_jit_function_new_block(func, "if_else");

                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  gcc_jit_block_end_with_conditional(current_block, loc, cond, then_block, else_block);

                  gcc_jit_block *after_then = NULL;
                  gcc_jit_block *after_else = NULL;

                  if (stmt->kind == stmk_constexpr_if) {
                      if (stmt->variant.constexpr_if->then_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(then_block)); 
                          err = gcc_gen_be_lower_statement(stmt->variant.constexpr_if->then_statement, func);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_then));
                      }
                      else
                          after_then = then_block;

                      if (stmt->variant.constexpr_if->else_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(else_block)); 
                          err = gcc_gen_be_lower_statement(stmt->variant.constexpr_if->else_statement, func);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_else));
                      }
                      else
                          after_else = else_block;
                  } else {
                      if (stmt->variant.if_stmt.then_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(then_block)); 
                          err = gcc_gen_be_lower_statement(stmt->variant.if_stmt.then_statement, func);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_then));
                      }
                      else
                          after_then = then_block;

                      if (stmt->variant.if_stmt.else_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(else_block)); 
                          err = gcc_gen_be_lower_statement(stmt->variant.if_stmt.else_statement, func);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_else));
                      }
                      else
                          after_else = else_block;
                  }

                  if (after_then || after_else) {
                      gcc_jit_block *merge_block = gcc_jit_function_new_block(func, "if_merge");
                      if (after_then) gcc_jit_block_end_with_jump(after_then, NULL, merge_block);
                      if (after_else) gcc_jit_block_end_with_jump(after_else, NULL, merge_block);
                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
                  } else {
                      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
                  }
              }
              break;

          case stmk_switch:
              {
                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &cond);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  if (!cond) cond = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));

                  a_switch_stmt_descr_ptr ssdp = stmt->variant.switch_stmt.extra_info;
                  int num_cases = 0;
                  a_switch_case_entry_ptr scep = ssdp->cases;
                  while (scep) {
                      if (scep != ssdp->default_case) num_cases++;
                      scep = scep->next;
                  }

                  gcc_jit_case **cases = NULL;
                  if (num_cases > 0) {
                      cases = (gcc_jit_case **)malloc((size_t)num_cases * sizeof(gcc_jit_case *));
                      if (!cases) return GCC_GEN_BE_ERROR_OOM;
                  }

                  int i = 0;
                  scep = ssdp->cases;
                  while (scep) {
                      if (scep != ssdp->default_case) {
                          gcc_jit_type *cond_type = gcc_jit_rvalue_get_type(cond);
                          gcc_jit_rvalue *val = NULL;
                          if (scep->case_value && scep->case_value->kind == ck_integer) {
                              err = gcc_gen_be_lower_constant_rvalue(scep->case_value, cond_type, &val);
                              if (err != GCC_GEN_BE_SUCCESS) { if (cases) free(cases); return err; }
                          }
                          if (!val) {
                              val = gcc_jit_context_new_rvalue_from_long(ctx, cond_type, 0);
                          }
                          
                          gcc_jit_rvalue *max_val = val;
#if GNU_EXTENSIONS_ALLOWED
                          if (scep->range_end && scep->range_end->kind == ck_integer) {
                              err = gcc_gen_be_lower_constant_rvalue(scep->range_end, cond_type, &max_val);
                              if (err != GCC_GEN_BE_SUCCESS) { if (cases) free(cases); return err; }
                          }
#endif
                          
                          gcc_jit_block *case_block = NULL;
                          err = gcc_gen_be_get_switch_case_block(func, scep, &case_block);
                          if (err != GCC_GEN_BE_SUCCESS) { if(cases) free(cases); return err; }
                          
                          cases[i++] = gcc_jit_context_new_case(ctx, val, max_val, case_block);
                      }
                      scep = scep->next;
                  }

                  gcc_jit_block *default_block = NULL;
                  if (ssdp->default_case) {
                      err = gcc_gen_be_get_switch_case_block(func, ssdp->default_case, &default_block);
                      if (err != GCC_GEN_BE_SUCCESS) { if(cases) free(cases); return err; }
                  } else {
                      default_block = gcc_jit_function_new_block(func, "switch_default");
                  }

                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                  gcc_jit_block_end_with_switch(current_block, NULL, cond, default_block, num_cases, cases);
                  if (num_cases > 0) free(cases);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL)); 
                  err = gcc_gen_be_lower_statement(stmt->variant.switch_stmt.body_statement, func);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_block *after_switch = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_switch));
                  gcc_jit_block *exit_block = gcc_jit_function_new_block(func, "switch_exit");

                  if (!ssdp->default_case) {
                      gcc_jit_block_end_with_jump(default_block, NULL, exit_block);
                  }
                  if (after_switch) {
                      gcc_jit_block_end_with_jump(after_switch, NULL, exit_block);
                  }

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(exit_block));
              }
              break;

          case stmk_while:
          case stmk_end_test_while:
              {
                  gcc_jit_block *cond_block = gcc_jit_function_new_block(func, "loop_cond");
                  gcc_jit_block *body_block = gcc_jit_function_new_block(func, "loop_body");
                  gcc_jit_block *exit_block = gcc_jit_function_new_block(func, "loop_exit");

                  if (stmt->kind == stmk_end_test_while) {
                      gcc_jit_block_end_with_jump(current_block, loc, body_block);
                  } else {
                      gcc_jit_block_end_with_jump(current_block, loc, cond_block);
                  }

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cond_block));
                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &cond);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  if (!cond) cond = gcc_jit_context_one(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  if (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(cond)) != NULL) {
                      cond = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, cond, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(cond)));
                  } else if (gcc_jit_rvalue_get_type(cond) != bool_type) {
                      cond = gcc_jit_context_new_cast(ctx, NULL, cond, bool_type);
                  }

                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cond_block));
                  if (cond_block) gcc_jit_block_end_with_conditional(cond_block, NULL, cond, body_block, exit_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(body_block)); 
                  err = gcc_gen_be_lower_statement(stmt->variant.loop_statement, func);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_block *after_body = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_body));
                  if (after_body) gcc_jit_block_end_with_jump(after_body, NULL, cond_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(exit_block));
              }
              break;

          case stmk_for:
              {
                  a_for_loop_ptr loop_info = stmt->variant.for_loop.extra_info;
                  if (loop_info && loop_info->initialization) {
                      err = gcc_gen_be_lower_statement(loop_info->initialization, func);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  gcc_jit_block *cond_block = gcc_jit_function_new_block(func, "for_cond");
                  gcc_jit_block *body_block = gcc_jit_function_new_block(func, "for_body");
                  gcc_jit_block *step_block = gcc_jit_function_new_block(func, "for_step");
                  gcc_jit_block *exit_block = gcc_jit_function_new_block(func, "for_exit");

                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block)); /* refresh */
                  if (current_block) gcc_jit_block_end_with_jump(current_block, loc, cond_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cond_block));
                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &cond);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  if (!cond) cond = gcc_jit_context_one(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
                  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                  if (gcc_jit_type_is_pointer(gcc_jit_rvalue_get_type(cond)) != NULL) {
                      cond = gcc_jit_context_new_comparison(ctx, loc, GCC_JIT_COMPARISON_NE, cond, gcc_jit_context_null(ctx, gcc_jit_rvalue_get_type(cond)));
                  } else if (gcc_jit_rvalue_get_type(cond) != bool_type) {
                      cond = gcc_jit_context_new_cast(ctx, NULL, cond, bool_type);
                  }

                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cond_block));
                  if (cond_block) gcc_jit_block_end_with_conditional(cond_block, NULL, cond, body_block, exit_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(body_block)); 
                  err = gcc_gen_be_lower_statement(stmt->variant.for_loop.statement, func);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_block *after_body = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_body));
                  if (after_body) gcc_jit_block_end_with_jump(after_body, NULL, step_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(step_block));
                  if (loop_info && loop_info->increment) {
                      gcc_jit_rvalue *rval = NULL;
                      err = gcc_gen_be_lower_expr_rvalue(loop_info->increment, &rval);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&step_block));
                      if (rval && step_block) gcc_jit_block_add_eval(step_block, NULL, rval);
                  }
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&step_block));
                  if (step_block) gcc_jit_block_end_with_jump(step_block, NULL, cond_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(exit_block));
              }
              break;


#if MICROSOFT_EXTENSIONS_ALLOWED
          case stmk_microsoft_try:
              {
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_microsoft_try(stmt, func));
              }
              break;
#endif

          case stmk_try_block:
              {
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_try_block(stmt, func));
              }
              break;

          case stmk_range_based_for:
              {
                  if (stmt->variant.range_based_for_loop.statement) {
                      err = gcc_gen_be_lower_statement(stmt->variant.range_based_for_loop.statement, func);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
              }
              break;
#if MICROSOFT_EXTENSIONS_ALLOWED
          case stmk_for_each:
              {
                  if (stmt->variant.for_each_loop.statement) {
                      err = gcc_gen_be_lower_statement(stmt->variant.for_each_loop.statement, func);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
              }
              break;
#endif



          case stmk_set_vla_size:
              {
                  a_vla_dimension_ptr dim = stmt->variant.vla_dimension;
                  if (dim->dimension_variable != NULL) {
                      gcc_jit_rvalue *ignore = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(dim->dimension_expr, &ignore));
                      if (ignore) {
                          gcc_jit_block *cblock = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                          gcc_jit_block_add_eval(cblock, loc, ignore);
                      }
                  }
              }
              break;
              
          case stmk_vla_decl:
              {
                  a_variable_ptr var = stmt->variant.vla.variant.variable;
                  if (var && !stmt->variant.vla.is_typedef_decl) {
                      gcc_jit_lvalue *vla_lval = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_vla_decl(func, var, &vla_lval));
                  }
              }
              break;
          case stmk_decl:
              err = gcc_gen_be_lower_statement_decl(stmt, func);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              break;
              
          case stmk_init:
              {
                  a_dynamic_init_ptr dip = stmt->variant.dynamic_init;
                  if (dip && dip->variable) {
                      void *cached = NULL;
                      err = cache_lookup(GCC_GEN_BE_CACHE_VAR, dip->variable, &cached);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      
                      gcc_jit_lvalue *var_lvalue = (gcc_jit_lvalue *)cached;
                      if (!var_lvalue) {
                          if ((dip->variable->storage_class == sc_static || dip->variable->is_thread_local) &&
                              !(!dip->variable->source_corresp.name && dip->variable->init_kind == initk_static && dip->kind == dik_none)) {
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(dip->variable, &var_lvalue));
                          } else {
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, dip->variable, &var_lvalue));
                          }
                      }
                      if (var_lvalue) {
                          if (dip->kind == dik_none) {
                              if (((dip->variable->storage_class != sc_static && !dip->variable->is_thread_local) ||
                                   (!dip->variable->source_corresp.name && dip->variable->init_kind == initk_static)) &&
                                  current_block) {
                                  gcc_jit_type *var_type = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(var_lvalue));
                                  if (dip->variable->init_kind == initk_static && dip->variable->initializer.constant) {
                                      gcc_jit_rvalue *init_rval = NULL;
                                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(dip->variable->initializer.constant, var_type, &init_rval));
                                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                                      if (init_rval && current_block) {
                                          gcc_jit_block_add_assignment(current_block, loc, var_lvalue, init_rval);
                                      }
                                  } else if (dip->variable->init_kind == initk_zero) {
                                      gcc_jit_rvalue *zero_rval = NULL;
                                      GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(dip->variable->type, var_type, &zero_rval));
                                      if (zero_rval && current_block) {
                                          gcc_jit_block_add_assignment(current_block, loc, var_lvalue, zero_rval);
                                      }
                                  }
                              }
                          } else if (dip->variable->storage_class == sc_static || dip->variable->is_thread_local) {
                              struct gcc_jit_lvalue *guard = NULL;
                              err = gcc_gen_be_get_or_create_guard(ctx, dip->variable, &guard);
                              if (err != GCC_GEN_BE_SUCCESS) return err;

                              gcc_jit_function *acquire_fn = NULL;
                              err = get_cxa_guard_func(ctx, "__cxa_guard_acquire", gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), &acquire_fn);
                              if (err != GCC_GEN_BE_SUCCESS) return err;

                              gcc_jit_function *release_fn = NULL;
                              err = get_cxa_guard_func(ctx, "__cxa_guard_release", gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), &release_fn);
                              if (err != GCC_GEN_BE_SUCCESS) return err;

                              gcc_jit_function *abort_fn = NULL;
                              err = get_cxa_guard_func(ctx, "__cxa_guard_abort", gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), &abort_fn);
                              if (err != GCC_GEN_BE_SUCCESS) return err;

                              gcc_jit_block *init_block = gcc_jit_function_new_block(func, "guard_init");
                              gcc_jit_block *done_block = gcc_jit_function_new_block(func, "guard_done");

                              gcc_jit_rvalue *guard_ptr = gcc_jit_lvalue_get_address((gcc_jit_lvalue*)guard, NULL);
                              gcc_jit_rvalue *acquire_call = gcc_jit_context_new_call(ctx, NULL, acquire_fn, 1, &guard_ptr);
                              gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, acquire_call, gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT)));

                              gcc_jit_block_end_with_conditional(current_block, loc, cond, init_block, done_block);

                              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(init_block));
                              gcc_jit_rvalue *rval = NULL;
                              err = gcc_gen_be_lower_dynamic_init(dip, var_lvalue, &rval);
                              if (err != GCC_GEN_BE_SUCCESS) {
                                  gcc_jit_block_add_eval(init_block, NULL, gcc_jit_context_new_call(ctx, NULL, abort_fn, 1, &guard_ptr));
                                  return err;
                              }
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&init_block));
                              if (init_block) {
                                  gcc_jit_block_add_eval(init_block, NULL, gcc_jit_context_new_call(ctx, NULL, release_fn, 1, &guard_ptr));
                                  gcc_jit_block_end_with_jump(init_block, NULL, done_block);
                              }

                              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(done_block));
                              current_block = done_block;
                          } else {
                              gcc_jit_rvalue *rval = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(dip, var_lvalue, &rval));
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
                          }
                      }
                  }
              }
              break;

#if ASM_FUNCTION_ALLOWED
          case stmk_asm_func_body:
              {
                  /* Handle naked assembly function bodies.
                     In libgccjit, you can use gcc_jit_context_add_top_level_asm
                     but that's global. For a specific function, if the function is entirely naked ASM,
                     there's no direct `gcc_jit_block_add_asm_func_body`.
                     Extended inline asm works, so we just add it to the block. */
                  if (stmt->variant.asm_func_body) {
                      gcc_jit_block *cblock = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                      if (cblock) {
                          gcc_jit_extended_asm *ext_asm = gcc_jit_block_add_extended_asm(cblock, NULL, stmt->variant.asm_func_body);
                          if (ext_asm) {
                              gcc_jit_extended_asm_set_volatile_flag(ext_asm, 1);
                          }
                      }
                  }
                  return GCC_GEN_BE_SUCCESS;
              }
#endif


          case stmk_asm:
              {
                  an_asm_entry_ptr asm_entry = stmt->variant.asm_entry;
                  if (asm_entry) {
                      const char *asm_str = "";
                      if (asm_entry->asm_string && asm_entry->asm_string->kind == ck_string) {
                          asm_str = (const char *)asm_entry->asm_string->variant.string.value;
                      }
                      
#if defined(GCC_JIT_EXTENDED_ASM_GOTO)
                      /* GCC 11+ supports asm goto via gcc_jit_block_end_with_extended_asm_goto */
                      gcc_jit_extended_asm *ext_asm = NULL;
                      if (asm_entry->is_asm_goto) {
                          /* Need to collect blocks from asm_entry->labels */
                          int num_labels = 0;
                          a_label_list_ptr lbl = asm_entry->labels;
                          while (lbl) { num_labels++; lbl = lbl->next; }
                          
                          gcc_jit_block **dest_blocks = NULL;
                          if (num_labels > 0) {
                              dest_blocks = (gcc_jit_block **)malloc((size_t)num_labels * sizeof(gcc_jit_block *));
                              if (!dest_blocks) return GCC_GEN_BE_ERROR_OOM;
                              int i = 0;
                              for (lbl = asm_entry->labels; lbl; lbl = lbl->next) {
                                  GCC_GEN_BE_CHECK(gcc_gen_be_get_label_block(func, lbl->label, &dest_blocks[i++]));
                              }
                          }
                          
                          gcc_jit_block *fallthrough_block = gcc_jit_function_new_block(func, "asm_goto_fallthrough");
                          ext_asm = gcc_jit_block_end_with_extended_asm_goto(current_block, NULL, asm_str, num_labels, dest_blocks, fallthrough_block);
                          
                          if (dest_blocks) free(dest_blocks);
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(fallthrough_block));
                      } else {
                          ext_asm = gcc_jit_block_add_extended_asm(current_block, NULL, asm_str);
                      }
#else
                      gcc_jit_extended_asm *ext_asm = gcc_jit_block_add_extended_asm(current_block, NULL, asm_str);
#endif
                      if (ext_asm) {
                          gcc_jit_extended_asm_set_volatile_flag(ext_asm, asm_entry->is_volatile);
                          
                          an_asm_operand_ptr op = asm_entry->operands;
                          int out_op_idx = 0;
                          while (op) {
                              const char *name = op->name;
                              const char *constraint = op->constraints_string;
                              if (op->is_output_operand) {
                                  gcc_jit_lvalue *lval = NULL;
                                  err = gcc_gen_be_lower_expr_lvalue(op->expression, &lval);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;
                                  if (lval) {
                                      char out_cstr[64];
                                      char in_cstr[32];
                                      a_boolean is_rw = FALSE;
                                      GCC_GEN_BE_CHECK(gcc_gen_be_normalize_asm_constraints(
                                          constraint, out_op_idx, out_cstr, sizeof(out_cstr),
                                          in_cstr, sizeof(in_cstr), &is_rw));
                                      gcc_jit_extended_asm_add_output_operand(ext_asm, name, out_cstr, (gcc_jit_lvalue*)lval);
                                      if (is_rw) {
                                          gcc_jit_extended_asm_add_input_operand(ext_asm, NULL, in_cstr, gcc_jit_lvalue_as_rvalue(lval));
                                      }
                                  }
                                  out_op_idx++;
                              } else {
                                  gcc_jit_rvalue *rval = NULL;
                                  err = gcc_gen_be_lower_expr_rvalue(op->expression, &rval);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;
                                  if (rval) {
                                      gcc_jit_extended_asm_add_input_operand(ext_asm, name, constraint, rval);
                                  }
                              }
                              op = op->next;
                          }
                          
                          a_named_register_list_ptr clobber = asm_entry->clobbers;
                          while (clobber) {
                              if (clobber->reg == anr_memory) {
                                  gcc_jit_extended_asm_add_clobber(ext_asm, "memory");
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
                              } else if (clobber->reg == anr_flags) {
                                  gcc_jit_extended_asm_add_clobber(ext_asm, "cc");
#endif
                              } else if (clobber->reg > anr_invalid && clobber->reg < anr_last) {
                                  const char *reg_name = named_register_names[clobber->reg];
                                  if (reg_name) {
                                      gcc_jit_extended_asm_add_clobber(ext_asm, reg_name);
                                  }
                              }
                              clobber = clobber->next;
                          }
                      }
                  }
              }
              break;

          case stmk_label:
          case stmk_empty:
              break;

          default:
              return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_UNHANDLED_STMT, "unhandled statement kind");
      }

      stmt = stmt->next;
  }
  
/** @brief GCC_GEN_BE_SUCCESS member. */
  return GCC_GEN_BE_SUCCESS;
}



/**
 * @brief Emits a stack save operation.
 *
 * @details Emits a call to __builtin_stack_save.
 *
 * @param[in] func The current function.
 * @param[in] block The current block.
 * @param[out] out_save Pointer to receive the saved stack variable.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_stack_save(struct gcc_jit_function *func, struct gcc_jit_block *block, gcc_jit_lvalue **out_save) GCC_GEN_BE_NOEXCEPT {
    if (!func || !block || !out_save) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    gcc_jit_function *save_fn = p_gcc_jit_context_get_builtin_function ? p_gcc_jit_context_get_builtin_function(ctx, "__builtin_stack_save") : NULL;
    if (save_fn) {
        gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, NULL, save_fn, 0, NULL);
        gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
        struct gcc_jit_lvalue *save_var = gcc_jit_function_new_local(func, NULL, void_ptr_type, "saved_stack");
        gcc_jit_block_add_assignment(block, NULL, (gcc_jit_lvalue*)save_var, call);
        *out_save = save_var;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a stack restore operation.
 *
 * @details Emits a call to __builtin_stack_restore.
 *
 * @param[in] func The current function.
 * @param[in] block The current block.
 * @param[in] save_var The saved stack variable.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_stack_restore(struct gcc_jit_function *func, struct gcc_jit_block *block, gcc_jit_lvalue *save_var) GCC_GEN_BE_NOEXCEPT {
    if (!func || !block || !save_var) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    gcc_jit_function *restore_fn = p_gcc_jit_context_get_builtin_function ? p_gcc_jit_context_get_builtin_function(ctx, "__builtin_stack_restore") : NULL;
    if (restore_fn) {
        gcc_jit_rvalue *arg = gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue*)save_var);
        gcc_jit_block_add_eval(block, NULL, gcc_jit_context_new_call(ctx, NULL, restore_fn, 1, &arg));
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_vla_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    if (!func || !var || !out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_type *type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(var->type, &type));
    if (!type) return GCC_GEN_BE_ERROR_VLA_ALLOCATION_FAILED;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!cblock) return GCC_GEN_BE_ERROR_VLA_ALLOCATION_FAILED;

    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_var(var, &loc));

    gcc_jit_lvalue *vla_lval = gcc_jit_function_new_local(
        func, loc, type, var->source_corresp.name ? var->source_corresp.name : "vla_var");
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, vla_lval));

    gcc_jit_type *size_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_rvalue *total_elements = gcc_jit_context_new_rvalue_from_long(ctx, size_type, 1);

    a_type_ptr curr_type = var->type;
    while (curr_type && curr_type->kind == tk_typeref) curr_type = curr_type->variant.typeref.type;

    extern struct a_scope *innermost_function_scope;

    while (curr_type && curr_type->kind == tk_array) {
        if (curr_type->variant.array.has_assoc_vla_dimension && innermost_function_scope) {
            a_vla_dimension_ptr dim = innermost_function_scope->vla_dimensions;
            while (dim) {
                if (dim->type == curr_type) break;
                dim = dim->next;
            }
            while (dim && !dim->dimension_variable && dim->original_dimension) {
                dim = dim->original_dimension;
            }
            if (dim && dim->dimension_variable) {
                gcc_jit_lvalue *dim_lval = NULL;
                void *cached = NULL;
                GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, dim->dimension_variable, &cached));
                if (cached) dim_lval = (gcc_jit_lvalue *)cached;
                else GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, dim->dimension_variable, &dim_lval));

                if (dim_lval) {
                    gcc_jit_rvalue *dim_val = gcc_jit_lvalue_as_rvalue(dim_lval);
                    dim_val = gcc_jit_context_new_cast(ctx, loc, dim_val, size_type);
                    total_elements = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, size_type, total_elements, dim_val);
                }
            }
        } else if (curr_type->variant.array.variant.number_of_elements > 0) {
            gcc_jit_rvalue *dim_val = gcc_jit_context_new_rvalue_from_long(ctx, size_type, (long)curr_type->variant.array.variant.number_of_elements);
            total_elements = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, size_type, total_elements, dim_val);
        }
        curr_type = curr_type->variant.array.element_type;
        while (curr_type && curr_type->kind == tk_typeref) curr_type = curr_type->variant.typeref.type;
    }

    gcc_jit_type *elem_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(curr_type, &elem_type));

    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_param *alloca_param = gcc_jit_context_new_param(ctx, NULL, size_type, "size");
    gcc_jit_function *alloca_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__builtin_alloca", 1, &alloca_param, 0);

    gcc_jit_rvalue *elem_size = gcc_jit_context_new_cast(ctx, loc, gcc_jit_context_new_sizeof(ctx, elem_type), size_type);
    gcc_jit_rvalue *total_bytes = gcc_jit_context_new_binary_op(ctx, loc, GCC_JIT_BINARY_OP_MULT, size_type, total_elements, elem_size);

    gcc_jit_rvalue *alloca_call = gcc_jit_context_new_call(ctx, loc, alloca_func, 1, &total_bytes);
    gcc_jit_rvalue *typed_mem = gcc_jit_context_new_cast(ctx, loc, alloca_call, type);
    gcc_jit_block_add_assignment(cblock, loc, vla_lval, typed_mem);

    *out_lval = vla_lval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers coroutine frame.
 * @details Stubs coroutine frame.
 * @param[in] rout Routine.
 * @param[out] out_frame_type Frame type.
 * @return GCC_GEN_BE_SUCCESS
 */

/**
 * @brief Recursively scans an expression tree for `enk_temp_init` temporaries to spill into the coroutine frame.
 * @details Promotes expression temporaries encountered inside a coroutine body to fields in the coroutine frame
 * and caches their `gcc_jit_field *` in `GCC_GEN_BE_CACHE_FIELD`.
 * @param[in] expr The frontend expression node to scan.
 * @param[in] ctx The active `libgccjit` context.
 * @param[in,out] fields Pointer to the coroutine frame field array.
 * @param[in,out] num_fields Pointer to the current field count.
 * @param[in,out] capacity Pointer to the allocated field array capacity.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t scan_expr_for_coro_temps(
    an_expr_node_ptr expr,
    gcc_jit_context *ctx,
    gcc_jit_field ***fields,
    int *num_fields,
    int *capacity) GCC_GEN_BE_NOEXCEPT {
    while (expr) {
        if (expr->kind == enk_temp_init && expr->type) {
            void *existing = NULL;
            if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, expr, &existing) != GCC_GEN_BE_SUCCESS || !existing) {
                gcc_jit_type *temp_type = NULL;
                if (gcc_gen_be_lower_type(expr->type, &temp_type) == GCC_GEN_BE_SUCCESS && temp_type) {
                    if (*num_fields >= *capacity) {
                        *capacity = (*capacity == 0) ? 16 : (*capacity * 2);
                        gcc_jit_field **new_fields = (gcc_jit_field **)realloc(*fields, (size_t)(*capacity) * sizeof(gcc_jit_field *));
                        if (!new_fields) return GCC_GEN_BE_ERROR_OOM;
                        *fields = new_fields;
                    }
                    char field_name[128];
                    snprintf(field_name, sizeof(field_name), "__coro_temp_%p", (void *)expr);
                    gcc_jit_field *fld = gcc_jit_context_new_field(ctx, NULL, temp_type, field_name);
                    (*fields)[*num_fields] = fld;
                    (*num_fields)++;
                    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, expr, fld));
                }
            }
            if (expr->variant.init.dynamic_init && expr->variant.init.dynamic_init->kind == dik_expression) {
                GCC_GEN_BE_CHECK(scan_expr_for_coro_temps(
                    expr->variant.init.dynamic_init->variant.expression, ctx, fields, num_fields, capacity));
            }
        } else if (expr->kind == enk_operation) {
            GCC_GEN_BE_CHECK(scan_expr_for_coro_temps(
                expr->variant.operation.operands, ctx, fields, num_fields, capacity));
        } else if (expr->kind == enk_await || expr->kind == enk_yield) {
            GCC_GEN_BE_CHECK(scan_expr_for_coro_temps(
                expr->variant.await_info.operand, ctx, fields, num_fields, capacity));
            GCC_GEN_BE_CHECK(scan_expr_for_coro_temps(
                expr->variant.await_info.ready_resume_suspend, ctx, fields, num_fields, capacity));
        }
        expr = expr->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively scans a statement list for `enk_temp_init` temporaries to spill into the coroutine frame.
 * @details Walks all statement kinds (`stmk_expr`, `stmk_return`, `stmk_coroutine_return`, `stmk_block`,
 * `stmk_if`, `stmk_constexpr_if`, `stmk_while`, `stmk_end_test_while`, `stmk_for`, `stmk_range_based_for`,
 * `stmk_switch`, `stmk_try_block`, and `stmk_init`) to discover temporaries spanning suspend points.
 * @param[in] stmt The frontend statement list to scan.
 * @param[in] ctx The active `libgccjit` context.
 * @param[in,out] fields Pointer to the coroutine frame field array.
 * @param[in,out] num_fields Pointer to the current field count.
 * @param[in,out] capacity Pointer to the allocated field array capacity.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t scan_stmt_for_coro_temps(
    a_statement_ptr stmt,
    gcc_jit_context *ctx,
    gcc_jit_field ***fields,
    int *num_fields,
    int *capacity) GCC_GEN_BE_NOEXCEPT {
    while (stmt) {
        if (stmt->expr) {
            GCC_GEN_BE_CHECK(scan_expr_for_coro_temps(stmt->expr, ctx, fields, num_fields, capacity));
        }
        if (stmt->kind == stmk_block) {
            GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(stmt->variant.block.statements, ctx, fields, num_fields, capacity));
        } else if (stmt->kind == stmk_if || stmt->kind == stmk_if_consteval || stmt->kind == stmk_if_not_consteval) {
            GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(stmt->variant.if_stmt.then_statement, ctx, fields, num_fields, capacity));
            GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(stmt->variant.if_stmt.else_statement, ctx, fields, num_fields, capacity));
        } else if (stmt->kind == stmk_constexpr_if && stmt->variant.constexpr_if) {
            GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(stmt->variant.constexpr_if->then_statement, ctx, fields, num_fields, capacity));
            GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(stmt->variant.constexpr_if->else_statement, ctx, fields, num_fields, capacity));
        } else if (stmt->kind == stmk_while || stmt->kind == stmk_end_test_while) {
            GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(stmt->variant.loop_statement, ctx, fields, num_fields, capacity));
        } else if (stmt->kind == stmk_for) {
            if (stmt->variant.for_loop.extra_info) {
                GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(
                    stmt->variant.for_loop.extra_info->initialization, ctx, fields, num_fields, capacity));
                GCC_GEN_BE_CHECK(scan_expr_for_coro_temps(
                    stmt->variant.for_loop.extra_info->increment, ctx, fields, num_fields, capacity));
            }
            GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(stmt->variant.for_loop.statement, ctx, fields, num_fields, capacity));
        } else if (stmt->kind == stmk_range_based_for) {
            GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(stmt->variant.range_based_for_loop.statement, ctx, fields, num_fields, capacity));
        } else if (stmt->kind == stmk_switch) {
            GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(stmt->variant.switch_stmt.body_statement, ctx, fields, num_fields, capacity));
        } else if (stmt->kind == stmk_try_block && stmt->variant.try_block) {
            GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(stmt->variant.try_block->statement, ctx, fields, num_fields, capacity));
            for (a_handler_ptr h = stmt->variant.try_block->handlers; h != NULL; h = h->next) {
                GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(h->statement, ctx, fields, num_fields, capacity));
            }
        } else if (stmt->kind == stmk_init && stmt->variant.dynamic_init &&
                   stmt->variant.dynamic_init->kind == dik_expression) {
            GCC_GEN_BE_CHECK(scan_expr_for_coro_temps(
                stmt->variant.dynamic_init->variant.expression, ctx, fields, num_fields, capacity));
        }
        stmt = stmt->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Collects all local variables in a scope and sub-scopes into a struct fields array.
 * @details Recursively walks EDG frontend scope nodes to build a flat frame layout.
 * @param[in] scope The EDG scope pointer.
 * @param[in] ctx The libgccjit context.
 * @param[in,out] fields Pointer to array of gcc_jit_field pointers.
 * @param[in,out] num_fields Pointer to current number of fields.
 * @param[in,out] capacity Pointer to current capacity of the array.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on failure.
 */
static gcc_gen_be_error_t collect_coro_locals(a_scope_ptr scope, gcc_jit_context *ctx, gcc_jit_field ***fields, int *num_fields, int *capacity) GCC_GEN_BE_NOEXCEPT {
    if (!scope) return GCC_GEN_BE_SUCCESS;
    if (scope->assoc_block) {
        GCC_GEN_BE_CHECK(scan_stmt_for_coro_temps(scope->assoc_block, ctx, fields, num_fields, capacity));
    }
    
    a_variable_ptr var = scope->nonstatic_variables;
    while (var) {
        if (var->type) {
            gcc_jit_type *var_type = NULL;
            gcc_gen_be_error_t err = gcc_gen_be_lower_type(var->type, &var_type);
            if (err == GCC_GEN_BE_SUCCESS && var_type) {
                if (*num_fields >= *capacity) {
                    *capacity = (*capacity == 0) ? 16 : (*capacity * 2);
                    gcc_jit_field **new_fields = (gcc_jit_field **)realloc(*fields, (size_t)(*capacity) * sizeof(gcc_jit_field *));
                    if (!new_fields) return GCC_GEN_BE_ERROR_OOM;
                    *fields = new_fields;
                }
                char field_name[128];
                snprintf(field_name, sizeof(field_name), "local_%p", (void *)var);
                gcc_jit_field *fld = gcc_jit_context_new_field(ctx, NULL, var_type, field_name);
                (*fields)[*num_fields] = fld;
                (*num_fields)++;
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, var, fld));
            }
        }
        var = var->next;
    }

    var = scope->variables;
    while (var) {
        if (!var->is_parameter && var->type) {
            void *existing = NULL;
            if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, var, &existing) != GCC_GEN_BE_SUCCESS || !existing) {
                gcc_jit_type *var_type = NULL;
                gcc_gen_be_error_t err = gcc_gen_be_lower_type(var->type, &var_type);
                if (err == GCC_GEN_BE_SUCCESS && var_type) {
                    if (*num_fields >= *capacity) {
                        *capacity = (*capacity == 0) ? 16 : (*capacity * 2);
                        gcc_jit_field **new_fields = (gcc_jit_field **)realloc(*fields, (size_t)(*capacity) * sizeof(gcc_jit_field *));
                        if (!new_fields) return GCC_GEN_BE_ERROR_OOM;
                        *fields = new_fields;
                    }
                    char field_name[128];
                    snprintf(field_name, sizeof(field_name), "temp_%p", (void *)var);
                    gcc_jit_field *fld = gcc_jit_context_new_field(ctx, NULL, var_type, field_name);
                    (*fields)[*num_fields] = fld;
                    (*num_fields)++;
                    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, var, fld));
                }
            }
        }
        var = var->next;
    }

    if (scope->lifetime && scope->lifetime->destructions) {
        for (a_dynamic_init_ptr d = scope->lifetime->destructions; d != NULL; d = d->next_in_destruction_list) {
            a_variable_ptr dvar = d->variable;
            if (dvar && dvar->type) {
                void *existing = NULL;
                if (cache_lookup(GCC_GEN_BE_CACHE_FIELD, dvar, &existing) != GCC_GEN_BE_SUCCESS || !existing) {
                    gcc_jit_type *var_type = NULL;
                    gcc_gen_be_error_t err = gcc_gen_be_lower_type(dvar->type, &var_type);
                    if (err == GCC_GEN_BE_SUCCESS && var_type) {
                        if (*num_fields >= *capacity) {
                            *capacity = (*capacity == 0) ? 16 : (*capacity * 2);
                            gcc_jit_field **new_fields = (gcc_jit_field **)realloc(*fields, (size_t)(*capacity) * sizeof(gcc_jit_field *));
                            if (!new_fields) return GCC_GEN_BE_ERROR_OOM;
                            *fields = new_fields;
                        }
                        char field_name[128];
                        snprintf(field_name, sizeof(field_name), "temp_lt_%p", (void *)dvar);
                        gcc_jit_field *fld = gcc_jit_context_new_field(ctx, NULL, var_type, field_name);
                        (*fields)[*num_fields] = fld;
                        (*num_fields)++;
                        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, dvar, fld));
                    }
                }
            }
        }
    }
    
    a_scope_ptr child = scope->scopes;
    while (child) {
        gcc_gen_be_error_t err = collect_coro_locals(child, ctx, fields, num_fields, capacity);
        if (err != GCC_GEN_BE_SUCCESS) return err;
        child = child->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Appends a single EDG type's size and alignment to a running coroutine frame layout.
 * @details Aligns `*inout_offset` to the type's alignment, advances `*inout_offset` by the type's size,
 * and updates `*inout_max_align`.
 * @param[in] tp The EDG type pointer (or NULL for pointer-sized fallback).
 * @param[in,out] inout_offset Running byte offset in the coroutine frame.
 * @param[in,out] inout_max_align Running maximum alignment of the coroutine frame.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
static gcc_gen_be_error_t accumulate_coro_type_size_align(
    a_type_ptr tp,
    size_t *inout_offset,
    size_t *inout_max_align) GCC_GEN_BE_NOEXCEPT {
    if (!inout_offset || !inout_max_align) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    while (tp && tp->kind == tk_typeref) tp = tp->variant.typeref.type;
    size_t f_size = (tp && tp->size > 0) ? (size_t)tp->size : sizeof(void *);
    size_t f_align = (tp && tp->alignment > 0) ? (size_t)tp->alignment : (f_size < sizeof(void *) ? f_size : sizeof(void *));
    if (f_align == 0) f_align = 1;
    if (f_align > *inout_max_align) *inout_max_align = f_align;
    size_t rem = *inout_offset % f_align;
    if (rem != 0) *inout_offset += (f_align - rem);
    *inout_offset += f_size;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively accumulates `enk_temp_init` temporary sizes and alignments in an expression tree.
 * @details Walks expression trees (`enk_temp_init`, `enk_operation`, `enk_await`, `enk_yield`) to include
 * temporaries spanning suspension points in the coroutine frame size and alignment calculation.
 * @param[in] expr The frontend expression node to scan.
 * @param[in,out] inout_offset Running byte offset in the coroutine frame.
 * @param[in,out] inout_max_align Running maximum alignment of the coroutine frame.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t accumulate_coro_expr_temp_sizes(
    an_expr_node_ptr expr,
    size_t *inout_offset,
    size_t *inout_max_align) GCC_GEN_BE_NOEXCEPT {
    while (expr) {
        if (expr->kind == enk_temp_init && expr->type) {
            GCC_GEN_BE_CHECK(accumulate_coro_type_size_align(expr->type, inout_offset, inout_max_align));
            if (expr->variant.init.dynamic_init && expr->variant.init.dynamic_init->kind == dik_expression) {
                GCC_GEN_BE_CHECK(accumulate_coro_expr_temp_sizes(
                    expr->variant.init.dynamic_init->variant.expression, inout_offset, inout_max_align));
            }
        } else if (expr->kind == enk_operation) {
            GCC_GEN_BE_CHECK(accumulate_coro_expr_temp_sizes(
                expr->variant.operation.operands, inout_offset, inout_max_align));
        } else if (expr->kind == enk_await || expr->kind == enk_yield) {
            GCC_GEN_BE_CHECK(accumulate_coro_expr_temp_sizes(
                expr->variant.await_info.operand, inout_offset, inout_max_align));
            GCC_GEN_BE_CHECK(accumulate_coro_expr_temp_sizes(
                expr->variant.await_info.ready_resume_suspend, inout_offset, inout_max_align));
        }
        expr = expr->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively accumulates `enk_temp_init` temporary sizes and alignments across a statement list.
 * @details Walks statements and nested blocks/branches/loops/try-blocks to include expression temporaries
 * in the coroutine frame layout calculation.
 * @param[in] stmt The frontend statement list to scan.
 * @param[in,out] inout_offset Running byte offset in the coroutine frame.
 * @param[in,out] inout_max_align Running maximum alignment of the coroutine frame.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t accumulate_coro_stmt_temp_sizes(
    a_statement_ptr stmt,
    size_t *inout_offset,
    size_t *inout_max_align) GCC_GEN_BE_NOEXCEPT {
    while (stmt) {
        if (stmt->expr) {
            GCC_GEN_BE_CHECK(accumulate_coro_expr_temp_sizes(stmt->expr, inout_offset, inout_max_align));
        }
        if (stmt->kind == stmk_block) {
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(stmt->variant.block.statements, inout_offset, inout_max_align));
        } else if (stmt->kind == stmk_if || stmt->kind == stmk_if_consteval || stmt->kind == stmk_if_not_consteval) {
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(stmt->variant.if_stmt.then_statement, inout_offset, inout_max_align));
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(stmt->variant.if_stmt.else_statement, inout_offset, inout_max_align));
        } else if (stmt->kind == stmk_constexpr_if && stmt->variant.constexpr_if) {
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(stmt->variant.constexpr_if->then_statement, inout_offset, inout_max_align));
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(stmt->variant.constexpr_if->else_statement, inout_offset, inout_max_align));
        } else if (stmt->kind == stmk_while || stmt->kind == stmk_end_test_while) {
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(stmt->variant.loop_statement, inout_offset, inout_max_align));
        } else if (stmt->kind == stmk_for) {
            if (stmt->variant.for_loop.extra_info) {
                GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(
                    stmt->variant.for_loop.extra_info->initialization, inout_offset, inout_max_align));
                GCC_GEN_BE_CHECK(accumulate_coro_expr_temp_sizes(
                    stmt->variant.for_loop.extra_info->increment, inout_offset, inout_max_align));
            }
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(stmt->variant.for_loop.statement, inout_offset, inout_max_align));
        } else if (stmt->kind == stmk_range_based_for) {
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(stmt->variant.range_based_for_loop.statement, inout_offset, inout_max_align));
        } else if (stmt->kind == stmk_switch) {
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(stmt->variant.switch_stmt.body_statement, inout_offset, inout_max_align));
        } else if (stmt->kind == stmk_try_block && stmt->variant.try_block) {
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(stmt->variant.try_block->statement, inout_offset, inout_max_align));
            for (a_handler_ptr h = stmt->variant.try_block->handlers; h != NULL; h = h->next) {
                GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(h->statement, inout_offset, inout_max_align));
            }
        } else if (stmt->kind == stmk_init && stmt->variant.dynamic_init &&
                   stmt->variant.dynamic_init->kind == dik_expression) {
            GCC_GEN_BE_CHECK(accumulate_coro_expr_temp_sizes(
                stmt->variant.dynamic_init->variant.expression, inout_offset, inout_max_align));
        }
        stmt = stmt->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively accumulates local variable sizes and alignments across a coroutine scope tree.
 * @details Walks `scope->assoc_block` temporaries, `scope->nonstatic_variables`, `scope->variables`,
 * `scope->lifetime->destructions`, and child scopes.
 * @param[in] scope The EDG scope pointer.
 * @param[in,out] inout_offset Running byte offset in the coroutine frame.
 * @param[in,out] inout_max_align Running maximum alignment of the coroutine frame.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t accumulate_coro_scope_sizes(
    a_scope_ptr scope,
    size_t *inout_offset,
    size_t *inout_max_align) GCC_GEN_BE_NOEXCEPT {
    if (!scope) return GCC_GEN_BE_SUCCESS;
    if (scope->assoc_block) {
        GCC_GEN_BE_CHECK(accumulate_coro_stmt_temp_sizes(scope->assoc_block, inout_offset, inout_max_align));
    }
    for (a_variable_ptr var = scope->nonstatic_variables; var != NULL; var = var->next) {
        if (var->type) {
            GCC_GEN_BE_CHECK(accumulate_coro_type_size_align(var->type, inout_offset, inout_max_align));
        }
    }
    for (a_variable_ptr var = scope->variables; var != NULL; var = var->next) {
        if (!var->is_parameter && var->type) {
            GCC_GEN_BE_CHECK(accumulate_coro_type_size_align(var->type, inout_offset, inout_max_align));
        }
    }
    if (scope->lifetime && scope->lifetime->destructions) {
        for (a_dynamic_init_ptr d = scope->lifetime->destructions; d != NULL; d = d->next_in_destruction_list) {
            if (d->variable && d->variable->type) {
                GCC_GEN_BE_CHECK(accumulate_coro_type_size_align(d->variable->type, inout_offset, inout_max_align));
            }
        }
    }
    for (a_scope_ptr child = scope->scopes; child != NULL; child = child->next) {
        GCC_GEN_BE_CHECK(accumulate_coro_scope_sizes(child, inout_offset, inout_max_align));
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_compute_coro_frame_size_and_align(
    a_routine_ptr rout,
    a_scope_ptr scope,
    size_t *out_frame_size,
    size_t *out_frame_align) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !out_frame_size || !out_frame_align) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_frame_size = 0;
    *out_frame_align = sizeof(void *);

    size_t offset = 2 * sizeof(void *); /* __resume_fn and __destroy_fn */
    size_t max_align = sizeof(void *);

    a_coroutine_descr_ptr descr = get_coroutine_descr(rout);
    GCC_GEN_BE_CHECK(accumulate_coro_type_size_align(
        (descr && descr->promise) ? descr->promise->type : NULL, &offset, &max_align));

    /* __suspend_index (int), initial_await_resume_called (bool), __coro_flags (int) */
    offset += 3 * sizeof(int);

    if (descr && descr->parameter_copies) {
        for (a_variable_ptr pcopy = descr->parameter_copies; pcopy != NULL; pcopy = pcopy->next) {
            if (pcopy->type) {
                GCC_GEN_BE_CHECK(accumulate_coro_type_size_align(pcopy->type, &offset, &max_align));
            }
        }
    }

    a_scope_ptr func_scope = scope;
    if (!func_scope && rout->function_def_number != 0 && il_header.function_def_table) {
        func_scope = il_header.function_def_table[rout->function_def_number].scope;
    }
    if (func_scope) {
        for (a_variable_ptr pvar = func_scope->variant.routine.parameters; pvar != NULL; pvar = pvar->next) {
            if (pvar->type) {
                GCC_GEN_BE_CHECK(accumulate_coro_type_size_align(pvar->type, &offset, &max_align));
            }
        }
        GCC_GEN_BE_CHECK(accumulate_coro_scope_sizes(func_scope, &offset, &max_align));
    }

    size_t rem = offset % max_align;
    if (rem != 0) offset += (max_align - rem);
    if (offset < 64U) offset = 64U;

    *out_frame_size = offset;
    *out_frame_align = max_align;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_compute_coro_frame_layout(
    a_routine_ptr rout,
    a_scope_ptr scope,
    struct gcc_jit_struct **out_frame_struct,
    size_t *out_frame_size) GCC_GEN_BE_NOEXCEPT {
    if (!rout || (!out_frame_struct && !out_frame_size)) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (out_frame_struct) *out_frame_struct = NULL;
    if (out_frame_size) *out_frame_size = 0;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    size_t exact_size = 0;
    size_t exact_align = sizeof(void *);
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_size_and_align(rout, scope, &exact_size, &exact_align));

    void *cached_frame = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_TYPE, rout, &cached_frame));
    if (cached_frame) {
        gcc_jit_type *ft = (gcc_jit_type *)cached_frame;
        if (out_frame_struct) *out_frame_struct = gcc_jit_type_is_struct(ft);
        if (out_frame_size) {
            ssize_t sz = (gcc_jit_type_is_struct(ft) == NULL) ? gcc_jit_type_get_size(ft) : (ssize_t)exact_size;
            *out_frame_size = (sz > 0) ? (size_t)sz : exact_size;
        }
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(void_type);
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);

    gcc_jit_type *dispatcher_type = gcc_jit_context_new_function_ptr_type(ctx, NULL, void_type, 1, &void_ptr_type, 0);

    a_coroutine_descr_ptr descr = get_coroutine_descr(rout);
    gcc_jit_type *promise_type = void_ptr_type;
    if (descr && descr->promise && descr->promise->type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(descr->promise->type, &promise_type));
    }

    gcc_jit_field *f_resume = gcc_jit_context_new_field(ctx, NULL, dispatcher_type, "__resume_fn");
    gcc_jit_field *f_destroy = gcc_jit_context_new_field(ctx, NULL, dispatcher_type, "__destroy_fn");
    gcc_jit_field *f_promise = gcc_jit_context_new_field(ctx, NULL, promise_type, "__promise");
    gcc_jit_field *f_state = gcc_jit_context_new_field(ctx, NULL, int_type, "__suspend_index");
    gcc_jit_field *f_init_await = gcc_jit_context_new_field(ctx, NULL, bool_type, "initial_await_resume_called");
    gcc_jit_field *f_flags = gcc_jit_context_new_field(ctx, NULL, int_type, "__coro_flags");

    int num_fields = 6;
    int capacity = 16;
    gcc_jit_field **fields = (gcc_jit_field **)malloc((size_t)capacity * sizeof(gcc_jit_field *));
    if (!fields) return GCC_GEN_BE_ERROR_OOM;
    fields[0] = f_resume;
    fields[1] = f_destroy;
    fields[2] = f_promise;
    fields[3] = f_state;
    fields[4] = f_init_await;
    fields[5] = f_flags;

    /* Add copied function parameter fields */
    if (descr && descr->parameter_copies) {
        for (a_variable_ptr pcopy = descr->parameter_copies; pcopy != NULL; pcopy = pcopy->next) {
            if (pcopy->type) {
                gcc_jit_type *ptype = NULL;
                if (gcc_gen_be_lower_type(pcopy->type, &ptype) == GCC_GEN_BE_SUCCESS && ptype) {
                    if (num_fields >= capacity) {
                        capacity *= 2;
                        gcc_jit_field **new_fields = (gcc_jit_field **)realloc(fields, (size_t)capacity * sizeof(gcc_jit_field *));
                        if (!new_fields) { free(fields); return GCC_GEN_BE_ERROR_OOM; }
                        fields = new_fields;
                    }
                    char pname[128];
                    snprintf(pname, sizeof(pname), "param_%p", (void *)pcopy);
                    gcc_jit_field *pfld = gcc_jit_context_new_field(ctx, NULL, ptype, pname);
                    fields[num_fields++] = pfld;
                    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, pcopy, pfld));
                }
            }
        }
    }

    a_scope_ptr func_scope = scope;
    if (!func_scope && rout->function_def_number != 0 && il_header.function_def_table) {
        func_scope = il_header.function_def_table[rout->function_def_number].scope;
    }
    if (func_scope) {
        for (a_variable_ptr pvar = func_scope->variant.routine.parameters; pvar != NULL; pvar = pvar->next) {
            void *existing = NULL;
            if (pvar->type && (cache_lookup(GCC_GEN_BE_CACHE_FIELD, pvar, &existing) != GCC_GEN_BE_SUCCESS || !existing)) {
                gcc_jit_type *ptype = NULL;
                if (gcc_gen_be_lower_type(pvar->type, &ptype) == GCC_GEN_BE_SUCCESS && ptype) {
                    if (num_fields >= capacity) {
                        capacity *= 2;
                        gcc_jit_field **new_fields = (gcc_jit_field **)realloc(fields, (size_t)capacity * sizeof(gcc_jit_field *));
                        if (!new_fields) { free(fields); return GCC_GEN_BE_ERROR_OOM; }
                        fields = new_fields;
                    }
                    char pname[128];
                    snprintf(pname, sizeof(pname), "arg_%p", (void *)pvar);
                    gcc_jit_field *pfld = gcc_jit_context_new_field(ctx, NULL, ptype, pname);
                    fields[num_fields++] = pfld;
                    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, pvar, pfld));
                }
            }
        }
        gcc_gen_be_error_t loc_err = collect_coro_locals(func_scope, ctx, &fields, &num_fields, &capacity);
        if (loc_err != GCC_GEN_BE_SUCCESS) { free(fields); return loc_err; }
    }

    char name[128];
    snprintf(name, sizeof(name), "_Z_coro_frame_%p", (void *)rout);
    gcc_jit_struct *frame_struct = gcc_jit_context_new_struct_type(ctx, NULL, name, num_fields, fields);
    free(fields);

    gcc_jit_type *frame_type = gcc_jit_struct_as_type(frame_struct);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, rout, frame_type));

    if (out_frame_struct) *out_frame_struct = frame_struct;
    if (out_frame_size) {
        size_t computed_from_fields = (size_t)num_fields * sizeof(void *);
        *out_frame_size = (exact_size >= computed_from_fields) ? exact_size : computed_from_fields;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_coroutine_frame(a_routine_ptr rout, struct gcc_jit_type **out_frame_type) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !out_frame_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_frame_type = NULL;

    gcc_jit_struct *frame_struct = NULL;
    size_t frame_size = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_layout(rout, NULL, &frame_struct, &frame_size));
    if (!frame_struct) return GCC_GEN_BE_ERROR_INTERNAL;
    *out_frame_type = gcc_jit_struct_as_type(frame_struct);
    return GCC_GEN_BE_SUCCESS;
}


/**
 * @brief Lowers the C++20 coroutine ramp function body.
 * @details Allocates the coroutine frame via promise or global `operator new` (including over-aligned
 * `operator new(size_t, std::align_val_t)` when frame alignment exceeds 16 bytes), handles
 * `get_return_object_on_allocation_failure()` when allocation is `noexcept`, initializes
 * `__resume_fn`, `__destroy_fn`, and `__suspend_index = 0`, copies parameters into the frame,
 * constructs the promise object, evaluates `get_return_object()`, invokes the actor function for
 * initial suspend, and returns the coroutine return object to the caller.
 * @param[in] rout The coroutine routine entry.
 * @param[in] ramp_func The `gcc_jit_function *` representing the coroutine ramp function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_coroutine_ramp(a_routine_ptr rout, struct gcc_jit_function *ramp_func) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !ramp_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

    gcc_jit_block *entry = gcc_jit_function_new_block(ramp_func, "ramp_entry");
    gcc_jit_block *alloc_done = gcc_jit_function_new_block(ramp_func, "alloc_done");

    a_coroutine_descr_ptr descr = get_coroutine_descr(rout);

    /* Call operator new */
    gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    
    gcc_jit_type *frame_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_frame(rout, &frame_type));
    size_t exact_frame_sz = 0;
    size_t exact_frame_align = sizeof(void *);
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_size_and_align(rout, NULL, &exact_frame_sz, &exact_frame_align));
    gcc_jit_struct *f_st = gcc_jit_type_is_struct(frame_type);
    long min_field_sz = (f_st != NULL) ? (long)(gcc_jit_struct_get_field_count(f_st) * 16 + 64) : (long)exact_frame_sz;
    long frame_sz = ((long)exact_frame_sz >= min_field_sz) ? (long)exact_frame_sz : min_field_sz;
    gcc_jit_rvalue *size_rval = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, frame_sz);

    extern int gcc_be_opt_level;
    a_boolean can_elide = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_try_coro_halo(descr, &can_elide));
    a_boolean use_halo_stack = (can_elide && gcc_be_opt_level >= 2) ? TRUE : FALSE;

    gcc_jit_rvalue *frame_ptr = NULL;
    if (use_halo_stack) {
        gcc_jit_lvalue *stack_frame = gcc_jit_function_new_local(ramp_func, NULL, frame_type, "coro_halo_stack_frame");
        frame_ptr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(stack_frame, NULL), void_ptr_type);
    } else if (descr && descr->new_routine) {
        gcc_jit_function *op_new_fn = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(descr->new_routine, &op_new_fn));
        int new_pcount = (int)gcc_jit_function_get_param_count(op_new_fn);
        int ramp_pcount = (int)gcc_jit_function_get_param_count(ramp_func);
        int argc = new_pcount > 1 ? new_pcount : 1;
        gcc_jit_rvalue **new_args = (gcc_jit_rvalue **)malloc((size_t)argc * sizeof(gcc_jit_rvalue *));
        if (!new_args) return GCC_GEN_BE_ERROR_OOM;
        new_args[0] = size_rval;
        for (int p = 1; p < argc; ++p) {
            gcc_jit_param *np = gcc_jit_function_get_param(op_new_fn, p);
            gcc_jit_type *nptype = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(np));
            if (p - 1 < ramp_pcount) {
                gcc_jit_rvalue *rp_val = gcc_jit_param_as_rvalue(gcc_jit_function_get_param(ramp_func, p - 1));
                new_args[p] = gcc_jit_context_new_cast(ctx, NULL, rp_val, nptype);
            } else {
                new_args[p] = gcc_jit_context_zero(ctx, nptype);
            }
        }
        frame_ptr = gcc_jit_context_new_call(ctx, NULL, op_new_fn, argc, new_args);
        free(new_args);
    } else if (exact_frame_align > 16U) {
        gcc_jit_param *new_params[2];
        new_params[0] = gcc_jit_context_new_param(ctx, NULL, size_t_type, "size");
        new_params[1] = gcc_jit_context_new_param(ctx, NULL, size_t_type, "alignment");
        gcc_jit_function *op_new_aligned = gcc_jit_context_new_function(
            ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "_ZnwmSt11align_val_t", 2, new_params, 0);
        gcc_jit_rvalue *align_rval = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)exact_frame_align);
        gcc_jit_rvalue *new_args[2] = { size_rval, align_rval };
        frame_ptr = gcc_jit_context_new_call(ctx, NULL, op_new_aligned, 2, new_args);
    } else {
        gcc_jit_param *new_param = gcc_jit_context_new_param(ctx, NULL, size_t_type, "size");
        gcc_jit_function *op_new = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "_Znwm", 1, &new_param, 0);
        frame_ptr = gcc_jit_context_new_call(ctx, NULL, op_new, 1, &size_rval);
    }
    
    gcc_jit_lvalue *frame_var = gcc_jit_function_new_local(ramp_func, NULL, void_ptr_type, "coro_frame_ptr");
    gcc_jit_block_add_assignment(entry, NULL, frame_var, frame_ptr);

    if (descr && descr->alloc_failure_gro_call) {
        gcc_jit_block *alloc_ok = gcc_jit_function_new_block(ramp_func, "ramp_alloc_ok");
        gcc_jit_block *alloc_fail = gcc_jit_function_new_block(ramp_func, "ramp_alloc_fail");
        gcc_jit_rvalue *is_null = gcc_jit_context_new_comparison(
            ctx, NULL, GCC_JIT_COMPARISON_EQ,
            gcc_jit_lvalue_as_rvalue(frame_var),
            gcc_jit_context_null(ctx, void_ptr_type));
        gcc_jit_block_end_with_conditional(entry, NULL, is_null, alloc_fail, alloc_ok);

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(alloc_fail));
        gcc_jit_rvalue *fail_gro = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(descr->alloc_failure_gro_call, &fail_gro));
        if (gcc_jit_function_get_return_type(ramp_func) == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
            if (fail_gro) gcc_jit_block_add_eval(alloc_fail, NULL, fail_gro);
            gcc_jit_block_end_with_void_return(alloc_fail, NULL);
        } else {
            if (!fail_gro) fail_gro = gcc_jit_context_null(ctx, gcc_jit_function_get_return_type(ramp_func));
            gcc_jit_block_end_with_return(alloc_fail, NULL, fail_gro);
        }
        entry = alloc_ok;
    }
    
    gcc_jit_function *resume_fn = NULL;
    gcc_jit_function *destroy_fn = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_dispatchers(rout, &resume_fn, &destroy_fn));

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    
    gcc_jit_lvalue *frame_deref = gcc_jit_rvalue_dereference(gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(frame_var), gcc_jit_type_get_pointer(frame_type)), NULL);
    gcc_jit_lvalue *f_resume = gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 0));
    gcc_jit_lvalue *f_destroy = gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 1));
    gcc_jit_lvalue *f_promise = gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 2));
    gcc_jit_lvalue *f_state = gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 3));
    gcc_jit_lvalue *f_init_await = gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 4));
    gcc_jit_lvalue *f_flags = gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 5));
    gcc_jit_type *dispatcher_type = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(f_resume));
    
    gcc_jit_block_add_assignment(entry, NULL, f_resume, gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(resume_fn, NULL), dispatcher_type));
    gcc_jit_block_add_assignment(entry, NULL, f_destroy, gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(destroy_fn, NULL), dispatcher_type));
    gcc_jit_type *prom_f_type = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(f_promise));
    gcc_jit_rvalue *zero_prom = NULL;
    if (prom_f_type == void_ptr_type || gcc_jit_type_is_pointer(prom_f_type) != NULL) {
        zero_prom = gcc_jit_context_null(ctx, prom_f_type);
    } else {
        GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(descr && descr->promise ? descr->promise->type : NULL, prom_f_type, &zero_prom));
    }
    if (zero_prom) {
        gcc_jit_block_add_assignment(entry, NULL, f_promise, zero_prom);
    }
    
    gcc_jit_rvalue *state_init = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
    gcc_jit_block_add_assignment(entry, NULL, f_state, state_init);
    gcc_jit_block_add_assignment(entry, NULL, f_init_await, gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL), 0));
    gcc_jit_block_add_assignment(entry, NULL, f_flags, gcc_jit_context_new_rvalue_from_int(ctx, int_type, use_halo_stack ? 1 : 0));

    if (descr && descr->init_await_resume) {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, descr->init_await_resume, f_init_await));
    }
    if (descr && descr->handle) {
        gcc_jit_type *handle_type = void_ptr_type;
        if (descr->handle->type) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(descr->handle->type, &handle_type));
        }
        gcc_jit_lvalue *handle_lval = gcc_jit_function_new_local(ramp_func, NULL, handle_type, "coro_handle");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, descr->handle, handle_lval));
    }

    /* Copy coroutine parameters into frame fields */
    if (descr && descr->parameter_copies) {
        int ramp_pcount = (int)gcc_jit_function_get_param_count(ramp_func);
        int p_idx = 0;
        for (a_variable_ptr pcopy = descr->parameter_copies; pcopy != NULL; pcopy = pcopy->next) {
            void *cached_fld = NULL;
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, pcopy, &cached_fld));
            if (cached_fld && p_idx < ramp_pcount) {
                gcc_jit_field *pfld = (gcc_jit_field *)cached_fld;
                gcc_jit_lvalue *param_slot = gcc_jit_lvalue_access_field(frame_deref, NULL, pfld);
                gcc_jit_rvalue *rp_val = gcc_jit_param_as_rvalue(gcc_jit_function_get_param(ramp_func, p_idx));
                gcc_jit_block_add_assignment(
                    entry, NULL, param_slot,
                    gcc_jit_context_new_cast(ctx, NULL, rp_val, gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(param_slot))));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, pcopy, param_slot));
            }
            p_idx++;
        }
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(entry));
    if (descr && descr->promise) {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, descr->promise, f_promise));
        if (descr->promise->init_kind == initk_dynamic && descr->promise->initializer.dynamic) {
            gcc_jit_rvalue *prom_init = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(descr->promise->initializer.dynamic, f_promise, &prom_init));
            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&entry));
        }
    }
    
    /* Evaluate get_return_object */
    gcc_jit_lvalue *ret_obj = NULL;
    if (descr && descr->get_return_object_call) {
        gcc_jit_type *ret_type = gcc_jit_function_get_return_type(ramp_func);
        gcc_jit_rvalue *gro_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(descr->get_return_object_call, &gro_rval));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&entry));
        if (ret_type != void_type) {
            ret_obj = gcc_jit_function_new_local(ramp_func, NULL, ret_type, "ret_obj");
            if (gro_rval && entry) {
                gcc_jit_block_add_assignment(entry, NULL, ret_obj, gro_rval);
            }
        } else if (gro_rval && entry) {
            gcc_jit_block_add_eval(entry, NULL, gro_rval);
        }
    }
    
    /* Evaluate initial_suspend */
    if (descr && descr->initial_suspend_call) {
        gcc_jit_rvalue *is_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(descr->initial_suspend_call, &is_rval));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&entry));
        if (is_rval && entry) {
            gcc_jit_block_add_eval(entry, NULL, is_rval);
        }
    }
    
    if (entry) {
        gcc_jit_block_end_with_jump(entry, NULL, alloc_done);
    }
    
    if (gcc_jit_function_get_return_type(ramp_func) == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
        gcc_jit_block_end_with_void_return(alloc_done, NULL);
    } else {
        gcc_jit_rvalue *ret_val = ret_obj ? gcc_jit_lvalue_as_rvalue(ret_obj) : gcc_jit_context_null(ctx, gcc_jit_function_get_return_type(ramp_func));
        gcc_jit_block_end_with_return(alloc_done, NULL, ret_val);
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers coroutine dispatchers.
 * @details Synthesizes the resume and destroy state-machine dispatchers for a coroutine,
 * including promise destruction and unhandled_exception / final_suspend handling.
 * @param[in] rout Routine.
 * @param[out] out_resume Resume fn.
 * @param[out] out_destroy Destroy fn.
 * @return GCC_GEN_BE_SUCCESS
 */
gcc_gen_be_error_t gcc_gen_be_lower_coroutine_dispatchers(a_routine_ptr rout, struct gcc_jit_function **out_resume, struct gcc_jit_function **out_destroy) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !out_resume || !out_destroy) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

    char res_name[128];
    snprintf(res_name, sizeof(res_name), "_Z_coro_resume_%p", (void *)rout);
    gcc_jit_param *p_res = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "frame");
    gcc_jit_function *resume_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, res_name, 1, &p_res, 0);

    char dest_name[128];
    snprintf(dest_name, sizeof(dest_name), "_Z_coro_destroy_%p", (void *)rout);
    gcc_jit_param *p_dest = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "frame");
    gcc_jit_function *destroy_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, dest_name, 1, &p_dest, 0);

    gcc_jit_block *res_entry = gcc_jit_function_new_block(resume_fn, "entry");
    gcc_jit_type *frame_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_coroutine_frame(rout, &frame_type));
    gcc_jit_type *frame_ptr_type = gcc_jit_type_get_pointer(frame_type);
    gcc_jit_lvalue *res_frame_local = gcc_jit_function_new_local(resume_fn, NULL, frame_ptr_type, "coro_frame");
    gcc_jit_block_add_assignment(
        res_entry, NULL, res_frame_local,
        gcc_jit_context_new_cast(ctx, NULL, gcc_jit_param_as_rvalue(p_res), frame_ptr_type));

    gcc_jit_lvalue *frame_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(res_frame_local), NULL);
    gcc_jit_field *state_fld = gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 3);
    gcc_jit_rvalue *state_val = gcc_jit_lvalue_as_rvalue(gcc_jit_lvalue_access_field(frame_deref, NULL, state_fld));
    
    gcc_jit_block *res_default = gcc_jit_function_new_block(resume_fn, "default_state");
    gcc_jit_block_end_with_void_return(res_default, NULL);
    
    gcc_jit_block *res_state0 = gcc_jit_function_new_block(resume_fn, "state_0");
    a_coroutine_descr_ptr descr = get_coroutine_descr(rout);

    GCC_GEN_BE_CHECK(push_coro_state());
    active_coro_frame_lval = res_frame_local;
    active_coro_frame_type = frame_type;
    active_coro_state_field = state_fld;
    active_coro_promise_field = gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 2);
    active_coro_descr = descr;
    active_coro_resume_blocks_count = 0;

    if (descr && descr->promise) {
        gcc_jit_lvalue *prom_lval = gcc_jit_lvalue_access_field(frame_deref, NULL, active_coro_promise_field);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, descr->promise, prom_lval));
    }
    if (descr && descr->init_await_resume) {
        gcc_jit_lvalue *init_await_lval = gcc_jit_lvalue_access_field(
            frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 4));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, descr->init_await_resume, init_await_lval));
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(res_state0));
    a_scope_ptr def_scope = NULL;
    if (rout->function_def_number != 0 && il_header.function_def_table) {
        def_scope = il_header.function_def_table[rout->function_def_number].scope;
    }
    gcc_jit_block *coro_eh_lp = NULL;
    if (descr && descr->unhandled_exception_call) {
        coro_eh_lp = gcc_jit_function_new_block(resume_fn, "coro_unhandled_exc_lp");
        gcc_jit_block *coro_body_start = gcc_jit_function_new_block(resume_fn, "coro_body_start");
        gcc_jit_rvalue *zero_i = gcc_jit_context_zero(ctx, int_type);
        gcc_jit_rvalue *cond_ok = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, zero_i, zero_i);
        gcc_jit_block_end_with_conditional(res_state0, NULL, cond_ok, coro_body_start, coro_eh_lp);
        res_state0 = coro_body_start;
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(res_state0));
        GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(coro_eh_lp));
    }
    if (def_scope && def_scope->assoc_block) {
        GCC_GEN_BE_CHECK(map_coro_scope_vars_to_frame(def_scope, frame_deref));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_statement(def_scope->assoc_block, resume_fn));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&res_state0));
    }
    if (coro_eh_lp) {
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    }
    gcc_jit_block *final_susp_entry = gcc_jit_function_new_block(resume_fn, "coro_final_suspend");
    if (res_state0) {
        gcc_jit_block_end_with_jump(res_state0, NULL, final_susp_entry);
    }
    if (coro_eh_lp && descr && descr->unhandled_exception_call) {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(coro_eh_lp));
        gcc_jit_rvalue *ue_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(descr->unhandled_exception_call, &ue_rval));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&coro_eh_lp));
        if (ue_rval && coro_eh_lp) {
            gcc_jit_block_add_eval(coro_eh_lp, NULL, ue_rval);
        }
        if (coro_eh_lp) {
            gcc_jit_block_end_with_jump(coro_eh_lp, NULL, final_susp_entry);
        }
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(final_susp_entry));
    res_state0 = final_susp_entry;
    if (descr && descr->final_suspend_call && res_state0) {
        gcc_jit_field *res_fn_fld = gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 0);
        gcc_jit_lvalue *res_fn_lval = gcc_jit_lvalue_access_field(frame_deref, NULL, res_fn_fld);
        gcc_jit_type *res_fn_type = gcc_jit_rvalue_get_type(gcc_jit_lvalue_as_rvalue(res_fn_lval));
        gcc_jit_block_add_assignment(res_state0, NULL, res_fn_lval, gcc_jit_context_null(ctx, res_fn_type));

        gcc_jit_rvalue *fs_rval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(descr->final_suspend_call, &fs_rval));
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&res_state0));
        if (fs_rval && res_state0) {
            gcc_jit_block_add_eval(res_state0, NULL, fs_rval);
        }
    }
    if (res_state0) {
        gcc_jit_block_end_with_void_return(res_state0, NULL);
    }
    
    int num_suspend_states = active_coro_resume_blocks_count;
    int total_cases = 1 + num_suspend_states;
    gcc_jit_case **cases = (gcc_jit_case **)malloc((size_t)total_cases * sizeof(gcc_jit_case *));
    if (!cases) {
        GCC_GEN_BE_CHECK(pop_coro_state());
        return GCC_GEN_BE_ERROR_OOM;
    }
    cases[0] = gcc_jit_context_new_case(
        ctx,
        gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0),
        gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0),
        res_state0);
    for (int idx = 0; idx < num_suspend_states; ++idx) {
        int state_num = idx + 1;
        cases[state_num] = gcc_jit_context_new_case(
            ctx,
            gcc_jit_context_new_rvalue_from_int(ctx, int_type, state_num),
            gcc_jit_context_new_rvalue_from_int(ctx, int_type, state_num),
            active_coro_resume_blocks[idx]);
    }
    gcc_jit_block_end_with_switch(res_entry, NULL, state_val, res_default, total_cases, cases);
    free(cases);

    gcc_jit_block *dest_entry = gcc_jit_function_new_block(destroy_fn, "entry");
    gcc_jit_lvalue *dest_frame_deref = gcc_jit_rvalue_dereference(
        gcc_jit_context_new_cast(ctx, NULL, gcc_jit_param_as_rvalue(p_dest), gcc_jit_type_get_pointer(frame_type)), NULL);
    gcc_jit_rvalue *dest_state_val = gcc_jit_lvalue_as_rvalue(
        gcc_jit_lvalue_access_field(dest_frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 3)));

    gcc_jit_block *dest_cleanup = gcc_jit_function_new_block(destroy_fn, "destroy_cleanup");
    int num_dest_cases = 1 + num_suspend_states;
    gcc_jit_case **dcases = (gcc_jit_case **)malloc((size_t)num_dest_cases * sizeof(gcc_jit_case *));
    if (!dcases) {
        GCC_GEN_BE_CHECK(pop_coro_state());
        return GCC_GEN_BE_ERROR_OOM;
    }
    dcases[0] = gcc_jit_context_new_case(
        ctx,
        gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0),
        gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0),
        dest_cleanup);
    for (int idx = 0; idx < num_suspend_states; ++idx) {
        int state_num = idx + 1;
        char dstate_name[64];
        snprintf(dstate_name, sizeof(dstate_name), "destroy_state_%d", state_num);
        gcc_jit_block *dstate_blk = gcc_jit_function_new_block(destroy_fn, dstate_name);
        a_scope_ptr active_susp_scope = (active_coro_suspend_scopes && active_coro_suspend_scopes[idx])
            ? active_coro_suspend_scopes[idx]
            : def_scope;
        if (active_susp_scope) {
            GCC_GEN_BE_CHECK(emit_coro_scope_dtors(ctx, active_susp_scope, dest_frame_deref, dstate_blk));
        }
        gcc_jit_block_end_with_jump(dstate_blk, NULL, dest_cleanup);
        dcases[state_num] = gcc_jit_context_new_case(
            ctx,
            gcc_jit_context_new_rvalue_from_int(ctx, int_type, state_num),
            gcc_jit_context_new_rvalue_from_int(ctx, int_type, state_num),
            dstate_blk);
    }
    gcc_jit_block_end_with_switch(dest_entry, NULL, dest_state_val, dest_cleanup, num_dest_cases, dcases);
    free(dcases);

    GCC_GEN_BE_CHECK(pop_coro_state());

    if (descr && descr->promise && descr->promise->init_kind == initk_dynamic &&
        descr->promise->initializer.dynamic && descr->promise->initializer.dynamic->destructor) {
        gcc_jit_function *prom_dtor = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(descr->promise->initializer.dynamic->destructor, &prom_dtor));
        if (prom_dtor) {
            gcc_jit_lvalue *prom_fld = gcc_jit_lvalue_access_field(
                dest_frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 2));
            gcc_jit_rvalue *prom_addr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(prom_fld, NULL), void_ptr_type);
            gcc_jit_block_add_eval(dest_cleanup, NULL, gcc_jit_context_new_call(ctx, NULL, prom_dtor, 1, &prom_addr));
        }
    }

    /* Guard heap deallocation so HALO stack-allocated frames ((__coro_flags & 1) != 0) skip operator delete */
    gcc_jit_lvalue *dest_flags_lval = gcc_jit_lvalue_access_field(
        dest_frame_deref, NULL, gcc_jit_struct_get_field(gcc_jit_type_is_struct(frame_type), 5));
    gcc_jit_rvalue *halo_bit = gcc_jit_context_new_binary_op(
        ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_AND, int_type,
        gcc_jit_lvalue_as_rvalue(dest_flags_lval),
        gcc_jit_context_one(ctx, int_type));
    gcc_jit_rvalue *is_heap_frame = gcc_jit_context_new_comparison(
        ctx, NULL, GCC_JIT_COMPARISON_EQ, halo_bit, gcc_jit_context_zero(ctx, int_type));
    gcc_jit_block *dest_heap_del = gcc_jit_function_new_block(destroy_fn, "destroy_heap_dealloc");
    gcc_jit_block *dest_done = gcc_jit_function_new_block(destroy_fn, "destroy_done");
    gcc_jit_block_end_with_conditional(dest_cleanup, NULL, is_heap_frame, dest_heap_del, dest_done);

    gcc_jit_rvalue *frame_val = gcc_jit_param_as_rvalue(p_dest);
    size_t dest_frame_sz = 0;
    size_t dest_frame_align = sizeof(void *);
    GCC_GEN_BE_CHECK(gcc_gen_be_compute_coro_frame_size_and_align(rout, def_scope, &dest_frame_sz, &dest_frame_align));
    if (descr && descr->delete_routine) {
        gcc_jit_function *op_del = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(descr->delete_routine, &op_del));
        gcc_jit_block_add_eval(dest_heap_del, NULL, gcc_jit_context_new_call(ctx, NULL, op_del, 1, &frame_val));
    } else if (dest_frame_align > 16U) {
        gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
        gcc_jit_param *del_params[2];
        del_params[0] = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
        del_params[1] = gcc_jit_context_new_param(ctx, NULL, size_t_type, "alignment");
        gcc_jit_function *op_del_aligned = gcc_jit_context_new_function(
            ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "_ZdlPvSt11align_val_t", 2, del_params, 0);
        gcc_jit_rvalue *align_rval = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)dest_frame_align);
        gcc_jit_rvalue *del_args[2] = { frame_val, align_rval };
        gcc_jit_block_add_eval(dest_heap_del, NULL, gcc_jit_context_new_call(ctx, NULL, op_del_aligned, 2, del_args));
    } else {
        gcc_jit_param *del_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
        gcc_jit_function *op_del = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "_ZdlPv", 1, &del_param, 0);
        gcc_jit_block_add_eval(dest_heap_del, NULL, gcc_jit_context_new_call(ctx, NULL, op_del, 1, &frame_val));
    }
    gcc_jit_block_end_with_jump(dest_heap_del, NULL, dest_done);
    gcc_jit_block_end_with_void_return(dest_done, NULL);

    *out_resume = resume_fn;
    *out_destroy = destroy_fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Evaluates whether a coroutine frame qualifies for Heap Allocation Elision Optimization (HALO).
 * @details Checks if the coroutine descriptor is present and has no custom allocator or allocation-failure handler.
 * @param[in] descr The EDG coroutine descriptor pointer.
 * @param[out] out_can_elide Pointer to receive TRUE if the frame can be elided onto the caller stack, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if out_can_elide is NULL.
 */
gcc_gen_be_error_t gcc_gen_be_try_coro_halo(
    a_coroutine_descr_ptr descr,
    a_boolean *out_can_elide) GCC_GEN_BE_NOEXCEPT {
    if (!out_can_elide) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_can_elide = FALSE;
    if (!descr) return GCC_GEN_BE_SUCCESS;
    if (!descr->new_routine && !descr->alloc_failure_gro_call &&
        (!descr->handle || !descr->handle->address_taken) &&
        (!descr->promise || !descr->promise->address_taken)) {
        *out_can_elide = TRUE;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Synthesizes or retrieves the `std::noop_coroutine()` frame singleton (`__gcc_be_noop_coro_frame`).
 * @details Emits a static coroutine frame header whose `__resume_fn` and `__destroy_fn` are no-op functions.
 * @param[out] out_noop_frame Pointer to receive the `__gcc_be_noop_coro_frame` global lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_noop_coroutine_frame(
    struct gcc_jit_lvalue **out_noop_frame) GCC_GEN_BE_NOEXCEPT {
    if (!out_noop_frame) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_noop_frame = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    void *cached = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_noop_coro_frame", &cached));
    if (cached) {
        *out_noop_frame = (gcc_jit_lvalue *)cached;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_param *noop_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "frame");
    gcc_jit_function *noop_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, "__gcc_be_noop_coro_actor", 1, &noop_param, 0);
    gcc_jit_block *noop_blk = gcc_jit_function_new_block(noop_fn, "entry");
    gcc_jit_block_end_with_void_return(noop_blk, NULL);

    gcc_jit_type *arr_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, 2);
    gcc_jit_lvalue *noop_global = gcc_jit_context_new_global(
        ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, arr_type, "__gcc_be_noop_coro_frame");
    gcc_jit_rvalue *fn_addr = gcc_jit_context_new_cast(
        ctx, NULL, gcc_jit_function_get_address(noop_fn, NULL), void_ptr_type);
    gcc_jit_rvalue *elems[2] = { fn_addr, fn_addr };
    gcc_jit_rvalue *init_val = gcc_jit_context_new_array_constructor(ctx, NULL, arr_type, 2, elems);
    gcc_jit_global_set_initializer_rvalue(noop_global, init_val);

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_noop_coro_frame", noop_global));
    *out_noop_frame = noop_global;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_coro_symmetric_transfer(
    struct gcc_jit_function *func,
    struct gcc_jit_block *suspend_block,
    struct gcc_jit_rvalue *handle_rval) GCC_GEN_BE_NOEXCEPT {
    if (!func || !suspend_block || !handle_rval) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *susp_type = gcc_jit_rvalue_get_type(handle_rval);

    gcc_jit_rvalue *raw_frame_addr = NULL;
    if (susp_type == void_ptr_type) {
        raw_frame_addr = handle_rval;
    } else if (gcc_jit_type_is_pointer(susp_type) != NULL) {
        raw_frame_addr = gcc_jit_context_new_cast(ctx, NULL, handle_rval, void_ptr_type);
    } else {
        gcc_jit_lvalue *handle_tmp = gcc_jit_function_new_local(func, NULL, susp_type, "sym_transfer_handle");
        gcc_jit_block_add_assignment(suspend_block, NULL, handle_tmp, handle_rval);
        gcc_jit_rvalue *handle_slot_ptr = gcc_jit_context_new_cast(
            ctx, NULL, gcc_jit_lvalue_get_address(handle_tmp, NULL),
            gcc_jit_type_get_pointer(void_ptr_type));
        raw_frame_addr = gcc_jit_lvalue_as_rvalue(
            gcc_jit_rvalue_dereference(handle_slot_ptr, NULL));
    }

    gcc_jit_lvalue *target_frame_local = gcc_jit_function_new_local(func, NULL, void_ptr_type, "sym_target_frame");
    gcc_jit_block_add_assignment(suspend_block, NULL, target_frame_local, raw_frame_addr);

    gcc_jit_rvalue *target_frame_addr = gcc_jit_lvalue_as_rvalue(target_frame_local);
    gcc_jit_rvalue *null_vp = gcc_jit_context_null(ctx, void_ptr_type);
    gcc_jit_rvalue *is_non_null = gcc_jit_context_new_comparison(
        ctx, NULL, GCC_JIT_COMPARISON_NE, target_frame_addr, null_vp);

    gcc_jit_block *resume_blk = gcc_jit_function_new_block(func, "sym_transfer_call");
    gcc_jit_block *done_blk = gcc_jit_function_new_block(func, "sym_transfer_done");
    gcc_jit_block_end_with_conditional(suspend_block, NULL, is_non_null, resume_blk, done_blk);

    gcc_jit_type *resume_fn_ptr_type = gcc_jit_context_new_function_ptr_type(
        ctx, NULL, void_type, 1, &void_ptr_type, 0);
    gcc_jit_rvalue *resume_slot_ptr = gcc_jit_context_new_cast(
        ctx, NULL, target_frame_addr,
        gcc_jit_type_get_pointer(resume_fn_ptr_type));
    gcc_jit_rvalue *target_resume_fn = gcc_jit_lvalue_as_rvalue(
        gcc_jit_rvalue_dereference(resume_slot_ptr, NULL));
    gcc_jit_rvalue *call_resume = gcc_jit_context_new_call_through_ptr(
        ctx, NULL, target_resume_fn, 1, &target_frame_addr);
    if (p_gcc_jit_rvalue_set_bool_require_tail_call) {
        p_gcc_jit_rvalue_set_bool_require_tail_call(call_resume, 1);
    }
    gcc_jit_block_add_eval(resume_blk, NULL, call_resume);
    gcc_jit_block_end_with_void_return(resume_blk, NULL);
    gcc_jit_block_end_with_void_return(done_blk, NULL);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_normalize_asm_constraints(
    const char *raw_constraint,
    int operand_index,
    char *out_output_constraint,
    size_t out_cap,
    char *out_tied_input_constraint,
    size_t tied_cap,
    a_boolean *out_is_read_write) GCC_GEN_BE_NOEXCEPT {
    if (!out_output_constraint || out_cap == 0 || !out_tied_input_constraint ||
        tied_cap == 0 || !out_is_read_write || operand_index < 0) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    out_output_constraint[0] = '\0';
    out_tied_input_constraint[0] = '\0';
    *out_is_read_write = FALSE;

    if (!raw_constraint || raw_constraint[0] == '\0' ||
        ((raw_constraint[0] != '=' && raw_constraint[0] != '+') || raw_constraint[1] == '\0')) {
        return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID, raw_constraint ? raw_constraint : "");
    }

    if (raw_constraint[0] == '+') {
        int w_out = snprintf(out_output_constraint, out_cap, "=%s", raw_constraint + 1);
        int w_tied = snprintf(out_tied_input_constraint, tied_cap, "%d", operand_index);
        if (w_out < 0 || (size_t)w_out >= out_cap || w_tied < 0 || (size_t)w_tied >= tied_cap) {
            return GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID;
        }
        *out_is_read_write = TRUE;
    } else {
        int w_out = snprintf(out_output_constraint, out_cap, "%s", raw_constraint);
        if (w_out < 0 || (size_t)w_out >= out_cap) {
            return GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
