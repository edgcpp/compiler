/**
 * @file llvm_gen_be_ptrmem.cpp
 * @ingroup llvm_gen_be_cxx_abi
 * @brief Pointer-to-member lowering subsystem for the EDG LLVM backend.
 * @details Implements representation, constants, calls, field selections, conversions,
 * and comparisons for pointer-to-member data and pointer-to-member functions under
 * the Itanium and Microsoft MSVC C++ ABIs.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_ptrmem.h"
#include "llvm_gen_be_expr.h"
#include "target.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/TargetParser/Triple.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Computes the LLVM Type for an EDG pointer-to-member type.
 * @details Lowers to {ptr, ptrdiff_t} for member functions under Itanium,
 * ptrdiff_t for member data, or MSVC variable-width struct models.
 * @param[in] type Pointer to the EDG pointer-to-member type.
 * @param[out] out_type Receives the computed LLVM Type pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_ptr_to_member(
    a_type_ptr type,
    llvm::Type** out_type) noexcept {
  if (!type || !out_type) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  type = skip_typerefs(type);
  if (type->kind != tk_ptr_to_member) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  bool is_msvc = triple.isWindowsMSVCEnvironment();

  a_type_ptr mem_ty = type->variant.ptr_to_member.type;
  bool is_func = (mem_ty != nullptr && skip_typerefs(mem_ty)->kind == tk_routine);

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* int_ty = llvm::Type::getInt64Ty(*be_state->context);
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);

  if (is_msvc) {
    if (is_func) {
      a_type_ptr class_ty = skip_typerefs(type->variant.ptr_to_member.class_of_which_a_member);
      a_class_type_supplement_ptr supp = class_ty->variant.class_struct_union.extra_info;
      int base_count = 0;
      for (a_base_class_ptr b = supp ? supp->direct_base_classes : nullptr; b != nullptr; b = b->next_direct) {
        base_count++;
      }
      if (base_count <= 1 && !class_ty->variant.class_struct_union.any_virtual_base_classes) {
        // Single inheritance: 1 word
        *out_type = ptr_ty;
      } else if (!class_ty->variant.class_struct_union.any_virtual_base_classes) {
        // Multiple inheritance: 2 words { ptr, i32 }
        *out_type = llvm::StructType::get(*be_state->context, {ptr_ty, i32_ty});
      } else {
        // Virtual inheritance: 3 words { ptr, i32, i32 }
        *out_type = llvm::StructType::get(*be_state->context, {ptr_ty, i32_ty, i32_ty});
      }
    } else {
      // Data member in MSVC: i32
      *out_type = i32_ty;
    }
    return llvm_gen_be_error_t::ok;
  }

  // Itanium ABI
  if (is_func) {
    // { ptr, ptrdiff_t }
    *out_type = llvm::StructType::get(*be_state->context, {ptr_ty, int_ty});
  } else {
    // ptrdiff_t
    *out_type = int_ty;
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Evaluates an EDG ck_ptr_to_member constant into an LLVM Constant.
 * @details Handles null pointer-to-member constants, member data offset constants,
 * and member function pointer constants (virtual and non-virtual).
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty Expected LLVM Type for the constant.
 * @param[out] out_const Receives the generated LLVM Constant on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_const_from_ptr_to_member(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);

  if (con->kind != ck_ptr_to_member) {
    if (con->kind == ck_address && con->variant.address.kind == abk_routine &&
        con->variant.address.variant.routine != nullptr) {
      a_routine_ptr ar = con->variant.address.variant.routine;
      const char* fname = ar->source_corresp.name ? ar->source_corresp.name : "routine";
      llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(
          fname, llvm::FunctionType::get(ptr_ty, {}, false));
      llvm::Constant* fn_ptr = llvm::cast<llvm::Constant>(callee.getCallee());
      int64_t adj_off = static_cast<int64_t>(con->variant.address.offset);
      if (expected_ty && expected_ty->isStructTy()) {
        llvm::StructType* st = llvm::cast<llvm::StructType>(expected_ty);
        std::vector<llvm::Constant*> fields = {fn_ptr, llvm::ConstantInt::get(st->getElementType(1), static_cast<uint64_t>(adj_off))};
        if (st->getNumElements() == 3) {
          fields.push_back(llvm::ConstantInt::get(st->getElementType(2), 0));
        }
        *out_const = llvm::ConstantStruct::get(st, fields);
      } else {
        *out_const = fn_ptr;
      }
      return llvm_gen_be_error_t::ok;
    }
    if (con->kind == ck_address && con->variant.address.kind == abk_variable) {
      int64_t f_off = static_cast<int64_t>(con->variant.address.offset);
      if (con->variant.address.subobject_path &&
          !con->variant.address.subobject_path->is_offset &&
          !con->variant.address.subobject_path->is_base_class &&
          con->variant.address.subobject_path->variant.field != nullptr) {
        f_off += static_cast<int64_t>(con->variant.address.subobject_path->variant.field->offset);
      }
      *out_const = llvm::ConstantInt::get(expected_ty ? expected_ty : i64_ty, static_cast<uint64_t>(f_off));
      return llvm_gen_be_error_t::ok;
    }
    // Null constant representation for member pointers
    if (expected_ty && expected_ty->isStructTy()) {
      llvm::StructType* st = llvm::cast<llvm::StructType>(expected_ty);
      std::vector<llvm::Constant*> null_fields;
      for (unsigned i = 0; i < st->getNumElements(); ++i) {
        null_fields.push_back(llvm::Constant::getNullValue(st->getElementType(i)));
      }
      *out_const = llvm::ConstantStruct::get(st, null_fields);
    } else if (expected_ty) {
      // Null pointer-to-data-member is -1
      *out_const = llvm::ConstantInt::getSigned(expected_ty, -1);
    } else {
      *out_const = llvm::ConstantInt::getSigned(i64_ty, -1);
    }
    return llvm_gen_be_error_t::ok;
  }

  if (con->variant.ptr_to_member.is_function_ptr) {
    a_routine_ptr r = con->variant.ptr_to_member.variant.routine;
    if (!r) {
      // Null member function pointer
      if (expected_ty && expected_ty->isPointerTy()) {
        *out_const = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
        return llvm_gen_be_error_t::ok;
      }
      llvm::StructType* st = llvm::dyn_cast_or_null<llvm::StructType>(expected_ty);
      if (!st) st = llvm::StructType::get(*be_state->context, {ptr_ty, i64_ty});
      std::vector<llvm::Constant*> null_elems;
      for (unsigned i = 0; i < st->getNumElements(); ++i) {
        null_elems.push_back(llvm::Constant::getNullValue(st->getElementType(i)));
      }
      *out_const = llvm::ConstantStruct::get(st, null_elems);
      return llvm_gen_be_error_t::ok;
    }

    int64_t this_adj = 0;
    if (con->variant.ptr_to_member.casting_base_class) {
      this_adj = static_cast<int64_t>(con->variant.ptr_to_member.casting_base_class->offset);
    }

    llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
    bool is_arm_itanium = !triple.isWindowsMSVCEnvironment() &&
                          (triple.isAArch64() || triple.isARM() || triple.isThumb());

    llvm::Constant* ptr_part = nullptr;
    if (r->number.virtual_function > 0) {
      if (is_arm_itanium) {
        // ARM64/ARM Itanium ABI: ptr is (vtable_index * sizeof(ptr)), bit 0 of adj is 1
        int64_t vtbl_byte_off = static_cast<int64_t>(r->number.virtual_function - 1) * 8;
        ptr_part = llvm::ConstantExpr::getIntToPtr(
            llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(vtbl_byte_off)), ptr_ty);
        this_adj = (this_adj << 1) | 1;
      } else {
        // Standard Itanium ABI: Word 0 is (vtable_index * sizeof(ptr)) + 1
        int64_t vtbl_byte_off = (static_cast<int64_t>(r->number.virtual_function - 1) * 8) + 1;
        ptr_part = llvm::ConstantExpr::getIntToPtr(
            llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(vtbl_byte_off)), ptr_ty);
      }
    } else {
      // Non-virtual method: Word 0 is function address
      llvm::Function* pm_fn = nullptr;
      LLVM_GEN_BE_CHECK(llvm_get_or_declare_routine(
          r, &pm_fn, llvm::FunctionType::get(ptr_ty, {}, false)));
      if (!pm_fn) {
        pm_fn = llvm::cast<llvm::Function>(
            be_state->module->getOrInsertFunction(
                "routine", llvm::FunctionType::get(ptr_ty, {}, false)).getCallee());
      }
      ptr_part = pm_fn;
      if (is_arm_itanium) {
        this_adj = (this_adj << 1);
      }
    }

    if (expected_ty && expected_ty->isPointerTy()) {
      *out_const = ptr_part;
      return llvm_gen_be_error_t::ok;
    }

    llvm::StructType* st = llvm::dyn_cast_or_null<llvm::StructType>(expected_ty);
    if (!st) st = llvm::StructType::get(*be_state->context, {ptr_ty, i64_ty});

    std::vector<llvm::Constant*> fields;
    fields.push_back(ptr_part);
    fields.push_back(llvm::ConstantInt::get(st->getElementType(1), static_cast<uint64_t>(this_adj)));
    if (st->getNumElements() == 3) {
      fields.push_back(llvm::ConstantInt::get(st->getElementType(2), 0));
    }
    *out_const = llvm::ConstantStruct::get(st, fields);
    return llvm_gen_be_error_t::ok;
  }

  // Data member pointer constant
  a_field_ptr f = con->variant.ptr_to_member.variant.field;
  if (!f) {
    // Null data member pointer is -1
    *out_const = llvm::ConstantInt::getSigned(
        expected_ty ? expected_ty : i64_ty, -1);
    return llvm_gen_be_error_t::ok;
  }

  int64_t offset = static_cast<int64_t>(f->offset);
  if (con->variant.ptr_to_member.casting_base_class) {
    offset += static_cast<int64_t>(con->variant.ptr_to_member.casting_base_class->offset);
  }

  *out_const = llvm::ConstantInt::get(
      expected_ty ? expected_ty : i64_ty, static_cast<uint64_t>(offset));
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a pointer-to-member-function call expression (.* or ->*).
 * @details Handles virtual and non-virtual invocation with this-adjustment under Itanium and MSVC.
 * @param[in] expr Pointer to the call expression node (eok_dot_pm_call or eok_points_to_pm_call).
 * @param[out] out_val Receives the result value of the call expression on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_pm_call(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  an_expr_node_ptr op1 = expr->variant.operation.operands; // PM expr
  an_expr_node_ptr op2 = op1 ? op1->next : nullptr;         // Object expr
  if (!op1 || !op2) return llvm_gen_be_error_t::invalid_argument;

  llvm::Value* pm_val = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(op1, &pm_val);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Value* this_val = nullptr;
  err = llvm_lower_expression(op2, &this_val);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);

  llvm::Value* func_ptr = nullptr;
  llvm::Value* adj_this = this_val;

  if (pm_val->getType()->isStructTy()) {
    llvm::StructType* pm_st = llvm::cast<llvm::StructType>(pm_val->getType());
    llvm::Value* raw_ptr = be_state->builder->CreateExtractValue(pm_val, 0, "pm.ptr");
    llvm::Value* adj = be_state->builder->CreateExtractValue(pm_val, 1, "pm.adj");

    // If MSVC 3-word member pointer { ptr, mdisp, vdisp }, apply VBTable adjustment first
    if (pm_st->getNumElements() == 3) {
      llvm::Value* vdisp = be_state->builder->CreateExtractValue(pm_val, 2, "pm.vdisp");
      llvm::Value* vbptr = be_state->builder->CreateLoad(ptr_ty, this_val, "pm.vbptr");
      llvm::Value* vboff_slot = be_state->builder->CreateInBoundsGEP(i8_ty, vbptr, vdisp, "pm.vboff_slot");
      llvm::Value* vboff = be_state->builder->CreateLoad(
          llvm::Type::getInt32Ty(*be_state->context), vboff_slot, "pm.vboff");
      adj = be_state->builder->CreateAdd(adj, vboff, "pm.total_adj");
    }

    llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
    bool is_arm_itanium = !triple.isWindowsMSVCEnvironment() &&
                          (triple.isAArch64() || triple.isARM() || triple.isThumb());
    llvm::Value* ptr_int = be_state->builder->CreatePtrToInt(raw_ptr, i64_ty, "pm.ptr_int");
    llvm::Value* one = llvm::ConstantInt::get(i64_ty, 1);
    llvm::Value* is_virt = nullptr;
    llvm::Value* voff = nullptr;

    if (is_arm_itanium) {
      llvm::Value* adj_64 = be_state->builder->CreateSExtOrTrunc(adj, i64_ty);
      llvm::Value* byte_adj = be_state->builder->CreateAShr(adj_64, one, "pm.arm_byte_adj");
      adj_this = be_state->builder->CreateInBoundsGEP(i8_ty, this_val, byte_adj, "pm.this_adj");
      is_virt = be_state->builder->CreateICmpNE(
          be_state->builder->CreateAnd(adj_64, one), llvm::ConstantInt::get(i64_ty, 0), "pm.is_virt");
      voff = ptr_int;
    } else {
      // Adjust 'this' pointer: this + adj
      adj_this = be_state->builder->CreateInBoundsGEP(i8_ty, this_val, adj, "pm.this_adj");
      // Test bit 0 of raw_ptr
      is_virt = be_state->builder->CreateICmpNE(
          be_state->builder->CreateAnd(ptr_int, one), llvm::ConstantInt::get(i64_ty, 0), "pm.is_virt");
      voff = be_state->builder->CreateSub(ptr_int, one, "pm.voff");
    }

    llvm::Function* parent = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* virt_bb = llvm::BasicBlock::Create(*be_state->context, "pm.virt", parent);
    llvm::BasicBlock* nonvirt_bb = llvm::BasicBlock::Create(*be_state->context, "pm.nonvirt", parent);
    llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*be_state->context, "pm.cont", parent);

    be_state->builder->CreateCondBr(is_virt, virt_bb, nonvirt_bb);

    // Non-virtual block: raw_ptr is function pointer directly
    be_state->builder->SetInsertPoint(nonvirt_bb);
    be_state->builder->CreateBr(cont_bb);

    // Virtual block: load vtable, load slot at voff
    be_state->builder->SetInsertPoint(virt_bb);
    llvm::Value* vptr = be_state->builder->CreateLoad(ptr_ty, adj_this, "pm.vptr");
    llvm::Value* vslot = be_state->builder->CreateInBoundsGEP(i8_ty, vptr, voff, "pm.vslot");
    llvm::Value* virt_fn = be_state->builder->CreateLoad(ptr_ty, vslot, "pm.virt_fn");
    be_state->builder->CreateBr(cont_bb);

    // Merge phi
    be_state->builder->SetInsertPoint(cont_bb);
    llvm::PHINode* phi = be_state->builder->CreatePHI(ptr_ty, 2, "pm.func_ptr");
    phi->addIncoming(raw_ptr, nonvirt_bb);
    phi->addIncoming(virt_fn, virt_bb);
    func_ptr = phi;
  } else {
    func_ptr = pm_val;
  }

  // Build argument list: first argument is adj_this, followed by remaining args
  std::vector<llvm::Value*> call_args;
  std::vector<llvm::Type*> call_arg_types;
  call_args.push_back(adj_this);
  call_arg_types.push_back(adj_this->getType());
  for (an_expr_node_ptr arg = op2->next; arg != nullptr; arg = arg->next) {
    llvm::Value* arg_val = nullptr;
    err = llvm_lower_expression(arg, &arg_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    call_args.push_back(arg_val);
    call_arg_types.push_back(arg_val->getType());
  }

  llvm::Type* pm_ret_ty = ptr_ty;
  if (expr->type) {
    LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &pm_ret_ty));
  }
  llvm::FunctionType* call_fn_ty = llvm::FunctionType::get(pm_ret_ty, call_arg_types, false);
  if (!be_state->current_landing_pads.empty()) {
    llvm::Function* parent = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*be_state->context, "pm.invoke.cont", parent);
    llvm::BasicBlock* lpad_bb = be_state->current_landing_pads.back();
    llvm::InvokeInst* inv = be_state->builder->CreateInvoke(
        call_fn_ty, func_ptr, cont_bb, lpad_bb, call_args,
        pm_ret_ty->isVoidTy() ? "" : "pm.invoke");
    be_state->builder->SetInsertPoint(cont_bb);
    *out_val = inv;
    return llvm_gen_be_error_t::ok;
  }

  llvm::CallInst* call = be_state->builder->CreateCall(
      call_fn_ty, func_ptr, call_args, pm_ret_ty->isVoidTy() ? "" : "pm.call");
  *out_val = call;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a pointer-to-member-data field dereference expression.
 * @details Displaces the object pointer by the member offset value with null-offset checking.
 * @param[in] expr Pointer to the field selection expression node (eok_pm_field or eok_pm_points_to_field).
 * @param[out] out_val Receives the pointer to the selected member field on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_pm_field_access(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  an_expr_node_ptr op1 = expr->variant.operation.operands;
  an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
  if (!op1 || !op2) return llvm_gen_be_error_t::invalid_argument;

  llvm::Value* v1 = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Value* v2 = nullptr;
  err = llvm_lower_expression(op2, &v2);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (!v1 || !v2) {
    return llvm_gen_be_error_t::unsupported_expr;
  }

  llvm::Value* obj_val = nullptr;
  llvm::Value* offset_val = nullptr;
  if (v1->getType()->isPointerTy()) {
    obj_val = v1;
    offset_val = v2;
  } else if (v2->getType()->isPointerTy()) {
    obj_val = v2;
    offset_val = v1;
  } else {
    return llvm_gen_be_error_t::unsupported_expr;
  }

  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  *out_val = be_state->builder->CreateInBoundsGEP(i8_ty, obj_val, offset_val, "pm.field_gep");
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Recursively computes the byte offset of `target_base` within `derived_cls`.
 * @details Traverses `base_classes` and `direct_base_classes` hierarchies to resolve single
 *          and multi-level base-to-derived byte displacements.
 * @param[in] derived_cls The derived EDG class type.
 * @param[in] target_base The target base EDG class type.
 * @param[out] out_offset Receives the accumulated byte offset if found.
 * @param[out] out_found Receives true if `target_base` is a base of `derived_cls`.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if output pointers are null.
 */
static llvm_gen_be_error_t find_base_class_offset(
    a_type_ptr derived_cls,
    a_type_ptr target_base,
    int64_t* out_offset,
    bool* out_found) noexcept {
  *out_offset = 0;
  *out_found = false;
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  derived_cls = skip_typerefs(derived_cls);
  target_base = skip_typerefs(target_base);
  if (derived_cls == target_base) {
    *out_found = true;
    return llvm_gen_be_error_t::ok;
  }
  a_class_type_supplement_ptr supp = derived_cls->variant.class_struct_union.extra_info;
  if (!supp) {
    return llvm_gen_be_error_t::ok;
  }
  for (a_base_class_ptr b = supp->base_classes; b != nullptr; b = b->next) {
    if (skip_typerefs(b->type) == target_base) {
      *out_offset = static_cast<int64_t>(b->offset);
      *out_found = true;
      return llvm_gen_be_error_t::ok;
    }
  }
  for (a_base_class_ptr b = supp->direct_base_classes; b != nullptr; b = b->next_direct) {
    if (skip_typerefs(b->type) == target_base) {
      *out_offset = static_cast<int64_t>(b->offset);
      *out_found = true;
      return llvm_gen_be_error_t::ok;
    }
    int64_t sub_off = 0;
    bool sub_found = false;
    LLVM_GEN_BE_CHECK(find_base_class_offset(b->type, target_base, &sub_off, &sub_found));
    if (sub_found) {
      *out_offset = static_cast<int64_t>(b->offset) + sub_off;
      *out_found = true;
      return llvm_gen_be_error_t::ok;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers pointer-to-member conversions between base and derived classes.
 * @details Adjusts the 'adj' displacement or data member offset across inheritance hierarchies.
 * @param[in] expr Pointer to the cast expression node (eok_pm_base_class_cast or eok_pm_derived_class_cast).
 * @param[out] out_val Receives the converted pointer-to-member value on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_pm_cast(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  an_expr_node_ptr sub = expr->variant.operation.operands;
  if (!sub) return llvm_gen_be_error_t::invalid_argument;

  llvm::Value* sub_val = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(sub, &sub_val);
  if (err != llvm_gen_be_error_t::ok) return err;

  int64_t delta = 0;
  if (expr->type && sub->type) {
    a_type_ptr dest_pm = skip_typerefs(expr->type);
    a_type_ptr src_pm = skip_typerefs(sub->type);
    if (dest_pm->kind == tk_ptr_to_member && src_pm->kind == tk_ptr_to_member) {
      a_type_ptr d_cls = skip_typerefs(dest_pm->variant.ptr_to_member.class_of_which_a_member);
      a_type_ptr s_cls = skip_typerefs(src_pm->variant.ptr_to_member.class_of_which_a_member);
      if ((d_cls->kind == tk_class || d_cls->kind == tk_struct) &&
          (s_cls->kind == tk_class || s_cls->kind == tk_struct)) {
        bool found = false;
        LLVM_GEN_BE_CHECK(find_base_class_offset(s_cls, d_cls, &delta, &found));
        if (!found) {
          LLVM_GEN_BE_CHECK(find_base_class_offset(d_cls, s_cls, &delta, &found));
        }
      }
    }
  }
  if (expr->variant.operation.kind == eok_pm_derived_class_cast) {
    delta = -delta;
  }

  if (sub_val && sub_val->getType()->isStructTy()) {
    // Pointer to member function { ptr, adj[, vdisp] } -> adjust 'adj' only when non-null!
    llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
    bool is_arm_itanium = !triple.isWindowsMSVCEnvironment() &&
                          (triple.isAArch64() || triple.isARM() || triple.isThumb());
    int64_t scaled_delta = is_arm_itanium ? (delta * 2) : delta;

    llvm::Value* raw_ptr = be_state->builder->CreateExtractValue(sub_val, 0, "pm.cast_ptr");
    llvm::Value* raw_adj = be_state->builder->CreateExtractValue(sub_val, 1, "pm.cast_adj");
    llvm::Value* ptr_is_null = be_state->builder->CreateIsNull(raw_ptr, "pm.ptr_null");
    llvm::Value* is_null_pmf = ptr_is_null;
    if (is_arm_itanium) {
      llvm::Value* adj_not_virt = be_state->builder->CreateICmpEQ(
          be_state->builder->CreateAnd(raw_adj, llvm::ConstantInt::get(raw_adj->getType(), 1)),
          llvm::ConstantInt::get(raw_adj->getType(), 0));
      is_null_pmf = be_state->builder->CreateAnd(ptr_is_null, adj_not_virt, "pm.arm_null");
    }
    llvm::Value* cand_adj = be_state->builder->CreateAdd(
        raw_adj, llvm::ConstantInt::getSigned(raw_adj->getType(), scaled_delta), "pm.adj_delta");
    llvm::Value* new_adj = be_state->builder->CreateSelect(is_null_pmf, raw_adj, cand_adj, "pm.sel_adj");

    llvm::Value* res = be_state->builder->CreateInsertValue(sub_val, raw_ptr, 0);
    res = be_state->builder->CreateInsertValue(res, new_adj, 1);
    *out_val = res;
    return llvm_gen_be_error_t::ok;
  }

  // Pointer to data member offset -> adjust if offset != -1
  if (sub_val && sub_val->getType()->isIntegerTy()) {
    llvm::Value* is_null = be_state->builder->CreateICmpEQ(
        sub_val, llvm::ConstantInt::getSigned(sub_val->getType(), -1), "pm.is_null");
    llvm::Value* adjusted = be_state->builder->CreateAdd(
        sub_val, llvm::ConstantInt::getSigned(sub_val->getType(), delta), "pm.off_delta");
    *out_val = be_state->builder->CreateSelect(is_null, sub_val, adjusted, "pm.cast_result");
    return llvm_gen_be_error_t::ok;
  }

  *out_val = sub_val;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers equality and inequality comparisons between pointer-to-member operands.
 * @details Emits component-wise comparisons for member functions and integer comparisons for data members.
 * @param[in] expr Pointer to the comparison expression node (eok_eq or eok_ne).
 * @param[out] out_val Receives the computed boolean comparison result on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_pm_comparison(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  an_expr_node_ptr op1 = expr->variant.operation.operands;
  an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
  if (!op1 || !op2) return llvm_gen_be_error_t::invalid_argument;

  llvm::Value* v1 = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Value* v2 = nullptr;
  err = llvm_lower_expression(op2, &v2);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (!v1 || !v2) {
    return llvm_gen_be_error_t::unsupported_expr;
  }

  bool is_eq = (expr->variant.operation.kind == eok_eq);

  if (v1->getType()->isStructTy()) {
    llvm::Triple pm_triple = llvm::Triple(be_state->module->getTargetTriple());
    bool is_arm_pmf = !pm_triple.isWindowsMSVCEnvironment() &&
                      (pm_triple.isAArch64() || pm_triple.isARM() || pm_triple.isThumb());
    // Member function comparison:
    // Standard Itanium: null iff p1 == 0; equal iff (p1 == p2) && (p1 == 0 || a1 == a2).
    // ARM/ARM64 Itanium: virtual slot 0 has p == 0 and (adj & 1) == 1, so null requires (adj & 1) == 0.
    llvm::Value* p1 = be_state->builder->CreateExtractValue(v1, 0, "pm.p1");
    llvm::Value* a1 = be_state->builder->CreateExtractValue(v1, 1, "pm.a1");
    llvm::Value* p2 = be_state->builder->CreateExtractValue(v2, 0, "pm.p2");
    llvm::Value* a2 = be_state->builder->CreateExtractValue(v2, 1, "pm.a2");

    llvm::Value* p_eq = be_state->builder->CreateICmpEQ(p1, p2, "pm.peq");
    llvm::Value* is_null_pmf = be_state->builder->CreateIsNull(p1, "pm.pnull");
    if (is_arm_pmf) {
      llvm::Type* adj_ty = a1->getType();
      llvm::Value* vbit_or = be_state->builder->CreateAnd(
          be_state->builder->CreateOr(a1, a2, "pm.a_or"),
          llvm::ConstantInt::get(adj_ty, 1),
          "pm.vbit");
      llvm::Value* both_non_virt = be_state->builder->CreateICmpEQ(
          vbit_or, llvm::ConstantInt::get(adj_ty, 0), "pm.both_nv");
      is_null_pmf = be_state->builder->CreateAnd(is_null_pmf, both_non_virt, "pm.arm_null");
    }
    llvm::Value* a_eq = be_state->builder->CreateICmpEQ(a1, a2, "pm.aeq");
    llvm::Value* valid_eq = be_state->builder->CreateOr(is_null_pmf, a_eq, "pm.valideq");
    llvm::Value* eq_res = be_state->builder->CreateAnd(p_eq, valid_eq, "pm.eqres");

    *out_val = is_eq ? eq_res : be_state->builder->CreateNot(eq_res, "pm.neres");
    return llvm_gen_be_error_t::ok;
  }

  // Data member integer offset comparison
  llvm::CmpInst::Predicate pred = is_eq ? llvm::CmpInst::ICMP_EQ : llvm::CmpInst::ICMP_NE;
  *out_val = be_state->builder->CreateICmp(pred, v1, v2, "pm.cmp");
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
