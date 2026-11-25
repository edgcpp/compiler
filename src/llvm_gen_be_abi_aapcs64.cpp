/**
 * @file llvm_gen_be_abi_aapcs64.cpp
 * @ingroup llvm_gen_be_abi
 * @brief ARM64 AAPCS64 classification implementation.
 * @details Implements the parameter passing and return value classification
 * rules specified in the Procedure Call Standard for the Arm 64-bit Architecture (AAPCS64).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_internal.h"
#include "target.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Internal recursive helper for HFA/HVA detection.
 * @details Traverses struct fields and arrays to count the number of identical
 * float or vector elements. Sets is_valid to false if heterogeneous elements are found.
 * @param[in] ty The EDG type to inspect.
 * @param[in,out] num_elements Counter for the number of homogeneous elements.
 * @param[in,out] elem_ty The resolved LLVM type of the elements.
 * @param[in,out] is_valid Boolean flag indicating if the composite remains homogeneous.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t detect_hfa_hva_internal(
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

#if GNU_VECTOR_TYPES_ALLOWED
    if (ty->kind == tk_vector) {
       // Only vector types up to 16 bytes (128 bits) are valid for HVA
       if (ty->size > 16) {
         *is_valid = false;
         return llvm_gen_be_error_t::ok;
       }
    }
#endif

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
    for (uint32_t i = 0; i < count; i++) {
      llvm_gen_be_error_t err = detect_hfa_hva_internal(ety, num_elements, elem_ty, is_valid);
      if (err != llvm_gen_be_error_t::ok || !*is_valid) return err;
    }
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_struct || ty->kind == tk_class) {
    for (a_field_ptr field = ty->variant.class_struct_union.field_list; field != nullptr; field = field->next) {
      if (field->is_bit_field && field->bit_size == 0) continue; // skip zero-length bitfields
      llvm_gen_be_error_t err = detect_hfa_hva_internal(field->type, num_elements, elem_ty, is_valid);
      if (err != llvm_gen_be_error_t::ok || !*is_valid) return err;
    }
    return llvm_gen_be_error_t::ok;
  }

  // Not a float, array, or struct/class, so it can't be part of an HFA/HVA
  *is_valid = false;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Detects if a type is a Homogeneous Floating-point or Vector Aggregate (HFA/HVA).
 * @details Traverses struct fields and array elements to determine if the composite
 * consists solely of 1 to 4 identical floating-point or vector types.
 * @param[in] ty The EDG type to inspect.
 * @param[out] out_num_elements Number of homogeneous elements found.
 * @param[out] out_elem_ty The LLVM type of the homogeneous element.
 * @param[out] out_is_hfa True if the type qualifies as HFA or HVA.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t detect_hfa_hva(
    a_type_ptr ty,
    uint32_t* out_num_elements,
    llvm::Type** out_elem_ty,
    bool* out_is_hfa) noexcept {
  
  if (!out_num_elements || !out_elem_ty || !out_is_hfa) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  *out_num_elements = 0;
  *out_elem_ty = nullptr;
  *out_is_hfa = true; // Assume true until proven false

  llvm_gen_be_error_t err = detect_hfa_hva_internal(ty, out_num_elements, out_elem_ty, out_is_hfa);
  if (err != llvm_gen_be_error_t::ok) return err;

  // AAPCS64 HFA/HVA rules: 1 to 4 identical elements
  if (*out_is_hfa && (*out_num_elements == 0 || *out_num_elements > 4)) {
    *out_is_hfa = false;
  }

  return llvm_gen_be_error_t::ok;
}


/**
 * @brief Classifies a single EDG type into AAPCS64 ABI argument passing rules.
 * @details Evaluates scalars, arrays, pointers, and aggregates to determine 
 * register or memory allocation according to ARM64 rules.
 * @param[in] ty The EDG type to classify.
 * @param[out] out_info The classification result information structure.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_aapcs64_argument(
    a_type_ptr ty,
    aapcs64_arg_info_t* out_info) noexcept {
  
  if (!ty || !out_info) return llvm_gen_be_error_t::invalid_argument;

  out_info->abi_class = aapcs64_abi_class_t::integer; // default
  out_info->hfa_hva_elem_ty = nullptr;
  out_info->num_elements = 1;
  out_info->is_sret = false;
  out_info->pass_in_memory = false;
  out_info->pass_by_invisible_reference = false;

  while (ty->kind == tk_typeref) {
    ty = ty->variant.typeref.type;
  }

  uint64_t size = static_cast<uint64_t>(ty->size);

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  bool is_apple = triple.isOSDarwin();

#if GNU_VECTOR_TYPES_ALLOWED
  if (ty->kind == tk_scalable_vector || ty->kind == tk_scalable_vector_count) {
    out_info->abi_class = aapcs64_abi_class_t::float_vector;
    out_info->num_elements = 1;
    return llvm_gen_be_error_t::ok;
  }
#endif

  bool is_empty = false;
  if (size == 0) {
    is_empty = true;
  } else if (ty->kind == tk_class || ty->kind == tk_struct) {
    if (is_empty_class_type(ty)) {
      is_empty = true;
    }
  }

  // Darwin overrides the psABI to ignore all empty records in all modes (C and C++).
  // AAPCS64 ignores empty records in C (size 0) but not C++ (size 1).
  if (is_empty && (is_apple || size == 0)) {
    out_info->abi_class = aapcs64_abi_class_t::no_class;
    out_info->num_elements = 0;
    return llvm_gen_be_error_t::ok;
  }

  // Non-trivial C++ types are passed by invisible reference.
  if ((ty->kind == tk_class || ty->kind == tk_struct) && !is_trivially_copyable_type(ty)) {
    out_info->abi_class = aapcs64_abi_class_t::reference;
    out_info->num_elements = 1;
    out_info->pass_in_memory = false;
    out_info->pass_by_invisible_reference = true;
    return llvm_gen_be_error_t::ok;
  }

  if ((ty->kind == tk_integer && size <= 16) ||
      ty->kind == tk_pointer || ty->kind == tk_nullptr || ty->kind == tk_ptr_to_member) {
    out_info->abi_class = aapcs64_abi_class_t::integer;
    out_info->num_elements = (size > 8) ? 2 : 1; // 128-bit ints take two registers
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_float
#if GNU_VECTOR_TYPES_ALLOWED
      || (ty->kind == tk_vector && size <= 16)
#endif
      ) {
    out_info->abi_class = aapcs64_abi_class_t::float_vector;
    out_info->num_elements = 1;
    return llvm_gen_be_error_t::ok;
  }

  // Composite types
  bool is_hfa = false;
  uint32_t hfa_elems = 0;
  llvm::Type* hfa_ty = nullptr;

  llvm_gen_be_error_t err = detect_hfa_hva(ty, &hfa_elems, &hfa_ty, &is_hfa);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (is_hfa) {
    out_info->abi_class = aapcs64_abi_class_t::hfa_hva;
    out_info->hfa_hva_elem_ty = hfa_ty;
    out_info->num_elements = hfa_elems;
    return llvm_gen_be_error_t::ok;
  }

  // If size > 16 bytes, passed by reference (indirect copy)
  if (size > 16) {
    out_info->abi_class = aapcs64_abi_class_t::reference;
    out_info->num_elements = 1; // 1 pointer register
    out_info->pass_in_memory = false; // The argument passed is a pointer, but we usually track the pointer in reg.
    return llvm_gen_be_error_t::ok;
  }

  // Size 1-16 bytes passed in 1 or 2 integer registers
  out_info->abi_class = aapcs64_abi_class_t::integer;
  out_info->num_elements = static_cast<uint32_t>((size + 7) / 8); 
  return llvm_gen_be_error_t::ok;
}


/**
 * @brief Computes ARM64 AAPCS64 return value passing information.
 * @details Evaluates the return type according to AAPCS64 return rules. 
 * Determines if the type requires an sret pointer, or if it can be returned 
 * in registers.
 * @param[in] ret_ty The EDG type representing the function return type.
 * @param[out] out_info The classification result information structure.
 * @param[out] out_sret Pointer to a boolean set to true if the return is via sret.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t compute_aapcs64_return_info(
    a_type_ptr ret_ty,
    aapcs64_arg_info_t* out_info,
    bool* out_sret) noexcept {

  if (!ret_ty || !out_info || !out_sret) return llvm_gen_be_error_t::invalid_argument;

  a_type_ptr raw_ret = skip_typerefs(ret_ty);
  if (raw_ret->kind != tk_void
#if GNU_VECTOR_TYPES_ALLOWED
      && raw_ret->kind != tk_scalable_vector && raw_ret->kind != tk_scalable_vector_count
#endif
      ) {
    bool is_empty = false;
    if (raw_ret->size == 0) {
      is_empty = true;
    } else if (raw_ret->kind == tk_class || raw_ret->kind == tk_struct) {
      if (is_empty_class_type(raw_ret)) {
        is_empty = true;
      }
    }
    // AAPCS64 return rules: ALL empty records (C and C++) are ignored (returned as void).
    if (is_empty) {
      out_info->abi_class = aapcs64_abi_class_t::no_class;
      out_info->num_elements = 0;
      out_info->is_sret = false;
      out_info->pass_in_memory = false;
      *out_sret = false;
      return llvm_gen_be_error_t::ok;
    }
  }

  llvm_gen_be_error_t err = classify_aapcs64_argument(ret_ty, out_info);
  if (err != llvm_gen_be_error_t::ok) return err;

  // AAPCS64 return rules:
  // - Types > 16 bytes are returned via a hidden sret pointer (X8 register usually).
  // - Or if it's a C++ non-trivial class, also sret (caller allocates).
  // Note: the classification engine sets abi_class to `reference` if > 16 bytes.
  
  if (out_info->abi_class == aapcs64_abi_class_t::reference) {
    *out_sret = true;
    out_info->is_sret = true;
  } else {
    *out_sret = false;
    out_info->is_sret = false;
  }

  return llvm_gen_be_error_t::ok;
}


/**
 * @brief Constructs an LLVM FunctionType and AttributeList matching AAPCS64.
 * @details Evaluates function parameters and return type against the AAPCS64 classification engine.
 * Determines sret, byval/indirect passes, and register allocation. Tracks available General Purpose
 * (X0-X7) and Floating Point (V0-V7) registers.
 * @param[in] routine_ty The EDG routine type structure.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_aapcs64_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {

  if (!routine_ty || !out_fn_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;

  uint32_t x_regs = 0; // max 8
  uint32_t v_regs = 0; // max 8

  std::vector<llvm::Type*> llvm_param_types;
  llvm::AttrBuilder ret_attr{*be_state->context};
  std::vector<llvm::AttributeSet> param_attrs;

  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  bool edg_ret_as_param = false;
#if DO_IL_LOWERING
  if (routine_ty->variant.routine.extra_info != nullptr &&
      routine_ty->variant.routine.extra_info->value_returned_as_parameter) {
    edg_ret_as_param = true;
  }
#endif
  
  aapcs64_arg_info_t ret_info;
  bool sret = false;
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;
  if (edg_ret_as_param) {
    ret_info.abi_class = aapcs64_abi_class_t::no_class;
  } else {
    err = compute_aapcs64_return_info(ret_ty, &ret_info, &sret);
    if (err != llvm_gen_be_error_t::ok) return err;
  }

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  bool is_apple = triple.isOSDarwin();
  bool is_windows = triple.isOSWindows();
  bool is_vararg = (routine_ty->variant.routine.extra_info != nullptr)
                       ? routine_ty->variant.routine.extra_info->has_ellipsis
                       : false;
  bool is_win_vararg = is_windows && is_vararg;

  llvm::Type* llvm_ret_ty = nullptr;

  if (sret) {
    llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
    llvm::Type* sret_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm_param_types.push_back(sret_ptr_ty);

    llvm::AttrBuilder sret_ab{*be_state->context};
    llvm::Type* sret_pointee_ty = nullptr;
    err = llvm_type_from_edg_type(ret_ty, &sret_pointee_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    sret_ab.addStructRetAttr(sret_pointee_ty);
    // On AAPCS64, XR8 holds the indirect result location and does NOT consume x0-x7,
    // except on Windows ARM64 instance methods where sret is passed in x1 (consuming 1 GPR).
    a_routine_type_supplement_ptr r_extra = routine_ty->variant.routine.extra_info;
    if (is_windows && r_extra->this_class != nullptr) {
      x_regs++;
      sret_ab.addAttribute(llvm::Attribute::InReg);
    }
    param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  } else if (ret_info.abi_class == aapcs64_abi_class_t::no_class) {
    llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
  } else {
    err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
    if (err != llvm_gen_be_error_t::ok) return err;

    a_type_ptr raw_ret_ty = skip_typerefs(ret_ty);
    bool ret_is_agg = is_class_struct_union_type(raw_ret_ty) || raw_ret_ty->kind == tk_array;
    if (ret_info.abi_class == aapcs64_abi_class_t::hfa_hva && !is_win_vararg) {
      llvm_ret_ty = llvm::ArrayType::get(ret_info.hfa_hva_elem_ty, ret_info.num_elements);
    } else if (ret_is_agg) {
      uint32_t num_i64 = static_cast<uint32_t>((raw_ret_ty->size + 7) / 8);
      llvm_ret_ty = (num_i64 <= 1)
          ? static_cast<llvm::Type*>(llvm::Type::getInt64Ty(*be_state->context))
          : static_cast<llvm::Type*>(llvm::ArrayType::get(llvm::Type::getInt64Ty(*be_state->context), num_i64));
    }

    if (is_apple && !ret_is_agg && ret_info.abi_class == aapcs64_abi_class_t::integer) {
      if (raw_ret_ty->size < 4) {
         if (int_type_is_signed(raw_ret_ty)) ret_attr.addAttribute(llvm::Attribute::SExt);
         else ret_attr.addAttribute(llvm::Attribute::ZExt);
      }
    }
  }

  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
      
      aapcs64_arg_info_t arg_info;
      err = classify_aapcs64_argument(param->type, &arg_info);
      if (err != llvm_gen_be_error_t::ok) return err;

      llvm::Type* pty = nullptr;
      err = llvm_type_from_edg_type(param->type, &pty);
      if (err != llvm_gen_be_error_t::ok) return err;

      llvm::AttrBuilder ab{*be_state->context};
      a_type_ptr raw_param_ty = skip_typerefs(param->type);
#if DO_IL_LOWERING
      if (param == routine_ty->variant.routine.extra_info->param_type_list &&
          !sret &&
          routine_ty->variant.routine.extra_info->value_returned_as_parameter &&
          raw_param_ty->kind == tk_pointer &&
          raw_param_ty->variant.pointer.type != nullptr) {
        llvm::Type* sret_ptee_ty = nullptr;
        LLVM_GEN_BE_CHECK(llvm_type_from_edg_type(raw_param_ty->variant.pointer.type, &sret_ptee_ty));
        ab.addStructRetAttr(sret_ptee_ty);
        if (is_windows && routine_ty->variant.routine.extra_info->this_class != nullptr) {
          x_regs++;
          ab.addAttribute(llvm::Attribute::InReg);
        }
        llvm_param_types.push_back(pty);
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
        continue;
      }
#endif
      bool param_is_agg = is_class_struct_union_type(raw_param_ty) || raw_param_ty->kind == tk_array;

      if (is_win_vararg && arg_info.abi_class == aapcs64_abi_class_t::hfa_hva) {
        if (raw_param_ty->size > 16) {
          arg_info.abi_class = aapcs64_abi_class_t::reference;
        } else {
          arg_info.abi_class = aapcs64_abi_class_t::integer;
          arg_info.num_elements = static_cast<uint32_t>((raw_param_ty->size + 7) / 8);
        }
      }

      if (is_apple && !param_is_agg && arg_info.abi_class == aapcs64_abi_class_t::integer && raw_param_ty->size < 4) {
         if (int_type_is_signed(raw_param_ty)) ab.addAttribute(llvm::Attribute::SExt);
         else ab.addAttribute(llvm::Attribute::ZExt);
      }

      if (arg_info.abi_class == aapcs64_abi_class_t::no_class) {
        // Ignored by ABI (e.g., Apple Darwin empty structs). Pass as size 0 LLVM struct {}.
        llvm_param_types.push_back(llvm::StructType::get(*be_state->context));
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
      } else if (arg_info.abi_class == aapcs64_abi_class_t::reference) {
        // Passed indirectly by address (caller-allocated copy; plain pointer without byval)
        llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
        if (x_regs < 8) x_regs++;
      } else if (arg_info.abi_class == aapcs64_abi_class_t::hfa_hva) {
        if (v_regs + arg_info.num_elements <= 8) {
          v_regs += arg_info.num_elements;
        } else {
          v_regs = 8;
        }
        llvm::Type* hfa_pty = llvm::ArrayType::get(arg_info.hfa_hva_elem_ty, arg_info.num_elements);
        llvm_param_types.push_back(hfa_pty);
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
      } else if (arg_info.abi_class == aapcs64_abi_class_t::float_vector) {
        if (is_win_vararg) {
          if (x_regs < 8) x_regs++;
          llvm_param_types.push_back(llvm::Type::getInt64Ty(*be_state->context));
        } else {
          if (v_regs < 8) v_regs++;
          llvm_param_types.push_back(pty);
        }
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
      } else { // integer
        if (x_regs + arg_info.num_elements <= 8) {
          x_regs += arg_info.num_elements;
        } else {
          x_regs = 8;
        }
        llvm::Type* int_pty = pty;
        if (param_is_agg) {
          int_pty = (arg_info.num_elements == 1)
              ? static_cast<llvm::Type*>(llvm::Type::getInt64Ty(*be_state->context))
              : static_cast<llvm::Type*>(llvm::ArrayType::get(llvm::Type::getInt64Ty(*be_state->context), arg_info.num_elements));
        }
        llvm_param_types.push_back(int_pty);
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
      }
    }
  }

  *out_fn_ty = llvm::FunctionType::get(llvm_ret_ty, llvm_param_types, is_vararg);
  
  llvm::AttributeSet ret_set = llvm::AttributeSet::get(*be_state->context, ret_attr);
  llvm::AttributeSet fn_set;
  
  *out_attrs = llvm::AttributeList::get(*be_state->context, fn_set, ret_set, param_attrs);

  return llvm_gen_be_error_t::ok;
}


#include <llvm/IR/Intrinsics.h>

/**
 * @brief Constructs the LLVM Type for a va_list under AAPCS64 ABI.
 * @details For Darwin, va_list is just a pointer (char*). For ELF/Linux, it's a struct 
 * [1 x { i8*, i8*, i8*, i32, i32 }].
 * @param[out] out_ty Pointer to store the resulting LLVM ArrayType or PointerType.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_aapcs64_va_list_type(
    llvm::Type** out_ty) noexcept {
  if (out_ty == nullptr) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  if (triple.isOSDarwin() || triple.isOSWindows()) {
    *out_ty = llvm::PointerType::getUnqual(*be_state->context);
  } else {
    llvm::Type* i8_ptr = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
    llvm::StructType* struct_ty = llvm::StructType::get(*be_state->context, {i8_ptr, i8_ptr, i8_ptr, i32_ty, i32_ty});
    *out_ty = llvm::ArrayType::get(struct_ty, 1);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_start intrinsic for the AAPCS64 ABI.
 * @details Casts the va_list pointer to an i8* and invokes the llvm.va_start intrinsic.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_aapcs64_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  llvm::Function* vastart_fn = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vastart);
  llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vastart_fn, {cast_ptr});
  
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_arg macro expansion for the AAPCS64 ABI.
 * @details Reads arguments from the va_list structure. Uses LLVM's va_arg instruction
 * which is supported for AAPCS64 in the LLVM backend.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[in] ty The EDG type being requested.
 * @param[out] out_val The resulting loaded value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_aapcs64_va_arg(
    llvm::Value* va_list_ptr,
    a_type_ptr ty,
    llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !ty || !out_val) return llvm_gen_be_error_t::invalid_argument;

  aapcs64_arg_info_t arg_info;
  llvm_gen_be_error_t err = classify_aapcs64_argument(ty, &arg_info);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Type* llvm_ty = nullptr;
  err = llvm_type_from_edg_type(ty, &llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  a_type_ptr raw_ty = skip_typerefs(ty);
  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  llvm::BasicBlock* cur_bb = be_state->builder->GetInsertBlock();
  llvm::Function* cur_fn = cur_bb->getParent();

  const bool is_indirect = (arg_info.abi_class == aapcs64_abi_class_t::reference);
  if (is_indirect) {
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    if (!cur_fn) {
      llvm::Value* ind_ptr = be_state->builder->CreateVAArg(va_list_ptr, ptr_ty, "vaarg.ind.ptr");
      uint64_t align_bytes = std::max<uint64_t>(1, static_cast<uint64_t>(raw_ty->alignment));
      *out_val = be_state->builder->CreateAlignedLoad(
          llvm_ty, ind_ptr, llvm::MaybeAlign(align_bytes), "vaarg.ind.val");
      return llvm_gen_be_error_t::ok;
    }
    if (triple.isOSDarwin() || triple.isOSWindows()) {
      llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
      llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
      llvm::Value* ap_cur = be_state->builder->CreateLoad(ptr_ty, va_list_ptr, "vaarg.ap");
      llvm::Value* ap_next = be_state->builder->CreateGEP(
          i8_ty, ap_cur, llvm::ConstantInt::get(i64_ty, 8), "vaarg.ap.next");
      be_state->builder->CreateStore(ap_next, va_list_ptr);
      llvm::Value* ind_ptr = be_state->builder->CreateLoad(ptr_ty, ap_cur, "vaarg.ind.ptr");
      *out_val = be_state->builder->CreateLoad(llvm_ty, ind_ptr, "vaarg.ind.val");
      return llvm_gen_be_error_t::ok;
    }
  } else if (!cur_fn) {
    if (raw_ty->alignment > 16) {
      *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty, "vaarg.aligned");
      return llvm_gen_be_error_t::ok;
    }
    *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty);
    return llvm_gen_be_error_t::ok;
  } else if (triple.isOSDarwin() || triple.isOSWindows()) {
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Value* ap_cur = be_state->builder->CreateLoad(ptr_ty, va_list_ptr, "vaarg.ap");
    if (raw_ty->alignment > 8 && triple.isOSDarwin()) {
      llvm::Value* int_ptr = be_state->builder->CreatePtrToInt(ap_cur, i64_ty);
      uint64_t align_mask = static_cast<uint64_t>(raw_ty->alignment) - 1ULL;
      llvm::Value* aligned_int = be_state->builder->CreateAnd(
          be_state->builder->CreateAdd(int_ptr, llvm::ConstantInt::get(i64_ty, align_mask)),
          llvm::ConstantInt::get(i64_ty, ~align_mask));
      ap_cur = be_state->builder->CreateIntToPtr(aligned_int, ptr_ty, "vaarg.ap.aligned");
    }
    uint64_t step = ((static_cast<uint64_t>(raw_ty->size) + 7ULL) & ~7ULL);
    if (step == 0) {
      *out_val = llvm::Constant::getNullValue(llvm_ty);
      return llvm_gen_be_error_t::ok;
    }
    llvm::Value* ap_next = be_state->builder->CreateGEP(
        i8_ty, ap_cur, llvm::ConstantInt::get(i64_ty, step), "vaarg.ap.next");
    be_state->builder->CreateStore(ap_next, va_list_ptr);
    *out_val = be_state->builder->CreateLoad(llvm_ty, ap_cur, "vaarg.val");
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::StructType* va_tag_ty = llvm::StructType::get(*be_state->context, {ptr_ty, ptr_ty, ptr_ty, i32_ty, i32_ty});

  bool is_fp_reg = (arg_info.abi_class == aapcs64_abi_class_t::float_vector ||
                    arg_info.abi_class == aapcs64_abi_class_t::hfa_hva);
  uint32_t num_regs = (!is_indirect && arg_info.num_elements > 0) ? arg_info.num_elements : 1;
  uint32_t reg_slot_size = is_fp_reg ? 16 : 8;
  uint32_t total_reg_bytes = num_regs * reg_slot_size;

  llvm::Value* stack_gep = be_state->builder->CreateStructGEP(va_tag_ty, va_list_ptr, 0, "vaarg.stack_ptr");
  llvm::Value* top_gep = be_state->builder->CreateStructGEP(va_tag_ty, va_list_ptr, is_fp_reg ? 2 : 1, "vaarg.top_ptr");
  llvm::Value* offs_gep = be_state->builder->CreateStructGEP(va_tag_ty, va_list_ptr, is_fp_reg ? 4 : 3, "vaarg.offs_ptr");

  llvm::Value* cur_offs = be_state->builder->CreateLoad(i32_ty, offs_gep, "vaarg.offs");
  llvm::Value* next_offs = be_state->builder->CreateAdd(
      cur_offs, llvm::ConstantInt::get(i32_ty, total_reg_bytes), "vaarg.next_offs");
  be_state->builder->CreateStore(next_offs, offs_gep);
  llvm::Value* fits_reg = be_state->builder->CreateICmpSLE(
      next_offs, llvm::ConstantInt::get(i32_ty, 0), "vaarg.fits_reg");

  llvm::BasicBlock* in_reg_bb = llvm::BasicBlock::Create(*be_state->context, "vaarg.in_reg", cur_fn);
  llvm::BasicBlock* in_mem_bb = llvm::BasicBlock::Create(*be_state->context, "vaarg.in_mem", cur_fn);
  llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "vaarg.end", cur_fn);
  be_state->builder->CreateCondBr(fits_reg, in_reg_bb, in_mem_bb);

  // 1. Register save area path
  be_state->builder->SetInsertPoint(in_reg_bb);
  llvm::Value* top_ptr = be_state->builder->CreateLoad(ptr_ty, top_gep, "vaarg.top");
  llvm::Value* offs_sext = be_state->builder->CreateSExt(cur_offs, i64_ty);
  llvm::Value* reg_addr = be_state->builder->CreateGEP(i8_ty, top_ptr, offs_sext, "vaarg.reg_addr");
  if (arg_info.abi_class == aapcs64_abi_class_t::hfa_hva) {
    llvm::AllocaInst* hfa_tmp = be_state->builder->CreateAlloca(llvm_ty, nullptr, "vaarg.hfa.tmp");
    uint64_t elem_sz = be_state->module->getDataLayout().getTypeAllocSize(arg_info.hfa_hva_elem_ty);
    for (uint32_t i = 0; i < num_regs; ++i) {
      llvm::Value* src_slot = be_state->builder->CreateGEP(
          i8_ty, reg_addr, llvm::ConstantInt::get(i64_ty, i * 16));
      llvm::Value* dst_slot = be_state->builder->CreateGEP(
          i8_ty, hfa_tmp, llvm::ConstantInt::get(i64_ty, i * elem_sz));
      llvm::Value* el_val = be_state->builder->CreateLoad(arg_info.hfa_hva_elem_ty, src_slot);
      be_state->builder->CreateStore(el_val, dst_slot);
    }
    reg_addr = hfa_tmp;
  }
  be_state->builder->CreateBr(end_bb);

  // 2. Stack overflow path
  be_state->builder->SetInsertPoint(in_mem_bb);
  llvm::Value* stack_cur = be_state->builder->CreateLoad(ptr_ty, stack_gep, "vaarg.stack");
  if (!is_indirect && raw_ty->alignment > 8) {
    llvm::Value* int_ptr = be_state->builder->CreatePtrToInt(stack_cur, i64_ty);
    uint64_t align_mask = static_cast<uint64_t>(raw_ty->alignment) - 1ULL;
    llvm::Value* aligned_int = be_state->builder->CreateAnd(
        be_state->builder->CreateAdd(int_ptr, llvm::ConstantInt::get(i64_ty, align_mask)),
        llvm::ConstantInt::get(i64_ty, ~align_mask));
    stack_cur = be_state->builder->CreateIntToPtr(aligned_int, ptr_ty, "vaarg.stack.aligned");
  }
  uint64_t step = is_indirect ? 8ULL : ((static_cast<uint64_t>(raw_ty->size) + 7ULL) & ~7ULL);
  if (step == 0) step = 8;
  llvm::Value* stack_next = be_state->builder->CreateGEP(
      i8_ty, stack_cur, llvm::ConstantInt::get(i64_ty, step), "vaarg.stack.next");
  be_state->builder->CreateStore(stack_next, stack_gep);
  be_state->builder->CreateBr(end_bb);

  // 3. Merge & load
  be_state->builder->SetInsertPoint(end_bb);
  llvm::PHINode* addr_phi = be_state->builder->CreatePHI(ptr_ty, 2, "vaarg.addr");
  addr_phi->addIncoming(reg_addr, in_reg_bb);
  addr_phi->addIncoming(stack_cur, in_mem_bb);
  if (is_indirect) {
    llvm::Value* ind_ptr = be_state->builder->CreateLoad(ptr_ty, addr_phi, "vaarg.ind.ptr");
    uint64_t align_bytes = std::max<uint64_t>(1, static_cast<uint64_t>(raw_ty->alignment));
    *out_val = be_state->builder->CreateAlignedLoad(
        llvm_ty, ind_ptr, llvm::MaybeAlign(align_bytes), "vaarg.ind.val");
    return llvm_gen_be_error_t::ok;
  }
  *out_val = be_state->builder->CreateLoad(llvm_ty, addr_phi, "vaarg.val");
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_copy intrinsic for the AAPCS64 ABI.
 * @details Invokes the llvm.va_copy intrinsic.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list struct.
 * @param[in] src_va_list_ptr Pointer to the source va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_aapcs64_va_copy(
    llvm::Value* dest_va_list_ptr,
    llvm::Value* src_va_list_ptr,
    llvm::Value** out_val) noexcept {
  if (!dest_va_list_ptr || !src_va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  llvm::Function* vacopy_fn = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vacopy);
  llvm::Value* dest_cast = be_state->builder->CreatePointerCast(dest_va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  llvm::Value* src_cast = be_state->builder->CreatePointerCast(src_va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vacopy_fn, {dest_cast, src_cast});
  
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_end intrinsic for the AAPCS64 ABI.
 * @details Invokes the llvm.va_end intrinsic.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_aapcs64_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  llvm::Function* vaend_fn = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vaend);
  llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vaend_fn, {cast_ptr});
  
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Builds Windows ARM64EC calling convention thunk signatures and entrypoints.
 * @details Emulation Compatible ARM64EC thunks mapping AAPCS64 to x64 conventions.
 * Constructs the LLVM FunctionType and AttributeList for an ARM64EC guest-to-host
 * or host-to-guest thunk, handling sret (mapped via x8/RCX adjustment) and variadic
 * parameter array forwarding (via x4/x9).
 * @param[in] ty The EDG routine type.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_arm64ec_thunk_signature(
    a_type_ptr ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {
  if (ty == nullptr || out_fn_ty == nullptr || out_attrs == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::FunctionType* base_fn_ty = nullptr;
  llvm::AttributeList base_attrs;
  llvm_gen_be_error_t err = build_aapcs64_function_type(ty, &base_fn_ty, &base_attrs);
  if (err != llvm_gen_be_error_t::ok) {
    return err;
  }

  bool is_vararg = (ty->variant.routine.extra_info != nullptr)
                       ? (ty->variant.routine.extra_info->has_ellipsis != FALSE)
                       : false;

  if (is_vararg) {
    // ARM64EC variadic routines pass a pointer to the variadic argument array
    // in x4 and the total byte size in x5, modeled as an explicit pointer parameter
    // followed by the variadic ellipsis.
    std::vector<llvm::Type*> thunk_params(
        base_fn_ty->param_begin(), base_fn_ty->param_end());
    thunk_params.push_back(llvm::PointerType::getUnqual(*be_state->context));
    *out_fn_ty = llvm::FunctionType::get(
        base_fn_ty->getReturnType(), thunk_params, /*isVarArg=*/true);
  } else {
    *out_fn_ty = base_fn_ty;
  }

  llvm::AttrBuilder fn_ab{*be_state->context};
  fn_ab.addAttribute("arm64ec-thunk", "true");
  llvm::AttributeSet fn_set = llvm::AttributeSet::get(*be_state->context, fn_ab);

  std::vector<llvm::AttributeSet> param_sets;
  unsigned num_base_params = base_fn_ty->getNumParams();
  for (unsigned i = 0; i < num_base_params; ++i) {
    param_sets.push_back(base_attrs.getParamAttrs(i));
  }
  if (is_vararg) {
    param_sets.push_back(llvm::AttributeSet());
  }

  *out_attrs = llvm::AttributeList::get(
      *be_state->context, fn_set, base_attrs.getRetAttrs(), param_sets);
  return llvm_gen_be_error_t::ok;
}
END_EDG_NAMESPACE
#endif
