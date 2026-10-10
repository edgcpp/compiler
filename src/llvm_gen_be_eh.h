/**
 * @file llvm_gen_be_eh.h
 * @brief Exception Handling subsystem for the EDG LLVM backend.
 * @details Implements C++ exception lowering for both Itanium ABI and MSVC SEH.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_EH_H
#define LLVM_GEN_BE_EH_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers a C++ try block into LLVM IR.
 * @details Handles Itanium ABI landing pads and MSVC SEH funclets based on the target OS.
 * @param[in] stmt Pointer to the EDG try-block statement node.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_try_block_stmt(a_statement_ptr stmt) noexcept;

/**
 * @brief Lowers a Microsoft SEH __try/__except/__finally block into LLVM IR.
 * @details Translates Windows SEH constructs into LLVM catchswitch/catchpad or cleanuppad instructions.
 * @param[in] stmt Pointer to the EDG Microsoft try statement node.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_microsoft_try_stmt(a_statement_ptr stmt) noexcept;

/**
 * @brief Emits a cleanup action (destructor call) and unwinds.
 * @details Adds the specified destruction to the current active cleanups list, which are
 *          executed when an exception propagates out of the current scope.
 * @param[in] dtor The destructor routine to invoke.
 * @param[in] obj_ptr The pointer to the object being destroyed.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t llvm_emit_eh_cleanup(a_routine_ptr dtor, llvm::Value* obj_ptr) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_EH_H */
