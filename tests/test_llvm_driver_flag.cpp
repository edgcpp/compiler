/**
 * @file test_llvm_driver_flag.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for EDG LLVM backend CLI driver option kinds and helpers.
 * @details Verifies that `cmd_line.h` registers all required `optk_*` enumerators for the
 *          LLVM IR backend (`--gen_llvm_file_name`, `-O0..-O3/-Os/-Oz`, `-g`, `-gsplit-dwarf`,
 *          `--llvm_target`, `-mcpu`, `-mattr`, `-fPIC`, `-mcmodel`, `-fsanitize`, `-fstack-protector`,
 *          `-fopenmp`, `-l`) and validates string/file helper error handling.
 */

#include "basic_hdrs.h"
#include "fe_common.h"
#include "cmd_line.h"
#include "llvm_gen_be_error.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

using namespace edg;

/**
 * @brief Checks whether a file exists at `path`.
 * @details Opens an `std::ifstream` and stores `f.good()` into `out_exists`.
 * @param[in] path Filesystem path to check.
 * @param[out] out_exists Receives true if the file exists and is readable.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_exists` is null.
 */
static llvm_gen_be_error_t file_exists(const std::string& path, bool* out_exists) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_exists);
  std::ifstream f(path);
  *out_exists = f.good();
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Converts an ASCII character to its lowercase equivalent.
 * @details Uses `std::tolower` and writes the converted character to @p out_ch.
 * @param[in] c Input character byte.
 * @param[out] out_ch Receives the lowercase character.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t lower_char(unsigned char c, char* out_ch) noexcept {
  *out_ch = static_cast<char>(std::tolower(c));
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Converts a string to lowercase.
 * @details Transforms each character in `s` via `lower_char` and writes the result to `out_str`.
 * @param[in] s Input string to convert.
 * @param[out] out_str Pointer to output string receiving the lowercase copy.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_str` is null.
 */
static llvm_gen_be_error_t to_lower(std::string s, std::string* out_str) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_str);
  for (char& ch : s) {
    LLVM_GEN_BE_CHECK(lower_char(static_cast<unsigned char>(ch), &ch));
  }
  *out_str = s;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Verifies that all LLVM CLI option enumerators in `an_option_kind` are distinct and ordered.
 * @details Checks the `optk_*` enumerators added under `BACK_END_IS_LLVM_GEN_BE` in `src/cmd_line.h`.
 * @param[out] out_valid Receives `true` when all LLVM CLI option enumerators are valid.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_valid` is null.
 */
static llvm_gen_be_error_t verify_cli_option_kinds(bool* out_valid) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_valid);
  *out_valid = (optk_gen_llvm_file_name < optk_gen_llvm_bc_file_name) &&
               (optk_gen_llvm_bc_file_name < optk_gen_llvm_thinlto_index) &&
               (optk_gen_llvm_thinlto_index < optk_flto) &&
               (optk_flto < optk_fprofile_generate) &&
               (optk_fprofile_generate < optk_fprofile_use) &&
               (optk_fprofile_use < optk_gen_obj_file_name) &&
               (optk_gen_obj_file_name < optk_gen_asm_file_name) &&
               (optk_gen_asm_file_name < optk_fsanitize) &&
               (optk_fsanitize < optk_fstack_protector) &&
               (optk_fstack_protector < optk_llvm_opt_level) &&
               (optk_llvm_opt_level < optk_llvm_debug_info) &&
               (optk_llvm_debug_info < optk_llvm_split_dwarf) &&
               (optk_llvm_split_dwarf < optk_llvm_target) &&
               (optk_llvm_target < optk_llvm_mcpu) &&
               (optk_llvm_mcpu < optk_llvm_mattr) &&
               (optk_llvm_mattr < optk_llvm_reloc_model) &&
               (optk_llvm_reloc_model < optk_llvm_code_model) &&
               (optk_llvm_code_model < optk_llvm_fopenmp) &&
               (optk_llvm_fopenmp < optk_llvm_linker_arg);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Entry point for the CLI driver flag test executable.
 * @details Exercises valid and invalid argument paths of `file_exists`, `to_lower`, and `verify_cli_option_kinds`.
 * @return 0 on success, or non-zero on failure.
 */
int main(void) {
  bool exists = false;
  llvm_gen_be_error_t e1 = file_exists("CMakeLists.txt", nullptr);
  llvm_gen_be_error_t e2 = file_exists("/nonexistent_edg_llvm_test_file.xyz", &exists);

  std::string lowered;
  llvm_gen_be_error_t e3 = to_lower("UnReCoGnIzEd", nullptr);
  llvm_gen_be_error_t e4 = to_lower("UnReCoGnIzEd", &lowered);

  bool valid_opts = false;
  llvm_gen_be_error_t e5 = verify_cli_option_kinds(nullptr);
  llvm_gen_be_error_t e6 = verify_cli_option_kinds(&valid_opts);

  return (e1 == llvm_gen_be_error_t::invalid_argument &&
          e2 == llvm_gen_be_error_t::ok && !exists &&
          e3 == llvm_gen_be_error_t::invalid_argument &&
          e4 == llvm_gen_be_error_t::ok && lowered == "unrecognized" &&
          e5 == llvm_gen_be_error_t::invalid_argument &&
          e6 == llvm_gen_be_error_t::ok && valid_opts)
             ? 0
             : 1;
}
