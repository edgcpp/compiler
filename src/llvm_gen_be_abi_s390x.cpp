/**
 * @file llvm_gen_be_abi_s390x.cpp
 * @ingroup llvm_gen_be_abi
 * @brief SystemZ (s390x) ABI classification subsystem.
 * @details Implements the parameter passing and return value classification
 * rules for SystemZ (s390x).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_abi_s390x.h"
#include "llvm_gen_be_internal.h"
#include <llvm/IR/Intrinsics.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Classifies a single EDG argument type according to SystemZ (s390x) ABI rules.
 * @details Determines whether the argument is passed in GPRs, FPRs, coerced into an integer,
 * or passed indirectly in memory.
 * @param[in] ty The EDG type to classify.
 * @param[out] out_info Pointer to the s390x_arg_info_t structure receiving classification results.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_s390x_argument(
    a_type_ptr ty,
    s390x_arg_info_t* out_info) noexcept {
  
  if (!ty || !out_info) return llvm_gen_be_error_t::invalid_argument;

  out_info->abi_class = s390x_abi_class_t::no_class;
  out_info->is_indirect = false;
  out_info->requires_sret = false;
  out_info->coerced_type = nullptr;

  while (ty->kind == tk_typeref) ty = ty->variant.typeref.type;

  if (ty->size == 0) {
    out_info->abi_class = s390x_abi_class_t::no_class;
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_integer || ty->kind == tk_pointer) {
    out_info->abi_class = s390x_abi_class_t::integer;
    return llvm_type_from_edg_type(ty, &out_info->coerced_type);
  }

  if (ty->kind == tk_float) {
    out_info->abi_class = s390x_abi_class_t::float_class;
    return llvm_type_from_edg_type(ty, &out_info->coerced_type);
  }

  // SystemZ rules for structs: sizes 1, 2, 4, 8 passed in GPRs
  if (ty->size == 1 || ty->size == 2 || ty->size == 4 || ty->size == 8) {
    out_info->abi_class = s390x_abi_class_t::struct_coerced;
    out_info->coerced_type = llvm::Type::getIntNTy(*be_state->context, static_cast<unsigned>(ty->size * 8));
  } else {
    out_info->abi_class = s390x_abi_class_t::memory;
    out_info->is_indirect = true;
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes return value classification and sret requirement for SystemZ (s390x).
 * @details Determines whether the return value is returned in registers or via sret pointer.
 * @param[in] ret_ty The EDG return type.
 * @param[out] out_info Pointer to store the return classification info.
 * @param[out] out_sret Pointer to boolean set to true if sret is required.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t compute_s390x_return_info(
    a_type_ptr ret_ty,
    s390x_arg_info_t* out_info,
    bool* out_sret) noexcept {
  
  if (!ret_ty || !out_info || !out_sret) return llvm_gen_be_error_t::invalid_argument;

  *out_sret = false;
  llvm_gen_be_error_t err = classify_s390x_argument(ret_ty, out_info);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (out_info->abi_class == s390x_abi_class_t::memory) {
    out_info->requires_sret = true;
    *out_sret = true;
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Builds the LLVM FunctionType and AttributeList for a SystemZ (s390x) routine.
 * @details Applies SystemZ ABI coercion and sret/byval attributes.
 * @param[in] routine_ty The EDG routine type.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_s390x_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {
    
  if (!routine_ty || !out_fn_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;

  std::vector<llvm::Type*> llvm_param_types;
  std::vector<llvm::AttributeSet> param_attrs;
  llvm::AttrBuilder ret_attr{*be_state->context};

  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  llvm::Type* llvm_ret_ty = nullptr;

  s390x_arg_info_t ret_info;
  bool is_sret = false;
  llvm_gen_be_error_t err = compute_s390x_return_info(ret_ty, &ret_info, &is_sret);
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
    if (ret_info.abi_class == s390x_abi_class_t::no_class) {
      llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
    } else {
      llvm_ret_ty = ret_info.coerced_type;
    }
  }

  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
      
      s390x_arg_info_t arg_info;
      err = classify_s390x_argument(param->type, &arg_info);
      if (err != llvm_gen_be_error_t::ok) return err;

      if (arg_info.abi_class == s390x_abi_class_t::no_class) continue;

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
 * @brief Constructs the LLVM Type for a va_list under SystemZ (s390x) ABI.
 * @details The s390x va_list type is `[1 x { i64, i64, ptr, ptr }]` representing
 *          `__gpr`, `__fpr`, `__overflow_arg_area`, and `__reg_save_area`.
 * @param[out] out_ty Pointer to store the resulting LLVM ArrayType.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_ty` is null.
 */
llvm_gen_be_error_t build_s390x_va_list_type(
    llvm::Type** out_ty) noexcept {
  if (!out_ty) return llvm_gen_be_error_t::invalid_argument;
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::StructType* struct_ty = llvm::StructType::get(*be_state->context, {i64_ty, i64_ty, ptr_ty, ptr_ty});
  *out_ty = llvm::ArrayType::get(struct_ty, 1);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_start intrinsic for the SystemZ (s390x) ABI.
 * @details Invokes the llvm.va_start intrinsic on the va_list pointer.
 * @param[in] va_list_ptr Pointer to the va_list object.
 * @param[out] out_val Pointer to store the resulting CallInst.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_s390x_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
    
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_start_func = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vastart);
  
  *out_val = be_state->builder->CreateCall(va_start_func, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_arg operation for the SystemZ (s390x) ABI.
 * @details Emits an LLVM VAArgInst for the requested EDG type.
 * @param[in] va_list_ptr Pointer to the va_list object.
 * @param[in] ty The EDG type being extracted.
 * @param[out] out_val Pointer to store the extracted LLVM value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_s390x_va_arg(
    llvm::Value* va_list_ptr,
    a_type_ptr ty,
    llvm::Value** out_val) noexcept {
  
  if (!va_list_ptr || !ty || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Type* llvm_ty = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(ty, &llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  s390x_arg_info_t info = {};
  LLVM_GEN_BE_CHECK(classify_s390x_argument(ty, &info));
  if (info.is_indirect) {
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Value* ind_ptr = be_state->builder->CreateVAArg(va_list_ptr, ptr_ty, "vaarg.ind.ptr");
    *out_val = be_state->builder->CreateLoad(llvm_ty, ind_ptr, "vaarg.ind.val");
    return llvm_gen_be_error_t::ok;
  }

  if (skip_typerefs(ty)->alignment > 8) {
    *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty, "vaarg.aligned");
    return llvm_gen_be_error_t::ok;
  }

  *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_end intrinsic for the SystemZ (s390x) ABI.
 * @details Invokes the llvm.va_end intrinsic on the va_list pointer.
 * @param[in] va_list_ptr Pointer to the va_list object.
 * @param[out] out_val Pointer to store the resulting CallInst.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_s390x_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
  
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* va_end_func = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vaend);
  
  *out_val = be_state->builder->CreateCall(va_end_func, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_copy intrinsic for the SystemZ (s390x) ABI.
 * @details Invokes the llvm.va_copy intrinsic on destination and source va_list pointers.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list object.
 * @param[in] src_va_list_ptr Pointer to the source va_list object.
 * @param[out] out_val Pointer to store the resulting CallInst.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_s390x_va_copy(
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
