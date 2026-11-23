/**
 * @file gcc_gen_be_builtin.c
 * @brief Implementation of Target SIMD Vector Intrinsics & GCC Builtins.
 * @details Maps compiler builtins (atomic, C23 `<stdbit.h>`, math, bitwise, overflow, checked
 * string/memory, `source_location`, and coroutine builtins) to `libgccjit` function calls or
 * native operations, and delegates x86 SSE/AVX and ARM NEON vector intrinsics to `gcc_gen_be_simd.c`.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_atomic.h"
#include "gcc_gen_be_simd.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_lib_loader.h"
#include <string.h>
#include <stdio.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Unwraps any leading `tk_typeref` nodes from a frontend type pointer.
 * @details Advances through `tp->variant.typeref.type` while `tp` is non-null and has kind `tk_typeref`.
 * @param[in] tp Frontend `a_type_ptr` (may be NULL).
 * @param[out] out_tp Non-null pointer receiving the unwrapped `a_type_ptr` (or NULL).
 * @return `GCC_GEN_BE_SUCCESS`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t unwrap_builtin_type(
    a_type_ptr tp,
    a_type_ptr *out_tp) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    while (tp && tp->kind == tk_typeref) {
        tp = tp->variant.typeref.type;
    }
    *out_tp = tp;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Unwraps any leading `tk_typeref` nodes and queries the raw byte size of `tp`.
 * @details Sets `*out_sz` to `(long)tp->size` when `tp` unwraps to a non-null type, else `0`.
 * @param[in] tp Frontend `a_type_ptr` (may be NULL).
 * @param[out] out_sz Non-null pointer receiving the byte size.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t unwrap_type_byte_size(
    a_type_ptr tp,
    long *out_sz) GCC_GEN_BE_NOEXCEPT {
    a_type_ptr utp = NULL;
    GCC_GEN_BE_CHECK(unwrap_builtin_type(tp, &utp));
    *out_sz = utp ? (long)utp->size : 0L;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Extracts the return type and first parameter descriptor from a frontend `a_routine_ptr`.
 * @details Safely inspects `rout->type` when it is a `tk_routine` node, populating `*out_ret_tp`
 * and `*out_first_param` (or NULL when absent).
 * @param[in] rout Non-null frontend routine pointer.
 * @param[out] out_ret_tp Optional pointer receiving `rout->type->variant.routine.return_type`.
 * @param[out] out_first_param Optional pointer receiving `extra_info->param_type_list`.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t get_routine_sig_info(
    a_routine_ptr rout,
    a_type_ptr *out_ret_tp,
    a_param_type_ptr *out_first_param) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    a_type_ptr ret_tp = NULL;
    a_param_type_ptr first_param = NULL;
    if (rout->type && rout->type->kind == tk_routine) {
        ret_tp = rout->type->variant.routine.return_type;
        if (rout->type->variant.routine.extra_info) {
            first_param = rout->type->variant.routine.extra_info->param_type_list;
        }
    }
    if (out_ret_tp) *out_ret_tp = ret_tp;
    if (out_first_param) *out_first_param = first_param;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Safely queries the byte size of a scalar or pointer `gcc_jit_type` without triggering `libgccjit` ICE on `__int128`.
 * @details Handles 16-byte `__int128` / `unsigned __int128` explicitly (since `libgccjit`'s `get_size` aborts on 128-bit ints)
 * and returns `0` for `void`, aggregate, or vector types.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] jt Target `gcc_jit_type` to inspect.
 * @param[out] out_sz Receives byte size (`1..16`), or `0` for non-scalar/void types.
 * @return `GCC_GEN_BE_SUCCESS`, or `GCC_GEN_BE_ERROR_OOM` on injected fault.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t safe_jit_scalar_size(
    gcc_jit_context *ctx,
    gcc_jit_type *jt,
    ssize_t *out_sz) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_sz = 0;
    if (gcc_jit_type_is_pointer(jt) != NULL) {
        *out_sz = gcc_jit_type_get_size(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T));
        return GCC_GEN_BE_SUCCESS;
    }
    if (gcc_jit_type_is_integral(jt) != 0) {
        static const enum gcc_jit_types k_std_ints[] = {
            GCC_JIT_TYPE_BOOL, GCC_JIT_TYPE_CHAR, GCC_JIT_TYPE_SIGNED_CHAR, GCC_JIT_TYPE_UNSIGNED_CHAR,
            GCC_JIT_TYPE_SHORT, GCC_JIT_TYPE_UNSIGNED_SHORT, GCC_JIT_TYPE_INT, GCC_JIT_TYPE_UNSIGNED_INT,
            GCC_JIT_TYPE_LONG, GCC_JIT_TYPE_UNSIGNED_LONG, GCC_JIT_TYPE_LONG_LONG, GCC_JIT_TYPE_UNSIGNED_LONG_LONG,
            GCC_JIT_TYPE_SIZE_T
        };
        for (size_t i = 0; i < sizeof(k_std_ints) / sizeof(k_std_ints[0]); ++i) {
            if (jt == gcc_jit_context_get_type(ctx, k_std_ints[i])) {
                *out_sz = gcc_jit_type_get_size(jt);
                return GCC_GEN_BE_SUCCESS;
            }
        }
        *out_sz = 16;
        return GCC_GEN_BE_SUCCESS;
    }
    if (jt == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT) ||
        jt == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE) ||
        jt == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE)) {
        *out_sz = gcc_jit_type_get_size(jt);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Checks whether `str` contains any NUL-terminated substring in `table`.
 * @details Scans `table` until a NULL sentinel is reached and sets `*out_match` to `TRUE` on the first match.
 * @param[in] str Non-null input string to inspect.
 * @param[in] table NULL-terminated array of substrings.
 * @param[out] out_match Receives `TRUE` if any entry is found in `str`, else `FALSE`.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t name_has_any_substr(
    const char *str,
    const char *const *table,
    a_boolean *out_match) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_match = FALSE;
    for (size_t i = 0; table[i] != NULL; ++i) {
        if (strstr(str, table[i]) != NULL) {
            *out_match = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Checks whether `str` starts with any NUL-terminated prefix in `table`.
 * @details Scans `table` until a NULL sentinel is reached and sets `*out_match` to `TRUE` on the first match.
 * @param[in] str Non-null input string to inspect.
 * @param[in] table NULL-terminated array of prefixes.
 * @param[out] out_match Receives `TRUE` if `str` starts with any entry, else `FALSE`.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t name_has_any_prefix(
    const char *str,
    const char *const *table,
    a_boolean *out_match) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_match = FALSE;
    for (size_t i = 0; table[i] != NULL; ++i) {
        if (strncmp(str, table[i], strlen(table[i])) == 0) {
            *out_match = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Helper to resolve and cache a builtin function.
 * @details Queries `gcc_jit_context_get_builtin_function` when available and permitted, falling back
 * to an imported `gcc_jit_function` cached by symbol name in `GCC_GEN_BE_CACHE_STRING`.
 * @param[in] ctx The libgccjit context.
 * @param[in] name The builtin name (e.g. `"__atomic_load_n"`).
 * @param[in] ret_type The return type.
 * @param[in] num_params Number of parameters.
 * @param[in] param_types Array of parameter types.
 * @param[in] is_variadic Whether it accepts variadic arguments.
 * @param[out] out_func Non-null pointer to receive the JIT function.
 * @return `GCC_GEN_BE_SUCCESS` or an error code.
 */
static gcc_gen_be_error_t get_builtin(gcc_jit_context *ctx, const char *name, gcc_jit_type *ret_type, int num_params, gcc_jit_type **param_types, int is_variadic, gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_func = NULL;

    static const char *const k_libc_substrs[] = {
        "va_start", "va_end", "va_copy", "va_arg",
        "vprintf", "vfprintf", "vsprintf", "vsnprintf", "vasprintf", "vdprintf",
        "vscanf", "vfscanf", "vsscanf", "vsyslog",
        "modf", "frexp", "sincos", "remquo", "lgamma", "nanl", "printf", "scanf",
        NULL
    };
    static const char *const k_libc_prefixes[] = {
        "__builtin_char_memchr", "__builtin_str", "__builtin_wcs", "__builtin_wmem", NULL
    };
    static const char *const k_jit_builtin_prefixes[] = {
        "__builtin_", "__sync_", "__atomic_", NULL
    };

    a_boolean sub_match = FALSE;
    a_boolean pfx_match = FALSE;
    a_boolean is_jit_pfx = FALSE;
    GCC_GEN_BE_CHECK(name_has_any_substr(name, k_libc_substrs, &sub_match));
    GCC_GEN_BE_CHECK(name_has_any_prefix(name, k_libc_prefixes, &pfx_match));
    GCC_GEN_BE_CHECK(name_has_any_prefix(name, k_jit_builtin_prefixes, &is_jit_pfx));

    if (p_gcc_jit_context_get_builtin_function && !sub_match && !pfx_match && is_jit_pfx) {
        gcc_jit_function *builtin = p_gcc_jit_context_get_builtin_function(ctx, name);
        if (builtin) {
            *out_func = builtin;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    
    /* Fallback to importing standard functions (e.g., libc, libatomic, libm) */
    const char *import_name = name;
    if (strcmp(name, "__builtin_char_memchr") == 0) {
        import_name = "memchr";
    } else if (strncmp(name, "__builtin_", 10) == 0) {
        import_name = name + 10;
    }
    char imp_key[256];
    snprintf(imp_key, sizeof(imp_key), ".imp:%s", import_name);
    void *cached_imp = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)imp_key, &cached_imp));
    if (cached_imp) {
        *out_func = (gcc_jit_function *)cached_imp;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_param **params = NULL;
    if (num_params > 0) {
        GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)num_params * sizeof(gcc_jit_param *), (void **)&params));
        for (int i = 0; i < num_params; i++) {
            params[i] = gcc_jit_context_new_param(ctx, NULL, param_types[i], "p");
        }
    }
    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, import_name, num_params, params, is_variadic);
    free(params);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)imp_key, fn));
    *out_func = fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Checks whether `str` equals any NUL-terminated string in `table`.
 * @details Scans `table` until a NULL sentinel is reached and sets `*out_match` to `TRUE` on exact match.
 * @param[in] str Non-null input string to inspect.
 * @param[in] table NULL-terminated array of candidate strings.
 * @param[out] out_match Receives `TRUE` if `str` equals any entry, else `FALSE`.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t name_equals_any(
    const char *str,
    const char *const *table,
    a_boolean *out_match) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_match = FALSE;
    for (size_t i = 0; table[i] != NULL; ++i) {
        if (strcmp(str, table[i]) == 0) {
            *out_match = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Checks whether `sz` is a lock-free / natively sized atomic byte width (`1, 2, 4, 8, 16`).
 * @details Sets `*out_ok` to `TRUE` iff `sz` is one of `1`, `2`, `4`, `8`, or `16`.
 * @param[in] sz Byte width to test.
 * @param[out] out_ok Non-null pointer receiving `TRUE` when `sz` is a supported width, else `FALSE`.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t is_supported_atomic_width(
    long sz,
    a_boolean *out_ok) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_ok = (sz == 1 || sz == 2 || sz == 4 || sz == 8 || sz == 16) ? TRUE : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers atomic memory builtins (__atomic_load_n, etc.).
 *
 * @details Resolves the atomic builtin function and emits a call with the lowered arguments.
 *
 * @param[in] name The builtin name.
 * @param[in] rout The routine pointer.
 * @param[in] num_args Number of arguments.
 * @param[in] args Array of lowered argument rvalues.
 * @param[out] out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_atomic_builtin(const char *name, a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    a_type_ptr rout_ret_tp = NULL;
    a_param_type_ptr first_param = NULL;
    GCC_GEN_BE_CHECK(get_routine_sig_info(rout, &rout_ret_tp, &first_param));

    gcc_jit_type *bool_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
    if (strcmp(name, "__atomic_always_lock_free") == 0 || strcmp(name, "__atomic_is_lock_free") == 0) {
        int is_lf = 1;
        if (first_param && first_param->next && first_param->next->type) {
            a_type_ptr pt = NULL;
            GCC_GEN_BE_CHECK(unwrap_builtin_type(first_param->next->type, &pt));
            if (pt && pt->kind == tk_pointer && pt->variant.pointer.type) {
                long ptsz = 0;
                a_boolean ptsz_ok = TRUE;
                GCC_GEN_BE_CHECK(unwrap_type_byte_size(pt->variant.pointer.type, &ptsz));
                GCC_GEN_BE_CHECK(is_supported_atomic_width(ptsz, &ptsz_ok));
                if (ptsz > 0 && !ptsz_ok) {
                    is_lf = 0;
                }
            }
        }
        *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, bool_type, is_lf);
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_type *ret_type = NULL;
    static const char *const k_void_atomics[] = {
        "__atomic_clear", "__sync_synchronize", "__atomic_thread_fence", "__atomic_signal_fence", NULL
    };
    a_boolean is_void_atomic = FALSE;
    GCC_GEN_BE_CHECK(name_has_any_prefix(name, k_void_atomics, &is_void_atomic));
    if (is_void_atomic) {
        ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    } else if (rout_ret_tp) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(rout_ret_tp, &ret_type));
        if (!ret_type) ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    } else {
        ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    }

    static const char *const k_generic_atomic_names[] = {
        "__atomic_load", "__atomic_store", "__atomic_exchange", "__atomic_compare_exchange", NULL
    };
    a_boolean is_generic_libatomic_call = FALSE;
    GCC_GEN_BE_CHECK(name_equals_any(name, k_generic_atomic_names, &is_generic_libatomic_call));
    if (is_generic_libatomic_call && num_args >= 3) {
        return gcc_gen_be_lower_generic_atomic_call(name, rout, num_args, args, 0UL, out_rval);
    }

    long atomic_elem_sz = 0;
    if (first_param && first_param->type) {
        a_type_ptr p0 = NULL;
        GCC_GEN_BE_CHECK(unwrap_builtin_type(first_param->type, &p0));
        if (p0 && p0->kind == tk_pointer && p0->variant.pointer.type) {
            GCC_GEN_BE_CHECK(unwrap_type_byte_size(p0->variant.pointer.type, &atomic_elem_sz));
        }
    }
    long ret_sz = 0;
    GCC_GEN_BE_CHECK(unwrap_type_byte_size(rout_ret_tp, &ret_sz));
    if (!is_generic_libatomic_call) {
        a_boolean ret_ok = TRUE;
        a_boolean elem_ok = TRUE;
        GCC_GEN_BE_CHECK(is_supported_atomic_width(ret_sz, &ret_ok));
        GCC_GEN_BE_CHECK(is_supported_atomic_width(atomic_elem_sz, &elem_ok));
        if ((ret_sz > 0 && !ret_ok) || (atomic_elem_sz > 0 && !elem_ok)) {
            return GCC_GEN_BE_ERROR_ATOMIC_ALIGNMENT_FAILED;
        }
    }
    if (atomic_elem_sz == 0) {
        atomic_elem_sz = ret_sz;
    }
    if (atomic_elem_sz == 0 && num_args >= 2 && args[1]) {
        ssize_t a1_sz = 0;
        a_boolean a1_ok = FALSE;
        GCC_GEN_BE_CHECK(safe_jit_scalar_size(ctx, gcc_jit_rvalue_get_type(args[1]), &a1_sz));
        GCC_GEN_BE_CHECK(is_supported_atomic_width((long)a1_sz, &a1_ok));
        if (a1_ok) {
            atomic_elem_sz = (long)a1_sz;
        }
    }
    if (atomic_elem_sz == 0 && strcmp(name, "__atomic_load_n") == 0) {
        ssize_t r_sz = 0;
        a_boolean r_ok = FALSE;
        GCC_GEN_BE_CHECK(safe_jit_scalar_size(ctx, ret_type, &r_sz));
        GCC_GEN_BE_CHECK(is_supported_atomic_width((long)r_sz, &r_ok));
        if (r_ok) {
            atomic_elem_sz = (long)r_sz;
        }
    }
    char sized_atomic_name[128];
    const char *resolved_name = name;
    a_boolean is_16byte_atomic = (atomic_elem_sz == 16) ? TRUE : FALSE;
    a_boolean elem_width_ok = FALSE;
    GCC_GEN_BE_CHECK(is_supported_atomic_width(atomic_elem_sz, &elem_width_ok));
    if (elem_width_ok) {
        static const char *const k_rmw_prefixes[] = {
            "__atomic_fetch_", "__atomic_add_fetch", "__atomic_sub_fetch",
            "__atomic_and_fetch", "__atomic_xor_fetch", "__atomic_or_fetch", "__atomic_nand_fetch",
            NULL
        };
        static const char *const k_n_atomics[] = {
            "__atomic_load_n", "__atomic_store_n", "__atomic_exchange_n", "__atomic_compare_exchange_n", NULL
        };
        a_boolean is_rmw_pfx = FALSE;
        a_boolean is_n_atomic = FALSE;
        GCC_GEN_BE_CHECK(name_has_any_prefix(name, k_rmw_prefixes, &is_rmw_pfx));
        GCC_GEN_BE_CHECK(name_equals_any(name, k_n_atomics, &is_n_atomic));
        char last_ch = name[strlen(name) - 1];
        if (is_n_atomic) {
            size_t base_n = strlen(name) - 2U;
            snprintf(sized_atomic_name, sizeof(sized_atomic_name), "%.*s_%ld", (int)base_n, name, atomic_elem_sz);
            resolved_name = sized_atomic_name;
        } else if (is_rmw_pfx && last_ch > '9') {
            snprintf(sized_atomic_name, sizeof(sized_atomic_name), "%s_%ld", name, atomic_elem_sz);
            resolved_name = sized_atomic_name;
        }
    }

    gcc_jit_type **arg_types = NULL;
    if (num_args > 0) {
        GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)num_args * sizeof(gcc_jit_type *), (void **)&arg_types));
        for (int i = 0; i < num_args; ++i) {
            arg_types[i] = args[i] ? gcc_jit_rvalue_get_type(args[i]) : gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        }
    }

    gcc_jit_function *fn = NULL;
    gcc_gen_be_error_t err = GCC_GEN_BE_SUCCESS;
    if (is_16byte_atomic && resolved_name != name) {
        gcc_jit_param **params = NULL;
        if (num_args > 0) {
            err = gcc_gen_be_alloc((size_t)num_args * sizeof(gcc_jit_param *), (void **)&params);
            if (err != GCC_GEN_BE_SUCCESS) {
                free(arg_types);
                return err;
            }
            for (int i = 0; i < num_args; ++i) {
                char pname[32];
                snprintf(pname, sizeof(pname), "arg_%d", i);
                params[i] = gcc_jit_context_new_param(ctx, NULL, arg_types[i], pname);
            }
        }
        fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, resolved_name, num_args, params, 0);
        free(params);
        free(arg_types);
    } else {
        err = get_builtin(ctx, resolved_name, ret_type, num_args, arg_types, 0, &fn);
        free(arg_types);
        if (err != GCC_GEN_BE_SUCCESS) return err;
    }
    int pcount = (int)gcc_jit_function_get_param_count(fn);
    gcc_jit_rvalue *coerced_args[8];
    int call_argc = num_args < 8 ? num_args : 8;
    gcc_jit_type *vptr_fallback = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    for (int i = 0; i < call_argc; ++i) {
        coerced_args[i] = args[i] ? args[i] : gcc_jit_context_null(ctx, vptr_fallback);
        if (i < pcount) {
            gcc_jit_param *p = gcc_jit_function_get_param(fn, i);
            gcc_jit_type *pt = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(p));
            gcc_jit_type *at = gcc_jit_rvalue_get_type(coerced_args[i]);
            if (pt != at) {
                gcc_jit_block *cblock = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                if (gcc_jit_type_is_pointer(pt) != NULL && gcc_jit_type_is_pointer(at) == NULL) {
                    gcc_jit_type *ul_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG);
                    gcc_jit_rvalue *ul_v = (at == ul_t) ? coerced_args[i] : gcc_jit_context_new_cast(ctx, NULL, coerced_args[i], ul_t);
                    if (cblock) {
                        gcc_jit_function *cfunc = gcc_jit_block_get_function(cblock);
                        gcc_jit_lvalue *tmp = gcc_jit_function_new_local(cfunc, NULL, ul_t, "atomic_i2p");
                        gcc_jit_block_add_assignment(cblock, NULL, tmp, ul_v);
                        gcc_jit_rvalue *addr = gcc_jit_lvalue_get_address(tmp, NULL);
                        gcc_jit_rvalue *dptr = gcc_jit_context_new_cast(ctx, NULL, addr, gcc_jit_type_get_pointer(pt));
                        coerced_args[i] = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(dptr, NULL));
                    } else {
                        coerced_args[i] = gcc_jit_context_null(ctx, pt);
                    }
                } else if (gcc_jit_type_is_pointer(pt) == NULL &&
                           (gcc_jit_type_is_pointer(at) != NULL || gcc_jit_type_is_struct(at) != NULL)) {
                    if (cblock) {
                        gcc_jit_function *cfunc = gcc_jit_block_get_function(cblock);
                        gcc_jit_lvalue *tmp = gcc_jit_function_new_local(cfunc, NULL, at, "atomic_p2i");
                        gcc_jit_block_add_assignment(cblock, NULL, tmp, coerced_args[i]);
                        gcc_jit_rvalue *addr = gcc_jit_lvalue_get_address(tmp, NULL);
                        gcc_jit_rvalue *uptr = gcc_jit_context_new_cast(ctx, NULL, addr, gcc_jit_type_get_pointer(pt));
                        coerced_args[i] = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(uptr, NULL));
                    } else {
                        coerced_args[i] = gcc_jit_context_zero(ctx, pt);
                    }
                } else {
                    coerced_args[i] = gcc_jit_context_new_cast(ctx, NULL, coerced_args[i], pt);
                }
            }
        }
    }
    gcc_jit_rvalue *call_rv = gcc_jit_context_new_call(ctx, NULL, fn, call_argc, coerced_args);
    gcc_jit_type *call_rt = gcc_jit_rvalue_get_type(call_rv);
    if (call_rt != ret_type && gcc_jit_type_is_struct(ret_type) != NULL) {
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (cblock) {
            gcc_jit_function *cfunc = gcc_jit_block_get_function(cblock);
            gcc_jit_lvalue *rtmp = gcc_jit_function_new_local(cfunc, NULL, call_rt, "atomic_ret_bits");
            gcc_jit_block_add_assignment(cblock, NULL, rtmp, call_rv);
            gcc_jit_rvalue *raddr = gcc_jit_lvalue_get_address(rtmp, NULL);
            gcc_jit_rvalue *sptr = gcc_jit_context_new_cast(ctx, NULL, raddr, gcc_jit_type_get_pointer(ret_type));
            *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(sptr, NULL));
            return GCC_GEN_BE_SUCCESS;
        }
    }
    *out_rval = call_rv;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers `__builtin_bit_cast(T, src)` to a bit-preserving reinterpretation.
 * @details Validates that `dst_type` and `src_type` have identical byte sizes and bitcasts `src_val` to the lowered destination type.
 * @param[in] dst_type Target EDG type.
 * @param[in] src_type Source EDG type.
 * @param[in] src_val Lowered source rvalue.
 * @param[out] out_rval Pointer to receive the bitcast rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_TYPE_MISMATCH if sizes differ.
 */
gcc_gen_be_error_t gcc_gen_be_lower_builtin_bit_cast(
    a_type_ptr dst_type,
    a_type_ptr src_type,
    gcc_jit_rvalue *src_val,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!src_val || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    long dst_tp_sz = 0;
    long src_tp_sz = 0;
    GCC_GEN_BE_CHECK(unwrap_type_byte_size(dst_type, &dst_tp_sz));
    GCC_GEN_BE_CHECK(unwrap_type_byte_size(src_type, &src_tp_sz));
    if (dst_type && src_type && dst_tp_sz != src_tp_sz) {
        return GCC_GEN_BE_ERROR_TYPE_MISMATCH;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;
    gcc_jit_type *src_t = gcc_jit_rvalue_get_type(src_val);
    gcc_jit_type *dst_t = src_t;
    if (dst_type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(dst_type, &dst_t));
    }
    if (!dst_t || dst_t == src_t) {
        *out_rval = src_val;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    ssize_t src_sz = 0;
    ssize_t dst_sz = 0;
    GCC_GEN_BE_CHECK(safe_jit_scalar_size(ctx, src_t, &src_sz));
    GCC_GEN_BE_CHECK(safe_jit_scalar_size(ctx, dst_t, &dst_sz));
    if (src_sz == 0 && src_type) src_sz = (ssize_t)src_tp_sz;
    if (dst_sz == 0) dst_sz = (ssize_t)dst_tp_sz;
    if (cblock && src_sz > 0 && dst_sz > 0) {
        gcc_jit_function *func = gcc_jit_block_get_function(cblock);
        gcc_jit_lvalue *src_tmp = gcc_jit_function_new_local(func, NULL, src_t, "bitcast_src");
        gcc_jit_block_add_assignment(cblock, NULL, src_tmp, src_val);
        gcc_jit_rvalue *src_addr = gcc_jit_lvalue_get_address(src_tmp, NULL);
        gcc_jit_rvalue *dst_ptr = gcc_jit_context_new_cast(ctx, NULL, src_addr, gcc_jit_type_get_pointer(dst_t));
        *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(dst_ptr, NULL));
    } else if (p_gcc_jit_context_new_bitcast &&
               (dst_type->kind == tk_integer || dst_type->kind == tk_float || dst_type->kind == tk_pointer) &&
               src_sz == dst_sz) {
        *out_rval = p_gcc_jit_context_new_bitcast(ctx, NULL, src_val, dst_t);
    } else {
        *out_rval = src_val;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_validate_simd_immediate(
    const char *intrinsic_name,
    an_expr_node_ptr imm_expr,
    int min_val,
    int max_val,
    a_boolean *out_is_valid) GCC_GEN_BE_NOEXCEPT {
    if (!intrinsic_name || !out_is_valid) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_is_valid = FALSE;
    if (!imm_expr || imm_expr->kind != enk_constant || !imm_expr->variant.constant.ptr ||
        imm_expr->variant.constant.ptr->kind != ck_integer) {
        return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED, intrinsic_name);
    }
    a_boolean ovf = FALSE;
    long val = (long)value_of_integer_constant(imm_expr->variant.constant.ptr, &ovf);
    if (val < (long)min_val || val > (long)max_val) {
        return gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED, intrinsic_name);
    }
    *out_is_valid = TRUE;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_eval_builtin_object_size(
    a_type_ptr ptr_type,
    int bos_mode,
    size_t *out_bound) GCC_GEN_BE_NOEXCEPT {
    if (!out_bound) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_bound = ((bos_mode & 2) == 0) ? (size_t)-1 : 0U;

    a_type_ptr t = NULL;
    GCC_GEN_BE_CHECK(unwrap_builtin_type(ptr_type, &t));
    if (t && t->kind == tk_pointer && t->variant.pointer.type) {
        a_type_ptr pointee = NULL;
        GCC_GEN_BE_CHECK(unwrap_builtin_type(t->variant.pointer.type, &pointee));
        if (pointee && (pointee->kind == tk_struct || pointee->kind == tk_class ||
                        pointee->kind == tk_union || pointee->kind == tk_array)) {
            long sz = (long)pointee->size;
            if (sz > 0) {
                *out_bound = (size_t)sz;
            }
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_builtin_clear_padding(
    a_type_ptr ptr_type,
    gcc_jit_rvalue *ptr_val,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!ptr_val || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));

    a_type_ptr t = NULL;
    GCC_GEN_BE_CHECK(unwrap_builtin_type(ptr_type, &t));
    a_type_ptr agg = NULL;
    GCC_GEN_BE_CHECK(unwrap_builtin_type((t && t->kind == tk_pointer) ? t->variant.pointer.type : t, &agg));

    if (cblock && agg && (agg->kind == tk_struct || agg->kind == tk_class) && agg->size > 0) {
        gcc_jit_type *uchar_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
        gcc_jit_type *uchar_ptr_t = gcc_jit_type_get_pointer(uchar_t);
        gcc_jit_type *size_t_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
        gcc_jit_rvalue *byte_ptr = gcc_jit_context_new_cast(ctx, NULL, ptr_val, uchar_ptr_t);
        gcc_jit_rvalue *zero_byte = gcc_jit_context_zero(ctx, uchar_t);

        long prev_end = 0;
        long total_sz = (long)agg->size;
        for (a_field_ptr f = agg->variant.class_struct_union.field_list; f != NULL; f = f->next) {
            long f_off = (long)f->offset;
            for (long p = prev_end; p < f_off && p < total_sz; ++p) {
                gcc_jit_rvalue *idx_rv = gcc_jit_context_new_rvalue_from_long(ctx, size_t_t, p);
                gcc_jit_lvalue *pad_lv = gcc_jit_context_new_array_access(ctx, NULL, byte_ptr, idx_rv);
                gcc_jit_block_add_assignment(cblock, NULL, pad_lv, zero_byte);
            }
            long f_sz = 0;
            GCC_GEN_BE_CHECK(unwrap_type_byte_size(f->type, &f_sz));
            if (f_off + f_sz > prev_end) prev_end = f_off + f_sz;
        }
        for (long p = prev_end; p < total_sz; ++p) {
            gcc_jit_rvalue *idx_rv = gcc_jit_context_new_rvalue_from_long(ctx, size_t_t, p);
            gcc_jit_lvalue *pad_lv = gcc_jit_context_new_array_access(ctx, NULL, byte_ptr, idx_rv);
            gcc_jit_block_add_assignment(cblock, NULL, pad_lv, zero_byte);
        }
    }

    *out_rval = ptr_val;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_source_location_builtin(
    const char *name,
    const a_source_position *call_pos,
    a_type_ptr result_type,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!name || !out_rval) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }

    a_const_char *file_name = "";
    a_line_number line_num = 0;
    int col_num = 0;
    if (call_pos) {
        col_num = (int)call_pos->column;
        if (call_pos->seq != 0) {
            a_const_char *raw_fname = NULL;
            a_const_char *full_fname = NULL;
            a_boolean at_end = FALSE;
            conv_seq_to_file_and_line(call_pos->seq, &raw_fname, &full_fname, &line_num, &at_end);
            file_name = (full_fname && full_fname[0] != '\0')
                ? full_fname
                : (raw_fname ? raw_fname : "");
        }
    }
    const char *func_name = gcc_gen_be_singleton_state.active_routine_name
        ? gcc_gen_be_singleton_state.active_routine_name
        : "";

    if (strcmp(name, "__builtin_FILE") == 0) {
        *out_rval = gcc_jit_context_new_string_literal(ctx, file_name);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_FILE_NAME") == 0) {
        const char *base = file_name;
        for (const char *p = file_name; *p != '\0'; ++p) {
            if (*p == '/' || *p == '\\') {
                base = p + 1;
            }
        }
        *out_rval = gcc_jit_context_new_string_literal(ctx, base);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_FUNCTION") == 0) {
        *out_rval = gcc_jit_context_new_string_literal(ctx, func_name);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_LINE") == 0 || strcmp(name, "__builtin_COLUMN") == 0) {
        gcc_jit_type *res_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT);
        if (result_type) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(result_type, &res_t));
        }
        int val = (strcmp(name, "__builtin_LINE") == 0) ? (int)line_num : col_num;
        *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, res_t, val);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_source_location") == 0) {
        gcc_jit_type *cchar_ptr = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CONST_CHAR_PTR);
        gcc_jit_type *u32_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T);
        gcc_jit_field *sl_fields[4] = {
            gcc_jit_context_new_field(ctx, NULL, cchar_ptr, "_M_file_name"),
            gcc_jit_context_new_field(ctx, NULL, cchar_ptr, "_M_function_name"),
            gcc_jit_context_new_field(ctx, NULL, u32_t, "_M_line"),
            gcc_jit_context_new_field(ctx, NULL, u32_t, "_M_column")
        };
        static unsigned long sl_counter = 0UL;
        sl_counter++;
        char sname[64];
        snprintf(sname, sizeof(sname), "__edg_srcloc_impl_t_%lu", sl_counter);
        gcc_jit_struct *sl_st = gcc_jit_context_new_struct_type(ctx, NULL, sname, 4, sl_fields);
        gcc_jit_type *sl_type = gcc_jit_struct_as_type(sl_st);

        char gname[64];
        snprintf(gname, sizeof(gname), "__edg_srcloc_const_%lu", sl_counter);
        gcc_jit_lvalue *sl_glob = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, sl_type, gname);
        if (p_gcc_jit_context_new_struct_constructor) {
            gcc_jit_rvalue *vals[4] = {
                gcc_jit_context_new_string_literal(ctx, file_name),
                gcc_jit_context_new_string_literal(ctx, func_name),
                gcc_jit_context_new_rvalue_from_int(ctx, u32_t, (int)line_num),
                gcc_jit_context_new_rvalue_from_int(ctx, u32_t, col_num)
            };
            gcc_jit_rvalue *init_rv = p_gcc_jit_context_new_struct_constructor(ctx, NULL, sl_type, 4, sl_fields, vals);
            gcc_jit_global_set_initializer_rvalue(sl_glob, init_rv);
        }
        gcc_jit_rvalue *addr_rv = gcc_jit_lvalue_get_address(sl_glob, NULL);
        if (result_type) {
            gcc_jit_type *res_ptr_t = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(result_type, &res_ptr_t));
            if (res_ptr_t && gcc_jit_type_is_pointer(res_ptr_t) != NULL) {
                addr_rv = gcc_jit_context_new_cast(ctx, NULL, addr_rv, res_ptr_t);
            }
        }
        *out_rval = addr_rv;
        return GCC_GEN_BE_SUCCESS;
    }

    return GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN;
}

/**
 * @brief Lowers C23 `<stdbit.h>` (`__builtin_stdc_*`), type-generic bit builtins (`*g`), and `__builtin_bswap128`.
 * @details Uses 64-bit hardware `clzll`/`ctzll`/`popcountll`/`parityll`/`bswap64` primitives with exact
 * operand bit-width masking so zero inputs and 8/16/32/64-bit unsigned types obey C23 semantics.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] name Builtin name.
 * @param[in] rout Frontend routine descriptor (may be NULL).
 * @param[in] num_args Number of lowered argument rvalues.
 * @param[in] args Lowered argument rvalues.
 * @param[out] out_handled Pointer receiving `TRUE` if lowered, else `FALSE`.
 * @param[out] out_rval Pointer receiving the lowered result rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t try_lower_c23_or_generic_bit_builtin(
    gcc_jit_context *ctx,
    const char *name,
    a_routine_ptr rout,
    int num_args,
    gcc_jit_rvalue **args,
    a_boolean *out_handled,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    *out_handled = FALSE;
    if (num_args < 1 || !args[0]) {
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__builtin_bswap128") == 0) {
        gcc_jit_type *u128_t = gcc_jit_rvalue_get_type(args[0]);
        gcc_jit_type *u64_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
        gcc_jit_function *bs64 = NULL;
        GCC_GEN_BE_CHECK(get_builtin(ctx, "__builtin_bswap64", u64_t, 1, &u64_t, 0, &bs64));
        gcc_jit_rvalue *sh64 = gcc_jit_context_new_rvalue_from_int(ctx, u128_t, 64);
        gcc_jit_rvalue *lo64 = gcc_jit_context_new_cast(ctx, NULL, args[0], u64_t);
        gcc_jit_rvalue *hi64 = gcc_jit_context_new_cast(
            ctx, NULL, gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_RSHIFT, u128_t, args[0], sh64), u64_t);
        gcc_jit_rvalue *sw_lo = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_context_new_call(ctx, NULL, bs64, 1, &lo64), u128_t);
        gcc_jit_rvalue *sw_hi = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_context_new_call(ctx, NULL, bs64, 1, &hi64), u128_t);
        *out_rval = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_OR, u128_t,
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, u128_t, sw_lo, sh64),
            sw_hi);
        *out_handled = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    static const char *const k_gen_bit_names[] = {
        "__builtin_clzg", "__builtin_ctzg", "__builtin_popcountg",
        "__builtin_ffsg", "__builtin_parityg", NULL
    };
    a_boolean is_stdc = (strncmp(name, "__builtin_stdc_", 15) == 0) ? TRUE : FALSE;
    a_boolean is_gen_bit = FALSE;
    GCC_GEN_BE_CHECK(name_equals_any(name, k_gen_bit_names, &is_gen_bit));
    if (!is_stdc && !is_gen_bit) {
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!cblock) {
        return GCC_GEN_BE_ERROR_INVALID_STATE;
    }

    gcc_jit_type *arg0_t = gcc_jit_rvalue_get_type(args[0]);
    a_param_type_ptr first_param = NULL;
    GCC_GEN_BE_CHECK(get_routine_sig_info(rout, NULL, &first_param));
    int bit_width = 32;
    if (first_param && first_param->type && first_param->type->size > 0) {
        bit_width = (int)(first_param->type->size * 8U);
    } else {
        ssize_t a0_sz = 0;
        GCC_GEN_BE_CHECK(safe_jit_scalar_size(ctx, arg0_t, &a0_sz));
        if (a0_sz > 0) {
            bit_width = (int)(a0_sz * 8);
        }
    }
    if (bit_width > 64) bit_width = 64;

    gcc_jit_type *u64_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
    gcc_jit_type *uint_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

    unsigned long long mask_ull = (bit_width >= 64) ? ~0ULL : ((1ULL << bit_width) - 1ULL);
    gcc_jit_rvalue *mask_rv = gcc_jit_context_new_rvalue_from_long(ctx, u64_t, (long)mask_ull);
    gcc_jit_rvalue *u64_raw = gcc_jit_context_new_cast(ctx, NULL, args[0], u64_t);
    gcc_jit_rvalue *u64_val = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_AND, u64_t, u64_raw, mask_rv);
    gcc_jit_rvalue *u64_inv = gcc_jit_context_new_binary_op(
        ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_AND, u64_t,
        gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_BITWISE_NEGATE, u64_t, u64_val), mask_rv);

    gcc_jit_function *fn_clz = NULL;
    gcc_jit_function *fn_ctz = NULL;
    gcc_jit_function *fn_pop = NULL;
    gcc_jit_function *fn_par = NULL;
    GCC_GEN_BE_CHECK(get_builtin(ctx, "__builtin_clzll", int_t, 1, &u64_t, 0, &fn_clz));
    GCC_GEN_BE_CHECK(get_builtin(ctx, "__builtin_ctzll", int_t, 1, &u64_t, 0, &fn_ctz));
    GCC_GEN_BE_CHECK(get_builtin(ctx, "__builtin_popcountll", int_t, 1, &u64_t, 0, &fn_pop));
    GCC_GEN_BE_CHECK(get_builtin(ctx, "__builtin_parityll", int_t, 1, &u64_t, 0, &fn_par));

    if (strcmp(name, "__builtin_popcountg") == 0 || strcmp(name, "__builtin_stdc_count_ones") == 0) {
        gcc_jit_rvalue *cnt = gcc_jit_context_new_call(ctx, NULL, fn_pop, 1, &u64_val);
        *out_rval = is_stdc ? gcc_jit_context_new_cast(ctx, NULL, cnt, uint_t) : cnt;
        *out_handled = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_stdc_count_zeros") == 0) {
        gcc_jit_rvalue *ones = gcc_jit_context_new_call(ctx, NULL, fn_pop, 1, &u64_val);
        gcc_jit_rvalue *bw_rv = gcc_jit_context_new_rvalue_from_int(ctx, int_t, bit_width);
        *out_rval = gcc_jit_context_new_cast(
            ctx, NULL, gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, int_t, bw_rv, ones), uint_t);
        *out_handled = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_parityg") == 0) {
        *out_rval = gcc_jit_context_new_call(ctx, NULL, fn_par, 1, &u64_val);
        *out_handled = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_stdc_has_single_bit") == 0) {
        gcc_jit_rvalue *ones = gcc_jit_context_new_call(ctx, NULL, fn_pop, 1, &u64_val);
        *out_rval = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, ones, gcc_jit_context_one(ctx, int_t));
        *out_handled = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_stdc_rotate_left") == 0 || strcmp(name, "__builtin_stdc_rotate_right") == 0) {
        if (num_args < 2 || !args[1]) return GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH;
        gcc_jit_rvalue *bw_u64 = gcc_jit_context_new_rvalue_from_int(ctx, u64_t, bit_width);
        gcc_jit_rvalue *sh_raw = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_MODULO, u64_t, gcc_jit_context_new_cast(ctx, NULL, args[1], u64_t), bw_u64);
        gcc_jit_rvalue *sh_opp = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_MODULO, u64_t,
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, u64_t, bw_u64, sh_raw), bw_u64);
        a_boolean is_left = (strcmp(name, "__builtin_stdc_rotate_left") == 0) ? TRUE : FALSE;
        gcc_jit_rvalue *p1 = gcc_jit_context_new_binary_op(
            ctx, NULL, is_left ? GCC_JIT_BINARY_OP_LSHIFT : GCC_JIT_BINARY_OP_RSHIFT, u64_t, u64_val, sh_raw);
        gcc_jit_rvalue *p2 = gcc_jit_context_new_binary_op(
            ctx, NULL, is_left ? GCC_JIT_BINARY_OP_RSHIFT : GCC_JIT_BINARY_OP_LSHIFT, u64_t, u64_val, sh_opp);
        gcc_jit_rvalue *comb = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_AND, u64_t,
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_OR, u64_t, p1, p2), mask_rv);
        *out_rval = gcc_jit_context_new_cast(ctx, NULL, comb, arg0_t);
        *out_handled = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    /* Conditional zero-safe bit scan / bit_width / bit_floor / bit_ceil helpers */
    gcc_jit_function *cur_fn = gcc_jit_block_get_function(cblock);
    gcc_jit_rvalue *target_v = (strcmp(name, "__builtin_stdc_leading_ones") == 0 ||
                                strcmp(name, "__builtin_stdc_trailing_ones") == 0 ||
                                strcmp(name, "__builtin_stdc_first_leading_zero") == 0 ||
                                strcmp(name, "__builtin_stdc_first_trailing_zero") == 0)
        ? u64_inv
        : u64_val;

    gcc_jit_block *nz_blk = gcc_jit_function_new_block(cur_fn, "bit_op_nz");
    gcc_jit_block *zr_blk = gcc_jit_function_new_block(cur_fn, "bit_op_zr");
    gcc_jit_block *mg_blk = gcc_jit_function_new_block(cur_fn, "bit_op_mg");

    if (strcmp(name, "__builtin_stdc_bit_floor") == 0 || strcmp(name, "__builtin_stdc_bit_ceil") == 0) {
        gcc_jit_lvalue *res_u64 = gcc_jit_function_new_local(cur_fn, NULL, u64_t, "__stdc_pow2");
        if (strcmp(name, "__builtin_stdc_bit_floor") == 0) {
            gcc_jit_rvalue *is_nz = gcc_jit_context_new_comparison(
                ctx, NULL, GCC_JIT_COMPARISON_NE, u64_val, gcc_jit_context_zero(ctx, u64_t));
            gcc_jit_block_end_with_conditional(cblock, NULL, is_nz, nz_blk, zr_blk);
            gcc_jit_rvalue *clz = gcc_jit_context_new_call(ctx, NULL, fn_clz, 1, &u64_val);
            gcc_jit_rvalue *sh = gcc_jit_context_new_cast(
                ctx, NULL,
                gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, int_t, gcc_jit_context_new_rvalue_from_int(ctx, int_t, 63), clz),
                u64_t);
            gcc_jit_block_add_assignment(
                nz_blk, NULL, res_u64,
                gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, u64_t, gcc_jit_context_one(ctx, u64_t), sh));
            gcc_jit_block_end_with_jump(nz_blk, NULL, mg_blk);
            gcc_jit_block_add_assignment(zr_blk, NULL, res_u64, gcc_jit_context_zero(ctx, u64_t));
            gcc_jit_block_end_with_jump(zr_blk, NULL, mg_blk);
        } else {
            gcc_jit_rvalue *gt_one = gcc_jit_context_new_comparison(
                ctx, NULL, GCC_JIT_COMPARISON_GT, u64_val, gcc_jit_context_one(ctx, u64_t));
            gcc_jit_block_end_with_conditional(cblock, NULL, gt_one, nz_blk, zr_blk);
            gcc_jit_rvalue *minus_one = gcc_jit_context_new_binary_op(
                ctx, NULL, GCC_JIT_BINARY_OP_MINUS, u64_t, u64_val, gcc_jit_context_one(ctx, u64_t));
            gcc_jit_rvalue *clz = gcc_jit_context_new_call(ctx, NULL, fn_clz, 1, &minus_one);
            gcc_jit_rvalue *sh = gcc_jit_context_new_cast(
                ctx, NULL,
                gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, int_t, gcc_jit_context_new_rvalue_from_int(ctx, int_t, 64), clz),
                u64_t);
            gcc_jit_block_add_assignment(
                nz_blk, NULL, res_u64,
                gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, u64_t, gcc_jit_context_one(ctx, u64_t), sh));
            gcc_jit_block_end_with_jump(nz_blk, NULL, mg_blk);
            gcc_jit_block_add_assignment(zr_blk, NULL, res_u64, gcc_jit_context_one(ctx, u64_t));
            gcc_jit_block_end_with_jump(zr_blk, NULL, mg_blk);
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(mg_blk));
        *out_rval = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(res_u64), arg0_t);
        *out_handled = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_lvalue *res_i = gcc_jit_function_new_local(cur_fn, NULL, int_t, "__stdc_bit_res");
    gcc_jit_rvalue *is_nz = gcc_jit_context_new_comparison(
        ctx, NULL, GCC_JIT_COMPARISON_NE, target_v, gcc_jit_context_zero(ctx, u64_t));
    gcc_jit_block_end_with_conditional(cblock, NULL, is_nz, nz_blk, zr_blk);

    a_boolean uses_leading = (strcmp(name, "__builtin_clzg") == 0 ||
                              strcmp(name, "__builtin_stdc_leading_zeros") == 0 ||
                              strcmp(name, "__builtin_stdc_leading_ones") == 0 ||
                              strcmp(name, "__builtin_stdc_first_leading_one") == 0 ||
                              strcmp(name, "__builtin_stdc_first_leading_zero") == 0 ||
                              strcmp(name, "__builtin_stdc_bit_width") == 0) ? TRUE : FALSE;
    a_boolean is_first_idx = (strcmp(name, "__builtin_ffsg") == 0 ||
                              strncmp(name, "__builtin_stdc_first_", 21) == 0) ? TRUE : FALSE;

    gcc_jit_rvalue *nz_val = NULL;
    if (uses_leading) {
        gcc_jit_rvalue *clz64 = gcc_jit_context_new_call(ctx, NULL, fn_clz, 1, &target_v);
        if (strcmp(name, "__builtin_stdc_bit_width") == 0) {
            nz_val = gcc_jit_context_new_binary_op(
                ctx, NULL, GCC_JIT_BINARY_OP_MINUS, int_t, gcc_jit_context_new_rvalue_from_int(ctx, int_t, 64), clz64);
        } else {
            gcc_jit_rvalue *adj = gcc_jit_context_new_rvalue_from_int(ctx, int_t, 64 - bit_width);
            nz_val = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, int_t, clz64, adj);
        }
    } else {
        nz_val = gcc_jit_context_new_call(ctx, NULL, fn_ctz, 1, &target_v);
    }
    if (is_first_idx) {
        nz_val = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_PLUS, int_t, nz_val, gcc_jit_context_one(ctx, int_t));
    }
    gcc_jit_block_add_assignment(nz_blk, NULL, res_i, nz_val);
    gcc_jit_block_end_with_jump(nz_blk, NULL, mg_blk);

    gcc_jit_rvalue *zr_val = NULL;
    if ((strcmp(name, "__builtin_clzg") == 0 || strcmp(name, "__builtin_ctzg") == 0) && num_args >= 2 && args[1]) {
        zr_val = gcc_jit_context_new_cast(ctx, NULL, args[1], int_t);
    } else if (is_first_idx || strcmp(name, "__builtin_stdc_bit_width") == 0) {
        zr_val = gcc_jit_context_zero(ctx, int_t);
    } else {
        zr_val = gcc_jit_context_new_rvalue_from_int(ctx, int_t, bit_width);
    }
    gcc_jit_block_add_assignment(zr_blk, NULL, res_i, zr_val);
    gcc_jit_block_end_with_jump(zr_blk, NULL, mg_blk);

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(mg_blk));
    *out_rval = is_stdc
        ? gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(res_i), uint_t)
        : gcc_jit_lvalue_as_rvalue(res_i);
    *out_handled = TRUE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers math, bitwise, and overflow builtins.
 *
 * @details Resolves standard GCC math/bitwise/overflow builtins and emits a call
 * supporting arbitrary argument counts without clamping.
 *
 * @param[in] name The builtin name.
 * @param[in] rout The routine pointer.
 * @param[in] num_args Number of arguments.
 * @param[in] args Array of lowered argument rvalues.
 * @param[out] out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_math_bitwise_builtin(const char *name, a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    a_type_ptr rout_ret_tp = NULL;
    a_param_type_ptr first_param = NULL;
    GCC_GEN_BE_CHECK(get_routine_sig_info(rout, &rout_ret_tp, &first_param));

    static const char *const k_srcloc_names[] = {
        "__builtin_FILE", "__builtin_FILE_NAME", "__builtin_LINE",
        "__builtin_COLUMN", "__builtin_FUNCTION", "__builtin_source_location", NULL
    };
    a_boolean is_srcloc = FALSE;
    GCC_GEN_BE_CHECK(name_equals_any(name, k_srcloc_names, &is_srcloc));
    if (is_srcloc) {
        return gcc_gen_be_lower_source_location_builtin(name, &rout->source_corresp.decl_position, rout_ret_tp, out_rval);
    }
    if ((strcmp(name, "__builtin_addressof") == 0 || strcmp(name, "__addressof") == 0) &&
        num_args >= 1 && args[0]) {
        *out_rval = args[0];
        return GCC_GEN_BE_SUCCESS;
    }
    {
        a_boolean c23_handled = FALSE;
        GCC_GEN_BE_CHECK(try_lower_c23_or_generic_bit_builtin(ctx, name, rout, num_args, args, &c23_handled, out_rval));
        if (c23_handled) {
            return GCC_GEN_BE_SUCCESS;
        }
    }

    static const char *const k_reduce_names[] = {
        "__builtin_reduce_or", "__builtin_reduce_and", "__builtin_reduce_add", "__builtin_reduce_xor", NULL
    };
    a_boolean is_reduce = FALSE;
    GCC_GEN_BE_CHECK(name_equals_any(name, k_reduce_names, &is_reduce));
    if (is_reduce && num_args >= 1 && args[0]) {
        gcc_jit_block *cblk = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblk));
        if (cblk) {
            gcc_jit_type *vt = gcc_jit_rvalue_get_type(args[0]);
            gcc_jit_type *rt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            int nlanes = 4;
            if (rout_ret_tp) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(rout_ret_tp, &rt));
                if (!rt) rt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            }
            if (first_param && first_param->type) {
                a_type_ptr vtp = NULL;
                GCC_GEN_BE_CHECK(unwrap_builtin_type(first_param->type, &vtp));
                if (vtp && vtp->kind == tk_vector && vtp->variant.vector.element_type &&
                    vtp->variant.vector.element_type->size > 0) {
                    nlanes = (int)(vtp->size / vtp->variant.vector.element_type->size);
                }
            }
            if (nlanes < 1) nlanes = 1;
            gcc_jit_function *cfn = gcc_jit_block_get_function(cblk);
            gcc_jit_lvalue *vslot = gcc_jit_function_new_local(cfn, NULL, vt, "__red_vec");
            gcc_jit_block_add_assignment(cblk, NULL, vslot, args[0]);
            gcc_jit_rvalue *lptr = gcc_jit_context_new_cast(
                ctx, NULL, gcc_jit_lvalue_get_address(vslot, NULL), gcc_jit_type_get_pointer(rt));
            enum gcc_jit_binary_op rop = GCC_JIT_BINARY_OP_BITWISE_OR;
            if (strcmp(name, "__builtin_reduce_and") == 0) rop = GCC_JIT_BINARY_OP_BITWISE_AND;
            else if (strcmp(name, "__builtin_reduce_add") == 0) rop = GCC_JIT_BINARY_OP_PLUS;
            else if (strcmp(name, "__builtin_reduce_xor") == 0) rop = GCC_JIT_BINARY_OP_BITWISE_XOR;
            gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            gcc_jit_rvalue *acc = NULL;
            for (int li = 0; li < nlanes; ++li) {
                gcc_jit_rvalue *idx = gcc_jit_context_new_rvalue_from_int(ctx, int_t, li);
                gcc_jit_rvalue *elt = gcc_jit_lvalue_as_rvalue(gcc_jit_context_new_array_access(ctx, NULL, lptr, idx));
                acc = (li == 0) ? elt : gcc_jit_context_new_binary_op(ctx, NULL, rop, rt, acc, elt);
            }
            *out_rval = acc;
            return GCC_GEN_BE_SUCCESS;
        }
    }

    if (strcmp(name, "__builtin_assume") == 0) {
        *out_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }

    if ((strncmp(name, "__builtin_addc", 14) == 0 || strncmp(name, "__builtin_subc", 14) == 0) &&
        num_args >= 4 && args[0]) {
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (cblock) {
            gcc_jit_function *cfunc = gcc_jit_block_get_function(cblock);
            gcc_jit_type *ut = gcc_jit_rvalue_get_type(args[0]);
            a_boolean is_add = (strncmp(name, "__builtin_addc", 14) == 0) ? TRUE : FALSE;
            enum gcc_jit_binary_op bop = is_add ? GCC_JIT_BINARY_OP_PLUS : GCC_JIT_BINARY_OP_MINUS;
            gcc_jit_lvalue *lx = gcc_jit_function_new_local(cfunc, NULL, ut, "addc_x");
            gcc_jit_lvalue *ly = gcc_jit_function_new_local(cfunc, NULL, ut, "addc_y");
            gcc_jit_lvalue *lcin = gcc_jit_function_new_local(cfunc, NULL, ut, "addc_cin");
            gcc_jit_lvalue *ls1 = gcc_jit_function_new_local(cfunc, NULL, ut, "addc_s1");
            gcc_jit_lvalue *ls2 = gcc_jit_function_new_local(cfunc, NULL, ut, "addc_s2");
            gcc_jit_block_add_assignment(cblock, NULL, lx, args[0]);
            gcc_jit_block_add_assignment(cblock, NULL, ly, args[1]);
            gcc_jit_block_add_assignment(cblock, NULL, lcin, args[2]);
            gcc_jit_rvalue *rx = gcc_jit_lvalue_as_rvalue(lx);
            gcc_jit_rvalue *ry = gcc_jit_lvalue_as_rvalue(ly);
            gcc_jit_rvalue *rcin = gcc_jit_lvalue_as_rvalue(lcin);
            gcc_jit_block_add_assignment(
                cblock, NULL, ls1, gcc_jit_context_new_binary_op(ctx, NULL, bop, ut, rx, ry));
            gcc_jit_rvalue *rs1 = gcc_jit_lvalue_as_rvalue(ls1);
            gcc_jit_rvalue *c1 = gcc_jit_context_new_comparison(
                ctx, NULL, GCC_JIT_COMPARISON_LT, is_add ? rs1 : rx, is_add ? rx : ry);
            gcc_jit_block_add_assignment(
                cblock, NULL, ls2, gcc_jit_context_new_binary_op(ctx, NULL, bop, ut, rs1, rcin));
            gcc_jit_rvalue *rs2 = gcc_jit_lvalue_as_rvalue(ls2);
            gcc_jit_rvalue *c2 = gcc_jit_context_new_comparison(
                ctx, NULL, GCC_JIT_COMPARISON_LT, is_add ? rs2 : rs1, is_add ? rs1 : rcin);
            gcc_jit_rvalue *co = gcc_jit_context_new_binary_op(
                ctx, NULL, GCC_JIT_BINARY_OP_PLUS, ut,
                gcc_jit_context_new_cast(ctx, NULL, c1, ut),
                gcc_jit_context_new_cast(ctx, NULL, c2, ut));
            gcc_jit_type *ptr_ut = gcc_jit_type_get_pointer(ut);
            gcc_jit_lvalue *co_lv = gcc_jit_rvalue_dereference(
                gcc_jit_context_new_cast(ctx, NULL, args[3], ptr_ut), NULL);
            gcc_jit_block_add_assignment(cblock, NULL, co_lv, co);
            *out_rval = rs2;
            return GCC_GEN_BE_SUCCESS;
        }
    }

    if (strcmp(name, "__builtin_unreachable") == 0 || strcmp(name, "__builtin_trap") == 0) {
        gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_function *fn_unreach = NULL;
        GCC_GEN_BE_CHECK(get_builtin(ctx, name, void_t, 0, NULL, 0, &fn_unreach));
        *out_rval = gcc_jit_context_new_call(ctx, NULL, fn_unreach, 0, NULL);
        return GCC_GEN_BE_SUCCESS;
    }

    static const char *const k_fp_pred_prefixes[] = {
        "__builtin_isnan", "__builtin_isinf", "__builtin_isfinite", "__builtin_isnormal",
        "__builtin_signbit", NULL
    };
    a_boolean is_fp_pred = FALSE;
    GCC_GEN_BE_CHECK(name_has_any_prefix(name, k_fp_pred_prefixes, &is_fp_pred));
    if (is_fp_pred && num_args >= 1 && args[0]) {
        gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        gcc_jit_rvalue *x = args[0];
        gcc_jit_type *xt = gcc_jit_rvalue_get_type(x);
        gcc_jit_rvalue *z = gcc_jit_context_new_rvalue_from_double(ctx, xt, 0.0);
        if (strncmp(name, "__builtin_signbit", 17) == 0) {
            const char *cs_name = "__builtin_copysign";
            if (xt == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT)) {
                cs_name = "__builtin_copysignf";
            } else if (xt == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE)) {
                cs_name = "__builtin_copysignl";
            }
            gcc_jit_function *cs_fn = gcc_jit_context_get_builtin_function(ctx, cs_name);
            gcc_jit_rvalue *cs_args[2] = {
                gcc_jit_context_new_rvalue_from_double(ctx, xt, 1.0), x
            };
            gcc_jit_rvalue *cs_rv = gcc_jit_context_new_call(ctx, NULL, cs_fn, 2, cs_args);
            gcc_jit_rvalue *is_neg = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_LT, cs_rv, z);
            *out_rval = gcc_jit_context_new_cast(ctx, NULL, is_neg, int_t);
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_rvalue *not_nan = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, x, x);
        if (strncmp(name, "__builtin_isnan", 15) == 0) {
            gcc_jit_rvalue *is_nan = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, x, x);
            *out_rval = gcc_jit_context_new_cast(ctx, NULL, is_nan, int_t);
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
        gcc_jit_rvalue *pos_inf = gcc_jit_context_new_rvalue_from_double(ctx, xt, __builtin_inf());
        gcc_jit_rvalue *neg_inf = gcc_jit_context_new_rvalue_from_double(ctx, xt, -__builtin_inf());
        gcc_jit_rvalue *is_inf = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_LOGICAL_OR, bool_t,
            gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, x, pos_inf),
            gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, x, neg_inf));
        gcc_jit_rvalue *not_inf = gcc_jit_context_new_unary_op(
            ctx, NULL, GCC_JIT_UNARY_OP_LOGICAL_NEGATE, bool_t, is_inf);
        if (strncmp(name, "__builtin_isfinite", 18) == 0) {
            gcc_jit_rvalue *fin = gcc_jit_context_new_binary_op(
                ctx, NULL, GCC_JIT_BINARY_OP_LOGICAL_AND, bool_t, not_nan, not_inf);
            *out_rval = gcc_jit_context_new_cast(ctx, NULL, fin, int_t);
            return GCC_GEN_BE_SUCCESS;
        }
        if (strncmp(name, "__builtin_isinf", 15) == 0) {
            *out_rval = gcc_jit_context_new_cast(ctx, NULL, is_inf, int_t);
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_rvalue *fin_b = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_LOGICAL_AND, bool_t, not_nan, not_inf);
        gcc_jit_rvalue *fin_nz = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_LOGICAL_AND, bool_t,
            fin_b,
            gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, x, z));
        *out_rval = gcc_jit_context_new_cast(ctx, NULL, fin_nz, int_t);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_fpclassify") == 0 && num_args >= 6 && args[5]) {
        gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
        gcc_jit_rvalue *x = args[5];
        gcc_jit_type *xt = gcc_jit_rvalue_get_type(x);
        gcc_jit_rvalue *z = gcc_jit_context_new_rvalue_from_double(ctx, xt, 0.0);
        gcc_jit_rvalue *pos_inf = gcc_jit_context_new_rvalue_from_double(ctx, xt, __builtin_inf());
        gcc_jit_rvalue *neg_inf = gcc_jit_context_new_rvalue_from_double(ctx, xt, -__builtin_inf());
        gcc_jit_rvalue *is_inf = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_LOGICAL_OR, bool_t,
            gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, x, pos_inf),
            gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, x, neg_inf));
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (cblock) {
            gcc_jit_function *cfn = gcc_jit_block_get_function(cblock);
            gcc_jit_lvalue *fpc_res = gcc_jit_function_new_local(cfn, NULL, int_t, "__fpcls");
            gcc_jit_block *b_nan = gcc_jit_function_new_block(cfn, "fpc_nan");
            gcc_jit_block *b_chk_inf = gcc_jit_function_new_block(cfn, "fpc_chk_inf");
            gcc_jit_block *b_inf = gcc_jit_function_new_block(cfn, "fpc_inf");
            gcc_jit_block *b_chk_z = gcc_jit_function_new_block(cfn, "fpc_chk_z");
            gcc_jit_block *b_zero = gcc_jit_function_new_block(cfn, "fpc_zero");
            gcc_jit_block *b_norm = gcc_jit_function_new_block(cfn, "fpc_norm");
            gcc_jit_block *b_done = gcc_jit_function_new_block(cfn, "fpc_done");
            gcc_jit_block_end_with_conditional(
                cblock, NULL, gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, x, x), b_nan, b_chk_inf);
            gcc_jit_block_add_assignment(b_nan, NULL, fpc_res, gcc_jit_context_new_cast(ctx, NULL, args[0], int_t));
            gcc_jit_block_end_with_jump(b_nan, NULL, b_done);
            gcc_jit_block_end_with_conditional(b_chk_inf, NULL, is_inf, b_inf, b_chk_z);
            gcc_jit_block_add_assignment(b_inf, NULL, fpc_res, gcc_jit_context_new_cast(ctx, NULL, args[1], int_t));
            gcc_jit_block_end_with_jump(b_inf, NULL, b_done);
            gcc_jit_block_end_with_conditional(
                b_chk_z, NULL, gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, x, z), b_zero, b_norm);
            gcc_jit_block_add_assignment(b_zero, NULL, fpc_res, gcc_jit_context_new_cast(ctx, NULL, args[4], int_t));
            gcc_jit_block_end_with_jump(b_zero, NULL, b_done);
            gcc_jit_block_add_assignment(b_norm, NULL, fpc_res, gcc_jit_context_new_cast(ctx, NULL, args[2], int_t));
            gcc_jit_block_end_with_jump(b_norm, NULL, b_done);
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(b_done));
            *out_rval = gcc_jit_lvalue_as_rvalue(fpc_res);
            return GCC_GEN_BE_SUCCESS;
        }
    }

    static const char *const k_passthrough_arg0[] = {
        "__builtin_launder", "__builtin_assume_aligned", "__builtin_expect",
        "__builtin_expect_with_probability", "__builtin_extract_return_addr", NULL
    };
    a_boolean is_pt_arg0 = FALSE;
    GCC_GEN_BE_CHECK(name_equals_any(name, k_passthrough_arg0, &is_pt_arg0));
    if (is_pt_arg0 && num_args > 0 && args[0]) {
        *out_rval = args[0];
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_frame_address") == 0 || strcmp(name, "__builtin_return_address") == 0 ||
        strcmp(name, "__builtin_dwarf_cfa") == 0) {
        gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_type *uint_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT);
        gcc_jit_function *fn_addr = NULL;
        GCC_GEN_BE_CHECK(get_builtin(ctx, name, void_ptr_t, num_args > 0 ? 1 : 0, &uint_t, 0, &fn_addr));
        gcc_jit_rvalue *zero_u = gcc_jit_context_new_rvalue_from_int(ctx, uint_t, 0);
        gcc_jit_rvalue *call_arg = (num_args > 0 && args[0])
            ? gcc_jit_context_new_cast(ctx, NULL, args[0], uint_t)
            : zero_u;
        *out_rval = gcc_jit_context_new_call(ctx, NULL, fn_addr, num_args > 0 ? 1 : 0, &call_arg);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_shufflevector") == 0 || strcmp(name, "__builtin_shuffle") == 0) {
        if (num_args >= 2 && args[0] && args[1]) {
            if (p_gcc_jit_context_new_rvalue_vector_perm && num_args >= 3 && args[2]) {
                *out_rval = p_gcc_jit_context_new_rvalue_vector_perm(ctx, NULL, args[0], args[1], args[2]);
                return GCC_GEN_BE_SUCCESS;
            }
            *out_rval = args[0];
            return GCC_GEN_BE_SUCCESS;
        }
    }
    if (strcmp(name, "__builtin_convertvector") == 0 && num_args >= 1 && args[0]) {
        gcc_jit_type *src_t = gcc_jit_rvalue_get_type(args[0]);
        gcc_jit_type *dst_t = src_t;
        if (rout_ret_tp && rout_ret_tp->kind == tk_vector) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(rout_ret_tp, &dst_t));
        }
        *out_rval = (dst_t == src_t)
            ? args[0]
            : (p_gcc_jit_context_new_bitcast
                ? p_gcc_jit_context_new_bitcast(ctx, NULL, args[0], dst_t)
                : args[0]);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_is_constant_evaluated") == 0 || strcmp(name, "__builtin_constant_p") == 0) {
        int is_const = (strcmp(name, "__builtin_constant_p") == 0 && rout->compiler_generated) ? 1 : 0;
        *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), is_const);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_va_arg_pack") == 0 || strcmp(name, "__builtin_va_arg_pack_len") == 0) {
        int pack_val = (strcmp(name, "__builtin_va_arg_pack_len") == 0 && num_args > 0) ? num_args : 0;
        *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), pack_val);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_object_size") == 0 || strcmp(name, "__builtin_dynamic_object_size") == 0) {
        a_type_ptr p0_type = first_param ? first_param->type : NULL;
        size_t bos_bound = (size_t)-1;
        GCC_GEN_BE_CHECK(gcc_gen_be_eval_builtin_object_size(p0_type, 0, &bos_bound));
        gcc_jit_type *sz_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
        *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, sz_t, (long)bos_bound);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_clear_padding") == 0 && num_args >= 1 && args[0]) {
        a_type_ptr p0_type = first_param ? first_param->type : NULL;
        return gcc_gen_be_lower_builtin_clear_padding(p0_type, args[0], out_rval);
    }
    if (strncmp(name, "__builtin___", 12) == 0 && strstr(name + 12, "_chk") != NULL) {
        const char *base_start = name + 12;
        const char *chk_pos = strstr(base_start, "_chk");
        size_t base_len = (size_t)(chk_pos - base_start);
        char libc_name[64];
        if (base_len > 0 && base_len < sizeof(libc_name)) {
            memcpy(libc_name, base_start, base_len);
            libc_name[base_len] = '\0';
            int forwarded_args = (num_args > 1) ? (num_args - 1) : num_args;
            if (strcmp(libc_name, "vsnprintf") == 0 && num_args >= 6) {
                gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                gcc_jit_type *char_ptr_t = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR));
                gcc_jit_type *const_char_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CONST_CHAR_PTR);
                gcc_jit_type *sz_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
                gcc_jit_type *ap_t = args[5] ? gcc_jit_rvalue_get_type(args[5]) : char_ptr_t;
                gcc_jit_type *vsn_arg_types[4] = { char_ptr_t, sz_t, const_char_ptr_t, ap_t };
                gcc_jit_function *vsn_fn = NULL;
                GCC_GEN_BE_CHECK(get_builtin(ctx, "vsnprintf", int_t, 4, vsn_arg_types, 0, &vsn_fn));
                gcc_jit_rvalue *vsn_args[4];
                vsn_args[0] = gcc_jit_context_new_cast(ctx, NULL, args[0], char_ptr_t);
                vsn_args[1] = gcc_jit_context_new_cast(ctx, NULL, args[1], sz_t);
                vsn_args[2] = gcc_jit_context_new_cast(ctx, NULL, args[4], const_char_ptr_t);
                vsn_args[3] = args[5] ? args[5] : gcc_jit_context_null(ctx, char_ptr_t);
                *out_rval = gcc_jit_context_new_call(ctx, NULL, vsn_fn, 4, vsn_args);
                return GCC_GEN_BE_SUCCESS;
            }
            if ((strcmp(libc_name, "snprintf") == 0 || strcmp(libc_name, "vsnprintf") == 0) && num_args >= 4) {
                gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                gcc_jit_type *char_ptr_t = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR));
                gcc_jit_type *const_char_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CONST_CHAR_PTR);
                gcc_jit_type *sz_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
                gcc_jit_type *sn_arg_types[3] = { char_ptr_t, sz_t, const_char_ptr_t };
                gcc_jit_function *sn_fn = NULL;
                GCC_GEN_BE_CHECK(get_builtin(ctx, "snprintf", int_t, 3, sn_arg_types, 1, &sn_fn));
                int extra_varargs = (num_args > 5) ? (num_args - 5) : 0;
                int total_sn_args = 3 + extra_varargs;
                gcc_jit_rvalue **sn_args = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)total_sn_args * sizeof(gcc_jit_rvalue *), (void **)&sn_args));
                sn_args[0] = gcc_jit_context_new_cast(ctx, NULL, args[0], char_ptr_t);
                sn_args[1] = gcc_jit_context_new_cast(ctx, NULL, args[1], sz_t);
                sn_args[2] = gcc_jit_context_new_cast(ctx, NULL, args[4 < num_args ? 4 : (num_args - 1)], const_char_ptr_t);
                for (int v = 0; v < extra_varargs; ++v) {
                    sn_args[3 + v] = args[5 + v];
                }
                *out_rval = gcc_jit_context_new_call(ctx, NULL, sn_fn, total_sn_args, sn_args);
                free(sn_args);
                return GCC_GEN_BE_SUCCESS;
            }
            gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
            gcc_jit_type **f_types = NULL;
            gcc_jit_rvalue **f_args = NULL;
            if (forwarded_args > 0) {
                GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)forwarded_args * sizeof(gcc_jit_type *), (void **)&f_types));
                gcc_gen_be_error_t fa_err = gcc_gen_be_alloc((size_t)forwarded_args * sizeof(gcc_jit_rvalue *), (void **)&f_args);
                if (fa_err != GCC_GEN_BE_SUCCESS) {
                    free(f_types);
                    return fa_err;
                }
                for (int i = 0; i < forwarded_args; ++i) {
                    f_args[i] = args[i] ? args[i] : gcc_jit_context_null(ctx, void_ptr_t);
                    f_types[i] = gcc_jit_rvalue_get_type(f_args[i]);
                }
            }
            gcc_jit_function *libc_fn = NULL;
            gcc_gen_be_error_t l_err = get_builtin(ctx, libc_name, void_ptr_t, forwarded_args, f_types, 0, &libc_fn);
            free(f_types);
            if (l_err != GCC_GEN_BE_SUCCESS) {
                free(f_args);
                return l_err;
            }
            for (int i = 0; i < forwarded_args; ++i) {
                gcc_jit_param *fp = gcc_jit_function_get_param(libc_fn, i);
                gcc_jit_type *pt = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(fp));
                if (gcc_jit_rvalue_get_type(f_args[i]) != pt) {
                    f_args[i] = gcc_jit_context_new_cast(ctx, NULL, f_args[i], pt);
                }
            }
            *out_rval = gcc_jit_context_new_call(ctx, NULL, libc_fn, forwarded_args, f_args);
            free(f_args);
            return GCC_GEN_BE_SUCCESS;
        }
    }
    if (strcmp(name, "__builtin_classify_type") == 0) {
        int cls_code = 1;
        if (first_param && first_param->type) {
            a_type_ptr pt = NULL;
            GCC_GEN_BE_CHECK(unwrap_builtin_type(first_param->type, &pt));
            if (pt) {
                if (pt->kind == tk_void) cls_code = 0;
                else if (pt->kind == tk_integer) {
                    if (pt->variant.integer.bool_type) cls_code = 4;
                    else if (pt->variant.integer.enum_type) cls_code = 3;
                    else if (pt->size == 1) cls_code = 2;
                    else cls_code = 1;
                }
                else if (pt->kind == tk_pointer) cls_code = pt->variant.pointer.is_reference ? 6 : 5;
                else if (pt->kind == tk_ptr_to_member) cls_code = 7;
                else if (pt->kind == tk_float) cls_code = 8;
                else if (pt->kind == tk_complex) cls_code = 9;
                else if (pt->kind == tk_routine) cls_code = 10;
                else if (pt->kind == tk_struct || pt->kind == tk_class) cls_code = 12;
                else if (pt->kind == tk_union) cls_code = 13;
                else if (pt->kind == tk_array) cls_code = 14;
                else if (pt->kind == tk_vector) cls_code = 19;
            }
        }
        *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), cls_code);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_types_compatible_p") == 0) {
        int compat = 1;
        if (first_param) {
            a_param_type_ptr p1 = first_param->next;
            if (p1 && first_param->type && p1->type) {
                a_type_ptr t0 = NULL;
                a_type_ptr t1 = NULL;
                GCC_GEN_BE_CHECK(unwrap_builtin_type(first_param->type, &t0));
                GCC_GEN_BE_CHECK(unwrap_builtin_type(p1->type, &t1));
                if (t0 && t1 && (t0->kind != t1->kind || t0->size != t1->size)) {
                    compat = 0;
                }
            }
        } else if (num_args >= 2 && args[0] && args[1]) {
            if (gcc_jit_rvalue_get_type(args[0]) != gcc_jit_rvalue_get_type(args[1])) {
                compat = 0;
            }
        }
        *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), compat);
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_offsetof") == 0) {
        if (num_args >= 1 && args[num_args - 1]) {
            *out_rval = gcc_jit_context_new_cast(ctx, NULL, args[num_args - 1], gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T));
        } else {
            *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T), 0);
        }
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_choose_expr") == 0 && num_args >= 3) {
        *out_rval = args[1] ? args[1] : args[2];
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "__builtin_bit_cast") == 0 && num_args >= 1 && args[num_args - 1]) {
        return gcc_gen_be_lower_builtin_bit_cast(rout_ret_tp, NULL, args[num_args - 1], out_rval);
    }
    if (strcmp(name, "_abnormal_termination") == 0 ||
        strcmp(name, "__builtin_eh_AbnormalTermination") == 0 ||
        strcmp(name, "AbnormalTermination") == 0) {
        void *abn_lval = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_abnormal_termination", &abn_lval));
        *out_rval = abn_lval
            ? gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)abn_lval)
            : gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "_exception_code") == 0 ||
        strcmp(name, "GetExceptionCode") == 0 ||
        strcmp(name, "__exception_code") == 0) {
        void *code_lval = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_exception_code", &code_lval));
        *out_rval = code_lval
            ? gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)code_lval)
            : gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG));
        return GCC_GEN_BE_SUCCESS;
    }
    if (strcmp(name, "_exception_info") == 0 ||
        strcmp(name, "GetExceptionInformation") == 0 ||
        strcmp(name, "__exception_info") == 0) {
        void *info_lval = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_seh_exception_info", &info_lval));
        *out_rval = info_lval
            ? gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)info_lval)
            : gcc_jit_context_null(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR));
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_type *ret_type = NULL;
    if (rout_ret_tp) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(rout_ret_tp, &ret_type));
        if (!ret_type) ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    } else {
        ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    }

    gcc_jit_type *int_fallback = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type **arg_types = NULL;
    if (num_args > 0) {
        GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)num_args * sizeof(gcc_jit_type *), (void **)&arg_types));
        for (int i = 0; i < num_args; ++i) {
            arg_types[i] = args[i] ? gcc_jit_rvalue_get_type(args[i]) : int_fallback;
        }
    }

    gcc_jit_function *fn = NULL;
    gcc_gen_be_error_t err = get_builtin(ctx, name, ret_type, num_args, arg_types, 0, &fn);
    free(arg_types);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_rvalue **coerced_args = NULL;
    int fn_pcount = (int)gcc_jit_function_get_param_count(fn);
    if (num_args > 0) {
        GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)num_args * sizeof(gcc_jit_rvalue *), (void **)&coerced_args));
        for (int i = 0; i < num_args; ++i) {
            gcc_jit_rvalue *ai = args[i] ? args[i] : gcc_jit_context_zero(ctx, int_fallback);
            if (i < fn_pcount) {
                gcc_jit_type *pt = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(fn, i)));
                if (gcc_jit_rvalue_get_type(ai) != pt) {
                    ai = gcc_jit_context_new_cast(ctx, NULL, ai, pt);
                }
            }
            coerced_args[i] = ai;
        }
    }
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, num_args, coerced_args);
    free(coerced_args);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry in the deterministic SIMD intrinsic mapping table.
 * @details Maps a frontend x86 SSE/AVX/AVX-512 or ARM NEON intrinsic name to its exact GCC target builtin name
 * and optional native `libgccjit` binary vector operator (`op_kind`, or `-1` if not a direct binary op).
 */
typedef struct gcc_gen_be_simd_entry {
    /** @brief Frontend intrinsic name (e.g. `_mm_add_ps`, `vaddq_f32`). */
    const char *intrinsic_name;
    /** @brief Target GCC builtin name (e.g. `__builtin_ia32_addps`, `__builtin_neon_vaddv4sf`). */
    const char *gcc_builtin_name;
    /** @brief Native `gcc_jit_binary_op` when lowerable as a vector binary op, or `-1`. */
    int binary_op;
} gcc_gen_be_simd_entry_t;

/** @brief Deterministic lookup table for x86 SSE/AVX/AVX-512 and ARM NEON intrinsics. */
static const gcc_gen_be_simd_entry_t simd_intrinsic_table[] = {
    { "_mm_add_ps", "__builtin_ia32_addps", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm_sub_ps", "__builtin_ia32_subps", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm_mul_ps", "__builtin_ia32_mulps", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm_div_ps", "__builtin_ia32_divps", (int)GCC_JIT_BINARY_OP_DIVIDE },
    { "_mm_add_pd", "__builtin_ia32_addpd", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm_sub_pd", "__builtin_ia32_subpd", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm_mul_pd", "__builtin_ia32_mulpd", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm_div_pd", "__builtin_ia32_divpd", (int)GCC_JIT_BINARY_OP_DIVIDE },
    { "_mm_add_epi32", "__builtin_ia32_paddd128", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm_sub_epi32", "__builtin_ia32_psubd128", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm_mullo_epi32", "__builtin_ia32_pmulld128", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm_and_si128", "__builtin_ia32_pand128", (int)GCC_JIT_BINARY_OP_BITWISE_AND },
    { "_mm_or_si128", "__builtin_ia32_por128", (int)GCC_JIT_BINARY_OP_BITWISE_OR },
    { "_mm_xor_si128", "__builtin_ia32_pxor128", (int)GCC_JIT_BINARY_OP_BITWISE_XOR },
    { "_mm256_add_ps", "__builtin_ia32_addps256", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm256_sub_ps", "__builtin_ia32_subps256", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm256_mul_ps", "__builtin_ia32_mulps256", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm256_div_ps", "__builtin_ia32_divps256", (int)GCC_JIT_BINARY_OP_DIVIDE },
    { "_mm256_add_pd", "__builtin_ia32_addpd256", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm256_sub_pd", "__builtin_ia32_subpd256", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm256_mul_pd", "__builtin_ia32_mulpd256", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm256_div_pd", "__builtin_ia32_divpd256", (int)GCC_JIT_BINARY_OP_DIVIDE },
    { "_mm512_add_ps", "__builtin_ia32_addps512_mask", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm512_sub_ps", "__builtin_ia32_subps512_mask", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm512_mul_ps", "__builtin_ia32_mulps512_mask", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm512_div_ps", "__builtin_ia32_divps512_mask", (int)GCC_JIT_BINARY_OP_DIVIDE },
    { "_mm512_add_pd", "__builtin_ia32_addpd512_mask", (int)GCC_JIT_BINARY_OP_PLUS },
    { "_mm512_sub_pd", "__builtin_ia32_subpd512_mask", (int)GCC_JIT_BINARY_OP_MINUS },
    { "_mm512_mul_pd", "__builtin_ia32_mulpd512_mask", (int)GCC_JIT_BINARY_OP_MULT },
    { "_mm512_div_pd", "__builtin_ia32_divpd512_mask", (int)GCC_JIT_BINARY_OP_DIVIDE },
    { "_mm512_mask_add_ps", "__builtin_ia32_addps512_mask", -1 },
    { "_mm512_maskz_add_ps", "__builtin_ia32_addps512_maskz", -1 },
    { "_mm_fmadd_ps", "__builtin_ia32_vfmaddps", -1 },
    { "_mm256_fmadd_ps", "__builtin_ia32_vfmaddps256", -1 },
    { "_mm512_fmadd_ps", "__builtin_ia32_vfmaddps512_mask", -1 },
    { "vaddq_f32", "__builtin_neon_vaddv4sf", (int)GCC_JIT_BINARY_OP_PLUS },
    { "vsubq_f32", "__builtin_neon_vsubv4sf", (int)GCC_JIT_BINARY_OP_MINUS },
    { "vmulq_f32", "__builtin_neon_vmulv4sf", (int)GCC_JIT_BINARY_OP_MULT },
    { "vaddq_s32", "__builtin_neon_vaddv4si", (int)GCC_JIT_BINARY_OP_PLUS },
    { "vsubq_s32", "__builtin_neon_vsubv4si", (int)GCC_JIT_BINARY_OP_MINUS },
    { "vmulq_s32", "__builtin_neon_vmulv4si", (int)GCC_JIT_BINARY_OP_MULT },
    { "vdotq_s32", "__builtin_neon_vdotsv4si", -1 },
    { "vfmaq_f32", "__builtin_neon_vfmav4sf", -1 },
    { "vpaddq_f32", "__builtin_neon_vpaddv4sf", -1 },
    { "_mm_min_ps", "__builtin_ia32_minps", -1 },
    { "_mm_max_ps", "__builtin_ia32_maxps", -1 },
    { "_mm_min_pd", "__builtin_ia32_minpd", -1 },
    { "_mm_max_pd", "__builtin_ia32_maxpd", -1 },
    { "_mm256_min_ps", "__builtin_ia32_minps256", -1 },
    { "_mm256_max_ps", "__builtin_ia32_maxps256", -1 },
    { "svadd_f32_z", "__builtin_sve_svadd_f32_z", -1 },
    { "svsub_f32_z", "__builtin_sve_svsub_f32_z", -1 },
    { "svmul_f32_z", "__builtin_sve_svmul_f32_z", -1 },
    { "svld1_f32", "__builtin_sve_svld1_f32", -1 },
    { "svst1_f32", "__builtin_sve_svst1_f32", -1 },
    { "svmla_f32_z", "__builtin_sve_svmla_f32_z", -1 },
    { "svptrue_b32", "__builtin_sve_svptrue_b32", -1 },
    { "svwhilelt_b32", "__builtin_sve_svwhilelt_b32", -1 },
    { "__riscv_vadd_vv_i32m1", "__builtin_rvv_vadd_vv_i32m1", -1 },
    { "__riscv_vsub_vv_i32m1", "__builtin_rvv_vsub_vv_i32m1", -1 },
    { "__riscv_vmul_vv_i32m1", "__builtin_rvv_vmul_vv_i32m1", -1 },
    { "__riscv_vle32_v_f32m1", "__builtin_rvv_vle32_v_f32m1", -1 },
    { "__riscv_vse32_v_f32m1", "__builtin_rvv_vse32_v_f32m1", -1 },
    { "__riscv_vfmacc_vv_f32m1", "__builtin_rvv_vfmacc_vv_f32m1", -1 },
    { "__riscv_vredsum_vs_i32m1_i32m1", "__builtin_rvv_vredsum_vs_i32m1_i32m1", -1 },
    { "__riscv_vsetvl_e32m1", "__builtin_rvv_vsetvl_e32m1", -1 },
    { "_mm_cmpeq_ps", "__builtin_ia32_cmpeqps", -1 },
    { "_mm_cmpgt_ps", "__builtin_ia32_cmpgtps", -1 },
    { "_mm_shuffle_epi8", "__builtin_ia32_pshufb128", -1 },
    { "_mm_blendv_ps", "__builtin_ia32_blendvps", -1 },
    { "_mm_unpacklo_ps", "__builtin_ia32_unpcklps", -1 },
    { "_mm_unpackhi_ps", "__builtin_ia32_unpckhps", -1 },
    { "_mm_load_ps", "__builtin_ia32_loadaps", -1 },
    { "_mm_loadu_ps", "__builtin_ia32_loadups", -1 },
    { "_mm_store_ps", "__builtin_ia32_storeaps", -1 },
    { "_mm_storeu_ps", "__builtin_ia32_storeups", -1 },
    { "_mm_loadu_si128", "__builtin_ia32_loaddqu", -1 },
    { "_mm_storeu_si128", "__builtin_ia32_storedqu", -1 },
    { "_mm_set1_ps", "__builtin_ia32_vec_init_v4sf", -1 },
    { "_mm_setzero_ps", "__builtin_ia32_xorps", -1 },
    { "_mm_hadd_ps", "__builtin_ia32_haddps", -1 },
    { "_mm_dp_ps", "__builtin_ia32_dpps", -1 },
    { "_mm_movemask_epi8", "__builtin_ia32_pmovmskb128", -1 },
    { "_mm_movemask_ps", "__builtin_ia32_movmskps", -1 },
    { "_mm256_permute2f128_ps", "__builtin_ia32_vperm2f128_ps256", -1 },
    { "_mm256_permute4x64_epi64", "__builtin_ia32_permdi256", -1 },
    { "_mm256_i32gather_epi32", "__builtin_ia32_gathersiv4si", -1 },
    { "_mm256_fmsub_ps", "__builtin_ia32_vfmsubps256", -1 },
    { "_mm256_fnmadd_ps", "__builtin_ia32_vfnmaddps256", -1 },
    { "_mm512_cmp_ps_mask", "__builtin_ia32_cmpps512_mask", -1 },
    { "_mm512_reduce_add_ps", "__builtin_ia32_reduce_add_v16sf", -1 },
    { "vld1q_f32", "__builtin_neon_vld1v4sf", -1 },
    { "vst1q_f32", "__builtin_neon_vst1v4sf", -1 },
    { "vld2q_f32", "__builtin_neon_vld2v4sf", -1 },
    { "vgetq_lane_s32", "__builtin_neon_vget_lanev4si", -1 },
    { "vsetq_lane_s32", "__builtin_neon_vset_lanev4si", -1 },
    { "vaddvq_f32", "__builtin_neon_vaddvv4sf", -1 },
    { "vmaxvq_f32", "__builtin_neon_vmaxvv4sf", -1 },
    { "vqtbl1q_u8", "__builtin_neon_vqtbl1v16qi", -1 },
    { NULL, NULL, -1 }
};

/** @brief Prefix table for ARM NEON intrinsics mapped to `__builtin_neon*` or `gcc_gen_be_simd.c`. */
static const char *const k_neon_prefixes[] = {
    "vadd", "vsub", "vmul", "vfma", "vcmp", "vbsl", "vext", "vrev",
    "vget_low", "vget_high", "vcombine", "vld", "vst", "vdup", "vreinterpret",
    "vpadd", "vdot", "vgetq_lane", "vsetq_lane", "vaddvq", "vmaxvq", "vqtbl",
    NULL
};

/**
 * @brief Lowers SIMD vector intrinsics for x86 (SSE/AVX/AVX-512) and ARM NEON.
 *
 * @details Translates x86 (`_mm_*`, `_mm256_*`, `_mm512_*`) and ARM NEON (`vadd*`, `vsub*`,
 * `vmul*`, `vld*`, `vst*`, `vdup*`, `vreinterpret*`) intrinsics via `simd_intrinsic_table` to
 * native vector operations or exact GCC target builtins, supporting arbitrary argument counts.
 *
 * @param[in] name The builtin name (e.g. "_mm_add_ps").
 * @param[in] rout The routine pointer.
 * @param[in] num_args Number of arguments.
 * @param[in] args Array of lowered argument rvalues.
 * @param[out] out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_simd_builtin(const char *name, a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    a_type_ptr rout_ret_tp = NULL;
    GCC_GEN_BE_CHECK(get_routine_sig_info(rout, &rout_ret_tp, NULL));

    gcc_jit_type *ret_type = NULL;
    if (rout_ret_tp) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(rout_ret_tp, &ret_type));
        if (!ret_type) ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    } else {
        ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    }

    const char *mapped_gcc_name = NULL;
    int mapped_bin_op = -1;
    for (size_t i = 0; simd_intrinsic_table[i].intrinsic_name != NULL; ++i) {
        if (strcmp(name, simd_intrinsic_table[i].intrinsic_name) == 0) {
            mapped_gcc_name = simd_intrinsic_table[i].gcc_builtin_name;
            mapped_bin_op = simd_intrinsic_table[i].binary_op;
            break;
        }
    }

    if (mapped_bin_op >= 0 && num_args == 2 && args[0] && args[1] &&
        rout_ret_tp && rout_ret_tp->kind == tk_vector &&
        gcc_jit_rvalue_get_type(args[0]) == ret_type &&
        gcc_jit_rvalue_get_type(args[1]) == ret_type) {
        *out_rval = gcc_jit_context_new_binary_op(
            ctx, NULL, (enum gcc_jit_binary_op)mapped_bin_op, ret_type, args[0], args[1]);
        return GCC_GEN_BE_SUCCESS;
    }

    a_boolean is_neon_pfx = FALSE;
    GCC_GEN_BE_CHECK(name_has_any_prefix(name, k_neon_prefixes, &is_neon_pfx));
    char gcc_name[128];
    if (mapped_gcc_name) {
        snprintf(gcc_name, sizeof(gcc_name), "%s", mapped_gcc_name);
    } else if (strncmp(name, "_mm_", 4) == 0) {
        snprintf(gcc_name, sizeof(gcc_name), "__builtin_ia32%s", name + 3);
    } else if (strncmp(name, "_mm256_", 7) == 0) {
        snprintf(gcc_name, sizeof(gcc_name), "__builtin_ia32_%s256", name + 7);
    } else if (strncmp(name, "_mm512_", 7) == 0) {
        snprintf(gcc_name, sizeof(gcc_name), "__builtin_ia32_%s512", name + 7);
    } else if (is_neon_pfx) {
        snprintf(gcc_name, sizeof(gcc_name), "__builtin_neon_%s", name);
    } else {
        snprintf(gcc_name, sizeof(gcc_name), "%s", name);
    }

    gcc_jit_type **arg_types = NULL;
    gcc_jit_rvalue **call_args = NULL;
    if (num_args > 0) {
        GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)num_args * sizeof(gcc_jit_type *), (void **)&arg_types));
        gcc_gen_be_error_t ca_err = gcc_gen_be_alloc((size_t)num_args * sizeof(gcc_jit_rvalue *), (void **)&call_args);
        if (ca_err != GCC_GEN_BE_SUCCESS) {
            free(arg_types);
            return ca_err;
        }
        gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        for (int i = 0; i < num_args; ++i) {
            call_args[i] = args[i] ? args[i] : gcc_jit_context_null(ctx, vp_t);
            arg_types[i] = gcc_jit_rvalue_get_type(call_args[i]);
        }
    }

    {
        a_boolean simd_handled = FALSE;
        gcc_gen_be_error_t simd_err = gcc_gen_be_try_lower_simd_builtin(
            gcc_name, rout, num_args, call_args, &simd_handled, out_rval);
        if (simd_err != GCC_GEN_BE_SUCCESS) {
            free(call_args);
            free(arg_types);
            return simd_err;
        }
        if (simd_handled) {
            free(call_args);
            free(arg_types);
            return GCC_GEN_BE_SUCCESS;
        }
    }

    gcc_jit_param **params = NULL;
    if (num_args > 0) {
        gcc_gen_be_error_t perr = gcc_gen_be_alloc((size_t)num_args * sizeof(gcc_jit_param *), (void **)&params);
        if (perr != GCC_GEN_BE_SUCCESS) {
            free(call_args);
            free(arg_types);
            return perr;
        }
        for (int i = 0; i < num_args; ++i) {
            char pname[32];
            snprintf(pname, sizeof(pname), "simd_p%d", i);
            params[i] = gcc_jit_context_new_param(ctx, NULL, arg_types[i], pname);
        }
    }
    gcc_jit_function *fn = (gcc_gen_be_step_oom_budget() == GCC_GEN_BE_SUCCESS)
        ? gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, gcc_name, num_args, params, 0)
        : NULL;
    free(params);
    free(arg_types);
    if (!fn) {
        free(call_args);
        return GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN;
    }

    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, num_args, call_args);
    free(call_args);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers sanitizer and profiling hooks.
 *
 * @details Emits calls to __asan_* and __ubsan_* instrumentation hooks.
 *
 * @param[in] name The builtin name.
 * @param[in] num_args Number of arguments.
 * @param[in] args Array of lowered argument rvalues.
 * @param[out] out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_sanitizer_hook(const char *name, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *vptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type **arg_types = NULL;
    gcc_jit_rvalue **call_args = NULL;
    if (num_args > 0) {
        GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)num_args * sizeof(gcc_jit_type *), (void **)&arg_types));
        gcc_gen_be_error_t ca_err = gcc_gen_be_alloc((size_t)num_args * sizeof(gcc_jit_rvalue *), (void **)&call_args);
        if (ca_err != GCC_GEN_BE_SUCCESS) {
            free(arg_types);
            return ca_err;
        }
        for (int i = 0; i < num_args; ++i) {
            call_args[i] = args[i] ? args[i] : gcc_jit_context_null(ctx, vptr_t);
            arg_types[i] = gcc_jit_rvalue_get_type(call_args[i]);
        }
    }

    gcc_jit_function *fn = NULL;
    gcc_gen_be_error_t err = get_builtin(ctx, name, void_type, num_args, arg_types, 0, &fn);
    free(arg_types);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(call_args);
        return err;
    }
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, num_args, call_args);
    free(call_args);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers C++20 `<coroutine>` GCC builtins (`__builtin_coro_*`).
 *
 * @details Implements `__builtin_coro_done`, `__builtin_coro_resume`,
 * `__builtin_coro_destroy`, `__builtin_coro_promise`, `__builtin_coro_size`,
 * `__builtin_coro_frame`, and `__builtin_coro_free` using the standard
 * coroutine frame header layout (`{ resume_fn, destroy_fn, promise_obj, state_index, ... }`).
 *
 * @param[in] name The coroutine builtin name.
 * @param[in] rout The routine pointer.
 * @param[in] num_args Number of arguments.
 * @param[in] args Array of lowered argument rvalues.
 * @param[out] out_rval Output JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t lower_coro_builtin(const char *name, a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
    gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);

    gcc_jit_rvalue *addr_arg = (num_args > 0 && args[0])
        ? gcc_jit_context_new_cast(ctx, NULL, args[0], void_ptr_type)
        : gcc_jit_context_null(ctx, void_ptr_type);

    if (strcmp(name, "__builtin_coro_resume") == 0 || strcmp(name, "__builtin_coro_destroy") == 0) {
        int slot_idx = (strcmp(name, "__builtin_coro_destroy") == 0) ? 1 : 0;
        gcc_jit_type *fn_ptr_type = gcc_jit_context_new_function_ptr_type(ctx, NULL, void_type, 1, &void_ptr_type, 0);
        gcc_jit_type *fn_ptr_ptr_type = gcc_jit_type_get_pointer(fn_ptr_type);
        gcc_jit_rvalue *fn_slot_base = gcc_jit_context_new_cast(ctx, NULL, addr_arg, fn_ptr_ptr_type);
        gcc_jit_rvalue *idx_rval = gcc_jit_context_new_rvalue_from_int(
            ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), slot_idx);
        gcc_jit_lvalue *fn_lval = gcc_jit_context_new_array_access(ctx, NULL, fn_slot_base, idx_rval);
        gcc_jit_rvalue *fn_ptr = gcc_jit_lvalue_as_rvalue(fn_lval);
        *out_rval = gcc_jit_context_new_call_through_ptr(ctx, NULL, fn_ptr, 1, &addr_arg);
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__builtin_coro_done") == 0) {
        gcc_jit_type *void_ptr_ptr_type = gcc_jit_type_get_pointer(void_ptr_type);
        gcc_jit_rvalue *resume_slot_ptr = gcc_jit_context_new_cast(ctx, NULL, addr_arg, void_ptr_ptr_type);
        gcc_jit_rvalue *resume_ptr_val = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(resume_slot_ptr, NULL));
        *out_rval = gcc_jit_context_new_comparison(
            ctx, NULL, GCC_JIT_COMPARISON_EQ, resume_ptr_val, gcc_jit_context_null(ctx, void_ptr_type));
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__builtin_coro_promise") == 0) {
        long hdr_offset = (long)(2 * sizeof(void *));
        gcc_jit_rvalue *byte_ptr = gcc_jit_context_new_cast(ctx, NULL, addr_arg, char_ptr_type);
        gcc_jit_rvalue *pos_off = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, hdr_offset);
        gcc_jit_rvalue *neg_off = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, -hdr_offset);
        gcc_jit_rvalue *to_prom = gcc_jit_context_new_cast(
            ctx, NULL,
            gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, byte_ptr, pos_off), NULL),
            void_ptr_type);
        gcc_jit_rvalue *from_prom = gcc_jit_context_new_cast(
            ctx, NULL,
            gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, byte_ptr, neg_off), NULL),
            void_ptr_type);
        if (num_args >= 3 && args[2]) {
            gcc_jit_rvalue *zero_arg = gcc_jit_context_zero(ctx, gcc_jit_rvalue_get_type(args[2]));
            gcc_jit_rvalue *is_from_prom = gcc_jit_context_new_comparison(
                ctx, NULL, GCC_JIT_COMPARISON_NE, args[2], zero_arg);
            gcc_jit_block *cblock = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
            if (cblock) {
                gcc_jit_function *func = gcc_jit_block_get_function(cblock);
                gcc_jit_lvalue *res_local = gcc_jit_function_new_local(func, NULL, void_ptr_type, "coro_prom_ptr");
                gcc_jit_block *then_blk = gcc_jit_function_new_block(func, "coro_from_prom");
                gcc_jit_block *else_blk = gcc_jit_function_new_block(func, "coro_to_prom");
                gcc_jit_block *merge_blk = gcc_jit_function_new_block(func, "coro_prom_merge");
                gcc_jit_block_end_with_conditional(cblock, NULL, is_from_prom, then_blk, else_blk);
                gcc_jit_block_add_assignment(then_blk, NULL, res_local, from_prom);
                gcc_jit_block_end_with_jump(then_blk, NULL, merge_blk);
                gcc_jit_block_add_assignment(else_blk, NULL, res_local, to_prom);
                gcc_jit_block_end_with_jump(else_blk, NULL, merge_blk);
                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_blk));
                *out_rval = gcc_jit_lvalue_as_rvalue(res_local);
                return GCC_GEN_BE_SUCCESS;
            }
        }
        *out_rval = to_prom;
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__builtin_coro_noop") == 0) {
        void *cached_noop = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)"__edg_builtin_noop_coro", &cached_noop));
        if (cached_noop) {
            *out_rval = gcc_jit_context_new_cast(
                ctx, NULL, gcc_jit_lvalue_get_address((gcc_jit_lvalue *)cached_noop, NULL), void_ptr_type);
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_param *np = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "frame");
        gcc_jit_function *noop_fn = gcc_jit_context_new_function(
            ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "__edg_builtin_noop_coro_fn", 1, &np, 0);
        gcc_jit_block *nblk = gcc_jit_function_new_block(noop_fn, "entry");
        gcc_jit_block_end_with_void_return(nblk, NULL);
        gcc_jit_type *arr_t = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, 2);
        gcc_jit_lvalue *noop_glob = gcc_jit_context_new_global(
            ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, arr_t, "__edg_builtin_noop_coro");
        gcc_jit_rvalue *fn_ptr = gcc_jit_context_new_cast(
            ctx, NULL, gcc_jit_function_get_address(noop_fn, NULL), void_ptr_type);
        gcc_jit_rvalue *elems[2] = { fn_ptr, fn_ptr };
        gcc_jit_global_set_initializer_rvalue(
            noop_glob, gcc_jit_context_new_array_constructor(ctx, NULL, arr_t, 2, elems));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"__edg_builtin_noop_coro", noop_glob));
        *out_rval = gcc_jit_context_new_cast(
            ctx, NULL, gcc_jit_lvalue_get_address(noop_glob, NULL), void_ptr_type);
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__builtin_coro_size") == 0) {
        gcc_jit_type *active_frame_type = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_active_coroutine_frame(&active_frame_type, NULL));
        if (active_frame_type) {
            ssize_t fsz = 0;
            GCC_GEN_BE_CHECK(safe_jit_scalar_size(ctx, active_frame_type, &fsz));
            if (fsz > 0) {
                *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)fsz);
            } else {
                *out_rval = gcc_jit_context_new_cast(ctx, NULL, p_gcc_jit_context_new_sizeof(ctx, active_frame_type), size_t_type);
            }
        } else {
            *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, 64);
        }
        return GCC_GEN_BE_SUCCESS;
    }

    if (strcmp(name, "__builtin_coro_frame") == 0 || strcmp(name, "__builtin_coro_free") == 0) {
        *out_rval = addr_arg;
        return GCC_GEN_BE_SUCCESS;
    }

    return lower_math_bitwise_builtin(name, rout, num_args, args, out_rval);
}

/**
 * @brief High-level dispatcher for lowering GCC builtins and intrinsics.
 *
 * @details Dispatches atomic, math/bitwise/overflow, SIMD, coroutine, and sanitizer builtins.
 *
 * @param[in] rout The routine entry of the builtin.
 * @param[in] num_args The number of arguments in the call.
 * @param[in] args Array of JIT rvalues for the arguments.
 * @param[out] out_rval Pointer to receive the generated JIT rvalue for the result.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_builtin_call(a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !out_rval || num_args < 0 || (num_args > 0 && !args)) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    const char *name = rout->source_corresp.name;
    if (!name && rout->special_kind == sfk_none && rout->variant.builtin_function_kind != bfk_none) {
        name = "__builtin_generic";
    }
    if (!name) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    if (strncmp(name, "__c11_atomic_", 13) == 0) {
        const char *c11_op = name + 13;
        gcc_jit_context *ctx = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
        if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;
        if (strcmp(c11_op, "init") == 0 && num_args >= 2 && args[0] && args[1]) {
            gcc_jit_block *cblk = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblk));
            if (cblk) {
                gcc_jit_type *vt = gcc_jit_rvalue_get_type(args[1]);
                gcc_jit_rvalue *dp = gcc_jit_context_new_cast(ctx, NULL, args[0], gcc_jit_type_get_pointer(vt));
                gcc_jit_block_add_assignment(cblk, NULL, gcc_jit_rvalue_dereference(dp, NULL), args[1]);
            }
            *out_rval = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
            return GCC_GEN_BE_SUCCESS;
        }
        if ((strcmp(c11_op, "compare_exchange_strong") == 0 ||
             strcmp(c11_op, "compare_exchange_weak") == 0) && num_args == 5) {
            gcc_jit_type *bool_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
            int is_weak = (strcmp(c11_op, "compare_exchange_weak") == 0) ? 1 : 0;
            gcc_jit_rvalue *cas_args[6] = {
                args[0], args[1], args[2],
                gcc_jit_context_new_rvalue_from_int(ctx, bool_t, is_weak),
                args[3], args[4]
            };
            return gcc_gen_be_lower_generic_atomic_call(
                "__atomic_compare_exchange_n", rout, 6, cas_args, 0UL, out_rval);
        }
        char mapped_atomic[64];
        if (strcmp(c11_op, "load") == 0 || strcmp(c11_op, "store") == 0 || strcmp(c11_op, "exchange") == 0) {
            snprintf(mapped_atomic, sizeof(mapped_atomic), "__atomic_%s_n", c11_op);
        } else {
            snprintf(mapped_atomic, sizeof(mapped_atomic), "__atomic_%s", c11_op);
        }
        return lower_atomic_builtin(mapped_atomic, rout, num_args, args, out_rval);
    }

    static const char *const k_simd_dispatch_prefixes[] = {
        "_mm_", "_mm256_", "_mm512_", "sv", "__riscv_v", NULL
    };
    a_boolean is_simd_pfx = FALSE;
    a_boolean is_neon_pfx = FALSE;
    GCC_GEN_BE_CHECK(name_has_any_prefix(name, k_simd_dispatch_prefixes, &is_simd_pfx));
    GCC_GEN_BE_CHECK(name_has_any_prefix(name, k_neon_prefixes, &is_neon_pfx));

    if (strncmp(name, "__atomic_", 9) == 0 || strncmp(name, "__sync_", 7) == 0) {
        return lower_atomic_builtin(name, rout, num_args, args, out_rval);
    } else if (strncmp(name, "__builtin_coro_", 15) == 0) {
        return lower_coro_builtin(name, rout, num_args, args, out_rval);
    } else if (is_simd_pfx || is_neon_pfx) {
        return lower_simd_builtin(name, rout, num_args, args, out_rval);
    } else if (strncmp(name, "__asan_", 7) == 0 || strncmp(name, "__ubsan_", 8) == 0) {
        return lower_sanitizer_hook(name, num_args, args, out_rval);
    }

    /* Fallback */
    return lower_math_bitwise_builtin(name, rout, num_args, args, out_rval);
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
