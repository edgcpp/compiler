#include "basic_hdrs.h"
#include "gcc_gen_be_cache.h"
#include <stdio.h>
#include <stdlib.h>

extern "C" {
    gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char* msg) GCC_GEN_BE_NOEXCEPT {
        return GCC_GEN_BE_SUCCESS;
    }
}

int main() {
    gcc_gen_be_error_t err;

    // Test basic insertion and lookup
    void *key1 = (void *)0x1000;
    void *val1 = (void *)0x2000;
    
    err = cache_insert(GCC_GEN_BE_CACHE_TYPE, key1, val1);
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    void *out_val = NULL;
    err = cache_lookup(GCC_GEN_BE_CACHE_TYPE, key1, &out_val);
    if (err != GCC_GEN_BE_SUCCESS || out_val != val1) return 1;

    // Test missing key
    void *key2 = (void *)0x1008;
    out_val = (void *)0x1;
    err = cache_lookup(GCC_GEN_BE_CACHE_TYPE, key2, &out_val);
    if (err != GCC_GEN_BE_SUCCESS || out_val != NULL) return 1;

    // Test metrics
    size_t hits = 0, misses = 0;
    err = cache_get_metrics(GCC_GEN_BE_CACHE_TYPE, &hits, &misses);
    if (err != GCC_GEN_BE_SUCCESS || hits != 1 || misses != 1) return 1;

    // Test cache resize and collisions
    // We insert more than INITIAL_CACHE_CAPACITY * MAX_LOAD_FACTOR items (64 * 0.75 = 48)
    for (int i = 1; i <= 100; ++i) {
        void *k = (void *)(size_t)(i * 16);
        void *v = (void *)(size_t)(i * 32);
        err = cache_insert(GCC_GEN_BE_CACHE_VAR, k, v);
        if (err != GCC_GEN_BE_SUCCESS) return 1;
    }

    // Verify all 100 items can be found
    for (int i = 1; i <= 100; ++i) {
        void *k = (void *)(size_t)(i * 16);
        err = cache_lookup(GCC_GEN_BE_CACHE_VAR, k, &out_val);
        if (err != GCC_GEN_BE_SUCCESS || out_val != (void *)(size_t)(i * 32)) return 1;
    }

    err = cache_get_metrics(GCC_GEN_BE_CACHE_VAR, &hits, &misses);
    if (err != GCC_GEN_BE_SUCCESS || hits != 100 || misses != 0) return 1;

    // Test clear
    err = cache_clear(GCC_GEN_BE_CACHE_VAR);
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    err = cache_lookup(GCC_GEN_BE_CACHE_VAR, (void*)16, &out_val);
    if (err != GCC_GEN_BE_SUCCESS || out_val != NULL) return 1; // Should be missing

    // Test clear all
    err = cache_insert(GCC_GEN_BE_CACHE_SWITCH_CASE, key1, val1);
    err = cache_insert(GCC_GEN_BE_CACHE_LABEL, key1, val1);
    err = cache_insert(GCC_GEN_BE_CACHE_FIELD, key1, val1);
    err = cache_insert(GCC_GEN_BE_CACHE_FUNC, key1, val1);
    
    err = cache_clear_all();
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    err = cache_lookup(GCC_GEN_BE_CACHE_SWITCH_CASE, key1, &out_val);
    if (err != GCC_GEN_BE_SUCCESS || out_val != NULL) return 1;

    printf("PASS\n");
    return 0;
}