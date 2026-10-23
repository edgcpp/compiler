/**
 * @file test_common.h
 * @brief Common assertion macros and error-percolation utilities for GCC backend tests.
 * @details Provides centralized assertion macros (`ASSERT_EQ`, `ASSERT_NE`, `ASSERT_TRUE`,
 * `ASSERT_FALSE`, `ASSERT_SUCCESS`) that return `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`
 * and percolate `gcc_gen_be_error_t` status codes via `GCC_GEN_BE_CHECK`.
 */

#ifndef GCC_GEN_BE_TEST_COMMON_H
#define GCC_GEN_BE_TEST_COMMON_H

#include "gcc_gen_be_error.h"

/**
 * @brief Asserts that two values compare equal, returning `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED` otherwise.
 * @details Evaluates `actual` and `expected` once and returns `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED` on mismatch.
 * @param[in] actual The actual evaluated value.
 * @param[in] expected The expected target value.
 */
#define ASSERT_EQ(actual, expected) do { if ((actual) != (expected)) { return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED; } } while (0)

/**
 * @brief Asserts that two values compare unequal, returning `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED` otherwise.
 * @details Evaluates `actual` and `unexpected` once and returns `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED` if equal.
 * @param[in] actual The actual evaluated value.
 * @param[in] unexpected The value that `actual` must not equal.
 */
#define ASSERT_NE(actual, unexpected) do { if ((actual) == (unexpected)) { return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED; } } while (0)

/**
 * @brief Asserts that a boolean condition is truthy, returning `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED` otherwise.
 * @details Evaluates `cond` and returns `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED` if false.
 * @param[in] cond The boolean expression to test.
 */
#define ASSERT_TRUE(cond) do { if (!(cond)) { return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED; } } while (0)

/**
 * @brief Asserts that a boolean condition is falsy, returning `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED` otherwise.
 * @details Evaluates `cond` and returns `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED` if true.
 * @param[in] cond The boolean expression to test.
 */
#define ASSERT_FALSE(cond) do { if (cond) { return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED; } } while (0)

/**
 * @brief Asserts that an expression returning `gcc_gen_be_error_t` succeeds (`GCC_GEN_BE_SUCCESS`).
 * @details Delegates to `GCC_GEN_BE_CHECK` so any non-success error code is immediately percolated.
 * @param[in] expr The expression returning `gcc_gen_be_error_t`.
 */
#define ASSERT_SUCCESS(expr) GCC_GEN_BE_CHECK(expr)

#endif /* GCC_GEN_BE_TEST_COMMON_H */
