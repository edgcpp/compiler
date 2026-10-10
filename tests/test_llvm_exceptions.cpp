/**
 * @file test_llvm_exceptions.cpp
 * @brief Unit tests for the LLVM backend Exception Handling subsystem.
 * @details Validates function coverage of llvm_lower_try_block_stmt and cleanups.
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

  // Test Itanium ABI
  assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // Test MSVC ABI
  state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
  bb = llvm::BasicBlock::Create(*state.context, "entry2", f);
  state.builder->SetInsertPoint(bb);
  
  a_routine r = {};
  llvm::Value* obj = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
  assert(llvm_emit_eh_cleanup(&r, obj) == llvm_gen_be_error_t::ok);
  
  assert(llvm_lower_try_block_stmt(&stmt) == llvm_gen_be_error_t::ok);

  printf("PASS\n");
  return 0;
}
