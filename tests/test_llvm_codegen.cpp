/**
 * @file test_llvm_codegen.cpp
 * @brief Unit tests for the LLVM backend code generation subsystem.
 * @details Validates function coverage of llvm_gen_be_codegen.cpp.
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_codegen.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;
namespace edg {

  llvm_gen_be_error_t llvm_gen_be_set_error(
      llvm_gen_be_error_context_t* ctx,
      llvm_gen_be_error_t err_code,
      const char* file,
      unsigned line,
      unsigned column,
      const char* format, ...) noexcept {
          return err_code;
  }

}

int main() {
  printf("Running test_llvm_codegen...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_cg", *state.context);
  edg::be_state = &state;

  assert(initialize_llvm_targets() == llvm_gen_be_error_t::ok);

  llvm::TargetMachine* tm = nullptr;
  assert(create_target_machine("x86_64-pc-linux-gnu", "generic", "", llvm::CodeGenOptLevel::None, &tm) == llvm_gen_be_error_t::ok);
  assert(tm != nullptr);

  assert(emit_machine_code_to_file(state.module.get(), tm, codegen_file_type_t::llvm_ir_text, "test.ll") == llvm_gen_be_error_t::ok);
  assert(emit_machine_code_to_file(state.module.get(), tm, codegen_file_type_t::bitcode_file, "test.bc") == llvm_gen_be_error_t::ok);
  assert(emit_machine_code_to_file(state.module.get(), tm, codegen_file_type_t::assembly_file, "test.s") == llvm_gen_be_error_t::ok);
  assert(emit_machine_code_to_file(state.module.get(), tm, codegen_file_type_t::object_file, "test.o") == llvm_gen_be_error_t::ok);

  printf("PASS\n");
  return 0;
}
