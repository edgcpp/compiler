/**
 * @file gcc_gen_be_decl.c
 * @brief Implementation of declaration lowering for the GCC backend.
 * @details Lowers EDG global/local variables, function declarations/bodies, `C1`/`C2`/`D0`/`D1`/`D2`
 * constructor/destructor variants, TU-local global initializer blocks, and weak/vague linkage.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_omp.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_dtor.h"
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_file.h"
#include "il_read.h"
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#include "expr.h"
#include <libgccjit.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Unwraps any leading `tk_typeref` nodes from a frontend type pointer.
 * @details Advances through `tp->variant.typeref.type` while `tp` is non-null and `tk_typeref`.
 * @param[in] tp Frontend `a_type_ptr` (may be NULL).
 * @param[out] out_tp Non-null pointer receiving the unwrapped `a_type_ptr` (or NULL).
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t unwrap_decl_type(
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
 * @brief Loads on-disk IL memory region if needed and retrieves a defined routine's scope.
 * @details Sets `*out_scope` to `NULL` when `rout->function_def_number == NULL_function_def_number`.
 * @param[in] rout Non-null frontend routine pointer.
 * @param[out] out_scope Non-null pointer receiving `scope_for_routine_or_null(rout)` (or NULL).
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t resolve_routine_def_scope(
    a_routine_ptr rout,
    a_scope_ptr *out_scope) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_scope = NULL;
    if (rout->function_def_number != NULL_function_def_number) {
#if IL_SHOULD_BE_WRITTEN_TO_FILE
        a_memory_region_number rnum = mem_region_for_routine(rout);
        if (rnum != NULL_region_number && mem_region_table && mem_region_table[rnum] == NULL && f_il_output != NULL) {
            read_memory_region(rnum);
        }
#endif
        *out_scope = scope_for_routine_or_null(rout);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Allocates a TU-unique disambiguated symbol name `<base>_<ptr>` on the heap.
 * @details Frees `*io_owned_buf` on both success and failure before replacing it when needed.
 * @param[in] base_name Current symbol name.
 * @param[in] entity_ptr Disambiguating IL node address.
 * @param[in,out] io_owned_buf Pointer to caller's optional heap-owned name buffer.
 * @param[out] out_name Receives the newly allocated name pointer.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t uniquify_symbol_name(
    const char *base_name,
    const void *entity_ptr,
    char **io_owned_buf,
    const char **out_name) GCC_GEN_BE_NOEXCEPT {
    size_t cap = strlen(base_name) + 32U;
    char *u_name = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_alloc(cap, (void **)&u_name);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(*io_owned_buf);
        *io_owned_buf = NULL;
        return err;
    }
    snprintf(u_name, cap, "%s_%p", base_name, entity_ptr);
    free(*io_owned_buf);
    *io_owned_buf = u_name;
    *out_name = u_name;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Mangles `rout` with dynamically resizing heap buffers up to 64 KiB.
 * @details Retries `gcc_gen_be_mangle_function_name` when `GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW` is returned.
 * @param[in] rout Frontend routine pointer.
 * @param[in] variant_code Optional Itanium ctor/dtor variant (`"C1"`, `"C2"`, `"D0"`, `"D1"`, `"D2"`, or NULL).
 * @param[out] out_mangled Receives newly allocated mangled string on `GCC_GEN_BE_SUCCESS`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t alloc_mangled_routine_name(
    a_routine_ptr rout,
    const char *variant_code,
    char **out_mangled) GCC_GEN_BE_NOEXCEPT {
    extern gcc_gen_be_error_t gcc_gen_be_mangle_function_name(
        a_routine_ptr r, const char *vcode, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT;
    *out_mangled = NULL;
    size_t cap = 512U;
    for (;;) {
        char *buf = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_alloc(cap, (void **)&buf));
        gcc_gen_be_error_t merr = gcc_gen_be_mangle_function_name(rout, variant_code, buf, cap);
        if (merr == GCC_GEN_BE_SUCCESS && buf[0] != '\0') {
            *out_mangled = buf;
            return GCC_GEN_BE_SUCCESS;
        }
        free(buf);
        if (merr == GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW && cap < 65536U) {
            cap *= 2U;
            continue;
        }
        return (merr != GCC_GEN_BE_SUCCESS) ? merr : GCC_GEN_BE_ERROR_INTERNAL;
    }
}

/**
 * @brief Computes a deterministic FNV-1a hash of the active primary source filename for TU-unique internal labels.
 * @details Ensures local assembler labels in `.mod_init_func` / `.init_array` are unique per TU.
 * @param[out] out_hash Pointer receiving the computed 32-bit hash.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t compute_tu_source_hash(
    unsigned long *out_hash) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
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

/**
 * @brief Dynamically mangles a C++ namespace-scoped or static-data-member variable when canonical mangling is unavailable.
 * @details Formats `_ZN<enclosing_scopes><len><name>E` on the heap when `var` belongs to a namespace or class scope.
 * @param[in] var The frontend variable node (non-null with non-null `source_corresp.name`).
 * @param[out] out_mangled Non-null pointer receiving a newly allocated string (or NULL if unmangled C name applies).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on allocation failure.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t mangle_scoped_variable_fallback(
    a_variable_ptr var,
    char **out_mangled) GCC_GEN_BE_NOEXCEPT {
    *out_mangled = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (var->source_corresp.name[0] == '\0' || strncmp(var->source_corresp.name, "_Z", 2) == 0) {
        return GCC_GEN_BE_SUCCESS;
    }
    a_scope_ptr pscope = var->source_corresp.parent_scope;
    if (!pscope || (pscope->kind != sck_namespace && pscope->kind != sck_class_struct_union)) {
        return GCC_GEN_BE_SUCCESS;
    }

    a_scope_ptr chain[64];
    int depth = 0;
    for (a_scope_ptr cur = pscope; cur != NULL && cur->kind != sck_file && depth < 64; cur = cur->parent) {
        chain[depth++] = cur;
    }
    size_t cap = strlen(var->source_corresp.name) + 64;
    for (int i = 0; i < depth; ++i) {
        if (chain[i]->kind == sck_namespace && chain[i]->variant.assoc_namespace &&
            chain[i]->variant.assoc_namespace->source_corresp.name) {
            cap += strlen(chain[i]->variant.assoc_namespace->source_corresp.name) + 16;
        } else if (chain[i]->kind == sck_class_struct_union && chain[i]->variant.assoc_type &&
                   chain[i]->variant.assoc_type->source_corresp.name) {
            cap += strlen(chain[i]->variant.assoc_type->source_corresp.name) + 16;
        } else {
            cap += 24;
        }
    }
    char *buf = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(cap, (void **)&buf));
    size_t pos = (size_t)snprintf(buf, cap, "_ZN");
    for (int i = depth - 1; i >= 0; --i) {
        a_scope_ptr sc = chain[i];
        if (sc->kind == sck_namespace && sc->variant.assoc_namespace) {
            a_const_char *ns_name = sc->variant.assoc_namespace->source_corresp.name;
            if (ns_name && ns_name[0] != '\0') {
                pos += (size_t)snprintf(buf + pos, cap - pos, "%lu%s",
                                        (unsigned long)strlen(ns_name), ns_name);
            } else {
                pos += (size_t)snprintf(buf + pos, cap - pos, "12_GLOBAL__N_1");
            }
        } else if (sc->kind == sck_class_struct_union && sc->variant.assoc_type &&
                   sc->variant.assoc_type->source_corresp.name) {
            a_const_char *cn = sc->variant.assoc_type->source_corresp.name;
            pos += (size_t)snprintf(buf + pos, cap - pos, "%lu%s",
                                    (unsigned long)strlen(cn), cn);
        }
    }
    snprintf(buf + pos, cap - pos, "%lu%sE",
             (unsigned long)strlen(var->source_corresp.name), var->source_corresp.name);
    *out_mangled = buf;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves or creates the TU-local global dynamic initialization block.
 * @details Lazily creates a `GCC_JIT_FUNCTION_INTERNAL` `__edg_global_init_<hash>_<seq>` function
 * registered in `__DATA,__mod_init_func` (Mach-O), `.CRT$XCU` (COFF), or `.init_array` (ELF).
 * @param[out] out_block A pointer to receive the `gcc_jit_block`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
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
        
        unsigned long tu_hash = 0UL;
        GCC_GEN_BE_CHECK(compute_tu_source_hash(&tu_hash));
        unsigned long seq = ++state->tu_sequence_id;
        char init_fn_name[96];
        snprintf(init_fn_name, sizeof(init_fn_name), "__edg_global_init_%08lx_%lu", tu_hash, seq);

        gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        state->global_ctor_func = gcc_jit_context_new_function(
            ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, init_fn_name, 0, NULL, 0);
        if (p_gcc_jit_function_add_attribute) {
            p_gcc_jit_function_add_attribute(state->global_ctor_func, GCC_JIT_FN_ATTRIBUTE_USED);
        }
        state->global_ctor_block = gcc_jit_function_new_block(state->global_ctor_func, "entry");
        
#if defined(__APPLE__) && defined(__MACH__)
        char asm_buf[256];
        snprintf(asm_buf, sizeof(asm_buf),
                 ".section __DATA,__mod_init_func,mod_init_funcs\n"
                 ".p2align 3\n"
                 ".quad _%s\n"
                 ".text\n",
                 init_fn_name);
        gcc_jit_context_add_top_level_asm(ctx, NULL, asm_buf);
#else
        gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
        gcc_jit_rvalue *func_addr = gcc_jit_function_get_address(state->global_ctor_func, NULL);
        gcc_jit_rvalue *func_ptr = gcc_jit_context_new_cast(ctx, NULL, func_addr, void_ptr_type);
        char entry_name[128];
        snprintf(entry_name, sizeof(entry_name), "%s_entry", init_fn_name);
        gcc_jit_lvalue *init_array_entry = gcc_jit_context_new_global(
            ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, void_ptr_type, entry_name);
        gcc_jit_global_set_initializer_rvalue(init_array_entry, func_ptr);
        if (p_gcc_jit_lvalue_set_link_section) {
#if defined(_WIN32)
            p_gcc_jit_lvalue_set_link_section(init_array_entry, ".CRT$XCU");
#else
            p_gcc_jit_lvalue_set_link_section(init_array_entry, ".init_array");
#endif
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
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_has_offset = FALSE;
    if (con->kind == ck_address && con->variant.address.offset != 0) {
        if (con->variant.address.kind == abk_variable && con->variant.address.variant.variable) {
            a_variable_ptr var = con->variant.address.variant.variable;
            a_type_ptr vtp = NULL;
            GCC_GEN_BE_CHECK(unwrap_decl_type(var->type, &vtp));
            long off = (long)con->variant.address.offset;
            if (vtp && vtp->kind == tk_array && vtp->variant.array.element_type) {
                a_type_ptr etp = NULL;
                GCC_GEN_BE_CHECK(unwrap_decl_type(vtp->variant.array.element_type, &etp));
                long elem_sz = etp ? (long)etp->size : 0;
                if (elem_sz > 0 && (off % elem_sz) == 0) {
                    return GCC_GEN_BE_SUCCESS;
                }
            } else if (vtp && (vtp->kind == tk_struct || vtp->kind == tk_class)) {
                for (a_field_ptr fld = vtp->variant.class_struct_union.field_list; fld != NULL; fld = fld->next) {
                    if ((long)fld->offset == off) {
                        return GCC_GEN_BE_SUCCESS;
                    }
                    a_type_ptr ftp = NULL;
                    GCC_GEN_BE_CHECK(unwrap_decl_type(fld->type, &ftp));
                    if (ftp && ftp->kind == tk_array && ftp->variant.array.element_type &&
                        (long)fld->offset <= off && off < (long)fld->offset + (long)ftp->size) {
                        a_type_ptr fetp = NULL;
                        GCC_GEN_BE_CHECK(unwrap_decl_type(ftp->variant.array.element_type, &fetp));
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

  a_type_ptr stripped_vtp = NULL;
  GCC_GEN_BE_CHECK(unwrap_decl_type(var->type, &stripped_vtp));
  gcc_jit_type *var_type = NULL;
  if (var->storage_class == sc_extern && var->init_kind == initk_none &&
      stripped_vtp && stripped_vtp->kind == tk_array &&
      !stripped_vtp->variant.array.is_variable_size_array &&
      stripped_vtp->variant.array.variant.number_of_elements == 0) {
      const char *raw_vname = var->source_corresp.name ? var->source_corresp.name : "";
      if (strncmp(raw_vname, "_ZTV", 4) == 0) {
          gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
          var_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_t, 8);
      } else {
          var_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
      }
  } else {
      err = gcc_gen_be_lower_type(var->type, &var_type);
      if (err != GCC_GEN_BE_SUCCESS) return err;
  }
  if (var_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
      var_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
  }

  char auto_name[128];
  char *heap_var_name = NULL;
  const char *name = var->source_corresp.name;
  if (!name) {
      snprintf(auto_name, sizeof(auto_name), "__edg_anon_var_%p", (void *)var);
      name = auto_name;
  } else {
      a_scope_ptr v_pscope = var->source_corresp.parent_scope;
      a_boolean is_cxx_var = (
          var->source_corresp.name_linkage == (a_name_linkage_kind)nlk_cplusplus_external ||
          var->is_template_variable ||
          (var->source_corresp.name_linkage != (a_name_linkage_kind)nlk_external &&
           v_pscope && (v_pscope->kind == sck_namespace || v_pscope->kind == sck_class_struct_union))) ? TRUE : FALSE;
      if (gcc_gen_be_singleton_state.canonical_mangle_var &&
          is_cxx_var &&
          var->storage_class != sc_static &&
          strncmp(name, "_Z", 2) != 0 &&
          strncmp(name, "__", 2) != 0) {
          a_const_char *canon_v = gcc_gen_be_singleton_state.canonical_mangle_var(var);
          if (canon_v && canon_v[0] != '\0') {
              size_t clen = strlen(canon_v);
              GCC_GEN_BE_CHECK(gcc_gen_be_alloc(clen + 1, (void **)&heap_var_name));
              memcpy(heap_var_name, canon_v, clen + 1);
              name = heap_var_name;
          }
      } else if (!gcc_gen_be_singleton_state.canonical_mangle_var) {
          GCC_GEN_BE_CHECK(mangle_scoped_variable_fallback(var, &heap_var_name));
          if (heap_var_name) {
              name = heap_var_name;
          }
      }

      void *existing_named = NULL;
      err = cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)name, &existing_named);
      if (err != GCC_GEN_BE_SUCCESS) {
          free(heap_var_name);
          return err;
      }
      if (existing_named) {
          void *prev_global = NULL;
          err = cache_lookup(GCC_GEN_BE_CACHE_VAR, existing_named, &prev_global);
          if (err != GCC_GEN_BE_SUCCESS) {
              free(heap_var_name);
              return err;
          }
          if (prev_global) {
              if (var->storage_class == sc_static && strncmp(name, "__", 2) != 0) {
                  GCC_GEN_BE_CHECK(uniquify_symbol_name(name, var, &heap_var_name, &name));
              } else {
                  err = cache_insert(GCC_GEN_BE_CACHE_VAR, var, prev_global);
                  if (err != GCC_GEN_BE_SUCCESS) {
                      free(heap_var_name);
                      return err;
                  }
                  if (var->init_kind == initk_static && var->initializer.constant) {
                      a_constant_ptr init_con = var->initializer.constant;
                      gcc_jit_rvalue *init_rval = NULL;
                      extern gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;
                      err = gcc_gen_be_lower_constant_rvalue(init_con, var_type, &init_rval);
                      if (err != GCC_GEN_BE_SUCCESS) {
                          free(heap_var_name);
                          return err;
                      }
                      if (init_rval) {
                          gcc_jit_global_set_initializer_rvalue((gcc_jit_lvalue *)prev_global, init_rval);
                      }
                  }
                  free(heap_var_name);
                  *out_lval = (gcc_jit_lvalue *)prev_global;
                  return GCC_GEN_BE_SUCCESS;
              }
          } else if (strncmp(name, "_ZTV", 4) == 0 ||
                     strncmp(name, "_ZTT", 4) == 0 ||
                     strncmp(name, "_ZTC", 4) == 0 ||
                     strncmp(name, "_ZTI", 4) == 0 ||
                     strncmp(name, "_ZTS", 4) == 0 ||
                     strncmp(name, "_ZZ", 3) == 0) {
              err = cache_insert(GCC_GEN_BE_CACHE_VAR, var, existing_named);
              free(heap_var_name);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              *out_lval = (gcc_jit_lvalue *)existing_named;
              return GCC_GEN_BE_SUCCESS;
          }
      } else {
          err = cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)name, (void *)var);
          if (err != GCC_GEN_BE_SUCCESS) {
              free(heap_var_name);
              return err;
          }
      }
  }

  a_boolean is_anon_var = FALSE;
  err = gcc_gen_be_is_scope_anonymous(var->source_corresp.parent_scope, &is_anon_var);
  if (err != GCC_GEN_BE_SUCCESS) {
      free(heap_var_name);
      return err;
  }

  enum gcc_jit_global_kind linkage;
  if (!var->source_corresp.name) {
      linkage = GCC_JIT_GLOBAL_INTERNAL;
  } else if (var->storage_class == sc_extern && var->init_kind == initk_none) {
      linkage = GCC_JIT_GLOBAL_IMPORTED;
  } else if (var->storage_class == sc_static || is_anon_var) {
      linkage = GCC_JIT_GLOBAL_INTERNAL;
  } else {
      linkage = GCC_JIT_GLOBAL_EXPORTED;
  }

  gcc_jit_location *loc = NULL;
  err = gcc_gen_be_get_location(&var->source_corresp.decl_position, &loc);
  if (err != GCC_GEN_BE_SUCCESS) {
      free(heap_var_name);
      return err;
  }

  gcc_jit_lvalue *global = gcc_jit_context_new_global(ctx, loc, linkage, var_type, name);
  err = cache_insert(GCC_GEN_BE_CACHE_VAR, var, global);
  if (err != GCC_GEN_BE_SUCCESS) {
      free(heap_var_name);
      return err;
  }

  a_boolean is_vague_global = (var->is_inline || var->is_weak || var->is_template_variable ||
      strncmp(name, "_ZTV", 4) == 0 || strncmp(name, "_ZTT", 4) == 0 ||
      strncmp(name, "_ZTC", 4) == 0 || strncmp(name, "_ZTI", 4) == 0 ||
      strncmp(name, "_ZTS", 4) == 0 || strncmp(name, "_ZGV", 4) == 0 ||
      strncmp(name, "_ZZ", 3) == 0) ? TRUE : FALSE;
  if (is_vague_global && linkage == GCC_JIT_GLOBAL_EXPORTED) {
      err = gcc_gen_be_apply_comdat_linkage(global, name, FALSE);
      if (err == GCC_GEN_BE_SUCCESS) {
          err = gcc_gen_be_mark_global_vague_linkage(ctx, global, name);
      }
      if (err != GCC_GEN_BE_SUCCESS) {
          free(heap_var_name);
          return err;
      }
  }
  extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;
  err = gcc_gen_be_emit_variable_debug_info(var, global);
  if (err != GCC_GEN_BE_SUCCESS) {
      free(heap_var_name);
      return err;
  }

#if defined(__APPLE__)
  if (var->is_thread_local) {
      size_t nlen = strlen(name);
      char *tls_fn_name = NULL;
      err = gcc_gen_be_alloc(nlen + 32U, (void **)&tls_fn_name);
      if (err != GCC_GEN_BE_SUCCESS) { free(heap_var_name); return err; }
      snprintf(tls_fn_name, nlen + 32U, "__edg_darwin_tls_%s", name);
      gcc_jit_type *var_ptr_t = gcc_jit_type_get_pointer(var_type);
      if (linkage == GCC_JIT_GLOBAL_IMPORTED) {
          gcc_jit_function *tls_fn = gcc_jit_context_new_function(
              ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, var_ptr_t, tls_fn_name, 0, NULL, 0);
          free(tls_fn_name);
          err = cache_insert(GCC_GEN_BE_CACHE_FUNC, var, tls_fn);
          if (err != GCC_GEN_BE_SUCCESS) { free(heap_var_name); return err; }
      } else {
          {
              gcc_jit_type *ulong_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG);
              gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
              gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
              gcc_jit_type *size_t_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
              char *key_name = NULL;
              err = gcc_gen_be_alloc(nlen + 32U, (void **)&key_name);
              if (err != GCC_GEN_BE_SUCCESS) { goto darwin_tls_fail; }
              snprintf(key_name, nlen + 32U, "__edg_tls_key_%s", name);
              gcc_jit_lvalue *key_gv = gcc_jit_context_new_global(
                  ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, ulong_t, key_name);
              free(key_name);
              gcc_jit_global_set_initializer_rvalue(key_gv, gcc_jit_context_zero(ctx, ulong_t));

              void *c_pkc = NULL;
              void *c_pgs = NULL;
              void *c_pss = NULL;
              void *c_mal = NULL;
              void *c_fre = NULL;
              void *c_tlv = NULL;
              GCC_GEN_BE_CHECK_CLEANUP(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"pthread_key_create", &c_pkc), err, darwin_tls_fail);
              GCC_GEN_BE_CHECK_CLEANUP(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"pthread_getspecific", &c_pgs), err, darwin_tls_fail);
              GCC_GEN_BE_CHECK_CLEANUP(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"pthread_setspecific", &c_pss), err, darwin_tls_fail);
              GCC_GEN_BE_CHECK_CLEANUP(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__tls_malloc", &c_mal), err, darwin_tls_fail);
              GCC_GEN_BE_CHECK_CLEANUP(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__tls_free", &c_fre), err, darwin_tls_fail);
              GCC_GEN_BE_CHECK_CLEANUP(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"_tlv_atexit", &c_tlv), err, darwin_tls_fail);
              gcc_jit_function *fn_pkc = (gcc_jit_function *)c_pkc;
              if (!fn_pkc) {
                  gcc_jit_param *pkc_p[2] = {
                      gcc_jit_context_new_param(ctx, NULL, gcc_jit_type_get_pointer(ulong_t), "key"),
                      gcc_jit_context_new_param(ctx, NULL, vp_t, "dtor")
                  };
                  fn_pkc = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_t, "pthread_key_create", 2, pkc_p, 0);
                  GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"pthread_key_create", fn_pkc), err, darwin_tls_fail);
              }
              gcc_jit_function *fn_pgs = (gcc_jit_function *)c_pgs;
              if (!fn_pgs) {
                  gcc_jit_param *pgs_p = gcc_jit_context_new_param(ctx, NULL, ulong_t, "key");
                  fn_pgs = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, vp_t, "pthread_getspecific", 1, &pgs_p, 0);
                  GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"pthread_getspecific", fn_pgs), err, darwin_tls_fail);
              }
              gcc_jit_function *fn_pss = (gcc_jit_function *)c_pss;
              if (!fn_pss) {
                  gcc_jit_param *pss_p[2] = {
                      gcc_jit_context_new_param(ctx, NULL, ulong_t, "key"),
                      gcc_jit_context_new_param(ctx, NULL, vp_t, "val")
                  };
                  fn_pss = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_t, "pthread_setspecific", 2, pss_p, 0);
                  GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"pthread_setspecific", fn_pss), err, darwin_tls_fail);
              }
              gcc_jit_function *fn_mal = (gcc_jit_function *)c_mal;
              if (!fn_mal) {
                  gcc_jit_param *mal_p = gcc_jit_context_new_param(ctx, NULL, size_t_t, "sz");
                  fn_mal = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, vp_t, "malloc", 1, &mal_p, 0);
                  GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__tls_malloc", fn_mal), err, darwin_tls_fail);
              }
              gcc_jit_function *fn_fre = (gcc_jit_function *)c_fre;
              if (!fn_fre) {
                  gcc_jit_param *fre_p = gcc_jit_context_new_param(ctx, NULL, vp_t, "ptr");
                  fn_fre = gcc_jit_context_new_function(
                      ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "free", 1, &fre_p, 0);
                  GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__tls_free", fn_fre), err, darwin_tls_fail);
              }
              gcc_jit_function *fn_tlv = (gcc_jit_function *)c_tlv;
              if (!fn_tlv) {
                  gcc_jit_param *tlv_p[2] = {
                      gcc_jit_context_new_param(ctx, NULL, vp_t, "dtor"),
                      gcc_jit_context_new_param(ctx, NULL, vp_t, "obj")
                  };
                  fn_tlv = gcc_jit_context_new_function(
                      ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "_tlv_atexit", 2, tlv_p, 0);
                  GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"_tlv_atexit", fn_tlv), err, darwin_tls_fail);
              }
              void *c_act = NULL;
              GCC_GEN_BE_CHECK_CLEANUP(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__edg_activate_tlv", &c_act), err, darwin_tls_fail);
              gcc_jit_function *fn_act = (gcc_jit_function *)c_act;
              if (!fn_act) {
                  unsigned long tu_hash = 0UL;
                  GCC_GEN_BE_CHECK_CLEANUP(compute_tu_source_hash(&tu_hash), err, darwin_tls_fail);
                  gcc_gen_be_context_t *be_st_tlv = NULL;
                  GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_get_state(&be_st_tlv), err, darwin_tls_fail);
                  unsigned long tlv_seq = ++be_st_tlv->tu_sequence_id;
                  char act_sym[96];
                  snprintf(act_sym, sizeof(act_sym), "__edg_act_tlv_%08lx_%lu", tu_hash, tlv_seq);
                  char tlv_asm[512];
#if defined(__aarch64__)
                  snprintf(tlv_asm, sizeof(tlv_asm),
                           ".section __DATA,__thread_bss,thread_local_zerofill\n"
                           ".p2align 2\n"
                           "L_%s$tlv$init:\n"
                           ".space 4\n"
                           ".section __DATA,__thread_vars,thread_local_variables\n"
                           ".p2align 3\n"
                           "L_%s$tlv:\n"
                           ".quad __tlv_bootstrap\n"
                           ".quad 0\n"
                           ".quad L_%s$tlv$init\n"
                           ".text\n"
                           ".p2align 2\n"
                           "_%s:\n"
                           "stp x29, x30, [sp, #-16]!\n"
                           "adrp x0, L_%s$tlv@PAGE\n"
                           "add x0, x0, L_%s$tlv@PAGEOFF\n"
                           "ldr x8, [x0]\n"
                           "blr x8\n"
                           "ldp x29, x30, [sp], #16\n"
                           "ret\n",
                           act_sym, act_sym, act_sym, act_sym, act_sym, act_sym);
#else
                  snprintf(tlv_asm, sizeof(tlv_asm),
                           ".section __DATA,__thread_bss,thread_local_zerofill\n"
                           ".p2align 2\n"
                           "L_%s$tlv$init:\n"
                           ".space 4\n"
                           ".section __DATA,__thread_vars,thread_local_variables\n"
                           ".p2align 3\n"
                           "L_%s$tlv:\n"
                           ".quad __tlv_bootstrap\n"
                           ".quad 0\n"
                           ".quad L_%s$tlv$init\n"
                           ".text\n"
                           "_%s:\n"
                           "pushq %%rbp\n"
                           "movq %%rsp, %%rbp\n"
                           "leaq L_%s$tlv(%%rip), %%rdi\n"
                           "callq *(%%rdi)\n"
                           "popq %%rbp\n"
                           "retq\n",
                           act_sym, act_sym, act_sym, act_sym, act_sym);
#endif
                  gcc_jit_context_add_top_level_asm(ctx, NULL, tlv_asm);
                  fn_act = gcc_jit_context_new_function(
                      ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), act_sym, 0, NULL, 0);
                  GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__edg_activate_tlv", fn_act), err, darwin_tls_fail);
              }

              gcc_jit_block *gctor_blk = NULL;
              GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_get_global_ctor_block(&gctor_blk), err, darwin_tls_fail);
              gcc_jit_rvalue *pkc_args[2] = {
                  gcc_jit_lvalue_get_address(key_gv, NULL),
                  gcc_jit_context_null(ctx, vp_t)
              };
              gcc_jit_block_add_eval(gctor_blk, NULL, gcc_jit_context_new_call(ctx, NULL, fn_pkc, 2, pkc_args));

              enum gcc_jit_function_kind tls_fk = (linkage == GCC_JIT_GLOBAL_INTERNAL)
                  ? GCC_JIT_FUNCTION_INTERNAL
                  : GCC_JIT_FUNCTION_EXPORTED;
              gcc_jit_function *tls_fn = gcc_jit_context_new_function(ctx, NULL, tls_fk, var_ptr_t, tls_fn_name, 0, NULL, 0);
              if (is_vague_global && tls_fk == GCC_JIT_FUNCTION_EXPORTED) {
                  GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_mark_function_vague_linkage(ctx, tls_fn, tls_fn_name), err, darwin_tls_fail);
              }
              gcc_jit_lvalue *p_loc = gcc_jit_function_new_local(tls_fn, NULL, vp_t, "tls_slot");
              gcc_jit_block *b_ent = gcc_jit_function_new_block(tls_fn, "entry");
              gcc_jit_block *b_ini = gcc_jit_function_new_block(tls_fn, "init");
              gcc_jit_block *b_ret = gcc_jit_function_new_block(tls_fn, "ret");
              gcc_jit_rvalue *kv = gcc_jit_lvalue_as_rvalue(key_gv);
              gcc_jit_block_add_assignment(b_ent, NULL, p_loc, gcc_jit_context_new_call(ctx, NULL, fn_pgs, 1, &kv));
              gcc_jit_rvalue *is_null = gcc_jit_context_new_comparison(
                  ctx, NULL, GCC_JIT_COMPARISON_EQ, gcc_jit_lvalue_as_rvalue(p_loc), gcc_jit_context_null(ctx, vp_t));
              gcc_jit_block_end_with_conditional(b_ent, NULL, is_null, b_ini, b_ret);

              unsigned long vsz = (unsigned long)stripped_vtp->size;
              if (vsz == 0UL) vsz = 8UL;
              gcc_jit_rvalue *sz_arg = gcc_jit_context_new_rvalue_from_long(ctx, size_t_t, (long)vsz);
              gcc_jit_block_add_assignment(b_ini, NULL, p_loc, gcc_jit_context_new_call(ctx, NULL, fn_mal, 1, &sz_arg));
              gcc_jit_rvalue *typed_p = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(p_loc), var_ptr_t);
              gcc_jit_block_add_assignment(b_ini, NULL, gcc_jit_rvalue_dereference(typed_p, NULL), gcc_jit_lvalue_as_rvalue(global));
              gcc_jit_rvalue *pss_args[2] = { gcc_jit_lvalue_as_rvalue(key_gv), gcc_jit_lvalue_as_rvalue(p_loc) };
              gcc_jit_block_add_eval(b_ini, NULL, gcc_jit_context_new_call(ctx, NULL, fn_pss, 2, pss_args));
              gcc_jit_block_add_eval(b_ini, NULL, gcc_jit_context_new_call(ctx, NULL, fn_act, 0, NULL));
              gcc_jit_rvalue *tlv_args[2] = {
                  gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(fn_fre, NULL), vp_t),
                  gcc_jit_lvalue_as_rvalue(p_loc)
              };
              gcc_jit_block_add_eval(b_ini, NULL, gcc_jit_context_new_call(ctx, NULL, fn_tlv, 2, tlv_args));
              gcc_jit_block_end_with_jump(b_ini, NULL, b_ret);

              gcc_jit_block_end_with_return(
                  b_ret, NULL, gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(p_loc), var_ptr_t));
              free(tls_fn_name);
              tls_fn_name = NULL;
              err = cache_insert(GCC_GEN_BE_CACHE_FUNC, var, tls_fn);
              if (err != GCC_GEN_BE_SUCCESS) { goto darwin_tls_fail; }
              goto darwin_tls_ok;
          }
      darwin_tls_fail:;
          free(tls_fn_name);
          free(heap_var_name);
          return err;
      darwin_tls_ok:;
      }
  }
  free(heap_var_name);
  heap_var_name = NULL;
#else
  free(heap_var_name);
  heap_var_name = NULL;

  if (var->is_thread_local && p_gcc_jit_lvalue_set_tls_model) {
      enum gcc_jit_tls_model tls_model = GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC;

      extern a_boolean gcc_be_fPIC;
      extern a_boolean gcc_be_fPIE;
      a_boolean pic_enabled = gcc_be_fPIC || gcc_be_fPIE;

      if (linkage == GCC_JIT_GLOBAL_INTERNAL) {
          tls_model = pic_enabled ? GCC_JIT_TLS_MODEL_LOCAL_DYNAMIC : GCC_JIT_TLS_MODEL_LOCAL_EXEC;
      } else if (linkage == GCC_JIT_GLOBAL_IMPORTED) {
          tls_model = pic_enabled ? GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC : GCC_JIT_TLS_MODEL_INITIAL_EXEC;
      }
      p_gcc_jit_lvalue_set_tls_model(global, tls_model);
  }
#endif

  a_boolean has_reloc_offset = FALSE;
  if (var->init_kind == initk_static && var->initializer.constant) {
      GCC_GEN_BE_CHECK(constant_has_non_zero_address_offset(var->initializer.constant, &has_reloc_offset));
  }

  a_boolean is_writable_const = (
      var->init_kind == initk_dynamic ||
      var->initialization_rewritten_as_assignment ||
      (stripped_vtp &&
       (stripped_vtp->kind == tk_class || stripped_vtp->kind == tk_struct) &&
       stripped_vtp->variant.class_struct_union.any_mutable_member)) ? TRUE : FALSE;

  if (var->type && var->type->kind == tk_typeref && (var->type->variant.typeref.qualifiers & TQ_CONST) &&
      !is_writable_const && var->source_corresp.name != NULL && !has_reloc_offset) {
#if defined(LIBGCCJIT_HAVE_gcc_jit_global_set_readonly)
      gcc_jit_global_set_readonly(global);
#endif
  }

  if (var->init_kind == initk_zero) {
      gcc_jit_rvalue *zero_val = NULL;
      GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(var->type, var_type, &zero_val));
      gcc_jit_global_set_initializer_rvalue(global, zero_val);
  } else if (var->init_kind == initk_static) {
      a_constant_ptr init_con = var->initializer.constant;
      if (init_con) {
          if (stripped_vtp && stripped_vtp->kind == tk_array && init_con->kind == ck_string) {
              size_t total_size = (size_t)stripped_vtp->size;
              if (total_size < (size_t)init_con->variant.string.length) {
                  total_size = (size_t)init_con->variant.string.length;
              }
              size_t alloc_sz = total_size > 0 ? total_size : 1U;
              char *padded = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_alloc(alloc_sz, (void **)&padded));
              memset(padded, 0, alloc_sz);
              if (init_con->variant.string.value) {
                  memcpy(padded, init_con->variant.string.value, (size_t)init_con->variant.string.length);
              }
              gcc_jit_global_set_initializer(global, padded, alloc_sz);
              free(padded);
          } else {
              gcc_jit_rvalue *init_rval = NULL;
              extern gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;
              gcc_jit_block *saved_blk = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&saved_blk));
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
              gcc_gen_be_error_t cst_err = gcc_gen_be_lower_constant_rvalue(init_con, var_type, &init_rval);
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_blk));
              GCC_GEN_BE_CHECK(cst_err);
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
                  if (has_reloc_offset &&
                      !(var->source_corresp.name &&
                        (strncmp(var->source_corresp.name, "_ZTV", 4) == 0 ||
                         strncmp(var->source_corresp.name, "_ZTT", 4) == 0 ||
                         strncmp(var->source_corresp.name, "_ZTI", 4) == 0))) {
                      gcc_jit_block *cblock = NULL;
                      GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblock));
                      gcc_jit_block_add_assignment(cblock, NULL, global, init_rval);
                  } else {
                      gcc_jit_global_set_initializer_rvalue(global, init_rval);
                  }
              }
          }
      } else if (var->storage_class != sc_static || strncmp(name, "__C", 3) != 0) {
          gcc_jit_rvalue *zero_val = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(var->type, var_type, &zero_val));
          gcc_jit_global_set_initializer_rvalue(global, zero_val);
      }
  } else if (var->init_kind == initk_dynamic && var->initializer.dynamic) {
      if (var->initializer.dynamic->kind != dik_none) {
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
          if (latest_ctor_block) {
              state->global_ctor_block = latest_ctor_block;
          }
          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(orig_block));
      }
      if (var->initializer.dynamic->destructor) {
          void *cached_dtor = NULL;
          GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, var->initializer.dynamic->destructor, &cached_dtor));
          if (!cached_dtor) {
              gcc_jit_function *lowered_dtor = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(var->initializer.dynamic->destructor, &lowered_dtor));
              cached_dtor = lowered_dtor;
          }
          if (cached_dtor) {
              gcc_jit_block *orig_block = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&orig_block));
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblock));
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(cblock));

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
              GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)"__dso_handle", &cached_dso_var));
              if (cached_dso_var) {
                  dso_global = (gcc_jit_lvalue *)cached_dso_var;
              } else {
                  dso_global = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_IMPORTED, void_ptr_type, "__dso_handle");
                  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)"__dso_handle", dso_global));
              }
              gcc_jit_rvalue *dso_handle = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(dso_global, NULL), void_ptr_type);

              gcc_jit_rvalue *atexit_args[] = {func_ptr, obj_ptr, dso_handle};
              gcc_jit_block_add_eval(cblock, NULL, gcc_jit_context_new_call(ctx, NULL, atexit_func, 3, atexit_args));

              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(orig_block));
          }
      }
  }

  *out_lval = global;
  return (linkage == GCC_JIT_GLOBAL_IMPORTED) ? GCC_GEN_BE_SUCCESS
                                              : gcc_gen_be_apply_variable_attributes(var, global);
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
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

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
  GCC_GEN_BE_CHECK(gcc_gen_be_get_active_coroutine_frame(&coro_ft, &coro_flval));
  if (coro_flval != NULL) {
      void *cached_coro_fld = NULL;
      GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FIELD, var, &cached_coro_fld));
      if (cached_coro_fld != NULL) {
          gcc_jit_lvalue *frame_deref = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(coro_flval), NULL);
          gcc_jit_lvalue *coro_slot = gcc_jit_lvalue_access_field(frame_deref, NULL, (gcc_jit_field *)cached_coro_fld);
          GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, coro_slot));
          *out_lval = coro_slot;
          return GCC_GEN_BE_SUCCESS;
      }
  }

  gcc_jit_type *var_type = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(var->type, &var_type));
  if (var_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
      var_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
  }

  const char *name = var->source_corresp.name;
  if (!name) name = "unnamed_local";

  gcc_jit_location *loc = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&var->source_corresp.decl_position, &loc));

  gcc_jit_lvalue *local = gcc_jit_function_new_local(func, loc, var_type, name);

  extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;
  GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(var, local));

  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, local));

  gcc_jit_block *cblock = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
  if (cblock) {
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
          gcc_jit_block_add_assignment(cblock, loc, local, zero_rval);
      }
  }

  *out_lval = local;
  return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers the body, parameter unpacking, local variables, RAII cleanups, and implicit return of a defined function.
 * @details Invoked by `gcc_gen_be_lower_function_decl` while caller-saved scope/landing-pad/EH stacks are stashed.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[in] rout Frontend routine node.
 * @param[in] r_extra Optional routine type supplement (`rout->type->variant.routine.extra_info`).
 * @param[in] frontend_ret Optional frontend return type (`rout->type->variant.routine.return_type`).
 * @param[in] def_scope Frontend function scope (`def_scope->assoc_block` non-null).
 * @param[in] func Lowered `gcc_jit_function`.
 * @param[in] ret_type Lowered `gcc_jit_type` for `func`'s return value.
 * @param[in,out] be_state Active backend singleton state.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t lower_defined_function_body_inner(
    gcc_jit_context *ctx,
    a_routine_ptr rout,
    a_routine_type_supplement_ptr r_extra,
    a_type_ptr frontend_ret,
    a_scope_ptr def_scope,
    gcc_jit_function *func,
    gcc_jit_type *ret_type,
    gcc_gen_be_context_t *be_state) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_block *entry = gcc_jit_function_new_block(func, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(entry));

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
        a_type_ptr utype = NULL;
        GCC_GEN_BE_CHECK(unwrap_decl_type(p_unpack->type, &utype));
        if (!utype) continue;
        a_boolean is_invis = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_is_passed_by_invisible_reference(utype, &is_invis));
        void *cached_p = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, p_unpack, &cached_p));
        if (!cached_p) continue;
        if (is_invis) {
            if (utype->size > 0) {
                gcc_jit_type *val_t = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(utype, &val_t));
                const char *pname = p_unpack->source_corresp.name ? p_unpack->source_corresp.name : "param_unpacked";
                gcc_jit_lvalue *unpacked_local = gcc_jit_function_new_local(func, NULL, val_t, pname);
                gcc_jit_block_add_assignment(entry, NULL, unpacked_local, gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached_p));
                GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(p_unpack, unpacked_local));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, p_unpack, unpacked_local));
            }
        } else {
            gcc_jit_type *coerced_pt = NULL;
            a_boolean needs_coercion = FALSE;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(utype, &coerced_pt, &needs_coercion));
            if (needs_coercion) {
                gcc_jit_lvalue *unpacked_local = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_unpack_coerced_Arg(
                    func, entry, utype,
                    gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached_p),
                    &unpacked_local));
                if (unpacked_local) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(p_unpack, unpacked_local));
                    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, p_unpack, unpacked_local));
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

    for (a_local_static_variable_init_ptr lsvip = def_scope->local_static_variable_inits;
         lsvip != NULL;
         lsvip = lsvip->next) {
        if (lsvip->variable && lsvip->variable->init_kind == initk_function_local) {
            lsvip->variable->init_kind = lsvip->init_kind;
            lsvip->variable->initializer = lsvip->initializer;
        }
    }

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
    GCC_GEN_BE_CHECK(gcc_gen_be_begin_function_eh(func));
    GCC_GEN_BE_CHECK(gcc_gen_be_push_scope(def_scope->lifetime));
#if !defined(EDG_GCC_BE_ISOLATED_TEST) && DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    const a_boolean use_lowered_eh_cleanups = (!C_mode() && il_header.primary_scope != NULL) ? TRUE : FALSE;
#else
    const a_boolean use_lowered_eh_cleanups = FALSE;
#endif
    if (!use_lowered_eh_cleanups && def_scope->lifetime && def_scope->lifetime->destructions) {
        a_boolean any_cleanup_reg = FALSE;
        for (a_dynamic_init_ptr d = def_scope->lifetime->destructions; d != NULL; d = d->next_in_destruction_list) {
            if (d->destructor && d->variable) {
                gcc_jit_lvalue *d_lval = NULL;
                gcc_jit_function *d_fn = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(func, d->variable, &d_lval));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(d->destructor, &d_fn));
                if (d_fn) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_register_cleanup(d_lval, d_fn));
                    any_cleanup_reg = TRUE;
                }
            }
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(entry));
        if (exceptions_enabled && any_cleanup_reg) {
            extern gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(gcc_jit_function *fn) GCC_GEN_BE_NOEXCEPT;
            GCC_GEN_BE_CHECK(gcc_gen_be_emit_scope_unwind_cleanup(func));
        }
    }
    an_exception_specification_ptr exc_spec = r_extra ? r_extra->exception_specification : NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_register_omp_pragmas_in_scope(def_scope));
    const char *saved_routine_name = be_state->active_routine_name;
    be_state->active_routine_name = rout->source_corresp.name;
    gcc_gen_be_error_t berr = gcc_gen_be_lower_routine_body(def_scope->assoc_block, func, exc_spec);
    be_state->active_routine_name = saved_routine_name;
    GCC_GEN_BE_CHECK(berr);
    GCC_GEN_BE_CHECK(gcc_gen_be_end_function_eh(func));
    GCC_GEN_BE_CHECK(gcc_gen_be_end_function_stmts(func));

    gcc_jit_block *current = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current));
    GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(current));
    if (current) {
        if (ret_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
            gcc_jit_block_end_with_void_return(current, NULL);
        } else {
            gcc_jit_rvalue *zero = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(frontend_ret, ret_type, &zero));
            if (gcc_jit_rvalue_get_type(zero) != ret_type) {
                GCC_GEN_BE_CHECK(gcc_gen_be_get_zero_rvalue(NULL, ret_type, &zero));
            }
            gcc_jit_block_end_with_return(current, NULL, zero);
        }
    }
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

  if (rout->function_def_number == NULL_function_def_number && rout->source_corresp.name) {
      const char *rname = rout->source_corresp.name;
      if (strncmp(rname, "__builtin_", 10) == 0 ||
          strncmp(rname, "__atomic_", 9) == 0 ||
          strncmp(rname, "__sync_", 7) == 0 ||
          strncmp(rname, "__c11_atomic_", 13) == 0 ||
          strncmp(rname, "__c99_", 6) == 0) {
          return GCC_GEN_BE_SUCCESS;
      }
  }
  
  a_type_ptr rout_tp = NULL;
  GCC_GEN_BE_CHECK(unwrap_decl_type(rout->type, &rout_tp));
  a_boolean is_routine_tp = (rout_tp && rout_tp->kind == tk_routine) ? TRUE : FALSE;
  a_type_ptr frontend_ret = is_routine_tp ? rout_tp->variant.routine.return_type : NULL;
  a_routine_type_supplement_ptr r_extra = is_routine_tp ? rout_tp->variant.routine.extra_info : NULL;
  a_param_type_ptr first_ptp = r_extra ? r_extra->param_type_list : NULL;

  gcc_jit_type *ret_type = NULL;
  a_boolean uses_sret = FALSE;
  a_boolean edg_ret_as_param = FALSE;
#if DO_IL_LOWERING
  if (r_extra && r_extra->value_returned_as_parameter) {
      edg_ret_as_param = TRUE;
  }
#endif
  gcc_jit_type *sret_ptr_type = NULL;
  if (is_routine_tp) {
      extern gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;
      if (!edg_ret_as_param) {
          GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(frontend_ret, &uses_sret));
      }
      if (edg_ret_as_param) {
          ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
      } else if (uses_sret) {
          ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
          gcc_jit_type *actual_ret = NULL;
          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(frontend_ret, &actual_ret));
          sret_ptr_type = gcc_jit_type_get_pointer(actual_ret);
      } else {
          GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(frontend_ret, &ret_type));
          if (frontend_ret) {
              extern gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;
              gcc_jit_type *coerced_ret = NULL;
              a_boolean ret_coerced = FALSE;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(frontend_ret, &coerced_ret, &ret_coerced));
              if (ret_coerced) {
                  ret_type = coerced_ret;
              }
          }
      }
  } else {
      ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
  }

  char auto_name[128];
  char *heap_fn_name = NULL;
  const char *name = rout->source_corresp.name;
#if NEED_NAME_MANGLING
  if (rout->source_corresp.unmangled_name_or_mangled_encoding &&
      strncmp(rout->source_corresp.unmangled_name_or_mangled_encoding, "_Z", 2) == 0) {
      name = rout->source_corresp.unmangled_name_or_mangled_encoding;
  }
#endif
  if (!name) {
      snprintf(auto_name, sizeof(auto_name), "__edg_anon_fn_%p", (void *)rout);
      name = auto_name;
  } else {
      a_scope_ptr pscope = rout->source_corresp.parent_scope;
      a_boolean should_mangle_cxx = FALSE;
      a_boolean is_extern_c = (rout->source_corresp.name_linkage == (a_name_linkage_kind)nlk_external) ? TRUE : FALSE;
      if (!is_extern_c && r_extra && r_extra->routine_name_linkage_is_explicit &&
          r_extra->routine_name_linkage == (a_name_linkage_kind)nlk_external) {
          is_extern_c = TRUE;
      }
      if (strncmp(name, "_Z", 2) != 0 && strcmp(name, "main") != 0 &&
          strncmp(name, "__", 2) != 0) {
          if (rout->source_corresp.name_linkage == (a_name_linkage_kind)nlk_cplusplus_external) {
              should_mangle_cxx = TRUE;
          } else if (!is_extern_c && pscope &&
                     (pscope->kind == sck_class_struct_union || pscope->kind == sck_namespace)) {
              should_mangle_cxx = TRUE;
          } else if (rout->is_template_function || rout->special_kind != sfk_none) {
              should_mangle_cxx = TRUE;
          }
      }

      if (should_mangle_cxx) {
          if (gcc_gen_be_singleton_state.canonical_mangle_func) {
              char *canon_fn = gcc_gen_be_singleton_state.canonical_mangle_func(rout);
              if (canon_fn && canon_fn[0] != '\0') {
                  size_t clen = strlen(canon_fn);
                  GCC_GEN_BE_CHECK(gcc_gen_be_alloc(clen + 1, (void **)&heap_fn_name));
                  memcpy(heap_fn_name, canon_fn, clen + 1);
                  name = heap_fn_name;
              }
          }
          if (!heap_fn_name) {
              char *mbuf = NULL;
              err = alloc_mangled_routine_name(rout, NULL, &mbuf);
              if (err == GCC_GEN_BE_ERROR_OOM) return err;
              if (err == GCC_GEN_BE_SUCCESS) {
                  heap_fn_name = mbuf;
                  name = heap_fn_name;
              }
          }
      }

      void *existing_named = NULL;
      err = cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)name, &existing_named);
      if (err != GCC_GEN_BE_SUCCESS) {
          free(heap_fn_name);
          return err;
      }
      if (existing_named) {
          if (rout->storage_class != sc_static &&
              rout->function_def_number == NULL_function_def_number) {
              void *prev_fn = NULL;
              err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, existing_named, &prev_fn);
              if (err != GCC_GEN_BE_SUCCESS) {
                  free(heap_fn_name);
                  return err;
              }
              if (prev_fn) {
                  err = cache_insert(GCC_GEN_BE_CACHE_FUNC, rout, prev_fn);
                  free(heap_fn_name);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  *out_func = (gcc_jit_function *)prev_fn;
                  return GCC_GEN_BE_SUCCESS;
              }
          }
          GCC_GEN_BE_CHECK(uniquify_symbol_name(name, rout, &heap_fn_name, &name));
      } else {
          err = cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)name, (void *)rout);
          if (err != GCC_GEN_BE_SUCCESS) {
              free(heap_fn_name);
              return err;
          }
      }
  }

  a_boolean is_anon_fn = FALSE;
  err = gcc_gen_be_is_scope_anonymous(rout->source_corresp.parent_scope, &is_anon_fn);
  if (err != GCC_GEN_BE_SUCCESS) {
      free(heap_fn_name);
      return err;
  }

  a_boolean emit_weak_zth_stub = FALSE;
  if (rout->function_def_number == NULL_function_def_number &&
      strncmp(name, "_ZTH", 4) == 0 &&
      il_header.primary_scope != NULL) {
      for (a_variable_ptr tv = il_header.primary_scope->variables; tv != NULL; tv = tv->next) {
          if (tv->is_thread_local && tv->storage_class != sc_extern && tv->source_corresp.name) {
              size_t vlen = strlen(tv->source_corresp.name);
              size_t flen = strlen(name);
              if (flen > 4U + vlen && strcmp(name + flen - vlen, tv->source_corresp.name) == 0) {
                  emit_weak_zth_stub = TRUE;
                  break;
              }
          }
      }
  }

  enum gcc_jit_function_kind linkage;
  if (rout->function_def_number == NULL_function_def_number && !emit_weak_zth_stub) {
      linkage = GCC_JIT_FUNCTION_IMPORTED;
  } else if (rout->storage_class == sc_static || is_anon_fn) {
      linkage = GCC_JIT_FUNCTION_INTERNAL;
  } else {
      linkage = GCC_JIT_FUNCTION_EXPORTED;
  }

  int num_params = uses_sret ? 1 : 0;
  for (a_param_type_ptr ptp = first_ptp; ptp != NULL; ptp = ptp->next) {
      num_params++;
  }

  a_scope_ptr def_scope = NULL;
  err = resolve_routine_def_scope(rout, &def_scope);
  if (err != GCC_GEN_BE_SUCCESS) {
      free(heap_fn_name);
      return err;
  }

  gcc_jit_param **params = NULL;
  if (num_params > 0) {
      err = gcc_gen_be_alloc((size_t)num_params * sizeof(gcc_jit_param *), (void **)&params);
      if (err != GCC_GEN_BE_SUCCESS) {
          free(heap_fn_name);
          return err;
      }

      int i = 0;
      if (uses_sret) {
          params[i++] = gcc_jit_context_new_param(ctx, NULL, sret_ptr_type, "sret_buffer");
      }

      a_variable_ptr var_param = def_scope ? def_scope->variant.routine.parameters : NULL;
      for (a_param_type_ptr ptp = first_ptp; ptp != NULL; ptp = ptp->next) {
          gcc_jit_type *param_type = NULL;
          a_type_ptr ptype = NULL;
          err = unwrap_decl_type(ptp->type, &ptype);
          if (err != GCC_GEN_BE_SUCCESS) { free(heap_fn_name); free(params); return err; }
          if (ptype && ptype->kind == tk_array) {
              gcc_jit_type *elem_type = NULL;
              err = gcc_gen_be_lower_type(ptype->variant.array.element_type, &elem_type);
              if (err != GCC_GEN_BE_SUCCESS) { free(heap_fn_name); free(params); return err; }
              param_type = gcc_jit_type_get_pointer(elem_type);
          } else {
              err = gcc_gen_be_lower_type(ptype, &param_type);
              if (err != GCC_GEN_BE_SUCCESS) {
                  free(heap_fn_name);
                  free(params);
                  return err;
              }
              param_type = gcc_jit_type_unqualified(param_type);
              extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
              extern gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;
              a_boolean is_invis_ref = FALSE;
              err = gcc_gen_be_is_passed_by_invisible_reference(ptype, &is_invis_ref);
              if (err != GCC_GEN_BE_SUCCESS) { free(heap_fn_name); free(params); return err; }
              if (is_invis_ref) {
                  param_type = gcc_jit_type_get_pointer(param_type);
              } else if (ptype) {
                  gcc_jit_type *coerced_pt = NULL;
                  a_boolean needs_coercion = FALSE;
                  err = gcc_gen_be_get_coerced_abi_type(ptype, &coerced_pt, &needs_coercion);
                  if (err != GCC_GEN_BE_SUCCESS) { free(heap_fn_name); free(params); return err; }
                  if (needs_coercion) {
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
                  free(heap_fn_name);
                  free(params);
                  return err;
              }
              var_param = var_param->next;
          }
          params[i] = gcc_jit_context_new_param(ctx, param_loc, param_type, param_name);
          i++;
      }
  }

  int is_variadic = (r_extra && r_extra->has_ellipsis) ? 1 : 0;

  gcc_jit_location *loc = NULL;
  err = gcc_gen_be_get_location(&rout->source_corresp.decl_position, &loc);
  if (err != GCC_GEN_BE_SUCCESS) {
      free(heap_fn_name);
      free(params);
      return err;
  }

  a_boolean has_always_inline = FALSE;
  for (an_attribute_ptr ap = rout->source_corresp.attributes; ap != NULL; ap = ap->next) {
      if (ap->kind == ak_always_inline) {
          has_always_inline = TRUE;
          break;
      }
  }
  if (has_always_inline && rout->function_def_number != NULL_function_def_number) {
      linkage = GCC_JIT_FUNCTION_ALWAYS_INLINE;
  }

  a_boolean use_aarch64_x8_thunk = FALSE;
#if DO_IL_LOWERING && defined(__aarch64__) && defined(LIBGCCJIT_HAVE_ASM_STATEMENTS)
  if ((edg_ret_as_param || uses_sret) && num_params >= 1 && !is_variadic) {
      extern gcc_gen_be_error_t gcc_gen_be_get_target_abi(gcc_gen_be_target_abi_t *out_abi) GCC_GEN_BE_NOEXCEPT;
      gcc_gen_be_target_abi_t cur_abi = GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64;
      err = gcc_gen_be_get_target_abi(&cur_abi);
      if (err != GCC_GEN_BE_SUCCESS) { free(heap_fn_name); free(params); return err; }
      if (cur_abi == GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64 || cur_abi == GCC_GEN_BE_TARGET_ABI_AAPCS64) {
          use_aarch64_x8_thunk = TRUE;
      }
  }
#endif

  char *x8_internal_name = NULL;
  enum gcc_jit_function_kind body_linkage = linkage;
  const char *body_name = name;
  if (use_aarch64_x8_thunk) {
      size_t nlen = strlen(name);
      err = gcc_gen_be_alloc(nlen + 32, (void **)&x8_internal_name);
      if (err != GCC_GEN_BE_SUCCESS) { free(heap_fn_name); free(params); return err; }
      snprintf(x8_internal_name, nlen + 32, "__edg_x8_fn_%s_%p", name, (void *)rout);
      body_linkage = GCC_JIT_FUNCTION_INTERNAL;
      body_name = x8_internal_name;
  }

  gcc_jit_function *func = gcc_jit_context_new_function(
      ctx, loc, body_linkage, ret_type, body_name, num_params, params, is_variadic);
  free(x8_internal_name);
  x8_internal_name = NULL;
  extern gcc_gen_be_error_t gcc_gen_be_apply_function_attributes(a_routine_ptr rout, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;
  if (use_aarch64_x8_thunk) {
      if (p_gcc_jit_function_add_attribute) {
          p_gcc_jit_function_add_attribute(
              func,
              (rout->function_def_number == NULL_function_def_number)
                  ? GCC_JIT_FN_ATTRIBUTE_NOINLINE
                  : GCC_JIT_FN_ATTRIBUTE_USED);
      }
  } else {
      err = gcc_gen_be_apply_function_attributes(rout, func);
      if (err != GCC_GEN_BE_SUCCESS) { free(heap_fn_name); free(params); return err; }
  }

  if (!use_aarch64_x8_thunk &&
      rout->function_def_number != NULL_function_def_number &&
      linkage == GCC_JIT_FUNCTION_EXPORTED &&
      (rout->is_weak || rout->is_inline ||
       (rout->is_template_function && !rout->is_specialized) ||
       rout->compiler_generated || strncmp(name, "_ZZ", 3) == 0 ||
       (rout->source_corresp.is_class_member && rout->special_kind != sfk_none))) {
      err = gcc_gen_be_mark_function_vague_linkage(ctx, func, name);
      if (err != GCC_GEN_BE_SUCCESS) { free(heap_fn_name); free(params); return err; }
  }
  if (emit_weak_zth_stub && linkage == GCC_JIT_FUNCTION_EXPORTED) {
      err = gcc_gen_be_mark_function_vague_linkage(ctx, func, name);
      if (err != GCC_GEN_BE_SUCCESS) { free(heap_fn_name); free(params); return err; }
      gcc_jit_block *zth_blk = gcc_jit_function_new_block(func, "zth_stub");
      gcc_jit_block_end_with_void_return(zth_blk, loc);
  }

#if DO_IL_LOWERING && defined(__aarch64__) && defined(LIBGCCJIT_HAVE_ASM_STATEMENTS)
  if (use_aarch64_x8_thunk) {
      int abi_nparams = num_params - 1;
      gcc_jit_param **abi_params = NULL;
      if (abi_nparams > 0) {
          err = gcc_gen_be_alloc((size_t)abi_nparams * sizeof(gcc_jit_param *), (void **)&abi_params);
          if (err != GCC_GEN_BE_SUCCESS) { free(heap_fn_name); free(params); return err; }
          for (int pi = 0; pi < abi_nparams; ++pi) {
              gcc_jit_type *pt = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(params[pi + 1]));
              char pbuf[32];
              snprintf(pbuf, sizeof(pbuf), "x8_abi_p%d", pi);
              abi_params[pi] = gcc_jit_context_new_param(ctx, loc, pt, pbuf);
          }
      }
      char *x8_abi_name = NULL;
      const char *abi_sym_name = name;
      if (linkage == GCC_JIT_FUNCTION_INTERNAL) {
          size_t anlen = strlen(name);
          err = gcc_gen_be_alloc(anlen + 36U, (void **)&x8_abi_name);
          if (err != GCC_GEN_BE_SUCCESS) { free(abi_params); free(heap_fn_name); free(params); return err; }
          snprintf(x8_abi_name, anlen + 36U, "__edg_x8_abi_%s_%p", name, (void *)rout);
          abi_sym_name = x8_abi_name;
      }
      gcc_jit_function *abi_fn = gcc_jit_context_new_function(
          ctx, loc, linkage, ret_type, abi_sym_name, abi_nparams, abi_params, 0);
      free(x8_abi_name);
      err = cache_insert(GCC_GEN_BE_CACHE_FUNC, func, abi_fn);
      if (err != GCC_GEN_BE_SUCCESS) { free(abi_params); free(heap_fn_name); free(params); return err; }
      if (rout->function_def_number == NULL_function_def_number) {
          /* Non-inlined internal forwarder: sets x8=sret_ptr immediately before tail-calling abi_fn(a1..) */
          gcc_jit_block *iblk = gcc_jit_function_new_block(func, "entry");
          gcc_jit_rvalue *sret_rv = gcc_jit_param_as_rvalue(params[0]);
          gcc_jit_extended_asm *ext_asm = gcc_jit_block_add_extended_asm(iblk, loc, "mov x8, %0");
          gcc_jit_extended_asm_add_input_operand(ext_asm, NULL, "r", sret_rv);
          gcc_jit_extended_asm_add_clobber(ext_asm, "memory");
          gcc_jit_extended_asm_set_volatile_flag(ext_asm, 1);
          gcc_jit_rvalue **fwd_args = NULL;
          if (abi_nparams > 0) {
              err = gcc_gen_be_alloc((size_t)abi_nparams * sizeof(gcc_jit_rvalue *), (void **)&fwd_args);
              if (err != GCC_GEN_BE_SUCCESS) { free(abi_params); free(heap_fn_name); free(params); return err; }
              for (int pi = 0; pi < abi_nparams; ++pi) {
                  fwd_args[pi] = gcc_jit_param_as_rvalue(params[pi + 1]);
              }
          }
          gcc_jit_block_add_eval(iblk, loc, gcc_jit_context_new_call(ctx, loc, abi_fn, abi_nparams, fwd_args));
          free(fwd_args);
          gcc_jit_block_end_with_void_return(iblk, loc);
      } else {
          err = gcc_gen_be_apply_function_attributes(rout, abi_fn);
          if (err != GCC_GEN_BE_SUCCESS) { free(abi_params); free(heap_fn_name); free(params); return err; }
          if (linkage == GCC_JIT_FUNCTION_EXPORTED &&
              (rout->is_weak || rout->is_inline ||
               (rout->is_template_function && !rout->is_specialized) ||
               rout->compiler_generated || strncmp(name, "_ZZ", 3) == 0 ||
               (rout->source_corresp.is_class_member && rout->special_kind != sfk_none))) {
              err = gcc_gen_be_mark_function_vague_linkage(ctx, abi_fn, name);
              if (err != GCC_GEN_BE_SUCCESS) { free(abi_params); free(heap_fn_name); free(params); return err; }
          }
          /* Defined function: abi_fn(a1..) reads x8 and forwards to func(x8_ptr, a1..) */
          gcc_jit_block *eblk = gcc_jit_function_new_block(abi_fn, "entry");
          gcc_jit_type *p0_type = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(params[0]));
          gcc_jit_lvalue *x8_loc = gcc_jit_function_new_local(abi_fn, loc, p0_type, "x8_in");
          gcc_jit_extended_asm *ext_asm = gcc_jit_block_add_extended_asm(eblk, loc, "mov %0, x8");
          gcc_jit_extended_asm_add_output_operand(ext_asm, NULL, "=r", x8_loc);
          gcc_jit_extended_asm_set_volatile_flag(ext_asm, 1);
          gcc_jit_rvalue **fwd_args = NULL;
          err = gcc_gen_be_alloc((size_t)num_params * sizeof(gcc_jit_rvalue *), (void **)&fwd_args);
          if (err != GCC_GEN_BE_SUCCESS) { free(abi_params); free(heap_fn_name); free(params); return err; }
          fwd_args[0] = gcc_jit_lvalue_as_rvalue(x8_loc);
          for (int pi = 0; pi < abi_nparams; ++pi) {
              fwd_args[pi + 1] = gcc_jit_param_as_rvalue(abi_params[pi]);
          }
          gcc_jit_block_add_eval(eblk, loc, gcc_jit_context_new_call(ctx, loc, func, num_params, fwd_args));
          free(fwd_args);
          gcc_jit_block_end_with_void_return(eblk, loc);
      }
      free(abi_params);
  }
#endif
  free(heap_fn_name);
  heap_fn_name = NULL;

  if (num_params > 0) {
      int i = 0;
      if (uses_sret) {
          err = cache_insert(GCC_GEN_BE_CACHE_VAR, func, params[0]);
          if (err != GCC_GEN_BE_SUCCESS) { free(params); return err; }
          i = 1;
      }
      a_variable_ptr var_param = def_scope ? def_scope->variant.routine.parameters : NULL;
      for (a_param_type_ptr ptp = first_ptp; ptp != NULL; ptp = ptp->next) {
          if (var_param) {
              gcc_jit_lvalue *param_lval = gcc_jit_param_as_lvalue(params[i]);

              extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;
              err = gcc_gen_be_emit_variable_debug_info(var_param, param_lval);
              if (err != GCC_GEN_BE_SUCCESS) { free(params); return err; }

              extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
              a_type_ptr ptype = NULL;
              err = unwrap_decl_type(ptp->type, &ptype);
              if (err != GCC_GEN_BE_SUCCESS) { free(params); return err; }
              a_boolean is_invis_ref = FALSE;
              err = gcc_gen_be_is_passed_by_invisible_reference(ptype, &is_invis_ref);
              if (err != GCC_GEN_BE_SUCCESS) { free(params); return err; }
              if (is_invis_ref) {
                  param_lval = gcc_jit_rvalue_dereference(gcc_jit_lvalue_as_rvalue(param_lval), NULL);
              }

              err = cache_insert(GCC_GEN_BE_CACHE_VAR, var_param, param_lval);
              if (err != GCC_GEN_BE_SUCCESS) { free(params); return err; }
              var_param = var_param->next;
          }
          i++;
      }
      free(params);
  }

  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, rout, func));

  if (rout->function_def_number != NULL_function_def_number && def_scope && def_scope->assoc_block) {
      if (rout->is_coroutine) {
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
      struct gcc_gen_be_scope **saved_scope_stack = be_state->scope_stack;
      size_t saved_scope_size = be_state->scope_stack_size;
      size_t saved_scope_cap = be_state->scope_stack_capacity;
      struct gcc_jit_block **saved_lp_stack = be_state->landing_pad_stack;
      size_t saved_lp_size = be_state->landing_pad_stack_size;
      size_t saved_lp_cap = be_state->landing_pad_stack_capacity;
      gcc_gen_be_eh_state_t saved_eh_state = be_state->eh_state;
      a_scope_ptr saved_active_fn_scope = be_state->active_function_scope;
      a_dynamic_init_ptr saved_eh_dips[64];
      size_t saved_eh_dip_depth = be_state->eh_cleanup_dip_depth;
      if (saved_eh_dip_depth > 0) {
          memcpy(saved_eh_dips, be_state->eh_cleanup_dips, saved_eh_dip_depth * sizeof(a_dynamic_init_ptr));
      }
      a_dynamic_init_ptr saved_eh_barriers[32];
      size_t saved_eh_barrier_depth = be_state->eh_try_barrier_depth;
      if (saved_eh_barrier_depth > 0) {
          memcpy(saved_eh_barriers, be_state->eh_try_barriers, saved_eh_barrier_depth * sizeof(a_dynamic_init_ptr));
      }
      be_state->scope_stack = NULL;
      be_state->scope_stack_size = 0;
      be_state->scope_stack_capacity = 0;
      be_state->landing_pad_stack = NULL;
      be_state->landing_pad_stack_size = 0;
      be_state->landing_pad_stack_capacity = 0;
      be_state->active_function_scope = def_scope;
      be_state->eh_cleanup_dip_depth = 0;
      be_state->eh_try_barrier_depth = 0;
      memset(&be_state->eh_state, 0, sizeof(be_state->eh_state));

      err = lower_defined_function_body_inner(ctx, rout, r_extra, frontend_ret, def_scope, func, ret_type, be_state);

      for (size_t si = 0; si < be_state->scope_stack_size; ++si) {
          free(be_state->scope_stack[si]);
      }
      free(be_state->scope_stack);
      be_state->scope_stack = saved_scope_stack;
      be_state->scope_stack_size = saved_scope_size;
      be_state->scope_stack_capacity = saved_scope_cap;
      free(be_state->landing_pad_stack);
      be_state->landing_pad_stack = saved_lp_stack;
      be_state->landing_pad_stack_size = saved_lp_size;
      be_state->landing_pad_stack_capacity = saved_lp_cap;
      free(be_state->eh_state.action_table);
      be_state->eh_state = saved_eh_state;
      be_state->active_function_scope = saved_active_fn_scope;
      be_state->eh_cleanup_dip_depth = saved_eh_dip_depth;
      if (saved_eh_dip_depth > 0) {
          memcpy(be_state->eh_cleanup_dips, saved_eh_dips, saved_eh_dip_depth * sizeof(a_dynamic_init_ptr));
      }
      be_state->eh_try_barrier_depth = saved_eh_barrier_depth;
      if (saved_eh_barrier_depth > 0) {
          memcpy(be_state->eh_try_barriers, saved_eh_barriers, saved_eh_barrier_depth * sizeof(a_dynamic_init_ptr));
      }
      innermost_function_scope = saved_innermost_scope;
      if (err != GCC_GEN_BE_SUCCESS) {
          return err;
      }
      GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(saved_caller_block));

      {
          a_boolean is_attr_ctor = rout->is_initialization_routine ? TRUE : FALSE;
          a_boolean is_attr_dtor = rout->is_finalization_routine ? TRUE : FALSE;
#if (GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED) && GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
          if (rout->has_ctor_priority) is_attr_ctor = TRUE;
          if (rout->has_dtor_priority) is_attr_dtor = TRUE;
#endif
          if ((is_attr_ctor || is_attr_dtor) &&
              gcc_jit_function_get_param_count(func) == 0 &&
              gcc_jit_function_get_return_type(func) == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
              gcc_jit_block *gctor_blk = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&gctor_blk));
              if (is_attr_ctor) {
                  gcc_jit_block_add_eval(gctor_blk, NULL, gcc_jit_context_new_call(ctx, NULL, func, 0, NULL));
              }
              if (is_attr_dtor) {
                  gcc_jit_type *vp_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                  gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                  void *cached_atexit = NULL;
                  GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_atexit", &cached_atexit));
                  gcc_jit_function *atexit_fn = (gcc_jit_function *)cached_atexit;
                  if (!atexit_fn) {
                      gcc_jit_param *ae_params[3];
                      ae_params[0] = gcc_jit_context_new_param(ctx, NULL, vp_t, "dtor");
                      ae_params[1] = gcc_jit_context_new_param(ctx, NULL, vp_t, "arg");
                      ae_params[2] = gcc_jit_context_new_param(ctx, NULL, vp_t, "dso");
                      atexit_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_t, "__cxa_atexit", 3, ae_params, 0);
                      GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_atexit", atexit_fn));
                  }
                  gcc_jit_rvalue *dso_rv = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_dso_handle(ctx, &dso_rv));
                  gcc_jit_rvalue *ae_args[3];
                  ae_args[0] = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(func, NULL), vp_t);
                  ae_args[1] = gcc_jit_context_null(ctx, vp_t);
                  ae_args[2] = dso_rv;
                  gcc_jit_block_add_eval(gctor_blk, NULL, gcc_jit_context_new_call(ctx, NULL, atexit_fn, 3, ae_args));
              }
          }
      }

      if ((rout->special_kind == sfk_constructor || rout->special_kind == sfk_destructor) &&
          il_header.primary_scope == NULL) {
          const char *rname = rout->source_corresp.name ? rout->source_corresp.name : "";
          if (strstr(rname, "C2E") == NULL && strstr(rname, "D2E") == NULL && strstr(rname, "D0E") == NULL) {
              gcc_jit_function *base_clone = NULL;
              gcc_jit_function *del_clone = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(rout, func, &base_clone, &del_clone));
          }
      }
  }

  *out_func = func;
  return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Searches `scope->routines` for an EDG-generated sibling constructor/destructor matching `target_name`.
 * @details Prevents `gcc_gen_be_emit_ctor_dtor_clones` from synthesizing a forwarding `C2`/`D2`/`D0` stub
 * when EDG's IL lowering has already created the real `a_routine` with that exact mangled name.
 * @param[in] scope Scope whose routine list is searched (may be NULL).
 * @param[in] exclude_rout Primary routine being cloned.
 * @param[in] target_name Mangled Itanium ABI symbol name (`base_name` or `d0_name`).
 * @param[out] out_found Pointer receiving the matching `a_routine_ptr`, or `NULL` if none exists.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t find_edg_ctor_dtor_sibling(
    a_scope_ptr scope,
    a_routine_ptr exclude_rout,
    const char *target_name,
    a_routine_ptr *out_found) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_found = NULL;
    if (!scope) {
        return GCC_GEN_BE_SUCCESS;
    }
    for (a_routine_ptr r = scope->routines; r != NULL; r = r->next) {
        if (r == exclude_rout) continue;
        const char *rn = r->source_corresp.name;
#if NEED_NAME_MANGLING
        if (r->source_corresp.unmangled_name_or_mangled_encoding &&
            strncmp(r->source_corresp.unmangled_name_or_mangled_encoding, "_Z", 2) == 0) {
            rn = r->source_corresp.unmangled_name_or_mangled_encoding;
        }
#endif
        if (rn && strcmp(rn, target_name) == 0) {
            *out_found = r;
            return GCC_GEN_BE_SUCCESS;
        }
    }
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

    char *primary_mangled = NULL;
    GCC_GEN_BE_CHECK(alloc_mangled_routine_name(rout, NULL, &primary_mangled));

    const char *base_variant = NULL;
    if (rout->special_kind == sfk_constructor) {
        base_variant = (strstr(primary_mangled, "C2") != NULL) ? "C1" : "C2";
    } else {
        base_variant = (strstr(primary_mangled, "D2") != NULL) ? "D1" : "D2";
    }

    char *base_name = NULL;
    gcc_gen_be_error_t cerr = alloc_mangled_routine_name(rout, base_variant, &base_name);
    if (cerr != GCC_GEN_BE_SUCCESS) {
        free(primary_mangled);
        return cerr;
    }

    a_boolean is_anon_clone = FALSE;
    cerr = gcc_gen_be_is_scope_anonymous(rout->source_corresp.parent_scope, &is_anon_clone);
    if (cerr != GCC_GEN_BE_SUCCESS) {
        free(primary_mangled);
        free(base_name);
        return cerr;
    }
    enum gcc_jit_function_kind clone_linkage = is_anon_clone ? GCC_JIT_FUNCTION_INTERNAL : GCC_JIT_FUNCTION_EXPORTED;

    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);

    a_type_ptr parent_class = (rout->source_corresp.is_class_member && rout->source_corresp.parent_scope)
        ? rout->source_corresp.parent_scope->variant.assoc_type
        : NULL;
    a_boolean has_vbases = (parent_class &&
        (parent_class->kind == tk_class || parent_class->kind == tk_struct) &&
        parent_class->variant.class_struct_union.any_virtual_base_classes) ? TRUE : FALSE;
    if (has_vbases) {
        extern gcc_gen_be_error_t gcc_gen_be_emit_vtt(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT;
        struct gcc_jit_lvalue *vtt_lval = NULL;
        cerr = gcc_gen_be_emit_vtt(parent_class, &vtt_lval);
        if (cerr != GCC_GEN_BE_SUCCESS) {
            free(primary_mangled);
            free(base_name);
            return cerr;
        }
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
    int call_cnt = primary_param_cnt;

    void *existing_base_sym = NULL;
    a_boolean skip_base_clone = (strcmp(primary_mangled, base_name) == 0) ? TRUE : FALSE;
    free(primary_mangled);
    if (!skip_base_clone) {
        a_routine_ptr edg_base_sib = NULL;
        cerr = find_edg_ctor_dtor_sibling(
            rout->source_corresp.parent_scope, rout, base_name, &edg_base_sib);
        if (cerr != GCC_GEN_BE_SUCCESS) {
            free(base_name);
            return cerr;
        }
        if (!edg_base_sib && il_header.primary_scope) {
            cerr = find_edg_ctor_dtor_sibling(
                il_header.primary_scope, rout, base_name, &edg_base_sib);
            if (cerr != GCC_GEN_BE_SUCCESS) {
                free(base_name);
                return cerr;
            }
        }
        if (edg_base_sib) {
            skip_base_clone = TRUE;
            gcc_jit_function *sib_fn = NULL;
            cerr = gcc_gen_be_lower_function_decl(edg_base_sib, &sib_fn);
            if (cerr != GCC_GEN_BE_SUCCESS) {
                free(base_name);
                return cerr;
            }
            if (out_base_clone) *out_base_clone = sib_fn;
        }
    }
    if (skip_base_clone) {
        if (out_base_clone && !*out_base_clone) *out_base_clone = primary_func;
    } else {
        cerr = cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)base_name, &existing_base_sym);
        if (cerr != GCC_GEN_BE_SUCCESS) {
            free(base_name);
            return cerr;
        }
        if (existing_base_sym != NULL) {
            skip_base_clone = TRUE;
            void *prev_fn = NULL;
            cerr = cache_lookup(GCC_GEN_BE_CACHE_FUNC, existing_base_sym, &prev_fn);
            if (cerr != GCC_GEN_BE_SUCCESS) {
                free(base_name);
                return cerr;
            }
            if (out_base_clone) {
                *out_base_clone = (gcc_jit_function *)(prev_fn ? prev_fn : existing_base_sym);
            }
        }
    }

    a_boolean clone_needs_vtt_parm = (has_vbases &&
        (strcmp(base_variant, "C2") == 0 || strcmp(base_variant, "D2") == 0)) ? TRUE : FALSE;
    int c2_param_cnt = clone_needs_vtt_parm ? (num_params + 1) : num_params;
    if (!skip_base_clone) {
        gcc_jit_param **c2_params = NULL;
        gcc_jit_rvalue **c2_args = NULL;
        if (c2_param_cnt > 0) {
            cerr = gcc_gen_be_alloc((size_t)c2_param_cnt * sizeof(gcc_jit_param *), (void **)&c2_params);
            if (cerr == GCC_GEN_BE_SUCCESS) {
                cerr = gcc_gen_be_alloc((size_t)(num_params > 0 ? num_params : 1) * sizeof(gcc_jit_rvalue *), (void **)&c2_args);
            }
            if (cerr != GCC_GEN_BE_SUCCESS) {
                free(c2_params);
                free(c2_args);
                free(base_name);
                return cerr;
            }
            a_param_type_ptr p = ptp;
            int c2_idx = 0;
            for (int idx = 0; idx < num_params; ++idx) {
                gcc_jit_type *pt = void_ptr_type;
                if (idx < primary_param_cnt) {
                    pt = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(primary_func, idx)));
                } else if (p->type) {
                    cerr = gcc_gen_be_lower_type(p->type, &pt);
                    if (cerr != GCC_GEN_BE_SUCCESS) {
                        free(c2_params);
                        free(c2_args);
                        free(base_name);
                        return cerr;
                    }
                }
                char pname[32];
                snprintf(pname, sizeof(pname), "arg_%d", idx);
                c2_params[c2_idx] = gcc_jit_context_new_param(ctx, NULL, pt, pname);
                c2_args[idx] = gcc_jit_param_as_rvalue(c2_params[c2_idx]);
                c2_idx++;
                if (idx == 0 && clone_needs_vtt_parm) {
                    gcc_jit_type *vtt_parm_t = gcc_jit_type_get_pointer(void_ptr_type);
                    c2_params[c2_idx++] = gcc_jit_context_new_param(ctx, NULL, vtt_parm_t, "__vtt_parm");
                }
                if (p) p = p->next;
            }
            if (num_params == 0) {
                gcc_jit_type *vtt_parm_t = gcc_jit_type_get_pointer(void_ptr_type);
                c2_params[0] = gcc_jit_context_new_param(ctx, NULL, vtt_parm_t, "__vtt_parm");
            }
        }

        gcc_jit_function *base_clone = gcc_jit_context_new_function(
            ctx, NULL, clone_linkage, void_type, base_name, c2_param_cnt, c2_params, 0);
        cerr = cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)base_name, (void *)base_clone);
        if (cerr != GCC_GEN_BE_SUCCESS) {
            free(c2_params);
            free(c2_args);
            free(base_name);
            return cerr;
        }
        if (!is_anon_clone) {
            cerr = gcc_gen_be_mark_function_vague_linkage(ctx, base_clone, base_name);
            if (cerr != GCC_GEN_BE_SUCCESS) {
                free(c2_params);
                free(c2_args);
                free(base_name);
                return cerr;
            }
        }
        gcc_jit_block *b_entry = gcc_jit_function_new_block(base_clone, "entry");
        if (call_cnt == num_params) {
            gcc_jit_block_add_eval(b_entry, NULL, gcc_jit_context_new_call(ctx, NULL, primary_func, num_params, c2_args));
        }
        gcc_jit_block_end_with_void_return(b_entry, NULL);

        free(c2_params);
        free(c2_args);
        if (out_base_clone) *out_base_clone = base_clone;
    }
    free(base_name);

    if (rout->special_kind == sfk_destructor) {
        char *d0_name = NULL;
        GCC_GEN_BE_CHECK(alloc_mangled_routine_name(rout, "D0", &d0_name));
        a_routine_ptr edg_d0_sib = NULL;
        cerr = find_edg_ctor_dtor_sibling(
            rout->source_corresp.parent_scope, rout, d0_name, &edg_d0_sib);
        if (cerr != GCC_GEN_BE_SUCCESS) {
            free(d0_name);
            return cerr;
        }
        if (!edg_d0_sib && il_header.primary_scope) {
            cerr = find_edg_ctor_dtor_sibling(
                il_header.primary_scope, rout, d0_name, &edg_d0_sib);
            if (cerr != GCC_GEN_BE_SUCCESS) {
                free(d0_name);
                return cerr;
            }
        }
        if (edg_d0_sib) {
            gcc_jit_function *sib_d0_fn = NULL;
            cerr = gcc_gen_be_lower_function_decl(edg_d0_sib, &sib_d0_fn);
            free(d0_name);
            if (cerr != GCC_GEN_BE_SUCCESS) return cerr;
            if (out_deleting_clone) *out_deleting_clone = sib_d0_fn;
            return GCC_GEN_BE_SUCCESS;
        }
        void *existing_d0_sym = NULL;
        cerr = cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)d0_name, &existing_d0_sym);
        if (cerr != GCC_GEN_BE_SUCCESS) {
            free(d0_name);
            return cerr;
        }
        if (existing_d0_sym != NULL) {
            void *prev_d0 = NULL;
            cerr = cache_lookup(GCC_GEN_BE_CACHE_FUNC, existing_d0_sym, &prev_d0);
            free(d0_name);
            if (cerr != GCC_GEN_BE_SUCCESS) return cerr;
            if (out_deleting_clone) {
                *out_deleting_clone = (gcc_jit_function *)(prev_d0 ? prev_d0 : existing_d0_sym);
            }
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_type *this_param_type = void_ptr_type;
        if (call_cnt >= 1) {
            this_param_type = gcc_jit_rvalue_get_type(gcc_jit_param_as_rvalue(gcc_jit_function_get_param(primary_func, 0)));
        } else if (ptp && ptp->type) {
            cerr = gcc_gen_be_lower_type(ptp->type, &this_param_type);
            if (cerr != GCC_GEN_BE_SUCCESS) {
                free(d0_name);
                return cerr;
            }
        }
        gcc_jit_param *d0_this = gcc_jit_context_new_param(ctx, NULL, this_param_type, "this_ptr");
        gcc_jit_function *d0_clone = gcc_jit_context_new_function(
            ctx, NULL, clone_linkage, void_type, d0_name, 1, &d0_this, 0);
        cerr = cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)d0_name, (void *)d0_clone);
        if (cerr != GCC_GEN_BE_SUCCESS) {
            free(d0_name);
            return cerr;
        }
        if (!is_anon_clone) {
            cerr = gcc_gen_be_mark_function_vague_linkage(ctx, d0_clone, d0_name);
            if (cerr != GCC_GEN_BE_SUCCESS) {
                free(d0_name);
                return cerr;
            }
        }
        free(d0_name);
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
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    out_section[0] = '\0';
    *out_linkage_attr = NULL;

    int written = 0;
    if (object_format == 0) {
        const char *kind_ch = is_function ? "t" : (is_readonly_data ? "r" : "d");
        written = snprintf(out_section, buf_cap, ".gnu.linkonce.%s.%s", kind_ch, symbol_name);
        *out_linkage_attr = "weak";
    } else if (object_format == 1) {
        const char *macho_sec = is_function
            ? "__TEXT,__textcoal_nt"
            : (is_readonly_data ? "__DATA,__const_coal" : "__DATA,__datacoal_nt");
        written = snprintf(out_section, buf_cap, "%s", macho_sec);
        *out_linkage_attr = "weak_definition";
    } else {
        const char *coff_sec = is_function
            ? ".text$mn"
            : (is_readonly_data ? ".rdata$r" : ".data$r");
        written = snprintf(out_section, buf_cap, "%s", coff_sec);
        *out_linkage_attr = "selectany";
    }
    if ((size_t)written >= buf_cap) {
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
    size_t sec_cap = strlen(symbol_name) + 64;
    char *sec_buf = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(sec_cap, (void **)&sec_buf));
    const char *attr = NULL;
#if defined(_WIN32)
    int obj_fmt = 2;
#elif defined(__APPLE__) && defined(__MACH__)
    int obj_fmt = 1;
#else
    int obj_fmt = 0;
#endif
    gcc_gen_be_error_t err = gcc_gen_be_format_comdat_section_name(
        symbol_name, FALSE, is_readonly_data, obj_fmt, sec_buf, sec_cap, &attr);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(sec_buf);
        return err;
    }
#if !defined(__APPLE__) || !defined(__MACH__)
    if (p_gcc_jit_lvalue_set_link_section) {
        p_gcc_jit_lvalue_set_link_section(global, sec_buf);
    }
#endif
    free(sec_buf);
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */