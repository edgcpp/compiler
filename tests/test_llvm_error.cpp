/**
 * @file test_llvm_error.cpp
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

static llvm_gen_be_error_t check_macro_ok() {
  LLVM_GEN_BE_CHECK(llvm_gen_be_error_t::ok);
  return llvm_gen_be_error_t::ok;
}

static llvm_gen_be_error_t check_macro_fail() {
  LLVM_GEN_BE_CHECK(llvm_gen_be_error_t::out_of_memory);
  return llvm_gen_be_error_t::ok;
}

static llvm_gen_be_error_t check_not_null_ok() {
  int x = 42;
  LLVM_GEN_BE_CHECK_NOT_NULL(&x);
  return llvm_gen_be_error_t::ok;
}

static llvm_gen_be_error_t check_not_null_fail() {
  int* p = nullptr;
  LLVM_GEN_BE_CHECK_NOT_NULL(p);
  return llvm_gen_be_error_t::ok;
}

int main() {
  printf("Running test_llvm_error...\n");

  // Test LLVM_GEN_BE_CHECK
  assert(check_macro_ok() == llvm_gen_be_error_t::ok);
  assert(check_macro_fail() == llvm_gen_be_error_t::out_of_memory);

  // Test LLVM_GEN_BE_CHECK_NOT_NULL
  assert(check_not_null_ok() == llvm_gen_be_error_t::ok);
  assert(check_not_null_fail() == llvm_gen_be_error_t::invalid_argument);

  // Test llvm_gen_be_set_error
  llvm_gen_be_error_context_t ctx;
  llvm_gen_be_error_context_reset(&ctx);
  
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
  llvm_gen_be_set_error(&ctx, llvm_gen_be_error_t::invalid_argument, "test2.cpp", 20, 2, nullptr);
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
  llvm_gen_be_format_diagnostic(&ctx, llvm_gen_be_error_t::invalid_argument, pos, nullptr);
  assert(ctx.error_code == llvm_gen_be_error_t::invalid_argument);
  assert(ctx.message[0] == '\0');

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

  assert(llvm_gen_be_error_to_string((llvm_gen_be_error_t)999, &str) == llvm_gen_be_error_t::ok);
  assert(strcmp(str, "unknown_error") == 0);
  
  // Test null out parameter
  assert(llvm_gen_be_error_to_string(llvm_gen_be_error_t::ok, nullptr) == llvm_gen_be_error_t::invalid_argument);

  // Test llvm_gen_be_error_context_reset
  assert(llvm_gen_be_error_context_reset(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_gen_be_error_context_reset(&ctx) == llvm_gen_be_error_t::ok);
  assert(ctx.error_code == llvm_gen_be_error_t::ok);
  assert(ctx.file_name == nullptr);
  assert(ctx.line_number == 0);
  assert(ctx.column_number == 0);
  assert(ctx.message[0] == '\0');

  printf("PASS\n");
  return 0;
}
