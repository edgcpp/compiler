/**
 * @file gcc_gen_be_main.c
 * @brief Implementation of Compiler Driver Orchestration, Multi-TU & LTO Pipeline.
 *
 * This file handles parsing and propagating command-line options (optimization
 * levels, target tuning, code models, PIC, debugging options) to the libgccjit
 * context. It manages multi-TU orchestration and link-time optimization (LTO)
 * artifact generation (assembler, object, shared library, executable).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "fe_common.h"
#include "il.h"
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_read.h"
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#include "gcc_gen_be_main.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_lib_loader.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

#if MAKE_FRONT_END_CALLABLE
STATIC_THREAD a_boolean back_end_initialized;
#endif /* MAKE_FRONT_END_CALLABLE */

/**
 * @brief Early initialization for the GCC backend.
 *
 * @details This function is called by the front end before command-line options
 * are processed. It ensures that the back end can be cleaned up properly if needed.
 */
gcc_gen_be_error_t gcc_gen_be_early_init(void) GCC_GEN_BE_NOEXCEPT {
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
 * @brief Orchestrates the translation of the EDG AST into libgccjit constructs.
 *
 * @details This function initializes the libgccjit context, traverses the EDG IL
 * for the primary translation unit, processes variables and routines, generates
 * the output artifact, and cleans up the context.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an appropriate error code.
 */

static gcc_gen_be_error_t gcc_gen_be_synthesize_global_ctors_dtors(void) GCC_GEN_BE_NOEXCEPT {
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
    if (state && state->global_ctor_block) {
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

                    char func_name[128];
                    if (prio == 65535) {
                        snprintf(func_name, sizeof(func_name), "__static_initialization_and_destruction_0");
                    } else {
                        snprintf(func_name, sizeof(func_name), "__static_initialization_and_destruction_%d", prio);
                    }

                    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                    current_ctor_func = gcc_jit_context_new_function(
                        ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, func_name, 0, NULL, 0);
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
                                 ".quad _%s\n",
                                 func_name);
                        gcc_jit_context_add_top_level_asm(ctx, NULL, asm_buf);
                    }
#else
                    {
                        gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                        gcc_jit_rvalue *func_addr = gcc_jit_function_get_address(current_ctor_func, NULL);
                        gcc_jit_rvalue *func_ptr = gcc_jit_context_new_cast(ctx, NULL, func_addr, void_ptr_type);
                        char entry_name[160];
                        snprintf(entry_name, sizeof(entry_name), "%s_entry", func_name);
                        gcc_jit_lvalue *init_entry = gcc_jit_context_new_global(
                            ctx, NULL, GCC_JIT_GLOBAL_EXPORTED, void_ptr_type, entry_name);
                        gcc_jit_global_set_initializer_rvalue(init_entry, func_ptr);
                        if (p_gcc_jit_lvalue_set_link_section) {
                            p_gcc_jit_lvalue_set_link_section(init_entry, ".init_array");
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
        gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        gcc_jit_function *tls_ctor_func = gcc_jit_context_new_function(
            ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, "__tls_init", 0, NULL, 0);
            
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
    /* Ensure optional libgccjit function pointers are loaded. */
    GCC_GEN_BE_CHECK(gcc_gen_be_early_init());

    /* Initialize gcc_jit_context with active command-line options and target configuration. */
    GCC_GEN_BE_CHECK(gcc_gen_be_init());

    /* Iterate over primary translation unit global variables */
    if (il_header.primary_scope) {
        for (an_asm_entry_ptr aep = il_header.primary_scope->asm_entries; aep != NULL; aep = aep->next) {
            if (aep->asm_string && aep->asm_string->kind == ck_string && aep->asm_string->variant.string.value) {
                gcc_jit_context *top_ctx = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&top_ctx));
                if (top_ctx) {
                    gcc_jit_context_add_top_level_asm(top_ctx, NULL, (const char *)aep->asm_string->variant.string.value);
                }
            }
        }
        a_variable_ptr v;
        for (v = il_header.primary_scope->variables; v != NULL; v = v->next) {
            if (ignore_variable_in_back_end(v)) continue;
            if (v->storage_class == sc_static && !v->source_corresp.name && v->init_kind == initk_static) continue;
            /* Lower file-scope variables, TLS variables, external declarations, and initializers. */
            struct gcc_jit_lvalue *lval = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(v, &lval));
        }

        /* Iterate over translation unit routines */
        a_routine_ptr r;
        for (r = il_header.primary_scope->routines; r != NULL; r = r->next) {
            if (ignore_routine_in_back_end(r)) continue;
            /* Lower function prototypes, attributes, and statement bodies for defined routines. */
            struct gcc_jit_function *func = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(r, &func));
        }

        /* Automatically emit _ZTV, _ZTT, and _ZTI for polymorphic classes in the TU
           when not already emitted by EDG IL lowering */
        a_type_ptr tp;
        for (tp = il_header.primary_scope->types; tp != NULL; tp = tp->next) {
            if (ignore_type_in_back_end(tp)) continue;
            if (tp->source_corresp.name && strncmp(tp->source_corresp.name, "__SO__", 6) == 0) continue;
            if ((tp->kind == tk_class || tp->kind == tk_struct) &&
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
        }
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

    if (gcc_be_output_file_name) {
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_artifact(ctx, output_kind, gcc_be_output_file_name));
    } else if (primary_source_file_name) {
        char out_name[512];
        if (ext[0] == '\0') {
            snprintf(out_name, sizeof(out_name), "a.out");
        } else {
            /* Strip existing extension from primary_source_file_name if present */
            const char *dot = strrchr(primary_source_file_name, '.');
            if (dot && dot != primary_source_file_name) {
                int len = (int)(dot - primary_source_file_name);
                if (len > 500) len = 500;
                snprintf(out_name, sizeof(out_name), "%.*s%s", len, primary_source_file_name, ext);
            } else {
                snprintf(out_name, sizeof(out_name), "%s%s", primary_source_file_name, ext);
            }
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_artifact(ctx, output_kind, out_name));
    }
    
    /* Perform graceful cleanup and release of JIT contexts and caches. */
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Hook called by the EDG frontend to execute the backend.
 *
 * @details This is the standard entry point expected by the EDG frontend architecture.
 * It marks the backend as initialized and invokes the main code generation routine.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an appropriate error code.
 */
gcc_gen_be_error_t back_end(void) GCC_GEN_BE_NOEXCEPT {
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

    return gcc_gen_be();
}

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
        gcc_jit_context *tu_ctx = gcc_jit_context_acquire();
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
        GCC_GEN_BE_CHECK(cache_clear_all());
        GCC_GEN_BE_CHECK(gcc_gen_be_set_context(tu_ctx));

        gcc_gen_be_context_t *tu_state = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_state(&tu_state));
        if (tu_state) {
            tu_state->global_ctor_func = NULL;
            tu_state->global_ctor_block = NULL;
            tu_state->eh_state_ptr = NULL;
        }

        /* Emit compile-unit location entry for the current TU */
        (void)gcc_jit_context_new_location(tu_ctx, primary_source_file_name, 1, 1);

#if IL_SHOULD_BE_WRITTEN_TO_FILE
        if (!skip_il_read && f_il_output != NULL) {
            il_read(f_il_output);
        }
#endif
        if (il_header.primary_scope) {
            for (a_variable_ptr v = il_header.primary_scope->variables; v != NULL; v = v->next) {
                struct gcc_jit_lvalue *lval = NULL;
                err = gcc_gen_be_lower_global_variable_decl(v, &lval);
                if (err != GCC_GEN_BE_SUCCESS) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(saved_ctx));
                    gcc_jit_context_release(tu_ctx);
                    return err;
                }
            }
            for (a_routine_ptr r = il_header.primary_scope->routines; r != NULL; r = r->next) {
                struct gcc_jit_function *func = NULL;
                err = gcc_gen_be_lower_function_decl(r, &func);
                if (err != GCC_GEN_BE_SUCCESS) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(saved_ctx));
                    gcc_jit_context_release(tu_ctx);
                    return err;
                }
            }
            for (a_type_ptr tp = il_header.primary_scope->types; tp != NULL; tp = tp->next) {
                if ((tp->kind == tk_class || tp->kind == tk_struct) &&
                    tp->variant.class_struct_union.any_virtual_functions) {
                    void *cached_vt = NULL;
                    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, tp, &cached_vt));
                    if (!cached_vt) {
                        struct gcc_jit_lvalue *vt_lval = NULL;
                        err = gcc_gen_be_emit_vtable(tp, &vt_lval);
                        if (err != GCC_GEN_BE_SUCCESS) {
                            GCC_GEN_BE_CHECK(gcc_gen_be_set_context(saved_ctx));
                            gcc_jit_context_release(tu_ctx);
                            return err;
                        }
                        if (tp->variant.class_struct_union.any_virtual_base_classes) {
                            struct gcc_jit_lvalue *vtt_lval = NULL;
                            err = gcc_gen_be_emit_vtt(tp, &vtt_lval);
                            if (err != GCC_GEN_BE_SUCCESS) {
                                GCC_GEN_BE_CHECK(gcc_gen_be_set_context(saved_ctx));
                                gcc_jit_context_release(tu_ctx);
                                return err;
                            }
                        }
                    }
                    struct gcc_jit_lvalue *tinfo_lval = NULL;
                    err = gcc_gen_be_emit_type_descriptor(tp, &tinfo_lval);
                    if (err != GCC_GEN_BE_SUCCESS) {
                        GCC_GEN_BE_CHECK(gcc_gen_be_set_context(saved_ctx));
                        gcc_jit_context_release(tu_ctx);
                        return err;
                    }
                }
            }
        }
        err = gcc_gen_be_synthesize_global_ctors_dtors();
        if (err != GCC_GEN_BE_SUCCESS) {
            GCC_GEN_BE_CHECK(gcc_gen_be_set_context(saved_ctx));
            gcc_jit_context_release(tu_ctx);
            return err;
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_set_context(saved_ctx));
        
        char obj_file[256];
        snprintf(obj_file, sizeof(obj_file), "%s.o", source_files[i]);
        err = gcc_gen_be_emit_artifact(tu_ctx, ".o", obj_file);
        gcc_jit_context_release(tu_ctx);
        if (err != GCC_GEN_BE_SUCCESS) {
            primary_source_file_name = saved_primary_file;
            for (int j = 0; j < i; ++j) {
                char prev_obj[256];
                snprintf(prev_obj, sizeof(prev_obj), "%s.o", source_files[j]);
                (void)remove(prev_obj);
            }
            return GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED;
        }
    }
    primary_source_file_name = saved_primary_file;

    if (!gcc_be_compile_only && !gcc_be_assemble_only && gcc_be_output_file_name) {
        const char **obj_list = (const char **)malloc((size_t)num_files * sizeof(const char *));
        char (*obj_bufs)[256] = (char (*)[256])malloc((size_t)num_files * sizeof(*obj_bufs));
        if (!obj_list || !obj_bufs) {
            free((void *)obj_list);
            free(obj_bufs);
            return GCC_GEN_BE_ERROR_OOM;
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

gcc_gen_be_error_t gcc_gen_be_link_multi_tu_artifacts(
    const char **obj_files,
    int num_objs,
    const char *output_path,
    int argc,
    char **argv) GCC_GEN_BE_NOEXCEPT {
    if (!obj_files || num_objs <= 0 || !output_path) return GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED;
    for (int i = 0; i < num_objs; ++i) {
        if (!obj_files[i] || obj_files[i][0] == '\0') {
            return GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED;
        }
    }

    gcc_jit_context *link_ctx = gcc_jit_context_acquire();
    if (!link_ctx) return GCC_GEN_BE_ERROR_OOM;

    a_boolean is_shared = FALSE;
    for (int i = 0; i < argc; ++i) {
        if (argv && argv[i]) {
            if (strcmp(argv[i], "-shared") == 0) is_shared = TRUE;
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

    enum gcc_jit_output_kind kind = GCC_JIT_OUTPUT_KIND_OBJECT_FILE;
    if (strcmp(output_kind, "asm") == 0 || strcmp(output_kind, ".s") == 0) {
        kind = GCC_JIT_OUTPUT_KIND_ASSEMBLER;
    } else if (strcmp(output_kind, "obj") == 0 || strcmp(output_kind, ".o") == 0) {
        kind = GCC_JIT_OUTPUT_KIND_OBJECT_FILE;
    } else if (strcmp(output_kind, "dll") == 0 || strcmp(output_kind, ".so") == 0 || strcmp(output_kind, ".dylib") == 0) {
        kind = GCC_JIT_OUTPUT_KIND_DYNAMIC_LIBRARY;
    } else if (strcmp(output_kind, "exe") == 0 || strcmp(output_kind, ".out") == 0) {
        kind = GCC_JIT_OUTPUT_KIND_EXECUTABLE;
    }

    gcc_jit_context_compile_to_file(ctx, kind, output_path);
    const char *first_err = gcc_jit_context_get_first_error(ctx);
    if (first_err != NULL) {
        return GCC_GEN_BE_ERROR_COMPILATION_FAILED;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Synthesizes an Itanium ABI C++20 module initializer function (`_ZGIW<module_name>`).
 * @details Emits an exported module initializer function guarded by an internal idempotency boolean
 * flag (`_ZGIW<module_name>__guard`) so imported module initializers execute at most once.
 * @param[in] module_name The C++20 module name (e.g., `"math"` or `"std.core"`).
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
    char guard_name[256];
    size_t pos = 0;
    pos += (size_t)snprintf(fn_name, sizeof(fn_name), "_ZGIW");
    const char *p = module_name;
    while (*p != '\0' && pos + 16 < sizeof(fn_name)) {
        const char *dot = strchr(p, '.');
        size_t seg_len = dot ? (size_t)(dot - p) : strlen(p);
        pos += (size_t)snprintf(fn_name + pos, sizeof(fn_name) - pos, "%zu%.*s", seg_len, (int)seg_len, p);
        if (!dot) break;
        p = dot + 1;
    }
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
            size_t ipos = (size_t)snprintf(imp_fn_name, sizeof(imp_fn_name), "_ZGIW");
            const char *ip = imp_name;
            while (*ip != '\0' && ipos + 16 < sizeof(imp_fn_name)) {
                const char *idot = strchr(ip, '.');
                size_t iseg = idot ? (size_t)(idot - ip) : strlen(ip);
                ipos += (size_t)snprintf(imp_fn_name + ipos, sizeof(imp_fn_name) - ipos, "%zu%.*s", iseg, (int)iseg, ip);
                if (!idot) break;
                ip = idot + 1;
            }
            gcc_jit_function *imp_fn = gcc_jit_context_new_function(
                ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, imp_fn_name, 0, NULL, 0);
            gcc_jit_block_add_eval(run_blk, NULL, gcc_jit_context_new_call(ctx, NULL, imp_fn, 0, NULL));
        }
    }

    /* Invoke the TU's internal dynamic initializer if synthesized */
    gcc_gen_be_context_t *state = NULL;
    if (gcc_gen_be_get_state(&state) == GCC_GEN_BE_SUCCESS && state && state->global_ctor_func) {
        gcc_jit_block_add_eval(run_blk, NULL, gcc_jit_context_new_call(ctx, NULL, state->global_ctor_func, 0, NULL));
    }

    gcc_jit_block_end_with_jump(run_blk, NULL, done_blk);
    gcc_jit_block_end_with_void_return(done_blk, NULL);

    *out_init_fn = init_fn;
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
