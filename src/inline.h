/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

inline.h -- Declarations related to inline.c (minimal inlining for IL
            lowering).

*/

/* Avoid including these declarations more than once: */
#ifndef INLINE_H
#define INLINE_H 1

/* Only include this code if it is needed: */
#if DO_IL_LOWERING
#if MINIMAL_INLINING

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_DEF_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


/*
Entry used to record information about the remapping to be done on a variable
while expanding an inline function call.
*/
enum a_variable_remapping_kind {
  vrk_none,		/* No remapping. */
  vrk_temporary,	/* Variable is remapped to a temporary variable. */
  vrk_constant_expr	/* Variable is remapped to a constant-valued
			   expression. */
};
typedef struct a_variable_remapping_for_inlining
                                        *a_variable_remapping_for_inlining_ptr;
typedef struct a_variable_remapping_for_inlining {
  a_variable_remapping_for_inlining_ptr
		next;	/* Used to link entries on a list. */
  a_variable_ptr
		orig_variable;
			/* The original variable, i.e. the one being
			   rewritten. */
  a_variable_remapping_kind
		kind;
			/* Kind of remapping. */
  union {
    /* When kind == vrk_none, no variant fields. */
    /* When kind == vrk_temporary: */
    a_variable_ptr
		variable;
    /* When kind == vrk_constant_expr: */
    an_expr_node_ptr
		expr;
  } variant;
  an_expr_node_ptr
		arg_expr;
			/* Used when remapping parameters to hold the argument
			   expression.  For vrk_temporary remappings, this
			   expression is used as the initial value for the
			   temporary.  NULL for remappings that aren't
			   parameters. */
  an_expr_node_ptr
		arg_side_effect_expr;
			/* If non-NULL, points to an expression that needs
			   to be executed before the inlined call.  Used in
			   the case where an argument with side-effects is
			   passed to an unused parameter.  Also used when
			   a (presumably lowering-generated) comma operation
			   is passed as an argument and the second operand of
			   the comma operation is constant-valued, but the
			   first operand has side-effects. */
  a_variable_ptr
		orig_temporary;
			/* If non-NULL, points to a temporary that was the
			   original remapping indicated in this entry, which
			   has been superseded temporarily. */
  a_byte_boolean
		temporary_used;
			/* TRUE if this has been used as a vrk_temporary
			   remapping. */
  a_byte_boolean
		remapping_used;
			/* TRUE if this remapping has been used.  That prevents
			   certain optimizations that involve changing the
			   remapping. */
  a_byte_boolean
		local_temporary_okay;
			/* TRUE if this remapping is to a variable that
			   could be freed as a reusable local temporary. */
  a_byte_boolean
		local_temporary_reused;
			/* TRUE if this remapping reuses a previously-allocated
			   reusable temporary variable. */
} a_variable_remapping_for_inlining;


EXTERN_THREAD a_variable_remapping_for_inlining_ptr
		avail_variable_remappings_for_inlining;
			/* List of remapping entries that have been freed
			    and are available for reuse. */

#if DEBUG
/* GCOVR_EXCL_START */
/*
Count of entries allocated, for debugging purposes.
*/
EXTERN_THREAD unsigned long
		num_variable_remappings_for_inlining;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */


extern a_variable_ptr remap_var_for_inlining(a_variable_ptr var);

extern void adjust_copied_expression_for_inlining(
                                            an_expr_node_ptr expr,
                                            a_boolean        *inlining_failed);

extern a_boolean copy_and_simplify_short_circuited_operation(
                                            an_expr_node_ptr expr,
                                            a_boolean        *inlining_failed);

extern void do_inlining_of_call(an_expr_node_ptr expr,
                                a_statement_ptr  statement,
                                a_boolean        *expr_has_been_detached);

extern void set_up_routine_for_inlining(a_scope_ptr scope);

extern void mark_inlined_routines_as_unreferenced(void);

extern void inline_one_time_init(void);

extern void inline_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* MINIMAL_INLINING */
#endif /* DO_IL_LOWERING */
#endif /* ifndef INLINE_H */

