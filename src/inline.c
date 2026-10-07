/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

inline.c -- Minimal inlining for IL lowering.

Does inlining of calls to inline functions, doing the transformations
on code in IL tree form as part of IL lowering.  Intended to be "minimal"
and mostly for use with the C-generating back end.  Intended to do
inlining at about the same level as cfront.  Called from IL lowering
to handle C++ lowering, and from C99 IL lowering to handle C99 lowering.
*/

#include "basic_hdrs.h"
#if DO_IL_LOWERING
/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in IL lowering. */
#include "lower_hdrs.h"
#endif /* DO_IL_LOWERING */

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Only include this code if it is needed: */
#if DO_IL_LOWERING
#if MINIMAL_INLINING

#include "folding.h"
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */

#if !DO_FULL_PORTABLE_EH_LOWERING
 #error -- Inlining requires full portable lowering of exception handling.
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


STATIC_THREAD a_variable_remapping_for_inlining_ptr
		variable_remappings_for_inlining;
			/* List of remappings of variables to be done while
			   copying the body of a function being inlined.  The
			   list is kept in the order of argument evaluation. */


STATIC_THREAD a_scope_ptr
		routine_scope_being_inlined;
			/* When non-NULL, a call of the routine associated
			   with this scope is being expanded as an inline. */


static void set_inline_statement_positions(
                                a_statement_ptr             statement,
                                ARG_UNUSED a_statement_ptr  original_statement)
/*
Set source positions for an inlined statement.  Statements being inlined at a
call site will have the source position of the call site when 
STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION is TRUE.  When FALSE,
the statements will have their original source positions.  Both position and
end_position (when EXTRA_SOURCE_POSITIONS_IN_IL is TRUE) are set in statement.
original_statement points to the original statement whose source positions may
be copied; it may be NULL if there is no corresponding original statement.
If statement is NULL, no assignments are performed.
*/
{
  if (statement != NULL) {
#if STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION
    set_stmt_pos_to_code_pos_for_lowering(statement);
#else /* !STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION */
    if (original_statement != NULL) {
      statement->position = original_statement->position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      statement->end_position = original_statement->end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    }  /* if */
#endif /* STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION */
  }  /* if */
}  /* set_inline_statement_positions */

static a_variable_remapping_for_inlining_ptr
                   alloc_variable_remapping_for_inlining(
                      a_variable_ptr                        var,
                      a_boolean                             eval_right_to_left,
                      a_variable_remapping_for_inlining_ptr *p_last_remap)
/*
Allocate and initialize an entry used to record a variable remapping in effect
during inlining of a function call.  var is the variable that will be remapped.
The list of variable remappings is kept, in the order that they should be
executed and pointed to by the variable_remappings_for_inlining global
variable.  When eval_right_to_left is FALSE, *p_last_remap points to the last
entry on that list, or is NULL if the last entry is not known; it is updated on
return.
*/
{
  a_variable_remapping_for_inlining_ptr vrip, last_remap = *p_last_remap;

  if (avail_variable_remappings_for_inlining != NULL) {
    /* Reuse an entry previously allocated and freed. */
    vrip = avail_variable_remappings_for_inlining;
    avail_variable_remappings_for_inlining = vrip->next;
  } else {
    /* Allocate a new entry. */
    vrip = (a_variable_remapping_for_inlining_ptr)
                           alloc_fe(sizeof(a_variable_remapping_for_inlining));
#if DEBUG
    num_variable_remappings_for_inlining++;
#endif /* DEBUG */
  }  /* if */
  /* Keep the variable mappings in the order that the arguments are being
     evaluated.  The C++ standard specifies the order of evaluation of the
     arguments in a call resulting from use of an overloaded operator but
     currently not for one written with the function call syntax.  In the
     absence of a specific order, assume left-to-right. */
  if (eval_right_to_left) {
    vrip->next = variable_remappings_for_inlining;
    variable_remappings_for_inlining = vrip;
  } else {
    if (last_remap == NULL) {
      /* Find last entry on list. */
      last_remap = variable_remappings_for_inlining;
      if (last_remap != NULL) {
        while (last_remap->next != NULL) last_remap = last_remap->next;
      }  /* if */
    } else {
      check_assertion(last_remap->next == NULL);
    }  /* if */
    if (last_remap == NULL) {
      variable_remappings_for_inlining = vrip;
    } else {
      last_remap->next = vrip;
    }  /* if */
    vrip->next = NULL;
    *p_last_remap = vrip;
  }  /* if */
  vrip->orig_variable = var;
  var->remapping_for_inlining = vrip;
  vrip->kind = vrk_none;
  vrip->arg_expr = NULL;
  vrip->arg_side_effect_expr = NULL;
  vrip->orig_temporary = NULL;
  vrip->temporary_used = FALSE;
  vrip->remapping_used = FALSE;
  vrip->local_temporary_okay = FALSE;
  vrip->local_temporary_reused = FALSE;
  return vrip;
}  /* alloc_variable_remapping_for_inlining */


static void free_variable_remappings_for_inlining(void)
/*
Free the variable remapping entries on the global list by returning them to
the available list.
*/
{
  a_variable_remapping_for_inlining_ptr vrip, vrip_next;

  for (vrip = variable_remappings_for_inlining;
       vrip != NULL;
       vrip = vrip_next) {
    vrip_next = vrip->next;
    vrip->next = avail_variable_remappings_for_inlining;
    avail_variable_remappings_for_inlining = vrip;
    vrip->orig_variable->remapping_for_inlining = NULL;
  }  /* for */
  variable_remappings_for_inlining = NULL;
}  /* free_variable_remappings_for_inlining */

#if DEBUG

static void db_variable_remapping(a_variable_remapping_for_inlining_ptr vrip)
/*
Display the indicated variable remapping for debugging purposes.
*/
{
  db_variable(vrip->orig_variable);
  if (vrip->kind == vrk_none) {
    fprintf(f_debug, " (no remapping)");
  } else {
    fprintf(f_debug, " --> ");
    if (vrip->kind == vrk_temporary) {
      db_name(&vrip->variant.variable->source_corresp);
    } else if (vrip->kind == vrk_constant_expr) {
      an_expr_node_ptr expr = vrip->variant.expr;
      if (is_constant_node(expr)) {
        db_constant(node_constant(vrip->variant.expr));
      } else if (is_variable_node(expr)) {
        if (expr->is_lvalue) fprintf(f_debug, "[lvalue]");
        db_name(&node_variable(expr)->source_corresp);
      } else {
        db_expression(expr);
      }  /* if */
    } else {
      fprintf(f_debug, " <bad remapping>");
    }  /* if */
  }  /* if */
  fprintf(f_debug, "\n");
}  /* db_variable_remapping */

#endif /* DEBUG */

static void make_remapping_temporary(
                          a_variable_remapping_for_inlining_ptr
                                    vrip,
                          a_boolean is_temp_for_constructor_this_inlined_param,
                          a_boolean is_temp_for_unmodified_inlined_param)
/*
Allocate a temporary variable as the remapping for the variable indicated
in the given remapping entry.  The is_temp_... flags give values for the
like-named flags in the temporary variable.
*/
{
  a_variable_ptr             orig_var = vrip->orig_variable;
  a_variable_ptr             temp_var = NULL;
  a_temporary_list_entry_ptr tlep = NULL;
  a_boolean                  is_register_mapped = FALSE;

  /* Only automatic variables should get here: Such variables cannot have a
     Embedded C (ISO/IEC TR 18037) named register storage class.  Similarly,
     they cannot have a GNU asm name.  However, they may have an associated
     GNU asm register. */ 
#if NAMED_REGISTERS_ALLOWED
  check_assertion(!orig_var->has_named_register_storage_class);
#endif /* NAMED_REGISTERS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  if (orig_var->asm_name_is_valid) {
    check_assertion(orig_var->asm_name_or_reg.name == NULL);
  } else {
    is_register_mapped = TRUE;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Try to reuse an existing local temporary.  Don't do that, though,
     if the variable being remapped has special attributes. */
  if (!orig_var->address_taken &&
      !orig_var->initialization_rewritten_as_assignment && 
      !(orig_var->init_kind == (an_init_kind)initk_dynamic &&
        orig_var->initializer.dynamic->is_partially_initialized) &&
      !(orig_var->init_kind == (an_init_kind)initk_zero) && 
      !is_register_mapped) {
    vrip->local_temporary_okay = TRUE;
    do {
      temp_var = find_reusable_temporary(orig_var->type, &tlep);
      if (temp_var == NULL) break;
      /* Reject the temporary and look for another if it doesn't have the
         right set of flags. */
    } while (is_temp_for_constructor_this_inlined_param !=
             temp_var->is_temp_for_constructor_this_inlined_param ||
             is_temp_for_unmodified_inlined_param !=
             temp_var->is_temp_for_unmodified_inlined_param);
  }  /* if */
  if (temp_var != NULL) {
    tlep->in_use = TRUE;
    vrip->local_temporary_reused = TRUE;
  } else {
    /* Allocate a new variable for the temporary. */
    temp_var = make_temporary(orig_var->type, /*force_static=*/FALSE);
  }  /* if */
  vrip->kind = vrk_temporary;
  vrip->variant.variable = temp_var;
  if (orig_var->address_taken) temp_var->address_taken = TRUE;
  if (orig_var->initialization_rewritten_as_assignment) {
    temp_var->initialization_rewritten_as_assignment = TRUE;
  }  /* if */
  /* Maintain information about initialization to zero. */
  if (orig_var->init_kind == (an_init_kind)initk_zero) {
    temp_var->init_kind = (an_init_kind)initk_zero;
  }  /* if */
  temp_var->is_temp_for_constructor_this_inlined_param =
                                    is_temp_for_constructor_this_inlined_param;
  temp_var->is_temp_for_unmodified_inlined_param =
                                    is_temp_for_unmodified_inlined_param;
#if GNU_EXTENSIONS_ALLOWED
  if (is_register_mapped) {
    /* The remapped temporary should be associated with the same register as
       the original. */
    temp_var->asm_name_is_valid = FALSE;
    temp_var->asm_name_or_reg.reg = orig_var->asm_name_or_reg.reg;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* make_remapping_temporary */


static a_boolean func_body_has_side_effects(a_routine_ptr routine)
/*
Return TRUE if the indicated routine's body has side effects.  The
safe answer is TRUE.  More specifically, this returns TRUE if the function
body has any side effects that can affect the values of argument expressions.
*/
{
  a_boolean       has_side_effects = TRUE;
  a_scope_ptr     scope = scope_for_routine(routine);
  a_statement_ptr stmt = scope->assoc_block;

  /* Only consider functions where the top block contains only a return
     statement. */
  if (stmt->kind == (a_statement_kind)stmk_block) {
    stmt = stmt->variant.block.statements;
    if (stmt != NULL &&
        stmt->kind == (a_statement_kind)stmk_return &&
        stmt->next == NULL) {
      check_assertion(stmt->variant.return_dynamic_init == NULL);
      if (stmt->expr != NULL) {
        an_expr_node_ptr expr = stmt->expr;
        /* If the final act of the function call is to do an assignment
           whose destination expression has no side effects, ignore that
           with respect to the side effects of the function; it happens
           after everything else, and therefore cannot affect the values
           of argument expressions. */
        if (is_operation_node(expr)) {
          an_expr_operator_kind op = expr->variant.operation.kind;
          if (is_simple_assignment(op)) {
            an_expr_node_ptr op1 = expr->variant.operation.operands;
            if (!node_has_side_effects(op1, (a_boolean *)NULL)) {
              expr = op1->next;
              /* if */
            }  /* if */
          }  /* if */
        }  /* if */
        has_side_effects = node_has_side_effects(expr,
                                                 (a_boolean *)NULL);
      } else {
        has_side_effects = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  return has_side_effects;
}  /* func_body_has_side_effects */


static void set_up_variable_remapping_for_inlining(
                                           a_scope_ptr      scope,
                                           a_boolean        eval_right_to_left,
                                           an_expr_node_ptr arg_expr_list)
/*
We are beginning an attempt to inline a call of the routine whose scope
is "scope" with the (already lowered) arguments arg_expr_list.  Generate
temporary variables for parameters and local variables and establish a
remapping list to be used when expanding the body of the function.
No code is inserted yet to set the temporary variables; see
finish_variable_remapping_for_inlining.  The arguments to the call are
evaluated from left-to-right unless eval_right_to_left is TRUE.
*/
{
  a_variable_ptr   param_var, var;
  an_expr_node_ptr arg;
  a_variable_remapping_for_inlining_ptr
                   vrip, last_remap = NULL;
  a_routine_ptr    routine = scope->variant.routine.ptr;
#if DEBUG
  a_boolean        first = TRUE;
#endif /* DEBUG */
  a_boolean        call_has_side_effects, arg_list_has_side_effects;

  /* Determine whether the call has side effects, either because the
     function body has side effects or because the argument expressions
     have side effects. */
  call_has_side_effects = func_body_has_side_effects(routine);
  arg_list_has_side_effects = FALSE;
  for (arg = arg_expr_list; arg != NULL; arg = arg->next) {
    if (node_has_side_effects(arg, (a_boolean *)NULL)) {
      call_has_side_effects = TRUE;
      arg_list_has_side_effects = TRUE;
      break;
    }  /* if */
  }  /* for */
  /* Process the parameters. */
  for (param_var = scope->variant.routine.parameters, arg = arg_expr_list;
       param_var != NULL;
       param_var = param_var->next, arg = arg->next) {
    check_assertion_str(arg != NULL,
                        "set_up_variable_remapping_...: too few args");
    /* Allocate a remapping for the parameter. */
    vrip = alloc_variable_remapping_for_inlining(param_var, eval_right_to_left,
                                                 &last_remap);
    vrip->arg_expr = arg;
    if (!param_var->source_corresp.referenced) {
      /* We don't need the parameter if it's not referenced.  However, if
         the argument has side effects, we need to evaluate it. */
      if (node_has_side_effects(arg, (a_boolean *)NULL)) {
        /* Note that, if successful, finish_variable_remapping_for_inlining
           will set arg->next to NULL. */
        vrip->arg_side_effect_expr = arg;
      }  /* if */
    } else {
      /* The parameter is referenced, so it has to be remapped. */
      /* See if the argument value is constant (meaning, in this case,
         invariant over the lifetime of the call). */
      a_boolean is_non_null;
      a_boolean arg_is_constant = is_constant_valued_expression(
                                                  arg,
                                                  arg_list_has_side_effects,
                                                  call_has_side_effects,
                                                  /*this_cannot_be_null=*/TRUE,
                                                  &is_non_null);
      /* See if the parameter is modified. */
      a_boolean param_is_unmodified = FALSE;
      a_boolean param_is_constructor_this = FALSE;
      if (!param_var->param_value_has_been_changed &&
          !param_var->param_used_as_lvalue) {
        /* The parameter doesn't get changed (because its address isn't taken
           and it is never used as an lvalue). */
        param_is_unmodified = TRUE;
      } else if (param_var->is_this_parameter &&
                 routine->special_kind ==
                                       (a_special_function_kind)sfk_constructor
#if NEW_CAN_BE_FOLDED_INTO_CTOR
                 && !routine->is_delegating_ctor
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if ASSIGNMENT_TO_THIS_ALLOWED
                 && !routine->assignment_to_this_done
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
                                                     ) {
        /* For the "this" parameter of a constructor, take advantage of
           the fact that we know how it works, so we can eliminate the
           allocation code at the top of the routine even though there's
           an assignment to "this" in that code when it does the allocation.
           We can only do that if the value being assigned to "this" is
           non-null (if that assumption is violated, the routine will fail
           to be inlined and a remark will be issued). */
        param_is_constructor_this = TRUE;
        if (is_non_null) param_is_unmodified = TRUE;
      }  /* if */
      if (!arg_is_constant && !call_has_side_effects) {
        /* The call has no side effects (and therefore this argument
           expression has no side effects), so any expression is invariant
           over the call and can be passed without a temporary, at the
           expense of extra code.  Do it if the parameter is used only once. */
        if (!param_var->param_used_more_than_once &&
            /* The param_used_more_than_once flag is not maintained for
               parameters added by IL lowering. */
            has_name(param_var)) {
          arg_is_constant = TRUE;
        }  /* if */
      }  /* if */
      if (param_is_unmodified && arg_is_constant &&
          /* A class value can't arbitrarily be copied to a class value; it
             must be used at the original location (make an exception for
             lowered pointer-to-member function constants because we know
             that we can copy those). */
          (!is_class_struct_union_type(param_var->type) ||
           is_ptr_to_member_function_constant_expr(arg))) {
        a_type_ptr arg_type = skip_typerefs(arg->type);
#if BACK_END_IS_C_GEN_BE
        if (gcc_or_clang_is_generated_code_target &&
            is_cast_operation_node(arg) &&
            is_pointer_type(arg_type) &&
            is_pointer_type(arg->variant.operation.operands->type) &&
            is_function_type(type_pointed_to(arg_type)) &&
            !f_identical_types(type_pointed_to(arg_type),
                               type_pointed_to(
                                        arg->variant.operation.operands->type),
                               ITF_NO_FLAGS)) {
          /* Add a check for one special case: If the argument is a cast of
             a function pointer where the cast and the function pointer don't
             have identical types, use a temporary rather than a constant
             expression for the parameter, otherwise a gcc back end will insert
             an illegal instruction if the function pointer cast is not
             identical to the function type (and the cast function pointer is
             used to make a call). */
        } else
#endif /* BACK_END_IS_C_GEN_BE */
        /* Do not insert code here. */
        {
          /* The argument is constant-valued and the parameter is unmodified.
             The parameter gets remapped to a constant-valued expression. */
          vrip->kind = vrk_constant_expr;
          vrip->variant.expr = arg;
        }  /* if */
      } else if (param_is_unmodified && is_operation_node(arg)) {
        /* Look for an argument of the form:
                ( expr-with-side-effects , constant-value-expr )
           Such expressions are often generated by lowering (e.g., to generate
           a reference).  Rather than create a temporary for the entire
           argument expression, use the second operand of the comma expression
           as a constant-valued expression for the parameter and evaluate the
           first operand of the comma expression before the call to the inlined
           routine (see finish_variable_remapping_for_inlining). */
        if (is_operation_node(arg) && node_operator_is(arg, eok_comma)) {
          an_expr_node_ptr first_op = arg->variant.operation.operands;
          an_expr_node_ptr second_op = first_op->next;
          if (is_constant_valued_expression(second_op,
                                            arg_list_has_side_effects,
                                            call_has_side_effects,
                                            /*this_cannot_be_null=*/TRUE,
                                            &is_non_null) &&
              /* A class value can't arbitrarily be copied to a class value;
                 it must be used at the original location (make an exception
                 for lowered pointer-to-member function constants because we
                 know that we can copy those). */
              (!is_class_struct_union_type(param_var->type) ||
               is_ptr_to_member_function_constant_expr(second_op))) {
            /* The second operand of the comma is constant-valued; the
               parameter gets remapped to this constant-valued expression
               to replace instances of the parameter in the inlined code and
               save the first operand of the comma to be executed before the
               call (if necessary).  Note that any transformations that are
               done here must be undone if inlining fails.  In this case
               we're simply capturing pointers to the first and second
               operands of the comma operation -- not re-linking anything, so
               there's nothing to undo. */
            vrip->kind = vrk_constant_expr;
            vrip->variant.expr = second_op;
            if (node_has_side_effects(first_op, (a_boolean *)NULL)) {
              /* Make sure the first operand of the comma expression is
                 evaluated before the inlined call. */
              vrip->arg_side_effect_expr = first_op;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      if (vrip->kind == vrk_none) {
        /* A temporary is needed for the parameter. */
        make_remapping_temporary(vrip,
                                 param_is_constructor_this,
                                 param_is_unmodified);
        /* We will need to generate code to initialize the variable to
           the argument value.  See finish_variable_remapping_for_inlining. */
      }  /* if */
#if DEBUG
      if (debug_level >= 4) {
        if (first) {
          fprintf(f_debug, "Parameter remappings established:\n");
          first = FALSE;
        }  /* if */
        db_variable_remapping(vrip);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* for */
  /* Process the local variables. */
#if DEBUG
  first = TRUE;
#endif /* DEBUG */
  for (var = scope->nonstatic_variables;
       var != NULL;
       var = var->next) {
    /* We don't need the variable if it's not referenced. */
    if (var->source_corresp.referenced) {
      /* Remap the local variable to a temporary. */
      vrip = alloc_variable_remapping_for_inlining(var, eval_right_to_left,
                                                   &last_remap);
      make_remapping_temporary(
                    vrip,
                    (a_boolean)var->is_temp_for_constructor_this_inlined_param,
                    (a_boolean)var->is_temp_for_unmodified_inlined_param);
#if DEBUG
      if (debug_level >= 4) {
        if (first) {
          fprintf(f_debug, "Variable remappings established:\n");
          first = FALSE;
        }  /* if */
        db_variable_remapping(vrip);
      }  /* if */
#endif /* DEBUG */
    }  /* if */        
  }  /* for */
}  /* set_up_variable_remapping_for_inlining */


static void restore_mapping_to_temporary(
                                    a_variable_remapping_for_inlining_ptr vrip)
/*
If vrip is a remapping that was originally a vrk_temporary and that has been
temporarily remapped to something else, restore the original mapping.
*/
{
  if (vrip->kind != vrk_temporary && vrip->orig_temporary != NULL) {
    vrip->kind = vrk_temporary;
    vrip->variant.variable = vrip->orig_temporary;
    vrip->orig_temporary = NULL;
  }  /* if */
}  /* restore_mapping_to_temporary */


static void finish_variable_remapping_for_inlining(
                                           a_statement_ptr    block_stmt,
                                           an_insert_location *insert_location)
/*
We have gotten to the end of the inlining of a function call, and
successfully.  Put the temporary variables previously created into the
calling context scope, and generate code to initialize the temporaries
for parameters from the argument expressions.  If block_stmt is non-NULL,
it indicates a block at the start of which any generated code should be
added.  If it is NULL, *insert_location indicates an expression for the
expanded call, and any generated code should be added preceding that
expression, and *insert_location updated to allow further insertion
following the original expression.
*/
{
  a_variable_remapping_for_inlining_ptr vrip;
  a_statement_ptr                       stmt;
  an_insert_location                    local_insert_location;
  a_boolean                             any_code_inserted = FALSE;

  /* Look at each remapping established. */
  for (vrip = variable_remappings_for_inlining;
       vrip != NULL;
       vrip = vrip->next) {
    if (vrip->temporary_used) {
      /* If a vrk_temporary remapping was temporarily switched to some
         other remapping, but it was used at some point when it was
         a vrk_temporary remapping, switch it back. */
      restore_mapping_to_temporary(vrip);
    }  /* if */
    if ((vrip->arg_expr != NULL &&
         vrip->kind == vrk_temporary) ||
         vrip->arg_side_effect_expr != NULL) {
      /* Some code will have to be inserted.  Set up the insert location for
         that the first time it's needed. */
      if (!any_code_inserted) {
        any_code_inserted = TRUE;
        if (block_stmt != NULL) {
          set_block_start_insert_location(block_stmt, &local_insert_location);
        } else {
          /* For an expression insert location, create an expression off
             to the side and insert it at the end of processing. */
          set_expr_creation_insert_location(&local_insert_location);
        }  /* if */
      }  /* if */
    }  /* if */
    if (vrip->kind == vrk_temporary) {
      /* A temporary. */
      a_variable_ptr temp_var = vrip->variant.variable;
      if (vrip->local_temporary_reused) {
        /* This variable is a previously-allocated temporary.  It's already
           on the scope list and on the reusable temporaries list. */
      } else {
        /* Add the variable to the current scope. */
        add_temporary_to_scope(temp_var, (a_scope_ptr)NULL,
                               /*promote_if_necessary=*/FALSE);
        if (vrip->local_temporary_okay) {
          /* This variable is used like a local temporary.  Put it on the list
             of such temporaries so that it can be reused after the end of
             the current full expression. */
          add_to_reusable_temporaries_list(temp_var);
        }  /* if */
      }  /* if */
      if (vrip->arg_expr != NULL) {
        /* Add code to initialize the temporary from the argument
           expression. */
        vrip->arg_expr->next = NULL;
        stmt = insert_var_assignment_statement(temp_var, vrip->arg_expr,
                                               &local_insert_location);
        set_stmt_pos_to_code_pos_for_lowering(stmt);
        temp_var->initialization_rewritten_as_assignment = TRUE;
      }  /* if */
    }  /* if */
    if (vrip->arg_side_effect_expr != NULL) {
      /* There's an expression that needs to be evaluated before the call
         (often because an argument with side-effects was passed to an
         unused parameter, but also in other cases).  Make sure the
         expression's "next" pointer is reset (it may still be pointing
         to the argument that follows it -- in case inlining failed -- but
         we know it's been successful at this point). */
      vrip->arg_side_effect_expr->next = NULL;
      stmt = insert_expr_statement(vrip->arg_side_effect_expr,
                                   &local_insert_location);
      set_stmt_pos_to_code_pos_for_lowering(stmt);
    }  /* if */
  }  /* for */
  if (any_code_inserted && block_stmt == NULL) {
    /* Some code was generated, so insert it at the position provided by
       the caller. */
    check_assertion(is_expr_insert_location(insert_location));
    insert_expr(insert_location->variant.expr, &local_insert_location);
    *insert_location = local_insert_location;
  }  /* if */
}  /* finish_variable_remapping_for_inlining */


static a_variable_remapping_for_inlining_ptr
                             get_var_remapping_for_inlining(a_variable_ptr var)
/*
See if the variable var is remapped in the current inlining operation.
If so, return a pointer to the remapping entry.  If not, return NULL.
*/
{
  a_variable_remapping_for_inlining_ptr vrip;

  vrip = var->remapping_for_inlining;
  if (vrip != NULL) {
    check_assertion(vrip->orig_variable == var);
    if (vrip->kind == vrk_none) vrip = NULL;
  }  /* if */
  return vrip;
}  /* get_var_remapping_for_inlining */


a_variable_ptr remap_var_for_inlining(a_variable_ptr var)
/*
See if the indicated variable is remapped in the current inlining operation.
Return the new variable if there is a remapping, or the original variable
if not.  NULL is returned if the remapping is to something other than a
temporary variable.
*/
{
  a_variable_ptr                        new_var;
  a_variable_remapping_for_inlining_ptr vrip;

  vrip = get_var_remapping_for_inlining(var);
  if (vrip != NULL) {
    /* There is a remapping. */
    if (vrip->kind == vrk_temporary) {
      new_var = vrip->variant.variable;
      vrip->remapping_used = TRUE;
      vrip->temporary_used = TRUE;
    } else {
      /* Not a temporary variable (as expected).  This can happen in some cases
         where the argument used for "this" in a constructor can't be
         determined to be non-null.  Returning NULL will typically result in
         a failure to inline the function. */
      new_var = NULL;
    }  /* if */
  } else {
    /* There is no remapping, so return the original variable. */
    new_var = var;
  }  /* if */
  return new_var;
}  /* remap_var_for_inlining */


void adjust_copied_expression_for_inlining(an_expr_node_ptr expr,
                                           a_boolean        *inlining_failed)
/*
The indicated (rvalue or lvalue) expression has just been created as a copy of
an expression during inlining.  See whether it should be adjusted, e.g.,
because of remapped variables.  *inlining_failed is set to TRUE if the
expression can't be inlined.
*/
{
  an_expr_node_kind     kind = enum_cast<an_expr_node_kind>(expr->kind);
  a_type_ptr            expr_type = expr->type;
  a_variable_remapping_for_inlining_ptr
                        vrip;
  a_constant_ptr        con;
  an_expr_node_ptr      constant_expr;
  a_constant_ptr        constant = local_constant();
  a_boolean             is_non_null, has_constant_value = FALSE;

  if (kind == (an_expr_node_kind)enk_variable) {
    if (expr->is_lvalue) {
      /* Variable lvalue.  See if the variable is remapped. */
      a_variable_ptr var = remap_var_for_inlining(node_variable(expr));
      if (var != NULL) {
        node_variable(expr) = var;
      } else {
        /* No temporary for this mapping; inlining fails. */
        *inlining_failed = TRUE;
      }  /* if */
    } else {
      /* Value of a variable.  See if the variable is remapped. */
      vrip = get_var_remapping_for_inlining(node_variable(expr));
      if (vrip != NULL) {
        /* Yes, there is some kind of remapping. */
        switch (vrip->kind) {
          case vrk_temporary:
            /* The variable is remapped to a temporary variable. */
            node_variable(expr) = vrip->variant.variable;
            vrip->temporary_used = TRUE;
            break;
          case vrk_constant_expr:
            /* The variable is remapped to a constant-valued expression.
               Look for some special cases. */
            constant_expr = vrip->variant.expr;
            if (is_constant_node(constant_expr)) {
              /* The variable is remapped to a constant.  Use an enk_constant
                 instead. */
              a_type_ptr  orig_lvalue_type = expr->orig_lvalue_type;
              set_expr_node_kind(expr, (an_expr_node_kind)enk_constant);
              node_constant(expr) = node_constant(constant_expr);
              expr->orig_lvalue_type = orig_lvalue_type;
            } else {
              /* Other, more complicated, cases.  Just copy the expression. */
              overwrite_node(expr,
                             copy_expr_tree_for_inlining(constant_expr,
                                                         inlining_failed));
              if (is_ptr_to_member_type(expr_type)) {
                /* Restore the original type, which might be slightly different
                   for pointer-to-member cases. */
                expr->type = expr_type;
              }  /* if */
            }  /* if */
            break;
          default:
            unexpected_condition_str(
                      "adjust_copied_expression_for_inlining: bad remap kind");
        }  /* switch */
        vrip->remapping_used = TRUE;
      }  /* if */
    }  /* if */
  } else if (kind == (an_expr_node_kind)enk_constant) {
    /* Value of a constant. */
    /* When doing inlining, we may have a constant here that is in
       a function scope memory region other than the one we are currently
       working in.  If so, we need to make a copy of the constant so we
       aren't pointing over to another function scope memory region.
       The constant we are copying is probably from an initializer, and
       therefore unshared, but this reference from an expression node
       can use a shareable constant. */
    con = node_constant(expr);
    if (!in_file_scope(con)) {
      node_constant(expr) = alloc_shareable_constant(con);
      /* The expression tree would also be in another function scope. */
      node_constant(expr)->expr = NULL;
    }  /* if */
  } else if (kind == (an_expr_node_kind)enk_operation) {
    /* Look for operations that now have constant operands because of
       parameter variables remapped to constants. */
    an_expr_operator_kind op = expr->variant.operation.kind;
    an_expr_node_ptr      operand = expr->variant.operation.operands;
    an_expr_node_ptr      operand2 = operand->next;
    a_constant_ptr        con2 = NULL;
#if GNU_EXTENSIONS_ALLOWED && BACK_END_IS_C_GEN_BE
    if (op == eok_call &&
        gcc_is_generated_code_target &&
        innermost_function_scope != NULL &&
        innermost_function_scope->variant.routine.ptr->always_inline &&
        is_eh_setjmp_routine(routine_from_function_expr(operand))) {
      /* The code being inlined calls setjmp, and the routine it is being
         inlined into has the GNU always_inline attribute.  gcc gives an
         error on an always_inline routine that calls setjmp
         (because such a routine can never be inlined), so do not inline the
         call. */
      *inlining_failed = TRUE;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && BACK_END_IS_C_GEN_BE */
    /* Look for operations that now have constant operands because of
       parameter variables remapped to constants. */
    if (is_constant_node(operand) &&
        (operand2 == NULL || is_constant_node(operand2))) {
      /* The operands are constant. */
      a_boolean did_not_fold = TRUE, template_constant = FALSE;
      con = node_constant(operand);
      if (operand2 != NULL) con2 = node_constant(operand2);
      /* Fold certain constant operations.  Some, like floating-point
         operations, are not folded because cfront does not do so,
         and because if it were done people might get different
         results with inlining. */
      /* See copy_and_simplify_short_circuited_operation for the
         short-circuited operations. */
      switch (op) {
        case eok_add:
        case eok_subtract:
        case eok_multiply:
        case eok_divide:
        case eok_remainder:
        case eok_padd:
        case eok_psubtract:
        case eok_pdiff:
        case eok_shiftl:
        case eok_shiftr:
        case eok_and:
        case eok_or:
        case eok_xor:
        case eok_eq:
        case eok_ne:
        case eok_gt:
        case eok_lt:
        case eok_ge:
        case eok_le:
          /* Avoid folding operations on pointers to data members that haven't
             been lowered into integers yet (because the constant is in the
             file scope).  Test is done for integer/pointer to be conservative
             in case other kinds of constants in the future are changed by
             lowering. */
          check_assertion(con2 != NULL);  /* For Coverity */
          if ((con->kind != (a_constant_repr_kind)ck_integer &&
               con->kind != (a_constant_repr_kind)ck_address) ||
              (con2->kind != (a_constant_repr_kind)ck_integer &&
               con2->kind != (a_constant_repr_kind)ck_address)) {
            /* Do not fold. */
          } else {
            if (is_ptr_to_member_type(expr_type)) {
              /* The result is a pointer to data member type (which will
                 eventually be lowered but isn't yet). */
              expr_type = integer_type(targ_ptr_to_data_member_int_kind);
            }  /* if */
            /* Setting evaluated_context to FALSE suppresses warnings on
               errors like division by zero.  Instead, did_not_fold is
               returned TRUE. */
            binary_operation(op, con, con2, expr_type, constant,
                             /*constant_context=*/FALSE,
                             /*evaluated_context=*/FALSE,
                             &did_not_fold,
                             &template_constant,
                             /*error_detected=*/(an_error_code *)NULL,
                             &code_pos_for_lowering);
          }  /* if */
          break;
        case eok_negate:
          if (!node_operator_type_kind_is(expr, tk_integer)) break;
          FALLTHROUGH
        case eok_complement:
        case eok_not:
          /* See comment above; avoid pointers to data members.  This is
             probably unnecessary. */
          if (con->kind != (a_constant_repr_kind)ck_integer &&
              con->kind != (a_constant_repr_kind)ck_address) {
            /* Do not fold. */
          } else {
            /* Setting evaluated_context to FALSE suppresses warnings on
               errors like division by zero.  Instead, did_not_fold is
               returned TRUE. */
            unary_operation(op, con, expr_type, constant,
                            /*constant_context=*/FALSE,
                            /*evaluated_context=*/FALSE,
                            &did_not_fold,
                            &template_constant,
                            /*error_detected=*/(an_error_code *)NULL,
                            &code_pos_for_lowering);
          }  /* if */
          break;
        default:
          /* Others cannot be folded. */
          break;
      }  /* switch */
      check_assertion(!template_constant);
      if (!did_not_fold) {
        /* The expression can be folded. */
        has_constant_value = TRUE;
      }  /* if */
    } else if ((op == (an_expr_operator_kind)eok_ne ||
                op == (an_expr_operator_kind)eok_eq) &&
               node_operator_type_kind_is(expr, tk_pointer)) {
      /* A special case where we can do folding even with a nonconstant
         operand: &variable != 0 is always 1.  The "== 0" case is
         always 0. */
      if (is_constant_valued_expression(operand,
                                        /*local_vars_change=*/TRUE,
                                        /*other_vars_change=*/TRUE,
                                        /*this_cannot_be_null=*/TRUE,
                                        &is_non_null) &&
          is_non_null) {
        operand = operand->next;
        if (is_constant_node(operand) &&
            constant_bool_value_known_at_compile_time(
                                                    node_constant(operand)) &&
            is_false_constant(node_constant(operand))) {
          /* Yes, this is &auto_variable != NULL, which is always 1,
             or the "== 0" case, which is always 0. */
          a_host_large_integer	temp_value;
          has_constant_value = TRUE;
          temp_value = (a_host_large_integer)
                              ((op == (an_expr_operator_kind)eok_ne)? 1L : 0L);
          set_integer_constant(constant, temp_value,
                               (an_integer_kind)ik_int);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (expr->is_non_normalized_boolean_controlling_expr) {
    /* Non-normalized boolean controlling expressions have an implied "!= 0",
       so check the inlined expression to see if it has a constant, non-zero
       value, and if it does, replace the expression with a "1".  This is
       similar to the eok_ne test above (which often replaces this test in
       configurations where LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS
       is TRUE). */
    if (is_constant_valued_expression(expr,
                                      /*local_vars_change=*/TRUE,
                                      /*other_vars_change=*/TRUE,
                                      /*this_cannot_be_null=*/TRUE,
                                      &is_non_null) &&
        is_non_null) {
      has_constant_value = TRUE;
      set_integer_constant(constant, (a_host_large_integer)1L,
                           (an_integer_kind)ik_int);
    }  /* if */
  }  /* if */
  if (has_constant_value) {
    /* Replace the expression by a constant value. */
    constant->type = expr_type;
    /* Avoid problems like a function-scope constant pointing to a file-scope
       backing expression. */
    constant->expr = NULL;
    set_expr_node_kind(expr, (an_expr_node_kind)enk_constant);
    node_constant(expr) = alloc_shareable_constant(constant);
  }  /* if */
  release_local_constant(&constant);
}  /* adjust_copied_expression_for_inlining */


a_boolean copy_and_simplify_short_circuited_operation(
                                             an_expr_node_ptr expr,
                                             a_boolean        *inlining_failed)
/*
expr is a copy of an enk_operation node being made while copying an
expression for inlining.  The node "expr" is a copy, but its subtree has
not been copied yet.  If expr is a short-circuitable operation, do
the rest of the copy (simplifying in the process) and return TRUE;
otherwise, do no copying and return FALSE.  *inlining_failed is set to
TRUE if there's a failure during inlining.
*/
{
  a_boolean             processed = FALSE;
  an_expr_operator_kind op = expr->variant.operation.kind;
  an_expr_node_ptr      operand, operand2, operand3;

  if (op == (an_expr_operator_kind)eok_question ||
      op == (an_expr_operator_kind)eok_lor ||
      op == (an_expr_operator_kind)eok_land) {
    /* This is a short-circuited operation. */
    a_boolean  op1_value;
    processed = TRUE;
    operand = expr->variant.operation.operands;
    operand2 = operand->next;
    operand3 = operand2->next;
    /* Copy the first operand.  In the process, simplify to a constant if
       possible by substituting for parameter variables. */
    operand = copy_expr_tree_for_inlining(operand, inlining_failed);
    if (bool_value_is_known_at_compile_time(operand,
                           assume_this_cannot_be_null_in_conditional_operators,
                                            &op1_value)) {
      /* The first operand is known false or known true, so the operation can
         be simplified. */
      if (op == (an_expr_operator_kind)eok_question) {
        /* "?" operation.  Keep the second or third operand on the basis
           of the value of the first operand. */
        if (op1_value) {
          /* The first operand is true, so keep the second operand. */
          operand2 = copy_expr_tree_for_inlining(operand2, inlining_failed);
          overwrite_node(expr, operand2);
        } else {
          /* The first operand is false, so keep the third operand. */
          operand3 = copy_expr_tree_for_inlining(operand3, inlining_failed);
          overwrite_node(expr, operand3);
        }  /* if */
      } else if (op == (an_expr_operator_kind)eok_lor) {
        /* "||" operation. */
        if (op1_value) {
          /* The first operand is true, so the overall operation has the
             value true. */
          overwrite_node(expr, operand);
        } else {
          /* The first operand is false, so the second operand is the value
             of the expression. */
          operand2 = copy_expr_tree_for_inlining(operand2, inlining_failed);
          overwrite_node(expr, operand2);
        }  /* if */
      } else if (op == (an_expr_operator_kind)eok_land) {
        /* "&&" operation. */
        if (op1_value) {
          /* The first operand is true, so the second operand is the value
             of the expression. */
          operand2 = copy_expr_tree_for_inlining(operand2, inlining_failed);
          overwrite_node(expr, operand2);
        } else {
          /* The first operand is false, so the overall operation has the
             value false. */
          overwrite_node(expr, operand);
        }  /* if */
      } else {
        unexpected_condition();
      }  /* if */
    } else {
      /* The first operand is not known false or known true, so this operation
         cannot be simplified.  Just copy the rest of the operands. */
      operand2 = copy_expr_tree_for_inlining(operand2, inlining_failed);
      if (operand3 != NULL) {
        operand3 = copy_expr_tree_for_inlining(operand3, inlining_failed);
      }  /* if */
      /* Link the copied operands together. */
      expr->variant.operation.operands = operand;
      operand->next = operand2;
      operand2->next = operand3;
    }  /* if */
  } else if (is_simple_assignment(op) &&
             (node_operator_type_kind_is(expr, tk_integer) ||
              node_operator_type_kind_is(expr, tk_pointer))) {
    /* If this is an assignment to a temporary with special properties
       created previously by inlining, we may be able to do something
       special. */
    operand = expr->variant.operation.operands;
    operand2 = operand->next;
    if (is_variable_node(operand)) {
      a_variable_ptr var = node_variable(operand);
      if (var->is_temp_for_constructor_this_inlined_param ||
          var->is_temp_for_unmodified_inlined_param) {
        a_variable_remapping_for_inlining_ptr vrip;
        a_boolean                             temp_elim_possible = FALSE;
        /* This is a temporary with special properties.  If the value
           being assigned to the temporary is constant (and non-null,
           for the constructor "this" case), the temporary can be remapped
           to the constant and eliminated.  Note that we are grabbing the
           operation here before the subtree under it has been remapped,
           so we can look at the original variable being assigned to. */
        a_boolean is_non_null;
        vrip = get_var_remapping_for_inlining(var);
        check_assertion(vrip != NULL);
        /* If a vrk_temporary remapping was temporarily switched to some
           other remapping, switch it back. */
        restore_mapping_to_temporary(vrip);
        check_assertion(vrip->kind == vrk_temporary);
        /* Copy the source operand with substitution and constant folding
           so we can see if we have a constant. */
        operand2 = copy_expr_tree_for_inlining(operand2, inlining_failed);
        processed = TRUE;
        if (is_constant_valued_expression(operand2,
                                          /*local_vars_change=*/TRUE,
                                          /*other_vars_change=*/TRUE,
                                          /*this_cannot_be_null=*/TRUE,
                                          &is_non_null) &&
            (!var->is_temp_for_constructor_this_inlined_param ||
             is_non_null)) {
          /* The temporary elimination cannot be done if the temporary has
             already been referenced. */
          if (!vrip->remapping_used) temp_elim_possible = TRUE;
        }  /* if */
        if (temp_elim_possible) {
          a_constant_ptr constant = local_constant();
          /* Change the remapping of the temporary.  Note that changing the
             remapping means that the temporary variable will not be
             added to the scope at the end of the current inline expansion,
             so the temporary disappears completely. */
          /* Save the original temporary pointer so it can be restored.
             This comes up if the temporary is reused; it might be used
             also for another parameter, and might or might not be
             optimized away in that case. */
          vrip->orig_temporary = vrip->variant.variable;
          vrip->kind = vrk_constant_expr;
          vrip->variant.expr = operand2;
          /* Eliminate the assignment node by replacing it with a zero
             of the right type. */
          make_zero_of_proper_type(expr->type, constant);
          set_expr_node_kind(expr, (an_expr_node_kind)enk_constant);
          node_constant(expr) = alloc_shareable_constant(constant);
          release_local_constant(&constant);
        } else {
          /* The operation cannot be eliminated, so finish the rewriting,
             leaving an updated assignment in place. */
          operand = copy_expr_tree_for_inlining(operand, inlining_failed);
          operand->next = operand2;
          expr->variant.operation.operands = operand;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return processed;
}  /* copy_and_simplify_short_circuited_operation */


static a_statement_ptr copy_inlined_statement(
                                           a_statement_ptr    statement,
                                           an_insert_location *insert_location)
/*
Make a copy of the indicated statement, insert it at *insert_location, and
return a pointer to the copy.  Note that any top level expressions that may
exist in the "input" statement are copied to the "output" statement (though
they are likely invalid in their new context) and should be overwritten by
the caller as appropriate.  No lowering post pass is performed on any
expressions contained in the statement.
*/
{
  a_statement_ptr new_statement = alloc_statement(statement->kind,
                                                  /*compiler_generated=*/TRUE);

  /* This does not use copy_statement, because we don't want to re-set any
     "back" pointers. */
  *new_statement = *statement;
  new_statement->next = NULL;
  new_statement->parent = NULL;
  new_statement->compiler_generated = TRUE;
#if DO_IL_LOWERING
  new_statement->lowering_generated = il_lowering_underway;
#endif /* DO_IL_LOWERING */
  set_inline_statement_positions(new_statement, statement);
  new_statement->has_associated_pragma = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  new_statement->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  insert_statement_full(new_statement, insert_location,
                        /*perform_post_pass=*/FALSE);
  return new_statement;
}  /* copy_inlined_statement */


static void expand_statement_inline(a_statement_ptr       statement,
                                    an_insert_location    *insert_location,
                                    a_host_large_unsigned *statement_count,
                                    a_boolean             *is_too_large,
                                    a_boolean             *inlinable,
                                    a_boolean             *failed)
/*
Generate a copy of the indicated statement as part of the expansion of an
inline function call.  Insert the copy at *insert_location and update
*insert_location accordingly.  If the expansion cannot be done for some
reason, set *failed TRUE.  If it is noted that the expansion can never
be done, in any context, also set *inlinable FALSE.  If statement is NULL,
do nothing.  Note that the insert location may be an expression insert
location; in that case, only expressions can be inserted, so other kinds
of statements can be inserted only if they can be turned into expressions.
If not, *failed is set.  Expressions are run through a lowering post pass
as they are copied and not when they are inserted (as part of a statement)
into the IL tree.  *statement_count is used to keep track of how large
the inlined routine is; the top-level caller sets *statement_count to
zero and *statement_count is incremented during the inlining process.
If it reaches a configured threshold, inlining fails and *is_too_large
is set to TRUE (*failed is also set to TRUE and *inlinable is set to FALSE).
This is useful in cases where iterative inlining can create huge routines.
*/
{
  an_expr_node_ptr   stmt_expr, expr;
  a_statement_ptr    new_statement, stmt, prev_stmt;
  a_label_ptr        label;
  an_insert_location sub_insert_location;
  a_boolean          result_is_then, result_is_else;

  if (statement == NULL) {
    /* No statement to copy. */
  } else if (statement->has_associated_pragma) {
    /* Can't inline a statement with an associated pragma. */
    goto cannot_inline_ever;
  } else if ((*statement_count)++ > inline_statement_limit &&
             inline_statement_limit != 0) {
    /* Some recursive inlined routines can run the front end out of memory
       so arbitrarily set a limit on how large an inlined routine can be.
       This measure is currently based solely on the number of statements
       that the inlined routine contains, but could be adjusted to take
       other factors (e.g., expressions, variables, etc.) into account. */
    *is_too_large = TRUE;
    *failed = TRUE;
    *inlinable = FALSE;
  } else {
    stmt_expr = statement->expr;
    if (stmt_expr != NULL) {
      /* Make a copy of the expression tree (also performs substitution
         of argument values for parameters). */
      stmt_expr = copy_expr_tree_for_inlining(stmt_expr, failed);
    }  /* if */
    switch (statement->kind) {
      case stmk_empty:
        /* An empty statement has no side effects: copy it over as is if we
           are inserting a statement.  If we are inserting an expression, just
           ignore this. */
        if (!is_expr_insert_location_kind(insert_location->kind)) {
          (void)copy_inlined_statement(statement, insert_location);
        }  /* if */
        break;
      case stmk_expr:
        if (is_expr_insert_location(insert_location)) {
          /* An expression statement is copied as an expression. */
          insert_expr(stmt_expr, insert_location);
        } else {
          /* An expression statement is copied as an expression statement. */
          new_statement = copy_inlined_statement(statement, insert_location);
          new_statement->expr = stmt_expr;
          check_assertion(stmt_expr != NULL);  /* For Coverity. */
          set_expr_result_not_used(stmt_expr);
        }  /* if */
        break;
      case stmk_goto:
        /* Gotos in general cannot be inlined, but look for the case
             { ... goto L; } L:
           at the top level, which comes up in destructor epilogues. */
        label = statement->variant.label.ptr;
        stmt = routine_scope_being_inlined->assoc_block->
                                                      variant.block.statements;
        /* See if the label is a top-level label. */
        prev_stmt = NULL;
        for (; stmt != NULL && stmt != label->exec_stmt;
             prev_stmt = stmt, stmt = stmt->next) {}
        if (stmt != NULL && prev_stmt != NULL) {
          /* The label is a top-level label.  See if the previous statement
             is the goto we're considering, or a block whose last statement is
             the goto we're considering. */
          if (prev_stmt->kind == (a_statement_kind)stmk_block) {
            prev_stmt = last_statement_in_block(prev_stmt);
          }  /* if */
          if (prev_stmt == statement) {
            /* Yes.  This goto can just be deleted. */
            break;
          }  /* if */
        }  /* if */
        goto cannot_inline_ever;
      case stmk_label:
#if GNU_EXTENSIONS_ALLOWED
        if (statement->variant.label.ptr->address_taken) {
          /* Don't inline routines where the address of a label has been
             taken (i.e., by the address-of-label GNU extension). */
          goto cannot_inline_ever;
        }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
        /* Labels are just removed.  The processing on the gotos, if any,
           will decide whether inlining can be done. */
        break;
      case stmk_return:
        /* Returns are allowed only as the last thing in the routine. */
        /* Find the last statement in the top block.  Do that repeatedly
           so we can find a return that is the last statement in a block
           that is the last statement in the block ... that is the last
           statement in the top level block of the function. */
        stmt = routine_scope_being_inlined->assoc_block;
        do {
          stmt = last_statement_in_block(stmt);
        } while (stmt != NULL && stmt->kind == (a_statement_kind)stmk_block);
        if (stmt == statement) {
          /* Yes, this return is the last in the top block, so it can be
             inlined. */
          if (is_expr_insert_location(insert_location)) {
            /* Expression insert location. */
            if (stmt_expr != NULL) {
              insert_expr(stmt_expr, insert_location);
            } else {
              /* No expression is being returned. */
              a_type_ptr routine_type = f_skip_typerefs(
                                                  routine_scope_being_inlined->
                                                   variant.routine.ptr->type);
              if (routine_type->variant.routine.extra_info->
                                                 value_returned_as_parameter) {
                /* The routine has been modified to return its value via
                   an added parameter, so don't check for the return type
                   matching the type of the expression. */
              } else {
                a_type_ptr routine_return_type = routine_type->
                                                   variant.routine.return_type;
                if (!is_void_type(routine_return_type)) {
                  /* The return statement returns nothing and the function
                     expects a return value, so this function cannot be
                     inlined. */
                  goto cannot_inline_ever;
                }  /* if */
              }  /* if */
            }  /* if */
          } else {
            /* Statement insert location.  A return without an expression
               is just thrown away.  A return with an expression is just
               the expression (for side effects).  In GNU statement
               expressions, the expression for the final statement is
               the value of the statement expression, and is used. */
            if (stmt_expr != NULL &&
                (!stmt_expr->result_is_not_used ||
                 node_has_side_effects(stmt_expr, (a_boolean *)NULL))) {
              if (stmt_expr->result_is_not_used) {
                stmt_expr = add_cast(stmt_expr, void_type());
              }  /* if */
              stmt = insert_expr_statement(stmt_expr, insert_location);
              set_inline_statement_positions(stmt, statement);
            }  /* if */
          }  /* if */
          break;
        }  /* if */
        goto cannot_inline_ever;
      case stmk_if:
        /* "if", "if consteval", or "if not consteval" statement. */
        /* See if the tested expression is known.  If so, the "if" can be
           reduced to the "then" or "else" statement.  An "if consteval" can
           always be reduced to the "else" statement since the "then" branch
           is only taken in constant evaluation (i.e., before inlining).
           Similarly, "if not consteval" can be reduced to the "then" branch.
        */
        result_is_then = result_is_else = FALSE;
        check_assertion(stmt_expr != NULL);
        if (statement->kind == (a_statement_kind)stmk_if_not_consteval) {
          result_is_then = TRUE;
        } else if (statement->kind == (a_statement_kind)stmk_if_consteval) {
          result_is_else = TRUE;
        } else if (is_constant_node(stmt_expr) &&
                   constant_bool_value_known_at_compile_time(
                                                  node_constant(stmt_expr))) {
          if (is_false_constant(node_constant(stmt_expr))) {
            result_is_else = TRUE;
          } else {
            result_is_then = TRUE;
          }  /* if */
        }  /* if */
        if (is_expr_insert_location(insert_location)) {
          an_expr_node_ptr then_expr = NULL, else_expr = NULL;
          /* Expression insert location.  Turn an "if" into a "?" operator. */
          if (!result_is_else) {
            /* Copy the "then" statement. */
            set_expr_creation_insert_location(&sub_insert_location);
            expand_statement_inline(statement->variant.if_stmt.then_statement,
                                    &sub_insert_location, statement_count,
                                    is_too_large, inlinable, failed);
            if (!*failed) {
              then_expr = sub_insert_location.variant.expr;
              if (then_expr == NULL) {
                /* No "then" statement; use 0. */
                then_expr = node_for_integer_constant(0L,
                                                      (an_integer_kind)ik_int);
              }  /* if */
              then_expr = add_cast_if_necessary(then_expr, void_type());
            }  /* if */
          }  /* if */
          if (!result_is_then) {
            if (statement->variant.if_stmt.else_statement != NULL) {
              /* Copy the "else" statement. */
              set_expr_creation_insert_location(&sub_insert_location);
              expand_statement_inline(
                                     statement->variant.if_stmt.else_statement,
                                     &sub_insert_location, statement_count,
                                     is_too_large, inlinable, failed);
              if (!*failed) {
                else_expr = sub_insert_location.variant.expr;
                else_expr = add_cast_if_necessary(else_expr, void_type());
              }  /* if */
            } else {
              /* No "else" statement; use (void)0. */
              else_expr = add_cast(
                            node_for_integer_constant(0L,
                                                      (an_integer_kind)ik_int),
                                                      void_type());
            }  /* if */
          }  /* if */
          if (!*failed) {
            if (result_is_then) {
              /* The result is the "then" expression. */
              expr = then_expr;
            } else if (result_is_else) {
              /* The result is the "else" expression. */
              expr = else_expr;
            } else {
              /* Make the "?" operator. */
              stmt_expr->next = then_expr;
              then_expr->next = else_expr;
              expr = make_operator_node((an_expr_operator_kind)eok_question,
                                        void_type(),
                                        stmt_expr);
              expr->is_initialization_guard=statement->is_initialization_guard;
            }  /* if */
            insert_expr(expr, insert_location);
          }  /* if */
        } else {
          a_statement_ptr then_stmt = NULL, else_stmt = NULL;
          /* Statement insert location. */
          if (!result_is_else) {
            /* Copy the "then" statement. */
            set_statement_creation_insert_location(&sub_insert_location);
            expand_statement_inline(statement->variant.if_stmt.then_statement,
                                    &sub_insert_location, statement_count,
                                    is_too_large, inlinable, failed);
            if (!*failed) {
              then_stmt = sub_insert_location.variant.statement.stmt;
            }  /* if */
          }  /* if */
          if (!result_is_then) {
            if (statement->variant.if_stmt.else_statement != NULL) {
              /* Copy the "else" statement. */
              set_statement_creation_insert_location(&sub_insert_location);
              expand_statement_inline(
                                     statement->variant.if_stmt.else_statement,
                                     &sub_insert_location, statement_count,
                                     is_too_large, inlinable, failed);
              if (!*failed) {
                else_stmt = sub_insert_location.variant.statement.stmt;
              }  /* if */
            } else {
              else_stmt = NULL;
            }  /* if */
          }  /* if */
          if (!*failed) {
            if ((result_is_then && then_stmt == NULL) ||
                (!result_is_then && !result_is_else &&
                 then_stmt == NULL && else_stmt == NULL)) {
              /* Either the result is known to be true, but there's no "then"
                 statement, or the result isn't known but there are no "then"
                 or "else" statements.  In these cases, turn the "if" statement
                 into an expression statement (to execute the condition). */
              set_expr_result_not_used(stmt_expr);
              new_statement = alloc_statement(stmk_expr,
                                              /*compiler_generated=*/TRUE);
              new_statement->expr = stmt_expr;
              set_inline_statement_positions(new_statement, statement);
              insert_statement_full(new_statement, insert_location,
                                    /*perform_post_pass=*/FALSE);
            } else if (result_is_then) {
              /* The result is the "then" statement. */
              insert_statement_full(then_stmt, insert_location,
                                    /*perform_post_pass=*/FALSE);
            } else if (result_is_else) {
              /* The result is the "else" statement. */
              if (else_stmt != NULL) {
                insert_statement_full(else_stmt, insert_location,
                                      /*perform_post_pass=*/FALSE);
              }  /* if */
            } else {
              /* Insert an "if" statement. */
              if (then_stmt == NULL) {
                /* Create an empty statement. */
                then_stmt = alloc_statement(stmk_empty,
                                            /*compiler_generated=*/TRUE);
              }  /* if */
              new_statement = copy_inlined_statement(statement,
                                                     insert_location);
              new_statement->expr = stmt_expr;
              new_statement->variant.if_stmt.then_statement = then_stmt;
              new_statement->variant.if_stmt.else_statement = else_stmt;
              then_stmt->parent = new_statement;
              if (else_stmt != NULL) else_stmt->parent = new_statement;
            }  /* if */
          }  /* if */
        }  /* if */
        break;
      case stmk_block:
        if (is_expr_insert_location(insert_location)) {
          /* An expression will be created for the statements in the block.
             It will be unattached as it is built, then attached below. */
          set_expr_creation_insert_location(&sub_insert_location);
        } else {
          /* Make and insert a new block statement.  This doesn't use
             copy_statement because that doesn't clone the block
             supplement. */
          new_statement = alloc_statement(stmk_block,
                                          /*compiler_generated=*/TRUE);
          set_inline_statement_positions(new_statement, statement);
#if !STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION
          new_statement->variant.block.extra_info->final_position =
              statement->variant.block.extra_info->final_position;
#endif /* !STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION */
          insert_statement_full(new_statement, insert_location,
                                /*perform_post_pass=*/FALSE);
          /* Copies of the statements in the block will be inserted under the
             copy of the block statement. */
          set_block_start_insert_location(new_statement, &sub_insert_location);
        }  /* if */
        /* Copy the statements inside the block. */
        for (stmt = statement->variant.block.statements;
             stmt != NULL;
             stmt = stmt->next) {
          expand_statement_inline(stmt, &sub_insert_location,
                                  statement_count, is_too_large,
                                  inlinable, failed);
          if (*failed) break;
        }  /* for */
        if (!*failed && is_expr_insert_location(insert_location)) {
          /* Insert the expression for the whole block at the proper
             location. */
          expr = sub_insert_location.variant.expr;
          if (expr == NULL) {
            /* The block contained no statements, so use a (void)0 for the
               while block. */
            expr = add_cast(node_for_integer_constant(0L,
                                                      (an_integer_kind)ik_int),
                            void_type());
          }  /* if */
          insert_expr(expr, insert_location);
        }  /* if */
        break;
      case stmk_init:
        { a_dynamic_init_ptr dip = statement->variant.dynamic_init;
          an_expr_node_ptr   var_expr, init_expr;
          a_variable_ptr     var = remap_var_for_inlining(dip->variable);
          check_assertion(var != NULL);
          /* A dynamic initialization is rendered as an assignment.  This is
             done even in statement insert mode, to avoid the complexity
             of copying and inserting the stmk_init.  The only negative to
             that is that it doesn't allow inlining aggregate
             initializations in statement insert mode. */
          if (is_array_type(var->type)) {
            /* Arrays can't be handled. */
            goto cannot_inline_ever;
          } else if (dyn_init_is(dip, dik_constant)) {
            if (constant_is(dip->variant.constant.ptr, ck_aggregate)) {
              /* Aggregate constants cannot in general be assigned, but an
                 empty aggregate has no data to transfer. */
              if (dip->variant.constant.ptr
                     ->variant.aggregate.first_constant != NULL) {
                goto cannot_inline_ever;
              }  /* if */
              break;
            }  /* if */
            /* Non-aggregate constant initial value. */
            /* This uses copy_constant_full because that routine does
               variable remapping if necessary. */
            init_expr = alloc_node_for_constant(
                       copy_constant_full(dip->variant.constant.ptr,
                                          (a_constant *)NULL,
                                          CE_DOING_INLINING_OF_FUNCTION_CALL));
          } else if (dyn_init_is(dip, dik_expression)) {
            init_expr = copy_expr_tree_for_inlining(dip->variant.expression,
                                                    failed);
          } else {
            /* Other cases cannot be handled (they probably can't happen here,
               but for the sake of safety...). */
            goto cannot_inline_ever;
          }  /* if */
          /* Assign the initial value to the variable. */
          var_expr = var_lvalue_expr(var);
          var_expr->next = init_expr;
          expr = make_operator_node((an_expr_operator_kind)eok_assign,
                                    f_skip_typerefs(var->type),
                                    var_expr);
          stmt = insert_expr_statement(expr, insert_location);
          set_inline_statement_positions(stmt, statement);
          var->initialization_rewritten_as_assignment = TRUE;
        }
        break;
      case stmk_while:
      case stmk_end_test_while:
        if (is_expr_insert_location(insert_location)) {
          /* Cannot inline this case in an expression context. */
          goto cannot_inline;
        }  /* if */
        /* Copy the dependent statement. */
        set_statement_creation_insert_location(&sub_insert_location);
        expand_statement_inline(statement->variant.loop_statement,
                                &sub_insert_location, statement_count,
                                is_too_large, inlinable, failed);
        if (*failed) break;
        stmt = sub_insert_location.variant.statement.stmt;
        /* Copy the "while" statement. */
        new_statement = copy_inlined_statement(statement, insert_location);
        new_statement->expr = stmt_expr;
        new_statement->variant.loop_statement = stmt;
        stmt->parent = new_statement;
        break;
      case stmk_for:
        { a_statement_ptr  init_stmt;
          an_expr_node_ptr increment_expr;
          if (is_expr_insert_location(insert_location)) {
            /* Cannot inline this case in an expression context. */
            goto cannot_inline;
          }  /* if */
          /* Copy the initialization statement. */
          set_statement_creation_insert_location(&sub_insert_location);
          expand_statement_inline(statement->variant.for_loop.extra_info->
                                                                initialization,
                                  &sub_insert_location, statement_count,
                                  is_too_large, inlinable, failed);
          if (*failed) break;
          init_stmt = sub_insert_location.variant.statement.stmt;
          /* Copy the dependent statement. */
          set_statement_creation_insert_location(&sub_insert_location);
          expand_statement_inline(statement->variant.for_loop.statement,
                                  &sub_insert_location, statement_count,
                                  is_too_large, inlinable, failed);
          if (*failed) break;
          stmt = sub_insert_location.variant.statement.stmt;
          /* Copy the increment expression. */
          increment_expr = statement->variant.for_loop.extra_info->increment;
          if (increment_expr != NULL) {
            increment_expr = copy_expr_tree_for_inlining(increment_expr,
                                                         failed);
            set_expr_result_not_used(increment_expr);
          }  /* if */
          /* Copy the "for" statement.  This does not use
             copy_inlined_statement because it needs to copy the for loop
             supplement. */
          new_statement = alloc_statement(stmk_for,
                                          /*compiler_generated=*/TRUE);
          set_inline_statement_positions(new_statement, statement);
          new_statement->expr = stmt_expr;
          new_statement->variant.for_loop.statement = stmt;
          stmt->parent = new_statement;
          new_statement->variant.for_loop.extra_info->initialization=init_stmt;
          if (init_stmt != NULL) init_stmt->parent = new_statement;
          new_statement->variant.for_loop.extra_info->increment=increment_expr;
          insert_statement_full(new_statement, insert_location,
                                /*perform_post_pass=*/FALSE);
        }
        break;
      case stmk_decl:
        /* Statement that marks the location of declarations.  Ignored here. */
        break;
      case stmk_asm:
      case stmk_switch_case:
      case stmk_switch:
      case stmk_try_block:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case stmk_microsoft_try:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case stmk_set_vla_size:
      case stmk_vla_decl:
#if GNU_EXTENSIONS_ALLOWED
      case stmk_assigned_goto:
#endif /* GNU_EXTENSIONS_ALLOWED */
      default:
cannot_inline_ever:
        /* This statement cannot be inlined in any context. */
        *inlinable = FALSE;
cannot_inline:
        /* This statement cannot be inlined in this case. */
        *failed = TRUE;
        break;
    }  /* switch */
  }  /* if */
}  /* expand_statement_inline */


static void issue_inlining_failure_diagnostic(a_routine_ptr routine,
                                              a_boolean     is_too_large)
/*
Issue a diagnostic about a failure to inline the indicated routine.
*/
{
  a_symbol_ptr sym = (a_symbol_ptr)routine->source_corresp.assoc_info;

  if (sym != NULL) {
    if (!routine->inlinable) {
      /* The routine cannot ever be inlined. */
      pos_sy_remark(is_too_large ? ec_too_large_to_inline :
                                   ec_cannot_inline,
                    &sym->decl_position, sym);
    } else {
      /* The routine cannot be inlined in this case. */
      sym_remark(ec_cannot_inline_call, sym);
    }  /* if */
  }  /* if */
}  /* issue_inlining_failure_diagnostic */


static a_boolean routine_type_is_temporarily_altered(a_routine_ptr routine)
/*
Return TRUE if the indicated routine's type is temporarily altered by
a block extern declaration.
*/
{
  a_boolean                altered = FALSE;
  an_extern_type_fixup_ptr etfp;
  a_scope_depth            scope_depth;

  if (in_front_end) {
    for (scope_depth = depth_scope_stack;
         scope_depth != NO_SCOPE_DEPTH;
         scope_depth--) {
      for (etfp = scope_stack[scope_depth].extern_type_fixup_list;
           etfp != NULL;
           etfp = etfp->next) {
        if (etfp->is_routine &&
            etfp->variant.routine == routine) {
          altered = TRUE;
          goto end_of_routine;
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* if */
end_of_routine:
  return altered;
}  /* routine_type_is_temporarily_altered */


void do_inlining_of_call(an_expr_node_ptr expr,
                         a_statement_ptr  statement,
                         a_boolean        *expr_has_been_detached)
/*
expr is a lowered call expression.  If the routine called is an inline
function, do inlining on the expression.  If statement is non-NULL, the call
is the top node of the indicated statement (which is an expression statement),
and the call (if inlinable) will replace this statement with a statement
representing the inlined code.  If expr_has_been_detached is non-NULL,
*expr_has_been_detached will be set to TRUE if expr has been effectively
detached from the IL (and should therefore no longer be used), FALSE otherwise.
*/
{
  an_expr_node_ptr      arg;
  a_routine_ptr         routine;
  a_statement_ptr       block_stmt;
  a_host_large_unsigned statement_count = 0;
  a_boolean             is_too_large = FALSE;

  db_enter(4, "do_inlining_of_call");
  /* Note that other kinds of calls (like eok_dot_member_call) have been
     lowered to eok_call already. */
  check_assertion(is_operation_node(expr) &&
                  node_operator_is(expr, eok_call));
  if (expr_has_been_detached != NULL) *expr_has_been_detached = FALSE;
  arg = expr->variant.operation.operands;
  routine = routine_from_function_expr(arg);
  if (routine != NULL) {
    /* We know which routine is being called. */
    if (!routine->is_inline) {
      /* Make sure that if the routine gets marked as inline later an
         out-of-line copy is generated to satisfy this call. */
      routine->need_out_of_line_copy = TRUE;
    } else if (C_mode() && routine_type_is_temporarily_altered(routine)) {
      /* If the routine is visible through a block extern declaration that
         doesn't match the real routine type (e.g., one that is
         unprototyped, where the real routine has a prototype), do not
         attempt inlining. */
      routine->need_out_of_line_copy = TRUE;
    } else {
      if (!routine->inlinable) {
        /* The routine cannot be inlined, so leave this call alone.  An
           out-of-line copy of the routine will be required.  Note that this
           comes up in particular for calls of inline functions prior
           to their definitions. */
        routine->need_out_of_line_copy = TRUE;
      } else {
        a_boolean          failed = FALSE, inlinable = TRUE;
        an_insert_location insert_location;
        a_scope_ptr        scope;
        /* The routine can be inlined.  Turn off the inlinable flag for the
           duration of the inlining to avoid problems with recursion. */
        routine->inlinable = FALSE;
#if DEBUG
        if (debug_level >= 4) {
          fprintf(f_debug, "Beginning inlining of call to ");
          db_name(&routine->source_corresp);
          fprintf(f_debug, ":\n");
        }  /* if */
#endif /* DEBUG */
        scope = scope_for_routine(routine);
        routine_scope_being_inlined = scope;
        /* Set the insert location.  Use a location unattached to the IL
           tree, because we may discover we can't inline the function.
           If the inlining works, we can insert the statement or expression
           into the tree.  If it doesn't, we discard the statement or
           expression and mark the routine so we will not attempt inlining
           in the future. */
        if (statement != NULL) {
          /* Build the code inside an extra block (even though the top
             statement of the function will be a block) because assignments
             to initialize parameter temporaries may be inserted. */
          block_stmt = alloc_statement(stmk_block,/*compiler_generated=*/TRUE);
          set_block_start_insert_location(block_stmt, &insert_location);
        } else {
          block_stmt = NULL;
          set_expr_creation_insert_location(&insert_location);
        }  /* if */
        check_assertion_str(variable_remappings_for_inlining == NULL,
                           "do_inlining_of_call: remappings list is non-NULL");
        /* Create new variables for parameters and local variables. */
        arg = arg->next;  /* Advance to first argument. */
        set_up_variable_remapping_for_inlining(
                                    scope,
                                    expr->variant.operation.eval_right_to_left,
                                    arg);
        /* Copy the code of the function, replacing references to the
           parameters and variables. */
        expand_statement_inline(scope->assoc_block, &insert_location,
                                &statement_count, &is_too_large,
                                &inlinable, &failed);
        if (failed) {
          /* Inlining failed, so we will need an out-of-line copy of the
             routine.  The statement or expression created above is just
             discarded. */
          routine->need_out_of_line_copy = TRUE;
        } else {
          /* Inlining was successful. */
          /* Now that inlining is known to have succeeded, add the temporary
             variables to the current scope. */
          finish_variable_remapping_for_inlining(block_stmt, &insert_location);
          if (statement != NULL) {
            /* Eliminate any extra unnecessary blocks that are present.
               This happens if no parameter assignments were generated. */
            a_statement_ptr inner_stmt;
            for (;;) {
              inner_stmt = block_stmt->variant.block.statements;
              if (inner_stmt != NULL &&
                  inner_stmt->kind == (a_statement_kind)stmk_block &&
                  inner_stmt->next == NULL &&
                  block_stmt->variant.block.extra_info->assoc_scope == NULL) {
                block_stmt = inner_stmt;
              } else {
                break;
              }  /* if */
            }  /* for */
            /* Replace the original call statement by overwriting it with
               the block statement containing the inlined code.  But keep
               the original statement source position. */
            { a_source_position  saved_position;
              a_statement_ptr    saved_parent = statement->parent;
#if EXTRA_SOURCE_POSITIONS_IN_IL
              a_source_position  saved_end_position;
              saved_end_position = statement->end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
              saved_position = statement->position;
              copy_statement(block_stmt, statement);
              statement->parent = saved_parent;
              statement->position = saved_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
              statement->end_position = saved_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
              statement->variant.block.extra_info->final_position =
                                                                saved_position;
            }
            /* The original expression has been detached from the IL tree and
               should no longer be used by the caller. */
            check_assertion(expr_has_been_detached != NULL);
            *expr_has_been_detached = TRUE;
          } else {
            an_expr_node_ptr inlined_call_expr = insert_location.variant.expr;
            check_assertion(inlined_call_expr != NULL);
            /* Make sure the expression has the type expected for the call. */
            if (is_void_type(expr->type)) {
              /* For void functions, make sure the type of the expression is
                 void (it's currently whatever type the last
                 statement/expression added has.) */
              inlined_call_expr = add_cast_if_necessary(inlined_call_expr,
                                                        expr->type);
            } else {
              /* For non-void functions, the type of the expression can be
                 wrong if the end of the function is unreachable (e.g.,
                 because it ends with a throw).  In that case, add a zero
                 cast to the right type. */
              if (!il_identical_types(expr->type, inlined_call_expr->type)) {
                a_constant_ptr   zero_constant = local_constant();
                a_type_ptr       needed_type = expr->type;
                an_expr_node_ptr zero_node;
                a_boolean        class_case =
                                       is_class_struct_union_type(needed_type);
                check_assertion_str(!scope->assoc_block->variant.block.
                                            extra_info->end_of_block_reachable,
                                 "do_inlining_of_call: incorrect result type");
                if (class_case) {
                  /* For a class case, make a null pointer to the type and
                     indirect through it. */
                  needed_type = make_pointer_type(needed_type);
                }  /* if */
                make_zero_of_proper_type(needed_type, zero_constant);
                zero_node = alloc_node_for_constant(zero_constant);
                if (class_case) zero_node = add_indirection_to_node(zero_node);
                insert_expr(zero_node, &insert_location);
                inlined_call_expr = insert_location.variant.expr;
                release_local_constant(&zero_constant);
              }  /* if */
            }  /* if */
            /* Replace the original call node by overwriting it with the
               expression for the inlined call. */
            overwrite_node(expr, inlined_call_expr);
          }  /* if */
        }  /* if */
        /* Free the remapping entries. */
        free_variable_remappings_for_inlining();
        /* Put the inlinable flag back on, unless we've discovered that this
           function can never be inlined. */
        routine->inlinable = inlinable;
        if (failed) issue_inlining_failure_diagnostic(routine, is_too_large);
        routine_scope_being_inlined = NULL;
#if DEBUG
        if (debug_level >= 4) {
          fprintf(f_debug, "End of inlining of call to ");
          db_name(&routine->source_corresp);
          fprintf(f_debug, "%s\n", failed ? " (failed)" : "");
        }  /* if */
#endif /* DEBUG */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* do_inlining_of_call */


void set_up_routine_for_inlining(a_scope_ptr scope)
/*
The routine associated with the indicated scope is an inline routine and
inlining is enabled.  The body of the routine has been lowered.  Set up
the routine so it can be inlined on calls from here on.
*/
{
  a_routine_ptr routine = scope->variant.routine.ptr;
  a_type_ptr    routine_type = skip_typerefs(routine->type);
  a_routine_type_supplement_ptr
                rtsp = routine_type->variant.routine.extra_info;

  /* GNU- and Microsoft-mode routines with a noinline attribute have had their
     never_inline flag set to TRUE in the front end.  GNU-mode routines that
     have the is_weak flag set are inlined if is_inline is set (i.e., the
     setting of is_weak is ignored for this inliner).  Note that this
     implementation of inlining ignores the setting of always_inline. */
  check_assertion(routine->is_inline);
  /* Rule out certain cases up front. */
  if (scope->variables != NULL) {
    /* Function has local static variables. */
  } else if (scope->constants != NULL) {
    /* Function has local constants. */
  } else if (scope->types != NULL) {
    /* Function has local types. */
  } else if (scope->scopes != NULL) {
    /* Function has block scopes. */
  } else if (scope->pragmas != NULL) {
    /* Function has pragmas. */
  } else if (rtsp->has_ellipsis) {
    /* Function has a variable number of arguments. */
#if ASM_FUNCTION_ALLOWED
  } else if (routine->storage_class == (a_storage_class)sc_asm) {
    /* The function is an asm function. */
#endif /* ASM_FUNCTION_ALLOWED */
#if ASSIGNMENT_TO_THIS_ALLOWED
  } else if (routine->assignment_to_this_done) {
    /* An assignment to "this" was done.  This is ruled out because
       we want to be able to count on "this" being constant across the
       whole invocation. */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  } else if (routine->contains_statement_expression) {
    /* The routine contains a GNU statement expression, ({...}).
       copy_expr_tree would have to be enhanced to be able to copy
       the statement subtree and associated scopes for those if we
       wanted to be able to inline such things. */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  } else if (routine->never_inline) {
    /* An inline function can be marked with a "noinline" attribute. */
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (!rtsp->prototyped && rtsp->param_type_list != NULL) {
    /* Old-style definitions cannot be inlined (C99 inline).  They're
       okay if they have no parameters. */
#if !LOWER_VARIABLE_LENGTH_ARRAYS
  } else if (scope->vla_dimensions != NULL) {
    /* VLA dimensions would need to be copied from the inlined scope to the
       inlining scope, but this can lead to memory region problems as well
       as issues when the function is inlined multiple times, so just disallow
       inlining in this case.  When lowering VLAs, the VLA dimensions are
       discarded soon after calling this routine, so there's no need to
       check for that case. */
#endif /* !LOWER_VARIABLE_LENGTH_ARRAYS */
  } else {
    /* The routine looks like it can be inlined. */
    routine->inlinable = TRUE;
  }  /* if */
  if (!routine->inlinable) {
    issue_inlining_failure_diagnostic(routine, /*is_too_large=*/FALSE);
  }  /* if */
}  /* set_up_routine_for_inlining */


void mark_inlined_routines_as_unreferenced(void)
/*
For any inline routines for which all calls were expanded inline, mark the
routines as being unreferenced.  This avoids lots of copies of the static
versions of those routines.
*/
{
  a_routine_ptr routine;

  for (routine = il_header.primary_scope->routines;
       routine != NULL;
       routine = routine->next) {
    if (routine->is_inline && routine->inlinable) {
      /* If the routine's address was taken, an out of line copy is needed. */
      if (routine->address_taken) routine->need_out_of_line_copy = TRUE;
      if (routine->storage_class == (a_storage_class)sc_unspecified &&
          (instantiate_extern_inline || c99_mode || gcc_mode) &&
          !routine->suppress_inline_body) {
        /* For extern inline functions we need to put out an out-of-line
           copy when told to do so via the suppress_inline_body flag.
           The suppress_inline_body flag is meaningful when instantiating
           extern inlines and in C99 and GNU C modes. */
        routine->need_out_of_line_copy = TRUE;
      }  /* if */
      if (!routine->need_out_of_line_copy) {
        /* We don't need an out-of-line copy, so mark the routine as
           unreferenced because it's no longer needed. */
        routine->source_corresp.referenced = FALSE;
      }  /* if */
    }  /* if */
  }  /* for */
}  /* mark_inlined_routines_as_unreferenced */


void inline_one_time_init(void)
/*
Do one-time initialization of static variables declared in inline.c.
(Variables that need to be reinitialized with each new compilation
are handled in inline_init.)
*/
{
  /* Save variables from inline.h and inline.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_variable_remappings_for_inlining),
#if DEBUG
      pch_saved_var_array_elem(num_variable_remappings_for_inlining),
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* inline_one_time_init */


void inline_init(void)
/*
Initialize static variables related to this file that must be initialized
for each compilation.
*/
{
  /* Variables in inline.h: */
  avail_variable_remappings_for_inlining = NULL;
#if DEBUG
  num_variable_remappings_for_inlining = 0;
#endif /* DEBUG */
  /* Static variables in inline.c: */
  variable_remappings_for_inlining = NULL;
  routine_scope_being_inlined = NULL;
}  /* inline_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* MINIMAL_INLINING */
#endif /* DO_IL_LOWERING */

