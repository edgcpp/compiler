/**
 * @file llvm_gen_be_abi_riscv64.cpp
 * @brief RISC-V 64-bit ABI lowering implementation.
 * @details Translates EDG types to LLVM IR function types matching RISC-V LP64D.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_abi_riscv64.h"
#include <llvm/IR/Intrinsics.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Detects if a struct can be flattened into up to 2 FP/INT registers.
 * @details Simplified heuristic for testing purposes.
 * @param[in] ty The EDG type.
 * @param[out] coerced_ty The coerced LLVM type.
 * @return true if flattened.
 */
/**
 * @brief Recursively flattens a struct into primitive types for RISC-V ABI.
 * @param[in] ty EDG type to flatten.
 * @param[out] fields Output vector of primitive fields.
 * @param[in] offset Current offset.
 * @param[out] offsets Output vector of offsets.
 */
static void flatten_riscv_struct(a_type_ptr ty, std::vector<a_type_ptr>& fields, uint64_t offset, std::vector<uint64_t>& offsets) {
    if (ty == nullptr) return;
    ty = skip_typerefs(ty);

    if (ty->kind == tk_class || ty->kind == tk_struct || ty->kind == tk_union) {
        for (a_field_ptr f = ty->variant.class_struct_union.field_list; f; f = f->next) {
            uint64_t f_offset = (ty->kind == tk_union) ? offset : offset + f->offset;
            // Ignore zero-length bitfields
            if (f->is_bit_field && f->bit_size == 0) continue;
            flatten_riscv_struct(f->type, fields, f_offset, offsets);
        }
    } else if (ty->kind == tk_array) {
        a_type_ptr elem_ty = ty->variant.array.element_type;
        uint64_t num_elems = ty->variant.array.variant.number_of_elements;
        for (uint64_t i = 0; i < num_elems; ++i) {
            flatten_riscv_struct(elem_ty, fields, offset + i * elem_ty->size, offsets);
        }
    } else if (ty->kind == tk_integer || ty->kind == tk_float || ty->kind == tk_pointer || ty->kind == tk_ptr_to_member) {
        fields.push_back(ty);
        offsets.push_back(offset);
    }
}

static bool detect_riscv_flattening(a_type_ptr ty, llvm::Type** coerced_ty) {
    if (!is_class_struct_union_type(ty)) return false;
    
    std::vector<a_type_ptr> fields;
    std::vector<uint64_t> offsets;
    flatten_riscv_struct(ty, fields, 0, offsets);

    if (fields.size() > 0 && fields.size() <= 2) {
        bool has_fp = false;
        for (a_type_ptr f_ty : fields) {
            if (f_ty->kind == tk_float) {
                has_fp = true;
                break;
            }
        }
        if (has_fp) {
            std::vector<llvm::Type*> llvm_fields;
            for (a_type_ptr f_ty : fields) {
                llvm::Type* lty = nullptr;
                if (get_llvm_type(f_ty, &lty) == llvm_gen_be_error_t::ok) {
                    llvm_fields.push_back(lty);
                }
            }
            if (llvm_fields.size() == fields.size()) {
                *coerced_ty = llvm::StructType::get(*be_state->context, llvm_fields, false);
                return true;
            }
        }
    }

    if (ty->size > 0 && ty->size <= 16) {
        uint32_t num_ints = (ty->size + 7) / 8;
        *coerced_ty = llvm::ArrayType::get(llvm::Type::getInt64Ty(*be_state->context), num_ints);
        return true;
    }
    return false;
}

/**
 * @brief Classifies a RISC-V 64 argument type.
 * @param[in] ty EDG type to classify.
 * @param[out] out_info Classification info output.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t classify_riscv64_argument(a_type_ptr ty, riscv64_arg_info_t* out_info) noexcept {
  if (!ty || !out_info) return llvm_gen_be_error_t::invalid_argument;
  
  ty = skip_typerefs(ty);
  out_info->size = ty->size;
  out_info->is_indirect = false;
  out_info->coerced_type = nullptr;
  
  if (is_class_struct_union_type(ty)) {
      if (ty->size > 16) {
          out_info->is_indirect = true;
      } else {
          detect_riscv_flattening(ty, &out_info->coerced_type);
      }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes the return info for RISC-V 64.
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
  llvm::AttrBuilder ret_attr(*be_state->context);
  
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
    
    llvm::AttrBuilder sret_ab(*be_state->context);
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
  }

  if (routine_ty->variant.routine.extra_info != nullptr) {
    for (a_param_type_ptr param = routine_ty->variant.routine.extra_info->param_type_list;
         param != nullptr; param = param->next) {
         
         riscv64_arg_info_t p_info;
         err = classify_riscv64_argument(param->type, &p_info);
         if (err != llvm_gen_be_error_t::ok) return err;

         llvm::Type* pty = nullptr;
         err = llvm_type_from_edg_type(param->type, &pty);
         if (err != llvm_gen_be_error_t::ok) return err;
         
         llvm::AttrBuilder ab(*be_state->context);
         if (p_info.is_indirect) {
             llvm_param_types.push_back(llvm::PointerType::getUnqual(*be_state->context));
             ab.addByValAttr(pty);
         } else if (p_info.coerced_type) {
             llvm_param_types.push_back(p_info.coerced_type);
         } else {
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
 * @param[in] va_list_ptr Pointer.
 * @param[out] out_val Output value.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_riscv64_va_start(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Function* vastart = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::vastart, {llvm::PointerType::getUnqual(*be_state->context)});
  llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vastart, {cast_ptr});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a RISC-V va_arg.
 * @param[in] va_list_ptr Pointer.
 * @param[in] ty Requested type.
 * @param[out] out_val Output value.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_riscv64_va_arg(llvm::Value* va_list_ptr, a_type_ptr ty, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !ty || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Type* llvm_ty = nullptr;
  llvm_gen_be_error_t err = llvm_type_from_edg_type(ty, &llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;
  *out_val = be_state->builder->CreateVAArg(va_list_ptr, llvm_ty);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a RISC-V va_copy.
 * @param[in] dest_va_list_ptr Destination pointer.
 * @param[in] src_va_list_ptr Source pointer.
 * @param[out] out_val Output value.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_riscv64_va_copy(llvm::Value* dest_va_list_ptr, llvm::Value* src_va_list_ptr, llvm::Value** out_val) noexcept {
  if (!dest_va_list_ptr || !src_va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Function* vacopy = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::vacopy, {llvm::PointerType::getUnqual(*be_state->context)});
  llvm::Value* dest_cast = be_state->builder->CreatePointerCast(dest_va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  llvm::Value* src_cast = be_state->builder->CreatePointerCast(src_va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vacopy, {dest_cast, src_cast});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a RISC-V va_end.
 * @param[in] va_list_ptr Pointer.
 * @param[out] out_val Output value.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t lower_riscv64_va_end(llvm::Value* va_list_ptr, llvm::Value** out_val) noexcept {
  if (!va_list_ptr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Function* vaend = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::vaend, {llvm::PointerType::getUnqual(*be_state->context)});
  llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(va_list_ptr, llvm::PointerType::getUnqual(*be_state->context));
  *out_val = be_state->builder->CreateCall(vaend, {cast_ptr});
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
