/**
 * @file llvm_gen_be_error.h
 * @ingroup llvm_gen_be_driver
 * @brief Centralized error reporting and propagation subsystem for LLVM backend.
 * @details Defines the centralized error enum, diagnostic context structure,
 * and reporting functions used across all LLVM generation backend components.
 * Every backend function returns llvm_gen_be_error_t and passes computed results
 * via output pointers or references.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_ERROR_H
#define LLVM_GEN_BE_ERROR_H 1

#include "basic_hdrs.h"
#include <cstddef>
#include <cstdint>
#include <new>

BEGIN_EDG_NAMESPACE

/**
 * @enum llvm_gen_be_error_t
 * @brief Centralized error status codes for the LLVM generation backend.
 * @details Every function in the LLVM backend returns this enumeration.
 * Decorated with [[nodiscard]] directly on the typedef/type definition to enforce
 * mandatory error inspection and bubbling without needing [[nodiscard]] on every function.
 */
typedef enum class [[nodiscard]] llvm_gen_be_error_t : int32_t {
  ok = 0,                        ///< Operation completed successfully without error.
  invalid_argument = 1,          ///< A null pointer or invalid argument was supplied.
  out_of_memory = 2,             ///< Dynamic heap allocation failed.
  unsupported_type = 3,          ///< Encountered an unsupported or unhandled EDG type kind.
  unsupported_expr = 4,          ///< Encountered an unsupported or unhandled EDG expression operator.
  unsupported_stmt = 5,          ///< Encountered an unsupported or unhandled EDG statement kind.
  abi_classification_failed = 6, ///< Target ABI argument or return classification failed.
  di_metadata_failure = 7,       ///< Debug information builder failure or invalid debug metadata.
  pass_pipeline_failure = 8,     ///< LLVM PassBuilder pipeline assembly or execution failure.
  code_gen_failure = 9,          ///< TargetMachine code generation or emission failure.
  verification_failure = 10,     ///< LLVM Module or Function verifier identified invalid IR.
  io_error = 11,                 ///< File stream opening, writing, or flushing failure.
  constant_eval_failure = 12,    ///< Constant evaluation failure.
  symbol_lookup_failure = 13,    ///< Symbol resolution or lookup failure.
  eh_lowering_failure = 14,      ///< Exception handling lowering failure.
  vtable_generation_failure = 15,///< Virtual table generation or layout failure.
  coroutine_lowering_failure = 16,///< Coroutine lowering or transformation failure.
  inline_asm_failure = 17,       ///< Inline assembly translation failure.
  internal_inconsistency = 18,   ///< Internal compiler inconsistency or invariant violation.
  not_implemented = 19,          ///< Feature not implemented.
  omp_lowering_failure = 20,     ///< OpenMP directive or construct lowering failure.
  offload_bundling_failed = 21,  ///< OpenMP target device image bundling or embedding failure.
  lto_indexing_failed = 22,      ///< ThinLTO/FullLTO summary index generation or merging failure.
  module_bmi_corrupt = 23,       ///< C++20 module BMI or initializer metadata is corrupt or invalid.
  dwarf_macro_emission_failed = 24, ///< DWARF v5 macro descriptor emission failure.
  coro_frame_layout_failed = 25, ///< C++20 coroutine frame layout or alignment failure.
  null_pointer = 26,             ///< A required pointer argument or internal reference was null.
  type_lowering_failed = 27,     ///< Lowering an EDG type to an LLVM type failed.
  coro_lowering_failed = 28,     ///< Lowering a C++20 coroutine construct failed.
  omp_lowering_failed = 29,      ///< Lowering an OpenMP construct or clause failed.
  eh_lowering_failed = 30,       ///< Lowering an exception handling construct failed.
  rtti_vtable_failed = 31,       ///< Lowering RTTI or virtual table structures failed.
  debug_info_failed = 32,        ///< Lowering DWARF or CodeView debug metadata failed.
  opt_pipeline_failed = 33,      ///< Configuring or running the LLVM optimization pipeline failed.
  codegen_emit_failed = 34,      ///< Emitting machine code or bitcode from the module failed.
  linker_invocation_failed = 35, ///< Invoking the target linker or LTO driver failed.
  gpu_offload_failed = 36,       ///< GPU device offloading or kernel lowering failed.
  module_io_failed = 37,         ///< Reading or writing module bitcode or IR files failed.
  verification_failed = 38,      ///< Module or state verification failed.
  internal_error = 39,           ///< Unspecified internal backend error.
  builtin_lowering_failed = 40,  ///< Target or GNU/Clang builtin lowering failed or had invalid operand types.
  atomic_lowering_failed = 41,   ///< C11/C++11 atomic operation or libatomic call lowering failed.
  complex_lowering_failed = 42,  ///< _Complex arithmetic or ABI coercion failed.
  vla_lowering_failed = 43,      ///< Variably-modified / VLA stack allocation or sizeof calculation failed.
  bitfield_access_failed = 44    ///< Bitfield load, store, or constant bit-packing failed.
} llvm_gen_be_error_t; ///< Typedef alias for the centralized `llvm_gen_be_error_t` error status enumeration.

/**
 * @brief Deterministic OOM fault-injection countdown (`-1` disabled, `0` fail next allocation, `>0` decrement).
 */
inline int32_t g_llvm_be_oom_countdown = -1;

/**
 * @brief Advances the deterministic OOM allocation budget for fault-injection testing.
 * @details Returns `llvm_gen_be_error_t::out_of_memory` and resets the countdown to `-1` when
 *          `g_llvm_be_oom_countdown == 0`; decrements positive countdowns and returns `ok` otherwise.
 * @return `llvm_gen_be_error_t::ok` if allocation may proceed, or `out_of_memory` on injected exhaustion.
 */
inline llvm_gen_be_error_t llvm_gen_be_step_oom_budget(void) noexcept {
  if (g_llvm_be_oom_countdown == 0) {
    g_llvm_be_oom_countdown = -1;
    return llvm_gen_be_error_t::out_of_memory;
  }
  if (g_llvm_be_oom_countdown > 0) {
    --g_llvm_be_oom_countdown;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Evaluates an expression returning llvm_gen_be_error_t, and returns it immediately if not ok.
 * @details This macro evaluates the provided expression once. If the result is not
 * llvm_gen_be_error_t::ok, the enclosing function returns the error code early.
 * @param expr The expression to evaluate, expected to return llvm_gen_be_error_t.
 */
#define LLVM_GEN_BE_CHECK(expr) \
  do { \
    llvm_gen_be_error_t _err_eval_result = (expr); \
    if (_err_eval_result != llvm_gen_be_error_t::ok) { \
      return _err_eval_result; \
    } \
  } while (0)

/**
 * @brief Evaluates an expression returning llvm_gen_be_error_t and returns early on failure.
 * @details Alias for `LLVM_GEN_BE_CHECK(expr)` used for explicit error propagation.
 * @param expr The expression to evaluate, expected to return llvm_gen_be_error_t.
 */
#define LLVM_GEN_BE_RETURN_IF_ERROR(expr) LLVM_GEN_BE_CHECK(expr)

/**
 * @brief Returns an `llvm_gen_be_error_t` status code directly from the enclosing function.
 * @details Explicit error-return macro for uniform error percolation.
 * @param err The `llvm_gen_be_error_t` status code to return.
 */
#define LLVM_GEN_BE_RETURN_ERR(err) return (err)

/**
 * @brief Allocates a heap object with `new (std::nothrow)` and returns `out_of_memory` on failure.
 * @details Checks `llvm_gen_be_step_oom_budget()` before invoking `new (std::nothrow) TypeExpr`
 *          and returns `llvm_gen_be_error_t::out_of_memory` if allocation fails.
 * @param ptr_var Destination pointer variable receiving the allocated object.
 * @param TypeExpr Constructor expression passed to `new (std::nothrow)`.
 */
#define LLVM_GEN_BE_NEW(ptr_var, TypeExpr) \
  do { \
    (ptr_var) = (llvm_gen_be_step_oom_budget() == llvm_gen_be_error_t::ok) \
                    ? new (std::nothrow) TypeExpr \
                    : nullptr; \
    if ((ptr_var) == nullptr) { \
      return llvm_gen_be_error_t::out_of_memory; \
    } \
  } while (0)

/**
 * @brief Checks if a pointer is null, and returns llvm_gen_be_error_t::invalid_argument if so.
 * @details This macro evaluates the provided pointer. If it evaluates to nullptr,
 * the enclosing function returns llvm_gen_be_error_t::invalid_argument early.
 * @param ptr The pointer expression to check against nullptr.
 */
#define LLVM_GEN_BE_CHECK_NOT_NULL(ptr) \
  do { \
    if ((ptr) == nullptr) { \
      return llvm_gen_be_error_t::invalid_argument; \
    } \
  } while (0)

/**
 * @brief Formats a source-positioned diagnostic into an error context and returns the error code.
 * @details Invokes `llvm_gen_be_format_diagnostic` with @p ctx, @p err, @p pos, and @p fmt
 *          (plus any variadic format arguments) and returns the resulting `llvm_gen_be_error_t`.
 * @param ctx Pointer to the `llvm_gen_be_error_context_t` to populate (may be nullptr).
 * @param err The `llvm_gen_be_error_t` status code to record and return.
 * @param pos The EDG `a_source_position` associated with the failure.
 * @param ... Printf-style format string and optional variadic arguments.
 */
#define LLVM_GEN_BE_RETURN_ERROR(ctx, err, pos, ...) \
  return llvm_gen_be_format_diagnostic((ctx), (err), (pos), __VA_ARGS__)

/**
 * @struct llvm_gen_be_error_context_t
 * @brief Diagnostic error context capturing source location and failure details.
 * @details When an error condition is encountered, this structure records the
 * source file, line, column, EDG sequence number, error code, and formatted human-readable diagnostic message.
 */
struct llvm_gen_be_error_context_t {
  llvm_gen_be_error_t error_code; ///< The error status code.
  const char* file_name;          ///< Source filename where the error occurred, or nullptr.
  uint32_t line_number;           ///< 1-based source line number, or 0 if unknown.
  uint32_t column_number;         ///< 1-based source column number, or 0 if unknown.
  a_seq_number seq_number;        ///< EDG IL source sequence number, or 0 if unknown.
  char message[512];              ///< Formatted diagnostic message buffer.
};

/**
 * @brief Sets and records error details within an error context.
 * @details Formats a diagnostic message into the error context structure and
 * sets the corresponding error code and location information.
 * @param[in,out] ctx Pointer to the error context to populate. If null, the function
 *                    still returns the error code without recording message details.
 * @param[in] err The error code representing the failure.
 * @param[in] file_name Source file path where the error originated, or nullptr.
 * @param[in] line Source line number where the error originated, or 0.
 * @param[in] col Source column number where the error originated, or 0.
 * @param[in] format Printf-style format string for the error message, or nullptr.
 * @param[in] ... Variadic arguments matching the format string.
 * @return The error code passed in @p err.
 */
llvm_gen_be_error_t llvm_gen_be_set_error(
    llvm_gen_be_error_context_t* ctx,
    llvm_gen_be_error_t err,
    const char* file_name,
    uint32_t line,
    uint32_t col,
    const char* format,
    ...) noexcept;

/**
 * @brief Formats a diagnostic message capturing EDG source coordinates.
 * @details Extracts file, line, and column from an EDG a_source_position
 * and populates the error context.
 * @param[in,out] ctx Pointer to the error context to populate. If null, the function
 *                    still returns the error code without recording message details.
 * @param[in] err The error code representing the failure.
 * @param[in] pos The EDG source position.
 * @param[in] format Printf-style format string for the error message.
 * @param[in] ... Variadic arguments matching the format string.
 * @return The error code passed in @p err.
 */
llvm_gen_be_error_t llvm_gen_be_format_diagnostic(
    llvm_gen_be_error_context_t* ctx,
    llvm_gen_be_error_t err,
    const a_source_position& pos,
    const char* format,
    ...) noexcept;

/**
 * @brief Converts an error code into a human-readable string representation.
 * @details Translates each enumeration value into a constant C-string name.
 * @param[in] err The error code to convert.
 * @param[out] out_str Pointer to a const char* receive parameter where the
 *                     resulting string pointer is stored.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if out_str is null.
 */
llvm_gen_be_error_t llvm_gen_be_error_to_string(
    llvm_gen_be_error_t err,
    const char** out_str) noexcept;

/**
 * @brief Resets an error context structure to default success state.
 * @details Clears the error code to ok, resets line and column to 0, and clears the message buffer.
 * @param[in,out] ctx Pointer to the error context to reset.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if ctx is null.
 */
llvm_gen_be_error_t llvm_gen_be_error_context_reset(
    llvm_gen_be_error_context_t* ctx) noexcept;

/**
 * @brief Formats an LLVM backend error context into a diagnostic string for the EDG front end.
 * @details Produces a structured human-readable diagnostic combining the file name, line,
 *          column, symbolic error code name, and detail message from @p ctx into @p out_buf.
 * @param[in] ctx Pointer to the populated error context structure to report.
 * @param[out] out_buf Destination character buffer receiving the formatted diagnostic string.
 * @param[in] buf_size Size of @p out_buf in bytes (must be greater than 0).
 * @param[out] out_is_error Receives true if @p ctx contains a non-ok error status, or false if ok.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null or buf_size is 0.
 */
llvm_gen_be_error_t llvm_gen_be_report_to_edg_fe(
    const llvm_gen_be_error_context_t* ctx,
    char* out_buf,
    size_t buf_size,
    bool* out_is_error) noexcept;

/**
 * @brief Retrieves the last recorded error code from an error context structure.
 * @details Reads `ctx->error_code` and stores it into `*out_err`.
 * @param[in] ctx Pointer to the error context structure to inspect.
 * @param[out] out_err Receives the last `llvm_gen_be_error_t` stored in @p ctx.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if ctx or out_err is null.
 */
llvm_gen_be_error_t llvm_gen_be_get_last_error(
    const llvm_gen_be_error_context_t* ctx,
    llvm_gen_be_error_t* out_err) noexcept;

/**
 * @brief Clears the error state in the given error context structure.
 * @details Delegates to `llvm_gen_be_error_context_reset(ctx)` to restore `ok` status.
 * @param[in,out] ctx Pointer to the error context structure to clear.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if ctx is null.
 */
llvm_gen_be_error_t llvm_gen_be_clear_error(
    llvm_gen_be_error_context_t* ctx) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_ERROR_H */
