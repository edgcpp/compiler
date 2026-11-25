/**
 * @file test_llvm_codegen.cpp
 * @ingroup llvm_gen_be_tests
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

  /**
   * @brief Stub implementation of `llvm_gen_be_set_error` for codegen unit tests.
   * @details Returns `err_code` directly.
   * @param[in,out] ctx Error context pointer.
   * @param[in] err_code Error code to return.
   * @param[in] file Source file name.
   * @param[in] line Source line number.
   * @param[in] column Source column number.
   * @param[in] format Diagnostic format string.
   * @return The passed `err_code`.
   */
  llvm_gen_be_error_t llvm_gen_be_set_error(
      llvm_gen_be_error_context_t* ctx,
      llvm_gen_be_error_t err_code,
      const char* file,
      unsigned line,
      unsigned column,
      const char* format, ...) noexcept {
          (void)ctx;
          (void)file;
          (void)line;
          (void)column;
          (void)format;
          return err_code;
  }

}

/**
 * @brief Entry point for the LLVM backend code generation unit tests.
 * @details Exercises target initialization, `TargetMachine` caching, file/memory emission, and ThinLTO summary output.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_codegen...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_cg", *state.context);
  edg::be_state = &state;

  assert(initialize_llvm_targets() == llvm_gen_be_error_t::ok);

  llvm::TargetMachine* tm = nullptr;
  assert(create_target_machine("x86_64-pc-linux-gnu", "generic", "", llvm_codegen_opt_level_t::None, std::nullopt, std::nullopt, &tm) == llvm_gen_be_error_t::ok);
  assert(tm != nullptr);

  // Test caching
  llvm::TargetMachine* tm2 = nullptr;
  assert(create_target_machine("x86_64-pc-linux-gnu", "generic", "", llvm_codegen_opt_level_t::None, std::nullopt, std::nullopt, &tm2) == llvm_gen_be_error_t::ok);
  assert(tm == tm2); // Should return cached instance

  // Test explicit reloc and code models, plus null cpu/features
  llvm::TargetMachine* tm_pic = nullptr;
  assert(create_target_machine("x86_64-pc-linux-gnu", nullptr, nullptr, llvm_codegen_opt_level_t::None, llvm::Reloc::PIC_, llvm::CodeModel::Small, &tm_pic) == llvm_gen_be_error_t::ok);
  assert(tm_pic != nullptr && tm_pic != tm); // Different parameters, different cache entry

  assert(emit_machine_code_to_file(state.module.get(), tm, codegen_file_type_t::llvm_ir_text, "/tmp/edg_cg_test.ll") == llvm_gen_be_error_t::ok);
  assert(emit_machine_code_to_file(state.module.get(), tm, codegen_file_type_t::bitcode_file, "/tmp/edg_cg_test.bc") == llvm_gen_be_error_t::ok);
  assert(emit_assembly_file(state.module.get(), tm, "/tmp/edg_cg_test.s") == llvm_gen_be_error_t::ok);
  assert(emit_object_file(state.module.get(), tm, "/tmp/edg_cg_test.o") == llvm_gen_be_error_t::ok);

  // Test Mach-O TargetMachine emission with a function and global variable to cover Mach-O Comdat stripping
  llvm::TargetMachine* tm_macho = nullptr;
  assert(create_target_machine("aarch64-apple-darwin", "generic", "", llvm_codegen_opt_level_t::None, std::nullopt, std::nullopt, &tm_macho) == llvm_gen_be_error_t::ok);
  state.module->setDataLayout(tm_macho->createDataLayout());
  llvm::Function* cg_fn = llvm::Function::Create(
      llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
      llvm::GlobalValue::ExternalLinkage, "cg_dummy_fn", state.module.get());
  llvm::BasicBlock* cg_bb = llvm::BasicBlock::Create(*state.context, "entry", cg_fn);
  llvm::ReturnInst::Create(*state.context, cg_bb);
  llvm::GlobalVariable* cg_gv = llvm::cast<llvm::GlobalVariable>(
      state.module->getOrInsertGlobal("cg_dummy_gv", llvm::Type::getInt32Ty(*state.context)));
  cg_gv->setConstant(false);
  cg_gv->setLinkage(llvm::GlobalValue::ExternalLinkage);
  cg_gv->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 0));
  assert(cg_gv != nullptr);
  assert(emit_object_file(state.module.get(), tm_macho, "/tmp/edg_cg_test.o") == llvm_gen_be_error_t::ok);
  cg_fn->eraseFromParent();
  cg_gv->eraseFromParent();
  state.module->setDataLayout("");

  llvm::SmallVector<char, 1024> buffer;
  assert(emit_machine_code_to_memory_buffer(state.module.get(), tm, codegen_file_type_t::object_file, &buffer) == llvm_gen_be_error_t::ok);
  assert(!buffer.empty());
  buffer.clear();
  assert(emit_machine_code_to_memory_buffer(state.module.get(), tm, codegen_file_type_t::assembly_file, &buffer) == llvm_gen_be_error_t::ok);
  assert(!buffer.empty());

  assert(enable_split_dwarf_emission(tm, "test.dwo") == llvm_gen_be_error_t::ok);
  state.split_dwarf_file = "/tmp/edg_cg_test.dwo";
  g_llvm_be_oom_countdown = 0;
  assert(emit_object_file(state.module.get(), tm, "/tmp/edg_cg_test.o") == llvm_gen_be_error_t::out_of_memory);
  g_llvm_be_oom_countdown = -1;
  assert(emit_object_file(state.module.get(), tm, "/tmp/edg_cg_test.o") == llvm_gen_be_error_t::ok);
  std::remove("/tmp/edg_cg_test.dwo");
  state.split_dwarf_file = "/invalid_path_that_does_not_exist/edg_cg_test.dwo";
  assert(emit_object_file(state.module.get(), tm, "/tmp/edg_cg_test.o") == llvm_gen_be_error_t::io_error);
  state.split_dwarf_file.clear();

  state.function_sections = true;
  state.data_sections = true;
  assert(create_target_machine("x86_64-pc-linux-gnu", nullptr, nullptr, llvm_codegen_opt_level_t::None, std::nullopt, std::nullopt, &tm) == llvm_gen_be_error_t::ok);
  state.function_sections = false;
  state.data_sections = false;
  be_state = nullptr;
  assert(create_target_machine("x86_64-pc-linux-gnu", nullptr, nullptr, llvm_codegen_opt_level_t::None, std::nullopt, std::nullopt, &tm) == llvm_gen_be_error_t::ok);
  be_state = &state;
  
  assert(emit_thinlto_summary(state.module.get(), "/tmp/edg_cg_test.thinlto.bc") == llvm_gen_be_error_t::ok);
  assert(emit_llvm_lto_bitcode_file(state.module.get(), "/tmp/edg_cg_test.thinlto.bc", true) == llvm_gen_be_error_t::ok);
  assert(emit_llvm_lto_bitcode_file(state.module.get(), "/tmp/edg_cg_test.bc", false) == llvm_gen_be_error_t::ok);
  assert(emit_llvm_lto_bitcode_file(nullptr, "/tmp/edg_cg_test.bc", false) == llvm_gen_be_error_t::invalid_argument);

  // Test IO error
  assert(emit_machine_code_to_file(state.module.get(), tm, codegen_file_type_t::llvm_ir_text, "/invalid_path_that_does_not_exist/test.ll") == llvm_gen_be_error_t::io_error);
  assert(emit_thinlto_summary(state.module.get(), "/invalid_path_that_does_not_exist/test.thinlto.bc") == llvm_gen_be_error_t::io_error);
  
  // Test invalid arguments and unknown file types
  assert(create_target_machine(nullptr, nullptr, nullptr, llvm_codegen_opt_level_t::None, std::nullopt, std::nullopt, &tm) == llvm_gen_be_error_t::invalid_argument);
  assert(create_target_machine("x86_64-pc-linux-gnu", nullptr, nullptr, llvm_codegen_opt_level_t::None, std::nullopt, std::nullopt, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(create_target_machine("invalid_arch_triple_xyz", nullptr, nullptr, llvm_codegen_opt_level_t::None, std::nullopt, std::nullopt, &tm) == llvm_gen_be_error_t::invalid_argument);

  assert(emit_machine_code_to_file(nullptr, tm, codegen_file_type_t::llvm_ir_text, "/tmp/edg_cg_test.ll") == llvm_gen_be_error_t::invalid_argument);
  assert(emit_machine_code_to_file(state.module.get(), tm, codegen_file_type_t::llvm_ir_text, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(emit_machine_code_to_file(state.module.get(), nullptr, codegen_file_type_t::object_file, "/tmp/edg_cg_test.o") == llvm_gen_be_error_t::invalid_argument);
  assert(emit_machine_code_to_file(state.module.get(), tm, static_cast<codegen_file_type_t>(99), "/tmp/edg_cg_test.o") == llvm_gen_be_error_t::invalid_argument);

  assert(emit_machine_code_to_memory_buffer(nullptr, tm, codegen_file_type_t::object_file, &buffer) == llvm_gen_be_error_t::invalid_argument);
  assert(emit_machine_code_to_memory_buffer(state.module.get(), nullptr, codegen_file_type_t::object_file, &buffer) == llvm_gen_be_error_t::invalid_argument);
  assert(emit_machine_code_to_memory_buffer(state.module.get(), tm, codegen_file_type_t::object_file, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(emit_machine_code_to_memory_buffer(state.module.get(), tm, codegen_file_type_t::llvm_ir_text, &buffer) == llvm_gen_be_error_t::invalid_argument);

  assert(emit_thinlto_summary(nullptr, "/tmp/edg_cg_test.thinlto.bc") == llvm_gen_be_error_t::invalid_argument);
  assert(emit_thinlto_summary(state.module.get(), nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(enable_split_dwarf_emission(nullptr, "test.dwo") == llvm_gen_be_error_t::invalid_argument);
  assert(enable_split_dwarf_emission(tm, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(release_target_machine_cache() == llvm_gen_be_error_t::ok);

  std::remove("/tmp/edg_cg_test.ll");
  std::remove("/tmp/edg_cg_test.bc");
  std::remove("/tmp/edg_cg_test.s");
  std::remove("/tmp/edg_cg_test.o");
  std::remove("/tmp/edg_cg_test.thinlto.bc");

  printf("PASS\n");
  return 0;
}
