/**
 * @file llvm_gen_be.cpp
 * @ingroup llvm_gen_be_driver
 * @brief Core driver and translation unit orchestrator for the EDG LLVM backend.
 * @details Initializes the LLVM context/module/builder, configures target triple and data layout,
 *          lowers module metadata, variables, routines, and global ctors/dtors, runs optimization
 *          passes, and emits target machine artifacts.
 */
#include <type_traits>

/* llvm_gen_be.cpp - LLVM IR-generating back end core */
#include "basic_hdrs.h"
#include "fe_common.h"
#include "il_write.h"
#include "il_read.h"


#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_codegen.h"
#include "llvm_gen_be_driver.h"
#include "llvm_gen_be_debug.h"
#include "target.h"
#include "llvm_gen_be_opt.h"
#include "cmd_line.h"
#include <iostream>

// LLVM Includes
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/TargetParser/Host.h>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#if BACK_END_IS_LLVM_GEN_BE

BEGIN_EDG_NAMESPACE

LLVMBackendState* be_state = nullptr; ///< Global pointer to the active LLVM backend state.

/**
 * @brief Constructs the target LLVM data layout string based on EDG configuration.
 * @details Encodes endianness, pointer size, and primitive alignments into the data layout string.
 * @param[out] out_dl Receives the generated data layout string on success.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointer is null.
 */
llvm_gen_be_error_t build_data_layout(std::string* out_dl) noexcept {
  if (!out_dl) return llvm_gen_be_error_t::invalid_argument;
  std::string dl = "";
  // Endianness
  dl += (targ_little_endian != FALSE) ? "e-" : "E-";

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
  dl += "f128:" + std::to_string((int)(targ_sizeof_long_double * targ_char_bit));

  *out_dl = dl;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Synchronizes EDG front-end target sizing, alignment, and endianness globals with an LLVM target triple.
 * @details Normalizes @p triple_str and updates `targ_little_endian`, `targ_has_signed_chars`,
 *          `targ_sizeof_*`, `targ_alignof_*`, `targ_size_t_int_kind`, `targ_ptrdiff_t_int_kind`,
 *          `targ_wchar_t_int_kind`, and `targ_wint_t_int_kind` for LP64, LLP64, and ILP32 targets.
 * @param[in] triple_str Null-terminated LLVM target triple string (e.g., `"x86_64-pc-windows-msvc"`).
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p triple_str is null or empty.
 */
llvm_gen_be_error_t llvm_sync_fe_target_configuration(const char* triple_str) noexcept {
  if (!triple_str || triple_str[0] == '\0') {
    return llvm_gen_be_error_t::invalid_argument;
  }
  llvm::Triple triple = llvm::Triple(llvm::Triple::normalize(triple_str));
  if (triple.getArch() == llvm::Triple::UnknownArch) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  targ_char_bit = 8;
  targ_little_endian = triple.isLittleEndian() ? TRUE : FALSE;

  bool is_unsigned_char_arch =
      triple.isAArch64() || triple.isARM() || triple.isThumb() ||
      triple.isPPC() || triple.isRISCV() || triple.isSystemZ();
  if (triple.isOSDarwin() || triple.isOSWindows()) {
    is_unsigned_char_arch = false;
  }
  targ_has_signed_chars = is_unsigned_char_arch ? FALSE : TRUE;

  const bool is_64bit = triple.isArch64Bit();
  const bool is_windows = triple.isOSWindows();
  const bool is_darwin = triple.isOSDarwin();

  targ_sizeof_short = 2;
  targ_alignof_short = 2;
  targ_sizeof_int = 4;
  targ_alignof_int = 4;
  targ_sizeof_long_long = 8;
  targ_alignof_long_long = (triple.getArch() == llvm::Triple::x86 && !is_windows) ? 4 : 8;

  if (!is_64bit) {
    targ_sizeof_long = 4;
    targ_alignof_long = 4;
    targ_sizeof_pointer = 4;
    targ_alignof_pointer = 4;
    targ_size_t_int_kind = ik_unsigned_int;
    targ_ptrdiff_t_int_kind = ik_int;
  } else if (is_windows) {
    targ_sizeof_long = 4;
    targ_alignof_long = 4;
    targ_sizeof_pointer = 8;
    targ_alignof_pointer = 8;
    targ_size_t_int_kind = ik_unsigned_long_long;
    targ_ptrdiff_t_int_kind = ik_long_long;
  } else {
    targ_sizeof_long = 8;
    targ_alignof_long = 8;
    targ_sizeof_pointer = 8;
    targ_alignof_pointer = 8;
    targ_size_t_int_kind = ik_unsigned_long;
    targ_ptrdiff_t_int_kind = ik_long;
  }
#if IA64_ABI
  targ_ia64_vtable_entry_int_kind = targ_ptrdiff_t_int_kind;
#endif

  targ_sizeof_float = 4;
  targ_alignof_float = 4;
  targ_sizeof_double = 8;
  targ_alignof_double = (triple.getArch() == llvm::Triple::x86 && !is_windows) ? 4 : 8;

  if (is_windows || (is_darwin && triple.isAArch64()) || triple.isARM() || triple.isThumb()) {
    targ_sizeof_long_double = 8;
    targ_alignof_long_double = 8;
  } else if (triple.getArch() == llvm::Triple::x86) {
    targ_sizeof_long_double = 12;
    targ_alignof_long_double = 4;
  } else {
    targ_sizeof_long_double = 16;
    targ_alignof_long_double = 16;
  }

  if (is_windows) {
    targ_wchar_t_int_kind = ik_unsigned_short;
    targ_sizeof_wchar_t = 2;
    targ_wint_t_int_kind = ik_unsigned_short;
  } else {
    targ_wchar_t_int_kind = is_unsigned_char_arch ? ik_unsigned_int : ik_int;
    targ_sizeof_wchar_t = 4;
    targ_wint_t_int_kind = is_unsigned_char_arch ? ik_unsigned_int : ik_int;
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Applies global symbol visibility (`-fvisibility=*` and `-fvisibility-inlines-hidden`) to @p mod.
 * @details Updates non-declaration functions and globals with `DefaultVisibility` to match
 *          `be_state->symbol_visibility` and marks inline/linkonce_odr functions hidden when requested.
 * @param[in,out] mod Active LLVM module.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t apply_module_visibility_and_pic_options(llvm::Module* mod) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  if (be_state->reloc_model && *be_state->reloc_model == llvm::Reloc::PIC_) {
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(mod->getContext());
    mod->setModuleFlag(
        llvm::Module::Max, "PIC Level",
        llvm::ConstantAsMetadata::get(
            llvm::ConstantInt::get(i32_ty, static_cast<uint32_t>(llvm::PICLevel::BigPIC))));
    if (be_state->pie_mode > 0) {
      mod->setModuleFlag(
          llvm::Module::Max, "PIE Level",
          llvm::ConstantAsMetadata::get(
              llvm::ConstantInt::get(i32_ty, static_cast<uint32_t>(llvm::PIELevel::Large))));
    }
  }
  if (be_state->symbol_visibility == edg_visibility_mode_t::default_vis &&
      !be_state->visibility_inlines_hidden) {
    return llvm_gen_be_error_t::ok;
  }
  llvm::GlobalValue::VisibilityTypes target_vis = llvm::GlobalValue::DefaultVisibility;
  if (be_state->symbol_visibility == edg_visibility_mode_t::hidden_vis ||
      be_state->symbol_visibility == edg_visibility_mode_t::internal_vis) {
    target_vis = llvm::GlobalValue::HiddenVisibility;
  } else if (be_state->symbol_visibility == edg_visibility_mode_t::protected_vis) {
    target_vis = llvm::GlobalValue::ProtectedVisibility;
  }
  for (llvm::Function& fn : *mod) {
    if (fn.isDeclaration() || fn.hasLocalLinkage() ||
        fn.getVisibility() != llvm::GlobalValue::DefaultVisibility ||
        be_state->explicit_default_vis.count(&fn) != 0) {
      continue;
    }
    if (target_vis != llvm::GlobalValue::DefaultVisibility) {
      fn.setVisibility(target_vis);
    } else if (fn.hasLinkOnceODRLinkage() || fn.hasWeakODRLinkage() ||
               fn.hasFnAttribute(llvm::Attribute::InlineHint)) {
      fn.setVisibility(llvm::GlobalValue::HiddenVisibility);
    }
  }
  if (target_vis != llvm::GlobalValue::DefaultVisibility) {
    for (llvm::GlobalVariable& gv : mod->globals()) {
      if (!gv.isDeclaration() && !gv.hasLocalLinkage() &&
          gv.getVisibility() == llvm::GlobalValue::DefaultVisibility &&
          be_state->explicit_default_vis.count(&gv) == 0) {
        gv.setVisibility(target_vis);
      }
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Finalizes multi-translation-unit linking or LTO after all source files have been compiled.
 * @details Invokes `run_thinlto_combined_index_and_link` or `llvm_link_executables` on accumulated
 *          per-TU artifacts in `be_state->extra_linker_inputs` when `ofile_name` or `output_file_path`
 *          is set, and cleans up temporary intermediate object/bitcode files.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_finalize_multi_tu_link(void) noexcept {
  extern a_const_char* ofile_name;
  extern a_const_char* flto_kind;
  if (!be_state || !be_state->defer_final_link || be_state->extra_linker_inputs.empty()) {
    return llvm_gen_be_error_t::ok;
  }
  const char* final_out = ofile_name;
  if (!final_out && !be_state->output_file_path.empty()) {
    final_out = be_state->output_file_path.c_str();
  }
  if (!final_out) {
    return llvm_gen_be_error_t::ok;
  }
  if (!be_state->module) {
    if (!be_state->context) {
      llvm::LLVMContext* raw_ctx = nullptr;
      LLVM_GEN_BE_NEW(raw_ctx, llvm::LLVMContext());
      be_state->context.reset(raw_ctx);
    }
    llvm::Module* raw_mod = nullptr;
    LLVM_GEN_BE_NEW(raw_mod, llvm::Module("edg_link_only", *be_state->context));
    be_state->module.reset(raw_mod);
    std::string trip = be_state->target_triple_override;
    if (trip.empty()) {
      trip = llvm::sys::getDefaultTargetTriple();
    }
    EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), trip);
  }
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;
  bool is_lto = (flto_kind && (std::string(flto_kind) == "thin" || std::string(flto_kind) == "full")) ||
                (be_state->lto_mode != edg_lto_mode_t::none);
  if (is_lto) {
    err = run_thinlto_combined_index_and_link(be_state->extra_linker_inputs, final_out);
  } else {
    err = llvm_link_executables(be_state->extra_linker_inputs, final_out);
  }
  if (!gen_obj_file_name && !gen_llvm_bc_file_name) {
    for (const std::string& path : be_state->temp_tu_artifacts) {
      llvm::sys::fs::remove(path);
    }
  }
  be_state->temp_tu_artifacts.clear();
  be_state->extra_linker_inputs.clear();
  return err;
}

/**
 * @brief Emits the configured output files (.ll, .bc, .s, .o) for the compiled module.
 * @details Runs optimization pipelines, verifies module consistency, applies symbol visibility
 *          and section flags, and invokes TargetMachine emission without clobbering `.ll` files
 *          unless textual LLVM IR output was explicitly requested.
 * @param[in] base_name The base filename to use for generated output artifacts.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t generate_llvm_output_file(const char* base_name) noexcept {
  std::string err_str;
  llvm::raw_string_ostream os{err_str};
  if (llvm::verifyModule(*be_state->module, &os)) {
    std::cerr << "VERIFY FAILED: " << err_str << "\n";
    return llvm_gen_be_error_t::verification_failure;
  }

  LLVM_GEN_BE_CHECK(apply_module_visibility_and_pic_options(be_state->module.get()));

  llvm_opt_options_t opts = {};
  opts.opt_level = be_state->opt_level;
  opts.use_asan = (sanitize_address != FALSE) || be_state->sanitize_address_opt;
  opts.use_msan = (sanitize_memory != FALSE) || be_state->sanitize_memory_opt;
  opts.use_tsan = (sanitize_thread != FALSE) || be_state->sanitize_thread_opt;
  opts.use_ubsan = (sanitize_undefined != FALSE) || be_state->sanitize_undefined;
  opts.use_lto = false;
  extern a_const_char* fprofile_generate_path;
  extern a_const_char* fprofile_use_path;
  opts.pgo_generate = fprofile_generate_path != nullptr;
  opts.pgo_use_path = fprofile_use_path;

  extern a_const_char* flto_kind;
  bool is_thin_lto = (flto_kind && std::string(flto_kind) == "thin") ||
                     (be_state->lto_mode == edg_lto_mode_t::thin_lto);
  bool is_full_lto = (flto_kind && std::string(flto_kind) == "full") ||
                     (be_state->lto_mode == edg_lto_mode_t::full_lto);
  if (flto_kind || is_thin_lto || is_full_lto) {
      opts.use_lto = true;
      if (is_thin_lto) {
          be_state->module->addModuleFlag(llvm::Module::Error, "ThinLTO", 1);
          be_state->module->addModuleFlag(llvm::Module::Error, "EnableSplitLTOUnit", 1);
      } else if (is_full_lto) {
          be_state->module->addModuleFlag(llvm::Module::Error, "FullLTO", 1);
      }
  }

  bool has_coro = (be_state->module->getFunction("llvm.coro.id") != nullptr);
  if (opts.opt_level != llvm_opt_level_t::O0 || has_coro || opts.use_asan || opts.use_msan ||
      opts.use_tsan || opts.use_ubsan || opts.use_lto || opts.pgo_generate || opts.pgo_use_path) {
      llvm_gen_be_error_t opt_err = run_optimization_pipeline(be_state->module.get(), &opts);
      if (opt_err != llvm_gen_be_error_t::ok) return opt_err;
  }

  llvm_gen_be_error_t init_err = initialize_llvm_targets();
  if (init_err != llvm_gen_be_error_t::ok) return init_err;

  llvm_codegen_opt_level_t cg_opt = llvm_codegen_opt_level_t::None;
  switch (be_state->opt_level) {
    case llvm_opt_level_t::O0: cg_opt = llvm_codegen_opt_level_t::None; break;
    case llvm_opt_level_t::O1: cg_opt = llvm_codegen_opt_level_t::Less; break;
    case llvm_opt_level_t::O2:
    case llvm_opt_level_t::Os:
    case llvm_opt_level_t::Oz: cg_opt = llvm_codegen_opt_level_t::Default; break;
    case llvm_opt_level_t::O3: cg_opt = llvm_codegen_opt_level_t::Aggressive; break;
    default: cg_opt = llvm_codegen_opt_level_t::Default; break;
  }

  llvm::TargetMachine* tm = nullptr;
  std::string triple_str = llvm::Triple(be_state->module->getTargetTriple()).str();
  const char* cpu_cstr = be_state->target_cpu.empty() ? nullptr : be_state->target_cpu.c_str();
  const char* feat_cstr = be_state->target_features.empty() ? nullptr : be_state->target_features.c_str();
  llvm_gen_be_error_t tm_err = create_target_machine(
      triple_str.c_str(), cpu_cstr, feat_cstr, cg_opt, be_state->reloc_model, be_state->code_model, &tm);
  if (tm_err != llvm_gen_be_error_t::ok) {
    return tm_err;
  }

  if (be_state->emit_debug_info && be_state->dbg_state && !be_state->dbg_state->is_codeview &&
      !be_state->split_dwarf_file.empty()) {
    llvm_gen_be_error_t dwo_err = enable_split_dwarf_emission(tm, be_state->split_dwarf_file.c_str());
    if (dwo_err != llvm_gen_be_error_t::ok) {
      return dwo_err;
    }
  }

  llvm_gen_be_error_t emit_err = llvm_gen_be_error_t::ok;

  // Emit textual .ll ONLY when explicitly requested via gen_llvm_file_name or -emit-llvm
  if (gen_llvm_file_name || be_state->emit_llvm_ir_file ||
      be_state->artifact_kind == edg_output_artifact_kind_t::ir_or_c_file) {
    std::string ll_name;
    if (gen_llvm_file_name) {
      ll_name = gen_llvm_file_name;
    } else if ((be_state->artifact_kind == edg_output_artifact_kind_t::ir_or_c_file ||
                be_state->artifact_kind == edg_output_artifact_kind_t::assembly_file) &&
               !be_state->output_file_path.empty()) {
      ll_name = be_state->output_file_path;
    } else if (base_name) {
      const char* derived = derived_name(base_name, ".ll");
      if (derived) ll_name = derived;
    }
    if (ll_name.empty()) ll_name = "output.ll";
    emit_err = emit_machine_code_to_file(be_state->module.get(), tm, codegen_file_type_t::llvm_ir_text, ll_name.c_str());
    if (emit_err != llvm_gen_be_error_t::ok) {
      return emit_err;
    }
  }

  // Emit .bc
  if (gen_llvm_bc_file_name) {
    if (is_thin_lto) {
      emit_err = emit_thinlto_summary(be_state->module.get(), gen_llvm_bc_file_name);
    } else {
      emit_err = emit_machine_code_to_file(be_state->module.get(), tm, codegen_file_type_t::bitcode_file, gen_llvm_bc_file_name);
    }
    if (emit_err != llvm_gen_be_error_t::ok) {
      return emit_err;
    }
  }

  // Emit .s (unless -S -emit-llvm already targeted output_file_path as LLVM IR)
  const char* asm_out_path = gen_asm_file_name;
  if (!asm_out_path && !be_state->emit_llvm_ir_file &&
      be_state->artifact_kind == edg_output_artifact_kind_t::assembly_file &&
      !be_state->output_file_path.empty()) {
    asm_out_path = be_state->output_file_path.c_str();
  }
  if (asm_out_path) {
    emit_err = emit_machine_code_to_file(be_state->module.get(), tm, codegen_file_type_t::assembly_file, asm_out_path);
    if (emit_err != llvm_gen_be_error_t::ok) {
      return emit_err;
    }
  }

  // Emit .o and optional executable/shared-library linking
  extern a_const_char* ofile_name;
  const char* explicit_obj_path = gen_obj_file_name;
  if (!explicit_obj_path && be_state->artifact_kind == edg_output_artifact_kind_t::object_file &&
      !be_state->output_file_path.empty()) {
    explicit_obj_path = be_state->output_file_path.c_str();
  }
  const char* link_out_path = ofile_name;
  if (!link_out_path &&
      (be_state->artifact_kind == edg_output_artifact_kind_t::executable ||
       be_state->artifact_kind == edg_output_artifact_kind_t::shared_library) &&
      !be_state->output_file_path.empty()) {
    link_out_path = be_state->output_file_path.c_str();
  }

  if (explicit_obj_path || link_out_path) {
    if (be_state->defer_final_link) {
      std::string tu_art = explicit_obj_path ? std::string(explicit_obj_path) : "";
      if (tu_art.empty() && !be_state->temp_work_dir.empty()) {
        tu_art = be_state->temp_work_dir + "/edg_tu_" +
                 std::to_string(be_state->extra_linker_inputs.size()) + ".o";
      }
      if (tu_art.empty() && base_name) {
        const char* derived = derived_name(base_name, ".o");
        if (derived) tu_art = derived;
      }
      if (tu_art.empty()) {
        tu_art = "edg_tu_" + std::to_string(be_state->extra_linker_inputs.size()) + ".o";
      }
      if (is_thin_lto || is_full_lto) {
        if (is_thin_lto) {
          emit_err = emit_thinlto_summary(be_state->module.get(), tu_art.c_str());
        } else {
          emit_err = emit_machine_code_to_file(
              be_state->module.get(), tm, codegen_file_type_t::bitcode_file, tu_art.c_str());
        }
      } else {
        emit_err = emit_machine_code_to_file(
            be_state->module.get(), tm, codegen_file_type_t::object_file, tu_art.c_str());
      }
      if (emit_err != llvm_gen_be_error_t::ok) {
        return emit_err;
      }
      be_state->extra_linker_inputs.push_back(tu_art);
      if (!explicit_obj_path) {
        be_state->temp_tu_artifacts.push_back(tu_art);
      }
    } else {
      llvm::SmallString<128> sys_tmp_dir;
      llvm::sys::path::system_temp_directory(true, sys_tmp_dir);
      std::string work_dir = be_state->temp_work_dir.empty()
                                 ? std::string(sys_tmp_dir.str())
                                 : be_state->temp_work_dir;
#ifdef _WIN32
      unsigned long cur_pid = static_cast<unsigned long>(_getpid());
#else
      unsigned long cur_pid = static_cast<unsigned long>(getpid());
#endif
      std::string tmp_obj_storage =
          work_dir + "/edg_tmp_link_" + std::to_string(cur_pid) + ".o";
      const char* obj_path = explicit_obj_path ? explicit_obj_path : tmp_obj_storage.c_str();
      emit_err = emit_machine_code_to_file(be_state->module.get(), tm, codegen_file_type_t::object_file, obj_path);
      if (emit_err != llvm_gen_be_error_t::ok) {
        return emit_err;
      }
      if (link_out_path) {
        emit_err = llvm_link_executable(obj_path, link_out_path);
        if (!explicit_obj_path) {
          llvm::sys::fs::remove(obj_path);
        }
        if (emit_err != llvm_gen_be_error_t::ok) {
          return emit_err;
        }
      }
    }
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Configures an LLVM backend CLI option on the active `LLVMBackendState`.
 * @details Allocates `be_state` on demand and updates optimization level, debug info,
 *          target triple/CPU/features, relocation/code models, OpenMP, or linker flags.
 * @param[in] key The CLI option category key (e.g., `"opt_level"`, `"debug_info"`, `"target"`).
 * @param[in] val The CLI option value string.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p key or @p val is null or unrecognized.
 */
llvm_gen_be_error_t llvm_configure_cli_option(const char* key, const char* val) noexcept {
  if (!key || !val) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (!be_state) {
    LLVM_GEN_BE_NEW(be_state, LLVMBackendState());
  }
  llvm::StringRef k = llvm::StringRef(key);
  llvm::StringRef v = llvm::StringRef(val);
  if (k == "opt_level") {
    if (v == "O0" || v == "0") be_state->opt_level = llvm_opt_level_t::O0;
    else if (v == "O1" || v == "1") be_state->opt_level = llvm_opt_level_t::O1;
    else if (v == "O2" || v == "2") be_state->opt_level = llvm_opt_level_t::O2;
    else if (v == "O3" || v == "3") be_state->opt_level = llvm_opt_level_t::O3;
    else if (v == "Os" || v == "s") be_state->opt_level = llvm_opt_level_t::Os;
    else if (v == "Oz" || v == "z") be_state->opt_level = llvm_opt_level_t::Oz;
    else return llvm_gen_be_error_t::invalid_argument;
  } else if (k == "debug_info") {
    be_state->emit_debug_info = (v != "0" && v != "false");
  } else if (k == "split_dwarf") {
    be_state->split_dwarf_file = v.str();
  } else if (k == "target") {
    be_state->target_triple_override = v.str();
    LLVM_GEN_BE_RETURN_IF_ERROR(llvm_sync_fe_target_configuration(val));
  } else if (k == "mcpu") {
    be_state->target_cpu = v.str();
  } else if (k == "mattr") {
    be_state->target_features = v.str();
  } else if (k == "reloc_model") {
    if (v == "pic" || v == "pie") be_state->reloc_model = llvm::Reloc::PIC_;
    else if (v == "static") be_state->reloc_model = llvm::Reloc::Static;
    else if (v == "dynamic-no-pic") be_state->reloc_model = llvm::Reloc::DynamicNoPIC;
    else return llvm_gen_be_error_t::invalid_argument;
  } else if (k == "code_model") {
    if (v == "tiny") be_state->code_model = llvm::CodeModel::Tiny;
    else if (v == "small") be_state->code_model = llvm::CodeModel::Small;
    else if (v == "kernel") be_state->code_model = llvm::CodeModel::Kernel;
    else if (v == "medium") be_state->code_model = llvm::CodeModel::Medium;
    else if (v == "large") be_state->code_model = llvm::CodeModel::Large;
    else return llvm_gen_be_error_t::invalid_argument;
  } else if (k == "fopenmp") {
    be_state->fopenmp_enabled = (v != "0" && v != "false");
  } else if (k == "defer_final_link") {
    be_state->defer_final_link = (v != "0" && v != "false");
  } else if (k == "linker_arg") {
    be_state->extra_linker_args.push_back(v.str());
  } else {
    return llvm_gen_be_error_t::invalid_argument;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Primary entry point for the EDG LLVM IR generating backend.
 * @details Translates the EDG IL translation unit into LLVM IR, runs passes, and writes output files.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_gen_be(void) noexcept
{
  if (!be_state) {
    LLVM_GEN_BE_NEW(be_state, LLVMBackendState());
  }
  LLVM_GEN_BE_CHECK(reset_llvm_backend_tu_state(be_state));
  llvm::LLVMContext* raw_ctx = nullptr;
  LLVM_GEN_BE_NEW(raw_ctx, llvm::LLVMContext());
  be_state->context.reset(raw_ctx);
  llvm::Module* raw_mod = nullptr;
  LLVM_GEN_BE_NEW(raw_mod, llvm::Module("edg_module", *be_state->context));
  be_state->module.reset(raw_mod);
  llvm::IRBuilder<>* raw_builder = nullptr;
  LLVM_GEN_BE_NEW(raw_builder, llvm::IRBuilder<>(*be_state->context));
  be_state->builder.reset(raw_builder);
  be_state->enable_ubsan_checks = (sanitize_undefined != FALSE);

  llvm_gen_be_error_t init_err = initialize_llvm_targets();
  if (init_err != llvm_gen_be_error_t::ok) return init_err;

  std::string triple_str = be_state->target_triple_override;
  if (triple_str.empty()) {
#if defined(__APPLE__)
    llvm::Triple host_tr{llvm::sys::getDefaultTargetTriple()};
    triple_str = (host_tr.getArchName() + "-apple-macosx" +
                  host_tr.getOSVersion().getAsString()).str();
#elif defined(_WIN32)
#if defined(__aarch64__) || defined(_M_ARM64)
    triple_str = "aarch64-pc-windows-msvc";
#else
    triple_str = "x86_64-pc-windows-msvc";
#endif
#else
#if defined(__aarch64__)
    triple_str = "aarch64-unknown-linux-gnu";
#else
    triple_str = "x86_64-unknown-linux-gnu";
#endif
#endif
  }

  EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), triple_str);

  if (be_state->emit_debug_info) {
    bool is_opt = (be_state->opt_level != llvm_opt_level_t::O0);
    llvm_gen_be_error_t err = debug_info_init(
        &be_state->dbg_state, be_state->module.get(), primary_source_file_name, "", is_opt);
    if (err != llvm_gen_be_error_t::ok) return err;
  }

  llvm::TargetMachine* tm = nullptr;
  llvm_gen_be_error_t tm_err = create_target_machine(triple_str.c_str(), nullptr, nullptr, llvm_codegen_opt_level_t::Default, std::nullopt, std::nullopt, &tm);
  if (tm_err != llvm_gen_be_error_t::ok) {
    return tm_err;
  }

  be_state->module->setDataLayout(tm->createDataLayout());

  
  llvm_gen_be_error_t err = llvm_lower_module_metadata();
  if (err != llvm_gen_be_error_t::ok) return err;

  err = llvm_lower_global_variables();
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

  err = llvm_gen_be_verify_state();
  if (err != llvm_gen_be_error_t::ok) return err;

  return generate_llvm_output_file(primary_source_file_name);
}

/**
 * @brief Verifies that all basic blocks in the module possess proper terminators.
 * @details Traverses all non-declaration functions and inspects each basic block.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_gen_be_verify_state(void) noexcept {
  if (!be_state || !be_state->module) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  for (llvm::Function& f : *be_state->module) {
    if (f.isDeclaration()) continue;
    for (llvm::BasicBlock& bb : f) {
      if (bb.empty() || !bb.back().isTerminator()) {
        return llvm_gen_be_set_error(&be_state->err_context, llvm_gen_be_error_t::internal_inconsistency,
                              __FILE__, __LINE__, 0, "Basic block '%s' in function '%s' is missing a terminator.",
                              bb.getName().str().c_str(), f.getName().str().c_str());
      }
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resets all per-translation-unit maps and state in an `LLVMBackendState`.
 * @details Clears `type_cache`, `local_vars`, `global_dynamic_inits`, `omp_pragmas`,
 *          `escaped_allocas`, label/case/cleanup stacks, and scope caches while preserving
 *          CLI configuration options across multi-TU compilation (`COMPILE_MULTIPLE_SOURCE_FILES`).
 * @param[in,out] state Pointer to the backend state to reset.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `state` is null.
 */
llvm_gen_be_error_t reset_llvm_backend_tu_state(LLVMBackendState* state) noexcept {
  if (!state) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  if (state->dbg_state) {
    LLVM_GEN_BE_CHECK(debug_info_cleanup(&state->dbg_state));
  }
  state->builder.reset();
  state->module.reset();
  state->type_cache.clear();
  state->local_vars.clear();
  state->vla_saved_stacks.clear();
  state->vla_ptr_slots.clear();
  state->array_to_vla_dim.clear();
  state->vla_dimension_values.clear();
  state->reused_values.clear();
  state->break_blocks.clear();
  state->continue_blocks.clear();
  state->break_cleanup_depths.clear();
  state->continue_cleanup_depths.clear();
  state->current_landing_pads.clear();
  state->current_lpad_dispatch_bbs.clear();
  state->current_lpad_exc_ptr_phis.clear();
  state->current_lpad_exc_sel_phis.clear();
  state->current_lpad_handlers.clear();
  state->landing_pad_cleanup_depths.clear();
  state->lowered_eh_cleanup_barriers.clear();
  state->active_funclet_pads.clear();
  state->seh_leave_blocks.clear();
  state->stmt_expr_cleanup_depths.clear();
  state->global_dynamic_inits.clear();
  state->current_routine_is_ctor_or_dtor = false;
  state->escaped_allocas.clear();
  state->escaped_var_indices.clear();
  state->cleanup_stack.cleanups.clear();
  state->label_blocks.clear();
  state->label_cleanup_depths.clear();
  state->case_blocks.clear();
  state->string_literal_cache.clear();
  state->temporary_constant_cache.clear();
  state->current_function_params.clear();
  state->visited_scopes.clear();
  state->emitted_vtables.clear();
  state->emitted_rtti.clear();
  state->active_pragma_list = nullptr;
  state->omp_pragmas.clear();
  state->omp_source_file_cache.clear();
  state->omp_scanned_pragma_strings.clear();
  state->current_stmt_expr_result_ptr = nullptr;
  state->current_coro_handle = nullptr;
  state->current_coro_promise = nullptr;
  state->current_coro_gro_slot = nullptr;
  state->current_coro_cleanup_bb = nullptr;
  state->current_coro_end_bb = nullptr;
  state->current_coro_suspend_bb = nullptr;
  state->current_coro_final_suspend_bb = nullptr;
  state->current_coro_final_await_expr = nullptr;
  state->current_edg_routine = nullptr;
  state->current_function_scope = nullptr;
  state->active_caught_object_ptr = nullptr;
  state->active_thrown_object_ptr = nullptr;
  state->active_lowered_eh_cleanup = nullptr;
  state->active_prvalue_dest_ptr = nullptr;
  state->active_prvalue_dest_ty = nullptr;
  state->prvalue_constructed_in_place = false;
  return llvm_gen_be_error_t::ok;
}

#if !STANDALONE_UTILITY_PROGRAM

/**
 * @brief Entry point invoked by the EDG front end to execute the LLVM backend.
 * @details Reads serialized IL if necessary and invokes `llvm_gen_be()`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t back_end(void) noexcept
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

/**
 * @brief Applies normalized Stage-0 driver invocation options to the active LLVM backend state.
 * @details Validates @p opts, allocates `be_state` with `std::nothrow` if needed, and maps
 *          artifact kind, optimization/size levels, debug info, relocation/code/LTO models,
 *          symbol visibility, section splitting, sanitizers, C/C++ language mode, output/temp
 *          paths, and linker arguments onto `be_state`.
 * @param[in] opts Pointer to the normalized invocation options structure.
 * @return llvm_gen_be_error_t::ok on success, `invalid_argument` if @p opts is null, or `out_of_memory`.
 */
llvm_gen_be_error_t llvm_apply_invocation_options(
    const edg_be_invocation_options_t* opts) noexcept
{
  if (!opts) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (!be_state) {
    LLVM_GEN_BE_NEW(be_state, LLVMBackendState());
  }

  be_state->artifact_kind = opts->artifact_kind;
  be_state->shared_link = (opts->artifact_kind == edg_output_artifact_kind_t::shared_library);
  be_state->defer_final_link =
      ((opts->num_input_source_files > 1) ||
       (opts->num_input_source_files == 0 && opts->num_input_object_files > 0)) &&
      (opts->artifact_kind == edg_output_artifact_kind_t::executable ||
       opts->artifact_kind == edg_output_artifact_kind_t::shared_library);

  if (opts->opt_size_level >= 2) {
    be_state->opt_level = llvm_opt_level_t::Oz;
  } else if (opts->opt_size_level == 1) {
    be_state->opt_level = llvm_opt_level_t::Os;
  } else if (opts->opt_level >= 3) {
    be_state->opt_level = llvm_opt_level_t::O3;
  } else {
    be_state->opt_level = static_cast<llvm_opt_level_t>(opts->opt_level);
  }

  be_state->emit_debug_info = (opts->debug_level > 0);
  be_state->lto_mode = opts->lto_mode;
  be_state->symbol_visibility = opts->symbol_visibility;
  be_state->visibility_inlines_hidden = opts->visibility_inlines_hidden;
  be_state->function_sections = opts->function_sections;
  be_state->data_sections = opts->data_sections;
  be_state->emit_llvm_ir_file =
      opts->emit_llvm_ir_file || (opts->artifact_kind == edg_output_artifact_kind_t::ir_or_c_file);
  be_state->language_is_cplusplus = opts->language_is_cplusplus;
  be_state->fopenmp_enabled = opts->openmp_enabled;
  be_state->pthread_enabled = opts->pthread_enabled;
  be_state->static_link = opts->static_link;
  be_state->rdynamic_enabled = opts->rdynamic_enabled;
  be_state->nostdlib_link = opts->nostdlib_link;
  be_state->nodefaultlibs_link = opts->nodefaultlibs_link;
  be_state->nostartfiles_link = opts->nostartfiles_link;

  be_state->sanitize_address_opt = opts->sanitize_address;
  be_state->sanitize_undefined = opts->sanitize_undefined;
  be_state->enable_ubsan_checks = opts->sanitize_undefined;
  be_state->sanitize_thread_opt = opts->sanitize_thread;
  be_state->sanitize_memory_opt = opts->sanitize_memory;

  switch (opts->reloc_mode) {
    case edg_reloc_mode_t::static_no_pic:
      be_state->reloc_model = llvm::Reloc::Static;
      be_state->pie_mode = -1;
      break;
    case edg_reloc_mode_t::pic:
      be_state->reloc_model = llvm::Reloc::PIC_;
      be_state->pie_mode = 0;
      break;
    case edg_reloc_mode_t::pie:
      be_state->reloc_model = llvm::Reloc::PIC_;
      be_state->pie_mode = 1;
      break;
    default:
      be_state->reloc_model = be_state->shared_link
                                  ? std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_)
                                  : std::nullopt;
      be_state->pie_mode = 0;
      break;
  }

  if (opts->mcmodel && opts->mcmodel[0] != '\0') {
    llvm::StringRef cm = llvm::StringRef(opts->mcmodel);
    if (cm == "tiny") be_state->code_model = llvm::CodeModel::Tiny;
    else if (cm == "small") be_state->code_model = llvm::CodeModel::Small;
    else if (cm == "kernel") be_state->code_model = llvm::CodeModel::Kernel;
    else if (cm == "medium") be_state->code_model = llvm::CodeModel::Medium;
    else if (cm == "large") be_state->code_model = llvm::CodeModel::Large;
  }

  if (opts->target_triple && opts->target_triple[0] != '\0') {
    be_state->target_triple_override = opts->target_triple;
    LLVM_GEN_BE_RETURN_IF_ERROR(llvm_sync_fe_target_configuration(opts->target_triple));
  }
  if (opts->mcpu && opts->mcpu[0] != '\0') {
    be_state->target_cpu = opts->mcpu;
  } else if (opts->march && opts->march[0] != '\0') {
    be_state->target_cpu = opts->march;
  }
  if (opts->mattr && opts->mattr[0] != '\0') {
    be_state->target_features = opts->mattr;
  }
  if (opts->split_dwarf_path && opts->split_dwarf_path[0] != '\0') {
    be_state->split_dwarf_file = opts->split_dwarf_path;
  }
  be_state->output_file_path = (opts->output_file_path && opts->output_file_path[0] != '\0')
                                   ? std::string(opts->output_file_path)
                                   : std::string();
  be_state->temp_work_dir = (opts->temp_work_dir && opts->temp_work_dir[0] != '\0')
                                ? std::string(opts->temp_work_dir)
                                : std::string();
  be_state->sysroot_override = (opts->sysroot && opts->sysroot[0] != '\0')
                                   ? std::string(opts->sysroot)
                                   : std::string();

  be_state->temp_tu_artifacts.clear();
  be_state->extra_linker_inputs.clear();
  if (opts->input_object_files) {
    for (size_t i = 0; i < opts->num_input_object_files; ++i) {
      if (opts->input_object_files[i] && opts->input_object_files[i][0] != '\0') {
        be_state->extra_linker_inputs.push_back(opts->input_object_files[i]);
      }
    }
  }

  be_state->extra_linker_args.clear();
  if (opts->library_search_dirs) {
    for (size_t i = 0; i < opts->num_library_search_dirs; ++i) {
      if (opts->library_search_dirs[i] && opts->library_search_dirs[i][0] != '\0') {
        be_state->extra_linker_args.push_back(std::string("-L") + opts->library_search_dirs[i]);
      }
    }
  }
  if (opts->libraries) {
    for (size_t i = 0; i < opts->num_libraries; ++i) {
      if (opts->libraries[i] && opts->libraries[i][0] != '\0') {
        be_state->extra_linker_args.push_back(std::string("-l") + opts->libraries[i]);
      }
    }
  }
  if (opts->rpath_dirs) {
    for (size_t i = 0; i < opts->num_rpath_dirs; ++i) {
      if (opts->rpath_dirs[i] && opts->rpath_dirs[i][0] != '\0') {
        be_state->extra_linker_args.push_back(std::string("-Wl,-rpath,") + opts->rpath_dirs[i]);
      }
    }
  }
  if (opts->linker_passthrough_args) {
    for (size_t i = 0; i < opts->num_linker_passthrough_args; ++i) {
      const char* larg = opts->linker_passthrough_args[i];
      if (larg && larg[0] != '\0') {
        if (std::strncmp(larg, "-fuse-ld=", 9) == 0 ||
            std::strncmp(larg, "-F", 2) == 0 ||
            std::strncmp(larg, "-Wl,", 4) == 0 ||
            std::strcmp(larg, "-Xlinker") == 0) {
          be_state->extra_linker_args.push_back(larg);
        } else {
          be_state->extra_linker_args.push_back("-Xlinker");
          be_state->extra_linker_args.push_back(larg);
        }
      }
    }
  }
  return llvm_gen_be_error_t::ok;
}

#if MAKE_FRONT_END_CALLABLE

/**
 * @brief Cleans up internal state and resources allocated by the LLVM backend.
 * @details Resets all LLVM handles, symbol caches, and cleanup stacks, and deletes `be_state`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_gen_be_cleanup(void) noexcept
{
  if (be_state) {
    llvm_gen_be_error_t tu_err = reset_llvm_backend_tu_state(be_state);
    be_state->builder.reset();
    be_state->module.reset();
    be_state->context.reset();
    be_state->extra_linker_args.clear();
    be_state->extra_linker_inputs.clear();
    be_state->temp_tu_artifacts.clear();
    be_state->defer_final_link = false;
    be_state->fopenmp_enabled = false;
    be_state->enable_ubsan_checks = false;
    be_state->target_triple_override.clear();
    be_state->split_dwarf_file.clear();
    be_state->target_cpu.clear();
    be_state->target_features.clear();
    be_state->output_file_path.clear();
    be_state->temp_work_dir.clear();
    be_state->sysroot_override.clear();
    be_state->reloc_model = std::nullopt;
    be_state->code_model = std::nullopt;
    be_state->err_context = llvm_gen_be_error_context_t{};
    delete be_state;
    be_state = nullptr;
    LLVM_GEN_BE_CHECK(tu_err);
  }
  return release_target_machine_cache();
}

#endif /* MAKE_FRONT_END_CALLABLE */

/**
 * @brief Retrieves the human-readable detail message recorded for the most recent LLVM backend error.
 * @details Returns a pointer to the internal error context buffer (or an empty string if `be_state` is null).
 * @param[out] out_msg Non-null pointer to receive the null-terminated detail string pointer.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if `out_msg` is null.
 */
llvm_gen_be_error_t llvm_gen_be_get_last_error_detail(const char** out_msg) noexcept
{
  if (!out_msg) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  *out_msg = (be_state != nullptr) ? be_state->err_context.message : "";
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_LLVM_GEN_BE */
