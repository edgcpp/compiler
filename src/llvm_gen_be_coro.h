/**
 * @defgroup llvm_gen_be_coro C++20 Coroutines
 * @ingroup llvm_gen_be
 * @brief C++20 coroutine lowering subsystem for the EDG LLVM backend.
 */

/**
 * @file llvm_gen_be_coro.h
 * @ingroup llvm_gen_be_coro
 * @brief C++20 Coroutine lowering subsystem for the EDG LLVM backend.
 * @details Handles translation of EDG coroutine statements (stmk_coroutine, stmk_coroutine_return)
 *          and expressions (co_await, co_yield) into LLVM Coroutine Intrinsics.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_CORO_H
#define LLVM_GEN_BE_CORO_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers a coroutine description statement into LLVM IR.
 * @details Emits llvm.coro.id, llvm.coro.alloc, and llvm.coro.begin intrinsics.
 * @param[in] stmt Pointer to the EDG coroutine statement node.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_coroutine_stmt(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers a coroutine return statement into LLVM IR.
 * @details Emits llvm.coro.end and cleans up the coroutine frame.
 * @param[in] stmt Pointer to the EDG coroutine return statement node.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers a C++20 co_await or co_yield expression to LLVM IR.
 * @details Evaluates await_ready, emits llvm.coro.save and llvm.coro.suspend intrinsics,
 *          implements the 3-way resume/destroy/suspend switch, and evaluates await_resume.
 * @param[in] expr Pointer to the EDG enk_await or enk_yield expression node.
 * @param[out] out_val Receives the computed result of the await_resume call on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_coro_suspend(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Emits Heap Allocation Elision Optimization (HALO) hint attributes.
 * @details Applies coro_elide_safe and related optimization hints.
 * @param[in] func Pointer to the LLVM Function.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_coro_halo_hints(llvm::Function* func) noexcept;

/**
 * @brief Lowers coroutine frame allocation via custom `promise_type::operator new` or global `operator new`.
 * @details Forwards coroutine function parameters to overloaded `operator new(size_t, Args...)` when present,
 *          or falls back to target-specific aligned/unaligned global `operator new`.
 * @param[in] descr Pointer to the EDG coroutine descriptor.
 * @param[in] func Pointer to the enclosing LLVM coroutine function.
 * @param[in] size_val Lowered `@llvm.coro.size` frame byte size value.
 * @param[in] prom_align Alignment requirement of the coroutine promise (0 if default-aligned).
 * @param[out] out_alloc_ptr Receives the allocated coroutine frame memory pointer.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_coro_promise_alloc(
    a_coroutine_descr_ptr descr,
    llvm::Function* func,
    llvm::Value* size_val,
    uint32_t prom_align,
    llvm::Value** out_alloc_ptr) noexcept;

/**
 * @brief Emits frame-promoted parameter allocas and copies in the coroutine entry ramp.
 * @details Allocates coroutine frame slots for `this_param_copy` and `parameter_copies` before
 *          `llvm.coro.begin` and stores incoming function arguments into them so they survive across suspends.
 * @param[in] descr Pointer to the EDG coroutine descriptor.
 * @param[in] func Pointer to the enclosing LLVM coroutine function.
 * @param[out] out_copied_count Receives the number of parameter copies emitted into the coroutine frame.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_emit_coro_parameter_copies(
    a_coroutine_descr_ptr descr,
    llvm::Function* func,
    uint32_t* out_copied_count) noexcept;

/**
 * @brief Emits the coroutine unhandled-exception landing pad and catch handler.
 * @details Creates a landing pad (Itanium) or `catchswitch`/`catchpad` (MSVC) that invokes
 *          `promise.unhandled_exception()` and branches to the `final_suspend` label.
 * @param[in] descr Pointer to the EDG coroutine descriptor.
 * @param[in] func Pointer to the enclosing LLVM coroutine function.
 * @param[out] out_lpad_bb Receives the created unhandled-exception landing pad basic block.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_emit_coro_unhandled_exception_landing_pad(
    a_coroutine_descr_ptr descr,
    llvm::Function* func,
    llvm::BasicBlock** out_lpad_bb) noexcept;

/**
 * @brief Emits a call to the `@llvm.coro.noop()` intrinsic for `std::noop_coroutine()`.
 * @details Inserts a declaration for `@llvm.coro.noop` in the active module and emits a call returning
 *          the constant noop coroutine frame handle pointer.
 * @param[out] out_noop_hdl Receives the resulting `@llvm.coro.noop()` handle value.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p out_noop_hdl is null.
 */
llvm_gen_be_error_t llvm_lower_coro_noop(llvm::Value** out_noop_hdl) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_CORO_H */
