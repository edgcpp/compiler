/**
 * @file llvm_gen_be_abi_win64.cpp
 * @brief Microsoft Windows x64 ABI classification and lowering subsystem.
 * @details Implements the parameter passing and return value classification
 * rules specified in the Microsoft x64 calling convention.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_abi_win64.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_type.h"
#include "target.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief classify_win64_argument
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
llvm_gen_be_error_t classify_win64_argument(a_type_ptr ty, win64_arg_info_t* out_info) noexcept {
  if (ty == nullptr || out_info == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  // Resolve type references
  while (ty->kind == tk_typeref) {
    ty = ty->variant.typeref.type;
  }

  uint64_t size = ty->size;
  out_info->size = size;
  out_info->is_indirect = false;

  // Basic types
  switch (ty->kind) {
    case tk_integer:
    case tk_pointer:
    case tk_ptr_to_member:
      out_info->abi_class = win64_abi_class_t::direct_integer;
      return llvm_gen_be_error_t::ok;

    case tk_float:
      // In MSVC, long double is 64-bit (same as double) and passed as float.
      // 32-bit and 64-bit floating point values.
      out_info->abi_class = win64_abi_class_t::direct_float;
      return llvm_gen_be_error_t::ok;

    case tk_class:
    case tk_struct:
    case tk_union:
    case tk_complex:
      break; // Handled below

    case tk_array:
    case tk_routine:
      out_info->abi_class = win64_abi_class_t::indirect_by_pointer;
      out_info->is_indirect = true;
      return llvm_gen_be_error_t::ok;

    case tk_void:
      out_info->size = 0;
      return llvm_gen_be_error_t::ok;

    default:
      return llvm_gen_be_error_t::unsupported_type;
  }

  // Composites (struct, union, class, complex)
  // Windows x64 passes structs by value if they are exactly 1, 2, 4, or 8 bytes.
  // Otherwise, or if they are non-trivial, they are passed by pointer.
  if ((ty->kind == tk_class || ty->kind == tk_struct || ty->kind == tk_union) && !is_trivially_copyable_type(ty)) {
    out_info->abi_class = win64_abi_class_t::indirect_by_pointer;
    out_info->is_indirect = true;
    return llvm_gen_be_error_t::ok;
  }

  if (size == 1 || size == 2 || size == 4 || size == 8) {
    out_info->abi_class = win64_abi_class_t::direct_integer;
    return llvm_gen_be_error_t::ok;
  }

  out_info->abi_class = win64_abi/**
 * @brief compute_win64_return_info
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
_class_t::indirect_by_pointer;
  out_info->is_indirect = true;
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t compute_win64_return_info(a_type_ptr ret_ty, win64_arg_info_t* out_info, bool* out_sret) noexcept {
  if (ret_ty == nullptr || out_info == nullptr || out_sret == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  *out_sret = false;

  llvm_gen_be_error_t err/**
 * @brief build_win64_function_type
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
 = classify_win64_argument(ret_ty, out_info);
  if (err != llvm_gen_be_error_t::ok) {
    return err;
  }

  if (out_info->is_indirect) {
    *out_sret = true;
  }

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t build_win64_function_type(a_type_ptr routine_ty, llvm::FunctionType** out_fn_ty, llvm::AttributeList* out_attrs) noexcept {
  if (routine_ty == nullptr || out_fn_ty == nullptr || out_attrs == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  std::vector<llvm::Type*> llvm_param_types;
  std::vector<llvm::AttributeSet> param_attrs;
  llvm::AttrBuilder ret_attr(*be_state->context);

  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  
  win64_arg_info_t ret_info;
  bool sret = false;
  llvm_gen_be_error_t err = compute_win64_return_info(ret_ty, &ret_info, &sret);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Type* llvm_ret_ty = nullptr;

  if (sret) {
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
    err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
  }

  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
      
      win64_arg_info_t arg_info;
      err = classify_win64_argument(param->type, &arg_info);
      if (err != llvm_gen_be_error_t::ok) return err;

      llvm::Type* pty = nullptr;
      err = llvm_type_from_edg_type(param->type, &pty);
      if (err != llvm_gen_be_error_t::ok) return err;

      llvm::AttrBuilder ab(*be_state->context);

      if (arg_info.is_indirect) {
        // Pass by pointer
        llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
        llvm_param_types.push_back(ptr_ty);
      } else {
        // Direct integer or float
        if (arg_info.abi_class == win64_abi_class_t::direct_integer) {
          if (arg_info.size == 1) pty = llvm::Type::getInt8Ty(*be_state->context);
          else if (arg_info.size == 2) pty = llvm::Type::getInt16Ty(*be_state->context);
          else if (arg_info.size == 4) pty = llvm::Type::getInt32Ty(*be_state->context);
          else if (arg_info.size == 8) pty = llvm::Type::getInt64Ty(*be_state->context);
        }
        llvm_param_types.push_back(pty);
      }

      param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
    }
  }

  bool is_var_arg = routine_ty->variant.routine.extra_info 
                       ? routine_ty->variant.routine.extra_info->has_ellipsis 
                       : false;
  *out_fn_ty = llvm/**
 * @brief lower_win64_va_start
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
::FunctionType::get(llvm_ret_ty, llvm_param_types, is_var_arg);

  llvm::AttributeSet ret_attr_set = llvm::AttributeSet::get(*be_state->context, ret_attr);
  *out_attrs = llvm::AttributeList::get(*be_state->context, llvm::AttributeSet(), ret_attr_set, param_attrs);

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t lower_win64_va_start(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept {
  if (va_list_ptr == nullptr || out_val == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  // To be implemented: lower va_start to char* based Win64 va_list increment and alignment.
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif // BACK_END_IS_LLVM_GEN_BE
