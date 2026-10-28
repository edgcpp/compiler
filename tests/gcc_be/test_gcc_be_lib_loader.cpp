/**
 * @file test_gcc_be_lib_loader.cpp
 * @brief Unit tests for dynamic loading of `libgccjit` (`gcc_gen_be_lib_loader.c`).
 * @details Uses mocked `dlopen` and `dlsym` implementations to test library load failure,
 * missing optional symbols, present optional symbols, and idempotent re-loading.
 */

#include "basic_hdrs.h"
#include <stdio.h>
#include <string.h>

extern "C" {
/**
 * @brief Mock `dlopen` implementation for unit testing `gcc_gen_be_lib_loader.c`.
 * @details Simulates library open success or failure based on `should_fail_dlopen`.
 * @param[in] filename Library filename.
 * @param[in] flags Dynamic linker flags.
 * @return Mock handle on success, or NULL on simulated failure.
 */
void *mock_dlopen(const char *filename, int flags);

/**
 * @brief Mock `dlsym` implementation for unit testing `gcc_gen_be_lib_loader.c`.
 * @details Simulates symbol lookup success or failure based on `should_fail_dlsym`.
 * @param[in] handle Library handle.
 * @param[in] symbol Symbol name to resolve.
 * @return Mock function pointer on success, or NULL on simulated failure.
 */
void *mock_dlsym(void *handle, const char *symbol);
}

/** @brief Redirects `dlopen` to `mock_dlopen` in the included loader implementation. */
#define dlopen mock_dlopen
/** @brief Redirects `dlsym` to `mock_dlsym` in the included loader implementation. */
#define dlsym mock_dlsym

#include "gcc_gen_be_lib_loader.c"

extern "C" {
/**
 * @brief Mock diagnostic reporter for lib loader tests.
 * @details Returns the error code passed in.
 * @param[in] err Error code.
 * @param[in] msg Context message.
 * @return `err`.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char *msg) GCC_GEN_BE_NOEXCEPT {
    (void)msg;
    return err;
}
}

/** @brief Controls whether `mock_dlopen` simulates failure. */
static bool should_fail_dlopen = false;
/** @brief Controls whether `mock_dlsym` simulates failure. */
static bool should_fail_dlsym = false;

void *mock_dlopen(const char *filename, int flags) {
    (void)filename;
    (void)flags;
    if (should_fail_dlopen) return NULL;
    return (void *)0xDEADBEEF;
}

void *mock_dlsym(void *handle, const char *symbol) {
    (void)handle;
    if (should_fail_dlsym) return NULL;

    if (strcmp(symbol, "gcc_jit_block_end_with_extended_asm_goto") == 0) return (void *)1;
    if (strcmp(symbol, "gcc_jit_lvalue_set_tls_model") == 0) return (void *)2;
    if (strcmp(symbol, "gcc_jit_function_add_attribute") == 0) return (void *)3;
    if (strcmp(symbol, "gcc_jit_lvalue_add_string_attribute") == 0) return (void *)4;
    if (strcmp(symbol, "gcc_jit_lvalue_set_alignment") == 0) return (void *)5;
    if (strcmp(symbol, "gcc_jit_context_get_int_type") == 0) return (void *)6;
    if (strcmp(symbol, "gcc_jit_type_get_vector") == 0) return (void *)7;
    if (strcmp(symbol, "gcc_jit_block_end_with_invoke") == 0) return (void *)8;
    if (strcmp(symbol, "gcc_jit_block_add_landing_pad") == 0) return (void *)9;
    if (strcmp(symbol, "gcc_jit_context_new_eh_filter") == 0) return (void *)10;

    return NULL;
}

using namespace edg;

/**
 * @brief Runs all dynamic loader test cases and percolates `gcc_gen_be_error_t`.
 * @details Tests `dlopen` failure, missing optional symbols, present optional symbols, and idempotency.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_loader_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    should_fail_dlopen = true;
    should_fail_dlsym = true;
    libgccjit_handle = NULL;
    gcc_gen_be_error_t err = load_libgccjit_posix();
    if (err != GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    should_fail_dlopen = false;
    should_fail_dlsym = true;
    libgccjit_handle = NULL;
    GCC_GEN_BE_CHECK(load_libgccjit_posix());
    if (p_gcc_jit_lvalue_set_tls_model != NULL) return GCC_GEN_BE_ERROR_INTERNAL;

    a_boolean has_inv = FALSE;
    a_boolean has_lp = FALSE;
    if (gcc_gen_be_resolve_eh_entrypoints(NULL, &has_inv, &has_lp) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_resolve_eh_entrypoints((struct gcc_jit_context *)0x1, NULL, &has_lp) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_resolve_eh_entrypoints((struct gcc_jit_context *)0x1, &has_inv, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_resolve_eh_entrypoints((struct gcc_jit_context *)0x1, &has_inv, &has_lp));
    if (has_inv != FALSE || has_lp != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;

    should_fail_dlopen = false;
    should_fail_dlsym = false;
    libgccjit_handle = NULL;
    GCC_GEN_BE_CHECK(load_libgccjit_posix());
    if (p_gcc_jit_block_end_with_extended_asm_goto != (void *)1) return GCC_GEN_BE_ERROR_INTERNAL;
    if (p_gcc_jit_lvalue_set_tls_model != (void *)2) return GCC_GEN_BE_ERROR_INTERNAL;
    if (p_gcc_jit_function_add_attribute != (void *)3) return GCC_GEN_BE_ERROR_INTERNAL;
    if (p_gcc_jit_lvalue_add_string_attribute != (void *)4) return GCC_GEN_BE_ERROR_INTERNAL;
    if (p_gcc_jit_lvalue_set_alignment != (void *)5) return GCC_GEN_BE_ERROR_INTERNAL;
    if (p_gcc_jit_context_get_int_type != (void *)6) return GCC_GEN_BE_ERROR_INTERNAL;
    if (p_gcc_jit_type_get_vector != (void *)7) return GCC_GEN_BE_ERROR_INTERNAL;
    if (p_gcc_jit_block_end_with_invoke != (void *)8) return GCC_GEN_BE_ERROR_INTERNAL;
    if (p_gcc_jit_block_add_landing_pad != (void *)9) return GCC_GEN_BE_ERROR_INTERNAL;
    if (p_gcc_jit_context_new_eh_filter != (void *)10) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_resolve_eh_entrypoints((struct gcc_jit_context *)0x1, &has_inv, &has_lp));
    if (has_inv != TRUE || has_lp != TRUE) return GCC_GEN_BE_ERROR_INTERNAL;

    libgccjit_handle = (void *)0xDEADBEEF;
    should_fail_dlopen = true;
    GCC_GEN_BE_CHECK(load_libgccjit_posix());

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for dynamic loader unit tests.
 * @details Runs `run_loader_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_loader_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    puts("PASS");
    return 0;
}
