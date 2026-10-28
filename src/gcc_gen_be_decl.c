/**
 * @file gcc_gen_be_decl.c
 * @brief Implementation of declaration lowering for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_dtor.h"
#if IL_SHOULD_BE_WRITTEN_TO_FILE && !defined(EDG_GCC_BE_ISOLATED_TEST)
#include "il_file.h"
#include "il_read.h"
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && !defined(EDG_GCC_BE_ISOLATED_TEST) */
#include "expr.h"
#include <libgccjit.h>
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE




/**
 * @brief Retrieves or creates the global dynamic initialization block.
 *
 * This lazily creates the `__edg_global_init` function which acts as
 * the constructor for global dynamic initializers.
 *
 * @param out_block A pointer to receive the gcc_jit_block.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_get_global_ctor_block(struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;
    
    gcc_gen_be_context_t *state = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_state(&state));
    
    if (!state->global_ctor_func) {
        gcc_jit_context *ctx = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
        if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
        
        gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        state->global_ctor_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, "__edg_global_init", 0, NULL, 0);
        if (p_gcc_jit_function_add_attribute) {
            p_gcc_jit_function_add_attribute(state->global_ctor_func, GCC_JIT_FN_ATTRIBUTE_USED);
        }
        state->global_ctor_block = gcc_jit_function_new_block(state->global_ctor_func, "entry");
        
#if defined(__APPLE__) && defined(__MACH__)
        gcc_jit_context_add_top_level_asm(
            ctx, NULL,
            ".section __DATA,__mod_init_func,mod_init_funcs\n"
            ".p2align 3\n"
            ".quad ___edg_global_init\n");
#else
        /* Put in .init_array */
        gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_rvalue *func_addr = gcc_jit_function_get_address(state->global_ctor_func, NULL);
        gcc_jit_rvalue *func_ptr = gcc_jit_context_new_cast(ctx, NULL, func_addr, void_ptr_type);
        gcc_jit_lvalue *init_array_entry = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_EXPORTED, void_ptr_type, "__edg_global_init_array_entry");
        gcc_jit_global_set_initializer_rvalue(init_array_entry, func_ptr);
        if (p_gcc_jit_lvalue_set_link_section) {
            p_gcc_jit_lvalue_set_link_section(init_array_entry, ".init_array");
        }
#endif
    }
    *out_block = state->global_ctor_block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively checks whether an EDG constant contains a relocatable address constant with a non-zero offset.
 * @details Used to detect static initializers containing interior vtable pointers (`_ZTV + 16`) or symbol+offset
 * expressions that require runtime constructor initialization when `libgccjit` rejects constant pointer arithmetic.
 * @param[in] con The EDG constant node to inspect.
 * @param[out] out_has_offset Receives TRUE if any nested `ck_address` has `offset != 0`, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if `out_has_offset` is NULL.
 */
static gcc_gen_be_error_t constant_has_non_zero_address_offset(
    a_constant_ptr con,
    a_boolean *out_has_offset) GCC_GEN_BE_NOEXCEPT {
    if (!out_has_offset) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_has_offset = FALSE;
    if (!con) return GCC_GEN_BE_SUCCESS;
    if (con->kind == ck_address && con->variant.address.offset != 0) {
        if (con->variant.address.kind == abk_variable && con->variant.address.variant.variable) {
            a_variable_ptr var = con->variant.address.variant.variable;
            a_type_ptr vtp = var->type;
            while (vtp && vtp->kind == tk_typeref) vtp = vtp->variant.typeref.type;
            long off = (long)con->variant.address.offset;
            if (vtp && vtp->kind == tk_array && vtp->variant.array.element_type) {
                a_type_ptr etp = vtp->variant.array.element_type;
                while (etp && etp->kind == tk_typeref) etp = etp->variant.typeref.type;
                long elem_sz = etp ? (long)etp->size : 0;
                if (elem_sz > 0 && (off % elem_sz) == 0) {
                    return GCC_GEN_BE_SUCCESS;
                }
            } else if (vtp && (vtp->kind == tk_struct || vtp->kind == tk_class)) {
                for (a_field_ptr fld = vtp->variant.class_struct_union.field_list; fld != NULL; fld = fld->next) {
                    if ((long)fld->offset == off) {
                        return GCC_GEN_BE_SUCCESS;
                    }
                    a_type_ptr ftp = fld->type;
                    while (ftp && ftp->kind == tk_typeref) ftp = ftp->variant.typeref.type;
                    if (ftp && ftp->kind == tk_array && ftp->variant.array.element_type &&
                        (long)fld->offset <= off && off < (long)fld->offset + (long)ftp->size) {
                        a_type_ptr fetp = ftp->variant.array.element_type;
                        while (fetp && fetp->kind == tk_typeref) fetp = fetp->variant.typeref.type;
                        long felem_sz = fetp ? (long)fetp->size : 0;
                        if (felem_sz > 0 && ((off - (long)fld->offset) % felem_sz) == 0) {
                            return GCC_GEN_BE_SUCCESS;
                        }
                    }
                }
            }
        }
        *out_has_offset = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }
    if (con->kind == ck_aggregate) {
        for (a_constant_ptr elem = con->variant.aggregate.first_constant; elem != NULL; elem = elem->next) {
            a_boolean sub_has = FALSE;
            GCC_GEN_BE_CHECK(constant_has_non_zero_address_offset(elem, &sub_has));
            if (sub_has) {
                *out_has_offset = TRUE;
                return GCC_GEN_BE_SUCCESS;
            }
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers an EDG variable declaration into a libgccjit global lvalue.
 *
 * This handles creating the global variable, applying linkage, TLS attributes,
 * and processing static or dynamic initializers.
 *
 * @param var The frontend variable node.
 * @param out_lval A pointer to a gcc_jit_lvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_lval` populated.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
  if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_lval = NULL;
  if (!var) return GCC_GEN_BE_SUCCESS;
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  void *cached = NULL;
  gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached);
  if (err != GCC_GEN_BE_SUCCESS) return err;
  if (cached) {
      *out_lval = (gcc_jit_lvalue *)cached;
      return GCC_GEN_BE_SUCCESS;
  }
  
  gcc_jit_type *var_type = NULL;
  err = gcc_gen_be_lower_type(var->type, &var_type);
  if (err != GCC_GEN_BE_SUCCESS) return err;

  char auto_name[128];
  const char *name = var->source_corresp.name;
  if (!name) {
      snprintf(auto_name, sizeof(auto_name), "__edg_anon_var_%p", (void *)var);
      name = auto_name;
  } else {
      void *existing_named = NULL;
      if (cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)name, &existing_named) == GCC_GEN_BE_SUCCESS &&
          existing_named && existing_named != (void *)var) {
          void *prev_global = NULL;
          if (cache_lookup(GCC_GEN_BE_CACHE_VAR, existing_named, &prev_global) == GCC_GEN_BE_SUCCESS && prev_global) {
              if (var->storage_class == sc_static && strncmp(name, "__", 2) != 0) {
                  snprintf(auto_name, sizeof(auto_name), "%s_%p", name, (void *)var);
                  name = auto_name;
              } else {
                  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, prev_global));
                  if (var->init_kind == initk_static && var->initializer.constant) {
                      a_constant_ptr init_con = var->initializer.constant;
                      gcc_jit_rvalue *init_rval = NULL;
                      extern gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(init_con, var_type, &init_rval));
                      if (init_rval) {
                          gcc_jit_global_set_initializer_rvalue((gcc_jit_lvalue *)prev_global, init_rval);
                      }
                  }
                  *out_lval = (gcc_jit_lvalue *)prev_global;
                  return GCC_GEN_BE_SUCCESS;
              }
          } else if (strncmp(name, "_ZTV", 4) == 0 ||
                     strncmp(name, "_ZTT", 4) == 0 ||
                     strncmp(name, "_ZTI", 4) == 0 ||
                     strncmp(name, "_ZTS", 4) == 0) {
              GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, existing_named));
              *out_lval = (gcc_jit_lvalue *)existing_named;
              return GCC_GEN_BE_SUCCESS;
          }
      } else {
          GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)name, (void *)var));
      }
  }
  
  enum gcc_jit_global_kind linkage;
  if (!var->source_corresp.name) {
      linkage = GCC_JIT_GLOBAL_INTERNAL;
  } else if (var->storage_class == sc_extern && var->init_kind == initk_none) {
      linkage = GCC_JIT_GLOBAL_IMPORTED;
  } else if (var->storage_class == sc_static) {
      linkage = GCC_JIT_GLOBAL_INTERNAL;
  } else {
      linkage = GCC_JIT_GLOBAL_EXPORTED;
  }
  
  gcc_jit_location *loc = NULL;
  err = gcc_gen_be_get_location(&var->source_corresp.decl_position, &loc);
  if (err != GCC_GEN_BE_SUCCESS) return err;

  gcc_jit_lvalue *global = gcc_jit_context_new_global(ctx, loc, linkage, var_type, name);
  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, global));

  if (var->is_inline || var->is_weak) {
      GCC_GEN_BE_CHECK(gcc_gen_be_apply_variable_attributes(var, global));
      GCC_GEN_BE_CHECK(gcc_gen_be_apply_comdat_linkage(global, name, FALSE));
  }
  
  extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;
  GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(var, global));
  
#if defined(GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC)
  if (var->is_thread_local && p_gcc_jit_lvalue_set_tls_model) {
      enum gcc_jit_tls_model tls_model = GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC;

      extern a_boolean gcc_be_fPIC;
      extern a_boolean gcc_be_fPIE;
      a_boolean pic_enabled = gcc_be_fPIC || gcc_be_fPIE;

      if (linkage == GCC_JIT_GLOBAL_EXPORTED) {
          tls_model = GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC;
      } else if (linkage == GCC_JIT_GLOBAL_INTERNAL) {
          if (!pic_enabled) {
              tls_model = GCC_JIT_TLS_MODEL_LOCAL_EXEC;
          } else {
              tls_model = GCC_JIT_TLS_MODEL_LOCAL_DYNAMIC;
          }
      } else if (linkage == GCC_JIT_GLOBAL_IMPORTED) {
          if (!pic_enabled) {
              tls_model = GCC_JIT_TLS_MODEL_INITIAL_EXEC;
          } else {
              tls_model = GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC;
          }
      }
      p_gcc_jit_lvalue_set_tls_model(global, tls_model);
  }
#endif

  a_boolean has_reloc_offset = FALSE;
  if (var->init_kind == initk_static && var->initializer.constant) {
      GCC_GEN_BE_CHECK(constant_has_non_zero_address_offset(var->initializer.constant, &has_reloc_offset));
  }

  if (var_type && var->type->kind == tk_typeref && (var->type->variant.typeref.qualifiers & TQ_CONST) &&
      var->source_corresp.name != NULL && !has_reloc_offset) {
#if defined(LIBGCCJIT_HAVE_gcc_jit_global_set_readonly)
      gcc_jit_global_set_readonly(global);
#endif
  }
  
  if (var->init_kind == initk_zero) {
      gcc_jit_rvalue *zero_val = NULL;
      GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(var->type, var_type, &zero_val));
      if (zero_val) {
          gcc_jit_global_set_initializer_rvalue(global, zero_val);
      }
  } else if (var->init_kind == initk_static) {
      a_constant_ptr init_con = var->initializer.constant;
      if (init_con) {
          a_type_ptr vtype_stripped = var->type;
          while (vtype_stripped && vtype_stripped->kind == tk_typeref) {
              vtype_stripped = vtype_stripped->variant.typeref.type;
          }
          if (vtype_stripped && vtype_stripped->kind == tk_array && init_con->kind == ck_string) {
              size_t total_size = (size_t)vtype_stripped->size;
              if (total_size == 0 || total_size < (size_t)init_con->variant.string.length) {
                  total_size = (size_t)init_con->variant.string.length;
              }
              char *padded = (char *)calloc(1, total_size);
              if (padded) {
                  memcpy(padded, init_con->variant.string.value, (size_t)init_con->variant.string.length);
                  gcc_jit_global_set_initializer(global, padded, total_size);
                  free(padded);
              }
          } else {
              gcc_jit_rvalue *init_rval = NULL;
              extern gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(init_con, var_type, &init_rval));
              if (init_rval) {
                  gcc_jit_type *init_type = gcc_jit_rvalue_get_type(init_rval);
                  if (init_type != var_type) {
                      if (gcc_jit_type_is_pointer(init_type) != NULL || gcc_jit_type_is_pointer(var_type) != NULL) {
                          init_rval = gcc_jit_context_new_cast(ctx, NULL, init_rval, var_type);
                      } else if (gcc_jit_type_is_struct(var_type) != NULL) {
                          gcc_jit_struct *outer_s = gcc_jit_type_is_struct(var_type);
                          if (gcc_jit_struct_get_field_count(outer_s) == 1) {
                              gcc_jit_field *outer_f0 = gcc_jit_struct_get_field(outer_s, 0);
                              init_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, var_type, 1, &outer_f0, &init_rval);
                          }
                      }
                  }
                  if (init_rval) {
                      if (has_reloc_offset &&
                          !(var->source_corresp.name &&
                            (strncmp(var->source_corresp.name, "_ZTV", 4) == 0 ||
                             strncmp(var->source_corresp.name, "_ZTT", 4) == 0 ||
                             strncmp(var->source_corresp.name, "_ZTI", 4) == 0))) {
                          gcc_jit_block *cblock = NULL;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblock));
                          if (cblock) {
                              gcc_jit_block_add_assignment(cblock, NULL, global, init_rval);
                          }
                      } else {
                          gcc_jit_global_set_initializer_rvalue(global, init_rval);
                      }
                  }
              }
          }
      } else if (var->storage_class != sc_static || strncmp(name, "__C", 3) != 0) {
          gcc_jit_rvalue *zero_val = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(var->type, var_type, &zero_val));
          if (zero_val) {
              gcc_jit_global_set_initializer_rvalue(global, zero_val);
          }
      }
  } else if (var->init_kind == initk_dynamic) {
      if (var->initializer.dynamic && var->initializer.dynamic->kind != dik_none) {
          gcc_jit_block *orig_block = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&orig_block));
          gcc_jit_block *cblock = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblock));
          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cblock));
          gcc_jit_rvalue *init_rval = NULL;
          extern gcc_gen_be_error_t gcc_gen_be_lower_dynamic_init(a_dynamic_init_ptr dip, struct gcc_jit_lvalue *target_lval, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;
          GCC_GEN_BE_CHECK(gcc_gen_be_lower_dynamic_init(var->initializer.dynamic, global, &init_rval));
          gcc_jit_block *latest_ctor_block = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&latest_ctor_block));
          gcc_gen_be_context_t *state = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_get_state(&state));
          if (state && latest_ctor_block) {
              state->global_ctor_block = latest_ctor_block;
          }
          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(orig_block));
      }
      if (var->initializer.dynamic && var->initializer.dynamic->destructor) {
          void *cached_dtor = NULL;
          err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, var->initializer.dynamic->destructor, &cached_dtor);
          if (err != GCC_GEN_BE_SUCCESS || !cached_dtor) {
              gcc_jit_function *lowered_dtor = NULL;
              if (gcc_gen_be_lower_function_decl(var->initializer.dynamic->destructor, &lowered_dtor) == GCC_GEN_BE_SUCCESS) {
                  cached_dtor = lowered_dtor;
              }
          }
          if (cached_dtor) {
              gcc_jit_block *orig_block = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&orig_block));
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblock));
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cblock));
              
              /* Call __cxa_atexit / __cxa_thread_atexit via imported functions */
              gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
              gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
              gcc_jit_param *p1 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "f");
              gcc_jit_param *p2 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "obj");
              gcc_jit_param *p3 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "dso_handle");
              gcc_jit_param *atexit_params[] = {p1, p2, p3};
              
              const char *atexit_name = var->is_thread_local ? "__cxa_thread_atexit" : "__cxa_atexit";
              gcc_jit_function *atexit_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, atexit_name, 3, atexit_params, 0);
              
              gcc_jit_rvalue *func_addr = gcc_jit_function_get_address((gcc_jit_function*)cached_dtor, NULL);
              gcc_jit_rvalue *func_ptr = gcc_jit_context_new_cast(ctx, NULL, func_addr, void_ptr_type);
              gcc_jit_rvalue *obj_ptr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(global, NULL), void_ptr_type);
              void *cached_dso_var = NULL;
              gcc_jit_lvalue *dso_global = NULL;
              if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__dso_handle", &cached_dso_var) == GCC_GEN_BE_SUCCESS && cached_dso_var) {
                  dso_global = (gcc_jit_lvalue *)cached_dso_var;
              } else {
                  dso_global = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_IMPORTED, void_ptr_type, "__dso_handle");
                  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__dso_handle", dso_global));
              }
              gcc_jit_rvalue *dso_handle = dso_global
                  ? gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(dso_global, NULL), void_ptr_type)
                  : gcc_jit_context_null(ctx, void_ptr_type);
              
              gcc_jit_rvalue *atexit_args[] = {func_ptr, obj_ptr, dso_handle};
              gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, atexit_func, 3, atexit_args));
              
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(orig_block));
          }
      }
  }
  
  *out_lval = global;
  return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers an EDG variable declaration into a libgccjit local lvalue.
 *
 * @param func The libgccjit function the local belongs to.
 * @param var The frontend variable node.
 * @param out_lval A pointer to a gcc_jit_lvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_lval` populated.
 */
gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
  if (!out_lval || !func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_lval = NULL;
  if (!var) return GCC_GEN_BE_SUCCESS;

  if (var->storage_class == sc_static) {
      /* Static locals are implemented as global variables */
      return gcc_gen_be_lower_global_variable_decl(var, out_lval);
  }

  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

  void *cached = NULL;
  gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_VAR, var, &cached);
  if (err != GCC_GEN_BE_SUCCESS) return err;
  if (cached) {
      *out_lval = (gcc_jit_lvalue *)cached;
      return GCC_GEN_BE_SUCCESS;
  }

  extern gcc_gen_be_error_t gcc_gen_be_get_active_coroutine_frame(struct gcc_jit_type **out_frame_type, struct gcc_jit_lvalue **out_frame_lval) GCC_GEN_BE_NOEXCEPT;
  gcc_jit_type *coro_ft = NULL;
  gcc_jit_lvalue *coro_flval = NULL;
  void *cached_coro_fld = NULL;
  if (gcc_gen_be_get_active_coroutine_frame(&coro_ft, &coro_flval) == GCC_GEN_BE_SUCCESS &&
      coro_flval != NULL &&
      cache_lookup(GCC_GEN_BE_CACHE_FIELD, var, &cached_coro_fld) == GCC_GEN_BE_SUCCESS &&
      cached_coro_fld != NULL) {
      gcc_jit_lvalue *frame_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(coro_flval), NULL);
      gcc_jit_lvalue *coro_slot = gcc_jit_lvalue_access_field(frame_deref, NULL, (gcc_jit_field *)cached_coro_fld);
      GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, coro_slot));
      *out_lval = coro_slot;
      return GCC_GEN_BE_SUCCESS;
  }

  gcc_jit_type *var_type = NULL;
  err = gcc_gen_be_lower_type(var->type, &var_type);
  if (err != GCC_GEN_BE_SUCCESS) return err;

  const char *name = var->source_corresp.name;
  if (!name) name = "unnamed_local";

  gcc_jit_location *loc = NULL;
  err = gcc_gen_be_get_location(&var->source_corresp.decl_position, &loc);
  if (err != GCC_GEN_BE_SUCCESS) return err;

  gcc_jit_lvalue *local = gcc_jit_function_new_local(func, loc, var_type, name);
  
  extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;
  GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(var, local));
  
  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, local));

  /* Emit static or zero initializer for local automatic variables if present */
  gcc_jit_block *cblock = NULL;
  if (gcc_gen_be_get_current_block(&cblock) == GCC_GEN_BE_SUCCESS && cblock) {
      if (var->init_kind == initk_static && var->initializer.constant) {
          extern gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;
          gcc_jit_rvalue *init_rval = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(var->initializer.constant, var_type, &init_rval));
          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
          if (init_rval && cblock) {
              gcc_jit_block_add_assignment(cblock, loc, local, init_rval);
          }
      } else if (var->init_kind == initk_zero) {
          gcc_jit_rvalue *zero_rval = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(var->type, var_type, &zero_rval));
          if (zero_rval && cblock) {
              gcc_jit_block_add_assignment(cblock, loc, local, zero_rval);
          }
      }
  }

  *out_lval = local;
  return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers an EDG function/routine declaration into a libgccjit function.
 *
 * Handles creation of the function signature, params, and caches the result.
 *
 * @param rout The frontend routine node.
 * @param out_func A pointer to a gcc_jit_function pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_func` populated.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
  if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_func = NULL;
  if (!rout) return GCC_GEN_BE_SUCCESS;
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  void *cached = NULL;
  gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, rout, &cached);
  if (err != GCC_GEN_BE_SUCCESS) return err;
  if (cached) {
      *out_func = (gcc_jit_function *)cached;
      return GCC_GEN_BE_SUCCESS;
  }
  
  gcc_jit_type *ret_type = NULL;
  a_boolean uses_sret = FALSE;
  gcc_jit_type *sret_ptr_type = NULL;
  if (rout->type && rout->type->kind == tk_routine) {
      a_type_ptr frontend_ret = rout->type->variant.routine.return_type;
      extern gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;
      err = gcc_gen_be_is_returned_via_sret(frontend_ret, &uses_sret);
      if (err != GCC_GEN_BE_SUCCESS) return err;
      if (uses_sret) {
          ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
          gcc_jit_type *actual_ret = NULL;
          err = gcc_gen_be_lower_type(frontend_ret, &actual_ret);
          if (err != GCC_GEN_BE_SUCCESS) return err;
          sret_ptr_type = gcc_jit_type_get_pointer(actual_ret);
      } else {
          err = gcc_gen_be_lower_type(frontend_ret, &ret_type);
          if (err != GCC_GEN_BE_SUCCESS) return err;
          if (frontend_ret) {
              extern gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;
              gcc_jit_type *coerced_ret = NULL;
              a_boolean ret_coerced = FALSE;
              if (gcc_gen_be_get_coerced_abi_type(frontend_ret, &coerced_ret, &ret_coerced) == GCC_GEN_BE_SUCCESS &&
                  ret_coerced && coerced_ret) {
                  ret_type = coerced_ret;
              }
          }
      }
  } else {
      ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
  }

  char auto_name[1024];
  const char *name = rout->source_corresp.name;
#if NEED_NAME_MANGLING
  if (rout->source_corresp.unmangled_name_or_mangled_encoding &&
      rout->source_corresp.unmangled_name_or_mangled_encoding[0] != '\0' &&
      strncmp(rout->source_corresp.unmangled_name_or_mangled_encoding, "_Z", 2) == 0) {
      name = rout->source_corresp.unmangled_name_or_mangled_encoding;
  }
#endif
  if (!name) {
      snprintf(auto_name, sizeof(auto_name), "__edg_anon_fn_%p", (void *)rout);
      name = auto_name;
  } else {
      a_scope_ptr pscope = rout->source_corresp.parent_scope;
      if (pscope && pscope->kind == sck_class_struct_union && pscope->variant.assoc_type &&
          strncmp(name, "_Z", 2) != 0) {
          extern gcc_gen_be_error_t gcc_gen_be_mangle_function_name(
              a_routine_ptr r, const char *variant_code, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT;
          if (gcc_gen_be_mangle_function_name(rout, NULL, auto_name, sizeof(auto_name)) == GCC_GEN_BE_SUCCESS &&
              auto_name[0] != '\0') {
              name = auto_name;
          }
      }
      void *existing_named = NULL;
      if (cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)name, &existing_named) == GCC_GEN_BE_SUCCESS &&
          existing_named && existing_named != (void *)rout) {
          if (rout->storage_class == sc_static) {
              char dup_name[1088];
              snprintf(dup_name, sizeof(dup_name), "%s_%p", name, (void *)rout);
              snprintf(auto_name, sizeof(auto_name), "%s", dup_name);
              name = auto_name;
          } else {
              void *prev_fn = NULL;
              if (cache_lookup(GCC_GEN_BE_CACHE_FUNC, existing_named, &prev_fn) == GCC_GEN_BE_SUCCESS && prev_fn &&
                  rout->function_def_number == NULL_function_def_number) {
                  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, rout, prev_fn));
                  *out_func = (gcc_jit_function *)prev_fn;
                  return GCC_GEN_BE_SUCCESS;
              }
              char dup_name[1088];
              snprintf(dup_name, sizeof(dup_name), "%s_%p", name, (void *)rout);
              snprintf(auto_name, sizeof(auto_name), "%s", dup_name);
              name = auto_name;
          }
      } else {
          GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)name, (void *)rout));
      }
  }
  
  enum gcc_jit_function_kind linkage;
  if (rout->function_def_number == NULL_function_def_number) {
      linkage = GCC_JIT_FUNCTION_IMPORTED;
  } else if (rout->storage_class == sc_static) {
      linkage = GCC_JIT_FUNCTION_INTERNAL;
  } else {
      linkage = GCC_JIT_FUNCTION_EXPORTED;
  }
  
  int num_params = 0;
  if (uses_sret) num_params++;
  
  a_param_type_ptr ptp;
  if (rout->type && rout->type->kind == tk_routine) {
      for (ptp = rout->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
          num_params++;
      }
  }
  
  gcc_jit_param **params = NULL;
  if (num_params > 0) {
      params = (gcc_jit_param **)malloc((size_t)num_params * sizeof(gcc_jit_param *));
      if (!params) return GCC_GEN_BE_ERROR_OOM;
      
      int i = 0;
      if (uses_sret) {
          params[i++] = gcc_jit_context_new_param(ctx, NULL, sret_ptr_type, "sret_buffer");
      }
      
      a_variable_ptr var_param = NULL;
      a_scope_ptr def_scope = NULL;
      if (rout->function_def_number != NULL_function_def_number) {
#if IL_SHOULD_BE_WRITTEN_TO_FILE && !defined(EDG_GCC_BE_ISOLATED_TEST)
          a_memory_region_number rnum = mem_region_for_routine(rout);
          if (rnum != NULL_region_number && mem_region_table && mem_region_table[rnum] == NULL && f_il_output != NULL) {
              read_memory_region(rnum);
          }
#endif
          def_scope = scope_for_routine_or_null(rout);
      }
      if (def_scope) {
          var_param = def_scope->variant.routine.parameters;
      }
      
      for (ptp = rout->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
          gcc_jit_type *param_type = NULL;
          a_type_ptr ptype = ptp->type;
          while (ptype && ptype->kind == tk_typeref) {
              ptype = ptype->variant.typeref.type;
          }
          if (ptype && ptype->kind == tk_array) {
              gcc_jit_type *elem_type = NULL;
              err = gcc_gen_be_lower_type(ptype->variant.array.element_type, &elem_type);
              if (err != GCC_GEN_BE_SUCCESS) { free(params); return err; }
              param_type = gcc_jit_type_get_pointer(elem_type);
          } else {
              err = gcc_gen_be_lower_type(ptype, &param_type);
              if (err != GCC_GEN_BE_SUCCESS) {
                  free(params);
                  return err;
              }
              param_type = gcc_jit_type_unqualified(param_type);
              extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
              extern gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;
              a_boolean is_invis_ref = FALSE;
              err = gcc_gen_be_is_passed_by_invisible_reference(ptype, &is_invis_ref);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              if (is_invis_ref) {
                  param_type = gcc_jit_type_get_pointer(param_type);
              } else if (ptype) {
                  gcc_jit_type *coerced_pt = NULL;
                  a_boolean needs_coercion = FALSE;
                  if (gcc_gen_be_get_coerced_abi_type(ptype, &coerced_pt, &needs_coercion) == GCC_GEN_BE_SUCCESS &&
                      needs_coercion && coerced_pt) {
                      param_type = coerced_pt;
                  }
              }
          }
          
          const char *param_name = "unnamed_param";
          gcc_jit_location *param_loc = NULL;
          if (var_param) {
              if (var_param->source_corresp.name) param_name = var_param->source_corresp.name;
              err = gcc_gen_be_get_location_from_var(var_param, &param_loc);
              if (err != GCC_GEN_BE_SUCCESS) {
                  free(params);
                  return err;
              }
              var_param = var_param->next;
          }
          params[i] = gcc_jit_context_new_param(ctx, param_loc, param_type, param_name);
          i++;
      }
  }

  int is_variadic = 0;
  if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info->has_ellipsis) {
      is_variadic = 1;
  }

  gcc_jit_location *loc = NULL;
  err = gcc_gen_be_get_location(&rout->source_corresp.decl_position, &loc);
  if (err != GCC_GEN_BE_SUCCESS) {
      if (params) free(params);
      return err;
  }

  gcc_jit_function *func = gcc_jit_context_new_function(
      ctx, loc, linkage, ret_type, name, num_params, params, is_variadic);
  extern gcc_gen_be_error_t gcc_gen_be_apply_function_attributes(a_routine_ptr rout, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
  GCC_GEN_BE_CHECK(gcc_gen_be_apply_function_attributes(rout, func));
      
  if (rout->function_def_number != NULL_function_def_number && p_gcc_jit_function_add_attribute) {
      if (rout->is_weak || rout->is_inline || rout->is_template_function) {
          p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_WEAK);
      }
  }
      
  if (num_params > 0) {
      int i = 0;
      if (uses_sret) {
          GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, func, params[0]));
          i = 1;
      }
      a_variable_ptr var_param = NULL;
      a_scope_ptr def_scope = NULL;
      if (rout->function_def_number != NULL_function_def_number) {
#if IL_SHOULD_BE_WRITTEN_TO_FILE && !defined(EDG_GCC_BE_ISOLATED_TEST)
          a_memory_region_number rnum = mem_region_for_routine(rout);
          if (rnum != NULL_region_number && mem_region_table && mem_region_table[rnum] == NULL && f_il_output != NULL) {
              read_memory_region(rnum);
          }
#endif
          def_scope = scope_for_routine_or_null(rout);
      }
      if (def_scope) {
          var_param = def_scope->variant.routine.parameters;
      }
      for (ptp = rout->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
          if (var_param) {
              gcc_jit_lvalue *param_lval = gcc_jit_param_as_lvalue(params[i]);
              
              extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;
              GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(var_param, param_lval));
              
              extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
              a_type_ptr ptype = ptp->type;
              while (ptype && ptype->kind == tk_typeref) ptype = ptype->variant.typeref.type;
              a_boolean is_invis_ref = FALSE;
              err = gcc_gen_be_is_passed_by_invisible_reference(ptype, &is_invis_ref);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              if (is_invis_ref) {
                  param_lval = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(param_lval), NULL);
              }
              
              GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var_param, param_lval));
              var_param = var_param->next;
          }
          i++;
      }
      free(params);
  }
  
  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, rout, func));
  
  if (rout->function_def_number != NULL_function_def_number) {
#if IL_SHOULD_BE_WRITTEN_TO_FILE && !defined(EDG_GCC_BE_ISOLATED_TEST)
      a_memory_region_number rnum = mem_region_for_routine(rout);
      if (rnum != NULL_region_number && mem_region_table && mem_region_table[rnum] == NULL && f_il_output != NULL) {
          read_memory_region(rnum);
      }
#endif
      a_scope_ptr def_scope = scope_for_routine_or_null(rout);
      if (def_scope && def_scope->assoc_block) {
          if (rout->is_coroutine &&
              def_scope->assoc_block->kind != stmk_coroutine &&
              !(def_scope->assoc_block->kind == stmk_block &&
                def_scope->assoc_block->variant.block.statements &&
                def_scope->assoc_block->variant.block.statements->kind == stmk_coroutine)) {
              gcc_jit_block *saved_caller_block = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_caller_block));
              a_scope_ptr saved_innermost_scope = innermost_function_scope;
              innermost_function_scope = def_scope;
              extern gcc_gen_be_error_t gcc_gen_be_lower_coroutine_ramp(a_routine_ptr r, struct gcc_jit_function *rf) GCC_GEN_BE_NOEXCEPT;
              err = gcc_gen_be_lower_coroutine_ramp(rout, func);
              innermost_function_scope = saved_innermost_scope;
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_caller_block));
              if (err != GCC_GEN_BE_SUCCESS) return err;
              *out_func = func;
              return GCC_GEN_BE_SUCCESS;
          }
          gcc_jit_block *saved_caller_block = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_caller_block));
          a_scope_ptr saved_innermost_scope = innermost_function_scope;
          innermost_function_scope = def_scope;
          gcc_gen_be_context_t *be_state = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_get_state(&be_state));
          struct gcc_gen_be_scope **saved_scope_stack = be_state ? be_state->scope_stack : NULL;
          size_t saved_scope_size = be_state ? be_state->scope_stack_size : 0;
          size_t saved_scope_cap = be_state ? be_state->scope_stack_capacity : 0;
          if (be_state) {
              be_state->scope_stack = NULL;
              be_state->scope_stack_size = 0;
              be_state->scope_stack_capacity = 0;
          }

          gcc_jit_block *entry = gcc_jit_function_new_block(func, "entry");
          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(entry));

          /* Unpack invisible-reference or register-coerced aggregate parameters into stack locals */
          extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
          extern gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;
          extern gcc_gen_be_error_t gcc_gen_be_unpack_coerced_Arg(
              gcc_jit_function *fn,
              gcc_jit_block *blk,
              a_type_ptr ptype,
              gcc_jit_rvalue *prval,
              gcc_jit_lvalue **out_unpacked) GCC_GEN_BE_NOEXCEPT;
          extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;
          for (a_variable_ptr p_unpack = def_scope->variant.routine.parameters; p_unpack != NULL; p_unpack = p_unpack->next) {
              a_type_ptr utype = p_unpack->type;
              while (utype && utype->kind == tk_typeref) utype = utype->variant.typeref.type;
              a_boolean is_invis = FALSE;
              if (utype && gcc_gen_be_is_passed_by_invisible_reference(utype, &is_invis) == GCC_GEN_BE_SUCCESS && is_invis) {
                  void *cached_p = NULL;
                  if (cache_lookup(GCC_GEN_BE_CACHE_VAR, p_unpack, &cached_p) == GCC_GEN_BE_SUCCESS && cached_p) {
                      gcc_jit_type *val_t = NULL;
                      if (gcc_gen_be_lower_type(utype, &val_t) == GCC_GEN_BE_SUCCESS && val_t &&
                          utype->size > 0) {
                          const char *pname = p_unpack->source_corresp.name ? p_unpack->source_corresp.name : "param_unpacked";
                          gcc_jit_lvalue *unpacked_local = gcc_jit_function_new_local(func, NULL, val_t, pname);
                          gcc_jit_block_add_assignment(entry, NULL, unpacked_local, gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached_p));
                          GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(p_unpack, unpacked_local));
                          GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, p_unpack, unpacked_local));
                      }
                  }
              } else if (utype) {
                  gcc_jit_type *coerced_pt = NULL;
                  a_boolean needs_coercion = FALSE;
                  if (gcc_gen_be_get_coerced_abi_type(utype, &coerced_pt, &needs_coercion) == GCC_GEN_BE_SUCCESS &&
                      needs_coercion && coerced_pt) {
                      void *cached_p = NULL;
                      if (cache_lookup(GCC_GEN_BE_CACHE_VAR, p_unpack, &cached_p) == GCC_GEN_BE_SUCCESS && cached_p) {
                          gcc_jit_lvalue *unpacked_local = NULL;
                          if (gcc_gen_be_unpack_coerced_Arg(
                                  func, entry, utype,
                                  gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached_p),
                                  &unpacked_local) == GCC_GEN_BE_SUCCESS && unpacked_local) {
                              GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(p_unpack, unpacked_local));
                              GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, p_unpack, unpacked_local));
                          }
                      }
                  }
              }
          }

          extern gcc_gen_be_error_t gcc_gen_be_begin_function_eh(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
          extern gcc_gen_be_error_t gcc_gen_be_end_function_eh(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
          extern gcc_gen_be_error_t gcc_gen_be_begin_function_stmts(void) GCC_GEN_BE_NOEXCEPT;
          extern gcc_gen_be_error_t gcc_gen_be_end_function_stmts(gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
          extern gcc_gen_be_error_t gcc_gen_be_lower_routine_body(a_statement_ptr stmt, gcc_jit_function *func, an_exception_specification_ptr exc_spec) GCC_GEN_BE_NOEXCEPT;
          GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_stmts());
          
#ifndef EDG_GCC_BE_ISOLATED_TEST
          for (a_local_static_variable_init_ptr lsvip = def_scope->local_static_variable_inits;
               lsvip != NULL;
               lsvip = lsvip->next) {
              if (lsvip->variable && lsvip->variable->init_kind == initk_function_local) {
                  lsvip->variable->init_kind = lsvip->init_kind;
                  lsvip->variable->initializer = lsvip->initializer;
              }
          }
#endif
          /* Pre-declare function-scope local variables and lowering temporaries */
          for (a_variable_ptr sv = def_scope->variables; sv != NULL; sv = sv->next) {
              if (!sv->is_parameter && !sv->has_variably_modified_type &&
                  !(sv->storage_class == sc_static && !sv->source_corresp.name && sv->init_kind == initk_static)) {
                  gcc_jit_lvalue *sv_lval = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, sv, &sv_lval));
              }
          }
          for (a_variable_ptr nsv = def_scope->nonstatic_variables; nsv != NULL; nsv = nsv->next) {
              if (!nsv->is_parameter && !nsv->has_variably_modified_type) {
                  gcc_jit_lvalue *nsv_lval = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, nsv, &nsv_lval));
              }
          }
          if (exceptions_enabled && def_scope->lifetime && def_scope->lifetime->destructions) {
              for (a_dynamic_init_ptr d = def_scope->lifetime->destructions; d != NULL; d = d->next_in_destruction_list) {
                  if (d->destructor && d->variable) {
                      gcc_jit_lvalue *d_lval = NULL;
                      gcc_jit_function *d_fn = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, d->variable, &d_lval));
                      GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(d->destructor, &d_fn));
                  }
              }
              /* Restore current_block to entry in case lowering d->destructor cleared current_block */
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(entry));
          }

          GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(func));
          GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(def_scope->lifetime));
          if (exceptions_enabled && def_scope->lifetime && def_scope->lifetime->destructions) {
              a_boolean any_cleanup_reg = FALSE;
              for (a_dynamic_init_ptr d = def_scope->lifetime->destructions; d != NULL; d = d->next_in_destruction_list) {
                  if (d->destructor && d->variable) {
                      void *d_lval = NULL;
                      void *d_fn = NULL;
                      if (cache_lookup(GCC_GEN_BE_CACHE_VAR, d->variable, &d_lval) == GCC_GEN_BE_SUCCESS &&
                          cache_lookup(GCC_GEN_BE_CACHE_FUNC, d->destructor, &d_fn) == GCC_GEN_BE_SUCCESS &&
                          d_lval && d_fn) {
                          if (gcc_gen_be_register_cleanup((gcc_jit_lvalue *)d_lval, (gcc_jit_function *)d_fn) == GCC_GEN_BE_SUCCESS) {
                              any_cleanup_reg = TRUE;
                          }
                      }
                  }
              }
              if (any_cleanup_reg) {
                  extern gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT;
                  GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(func));
              }
          }
          an_exception_specification_ptr exc_spec =
              (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info)
                  ? rout->type->variant.routine.extra_info->exception_specification
                  : NULL;
          err = gcc_gen_be_lower_routine_body(def_scope->assoc_block, func, exc_spec);
          if (err != GCC_GEN_BE_SUCCESS) {
              if (be_state) {
                  if (be_state->scope_stack) free(be_state->scope_stack);
                  be_state->scope_stack = saved_scope_stack;
                  be_state->scope_stack_size = saved_scope_size;
                  be_state->scope_stack_capacity = saved_scope_cap;
              }
              innermost_function_scope = saved_innermost_scope;
              return err;
          }
          GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(func));
          GCC_GEN_BE_CHECK(gcc_gen_be_end_function_stmts(func));
          
          gcc_jit_block *current = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current));
          GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(current));
          if (current) {
              /* If flow reaches end of function without return, and it's void, add void return.
                 Otherwise, libgccjit will error out. */
              if (ret_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                  gcc_jit_block_end_with_void_return(current, NULL);
              } else {
                  a_type_ptr frontend_ret = (rout->type && rout->type->kind == tk_routine)
                      ? rout->type->variant.routine.return_type
                      : NULL;
                  gcc_jit_rvalue *zero = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(frontend_ret, ret_type, &zero));
                  if (!zero) {
                      zero = gcc_jit_context_null(ctx, ret_type);
                  }
                  gcc_jit_block_end_with_return(current, NULL, zero);
              }
          }
          if (be_state) {
              if (be_state->scope_stack) free(be_state->scope_stack);
              be_state->scope_stack = saved_scope_stack;
              be_state->scope_stack_size = saved_scope_size;
              be_state->scope_stack_capacity = saved_scope_cap;
          }
          innermost_function_scope = saved_innermost_scope;
          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_caller_block));

          if (rout->special_kind == sfk_constructor || rout->special_kind == sfk_destructor) {
              const char *rname = rout->source_corresp.name ? rout->source_corresp.name : "";
              if (strstr(rname, "C2E") == NULL && strstr(rname, "D2E") == NULL && strstr(rname, "D0E") == NULL) {
                  gcc_jit_function *base_clone = NULL;
                  gcc_jit_function *del_clone = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(rout, func, &base_clone, &del_clone));
              }
          }
      }
  }
  
  *out_func = func;
  return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_ctor_dtor_clones(
    a_routine_ptr rout,
    struct gcc_jit_function *primary_func,
    struct gcc_jit_function **out_base_clone,
    struct gcc_jit_function **out_deleting_clone) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !primary_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (out_base_clone) *out_base_clone = NULL;
    if (out_deleting_clone) *out_deleting_clone = NULL;

    if (rout->special_kind != sfk_constructor && rout->special_kind != sfk_destructor) {
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    extern gcc_gen_be_error_t gcc_gen_be_mangle_function_name(
        a_routine_ptr rout, const char *variant_code, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT;

    const char *base_variant = (rout->special_kind == sfk_constructor) ? "C2" : "D2";
    char base_name[256];
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(rout, base_variant, base_name, sizeof(base_name)));

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);

    a_type_ptr parent_class = rout->source_corresp.is_class_member
        ? rout->source_corresp.parent_scope->variant.assoc_type
        : NULL;
    a_boolean has_vbases = (parent_class &&
        (parent_class->kind == tk_class || parent_class->kind == tk_struct) &&
        parent_class->variant.class_struct_union.any_virtual_base_classes) ? TRUE : FALSE;
    if (has_vbases) {
        extern gcc_gen_be_error_t gcc_gen_be_emit_vtt(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT;
        struct gcc_jit_lvalue *vtt_lval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_vtt(parent_class, &vtt_lval));
    }

    int primary_param_cnt = (int)gcc_jit_function_get_param_count(primary_func);
    int num_params = primary_param_cnt;
    a_param_type_ptr ptp = (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info)
        ? rout->type->variant.routine.extra_info->param_type_list
        : NULL;
    if (num_params == 0) {
        for (a_param_type_ptr p = ptp; p != NULL; p = p->next) {
            num_params++;
        }
    }

    int c2_param_cnt = has_vbases ? (num_params + 1) : num_params;
    gcc_jit_param **c2_params = NULL;
    gcc_jit_rvalue **c2_args = NULL;
    if (c2_param_cnt > 0) {
        c2_params = (gcc_jit_param **)malloc((size_t)c2_param_cnt * sizeof(gcc_jit_param *));
        c2_args = (gcc_jit_rvalue **)malloc((size_t)(num_params > 0 ? num_params : 1) * sizeof(gcc_jit_rvalue *));
        if (!c2_params || !c2_args) {
            if (c2_params) free(c2_params);
            if (c2_args) free(c2_args);
            return GCC_GEN_BE_ERROR_OOM;
        }
        a_param_type_ptr p = ptp;
        int c2_idx = 0;
        for (int idx = 0; idx < num_params; ++idx) {
            gcc_jit_type *pt = void_ptr_type;
            if (idx < primary_param_cnt) {
                pt = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(primary_func, idx)));
            } else if (p && p->type) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(p->type, &pt));
            }
            char pname[32];
            snprintf(pname, sizeof(pname), "arg_%d", idx);
            c2_params[c2_idx] = gcc_jit_context_new_param(ctx, NULL, pt ? pt : void_ptr_type, pname);
            c2_args[idx] = gcc_jit_param_as_rvalue(c2_params[c2_idx]);
            c2_idx++;
            if (idx == 0 && has_vbases) {
                gcc_jit_type *vtt_parm_t = gcc_jit_type_get_pointer(void_ptr_type);
                c2_params[c2_idx++] = gcc_jit_context_new_param(ctx, NULL, vtt_parm_t, "__vtt_parm");
            }
            if (p) p = p->next;
        }
        if (num_params == 0 && has_vbases) {
            gcc_jit_type *vtt_parm_t = gcc_jit_type_get_pointer(void_ptr_type);
            c2_params[0] = gcc_jit_context_new_param(ctx, NULL, vtt_parm_t, "__vtt_parm");
        }
    }

    gcc_jit_function *base_clone = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, base_name, c2_param_cnt, c2_params, 0);
    if (p_gcc_jit_function_add_attribute) {
        p_gcc_jit_function_add_attribute(base_clone, GCC_JIT_FN_ATTRIBUTE_WEAK);
    }
    gcc_jit_block *b_entry = gcc_jit_function_new_block(base_clone, "entry");
    int call_cnt = (int)gcc_jit_function_get_param_count(primary_func);
    if (call_cnt == num_params) {
        gcc_jit_block_add_eval(b_entry, NULL, gcc_jit_context_new_call(ctx, NULL, primary_func, num_params, c2_args));
    }
    gcc_jit_block_end_with_void_return(b_entry, NULL);

    if (c2_params) free(c2_params);
    if (c2_args) free(c2_args);
    if (out_base_clone) *out_base_clone = base_clone;

    /* For destructors, also synthesize the D0 deleting destructor clone */
    if (rout->special_kind == sfk_destructor) {
        char d0_name[256];
        GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name(rout, "D0", d0_name, sizeof(d0_name)));
        gcc_jit_type *this_param_type = void_ptr_type;
        if (call_cnt >= 1) {
            this_param_type = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(primary_func, 0)));
        } else if (ptp && ptp->type) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(ptp->type, &this_param_type));
        }
        gcc_jit_param *d0_this = gcc_jit_context_new_param(ctx, NULL, this_param_type, "this_ptr");
        gcc_jit_function *d0_clone = gcc_jit_context_new_function(
            ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, d0_name, 1, &d0_this, 0);
        if (p_gcc_jit_function_add_attribute) {
            p_gcc_jit_function_add_attribute(d0_clone, GCC_JIT_FN_ATTRIBUTE_WEAK);
        }
        gcc_jit_block *d0_entry = gcc_jit_function_new_block(d0_clone, "entry");
        gcc_jit_rvalue *this_rval = gcc_jit_param_as_rvalue(d0_this);
        if (call_cnt == 1) {
            gcc_jit_block_add_eval(d0_entry, NULL, gcc_jit_context_new_call(ctx, NULL, primary_func, 1, &this_rval));
        }
        gcc_jit_rvalue *void_this = gcc_jit_context_new_cast(ctx, NULL, this_rval, void_ptr_type);
        size_t cls_size = (parent_class && parent_class->size > 0) ? (size_t)parent_class->size : 0;
        size_t cls_align = (parent_class && parent_class->alignment > 0) ? (size_t)parent_class->alignment : 0;
        gcc_jit_type *size_t_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
        if (cls_align > 16 && cls_size > 0) {
            gcc_jit_param *del_p[3];
            del_p[0] = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
            del_p[1] = gcc_jit_context_new_param(ctx, NULL, size_t_type, "size");
            del_p[2] = gcc_jit_context_new_param(ctx, NULL, size_t_type, "align");
            gcc_jit_function *op_del_align = gcc_jit_context_new_function(
                ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "_ZdlPvmSt11align_val_t", 3, del_p, 0);
            gcc_jit_rvalue *del_args[3];
            del_args[0] = void_this;
            del_args[1] = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)cls_size);
            del_args[2] = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)cls_align);
            gcc_jit_block_add_eval(d0_entry, NULL, gcc_jit_context_new_call(ctx, NULL, op_del_align, 3, del_args));
        } else if (cls_size > 0) {
            gcc_jit_param *del_p[2];
            del_p[0] = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
            del_p[1] = gcc_jit_context_new_param(ctx, NULL, size_t_type, "size");
            gcc_jit_function *op_del_sized = gcc_jit_context_new_function(
                ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "_ZdlPvm", 2, del_p, 0);
            gcc_jit_rvalue *del_args[2];
            del_args[0] = void_this;
            del_args[1] = gcc_jit_context_new_rvalue_from_long(ctx, size_t_type, (long)cls_size);
            gcc_jit_block_add_eval(d0_entry, NULL, gcc_jit_context_new_call(ctx, NULL, op_del_sized, 2, del_args));
        } else {
            gcc_jit_param *del_p = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "ptr");
            gcc_jit_function *op_del = gcc_jit_context_new_function(
                ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "_ZdlPv", 1, &del_p, 0);
            gcc_jit_block_add_eval(d0_entry, NULL, gcc_jit_context_new_call(ctx, NULL, op_del, 1, &void_this));
        }
        gcc_jit_block_end_with_void_return(d0_entry, NULL);
        if (out_deleting_clone) *out_deleting_clone = d0_clone;
    }

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_format_comdat_section_name(
    const char *symbol_name,
    a_boolean is_function,
    a_boolean is_readonly_data,
    int object_format,
    char *out_section,
    size_t buf_cap,
    const char **out_linkage_attr) GCC_GEN_BE_NOEXCEPT {
    if (!symbol_name || symbol_name[0] == '\0' || !out_section || buf_cap == 0 ||
        !out_linkage_attr || object_format < 0 || object_format > 2) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    out_section[0] = '\0';
    *out_linkage_attr = NULL;

    int written = 0;
    if (object_format == 0) {
        /* ELF .gnu.linkonce.{t,r,d}.<symbol> */
        const char *kind_ch = is_function ? "t" : (is_readonly_data ? "r" : "d");
        written = snprintf(out_section, buf_cap, ".gnu.linkonce.%s.%s", kind_ch, symbol_name);
        *out_linkage_attr = "weak";
    } else if (object_format == 1) {
        /* Mach-O coalesced section */
        const char *macho_sec = is_function
            ? "__TEXT,__textcoal_nt"
            : (is_readonly_data ? "__DATA,__const_coal" : "__DATA,__datacoal_nt");
        written = snprintf(out_section, buf_cap, "%s", macho_sec);
        *out_linkage_attr = "weak_definition";
    } else {
        /* COFF / PE COMDAT section */
        const char *coff_sec = is_function
            ? ".text$mn"
            : (is_readonly_data ? ".rdata$r" : ".data$r");
        written = snprintf(out_section, buf_cap, "%s", coff_sec);
        *out_linkage_attr = "selectany";
    }
    if (written < 0 || (size_t)written >= buf_cap) {
        return GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_apply_comdat_linkage(
    struct gcc_jit_lvalue *global,
    const char *symbol_name,
    a_boolean is_readonly_data) GCC_GEN_BE_NOEXCEPT {
    if (!global || !symbol_name || symbol_name[0] == '\0') {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    char sec_buf[256];
    const char *attr = NULL;
#if defined(_WIN32)
    int obj_fmt = 2;
#elif defined(__APPLE__) && defined(__MACH__)
    int obj_fmt = 1;
#else
    int obj_fmt = 0;
#endif
    GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name(
        symbol_name, FALSE, is_readonly_data, obj_fmt, sec_buf, sizeof(sec_buf), &attr));
    if (p_gcc_jit_lvalue_set_link_section && obj_fmt != 1) {
        p_gcc_jit_lvalue_set_link_section(global, sec_buf);
    }
    if (p_gcc_jit_lvalue_add_string_attribute && attr) {
        p_gcc_jit_lvalue_add_string_attribute(global, (enum gcc_jit_variable_attribute)0, "default");
    }
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */