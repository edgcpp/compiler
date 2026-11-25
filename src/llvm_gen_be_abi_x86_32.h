/**
 * @file llvm_gen_be_abi_x86_32.h
 * @ingroup llvm_gen_be_abi
 * @brief x86 32-bit ABI classification declarations.
 * @details Declares parameter and return value classification structures and functions
 *          for 32-bit x86 (`cdecl`, `stdcall`, `fastcall`, `thiscall`, `regparm`) conventions.
 */
#ifndef LLVM_GEN_BE_ABI_X86_32_H
#define LLVM_GEN_BE_ABI_X86_32_H

#include "basic_hdrs.h"
#include "il_def.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Function.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @struct x86_32_arg_info_t
 * @brief Classification descriptor for a 32-bit x86 parameter or return value.
 * @details Stores argument size, indirect flag, `inreg` promotion flag, and coerced LLVM type.
 */
struct x86_32_arg_info_t {
  uint64_t size;            ///< Size of the argument in bytes.
  bool is_indirect;         ///< True if passed or returned indirectly via stack/pointer.
  bool in_reg;              ///< True if promoted to `ECX`/`EDX` via `inreg`.
  llvm::Type* coerced_type; ///< Coerced LLVM type when packed into registers.
};

/**
 * @brief Classifies an argument or return type according to x86 32-bit ABI rules.
 * @details Inspects the type and determines register allocation or stack passing requirements.
 * @param[in] ty The EDG AST type pointer to classify.
 * @param[out] out_info Pointer to the x86_32_arg_info_t structure receiving classification results.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
llvm_gen_be_error_t classify_x86_32_argument(a_type_ptr ty, x86_32_arg_info_t* out_info) noexcept;

/**
 * @brief Builds the LLVM FunctionType and attributes for an x86 32-bit function.
 * @details Computes argument and return coercions and adds struct_ret attributes when indirect.
 * @param[in] routine_ty The EDG routine AST type pointer.
 * @param[out] out_func_ty Receives the constructed LLVM FunctionType pointer on success.
 * @param[out] out_attrs Receives the constructed LLVM AttributeList on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_x86_32_function_type(a_type_ptr routine_ty, llvm::FunctionType** out_func_ty, llvm::AttributeList* out_attrs) noexcept;

/**
 * @brief Lowers a va_start intrinsic for the x86 32-bit ABI.
 * @details Emits `llvm.va_start` on the provided `va_list` pointer.
 * @param[in] va_list_ptr Pointer to the `va_list` object.
 * @param[out] out_val Receives the resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_x86_32_va_start(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_arg operation for the x86 32-bit ABI.
 * @details Emits an LLVM `va_arg` instruction for the requested EDG type.
 * @param[in] va_list_ptr Pointer to the `va_list` object.
 * @param[in] ty The EDG type being extracted.
 * @param[out] out_val Receives the extracted LLVM value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_x86_32_va_arg(llvm::Value* va_list_ptr, a_type_ptr ty, llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_end intrinsic for the x86 32-bit ABI.
 * @details Emits `llvm.va_end` on the provided `va_list` pointer.
 * @param[in] va_list_ptr Pointer to the `va_list` object.
 * @param[out] out_val Receives the resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_x86_32_va_end(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a va_copy intrinsic for the x86 32-bit ABI.
 * @details Emits `llvm.va_copy` from `src_va_list_ptr` to `dest_va_list_ptr`.
 * @param[in] dest_va_list_ptr Pointer to the destination `va_list` object.
 * @param[in] src_va_list_ptr Pointer to the source `va_list` object.
 * @param[out] out_val Receives the resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_x86_32_va_copy(llvm::Value* dest_va_list_ptr, llvm::Value* src_va_list_ptr, llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE
#endif

#endif
