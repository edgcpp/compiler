/**
 * @file llvm_gen_be_abi_s390x.h
 * @ingroup llvm_gen_be_abi
 * @brief SystemZ (s390x) ABI classification subsystem.
 * @details Implements the parameter passing and return value classification
 * rules for SystemZ (s390x).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_ABI_S390X_H
#define LLVM_GEN_BE_ABI_S390X_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Attributes.h>

BEGIN_EDG_NAMESPACE

/**
 * @enum s390x_abi_class_t
 * @brief s390x ABI classification kinds for arguments.
 * @details Specifies how an argument or return value is passed under the SystemZ ELF ABI.
 */
enum class s390x_abi_class_t {
  no_class,       ///< Unclassified or void type.
  integer,        ///< Passed in general-purpose registers (%r2-%r6).
  float_class,    ///< Passed in floating-point registers (%f0, %f2, %f4, %f6).
  memory,         ///< Passed indirectly or in the stack parameter area.
  struct_coerced  ///< Small power-of-two aggregate coerced into an integer register.
};

/**
 * @struct s390x_arg_info_t
 * @brief Represents the classification result for an s390x argument.
 * @details Encodes register class, indirection, sret requirement, and coerced type.
 */
struct s390x_arg_info_t {
  s390x_abi_class_t abi_class; ///< Determined s390x ABI register/memory class.
  bool is_indirect;            ///< True if passed indirectly by hidden reference.
  bool requires_sret;          ///< True if returned via hidden sret pointer in %r2.
  llvm::Type* coerced_type;    ///< Coerced LLVM type for direct register passing.
};

/**
 * @brief Classifies a single EDG argument type according to SystemZ (s390x) ABI rules.
 * @details Determines whether the argument is passed in GPRs, FPRs, coerced into an integer,
 * or passed indirectly in memory.
 * @param[in] ty The EDG type to classify.
 * @param[out] out_info Pointer to the s390x_arg_info_t structure receiving classification results.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_s390x_argument(
    a_type_ptr ty,
    s390x_arg_info_t* out_info) noexcept;

/**
 * @brief Computes return value classification and sret requirement for SystemZ (s390x).
 * @details Determines whether the return value is returned in registers or via sret pointer.
 * @param[in] ret_ty The EDG return type.
 * @param[out] out_info Pointer to store the return classification info.
 * @param[out] out_sret Pointer to boolean set to true if sret is required.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t compute_s390x_return_info(
    a_type_ptr ret_ty,
    s390x_arg_info_t* out_info,
    bool* out_sret) noexcept;

/**
 * @brief Builds the LLVM FunctionType and AttributeList for a SystemZ (s390x) routine.
 * @details Applies SystemZ ABI coercion and sret/byval attributes.
 * @param[in] routine_ty The EDG routine type.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_s390x_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept;

/**
 * @brief Constructs the LLVM Type for a va_list under SystemZ (s390x) ABI.
 * @details The s390x va_list type is `[1 x { i64, i64, ptr, ptr }]` representing
 *          `__gpr`, `__fpr`, `__overflow_arg_area`, and `__reg_save_area`.
 * @param[out] out_ty Pointer to store the resulting LLVM ArrayType.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_ty` is null.
 */
llvm_gen_be_error_t build_s390x_va_list_type(
    llvm::Type** out_ty) noexcept;

/**
 * @brief Lowers a va_start intrinsic for the SystemZ (s390x) ABI.
 * @details Invokes the llvm.va_start intrinsic on the va_list pointer.
 * @param[in] va_list_ptr Pointer to the va_list object.
 * @param[out] out_val Pointer to store the resulting CallInst.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_s390x_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_arg operation for the SystemZ (s390x) ABI.
 * @details Emits an LLVM VAArgInst for the requested EDG type.
 * @param[in] va_list_ptr Pointer to the va_list object.
 * @param[in] ty The EDG type being extracted.
 * @param[out] out_val Pointer to store the extracted LLVM value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_s390x_va_arg(
    llvm::Value* va_list_ptr,
    a_type_ptr ty,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_end intrinsic for the SystemZ (s390x) ABI.
 * @details Invokes the llvm.va_end intrinsic on the va_list pointer.
 * @param[in] va_list_ptr Pointer to the va_list object.
 * @param[out] out_val Pointer to store the resulting CallInst.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_s390x_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_copy intrinsic for the SystemZ (s390x) ABI.
 * @details Invokes the llvm.va_copy intrinsic on destination and source va_list pointers.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list object.
 * @param[in] src_va_list_ptr Pointer to the source va_list object.
 * @param[out] out_val Pointer to store the resulting CallInst.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_s390x_va_copy(
    llvm::Value* dest_va_list_ptr,
    llvm::Value* src_va_list_ptr,
    llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_ABI_S390X_H */
