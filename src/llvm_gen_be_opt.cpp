/**
 * @file llvm_gen_be_opt.cpp
 * @ingroup llvm_gen_be_driver
 * @brief Implementation of the LLVM Optimization Pipeline configuration and PassBuilder integration.
 * @details Provides functions for configuring the LLVM New Pass Manager,
 * specifying optimization levels, and assembling pass pipelines.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_opt.h"
#include <string.h>

#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Module.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/VirtualFileSystem.h>

#include <llvm/Transforms/Instrumentation/AddressSanitizer.h>
#include <llvm/Transforms/Instrumentation/ThreadSanitizer.h>
#include <llvm/Transforms/Instrumentation/MemorySanitizer.h>
#include <llvm/Transforms/Instrumentation/BoundsChecking.h>
#include <llvm/Transforms/Instrumentation/PGOInstrumentation.h>
#include <llvm/Transforms/Coroutines/CoroEarly.h>
#include <llvm/Transforms/Coroutines/CoroSplit.h>
#include <llvm/Transforms/Coroutines/CoroCleanup.h>
#include <llvm/Transforms/IPO/ThinLTOBitcodeWriter.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <type_traits>

#if BACK_END_IS_LLVM_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Parses an optimization level string into an options structure.
 * @details Translates "O0", "O1", "O2", "O3", "Os", "Oz" into llvm_opt_level_t and sets default flags.
 * @param[in] opt_str The optimization level string.
 * @param[out] out_opts Receives the configured optimization options on success.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if invalid.
 */
llvm_gen_be_error_t parse_opt_level_string(const char* opt_str, llvm_opt_options_t* out_opts) noexcept {
  if (!opt_str || !out_opts) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  out_opts->opt_level = llvm_opt_level_t::O0;
  out_opts->vectorize_loops = false;
  out_opts->vectorize_slp = false;
  out_opts->inlining_threshold = 0;
  out_opts->use_lto = false;
  out_opts->use_asan = false;
  out_opts->use_tsan = false;
  out_opts->use_ubsan = false;


  if (strcmp(opt_str, "O0") == 0) {
    out_opts->opt_level = llvm_opt_level_t::O0;
  } else if (strcmp(opt_str, "O1") == 0) {
    out_opts->opt_level = llvm_opt_level_t::O1;
    out_opts->vectorize_slp = true;
  } else if (strcmp(opt_str, "O2") == 0) {
    out_opts->opt_level = llvm_opt_level_t::O2;
    out_opts->vectorize_loops = true;
    out_opts->vectorize_slp = true;
  } else if (strcmp(opt_str, "O3") == 0) {
    out_opts->opt_level = llvm_opt_level_t::O3;
    out_opts->vectorize_loops = true;
    out_opts->vectorize_slp = true;
    out_opts->inlining_threshold = 275;
  } else if (strcmp(opt_str, "Os") == 0) {
    out_opts->opt_level = llvm_opt_level_t::Os;
    out_opts->vectorize_slp = true;
  } else if (strcmp(opt_str, "Oz") == 0) {
    out_opts->opt_level = llvm_opt_level_t::Oz;
  } else {
    return llvm_gen_be_error_t::invalid_argument;
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Adds PGO and sanitizer instrumentation passes at the start of the optimization pipeline.
 * @details Appends PGO generation/use passes and ASan, TSan, MSan, and UBSan passes onto @p out_mpm
 *          according to the flags in @p opts.
 * @param[in] opts Pointer to the optimization options configuration.
 * @param[in,out] out_mpm Pointer to the `llvm::ModulePassManager` to populate.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t add_pipeline_start_passes(
    const llvm_opt_options_t* opts,
    llvm::ModulePassManager* out_mpm) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  if (opts->pgo_generate) {
    out_mpm->addPass(llvm::PGOInstrumentationGen());
  } else if (opts->pgo_use_path) {
    out_mpm->addPass(llvm::PGOInstrumentationUse(opts->pgo_use_path));
  }
  if (opts->use_asan) {
    out_mpm->addPass(llvm::AddressSanitizerPass(llvm::AddressSanitizerOptions{}));
  }
  if (opts->use_tsan) {
    out_mpm->addPass(llvm::ModuleThreadSanitizerPass());
    out_mpm->addPass(llvm::createModuleToFunctionPassAdaptor(llvm::ThreadSanitizerPass()));
  }
  if (opts->use_msan) {
    out_mpm->addPass(llvm::MemorySanitizerPass(llvm::MemorySanitizerOptions{}));
  }
  if (opts->use_ubsan) {
#if LLVM_VERSION_MAJOR >= 20
    llvm::BoundsCheckingPass::Options ubsan_opts;
    ubsan_opts.Rt = llvm::BoundsCheckingPass::Options::Runtime(
        false, // MinRuntime: false uses full __ubsan_handle_out_of_bounds handlers
        false, // MayReturn: false for fatal checks
        false  // HandlerPreserveAllRegs
    );
    out_mpm->addPass(llvm::createModuleToFunctionPassAdaptor(llvm::BoundsCheckingPass(ubsan_opts)));
#else
    out_mpm->addPass(llvm::createModuleToFunctionPassAdaptor(llvm::BoundsCheckingPass()));
#endif
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Configures LLVM PGOOptions from the backend optimization options.
 * @details Populates @p out_pgo for IR instrumentation generation or profile use when
 *          enabled in @p opts, adapting to the LLVM version's `llvm::PGOOptions` constructor.
 * @param[in] opts Pointer to the optimization options configuration.
 * @param[out] out_pgo Pointer to the optional `llvm::PGOOptions` to populate.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t configure_pgo_options(
    const llvm_opt_options_t* opts,
    std::optional<llvm::PGOOptions>* out_pgo) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  extern a_const_char* fprofile_generate_path;
  const char* path = nullptr;
  llvm::PGOOptions::PGOAction action = llvm::PGOOptions::NoAction;
  if (opts->pgo_generate) {
    path = fprofile_generate_path ? fprofile_generate_path : "default.profraw";
    action = llvm::PGOOptions::IRInstr;
  } else if (opts->pgo_use_path) {
    path = opts->pgo_use_path;
    action = llvm::PGOOptions::IRUse;
  }
  if (path) {
#if LLVM_VERSION_MAJOR >= 20
    out_pgo->emplace(path, "", "", "", action);
#elif LLVM_VERSION_MAJOR >= 17
    out_pgo->emplace(path, "", "", "", llvm::vfs::getRealFileSystem(), action);
#else
    out_pgo->emplace(path, "", "", llvm::vfs::getRealFileSystem(), action);
#endif
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Executes the LLVM PassBuilder optimization pipeline on a Module.
 * @details Constructs PassManagers, adds sanitizer passes, PGO, LTO, and runs module passes.
 * @param[in,out] module The LLVM Module to optimize.
 * @param[in] opts The optimization configuration options.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t run_optimization_pipeline(llvm::Module* module, const llvm_opt_options_t* opts) noexcept {
  if (!module || !opts) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  for (llvm::Function& fn : *module) {
    if (!fn.isDeclaration()) {
      if (opts->use_asan) fn.addFnAttr(llvm::Attribute::SanitizeAddress);
      if (opts->use_tsan) fn.addFnAttr(llvm::Attribute::SanitizeThread);
      if (opts->use_msan) fn.addFnAttr(llvm::Attribute::SanitizeMemory);
    }
  }

  llvm::OptimizationLevel llvm_opt_level;
  switch (opts->opt_level) {
    case llvm_opt_level_t::O0: llvm_opt_level = llvm::OptimizationLevel::O0; break;
    case llvm_opt_level_t::O1: llvm_opt_level = llvm::OptimizationLevel::O1; break;
    case llvm_opt_level_t::O2: llvm_opt_level = llvm::OptimizationLevel::O2; break;
    case llvm_opt_level_t::O3: llvm_opt_level = llvm::OptimizationLevel::O3; break;
    case llvm_opt_level_t::Os:
      llvm_opt_level = llvm::OptimizationLevel::O2;
      for (llvm::Function& fn : *module) {
        if (!fn.isDeclaration()) fn.addFnAttr(llvm::Attribute::OptimizeForSize);
      }
      break;
    case llvm_opt_level_t::Oz:
      llvm_opt_level = llvm::OptimizationLevel::O2;
      for (llvm::Function& fn : *module) {
        if (!fn.isDeclaration()) {
          fn.addFnAttr(llvm::Attribute::OptimizeForSize);
          fn.addFnAttr(llvm::Attribute::MinSize);
        }
      }
      break;
    default: llvm_opt_level = llvm::OptimizationLevel::O0; break;
  }

  llvm::LoopAnalysisManager lam;
  llvm::FunctionAnalysisManager fam;
  llvm::CGSCCAnalysisManager cgam;
  llvm::ModuleAnalysisManager mam;

  llvm::PassInstrumentationCallbacks pic;
  llvm::StandardInstrumentations si{module->getContext(), /*DebugLogging=*/false};
#if LLVM_VERSION_MAJOR >= 17
  si.registerCallbacks(pic, &mam);
#else
  si.registerCallbacks(pic, &fam);
#endif

  llvm::PipelineTuningOptions pto;
  pto.LoopVectorization = opts->vectorize_loops;
  pto.SLPVectorization = opts->vectorize_slp;

  std::optional<llvm::PGOOptions> pgo_opts;
  LLVM_GEN_BE_CHECK(configure_pgo_options(opts, &pgo_opts));

  llvm::PassBuilder pb{nullptr, pto, pgo_opts, &pic};

  pb.registerModuleAnalyses(mam);
  pb.registerCGSCCAnalyses(cgam);
  pb.registerFunctionAnalyses(fam);
  pb.registerLoopAnalyses(lam);
  pb.crossRegisterProxies(lam, fam, cgam, mam);

  llvm::ModulePassManager mpm;
  LLVM_GEN_BE_CHECK(add_pipeline_start_passes(opts, &mpm));
  if (llvm_opt_level == llvm::OptimizationLevel::O0) {
    if (module->getFunction("llvm.coro.id") != nullptr) {
      mpm.addPass(llvm::CoroEarlyPass());
      mpm.addPass(llvm::createModuleToPostOrderCGSCCPassAdaptor(llvm::CoroSplitPass()));
      mpm.addPass(llvm::CoroCleanupPass());
    }
    mpm.addPass(pb.buildO0DefaultPipeline(llvm_opt_level));
  } else {
    if (opts->use_lto) {
      mpm.addPass(pb.buildThinLTOPreLinkDefaultPipeline(llvm_opt_level));
    } else {
      mpm.addPass(pb.buildPerModuleDefaultPipeline(llvm_opt_level));
    }
  }

  mpm.run(*module, mam);

  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_LLVM_GEN_BE */
