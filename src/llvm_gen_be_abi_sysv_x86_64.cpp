/**
 * @file llvm_gen_be_abi_sysv_x86_64.cpp
 * @ingroup llvm_gen_be_abi
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
#include "llvm_gen_be_abi_win64.h"
#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_abi_x86_32.h"
#include "llvm_gen_be_abi_arm32.h"
#include "llvm_gen_be_abi_riscv64.h"
#include "llvm_gen_be_abi_loongarch64.h"
#include "llvm_gen_be_abi_mips.h"
#include "llvm_gen_be_abi_ppc64.h"
#include "llvm_gen_be_abi_s390x.h"
#include "llvm_gen_be_abi_sparc.h"
#include "llvm_gen_be_abi_wasm.h"
#include "llvm_gen_be_internal.h"
#include "target.h"
#include <llvm/TargetParser/Triple.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Merges two ABI classes for an eightbyte according to System V rules.
 * @details Implements or declares `merge_classes` for the EDG LLVM backend.
 * @param[in] accum The accumulated class so far.
 * @param[in] field The class of the new field.
 * @param[out] out_class Pointer to store the merged class.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t merge_classes(x86_64_abi_class_t accum, x86_64_abi_class_t field, x86_64_abi_class_t* out_class) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  if (accum == field) {
    *out_class = accum;
    return llvm_gen_be_error_t::ok;
  }
  if (accum == x86_64_abi_class_t::no_class) {
    *out_class = field;
    return llvm_gen_be_error_t::ok;
  }
  if (accum == x86_64_abi_class_t::integer || field == x86_64_abi_class_t::integer) {
    *out_class = x86_64_abi_class_t::integer;
    return llvm_gen_be_error_t::ok;
  }
  if (accum == x86_64_abi_class_t::x87 || accum == x86_64_abi_class_t::x87up ||
      field == x86_64_abi_class_t::x87 || field == x86_64_abi_class_t::x87up) {
    *out_class = x86_64_abi_class_t::memory;
    return llvm_gen_be_error_t::ok;
  }
  *out_class = x86_64_abi_class_t::sse;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Recursively classifies an EDG type into System V AMD64 ABI eightbyte classes.
 * @details Walks through the fields, arrays, and scalars of a type to accumulate the overall
 * eightbyte classification for up to two eightbytes (16 bytes maximum).
 * @param[in] ty The EDG type being classified.
 * @param[in] offset The current byte offset within the outermost aggregate.
 * @param[in,out] classes An array of two x86_64_abi_class_t elements accumulating the classification.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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

  uint64_t size = static_cast<uint64_t>(ty->size); 
  if (size == 0) {
    return llvm_gen_be_error_t::ok; // no_class
  }

  if (offset + size > 16) {
    classes[0] = x86_64_abi_class_t::memory;
    classes[1] = x86_64_abi_class_t::memory;
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_integer || ty->kind == tk_pointer || ty->kind == tk_nullptr || ty->kind == tk_ptr_to_member) {
    uint64_t idx = offset / 8;
    LLVM_GEN_BE_CHECK(merge_classes(classes[idx], x86_64_abi_class_t::integer, &classes[idx]));
    if (offset % 8 + size > 8) {
      LLVM_GEN_BE_CHECK(merge_classes(classes[idx + 1], x86_64_abi_class_t::integer, &classes[idx + 1]));
    }
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_float) {
    const bool is_android = llvm::Triple(be_state->module->getTargetTriple()).isAndroid();
    if (ty->variant.float_kind == fk_float80 ||
        (ty->variant.float_kind == fk_long_double && size == 16 && !is_android)) {
      uint64_t idx = offset / 8;
      LLVM_GEN_BE_CHECK(merge_classes(classes[idx], x86_64_abi_class_t::x87, &classes[idx]));
      LLVM_GEN_BE_CHECK(merge_classes(classes[idx + 1], x86_64_abi_class_t::x87up, &classes[idx + 1]));
    } else if (ty->variant.float_kind == fk_float128 || ty->variant.float_kind == fk_std_float128 ||
               (ty->variant.float_kind == fk_long_double && size == 16)) {
      uint64_t idx = offset / 8;
      LLVM_GEN_BE_CHECK(merge_classes(classes[idx], x86_64_abi_class_t::sse, &classes[idx]));
      LLVM_GEN_BE_CHECK(merge_classes(classes[idx + 1], x86_64_abi_class_t::sseup, &classes[idx + 1]));
    } else {
      uint64_t idx = offset / 8;
      LLVM_GEN_BE_CHECK(merge_classes(classes[idx], x86_64_abi_class_t::sse, &classes[idx]));
      if (offset % 8 + size > 8) {
        LLVM_GEN_BE_CHECK(merge_classes(classes[idx + 1], x86_64_abi_class_t::sse, &classes[idx + 1]));
      }
    }
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_complex) {
    uint64_t idx = offset / 8;
    LLVM_GEN_BE_CHECK(merge_classes(classes[idx], x86_64_abi_class_t::sse, &classes[idx]));
    if (size > 8) {
      LLVM_GEN_BE_CHECK(merge_classes(classes[idx + 1], x86_64_abi_class_t::sse, &classes[idx + 1]));
    }
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_array) {
    a_type_ptr elem_ty = ty->variant.array.element_type;
    uint64_t elem_size = static_cast<uint64_t>(elem_ty->size);
    uint64_t num_elems = static_cast<uint64_t>(ty->variant.array.variant.number_of_elements);
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
      if (field->is_bit_field) {
        if (field->bit_size == 0) continue;
        uint64_t start_bit = ((ty->kind == tk_union) ? offset : offset + static_cast<uint64_t>(field->offset)) * 8 +
                             field->offset_bit_remainder;
        uint64_t end_bit = start_bit + field->bit_size - 1;
        uint64_t first_eb = start_bit / 64;
        uint64_t last_eb = end_bit / 64;
        for (uint64_t eb = first_eb; eb <= last_eb && eb < 2; ++eb) {
          LLVM_GEN_BE_CHECK(merge_classes(classes[eb], x86_64_abi_class_t::integer, &classes[eb]));
        }
        continue;
      }
      uint64_t field_offset = (ty->kind == tk_union) ? offset : offset + static_cast<uint64_t>(field->offset);
      llvm_gen_be_error_t err = classify_internal(field->type, field_offset, classes);
      if (err != llvm_gen_be_error_t::ok) {
        return err;
      }

      // Handle unaligned fields
      if (field_offset % alignment_of_type(field->type) != 0) {
        classes[0] = x86_64_abi_class_t::memory;
        classes[1] = x86_64_abi_class_t::memory;
        return llvm_gen_be_error_t::ok;
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
 * @brief Determines whether an EDG type represents System V AMD64 `COMPLEX_X87` (`_Complex long double` / `_Complex __float80`).
 * @details Recognizes both unlowered `tk_complex` nodes and IL-lowered `_Complex` representations
 *          (`_Complex_long_double` and `_Complex_float80` structs synthesized by `lower_c99.c`), while
 *          preserving `MEMORY` (`sret`) classification for user-defined `struct { long double a[2]; }`
 *          aggregates and Android `fp128` `_Complex long double` per System V psABI §3.2.3.
 * @param[in] ty The EDG type to inspect.
 * @param[out] out_is_complex_x87 Receives `true` if `ty` has `COMPLEX_X87` ABI class, `false` otherwise.
 * @return llvm_gen_be_error_t::ok on success, `invalid_argument` if `ty` or `out_is_complex_x87` is null,
 *         or `out_of_memory` on allocation budget exhaustion.
 */
llvm_gen_be_error_t is_sysv_complex_x87_type(
    a_type_ptr ty,
    bool* out_is_complex_x87) noexcept {
  if (ty == nullptr || out_is_complex_x87 == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_is_complex_x87 = false;

  ty = skip_typerefs(ty);
  const bool is_android = llvm::Triple(be_state->module->getTargetTriple()).isAndroid();

  if (ty->kind == tk_complex) {
    *out_is_complex_x87 =
        (ty->variant.float_kind == fk_float80) ||
        (ty->variant.float_kind == fk_long_double && ty->size >= 20 && !is_android);
    return llvm_gen_be_error_t::ok;
  }

  if (ty->kind == tk_struct && ty->source_corresp.name != nullptr) {
    llvm::StringRef sname = ty->source_corresp.name;
    if (sname == "_Complex_float80") {
      *out_is_complex_x87 = true;
    } else if (sname == "_Complex_long_double") {
      *out_is_complex_x87 = (ty->size >= 20 && !is_android);
    }
  }

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
  out_info->pass_by_invisible_reference = false;

  if (ty == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_type_ptr orig_ty = ty;

  // Unwrap typerefs
  while (ty->kind == tk_typeref) {
    ty = ty->variant.typeref.type;
  }

  if ((ty->kind == tk_class || ty->kind == tk_struct) && !is_trivially_copyable_type(ty)) {
    out_info->pass_in_memory = true;
    out_info->pass_by_invisible_reference = true;
    out_info->eightbyte_classes[0] = x86_64_abi_class_t::memory;
    out_info->eightbyte_classes[1] = x86_64_abi_class_t::memory;
    return llvm_gen_be_error_t::ok;
  }

  uint64_t size = static_cast<uint64_t>(ty->size); 

  if (size == 0) {
    out_info->num_eightbytes = 0;
    return llvm_gen_be_error_t::ok;
  }

  bool is_complex_x87 = false;
  LLVM_GEN_BE_CHECK(is_sysv_complex_x87_type(orig_ty, &is_complex_x87));
  if (is_complex_x87) {
    out_info->pass_in_memory = true;
    out_info->eightbyte_classes[0] = x86_64_abi_class_t::complex_x87;
    out_info->eightbyte_classes[1] = x86_64_abi_class_t::complex_x87;
    out_info->num_eightbytes = 2;
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
    if (classes[1] == x86_64_abi_class_t::x87up && classes[0] != x86_64_abi_class_t::x87) {
      out_info->pass_in_memory = true;
      out_info->eightbyte_classes[0] = x86_64_abi_class_t::memory;
      out_info->eightbyte_classes[1] = x86_64_abi_class_t::memory;
    } else {
      if (classes[1] == x86_64_abi_class_t::sseup && classes[0] != x86_64_abi_class_t::sse) {
        classes[1] = x86_64_abi_class_t::sse;
      }
      out_info->eightbyte_classes[0] = classes[0];
      out_info->eightbyte_classes[1] = classes[1];
      out_info->num_eightbytes = (size > 8) ? 2 : 1;
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
  // argument to the function. COMPLEX_X87 returns in %st(0)/%st(1), not via sret.
  if (out_info->eightbyte_classes[0] == x86_64_abi_class_t::complex_x87) {
    *out_sret = false;
    out_info->requires_sret = false;
  } else if (out_info->pass_in_memory) {
    *out_sret = true;
    out_info->requires_sret = true;
  } else {
    *out_sret = false;
    out_info->requires_sret = false;
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Constructs the LLVM register-coerced type for a System V AMD64 aggregate or scalar.
 * @details Maps 1-eightbyte and 2-eightbyte INTEGER, SSE, SSEUP, X87, and COMPLEX_X87
 *          classifications into their psABI-mandated LLVM IR representation
 *          (e.g., `i64`, `double`, `{i64, double}`, `fp128`, `x86_fp80`, `{x86_fp80, x86_fp80}`).
 * @param[in] ty The EDG type being coerced.
 * @param[in] info Pointer to the computed System V classification info.
 * @param[out] out_coerced_ty Receives the coerced `llvm::Type*`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_sysv_coerced_type(
    a_type_ptr ty,
    const x86_64_abi_arg_info_t* info,
    llvm::Type** out_coerced_ty) noexcept {
  if (ty == nullptr || info == nullptr || out_coerced_ty == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());

  ty = skip_typerefs(ty);

  if (info->eightbyte_classes[0] == x86_64_abi_class_t::complex_x87) {
    llvm::Type* fp80_ty = llvm::Type::getX86_FP80Ty(*be_state->context);
    *out_coerced_ty = llvm::StructType::get(*be_state->context, {fp80_ty, fp80_ty});
    return llvm_gen_be_error_t::ok;
  }

  if (info->eightbyte_classes[1] == x86_64_abi_class_t::x87up) {
    *out_coerced_ty = llvm::Type::getX86_FP80Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  if (info->eightbyte_classes[1] == x86_64_abi_class_t::sseup) {
    *out_coerced_ty = llvm::Type::getFP128Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  bool is_aggregate = is_class_struct_union_type(ty) || ty->kind == tk_complex;
  if (!is_aggregate || info->num_eightbytes == 0) {
    return llvm_type_from_edg_type(ty, out_coerced_ty);
  }

  if (info->num_eightbytes == 1) {
    if (info->eightbyte_classes[0] == x86_64_abi_class_t::integer) {
      uint64_t bits = (ty->size <= 1) ? 8 : (ty->size <= 2) ? 16 : (ty->size <= 4) ? 32 : 64;
      *out_coerced_ty = llvm::Type::getIntNTy(*be_state->context, static_cast<unsigned>(bits));
    } else {
      *out_coerced_ty = (ty->size <= 4)
          ? llvm::Type::getFloatTy(*be_state->context)
          : llvm::Type::getDoubleTy(*be_state->context);
    }
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* eb0_ty = (info->eightbyte_classes[0] == x86_64_abi_class_t::integer)
      ? llvm::Type::getInt64Ty(*be_state->context)
      : llvm::Type::getDoubleTy(*be_state->context);
  uint64_t hi_bytes = static_cast<uint64_t>(ty->size) - 8;
  uint64_t hi_bits = (hi_bytes <= 4) ? 32 : 64;
  llvm::Type* eb1_ty = (info->eightbyte_classes[1] == x86_64_abi_class_t::integer)
      ? llvm::Type::getIntNTy(*be_state->context, static_cast<unsigned>(hi_bits))
      : ((hi_bytes <= 4) ? llvm::Type::getFloatTy(*be_state->context)
                         : llvm::Type::getDoubleTy(*be_state->context));
  *out_coerced_ty = llvm::StructType::get(*be_state->context, {eb0_ty, eb1_ty});
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
  llvm::AttrBuilder ret_attr{*be_state->context};
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

    llvm::AttrBuilder sret_ab{*be_state->context};
    llvm::Type* sret_pointee_ty = nullptr;
    err = llvm_type_from_edg_type(ret_ty, &sret_pointee_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    sret_ab.addStructRetAttr(sret_pointee_ty);
    int_regs++;
    param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, sret_ab));
  } else {
    err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    LLVM_GEN_BE_CHECK(build_sysv_coerced_type(ret_ty, &ret_info, &llvm_ret_ty));
    a_type_ptr raw_ret = skip_typerefs(ret_ty);
    if (raw_ret->kind == tk_integer && raw_ret->size < 4) {
      if (int_type_is_signed(raw_ret)) ret_attr.addAttribute(llvm::Attribute::SExt);
      else ret_attr.addAttribute(llvm::Attribute::ZExt);
    }
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

      llvm::AttrBuilder ab{*be_state->context};
      a_type_ptr raw_param_ty = skip_typerefs(param->type);
      if (raw_param_ty->kind == tk_integer && raw_param_ty->size < 4) {
        if (int_type_is_signed(raw_param_ty)) ab.addAttribute(llvm::Attribute::SExt);
        else ab.addAttribute(llvm::Attribute::ZExt);
      }

      if (arg_info.pass_in_memory) {
        llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
        if (!arg_info.pass_by_invisible_reference) {
          ab.addByValAttr(pty);
          if (raw_param_ty->alignment > 8) {
            ab.addAlignmentAttr(llvm::Align(static_cast<uint64_t>(raw_param_ty->alignment)));
          }
        }
        param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
      } else {
        uint32_t needed_ints = 0;
        uint32_t needed_sses = 0;

        for (uint32_t i = 0; i < arg_info.num_eightbytes; i++) {
          if (arg_info.eightbyte_classes[i] == x86_64_abi_class_t::integer) needed_ints++;
          if (arg_info.eightbyte_classes[i] == x86_64_abi_class_t::sse) needed_sses++;
        }

        bool is_aggregate = is_class_struct_union_type(raw_param_ty) ||
                            raw_param_ty->kind == tk_complex;
        if (int_regs + needed_ints <= 6 && sse_regs + needed_sses <= 8) {
          int_regs += needed_ints;
          sse_regs += needed_sses;
          llvm::Type* coerced_pty = nullptr;
          LLVM_GEN_BE_CHECK(build_sysv_coerced_type(param->type, &arg_info, &coerced_pty));
          llvm_param_types.push_back(coerced_pty);
          param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
        } else if (is_aggregate) {
          // Aggregate exceeds remaining GP/SSE registers -> pass in memory via byval
          llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
          ab.addByValAttr(pty);
          param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
        } else {
          llvm_param_types.push_back(pty);
          param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
        }
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
 * @brief Constructs the LLVM Type for a va_list under System V AMD64 ABI.
 * @details The va_list type is a struct [1 x { i32, i32, i8*, i8* }] representing 
 * gp_offset, fp_offset, overflow_arg_area, and reg_save_area.
 * @param[out] out_ty Pointer to store the resulting LLVM ArrayType.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_sysv_va_list_type(
    llvm::Type** out_ty) noexcept {
  if (out_ty == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::StructType* struct_ty = llvm::StructType::get(*be_state->context, {i32_ty, i32_ty, ptr_ty, ptr_ty});
  *out_ty = llvm::ArrayType::get(struct_ty, 1);
  return llvm_gen_be_error_t::ok;
}

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

  llvm::Function* vastart_fn = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vastart);
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

  if (arg_info.pass_by_invisible_reference) {
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Value* ind_ptr = be_state->builder->CreateVAArg(va_list_ptr, ptr_ty, "vaarg.ind.ptr");
    *out_val = be_state->builder->CreateLoad(llvm_ty, ind_ptr, "vaarg.ind.val");
    return llvm_gen_be_error_t::ok;
  }

  a_type_ptr raw_ty = skip_typerefs(ty);
  llvm::BasicBlock* cur_bb = be_state->builder->GetInsertBlock();
  llvm::Function* cur_fn = cur_bb->getParent();

  if (!cur_fn) {
    if (raw_ty->alignment > 16) {
      *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty, "vaarg.aligned");
      return llvm_gen_be_error_t::ok;
    }
    *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty);
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::StructType* va_tag_ty = llvm::StructType::get(*be_state->context, {i32_ty, i32_ty, ptr_ty, ptr_ty});

  llvm::Value* ovf_ptr_gep = be_state->builder->CreateStructGEP(va_tag_ty, va_list_ptr, 2, "vaarg.ovf_ptr");

  uint32_t needed_gp = 0;
  uint32_t needed_fp = 0;
  for (uint32_t i = 0; i < arg_info.num_eightbytes; ++i) {
    if (arg_info.eightbyte_classes[i] == x86_64_abi_class_t::integer) needed_gp++;
    else if (arg_info.eightbyte_classes[i] == x86_64_abi_class_t::sse) needed_fp++;
  }

  // Memory-passed types (pass_in_memory, x87, complex_x87, or size > 16) come directly from overflow_arg_area
  if (arg_info.pass_in_memory || (needed_gp == 0 && needed_fp == 0)) {
    llvm::Value* ovf_cur = be_state->builder->CreateLoad(ptr_ty, ovf_ptr_gep, "vaarg.ovf");
    if (raw_ty->alignment > 8) {
      llvm::Value* int_ptr = be_state->builder->CreatePtrToInt(ovf_cur, i64_ty);
      uint64_t align_mask = static_cast<uint64_t>(raw_ty->alignment) - 1ULL;
      llvm::Value* aligned_int = be_state->builder->CreateAnd(
          be_state->builder->CreateAdd(int_ptr, llvm::ConstantInt::get(i64_ty, align_mask)),
          llvm::ConstantInt::get(i64_ty, ~align_mask));
      ovf_cur = be_state->builder->CreateIntToPtr(aligned_int, ptr_ty, "vaarg.ovf.aligned");
    }
    uint64_t step = ((static_cast<uint64_t>(raw_ty->size) + 7ULL) & ~7ULL);
    if (step == 0) step = 8;
    llvm::Value* ovf_next = be_state->builder->CreateGEP(
        i8_ty, ovf_cur, llvm::ConstantInt::get(i64_ty, step), "vaarg.ovf.next");
    be_state->builder->CreateStore(ovf_next, ovf_ptr_gep);
    *out_val = be_state->builder->CreateLoad(llvm_ty, ovf_cur, "vaarg.mem.val");
    return llvm_gen_be_error_t::ok;
  }

  llvm::Value* gp_off_gep = be_state->builder->CreateStructGEP(va_tag_ty, va_list_ptr, 0, "vaarg.gp_ptr");
  llvm::Value* fp_off_gep = be_state->builder->CreateStructGEP(va_tag_ty, va_list_ptr, 1, "vaarg.fp_ptr");
  llvm::Value* reg_save_gep = be_state->builder->CreateStructGEP(va_tag_ty, va_list_ptr, 3, "vaarg.reg_ptr");

  llvm::Value* gp_off = (needed_gp > 0) ? be_state->builder->CreateLoad(i32_ty, gp_off_gep, "vaarg.gp") : nullptr;
  llvm::Value* fp_off = (needed_fp > 0) ? be_state->builder->CreateLoad(i32_ty, fp_off_gep, "vaarg.fp") : nullptr;

  llvm::Value* fits_reg = nullptr;
  if (needed_gp > 0) {
    fits_reg = be_state->builder->CreateICmpULE(
        gp_off, llvm::ConstantInt::get(i32_ty, 48 - needed_gp * 8), "vaarg.fits_gp");
  }
  if (needed_fp > 0) {
    llvm::Value* fits_fp = be_state->builder->CreateICmpULE(
        fp_off, llvm::ConstantInt::get(i32_ty, 176 - needed_fp * 16), "vaarg.fits_fp");
    fits_reg = fits_reg ? be_state->builder->CreateAnd(fits_reg, fits_fp, "vaarg.fits_both") : fits_fp;
  }

  llvm::BasicBlock* in_reg_bb = llvm::BasicBlock::Create(*be_state->context, "vaarg.in_reg", cur_fn);
  llvm::BasicBlock* in_mem_bb = llvm::BasicBlock::Create(*be_state->context, "vaarg.in_mem", cur_fn);
  llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "vaarg.end", cur_fn);
  be_state->builder->CreateCondBr(fits_reg, in_reg_bb, in_mem_bb);

  // 1. In-register path
  be_state->builder->SetInsertPoint(in_reg_bb);
  llvm::Value* reg_save = be_state->builder->CreateLoad(ptr_ty, reg_save_gep, "vaarg.reg_save");
  llvm::Value* reg_addr = nullptr;
  if (arg_info.num_eightbytes <= 1) {
    llvm::Value* off_val = (needed_gp > 0) ? gp_off : fp_off;
    llvm::Value* off_64 = be_state->builder->CreateZExt(off_val, i64_ty);
    reg_addr = be_state->builder->CreateGEP(i8_ty, reg_save, off_64, "vaarg.reg_addr");
    if (needed_gp > 0) {
      be_state->builder->CreateStore(
          be_state->builder->CreateAdd(gp_off, llvm::ConstantInt::get(i32_ty, 8)), gp_off_gep);
    } else {
      be_state->builder->CreateStore(
          be_state->builder->CreateAdd(fp_off, llvm::ConstantInt::get(i32_ty, 16)), fp_off_gep);
    }
  } else {
    llvm::AllocaInst* tmp_buf = be_state->builder->CreateAlloca(
        llvm::ArrayType::get(i64_ty, 2), nullptr, "vaarg.reg.tmp");
    tmp_buf->setAlignment(llvm::Align(16));
    llvm::Value* cur_gp = gp_off;
    llvm::Value* cur_fp = fp_off;
    for (uint32_t i = 0; i < arg_info.num_eightbytes; ++i) {
      llvm::Value* slot_ptr = be_state->builder->CreateGEP(
          i8_ty, tmp_buf, llvm::ConstantInt::get(i64_ty, i * 8), "vaarg.slot");
      if (arg_info.eightbyte_classes[i] == x86_64_abi_class_t::integer) {
        llvm::Value* src_p = be_state->builder->CreateGEP(
            i8_ty, reg_save, be_state->builder->CreateZExt(cur_gp, i64_ty));
        llvm::Value* word = be_state->builder->CreateLoad(i64_ty, src_p);
        be_state->builder->CreateStore(word, slot_ptr);
        cur_gp = be_state->builder->CreateAdd(cur_gp, llvm::ConstantInt::get(i32_ty, 8));
        be_state->builder->CreateStore(cur_gp, gp_off_gep);
      } else {
        llvm::Value* src_p = be_state->builder->CreateGEP(
            i8_ty, reg_save, be_state->builder->CreateZExt(cur_fp, i64_ty));
        llvm::Value* word = be_state->builder->CreateLoad(i64_ty, src_p);
        be_state->builder->CreateStore(word, slot_ptr);
        cur_fp = be_state->builder->CreateAdd(cur_fp, llvm::ConstantInt::get(i32_ty, 16));
        be_state->builder->CreateStore(cur_fp, fp_off_gep);
      }
    }
    reg_addr = tmp_buf;
  }
  be_state->builder->CreateBr(end_bb);

  // 2. Overflow stack path
  be_state->builder->SetInsertPoint(in_mem_bb);
  llvm::Value* ovf_cur = be_state->builder->CreateLoad(ptr_ty, ovf_ptr_gep, "vaarg.ovf");
  if (raw_ty->alignment > 8) {
    llvm::Value* int_ptr = be_state->builder->CreatePtrToInt(ovf_cur, i64_ty);
    uint64_t align_mask = static_cast<uint64_t>(raw_ty->alignment) - 1ULL;
    llvm::Value* aligned_int = be_state->builder->CreateAnd(
        be_state->builder->CreateAdd(int_ptr, llvm::ConstantInt::get(i64_ty, align_mask)),
        llvm::ConstantInt::get(i64_ty, ~align_mask));
    ovf_cur = be_state->builder->CreateIntToPtr(aligned_int, ptr_ty, "vaarg.ovf.aligned");
  }
  uint64_t step = ((static_cast<uint64_t>(raw_ty->size) + 7ULL) & ~7ULL);
  llvm::Value* ovf_next = be_state->builder->CreateGEP(
      i8_ty, ovf_cur, llvm::ConstantInt::get(i64_ty, step), "vaarg.ovf.next");
  be_state->builder->CreateStore(ovf_next, ovf_ptr_gep);
  be_state->builder->CreateBr(end_bb);

  // 3. Merge & load
  be_state->builder->SetInsertPoint(end_bb);
  llvm::PHINode* addr_phi = be_state->builder->CreatePHI(ptr_ty, 2, "vaarg.addr");
  addr_phi->addIncoming(reg_addr, in_reg_bb);
  addr_phi->addIncoming(ovf_cur, in_mem_bb);
  *out_val = be_state->builder->CreateLoad(llvm_ty, addr_phi, "vaarg.val");

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

  llvm::Function* vaend_fn = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vaend);
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

  llvm::Function* vacopy_fn = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vacopy);
  llvm::Value* dest_cast = be_state->builder->CreatePointerCast(dest_va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  llvm::Value* src_cast = be_state->builder->CreatePointerCast(src_va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vacopy_fn, {dest_cast, src_cast});
  
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Dispatches to the active target architecture's ABI classifier to compute lowered function ABI metadata.
 * @details Supports System V x86_64, Windows x64, AArch64 AAPCS64, x86-32, ARM32,
 *          RISC-V 32/64, LoongArch64, MIPS, PowerPC 32/64, SystemZ (s390x), SPARC, and WebAssembly.
 * @param[in] routine_ty Pointer to the EDG `tk_routine` type descriptor.
 * @param[in] triple_str Target triple string identifying architecture and OS ABI.
 * @param[out] out_abi_info Receives the populated `llvm_abi_function_info_t` descriptor.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_compute_function_abi_info(
    a_type_ptr routine_ty,
    const std::string& triple_str,
    llvm_abi_function_info_t* out_abi_info) noexcept {
  if (routine_ty == nullptr || out_abi_info == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  *out_abi_info = {};
  llvm::Triple triple = llvm::Triple(triple_str);
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;

  if (triple.isOSWindows() && triple.getArch() == llvm::Triple::x86_64) {
    err = build_win64_function_type(routine_ty, &out_abi_info->fn_type, &out_abi_info->attrs);
  } else if (triple.isAArch64()) {
    err = build_aapcs64_function_type(routine_ty, &out_abi_info->fn_type, &out_abi_info->attrs);
  } else if (triple.getArch() == llvm::Triple::x86) {
    err = build_x86_32_function_type(routine_ty, &out_abi_info->fn_type, &out_abi_info->attrs);
  } else if (triple.isARM() || triple.isThumb()) {
    err = build_arm32_function_type(routine_ty, &out_abi_info->fn_type, &out_abi_info->attrs);
  } else if (triple.isRISCV()) {
    err = build_riscv64_function_type(routine_ty, &out_abi_info->fn_type, &out_abi_info->attrs);
  } else if (triple.isLoongArch()) {
    err = build_loongarch64_function_type(routine_ty, &out_abi_info->fn_type, &out_abi_info->attrs);
  } else if (triple.isMIPS()) {
    err = build_mips_function_type(routine_ty, &out_abi_info->fn_type, &out_abi_info->attrs);
  } else if (triple.isPPC()) {
    err = build_ppc64_function_type(routine_ty, &out_abi_info->fn_type, &out_abi_info->attrs);
  } else if (triple.isSystemZ()) {
    err = build_s390x_function_type(routine_ty, &out_abi_info->fn_type, &out_abi_info->attrs);
  } else if (triple.isSPARC()) {
    err = build_sparc_function_type(routine_ty, &out_abi_info->fn_type, &out_abi_info->attrs);
  } else if (triple.isWasm()) {
    err = build_wasm_function_type(routine_ty, &out_abi_info->fn_type, &out_abi_info->attrs);
  } else {
    err = build_sysv_function_type(routine_ty, &out_abi_info->fn_type, &out_abi_info->attrs);
  }

  if (err != llvm_gen_be_error_t::ok) {
    return err;
  }

  out_abi_info->has_sret = out_abi_info->attrs.hasParamAttr(0, llvm::Attribute::StructRet);
  if (out_abi_info->has_sret) {
    out_abi_info->return_kind = llvm_abi_passing_kind_t::indirect_sret;
  } else if (out_abi_info->fn_type->getReturnType()->isVoidTy()) {
    out_abi_info->return_kind = llvm_abi_passing_kind_t::ignore_void;
  } else {
    llvm::Type* natural_ret_ty = nullptr;
    LLVM_GEN_BE_CHECK(llvm_type_from_edg_type(routine_ty->variant.routine.return_type, &natural_ret_ty));
    out_abi_info->return_kind = (natural_ret_ty == out_abi_info->fn_type->getReturnType())
                                    ? llvm_abi_passing_kind_t::direct
                                    : llvm_abi_passing_kind_t::coerced_register;
  }

  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
