/**
 * @file gcc_gen_be_cache.c
 * @brief Implementation of cache management for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "fe_common.h"
#include "gcc_gen_be_cache.h"
#include <stdlib.h>
#include <string.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE




#define INITIAL_CACHE_CAPACITY 64
#define MAX_LOAD_FACTOR 0.75

static be_cache_map type_cache = {NULL, 0, 0, 0, 0};
static be_cache_map var_cache = {NULL, 0, 0, 0, 0};
static be_cache_map func_cache = {NULL, 0, 0, 0, 0};
static be_cache_map label_cache = {NULL, 0, 0, 0, 0};
static be_cache_map field_cache = {NULL, 0, 0, 0, 0};
static be_cache_map switch_case_cache = {NULL, 0, 0, 0, 0};

static gcc_gen_be_error_t get_cache_map(gcc_gen_be_cache_type_t cache_type, be_cache_map **out_cache) GCC_GEN_BE_NOEXCEPT {
    if (!out_cache) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    switch (cache_type) {
        case GCC_GEN_BE_CACHE_TYPE: *out_cache = &type_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_VAR: *out_cache = &var_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_FUNC: *out_cache = &func_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_LABEL: *out_cache = &label_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_FIELD: *out_cache = &field_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_SWITCH_CASE: *out_cache = &switch_case_cache; return GCC_GEN_BE_SUCCESS;
        default: return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
}

static size_t hash_key(void *key, size_t capacity) {
    if (capacity == 0) return 0;
    /* Basic pointer hash: shift right by 3 to ignore alignment bits */
    size_t k = (size_t)key;
    return (k >> 3) % capacity;
}

static gcc_gen_be_error_t cache_resize(be_cache_map *map) GCC_GEN_BE_NOEXCEPT {
    size_t new_capacity = map->capacity == 0 ? INITIAL_CACHE_CAPACITY : map->capacity * 2;
    be_cache_entry **new_buckets = (be_cache_entry **)calloc(new_capacity, sizeof(be_cache_entry *));
    if (!new_buckets) return GCC_GEN_BE_ERROR_OOM;

    /* Rehash all elements */
    if (map->buckets) {
        for (size_t i = 0; i < map->capacity; ++i) {
            be_cache_entry *e = map->buckets[i];
            while (e) {
                be_cache_entry *next = e->next;
                size_t new_hash = hash_key(e->key, new_capacity);
                e->next = new_buckets[new_hash];
                new_buckets[new_hash] = e;
                e = next;
            }
        }
        free(map->buckets);
    }
    
    map->buckets = new_buckets;
    map->capacity = new_capacity;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t cache_lookup(gcc_gen_be_cache_type_t cache_type, void *key, void **out_value) GCC_GEN_BE_NOEXCEPT {
    if (!out_value) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_value = NULL;

    be_cache_map *map = NULL;
    GCC_GEN_BE_CHECK(get_cache_map(cache_type, &map));
    
    if (map->capacity == 0 || !map->buckets) {
        map->misses++;
        return GCC_GEN_BE_SUCCESS;
    }

    size_t hash = hash_key(key, map->capacity);
    be_cache_entry *e = map->buckets[hash];
    while (e) {
        if (e->key == key) {
            *out_value = e->value;
            map->hits++;
            return GCC_GEN_BE_SUCCESS;
        }
        e = e->next;
    }

    map->misses++;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t cache_insert(gcc_gen_be_cache_type_t cache_type, void *key, void *value) GCC_GEN_BE_NOEXCEPT {
    be_cache_map *map = NULL;
    GCC_GEN_BE_CHECK(get_cache_map(cache_type, &map));
    
    /* Resize if needed */
    if (map->capacity == 0 || (double)map->size / (double)map->capacity >= MAX_LOAD_FACTOR) {
        GCC_GEN_BE_CHECK(cache_resize(map));
    }

    size_t hash = hash_key(key, map->capacity);
    be_cache_entry *e = (be_cache_entry *)malloc(sizeof(be_cache_entry));
    if (!e) return GCC_GEN_BE_ERROR_OOM;
    
    e->key = key;
    e->value = value;
    e->next = map->buckets[hash];
    map->buckets[hash] = e;
    map->size++;
    
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t cache_clear(gcc_gen_be_cache_type_t cache_type) GCC_GEN_BE_NOEXCEPT {
    be_cache_map *map = NULL;
    GCC_GEN_BE_CHECK(get_cache_map(cache_type, &map));

    if (map->buckets) {
        for (size_t i = 0; i < map->capacity; ++i) {
            be_cache_entry *e = map->buckets[i];
            while (e) {
                be_cache_entry *next = e->next;
                free(e);
                e = next;
            }
        }
        free(map->buckets);
        map->buckets = NULL;
    }
    
    map->capacity = 0;
    map->size = 0;
    map->hits = 0;
    map->misses = 0;
    
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t cache_clear_all(void) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_TYPE));
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_VAR));
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_FUNC));
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_LABEL));
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_FIELD));
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_SWITCH_CASE));
    
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t cache_get_metrics(gcc_gen_be_cache_type_t cache_type, size_t *out_hits, size_t *out_misses) GCC_GEN_BE_NOEXCEPT {
    if (!out_hits || !out_misses) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    be_cache_map *map = NULL;
    GCC_GEN_BE_CHECK(get_cache_map(cache_type, &map));
    *out_hits = map->hits;
    *out_misses = map->misses;
    return GCC_GEN_BE_SUCCESS;
}



END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
