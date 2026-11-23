/**
 * @file llvm_gen_be_abi_wasm.cpp
 * @ingroup llvm_gen_be_abi
 * @brief WebAssembly ABI classification implementation.
 * @details Implements parameter passing and function type construction for WebAssembly (wasm32/wasm64).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_abi_wasm.h"
#include "llvm_gen_be_internal.h"
#include <llvm/IR/Intrinsics.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Recursively extracts a single scalar field type from a WebAssembly aggregate.
 * @details Under the WebAssembly Basic C ABI, a struct or class containing a single
 *          non-empty scalar field (recursively) is passed and returned directly as that scalar.
 * @param[in] ty The EDG type to inspect.
 * @param[out] out_scalar_ty Receives the unwrapped scalar `a_type_ptr`, or `nullptr` if composite.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t unwrap_wasm_single_scalar(
    a_type_ptr ty,
    a_type_ptr* out_scalar_ty) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_scalar_ty = nullptr;
  ty = skip_typerefs(ty);
  if (ty->kind == tk_integer || ty->kind == tk_float || ty->kind == tk_pointer) {
    *out_scalar_ty = ty;
    return llvm_gen_be_error_t::ok;
  }
  if (ty->kind == tk_struct || ty->kind == tk_class) {
    a_field_ptr single_field = nullptr;
    for (a_field_ptr f = ty->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
      if (f->is_bit_field || single_field != nullptr) {
        *out_scalar_ty = nullptr;
        return llvm_gen_be_error_t::ok;
      }
      single_field = f;
    }
    if (single_field != nullptr) {
      return unwrap_wasm_single_scalar(single_field->type, out_scalar_ty);
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Classifies an argument or return type according to WebAssembly ABI rules.
 * @details Direct scalar mapping (including single-element scalar structs) or indirect pointer
 *          passing for multi-element composite aggregates and unions.
 * @param[in] ty The EDG AST type pointer to classify.
 * @param[out] out_info Pointer to the wasm_arg_info_t structure receiving classification results.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
llvm_gen_be_error_t classify_wasm_argument(a_type_ptr ty, wasm_arg_info_t* out_info) noexcept {
  if (!ty || !out_info) return llvm_gen_be_error_t::invalid_argument;
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  ty = skip_typerefs(ty);
  
  out_info->size = static_cast<uint64_t>(ty->size);
  out_info->is_indirect = false;

  if (ty->kind == tk_class || ty->kind == tk_struct || ty->kind == tk_union) {
      a_type_ptr scalar_ty = nullptr;
      LLVM_GEN_BE_CHECK(unwrap_wasm_single_scalar(ty, &scalar_ty));
      if (scalar_ty == nullptr) {
          out_info->is_indirect = true;
      }
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Builds the LLVM FunctionType and attributes for a WebAssembly function.
 * @details Computes argument and return types and sets struct_ret for indirect composites.
 * @param[in] routine_ty The EDG routine AST type pointer.
 * @param[out] out_func_ty Receives the constructed LLVM FunctionType pointer on success.
 * @param[out] out_attrs Receives the constructed LLVM AttributeList on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_wasm_function_type(a_type_ptr routine_ty, llvm::FunctionType** out_func_ty, llvm::AttributeList* out_attrs) noexcept {
  if (!routine_ty || !out_func_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;
  
  a_routine_type_supplement_ptr supp = routine_ty->variant.routine.extra_info;
  a_type_ptr ret_edg_ty = routine_ty->variant.routine.return_type;
  
  llvm::Type* ret_llvm_ty = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(ret_edg_ty, &ret_llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  wasm_arg_info_t ret_info;
  LLVM_GEN_BE_CHECK(classify_wasm_argument(ret_edg_ty, &ret_info));
  
  llvm::Type* actual_ret_ty = ret_llvm_ty;
  llvm::AttrBuilder ret_attrs{*be_state->context};
  
  std::vector<llvm::Type*> arg_types;
  std::vector<llvm::AttributeSet> param_attrs;
  if (ret_info.is_indirect) {
      actual_ret_ty = llvm::Type::getVoidTy(*be_state->context);
      arg_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
      llvm::AttrBuilder sret_ab{*be_state->context};
      sret_ab.addStructRetAttr(ret_llvm_ty);
      param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  } else {
      a_type_ptr scalar_ret_ty = nullptr;
      LLVM_GEN_BE_CHECK(unwrap_wasm_single_scalar(ret_edg_ty, &scalar_ret_ty));
      if (scalar_ret_ty != nullptr) {
          LLVM_GEN_BE_CHECK(get_llvm_type(scalar_ret_ty, &actual_ret_ty));
      }
  }
  
  if (supp) {
    for (a_param_type_ptr p = supp->param_type_list; p; p = p->next) {
        if (p->type->kind == tk_void) break;
        llvm::Type* p_ty = nullptr;
        err = get_llvm_type(p->type, &p_ty);
        if (err != llvm_gen_be_error_t::ok) return err;
        
        wasm_arg_info_t p_info;
        LLVM_GEN_BE_CHECK(classify_wasm_argument(p->type, &p_info));
        
        if (p_info.is_indirect) {
            arg_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
            llvm::AttrBuilder ab{*be_state->context};
            ab.addByValAttr(p_ty);
            param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
        } else {
            a_type_ptr scalar_p_ty = nullptr;
            LLVM_GEN_BE_CHECK(unwrap_wasm_single_scalar(p->type, &scalar_p_ty));
            LLVM_GEN_BE_CHECK(get_llvm_type(scalar_p_ty, &p_ty));
            arg_types.push_back(p_ty);
            param_attrs.push_back(llvm::AttributeSet());
        }
    }
  }
  
  llvm::AttributeSet ret_set = llvm::AttributeSet::get(*be_state->context, ret_attrs);
  llvm::AttributeSet fn_set;
  *out_func_ty = llvm::FunctionType::get(actual_ret_ty, arg_types, supp ? supp->has_ellipsis : false);
  *out_attrs = llvm::AttributeList::get(*be_state->context, fn_set, ret_set, param_attrs);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_start intrinsic for the WebAssembly ABI.
 * @details Emits `llvm.va_start` on the provided `va_list` pointer.
 * @param[in] va_list_ptr Pointer to the `va_list` object.
 * @param[out] out_val Receives the resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_wasm_va_start(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Function* fn = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vastart);
  *out_val = be_state->builder->CreateCall(fn, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_arg operation for the WebAssembly ABI.
 * @details Emits an LLVM `va_arg` instruction for the requested EDG type.
 * @param[in] va_list_ptr Pointer to the `va_list` object.
 * @param[in] ty The EDG type being extracted.
 * @param[out] out_val Receives the extracted LLVM value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_wasm_va_arg(llvm::Value* va_list_ptr, a_type_ptr ty, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !ty || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Type* llvm_ty = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(ty, &llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  wasm_arg_info_t info = {};
  LLVM_GEN_BE_CHECK(classify_wasm_argument(ty, &info));
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
 * @brief Lowers a va_end intrinsic for the WebAssembly ABI.
 * @details Emits `llvm.va_end` on the provided `va_list` pointer.
 * @param[in] va_list_ptr Pointer to the `va_list` object.
 * @param[out] out_val Receives the resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_wasm_va_end(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Function* fn = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vaend);
  *out_val = be_state->builder->CreateCall(fn, {va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_copy intrinsic for the WebAssembly ABI.
 * @details Emits `llvm.va_copy` from `src_va_list_ptr` to `dest_va_list_ptr`.
 * @param[in] dest_va_list_ptr Pointer to the destination `va_list` object.
 * @param[in] src_va_list_ptr Pointer to the source `va_list` object.
 * @param[out] out_val Receives the resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_wasm_va_copy(llvm::Value* dest_va_list_ptr, llvm::Value* src_va_list_ptr, llvm::Value** out_val) noexcept {
  if (!dest_va_list_ptr || !src_va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Function* fn = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vacopy);
  *out_val = be_state->builder->CreateCall(fn, {dest_va_list_ptr, src_va_list_ptr});
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
