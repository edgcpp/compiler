/**
 * @file gcc_gen_be_stmt.h
 * @brief Statement lowering for the GCC backend.
 *
 * This file declares functions for lowering EDG AST statement nodes
 * into libgccjit blocks and operations.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_STMT_H
#define GCC_GEN_BE_STMT_H

#include "gcc_gen_be_error.h"
#include "fe_common.h"
#include "statements.h"

struct gcc_jit_block;
struct gcc_jit_function;
struct gcc_jit_struct;
struct gcc_jit_type;
struct gcc_jit_lvalue;
struct gcc_jit_rvalue;

BEGIN_EDG_NAMESPACE

/**
 * @brief Retrieves or creates a libgccjit block for a given label.
 *
 * @param func The parent function.
 * @param label The frontend label node.
 * @param out_block A pointer to a gcc_jit_block pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_block` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_label_block(struct gcc_jit_function *func, a_label_ptr label, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Resolves or creates the unique `gcc_jit_block` for a label using `GCC_GEN_BE_CACHE_LABEL`.
 * @details Ensures both `stmk_label` in statement lowering and `abk_label` in address-of-label
 * (`&&label`) constant lowering resolve the exact same `gcc_jit_block *` for a given `a_label_ptr`.
 * @param[in] label The frontend label entry.
 * @param[in] func The containing `gcc_jit_function` (or NULL to infer from the active basic block).
 * @param[out] out_block Pointer to receive the resolved `gcc_jit_block`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_or_create_label_block(
    a_label_ptr label,
    struct gcc_jit_function *func,
    struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Retrieves or creates a libgccjit block for a given switch case entry.
 *
 * @param func The parent function.
 * @param scep The frontend switch case entry.
 * @param out_block A pointer to a gcc_jit_block pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_block` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_switch_case_block(struct gcc_jit_function *func, a_switch_case_entry_ptr scep, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG statement node into the current libgccjit block.
 *
 * @param stmt The frontend statement node.
 * @param func The parent libgccjit function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Finalizes the computed goto (`&&label` / `goto *ptr`) switch dispatcher for a function.
 * @details Synthesizes the switch dispatcher block over all address-taken labels in `func`,
 * supporting functions with multiple `goto *` statements, label address comparisons (`&&l1 == &&l2`),
 * and static array tables of label addresses.
 * @param[in] func The `gcc_jit_function` being finalized.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_finalize_computed_goto(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a Microsoft Structured Exception Handling (`__try` / `__except` / `__finally`) statement (`stmk_microsoft_try`).
 * @details Supports abnormal termination tracking (`_abnormal_termination()`), early-exit `__finally`
 * execution (`return`, `break`, `continue`, `goto`), 3-way `__except` filter dispatch (`1`, `0`, `-1`),
 * `_exception_code()` / `_exception_info()` intrinsic bindings, and Win64 `.pdata`/`.xdata` table emission.
 * @param[in] stmt The `stmk_microsoft_try` statement node.
 * @param[in] func The active `gcc_jit_function`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_microsoft_try(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a stack save operation.
 * @details Emits a call to __builtin_stack_save.
 * @param[in] func The current function.
 * @param[in] block The current block.
 * @param[out] out_save Pointer to receive the saved stack variable.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_stack_save(struct gcc_jit_function *func, struct gcc_jit_block *block, struct gcc_jit_lvalue **out_save) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a stack restore operation.
 * @details Emits a call to __builtin_stack_restore.
 * @param[in] func The current function.
 * @param[in] block The current block.
 * @param[in] save_var The saved stack variable.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_stack_restore(struct gcc_jit_function *func, struct gcc_jit_block *block, struct gcc_jit_lvalue *save_var) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a variable-length array (VLA) local variable declaration (`stmk_vla_decl` / `dik_vla_alloc`).
 * @details Computes the runtime byte size of the VLA from its dimension expressions and invokes
 * `__builtin_alloca` to allocate stack storage, caching the resulting pointer lvalue in `GCC_GEN_BE_CACHE_VAR`.
 * @param[in] func The active `gcc_jit_function`.
 * @param[in] var The frontend VLA variable entry.
 * @param[out] out_lval Pointer to receive the allocated VLA local lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_vla_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Computes the dynamic coroutine frame struct layout and total byte size.
 * @details Constructs a per-coroutine `gcc_jit_struct` containing `void (*__resume_fn)(void *)` at offset 0,
 * `void (*__destroy_fn)(void *)` at offset `sizeof(void *)`, the lowered `promise_type` field at offset `2 * sizeof(void *)`,
 * `int __suspend_index`, `int __coro_flags`, copied function parameter fields, and promoted local variables and temporaries.
 * @param[in] rout The coroutine routine entry.
 * @param[in] scope The function scope (may be NULL to resolve from `rout`).
 * @param[out] out_frame_struct Pointer to receive the constructed `gcc_jit_struct`.
 * @param[out] out_frame_size Pointer to receive the computed frame byte size.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_compute_coro_frame_layout(
    a_routine_ptr rout,
    a_scope_ptr scope,
    struct gcc_jit_struct **out_frame_struct,
    size_t *out_frame_size) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers the heap-allocated C++20 coroutine frame structure type for a routine.
 * @details Computes and caches the `gcc_jit_struct` layout containing `__resume_fn`,
 * `__destroy_fn`, `__promise`, `__suspend_index`, `initial_await_resume_called`,
 * `__coro_flags`, copied parameters, and spilled cross-suspend local variables.
 * @param[in] rout The coroutine routine entry.
 * @param[out] out_frame_type Pointer to receive the lowered `gcc_jit_type *` for the coroutine frame.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_coroutine_frame(a_routine_ptr rout, struct gcc_jit_type **out_frame_type) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers the C++20 coroutine ramp function body.
 * @details Allocates the coroutine frame via promise or global `operator new`, handles
 * `get_return_object_on_allocation_failure()` when allocation is `noexcept`, initializes
 * `__resume_fn`, `__destroy_fn`, and `__suspend_index = 0`, copies parameters into the frame,
 * constructs the promise object, evaluates `get_return_object()`, invokes the actor function for
 * initial suspend, and returns the coroutine return object to the caller.
 * @param[in] rout The coroutine routine entry.
 * @param[in] ramp_func The `gcc_jit_function *` representing the coroutine ramp function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_coroutine_ramp(a_routine_ptr rout, struct gcc_jit_function *ramp_func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers coroutine dispatchers.
 * @details Synthesizes the resume and destroy state-machine dispatchers for a coroutine.
 * @param[in] rout Routine.
 * @param[out] out_resume Resume fn.
 * @param[out] out_destroy Destroy fn.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_coroutine_dispatchers(a_routine_ptr rout, struct gcc_jit_function **out_resume, struct gcc_jit_function **out_destroy) GCC_GEN_BE_NOEXCEPT;

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
extern gcc_gen_be_error_t gcc_gen_be_register_coro_suspend_point(
    struct gcc_jit_block *resume_block,
    int *out_state_idx,
    struct gcc_jit_lvalue **out_state_lval,
    struct gcc_jit_rvalue **out_frame_ptr_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Retrieves the active coroutine frame struct type and frame pointer lvalue.
 * @details Used by `<coroutine>` intrinsics (`__builtin_coro_*`) and expression lowering.
 * @param[out] out_frame_type Pointer to receive the active coroutine frame `gcc_jit_type *` (or NULL).
 * @param[out] out_frame_lval Pointer to receive the active coroutine frame `gcc_jit_lvalue *` (or NULL).
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_active_coroutine_frame(
    struct gcc_jit_type **out_frame_type,
    struct gcc_jit_lvalue **out_frame_lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Evaluates whether a coroutine frame qualifies for Heap Allocation Elision Optimization (HALO).
 * @details Checks if the coroutine descriptor is present and has a non-escaping frame allocation profile.
 * @param[in] descr The EDG coroutine descriptor pointer.
 * @param[out] out_can_elide Pointer to receive TRUE if the frame can be elided onto the caller stack, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if out_can_elide is NULL.
 */
extern gcc_gen_be_error_t gcc_gen_be_try_coro_halo(
    a_coroutine_descr_ptr descr,
    a_boolean *out_can_elide) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Synthesizes or retrieves the `std::noop_coroutine()` frame singleton (`__gcc_be_noop_coro_frame`).
 * @details Emits a static coroutine frame header whose `__resume_fn` and `__destroy_fn` are no-op functions.
 * @param[out] out_noop_frame Pointer to receive the `__gcc_be_noop_coro_frame` global lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_noop_coroutine_frame(
    struct gcc_jit_lvalue **out_noop_frame) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Computes the exact cumulative aligned byte size and alignment of a coroutine frame.
 * @details Walks the coroutine header (`__resume_fn`, `__destroy_fn`), promise type, state/flag fields,
 * parameter copies, and scope local/temporary variables using EDG `a_type_ptr` size and alignment metadata.
 * @param[in] rout The coroutine routine entry.
 * @param[in] scope Optional function scope (or NULL to resolve from `rout`).
 * @param[out] out_frame_size Pointer to receive the aligned total byte size of the coroutine frame.
 * @param[out] out_frame_align Pointer to receive the maximum byte alignment requirement of the coroutine frame.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if required pointers are NULL.
 */
extern gcc_gen_be_error_t gcc_gen_be_compute_coro_frame_size_and_align(
    a_routine_ptr rout,
    a_scope_ptr scope,
    size_t *out_frame_size,
    size_t *out_frame_align) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a C++20 coroutine symmetric transfer sequence in `suspend_block`.
 * @details Extracts the target coroutine frame `void *` address from `handle_rval` (supporting both
 * pointer and struct `std::coroutine_handle<P>` values), falls back to `__gcc_be_noop_coro_frame` if NULL,
 * loads `__resume_fn` at offset 0 of the target frame, invokes `__resume_fn(target_frame)`, and ends
 * `suspend_block` with a `void` return.
 * @param[in] func The active coroutine resume `gcc_jit_function`.
 * @param[in] suspend_block The basic block handling the `await_suspend` transfer.
 * @param[in] handle_rval The lowered `await_suspend` return value representing the target coroutine handle.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_coro_symmetric_transfer(
    struct gcc_jit_function *func,
    struct gcc_jit_block *suspend_block,
    struct gcc_jit_rvalue *handle_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Normalizes extended inline assembly output operand constraints for `libgccjit`.
 * @details Splits read-modify-write (`+r`, `+m`, `+x`, `+&r`) output constraints into an
 * `=`-prefixed output constraint in `out_output_constraint` (`=r`, `=m`, `=x`, `=&r`) paired
 * with a numeric tied input constraint in `out_tied_input_constraint` (`"0"`, `"1"`, etc.),
 * and sets `*out_is_read_write = TRUE`. Validates that output constraints begin with `=` or `+`.
 * @param[in] raw_constraint The raw output operand constraint string from the frontend.
 * @param[in] operand_index Zero-based output operand index (`>= 0`).
 * @param[out] out_output_constraint Destination buffer for the normalized output constraint.
 * @param[in] out_cap Capacity of `out_output_constraint` in bytes.
 * @param[out] out_tied_input_constraint Destination buffer for the tied input constraint (when `+`).
 * @param[in] tied_cap Capacity of `out_tied_input_constraint` in bytes.
 * @param[out] out_is_read_write Pointer to receive `TRUE` if `raw_constraint` is read-modify-write (`+`).
 * @return `GCC_GEN_BE_SUCCESS` on success, `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` on NULL/empty buffers,
 * or `GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID` if `raw_constraint` is malformed.
 */
extern gcc_gen_be_error_t gcc_gen_be_normalize_asm_constraints(
    const char *raw_constraint,
    int operand_index,
    char *out_output_constraint,
    size_t out_cap,
    char *out_tied_input_constraint,
    size_t tied_cap,
    a_boolean *out_is_read_write) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif
