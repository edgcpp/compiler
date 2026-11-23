/**
 * @file gcc_gen_be_main.c
 * @brief Implementation of Compiler Driver Orchestration, Multi-TU & LTO Pipeline.
 * @details Handles Stage-0 driver invocation option mapping, recursive C++ namespace/class
 * scope traversal, canonical EDG mangler registration, TU-local global constructor emission,
 * and native C/C++ (`gcc`/`g++` + `libedg_gcc_eh_rt.a`) single-TU & multi-TU linking.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "fe_common.h"
#include "il.h"
#if NEED_NAME_MANGLING
#include "lower_name.h"
#endif /* NEED_NAME_MANGLING */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_read.h"
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#include "gcc_gen_be_main.h"
#include "gcc_gen_be_omp.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_lib_loader.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if !defined(_WIN32)
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <spawn.h>
#if defined(__APPLE__)
#include <crt_externs.h>
#else
/** @brief Process environment pointer vector required by POSIX `posix_spawnp`. */
extern char **environ;
#endif
#else
#include <io.h>
#include <process.h>
#endif

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

#if MAKE_FRONT_END_CALLABLE
/** @brief Tracks whether the GCC backend was initialized during the current front-end invocation. */
STATIC_THREAD a_boolean back_end_initialized;
#endif /* MAKE_FRONT_END_CALLABLE */

#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
/**
 * @brief Computes a 32-bit FNV-1a hash of `primary_source_file_name` for TU-local internal symbols.
 * @details Produces deterministic per-TU suffixes for internal global constructor functions.
 * @param[out] out_hash Pointer receiving the 32-bit hash value.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t compute_main_tu_hash(
    unsigned long *out_hash) GCC_GEN_BE_NOEXCEPT {
    if (!out_hash) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    unsigned long h = 2166136261UL;
    if (primary_source_file_name) {
        for (const unsigned char *p = (const unsigned char *)primary_source_file_name; *p != '\0'; ++p) {
            h ^= (unsigned long)(*p);
            h *= 16777619UL;
        }
    }
    *out_hash = h & 0xFFFFFFFFUL;
    return GCC_GEN_BE_SUCCESS;
}
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */

/**
 * @brief Checks and records a scope pointer in `gcc_gen_be_singleton_state.visited_scopes`.
 * @details Dynamically resizes `visited_scopes` via `realloc` and sets `*out_already_visited = TRUE`
 * if `scope` was already processed during the current traversal pass.
 * @param[in] scope Non-null `a_scope` pointer.
 * @param[out] out_already_visited Pointer receiving `TRUE` if already visited, `FALSE` if newly added.
 * @return `GCC_GEN_BE_SUCCESS` on success, `GCC_GEN_BE_ERROR_OOM` on allocation failure,
 * or `GCC_GEN_BE_ERROR_SCOPE_TRAVERSAL_FAILED` if arguments are invalid.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t mark_scope_visited(
    struct a_scope *scope,
    a_boolean *out_already_visited) GCC_GEN_BE_NOEXCEPT {
    if (!scope) {
        return GCC_GEN_BE_ERROR_SCOPE_TRAVERSAL_FAILED;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_already_visited = FALSE;
    gcc_gen_be_context_t *state = &gcc_gen_be_singleton_state;
    for (size_t i = 0; i < state->visited_scopes_count; ++i) {
        if (state->visited_scopes[i] == (void *)scope) {
            *out_already_visited = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    if (state->visited_scopes_count >= state->visited_scopes_capacity) {
        size_t new_cap = (state->visited_scopes_capacity == 0) ? 32 : (state->visited_scopes_capacity * 2);
        void **new_arr = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_realloc_buf(
            state->visited_scopes, new_cap * sizeof(void *), (void **)&new_arr));
        state->visited_scopes = new_arr;
        state->visited_scopes_capacity = new_cap;
    }
    state->visited_scopes[state->visited_scopes_count++] = (void *)scope;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_types_in_scope(
    struct a_scope *scope) GCC_GEN_BE_NOEXCEPT {
    a_boolean visited = FALSE;
    GCC_GEN_BE_CHECK(mark_scope_visited(scope, &visited));
    if (visited) return GCC_GEN_BE_SUCCESS;

    for (an_asm_entry_ptr aep = scope->asm_entries; aep != NULL; aep = aep->next) {
        if (aep->asm_string && aep->asm_string->kind == ck_string && aep->asm_string->variant.string.value) {
            gcc_jit_context *top_ctx = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&top_ctx));
            if (top_ctx) {
                gcc_jit_context_add_top_level_asm(top_ctx, NULL, (const char *)aep->asm_string->variant.string.value);
            }
        }
    }

    for (a_type_ptr tp = scope->types; tp != NULL; tp = tp->next) {
        if (ignore_type_in_back_end(tp)) continue;
        if (tp->source_corresp.name && strncmp(tp->source_corresp.name, "__SO__", 6) == 0) continue;
        if ((tp->kind == tk_class || tp->kind == tk_struct || tp->kind == tk_union)) {
            if (tp->variant.class_struct_union.is_prototype_instantiation ||
                (tp->variant.class_struct_union.is_template_class && !tp->variant.class_struct_union.is_specialized)) {
                continue;
            }
            if (il_header.function_def_table == NULL &&
                (tp->kind == tk_class || tp->kind == tk_struct) &&
                tp->variant.class_struct_union.any_virtual_functions) {
                void *cached_vt = NULL;
                GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, tp, &cached_vt));
                if (!cached_vt) {
                    struct gcc_jit_lvalue *vtable_lval = NULL;
                    GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtable(tp, &vtable_lval));
                    if (tp->variant.class_struct_union.any_virtual_base_classes) {
                        struct gcc_jit_lvalue *vtt_lval = NULL;
                        GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtt(tp, &vtt_lval));
                    }
                }
                struct gcc_jit_lvalue *tinfo_lval = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(tp, &tinfo_lval));
            }
            a_class_type_supplement_ptr ctsp = tp->variant.class_struct_union.extra_info;
            if (ctsp && ctsp->assoc_scope) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_types_in_scope(ctsp->assoc_scope));
            }
        }
    }

    for (a_namespace_ptr ns = scope->namespaces; ns != NULL; ns = ns->next) {
        if (!ns->is_namespace_alias && ns->variant.assoc_scope) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_types_in_scope(ns->variant.assoc_scope));
        }
    }

    for (a_scope_ptr sub = scope->scopes; sub != NULL; sub = sub->next) {
        if (sub->kind == sck_file || sub->kind == sck_namespace ||
            sub->kind == sck_namespace_extension || sub->kind == sck_class_struct_union) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_types_in_scope(sub));
        }
    }

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_variables_in_scope(
    struct a_scope *scope) GCC_GEN_BE_NOEXCEPT {
    a_boolean visited = FALSE;
    GCC_GEN_BE_CHECK(mark_scope_visited(scope, &visited));
    if (visited) return GCC_GEN_BE_SUCCESS;

    a_boolean is_primary = (scope == il_header.primary_scope) ? TRUE : FALSE;
    for (a_variable_ptr v = scope->variables; v != NULL; v = v->next) {
        if (ignore_variable_in_back_end(v)) continue;
        if (v->is_parameter || v->is_pack || v->is_handler_param ||
            (v->is_template_variable && !v->is_specialized) ||
            (v->type != NULL && type_is_nonreal(v->type))) continue;
        if (v->storage_class == sc_static && !v->source_corresp.name && v->init_kind == initk_static) continue;
        if (!is_primary && v->storage_class == sc_extern && v->init_kind == initk_none) continue;
        struct gcc_jit_lvalue *lval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(v, &lval));
    }

    for (a_type_ptr tp = scope->types; tp != NULL; tp = tp->next) {
        if (ignore_type_in_back_end(tp)) continue;
        if (tp->kind == tk_class || tp->kind == tk_struct || tp->kind == tk_union) {
            if (tp->variant.class_struct_union.is_prototype_instantiation ||
                (tp->variant.class_struct_union.is_template_class && !tp->variant.class_struct_union.is_specialized)) {
                continue;
            }
            a_class_type_supplement_ptr ctsp = tp->variant.class_struct_union.extra_info;
            if (ctsp && ctsp->assoc_scope) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_variables_in_scope(ctsp->assoc_scope));
            }
        }
    }

    for (a_namespace_ptr ns = scope->namespaces; ns != NULL; ns = ns->next) {
        if (!ns->is_namespace_alias && ns->variant.assoc_scope) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_variables_in_scope(ns->variant.assoc_scope));
        }
    }

    for (a_scope_ptr sub = scope->scopes; sub != NULL; sub = sub->next) {
        if (sub->kind == sck_file || sub->kind == sck_namespace ||
            sub->kind == sck_namespace_extension || sub->kind == sck_class_struct_union) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_variables_in_scope(sub));
        }
    }

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_routines_in_scope(
    struct a_scope *scope) GCC_GEN_BE_NOEXCEPT {
    a_boolean visited = FALSE;
    GCC_GEN_BE_CHECK(mark_scope_visited(scope, &visited));
    if (visited) return GCC_GEN_BE_SUCCESS;

    a_boolean is_primary = (scope == il_header.primary_scope) ? TRUE : FALSE;
    for (a_routine_ptr r = scope->routines; r != NULL; r = r->next) {
        if (ignore_routine_in_back_end(r)) continue;
        if (r->is_deleted || (r->pure_virtual && r->function_def_number == NULL_function_def_number)) continue;
        if ((!is_primary || il_header.function_def_table != NULL) &&
            r->function_def_number == NULL_function_def_number) {
            continue;
        }
#if MAINTAIN_NEEDED_FLAGS || BACK_END_IS_GCC_GEN_BE
        if (il_header.function_def_table != NULL && !r->definition_needed && !r->keep_definition_in_il &&
            ((r->is_inline && (r->storage_class == sc_static || !is_primary)) ||
             (!is_primary && !r->source_corresp.referenced))) {
            continue;
        }
#endif
        struct gcc_jit_function *func = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(r, &func));
        if (r->function_def_number != NULL_function_def_number) {
            a_scope_ptr fscope = scope_for_routine(r);
            if (!fscope && il_header.function_def_table) {
                fscope = il_header.function_def_table[r->function_def_number].scope;
            }
            if (fscope) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_routines_in_scope(fscope));
            }
        }
    }

    for (a_type_ptr tp = scope->types; tp != NULL; tp = tp->next) {
        if (ignore_type_in_back_end(tp)) continue;
        if (tp->kind == tk_class || tp->kind == tk_struct || tp->kind == tk_union) {
            if (tp->variant.class_struct_union.is_prototype_instantiation ||
                (tp->variant.class_struct_union.is_template_class && !tp->variant.class_struct_union.is_specialized)) {
                continue;
            }
            a_class_type_supplement_ptr ctsp = tp->variant.class_struct_union.extra_info;
            if (ctsp && ctsp->assoc_scope) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_routines_in_scope(ctsp->assoc_scope));
            }
        }
    }

    for (a_namespace_ptr ns = scope->namespaces; ns != NULL; ns = ns->next) {
        if (!ns->is_namespace_alias && ns->variant.assoc_scope) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_routines_in_scope(ns->variant.assoc_scope));
        }
    }

    for (a_scope_ptr sub = scope->scopes; sub != NULL; sub = sub->next) {
        if (sub->kind != sck_func_prototype && !sub->is_expansion_prototype) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_routines_in_scope(sub));
        }
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Early initialization for the GCC backend.
 *
 * @details This function is called by the front end before command-line options
 * are processed. It ensures that the back end can be cleaned up properly if needed.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code if `libgccjit` loading fails.
 */
gcc_gen_be_error_t gcc_gen_be_early_init(void) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
#if MAKE_FRONT_END_CALLABLE
    back_end_initialized = FALSE;
#endif

    gcc_gen_be_error_t err;
#if defined(_WIN32)
    err = load_libgccjit_windows();
#elif defined(__unix__) || defined(__APPLE__) || defined(__FreeBSD__)
    err = load_libgccjit_posix();
#else
    err = GCC_GEN_BE_SUCCESS;
#endif
    return err;
}

/**
 * @brief Synthesizes TU-local global constructor and destructor registration blocks.
 * @details Lowers any `__sti__*` dynamic initializer routines into the TU-local global constructor
 * block and terminates `state->global_ctor_block` when active.
 * @return GCC_GEN_BE_SUCCESS on success, or an appropriate error code.
 */
static gcc_gen_be_error_t gcc_gen_be_synthesize_global_ctors_dtors(void) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

    /* Invoke any EDG-generated __sti__* file-scope dynamic init routines when present */
    if (il_header.primary_scope != NULL) {
        for (a_routine_ptr r = il_header.primary_scope->routines; r != NULL; r = r->next) {
            if (r->source_corresp.name != NULL &&
                strncmp(r->source_corresp.name, "__sti__", 7) == 0 &&
                r->function_def_number != NULL_function_def_number) {
                struct gcc_jit_function *sti_func = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(r, &sti_func));
                if (sti_func) {
                    struct gcc_jit_block *cblock = NULL;
                    GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblock));
                    gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, sti_func, 0, NULL));
                }
            }
        }
    }

    /* Terminate __edg_global_init block if it was created during global variable lowering */
    gcc_gen_be_context_t *state = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_state(&state));
    if (state->global_ctor_block) {
        gcc_jit_block_end_with_void_return(state->global_ctor_block, NULL);
        state->global_ctor_block = NULL;
    }

#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
    /* Check if we have file-scope dynamic init routines */
    if (il_header.file_scope_dynamic_init_routines != NULL) {
        /* Collect all entries and stably sort by ascending init_priority (with default 0 -> 65535 last) */
        int num_routines = 0;
        for (a_routine_list_entry_ptr rlep = il_header.file_scope_dynamic_init_routines; rlep != NULL; rlep = rlep->next) {
            if (rlep->routine) num_routines++;
        }

        if (num_routines > 0) {
            a_routine_ptr *sorted_routs = (a_routine_ptr *)malloc((size_t)num_routines * sizeof(a_routine_ptr));
            int *sorted_prios = (int *)malloc((size_t)num_routines * sizeof(int));
            if (!sorted_routs || !sorted_prios) {
                free(sorted_routs);
                free(sorted_prios);
                return GCC_GEN_BE_ERROR_OOM;
            }

            int idx = 0;
            for (a_routine_list_entry_ptr rlep = il_header.file_scope_dynamic_init_routines; rlep != NULL; rlep = rlep->next) {
                if (!rlep->routine) continue;
                sorted_routs[idx] = rlep->routine;
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED && DO_IL_LOWERING
                int raw_p = (int)rlep->routine->init_priority;
                sorted_prios[idx] = (raw_p > 0) ? raw_p : 65535;
#else
                sorted_prios[idx] = 65535;
#endif
                idx++;
            }

            /* Stable insertion sort by ascending priority */
            for (int i = 1; i < num_routines; ++i) {
                a_routine_ptr key_r = sorted_routs[i];
                int key_p = sorted_prios[i];
                int j = i - 1;
                while (j >= 0 && sorted_prios[j] > key_p) {
                    sorted_routs[j + 1] = sorted_routs[j];
                    sorted_prios[j + 1] = sorted_prios[j];
                    j--;
                }
                sorted_routs[j + 1] = key_r;
                sorted_prios[j + 1] = key_p;
            }

            unsigned long tu_hash = 0UL;
            GCC_GEN_BE_CHECK(compute_main_tu_hash(&tu_hash));
            unsigned long seq_base = ++gcc_gen_be_singleton_state.tu_sequence_id;

            int current_priority = -1;
            struct gcc_jit_function *current_ctor_func = NULL;
            struct gcc_jit_block *current_block = NULL;

            for (int i = 0; i < num_routines; ++i) {
                int prio = sorted_prios[i];
                if (current_priority != prio || current_ctor_func == NULL) {
                    if (current_block != NULL) {
                        gcc_jit_block_end_with_void_return(current_block, NULL);
                    }
                    current_priority = prio;

                    char func_name[160];
                    if (prio == 65535) {
                        snprintf(func_name, sizeof(func_name),
                                 "__static_initialization_and_destruction_0_%08lx_%lu",
                                 tu_hash, seq_base);
                    } else {
                        snprintf(func_name, sizeof(func_name),
                                 "__static_initialization_and_destruction_%d_%08lx_%lu",
                                 prio, tu_hash, seq_base);
                    }

                    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                    current_ctor_func = gcc_jit_context_new_function(
                        ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, func_name, 0, NULL, 0);
                    if (!current_ctor_func) {
                        free(sorted_routs);
                        free(sorted_prios);
                        return GCC_GEN_BE_ERROR_INTERNAL;
                    }

                    if (p_gcc_jit_function_add_attribute) {
                        p_gcc_jit_function_add_attribute(current_ctor_func, GCC_JIT_FN_ATTRIBUTE_USED);
                    }
#if defined(__APPLE__) && defined(__MACH__)
                    {
                        char asm_buf[256];
                        snprintf(asm_buf, sizeof(asm_buf),
                                 ".section __DATA,__mod_init_func,mod_init_funcs\n"
                                 ".p2align 3\n"
                                 ".quad _%s\n"
                                 ".text\n",
                                 func_name);
                        gcc_jit_context_add_top_level_asm(ctx, NULL, asm_buf);
                    }
#else
                    {
                        gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                        gcc_jit_rvalue *func_addr = gcc_jit_function_get_address(current_ctor_func, NULL);
                        gcc_jit_rvalue *func_ptr = gcc_jit_context_new_cast(ctx, NULL, func_addr, void_ptr_type);
                        char entry_name[192];
                        snprintf(entry_name, sizeof(entry_name), "%s_entry", func_name);
                        gcc_jit_lvalue *init_entry = gcc_jit_context_new_global(
                            ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, void_ptr_type, entry_name);
                        gcc_jit_global_set_initializer_rvalue(init_entry, func_ptr);
                        if (p_gcc_jit_lvalue_set_link_section) {
#if defined(_WIN32)
                            p_gcc_jit_lvalue_set_link_section(init_entry, ".CRT$XCU");
#else
                            p_gcc_jit_lvalue_set_link_section(init_entry, ".init_array");
#endif
                        }
                    }
#endif

                    current_block = gcc_jit_function_new_block(current_ctor_func, "entry");
                }

                struct gcc_jit_function *init_func = NULL;
                gcc_gen_be_error_t l_err = gcc_gen_be_lower_function_decl(sorted_routs[i], &init_func);
                if (l_err != GCC_GEN_BE_SUCCESS) {
                    free(sorted_routs);
                    free(sorted_prios);
                    return l_err;
                }
                if (init_func) {
                    gcc_jit_block_add_eval(current_block, NULL, gcc_jit_context_new_call(ctx, NULL, init_func, 0, NULL));
                }
            }

            if (current_block != NULL) {
                gcc_jit_block_end_with_void_return(current_block, NULL);
            }
            free(sorted_routs);
            free(sorted_prios);
        }
    }

#if !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
    if (il_header.thread_local_dynamic_init_routines != NULL) {
        unsigned long tu_hash = 0UL;
        GCC_GEN_BE_CHECK(compute_main_tu_hash(&tu_hash));
        unsigned long seq_tls = ++gcc_gen_be_singleton_state.tu_sequence_id;
        char tls_name[96];
        snprintf(tls_name, sizeof(tls_name), "__tls_init_%08lx_%lu", tu_hash, seq_tls);

        gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_function *tls_ctor_func = gcc_jit_context_new_function(
            ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, tls_name, 0, NULL, 0);
            
        if (tls_ctor_func) {
            gcc_jit_block *tls_block = gcc_jit_function_new_block(tls_ctor_func, "entry");
            
            a_routine_list_entry_ptr rlep;
            for (rlep = il_header.thread_local_dynamic_init_routines; rlep != NULL; rlep = rlep->next) {
                struct gcc_jit_function *init_func = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(rlep->routine, &init_func));
                if (init_func) {
                    gcc_jit_block_add_eval(tls_block, NULL, gcc_jit_context_new_call(ctx, NULL, init_func, 0, NULL));
                }
            }
            gcc_jit_block_end_with_void_return(tls_block, NULL);
        }
    }
#endif
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be(void) GCC_GEN_BE_NOEXCEPT {
#if MAKE_FRONT_END_CALLABLE
    back_end_initialized = TRUE;
#endif
    GCC_GEN_BE_CHECK(cache_clear_all());
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    if (!skip_il_read && f_il_output != NULL) {
        primary_source_file_name = NULL;
        il_read(f_il_output);
        if (il_header.primary_source_file != NULL) {
            primary_source_file_name = il_header.primary_source_file->file_name;
        }
    }
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

#if NEED_NAME_MANGLING
    gcc_gen_be_singleton_state.canonical_mangle_func = get_mangled_function_name;
    gcc_gen_be_singleton_state.canonical_mangle_var = get_mangled_variable_name;
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo = mangled_typeinfo_name;
    gcc_gen_be_singleton_state.canonical_mangle_typeinfo_str = mangled_typeinfo_string_name;
    gcc_gen_be_singleton_state.canonical_mangle_vtt = mangled_virtual_table_table_name;
#endif /* NEED_NAME_MANGLING */

    /* Ensure optional libgccjit function pointers are loaded. */
    GCC_GEN_BE_CHECK(gcc_gen_be_early_init());

    /* Initialize gcc_jit_context with active command-line options and target configuration. */
    GCC_GEN_BE_CHECK(gcc_gen_be_init());
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_compile_unit_debug_info());

    /* Recursively traverse C++ namespaces, classes, and sub-scopes in three deterministic passes. */
    gcc_gen_be_singleton_state.has_primary_il_scope = (il_header.primary_scope != NULL) ? TRUE : FALSE;
    if (il_header.primary_scope) {
        GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(il_header.primary_scope));
        gcc_gen_be_singleton_state.visited_scopes_count = 0;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_variables_in_scope(il_header.primary_scope));

        gcc_gen_be_singleton_state.visited_scopes_count = 0;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_routines_in_scope(il_header.primary_scope));

        gcc_gen_be_singleton_state.visited_scopes_count = 0;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_types_in_scope(il_header.primary_scope));
    }

    /* Trigger static constructor / destructor registration blocks. */
    GCC_GEN_BE_CHECK(gcc_gen_be_synthesize_global_ctors_dtors());

    /* Automatically emit _ZGIW<module_name> when the TU imports or defines C++20 modules */
    if (il_header.imported_modules != NULL) {
        const char *tu_mod_name = NULL;
        for (a_module_import_decl_ptr midp = il_header.imported_modules; midp != NULL; midp = midp->next) {
            if (midp->module_info) {
                if (midp->module_info->kind == mk_unit && midp->module_info->variant.unit.name) {
                    tu_mod_name = midp->module_info->variant.unit.name;
                    break;
                } else if (midp->module_info->kind == mk_unit_partition &&
                           midp->module_info->variant.unit_partition.name) {
                    tu_mod_name = midp->module_info->variant.unit_partition.name;
                    break;
                }
            }
        }
        if (tu_mod_name) {
            struct gcc_jit_function *mod_init_fn = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_emit_module_initializer(tu_mod_name, &mod_init_fn));
        }
    }
    
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

    /* Trigger compile-to-file (.o, .s, dynamic library, or executable) via libgccjit. */
    const char *output_kind = ".o";
    const char *ext = ".o";
    if (gcc_be_assemble_only) {
        output_kind = ".s";
        ext = ".s";
    } else if (gcc_be_compile_only) {
        output_kind = ".o";
        ext = ".o";
    } else if (gcc_be_shared) {
        output_kind = ".so";
        ext = ".so";
    } else {
        output_kind = ".out";
        ext = "";
    }

    char default_out_name[512];
    const char *final_out_path = gcc_be_output_file_name;
    if (!final_out_path && primary_source_file_name) {
        if (ext[0] == '\0') {
            snprintf(default_out_name, sizeof(default_out_name), "a.out");
        } else {
            const char *dot = strrchr(primary_source_file_name, '.');
            if (dot && dot != primary_source_file_name) {
                int len = (int)(dot - primary_source_file_name);
                if (len > 500) len = 500;
                snprintf(default_out_name, sizeof(default_out_name), "%.*s%s", len, primary_source_file_name, ext);
            } else {
                snprintf(default_out_name, sizeof(default_out_name), "%s%s", primary_source_file_name, ext);
            }
        }
        final_out_path = default_out_name;
    }

    if (final_out_path) {
        if (!gcc_be_compile_only && !gcc_be_assemble_only &&
            gcc_gen_be_singleton_state.lto_mode == 0) {
            size_t tcap = strlen(final_out_path) + 32;
            char *tmp_obj = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_alloc(tcap, (void **)&tmp_obj));
            snprintf(tmp_obj, tcap, "%s.edg_tmp.o", final_out_path);
            gcc_gen_be_error_t oerr = gcc_gen_be_emit_artifact(ctx, ".o", tmp_obj);
            if (oerr != GCC_GEN_BE_SUCCESS) {
                int rm_rc = remove(tmp_obj);
                rm_rc ^= rm_rc;
                free(tmp_obj);
                return oerr;
            }
            const char *single_obj[1] = { tmp_obj };
            const char *shared_argv[1] = { "-shared" };
            gcc_gen_be_error_t lerr = gcc_gen_be_link_multi_tu_artifacts(
                single_obj, 1, final_out_path,
                gcc_be_shared ? 1 : 0,
                gcc_be_shared ? (char **)shared_argv : NULL);
            int rm_rc = remove(tmp_obj);
            rm_rc ^= rm_rc;
            free(tmp_obj);
            GCC_GEN_BE_CHECK(lerr);
        } else {
            GCC_GEN_BE_CHECK(gcc_gen_be_emit_artifact(ctx, output_kind, final_out_path));
        }
    }
    
    /* Perform graceful cleanup and release of JIT contexts and caches. */
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    return GCC_GEN_BE_SUCCESS;
}

#if !BACK_END_IS_LLVM_GEN_BE && !BACK_END_IS_C_GEN_BE && !BACK_END_IS_CP_GEN_BE
/**
 * @brief Hook called by the EDG frontend to execute the backend.
 *
 * @details This is the standard entry point expected by the EDG frontend architecture.
 * It marks the backend as initialized and invokes the main code generation routine.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an appropriate error code.
 */
gcc_gen_be_error_t back_end(void) GCC_GEN_BE_NOEXCEPT {
    return gcc_gen_be();
}
#endif /* !BACK_END_IS_LLVM_GEN_BE && !BACK_END_IS_C_GEN_BE && !BACK_END_IS_CP_GEN_BE */

/**
 * @brief Parses and applies command-line options to the given context.
 *
 * @details Propagates optimization levels (-O0 to -Ofast), architecture tuning
 * (-march, -mcpu, etc.), code models, position independence flags (-fPIC, etc.),
 * and debugging options (-g).
 *
 * @param ctx The libgccjit context.
 * @param argc Number of arguments.
 * @param argv Array of arguments.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_apply_command_line_options(gcc_jit_context *ctx, int argc, char **argv) GCC_GEN_BE_NOEXCEPT {
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());

    int opt_level = 0;

    for (int i = 0; i < argc; ++i) {
        const char *arg = argv[i];
        
        if (strncmp(arg, "-O", 2) == 0) {
            if (strcmp(arg, "-O0") == 0) opt_level = 0;
            else if (strcmp(arg, "-O1") == 0) opt_level = 1;
            else if (strcmp(arg, "-O2") == 0) opt_level = 2;
            else if (strcmp(arg, "-O3") == 0) opt_level = 3;
            else if (strcmp(arg, "-Os") == 0) opt_level = 2; /* Size optimization */
            else if (strcmp(arg, "-Ofast") == 0) opt_level = 3;
            gcc_jit_context_set_int_option(ctx, GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, opt_level);
        } else if (strncmp(arg, "-gdwarf-", 8) == 0 ||
                   strcmp(arg, "-g0") == 0 ||
                   strcmp(arg, "-g1") == 0 ||
                   strcmp(arg, "-g2") == 0 ||
                   strcmp(arg, "-g3") == 0 ||
                   strcmp(arg, "-ggdb") == 0) {
            if (strcmp(arg, "-g0") != 0) {
                gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DEBUGINFO, 1);
            }
            gcc_jit_context_add_command_line_option(ctx, arg);
            gcc_jit_context_add_command_line_option(ctx, "-fno-omit-frame-pointer");
            gcc_jit_context_add_command_line_option(ctx, "-fasynchronous-unwind-tables");
        } else if (strncmp(arg, "-g", 2) == 0) {
            gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DEBUGINFO, 1);
            gcc_jit_context_add_command_line_option(ctx, "-fno-omit-frame-pointer");
            gcc_jit_context_add_command_line_option(ctx, "-fasynchronous-unwind-tables");
        } else if (strncmp(arg, "-march=", 7) == 0 ||
                   strncmp(arg, "-mcpu=", 6) == 0 ||
                   strncmp(arg, "-mtune=", 7) == 0 ||
                   strncmp(arg, "-mfpu=", 6) == 0 ||
                   strncmp(arg, "-mcmodel=", 9) == 0 ||
                   strcmp(arg, "-fPIC") == 0 ||
                   strcmp(arg, "-fPIE") == 0 ||
                   strcmp(arg, "-fno-pie") == 0 ||
                   strcmp(arg, "-ffast-math") == 0 ||
                   strcmp(arg, "-fno-strict-aliasing") == 0 ||
                   strcmp(arg, "-fwrapv") == 0 ||
                   strcmp(arg, "-ftrapv") == 0 ||
                   strcmp(arg, "-fdata-sections") == 0 ||
                   strcmp(arg, "-ffunction-sections") == 0 ||
                   strncmp(arg, "-fvisibility=", 13) == 0 ||
                   strncmp(arg, "-fsanitize=", 11) == 0) {
            gcc_jit_context_add_command_line_option(ctx, arg);
        } else if (strncmp(arg, "-L", 2) == 0 ||
                   strncmp(arg, "-l", 2) == 0 ||
                   strncmp(arg, "-Wl,", 4) == 0 ||
                   strcmp(arg, "-shared") == 0 ||
                   strcmp(arg, "-static") == 0 ||
                   strcmp(arg, "-pie") == 0 ||
                   strcmp(arg, "-no-pie") == 0 ||
                   strcmp(arg, "-pthread") == 0 ||
                   strcmp(arg, "-rdynamic") == 0 ||
                   strncmp(arg, "-soname", 7) == 0) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
            gcc_jit_context_add_driver_option(ctx, arg);
#endif
        } else if (strcmp(arg, "-fopenmp") == 0) {
            gcc_gen_be_singleton_state.openmp_enabled = TRUE;
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
            gcc_jit_context_add_driver_option(ctx, "-fopenmp");
            gcc_jit_context_add_driver_option(ctx, "-lgomp");
#endif
        } else if (strncmp(arg, "-flto", 5) == 0) {
            gcc_jit_context_add_command_line_option(ctx, arg);
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
            gcc_jit_context_add_driver_option(ctx, arg);
#endif
        }
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Manages Multi-TU compilation by isolating contexts.
 *
 * @details Processes each input translation unit in an isolated `gcc_jit_context`,
 * applies command-line options (including `-flto`), and emits per-TU object artifacts.
 *
 * @param source_files Array of source files to process.
 * @param num_files Number of source files.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_compile_multi_tu(const char **source_files, int num_files, int argc, char **argv) GCC_GEN_BE_NOEXCEPT {
    if (!source_files || num_files <= 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_context *saved_ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&saved_ctx));
    a_const_char *saved_primary_file = primary_source_file_name;

    for (int i = 0; i < num_files; ++i) {
        if (!source_files[i]) {
            primary_source_file_name = saved_primary_file;
            return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        }
        gcc_jit_context *tu_ctx = (gcc_gen_be_step_oom_budget() == GCC_GEN_BE_SUCCESS)
                                      ? gcc_jit_context_acquire()
                                      : NULL;
        if (!tu_ctx) {
            primary_source_file_name = saved_primary_file;
            return GCC_GEN_BE_ERROR_OOM;
        }
        
        gcc_gen_be_error_t err = gcc_gen_be_apply_command_line_options(tu_ctx, argc, argv);
        if (err != GCC_GEN_BE_SUCCESS) {
            primary_source_file_name = saved_primary_file;
            gcc_jit_context_release(tu_ctx);
            return err;
        }

        primary_source_file_name = source_files[i];
        gcc_gen_be_context_t *tu_state = NULL;
        err = cache_clear_all();
        if (err == GCC_GEN_BE_SUCCESS) {
            err = gcc_gen_be_get_state(&tu_state);
        }
        if (err == GCC_GEN_BE_SUCCESS) {
            err = gcc_gen_be_set_context(tu_ctx);
        }
        if (err != GCC_GEN_BE_SUCCESS) {
            primary_source_file_name = saved_primary_file;
            gcc_jit_context_release(tu_ctx);
            return err;
        }
        tu_state->global_ctor_func = NULL;
        tu_state->global_ctor_block = NULL;
        tu_state->eh_state_ptr = NULL;

        /* Emit compile-unit location entry for the current TU when debug info is enabled */
        if (gcc_be_debug_info || gcc_gen_be_singleton_state.debug_level > 0) {
            gcc_jit_location *tu_loc = (gcc_gen_be_step_oom_budget() == GCC_GEN_BE_SUCCESS)
                ? gcc_jit_context_new_location(tu_ctx, primary_source_file_name, 1, 1)
                : NULL;
            if (!tu_loc) {
                primary_source_file_name = saved_primary_file;
                gcc_gen_be_error_t rest_err = gcc_gen_be_set_context(saved_ctx);
                gcc_jit_context_release(tu_ctx);
                return gcc_gen_be_first_error(rest_err, GCC_GEN_BE_ERROR_DWARF_EMISSION_FAILED);
            }
        }

#if IL_SHOULD_BE_WRITTEN_TO_FILE
        if (!skip_il_read && f_il_output != NULL) {
            il_read(f_il_output);
        }
#endif
        if (il_header.primary_scope) {
            gcc_gen_be_singleton_state.visited_scopes_count = 0;
            err = gcc_gen_be_lower_variables_in_scope(il_header.primary_scope);
            if (err == GCC_GEN_BE_SUCCESS) {
                gcc_gen_be_singleton_state.visited_scopes_count = 0;
                err = gcc_gen_be_lower_routines_in_scope(il_header.primary_scope);
            }
            if (err == GCC_GEN_BE_SUCCESS) {
                gcc_gen_be_singleton_state.visited_scopes_count = 0;
                err = gcc_gen_be_lower_types_in_scope(il_header.primary_scope);
            }
            if (err != GCC_GEN_BE_SUCCESS) {
                primary_source_file_name = saved_primary_file;
                err = gcc_gen_be_first_error(err, gcc_gen_be_set_context(saved_ctx));
                gcc_jit_context_release(tu_ctx);
                return err;
            }
        }
        err = gcc_gen_be_synthesize_global_ctors_dtors();
        err = gcc_gen_be_first_error(err, gcc_gen_be_set_context(saved_ctx));
        if (err != GCC_GEN_BE_SUCCESS) {
            primary_source_file_name = saved_primary_file;
            gcc_jit_context_release(tu_ctx);
            return err;
        }
        
        char obj_file[512];
        snprintf(obj_file, sizeof(obj_file), "%s.o", source_files[i]);
        err = gcc_gen_be_emit_artifact(tu_ctx, ".o", obj_file);
        gcc_jit_context_release(tu_ctx);
        if (err != GCC_GEN_BE_SUCCESS) {
            primary_source_file_name = saved_primary_file;
            for (int j = 0; j < i; ++j) {
                char prev_obj[512];
                snprintf(prev_obj, sizeof(prev_obj), "%s.o", source_files[j]);
                int rm_rc = remove(prev_obj);
                rm_rc ^= rm_rc;
            }
            return GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED;
        }
    }
    primary_source_file_name = saved_primary_file;

    if (!gcc_be_compile_only && !gcc_be_assemble_only && gcc_be_output_file_name) {
        const char **obj_list = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)num_files * sizeof(const char *), (void **)&obj_list));
        char (*obj_bufs)[512] = NULL;
        gcc_gen_be_error_t b_err = gcc_gen_be_alloc((size_t)num_files * sizeof(*obj_bufs), (void **)&obj_bufs);
        if (b_err != GCC_GEN_BE_SUCCESS) {
            free((void *)obj_list);
            return b_err;
        }
        for (int i = 0; i < num_files; ++i) {
            snprintf(obj_bufs[i], sizeof(obj_bufs[i]), "%s.o", source_files[i]);
            obj_list[i] = obj_bufs[i];
        }
        int link_status = 0;
        gcc_gen_be_error_t lerr = gcc_gen_be_link_multi_tu_objects(
            obj_list, num_files, gcc_be_output_file_name, argc, argv, &link_status);
        free((void *)obj_list);
        free(obj_bufs);
        GCC_GEN_BE_CHECK(lerr);
    }
    
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_link_multi_tu_objects(
    const char **obj_files,
    int num_objs,
    const char *output_path,
    int argc,
    char **argv,
    int *out_link_status) GCC_GEN_BE_NOEXCEPT {
    if (!out_link_status) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_link_status = -1;
    gcc_gen_be_error_t err = gcc_gen_be_link_multi_tu_artifacts(
        obj_files, num_objs, output_path, argc, argv);
    if (err != GCC_GEN_BE_SUCCESS) {
        return err;
    }
    *out_link_status = 0;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Resolves the path to `libedg_gcc_eh_rt.a` relative to the active binary or build directory.
 * @details Checks `EDG_GCC_EH_RT_LIB` (empty string disables discovery), `EDG_GCC_EH_RT_DIR`, and
 * `dladdr` sibling `../../lib/libedg_gcc_eh_rt.a` and `../lib/libedg_gcc_eh_rt.a` locations.
 * @param[out] out_buf Buffer receiving the resolved archive path (empty string if not found).
 * @param[in] buf_cap Capacity of `out_buf` in bytes.
 * @return `GCC_GEN_BE_SUCCESS` on success, `GCC_GEN_BE_ERROR_OOM` on injected fault, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t locate_eh_runtime_archive(
    char *out_buf,
    size_t buf_cap) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    out_buf[0] = '\0';

    const char *env_path = getenv("EDG_GCC_EH_RT_LIB");
#if !defined(_WIN32)
    if (env_path) {
        if (env_path[0] == '\0') {
            return GCC_GEN_BE_SUCCESS;
        }
        if (access(env_path, R_OK) == 0) {
            snprintf(out_buf, buf_cap, "%s", env_path);
            return GCC_GEN_BE_SUCCESS;
        }
    }
    char dir_buf[1024];
    dir_buf[0] = '\0';
    const char *env_dir = getenv("EDG_GCC_EH_RT_DIR");
    if (env_dir && env_dir[0] != '\0') {
        snprintf(dir_buf, sizeof(dir_buf), "%s", env_dir);
    } else {
        Dl_info dl_info;
        memset(&dl_info, 0, sizeof(dl_info));
        int dl_ok = dladdr((const void *)&gcc_gen_be_link_multi_tu_objects, &dl_info);
        dl_ok ^= dl_ok;
        const char *slash = strrchr(dl_info.dli_fname, '/');
        int dir_len = (int)(slash - dl_info.dli_fname);
        snprintf(dir_buf, sizeof(dir_buf), "%.*s", dir_len, dl_info.dli_fname);
    }
    const char *rel_cands[2] = {
        "../../lib/libedg_gcc_eh_rt.a",
        "../lib/libedg_gcc_eh_rt.a"
    };
    for (size_t i = 0; i < 2; ++i) {
        char cand[1088];
        snprintf(cand, sizeof(cand), "%s/%s", dir_buf, rel_cands[i]);
        if (access(cand, R_OK) == 0) {
            snprintf(out_buf, buf_cap, "%s", cand);
            return GCC_GEN_BE_SUCCESS;
        }
    }
#else
    if (env_path && _access(env_path, 4) == 0) {
        snprintf(out_buf, buf_cap, "%s", env_path);
    }
#endif
    return GCC_GEN_BE_SUCCESS;
}

#if defined(__APPLE__)
/**
 * @brief Appends `-L`, `-Wl,-rpath,`, and `-l<lib>` flags for a GCC runtime dylib on macOS.
 * @details Searches `EDG_GCC_LIB_DIR` (when set), `/usr/local/opt/gcc/lib/gcc/current`, and
 * `/opt/homebrew/opt/gcc/lib/gcc/current` for @p dylib_name.
 * @param[in,out] cmd Argument vector being constructed.
 * @param[in,out] io_k Current write index in @p cmd.
 * @param[in,out] owned Array tracking heap-allocated flag strings to free after linking.
 * @param[in,out] io_owned Current write index in @p owned.
 * @param[in] dylib_name Dylib filename to probe (e.g. `"libgomp.dylib"` or `"libatomic.dylib"`).
 * @param[in] link_flag Linker library flag to append (e.g. `"-lgomp"` or `"-latomic"`).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on allocation failure.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t append_apple_gcc_runtime_lib(
    char **cmd,
    size_t *io_k,
    char **owned,
    size_t *io_owned,
    const char *dylib_name,
    const char *link_flag) GCC_GEN_BE_NOEXCEPT {
    const char *env_dir = getenv("EDG_GCC_LIB_DIR");
    const char *dirs[3] = {
        (env_dir && env_dir[0] != '\0') ? env_dir : "/nonexistent_edg_gcc_dir",
        "/usr/local/opt/gcc/lib/gcc/current",
        "/opt/homebrew/opt/gcc/lib/gcc/current"
    };
    size_t max_dirs = (env_dir && strcmp(env_dir, "NONE") == 0) ? 0U : 3U;
    for (size_t i = 0; i < max_dirs; ++i) {
        char probe[1024];
        snprintf(probe, sizeof(probe), "%s/%s", dirs[i], dylib_name);
        if (access(probe, R_OK) == 0) {
            size_t lcap = strlen(dirs[i]) + 8;
            size_t rcap = strlen(dirs[i]) + 16;
            char *lflag = NULL;
            char *rflag = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_alloc(lcap, (void **)&lflag));
            gcc_gen_be_error_t rerr = gcc_gen_be_alloc(rcap, (void **)&rflag);
            if (rerr != GCC_GEN_BE_SUCCESS) {
                free(lflag);
                return rerr;
            }
            snprintf(lflag, lcap, "-L%s", dirs[i]);
            snprintf(rflag, rcap, "-Wl,-rpath,%s", dirs[i]);
            owned[(*io_owned)++] = lflag;
            owned[(*io_owned)++] = rflag;
            cmd[(*io_k)++] = lflag;
            cmd[(*io_k)++] = rflag;
            cmd[(*io_k)++] = (char *)link_flag;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}
#endif /* __APPLE__ */

gcc_gen_be_error_t gcc_gen_be_link_multi_tu_artifacts(
    const char **obj_files,
    int num_objs,
    const char *output_path,
    int argc,
    char **argv) GCC_GEN_BE_NOEXCEPT {
    if (!obj_files || num_objs <= 0 || !output_path || output_path[0] == '\0') {
        return GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED;
    }
    a_boolean all_objs_exist = TRUE;
    for (int i = 0; i < num_objs; ++i) {
        if (!obj_files[i] || obj_files[i][0] == '\0') {
            return GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED;
        }
#if !defined(_WIN32)
        if (access(obj_files[i], R_OK) != 0) {
            all_objs_exist = FALSE;
        }
#endif
    }

    a_boolean is_shared = gcc_be_shared ? TRUE : FALSE;
    a_boolean has_lto = (gcc_gen_be_singleton_state.lto_mode != 0) ? TRUE : FALSE;
    for (int i = 0; i < argc; ++i) {
        if (argv && argv[i]) {
            if (strcmp(argv[i], "-shared") == 0) is_shared = TRUE;
            if (strncmp(argv[i], "-flto", 5) == 0) has_lto = TRUE;
        }
    }

    char eh_rt_path[1024];
    GCC_GEN_BE_CHECK(locate_eh_runtime_archive(eh_rt_path, sizeof(eh_rt_path)));

#if !defined(_WIN32)
    /* Use native C (`cc`/`gcc`) or C++ (`c++`/`g++` + libedg_gcc_eh_rt.a) driver when linking real .o files without GCC GIMPLE LTO */
    if (all_objs_exist && !has_lto) {
        gcc_gen_be_context_t *state = &gcc_gen_be_singleton_state;
        size_t max_args = (size_t)num_objs + (size_t)(argc > 0 ? argc : 0) +
            state->driver_lib_dirs_count + state->driver_libs_count + (2U * state->driver_ldflags_count) + 32;
        char **cmd = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_alloc(max_args * sizeof(char *), (void **)&cmd));
        memset(cmd, 0, max_args * sizeof(char *));
        char **owned = NULL;
        gcc_gen_be_error_t own_err = gcc_gen_be_alloc(max_args * sizeof(char *), (void **)&owned);
        if (own_err != GCC_GEN_BE_SUCCESS) {
            free(cmd);
            return own_err;
        }
        memset(owned, 0, max_args * sizeof(char *));
        size_t k = 0;
        size_t n_owned = 0;
        gcc_gen_be_error_t build_err = GCC_GEN_BE_SUCCESS;
#if defined(__APPLE__)
        const char *driver_bin = C_mode() ? "cc" : "c++";
#else
        const char *driver_bin = C_mode() ? "gcc" : "g++";
#endif
        cmd[k++] = (char *)driver_bin;
#if !defined(__APPLE__)
        cmd[k++] = (char *)"-fno-use-linker-plugin";
#endif
        if (is_shared) cmd[k++] = (char *)"-shared";
        if (gcc_be_static) cmd[k++] = (char *)"-static";
#if !defined(__APPLE__)
        if (gcc_be_fPIE && !is_shared) cmd[k++] = (char *)"-pie";
#endif
        if (state->pthread_enabled) cmd[k++] = (char *)"-pthread";
        if (state->openmp_enabled) {
#if !defined(__APPLE__)
            cmd[k++] = (char *)"-fopenmp";
#endif
        }
        if (state->sanitizers & (1U << 0)) cmd[k++] = (char *)"-fsanitize=address";
        if (state->sanitizers & (1U << 1)) cmd[k++] = (char *)"-fsanitize=undefined";
        if (state->sanitizers & (1U << 2)) cmd[k++] = (char *)"-fsanitize=thread";
        if (state->sanitizers & (1U << 4)) cmd[k++] = (char *)"-fsanitize=leak";

        cmd[k++] = (char *)"-o";
        cmd[k++] = (char *)output_path;

        for (int i = 0; i < num_objs; ++i) {
            cmd[k++] = (char *)obj_files[i];
        }
        if (!C_mode() && eh_rt_path[0] != '\0') {
            cmd[k++] = eh_rt_path;
        }
        for (int i = 0; i < argc; ++i) {
            if (argv && argv[i]) {
                if (strcmp(argv[i], "-shared") == 0) continue;
                cmd[k++] = argv[i];
            }
        }
        for (size_t i = 0; i < state->driver_lib_dirs_count; ++i) {
            if (state->driver_lib_dirs[i]) {
                size_t lcap = strlen(state->driver_lib_dirs[i]) + 8;
                char *s = NULL;
                GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_alloc(lcap, (void **)&s), build_err, native_link_cleanup);
                snprintf(s, lcap, "-L%s", state->driver_lib_dirs[i]);
                owned[n_owned++] = s;
                cmd[k++] = s;
            }
        }
        for (size_t i = 0; i < state->driver_libs_count; ++i) {
            if (state->driver_libs[i]) {
                size_t lcap = strlen(state->driver_libs[i]) + 8;
                char *s = NULL;
                GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_alloc(lcap, (void **)&s), build_err, native_link_cleanup);
                snprintf(s, lcap, "-l%s", state->driver_libs[i]);
                owned[n_owned++] = s;
                cmd[k++] = s;
            }
        }
        for (size_t i = 0; i < state->driver_ldflags_count; ++i) {
            const char *larg = state->driver_ldflags[i];
            if (larg && larg[0] != '\0') {
                if (strncmp(larg, "-fuse-ld=", 9) == 0 ||
                    strncmp(larg, "-F", 2) == 0 ||
                    strncmp(larg, "-Wl,", 4) == 0 ||
                    strcmp(larg, "-Xlinker") == 0) {
                    cmd[k++] = (char *)larg;
                } else {
                    cmd[k++] = (char *)"-Xlinker";
                    cmd[k++] = (char *)larg;
                }
            }
        }
#if defined(__APPLE__)
        if (state->openmp_enabled) {
            GCC_GEN_BE_CHECK_CLEANUP(
                append_apple_gcc_runtime_lib(cmd, &k, owned, &n_owned, "libgomp.dylib", "-lgomp"),
                build_err, native_link_cleanup);
        }
        if (state->needs_libatomic) {
            GCC_GEN_BE_CHECK_CLEANUP(
                append_apple_gcc_runtime_lib(cmd, &k, owned, &n_owned, "libatomic.dylib", "-latomic"),
                build_err, native_link_cleanup);
        }
#else
        if (state->openmp_enabled) {
            cmd[k++] = (char *)"-lgomp";
        }
        if (state->needs_libatomic) {
            cmd[k++] = (char *)"-latomic";
        }
#endif
        cmd[k] = NULL;

        {
            const char *override_driver = getenv("EDG_GCC_DRIVER_BIN");
            const char *spawn_bin = (override_driver && override_driver[0] != '\0') ? override_driver : driver_bin;
#if defined(__APPLE__)
            char **envp = *_NSGetEnviron();
#else
            char **envp = environ;
#endif
            pid_t pid = -1;
            int status = -1;
            int spawn_rc = posix_spawnp(&pid, spawn_bin, NULL, NULL, cmd, envp);
            if (spawn_rc == 0) {
                pid_t wpid = waitpid(pid, &status, 0);
                wpid ^= wpid;
            }
            build_err = (spawn_rc == 0 && status == 0)
                            ? GCC_GEN_BE_SUCCESS
                            : GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED;
        }

native_link_cleanup:;
        for (size_t i = 0; i < n_owned; ++i) {
            free(owned[i]);
        }
        free(owned);
        free(cmd);
        return build_err;
    }
#endif

    gcc_jit_context *link_ctx = (gcc_gen_be_step_oom_budget() == GCC_GEN_BE_SUCCESS)
                                    ? gcc_jit_context_acquire()
                                    : NULL;
    if (!link_ctx) return GCC_GEN_BE_ERROR_OOM;

    for (int i = 0; i < argc; ++i) {
        if (argv && argv[i]) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
            gcc_jit_context_add_driver_option(link_ctx, argv[i]);
#endif
        }
    }
    for (int i = 0; i < num_objs; ++i) {
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
        gcc_jit_context_add_driver_option(link_ctx, obj_files[i]);
#endif
    }
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
    if (!C_mode() && eh_rt_path[0] != '\0') {
        gcc_jit_context_add_driver_option(link_ctx, eh_rt_path);
    }
    if (gcc_gen_be_singleton_state.openmp_enabled) {
        gcc_jit_context_add_driver_option(link_ctx, "-lgomp");
        gcc_jit_context_add_driver_option(link_ctx, "-lpthread");
    }
    if (gcc_gen_be_singleton_state.pthread_enabled) {
        gcc_jit_context_add_driver_option(link_ctx, "-pthread");
    }
#if defined(__APPLE__)
    gcc_jit_context_add_driver_option(link_ctx, "-nodefaultlibs");
    gcc_jit_context_add_driver_option(link_ctx, "-lSystem");
#endif
    if (!C_mode()) {
#if defined(__APPLE__)
        gcc_jit_context_add_driver_option(link_ctx, "-lc++");
#else
        gcc_jit_context_add_driver_option(link_ctx, "-lstdc++");
        gcc_jit_context_add_driver_option(link_ctx, "-lgcc_s");
#endif
        gcc_jit_context_add_driver_option(link_ctx, "-lm");
    }
#endif

    const char *out_kind = is_shared ? ".so" : ".out";
    gcc_gen_be_error_t err = gcc_gen_be_emit_artifact(link_ctx, out_kind, output_path);
    gcc_jit_context_release(link_ctx);
    return err;
}

/**
 * @brief Emits the final output artifact (assembler, object, shared library, executable).
 *
 * @details Invokes `gcc_jit_context_compile_to_file` and checks `gcc_jit_context_get_first_error`
 * to detect any compilation failure reported by `libgccjit`.
 *
 * @param ctx The context.
 * @param output_kind Target kind string (e.g. "asm", "obj", "dll", "exe").
 * @param output_path Target file path.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_COMPILATION_FAILED on error.
 */
gcc_gen_be_error_t gcc_gen_be_emit_artifact(gcc_jit_context *ctx, const char *output_kind, const char *output_path) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !output_kind || !output_path) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());

    enum gcc_jit_output_kind kind = GCC_JIT_OUTPUT_KIND_OBJECT_FILE;
    if (strcmp(output_kind, "asm") == 0 || strcmp(output_kind, ".s") == 0) {
        kind = GCC_JIT_OUTPUT_KIND_ASSEMBLER;
    } else if (strcmp(output_kind, "obj") == 0 || strcmp(output_kind, ".o") == 0) {
        kind = GCC_JIT_OUTPUT_KIND_OBJECT_FILE;
    } else if (strcmp(output_kind, "dll") == 0 || strcmp(output_kind, ".so") == 0 || strcmp(output_kind, ".dylib") == 0) {
        kind = GCC_JIT_OUTPUT_KIND_DYNAMIC_LIBRARY;
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
        gcc_jit_context_add_driver_option(ctx, "-fno-use-linker-plugin");
#endif
    } else if (strcmp(output_kind, "exe") == 0 || strcmp(output_kind, ".out") == 0) {
        kind = GCC_JIT_OUTPUT_KIND_EXECUTABLE;
#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option
        gcc_jit_context_add_driver_option(ctx, "-fno-use-linker-plugin");
#endif
    }

    remove(output_path);
    gcc_jit_context_compile_to_file(ctx, kind, output_path);
    const char *first_err = gcc_jit_context_get_first_error(ctx);
    int out_missing = (access(output_path, F_OK) != 0);
    int has_jit_err = (first_err != NULL);
    if ((has_jit_err | out_missing) != 0) {
        return GCC_GEN_BE_ERROR_COMPILATION_FAILED;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Formats an Itanium ABI C++20 module initializer symbol (`_ZGIW...`) into @p buf.
 * @details Encodes dot-separated module segments as `<len><seg>` and colon-separated partition
 * segments with `WP<len><seg>` so no raw `.` or `:` characters appear in assembler identifiers.
 * @param[in] mod_name Non-empty C++20 module or partition name.
 * @param[out] buf Destination buffer receiving the mangled `_ZGIW` symbol name.
 * @param[in] cap Capacity of @p buf in bytes.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on injected fault.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t format_module_init_symbol(
    const char *mod_name,
    char *buf,
    size_t cap) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    size_t pos = (size_t)snprintf(buf, cap, "_ZGIW");
    const char *p = mod_name;
    while (*p != '\0' && pos + 16 < cap) {
        const char *sep = p;
        while (*sep != '\0' && *sep != '.' && *sep != ':') {
            ++sep;
        }
        size_t seg_len = (size_t)(sep - p);
        pos += (size_t)snprintf(buf + pos, cap - pos, "%zu%.*s", seg_len, (int)seg_len, p);
        if (*sep == '\0') break;
        if (*sep == ':') {
            pos += (size_t)snprintf(buf + pos, cap - pos, "WP");
        }
        p = sep + 1;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Synthesizes an Itanium ABI C++20 module initializer function (`_ZGIW<module_name>`).
 * @details Emits an exported module initializer function guarded by an internal idempotency boolean
 * flag (`_ZGIW<module_name>__guard`) so imported module initializers execute at most once.
 * @param[in] module_name The C++20 module name (e.g., `"math"` or `"std.core"` or `"mod:part"`).
 * @param[out] out_init_fn Pointer to receive the synthesized `_ZGIW` `gcc_jit_function`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_module_initializer(
    const char *module_name,
    struct gcc_jit_function **out_init_fn) GCC_GEN_BE_NOEXCEPT {
    if (!module_name || module_name[0] == '\0' || !out_init_fn) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_init_fn = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    char fn_name[256];
    char guard_name[272];
    GCC_GEN_BE_CHECK(format_module_init_symbol(module_name, fn_name, sizeof(fn_name)));
    snprintf(guard_name, sizeof(guard_name), "%s__guard", fn_name);

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *uchar_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
    gcc_jit_lvalue *guard_var = gcc_jit_context_new_global(
        ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, uchar_type, guard_name);
    gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, uchar_type, 0);
    gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, uchar_type, 1);
    gcc_jit_global_set_initializer_rvalue(guard_var, zero);

    gcc_jit_function *init_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, fn_name, 0, NULL, 0);
    gcc_jit_block *entry_blk = gcc_jit_function_new_block(init_fn, "entry");
    gcc_jit_block *run_blk = gcc_jit_function_new_block(init_fn, "run_init");
    gcc_jit_block *done_blk = gcc_jit_function_new_block(init_fn, "done");

    gcc_jit_rvalue *already_init = gcc_jit_context_new_comparison(
        ctx, NULL, GCC_JIT_COMPARISON_NE, gcc_jit_lvalue_as_rvalue(guard_var), zero);
    gcc_jit_block_end_with_conditional(entry_blk, NULL, already_init, done_blk, run_blk);
    gcc_jit_block_add_assignment(run_blk, NULL, guard_var, one);

    /* Call imported modules' _ZGIW<imported_module> initializers first */
    for (a_module_import_decl_ptr midp = il_header.imported_modules; midp != NULL; midp = midp->next) {
        const char *imp_name = NULL;
        if (midp->module_info) {
            if (midp->module_info->kind == mk_unit) {
                imp_name = midp->module_info->variant.unit.name;
            } else if (midp->module_info->kind == mk_unit_partition) {
                imp_name = midp->module_info->variant.unit_partition.name;
            }
        }
        if (imp_name && imp_name[0] != '\0' && strcmp(imp_name, module_name) != 0) {
            char imp_fn_name[256];
            GCC_GEN_BE_CHECK(format_module_init_symbol(imp_name, imp_fn_name, sizeof(imp_fn_name)));
            gcc_jit_function *imp_fn = gcc_jit_context_new_function(
                ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, imp_fn_name, 0, NULL, 0);
            gcc_jit_block_add_eval(run_blk, NULL, gcc_jit_context_new_call(ctx, NULL, imp_fn, 0, NULL));
        }
    }

    /* Invoke the TU's internal dynamic initializer if synthesized */
    gcc_gen_be_context_t *state = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_state(&state));
    if (state->global_ctor_func) {
        gcc_jit_block_add_eval(run_blk, NULL, gcc_jit_context_new_call(ctx, NULL, state->global_ctor_func, 0, NULL));
    }

    gcc_jit_block_end_with_jump(run_blk, NULL, done_blk);
    gcc_jit_block_end_with_void_return(done_blk, NULL);

    *out_init_fn = init_fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deep-copies a Stage-0 string list (`const char *const *`) into a heap-owned `char **` array.
 * @details Frees any previous contents of `*io_arr` before allocating and copying `count` strings.
 * @param[in,out] io_arr Destination pointer to heap-allocated `char **` array.
 * @param[in,out] io_count Destination element count.
 * @param[in] src Source string array (may be NULL when `count == 0`).
 * @param[in] count Number of strings in `src`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on allocation failure.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t copy_driver_string_array(
    char ***io_arr,
    size_t *io_count,
    const char *const *src,
    size_t count) GCC_GEN_BE_NOEXCEPT {
    if (*io_arr) {
        for (size_t i = 0; i < *io_count; ++i) {
            if ((*io_arr)[i]) free((*io_arr)[i]);
        }
        free(*io_arr);
        *io_arr = NULL;
    }
    *io_count = 0;
    if (!src || count == 0) return GCC_GEN_BE_SUCCESS;

    char **arr = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(count * sizeof(char *), (void **)&arr));
    memset(arr, 0, count * sizeof(char *));
    for (size_t i = 0; i < count; ++i) {
        if (src[i]) {
            size_t len = strlen(src[i]);
            gcc_gen_be_error_t s_err = gcc_gen_be_alloc(len + 1, (void **)&arr[i]);
            if (s_err != GCC_GEN_BE_SUCCESS) {
                for (size_t j = 0; j < i; ++j) free(arr[j]);
                free(arr);
                return s_err;
            }
            memcpy(arr[i], src[i], len + 1);
        }
    }
    *io_arr = arr;
    *io_count = count;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Applies normalized Stage-0 driver invocation options to the GCC backend state.
 * @details Validates @p opts and maps optimization, size optimization, debug verbosity, relocation,
 * LTO, symbol visibility, function/data sections, OpenMP, pthread, sanitizers, target CPU/code-model,
 * output path, `-L`, `-l`, and `-Wl,*` passthrough flags onto `gcc_gen_be_singleton_state`.
 * @param[in] opts Pointer to the normalized invocation options structure.
 * @return `GCC_GEN_BE_SUCCESS` on success, `GCC_GEN_BE_ERROR_OOM` on allocation failure,
 * or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if @p opts is NULL.
 */
gcc_gen_be_error_t gcc_gen_be_apply_invocation_options(
    const struct edg_be_invocation_options_t *opts) GCC_GEN_BE_NOEXCEPT {
    if (!opts) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    gcc_gen_be_context_t *state = &gcc_gen_be_singleton_state;

    gcc_be_opt_level = (int)opts->opt_level;
    state->opt_size_level = (int)opts->opt_size_level;
    state->debug_level = (int)opts->debug_level;
    gcc_be_debug_info = (opts->debug_level > 0) ? TRUE : FALSE;

    gcc_be_compile_only = (opts->artifact_kind == edg_output_artifact_kind_t::object_file) ? TRUE : FALSE;
    gcc_be_assemble_only = (opts->artifact_kind == edg_output_artifact_kind_t::assembly_file ||
                            opts->artifact_kind == edg_output_artifact_kind_t::ir_or_c_file) ? TRUE : FALSE;
    if (opts->artifact_kind == edg_output_artifact_kind_t::ir_or_c_file) {
        gcc_be_dump_gimple = TRUE;
    }
    gcc_be_shared = (opts->artifact_kind == edg_output_artifact_kind_t::shared_library) ? TRUE : FALSE;
    gcc_be_static = opts->static_link ? TRUE : FALSE;

    gcc_be_fno_pie = (opts->reloc_mode == edg_reloc_mode_t::static_no_pic) ? TRUE : FALSE;
    gcc_be_fPIC = (opts->reloc_mode == edg_reloc_mode_t::pic) ? TRUE : FALSE;
    gcc_be_fPIE = (opts->reloc_mode == edg_reloc_mode_t::pie) ? TRUE : FALSE;
    gcc_be_ffast_math = opts->fast_math ? TRUE : FALSE;

    state->lto_mode = (int)opts->lto_mode;
    state->symbol_visibility = (int)opts->symbol_visibility;
    state->function_sections = opts->function_sections ? TRUE : FALSE;
    state->data_sections = opts->data_sections ? TRUE : FALSE;
    state->openmp_enabled = opts->openmp_enabled ? TRUE : FALSE;
    state->pthread_enabled = opts->pthread_enabled ? TRUE : FALSE;
    unsigned int san_bits = 0U;
    if (opts->sanitize_address) san_bits |= (1U << 0);
    if (opts->sanitize_undefined) san_bits |= (1U << 1);
    if (opts->sanitize_thread) san_bits |= (1U << 2);
    if (opts->sanitize_memory) san_bits |= (1U << 3);
    state->sanitizers = san_bits;

    if (opts->mcpu && opts->mcpu[0] != '\0') {
        gcc_be_mcpu = opts->mcpu;
    }
    if (opts->march && opts->march[0] != '\0') {
        gcc_be_march = opts->march;
    }
    if (opts->mcmodel && opts->mcmodel[0] != '\0') {
        gcc_be_mcmodel = opts->mcmodel;
    }
    if (opts->output_file_path) {
        gcc_be_output_file_name = opts->output_file_path;
    }

    GCC_GEN_BE_CHECK(copy_driver_string_array(
        &state->driver_lib_dirs, &state->driver_lib_dirs_count,
        opts->library_search_dirs, opts->num_library_search_dirs));
    GCC_GEN_BE_CHECK(copy_driver_string_array(
        &state->driver_libs, &state->driver_libs_count,
        opts->libraries, opts->num_libraries));
    GCC_GEN_BE_CHECK(copy_driver_string_array(
        &state->driver_ldflags, &state->driver_ldflags_count,
        opts->linker_passthrough_args, opts->num_linker_passthrough_args));

    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
