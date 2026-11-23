/**
 * @defgroup llvm_gen_be_tests LLVM Backend Unit & Execution Tests
 * @ingroup llvm_gen_be
 * @brief Unit, fuzz, sanitizer, PGO, and native end-to-end execution tests for the LLVM backend.
 */

/**
 * @file test_llvm_error.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the LLVM backend error handling subsystem.
 * @details Validates function coverage of llvm_gen_be_error.cpp and llvm_gen_be_error.h macros.
 */

#include "llvm_gen_be_error.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace edg;

// Dummy implementation of conv_seq_to_file_and_line to satisfy the linker
// for llvm_gen_be_format_diagnostic.
namespace edg {
  /**
   * @brief Mock implementation of `conv_seq_to_file_and_line` for unit testing.
   * @details Returns `"dummy.cpp"` and maps `seq_number` directly to `line_number`.
   * @param[in] seq_number The source sequence number.
   * @param[out] file_name Receives the short file name string.
   * @param[out] full_name Receives the full file path string.
   * @param[out] line_number Receives the 1-based line number.
   * @param[out] at_end_of_source Receives FALSE.
   * @return Always returns nullptr.
   */
  a_source_file_ptr conv_seq_to_file_and_line(a_seq_number seq_number,
                                              a_const_char **file_name,
                                              a_const_char **full_name,
                                              a_line_number *line_number,
                                              a_boolean *at_end_of_source) {
    if (file_name) *file_name = "dummy.cpp";
    if (full_name) *full_name = "/path/to/dummy.cpp";
    if (line_number) *line_number = seq_number;
    if (at_end_of_source) *at_end_of_source = FALSE;
    return nullptr;
  }
}

/**
 * @brief Tests `LLVM_GEN_BE_CHECK` when the inner expression succeeds (`llvm_gen_be_error_t::ok`).
 * @details Verifies execution continues past the macro and returns `ok`.
 * @return llvm_gen_be_error_t::ok.
 */
static llvm_gen_be_error_t check_macro_ok(void) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_error_t::ok);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests `LLVM_GEN_BE_CHECK` when the inner expression fails (`llvm_gen_be_error_t::out_of_memory`).
 * @details Verifies the macro returns the error code immediately.
 * @return llvm_gen_be_error_t::out_of_memory.
 */
static llvm_gen_be_error_t check_macro_fail(void) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_error_t::out_of_memory);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests `LLVM_GEN_BE_CHECK_NOT_NULL` with a valid non-null pointer.
 * @details Verifies execution continues past the macro and returns `ok`.
 * @return llvm_gen_be_error_t::ok.
 */
static llvm_gen_be_error_t check_not_null_ok(void) noexcept {
  int x = 42;
  LLVM_GEN_BE_CHECK_NOT_NULL(&x);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests `LLVM_GEN_BE_CHECK_NOT_NULL` with a `nullptr` argument.
 * @details Verifies the macro returns `llvm_gen_be_error_t::invalid_argument`.
 * @return llvm_gen_be_error_t::invalid_argument.
 */
static llvm_gen_be_error_t check_not_null_fail(void) noexcept {
  int* p = nullptr;
  LLVM_GEN_BE_CHECK_NOT_NULL(p);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests `LLVM_GEN_BE_RETURN_ERROR` formatting and returning a diagnostic error code.
 * @details Verifies that `LLVM_GEN_BE_RETURN_ERROR` populates @p ctx and returns `unsupported_expr`.
 * @param[in,out] ctx Pointer to the error context to populate.
 * @param[in] pos Source position to record.
 * @return llvm_gen_be_error_t::unsupported_expr.
 */
static llvm_gen_be_error_t check_return_error_macro(
    llvm_gen_be_error_context_t* ctx,
    const a_source_position& pos) noexcept {
  LLVM_GEN_BE_RETURN_ERROR(ctx, llvm_gen_be_error_t::unsupported_expr, pos, "Macro error %d", 77);
}

/**
 * @brief Tests `LLVM_GEN_BE_NEW` heap allocation and deterministic OOM failure propagation.
 * @details Allocates an integer with `LLVM_GEN_BE_NEW`, stores its value into @p out_val, and frees it.
 * @param[out] out_val Receives the allocated integer value (`99`) on success.
 * @return `llvm_gen_be_error_t::ok` on success, or `out_of_memory` on injected budget exhaustion.
 */
static llvm_gen_be_error_t check_new_macro(int* out_val) noexcept {
  int* allocated = nullptr;
  LLVM_GEN_BE_NEW(allocated, int(99));
  *out_val = *allocated;
  delete allocated;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Entry point for the LLVM backend error handling unit tests.
 * @details Exercises all macros, error context functions, and `llvm_gen_be_error_to_string` enumerators.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_error...\n");

  // Test LLVM_GEN_BE_CHECK
  assert(check_macro_ok() == llvm_gen_be_error_t::ok);
  assert(check_macro_fail() == llvm_gen_be_error_t::out_of_memory);

  // Test LLVM_GEN_BE_CHECK_NOT_NULL
  assert(check_not_null_ok() == llvm_gen_be_error_t::ok);
  assert(check_not_null_fail() == llvm_gen_be_error_t::invalid_argument);

  // Test llvm_gen_be_set_error
  llvm_gen_be_error_context_t ctx;
  assert(llvm_gen_be_error_context_reset(&ctx) == llvm_gen_be_error_t::ok);
  a_source_position macro_pos = {};
  macro_pos.seq = 15;
  macro_pos.column = 4;
  assert(check_return_error_macro(&ctx, macro_pos) == llvm_gen_be_error_t::unsupported_expr);
  assert(ctx.error_code == llvm_gen_be_error_t::unsupported_expr);
  assert(strcmp(ctx.message, "Macro error 77") == 0);
  assert(llvm_gen_be_error_context_reset(&ctx) == llvm_gen_be_error_t::ok);
  
  llvm_gen_be_error_t err = llvm_gen_be_set_error(&ctx, llvm_gen_be_error_t::io_error, "test.cpp", 10, 5, "Failed to open file %s", "data.txt");
  assert(err == llvm_gen_be_error_t::io_error);
  assert(ctx.error_code == llvm_gen_be_error_t::io_error);
  assert(strcmp(ctx.file_name, "test.cpp") == 0);
  assert(ctx.line_number == 10);
  assert(ctx.column_number == 5);
  assert(strcmp(ctx.message, "Failed to open file data.txt") == 0);

  // Test set_error with null context
  assert(llvm_gen_be_set_error(nullptr, llvm_gen_be_error_t::out_of_memory, "test.cpp", 1, 1, nullptr) == llvm_gen_be_error_t::out_of_memory);

  // Test set_error without format string
  assert(llvm_gen_be_set_error(&ctx, llvm_gen_be_error_t::invalid_argument, "test2.cpp", 20, 2, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(ctx.error_code == llvm_gen_be_error_t::invalid_argument);
  assert(ctx.message[0] == '\0');

  // Test llvm_gen_be_format_diagnostic
  a_source_position pos;
  pos.seq = 42;
  pos.column = 7;
  err = llvm_gen_be_format_diagnostic(&ctx, llvm_gen_be_error_t::di_metadata_failure, pos, "Bad debug info: %d", 123);
  assert(err == llvm_gen_be_error_t::di_metadata_failure);
  assert(ctx.error_code == llvm_gen_be_error_t::di_metadata_failure);
  assert(strcmp(ctx.file_name, "dummy.cpp") == 0);
  assert(ctx.line_number == 42);
  assert(ctx.column_number == 7);
  assert(strcmp(ctx.message, "Bad debug info: 123") == 0);

  // Test format_diagnostic with null context
  assert(llvm_gen_be_format_diagnostic(nullptr, llvm_gen_be_error_t::out_of_memory, pos, nullptr) == llvm_gen_be_error_t::out_of_memory);

  // Test format_diagnostic without format string
  assert(llvm_gen_be_format_diagnostic(&ctx, llvm_gen_be_error_t::invalid_argument, pos, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(ctx.error_code == llvm_gen_be_error_t::invalid_argument);
  assert(ctx.message[0] == '\0');

  // Test format_diagnostic with pos.seq == 0
  a_source_position zero_pos = {};
  zero_pos.seq = 0;
  zero_pos.column = 3;
  assert(llvm_gen_be_format_diagnostic(&ctx, llvm_gen_be_error_t::io_error, zero_pos, nullptr) == llvm_gen_be_error_t::io_error);
  assert(ctx.file_name == nullptr);
  assert(ctx.line_number == 0);
  assert(ctx.column_number == 3);

  // Test llvm_gen_be_error_to_string
  const char* str = nullptr;
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::ok, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "ok") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::invalid_argument, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "invalid_argument") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::out_of_memory, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "out_of_memory") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::unsupported_type, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "unsupported_type") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::unsupported_expr, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "unsupported_expr") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::unsupported_stmt, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "unsupported_stmt") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::abi_classification_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "abi_classification_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::di_metadata_failure, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "di_metadata_failure") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::pass_pipeline_failure, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "pass_pipeline_failure") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::code_gen_failure, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "code_gen_failure") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::verification_failure, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "verification_failure") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::io_error, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "io_error") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::constant_eval_failure, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "constant_eval_failure") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::symbol_lookup_failure, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "symbol_lookup_failure") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::eh_lowering_failure, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "eh_lowering_failure") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::vtable_generation_failure, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "vtable_generation_failure") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::coroutine_lowering_failure, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "coroutine_lowering_failure") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::inline_asm_failure, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "inline_asm_failure") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::internal_inconsistency, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "internal_inconsistency") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::not_implemented, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "not_implemented") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::omp_lowering_failure, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "omp_lowering_failure") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::offload_bundling_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "offload_bundling_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::lto_indexing_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "lto_indexing_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::module_bmi_corrupt, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "module_bmi_corrupt") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::dwarf_macro_emission_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "dwarf_macro_emission_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::coro_frame_layout_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "coro_frame_layout_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::null_pointer, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "null_pointer") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::type_lowering_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "type_lowering_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::coro_lowering_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "coro_lowering_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::omp_lowering_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "omp_lowering_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::eh_lowering_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "eh_lowering_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::rtti_vtable_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "rtti_vtable_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::debug_info_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "debug_info_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::opt_pipeline_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "opt_pipeline_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::codegen_emit_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "codegen_emit_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::linker_invocation_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "linker_invocation_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::gpu_offload_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "gpu_offload_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::module_io_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "module_io_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::verification_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "verification_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::internal_error, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "internal_error") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::builtin_lowering_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "builtin_lowering_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::atomic_lowering_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "atomic_lowering_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::complex_lowering_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "complex_lowering_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::vla_lowering_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "vla_lowering_failed") == 0);
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::bitfield_access_failed, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "bitfield_access_failed") == 0);

  // Test deterministic OOM budget and LLVM_GEN_BE_NEW macro
  g_llvm_be_oom_countdown = -1;
  int alloc_v = 0;
  assert(check_new_macro(&alloc_v) == llvm_gen_be_error_t::ok);
  assert(alloc_v == 99);
  g_llvm_be_oom_countdown = 1;
  assert(check_new_macro(&alloc_v) == llvm_gen_be_error_t::ok);
  assert(g_llvm_be_oom_countdown == 0);
  assert(check_new_macro(&alloc_v) == llvm_gen_be_error_t::out_of_memory);
  assert(g_llvm_be_oom_countdown == -1);

  assert(llvm_gen_be_error_to_string((llvm_gen_be_error_t)999, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "unknown_error") == 0);
  
  // Test null out parameter
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::ok, nullptr) == llvm_gen_be_error_t::invalid_argument);

  // Test llvm_gen_be_error_context_reset, llvm_gen_be_get_last_error, and llvm_gen_be_clear_error
  llvm_gen_be_error_t last_err = llvm_gen_be_error_t::out_of_memory;
  assert(llvm_gen_be_get_last_error(nullptr, &last_err) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_gen_be_get_last_error(&ctx, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_gen_be_clear_error(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_gen_be_error_context_reset(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_gen_be_clear_error(&ctx) == llvm_gen_be_error_t::ok);
  assert(llvm_gen_be_get_last_error(&ctx, &last_err) == llvm_gen_be_error_t::ok);
  assert(last_err == llvm_gen_be_error_t::ok);
  assert(ctx.error_code == llvm_gen_be_error_t::ok);
  assert(ctx.file_name == nullptr);
  assert(ctx.line_number == 0);
  assert(ctx.column_number == 0);
  assert(ctx.message[0] == '\0');

  // Test llvm_gen_be_report_to_edg_fe
  char report_buf[256] = {};
  bool is_err = false;
  assert(llvm_gen_be_report_to_edg_fe(nullptr, report_buf, sizeof(report_buf), &is_err) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_gen_be_report_to_edg_fe(&ctx, nullptr, sizeof(report_buf), &is_err) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_gen_be_report_to_edg_fe(&ctx, report_buf, 0, &is_err) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_gen_be_report_to_edg_fe(&ctx, report_buf, sizeof(report_buf), nullptr) == llvm_gen_be_error_t::invalid_argument);

  // ok status
  assert(llvm_gen_be_report_to_edg_fe(&ctx, report_buf, sizeof(report_buf), &is_err) == llvm_gen_be_error_t::ok);
  assert(!is_err);
  assert(report_buf[0] == '\0');

  // Error with file and message
  assert(llvm_gen_be_set_error(&ctx, llvm_gen_be_error_t::unsupported_type, "foo.cpp", 12, 8, "bad type") == llvm_gen_be_error_t::unsupported_type);
  assert(llvm_gen_be_report_to_edg_fe(&ctx, report_buf, sizeof(report_buf), &is_err) == llvm_gen_be_error_t::ok);
  assert(is_err);
  assert(strcmp(report_buf, "foo.cpp:12:8: error [unsupported_type]: bad type") == 0);

  // Error without file and without message
  assert(llvm_gen_be_set_error(&ctx, llvm_gen_be_error_t::internal_inconsistency, nullptr, 0, 0, nullptr) == llvm_gen_be_error_t::internal_inconsistency);
  assert(llvm_gen_be_report_to_edg_fe(&ctx, report_buf, sizeof(report_buf), &is_err) == llvm_gen_be_error_t::ok);
  assert(is_err);
  assert(strcmp(report_buf, "<unknown>:0:0: error [internal_inconsistency]") == 0);

  printf("PASS\n");
  return 0;
}
