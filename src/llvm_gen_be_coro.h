/**
 * @file llvm_gen_be_coro.h
 * @brief C++20 Coroutine lowering subsystem for the EDG LLVM backend.
 * @details Handles translation of EDG coroutine statements (stmk_coroutine, stmk_coroutine_return)
 *          and expressions (co_await, co_yield) into LLVM Coroutine Intrinsics.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_CORO_H
#define LLVM_GEN_BE_CORO_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"

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
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_CORO_H */
