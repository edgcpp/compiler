/**
 * @file test_stl_eh_a.cpp
 * @brief Native C++ exception interop test linked against real system `libc++` / `libstdc++`.
 * @details Exercises all mandatory Section 7.1 EH scenarios without any mock `__cxa_throw`:
 * 1. `std::vector<int>().at(5)` throwing `std::out_of_range` inside precompiled STL and caught by `catch (const std::exception&)`,
 * 2. Cross-TU exception unwinding through intermediate RAII frames in exact reverse construction order,
 * 3. Multi-threaded `std::thread` concurrent `try`/`catch`/`throw` stress test (16 threads x 1,000 throws),
 * 4. Multiple inheritance (`flags == 0`) `catch (MiRight&)` non-zero base pointer adjustment and
 *    virtual diamond inheritance (`flags == 2`) `catch (VRoot&)` virtual base pointer adjustment.
 * @ingroup gcc_gen_be_tests
 */

#include <atomic>
#include <exception>
#include <stdexcept>
#include <thread>
#include <vector>
#include "test_stl_eh_hdr.h"

namespace {

/**
 * @brief Local RAII guard in TU A verifying cross-TU reverse destruction order.
 */
struct TuARaiiGuard {
    /** @brief Pointer to trace accumulator. */
    int *trace;
    /** @brief Step digit appended on destruction. */
    int step;

    /**
     * @brief Constructs `TuARaiiGuard`.
     * @param[in] t Trace pointer.
     * @param[in] s Step digit.
     */
    TuARaiiGuard(int *t, int s) noexcept : trace(t), step(s) {}

    /**
     * @brief Appends `step` to `*trace` on scope exit / unwind.
     */
    ~TuARaiiGuard() noexcept {
        if (trace) {
            *trace = (*trace * 10) + step;
        }
    }
};

/**
 * @brief Tests catching `std::out_of_range` thrown inside `std::vector<int>().at(5)` via `catch (const std::exception&)`.
 * @details Also verifies local RAII cleanup runs during unwinding out of precompiled STL code.
 * @param[out] out_ok Pointer receiving `1` on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
gcc_gen_be_error_t test_stl_vector_at_exception(int *out_ok) noexcept {
    *out_ok = 0;
    int raii_trace = 0;
    int caught = 0;
    try {
        TuARaiiGuard guard(&raii_trace, 4);
        std::vector<int> vec;
        int unreachable_v = vec.at(5);
        if (unreachable_v == 999) return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
    } catch (const std::exception &ex) {
        if (ex.what() != nullptr) {
            caught = 1;
        }
    }
    ASSERT_EQ(caught, 1);
    ASSERT_EQ(raii_trace, 4);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests cross-TU unwinding + RAII order + MI (`flags == 0`) & virtual diamond (`flags == 2`) base adjustment.
 * @details Verifies `catch (MiRight&)` adjusts `obj_ptr` to the second base subobject (`right_val == 222`)
 * and `catch (VRoot&)` adjusts `obj_ptr` via vtable vbase offset (`root_magic == 777`).
 * @param[out] out_ok Pointer receiving `1` on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
gcc_gen_be_error_t test_cross_tu_mi_and_diamond(int *out_ok) noexcept {
    *out_ok = 0;
    int trace = 0;
    long got_right = 0;
    try {
        TuARaiiGuard g1(&trace, 1);
        gcc_gen_be_error_t unused = throw_mi_across_tu(&trace);
        if (unused != GCC_GEN_BE_SUCCESS) return unused;
    } catch (MiRight &r_ref) {
        got_right = r_ref.right_val;
    }
    ASSERT_EQ(got_right, 222L);
    /* Destruction order: g3 (3) -> g2 (2) -> g1 (1) == 321 */
    ASSERT_EQ(trace, 321);

    int dtrace = 0;
    long got_root = 0;
    try {
        TuARaiiGuard g5(&dtrace, 5);
        gcc_gen_be_error_t unused = throw_diamond_across_tu(&dtrace);
        if (unused != GCC_GEN_BE_SUCCESS) return unused;
    } catch (VRoot &v_ref) {
        got_root = v_ref.root_magic;
    }
    ASSERT_EQ(got_root, 777L);
    ASSERT_EQ(dtrace, 95);

    int f3_trace = 0;
    long got_f3_root = 0;
    int caught_ambiguous = 0;
    try {
        TuARaiiGuard g6(&f3_trace, 6);
        gcc_gen_be_error_t unused = throw_diamond_flags3_across_tu(&f3_trace);
        if (unused != GCC_GEN_BE_SUCCESS) return unused;
    } catch (NonVirtRepeat &) {
        /* Repeated non-virtual base is ambiguous in C++ -> must NOT match! */
        caught_ambiguous = 1;
    } catch (VRoot &vr3) {
        got_f3_root = vr3.root_magic;
    }
    ASSERT_EQ(caught_ambiguous, 0);
    ASSERT_EQ(got_f3_root, 888L);
    ASSERT_EQ(f3_trace, 86);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Worker body for a single thread in the 16-thread x 1,000-throw concurrent stress test.
 * @details Throws and catches 1,000 distinct integer-tagged exceptions per thread while verifying
 * thread-local EH state isolation and per-iteration RAII destructor execution.
 * @param[in] tid Thread index `[0, 15]`.
 * @param[in,out] total_caught Atomic counter incremented on each verified catch.
 */
void stress_thread_worker(int tid, std::atomic<int> *total_caught) noexcept {
    int local_ok = 0;
    for (int iter = 0; iter < 1000; ++iter) {
        int dtor_ran = 0;
        int expected = (tid * 10000) + iter + 1;
        try {
            TuARaiiGuard g(&dtor_ran, 7);
            throw MiLeft(expected);
        } catch (const MiLeft &e) {
            if (e.left_val == expected && dtor_ran == 7) {
                ++local_ok;
            }
        }
    }
    total_caught->fetch_add(local_ok);
}

/**
 * @brief Runs 16 `std::thread`s concurrently executing 1,000 `try`/`catch`/`throw` cycles each.
 * @details Verifies all 16,000 concurrent throws unwind cleanly with zero thread-local state corruption.
 * @param[out] out_ok Pointer receiving `1` on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
gcc_gen_be_error_t test_multithreaded_eh_stress(int *out_ok) noexcept {
    *out_ok = 0;
    std::atomic<int> total_caught(0);
    std::vector<std::thread> threads;
    threads.reserve(16);
    for (int t = 0; t < 16; ++t) {
        threads.emplace_back(stress_thread_worker, t, &total_caught);
    }
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }
    ASSERT_EQ(total_caught.load(), 16000);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

} // namespace

/**
 * @brief Entry point for the native STL & multi-threaded C++ exception interop test suite.
 * @details Runs STL `vector::at`, cross-TU MI/diamond catch, and 16x1,000 `std::thread` stress tests.
 * @return `0` when all tests pass, `1` otherwise.
 */
int main() noexcept {
    int ok1 = 0;
    int ok2 = 0;
    int ok3 = 0;
    if (test_stl_vector_at_exception(&ok1) != GCC_GEN_BE_SUCCESS || !ok1) return 1;
    if (test_cross_tu_mi_and_diamond(&ok2) != GCC_GEN_BE_SUCCESS || !ok2) return 1;
    if (test_multithreaded_eh_stress(&ok3) != GCC_GEN_BE_SUCCESS || !ok3) return 1;
    return 0;
}
