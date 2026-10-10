/**
 * @file test_llvm_opt_pipeline.cpp
 * @brief Unit tests for the LLVM backend optimization pipeline.
 * @details Validates function coverage of llvm_gen_be_opt.cpp.
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_opt.h"
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
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
}

int main() {
  printf("Running test_llvm_opt_pipeline...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_opt", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);
  state.builder->CreateRetVoid();

  llvm_opt_options_t opts = {};
  assert(parse_opt_level_string("O0", &opts) == llvm_gen_be_error_t::ok);
  assert(opts.opt_level == llvm_opt_level_t::O0);
  assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);

  assert(parse_opt_level_string("O3", &opts) == llvm_gen_be_error_t::ok);
  opts.use_asan = true;
  opts.use_tsan = true;
  opts.use_ubsan = true;
  assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);
  
  assert(parse_opt_level_string("Os", &opts) == llvm_gen_be_error_t::ok);
  opts.use_lto = true;
  assert(run_optimization_pipeline(state.module.get(), &opts) == llvm_gen_be_error_t::ok);

  assert(parse_opt_level_string("invalid", &opts) == llvm_gen_be_error_t::invalid_argument);

  printf("PASS\n");
  return 0;
}
