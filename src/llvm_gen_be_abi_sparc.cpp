/**
 * @file llvm_gen_be_abi_sparc.cpp
 * @brief SPARC / SPARC V9 ABI classification subsystem.
 * @details Implements the parameter passing and return value classification
 * rules for SPARC / SPARC V9.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_abi_sparc.h"
#include "llvm_gen_be_internal.h"
#include <llvm/IR/Intrinsics.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

llvm_gen_be_error_t classify_sparc_argument(
    a_type_ptr ty,
    bool is_v9,
    sparc_arg_info_t* out_info) noexcept {
  
  if (!ty || !out_info) return llvm_gen_be_error_t::invalid_argument;

  out_info->abi_class = sparc_abi_class_t::no_class;
  out_info->is_indirect = false;
  out_info->requires_sret = false;
  out_info->coerced_type = nullptr;

  while (ty->kind == tk_typeref) ty = ty->variant.typeref.type;

  if (ty->size == 0) {
    out_info->abi_class = sparc_abi_class_t::no_class;
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_integer || ty->kind == tk_enum || ty->kind == tk_pointer) {
    out_info->abi_class = sparc_abi_class_t::integer;
    return llvm_type_from_edg_type(ty, &out_info->coerced_type);
  }

  if (ty->kind == tk_float) {
    out_info->abi_class = sparc_abi_class_t::float_class;
    return llvm_type_from_edg_type(ty, &out_info->coerced_type);
  }

  if (is_v9) {
    if (ty->size > 32) {
      out_info->abi_class = sparc_abi_class_t::memory;
      out_info->is_indirect = true;
    } else {
      out_info->abi_class = sparc_abi_class_t::struct_coerced;
      out_info->coerced_type = llvm::ArrayType::get(llvm::Type::getInt64Ty(*be_state->context), (ty->size + 7) / 8);
    }
  } else {
    out_info->abi_class = sparc_abi_class_t::struct_byval;
  }

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t compute_sparc_return_info(
    a_type_ptr ret_ty,
    bool is_v9,
    sparc_arg_info_t* out_info,
    bool* out_sret) noexcept {
  
  if (!ret_ty || !out_info || !out_sret) return llvm_gen_be_error_t::invalid_argument;

  *out_sret = false;
  llvm_gen_be_error_t err = classify_sparc_argument(ret_ty, is_v9, out_info);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (out_info->abi_class == sparc_abi_class_t::struct_byval || out_info->abi_class == sparc_abi_class_t::memory) {
    out_info->requires_sret = true;
    *out_sret = true;
  } else if (is_v9 && out_info->abi_class == sparc_abi_class_t::struct_coerced && ret_ty->size > 32) {
    out_info->requires_sret = true;
    *out_sret = true;
  }

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t build_sparc_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {
    
  if (!routine_ty || !out_fn_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;

  std::vector<llvm::Type*> llvm_param_types;
  std::vector<llvm::AttributeSet> param_attrs;
  llvm::AttrBuilder ret_attr(*be_state->context);

  bool is_v9 = targ_sizeof_pointer == 8;

  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  llvm::Type* llvm_ret_ty = nullptr;

  sparc_arg_info_t ret_info;
  bool is_sret = false;
  llvm_gen_be_error_t err = compute_sparc_return_info(ret_ty, is_v9, &ret_info, &is_sret);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (is_sret) {
    llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
    llvm::Type* sret_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm_param_types.push_back(sret_ptr_ty);
    
    llvm::AttrBuilder sret_ab(*be_state->context);
    llvm::Type* sret_pointee_ty = nullptr;
    err = llvm_type_from_edg_type(ret_ty, &sret_pointee_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    sret_ab.addStructRetAttr(sret_pointee_ty);
    if (!is_v9) {
      sret_ab.addAttribute(llvm::Attribute::InReg); // SPARC V8 unimp protocol uses inreg for sret
    }
    param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  } else {
    if (ret_info.abi_class == sparc_abi_class_t::no_class) {
      llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
    } else if (ret_info.coerced_type) {
      llvm_ret_ty = ret_info.coerced_type;
    } else {
      err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }

  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
      
      sparc_arg_info_t arg_info;
      err = classify_sparc_argument(param->type, is_v9, &arg_info);
      if (err != llvm_gen_be_error_t::ok) return err;

      if (arg_info.abi_class == sparc_abi_class_t::no_class) continue;

      llvm::AttrBuilder ab(*be_state->context);
      if (arg_info.abi_class == sparc_abi_class_t::struct_byval || arg_info.is_indirect) {
        llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
        llvm::Type* pointee_ty = nullptr;
        err = llvm_type_from_edg_type(param->type, &pointee_ty);
        if (err != llvm_gen_be_error_t::ok) return err;
        ab.addByValAttr(pointee_ty);
      } else if (arg_info.coerced_type) {
        llvm_param_types.push_back(arg_info.coerced_type);
      } else {
        llvm::Type* pty = nullptr;
        err = llvm_type_from_edg_type(param->type, &pty);
        if (err != llvm_gen_be_error_t::ok) return err;
        llvm_param_types.push_back(pty);
      }
      
      param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
    }
  }

  bool is_vararg = (routine_ty->variant.routine.extra_info != nullptr)
                       ? routine_ty->variant.routine.extra_info->has_ellipsis
                       : false;

  *out_fn_ty = llvm::FunctionType::get(llvm_ret_ty, llvm_param_types, is_vararg);
  *out_attrs = llvm::AttributeList::get(*be_state->context, 
                                        llvm::AttributeSet(), 
                                        llvm::AttributeSet::get(*be_state->context, ret_attr), 
                                        param_attrs);

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t lower_sparc_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
    
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_start_func = llvm::Intrinsic::getOrInsertDeclaration(
      be_state->module.get(), llvm::Intrinsic::vastart, {llvm::PointerType::getUnqual(*be_state->context)});
  
  *out_val = be_state->builder->CreateCall(va_start_func, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t lower_sparc_va_arg(
    llvm::Value* va_list_ptr,
    a_type_ptr ty,
    llvm::Value** out_val) noexcept {
  
  if (!va_list_ptr || !ty || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Type* llvm_ty = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(ty, &llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;
  *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t lower_sparc_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
  
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_end_func = llvm::Intrinsic::getOrInsertDeclaration(
      be_state->module.get(), llvm::Intrinsic::vaend, {llvm::PointerType::getUnqual(*be_state->context)});
  
  *out_val = be_state->builder->CreateCall(va_end_func, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t lower_sparc_va_copy(
    llvm::Value* dest_va_list_ptr,
    llvm::Value* src_va_list_ptr,
    llvm::Value** out_val) noexcept {
  
  if (!dest_va_list_ptr || !src_va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_copy_func = llvm::Intrinsic::getOrInsertDeclaration(
      be_state->module.get(), llvm::Intrinsic::vacopy, {llvm::PointerType::getUnqual(*be_state->context)});
  
  *out_val = be_state->builder->CreateCall(va_copy_func, {dest_va_list_ptr, src_va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
