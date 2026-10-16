/**
 * @file llvm_gen_be_asm.h
 * @brief Inline Assembly subsystem for the EDG LLVM backend.
 * @details Implements inline assembly lowering for GNU-style asm and MSVC-style __asm.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_ASM_H
#define LLVM_GEN_BE_ASM_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers an inline assembly statement into LLVM IR.
 * @details Translates GNU-style extended asm (with operands, clobbers, labels)
 *          and MSVC-style __asm blocks into llvm::InlineAsm calls.
 * @param[in] stmt Pointer to the EDG asm statement node.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_ASM_H */
