/**
 * @file gcc_gen_be_cache.c
 * @brief Implementation of cache management for the GCC backend.
 * @details Implements open-addressed hash tables mapping EDG IL pointers and symbol strings
 * to `libgccjit` handles with dynamic resizing and deterministic cleanup.
 * @ingroup gcc_gen_be
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
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
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
 * @details Uses bit shifting to compute a basic hash over `capacity > 0`.
 *
 * @param[in] key The pointer to hash.
 * @param[in] capacity The positive capacity of the map.
 * @param[out] out_hash Non-null pointer receiving the computed hash index.
 * @return GCC_GEN_BE_SUCCESS on success, or OOM on injected fault.
 */
static gcc_gen_be_error_t hash_key(void *key, size_t capacity, size_t *out_hash) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    /* Basic pointer hash: shift right by 3 to ignore alignment bits */
    size_t k = (size_t)key;
    *out_hash = ((k >> 3) ^ (k & 7u) ^ (k >> 48)) % capacity;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Computes a hash for a string key.
 *
 * @details Uses DJB2 hashing over `str` (or `0` when `str` is NULL).
 *
 * @param[in] str The string to hash (may be NULL).
 * @param[in] capacity The positive capacity of the map.
 * @param[out] out_hash Non-null pointer receiving the computed hash index.
 * @return GCC_GEN_BE_SUCCESS on success, or OOM on injected fault.
 */
static gcc_gen_be_error_t hash_string(const char *str, size_t capacity, size_t *out_hash) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (!str) { *out_hash = 0; return GCC_GEN_BE_SUCCESS; }
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
 * @details Dispatches to `hash_string` for `GCC_GEN_BE_CACHE_STRING` and `hash_key` otherwise.
 *
 * @param[in] map The cache map.
 * @param[in] key The key to hash.
 * @param[in] capacity The positive capacity of the map.
 * @param[out] out_hash Non-null pointer receiving the computed hash index.
 * @return GCC_GEN_BE_SUCCESS on success, or OOM on injected fault.
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
 * @details Compares string keys via `strcmp` (handling NULL) and pointer keys by address.
 *
 * @param[in] map The cache map.
 * @param[in] k1 First key.
 * @param[in] k2 Second key.
 * @param[out] out_equal Non-null pointer receiving TRUE if keys match.
 * @return GCC_GEN_BE_SUCCESS on success, or OOM on injected fault.
 */
static gcc_gen_be_error_t keys_equal(be_cache_map *map, void *k1, void *k2, a_boolean *out_equal) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
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
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_OOM.
 */
static gcc_gen_be_error_t cache_resize(be_cache_map *map) GCC_GEN_BE_NOEXCEPT {
    size_t new_capacity = map->capacity == 0 ? INITIAL_CACHE_CAPACITY : map->capacity * 2;
    void *raw_buckets = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(new_capacity * sizeof(be_cache_entry *), &raw_buckets));
    memset(raw_buckets, 0, new_capacity * sizeof(be_cache_entry *));
    be_cache_entry **new_buckets = (be_cache_entry **)raw_buckets;

    /* Rehash all elements */
    if (map->buckets) {
        be_cache_entry *detached = NULL;
        for (size_t i = 0; i < map->capacity; ++i) {
            be_cache_entry *e = map->buckets[i];
            while (e) {
                be_cache_entry *next = e->next;
                e->next = detached;
                detached = e;
                e = next;
            }
            map->buckets[i] = NULL;
        }
        while (detached) {
            be_cache_entry *next = detached->next;
            size_t new_hash = 0;
            gcc_gen_be_error_t err = hash_entry(map, detached->key, new_capacity, &new_hash);
            if (err != GCC_GEN_BE_SUCCESS) {
                /* Restore remaining entries into old bucket 0 so map remains valid and leak-free */
                while (detached) {
                    be_cache_entry *dn = detached->next;
                    detached->next = map->buckets[0];
                    map->buckets[0] = detached;
                    detached = dn;
                }
                for (size_t j = 0; j < new_capacity; ++j) {
                    be_cache_entry *mv = new_buckets[j];
                    while (mv) {
                        be_cache_entry *mn = mv->next;
                        mv->next = map->buckets[0];
                        map->buckets[0] = mv;
                        mv = mn;
                    }
                }
                free(new_buckets);
                return err;
            }
            detached->next = new_buckets[new_hash];
            new_buckets[new_hash] = detached;
            detached = next;
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

    if (!map->buckets) {
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

    if (map->buckets) {
        size_t existing_hash = 0;
        GCC_GEN_BE_CHECK(hash_entry(map, key, map->capacity, &existing_hash));
        for (be_cache_entry *cur = map->buckets[existing_hash]; cur != NULL; cur = cur->next) {
            a_boolean eq = FALSE;
            GCC_GEN_BE_CHECK(keys_equal(map, cur->key, key, &eq));
            if (eq) {
                cur->value = value;
                return GCC_GEN_BE_SUCCESS;
            }
        }
    }

    /* Resize if needed */
    if (!map->buckets || (double)map->size / (double)map->capacity >= MAX_LOAD_FACTOR) {
        GCC_GEN_BE_CHECK(cache_resize(map));
    }

    size_t hash = 0;
    GCC_GEN_BE_CHECK(hash_entry(map, key, map->capacity, &hash));
    void *raw_entry = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(sizeof(be_cache_entry), &raw_entry));
    be_cache_entry *e = (be_cache_entry *)raw_entry;

    if (map->type == GCC_GEN_BE_CACHE_STRING && key != NULL) {
        size_t slen = strlen((const char *)key) + 1U;
        void *dup_str = NULL;
        gcc_gen_be_error_t dup_err = gcc_gen_be_alloc(slen, &dup_str);
        if (dup_err != GCC_GEN_BE_SUCCESS) {
            free(e);
            return dup_err;
        }
        memcpy(dup_str, key, slen);
        e->key = dup_str;
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
                if (map->type == GCC_GEN_BE_CACHE_STRING) {
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
