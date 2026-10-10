/**
 * @file llvm_gen_be_abi_riscv64.h
 * @brief RISC-V 64-bit ABI lowering rules for the EDG LLVM backend.
 * @details Implements the standard RISC-V 64-bit ABI (lp64d) for argument passing.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
#ifndef LLVM_GEN_BE_ABI_RISCV64_H
#define LLVM_GEN_BE_ABI_RISCV64_H

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Attributes.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Builds the LLVM function type for a RISC-V 64-bit routine.
 * @details Applies LP64D rules, including integer/fp promotion and byval/sret.
 * @param[in] routine_ty The EDG routine type.
 * @param[out] out_fn_ty The generated LLVM function type.
 * @param[out] out_attrs The generated LLVM attribute list.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t build_riscv64_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept;

END_EDG_NAMESPACE
#endif // BACK_END_IS_LLVM_GEN_BE

#endif // LLVM_GEN_BE_ABI_RISCV64_H
