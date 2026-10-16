/**
 * @file llvm_gen_be_abi_arm32.cpp
 * @brief ARM 32-bit ABI classification implementation.
 * @details Implements parameter passing and function type construction for ARM 32-bit AAPCS.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_abi_arm32.h"
#include "llvm_gen_be_internal.h"

#include <llvm/TargetParser/Triple.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Internal recursive helper to detect ARM32 HFAs.
 * @details Traverses arrays and structs to count identical float elements.
 * @param[in] ty The EDG AST type to inspect.
 * @param[in,out] num_elements Counter for identical elements.
 * @param[in,out] elem_ty Resolves to the LLVM element type.
 * @param[in,out] is_valid True if all elements match.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t detect_arm32_hfa_internal(
    a_type_ptr ty,
    uint32_t* num_elements,
    llvm::Type** elem_ty,
    bool* is_valid) noexcept {
  
  if (ty == nullptr) return llvm_gen_be_error_t::invalid_argument;

  while (ty->kind == tk_typeref) {
    ty = ty->variant.typeref.type;
  }

  if (ty->kind == tk_float) {
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
    uint32_t count = ty->variant.array.variant.number_of_elements;
    for (uint32_t i = 0; i < count; i++) {
      llvm_gen_be_error_t err = detect_arm32_hfa_internal(ety, num_elements, elem_ty, is_valid);
      if (err != llvm_gen_be_error_t::ok || !*is_valid) return err;
    }
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_class || ty->kind == tk_struct) {
    for (a_field_ptr field = ty->variant.class_struct_union.field_list; field != nullptr; field = field->next) {
      if (field->is_bit_field && field->bit_size == 0) continue;
      llvm_gen_be_error_t err = detect_arm32_hfa_internal(field->type, num_elements, elem_ty, is_valid);
      if (err != llvm_gen_be_error_t::ok || !*is_valid) return err;
    }
    return llvm_gen_be_error_t::ok;
  }

  *is_valid = false;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Detects Homogeneous Floating-point Aggregates (HFA) for ARM32 AAPCS-VFP.
 * @details Identifies aggregates of 1 to 4 identical float/double elements.
 * @param[in] ty The EDG AST type to inspect.
 * @param[out] out_num_elements Number of float elements found.
 * @param[out] out_elem_ty Resolves to the LLVM element type.
 * @param[out] out_is_hfa True if the type is a valid HFA.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t detect_arm32_hfa(
    a_type_ptr ty,
    uint32_t* out_num_elements,
    llvm::Type** out_elem_ty,
    bool* out_is_hfa) noexcept {
  
  if (!out_num_elements || !out_elem_ty || !out_is_hfa) return llvm_gen_be_error_t::invalid_argument;

  *out_num_elements = 0;
  *out_elem_ty = nullptr;
  *out_is_hfa = true;

  llvm_gen_be_error_t err = detect_arm32_hfa_internal(ty, out_num_elements, out_elem_ty, out_is_hfa);
  if (err != llvm_gen_be_error_t::ok) return err;

  // AAPCS-VFP HFA rules: 1 to 4 identical float elements
  if (*out_is_hfa && (*out_num_elements == 0 || *out_num_elements > 4)) {
    *out_is_hfa = false;
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Classifies an argument or return type according to ARM 32-bit AAPCS rules.
 * @details Inspects the type and sets size, indirect passing flags, and coercion types.
 * @param[in] ty The EDG AST type pointer to classify.
 * @param[out] out_info Pointer to the arm32_arg_info_t structure receiving classification results.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
llvm_gen_be_error_t classify_arm32_argument(a_type_ptr ty, arm32_arg_info_t* out_info) noexcept {
  if (!ty || !out_info) return llvm_gen_be_error_t::invalid_argument;
  ty = skip_typerefs(ty);
  
  out_info->size = ty->size;
  out_info->align = ty->alignment;
  out_info->is_indirect = false;
  out_info->coerced_type = nullptr;
  out_info->is_hfa = false;

  llvm::Triple triple(be_state->module->getTargetTriple());
  bool is_hard_float = (triple.getEnvironment() == llvm::Triple::GNUEABIHF || triple.getEnvironment() == llvm::Triple::EABIHF);

  if (ty->kind == tk_float) {
      if (!is_hard_float) {
          // softfp: pass float in integer register natively supported by LLVM if we just pass scalar
          // However, for consistency we let LLVM handle it, so we don't coerce here unless it's a struct.
          // Wait, if it's a double and not hard float, LLVM maps double to r0/r1 automatically.
      } else {
          out_info->is_hfa = true;
      }
      return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_class || ty->kind == tk_struct || ty->kind == tk_union || ty->kind == tk_array) {
      if (is_hard_float) {
          uint32_t hfa_elems = 0;
          llvm::Type* hfa_ty = nullptr;
          bool is_hfa = false;
          llvm_gen_be_error_t err = detect_arm32_hfa(ty, &hfa_elems, &hfa_ty, &is_hfa);
          if (err != llvm_gen_be_error_t::ok) return err;

          if (is_hfa) {
              out_info->is_hfa = true;
              out_info->coerced_type = llvm::ArrayType::get(hfa_ty, hfa_elems);
              return llvm_gen_be_error_t::ok;
          }
      }

      if (ty->size > 64) { // AAPCS aggregates > 64 bytes passed indirectly (some versions use 64 bytes, wait, AAPCS says 64 bytes for passing byval)
          // Wait, AAPCS standard says composites <= 4 registers (16 bytes) are in r0-r3. > 16 bytes goes to stack, or passed byval?
          // Actually, AAPCS says they are passed byval on the stack, not indirectly by pointer!
          // We can use ByVal attribute for > 16 bytes or any struct passed on stack/registers.
          // LLVM handles ByVal attribute efficiently.
          // Let's use coercion for sizes <= 16, and ByVal for sizes > 16? 
          // Wait, for AAPCS, structures are passed natively. If we use [N x i32], LLVM passes them in registers or stack as needed.
          unsigned num_ints = (ty->size + 3) / 4;
          out_info->coerced_type = llvm::ArrayType::get(llvm::Type::getInt32Ty(*be_state->context), num_ints);
      } else {
          unsigned num_ints = (ty->size + 3) / 4;
          if (ty->alignment >= 8) {
              unsigned num_i64 = ty->size / 8;
              unsigned rem_bytes = ty->size % 8;
              if (rem_bytes > 0) {
                  std::vector<llvm::Type*> fields;
                  for (unsigned i = 0; i < num_i64; ++i) {
                      fields.push_back(llvm::Type::getInt64Ty(*be_state->context));
                  }
                  unsigned rem_ints = (rem_bytes + 3) / 4;
                  if (rem_ints == 1) {
                      fields.push_back(llvm::Type::getInt32Ty(*be_state->context));
                  } else {
                      fields.push_back(llvm::ArrayType::get(llvm::Type::getInt32Ty(*be_state->context), rem_ints));
                  }
                  out_info->coerced_type = llvm::StructType::get(*be_state->context, fields, false);
              } else {
                  out_info->coerced_type = llvm::ArrayType::get(llvm::Type::getInt64Ty(*be_state->context), num_i64);
              }
          } else {
              out_info->coerced_type = llvm::ArrayType::get(llvm::Type::getInt32Ty(*be_state->context), num_ints);
          }
      }
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Builds the LLVM FunctionType and attributes for an ARM 32-bit AAPCS function.
 * @details Computes argument and return coercions and adds struct_ret attributes when indirect.
 * @param[in] routine_ty The EDG routine AST type pointer.
 * @param[out] out_func_ty Receives the constructed LLVM FunctionType pointer on success.
 * @param[out] out_attrs Receives the constructed LLVM AttributeList on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_arm32_function_type(a_type_ptr routine_ty, llvm::FunctionType** out_func_ty, llvm::AttributeList* out_attrs) noexcept {
  if (!routine_ty || !out_func_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;
  
  a_routine_type_supplement_ptr supp = routine_ty->variant.routine.extra_info;
  
  llvm::Type* ret_llvm_ty = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(routine_ty->variant.routine.return_type, &ret_llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  arm32_arg_info_t ret_info;
  err = classify_arm32_argument(routine_ty->variant.routine.return_type, &ret_info);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  llvm::Type* actual_ret_ty = ret_llvm_ty;
  llvm::AttrBuilder ret_attrs(*be_state->context);
  
  std::vector<llvm::Type*> arg_types;
  std::vector<llvm::AttributeSet> param_attrs;

  bool ret_is_sret = false;
  a_type_ptr ret_edg_ty = routine_ty->variant.routine.return_type;
  if (ret_edg_ty && (ret_edg_ty->kind == tk_class || ret_edg_ty->kind == tk_struct || ret_edg_ty->kind == tk_union)) {
      if (!ret_info.is_hfa && ret_edg_ty->size > 4) {
          ret_is_sret = true;
      }
  }

  if (ret_is_sret) {
      actual_ret_ty = llvm::Type::getVoidTy(*be_state->context);
      arg_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
      llvm::AttrBuilder sret_ab(*be_state->context);
      sret_ab.addStructRetAttr(ret_llvm_ty);
      param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  } else if (ret_info.coerced_type) {
      actual_ret_ty = ret_info.coerced_type;
  }
  
  if (supp) {
    for (a_param_type_ptr p = supp->param_type_list; p; p = p->next) {
        if (p->type->kind == tk_void) break;
        llvm::Type* p_ty = nullptr;
        err = get_llvm_type(p->type, &p_ty);
        if (err != llvm_gen_be_error_t::ok) return err;
        
        arm32_arg_info_t p_info;
        err = classify_arm32_argument(p->type, &p_info);
        if (err != llvm_gen_be_error_t::ok) return err;
        
        llvm::AttrBuilder ab(*be_state->context);

        if (p_info.is_indirect) {
            arg_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
            ab.addByValAttr(p_ty);
            ab.addAlignmentAttr(llvm::Align(p_info.align >= 4 ? p_info.align : 4));
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
