/**
 * @file llvm_gen_be_abi_mips.h
 * @ingroup llvm_gen_be_abi
 * @brief MIPS / MIPS64 ABI classification subsystem.
 * @details Implements the parameter passing and return value classification
 * rules for MIPS / MIPS64.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_ABI_MIPS_H
#define LLVM_GEN_BE_ABI_MIPS_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Attributes.h>

BEGIN_EDG_NAMESPACE

/**
 * @enum mips_abi_class_t
 * @brief MIPS ABI classification kinds for arguments.
 * @details Specifies how an argument or return value is passed under the MIPS O32/N64 ABIs.
 */
enum class mips_abi_class_t {
  no_class,       ///< Unclassified or void type.
  integer,        ///< Passed in general-purpose integer registers ($a0-$a7).
  float_class,    ///< Passed in floating-point registers ($f12-$f19).
  struct_byval,   ///< Aggregate passed indirectly on stack via byval pointer.
  struct_coerced, ///< Aggregate coerced into integer register chunks.
  memory          ///< Passed on the stack frame.
};

/**
 * @struct mips_arg_info_t
 * @brief Represents the classification result for a MIPS argument.
 * @details Encodes register class, indirection, sret requirement, and coerced type.
 */
struct mips_arg_info_t {
  mips_abi_class_t abi_class; ///< Determined MIPS ABI register/memory class.
  bool is_indirect;           ///< True if passed indirectly by reference.
  bool requires_sret;         ///< True if returned via hidden sret pointer.
  llvm::Type* coerced_type;   ///< Coerced LLVM type for register packing.
  uint32_t align;             ///< Required alignment in bytes.
  uint32_t padding;           ///< Required padding in bytes.
};

/**
 * @brief Classifies a single EDG argument type according to MIPS / MIPS64 ABI rules.
 * @details Determines whether the argument is passed in integer registers, floating-point
 * registers, coerced into an integer/FP array or struct, or passed by value on the stack.
 * @param[in] ty The EDG type to classify.
 * @param[in] is_soft_float True if soft-float ABI is active.
 * @param[in] is_n64 True if targeting 64-bit MIPS N64 ABI; false for 32-bit O32 ABI.
 * @param[out] out_info Pointer to the mips_arg_info_t structure receiving classification results.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_mips_argument(
    a_type_ptr ty,
    bool is_soft_float,
    bool is_n64,
    mips_arg_info_t* out_info) noexcept;

/**
 * @brief Computes MIPS return value passing information.
 * @details Determines whether the return value is passed in registers, coerced, or via hidden `sret` pointer.
 * @param[in] ret_ty The EDG type representing the function return type.
 * @param[in] is_soft_float Whether soft-float ABI is used.
 * @param[in] is_n64 Whether N64 ABI is used (64-bit pointers).
 * @param[out] out_info The classification result information structure.
 * @param[out] out_sret Pointer to a boolean set to true if the return is via sret.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t compute_mips_return_info(
    a_type_ptr ret_ty,
    bool is_soft_float,
    bool is_n64,
    mips_arg_info_t* out_info,
    bool* out_sret) noexcept;

/**
 * @brief Constructs an LLVM FunctionType and AttributeList matching MIPS / MIPS64 ABI.
 * @details Classifies return and parameter types and attaches `sret`/`byval` attributes as needed.
 * @param[in] routine_ty The EDG routine type structure.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_mips_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept;

/**
 * @brief Lowers a va_start intrinsic for the MIPS / MIPS64 ABI.
 * @details Emits `llvm.va_start` on the provided `va_list` pointer.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_mips_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_arg macro expansion for the MIPS ABI.
 * @details Emits an LLVM `va_arg` instruction for the requested EDG type.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[in] ty The EDG type being requested.
 * @param[out] out_val The resulting loaded value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_mips_va_arg(
    llvm::Value* va_list_ptr,
    a_type_ptr ty,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_end intrinsic for the MIPS ABI.
 * @details Emits `llvm.va_end` on the provided `va_list` pointer.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_mips_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_copy intrinsic for the MIPS ABI.
 * @details Emits `llvm.va_copy` from `src_va_list_ptr` to `dest_va_list_ptr`.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list struct.
 * @param[in] src_va_list_ptr Pointer to the source va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_mips_va_copy(
    llvm::Value* dest_va_list_ptr,
    llvm::Value* src_va_list_ptr,
    llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_ABI_MIPS_H */
