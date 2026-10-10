/**
 * @file llvm_gen_be_abi_riscv64.cpp
 * @brief RISC-V 64-bit ABI lowering implementation.
 * @details Translates EDG types to LLVM IR function types matching RISC-V LP64D.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_abi_riscv64.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

llvm_gen_be_error_t build_riscv64_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {

  if (!routine_ty || !out_fn_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;

  std::vector<llvm::Type*> llvm_param_types;
  std::vector<llvm::AttributeSet> param_attrs;
  llvm::AttrBuilder ret_attr(*be_state->context);
  
  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  llvm::Type* llvm_ret_ty = nullptr;
  
  // Very simplified: always use sret for structs
  if (is_class_struct_union_type(ret_ty)) {
    llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
    llvm::Type* sret_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm_param_types.push_back(sret_ptr_ty);
    
    llvm::AttrBuilder sret_ab(*be_state->context);
    llvm::Type* sret_pointee_ty = nullptr;
    llvm_gen_be_error_t err = llvm_type_from_edg_type(ret_ty, &sret_pointee_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    sret_ab.addStructRetAttr(sret_pointee_ty);
    param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  } else {
    llvm_gen_be_error_t err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
  }

  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
         
         llvm::Type* pty = nullptr;
         llvm_gen_be_error_t err = llvm_type_from_edg_type(param->type, &pty);
         if (err != llvm_gen_be_error_t::ok) return err;
         
         llvm::AttrBuilder ab(*be_state->context);
         if (is_class_struct_union_type(param->type)) {
             llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
             ab.addByValAttr(pty);
         } else {
             llvm_param_types.push_back(pty);
         }
         param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
    }
  }

  *out_fn_ty = llvm::FunctionType::get(llvm_ret_ty, llvm_param_types, routine_ty->variant.routine.has_ellipsis);
  *out_attrs = llvm::AttributeList::get(*be_state->context, llvm::AttributeSet::get(*be_state->context, ret_attr),
                                        llvm::AttributeSet::get(*be_state->context), param_attrs);
  
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
