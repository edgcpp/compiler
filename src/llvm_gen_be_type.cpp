/**
 * @file llvm_gen_be_type.cpp
 * @ingroup llvm_gen_be_types
 * @brief Implementation of type translation and lowering for LLVM backend.
 * @details Translates EDG types into LLVM types, returning llvm_gen_be_error_t
 * and propagating error codes on failure.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_type.h"
#include "llvm_gen_be_ptrmem.h"
#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "target.h"
#include "cmd_line.h"
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
#include <llvm/IR/MDBuilder.h>
#include <llvm/TargetParser/Triple.h>
#include <vector>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers an EDG integer type into a matching LLVM IntegerType.
 * @details Maps integer kinds (char, short, int, long, long long, __int128, bool)
 * into appropriate bit-width LLVM integer types.
 * @param[in] edg_type Pointer to the EDG integer type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_integer(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr) {
    *out_type = llvm::Type::getInt32Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  if (is_bool_type(edg_type)) {
    *out_type = llvm::IntegerType::get(*be_state->context, targ_char_bit);
    return llvm_gen_be_error_t::ok;
  }

  switch (edg_type->variant.integer.int_kind) {
    case ik_char:
    case ik_signed_char:
    case ik_unsigned_char:
      *out_type = llvm::IntegerType::get(*be_state->context, static_cast<unsigned>(targ_char_bit));
      break;
    case ik_short:
    case ik_unsigned_short:
      *out_type = llvm::IntegerType::get(*be_state->context, static_cast<unsigned>(targ_sizeof_short * targ_char_bit));
      break;
    case ik_int:
    case ik_unsigned_int:
      *out_type = llvm::IntegerType::get(*be_state->context, static_cast<unsigned>(targ_sizeof_int * targ_char_bit));
      break;
    case ik_long:
    case ik_unsigned_long:
      *out_type = llvm::IntegerType::get(*be_state->context, static_cast<unsigned>(targ_sizeof_long * targ_char_bit));
      break;
    case ik_long_long:
    case ik_unsigned_long_long:
      *out_type = llvm::IntegerType::get(*be_state->context, static_cast<unsigned>(targ_sizeof_long_long * targ_char_bit));
      break;
#if INT128_EXTENSIONS_ALLOWED
    case ik_int128:
    case ik_unsigned_int128:
      *out_type = llvm::IntegerType::get(*be_state->context, 128);
      break;
#endif /* INT128_EXTENSIONS_ALLOWED */
    case ik_bit_precise:
    case ik_unsigned_bit_precise: {
      an_integer_type_supplement_ptr extra_info = edg_type->variant.integer.extra_info;
      if (extra_info == nullptr) {
        return llvm_gen_be_error_t::internal_inconsistency;
      }
      unsigned bw = (extra_info->bit_width > 0)
                        ? static_cast<unsigned>(extra_info->bit_width)
                        : static_cast<unsigned>(edg_type->size * targ_char_bit);
      *out_type = llvm::IntegerType::get(*be_state->context, bw);
    } break;
    default:
      *out_type = llvm::IntegerType::get(*be_state->context, static_cast<unsigned>(edg_type->size * targ_char_bit));
      break;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes the fractional bit scale, signedness, and saturation flags for an EDG fixed-point type.
 * @details Maps ISO/IEC TR 18037 `_Fract` and `_Accum` (`fpp_short`, `fpp_default`, `fpp_long`,
 *          signed and unsigned, saturating and non-saturating) to their exact fractional bit count.
 * @param[in] tp Pointer to the EDG `tk_fixed_point` type structure.
 * @param[out] out_frac_bits Receives the number of fractional bits (`F`).
 * @param[out] out_is_signed Receives `true` if the fixed-point type is signed.
 * @param[out] out_is_saturating Receives `true` if the fixed-point type is saturating (`_Sat`).
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null or not `tk_fixed_point`.
 */
llvm_gen_be_error_t llvm_get_fixed_point_scale(
    a_type_ptr tp,
    unsigned* out_frac_bits,
    bool* out_is_signed,
    bool* out_is_saturating) noexcept {
  if (!tp || !out_frac_bits || !out_is_signed || !out_is_saturating) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  a_type_ptr raw_tp = skip_typerefs(tp);
#if FIXED_POINT_ALLOWED
  if (raw_tp->kind != tk_fixed_point) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  const a_fixed_point_type_descr& fxd = raw_tp->variant.fixed_point;
  bool is_unsigned = (fxd.is_unsigned != 0);
  *out_is_signed = !is_unsigned;
  *out_is_saturating = (fxd.saturating != 0);
  unsigned base_frac = 15;
  if (fxd.precision == fpp_short) {
    base_frac = 7;
  } else if (fxd.precision == fpp_long) {
    base_frac = 31;
  }
  *out_frac_bits = is_unsigned ? (base_frac + 1) : base_frac;
  return llvm_gen_be_error_t::ok;
#else
  (void)raw_tp;
  return llvm_gen_be_error_t::invalid_argument;
#endif
}

/**
 * @brief Lowers an EDG floating-point type into a matching LLVM floating-point Type.
 * @details Handles half/fp16, bfloat16, float, double, x86_fp80, and fp128.
 * @param[in] edg_type Pointer to the EDG floating-point type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_float(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr) {
    *out_type = llvm::Type::getDoubleTy(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  switch (edg_type->variant.float_kind) {
    case fk_float16:
    case fk_fp16:
    case fk_std_float16:
      *out_type = llvm::Type::getHalfTy(*be_state->context);
      break;
    case fk_std_bfloat16:
      *out_type = llvm::Type::getBFloatTy(*be_state->context);
      break;
    case fk_float:
    case fk_std_float32:
    case fk_float32x:
      *out_type = llvm::Type::getFloatTy(*be_state->context);
      break;
    case fk_double:
    case fk_std_float64:
    case fk_float64x:
      *out_type = llvm::Type::getDoubleTy(*be_state->context);
      break;
    case fk_float80:
      *out_type = llvm::Type::getX86_FP80Ty(*be_state->context);
      break;
    case fk_float128:
    case fk_std_float128: {
      llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
      if (triple.isPPC()) {
        *out_type = llvm::Type::getPPC_FP128Ty(*be_state->context);
      } else {
        *out_type = llvm::Type::getFP128Ty(*be_state->context);
      }
      break;
    }
    case fk_long_double: {
      llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
      if (edg_type->size * targ_char_bit == 80) {
        *out_type = llvm::Type::getX86_FP80Ty(*be_state->context);
      } else if (edg_type->size * targ_char_bit == 128) {
        if (triple.isPPC()) {
          *out_type = llvm::Type::getPPC_FP128Ty(*be_state->context);
        } else {
          *out_type = llvm::Type::getFP128Ty(*be_state->context);
        }
      } else {
        *out_type = llvm::Type::getDoubleTy(*be_state->context);
      }
      break;
    }
    default:
      *out_type = llvm::Type::getDoubleTy(*be_state->context);
      return llvm_gen_be_error_t::unsupported_type;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an EDG pointer or reference type into an LLVM opaque PointerType.
 * @details Constructs an opaque pointer using LLVM's unadorned pointer representation.
 * Supports non-default address spaces (e.g. __attribute__((address_space(N)))).
 * @param[in] edg_type Pointer to the EDG pointer type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_pointer(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  unsigned addr_space = 0;
#if NAMED_ADDRESS_SPACES_ALLOWED
  if (edg_type != nullptr && named_address_spaces_enabled) {
    a_type_ptr pointee = edg_type->variant.pointer.type;
    if (pointee != nullptr) {
      a_type_qualifier_set tqs = f_get_type_qualifiers(pointee, FALSE);
      addr_space = named_address_space_from_qualifier_set(tqs);
    }
  }
#endif
  *out_type = llvm::PointerType::get(*be_state->context, addr_space);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an EDG array type into an LLVM ArrayType or PointerType.
 * @details Maps fixed-size arrays to llvm::ArrayType, zero-bound arrays to 0-length arrays,
 * and variable-length or incomplete arrays to opaque pointers.
 * @param[in] edg_type Pointer to the EDG array type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_array(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr) {
    *out_type = llvm::PointerType::getUnqual(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  if (!edg_type->variant.array.is_vla && !edg_type->variant.array.bound_is_zero &&
      !edg_type->variant.array.is_variable_size_array &&
      !edg_type->variant.array.is_template_dependent_size_array &&
      !edg_type->incomplete) {
    llvm::Type* elem_ty = nullptr;
    llvm_gen_be_error_t err = llvm_type_from_edg_type(edg_type->variant.array.element_type, &elem_ty);
    if (err != llvm_gen_be_error_t::ok) {
      return err;
    }
    *out_type = llvm::ArrayType::get(elem_ty, static_cast<uint64_t>(edg_type->variant.array.variant.number_of_elements));
  } else if (edg_type->variant.array.bound_is_zero || edg_type->incomplete) {
    llvm::Type* elem_ty = nullptr;
    llvm_gen_be_error_t err = llvm_type_from_edg_type(edg_type->variant.array.element_type, &elem_ty);
    if (err != llvm_gen_be_error_t::ok) {
      return err;
    }
    *out_type = llvm::ArrayType::get(elem_ty, 0);
  } else {
    *out_type = llvm::PointerType::getUnqual(*be_state->context);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an EDG vector type into an LLVM VectorType.
 * @details Translates GNU/Clang vector types into llvm::FixedVectorType.
 * @param[in] edg_type Pointer to the EDG vector type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_vector(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
  if (edg_type == nullptr || edg_type->kind != tk_vector) {
    *out_type = llvm::PointerType::getUnqual(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }
  llvm::Type* elem_ty = nullptr;
  llvm_gen_be_error_t err = llvm_type_from_edg_type(edg_type->variant.vector.element_type, &elem_ty);
  if (err != llvm_gen_be_error_t::ok) {
    return err;
  }
  a_targ_size_t elem_size = skip_typerefs(edg_type->variant.vector.element_type)->size;
  unsigned num_elements = (elem_size > 0)
                              ? std::max<unsigned>(1U, static_cast<unsigned>(edg_type->size / elem_size))
                              : 1U;
  *out_type = llvm::FixedVectorType::get(elem_ty, num_elements);
  return llvm_gen_be_error_t::ok;
#else
  *out_type = llvm::PointerType::getUnqual(*be_state->context);
  return llvm_gen_be_error_t::ok;
#endif
}

/**
 * @brief Lowers an EDG struct, union, or class type into an LLVM StructType.
 * @details Generates packed structures matching EDG field byte offsets and tail padding.
 * Cyclic struct references are resolved using forward-declared opaque StructTypes.
 * @param[in] edg_type Pointer to the EDG struct, union, or class type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_struct(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr) {
    *out_type = llvm::StructType::get(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  if (edg_type->variant.class_struct_union.extra_info &&
      edg_type->variant.class_struct_union.extra_info->is_va_list_tag) {
    llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
    if (triple.isOSWindows() || triple.isRISCV() || triple.isLoongArch() ||
        triple.isMIPS() || triple.isPPC() || triple.isARM() || triple.isThumb() ||
        triple.getArch() == llvm::Triple::x86 || triple.isSPARC() || triple.isWasm()) {
      return build_win64_va_list_type(out_type);
    }
    if (triple.isAArch64()) {
      return build_aapcs64_va_list_type(out_type);
    }
    if (triple.isSystemZ()) {
      return build_s390x_va_list_type(out_type);
    }
    return build_sysv_va_list_type(out_type);
  }

  llvm::StructType* struct_ty = nullptr;
  auto it = be_state->type_cache.find(edg_type);
  if (it != be_state->type_cache.end()) {
    struct_ty = llvm::cast<llvm::StructType>(it->second);
    if (!struct_ty->isOpaque() || edg_type->incomplete) {
      *out_type = struct_ty;
      return llvm_gen_be_error_t::ok;
    }
  } else {
    struct_ty = llvm::StructType::create(*be_state->context);
    be_state->type_cache[edg_type] = struct_ty;
  }

  if (edg_type->incomplete) {
    *out_type = struct_ty;
    return llvm_gen_be_error_t::ok;
  }

  if (edg_type->kind == tk_union) {
    std::vector<llvm::Type*> elem_tys;
    llvm::Type* carrier_elem = nullptr;
    uint64_t carrier_size = 0;
    uint64_t max_align = 0;
    for (a_field_ptr field = edg_type->variant.class_struct_union.field_list;
         field != nullptr; field = field->next) {
      llvm::Type* cand_ty = nullptr;
      llvm_gen_be_error_t err = llvm_type_from_edg_type(field->type, &cand_ty);
      if (err != llvm_gen_be_error_t::ok) {
        return err;
      }
      uint64_t f_sz = be_state->module->getDataLayout().getTypeAllocSize(cand_ty);
      uint64_t f_align = be_state->module->getDataLayout().getABITypeAlign(cand_ty).value();
      if (f_align > max_align || (f_align == max_align && f_sz > carrier_size)) {
        max_align = f_align;
        carrier_size = f_sz;
        carrier_elem = cand_ty;
      }
    }
    if (carrier_elem != nullptr) {
      elem_tys.push_back(carrier_elem);
      if (carrier_size < static_cast<uint64_t>(edg_type->size)) {
        elem_tys.push_back(llvm::ArrayType::get(
            llvm::Type::getInt8Ty(*be_state->context),
            static_cast<uint64_t>(edg_type->size) - carrier_size));
      }
    } else if (edg_type->size > 0) {
      elem_tys.push_back(llvm::ArrayType::get(
          llvm::Type::getInt8Ty(*be_state->context),
          static_cast<uint64_t>(edg_type->size)));
    }
    struct_ty->setBody(elem_tys, /*isPacked=*/false);
    *out_type = struct_ty;
    return llvm_gen_be_error_t::ok;
  }

  // 1. Determine if the EDG struct is packed by checking for misaligned fields
  bool is_packed = false;
  for (a_field_ptr f = edg_type->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
    if (f->is_bit_field || !f->type) continue;
    a_type_ptr f_ut = skip_typerefs(f->type);
    if (f->is_optimized_empty_class ||
        (f->next != nullptr && f->next->offset == f->offset &&
         (f_ut->kind == tk_struct || f_ut->kind == tk_class) &&
         is_empty_class_type(f_ut))) {
      continue;
    }
    llvm::Type* fty = nullptr;
    llvm_gen_be_error_t f_err = llvm_type_from_edg_type(f->type, &fty);
    if (f_err != llvm_gen_be_error_t::ok) {
      return f_err;
    }
    uint64_t align = be_state->module->getDataLayout().getABITypeAlign(fty).value();
    if (f->offset % align != 0) {
      is_packed = true;
      break;
    }
  }

  /* Struct / Class */
  std::vector<llvm::Type*> elem_tys;
  uint64_t current_offset = 0;
  for (a_field_ptr field = edg_type->variant.class_struct_union.field_list;
       field != nullptr; field = field->next) {
    if (!field->type) continue;
    a_type_ptr field_ut = skip_typerefs(field->type);
    if (!field->is_bit_field &&
        (field->is_optimized_empty_class ||
         (field->next != nullptr && field->next->offset == field->offset &&
          (field_ut->kind == tk_struct || field_ut->kind == tk_class) &&
          is_empty_class_type(field_ut)))) {
      continue;
    }
    a_targ_size_t field_size = field_ut->size;
    if (field->offset > current_offset) {
      elem_tys.push_back(llvm::ArrayType::get(
          llvm::Type::getInt8Ty(*be_state->context),
          static_cast<uint64_t>(field->offset) - current_offset));
      current_offset = static_cast<uint64_t>(field->offset);
    } else if (field->offset < current_offset) {
      if (!field->is_bit_field &&
          static_cast<uint64_t>(field->offset + field_size) > current_offset) {
        uint64_t extra_bytes = static_cast<uint64_t>(field->offset + field_size) - current_offset;
        elem_tys.push_back(llvm::ArrayType::get(
            llvm::Type::getInt8Ty(*be_state->context), extra_bytes));
        current_offset += extra_bytes;
      }
      continue; // Handled as part of a previous bitfield run or overlapping [[no_unique_address]] subobject
    }

    if (field->is_bit_field) {
      if (field->bit_size == 0) continue;

      llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
      bool is_msvc = triple.isWindowsMSVCEnvironment();
      
      uint64_t container_bytes = (field_size > 0) ? static_cast<uint64_t>(field_size) : 1ULL;
      uint64_t run_byte_end = current_offset + container_bytes;

      if (!is_msvc) {
        // GCC/SysV rules: run spans overlapping bits
        run_byte_end = static_cast<uint64_t>((field->offset * 8 + field->offset_bit_remainder + field->bit_size + 7) / 8);
        a_field_ptr next_bf = field->next;
        while (next_bf != nullptr && next_bf->is_bit_field && next_bf->bit_size > 0 && next_bf->offset < run_byte_end) {
          uint64_t next_bit_end = static_cast<uint64_t>(next_bf->offset * 8 + next_bf->offset_bit_remainder + next_bf->bit_size);
          uint64_t next_byte_end = (next_bit_end + 7) / 8;
          if (next_byte_end > run_byte_end) run_byte_end = next_byte_end;
          next_bf = next_bf->next;
        }
        
        // Round container_bytes up to the nearest power of 2 that covers the span
        uint64_t span = run_byte_end - current_offset;
        container_bytes = 1;
        while (container_bytes < span) container_bytes *= 2;
      }
      
      uint64_t span_bytes = run_byte_end - current_offset;
      bool pow2_scalar = (span_bytes == 1 || span_bytes == 2 || span_bytes == 4 || span_bytes == 8) &&
                         (is_packed || (current_offset % span_bytes == 0));
      if (pow2_scalar) {
        elem_tys.push_back(llvm::IntegerType::get(*be_state->context, static_cast<unsigned>(span_bytes * 8)));
      } else {
        elem_tys.push_back(llvm::ArrayType::get(
            llvm::Type::getInt8Ty(*be_state->context), span_bytes));
      }
      current_offset = run_byte_end;
      continue;
    }

    llvm::Type* ty = nullptr;
    LLVM_GEN_BE_CHECK(llvm_type_from_edg_type(field->type, &ty));
    elem_tys.push_back(ty);
    current_offset += static_cast<uint64_t>(field_size);
  }

  if (current_offset < static_cast<uint64_t>(edg_type->size)) {
    elem_tys.push_back(llvm::ArrayType::get(
        llvm::Type::getInt8Ty(*be_state->context),
        static_cast<uint64_t>(edg_type->size) - current_offset));
  }

  if (!is_packed && edg_type->size > 0) {
    uint64_t final_max_align = 1;
    for (llvm::Type* et : elem_tys) {
      uint64_t ea = be_state->module->getDataLayout().getABITypeAlign(et).value();
      if (ea > final_max_align) final_max_align = ea;
    }
    if (static_cast<uint64_t>(edg_type->size) % final_max_align != 0) {
      is_packed = true;
    }
  }

  struct_ty->setBody(elem_tys, is_packed);
  *out_type = struct_ty;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Dispatches a single EDG `tk_routine` descriptor to the active target architecture's ABI builder.
 * @details Supports Win64 x86_64, AAPCS64, SysV x86_64, x86-32, ARM32, RISC-V, LoongArch, MIPS, PowerPC,
 *          SystemZ, SPARC, WebAssembly, and generic fallback targets, ensuring narrow `bool`/`i8`/`i16`
 *          `ZExt`/`SExt` attributes are attached consistently.
 * @param[in] edg_type Pointer to the EDG `tk_routine` type structure.
 * @param[out] out_fn_ty Receives the target-lowered `llvm::FunctionType*`.
 * @param[out] out_attrs Receives the target-lowered `llvm::AttributeList`.
 * @return `llvm_gen_be_error_t::ok` on success, or an appropriate error code.
 */
static llvm_gen_be_error_t dispatch_single_routine_target_abi(
    a_type_ptr edg_type,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {
  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;

  if (triple.isOSWindows() && triple.getArch() == llvm::Triple::x86_64) {
    err = build_win64_function_type(edg_type, out_fn_ty, out_attrs);
  } else if (triple.getArch() == llvm::Triple::aarch64 || triple.getArch() == llvm::Triple::aarch64_be) {
    err = build_aapcs64_function_type(edg_type, out_fn_ty, out_attrs);
  } else if (triple.getArch() == llvm::Triple::x86_64) {
    err = build_sysv_function_type(edg_type, out_fn_ty, out_attrs);
  } else if (triple.getArch() == llvm::Triple::x86) {
    err = build_x86_32_function_type(edg_type, out_fn_ty, out_attrs);
  } else if (triple.getArch() == llvm::Triple::arm || triple.getArch() == llvm::Triple::thumb) {
    err = build_arm32_function_type(edg_type, out_fn_ty, out_attrs);
  } else if (triple.getArch() == llvm::Triple::riscv64 || triple.getArch() == llvm::Triple::riscv32) {
    err = build_riscv64_function_type(edg_type, out_fn_ty, out_attrs);
  } else if (triple.getArch() == llvm::Triple::loongarch64 || triple.getArch() == llvm::Triple::loongarch32) {
    err = build_loongarch64_function_type(edg_type, out_fn_ty, out_attrs);
  } else if (triple.isMIPS()) {
    err = build_mips_function_type(edg_type, out_fn_ty, out_attrs);
  } else if (triple.isPPC64() || triple.isPPC32()) {
    err = build_ppc64_function_type(edg_type, out_fn_ty, out_attrs);
  } else if (triple.getArch() == llvm::Triple::systemz) {
    err = build_s390x_function_type(edg_type, out_fn_ty, out_attrs);
  } else if (triple.getArch() == llvm::Triple::sparc || triple.getArch() == llvm::Triple::sparcv9 || triple.getArch() == llvm::Triple::sparcel) {
    err = build_sparc_function_type(edg_type, out_fn_ty, out_attrs);
  } else if (triple.isWasm()) {
    err = build_wasm_function_type(edg_type, out_fn_ty, out_attrs);
  } else {
    a_type_ptr ret_ty = edg_type->variant.routine.return_type;
    llvm::Type* llvm_ret_ty = nullptr;
    err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
    if (err != llvm_gen_be_error_t::ok) {
      return err;
    }
    std::vector<llvm::Type*> param_tys;
    if (edg_type->variant.routine.extra_info != nullptr) {
      for (a_param_type_ptr param = edg_type->variant.routine.extra_info->param_type_list;
           param != nullptr; param = param->next) {
        llvm::Type* pty = nullptr;
        err = llvm_type_from_edg_type(param->type, &pty);
        if (err != llvm_gen_be_error_t::ok) {
          return err;
        }
        param_tys.push_back(pty);
      }
    }
    bool is_vararg = (edg_type->variant.routine.extra_info != nullptr)
                         ? (edg_type->variant.routine.extra_info->has_ellipsis != FALSE)
                         : false;
    *out_fn_ty = llvm::FunctionType::get(llvm_ret_ty, param_tys, is_vararg);
    *out_attrs = llvm::AttributeList();
  }
  return err;
}

/**
 * @brief Builds the target-ABI coerced `llvm::FunctionType*` and `llvm::AttributeList` for an EDG routine type.
 * @details Unifies ABI coercion (`sret`, `byval`, `inreg`, `zext`/`sext` on narrow `i8`/`i16`/`bool` integers,
 *          HFA/eightbyte packing, and variadic parameter expansion) across function definitions, direct calls,
 *          indirect function-pointer calls, and virtual dispatch/thunks.
 * @param[in] routine_edg_type Pointer to the EDG `tk_routine` type descriptor.
 * @param[in] is_variadic_call True when building an expanded call-site signature for a variadic (`...`) call.
 * @param[in] num_fixed_args Number of fixed (non-variadic) formal parameters.
 * @param[in] call_arg_types EDG types of all actual call-site arguments (used for variadic `...` classification).
 * @param[out] out_fn_ty Receives the target-ABI lowered `llvm::FunctionType*`.
 * @param[out] out_attrs Receives the target-ABI `llvm::AttributeList`.
 * @return `llvm_gen_be_error_t::ok` on success, or an error code on invalid/unsupported types.
 */
llvm_gen_be_error_t llvm_build_target_abi_function_type(
    a_type_ptr routine_edg_type,
    bool is_variadic_call,
    unsigned num_fixed_args,
    const std::vector<a_type_ptr>& call_arg_types,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {
  if (!routine_edg_type || !out_fn_ty || !out_attrs) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  a_type_ptr raw_rout = skip_typerefs(routine_edg_type);
  if (raw_rout->kind != tk_routine) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  if (!is_variadic_call || call_arg_types.size() <= num_fixed_args) {
    return dispatch_single_routine_target_abi(raw_rout, out_fn_ty, out_attrs);
  }

  // Synthesize an expanded parameter list covering both fixed and variadic (...) arguments
  a_type synth_rout = *raw_rout;
  a_routine_type_supplement synth_extra = {};
  if (raw_rout->variant.routine.extra_info != nullptr) {
    synth_extra = *raw_rout->variant.routine.extra_info;
  }
  std::vector<a_param_type> synth_params(call_arg_types.size());
  std::vector<a_type> promoted_types(call_arg_types.size());
  unsigned idx = 0;
  a_param_type_ptr fixed_cur = synth_extra.param_type_list;
  for (; idx < num_fixed_args && fixed_cur != nullptr; ++idx, fixed_cur = fixed_cur->next) {
    synth_params[idx] = *fixed_cur;
  }
  for (; idx < call_arg_types.size(); ++idx) {
    synth_params[idx] = {};
    a_type_ptr arg_t = call_arg_types[idx];
    a_type_ptr raw_a = arg_t ? skip_typerefs(arg_t) : nullptr;
    if (raw_a && raw_a->kind == tk_integer && raw_a->size < targ_sizeof_int) {
      promoted_types[idx] = *raw_a;
      promoted_types[idx].variant.integer.bool_type = FALSE;
      promoted_types[idx].variant.integer.int_kind = ik_int;
      promoted_types[idx].size = targ_sizeof_int;
      arg_t = &promoted_types[idx];
    } else if (raw_a && raw_a->kind == tk_float &&
               (raw_a->variant.float_kind == fk_float ||
                raw_a->variant.float_kind == fk_float16 ||
                raw_a->variant.float_kind == fk_std_float16 ||
                raw_a->variant.float_kind == fk_std_bfloat16)) {
      promoted_types[idx] = *raw_a;
      promoted_types[idx].variant.float_kind = fk_double;
      promoted_types[idx].size = 8;
      arg_t = &promoted_types[idx];
    }
    synth_params[idx].type = arg_t;
  }
  for (size_t k = 0; k < synth_params.size(); ++k) {
    synth_params[k].next = (k + 1 < synth_params.size()) ? &synth_params[k + 1] : nullptr;
  }
  synth_extra.param_type_list = &synth_params[0];
  synth_extra.has_ellipsis = TRUE;
  synth_rout.variant.routine.extra_info = &synth_extra;
  return dispatch_single_routine_target_abi(&synth_rout, out_fn_ty, out_attrs);
}

/**
 * @brief Computes both the callee declaration `FunctionType*` and the full call-site coerced parameter/attribute schedule.
 * @details For non-variadic calls, `out_call_param_tys` matches `(*out_callee_fn_ty)->params()`. For variadic (`...`)
 *          calls with extra arguments beyond @p num_fixed_args, `*out_callee_fn_ty` retains the fixed variadic prototype
 *          while `out_call_param_tys` and `out_call_attrs` include target-ABI coerced types and `byval`/`zext`/`sext`
 *          attributes for every variadic argument.
 * @param[in] routine_edg_type Pointer to the EDG `tk_routine` type descriptor.
 * @param[in] is_variadic_call True when the routine has an ellipsis (`...`).
 * @param[in] num_fixed_args Number of fixed formal parameters before `...`.
 * @param[in] call_arg_types EDG types of all actual call-site arguments.
 * @param[out] out_callee_fn_ty Receives the callee `llvm::FunctionType*`.
 * @param[out] out_call_param_tys Receives the full list of expected LLVM parameter types for all call-site arguments.
 * @param[out] out_call_attrs Receives the combined call-site `llvm::AttributeList`.
 * @return `llvm_gen_be_error_t::ok` on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_build_target_abi_call_schedule(
    a_type_ptr routine_edg_type,
    bool is_variadic_call,
    unsigned num_fixed_args,
    const std::vector<a_type_ptr>& call_arg_types,
    llvm::FunctionType** out_callee_fn_ty,
    std::vector<llvm::Type*>* out_call_param_tys,
    llvm::AttributeList* out_call_attrs) noexcept {
  if (!routine_edg_type || !out_callee_fn_ty || !out_call_param_tys || !out_call_attrs) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  llvm::AttributeList fixed_attrs;
  LLVM_GEN_BE_CHECK(llvm_build_target_abi_function_type(
      routine_edg_type, false, num_fixed_args, {}, out_callee_fn_ty, &fixed_attrs));
  if (!is_variadic_call || call_arg_types.size() <= num_fixed_args) {
    out_call_param_tys->assign(
        (*out_callee_fn_ty)->param_begin(), (*out_callee_fn_ty)->param_end());
    *out_call_attrs = fixed_attrs;
    return llvm_gen_be_error_t::ok;
  }
  llvm::FunctionType* expanded_fn_ty = nullptr;
  LLVM_GEN_BE_CHECK(llvm_build_target_abi_function_type(
      routine_edg_type, true, num_fixed_args, call_arg_types, &expanded_fn_ty, out_call_attrs));
  out_call_param_tys->assign(
      expanded_fn_ty->param_begin(), expanded_fn_ty->param_end());
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an EDG routine (function) type into an LLVM FunctionType.
 * @details Gathers return type, parameter types, and variadic ellipsis flag.
 * Dispatches to `llvm_build_target_abi_function_type` to perform target ABI adjustments.
 * @param[in] edg_type Pointer to the EDG routine type structure.
 * @param[out] out_fn_type Pointer to the variable where the resulting FunctionType* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_routine(
    a_type_ptr edg_type,
    llvm::FunctionType** out_fn_type) noexcept {
  if (out_fn_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr) {
    *out_fn_type = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
    return llvm_gen_be_error_t::ok;
  }
  llvm::AttributeList dummy_attrs;
  return llvm_build_target_abi_function_type(
      edg_type, false, 0, {}, out_fn_type, &dummy_attrs);
}

/**
 * @brief Translates an EDG type representation into a corresponding LLVM type.
 * @details Traverses the EDG type structure, looks up or populates the type cache,
 * and creates LLVM primitive, pointer, array, aggregate, or routine types.
 * @param[in] edg_type Pointer to the EDG type description structure. If null, produces void.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, llvm_gen_be_error_t::invalid_argument if
 *         out_type is null, or an error code indicating the failure reason.
 */
llvm_gen_be_error_t llvm_type_from_edg_type(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr) {
    *out_type = llvm::Type::getVoidTy(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }

  auto it = be_state->type_cache.find(edg_type);
  if (it != be_state->type_cache.end()) {
    llvm::Type* cached_ty = it->second;
    bool needs_completion = false;
    if (auto* struct_ty = llvm::dyn_cast<llvm::StructType>(cached_ty)) {
      if (struct_ty->isOpaque() && !edg_type->incomplete) {
        needs_completion = true;
      }
    }
    if (!needs_completion) {
      *out_type = cached_ty;
      return llvm_gen_be_error_t::ok;
    }
  }

  llvm::Type* llvm_ty = nullptr;
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;

  switch (edg_type->kind) {
    case tk_void:
      llvm_ty = llvm::Type::getVoidTy(*be_state->context);
      break;
    case tk_nullptr:
      llvm_ty = llvm::PointerType::getUnqual(*be_state->context);
      break;
    case tk_integer:
      err = llvm_type_from_integer(edg_type, &llvm_ty);
      break;
#if FIXED_POINT_ALLOWED
    case tk_fixed_point: {
      unsigned bits = (edg_type->size > 0) ? static_cast<unsigned>(edg_type->size * 8) : 32;
      llvm_ty = llvm::IntegerType::get(*be_state->context, bits);
    } break;
#endif
    case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
#endif
      err = llvm_type_from_float(edg_type, &llvm_ty);
      break;
    case tk_complex: {
      llvm::Type* elem_ty = nullptr;
      err = llvm_type_from_float(edg_type, &elem_ty);
      if (err == llvm_gen_be_error_t::ok) {
        llvm_ty = llvm::StructType::get(*be_state->context, {elem_ty, elem_ty});
      }
    } break;
    case tk_pointer:
      err = llvm_type_from_pointer(edg_type, &llvm_ty);
      break;
    case tk_array:
      err = llvm_type_from_array(edg_type, &llvm_ty);
      break;
#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      err = llvm_type_from_vector(edg_type, &llvm_ty);
      break;
    case tk_scalable_vector: {
      llvm::Type* elem_ty = nullptr;
      err = llvm_type_from_edg_type(edg_type->variant.scalable_vector.element_type, &elem_ty);
      if (err == llvm_gen_be_error_t::ok) {
        // SVE scalable vectors have a minimum of 128 bits (16 bytes)
        unsigned elem_bytes = static_cast<unsigned>(be_state->module->getDataLayout().getTypeStoreSize(elem_ty));
        unsigned min_elts = (elem_bytes > 0) ? std::max<unsigned>(1U, 16U / elem_bytes) : 1U;
        llvm::Type* sv_ty = llvm::ScalableVectorType::get(elem_ty, min_elts);
        unsigned tuple_elements = edg_type->variant.scalable_vector.tuple_elements;
        if (tuple_elements > 1) {
            std::vector<llvm::Type*> elements(tuple_elements, sv_ty);
            llvm_ty = llvm::StructType::get(*be_state->context, elements);
        } else {
            llvm_ty = sv_ty;
        }
      }
    } break;
    case tk_riscv_vector: {
      llvm::Type* elem_ty = nullptr;
      err = llvm_type_from_edg_type(edg_type->variant.riscv_vector.element_type, &elem_ty);
      if (err == llvm_gen_be_error_t::ok) {
        // RVV scalable vectors have a minimum of 64 bits (8 bytes) per m1
        unsigned elem_bytes = static_cast<unsigned>(be_state->module->getDataLayout().getTypeStoreSize(elem_ty));
        // Base minimum elements for LMUL=1 (m1) is 8 bytes total
        unsigned min_elts = (elem_bytes > 0) ? (8U / elem_bytes) : 0U;
        
        int8_t lmul = edg_type->variant.riscv_vector.length_multiplier;
        if (lmul > 0) {
            min_elts *= static_cast<unsigned>(lmul);
        } else if (lmul < 0) {
            // Fractional LMUL, e.g. -2 means mf2 (1/2)
            min_elts /= static_cast<unsigned>(-lmul);
            if (min_elts == 0) min_elts = 1; // At least 1 element
        }
        
        llvm::Type* sv_ty = llvm::ScalableVectorType::get(elem_ty, min_elts);
        unsigned tuple_elements = edg_type->variant.riscv_vector.tuple_elements;
        if (tuple_elements > 1) {
            std::vector<llvm::Type*> elements(tuple_elements, sv_ty);
            llvm_ty = llvm::StructType::get(*be_state->context, elements);
        } else {
            llvm_ty = sv_ty;
        }
      }
    } break;
    case tk_scalable_vector_count:
      // Opaque count type, often represented as boolean scalable vector for predicates
      llvm_ty = llvm::ScalableVectorType::get(llvm::Type::getInt1Ty(*be_state->context), 16);
      break;
#endif
    case tk_ptr_to_member:
      err = llvm_type_from_ptr_to_member(edg_type, &llvm_ty);
      break;
    case tk_routine: {
      llvm::FunctionType* fn_ty = nullptr;
      err = llvm_type_from_routine(edg_type, &fn_ty);
      llvm_ty = fn_ty;
    } break;
    case tk_struct:
    case tk_class:
    case tk_union:
      err = llvm_type_from_struct(edg_type, &llvm_ty);
      break;
    case tk_typeref: {
      unsigned dec_bytes = 0;
      LLVM_GEN_BE_CHECK(llvm_get_decimal_float_byte_width(edg_type, &dec_bytes));
      if (dec_bytes != 0) {
        err = llvm_lower_decimal_float_type(dec_bytes, &llvm_ty);
      } else {
        err = llvm_type_from_edg_type(edg_type->variant.typeref.type, &llvm_ty);
      }
    } break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_mfp8:
    case tk_float8e4m3:
    case tk_float8e5m2:
      llvm_ty = llvm::Type::getInt8Ty(*be_state->context);
      break;
#endif
    default:
      llvm_ty = llvm::Type::getInt8Ty(*be_state->context);
      err = llvm_gen_be_error_t::unsupported_type;
      break;
  }

  if (err != llvm_gen_be_error_t::ok) {
    return err;
  }

  be_state->type_cache[edg_type] = llvm_ty;
  *out_type = llvm_ty;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Maps an EDG routine type's calling convention to an LLVM CallingConv::ID.
 * @details Examines the routine type supplement and maps conventions like cdecl, fastcall, stdcall, etc.
 * @param[in] edg_type Pointer to the EDG routine type structure.
 * @param[out] out_cc Pointer to the variable where the resulting LLVM calling convention ID is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_calling_conv_from_routine(
    a_type_ptr edg_type,
    unsigned* out_cc) noexcept {
  if (out_cc == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (edg_type == nullptr || edg_type->kind != tk_routine) {
    *out_cc = llvm::CallingConv::C;
    return llvm_gen_be_error_t::ok;
  }

#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
  a_routine_type_supplement_ptr supp = edg_type->variant.routine.extra_info;
  if (supp != nullptr) {
    switch (supp->calling_convention) {
      case cc_default:
      case cc_cdecl:
        *out_cc = llvm::CallingConv::C;
        break;
      case cc_fastcall:
        *out_cc = llvm::CallingConv::X86_FastCall;
        break;
      case cc_stdcall:
        *out_cc = llvm::CallingConv::X86_StdCall;
        break;
      case cc_thiscall:
        *out_cc = llvm::CallingConv::X86_ThisCall;
        break;
      case cc_vectorcall:
        *out_cc = llvm::CallingConv::X86_VectorCall;
        break;
      case cc_clrcall:
        *out_cc = llvm::CallingConv::C;
        return llvm_gen_be_error_t::abi_classification_failed;
      default:
        *out_cc = llvm::CallingConv::C;
        break;
    }
  } else {
    *out_cc = llvm::CallingConv::C;
  }
#else
  *out_cc = llvm::CallingConv::C;
#endif

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Legacy compatibility helper redirecting to llvm_type_from_edg_type.
 * @details Convenience wrapper returning llvm::Type* while inspecting error codes internally.
 * @param[in] edg_type Pointer to the EDG type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t get_llvm_type(a_type_ptr edg_type, llvm::Type** out_type) noexcept {
  if (out_type == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  llvm::Type* ty = nullptr;
  llvm_gen_be_error_t err = llvm_type_from_edg_type(edg_type, &ty);
  if (err != llvm_gen_be_error_t::ok) {
    return err;
  }
  *out_type = ty;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Constructs or retrieves the LLVM TBAA type descriptor `MDNode` for an EDG type.
 * @details Creates the `"Simple C/C++ TBAA"` root, `"omnipotent char"` node, scalar type nodes,
 *          and struct path descriptor nodes using `llvm::MDBuilder`.
 * @param[in] type Pointer to the EDG type structure.
 * @param[out] out_node Receives the constructed `llvm::MDNode*` type descriptor.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_node` is null.
 */
llvm_gen_be_error_t llvm_get_tbaa_type_node(
    a_type_ptr type,
    llvm::MDNode** out_node) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_node);
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  llvm::MDBuilder mdb{*be_state->context};
  llvm::MDNode* root = mdb.createTBAARoot("Simple C/C++ TBAA");
  llvm::MDNode* char_node = mdb.createTBAAScalarTypeNode("omnipotent char", root, 0);
  if (!type) {
    *out_node = char_node;
    return llvm_gen_be_error_t::ok;
  }
  a_type_ptr tp = skip_typerefs(type);
  if (tp->kind == tk_integer) {
    if (tp->size <= 1) {
      *out_node = char_node;
    } else if (tp->size == 2) {
      *out_node = mdb.createTBAAScalarTypeNode("short", char_node, 0);
    } else if (tp->size == 4) {
      *out_node = mdb.createTBAAScalarTypeNode("int", char_node, 0);
    } else {
      *out_node = mdb.createTBAAScalarTypeNode("long long", char_node, 0);
    }
    return llvm_gen_be_error_t::ok;
  }
  if (tp->kind == tk_float) {
    const char* fname = (tp->size == 4) ? "float" : "double";
    *out_node = mdb.createTBAAScalarTypeNode(fname, char_node, 0);
    return llvm_gen_be_error_t::ok;
  }
  if (tp->kind == tk_pointer || tp->kind == tk_nullptr) {
    *out_node = mdb.createTBAAScalarTypeNode("any pointer", char_node, 0);
    return llvm_gen_be_error_t::ok;
  }
  if (tp->kind == tk_struct || tp->kind == tk_class) {
    std::vector<std::pair<llvm::MDNode*, uint64_t>> fields;
    for (a_field_ptr f = tp->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
      if (f->is_bit_field) continue;
      llvm::MDNode* f_node = nullptr;
      LLVM_GEN_BE_CHECK(llvm_get_tbaa_type_node(f->type, &f_node));
      fields.push_back({f_node, static_cast<uint64_t>(f->offset)});
    }
    const char* sname = (tp->source_corresp.name && tp->source_corresp.name[0] != '\0')
                            ? tp->source_corresp.name
                            : "anon_struct";
    *out_node = mdb.createTBAAStructTypeNode(sname, fields);
    return llvm_gen_be_error_t::ok;
  }
  *out_node = char_node;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Constructs an LLVM `!tbaa` access tag `MDNode` for a memory load or store.
 * @details Combines the base type node, scalar access type node, and byte offset via `MDBuilder::createTBAAAccessTag`.
 * @param[in] base_type Pointer to the base EDG type (struct or scalar).
 * @param[in] access_type Pointer to the accessed scalar/pointer EDG type.
 * @param[in] offset Byte offset of the access within `base_type`.
 * @param[out] out_tag Receives the constructed `!tbaa` access tag `llvm::MDNode*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_tag` is null.
 */
llvm_gen_be_error_t llvm_get_tbaa_access_tag(
    a_type_ptr base_type,
    a_type_ptr access_type,
    uint64_t offset,
    llvm::MDNode** out_tag) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_tag);
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  a_type_ptr acc_tp = access_type ? skip_typerefs(access_type) : nullptr;
  if (acc_tp && (acc_tp->kind == tk_struct || acc_tp->kind == tk_class || acc_tp->kind == tk_union)) {
    acc_tp = nullptr;
    if (base_type == access_type) {
      base_type = nullptr;
    }
  }
  llvm::MDNode* base_node = nullptr;
  llvm::MDNode* access_node = nullptr;
  LLVM_GEN_BE_CHECK(llvm_get_tbaa_type_node(base_type, &base_node));
  LLVM_GEN_BE_CHECK(llvm_get_tbaa_type_node(acc_tp, &access_node));
  llvm::MDBuilder mdb{*be_state->context};
  *out_tag = mdb.createTBAAStructTagNode(base_node, access_node, offset);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Constructs an LLVM `!tbaa.struct` metadata node for aggregate memory copy operations.
 * @details Builds field offset/size/tag triples for all non-bitfield fields of `struct_type`.
 * @param[in] struct_type Pointer to the EDG struct/class type.
 * @param[out] out_node Receives the constructed `!tbaa.struct` `llvm::MDNode*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_node` is null.
 */
llvm_gen_be_error_t llvm_get_tbaa_struct_node(
    a_type_ptr struct_type,
    llvm::MDNode** out_node) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_node);
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  std::vector<llvm::MDBuilder::TBAAStructField> fields;
  a_type_ptr tp = struct_type ? skip_typerefs(struct_type) : nullptr;
  if (tp && (tp->kind == tk_struct || tp->kind == tk_class)) {
    for (a_field_ptr f = tp->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
      if (f->is_bit_field || !f->type) continue;
      llvm::MDNode* tag = nullptr;
      LLVM_GEN_BE_CHECK(llvm_get_tbaa_access_tag(f->type, f->type, 0, &tag));
      a_targ_size_t raw_fsz = skip_typerefs(f->type)->size;
      uint64_t fsz = (raw_fsz > 0) ? static_cast<uint64_t>(raw_fsz) : 1;
      fields.push_back({static_cast<uint64_t>(f->offset), fsz, tag});
    }
  }
  llvm::MDBuilder mdb{*be_state->context};
  *out_node = mdb.createTBAAStructNode(fields);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resolves a symbolic GPU/target address-space name (`shared`, `constant`, `global`, `local`, `generic`) to an LLVM address space ID.
 * @details Maps address-space names for `nvptx64`, `amdgcn`, and `spirv64` target triples (or generic targets).
 * @param[in] space_name Null-terminated address-space name (`"shared"`, `"constant"`, `"global"`, `"local"`, `"generic"`).
 * @param[out] out_addr_space Receives the target-specific LLVM address space integer.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_addr_space` is null.
 */
llvm_gen_be_error_t llvm_resolve_target_address_space(
    const char* space_name,
    unsigned* out_addr_space) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_addr_space);
  *out_addr_space = 0;
  if (!space_name || space_name[0] == '\0') {
    return llvm_gen_be_error_t::ok;
  }
  llvm::StringRef sname = llvm::StringRef(space_name);
  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  bool is_nvptx = triple.isNVPTX();
  bool is_amdgpu = triple.isAMDGPU();
  bool is_spirv = triple.isSPIRV();

  if (sname == "global") {
    *out_addr_space = 1;
  } else if (sname == "shared") {
    *out_addr_space = 3;
  } else if (sname == "constant") {
    *out_addr_space = is_spirv ? 2 : 4;
  } else if (sname == "local") {
    *out_addr_space = is_nvptx ? 5 : (is_amdgpu ? 3 : 0);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Determines whether an EDG type represents an IEEE-754 decimal floating-point type (`_Decimal32`, `_Decimal64`, `_Decimal128`).
 * @details Inspects typedef chains (`tk_typeref`) for canonical decimal floating-point type names and returns the byte width (`4`, `8`, `16`, or `0` if not a decimal float).
 * @param[in] edg_type Pointer to the EDG type to inspect (may be `nullptr`).
 * @param[out] out_byte_width Receives `4`, `8`, or `16` for decimal floating-point types, or `0` otherwise.
 * @return `llvm_gen_be_error_t::ok` on success, or `invalid_argument` if `out_byte_width` is null.
 */
llvm_gen_be_error_t llvm_get_decimal_float_byte_width(
    a_type_ptr edg_type,
    unsigned* out_byte_width) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_byte_width);
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_byte_width = 0;
  for (a_type_ptr cur = edg_type; cur != nullptr; ) {
    const char* tname = cur->source_corresp.name;
    if (tname != nullptr) {
      llvm::StringRef nr = llvm::StringRef(tname);
      if (nr == "_Decimal32" || nr == "decimal32") {
        *out_byte_width = 4;
        return llvm_gen_be_error_t::ok;
      }
      if (nr == "_Decimal64" || nr == "decimal64") {
        *out_byte_width = 8;
        return llvm_gen_be_error_t::ok;
      }
      if (nr == "_Decimal128" || nr == "decimal128") {
        *out_byte_width = 16;
        return llvm_gen_be_error_t::ok;
      }
    }
    if (cur->kind == tk_typeref) {
      cur = cur->variant.typeref.type;
    } else {
      break;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an IEEE-754 decimal floating-point byte width (`4`, `8`, or `16`) to its BID/DPD integer carrier `llvm::Type*`.
 * @details Maps `_Decimal32` (4 bytes) to `i32`, `_Decimal64` (8 bytes) to `i64`, and `_Decimal128` (16 bytes) to `i128`.
 * @param[in] byte_width Byte width of the decimal floating-point type (`4`, `8`, or `16`).
 * @param[out] out_type Receives the corresponding LLVM integer carrier type.
 * @return `llvm_gen_be_error_t::ok` on success, `invalid_argument` if `out_type` is null, or `unsupported_type` if `byte_width` is invalid.
 */
llvm_gen_be_error_t llvm_lower_decimal_float_type(
    unsigned byte_width,
    llvm::Type** out_type) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_type);
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  if (byte_width == 4) {
    *out_type = llvm::Type::getInt32Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }
  if (byte_width == 8) {
    *out_type = llvm::Type::getInt64Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }
  if (byte_width == 16) {
    *out_type = llvm::Type::getInt128Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }
  return llvm_gen_be_error_t::unsupported_type;
}

END_EDG_NAMESPACE
#endif
