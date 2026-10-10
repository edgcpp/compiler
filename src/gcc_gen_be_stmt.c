/**
 * @file gcc_gen_be_stmt.c
 * @brief Implementation of statement lowering for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_type.h"
#include "expr.h"
#include <libgccjit.h>
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE




gcc_gen_be_error_t gcc_gen_be_get_label_block(struct gcc_jit_function *func, a_label_ptr label, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;
    if (!label) return GCC_GEN_BE_SUCCESS;
    
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_LABEL, label, &cached);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    if (!cached) {
        const char *name = "label";
        if (label->source_corresp.name) {
            name = label->source_corresp.name;
        } else if (label->break_label) {
            name = "break";
        } else if (label->continue_label) {
            name = "continue";
        }
        gcc_jit_block *block = gcc_jit_function_new_block(func, name);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_LABEL, label, block));
        *out_block = block;
    } else {
        *out_block = (gcc_jit_block *)cached;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_get_switch_case_block(struct gcc_jit_function *func, a_switch_case_entry_ptr scep, struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;
    if (!scep) return GCC_GEN_BE_SUCCESS;
    
    void *cached = NULL;
    gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_LABEL, scep, &cached);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    if (!cached) {
        gcc_jit_block *block = gcc_jit_function_new_block(func, scep->case_value ? "case" : "default");
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_LABEL, scep, block));
        *out_block = block;
    } else {
        *out_block = (gcc_jit_block *)cached;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_statement(a_statement_ptr stmt, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  gcc_gen_be_error_t err;

  while (stmt) {
      gcc_jit_block *current_block = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block));
      
      if (!current_block) {
          if (stmt->kind == stmk_label) {
              gcc_jit_block *lbl_block = NULL;
              err = gcc_gen_be_get_label_block(func, stmt->variant.label.ptr, &lbl_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(lbl_block));
              current_block = lbl_block;
          } else if (stmt->kind == stmk_switch_case) {
              gcc_jit_block *case_block = NULL;
              err = gcc_gen_be_get_switch_case_block(func, stmt->variant.switch_case.extra_info, &case_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(case_block));
              current_block = case_block;
          } else {
              stmt = stmt->next;
              continue;
          }
      } else {
          if (stmt->kind == stmk_label) {
              gcc_jit_block *label_block = NULL;
              err = gcc_gen_be_get_label_block(func, stmt->variant.label.ptr, &label_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              gcc_jit_block_end_with_jump(current_block, NULL, label_block);
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(label_block));
              current_block = label_block;
          } else if (stmt->kind == stmk_switch_case) {
              gcc_jit_block *case_block = NULL;
              err = gcc_gen_be_get_switch_case_block(func, stmt->variant.switch_case.extra_info, &case_block);
              if (err != GCC_GEN_BE_SUCCESS) return err;
              gcc_jit_block_end_with_jump(current_block, NULL, case_block);
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(case_block));
              current_block = case_block;
          }
      }

      switch (stmt->kind) {
          case stmk_block:
              if (stmt->variant.block.statements) {
                  err = gcc_gen_be_lower_statement(stmt->variant.block.statements, func);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
              }
              break;

          case stmk_expr:
              if (stmt->expr) {
                  gcc_jit_rvalue *rval = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &rval);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  if (rval) gcc_jit_block_add_eval(current_block, NULL, rval);
              }
              break;

          case stmk_return:
              if (stmt->expr) {
                  gcc_jit_rvalue *rval = NULL;
                  err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &rval);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  if (!rval) rval = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));
                  gcc_jit_block_end_with_return(current_block, NULL, rval);
              } else {
                  gcc_jit_block_end_with_void_return(current_block, NULL);
              }
              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
              break;

          case stmk_goto:
              {
                  gcc_jit_block *target = NULL;
                  err = gcc_gen_be_get_label_block(func, stmt->variant.label.ptr, &target);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  gcc_jit_block_end_with_jump(current_block, NULL, target);
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
              }
              break;

#if GNU_EXTENSIONS_ALLOWED
          case stmk_assigned_goto:
              {
                  /* STUB: libgccjit does not natively support computed gotos (goto *ptr). 
                     To fully implement this, we would need to map the pointer to a 
                     jump table or wait for libgccjit to add gcc_jit_block_end_with_indirect_jump. */
                  gcc_jit_rvalue *rval = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &rval);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      if (rval) gcc_jit_block_add_eval(current_block, NULL, rval);
                  }
                  
                  /* Unreachable fallback */
                  gcc_jit_block *unreachable_block = gcc_jit_function_new_block(func, "unreachable_after_assigned_goto");
                  gcc_jit_block_end_with_jump(current_block, NULL, unreachable_block);
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
              }
              break;
#endif

          case stmk_if:
          case stmk_constexpr_if:
          case stmk_if_consteval:
          case stmk_if_not_consteval:
              {
                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &cond);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  if (!cond) cond = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));

                  gcc_jit_block *then_block = gcc_jit_function_new_block(func, "if_then");
                  gcc_jit_block *else_block = gcc_jit_function_new_block(func, "if_else");
                  gcc_jit_block *merge_block = gcc_jit_function_new_block(func, "if_merge");

                  gcc_jit_block_end_with_conditional(current_block, NULL, cond, then_block, else_block);

                  gcc_jit_block *after_then = NULL;
                  gcc_jit_block *after_else = NULL;

                  if (stmt->kind == stmk_constexpr_if) {
                      if (stmt->variant.constexpr_if->then_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(then_block)); 
                          err = gcc_gen_be_lower_statement(stmt->variant.constexpr_if->then_statement, func);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_then));
                      }
                      else
                          after_then = then_block;

                      if (stmt->variant.constexpr_if->else_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(else_block)); 
                          err = gcc_gen_be_lower_statement(stmt->variant.constexpr_if->else_statement, func);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_else));
                      }
                      else
                          after_else = else_block;
                  } else {
                      if (stmt->variant.if_stmt.then_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(then_block)); 
                          err = gcc_gen_be_lower_statement(stmt->variant.if_stmt.then_statement, func);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_then));
                      }
                      else
                          after_then = then_block;

                      if (stmt->variant.if_stmt.else_statement) {
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(else_block)); 
                          err = gcc_gen_be_lower_statement(stmt->variant.if_stmt.else_statement, func);
                          if (err != GCC_GEN_BE_SUCCESS) return err;
                          GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_else));
                      }
                      else
                          after_else = else_block;
                  }

                  if (after_then) gcc_jit_block_end_with_jump(after_then, NULL, merge_block);
                  if (after_else) gcc_jit_block_end_with_jump(after_else, NULL, merge_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
              }
              break;

          case stmk_switch:
              {
                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &cond);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  if (!cond) cond = gcc_jit_context_zero(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));

                  a_switch_stmt_descr_ptr ssdp = stmt->variant.switch_stmt.extra_info;
                  int num_cases = 0;
                  a_switch_case_entry_ptr scep = ssdp->cases;
                  while (scep) {
                      if (scep != ssdp->default_case) num_cases++;
                      scep = scep->next;
                  }

                  gcc_jit_case **cases = NULL;
                  if (num_cases > 0) {
                      cases = (gcc_jit_case **)malloc((size_t)num_cases * sizeof(gcc_jit_case *));
                      if (!cases) return GCC_GEN_BE_ERROR_OOM;
                  }

                  int i = 0;
                  scep = ssdp->cases;
                  while (scep) {
                      if (scep != ssdp->default_case) {
                          long val_int = 0;
                          if (scep->case_value && scep->case_value->kind == ck_integer) {
                              a_boolean local_err = FALSE;
                              val_int = (long)value_of_integer_constant(scep->case_value, &local_err);
                          }
                          gcc_jit_rvalue *val = gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), val_int);
                          /* Cast to condition type */
                          val = gcc_jit_context_new_cast(ctx, NULL, val, gcc_jit_rvalue_get_type(cond));
                          
                          gcc_jit_block *case_block = NULL;
                          err = gcc_gen_be_get_switch_case_block(func, scep, &case_block);
                          if (err != GCC_GEN_BE_SUCCESS) { if(cases) free(cases); return err; }
                          
                          cases[i++] = gcc_jit_context_new_case(ctx, val, val, case_block);
                      }
                      scep = scep->next;
                  }

                  gcc_jit_block *default_block = NULL;
                  if (ssdp->default_case) {
                      err = gcc_gen_be_get_switch_case_block(func, ssdp->default_case, &default_block);
                      if (err != GCC_GEN_BE_SUCCESS) { if(cases) free(cases); return err; }
                  } else {
                      default_block = gcc_jit_function_new_block(func, "switch_default");
                  }

                  gcc_jit_block_end_with_switch(current_block, NULL, cond, default_block, num_cases, cases);
                  if (num_cases > 0) free(cases);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL)); 
                  err = gcc_gen_be_lower_statement(stmt->variant.switch_stmt.body_statement, func);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_block *after_switch = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_switch));
                  gcc_jit_block *exit_block = gcc_jit_function_new_block(func, "switch_exit");

                  if (!ssdp->default_case) {
                      gcc_jit_block_end_with_jump(default_block, NULL, exit_block);
                  }
                  if (after_switch) {
                      gcc_jit_block_end_with_jump(after_switch, NULL, exit_block);
                  }

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(exit_block));
              }
              break;

          case stmk_while:
          case stmk_end_test_while:
              {
                  gcc_jit_block *cond_block = gcc_jit_function_new_block(func, "loop_cond");
                  gcc_jit_block *body_block = gcc_jit_function_new_block(func, "loop_body");
                  gcc_jit_block *exit_block = gcc_jit_function_new_block(func, "loop_exit");

                  if (stmt->kind == stmk_end_test_while) {
                      gcc_jit_block_end_with_jump(current_block, NULL, body_block);
                  } else {
                      gcc_jit_block_end_with_jump(current_block, NULL, cond_block);
                  }

                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &cond);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  if (!cond) cond = gcc_jit_context_one(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));

                  gcc_jit_block_end_with_conditional(cond_block, NULL, cond, body_block, exit_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(body_block)); 
                  err = gcc_gen_be_lower_statement(stmt->variant.loop_statement, func);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_block *after_body = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_body));
                  if (after_body) gcc_jit_block_end_with_jump(after_body, NULL, cond_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(exit_block));
              }
              break;

          case stmk_for:
              {
                  a_for_loop_ptr loop_info = stmt->variant.for_loop.extra_info;
                  if (loop_info && loop_info->initialization) {
                      err = gcc_gen_be_lower_statement(loop_info->initialization, func);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  gcc_jit_block *cond_block = gcc_jit_function_new_block(func, "for_cond");
                  gcc_jit_block *body_block = gcc_jit_function_new_block(func, "for_body");
                  gcc_jit_block *step_block = gcc_jit_function_new_block(func, "for_step");
                  gcc_jit_block *exit_block = gcc_jit_function_new_block(func, "for_exit");

                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&current_block)); /* refresh */
                  gcc_jit_block_end_with_jump(current_block, NULL, cond_block);

                  gcc_jit_rvalue *cond = NULL;
                  if (stmt->expr) {
                      err = gcc_gen_be_lower_expr_rvalue(stmt->expr, &cond);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  if (!cond) cond = gcc_jit_context_one(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT));

                  gcc_jit_block_end_with_conditional(cond_block, NULL, cond, body_block, exit_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(body_block)); 
                  err = gcc_gen_be_lower_statement(stmt->variant.for_loop.statement, func);
                  if (err != GCC_GEN_BE_SUCCESS) return err;
                  
                  gcc_jit_block *after_body = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_body));
                  if (after_body) gcc_jit_block_end_with_jump(after_body, NULL, step_block);

                  if (loop_info && loop_info->increment) {
                      gcc_jit_rvalue *rval = NULL;
                      err = gcc_gen_be_lower_expr_rvalue(loop_info->increment, &rval);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      if (rval) gcc_jit_block_add_eval(step_block, NULL, rval);
                  }
                  gcc_jit_block_end_with_jump(step_block, NULL, cond_block);

                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(exit_block));
              }
              break;

          case stmk_try_block:
              {
                  /* Setup landing pad and exception handler basic blocks.
                     Because libgccjit lacks explicit try/catch primitives without using
                     __builtin_eh_return or other mechanisms, we stub the actual landing pad. */
                  gcc_jit_block *try_block = gcc_jit_function_new_block(func, "try_body");
                  gcc_jit_block *catch_pad = gcc_jit_function_new_block(func, "catch_pad");
                  gcc_jit_block *merge_block = gcc_jit_function_new_block(func, "try_merge");

                  gcc_jit_block_end_with_jump(current_block, NULL, try_block);
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(try_block));
                  if (stmt->variant.try_block && stmt->variant.try_block->statement) {
                      err = gcc_gen_be_lower_statement(stmt->variant.try_block->statement, func);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                  }
                  
                  gcc_jit_block *after_try = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_try));
                  if (after_try) gcc_jit_block_end_with_jump(after_try, NULL, merge_block);
                  
                  /* Lower catch blocks (unreachable without explicit landing pad hook) */
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(catch_pad));
                  a_handler_ptr handler = stmt->variant.try_block ? stmt->variant.try_block->handlers : NULL;
                  
                  if (handler) {
                      gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                      gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                      gcc_jit_param *cxa_begin_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "exc");
                      gcc_jit_function *begin_catch_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__cxa_begin_catch", 1, &cxa_begin_param, 0);
                      gcc_jit_function *end_catch_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_end_catch", 0, NULL, 0);
                      
                      while (handler) {
                          gcc_jit_block *catch_block = gcc_jit_function_new_block(func, "catch_body");
                          {
                              gcc_jit_block *temp_block = NULL;
                              GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&temp_block));
                              gcc_jit_block_end_with_jump(temp_block, NULL, catch_block);
                          }
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(catch_block));
                          
                          gcc_jit_rvalue *exc_ptr = gcc_jit_context_null(ctx, void_ptr_type);
                          gcc_jit_block_add_eval(catch_block, NULL, gcc_jit_context_new_call(ctx, NULL, begin_catch_fn, 1, &exc_ptr));
                          
                          if (handler->statement) {
                              err = gcc_gen_be_lower_statement(handler->statement, func);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                          }
                          
                          gcc_jit_block *after_catch = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&after_catch));
                          if (after_catch) {
                              gcc_jit_block_add_eval(after_catch, NULL, gcc_jit_context_new_call(ctx, NULL, end_catch_fn, 0, NULL));
                              gcc_jit_block_end_with_jump(after_catch, NULL, merge_block);
                          }
                          handler = handler->next;
                          
                          if (handler) {
                              gcc_jit_block *next_catch_pad = gcc_jit_function_new_block(func, "next_catch_pad");
                              GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(next_catch_pad));
                          }
                      }
                  } else {
                      gcc_jit_block_end_with_jump(catch_pad, NULL, merge_block);
                  }
                  
                  GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(merge_block));
              }
              break;

          case stmk_decl:
              /* Ignored: variables allocated at function entry */
              break;
              
          case stmk_init:
              {
                  a_dynamic_init_ptr dip = stmt->variant.dynamic_init;
                  if (dip && dip->kind != dik_none && dip->variable) {
                      void *cached = NULL;
                      err = cache_lookup(GCC_GEN_BE_CACHE_VAR, dip->variable, &cached);
                      if (err != GCC_GEN_BE_SUCCESS) return err;
                      
                      gcc_jit_lvalue *var_lvalue = (gcc_jit_lvalue *)cached;
                      if (var_lvalue) {
                          gcc_jit_rvalue *rval = NULL;
                          if (dip->kind == dik_zero) {
                              gcc_jit_type *vtype = NULL;
                              err = gcc_gen_be_lower_type(dip->variable->type, &vtype);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                              rval = gcc_jit_context_zero(ctx, vtype);
                          } else if (dip->kind == dik_constant || dip->kind == dik_nonconstant_aggregate) {
                              a_constant_ptr con = dip->variant.constant.ptr;
                              gcc_jit_type *type = NULL;
                              err = gcc_gen_be_lower_type(dip->variable->type, &type);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                              
                              if (con->kind == ck_integer) {
                                  a_boolean local_err = FALSE;
                                  long val = (long)value_of_integer_constant(con, &local_err);
                                  rval = gcc_jit_context_new_rvalue_from_long(ctx, type, val);
                              } else if (con->kind == ck_float) {
                                  double val = (double)fetch_host_fp_value(dip->variable->type->variant.float_kind, &con->variant.float_value);
                                  rval = gcc_jit_context_new_rvalue_from_double(ctx, type, val);
                              } else if (con->kind == ck_string) {
                                  rval = gcc_jit_context_new_string_literal(ctx, (const char *)con->variant.string.value);
                              } else {
                                  rval = gcc_jit_context_zero(ctx, type);
                              }
                          } else if (dip->kind == dik_expression || dip->kind == dik_class_result_via_ctor) {
                              err = gcc_gen_be_lower_expr_rvalue(dip->variant.expression, &rval);
                              if (err != GCC_GEN_BE_SUCCESS) return err;
                          }
                          
                          if (rval) {
                              gcc_jit_block_add_assignment(current_block, NULL, var_lvalue, rval);
                          }
                      }
                  }
              }
              break;

          case stmk_asm:
              {
                  an_asm_entry_ptr asm_entry = stmt->variant.asm_entry;
                  if (asm_entry) {
                      const char *asm_str = "";
                      if (asm_entry->asm_string && asm_entry->asm_string->kind == ck_string) {
                          asm_str = (const char *)asm_entry->asm_string->variant.string.value;
                      }
                      
#if defined(GCC_JIT_EXTENDED_ASM_GOTO)
                      /* GCC 11+ supports asm goto via gcc_jit_block_end_with_extended_asm_goto */
                      gcc_jit_extended_asm *ext_asm = NULL;
                      if (asm_entry->is_asm_goto) {
                          /* Need to collect blocks from asm_entry->labels */
                          int num_labels = 0;
                          a_label_list_ptr lbl = asm_entry->labels;
                          while (lbl) { num_labels++; lbl = lbl->next; }
                          
                          gcc_jit_block **dest_blocks = NULL;
                          if (num_labels > 0) {
                              dest_blocks = (gcc_jit_block **)malloc((size_t)num_labels * sizeof(gcc_jit_block *));
                              if (!dest_blocks) return GCC_GEN_BE_ERROR_OOM;
                              int i = 0;
                              for (lbl = asm_entry->labels; lbl; lbl = lbl->next) {
                                  GCC_GEN_BE_CHECK(gcc_gen_be_get_label_block(func, lbl->label, &dest_blocks[i++]));
                              }
                          }
                          
                          gcc_jit_block *fallthrough_block = gcc_jit_function_new_block(func, "asm_goto_fallthrough");
                          ext_asm = gcc_jit_block_end_with_extended_asm_goto(current_block, NULL, asm_str, num_labels, dest_blocks, fallthrough_block);
                          
                          if (dest_blocks) free(dest_blocks);
                          GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(fallthrough_block));
                      } else {
                          ext_asm = gcc_jit_block_add_extended_asm(current_block, NULL, asm_str);
                      }
#else
                      gcc_jit_extended_asm *ext_asm = gcc_jit_block_add_extended_asm(current_block, NULL, asm_str);
#endif
                      if (ext_asm) {
                          gcc_jit_extended_asm_set_volatile_flag(ext_asm, asm_entry->is_volatile);
                          
                          an_asm_operand_ptr op = asm_entry->operands;
                          while (op) {
                              const char *name = op->name;
                              const char *constraint = op->constraints_string;
                              if (op->is_output_operand) {
                                  gcc_jit_lvalue *lval = NULL;
                                  err = gcc_gen_be_lower_expr_lvalue(op->expression, &lval);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;
                                  if (lval) {
                                      gcc_jit_extended_asm_add_output_operand(ext_asm, name, constraint, lval);
                                  }
                              } else {
                                  gcc_jit_rvalue *rval = NULL;
                                  err = gcc_gen_be_lower_expr_rvalue(op->expression, &rval);
                                  if (err != GCC_GEN_BE_SUCCESS) return err;
                                  if (rval) {
                                      gcc_jit_extended_asm_add_input_operand(ext_asm, name, constraint, rval);
                                  }
                              }
                              op = op->next;
                          }
                          
                          a_named_register_list_ptr clobber = asm_entry->clobbers;
                          while (clobber) {
                              if (clobber->reg > anr_invalid && clobber->reg < anr_last) {
                                  const char *reg_name = named_register_names[clobber->reg];
                                  if (reg_name) {
                                      gcc_jit_extended_asm_add_clobber(ext_asm, reg_name);
                                  }
                              }
                              clobber = clobber->next;
                          }
                      }
                  }
              }
              break;

          default:
              break;
      }

      stmt = stmt->next;
  }
  
  return GCC_GEN_BE_SUCCESS;
}



END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */