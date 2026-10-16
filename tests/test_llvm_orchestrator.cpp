/**
 * @file test_llvm_orchestrator.cpp
 * @brief Unit tests for the LLVM backend state and lifecycle.
 * @details Validates LLVMBackendState cleanup, state verification, and re-entrant safety.
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_codegen.h"
#include "llvm_gen_be_opt.h"
#include "llvm_gen_be_driver.h"
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/BasicBlock.h>

using namespace edg;

// Provide stubs for external functions called by llvm_gen_be.cpp
namespace edg {

const char* primary_source_file_name = "test.cpp";
const char* gen_llvm_file_name = nullptr;
const char* gen_llvm_bc_file_name = nullptr;
const char* gen_asm_file_name = nullptr;
const char* gen_obj_file_name = nullptr;
const char* ofile_name = nullptr;

a_boolean sanitize_address = FALSE; a_boolean sanitize_thread = FALSE; a_boolean sanitize_memory = FALSE; a_boolean sanitize_undefined = FALSE; a_boolean sanitize_hwaddress = FALSE; int stack_protector_mode = 0; a_boolean use_safestack = FALSE; a_boolean use_shadowcallstack = FALSE;
a_const_char* flto_kind = nullptr;
a_const_char* fprofile_generate_path = nullptr;
a_const_char* fprofile_use_path = nullptr;

llvm_gen_be_error_t run_optimization_pipeline(llvm::Module* module, const llvm_opt_options_t* opts) noexcept {
  return llvm_gen_be_error_t::ok;
}
a_boolean skip_il_read = TRUE;

llvm_gen_be_error_t llvm_gen_be_set_error(
    llvm_gen_be_error_context_t* ctx,
    llvm_gen_be_error_t err_code,
    const char* file,
    unsigned line,
    unsigned column,
    const char* format, ...) noexcept {
  if (ctx) ctx->error_code = err_code;
  return err_code;
}

llvm_gen_be_error_t debug_info_init(llvm_gen_be_debug_state_t** dbg_state, llvm::Module* mod, const char* main_file, const char* producer, bool is_optimized) noexcept {
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t debug_info_finalize(llvm_gen_be_debug_state_t* dbg_state) noexcept {
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t debug_info_cleanup(llvm_gen_be_debug_state_t** dbg_state) noexcept {
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_global_variables(void) noexcept { return llvm_gen_be_error_t::ok; }
llvm_gen_be_error_t llvm_lower_function_declarations(void) noexcept { return llvm_gen_be_error_t::ok; }
llvm_gen_be_error_t llvm_lower_function_definitions(void) noexcept { return llvm_gen_be_error_t::ok; }
llvm_gen_be_error_t llvm_lower_global_ctors_and_dtors(void) noexcept { return llvm_gen_be_error_t::ok; }

// Stubs for target machine
llvm_gen_be_error_t initialize_llvm_targets() noexcept {
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  return llvm_gen_be_error_t::ok;
}
llvm_gen_be_error_t create_target_machine(const char* triple, const char* cpu, const char* features, llvm::CodeGenOptLevel opt_level, std::optional<llvm::Reloc::Model> reloc_model, std::optional<llvm::CodeModel::Model> code_model, llvm::TargetMachine** out_tm) noexcept {
  if (!out_tm) return llvm_gen_be_error_t::invalid_argument;
  std::string err;
  llvm::Triple theTriple(triple);
  const llvm::Target* target = llvm::TargetRegistry::lookupTarget(theTriple, err);
  if (!target) return llvm_gen_be_error_t::code_gen_failure;
  llvm::TargetOptions opt;
  *out_tm = target->createTargetMachine(theTriple, cpu ? cpu : "generic", features ? features : "", opt, std::nullopt);
  return llvm_gen_be_error_t::ok;
}
llvm_gen_be_error_t emit_machine_code_to_file(llvm::Module* module, llvm::TargetMachine* tm, codegen_file_type_t file_type, const char* out_path) noexcept { return llvm_gen_be_error_t::ok; }
char* derived_name(a_const_char* base, a_const_char* ext) { return (char*)"test.ll"; }

unsigned int targ_char_bit = 8;
a_targ_size_t targ_sizeof_short = 2;
a_targ_size_t targ_sizeof_int = 4;
a_targ_size_t targ_sizeof_long = 8;
a_targ_size_t targ_sizeof_long_long = 8;
a_targ_size_t targ_sizeof_pointer = 8;
a_targ_size_t targ_sizeof_float = 4;
a_targ_size_t targ_sizeof_double = 8;
a_targ_size_t targ_sizeof_long_double = 16;
a_boolean targ_little_endian = TRUE;

} // namespace edg

void test_verify_state() {
  be_state = new (std::nothrow) LLVMBackendState();
  be_state->context = std::make_unique<llvm::LLVMContext>();
  be_state->module = std::make_unique<llvm::Module>("test", *be_state->context);

  // Test with no functions
  assert(llvm_gen_be_verify_state() == llvm_gen_be_error_t::ok);

  // Add a function with a terminated block
  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "ok_func", be_state->module.get());
  llvm::BasicBlock* bb1 = llvm::BasicBlock::Create(*be_state->context, "entry", f);
  llvm::ReturnInst::Create(*be_state->context, nullptr, bb1);
  assert(llvm_gen_be_verify_state() == llvm_gen_be_error_t::ok);

  // Add a function with an unterminated block
  llvm::Function* f2 = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "bad_func", be_state->module.get());
  llvm::BasicBlock::Create(*be_state->context, "entry2", f2);
  assert(llvm_gen_be_verify_state() == llvm_gen_be_error_t::internal_inconsistency);

  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  assert(be_state == nullptr);
}

void test_lifecycle() {
  // Simulate compilation 1
  assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
  assert(be_state != nullptr);
  assert(be_state->context != nullptr);
  
  // Clean up
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  assert(be_state == nullptr);

  // Simulate compilation 2 (re-entrant)
  assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
  
  // Test linking
  be_state->module->setTargetTriple(llvm::Triple("aarch64-unknown-linux-gnu"));
  assert(llvm_link_executable(nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_link_executable("dummy.o", "dummy.exe") == llvm_gen_be_error_t::code_gen_failure); // Will fail because file doesn't exist

  // Clean up
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  assert(be_state == nullptr);
}

int main() {
  printf("Running test_llvm_orchestrator...\n");
  test_verify_state();
  test_lifecycle();
  printf("PASS\n");
  return 0;
}
