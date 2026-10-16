/**
 * @file llvm_gen_be_abi_aapcs64.cpp
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
    uint32_t count = ty->variant.array.variant.number_of_elements;
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

  uint64_t size = ty->size;

  llvm::Triple triple(be_state->module->getTargetTriple());
  bool is_apple = triple.isOSDarwin();

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

  if (ty->kind == tk_integer || ty->kind == tk_pointer || ty->kind == tk_ptr_to_member) {
    out_info->abi_class = aapcs64_abi_class_t::integer;
    out_info->num_elements = (size > 8) ? 2 : 1; // 128-bit ints take two registers
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_float) {
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
  out_info->num_elements = (size + 7) / 8; 
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

  if (!out_info || !out_sret) return llvm_gen_be_error_t::invalid_argument;

  if (ret_ty != nullptr && ret_ty->kind != tk_void) {
    bool is_empty = false;
    if (ret_ty->size == 0) {
      is_empty = true;
    } else if (ret_ty->kind == tk_class || ret_ty->kind == tk_struct) {
      if (is_empty_class_type(ret_ty)) {
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
  llvm::AttrBuilder ret_attr(*be_state->context);
  std::vector<llvm::AttributeSet> param_attrs;

  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  
  aapcs64_arg_info_t ret_info;
  bool sret = false;
  llvm_gen_be_error_t err = compute_aapcs64_return_info(ret_ty, &ret_info, &sret);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Type* llvm_ret_ty = nullptr;

  if (sret) {
    llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
    llvm::Type* sret_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm_param_types.push_back(sret_ptr_ty);

    llvm::AttrBuilder sret_ab(*be_state->context);
    llvm::Type* sret_pointee_ty = nullptr;
    err = llvm_type_from_edg_type(ret_ty, &sret_pointee_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    sret_ab.addStructRetAttr(sret_pointee_ty);
    x_regs++; // XR8 is used for sret
    param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  } else if (ret_info.abi_class == aapcs64_abi_class_t::no_class) {
    llvm_ret_ty = llvm::Type::getVoidTy(*be_state->context);
  } else {
    err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::Triple triple(be_state->module->getTargetTriple());
    if (triple.isOSDarwin() && ret_info.abi_class == aapcs64_abi_class_t::integer) {
      if (ret_ty->size < 4) {
         if (int_type_is_signed(ret_ty)) ret_attr.addAttribute(llvm::Attribute::SExt);
         else ret_attr.addAttribute(llvm::Attribute::ZExt);
      }
    }
  }

  bool is_vararg = (routine_ty->variant.routine.extra_info != nullptr)
                       ? routine_ty->variant.routine.extra_info->has_ellipsis
                       : false;

  llvm::Triple triple(be_state->module->getTargetTriple());
  bool is_apple = triple.isOSDarwin();

  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
      
      aapcs64_arg_info_t arg_info;
      err = classify_aapcs64_argument(param->type, &arg_info);
      if (err != llvm_gen_be_error_t::ok) return err;

      llvm::Type* pty = nullptr;
      err = llvm_type_from_edg_type(param->type, &pty);
      if (err != llvm_gen_be_error_t::ok) return err;

      llvm::AttrBuilder ab(*be_state->context);

      if (is_apple && arg_info.abi_class == aapcs64_abi_class_t::integer && param->type->size < 4) {
         if (int_type_is_signed(param->type)) ab.addAttribute(llvm::Attribute::SExt);
         else ab.addAttribute(llvm::Attribute::ZExt);
      }

      if (arg_info.abi_class == aapcs64_abi_class_t::no_class) {
        // Ignored by ABI (e.g., Apple Darwin empty structs). Pass as size 0 LLVM struct {}.
        llvm_param_types.push_back(llvm::StructType::get(*be_state->context));
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
      } else if (arg_info.abi_class == aapcs64_abi_class_t::reference) {
        // Passed indirectly by address (caller allocated)
        llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
        // We typically don't mark it `byval` on ARM64 if it's passed as an opaque indirect pointer,
        // but clang sometimes uses `byval` depending on the exact C/C++ frontend semantics.
        // For simplicity and matching typical AAPCS64 copy-on-call, we add byval.
        // Actually AAPCS64 specifies it's passed by pointer to a caller-allocated copy.
        // We can use the ByVal attribute to let LLVM handle the copy, or we can explicitly copy.
        if (!arg_info.pass_by_invisible_reference) {
          ab.addByValAttr(pty);
        }
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
        if (x_regs < 8) x_regs++;
      } else if (arg_info.abi_class == aapcs64_abi_class_t::hfa_hva) {
        if (v_regs + arg_info.num_elements <= 8) {
          v_regs += arg_info.num_elements;
        }
        // HFA/HVA are mapped to LLVM IR as arrays or structs.
        llvm_param_types.push_back(pty);
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
      } else if (arg_info.abi_class == aapcs64_abi_class_t::float_vector) {
        if (v_regs < 8) v_regs++;
        llvm_param_types.push_back(pty);
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
      } else { // integer
        if (x_regs + arg_info.num_elements <= 8) {
          x_regs += arg_info.num_elements;
        }
        llvm_param_types.push_back(pty);
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
  
  llvm::Triple triple(be_state->module->getTargetTriple());
  if (triple.isOSDarwin()) {
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

  llvm::Function* vastart_fn = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::vastart, {llvm::PointerType::getUnqual(*be_state->context)});
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

  llvm::Type* llvm_ty = nullptr;
  llvm_gen_be_error_t err = llvm_type_from_edg_type(ty, &llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty);
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

  llvm::Function* vacopy_fn = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::vacopy, {llvm::PointerType::getUnqual(*be_state->context)});
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

  llvm::Function* vaend_fn = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::vaend, {llvm::PointerType::getUnqual(*be_state->context)});
  llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vaend_fn, {cast_ptr});
  
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t build_arm64ec_thunk_signature(
    a_type_ptr ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {
  if (out_fn_ty == nullptr || out_attrs == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  // Windows ARM64EC specific thunk generation mapping x64 structures and
  // calling conventions onto AAPCS64 entrypoints.
  return llvm_gen_be_error_t::not_implemented;
}
END_EDG_NAMESPACE
#endif

/**
 * @brief Builds Windows ARM64EC calling convention thunk signatures and entrypoints.
 * @details Emulation Compatible ARM64EC thunks mapping AAPCS64 to x64 conventions.
 * @param[in] ty The EDG routine type.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
