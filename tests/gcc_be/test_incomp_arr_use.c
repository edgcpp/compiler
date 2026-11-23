/**
 * @file test_incomp_arr_use.c
 * @brief `-O2` execution test for incomplete `extern int arr[];` and flexible array members at index `200`.
 * @details Verifies that removing the former `T[64]` fake array bound prevents GCC `-O2` `-ftree-vrp`
 * from folding `arr[200]` accesses as out-of-bounds undefined behavior.
 * @ingroup gcc_gen_be_tests
 */

#include "test_common.h"

/** @brief Incomplete external array declaration whose true definition in `test_incomp_arr_def.c` has 256 elements. */
extern int g_incomp_arr[];

/**
 * @brief Initializes `g_incomp_arr` in `test_incomp_arr_def.c`.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
extern gcc_gen_be_error_t init_incomp_arr_storage(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Struct with C99 flexible array member `data[]` tested beyond index `64` at `-O2`.
 */
struct FlexPacket {
    /** @brief Number of payload elements. */
    int count;
    /** @brief Flexible array member tail. */
    int data[];
};

/**
 * @brief Backing union providing 256 elements of storage for `struct FlexPacket` without `malloc`.
 */
union FlexStorage {
    /** @brief Header + flexible tail view. */
    struct FlexPacket pkt;
    /** @brief Raw integer storage (`1 + 220` ints). */
    int raw[221];
};

/**
 * @brief Runs `-O2` incomplete array and flexible array member indexing assertions at index `200`.
 * @details Verifies reads and writes at `g_incomp_arr[200]` and `pkt.data[200]` under `-O2`.
 * @param[out] out_ok Pointer receiving `1` on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
static gcc_gen_be_error_t run_incomp_arr_o2_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    union FlexStorage st;
    int i;
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    GCC_GEN_BE_CHECK(init_incomp_arr_storage());
    ASSERT_EQ(g_incomp_arr[0], 17);
    ASSERT_EQ(g_incomp_arr[64], 209);
    ASSERT_EQ(g_incomp_arr[200], 617);

    g_incomp_arr[200] = 9876;
    ASSERT_EQ(g_incomp_arr[200], 9876);

    st.pkt.count = 210;
    for (i = 0; i < 210; ++i) {
        st.pkt.data[i] = i * 5 + 3;
    }
    ASSERT_EQ(st.pkt.data[200], 1003);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for `test_incomp_arr_use.c`.
 * @details Runs `run_incomp_arr_o2_test` and returns `0` on success.
 * @return `0` on success, `1` on failure.
 */
int main(void) GCC_GEN_BE_NOEXCEPT {
    int ok = 0;
    gcc_gen_be_error_t err = run_incomp_arr_o2_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok == 1) ? 0 : 1;
}
