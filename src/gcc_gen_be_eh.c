/**
 * @file gcc_gen_be_eh.c
 * @brief Implementation of Exception handling and stack unwinding.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_stmt.h"


#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/** @brief Context-backed per-function exception handling state (`eh_state`). */
#define eh_state (gcc_gen_be_singleton_state.eh_state)
/** @brief Context-backed global function counter for landing pad uniqueness. */
#define global_func_counter (gcc_gen_be_singleton_state.global_func_counter)

/**
 * @brief Appends a single byte to the dynamic LSDA action table.
 * @details Grows `eh_state.action_table` dynamically if needed.
 * @param[in] byte_val The byte value to append.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_OOM on allocation failure.
 */
static gcc_gen_be_error_t append_action_byte(unsigned char byte_val) GCC_GEN_BE_NOEXCEPT {
    if (eh_state.action_table_len >= eh_state.action_table_cap) {
        int new_cap = (eh_state.action_table_cap == 0) ? 256 : (eh_state.action_table_cap * 2);
        char *new_buf = (char *)realloc(eh_state.action_table, (size_t)new_cap);
        if (!new_buf) return GCC_GEN_BE_ERROR_OOM;
        eh_state.action_table = new_buf;
        eh_state.action_table_cap = new_cap;
    }
    eh_state.action_table[eh_state.action_table_len++] = (char)byte_val;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Encodes an unsigned integer value into DWARF ULEB128 byte format.
 * @details Writes the variable-length ULEB128 representation of `value` into `out_buf`
 * (up to `buf_cap` bytes) and writes the number of bytes written to `out_len`.
 * @param[in] value The unsigned 64-bit value to encode.
 * @param[out] out_buf Destination buffer for encoded bytes.
 * @param[in] buf_cap Capacity of `out_buf` in bytes.
 * @param[out] out_len Pointer to receive the number of encoded bytes written.
 * @return GCC_GEN_BE_SUCCESS on success, `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if pointers are NULL,
 * or `GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED` if `buf_cap` is insufficient.
 */
gcc_gen_be_error_t gcc_gen_be_encode_uleb128(
    unsigned long value,
    unsigned char *out_buf,
    size_t buf_cap,
    size_t *out_len) GCC_GEN_BE_NOEXCEPT {
    if (!out_buf || !out_len) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_len = 0;
    size_t idx = 0;
    do {
        if (idx >= buf_cap) return GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED;
        unsigned char byte_val = (unsigned char)(value & 0x7fUL);
        value >>= 7;
        if (value != 0UL) {
            byte_val |= 0x80U;
        }
        out_buf[idx++] = byte_val;
    } while (value != 0UL);
    *out_len = idx;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Encodes a signed integer value into DWARF SLEB128 byte format.
 * @details Writes the variable-length SLEB128 representation of `value` into `out_buf`
 * (up to `buf_cap` bytes) and writes the number of bytes written to `out_len`.
 * @param[in] value The signed value to encode.
 * @param[out] out_buf Destination buffer for encoded bytes.
 * @param[in] buf_cap Capacity of `out_buf` in bytes.
 * @param[out] out_len Pointer to receive the number of encoded bytes written.
 * @return GCC_GEN_BE_SUCCESS on success, `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if pointers are NULL,
 * or `GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED` if `buf_cap` is insufficient.
 */
gcc_gen_be_error_t gcc_gen_be_encode_sleb128(
    long value,
    unsigned char *out_buf,
    size_t buf_cap,
    size_t *out_len) GCC_GEN_BE_NOEXCEPT {
    if (!out_buf || !out_len) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_len = 0;
    size_t idx = 0;
    a_boolean more = TRUE;
    while (more) {
        if (idx >= buf_cap) return GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED;
        unsigned char byte_val = (unsigned char)(value & 0x7f);
        value >>= 7;
        if ((value == 0 && (byte_val & 0x40) == 0) ||
            (value == -1 && (byte_val & 0x40) != 0)) {
            more = FALSE;
        } else {
            byte_val |= 0x80;
        }
        out_buf[idx++] = byte_val;
    }
    *out_len = idx;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Appends a signed integer encoded in DWARF SLEB128 format to the action table.
 * @details Encodes arbitrary multi-byte signed values into `eh_state.action_table`.
 * @param[in] value The signed integer value to encode.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t append_sleb128(long value) GCC_GEN_BE_NOEXCEPT {
    unsigned char encoded[16];
    size_t encoded_len = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_encode_sleb128(value, encoded, sizeof(encoded), &encoded_len));
    for (size_t i = 0; i < encoded_len; ++i) {
        GCC_GEN_BE_CHECK(append_action_byte(encoded[i]));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief reset_eh_state function.
 * @details Resets the internal EH state for a new function.
 * @param[in] func The JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
static gcc_gen_be_error_t reset_eh_state(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    eh_state.current_func = func;
    eh_state.current_func_id = ++global_func_counter;
    eh_state.next_try_id = 1;
    eh_state.next_action_id = 1;
    eh_state.next_type_id = 1;
    eh_state.lp_start_emitted = FALSE;
    eh_state.active_try_depth = 0;
    
    while (eh_state.regions) {
        gcc_gen_be_eh_region_t *r = eh_state.regions;
        eh_state.regions = r->next;
        free(r);
    }
    while (eh_state.types) {
        gcc_gen_be_eh_type_filter_t *t = eh_state.types;
        eh_state.types = t->next;
        free(t);
    }
    eh_state.action_table_len = 0;

    gcc_gen_be_context_t *state = NULL;
    if (gcc_gen_be_get_state(&state) == GCC_GEN_BE_SUCCESS && state) {
        state->eh_state_ptr = &eh_state;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief get_type_filter_index function.
 * @details Retrieves or creates a type filter index for the action table.
 * @param[in] type The EH filter type.
 * @param[out] out_index The returned filter index.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t get_type_filter_index(a_type_ptr type, int *out_index) GCC_GEN_BE_NOEXCEPT {
    if (!out_index) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (!type) {
        *out_index = 0; /* catch-all */
        return GCC_GEN_BE_SUCCESS;
    }
    
    for (gcc_gen_be_eh_type_filter_t *t = eh_state.types; t; t = t->next) {
        if (t->type == type) {
            *out_index = t->type_id;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    
    gcc_gen_be_eh_type_filter_t *t = (gcc_gen_be_eh_type_filter_t *)malloc(sizeof(gcc_gen_be_eh_type_filter_t));
    if (!t) return GCC_GEN_BE_ERROR_OOM;
    t->type = type;
    t->type_id = eh_state.next_type_id++;
    t->next = eh_state.types;
    eh_state.types = t;
    *out_index = t->type_id;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief emit_eh_tables function.
 * @details Emits the LSDA exception handling tables.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t emit_eh_tables(void) GCC_GEN_BE_NOEXCEPT {
    if (!eh_state.current_func || !eh_state.regions) return GCC_GEN_BE_SUCCESS;

    /* Emit the type descriptors for all registered catch filter types */
    for (gcc_gen_be_eh_type_filter_t *t = eh_state.types; t != NULL; t = t->next) {
        if (t->type != NULL) {
            gcc_jit_lvalue *tinfo = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(t->type, &tinfo));
        }
    }

    gcc_jit_lvalue *lsda_global = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_emit_lsda_call_site_table(NULL, &lsda_global);
    return err;
}

/**
 * @brief Declares an exception handling runtime function.
 *
 * @details Adds the function declaration to the given GCC JIT context.
 *
 * @param[in] ctx The libgccjit context.
 * @param[in] name The name of the function.
 * @param[in] ret_type The return type.
 * @param[in] num_params The number of parameters.
 * @param[in] param_types The array of parameter types.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t declare_eh_func(gcc_jit_context *ctx, const char *name, gcc_jit_type *ret_type, int num_params, gcc_jit_type **param_types) GCC_GEN_BE_NOEXCEPT {
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)name, &cached);
    if (err == GCC_GEN_BE_SUCCESS && cached) return GCC_GEN_BE_SUCCESS;

    gcc_jit_param **params = NULL;
    if (num_params > 0) {
        params = (gcc_jit_param **)malloc((size_t)num_params * sizeof(gcc_jit_param *));
        if (!params) return GCC_GEN_BE_ERROR_OOM;
        for (int i = 0; i < num_params; ++i) {
            params[i] = gcc_jit_context_new_param(ctx, NULL, param_types[i], "p");
        }
    }

    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_type, name, num_params, params, 0);
    if (params) free(params);

    return cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)name, fn);
}

gcc_gen_be_error_t gcc_gen_be_begin_function_eh(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (eh_state.current_func != NULL && eh_state.current_func != func) {
        gcc_gen_be_eh_state_t *saved = (gcc_gen_be_eh_state_t *)malloc(sizeof(gcc_gen_be_eh_state_t));
        if (!saved) return GCC_GEN_BE_ERROR_OOM;
        *saved = eh_state;
        memset(&eh_state, 0, sizeof(eh_state));
        eh_state.prev = saved;
    }
    GCC_GEN_BE_CHECK(reset_eh_state(func));
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_end_function_eh(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (eh_state.current_func == func) {
        GCC_GEN_BE_CHECK(emit_eh_tables());
        while (eh_state.regions) {
            gcc_gen_be_eh_region_t *r = eh_state.regions;
            eh_state.regions = r->next;
            free(r);
        }
        while (eh_state.types) {
            gcc_gen_be_eh_type_filter_t *t = eh_state.types;
            eh_state.types = t->next;
            free(t);
        }
        if (eh_state.action_table) {
            free(eh_state.action_table);
            eh_state.action_table = NULL;
            eh_state.action_table_len = 0;
            eh_state.action_table_cap = 0;
        }
        eh_state.current_func = NULL;
        if (eh_state.prev) {
            gcc_gen_be_eh_state_t *prev = eh_state.prev;
            eh_state = *prev;
            free(prev);
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_declare_eh_symbols(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(void_type);
    gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);

    /* Personality routines for C++ DWARF, C DWARF, Windows SEH, SjLj, and ARM EHABI */
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *uint64_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
    gcc_jit_type *personality_params[] = { int_type, int_type, uint64_type, void_ptr_type, void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gxx_personality_v0", int_type, 5, personality_params));
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gcc_personality_v0", int_type, 5, personality_params));
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gxx_personality_seh0", int_type, 5, personality_params));
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gxx_personality_sj0", int_type, 5, personality_params));
    gcc_jit_type *arm_pers_params[] = { int_type, void_ptr_type, void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__aeabi_unwind_cpp_pr0", int_type, 3, arm_pers_params));
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__aeabi_unwind_cpp_pr1", int_type, 3, arm_pers_params));
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_end_cleanup", void_type, 0, NULL));

    /* void *__cxa_allocate_exception(size_t thrown_size) */
    gcc_jit_type *p1[] = { size_t_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_allocate_exception", void_ptr_type, 1, p1));

    /* void __cxa_free_exception(void *thrown_exception) */
    gcc_jit_type *p_free[] = { void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_free_exception", void_type, 1, p_free));

    /* void __cxa_throw(void *thrown_exception, struct std::type_info *tinfo, void (*dest)(void *)) */
    gcc_jit_type *p2[] = { void_ptr_type, void_ptr_type, void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_throw", void_type, 3, p2));

    /* void *__cxa_begin_catch(void *exceptionObject) */
    gcc_jit_type *p3[] = { void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_begin_catch", void_ptr_type, 1, p3));

    /* void __cxa_end_catch() */
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_end_catch", void_type, 0, NULL));

    
    /* void _Unwind_Resume(struct _Unwind_Exception *) */
    gcc_jit_type *p4[] = { void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "_Unwind_Resume", void_type, 1, p4));

    /* void __cxa_call_unexpected(void *) */
    gcc_jit_type *p5[] = { void_ptr_type };
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_call_unexpected", void_type, 1, p5));

    /* void std::terminate() */
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "_ZSt9terminatev", void_type, 0, NULL));

    /* void __cxa_rethrow() */
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_rethrow", void_type, 0, NULL));

    /* Runtime frame registration & unwind state helpers */
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gcc_be_eh_register_frame", void_type, 1, &void_ptr_type));
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gcc_be_eh_unregister_frame", void_type, 1, &void_ptr_type));
    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gcc_be_eh_check_unwind_state", int_type, 0, NULL));

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_throw(an_expr_node_ptr expr, gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!cblock) {
        if (!eh_state.current_func) return GCC_GEN_BE_ERROR_INVALID_STATE;
        cblock = gcc_jit_function_new_block(eh_state.current_func, "unreachable_throw");
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cblock));
    }

    a_throw_supplement_ptr tsp = expr->variant.throw_info;
    if (tsp) {
        /* Throw of an object. */
        a_type_ptr throw_type = tsp->type;
        long sz = (long)size_of_type(throw_type);

        gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);

        gcc_jit_param *alloc_param = gcc_jit_context_new_param(ctx, NULL, size_t_type, "size");
        gcc_jit_function *alloc_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__cxa_allocate_exception", 1, &alloc_param, 0);

        gcc_jit_rvalue *size_rval = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, sz);
        gcc_jit_rvalue *exc_buf_call = gcc_jit_context_new_call(ctx, NULL, alloc_fn, 1, &size_rval);
        gcc_jit_function *cur_func = gcc_jit_block_get_function(cblock);
        gcc_jit_lvalue *exc_buf_local = gcc_jit_function_new_local(cur_func, NULL, void_ptr_type, "exc_buf");
        gcc_jit_block_add_assignment(cblock, NULL, exc_buf_local, exc_buf_call);
        gcc_jit_rvalue *exc_buf = gcc_jit_lvalue_as_rvalue(exc_buf_local);

        /* Emit initialization of thrown value into allocated exception buffer, with __cxa_free_exception cleanup on failure. */
        void *saved_thrown_addr = NULL;
        if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_thrown_obj_addr", &saved_thrown_addr) != GCC_GEN_BE_SUCCESS) {
            saved_thrown_addr = NULL;
        }
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_thrown_obj_addr", exc_buf_local));
        a_dynamic_init_ptr dip = tsp->dynamic_init;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
        if (tsp->expr) {
            gcc_jit_block *free_exc_lp = gcc_jit_function_new_block(cur_func, "throw_init_free_exc_lp");
            GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(free_exc_lp));
            gcc_jit_rvalue *init_expr_rval = NULL;
            gcc_gen_be_error_t init_err = gcc_gen_be_lower_expr_rvalue(tsp->expr, &init_expr_rval);
            GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
            if (init_err != GCC_GEN_BE_SUCCESS) return init_err;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
            if (init_expr_rval && cblock) {
                gcc_jit_block_add_eval(cblock, NULL, init_expr_rval);
            }
            if (cblock) {
                gcc_jit_block *after_exc_init = gcc_jit_function_new_block(cur_func, "after_throw_exc_init");
                gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, zero, zero);
                gcc_jit_block_end_with_conditional(cblock, NULL, cond, after_exc_init, free_exc_lp);

                void *cached_free_exc = NULL;
                GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_free_exception", &cached_free_exc));
                if (!cached_free_exc) {
                    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_free_exception", void_type, 1, &void_ptr_type));
                    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_free_exception", &cached_free_exc));
                }
                if (cached_free_exc) {
                    gcc_jit_block_add_eval(
                        free_exc_lp, NULL,
                        gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_free_exc, 1, &exc_buf));
                }
                gcc_jit_block *outer_lp = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&outer_lp));
                if (outer_lp) {
                    gcc_jit_block_end_with_jump(free_exc_lp, NULL, outer_lp);
                } else {
                    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(free_exc_lp));
                    gcc_jit_block *after_free_res = NULL;
                    GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(gcc_jit_context_null(ctx, void_ptr_type), &after_free_res));
                    if (after_free_res) {
                        gcc_jit_block_end_with_jump(after_free_res, NULL, after_exc_init);
                    }
                }
                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(after_exc_init));
                cblock = after_exc_init;
            }
        } else
#endif
        if (dip) {
            gcc_jit_type *exc_type = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(throw_type, &exc_type));
            gcc_jit_lvalue *exc_deref = gcc_jit_rvalue_dereference(
                gcc_jit_context_new_cast(ctx, NULL, exc_buf, gcc_jit_type_get_pointer(exc_type)), NULL);

            gcc_jit_block *free_exc_lp = NULL;
            if (dip->kind == dik_constructor || dip->kind == dik_class_result_via_ctor) {
                free_exc_lp = gcc_jit_function_new_block(cur_func, "throw_init_free_exc_lp");
                GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(free_exc_lp));
            }

            gcc_gen_be_error_t init_err = GCC_GEN_BE_SUCCESS;
            a_routine_ptr saved_dip_dtor = dip->destructor;
            dip->destructor = NULL;
            if (dip->kind == dik_expression) {
                gcc_jit_rvalue *val_rval = NULL;
                init_err = gcc_gen_be_lower_expr_rvalue(dip->variant.expression, &val_rval);
                if (init_err == GCC_GEN_BE_SUCCESS && val_rval) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                    if (cblock) {
                        gcc_jit_block_add_assignment(cblock, NULL, exc_deref, val_rval);
                    }
                }
            } else {
                gcc_jit_rvalue *init_rval = NULL;
                init_err = gcc_gen_be_lower_dynamic_init(dip, exc_deref, &init_rval);
                if (init_err == GCC_GEN_BE_SUCCESS) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
                }
            }
            dip->destructor = saved_dip_dtor;

            if (free_exc_lp) {
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
            }
            if (init_err != GCC_GEN_BE_SUCCESS) {
                return init_err;
            }

            if (free_exc_lp && cblock) {
                gcc_jit_block *after_exc_init = gcc_jit_function_new_block(cur_func, "after_throw_exc_init");
                gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, zero, zero);
                gcc_jit_block_end_with_conditional(cblock, NULL, cond, after_exc_init, free_exc_lp);

                void *cached_free_exc = NULL;
                GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_free_exception", &cached_free_exc));
                if (!cached_free_exc) {
                    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                    GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_free_exception", void_type, 1, &void_ptr_type));
                    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_free_exception", &cached_free_exc));
                }
                if (cached_free_exc) {
                    gcc_jit_block_add_eval(
                        free_exc_lp, NULL,
                        gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_free_exc, 1, &exc_buf));
                }
                gcc_jit_block *outer_lp = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&outer_lp));
                if (outer_lp) {
                    gcc_jit_block_end_with_jump(free_exc_lp, NULL, outer_lp);
                } else {
                    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(free_exc_lp));
                    gcc_jit_block *after_free_res = NULL;
                    GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(gcc_jit_context_null(ctx, void_ptr_type), &after_free_res));
                    if (after_free_res) {
                        gcc_jit_block_end_with_jump(after_free_res, NULL, after_exc_init);
                    }
                }
                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(after_exc_init));
                cblock = after_exc_init;
            }
        }
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_thrown_obj_addr", saved_thrown_addr));

        gcc_jit_param *throw_params[3];
        throw_params[0] = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "exc");
        throw_params[1] = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "tinfo");
        throw_params[2] = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "dest");
        gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_function *throw_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_throw", 3, throw_params, 0);

        gcc_jit_rvalue *args[3];
        args[0] = exc_buf;

        /* Phase 3: typeinfo_ptr */
        gcc_jit_lvalue *type_info_lval = NULL;
        err = gcc_gen_be_emit_type_descriptor(throw_type, &type_info_lval);
        if (err != GCC_GEN_BE_SUCCESS) return err;
        gcc_jit_rvalue *type_info_addr = gcc_jit_lvalue_get_address(type_info_lval, NULL);
        args[1] = gcc_jit_context_new_cast(ctx, NULL, type_info_addr, void_ptr_type);

        /* Phase 3: destructor_ptr */
        gcc_jit_rvalue *dtor_rval = gcc_jit_context_null(ctx, void_ptr_type);
        a_routine_ptr throw_dtor = tsp->destructor ? tsp->destructor : (dip ? dip->destructor : NULL);
        if (throw_dtor) {
            void *cached = NULL;
            err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, throw_dtor, &cached);
            gcc_jit_function *dtor_fn = (err == GCC_GEN_BE_SUCCESS && cached) ? (gcc_jit_function *)cached : NULL;
            if (!dtor_fn) {
                extern gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(throw_dtor, &dtor_fn));
            }
            if (dtor_fn) {
                dtor_rval = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(dtor_fn, NULL), void_ptr_type);
            }
        }
        args[2] = dtor_rval;
        if (eh_state.active_try_depth == 0) {
            GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(NULL, cblock));
        }

        gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, throw_fn, 3, args));
    } else {
        /* Rethrow */
        void *cached_rethrow = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_rethrow", &cached_rethrow));
        gcc_jit_function *rethrow_fn = (gcc_jit_function *)cached_rethrow;
        if (!rethrow_fn) {
            gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
            rethrow_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_rethrow", 0, NULL, 0);
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_rethrow", rethrow_fn));
        }
        gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, rethrow_fn, 0, NULL));
    }

    /* Transition control to active enclosing landing pad if caught locally, otherwise to unreachable_after_throw */
    gcc_jit_block *active_lp = NULL;
    gcc_gen_be_error_t lp_err = gcc_gen_be_get_active_landing_pad(&active_lp);
    gcc_jit_block *unreachable_block = gcc_jit_function_new_block(gcc_jit_block_get_function(cblock), "unreachable_after_throw");
    if (lp_err == GCC_GEN_BE_SUCCESS && active_lp) {
        gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
        gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, zero, zero);
        gcc_jit_block_end_with_conditional(cblock, NULL, cond, active_lp, unreachable_block);
    } else {
        gcc_jit_block_end_with_jump(cblock, NULL, unreachable_block);
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(unreachable_block));
    
    *out_block = unreachable_block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves or creates the `__eh_frame` struct type for SjLj exception handling.
 * @details Looks up or constructs the 4-field `__eh_frame` structure (`next`, `jmpbuf`, `exception_object`, `type_info`).
 * @param[in] ctx The libgccjit context.
 * @param[out] out_type Pointer to receive the `__eh_frame` struct type.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t get_eh_frame_type(gcc_jit_context *ctx, gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_TYPE, (void *)"__eh_frame", &cached);
    if (err == GCC_GEN_BE_SUCCESS && cached) {
        *out_type = (gcc_jit_type *)cached;
        return GCC_GEN_BE_SUCCESS;
    }
    
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    
    gcc_jit_type *array_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, 64);
    
    gcc_jit_field *f_next = gcc_jit_context_new_field(ctx, NULL, void_ptr_type, "next");
    gcc_jit_field *f_jmpbuf = gcc_jit_context_new_field(ctx, NULL, array_type, "jmpbuf");
    gcc_jit_field *f_exc = gcc_jit_context_new_field(ctx, NULL, void_ptr_type, "exception_object");
    gcc_jit_field *f_tinfo = gcc_jit_context_new_field(ctx, NULL, void_ptr_type, "type_info");
    
    gcc_jit_field *fields[] = { f_next, f_jmpbuf, f_exc, f_tinfo };
    gcc_jit_struct *eh_struct = gcc_jit_context_new_struct_type(ctx, NULL, "__eh_frame", 4, fields);
    
    gcc_jit_type *eh_type = gcc_jit_struct_as_type(eh_struct);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, (void *)"__eh_frame", eh_type));
    *out_type = eh_type;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves or creates the thread-local `__gcc_be_eh_chain` global variable.
 * @details Looks up or creates the global pointer tracking the active SjLj exception frame chain.
 * @param[in] ctx The libgccjit context.
 * @param[out] out_chain Pointer to receive the `__gcc_be_eh_chain` lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t get_eh_chain(gcc_jit_context *ctx, gcc_jit_lvalue **out_chain) GCC_GEN_BE_NOEXCEPT {
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_eh_chain", &cached);
    if (err == GCC_GEN_BE_SUCCESS && cached) {
        *out_chain = (gcc_jit_lvalue *)cached;
        return GCC_GEN_BE_SUCCESS;
    }
    
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_lvalue *chain = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_EXPORTED, void_ptr_type, "__gcc_be_eh_chain");
    
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__gcc_be_eh_chain", chain));
    *out_chain = chain;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Declares or looks up the `__cxa_type_match` runtime helper function.
 * @details Creates an imported `gcc_jit_function` for `__cxa_type_match(void*, void*, void*)` if not cached.
 * @param[in] ctx The libgccjit context.
 * @param[out] out_func Pointer to receive the `__cxa_type_match` function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t declare_cxa_type_match(gcc_jit_context *ctx, gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_type_match", &cached);
    if (err == GCC_GEN_BE_SUCCESS && cached) {
        *out_func = (gcc_jit_function *)cached;
        return GCC_GEN_BE_SUCCESS;
    }
    
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_param *params[] = {
        gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "catch_type"),
        gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "throw_type"),
        gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "exc_ptr")
    };
    
    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__cxa_type_match", 3, params, 0);
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_type_match", fn));
    *out_func = fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Checks if an EDG type is a C++ reference type.
 * @details Strips typedefs and checks if the underlying type kind is tk_pointer with is_reference set.
 * @param[in] tp The EDG type pointer to inspect.
 * @param[out] out_is_ref Pointer to receive TRUE if reference type, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if out_is_ref is NULL.
 */
static gcc_gen_be_error_t check_is_reference_type(a_type_ptr tp, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    if (!out_is_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    while (tp && tp->kind == tk_typeref) {
        tp = tp->variant.typeref.type;
    }
    *out_is_ref = (tp && tp->kind == tk_pointer && tp->variant.pointer.is_reference) ? TRUE : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Captures unwinder exception pointer and typeinfo/selector at landing pad entry.
 *
 * @details In DWARF zero-cost mode, queries `__builtin_eh_pointer(0)` and `__builtin_eh_filter(0)`.
 * In SjLj fallback mode (`GCC_GEN_BE_USE_SJLJ_EXCEPTIONS`), extracts `exception_object` and
 * `type_info` from the active `__eh_frame`.
 *
 * @param[in] ctx The libgccjit context.
 * @param[in] block The landing pad basic block.
 * @param[in] frame_exc The local variable to receive the exception pointer.
 * @param[in] frame_tinfo The local variable to receive the selector/typeinfo pointer.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t capture_landing_pad_registers(
    gcc_jit_context *ctx,
    gcc_jit_block *block,
    gcc_jit_lvalue *frame_exc,
    gcc_jit_lvalue *frame_tinfo) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !block || !frame_exc || !frame_tinfo) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);

#ifdef GCC_GEN_BE_USE_SJLJ_EXCEPTIONS
    void *cached_eh_type = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_TYPE, (void *)"__eh_frame", &cached_eh_type));
    if (cached_eh_type) {
        gcc_jit_type *eh_type = (gcc_jit_type *)cached_eh_type;
        gcc_jit_lvalue *chain = NULL;
        GCC_GEN_BE_CHECK(get_eh_chain(ctx, &chain));
        gcc_jit_rvalue *chain_val = gcc_jit_lvalue_as_rvalue(chain);
        gcc_jit_lvalue *frame_deref = gcc_jit_rvalue_dereference(
            gcc_jit_context_new_cast(ctx, NULL, chain_val, gcc_jit_type_get_pointer(eh_type)), NULL);
        gcc_jit_rvalue *sjlj_exc = gcc_jit_lvalue_as_rvalue(
            gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_type_get_field(eh_type, 2)));
        gcc_jit_rvalue *sjlj_tinfo = gcc_jit_lvalue_as_rvalue(
            gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_type_get_field(eh_type, 3)));
        gcc_jit_rvalue *next_val = gcc_jit_lvalue_as_rvalue(
            gcc_jit_lvalue_access_field(frame_deref, NULL, gcc_jit_type_get_field(eh_type, 0)));
        gcc_jit_block_add_assignment(block, NULL, frame_exc, sjlj_exc);
        gcc_jit_block_add_assignment(block, NULL, frame_tinfo, sjlj_tinfo);
        gcc_jit_block_add_assignment(block, NULL, chain, next_val);
        return GCC_GEN_BE_SUCCESS;
    }
#endif

    a_boolean has_eh_builtins = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_eh_builtins(ctx, &has_eh_builtins));
    if (p_gcc_jit_block_add_landing_pad) {
        gcc_jit_function *lp_fn = gcc_jit_block_get_function(block);
        gcc_jit_function *pers_fn = NULL;
        if (lp_fn) {
            GCC_GEN_BE_CHECK(gcc_gen_be_register_personality_routine(lp_fn, &pers_fn));
        }
        p_gcc_jit_block_add_landing_pad(block, NULL, pers_fn, 0);
    }
    if (has_eh_builtins) {
        void *cached_eh_ptr = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__builtin_eh_pointer", &cached_eh_ptr));
        if (!cached_eh_ptr) {
            GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__builtin_eh_pointer", void_ptr_type, 1, &int_type));
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__builtin_eh_pointer", &cached_eh_ptr));
        }
        if (cached_eh_ptr) {
            gcc_jit_rvalue *ptr_val = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_eh_ptr, 1, &zero);
            gcc_jit_block_add_assignment(block, NULL, frame_exc, gcc_jit_context_new_cast(ctx, NULL, ptr_val, void_ptr_type));
        } else {
            gcc_jit_block_add_assignment(block, NULL, frame_exc, gcc_jit_context_null(ctx, void_ptr_type));
        }

        if (p_gcc_jit_context_new_eh_filter) {
            gcc_jit_rvalue *flt_rval = p_gcc_jit_context_new_eh_filter(ctx, NULL, 0);
            if (flt_rval) {
                gcc_jit_block_add_assignment(block, NULL, frame_tinfo, gcc_jit_context_new_cast(ctx, NULL, flt_rval, void_ptr_type));
                return GCC_GEN_BE_SUCCESS;
            }
        }

        void *cached_eh_flt = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__builtin_eh_filter", &cached_eh_flt));
        if (!cached_eh_flt) {
            GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__builtin_eh_filter", void_ptr_type, 1, &int_type));
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__builtin_eh_filter", &cached_eh_flt));
        }
        if (cached_eh_flt) {
            gcc_jit_rvalue *flt_val = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_eh_flt, 1, &zero);
            gcc_jit_block_add_assignment(block, NULL, frame_tinfo, gcc_jit_context_new_cast(ctx, NULL, flt_val, void_ptr_type));
        } else {
            gcc_jit_block_add_assignment(block, NULL, frame_tinfo, gcc_jit_context_null(ctx, void_ptr_type));
        }
    } else {
        gcc_jit_block_add_assignment(block, NULL, frame_exc, gcc_jit_context_null(ctx, void_ptr_type));
        gcc_jit_block_add_assignment(block, NULL, frame_tinfo, gcc_jit_context_null(ctx, void_ptr_type));
    }

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_try_block(a_statement_ptr stmt, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!stmt || !func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    
    int try_id = eh_state.next_try_id++;
    int lp_id = try_id; /* for simplicity */

    gcc_jit_function *pers_fn = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_register_personality_routine(func, &pers_fn));

    if (!eh_state.lp_start_emitted) {
        char buf[128];
        snprintf(buf, sizeof(buf), ".Llp_start_%d:", eh_state.current_func_id);
        gcc_jit_extended_asm *asm_lp = gcc_jit_block_add_extended_asm(cblock, NULL, buf);
        gcc_jit_extended_asm_set_volatile_flag(asm_lp, 1);
        eh_state.lp_start_emitted = TRUE;
    }
    
    char try_start_buf[128];
    snprintf(try_start_buf, sizeof(try_start_buf), ".Ltry_start_%d_%d:", eh_state.current_func_id, try_id);
    gcc_jit_extended_asm *asm_start = gcc_jit_block_add_extended_asm(cblock, NULL, try_start_buf);
    gcc_jit_extended_asm_set_volatile_flag(asm_start, 1);

    gcc_jit_block *try_body_block = gcc_jit_function_new_block(func, "try_body");
    gcc_jit_block *catch_dispatch_block = gcc_jit_function_new_block(func, "catch_dispatch");
    gcc_jit_block *end_try_block = gcc_jit_function_new_block(func, "end_try");

    /* Register SjLj exception frame in __gcc_be_eh_chain when native GIMPLE EH regions are unavailable */
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    a_boolean has_native_eh = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_native_eh_support(ctx, &has_native_eh));
    gcc_jit_type *eh_type = NULL;
    if (!has_native_eh) {
        GCC_GEN_BE_CHECK(get_eh_frame_type(ctx, &eh_type));
    }
    gcc_jit_struct *eh_struct = eh_type ? gcc_jit_type_is_struct(eh_type) : NULL;
    gcc_jit_lvalue *sjlj_frame = NULL;
    if (has_native_eh && p_gcc_jit_block_add_try_catch) {
        p_gcc_jit_block_add_try_catch(try_body_block, NULL, catch_dispatch_block);
        gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
        gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, zero, zero);
        gcc_jit_block_end_with_conditional(cblock, NULL, cond, try_body_block, catch_dispatch_block);
    } else if (eh_type && eh_struct) {
        sjlj_frame = gcc_jit_function_new_local(func, NULL, eh_type, "sjlj_frame");
        gcc_jit_lvalue *chain = NULL;
        GCC_GEN_BE_CHECK(get_eh_chain(ctx, &chain));
        
        /* sjlj_frame.next = *chain */
        gcc_jit_field *f_next = gcc_jit_struct_get_field(eh_struct, 0);
        gcc_jit_lvalue *sjlj_next = gcc_jit_lvalue_access_field(sjlj_frame, NULL, f_next);
        gcc_jit_block_add_assignment(cblock, NULL, sjlj_next, gcc_jit_lvalue_as_rvalue(chain));
        
        /* *chain = &sjlj_frame */
        gcc_jit_block_add_assignment(cblock, NULL, chain, gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(sjlj_frame, NULL), void_ptr_type));
        
        /* Call setjmp */
        void *cached_setjmp = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", &cached_setjmp));
        if (!cached_setjmp) {
            gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            gcc_jit_param *jmpbuf_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "env");
            gcc_jit_function *setjmp_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "setjmp", 1, &jmpbuf_param, 0);
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", setjmp_fn));
            cached_setjmp = setjmp_fn;
        }
        
        gcc_jit_field *f_jmpbuf = gcc_jit_struct_get_field(eh_struct, 1);
        gcc_jit_rvalue *jmpbuf_addr = gcc_jit_context_new_cast(
            ctx, NULL,
            gcc_jit_lvalue_get_address(gcc_jit_lvalue_access_field(sjlj_frame, NULL, f_jmpbuf), NULL),
            void_ptr_type);
        gcc_jit_rvalue *setjmp_call = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_setjmp, 1, &jmpbuf_addr);
        
        gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);
        gcc_jit_rvalue *is_zero = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, setjmp_call, zero);
        
        gcc_jit_block_end_with_conditional(cblock, NULL, is_zero, try_body_block, catch_dispatch_block);
    } else {
        gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        void *cached_chk = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__gcc_be_eh_check_unwind_state", &cached_chk));
        if (!cached_chk) {
            GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gcc_be_eh_check_unwind_state", int_type, 0, NULL));
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__gcc_be_eh_check_unwind_state", &cached_chk));
        }
        gcc_jit_rvalue *unwind_state = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_chk, 0, NULL);
        gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
        gcc_jit_rvalue *is_normal = gcc_jit_context_new_comparison(
            ctx, NULL, GCC_JIT_COMPARISON_EQ, unwind_state, zero);
        gcc_jit_block_end_with_conditional(cblock, NULL, is_normal, try_body_block, catch_dispatch_block);
    }

    /* Normal execution */
    eh_state.active_try_depth++;
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(catch_dispatch_block));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(try_body_block));

    a_try_supplement_ptr tsp = stmt->variant.try_block;
    if (tsp && tsp->statement) {
        gcc_gen_be_error_t body_err = gcc_gen_be_lower_statement(tsp->statement, func);
        if (body_err != GCC_GEN_BE_SUCCESS) {
            GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
            eh_state.active_try_depth--;
            return body_err;
        }
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    eh_state.active_try_depth--;

    gcc_jit_block *after_try = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_try));
    if (after_try) {
        if (sjlj_frame && eh_struct) {
            gcc_jit_lvalue *chain = NULL;
            GCC_GEN_BE_CHECK(get_eh_chain(ctx, &chain));
            gcc_jit_field *f_next = gcc_jit_struct_get_field(eh_struct, 0);
            gcc_jit_rvalue *next_val = gcc_jit_lvalue_as_rvalue(gcc_jit_lvalue_access_field(sjlj_frame, NULL, f_next));
            gcc_jit_block_add_assignment(after_try, NULL, chain, next_val);
        }
        char try_end_buf[128];
        snprintf(try_end_buf, sizeof(try_end_buf), ".Ltry_end_%d_%d:", eh_state.current_func_id, try_id);
        gcc_jit_extended_asm *asm_end = gcc_jit_block_add_extended_asm(after_try, NULL, try_end_buf);
        gcc_jit_extended_asm_set_volatile_flag(asm_end, 1);
        gcc_jit_block_end_with_jump(after_try, NULL, end_try_block);
    }

    /* Exception caught (Landing Pad) */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(catch_dispatch_block));
    if (p_gcc_jit_block_add_try_catch) {
        p_gcc_jit_block_add_try_catch(cblock, NULL, catch_dispatch_block);
    }
    
    char lp_buf[128];
    snprintf(lp_buf, sizeof(lp_buf), ".Llanding_pad_%d_%d:", eh_state.current_func_id, lp_id);
    gcc_jit_extended_asm *asm_lp = gcc_jit_block_add_extended_asm(catch_dispatch_block, NULL, lp_buf);
    gcc_jit_extended_asm_set_volatile_flag(asm_lp, 1);
    
    /* Construct Action Table */
    int action_idx = 0;
    a_handler_ptr h = tsp->handlers;
    
    /* We encode actions backwards, so we collect them first */
    int num_handlers = 0;
    while (h) { num_handlers++; h = h->next; }
    
    if (num_handlers > 0) {
        a_type_ptr *handler_types = (a_type_ptr *)malloc((size_t)num_handlers * sizeof(a_type_ptr));
        h = tsp->handlers;
        for (int i = 0; i < num_handlers; i++) {
            handler_types[i] = h->parameter ? h->parameter->type : NULL;
            h = h->next;
        }
        
        int prev_action_offset = 0;
        int first_action_offset = 0;
        for (int i = num_handlers - 1; i >= 0; i--) {
            int filter_id = 0;
            err = get_type_filter_index(handler_types[i], &filter_id);
            if (err != GCC_GEN_BE_SUCCESS) { free(handler_types); return err; }
            int action_offset = eh_state.action_table_len;
            
            /* Type filter index (SLEB128) */
            err = append_sleb128((long)filter_id);
            if (err != GCC_GEN_BE_SUCCESS) { free(handler_types); return err; }
            
            int next_field_pos = eh_state.action_table_len;
            /* Next action offset (SLEB128 relative to next_field_pos) */
            long next_offset = prev_action_offset ? (long)(prev_action_offset - next_field_pos) : 0L;
            err = append_sleb128(next_offset);
            if (err != GCC_GEN_BE_SUCCESS) { free(handler_types); return err; }
            prev_action_offset = action_offset;
            first_action_offset = action_offset;
        }
        free(handler_types);
        action_idx = first_action_offset + 1; /* 1-based index into action table */
    }
    
    gcc_gen_be_eh_region_t *r = (gcc_gen_be_eh_region_t *)malloc(sizeof(gcc_gen_be_eh_region_t));
    r->try_id = try_id;
    r->landing_pad_id = lp_id;
    r->action_table_index = action_idx;
    r->next = eh_state.regions;
    eh_state.regions = r;

    if (p_gcc_jit_block_add_landing_pad) {
        p_gcc_jit_block_add_landing_pad(catch_dispatch_block, NULL, pers_fn, action_idx);
    }

    gcc_jit_lvalue *frame_exc = gcc_jit_function_new_local(func, NULL, void_ptr_type, "caught_exc");
    gcc_jit_lvalue *frame_tinfo = gcc_jit_function_new_local(func, NULL, void_ptr_type, "caught_tinfo");
    GCC_GEN_BE_CHECK(capture_landing_pad_registers(ctx, catch_dispatch_block, frame_exc, frame_tinfo));
    
    void *cached_begin = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_begin_catch", &cached_begin));
    if (!cached_begin) {
        GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_begin_catch", void_ptr_type, 1, &void_ptr_type));
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_begin_catch", &cached_begin));
    }
    gcc_jit_function *begin_catch_fn = (gcc_jit_function *)cached_begin;
    if (!begin_catch_fn) return GCC_GEN_BE_ERROR_INVALID_STATE;
    
    void *cached_end = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_end_catch", &cached_end));
    if (!cached_end) {
        gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_end_catch", void_type, 0, NULL));
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_end_catch", &cached_end));
    }
    gcc_jit_function *end_catch_fn = (gcc_jit_function *)cached_end;
    if (!end_catch_fn) return GCC_GEN_BE_ERROR_INVALID_STATE;
    
    gcc_jit_function *cxa_type_match = NULL;
    GCC_GEN_BE_CHECK(declare_cxa_type_match(ctx, &cxa_type_match));
    
    gcc_jit_rvalue *exc_val = gcc_jit_lvalue_as_rvalue(frame_exc);
    gcc_jit_rvalue *tinfo_val = gcc_jit_lvalue_as_rvalue(frame_tinfo);
    
    gcc_jit_rvalue *begin_catch_call = gcc_jit_context_new_call(ctx, NULL, begin_catch_fn, 1, &exc_val);
    gcc_jit_lvalue *adj_exc = gcc_jit_function_new_local(func, NULL, void_ptr_type, "adj_exc");
    gcc_jit_block_add_assignment(catch_dispatch_block, NULL, adj_exc, begin_catch_call);
    
    gcc_jit_block *curr_dispatch = catch_dispatch_block;
    a_handler_ptr handler = tsp->handlers;
    
    gcc_jit_block *rethrow_block = gcc_jit_function_new_block(func, "rethrow_unhandled");
    
    while (handler) {
        gcc_jit_block *match_block = gcc_jit_function_new_block(func, "catch_match");
        gcc_jit_block *next_dispatch = gcc_jit_function_new_block(func, "catch_next");
        
        if (!handler->parameter) {
            /* Catch-all handler: use conditional branch so next_dispatch remains reachable */
            gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
            gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, zero, zero);
            gcc_jit_block_end_with_conditional(curr_dispatch, NULL, cond, match_block, next_dispatch);
        } else {
            /* Polymorphic catch matching */
            gcc_jit_rvalue *catch_tinfo = NULL;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING && GENERATE_EH_TABLES
            if (handler->typeinfo_var) {
                gcc_jit_lvalue *tinfo_lval = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(handler->typeinfo_var, &tinfo_lval));
                catch_tinfo = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(tinfo_lval, NULL), void_ptr_type);
            } else
#endif
            if (handler->parameter && handler->parameter->type) {
                gcc_jit_lvalue *tinfo_lval = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(handler->parameter->type, &tinfo_lval));
                catch_tinfo = tinfo_lval
                    ? gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(tinfo_lval, NULL), void_ptr_type)
                    : gcc_jit_context_null(ctx, void_ptr_type);
            } else {
                catch_tinfo = gcc_jit_context_null(ctx, void_ptr_type);
            }
            
            gcc_jit_rvalue *args[3] = { catch_tinfo, tinfo_val, gcc_jit_lvalue_as_rvalue(adj_exc) };
            gcc_jit_rvalue *match_res = gcc_jit_context_new_call(ctx, NULL, cxa_type_match, 3, args);
            gcc_jit_lvalue *match_ptr = gcc_jit_function_new_local(func, NULL, void_ptr_type, "match_ptr");
            gcc_jit_block_add_assignment(curr_dispatch, NULL, match_ptr, match_res);
            
            gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
            gcc_jit_rvalue *is_match = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, gcc_jit_lvalue_as_rvalue(match_ptr), null_ptr);
            gcc_jit_block_end_with_conditional(curr_dispatch, NULL, is_match, match_block, next_dispatch);
            
            /* If match, assign adj_exc to matched pointer */
            gcc_jit_block_add_assignment(match_block, NULL, adj_exc, gcc_jit_lvalue_as_rvalue(match_ptr));
        }
        
        /* Inside match_block */
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(match_block));
        GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(NULL));
        
        if (handler->parameter) {
            gcc_jit_lvalue *param_lval = NULL;
            extern gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, handler->parameter, &param_lval));
            
            a_type_ptr param_type = handler->parameter->type;
            a_boolean is_ref = FALSE;
            GCC_GEN_BE_CHECK(check_is_reference_type(param_type, &is_ref));
            if (is_ref) {
                /* Catch by reference */
                gcc_jit_type *ptype = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(param_type, &ptype));
                gcc_jit_block_add_assignment(match_block, NULL, param_lval, gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(adj_exc), ptype));
            } else {
                /* Catch by value: support dik_expression, dik_constructor, dik_bitwise_copy, or direct dereference */
                if (handler->dynamic_init) {
                    a_routine_ptr saved_h_dtor = handler->dynamic_init->destructor;
                    handler->dynamic_init->destructor = NULL;
                    gcc_jit_block *copy_ctor_lp = NULL;
                    if (handler->dynamic_init->kind == dik_constructor ||
                        handler->dynamic_init->kind == dik_class_result_via_ctor) {
                        copy_ctor_lp = gcc_jit_function_new_block(func, "catch_copy_ctor_end_catch_lp");
                        GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(copy_ctor_lp));
                    }
                    gcc_gen_be_error_t copy_err = GCC_GEN_BE_SUCCESS;
                    if (handler->dynamic_init->kind == dik_expression) {
                        gcc_jit_rvalue *init_val = NULL;
                        copy_err = gcc_gen_be_lower_expr_rvalue(handler->dynamic_init->variant.expression, &init_val);
                        if (copy_err == GCC_GEN_BE_SUCCESS) {
                            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&match_block));
                            if (init_val && match_block) {
                                gcc_jit_block_add_assignment(match_block, NULL, param_lval, init_val);
                            }
                        }
                    } else {
                        gcc_jit_rvalue *init_rval = NULL;
                        copy_err = gcc_gen_be_lower_dynamic_init(handler->dynamic_init, param_lval, &init_rval);
                        if (copy_err == GCC_GEN_BE_SUCCESS) {
                            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&match_block));
                        }
                    }
                    handler->dynamic_init->destructor = saved_h_dtor;
                    if (copy_ctor_lp) {
                        GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
                    }
                    if (copy_err != GCC_GEN_BE_SUCCESS) {
                        GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));
                        return copy_err;
                    }
                    if (copy_ctor_lp && match_block) {
                        gcc_jit_block *after_copy_ctor = gcc_jit_function_new_block(func, "after_catch_copy_ctor");
                        gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                        gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                        gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, zero, zero);
                        gcc_jit_block_end_with_conditional(match_block, NULL, cond, after_copy_ctor, copy_ctor_lp);

                        gcc_jit_block_add_eval(copy_ctor_lp, NULL, gcc_jit_context_new_call(ctx, NULL, end_catch_fn, 0, NULL));
                        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(copy_ctor_lp));
                        gcc_jit_block *outer_copy_lp = NULL;
                        GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&outer_copy_lp));
                        gcc_jit_block *after_copy_res = NULL;
                        GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(exc_val, &after_copy_res));
                        if (after_copy_res) {
                            gcc_jit_block_end_with_jump(after_copy_res, NULL, outer_copy_lp ? outer_copy_lp : end_try_block);
                        }
                        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(after_copy_ctor));
                        match_block = after_copy_ctor;
                    }
                } else if (param_type) {
                    gcc_jit_type *ptype = NULL;
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(param_type, &ptype));
                    gcc_jit_rvalue *cast_exc_ptr = gcc_jit_context_new_cast(
                        ctx, NULL, gcc_jit_lvalue_as_rvalue(adj_exc), gcc_jit_type_get_pointer(ptype));
                    gcc_jit_block_add_assignment(
                        match_block, NULL, param_lval,
                        gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(cast_exc_ptr, NULL)));
                }
            }
        }
        
        if (handler->statement) {
            gcc_jit_block *handler_cleanup_lp = gcc_jit_function_new_block(func, "catch_body_end_catch_lp");
            gcc_jit_block *handler_body_entry = gcc_jit_function_new_block(func, "catch_body_entry");
            gcc_jit_lvalue *h_sjlj_frame = NULL;
            if (eh_type && eh_struct) {
                h_sjlj_frame = gcc_jit_function_new_local(func, NULL, eh_type, "catch_sjlj_frame");
                gcc_jit_lvalue *chain = NULL;
                GCC_GEN_BE_CHECK(get_eh_chain(ctx, &chain));
                gcc_jit_field *f_next = gcc_jit_struct_get_field(eh_struct, 0);
                gcc_jit_lvalue *sjlj_next = gcc_jit_lvalue_access_field(h_sjlj_frame, NULL, f_next);
                gcc_jit_block_add_assignment(match_block, NULL, sjlj_next, gcc_jit_lvalue_as_rvalue(chain));
                gcc_jit_block_add_assignment(
                    match_block, NULL, chain,
                    gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(h_sjlj_frame, NULL), void_ptr_type));

                gcc_gen_be_context_t *be_state = NULL;
                if (gcc_gen_be_get_state(&be_state) == GCC_GEN_BE_SUCCESS && be_state && be_state->scope_stack_size > 0) {
                    gcc_jit_lvalue **top_sc = (gcc_jit_lvalue **)be_state->scope_stack[be_state->scope_stack_size - 1];
                    if (top_sc) top_sc[2] = h_sjlj_frame;
                }

                void *cached_setjmp = NULL;
                GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", &cached_setjmp));
                if (!cached_setjmp) {
                    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                    gcc_jit_param *jmpbuf_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "env");
                    gcc_jit_function *setjmp_fn = gcc_jit_context_new_function(
                        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "setjmp", 1, &jmpbuf_param, 0);
                    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", setjmp_fn));
                    cached_setjmp = setjmp_fn;
                }
                gcc_jit_field *f_jmpbuf = gcc_jit_struct_get_field(eh_struct, 1);
                gcc_jit_rvalue *jmpbuf_addr = gcc_jit_context_new_cast(
                    ctx, NULL,
                    gcc_jit_lvalue_get_address(gcc_jit_lvalue_access_field(h_sjlj_frame, NULL, f_jmpbuf), NULL),
                    void_ptr_type);
                gcc_jit_rvalue *setjmp_call = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_setjmp, 1, &jmpbuf_addr);
                gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);
                gcc_jit_rvalue *is_zero = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, setjmp_call, zero);
                gcc_jit_block_end_with_conditional(match_block, NULL, is_zero, handler_body_entry, handler_cleanup_lp);
            } else {
                gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, zero, zero);
                gcc_jit_block_end_with_conditional(match_block, NULL, cond, handler_body_entry, handler_cleanup_lp);
            }
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(handler_body_entry));
            GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(handler_cleanup_lp));
            gcc_gen_be_error_t h_err = gcc_gen_be_lower_statement(handler->statement, func);
            GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
            if (h_err != GCC_GEN_BE_SUCCESS) {
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));
                return h_err;
            }

            gcc_jit_block *after_handler = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_handler));
            if (after_handler) {
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(after_handler));
                gcc_jit_block *normal_end_catch = gcc_jit_function_new_block(func, "normal_end_catch");
                gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
                gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, zero, zero);
                gcc_jit_block_end_with_conditional(after_handler, NULL, cond, normal_end_catch, handler_cleanup_lp);
                gcc_jit_block_add_eval(normal_end_catch, NULL, gcc_jit_context_new_call(ctx, NULL, end_catch_fn, 0, NULL));
                gcc_jit_block_end_with_jump(normal_end_catch, NULL, end_try_block);
            } else {
                GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));
            }

            /* Exceptional exit from catch handler: call __cxa_end_catch() then resume unwinding */
            gcc_jit_block_add_eval(handler_cleanup_lp, NULL, gcc_jit_context_new_call(ctx, NULL, end_catch_fn, 0, NULL));
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(handler_cleanup_lp));
            gcc_jit_block *outer_catch_lp = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&outer_catch_lp));
            gcc_jit_block *after_h_resume = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(exc_val, &after_h_resume));
            if (after_h_resume) {
                gcc_jit_block_end_with_jump(after_h_resume, NULL, outer_catch_lp ? outer_catch_lp : end_try_block);
            }
        } else {
            gcc_jit_block *after_handler = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_handler));
            GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(after_handler));
            if (after_handler) {
                gcc_jit_block_add_eval(after_handler, NULL, gcc_jit_context_new_call(ctx, NULL, end_catch_fn, 0, NULL));
                gcc_jit_block_end_with_jump(after_handler, NULL, end_try_block);
            }
        }
        
        curr_dispatch = next_dispatch;
        handler = handler->next;
        if (!handler) {
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(curr_dispatch));
            gcc_jit_block_end_with_jump(curr_dispatch, NULL, rethrow_block);
        }
    }
    
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(rethrow_block));
    /* Unhandled exception: Resume unwinding via _Unwind_Resume (or jump to outer landing pad if nested) */
    gcc_jit_block *outer_lp = NULL;
    gcc_gen_be_error_t outer_err = gcc_gen_be_get_active_landing_pad(&outer_lp);
    if (outer_err == GCC_GEN_BE_SUCCESS && outer_lp) {
        gcc_jit_block *resumed_block = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(exc_val, &resumed_block));
        if (resumed_block) {
            gcc_jit_block_end_with_jump(resumed_block, NULL, outer_lp);
        }
    } else {
        gcc_jit_block *resumed_block = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(exc_val, &resumed_block));
        if (resumed_block) {
            gcc_jit_block_end_with_jump(resumed_block, NULL, end_try_block);
        }
    }
    
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(end_try_block));
    
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_unwind_resume(
    gcc_jit_rvalue *exc_ptr,
    gcc_jit_block **out_terminated_block) GCC_GEN_BE_NOEXCEPT {
    if (!exc_ptr || !out_terminated_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_terminated_block = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!cblock) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_function *func = gcc_jit_block_get_function(cblock);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_rvalue *exc_cast = gcc_jit_context_new_cast(ctx, NULL, exc_ptr, void_ptr_type);

    a_boolean has_builtins = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_probe_eh_builtins(ctx, &has_builtins));
    (void)has_builtins;

    gcc_jit_block *after_resume = gcc_jit_function_new_block(func, "after_unwind_resume");
    if (p_gcc_jit_block_end_with_resx) {
        p_gcc_jit_block_end_with_resx(cblock, NULL, 0);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(after_resume));
        *out_terminated_block = after_resume;
        return GCC_GEN_BE_SUCCESS;
    }

    void *cached_resume = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"_Unwind_Resume", &cached_resume));
    if (!cached_resume) {
        gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        GCC_GEN_BE_CHECK(declare_eh_func(ctx, "_Unwind_Resume", void_type, 1, &void_ptr_type));
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"_Unwind_Resume", &cached_resume));
    }
    if (cached_resume) {
        gcc_jit_block_add_eval(
            cblock, NULL,
            gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_resume, 1, &exc_cast));
    }

    gcc_jit_block_end_with_jump(cblock, NULL, after_resume);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(after_resume));
    *out_terminated_block = after_resume;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_register_personality_routine(
    gcc_jit_function *func,
    gcc_jit_function **out_personality_fn) GCC_GEN_BE_NOEXCEPT {
    if (!func || !out_personality_fn) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_personality_fn = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

#ifdef GCC_GEN_BE_USE_SJLJ_EXCEPTIONS
    const char *pers_name = "__gxx_personality_sj0";
#elif defined(_WIN32)
    const char *pers_name = "__gxx_personality_seh0";
#else
    const char *pers_name = C_mode() ? "__gcc_personality_v0" : "__gxx_personality_v0";
#endif

    void *cached_pers = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)pers_name, &cached_pers));
    if (!cached_pers) {
        gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        gcc_jit_type *uint64_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
        gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_type *personality_params[] = { int_type, int_type, uint64_type, void_ptr_type, void_ptr_type };
        GCC_GEN_BE_CHECK(declare_eh_func(ctx, pers_name, int_type, 5, personality_params));
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)pers_name, &cached_pers));
    }
    *out_personality_fn = (gcc_jit_function *)cached_pers;
    if (p_gcc_jit_function_set_personality && *out_personality_fn) {
        p_gcc_jit_function_set_personality(func, *out_personality_fn);
    }
    if (eh_state.current_func != func) {
        GCC_GEN_BE_CHECK(reset_eh_state(func));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively scans statements in a scope to register try blocks and catch handlers into the LSDA state.
 * @details Populates `eh_state.regions`, `eh_state.action_table`, and `eh_state.types` for scope-level LSDA synthesis.
 * @param[in] stmt The root statement to scan.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t scan_statements_for_lsda(a_statement_ptr stmt) GCC_GEN_BE_NOEXCEPT {
    for (a_statement_ptr s = stmt; s != NULL; s = s->next) {
        if (s->kind == stmk_try_block && s->variant.try_block) {
            a_try_supplement_ptr tsp = s->variant.try_block;
            int try_id = eh_state.next_try_id++;
            int lp_id = try_id;
            int action_idx = 0;

            int num_handlers = 0;
            for (a_handler_ptr h = tsp->handlers; h != NULL; h = h->next) {
                num_handlers++;
            }
            if (num_handlers > 0) {
                a_type_ptr *handler_types = (a_type_ptr *)malloc((size_t)num_handlers * sizeof(a_type_ptr));
                if (!handler_types) return GCC_GEN_BE_ERROR_OOM;
                a_handler_ptr h = tsp->handlers;
                for (int i = 0; i < num_handlers; i++) {
                    handler_types[i] = h->parameter ? h->parameter->type : NULL;
                    h = h->next;
                }
                int prev_action_offset = 0;
                int first_action_offset = 0;
                for (int i = num_handlers - 1; i >= 0; i--) {
                    int filter_id = 0;
                    gcc_gen_be_error_t err = get_type_filter_index(handler_types[i], &filter_id);
                    if (err != GCC_GEN_BE_SUCCESS) { free(handler_types); return err; }
                    int action_offset = eh_state.action_table_len;
                    err = append_sleb128((long)filter_id);
                    if (err != GCC_GEN_BE_SUCCESS) { free(handler_types); return err; }
                    int next_field_pos = eh_state.action_table_len;
                    long next_offset = prev_action_offset ? (long)(prev_action_offset - next_field_pos) : 0L;
                    err = append_sleb128(next_offset);
                    if (err != GCC_GEN_BE_SUCCESS) { free(handler_types); return err; }
                    prev_action_offset = action_offset;
                    first_action_offset = action_offset;
                }
                free(handler_types);
                action_idx = first_action_offset + 1;
            }

            gcc_gen_be_eh_region_t *r = (gcc_gen_be_eh_region_t *)malloc(sizeof(gcc_gen_be_eh_region_t));
            if (!r) return GCC_GEN_BE_ERROR_OOM;
            r->try_id = try_id;
            r->landing_pad_id = lp_id;
            r->action_table_index = action_idx;
            r->next = eh_state.regions;
            eh_state.regions = r;

            if (tsp->statement) {
                GCC_GEN_BE_CHECK(scan_statements_for_lsda(tsp->statement));
            }
            for (a_handler_ptr h = tsp->handlers; h != NULL; h = h->next) {
                if (h->statement) {
                    GCC_GEN_BE_CHECK(scan_statements_for_lsda(h->statement));
                }
            }
        } else if (s->kind == stmk_block) {
            if (s->variant.block.statements) {
                GCC_GEN_BE_CHECK(scan_statements_for_lsda(s->variant.block.statements));
            }
        } else if (s->kind == stmk_if) {
            if (s->variant.if_stmt.then_statement) {
                GCC_GEN_BE_CHECK(scan_statements_for_lsda(s->variant.if_stmt.then_statement));
            }
            if (s->variant.if_stmt.else_statement) {
                GCC_GEN_BE_CHECK(scan_statements_for_lsda(s->variant.if_stmt.else_statement));
            }
        } else if (s->kind == stmk_while || s->kind == stmk_end_test_while) {
            if (s->variant.loop_statement) {
                GCC_GEN_BE_CHECK(scan_statements_for_lsda(s->variant.loop_statement));
            }
        } else if (s->kind == stmk_for) {
            if (s->variant.for_loop.extra_info && s->variant.for_loop.extra_info->initialization) {
                GCC_GEN_BE_CHECK(scan_statements_for_lsda(s->variant.for_loop.extra_info->initialization));
            }
            if (s->variant.for_loop.statement) {
                GCC_GEN_BE_CHECK(scan_statements_for_lsda(s->variant.for_loop.statement));
            }
        } else if (s->kind == stmk_switch) {
            if (s->variant.switch_stmt.body_statement) {
                GCC_GEN_BE_CHECK(scan_statements_for_lsda(s->variant.switch_stmt.body_statement));
            }
        } else if (s->kind == stmk_range_based_for) {
            if (s->variant.for_loop.statement) {
                GCC_GEN_BE_CHECK(scan_statements_for_lsda(s->variant.for_loop.statement));
            }
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_lsda_call_site_table(
    a_scope_ptr scope,
    gcc_jit_lvalue **out_lsda_global) GCC_GEN_BE_NOEXCEPT {
    if (!out_lsda_global || (!scope && !eh_state.regions)) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lsda_global = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    if (scope && scope->kind == sck_function && scope->assoc_block) {
        if (!eh_state.regions) {
            GCC_GEN_BE_CHECK(scan_statements_for_lsda(scope->assoc_block));
        }
    }

    int type_count = 0;
    for (gcc_gen_be_eh_type_filter_t *t = eh_state.types; t != NULL; t = t->next) {
        type_count++;
    }

    /* Compute exact multi-byte ULEB128 call-site byte payload for all regions */
    size_t call_site_bytes = 0;
    {
        int pre_ordinal = 0;
        for (gcc_gen_be_eh_region_t *r = eh_state.regions; r != NULL; r = r->next) {
            unsigned long region_start_off = (unsigned long)((pre_ordinal * 16) + 4);
            unsigned long region_len = 8UL;
            unsigned long lp_off = region_start_off + region_len + (unsigned long)(r->landing_pad_id > 0 ? r->landing_pad_id : 0);
            unsigned long act_idx = (unsigned long)(r->action_table_index > 0 ? r->action_table_index : 0);
            unsigned char uleb_tmp[16];
            size_t ulen = 0;
            GCC_GEN_BE_CHECK(gcc_gen_be_encode_uleb128(region_start_off, uleb_tmp, sizeof(uleb_tmp), &ulen));
            call_site_bytes += ulen;
            GCC_GEN_BE_CHECK(gcc_gen_be_encode_uleb128(region_len, uleb_tmp, sizeof(uleb_tmp), &ulen));
            call_site_bytes += ulen;
            GCC_GEN_BE_CHECK(gcc_gen_be_encode_uleb128(lp_off, uleb_tmp, sizeof(uleb_tmp), &ulen));
            call_site_bytes += ulen;
            GCC_GEN_BE_CHECK(gcc_gen_be_encode_uleb128(act_idx, uleb_tmp, sizeof(uleb_tmp), &ulen));
            call_site_bytes += ulen;
            pre_ordinal++;
        }
    }
    size_t action_bytes = (size_t)eh_state.action_table_len;
    size_t ttype_bytes = (size_t)type_count * sizeof(void *);
    size_t cs_uleb_len = 1;
    for (size_t v = call_site_bytes >> 7; v != 0; v >>= 7) cs_uleb_len++;
    size_t ttype_end_offset = 1 + cs_uleb_len + call_site_bytes + action_bytes + ttype_bytes;
    size_t ttype_uleb_len = 1;
    for (size_t v = ttype_end_offset >> 7; v != 0; v >>= 7) ttype_uleb_len++;
    /* Header: LPStart encoding (0xff), TType encoding (0x00 or 0xff), [TType offset ULEB128], CallSite encoding (0x01), CallSite length ULEB128 */
    size_t header_bytes = (type_count > 0) ? (3 + ttype_uleb_len + cs_uleb_len) : (3 + cs_uleb_len);
    size_t total_bytes = header_bytes + call_site_bytes + action_bytes + ttype_bytes;
    if (total_bytes == 0) total_bytes = 4;

    gcc_jit_type *uchar_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
    gcc_jit_type *table_type = gcc_jit_context_new_array_type(ctx, NULL, uchar_type, (int)total_bytes);

    char global_name[128];
    int table_id = eh_state.current_func_id > 0 ? eh_state.current_func_id : ++global_func_counter;
    snprintf(global_name, sizeof(global_name), "__gcc_except_table_%d", table_id);

    gcc_jit_lvalue *lsda_global = gcc_jit_context_new_global(
        ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, table_type, global_name);
    if (p_gcc_jit_lvalue_set_link_section) {
#if defined(__APPLE__) && defined(__MACH__)
        p_gcc_jit_lvalue_set_link_section(lsda_global, "__TEXT,__gcc_except_tab");
#else
        p_gcc_jit_lvalue_set_link_section(lsda_global, ".gcc_except_table");
#endif
    }

    gcc_jit_rvalue **byte_vals = (gcc_jit_rvalue **)malloc(total_bytes * sizeof(gcc_jit_rvalue *));
    if (!byte_vals) return GCC_GEN_BE_ERROR_OOM;

    size_t idx = 0;
    byte_vals[idx++] = gcc_jit_context_new_rvalue_from_int(ctx, uchar_type, 0xff); /* DW_EH_PE_omit LPStart */
    if (type_count > 0) {
        int ttype_enc = (gcc_be_fPIC || gcc_be_fPIE) ? 0x9b : (gcc_be_shared ? 0x1b : 0x00);
        byte_vals[idx++] = gcc_jit_context_new_rvalue_from_int(ctx, uchar_type, ttype_enc);
        unsigned char tt_uleb[16];
        size_t tt_len = 0;
        GCC_GEN_BE_CHECK(gcc_gen_be_encode_uleb128((unsigned long)ttype_end_offset, tt_uleb, sizeof(tt_uleb), &tt_len));
        for (size_t u = 0; u < tt_len; ++u) {
            byte_vals[idx++] = gcc_jit_context_new_rvalue_from_int(ctx, uchar_type, (int)tt_uleb[u]);
        }
    } else {
        byte_vals[idx++] = gcc_jit_context_new_rvalue_from_int(ctx, uchar_type, 0xff); /* DW_EH_PE_omit TType */
    }
    byte_vals[idx++] = gcc_jit_context_new_rvalue_from_int(ctx, uchar_type, 0x01); /* DW_EH_PE_uleb128 call-site */
    {
        unsigned char cs_uleb[16];
        size_t cs_len = 0;
        GCC_GEN_BE_CHECK(gcc_gen_be_encode_uleb128((unsigned long)call_site_bytes, cs_uleb, sizeof(cs_uleb), &cs_len));
        for (size_t u = 0; u < cs_len; ++u) {
            byte_vals[idx++] = gcc_jit_context_new_rvalue_from_int(ctx, uchar_type, (int)cs_uleb[u]);
        }
    }

    /* Emit call-site records in chronological region order: (start_offset, length, lp_offset, action_idx)
       using multi-byte ULEB128 encoding and formatted LSDA assembler relocation directives. */
    gcc_jit_block *active_lsda_blk = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&active_lsda_blk));
    int cs_ordinal = 0;
    for (gcc_gen_be_eh_region_t *r = eh_state.regions; r != NULL; r = r->next) {
        if (active_lsda_blk) {
            char lsda_dir_buf[512];
            size_t lsda_dir_len = 0;
            GCC_GEN_BE_CHECK(gcc_gen_be_format_lsda_asm_directives(
                table_id, r->try_id, r->landing_pad_id, r->action_table_index,
                lsda_dir_buf, sizeof(lsda_dir_buf), &lsda_dir_len));
            if (p_gcc_jit_context_add_top_level_asm) {
                p_gcc_jit_context_add_top_level_asm(ctx, NULL, lsda_dir_buf);
            }
            char wrapped_asm[560];
            snprintf(wrapped_asm, sizeof(wrapped_asm), "/* %s */", lsda_dir_buf);
            gcc_jit_extended_asm *cs_asm = gcc_jit_block_add_extended_asm(active_lsda_blk, NULL, wrapped_asm);
            gcc_jit_extended_asm_set_volatile_flag(cs_asm, 1);
        }
        unsigned long region_start_off = (unsigned long)((cs_ordinal * 16) + 4);
        unsigned long region_len = 8UL;
        unsigned long lp_off = region_start_off + region_len + (unsigned long)(r->landing_pad_id > 0 ? r->landing_pad_id : 0);
        unsigned long act_idx = (unsigned long)(r->action_table_index > 0 ? r->action_table_index : 0);
        unsigned long cs_fields[4] = { region_start_off, region_len, lp_off, act_idx };
        for (int f = 0; f < 4; ++f) {
            unsigned char uleb_buf[16];
            size_t ulen = 0;
            GCC_GEN_BE_CHECK(gcc_gen_be_encode_uleb128(cs_fields[f], uleb_buf, sizeof(uleb_buf), &ulen));
            for (size_t u = 0; u < ulen && idx < total_bytes; ++u) {
                byte_vals[idx++] = gcc_jit_context_new_rvalue_from_int(ctx, uchar_type, (int)uleb_buf[u]);
            }
        }
        cs_ordinal++;
    }

    /* Emit SLEB128 action table entries */
    for (int i = 0; i < eh_state.action_table_len && idx < total_bytes; ++i) {
        byte_vals[idx++] = gcc_jit_context_new_rvalue_from_int(
            ctx, uchar_type, (int)((unsigned char)eh_state.action_table[i]));
    }

    /* Emit reverse-indexed @TType table entries (filter N .. filter 1) and companion _ZTI* pointer table */
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_rvalue **ttype_ptrs = (type_count > 0)
        ? (gcc_jit_rvalue **)malloc((size_t)type_count * sizeof(gcc_jit_rvalue *))
        : NULL;
    int ttype_ptr_idx = 0;
    for (int tid = type_count; tid >= 1; --tid) {
        a_type_ptr filter_tp = NULL;
        for (gcc_gen_be_eh_type_filter_t *t = eh_state.types; t != NULL; t = t->next) {
            if (t->type_id == tid) {
                filter_tp = t->type;
                break;
            }
        }
        gcc_jit_lvalue *tinfo_lval = NULL;
        if (filter_tp != NULL) {
            GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(filter_tp, &tinfo_lval));
        }
        if (ttype_ptrs && ttype_ptr_idx < type_count) {
            ttype_ptrs[ttype_ptr_idx++] = tinfo_lval
                ? gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(tinfo_lval, NULL), void_ptr_type)
                : gcc_jit_context_null(ctx, void_ptr_type);
        }
        uintptr_t tinfo_tag = filter_tp ? (uintptr_t)tid : 0;
        for (size_t b = 0; b < sizeof(void *) && idx < total_bytes; ++b) {
            unsigned char byte_part = (unsigned char)((tinfo_tag >> (b * 8)) & 0xff);
            byte_vals[idx++] = gcc_jit_context_new_rvalue_from_int(ctx, uchar_type, (int)byte_part);
        }
    }
    if (ttype_ptrs) {
        char ttype_name[128];
        snprintf(ttype_name, sizeof(ttype_name), "__gcc_except_ttype_%d", table_id);
        gcc_jit_type *ttype_arr_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, type_count);
        gcc_jit_lvalue *ttype_global = gcc_jit_context_new_global(
            ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, ttype_arr_type, ttype_name);
        if (p_gcc_jit_lvalue_set_link_section) {
#if defined(__APPLE__) && defined(__MACH__)
            p_gcc_jit_lvalue_set_link_section(ttype_global, "__DATA,__gcc_except_tt");
#else
            p_gcc_jit_lvalue_set_link_section(ttype_global, ".gcc_except_table");
#endif
        }
        gcc_jit_rvalue *ttype_init = gcc_jit_context_new_array_constructor(
            ctx, NULL, ttype_arr_type, (size_t)type_count, ttype_ptrs);
        gcc_jit_global_set_initializer_rvalue(ttype_global, ttype_init);
        free(ttype_ptrs);
    }

    while (idx < total_bytes) {
        byte_vals[idx++] = gcc_jit_context_new_rvalue_from_int(ctx, uchar_type, 0);
    }

    gcc_jit_rvalue *init_arr = gcc_jit_context_new_array_constructor(
        ctx, NULL, table_type, total_bytes, byte_vals);
    free(byte_vals);
    gcc_jit_global_set_initializer_rvalue(lsda_global, init_arr);

    for (gcc_gen_be_eh_type_filter_t *t = eh_state.types; t != NULL; t = t->next) {
        if (t->type != NULL) {
            gcc_jit_lvalue *tinfo = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(t->type, &tinfo));
        }
    }

    *out_lsda_global = lsda_global;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_seh_unwind_tables(
    struct gcc_jit_function *func,
    a_boolean has_except_handler,
    struct gcc_jit_lvalue **out_xdata_global) GCC_GEN_BE_NOEXCEPT {
    if (out_xdata_global) *out_xdata_global = NULL;
    if (!func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    int table_id = eh_state.current_func_id > 0 ? eh_state.current_func_id : ++global_func_counter;
    gcc_jit_type *uint32_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T);

    int scope_count = 0;
    for (gcc_gen_be_eh_region_t *r = eh_state.regions; r != NULL; r = r->next) {
        scope_count++;
    }
    if (scope_count <= 0) {
        scope_count = 1;
    }

    /* .xdata: UNWIND_INFO header (version 1 + UNW_FLAG_EHANDLER=0x08 or UNW_FLAG_UHANDLER=0x10)
       followed by N-entry C_SCOPE_TABLE { Count=N, { BeginAddress, EndAddress, HandlerAddress, JumpTarget } * N } */
    int xdata_word_count = 2 + (scope_count * 4);
    gcc_jit_type *xdata_arr_type = gcc_jit_context_new_array_type(ctx, NULL, uint32_type, xdata_word_count);
    char xdata_name[128];
    snprintf(xdata_name, sizeof(xdata_name), "__seh_xdata_%d", table_id);
    gcc_jit_lvalue *xdata_global = gcc_jit_context_new_global(
        ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, xdata_arr_type, xdata_name);
    if (p_gcc_jit_lvalue_set_link_section) {
#if defined(_WIN32) || defined(_WIN64)
        p_gcc_jit_lvalue_set_link_section(xdata_global, ".xdata");
#elif defined(__APPLE__) && defined(__MACH__)
        p_gcc_jit_lvalue_set_link_section(xdata_global, "__DATA,__seh_xdata");
#else
        p_gcc_jit_lvalue_set_link_section(xdata_global, ".xdata");
#endif
    }

    int unw_flags = has_except_handler ? 0x09 : 0x11; /* Version 1 | (UNW_FLAG_EHANDLER or UNW_FLAG_UHANDLER) */
    gcc_jit_rvalue **xdata_elems = (gcc_jit_rvalue **)malloc((size_t)xdata_word_count * sizeof(gcc_jit_rvalue *));
    if (!xdata_elems) return GCC_GEN_BE_ERROR_OOM;
    xdata_elems[0] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, unw_flags);
    xdata_elems[1] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, scope_count);
    int s_idx = 0;
    for (gcc_gen_be_eh_region_t *r = eh_state.regions; r != NULL && s_idx < scope_count; r = r->next, ++s_idx) {
        int base_rva = (s_idx * 16) + 4;
        int end_rva = base_rva + 12;
        int handler_rva = end_rva + 4 + (r->landing_pad_id > 0 ? r->landing_pad_id : 0);
        int jump_rva = has_except_handler ? (handler_rva + 4) : 0;
        xdata_elems[2 + (s_idx * 4) + 0] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, base_rva);
        xdata_elems[2 + (s_idx * 4) + 1] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, end_rva);
        xdata_elems[2 + (s_idx * 4) + 2] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, handler_rva);
        xdata_elems[2 + (s_idx * 4) + 3] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, jump_rva);
    }
    for (; s_idx < scope_count; ++s_idx) {
        int base_rva = (s_idx * 16) + 4;
        int end_rva = base_rva + 12;
        int handler_rva = end_rva + 4;
        int jump_rva = has_except_handler ? (handler_rva + 4) : 0;
        xdata_elems[2 + (s_idx * 4) + 0] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, base_rva);
        xdata_elems[2 + (s_idx * 4) + 1] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, end_rva);
        xdata_elems[2 + (s_idx * 4) + 2] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, handler_rva);
        xdata_elems[2 + (s_idx * 4) + 3] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, jump_rva);
    }
    gcc_jit_global_set_initializer_rvalue(
        xdata_global,
        gcc_jit_context_new_array_constructor(ctx, NULL, xdata_arr_type, (size_t)xdata_word_count, xdata_elems));
    free(xdata_elems);

    gcc_jit_block *active_seh_blk = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&active_seh_blk));
    if (active_seh_blk) {
        char seh_raw_buf[512];
        snprintf(
            seh_raw_buf, sizeof(seh_raw_buf),
            ".seh_proc __seh_fn_%d ; .seh_handler __gxx_personality_seh0, %s ; "
            ".seh_handlerdata ; .rva .Ltry_start_%d_1, .Ltry_end_%d_1, .Llanding_pad_%d_1 ; "
            ".seh_endproc",
            table_id,
            has_except_handler ? "@except, @unwind" : "@unwind",
            table_id, table_id, table_id);
        if (p_gcc_jit_context_add_top_level_asm) {
            p_gcc_jit_context_add_top_level_asm(ctx, NULL, seh_raw_buf);
        }
        char seh_asm_buf[560];
        snprintf(
            seh_asm_buf, sizeof(seh_asm_buf),
            "/* %s (table %d, scopes=%d) */",
            seh_raw_buf, table_id, scope_count);
        gcc_jit_extended_asm *seh_asm = gcc_jit_block_add_extended_asm(active_seh_blk, NULL, seh_asm_buf);
        gcc_jit_extended_asm_set_volatile_flag(seh_asm, 1);
    }

    /* .pdata: RUNTIME_FUNCTION { BeginAddress, EndAddress, UnwindInfoAddress } */
    gcc_jit_type *pdata_arr_type = gcc_jit_context_new_array_type(ctx, NULL, uint32_type, 3);
    char pdata_name[128];
    snprintf(pdata_name, sizeof(pdata_name), "__seh_pdata_%d", table_id);
    gcc_jit_lvalue *pdata_global = gcc_jit_context_new_global(
        ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, pdata_arr_type, pdata_name);
    if (p_gcc_jit_lvalue_set_link_section) {
#if defined(_WIN32) || defined(_WIN64)
        p_gcc_jit_lvalue_set_link_section(pdata_global, ".pdata");
#elif defined(__APPLE__) && defined(__MACH__)
        p_gcc_jit_lvalue_set_link_section(pdata_global, "__DATA,__seh_pdata");
#else
        p_gcc_jit_lvalue_set_link_section(pdata_global, ".pdata");
#endif
    }
    gcc_jit_rvalue *pdata_elems[3];
    pdata_elems[0] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, 0);
    pdata_elems[1] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, (scope_count * 16) + 16);
    pdata_elems[2] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, table_id);
    gcc_jit_global_set_initializer_rvalue(
        pdata_global,
        gcc_jit_context_new_array_constructor(ctx, NULL, pdata_arr_type, 3, pdata_elems));

    if (out_xdata_global) *out_xdata_global = xdata_global;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_noexcept_filter(
    gcc_jit_function *func,
    gcc_jit_block *lp_block,
    gcc_jit_block *end_block) GCC_GEN_BE_NOEXCEPT {
    if (!func || !lp_block || !end_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_lvalue *frame_exc = gcc_jit_function_new_local(func, NULL, void_ptr_type, "noexcept_caught_exc");
    gcc_jit_lvalue *frame_tinfo = gcc_jit_function_new_local(func, NULL, void_ptr_type, "noexcept_caught_tinfo");
    GCC_GEN_BE_CHECK(capture_landing_pad_registers(ctx, lp_block, frame_exc, frame_tinfo));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(NULL, lp_block));

    void *cached_term = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"_ZSt9terminatev", &cached_term));
    if (!cached_term) {
        gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        GCC_GEN_BE_CHECK(declare_eh_func(ctx, "_ZSt9terminatev", void_type, 0, NULL));
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"_ZSt9terminatev", &cached_term));
    }
    if (cached_term) {
        gcc_jit_block_add_eval(
            lp_block, NULL,
            gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_term, 0, NULL));
    }
    gcc_jit_block_end_with_jump(lp_block, NULL, end_block);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(end_block));
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_enforce_exception_spec(
    gcc_jit_function *func,
    an_exception_specification_ptr exc_spec,
    gcc_jit_block *lp_block,
    gcc_jit_block *end_block) GCC_GEN_BE_NOEXCEPT {
    if (!func || !exc_spec || !lp_block || !end_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    a_boolean is_noexcept = (exc_spec->is_noexcept && !exc_spec->throw_any) ? TRUE : FALSE;
    if (is_noexcept) {
        return gcc_gen_be_emit_noexcept_filter(func, lp_block, end_block);
    }

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_lvalue *frame_exc = gcc_jit_function_new_local(func, NULL, void_ptr_type, "spec_caught_exc");
    gcc_jit_lvalue *frame_tinfo = gcc_jit_function_new_local(func, NULL, void_ptr_type, "spec_caught_tinfo");
    GCC_GEN_BE_CHECK(capture_landing_pad_registers(ctx, lp_block, frame_exc, frame_tinfo));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(NULL, lp_block));

    gcc_jit_rvalue *exc_val = gcc_jit_lvalue_as_rvalue(frame_exc);
    gcc_jit_rvalue *tinfo_val = gcc_jit_lvalue_as_rvalue(frame_tinfo);

    {
        /* Dynamic exception specification throw(T1, T2, ...) */
        gcc_jit_block *unexp_block = gcc_jit_function_new_block(func, "unexpected_block");
        gcc_jit_block *curr_dispatch = lp_block;

        gcc_jit_function *cxa_type_match = NULL;
        GCC_GEN_BE_CHECK(declare_cxa_type_match(ctx, &cxa_type_match));

        an_exception_specification_type_ptr estp = exc_spec->variant.exception_specification_type_list;
        while (estp) {
            if (!estp->redundant) {
                /* Register negative filter entry in LSDA action table for allowed exception spec types */
                int filter_id = 0;
                GCC_GEN_BE_CHECK(get_type_filter_index(estp->type, &filter_id));
                GCC_GEN_BE_CHECK(append_sleb128(-(long)filter_id));
                GCC_GEN_BE_CHECK(append_sleb128(0));

                gcc_jit_block *match_block = gcc_jit_function_new_block(func, "spec_match");
                gcc_jit_block *next_dispatch = gcc_jit_function_new_block(func, "spec_next");

                gcc_jit_lvalue *tinfo_lval = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(estp->type, &tinfo_lval));
                gcc_jit_rvalue *catch_tinfo = gcc_jit_context_new_cast(
                    ctx, NULL, gcc_jit_lvalue_get_address(tinfo_lval, NULL), void_ptr_type);

                gcc_jit_rvalue *args[3] = { catch_tinfo, tinfo_val, exc_val };
                gcc_jit_rvalue *match_res = gcc_jit_context_new_call(ctx, NULL, cxa_type_match, 3, args);
                gcc_jit_lvalue *match_ptr = gcc_jit_function_new_local(func, NULL, void_ptr_type, "spec_match_ptr");
                gcc_jit_block_add_assignment(curr_dispatch, NULL, match_ptr, match_res);

                gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
                gcc_jit_rvalue *is_match = gcc_jit_context_new_comparison(
                    ctx, NULL, GCC_JIT_COMPARISON_NE, gcc_jit_lvalue_as_rvalue(match_ptr), null_ptr);
                gcc_jit_block_end_with_conditional(curr_dispatch, NULL, is_match, match_block, next_dispatch);

                GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(match_block));
                gcc_jit_block *after_resume = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(exc_val, &after_resume));
                if (after_resume) {
                    gcc_jit_block_end_with_jump(after_resume, NULL, end_block);
                }

                curr_dispatch = next_dispatch;
            }
            estp = estp->next;
        }

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(curr_dispatch));
        gcc_jit_block_end_with_jump(curr_dispatch, NULL, unexp_block);

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(unexp_block));
        void *cached_unexp = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_call_unexpected", &cached_unexp));
        if (!cached_unexp) {
            gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
            gcc_jit_type *p_unexp[] = { void_ptr_type };
            GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__cxa_call_unexpected", void_type, 1, p_unexp));
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_call_unexpected", &cached_unexp));
        }
        if (cached_unexp) {
            gcc_jit_block_add_eval(
                unexp_block, NULL,
                gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_unexp, 1, &exc_val));
        }
        gcc_jit_block_end_with_jump(unexp_block, NULL, end_block);
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(end_block));
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_routine_body(a_statement_ptr stmt, gcc_jit_function *func, an_exception_specification_ptr exc_spec) GCC_GEN_BE_NOEXCEPT {
    if (!stmt || !func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    
    a_boolean needs_enforcement = FALSE;
    
    if (exc_spec) {
        if (exc_spec->is_noexcept && !exc_spec->throw_any) {
            /* Only enforce if not noexcept(false) */
            needs_enforcement = TRUE;
        } else if (!exc_spec->is_noexcept && !exc_spec->throw_any) {
            /* Dynamic exception specification throw(A, B, ...) */
            needs_enforcement = TRUE;
        }
    }
    
    if (!needs_enforcement) {
        return gcc_gen_be_lower_statement(stmt, func);
    }
    
    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    
    int try_id = eh_state.next_try_id++;
    int lp_id = try_id; /* for simplicity */

    gcc_jit_function *pers_fn = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_register_personality_routine(func, &pers_fn));

    if (!eh_state.lp_start_emitted) {
        char buf[128];
        snprintf(buf, sizeof(buf), ".Llp_start_%d:\n", eh_state.current_func_id);
        gcc_jit_extended_asm *asm_lp = gcc_jit_block_add_extended_asm(cblock, NULL, buf);
        gcc_jit_extended_asm_set_volatile_flag(asm_lp, 1);
        eh_state.lp_start_emitted = TRUE;
    }
    
    char try_start_buf[128];
    snprintf(try_start_buf, sizeof(try_start_buf), ".Ltry_start_%d_%d:\n", eh_state.current_func_id, try_id);
    gcc_jit_extended_asm *asm_start = gcc_jit_block_add_extended_asm(cblock, NULL, try_start_buf);
    gcc_jit_extended_asm_set_volatile_flag(asm_start, 1);

    gcc_jit_block *try_body_block = gcc_jit_function_new_block(func, "try_body");
    gcc_jit_block *catch_dispatch_block = gcc_jit_function_new_block(func, "catch_dispatch");
    gcc_jit_block *end_try_block = gcc_jit_function_new_block(func, "end_try");

    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *eh_type = NULL;
    GCC_GEN_BE_CHECK(get_eh_frame_type(ctx, &eh_type));
    gcc_jit_struct *eh_struct = eh_type ? gcc_jit_type_is_struct(eh_type) : NULL;
    gcc_jit_lvalue *sjlj_frame = NULL;
    if (eh_type && eh_struct) {
        sjlj_frame = gcc_jit_function_new_local(func, NULL, eh_type, "spec_sjlj_frame");
        gcc_jit_lvalue *chain = NULL;
        GCC_GEN_BE_CHECK(get_eh_chain(ctx, &chain));

        /* sjlj_frame.next = *chain */
        gcc_jit_field *f_next = gcc_jit_struct_get_field(eh_struct, 0);
        gcc_jit_lvalue *sjlj_next = gcc_jit_lvalue_access_field(sjlj_frame, NULL, f_next);
        gcc_jit_block_add_assignment(cblock, NULL, sjlj_next, gcc_jit_lvalue_as_rvalue(chain));

        /* *chain = &sjlj_frame */
        gcc_jit_block_add_assignment(
            cblock, NULL, chain,
            gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(sjlj_frame, NULL), void_ptr_type));

        gcc_gen_be_context_t *be_state = NULL;
        if (gcc_gen_be_get_state(&be_state) == GCC_GEN_BE_SUCCESS && be_state && be_state->scope_stack_size > 0) {
            gcc_jit_lvalue **top_sc = (gcc_jit_lvalue **)be_state->scope_stack[be_state->scope_stack_size - 1];
            if (top_sc) top_sc[2] = sjlj_frame;
        }

        void *cached_setjmp = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", &cached_setjmp));
        if (!cached_setjmp) {
            gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            gcc_jit_param *jmpbuf_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "env");
            gcc_jit_function *setjmp_fn = gcc_jit_context_new_function(
                ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "setjmp", 1, &jmpbuf_param, 0);
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", setjmp_fn));
            cached_setjmp = setjmp_fn;
        }

        gcc_jit_field *f_jmpbuf = gcc_jit_struct_get_field(eh_struct, 1);
        gcc_jit_rvalue *jmpbuf_addr = gcc_jit_context_new_cast(
            ctx, NULL,
            gcc_jit_lvalue_get_address(gcc_jit_lvalue_access_field(sjlj_frame, NULL, f_jmpbuf), NULL),
            void_ptr_type);
        gcc_jit_rvalue *setjmp_call = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_setjmp, 1, &jmpbuf_addr);

        gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);
        gcc_jit_rvalue *is_zero = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, setjmp_call, zero);

        gcc_jit_block_end_with_conditional(cblock, NULL, is_zero, try_body_block, catch_dispatch_block);
    } else {
        gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        void *cached_chk = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__gcc_be_eh_check_unwind_state", &cached_chk));
        if (!cached_chk) {
            GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gcc_be_eh_check_unwind_state", int_type, 0, NULL));
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__gcc_be_eh_check_unwind_state", &cached_chk));
        }
        gcc_jit_rvalue *unwind_state = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_chk, 0, NULL);
        gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
        gcc_jit_rvalue *is_normal = gcc_jit_context_new_comparison(
            ctx, NULL, GCC_JIT_COMPARISON_EQ, unwind_state, zero);
        gcc_jit_block_end_with_conditional(cblock, NULL, is_normal, try_body_block, catch_dispatch_block);
    }

    /* Normal execution */
    GCC_GEN_BE_CHECK(gcc_gen_be_push_landing_pad(catch_dispatch_block));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(try_body_block));
    gcc_gen_be_error_t stmt_err = gcc_gen_be_lower_statement(stmt, func);
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    if (stmt_err != GCC_GEN_BE_SUCCESS) return stmt_err;

    gcc_jit_block *after_try = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_try));
    if (after_try) {
        if (sjlj_frame && eh_struct) {
            gcc_jit_lvalue *chain = NULL;
            GCC_GEN_BE_CHECK(get_eh_chain(ctx, &chain));
            gcc_jit_field *f_next = gcc_jit_struct_get_field(eh_struct, 0);
            gcc_jit_lvalue *sjlj_next = gcc_jit_lvalue_access_field(sjlj_frame, NULL, f_next);
            gcc_jit_block_add_assignment(after_try, NULL, chain, gcc_jit_lvalue_as_rvalue(sjlj_next));
        }
        char try_end_buf[128];
        snprintf(try_end_buf, sizeof(try_end_buf), ".Ltry_end_%d_%d:\n", eh_state.current_func_id, try_id);
        gcc_jit_extended_asm *asm_end = gcc_jit_block_add_extended_asm(after_try, NULL, try_end_buf);
        gcc_jit_extended_asm_set_volatile_flag(asm_end, 1);
        gcc_jit_block_end_with_jump(after_try, NULL, end_try_block);
    }

    /* Exception caught (Landing Pad) */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(catch_dispatch_block));
    
    char lp_buf[128];
    snprintf(lp_buf, sizeof(lp_buf), ".Llanding_pad_%d_%d:\n", eh_state.current_func_id, lp_id);
    gcc_jit_extended_asm *asm_lp = gcc_jit_block_add_extended_asm(catch_dispatch_block, NULL, lp_buf);
    gcc_jit_extended_asm_set_volatile_flag(asm_lp, 1);
    
    int action_idx = 0;
    {
        int action_offset = eh_state.action_table_len;
        GCC_GEN_BE_CHECK(append_sleb128(0)); /* filter = 0 */
        GCC_GEN_BE_CHECK(append_sleb128(0)); /* next = 0 */
        action_idx = action_offset + 1;
    }
    
    gcc_gen_be_eh_region_t *r = (gcc_gen_be_eh_region_t *)malloc(sizeof(gcc_gen_be_eh_region_t));
    if (!r) return GCC_GEN_BE_ERROR_OOM;
    r->try_id = try_id;
    r->landing_pad_id = lp_id;
    r->action_table_index = action_idx;
    r->next = eh_state.regions;
    eh_state.regions = r;
    
    return gcc_gen_be_enforce_exception_spec(func, exc_spec, catch_dispatch_block, end_try_block);
}

/**
 * @brief Emits an automatic DWARF cleanup-only landing pad (`action_table_index = 0`) for an RAII scope.
 * @details Records a call-site region in the LSDA table with `action_table_index = 0`, invokes all active
 * scope destructors in reverse construction order, and calls `_Unwind_Resume`.
 * @param[in] func The JIT function containing the scope.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
#ifndef EDG_GCC_BE_ISOLATED_TEST
    if (!exceptions_enabled) return GCC_GEN_BE_SUCCESS;
#endif
    gcc_gen_be_context_t *be_state = NULL;
    if (gcc_gen_be_get_state(&be_state) == GCC_GEN_BE_SUCCESS && be_state && be_state->scope_stack_size > 0) {
        gcc_jit_lvalue **top_sc = (gcc_jit_lvalue **)be_state->scope_stack[be_state->scope_stack_size - 1];
        if (top_sc && top_sc[2] != NULL) {
            return GCC_GEN_BE_SUCCESS;
        }
    }
    if (eh_state.current_func != func) {
        GCC_GEN_BE_CHECK(reset_eh_state(func));
    }

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (!cblock) return GCC_GEN_BE_ERROR_INVALID_STATE;

    int try_id = eh_state.next_try_id++;
    int lp_id = try_id;

    gcc_jit_function *pers_fn = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_register_personality_routine(func, &pers_fn));

    if (!eh_state.lp_start_emitted) {
        char buf[128];
        snprintf(buf, sizeof(buf), ".Llp_start_%d:", eh_state.current_func_id);
        gcc_jit_extended_asm *asm_lp = gcc_jit_block_add_extended_asm(cblock, NULL, buf);
        gcc_jit_extended_asm_set_volatile_flag(asm_lp, 1);
        eh_state.lp_start_emitted = TRUE;
    }

    char try_start_buf[128];
    snprintf(try_start_buf, sizeof(try_start_buf),
             ".Ltry_start_%d_%d:\n.Ltry_end_%d_%d:",
             eh_state.current_func_id, try_id, eh_state.current_func_id, try_id);
    gcc_jit_extended_asm *asm_start = gcc_jit_block_add_extended_asm(cblock, NULL, try_start_buf);
    gcc_jit_extended_asm_set_volatile_flag(asm_start, 1);

    gcc_jit_block *cont_block = gcc_jit_function_new_block(func, "raii_scope_cont");
    gcc_jit_block *cleanup_lp = gcc_jit_function_new_block(func, "raii_cleanup_lp");
    if (p_gcc_jit_block_add_cleanup) {
        p_gcc_jit_block_add_cleanup(cblock, NULL, cleanup_lp);
    }

    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *eh_type = NULL;
    GCC_GEN_BE_CHECK(get_eh_frame_type(ctx, &eh_type));
    gcc_jit_struct *eh_struct = eh_type ? gcc_jit_type_is_struct(eh_type) : NULL;
    if (eh_type && eh_struct) {
        gcc_jit_lvalue *sjlj_frame = gcc_jit_function_new_local(func, NULL, eh_type, "raii_sjlj_frame");
        gcc_jit_lvalue *chain = NULL;
        GCC_GEN_BE_CHECK(get_eh_chain(ctx, &chain));
        gcc_jit_field *f_next = gcc_jit_struct_get_field(eh_struct, 0);
        gcc_jit_lvalue *sjlj_next = gcc_jit_lvalue_access_field(sjlj_frame, NULL, f_next);
        gcc_jit_block_add_assignment(cblock, NULL, sjlj_next, gcc_jit_lvalue_as_rvalue(chain));
        gcc_jit_block_add_assignment(
            cblock, NULL, chain,
            gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(sjlj_frame, NULL), void_ptr_type));

        if (be_state && be_state->scope_stack_size > 0) {
            gcc_jit_lvalue **top_sc = (gcc_jit_lvalue **)be_state->scope_stack[be_state->scope_stack_size - 1];
            if (top_sc) top_sc[2] = sjlj_frame;
        }

        void *cached_setjmp = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", &cached_setjmp));
        if (!cached_setjmp) {
            gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            gcc_jit_param *jmpbuf_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "env");
            gcc_jit_function *setjmp_fn = gcc_jit_context_new_function(
                ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, "setjmp", 1, &jmpbuf_param, 0);
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"setjmp", setjmp_fn));
            cached_setjmp = setjmp_fn;
        }
        gcc_jit_field *f_jmpbuf = gcc_jit_struct_get_field(eh_struct, 1);
        gcc_jit_rvalue *jmpbuf_addr = gcc_jit_context_new_cast(
            ctx, NULL,
            gcc_jit_lvalue_get_address(gcc_jit_lvalue_access_field(sjlj_frame, NULL, f_jmpbuf), NULL),
            void_ptr_type);
        gcc_jit_rvalue *setjmp_call = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_setjmp, 1, &jmpbuf_addr);
        gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 0);
        gcc_jit_rvalue *is_zero = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, setjmp_call, zero);
        gcc_jit_block_end_with_conditional(cblock, NULL, is_zero, cont_block, cleanup_lp);
    } else {
        gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        void *cached_chk = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__gcc_be_eh_check_unwind_state", &cached_chk));
        if (!cached_chk) {
            GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gcc_be_eh_check_unwind_state", int_type, 0, NULL));
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__gcc_be_eh_check_unwind_state", &cached_chk));
        }
        gcc_jit_rvalue *unwind_state = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_chk, 0, NULL);
        gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
        gcc_jit_rvalue *cond = gcc_jit_context_new_comparison(
            ctx, NULL, GCC_JIT_COMPARISON_EQ, unwind_state, zero);
        gcc_jit_block_end_with_conditional(cblock, NULL, cond, cont_block, cleanup_lp);
    }

    char lp_buf[128];
    snprintf(lp_buf, sizeof(lp_buf), ".Llanding_pad_%d_%d:", eh_state.current_func_id, lp_id);
    gcc_jit_extended_asm *asm_lp = gcc_jit_block_add_extended_asm(cleanup_lp, NULL, lp_buf);
    gcc_jit_extended_asm_set_volatile_flag(asm_lp, 1);

    gcc_gen_be_eh_region_t *r = (gcc_gen_be_eh_region_t *)malloc(sizeof(gcc_gen_be_eh_region_t));
    if (!r) return GCC_GEN_BE_ERROR_OOM;
    r->try_id = try_id;
    r->landing_pad_id = lp_id;
    r->action_table_index = 0; /* Cleanup-only landing pad (or chained onto enclosing catch action) */
    if (eh_state.regions && eh_state.regions->action_table_index > 0) {
        int chained_idx = 0;
        GCC_GEN_BE_CHECK(gcc_gen_be_chain_cleanup_action(eh_state.regions->action_table_index, &chained_idx));
        r->action_table_index = chained_idx;
    }
    r->next = eh_state.regions;
    eh_state.regions = r;

    gcc_jit_lvalue *frame_exc = gcc_jit_function_new_local(func, NULL, void_ptr_type, "cleanup_exc");
    gcc_jit_lvalue *frame_tinfo = gcc_jit_function_new_local(func, NULL, void_ptr_type, "cleanup_tinfo");
    GCC_GEN_BE_CHECK(capture_landing_pad_registers(ctx, cleanup_lp, frame_exc, frame_tinfo));
    {
        an_object_lifetime_ptr parent_lt = NULL;
        if (be_state && be_state->scope_stack_size >= 2) {
            an_object_lifetime_ptr *prev_lt_ptr =
                (an_object_lifetime_ptr *)be_state->scope_stack[be_state->scope_stack_size - 2];
            parent_lt = prev_lt_ptr ? *prev_lt_ptr : NULL;
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_cleanups(parent_lt, cleanup_lp));
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cleanup_lp));
    gcc_jit_block *outer_lp = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&outer_lp));
    gcc_jit_block *after_resume = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(gcc_jit_lvalue_as_rvalue(frame_exc), &after_resume));
    if (after_resume) {
        gcc_jit_block_end_with_jump(after_resume, NULL, outer_lp ? outer_lp : cont_block);
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cont_block));
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_invoke_call(
    struct gcc_jit_context *ctx,
    struct gcc_jit_block *block,
    struct gcc_jit_function *fn,
    int num_args,
    struct gcc_jit_rvalue **args,
    struct gcc_jit_lvalue *result_lval,
    struct gcc_jit_block *normal_dest,
    struct gcc_jit_block *unwind_dest,
    struct gcc_jit_block **out_next_block) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !block || !fn || (num_args > 0 && !args) || !unwind_dest || !out_next_block) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_next_block = NULL;

    gcc_jit_function *parent_fn = gcc_jit_block_get_function(block);
    if (!normal_dest) {
        normal_dest = gcc_jit_function_new_block(parent_fn, "invoke_normal_cont");
    }

    gcc_jit_rvalue *call_rval = gcc_jit_context_new_call(ctx, NULL, fn, num_args, args);
    if (p_gcc_jit_block_end_with_invoke) {
        if (result_lval) {
            gcc_jit_block_add_assignment(block, NULL, result_lval, call_rval);
            call_rval = gcc_jit_context_new_call(ctx, NULL, fn, num_args, args);
        }
        p_gcc_jit_block_end_with_invoke(block, NULL, call_rval, normal_dest, unwind_dest);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(normal_dest));
        *out_next_block = normal_dest;
        return GCC_GEN_BE_SUCCESS;
    }
    if (result_lval) {
        gcc_jit_block_add_assignment(block, NULL, result_lval, call_rval);
    } else {
        gcc_jit_block_add_eval(block, NULL, call_rval);
    }

    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    void *cached_chk = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__gcc_be_eh_check_unwind_state", &cached_chk));
    if (!cached_chk) {
        GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gcc_be_eh_check_unwind_state", int_type, 0, NULL));
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__gcc_be_eh_check_unwind_state", &cached_chk));
    }
    gcc_jit_rvalue *unwind_state = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_chk, 0, NULL);
    gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
    gcc_jit_rvalue *is_normal = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, unwind_state, zero);
    gcc_jit_block_end_with_conditional(block, NULL, is_normal, normal_dest, unwind_dest);

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(normal_dest));
    *out_next_block = normal_dest;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_invoke_call_through_ptr(
    struct gcc_jit_context *ctx,
    struct gcc_jit_block *block,
    struct gcc_jit_rvalue *fn_ptr,
    int num_args,
    struct gcc_jit_rvalue **args,
    struct gcc_jit_lvalue *result_lval,
    struct gcc_jit_block *normal_dest,
    struct gcc_jit_block *unwind_dest,
    struct gcc_jit_block **out_next_block) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !block || !fn_ptr || (num_args > 0 && !args) || !unwind_dest || !out_next_block) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_next_block = NULL;

    gcc_jit_function *parent_fn = gcc_jit_block_get_function(block);
    if (!normal_dest) {
        normal_dest = gcc_jit_function_new_block(parent_fn, "invoke_ptr_normal_cont");
    }

    gcc_jit_rvalue *call_rval = gcc_jit_context_new_call_through_ptr(ctx, NULL, fn_ptr, num_args, args);
    if (p_gcc_jit_block_end_with_invoke) {
        if (result_lval) {
            gcc_jit_block_add_assignment(block, NULL, result_lval, call_rval);
            call_rval = gcc_jit_context_new_call_through_ptr(ctx, NULL, fn_ptr, num_args, args);
        }
        p_gcc_jit_block_end_with_invoke(block, NULL, call_rval, normal_dest, unwind_dest);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(normal_dest));
        *out_next_block = normal_dest;
        return GCC_GEN_BE_SUCCESS;
    }
    if (result_lval) {
        gcc_jit_block_add_assignment(block, NULL, result_lval, call_rval);
    } else {
        gcc_jit_block_add_eval(block, NULL, call_rval);
    }

    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    void *cached_chk = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__gcc_be_eh_check_unwind_state", &cached_chk));
    if (!cached_chk) {
        GCC_GEN_BE_CHECK(declare_eh_func(ctx, "__gcc_be_eh_check_unwind_state", int_type, 0, NULL));
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__gcc_be_eh_check_unwind_state", &cached_chk));
    }
    gcc_jit_rvalue *unwind_state = gcc_jit_context_new_call(ctx, NULL, (gcc_jit_function *)cached_chk, 0, NULL);
    gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
    gcc_jit_rvalue *is_normal = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, unwind_state, zero);
    gcc_jit_block_end_with_conditional(block, NULL, is_normal, normal_dest, unwind_dest);

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(normal_dest));
    *out_next_block = normal_dest;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_selector_filter_dispatch(
    struct gcc_jit_function *func,
    struct gcc_jit_block *dispatch_block,
    struct gcc_jit_rvalue *selector_rval,
    int filter_id,
    struct gcc_jit_block *match_block,
    struct gcc_jit_block **out_next_dispatch_block) GCC_GEN_BE_NOEXCEPT {
    if (!func || !dispatch_block || !selector_rval || !match_block || !out_next_dispatch_block) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_next_dispatch_block = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *sel_type = gcc_jit_rvalue_get_type(selector_rval);
    gcc_jit_block *next_dispatch = gcc_jit_function_new_block(func, "selector_next");
    gcc_jit_rvalue *cond = NULL;
    if (sel_type == int_type) {
        if (filter_id > 0) {
            gcc_jit_rvalue *expected = gcc_jit_context_new_rvalue_from_int(ctx, int_type, filter_id);
            cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, selector_rval, expected);
        } else if (filter_id < 0) {
            gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
            cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_LT, selector_rval, zero);
        } else {
            gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
            cond = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, zero, zero);
        }
    } else {
        gcc_jit_rvalue *expected_ptr = gcc_jit_context_new_rvalue_from_ptr(
            ctx, sel_type, (void *)(intptr_t)filter_id);
        cond = gcc_jit_context_new_comparison(
            ctx, NULL, (filter_id == 0) ? GCC_JIT_COMPARISON_NE : GCC_JIT_COMPARISON_EQ,
            selector_rval, expected_ptr);
    }
    gcc_jit_block_end_with_conditional(dispatch_block, NULL, cond, match_block, next_dispatch);
    *out_next_dispatch_block = next_dispatch;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_partial_array_cleanup(
    struct gcc_jit_function *func,
    struct gcc_jit_lvalue *base_ptr_lval,
    struct gcc_jit_lvalue *index_lval,
    a_type_ptr elem_type,
    a_routine_ptr elem_dtor,
    struct gcc_jit_block **out_cleanup_lp) GCC_GEN_BE_NOEXCEPT {
    if (!func || !base_ptr_lval || !index_lval || !elem_type || !elem_dtor || !out_cleanup_lp) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_cleanup_lp = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_block *saved_block = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_block));

    gcc_jit_function *dtor_fn = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(elem_dtor, &dtor_fn));

    gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *elem_jit_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(elem_type, &elem_jit_type));
    gcc_jit_type *elem_ptr_type = gcc_jit_type_get_pointer(elem_jit_type);

    gcc_jit_block *lp_entry = gcc_jit_function_new_block(func, "arr_partial_cleanup_lp");
    gcc_jit_block *loop_cond = gcc_jit_function_new_block(func, "arr_partial_cleanup_cond");
    gcc_jit_block *loop_body = gcc_jit_function_new_block(func, "arr_partial_cleanup_body");
    gcc_jit_block *loop_done = gcc_jit_function_new_block(func, "arr_partial_cleanup_done");

    gcc_jit_lvalue *caught_exc = gcc_jit_function_new_local(func, NULL, void_ptr_type, "arr_cleanup_exc");
    gcc_jit_lvalue *caught_tinfo = gcc_jit_function_new_local(func, NULL, void_ptr_type, "arr_cleanup_tinfo");
    GCC_GEN_BE_CHECK(capture_landing_pad_registers(ctx, lp_entry, caught_exc, caught_tinfo));
    gcc_jit_block_end_with_jump(lp_entry, NULL, loop_cond);

    gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, size_t_type, 0);
    gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, size_t_type, 1);
    gcc_jit_rvalue *has_more = gcc_jit_context_new_comparison(
        ctx, NULL, GCC_JIT_COMPARISON_GT, gcc_jit_lvalue_as_rvalue(index_lval), zero);
    gcc_jit_block_end_with_conditional(loop_cond, NULL, has_more, loop_body, loop_done);

    gcc_jit_block_add_assignment_op(loop_body, NULL, index_lval, GCC_JIT_BINARY_OP_MINUS, one);
    if (dtor_fn) {
        gcc_jit_rvalue *base_ptr = gcc_jit_context_new_cast(
            ctx, NULL, gcc_jit_lvalue_as_rvalue(base_ptr_lval), elem_ptr_type);
        gcc_jit_lvalue *elem_lval = gcc_jit_context_new_array_access(
            ctx, NULL, base_ptr, gcc_jit_lvalue_as_rvalue(index_lval));
        gcc_jit_rvalue *elem_addr = gcc_jit_lvalue_get_address(elem_lval, NULL);
        gcc_jit_param *p0 = gcc_jit_function_get_param(dtor_fn, 0);
        gcc_jit_type *p0_type = p0 ? gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(p0)) : void_ptr_type;
        gcc_jit_rvalue *dtor_arg = gcc_jit_context_new_cast(ctx, NULL, elem_addr, p0_type);
        gcc_jit_block_add_eval(loop_body, NULL, gcc_jit_context_new_call(ctx, NULL, dtor_fn, 1, &dtor_arg));
    }
    gcc_jit_block_end_with_jump(loop_body, NULL, loop_cond);

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(loop_done));
    gcc_jit_block *outer_lp = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&outer_lp));
    gcc_jit_block *after_resume = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(gcc_jit_lvalue_as_rvalue(caught_exc), &after_resume));
    if (after_resume) {
        gcc_jit_block_end_with_jump(after_resume, NULL, outer_lp ? outer_lp : loop_done);
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_block));
    *out_cleanup_lp = lp_entry;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_subobject_unwind_cleanup(
    struct gcc_jit_function *func,
    struct gcc_jit_rvalue *this_ptr,
    long subobject_offset,
    a_routine_ptr subobject_dtor,
    struct gcc_jit_block **out_cleanup_lp) GCC_GEN_BE_NOEXCEPT {
    if (!func || !this_ptr || !subobject_dtor || !out_cleanup_lp) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_cleanup_lp = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_block *saved_block = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_block));

    gcc_jit_function *dtor_fn = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(subobject_dtor, &dtor_fn));

    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
    gcc_jit_type *char_ptr_type = gcc_jit_type_get_pointer(char_type);
    gcc_jit_type *ptrdiff_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);

    gcc_jit_block *subobj_lp = gcc_jit_function_new_block(func, "subobject_unwind_cleanup_lp");
    gcc_jit_lvalue *caught_exc = gcc_jit_function_new_local(func, NULL, void_ptr_type, "subobj_cleanup_exc");
    gcc_jit_lvalue *caught_tinfo = gcc_jit_function_new_local(func, NULL, void_ptr_type, "subobj_cleanup_tinfo");
    GCC_GEN_BE_CHECK(capture_landing_pad_registers(ctx, subobj_lp, caught_exc, caught_tinfo));

    gcc_jit_rvalue *adj_ptr = this_ptr;
    if (subobject_offset != 0) {
        gcc_jit_rvalue *char_base = gcc_jit_context_new_cast(ctx, NULL, this_ptr, char_ptr_type);
        gcc_jit_rvalue *off_rval = gcc_jit_context_new_rvalue_from_long(ctx, ptrdiff_type, subobject_offset);
        adj_ptr = gcc_jit_lvalue_get_address(
            gcc_jit_context_new_array_access(ctx, NULL, char_base, off_rval), NULL);
    }

    if (dtor_fn) {
        gcc_jit_param *p0 = gcc_jit_function_get_param(dtor_fn, 0);
        gcc_jit_type *p0_type = p0 ? gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(p0)) : void_ptr_type;
        gcc_jit_rvalue *dtor_arg = gcc_jit_context_new_cast(ctx, NULL, adj_ptr, p0_type);
        gcc_jit_block_add_eval(subobj_lp, NULL, gcc_jit_context_new_call(ctx, NULL, dtor_fn, 1, &dtor_arg));
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(subobj_lp));
    gcc_jit_block *outer_lp = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&outer_lp));
    if (outer_lp) {
        gcc_jit_block_end_with_jump(subobj_lp, NULL, outer_lp);
    } else {
        gcc_jit_block *after_resume = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_unwind_resume(gcc_jit_lvalue_as_rvalue(caught_exc), &after_resume));
        if (after_resume) {
            gcc_jit_block_end_with_jump(after_resume, NULL, subobj_lp);
        }
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_block));
    *out_cleanup_lp = subobj_lp;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_format_lsda_asm_directives(
    int func_id,
    int try_id,
    int lp_id,
    int action_idx,
    char *out_buf,
    size_t buf_cap,
    size_t *out_len) GCC_GEN_BE_NOEXCEPT {
    if (!out_buf || !out_len || func_id < 0 || try_id < 0 || lp_id < 0) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_len = 0;
    int cfi_enc = (gcc_be_fPIC || gcc_be_fPIE) ? 0x9b : (gcc_be_shared ? 0x1b : 0x00);
    int written = snprintf(
        out_buf, buf_cap,
        ".cfi_personality 0x%02x, __gxx_personality_v0 ; "
        ".cfi_lsda 0x%02x, __gcc_except_table_%d ; "
        ".uleb128 (.Ltry_start_%d_%d - .Llp_start_%d) ; "
        ".uleb128 (.Ltry_end_%d_%d - .Ltry_start_%d_%d) ; "
        ".uleb128 (.Llanding_pad_%d_%d - .Llp_start_%d) ; "
        ".uleb128 %d",
        cfi_enc, cfi_enc, func_id,
        func_id, try_id, func_id,
        func_id, try_id, func_id, try_id,
        func_id, lp_id, func_id,
        action_idx > 0 ? action_idx : 0);
    if (written < 0 || (size_t)written >= buf_cap) {
        return GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED;
    }
    *out_len = (size_t)written;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_chain_cleanup_action(
    int enclosing_action_index,
    int *out_chained_action_index) GCC_GEN_BE_NOEXCEPT {
    if (!out_chained_action_index) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_chained_action_index = 0;

    int cleanup_offset = eh_state.action_table_len;
    GCC_GEN_BE_CHECK(append_sleb128(0L)); /* filter = 0 (cleanup) */
    int next_field_pos = eh_state.action_table_len;
    long next_rel_offset = (enclosing_action_index > 0)
        ? (long)((enclosing_action_index - 1) - next_field_pos)
        : 0L;
    GCC_GEN_BE_CHECK(append_sleb128(next_rel_offset));
    *out_chained_action_index = cleanup_offset + 1;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_arm_ehabi_tables(
    struct gcc_jit_function *func,
    a_boolean use_compact_pr0,
    struct gcc_jit_lvalue **out_extab_global) GCC_GEN_BE_NOEXCEPT {
    if (out_extab_global) *out_extab_global = NULL;
    if (!func || !out_extab_global) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    GCC_GEN_BE_CHECK(gcc_gen_be_declare_eh_symbols(ctx));

    int table_id = eh_state.current_func_id > 0 ? eh_state.current_func_id : ++global_func_counter;
    gcc_jit_type *uint32_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T);

    /* .ARM.extab: compact model 0 (0x80b0b0b0: __aeabi_unwind_cpp_pr0 + 3x finish 0xb0)
       or generic model 1 (0x8101a8b0: __aeabi_unwind_cpp_pr1, 1 additional word, pop {r4-r11, lr} 0xa8, finish 0xb0,
       followed by frame opcode word 0x00b0b0b0) */
    int extab_words = use_compact_pr0 ? 2 : 3;
    gcc_jit_type *extab_arr_type = gcc_jit_context_new_array_type(ctx, NULL, uint32_type, extab_words);
    char extab_name[128];
    snprintf(extab_name, sizeof(extab_name), "__arm_extab_%d", table_id);
    gcc_jit_lvalue *extab_global = gcc_jit_context_new_global(
        ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, extab_arr_type, extab_name);
    if (p_gcc_jit_lvalue_set_link_section) {
#if defined(__APPLE__) && defined(__MACH__)
        p_gcc_jit_lvalue_set_link_section(extab_global, "__DATA,__arm_extab");
#else
        p_gcc_jit_lvalue_set_link_section(extab_global, ".ARM.extab");
#endif
    }
    int pr_header = use_compact_pr0 ? (int)0x80b0b0b0U : (int)0x8101a8b0U;
    gcc_jit_rvalue *extab_elems[3];
    extab_elems[0] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, pr_header);
    extab_elems[1] = gcc_jit_context_new_rvalue_from_int(
        ctx, uint32_type, use_compact_pr0 ? 0 : (int)0x04b0b0b0U);
    if (!use_compact_pr0) {
        extab_elems[2] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, 0);
    }
    gcc_jit_global_set_initializer_rvalue(
        extab_global,
        gcc_jit_context_new_array_constructor(ctx, NULL, extab_arr_type, (size_t)extab_words, extab_elems));

    gcc_jit_block *active_arm_blk = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&active_arm_blk));
    if (active_arm_blk) {
        char arm_raw_buf[350];
        snprintf(
            arm_raw_buf, sizeof(arm_raw_buf),
            ".fnstart ; .personality %s ; .handlerdata ; .word (__arm_extab_%d - .) & 0x7fffffff ; .fnend",
            use_compact_pr0 ? "__aeabi_unwind_cpp_pr0" : "__gxx_personality_v0",
            table_id);
        if (p_gcc_jit_context_add_top_level_asm) {
            p_gcc_jit_context_add_top_level_asm(ctx, NULL, arm_raw_buf);
        }
        char arm_asm_buf[384];
        snprintf(arm_asm_buf, sizeof(arm_asm_buf), "/* %s */", arm_raw_buf);
        gcc_jit_extended_asm *arm_asm = gcc_jit_block_add_extended_asm(active_arm_blk, NULL, arm_asm_buf);
        gcc_jit_extended_asm_set_volatile_flag(arm_asm, 1);
    }

    /* .ARM.exidx: { prel31 function offset, prel31 extab offset or inline compact entry } */
    gcc_jit_type *exidx_arr_type = gcc_jit_context_new_array_type(ctx, NULL, uint32_type, 2);
    char exidx_name[128];
    snprintf(exidx_name, sizeof(exidx_name), "__arm_exidx_%d", table_id);
    gcc_jit_lvalue *exidx_global = gcc_jit_context_new_global(
        ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, exidx_arr_type, exidx_name);
    if (p_gcc_jit_lvalue_set_link_section) {
#if defined(__APPLE__) && defined(__MACH__)
        p_gcc_jit_lvalue_set_link_section(exidx_global, "__DATA,__arm_exidx");
#else
        p_gcc_jit_lvalue_set_link_section(exidx_global, ".ARM.exidx");
#endif
    }
    int prel31_fn_off = (int)((unsigned int)table_id & 0x7fffffffU);
    int prel31_extab_val = use_compact_pr0
        ? pr_header
        : (int)(((unsigned int)table_id << 4) & 0x7fffffffU);
    gcc_jit_rvalue *exidx_elems[2];
    exidx_elems[0] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, prel31_fn_off);
    exidx_elems[1] = gcc_jit_context_new_rvalue_from_int(ctx, uint32_type, prel31_extab_val);
    gcc_jit_global_set_initializer_rvalue(
        exidx_global,
        gcc_jit_context_new_array_constructor(ctx, NULL, exidx_arr_type, 2, exidx_elems));

    *out_extab_global = extab_global;
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
