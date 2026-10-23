/**
 * @file test_types.c
 * @brief End-to-end C test for basic struct type lowering and aggregate field access.
 * @details Verifies `struct Foo` layout and member read access (`foo.x`).
 */

#include "test_common.h"

/** @brief Basic two-field struct for type lowering verification. */
struct Foo {
    /** @brief Integer field `x`. */
    int x;
    /** @brief Floating-point field `y`. */
    float y;
};

/**
 * @brief Executes basic struct layout and member access verification.
 * @details Initializes `struct Foo` and verifies `foo.x == 1`.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_types_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    struct Foo foo = { 1, 2.0f };
    ASSERT_EQ(foo.x, 1);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_types.c`.
 * @details Invokes `run_types_test` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_types_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
