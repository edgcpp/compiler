/**
 * @file llvm_gen_be_ptrmem.cpp
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

  llvm::Triple triple(be_state->module->getTargetTriple());
  bool is_msvc = triple.isWindowsMSVCEnvironment();

  a_type_ptr mem_ty = type->variant.ptr_to_member.type;
  bool is_func = (mem_ty != nullptr && skip_typerefs(mem_ty)->kind == tk_routine);

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* int_ty = llvm::Type::getInt64Ty(*be_state->context);
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);

  if (is_msvc) {
    if (is_func) {
      a_type_ptr class_ty = skip_typerefs(type->variant.ptr_to_member.class_of_which_a_member);
      a_class_type_supplement_ptr supp = class_ty ? class_ty->variant.class_struct_union.extra_info : nullptr;
      int base_count = 0;
      for (a_base_class_ptr b = supp ? supp->direct_base_classes : nullptr; b != nullptr; b = b->next_direct) {
        base_count++;
      }
      if (base_count <= 1 && (!class_ty || !class_ty->variant.class_struct_union.any_virtual_base_classes)) {
        // Single inheritance: 1 word
        *out_type = ptr_ty;
      } else if (!class_ty || !class_ty->variant.class_struct_union.any_virtual_base_classes) {
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
      *out_const = llvm::ConstantInt::get(expected_ty, -1, true);
    } else {
      *out_const = llvm::ConstantInt::get(i64_ty, -1, true);
    }
    return llvm_gen_be_error_t::ok;
  }

  if (con->variant.ptr_to_member.is_function_ptr) {
    a_routine_ptr r = con->variant.ptr_to_member.variant.routine;
    if (!r) {
      // Null member function pointer { 0, 0 }
      llvm::StructType* st = llvm::dyn_cast_or_null<llvm::StructType>(expected_ty);
      if (!st) st = llvm::StructType::get(*be_state->context, {ptr_ty, i64_ty});
      *out_const = llvm::ConstantStruct::get(
          st, {llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty)),
               llvm::ConstantInt::get(i64_ty, 0)});
      return llvm_gen_be_error_t::ok;
    }

    int64_t this_adj = 0;
    if (con->variant.ptr_to_member.casting_base_class) {
      this_adj = con->variant.ptr_to_member.casting_base_class->offset;
    }

    llvm::Constant* ptr_part = nullptr;
    if (r->number.virtual_function > 0) {
      // Virtual method: Word 0 is (vtable_index * sizeof(ptr)) + 1
      int64_t vtbl_byte_off = ((r->number.virtual_function - 1) * 8) + 1;
      ptr_part = llvm::ConstantExpr::getIntToPtr(
          llvm::ConstantInt::get(i64_ty, vtbl_byte_off), ptr_ty);
    } else {
      // Non-virtual method: Word 0 is function address
      const char* fname = r->source_corresp.name ? r->source_corresp.name : "routine";
      llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(
          fname, llvm::FunctionType::get(ptr_ty, {}, false));
      ptr_part = llvm::cast<llvm::Constant>(callee.getCallee());
    }

    llvm::StructType* st = llvm::dyn_cast_or_null<llvm::StructType>(expected_ty);
    if (!st) st = llvm::StructType::get(*be_state->context, {ptr_ty, i64_ty});

    *out_const = llvm::ConstantStruct::get(
        st, {ptr_part, llvm::ConstantInt::get(i64_ty, this_adj)});
    return llvm_gen_be_error_t::ok;
  }

  // Data member pointer constant
  a_field_ptr f = con->variant.ptr_to_member.variant.field;
  if (!f) {
    // Null data member pointer is -1
    *out_const = llvm::ConstantInt::get(
        expected_ty ? expected_ty : i64_ty, -1, true);
    return llvm_gen_be_error_t::ok;
  }

  int64_t offset = f->offset;
  if (con->variant.ptr_to_member.casting_base_class) {
    offset += con->variant.ptr_to_member.casting_base_class->offset;
  }

  *out_const = llvm::ConstantInt::get(
      expected_ty ? expected_ty : i64_ty, offset);
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

  if (pm_val && pm_val->getType()->isStructTy()) {
    // Itanium ABI { ptr, adj }
    llvm::Value* raw_ptr = be_state->builder->CreateExtractValue(pm_val, 0, "pm.ptr");
    llvm::Value* adj = be_state->builder->CreateExtractValue(pm_val, 1, "pm.adj");

    // Adjust 'this' pointer: this + adj
    adj_this = be_state->builder->CreateInBoundsGEP(i8_ty, this_val, adj, "pm.this_adj");

    // Test bit 0 of raw_ptr
    llvm::Value* ptr_int = be_state->builder->CreatePtrToInt(raw_ptr, i64_ty, "pm.ptr_int");
    llvm::Value* one = llvm::ConstantInt::get(i64_ty, 1);
    llvm::Value* is_virt = be_state->builder->CreateICmpNE(
        be_state->builder->CreateAnd(ptr_int, one), llvm::ConstantInt::get(i64_ty, 0), "pm.is_virt");

    llvm::Function* parent = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* virt_bb = llvm::BasicBlock::Create(*be_state->context, "pm.virt", parent);
    llvm::BasicBlock* nonvirt_bb = llvm::BasicBlock::Create(*be_state->context, "pm.nonvirt", parent);
    llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*be_state->context, "pm.cont", parent);

    be_state->builder->CreateCondBr(is_virt, virt_bb, nonvirt_bb);

    // Non-virtual block: raw_ptr is function pointer directly
    be_state->builder->SetInsertPoint(nonvirt_bb);
    be_state->builder->CreateBr(cont_bb);

    // Virtual block: load vtable, load slot at (ptr_int - 1)
    be_state->builder->SetInsertPoint(virt_bb);
    llvm::Value* vptr = be_state->builder->CreateLoad(ptr_ty, adj_this, "pm.vptr");
    llvm::Value* voff = be_state->builder->CreateSub(ptr_int, one, "pm.voff");
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
  call_args.push_back(adj_this);
  for (an_expr_node_ptr arg = op2->next; arg != nullptr; arg = arg->next) {
    llvm::Value* arg_val = nullptr;
    err = llvm_lower_expression(arg, &arg_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    call_args.push_back(arg_val);
  }

  llvm::FunctionType* call_fn_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty}, false);
  llvm::CallInst* call = be_state->builder->CreateCall(call_fn_ty, func_ptr, call_args, "pm.call");
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
      if (d_cls && s_cls && (d_cls->kind == tk_class || d_cls->kind == tk_struct) &&
          (s_cls->kind == tk_class || s_cls->kind == tk_struct)) {
        a_class_type_supplement_ptr supp = s_cls->variant.class_struct_union.extra_info;
        if (supp && supp->direct_base_classes) {
          for (a_base_class_ptr b = supp->direct_base_classes; b != nullptr; b = b->next_direct) {
            if (b->type == d_cls) {
              delta = b->offset;
              break;
            }
          }
        }
      }
    }
  }
  if (expr->variant.operation.kind == eok_pm_derived_class_cast) {
    delta = -delta;
  }

  llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);

  if (sub_val && sub_val->getType()->isStructTy()) {
    // Pointer to member function { ptr, adj } -> adjust 'adj'
    llvm::Value* raw_ptr = be_state->builder->CreateExtractValue(sub_val, 0, "pm.cast_ptr");
    llvm::Value* raw_adj = be_state->builder->CreateExtractValue(sub_val, 1, "pm.cast_adj");
    llvm::Value* new_adj = be_state->builder->CreateAdd(
        raw_adj, llvm::ConstantInt::get(i64_ty, delta), "pm.adj_delta");

    llvm::Value* res = be_state->builder->CreateInsertValue(sub_val, raw_ptr, 0);
    res = be_state->builder->CreateInsertValue(res, new_adj, 1);
    *out_val = res;
    return llvm_gen_be_error_t::ok;
  }

  // Pointer to data member offset -> adjust if offset != -1
  if (sub_val && sub_val->getType()->isIntegerTy()) {
    llvm::Value* is_null = be_state->builder->CreateICmpEQ(
        sub_val, llvm::ConstantInt::get(sub_val->getType(), -1, true), "pm.is_null");
    llvm::Value* adjusted = be_state->builder->CreateAdd(
        sub_val, llvm::ConstantInt::get(sub_val->getType(), delta), "pm.off_delta");
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

  bool is_eq = (expr->variant.operation.kind == eok_eq);

  if (v1 && v2 && v1->getType()->isStructTy() && v2->getType()->isStructTy()) {
    // Member function comparison: (p1 == p2) && (p1 == 0 || a1 == a2)
    llvm::Value* p1 = be_state->builder->CreateExtractValue(v1, 0, "pm.p1");
    llvm::Value* a1 = be_state->builder->CreateExtractValue(v1, 1, "pm.a1");
    llvm::Value* p2 = be_state->builder->CreateExtractValue(v2, 0, "pm.p2");
    llvm::Value* a2 = be_state->builder->CreateExtractValue(v2, 1, "pm.a2");

    llvm::Value* p_eq = be_state->builder->CreateICmpEQ(p1, p2, "pm.peq");
    llvm::Value* p_null = be_state->builder->CreateIsNull(p1, "pm.pnull");
    llvm::Value* a_eq = be_state->builder->CreateICmpEQ(a1, a2, "pm.aeq");
    llvm::Value* valid_eq = be_state->builder->CreateOr(p_null, a_eq, "pm.valideq");
    llvm::Value* eq_res = be_state->builder->CreateAnd(p_eq, valid_eq, "pm.eqres");

    *out_val = is_eq ? eq_res : be_state->builder->CreateNot(eq_res, "pm.neres");
    return llvm_gen_be_error_t::ok;
  }

  // Data member integer offset comparison
  if (v1 && v2) {
    llvm::CmpInst::Predicate pred = is_eq ? llvm::CmpInst::ICMP_EQ : llvm::CmpInst::ICMP_NE;
    *out_val = be_state->builder->CreateICmp(pred, v1, v2, "pm.cmp");
    return llvm_gen_be_error_t::ok;
  }

  return llvm_gen_be_error_t::unsupported_expr;
}

END_EDG_NAMESPACE
#endif
