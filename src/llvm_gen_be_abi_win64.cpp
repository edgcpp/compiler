/**
 * @file llvm_gen_be_abi_win64.cpp
 * @ingroup llvm_gen_be_abi
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
 * @brief Internal recursive helper for Windows x64 __vectorcall HFA/HVA detection.
 * @details Traverses struct fields and fixed-size arrays to count homogeneous
 * float, double, or vector elements (1 to 4 identical elements).
 * @param[in] ty The EDG type to inspect.
 * @param[in,out] num_elements Counter for homogeneous elements found.
 * @param[in,out] elem_ty The resolved LLVM element type.
 * @param[in,out] is_valid Flag set to false if heterogeneous or non-HVA fields are encountered.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t detect_win64_hva_internal(
    a_type_ptr ty,
    uint32_t* num_elements,
    llvm::Type** elem_ty,
    bool* is_valid) noexcept {
  if (ty == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  while (ty->kind == tk_typeref) {
    ty = ty->variant.typeref.type;
  }

  if (ty->kind == tk_float
#if GNU_VECTOR_TYPES_ALLOWED
      || ty->kind == tk_vector
#endif
  ) {
    llvm::Type* fty = nullptr;
    llvm_gen_be_error_t err = llvm_type_from_edg_type(ty, &fty);
    if (err != llvm_gen_be_error_t::ok) return err;

    if (*elem_ty == nullptr) {
      *elem_ty = fty;
    } else if (*elem_ty != fty) {
      *is_valid = false;
      return llvm_gen_be_error_t::ok;
    }
    (*num_elements)++;
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_array) {
    a_type_ptr ety = ty->variant.array.element_type;
    uint32_t count = static_cast<uint32_t>(ty->variant.array.variant.number_of_elements);
    if (count == 0) {
      *is_valid = false;
      return llvm_gen_be_error_t::ok;
    }
    for (uint32_t i = 0; i < count; ++i) {
      llvm_gen_be_error_t err = detect_win64_hva_internal(ety, num_elements, elem_ty, is_valid);
      if (err != llvm_gen_be_error_t::ok || !*is_valid) return err;
    }
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_struct || ty->kind == tk_class) {
    if (ty->variant.class_struct_union.field_list == nullptr) {
      *is_valid = false;
      return llvm_gen_be_error_t::ok;
    }
    for (a_field_ptr field = ty->variant.class_struct_union.field_list; field != nullptr; field = field->next) {
      if (field->is_bit_field && field->bit_size == 0) continue;
      llvm_gen_be_error_t err = detect_win64_hva_internal(field->type, num_elements, elem_ty, is_valid);
      if (err != llvm_gen_be_error_t::ok || !*is_valid) return err;
    }
    return llvm_gen_be_error_t::ok;
  }

  *is_valid = false;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Classifies a single EDG type into Win64 ABI class.
 * @details Implements or declares `classify_win64_argument` for the EDG LLVM backend.
 * @param[in] ty The EDG type to classify.
 * @param[in] is_vectorcall True if the function uses __vectorcall convention.
 * @param[out] out_info The classification result information structure.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_win64_argument(a_type_ptr ty, bool is_vectorcall, win64_arg_info_t* out_info) noexcept {
  if (ty == nullptr || out_info == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());

  while (ty->kind == tk_typeref) {
    ty = ty->variant.typeref.type;
  }

  uint64_t size = static_cast<uint64_t>(ty->size);
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
    case tk_nullptr:
    case tk_ptr_to_member:
      out_info->abi_class = win64_abi_class_t::direct_integer;
      return llvm_gen_be_error_t::ok;

    case tk_float:
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
#endif
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
      if ((ty->kind == tk_class || ty->kind == tk_struct) && !is_trivially_copyable_type(ty)) {
        out_info->abi_class = win64_abi_class_t::indirect_by_pointer;
        out_info->is_indirect = true;
        out_info->pass_by_invisible_reference = true;
        return llvm_gen_be_error_t::ok;
      }
      if (is_vectorcall && (ty->kind == tk_class || ty->kind == tk_struct || ty->kind == tk_array)) {
        uint32_t hva_elems = 0;
        llvm::Type* hva_ty = nullptr;
        bool is_hva = true;
        llvm_gen_be_error_t err = detect_win64_hva_internal(ty, &hva_elems, &hva_ty, &is_hva);
        if (err != llvm_gen_be_error_t::ok) return err;
        if (is_hva && hva_elems <= 4) {
          out_info->abi_class = win64_abi_class_t::vectorcall_hva;
          out_info->hva_elem_ty = hva_ty;
          out_info->num_elements = hva_elems;
          out_info->is_indirect = false;
          return llvm_gen_be_error_t::ok;
        }
      }
      if (size == 1 || size == 2 || size == 4 || size == 8) {
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
 * @details Implements or declares `compute_win64_return_info` for the EDG LLVM backend.
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
 * @details Implements or declares `build_win64_function_type` for the EDG LLVM backend.
 * @param[in] routine_ty The EDG routine type structure.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_win64_function_type(a_type_ptr routine_ty, llvm::FunctionType** out_fn_ty, llvm::AttributeList* out_attrs) noexcept {
  if (!routine_ty || !out_fn_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;

  std::vector<llvm::Type*> llvm_param_types;
  std::vector<llvm::AttributeSet> param_attrs;
  llvm::AttrBuilder ret_attr{*be_state->context};

  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  llvm::Type* llvm_ret_ty = nullptr;

  bool is_vectorcall = false;
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
  if (routine_ty->variant.routine.extra_info != nullptr &&
      routine_ty->variant.routine.extra_info->calling_convention == cc_vectorcall) {
    is_vectorcall = true;
  }
#endif

  win64_arg_info_t ret_info;
  bool is_sret = false;
  llvm_gen_be_error_t err = compute_win64_return_info(ret_ty, is_vectorcall, &ret_info, &is_sret);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (is_sret) {
    llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
    llvm::Type* sret_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm_param_types.push_back(sret_ptr_ty);
    
    llvm::AttrBuilder sret_ab{*be_state->context};
    llvm::Type* sret_pointee_ty = nullptr;
    err = llvm_type_from_edg_type(ret_ty, &sret_pointee_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    sret_ab.addStructRetAttr(sret_pointee_ty);
    param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  } else {
    if (ret_ty->size == 0) {
      llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
    } else if (ret_info.abi_class == win64_abi_class_t::vectorcall_hva) {
      err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
    } else if (ret_info.abi_class == win64_abi_class_t::direct_integer && (ret_ty->kind == tk_class || ret_ty->kind == tk_struct)) {
      llvm_ret_ty = llvm::Type::getIntNTy(*be_state->context, static_cast<unsigned>(ret_info.size * 8));
    } else {
      err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }

  uint32_t vector_regs_used = 0;
  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
      
      win64_arg_info_t arg_info;
      err = classify_win64_argument(param->type, is_vectorcall, &arg_info);
      if (err != llvm_gen_be_error_t::ok) return err;

      llvm::AttrBuilder ab{*be_state->context};
      if (arg_info.abi_class == win64_abi_class_t::vectorcall_hva) {
        if (vector_regs_used + arg_info.num_elements <= 6) {
          vector_regs_used += arg_info.num_elements;
          llvm::Type* pty = nullptr;
          err = llvm_type_from_edg_type(param->type, &pty);
          if (err != llvm_gen_be_error_t::ok) return err;
          llvm_param_types.push_back(pty);
          if (param->type->kind == tk_class || param->type->kind == tk_struct) {
            ab.addAttribute(llvm::Attribute::InReg);
          }
        } else {
          llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
          llvm::Type* pointee_ty = nullptr;
          err = llvm_type_from_edg_type(param->type, &pointee_ty);
          if (err != llvm_gen_be_error_t::ok) return err;
          ab.addByValAttr(pointee_ty);
        }
      } else if (arg_info.is_indirect) {
        if (!arg_info.pass_by_invisible_reference) {
          llvm::Type* pointee_ty = nullptr;
          err = llvm_type_from_edg_type(param->type, &pointee_ty);
          if (err != llvm_gen_be_error_t::ok) return err;
        }
        llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
      } else if (arg_info.abi_class == win64_abi_class_t::direct_integer && (param->type->kind == tk_class || param->type->kind == tk_struct)) {
        llvm_param_types.push_back(llvm::Type::getIntNTy(*be_state->context, static_cast<unsigned>(arg_info.size * 8)));
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
 * @details Implements or declares `lower_win64_va_start` for the EDG LLVM backend.
 * @param[in] va_list_ptr The LLVM value pointing to the va_list.
 * @param[out] out_val The resulting LLVM value for the lowered intrinsic.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t lower_win64_va_start(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_start_func = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vastart);
  
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

  win64_arg_info_t arg_info = {};
  LLVM_GEN_BE_CHECK(classify_win64_argument(ty, false, &arg_info));
  llvm::BasicBlock* cur_bb = be_state->builder->GetInsertBlock();
  llvm::Function* cur_fn = cur_bb->getParent();
  if (cur_fn != nullptr) {
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
    llvm::Value* ap_cur = be_state->builder->CreateLoad(ptr_ty, va_list_ptr, "vaarg.win.ap");
    llvm::Value* ap_next = be_state->builder->CreateGEP(
        i8_ty, ap_cur, llvm::ConstantInt::get(i64_ty, 8), "vaarg.win.next");
    be_state->builder->CreateStore(ap_next, va_list_ptr);
    if (arg_info.is_indirect) {
      llvm::Value* ind_ptr = be_state->builder->CreateLoad(ptr_ty, ap_cur, "vaarg.ind.ptr");
      *out_val = be_state->builder->CreateLoad(llvm_ty, ind_ptr, "vaarg.ind.val");
    } else {
      *out_val = be_state->builder->CreateLoad(llvm_ty, ap_cur, "vaarg.win.val");
    }
    return llvm_gen_be_error_t::ok;
  }
  if (arg_info.is_indirect) {
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Value* ind_ptr = be_state->builder->CreateVAArg(va_list_ptr, ptr_ty, "vaarg.ind.ptr");
    uint64_t align_bytes = std::max<uint64_t>(1, static_cast<uint64_t>(ty->alignment));
    *out_val = be_state->builder->CreateAlignedLoad(
        llvm_ty, ind_ptr, llvm::MaybeAlign(align_bytes), "vaarg.ind.val");
    return llvm_gen_be_error_t::ok;
  }

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

  llvm::Function* vacopy_fn = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vacopy);
  llvm::Value* dest_cast = be_state->builder->CreatePointerCast(dest_va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  llvm::Value* src_cast = be_state->builder->CreatePointerCast(src_va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vacopy_fn, {dest_cast, src_cast});
  
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_end intrinsic for Windows x64 ABI.
 * @details Implements or declares `lower_win64_va_end` for the EDG LLVM backend.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_win64_va_end(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  llvm::Function* vaend_fn = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vaend);
  llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vaend_fn, {cast_ptr});
  
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
