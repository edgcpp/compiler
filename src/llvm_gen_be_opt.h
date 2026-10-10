/**
 * @file llvm_gen_be_opt.h
 * @brief LLVM Optimization Pipeline configuration and PassBuilder integration.
 * @details Provides data structures and functions for configuring the LLVM New Pass Manager,
 * specifying optimization levels, and assembling pass pipelines.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_OPT_H
#define LLVM_GEN_BE_OPT_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"

namespace llvm {
  class Module;
}

BEGIN_EDG_NAMESPACE

/**
 * @enum llvm_opt_level_t
 * @brief Specifies the optimization level for the LLVM PassBuilder pipeline.
 */
enum class llvm_opt_level_t {
  O0, ///< No optimization.
  O1, ///< Optimize quickly without destroying debuggability.
  O2, ///< Optimize for execution speed.
  O3, ///< Optimize aggressively for execution speed (may increase code size).
  Os, ///< Optimize for code size.
  Oz  ///< Optimize aggressively for code size.
};

/**
 * @struct llvm_opt_options_t
 * @brief Encapsulates optimization configuration options.
 */
struct llvm_opt_options_t {
  llvm_opt_level_t opt_level; ///< The target optimization level.
  bool vectorize_loops;       ///< True to enable loop vectorization.
  bool vectorize_slp;         ///< True to enable SLP vectorization.
  unsigned inlining_threshold;///< Threshold for function inlining.
  bool use_lto;
  bool use_asan;

  bool use_tsan;
  bool use_ubsan;
  bool pgo_generate;          ///< True if generating PGO profiles.
  const char* pgo_use_path;   ///< Path to PGO profile data, or null.
               ///< True if Link Time Optimization (LTO) is enabled.
};

/**
 * @brief Parses an optimization level string (e.g., "O0", "O3", "Os") into an options structure.
 * @details Initializes the out_opts structure with the parsed optimization level and default options for that level.
 * @param[in] opt_str The optimization level string (e.g., "O0", "O1", "O2", "O3", "Os", "Oz").
 * @param[out] out_opts Pointer to the optimization options structure to populate.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if the string is invalid or out_opts is null.
 */
llvm_gen_be_error_t parse_opt_level_string(const char* opt_str, llvm_opt_options_t* out_opts) noexcept;

/**
 * @brief Executes the LLVM optimization pipeline on the given module.
 * @details Instantiates the New Pass Manager (PassBuilder, AnalysisManagers), constructs the default pass pipeline for the specified options, and runs it on the module.
 * @param[in,out] module The LLVM Module to optimize.
 * @param[in] opts The optimization options to configure the pipeline.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::pass_pipeline_failure if execution fails.
 */
llvm_gen_be_error_t run_optimization_pipeline(llvm::Module* module, const llvm_opt_options_t* opts) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_OPT_H */
