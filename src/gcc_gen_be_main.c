/**
 * @file gcc_gen_be_main.c
 * @brief Implementation of main entry points for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_main.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_stmt.h"
#include "host_envir.h"
#include "fe_common.h"
#include "il.h"
#include <libgccjit.h>
#include <string.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE




gcc_gen_be_error_t gcc_gen_be(void) GCC_GEN_BE_NOEXCEPT {
  a_scope_ptr scope;
  a_routine_ptr rout;
  a_variable_ptr var;

  gcc_gen_be_error_t err = gcc_gen_be_init();
  if (err != GCC_GEN_BE_SUCCESS) {
      const char *err_msg = NULL;
      gcc_gen_be_error_string(err, &err_msg);
      fprintf(f_error, "gcc_gen_be init error: %s\n", err_msg ? err_msg : "Unknown");
      diagnostic_counters.total.catastrophes++;
      return err;
  }
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) {
      fprintf(f_error, "gcc_gen_be init error: Context is NULL\n");
      diagnostic_counters.total.catastrophes++;
      return GCC_GEN_BE_ERROR_INTERNAL;
  }

  scope = il_header.primary_scope;
  
  if (scope) {
    for (var = scope->variables; var != NULL; var = var->next) {
        gcc_jit_lvalue *lval = NULL;
        err = gcc_gen_be_lower_variable_decl(var, &lval);
        if (err != GCC_GEN_BE_SUCCESS) {
            const char *err_msg = NULL;
            gcc_gen_be_error_string(err, &err_msg);
            fprintf(f_error, "gcc_gen_be variable lowering error: %s\n", err_msg ? err_msg : "Unknown");
            diagnostic_counters.total.errors++;
        }
    }
    
    for (rout = scope->routines; rout != NULL; rout = rout->next) {
      if (ignore_routine_in_back_end(rout)) continue;

      gcc_jit_function *func = NULL;
      err = gcc_gen_be_lower_function_decl(rout, &func);
      if (err != GCC_GEN_BE_SUCCESS || !func) {
          const char *err_msg = NULL;
          gcc_gen_be_error_string(err, &err_msg);
          fprintf(f_error, "gcc_gen_be function lowering error: %s\n", err_msg ? err_msg : "Unknown");
          diagnostic_counters.total.errors++;
          continue;
      }

      if (rout->function_def_number != NULL_function_def_number) {
        a_scope_ptr def_scope = scope_for_routine(rout);
        if (def_scope && def_scope->assoc_block) {
          /* First, iterate and lower all block-local variables in the function */
          for (var = def_scope->variables; var != NULL; var = var->next) {
             /* Parameters are already handled in lower_function_decl */
             if (!var->is_parameter) {
                 const char *lname = var->source_corresp.name ? var->source_corresp.name : "unnamed_local";
                 gcc_jit_type *vtype = NULL;
                 err = gcc_gen_be_lower_type(var->type, &vtype);
                 if (err == GCC_GEN_BE_SUCCESS && vtype) {
                     gcc_jit_lvalue *local = gcc_jit_function_new_local(func, NULL, vtype, lname);
                     GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, local));
                 } else {
                     const char *err_msg = NULL;
                     gcc_gen_be_error_string(err, &err_msg);
                     fprintf(f_error, "gcc_gen_be local variable lowering error: %s\n", err_msg ? err_msg : "Unknown");
                     diagnostic_counters.total.errors++;
                 }
             }
          }

          gcc_jit_block *block = gcc_jit_function_new_block(func, "entry");
          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(block)); 
          err = gcc_gen_be_lower_statement(def_scope->assoc_block, func);
          if (err != GCC_GEN_BE_SUCCESS) {
              const char *err_msg = NULL;
              gcc_gen_be_error_string(err, &err_msg);
              fprintf(f_error, "gcc_gen_be statement lowering error: %s\n", err_msg ? err_msg : "Unknown");
              diagnostic_counters.total.errors++;
          }
        }
      }
    }
  }

  
  if (primary_source_file_name && diagnostic_counters.total.errors == 0 && diagnostic_counters.total.catastrophes == 0) {
    a_const_char *obj_name = gcc_be_output_file_name ? gcc_be_output_file_name : derived_name(primary_source_file_name, ".o");
    
    enum gcc_jit_output_kind output_kind = GCC_JIT_OUTPUT_KIND_OBJECT_FILE;
    if (obj_name) {
        size_t len = strlen(obj_name);
        if (len >= 2 && obj_name[len-2] == '.' && obj_name[len-1] == 's') {
            output_kind = GCC_JIT_OUTPUT_KIND_ASSEMBLER;
        } else if (len >= 3 && obj_name[len-3] == '.' && obj_name[len-2] == 's' && obj_name[len-1] == 'o') {
            output_kind = GCC_JIT_OUTPUT_KIND_DYNAMIC_LIBRARY;
        } else if (len >= 4 && obj_name[len-4] == '.' && obj_name[len-3] == 'd' && obj_name[len-2] == 'l' && obj_name[len-1] == 'l') {
            output_kind = GCC_JIT_OUTPUT_KIND_DYNAMIC_LIBRARY;
        } else if (len >= 4 && obj_name[len-4] == '.' && obj_name[len-3] == 'e' && obj_name[len-2] == 'x' && obj_name[len-1] == 'e') {
            output_kind = GCC_JIT_OUTPUT_KIND_EXECUTABLE;
        } else if (len >= 4 && obj_name[len-4] == '.' && obj_name[len-3] == 'o' && obj_name[len-2] == 'u' && obj_name[len-1] == 't') {
            output_kind = GCC_JIT_OUTPUT_KIND_EXECUTABLE;
        } else if (!strchr(obj_name, '.')) {
            output_kind = GCC_JIT_OUTPUT_KIND_EXECUTABLE;
        }
    }
    
    gcc_jit_context_compile_to_file(ctx, output_kind, obj_name);
    
    const char *err_str = gcc_jit_context_get_first_error(ctx);
    if (err_str) {
        fprintf(f_error, "gcc_gen_be compilation error: %s\n", err_str);
        diagnostic_counters.total.catastrophes++;
        return GCC_GEN_BE_ERROR_COMPILATION_FAILED;
    }
  }

  GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

  return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t back_end(void) GCC_GEN_BE_NOEXCEPT {
  return gcc_gen_be();
}



END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */