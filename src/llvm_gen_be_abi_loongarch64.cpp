/**
 * @file llvm_gen_be_abi_loongarch64.cpp
 * @ingroup llvm_gen_be_abi
 * @brief LoongArch 64 ABI classification subsystem.
 * @details Implements the parameter passing and return value classification
 * rules for LoongArch 64.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_abi_loongarch64.h"
#include "llvm_gen_be_internal.h"
#include <llvm/IR/Intrinsics.h>
#include <vector>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @struct loongarch_field_info
 * @brief Describes a flattened primitive field within a LoongArch aggregate.
 * @details Records whether the primitive scalar is floating-point and its byte size.
 */
struct loongarch_field_info {
  bool is_float; ///< True if the field is a floating-point type.
  uint64_t size; ///< Size of the field in bytes.
};

/**
 * @brief Recursively flattens an aggregate type into primitive scalar fields for LoongArch64.
 * @details Traverses nested structs and arrays to collect primitive scalar fields.
 * @param[in] ty The EDG type to flatten.
 * @param[in,out] fields Pointer to vector receiving flattened field descriptors.
 * @param[in,out] is_valid Pointer to flag set to false if an unflatten-able member is found.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t flatten_loongarch_struct(
    a_type_ptr ty,
    std::vector<loongarch_field_info>* fields,
    bool* is_valid) noexcept {
  
  if (!ty) return llvm_gen_be_error_t::invalid_argument;
  
  while (ty->kind == tk_typeref) {
    ty = ty->variant.typeref.type;
  }

  if (ty->kind == tk_float) {
    fields->push_back({true, static_cast<uint64_t>(ty->size)});
    return llvm_gen_be_error_t::ok;
  } else if (ty->kind == tk_integer || ty->kind == tk_pointer) {
    fields->push_back({false, static_cast<uint64_t>(ty->size)});
    return llvm_gen_be_error_t::ok;
  } else if (ty->kind == tk_array) {
    a_type_ptr ety = ty->variant.array.element_type;
    uint32_t count = static_cast<uint32_t>(ty->variant.array.variant.number_of_elements);
    for (uint32_t i = 0; i < count; i++) {
      llvm_gen_be_error_t err = flatten_loongarch_struct(ety, fields, is_valid);
      if (err != llvm_gen_be_error_t::ok || !*is_valid) return err;
    }
    return llvm_gen_be_error_t::ok;
  } else if (ty->kind == tk_struct || ty->kind == tk_class) {
    for (a_field_ptr field = ty->variant.class_struct_union.field_list; field != nullptr; field = field->next) {
      if (field->is_bit_field && field->bit_size == 0) continue;
      llvm_gen_be_error_t err = flatten_loongarch_struct(field->type, fields, is_valid);
      if (err != llvm_gen_be_error_t::ok || !*is_valid) return err;
    }
    return llvm_gen_be_error_t::ok;
  }

  *is_valid = false;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Classifies a single EDG argument type according to LoongArch 64 ABI rules.
 * @details Determines register classes (integer, float, pair, or memory) and coerced LLVM type.
 * @param[in] ty The EDG type to classify.
 * @param[out] out_info Pointer to the loongarch64_arg_info_t structure receiving classification results.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_loongarch64_argument(
    a_type_ptr ty,
    loongarch64_arg_info_t* out_info) noexcept {
  
  if (!ty || !out_info) return llvm_gen_be_error_t::invalid_argument;

  out_info->abi_class = loongarch64_abi_class_t::no_class;
  out_info->is_indirect = false;
  out_info->requires_sret = false;
  out_info->coerced_type = nullptr;

  while (ty->kind == tk_typeref) {
    ty = ty->variant.typeref.type;
  }

  if (ty->size == 0) {
    out_info->abi_class = loongarch64_abi_class_t::no_class;
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_integer || ty->kind == tk_pointer) {
    out_info->abi_class = loongarch64_abi_class_t::integer;
    return llvm_type_from_edg_type(ty, &out_info->coerced_type);
  }

  if (ty->kind == tk_float) {
    out_info->abi_class = loongarch64_abi_class_t::float_class;
    return llvm_type_from_edg_type(ty, &out_info->coerced_type);
  }

  if (ty->size > 16) {
    out_info->abi_class = loongarch64_abi_class_t::memory;
    out_info->is_indirect = true;
    return llvm_gen_be_error_t::ok;
  }

  std::vector<loongarch_field_info> fields;
  bool is_valid = true;
  llvm_gen_be_error_t err = flatten_loongarch_struct(ty, &fields, &is_valid);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (!is_valid || fields.empty()) {
    out_info->abi_class = loongarch64_abi_class_t::integer_integer;
    out_info->coerced_type = llvm::ArrayType::get(llvm::Type::getInt64Ty(*be_state->context), static_cast<uint64_t>((ty->size + 7) / 8));
    return llvm_gen_be_error_t::ok;
  }

  uint32_t num_floats = 0;
  for (const auto& f : fields) {
    if (f.is_float) num_floats++;
  }

  if (fields.size() == 1 && num_floats == 1) {
    out_info->abi_class = loongarch64_abi_class_t::float_class;
    out_info->coerced_type = fields[0].size == 4 ? llvm::Type::getFloatTy(*be_state->context) : llvm::Type::getDoubleTy(*be_state->context);
  } else if (fields.size() == 2 && num_floats == 2) {
    out_info->abi_class = loongarch64_abi_class_t::float_float;
    llvm::Type* ft1 = fields[0].size == 4 ? llvm::Type::getFloatTy(*be_state->context) : llvm::Type::getDoubleTy(*be_state->context);
    llvm::Type* ft2 = fields[1].size == 4 ? llvm::Type::getFloatTy(*be_state->context) : llvm::Type::getDoubleTy(*be_state->context);
    out_info->coerced_type = llvm::StructType::get(ft1, ft2);
  } else if (fields.size() == 2 && num_floats == 1) {
    if (fields[0].is_float) {
      out_info->abi_class = loongarch64_abi_class_t::float_integer;
      llvm::Type* ft = fields[0].size == 4 ? llvm::Type::getFloatTy(*be_state->context) : llvm::Type::getDoubleTy(*be_state->context);
      llvm::Type* it = llvm::Type::getIntNTy(*be_state->context, static_cast<unsigned>(fields[1].size * 8));
      out_info->coerced_type = llvm::StructType::get(ft, it);
    } else {
      out_info->abi_class = loongarch64_abi_class_t::integer_float;
      llvm::Type* it = llvm::Type::getIntNTy(*be_state->context, static_cast<unsigned>(fields[0].size * 8));
      llvm::Type* ft = fields[1].size == 4 ? llvm::Type::getFloatTy(*be_state->context) : llvm::Type::getDoubleTy(*be_state->context);
      out_info->coerced_type = llvm::StructType::get(it, ft);
    }
  } else {
    out_info->abi_class = loongarch64_abi_class_t::integer_integer;
    if (ty->size <= 8) {
       out_info->coerced_type = llvm::Type::getInt64Ty(*be_state->context);
    } else {
       out_info->coerced_type = llvm::ArrayType::get(llvm::Type::getInt64Ty(*be_state->context), 2);
    }
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes the return value classification and sret requirement for LoongArch 64.
 * @details Evaluates whether the return type is passed in registers or via sret pointer.
 * @param[in] ret_ty The EDG return type.
 * @param[out] out_info Pointer to store the return classification info.
 * @param[out] out_sret Pointer to boolean set to true if sret is required.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t compute_loongarch64_return_info(
    a_type_ptr ret_ty,
    loongarch64_arg_info_t* out_info,
    bool* out_sret) noexcept {
  
  if (!ret_ty || !out_info || !out_sret) return llvm_gen_be_error_t::invalid_argument;

  *out_sret = false;
  llvm_gen_be_error_t err = classify_loongarch64_argument(ret_ty, out_info);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (out_info->abi_class == loongarch64_abi_class_t::memory) {
    out_info->requires_sret = true;
    *out_sret = true;
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Builds the LLVM FunctionType and AttributeList for a LoongArch 64 routine.
 * @details Applies LoongArch LP64D ABI coercion rules to return type and parameters.
 * @param[in] routine_ty The EDG routine type.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_loongarch64_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {
    
  if (!routine_ty || !out_fn_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;

  std::vector<llvm::Type*> llvm_param_types;
  std::vector<llvm::AttributeSet> param_attrs;
  llvm::AttrBuilder ret_attr{*be_state->context};

  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  llvm::Type* llvm_ret_ty = nullptr;
  
  loongarch64_arg_info_t ret_info;
  bool is_sret = false;
  llvm_gen_be_error_t err = compute_loongarch64_return_info(ret_ty, &ret_info, &is_sret);
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
    if (ret_info.abi_class == loongarch64_abi_class_t::no_class) {
      llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
    } else {
      llvm_ret_ty = ret_info.coerced_type;
    }
  }

  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
      
      loongarch64_arg_info_t arg_info;
      err = classify_loongarch64_argument(param->type, &arg_info);
      if (err != llvm_gen_be_error_t::ok) return err;

      if (arg_info.abi_class == loongarch64_abi_class_t::no_class) {
        continue;
      }

      llvm::AttrBuilder ab{*be_state->context};
      if (arg_info.is_indirect) {
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
 * @brief Lowers a va_start intrinsic for the LoongArch 64 ABI.
 * @details Invokes the llvm.va_start intrinsic on the va_list pointer.
 * @param[in] va_list_ptr Pointer to the va_list object.
 * @param[out] out_val Pointer to store the resulting CallInst.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_loongarch64_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
    
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_start_func = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vastart);
  
  *out_val = be_state->builder->CreateCall(va_start_func, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_arg operation for the LoongArch 64 ABI.
 * @details Emits an LLVM VAArgInst for the requested EDG type.
 * @param[in] va_list_ptr Pointer to the va_list object.
 * @param[in] ty The EDG type being extracted.
 * @param[out] out_val Pointer to store the extracted LLVM value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_loongarch64_va_arg(
    llvm::Value* va_list_ptr,
    a_type_ptr ty,
    llvm::Value** out_val) noexcept {
  
  if (!va_list_ptr || !ty || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Type* llvm_ty = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(ty, &llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  loongarch64_arg_info_t info = {};
  LLVM_GEN_BE_CHECK(classify_loongarch64_argument(ty, &info));
  if (info.is_indirect) {
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Value* ind_ptr = be_state->builder->CreateVAArg(va_list_ptr, ptr_ty, "vaarg.ind.ptr");
    *out_val = be_state->builder->CreateLoad(llvm_ty, ind_ptr, "vaarg.ind.val");
    return llvm_gen_be_error_t::ok;
  }

  *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_end intrinsic for the LoongArch 64 ABI.
 * @details Invokes the llvm.va_end intrinsic on the va_list pointer.
 * @param[in] va_list_ptr Pointer to the va_list object.
 * @param[out] out_val Pointer to store the resulting CallInst.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_loongarch64_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
  
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_end_func = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vaend);
  
  *out_val = be_state->builder->CreateCall(va_end_func, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_copy intrinsic for the LoongArch 64 ABI.
 * @details Invokes the llvm.va_copy intrinsic on destination and source va_list pointers.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list object.
 * @param[in] src_va_list_ptr Pointer to the source va_list object.
 * @param[out] out_val Pointer to store the resulting CallInst.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_loongarch64_va_copy(
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
