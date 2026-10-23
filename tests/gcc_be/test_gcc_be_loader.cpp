/**
 * @file test_gcc_be_loader.cpp
 * @brief Unit tests for the libgccjit dynamic loader (`gcc_gen_be_lib_loader.c`).
 * @details Tests dynamic loading and idempotent invocation in `load_libgccjit_posix`.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include <cstdio>

USING_NAMESPACE_EDG

/**
 * @brief Executes POSIX libgccjit loading, extended float probing, and idempotency tests.
 * @details Verifies that `load_libgccjit_posix` and `gcc_gen_be_probe_extended_float_types` succeed.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_loader_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    GCC_GEN_BE_CHECK(load_libgccjit_posix());
    GCC_GEN_BE_CHECK(load_libgccjit_posix());

    a_boolean has_ext = FALSE;
    if (gcc_gen_be_probe_extended_float_types(NULL, &has_ext) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_extended_float_types(ctx, &has_ext));
    gcc_jit_context_release(ctx);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_loader.cpp`.
 * @details Runs `run_loader_unit_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_loader_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
