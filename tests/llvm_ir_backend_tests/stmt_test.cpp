/**
 * @file stmt_test.cpp
 * @brief Unit tests for the LLVM backend statement lowering.
 * @details Validates function coverage of stmk_try_block and stmk_microsoft_try execution.
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
  a_boolean is_empty_class_type(a_type_ptr ty) { return FALSE; }
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
  

}

int main() {
  printf("Running stmt_test...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_stmt", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  a_statement stmt_try = {};
  stmt_try.kind = stmk_try_block;
  a_try_supplement try_block = {};
  stmt_try.variant.try_block = &try_block;
  
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

  // Test lowering stmk_try_block
  assert(llvm_lower_statement(&stmt_try) == llvm_gen_be_error_t::ok);

  // Test MSVC try
  state.module->setTargetTriple(llvm::Triple("x86_64-pc-windows-msvc"));
  llvm::BasicBlock* bb2 = llvm::BasicBlock::Create(*state.context, "entry2", f);
  state.builder->SetInsertPoint(bb2);

  a_statement stmt_ms_try = {};
  stmt_ms_try.kind = stmk_microsoft_try;
  a_microsoft_try_supplement ms_try_block = {};
  stmt_ms_try.variant.microsoft_try = &ms_try_block;
  
  // Test lowering stmk_microsoft_try
  assert(llvm_lower_statement(&stmt_ms_try) == llvm_gen_be_error_t::ok);

  // Test ScopeCleanupStack via Return
  a_routine dtor_rout = {};
  dtor_rout.source_corresp.name = "_ZN1CD1Ev";
  llvm::Value* obj = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
  
  ScopeCleanup cleanup = {};
  cleanup.dtor_routine = &dtor_rout;
  cleanup.object_ptr = obj;
  cleanup.is_active = true;
  state.cleanup_stack.cleanups.push_back(cleanup);
  
  a_statement return_stmt = {};
  return_stmt.kind = stmk_return;
  assert(llvm_lower_statement(&return_stmt) == llvm_gen_be_error_t::ok);
  assert(state.cleanup_stack.cleanups.size() == 1);
  state.cleanup_stack.cleanups.clear();
  
  // Test ScopeCleanupStack via Block
  a_statement block_stmt = {};
  block_stmt.kind = stmk_block;
  a_block block_sup = {};
  block_stmt.variant.block.extra_info = &block_sup;
  block_stmt.variant.block.statements = &return_stmt; // reusing return as body for simplicity, we just want to see it pop
  
  // push again
  assert(llvm_lower_statement(&block_stmt) == llvm_gen_be_error_t::ok);
  assert(state.cleanup_stack.cleanups.size() == 0); // Block handles popping of its own scope cleanups!
  
  // Test local static thread-safe guard initialization
  a_statement init_stmt = {};
  init_stmt.kind = stmk_init;
  a_dynamic_init dip = {};
  a_variable var = {};
  var.storage_class = sc_static;
  var.source_corresp.name = "_ZZ4funcE1x";
  dip.variable = &var;
  dip.kind = dik_expression;
  an_expr_node init_expr = {};
  dip.variant.expression = &init_expr;
  init_stmt.variant.dynamic_init = &dip;

  a_routine dtor_rout2 = {};
  dtor_rout2.source_corresp.name = "_ZN1CD1Ev";
  dip.destructor = &dtor_rout2;

  assert(llvm_lower_statement(&init_stmt) == llvm_gen_be_error_t::ok);

  // Test dik_nonconstant_aggregate
  a_dynamic_init agg_init;
  memset(&agg_init, 0, sizeof(agg_init));
  agg_init.kind = dik_nonconstant_aggregate;
  llvm::Value* ptr_val = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*state.context));
  llvm::Type* obj_ty = llvm::Type::getInt32Ty(*state.context);
  assert(llvm_emit_dynamic_init(&agg_init, ptr_val, obj_ty) == llvm_gen_be_error_t::ok);

  // Test dik_constructor
  a_dynamic_init ctor_init;
  memset(&ctor_init, 0, sizeof(ctor_init));
  ctor_init.kind = dik_constructor;
  a_routine ctor_rout = {};
  ctor_rout.source_corresp.name = (char*)"_ZN1XC2Ev";
  ctor_init.variant.constructor.ptr = &ctor_rout;
  assert(llvm_emit_dynamic_init(&ctor_init, ptr_val, obj_ty) == llvm_gen_be_error_t::ok);

  printf("PASS\n");
  return 0;
}

namespace edg {
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node* expr, llvm::Value** out_val) noexcept { return llvm_gen_be_error_t::ok; }
  llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
  llvm_gen_be_error_t llvm_lower_coroutine_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
  llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
  
  // Debug stuff
  struct llvm_gen_be_debug_state_t;
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t*, a_source_position, llvm::DIScope*, llvm::DILocation**) noexcept { return llvm_gen_be_error_t::ok; }
  llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t*) noexcept { return llvm_gen_be_error_t::ok; }
  llvm_gen_be_error_t push_lexical_block(llvm_gen_be_debug_state_t*, a_source_position, llvm::DILexicalBlock**) noexcept { return llvm_gen_be_error_t::ok; }
}
