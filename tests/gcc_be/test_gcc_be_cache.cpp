/**
 * @file test_gcc_be_cache.cpp
 * @brief Unit tests for backend AST-to-JIT hash map caching (`gcc_gen_be_cache.c`).
 * @details Exercises `cache_insert`, `cache_lookup`, `cache_get_metrics`, `cache_clear`,
 * `cache_clear_all`, string caching (`GCC_GEN_BE_CACHE_STRING`), and bucket resizing.
 * @ingroup gcc_gen_be_tests
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

USING_NAMESPACE_EDG

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
    void *out_val = NULL;
    size_t hits = 0, misses = 0;

    /* Invalid arguments and invalid cache_type */
    if (cache_lookup(GCC_GEN_BE_CACHE_TYPE, key1, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (cache_lookup((gcc_gen_be_cache_type_t)999, key1, &out_val) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (cache_insert((gcc_gen_be_cache_type_t)999, key1, val1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (cache_clear((gcc_gen_be_cache_type_t)999) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (cache_get_metrics(GCC_GEN_BE_CACHE_TYPE, NULL, &misses) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (cache_get_metrics(GCC_GEN_BE_CACHE_TYPE, &hits, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (cache_get_metrics((gcc_gen_be_cache_type_t)999, &hits, &misses) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, key1, val1));
    /* Update existing key */
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, key1, (void *)0x2001));
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_TYPE, key1, &out_val));
    if (out_val != (void *)0x2001) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Collision in same bucket (key1 + 64*8 hashes to same bucket mod 64) */
    void *coll_key = (void *)(0x1000 + 64 * 8);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, coll_key, val1));
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_TYPE, key1, &out_val));

    void *key2 = (void *)0x1008;
    out_val = (void *)0x1;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_TYPE, key2, &out_val));
    if (out_val != NULL) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(cache_get_metrics(GCC_GEN_BE_CACHE_TYPE, &hits, &misses));
    if (hits != 2 || misses != 1) return GCC_GEN_BE_ERROR_INTERNAL;

    /* String cache: bucket-0 non-NULL key ("a:") vs NULL key, non-NULL keys, and OOM sweeps */
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"a:", (void *)0x2999));
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_STRING, NULL, &out_val));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, NULL, (void *)0x3000));
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_STRING, NULL, &out_val));
    if (out_val != (void *)0x3000) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)"a:", &out_val));
    for (int s = 0; s < 80; ++s) {
        char buf[32];
        snprintf(buf, sizeof(buf), "str_key_%d", s);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)buf, val1));
    }
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_STRING, NULL, &out_val));
    for (int b = 0; b < 12; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"oom_str", val1);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 12; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)"oom_str", &out_val);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* Resize & rehash OOM sweep on GCC_GEN_BE_CACHE_VAR (49th insert when size==48 triggers resize 64 -> 128) */
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_VAR));
    for (int i = 1; i <= 48; ++i) {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)(size_t)(i * 16), (void *)(size_t)(i * 32)));
    }
    for (int b = 0; b < 60; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)(size_t)(49 * 16), (void *)(size_t)(49 * 32));
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int i = 50; i <= 100; ++i) {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)(size_t)(i * 16), (void *)(size_t)(i * 32)));
    }
    for (int i = 1; i <= 100; ++i) {
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)(size_t)(i * 16), &out_val));
        if (out_val != (void *)(size_t)(i * 32)) return GCC_GEN_BE_ERROR_INTERNAL;
    }

    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_VAR));
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)16, &out_val));
    if (out_val != NULL) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_SWITCH_CASE, key1, val1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_LABEL, key1, val1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, key1, val1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, key1, val1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_GUARD, key1, val1));

    /* Exercise all 8 GCC_GEN_BE_CHECK(cache_clear(...)) branches in cache_clear_all */
    for (int b = 0; b <= 10; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = cache_clear_all();
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    g_gcc_be_oom_countdown = 0;
    if (cache_get_metrics(GCC_GEN_BE_CACHE_TYPE, &hits, &misses) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;

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
int main(void) GCC_GEN_BE_NOEXCEPT {
    int ok = 0;
    gcc_gen_be_error_t err = run_cache_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    puts("PASS");
    return 0;
}
