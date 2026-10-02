/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

lower_c99.c -- Routines to transform C99 IL constructs into constructs
               available in classic ANSI/ISO C ("C89").  Some GNU C
               extensions are also lowered here.

*/

#include "basic_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Only include this code if it is needed: */
#if DO_IL_LOWERING

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in IL lowering. */
#include "lower_hdrs.h"
/* Additional header files. */
#include "exprutil.h"
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

a_boolean c99_il_lowering_needed(void)
/*
Return TRUE if the current mode or configuration requires the lowering of C
constructs (the name of this function is historical; lowering may be required
for non-C99 dialects and even for plain C89).
*/
{
  a_boolean  result;

  if (suppress_il_lowering || is_at_least_one_error()) {
    result = FALSE;
  } else if (c99_mode || gcc_mode || microsoft_mode ||
             compound_literals_allowed || vla_enabled || designators_allowed ||
             lowering_normalizes_boolean_controlling_expressions) {
    result = TRUE;
#if FIXED_POINT_ALLOWED
  } else if (fixed_point_enabled) {
    result = TRUE;
#endif /* FIXED_POINT_ALLOWED */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* c99_il_lowering_needed */


/* Forward declarations (needed because of mutual recursion situations). */
static void lower_c99_constant_list(a_constant_ptr constant_list);
static void lower_c99_expr_full(an_expr_node_ptr expr,
                                a_statement_ptr  statement);
#if LOWER_FIXED_POINT
static void lower_c99_fixed_point_constant(a_constant_ptr constant);
static void lower_c99_fixed_point_operation(an_expr_node_ptr expr);
#endif /* LOWER_FIXED_POINT */

void lower_vla_dimension_expression(a_vla_dimension_ptr  vdp)
/*
Lower the expression in a VLA dimension entry.
*/
{
  an_expr_node_ptr  expr = vdp->dimension_expr;

  if (expr != NULL) {
    a_context    context;
    a_scope_ptr  saved_innermost_function_scope = NULL;
    a_context    *saved_curr_context;
    a_scope_ptr  scope = NULL;
    if (vdp->in_prototype_scope) {
      /* The expression we're about to lower may generate temporaries, but
         those temporaries would be allocated in the function scope (and
         therefore not available for use in the prototype scope).  As a
         workaround, temporarily restore the file scope context to catch these
         temporaries.  (Note that VLAs can only appear in prototype scope in
         C modes.) */
      check_assertion(C_mode());
      saved_innermost_function_scope = innermost_function_scope;
      innermost_function_scope = NULL;
      scope = il_header.primary_scope;
    }  /* if */
    /* We're about to lower a VLA expression as a full expression, but we're
       likely already be in a full expression context (and full expressions
       can't be nested).  Save the existing context stack and push a new
       one with the same scope and lifetime (so temporaries don't
       inadvertently get reused in the outer full expression). */
    save_and_push_context(&context, scope, (an_object_lifetime_ptr)NULL,
                          &saved_curr_context);
    if (C_mode()) {
      lower_c99_full_expr(expr);
    } else {
      lower_full_expr(expr, (a_statement_ptr)NULL);
    }  /* if */
    restore_saved_context(saved_curr_context);
    if (vdp->in_prototype_scope) {
      innermost_function_scope = saved_innermost_function_scope;
    }  /* if */
#if MINIMAL_INLINING
    /* Catch constant nonpositive sizes introduced by inlining. */
    if (is_constant_node(expr)) {
      a_constant_ptr con = node_constant(expr);
      if (con->kind == (a_constant_repr_kind)ck_integer &&
          sign_of_integer_constant(con) <= 0) {
        pos_error(ec_array_size_must_be_positive, &vdp->position);
      }  /* if */
    }  /* if */
#endif /* MINIMAL_INLINING */
  }  /* if */
}  /* lower_vla_dimension_expression */

#if LOWER_VARIABLE_LENGTH_ARRAYS

/*
The front end can be configured to transform variable-length array IL
into plain C89 IL by setting the macro LOWER_VARIABLE_LENGTH_ARRAYS to TRUE.
The allocation and deallocation of VLA storage is handled by portable
routines in the run-time support library (these routines ultimately rely
on the standard malloc and free functions).

By way of example, this VLA code snippet:

  void f(int n, int m) {
    int v[n][m];
    n = v[1][m];
  }

generates this lowered C pseudo-code:

  void f(int n, int m) {
    auto long __T8592264;
    auto long __T8592528;
    auto int *v;
    ((__T8592264 = n) ,
     (__T8592528 = m)) ,
     (__T8592264 *= __T8592528);
    __vla_alloc((void *)(&v), (__T8592264 * 4L));
    n = (((int *)(&(v[(__T8592528 * 1)])))[m]);
    __vla_dealloc((void *)(&v));
  }

As can be seen in the example, VLA variables are lowered to a pointer to the
underlying type of the array (int in this case).  Storage for the VLA is
allocated at the beginning of the scope in which the VLA is defined by calling
the __vla_alloc run-time routine.  VLA storage is likewise deallocated by
calling __vla_dealloc when the variable goes out of scope.  VLAs are found only
in function and block scopes and are never static.

Temporary helper variables (e.g., __T8592264 and __T8592528 in the example
above) are created to hold the size of significant components of variable
dimensions.  The temporary helper variables are used to properly scale
operations involving a VLA operand.  See lower_vla_dimensions for a full
description.

In addition to rewriting operations for scaling purposes, operations involving
the lvalue of a VLA expression must be rewritten.  For example, the lvalue of
a VLA variable (really a pointer in the lowered code) is replaced by an
indirection through the VLA variable (see lower_vla_variable_lvalue).
Such operations are identified during the lowering of expressions.

VLA types are also lowered to non-VLA types.  A VLA type, 'array [EXPR] of T',
or 'array [EXPR] of array [EXPR] of T' are both lowered to 'T', and VLA
variable types are lowered from 'array [EXPR] of T' to 'pointer to T'.
Multi-dimensional VLA variable types are also lowered to 'pointer to T'.  This
VLA type lowering process consists of these three steps:

1)  During lowering, types that contain a VLA component are identified by calls
    to record_vla_component_types_for_lowering and queued (on the vla_types
    list) for later processing.  No changes to VLA types are made at this time
    (the type contains information about VLA dimensions).

2)  After expressions have been lowered in a scope, the type of each VLA
    variable and parameter in the scope is modified to be a pointer to its
    previous (still VLA) type.

3)  Lastly, in lower_vla_types, the entire list of VLA types is walked and each
    VLA type is replaced with the underlying element type.

There are "variably-modified" types, which are types that contain a VLA but
aren't just a VLA (e.g., a pointer to VLA).  Static variables can have
variably-modified types, even though they can't have VLA types.

The expressions in a VLA are evaluated at the point of declaration
(as indicated by an stmk_set_vla_size statement), either of a VLA variable or a
variable with a variably-modified type.  (Or a cast that uses a
variably-modified type.)

The front end produces a stmk_vla_decl statement at the point of declaration of
a variable or typedef with a variably modified type.  Lowering uses this as a
trigger to produce a call to the __vla_alloc run-time call (when the
stmk_vla_decl refers to a variable).  For the others, there's no allocation.

In C mode, the point of deallocation is indicated by an enk_vla_dealloc
expression node (see VLA_DEALLOCATIONS_IN_IL).  In C++ mode, the deallocation
is handled by the object lifetime mechanism (the deallocation is like
destruction of a variable on exit from its scope).

When LOWER_VARIABLE_LENGTH_ARRAYS is FALSE, the back end must be prepared to
handle variably modified types, as well as stmk_set_vla_size and stmk_vla_decl
statements, and enk_vla_dealloc nodes (when VLA_DEALLOCATIONS_IN_IL is TRUE).
*/

STATIC_THREAD a_type_list_entry_ptr
		vla_types;
			/* A list of all the VLA type entries.  The types are
			   collected as we traverse the IL for lowering.  The
			   actual lowering occurs later when we no longer need
			   the VLA expression information. */


static void record_vla_type_for_lowering(a_type_ptr  tp)
/*
The given type must be a VLA-based type.  Add it to the list of types to be
lowered later on.
*/
{
  a_type_list_entry_ptr  entry = alloc_type_list_entry();

  entry->type = tp;
  entry->next = vla_types;
  vla_types = entry;
}  /* record_vla_type_for_lowering */


static a_boolean ttt_record_vla_type_for_lowering(a_type_ptr  tp,
                                                  a_boolean   *end_traversal)
/*
If the given type is a VLA type, record it for later lowering.  This is a
routine meant to be used with traverse_type_tree.  It always returns FALSE
(so the whole type is traversed).  It takes advantage of a dedicated flag
in a_type entries to avoid visiting any type node more than once.
*/
{
  if (tp->visited_for_vla_lowering) {
    *end_traversal = TRUE;
  } else {
    tp->visited_for_vla_lowering = TRUE;
    if (tp->kind == (a_type_kind)tk_array && is_vla_type(tp)) {
      /* VLA types will be lowered to the underlying element type by
         lower_vla_types. */
      record_vla_type_for_lowering(tp);
    }  /* if */
  }  /* if */
  return FALSE;
}  /* ttt_record_vla_type_for_lowering */


void record_vla_component_types_for_lowering(a_type_ptr  tp)
/*
Go through the types underlying the given type and record any VLA components
for later lowering.  Stop at typedefs since variably modified typedefs should
have been treated separately.
*/
{
  /* Don't bother traversing the type if we know there are no VLAs. */
  if (il_header.vla_used) {
    a_type_tree_traversal_flag_set  tt_flags = TTT_STOP_AT_TYPEDEFS |
                                               TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_THIS_PARAM_TYPE |
                                               TTT_TEMPLATE_ARGS |
                                               TTT_EXCEPTION_SPECS;

    (void)traverse_type_tree(tp, ttt_record_vla_type_for_lowering, tt_flags);
  }  /* if */
}  /* record_vla_component_types_for_lowering */


void lower_vla_types(void)
/*
Lower all VLA types that were recorded by record_vla_type_for_lowering.
*/
{
  a_type_list_entry_ptr  entry;

  for (entry = vla_types; entry != NULL; entry = entry->next) {
    if (entry->type->kind == (a_type_kind)tk_array) {
      /* A VLA type is lowered to its underlying element type. */
      /* A do-nothing typeref is added here so we don't actually copy the
         underlying type, but rather refer to the original type. */
      a_type_ptr  type_ref = alloc_type((a_type_kind)tk_typeref);
      type_ref->variant.typeref.type = 
                                    underlying_array_element_type(entry->type);
      type_ref->variant.typeref.is_lowered_variably_modified_type = TRUE;
      *entry->type = *type_ref;
    } else if (entry->type->kind == (a_type_kind)tk_typeref) {
      /* A VLA typedef is now marked as no longer variably modified. */
      check_assertion(typeref_is_typedef(entry->type));
      entry->type->variant.typeref.has_variably_modified_type = FALSE;
      entry->type->variant.typeref.is_lowered_variably_modified_type = TRUE;
    }  /* if */
  }  /* for */
  free_list_of_type_list_entries(vla_types);
}  /* lower_vla_types */


void lower_vla_variable_types(a_variable_ptr variable_list)
/*
Lower the type of all VLA variables on variable_list by adding a pointer
to the current type.  The variable itself has already been lowered, and
the type will be further lowered (by lower_vla_types) where the VLA type will
be lowered to its underlying element type.  This step must be performed
after lowering of all executable code that uses the variable, but before
the VLA types themselves are lowered.
*/
{
  a_variable_ptr variable;

  for (variable = variable_list; variable != NULL; variable = variable->next) {
    if (is_vla_type(variable->type)) {
      variable->type = make_pointer_type(variable->type);
    }  /* if */
  }  /* for */
}  /* lower_vla_variable_types */


void lower_vla_variable_types_in_scope(a_scope_ptr scope)
/*
Lower the type of all VLA variables in the specified function or block scope.
*/
{
  a_scope_ptr sp;

  check_assertion(scope->kind == (a_scope_kind)sck_function || 
                  scope->kind == (a_scope_kind)sck_block);
  if (scope->kind == (a_scope_kind)sck_function) {
    lower_vla_variable_types(scope->variant.routine.parameters);
  }  /* if */
  lower_vla_variable_types(scope->nonstatic_variables);
  for (sp = scope->scopes; sp != NULL; sp = sp->next) {
    if (sp->kind == (a_scope_kind)sck_block) {
      lower_vla_variable_types_in_scope(sp);
    }  /* if */
  }  /* for */
}  /* lower_vla_variable_types_in_scope */


void prepare_to_lower_variably_modified_typedef(a_type_ptr  type)
/*
The given type must be a typeref representing a typedef.  Record any variably-
modified component types that may need to be lowered later on.
*/
{
  check_assertion(type_is_typedef(type));
  record_vla_component_types_for_lowering(type->variant.typeref.type);
  if (type->variant.typeref.has_variably_modified_type) {
    /* The "has_variably_modified_type" flag will be cleared when the
       underlying type is lowered. */
    record_vla_type_for_lowering(type);
  }  /* if */
}  /* prepare_to_lower_variably_modified_typedef */


static a_variable_ptr vla_dimension_variable(a_type_ptr        tp,
                                             an_expr_node_ptr  *inits,
                                             a_boolean         *new_var)
/*
Return a variable representing the total number of elements in the VLA type tp.
If there is no such variable yet, create one, assign to it the expression
recorded in the a_vla_dimension entry associated with tp, and add the
assignment to the given expression tree (which could be NULL initially).
*new_var is set to TRUE if a new variable was created and to FALSE otherwise.
*/
{
  a_vla_dimension_ptr  vla_dim = find_vla_dimension(tp);

  if (vla_dim->total_number_of_elements == NULL) {
    a_type_ptr        ptrdiff_type = integer_type(targ_ptrdiff_t_int_kind);
    an_expr_node_ptr  expr, assign_ops;
    /* Create a new temporary variable and assign to it the expression
       computing the array length. */
    lower_vla_dimension_expression(vla_dim);
    expr = vla_dim->dimension_expr;
    vla_dim->total_number_of_elements = make_lowered_temporary(ptrdiff_type);
    assign_ops = var_lvalue_expr(vla_dim->total_number_of_elements);
    assign_ops->next = add_cast_if_necessary(expr, ptrdiff_type);
    assign_ops = make_operator_node((an_expr_operator_kind)eok_assign,
                                    ptrdiff_type, assign_ops);
    if (*inits == NULL) {
      *inits = assign_ops;
    } else {
      *inits = make_comma_node(*inits, assign_ops);
    }  /* if */
    *new_var = TRUE;
  } else {
    *new_var = FALSE;
  }  /* if */
  return vla_dim->total_number_of_elements;
}  /* vla_dimension_variable */


an_expr_node_ptr lower_vla_dimensions(a_type_ptr  tp)
/*
Create helper variables for the significant VLA components of the given type
and set them to the appropriate values.  Specifically, each variable is to
hold the total number or elements of the associated VLA.  The generated
assignments are aggregated via comma operators and returned as a single
expression (NULL if no variables needed to be created).

For example, a declaration like
	int (*p)[n][2*n][3][4*n][5];
will cause the creation of three variables (say _D1, _D2, and _D3) initialized
as follows:
	_D1 = n,
	_D2 = 2*n,
	_D3 = 4*n
followed by the following updates to count the total number of elements at
each level:
	_D3 *= 5,
	_D2 *= 3*_D3,
	_D1 *= _D2  // _D1 now holds the total number of elements in *p
Arranging the computations in this way provides the quantity needed to compute
any storage to be allocated for a VLA variable, but the other computed
variables also make indexing into the VLA arrays more efficient.
*/
{
  an_expr_node_ptr  inits = NULL, accums = NULL;
  a_type_ptr        ptrdiff_type = integer_type(targ_ptrdiff_t_int_kind);

  /* There may be multiple multilevel VLA components in a type.  For example:
       int (*const (a[n][n]))[m][m];
     The outer loop deals with that possibility. */
  while (is_variably_modified_type(tp)) {
    a_variable_ptr  dim_var;
    a_boolean       dim_var_created;
    /* Skip over pointer and type qualifier components.  If we encounter a
       typedef we can stop since variably modified typedefs have their own
       stmk_vla_decl that would have caused this processing for the underlying
       type already.  For this reason we cannot easily call is_vla_type since
       it ignores typedefs.  (Note that we don't have to worry about pointer
       to member types since they are not allowed to be variably modified.) */
    for (;;) {
      if (tp->kind == (a_type_kind)tk_pointer) {
        tp = tp->variant.pointer.type;
      } else if (tp->kind == (a_type_kind)tk_routine) {
        tp = tp->variant.routine.return_type;
      } else if (tp->kind == (a_type_kind)tk_typeref) {
        if (typeref_is_typedef(tp)) {
          goto done;
        } else {
          tp = tp->variant.typeref.type;
        }  /* if */
      } else {
        /* Since tp was a variably modified type, the only remaining case is
           tk_array.  Top-level non-VLA array components can be skipped (e.g.,
           "int[2][3][n][m][6]" can be handled as "int[n][m][6]". */
        check_assertion_str(tp->kind == (a_type_kind)tk_array,
                            "lower_vla_dimensions: unexpected type kind");
        if (tp->variant.array.is_vla) {
          if (tp->variant.array.has_assoc_vla_dimension) {
            /* The normal case. */
            break;
          } else {
            /* A [*] dimension.  This dimension requires no work, but the
               underlying type may or may not contain more components to
               lower. */
            tp = tp->variant.array.element_type;
            goto skip_to_next_component;
          }  /* if */
        } else {
          tp = tp->variant.array.element_type;
        }  /* if */
      }  /* if */
    }  /* for */
    /* tp should now point to a VLA type: Create and initialize the variable
       associated with the top-level dimension. */
    dim_var = vla_dimension_variable(tp, &inits, &dim_var_created);
    tp = skip_typerefs(tp->variant.array.element_type);
    do {
      a_targ_size_t  constant_factor;
      if (!dim_var_created) {
        /* We ran into a VLA type that already has an updated variable.
           (Presumably as part of a typedef.) No more components need
           processing. */
        goto done;
      }  /* if */
      /* Look for additional dimension and create/update dimension variables
         accordingly. */
      constant_factor = 1;
      while (tp->kind == (a_type_kind)tk_array && !tp->variant.array.is_vla) {
        constant_factor *= tp->variant.array.variant.number_of_elements;
        tp = skip_typerefs(tp->variant.array.element_type);
      }  /* while */
      if (constant_factor == 1 && tp->kind != (a_type_kind)tk_array) {
        /* A bottom-level VLA (e.g., T[n]): No adjustment needed. */
        break;
      } else {
        an_expr_node_ptr  const_expr = NULL, var_expr = NULL, lhs, rhs, acc;
        lhs = var_lvalue_expr(dim_var);
        if (constant_factor != 1) {
          const_expr =
            node_for_host_large_integer((a_host_large_integer)constant_factor,
                                        targ_ptrdiff_t_int_kind);
        }  /* if */
        if (tp->kind == (a_type_kind)tk_array) {
          dim_var = vla_dimension_variable(tp, &inits, &dim_var_created);
          tp = skip_typerefs(tp->variant.array.element_type);
          var_expr = var_rvalue_expr(dim_var);
        }  /* if */
        if (var_expr != NULL && const_expr != NULL) {
          /* Multiply the constant and variable parts. */
          var_expr->next = const_expr;
          rhs = make_operator_node((an_expr_operator_kind)eok_multiply,
                                   ptrdiff_type, var_expr);
        } else if (var_expr != NULL) {
          rhs = var_expr;
        } else {
          rhs = const_expr;
        }  /* if */
        /* Now multiply the variable associated with vla_dim with the rhs
           value. */
        lhs->next = rhs;
        acc = make_operator_node((an_expr_operator_kind)eok_multiply_assign,
                                 ptrdiff_type, lhs);
        /* Insert the accumulation expression in the right location. */
        if (accums == NULL) {
          accums = acc;
        } else {
          accums = make_comma_node(acc, accums);
        }  /* if */
      }  /* if */
    }  while (tp->kind == (a_type_kind)tk_array);
skip_to_next_component:;
  }  /* while */
done:
  if (inits != NULL) {
    if (accums != NULL) {
      inits = make_comma_node(inits, accums);
    }  /* if */
  }  /* if */
  return inits;
}  /* lower_vla_dimensions */


void lower_set_vla_size(a_statement_ptr  stmt)
/*
Replace the stmk_set_vla_size statement by the computation of helper variables
holding the total number of elements at each level of the associated VLA type.
For multilevel VLA types, all the VLA components are handled with the first
stmk_set_vla_size statement and any other stmk_set_vla_size statement
associated with the same type is turned into a no-op.
*/
{
  a_vla_dimension_ptr  vla_dim = stmt->variant.vla_dimension;

  set_statement_kind(stmt, (a_statement_kind)stmk_expr);
  if (vla_dim->total_number_of_elements == NULL) {
    /* There is no associated variable yet: Compute all the necessary
       dimension quantities for the associated VLA type. */
    stmt->expr = lower_vla_dimensions(vla_dim->type);
    check_assertion(stmt->expr != NULL);
    /* The result of the expression is not used. */
    set_expr_result_not_used(stmt->expr);
  } else {
    /* Since there already is an associated variable, work for this entry was
       presumably done with a preceding stmk_set_vla_size entry.  Turn this
       statement into a no-op. */
    turn_statement_into_noop(stmt);
  }  /* if */
}  /* lower_set_vla_size */


static an_expr_node_ptr vla_size_expr(a_type_ptr  vla_type,
                                      a_boolean   byte_count)
/*
Return an expression describing the (nonconstant) size of the given VLA type.
If byte_count is TRUE, the expression should reflect the size as a number of
bytes; otherwise, the size should be the number of elements.  In both cases,
the expression type is ptrdiff_t.
*/
{
  an_expr_node_ptr     result;
  a_type_ptr           array_type = skip_typerefs(vla_type);
  a_type_ptr           ptrdiff_type = integer_type(targ_ptrdiff_t_int_kind);
  a_targ_size_t        constant_factor = 1;
  a_vla_dimension_ptr  vla_dim;

  check_assertion(is_array_type(array_type));
  /* Accumulate any constant dimensions first. */
  while (!array_type->variant.array.is_vla) {
    a_targ_size_t  length =
                         array_type->variant.array.variant.number_of_elements;
    constant_factor *= length;
    array_type = skip_typerefs(array_type->variant.array.element_type);
  }  /* while */
  if (byte_count) {
    /* Multiply the constant factor by the size of the underlying element
       type. */
    a_type_ptr  element_type = underlying_array_element_type(array_type);
    constant_factor *= skip_typerefs(element_type)->size;
  }  /* if */
  /* Retrieve the previously computed number of elements in the VLA. */
  vla_dim = find_vla_dimension(array_type);
  check_assertion(vla_dim->total_number_of_elements != NULL);
  result = var_rvalue_expr(vla_dim->total_number_of_elements);
  if (constant_factor != 1) {
    /* For an array_type like T[4][5][expr][7] constant_factor is 20 and
       result is an expression representing expr*7.  Multiply the
       two factors to obtain the total scaling. */
    result->next =
            node_for_host_large_integer((a_host_large_integer)constant_factor,
                                        targ_ptrdiff_t_int_kind);
    result = make_operator_node((an_expr_operator_kind)eok_multiply,
                                ptrdiff_type, result);
  }  /* if */
  return result;
}  /* vla_size_expr */


STATIC_THREAD a_routine_ptr
                vla_alloc_routine;


static an_expr_node_ptr make_vla_allocation_expr(a_variable_ptr  vla_var)
/*
Create an expression that calls the run-time support library to allocate
storage for the given VLA variable.  In C++ mode, also record a variable
indicating the number of elements in the VLA (needed by other parts of C++
VLA lowering).
*/
{
  an_expr_node_ptr  result = var_addr_expr(vla_var), size_expr;
  a_type_ptr        ptrdiff_type = integer_type(targ_ptrdiff_t_int_kind);

  if (C_mode()) {
    size_expr = vla_size_expr(vla_var->type, /*byte_count=*/TRUE);
  } else {
    /* In C++ mode, other aspects of VLA lowering expect to find the element
       count in a variable.  So we create and record that variable here (if
       necessary), and derive a size expression from it. */
    a_type_ptr        array_type = skip_typerefs(vla_var->type), element_type;
    a_targ_size_t     element_size;
    an_expr_node_ptr  count_init = NULL;
    check_assertion(array_type->kind == (a_type_kind)tk_array);
    if (array_type->variant.array.is_vla) {
      /* The top-level type component is a VLA (unlike, e.g. "int[3][n]").
         So the needed variable already exists: It's the dimensions variable
         associated with the top-level type component. */
      a_vla_dimension_ptr  vla_dim = find_vla_dimension(array_type);
      check_assertion(vla_dim != NULL);
      vla_var->vla_element_count_variable = vla_dim->total_number_of_elements;
    } else {
      /* An element count variable must be created and initialized. */
      an_expr_node_ptr  count_expr = vla_size_expr(vla_var->type,
                                                   /*byte_count=*/FALSE);
      vla_var->vla_element_count_variable =
                                          make_lowered_temporary(ptrdiff_type);
      count_init = var_lvalue_expr(vla_var->vla_element_count_variable);
      count_init->next = add_cast_if_necessary(count_expr, ptrdiff_type);
      count_init = make_operator_node((an_expr_operator_kind)eok_assign,
                                      ptrdiff_type, count_init);
    }  /* if */
    size_expr = var_rvalue_expr(vla_var->vla_element_count_variable);
    element_type = underlying_array_element_type(array_type);
    element_size = skip_typerefs(element_type)->size;
    if (element_size != 1) {
      /* The element count must be multiplied by the element size to obtain
         the allocation size. */
      size_expr->next =
               node_for_host_large_integer((a_host_large_integer)element_size,
                                           targ_ptrdiff_t_int_kind);
      size_expr = make_operator_node((an_expr_operator_kind)eok_multiply,
                                     ptrdiff_type, size_expr);
    }  /* if */
    if (count_init != NULL) {
      size_expr = make_comma_node(count_init, size_expr);
    }  /* if */
  }  /* if */
  size_expr = add_cast_if_necessary(size_expr, ptrdiff_type);
  result = add_cast_if_necessary(result, void_star_type());
  result->next = size_expr;
  result = make_prototyped_runtime_call("__vla_alloc", &vla_alloc_routine,
                                        void_type(), void_star_type(),
                                        ptrdiff_type, result);
  return result;
}  /* make_vla_allocation_expr */


void lower_vla_decl(a_statement_ptr  stmt)
/*
Lower the given stmk_vla_decl statement.  If this is a VLA variable, allocate
memory for it.
*/
{
  a_variable_ptr  vla_var;

  if (stmt->variant.vla.is_typedef_decl) {
    vla_var = NULL;
  } else {
    vla_var = stmt->variant.vla.variant.variable;
  }  /* if */
  set_statement_kind(stmt, (a_statement_kind)stmk_expr);
  /* If necessary, allocate storage for the variable. */
  if (vla_var != NULL && vla_var->is_vla) {
    stmt->expr = make_vla_allocation_expr(vla_var);
  }  /* if */
  if (stmt->expr == NULL) {
    /* This may happen for simple VLA typedefs.  Create a dummy expression. */
    stmt->expr = node_for_host_large_integer((a_host_large_integer)0,
                                             (an_integer_kind)ik_int);
  }  /* if */
  /* The result of the statement expression is not used. */
  set_expr_result_not_used(stmt->expr);
}  /* lower_vla_decl */


STATIC_THREAD a_routine_ptr
                vla_dealloc_routine;


void lower_vla_dealloc(an_expr_node_ptr  expr)
/*
Lower the given enk_vla_dealloc expression.  Currently we call the run-time
support routine __vla_dealloc in all cases, but this could potentially be
optimized to only call that routine for the first allocated variable in the
scope (which would automatically deallocate all the other VLA variables as
well).
*/
{
  a_variable_ptr    vla_var = expr->variant.vla_variable;
  an_expr_node_ptr  arg = var_addr_expr(vla_var);

  arg = add_cast_if_necessary(arg, void_star_type());
  overwrite_node(expr,
                 make_prototyped_runtime_call("__vla_dealloc",
                                              &vla_dealloc_routine,
                                              void_type(), void_star_type(),
                                              (a_type_ptr)NULL, arg));
}  /* lower_vla_dealloc */


static void lower_vla_cast(an_expr_node_ptr  expr)
/*
The given expression node is a cast to a variably modified type (i.e.,
involving a VLA type).  Compute any needed dimension variables and record
VLA component types for a separate lowering pass.
*/
{
  a_type_ptr        tp = expr->type;
  an_expr_node_ptr  vla_inits = lower_vla_dimensions(tp);

  if (vla_inits != NULL) {
    expr->variant.operation.operands =
             make_comma_node(vla_inits, expr->variant.operation.operands);
  }  /* if */
  record_vla_component_types_for_lowering(expr->type);
}  /* lower_vla_cast */


void lower_vla_pointer_integer_arithmetic(an_expr_node_ptr  expr)
/*
The given expression node adds or subtracts an integer to or from a pointer
to a VLA.  Scale up the integer to compensate for the fact that the pointer
will be lowered to a pointer to the underlying element type.  For pre- and
post-increment operators, the operator needs to be changed since the amount
incremented or decremented will no longer be one.
*/
{
  a_type_ptr             array_type, new_type;
  a_type_ptr             ptrdiff_type = integer_type(targ_ptrdiff_t_int_kind);
  an_expr_node_ptr       offset, scale_factor, lval, lval_copy, result;
  an_expr_operator_kind  op = expr->variant.operation.kind;

  /* Scale the offset according to the (nonconstant) total number of elements
     in the underlying array type. */
  if (op == (an_expr_operator_kind)eok_subscript) {
    array_type = expr->type;
  } else {
    check_assertion(is_pointer_type(expr->type));
    array_type = type_pointed_to(expr->type);
  }  /* if */
  check_assertion(is_array_type(array_type));
  array_type = skip_typerefs(array_type);
  scale_factor = vla_size_expr(array_type, /*byte_count=*/FALSE);
  switch (op) {
    case eok_padd:
    case eok_subscript:
      /* Scale up the integer operand, but integer operand can be either
         first or second operand. */
      { an_expr_node_ptr *integer_op, save_next;
        if (expr->variant.operation.pointer_operand_is_second) {
          integer_op = &expr->variant.operation.operands;
        } else {
          integer_op = &expr->variant.operation.operands->next;
        }  /* if */
        save_next = (*integer_op)->next;
        (*integer_op)->next = NULL;
        offset = add_lowered_cast_if_necessary(*integer_op, ptrdiff_type);
        scale_factor->next = offset;
        *integer_op = make_operator_node((an_expr_operator_kind)eok_multiply,
                                         ptrdiff_type, scale_factor);
        (*integer_op)->next = save_next;
      }
      break;
    case eok_psubtract:
    case eok_padd_assign:
    case eok_psubtract_assign:
      /* Binary operators: Simply scale up the integer operand. */
      offset = expr->variant.operation.operands->next;
      offset = add_lowered_cast_if_necessary(offset, ptrdiff_type);
      scale_factor->next = offset;
      expr->variant.operation.operands->next =
                      make_operator_node((an_expr_operator_kind)eok_multiply,
                                         ptrdiff_type, scale_factor);
      break;
    case eok_pre_incr:
      /* Turn pre-increment into a += operator. */
      expr->variant.operation.kind = (an_expr_operator_kind)eok_padd_assign;
      expr->variant.operation.operands->next = scale_factor;
      break;
    case eok_pre_decr:
      /* Turn pre-decrement into a -= operator. */
      expr->variant.operation.kind =
                                  (an_expr_operator_kind)eok_psubtract_assign;
      expr->variant.operation.operands->next = scale_factor;
      break;
    case eok_post_incr:
    case eok_post_decr:
      /* expr++ is transformed also transformed into a += operator, but we
         need to save the original value to produce the result of the
         expression.  expr-- is entirely similar. */
      /* x@@ -> ((temp = x, x @= scale), temp) */
      op = (op == (an_expr_operator_kind)eok_post_incr) ?
                                   (an_expr_operator_kind)eok_padd_assign :
                                   (an_expr_operator_kind)eok_psubtract_assign;
      /* The original lvalue: */
      lval = expr->variant.operation.operands;
      /* Make a copy that we are going to use to increment/decrement the
         value pointed to: */
      lval_copy = make_lvalue_reusable_copy(lval, /*vars_can_change=*/TRUE);
      lval_copy->next = scale_factor;
      /* Save the original rvalue in a temporary that will be used to produce
         the result: */
      lval = rvalue_expr_for_lvalue(lval);
      result = assign_expr_to_temp_and_make_expr_for_reuse(lval);
      /* Adjust the type of the pointer operand to point to the underlying
         element type. */
      new_type = make_pointer_type(underlying_array_element_type(array_type));
      /* Assemble the three expressions as a replacement for the given node. */
      overwrite_node(
        expr,
        make_comma_node(
          make_comma_node(lval, make_operator_node(op, new_type, lval_copy)),
          result));
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
}  /* lower_vla_pointer_integer_arithmetic */


void lower_vla_pointer_difference(an_expr_node_ptr  expr)
/*
The given expression is a difference of pointers to VLAs.  The result must
be scaled down by the number of elements in the VLAs pointed to.
*/
{
  a_type_ptr        array_type =
                      type_pointed_to(expr->variant.operation.operands->type);
  a_type_ptr        ptrdiff_type = integer_type(targ_ptrdiff_t_int_kind);
  an_expr_node_ptr  scale_factor, copy;

  /* Scale the result according to the (nonconstant) total number of elements
     in the underlying array type. */
  array_type = skip_typerefs(array_type);
  scale_factor = vla_size_expr(array_type, /*byte_count=*/FALSE);
  copy = copy_node(expr);
  copy->next = scale_factor;
  overwrite_node(expr, make_operator_node((an_expr_operator_kind)eok_divide,
                                          ptrdiff_type, copy));
}  /* lower_vla_pointer_difference */ 


void lower_vla_variable_lvalue(an_expr_node_ptr  expr)
/*
The given expr is an lvalue enk_variable for a VLA variable.  VLA variable
lvalues need to be rewritten because the VLA variable gets changed to be a
pointer to the array instead of the array itself.  Convert an lvalue for VLA
variable v, whose type is array [] of T, to *(T *)v.  v's type (once fully
lowered) will be 'pointer to the underlying array element type'.  Note that
although the expression is a variable node expression on input, it won't be on
output.
*/
{
  an_expr_node_ptr  new_expr;

  check_assertion(is_variable_node(expr) && expr->is_lvalue);
  expr->type = make_pointer_type(expr->type);
  new_expr = rvalue_expr_for_lvalue(expr);
  new_expr = add_cast(copy_node(new_expr), expr->type);
  overwrite_node(expr, add_indirection_to_node(new_expr));
  /* If the underlying element type is cv-qualified, a new unqualified
     VLA type might have been created (during the lvalue to rvalue
     conversion above), so record it. */
  record_vla_component_types_for_lowering(expr->type);
}  /* lower_vla_variable_lvalue */


static void lower_vla_array_to_pointer_decay(an_expr_node_ptr expr)
/*
Lower an array-to-pointer decay operation (expr) that sits on top of
an expression with VLA type.  The operands of expr have not been lowered yet.
*/
{
  an_expr_node_ptr  operand = expr->variant.operation.operands;

  check_assertion(is_operation_node(expr) &&
                  node_operator_is(expr, eok_array_to_pointer) &&
                  is_vla_type(operand->type) &&
                  !expr->is_lvalue &&
                  operand->is_lvalue);
  if (is_variable_node(operand)) {
    /* Change the lvalue reference to an rvalue pointer.  VLA variables
       are lowered from 'array [] of T' to 'pointer to T'.  Expressions
       (like this one) that refer to VLA variables must also have the
       correct type.  This step changes the operand type from 
       'array [] of T' to 'pointer to array [] of T'.  Once lower_vla_types
       is called, this expression's type will match that of the lowered VLA
       variable ('pointer to T').  The overall type of the expression remains
       the same as the array decay is replaced with a cast of the same type
       below. */
    a_type_ptr  new_operand_type = make_pointer_type(operand->type);
    operand = rvalue_expr_for_lvalue(operand);
    operand->type = new_operand_type;
  } else {
    /* Lower the eok_array_to_pointer by changing it to a cast over an &. */
    operand = add_address_of_to_node(operand);
  }  /* if */
  /* Change the operation to a cast of the same type.  This cast will
     later be changed to a cast to pointer to the underlying array
     element type. */
  change_to_cast(expr, operand, expr->type);
}  /* lower_vla_array_to_pointer_decay */


void lower_vla_operations_before_operands_are_lowered(an_expr_node_ptr expr)
/*
Perform any VLA lowering operations on the operation specified by expr
that need to be performed before the operands of expr are lowered.
expr is an operation whose first operand is of VLA type.
*/
{
  an_expr_operator_kind  op = expr->variant.operation.kind;

  check_assertion(is_operation_node(expr) &&
                  is_vla_type(expr->variant.operation.operands->type));
  if (op == (an_expr_operator_kind)eok_array_to_pointer) {
    /* Lower an eok_array_to_pointer operation being applied to a VLA
       expression. */
    lower_vla_array_to_pointer_decay(expr);
  }  /* if */
}  /* lower_vla_operations_before_operands_are_lowered */

#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */

void create_dimension_variable(a_statement_ptr  stmt)
/*
stmt is a stmk_set_vla_size statement.  Create a new variable initialized with
the dimension expression associated with this statement.  The expression is
updated to include the initialization of the variable.  This routine is only
used by configurations that do not lower VLAs.  It is particularly useful for
the C-generating back end to avoid duplicating side-effects of VLA bounds if
the type of a VLA variable appears multiple times in the lowered IL.
*/
{
  a_vla_dimension_ptr  vla_dim;

  check_assertion(stmt->kind == (a_statement_kind)stmk_set_vla_size);
  vla_dim = stmt->variant.vla_dimension;
  lower_vla_dimension_expression(vla_dim);
#if NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES
  if (vla_dim->in_prototype_scope) {
    /* VLA dimension variables should not be created in function prototype
       scopes (probably because we are using the C-generating back end, which
       cannot validly declare such a variable). */
  } else
#endif /* NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES */
  /* Do not insert code here. */
  {
    vla_dim->dimension_variable = assign_expr_to_temp(vla_dim->dimension_expr);
  }  /* if */
}  /* create_dimension_variable */

 
void create_element_count_variable_for_vla(a_statement_ptr  stmt)
/*
stmt is a stmk_vla_decl statement.  Create a variable holding the total
number of elements in the associated VLA variable (if any).  Also create
any IL necessary to initialize this new variable.  This routine is only
used by configurations that do not lower VLAs (when VLAs are lowered, an
extended version of this work is done by make_vla_allocation_expr and
lower_vla_dimensions).
*/
{
  check_assertion(stmt->kind == (a_statement_kind)stmk_vla_decl);
  if (!stmt->variant.vla.is_typedef_decl &&
      stmt->variant.vla.variant.variable->is_vla) {
    /* There is indeed an associated variable. */
    a_type_ptr        ptrdiff_type = integer_type(targ_ptrdiff_t_int_kind);
    a_variable_ptr    var = stmt->variant.vla.variant.variable;
    a_type_ptr        type = skip_typerefs(var->type);
    an_expr_node_ptr  count = NULL, count_init;
    a_targ_size_t     constant_factor = 1;
    a_statement_ptr   new_stmt;
    /* Create the temporary and record it. */
    var->vla_element_count_variable = make_lowered_temporary(ptrdiff_type);
    /* Multiply all the dimensions of a possibly multi-dimensional array.
       The variable-length dimensions are represented by the "count" expression
       and the constant dimensions are accumulated in "constant_factor". */
    do {
      if (type->variant.array.is_vla) {
        /* A variable-length dimension. */
        a_vla_dimension_ptr  dim = find_vla_dimension(type);
        an_expr_node_ptr     dim_expr;
        check_assertion(dim != NULL);
        if (dim->dimension_variable != NULL) {
          /* The dimension expression's value has already been stored in a
             variable.  Reuse that. */
          dim_expr = var_rvalue_expr(dim->dimension_variable);
        } else {
          dim_expr = make_reusable_copy(dim->dimension_expr,
                                        /*vars_can_change=*/TRUE);
        }  /* if */
        dim_expr = add_cast_if_necessary(dim_expr, ptrdiff_type);
        if (count == NULL) {
          count = dim_expr;
        } else {
          count->next = dim_expr;
          count = make_operator_node((an_expr_operator_kind)eok_multiply,
                                     ptrdiff_type, count);
        }  /* if */
      } else {
        /* A constant dimension. */
        constant_factor *= type->variant.array.variant.number_of_elements;
      }  /* if */
      type = skip_typerefs(type->variant.array.element_type);
    } while (type->kind == (a_type_kind)tk_array);
    check_assertion(count != NULL);
    if (constant_factor != 1) {
      /* This was a multi-dimensional array with a nontrivial constant
         dimension. */
      count->next = node_for_host_large_integer(
                                        (a_host_large_integer)constant_factor,
                                        targ_ptrdiff_t_int_kind);
      count = make_operator_node((an_expr_operator_kind)eok_multiply,
                                 ptrdiff_type, count);
    }  /* if */
    /* Assign the total number of elements to the variable we created. */
    count_init = var_lvalue_expr(var->vla_element_count_variable);
    count_init->next = count;
    count_init = make_operator_node((an_expr_operator_kind)eok_assign,
                                    ptrdiff_type, count_init);
    /* Create an expression statement to actually perform the computation.
       Insert it before the stmk_vla_decl statement.  We cannot use 
       turn_statement_into_block on stmk_vla_decl statements because that
       would change the lifetime of the associated VLA.  However, since this
       is only called for C++ IL, we know the stmk_vla_decl must be part of
       a block already (declarations cannot appear as the only dependent
       statement of an "if" statement, for example).  We also count on this
       being called only from lower_statement_list (via lower_statement),
       which has code necessary to avoid lowering a statement twice. */
    check_assertion(!C_mode());
    new_stmt = alloc_statement(stmk_vla_decl, /*compiler_generated=*/TRUE);
    copy_statement(stmt, new_stmt);
    new_stmt->next = stmt->next;
    stmt->next = new_stmt;
    stmt->kind = (a_statement_kind)stmk_expr;
    stmt->expr = count_init;
    set_expr_result_not_used(stmt->expr);
  }  /* if */
}  /* create_element_count_variable_for_vla */

#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */

an_expr_node_ptr vla_dimension_expr_for_type(a_type_ptr type)
/*
Returns an expression for the number of elements in the specified VLA type.
Note that this routine assumes that, if needed, the VLA dimension variable(s)
have already been created.
*/
{
  an_expr_node_ptr result = NULL;
#if LOWER_VARIABLE_LENGTH_ARRAYS
  an_expr_node_ptr vla_inits = NULL;
  a_boolean        new_var;
  result = var_rvalue_expr(vla_dimension_variable(type, &vla_inits, &new_var));
  check_assertion(vla_inits == NULL);
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
  a_vla_dimension_ptr  dim;
  an_expr_node_ptr     dim_expr;
  check_assertion(is_array_type(type));
  /* A VLA type could have non-VLA components (e.g., X[2][n]), so loop for
     each array to create an expression for the entire type. */
  do {
    type = skip_typerefs(type);
    if (type->variant.array.is_vla) {
      dim = find_vla_dimension(type);
      if (dim->dimension_variable != NULL) {
        /* The dimension expression's value has already been stored in a
           variable.  Reuse that. */
        dim_expr = var_rvalue_expr(dim->dimension_variable);
      } else {
        dim_expr = make_reusable_copy(dim->dimension_expr,
                                      /*vars_can_change=*/TRUE);
      }  /* if */
    } else {
      check_assertion(!type->variant.array.is_variable_size_array &&
                      !type->variant.array.is_template_dependent_size_array);
      dim_expr = node_for_host_large_integer(
          (a_host_large_integer)type->variant.array.variant.number_of_elements,
          ik_int);
    }  /* if */
    if (result == NULL) {
      result = dim_expr;
    } else {
      result->next = dim_expr;
      result = make_operator_node((an_expr_operator_kind)eok_multiply,
                                  dim_expr->type, result);
    }  /* if */
    type = array_element_type(type);
  } while (is_array_type(type));
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  return result;
}  /* vla_dimension_expr_for_type */


void lower_runtime_sizeof(an_expr_node_ptr expr)
/*
Do lowering for an enk_sizeof, which can appear for VLAs or when
SIZEOF_TYPE_IS_UNKNOWN is defined.  In the non-VLA case, the "lowering" is
really just lowering the subtree and leaving the enk_sizeof itself
in the IL.  If VLAs are lowered, the node is replaced by an expression
representing the number of bytes of the VLA type underlying the sizeof
expression.
*/
{
#if LOWER_VARIABLE_LENGTH_ARRAYS
  an_expr_node_ptr  byte_count, precomputation = NULL;
  a_type_ptr        vla_type;

  if (expr->variant.sizeof_info.is_type) {
    /* Something like "sizeof(X[2][n][m/2])".  Unlike uses of VLAs in
       declarations there is no stmk_set_vla_size for VLA types named in
       expressions.  So we may have to perform computations on the fly. */
    vla_type = expr->variant.sizeof_info.variant.type;
    if (!(vla_enabled && is_vla_type(vla_type))) {
      if (!C_mode()) {
        lower_os_type(vla_type);
      }  /* if */
      goto done;
    }  /* if */
    precomputation = lower_vla_dimensions(vla_type);
  } else {
    /* sizeof was applied to a VLA expression. */
    precomputation = expr->variant.sizeof_info.variant.expr;
    vla_type = precomputation->type;
    if (precomputation->is_lvalue) {
      precomputation = add_address_of_to_node(precomputation);
    }  /* if */
    /* Lower the argument expression, but be sure to have extracted the
       type first.  (The lowered type is no longer a VLA.) */
    lower_any_expr(precomputation);
    if (!(vla_enabled && is_vla_type(vla_type))) {
      goto done;
    }  /* if */
  }  /* if */
  byte_count = vla_size_expr(vla_type, /*byte_count=*/TRUE);
  byte_count = add_cast_if_necessary(byte_count,
                                     integer_type(targ_size_t_int_kind));
  if (precomputation != NULL) {
    byte_count = make_comma_node(precomputation, byte_count);
  }  /* if */
  overwrite_node(expr, byte_count);
done:;
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
  /* We're not lowering the sizeof operator, but we may have to
     lower the argument if that argument is an expression.  (expr->type
     was lowered by the caller.) */
  if (expr->variant.sizeof_info.is_type) {
    a_type_ptr  type = expr->variant.sizeof_info.variant.type;
    lower_vla_dimensions_in_type(type);
    if (!C_mode()) {
      lower_os_type(type);
    }  /* if */
  } else {
    lower_any_expr(expr->variant.sizeof_info.variant.expr);
  }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
}  /* lower_runtime_sizeof */


a_float_kind map_extended_float_kinds(a_float_kind fkind)
/*
Map certain extended floating-point kinds (i.e., ones that we have an
implementation for) to existing floating-point formats that use the same
floating-point format.
*/
{
  if (fkind == fk_std_float16) {
    fkind = fk_float16;
  } else if (fkind == fk_std_float32) {
    fkind = fk_float;
  } else if (fkind == fk_std_float64) {
    fkind = fk_double;
  } else if (fkind == fk_std_float128) {
    fkind = fk_float128;
  }  /* if */
  return fkind;
}  /* map_extended_float_kinds */

#if LOWER_COMPLEX || LOWER_FIXED_POINT

/*
Typedef for a list of library routine names that perform the same function
but on complex types with different underlying floating-point types.  Used
when calling select_name_from_float_kind.  The order of elements
corresponds to _Float16, __fp16, float, _Float32x, double, _Float64x, long
double, __float80, __float128, and __bf16/std::bfloat116 types (where
_Complex __fp16 is currently unsupported).  _Float32x is implemented as
double, and _Float64x is implemented as long double.
(NUM_COMPLEX_FLOAT_KINDS is set to fk_first_extended_type+1 to include
fk_std_bfloat16, which gcc allows to be combined with the
_Complex/__complex__ type specifiers.)
*/
#define NUM_COMPLEX_FLOAT_KINDS ((int)fk_first_extended_type+1)

typedef a_const_char *a_library_name_array[NUM_COMPLEX_FLOAT_KINDS];

static a_const_char* select_name_from_float_kind(
                                              a_float_kind               fkind,
                                              const a_library_name_array names)
/*
Return names[fkind] (a string naming the given floating-point precision).
*/
{
  a_const_char *result;

  fkind = map_extended_float_kinds(fkind);
  check_assertion((int)fkind < NUM_COMPLEX_FLOAT_KINDS);
  result = names[(int)fkind];
  check_assertion(result != NULL);
  return result;
}  /* select_name_from_float_kind */


static a_routine_ptr *select_routine_from_float_kind(
                                              a_routine_ptr *table,
                                              a_float_kind  fkind)
/*
Return a pointer to the slot in table for the runtime routine associated
with fkind.  Extended floating-point kinds are mapped to the format used
by the tables before the slot is selected.
*/
{
  fkind = map_extended_float_kinds(fkind);
  check_assertion((int)fkind < NUM_COMPLEX_FLOAT_KINDS);
  return &table[(int)fkind];
}  /* select_routine_from_float_kind */

#endif /* LOWER_COMPLEX || LOWER_FIXED_POINT */
#if LOWER_COMPLEX

/* Pointers to lowered versions of complex types, once allocated. */
STATIC_THREAD a_type_ptr
                lowered_complex_float16;
STATIC_THREAD a_type_ptr
                lowered_complex_bfloat16;
STATIC_THREAD a_type_ptr
                lowered_complex_float;
STATIC_THREAD a_type_ptr
                lowered_complex_double;
STATIC_THREAD a_type_ptr
                lowered_complex_long_double;
STATIC_THREAD a_type_ptr
                lowered_complex_float80;
STATIC_THREAD a_type_ptr
                lowered_complex_float128;


static a_type_ptr make_lowered_complex_type(a_float_kind  fkind,
                                            a_const_char  *name)
/*
Create a struct type with the given name to represent a complex type of the
given precision.  The struct contains a single field that is an array of two
floating point elements.
*/
{
  a_type_ptr   result = make_lowered_class_type((a_type_kind)tk_struct);
  a_type_ptr   array_type;
  a_field_ptr  last_field = NULL;

  result->source_corresp.name = alloc_il((sizeof_t)(strlen(name)+1));
  strcpy((char *)result->source_corresp.name, name);
  /* Create a type "array of two real values". */
  array_type = alloc_type((a_type_kind)tk_array);
  array_type->variant.array.variant.number_of_elements = 2;
  array_type->variant.array.element_type = float_type(fkind);
  set_type_size(array_type);
  /* Add the field. */
  make_lowered_field("_Vals", array_type, result, &last_field);
  finish_class_type(result);
  return result;
}  /* make_lowered_complex_type */


a_type_ptr lowered_complex_type(a_float_kind fkind)
/*
Return the structure used to represent a complex type of the kind fkind in
lowered IL.
*/
{
  a_type_ptr  result = NULL;

  fkind = map_extended_float_kinds(fkind);
  switch (fkind) {
    case fk_float16:
      if (lowered_complex_float16 == NULL) {
        lowered_complex_float16 = make_lowered_complex_type(
                                                    fkind, "_Complex_float16");
      }  /* if */
      result = lowered_complex_float16;
      break;
    case fk_std_bfloat16:
      if (lowered_complex_bfloat16 == NULL) {
        lowered_complex_bfloat16 = make_lowered_complex_type(
                                                   fkind, "_Complex_bfloat16");
      }  /* if */
      result = lowered_complex_bfloat16;
      break;
    case fk_std_float32:
    case fk_float:
      if (lowered_complex_float == NULL) {
        lowered_complex_float = make_lowered_complex_type(
                                                   fk_float, "_Complex_float");
      }  /* if */
      result = lowered_complex_float;
      break;
    case fk_float32x:
    case fk_std_float64:
    case fk_double:
      if (lowered_complex_double == NULL) {
        lowered_complex_double = make_lowered_complex_type(
                                                 fk_double, "_Complex_double");
      }  /* if */
      result = lowered_complex_double;
      break;
    case fk_float64x:
    case fk_long_double:
      if (lowered_complex_long_double == NULL) {
        lowered_complex_long_double = make_lowered_complex_type(
                                       fk_long_double, "_Complex_long_double");
      }  /* if */
      result = lowered_complex_long_double;
      break;
    case fk_float80:
      if (lowered_complex_float80 == NULL) {
        lowered_complex_float80 = make_lowered_complex_type(
                                                    fkind, "_Complex_float80");
      }  /* if */
      result = lowered_complex_float80;
      break;
    case fk_std_float128:
    case fk_float128:
      if (lowered_complex_float128 == NULL) {
        lowered_complex_float128 = make_lowered_complex_type(
                                             fk_float128, "_Complex_float128");
      }  /* if */
      result = lowered_complex_float128;
      break;
    default:
      unexpected_condition_str("lowered_complex_type: invalid float kind");
  }  /* switch */
  return result;
}  /* lowered_complex_type */


static a_field_ptr complex_vals_field(a_type_ptr ctype)
/*
ctype is a complex type, possibly lowered.  Return a pointer to the
single field in the struct for the lowered version of the type.
*/
{
  a_field_ptr field;

  ctype = skip_typerefs(ctype);
  if (ctype->kind == (a_type_kind)tk_complex) {
    /* Not lowered yet.  Substitute the proper lowered type. */
    ctype = lowered_complex_type(ctype->variant.float_kind);
  }  /* if */
  check_assertion(ctype->kind == (a_type_kind)tk_struct);
  field = ctype->variant.class_struct_union.field_list;
  check_assertion(field != NULL && field->next == NULL);
  return field;
}  /* complex_vals_field */


/* Complex arithmetic and comparison routines. */
STATIC_THREAD a_routine_ptr
                xnegate_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                xadd_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                xsubtract_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                xmultiply_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                xdivide_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                xeq_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                xne_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                rtoc_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                ctor_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                itoc_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                ctoi_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                cast_float16_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                cast_bfloat16_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                cast_float_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                cast_double_routine[NUM_COMPLEX_FLOAT_KINDS];
STATIC_THREAD a_routine_ptr
                cast_long_double_routine[NUM_COMPLEX_FLOAT_KINDS];
#if FLOAT80_ENABLING_POSSIBLE
STATIC_THREAD a_routine_ptr
                cast_float80_routine[NUM_COMPLEX_FLOAT_KINDS];
#endif /* FLOAT80_ENABLING_POSSIBLE */
#if FLOAT128_ENABLING_POSSIBLE
STATIC_THREAD a_routine_ptr
                cast_float128_routine[NUM_COMPLEX_FLOAT_KINDS];
#endif /* FLOAT128_ENABLING_POSSIBLE */


/* Imaginary to complex library routines. */
static constexpr a_library_name_array itoc_routine_name = {
                                          "__c99_ifloat16_to_cfloat16",
                                          NULL, /* fk_fp16 */
                                          "__c99_ifloat_to_cfloat",
                                          "__c99_idouble_to_cdouble",
                                          "__c99_idouble_to_cdouble",
                                          "__c99_ilong_double_to_clong_double",
                                          "__c99_ilong_double_to_clong_double",
                                          "__c99_ifloat80_to_cfloat80",
                                          "__c99_ifloat128_to_cfloat128",
                                          "__c99_ibfloat16_to_cbfloat16"};

/* Complex to imaginary library routines. */
static constexpr a_library_name_array ctoi_routine_name = {
                                          "__c99_cfloat16_to_ifloat16",
                                          NULL, /* fk_fp16 */
                                          "__c99_cfloat_to_ifloat",
                                          "__c99_cdouble_to_idouble",
                                          "__c99_cdouble_to_idouble",
                                          "__c99_clong_double_to_ilong_double",
                                          "__c99_clong_double_to_ilong_double",
                                          "__c99_cfloat80_to_ifloat80",
                                          "__c99_cfloat128_to_ifloat128",
                                          "__c99_cbfloat16_to_ibfloat16"};

/* Floating-point to complex library routines. */
static constexpr a_library_name_array rtoc_routine_name = {
                                          "__c99_float16_to_cfloat16",
                                          NULL, /* fk_fp16 */
                                          "__c99_float_to_cfloat",
                                          "__c99_double_to_cdouble",
                                          "__c99_double_to_cdouble",
                                          "__c99_long_double_to_clong_double",
                                          "__c99_long_double_to_clong_double",
                                          "__c99_float80_to_cfloat80",
                                          "__c99_float128_to_cfloat128",
                                          "__c99_bfloat16_to_cbfloat16"};

/* Complex to floating-point to library routines. */
static constexpr a_library_name_array ctor_routine_name = {
                                          "__c99_cfloat16_to_float16",
                                          NULL, /* fk_fp16 */
                                          "__c99_cfloat_to_float",
                                          "__c99_cdouble_to_double",
                                          "__c99_cdouble_to_double",
                                          "__c99_clong_double_to_long_double",
                                          "__c99_clong_double_to_long_double",
                                          "__c99_cfloat80_to_float80",
                                          "__c99_cfloat128_to_float128",
                                          "__c99_cbfloat16_to_bfloat16"};

/* Library routines for complex casts. */
static constexpr a_library_name_array cast_float16_routine_name = {
                                          NULL,
                                          NULL, /* fk_fp16 */
                                          "__c99_cfloat16_to_cfloat",
                                          "__c99_cfloat16_to_cdouble",
                                          "__c99_cfloat16_to_cdouble",
                                          "__c99_cfloat16_to_clong_double",
                                          "__c99_cfloat16_to_clong_double",
                                          "__c99_cfloat16_to_cfloat80",
                                          "__c99_cfloat16_to_cfloat128",
                                          "__c99_cfloat16_to_cbfloat16"};
static constexpr a_library_name_array cast_bfloat16_routine_name = {
                                          "__c99_cbfloat16_to_cfloat16",
                                          "__c99_cbfloat16_to_cfloat16",
                                          "__c99_cbfloat16_to_cfloat",
                                          "__c99_cbfloat16_to_cdouble",
                                          "__c99_cbfloat16_to_cdouble",
                                          "__c99_cbfloat16_to_clong_double",
                                          "__c99_cbfloat16_to_clong_double",
                                          "__c99_cbfloat16_to_cfloat80",
                                          "__c99_cbfloat16_to_cfloat128",
                                          NULL};
static constexpr a_library_name_array cast_float_routine_name = {
                                          "__c99_cfloat_to_cfloat16",
                                          NULL, /* fk_fp16 */
                                          NULL,
                                          "__c99_cfloat_to_cdouble",
                                          "__c99_cfloat_to_cdouble",
                                          "__c99_cfloat_to_clong_double",
                                          "__c99_cfloat_to_clong_double",
                                          "__c99_cfloat_to_cfloat80",
                                          "__c99_cfloat_to_cfloat128",
                                          "__c99_cfloat_to_cbfloat16"};

static constexpr a_library_name_array cast_double_routine_name = {
                                          "__c99_cdouble_to_cfloat16",
                                          NULL, /* fk_fp16 */
                                          "__c99_cdouble_to_cfloat",
                                          NULL,
                                          NULL,
                                          "__c99_cdouble_to_clong_double",
                                          "__c99_cdouble_to_clong_double",
                                          "__c99_cdouble_to_cfloat80",
                                          "__c99_cdouble_to_cfloat128",
                                          "__c99_cdouble_to_cbfloat16"};

static constexpr a_library_name_array cast_long_double_routine_name = {
                                          "__c99_clong_double_to_cfloat16",
                                          NULL, /* fk_fp16 */
                                          "__c99_clong_double_to_cfloat",
                                          "__c99_clong_double_to_cdouble",
                                          "__c99_clong_double_to_cdouble",
                                          NULL,
                                          NULL,
                                          "__c99_clong_double_to_cfloat80",
                                          "__c99_clong_double_to_cfloat128",
                                          "__c99_clong_double_to_cbfloat16"};

#if FLOAT80_ENABLING_POSSIBLE
static constexpr a_library_name_array cast_float80_routine_name = {
                                          "__c99_cfloat80_to_cfloat16",
                                          NULL, /* fk_fp16 */
                                          "__c99_cfloat80_to_cfloat",
                                          "__c99_cfloat80_to_cdouble",
                                          "__c99_cfloat80_to_cdouble",
                                          "__c99_cfloat80_to_clong_double",
                                          "__c99_cfloat80_to_clong_double",
                                          NULL,
                                          "__c99_cfloat80_to_cfloat128",
                                          "__c99_cfloat80_t0_cbfloat16"};
#endif /* FLOAT80_ENABLING_POSSIBLE */

#if FLOAT128_ENABLING_POSSIBLE
static constexpr a_library_name_array cast_float128_routine_name = {
                                          "__c99_cfloat128_to_cfloat16",
                                          NULL, /* fk_fp16 */
                                          "__c99_cfloat128_to_cfloat",
                                          "__c99_cfloat128_to_cdouble",
                                          "__c99_cfloat128_to_cdouble",
                                          "__c99_cfloat128_to_clong_double",
                                          "__c99_cfloat128_to_clong_double",
                                          "__c99_cfloat128_to_cfloat80",
                                          NULL,
                                          "__c99_cfloat128_to_cbfloat16"};
#endif /* FLOAT128_ENABLING_POSSIBLE */

/* Names of the complex negate runtime routines. */
static constexpr a_library_name_array xnegate_routine_name = {
                                          "__c99_complex_float16_negate",
                                          NULL, /* fk_fp16 */
                                          "__c99_complex_float_negate",
                                          "__c99_complex_double_negate",
                                          "__c99_complex_double_negate",
                                          "__c99_complex_long_double_negate",
                                          "__c99_complex_long_double_negate",
                                          "__c99_complex_float80_negate",
                                          "__c99_complex_float128_negate",
                                          "__c99_complex_bfloat16_negate"};


void lower_c99_xnegate(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("-z") into a function call (compatible
with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_float_kind      fkind;
  a_const_char      *rout_name;
  an_expr_node_ptr  xnegate_call;

  check_assertion(is_complex_type(return_type));
  fkind = return_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xnegate_routine_name);
  xnegate_call = make_prototyped_runtime_call(
                    rout_name,
                    select_routine_from_float_kind(xnegate_routine, fkind),
                    return_type, return_type, (a_type_ptr)NULL,
                    expr->variant.operation.operands);
  overwrite_node(expr, xnegate_call);
}  /* lower_c99_xnegate */


/* Names of the complex add runtime routines. */
static constexpr a_library_name_array xadd_routine_name = {
                                               "__c99_complex_float16_add",
                                               NULL, /* fk_fp16 */
                                               "__c99_complex_float_add",
                                               "__c99_complex_double_add",
                                               "__c99_complex_double_add",
                                               "__c99_complex_long_double_add",
                                               "__c99_complex_long_double_add",
                                               "__c99_complex_float80_add",
                                               "__c99_complex_float128_add",
                                               "__c99_complex_bfloat16_add"};


void lower_c99_xadd(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1+z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_float_kind      fkind;
  a_const_char      *rout_name;
  an_expr_node_ptr  xadd_call;

  check_assertion(is_complex_type(return_type));
  fkind = return_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xadd_routine_name);
  xadd_call = make_prototyped_runtime_call(
                    rout_name,
                    select_routine_from_float_kind(xadd_routine, fkind),
                    return_type, return_type, return_type,
                    expr->variant.operation.operands);
  overwrite_node(expr, xadd_call);
}  /* lower_c99_xadd */


/* Names of the complex subtract runtime routines. */
static constexpr a_library_name_array xsubtract_routine_name = {
                                         "__c99_complex_float16_subtract",
                                         NULL, /* fk_fp16 */
                                         "__c99_complex_float_subtract",
                                         "__c99_complex_double_subtract",
                                         "__c99_complex_double_subtract",
                                         "__c99_complex_long_double_subtract",
                                         "__c99_complex_long_double_subtract",
                                         "__c99_complex_float80_subtract",
                                         "__c99_complex_float128_subtract",
                                         "__c99_complex_bfloat16_subtract"};


void lower_c99_xsubtract(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1-z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_float_kind      fkind;
  a_const_char      *rout_name;
  an_expr_node_ptr  xsubtract_call;

  check_assertion(is_complex_type(return_type));
  fkind = return_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xsubtract_routine_name);
  xsubtract_call = make_prototyped_runtime_call(
                    rout_name,
                    select_routine_from_float_kind(xsubtract_routine, fkind),
                    return_type, return_type, return_type,
                    expr->variant.operation.operands);
  overwrite_node(expr, xsubtract_call);
}  /* lower_c99_xsubtract */


/* Names of the complex multiply runtime routines. */
static constexpr a_library_name_array xmultiply_routine_name = {
                                         "__c99_complex_float16_multiply",
                                         NULL, /* fk_fp16 */
                                         "__c99_complex_float_multiply",
                                         "__c99_complex_double_multiply",
                                         "__c99_complex_double_multiply",
                                         "__c99_complex_long_double_multiply",
                                         "__c99_complex_long_double_multiply",
                                         "__c99_complex_float80_multiply",
                                         "__c99_complex_float128_multiply",
                                         "__c99_complex_bfloat16_multiply"};


void lower_c99_xmultiply(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1*z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_float_kind      fkind;
  a_const_char      *rout_name;
  an_expr_node_ptr  xmultiply_call;

  check_assertion(is_complex_type(return_type));
  fkind = return_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xmultiply_routine_name);
  xmultiply_call = make_prototyped_runtime_call(
                    rout_name,
                    select_routine_from_float_kind(xmultiply_routine, fkind),
                    return_type, return_type, return_type,
                    expr->variant.operation.operands);
  overwrite_node(expr, xmultiply_call);
}  /* lower_c99_xmultiply */


/* Names of the complex divide runtime routines. */
static constexpr a_library_name_array xdivide_routine_name = {
                                           "__c99_complex_float16_divide",
                                           NULL, /* fk_fp16 */
                                           "__c99_complex_float_divide",
                                           "__c99_complex_double_divide",
                                           "__c99_complex_double_divide",
                                           "__c99_complex_long_double_divide",
                                           "__c99_complex_long_double_divide",
                                           "__c99_complex_float80_divide",
                                           "__c99_complex_float128_divide",
                                           "__c99_complex_bfloat16_divide"};


void lower_c99_xdivide(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1/z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_float_kind      fkind;
  a_const_char      *rout_name;
  an_expr_node_ptr  xdivide_call;

  check_assertion(is_complex_type(return_type));
  fkind = return_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xdivide_routine_name);
  xdivide_call = make_prototyped_runtime_call(
                    rout_name,
                    select_routine_from_float_kind(xdivide_routine, fkind),
                    return_type, return_type, return_type,
                    expr->variant.operation.operands);
  overwrite_node(expr, xdivide_call);
}  /* lower_c99_xdivide */


/* Names of the complex == runtime routines. */
static constexpr a_library_name_array xeq_routine_name = {
                                                "__c99_complex_float16_eq",
                                                NULL, /* fk_fp16 */
                                                "__c99_complex_float_eq",
                                                "__c99_complex_double_eq",
                                                "__c99_complex_double_eq",
                                                "__c99_complex_long_double_eq",
                                                "__c99_complex_long_double_eq",
                                                "__c99_complex_float80_eq",
                                                "__c99_complex_float128_eq",
                                                "__c99_complex_bfloat16_eq"};


void lower_c99_xeq(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1==z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_type_ptr        op_type = expr->variant.operation.operands->type;
  a_float_kind      fkind;
  a_const_char      *rout_name;
  an_expr_node_ptr  xeq_call;

  op_type = skip_typerefs(op_type);
  check_assertion(is_complex_type(op_type));
  fkind = op_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xeq_routine_name);
  xeq_call = make_prototyped_runtime_call(
                    rout_name,
                    select_routine_from_float_kind(xeq_routine, fkind),
                    return_type, op_type, op_type,
                    expr->variant.operation.operands);
  overwrite_node(expr, xeq_call);
}  /* lower_c99_xeq */


/* Names of the complex != runtime routines. */
static constexpr a_library_name_array xne_routine_name = {
                                                "__c99_complex_float16_ne",
                                                NULL, /* fk_fp16 */
                                                "__c99_complex_float_ne",
                                                "__c99_complex_double_ne",
                                                "__c99_complex_double_ne",
                                                "__c99_complex_long_double_ne",
                                                "__c99_complex_long_double_ne",
                                                "__c99_complex_float80_ne",
                                                "__c99_complex_float128_ne",
                                                "__c99_complex_bfloat16_ne"};


void lower_c99_xne(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1!=z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_type_ptr        op_type = expr->variant.operation.operands->type;
  a_float_kind      fkind;
  a_const_char      *rout_name;
  an_expr_node_ptr  xne_call;

  op_type = skip_typerefs(op_type);
  check_assertion(is_complex_type(op_type));
  fkind = op_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xne_routine_name);
  xne_call = make_prototyped_runtime_call(
                    rout_name,
                    select_routine_from_float_kind(xne_routine, fkind),
                    return_type, op_type, op_type,
                    expr->variant.operation.operands);
  overwrite_node(expr, xne_call);
}  /* lower_c99_xne */


void lower_c99_xincr_decr(an_expr_node_ptr expr)
/*
Lower a complex pre-/post- increment/decrement of a complex value (allowed only
in GNU modes).  Only the "real" portion of value is affected (i.e., when
incrementing/decrementing the value 1.0+0.0i -- of the appropriate type --
is used for the increment/decrement).
*/
{
  an_expr_operator_kind op = expr->variant.operation.kind;
  an_expr_node_ptr      op1 = expr->variant.operation.operands;
  an_expr_node_ptr      op1_for_argument, op1_for_assign, op_node;
  an_expr_node_ptr      op2_node = NULL;
  an_expr_node_ptr      con_node;
  a_variable_ptr        temp_var = NULL;
  a_boolean             is_post_op = FALSE, temp_init_used;
  a_type_ptr            return_type = skip_typerefs(expr->type);
  a_const_char          *rout_name;
  a_const_char* const   *routine_names = NULL;
  a_routine_ptr         *routines = NULL;
  a_float_kind          fkind;
  a_constant_ptr        con = local_constant();

  check_assertion(op1->is_lvalue && is_complex_type(return_type));
  switch (op) {
    case eok_post_incr:
      is_post_op = TRUE;
      routine_names = xadd_routine_name;
      routines = xadd_routine;
      break;
    case eok_post_decr:
      is_post_op = TRUE;
      routine_names = xsubtract_routine_name;
      routines = xsubtract_routine;
      break;
    case eok_pre_incr:
      is_post_op = FALSE;
      routine_names = xadd_routine_name;
      routines = xadd_routine;
      break;
    case eok_pre_decr:
      is_post_op = FALSE;
      routine_names = xsubtract_routine_name;
      routines = xsubtract_routine;
      break;
    default:
      unexpected_condition_str("lower_c99_xincr_decr: bad operator");
  }  /* switch */
  fkind = return_type->variant.float_kind;
  /* Create a complex constant 1.0+0.0i of the appropriate type. */
  set_complex_constant(fkind, "1.0", "0.0", con);
  con_node = alloc_node_for_constant(con);
  /* Mark the constant as un-lowered, then lower it.  This will replace the
     constant expression with a file scope static temporary that will be used
     in place of the constant. */
  mark_as_not_visited(node_constant(con_node));
  lower_c99_constant_expr(con_node);
  if (is_post_op && expr->result_is_not_used) {
    /* We don't need the more complicated post-incr/decr code if the
       result is not used. */
    is_post_op = FALSE;
  }  /* if */
  /* The normal rewrite of
       ++x
     is
       static_temp = {1.0, 0.0};
       ...
       x = __c99_complex_*_add(x, static_temp);
     Make a copy of op1 to be used as the argument of the call.
     op1 itself will be used as the left operand of the assignment. */
  op1_for_argument = make_lvalue_reusable_copy_full(op1,
                                                    /*vars_can_change=*/FALSE,
                                                    &temp_init_used);
  op1_for_assign = op1;
  if (temp_init_used || is_post_op) {
    /* op1 is complicated and was assigned to a temporary.  Make sure that
       the temporary is initialized before it is used by doing the
       overall rewrite of
         ++x;
       as
         static_temp = {1.0, 0.0};
         ...
         ((temp = *(t = &x)), *t = __c99_complex_*_add(temp, static_temp))
       We also use the temporary if the operation is a post-increment
       or -decrement, because we want to save and return the original value. */
    temp_var = make_local_temporary(return_type);
    op1_for_assign = op1_for_argument;
    op1_for_argument = var_lvalue_expr(temp_var);
    /* Make the (temp = *(t = &x)) assignment, to be inserted later. */
    op2_node = make_var_assignment_expr(temp_var, rvalue_expr_for_lvalue(op1));
  }  /* if */
  /* Make the arguments for the call. */
  op1_for_argument = rvalue_expr_for_lvalue(op1_for_argument);
  op1_for_argument->next = con_node;
  /* Select the proper routine based on whether we're incrementing or
     decrementing, as well as the type of complex expression we're
     computing. */
  rout_name = select_name_from_float_kind(fkind, routine_names);
  op_node = make_prototyped_runtime_call(
                    rout_name,
                    select_routine_from_float_kind(routines, fkind),
                    return_type, return_type, return_type,
                    op1_for_argument);
  /* Assign the result to op1 (or the temporary). */
  op_node = make_assignment_expr(
                   op1_for_assign, (an_expr_operator_kind)eok_assign, op_node);
  if (temp_var != NULL) {
    /* Combine the assignment to the temporary and the assignment that
       does the incr/decr call and stores it back in the original
       operand. */
    op_node = make_comma_node(op2_node, op_node);
  }  /* if */
  /* Here, op_node is "x = __c99_complex_*_add(x, static_temp)" or a fancier
     but equivalent expression if a temporary was used.  For a pre-operation,
     that's all we need. */
  if (is_post_op) {
    /* A post-increment or post-decrement.  Add a comma expression to
       return the value of the temporary, which is the original value
       of the operand. */
    check_assertion(temp_var != NULL);
    op_node = make_comma_node(op_node, var_rvalue_expr(temp_var));
  }  /* if */
  overwrite_node(expr, op_node);
  release_local_constant(&con);
}  /* lower_c99_xincr_decr */


static an_expr_node_ptr select_complex_vals(an_expr_node_ptr  expr)
/*
The given expression node represents a complex lvalue or rvalue.  Return a node
(constructed on top of the given one) for "<expr>.Vals" where "Vals" is the
single field of the lowered complex type.  The returned node is an rvalue
pointer (because the field Vals is an array, which decays to a pointer).
*/
{
  an_expr_node_ptr      result;
  a_field_ptr           vals_field;
  a_type_ptr            ctype = skip_typerefs(expr->type);

  check_assertion(is_complex_type(ctype));
  if (!expr->is_lvalue) {
    /* Assign the rvalue to a temporary and create a comma operation
       to return the address of the temporary (i.e., (temp = expr, &temp) ). */
    an_expr_node_ptr assign_node;
    a_variable_ptr   temp_var = make_local_temporary(ctype);
    assign_node = make_var_assignment_expr(temp_var, expr);
    expr = make_comma_node(assign_node, var_addr_expr(temp_var));
  }  /* if */
  vals_field = complex_vals_field(ctype);
  /* Construct "expr._Vals" or "expr->_Vals". */
  result = field_lvalue_selection_expr(expr, vals_field);
  /* Perform array to pointer decay. */
  result = make_array_to_pointer_node(result);
  return result;
}  /* select_complex_vals */


static an_expr_node_ptr make_real_part(an_expr_node_ptr  expr)
/*
The given expression represents a complex lvalue or rvalue.  Return a node
representing just the real part of that complex value.  The result is always
an lvalue.  The caller is responsible for conversion to an rvalue if
necessary.
*/
{
  an_expr_node_ptr  real_part = select_complex_vals(expr);

  /* A pointer to (the first element of) the "Vals" field is also a pointer
     to the real part of the associated complex value. */
  real_part = add_indirection_to_node(real_part);
  return real_part;
}  /* make_real_part */


static an_expr_node_ptr make_imag_part(an_expr_node_ptr  expr)
/*
The given expression represents a complex lvalue or rvalue.  Return a node
representing just the imaginary part of that complex value.  The result is
always an lvalue.  The caller is responsible for conversion to an rvalue if
necessary.
*/
{
  an_expr_node_ptr  imag_part = select_complex_vals(expr);

  /* Select the second element from the "Vals" field. */
  imag_part->next = node_for_integer_constant((long)1,
                                              targ_ptrdiff_t_int_kind);
  imag_part = make_lvalue_operator_node((an_expr_operator_kind)eok_subscript,
                                        type_pointed_to(imag_part->type),
                                        imag_part);
  return imag_part;
}  /* make_imag_part */


static void lower_c99_jmultiply(an_expr_node_ptr  expr)
/*
Turn the given multiplication of two imaginary values into a real multiply
followed by a sign inversion ( (__I__*a)*(__I__*b) = -(a*b) ).
*/
{
  expr->variant.operation.operands =
                  make_operator_node((an_expr_operator_kind)eok_multiply,
                                     expr->type,
                                     expr->variant.operation.operands);
  expr->variant.operation.kind = (an_expr_operator_kind)eok_negate;
}  /* lower_c99_jmultiply */


static void lower_c99_jdivide(an_expr_node_ptr  expr)
/*
Turn the given division of a real value by an imaginary value into a real
division followed by a sign inversion ( a/(b*__I__) = -(a/b)*__I__ ).
*/
{
  expr->variant.operation.operands =
                  make_operator_node((an_expr_operator_kind)eok_divide,
                                     expr->type,
                                     expr->variant.operation.operands);
  expr->variant.operation.kind = (an_expr_operator_kind)eok_negate;
}  /* lower_c99_jdivide */


static void lower_real_imag_add_subtract(an_expr_node_ptr expr)
/*
Lower a mixed real/imaginary add/subtract operation, i.e.,

  eok_fjadd      real      + imaginary
  eok_jfadd      imaginary + real
  eok_fjsubtract real      - imaginary
  eok_jfsubtract imaginary - real

The code for these assembles a complex value from the two parts,
negating one part in the "-" case.
*/
{
  a_variable_ptr   temp_var = make_lowered_temporary(expr->type);
  an_expr_node_ptr real_part_lvalue, imag_part_lvalue;
  an_expr_node_ptr operand_1, operand_2, assign_1 = NULL, assign_2 = NULL;
  an_expr_node_ptr comma_node;

  /* We will assign the proper values to the components in the temporary,
     then use the temporary as the result. */
  real_part_lvalue = make_real_part(var_lvalue_expr(temp_var));
  imag_part_lvalue = make_imag_part(var_lvalue_expr(temp_var));
  operand_1 = expr->variant.operation.operands;
  operand_2 = operand_1->next;
  operand_1->next = NULL;
  switch (expr->variant.operation.kind) {
    case eok_fjadd:
      /* Real + imaginary.
           (temp.real = operand_1, temp.imag = operand_2)
      */
      real_part_lvalue->next = operand_1;
      assign_1 = make_operator_node((an_expr_operator_kind)eok_assign,
                                    operand_1->type, real_part_lvalue);
      imag_part_lvalue->next = operand_2;
      assign_2 = make_operator_node((an_expr_operator_kind)eok_assign,
                                    operand_2->type, imag_part_lvalue);
      break;
    case eok_jfadd:
      /* Imaginary + real.
           (temp.imag = operand_1, temp.real = operand_2)
      */
      imag_part_lvalue->next = operand_1;
      assign_1 = make_operator_node((an_expr_operator_kind)eok_assign,
                                    operand_1->type, imag_part_lvalue);
      real_part_lvalue->next = operand_2;
      assign_2 = make_operator_node((an_expr_operator_kind)eok_assign,
                                    operand_2->type, real_part_lvalue);
      break;
    case eok_fjsubtract:
      /* Real - imaginary.
           (temp.real = operand_1, temp.imag = -operand_2)
      */
      real_part_lvalue->next = operand_1;
      assign_1 = make_operator_node((an_expr_operator_kind)eok_assign,
                                    operand_1->type, real_part_lvalue);
      operand_2 = make_operator_node((an_expr_operator_kind)eok_negate,
                                     operand_2->type, operand_2);
      imag_part_lvalue->next = operand_2;
      assign_2 = make_operator_node((an_expr_operator_kind)eok_assign,
                                    operand_2->type, imag_part_lvalue);
      break;
    case eok_jfsubtract:
      /* Imaginary - real.
           (temp.imag = operand_1, temp.real = -operand_2)
      */
      imag_part_lvalue->next = operand_1;
      assign_1 = make_operator_node((an_expr_operator_kind)eok_assign,
                                    operand_1->type, imag_part_lvalue);
      operand_2 = make_operator_node((an_expr_operator_kind)eok_negate,
                                     operand_2->type, operand_2);
      real_part_lvalue->next = operand_2;
      assign_2 = make_operator_node((an_expr_operator_kind)eok_assign,
                                    operand_2->type, real_part_lvalue);
      break;
    default:
      unexpected_condition_str("lower_real_imag_add_subtract: bad operator");
  }  /* switch */
  /* Combine the two assignments with a comma. */
  comma_node = make_comma_node(assign_1, assign_2);
  /* Add another comma to return the temporary as the result of the
     operation.  This node is created by overwriting the original node. */
  comma_node->next = var_rvalue_expr(temp_var);
  set_node_operator(expr, (an_expr_operator_kind)eok_comma,
                    expr->type, /*is_lvalue=*/FALSE, comma_node);
}  /* lower_real_imag_add_subtract */

#if C99_IL_EXTENSIONS_SUPPORTED

/* Complex conjugation routines. */
STATIC_THREAD a_routine_ptr
                xconj_routine[NUM_COMPLEX_FLOAT_KINDS];

/* Names of the complex conjugation runtime routines. */
static constexpr a_library_name_array xconj_routine_name = {
                                             "__c99_complex_float16_conj",
                                             NULL, /* fk_fp16 */
                                             "__c99_complex_float_conj",
                                             "__c99_complex_double_conj",
                                             "__c99_complex_double_conj",
                                             "__c99_complex_long_double_conj",
                                             "__c99_complex_long_double_conj",
                                             "__c99_complex_float80_conj",
                                             "__c99_complex_float128_conj",
                                             "__c99_complex_bfloat16_conj"};

void lower_xconj(an_expr_node_ptr  expr)
/*
Transform the given complex conjugation expression (GNU notation "~z", which
produces a value equal to "z" except for the imaginary part being negated)
into a function call.
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_float_kind      fkind;
  a_const_char      *rout_name;
  an_expr_node_ptr  xconj_call;

  check_assertion(is_complex_type(return_type));
  fkind = return_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xconj_routine_name);
  xconj_call = make_prototyped_runtime_call(
                    rout_name,
                    select_routine_from_float_kind(xconj_routine, fkind),
                    return_type, return_type, (a_type_ptr)NULL,
                    expr->variant.operation.operands);
  overwrite_node(expr, xconj_call);
}  /* lower_xconj */


void lower_complex_projection(an_expr_node_ptr  expr)
/*
Lower the given complex projection expression ("__real z" or "__imag z").
Preserve the lvalueness of the expression.
*/
{
  an_expr_node_ptr  arg = expr->variant.operation.operands, result = NULL;
  a_boolean         is_rvalue = !expr->is_lvalue;

  switch (expr->variant.operation.kind) {
    case eok_real_part:
      result = make_real_part(arg);
      break;
    case eok_imag_part:
      result = make_imag_part(arg);
      break;
    default:
      unexpected_condition();
  }  /* switch */
  if (is_rvalue) {
    result = rvalue_expr_for_lvalue(result);
  }  /* if */
  overwrite_node(expr, result);
}  /* lower_complex_projection */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

void lower_c99_complex_cast(an_expr_node_ptr  expr)
/*
Transform the given complex cast expression into a function call
(compatible with C89).
*/
{
  an_expr_node_ptr  src = expr->variant.operation.operands, cast_call;
  a_type_ptr        src_type = skip_typerefs(src->type);
  a_type_ptr        dst_type = skip_typerefs(expr->type);
  a_routine_ptr     *routine = NULL;
  a_const_char      *routine_name = NULL;

  if (is_void_type(dst_type)) {
    /* A cast to void.  Nothing needs to be done. */
  } else if (il_identical_types(src_type, dst_type)) {
    /* A do-nothing cast. */
    if (is_imaginary_type(src_type)) {
      /* Imaginary types become floating-point types, so the cast can be
         left as it is.  This may actually be useful/necessary, because
         such a cast will drop extra precision on intermediate results. */
    } else {
      /* A cast to a complex type is eliminated because it would become
         a cast to struct type. */
      overwrite_node(expr, src);
    }  /* if */
  } else if (is_complex_type(src_type) && is_complex_type(dst_type) &&
             map_extended_float_kinds(src_type->variant.float_kind) ==
                      map_extended_float_kinds(dst_type->variant.float_kind)) {
    /* Effectively a do-nothing cast, mapping complex types with equivalent
       underlying floating-point types.  Eliminate the cast and just change
       the type of the node. */
    overwrite_node(expr, src);
    expr->type = dst_type;
  } else if (is_complex_type(dst_type)) {
    if (is_complex_type(src_type)) {
      /* A change in floating-point precision, complex to complex. */
      const a_library_name_array *library_routine_name =
                                      /*lint -e(545)*/&cast_float_routine_name;
      a_routine_ptr              *routine_ptr = cast_float_routine;
      check_assertion(src_type->variant.float_kind !=
                      dst_type->variant.float_kind);
      switch (map_extended_float_kinds(src_type->variant.float_kind)) {
        case fk_float16:
          library_routine_name = /*line -e(545)*/&cast_float16_routine_name;
          routine_ptr = cast_float16_routine;
          break;
        case fk_std_bfloat16:
          library_routine_name = /*line -e(545)*/&cast_bfloat16_routine_name;
          routine_ptr = cast_bfloat16_routine;
          break;
        case fk_float:
          library_routine_name = /*lint -e(545)*/&cast_float_routine_name;
          routine_ptr = cast_float_routine;
          break;
        case fk_float32x:
        case fk_double:
          library_routine_name = /*lint -e(545)*/&cast_double_routine_name;
          routine_ptr = cast_double_routine;
          break;
        case fk_float64x:
        case fk_long_double:
          library_routine_name =/*lint -e(545)*/&cast_long_double_routine_name;
          routine_ptr = cast_long_double_routine;
          break;
#if FLOAT80_ENABLING_POSSIBLE
        case fk_float80:
          library_routine_name = /*lint -e(545)*/&cast_float80_routine_name;
          routine_ptr = cast_float80_routine;
          break;
#endif /* FLOAT80_ENABLING_POSSIBLE */
#if FLOAT128_ENABLING_POSSIBLE
        case fk_float128:
          library_routine_name = /*lint -e(545)*/&cast_float128_routine_name;
          routine_ptr = cast_float128_routine;
          break;
#endif /* FLOAT128_ENABLING_POSSIBLE */
        default:
          unexpected_condition_str("invalid floating-point kind");
      }  /* switch */
      routine_name = select_name_from_float_kind(dst_type->variant.float_kind,
                                                 *library_routine_name);
      routine = select_routine_from_float_kind(routine_ptr,
                                               dst_type->variant.float_kind);
      cast_call = make_prototyped_runtime_call(
                                         routine_name, routine,
                                         dst_type, src->type, (a_type_ptr)NULL,
                                         src);
    } else if (is_imaginary_type(src_type)) {
      /* Convert imaginary to complex. */
      /* Create a new complex value 0.0 + x*__I__. */
      routine_name = select_name_from_float_kind(dst_type->variant.float_kind,
                                                 itoc_routine_name);
      routine = select_routine_from_float_kind(itoc_routine,
                                               dst_type->variant.float_kind);
      /* Before creating a complex value, be sure the imaginary value is cast
         to the needed precision. */
      src = add_cast_if_necessary(src,
                                 imaginary_type(dst_type->variant.float_kind));
      cast_call = make_prototyped_runtime_call(
                                         routine_name, routine,
                                         dst_type, src->type, (a_type_ptr)NULL,
                                         src);
    } else {
      /* Convert floating-point, fixed-point, or integral to complex. */
      check_assertion(is_arithmetic_or_enum_type(src_type));
      routine_name = select_name_from_float_kind(dst_type->variant.float_kind,
                                                 rtoc_routine_name);
      routine = select_routine_from_float_kind(rtoc_routine,
                                               dst_type->variant.float_kind);
      /* Create a new complex value x + 0.0*__I__. */
      /* Before creating a complex value, be sure the real value is cast
         to the needed precision. */
      src = add_cast_if_necessary(src,
                                  float_type(dst_type->variant.float_kind));
      cast_call = make_prototyped_runtime_call(
                                         routine_name, routine,
                                         dst_type, src->type, (a_type_ptr)NULL,
                                         src);
    }  /* if */
    overwrite_node(expr, cast_call);
  } else if (is_imaginary_type(dst_type)) {
    if (is_complex_type(src_type)) {
      /* Converting a complex value to an imaginary type.  This amounts to
         keeping the imaginary part of the given value. */
      routine_name = select_name_from_float_kind(src_type->variant.float_kind,
                                                 ctoi_routine_name);
      routine = select_routine_from_float_kind(ctoi_routine,
                                               src_type->variant.float_kind);
      cast_call = make_prototyped_runtime_call(
                           routine_name, routine,
                           imaginary_type(src_type->variant.float_kind),
                           src->type, (a_type_ptr)NULL, src);
      cast_call = add_cast_if_necessary(cast_call, dst_type);
      overwrite_node(expr, cast_call);
    } else if (is_real_floating_type(src_type) ||
#if FIXED_POINT_ALLOWED
               is_fixed_point_type(src_type) ||
#endif /* FIXED_POINT_ALLOWED */
               is_integral_or_enum_type(src_type)) {
      /* A real, fixed-point, or integral value converted to an imaginary type
         is always zero.  Use a comma operator to preserve side-effects of the
         source expression. */
      a_constant_ptr    zero_constant = local_constant();
      an_expr_node_ptr  new_expr;
      make_zero_of_proper_type(float_type(dst_type->variant.float_kind),
                               zero_constant);
      new_expr = make_comma_node(src, alloc_node_for_constant(zero_constant));
      overwrite_node(expr, new_expr);
      release_local_constant(&zero_constant);
    } else {
      /* Nothing to be done (imaginary->imaginary). */
      check_assertion(is_imaginary_type(src_type));
    }  /* if */
  } else {
    check_assertion(is_arithmetic_or_enum_type(dst_type));
    if (is_complex_type(src_type)) {
      /* Converting a complex value to a real type.  This amounts to keeping
         the real part of the given value. */
      routine_name = select_name_from_float_kind(src_type->variant.float_kind,
                                                 ctor_routine_name);
      routine = select_routine_from_float_kind(ctor_routine,
                                               src_type->variant.float_kind);
      cast_call = make_prototyped_runtime_call(
                           routine_name, routine,
                           float_type(src_type->variant.float_kind),
                           src->type, (a_type_ptr)NULL, src);
      cast_call = add_cast_if_necessary(cast_call, dst_type);
      overwrite_node(expr, cast_call);
    } else if (is_imaginary_type(src_type)) {
      /* An imaginary value converted to a real or integral type is always
         zero.  Use a comma operator to preserve side-effects of the source
         expression. */
      a_constant_ptr    zero_constant = local_constant();
      an_expr_node_ptr  new_expr;
      make_zero_of_proper_type(dst_type, zero_constant);
      new_expr = make_comma_node(src, alloc_node_for_constant(zero_constant));
      overwrite_node(expr, new_expr);
      release_local_constant(&zero_constant);
    } else {
      unexpected_condition();
    }  /* if */
  }  /* if */
}  /* lower_c99_complex_cast */

#endif /* LOWER_COMPLEX */
#if LOWER_FIXED_POINT

/*
Fixed-point lowering:
--------------------

When LOWER_FIXED_POINT is TRUE, fixed-point constructs are lowered
to standard C.  Fixed-point types are lowered to appropriately-
sized integral types; fixed-point constants are lowered to
integral constants; and fixed-point operations and casts are
lowered to calls of runtime routines.  The runtime routines
are not supplied by EDG.  They can be obtained from Dinkumware, Ltd.
(www.dinkumware.com).

Here's an overview of the runtime interface.

First, let's define a generic container called an "fxvalue"
that can hold the bits of any fixed-point value or any integer value:

  typedef unsigned long long fxvalue;

The underlying type is configurable, but usually it's the largest
integer type, e.g., unsigned long long if that is supported.
It must be big enough to fit all target integers and also all
fixed-point types represented in integral form.  For values taking
less than the full set of bits, the bits of the value are placed
at the least-significant end of the fxvalue (this is done simply
by casting; having the fxvalue type be unsigned prevents sign
extension).

Next, let's define a 5-bit field called an "fxtype" that describes
the type contained in an fxvalue.  Starting from the least-significant
bit:

(2 bits) Precision: 00 short
                    01 default
                    10 long
                    11 integral operand
(1 bit)  Kind:       0 _Fract
                     1 _Accum
(1 bit)  Sign:       0 signed     ]
                     1 unsigned   ]--- also used for integral operands
(1 bit)  Saturation: 0 not _Sat
                     1 _Sat

Next, let's define a 3-bit field called an "fxcontrol" that
describes the pragma state at the point of an operation.
Starting from the least-significant bit:

(1 bit)  FX_FRACT_OVERFLOW   0 DEFAULT
                             1 SAT
(1 bit)  FX_ACCUM_OVERFLOW   0 DEFAULT
                             1 SAT
(1 bit)  FX_FULL_PRECISION   0 OFF
                             1 ON

This may not be used by the runtime (and in fact at the moment the
EDG front end always passes zeroes for those bits).

These fields are combined into several sets of bits that 
describe operand and result types and pragma state.  In each case,
the fields are listed starting from the least-significant bit:

fxmask1: fxcontrol, fxtype for operand (also gives result type)
fxmask2: fxcontrol, fxtype for operand 1, fxtype for operand 2,
         fxtype for result
fxmaskr: fxcontrol, fxtype for operand 1, fxtype for operand 2
fxmaskc: fxcontrol, fxtype for operand, fxtype for result
fxmaskf: fxcontrol, fxtype for operand
fxmaskg: fxcontrol, fxtype for result

The integral types in which these are passed are configurable,
but the default (to match the Dinkumware runtime) is unsigned short
for all fxmasks except fxmask2, which is passed as unsigned long.

The runtime routines are as follows:

// Unary operators:
fxvalue _Fixed_negate(fxmask1, fxvalue);
fxvalue _Fixed_incr  (fxmask1, fxvalue);  // Used for ++
fxvalue _Fixed_decr  (fxmask1, fxvalue);  // Used for --

// Binary operators:
fxvalue _Fixed_add     (fxmask2, fxvalue, fxvalue);
fxvalue _Fixed_subtract(fxmask2, fxvalue, fxvalue);
fxvalue _Fixed_multiply(fxmask2, fxvalue, fxvalue);
fxvalue _Fixed_fivide  (fxmask2, fxvalue, fxvalue);

// Relational operators:
int     _Fixed_eq      (fxmaskr, fxvalue, fxvalue);
int     _Fixed_ne      (fxmaskr, fxvalue, fxvalue);
int     _Fixed_gt      (fxmaskr, fxvalue, fxvalue);
int     _Fixed_lt      (fxmaskr, fxvalue, fxvalue);
int     _Fixed_ge      (fxmaskr, fxvalue, fxvalue);
int     _Fixed_le      (fxmaskr, fxvalue, fxvalue);

// Shift
fxvalue _Fixed_shiftl  (fxmask1, fxvalue, int shift_count);
fxvalue _Fixed_shiftr  (fxmask1, fxvalue, int shift_count);

// Conversion
//   Fixed to (other) fixed, fixed-point to integer, and
//   integer to fixed
fxvalue     _Fixed_conv        (fxmaskc, fxvalue);
//   Fixed to floating-point
float       _Fixed_to_float    (fxmaskf, fxvalue);
double      _Fixed_to_double   (fxmaskf, fxvalue);
long double _Fixed_to_ldouble  (fxmaskf, fxvalue);
//   Floating-point to fixed
fxvalue     _Fixed_from_float  (fxmaskg, float);
fxvalue     _Fixed_from_double (fxmaskg, double);
fxvalue     _Fixed_from_ldouble(fxmaskg, long double);

Conversions to/from complex and imaginary are handled by using
the floating-point conversions on the real part and/or an
appropriate zero.

Compound assignments are handled by rewriting, e.g.,
x += 1 is turned into x = x + 1 (but x is evaluated only
once) and that is lowered.  Likewise increments and decrements
are rewritten as adds or subtracts of 1, e.g., ++x becomes
x = x + 1 and that is lowered.
*/

/*
Integer kind for the fxmask parameter to fixed-point runtime routines.
Must match the size chosen in the runtime provided by Dinkumware.
The fxmask2 case is used for two-operand routines like _Fixed_add,
which require a bigger fxmask because it includes two operand types
and a result type.
*/
#define FXMASK_INT_KIND ((an_integer_kind)ik_unsigned_short)
#define FXMASK2_INT_KIND ((an_integer_kind)ik_unsigned_long)


static int fxtype_value(a_fixed_point_type_descr descr/*lint !e1746*/)
/*
Return the "fxtype" value that describes the indicated fixed-point
type description.  See the documentation above for the bit values.
*/
{
#define FXTYPE_SIZE 5
  int fxtype = 0;

  if (descr.precision == (a_fixed_point_precision)fpp_short) {
    fxtype = 0;
  } else if (descr.precision == (a_fixed_point_precision)fpp_default) {
    fxtype = 1;
  } else if (descr.precision == (a_fixed_point_precision)fpp_long) {
    fxtype = 2;
  } else {
    unexpected_condition();
  }  /* if */
  fxtype |= descr.is_fract_type ? 0 : 0x4;
  fxtype |= descr.is_unsigned ? 0x8 : 0;
  fxtype |= descr.saturating ? 0x10 : 0;
  return fxtype;
}  /* fxtype_value */


static int integral_fxtype_value(a_boolean is_signed)
/*
Return the fxtype value used to represent an integral operand,
signed if is_signed is TRUE.    See the documentation above for the
bit values.
*/
{
  int fxtype = 3;  /* 11 in bottom 2 bits means integral value. */

  if (!is_signed) fxtype |= 0x8;
  return fxtype;
}  /* integral_fxtype_value */


static int fxtype_value_for_type(a_type_ptr type)
/*
Return the fxtype value for the given (fixed-point or integral)
type.  See the documentation above for the bit values.
*/
{
  int fxtype = 0;

  if (is_integral_or_enum_type(type)) {
    fxtype = integral_fxtype_value(is_signed_integral_type(type));
  } else if (is_fixed_point_type(type)) {
    fxtype = fxtype_value(skip_typerefs(type)->variant.fixed_point);
  } else {
    unexpected_condition();
  }  /* if */
  return fxtype;
}  /* fxtype_value_for_type */


static a_type_ptr fxvalue_type(void)
/*
Return the "fxvalue" type, which is the type of the container used to
pass fixed-point and integral values to and from the fixed-point runtime.
*/
{
  /* This will usually be a 64-bit data type if long long is supported,
     and a 32-bit data type otherwise.  One should ensure that the
     runtime is configured to use the same type. */
  a_type_ptr type = integer_type(targ_uintmax_kind);
  return type;
}  /* fxvalue_type */


static unsigned fxcontrol_value(void)
/*
Return the "fxcontrol" value for the current location in the
program.  It describes the current fixed-point pragma state.
See the documentation above for the bit values.
*/
{
#define FXCONTROL_SIZE 3
  /* Currently always returns zero, because the Dinkumware runtime
     does not use the bits. */
  return 0;
}  /* fxcontrol_value */


static an_expr_node_ptr add_cast_to_fxvalue_type(an_expr_node_ptr expr)
/*
Cast the indicated rvalue expression to the fxvalue type used to interface
to the fixed-point runtime routines.
*/
{
  a_type_ptr type = expr->type;

  check_assertion(!expr->is_lvalue);
  if (is_fixed_point_type(type)) {
    type = lowered_integer_type_for_fixed_point_type(type);
    if (is_signed_integral_type(type)) {
      /* For fixed-point types represented as signed integral types,
         cast to the same-sized unsigned type first before widening to
         avoid sign extension. */
      a_type_ptr unsigned_type =
                   other_signedness_integer_type(f_skip_typerefs(type)->
                                                     variant.integer.int_kind);
      expr = add_cast_if_necessary(expr, unsigned_type);
    }  /* if */
  }  /* if */
  expr = add_cast_if_necessary(expr, fxvalue_type());
  return expr;
}  /* add_cast_to_fxvalue_type */


/*
Runtime routine for fixed-point conversions (including integral
source or destination).
*/
STATIC_THREAD a_routine_ptr fixed_conv_routine;
/*
Runtime routines for conversions between floating point and fixed point.  Note
that there are no conversion routines from _Float16, __fp16, __float80, or
__float128.
*/
static constexpr a_library_name_array float_fixed_conv_routine_name = {
                                                         NULL,
                                                         NULL, /* fk_fp16 */
                                                         "_Fixed_from_float",
                                                         "_Fixed_from_double",
                                                         "_Fixed_from_double",
                                                         "_Fixed_from_ldouble",
                                                         "_Fixed_from_ldouble",
                                                         NULL,
                                                         NULL,
                                                         NULL};
STATIC_THREAD a_routine_ptr
                float_fixed_conv_routine[NUM_COMPLEX_FLOAT_KINDS];

static constexpr a_library_name_array fixed_float_conv_routine_name = {
                                                           NULL,
                                                           NULL, /* fk_fp16 */
                                                           "_Fixed_to_float",
                                                           "_Fixed_to_double",
                                                           "_Fixed_to_double",
                                                           "_Fixed_to_ldouble",
                                                           "_Fixed_to_ldouble",
                                                           NULL,
                                                           NULL,
                                                           NULL};
STATIC_THREAD a_routine_ptr
                fixed_float_conv_routine[NUM_COMPLEX_FLOAT_KINDS];


static void lower_c99_fixed_point_cast(an_expr_node_ptr expr)
/*
Lower the indicated cast (which has a fixed-point source and/or
destination) to a runtime call).
*/
{
  an_expr_node_ptr  src = expr->variant.operation.operands;
  an_expr_node_ptr  fxmask_expr, new_expr;
  a_type_ptr        src_type = src->type;
  a_type_ptr        base_src_type = skip_typerefs(src_type);
  a_type_ptr        dst_type = expr->type;
  a_type_ptr        base_dst_type = skip_typerefs(dst_type);
  a_type_ptr        return_type;
  a_type_ptr        param2_type;
  a_routine_ptr     *routine;
  a_const_char      *routine_name;
  a_float_kind      fkind;
  unsigned long     fxmask;
  int               shift_amount = 0;
  a_constant_ptr    zero_constant = local_constant();

  if (is_void_type(dst_type)) {
    /* A cast to void.  Nothing needs to be done. */
  } else if (il_identical_types(base_src_type, base_dst_type)) {
    /* A do-nothing cast.  Leave it as it is (it will be a cast between
       integral types). */
#if C99_IL_EXTENSIONS_SUPPORTED
  } else if (is_imaginary_type(dst_type)) {
    /* A fixed-point value converted to an imaginary type is always zero.
       Use a comma operator to preserve side-effects of the source
       expression. */
    check_assertion(is_fixed_point_type(src_type));
#if LOWER_COMPLEX
    dst_type = float_type(base_dst_type->variant.float_kind);
#endif /* LOWER_COMPLEX */
    make_zero_of_proper_type(dst_type, zero_constant);
    new_expr = make_comma_node(src, alloc_node_for_constant(zero_constant));
    overwrite_node(expr, new_expr);
  } else if (is_imaginary_type(src_type)) {
    /* An imaginary value converted to a fixed-point type is always zero.
       Use a comma operator to preserve side-effects of the source
       expression. */
    check_assertion(is_fixed_point_type(dst_type));
    make_zero_of_proper_type(dst_type, zero_constant);
    lower_c99_fixed_point_constant(zero_constant);
    new_expr = make_comma_node(src, alloc_node_for_constant(zero_constant));
    overwrite_node(expr, new_expr);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  } else {
    /* Generate a call to the runtime cast routine.  There's a primary
       routine _Fixed_conv that handles all the fixed-point/fixed-point
       and fixed-point/integer cases, and 3 routines each (for the different
       precisions) that handle the fixed-point/floating-point cases. */
    routine_name = "_Fixed_conv";
    routine = &fixed_conv_routine;
    return_type = param2_type = fxvalue_type();
    /* Build up the fxmask argument describing the operand and result
       types. */
    fxmask = fxcontrol_value();
    shift_amount = FXCONTROL_SIZE;
    if (is_fixed_point_type(src_type) || is_integral_or_enum_type(src_type)) {
      /* Add the mask for the source type. */
      fxmask |= ((unsigned long)(unsigned)fxtype_value_for_type(src_type) <<
                                                                 shift_amount);
      shift_amount += FXTYPE_SIZE;
      /* Convert the operand to the fxvalue type used to interface to the
         runtime. */
      src = add_cast_to_fxvalue_type(src);
    } else {
      /* Conversion from floating or complex to fixed point. */
#if C99_IL_EXTENSIONS_SUPPORTED
      check_assertion(is_floating_type(src_type) ||
                      is_complex_type(src_type));
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
      check_assertion(is_floating_type(src_type));
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      check_assertion(is_fixed_point_type(dst_type));
      fkind = base_src_type->variant.float_kind;
#if C99_IL_EXTENSIONS_SUPPORTED
      if (is_complex_type(src_type)) {
        /* Convert the operand from complex to the same-precision floating
           type. */
        src = add_cast_if_necessary(src, float_type(fkind));
#if LOWER_COMPLEX
        lower_c99_complex_cast(src);
#endif /* LOWER_COMPLEX */
      }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      routine_name = select_name_from_float_kind(fkind,
                                                float_fixed_conv_routine_name);
      routine = select_routine_from_float_kind(float_fixed_conv_routine,
                                               fkind);
      param2_type = float_type(fkind);
    }  /* if */
    if (is_fixed_point_type(dst_type) || is_integral_or_enum_type(dst_type)) {
      /* Add the mask for the destination type. */
      fxmask |= ((unsigned long)(unsigned)fxtype_value_for_type(dst_type) <<
                                                                 shift_amount);
      shift_amount += FXTYPE_SIZE;
    } else {
      /* Conversion from fixed point to floating or complex. */
#if C99_IL_EXTENSIONS_SUPPORTED
      check_assertion(is_floating_type(dst_type) ||
                      is_complex_type(dst_type));
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
      check_assertion(is_floating_type(dst_type));
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      check_assertion(is_fixed_point_type(src_type));
      fkind = base_dst_type->variant.float_kind;
      routine_name = select_name_from_float_kind(fkind,
                                                fixed_float_conv_routine_name);
      routine = select_routine_from_float_kind(fixed_float_conv_routine,
                                               fkind);
      return_type = float_type(fkind);
    }  /* if */
    fxmask_expr = node_for_integer_constant((long)fxmask, FXMASK_INT_KIND);
    /* Make the call of the runtime cast routine. */
    fxmask_expr->next = src;
    new_expr = make_prototyped_runtime_call(routine_name, routine,
                                            return_type,
                                            integer_type(FXMASK_INT_KIND),
                                            param2_type,
                                            fxmask_expr);
    /* Cast the value returned by the runtime routine to the final
       desired type. */
    new_expr = add_cast_if_necessary(new_expr, expr->type);
#if LOWER_COMPLEX
    if (is_complex_type(dst_type)) {
      lower_c99_complex_cast(new_expr);
    }  /* if */
#endif /* LOWER_COMPLEX */
    /* Overwrite the original node with the lowered expression. */
    overwrite_node(expr, new_expr);
  }  /* if */
  release_local_constant(&zero_constant);
}  /* lower_c99_fixed_point_cast */

#endif /* LOWER_FIXED_POINT */

void lower_c99_ne_0_if_needed(an_expr_node_ptr expr)
/*
The given operation is a "x != 0" operation generated for boolean normalization
purposes (either to implement eok_bool_cast, or to normalize a boolean
controlling expression).  If the comparison involves C99 types that require
lowering (like complex or fixed-point types), perform that lowering.  The first
operand is lowered already, but the second operand (i.e., the zero constant) is
not.
*/
{
  check_assertion(is_operation_node(expr) && node_operator_is(expr, eok_ne));
  switch (expr->variant.operation.type_kind) {
#if LOWER_FIXED_POINT
    case tk_fixed_point:
      /* Do further lowering for fixed-point != 0. */
      lower_c99_expr(expr->variant.operation.operands->next);
      lower_c99_fixed_point_operation(expr);
      break;
#endif /* LOWER_FIXED_POINT */
#if LOWER_COMPLEX
    case tk_float:
    case tk_imaginary:
      /* Do further lowering for imaginary != 0 if needed. */
      { an_expr_node_ptr  op2 = expr->variant.operation.operands->next;
        if (is_imaginary_type(op2->type)) {
          /* Lower the imaginary zero constant. */
          lower_c99_expr(op2);
        }  /* if */
      }
      break;
    case tk_complex:
      /* Do further lowering for complex != 0. */
      /* Lower the complex zero constant. */
      lower_c99_expr(expr->variant.operation.operands->next);
      lower_c99_xne(expr);
      break;
#endif /* LOWER_COMPLEX */
    default:;
  }  /* switch */
}  /* lower_c99_ne_0_if_needed */


void lower_type_of_vla_cast_if_necessary(an_expr_node_ptr expr)
/*
The expression is a type of cast operation that may or may not operate
on a VLA type.  If the cast introduces a VLA type, we need to lower its
dimension expression and (in some configurations) compute its dimension
variables.  Note that compiler-generated casts may cast to variably modified
types that have already been visited.
*/
{
  a_type_ptr    type = expr->type;

  if (vla_enabled &&
#if LOWER_VARIABLE_LENGTH_ARRAYS
      !type->visited_for_vla_lowering &&
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
      is_directly_variably_modified_type(type)) {
#if LOWER_VARIABLE_LENGTH_ARRAYS
    lower_vla_cast(expr);
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
    lower_vla_dimensions_in_type(type);
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  }  /* if */
}  /* lower_type_of_vla_cast_if_necessary */


void lower_c99_cast(an_expr_node_ptr  expr)
/*
Transform the given cast expression into a function call (compatible with C89).
*/
{
  if (expr->variant.operation.kind == (an_expr_operator_kind)eok_bool_cast) {
    /* Change a cast to bool to a "!= 0" test. */
    lower_bool_cast(expr);
  } else {
#if LOWER_FIXED_POINT || LOWER_COMPLEX
    a_type_ptr  tp = expr->type;
    a_type_ptr  src_tp = expr->variant.operation.operands->type;
#endif /* LOWER_FIXED_POINT || LOWER_COMPLEX */
    check_assertion(expr->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_cast);
    /* If the cast introduces a VLA type, handle that. */
    lower_type_of_vla_cast_if_necessary(expr);
#if LOWER_FIXED_POINT
    if (fixed_point_enabled &&
        (is_fixed_point_type(tp) ||
         is_fixed_point_type(src_tp))) {
      lower_c99_fixed_point_cast(expr);
    } else
#endif /* LOWER_FIXED_POINT */
    /* Do not add code here. */
    {
#if LOWER_COMPLEX
      if (is_nonreal_floating_type(tp) ||
          is_nonreal_floating_type(src_tp)) {
        lower_c99_complex_cast(expr);
      }  /* if */
#endif /* LOWER_COMPLEX */
    }  /* if */
  }  /* if */
}  /* lower_c99_cast */

#if LOWER_FIXED_POINT

/*
Runtime routines for fixed-point operations.
*/
STATIC_THREAD a_routine_ptr
		fixed_negate_routine,
		fixed_eq_routine,
		fixed_ne_routine,
		fixed_gt_routine,
		fixed_lt_routine,
		fixed_ge_routine,
		fixed_le_routine,
		fixed_add_routine,
		fixed_subtract_routine,
		fixed_multiply_routine,
		fixed_divide_routine,
		fixed_shiftl_routine,
		fixed_shiftr_routine,
		fixed_incr_routine,
		fixed_decr_routine;


static void lower_c99_fixed_point_operation(an_expr_node_ptr expr)
/*
Lower a fixed-point operation expression.
*/
{
  an_expr_operator_kind op = expr->variant.operation.kind;
  an_expr_node_ptr      op1 = expr->variant.operation.operands;
  an_expr_node_ptr      op2 = op1->next;
  an_expr_node_ptr      fxmask_expr, new_expr;
  a_const_char          *routine_name = NULL;
  a_routine_ptr         *routine = NULL;
  unsigned long         fxmask;
  int                   shift_amount = 0;
  a_boolean             need_result_fxtype = FALSE;
  a_boolean             is_comparison = FALSE;
  a_boolean             is_shift = FALSE;
  a_boolean             is_unary = FALSE;
  an_integer_kind       fxmask_int_kind = FXMASK_INT_KIND;
  a_type_ptr            return_type;
  a_type_ptr            op2_arg_type = fxvalue_type();

  /* Select the proper runtime routine for the operation. */
  switch (op) {
    case eok_negate:
      routine_name = "_Fixed_negate";
      routine = &fixed_negate_routine;
      is_unary = TRUE;
      break;
    case eok_eq:
      routine_name = "_Fixed_eq";
      routine = &fixed_eq_routine;
      is_comparison = TRUE;
      break;
    case eok_ne:
      routine_name = "_Fixed_ne";
      routine = &fixed_ne_routine;
      is_comparison = TRUE;
      break;
    case eok_gt:
      routine_name = "_Fixed_gt";
      routine = &fixed_gt_routine;
      is_comparison = TRUE;
      break;
    case eok_lt:
      routine_name = "_Fixed_lt";
      routine = &fixed_lt_routine;
      is_comparison = TRUE;
      break;
    case eok_ge:
      routine_name = "_Fixed_ge";
      routine = &fixed_ge_routine;
      is_comparison = TRUE;
      break;
    case eok_le:
      routine_name = "_Fixed_le";
      routine = &fixed_le_routine;
      is_comparison = TRUE;
      break;
    case eok_add:
      routine_name = "_Fixed_add";
      routine = &fixed_add_routine;
      need_result_fxtype = TRUE;
      break;
    case eok_subtract:
      routine_name = "_Fixed_subtract";
      routine = &fixed_subtract_routine;
      need_result_fxtype = TRUE;
      break;
    case eok_multiply:
      routine_name = "_Fixed_multiply";
      routine = &fixed_multiply_routine;
      need_result_fxtype = TRUE;
      break;
    case eok_divide:
      routine_name = "_Fixed_divide";
      routine = &fixed_divide_routine;
      need_result_fxtype = TRUE;
      break;
    case eok_shiftl:
      routine_name = "_Fixed_shiftl";
      routine = &fixed_shiftl_routine;
      is_shift = TRUE;
      break;
    case eok_shiftr:
      routine_name = "_Fixed_shiftr";
      routine = &fixed_shiftr_routine;
      is_shift = TRUE;
      break;
    default:
      unexpected_condition_str("bad fixed point operator");
  }  /* switch */
  if (is_comparison) {
    /* Result type for comparisons is int. */
    return_type = integer_type((an_integer_kind)ik_int);
  } else {
    /* For most operations, it's the fxvalue type. */
    return_type = fxvalue_type();
  }  /* if */
  /* Build up the fxmask argument describing the operand types. */
  fxmask = fxcontrol_value();
  shift_amount = FXCONTROL_SIZE;
  /* First operand fxtype. */
  fxmask |= ((unsigned long)(unsigned)fxtype_value_for_type(op1->type) <<
                                                                 shift_amount);
  shift_amount += FXTYPE_SIZE;
  if (!is_shift && !is_unary) {
    /* Second operand fxtype. */
    fxmask |= ((unsigned long)(unsigned)fxtype_value_for_type(op2->type) <<
                                                                 shift_amount);
    shift_amount += FXTYPE_SIZE;
  }  /* if */
  if (need_result_fxtype) {
    /* Result fxtype. */
    fxmask |= ((unsigned long)(unsigned)fxtype_value_for_type(expr->type) <<
                                                                 shift_amount);
    /* Operations like "add" need the fxmask2 variant, which includes
       two operand types and a result type and is therefore bigger. */
    fxmask_int_kind = FXMASK2_INT_KIND;
  }  /* if */
  fxmask_expr = node_for_integer_constant((long)fxmask, fxmask_int_kind);
  /* Convert the first operand to the fxvalue type used to interface to the
     runtime. */
  op1->next = NULL;
  op1 = add_cast_to_fxvalue_type(op1);
  if (is_unary) {
    /* A unary operation has no second operand. */
    op2_arg_type = NULL;
  } else {
    /* Convert the second operand to the type used to interface to the
       runtime. */
    if (is_shift) {
      /* For a shift, the second operand is the int shift count. */
      op2_arg_type = integer_type((an_integer_kind)ik_int);
      op2 = add_cast_if_necessary(op2, op2_arg_type);
    } else {
      op2 = add_cast_to_fxvalue_type(op2);
    }  /* if */
  }  /* if */
  /* Make the call of the runtime comparison routine. */
  fxmask_expr->next = op1;
  op1->next = op2;
  new_expr = make_prototyped_runtime_call_full(routine_name, routine,
                                               return_type,
                                               integer_type(fxmask_int_kind),
                                               fxvalue_type(),
                                               op2_arg_type,
                                               NULL, NULL, NULL, NULL,
                                               fxmask_expr);
  /* Cast the value returned by the runtime routine to the final
     desired type (probably does nothing except add a typedef if
     appropriate). */
  new_expr = add_cast_if_necessary(new_expr, expr->type);
  /* Overwrite the original node with the lowered expression. */
  overwrite_node(expr, new_expr);
}  /* lower_c99_fixed_point_operation */


static void lower_c99_fixed_point_incr_decr(an_expr_node_ptr expr)
/*
Lower the indicated fixed-point increment or decrement operation.
*/
{
  an_expr_operator_kind op = expr->variant.operation.kind;
  an_expr_node_ptr      op1 = expr->variant.operation.operands;
  an_expr_node_ptr      op1_for_argument, op1_for_assign, op_node;
  an_expr_node_ptr      op2_node = NULL;
  an_expr_node_ptr      fxmask_expr;
  a_variable_ptr        temp_var = NULL;
  a_boolean             is_post_op = FALSE, temp_init_used;
  a_type_ptr            result_type = prvalue_type(op1->type);
  a_const_char          *routine_name = NULL;
  a_routine_ptr         *routine = NULL;
  unsigned long         fxmask;
  int                   shift_amount = 0;

  check_assertion(op1->is_lvalue);
  switch (op) {
    case eok_post_incr:
      is_post_op = TRUE;
      routine_name = "_Fixed_incr";
      routine = &fixed_incr_routine;
      break;
    case eok_post_decr:
      is_post_op = TRUE;
      routine_name = "_Fixed_decr";
      routine = &fixed_decr_routine;
      break;
    case eok_pre_incr:
      is_post_op = FALSE;
      routine_name = "_Fixed_incr";
      routine = &fixed_incr_routine;
      break;
    case eok_pre_decr:
      is_post_op = FALSE;
      routine_name = "_Fixed_decr";
      routine = &fixed_decr_routine;
      break;
    default:
      unexpected_condition_str(
                              "lower_c99_fixed_point_incr_decr: bad operator");
  }  /* switch */
  if (is_post_op && expr->result_is_not_used) {
    /* We don't need the more complicated post-incr/decr code if the
       result is not used. */
    is_post_op = FALSE;
  }  /* if */
  /* The normal rewrite of
       ++x
     is
       x = _Fixed_incr(x);
     Make a copy of op1 to be used as the argument of the call.
     op1 itself will be used as the left operand of the assignment. */ 
  op1_for_argument = make_lvalue_reusable_copy_full(op1,
                                                    /*vars_can_change=*/FALSE,
                                                    &temp_init_used);
  op1_for_assign = op1;
  if (temp_init_used || is_post_op) {
    /* op1 is complicated and was assigned to a temporary.  Make sure that
       the temporary is initialized before it is used by doing the
       overall rewrite of
         ++x;
       as
         ((temp = *(t = &x)), *t = _Fixed_incr(temp))
       We also use the temporary if the operation is a post-increment
       or -decrement, because we want to save and return the original value. */
    temp_var = make_local_temporary(result_type);
    op1_for_assign = op1_for_argument;
    op1_for_argument = var_lvalue_expr(temp_var);
    /* Make the (temp = *(t = &x)) assignment, to be inserted later. */
    op2_node = make_var_assignment_expr(temp_var, rvalue_expr_for_lvalue(op1));
  }  /* if */
  /* Make the argument for the call. */
  op1_for_argument = rvalue_expr_for_lvalue(op1_for_argument);
  op1_for_argument = add_cast_to_fxvalue_type(op1_for_argument);
  /* Build up the fxmask argument describing the operand and result types. */
  fxmask = fxcontrol_value();
  shift_amount = FXCONTROL_SIZE;
  fxmask |= ((unsigned long)(unsigned)fxtype_value_for_type(result_type) <<
                                                                 shift_amount);
  fxmask_expr = node_for_integer_constant((long)fxmask, FXMASK_INT_KIND);
  /* Make the call of the runtime cast routine. */
  fxmask_expr->next = op1_for_argument;
  op_node = make_prototyped_runtime_call(routine_name, routine,
                                         fxvalue_type(),
                                         integer_type(FXMASK_INT_KIND),
                                         fxvalue_type(),
                                         fxmask_expr);
  /* Cast the value returned by the runtime routine to the final
     desired type. */
  op_node = add_cast_if_necessary(op_node, result_type);
  /* Assign the result to op1 (or the temporary). */
  op_node = make_assignment_expr(
                   op1_for_assign, (an_expr_operator_kind)eok_assign, op_node);
  if (temp_var != NULL) {
    /* Combine the assignment to the temporary and the assignment that
       does the incr/decr call and stores it back in the original
       operand. */
    op_node = make_comma_node(op2_node, op_node);
  }  /* if */
  /* Here, op_node is "x = _Fixed_incr(x)" or a fancier but equivalent
     expression if a temporary was used.  For a pre-operation, that's all
     we need. */
  if (is_post_op) {
    /* A post-increment or post-decrement.  Add a comma expression to
       return the value of the temporary, which is the original value
       of the operand. */
    check_assertion(temp_var != NULL);
    op_node = make_comma_node(op_node, var_rvalue_expr(temp_var));
  }  /* if */
  overwrite_node(expr, op_node);
}  /* lower_c99_fixed_point_incr_decr */

#endif /* LOWER_FIXED_POINT */

static void match_routine_type_in_call(an_expr_node_ptr  call)
/*
"call" is an eok_call node.  If it represents a call to a known routine,
ensure that the type of the enk_routine node matches that of the
a_routine entry.  This may not be the case on entry if the call found a
block-extern declaration of the entry with a type different from an earlier
namespace-scope declaration.  Consider the following example:

  int f(int (*p)[*]);      // (1)
  int main() {
    int a[10][5] = { 0 };
    int f(int (*a)[5]);    // (2)
    f(a);                  // (3)
  }

After the VLA-based declaration in (1) is lowered, it is no longer compatible
with the non-VLA-based type of (2) (and the latter determines the type of the
enk_routine node).  This can lead to problems in the back end.  In
particular, the C-generating back end would generate code that some C
compilers (including newer versions of GCC) do not accept.  The transformation
here (when applicable) consists in changing the type of the enk_routine
node to match the type of the a_routine node, and casting the arguments to
match the adjusted parameter types if needed.
*/
{
  an_expr_node_ptr  target;
  a_type_ptr        call_type;
  a_routine_ptr     callee;

  check_assertion(is_operation_node(call) && node_operator_is(call, eok_call));
  target = call->variant.operation.operands;
  callee = routine_from_function_expr(target);
  if (callee != NULL) {
    /* A known callee. */
    a_routine_type_supplement_ptr  callee_rtsp, call_rtsp;
    callee_rtsp = skip_typerefs(callee->type)->variant.routine.extra_info;
    call_type = f_skip_typerefs(type_pointed_to(target->type));
    check_assertion(is_function_type(call_type));
    call_rtsp = call_type->variant.routine.extra_info;
    if (callee_rtsp->prototyped && call_rtsp->prototyped &&
        !identical_types(call_type, callee->type)) {
      /* The type used for the call and the type of the routine being called
         are different (and the parameter types are known).  Change the type
         of the enk_routine node, and convert the argument types
         accordingly (if needed). */
      an_expr_node_ptr  ap = call->variant.operation.operands->next;
      an_expr_node_ptr  *ip = &target->next;
      a_param_type_ptr  ptp = callee_rtsp->param_type_list;
      a_type_ptr        save_type = target->type;
      target->type = make_pointer_type(callee->type);
      if (!is_routine_node(target)) {
        /* In cases where the function is invoked as (&f)(a), target will
           be an eok_address_of and the underlying enk_routine's type needs
           to be changed as well. */
        check_assertion(is_operation_node(target) &&
                        node_operator_is(target, eok_address_of) &&
                        is_routine_node(target->variant.operation.operands));
        target->variant.operation.operands->type = callee->type;
      }  /* if */
      while (ap != NULL) {
        ap = ap->next;
        if (ptp != NULL) {
          an_expr_node_ptr save_next = (*ip)->next;
          (*ip)->next = NULL;
          *ip = add_cast_if_necessary(*ip, ptp->type);
          (*ip)->next = save_next;
#if LOWER_VARIABLE_LENGTH_ARRAYS
          if (vla_enabled && !(*ip)->type->visited_for_vla_lowering) {
            /* We may be casting to a type that would otherwise not have
               appeared in the lowered IL: Make sure that any newly-introduced
               VLA types are lowered too. */
            record_vla_component_types_for_lowering((*ip)->type);
          }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
          ptp = ptp->next;
        } else {
          if (!callee_rtsp->has_ellipsis) {
            /* There are more arguments than parameters (which can happen in
               GNU emulation mode when gnu_version < 30400).  Restore the
               original type and let the back end deal with it (the C
               generating back end adds a cast to the routine before the
               call).  */
            target->type = save_type;
            break;
          }  /* if */
        }  /* if */
        ip = &(*ip)->next;
      }  /* while */
      if (ptp != NULL) {
        /* If both the file-scope and the local-scope declaration were
           prototyped, they should originally be compatible, and therefore the
           call should have at least as many arguments as there are parameters
           (possibly more if there is an ellipsis parameter).  Such a mismatch
           can occur in gcc mode when gnu_version < 30400.  In this case,
           restore the original type and let the back end deal with it (the C
           generating back end adds a cast to the routine before the call). */
        target->type = save_type;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* match_routine_type_in_call */

                             
static void lower_c99_call(ARG_UNUSED an_expr_node_ptr expr)
/*
Do any required lowering on an eok_call expression node.  The operands
have been lowered already.
*/
{
  match_routine_type_in_call(expr);
#if LOWER_FIXED_POINT
  if (fixed_point_enabled) {
    /* May need to widen some fixed-point arguments passed to
       unprototyped parameters. */
    an_expr_node_ptr op1 = expr->variant.operation.operands;
    a_type_ptr       rout_type = f_skip_typerefs(type_pointed_to(op1->type));
    a_routine_type_supplement_ptr
                     rtsp = rout_type->variant.routine.extra_info;
    a_param_type_ptr param = rtsp->param_type_list;
    an_expr_node_ptr arg;
    if (!rtsp->prototyped) param = NULL;
    for (arg = op1->next; arg != NULL; arg = arg->next) {
      if (param == NULL) {
        /* An unprototyped parameter. */
        if (is_fixed_point_type(arg->type)) {
          do_default_arg_promotions_on_node(arg);
        }  /* if */
      } else {
        /* A prototyped parameter. */
        param = param->next;
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* LOWER_FIXED_POINT */
}  /* lower_c99_call */


void lower_c99_operator(an_expr_node_ptr  expr)
/*
The given expression should be an operation: Replace it by IL that is
compatible with C89 IL.  Nontrivial transformations are needed (among
others) for operations involving complex types, fixed-point types, the
_Bool type, and VLA types.  The type_kind of the operation is lowered
(if necessary) during the lowering post pass.
*/
{
  a_type_kind  op_kind;

  check_assertion(expr->kind == (an_expr_node_kind)enk_operation);
  op_kind = expr->variant.operation.type_kind;
  switch (expr->variant.operation.kind) {
    case eok_address_of:
    case eok_array_to_pointer:
      /* Make sure the address_taken flag is set (it may already have
         been set by the front end, but in some cases the operand has
         since been lowered to a variable and the flag needs setting). 
         There are also some cases where unevaluated expressions that are
         retained in the tree (e.g., operands of sizeof) may not have set
         the address_taken flag. */
      set_address_taken_for_variable_or_routine_expr(
                                             expr->variant.operation.operands);
      break;
    case eok_negate:
      switch (op_kind) {
#if LOWER_FIXED_POINT
        case tk_fixed_point:
          lower_c99_fixed_point_operation(expr);
          break;
#endif /* LOWER_FIXED_POINT */
#if LOWER_COMPLEX
        case tk_complex:
          lower_c99_xnegate(expr);
          break;
#endif /* LOWER_COMPLEX */
        default:
          break;
      }  /* switch */
      break;
    case eok_unary_plus:
      /* Remove this do-nothing operation (it can cause problems in some
         lowered code, e.g., with complex types).  The operand has already been
         lowered. */
      overwrite_node(expr, expr->variant.operation.operands);
      break;
    case eok_post_incr:
    case eok_post_decr:
    case eok_pre_incr:
    case eok_pre_decr:
      switch (op_kind) {
        case tk_integer:
          if (is_bool_type(expr->type)) {
          /* Increments/decrements of bool. */
            lower_bool_incr_decr(expr);
          }  /* if */
          break;
#if LOWER_FIXED_POINT
        case tk_fixed_point:
          lower_c99_fixed_point_incr_decr(expr);
          break;
#endif /* LOWER_FIXED_POINT */
#if LOWER_VARIABLE_LENGTH_ARRAYS
        case tk_pointer:
          if (vla_enabled &&
              is_vla_type(type_pointed_to(expr->type))) {
            /* Arithmetic on pointers to VLAs depends on the run-time sizes of
               those VLAs.  Since the VLAs are lowered, the pointer arithmetic
               must be transformed to explicitly include the run-time sizes. */
            lower_vla_pointer_integer_arithmetic(expr);
          }  /* if */
          break;
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#if LOWER_COMPLEX
        case tk_complex:
          /* In GNU modes, increment/decrement of complex numbers are
             permitted.  */
          lower_c99_xincr_decr(expr);
          break;
#endif /* LOWER_COMPLEX */
        default:
          break;
      }  /* switch */
      break;
    case eok_add:
      switch (op_kind) {
#if LOWER_FIXED_POINT
        case tk_fixed_point:
          lower_c99_fixed_point_operation(expr);
          break;
#endif /* LOWER_FIXED_POINT */
#if LOWER_COMPLEX
        case tk_complex:
          lower_c99_xadd(expr);
          break;
#endif /* LOWER_COMPLEX */
        default:
          break;
      }  /* switch */
      break;
    case eok_subtract:
      switch (op_kind) {
#if LOWER_FIXED_POINT
        case tk_fixed_point:
          lower_c99_fixed_point_operation(expr);
          break;
#endif /* LOWER_FIXED_POINT */
#if LOWER_COMPLEX
        case tk_complex:
          lower_c99_xsubtract(expr);
          break;
#endif /* LOWER_COMPLEX */
        default:
          break;
      }  /* switch */
      break;
    case eok_multiply:
      switch (op_kind) {
#if LOWER_FIXED_POINT
        case tk_fixed_point:
          lower_c99_fixed_point_operation(expr);
          break;
#endif /* LOWER_FIXED_POINT */
#if LOWER_COMPLEX
        case tk_complex:
          lower_c99_xmultiply(expr);
          break;
#endif /* LOWER_COMPLEX */
        default:
          break;
      }  /* switch */
      break;
    case eok_divide:
      switch (op_kind) {
#if LOWER_FIXED_POINT
        case tk_fixed_point:
          lower_c99_fixed_point_operation(expr);
          break;
#endif /* LOWER_FIXED_POINT */
#if LOWER_COMPLEX
        case tk_complex:
          lower_c99_xdivide(expr);
          break;
#endif /* LOWER_COMPLEX */
        default:
          break;
      }  /* switch */
      break;
#if LOWER_FIXED_POINT
    case eok_shiftl:
    case eok_shiftr:
      if (op_kind == (a_type_kind)tk_fixed_point) {
        lower_c99_fixed_point_operation(expr);
      }  /* if */
      break;
#endif /* LOWER_FIXED_POINT */
    case eok_eq:
      switch (op_kind) {
#if LOWER_FIXED_POINT
        case tk_fixed_point:
          lower_c99_fixed_point_operation(expr);
          break;
#endif /* LOWER_FIXED_POINT */
#if LOWER_COMPLEX
        case tk_complex:
          lower_c99_xeq(expr);
          break;
#endif /* LOWER_COMPLEX */
        default:
          break;
      }  /* switch */
      break;
    case eok_ne:
      switch (op_kind) {
#if LOWER_FIXED_POINT
        case tk_fixed_point:
          lower_c99_fixed_point_operation(expr);
          break;
#endif /* LOWER_FIXED_POINT */
#if LOWER_COMPLEX
        case tk_complex:
          lower_c99_xne(expr);
          break;
#endif /* LOWER_COMPLEX */
        default:
          break;
      }  /* switch */
      break;
#if LOWER_FIXED_POINT
    case eok_gt:
    case eok_lt:
    case eok_ge:
    case eok_le:
      if (op_kind == (a_type_kind)tk_fixed_point) {
        lower_c99_fixed_point_operation(expr);
      }  /* if */
      break;
#endif /* LOWER_FIXED_POINT */

    case eok_add_assign:
    case eok_subtract_assign:
    case eok_multiply_assign:
    case eok_divide_assign:
    case eok_remainder_assign:
    case eok_shiftl_assign:
    case eok_shiftr_assign:
    case eok_and_assign:
    case eok_or_assign:
    case eok_xor_assign:
      switch (op_kind) {
        case tk_integer:
          if (is_bool_type(expr->type)) {
            /* Compound assignments to bool don't exist in C89, and must be
               lowered to get the value reduced to 0/1. */
            rewrite_compound_assignment(expr);
          }  /* if */
          break;
        case tk_float:
          if (is_bool_type(expr->type)
#if LOWER_FIXED_POINT
              || is_fixed_point_type(expr->type)
#endif /* LOWER_FIXED_POINT */
                                                ) {
            /* Rewrite operations that involve floating-point arithmetic, but
               whose final result is a boolean or fixed-point value.  E.g.,
                 _Bool b = 1; b += 2.3;  // Requires normalization
               or
                 _Accum acc = 0k; acc += 1.2;
            */
            rewrite_compound_assignment(expr);
          }  /* if */
          break;
#if LOWER_FIXED_POINT
        case tk_fixed_point:
          rewrite_compound_assignment(expr);
          break;
#endif /* LOWER_FIXED_POINT */
#if LOWER_COMPLEX
        case tk_imaginary:
        case tk_complex:
          rewrite_compound_assignment(expr);
          break;
#endif /* LOWER_COMPLEX */
        default:;
      }  /* switch */
      break;
#if LOWER_COMPLEX
    case eok_jmultiply:
      lower_c99_jmultiply(expr);
      break;
    case eok_jdivide:
      lower_c99_jdivide(expr);
      break;
    case eok_fjadd:
    case eok_jfadd:
    case eok_fjsubtract:
    case eok_jfsubtract:
      /* Mixed real/imaginary add/subtract. */
      lower_real_imag_add_subtract(expr);
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case eok_xconj:
      lower_xconj(expr);
      break;
    case eok_real_part:
    case eok_imag_part:
      lower_complex_projection(expr);
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#endif /* LOWER_COMPLEX */
    case eok_cast:
    case eok_bool_cast:
      lower_c99_cast(expr);
      break;
    case eok_call:
      lower_c99_call(expr);
      break;
#if LOWER_VARIABLE_LENGTH_ARRAYS
    case eok_subscript:
      if (vla_enabled && is_vla_type(expr->type)) {
        /* VLA subscript operations depend on the run-time sizes of the VLA. */
        lower_vla_pointer_integer_arithmetic(expr);
      }  /* if */
      break;
    case eok_padd:
    case eok_psubtract:
    case eok_padd_assign:
    case eok_psubtract_assign:
      if (vla_enabled && is_vla_type(type_pointed_to(expr->type))) {
        /* Arithmetic on pointers to VLAs depends on the run-time sizes of
           those VLAs.  Since the VLAs are lowered, the pointer arithmetic
           must be transformed to explicitly include the run-time sizes. */
        lower_vla_pointer_integer_arithmetic(expr);
      }  /* if */
      break;
    case eok_pdiff:
      if (vla_enabled &&
          is_vla_type(type_pointed_to(
                                    expr->variant.operation.operands->type))) {
        /* Arithmetic on pointers to VLAs depends on the run-time sizes of
           those VLAs.  Since the VLAs are lowered, the pointer arithmetic
           must be transformed to explicitly include the run-time sizes. */
        lower_vla_pointer_difference(expr);
      }  /* if */
      break;
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#if !PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL
    case eok_comma:
      /* Remove void casts from a comma operator. */
      lower_comma(expr);
      break;
#endif /* !PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL */
    default:
      /* Nothing needs to be done. */
      break;
  }  /* switch */
}  /* lower_c99_operator */

#if LOWER_FIXED_POINT

static void lower_c99_fixed_point_constant(a_constant_ptr constant)
/*
Lower the indicated fixed-point constant.  The lowered form is an
integral constant.
*/
{
  an_integer_value int_value;

  int_value = constant->variant.fixed_point_value;
  set_constant_kind(constant, (a_constant_repr_kind)ck_integer);
  constant->variant.integer_value = int_value;
  constant->type = lowered_integer_type_for_fixed_point_type(constant->type);
}  /* lower_c99_fixed_point_constant */

#endif /* LOWER_FIXED_POINT */
#if LOWER_COMPLEX

void lower_c99_complex_constant(a_constant_ptr  constant)
/*
Replace the given ck_complex constant by a ck_aggregate constant structure
that can initialize a lowered complex variable.  (Since complex constants are
allocated in file scope, the lowered structure must also be placed there.)
*/
{
  a_constant_ptr  real_part, imag_part, pair;
  a_float_kind    fkind = skip_typerefs(constant->type)->variant.float_kind;
  a_type_ptr      lowered_type = lowered_complex_type(fkind);

  real_part = fs_constant((a_constant_repr_kind)ck_float);
  real_part->type = float_type(fkind);
  memcpy((char *)&real_part->variant.float_value,
         (char *)&constant->variant.complex_value->real,
         sizeof(an_internal_float_value));
  imag_part = fs_constant((a_constant_repr_kind)ck_float);
  imag_part->type = float_type(fkind);
  memcpy((char *)&imag_part->variant.float_value,
         (char *)&constant->variant.complex_value->imag,
         sizeof(an_internal_float_value));
  real_part->next = imag_part;

  pair = fs_constant((a_constant_repr_kind)ck_aggregate);
  pair->type = lowered_type->variant.class_struct_union.field_list->type;
  check_assertion(is_array_type(pair->type));
  pair->variant.aggregate.first_constant = real_part;
  pair->variant.aggregate.last_constant = imag_part;

  set_constant_kind(constant, (a_constant_repr_kind)ck_aggregate);
  constant->type = lowered_type;
  constant->variant.aggregate.first_constant = pair;
  constant->variant.aggregate.last_constant = pair;
}  /* lower_c99_complex_constant */


void lower_c99_complex_aggregate_constant(a_constant_ptr constant)
/*
In some GNU C++ modes (as well as clang C mode), initializer-list syntax can be
used to initialize a complex object.  In such cases the front end provides an
aggregate with two values (for the real and imaginary components).  The lowered
type for a complex object is a structure that contains an array of two
elements, so re-write the aggregate constant to include another level of
aggregate so that it'll match the lowered complex type.  Note that the elements
of the original constant are not lowered here.
*/
{
  a_constant_ptr copy_con;
  a_type_ptr     con_type = skip_typerefs(constant->type);

  check_assertion(constant->kind == (a_constant_repr_kind)ck_aggregate);
  copy_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  check_assertion(in_file_scope(constant) == in_file_scope(copy_con));
  copy_constant(constant, copy_con);
  constant->variant.aggregate.first_constant = copy_con;
  constant->variant.aggregate.last_constant = copy_con;
  constant->type = lowered_complex_type(con_type->variant.float_kind);
  copy_con->type = complex_vals_field(constant->type)->type;
}  /* lower_c99_complex_aggregate_constant */

#endif /* LOWER_COMPLEX */

void lower_c99_constant(a_constant_ptr  constant)
/*
If the given constant contains C99-specific constructs (like _Complex values),
replace them by a representation compatible with C89.
*/
{
  switch (constant->kind) {
#if FIXED_POINT_ALLOWED
    case ck_fixed_point:
#if LOWER_FIXED_POINT
      lower_c99_fixed_point_constant(constant);
#endif /* LOWER_FIXED_POINT */
      break;
#endif /* FIXED_POINT_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_complex:
#if LOWER_COMPLEX
      lower_c99_complex_constant(constant);
#endif /* LOWER_COMPLEX */
      break;
    case ck_imaginary:
#if LOWER_COMPLEX
      /* Represent the constant as a regular floating-point constant.
         Its type will similarly be adjusted. */
      constant->kind = (a_constant_repr_kind)ck_float;
#endif /* LOWER_COMPLEX */
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case ck_aggregate:
#if LOWER_COMPLEX
      if (is_complex_type(constant->type)) {
          /* Clang allows the use of aggregate syntax to initialize the real
             and imaginary portions of a complex object in C mode.  Convert the
             aggregate constant to the proper format before lowering. */
        lower_c99_complex_aggregate_constant(constant);
      }  /* if */
#endif /* LOWER_COMPLEX */
      lower_c99_constant_list(constant->variant.aggregate.first_constant);
      break;
    case ck_address:
      switch (constant->variant.address.kind) {
        case abk_routine:
          /* Routines will be visited from the scope. */
          break;
        case abk_variable:
          /* Variables will be visited from the scope. */
          break;
        case abk_constant:
          /* Nothing to be done (appears only for addresses of string
             constants). */
          break;
        case abk_label:
          /* Nothing to be done. */
          break;
        case abk_temporary:
        case abk_param_ref:
        default:
          unexpected_condition_str("Bad c99 address const kind");
      }  /* switch */
      break;
    case ck_init_repeat:
      lower_c99_constant(constant->variant.init_repeat.constant);
      break;
    case ck_designator:
      /* Note that designated initializers for unions remain even when
         LOWER_DESIGNATED_INITIALIZERS is TRUE. */
      break;
    case ck_error:
    case ck_integer:
    case ck_float:
    case ck_string:
#if GNU_EXTENSIONS_ALLOWED
    case ck_label_difference:
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
    case ck_upc_threads:
    case ck_upc_mythread:
#endif /* UPC_EXTENSIONS_ALLOWED */
      /* Nothing to be done. */
      break;
    case ck_dynamic_init:  /* Not expected here. */
    default:
      unexpected_condition_str("Invalid C99 constant");
      break;
  }  /* switch */
}  /* lower_c99_constant */


void lower_c99_constant_expr(ARG_UNUSED an_expr_node_ptr  expr)
/*
Transform the given enk_constant expression to remove certain C99-specific
constructs.
*/
{
#if LOWER_COMPLEX
  if (is_imaginary_type(expr->type)) {
    /* Turn the imaginary constant into a real floating point constant. */
    lower_c99_constant(node_constant(expr));
  } else if (is_complex_type(expr->type)) {
    /* Replace this node by a reference to a static variable initialized
       with an aggregate representing the constant complex value. */
    a_variable_ptr  tmp;
    a_constant_ptr  constant = node_constant(expr);
    /* See if the variable has been allocated already.  If so, a pointer to
       the variable will have been stored in the assoc_var field. */
    if (constant->assoc_var != NULL) {
      /* Reuse the previously created temporary. */
      tmp = constant->assoc_var;
    } else {
      /* No static variable was created for this constant yet. */
      tmp = make_temporary_in_scope(expr->type,
                                    scope_stack[DEPTH_OF_FILE_SCOPE].il_scope,
                                    /*force_static=*/FALSE,
                                    /*promote_if_necessary=*/FALSE);
      tmp->init_kind = (an_init_kind)initk_static;
      if (!in_file_scope(constant)) {
        /* The constant is local to a function (this happens, for example,
           for recorded constant expressions).  Make a copy in the file
           scope so it can be pointed to from the file-scope variable. */
        a_memory_region_number region_to_switch_back_to;
        switch_to_file_scope_region(&region_to_switch_back_to);
        constant = alloc_unshared_constant_full(constant,
                                                /*source_in_il=*/TRUE,
                                                /*suppress_copy=*/FALSE);
        switch_back_to_original_region(region_to_switch_back_to);
      }  /* if */
      tmp->initializer.constant = constant;
      /* Lower the complex constant.  If the constant is in the file scope,
         lowering of the constant will be delayed until the file scope is
         lowered; this is necessary in cases where the constant may be
         shared and portions of the front end may not be expecting a lowered
         complex constant (i.e., a ck_aggregate) in certain circumstances. */
      lower_os_constant(tmp->initializer.constant);
      constant->assoc_var = tmp;
    }  /* if */
    overwrite_node(expr, var_rvalue_expr(tmp));
  } else
#endif /* LOWER_COMPLEX */
  {
#if LOWER_FIXED_POINT
    if (fixed_point_enabled && is_fixed_point_type(expr->type)) {
      lower_c99_constant(node_constant(expr));
    }  /* if */
#endif /* LOWER_FIXED_POINT */
  }  /* if */
}  /* lower_c99_constant_expr */


static void lower_c99_temp_init(an_expr_node_ptr expr)
/*
Lower the given enk_temp_init expression.  An enk_temp_init is used
in C99 mode to represent a compound literal.
*/
{
  a_dynamic_init_ptr dip = expr->variant.init.dynamic_init;
  a_variable_ptr     var;
  a_type_ptr         temp_type = expr->type;
  an_insert_location insert_location;
  an_init_pos_descr  ipd;
  a_boolean          keep_dynamic_init;
  a_boolean          variably_modified = (vla_enabled &&
                                        is_variably_modified_type(temp_type));
#if LOWER_VARIABLE_LENGTH_ARRAYS
  an_expr_node_ptr   vla_inits = NULL;
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */

  /* This routine is similar to lower_temp_init. */
  /* The compound literal is rewritten to use a temporary.  The temporary
     is initialized to the constant part of the aggregate, and code is
     generated for any non-constant parts. */
  if (variably_modified && !type_is_typedef(temp_type)) {
    /* If the construct introduces a VLA type, we need to lower its dimension
       expression (even when not lowering VLAs). */
    lower_vla_dimensions_in_type(temp_type);
#if LOWER_VARIABLE_LENGTH_ARRAYS
    /* If the compound literal introduces a VLA type, we need to compute its
       dimension variables (this is similar to cast operations). */
    vla_inits = lower_vla_dimensions(temp_type);
    record_vla_component_types_for_lowering(temp_type);
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  }  /* if */
  /* Create the temporary variable.  Note that compound literals created
     outside of functions do not use enk_temp_init so they are not
     seen here (the front end creates an initialized static variable
     for them). */
  check_assertion(!dip->static_temp);
  dip->variable = var = make_lowered_temporary(temp_type);
  if (c11_mode && temp_type->kind == (a_type_kind)tk_typeref &&
      is_typeref_kind(temp_type, trk_for_type_attributes)) {
    /* A compound literal may have alignment, in which case the _Alignas
       attributes have been attached to the enk_temp_init type.  Move those
       to the newly created variable.  Note that this creates a
       trk_for_type_attributes tk_typeref that no longer has any attributes
       (but that's just treated as a no-op). */
    var->source_corresp.attributes = temp_type->source_corresp.attributes;
    temp_type->source_corresp.attributes = NULL;
  }  /* if */
  if (variably_modified) {
    var->has_variably_modified_type = TRUE;
  }  /* if */
  /* Change the enk_temp_init node to an enk_variable node; its lvalueness is
     unchanged. */
  set_expr_node_kind(expr, (an_expr_node_kind)enk_variable);
  node_variable(expr) = var;
  /* Set the insert point preceding the (modified) original expression. */
  set_expr_insert_location(expr, &insert_location);
  set_var_init_pos_descr(var, &ipd);
  /* Lower the initialization. */
  lower_dynamic_init(dip, &ipd,
                     (an_implied_copy_source *)NULL,
                     (a_variable_ptr)NULL,
                     LDIO_NONE,
                     /*others_follow_in_aggr=*/FALSE,
                     &insert_location,
                     &keep_dynamic_init,
                     (a_constant **)NULL);
#if LOWER_VARIABLE_LENGTH_ARRAYS
  /* After lowering, the type will no longer be variably modified. */
  var->has_variably_modified_type = FALSE;
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
  if (var->has_variably_modified_type) {
    /* If the variable has variably modified type, put out an stmk_vla_decl
       for it. */
    a_statement_ptr stmk_vla_decl_stmt =
                   alloc_statement(stmk_vla_decl, /*compiler_generated=*/TRUE);
    stmk_vla_decl_stmt->variant.vla.is_typedef_decl = FALSE;
    stmk_vla_decl_stmt->variant.vla.variant.variable = var;
    add_to_end_of_pending_stmk_init_statements_list(stmk_vla_decl_stmt);
  }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  if (keep_dynamic_init) {
    /* If the initialization for the (non-reusable) temporary is being kept,
       add an stmk_init statement for the initialization. */
    add_stmk_init_for_temp_init(var, dip);
  }  /* if */
  if (var->init_kind == (an_init_kind)initk_zero &&
      !var_has_static_or_thread_storage_duration(var)) {
    /* If an automatic temporary ends up with initk_zero initialization,
       insert code to do the zeroing because we can't count on the block
       being entered at the top. */
    zero_automatic_temporary(var, expr);
  }  /* if */
#if LOWER_VARIABLE_LENGTH_ARRAYS
  if (vla_inits != NULL) {
    /* Be sure to compute any needed VLA dimension variables before any
       expressions inside the compound literal braces. */
    an_expr_node_ptr  new_expr = make_comma_node(vla_inits, copy_node(expr));
    overwrite_node(expr, new_expr);
  }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
}  /* lower_c99_temp_init */


static void lower_c11_generic(an_expr_node_ptr expr,
                              a_statement_ptr  statement)
/*
Lower the C11 _Generic construct.  Replace the node with the operand that
is pointed to by "result" (the front end has determined which of the operands
should be used).  Perform an lvalue-to-rvalue conversion if necessary.
*/
{
  an_expr_node_ptr new_expr;

  new_expr = expr->variant.c11_generic.result;
  if (new_expr->is_lvalue && !expr->is_lvalue) {
    new_expr = rvalue_expr_for_lvalue(new_expr);
  }  /* if */
  check_assertion(expr->is_lvalue == new_expr->is_lvalue);
  new_expr->next = expr->next;
  overwrite_node(expr, new_expr);
  lower_c99_expr_full(expr, statement);
}  /* lower_c11_generic */


static void lower_c99_expr_list(an_expr_node_ptr list,
                                unsigned int     is_bool_controlling_expr_mask)
/*
Lower the given (short) list of expressions (normally, the operands of an
operator).  is_bool_controlling_expr_mask is a bit mask indicating
operands that are boolean controlling expressions.  The least significant bit
of the mask corresponds to the first expression in the list.
*/
{
  an_expr_node_ptr  expr;

  for (expr = list; expr != NULL; expr = expr->next) {
    if (is_bool_controlling_expr_mask & 1) {
      lower_c99_boolean_controlling_expr(expr, /*is_full_expr=*/FALSE);
    } else {
      lower_c99_expr(expr);
    }  /* if */
    is_bool_controlling_expr_mask >>= 1;
  }  /* if */
}  /* lower_c99_expr_list */


static void lower_c99_expr_full(an_expr_node_ptr           expr,
                                ARG_UNUSED a_statement_ptr statement)
/*
Transform the given expression to remove certain C99-specific constructs.
If statement is non-NULL, expr is the expression of the expression
statement "statement".  See lower_c99_expr for an interface without the
second parameter.
*/
{
  unsigned int bool_controlling_expr_mask;

  mark_as_visited(expr);
  switch (expr->kind) {
    case enk_operation:
#if LOWER_VARIABLE_LENGTH_ARRAYS
      /* Before lowering operands of an expression, look for a couple
         of special cases that operate on a VLA operand. */
      if (vla_enabled && is_vla_type(expr->variant.operation.operands->type)) {
        lower_vla_operations_before_operands_are_lowered(expr);
      }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
      /* Look for some special cases before the expression is lowered.
         In each of these cases, the expr subtree must be lowered during
         the special processing. */
      if (node_operator_is(expr, eok_question)) {
        /* Lower a question operator and everything under it. */
        lower_question_operator(expr, /*assume_expr_is_non_null=*/FALSE);
      } else if (node_operator_is(expr, eok_land) ||
                 node_operator_is(expr, eok_lor)) {
        /* Lower a logical operator and everything under it. */
        lower_logical_operator(expr);
      } else {
        /* First lower all the operands (if any). */
        /* Determine which operands if any are boolean controlling
           expressions. */
        bool_controlling_expr_mask = expr_boolean_controlling_expr_mask(expr);
        lower_c99_expr_list(expr->variant.operation.operands,
                            bool_controlling_expr_mask);
        /* Then transform the current operator if needed. */
        lower_c99_operator(expr);
#if MINIMAL_INLINING
        if (is_call_node(expr)) {
          /* Do inlining of a call if appropriate. */
          if (inlining_enabled) {
            a_boolean expr_has_been_detached;
            do_inlining_of_call(expr, statement, &expr_has_been_detached);
            if (expr_has_been_detached) expr = NULL;
          }  /* if */
        }  /* if */
#endif /* MINIMAL_INLINING */
      }  /* if */
      break;
    case enk_constant:
      lower_c99_constant_expr(expr);
      break;
    case enk_temp_init:
      lower_c99_temp_init(expr);
      break;
    case enk_variable:
#if MINIMAL_INLINING
      if (expr->is_lvalue && node_variable(expr)->is_parameter) {
        /* Set flag indicating that this parameter is used as an lvalue. */
        node_variable(expr)->param_used_as_lvalue = TRUE;
      }  /* if */
#endif /* MINIMAL_INLINING */
#if LOWER_VARIABLE_LENGTH_ARRAYS
      if (node_variable(expr)->is_vla && expr->is_lvalue) {
        /* VLAs are lowered to pointers (to automatically managed storage).
           The pointer value should be used. */
        lower_vla_variable_lvalue(expr);
        /* Note that expr is no longer an enk_variable node. */
      }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
      break;
    case enk_routine:
      /* Although enk_routine nodes are not lowered here, they may be
         adjusted (in match_routine_type_in_call) when processing call
         nodes. */
#if LOWER_IFUNC
      if (node_routine(expr)->is_ifunc) {
        /* Re-write the node to avoid calling the wrapper. */
        lower_ifunc_expr(expr);
      }  /* if */
#endif /* LOWER_IFUNC */
      break;
    case enk_field:
    case enk_address_of_ellipsis:
      /* Nothing to be done. */
      break;
    case enk_sizeof:
      lower_runtime_sizeof(expr);
      break;
    case enk_statement:
      /* GNU C statement expression, ({...}). */
      lower_gnu_statement_expression(expr);
      break;
    case enk_reuse_value:
      lower_reuse_value_expr(expr);
      break;
    case enk_builtin_operation:
      lower_builtin_operation(expr);
      break;
#if VLA_DEALLOCATIONS_IN_IL
    case enk_vla_dealloc:
#if LOWER_VARIABLE_LENGTH_ARRAYS
      lower_vla_dealloc(expr);
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
      break;
#endif /* VLA_DEALLOCATIONS_IN_IL */
    case enk_param_ref:
      /* Nothing to be done (the back end handles this case). */
      check_assertion(expr->variant.param_ref.param_num != 0 &&
                      expr->variant.param_ref.levels_up == 1);
      break;
    case enk_c11_generic:
      lower_c11_generic(expr, statement);
      break;
#if BUILTIN_FUNCTIONS_ENABLED
    case enk_builtin_choose_expr:
      lower_c99_expr_list(expr->variant.builtin_choose_expr.operands,
                          /*is_bool_controlling_expr_mask=*/0x0);
      break;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
    default:
      unexpected_condition_str("Invalid C99 IL expression kind");
      break;
  }  /* switch */
  if (expr != NULL) {
#if LOWER_VARIABLE_LENGTH_ARRAYS
    if (vla_enabled && !expr->type->visited_for_vla_lowering) {
      record_vla_component_types_for_lowering(expr->type);
    }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  }  /* if */
}  /* lower_c99_expr_full */


void lower_c99_expr(an_expr_node_ptr  expr)
/*
Do C99 lowering on the indicated expression.
*/
{
#if DEBUG
  unsigned long         checksum = 0;
#endif /* DEBUG */

#if DEBUG
  if (db_flag_is_set("lower_expr")) {
    checksum = compute_checksum_for_expr(expr);
    (void)fprintf(f_debug, "C99 Expression before lowering");
    db_expr_range(expr);
    fputs(":\n", f_debug);
    db_expression(expr);
  }  /* if */
#endif /* DEBUG */
  lower_c99_expr_full(expr, (a_statement_ptr)NULL);
#if DEBUG
  if (db_flag_is_set("lower_expr") &&
      (checksum != compute_checksum_for_expr(expr))) {
    (void)fprintf(f_debug, "C99 Expression after lowering");
    db_expr_range(expr);
    fputs(":\n", f_debug);
    db_expression(expr);
  }  /* if */
#endif /* DEBUG */
}  /* lower_c99_expr */


void lower_c99_full_expr(an_expr_node_ptr expr)
/*
Do C99 lowering on the indicated full expression.  A full expression is
one not contained inside another expression.
*/
{
#if CHECKING
  /* Make sure we're not in a nested full-expression (by definition, that
     would make this a subexpression and temporaries might be incorrectly
     reused in that case). */
  check_assertion(!curr_context->in_full_expression);
  curr_context->in_full_expression = TRUE;
#endif /* CHECKING */
#if !PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL
  if (expr->result_is_not_used &&
      is_operation_node(expr) &&
      node_operator_is(expr, eok_cast) &&
      is_void_type(expr->type)) {
    /* Remove a top-level cast to void. */
    overwrite_node(expr, expr->variant.operation.operands);
  }  /* if */
#endif /* !PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL */
  lower_c99_expr(expr);
  end_of_full_expr_processing(expr);
#if CHECKING
  curr_context->in_full_expression = FALSE;
#endif /* CHECKING */
}  /* lower_c99_full_expr */


void lower_c99_boolean_controlling_expr(an_expr_node_ptr expr,
                                        a_boolean        is_full_expr)
/*
Lower a boolean controlling expression, e.g., the expression in an "if"
statement.  The expression is not an lvalue.  The expression is a full
expression (i.e., not an expression inside some other expression) if
is_full_expr is TRUE.
*/
{
  if (is_full_expr) {
    lower_c99_full_expr(expr);
  } else {
    lower_c99_expr(expr);
  }  /* if */
  normalize_boolean_controlling_expr_if_needed(expr);
}  /* lower_c99_boolean_controlling_expr */


#if BACK_END_IS_C_GEN_BE

static a_boolean constant_has_empty_initializer(a_constant_ptr con)
/*
Return TRUE if the constant con was written as an empty initializer ("{}") in
the source, or if it contains one at any depth.  An empty initializer appears
in the IL as a ck_aggregate constant that has no values on its list and that
records explicit braces.
*/
{
  a_boolean       result = FALSE;
  a_constant_ptr  elem;

  if (constant_is(con, ck_init_repeat)) {
    result = constant_has_empty_initializer(con->variant.init_repeat.constant);
  } else if (constant_is(con, ck_aggregate)) {
    if (con->variant.aggregate.first_constant == NULL) {
      result = con->explicit_braces_on_aggregate;
    } else {
      for (elem = con->variant.aggregate.first_constant;
           elem != NULL;
           elem = elem->next) {
        if (constant_has_empty_initializer(elem)) {
          result = TRUE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return result;
}  /* constant_has_empty_initializer */


static a_boolean init_must_zero_whole_variable(a_dynamic_init_ptr dip)
/*
Return TRUE if the initialization described by dip must be preceded by a call
that sets the whole variable being initialized to zero.  That is the case when
an empty initializer ("{}") is used somewhere in it: An empty initializer sets
the padding of the object or subobject that it initializes to zero, and an
initializer in the generated C cannot express that, so the values that are
specified are instead assigned to a variable that has already been zeroed.  A
variable that does not have automatic storage duration is zeroed before the
program starts and needs no such treatment.
*/
{
  a_boolean       result = FALSE;
  a_variable_ptr  var = dip->variable;

  if (var != NULL && !var_has_static_or_thread_storage_duration(var) &&
      (dyn_init_is(dip, dik_constant) ||
       dyn_init_is(dip, dik_nonconstant_aggregate))) {
    result = constant_has_empty_initializer(dip->variant.constant.ptr);
  }  /* if */
  return result;
}  /* init_must_zero_whole_variable */

#endif /* BACK_END_IS_C_GEN_BE */


#if LOWER_VARIABLE_LENGTH_ARRAYS

static void zero_vla_storage(a_variable_ptr      vla_var,
                             an_insert_location  *insert_location)
/*
Insert at *insert_location a call that sets the storage of the variable-length
array vla_var to zero.  That storage was allocated by the code generated for
the stmk_vla_decl statement that precedes the insertion point.
*/
{
  an_expr_node_ptr  size_expr = vla_size_expr(vla_var->type,
                                              /*byte_count=*/TRUE),
                    addr_expr = var_lvalue_expr(vla_var);
  a_type_ptr        addr_type = make_pointer_type(addr_expr->type);

  /* A VLA variable is lowered to a pointer to the storage of the array, so
     the address to zero is an rvalue reference to the variable.  Its type is
     adjusted to match; lower_vla_types later replaces the array type it
     points to by the underlying element type. */
  addr_expr = rvalue_expr_for_lvalue(addr_expr);
  addr_expr->type = addr_type;
  insert_runtime_zeroing_call(addr_expr, size_expr, insert_location);
}  /* zero_vla_storage */

#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */


static void lower_c99_stmk_init(a_statement_ptr statement)
/*
Do C99 lowering on the indicated stmk_init statement.
*/
{
  a_dynamic_init_ptr dip = statement->variant.dynamic_init;
  an_insert_location insert_location;
#if BACK_END_IS_C_GEN_BE
  a_boolean          zero_whole_variable = init_must_zero_whole_variable(dip);
#endif /* BACK_END_IS_C_GEN_BE */

  set_insert_location(statement, &insert_location);
#if LOWER_VARIABLE_LENGTH_ARRAYS
  if (is_dynamic_init_for_vla(dip)) {
    /* The only initializer a variable-length array can have is an empty one,
       which zeroes the whole array.  The code that generates initializations
       describes the entity being initialized in terms of its type, which
       cannot express a size that is not known until run time, so zero the
       storage here and discard the initialization. */
    zero_vla_storage(dip->variable, &insert_location);
    turn_statement_into_noop(statement);
    goto done;
  }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#if LOWER_DESIGNATED_INITIALIZERS
  lower_dynamic_init_designated_initializers(dip, (a_type_ptr)NULL,
                                             &insert_location);
#endif /* LOWER_DESIGNATED_INITIALIZERS */
#if BACK_END_IS_C_GEN_BE
  if (zero_whole_variable) {
    /* Record that the initialization does not provide a value for every part
       of the variable, and that it must be done where it appears rather than
       on the declaration of the variable.  Together those cause the back end
       to zero the variable and then assign the specified values to it.  This
       has to be done after the designated initializers have been merged into
       the constant, because that merging removes the empty initializers and
       recomputes is_partially_initialized. */
    dip->is_partially_initialized = TRUE;
    dip->follows_an_exec_statement = TRUE;
  }  /* if */
#endif /* BACK_END_IS_C_GEN_BE */
  /* This routine is similar to lower_stmk_init. */
  switch (dip->kind) {
    case dik_constant:
      /* A case that's valid in C89.  Lower the subtree but leave the
         stmk_init statement. */
      lower_c99_constant(dip->variant.constant.ptr);
      break;
    case dik_expression:
      /* A case that's valid in C89.  Lower the subtree but leave the
         stmk_init statement. */
      lower_c99_full_expr(dip->variant.expression);
      break;
    case dik_nonconstant_aggregate:
      /* An aggregate containing some non-constant parts. */
      /* This is not valid in C89, so convert the nonconstant parts to
         executable code. */
      { a_boolean          keep_dynamic_init;
        an_init_pos_descr  ipd;
        a_variable_ptr     var = dip->variable;

        check_assertion(var != NULL);
        set_var_init_pos_descr(var, &ipd);
        lower_dynamic_init(dip, &ipd,
                           (an_implied_copy_source *)NULL,
                           (a_variable_ptr)NULL,
                           LDIO_FULL_EXPR,
                           /*others_follow_in_aggr=*/FALSE,
                           &insert_location, &keep_dynamic_init,
                           (a_constant **)NULL);
        if (!keep_dynamic_init) {
          /* Delete the stmk_init statement. */
          turn_statement_into_noop(statement);
        }  /* if */
      }
      break;
    default:
      unexpected_condition_str("lower_c99_stmk_init: bad kind");
  }  /* switch */
#if LOWER_VARIABLE_LENGTH_ARRAYS
done:;
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
}  /* lower_c99_stmk_init */


static void lower_c99_for_statement(a_statement_ptr statement)
/*
Do C99 lowering on an stmk_for statement.
*/
{
  a_statement_ptr for_stmt = statement;
  a_for_loop_ptr  flp = for_stmt->variant.for_loop.extra_info;

  if (flp->initialization != NULL) {
    /* Process the initialization expression or declaration. */
    a_statement_ptr init_stmt = flp->initialization;
    check_assertion(init_stmt->next == NULL);
    lower_c99_statement(init_stmt);
    if (init_stmt->kind == (a_statement_kind)stmk_expr &&
        init_stmt->next == NULL) {
      /* The simple C89 case: the initialization is an expression.
         Leave it attached to the for loop. */
    } else {
      /* The C99 case: move the initialization code out into a
         block surrounding the for loop.  Note that this is particularly
         desirable when there is a VLA declaration in the initialization.
         Any pragmas attached to the for loop (e.g., "#pragma GCC unroll")
         must stay with the for loop. */
      an_insert_location insert_location;
      flp->initialization = NULL;
      turn_statement_into_block_transferring_pragma(for_stmt,
                                                    &insert_location,
                                                    &for_stmt,
                                                    curr_context->scope);
      reinsert_for_loop_initialization(init_stmt, &insert_location);
    }  /* if */
  }  /* if */
  if (for_stmt->expr != NULL) {
    lower_c99_boolean_controlling_expr(for_stmt->expr, /*is_full_expr=*/TRUE);
  }  /* if */
  if (flp->increment != NULL) {
    lower_c99_full_expr(flp->increment);
  }  /* if */
  lower_c99_statement(for_stmt->variant.for_loop.statement);
}  /* lower_c99_for_statement */


static void lower_c99_constant_list(a_constant_ptr constant_list)
/*
Do C99 lowering on a constant list.
*/
{
  a_constant_ptr constant;

  for (constant = constant_list;
       constant != NULL;
       constant = constant->next) {
    lower_c99_constant(constant);
  }  /* if */
}  /* lower_c99_constant_list */


static void lower_c99_switch_case(a_switch_case_entry_ptr  entry)
/*
Do C99 lowering on the constants pointed to by the given switch case entry.
*/
{
  if (entry->case_value != NULL) {
    lower_c99_constant(entry->case_value);
#if GNU_EXTENSIONS_ALLOWED
    if (entry->range_end != NULL) {
      lower_c99_constant(entry->range_end);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
}  /* lower_c99_switch_case */


static void lower_c99_statement_list(a_statement_ptr statement_list)
/*
Do C99 lowering on the indicated statement list.
*/
{
  a_statement_ptr statement;

  for (statement = statement_list;
       statement != NULL;
       statement = statement->next) {
    lower_c99_statement(statement);
  }  /* for */
}  /* lower_c99_statement_list */


void lower_c99_statement(a_statement_ptr statement)
/*
Do C99 lowering on the indicated statement.
*/
{
#if DEBUG
  unsigned long      checksum = 0;
#endif /* DEBUG */

  if (statement != NULL) {
    a_statement_ptr   saved_pending_stmk_init_statements =
                                                  pending_stmk_init_statements;
    a_source_position saved_error_position, saved_code_pos;

#if DEBUG
    if (db_flag_is_set("lower_statement")) {
      checksum = compute_checksum_for_statement(statement);
      (void)fprintf(f_debug, "C99 Statement before lowering: ");
      db_statement(statement);
    }  /* if */
#endif /* DEBUG */
    pending_stmk_init_statements = NULL;
    /* Track the source position. */
    saved_code_pos = code_pos_for_lowering;
    code_pos_for_lowering = statement->position;
    saved_error_position = error_position;
    error_position = code_pos_for_lowering;
    if (statement->expr != NULL) {
      /* Lower the expression of the statement. */
      switch (statement->kind) {
        case stmk_expr:
        case stmk_if:
        case stmk_while:
        case stmk_end_test_while:
        case stmk_for:
        case stmk_stmt_expr_result:
          /* These cases are handled below in some special way. */
          break;
        default:
          lower_c99_full_expr(statement->expr);
          break;
      }  /* switch */
    }  /* if */
    switch (statement->kind) {
      case stmk_goto:
      case stmk_label:
      case stmk_return:
#if ASM_FUNCTION_ALLOWED
      case stmk_asm_func_body:
#endif /* ASM_FUNCTION_ALLOWED */
      case stmk_decl:
      case stmk_empty:
#if UPC_EXTENSIONS_ALLOWED
      case stmk_upc_notify:
      case stmk_upc_wait:
      case stmk_upc_barrier:
      case stmk_upc_fence:
#endif /* UPC_EXTENSIONS_ALLOWED */
        /* Nothing to lower. */
        break; 
      case stmk_asm:
        lower_asm_statement(statement);
        break;
      case stmk_stmt_expr_result:
        /* In non-C++ mode, this cannot involve a "return-by-constructor-call"
           and so we just fall through to the ordinary expression case. */
        check_assertion(statement->variant.stmt_expr_result.dynamic_init ==
                                                                         NULL);
        FALLTHROUGH
      case stmk_expr:
        /* Expression statement.  Pass in the statement to allow better
           inlining. */
        check_assertion(statement->expr != NULL); /* For Coverity. */
        lower_c99_expr_full(statement->expr, statement);
        end_of_full_expr_processing(statement->expr);
        break;
      case stmk_if:
        check_assertion(statement->expr != NULL); /* For Coverity. */
        lower_c99_boolean_controlling_expr(statement->expr,
                                           /*is_full_expr=*/TRUE);
        lower_c99_statement(statement->variant.if_stmt.then_statement);
        if (statement->variant.if_stmt.else_statement != NULL) {
          lower_c99_statement(statement->variant.if_stmt.else_statement);
        }  /* if */
        break;
      case stmk_while:
      case stmk_end_test_while:
        check_assertion(statement->expr != NULL); /* For Coverity. */
        lower_c99_boolean_controlling_expr(statement->expr,
                                           /*is_full_expr=*/TRUE);
        lower_c99_statement(statement->variant.loop_statement);
        break;
      case stmk_for:
#if UPC_EXTENSIONS_ALLOWED
      case stmk_upc_forall:
#endif /* UPC_EXTENSIONS_ALLOWED */
        lower_c99_for_statement(statement);
        break;
      case stmk_block:
        { a_context context;
          a_scope_ptr scope = statement->variant.block.extra_info->assoc_scope;
          if (scope != NULL) {
            /* The block has an associated scope.  Push it. */
            push_context(&context, scope, (an_object_lifetime_ptr)NULL);
          }  /* if */
          lower_c99_statement_list(statement->variant.block.statements);
          if (scope != NULL) pop_context();
        }
        break;
      case stmk_switch_case:
        lower_c99_switch_case(statement->variant.switch_case.extra_info);
        break;
      case stmk_switch:
        lower_c99_statement(statement->variant.switch_stmt.body_statement);
        break;
      case stmk_init:
        lower_c99_stmk_init(statement);
        break;
      case stmk_set_vla_size:
#if LOWER_VARIABLE_LENGTH_ARRAYS
        /* Replace this statement by one that computes various variables
           describing the size of the VLA. */
        lower_set_vla_size(statement);
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
        /* Record the associated VLA dimension in a temporary variable. */
        create_dimension_variable(statement);
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
        break;
      case stmk_vla_decl:
#if LOWER_VARIABLE_LENGTH_ARRAYS
        lower_vla_decl(statement);
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case stmk_microsoft_try:
        { a_microsoft_try_supplement_ptr tsp =statement->variant.microsoft_try;
          lower_c99_statement(tsp->guarded_statement);
          if (tsp->except_expr != NULL) lower_c99_full_expr(tsp->except_expr);
          lower_c99_statement(tsp->cleanup_statement);
        }
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
      case stmk_assigned_goto:
        /* Lower the expression for the assigned goto.  Although it should
           never occur, pass a NULL statement pointer to the expression
           lowering to prevent inlining from replacing the entire statement. */
        check_assertion(statement->expr != NULL);
        lower_c99_expr_full(statement->expr, (a_statement_ptr)NULL);
        end_of_full_expr_processing(statement->expr);
        break;
#endif /* GNU_EXTENSIONS_ALLOWED */
      default:
        unexpected_condition_str("lower_c99_statement: bad statement kind");
    }  /* switch */
    insert_pending_stmk_init_statements(statement);
    pending_stmk_init_statements = saved_pending_stmk_init_statements;
    error_position = saved_error_position;
    code_pos_for_lowering = saved_code_pos;
#if DEBUG
    if (db_flag_is_set("lower_statement") &&
        (checksum != compute_checksum_for_statement(statement))) {
      (void)fprintf(f_debug, "C99 Statement after lowering: ");
      db_statement(statement);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
}  /* lower_c99_statement */


static void lower_c99_initializer(an_init_kind    init_kind,
                                  an_initializer  *initializer)
/*
Do C99 lowering for an initializer (e.g., from a variable).  init_kind
indicates the kind of initialization, and *initializer provides the details.
*/
{
  switch (init_kind) {
    case initk_static:
      /* The initializer is a constant. */
#if LOWER_DESIGNATED_INITIALIZERS
      lower_designated_initializers(initializer->constant,
                                    (a_dynamic_init *)NULL,
                                    (a_type_ptr)NULL,
                                    (an_insert_location*)NULL);
#endif /* LOWER_DESIGNATED_INITIALIZERS */
      lower_c99_constant(initializer->constant);
      break;
    case initk_dynamic:
      /* The initializer is dynamic. */
      /* That's handled when the stmk_init statement comes up. */
      break;
    case initk_function_local:
      /* Local static variable inits are handled at the scope level. */
      break;
    case initk_none:
      /* Nothing to be done. */
      break;
    case initk_zero:
      /* Should only appear when lowering C++ IL. */
    default:
      unexpected_condition_str("lower_c99_initializer: bad init kind");
  }  /* switch */
}  /* lower_c99_initializer */


static void lower_c99_source_correspondence(
                                       a_source_correspondence *source_corresp)
/*
Do C99 lowering on the indicated source correspondence entry.
*/
{
#if REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING
  rewrite_ucns_in_name(source_corresp);
#endif /* REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING */
}  /* lower_c99_source_correspondence */


static void lower_c99_variable(a_variable_ptr var)
/*
Do C99 lowering on the indicated variable and its subtree.
*/
{
  a_source_position saved_error_position;

  /* Set the error position to the variable position, in case there is
     an error in lowering. */
  saved_error_position = error_position;
  error_position = var->source_corresp.decl_position;
  lower_c99_source_correspondence(&var->source_corresp);
  lower_c99_initializer(var->init_kind, &var->initializer);
#if GNU_EXTENSIONS_ALLOWED
  if (force_variable_definition_via_zeroing &&
      (var->is_not_common ||
       (il_header.default_nocommon && !var->is_common)) &&
      var->storage_class == (a_storage_class)sc_unspecified &&
      var->init_kind == (an_init_kind)initk_none) {
    /* GNU C allows variables without initializers to be marked as "nocommon",
       which indicates that such variables are nontentative definitions.
       This can be translated to plain C IL by providing an explicitly
       zeroing initializer. */
    var->init_kind = (an_init_kind)initk_zero;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if LOWER_VARIABLE_LENGTH_ARRAYS
  /* Traverse the variable type looking for VLA components regardless of the
     setting of has_variably_modified_type because of cases like:
       int (*f)(int (*a)[*]);
     where has_variably_modified_type is FALSE (because parameter types
     aren't considered when setting this flag). */
  var->has_variably_modified_type = FALSE;
  record_vla_component_types_for_lowering(var->type);
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  error_position = saved_error_position;
}  /* lower_c99_variable */


static void lower_c99_type(a_type_ptr type)
/*
Do C99 lowering on the indicated type.  Note that, at present, this
need be called only for named types and tags, i.e., the things that are
on the scope types list.
*/
{
#if REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING
  /* If there are no UCNs avoid some processing. */
  if (il_header.UCN_identifiers_used) {
    lower_c99_source_correspondence(&type->source_corresp);
    if (type->kind == (a_type_kind)tk_struct ||
        type->kind == (a_type_kind)tk_union) {
      /* Visit fields of structs and unions to rewrite UCNs in their names. */
      a_field_ptr field = type->variant.class_struct_union.field_list;
      for (; field != NULL; field = field->next) {
        lower_c99_source_correspondence(&field->source_corresp);
      }  /* for */
    }  /* if */
  }  /* if */
#endif /* REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING */
#if LOWER_VARIABLE_LENGTH_ARRAYS
  if (vla_enabled && type_is_typedef(type)) {
    prepare_to_lower_variably_modified_typedef(type);
  }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#if LOWER_COMPLEX && GNU_EXTENSIONS_ALLOWED
  if (type_is_typedef(type) &&
      (type->variant.typeref.is_lowered_complex_type ||
       is_complex_type(type)) &&
      type->source_corresp.attributes != NULL) {
    /* When lowering complex types, remove any "mode" attributes on typedefs
       that refer to complex types that are (or will be) lowered by setting
       the attribute type to ak_unrecognized.  This occurs with typedefs like:
         typedef _Complex float __cfloat128 __attribute__((mode(TC)));
       */
    an_attribute_ptr ap;
    for (ap = type->source_corresp.attributes; ap != NULL; ap = ap->next) {
      if (ap->kind == ak_mode) {
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* LOWER_COMPLEX && GNU_EXTENSIONS_ALLOWED */
  if (is_nullptr_type(type)) {
    /* Lower the C23 nullptr_t type to a void* type. */
    type = skip_typerefs(type);
    overwrite_type_with_new_type(type, void_star_type());
  }  /* if */
}  /* lower_c99_type */


static void lower_c99_routine(a_routine_ptr routine)
/*
Do C99 lowering on the indicated routine (the header, not the body).
*/
{
  lower_c99_source_correspondence(&routine->source_corresp);
#if LOWER_VARIABLE_LENGTH_ARRAYS
  if (vla_enabled) {
    record_vla_component_types_for_lowering(routine->type);
  }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#if LOWER_IFUNC
  if (routine->is_ifunc) {
    /* Lower the ifunc routine. */
    lower_ifunc_routine(routine);
  }  /* if */
#endif /* LOWER_IFUNC */
}  /* lower_c99_routine */


static void lower_c99_scope(a_scope_ptr scope)
/*
Do C99 lowering for all entities in and under the given scope.
*/
{
  a_variable_ptr                   variable;
  a_type_ptr                       type;
  a_routine_ptr                    routine;
  a_scope_ptr                      block_scope;
  a_local_static_variable_init_ptr lsvip;
  a_context                        context;
  a_scope_ptr                      saved_innermost_function_scope;

  push_context(&context, scope, (an_object_lifetime_ptr)NULL);
  saved_innermost_function_scope = innermost_function_scope;
  /* Mark the scope as lowered.  This is used by
     check_for_done_with_memory_region to tell whether the code for a function
     has been lowered yet. */
  mark_as_visited(scope);
  switch (scope->kind) {
    case sck_file:
    case sck_block:
      /* Nothing to lower. */
      break;
    case sck_function:
      /* Lower all parameters. */
      innermost_function_scope = scope;
      for (variable = scope->variant.routine.parameters;
           variable != NULL;
           variable = variable->next) {
        lower_c99_variable(variable);
      }  /* for */
      break;
    default:
      unexpected_condition_str("lower_c99_scope: bad scope kind");
  }  /* switch */
  /* Visit all nonstatic variables. */
  for (variable = scope->nonstatic_variables;
       variable != NULL;
       variable = variable->next) {
    lower_c99_variable(variable);
  }  /* for */
  /* Visit all static variables. */
  for (variable = scope->variables;
       variable != NULL;
       variable = variable->next) {
    lower_c99_variable(variable);
  }  /* for */
  /* Visit all types. */
  for (type = scope->types;
       type != NULL;
       type = type->next) {
    lower_c99_type(type);
  }  /* for */
  /* Visit all routines (the headers, not the bodies). */
  for (routine = scope->routines;
       routine != NULL;
       routine = routine->next) {
    lower_c99_routine(routine);
  }  /* for */
  /* Visit all block scopes (only present in function and block scopes). */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    lower_c99_scope(block_scope);
  }  /* for */
  /* Visit all initializers for local static variables. */
  for (lsvip = scope->local_static_variable_inits;
       lsvip != NULL;
       lsvip = lsvip->next) {
    lower_c99_initializer(lsvip->init_kind, &lsvip->initializer);
  }  /* for */
  if (scope->kind == (a_scope_kind)sck_function) {
    /* Lower the function block statement. */
    lower_c99_statement(scope->assoc_block);
    insert_pending_stmk_init_statements(scope->assoc_block);
#if MINIMAL_INLINING
    if (inlining_enabled && scope->variant.routine.ptr->is_inline) {
      /* For an inline routine, set the inlinable flag now that the body has
         been processed. */
      set_up_routine_for_inlining(scope);
    }  /* if */
#endif /* MINIMAL_INLINING */
#if LOWER_VARIABLE_LENGTH_ARRAYS
    if (vla_enabled) {
      /* Lowers the type of any VLA variables in this function scope. */
      lower_vla_variable_types_in_scope(scope);
    }  /* if */
    /* Discard the VLA dimensions list since the VLAs have all been lowered. */
    scope->vla_dimensions = NULL;
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  } else if (scope->kind == (a_scope_kind)sck_file) {
#if LOWER_VARIABLE_LENGTH_ARRAYS
    if (vla_enabled) {
      lower_vla_types();
    }  /* if */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#if MINIMAL_INLINING
    if (inlining_enabled) {
      /* For any inline routines for which all calls were expanded inline,
         mark the routines as being unreferenced. */
      mark_inlined_routines_as_unreferenced();
    }  /* if */
#endif /* MINIMAL_INLINING */
  }  /* if */
  innermost_function_scope = saved_innermost_function_scope;
  pop_context();
}  /* lower_c99_scope */

#if LOWER_COMPLEX

static void lower_c99_imaginary_type(a_float_kind  kind,
                                     a_const_char  *name)
/*
Lower the C99 imaginary type whose precision is given by kind.
The lowered form is a typedef to one of the floating-point types.
The lowered type is given the name indicated by "name".
*/
{
  if (imaginary_type_used_in_primary_IL(kind)) {
    a_type_ptr  im_type = imaginary_type(kind);

    set_type_kind(im_type, (a_type_kind)tk_typeref);
    im_type->variant.typeref.type = float_type(kind);
    im_type->source_corresp.name = alloc_il((sizeof_t)(strlen(name)+1));
    strcpy((char *)im_type->source_corresp.name, name);
    add_to_front_of_file_scope_types_list(im_type);
  }  /* if */
}  /* lower_c99_imaginary_type */


static void lower_c99_complex_type(a_float_kind  kind,
                                   a_const_char  *name)
/*
Lower the C99 complex type whose precision is given by kind (if it was used).
The lowered form is a typedef to a struct containing an array of
two floating-point values of the appropriate kind.
The lowered type is given the name indicated by "name".
*/
{
  if (complex_type_used_in_primary_IL(kind)) {
    a_type_ptr   cmplx_type = complex_type(kind);
    a_type_ptr   lowered_repr = lowered_complex_type(kind);

    /* Typedef the complex type to its lowered representation. */
    set_type_kind(cmplx_type, (a_type_kind)tk_typeref);
    cmplx_type->source_corresp.name = alloc_il((sizeof_t)(strlen(name)+1));
    strcpy((char *)cmplx_type->source_corresp.name, name);
    cmplx_type->variant.typeref.type = lowered_repr;
    cmplx_type->variant.typeref.is_lowered_complex_type = TRUE;
#if MAINTAIN_NEEDED_FLAGS
    if (needed_flag_is_set(&cmplx_type->source_corresp)) {
      mark_as_needed((char *)lowered_repr, iek_type);
      set_class_definition_needed(lowered_repr);
      set_class_keep_definition_in_il(lowered_repr);
    }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
    /* Link the types into the IL (in the right order). */
    add_to_front_of_file_scope_types_list(cmplx_type);
    if (lowered_repr->next == NULL) {
      /* More than one complex type can be mapped to the same lowered
         type, so be sure to add the lowered type only once. */
      add_to_front_of_file_scope_types_list(lowered_repr);
    }  /* if */
  }  /* if */
}  /* lower_c99_complex_type */


void lower_c99_nonreal_float_types(void)
/*
Replace the imaginary and complex C99 types by their lowered representations.
*/
{
  lower_c99_imaginary_type((a_float_kind)fk_float, "_Imaginary_float");
  lower_c99_imaginary_type((a_float_kind)fk_float32x, "_Imaginary_double");
  lower_c99_imaginary_type((a_float_kind)fk_double, "_Imaginary_double");
  lower_c99_imaginary_type((a_float_kind)fk_float64x,
                           "_Imaginary_long_double");
  lower_c99_imaginary_type((a_float_kind)fk_long_double,
                           "_Imaginary_long_double");
  lower_c99_complex_type((a_float_kind)fk_float16, "_Complex_float16");
  lower_c99_complex_type((a_float_kind)fk_std_bfloat16, "_Complex_bfloat16");
  lower_c99_complex_type((a_float_kind)fk_float, "_Complex_float");
  lower_c99_complex_type((a_float_kind)fk_float32x, "_Complex_double");
  lower_c99_complex_type((a_float_kind)fk_double, "_Complex_double");
  lower_c99_complex_type((a_float_kind)fk_float64x, "_Complex_long_double");
  lower_c99_complex_type((a_float_kind)fk_long_double, "_Complex_long_double");
  lower_c99_complex_type((a_float_kind)fk_float80, "_Complex_float80");
  lower_c99_complex_type((a_float_kind)fk_float128, "_Complex_float128");
  lower_c99_complex_type((a_float_kind)fk_std_float16, "_Complex_float16");
  lower_c99_complex_type((a_float_kind)fk_std_float32, "_Complex_float");
  lower_c99_complex_type((a_float_kind)fk_std_float64, "_Complex_double");
  lower_c99_complex_type((a_float_kind)fk_std_float128, "_Complex_float128");
}  /* lower_c99_nonreal_float_types */

#endif /* LOWER_COMPLEX */
#if LOWER_FIXED_POINT

a_type_ptr lowered_integer_type_for_fixed_point_type(a_type_ptr fx_type)
/*
Return the integer type that is the lowered form of the indicated
fixed-point type.
*/
{
  a_byte           ikind;
  a_targ_size_t    int_size;
  a_targ_alignment int_alignment;
  a_type_ptr       int_type;

  fx_type = skip_typerefs(fx_type);
  check_assertion(fx_type->kind == (a_type_kind)tk_fixed_point);
  for (ikind = 0; ; ikind++) {
    check_assertion_str(ikind < (int)ik_last,
            "lowered_integer_type_for_fixed_point_type: no suitable int type");
#if INT128_EXTENSIONS_ALLOWED
    if (ikind == (an_integer_kind)ik_int128 ||
        ikind == (an_integer_kind)ik_unsigned_int128) {
      /* Don't consider the 128-bit integer kinds.  This could be changed if
         needed. */
      continue;
    }  /* if */
#endif /* INT128_EXTENSIONS_ALLOWED */
    get_integer_size_and_alignment((an_integer_kind)ikind, &int_size,
                                   &int_alignment);
    if (int_size == fx_type->size &&
        int_alignment == fx_type->alignment &&
        int_kind_is_signed[(int)ikind] ==
                                   !fx_type->variant.fixed_point.is_unsigned) {
      /* This integral type is okay. */
      break;
    }  /* if */
  }  /* for */
  int_type = integer_type((an_integer_kind)ikind);
  return int_type;
}  /* lowered_integer_type_for_fixed_point_type */


static void lower_c99_fixed_point_type(
                                 a_fixed_point_type_descr descr/*lint !e1746*/)
/*
Lower the C99 fixed-point type whose precision is given by kind.
The lowered form is a typedef to one of the integral types.
*/
{
  if (fixed_point_type_used_in_primary_IL(descr)) {
    a_type_ptr      fx_type = fixed_point_type(descr);
    char            name[30];
    a_type_ptr      int_type;

    /* Develop the name for the typedef. */
    (void)strcpy(name, "_Fixed_point_");
    if (descr.precision == (a_fixed_point_precision)fpp_short) {
      (void)strcat(name, "h");
    } else if (descr.precision == (a_fixed_point_precision)fpp_long) {
      (void)strcat(name, "l");
    }  /* if */
    if (descr.is_unsigned) {
      (void)strcat(name, "u");
    }  /* if */
    if (descr.is_fract_type) {
      (void)strcat(name, "r");
    } else {
      (void)strcat(name, "k");
    }  /* if */
    if (descr.saturating) {
      (void)strcat(name, "_sat");
    }  /* if */
    /* Determine the corresponding integral type (it must have the same
       size, alignment, and signedness). */
    int_type = lowered_integer_type_for_fixed_point_type(fx_type);
    set_type_kind(fx_type, (a_type_kind)tk_typeref);
    fx_type->variant.typeref.type = int_type;
    fx_type->source_corresp.name = alloc_il((sizeof_t)(strlen(name)+1));
    (void)strcpy((char *)fx_type->source_corresp.name, name);
    add_to_front_of_file_scope_types_list(fx_type);
  }  /* if */
}  /* lower_c99_fixed_point_type */


static void lower_c99_fixed_point_types(void)
/*
Replace the fixed-point types by their lowered representations.
*/
{
  a_byte                   precision;
  a_boolean                is_unsigned, is_fract_type, saturating;
  a_fixed_point_type_descr descr;

  for (precision = fpp_short; precision < fpp_last; precision++) {
    descr.precision = (a_fixed_point_precision)precision;
    for (is_unsigned = FALSE;; is_unsigned = TRUE) {
      descr.is_unsigned = is_unsigned;
      for (is_fract_type = FALSE;; is_fract_type = TRUE) {
        descr.is_fract_type = is_fract_type;
        for (saturating = FALSE;; saturating = TRUE) {
          descr.saturating = saturating;
          lower_c99_fixed_point_type(descr);
          if (saturating) break;
        }  /* for */
        if (is_fract_type) break;
      }  /* for */
      if (is_unsigned) break;
    }  /* for */
  }  /* for */
}  /* lower_c99_fixed_point_types */

#endif /* LOWER_FIXED_POINT */

void lower_c99_il_memory_region(a_memory_region_number region_number)
/*
Do C99 lowering for a memory region (for the file scope or a function scope).
*/
{
  a_scope_ptr scope = il_header.region_scope_entry[region_number];
  a_context   context;
  a_scope_ptr saved_innermost_function_scope = innermost_function_scope;
  a_memory_region_number
              saved_region_number = curr_il_region_number;

  il_lowering_underway = TRUE;
  curr_context = NULL;
  innermost_function_scope = NULL;
  curr_object_lifetime = NULL;
  switch_il_region(region_number);
  if (scope->kind == (a_scope_kind)sck_function) {
    /* Push the file scope around lowering of a function scope. */
    push_context(&context, il_header.primary_scope,
                 (an_object_lifetime_ptr)NULL);
  }  /* if */
  lower_c99_scope(scope);
  if (scope->kind == (a_scope_kind)sck_file) {
#if LOWER_COMPLEX
    lower_c99_nonreal_float_types();
#endif /* LOWER_COMPLEX */
#if LOWER_FIXED_POINT
    if (fixed_point_enabled) {
      lower_c99_fixed_point_types();
    }  /* if */
#endif /* LOWER_FIXED_POINT */
    if (nullptr_enabled || gcc_version_is(>=150000)) {
      lower_c99_type(standard_nullptr_type());
    }  /* if */
  }  /* if */
  if (scope->kind == (a_scope_kind)sck_function) {
    pop_context();
  }  /* if */
  innermost_function_scope = saved_innermost_function_scope;
  il_lowering_underway = FALSE;
  switch_il_region(saved_region_number);
}  /* lower_c99_il_memory_region */


void lower_c99_one_time_init(void)
/*
Do one-time initialization of variables related to C99 IL lowering.
*/
{
  /* Save variables from lower_c99.c that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
#if LOWER_COMPLEX
      pch_saved_var_array_elem(lowered_complex_float16),
      pch_saved_var_array_elem(lowered_complex_bfloat16),
      pch_saved_var_array_elem(lowered_complex_float),
      pch_saved_var_array_elem(lowered_complex_double),
      pch_saved_var_array_elem(lowered_complex_long_double),
      pch_saved_var_array_elem(lowered_complex_float80),
      pch_saved_var_array_elem(lowered_complex_float128),
      pch_array_saved_var_array_elem(xnegate_routine),
      pch_array_saved_var_array_elem(xadd_routine),
      pch_array_saved_var_array_elem(xsubtract_routine),
      pch_array_saved_var_array_elem(xmultiply_routine),
      pch_array_saved_var_array_elem(xdivide_routine),
      pch_array_saved_var_array_elem(xeq_routine),
      pch_array_saved_var_array_elem(xne_routine),
#if C99_IL_EXTENSIONS_SUPPORTED
      pch_array_saved_var_array_elem(xconj_routine),
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      pch_array_saved_var_array_elem(rtoc_routine),
      pch_array_saved_var_array_elem(ctor_routine),
      pch_array_saved_var_array_elem(itoc_routine),
      pch_array_saved_var_array_elem(ctoi_routine),
      pch_array_saved_var_array_elem(cast_float16_routine),
      pch_array_saved_var_array_elem(cast_bfloat16_routine),
      pch_array_saved_var_array_elem(cast_float_routine),
      pch_array_saved_var_array_elem(cast_double_routine),
      pch_array_saved_var_array_elem(cast_long_double_routine),
#if FLOAT80_ENABLING_POSSIBLE
      pch_array_saved_var_array_elem(cast_float80_routine),
#endif /* FLOAT80_ENABLING_POSSIBLE */
#if FLOAT128_ENABLING_POSSIBLE
      pch_array_saved_var_array_elem(cast_float128_routine),
#endif /* FLOAT128_ENABLING_POSSIBLE */
#endif /* LOWER_COMPLEX */
#if LOWER_FIXED_POINT
      pch_saved_var_array_elem(fixed_conv_routine),
      pch_array_saved_var_array_elem(float_fixed_conv_routine),
      pch_array_saved_var_array_elem(fixed_float_conv_routine),
      pch_saved_var_array_elem(fixed_negate_routine),
      pch_saved_var_array_elem(fixed_eq_routine),
      pch_saved_var_array_elem(fixed_ne_routine),
      pch_saved_var_array_elem(fixed_gt_routine),
      pch_saved_var_array_elem(fixed_lt_routine),
      pch_saved_var_array_elem(fixed_ge_routine),
      pch_saved_var_array_elem(fixed_le_routine),
      pch_saved_var_array_elem(fixed_add_routine),
      pch_saved_var_array_elem(fixed_subtract_routine),
      pch_saved_var_array_elem(fixed_multiply_routine),
      pch_saved_var_array_elem(fixed_divide_routine),
      pch_saved_var_array_elem(fixed_shiftl_routine),
      pch_saved_var_array_elem(fixed_shiftr_routine),
      pch_saved_var_array_elem(fixed_incr_routine),
      pch_saved_var_array_elem(fixed_decr_routine),
#endif /* LOWER_FIXED_POINT */
#if LOWER_VARIABLE_LENGTH_ARRAYS
      pch_saved_var_array_elem(vla_types),
      pch_saved_var_array_elem(vla_alloc_routine),
      pch_saved_var_array_elem(vla_dealloc_routine),
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
#if LOWER_COMPLEX
  register_trans_unit_array(xnegate_routine);
  register_trans_unit_array(xadd_routine);
  register_trans_unit_array(xsubtract_routine);
  register_trans_unit_array(xmultiply_routine);
  register_trans_unit_array(xdivide_routine);
  register_trans_unit_array(xeq_routine);
  register_trans_unit_array(xne_routine);
#if C99_IL_EXTENSIONS_SUPPORTED
  register_trans_unit_array(xconj_routine);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  register_trans_unit_array(rtoc_routine);
  register_trans_unit_array(ctor_routine);
  register_trans_unit_array(itoc_routine);
  register_trans_unit_array(ctoi_routine);
  register_trans_unit_array(cast_float16_routine);
  register_trans_unit_array(cast_bfloat16_routine);
  register_trans_unit_array(cast_float_routine);
  register_trans_unit_array(cast_double_routine);
  register_trans_unit_array(cast_long_double_routine);
#if FLOAT80_ENABLING_POSSIBLE
  register_trans_unit_array(cast_float80_routine);
#endif /* FLOAT80_ENABLING_POSSIBLE */
#if FLOAT128_ENABLING_POSSIBLE
  register_trans_unit_array(cast_float128_routine);
#endif /* FLOAT128_ENABLING_POSSIBLE */
  register_trans_unit_variable(lowered_complex_float16),
  register_trans_unit_variable(lowered_complex_bfloat16),
  register_trans_unit_variable(lowered_complex_float);
  register_trans_unit_variable(lowered_complex_double);
  register_trans_unit_variable(lowered_complex_long_double);
  register_trans_unit_variable(lowered_complex_float80);
  register_trans_unit_variable(lowered_complex_float128);
#endif /* LOWER_COMPLEX */
#if LOWER_FIXED_POINT
  register_trans_unit_variable(fixed_conv_routine);
  register_trans_unit_variable(fixed_negate_routine);
  register_trans_unit_variable(fixed_eq_routine);
  register_trans_unit_variable(fixed_ne_routine);
  register_trans_unit_variable(fixed_gt_routine);
  register_trans_unit_variable(fixed_lt_routine);
  register_trans_unit_variable(fixed_ge_routine);
  register_trans_unit_variable(fixed_le_routine);
  register_trans_unit_variable(fixed_add_routine);
  register_trans_unit_variable(fixed_subtract_routine);
  register_trans_unit_variable(fixed_multiply_routine);
  register_trans_unit_variable(fixed_divide_routine);
  register_trans_unit_variable(fixed_shiftl_routine);
  register_trans_unit_variable(fixed_shiftr_routine);
  register_trans_unit_variable(fixed_incr_routine);
  register_trans_unit_variable(fixed_decr_routine);
  register_trans_unit_array(float_fixed_conv_routine);
  register_trans_unit_array(fixed_float_conv_routine);
#endif /* LOWER_FIXED_POINT */
#if LOWER_VARIABLE_LENGTH_ARRAYS
  register_trans_unit_variable(vla_types);
  register_trans_unit_variable(vla_dealloc_routine);
  register_trans_unit_variable(vla_alloc_routine);
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  register_trans_unit_variable(pending_stmk_init_statements);
#if MINIMAL_INLINING
  /* Do inline.c initialization. */
  if (inlining_enabled) inline_one_time_init();
#endif /* MINIMAL_INLINING */
}  /* lower_c99_one_time_init */


void lower_c99_trans_unit_init(void)
/*
Initialize static variables related to C99 IL that must be initialized
for each translation unit.
*/
{
#if LOWER_COMPLEX
  { int k;
    for (k = 0; k < NUM_COMPLEX_FLOAT_KINDS; ++k) {
      xnegate_routine[k] = NULL;
      xadd_routine[k] = NULL;
      xsubtract_routine[k] = NULL;
      xmultiply_routine[k] = NULL;
      xdivide_routine[k] = NULL;
      xeq_routine[k] = NULL;
      xne_routine[k] = NULL;
#if C99_IL_EXTENSIONS_SUPPORTED
      xconj_routine[k] = NULL;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      rtoc_routine[k] = NULL;
      ctor_routine[k] = NULL;
      itoc_routine[k] = NULL;
      ctoi_routine[k] = NULL;
      cast_float16_routine[k] = NULL;
      cast_bfloat16_routine[k] = NULL;
      cast_float_routine[k] = NULL;
      cast_double_routine[k] = NULL;
      cast_long_double_routine[k] = NULL;
#if FLOAT80_ENABLING_POSSIBLE
      cast_float80_routine[k] = NULL;
#endif /* FLOAT80_ENABLING_POSSIBLE */
#if FLOAT128_ENABLING_POSSIBLE
      cast_float128_routine[k] = NULL;
#endif /* FLOAT128_ENABLING_POSSIBLE */
    }  /* for */
  }
  lowered_complex_float16 = NULL;
  lowered_complex_bfloat16 = NULL;
  lowered_complex_float = NULL;
  lowered_complex_double = NULL;
  lowered_complex_long_double = NULL;
  lowered_complex_float80 = NULL;
  lowered_complex_float128 = NULL;
#endif /* LOWER_COMPLEX */
#if LOWER_FIXED_POINT
  fixed_conv_routine = NULL;
  fixed_negate_routine = NULL;
  fixed_eq_routine = NULL;
  fixed_ne_routine = NULL;
  fixed_gt_routine = NULL;
  fixed_lt_routine = NULL;
  fixed_ge_routine = NULL;
  fixed_le_routine = NULL;
  fixed_add_routine = NULL;
  fixed_subtract_routine = NULL;
  fixed_multiply_routine = NULL;
  fixed_divide_routine = NULL;
  fixed_shiftl_routine = NULL;
  fixed_shiftr_routine = NULL;
  fixed_incr_routine = NULL;
  fixed_decr_routine = NULL;
  { int k;
    for (k = 0; k < NUM_COMPLEX_FLOAT_KINDS; ++k) {
      float_fixed_conv_routine[k] = NULL;
      fixed_float_conv_routine[k] = NULL;
    }  /* for */
  }
#endif /* LOWER_FIXED_POINT */
#if LOWER_VARIABLE_LENGTH_ARRAYS
  vla_types = NULL;
  vla_dealloc_routine = NULL;
  vla_alloc_routine = NULL;
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  pending_stmk_init_statements = NULL;
#if MINIMAL_INLINING
  /* Do inline.c initialization. */
  if (inlining_enabled) inline_init();
#endif /* MINIMAL_INLINING */
  /* The following is also cleared in il_lower_init, but clear it here also
     to be sure. */
  il_lowering_underway = FALSE;
}  /* lower_c99_trans_unit_init */


void lower_c99_init(void)
/*
Initialize static variables related to C99 IL lowering that must be
initialized for each compilation.
*/
{
  code_pos_for_lowering = null_source_position;
#if LOWER_VARIABLE_LENGTH_ARRAYS
  /* The code to lower C VLAs assumes that the deallocation points have been
     marked using enk_vla_dealloc expression nodes. */
  check_assertion(vla_deallocations_in_il || !C_mode());
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
}  /* lower_c99_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* DO_IL_LOWERING */

