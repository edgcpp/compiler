/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

llvm_gen_be.h - Declarations related to llvm_gen_be.cpp (LLVM IR-generating back end)

*/

/* Avoid including these declarations more than once: */
#ifndef LLVM_GEN_BE_H
#define LLVM_GEN_BE_H 1

/**
 * @defgroup llvm_gen_be EDG LLVM IR Generation Backend
 * @brief Top-level module for the EDG LLVM IR generation backend.
 */

/**
 * @defgroup llvm_gen_be_driver Driver, Orchestrator & Codegen
 * @ingroup llvm_gen_be
 * @brief Driver, translation-unit orchestrator, optimization pipeline, and target codegen.
 */

/**
 * @file llvm_gen_be.h
 * @ingroup llvm_gen_be_driver
 * @brief Interface for the EDG LLVM generation backend driver.
 * @details Declares the entry point for lowering EDG IL into LLVM IR, writing
 * to output files, and managing backend state cleanup.
 */

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

#ifndef LLVM_GEN_BE_ERROR_H
#include "llvm_gen_be_error.h"
#endif /* ifndef LLVM_GEN_BE_ERROR_H */

#ifndef EDG_BE_COMMON_H
#include "edg_be_common.h"
#endif /* ifndef EDG_BE_COMMON_H */

#if BACK_END_IS_LLVM_GEN_BE

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if !STANDALONE_UTILITY_PROGRAM
/**
 * @brief Entry point invoked by the EDG front end to execute the LLVM backend.
 * @details Reads serialized IL if necessary and invokes `llvm_gen_be()`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
extern llvm_gen_be_error_t back_end(void) noexcept;
#endif /* !STANDALONE_UTILITY_PROGRAM */

#if MAKE_FRONT_END_CALLABLE
/**
 * @brief Cleans up internal state and resources allocated by the LLVM backend.
 * @details Resets all LLVM handles, symbol caches, and cleanup stacks, and deletes `be_state`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
extern llvm_gen_be_error_t llvm_gen_be_cleanup(void) noexcept;
#endif /* MAKE_FRONT_END_CALLABLE */

/**
 * @brief Synchronizes EDG front-end target sizing, alignment, and endianness globals with an LLVM target triple.
 * @details Normalizes @p triple_str and updates `targ_little_endian`, `targ_has_signed_chars`,
 *          `targ_sizeof_*`, `targ_alignof_*`, `targ_size_t_int_kind`, `targ_ptrdiff_t_int_kind`,
 *          `targ_wchar_t_int_kind`, and `targ_wint_t_int_kind` for LP64, LLP64, and ILP32 targets.
 * @param[in] triple_str Null-terminated LLVM target triple string (e.g., `"x86_64-pc-windows-msvc"`).
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p triple_str is null or empty.
 */
extern llvm_gen_be_error_t llvm_sync_fe_target_configuration(const char* triple_str) noexcept;

/**
 * @brief Finalizes multi-translation-unit linking or LTO after all source files have been compiled.
 * @details Invokes `run_thinlto_combined_index_and_link` or `llvm_link_executables` on accumulated
 *          per-TU artifacts in `be_state->extra_linker_inputs` when `ofile_name` is set, and cleans up
 *          temporary intermediate object/bitcode files.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
extern llvm_gen_be_error_t llvm_finalize_multi_tu_link(void) noexcept;

/**
 * @brief Configures an LLVM backend CLI option on the active `LLVMBackendState`.
 * @details Allocates `be_state` on demand and updates optimization level, debug info,
 *          target triple/CPU/features, relocation/code models, OpenMP, or linker flags.
 * @param[in] key The CLI option category key (e.g., `"opt_level"`, `"debug_info"`, `"target"`).
 * @param[in] val The CLI option value string.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p key or @p val is null or unrecognized.
 */
extern llvm_gen_be_error_t llvm_configure_cli_option(const char* key, const char* val) noexcept;

/**
 * @brief Applies normalized Stage-0 driver invocation options to the active LLVM backend state.
 * @details Validates @p opts, initializes `be_state` on demand, and synchronizes optimization,
 *          debug level, relocation/LTO modes, target triple/CPU/features, OpenMP, sanitizers,
 *          and linker arguments without coupling to front-end CLI globals.
 * @param[in] opts Pointer to the normalized invocation options structure.
 * @return llvm_gen_be_error_t::ok on success, `invalid_argument` if @p opts is null, or `out_of_memory`.
 */
extern llvm_gen_be_error_t llvm_apply_invocation_options(
    const edg_be_invocation_options_t* opts) noexcept;

/**
 * @brief Primary entry point for the EDG LLVM IR generating backend.
 * @details Translates the EDG IL translation unit into LLVM IR, runs passes, and writes output files.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
extern llvm_gen_be_error_t llvm_gen_be(void) noexcept;

/* Conditionally close the "edg" namespace. */
/**
 * @brief Retrieves the human-readable detail message recorded for the most recent LLVM backend error.
 * @details Returns a pointer to the internal error context buffer (or empty string if none).
 * @param[out] out_msg Non-null pointer to receive the detail string pointer.
 * @return llvm_gen_be_error_t::ok on success, or invalid_argument if out_msg is null.
 */
llvm_gen_be_error_t llvm_gen_be_get_last_error_detail(const char** out_msg) noexcept;

END_EDG_NAMESPACE

#endif /* BACK_END_IS_LLVM_GEN_BE */

#endif /* ifndef LLVM_GEN_BE_H */