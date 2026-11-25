/**
 * @file test_multi_tu_a.cpp
 * @brief Primary translation unit for multi-TU C++ weak-linkage and `<iostream>` init verification.
 * @details Includes `<iostream>` and `test_multi_tu_hdr.h`, runs a file-scope dynamic initializer
 * alongside `test_multi_tu_b.cpp` (verifying TU-local global ctor symbols do not collide), and
 * exercises cross-TU C++ namespace functions, variables, and out-of-line class methods.
 * @ingroup gcc_gen_be_tests
 */

#include <iostream>
#include "test_multi_tu_hdr.h"

namespace {

/**
 * @brief Helper computing the initial value for TU A's dynamic global constructor.
 * @details Ensures dynamic initialization runs prior to `main()`.
 * @return Constant `105`.
 */
int init_tu_a_seed() noexcept {
    return (std::cout.good() ? 100 : 0) + 5;
}

/** @brief TU-A file-scope dynamically initialized global variable. */
int g_tu_a_ctor_value = init_tu_a_seed();

} // namespace

/**
 * @brief Runs all multi-TU linkage, global initializer, namespace, and vtable checks.
 * @details Verifies both TU A and TU B global constructors ran, `std::cout` is initialized,
 * weak `_ZTV`/`_ZTI` polymorphic dispatch works across TUs, and out-of-line methods succeed.
 * @param[out] out_ok Pointer receiving `1` when all assertions pass.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
static gcc_gen_be_error_t run_multi_tu_checks(int *out_ok) noexcept {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    ASSERT_EQ(g_tu_a_ctor_value, 105);
    ASSERT_EQ(multi_ns::g_tu_b_ctor_value, 210);

    multi_ns::MultiTuDerived d(7, 4);
    const multi_ns::MultiTuBase *base_ptr = &d;
    int eval_res = 0;
    GCC_GEN_BE_CHECK(base_ptr->evaluate(10, &eval_res));
    ASSERT_EQ(eval_res, 47);

    int ool_res = 0;
    GCC_GEN_BE_CHECK(d.compute_out_of_line(6, &ool_res));
    ASSERT_EQ(ool_res, 234);

    int ns_res = 0;
    GCC_GEN_BE_CHECK(multi_ns::compute_from_tu_b(15, &ns_res));
    ASSERT_EQ(ns_res, 255);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for the multi-TU C++ execution test binary.
 * @details Invokes `run_multi_tu_checks` and returns `0` on success.
 * @return `0` on success, `1` on failure.
 */
int main() noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_multi_tu_checks(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok == 1) ? 0 : 1;
}
