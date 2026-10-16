/**
 * @file llvm_gen_be_abi_aapcs64.h
 * @brief ARM64 AAPCS64 classification and lowering subsystem.
 * @details Implements the parameter passing and return value classification
 * rules specified in the Procedure Call Standard for the Arm 64-bit Architecture (AAPCS64).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_ABI_AAPCS64_H
#define LLVM_GEN_BE_ABI_AAPCS64_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Attributes.h>

BEGIN_EDG_NAMESPACE

/**
 * @enum aapcs64_abi_class_t
 * @brief AAPCS64 classification kinds for arguments and return values.
 * @details Represents how values are passed: in general-purpose registers,
 * SIMD/floating-point registers, or indirectly via memory.
 */
enum class aapcs64_abi_class_t {
  no_class,     ///< Ignored (e.g. empty struct on Darwin).
  integer,      ///< Passed in general-purpose integer registers (X0-X7).
  float_vector, ///< Passed in SIMD and Floating-point registers (V0-V7).
  hfa_hva,      ///< Homogeneous Floating-point/Vector Aggregate, passed in V registers.
  reference,    ///< Passed by reference (pointer in integer register or stack).
  memory        ///< Passed on the stack.
};

/**
 * @struct aapcs64_arg_info_t
 * @brief Represents the classification result for an AAPCS64 argument or return value.
 * @details Tracks the passing convention, the underlying element type (for HFA/HVA),
 * and the number of elements.
 */
struct aapcs64_arg_info_t {
  aapcs64_abi_class_t abi_class; ///< The ABI classification for the type.
  llvm::Type* hfa_hva_elem_ty;   ///< The element type if classified as HFA/HVA (nullptr otherwise).
  uint32_t num_elements;         ///< Number of elements (for HFA/HVA) or registers needed.
  bool is_sret;                  ///< True if this requires an sret pointer.
  bool pass_in_memory;           ///< True if this must be passed on the stack.
  bool pass_by_invisible_reference; ///< True if the argument is a non-trivially copyable C++ class.
};

/**
 * @brief Detects if a type is a Homogeneous Floating-point or Vector Aggregate (HFA/HVA).
 * @details Traverses struct fields and array elements to determine if the composite
 * consists solely of 1 to 4 identical floating-point or vector types.
 * @param[in] ty The EDG type to inspect.
 * @param[out] out_num_elements Number of homogeneous elements found.
 * @param[out] out_elem_ty The LLVM type of the homogeneous element.
 * @param[out] out_is_hfa True if the type qualifies as HFA or HVA.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t detect_hfa_hva(
    a_type_ptr ty,
    uint32_t* out_num_elements,
    llvm::Type** out_elem_ty,
    bool* out_is_hfa) noexcept;



/**
 * @brief Classifies a single EDG type into AAPCS64 ABI argument passing rules.
 * @details Evaluates scalars, arrays, pointers, and aggregates to determine 
 * register or memory allocation according to ARM64 rules. Handles Apple Darwin
 * ARM64 ABI specific quirks such as completely ignoring empty C++ struct types.
 * @param[in] ty The EDG type to classify.
 * @param[out] out_info The classification result information structure.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_aapcs64_argument(
    a_type_ptr ty,
    aapcs64_arg_info_t* out_info) noexcept;



/**
 * @brief Computes ARM64 AAPCS64 return value passing information.
 * @details Evaluates the return type according to AAPCS64 return rules. 
 * Determines if the type requires an sret pointer, or if it can be returned 
 * in registers. Empty struct types are correctly classified as ignored for
 * both C and C++ on all AAPCS64 platforms.
 * @param[in] ret_ty The EDG type representing the function return type.
 * @param[out] out_info The classification result information structure.
 * @param[out] out_sret Pointer to a boolean set to true if the return is via sret.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t compute_aapcs64_return_info(
    a_type_ptr ret_ty,
    aapcs64_arg_info_t* out_info,
    bool* out_sret) noexcept;



/**
 * @brief Constructs an LLVM FunctionType and AttributeList matching AAPCS64.
 * @details Evaluates function parameters and return type against the AAPCS64 classification engine.
 * Determines sret, byval/indirect passes, and register allocation. Tracks available General Purpose
 * (X0-X7) and Floating Point (V0-V7) registers.
 * @param[in] routine_ty The EDG routine type structure.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_aapcs64_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept;



/**
 * @brief Constructs the LLVM Type for a va_list under AAPCS64 ABI.
 * @details For Darwin, va_list is just a pointer (char*). For ELF/Linux, it's a struct 
 * [1 x { i8*, i8*, i8*, i32, i32 }].
 * @param[out] out_ty Pointer to store the resulting LLVM ArrayType or PointerType.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_aapcs64_va_list_type(
    llvm::Type** out_ty) noexcept;

/**
 * @brief Lowers a va_start intrinsic for the AAPCS64 ABI.
 * @details Casts the va_list pointer to an i8* and invokes the llvm.va_start intrinsic.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_aapcs64_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_arg macro expansion for the AAPCS64 ABI.
 * @details Reads arguments from the va_list structure. Uses LLVM's va_arg instruction
 * which is supported for AAPCS64 in the LLVM backend.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[in] ty The EDG type being requested.
 * @param[out] out_val The resulting loaded value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_aapcs64_va_arg(
    llvm::Value* va_list_ptr,
    a_type_ptr ty,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_copy intrinsic for the AAPCS64 ABI.
 * @details Invokes the llvm.va_copy intrinsic.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list struct.
 * @param[in] src_va_list_ptr Pointer to the source va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_aapcs64_va_copy(
    llvm::Value* dest_va_list_ptr,
    llvm::Value* src_va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_end intrinsic for the AAPCS64 ABI.
 * @details Invokes the llvm.va_end intrinsic.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_aapcs64_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Builds Windows ARM64EC calling convention thunk signatures and entrypoints.
 * @details Emulation Compatible ARM64EC thunks mapping AAPCS64 to x64 conventions.
 * @param[in] ty The EDG routine type.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_arm64ec_thunk_signature(
    a_type_ptr ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept;
END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_ABI_AAPCS64_H */

