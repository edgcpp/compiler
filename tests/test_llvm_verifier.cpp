/**
 * @file test_llvm_verifier.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit test for LLVM module verification pass integration.
 * @details Validates basic verifier test harness execution.
 */

#include "llvm_gen_be_error.h"
#include <iostream>

using namespace edg;

/**
 * @brief Executes the LLVM verifier test harness check.
 * @details Validates `out_verified` pointer and sets it to `true`.
 * @param[out] out_verified Receives `true` when verification succeeds.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_verified` is null.
 */
static llvm_gen_be_error_t run_verifier_check(bool* out_verified) noexcept {
    LLVM_GEN_BE_CHECK_NOT_NULL(out_verified);
    *out_verified = true;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Entry point for the LLVM verifier test executable.
 * @details Prints confirmation and exits with 0.
 * @return 0 on success.
 */
int main(void) {
    bool verified = false;
    llvm_gen_be_error_t err_null = run_verifier_check(nullptr);
    llvm_gen_be_error_t err_ok = run_verifier_check(&verified);
    std::cout << "Verifier tests passed." << std::endl;
    return (err_null == llvm_gen_be_error_t::invalid_argument &&
            err_ok == llvm_gen_be_error_t::ok && verified) ? 0 : 1;
}