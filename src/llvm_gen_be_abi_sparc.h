/**
 * @file llvm_gen_be_abi_sparc.h
 * @ingroup llvm_gen_be_abi
 * @brief SPARC / SPARC V9 ABI classification subsystem.
 * @details Implements the parameter passing and return value classification
 * rules for SPARC / SPARC V9.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_ABI_SPARC_H
#define LLVM_GEN_BE_ABI_SPARC_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Attributes.h>

BEGIN_EDG_NAMESPACE

/**
 * @enum sparc_abi_class_t
 * @brief SPARC ABI classification kinds for arguments.
 * @details Specifies how an argument or return value is passed under SPARC V8/V9 ABIs.
 */
enum class sparc_abi_class_t {
  no_class,       ///< Unclassified or void type.
  integer,        ///< Passed in integer registers (%o0-%o5).
  float_class,    ///< Passed in floating-point registers (%f0-%f31).
  struct_byval,   ///< Aggregate passed indirectly via caller-allocated copy.
  struct_coerced, ///< Aggregate <= 32 bytes coerced into registers under SPARC V9.
  memory          ///< Passed on the stack frame.
};

/**
 * @struct sparc_arg_info_t
 * @brief Represents the classification result for a SPARC argument.
 * @details Encodes register class, indirection, sret requirement, and coerced type.
 */
struct sparc_arg_info_t {
  sparc_abi_class_t abi_class; ///< Determined SPARC ABI register/memory class.
  bool is_indirect;            ///< True if passed indirectly by pointer.
  bool requires_sret;          ///< True if returned via hidden sret pointer.
  llvm::Type* coerced_type;    ///< Coerced LLVM type for register passing.
};

/**
 * @brief Classifies a single EDG argument type according to SPARC / SPARC V9 ABI rules.
 * @details Determines whether the argument is passed in registers, coerced, or passed indirectly.
 * @param[in] ty The EDG type to classify.
 * @param[in] is_v9 True if targeting 64-bit SPARC V9; false for 32-bit SPARC V8.
 * @param[out] out_info Pointer to the sparc_arg_info_t structure receiving classification results.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_sparc_argument(
    a_type_ptr ty,
    bool is_v9,
    sparc_arg_info_t* out_info) noexcept;

/**
 * @brief Computes return value classification and sret requirement for SPARC / SPARC V9.
 * @details Determines whether the return value is returned in registers or via sret pointer.
 * @param[in] ret_ty The EDG return type.
 * @param[in] is_v9 True if targeting 64-bit SPARC V9; false for 32-bit SPARC V8.
 * @param[out] out_info Pointer to store the return classification info.
 * @param[out] out_sret Pointer to boolean set to true if sret is required.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t compute_sparc_return_info(
    a_type_ptr ret_ty,
    bool is_v9,
    sparc_arg_info_t* out_info,
    bool* out_sret) noexcept;

/**
 * @brief Builds the LLVM FunctionType and AttributeList for a SPARC / SPARC V9 routine.
 * @details Applies SPARC V8 or V9 ABI coercion and sret/byval attributes.
 * @param[in] routine_ty The EDG routine type.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_sparc_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept;

/**
 * @brief Lowers a va_start intrinsic for the SPARC ABI.
 * @details Invokes the llvm.va_start intrinsic on the va_list pointer.
 * @param[in] va_list_ptr Pointer to the va_list object.
 * @param[out] out_val Pointer to store the resulting CallInst.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_sparc_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_arg operation for the SPARC ABI.
 * @details Emits an LLVM VAArgInst for the requested EDG type.
 * @param[in] va_list_ptr Pointer to the va_list object.
 * @param[in] ty The EDG type being extracted.
 * @param[out] out_val Pointer to store the extracted LLVM value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_sparc_va_arg(
    llvm::Value* va_list_ptr,
    a_type_ptr ty,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_end intrinsic for the SPARC ABI.
 * @details Invokes the llvm.va_end intrinsic on the va_list pointer.
 * @param[in] va_list_ptr Pointer to the va_list object.
 * @param[out] out_val Pointer to store the resulting CallInst.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_sparc_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_copy intrinsic for the SPARC ABI.
 * @details Invokes the llvm.va_copy intrinsic on destination and source va_list pointers.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list object.
 * @param[in] src_va_list_ptr Pointer to the source va_list object.
 * @param[out] out_val Pointer to store the resulting CallInst.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_sparc_va_copy(
    llvm::Value* dest_va_list_ptr,
    llvm::Value* src_va_list_ptr,
    llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_ABI_SPARC_H */
