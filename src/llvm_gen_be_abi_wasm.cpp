/**
 * @file llvm_gen_be_abi_wasm.cpp
 * @brief WebAssembly ABI classification implementation.
 * @details Implements parameter passing and function type construction for WebAssembly (wasm32/wasm64).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_abi_wasm.h"
#include "llvm_gen_be_internal.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Classifies an argument or return type according to WebAssembly ABI rules.
 * @details Direct scalar mapping or indirect pointer passing for composite aggregates.
 * @param[in] ty The EDG AST type pointer to classify.
 * @param[out] out_info Pointer to the wasm_arg_info_t structure receiving classification results.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
llvm_gen_be_error_t classify_wasm_argument(a_type_ptr ty, wasm_arg_info_t* out_info) noexcept {
  if (!ty || !out_info) return llvm_gen_be_error_t::invalid_argument;
  ty = skip_typerefs(ty);
  
  out_info->size = ty->size;
  out_info->is_indirect = false;

  if (ty->kind == tk_class || ty->kind == tk_struct || ty->kind == tk_union) {
      out_info->is_indirect = true;
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Builds the LLVM FunctionType and attributes for a WebAssembly function.
 * @details Computes argument and return types and sets struct_ret for indirect composites.
 * @param[in] routine_ty The EDG routine AST type pointer.
 * @param[out] out_func_ty Receives the constructed LLVM FunctionType pointer on success.
 * @param[out] out_attrs Receives the constructed LLVM AttributeList on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_wasm_function_type(a_type_ptr routine_ty, llvm::FunctionType** out_func_ty, llvm::AttributeList* out_attrs) noexcept {
  if (!routine_ty || !out_func_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;
  
  a_routine_type_supplement_ptr supp = routine_ty->variant.routine.extra_info;
  
  llvm::Type* ret_llvm_ty = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(routine_ty->variant.routine.return_type, &ret_llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  wasm_arg_info_t ret_info;
  err = classify_wasm_argument(routine_ty->variant.routine.return_type, &ret_info);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  llvm::Type* actual_ret_ty = ret_llvm_ty;
  llvm::AttrBuilder ret_attrs(*be_state->context);
  
  std::vector<llvm::Type*> arg_types;
  std::vector<llvm::AttributeSet> param_attrs;
  if (ret_info.is_indirect) {
      actual_ret_ty = llvm::Type::getVoidTy(*be_state->context);
      arg_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
      llvm::AttrBuilder sret_ab(*be_state->context);
      sret_ab.addStructRetAttr(ret_llvm_ty);
      param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  }
  
  if (supp) {
    for (a_param_type_ptr p = supp->param_type_list; p; p = p->next) {
        if (p->type->kind == tk_void) break;
        llvm::Type* p_ty = nullptr;
        err = get_llvm_type(p->type, &p_ty);
        if (err != llvm_gen_be_error_t::ok) return err;
        
        wasm_arg_info_t p_info;
        err = classify_wasm_argument(p->type, &p_info);
        if (err != llvm_gen_be_error_t::ok) return err;
        
        if (p_info.is_indirect) {
            arg_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
            llvm::AttrBuilder ab(*be_state->context);
            ab.addByValAttr(p_ty);
            param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
        } else {
            arg_types.push_back(p_ty);
            param_attrs.push_back(llvm::AttributeSet());
        }
    }
  }
  
  llvm::AttributeSet ret_set = llvm::AttributeSet::get(*be_state->context, ret_attrs);
  llvm::AttributeSet fn_set;
  *out_func_ty = llvm::FunctionType::get(actual_ret_ty, arg_types, supp ? supp->has_ellipsis : false);
  *out_attrs = llvm::AttributeList::get(*be_state->context, fn_set, ret_set, param_attrs);
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
