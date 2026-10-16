/**
 * @file llvm_gen_be_abi_loongarch64.h
 * @brief LoongArch 64 ABI classification subsystem.
 * @details Implements the parameter passing and return value classification
 * rules for LoongArch 64.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_ABI_LOONGARCH64_H
#define LLVM_GEN_BE_ABI_LOONGARCH64_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Attributes.h>

BEGIN_EDG_NAMESPACE

/**
 * @enum loongarch64_abi_class_t
 * @brief LoongArch 64 classification kinds for arguments.
 * @details Represents the target register classes used for parameter passing
 * according to the LoongArch ELF ABI specification.
 */
enum class loongarch64_abi_class_t {
  no_class,         ///< Ignored (e.g. empty struct)
  integer,          ///< Passed in GARs ($a0-$a7)
  float_class,      ///< Passed in FARs ($fa0-$fa7)
  float_float,      ///< Struct with 2 floats (HFA), passed in 2 FARs
  integer_float,    ///< Struct with 1 int and 1 float, passed in 1 GAR and 1 FAR
  float_integer,    ///< Struct with 1 float and 1 int, passed in 1 FAR and 1 GAR
  integer_integer,  ///< Struct with 2 ints (or smaller), passed in 2 GARs
  memory            ///< Passed in memory (structs > 16 bytes)
};

/**
 * @struct loongarch64_arg_info_t
 * @brief Represents the classification result for a LoongArch 64 argument.
 */
struct loongarch64_arg_info_t {
  loongarch64_abi_class_t abi_class; ///< The determined ABI classification.
  bool is_indirect;                  ///< Set to true if passed by reference (pointer in GAR).
  bool requires_sret;                ///< Set to true if the return value requires an sret pointer.
  llvm::Type* coerced_type;          ///< Type to coerce to for passing in registers.
};

/**
 * @brief Classifies a single EDG type into LoongArch 64 ABI class.
 * @param[in] ty The EDG type to classify.
 * @param[out] out_info The classification result information structure.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_loongarch64_argument(
    a_type_ptr ty,
    loongarch64_arg_info_t* out_info) noexcept;

/**
 * @brief Computes LoongArch 64 return value passing information.
 * @param[in] ret_ty The EDG type representing the function return type.
 * @param[out] out_info The classification result information structure.
 * @param[out] out_sret Pointer to a boolean set to true if the return is via sret.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t compute_loongarch64_return_info(
    a_type_ptr ret_ty,
    loongarch64_arg_info_t* out_info,
    bool* out_sret) noexcept;

/**
 * @brief Constructs an LLVM FunctionType and AttributeList matching LoongArch 64 ABI.
 * @param[in] routine_ty The EDG routine type structure.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_loongarch64_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept;

/**
 * @brief Lowers a va_start intrinsic for the LoongArch 64 ABI.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_loongarch64_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_arg macro expansion for the LoongArch 64 ABI.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[in] ty The EDG type being requested.
 * @param[out] out_val The resulting loaded value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_loongarch64_va_arg(
    llvm::Value* va_list_ptr,
    a_type_ptr ty,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_end intrinsic for the LoongArch 64 ABI.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_loongarch64_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_copy intrinsic for the LoongArch 64 ABI.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list struct.
 * @param[in] src_va_list_ptr Pointer to the source va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_loongarch64_va_copy(
    llvm::Value* dest_va_list_ptr,
    llvm::Value* src_va_list_ptr,
    llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_ABI_LOONGARCH64_H */
