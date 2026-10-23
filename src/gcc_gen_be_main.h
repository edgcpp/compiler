/**
 * @file gcc_gen_be_main.h
 * @brief Main entry points for the GCC backend.
 *
 * This file declares the primary entry points (`gcc_gen_be` and `back_end`)
 * that interface directly with the EDG frontend's execution flow.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_MAIN_H
#define GCC_GEN_BE_MAIN_H

#include "gcc_gen_be_error.h"
#include "basic_hdrs.h"

struct gcc_jit_context;
struct gcc_jit_function;

BEGIN_EDG_NAMESPACE

/**
 * @brief Early initialization for the GCC backend.
 *
 * This function is called by the front end before command-line options
 * are processed.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_early_init(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Main entry point for the GCC backend.
 *
 * It orchestrates the translation of the EDG AST into libgccjit constructs
 * and triggers compilation.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Hook called by the EDG frontend to execute the backend.
 *
 * This is the standard entry point expected by the EDG frontend architecture.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t back_end(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Parses and applies command-line options to the given context.
 *
 * @param ctx The libgccjit context.
 * @param argc Number of arguments.
 * @param argv Array of arguments.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_apply_command_line_options(struct gcc_jit_context *ctx, int argc, char **argv) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Manages Multi-TU compilation by isolating contexts.
 *
 * @param source_files Array of source files to process.
 * @param num_files Number of source files.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_compile_multi_tu(const char **source_files, int num_files, int argc, char **argv) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Links multiple per-TU object artifacts into a final executable or shared library.
 * @details Propagates `-flto`, `-shared`, `-static`, and driver options to a linker `gcc_jit_context`
 * or validates all input object files and output paths.
 * @param[in] obj_files Array of object file paths.
 * @param[in] num_objs Number of object files.
 * @param[in] output_path Target output binary or shared library path.
 * @param[in] argc Number of command-line arguments.
 * @param[in] argv Command-line argument vector.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_link_multi_tu_artifacts(
    const char **obj_files,
    int num_objs,
    const char *output_path,
    int argc,
    char **argv) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Links multiple per-TU object files into a final binary or shared library and reports link status.
 * @details Validates `obj_files`, `output_path`, and `out_link_status`, propagates `-flto`, `-shared`,
 * `-pie`, `-static`, `-pthread`, `-L*`, `-l*`, and `-Wl,*` flags via `gcc_jit_context_add_driver_option`,
 * invokes `gcc_gen_be_link_multi_tu_artifacts`, and writes `0` to `*out_link_status` on success (`-1` on failure).
 * @param[in] obj_files Array of object file paths.
 * @param[in] num_objs Number of object files.
 * @param[in] output_path Target output binary or shared library path.
 * @param[in] argc Number of command-line arguments.
 * @param[in] argv Command-line argument vector.
 * @param[out] out_link_status Pointer to receive `0` on link success, or `-1` on failure.
 * @return `GCC_GEN_BE_SUCCESS` on success, `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_link_status` is NULL,
 * or `GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED`.
 */
extern gcc_gen_be_error_t gcc_gen_be_link_multi_tu_objects(
    const char **obj_files,
    int num_objs,
    const char *output_path,
    int argc,
    char **argv,
    int *out_link_status) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits the final output artifact (assembler, object, shared library, executable).
 *
 * @param ctx The context.
 * @param output_kind Target kind string (e.g. "asm", "obj", "dll", "exe").
 * @param output_path Target file path.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_artifact(struct gcc_jit_context *ctx, const char *output_kind, const char *output_path) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Synthesizes an Itanium ABI C++20 module initializer function (`_ZGIW<module_name>`).
 * @details Emits an exported module initializer function guarded by an internal idempotency boolean
 * flag (`_ZGIW<module_name>__guard`) so imported module initializers execute at most once.
 * @param[in] module_name The C++20 module name (e.g., `"math"` or `"std.core"`).
 * @param[out] out_init_fn Pointer to receive the synthesized `_ZGIW` `gcc_jit_function`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_module_initializer(
    const char *module_name,
    struct gcc_jit_function **out_init_fn) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_MAIN_H */