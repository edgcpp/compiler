/**
 * @file llvm_gen_be_abi_sysv_x86_64.h
 * @brief System V AMD64 ABI classification and lowering subsystem.
 * @details Implements the parameter passing and return value classification
 * rules specified in the System V Application Binary Interface AMD64 Architecture
 * Processor Supplement (Draft Version 1.0).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_ABI_SYSV_X86_64_H
#define LLVM_GEN_BE_ABI_SYSV_X86_64_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Attributes.h>

BEGIN_EDG_NAMESPACE

/**
 * @enum x86_64_abi_class_t
 * @brief System V AMD64 classification kinds for eightbytes.
 * @details Represents the target register classes used for parameter passing
 * and return values according to the AMD64 ABI classification algorithm.
 */
enum class x86_64_abi_class_t {
  no_class,    ///< Ignored for parameter passing (e.g. empty structs or padding).
  integer,     ///< Passed in general-purpose integer registers (RDI, RSI, RDX, RCX, R8, R9).
  sse,         ///< Passed in SSE vector registers (XMM0-XMM7).
  sseup,       ///< Passed in the upper half of an SSE register (for 256/512-bit vectors).
  x87,         ///< Passed in the x87 FPU stack (for long double, 80-bit floats).
  x87up,       ///< Upper padding of an 80-bit x87 value.
  complex_x87, ///< Passed in x87 FPU stack as a complex number.
  memory       ///< Passed in memory via the stack or indirectly (e.g. large structs).
};

/**
 * @struct x86_64_abi_arg_info_t
 * @brief Represents the classification result for a System V AMD64 argument.
 * @details Stores the ABI classification for up to two 8-byte chunks (eightbytes)
 * that make up an argument, as well as flags indicating memory pass-by-reference.
 */
struct x86_64_abi_arg_info_t {
  x86_64_abi_class_t eightbyte_classes[2]; ///< Classification for the first and second eightbytes.
  uint32_t num_eightbytes;                 ///< The number of valid eightbytes (0, 1, or 2).
  bool pass_in_memory;                     ///< Set to true if the entire argument must be passed in memory.
  bool requires_sret;                      ///< Set to true if the return value requires an sret pointer.
};

/**
 * @brief Classifies a single EDG type into System V AMD64 ABI eightbyte classes.
 * @details Evaluates scalars, arrays, pointers, and aggregates against the ABI rules
 * to determine register or memory allocation. Empty structs map to no_class. Types
 * larger than 16 bytes default to memory.
 * @param[in] ty The EDG type to classify.
 * @param[in] offset The current byte offset of the type within a parent aggregate (usually 0).
 * @param[out] out_info The classification result information structure.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_sysv_argument(
    a_type_ptr ty,
    uint64_t offset,
    x86_64_abi_arg_info_t* out_info) noexcept;

/**
 * @brief Computes System V AMD64 return value passing information.
 * @details Evaluates the return type according to System V ABI return rules. 
 * Determines if the type requires an sret pointer, or if it can be returned 
 * in registers.
 * @param[in] ret_ty The EDG type representing the function return type.
 * @param[out] out_info The classification result information structure.
 * @param[out] out_sret Pointer to a boolean set to true if the return is via sret.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t compute_sysv_return_info(
    a_type_ptr ret_ty,
    x86_64_abi_arg_info_t* out_info,
    bool* out_sret) noexcept;

/**
 * @brief Constructs an LLVM FunctionType and AttributeList matching System V AMD64 ABI.
 * @details Evaluates function parameters and return type against the classification engine.
 * Determines sret, byval, and register allocation. Tracks available integer and SSE
 * registers.
 * @param[in] routine_ty The EDG routine type structure.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_sysv_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept;

/**
 * @brief Lowers a va_start intrinsic for the System V AMD64 ABI.
 * @details Casts the va_list pointer to an i8* and invokes the llvm.va_start intrinsic.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_sysv_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_arg macro expansion for the System V AMD64 ABI.
 * @details Reads arguments from the gp_offset, fp_offset, or overflow_arg_area
 * depending on the classification of the requested type.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[in] ty The EDG type being requested.
 * @param[out] out_val The resulting loaded value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_sysv_va_arg(
    llvm::Value* va_list_ptr,
    a_type_ptr ty,
    llvm::Value** out_val) noexcept;



/**
 * @brief Lowers a va_end intrinsic for the System V AMD64 ABI.
 * @details Invokes the llvm.va_end intrinsic.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_sysv_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_copy intrinsic for the System V AMD64 ABI.
 * @details Invokes the llvm.va_copy intrinsic.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list struct.
 * @param[in] src_va_list_ptr Pointer to the source va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_sysv_va_copy(
    llvm::Value* dest_va_list_ptr,
    llvm::Value* src_va_list_ptr,
    llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_ABI_SYSV_X86_64_H */
