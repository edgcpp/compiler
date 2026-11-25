/**
 * @file test_omp_exec.c
 * @brief Native OpenMP execution test linked against GNU `libgomp` (`-lgomp`).
 * @details Exercises `#pragma omp parallel for reduction(+:sum)`, `schedule(dynamic, 4)`,
 * `collapse(2)`, `firstprivate`, `lastprivate`, `#pragma omp critical`, `#pragma omp single`,
 * `#pragma omp master`, `#pragma omp barrier`, `#pragma omp task` / `taskwait`, `#pragma omp atomic`,
 * and `#pragma omp simd` end-to-end with real multi-threaded `libgomp` execution.
 * @ingroup gcc_gen_be_tests
 */

#include "test_common.h"

/**
 * @brief Verifies `#pragma omp parallel for reduction(+:sum)` and `collapse(2)` with `libgomp`.
 * @param[out] out_ok Pointer receiving `1` on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
static gcc_gen_be_error_t test_omp_parallel_for_reduction(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    long sum = 0;
    long grid_sum = 0;
    int last_i = -1;
    int bonus = 10;
    int i;
    int r;
    int c;

    *out_ok = 0;

    #pragma omp parallel for num_threads(4) firstprivate(bonus) lastprivate(last_i) reduction(+:sum)
    for (i = 1; i <= 100; ++i) {
        sum += (long)(i + bonus);
        last_i = i;
    }
    /* sum_{i=1..100} i = 5050, plus 100 * 10 = 6050 */
    ASSERT_EQ(sum, 6050L);
    ASSERT_EQ(last_i, 100);

    #pragma omp parallel for schedule(dynamic, 4) collapse(2) num_threads(4) reduction(+:grid_sum)
    for (r = 0; r < 8; ++r) {
        for (c = 0; c < 6; ++c) {
            grid_sum += (long)(r * 10 + c);
        }
    }
    /* 8 * sum(0..5=15) + 6 * 10 * sum(0..7=28) = 120 + 1680 = 1800 */
    ASSERT_EQ(grid_sum, 1800L);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Verifies `parallel`, `single`, `master`, `barrier`, `critical`, `atomic`, `task`/`taskwait`, and `simd`.
 * @param[out] out_ok Pointer receiving `1` on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED`.
 */
static gcc_gen_be_error_t test_omp_sync_task_and_simd(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    int single_cnt = 0;
    int master_cnt = 0;
    int crit_cnt = 0;
    int atomic_cnt = 0;
    int task_val = 0;
    int simd_arr[32];
    int simd_sum = 0;
    int k;

    *out_ok = 0;

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            single_cnt += 1;
            #pragma omp task shared(task_val)
            {
                task_val = 42;
            }
            #pragma omp taskwait
            ;
        }

        #pragma omp barrier
        ;

        #pragma omp master
        {
            master_cnt += 1;
        }

        #pragma omp critical(test_lock)
        {
            crit_cnt += 1;
        }

        #pragma omp atomic
        atomic_cnt += 2;
    }

    ASSERT_EQ(single_cnt, 1);
    ASSERT_EQ(master_cnt, 1);
    ASSERT_EQ(task_val, 42);
    ASSERT_TRUE(crit_cnt >= 1 && crit_cnt <= 4);
    ASSERT_EQ(atomic_cnt, crit_cnt * 2);

    #pragma omp simd
    for (k = 0; k < 32; ++k) {
        simd_arr[k] = k * 2;
    }
    for (k = 0; k < 32; ++k) {
        simd_sum += simd_arr[k];
    }
    ASSERT_EQ(simd_sum, 992);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for `test_omp_exec.c`.
 * @details Runs `test_omp_parallel_for_reduction` and `test_omp_sync_task_and_simd`.
 * @return `0` on success, `1` on failure.
 */
int main(void) GCC_GEN_BE_NOEXCEPT {
    int ok1 = 0;
    int ok2 = 0;
    if (test_omp_parallel_for_reduction(&ok1) != GCC_GEN_BE_SUCCESS || !ok1) return 1;
    if (test_omp_sync_task_and_simd(&ok2) != GCC_GEN_BE_SUCCESS || !ok2) return 1;
    return 0;
}
