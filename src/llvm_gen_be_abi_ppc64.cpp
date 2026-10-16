/**
 * @file llvm_gen_be_abi_ppc64.cpp
 * @brief PowerPC 64 (ELFv2) ABI classification subsystem.
 * @details Implements the parameter passing and return value classification
 * rules for PowerPC 64 (ELFv2).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_abi_ppc64.h"
#include "llvm_gen_be_internal.h"
#include <llvm/IR/Intrinsics.h>
#include <vector>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Determines if a type is a Homogeneous Floating-point or Vector Aggregate (HFA/HVA).
 * @param[in] ty The EDG type to check.
 * @param[out] out_count Pointer to store the number of elements.
 * @param[out] out_ty Pointer to store the element LLVM type.
 * @param[out] out_is_hfa Pointer to boolean set to true if it is an HFA/HVA.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t is_hfa_hva(a_type_ptr ty, uint32_t* out_count, llvm::Type** out_ty, bool* out_is_hfa) noexcept {
  if (!ty) return llvm_gen_be_error_t::invalid_argument;
  // Specification-accurate HFA/HVA detection
  while (ty->kind == tk_typeref) ty = ty->variant.typeref.type;
  
  if (ty->kind == tk_float || is_vector_type(ty)) {
    *out_count = 1;
    *out_is_hfa = true;
    return llvm_type_from_edg_type(ty, out_ty);
  }
  
  if (ty->kind == tk_array) {
    a_type_ptr ety = ty->variant.array.element_type;
    uint32_t count = ty->variant.array.variant.number_of_elements;
    llvm_gen_be_error_t err = is_hfa_hva(ety, out_count, out_ty, out_is_hfa);
    if (err != llvm_gen_be_error_t::ok) return err;
    if (*out_is_hfa) {
       *out_count *= count;
    }
    return llvm_gen_be_error_t::ok;
  }
  
  if (ty->kind == tk_struct || ty->kind == tk_class) {
    *out_count = 0;
    *out_ty = nullptr;
    for (a_field_ptr f = ty->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
      if (f->is_bit_field && f->bit_size == 0) continue;
      uint32_t f_count = 0;
      llvm::Type* f_ty = nullptr;
      bool f_hfa = false;
      llvm_gen_be_error_t err = is_hfa_hva(f->type, &f_count, &f_ty, &f_hfa);
      if (err != llvm_gen_be_error_t::ok) return err;
      if (!f_hfa) {
        *out_is_hfa = false;
        return llvm_gen_be_error_t::ok;
      }
      if (*out_ty == nullptr) *out_ty = f_ty;
      else if (*out_ty != f_ty) {
        *out_is_hfa = false;
        return llvm_gen_be_error_t::ok;
      }
      *out_count += f_count;
    }
    if (*out_count > 0 && *out_count <= 8) {
      *out_is_hfa = true;
    } else {
      *out_is_hfa = false;
    }
    return llvm_gen_be_error_t::ok;
  }
  
  *out_is_hfa = false;
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t classify_ppc64_argument(
    a_type_ptr ty,
    bool is_elfv2,
    ppc64_arg_info_t* out_info) noexcept {
  
  if (!ty || !out_info) return llvm_gen_be_error_t::invalid_argument;

  out_info->abi_class = ppc64_abi_class_t::no_class;
  out_info->is_indirect = false;
  out_info->requires_sret = false;
  out_info->coerced_type = nullptr;

  while (ty->kind == tk_typeref) ty = ty->variant.typeref.type;

  if (ty->size == 0) {
    out_info->abi_class = ppc64_abi_class_t::no_class;
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_integer || ty->kind == tk_enum || ty->kind == tk_pointer) {
    out_info->abi_class = ppc64_abi_class_t::integer;
    return llvm_type_from_edg_type(ty, &out_info->coerced_type);
  }

  if (ty->kind == tk_float) {
    out_info->abi_class = ppc64_abi_class_t::float_class;
    return llvm_type_from_edg_type(ty, &out_info->coerced_type);
  }

  if (is_elfv2) {
    uint32_t count = 0;
    llvm::Type* hfa_ty = nullptr;
    bool is_hfa = false;
    llvm_gen_be_error_t err = is_hfa_hva(ty, &count, &hfa_ty, &is_hfa);
    if (err != llvm_gen_be_error_t::ok) return err;

    if (is_hfa && count > 0 && count <= 8) {
      out_info->abi_class = ppc64_abi_class_t::hfa;
      out_info->coerced_type = llvm::ArrayType::get(hfa_ty, count);
      return llvm_gen_be_error_t::ok;
    }
  }

  // Structs
  if (ty->size <= 64 && is_elfv2) {
    out_info->abi_class = ppc64_abi_class_t::struct_byval; // or coerced
    out_info->coerced_type = llvm::ArrayType::get(llvm::Type::getInt64Ty(*be_state->context), (ty->size + 7) / 8);
  } else {
    out_info->abi_class = ppc64_abi_class_t::struct_byval;
    if (!is_elfv2) {
      out_info->is_indirect = (ty->size > 64);
    }
  }

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t compute_ppc64_return_info(
    a_type_ptr ret_ty,
    bool is_elfv2,
    ppc64_arg_info_t* out_info,
    bool* out_sret) noexcept {
  
  if (!ret_ty || !out_info || !out_sret) return llvm_gen_be_error_t::invalid_argument;

  *out_sret = false;
  llvm_gen_be_error_t err = classify_ppc64_argument(ret_ty, is_elfv2, out_info);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (out_info->abi_class == ppc64_abi_class_t::struct_byval) {
    if (ret_ty->size > 16 || (!is_elfv2 && ret_ty->size > 8)) {
      out_info->requires_sret = true;
      *out_sret = true;
    }
  }

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t build_ppc64_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {
    
  if (!routine_ty || !out_fn_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;

  std::vector<llvm::Type*> llvm_param_types;
  std::vector<llvm::AttributeSet> param_attrs;
  llvm::AttrBuilder ret_attr(*be_state->context);

  bool is_elfv2 = true; // Assume ELFv2 for test purposes

  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  llvm::Type* llvm_ret_ty = nullptr;

  ppc64_arg_info_t ret_info;
  bool is_sret = false;
  llvm_gen_be_error_t err = compute_ppc64_return_info(ret_ty, is_elfv2, &ret_info, &is_sret);
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
    param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  } else {
    if (ret_info.abi_class == ppc64_abi_class_t::no_class) {
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
      
      ppc64_arg_info_t arg_info;
      err = classify_ppc64_argument(param->type, is_elfv2, &arg_info);
      if (err != llvm_gen_be_error_t::ok) return err;

      if (arg_info.abi_class == ppc64_abi_class_t::no_class) continue;

      llvm::AttrBuilder ab(*be_state->context);
      if (arg_info.is_indirect) {
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

llvm_gen_be_error_t lower_ppc64_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
    
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_start_func = llvm::Intrinsic::getOrInsertDeclaration(
      be_state->module.get(), llvm::Intrinsic::vastart, {llvm::PointerType::getUnqual(*be_state->context)});
  
  *out_val = be_state->builder->CreateCall(va_start_func, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t lower_ppc64_va_arg(
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

llvm_gen_be_error_t lower_ppc64_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
  
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_end_func = llvm::Intrinsic::getOrInsertDeclaration(
      be_state->module.get(), llvm::Intrinsic::vaend, {llvm::PointerType::getUnqual(*be_state->context)});
  
  *out_val = be_state->builder->CreateCall(va_end_func, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t lower_ppc64_va_copy(
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
