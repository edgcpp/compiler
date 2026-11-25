/**
 * @file llvm_gen_be_abi_mips.cpp
 * @ingroup llvm_gen_be_abi
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
#include <llvm/TargetParser/Triple.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Recursively flattens a struct into primitive types for MIPS N64 ABI.
 * @details Traverses nested structs and arrays to collect primitive scalar fields.
 * @param[in] ty EDG type to flatten.
 * @param[in,out] fields Output vector of primitive fields.
 * @param[in] offset Current byte offset.
 * @param[in,out] offsets Output vector of field byte offsets.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t flatten_mips_struct(
    a_type_ptr ty,
    std::vector<a_type_ptr>* fields,
    uint64_t offset,
    std::vector<uint64_t>* offsets) noexcept {
  if (ty == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  ty = skip_typerefs(ty);

  if (ty->kind == tk_class || ty->kind == tk_struct || ty->kind == tk_union) {
    for (a_field_ptr f = ty->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
      uint64_t f_offset = (ty->kind == tk_union) ? offset : offset + static_cast<uint64_t>(f->offset);
      if (f->is_bit_field && f->bit_size == 0) continue;
      llvm_gen_be_error_t err = flatten_mips_struct(f->type, fields, f_offset, offsets);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  } else if (ty->kind == tk_array) {
    a_type_ptr elem_ty = ty->variant.array.element_type;
    uint64_t num_elems = static_cast<uint64_t>(ty->variant.array.variant.number_of_elements);
    for (uint64_t i = 0; i < num_elems; ++i) {
      llvm_gen_be_error_t err = flatten_mips_struct(elem_ty, fields, offset + i * static_cast<uint64_t>(elem_ty->size), offsets);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  } else if (ty->kind == tk_integer || ty->kind == tk_float || ty->kind == tk_pointer || ty->kind == tk_ptr_to_member) {
    fields->push_back(ty);
    offsets->push_back(offset);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Detects if a MIPS N64 struct can be flattened into up to 2 FP registers.
 * @details Inspects flattened scalar fields and produces a coerced StructType when
 * 1 or 2 floating-point fields are present.
 * @param[in] ty The EDG aggregate type to inspect.
 * @param[out] coerced_ty Pointer to store the coerced LLVM type.
 * @param[out] out_flattened Set to true if the aggregate was flattened.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t detect_mips_flattening(
    a_type_ptr ty,
    llvm::Type** coerced_ty,
    bool* out_flattened) noexcept {
  *out_flattened = false;
  if (!is_class_struct_union_type(ty)) {
    return llvm_gen_be_error_t::ok;
  }

  std::vector<a_type_ptr> fields;
  std::vector<uint64_t> offsets;
  llvm_gen_be_error_t err = flatten_mips_struct(ty, &fields, 0, &offsets);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (!fields.empty() && fields.size() <= 2) {
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
        err = get_llvm_type(f_ty, &lty);
        if (err != llvm_gen_be_error_t::ok) return err;
        llvm_fields.push_back(lty);
      }
      *coerced_ty = llvm::StructType::get(*be_state->context, llvm_fields, false);
      *out_flattened = true;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Classifies a single EDG argument type according to MIPS / MIPS64 ABI rules.
 * @details Determines whether the argument is passed in integer registers, floating-point
 * registers, coerced into an integer/FP array or struct, or passed by value on the stack.
 * @param[in] ty The EDG type to classify.
 * @param[in] is_soft_float True if soft-float ABI is active.
 * @param[in] is_n64 True if targeting 64-bit MIPS N64 ABI; false for 32-bit O32 ABI.
 * @param[out] out_info Pointer to the mips_arg_info_t structure receiving classification results.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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
  out_info->padding = 0;

  while (ty->kind == tk_typeref) {
    ty = ty->variant.typeref.type;
  }
  out_info->align = ty->alignment;

  if (ty->size == 0) {
    out_info->abi_class = mips_abi_class_t::no_class;
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_integer || ty->kind == tk_pointer) {
    out_info->abi_class = mips_abi_class_t::integer;
    return llvm_type_from_edg_type(ty, &out_info->coerced_type);
  }

  if (ty->kind == tk_float) {
    out_info->abi_class = is_soft_float ? mips_abi_class_t::integer : mips_abi_class_t::float_class;
    return llvm_type_from_edg_type(ty, &out_info->coerced_type);
  }

  // Struct handling
  if (is_n64) {
    // N64/N32
    if (ty->size <= 16) {
      out_info->abi_class = mips_abi_class_t::struct_coerced;
      bool flattened = false;
      if (!is_soft_float) {
        llvm_gen_be_error_t err = detect_mips_flattening(ty, &out_info->coerced_type, &flattened);
        if (err != llvm_gen_be_error_t::ok) return err;
      }
      if (!flattened) {
        out_info->coerced_type = llvm::ArrayType::get(llvm::Type::getInt64Ty(*be_state->context), static_cast<uint64_t>((ty->size + 7) / 8));
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

/**
 * @brief Computes MIPS return value passing information.
 * @details Determines whether the return value is passed in registers, coerced, or via hidden `sret` pointer.
 * @param[in] ret_ty The EDG type representing the function return type.
 * @param[in] is_soft_float Whether soft-float ABI is used.
 * @param[in] is_n64 Whether N64 ABI is used (64-bit pointers).
 * @param[out] out_info The classification result information structure.
 * @param[out] out_sret Pointer to a boolean set to true if the return is via sret.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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
    if (out_info->abi_class == mips_abi_class_t::struct_byval) {
      out_info->requires_sret = true;
      *out_sret = true;
    }
  } else {
    if (out_info->abi_class == mips_abi_class_t::struct_byval && ret_ty->size > 8) {
      out_info->requires_sret = true;
      *out_sret = true;
    } else if (out_info->abi_class == mips_abi_class_t::struct_byval) {
      out_info->abi_class = mips_abi_class_t::struct_coerced;
      out_info->coerced_type = llvm::ArrayType::get(llvm::Type::getInt32Ty(*be_state->context), static_cast<uint64_t>((ret_ty->size + 3) / 4));
    }
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Constructs an LLVM FunctionType and AttributeList matching MIPS / MIPS64 ABI.
 * @details Classifies return and parameter types and attaches `sret`/`byval` attributes as needed.
 * @param[in] routine_ty The EDG routine type structure.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_mips_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {
    
  if (!routine_ty || !out_fn_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;

  std::vector<llvm::Type*> llvm_param_types;
  std::vector<llvm::AttributeSet> param_attrs;
  llvm::AttrBuilder ret_attr{*be_state->context};

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  bool is_soft_float = (triple.getEnvironmentName().find("softfloat") != std::string::npos);
  bool is_n64 = (targ_sizeof_pointer == 8 || triple.isMIPS64());

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
    
    llvm::AttrBuilder sret_ab{*be_state->context};
    llvm::Type* sret_pointee_ty = nullptr;
    err = llvm_type_from_edg_type(ret_ty, &sret_pointee_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    sret_ab.addStructRetAttr(sret_pointee_ty);
    param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  } else {
    if (ret_info.abi_class == mips_abi_class_t::no_class) {
      llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
    } else {
      llvm_ret_ty = ret_info.coerced_type;
    }
  }

  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
      
      mips_arg_info_t arg_info;
      err = classify_mips_argument(param->type, is_soft_float, is_n64, &arg_info);
      if (err != llvm_gen_be_error_t::ok) return err;

      if (arg_info.abi_class == mips_abi_class_t::no_class) continue;

      llvm::AttrBuilder ab{*be_state->context};
      if (arg_info.abi_class == mips_abi_class_t::struct_byval) {
        llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
        llvm::Type* pointee_ty = nullptr;
        err = llvm_type_from_edg_type(param->type, &pointee_ty);
        if (err != llvm_gen_be_error_t::ok) return err;
        ab.addByValAttr(pointee_ty);
      } else {
        llvm_param_types.push_back(arg_info.coerced_type);
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
 * @brief Lowers a va_start intrinsic for the MIPS / MIPS64 ABI.
 * @details Emits `llvm.va_start` on the provided `va_list` pointer.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_mips_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
    
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_start_func = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vastart);
  
  *out_val = be_state->builder->CreateCall(va_start_func, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_arg macro expansion for the MIPS ABI.
 * @details Emits an LLVM `va_arg` instruction for the requested EDG type.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[in] ty The EDG type being requested.
 * @param[out] out_val The resulting loaded value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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

/**
 * @brief Lowers a va_end intrinsic for the MIPS ABI.
 * @details Emits `llvm.va_end` on the provided `va_list` pointer.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_mips_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
  
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_end_func = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vaend);
  
  *out_val = be_state->builder->CreateCall(va_end_func, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_copy intrinsic for the MIPS ABI.
 * @details Emits `llvm.va_copy` from `src_va_list_ptr` to `dest_va_list_ptr`.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list struct.
 * @param[in] src_va_list_ptr Pointer to the source va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_mips_va_copy(
    llvm::Value* dest_va_list_ptr,
    llvm::Value* src_va_list_ptr,
    llvm::Value** out_val) noexcept {
  
  if (!dest_va_list_ptr || !src_va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_copy_func = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vacopy);
  
  *out_val = be_state->builder->CreateCall(va_copy_func, {dest_va_list_ptr, src_va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
