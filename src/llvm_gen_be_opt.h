/**
 * @file llvm_gen_be_opt.h
 * @ingroup llvm_gen_be_driver
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
 * @details Controls optimization levels, vectorization, sanitizers, LTO, and PGO settings.
 */
struct llvm_opt_options_t {
  llvm_opt_level_t opt_level;  ///< The target optimization level.
  bool vectorize_loops;        ///< True to enable loop vectorization.
  bool vectorize_slp;          ///< True to enable SLP vectorization.
  unsigned inlining_threshold; ///< Threshold for function inlining.
  bool use_lto;                ///< True if Link Time Optimization (LTO) is enabled.
  bool use_asan;               ///< True if AddressSanitizer (ASan) is enabled.
  bool use_msan;               ///< True if MemorySanitizer (MSan) is enabled.
  bool use_tsan;               ///< True if ThreadSanitizer (TSan) is enabled.
  bool use_ubsan;              ///< True if UndefinedBehaviorSanitizer (UBSan) is enabled.
  bool pgo_generate;           ///< True if generating PGO profiles.
  const char* pgo_use_path;    ///< Path to PGO profile data, or null.
};

/**
 * @brief Parses an optimization level string into an options structure.
 * @details Translates "O0", "O1", "O2", "O3", "Os", "Oz" into llvm_opt_level_t and sets default flags.
 * @param[in] opt_str The optimization level string.
 * @param[out] out_opts Receives the configured optimization options on success.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if invalid.
 */
llvm_gen_be_error_t parse_opt_level_string(const char* opt_str, llvm_opt_options_t* out_opts) noexcept;

/**
 * @brief Executes the LLVM PassBuilder optimization pipeline on a Module.
 * @details Constructs PassManagers, adds sanitizer passes, PGO, LTO, and runs module passes.
 * @param[in,out] module The LLVM Module to optimize.
 * @param[in] opts The optimization configuration options.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t run_optimization_pipeline(llvm::Module* module, const llvm_opt_options_t* opts) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_OPT_H */
