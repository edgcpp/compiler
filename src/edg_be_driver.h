/**
 * @file edg_be_driver.h
 * @brief GCC/Clang-compatible CLI driver translator, response-file expander, and depfile writer.
 * @details Translates single- and double-hyphen GCC/Clang/CMake/Meson/Autotools command-line
 * invocations into EDG front-end arguments plus normalized `edg_be_invocation_options_t`,
 * handles `@rspfile` expansion, executes build-system introspection queries, and writes
 * Make/Ninja dependency files (`-MD` / `-MMD` / `-MF` / `-MT` / `-MQ` / `-MP`).
 * @ingroup edg_be_driver
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef EDG_BE_DRIVER_H
#define EDG_BE_DRIVER_H 1

#include "edg_be_common.h"
#include "edg_be_sdk.h"
#include <cstdio>

/**
 * @defgroup edg_be_driver GCC/Clang CLI Driver & Dependency Writer
 * @ingroup edg_be_common
 * @brief Drop-in GCC/Clang command-line translation, `@rspfile` expansion, and Make depfiles.
 * @details Translates build-system flags into EDG options and `edg_be_invocation_options_t`.
 * @{
 */

/**
 * @def EDG_BE_DRIVER_MAX_ARGS
 * @brief Maximum number of expanded CLI tokens and synthesized EDG arguments per invocation.
 * @details Inline storage capacity used by `edg_be_translated_command_t`.
 */
#define EDG_BE_DRIVER_MAX_ARGS 2048U

/**
 * @def EDG_BE_DRIVER_ARENA_CAPACITY
 * @brief Inline character arena byte capacity for synthesized CLI flags and paths.
 * @details Eliminates heap allocations and ensures deterministic cleanup and OOM testing.
 */
#define EDG_BE_DRIVER_ARENA_CAPACITY 131072U

/**
 * @def EDG_BE_DRIVER_MAX_LIST_ITEMS
 * @brief Maximum number of entries per option pointer list (sources, objects, `-L`, `-l`, `-Wl`).
 * @details Inline pointer-list capacity inside `edg_be_translated_command_t`.
 */
#define EDG_BE_DRIVER_MAX_LIST_ITEMS 512U

/**
 * @def EDG_BE_DRIVER_MAX_RSP_DEPTH
 * @brief Maximum nested `@rspfile` inclusion depth before reporting recursion error.
 * @details Guards against self-referential `@a.rsp` -> `@a.rsp` response-file cycles.
 */
#define EDG_BE_DRIVER_MAX_RSP_DEPTH 8U

BEGIN_EDG_NAMESPACE

/**
 * @struct edg_be_translated_command_t
 * @brief Self-contained arena and result container for a translated compiler command line.
 * @details Holds the normalized `edg_be_invocation_options_t`, discovered `edg_be_sdk_result_t`,
 * expanded raw tokens, and synthesized `edg_argv` vector suitable for `proc_command_line`.
 */
struct edg_be_translated_command_t {
  edg_be_invocation_options_t options;                       ///< Normalized backend-agnostic invocation options.
  edg_be_sdk_result_t sdk_info;                              ///< Discovered host/target SDK and STL paths.
  bool sdk_discovered;                                       ///< True when `sdk_info` was populated.
  bool native_edg_mode;                                      ///< True when explicit EDG flags bypassed SDK probing.
  size_t arena_used;                                         ///< Bytes currently allocated inside `arena`.
  size_t arena_limit;                                        ///< Effective byte cap (<= EDG_BE_DRIVER_ARENA_CAPACITY).
  size_t args_limit;                                         ///< Effective token cap (<= EDG_BE_DRIVER_MAX_ARGS).
  size_t list_limit;                                         ///< Effective per-list cap (<= EDG_BE_DRIVER_MAX_LIST_ITEMS).
  char arena[EDG_BE_DRIVER_ARENA_CAPACITY];                  ///< Inline character storage for strings.
  const char* expanded_tokens[EDG_BE_DRIVER_MAX_ARGS];       ///< Tokens after `@rspfile` expansion.
  size_t num_expanded_tokens;                                ///< Number of tokens in `expanded_tokens`.
  char* edg_argv[EDG_BE_DRIVER_MAX_ARGS];                    ///< Synthesized mutable argv for `proc_command_line`.
  int edg_argc;                                              ///< Argument count in `edg_argv`.
  const char* dep_targets_buf[EDG_BE_DRIVER_MAX_LIST_ITEMS]; ///< Storage backing `options.dep_targets`.
  bool dep_target_is_quoted[EDG_BE_DRIVER_MAX_LIST_ITEMS];   ///< True when entry came from `-MQ` (already escaped).
  const char* sources_buf[EDG_BE_DRIVER_MAX_LIST_ITEMS];     ///< Storage backing `options.input_source_files`.
  const char* objects_buf[EDG_BE_DRIVER_MAX_LIST_ITEMS];     ///< Storage backing `options.input_object_files`.
  const char* lib_dirs_buf[EDG_BE_DRIVER_MAX_LIST_ITEMS];    ///< Storage backing `options.library_search_dirs`.
  const char* libs_buf[EDG_BE_DRIVER_MAX_LIST_ITEMS];        ///< Storage backing `options.libraries`.
  const char* linker_args_buf[EDG_BE_DRIVER_MAX_LIST_ITEMS]; ///< Storage backing `options.linker_passthrough_args`.
  const char* rpaths_buf[EDG_BE_DRIVER_MAX_LIST_ITEMS];      ///< Storage backing `options.rpath_dirs`.
  const char* extra_be_buf[EDG_BE_DRIVER_MAX_LIST_ITEMS];    ///< Storage backing `options.extra_backend_args`.
};

/**
 * @brief Initializes an `edg_be_translated_command_t` structure to empty defaults.
 * @details Resets the string arena, pointer lists, and `options` baseline.
 * @param[out] out_cmd Pointer to the translated command container to initialize.
 * @return `edg_be_driver_error_t::ok` on success, or `invalid_argument` if `out_cmd` is null.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_translated_command_init(
    edg_be_translated_command_t* out_cmd) EDG_BE_NOEXCEPT;

/**
 * @brief Expands `@rspfile` response-file arguments into `cmd->expanded_tokens`.
 * @details Supports single/double quotes, backslash escaping, `#` comments, and nested `@file`
 * inclusions up to `EDG_BE_DRIVER_MAX_RSP_DEPTH`.
 * @param[in] argc Argument count of raw `argv`.
 * @param[in] argv Raw command-line argument array (`argv[0]` is program name).
 * @param[in,out] cmd Initialized translated command structure receiving `expanded_tokens`.
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument` on null/invalid args,
 * `response_file_io_error` on missing/unreadable/cyclic `@file`, or `out_of_memory`.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_expand_response_files(
    int argc,
    const char* const* argv,
    edg_be_translated_command_t* cmd) EDG_BE_NOEXCEPT;

/**
 * @brief Translates a GCC/Clang or EDG command line into `edg_argv` and `edg_be_invocation_options_t`.
 * @details Recognizes GCC/Clang compilation, include, standard, dependency, optimization, debug,
 * relocation, sanitizer, warning, query, and linker flags alongside native EDG `--*` options,
 * and auto-discovers host/target system headers when `sdk_env` is provided.
 * @param[in] argc Raw command-line argument count.
 * @param[in] argv Raw command-line argument vector.
 * @param[in] sdk_env Optional SDK probe environment (pass nullptr to skip auto-SDK injection).
 * @param[out] out_cmd Pointer to receive the translated EDG argv and normalized options.
 * @return `edg_be_driver_error_t::ok` on success, or a specific `edg_be_driver_error_t` code.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_translate_command_line(
    int argc,
    const char* const* argv,
    const edg_be_sdk_probe_env_t* sdk_env,
    edg_be_translated_command_t* out_cmd) EDG_BE_NOEXCEPT;

/**
 * @brief Translates a command line using explicit arena, argument, and list capacity caps.
 * @details Identical to `edg_be_translate_command_line` except non-zero limits clamp internal
 * capacities for deterministic resource-bound enforcement and fault-injection testing.
 * @param[in] argc Raw command-line argument count.
 * @param[in] argv Raw command-line argument vector.
 * @param[in] sdk_env Optional SDK probe environment (pass nullptr to skip auto-SDK injection).
 * @param[in] arena_limit Maximum arena bytes (`0` uses `EDG_BE_DRIVER_ARENA_CAPACITY`).
 * @param[in] args_limit Maximum token/argv slots (`0` uses `EDG_BE_DRIVER_MAX_ARGS`).
 * @param[in] list_limit Maximum entries per option list (`0` uses `EDG_BE_DRIVER_MAX_LIST_ITEMS`).
 * @param[out] out_cmd Pointer to receive the translated EDG argv and normalized options.
 * @return `edg_be_driver_error_t::ok` on success, or a specific `edg_be_driver_error_t` code.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_translate_command_line_bounded(
    int argc,
    const char* const* argv,
    const edg_be_sdk_probe_env_t* sdk_env,
    size_t arena_limit,
    size_t args_limit,
    size_t list_limit,
    edg_be_translated_command_t* out_cmd) EDG_BE_NOEXCEPT;

/**
 * @brief Executes an immediate build-system introspection query (`--version`, `-dumpmachine`, etc.).
 * @details Writes GCC/Clang-compatible query output to `out_stream` so CMake, Meson, and Autotools
 * compiler detection probes succeed without requiring a dummy source file.
 * @param[in] opts Normalized invocation options with `query_kind != edg_driver_query_kind_t::none`.
 * @param[in] sdk_info Optional discovered SDK metadata (for target triple and resource dir).
 * @param[in,out] out_stream Writable `FILE*` stream (e.g., `stdout`).
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument` on null/none query, or `io_error`.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_execute_driver_query(
    const edg_be_invocation_options_t* opts,
    const edg_be_sdk_result_t* sdk_info,
    FILE* out_stream) EDG_BE_NOEXCEPT;

/**
 * @brief Derives the POSIX-standard current-working-directory object filename (`<stem>.o`).
 * @details Strips all leading `/` and backslash directory components and replaces the final extension
 * with `.o` (or `.obj` when `use_obj_ext` is true), preventing writes into read-only source trees.
 * @param[in] source_path Input source path (e.g., `"/ro/src/foo.cpp"`).
 * @param[in] use_obj_ext When true, appends `".obj"`; otherwise appends `".o"`.
 * @param[out] out_buf Destination buffer for derived object filename.
 * @param[in] buf_cap Byte capacity of `out_buf`.
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument` on null/empty input, or `out_of_memory`.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_derive_cwd_object_name(
    const char* source_path,
    bool use_obj_ext,
    char* out_buf,
    size_t buf_cap) EDG_BE_NOEXCEPT;

/**
 * @brief Formats Make dependency rules into a caller-supplied character buffer.
 * @details Escapes spaces (`\ `), `#` (`\#`), and `$` (`$$`), honors `-MQ` pre-quoted targets,
 * and appends `-MP` phony header rules when enabled.
 * @param[in] cmd Translated command holding `options` and `dep_target_is_quoted` metadata.
 * @param[in] primary_source Path to the primary source file being compiled.
 * @param[in] included_files Array of header file paths included during preprocessing.
 * @param[in] num_included_files Number of entries in `included_files`.
 * @param[out] out_buf Destination character buffer.
 * @param[in] buf_cap Byte capacity of `out_buf`.
 * @param[out] out_len Receives the number of formatted bytes written (excluding null terminator).
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument` on null args, or `out_of_memory`.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_format_depfile_buffer(
    const edg_be_translated_command_t* cmd,
    const char* primary_source,
    const char* const* included_files,
    size_t num_included_files,
    char* out_buf,
    size_t buf_cap,
    size_t* out_len) EDG_BE_NOEXCEPT;

/**
 * @brief Formats and writes Make dependency rules into an already-open `FILE*` stream.
 * @details Validates stream write completion and flushes `out_stream`.
 * @param[in] cmd Translated command holding `options` and `dep_target_is_quoted` metadata.
 * @param[in] primary_source Path to the primary source file being compiled.
 * @param[in] included_files Array of header file paths included during preprocessing.
 * @param[in] num_included_files Number of entries in `included_files`.
 * @param[in,out] out_stream Open writable `FILE*` stream.
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument`, `out_of_memory`, or `depfile_write_failed`.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_write_depfile_stream(
    const edg_be_translated_command_t* cmd,
    const char* primary_source,
    const char* const* included_files,
    size_t num_included_files,
    FILE* out_stream) EDG_BE_NOEXCEPT;

/**
 * @brief Formats and writes a GNU Make / Ninja dependency file (`-MD`, `-MMD`, `-MF`, `-MT`, `-MQ`, `-MP`).
 * @details Escapes spaces (`\ `), `#` (`\#`), and `$` (`$$`) in Make target and prerequisite paths,
 * honors `-MQ` pre-quoted targets, and emits phony header rules when `opts->dep_phony_targets` is set.
 * @param[in] cmd Translated command holding `options` and `dep_target_is_quoted` metadata.
 * @param[in] primary_source Path to the primary source file being compiled.
 * @param[in] included_files Array of header file paths included during preprocessing.
 * @param[in] num_included_files Number of entries in `included_files`.
 * @return `edg_be_driver_error_t::ok` on success, `invalid_argument` on null args, or `depfile_write_failed`.
 */
EDG_BE_NODISCARD edg_be_driver_error_t edg_be_write_depfile(
    const edg_be_translated_command_t* cmd,
    const char* primary_source,
    const char* const* included_files,
    size_t num_included_files) EDG_BE_NOEXCEPT;

END_EDG_NAMESPACE

/** @} */

#endif /* EDG_BE_DRIVER_H */
