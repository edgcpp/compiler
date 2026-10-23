/**
 * @file gcc_gen_be_dtor.c
 * @brief Implementation of global constructors and destructors.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_dtor.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_lib_loader.h"

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

gcc_gen_be_error_t gcc_gen_be_get_dso_handle(gcc_jit_context *ctx, gcc_jit_rvalue **out_dso_handle) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !out_dso_handle) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_dso_handle = NULL;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(void_type);

    void *cached_dso = NULL;
    if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__dso_handle", &cached_dso) == GCC_GEN_BE_SUCCESS && cached_dso) {
        *out_dso_handle = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address((gcc_jit_lvalue *)cached_dso, NULL), void_ptr_type);
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_lvalue *dso_handle = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_IMPORTED, void_ptr_type, "__dso_handle");
    if (!dso_handle) return GCC_GEN_BE_ERROR_DSO_HANDLE_FAILED;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__dso_handle", dso_handle));
    *out_dso_handle = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(dso_handle, NULL), void_ptr_type);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_cxa_atexit(gcc_jit_lvalue *var, gcc_jit_function *dtor_func) GCC_GEN_BE_NOEXCEPT {
    if (!var || !dtor_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));

    /* Declare __cxa_atexit if not present */
    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(void_type);
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    
    /* Function signature: int __cxa_atexit(void (*f)(void *), void *objptr, void *dso) */
    gcc_jit_param *p1 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "f");
    gcc_jit_param *p2 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "objptr");
    gcc_jit_param *p3 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "dso");
    gcc_jit_param *params[] = {p1, p2, p3};
    gcc_jit_function *atexit_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "__cxa_atexit", 3, params, 0);

    gcc_jit_rvalue *dso_handle_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_dso_handle(ctx, &dso_handle_rval));
    
    gcc_jit_rvalue *args[3];
    args[0] = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(dtor_func, NULL), void_ptr_type);
    args[1] = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(var, NULL), void_ptr_type);
    args[2] = dso_handle_rval;

    gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, atexit_fn, 3, args));

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_cxa_thread_atexit(gcc_jit_lvalue *var, gcc_jit_function *dtor_func) GCC_GEN_BE_NOEXCEPT {
    if (!var || !dtor_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));

    /* Declare __cxa_thread_atexit if not present */
    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(void_type);
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    
    /* Function signature: int __cxa_thread_atexit(void (*f)(void *), void *objptr, void *dso) */
    gcc_jit_param *p1 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "f");
    gcc_jit_param *p2 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "objptr");
    gcc_jit_param *p3 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "dso");
    gcc_jit_param *params[] = {p1, p2, p3};
    gcc_jit_function *atexit_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "__cxa_thread_atexit", 3, params, 0);

    gcc_jit_rvalue *dso_handle_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_dso_handle(ctx, &dso_handle_rval));
    
    gcc_jit_rvalue *args[3];
    args[0] = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(dtor_func, NULL), void_ptr_type);
    args[1] = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(var, NULL), void_ptr_type);
    args[2] = dso_handle_rval;

    gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, atexit_fn, 3, args));

    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
