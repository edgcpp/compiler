/**
 * @file test_llvm_control_flow.cpp
 * @brief Unit tests for the LLVM backend control flow statements lowering subsystem.
 * @details Validates function coverage of llvm_lower_statement.
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_debug.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  // Stub implementations required to link test
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
  char* mangled_typeinfo_name(a_type_ptr type) { static char buf[] = "_ZTIi"; return buf; }
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
  
  llvm_gen_be_error_t push_lexical_block(llvm_gen_be_debug_state_t* dbg, a_source_position pos, llvm::DILexicalBlock** out_block) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t* dbg) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t* dbg, a_source_position pos, llvm::DIScope* scope, llvm::DILocation** out_loc) noexcept {
    *out_loc = nullptr;
    return llvm_gen_be_error_t::ok;
  }

  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    if (expr == nullptr) {
       *out_val = nullptr;
       return llvm_gen_be_error_t::ok;
    }
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 1);
    return llvm_gen_be_error_t::ok;
  }
  
  // named_register_names array stub
  const char* named_register_names[anr_last + 1] = {0};
}

int main() {
  printf("Running test_llvm_control_flow...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_cf", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  a_statement stmt = {};
  stmt.kind = stmk_empty;
  assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);

  // stmk_expr
  {
      a_statement stmt = {};
      stmt.kind = stmk_expr;
      an_expr_node expr = {};
      stmt.expr = &expr;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }
  
  // stmk_return
  {
      a_statement stmt = {};
      stmt.kind = stmk_return;
      an_expr_node expr = {};
      stmt.expr = &expr;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_if
  {
      a_statement stmt = {};
      stmt.kind = stmk_if;
      an_expr_node expr = {};
      stmt.expr = &expr;
      a_statement then_stmt = {};
      then_stmt.kind = stmk_empty;
      stmt.variant.if_stmt.then_statement = &then_stmt;
      a_statement else_stmt = {};
      else_stmt.kind = stmk_empty;
      stmt.variant.if_stmt.else_statement = &else_stmt;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_block
  {
      a_statement stmt = {};
      stmt.kind = stmk_block;
      a_statement inner = {};
      inner.kind = stmk_empty;
      stmt.variant.block.statements = &inner;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_while
  {
      a_statement stmt = {};
      stmt.kind = stmk_while;
      an_expr_node expr = {};
      stmt.expr = &expr;
      a_statement body = {};
      body.kind = stmk_empty;
      stmt.variant.loop_statement = &body;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_for
  {
      a_statement stmt = {};
      stmt.kind = stmk_for;
      an_expr_node expr = {};
      stmt.expr = &expr;
      a_statement body = {};
      body.kind = stmk_empty;
      a_for_loop loop_info = {};
      a_statement init = {};
      init.kind = stmk_empty;
      loop_info.initialization = &init;
      an_expr_node inc = {};
      loop_info.increment = &inc;
      stmt.variant.for_loop.statement = &body;
      stmt.variant.for_loop.extra_info = &loop_info;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_label
  {
      a_statement stmt = {};
      stmt.kind = stmk_label;
      a_label lbl = {};
      stmt.variant.label.ptr = &lbl;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_goto
  {
      a_statement stmt = {};
      stmt.kind = stmk_goto;
      a_label lbl = {};
      stmt.variant.label.ptr = &lbl;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }
  
  // stmk_end_test_while
  {
      a_statement stmt = {};
      stmt.kind = stmk_end_test_while;
      an_expr_node expr = {};
      stmt.expr = &expr;
      a_statement body = {};
      body.kind = stmk_empty;
      stmt.variant.loop_statement = &body;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_switch
  {
      a_statement stmt = {};
      stmt.kind = stmk_switch;
      an_expr_node expr = {};
      stmt.expr = &expr;
      a_switch_stmt_descr descr = {};
      a_switch_case_entry c1 = {};
      a_switch_case_entry c2 = {};
      c1.next = &c2;
      a_constant con1 = {};
      c1.case_value = &con1;
      a_constant con2 = {};
      c2.case_value = &con2;
      descr.cases = &c1;
      descr.default_case = &c2;
      stmt.variant.switch_stmt.extra_info = &descr;
      a_statement body = {};
      body.kind = stmk_empty;
      stmt.variant.switch_stmt.body_statement = &body;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }



  printf("PASS\n");
  return 0;
}
