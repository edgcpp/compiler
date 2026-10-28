/**
 * @file test_gcc_be_cache.cpp
 * @brief Unit tests for backend AST-to-JIT hash map caching (`gcc_gen_be_cache.c`).
 * @details Exercises `cache_insert`, `cache_lookup`, `cache_get_metrics`, `cache_clear`,
 * `cache_clear_all`, string caching (`GCC_GEN_BE_CACHE_STRING`), and bucket resizing.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_cache.h"
#include <stdio.h>
#include <stdlib.h>

extern "C" {
/**
 * @brief Mock diagnostic reporter for cache tests.
 * @details Returns the error code passed in.
 * @param[in] err Error code.
 * @param[in] msg Context message.
 * @return `err`.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char *msg) GCC_GEN_BE_NOEXCEPT {
    (void)msg;
    return err;
}
}

using namespace edg;

/**
 * @brief Executes all cache insertion, lookup, resize, and clear unit tests.
 * @details Verifies pointer and string keys, collision chaining, resizing past load factor, and metrics.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_cache_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    void *key1 = (void *)0x1000;
    void *val1 = (void *)0x2000;

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, key1, val1));

    void *out_val = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_TYPE, key1, &out_val));
    if (out_val != val1) return GCC_GEN_BE_ERROR_INTERNAL;

    void *key2 = (void *)0x1008;
    out_val = (void *)0x1;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_TYPE, key2, &out_val));
    if (out_val != NULL) return GCC_GEN_BE_ERROR_INTERNAL;

    size_t hits = 0, misses = 0;
    GCC_GEN_BE_CHECK(cache_get_metrics(GCC_GEN_BE_CACHE_TYPE, &hits, &misses));
    if (hits != 1 || misses != 1) return GCC_GEN_BE_ERROR_INTERNAL;

    for (int i = 1; i <= 100; ++i) {
        void *k = (void *)(size_t)(i * 16);
        void *v = (void *)(size_t)(i * 32);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, k, v));
    }

    for (int i = 1; i <= 100; ++i) {
        void *k = (void *)(size_t)(i * 16);
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, k, &out_val));
        if (out_val != (void *)(size_t)(i * 32)) return GCC_GEN_BE_ERROR_INTERNAL;
    }

    GCC_GEN_BE_CHECK(cache_get_metrics(GCC_GEN_BE_CACHE_VAR, &hits, &misses));
    if (hits != 100 || misses != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_VAR));
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)16, &out_val));
    if (out_val != NULL) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_SWITCH_CASE, key1, val1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_LABEL, key1, val1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, key1, val1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, key1, val1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_GUARD, key1, val1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"hello_key", val1));

    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)"hello_key", &out_val));
    if (out_val != val1) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(cache_clear_all());

    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_SWITCH_CASE, key1, &out_val));
    if (out_val != NULL) return GCC_GEN_BE_ERROR_INTERNAL;

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for cache unit tests.
 * @details Runs `run_cache_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_cache_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    puts("PASS");
    return 0;
}
