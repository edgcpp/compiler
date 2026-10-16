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
#include <llvm/IR/Value.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @struct riscv64_arg_info_t
 * @brief Classification info for a RISC-V argument.
 */
struct riscv64_arg_info_t {
    uint64_t size;           ///< Size of the argument.
    bool is_indirect;        ///< True if passed by reference (size > 16 bytes).
    llvm::Type* coerced_type; ///< LLVM type to coerce to (e.g. {double, double}).
};

/**
 * @brief Classifies a RISC-V 64 argument type.
 * @param[in] ty EDG type to classify.
 * @param[out] out_info Classification info output.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t classify_riscv64_argument(a_type_ptr ty, riscv64_arg_info_t* out_info) noexcept;

/**
 * @brief Computes the return info for RISC-V 64.
 * @param[in] ret_ty EDG return type.
 * @param[out] out_info Classification info output.
 * @param[out] out_sret Set to true if an sret pointer is required.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t compute_riscv64_return_info(a_type_ptr ret_ty, riscv64_arg_info_t* out_info, bool* out_sret) noexcept;

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

/**
 * @brief Builds a RISC-V va_list type.
 * @param[out] out_ty Output pointer.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t build_riscv64_va_list_type(llvm::Type** out_ty) noexcept;

/**
 * @brief Lowers a RISC-V va_start.
 * @param[in] va_list_ptr Pointer.
 * @param[out] out_val Output value.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_riscv64_va_start(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a RISC-V va_arg.
 * @param[in] va_list_ptr Pointer.
 * @param[in] ty Requested type.
 * @param[out] out_val Output value.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_riscv64_va_arg(llvm::Value* va_list_ptr, a_type_ptr ty, llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a RISC-V va_copy.
 * @param[in] dest_va_list_ptr Destination pointer.
 * @param[in] src_va_list_ptr Source pointer.
 * @param[out] out_val Output value.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_riscv64_va_copy(llvm::Value* dest_va_list_ptr, llvm::Value* src_va_list_ptr, llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a RISC-V va_end.
 * @param[in] va_list_ptr Pointer.
 * @param[out] out_val Output value.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_riscv64_va_end(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE
#endif // BACK_END_IS_LLVM_GEN_BE

#endif // LLVM_GEN_BE_ABI_RISCV64_H
