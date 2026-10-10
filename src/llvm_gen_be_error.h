/**
 * @file llvm_gen_be_error.h
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

BEGIN_EDG_NAMESPACE

/**
 * @enum llvm_gen_be_error_t
 * @brief Centralized error status codes for the LLVM generation backend.
 * @details Every function in the LLVM backend returns this enumeration.
 * Decorated with [[nodiscard]] directly on the typedef/type definition to enforce
 * mandatory error inspection and bubbling without needing [[nodiscard]] on every function.
 */
typedef enum class [[nodiscard]] llvm_gen_be_error_t : int {
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
  internal_inconsistency = 18    ///< Internal compiler inconsistency or invariant violation.
} llvm_gen_be_error_t;

/**
 * @struct llvm_gen_be_error_context_t
 * @brief Diagnostic error context capturing source location and failure details.
 * @details When an error condition is encountered, this structure records the
 * source file, line, column, error code, and formatted human-readable diagnostic message.
 */
struct llvm_gen_be_error_context_t {
  llvm_gen_be_error_t error_code; ///< The error status code.
  const char* file_name;          ///< Source filename where the error occurred, or nullptr.
  uint32_t line_number;           ///< 1-based source line number, or 0 if unknown.
  uint32_t column_number;         ///< 1-based source column number, or 0 if unknown.
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

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_ERROR_H */
