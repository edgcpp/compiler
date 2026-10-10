/**
 * @file llvm_gen_be.cpp
 * @brief Auto-generated brief.
 */
#include <type_traits>

/* llvm_gen_be.cpp - LLVM IR-generating back end core */
#include "basic_hdrs.h"
#include "fe_common.h"
#include "il_write.h"
#include "il_read.h"


#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_codegen.h"
#include "llvm_gen_be_debug.h"
#include "target.h"

// LLVM Includes
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/TargetParser/Host.h>

BEGIN_EDG_NAMESPACE

LLVMBackendState* be_state = nullptr;

/**
 * @brief build_data_layout
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
llvm_gen_be_error_t build_data_layout(std::string* out_dl) noexcept {
  if (!out_dl) return llvm_gen_be_error_t::invalid_argument;
  std::string dl = "";
  // Endianness
#if targ_little_endian
  dl += "e-";
#else
  dl += "E-";
#endif

#if 0
  dl += "m:o-";
#elif TARG_MICROSOFT
  dl += "m:w-";
#else
  dl += "m:e-";
#endif

  // Pointer size
  dl += "p:" + std::to_string((int)(targ_sizeof_pointer * targ_char_bit)) + ":" + std::to_string((int)(targ_sizeof_pointer * targ_char_bit)) + "-";

  // Data layout construction based on EDG target macros
  dl += "i8:" + std::to_string((int)(targ_char_bit)) + "-";
  dl += "i16:" + std::to_string((int)(targ_sizeof_short * targ_char_bit)) + "-";
  dl += "i32:" + std::to_string((int)(targ_sizeof_int * targ_char_bit)) + "-";
  dl += "i64:" + std::to_string((int)(targ_sizeof_long_long * targ_char_bit)) + "-";
  dl += "f32:" + std::to_string((int)(targ_sizeof_float * targ_char_bit)) + "-";
  dl += "f64:" + std::to_string((int)(targ_sizeof_double * targ_char_bit)) + "-";
  dl += "f128:" + std::to_string((int)(tar/**
 * @brief generate_llvm_output_file
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
g_sizeof_long_double * targ_char_bit));

  *out_dl = dl;
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t generate_llvm_output_file(const char* base_name) noexcept {
  std::string err_str;
  llvm::raw_string_ostream os(err_str);
  if (llvm::verifyModule(*be_state->module, &os)) {
    return llvm_gen_be_error_t::verification_failure;
  }

  // Initialize target registry once
  static bool targets_initialized = false;
  if (!targets_initialized) {
    llvm_gen_be_error_t init_err = initialize_llvm_targets();
    if (init_err != llvm_gen_be_error_t::ok) return init_err;
    targets_initialized = true;
  }

  llvm::TargetMachine* tm = nullptr;
  std::string triple_str = be_state->module->getTargetTriple().str();
  // Assume default target for now
  llvm_gen_be_error_t tm_err = create_target_machine(triple_str.c_str(), nullptr, nullptr, llvm::CodeGenOptLevel::Default, &tm);
  if (tm_err != llvm_gen_be_error_t::ok) {
    return tm_err;
  }

  // Emit .ll
  std::string ll_name;
  if (gen_llvm_file_name) {
    ll_name = gen_llvm_file_name;
  } else if (base_name) {
    const char* derived = derived_name(base_name, ".ll");
    if (derived) ll_name = derived;
  }
  if (ll_name.empty()) ll_name = "output.ll";
  
  llvm_gen_be_error_t emit_err = emit_machine_code_to_file(be_state->module.get(), tm, codegen_file_type_t::llvm_ir_text, ll_name.c_str());
  if (emit_err != llvm_gen_be_error_t::ok) {
    delete tm;
    return emit_err;
  }

  // Emit .bc
  if (gen_llvm_bc_file_name) {
    emit_err = emit_machine_code_to_file(be_state->module.get(), tm, codegen_file_type_t::bitcode_file, gen_llvm_bc_file_name);
    if (emit_err != llvm_gen_be_error_t::ok) {
      delete tm;
      return emit_err;
    }
  }

  // Emit .s
  if (gen_asm_file_name) {
    emit_err = emit_machine_code_to_file(be_state->module.get(), tm, codegen_file_type_t::assembly_file, gen_asm_file_name);
    if (emit_err != llvm_gen_be_error_t::ok) {
      delete tm;
      return emit_err;
    }
  }

  // Emit .o
  if (gen_obj_file_name) {
    emit_err = emit_machine_code_to_file(be_state->module.get(), tm, codege/**
 * @brief llvm_gen_be
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
n_file_type_t::object_file, gen_obj_file_name);
    if (emit_err != llvm_gen_be_error_t::ok) {
      delete tm;
      return emit_err;
    }
  }

  delete tm;
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_gen_be(void) noexcept
{
  if (!be_state) {
    be_state = new (std::nothrow) LLVMBackendState();
  }
  if (!be_state) return llvm_gen_be_error_t::out_of_memory;

  be_state->context = std::make_unique<llvm::LLVMContext>();
  if (!be_state->context) return llvm_gen_be_error_t::out_of_memory;

  be_state->module = std::make_unique<llvm::Module>("edg_module", *be_state->context);
  if (!be_state->module) return llvm_gen_be_error_t::out_of_memory;

  be_state->builder = std::make_unique<llvm::IRBuilder<>>(*be_state->context);
  if (!be_state->builder) return llvm_gen_be_error_t::out_of_memory;

  bool emit_debug_info = true; // Hardcode for now, or check an EDG option
  if (emit_debug_info) {
    llvm_gen_be_error_t err = debug_info_init(&be_state->dbg_state, be_state->module.get(), primary_source_file_name, "", false);
    if (err != llvm_gen_be_error_t::ok) return err;
  }
  
  // Set TargetTriple
// Initialize target registry once
static bool targets_initialized = false;
if (!targets_initialized) {
  llvm_gen_be_error_t init_err = initialize_llvm_targets();
  if (init_err != llvm_gen_be_error_t::ok) return init_err;
  targets_initialized = true;
}

std::string triple_str;

#if TARG_MAC_OS
#if TARG_AARCH64
  triple_str = "aarch64-apple-darwin";
#else
  triple_str = "x86_64-apple-darwin";
#endif
#elif TARG_MICROSOFT
#if TARG_AARCH64
  triple_str = "aarch64-pc-windows-msvc";
#else
  triple_str = "x86_64-pc-windows-msvc";
#endif
#else
#if TARG_AARCH64
  triple_str = "aarch64-unknown-linux-gnu";
#else
  triple_str = "x86_64-unknown-linux-gnu";
#endif
#endif

be_state->module->setTargetTriple(llvm::Triple(triple_str));

llvm::TargetMachine* tm = nullptr;
llvm_gen_be_error_t tm_err = create_target_machine(triple_str.c_str(), nullptr, nullptr, llvm::CodeGenOptLevel::Default, &tm);
if (tm_err != llvm_gen_be_error_t::ok) {
  return tm_err;
}

be_state->module->setDataLayout(tm->createDataLayout());
delete tm;

  
  llvm_gen_be_error_t err = llvm_lower_global_variables();
  if (err != llvm_gen_be_error_t::ok) return err;

  
  err = llvm_lower_function_declarations();
  if (err != llvm_gen_be_error_t::ok) return err;

  
  err = llvm_lower_function_definitions();
  if (err != llvm_gen_be_error_t::ok) return err;

  
  err = llvm_lower_global_ctors_and_dtors();
  if (err != llvm_gen_be_error_t::ok) return err;

  if (be_state->dbg_state) {
    err = debug_info_finalize(be_state->dbg_state);
    if (err != llvm_gen_be_error_t::ok) return err;
  }

  return generate_llvm_output_file(primary_source_file_name);
}

#if !STANDALONE_UTILITY_PROGRAM

llvm_gen_be_error_t back_end(void) noexcept
/*
Simple "back end" that generates LLVM IR. This version is for use as a
subroutine called in the same program as the front end.
*/
{
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* If the intermediate language was written to a file, read it back in. */
  primary_source_file_name = NULL;
  if (skip_il_read) {
    /* The IL should still be in memory. */
  } else {
    il_read(f_il_output);
  }  /* if */
  primary_source_file_name = il_header.primary_source_file->file_name;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

  /* Generate LLVM IR. */
  err = llvm_gen_be();
  return err;
}

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if MAKE_FRONT_END_CALLABLE

llvm_gen_be_error_t llvm_gen_be_cleanup(void) noexcept
/*
This routine is called at the end of compilation, or if compilation is
terminated prematurely for some reason. It performs any cleanup operations
required.
*/
{
  if (be_state) {
    if (be_state->dbg_state) {
      llvm_gen_be_error_t err = debug_info_cleanup(&be_state->dbg_state);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
    be_state->builder.reset();
    be_state->module.reset();
    be_state->context.reset();
    be_state->type_cache.clear();
    be_state->local_vars.clear();
    be_state->break_blocks.clear();
    be_state->continue_blocks.clear();
    be_state->current_landing_pads.clear();
    be_state->label_blocks.clear();
    be_state->case_blocks.clear();
    delete be_state;
    be_state = nullptr;
  }
  return llvm_gen_be_error_t::ok;
}

#endif /* MAKE_FRONT_END_CALLABLE */

END_EDG_NAMESPACE
