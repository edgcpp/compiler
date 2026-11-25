/**
 * @defgroup llvm_gen_be_abi Target Calling Convention ABIs
 * @ingroup llvm_gen_be
 * @brief Target calling-convention ABI classification and coercion subsystems.
 */

/**
 * @file llvm_gen_be_abi_sysv_x86_64.h
 * @ingroup llvm_gen_be_abi
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
  bool pass_by_invisible_reference;        ///< Set to true if the argument is a non-trivially copyable C++ class.
};

/**
 * @brief Resolves the effective non-zero byte alignment of a System V AMD64 aggregate field type.
 * @details Queries `alignment_of_type(field_ty)` and normalizes zero alignment (which occurs on
 *          zero-sized/empty types or synthetic zero-initialized AST nodes whose `alignment` field is `0`)
 *          to `1` byte so natural-alignment modulo checks (`field_offset % field_align`) never
 *          divide by zero on x86_64.
 * @param[in] field_ty The EDG type of the struct, class, or union field.
 * @param[out] out_align Receives the effective non-zero alignment in bytes (`>= 1`).
 * @return llvm_gen_be_error_t::ok on success, `invalid_argument` if `field_ty` or `out_align` is null,
 *         or `out_of_memory` on allocation budget exhaustion.
 */
llvm_gen_be_error_t resolve_sysv_field_alignment(
    a_type_ptr field_ty,
    uint64_t* out_align) noexcept;

/**
 * @brief Determines whether an EDG type represents System V AMD64 `COMPLEX_X87` (`_Complex long double` / `_Complex __float80`).
 * @details Recognizes both unlowered `tk_complex` nodes and IL-lowered `_Complex` representations
 *          (`_Complex_long_double` and `_Complex_float80` structs synthesized by `lower_c99.c`), while
 *          preserving `MEMORY` (`sret`) classification for user-defined `struct { long double a[2]; }`
 *          aggregates and Android `fp128` `_Complex long double` per System V psABI §3.2.3.
 * @param[in] ty The EDG type to inspect.
 * @param[out] out_is_complex_x87 Receives `true` if `ty` has `COMPLEX_X87` ABI class, `false` otherwise.
 * @return llvm_gen_be_error_t::ok on success, `invalid_argument` if `ty` or `out_is_complex_x87` is null,
 *         or `out_of_memory` on allocation budget exhaustion.
 */
llvm_gen_be_error_t is_sysv_complex_x87_type(
    a_type_ptr ty,
    bool* out_is_complex_x87) noexcept;

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
 * @brief Constructs the LLVM register-coerced type for a System V AMD64 aggregate or scalar.
 * @details Maps 1-eightbyte and 2-eightbyte INTEGER, SSE, SSEUP, X87, and COMPLEX_X87
 *          classifications into their psABI-mandated LLVM IR representation
 *          (e.g., `i64`, `double`, `{i64, double}`, `fp128`, `x86_fp80`, `{x86_fp80, x86_fp80}`).
 * @param[in] ty The EDG type being coerced.
 * @param[in] info Pointer to the computed System V classification info.
 * @param[out] out_coerced_ty Receives the coerced `llvm::Type*`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_sysv_coerced_type(
    a_type_ptr ty,
    const x86_64_abi_arg_info_t* info,
    llvm::Type** out_coerced_ty) noexcept;

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
 * @brief Constructs the LLVM Type for a va_list under System V AMD64 ABI.
 * @details The va_list type is a struct [1 x { i32, i32, i8*, i8* }] representing 
 * gp_offset, fp_offset, overflow_arg_area, and reg_save_area.
 * @param[out] out_ty Pointer to store the resulting LLVM ArrayType.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_sysv_va_list_type(
    llvm::Type** out_ty) noexcept;

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

/**
 * @enum llvm_abi_passing_kind_t
 * @brief Target-agnostic ABI argument/return passing mechanism classification.
 * @details Summarizes how a return value or parameter is lowered in an LLVM FunctionType.
 */
enum class llvm_abi_passing_kind_t {
  ignore_void,      ///< Void or zero-sized empty type omitted from register/stack passing.
  direct,           ///< Passed directly as its natural LLVM scalar, pointer, or vector type.
  coerced_register, ///< Coerced into one or more target integer/SSE/HFA register types.
  indirect_sret,    ///< Returned indirectly via a hidden caller-allocated `sret(T)` pointer parameter.
  indirect_byval,   ///< Passed on the stack via a `byval(T)` pointer parameter.
  indirect_pointer  ///< Passed by pointer to caller-allocated temporary memory.
};

/**
 * @struct llvm_target_abi_reg_state_t
 * @brief Tracks cumulative general-purpose and floating-point/vector register usage across a call or function signature.
 * @details Used across target ABI classifiers and call/prologue lowering to determine when
 *          register arguments overflow into memory (`byval` or stack slots).
 */
struct llvm_target_abi_reg_state_t {
  uint32_t gpr_used; ///< Number of general-purpose argument registers consumed so far.
  uint32_t fpr_used; ///< Number of floating-point or vector argument registers consumed so far.
  uint32_t gpr_max;  ///< Maximum available general-purpose argument registers on the target ABI.
  uint32_t fpr_max;  ///< Maximum available floating-point or vector argument registers on the target ABI.
};

/**
 * @struct llvm_abi_function_info_t
 * @brief Target-agnostic summary of a lowered function's ABI signature and attributes.
 * @details Holds the lowered `llvm::FunctionType*`, `llvm::AttributeList`, return passing
 *          kind, and hidden `sret` flag computed by the target ABI dispatcher.
 */
struct llvm_abi_function_info_t {
  llvm::FunctionType* fn_type;        ///< Lowered target ABI LLVM FunctionType.
  llvm::AttributeList attrs;          ///< Parameter, return, and function ABI attributes.
  llvm_abi_passing_kind_t return_kind; ///< Classification of the function return value.
  bool has_sret;                      ///< True if parameter 0 carries `StructRet`.
};

/**
 * @brief Dispatches to the active target architecture's ABI classifier to compute lowered function ABI metadata.
 * @details Supports System V x86_64, Windows x64, AArch64 AAPCS64, x86-32, ARM32,
 *          RISC-V 32/64, LoongArch64, MIPS, PowerPC 32/64, SystemZ (s390x), SPARC, and WebAssembly.
 * @param[in] routine_ty Pointer to the EDG `tk_routine` type descriptor.
 * @param[in] triple_str Target triple string identifying architecture and OS ABI.
 * @param[out] out_abi_info Receives the populated `llvm_abi_function_info_t` descriptor.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_compute_function_abi_info(
    a_type_ptr routine_ty,
    const std::string& triple_str,
    llvm_abi_function_info_t* out_abi_info) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_ABI_SYSV_X86_64_H */
