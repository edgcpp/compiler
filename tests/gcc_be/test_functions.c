/**
 * @file test_functions.c
 * @brief End-to-end C test for function pointers and error-percolating out-parameter calls.
 * @details Verifies indirect calls through function pointers returning `gcc_gen_be_error_t` with out-parameters.
 */

#include "test_common.h"

/**
 * @brief Adds two integers and stores the sum in `out_sum`.
 * @details Validates `out_sum` and writes `a + b`.
 * @param[in] a First integer operand.
 * @param[in] b Second integer operand.
 * @param[out] out_sum Pointer to receive the computed sum.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_sum` is NULL.
 */
gcc_gen_be_error_t add(int a, int b, int *out_sum) GCC_GEN_BE_NOEXCEPT {
    if (!out_sum) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_sum = a + b;
    return GCC_GEN_BE_SUCCESS;
}

/** @brief Mixed integer and double 16-byte struct for register coercion testing. */
struct MixedPair {
    /** @brief Integer eightbyte field. */
    long a;
    /** @brief Floating-point eightbyte field. */
    double b;
};

/** @brief Homogeneous floating-point 3-element struct for HFA coercion testing. */
struct Vec3f {
    /** @brief First float component. */
    float x;
    /** @brief Second float component. */
    float y;
    /** @brief Third float component. */
    float z;
};

/** @brief Small 4-byte struct for register coercion testing. */
struct Small4 {
    /** @brief 32-bit integer value. */
    int v;
};

/**
 * @brief Bounces a `MixedPair` value across a function call boundary.
 * @details Tests register coercion of `{ long, double }` parameters and return values.
 * @param[in] p Input `MixedPair` passed by value.
 * @param[out] out_res Pointer to receive the modified `MixedPair`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t bounce_mixed_pair(struct MixedPair p, struct MixedPair *out_res) GCC_GEN_BE_NOEXCEPT {
    if (!out_res) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    p.a += 10;
    p.b += 1.5;
    *out_res = p;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Bounces a `Vec3f` HFA struct across a function call boundary.
 * @details Tests register coercion of `{ float, float, float }` parameters.
 * @param[in] v Input `Vec3f` passed by value.
 * @param[out] out_sum Pointer to receive the sum of components as `int`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t sum_vec3f(struct Vec3f v, int *out_sum) GCC_GEN_BE_NOEXCEPT {
    if (!out_sum) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_sum = (int)(v.x + v.y + v.z);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Bounces a 4-byte `Small4` struct across a function call boundary.
 * @details Tests register coercion of small `<= 8` byte structs.
 * @param[in] s Input `Small4` passed by value.
 * @param[out] out_val Pointer to receive `s.v * 2`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t double_small4(struct Small4 s, int *out_val) GCC_GEN_BE_NOEXCEPT {
    if (!out_val) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_val = s.v * 2;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes function pointer and register coercion assertions.
 * @details Percolates all call errors via `GCC_GEN_BE_CHECK` and `ASSERT_EQ`.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_functions_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_gen_be_error_t (*fn)(int, int, int *) = add;
    int res = 0;
    GCC_GEN_BE_CHECK(fn(2, 3, &res));
    ASSERT_EQ(res, 5);

    struct MixedPair mp = { 30, 0.5 };
    struct MixedPair mp_out = { 0, 0.0 };
    GCC_GEN_BE_CHECK(bounce_mixed_pair(mp, &mp_out));
    ASSERT_EQ(mp_out.a, 40);
    ASSERT_EQ((int)mp_out.b, 2);

    struct Vec3f v3 = { 10.0f, 20.0f, 12.0f };
    int v3_sum = 0;
    GCC_GEN_BE_CHECK(sum_vec3f(v3, &v3_sum));
    ASSERT_EQ(v3_sum, 42);

    struct Small4 s4 = { 21 };
    int s4_val = 0;
    GCC_GEN_BE_CHECK(double_small4(s4, &s4_val));
    ASSERT_EQ(s4_val, 42);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point.
 * @details Runs function pointer invocation and struct register coercion tests and verifies results.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_functions_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
