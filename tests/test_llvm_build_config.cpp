/**
 * @file test_llvm_build_config.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit and integration tests for CMake LLVM backend macro configuration and lowering alignment.
 * @details Validates macro exclusivity (`BACK_END_IS_LLVM_GEN_BE == 1`, `BACK_END_IS_C_GEN_BE == 0`),
 *          default C++ exception enabling (`DEFAULT_EXCEPTIONS_ENABLED == 1`), and native LLVM EH
 *          lowering configuration (`DO_FULL_PORTABLE_EH_LOWERING == 0`, `GENERATE_EH_TABLES == 0`).
 */

#include "basic_hdrs.h"
#include "fe_common.h"
#include "llvm_gen_be_error.h"

using namespace edg;

/**
 * @brief Evaluates the active backend macro exclusivity configuration.
 * @details Writes `true` to @p out_exclusive when `BACK_END_IS_LLVM_GEN_BE == 1` and
 *          `BACK_END_IS_C_GEN_BE == 0` and `BACK_END_IS_CP_GEN_BE == 0`.
 * @param[out] out_exclusive Receives `true` if the LLVM backend macro configuration is exclusive.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p out_exclusive is null.
 */
static llvm_gen_be_error_t check_backend_macro_exclusivity(bool* out_exclusive) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_exclusive);
  *out_exclusive = (BACK_END_IS_LLVM_GEN_BE == 1) &&
                   (BACK_END_IS_C_GEN_BE == 0) &&
                   (BACK_END_IS_CP_GEN_BE == 0);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Evaluates the EDG exception handling and IL lowering macro alignment for LLVM IR.
 * @details Writes `true` to @p out_aligned when `DEFAULT_EXCEPTIONS_ENABLED == 1`,
 *          `DO_FULL_PORTABLE_EH_LOWERING == 0`, and `GENERATE_EH_TABLES == 0`.
 * @param[out] out_aligned Receives `true` if native LLVM EH lowering macros are aligned.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p out_aligned is null.
 */
static llvm_gen_be_error_t check_eh_lowering_macro_alignment(bool* out_aligned) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_aligned);
  *out_aligned = (DEFAULT_EXCEPTIONS_ENABLED == 1) &&
                 (DO_FULL_PORTABLE_EH_LOWERING == 0) &&
                 (GENERATE_EH_TABLES == 0) &&
                 (KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED == 1);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Entry point for the LLVM build configuration and macro alignment test suite.
 * @details Exercises both valid and null-argument paths of the configuration verifiers.
 * @return 0 on success.
 */
int main(void) {
  bool exclusive = false;
  llvm_gen_be_error_t err1 = check_backend_macro_exclusivity(nullptr);
  llvm_gen_be_error_t err2 = check_backend_macro_exclusivity(&exclusive);

  bool aligned = false;
  llvm_gen_be_error_t err3 = check_eh_lowering_macro_alignment(nullptr);
  llvm_gen_be_error_t err4 = check_eh_lowering_macro_alignment(&aligned);

  return (err1 == llvm_gen_be_error_t::invalid_argument &&
          err2 == llvm_gen_be_error_t::ok && exclusive &&
          err3 == llvm_gen_be_error_t::invalid_argument &&
          err4 == llvm_gen_be_error_t::ok && aligned)
             ? 0
             : 1;
}
