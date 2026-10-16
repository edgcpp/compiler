/**
 * @file llvm_gen_be_abi_x86_32.h
 * @brief x86 32-bit ABI classification declarations.
 */
#ifndef LLVM_GEN_BE_ABI_X86_32_H
#define LLVM_GEN_BE_ABI_X86_32_H

#include "basic_hdrs.h"
#include "il_def.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Function.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

struct x86_32_arg_info_t {
  uint64_t size;
  bool is_indirect;
  bool in_reg;
  llvm::Type* coerced_type;
};

llvm_gen_be_error_t classify_x86_32_argument(a_type_ptr ty, x86_32_arg_info_t* out_info) noexcept;
llvm_gen_be_error_t build_x86_32_function_type(a_type_ptr routine_ty, llvm::FunctionType** out_func_ty, llvm::AttributeList* out_attrs) noexcept;

END_EDG_NAMESPACE
#endif

#endif
