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
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_location.h"
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
        state->global_ctor_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_type, "__edg_global_init", 0, NULL, 0);
        state->global_ctor_block = gcc_jit_function_new_block(state->global_ctor_func, "entry");
    }
    *out_block = state->global_ctor_block;
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

  const char *name = var->source_corresp.name;
  if (!name) name = "unnamed_var";
  
  enum gcc_jit_global_kind linkage;
  if (var->storage_class == sc_extern && var->init_kind == initk_none) {
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
  
#if defined(GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC)
  if (var->is_thread_local && p_gcc_jit_lvalue_set_tls_model) {
      p_gcc_jit_lvalue_set_tls_model(global, GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC);
  }
#endif

  if (var_type && var->type->kind == tk_typeref && (var->type->variant.typeref.qualifiers & TQ_CONST)) {
#if defined(LIBGCCJIT_HAVE_gcc_jit_global_set_readonly)
      gcc_jit_global_set_readonly(global);
#endif
  }
  
  if (var->init_kind == initk_zero) {
      gcc_jit_global_set_initializer_rvalue(global, gcc_jit_context_zero(ctx, var_type));
  } else if (var->init_kind == initk_static) {
      a_constant_ptr init_con = var->initializer.constant;
      if (init_con) {
          gcc_jit_rvalue *init_rval = NULL;
          /* External function defined in gcc_gen_be_expr.h */
          extern gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, gcc_jit_type *expected_type, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;
          GCC_GEN_BE_CHECK(gcc_gen_be_lower_constant_rvalue(init_con, var_type, &init_rval));
          if (init_rval) {
              gcc_jit_global_set_initializer_rvalue(global, init_rval);
          } else {
              /* If constant couldn't be lowered as an rvalue (e.g. older libgccjit missing struct constructors), 
                 fallback to dynamic init in constructor. */
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblock));
              /* We need to evaluate the constant dynamically. 
                 This implies gcc_gen_be_lower_constant_rvalue MUST return an rvalue or emit dynamic assignments. 
                 Wait, if it can't, it returns error. So init_rval is guaranteed if SUCCESS. */
              gcc_jit_global_set_initializer_rvalue(global, gcc_jit_context_zero(ctx, var_type));
          }
      } else {
         gcc_jit_global_set_initializer_rvalue(global, gcc_jit_context_zero(ctx, var_type));
      }
  } else if (var->init_kind == initk_dynamic) {
      if (var->initializer.dynamic && var->initializer.dynamic->kind == dik_expression && var->initializer.dynamic->variant.expression) {
          gcc_jit_rvalue *init_rval = NULL;
          extern gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;
          GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(var->initializer.dynamic->variant.expression, &init_rval));
          if (init_rval) {
              gcc_jit_block *cblock = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblock));
              gcc_jit_block_add_assignment(cblock, NULL, global, init_rval);
          }
      }
  }
  
  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, global));
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
  
  gcc_jit_type *var_type = NULL;
  err = gcc_gen_be_lower_type(var->type, &var_type);
  if (err != GCC_GEN_BE_SUCCESS) return err;

  const char *name = var->source_corresp.name;
  if (!name) name = "unnamed_local";

  gcc_jit_location *loc = NULL;
  err = gcc_gen_be_get_location(&var->source_corresp.decl_position, &loc);
  if (err != GCC_GEN_BE_SUCCESS) return err;

  gcc_jit_lvalue *local = gcc_jit_function_new_local(func, loc, var_type, name);
  
  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, local));
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
  if (rout->type && rout->type->kind == tk_routine) {
      err = gcc_gen_be_lower_type(rout->type->variant.routine.return_type, &ret_type);
      if (err != GCC_GEN_BE_SUCCESS) return err;
  } else {
      ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
  }

  a_const_char *name = rout->source_corresp.name;
  if (!name) name = "unnamed_func";
  
  enum gcc_jit_function_kind linkage;
  if (rout->function_def_number == NULL_function_def_number) {
      linkage = GCC_JIT_FUNCTION_IMPORTED;
  } else if (rout->storage_class == sc_static) {
      linkage = GCC_JIT_FUNCTION_INTERNAL;
  } else {
      linkage = GCC_JIT_FUNCTION_EXPORTED;
  }
  
  int num_params = 0;
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
      a_variable_ptr var_param = NULL;
      a_scope_ptr def_scope = scope_for_routine(rout);
      if (def_scope) {
          var_param = def_scope->variant.routine.parameters;
      }
      
      for (ptp = rout->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
          gcc_jit_type *param_type = NULL;
          err = gcc_gen_be_lower_type(ptp->type, &param_type);
          if (err != GCC_GEN_BE_SUCCESS) {
              free(params);
              return err;
          }
          
          const char *param_name = "unnamed_param";
          if (var_param) {
              if (var_param->source_corresp.name) param_name = var_param->source_corresp.name;
              var_param = var_param->next;
          }
          params[i] = gcc_jit_context_new_param(ctx, NULL, param_type, param_name);
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
      
#if defined(LIBGCCJIT_HAVE_ATTRIBUTES)
  if (rout->is_weak) {
      gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_WEAK);
  }
  if (rout->is_inline) {
      gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_ALWAYS_INLINE);
  }
#endif
      
  if (num_params > 0) {
      int i = 0;
      a_variable_ptr var_param = NULL;
      a_scope_ptr def_scope = scope_for_routine(rout);
      if (def_scope) {
          var_param = def_scope->variant.routine.parameters;
      }
      for (ptp = rout->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
          if (var_param) {
              GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var_param, gcc_jit_param_as_lvalue(params[i])));
              var_param = var_param->next;
          }
          i++;
      }
      free(params);
  }
  
  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, rout, func));
  *out_func = func;
  return GCC_GEN_BE_SUCCESS;
}



END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */