/**
 * @file test_multi_tu_b.cpp
 * @brief Secondary translation unit for multi-TU C++ weak-linkage and `<iostream>` init verification.
 * @details Includes `<iostream>` and `test_multi_tu_hdr.h` in a second `.o` object file, emitting
 * duplicate vague-linkage `_ZTV`/`_ZTI` globals, its own TU-local global constructor, namespace
 * variables/functions, and the out-of-line definition of `MultiTuDerived::compute_out_of_line`.
 * @ingroup gcc_gen_be_tests
 */

#include <iostream>
#include "test_multi_tu_hdr.h"

namespace multi_ns {

/**
 * @brief Computes initial value for `g_tu_b_ctor_value` at dynamic initialization time.
 * @details References `std::cerr` from `<iostream>` to verify `<iostream>` init across 2+ TUs.
 * @return Constant `210`.
 */
static int compute_tu_b_init() noexcept {
    return (std::cerr.good() ? 200 : 0) + 10;
}

int g_tu_b_ctor_value = compute_tu_b_init();

gcc_gen_be_error_t MultiTuDerived::compute_out_of_line(int x, int *out_val) const noexcept {
    if (!out_val) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_val = g_tu_b_ctor_value + (x * scale);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t compute_from_tu_b(int a, int *out_sum) noexcept {
    if (!out_sum) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    MultiTuDerived local_d(10, 2);
    const MultiTuBase *bp = &local_d;
    int v = 0;
    GCC_GEN_BE_CHECK(bp->evaluate(a, &v));
    *out_sum = g_tu_b_ctor_value + (v - 10 + 15);
    return GCC_GEN_BE_SUCCESS;
}

} // namespace multi_ns
