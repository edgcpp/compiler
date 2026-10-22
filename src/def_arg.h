/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

def_arg.h -- Declarations related to def_arg.c (having to do with
	     default argument processing).

*/

/* Avoid including these declarations more than once: */
#ifndef DEF_ARG_H
#define DEF_ARG_H 1

#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* ifndef LANG_FEAT_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* Note: a_def_arg_expr_fixup_ptr is defined in symbol_tbl.h. */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Structure for keeping track of token cache representing a default argument
expression, prescanned during a member function declaration within a class
definition and actually processed once the class definition is complete.
The declaration for a_def_arg_expr_fixup_ptr is in symbol_tbl.h.
*/
typedef struct a_def_arg_expr_fixup {
  a_def_arg_expr_fixup_ptr
		next;
			/* Next in a linked list of entries representing
			   default argument expressions for the parameters
			   of a given function. */
  a_template_cache
		cache;
			/* A pointer to the template cache that describes the
			   default argument expression tokens, and the
			   template declaration information if the tokens
			   are part of a template. */
  a_param_type_ptr
		param_type;
			/* A pointer to the param type entry in which the
			   expression node is to be stored once its tokens
			   have been scanned. */
  unsigned long	param_number;
			/* The position of associated parameter in the
			   parameter list. */
} a_def_arg_expr_fixup;

extern
void prescan_default_argument(a_token_cache_ptr	token_cache,
                              a_boolean		is_template_param,
                              a_boolean		is_function_template,
                              a_boolean		is_friend_decl,
                              a_boolean		is_expression = TRUE);

extern
void prescan_default_function_arg_expr(
			a_param_type_ptr		ptp,
		        a_def_arg_expr_fixup_ptr	*list,
			a_boolean			is_function_template,
			a_boolean			is_friend_decl,
			unsigned long			param_number);

extern void delayed_scan_of_default_arg_expr(
                                           a_param_type_ptr param_type_entry,
                                           a_symbol_ptr     rout_sym,
                                           a_boolean        check_for_errors);

extern void delayed_scan_of_template_default_arg_expr(a_type_ptr     type,
					              a_constant_ptr constant);

extern a_type_ptr delayed_scan_of_template_default_type_arg(void);

extern a_template_ptr delayed_scan_of_template_default_template_arg(
				a_template_ptr		param_template,
				a_boolean		dependent_default,
				a_source_position	*err_pos);

extern void free_def_arg_expr_fixup(a_def_arg_expr_fixup_ptr  daefp);

extern a_def_arg_expr_fixup_ptr copy_def_arg_expr_fixup_list(
				a_def_arg_expr_fixup_ptr	orig_list);

extern void def_arg_one_time_init(void);

extern void def_arg_init(void);

#if DEBUG
/* GCOVR_EXCL_START */
extern unsigned long db_show_def_arg_expr_fixups_used(
                                                   unsigned long  grand_total);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* DEF_ARG_H */

