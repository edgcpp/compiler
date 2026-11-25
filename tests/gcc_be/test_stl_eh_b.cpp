/**
 * @file test_stl_eh_b.cpp
 * @brief Secondary translation unit throwing exceptions across TU boundaries with intermediate RAII cleanups.
 * @details Defines `throw_mi_across_tu` and `throw_diamond_across_tu`, verifying that exceptions
 * thrown in TU B run local RAII destructors in reverse construction order and unwind via DWARF
 * `.eh_frame` into `catch` handlers in `test_stl_eh_a.cpp`.
 * @ingroup gcc_gen_be_tests
 */

#include "test_stl_eh_hdr.h"

namespace {

/**
 * @brief Scoped RAII tracker recording destruction order across TU boundaries.
 */
struct TuBRaiiGuard {
    /** @brief Pointer to trace accumulator. */
    int *trace;
    /** @brief Step digit appended on destruction. */
    int step;

    /**
     * @brief Constructs `TuBRaiiGuard`.
     * @param[in] t Trace pointer.
     * @param[in] s Step digit.
     */
    TuBRaiiGuard(int *t, int s) noexcept : trace(t), step(s) {}

    /**
     * @brief Appends `step` to `*trace` during stack unwinding.
     */
    ~TuBRaiiGuard() noexcept {
        if (trace) {
            *trace = (*trace * 10) + step;
        }
    }
};

/**
 * @brief Leaf helper throwing `MiCombined` inside TU B.
 * @details Constructs `TuBRaiiGuard(3)` then throws `MiCombined(111, 222, 333)`.
 * @param[in,out] inout_trace Pointer to destruction order trace.
 * @return Never returns normally.
 */
gcc_gen_be_error_t leaf_throw_mi(int *inout_trace) noexcept(false) {
    TuBRaiiGuard g3(inout_trace, 3);
    throw MiCombined(111L, 222L, 333L);
}

} // namespace

gcc_gen_be_error_t throw_mi_across_tu(int *inout_trace) noexcept(false) {
    TuBRaiiGuard g2(inout_trace, 2);
    return leaf_throw_mi(inout_trace);
}

gcc_gen_be_error_t throw_diamond_across_tu(int *inout_trace) noexcept(false) {
    TuBRaiiGuard gd(inout_trace, 9);
    throw VDiamond(777L, 10L, 20L, 30L);
}

gcc_gen_be_error_t throw_diamond_flags3_across_tu(int *inout_trace) noexcept(false) {
    TuBRaiiGuard gf3(inout_trace, 8);
    throw VDiamondPlusRepeat(888L, 11L, 22L, 33L, 44L, 55L, 66L, 77L);
}
