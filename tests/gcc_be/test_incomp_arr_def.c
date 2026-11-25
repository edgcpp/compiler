/**
 * @file test_incomp_arr_def.c
 * @brief Definition TU for the `-O2` incomplete `extern int arr[]` VRP regression test.
 * @details Defines a 256-element external array `g_incomp_arr[256]` in a separate translation unit
 * so `test_incomp_arr_use.c` sees only `extern int g_incomp_arr[];` at `-O2`.
 * @ingroup gcc_gen_be_tests
 */

#include "test_common.h"

/** @brief External 256-element integer array accessed at index `200` from `test_incomp_arr_use.c`. */
int g_incomp_arr[256];

/**
 * @brief Initializes `g_incomp_arr[0..255]` with deterministic non-zero values.
 * @details Sets `g_incomp_arr[i] = (i * 3) + 17` for every element.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t init_incomp_arr_storage(void) GCC_GEN_BE_NOEXCEPT {
    int i;
    for (i = 0; i < 256; ++i) {
        g_incomp_arr[i] = (i * 3) + 17;
    }
    return GCC_GEN_BE_SUCCESS;
}
