/**
 * @file test_gcc_be_lib_loader.cpp
 * @brief Unit tests for dynamic loading of `libgccjit` (`gcc_gen_be_lib_loader.c`).
 * @details Uses mocked `dlopen` and `dlsym` implementations to test library load failure,
 * missing optional symbols, present optional symbols, and idempotent re-loading.
 * @ingroup gcc_gen_be_tests
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_error.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
/**
 * @brief Mock `dlopen` implementation for unit testing `gcc_gen_be_lib_loader.c`.
 * @details Simulates library open success or failure based on `should_fail_dlopen`.
 * @param[in] filename Library filename.
 * @param[in] flags Dynamic linker flags.
 * @return Mock handle on success, or NULL on simulated failure.
 */
void *mock_dlopen(const char *filename, int flags) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Mock `dlsym` implementation for unit testing `gcc_gen_be_lib_loader.c`.
 * @details Simulates symbol lookup success or failure based on `should_fail_dlsym`.
 * @param[in] handle Library handle.
 * @param[in] symbol Symbol name to resolve.
 * @return Mock function pointer on success, or NULL on simulated failure.
 */
void *mock_dlsym(void *handle, const char *symbol) GCC_GEN_BE_NOEXCEPT;
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
    return msg ? err : err;
}
}

/** @brief Controls whether `mock_dlopen` simulates failure. */
static bool should_fail_dlopen = false;
/** @brief Controls whether `mock_dlsym` simulates failure. */
static bool should_fail_dlsym = false;
/** @brief Controls whether `gcc_jit_context_acquire` is found on `RTLD_DEFAULT`. */
static bool allow_rtld_default_acquire = false;

void *mock_dlopen(const char *filename, int flags) GCC_GEN_BE_NOEXCEPT {
    if (!filename && flags == 0) return NULL;
    if (should_fail_dlopen) return NULL;
    return (void *)0xDEADBEEF;
}

void *mock_dlsym(void *handle, const char *symbol) GCC_GEN_BE_NOEXCEPT {
    if (allow_rtld_default_acquire && strcmp(symbol, "gcc_jit_context_acquire") == 0) {
        return handle ? (void *)0x1111 : (void *)0x1111;
    }
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

/** @brief Controls mock builtin lookup return in `mock_get_builtin`. */
static bool g_mock_builtin_nonnull = false;

/**
 * @brief Mock `gcc_jit_context_get_builtin_function` callback for probing EH builtins.
 * @details Returns a non-null sentinel iff `g_mock_builtin_nonnull` is true.
 * @param[in] ctx Mock JIT context pointer.
 * @param[in] name Requested builtin name.
 * @return Non-null sentinel or NULL.
 */
static struct gcc_jit_function *mock_get_builtin(struct gcc_jit_context *ctx, const char *name) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !name) return NULL;
    return g_mock_builtin_nonnull ? (struct gcc_jit_function *)0x2222 : NULL;
}

USING_NAMESPACE_EDG

/**
 * @brief Runs all dynamic loader test cases and percolates `gcc_gen_be_error_t`.
 * @details Tests `dlopen` failure, `EDG_LIBGCCJIT_PATH`, `RTLD_DEFAULT` fallback, optional symbols, and probes.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_loader_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    /* OOM budget check */
    libgccjit_handle = NULL;
    g_gcc_be_oom_countdown = 0;
    if (load_libgccjit_posix() != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;

    /* Empty EDG_LIBGCCJIT_PATH + dlopen failure + dlsym failure */
    setenv("EDG_LIBGCCJIT_PATH", "", 1);
    should_fail_dlopen = true;
    should_fail_dlsym = true;
    allow_rtld_default_acquire = false;
    libgccjit_handle = NULL;
    if (load_libgccjit_posix() != GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Non-empty EDG_LIBGCCJIT_PATH failing dlopen, then RTLD_DEFAULT succeeding! */
    setenv("EDG_LIBGCCJIT_PATH", "/nonexistent/mock_libgccjit.dylib", 1);
    allow_rtld_default_acquire = true;
    libgccjit_handle = NULL;
    GCC_GEN_BE_CHECK(load_libgccjit_posix());
    allow_rtld_default_acquire = false;

    /* Non-empty EDG_LIBGCCJIT_PATH succeeding directly (skips candidate loop) */
    should_fail_dlopen = false;
    should_fail_dlsym = true;
    libgccjit_handle = NULL;
    GCC_GEN_BE_CHECK(load_libgccjit_posix());
    unsetenv("EDG_LIBGCCJIT_PATH");
    if (p_gcc_jit_lvalue_set_tls_model != NULL) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Probe EH builtins & native EH support across all combinations */
    struct gcc_jit_context *fake_ctx = (struct gcc_jit_context *)0x1;
    a_boolean flag = FALSE;
    if (gcc_gen_be_probe_eh_builtins(NULL, &flag) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_probe_eh_builtins(fake_ctx, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_probe_eh_builtins(fake_ctx, &flag) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;
    p_gcc_jit_context_get_builtin_function = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_eh_builtins(fake_ctx, &flag));
    if (flag != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;
    p_gcc_jit_context_get_builtin_function = &mock_get_builtin;
    g_mock_builtin_nonnull = false;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_eh_builtins(fake_ctx, &flag));
    if (flag != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_builtin_nonnull = true;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_eh_builtins(fake_ctx, &flag));
    if (flag != TRUE) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_probe_native_eh_support(NULL, &flag) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_probe_native_eh_support(fake_ctx, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = 0;
    if (gcc_gen_be_probe_native_eh_support(fake_ctx, &flag) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;
    p_gcc_jit_block_add_try_catch = NULL;
    p_gcc_jit_block_add_cleanup = NULL;
    p_gcc_jit_block_end_with_resx = NULL;
    p_gcc_jit_function_set_personality = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_native_eh_support(fake_ctx, &flag));
    p_gcc_jit_block_add_try_catch = (pfn_gcc_jit_block_add_try_catch)0x1;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_native_eh_support(fake_ctx, &flag));
    p_gcc_jit_block_add_cleanup = (pfn_gcc_jit_block_add_cleanup)0x1;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_native_eh_support(fake_ctx, &flag));
    p_gcc_jit_block_end_with_resx = (pfn_gcc_jit_block_end_with_resx)0x1;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_native_eh_support(fake_ctx, &flag));
    if (flag != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;
    p_gcc_jit_function_set_personality = (pfn_gcc_jit_function_set_personality)0x1;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_native_eh_support(fake_ctx, &flag));
    if (flag != TRUE) return GCC_GEN_BE_ERROR_INTERNAL;

    a_boolean has_inv = FALSE;
    a_boolean has_lp = FALSE;
    if (gcc_gen_be_resolve_eh_entrypoints(NULL, &has_inv, &has_lp) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_resolve_eh_entrypoints(fake_ctx, NULL, &has_lp) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_resolve_eh_entrypoints(fake_ctx, &has_inv, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    p_gcc_jit_block_add_landing_pad = NULL;
    p_gcc_jit_block_add_try_catch = (pfn_gcc_jit_block_add_try_catch)0x1;
    GCC_GEN_BE_CHECK(gcc_gen_be_resolve_eh_entrypoints(fake_ctx, &has_inv, &has_lp));
    if (has_lp != TRUE) return GCC_GEN_BE_ERROR_INTERNAL;
    p_gcc_jit_block_add_try_catch = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_resolve_eh_entrypoints(fake_ctx, &has_inv, &has_lp));
    if (has_inv != FALSE || has_lp != FALSE) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_probe_extended_float_types(fake_ctx, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

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

    GCC_GEN_BE_CHECK(gcc_gen_be_resolve_eh_entrypoints(fake_ctx, &has_inv, &has_lp));
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
int main(void) GCC_GEN_BE_NOEXCEPT {
    int ok = 0;
    gcc_gen_be_error_t err = run_loader_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    puts("PASS");
    return 0;
}
