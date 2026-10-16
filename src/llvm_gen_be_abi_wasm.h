/**
 * @file llvm_gen_be_abi_wasm.h
 * @brief WebAssembly ABI classification declarations.
 */
#ifndef LLVM_GEN_BE_ABI_WASM_H
#define LLVM_GEN_BE_ABI_WASM_H

#include "basic_hdrs.h"
#include "il_def.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Function.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

struct wasm_arg_info_t {
  uint64_t size;
  bool is_indirect;
};

llvm_gen_be_error_t classify_wasm_argument(a_type_ptr ty, wasm_arg_info_t* out_info) noexcept;
llvm_gen_be_error_t build_wasm_function_type(a_type_ptr routine_ty, llvm::FunctionType** out_func_ty, llvm::AttributeList* out_attrs) noexcept;

END_EDG_NAMESPACE
#endif

#endif
