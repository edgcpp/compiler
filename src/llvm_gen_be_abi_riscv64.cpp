/**
 * @file llvm_gen_be_abi_riscv64.cpp
 * @ingroup llvm_gen_be_abi
 * @brief RISC-V 64-bit ABI lowering implementation.
 * @details Translates EDG types to LLVM IR function types matching RISC-V LP64D.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_abi_riscv64.h"
#include "llvm_gen_be_abi_sysv_x86_64.h"
#include <llvm/IR/Intrinsics.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Recursively flattens a struct into primitive types for RISC-V ABI.
 * @details Traverses nested structs and fixed-size arrays to collect scalar fields.
 * Unions cannot be flattened into FP register pairs under LP64D.
 * @param[in] ty EDG type to flatten.
 * @param[in,out] fields Output vector of primitive fields.
 * @param[in] offset Current byte offset.
 * @param[in,out] offsets Output vector of field byte offsets.
 * @param[in,out] is_valid Set to false if a union or unflatten-able member is encountered.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t flatten_riscv_struct(
    a_type_ptr ty,
    std::vector<a_type_ptr>* fields,
    uint64_t offset,
    std::vector<uint64_t>* offsets,
    bool* is_valid) noexcept {
  if (ty == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  ty = skip_typerefs(ty);

  if (ty->kind == tk_union) {
    *is_valid = false;
    return llvm_gen_be_error_t::ok;
  } else if (ty->kind == tk_class || ty->kind == tk_struct) {
    for (a_field_ptr f = ty->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
      if (f->is_bit_field && f->bit_size == 0) continue;
      if (f->is_bit_field) {
        *is_valid = false;
        return llvm_gen_be_error_t::ok;
      }
      llvm_gen_be_error_t err = flatten_riscv_struct(
          f->type, fields, offset + static_cast<uint64_t>(f->offset), offsets, is_valid);
      if (err != llvm_gen_be_error_t::ok || !*is_valid) return err;
    }
    return llvm_gen_be_error_t::ok;
  } else if (ty->kind == tk_array) {
    a_type_ptr elem_ty = ty->variant.array.element_type;
    uint64_t elem_sz = elem_ty ? static_cast<uint64_t>(elem_ty->size) : 0ULL;
    uint64_t num_elems = static_cast<uint64_t>(ty->variant.array.variant.number_of_elements);
    for (uint64_t i = 0; i < num_elems; ++i) {
      llvm_gen_be_error_t err = flatten_riscv_struct(
          elem_ty, fields, offset + i * elem_sz, offsets, is_valid);
      if (err != llvm_gen_be_error_t::ok || !*is_valid) return err;
    }
    return llvm_gen_be_error_t::ok;
  } else if (ty->kind == tk_integer || ty->kind == tk_float ||
             ty->kind == tk_pointer || ty->kind == tk_nullptr || ty->kind == tk_ptr_to_member) {
    fields->push_back(ty);
    offsets->push_back(offset);
    return llvm_gen_be_error_t::ok;
  }

  *is_valid = false;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Detects if a struct can be flattened into up to 2 FP/INT registers under LP64D.
 * @details Inspects flattened scalar fields for float, float+float, float+int, or int+float
 * pairs fitting within FLEN/XLEN (8 bytes per register), falling back to integer array coercion
 * for structs <= 16 bytes.
 * @param[in] ty The EDG aggregate type to inspect.
 * @param[out] coerced_ty Pointer to store the coerced LLVM type.
 * @param[out] out_flattened Set to true if the aggregate was coerced/flattened.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t detect_riscv_flattening(
    a_type_ptr ty,
    llvm::Type** coerced_ty,
    bool* out_flattened) noexcept {
  *out_flattened = false;

  std::vector<a_type_ptr> fields;
  std::vector<uint64_t> offsets;
  bool is_valid = true;
  llvm_gen_be_error_t err = flatten_riscv_struct(ty, &fields, 0, &offsets, &is_valid);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  uint64_t xlen = triple.isRISCV32() ? 4 : 8;

  if (is_valid && !fields.empty() && fields.size() <= 2) {
    bool has_fp = false;
    bool sizes_valid = true;
    for (a_type_ptr f_ty : fields) {
      if (f_ty->kind == tk_float) {
        has_fp = true;
      }
      if (static_cast<uint64_t>(f_ty->size) > xlen) {
        sizes_valid = false;
      }
    }
    if (has_fp && sizes_valid) {
      std::vector<llvm::Type*> llvm_fields;
      for (a_type_ptr f_ty : fields) {
        llvm::Type* lty = nullptr;
        err = get_llvm_type(f_ty, &lty);
        if (err != llvm_gen_be_error_t::ok) return err;
        llvm_fields.push_back(lty);
      }
      *coerced_ty = llvm::StructType::get(*be_state->context, llvm_fields, false);
      *out_flattened = true;
      return llvm_gen_be_error_t::ok;
    }
  }

  if (ty->size > 0) {
    uint32_t num_ints = static_cast<uint32_t>((ty->size + xlen - 1) / xlen);
    llvm::Type* word_ty = (xlen == 4)
                              ? llvm::Type::getInt32Ty(*be_state->context)
                              : llvm::Type::getInt64Ty(*be_state->context);
    *coerced_ty = llvm::ArrayType::get(word_ty, num_ints);
    *out_flattened = true;
    return llvm_gen_be_error_t::ok;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Classifies a RISC-V 64 argument type.
 * @details Implements or declares `classify_riscv64_argument` for the EDG LLVM backend.
 * @param[in] ty EDG type to classify.
 * @param[out] out_info Classification info output.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t classify_riscv64_argument(a_type_ptr ty, riscv64_arg_info_t* out_info) noexcept {
  if (!ty || !out_info) return llvm_gen_be_error_t::invalid_argument;
  
  ty = skip_typerefs(ty);
  out_info->size = static_cast<uint64_t>(ty->size);
  out_info->is_indirect = false;
  out_info->pass_by_invisible_reference = false;
  out_info->coerced_type = nullptr;
  
  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  uint64_t max_reg_bytes = triple.isRISCV32() ? 8 : 16;

  if (is_class_struct_union_type(ty)) {
      if ((ty->kind == tk_class || ty->kind == tk_struct) && !is_trivially_copyable_type(ty)) {
          out_info->is_indirect = true;
          out_info->pass_by_invisible_reference = true;
      } else if (static_cast<uint64_t>(ty->size) > max_reg_bytes) {
          out_info->is_indirect = true;
      } else {
          bool flattened = false;
          llvm_gen_be_error_t err = detect_riscv_flattening(ty, &out_info->coerced_type, &flattened);
          if (err != llvm_gen_be_error_t::ok) return err;
      }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes the return info for RISC-V 64.
 * @details Implements or declares `compute_riscv64_return_info` for the EDG LLVM backend.
 * @param[in] ret_ty EDG return type.
 * @param[out] out_info Classification info output.
 * @param[out] out_sret Set to true if an sret pointer is required.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t compute_riscv64_return_info(a_type_ptr ret_ty, riscv64_arg_info_t* out_info, bool* out_sret) noexcept {
  if (!ret_ty || !out_info || !out_sret) return llvm_gen_be_error_t::invalid_argument;
  
  *out_sret = false;
  llvm_gen_be_error_t err = classify_riscv64_argument(ret_ty, out_info);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  if (out_info->is_indirect) {
      *out_sret = true;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Builds the LLVM function type for a RISC-V 64-bit routine.
 * @details Applies LP64D rules, including integer/fp promotion and byval/sret.
 * @param[in] routine_ty The EDG routine type.
 * @param[out] out_fn_ty The generated LLVM function type.
 * @param[out] out_attrs The generated LLVM attribute list.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t build_riscv64_function_type(
    a_type_ptr routine_ty,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept {

  if (!routine_ty || !out_fn_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;

  std::vector<llvm::Type*> llvm_param_types;
  std::vector<llvm::AttributeSet> param_attrs;
  llvm::AttrBuilder ret_attr{*be_state->context};
  
  a_type_ptr ret_ty = routine_ty->variant.routine.return_type;
  llvm::Type* llvm_ret_ty = nullptr;
  
  riscv64_arg_info_t ret_info;
  bool is_sret = false;
  llvm_gen_be_error_t err = compute_riscv64_return_info(ret_ty, &ret_info, &is_sret);
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
  } else if (ret_info.coerced_type) {
    llvm_ret_ty = ret_info.coerced_type;
  } else {
    err = llvm_type_from_edg_type(ret_ty, &llvm_ret_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    a_type_ptr raw_ret_ty = skip_typerefs(ret_ty);
    if (raw_ret_ty->kind == tk_integer && raw_ret_ty->size <= 4) {
      if (int_type_is_signed(raw_ret_ty)) ret_attr.addAttribute(llvm::Attribute::SExt);
      else ret_attr.addAttribute(llvm::Attribute::ZExt);
    }
  }

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  uint64_t xlen = triple.isRISCV32() ? 4 : 8;
  llvm_target_abi_reg_state_t reg_state = {is_sret ? 1u : 0u, 0u, 8u, 8u};

  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
         
         riscv64_arg_info_t p_info;
         err = classify_riscv64_argument(param->type, &p_info);
         if (err != llvm_gen_be_error_t::ok) return err;

         llvm::Type* pty = nullptr;
         err = llvm_type_from_edg_type(param->type, &pty);
         if (err != llvm_gen_be_error_t::ok) return err;
         
         llvm::AttrBuilder ab{*be_state->context};
         a_type_ptr raw_pty = skip_typerefs(param->type);
         if (p_info.is_indirect) {
             if (reg_state.gpr_used < reg_state.gpr_max) reg_state.gpr_used++;
             llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
         } else if (p_info.coerced_type) {
             uint32_t needed_fpr = 0;
             uint32_t needed_gpr = 0;
             if (auto* st_ty = llvm::dyn_cast<llvm::StructType>(p_info.coerced_type)) {
               for (llvm::Type* el : st_ty->elements()) {
                 if (el->isFloatingPointTy()) needed_fpr++;
                 else needed_gpr++;
               }
             } else {
               needed_gpr = static_cast<uint32_t>(llvm::cast<llvm::ArrayType>(p_info.coerced_type)->getNumElements());
             }
             if (needed_fpr > 0 &&
                 (reg_state.fpr_used + needed_fpr > reg_state.fpr_max ||
                  reg_state.gpr_used + needed_gpr > reg_state.gpr_max)) {
               uint32_t num_ints = static_cast<uint32_t>((raw_pty->size + xlen - 1) / xlen);
               llvm::Type* word_ty = (xlen == 4)
                                         ? llvm::Type::getInt32Ty(*be_state->context)
                                         : llvm::Type::getInt64Ty(*be_state->context);
               p_info.coerced_type = llvm::ArrayType::get(word_ty, num_ints);
               reg_state.gpr_used = std::min(reg_state.gpr_max, reg_state.gpr_used + num_ints);
             } else {
               reg_state.fpr_used += needed_fpr;
               reg_state.gpr_used = std::min(reg_state.gpr_max, reg_state.gpr_used + needed_gpr);
             }
             llvm_param_types.push_back(p_info.coerced_type);
         } else {
             if (raw_pty->kind == tk_float && reg_state.fpr_used < reg_state.fpr_max) {
               reg_state.fpr_used++;
             } else if (reg_state.gpr_used < reg_state.gpr_max) {
               reg_state.gpr_used++;
             }
             if (raw_pty->kind == tk_integer && raw_pty->size <= 4) {
               if (int_type_is_signed(raw_pty)) ab.addAttribute(llvm::Attribute::SExt);
               else ab.addAttribute(llvm::Attribute::ZExt);
             }
             llvm_param_types.push_back(pty);
         }
         param_attrs.push_back(llvm::AttributeSet::get(*be_state->context, ab));
    }
  }

  bool is_vararg = routine_ty->variant.routine.extra_info ? routine_ty->variant.routine.extra_info->has_ellipsis : false;
  *out_fn_ty = llvm::FunctionType::get(llvm_ret_ty, llvm_param_types, is_vararg);
  llvm::AttributeSet ret_set = llvm::AttributeSet::get(*be_state->context, ret_attr);
  llvm::AttributeSet fn_set;
  *out_attrs = llvm::AttributeList::get(*be_state->context, fn_set, ret_set, param_attrs);
  
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Builds a RISC-V va_list type.
 * @details Implements or declares `build_riscv64_va_list_type` for the EDG LLVM backend.
 * @param[out] out_ty Output pointer.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t build_riscv64_va_list_type(llvm::Type** out_ty) noexcept {
  if (!out_ty) return llvm_gen_be_error_t::invalid_argument;
  *out_ty = llvm::PointerType::getUnqual(*be_state->context);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a RISC-V va_start.
 * @details Implements or declares `lower_riscv64_va_start` for the EDG LLVM backend.
 * @param[in] va_list_ptr Pointer.
 * @param[out] out_val Output value.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_riscv64_va_start(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Function* vastart = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vastart);
  llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vastart, {cast_ptr});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a RISC-V va_arg.
 * @details Implements or declares `lower_riscv64_va_arg` for the EDG LLVM backend.
 * @param[in] va_list_ptr Pointer.
 * @param[in] ty Requested type.
 * @param[out] out_val Output value.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_riscv64_va_arg(llvm::Value* va_list_ptr, a_type_ptr ty, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !ty || !out_val) return llvm_gen_be_error_t::invalid_argument;
  riscv64_arg_info_t info = {};
  llvm_gen_be_error_t err = classify_riscv64_argument(ty, &info);
  if (err != llvm_gen_be_error_t::ok) return err;
  llvm::Type* llvm_ty = nullptr;
  err = llvm_type_from_edg_type(ty, &llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;
  if (info.is_indirect) {
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Value* ind_ptr = be_state->builder->CreateVAArg(va_list_ptr, ptr_ty, "vaarg.ind.ptr");
    *out_val = be_state->builder->CreateLoad(llvm_ty, ind_ptr, "vaarg.ind.val");
    return llvm_gen_be_error_t::ok;
  }
  if (skip_typerefs(ty)->alignment > 16) {
    *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty, "vaarg.aligned");
    return llvm_gen_be_error_t::ok;
  }
  *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a RISC-V va_copy.
 * @details Implements or declares `lower_riscv64_va_copy` for the EDG LLVM backend.
 * @param[in] dest_va_list_ptr Destination pointer.
 * @param[in] src_va_list_ptr Source pointer.
 * @param[out] out_val Output value.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_riscv64_va_copy(llvm::Value* dest_va_list_ptr, llvm::Value* src_va_list_ptr, llvm::Value** out_val) noexcept {
  if (!dest_va_list_ptr || !src_va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Function* vacopy = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vacopy);
  llvm::Value* dest_cast = be_state->builder->CreatePointerCast(dest_va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  llvm::Value* src_cast = be_state->builder->CreatePointerCast(src_va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vacopy, {dest_cast, src_cast});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a RISC-V va_end.
 * @details Implements or declares `lower_riscv64_va_end` for the EDG LLVM backend.
 * @param[in] va_list_ptr Pointer.
 * @param[out] out_val Output value.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_riscv64_va_end(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Function* vaend = EDG_LLVM_GET_VA_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::vaend);
  llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vaend, {cast_ptr});
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
