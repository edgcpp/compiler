/**
 * @file llvm_gen_be_abi_arm32.h
 * @brief ARM 32-bit ABI classification declarations.
 */
#ifndef LLVM_GEN_BE_ABI_ARM32_H
#define LLVM_GEN_BE_ABI_ARM32_H

#include "basic_hdrs.h"
#include "il_def.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Function.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @struct arm32_arg_info_t
 * @brief Represents the classification and layout of a single argument or return value.
 */
struct arm32_arg_info_t {
  uint64_t size;           ///< Size of the argument in bytes.
  bool is_indirect;        ///< True if the argument must be passed indirectly.
  llvm::Type* coerced_type; ///< The LLVM type to coerce the argument to (e.g. array of i32).
  bool is_hfa;             ///< True if the argument is a Homogeneous Floating-point Aggregate.
  uint32_t align;          ///< Alignment requirement for the argument in bytes.
};

/**
 * @brief Classifies a type according to the ARM 32-bit AAPCS parameter passing rules.
 * @param[in] ty The EDG type pointer to classify.
 * @param[out] out_info Pointer to the arm32_arg_info_t struct to populate with classification results.
 * @return An llvm_gen_be_error_t code indicating success or failure.
 */
llvm_gen_be_error_t classify_arm32_argument(a_type_ptr ty, arm32_arg_info_t* out_info) noexcept;

/**
 * @brief Builds an LLVM function type and attributes according to ARM 32-bit AAPCS.
 * @param[in] routine_ty The EDG routine type to lower.
 * @param[out] out_func_ty Pointer to output LLVM function type.
 * @param[out] out_attrs Pointer to output LLVM attribute list.
 * @return An llvm_gen_be_error_t code indicating success or failure.
 */
llvm_gen_be_error_t build_arm32_function_type(a_type_ptr routine_ty, llvm::FunctionType** out_func_ty, llvm::AttributeList* out_attrs) noexcept;

END_EDG_NAMESPACE
#endif

#endif
