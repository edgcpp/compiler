/**
 * @file llvm_gen_be_coro.h
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

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_CORO_H */
