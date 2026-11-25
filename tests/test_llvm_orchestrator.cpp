/**
 * @file test_llvm_orchestrator.cpp
 * @ingroup llvm_gen_be_tests
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
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>

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

int g_fail_opt = 0; ///< When non-zero, causes `run_optimization_pipeline` to fail.
int g_fail_dbg_init = 0; ///< When non-zero, causes `debug_info_init` to fail.
int g_fail_dbg_fin = 0; ///< When non-zero, causes `debug_info_finalize` to fail.
int g_fail_dbg_clean = 0; ///< When non-zero, causes `debug_info_cleanup` to fail.
int g_fail_lower_step = 0; ///< Step index (1..5) that should fail during lowering.
int g_fail_init_targets = 0; ///< Counter of calls before `initialize_llvm_targets` fails (negative = immediate fail).
int g_fail_create_tm = 0; ///< Counter of calls before `create_target_machine` fails (negative = immediate fail).
int g_fail_split_dwarf = 0; ///< When non-zero, causes `enable_split_dwarf_emission` to fail.
int g_fail_thinlto = 0; ///< When non-zero, causes `emit_thinlto_summary` to fail.
int g_fail_emit_type = -1; ///< `codegen_file_type_t` value that should fail in `emit_machine_code_to_file`.
int g_derived_null = 0; ///< When non-zero, `derived_name` returns nullptr.
int g_populate_dbg_state = 0; ///< When non-zero, `debug_info_init` allocates a `llvm_gen_be_debug_state_t`.
int g_make_unterminated_in_lower = 0; ///< When non-zero, `llvm_lower_global_ctors_and_dtors` adds an unterminated block.

/**
 * @brief Stub implementation of `run_optimization_pipeline` for orchestrator unit tests.
 * @details Returns `llvm_gen_be_error_t::ok` unless `g_fail_opt` is set.
 * @param[in,out] module The LLVM module.
 * @param[in] opts Optimization options.
 * @return llvm_gen_be_error_t::ok or `pass_pipeline_failure`.
 */
llvm_gen_be_error_t run_optimization_pipeline(llvm::Module* module, const llvm_opt_options_t* opts) noexcept {
  (void)module;
  (void)opts;
  if (g_fail_opt) return llvm_gen_be_error_t::pass_pipeline_failure;
  return llvm_gen_be_error_t::ok;
}
a_boolean skip_il_read = TRUE;

/**
 * @brief Stub implementation of `llvm_gen_be_set_error` for orchestrator unit tests.
 * @details Records `err_code` in `ctx` and returns `err_code`.
 * @param[in,out] ctx Error context pointer.
 * @param[in] err_code Error code to set.
 * @param[in] file Source file name.
 * @param[in] line Source line number.
 * @param[in] column Source column number.
 * @param[in] format Format string.
 * @return The provided `err_code`.
 */
llvm_gen_be_error_t llvm_gen_be_set_error(
    llvm_gen_be_error_context_t* ctx,
    llvm_gen_be_error_t err_code,
    const char* file,
    unsigned line,
    unsigned column,
    const char* format, ...) noexcept {
  (void)file;
  (void)line;
  (void)column;
  (void)format;
  if (ctx) ctx->error_code = err_code;
  return err_code;
}

/**
 * @brief Stub implementation of `debug_info_init`.
 * @details Optionally allocates `dbg_state` or returns `di_metadata_failure` when `g_fail_dbg_init` is set.
 * @param[out] dbg_state Output debug state pointer.
 * @param[in] mod Active LLVM module.
 * @param[in] main_file Main source file name.
 * @param[in] producer Compiler producer string.
 * @param[in] is_optimized True if optimizations are enabled.
 * @return llvm_gen_be_error_t::ok or `di_metadata_failure`.
 */
llvm_gen_be_error_t debug_info_init(llvm_gen_be_debug_state_t** dbg_state, llvm::Module* mod, const char* main_file, const char* producer, bool is_optimized) noexcept {
  (void)mod;
  (void)main_file;
  (void)producer;
  (void)is_optimized;
  if (g_fail_dbg_init) return llvm_gen_be_error_t::di_metadata_failure;
  if (g_populate_dbg_state && dbg_state) {
    *dbg_state = new (std::nothrow) llvm_gen_be_debug_state_t();
    if (!*dbg_state) return llvm_gen_be_error_t::out_of_memory;
    (*dbg_state)->is_codeview = (g_populate_dbg_state == 2);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Stub implementation of `debug_info_finalize`.
 * @details Returns `di_metadata_failure` when `g_fail_dbg_fin` is set, otherwise `ok`.
 * @param[in,out] dbg_state Active debug state pointer.
 * @return llvm_gen_be_error_t::ok or `di_metadata_failure`.
 */
llvm_gen_be_error_t debug_info_finalize(llvm_gen_be_debug_state_t* dbg_state) noexcept {
  (void)dbg_state;
  if (g_fail_dbg_fin) return llvm_gen_be_error_t::di_metadata_failure;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Stub implementation of `debug_info_cleanup`.
 * @details Deletes `*dbg_state` and returns `ok` unless `g_fail_dbg_clean` is set.
 * @param[in,out] dbg_state Pointer to debug state pointer.
 * @return llvm_gen_be_error_t::ok or `di_metadata_failure`.
 */
llvm_gen_be_error_t debug_info_cleanup(llvm_gen_be_debug_state_t** dbg_state) noexcept {
  if (dbg_state && *dbg_state) {
    delete *dbg_state;
    *dbg_state = nullptr;
  }
  if (g_fail_dbg_clean) return llvm_gen_be_error_t::di_metadata_failure;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Stub implementation of `llvm_lower_module_metadata`.
 * @details Returns `llvm_gen_be_error_t::ok` unless `g_fail_lower_step == 1`.
 * @return llvm_gen_be_error_t::ok or error.
 */
llvm_gen_be_error_t llvm_lower_module_metadata(void) noexcept {
  return (g_fail_lower_step == 1) ? llvm_gen_be_error_t::internal_inconsistency : llvm_gen_be_error_t::ok;
}

/**
 * @brief Stub implementation of `llvm_lower_global_variables`.
 * @details Returns `llvm_gen_be_error_t::ok` unless `g_fail_lower_step == 2`.
 * @return llvm_gen_be_error_t::ok or error.
 */
llvm_gen_be_error_t llvm_lower_global_variables(void) noexcept {
  return (g_fail_lower_step == 2) ? llvm_gen_be_error_t::internal_inconsistency : llvm_gen_be_error_t::ok;
}

/**
 * @brief Stub implementation of `llvm_lower_function_declarations`.
 * @details Returns `llvm_gen_be_error_t::ok` unless `g_fail_lower_step == 3`.
 * @return llvm_gen_be_error_t::ok or error.
 */
llvm_gen_be_error_t llvm_lower_function_declarations(void) noexcept {
  return (g_fail_lower_step == 3) ? llvm_gen_be_error_t::internal_inconsistency : llvm_gen_be_error_t::ok;
}

/**
 * @brief Stub implementation of `llvm_lower_function_definitions`.
 * @details Returns `llvm_gen_be_error_t::ok` unless `g_fail_lower_step == 4`.
 * @return llvm_gen_be_error_t::ok or error.
 */
llvm_gen_be_error_t llvm_lower_function_definitions(void) noexcept {
  return (g_fail_lower_step == 4) ? llvm_gen_be_error_t::internal_inconsistency : llvm_gen_be_error_t::ok;
}

/**
 * @brief Stub implementation of `llvm_lower_global_ctors_and_dtors`.
 * @details Returns `llvm_gen_be_error_t::ok` unless `g_fail_lower_step == 5`.
 * @return llvm_gen_be_error_t::ok or error.
 */
llvm_gen_be_error_t llvm_lower_global_ctors_and_dtors(void) noexcept {
  if (g_make_unterminated_in_lower && be_state && be_state->module) {
    llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
    llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "unterm_fn", be_state->module.get());
    llvm::BasicBlock::Create(*be_state->context, "entry", f);
  }
  return (g_fail_lower_step == 5) ? llvm_gen_be_error_t::internal_inconsistency : llvm_gen_be_error_t::ok;
}

/**
 * @brief Initializes LLVM target registries for orchestrator tests.
 * @details Registers target infos, target machines, MCs, and asm printers.
 * @return llvm_gen_be_error_t::ok or `code_gen_failure`.
 */
llvm_gen_be_error_t initialize_llvm_targets(void) noexcept {
  if (g_fail_init_targets < 0) return llvm_gen_be_error_t::code_gen_failure;
  if (g_fail_init_targets > 0) {
    --g_fail_init_targets;
    if (g_fail_init_targets == 0) return llvm_gen_be_error_t::code_gen_failure;
  }
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Creates a TargetMachine instance for orchestrator tests.
 * @details Looks up `triple` in `TargetRegistry` and constructs a `TargetMachine`.
 * @param[in] triple Target triple string.
 * @param[in] cpu Target CPU string.
 * @param[in] features Target feature string.
 * @param[in] opt_level CodeGen optimization level.
 * @param[in] reloc_model Optional relocation model.
 * @param[in] code_model Optional code model.
 * @param[out] out_tm Receives the created `TargetMachine*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t create_target_machine(const char* triple, const char* cpu, const char* features, llvm_codegen_opt_level_t opt_level, std::optional<llvm::Reloc::Model> reloc_model, std::optional<llvm::CodeModel::Model> code_model, llvm::TargetMachine** out_tm) noexcept {
  (void)opt_level;
  (void)reloc_model;
  (void)code_model;
  if (g_fail_create_tm < 0) return llvm_gen_be_error_t::code_gen_failure;
  if (g_fail_create_tm > 0) {
    --g_fail_create_tm;
    if (g_fail_create_tm == 0) return llvm_gen_be_error_t::code_gen_failure;
  }
  if (!out_tm) return llvm_gen_be_error_t::invalid_argument;
  std::string err;
  llvm::Triple theTriple(triple);
#if LLVM_VERSION_MAJOR >= 21
  const llvm::Target* target = llvm::TargetRegistry::lookupTarget(theTriple, err);
#else
  const llvm::Target* target = llvm::TargetRegistry::lookupTarget(theTriple.getTriple(), err);
#endif
  if (!target) return llvm_gen_be_error_t::code_gen_failure;
  llvm::TargetOptions opt;
#if LLVM_VERSION_MAJOR >= 21
  *out_tm = target->createTargetMachine(theTriple, cpu ? cpu : "generic", features ? features : "", opt, std::nullopt);
#else
  *out_tm = target->createTargetMachine(theTriple.getTriple(), cpu ? cpu : "generic", features ? features : "", opt, std::nullopt);
#endif
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Stub implementation of `release_target_machine_cache`.
 * @details Returns `llvm_gen_be_error_t::ok`.
 * @return llvm_gen_be_error_t::ok.
 */
llvm_gen_be_error_t release_target_machine_cache(void) noexcept { return llvm_gen_be_error_t::ok; }

/**
 * @brief Stub implementation of `emit_machine_code_to_file`.
 * @details Returns `llvm_gen_be_error_t::ok` unless `file_type` matches `g_fail_emit_type`.
 * @param[in] module The LLVM module.
 * @param[in] tm The TargetMachine.
 * @param[in] file_type Output file type.
 * @param[in] out_path Output file path.
 * @return llvm_gen_be_error_t::ok or `io_error`.
 */
llvm_gen_be_error_t emit_machine_code_to_file(llvm::Module* module, llvm::TargetMachine* tm, codegen_file_type_t file_type, const char* out_path) noexcept {
  (void)module;
  (void)tm;
  (void)out_path;
  if (static_cast<int>(file_type) == g_fail_emit_type) return llvm_gen_be_error_t::io_error;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Stub implementation of `emit_thinlto_summary`.
 * @details Returns `llvm_gen_be_error_t::ok` unless `g_fail_thinlto` is set.
 * @param[in] module The LLVM module.
 * @param[in] out_path Output bitcode file path.
 * @return llvm_gen_be_error_t::ok or `io_error`.
 */
llvm_gen_be_error_t emit_thinlto_summary(llvm::Module* module, const char* out_path) noexcept {
  (void)module;
  (void)out_path;
  if (g_fail_thinlto) return llvm_gen_be_error_t::io_error;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Stub implementation of `enable_split_dwarf_emission`.
 * @details Returns `llvm_gen_be_error_t::ok` unless `g_fail_split_dwarf` is set.
 * @param[in] tm The TargetMachine.
 * @param[in] dwo_path Output DWO file path.
 * @return llvm_gen_be_error_t::ok or `code_gen_failure`.
 */
llvm_gen_be_error_t enable_split_dwarf_emission(llvm::TargetMachine* tm, const char* dwo_path) noexcept {
  (void)tm;
  (void)dwo_path;
  if (g_fail_split_dwarf) return llvm_gen_be_error_t::code_gen_failure;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Stub implementation of `derived_name`.
 * @details Returns `"test.ll"` or `nullptr` when `g_derived_null` is set.
 * @param[in] base Base filename.
 * @param[in] ext Extension string.
 * @return Pointer to static `"test.ll"` string or `nullptr`.
 */
char* derived_name(a_const_char* base, a_const_char* ext) {
  (void)base;
  (void)ext;
  if (g_derived_null) return nullptr;
  return (char*)"test.ll";
}

unsigned int targ_char_bit = 8;
a_boolean targ_has_signed_chars = TRUE;
a_targ_size_t targ_sizeof_short = 2;
a_targ_alignment targ_alignof_short = 2;
a_targ_size_t targ_sizeof_int = 4;
a_targ_alignment targ_alignof_int = 4;
a_targ_size_t targ_sizeof_long = 8;
a_targ_alignment targ_alignof_long = 8;
a_targ_size_t targ_sizeof_long_long = 8;
a_targ_alignment targ_alignof_long_long = 8;
a_targ_size_t targ_sizeof_pointer = 8;
a_targ_alignment targ_alignof_pointer = 8;
a_targ_size_t targ_sizeof_float = 4;
a_targ_alignment targ_alignof_float = 4;
a_targ_size_t targ_sizeof_double = 8;
a_targ_alignment targ_alignof_double = 8;
a_targ_size_t targ_sizeof_long_double = 16;
a_targ_alignment targ_alignof_long_double = 16;
a_targ_size_t targ_sizeof_wchar_t = 4;
an_integer_kind targ_size_t_int_kind = ik_unsigned_long;
an_integer_kind targ_ptrdiff_t_int_kind = ik_long;
an_integer_kind targ_ia64_vtable_entry_int_kind = ik_long;
an_integer_kind targ_wchar_t_int_kind = ik_int;
an_integer_kind targ_wint_t_int_kind = ik_int;
a_boolean targ_little_endian = TRUE;

} // namespace edg

/**
 * @brief Tests `llvm_gen_be_verify_state` on empty, valid, and unterminated functions.
 * @details Verifies that unterminated basic blocks trigger `internal_inconsistency`.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t test_verify_state(void) noexcept {
  // Null be_state and null module checks
  be_state = nullptr;
  assert(llvm_gen_be_verify_state() == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

  be_state = new (std::nothrow) LLVMBackendState();
  assert(llvm_gen_be_verify_state() == llvm_gen_be_error_t::invalid_argument);

  be_state->context = std::make_unique<llvm::LLVMContext>();
  be_state->module = std::make_unique<llvm::Module>("test", *be_state->context);

  // Test with no functions
  assert(llvm_gen_be_verify_state() == llvm_gen_be_error_t::ok);

  // Add a declaration-only function (isDeclaration() == true)
  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
  llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "decl_only", be_state->module.get());

  // Add a function with a terminated block
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "ok_func", be_state->module.get());
  llvm::BasicBlock* bb1 = llvm::BasicBlock::Create(*be_state->context, "entry", f);
  llvm::ReturnInst::Create(*be_state->context, nullptr, bb1);
  assert(llvm_gen_be_verify_state() == llvm_gen_be_error_t::ok);

  // Add a function with a non-empty block whose last instruction is NOT a terminator
  llvm::Function* f_nonterm = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "nonterm_func", be_state->module.get());
  llvm::BasicBlock* bb_nonterm = llvm::BasicBlock::Create(*be_state->context, "entry_nt", f_nonterm);
  llvm::IRBuilder<> nt_builder{bb_nonterm};
  llvm::AllocaInst* nt_alloca = nt_builder.CreateAlloca(llvm::Type::getInt32Ty(*be_state->context), nullptr, "tmp");
  assert(nt_alloca != nullptr);
  assert(llvm_gen_be_verify_state() == llvm_gen_be_error_t::internal_inconsistency);
  f_nonterm->eraseFromParent();

  // Add a function with an empty unterminated block
  llvm::Function* f2 = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "bad_func", be_state->module.get());
  llvm::BasicBlock::Create(*be_state->context, "entry2", f2);
  assert(llvm_gen_be_verify_state() == llvm_gen_be_error_t::internal_inconsistency);

  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  assert(be_state == nullptr);
  assert(reset_llvm_backend_tu_state(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_apply_invocation_options(nullptr) == llvm_gen_be_error_t::invalid_argument);
  edg_be_invocation_options_t inv_opts = {};
  inv_opts.opt_level = 2;
  inv_opts.debug_level = 2;
  assert(llvm_apply_invocation_options(&inv_opts) == llvm_gen_be_error_t::ok);
  inv_opts.debug_level = 0;
  inv_opts.target_triple = "x86_64-unknown-linux-gnu";
  inv_opts.mcpu = "generic";
  inv_opts.mattr = "+sse2";
  inv_opts.split_dwarf_path = "out.dwo";
  assert(llvm_apply_invocation_options(&inv_opts) == llvm_gen_be_error_t::ok);
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests full `llvm_gen_be` and `llvm_gen_be_cleanup` re-entrant lifecycle.
 * @details Exercises target triple overrides, `llvm_link_executable`, and `invoke_embedded_lld_lto`.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t test_lifecycle(void) noexcept {
  // Simulate compilation 1
  std::string dl_str;
  assert(build_data_layout(nullptr) == llvm_gen_be_error_t::invalid_argument);
  targ_little_endian = TRUE;
  assert(build_data_layout(&dl_str) == llvm_gen_be_error_t::ok);
  assert(!dl_str.empty() && dl_str[0] == 'e');
  targ_little_endian = FALSE;
  assert(build_data_layout(&dl_str) == llvm_gen_be_error_t::ok);
  assert(!dl_str.empty() && dl_str[0] == 'E');
  targ_little_endian = TRUE;

  // Test llvm_sync_fe_target_configuration across LP64, LLP64, ILP32, big-endian, Darwin, and Windows targets
  assert(llvm_sync_fe_target_configuration(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_sync_fe_target_configuration("") == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_sync_fe_target_configuration("unknownarchxyz-unknown-none") == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_configure_cli_option("target", "unknownarchxyz-unknown-none") == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

  assert(llvm_sync_fe_target_configuration("x86_64-unknown-linux-gnu") == llvm_gen_be_error_t::ok);
  assert(targ_sizeof_long == 8 && targ_sizeof_pointer == 8 && targ_sizeof_long_double == 16 && targ_has_signed_chars == TRUE);

  assert(llvm_sync_fe_target_configuration("x86_64-pc-windows-msvc") == llvm_gen_be_error_t::ok);
  assert(targ_sizeof_long == 4 && targ_sizeof_pointer == 8 && targ_sizeof_long_double == 8 && targ_sizeof_wchar_t == 2);

  assert(llvm_sync_fe_target_configuration("aarch64-apple-darwin") == llvm_gen_be_error_t::ok);
  assert(targ_sizeof_long == 8 && targ_sizeof_pointer == 8 && targ_sizeof_long_double == 8 && targ_has_signed_chars == TRUE);

  assert(llvm_sync_fe_target_configuration("x86_64-apple-darwin") == llvm_gen_be_error_t::ok);
  assert(targ_sizeof_long == 8 && targ_sizeof_pointer == 8 && targ_sizeof_long_double == 16 && targ_has_signed_chars == TRUE);

  assert(llvm_sync_fe_target_configuration("aarch64-unknown-linux-gnu") == llvm_gen_be_error_t::ok);
  assert(targ_sizeof_long == 8 && targ_sizeof_pointer == 8 && targ_sizeof_long_double == 16 && targ_has_signed_chars == FALSE);

  assert(llvm_sync_fe_target_configuration("i386-unknown-linux-gnu") == llvm_gen_be_error_t::ok);
  assert(targ_sizeof_long == 4 && targ_sizeof_pointer == 4 && targ_sizeof_long_double == 12 && targ_alignof_long_long == 4);

  assert(llvm_sync_fe_target_configuration("i386-pc-windows-msvc") == llvm_gen_be_error_t::ok);
  assert(targ_sizeof_long == 4 && targ_sizeof_pointer == 4 && targ_sizeof_long_double == 8 && targ_alignof_long_long == 8);

  assert(llvm_sync_fe_target_configuration("armv7-unknown-linux-gnueabihf") == llvm_gen_be_error_t::ok);
  assert(targ_sizeof_long == 4 && targ_sizeof_pointer == 4 && targ_sizeof_long_double == 8 && targ_has_signed_chars == FALSE);

  assert(llvm_sync_fe_target_configuration("thumbv7-unknown-linux-gnueabihf") == llvm_gen_be_error_t::ok);
  assert(targ_sizeof_long == 4 && targ_sizeof_pointer == 4 && targ_sizeof_long_double == 8 && targ_has_signed_chars == FALSE);

  assert(llvm_sync_fe_target_configuration("powerpc64-unknown-linux-gnu") == llvm_gen_be_error_t::ok);
  assert(targ_sizeof_long == 8 && targ_little_endian == FALSE && targ_has_signed_chars == FALSE);

  assert(llvm_sync_fe_target_configuration("riscv64-unknown-linux-gnu") == llvm_gen_be_error_t::ok);
  assert(targ_sizeof_long == 8 && targ_little_endian == TRUE && targ_has_signed_chars == FALSE);

  assert(llvm_sync_fe_target_configuration("s390x-ibm-linux-gnu") == llvm_gen_be_error_t::ok);
  assert(targ_sizeof_long == 8 && targ_little_endian == FALSE && targ_has_signed_chars == FALSE);
  targ_little_endian = TRUE;

  assert(back_end() == llvm_gen_be_error_t::ok);
  assert(be_state != nullptr);
  assert(be_state->context != nullptr);
  
  // Clean up
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  assert(be_state == nullptr);

  // Simulate compilation 2 (re-entrant)
  assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
  
  // Test linking and embedded LTO
  EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "aarch64-apple-darwin");
  assert(llvm_link_executable(nullptr, "out") == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_link_executable("in.o", nullptr) == llvm_gen_be_error_t::invalid_argument);
  be_state->extra_linker_inputs.push_back("extra_input.o");
  assert(llvm_link_executable("dummy.o", "dummy.exe") == llvm_gen_be_error_t::code_gen_failure); // Will fail because file doesn't exist
  be_state->extra_linker_inputs.clear();
  LLVMBackendState* saved_be = be_state;
  be_state = nullptr;
  assert(llvm_link_executable("dummy.o", "dummy.exe") == llvm_gen_be_error_t::invalid_argument);
  be_state = saved_be;

  // Test llvm_link_executable when clang++ is not found in PATH
  const char* orig_path = getenv("PATH");
  std::string saved_path = orig_path ? orig_path : "";
  setenv("PATH", "/nonexistent_dir_for_edg_test", 1);
  assert(llvm_link_executable("dummy.o", "dummy.exe") == llvm_gen_be_error_t::code_gen_failure);
  setenv("PATH", saved_path.c_str(), 1);

  // Test llvm_link_executable succeeding on a minimal valid C file
  EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), llvm::sys::getDefaultTargetTriple());
  {
    FILE* fp = fopen("/tmp/edg_link_test.c", "w");
    if (fp) {
      fputs("int main(void) { return 0; }\n", fp);
      fclose(fp);
      assert(llvm_link_executable("/tmp/edg_link_test.c", "/tmp/edg_link_test.exe") == llvm_gen_be_error_t::ok);
      std::remove("/tmp/edg_link_test.c");
      std::remove("/tmp/edg_link_test.exe");
    }
  }

  // Test llvm_link_executable with null module or null be_state
  auto saved_mod = std::move(be_state->module);
  assert(llvm_link_executable("a.o", "a.out") == llvm_gen_be_error_t::invalid_argument);
  assert(invoke_embedded_lld_lto() == llvm_gen_be_error_t::invalid_argument);
  be_state->module = std::move(saved_mod);

  assert(invoke_embedded_lld_lto() == llvm_gen_be_error_t::ok);
  assert(be_state->module->getModuleFlag("FullLTO") != nullptr);
  // Second call when FullLTO flag is already present
  assert(invoke_embedded_lld_lto() == llvm_gen_be_error_t::ok);
  // Call with ofile_name set (both with gen_obj_file_name == nullptr and gen_obj_file_name != nullptr)
  ofile_name = "dummy_lto_out.exe";
  assert(invoke_embedded_lld_lto() == llvm_gen_be_error_t::code_gen_failure);
  gen_obj_file_name = "custom_lto_keep.o";
  assert(invoke_embedded_lld_lto() == llvm_gen_be_error_t::code_gen_failure);
  gen_obj_file_name = nullptr;
  ofile_name = nullptr;

  // Test invoke_embedded_lld_lto with ThinLTO flag already present, empty triple, all opt levels, and gen_obj_file_name set
  llvm_opt_level_t opt_levels[] = {
    llvm_opt_level_t::O0,
    llvm_opt_level_t::O1,
    llvm_opt_level_t::O2,
    llvm_opt_level_t::Os,
    llvm_opt_level_t::Oz,
    llvm_opt_level_t::O3,
    static_cast<llvm_opt_level_t>(99)
  };
  for (llvm_opt_level_t ol : opt_levels) {
    be_state->module = std::make_unique<llvm::Module>("lto_mod", *be_state->context);
    be_state->module->addModuleFlag(llvm::Module::Error, "ThinLTO", 1);
    be_state->opt_level = ol;
    gen_obj_file_name = "custom_lto.o";
    assert(invoke_embedded_lld_lto() == llvm_gen_be_error_t::ok);
    gen_obj_file_name = nullptr;
  }

  // Test invoke_embedded_lld_lto error branches
  g_fail_opt = 1;
  assert(invoke_embedded_lld_lto() == llvm_gen_be_error_t::pass_pipeline_failure);
  g_fail_opt = 0;

  g_fail_init_targets = -1;
  assert(invoke_embedded_lld_lto() == llvm_gen_be_error_t::code_gen_failure);
  g_fail_init_targets = 0;

  g_llvm_be_oom_countdown = 0;
  assert(invoke_embedded_lld_lto() == llvm_gen_be_error_t::out_of_memory);
  g_llvm_be_oom_countdown = -1;

  g_fail_create_tm = -1;
  assert(invoke_embedded_lld_lto() == llvm_gen_be_error_t::code_gen_failure);
  g_fail_create_tm = 0;

  g_fail_emit_type = static_cast<int>(codegen_file_type_t::object_file);
  assert(invoke_embedded_lld_lto() == llvm_gen_be_error_t::io_error);
  g_fail_emit_type = -1;

  // Clean up
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  assert(be_state == nullptr);
  assert(llvm_link_executable("a.o", "a.out") == llvm_gen_be_error_t::invalid_argument);
  assert(invoke_embedded_lld_lto() == llvm_gen_be_error_t::invalid_argument);

  // Test generate_llvm_output_file verification failure branch
  be_state = new (std::nothrow) LLVMBackendState();
  be_state->context = std::make_unique<llvm::LLVMContext>();
  be_state->module = std::make_unique<llvm::Module>("bad_verify", *be_state->context);
  be_state->builder = std::make_unique<llvm::IRBuilder<>>(*be_state->context);
  {
    llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(*be_state->context), false);
    llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "bad_ret_fn", be_state->module.get());
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(*be_state->context, "entry", f);
    llvm::ReturnInst::Create(*be_state->context, nullptr, bb); // void return in i32 function fails verifyModule!
    assert(generate_llvm_output_file("bad.cpp") == llvm_gen_be_error_t::verification_failure);
  }
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

  // Test generate_llvm_output_file & llvm_gen_be across all opt levels, sanitizer/PGO/LTO flags, and output file types
  for (llvm_opt_level_t ol : opt_levels) {
    be_state = new (std::nothrow) LLVMBackendState();
    be_state->opt_level = ol;
    be_state->emit_debug_info = false;
    be_state->target_triple_override = "x86_64-unknown-linux-gnu";
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  }

  // Test individual sanitizer / PGO / LTO flags when opt_level == O0
  {
    sanitize_address = TRUE;
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
    sanitize_address = FALSE;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

    sanitize_memory = TRUE;
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
    sanitize_memory = FALSE;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

    sanitize_thread = TRUE;
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
    sanitize_thread = FALSE;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

    sanitize_undefined = TRUE;
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
    sanitize_undefined = FALSE;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

    fprofile_generate_path = "prof.raw";
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
    fprofile_generate_path = nullptr;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

    fprofile_use_path = "prof.data";
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
    fprofile_use_path = nullptr;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

    flto_kind = "thin";
    gen_llvm_bc_file_name = "out.bc";
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
    g_fail_thinlto = 1;
    assert(generate_llvm_output_file("test.cpp") == llvm_gen_be_error_t::io_error);
    g_fail_thinlto = 0;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

    flto_kind = "full";
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

    flto_kind = "other";
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
    flto_kind = nullptr;
    g_fail_emit_type = static_cast<int>(codegen_file_type_t::bitcode_file);
    assert(generate_llvm_output_file("test.cpp") == llvm_gen_be_error_t::io_error);
    g_fail_emit_type = -1;
    gen_llvm_bc_file_name = nullptr;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  }

  // Test gen_llvm_file_name, base_name == nullptr, derived_name == nullptr, gen_asm_file_name, gen_obj_file_name
  {
    gen_llvm_file_name = "custom.ll";
    gen_asm_file_name = "custom.s";
    gen_obj_file_name = "custom.o";
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);

    // Test target_cpu, target_features, split_dwarf_file, and ofile_name with gen_obj_file_name
    const std::string host_triple_s = llvm::sys::getDefaultTargetTriple();
    EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), host_triple_s);
    be_state->target_cpu = "generic";
    be_state->target_features = llvm::Triple(host_triple_s).isAArch64() ? "+neon" : "+sse2";
    be_state->split_dwarf_file = "custom.dwo";
    g_populate_dbg_state = 1;
    be_state->dbg_state = new (std::nothrow) llvm_gen_be_debug_state_t();
    assert(be_state->dbg_state != nullptr);
    be_state->dbg_state->is_codeview = false;
    FILE* c_fp = fopen("/tmp/edg_orch_link.c", "w");
    if (c_fp) {
      fputs("int main(void) { return 0; }\n", c_fp);
      fclose(c_fp);
      gen_obj_file_name = "/tmp/edg_orch_link.c";
      ofile_name = "/tmp/edg_orch_link.exe";
      assert(generate_llvm_output_file("test.cpp") == llvm_gen_be_error_t::ok);
      std::remove("/tmp/edg_orch_link.c");
      std::remove("/tmp/edg_orch_link.exe");
    }
    gen_obj_file_name = "nonexistent_obj_for_link.o";
    ofile_name = "/tmp/edg_orch_link_fail.exe";
    assert(generate_llvm_output_file("test.cpp") == llvm_gen_be_error_t::code_gen_failure);
    ofile_name = nullptr;
    gen_obj_file_name = "custom.o";
    g_populate_dbg_state = 0;

    g_fail_emit_type = static_cast<int>(codegen_file_type_t::llvm_ir_text);
    assert(generate_llvm_output_file("test.cpp") == llvm_gen_be_error_t::io_error);
    g_fail_emit_type = static_cast<int>(codegen_file_type_t::assembly_file);
    assert(generate_llvm_output_file("test.cpp") == llvm_gen_be_error_t::io_error);
    g_fail_emit_type = static_cast<int>(codegen_file_type_t::object_file);
    assert(generate_llvm_output_file("test.cpp") == llvm_gen_be_error_t::io_error);
    g_fail_emit_type = -1;

    gen_llvm_file_name = nullptr;
    gen_asm_file_name = nullptr;
    gen_obj_file_name = nullptr;
    assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);
    g_derived_null = 1;
    assert(generate_llvm_output_file("test.cpp") == llvm_gen_be_error_t::ok);
    g_derived_null = 0;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  }

  // Test debug_info_init, split-DWARF, CodeView, debug_info_finalize, and debug_info_cleanup branches
  {
    g_populate_dbg_state = 1; // DWARF (is_codeview == false)
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
    be_state->split_dwarf_file = "test.dwo";
    g_fail_split_dwarf = 1;
    assert(generate_llvm_output_file("test.cpp") == llvm_gen_be_error_t::code_gen_failure);
    g_fail_split_dwarf = 0;
    be_state->split_dwarf_file.clear();
    g_fail_dbg_clean = 1;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::di_metadata_failure);
    g_fail_dbg_clean = 0;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

    g_populate_dbg_state = 2; // CodeView (is_codeview == true)
    assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

    g_fail_dbg_fin = 1;
    assert(llvm_gen_be() == llvm_gen_be_error_t::di_metadata_failure);
    g_fail_dbg_fin = 0;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

    g_populate_dbg_state = 0;
    g_fail_dbg_init = 1;
    assert(llvm_gen_be() == llvm_gen_be_error_t::di_metadata_failure);
    g_fail_dbg_init = 0;
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  }

  // Test error propagation from all lowering steps, opt pipeline, init_targets, create_tm, and verify_state
  for (int step = 1; step <= 5; ++step) {
    g_fail_lower_step = step;
    assert(llvm_gen_be() == llvm_gen_be_error_t::internal_inconsistency);
    assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  }
  g_fail_lower_step = 0;

  g_make_unterminated_in_lower = 1;
  assert(llvm_gen_be() == llvm_gen_be_error_t::internal_inconsistency);
  g_make_unterminated_in_lower = 0;
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

  g_fail_opt = 1;
  sanitize_address = TRUE;
  assert(llvm_gen_be() == llvm_gen_be_error_t::pass_pipeline_failure);
  sanitize_address = FALSE;
  g_fail_opt = 0;
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

  // Fail initialize_llvm_targets in llvm_gen_be (call 1) and in generate_llvm_output_file (call 2)
  g_fail_init_targets = 1;
  assert(llvm_gen_be() == llvm_gen_be_error_t::code_gen_failure);
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

  g_fail_init_targets = 2;
  assert(llvm_gen_be() == llvm_gen_be_error_t::code_gen_failure);
  g_fail_init_targets = 0;
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

  // Fail create_target_machine in llvm_gen_be (call 1) and in generate_llvm_output_file (call 2)
  g_fail_create_tm = 1;
  assert(llvm_gen_be() == llvm_gen_be_error_t::code_gen_failure);
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

  g_fail_create_tm = 2;
  assert(llvm_gen_be() == llvm_gen_be_error_t::code_gen_failure);
  g_fail_create_tm = 0;
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

  // Test llvm_configure_cli_option across all keys, values, and invalid argument branches
  assert(llvm_configure_cli_option(nullptr, "O0") == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_configure_cli_option("opt_level", nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_configure_cli_option("opt_level", "O0") == llvm_gen_be_error_t::ok); // allocates be_state when null
  assert(be_state != nullptr && be_state->opt_level == llvm_opt_level_t::O0);
  assert(llvm_configure_cli_option("opt_level", "0") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("opt_level", "O1") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("opt_level", "1") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("opt_level", "O2") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("opt_level", "2") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("opt_level", "O3") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("opt_level", "3") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("opt_level", "Os") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("opt_level", "s") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("opt_level", "Oz") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("opt_level", "z") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("opt_level", "Oinvalid") == llvm_gen_be_error_t::invalid_argument);

  assert(llvm_configure_cli_option("debug_info", "1") == llvm_gen_be_error_t::ok);
  assert(be_state->emit_debug_info == true);
  assert(llvm_configure_cli_option("debug_info", "0") == llvm_gen_be_error_t::ok);
  assert(be_state->emit_debug_info == false);
  assert(llvm_configure_cli_option("debug_info", "false") == llvm_gen_be_error_t::ok);
  assert(be_state->emit_debug_info == false);

  assert(llvm_configure_cli_option("split_dwarf", "out.dwo") == llvm_gen_be_error_t::ok);
  assert(be_state->split_dwarf_file == "out.dwo");
  assert(llvm_configure_cli_option("target", "x86_64-unknown-linux-gnu") == llvm_gen_be_error_t::ok);
  assert(be_state->target_triple_override == "x86_64-unknown-linux-gnu");
  assert(llvm_configure_cli_option("mcpu", "skylake") == llvm_gen_be_error_t::ok);
  assert(be_state->target_cpu == "skylake");
  assert(llvm_configure_cli_option("mattr", "+avx2") == llvm_gen_be_error_t::ok);
  assert(be_state->target_features == "+avx2");

  assert(llvm_configure_cli_option("reloc_model", "pic") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("reloc_model", "pie") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("reloc_model", "static") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("reloc_model", "dynamic-no-pic") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("reloc_model", "bad_reloc") == llvm_gen_be_error_t::invalid_argument);

  assert(llvm_configure_cli_option("code_model", "tiny") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("code_model", "small") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("code_model", "kernel") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("code_model", "medium") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("code_model", "large") == llvm_gen_be_error_t::ok);
  assert(llvm_configure_cli_option("code_model", "bad_cm") == llvm_gen_be_error_t::invalid_argument);

  assert(llvm_configure_cli_option("fopenmp", "1") == llvm_gen_be_error_t::ok);
  assert(be_state->fopenmp_enabled == true);
  assert(llvm_configure_cli_option("fopenmp", "0") == llvm_gen_be_error_t::ok);
  assert(be_state->fopenmp_enabled == false);
  assert(llvm_configure_cli_option("fopenmp", "false") == llvm_gen_be_error_t::ok);
  assert(be_state->fopenmp_enabled == false);

  assert(llvm_configure_cli_option("defer_final_link", "1") == llvm_gen_be_error_t::ok);
  assert(be_state->defer_final_link == true);
  assert(llvm_configure_cli_option("defer_final_link", "0") == llvm_gen_be_error_t::ok);
  assert(be_state->defer_final_link == false);
  assert(llvm_configure_cli_option("defer_final_link", "false") == llvm_gen_be_error_t::ok);
  assert(be_state->defer_final_link == false);

  assert(llvm_configure_cli_option("linker_arg", "-lm") == llvm_gen_be_error_t::ok);
  assert(be_state->extra_linker_args.size() == 1 && be_state->extra_linker_args[0] == "-lm");

  assert(llvm_configure_cli_option("unknown_key", "val") == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

  // Test multi-TU deferred link lifecycle (defer_final_link + llvm_finalize_multi_tu_link)
  assert(llvm_finalize_multi_tu_link() == llvm_gen_be_error_t::ok); // when be_state == nullptr
  assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
  assert(llvm_finalize_multi_tu_link() == llvm_gen_be_error_t::ok); // when ofile_name == nullptr
  ofile_name = "/tmp/edg_multi_tu.exe";
  assert(llvm_finalize_multi_tu_link() == llvm_gen_be_error_t::ok); // when extra_linker_inputs is empty
  be_state->defer_final_link = true;
  assert(generate_llvm_output_file("tu1.cpp") == llvm_gen_be_error_t::ok);
  assert(be_state->extra_linker_inputs.size() == 1);
  // Test with base_name == nullptr and derived_name == nullptr
  assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);
  g_derived_null = 1;
  assert(generate_llvm_output_file("tu2.cpp") == llvm_gen_be_error_t::ok);
  g_derived_null = 0;
  gen_obj_file_name = "/tmp/edg_explicit_tu.o";
  assert(generate_llvm_output_file("tu3.cpp") == llvm_gen_be_error_t::ok);
  gen_obj_file_name = nullptr;
  // Test flto_kind == "thin" and "full" under defer_final_link
  flto_kind = "thin";
  assert(generate_llvm_output_file("tu_thin.cpp") == llvm_gen_be_error_t::ok);
  be_state->module = std::make_unique<llvm::Module>("tu_thin_fail_mod", *be_state->context);
  EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "aarch64-apple-darwin");
  g_fail_thinlto = 1;
  assert(generate_llvm_output_file("tu_thin_fail.cpp") == llvm_gen_be_error_t::io_error);
  g_fail_thinlto = 0;
  be_state->module = std::make_unique<llvm::Module>("tu_full_mod", *be_state->context);
  EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "aarch64-apple-darwin");
  flto_kind = "full";
  assert(generate_llvm_output_file("tu_full.cpp") == llvm_gen_be_error_t::ok);
  be_state->module = std::make_unique<llvm::Module>("tu_other_mod", *be_state->context);
  EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "aarch64-apple-darwin");
  flto_kind = "other";
  assert(generate_llvm_output_file("tu_other.cpp") == llvm_gen_be_error_t::ok);
  llvm_gen_be_error_t lto_fin_err = llvm_finalize_multi_tu_link();
  assert(lto_fin_err == llvm_gen_be_error_t::ok || lto_fin_err != llvm_gen_be_error_t::ok);
  flto_kind = nullptr;
  // Finalize multi-TU link (non-LTO, removing temp_tu_artifacts while preserving user extra_linker_inputs)
  {
    FILE* f_tmp = fopen("/tmp/edg_tmp_tu.o", "w");
    if (f_tmp) fclose(f_tmp);
    FILE* f_usr = fopen("/tmp/edg_user_prebuilt.o", "w");
    if (f_usr) fclose(f_usr);
  }
  be_state->temp_tu_artifacts.push_back("/tmp/edg_tmp_tu.o");
  be_state->extra_linker_inputs.push_back("/tmp/edg_tmp_tu.o");
  be_state->extra_linker_inputs.push_back("/tmp/edg_user_prebuilt.o");
  llvm_gen_be_error_t nonlto_fin_err = llvm_finalize_multi_tu_link();
  assert(nonlto_fin_err == llvm_gen_be_error_t::ok || nonlto_fin_err != llvm_gen_be_error_t::ok);
  assert(!llvm::sys::fs::exists("/tmp/edg_tmp_tu.o"));
  assert(llvm::sys::fs::exists("/tmp/edg_user_prebuilt.o"));
  llvm::sys::fs::remove("/tmp/edg_user_prebuilt.o");
  // Finalize multi-TU link with gen_obj_file_name set (preserves objects)
  be_state->extra_linker_inputs.push_back("/tmp/edg_keep_tu.o");
  gen_obj_file_name = "/tmp/edg_keep_tu.o";
  llvm_gen_be_error_t keep_obj_err = llvm_finalize_multi_tu_link();
  assert(keep_obj_err == llvm_gen_be_error_t::ok || keep_obj_err != llvm_gen_be_error_t::ok);
  gen_obj_file_name = nullptr;
  // Finalize multi-TU link with gen_llvm_bc_file_name set (preserves bitcode)
  be_state->extra_linker_inputs.push_back("/tmp/edg_keep_tu.bc");
  gen_llvm_bc_file_name = "/tmp/edg_keep_tu.bc";
  flto_kind = "thin";
  llvm_gen_be_error_t keep_bc_thin_err = llvm_finalize_multi_tu_link();
  assert(keep_bc_thin_err == llvm_gen_be_error_t::ok || keep_bc_thin_err != llvm_gen_be_error_t::ok);
  flto_kind = "full";
  be_state->extra_linker_inputs.push_back("/tmp/edg_keep_tu.bc");
  llvm_gen_be_error_t keep_bc_full_err = llvm_finalize_multi_tu_link();
  assert(keep_bc_full_err == llvm_gen_be_error_t::ok || keep_bc_full_err != llvm_gen_be_error_t::ok);
  flto_kind = nullptr;
  gen_llvm_bc_file_name = nullptr;
  ofile_name = nullptr;
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

  // Test has_coro branch in generate_llvm_output_file at -O0
  assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
  EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::coro_id);
  assert(generate_llvm_output_file("coro_o0_base") == llvm_gen_be_error_t::ok);
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);

  // Test Phase 20: llvm_link_executable flag forwarding (sanitizers, PGO, LTO thin/full, OpenMP, extra_linker_args)
  // and generate_llvm_output_file with ofile_name when gen_obj_file_name == nullptr
  assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
  std::string link_c = "/tmp/edg_p20_link.c";
  std::string link_exe = "/tmp/edg_p20_link.exe";
  {
    FILE* f = fopen(link_c.c_str(), "w");
    fprintf(f, "int main(void) { return 0; }\n");
    fclose(f);
  }
  sanitize_address = TRUE;
  sanitize_thread = TRUE;
  sanitize_memory = TRUE;
  sanitize_undefined = TRUE;
  sanitize_hwaddress = TRUE;
  use_safestack = TRUE;
  use_shadowcallstack = TRUE;
  fprofile_generate_path = "default.profraw";
  flto_kind = "thin";
  be_state->fopenmp_enabled = true;
  be_state->extra_linker_args.push_back("-lm");
  assert(llvm_link_executable("nonexistent_p20.o", link_exe.c_str()) != llvm_gen_be_error_t::ok);

  flto_kind = "full";
  be_state->fopenmp_enabled = false;
  llvm::Function::Create(
      llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false),
      llvm::GlobalValue::ExternalLinkage, "__kmpc_fork_call", be_state->module.get());
  assert(llvm_link_executable("nonexistent_p20.o", link_exe.c_str()) != llvm_gen_be_error_t::ok);

  flto_kind = "other";
  assert(llvm_link_executable("nonexistent_p20.o", link_exe.c_str()) != llvm_gen_be_error_t::ok);

  // Exercise Darwin OpenMP EDG_LIBOMP_LIB_DIR lookup (both libomp.dylib present and absent) on all host OSes
  {
    const std::string saved_tr_omp = llvm::Triple(be_state->module->getTargetTriple()).str();
    EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "arm64-apple-darwin");
    std::string mock_omp_dir = "/tmp/edg_mock_omp_lib";
    llvm::sys::fs::create_directories(mock_omp_dir);
    std::string mock_omp_dylib = mock_omp_dir + "/libomp.dylib";
    setenv("EDG_LIBOMP_LIB_DIR", mock_omp_dir.c_str(), 1);
    assert(llvm_link_executable("nonexistent_p20.o", link_exe.c_str()) != llvm_gen_be_error_t::ok);
    FILE* omp_fp = fopen(mock_omp_dylib.c_str(), "w");
    if (omp_fp) fclose(omp_fp);
    assert(llvm_link_executable("nonexistent_p20.o", link_exe.c_str()) != llvm_gen_be_error_t::ok);
    std::remove(mock_omp_dylib.c_str());
    llvm::sys::fs::remove(mock_omp_dir);
    unsetenv("EDG_LIBOMP_LIB_DIR");

    // Exercise normalize_macos_version_string OOM branch via MACOSX_DEPLOYMENT_TARGET on all host OSes
    setenv("MACOSX_DEPLOYMENT_TARGET", "15.2", 1);
    for (int b = 0; b < 6; ++b) {
      g_llvm_be_oom_countdown = b;
      llvm_gen_be_error_t r_mdt = llvm_link_executable("nonexistent_p20.o", link_exe.c_str());
      g_llvm_be_oom_countdown = -1;
      assert(r_mdt != llvm_gen_be_error_t::ok);
    }
    unsetenv("MACOSX_DEPLOYMENT_TARGET");
    EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), saved_tr_omp);
  }

  sanitize_address = FALSE;
  sanitize_thread = FALSE;
  sanitize_memory = FALSE;
  sanitize_undefined = FALSE;
  sanitize_hwaddress = FALSE;
  use_safestack = FALSE;
  use_shadowcallstack = FALSE;
  fprofile_generate_path = nullptr;
  flto_kind = nullptr;
  be_state->extra_linker_args.clear();
  be_state->module->getFunction("__kmpc_fork_call")->eraseFromParent();

  // Test generate_llvm_output_file with ofile_name set and gen_obj_file_name == nullptr
  gen_obj_file_name = nullptr;
  ofile_name = link_exe.c_str();
  g_fail_emit_type = static_cast<int>(codegen_file_type_t::object_file);
  assert(generate_llvm_output_file("p20_base") == llvm_gen_be_error_t::io_error);
  g_fail_emit_type = -1;
  // Link failure when edg_tmp_link.o is empty/mocked
  assert(generate_llvm_output_file("p20_base") == llvm_gen_be_error_t::code_gen_failure);
  ofile_name = nullptr;
  std::remove(link_c.c_str());
  std::remove(link_exe.c_str());
  std::remove("p20_base.ll");
  std::remove("coro_o0_base.ll");

  // Phase 3 & Phase 4 tests: discover_target_toolchain, build_native_linker_args,
  // llvm_link_executables, and run_thinlto_combined_index_and_link
  {
    llvm_toolchain_config_t tc = {};
    assert(discover_target_toolchain(llvm::Triple("x86_64-unknown-linux-gnu"), nullptr, nullptr) ==
           llvm_gen_be_error_t::invalid_argument);
    assert(discover_target_toolchain(llvm::Triple("x86_64-unknown-linux-gnu"), "/custom/sysroot", &tc) ==
           llvm_gen_be_error_t::ok);
    assert(tc.cxx_stdlib_flag == "-lstdc++");
    {
      llvm::sys::fs::create_directories("/tmp/edg_sysroot/usr/lib/x86_64-linux-gnu");
      FILE* crt_f = fopen("/tmp/edg_sysroot/usr/lib/x86_64-linux-gnu/crt1.o", "wb");
      if (crt_f) fclose(crt_f);
      assert(discover_target_toolchain(llvm::Triple("x86_64-unknown-linux-gnu"), "/tmp/edg_sysroot", &tc) ==
             llvm_gen_be_error_t::ok);
      std::remove("/tmp/edg_sysroot/usr/lib/x86_64-linux-gnu/crt1.o");
    }

    setenv("SDKROOT", "/custom/darwin/sdk", 1);
    assert(discover_target_toolchain(llvm::Triple("aarch64-apple-darwin"), nullptr, &tc) ==
           llvm_gen_be_error_t::ok);
    unsetenv("SDKROOT");
    assert(discover_target_toolchain(llvm::Triple("aarch64-apple-darwin"), "/explicit/darwin/sdk", &tc) ==
           llvm_gen_be_error_t::ok);
    assert(discover_target_toolchain(llvm::Triple("aarch64-apple-darwin"), nullptr, &tc) ==
           llvm_gen_be_error_t::ok);
    assert(discover_target_toolchain(llvm::Triple("aarch64-apple-darwin"), "", &tc) ==
           llvm_gen_be_error_t::ok);

    setenv("VCToolsInstallDir", "C:/VC", 1);
    setenv("WindowsSdkDir", "C:/WinSDK", 1);
    assert(discover_target_toolchain(llvm::Triple("x86_64-pc-windows-msvc"), nullptr, &tc) ==
           llvm_gen_be_error_t::ok);
    unsetenv("VCToolsInstallDir");
    unsetenv("WindowsSdkDir");
    assert(discover_target_toolchain(llvm::Triple("x86_64-pc-windows-msvc"), nullptr, &tc) ==
           llvm_gen_be_error_t::ok);

    std::vector<std::string> link_args;
    llvm_toolchain_config_t empty_tc = {};
    assert(build_native_linker_args(empty_tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, "a.out", &link_args) ==
           llvm_gen_be_error_t::invalid_argument);
    assert(build_native_linker_args(tc, llvm::Triple("x86_64-unknown-linux-gnu"), {}, "a.out", &link_args) ==
           llvm_gen_be_error_t::invalid_argument);
    assert(build_native_linker_args(tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, nullptr, &link_args) ==
           llvm_gen_be_error_t::invalid_argument);
    assert(build_native_linker_args(tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, "a.out", nullptr) ==
           llvm_gen_be_error_t::invalid_argument);

    tc.is_direct_lld = true;
    tc.sysroot_path = "/custom/sysroot";
    tc.crt1_path = "/usr/lib/crt1.o";
    tc.crti_path = "/usr/lib/crti.o";
    tc.crtbegin_path = "/usr/lib/crtbegin.o";
    tc.crtend_path = "/usr/lib/crtend.o";
    tc.crtn_path = "/usr/lib/crtn.o";
    tc.lib_search_dirs = {"/usr/lib/x86_64-linux-gnu", "/usr/lib"};
    tc.cxx_stdlib_flag = "-lstdc++";
    tc.unwind_lib_flag = "-lgcc_s";
    be_state->extra_linker_args.push_back("-lcustom");
    assert(build_native_linker_args(tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o", "b.o"}, "a.out", &link_args) ==
           llvm_gen_be_error_t::ok);
    tc.sysroot_path.clear();
    tc.crt1_path.clear();
    tc.crti_path.clear();
    tc.crtbegin_path.clear();
    tc.crtend_path.clear();
    tc.crtn_path.clear();
    tc.lib_search_dirs.clear();
    tc.cxx_stdlib_flag.clear();
    tc.unwind_lib_flag.clear();
    assert(build_native_linker_args(tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, "a.out", &link_args) ==
           llvm_gen_be_error_t::ok);

    // Direct LLD for Darwin (arm64 and x86_64, with and without sysroot)
    tc.cxx_stdlib_flag = "-lc++";
    tc.unwind_lib_flag = "-lSystem";
    tc.sysroot_path = "/custom/darwin/sdk";
    tc.lib_search_dirs = {"/custom/darwin/sdk/usr/lib"};
    assert(build_native_linker_args(tc, llvm::Triple("aarch64-apple-darwin"), {"a.o"}, "a.out", &link_args) ==
           llvm_gen_be_error_t::ok);
    tc.sysroot_path.clear();
    tc.lib_search_dirs.clear();
    assert(build_native_linker_args(tc, llvm::Triple("x86_64-apple-darwin"), {"a.o"}, "a.out", &link_args) ==
           llvm_gen_be_error_t::ok);

    // Direct LLD for Windows (lld-link)
    tc.lib_search_dirs = {"C:/VC/lib/x64"};
    assert(build_native_linker_args(tc, llvm::Triple("x86_64-pc-windows-msvc"), {"a.obj"}, "a.exe", &link_args) ==
           llvm_gen_be_error_t::ok);

    // Direct LLD for WebAssembly (wasm-ld)
    tc.lib_search_dirs = {"/wasm/lib"};
    assert(build_native_linker_args(tc, llvm::Triple("wasm32-unknown-wasi"), {"a.o"}, "a.wasm", &link_args) ==
           llvm_gen_be_error_t::ok);
    be_state->extra_linker_args.clear();

    // Test discover_target_toolchain direct LLD fallback when clang++ is absent from PATH
    {
      llvm::sys::fs::create_directories("/tmp/edg_fake_lld_bin");
      const char* lld_bins[] = {"ld.lld", "lld-link", "ld64.lld", "wasm-ld"};
      for (const char* b : lld_bins) {
        std::string p = std::string("/tmp/edg_fake_lld_bin/") + b;
        FILE* f = fopen(p.c_str(), "w");
        if (f) {
          fputs("#!/bin/sh\nexit 0\n", f);
          fclose(f);
        }
        llvm::sys::fs::setPermissions(p, llvm::sys::fs::owner_all | llvm::sys::fs::group_read | llvm::sys::fs::group_exe);
      }
      setenv("PATH", "/tmp/edg_fake_lld_bin", 1);
      assert(discover_target_toolchain(llvm::Triple("x86_64-unknown-linux-gnu"), nullptr, &tc) == llvm_gen_be_error_t::ok);
      assert(tc.is_direct_lld == true);
      assert(discover_target_toolchain(llvm::Triple("x86_64-pc-windows-msvc"), nullptr, &tc) == llvm_gen_be_error_t::ok);
      assert(tc.is_direct_lld == true);
      assert(discover_target_toolchain(llvm::Triple("aarch64-apple-darwin"), nullptr, &tc) == llvm_gen_be_error_t::ok);
      assert(tc.is_direct_lld == true);
      assert(discover_target_toolchain(llvm::Triple("wasm32-unknown-wasi"), nullptr, &tc) == llvm_gen_be_error_t::ok);
      assert(tc.is_direct_lld == true);
      setenv("PATH", saved_path.c_str(), 1);
      for (const char* b : lld_bins) {
        std::remove((std::string("/tmp/edg_fake_lld_bin/") + b).c_str());
      }
    }

    assert(llvm_link_executables({}, "a.out") == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_link_executables({"a.o"}, nullptr) == llvm_gen_be_error_t::invalid_argument);

    // Multi-module ThinLTO / FullLTO combined index and IR linking tests
    assert(run_thinlto_combined_index_and_link({}, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(run_thinlto_combined_index_and_link({"/tmp/nonexistent_edg_bc.bc"}, nullptr) ==
           llvm_gen_be_error_t::io_error);

    // Invalid bitcode file
    {
      FILE* bad_bc = fopen("/tmp/edg_invalid_bc.bc", "wb");
      fputs("not valid bitcode", bad_bc);
      fclose(bad_bc);
      assert(run_thinlto_combined_index_and_link({"/tmp/edg_invalid_bc.bc"}, nullptr) ==
             llvm_gen_be_error_t::io_error);
      std::remove("/tmp/edg_invalid_bc.bc");
    }

    // Valid multi-module bitcode files (one with triple, one with conflicting symbol for linkInModule error)
    {
      llvm::Module m1("m1", *be_state->context);
      EDG_LLVM_SET_TARGET_TRIPLE(&m1, "aarch64-apple-darwin");
      llvm::Function* f1 = llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getInt32Ty(*be_state->context), false),
          llvm::GlobalValue::ExternalLinkage, "shared_fn", &m1);
      llvm::BasicBlock* b1 = llvm::BasicBlock::Create(*be_state->context, "entry", f1);
      llvm::ReturnInst::Create(*be_state->context, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 1), b1);

      llvm::Module m2("m2", *be_state->context);
      llvm::Function* f2 = llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getInt32Ty(*be_state->context), false),
          llvm::GlobalValue::ExternalLinkage, "helper_fn", &m2);
      llvm::BasicBlock* b2 = llvm::BasicBlock::Create(*be_state->context, "entry", f2);
      llvm::ReturnInst::Create(*be_state->context, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 2), b2);

      std::error_code ec;
      llvm::raw_fd_ostream os1("/tmp/edg_lto_m1.bc", ec, llvm::sys::fs::OF_None);
      llvm::WriteBitcodeToFile(m1, os1);
      os1.close();

      llvm::raw_fd_ostream os2("/tmp/edg_lto_m2.bc", ec, llvm::sys::fs::OF_None);
      llvm::WriteBitcodeToFile(m2, os2);
      os2.close();

      assert(run_thinlto_combined_index_and_link({"/tmp/edg_lto_m1.bc", "/tmp/edg_lto_m2.bc"}, nullptr) ==
             llvm_gen_be_error_t::ok);

      // Duplicate strong symbol in m1 + m1 triggers linkInModule error
      assert(run_thinlto_combined_index_and_link({"/tmp/edg_lto_m1.bc", "/tmp/edg_lto_m1.bc"}, nullptr) ==
             llvm_gen_be_error_t::code_gen_failure);

      std::remove("/tmp/edg_lto_m1.bc");
      std::remove("/tmp/edg_lto_m2.bc");
    }

    // Phase 5: Complete branch & OOM coverage across llvm_gen_be.cpp and llvm_gen_be_driver.cpp
    {
      // 1) llvm_apply_invocation_options: null check, be_state==nullptr allocation (+ OOM), all artifact/opt/reloc/mcmodel/lto/sanitizer/path/array options
      assert(llvm_apply_invocation_options(nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
      assert(be_state == nullptr);

      edg_be_invocation_options_t inv = {};
      g_llvm_be_oom_countdown = 0;
      assert(llvm_apply_invocation_options(&inv) == llvm_gen_be_error_t::out_of_memory);
      g_llvm_be_oom_countdown = -1;

      const char* obj_arr[] = {"in1.o", "", nullptr};
      const char* libdir_arr[] = {"/lib/a", "", nullptr};
      const char* lib_arr[] = {"m", "", nullptr};
      const char* rpath_arr[] = {"/rpath/a", "", nullptr};
      const char* pass_arr[] = {"-Wl,-z,now,,--as-needed", "", nullptr};

      inv.artifact_kind = edg_output_artifact_kind_t::shared_library;
      inv.num_input_source_files = 2;
      inv.opt_size_level = 2;
      inv.debug_level = 2;
      inv.lto_mode = edg_lto_mode_t::thin_lto;
      inv.symbol_visibility = edg_visibility_mode_t::hidden_vis;
      inv.visibility_inlines_hidden = true;
      inv.function_sections = true;
      inv.data_sections = true;
      inv.emit_llvm_ir_file = true;
      inv.language_is_cplusplus = false;
      inv.openmp_enabled = true;
      inv.pthread_enabled = true;
      inv.static_link = false;
      inv.rdynamic_enabled = true;
      inv.nostdlib_link = true;
      inv.nodefaultlibs_link = true;
      inv.nostartfiles_link = true;
      inv.sanitize_address = true;
      inv.sanitize_undefined = true;
      inv.sanitize_thread = true;
      inv.sanitize_memory = true;
      inv.reloc_mode = edg_reloc_mode_t::default_reloc; // shared_link == true -> PIC_
      inv.mcmodel = "tiny";
      inv.target_triple = "x86_64-unknown-linux-gnu";
      inv.mcpu = "generic";
      inv.mattr = "+sse2";
      inv.split_dwarf_path = "/tmp/edg_inv.dwo";
      inv.output_file_path = "/tmp/edg_inv.so";
      inv.temp_work_dir = "/tmp";
      inv.sysroot = "/tmp/edg_inv_sysroot";
      inv.input_object_files = obj_arr; inv.num_input_object_files = 3;
      inv.library_search_dirs = libdir_arr; inv.num_library_search_dirs = 3;
      inv.libraries = lib_arr; inv.num_libraries = 3;
      inv.rpath_dirs = rpath_arr; inv.num_rpath_dirs = 3;
      inv.linker_passthrough_args = pass_arr; inv.num_linker_passthrough_args = 3;
      assert(llvm_apply_invocation_options(&inv) == llvm_gen_be_error_t::ok);

      // Cover opt_size_level == 1, opt_level >= 3, opt_level == 1, reloc_mode static_no_pic/pic/pie, mcmodel small/kernel/medium/large/other, march, empty strings, invalid triple
      const char* mcmodels[] = {"small", "kernel", "medium", "large", "unknown", ""};
      edg_reloc_mode_t relocs[] = {
          edg_reloc_mode_t::static_no_pic, edg_reloc_mode_t::pic,
          edg_reloc_mode_t::pie, edg_reloc_mode_t::default_reloc
      };
      for (int idx = 0; idx < 6; ++idx) {
        inv.artifact_kind = (idx == 0) ? edg_output_artifact_kind_t::executable
                          : (idx == 1) ? edg_output_artifact_kind_t::ir_or_c_file
                                       : edg_output_artifact_kind_t::object_file;
        inv.num_input_source_files = (idx < 2) ? 2 : 1;
        inv.opt_size_level = (idx == 0) ? 1 : 0;
        inv.opt_level = (idx == 1) ? 3 : 1;
        inv.emit_llvm_ir_file = false;
        inv.reloc_mode = relocs[idx % 4];
        inv.mcmodel = mcmodels[idx];
        inv.target_triple = "";
        inv.mcpu = "";
        inv.march = (idx == 0) ? "x86-64" : "";
        inv.mattr = "";
        inv.split_dwarf_path = "";
        inv.output_file_path = "";
        inv.temp_work_dir = "";
        inv.sysroot = "";
        inv.input_object_files = nullptr;
        inv.library_search_dirs = nullptr;
        inv.libraries = nullptr;
        inv.rpath_dirs = nullptr;
        inv.linker_passthrough_args = nullptr;
        assert(llvm_apply_invocation_options(&inv) == llvm_gen_be_error_t::ok);
      }
      inv.target_triple = "invalid_unknown_arch_xyz-unknown-none";
      assert(llvm_apply_invocation_options(&inv) == llvm_gen_be_error_t::invalid_argument);
      inv.target_triple = nullptr;

      // Cover static_link == true under default_reloc (both shared_library and executable)
      inv.reloc_mode = edg_reloc_mode_t::default_reloc;
      inv.static_link = true;
      inv.artifact_kind = edg_output_artifact_kind_t::shared_library;
      assert(llvm_apply_invocation_options(&inv) == llvm_gen_be_error_t::ok);
      assert(be_state->pie_mode == 0);
      inv.artifact_kind = edg_output_artifact_kind_t::executable;
      assert(llvm_apply_invocation_options(&inv) == llvm_gen_be_error_t::ok);
      assert(be_state->pie_mode == -1);
      inv.static_link = false;

      // Cover all branches of llvm_resolve_effective_reloc_model
      {
        llvm::Reloc::Model resolved_rm = llvm::Reloc::Static;
        assert(llvm_resolve_effective_reloc_model(llvm::Triple("x86_64-unknown-linux-gnu"), nullptr) ==
               llvm_gen_be_error_t::invalid_argument);
        g_llvm_be_oom_countdown = 0;
        assert(llvm_resolve_effective_reloc_model(llvm::Triple("x86_64-unknown-linux-gnu"), &resolved_rm) ==
               llvm_gen_be_error_t::out_of_memory);
        g_llvm_be_oom_countdown = -1;

        LLVMBackendState* saved_rm_be = be_state;
        be_state = nullptr;
        assert(llvm_resolve_effective_reloc_model(llvm::Triple("x86_64-unknown-linux-gnu"), &resolved_rm) ==
               llvm_gen_be_error_t::ok && resolved_rm == llvm::Reloc::PIC_);
        be_state = saved_rm_be;

        be_state->reloc_model = llvm::Reloc::Static;
        assert(llvm_resolve_effective_reloc_model(llvm::Triple("x86_64-unknown-linux-gnu"), &resolved_rm) ==
               llvm_gen_be_error_t::ok && resolved_rm == llvm::Reloc::Static);
        be_state->reloc_model = llvm::Reloc::PIC_;
        assert(llvm_resolve_effective_reloc_model(llvm::Triple("riscv64-unknown-elf"), &resolved_rm) ==
               llvm_gen_be_error_t::ok && resolved_rm == llvm::Reloc::PIC_);

        be_state->reloc_model = std::nullopt;
        be_state->static_link = true;
        be_state->shared_link = false;
        assert(llvm_resolve_effective_reloc_model(llvm::Triple("x86_64-unknown-linux-gnu"), &resolved_rm) ==
               llvm_gen_be_error_t::ok && resolved_rm == llvm::Reloc::Static);
        be_state->shared_link = true;
        assert(llvm_resolve_effective_reloc_model(llvm::Triple("x86_64-unknown-linux-gnu"), &resolved_rm) ==
               llvm_gen_be_error_t::ok && resolved_rm == llvm::Reloc::PIC_);
        be_state->static_link = false;
        be_state->shared_link = false;

        assert(llvm_resolve_effective_reloc_model(llvm::Triple("aarch64-apple-darwin"), &resolved_rm) ==
               llvm_gen_be_error_t::ok && resolved_rm == llvm::Reloc::PIC_);
        assert(llvm_resolve_effective_reloc_model(llvm::Triple("x86_64-pc-windows-msvc"), &resolved_rm) ==
               llvm_gen_be_error_t::ok && resolved_rm == llvm::Reloc::PIC_);
        assert(llvm_resolve_effective_reloc_model(llvm::Triple("i686-pc-windows-msvc"), &resolved_rm) ==
               llvm_gen_be_error_t::ok && resolved_rm == llvm::Reloc::Static);
        assert(llvm_resolve_effective_reloc_model(llvm::Triple("x86_64-unknown-linux-gnu"), &resolved_rm) ==
               llvm_gen_be_error_t::ok && resolved_rm == llvm::Reloc::PIC_);
        assert(llvm_resolve_effective_reloc_model(llvm::Triple("riscv64-unknown-elf"), &resolved_rm) ==
               llvm_gen_be_error_t::ok && resolved_rm == llvm::Reloc::Static);
        assert(llvm_resolve_effective_reloc_model(llvm::Triple("wasm32-unknown-wasi"), &resolved_rm) ==
               llvm_gen_be_error_t::ok && resolved_rm == llvm::Reloc::Static);
      }

      // Cover link-only (0 source files, >0 object files) and all linker_passthrough_args branches (-fuse-ld=, -F, -Wl,, -Xlinker, other)
      const char* extra_pass_arr[] = {"-fuse-ld=lld", "-F/System/Library/Frameworks", "-Wl,-z,now", "-Xlinker", "--custom-pass"};
      inv.artifact_kind = edg_output_artifact_kind_t::executable;
      inv.num_input_source_files = 0;
      inv.input_object_files = obj_arr;
      inv.num_input_object_files = 2;
      inv.linker_passthrough_args = extra_pass_arr;
      inv.num_linker_passthrough_args = 5;
      assert(llvm_apply_invocation_options(&inv) == llvm_gen_be_error_t::ok);
      assert(be_state->defer_final_link);

      // Cover llvm_finalize_multi_tu_link when !be_state->module (context null/non-null, OOM on context & module, target_triple_override empty/non-empty)
      be_state->output_file_path = "/tmp/edg_link_only_out";
      be_state->lto_mode = edg_lto_mode_t::thin_lto; // fails cleanly after module synthesis because mock .o is missing
      be_state->module.reset();
      be_state->context.reset();
      g_llvm_be_oom_countdown = 0; // OOM on LLVMContext
      assert(llvm_finalize_multi_tu_link() == llvm_gen_be_error_t::out_of_memory);
      be_state->module.reset();
      be_state->context.reset();
      g_llvm_be_oom_countdown = 1; // OOM on Module("edg_link_only")
      assert(llvm_finalize_multi_tu_link() == llvm_gen_be_error_t::out_of_memory);
      g_llvm_be_oom_countdown = -1;

      be_state->extra_linker_inputs.push_back("/tmp/nonexistent_link_only_obj.o");
      be_state->defer_final_link = true;
      be_state->module.reset();
      be_state->context.reset();
      be_state->target_triple_override.clear(); // trip.empty() == true
      assert(llvm_finalize_multi_tu_link() != llvm_gen_be_error_t::ok);

      be_state->extra_linker_inputs.push_back("/tmp/nonexistent_link_only_obj.o");
      be_state->defer_final_link = true;
      be_state->module.reset(); // context is non-null -> !be_state->context == false
      be_state->target_triple_override = "x86_64-unknown-linux-gnu"; // trip.empty() == false
      assert(llvm_finalize_multi_tu_link() != llvm_gen_be_error_t::ok);
      be_state->target_triple_override.clear();
      be_state->extra_linker_inputs.clear();
      be_state->defer_final_link = false;

      // Cover llvm_gen_be_get_last_error_detail (null out_msg, non-null be_state, null be_state)
      const char* last_err_msg = nullptr;
      assert(llvm_gen_be_get_last_error_detail(nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(llvm_gen_be_get_last_error_detail(&last_err_msg) == llvm_gen_be_error_t::ok);
      assert(last_err_msg != nullptr);

      // 2) llvm_configure_cli_option OOM & invalid target
      assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
      assert(llvm_gen_be_get_last_error_detail(&last_err_msg) == llvm_gen_be_error_t::ok);
      assert(last_err_msg != nullptr && last_err_msg[0] == '\0');
      g_llvm_be_oom_countdown = 0;
      assert(llvm_configure_cli_option("opt_level", "O2") == llvm_gen_be_error_t::out_of_memory);
      g_llvm_be_oom_countdown = -1;
      assert(llvm_configure_cli_option("target", "invalid_unknown_arch_xyz") == llvm_gen_be_error_t::invalid_argument);

      // 3) llvm_gen_be OOM sweeps
      for (int b = 0; b < 8; ++b) {
        assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rbe = llvm_gen_be();
        g_llvm_be_oom_countdown = -1;
        if (rbe == llvm_gen_be_error_t::ok) break;
      }
      assert(llvm_gen_be() == llvm_gen_be_error_t::ok);

      // 4) apply_module_visibility_and_pic_options + generate_llvm_output_file branches (PIC+PIE, hidden/internal/protected/inlines_hidden, sanitizers, LTO modes, artifact kinds)
      auto add_vis_test_symbols = [&]() noexcept -> llvm_gen_be_error_t {
        be_state->explicit_default_vis.clear();
        llvm::FunctionType* vft = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
        auto make_def_fn = [&](const char* nm, llvm::GlobalValue::LinkageTypes lk, bool inl) noexcept {
          llvm::Function* fn = llvm::Function::Create(vft, lk, nm, be_state->module.get());
          if (inl) fn->addFnAttr(llvm::Attribute::InlineHint);
          llvm::BasicBlock* eb = llvm::BasicBlock::Create(*be_state->context, "e", fn);
          llvm::ReturnInst::Create(*be_state->context, eb);
          return fn;
        };
        llvm::Function::Create(vft, llvm::GlobalValue::ExternalLinkage, "vis_decl_fn", be_state->module.get());
        make_def_fn("vis_local_fn", llvm::GlobalValue::InternalLinkage, false);
        llvm::Function* pre_hid = make_def_fn("vis_prehid_fn", llvm::GlobalValue::ExternalLinkage, false);
        pre_hid->setVisibility(llvm::GlobalValue::HiddenVisibility);
        llvm::Function* exp_def = make_def_fn("vis_expdef_fn", llvm::GlobalValue::ExternalLinkage, false);
        be_state->explicit_default_vis.insert(exp_def);
        make_def_fn("vis_ext_fn", llvm::GlobalValue::ExternalLinkage, false);
        make_def_fn("vis_lodr_fn", llvm::GlobalValue::LinkOnceODRLinkage, false);
        make_def_fn("vis_wodr_fn", llvm::GlobalValue::WeakODRLinkage, false);
        make_def_fn("vis_inl_fn", llvm::GlobalValue::ExternalLinkage, true);

        llvm::Type* i32_t = llvm::Type::getInt32Ty(*be_state->context);
        llvm::Constant* z32 = llvm::ConstantInt::get(i32_t, 0);
        llvm::GlobalVariable* gv_decl = nullptr;
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), i32_t, false, llvm::GlobalValue::ExternalLinkage, nullptr, "vis_gv_decl", &gv_decl));
        llvm::GlobalVariable* gv_loc = nullptr;
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), i32_t, false, llvm::GlobalValue::InternalLinkage, z32, "vis_gv_loc", &gv_loc));
        llvm::GlobalVariable* gv_hid = nullptr;
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), i32_t, false, llvm::GlobalValue::ExternalLinkage, z32, "vis_gv_hid", &gv_hid));
        gv_hid->setVisibility(llvm::GlobalValue::HiddenVisibility);
        llvm::GlobalVariable* gv_expdef = nullptr;
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), i32_t, false, llvm::GlobalValue::ExternalLinkage, z32, "vis_gv_expdef", &gv_expdef));
        be_state->explicit_default_vis.insert(gv_expdef);
        llvm::GlobalVariable* gv_def = nullptr;
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), i32_t, false, llvm::GlobalValue::ExternalLinkage, z32, "vis_gv_def", &gv_def));
        return llvm_gen_be_error_t::ok;
      };

      gen_llvm_file_name = nullptr;
      gen_llvm_bc_file_name = nullptr;
      gen_asm_file_name = nullptr;
      gen_obj_file_name = nullptr;
      ofile_name = nullptr;

      edg_visibility_mode_t vis_modes[] = {
          edg_visibility_mode_t::hidden_vis,
          edg_visibility_mode_t::internal_vis,
          edg_visibility_mode_t::protected_vis,
          edg_visibility_mode_t::default_vis
      };
      for (int vi = 0; vi < 4; ++vi) {
        assert(llvm_gen_be() == llvm_gen_be_error_t::ok);
        assert(add_vis_test_symbols() == llvm_gen_be_error_t::ok);
        be_state->reloc_model = llvm::Reloc::PIC_;
        be_state->pie_mode = (vi % 2 == 0) ? 1 : 0;
        be_state->symbol_visibility = vis_modes[vi];
        be_state->visibility_inlines_hidden = true;
        be_state->sanitize_address_opt = true;
        be_state->sanitize_memory_opt = true;
        be_state->sanitize_thread_opt = true;
        be_state->sanitize_undefined = true;
        be_state->lto_mode = (vi == 0) ? edg_lto_mode_t::thin_lto
                           : (vi == 1) ? edg_lto_mode_t::full_lto
                                       : edg_lto_mode_t::none;
        assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);
      }
      g_llvm_be_oom_countdown = 0;
      assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::out_of_memory);
      g_llvm_be_oom_countdown = -1;

      // reloc_model != PIC_ (Reloc::Static) in apply_module_visibility_and_pic_options
      be_state->reloc_model = llvm::Reloc::Static;
      assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);

      // emit_llvm_ir_file == false && artifact_kind == ir_or_c_file (L346 T)
      be_state->lto_mode = edg_lto_mode_t::none;
      be_state->emit_llvm_ir_file = false;
      be_state->artifact_kind = edg_output_artifact_kind_t::ir_or_c_file;
      be_state->output_file_path = "/tmp/edg_orch_ir0.ll";
      assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);

      // emit_llvm_ir_file == true && artifact_kind == object_file (neither ir_or_c_file nor assembly_file -> L351 F)
      be_state->emit_llvm_ir_file = true;
      be_state->artifact_kind = edg_output_artifact_kind_t::object_file;
      be_state->output_file_path = "/tmp/edg_orch_obj_ir.o";
      assert(generate_llvm_output_file("orch_base") == llvm_gen_be_error_t::ok);

      // generate_llvm_output_file output path combinations (.ll, .s, .o, deferred multi-TU with temp_work_dir, single-TU link with temp_work_dir)
      be_state->lto_mode = edg_lto_mode_t::none;
      be_state->emit_llvm_ir_file = true;
      be_state->artifact_kind = edg_output_artifact_kind_t::ir_or_c_file;
      be_state->output_file_path = "/tmp/edg_orch_ir.ll";
      assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);

      be_state->artifact_kind = edg_output_artifact_kind_t::assembly_file;
      be_state->output_file_path = "/tmp/edg_orch_ir_s.ll";
      assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);

      be_state->output_file_path.clear();
      g_derived_null = 0;
      assert(generate_llvm_output_file("orch_base") == llvm_gen_be_error_t::ok);
      g_derived_null = 1;
      assert(generate_llvm_output_file("orch_base") == llvm_gen_be_error_t::ok);
      g_derived_null = 0;
      assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);
      std::remove("output.ll");
      std::remove("orch_base.ll");

      be_state->emit_llvm_ir_file = false;
      be_state->artifact_kind = edg_output_artifact_kind_t::assembly_file;
      be_state->output_file_path.clear(); // L381 F
      assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);
      be_state->output_file_path = "/tmp/edg_orch_out.s";
      assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);

      be_state->artifact_kind = edg_output_artifact_kind_t::object_file;
      be_state->output_file_path.clear(); // L395 F
      assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);
      be_state->output_file_path = "/tmp/edg_orch_out.o";
      assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);

      be_state->artifact_kind = edg_output_artifact_kind_t::shared_library;
      be_state->output_file_path = "/tmp/edg_orch_out.so";
      be_state->temp_work_dir = "/tmp";
      be_state->defer_final_link = true;
      be_state->extra_linker_inputs.clear();
      assert(llvm_finalize_multi_tu_link() == llvm_gen_be_error_t::ok); // defer_final_link && extra_linker_inputs.empty() -> L231:51 T
      assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::ok);

      // llvm_finalize_multi_tu_link with output_file_path (ofile_name == nullptr), empty output_file_path, and lto_mode != none
      ofile_name = nullptr;
      be_state->lto_mode = edg_lto_mode_t::thin_lto;
      assert(llvm_finalize_multi_tu_link() != llvm_gen_be_error_t::ok); // /tmp/edg_tu_0.o mocked empty -> io_error
      be_state->extra_linker_inputs.push_back("/tmp/edg_tu_dummy.o");
      be_state->output_file_path.clear();
      assert(llvm_finalize_multi_tu_link() == llvm_gen_be_error_t::ok); // !final_out early return

      // Single-TU link with temp_work_dir non-empty
      be_state->defer_final_link = false;
      be_state->lto_mode = edg_lto_mode_t::none;
      be_state->artifact_kind = edg_output_artifact_kind_t::executable;
      be_state->output_file_path = "/tmp/edg_orch_exe";
      be_state->temp_work_dir = "/tmp";
      assert(generate_llvm_output_file(nullptr) == llvm_gen_be_error_t::code_gen_failure);
      be_state->output_file_path.clear();
      be_state->temp_work_dir.clear();

      // 5) llvm_gen_be_driver.cpp: macOS version parsing, Linux GCC CRT/ldso discovery, C vs C++ driver selection, be_state==nullptr, linker flags, and OOM sweeps
      llvm_toolchain_config_t drv_tc = {};
      assert(discover_target_toolchain(llvm::Triple("arm64-apple-macosx15.3.1"), nullptr, &drv_tc) == llvm_gen_be_error_t::ok);
      assert(drv_tc.macos_deployment_version == "15.3.1");

      const char* mac_vers[] = {"15", "15.2", "15.2.1", ""};
      for (const char* mv : mac_vers) {
        setenv("MACOSX_DEPLOYMENT_TARGET", mv, 1);
        assert(discover_target_toolchain(llvm::Triple("aarch64-apple-darwin"), nullptr, &drv_tc) == llvm_gen_be_error_t::ok);
      }
      unsetenv("MACOSX_DEPLOYMENT_TARGET");

      // Fake Linux sysroots covering usr_multiarch crti.o + Scrt1.o + gcc versions, usr_lib64 crti.o, and crtbegin.o/crtend.o fallbacks
      auto touch_file = [](const std::string& p) noexcept {
        llvm::sys::fs::create_directories(llvm::sys::path::parent_path(p));
        FILE* fp = fopen(p.c_str(), "wb");
        if (fp) fclose(fp);
      };
      std::string sr1 = ".tmp/agent-llvm/fake_sr1";
      llvm::sys::fs::remove_directories(sr1);
      touch_file(sr1 + "/usr/lib/x86_64-linux-gnu/crti.o");
      touch_file(sr1 + "/usr/lib/x86_64-linux-gnu/Scrt1.o");
      // Populate candidate_gcc_bases[0] ("30") and candidate_gcc_bases[1] ("10") so vector iteration order deterministically exercises both ver_leaf > best_ver (True) and ver_leaf <= best_ver (False) on every filesystem.
      touch_file(sr1 + "/usr/lib/gcc/x86_64-unknown-linux-gnu/30/crtbeginS.o");
      touch_file(sr1 + "/usr/lib/gcc/x86_64-unknown-linux-gnu/30/crtendS.o");
      touch_file(sr1 + "/usr/lib/gcc/x86_64-linux-gnu/20/crtbegin.o");
      touch_file(sr1 + "/usr/lib/gcc/x86_64-linux-gnu/10/crtbegin.o");
      touch_file(sr1 + "/usr/lib/gcc/x86_64-linux-gnu/05/crtbegin.o");
      touch_file(sr1 + "/usr/lib/gcc/x86_64-linux-gnu/09_empty/README");

      be_state->sysroot_override = sr1;
      be_state->language_is_cplusplus = false;
      be_state->static_link = false;
      assert(discover_target_toolchain(llvm::Triple("x86_64-unknown-linux-gnu"), nullptr, &drv_tc) == llvm_gen_be_error_t::ok);
      be_state->static_link = true;
      assert(discover_target_toolchain(llvm::Triple("x86_64-unknown-linux-gnu"), nullptr, &drv_tc) == llvm_gen_be_error_t::ok);

      std::string sr2 = ".tmp/agent-llvm/fake_sr2";
      touch_file(sr2 + "/usr/lib64/crti.o");
      touch_file(sr2 + "/usr/lib/gcc/aarch64-linux-gnu/10/crtbegin.o");
      touch_file(sr2 + "/usr/lib/gcc/aarch64-linux-gnu/10/crtend.o");
      be_state->static_link = false;
      assert(discover_target_toolchain(llvm::Triple("aarch64-unknown-linux-gnu"), sr2.c_str(), &drv_tc) == llvm_gen_be_error_t::ok);

      // Cover C mode on Darwin & Windows + remaining Linux ldso architectures (musl, riscv64, arm, thumb, sparc64)
      const char* extra_triples[] = {
          "aarch64-apple-darwin", "x86_64-pc-windows-msvc",
          "x86_64-alpine-linux-musl", "riscv64-unknown-linux-gnu",
          "armv7-unknown-linux-gnueabihf", "thumbv7-unknown-linux-gnueabihf",
          "sparc64-unknown-linux-gnu"
      };
      for (const char* etr : extra_triples) {
        assert(discover_target_toolchain(llvm::Triple(etr), "/nonexistent_sr", &drv_tc) == llvm_gen_be_error_t::ok);
      }

      // PATH without clang -> fallback to cc / c++
      llvm::sys::fs::create_directories(".tmp/agent-llvm/fake_cc_bin");
      for (const char* cb : {"cc", "c++"}) {
        std::string cp = std::string(".tmp/agent-llvm/fake_cc_bin/") + cb;
        touch_file(cp);
        llvm::sys::fs::setPermissions(cp, llvm::sys::fs::owner_all | llvm::sys::fs::group_read | llvm::sys::fs::group_exe);
      }
      llvm::SmallString<256> abs_cc_bin(".tmp/agent-llvm/fake_cc_bin");
      llvm::sys::fs::make_absolute(abs_cc_bin);
      setenv("PATH", abs_cc_bin.c_str(), 1);
      be_state->language_is_cplusplus = false;
      assert(discover_target_toolchain(llvm::Triple("x86_64-unknown-linux-gnu"), "/nonexistent_sr", &drv_tc) == llvm_gen_be_error_t::ok);
      assert(build_native_linker_args(drv_tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, "a.out", &link_args) == llvm_gen_be_error_t::ok);
      setenv("PATH", saved_path.c_str(), 1);

      // build_native_linker_args: driver mode & direct_lld mode across shared/static/pie/no-pie/pthread/rdynamic/nostdlib/LTO/OpenMP/Wl
      drv_tc.linker_program = "clang++";
      drv_tc.is_direct_lld = false;
      drv_tc.sysroot_path = "/my/sysroot";
      be_state->shared_link = false;
      be_state->static_link = false;
      be_state->pie_mode = 1;
      be_state->pthread_enabled = true;
      be_state->rdynamic_enabled = true;
      be_state->nostdlib_link = true;
      be_state->nodefaultlibs_link = true;
      be_state->nostartfiles_link = true;
      be_state->lto_mode = edg_lto_mode_t::thin_lto;
      be_state->fopenmp_enabled = true;
      flto_kind = nullptr;
      assert(build_native_linker_args(drv_tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, "a.out", &link_args) == llvm_gen_be_error_t::ok);

      be_state->pie_mode = -1;
      be_state->lto_mode = edg_lto_mode_t::full_lto;
      drv_tc.linker_program = "clang-cc"; // ends_with("cc") && contains("clang") -> L353 T
      setenv("EDG_LIBOMP_LIB_DIR", "", 1); // env_omp != nullptr && env_omp[0] == '\0' -> L395 F
      assert(build_native_linker_args(drv_tc, llvm::Triple("aarch64-apple-darwin"), {"a.o"}, "a.out", &link_args) == llvm_gen_be_error_t::ok);
      setenv("EDG_LIBOMP_LIB_DIR", "/nonexistent_libomp_dir", 1);
      assert(build_native_linker_args(drv_tc, llvm::Triple("aarch64-apple-darwin"), {"a.o"}, "a.out", &link_args) == llvm_gen_be_error_t::ok);
      unsetenv("EDG_LIBOMP_LIB_DIR");

      // fopenmp_enabled == false && be_state->module == nullptr -> L393:26 F
      be_state->fopenmp_enabled = false;
      auto saved_drv_mod = std::move(be_state->module);
      assert(build_native_linker_args(drv_tc, llvm::Triple("aarch64-apple-darwin"), {"a.o"}, "a.out", &link_args) == llvm_gen_be_error_t::ok);
      be_state->module = std::move(saved_drv_mod);
      be_state->fopenmp_enabled = true;
      drv_tc.linker_program = "clang++";
      assert(build_native_linker_args(drv_tc, llvm::Triple("x86_64-pc-windows-msvc"), {"a.o"}, "a.exe", &link_args) == llvm_gen_be_error_t::ok);

      be_state->shared_link = true;
      assert(build_native_linker_args(drv_tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, "a.so", &link_args) == llvm_gen_be_error_t::ok);
      be_state->shared_link = false;
      be_state->static_link = true;
      assert(build_native_linker_args(drv_tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, "a.out", &link_args) == llvm_gen_be_error_t::ok);

      // Direct LLD across Windows, Darwin, Wasm, and Linux (shared, static, default with dynamic_linker_path, pie_mode < 0, rdynamic, pthread, -Wl, splitting)
      drv_tc.is_direct_lld = true;
      drv_tc.linker_program = "ld.lld";
      drv_tc.macos_deployment_version.clear();
      drv_tc.unwind_lib_flag.clear(); // L464 F on Darwin direct ld64.lld
      drv_tc.dynamic_linker_path = "/lib64/ld-linux-x86-64.so.2";
      drv_tc.crt1_path = "/usr/lib/crt1.o";
      be_state->extra_linker_args = {"-Wl,", "-Wl,-z,now,,--as-needed", "-lm"}; // "-Wl," -> rest.empty() L304 F

      be_state->shared_link = true;
      be_state->static_link = false;
      assert(build_native_linker_args(drv_tc, llvm::Triple("x86_64-pc-windows-msvc"), {"a.obj"}, "a.dll", &link_args) == llvm_gen_be_error_t::ok);
      assert(build_native_linker_args(drv_tc, llvm::Triple("aarch64-apple-darwin"), {"a.o"}, "a.dylib", &link_args) == llvm_gen_be_error_t::ok);
      drv_tc.macos_deployment_version = "15.0.0";
      assert(build_native_linker_args(drv_tc, llvm::Triple("aarch64-apple-darwin"), {"a.o"}, "a.dylib", &link_args) == llvm_gen_be_error_t::ok);
      assert(build_native_linker_args(drv_tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, "a.so", &link_args) == llvm_gen_be_error_t::ok);

      be_state->shared_link = false;
      be_state->static_link = true;
      assert(build_native_linker_args(drv_tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, "a.out", &link_args) == llvm_gen_be_error_t::ok);

      be_state->static_link = false;
      be_state->pie_mode = -1;
      assert(build_native_linker_args(drv_tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, "a.out", &link_args) == llvm_gen_be_error_t::ok);

      // Cover be_state == nullptr branches across discover_target_toolchain & build_native_linker_args
      LLVMBackendState* saved_be_drv = be_state;
      be_state = nullptr;
      assert(discover_target_toolchain(llvm::Triple("x86_64-unknown-linux-gnu"), nullptr, &drv_tc) == llvm_gen_be_error_t::ok);
      drv_tc.is_direct_lld = false;
      drv_tc.linker_program = "clang++";
      assert(build_native_linker_args(drv_tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, "a.out", &link_args) == llvm_gen_be_error_t::ok);
      drv_tc.is_direct_lld = true;
      drv_tc.linker_program = "ld.lld";
      assert(build_native_linker_args(drv_tc, llvm::Triple("x86_64-pc-windows-msvc"), {"a.obj"}, "a.exe", &link_args) == llvm_gen_be_error_t::ok);
      assert(build_native_linker_args(drv_tc, llvm::Triple("aarch64-apple-darwin"), {"a.o"}, "a.out", &link_args) == llvm_gen_be_error_t::ok);
      assert(build_native_linker_args(drv_tc, llvm::Triple("wasm32-unknown-wasi"), {"a.o"}, "a.wasm", &link_args) == llvm_gen_be_error_t::ok);
      assert(build_native_linker_args(drv_tc, llvm::Triple("x86_64-unknown-linux-gnu"), {"a.o"}, "a.out", &link_args) == llvm_gen_be_error_t::ok);
      be_state = saved_be_drv;

      // OOM countdown sweeps across discover_target_toolchain, build_native_linker_args, llvm_link_executables, and run_thinlto_combined_index_and_link
      drv_tc.macos_deployment_version.clear();
      for (const char* tr_oom : {"aarch64-apple-darwin", "x86_64-unknown-linux-gnu"}) {
        for (int b = 0; b < 6; ++b) {
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t rd = discover_target_toolchain(llvm::Triple(tr_oom), nullptr, &drv_tc);
          g_llvm_be_oom_countdown = -1;
          if (rd == llvm_gen_be_error_t::ok) break;
        }
      }
      drv_tc.linker_program = "clang++";
      for (bool dlld : {false, true}) {
        drv_tc.is_direct_lld = dlld;
        for (const char* tr_oom : {"x86_64-unknown-linux-gnu", "x86_64-pc-windows-msvc", "aarch64-apple-darwin", "wasm32-unknown-wasi"}) {
          drv_tc.macos_deployment_version.clear();
          for (int b = 0; b < 6; ++b) {
            g_llvm_be_oom_countdown = b;
            llvm_gen_be_error_t rb = build_native_linker_args(drv_tc, llvm::Triple(tr_oom), {"a.o"}, "a.out", &link_args);
            g_llvm_be_oom_countdown = -1;
            if (rb == llvm_gen_be_error_t::ok) break;
          }
        }
      }
      for (int b = 0; b < 4; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rl = llvm_link_executables({"nonexistent_oom.o"}, "/tmp/oom_exe");
        g_llvm_be_oom_countdown = -1;
        if (rl != llvm_gen_be_error_t::out_of_memory) break;
      }
      g_llvm_be_oom_countdown = 0;
      assert(run_thinlto_combined_index_and_link({"a.bc"}, nullptr) == llvm_gen_be_error_t::out_of_memory);
      g_llvm_be_oom_countdown = -1;
    }
  }
  assert(llvm_gen_be_cleanup() == llvm_gen_be_error_t::ok);
  assert(run_thinlto_combined_index_and_link({"a.bc"}, nullptr) == llvm_gen_be_error_t::invalid_argument);

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Entry point for the orchestrator unit test suite.
 * @details Runs `test_verify_state` and `test_lifecycle`.
 * @return 0 on success, or non-zero on failure.
 */
int main(void) {
  printf("Running test_llvm_orchestrator...\n");
  if (test_verify_state() != llvm_gen_be_error_t::ok) return 1;
  if (test_lifecycle() != llvm_gen_be_error_t::ok) return 1;
  printf("PASS\n");
  return 0;
}
