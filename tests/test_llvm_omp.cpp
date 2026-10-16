/**
 * @file test_llvm_omp.cpp
 * @brief Unit tests for the LLVM backend OpenMP lowering subsystem.
 */

#include "llvm_gen_be_omp.h"
#include "llvm_gen_be_internal.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

using namespace edg;

namespace edg {
  char* get_mangled_function_name(a_routine_ptr routine) { return nullptr; }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  a_targ_size_t targ_sizeof_pointer = 8;
  
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
      *out_ty = llvm::Type::getInt32Ty(*be_state->context);
      return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t*, a_source_position, llvm::DIScope*, llvm::DILocation**) noexcept {
      return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept {
      *out_const = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
      return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t*) noexcept { return llvm_gen_be_error_t::ok; }
  llvm_gen_be_error_t push_lexical_block(llvm_gen_be_debug_state_t*, a_source_position, llvm::DILexicalBlock**) noexcept { return llvm_gen_be_error_t::ok; }
  
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr ty, llvm::Constant** out_const) noexcept {
      *out_const = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
      return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
  
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
      *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
      return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t llvm_lower_coroutine_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
  llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
}


LLVMBackendState* edg::be_state = nullptr;

int main() {
  printf("Running test_llvm_omp...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_omp", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  a_statement stmt = {};
  stmt.kind = stmk_empty;

  assert(lower_omp_parallel_region(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_parallel_region(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_for_worksharing(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_for_worksharing(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_critical(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_critical(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_barrier(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_barrier(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_target_region(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_target_region(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_task(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_task(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_taskwait(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_taskwait(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_taskgroup(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_taskgroup(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_reduce(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_reduce(&stmt) == llvm_gen_be_error_t::ok);


  assert(lower_omp_sections(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_sections(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_section(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_section(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_single(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_single(&stmt) == llvm_gen_be_error_t::ok);

  printf("PASS\n");
  return 0;
}
