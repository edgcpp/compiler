/**
 * @file llvm_gen_be_abi_mips.cpp
 * @brief MIPS / MIPS64 ABI classification subsystem.
 * @details Implements the parameter passing and return value classification
 * rules for MIPS / MIPS64.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_abi_mips.h"
#include "llvm_gen_be_internal.h"
#include <llvm/IR/Intrinsics.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

static void flatten_mips_struct(a_type_ptr ty, std::vector<a_type_ptr>& fields, uint64_t offset, std::vector<uint64_t>& offsets) {
    if (ty == nullptr) return;
    ty = skip_typerefs(ty);

    if (ty->kind == tk_class || ty->kind == tk_struct || ty->kind == tk_union) {
        for (a_field_ptr f = ty->variant.class_struct_union.field_list; f; f = f->next) {
            uint64_t f_offset = (ty->kind == tk_union) ? offset : offset + f->offset;
            if (f->is_bit_field && f->bit_size == 0) continue;
            flatten_mips_struct(f->type, fields, f_offset, offsets);
        }
    } else if (ty->kind == tk_array) {
        a_type_ptr elem_ty = ty->variant.array.element_type;
        uint64_t num_elems = ty->variant.array.variant.number_of_elements;
        for (uint64_t i = 0; i < num_elems; ++i) {
            flatten_mips_struct(elem_ty, fields, offset + i * elem_ty->size, offsets);
        }
    } else if (ty->kind == tk_integer || ty->kind == tk_float || ty->kind == tk_pointer || ty->kind == tk_ptr_to_member) {
        fields.push_back(ty);
        offsets.push_back(offset);
    }
}

static bool detect_mips_flattening(a_type_ptr ty, llvm::Type** coerced_ty) {
    if (!is_class_struct_union_type(ty)) return false;
    
    std::vector<a_type_ptr> fields;
    std::vector<uint64_t> offsets;
    flatten_mips_struct(ty, fields, 0, offsets);

    if (fields.size() > 0 && fields.size() <= 2) {
        bool has_fp = false;
        for (a_type_ptr f_ty : fields) {
            if (f_ty->kind == tk_float) {
                has_fp = true;
                break;
            }
        }
        if (has_fp) {
            std::vector<llvm::Type*> llvm_fields;
            for (a_type_ptr f_ty : fields) {
                llvm::Type* lty = nullptr;
                if (get_llvm_type(f_ty, &lty) == llvm_gen_be_error_t::ok) {
                    llvm_fields.push_back(lty);
                }
            }
            if (llvm_fields.size() == fields.size()) {
                *coerced_ty = llvm::StructType::get(*be_state->context, llvm_fields, false);
                return true;
            }
        }
    }
    return false;
}

llvm_gen_be_error_t classify_mips_argument(
    a_type_ptr ty,
    bool is_soft_float,
    bool is_n64,
    mips_arg_info_t* out_info) noexcept {
  
  if (!ty || !out_info) return llvm_gen_be_error_t::invalid_argument;

  out_info->abi_class = mips_abi_class_t::no_class;
  out_info->is_indirect = false;
  out_info->requires_sret = false;
  out_info->coerced_type = nullptr;
  out_info->align = ty->alignment;
  out_info->padding = 0;

  while (ty->kind == tk_typeref) {
    ty = ty->variant.typeref.type;
  }

  if (ty->size == 0) {
    out_info->abi_class = mips_abi_class_t::no_class;
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_integer || ty->kind == tk_enum || ty->kind == tk_pointer) {
    out_info->abi_class = mips_abi_class_t::integer;
    return llvm_type_from_edg_type(ty, &out_info->coerced_type);
  }

  if (ty->kind == tk_float) {
    out_info->abi_class = mips_abi_class_t::float_class;
    return llvm_type_from_edg_type(ty, &out_info->coerced_type);
  }

  // Struct handling
  if (is_n64) {
    // N64/N32
    if (ty->size <= 16) {
      out_info->abi_class = mips_abi_class_t::struct_coerced;
      if (!is_soft_float && detect_mips_flattening(ty, &out_info->coerced_type)) {
          // Flattened to FP struct
      } else {
          out_info->coerced_type = llvm::ArrayType::get(llvm::Type::getInt64Ty(*be_state->context), (ty->size + 7) / 8);
      }
    } else {
      out_info->abi_class = mips_abi_class_t::struct_byval;
    }
  } else {
    // O32
    out_info->abi_class = mips_abi_class_t::struct_byval;
  }

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t compute_mips_return_info(
    a_type_ptr ret_ty,
    bool is_soft_float,
    bool is_n64,
    mips_arg_info_t* out_info,
    bool* out_sret) noexcept {
  
  if (!ret_ty || !out_info || !out_sret) return llvm_gen_be_error_t::invalid_argument;

  *out_sret = false;
  llvm_gen_be_error_t err = classify_mips_argument(ret_ty, is_soft_float, is_n64, out_info);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (is_n64) {
    if (out_info->abi_class == mips_abi_class_t::struct_byval || ret_ty->size > 16) {
      out_info->requires_sret = true;
      *out_sret = true;
    }
  } else {
    // O32 returns struct in registers if size <= 8. Wait, O32 returns struct > 8 indirectly.
    // Sometimes even 8 bytes is indirect depending on type. Usually struct > 8 indirect.
    if (out_info->abi_class == mips_abi_class_t::struct_byval && ret_ty->size > 8) {
      out_info->requires_sret = true;
      *out_sret = true;
    } else if (out_info->abi_class == mips_abi_class_t::struct_byval) {
      out_info->abi_class = mips_abi_class_t::struct_coerced;
      out_info->coerced_type = llvm::ArrayType::get(llvm::Type::getInt32Ty(*be_state->context), (ret_ty->size + 3) / 4);
    }
  }

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t build_mips_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {
    
  if (!routine_ty || !out_fn_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;

  std::vector<llvm::Type*> llvm_param_types;
  std::vector<llvm::AttributeSet> param_attrs;
  llvm::AttrBuilder ret_attr(*be_state->context);

  bool is_soft_float = false; // Simplified
  bool is_n64 = targ_sizeof_pointer == 8;

  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  llvm::Type* llvm_ret_ty = nullptr;

  mips_arg_info_t ret_info;
  bool is_sret = false;
  llvm_gen_be_error_t err = compute_mips_return_info(ret_ty, is_soft_float, is_n64, &ret_info, &is_sret);
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
    if (ret_info.abi_class == mips_abi_class_t::no_class) {
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
      
      mips_arg_info_t arg_info;
      err = classify_mips_argument(param->type, is_soft_float, is_n64, &arg_info);
      if (err != llvm_gen_be_error_t::ok) return err;

      if (arg_info.abi_class == mips_abi_class_t::no_class) continue;

      llvm::AttrBuilder ab(*be_state->context);
      if (arg_info.abi_class == mips_abi_class_t::struct_byval) {
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

llvm_gen_be_error_t lower_mips_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
    
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_start_func = llvm::Intrinsic::getOrInsertDeclaration(
      be_state->module.get(), llvm::Intrinsic::vastart, {llvm::PointerType::getUnqual(*be_state->context)});
  
  *out_val = be_state->builder->CreateCall(va_start_func, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t lower_mips_va_arg(
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

llvm_gen_be_error_t lower_mips_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
  
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_end_func = llvm::Intrinsic::getOrInsertDeclaration(
      be_state->module.get(), llvm::Intrinsic::vaend, {llvm::PointerType::getUnqual(*be_state->context)});
  
  *out_val = be_state->builder->CreateCall(va_end_func, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t lower_mips_va_copy(
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
