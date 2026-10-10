/**
 * @file llvm_gen_be_abi_sysv_x86_64.cpp
 * @brief System V AMD64 ABI classification implementation.
 * @details Implements the parameter passing and return value classification
 * rules specified in the System V Application Binary Interface AMD64 Architecture
 * Processor Supplement (Draft Version 1.0).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_abi_sysv_x86_64.h"
#include "llvm_gen_be_internal.h"
#include "target.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Merges two ABI classes for an eightbyte according to System V rules.
 * @param[in] accum The accumulated class so far.
 * @param[in] field The class of the new field.
 * @param[out] out_class Pointer to store the merged class.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t merge_classes(x86_64_abi_class_t accum, x86_64_abi_class_t field, x86_64_abi_class_t* out_class) noexcept {
  if (out_class == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (accum == field) {
    *out_class = accum;
    return llvm_gen_be_error_t::ok;
  }
  if (accum == x86_64_abi_class_t::no_class) {
    *out_class = field;
    return llvm_gen_be_error_t::ok;
  }
  if (field == x86_64_abi_class_t::no_class) {
    *out_class = accum;
    return llvm_gen_be_error_t::ok;
  }
  if (accum == x86_64_abi_class_t::memory || field == x86_64_abi_class_t::memory) {
    *out_class = x86_64_abi_class_t::memory;
    return llvm_gen_be_error_t::ok;
  }
  if (accum == x86_64_abi_class_t::integer || field == x86_64_abi_class_t::integer) {
    *out_class = x86_64_abi_class_t::integer;
    return llvm_gen_be_error_t::ok;
  }
  if (accum == x86_64_abi_class_t::x87 || accum == x86_64_abi_class_t::x87up || accum == x86_64_abi_class_t::complex_x87 ||
      field == x86_64_abi_class_t::x87 || field == x86_64_abi_class_t::x87up || field == x86_64_abi_class_t::complex_x87) {
    *out_class = x86_64_abi_class_t::memory;
    return llvm_gen_be_error_t::ok;
  }
  *out_class = x86_64_abi_class_t::sse;
  return llvm_gen_be_error_t::ok;
}

static llvm_gen_be_error_t classify_internal(
    a_type_ptr ty,
    uint64_t offset,
    x86_64_abi_class_t classes[2]) noexcept {

  if (ty == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  if (ty->kind == tk_typeref) {
    return classify_internal(ty->variant.typeref.type, offset, classes);
  }

  uint64_t size = ty->size;
  if (size == 0) {
    return llvm_gen_be_error_t::ok; // no_class
  }

  if (offset + size > 16) {
    classes[0] = x86_64_abi_class_t::memory;
    classes[1] = x86_64_abi_class_t::memory;
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_integer || ty->kind == tk_pointer || ty->kind == tk_ptr_to_member) {
    uint64_t idx = offset / 8;
    llvm_gen_be_error_t err = merge_classes(classes[idx], x86_64_abi_class_t::integer, &classes[idx]);
    if (err != llvm_gen_be_error_t::ok) return err;
    if (offset % 8 + size > 8) {
      if (idx + 1 < 2) {
        err = merge_classes(classes[idx + 1], x86_64_abi_class_t::integer, &classes[idx + 1]);
        if (err != llvm_gen_be_error_t::ok) return err;
      }
    }
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_float) {
    if (ty->variant.float_kind == fk_long_double && size == 16) {
      uint64_t idx = offset / 8;
      llvm_gen_be_error_t err = merge_classes(classes[idx], x86_64_abi_class_t::x87, &classes[idx]);
      if (err != llvm_gen_be_error_t::ok) return err;
      if (idx + 1 < 2) {
        err = merge_classes(classes[idx + 1], x86_64_abi_class_t::x87up, &classes[idx + 1]);
        if (err != llvm_gen_be_error_t::ok) return err;
      }
    } else if (ty->variant.float_kind == fk_float128 || ty->variant.float_kind == fk_std_float128) {
      uint64_t idx = offset / 8;
      llvm_gen_be_error_t err = merge_classes(classes[idx], x86_64_abi_class_t::sse, &classes[idx]);
      if (err != llvm_gen_be_error_t::ok) return err;
      if (idx + 1 < 2) {
        err = merge_classes(classes[idx + 1], x86_64_abi_class_t::sseup, &classes[idx + 1]);
        if (err != llvm_gen_be_error_t::ok) return err;
      }
    } else {
      uint64_t idx = offset / 8;
      llvm_gen_be_error_t err = merge_classes(classes[idx], x86_64_abi_class_t::sse, &classes[idx]);
      if (err != llvm_gen_be_error_t::ok) return err;
      if (offset % 8 + size > 8) {
        if (idx + 1 < 2) {
          err = merge_classes(classes[idx + 1], x86_64_abi_class_t::sse, &classes[idx + 1]);
          if (err != llvm_gen_be_error_t::ok) return err;
        }
      }
    }
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_array) {
    a_type_ptr elem_ty = ty->variant.array.element_type;
    uint64_t elem_size = elem_ty->size;
    uint64_t num_elems = ty->variant.array.variant.number_of_elements;
    for (uint64_t i = 0; i < num_elems; ++i) {
      llvm_gen_be_error_t err = classify_internal(elem_ty, offset + i * elem_size, classes);
      if (err != llvm_gen_be_error_t::ok) {
        return err;
      }
    }
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_struct || ty->kind == tk_union || ty->kind == tk_class) {
    for (a_field_ptr field = ty->variant.class_struct_union.field_list; field != nullptr; field = field->next) {
      uint64_t field_offset = offset + field->offset;

      // Handle unaligned fields
      if (field->offset % alignment_of_type(field->type) != 0) {
        classes[0] = x86_64_abi_class_t::memory;
        classes[1] = x86_64_abi_class_t::memory;
        return llvm_gen_be_error_t::ok;
      }

      if (ty->kind == tk_union) {
        field_offset = offset;
      }
      llvm_gen_be_error_t err = classify_internal(field->type, field_offset, classes);
      if (err != llvm_gen_be_error_t::ok) {
        return err;
      }
    }
    return llvm_gen_be_error_t::ok;
  }

  // Fallback for unsupported types
  classes[0] = x86_64_abi_class_t::memory;
  classes[1] = x86_64_abi_class_t::memory;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Classifies a single EDG type into System V AMD64 ABI eightbyte classes.
 * @details Evaluates scalars, arrays, pointers, and aggregates against the ABI rules
 * to determine register or memory allocation. Empty structs map to no_class. Types
 * larger than 16 bytes default to memory.
 * @param[in] ty The EDG type to classify.
 * @param[in] offset The current byte offset of the type within a parent aggregate (usually 0).
 * @param[out] out_info The classification result information structure.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t classify_sysv_argument(
    a_type_ptr ty,
    uint64_t offset,
    x86_64_abi_arg_info_t* out_info) noexcept {

  if (out_info == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  out_info->eightbyte_classes[0] = x86_64_abi_class_t::no_class;
  out_info->eightbyte_classes[1] = x86_64_abi_class_t::no_class;
  out_info->num_eightbytes = 0;
  out_info->pass_in_memory = false;
  out_info->requires_sret = false;

  if (ty == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  // Unwrap typerefs
  while (ty->kind == tk_typeref) {
    ty = ty->variant.typeref.type;
  }

  uint64_t size = ty->size;

  if (size == 0) {
    out_info->num_eightbytes = 0;
    return llvm_gen_be_error_t::ok;
  }

  if (size > 16) {
    out_info->pass_in_memory = true;
    out_info->eightbyte_classes[0] = x86_64_abi_class_t::memory;
    out_info->eightbyte_classes[1] = x86_64_abi_class_t::memory;
    return llvm_gen_be_error_t::ok;
  }

  x86_64_abi_class_t classes[2] = {x86_64_abi_class_t::no_class, x86_64_abi_class_t::no_class};

  llvm_gen_be_error_t err = classify_internal(ty, offset, classes);
  if (err != llvm_gen_be_error_t::ok) {
    return err;
  }

  // Post-merge fixups
  if (classes[0] == x86_64_abi_class_t::memory || classes[1] == x86_64_abi_class_t::memory) {
    out_info->pass_in_memory = true;
    out_info->eightbyte_classes[0] = x86_64_abi_class_t::memory;
    out_info->eightbyte_classes[1] = x86_64_abi_class_t::memory;
  } else {
    out_info->eightbyte_classes[0] = classes[0];
    out_info->eightbyte_classes[1] = classes[1];
    out_info->num_eightbytes = (size > 8) ? 2 : 1;

    if (classes[0] == x86_64_abi_class_t::x87up) {
      if (classes[1] == x86_64_abi_class_t::x87up) {
        // ...
      }
    }
  }

  return llvm_gen_be_error_t::ok;
}



/**
 * @brief Computes System V AMD64 return value passing information.
 * @details Evaluates the return type according to System V ABI return rules. 
 * Determines if the type requires an sret pointer, or if it can be returned 
 * in registers.
 * @param[in] ret_ty The EDG type representing the function return type.
 * @param[out] out_info The classification result information structure.
 * @param[out] out_sret Pointer to a boolean set to true if the return is via sret.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t compute_sysv_return_info(
    a_type_ptr ret_ty,
    x86_64_abi_arg_info_t* out_info,
    bool* out_sret) noexcept {

  if (out_info == nullptr || out_sret == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm_gen_be_error_t err = classify_sysv_argument(ret_ty, 0, out_info);
  if (err != llvm_gen_be_error_t::ok) {
    return err;
  }

  // System V AMD64 return rules:
  // If the type has class MEMORY, then the caller provides space for the return
  // value and passes the address of this storage in %rdi as if it were the first
  // argument to the function.
  if (out_info->pass_in_memory) {
    *out_sret = true;
    out_info->requires_sret = true;
  } else {
    *out_sret = false;
    out_info->requires_sret = false;
  }

  // Handle C++ non-trivial classes
  // A C++ class that has a non-trivial copy constructor or destructor is passed in memory.
  // We can check if it's a class with those properties, but the EDG frontend usually
  // handles this. Let's assume class MEMORY is correctly populated, but if we need
  // to force sret for non-trivial types we would do it here.
  // Actually, wait, the ABI says "If a C++ object has either a non-trivial copy 
  // constructor or a non-trivial destructor, it is passed by invisible reference"
  // Let's implement that.
  if (ret_ty != nullptr && (ret_ty->kind == tk_class || ret_ty->kind == tk_struct)) {
    if (!is_trivially_copyable_type(ret_ty)) {
      *out_sret = true;
      out_info->requires_sret = true;
      out_info->pass_in_memory = true;
    }
  }

  return llvm_gen_be_error_t::ok;
}


/**
 * @brief Constructs an LLVM FunctionType and AttributeList matching System V AMD64 ABI.
 * @details Evaluates function parameters and return type against the classification engine.
 * Determines sret, byval, and register allocation. Tracks available integer and SSE
 * registers.
 * @param[in] routine_ty The EDG routine type structure.
 * @param[out] out_fn_ty Pointer to store the resulting LLVM FunctionType.
 * @param[out] out_attrs Pointer to store the resulting LLVM AttributeList.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_sysv_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {

  if (routine_ty == nullptr || out_fn_ty == nullptr || out_attrs == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  uint32_t int_regs = 0;
  uint32_t sse_regs = 0;

  std::vector<llvm::Type*> llvm_param_types;
  llvm::AttrBuilder ret_attr(*be_state->context);
  std::vector<llvm::AttributeSet> param_attrs;

  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  
  x86_64_abi_arg_info_t ret_info;
  bool sret = false;
  llvm_gen_be_error_t err = compute_sysv_return_info(ret_ty, &ret_info, &sret);
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
    int_regs++;
    param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  } else {
    err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
  }

  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
      
      x86_64_abi_arg_info_t arg_info;
      err = classify_sysv_argument(param->type, 0, &arg_info);
      if (err != llvm_gen_be_error_t::ok) return err;

      llvm::Type* pty = nullptr;
      err = llvm_type_from_edg_type(param->type, &pty);
      if (err != llvm_gen_be_error_t::ok) return err;

      llvm::AttrBuilder ab(*be_state->context);

      if (arg_info.pass_in_memory) {
        llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
        ab.addByValAttr(pty);
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
      } else {
        uint32_t needed_ints = 0;
        uint32_t needed_sses = 0;

        for (uint32_t i = 0; i < arg_info.num_eightbytes; i++) {
          if (arg_info.eightbyte_classes[i] == x86_64_abi_class_t::integer) needed_ints++;
          if (arg_info.eightbyte_classes[i] == x86_64_abi_class_t::sse) needed_sses++;
        }

        if (int_regs + needed_ints <= 6 && sse_regs + needed_sses <= 8) {
          int_regs += needed_ints;
          sse_regs += needed_sses;
        }
        llvm_param_types.push_back(pty);
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
      }
    }
  }

  bool is_vararg = (routine_ty->variant.routine.extra_info != nullptr)
                       ? routine_ty->variant.routine.extra_info->has_ellipsis
                       : false;

  *out_fn_ty = llvm::FunctionType::get(llvm_ret_ty, llvm_param_types, is_vararg);
  
  llvm::AttributeSet ret_set = llvm::AttributeSet::get(*be_state->context, ret_attr);
  llvm::AttributeSet fn_set;
  
  *out_attrs = llvm::AttributeList::get(*be_state->context, fn_set, ret_set, param_attrs);

  return llvm_gen_be_error_t::ok;
}


#include <llvm/IR/Intrinsics.h>

/**
 * @brief Lowers a va_start intrinsic for the System V AMD64 ABI.
 * @details Casts the va_list pointer to an i8* and invokes the llvm.va_start intrinsic.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_sysv_va_start(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  llvm::Function* vastart_fn = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::vastart, {llvm::PointerType::getUnqual(*be_state->context)});
  llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vastart_fn, {cast_ptr});
  
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_arg macro expansion for the System V AMD64 ABI.
 * @details Reads arguments from the gp_offset, fp_offset, or overflow_arg_area
 * depending on the classification of the requested type.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[in] ty The EDG type being requested.
 * @param[out] out_val The resulting loaded value.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_sysv_va_arg(
    llvm::Value* va_list_ptr,
    a_type_ptr ty,
    llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !ty || !out_val) return llvm_gen_be_error_t::invalid_argument;

  x86_64_abi_arg_info_t arg_info;
  llvm_gen_be_error_t err = classify_sysv_argument(ty, 0, &arg_info);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Type* llvm_ty = nullptr;
  err = llvm_type_from_edg_type(ty, &llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  // For System V AMD64, we can actually use the LLVM va_arg instruction which 
  // correctly implements the gp_offset, fp_offset, overflow_arg_area, reg_save_area 
  // unpacking internally in the x86_64 LLVM backend.
  *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty);

  return llvm_gen_be_error_t::ok;
}


/**
 * @brief Lowers a va_end intrinsic for the System V AMD64 ABI.
 * @details Invokes the llvm.va_end intrinsic.
 * @param[in] va_list_ptr Pointer to the va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_sysv_va_end(
    llvm::Value* va_list_ptr,
    llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  llvm::Function* vaend_fn = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::vaend, {llvm::PointerType::getUnqual(*be_state->context)});
  llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vaend_fn, {cast_ptr});
  
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a va_copy intrinsic for the System V AMD64 ABI.
 * @details Invokes the llvm.va_copy intrinsic.
 * @param[in] dest_va_list_ptr Pointer to the destination va_list struct.
 * @param[in] src_va_list_ptr Pointer to the source va_list struct.
 * @param[out] out_val The resulting intrinsic call instruction.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t lower_sysv_va_copy(
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

END_EDG_NAMESPACE
#endif
