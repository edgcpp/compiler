/**
 * @file test_llvm_exceptions.cpp
 * @brief Unit tests for the LLVM backend Exception Handling subsystem.
 * @details Validates function coverage of llvm_lower_try_block_stmt, llvm_lower_microsoft_try_stmt, and cleanups.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_eh.h"
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
  a_source_file_ptr conv_seq_to_file_and_line(a_seq_number, a_const_char**, a_const_char**, a_line_number*, a_boolean*) { return nullptr; }
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
    if (out_val) *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 1);
    return llvm_gen_be_error_t::ok;
  }
}

int main() {
  printf("Running test_llvm_exceptions...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_eh", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  // 1. Negative tests for try_block and cleanup
  assert(llvm_lower_try_block_stmt(nullptr) == llvm_gen_be_error_t::invalid_argument);
  a_statement wrong_stmt = {};
  wrong_stmt.kind = stmk_empty;
  assert(llvm_lower_try_block_stmt(&wrong_stmt) == llvm_gen_be_error_t::invalid_argument);

  a_routine r = {};
  llvm::Value* obj = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
  assert(llvm_emit_eh_cleanup(nullptr, obj) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_eh_cleanup(&r, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_eh_cleanup(&r, obj) == llvm_gen_be_error_t::ok);

  // 2. Itanium ABI try-catch test
  a_statement stmt = {};
  stmt.kind = stmk_try_block;
  a_try_supplement try_block = {};
  stmt.variant.try_block = &try_block;
  
  a_handler handler = {};
  try_block.handlers = &handler;
  a_variable param = {};
  a_type ty = {};
  ty.kind = tk_integer;
  param.type = &ty;
  handler.parameter = &param;
  
  a_statement body = {};
  body.kind = stmk_empty;
  try_block.statement = &body;
  handler.statement = &body;

  assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // 3. MSVC ABI C++ try-catch test
  state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
  bb = llvm::BasicBlock::Create(*state.context, "entry2", f);
  state.builder->SetInsertPoint(bb);
  assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // 4. MSVC SEH __try / __except test
  bb = llvm::BasicBlock::Create(*state.context, "entry3", f);
  state.builder->SetInsertPoint(bb);

  assert(llvm_lower_microsoft_try_stmt(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_microsoft_try_stmt(&wrong_stmt) == llvm_gen_be_error_t::invalid_argument);

  a_statement ms_stmt = {};
  ms_stmt.kind = stmk_microsoft_try;
  a_microsoft_try_supplement ms_sup = {};
  ms_stmt.variant.microsoft_try = &ms_sup;
  ms_sup.guarded_statement = &body;
  an_expr_node except_expr = {};
  except_expr.kind = enk_constant;
  ms_sup.except_expr = &except_expr;
  ms_sup.cleanup_statement = &body;

  assert(llvm_lower_microsoft_try_stmt(&ms_stmt) == llvm_gen_be_error_t::ok);

  // 5. MSVC SEH __try / __finally test
  bb = llvm::BasicBlock::Create(*state.context, "entry4", f);
  state.builder->SetInsertPoint(bb);

  a_statement ms_finally_stmt = {};
  ms_finally_stmt.kind = stmk_microsoft_try;
  a_microsoft_try_supplement ms_finally_sup = {};
  ms_finally_stmt.variant.microsoft_try = &ms_finally_sup;
  ms_finally_sup.guarded_statement = &body;
  ms_finally_sup.except_expr = nullptr;
  ms_finally_sup.cleanup_statement = &body;

  // 6. Intrinsic Exception Functions & SEH setjmp/longjmp
  bb = llvm::BasicBlock::Create(*state.context, "entry_intrinsics", f);
  state.builder->SetInsertPoint(bb);
  llvm::Value* out_val = nullptr;

  assert(llvm_lower_eh_exceptioncode(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_eh_exceptioncode(&out_val) == llvm_gen_be_error_t::ok);

  assert(llvm_lower_eh_exceptioninfo(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_eh_exceptioninfo(&out_val) == llvm_gen_be_error_t::ok);

  llvm::FunctionType* filter_ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(*state.context), {llvm::PointerType::getUnqual(*state.context), llvm::PointerType::getUnqual(*state.context)}, false);
  llvm::Function* filter_func = llvm::Function::Create(filter_ft, llvm::Function::InternalLinkage, "filt2", state.module.get());
  llvm::BasicBlock* filter_entry = llvm::BasicBlock::Create(*state.context, "entry", filter_func);
  state.builder->SetInsertPoint(filter_entry);
  assert(llvm_lower_eh_exceptioninfo(&out_val) == llvm_gen_be_error_t::ok);

  llvm::Value* jmp_buf = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
  llvm::Value* longjmp_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 1);
  
  assert(llvm_lower_seh_setjmp(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_seh_setjmp(jmp_buf, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_seh_setjmp(jmp_buf, &out_val) == llvm_gen_be_error_t::ok);

  assert(llvm_lower_seh_longjmp(nullptr, longjmp_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_seh_longjmp(jmp_buf, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_seh_longjmp(jmp_buf, longjmp_val) == llvm_gen_be_error_t::ok);

  // 7. Exception Specifications
  a_routine spec_rout = {};
  a_type spec_rout_ty = {};
  spec_rout_ty.kind = tk_routine;
  spec_rout.type = &spec_rout_ty;
  a_routine_type_supplement rts = {};
  spec_rout_ty.variant.routine.extra_info = &rts;

  an_exception_specification noexcept_spec = {};
  noexcept_spec.is_noexcept = TRUE;
  rts.exception_specification = &noexcept_spec;

  llvm::BasicBlock* lpad_bb = llvm::BasicBlock::Create(*state.context, "lpad", f);
  assert(llvm_emit_exception_specification_lpad(nullptr, lpad_bb) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_exception_specification_lpad(&spec_rout, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_exception_specification_lpad(&spec_rout, lpad_bb) == llvm_gen_be_error_t::ok);

  an_exception_specification throw_spec = {};
  throw_spec.is_noexcept = FALSE;
  an_exception_specification_type ex_ty = {};
  ex_ty.type = &ty;
  throw_spec.variant.exception_specification_type_list = &ex_ty;
  rts.exception_specification = &throw_spec;

  llvm::BasicBlock* lpad2_bb = llvm::BasicBlock::Create(*state.context, "lpad2", f);
  assert(llvm_emit_exception_specification_lpad(&spec_rout, lpad2_bb) == llvm_gen_be_error_t::ok);

  printf("PASS\n");
  return 0;
}
