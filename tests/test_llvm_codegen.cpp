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
#include <llvm/ADT/SmallVector.h>

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
  assert(create_target_machine("x86_64-pc-linux-gnu", "generic", "", llvm::CodeGenOptLevel::None, std::nullopt, std::nullopt, &tm) == llvm_gen_be_error_t::ok);
  assert(tm != nullptr);

  // Test caching
  llvm::TargetMachine* tm2 = nullptr;
  assert(create_target_machine("x86_64-pc-linux-gnu", "generic", "", llvm::CodeGenOptLevel::None, std::nullopt, std::nullopt, &tm2) == llvm_gen_be_error_t::ok);
  assert(tm == tm2); // Should return cached instance

  // Test explicit reloc and code models
  llvm::TargetMachine* tm_pic = nullptr;
  assert(create_target_machine("x86_64-pc-linux-gnu", "generic", "", llvm::CodeGenOptLevel::None, llvm::Reloc::PIC_, llvm::CodeModel::Small, &tm_pic) == llvm_gen_be_error_t::ok);
  assert(tm_pic != nullptr && tm_pic != tm); // Different parameters, different cache entry

  assert(emit_machine_code_to_file(state.module.get(), tm, codegen_file_type_t::llvm_ir_text, "test.ll") == llvm_gen_be_error_t::ok);
  assert(emit_machine_code_to_file(state.module.get(), tm, codegen_file_type_t::bitcode_file, "test.bc") == llvm_gen_be_error_t::ok);
  assert(emit_assembly_file(state.module.get(), tm, "test_direct.s") == llvm_gen_be_error_t::ok);
  assert(emit_object_file(state.module.get(), tm, "test_direct.o") == llvm_gen_be_error_t::ok);

  llvm::SmallVector<char, 1024> buffer;
  assert(emit_machine_code_to_memory_buffer(state.module.get(), tm, codegen_file_type_t::object_file, buffer) == llvm_gen_be_error_t::ok);
  assert(!buffer.empty());

  assert(enable_split_dwarf_emission(tm, "test.dwo") == llvm_gen_be_error_t::ok);
  
  assert(emit_thinlto_summary(state.module.get(), "test.thinlto.bc") == llvm_gen_be_error_t::ok);

  // Test IO error
  assert(emit_machine_code_to_file(state.module.get(), tm, codegen_file_type_t::llvm_ir_text, "/invalid_path_that_does_not_exist/test.ll") == llvm_gen_be_error_t::io_error);
  assert(emit_thinlto_summary(state.module.get(), "/invalid_path_that_does_not_exist/test.thinlto.bc") == llvm_gen_be_error_t::io_error);
  
  // Test invalid arguments
  assert(create_target_machine(nullptr, nullptr, nullptr, llvm::CodeGenOptLevel::None, std::nullopt, std::nullopt, &tm) == llvm_gen_be_error_t::invalid_argument);
  assert(emit_machine_code_to_memory_buffer(nullptr, tm, codegen_file_type_t::object_file, buffer) == llvm_gen_be_error_t::invalid_argument);
  assert(emit_thinlto_summary(nullptr, "test.thinlto.bc") == llvm_gen_be_error_t::invalid_argument);
  assert(enable_split_dwarf_emission(nullptr, "test.dwo") == llvm_gen_be_error_t::invalid_argument);

  printf("PASS\n");
  return 0;
}
