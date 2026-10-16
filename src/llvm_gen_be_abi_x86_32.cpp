/**
 * @file llvm_gen_be_abi_x86_32.cpp
 * @brief x86 32-bit ABI classification implementation.
 * @details Implements parameter passing and function type construction for x86 32-bit.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_abi_x86_32.h"
#include "llvm_gen_be_internal.h"
#include <llvm/TargetParser/Triple.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Classifies an argument or return type according to x86 32-bit ABI rules.
 * @details Inspects the type and determines register allocation or stack passing requirements.
 * @param[in] ty The EDG AST type pointer to classify.
 * @param[out] out_info Pointer to the x86_32_arg_info_t structure receiving classification results.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
llvm_gen_be_error_t classify_x86_32_argument(a_type_ptr ty, x86_32_arg_info_t* out_info) noexcept {
  if (!ty || !out_info) return llvm_gen_be_error_t::invalid_argument;
  ty = skip_typerefs(ty);
  
  out_info->size = ty->size;
  out_info->is_indirect = false;
  out_info->in_reg = false;
  out_info->coerced_type = nullptr;

  if (ty->kind == tk_class || ty->kind == tk_struct || ty->kind == tk_union) {
      // MSVC and GCC have different rules for struct return/passing. 
      // For passing: MSVC passes structs by value if sizes are 1,2,4,8 bytes.
      // GCC passes structs by value, but arrays and complex might be coerced.
      // For simplicity, we implement generic 32-bit struct passing.
      if (ty->size > 8) {
          out_info->is_indirect = true;
      } else {
          out_info->coerced_type = llvm::Type::getIntNTy(*be_state->context, ty->size * 8);
      }
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Builds the LLVM FunctionType and attributes for an x86 32-bit function.
 * @details Computes argument and return coercions and adds struct_ret attributes when indirect.
 * @param[in] routine_ty The EDG routine AST type pointer.
 * @param[out] out_func_ty Receives the constructed LLVM FunctionType pointer on success.
 * @param[out] out_attrs Receives the constructed LLVM AttributeList on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_x86_32_function_type(a_type_ptr routine_ty, llvm::FunctionType** out_func_ty, llvm::AttributeList* out_attrs) noexcept {
  if (!routine_ty || !out_func_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;
  
  a_routine_type_supplement_ptr supp = routine_ty->variant.routine.extra_info;
  
  llvm::Type* ret_llvm_ty = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(routine_ty->variant.routine.return_type, &ret_llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  x86_32_arg_info_t ret_info;
  err = classify_x86_32_argument(routine_ty->variant.routine.return_type, &ret_info);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  llvm::Type* actual_ret_ty = ret_llvm_ty;
  llvm::AttrBuilder ret_attrs(*be_state->context);
  
  std::vector<llvm::Type*> arg_types;
  std::vector<llvm::AttributeSet> param_attrs;

  bool is_msvc = llvm::Triple(be_state->module->getTargetTriple()).isOSMSVCRT() || 
                 llvm::Triple(be_state->module->getTargetTriple()).isOSWindows();
  
  a_calling_convention call_conv = supp ? supp->calling_convention : cc_default;
  bool is_thiscall = (call_conv == cc_thiscall);
  bool is_fastcall = (call_conv == cc_fastcall);

  if (ret_info.is_indirect) {
      actual_ret_ty = llvm::Type::getVoidTy(*be_state->context);
      arg_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
      llvm::AttrBuilder sret_ab(*be_state->context);
      sret_ab.addStructRetAttr(ret_llvm_ty);
      if (is_msvc && is_thiscall) {
          sret_ab.addAttribute(llvm::Attribute::InReg);
      }
      param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  } else if (ret_info.coerced_type) {
      actual_ret_ty = ret_info.coerced_type;
  }
  
  if (supp) {
    uint32_t fastcall_regs = 0;
    uint32_t arg_idx = 0;

    for (a_param_type_ptr p = supp->param_type_list; p; p = p->next, arg_idx++) {
        if (p->type->kind == tk_void) break;
        llvm::Type* p_ty = nullptr;
        err = get_llvm_type(p->type, &p_ty);
        if (err != llvm_gen_be_error_t::ok) return err;
        
        x86_32_arg_info_t p_info;
        err = classify_x86_32_argument(p->type, &p_info);
        if (err != llvm_gen_be_error_t::ok) return err;
        
        llvm::AttrBuilder ab(*be_state->context);

        if (is_thiscall && arg_idx == 0 && !p_info.is_indirect) {
            ab.addAttribute(llvm::Attribute::InReg);
        } else if (is_fastcall && fastcall_regs < 2 && !p_info.is_indirect && p_info.size <= 4) {
            if (p->type->kind == tk_integer || p->type->kind == tk_pointer) {
                ab.addAttribute(llvm::Attribute::InReg);
                fastcall_regs++;
            }
        }

        if (p_info.in_reg) {
            ab.addAttribute(llvm::Attribute::InReg);
        }

        if (p_info.is_indirect) {
            arg_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
            ab.addByValAttr(p_ty);
        } else if (p_info.coerced_type) {
            arg_types.push_back(p_info.coerced_type);
        } else {
            arg_types.push_back(p_ty);
        }

        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
    }
  }
  
  *out_func_ty = llvm::FunctionType::get(actual_ret_ty, arg_types, supp ? supp->has_ellipsis : false);
  *out_attrs = llvm::AttributeList::get(*be_state->context, 
                                        llvm::AttributeSet(), 
                                        llvm::AttributeSet::get(*be_state->context, ret_attrs), 
                                        param_attrs);
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
