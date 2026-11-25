/**
 * @file llvm_gen_be_error.cpp
 * @ingroup llvm_gen_be_driver
 * @brief Implementation of error reporting and conversion functions for LLVM backend.
 * @details Implements llvm_gen_be_set_error, llvm_gen_be_error_to_string, and
 * llvm_gen_be_error_context_reset.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "fe_common.h"
#include "il.h"
#include "llvm_gen_be_error.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

BEGIN_EDG_NAMESPACE

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
    ...) noexcept {
  if (ctx != nullptr) {
    ctx->error_code = err;
    ctx->file_name = file_name;
    ctx->line_number = line;
    ctx->column_number = col;
    ctx->seq_number = 0;
    if (format != nullptr) {
      va_list args;
      va_start(args, format);
      vsnprintf(ctx->message, sizeof(ctx->message), format, args);
      va_end(args);
    } else {
      ctx->message[0] = '\0';
    }
  }
  return err;
}

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
    ...) noexcept {
  if (ctx != nullptr) {
    a_const_char* file_name = nullptr;
    a_const_char* full_name = nullptr;
    a_line_number line = 0;
    a_boolean at_end_of_source = FALSE;

    if (pos.seq != 0) {
      conv_seq_to_file_and_line(pos.seq, &file_name, &full_name, &line, &at_end_of_source);
    }

    ctx->error_code = err;
    ctx->file_name = file_name;
    ctx->line_number = line;
    ctx->column_number = pos.column;
    ctx->seq_number = pos.seq;

    if (format != nullptr) {
      va_list args;
      va_start(args, format);
      vsnprintf(ctx->message, sizeof(ctx->message), format, args);
      va_end(args);
    } else {
      ctx->message[0] = '\0';
    }
  }
  return err;
}

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
    const char** out_str) noexcept {
  if (out_str == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  switch (err) {
    case llvm_gen_be_error_t::ok:
      *out_str = "ok";
      break;
    case llvm_gen_be_error_t::invalid_argument:
      *out_str = "invalid_argument";
      break;
    case llvm_gen_be_error_t::out_of_memory:
      *out_str = "out_of_memory";
      break;
    case llvm_gen_be_error_t::unsupported_type:
      *out_str = "unsupported_type";
      break;
    case llvm_gen_be_error_t::unsupported_expr:
      *out_str = "unsupported_expr";
      break;
    case llvm_gen_be_error_t::unsupported_stmt:
      *out_str = "unsupported_stmt";
      break;
    case llvm_gen_be_error_t::abi_classification_failed:
      *out_str = "abi_classification_failed";
      break;
    case llvm_gen_be_error_t::di_metadata_failure:
      *out_str = "di_metadata_failure";
      break;
    case llvm_gen_be_error_t::pass_pipeline_failure:
      *out_str = "pass_pipeline_failure";
      break;
    case llvm_gen_be_error_t::code_gen_failure:
      *out_str = "code_gen_failure";
      break;
    case llvm_gen_be_error_t::verification_failure:
      *out_str = "verification_failure";
      break;
    case llvm_gen_be_error_t::io_error:
      *out_str = "io_error";
      break;
    case llvm_gen_be_error_t::constant_eval_failure:
      *out_str = "constant_eval_failure";
      break;
    case llvm_gen_be_error_t::symbol_lookup_failure:
      *out_str = "symbol_lookup_failure";
      break;
    case llvm_gen_be_error_t::eh_lowering_failure:
      *out_str = "eh_lowering_failure";
      break;
    case llvm_gen_be_error_t::vtable_generation_failure:
      *out_str = "vtable_generation_failure";
      break;
    case llvm_gen_be_error_t::coroutine_lowering_failure:
      *out_str = "coroutine_lowering_failure";
      break;
    case llvm_gen_be_error_t::inline_asm_failure:
      *out_str = "inline_asm_failure";
      break;
    case llvm_gen_be_error_t::internal_inconsistency:
      *out_str = "internal_inconsistency";
      break;
    case llvm_gen_be_error_t::not_implemented:
      *out_str = "not_implemented";
      break;
    case llvm_gen_be_error_t::omp_lowering_failure:
      *out_str = "omp_lowering_failure";
      break;
    case llvm_gen_be_error_t::offload_bundling_failed:
      *out_str = "offload_bundling_failed";
      break;
    case llvm_gen_be_error_t::lto_indexing_failed:
      *out_str = "lto_indexing_failed";
      break;
    case llvm_gen_be_error_t::module_bmi_corrupt:
      *out_str = "module_bmi_corrupt";
      break;
    case llvm_gen_be_error_t::dwarf_macro_emission_failed:
      *out_str = "dwarf_macro_emission_failed";
      break;
    case llvm_gen_be_error_t::coro_frame_layout_failed:
      *out_str = "coro_frame_layout_failed";
      break;
    case llvm_gen_be_error_t::null_pointer:
      *out_str = "null_pointer";
      break;
    case llvm_gen_be_error_t::type_lowering_failed:
      *out_str = "type_lowering_failed";
      break;
    case llvm_gen_be_error_t::coro_lowering_failed:
      *out_str = "coro_lowering_failed";
      break;
    case llvm_gen_be_error_t::omp_lowering_failed:
      *out_str = "omp_lowering_failed";
      break;
    case llvm_gen_be_error_t::eh_lowering_failed:
      *out_str = "eh_lowering_failed";
      break;
    case llvm_gen_be_error_t::rtti_vtable_failed:
      *out_str = "rtti_vtable_failed";
      break;
    case llvm_gen_be_error_t::debug_info_failed:
      *out_str = "debug_info_failed";
      break;
    case llvm_gen_be_error_t::opt_pipeline_failed:
      *out_str = "opt_pipeline_failed";
      break;
    case llvm_gen_be_error_t::codegen_emit_failed:
      *out_str = "codegen_emit_failed";
      break;
    case llvm_gen_be_error_t::linker_invocation_failed:
      *out_str = "linker_invocation_failed";
      break;
    case llvm_gen_be_error_t::gpu_offload_failed:
      *out_str = "gpu_offload_failed";
      break;
    case llvm_gen_be_error_t::module_io_failed:
      *out_str = "module_io_failed";
      break;
    case llvm_gen_be_error_t::verification_failed:
      *out_str = "verification_failed";
      break;
    case llvm_gen_be_error_t::internal_error:
      *out_str = "internal_error";
      break;
    case llvm_gen_be_error_t::builtin_lowering_failed:
      *out_str = "builtin_lowering_failed";
      break;
    case llvm_gen_be_error_t::atomic_lowering_failed:
      *out_str = "atomic_lowering_failed";
      break;
    case llvm_gen_be_error_t::complex_lowering_failed:
      *out_str = "complex_lowering_failed";
      break;
    case llvm_gen_be_error_t::vla_lowering_failed:
      *out_str = "vla_lowering_failed";
      break;
    case llvm_gen_be_error_t::bitfield_access_failed:
      *out_str = "bitfield_access_failed";
      break;
    default:
      *out_str = "unknown_error";
      break;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resets an error context structure to default success state.
 * @details Clears the error code to ok, resets line, column, and sequence number to 0, and clears the message buffer.
 * @param[in,out] ctx Pointer to the error context to reset.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if ctx is null.
 */
llvm_gen_be_error_t llvm_gen_be_error_context_reset(
    llvm_gen_be_error_context_t* ctx) noexcept {
  if (ctx == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  ctx->error_code = llvm_gen_be_error_t::ok;
  ctx->file_name = nullptr;
  ctx->line_number = 0;
  ctx->column_number = 0;
  ctx->seq_number = 0;
  ctx->message[0] = '\0';
  return llvm_gen_be_error_t::ok;
}

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
    bool* out_is_error) noexcept {
  if (ctx == nullptr || out_buf == nullptr || buf_size == 0 || out_is_error == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (ctx->error_code == llvm_gen_be_error_t::ok) {
    out_buf[0] = '\0';
    *out_is_error = false;
    return llvm_gen_be_error_t::ok;
  }
  *out_is_error = true;
  const char* err_name = nullptr;
  const llvm_gen_be_error_t str_err = llvm_gen_be_error_to_string(ctx->error_code, &err_name);
  const char* file = (ctx->file_name != nullptr) ? ctx->file_name : "<unknown>";
  if (ctx->message[0] != '\0') {
    snprintf(out_buf, buf_size, "%s:%u:%u: error [%s]: %s",
             file, ctx->line_number, ctx->column_number, err_name, ctx->message);
  } else {
    snprintf(out_buf, buf_size, "%s:%u:%u: error [%s]",
             file, ctx->line_number, ctx->column_number, err_name);
  }
  return str_err;
}

/**
 * @brief Retrieves the last recorded error code from an error context structure.
 * @details Reads `ctx->error_code` and stores it into `*out_err`.
 * @param[in] ctx Pointer to the error context structure to inspect.
 * @param[out] out_err Receives the last `llvm_gen_be_error_t` stored in @p ctx.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if ctx or out_err is null.
 */
llvm_gen_be_error_t llvm_gen_be_get_last_error(
    const llvm_gen_be_error_context_t* ctx,
    llvm_gen_be_error_t* out_err) noexcept {
  if (ctx == nullptr || out_err == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  *out_err = ctx->error_code;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Clears the error state in the given error context structure.
 * @details Delegates to `llvm_gen_be_error_context_reset(ctx)` to restore `ok` status.
 * @param[in,out] ctx Pointer to the error context structure to clear.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if ctx is null.
 */
llvm_gen_be_error_t llvm_gen_be_clear_error(
    llvm_gen_be_error_context_t* ctx) noexcept {
  return llvm_gen_be_error_context_reset(ctx);
}

END_EDG_NAMESPACE
