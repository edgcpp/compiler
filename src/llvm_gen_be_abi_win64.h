/**
 * @file llvm_gen_be_abi_win64.h
 * @brief Microsoft Windows x64 ABI classification and lowering subsystem.
 * @details Implements the parameter passing and return value classification
 * rules specified in the Microsoft x64 calling convention.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_ABI_WIN64_H
#define LLVM_GEN_BE_ABI_WIN64_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/Value.h>

BEGIN_EDG_NAMESPACE

/**
 * @enum win64_abi_class_t
 * @brief Windows x64 ABI classification kinds.
 * @details Represents the target register classes and passing mechanisms used
 * for parameter passing and return values according to the Windows x64 ABI.
 */
enum class win64_abi_class_t {
  /// @brief Scalar integers, pointers, and composites of size 1, 2, 4, or 8 bytes.
  /// Passed in integer registers (RCX, RDX, R8, R9) or on the stack.
  direct_integer,

  /// @brief 32-bit and 64-bit floating point values.
  /// Passed in XMM registers (XMM0-XMM3) or on the stack.
  direct_float,

  /// @brief Homogeneous Vector Aggregate (HVA) or floating-point aggregate,
  /// specifically used for the __vectorcall convention (passed in XMM0-XMM5).
  vectorcall_hva,

  /// @brief Composites of sizes other than 1, 2, 4, 8 bytes, or non-trivial types.
  /// Passed indirectly by caller-allocated copy pointer.
  indirect_by_pointer
};

/**
 * @struct win64_arg_info_t
 * @brief Represents the classification and layout of a single argument or return value.
 * @details Stores the size, register slot index, and indirect flag as required
 * by the Microsoft x64 calling convention.
 */
struct win64_arg_info_t {
  /// @brief The ABI classification of the argument.
  win64_abi_class_t abi_class;

  /// @brief The size of the argument in bytes.
  uint64_t size;

  /// @brief True if the argument is passed indirectly by pointer.
  bool is_indirect;

  /// @brief True if the argument is a non-trivially copyable C++ class.
  bool pass_by_invisible_reference;

  /// @brief For vectorcall HVA, the underlying LLVM element type.
  llvm::Type* hva_elem_ty;

  /// @brief For vectorcall HVA, the number of elements (1 to 4).
  uint32_t num_elements;
};

/**
 * @brief Classifies a type according to the Windows x64 ABI parameter passing rules.
 * @details Analyzes a C/C++ type to determine its `win64_abi_class_t` and layout requirements.
 *
 * @param[in] ty The EDG type pointer to classify.
 * @param[in] is_vectorcall True if the function uses __vectorcall convention.
 * @param[out] out_info Pointer to the `win64_arg_info_t` struct to populate with classification results.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t classify_win64_argument(a_type_ptr ty, bool is_vectorcall, win64_arg_info_t* out_info) noexcept;

/**
 * @brief Computes the ABI return passing mechanism for Windows x64.
 *
 * @param[in] ret_ty The EDG return type pointer.
 * @param[in] is_vectorcall True if the function uses __vectorcall convention.
 * @param[out] out_info Pointer to the `win64_arg_info_t` struct to populate with return classification results.
 * @param[out] out_sret Pointer to boolean set to true if the return uses the `sret` (structural return) mechanism.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t compute_win64_return_info(a_type_ptr ret_ty, bool is_vectorcall, win64_arg_info_t* out_info, bool* out_sret) noexcept;

/**
 * @brief Builds an LLVM function type and attributes according to Windows x64 ABI.
 *
 * @param[in] routine_ty The EDG routine type to lower.
 * @param[out] out_fn_ty Pointer to output LLVM function type.
 * @param[out] out_attrs Pointer to output LLVM attribute list.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t build_win64_function_type(a_type_ptr routine_ty, llvm::FunctionType** out_fn_ty, llvm::AttributeList* out_attrs) noexcept;

/**
 * @brief Constructs the LLVM Type for a va_list under Windows x64 ABI.
 * @details Windows x64 va_list is just a continuous pointer (char*).
 * @param[out] out_ty Pointer to store the resulting LLVM PointerType.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_win64_va_list_type(llvm::Type** out_ty) noexcept;

/**
 * @brief Lowers a va_start call for Windows x64 ABI.
 *
 * @param[in] va_list_ptr The LLVM value pointing to the va_list.
 * @param[out] out_val The resulting LLVM value for the lowered intrinsic.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t lower_win64_va_start(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_arg macro expansion for Windows x64 ABI.
 * @details Traverses the stack pointer upwards (by 8 bytes per slot) for Windows x64 va_arg.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[in] ty The EDG type being requested.
 * @param[out] out_val The resulting loaded value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_win64_va_arg(llvm::Value* va_list_ptr, a_type_ptr ty, llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_copy intrinsic for Windows x64 ABI.
 * @details Copies the char* pointer.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list.
 * @param[in] src_va_list_ptr Pointer to the source va_list.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_win64_va_copy(llvm::Value* dest_va_list_ptr, llvm::Value* src_va_list_ptr, llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_end intrinsic for Windows x64 ABI.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_win64_va_end(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_ABI_WIN64_H */
