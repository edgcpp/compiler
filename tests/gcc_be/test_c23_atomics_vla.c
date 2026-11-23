/**
 * @file test_c23_atomics_vla.c
 * @brief Native C execution test for `_BitInt(64/128)`, C11 `_Atomic(T)`, generic CAS, C23 `<stdbit.h>`, and VLAs.
 * @details Verifies `_BitInt(64)` / `_BitInt(128)` / `_BitInt(24)` arithmetic, C11 `_Atomic` loads,
 * stores, and RMWs, generic `__atomic_compare_exchange` writing `*expected` strictly on failure
 * (including 24-byte non-lock-free structs forwarded to `libatomic`), C23 `__builtin_stdc_*` bit
 * builtins, and multi-dimensional VLA exact `sizeof(vla)` and stack alignment.
 * @ingroup gcc_gen_be_tests
 */

#include "test_common.h"

/**
 * @brief 24-byte non-lock-free struct exercised with generic `__atomic_load`/`store`/`compare_exchange` (`libatomic`).
 */
struct BigAtomicPayload {
    /** @brief First 64-bit word. */
    long a;
    /** @brief Second 64-bit word. */
    long b;
    /** @brief Third 64-bit word. */
    long c;
};

/**
 * @brief Tests `_BitInt(24)`, `_BitInt(64)`, and `_BitInt(128)` signed/unsigned arithmetic and wraparound.
 * @param[out] out_ok Pointer receiving `1` on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
static gcc_gen_be_error_t test_bitint_widths(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    _BitInt(64) b64_a = -5000000000LL;
    _BitInt(64) b64_b = 8000000000LL;
    _BitInt(64) b64_sum = b64_a + b64_b;
    unsigned _BitInt(128) u128 = ((unsigned _BitInt(128))1 << 100) + 42U;
    unsigned _BitInt(128) u128_hi = u128 >> 100;
    unsigned _BitInt(24) u24 = (unsigned _BitInt(24))0xFFFFFFU;

    *out_ok = 0;
    ASSERT_EQ((long long)b64_sum, 3000000000LL);
    ASSERT_EQ((unsigned long)u128_hi, 1UL);
    ASSERT_EQ((unsigned long)(u128 & 0xFFU), 42UL);
    u24 = (unsigned _BitInt(24))(u24 + 2U);
    ASSERT_EQ((unsigned int)u24, 1U);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests C11 `_Atomic` loads/stores/RMWs and generic `__atomic_compare_exchange` (`*expected` written only on failure).
 * @param[out] out_ok Pointer receiving `1` on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
static gcc_gen_be_error_t test_c11_and_generic_atomics(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    _Atomic int ai = 10;
    _Atomic double ad = 2.5;
    int prev;
    int cell = 100;
    int exp_ok = 100;
    int des_ok = 250;
    int exp_fail = 999;
    int des_fail = 500;
    struct BigAtomicPayload big_cell = { 10L, 20L, 30L };
    struct BigAtomicPayload big_out = { 0L, 0L, 0L };
    struct BigAtomicPayload big_exp = { 10L, 20L, 30L };
    struct BigAtomicPayload big_des = { 40L, 50L, 60L };
    struct BigAtomicPayload big_wrong = { 1L, 2L, 3L };

    *out_ok = 0;
    ai += 5;
    ASSERT_EQ((int)ai, 15);
    prev = ai++;
    ASSERT_EQ(prev, 15);
    ASSERT_EQ((int)ai, 16);
    ASSERT_EQ(++ai, 17);

    ad += 1.5;
    ASSERT_TRUE((double)ad > 3.99 && (double)ad < 4.01);

    /* Lock-free CAS success: *exp_ok MUST remain 100 (not overwritten with 250!) */
    ASSERT_TRUE(__atomic_compare_exchange(&cell, &exp_ok, &des_ok, 0, 5, 5));
    ASSERT_EQ(cell, 250);
    ASSERT_EQ(exp_ok, 100);

    /* Lock-free CAS failure: *exp_fail MUST be updated to 250 */
    ASSERT_FALSE(__atomic_compare_exchange(&cell, &exp_fail, &des_fail, 0, 5, 5));
    ASSERT_EQ(cell, 250);
    ASSERT_EQ(exp_fail, 250);

    /* 24-byte struct forwarded to libatomic */
    __atomic_load(&big_cell, &big_out, 5);
    ASSERT_EQ(big_out.a + big_out.b + big_out.c, 60L);
    ASSERT_TRUE(__atomic_compare_exchange(&big_cell, &big_exp, &big_des, 0, 5, 5));
    ASSERT_EQ(big_exp.a, 10L);
    __atomic_load(&big_cell, &big_out, 5);
    ASSERT_EQ(big_out.a + big_out.b + big_out.c, 150L);
    ASSERT_FALSE(__atomic_compare_exchange(&big_cell, &big_wrong, &big_exp, 0, 5, 5));
    ASSERT_EQ(big_wrong.c, 60L);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Tests C23 `__builtin_stdc_*` bit builtins and 2D VLA exact `sizeof(vla)` and alignment.
 * @param[in] rows Runtime VLA row count.
 * @param[in] cols Runtime VLA column count.
 * @param[out] out_ok Pointer receiving `1` on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
static gcc_gen_be_error_t test_c23_bits_and_vla(int rows, int cols, int *out_ok) GCC_GEN_BE_NOEXCEPT {
    long vla[rows][cols];
    unsigned long addr;
    int r;
    int c;

    *out_ok = 0;
    /* Exact VLA sizeof without +1 off-by-one bug! */
    ASSERT_EQ((unsigned long)sizeof(vla), (unsigned long)rows * (unsigned long)cols * sizeof(long));
    addr = (unsigned long)&vla[0][0];
    ASSERT_EQ(addr % _Alignof(long), 0UL);

    for (r = 0; r < rows; ++r) {
        for (c = 0; c < cols; ++c) {
            vla[r][c] = (long)(r * 10 + c + 1);
        }
    }
    ASSERT_EQ(vla[rows - 1][cols - 1], (long)((rows - 1) * 10 + (cols - 1) + 1));

    ASSERT_EQ(__builtin_stdc_leading_zeros(0U), 32U);
    ASSERT_EQ(__builtin_stdc_trailing_zeros(8U), 3U);
    ASSERT_EQ(__builtin_stdc_count_ones(0xF0F0U), 8U);
    ASSERT_TRUE(__builtin_stdc_has_single_bit(64U));
    ASSERT_FALSE(__builtin_stdc_has_single_bit(65U));
    ASSERT_EQ(__builtin_stdc_bit_width(19U), 5U);
    ASSERT_EQ(__builtin_stdc_bit_floor(19U), 16U);
    ASSERT_EQ(__builtin_stdc_bit_ceil(19U), 32U);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for `test_c23_atomics_vla.c`.
 * @details Executes `_BitInt`, C11/`libatomic`, C23 bit, and VLA verification routines.
 * @return `0` on success, `1` on failure.
 */
int main(void) GCC_GEN_BE_NOEXCEPT {
    int ok1 = 0;
    int ok2 = 0;
    int ok3 = 0;
    if (test_bitint_widths(&ok1) != GCC_GEN_BE_SUCCESS || !ok1) return 1;
    if (test_c11_and_generic_atomics(&ok2) != GCC_GEN_BE_SUCCESS || !ok2) return 1;
    if (test_c23_bits_and_vla(5, 7, &ok3) != GCC_GEN_BE_SUCCESS || !ok3) return 1;
    return 0;
}
