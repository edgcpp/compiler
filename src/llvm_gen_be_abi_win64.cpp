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
#include <llvm/IR/Intrinsics.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Classifies a single EDG type into Win64 ABI class.
 * @param[in] ty The EDG type to classify.
 * @param[in] is_vectorcall True if the function uses __vectorcall convention.
 * @param[out] out_info The classification result information structure.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_win64_argument(a_type_ptr ty, bool is_vectorcall, win64_arg_info_t* out_info) noexcept {
  if (ty == nullptr || out_info == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  while (ty->kind == tk_typeref) {
    ty = ty->variant.typeref.type;
  }

  uint64_t size = ty->size;
  out_info->size = size;
  out_info->is_indirect = false;
  out_info->pass_by_invisible_reference = false;
  out_info->hva_elem_ty = nullptr;
  out_info->num_elements = 1;

  switch (ty->kind) {
    case tk_integer:
      if (size > 8) {
        out_info->abi_class = win64_abi_class_t::indirect_by_pointer;
        out_info->is_indirect = true;
        return llvm_gen_be_error_t::ok;
      }
      out_info->abi_class = win64_abi_class_t::direct_integer;
      return llvm_gen_be_error_t::ok;

    case tk_pointer:
    case tk_ptr_to_member:
      out_info->abi_class = win64_abi_class_t::direct_integer;
      return llvm_gen_be_error_t::ok;

    case tk_float:
      if (is_vectorcall) {
        out_info->abi_class = win64_abi_class_t::vectorcall_hva;
        llvm_gen_be_error_t err = llvm_type_from_edg_type(ty, &out_info->hva_elem_ty);
        if (err != llvm_gen_be_error_t::ok) return err;
        return llvm_gen_be_error_t::ok;
      }
      out_info->abi_class = win64_abi_class_t::direct_float;
      return llvm_gen_be_error_t::ok;

    case tk_class:
    case tk_struct:
    case tk_union:
    case tk_array:
      // Microsoft x64 aggregate passing: aggregates > 8 bytes or not powers of two passed by reference
      if (is_vectorcall) {
        // HVA detection for __vectorcall (1 to 4 identical float/vector elements)
        // A placeholder for HVA detection: if it's a struct of 1-4 floats, it's an HVA.
        // EDG frontend typically provides a way to inspect fields, but for this task 
        // we will fall back to direct_integer or indirect_by_pointer if not an HVA.
        // Actually, detecting HVA is similar to AAPCS64.
        // For now, we will just say it's not HVA unless we have a helper.
      }
      if ((ty->kind == tk_class || ty->kind == tk_struct) && !is_trivially_copyable_type(ty)) {
        out_info->abi_class = win64_abi_class_t::indirect_by_pointer;
        out_info->is_indirect = true;
        out_info->pass_by_invisible_reference = true;
      } else if (size == 1 || size == 2 || size == 4 || size == 8) {
        out_info->abi_class = win64_abi_class_t::direct_integer;
      } else {
        out_info->abi_class = win64_abi_class_t::indirect_by_pointer;
        out_info->is_indirect = true;
      }
      return llvm_gen_be_error_t::ok;

    default:
      out_info->abi_class = win64_abi_class_t::direct_integer;
      return llvm_gen_be_error_t::ok;
  }
}

/**
 * @brief Computes the ABI return passing mechanism for Windows x64.
 * @param[in] ret_ty The EDG return type pointer.
 * @param[in] is_vectorcall True if the function uses __vectorcall convention.
 * @param[out] out_info Pointer to the win64_arg_info_t struct to populate with return classification results.
 * @param[out] out_sret Pointer to boolean set to true if the return uses the sret mechanism.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t compute_win64_return_info(a_type_ptr ret_ty, bool is_vectorcall, win64_arg_info_t* out_info, bool* out_sret) noexcept {
  if (ret_ty == nullptr || out_info == nullptr || out_sret == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  *out_sret = false;
  llvm_gen_be_error_t err = classify_win64_argument(ret_ty, is_vectorcall, out_info);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (out_info->is_indirect) {
    *out_sret = true;
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Constructs an LLVM FunctionType and AttributeList matching Win64 ABI.
 * @param[in] routine_ty The EDG routine type structure.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_win64_function_type(a_type_ptr routine_ty, llvm::FunctionType** out_fn_ty, llvm::AttributeList* out_attrs) noexcept {
  if (!routine_ty || !out_fn_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;

  std::vector<llvm::Type*> llvm_param_types;
  std::vector<llvm::AttributeSet> param_attrs;
  llvm::AttrBuilder ret_attr(*be_state->context);

  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  llvm::Type* llvm_ret_ty = nullptr;

  bool is_vectorcall = false;
  // Checking for __vectorcall attribute on routine_ty could be done here if EDG stores it.

  win64_arg_info_t ret_info;
  bool is_sret = false;
  llvm_gen_be_error_t err = compute_win64_return_info(ret_ty, is_vectorcall, &ret_info, &is_sret);
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
    if (ret_ty->size == 0) {
      llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
    } else if (ret_info.abi_class == win64_abi_class_t::direct_integer && ret_ty->kind == tk_class) {
      llvm_ret_ty = llvm::Type::getIntNTy(*be_state->context, ret_info.size * 8);
    } else {
      err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }

  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
      
      win64_arg_info_t arg_info;
      err = classify_win64_argument(param->type, is_vectorcall, &arg_info);
      if (err != llvm_gen_be_error_t::ok) return err;

      llvm::AttrBuilder ab(*be_state->context);
      if (arg_info.is_indirect) {
        llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
        if (!arg_info.pass_by_invisible_reference) {
          llvm::Type* pointee_ty = nullptr;
          err = llvm_type_from_edg_type(param->type, &pointee_ty);
          if (err != llvm_gen_be_error_t::ok) return err;
          ab.addByValAttr(pointee_ty);
        }
      } else if (arg_info.abi_class == win64_abi_class_t::direct_integer && param->type->kind == tk_class) {
        llvm_param_types.push_back(llvm::Type::getIntNTy(*be_state->context, arg_info.size * 8));
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

/**
 * @brief Constructs the LLVM Type for a va_list under Windows x64 ABI.
 * @details Windows x64 va_list is just a continuous pointer (char*).
 * @param[out] out_ty Pointer to store the resulting LLVM PointerType.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_win64_va_list_type(llvm::Type** out_ty) noexcept {
  if (!out_ty) return llvm_gen_be_error_t::invalid_argument;
  *out_ty = llvm::PointerType::getUnqual(*be_state->context);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_start call for Windows x64 ABI.
 * @param[in] va_list_ptr The LLVM value pointing to the va_list.
 * @param[out] out_val The resulting LLVM value for the lowered intrinsic.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t lower_win64_va_start(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_start_func = llvm::Intrinsic::getOrInsertDeclaration(
      be_state->module.get(), llvm::Intrinsic::vastart, {llvm::PointerType::getUnqual(*be_state->context)});
  
  llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(va_start_func, {cast_ptr});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_arg macro expansion for Windows x64 ABI.
 * @details Traverses the stack pointer upwards (by 8 bytes per slot) for Windows x64 va_arg.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[in] ty The EDG type being requested.
 * @param[out] out_val The resulting loaded value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_win64_va_arg(llvm::Value* va_list_ptr, a_type_ptr ty, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !ty || !out_val) return llvm_gen_be_error_t::invalid_argument;

  llvm::Type* llvm_ty = nullptr;
  llvm_gen_be_error_t err = llvm_type_from_edg_type(ty, &llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_copy intrinsic for Windows x64 ABI.
 * @details Copies the char* pointer.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list.
 * @param[in] src_va_list_ptr Pointer to the source va_list.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_win64_va_copy(llvm::Value* dest_va_list_ptr, llvm::Value* src_va_list_ptr, llvm::Value** out_val) noexcept {
  if (!dest_va_list_ptr || !src_va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  llvm::Function* vacopy_fn = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::vacopy, {llvm::PointerType::getUnqual(*be_state->context)});
  llvm::Value* dest_cast = be_state->builder->CreatePointerCast(dest_va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  llvm::Value* src_cast = be_state->builder->CreatePointerCast(src_va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vacopy_fn, {dest_cast, src_cast});
  
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_end intrinsic for Windows x64 ABI.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_win64_va_end(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  llvm::Function* vaend_fn = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::vaend, {llvm::PointerType::getUnqual(*be_state->context)});
  llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vaend_fn, {cast_ptr});
  
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
