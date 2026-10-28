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




/** @brief Initial bucket capacity for newly allocated backend hash maps. */
#define INITIAL_CACHE_CAPACITY 64
/** @brief Maximum load factor threshold before doubling hash map bucket capacity. */
#define MAX_LOAD_FACTOR 0.75

/** @brief Cache instance. */
static be_cache_map type_cache = {NULL, 0, 0, 0, 0, GCC_GEN_BE_CACHE_TYPE};
/** @brief Cache instance. */
static be_cache_map var_cache = {NULL, 0, 0, 0, 0, GCC_GEN_BE_CACHE_VAR};
/** @brief Cache instance. */
static be_cache_map func_cache = {NULL, 0, 0, 0, 0, GCC_GEN_BE_CACHE_FUNC};
/** @brief Cache instance. */
static be_cache_map label_cache = {NULL, 0, 0, 0, 0, GCC_GEN_BE_CACHE_LABEL};
/** @brief Cache instance. */
static be_cache_map field_cache = {NULL, 0, 0, 0, 0, GCC_GEN_BE_CACHE_FIELD};
/** @brief Cache instance. */
static be_cache_map switch_case_cache = {NULL, 0, 0, 0, 0, GCC_GEN_BE_CACHE_SWITCH_CASE};
/** @brief Cache instance. */
static be_cache_map guard_cache = {NULL, 0, 0, 0, 0, GCC_GEN_BE_CACHE_GUARD};
/** @brief Cache instance. */
static be_cache_map string_cache = {NULL, 0, 0, 0, 0, GCC_GEN_BE_CACHE_STRING};

/**
 * @brief Retrieves the requested cache map.
 *
 * @details Returns a pointer to the specific cache instance requested.
 *
 * @param[in] cache_type The type of cache to retrieve.
 * @param[out] out_cache Pointer to receive the cache map.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t get_cache_map(gcc_gen_be_cache_type_t cache_type, be_cache_map **out_cache) GCC_GEN_BE_NOEXCEPT {
    if (!out_cache) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    switch (cache_type) {
        case GCC_GEN_BE_CACHE_TYPE: *out_cache = &type_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_VAR: *out_cache = &var_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_FUNC: *out_cache = &func_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_LABEL: *out_cache = &label_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_FIELD: *out_cache = &field_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_SWITCH_CASE: *out_cache = &switch_case_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_GUARD: *out_cache = &guard_cache; return GCC_GEN_BE_SUCCESS;
        case GCC_GEN_BE_CACHE_STRING: *out_cache = &string_cache; return GCC_GEN_BE_SUCCESS;
        default: return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
}

/**
 * @brief Computes a hash for a pointer key.
 *
 * @details Uses bit shifting to compute a basic hash.
 *
 * @param[in] key The pointer to hash.
 * @param[in] capacity The capacity of the map.
 * @param[out] out_hash The computed hash index.
 * @return GCC_GEN_BE_SUCCESS.
 */
static gcc_gen_be_error_t hash_key(void *key, size_t capacity, size_t *out_hash) GCC_GEN_BE_NOEXCEPT {
    if (!out_hash) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (capacity == 0) { *out_hash = 0; return GCC_GEN_BE_SUCCESS; }
    /* Basic pointer hash: shift right by 3 to ignore alignment bits */
    size_t k = (size_t)key;
    *out_hash = (k >> 3) % capacity;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Computes a hash for a string key.
 *
 * @param[in] str The string to hash.
 * @param[in] capacity The capacity of the map.
 * @param[out] out_hash The computed hash index.
 * @return GCC_GEN_BE_SUCCESS.
 */
static gcc_gen_be_error_t hash_string(const char *str, size_t capacity, size_t *out_hash) GCC_GEN_BE_NOEXCEPT {
    if (!out_hash) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (!str || capacity == 0) { *out_hash = 0; return GCC_GEN_BE_SUCCESS; }
    size_t hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + (size_t)c;
    }
    *out_hash = hash % capacity;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Computes hash for an entry depending on cache map type.
 *
 * @param[in] map The cache map.
 * @param[in] key The key to hash.
 * @param[in] capacity The capacity of the map.
 * @param[out] out_hash The computed hash index.
 * @return GCC_GEN_BE_SUCCESS.
 */
static gcc_gen_be_error_t hash_entry(be_cache_map *map, void *key, size_t capacity, size_t *out_hash) GCC_GEN_BE_NOEXCEPT {
    if (map->type == GCC_GEN_BE_CACHE_STRING) {
        return hash_string((const char *)key, capacity, out_hash);
    }
    return hash_key(key, capacity, out_hash);
}

/**
 * @brief Tests if two keys are equal for a given cache map.
 *
 * @param[in] map The cache map.
 * @param[in] k1 First key.
 * @param[in] k2 Second key.
 * @param[out] out_equal TRUE if keys match.
 * @return GCC_GEN_BE_SUCCESS.
 */
static gcc_gen_be_error_t keys_equal(be_cache_map *map, void *k1, void *k2, a_boolean *out_equal) GCC_GEN_BE_NOEXCEPT {
    if (!out_equal) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (map->type == GCC_GEN_BE_CACHE_STRING) {
        if (!k1 || !k2) { *out_equal = (k1 == k2); return GCC_GEN_BE_SUCCESS; }
        *out_equal = (strcmp((const char *)k1, (const char *)k2) == 0);
        return GCC_GEN_BE_SUCCESS;
    }
    *out_equal = (k1 == k2);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Resizes the cache map.
 *
 * @details Expands the map capacity and rehashes elements.
 *
 * @param[in,out] map The map to resize.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
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
                size_t new_hash = 0;
                gcc_gen_be_error_t err = hash_entry(map, e->key, new_capacity, &new_hash);
                if (err != GCC_GEN_BE_SUCCESS) {
                    free(new_buckets);
                    return err;
                }
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

    size_t hash = 0;
    GCC_GEN_BE_CHECK(hash_entry(map, key, map->capacity, &hash));
    be_cache_entry *e = map->buckets[hash];
    while (e) {
        a_boolean eq = FALSE;
        GCC_GEN_BE_CHECK(keys_equal(map, e->key, key, &eq));
        if (eq) {
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

    size_t hash = 0;
    GCC_GEN_BE_CHECK(hash_entry(map, key, map->capacity, &hash));
    be_cache_entry *e = (be_cache_entry *)malloc(sizeof(be_cache_entry));
    if (!e) return GCC_GEN_BE_ERROR_OOM;
    
    if (map->type == GCC_GEN_BE_CACHE_STRING) {
        e->key = (void *)strdup((const char *)key);
    } else {
        e->key = key;
    }
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
                if (map->type == GCC_GEN_BE_CACHE_STRING && e->key) {
                    free(e->key);
                }
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
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_GUARD));
    GCC_GEN_BE_CHECK(cache_clear(GCC_GEN_BE_CACHE_STRING));
    
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
