/**
 * @file llvm_gen_be_abi_ppc64.h
 * @brief PowerPC 64 (ELFv2) ABI classification subsystem.
 * @details Implements the parameter passing and return value classification
 * rules for PowerPC 64 (ELFv2).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_ABI_PPC64_H
#define LLVM_GEN_BE_ABI_PPC64_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Attributes.h>

BEGIN_EDG_NAMESPACE

/**
 * @enum ppc64_abi_class_t
 * @brief PPC64 ABI classification kinds for arguments.
 */
enum class ppc64_abi_class_t {
  no_class,
  integer,
  float_class,
  vector,
  hfa,
  memory,
  struct_byval
};

/**
 * @struct ppc64_arg_info_t
 * @brief Represents the classification result for a PPC64 argument.
 */
struct ppc64_arg_info_t {
  ppc64_abi_class_t abi_class;
  bool is_indirect;
  bool requires_sret;
  llvm::Type* coerced_type;
};

/**
 * @brief Classifies a single EDG type into PPC64 ABI class.
 * @param[in] ty The EDG type to classify.
 * @param[in] is_elfv2 Whether ELFv2 ABI is used.
 * @param[out] out_info The classification result information structure.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_ppc64_argument(
    a_type_ptr ty,
    bool is_elfv2,
    ppc64_arg_info_t* out_info) noexcept;

/**
 * @brief Computes PPC64 return value passing information.
 * @param[in] ret_ty The EDG type representing the function return type.
 * @param[in] is_elfv2 Whether ELFv2 ABI is used.
 * @param[out] out_info The classification result information structure.
 * @param[out] out_sret Pointer to a boolean set to true if the return is via sret.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t compute_ppc64_return_info(
    a_type_ptr ret_ty,
    bool is_elfv2,
    ppc64_arg_info_t* out_info,
    bool* out_sret) noexcept;

/**
 * @brief Constructs an LLVM FunctionType and AttributeList matching PowerPC 64 ABI.
 * @param[in] routine_ty The EDG routine type structure.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_ppc64_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept;

/**
 * @brief Lowers a va_start intrinsic for the PowerPC 64 ABI.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_ppc64_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_arg macro expansion for the PowerPC 64 ABI.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[in] ty The EDG type being requested.
 * @param[out] out_val The resulting loaded value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_ppc64_va_arg(
    llvm::Value* va_list_ptr,
    a_type_ptr ty,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_end intrinsic for the PowerPC 64 ABI.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_ppc64_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_copy intrinsic for the PowerPC 64 ABI.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list struct.
 * @param[in] src_va_list_ptr Pointer to the source va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_ppc64_va_copy(
    llvm::Value* dest_va_list_ptr,
    llvm::Value* src_va_list_ptr,
    llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_ABI_PPC64_H */
