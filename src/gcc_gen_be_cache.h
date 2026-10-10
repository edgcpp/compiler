/**
 * @file gcc_gen_be_cache.h
 * @brief Cache management for the GCC backend.
 *
 * This file declares the cache structure and functions used to map frontend
 * AST nodes to their backend libgccjit equivalents (e.g., types, variables, functions).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_CACHE_H
#define GCC_GEN_BE_CACHE_H

#include "gcc_gen_be_error.h"
#include <stddef.h>
#include "fe_common.h"

BEGIN_EDG_NAMESPACE

/**
 * @struct be_cache_entry
 * @brief Represents a single entry in a hash map cache.
 */
typedef struct be_cache_entry {
    void *key;                      /**< The key, typically an AST node pointer. */
    void *value;                    /**< The mapped value, typically a libgccjit object. */
    struct be_cache_entry *next;    /**< Pointer to the next entry in case of a hash collision. */
} be_cache_entry;

/**
 * @struct be_cache_map
 * @brief Represents a dynamically resizing hash map cache.
 */
typedef struct {
    be_cache_entry **buckets;       /**< Array of bucket pointers. */
    size_t capacity;                /**< Number of buckets in the map. */
    size_t size;                    /**< Number of items stored in the map. */
    size_t hits;                    /**< Metric: Number of successful lookups. */
    size_t misses;                  /**< Metric: Number of unsuccessful lookups. */
} be_cache_map;

/**
 * @brief Enum for identifying specific global caches.
 */
typedef enum {
    GCC_GEN_BE_CACHE_TYPE,          /**< Cache for types. */
    GCC_GEN_BE_CACHE_VAR,           /**< Cache for variables. */
    GCC_GEN_BE_CACHE_FUNC,          /**< Cache for functions. */
    GCC_GEN_BE_CACHE_LABEL,         /**< Cache for labels. */
    GCC_GEN_BE_CACHE_FIELD,         /**< Cache for fields. */
    GCC_GEN_BE_CACHE_SWITCH_CASE    /**< Cache for switch cases. */
} gcc_gen_be_cache_type_t;

/**
 * @brief Looks up a value in a specified cache.
 *
 * @param cache_type The type of cache to search.
 * @param key The key to look up.
 * @param out_value A pointer to a void pointer that will receive the value.
 * @return GCC_GEN_BE_SUCCESS if lookup logic completed (with out_value potentially NULL if not found), or an error.
 */
extern gcc_gen_be_error_t cache_lookup(gcc_gen_be_cache_type_t cache_type, void *key, void **out_value) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Inserts a key-value pair into a specified cache.
 *
 * @param cache_type The type of cache to modify.
 * @param key The key to insert.
 * @param value The value to associate with the key.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code (e.g., GCC_GEN_BE_ERROR_OOM).
 */
extern gcc_gen_be_error_t cache_insert(gcc_gen_be_cache_type_t cache_type, void *key, void *value) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Clears a specified cache, freeing all entries and resetting metrics.
 *
 * @param cache_type The type of cache to clear.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t cache_clear(gcc_gen_be_cache_type_t cache_type) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Clears all global caches.
 *
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t cache_clear_all(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Retrieves the metrics for a specified cache.
 *
 * @param cache_type The type of cache to query.
 * @param out_hits Pointer to receive the number of hits.
 * @param out_misses Pointer to receive the number of misses.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t cache_get_metrics(gcc_gen_be_cache_type_t cache_type, size_t *out_hits, size_t *out_misses) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_CACHE_H */
