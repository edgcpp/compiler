/**
 * @file test_coro_large_frame.cpp
 * @brief C++20 coroutine execution test with a 4,096-byte local array surviving across `co_await`.
 * @details Uses a custom promise `operator new` / `operator delete` with redzone guard canaries
 * immediately past the exact requested allocation size to verify that `gcc_gen_be_coro.c` allocates
 * the full frame size (`>= 4096` bytes) without any heap buffer overflow across suspension points!
 * @ingroup gcc_gen_be_tests
 */

#include <coroutine>
#include <cstddef>
#include <cstdlib>
#include "test_common.h"

namespace {

/** @brief Tracks the last byte count requested by the coroutine frame `operator new`. */
std::size_t g_last_coro_alloc_bytes = 0;

/** @brief Tracks whether any redzone canary byte past the coroutine frame was overwritten. */
int g_redzone_corrupted = 0;

/** @brief Tracks balanced coroutine frame deallocation. */
int g_coro_freed_count = 0;

/**
 * @brief Task coroutine return type with redzone-checked heap frame allocation.
 */
struct LargeFrameTask {
    /**
     * @brief Coroutine promise type instrumenting `operator new` and `operator delete` with 64-byte redzones.
     */
    struct promise_type {
        /** @brief Final integer result produced by `co_return`. */
        int result_value = 0;

        /**
         * @brief Allocates `sz` bytes flanked by 64-byte `0xA5` redzone canaries.
         * @param[in] sz Requested coroutine frame size in bytes.
         * @return Pointer to the usable coroutine frame payload.
         */
        static void *operator new(std::size_t sz) noexcept {
            g_last_coro_alloc_bytes = sz;
            unsigned char *raw = static_cast<unsigned char *>(std::malloc(sz + 128U));
            if (!raw) return nullptr;
            std::size_t *hdr = reinterpret_cast<std::size_t *>(raw);
            hdr[0] = sz;
            for (std::size_t i = 0; i < 64U; ++i) {
                raw[64U + sz + i] = 0xA5U;
            }
            return raw + 64U;
        }

        /**
         * @brief Verifies all 64 trailing redzone canary bytes are intact (`0xA5`) before freeing.
         * @param[in] ptr Coroutine frame pointer.
         */
        static void operator delete(void *ptr) noexcept {
            if (!ptr) return;
            unsigned char *raw = static_cast<unsigned char *>(ptr) - 64U;
            std::size_t sz = reinterpret_cast<std::size_t *>(raw)[0];
            for (std::size_t i = 0; i < 64U; ++i) {
                if (raw[64U + sz + i] != 0xA5U) {
                    g_redzone_corrupted = 1;
                }
            }
            ++g_coro_freed_count;
            std::free(raw);
        }

        /**
         * @brief Constructs the ramp return object from this promise.
         * @return `LargeFrameTask` owning the coroutine handle.
         */
        LargeFrameTask get_return_object() noexcept {
            return LargeFrameTask{ std::coroutine_handle<promise_type>::from_promise(*this) };
        }

        /**
         * @brief Suspends initially so caller controls resumption.
         * @return `std::suspend_always`.
         */
        std::suspend_always initial_suspend() noexcept { return {}; }

        /**
         * @brief Suspends at final suspension so caller can read `result_value` before `destroy()`.
         * @return `std::suspend_always`.
         */
        std::suspend_always final_suspend() noexcept { return {}; }

        /**
         * @brief Records unhandled exception (unused under normal execution).
         */
        void unhandled_exception() noexcept {}

        /**
         * @brief Stores the `co_return` value into `result_value`.
         * @param[in] v Returned integer checksum.
         */
        void return_value(int v) noexcept { result_value = v; }
    };

    /** @brief Underlying C++20 coroutine handle. */
    std::coroutine_handle<promise_type> handle;
};

/**
 * @brief Coroutine owning `char buf[4096]` across two `co_await` suspension points.
 * @details Writes all 4,096 bytes before the first `co_await`, mutates across a second `co_await`,
 * and returns the checksum across `buf[0]`, `buf[2047]`, and `buf[4095]`.
 * @param[in] seed Initial byte seed.
 * @return `LargeFrameTask` coroutine handle.
 */
LargeFrameTask run_large_buffer_coro(int seed) noexcept {
    char buf[4096];
    for (int i = 0; i < 4096; ++i) {
        buf[i] = static_cast<char>((i + seed) & 0x7F);
    }
    co_await std::suspend_always{};
    for (int i = 0; i < 4096; ++i) {
        buf[i] = static_cast<char>((buf[i] + 3) & 0x7F);
    }
    co_await std::suspend_always{};
    int checksum = static_cast<int>(buf[0]) + static_cast<int>(buf[2047]) + static_cast<int>(buf[4095]);
    co_return checksum;
}

/**
 * @brief Executes `run_large_buffer_coro` to completion and verifies frame size and redzone integrity.
 * @param[out] out_ok Pointer receiving `1` when all assertions succeed.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
gcc_gen_be_error_t verify_large_coro(int *out_ok) noexcept {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    LargeFrameTask task = run_large_buffer_coro(5);
    ASSERT_TRUE(g_last_coro_alloc_bytes >= 4096U);

    task.handle.resume();
    ASSERT_FALSE(task.handle.done());
    task.handle.resume();
    ASSERT_FALSE(task.handle.done());
    task.handle.resume();
    ASSERT_TRUE(task.handle.done());

    /* buf[0] = (5+3)&127 = 8; buf[2047] = ((2052)&127 + 3)&127 = 7; buf[4095] = ((4100)&127 + 3)&127 = 7 -> 22 */
    ASSERT_EQ(task.handle.promise().result_value, 22);
    task.handle.destroy();

    ASSERT_EQ(g_redzone_corrupted, 0);
    ASSERT_EQ(g_coro_freed_count, 1);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

} // namespace

/**
 * @brief Entry point for `test_coro_large_frame.cpp`.
 * @details Runs `verify_large_coro` and returns `0` on success.
 * @return `0` on success, `1` on failure.
 */
int main() noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = verify_large_coro(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok == 1) ? 0 : 1;
}
