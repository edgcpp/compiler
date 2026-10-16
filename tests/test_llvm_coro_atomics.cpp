/**
 * @file test_llvm_coro_atomics.cpp
 * @brief Unit tests for the LLVM backend coroutine and atomic lowering subsystem.
 * @details Validates function coverage of llvm_lower_coroutine_stmt, llvm_lower_coroutine_return_stmt, and co_await/co_yield expressions.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_coro.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  // Stubs
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { return TRUE; }
  unsigned int targ_char_bit = 8;
  a_targ_size_t targ_sizeof_short = 2;
  a_targ_size_t targ_sizeof_int = 4;
  a_targ_size_t targ_sizeof_long = 8;
  a_targ_size_t targ_sizeof_long_long = 8;
  a_targ_size_t targ_sizeof_pointer = 8;
  a_boolean targ_little_endian = TRUE;
  a_C_dialect C_dialect = static_cast<a_C_dialect>(0);
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTIi"; }
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept { 
    if (ty->isIntegerTy()) *out_const = llvm::ConstantInt::get(ty, 1);
    else if (ty->isFloatingPointTy()) *out_const = llvm::ConstantFP::get(ty, 1.0);
    else *out_const = llvm::Constant::getNullValue(ty);
    return llvm_gen_be_error_t::ok; 
  }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_ti) noexcept {
    *out_ti = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    if (expr == nullptr) {
       *out_val = nullptr;
       return llvm_gen_be_error_t::ok;
    }
    if (expr->type) {
        if (expr->type->kind == tk_pointer) {
            *out_val = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
            return llvm_gen_be_error_t::ok;
        } else if (expr->type->kind == tk_integer && expr->type->variant.integer.bool_type) {
            *out_val = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*be_state->context), 1);
            return llvm_gen_be_error_t::ok;
        }
    }
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 1);
    return llvm_gen_be_error_t::ok;
  }
}

int main() {
  printf("Running test_llvm_coro_atomics...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_coro", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  // 1. Negative tests
  assert(llvm_lower_coroutine_stmt(nullptr) == llvm_gen_be_error_t::invalid_argument);
  a_statement wrong_stmt = {};
  wrong_stmt.kind = stmk_empty;
  assert(llvm_lower_coroutine_stmt(&wrong_stmt) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_coroutine_return_stmt(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_coroutine_return_stmt(&wrong_stmt) == llvm_gen_be_error_t::invalid_argument);

  llvm::Value* coro_val = nullptr;
  assert(llvm_lower_coro_suspend(nullptr, &coro_val) == llvm_gen_be_error_t::invalid_argument);
  an_expr_node wrong_expr = {};
  wrong_expr.kind = enk_constant;
  assert(llvm_lower_coro_suspend(&wrong_expr, &coro_val) == llvm_gen_be_error_t::invalid_argument);

  // 2. Coroutine setup statement
  a_statement stmt = {};
  stmt.kind = stmk_coroutine;
  a_coroutine_descr descr = {};
  stmt.variant.coroutine.descr = &descr;
  
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // Custom allocator
  a_routine new_rt = {};
  new_rt.source_corresp.name = "custom_new";
  descr.new_routine = &new_rt;
  
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*state.context);
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*state.context);
  llvm::FunctionType* custom_new_fty = llvm::FunctionType::get(int8_ptr_ty, {llvm::Type::getInt64Ty(*state.context), int32_ty}, false);
  llvm::Function::Create(custom_new_fty, llvm::Function::ExternalLinkage, "custom_new", state.module.get());
  
  // Create an argument in the parent function
  llvm::FunctionType* f_with_args = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {int32_ty}, false);
  llvm::Function* parent_f = llvm::Function::Create(f_with_args, llvm::Function::ExternalLinkage, "coro_func", state.module.get());
  llvm::BasicBlock* bb2 = llvm::BasicBlock::Create(*state.context, "entry", parent_f);
  state.builder->SetInsertPoint(bb2);
  
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

  an_expr_node unhandled_ex = {};
  unhandled_ex.kind = enk_constant;
  descr.unhandled_exception_call = &unhandled_ex;
  a_label fin_lbl = {};
  descr.final_suspend_label = &fin_lbl;
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // 3. Coroutine return statement
  bb = llvm::BasicBlock::Create(*state.context, "entry_ret", f);
  state.builder->SetInsertPoint(bb);
  stmt.kind = stmk_coroutine_return;
  assert(llvm_lower_coroutine_return_stmt(&stmt) == llvm_gen_be_error_t::ok);
  
  // 4. Coroutine co_await expression
  bb = llvm::BasicBlock::Create(*state.context, "entry_await", f);
  state.builder->SetInsertPoint(bb);

  an_expr_node ready_op = {};
  ready_op.kind = enk_constant;
  an_expr_node resume_op = {};
  resume_op.kind = enk_constant;
  an_expr_node suspend_op = {};
  suspend_op.kind = enk_constant;

  ready_op.next = &resume_op;
  resume_op.next = &suspend_op;

  an_expr_node await_expr = {};
  await_expr.kind = enk_await;
  await_expr.variant.await_info.ready_resume_suspend = &ready_op;
  assert(llvm_lower_coro_suspend(&await_expr, &coro_val) == llvm_gen_be_error_t::ok);
  assert(coro_val != nullptr);

  // Symmetric transfer await_suspend (pointer)
  a_type ptr_ty = {};
  ptr_ty.kind = tk_pointer;
  an_expr_node suspend_ptr_op = {};
  suspend_ptr_op.kind = enk_constant;
  suspend_ptr_op.type = &ptr_ty;
  resume_op.next = &suspend_ptr_op;
  bb = llvm::BasicBlock::Create(*state.context, "entry_await_ptr", f);
  state.builder->SetInsertPoint(bb);
  assert(llvm_lower_coro_suspend(&await_expr, &coro_val) == llvm_gen_be_error_t::ok);

  // Bool await_suspend (boolean)
  a_type bool_ty = {};
  bool_ty.kind = tk_integer;
  bool_ty.variant.integer.bool_type = TRUE;
  an_expr_node suspend_bool_op = {};
  suspend_bool_op.kind = enk_constant;
  suspend_bool_op.type = &bool_ty;
  resume_op.next = &suspend_bool_op;
  bb = llvm::BasicBlock::Create(*state.context, "entry_await_bool", f);
  state.builder->SetInsertPoint(bb);
  assert(llvm_lower_coro_suspend(&await_expr, &coro_val) == llvm_gen_be_error_t::ok);

  // 5. Coroutine co_yield expression
  bb = llvm::BasicBlock::Create(*state.context, "entry_yield", f);
  state.builder->SetInsertPoint(bb);

  an_expr_node yield_expr = {};
  yield_expr.kind = enk_yield;
  yield_expr.variant.await_info.ready_resume_suspend = &ready_op;
  assert(llvm_lower_coro_suspend(&yield_expr, &coro_val) == llvm_gen_be_error_t::ok);
  assert(coro_val != nullptr);

  printf("PASS\n");
  return 0;
}
