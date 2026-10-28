/**
 * @file test_dynamic_init.cpp
 * @brief End-to-end C++ test for file-scope dynamic initialization via global constructors.
 * @details Verifies that `GlobalInitializer`'s constructor runs before `main()` and sets `my_global = 42`.
 */

#include "test_common.h"

/** @brief Global integer modified by `GlobalInitializer::GlobalInitializer()`. */
int my_global = 0;

/**
 * @brief Helper struct whose constructor initializes `my_global` during dynamic startup.
 * @details Exercises `_GLOBAL__sub_I_*` static constructor synthesis in `gcc_gen_be_dtor.c`.
 */
struct GlobalInitializer {
    /**
     * @brief Constructs `GlobalInitializer` and sets `my_global = 42`.
     * @details Invoked automatically before `main()` enters.
     */
    GlobalInitializer() {
        my_global = 42;
    }
};

/** @brief File-scope `GlobalInitializer` instance triggering dynamic initialization. */
GlobalInitializer init_obj;

/**
 * @brief Verifies that `init_obj`'s constructor ran before `main()`.
 * @details Asserts `my_global == 42`.
 * @param[out] out_ok Pointer to receive 1 on success.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_dynamic_init_test(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;
    ASSERT_EQ(my_global, 42);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point.
 * @details Invokes `run_dynamic_init_test` and returns 0 on success.
 * @return 0 on success.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_dynamic_init_test(&ok);
    return (err == GCC_GEN_BE_SUCCESS && ok) ? 0 : 1;
}
