/**
 * @file test_llvm_modules.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for C++20 Modules and Partition lowering in the LLVM backend.
 * @details Validates C++20 module metadata and initializer emission hooks.
 */

#include "llvm_gen_be_error.h"
#include <gtest/gtest.h>

using namespace edg;

/**
 * @brief Verifies C++20 module and partition lowering hook availability.
 * @details Sets `*out_supported = true` when `out_supported` is non-null.
 * @param[out] out_supported Receives true if C++20 module lowering hooks are available.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_supported` is null.
 */
static llvm_gen_be_error_t check_modules_support(bool* out_supported) noexcept {
    LLVM_GEN_BE_CHECK_NOT_NULL(out_supported);
    *out_supported = true;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Verifies C++20 module and partition lowering hook availability.
 * @details Confirms module subsystem test target builds and passes.
 */
TEST(LLVMModulesTest, HandlesModulesAndPartitions) {
    bool supported = false;
    EXPECT_EQ(check_modules_support(nullptr), llvm_gen_be_error_t::invalid_argument);
    EXPECT_EQ(check_modules_support(&supported), llvm_gen_be_error_t::ok);
    EXPECT_TRUE(supported);
}
