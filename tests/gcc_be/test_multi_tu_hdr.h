/**
 * @file test_multi_tu_hdr.h
 * @brief Shared polymorphic class and namespace header for multi-TU GCC backend execution testing.
 * @details Defines polymorphic classes `MultiTuBase` and `MultiTuDerived` (emitting weak/linkonce
 * `_ZTV` and `_ZTI` globals in multiple translation units) and `namespace multi_ns` declarations.
 * @ingroup gcc_gen_be_tests
 */

#ifndef GCC_BE_TEST_MULTI_TU_HDR_H
#define GCC_BE_TEST_MULTI_TU_HDR_H

#include "test_common.h"

namespace multi_ns {

/**
 * @brief Shared polymorphic base class included in multiple `.cpp` translation units.
 */
class MultiTuBase {
public:
    /** @brief Base bias value. */
    int bias;

    /**
     * @brief Constructs `MultiTuBase` with `b`.
     * @param[in] b Initial bias integer.
     */
    explicit MultiTuBase(int b) noexcept : bias(b) {}

    /**
     * @brief Virtual destructor ensuring `_ZTV`/`_ZTI`/`D0`/`D1`/`D2` emission in each TU.
     * @details Destroys the `MultiTuBase` subobject cleanly without throwing exceptions.
     */
    virtual ~MultiTuBase() noexcept {}

    /**
     * @brief Computes a polymorphic integer result into `*out_val`.
     * @details Base implementation adds `bias` and `x`.
     * @param[in] x Input operand.
     * @param[out] out_val Pointer receiving computed value.
     * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
     */
    virtual gcc_gen_be_error_t evaluate(int x, int *out_val) const noexcept {
        if (!out_val) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        *out_val = bias + x;
        return GCC_GEN_BE_SUCCESS;
    }
};

/**
 * @brief Derived polymorphic class with inline virtual override shared across multiple TUs.
 */
class MultiTuDerived : public MultiTuBase {
public:
    /** @brief Multiplicative scale factor. */
    int scale;

    /**
     * @brief Constructs `MultiTuDerived`.
     * @param[in] b Base bias.
     * @param[in] s Scale factor.
     */
    MultiTuDerived(int b, int s) noexcept : MultiTuBase(b), scale(s) {}

    /**
     * @brief Virtual destructor for `MultiTuDerived`.
     * @details Overrides `~MultiTuBase` so weak destructor variants emit in each TU.
     */
    ~MultiTuDerived() noexcept override {}

    /**
     * @brief Overrides `evaluate` to compute `bias + x * scale`.
     * @details Emits weak inline virtual method and `_ZTV` across both TUs.
     * @param[in] x Input operand.
     * @param[out] out_val Pointer receiving `bias + x * scale`.
     * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
     */
    gcc_gen_be_error_t evaluate(int x, int *out_val) const noexcept override {
        if (!out_val) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        *out_val = bias + (x * scale);
        return GCC_GEN_BE_SUCCESS;
    }

    /**
     * @brief Out-of-line class method defined in `test_multi_tu_b.cpp`.
     * @details Multiplies `x` by `scale` and adds `tu_b_global_state`.
     * @param[in] x Input value.
     * @param[out] out_val Pointer receiving the computed result.
     * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
     */
    gcc_gen_be_error_t compute_out_of_line(int x, int *out_val) const noexcept;
};

/** @brief Namespace-scoped variable defined in `test_multi_tu_b.cpp` and initialized before `main`. */
extern int g_tu_b_ctor_value;

/**
 * @brief Namespace-scoped function defined in `test_multi_tu_b.cpp`.
 * @details Combines `a` with `g_tu_b_ctor_value` and polymorphic evaluation from TU B.
 * @param[in] a Input integer.
 * @param[out] out_sum Pointer receiving the result.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t compute_from_tu_b(int a, int *out_sum) noexcept;

} // namespace multi_ns

#endif /* GCC_BE_TEST_MULTI_TU_HDR_H */
