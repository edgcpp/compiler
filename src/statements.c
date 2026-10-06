/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

statements.c -- Scanning of statements.

*/


/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "decls.h"
#include "decl_spec.h"
#include "disambig.h"
#include "expr.h"
#include "exprutil.h"
#include "folding.h"
#include "interpret.h"
#include "pch.h"
#include "pragma.h"
#include "statements.h"
#include "macro.h"
#include "func_def.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

STATIC_THREAD a_struct_stmt_stack_entry_ptr
		struct_stmt_stack_container;
			/* A dynamically allocated array of structured
			   statement stack entries that can accommodate
			   the coexistence of more than one stack.  When a
                           stack is currently active and a new stack is
                           required (for member function definitions of
			   local classes, for instance) an unused segment of
			   the container is employed; later the inactive
			   stack can be reactivated.  Because the container
			   is dynamically allocated, it can be expanded if
			   necessary.  size_struct_stmt_stack_container
			   gives the number of elements currently allocated.
			   Allocation is not per-file. */
STATIC_THREAD sizeof_t
		size_struct_stmt_stack_container;
			/* Size of struct_stmt_stack_container, in terms of
			   the number of elements. */
#define STRUCT_STMT_STACK_INCREMENTAL_ALLOCATION 30
			/* The number of elements added to struct_stmt_stack
			   each time it is reallocated; also the initial
			   allocation. */

STATIC_THREAD a_reachability_summary
		curr_reachability;
			/* Indicates whether or not the current location in
			   the code (following the last statement of the top
			   structured statement on the statement stack) is
			   reachable by flowing into it from the previous
			   statement. */

STATIC_THREAD a_control_flow_descr_ptr
		control_flow_descr_list;
			/* A linked list that represents that part of the
			   static control flow pattern of a given function
			   that is relevant to detecting transfers of control
			   that bypass declarations with explicit or implicit
			   initializers.  The list represents labels, gotos,
			   blocks, and initializing declarations.  Note that
			   the list is dynamically pruned -- it has only
			   enough information on it for the checking that is
			   required.  For instance, once a block is closed,
			   it may be removed from the list entirely if it is
			   not relevant to subsequent analysis.  Similarly,
			   once a forward goto has been checked, it is no
			   longer interesting and is removed.  Some of the
			   entries on the list point to IL entries, but the
			   IL does not point back.  It is for front-end use
			   only. */
STATIC_THREAD a_control_flow_descr_ptr
		end_of_control_flow_descr_list;
			/* Pointer to the tail of control_flow_descr_list;
			   NULL only when control_flow_descr_list itself is
			   NULL. */
STATIC_THREAD a_control_flow_descr_ptr
		avail_control_flow_descrs;
			/* Linked list of a_control_flow_descr entries that
			   have been freed for reuse. */

#define function_scope_object_lifetime                                \
  (scope_stack[depth_innermost_function_scope].curr_scope_object_lifetime)

#if UPC_EXTENSIONS_ALLOWED
STATIC_THREAD a_statement_ptr
		affinity_forall_loop;
			/* The enclosing UPC forall loop with an affinity.
			   NULL if there is no such loop. */

STATIC_THREAD a_statement_ptr
		innermost_forall_loop;
			/* The innermost enclosing UPC forall loop (with or
			   without an affinity).  NULL if there is no such
			   loop. */
#endif /* UPC_EXTENSIONS_ALLOWED */

#if DEBUG
/*
Counts of tables allocated, to track total use of memory.
*/
STATIC_THREAD unsigned long
		num_control_flow_descrs_allocated;
#endif /* DEBUG */

/*
Set var to indicate that the associated code is reachable.
*/
#define set_reachable(var)                                            \
{ (var).reachable = TRUE;                                             \
  (var).reachable_considering_hints = TRUE;                           \
  (var).suppress_unreachable_warning = FALSE;                         \
}  /* set_reachable */

/*
Set var to indicate that the associated code is unreachable.
*/
#define set_unreachable(var)                                          \
{ (var).reachable = FALSE;                                            \
  (var).reachable_considering_hints = FALSE;                          \
  (var).suppress_unreachable_warning = FALSE;                         \
}  /* set_unreachable */


/*
Declarations needed because of forward references:
*/
static void statement(a_boolean is_dependent_statement,
                      a_boolean marked_as_gnu_extension);

static void empty_statement(a_boolean compiler_generated);


static void check_lint_notreached_state(void)
/*
Check for a lint-notreached comment on the pending pragma list for the current
statement, and if one is found update the curr_reachability state so as to
suppress warnings that might otherwise be issued later.
*/
{
  /* Determine whether a lint notreached comment immediately preceded this
     statement.  (Note that we don't need to pass a statement pointer to
     extract_specific_pragmas since no IL entry is generated for lint
     notreached comments.) */
  a_pending_pragma_list ppl = extract_specific_pragmas(
                                        pk_lint_notreached, (a_symbol_ptr)NULL,
                                        (a_statement_ptr)NULL,
                                        /*curr_scope_only=*/FALSE);

  if (!ppl.is_empty()) {
    /* There is a currently active notreached comment. */
    curr_reachability.reachable_considering_hints = FALSE;
    curr_reachability.suppress_unreachable_warning = TRUE;
  }  /* if */
}  /* check_lint_notreached_state */


static void check_reachability_following_expression(an_expr_node_ptr  node)
/*
If the indicated expression node represents a throw or calls a function
that may not return, update the current "reachability" to indicate that the
code directly following the expression is (or may be) unreachable.
*/
{
  if (node->kind == (an_expr_node_kind)enk_object_lifetime) {
    node = node->variant.object_lifetime.expr;
  }  /* if */
  node = skip_parens(node);
  while (is_operation_node(node) && node_operator_is(node, eok_cast) &&
         is_void_type(node->type)) {
    /* Explicit cast to void -- ignore it for this test. */
    node = skip_parens(node->variant.operation.operands);
  }  /* while */
  if (node->kind == (an_expr_node_kind)enk_throw) {
    /* A throw expression. */
    /* This could be much fancier and could check for things like
         x ? throw a : throw b
         (throw c, y)
       but it doesn't seem worth it. */
    set_unreachable(curr_reachability);
  } else if (node_is(node, enk_temp_init)) {
    a_dynamic_init_ptr dip = node->variant.init.dynamic_init;
    a_boolean          does_not_return = FALSE;
    if (!dip) return;
    if (dip->destructor && routine_does_not_return(dip->destructor)) {
      does_not_return = TRUE;
    }  /* if */
    if (dyn_init_is(dip, dik_constructor)) {
      a_routine_ptr rp = dip->variant.constructor.ptr;
      if (rp && routine_does_not_return(rp)) {
        does_not_return = TRUE;
      }  /* if */
    }  /* if */
    if (does_not_return) {
      /* The statement is invoking a constructor or destructor that
         is marked as not returning.  Treat this like a lint notreached
         comment -- i.e., as a hint to the compiler but not something we
         know for sure. */
      curr_reachability.reachable_considering_hints = FALSE;
      curr_reachability.suppress_unreachable_warning = TRUE;
    }
  } else if (is_call_node(node)) {
    a_boolean   call_does_not_return = FALSE;
    a_type_ptr  routine_type;
    node = node->variant.operation.operands;
    routine_type = node->type;
    if (is_pointer_type(routine_type)) {
      /* Possibly a call through a function pointer. */
      routine_type = type_pointed_to(routine_type);
    }  /* if */
    if (is_function_type(routine_type)) {
      call_does_not_return = skip_typerefs(routine_type)
        ->variant.routine.extra_info
        ->does_not_return;
    }  /* if */
    if (call_does_not_return) {
      /* The statement is a call of a routine that is marked as not
         returning.  Treat this like a lint notreached comment -- i.e.,
         as a hint to the compiler but not something we know for sure. */
      curr_reachability.reachable_considering_hints = FALSE;
      curr_reachability.suppress_unreachable_warning = TRUE;
    }  /* if */
  }  /* if */
}  /* check_reachability_following_expression */


static void merge_reachability(a_reachability_summary *reachability,
                               a_reachability_summary *merged_reachability)
/*
Merge the reachability information from "reachability" into
"merged_reachability".
*/
{
  merged_reachability->reachable |= reachability->reachable;
  merged_reachability->reachable_considering_hints |= 
                                    reachability->reachable_considering_hints;
  merged_reachability->suppress_unreachable_warning |=
                                    reachability->suppress_unreachable_warning;
}  /* merge_reachability */


a_boolean at_end_of_statement_expression(void)
/*
We are normally at a semicolon terminating an expression statement in a GNU
statement expression.  Return TRUE if, ignoring attributes and semicolons, the
next token is a right brace.  In GNU C++ mode, empty statements are not skipped
(i.e., we return TRUE only if one or zero semicolons precede a right brace).
*/
{
  a_boolean      result = FALSE, semicolon_seen = FALSE;
  a_token_cache  cache;

  for (;;) {
    switch (curr_token) {
      case tok_rbrace:
        result = TRUE;
        goto done;
      case tok_semicolon:
        if (semicolon_seen && gpp_version_is(any_version)) {
          goto done;
        }  /* if */
        semicolon_seen = TRUE;
        cache_curr_token(&cache);
        (void)get_token();
        break;
      case tok_lbracket:
        if (!std_attribute_tokens_next()) goto done;
        FALLTHROUGH
      case tok_attribute:
        cache_attributes(&cache);
        break;
      default:
        goto done;
    }  /* switch */
  }  /* for */
done:
  rescan_cached_tokens(&cache);
  return result;
}  /* at_end_of_statement_expression */


a_boolean inside_statement_expression(void)
/*
Return TRUE if we are currently inside a GNU statement expression,
i.e., ({ ... }).
*/
{
  a_boolean inside_se = (depth_stmt_stack != NO_SCOPE_DEPTH &&
                         struct_stmt_stack[depth_stmt_stack].
                                                        inside_statement_expr);
  return inside_se;
}  /* inside_statement_expression */


static void statement_not_allowed_inside_statement_expression(
                                                    a_source_position *err_pos)
/*
We're scanning a statement that is not allowed inside a GNU statement
expression (i.e., ({ ... }) ).  If we are indeed inside a statement
expression, issue an error at the indicated source position.
*/
{
  if (inside_statement_expression()) {
    pos_error(ec_bad_statement_in_statement_expr, err_pos);
  }  /* if */
}  /* statement_not_allowed_inside_statement_expression */

#if DEBUG

static void db_cfd(a_control_flow_descr_ptr cfdp)
/*
Routine to display an entry of type a_control_flow_descr, for debugging
purposes.
*/
{
  a_statement_ptr  sp;
  a_label_ptr      label;

  switch (cfdp->kind) {
    case cfdk_block:
      fprintf(f_debug, "block (#%lu, line %lu)", cfdp->id_number,
              (unsigned long)cfdp->source_pos.seq);
      if (cfdp->variant.block.is_catch_block) {
        fprintf(f_debug, ", catch");
      } else if (cfdp->variant.block.is_try_block) {
        fprintf(f_debug, ", try");
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (cfdp->variant.block.is_finally_block) {
        fprintf(f_debug, ", finally");
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else if (cfdp->variant.block.is_statement_expr) {
        fprintf(f_debug, ", statement expr");
      } else if (cfdp->variant.block.is_within_goto_protected_block) {
        fprintf(f_debug, ", inside goto protected block");
      }  /* if */
      if (cfdp->variant.block.is_switch_block) {
        fprintf(f_debug, ", switch");
      } else if (cfdp->variant.block.is_switch_subblock) {
        fprintf(f_debug, ", inside switch");
      }  /* if */
      if (cfdp->variant.block.any_labels) fprintf(f_debug, ", labels");
      if (cfdp->variant.block.goto_count > 0) {
        fprintf (f_debug, ", %lu goto%s",
                 cfdp->variant.block.goto_count,
                 cfdp->variant.block.goto_count == 1 ? "" : "s");
      }  /* if */
      if (cfdp->variant.block.last_case_label != NULL) {
        fprintf(f_debug, ", last case label #%lu",
                cfdp->variant.block.last_case_label->id_number);
      }  /* if */
      if (cfdp->variant.block.end_of_block != NULL) {
        fprintf(f_debug, ", EOB #%lu",
                cfdp->variant.block.end_of_block->id_number);
      }  /* if */
      break;
    case cfdk_goto:
      label = cfdp->variant.goto_statement.ptr->variant.label.ptr;
      if (label->continue_label) {
        fputs("continue", f_debug);
      } else if (label->switch_break_label) {
        fputs("switch break", f_debug);
      } else if (label->break_label) {
        fputs("break", f_debug);
      } else {
        fprintf(f_debug, "goto %s", label->source_corresp.name);
      }  /* if */
      fprintf(f_debug, " (#%lu, line %lu)", cfdp->id_number,
              (unsigned long)cfdp->source_pos.seq);
      break;
    case cfdk_label:
      label = cfdp->variant.label_statement->variant.label.ptr;
      if (label->continue_label) {
        fputs("continue label", f_debug);
      } else if (label->switch_break_label) {
        fputs("switch break label", f_debug);
      } else if (label->break_label) {
        fputs("break label", f_debug);
      } else {
        fprintf(f_debug, "label \"%s\"", label->source_corresp.name);
      }  /* if */
      fprintf(f_debug, " (#%lu, line %lu)", cfdp->id_number,
              (unsigned long)cfdp->source_pos.seq);
      break;
    case cfdk_init:
      sp = cfdp->variant.init.statement;
      if (sp == NULL ||
          sp->kind == (a_statement_kind)stmk_init ||
          (C_mode() && microsoft_mode &&
           sp->kind == (a_statement_kind)stmk_block)) {
        a_variable_ptr  vp = cfdp->variant.init.variable;
        fprintf(f_debug, "initialization");
        if (vp != NULL) {
          fputs(" of \"", f_debug);
          db_name(&vp->source_corresp);
          fputc('"', f_debug);
        }  /* if */
      } else if (sp->kind == (a_statement_kind)stmk_set_vla_size) {
        a_vla_dimension_ptr  vdp = sp->variant.vla_dimension;
        fprintf(f_debug, "VLA declaration");
        if (vdp != NULL) {
          fputs(": \"", f_debug);
          db_type(vdp->type);
          fputc('"', f_debug);
        }  /* if */
      } else if (sp->kind == (a_statement_kind)stmk_vla_decl) {
        fprintf(f_debug, "VLA declaration: ");
        if (sp->variant.vla.is_typedef_decl) {
          db_type(sp->variant.vla.variant.typedef_type);
        } else {
          db_variable(sp->variant.vla.variant.variable);
        }  /* if */
      } else {
        fprintf(f_debug, "***BAD STMT KIND***");
      }  /* if */
      fprintf(f_debug, " (#%lu, line %lu)", cfdp->id_number,
              (unsigned long)cfdp->source_pos.seq);
      break;
    case cfdk_end_of_block:
      fprintf(f_debug, "EOB (#%lu, line %lu)", cfdp->id_number,
              (unsigned long)cfdp->source_pos.seq);
      if (cfdp->variant.start_of_block != NULL) {
        fprintf(f_debug, " for block #%lu",
                cfdp->variant.start_of_block->id_number);
      }  /* if */
      break;
    case cfdk_case_label:
      fprintf(f_debug, "case label (#%lu, line %lu)", cfdp->id_number,
              (unsigned long)cfdp->source_pos.seq);
      break;
    default:
      fprintf(f_debug, "***UNKNOWN KIND***");
  }  /* switch */
  if (cfdp->parent != NULL) {
    fprintf(f_debug, ", parent #%lu", cfdp->parent->id_number);
  }  /* if */
  fputc('\n', f_debug);
}  /* db_cfd */


static void db_cfd_list(a_control_flow_descr_ptr cfdp,
                        int                      back,
                        int                      forward)
/*
Routine to display a sublist of linked list of entries of type
a_control_flow_descr, for debugging purposes.  cfdp is a pointer to some
entry on the list; back and forward represent the number of entries
preceding and following cfdp that should be displayed.
*/
{
  int                     count;
  an_object_lifetime_ptr  olp;
  a_boolean               is_provisional;

  if (cfdp != NULL) {  
    for (count = 0; count < back; ++count) {
      if (cfdp->prev == NULL) break;
      cfdp = cfdp->prev;
    }  /* if */
    count = count + forward;
    for (; count >= 0 && cfdp != NULL; --count, cfdp = cfdp->next) {
      fputs("  ", f_debug);
      db_cfd(cfdp);
      if (!C_mode()) {
        olp = NULL;
        is_provisional = FALSE;
        if (cfdp->kind == (a_control_flow_descr_kind)cfdk_label) {
          olp = cfdp->variant.label_statement->variant.label.lifetime;
        } else if (cfdp->kind == (a_control_flow_descr_kind)cfdk_goto) {
          olp = cfdp->variant.goto_statement.ptr->variant.label.lifetime;
          is_provisional = (cfdp->variant.goto_statement.ptr->
                               variant.label.ptr->exec_stmt == NULL);
        } else if (cfdp->kind == (a_control_flow_descr_kind)cfdk_block) {
          olp = cfdp->variant.block.object_lifetime;
        } else {
          continue;
        }  /* if */
        fprintf(f_debug, "    %slifetime = %s",
                         is_provisional ? "provisional " : "",
                         olp == NULL ? "<null>" : "");
        if (olp != NULL) db_object_lifetime_name(olp);
        fputc('\n', f_debug);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* db_cfd_list */


static void db_cfd_and_parents(a_control_flow_descr_ptr cfdp)
/*
Routine to display an entry of type a_control_flow_descr along with its
parent entries (i.e., the blocks which contain it), for debugging purposes.
*/
{
  if (cfdp != NULL) {
    db_cfd(cfdp);
    while ((cfdp = cfdp->parent) != NULL) {
      fprintf(f_debug, "  with parent: ");
      db_cfd(cfdp);
    }  /* while */
  }  /* if */
}  /* db_cfd_and_parents */


static void db_cfd_with_indentation(a_control_flow_descr_ptr  cfdp)
/*
Display a control flow description in a special format, for use when
dump_control_flow has been enabled at the command line.  The display line
includes the current sequence number, indentation corresponding to the depth
of parent block, and the control flow entry itself.
*/
{
  a_control_flow_descr_ptr  parent = cfdp->parent;

  fprintf(f_debug, "CF-%.4d    ", (int)pos_curr_token.seq);
  for (; parent != NULL; parent = parent->parent) {
    fputs("  ", f_debug);
  }  /* for */
  db_cfd(cfdp);
}  /* db_cfd_with_indentation */


static void db_ssse_with_indentation(a_struct_stmt_kind  kind,
                                     a_const_char        *str)
/*
Display a structured statement stack entry in a special format, for use when
dump_control_flow has been enabled at the command line.
*/
{
  fprintf(f_debug, "SS-%.4d    %*.10s", (int)pos_curr_token.seq,
          (int)strlen(str)+2*depth_stmt_stack, str);
  switch (kind) {
    case ssk_compound:   str = "compound";   break;
    case ssk_if:         str = "if";         break;
    case ssk_constexpr_if: str = "constexpr if"; break;
    case ssk_switch:     str = "switch";     break;
    case ssk_while:      str = "while";      break;
    case ssk_do:         str = "do";         break;
    case ssk_for:        str = "for";        break;
    case ssk_range_based_for: str = "range-based-for"; break;
    case ssk_try_block:  str = "try_block";  break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case ssk_for_each:   str = "for each";   break;
    case ssk_microsoft_try:  str = "microsoft_try";  break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default:             str = "<unknown>"; break;
  }  /* switch */
  fprintf(f_debug, "ssk_%s\n", str);
}  /* db_ssse_with_indentation */

#endif /* DEBUG */

#if DEBUG
STATIC_THREAD unsigned long
		cfd_id_number;
			/* Identifier number used in control flow debug
			   output. */
#endif /* DEBUG */

static a_control_flow_descr_ptr alloc_control_flow_descr(
                                               a_control_flow_descr_kind kind)
/*
Allocate a control-flow descriptor of the specified kind (or reuse one from
the available list), set its fields to default values, and return a pointer
to it.
*/
{
  a_control_flow_descr_ptr  cfdp;

  db_enter(5, "alloc_control_flow_descr");
  if (avail_control_flow_descrs != NULL) {
    /* Reuse a previously freed entry. */
    cfdp = avail_control_flow_descrs;
    avail_control_flow_descrs = avail_control_flow_descrs->next;
  } else {
    /* Allocate a new entry. */
    cfdp = (a_control_flow_descr_ptr)alloc_fe(sizeof(a_control_flow_descr));
#if DEBUG
    num_control_flow_descrs_allocated++;
#endif /* DEBUG */
  }  /* if */
  /* Set the entry's fields to default values. */
  cfdp->next = NULL;
  cfdp->prev = NULL;
  cfdp->parent = NULL;
  cfdp->kind = kind;
  cfdp->source_pos = error_position;
#if DEBUG
  cfdp->id_number = ++cfd_id_number;
#endif /* DEBUG */
#if UPC_EXTENSIONS_ALLOWED
  cfdp->enclosing_forall = NULL;
#endif /* UPC_EXTENSIONS_ALLOWED */
  switch (kind) {
    case cfdk_block:
      cfdp->variant.block.end_of_block = NULL;
      cfdp->variant.block.last_case_label = NULL;
      cfdp->variant.block.object_lifetime = NULL;
      cfdp->variant.block.goto_count = 0;
      cfdp->variant.block.any_labels = FALSE;
      cfdp->variant.block.any_vla_variables = FALSE;
      cfdp->variant.block.is_switch_block = FALSE;
      cfdp->variant.block.is_switch_subblock = FALSE;
      cfdp->variant.block.exposed_init_in_switch = FALSE;
      cfdp->variant.block.is_catch_block = FALSE;
      cfdp->variant.block.is_try_block = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      cfdp->variant.block.is_finally_block = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      cfdp->variant.block.is_statement_expr = FALSE;
      cfdp->variant.block.is_within_goto_protected_block = FALSE;
      cfdp->variant.block.is_constexpr_if = FALSE;
      cfdp->variant.block.is_if_consteval_branch = FALSE;
      break;
    case cfdk_init:
      cfdp->variant.init.statement = NULL;
      cfdp->variant.init.variable = NULL;
      cfdp->variant.init.is_vla_variable = FALSE;
      cfdp->variant.init.in_statement_expression = FALSE;
      break;
    case cfdk_goto:
      cfdp->variant.goto_statement.ptr = NULL;
      cfdp->variant.goto_statement.prev_goto = NULL;
      break;
    case cfdk_label:
      cfdp->variant.label_statement = NULL;
      break;
    case cfdk_end_of_block:
      cfdp->variant.start_of_block = NULL;
      break;
    case cfdk_case_label:
      break;
    default:
      unexpected_condition_str("alloc_control_flow_descr: bad kind");
  }  /* switch */
  db_exit();
  return cfdp;
}  /* alloc_control_flow_descr */


static void free_control_flow_descr(a_control_flow_descr_ptr cfdp)
/*
Return an entry of type a_control_flow_descr to the available list for reuse.
It will already have been removed from any other lists.  Note that the
available list does not make use of the prev pointer, which (like all fields
except "next" of entries on this list) is likely to be invalid.
*/
{
  cfdp->next = avail_control_flow_descrs;
  avail_control_flow_descrs = cfdp;
}  /* free_control_flow_descr */


static void remove_list_of_control_flow_descrs(a_control_flow_descr_ptr  head,
                                               a_control_flow_descr_ptr  tail)
/*
Remove the list of control flow descriptors headed by head and terminated by
tail from control_flow_descr_list and add it onto the available list.  It
may be a sublist of the larger list, so link around it (both next and prev
pointers) and move the list as a whole to the available list.
*/
{
  db_enter(5, "remove_list_of_control_flow_descrs");
  if (head != NULL) {
#if DEBUG
    if (debug_level >= 5) {
      a_control_flow_descr_ptr cfdp = head;
      fprintf(f_debug, "Removing entire list:\n");
      for (;;) {
        fprintf(f_debug, "  ");
        check_assertion(cfdp != NULL);
        db_cfd(cfdp);
        if (cfdp == tail) break;
        cfdp = cfdp->next;
        if (cfdp == NULL) {
          if (tail != NULL) {
            fprintf(f_debug, "  ***TAIL NOT FOUND*** tail = ");
            db_cfd(tail);
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
#endif /* DEBUG */
    /* Reset the next pointer of the entry on the list that precedes head, or
       if there is no preceding entry reset the list head pointer. */
    if (head->prev == NULL) {
      check_assertion(head == control_flow_descr_list);
      control_flow_descr_list = tail->next;
    } else {
      head->prev->next = tail->next;
    }  /* if */
    /* Reset the prev pointer of tail's successor on the list, or if there is
       no successor entry reset the list tail pointer. */
    if (tail->next == NULL) {
      check_assertion(tail == end_of_control_flow_descr_list);
      end_of_control_flow_descr_list = head->prev;
    } else {
      tail->next->prev = head->prev;
    }  /* if */
    tail->next = avail_control_flow_descrs;
    avail_control_flow_descrs = head;
  }  /* if */
  db_exit();
}  /* remove_list_of_control_flow_descrs */


static void remove_control_flow_descr(a_control_flow_descr_ptr  cfdp)
/*
Remove cfdp from the control_flow_descr_list and put it on the available list.
If a goto is removed, the goto-counts of its parent, grandparent, and so
forth, are decremented.
*/
{
  db_enter(5, "remove_control_flow_descr");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Removing: ");
    db_cfd(cfdp);
  }  /* if */
#endif /* DEBUG */
  /* Reset the next pointer of the entry on the list that precedes cfdp, or
     if there is no preceding entry reset the list head pointer. */
  if (cfdp->prev == NULL) {
    check_assertion(cfdp == control_flow_descr_list);
    control_flow_descr_list = cfdp->next;
  } else {
    cfdp->prev->next = cfdp->next;
  }  /* if */
  /* Reset the prev pointer of cfdp's successor on the list, or if there is no
     successor entry reset the list tail pointer. */
  if (cfdp->next == NULL) {
    check_assertion(cfdp == end_of_control_flow_descr_list);
    end_of_control_flow_descr_list = cfdp->prev;
  } else {
    cfdp->next->prev = cfdp->prev;
  }  /* if */
  check_assertion(cfdp->kind != (a_control_flow_descr_kind)cfdk_goto);
  free_control_flow_descr(cfdp);
  db_exit();
}  /* remove_control_flow_descr */


static a_boolean is_on_cfd_parent_list(a_control_flow_descr_ptr cfdp,
                                       a_control_flow_descr_ptr cfdp2)
/*
Return TRUE if cfdp (a block entry) is on the list of parent blocks of
cfdp2. */
{
  a_control_flow_descr_ptr  parent;
  a_boolean                 on_list = FALSE;

  for (parent = cfdp2->parent; parent != NULL; parent = parent->parent) {
    if (cfdp == parent) {
      on_list = TRUE;
      break;
    }  /* if */
  }  /* for */
  return on_list;
}  /* is_on_cfd_parent_list */


static a_boolean check_for_branch_into_goto_protected_block(
                                      a_control_flow_descr_ptr  label_cfdp,
                                      a_control_flow_descr_ptr  goto_cfdp)
/*
Check for an attempt to branch into a try block, a catch clause (an
exception handler), a constexpr if (C++17), an "if consteval"/"if not
consteval" statement, a C++/CLI finally clause, or a GNU statement expression.
Either label_cfdp points to a label entry and goto_cfdp to a goto entry, or
else label_cfdp points to a case label entry and goto_cfdp is NULL (in which
case we need to find the switch with which the case label is associated).  If
an error is found, issue the diagnostic and return TRUE.
*/
{
  a_boolean                 err = FALSE;
  a_control_flow_descr_ptr  cfdp;

  db_enter(4, "check_for_branch_into_goto_protected_block");
  cfdp = label_cfdp->parent;
  if (cfdp->variant.block.is_within_goto_protected_block) {
    /* The label is inside a statement that cannot be branched into. */
    while (!cfdp->variant.block.is_catch_block &&
           !cfdp->variant.block.is_try_block &&
#if MICROSOFT_EXTENSIONS_ALLOWED
           !cfdp->variant.block.is_finally_block &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
           !cfdp->variant.block.is_constexpr_if &&
           !cfdp->variant.block.is_if_consteval_branch &&
           !cfdp->variant.block.is_statement_expr) {
      cfdp = cfdp->parent;
      check_assertion(cfdp != NULL);
    }  /* while */
    if (goto_cfdp == NULL) {
      /* This must be a branch to a case label -- there's no explicit goto
         statement. */
      check_assertion(label_cfdp->kind ==
                             (a_control_flow_descr_kind)cfdk_case_label);
      /* Find the innermost enclosing switch block -- it's the block in which
         the implicit goto occurs. */
      goto_cfdp = label_cfdp->parent;
      while (!goto_cfdp->variant.block.is_switch_block) {
        goto_cfdp = goto_cfdp->parent;
        check_assertion(goto_cfdp != NULL);
      }  /* for */
    }  /* if */
    if (is_on_cfd_parent_list(cfdp, goto_cfdp)) {
      /* The catch or try block is a parent (or grandparent, etc.) of the
         block where the goto occurs.  A local branch within a single
         catch clause or try block is allowed. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_bugs && microsoft_version <= 1200 &&
               cfdp->variant.block.is_try_block) {
      /* Just a warning for branching into a try-block in some Microsoft bugs
         modes. */
      pos_warning(ec_branch_into_try_block, &goto_cfdp->source_pos);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      /* It's a branch into a protected block from outside. */
      an_error_code err_code = ec_no_error;
      if (cfdp->variant.block.is_catch_block) {
        err_code = ec_branch_into_handler;
      } else if (cfdp->variant.block.is_try_block) {
        err_code = ec_branch_into_try_block;
      } else if (cfdp->variant.block.is_statement_expr) {
        err_code = ec_branch_into_statement_expr;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (cfdp->variant.block.is_finally_block) {
        err_code = ec_branch_into_finally;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else if (cfdp->variant.block.is_constexpr_if) {
        err_code = ec_branch_into_constexpr_if;
      } else if (cfdp->variant.block.is_if_consteval_branch) {
        err_code = ec_branch_into_if_consteval;
      } else {
        unexpected_condition_str(
             "check_for_branch_into_goto_protected_block: unknown block kind");
      }  /* if */
      pos_error(err_code, &goto_cfdp->source_pos);
      err = TRUE;
      if (current_routine_entry()->is_constexpr) {
        /* Disable the interpretation of a function that might include an
           invalid branch. */
        scope_stack[depth_innermost_function_scope].constexpr_ruled_out = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return err;
}  /* check_for_branch_into_goto_protected_block */


static void report_switch_past_init(a_control_flow_descr_ptr  block,
                                    a_diagnostic_ptr          *prev_dp,
                                    an_error_severity         *prev_severity)
/*
This routine traverses the portion of the control_flow_descr_list associated
with "block", which is a switch block or a block contained within a switch
block, and looks for initializing declarations that may be bypassed by a
transfer of control to a case label.  Once the last case label in the
block has been reached, the search stops, since any subsequent initialization
cannot be jumped over (at least, not by the switch).  When an initialization
is found, a diagnostic is issued (an error in C++, a warning otherwise), and
*err is set to TRUE.  If *prev_severity is es_none, no diagnostic is in the
process of being generated.  If it not es_none, *prev_dp is the diagnostic
that is being generated.
*/
{
  a_control_flow_descr_ptr  cfdp, next_cfdp = NULL, parent;
  a_variable_ptr            vp;
  a_boolean                 done;
  an_error_severity         severity;
  a_type_ptr                tp;
  a_statement_ptr           sp;


  db_enter(4, "report_switch_past_init");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "block = ");
    db_cfd(block);
  }  /* if */
#endif /* DEBUG */
  /* Start at the first entry within the block. */
  cfdp = block->next;
  done = FALSE;
  for (;;) {
    switch (cfdp->kind) {
      case cfdk_block:
        /* A nested block.  If it has any case labels (either directly
           contained or in a subblock) search for initializations. */
        next_cfdp = cfdp->variant.block.end_of_block->next;
        if (cfdp->variant.block.last_case_label != NULL) {
          report_switch_past_init(cfdp, prev_dp, prev_severity);
          /* All case labels will have been removed.  Is there any reason to
             keep this block around? */
          check_assertion(cfdp->variant.block.last_case_label == NULL);
          if (!cfdp->variant.block.any_labels &&
              cfdp->variant.block.goto_count == 0) {
            /* A block with no labels and no forward gotos. */
            remove_list_of_control_flow_descrs(cfdp, cfdp->variant.
                                                       block.end_of_block);
          }  /* if */
          /* Processing the subblock may mean the current block does not
             need to be searched any more.  For example:
               switch (i) {
                 case 1:
                   {                // start of subblock
                   int i = 0;       // diagnostic issued
                   case 2:
                   }                // end of subblock
                   int j = 0;       // no diagnostic (since decl is not
               }                    //   followed by another case label)
             In this example, the outer block is originally marked as having
             "case 2" as its last case label (even though it is contained
             within a subblock), but when case 2 is found, it is removed and
             the last_case_label fields of both the inner and outer block are
             set to NULL. */
          done = (block->variant.block.last_case_label == NULL);
        }  /* if */
        break;
      case cfdk_case_label:
        /* A case label. */
        if (cfdp == block->variant.block.last_case_label) {
          /* Moreover, the last case label in the current block.  No further
             checking in this block is required, so done is set to TRUE. */
          done = TRUE;
          /* Now that the case label has been seen (it's really just serving
             as a marker to tell us to stop searching for initializations),
             it can be removed from the list.  Therefore the last_case_label
             pointer in the current block should be set to NULL, as should
             the pointers to this case label in the parent chain. */
          block->variant.block.last_case_label = NULL;
          if (!block->variant.block.is_switch_block) {
            for (parent = block->parent; ; parent = parent->parent) {
              if (cfdp == parent->variant.block.last_case_label) {
                parent->variant.block.last_case_label = NULL;
                if (parent->variant.block.is_switch_block) break;
              } else {
                break;
              }  /* if */
            }  /* for */
          }  /* if */
        } else {
          /* It's not the last case label in the block, so we keep searching,
             but the case label can still be removed. */
          next_cfdp = cfdp->next;
        }  /* if */
        remove_control_flow_descr(cfdp);
        break;
      case cfdk_end_of_block:
        /* End of block.  Only under rare circumstances should we get all
           the way to the end of the block before stopping. */
        done = TRUE;
        break;
      case cfdk_init:
        /* An initialization or (when support for VLAs is enabled) a VLA
           declaration.  Issue a diagnostic for automatic variables;
           initializations for which a diagnostic should not be issued will
           not be found, since we stop searching the block once its last case
           label has been seen. */
        sp = cfdp->variant.init.statement;
        vp = cfdp->variant.init.variable;
        severity = es_none;
        if (vp != NULL && !cfdp->variant.init.is_vla_variable) {
          if (!var_has_static_or_thread_storage_duration(vp)) {
            severity = es_warning;
            if (!C_mode()) {
              if (current_routine_entry()->is_constexpr) {
                /* Uninitialized variables cannot be permitted in constexpr
                   functions. */
                severity = es_error;
              } else if (!cfront_2_1_mode) {
                a_boolean  has_nontrivial_dtor = FALSE;
                tp = vp->type;
                if (is_array_type(tp)) tp = underlying_array_element_type(tp);
                tp = skip_typerefs(tp);
                if (is_immediate_class_type(tp)) {
                  a_class_symbol_supplement_ptr  cssp =
                                              symbol_supplement_for_class(tp);
                  has_nontrivial_dtor = has_nontrivial_destructor(cssp);
                }  /* if */
                if (has_nontrivial_dtor) {
                  severity = es_error;
                } else if (strict_ansi_mode) {
                  severity = strict_ansi_error_severity;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
        } else {
          if (sp->kind == (a_statement_kind)stmk_vla_decl) {
            if (!sp->variant.vla.is_typedef_decl) {
              vp = sp->variant.vla.variant.variable;
            }  /* if */
          } else {
            check_assertion(sp->kind == (a_statement_kind)stmk_set_vla_size);
          }  /* if */
          severity = es_error;
        }  /* if */
        if (severity != es_none) {
          if (severity != *prev_severity) {
            if (*prev_severity != es_none) end_diagnostic(*prev_dp);
            /* This is the first initializing declaration seen.  Issue the
               header diagnostic. */
            /* We need the switch block itself for the error position. */
            parent = cfdp->parent;
            while (parent->variant.block.is_switch_subblock) {
              parent = parent->parent;
            }  /* while */
            check_assertion(parent->variant.block.is_switch_block);
            /* Issue a warning in C mode or for compatibility with cfront 2.1.
               Otherwise, issue an error. */
            *prev_dp = pos_start_diagnostic(severity,
                                            ec_branch_past_initialization,
                                            &parent->source_pos);
            *prev_severity = severity;
          }  /* if */
          if (vp != NULL) {
            /* Issue the diagnostic addendum that identifies this particular
               variable. */
            if (vp->is_anonymous_parent_object) {
              add_diag_info_with_pos_insert(*prev_dp,
                                            ec_anon_union_at_decl_position,
                                            &vp->source_corresp.decl_position);
            } else {
              sym_add_diag_info(*prev_dp,
                                (sp != NULL &&
                                 sp->kind == (a_statement_kind)stmk_vla_decl) ?
                                    ec_vla_name_at_decl_position :
                                    ec_name_at_decl_position,
                                (a_symbol_ptr)vp->source_corresp.assoc_info);
            }  /* if */
          } else {	
            /* Diagnostic addendum that identifies the VLA declaration. */
            add_diag_info_with_pos_insert(*prev_dp, ec_vla_at_decl_pos,
                                          &sp->position);
          }  /* if */
        }  /* if */
        FALLTHROUGH
      default:
        /* Advance to the next entry in the list. */
        next_cfdp = cfdp->next;
    }  /* switch */
    if (done) break;
    cfdp = next_cfdp;
  }  /* for */
  db_exit();
}  /* report_switch_past_init */


static void promote_label_and_goto_lifetimes(
                                        a_control_flow_descr_ptr  block_cfdp,
                                        an_object_lifetime_ptr    promote_from,
                                        an_object_lifetime_ptr    promote_to)
/*
Loop through the entries in the list headed by block_cfdp, and for each
goto and label statement with a lifetime matching promote_from, change it
to point to the lifetime promote_to.
*/
{
  a_control_flow_descr_ptr  cfdp;
  a_statement_ptr           sp;

  /*lint --e{850} cfdp modified in loop */
  for (cfdp = block_cfdp->next; cfdp != NULL; cfdp = cfdp->next) {
    switch (cfdp->kind) {
      case cfdk_goto:
        /* Pull out the goto statement pointer. */
        sp = cfdp->variant.goto_statement.ptr;
        break;
      case cfdk_label:
        /* Pull out the label statement pointer. */
        sp = cfdp->variant.label_statement;
        break;
      case cfdk_block:
        if (cfdp->variant.block.goto_count > 0 ||
            cfdp->variant.block.any_labels) {
          /* Find the gotos and labels in the nested block and promote
             their object lifetime pointers, too. */
        } else {
          /* Skip over the nested block. */
          cfdp = cfdp->variant.block.end_of_block;
        }  /* if */
        FALLTHROUGH
      default:
        continue;
    }  /* switch */
    /* If the object lifetime pointer in the goto or label statement matches
       promote_from, change it to refer to promote_to. */
    if (sp->variant.label.lifetime == promote_from) {
      sp->variant.label.lifetime = promote_to;
    }  /* if */
  }  /* for */
}  /* promote_label_and_goto_lifetimes */


static void fixup_curr_block_labels_and_gotos(
                                         a_control_flow_descr_ptr  block_cfdp)
/*
block_dfdp points to a control flow description that represents a block that
is about to be terminated.  That means the lifetime associated with it (along
with indirectly associated block-after-label lifetimes) will be popped from
the object lifetime stack.  If any goto or label statement points to an object
lifetime entry that is useless (that will not be retained in the IL), then
the pointer must be "promoted" to refer to a lifetime that is still on the
stack.  (Successive poppings of the structured statement stack may result in
successive promotions of the lifetime associated with an inner-block label or
goto.)  Here's an example:
  void f()
  {                     // function scope
    {                   // block scope #1
      {                 // block scope #2
        goto L;
      }
  L:;
    }
  }
When block scope #2 terminates, if there were no destructible objects declared
in it, the lifetime for the goto statement is promoted to that of block scope
#1.  Then, when block scope #1 terminates, the lifetimes of both the goto and
the label are promoted to the lifetime of the function scope.
*/
{
  an_object_lifetime_ptr  block_olp, promote_from = NULL, promote_to;
  a_boolean               keep_block_object_lifetime;

  db_enter(4, "fixup_curr_block_labels_and_gotos");
  if (!block_cfdp->variant.block.is_switch_block) {
    block_olp = block_cfdp->variant.block.object_lifetime;
    check_assertion(block_olp != NULL &&
                    block_olp->kind == (an_object_lifetime_kind)olk_block);
    if (block_cfdp->parent == NULL) {
      /* block_cfdp must represent the function scope.  Don't try to promote
         its lifetime. */
      keep_block_object_lifetime = TRUE;
    } else if (block_olp->destructions != NULL) {
      /* Don't promote the lifetime of an inner block if it has destructions
         associated with it. */
      keep_block_object_lifetime = TRUE;
    } else if (block_cfdp->variant.block.is_catch_block
#if MICROSOFT_EXTENSIONS_ALLOWED
               || block_cfdp->variant.block.is_finally_block
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                            ) {
      /* The lifetime of a try block, catch clause or finally clause is
         retained in the IL, even if it has no destructions. */
      keep_block_object_lifetime = TRUE;
    } else {
      keep_block_object_lifetime = FALSE;
    }  /* if */
    /* Loop through any olk_block_after_label lifetimes that may belong to the
       block that is being terminated. */
    while (curr_object_lifetime != block_olp) {
      check_assertion(curr_object_lifetime->kind ==
                              (an_object_lifetime_kind)olk_block_after_label);
      promote_from = curr_object_lifetime;
      promote_to = curr_object_lifetime->parent_lifetime;
      /* Pop the block-after-label object lifetime.  If, after it's popped,
         it's no longer in the IL, we'll need to promote pointers that
         reference it. */
      if (pop_object_lifetime()) {
        /* Popping the object lifetime did not result in its being removed
           from the IL, so labels and gotos that reference it don't need to
           have their pointers updated.  This means the block lifetime will
           be retained. */
        keep_block_object_lifetime = TRUE;
      } else if (block_cfdp->variant.block.goto_count != 0 ||
                 block_cfdp->variant.block.any_labels) {
        /* Promote the label and goto lifetime pointers. */
        promote_label_and_goto_lifetimes(block_cfdp, promote_from, promote_to);
      }  /* if */
      check_assertion(promote_to == curr_object_lifetime);
    }  /* while */
    /* At this point all the promotions have been done for the subblocks
       created by label declarations.  Now do the top-level lifetime of the
       block -- if required. */
    if (block_cfdp->variant.block.goto_count != 0 ||
        block_cfdp->variant.block.any_labels) {
      block_olp->block_lifetime_with_label_or_goto = TRUE;
      if (block_olp->parent_lifetime->kind ==
                                (an_object_lifetime_kind)olk_expr_temporary) {
        /* We can get here with GNU statement expressions.  However, we do not
           want to promote a lifetime for a goto or label statement to be a
           child of an olk_expr_temporary lifetime since that would complicate
           things elsewhere. */
        keep_block_object_lifetime = TRUE;
      }  /* if */
      if (!keep_block_object_lifetime &&
          is_useless_object_lifetime(block_olp)) {
        promote_to = block_olp->parent_lifetime;
        check_assertion_str2(
                    promote_to->kind == (an_object_lifetime_kind)olk_block ||
                    promote_to->kind ==
                            (an_object_lifetime_kind)olk_block_after_label ||
                    promote_to->kind == (an_object_lifetime_kind)olk_try_block,
                    "fixup_curr_block_labels_and_gotos:",
                    "bad parent of curr block lifetime");
        promote_label_and_goto_lifetimes(block_cfdp, block_olp, promote_to);
        /* Null out the lifetime pointer in the block control flow entry.
           "NULL" means that the lifetimes of any labels or statements within
           are still subject to further promotion. */
        block_cfdp->variant.block.object_lifetime = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* fixup_curr_block_labels_and_gotos */


static void remove_unneeded_set_vla_size_control_flow_entries(
                                           a_control_flow_descr_ptr  cfdp)
/*
Examine the control flow entries immediately preceding the indicated entry,
which should point to a vla-decl statement.  Each preceding entry that is
associated with a set-vla-size statement belonging to the same declaration
as the vla-decl statement can be deleted.  For example,
    void f(int n) {
      int a[n];
      typedef int T[n];
      T b;
    }
The statements generated for this declaration are:
    set-vla-size
    vla-decl
    set-vla-size
    vla-decl
and for each a cfdk_init control-flow entry is generated.  The first two
statements belong to the declaration of a; the third and fourth to the
declarations of T and b, respectively.  The control-flow entry pointing at
the first set-vla-size statement is removed, but the one pointing at the
second is not.  The reason for removing superfluous entries is to avoid
issuing redundant errors for branching around the declarations.  (The
set-vla-size statements are generated in declarator, which doesn't know what
the context of the array declarator is; the simplest approach is just to put
them out and then remove them if they prove superfluous.)
*/
{
  a_statement_ptr           sp, prev_sp;
  a_control_flow_descr_ptr  prev;

  sp = cfdp->variant.init.statement;
  check_assertion(sp != NULL && sp->kind == (a_statement_kind)stmk_vla_decl);
  /* A multidimensional array may have more than one variable dimension, and
     so more than one set-vla-size statement.  Therefore the checking is done
     inside a loop. */
  for (;;) {
    prev = cfdp->prev;
    if (prev != NULL && prev->kind == (a_control_flow_descr_kind)cfdk_init) {
      prev_sp = prev->variant.init.statement;
      if (prev_sp != NULL &&
          prev_sp->kind == (a_statement_kind)stmk_set_vla_size) {
        /* The preceding entry is indeed a cfdk_init that points to a
           set-vla-size statement. */
        a_type_ptr           tp = sp->variant.vla.variant.variable->type;
        a_vla_dimension_ptr  vdp = prev_sp->variant.vla_dimension;
        a_boolean            match = FALSE;

        /* Look at each dimension of the variable length array, looking for
           a match with the set-vla-size statement. */
        while (is_array_type(tp) &&
               !(tp->kind == (a_type_kind)tk_typeref &&
                 typeref_is_typedef(tp))) {
          a_type_ptr  unqualified_tp = skip_typerefs(tp);
          if (same_entities(vdp->type, unqualified_tp)) {
            match = TRUE;
            break;
          }  /* if */
          /* No match -- advance to the array element, which may itself be
             an array. */
          tp = array_element_type(tp);
        }  /* while */
        if (match) {
          /* Remove the superfluous control-flow entry. */
          remove_control_flow_descr(prev);
          /* Continue the loop, in case there's another dimension to
             check. */
          continue;
        }  /* if */
      }  /* if */
    }  /* if */
    /* Assume one pass, unless there was a match and prev was removed from
       the control flow list. */
    break;
  }  /* for */
}  /* remove_unneeded_set_vla_size_control_flow_entries */


static void add_to_control_flow_descr_list(a_control_flow_descr_ptr  new_cfdp)
/*
Add new_cfdp to the end of control_flow_descr_list.  This typically involves
setting its prev and parent pointers (its next pointer will be NULL), and
setting end_of_control_flow_descr_list to point to it.  If it is a goto or
label entry, its addition may produce changes to fields of parent (and
grandparent, etc.) entries.  In some cases it will not be added to the list
at all and will even cause other entries to be removed -- for instance, if
appending an end-of-block entry will result in an empty block, or if the
completed block would have no labels or gotos, the block can be eliminated,
because such blocks are not relevant to detecting transfer of control past
initializing declarations.
*/
{
  a_control_flow_descr_ptr  cfdp, prev_cfdp, prev_parent, parent;

  db_enter(4, "add_to_control_flow_descr_list");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Candidate to add to list: ");
    db_cfd(new_cfdp);
  }  /* if */
#endif /* DEBUG */
  if (control_flow_descr_list == NULL) {
    /* Add this entry to the start of the list. */
    control_flow_descr_list = new_cfdp;
  } else {
    prev_parent = end_of_control_flow_descr_list->parent;
    if (new_cfdp->kind == (a_control_flow_descr_kind)cfdk_end_of_block) {
      if (end_of_control_flow_descr_list->kind ==
                                   (a_control_flow_descr_kind)cfdk_block) {
        if (!C_mode()) {
          /* If there are any block-after-label object lifetimes (which can
             happen if this is inside a switch statement) they need to be
             popped off the object lifetime stack. */
          fixup_curr_block_labels_and_gotos(end_of_control_flow_descr_list);
        }  /* if */
        /* No need to create an empty block. */
        remove_control_flow_descr(end_of_control_flow_descr_list);
        free_control_flow_descr(new_cfdp);
        goto done;
      }  /* if */
      if (!C_mode()) {
        /* Fix up the object lifetime pointers for labels and gotos, if
           necessary.  Note: this function is called even when there are no
           gotos and labels to worry about, since there may be
           block-after-label lifetimes to pop off the object lifetime stack. */
        fixup_curr_block_labels_and_gotos(prev_parent);
      }  /* if */
      if (!prev_parent->variant.block.any_labels &&
          prev_parent->variant.block.last_case_label == NULL &&
          prev_parent->variant.block.goto_count == 0) {
        /* A block with no labels and no forward gotos is being closed.  It
           can be removed from the list -- even if it has initializations,
           it can't be jumped into. */
        remove_list_of_control_flow_descrs(prev_parent,
                                           end_of_control_flow_descr_list);
        free_control_flow_descr(new_cfdp);
        goto done;
      }  /* if */
      /* No initialization remains "exposed" after the block is closed. */
      prev_parent->variant.block.exposed_init_in_switch = FALSE;
      /* Set the association between the end-of-block and the block -- they
         each point to the other. */
      new_cfdp->variant.start_of_block = prev_parent;
      prev_parent->variant.block.end_of_block = new_cfdp;
      /* The parent of an end-of-block entry is the same as the parent of the
         block entry it's associated with. */
      new_cfdp->parent = prev_parent->parent;
      /* We have reached the end-of-block entry for a switch block.  Traverse
         the block looking for illegal initializations -- there should be one
         if any case labels were entered, since that occurs only if "exposed"
         initializations exist (that is, initializations that can be jumped
         over when the switch is executed). */
      if (new_cfdp->variant.start_of_block->variant.block.is_switch_block &&
          new_cfdp->variant.start_of_block->
                                      variant.block.last_case_label != NULL) {
        an_error_severity  severity = es_none;
        a_diagnostic_ptr   dp = NULL;

        /* Check for and report switch-over errors. */
        report_switch_past_init(new_cfdp->variant.start_of_block, &dp,
                                &severity);
        /* Unless the initializations were of static variables only, there
           will have been at least one diagnostic. */
        if (dp != NULL) end_diagnostic(dp);
      }  /* if */
      /* Remove all init entries in the block that trail the last label or
         case label in the block; if there is no label or case label *all*
         the init entries will be removed. */
      for (cfdp = end_of_control_flow_descr_list;
           cfdp != new_cfdp->variant.start_of_block;
           cfdp = prev_cfdp) {
        if (cfdp->kind == (a_control_flow_descr_kind)cfdk_label ||
            cfdp->kind == (a_control_flow_descr_kind)cfdk_case_label ||
            (cfdp->kind == (a_control_flow_descr_kind)cfdk_block &&
             (cfdp->variant.block.any_labels ||
              cfdp->variant.block.last_case_label != NULL))) {
          /* A label, a case label, or a block containing one or the other. */
          break;
        } else {
          prev_cfdp = cfdp->prev;
          if (cfdp->kind == (a_control_flow_descr_kind)cfdk_init) {
            if (cfdp->variant.init.is_vla_variable &&
                cfdp->parent->variant.block.goto_count != 0) {
              /* Leave it on the list, in case there is a forward goto that
                 needs fixup. */
            } else {
              remove_control_flow_descr(cfdp);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
    } else {
      /* This is not an end-of-block entry.  Determine its parent. */      
      if (end_of_control_flow_descr_list->kind ==
                                   (a_control_flow_descr_kind)cfdk_block) {
        /* Immediate successors of a block entry have that block as a
           parent. */
        parent = end_of_control_flow_descr_list;
      } else {
        /* Immediate successors of a nonblock have the same parent as the
           entry they follow. */
        parent = prev_parent;
      }  /* if */
      new_cfdp->parent = parent;
      switch (new_cfdp->kind) {
        case cfdk_init:
          /* Initialization entries in the outermost block (the function scope)
             can simply be ignored when no forward goto has been seen. */
          if (parent->parent == NULL &&
              parent->variant.block.goto_count == 0) {
            if (new_cfdp->variant.init.is_vla_variable) {
              /* This is the declaration of a VLA object.  Leave it on the
                 list to signal the need for deallocation later. */
            } else {
              free_control_flow_descr(new_cfdp);
              goto done;
            }  /* if */
          }  /* if */
          if (new_cfdp->variant.init.is_vla_variable) {
            parent->variant.block.any_vla_variables = TRUE;
          }  /* if */
          /* If the initializing declaration appears within the body of a
             switch statement, set a flag in the current block to say that
             there is an "exposed initialization" -- i.e., one that could
             cause an error if case selection skips past it. */
          parent->variant.block.exposed_init_in_switch = TRUE;
          break;
        case cfdk_label:
          /* Set the any_labels flag of the parent of a new label entry (and
             of the parent's parent, etc.). */
          cfdp = parent;
          do {
            if (cfdp->variant.block.any_labels) {
              /* The flag will already have been set further up the parent
                 chain. */
              break;
            } else {
              cfdp->variant.block.any_labels = TRUE;
              cfdp = cfdp->parent;
            }  /* if */
          } while (cfdp != NULL);
          break;
        case cfdk_case_label:
          if (check_for_branch_into_goto_protected_block(
                                  new_cfdp, (a_control_flow_descr_ptr)NULL)) {
            /* Case label is within a handler or try block and the switch
               statement with which it is associated is outside.  The error
               has already been issued. */
            free_control_flow_descr(new_cfdp);
            goto done;
          }  /* if */
          if (!parent->variant.block.exposed_init_in_switch) {
            /* There is no initializing declaration that would be jumped over
               to reach this case label, so don't bother putting it on the
               list.  This is done for reasons of economy -- the more trimmed
               the list, the easier it is to search. */
            free_control_flow_descr(new_cfdp);
            goto done;
          }  /* if */
          /* Record the fact that a case label has been entered on each of
             the blocks on the parent chain, up to the switch block itself. */
          for (cfdp = parent; ; cfdp = cfdp->parent) {
            check_assertion(cfdp != NULL &&
                            (cfdp->variant.block.is_switch_block ||
                             cfdp->variant.block.is_switch_subblock));
            cfdp->variant.block.exposed_init_in_switch = FALSE;
            cfdp->variant.block.last_case_label = new_cfdp;
            if (cfdp->variant.block.is_switch_block) break;
          }  /* for */
          break;
        case cfdk_goto:
          /* Increment the goto_count field of the parent of a new goto entry
             (and of the parent's parent, etc.). */
          cfdp = parent;
          do {
            ++(cfdp->variant.block.goto_count);
            cfdp = cfdp->parent;
          } while (cfdp != NULL);
          break;
        case cfdk_block:
          if (!new_cfdp->variant.block.is_switch_block) {
            if (parent->variant.block.is_switch_block ||
                parent->variant.block.is_switch_subblock) {
              new_cfdp->variant.block.is_switch_subblock = TRUE;
              new_cfdp->variant.block.exposed_init_in_switch =
                          parent->variant.block.exposed_init_in_switch;
            }  /* if */
          }  /* if */
          if (parent->variant.block.is_within_goto_protected_block) {
            new_cfdp->variant.block.is_within_goto_protected_block = TRUE;
          }  /* if */
          break;
        default:
          unexpected_condition();
      }  /* switch */
    }  /* if */
#if DEBUG
    if (debug_level >= 5) {
      fprintf(f_debug, "Adding:  ");
      db_cfd_and_parents(new_cfdp);
    }  /* if */
#endif /* DEBUG */
    /* Actually append it to the list. */
    end_of_control_flow_descr_list->next = new_cfdp;
    new_cfdp->prev = end_of_control_flow_descr_list;
  }  /* if */
  /* Set the tail pointer to point to the new entry. */
  end_of_control_flow_descr_list = new_cfdp;
  /* If the current entry represents a vla-decl statement, remove any
     entries representing set-vla-size statements that belong to the same
     declaration. */
  if (vla_enabled &&
      new_cfdp->kind == (a_control_flow_descr_kind)cfdk_init &&
      new_cfdp->variant.init.statement != NULL &&
      new_cfdp->variant.init.statement->kind ==
                                      (a_statement_kind)stmk_vla_decl) {
    remove_unneeded_set_vla_size_control_flow_entries(new_cfdp);
  }  /* if */
#if DEBUG
  if (db_flag_is_set("dump_control_flow")) {
    db_cfd_with_indentation(new_cfdp);
  }  /* if */
#endif /* DEBUG */
done:;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Tail of control_flow_descr_list:\n");
    db_cfd_list(end_of_control_flow_descr_list,10,0);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* add_to_control_flow_descr_list */


/*
Return TRUE if this block statement is bound to an object lifetime and
must therefore be a cfront dependent statement (a dependent statement for
which no scope is created).  Note: this test is only reliable
while the corresponding structured statement is on the structured statement
stack -- i.e., between calls of push_stmt_stack and pop_stmt_stack.
*/
#define block_stmt_is_cfront_dependent_stmt(sp)                        \
  ((sp)->variant.block.extra_info->lifetime != NULL)


static void add_statement_list(a_statement_ptr  sp,
                               a_boolean        reachable)
/*
Link the given list of statements onto the end of the current statement
sequence.  If the first statement of the given list is reachable, reachable
should be set to TRUE.
*/
{
  a_boolean                     is_list = (sp->next != NULL);
  a_struct_stmt_stack_entry_ptr sssep;
  a_statement_ptr               ssp;
  a_statement_ptr               *head_ptr = NULL;
  a_boolean                     statement_list_allowed;
  a_statement_ptr               temp_stmt;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_boolean                     in_guarded_statement_of_microsoft_try = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_enter(4, "add_statement_list");
  /* Find the header pointer for the statement list for the current
     structured statement. */
#if CHECKING
  if (depth_stmt_stack < 0) {
    internal_error("add_statement_list: struct_stmt_stack is empty");
  }  /* if */
#endif /* CHECKING */
  sssep = &struct_stmt_stack_top();
  statement_list_allowed = FALSE;
  if (sssep->extra_block != NULL) {
    /* An extra block statement has already been added under the primary
       statement.  The instruction should be added under this extra block. */
    ssp = sssep->extra_block;
    head_ptr = &ssp->variant.block.statements;
    statement_list_allowed = TRUE;
  } else {
    ssp = sssep->statement;
    switch(ssp->kind) {
      case stmk_if:
      case stmk_if_consteval:
      case stmk_if_not_consteval:
        if (sssep->in_else_of_if) {
          head_ptr = &ssp->variant.if_stmt.else_statement;
        } else {
          head_ptr = &ssp->variant.if_stmt.then_statement;
        }  /* if */
        break;
      case stmk_constexpr_if:
        if (sssep->in_else_of_if) {
          head_ptr = &ssp->variant.constexpr_if->else_statement;
        } else {
          head_ptr = &ssp->variant.constexpr_if->then_statement;
        }  /* if */
        break;
      case stmk_while:
      case stmk_end_test_while:
        head_ptr = &ssp->variant.loop_statement;
        break;
#if UPC_EXTENSIONS_ALLOWED
      /* Handle UPC forall like for. */
      case stmk_upc_forall:
#endif /* UPC_EXTENSIONS_ALLOWED */
      case stmk_for:
        if (sssep->for_init) {
          /* This "for" statement may end up being a range-based "for"
             statement, in which case this initializer statement will be
             moved to the proper variant. */
          head_ptr = &ssp->variant.for_loop.extra_info->initialization;
        } else {
          head_ptr = &ssp->variant.for_loop.statement;
        }  /* if */
        break;
      case stmk_range_based_for:
        check_assertion(!sssep->for_init);
        head_ptr = &ssp->variant.range_based_for_loop.statement;
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case stmk_for_each:
        head_ptr = &ssp->variant.for_each_loop.statement;
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case stmk_switch:
        head_ptr = &ssp->variant.switch_stmt.body_statement;
        break;
      case stmk_block:
        head_ptr = &ssp->variant.block.statements;
        statement_list_allowed = TRUE;
        break;
      case stmk_try_block:
        head_ptr = &ssp->variant.try_block->statement;
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case stmk_microsoft_try:
        if (sssep->in_cleanup_statement_of_microsoft_try) {
          head_ptr = &ssp->variant.microsoft_try->cleanup_statement;
        } else {
          head_ptr = &ssp->variant.microsoft_try->guarded_statement;
          in_guarded_statement_of_microsoft_try = TRUE;
        }  /* if */
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      default:
        unexpected_condition_str(
             "add_statement_list: bad stmt kind in struct stmt stack");
    }  /* switch */
  }  /* if */

  /* See if the statement can be attached under the existing statement. */
  if ((*head_ptr != NULL || is_list) && !statement_list_allowed) {
    /* Sometimes, the structured statement already has a statement attached
       to it, and it is not a statement to which a list of statements may
       be attached.  This happens in rare cases like

         if (a) b: c = 1;

       i.e., the dependent statement of the "if" is labeled, and therefore
       two dependent statements are required under the if, which only allows
       one.  It also happens for "continue" labels.  For cases like this,
       we create an additional block to contain the list of statements. 
       If the dependent statement is a block (because the source dependent
       statement is a block), that block is used.
       A similar approach is also needed if more than one statement is
       being appended. */
    if (*head_ptr != NULL &&
        (*head_ptr)->kind == (a_statement_kind)stmk_block &&
        ((*head_ptr)->variant.block.extra_info->assoc_scope == NULL
#if MICROSOFT_EXTENSIONS_ALLOWED
         /* Avoid adding a new block for the continue label of a Microsoft
            __try, because the top-level variables in the __try are visible
            in the __except expression. */
         || in_guarded_statement_of_microsoft_try
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                                   ) &&
        !block_stmt_is_cfront_dependent_stmt(*head_ptr)) {
      /* There is an existing block from a source construct.  Find the 
         end of its statement list, and add there.  Note that blocks that
         contain declarations are ruled out: we don't want to add a
         statement inside such a block.  (That's especially true in
         C++, where the end of the block may kick off destructor calls
         which must be done before the statement being added is executed.)
         Also note that the top compound statement of a switch never has
         an associated scope at this point (the scope gets added at the
         closing brace), so it's acceptable, which is what we want. */
      ssp = *head_ptr;
      temp_stmt = ssp->variant.block.statements;
      if (temp_stmt != NULL) {
        while (temp_stmt->next != NULL) temp_stmt = temp_stmt->next;
      }  /* if */
      sssep->last_dep_statement = temp_stmt;
      if (reachable) {
        /* The end of the block is reachable if the new statement is
           reachable.  This is important because continue labels are
           always reachable. */
        ssp->variant.block.extra_info->end_of_block_reachable = TRUE;
      }  /* if */
    } else {
      /* Create a new block to allow additional statements. */
      ssp = alloc_statement(stmk_block, /*compiler_generated=*/TRUE);
      /* This doesn't get added to the source sequence list; it's not
         in the source. */
      ssp->variant.block.extra_info->implicit_scope_not_allowed = TRUE;
      ssp->variant.block.statements = *head_ptr;
      *head_ptr = ssp;
    }  /* if */
    head_ptr = &ssp->variant.block.statements;
    sssep->extra_block = ssp;
  }  /* if */
  /* Add the new statement to the end of the statement list for the
     current level of the structured statement stack.  Even unreachable
     code is kept. */
  if (*head_ptr == NULL) {
    /* Add the statement as the first statement on the list. */
    *head_ptr = sp;
  } else {
    if (sssep->last_dep_statement == NULL) {
      /* If the last pointer is NULL, find the last statement in the list
         and set the pointer to it.  This is needed when switching back to
         a statement list that already has some statements in it (e.g.,
         after a break statement in a switch). */
      temp_stmt = *head_ptr;
      while (temp_stmt->next != NULL) temp_stmt = temp_stmt->next;
      sssep->last_dep_statement = temp_stmt;
    }  /* if */
    if (sssep->last_dep_statement->is_fallthrough_statement &&
        !(sp->kind == (a_statement_kind)stmk_switch_case ||
          (gnu_mode && !clang_mode &&
           sp->kind == (a_statement_kind)stmk_label))) {
      /* Only a case label or default label may follow a fallthrough
         statement (GCC also allows a user-defined label). */
      pos_diagnostic(clang_mode ? es_error :
                                  strict_ansi_discretionary_severity,
                     ec_fallthrough_must_precede_switch_case,
                     &sssep->last_dep_statement->position);
    }  /* if */
    sssep->last_dep_statement->next = sp;
  }  /* if */
  /* Find the last statement in the inserted list, and update the parent
     pointer for every element of that list. */
  temp_stmt = sp;
  while (temp_stmt->next != NULL) {
    temp_stmt->parent = ssp;
    temp_stmt = temp_stmt->next;
  }  /* while */
  temp_stmt->parent = ssp;
  sssep->last_dep_statement = temp_stmt;
  if (sssep->prefix_attributes != NULL && sp->kind != stmk_label) {
    /* Attach any attributes.  Label definitions are handled elsewhere (and
       implicit label definitions should not pick up the attributes of the
       statements that generate them). */
    attach_attributes(sssep->prefix_attributes, (char*)sp, iek_statement);
    sssep->prefix_attributes = NULL;
  }  /* if */
  db_exit();
}  /* add_statement_list */

#if UPC_EXTENSIONS_ALLOWED

static void check_for_return_in_upc_forall(a_source_position  *stmt_pos)
/*
We're about to create a return statement.  Issue a warning if it is a reachable
statement within a upc_forall construct.  (The UPC specification indicates that
this leads to undefined behavior when executed.)
*/
{
  if (upc_mode && curr_reachability.reachable_considering_hints &&
      innermost_forall_loop != NULL) {
    pos_warning(ec_exit_forall, stmt_pos);
  }  /* if */
}  /* check_for_return_in_upc_forall */

#else /* !UPC_EXTENSIONS_ALLOWED */

#define check_for_return_in_upc_forall(stmt_pos)  /* Nothing */

#endif /* UPC_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED

a_boolean in_gnu_stmt_expression()
/*
Return TRUE if currently parsing a GNU statement expression; otherwise, return
FALSE.
*/
{
  return (depth_stmt_stack != NO_SCOPE_DEPTH &&
          struct_stmt_stack[depth_stmt_stack].inside_statement_expr);
}  /* in_gnu_stmt_expression */

#endif /* GNU_EXTENSIONS_ALLOWED */

a_statement_ptr add_statement_at_stmt_pos(a_statement_kind  kind,
                                          a_source_position *stmt_pos,
                                          a_boolean         compiler_generated)
/*
Allocate a statement of the indicated kind, record the statement
source position specified in *stmt_pos, and link it onto the end of
the current statement sequence.  Set the compiler_generated flag as indicated
by the compiler_generated argument.
*/
{
  a_statement_ptr  sp;

  db_enter(5, "add_statement_at_stmt_pos");
  /* Maintain the code reachable flag.  Labels are always reachable. */
  if (kind == (a_statement_kind)stmk_label) {
    set_reachable(curr_reachability);
  } else if (kind == (a_statement_kind)stmk_return
             || kind == (a_statement_kind)stmk_coroutine_return) {
    a_routine_ptr  rp = current_routine_entry();
    a_type_ptr     rtp = skip_typerefs(rp->type);
    if (rtp->variant.routine.extra_info->does_not_return &&
        curr_reachability.reachable_considering_hints &&
        !rp->is_prototype_instantiation) {
      /* Issue a warning on the return statement.  (For implicit returns,
         the warning is issued on the current token, which is normally the
         closing brace.) */
      pos_warning(ec_noreturn_function_does_return,
                  stmt_pos->seq != 0 ? stmt_pos
                                     : &pos_curr_token);
    }  /* if */
    check_for_return_in_upc_forall(stmt_pos);
  }  /* if */

  /* Allocate the statement entry. */
  sp = alloc_statement(kind, compiler_generated);
  /* Set the position from *stmt_pos. */
  sp->position = *stmt_pos;

  /* Append the statement. */
  add_statement_list(sp, curr_reachability.reachable);

  /* Turn off curr_reachability if the current statement is an
     unconditional branch. */
  if (kind == (a_statement_kind)stmk_goto   ||
#if GNU_EXTENSIONS_ALLOWED
      kind == (a_statement_kind)stmk_assigned_goto ||
#endif /* GNU_EXTENSIONS_ALLOWED */
      kind == (a_statement_kind)stmk_coroutine_return ||
      kind == (a_statement_kind)stmk_return) {
    set_unreachable(curr_reachability);
  }  /* if */
  if (kind == (a_statement_kind)stmk_init ||
      kind == (a_statement_kind)stmk_empty ||
      kind == (a_statement_kind)stmk_decl ||
      kind == (a_statement_kind)stmk_set_vla_size) {
    /* Not an executable statement. */
  } else {
    /* Anything else is an executable statement.  Set a flag indicating
       that an executable statement has been seen in the current block. */
    struct_stmt_stack_top().any_exec_statement_seen = TRUE;
  }  /* if */
  struct_stmt_stack_top().p_start_pos = NULL;
  db_exit();
  return(sp);
}  /* add_statement_at_stmt_pos */


/*
Call add_statement_at_stmt_pos using pos_curr_token as statement source
position.  Set the compiler_generated flag as appropriate.
*/
#define add_statement(kind, compiler_generated)                              \
  add_statement_at_stmt_pos((kind),                                          \
                            struct_stmt_stack_top().p_start_pos != NULL ?    \
                                         struct_stmt_stack_top().p_start_pos \
                                       : &pos_curr_token,                    \
                            (compiler_generated))

void update_init_statement_control_flow(a_statement_ptr  sp)
/*
An stmk_init or stmk_set_vla_size statement is being added to the IL.  Add
an entry to the control_flow_descr_list to point to it.  This will be part
of the information used to diagnose transfers of control over initializing
declarations.
*/
{
  a_control_flow_descr_ptr  cfdp;

  check_assertion(sp->kind == (a_statement_kind)stmk_init ||
                  sp->kind == (a_statement_kind)stmk_vla_decl ||
                  sp->kind == (a_statement_kind)stmk_set_vla_size);
  cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_init);
  cfdp->variant.init.statement = sp;
  if (sp->kind == (a_statement_kind)stmk_init) {
    cfdp->variant.init.variable = sp->variant.dynamic_init->variable;
  } else if (sp->kind == (a_statement_kind)stmk_vla_decl &&
             !sp->variant.vla.is_typedef_decl &&
             is_vla_type(sp->variant.vla.variant.variable->type)) {
    cfdp->variant.init.variable = sp->variant.vla.variant.variable;
    cfdp->variant.init.is_vla_variable = TRUE;
  }  /* if */
  cfdp->variant.init.in_statement_expression = inside_statement_expression();
  add_to_control_flow_descr_list(cfdp);
}  /* update_init_statement_control_flow */


void record_trivial_init_control_flow(a_variable_ptr  var)
/*
Record a control flow entry for a trivial initialization of the given variable
(corresponding to the invocation of a trivial constructor).   Such an
initialization does not require an init statement (since no actual
initialization work must be performed), but it must be diagnosed when branched
over.
*/
{
  a_control_flow_descr_ptr  cfdp;

  cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_init);
  cfdp->variant.init.variable = var;
  add_to_control_flow_descr_list(cfdp);
}  /* record_trivial_init_control_flow */

#if GENERATE_SOURCE_SEQUENCE_LISTS

static void record_sse_for_decl_statement(a_statement_ptr  sp)
/*
Record a source sequence entry for the given stmk_decl statement.  Because
such statements are decided after prescanning for declaration vs. expression
ambiguities, the source sequence entry is not always recorded at the end of
the list of source sequence entries for the current scope.  Also, the case
of a stmk_decl entry created for a C++17 "if" or "switch" initializer needs
extra care (since it is created after the declaration is fully scanned).
*/
{
  a_boolean                      early_sses_present = FALSE;
  a_source_sequence_entry_ptr    move_to_point = NULL;
  a_struct_stmt_stack_entry_ptr  sssep = &struct_stmt_stack_top();

  /* Identify if any source sequence entries have been added during
     declaration vs. expression disambiguation. */
  if (C_mode()) {
    /* Nothing to do. */
  } else {
    /* We recorded the last source sequence entry emitted before starting
       a look-ahead procedure that may have added additional entries.  The
       new entry should be moved ahead of those additional entries. */
    a_source_sequence_entry_ptr  prev_last;
    prev_last = sssep->last_sse_before_expr_decl_disambiguation;
    if (prev_last != NULL) {
      if (prev_last->next != NULL) {
        /* An entry was appended directly after the last one recorded before
           disambiguation. */
        early_sses_present = TRUE;
        move_to_point = prev_last->next;
      } else if (scope_is(&scope_stack_top(), sck_condition) &&
                 (sssep->kind == ssk_if ||
                  sssep->kind == ssk_constexpr_if ||
                  sssep->kind == ssk_switch)) {
        /* A stmk_decl entry in a condition scope of an "if" or "switch".
           This can happen in C++17 with constructs like:
                 switch (int x = f(); int y = x+1) ...
        */
        early_sses_present = TRUE;
        move_to_point = scope_stack_top().source_sequence_list;
      }  /* if */
    } else if (scope_stack_top().source_sequence_list != NULL) {
      /* At the time we started disambiguation, the source sequence list for
         this scope was empty, but now additional entries have been added.
         Move the new entry to the head of the list. */
      early_sses_present = TRUE;
      move_to_point = scope_stack_top().source_sequence_list;
    }  /* if */
  }  /* if */
  f_update_source_sequence_list((char*)sp, (an_il_entry_kind)iek_statement,
                                (a_source_sequence_entry_ptr)NULL);
  if (early_sses_present) {
    /* One or more source sequence entries were added as a side effect of
       disambiguation before we added the entry for the statement above.
       Move the entry for the statement to before those added entries. */
    move_src_seq_entry(sp->source_sequence_entry, depth_scope_stack,
                       move_to_point, depth_scope_stack);
  }  /* if */
}  /* record_sse_for_decl_statement */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */


static void decl_statement(a_boolean  marked_as_gnu_extension,
                           a_boolean  *p_okay_in_constexpr_body)
/*
Parse a declaration statement.  An stmk_decl statement is created for the
statement and the declared entities are recorded in it (except for entities
declared in embedded scopes, like function prototype scopes or block scopes
for GNU statement expressions).  If marked_as_gnu_extension is TRUE, the
__extension__ keyword was scanned just before the upcoming declaration.
If p_okay_in_constexpr_body is non-NULL, *p_okay_in_constexpr_body is set
to TRUE if (and only if) the declaration that is parsed can be valid in the
body of a constexpr function or constructor.
*/
{
  a_decl_parse_state             dps;
  a_statement_ptr                sp;
  a_struct_stmt_stack_entry_ptr  sssep = &struct_stmt_stack_top();
  an_il_entity_list_entry_ptr    entity_list;
  a_boolean                      is_static_assert;

  sp = add_statement(stmk_decl, /*compiler_generated=*/FALSE);
  if (!sssep->record_declared_entities) {
    /* The caller didn't set up a list to record declared entities.  Do it
       now. */
    sssep->record_declared_entities = TRUE;
    entity_list = NULL;
    sssep->p_declared_entities = &entity_list;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (!source_sequence_entries_disallowed) {
    record_sse_for_decl_statement(sp);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  init_decl_parse_state(&dps);
  dps.marked_as_gnu_extension = marked_as_gnu_extension;
  is_static_assert = curr_token == tok_static_assert;
  scan_nonmember_declaration(&dps, (a_source_range *)NULL);
  if (p_okay_in_constexpr_body != NULL) {
    if (!relaxed_constexpr_allowed() &&
        !struct_stmt_stack_top().inside_statement_expr) {
      /* In C++11-style constexpr, the declaration might have made the function
         non-constexpr.  However, those limitations don't apply within GNU
         statement expressions. */
      *p_okay_in_constexpr_body = dps.decl_okay_in_constexpr_body;
    } else {
      *p_okay_in_constexpr_body = TRUE;
    }  /* if */
  }  /* if */
  /* Re-load sssep since the call to scan_nonmember_declaration may have
     caused the statement stack to be reallocated. */
  sssep = &struct_stmt_stack_top();
  if (sssep->for_init) {
    if (dps.specifiers_type == NULL &&
        !(dps.is_empty_decl && !strict_ansi_mode) &&
        !(C_mode() && is_static_assert)) {
      /* If dps.specifiers_type is NULL, the declaration we just scanned was
         not a "simple-declaration" (i.e., a declaration consisting of some
         optional attributes, followed by decl-specifiers, and optionally
         followed by a declarator) nor an alias-declaration.  As an extension,
         we also accept an empty declaration (with attributes) here in
         nonstrict modes.  C allows a static assertion declaration here as
         well. */
      pos_error(ec_invalid_init_statement, &dps.start_pos);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (!source_sequence_entries_disallowed) {
      /* Add a source sequence entry marking the end of the for-init
         declaration.  This marker is necessary in case what immediately
         follows in the source sequence list is an entry for a condition
         declaration.  E.g., without the marker, there would be no distinction
         between "for (int i = 0; int j = 3; --j);" and
         "for (int i = 0, j = 3; ; --j);". */
      add_end_of_construct_source_sequence_entry((char*)sp, iek_statement);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  if (*sssep->p_declared_entities != NULL) {
    an_il_entity_list_entry_ptr  ielep = *sssep->p_declared_entities;
    sssep->p_declared_entities = NULL;
    sp->variant.decl.entities = ielep;
    for (; ielep != NULL; ielep = ielep->next) {
      if (ielep->entity.kind == iek_variable) {
        a_variable_ptr  vp = (a_variable_ptr)ielep->entity.ptr;
        if (var_has_static_or_thread_storage_duration(vp)) {
          sp->variant.decl.has_static_or_thread_variable = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  sssep->record_declared_entities = FALSE;
}  /* decl_statement */


static void start_potential_decl_statement(
                                  an_il_entity_list_entry_ptr  *p_entity_list)
/*
A declaration vs. expression disambiguation is next and that may produce
IL for declared entities.  E.g.:

    template<typename> struct X {};
    int main() {
      X<struct E> x;
    }

The declaration of "struct E" in the argument list of X will trigger the
creation of its associated source sequence entry, but the entry for the
declaration statement for x is not yet on the list.  This routine records the
needed information to be able to move the forthcoming source sequence entry for
the statement to before the entries created during disambiguation.

Similarly, this routine also enables the recording of an_il_entity_list entries
for any declared entities.  The list will be pointed to by *p_entity_list.
*p_entity_list must be allocated in the caller's stack frame so it lasts until
the end of the statement's processing.  In particular, it cannot be placed in
the statement stack itself because it may also be referred to by other
structures (e.g., a_decl_parse_state) and the statement stack could move during
declaration processing.
*/
{
  check_assertion(!C_mode());
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Record the current last entry in this scope so that any entries created
     as part of disambiguation can be identified and moved later on. */
  struct_stmt_stack_top().last_sse_before_expr_decl_disambiguation =
                                scope_stack_top().end_of_source_sequence_list;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Start recording declared entities. */
  struct_stmt_stack_top().record_declared_entities = TRUE;
  struct_stmt_stack_top().p_declared_entities = p_entity_list;
  *p_entity_list = NULL;
}  /* start_potential_decl_statement */


static void end_potential_decl_statement(void)
/*
Undo any leftover state changes that resulted from calling
start_potential_decl_statement and reclaim associated unused memory.
*/
{
#if GENERATE_SOURCE_SEQUENCE_LISTS
  struct_stmt_stack_top().last_sse_before_expr_decl_disambiguation = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  struct_stmt_stack_top().record_declared_entities = FALSE;
  struct_stmt_stack_top().p_declared_entities = NULL;
}  /* end_potential_decl_statement */


static void for_range_declaration(a_decl_parse_state  *dps)
/*
Parse the for-range-declaration portion of a range-based-for statement.
Initialize and update *dps to reflect the declaration state (e.g., dps->sym
is set to the symbol pointer of the variable just scanned; it may be NULL in
some error cases).
*/
{
  init_decl_parse_state(dps);
  dps->range_based_for = TRUE;
  scan_nonmember_declaration(dps, (a_source_range *)NULL);
  check_for_range_declaration(dps);
}  /* for_range_declaration */


void record_entity_in_decl_stmt_if_needed(a_symbol_ptr  sym)
/*
If we are in a declaration statement, update its associated list of declared
entities to include the entity described by sym.  Only entities declared in
function or block scopes are recorded (in particular, entities declared in
function prototype scope are not recorded).
*/
{
  if (depth_stmt_stack >= 0 && sym != NULL &&
      struct_stmt_stack_top().record_declared_entities) {
    a_scope_depth  decl_level = depth_scope_stack;
    /* Determine in which scope the symbol was declared.  Usually, this is the
       scope currently on top of the scope stack. */
    while (decl_level >= 0 &&
           scope_stack[decl_level].number != sym->decl_scope) {
      decl_level -= 1;
    }  /* while */
    if (decl_level >= 0 && is_local_scope_kind(scope_stack[decl_level].kind)) {
      a_memory_region_number         region_to_switch_back_to;
      a_struct_stmt_stack_entry_ptr  sssep = &struct_stmt_stack_top();
      an_il_entity_list_entry_ptr    *p = sssep->p_declared_entities;
      an_il_entry_kind               entity_kind;
      /* Skip to the end of the list to append a new entry. */
      while (*p != NULL) p = &(*p)->next;
      /* Ensure the entry is allocated in the same memory region as the
         stmk_decl statement. */
      switch_to_scope_region(decl_level, &region_to_switch_back_to);
      *p = alloc_il_entity_list_entry();
      switch_back_to_original_region(region_to_switch_back_to);
      (*p)->entity.ptr = il_entry_for_symbol(sym, &entity_kind);
      (*p)->entity.kind = entity_kind;
    }  /* if */
  }  /* if */
}  /* record_entity_in_decl_stmt_if_needed */

#if GENERATE_SOURCE_SEQUENCE_LISTS
#define stmt_update_source_sequence_list(sp)                          \
  update_source_sequence_list((char *)sp, iek_statement,              \
                              (a_source_sequence_entry_ptr)NULL)
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
#define stmt_update_source_sequence_list(sp)       /* Nothing */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if VLA_DEALLOCATIONS_IN_IL

static a_statement_ptr create_vla_deallocation_stmt(a_variable_ptr  vla_var)
/*
Allocate and return a statement entry for the deallocation of a variable-length
array (represented by the given variable entry).  This should only be called in
C mode (in C++ mode, object lifetime entries are used instead; C++ IL lowering
may produce enk_vla_dealloc entries from those elsewhere).
*/
{
  an_expr_node_ptr  expr = alloc_expr_node((an_expr_node_kind)enk_vla_dealloc);
  a_statement_ptr   result;

  expr->type = void_type();
  expr->variant.vla_variable = vla_var;
  result = alloc_expr_statement(expr);
#if DEBUG
  if (debug_level >= 4) {
    fputs("  creating vla-dealloc statement for \"", f_debug);
    db_name(&vla_var->source_corresp);
    fputs("\"\n", f_debug);
  }  /* if */
#endif /* DEBUG */
  check_assertion(C_mode());
  return result;
}  /* create_vla_deallocation_stmt */


static a_statement_ptr collect_vla_dealloc_stmts(
                                              a_control_flow_descr_ptr  start,
                                              a_control_flow_descr_ptr  end)
/*
Create a list (possibly NULL) of vla-dealloc statements for each vla-decl
statement in the current block that represents the declaration (and
therefore allocation) of a VLA object.  The vla-decl statements are located
by traversing the control-flow list backwards from *start to *end.
*/
{
  a_control_flow_descr_ptr  cfdp, parent, end_parent, stop_at;
  a_statement_ptr           dealloc_stmt, first = NULL, last = NULL;
  a_boolean                 done;

  db_enter(4, "collect_vla_dealloc_stmts");
  check_assertion(vla_deallocations_in_il);
#if DEBUG
  if (debug_level == 4) {
    fputs("  start = ", f_debug);
    db_cfd(start);
    fputs("  end = ", f_debug);
    if (end == NULL) {
      fputs("NULL\n", f_debug);
    } else {
      db_cfd(end);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  cfdp = start;
  parent = cfdp->parent;
  if (end == NULL) {
    end = control_flow_descr_list;
    end_parent = NULL;
  } else {
    end_parent = end->parent;
  }  /* if */
  /* Step through control-flow entries from "start" to "end", which means
     stepping backwards.  As an optimization, blocks that are marked as
     having no VLA variable declarations are bypassed. */
  done = (parent == NULL);
  while (!done) {
    check_assertion(parent != NULL);
    if (!parent->variant.block.any_vla_variables) {
#if DEBUG
      if (debug_level == 4) {
        fputs("  skipping block (no vla vars): ", f_debug);
        db_cfd(parent);
      }  /* if */
#endif /* DEBUG */
      if (parent == end_parent) {
        done = TRUE;
      } else {
        cfdp = parent;
        if (parent == end) done = TRUE;
        parent = parent->parent;
      }  /* if */
    } else {
      if (parent == end_parent) {
	stop_at = end;
      } else {
        stop_at = parent;
      }  /* if */
#if DEBUG
      if (debug_level == 4) {
        fputs("  inner loop: first entry = ", f_debug);
        db_cfd(cfdp);
        fputs("  stop_at = ", f_debug);
        db_cfd(stop_at);
      }  /* if */
#endif /* DEBUG */
      /* Back up from *cfdp to *stop_at, looking for VLA variable
         declarations. */
      while (cfdp != stop_at) {
        if (cfdp->kind == (a_control_flow_descr_kind)cfdk_init) {
          if (cfdp->variant.init.is_vla_variable) {
            /* The declaration of a VLA variable has been located.  Create a
               new statement to represent its deallocation. */
            if (cfdp->variant.init.in_statement_expression) {
              /* Variable-length arrays are not accepted in GNU statement
                 expressions.  Since a deallocation statement may displace the
                 actual "last statement", it could also trigger additional
                 (spurious) errors about the type of the last statement.  So
                 we don't create the deallocation node in such cases. */
              check_assertion(is_at_least_one_error());
            } else {
              dealloc_stmt = create_vla_deallocation_stmt(
                                                 cfdp->variant.init.variable);
              /* Append it to the collected list. */
              if (first == NULL) {
                first = last = dealloc_stmt;
              } else {
                check_assertion(last != NULL);
                last->next = dealloc_stmt;
                last = last->next;
              }  /* if */
            }  /* if */
          }  /* if */
        } else if (cfdp->kind ==
                          (a_control_flow_descr_kind)cfdk_end_of_block) {
          /* Skip a nested block. */
          cfdp = cfdp->variant.start_of_block;
#if DEBUG
          if (debug_level == 4) {
            fputs("  skipping nested block: ", f_debug);
            db_cfd(cfdp);
          }  /* if */
#endif /* DEBUG */
        }  /* if */
        cfdp = cfdp->prev;
#if DEBUG
        if (debug_level == 4) {
          fputs("  inner loop: next entry = ", f_debug);
          db_cfd(cfdp);
        }  /* if */
#endif /* DEBUG */
      }  /* while */
      /* cpfe now points to the start of a block.  Keep stepping through
         its parent block, if appropriate. */
      if (cfdp == end) {
        done = TRUE;
      } else {
        parent = cfdp->parent;
      }  /* if */
    }  /* if */
  }  /* while */
  db_exit();
  return first;
}  /* collect_vla_dealloc_stmts */


static void add_vla_dealloc_stmts(a_control_flow_descr_ptr  start,
                                  a_control_flow_descr_ptr  end,
                                  a_boolean                 is_goto)
/*
Add a vla-dealloc statement to the statements list for each vla-decl
statement in the current block that represents the declaration (and
therefore allocation) of a VLA object.  The vla-decl statements are located
by traversing the control-flow list backwards from *start to *end.  is_goto
is TRUE if vla-dealloc statements are to be inserted in front of *start,
which is a goto statement.  (It can be a goto statement even when is_goto
is FALSE.)
*/
{
  a_statement_ptr           dealloc_stmts;

  db_enter(5, "add_vla_dealloc_stmts");
  dealloc_stmts = collect_vla_dealloc_stmts(start, end);

  if (dealloc_stmts == NULL) {
    /* Nothing to do. */
  } else if (!is_goto) {
    /* If the deallocation point is the end of a block or a return statement,
       the deallocation statements are just added to the end of the statements
       list. */
    add_statement_list(dealloc_stmts, curr_reachability.reachable);
  } else {
    /* If the allocation point is a goto statement, the
       deallocation statement needs to be inserted immediately
       before the goto.  The trick is to turn the goto into a
       block statement with a single statement in its list
       (namely, the goto).  One or more vla-dealloc statements can
       then be inserted before it -- local variable insert_point
       is used to remember the point if multiple vla-decl
       statements are found. */
    a_statement_ptr  block_stmt, copy_of_goto_stmt;
    /* Save the address of the goto statement in "block_stmt".
       It will become a block statement with the call of
       change_statement_into_block. */
    block_stmt = start->variant.goto_statement.ptr;
    change_statement_into_block(block_stmt, &copy_of_goto_stmt);
    /* Reset the control flow entry pointing at the goto statement to use the
       new pointer. */
    start->variant.goto_statement.ptr = copy_of_goto_stmt;
    /* Link the deallocation statement into the block. */
    block_stmt->variant.block.statements = dealloc_stmts;
    /* Find the end of the list of deallocation statements to link the copied
       goto statement after it.  Also update the parent pointers. */
    while (dealloc_stmts->next != NULL) {
      dealloc_stmts->parent = block_stmt;
      dealloc_stmts = dealloc_stmts->next;
    }  /* while */
    dealloc_stmts->parent = block_stmt;
    dealloc_stmts->next = copy_of_goto_stmt;
  }  /* if */
  db_exit();
}  /* add_vla_dealloc_stmts */


static void add_vla_dealloc_stmts_for_block(a_control_flow_descr_ptr  cfdp)
/*
Add vla-dealloc statements to the end of the statements list for declarations
of VLA objects found within the current block, between *cfdp and the beginning
of the block to which it belongs.  This routine is typically called when
processing the normal end of a block.
*/
{
  check_assertion(end_of_control_flow_descr_list == cfdp);
  if (cfdp->kind == (a_control_flow_descr_kind)cfdk_block) {
    /* Empty block: no need to look for VLA object initializations. */
  } else {
    add_vla_dealloc_stmts(cfdp, cfdp->parent, /*is_goto=*/FALSE);
  }  /* if */
}  /* add_vla_dealloc_stmts_for_block */


static a_statement_ptr collect_vla_dealloc_stmts_for_function(
                                               a_control_flow_descr_ptr  cfdp)
/*
Add vla-dealloc statements for all declarations of VLA objects in currently
active scopes.  The active scopes are located by starting with *cfdp and
backing up through the control flow list (via the parent pointer) to the
outermost scope of the function.  This function is typically called when
processing a return statement (implicit or explicit).
*/
{
  check_assertion(end_of_control_flow_descr_list == cfdp);
  return collect_vla_dealloc_stmts(cfdp, control_flow_descr_list);
}  /* collect_vla_dealloc_stmts_for_function */


static void add_vla_dealloc_stmts_for_goto(
                                       a_control_flow_descr_ptr  goto_cfdp,
                                       a_control_flow_descr_ptr  label_cfdp)
/*
Add vla-dealloc statements needed at a goto statement.  goto_cfdp describes
the goto, and label_cfsp describes the associated label.
*/
{
  a_boolean                 forward_goto;
  a_control_flow_descr_ptr  common_parent, outermost_noncommon_parent;
  a_control_flow_descr_ptr  goto_parent, label_parent;

  db_enter(4, "add_vla_dealloc_stmts_for_goto");
#if DEBUG
  if (debug_level == 4) {
    db_cfd(goto_cfdp);
    db_cfd(label_cfdp);
  }  /* if */
#endif /* DEBUG */
  /* If the last entry on the control flow list is goto_cfdp, then the
     label has already been defined and this is a backwards goto; otherwise
     it's a forward goto. */
  forward_goto = (goto_cfdp != end_of_control_flow_descr_list);
  /* Find the common parent block (that is, the innermost block that
     contains both the label and the goto), and the outermost noncommon
     block, which is the block (if any) that is immediately within the
     common parent block and that contains the label, if this is a backwards
     goto, or the goto, if this is a forward goto.  The rationale is
     provided below. */
  common_parent = NULL;
  outermost_noncommon_parent = NULL;
  /* Step from inner scope to outer, starting with the block of the goto. */
  for (goto_parent = goto_cfdp->parent;
       goto_parent != NULL;
       goto_parent = goto_parent->parent) {
    /* Step from inner scope to outer, starting with the block of the label.
       If this is a backwards goto, the outermost noncommon parent is reset
       each time the inner loop is entered. */
    if (!forward_goto) outermost_noncommon_parent = NULL;
    for (label_parent = label_cfdp->parent;
         label_parent != NULL;
         label_parent = label_parent->parent) {
      /* The first block that is common to both loops is the common parent. */
      if (goto_parent == label_parent) {
        common_parent = goto_parent;
        break;
      }  /* if */
      if (!forward_goto) outermost_noncommon_parent = label_parent;
    }  /* for */
    if (common_parent != NULL) break;
    if (forward_goto) outermost_noncommon_parent = goto_parent;
  }  /* for */
#if DEBUG
  if (debug_level == 4) {
    fputs("common_parent = ", f_debug);
    if (common_parent == NULL) {
      fputs("NULL\n", f_debug);
    } else {
      db_cfd(common_parent);
    }  /* if */
    fputs("outermost_noncommon_parent = ", f_debug);
    if (outermost_noncommon_parent == NULL) {
      fputs("NULL\n", f_debug);
    } else {
      db_cfd(outermost_noncommon_parent);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* There are 2 orthogonal sets of criteria for determining how to search
     for VLAs that need to be deallocated.  There is the direction of the
     goto (forward or backwards) and the nesting relationship: (1) the goto
     and label are in the same scope; (2) the scope of the goto is nested
     within the scope of the label; (3) the scope of the label is nested
     within the scope of the goto; and (4) neither scope includes the
     other.  The 8 situations can be pictured as follows:

       forward:
         (1)          (2)           (3)          (4)
              |           |             |            |
              |           ----          |            ----
             goto            |*        goto             |*
              |             goto        |              goto
              |              |          ----            |
              |           ----             |         ----
            label         |              label       |
                        label                        ----
                                                        |
                                                      label
       backwards:
         (1)          (2)           (3)          (4)
              |           |             |            |
              |           |             ----         ----
            label       label              |            |
              |*          |*             label        label
              |*          ----             |            |
              |*             |*         ----         ----
             goto          goto         |*           |*
                                       goto          ----
                                                        |*
                                                       goto

     The sections marked "|*" are searched for VLA declarations for which
     deallocation statements are inserted immediately before the goto. */
  if (forward_goto) {
    if (goto_cfdp->parent == common_parent) {
      /* Forward (1) and (3): no action required. */
    } else {
      /* Forward (2) and (4): back up from the goto to the beginning of the
         outermost noncommon parent. */
      check_assertion(outermost_noncommon_parent != NULL &&
                      outermost_noncommon_parent->parent == common_parent);
      add_vla_dealloc_stmts(goto_cfdp, outermost_noncommon_parent,
                            /*is_goto=*/TRUE);
    }  /* if */
  } else {
    if (label_cfdp->parent == common_parent) {
      /* Backwards (1) and (2): back up from the goto to the label. */
      add_vla_dealloc_stmts(goto_cfdp, label_cfdp, /*is_goto=*/TRUE);
    } else {
      /* Backwards (3) and (4): back up from the goto to the end of the
         outermost noncommon parent. */
      check_assertion(outermost_noncommon_parent != NULL &&
                      outermost_noncommon_parent->parent == common_parent);
      check_assertion(outermost_noncommon_parent->
                                  variant.block.end_of_block != NULL);
      add_vla_dealloc_stmts(goto_cfdp,
                            outermost_noncommon_parent->
                              variant.block.end_of_block,
                            /*is_goto=*/TRUE);
    }  /* if */
  }  /* if */
  db_exit();
}  /* add_vla_dealloc_stmts_for_goto */

#endif /* VLA_DEALLOCATIONS_IN_IL */

void set_vla_size_statement(a_vla_dimension_ptr  vdp,
                            a_source_position    *pos)
/*
Generate an stmk_set_vla_size statement for a variable length array
(represented by vdp) to indicate when (at runtime) the VLA dimension
expression is to be evaluated to fix the size of the array.
*/
{
  a_statement_ptr          vla_stmt;

  check_assertion(!vdp->has_size_statement);
  vla_stmt = add_statement_at_stmt_pos(stmk_set_vla_size, pos,
                                       /*compiler_generated=*/TRUE);
  vla_stmt->variant.vla_dimension = vdp;
  vdp->has_size_statement = TRUE;
  update_init_statement_control_flow(vla_stmt);
}  /* set_vla_size_statement */


STATIC_THREAD a_source_position
		*pos_for_vla_size_statements;
			/* The position at which the statements generated by
			   ttt_set_vla_size_for_component are put out. */


static a_boolean ttt_set_vla_size_for_component(
                            a_type_ptr           type_ptr,
                            ARG_UNUSED a_boolean *force_end_of_traversal)
/*
If type_ptr is a VLA type whose dimension is not evaluated anywhere yet,
generate an stmk_set_vla_size statement to evaluate it at
*pos_for_vla_size_statements.  This is a type tree traversal function that
visits every component of a type, so it always returns FALSE to let the
traversal run to completion.
*/
{
  if (type_is(type_ptr, tk_array) &&
      type_ptr->variant.array.has_assoc_vla_dimension) {
    a_vla_dimension_ptr  vdp =
                            find_vla_dimension_in_current_function(type_ptr);
    /* The entry belongs to another function when a local class is being
       parsed, and one that already has a statement is evaluated where that
       statement appears. */
    if (vdp != NULL && !vdp->has_size_statement) {
      set_vla_size_statement(vdp, pos_for_vla_size_statements);
    }  /* if */
  }  /* if */
  return FALSE;
}  /* ttt_set_vla_size_for_component */


void generate_vla_size_statements_for_type(a_type_ptr         tp,
                                           a_source_position  *pos)
/*
Arrange for the dimensions of the VLA components of tp to be evaluated at
*pos, which is where the declaration that uses tp appears.  This is needed
for a type that was built while an expression was being scanned, as happens
for the type deduced for "auto p = (int (*)[n]) q;": No evaluation point was
established for its dimensions then, because a type named in an expression is
normally written just once, whereas the type of a variable is also written
wherever the variable is declared.  Establishing one here keeps a dimension
expression such as "++n" from being evaluated more than once.
*/
{
  if (vla_enabled && il_header.vla_used) {
    a_type_tree_traversal_flag_set  tt_flags = TTT_STOP_AT_TYPEDEFS |
                                               TTT_RETURN_TYPE;
    pos_for_vla_size_statements = pos;
    (void)traverse_type_tree(tp, ttt_set_vla_size_for_component, tt_flags);
  }  /* if */
}  /* generate_vla_size_statements_for_type */


void warn_if_code_is_unreachable(an_error_code      error_code,
                                 a_source_position  *err_pos)
/*
If the current location in the code is unreachable, generate a warning,
using the specified diagnostic message at the specified source position.
*/
{
  if (!curr_reachability.reachable) {
    if (!curr_reachability.suppress_unreachable_warning) {
      pos_warning(error_code, err_pos);
      /* Suppress the warning once it has been issued. */
      curr_reachability.suppress_unreachable_warning = TRUE;
    }  /* if */
  }  /* if */
}  /* warn_if_code_is_unreachable */


/* 
Generate a standard warning if the current location in the code is
unreachable.
*/
#define check_for_unreachable_code()                                    \
  warn_if_code_is_unreachable(ec_code_is_unreachable, &error_position)


static a_label_ptr alloc_temp_label(void)
/*
Allocate and return a pointer to a temporary label entry.
*/
{
  a_label_ptr label;

  label = alloc_label();
  add_to_labels_list(label);

  return (label);
}  /* alloc_temp_label */


static void define_label(a_label_ptr  label,
                         a_boolean    add_to_stmt_list = TRUE)
/*
Put out the definition for the indicated label.  If label == NULL, do nothing.
By default, the associated statement is appended to the statement list
currently active on the statement stack, but if add_to_stmt_list is FALSE,
that is not done and the caller is responsible to place the resulting
statement (which can be retrieved via label->exec_stmt).
*/
{
  a_statement_ptr sp;

  db_enter(4, "define_label");
  if (label != NULL) {
    label->reachable_by_fall_through = curr_reachability.reachable;
#if MICROSOFT_EXTENSIONS_ALLOWED
    label->num_microsoft_trys_inside_of =
              struct_stmt_stack[depth_stmt_stack].num_microsoft_trys_inside_of;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    set_reachable(curr_reachability);
    sp = alloc_statement(stmk_label, /*compiler_generated=*/FALSE);
    sp->position = label->source_corresp.decl_position;
    if (add_to_stmt_list) {
      add_statement_list(sp, curr_reachability.reachable);
      struct_stmt_stack_top().any_exec_statement_seen = TRUE;
      struct_stmt_stack_top().p_start_pos = NULL;
    }  /* if */
    label->exec_stmt = sp;
    sp->variant.label.ptr = label;
  }  /* if */
  db_exit();
}  /* define_label */


static an_object_lifetime_ptr common_object_lifetime(
                                              an_object_lifetime_ptr  olp1,
                                              an_object_lifetime_ptr  olp2)
/*
Return the innermost olk_block or olk_block_after_label lifetime that is
common to the parent lists of olp1 and olp2.  It is assumed that olp1 and
olp2 appear in the same function context and therefore that the current
function scope object lifetime, at least, appears on both ancestries.  This
function should only be called in C++ mode.
*/
{
  an_object_lifetime_ptr  olp;

  db_enter(4, "common_object_lifetime");
  if (olp1 != olp2) {
#if DEBUG
    if (debug_level >= 4) {
      db_object_lifetime_stack();
      fputs("olp1 = ", f_debug);
      db_object_lifetime(olp1);
      fputs("olp2 = ", f_debug);
      db_object_lifetime(olp2);
    }  /* if */
#endif /* DEBUG */
    /* The outer loop follows olp2 and its parent lifetimes. */
    for (; olp2 != function_scope_object_lifetime;
           olp2 = innermost_block_object_lifetime(olp2->parent_lifetime)) {
      /* The inner loop follows olp1 and its parent lifetimes. */
      for (olp = olp1;
           olp != function_scope_object_lifetime;
           olp = innermost_block_object_lifetime(olp->parent_lifetime)) {
        if (olp == olp2) {
          /* Found a match. */
#if DEBUG
          if (debug_level >= 4) {
            fputs("common = ", f_debug);
            db_object_lifetime(olp2);
          }  /* if */
#endif /* DEBUG */
          goto done;
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* if */
done:

  db_exit();
  return olp2;
}  /* common_object_lifetime */


static void define_implicit_label(
                            a_label_ptr               label,
                            a_control_flow_descr_ptr  goto_cfdp,
                            a_boolean                 add_to_stmt_list = TRUE)
/*
Define the specified label, which will have been referenced by one or more
goto statements represented by the linked list of control flow entries headed
by goto_cfdp.  By default, the associated statement is appended to the
statement list currently active on the statement stack, but if add_to_stmt_list
is FALSE, that is not done and the caller is responsible to place the resulting
statement (which can be retrieved via label->exec_stmt).
*/
{
  an_object_lifetime_ptr    label_olp, *goto_olp_addr;
  a_control_flow_descr_ptr  cfdp = NULL;

  define_label(label, add_to_stmt_list);
  if (!C_mode() || vla_enabled) {
    /* Do special C++ processing -- it's not needed in C mode because it is
       only used to support object lifetimes.  It is needed in C for
       variable-length arrays, however. */
    /* Create a control-flow entry to represent this label. */
    cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_label);
    cfdp->variant.label_statement = label->exec_stmt;
    cfdp->source_pos = pos_curr_token;
    add_to_control_flow_descr_list(cfdp);
  }  /* if */
  if (!C_mode()) {
    /* Record the innermost object lifetime as the object lifetime associated
       with this label. */
    label_olp = innermost_block_object_lifetime(curr_object_lifetime);
    label->exec_stmt->variant.label.lifetime = label_olp;
    /* For each branch that has this label as a target, record in the goto
       statement the common object lifetime (the one embracing both the label
       and the goto). */
    for (; goto_cfdp != NULL;
         goto_cfdp = goto_cfdp->variant.goto_statement.prev_goto) {
      goto_olp_addr = &goto_cfdp->variant.goto_statement.ptr->
                                               variant.label.lifetime;
      *goto_olp_addr = common_object_lifetime(label_olp, *goto_olp_addr);
    }  /* for */
#if VLA_DEALLOCATIONS_IN_IL
  } else if (vla_enabled && vla_deallocations_in_il &&
             (label->continue_label || label->break_label)) {
    for (; goto_cfdp != NULL;
         goto_cfdp = goto_cfdp->variant.goto_statement.prev_goto) {
      /* Put out vla-dealloc statements on forward gotos to this label. */
      add_vla_dealloc_stmts_for_goto(goto_cfdp, cfdp);
    }  /* for */
#endif /* VLA_DEALLOCATIONS_IN_IL */
  }  /* if */
}  /* define_implicit_label */


static void define_continue_label(void)
/*
Define the "continue" label for the current structured statement,
if it has been used.
*/
{
  a_struct_stmt_stack_entry_ptr  sssep = &struct_stmt_stack[depth_stmt_stack];

  if (sssep->continue_label != NULL) {
    define_implicit_label(sssep->continue_label, sssep->continue_statements);
  }  /* if */
}  /* define_continue_label */


static void expand_struct_stmt_stack(void)
/*
Reallocate the structured statement stack container, copying the contents
of the present one into the new one.  Also reset static variables defining
the size and state of the stack:  size_struct_stmt_stack_container,
struct_stmt_stack_container, and struct_stmt_stack.
*/
{
  sizeof_t  struct_stmt_stack_offset, new_size;

  /* Note that struct_stmt_stack is a pointer into the container.  This
     allows several stacks to coexist, though only that pointed to by
     struct_stmt_stack is currently active.  The offset computed is the
     element count from the start of the container to the start of the
     currently active stack. */
  struct_stmt_stack_offset = (sizeof_t)(struct_stmt_stack -
                                        struct_stmt_stack_container);
  /* Recompute the size of the container. */
  new_size = size_struct_stmt_stack_container +
                                      STRUCT_STMT_STACK_INCREMENTAL_ALLOCATION;
  /* Reallocate the container, copying the old to the new. */
  struct_stmt_stack_container =
                       (a_struct_stmt_stack_entry_ptr)realloc_buffer(
                       (char *)struct_stmt_stack_container,
                       (sizeof_t)(size_struct_stmt_stack_container*
                                            sizeof(a_struct_stmt_stack_entry)),
                       (sizeof_t)(new_size*sizeof(a_struct_stmt_stack_entry)));
  /* Record the size of the container. */
  size_struct_stmt_stack_container = new_size;
  /* Recompute the address of the struct_stmt_stack.  The offset remains the
     the same, but the address of the container has changed. */
  struct_stmt_stack = struct_stmt_stack_container + struct_stmt_stack_offset;
}  /* expand_struct_stmt_stack */


/* Macro to check whether the structured statement stack is large enough to
   accept one more entry and if it is not to reallocate it to a larger size. */
#define ensure_struct_stmt_stack_space()                            	\
  if ((sizeof_t)(struct_stmt_stack -					\
		 struct_stmt_stack_container +                          \
                 depth_stmt_stack + 1) ==                               \
                                          size_struct_stmt_stack_container) { \
    expand_struct_stmt_stack();                                   \
  }  /* if */


void new_struct_stmt_stack(a_struct_stmt_stack_state *saved_state)
/*
Save the state of the current structured statement stack, returning it to
the caller, and create a new structured statement stack.  This is used to
support function definitions nested within function definitions -- a
possibility in C++ with member functions of local classes.  There is no
algorithmic limit on the number of levels of nesting supported.
*/
{
  /* Expand the structured statement stack if necessary. */
  ensure_struct_stmt_stack_space();
  saved_state->container_pos = struct_stmt_stack - struct_stmt_stack_container;
  saved_state->depth_stmt_stack = depth_stmt_stack;
  struct_stmt_stack = &struct_stmt_stack[depth_stmt_stack+1];
  depth_stmt_stack = -1;
  saved_state->code_reachability = curr_reachability;
  saved_state->control_flow_list = control_flow_descr_list;
  saved_state->end_of_control_flow_list = end_of_control_flow_descr_list;
}  /* new_struct_stmt_stack */


void restore_struct_stmt_stack(a_struct_stmt_stack_state *saved_state)
/*
Using state values returned from new_struct_stmt_stack, restore the original
statement stack.
*/
{
#if CHECKING
  if (saved_state->container_pos < 0 ||
      saved_state->container_pos >
                              (a_ptrdiff)size_struct_stmt_stack_container) {
    internal_error(
             "restore_struct_stmt_stack: saved container_pos out of range");
  } else if (saved_state->container_pos + saved_state->depth_stmt_stack >
                                      (int)size_struct_stmt_stack_container) {
    internal_error(
          "restore_struct_stmt_stack: saved depth_stmt_stack out of range");
  }  /* if */
#endif /* CHECKING */  
  struct_stmt_stack = &struct_stmt_stack_container[saved_state->container_pos];
  depth_stmt_stack = saved_state->depth_stmt_stack;
  curr_reachability = saved_state->code_reachability;
  control_flow_descr_list = saved_state->control_flow_list;
  end_of_control_flow_descr_list = saved_state->end_of_control_flow_list;
}  /* restore_struct_stmt_stack */


static void push_stmt_stack_full(a_struct_stmt_kind      kind,
                                 a_statement_ptr         sp,
                                 an_object_lifetime_ptr  olp,
                                 a_boolean               is_statement_expr)
/*
Push an entry onto the structured statement stack, to record that we
are within a structured statement of the indicated kind.  sp points to
the associated il statement.  olp (NULL unless kind is ssk_compound)
points to an object lifetime entry that was expressly created for the
current structured statement.  is_statement_expr is TRUE if this
statement is the top block of a GNU statement expression ({ ... }).
*/
{
  a_struct_stmt_stack_entry_ptr sssep;
  a_control_flow_descr_ptr      cfdp;


  db_enter(4, "push_stmt_stack_full");
  /* Expand the structured statement stack if necessary. */
  ensure_struct_stmt_stack_space();
  /* Push the stack and initialize the new entry. */
  sssep = &struct_stmt_stack[++depth_stmt_stack];
  sssep->kind                 = kind;
  sssep->in_else_of_if        = FALSE;
  sssep->dependent_constexpr_if
                              = FALSE;
  sssep->in_discarded_statement
                              = FALSE;
  sssep->scope_stack_in_discarded_statement_state
                              = FALSE;
  sssep->for_init             = FALSE;
  sssep->is_catch_clause      = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  sssep->parsing_finally_clause = FALSE;
  sssep->in_cleanup_statement_of_microsoft_try = FALSE;
  sssep->in_handler_parameter_declaration = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  sssep->rout_type_explicitly_specified
                              = FALSE;
  sssep->any_exec_statement_seen
                              = FALSE;
  sssep->label_invalidates_curr_block_object_lifetime
                              = FALSE;
  sssep->is_statement_expr    = is_statement_expr;
  sssep->inside_statement_expr= is_statement_expr;
  if (depth_stmt_stack > 0 && (sssep-1)->inside_statement_expr) {
    sssep->inside_statement_expr = TRUE;
  }  /* if */
  sssep->switch_has_dependent_case
                               = FALSE;
  sssep->contains_user_label   = FALSE;
  sssep->contains_active_switch_case = FALSE;
  sssep->record_declared_entities = FALSE;
  sssep->statement             = sp;
  sssep->prefix_attributes     = NULL;
  sssep->switch_max_case_value = NULL;
  sssep->last_switch_case_entry = NULL;
  sssep->last_switch_case_on_sorted_list = NULL;
  sssep->extra_block          = NULL;
  sssep->last_dep_statement   = NULL;
  sssep->break_label          = NULL;
  sssep->break_statements     = NULL;
  sssep->continue_label       = NULL;
  sssep->continue_statements  = NULL;
  sssep->type                 = NULL;
  sssep->curr_block_object_lifetime = olp;
  sssep->p_declared_entities  = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sssep->last_sse_before_expr_decl_disambiguation = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  sssep->depth_of_assoc_scope        = NO_SCOPE_DEPTH;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (depth_stmt_stack == 0) {
    sssep->num_microsoft_trys_inside_of = 0;
  } else {
    sssep->num_microsoft_trys_inside_of =
                                       (sssep-1)->num_microsoft_trys_inside_of;
  }  /* if */
  if (kind == ssk_microsoft_try) sssep->num_microsoft_trys_inside_of++;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  sssep->p_start_pos = NULL;
  sssep->fallthrough_statement = NULL;
#if DEBUG
  if (db_flag_is_set("dump_control_flow")) {
    db_ssse_with_indentation(kind, "pushing ");
  }  /* if */
#endif /* DEBUG */
  if (kind == ssk_compound && !block_stmt_is_cfront_dependent_stmt(sp)) {
    sssep->depth_of_assoc_scope = depth_scope_stack;
  } else if (depth_stmt_stack > 0) {
    /* For statements other than blocks, copy down the any_exec_statement_seen
       flag.  It's really being maintained for the block containing this
       non-block statement, and it gets copied back up at the end of the
       statement. */
    sssep->any_exec_statement_seen = sssep[-1].any_exec_statement_seen;
  }  /* if */
  sssep->start_reachable      = curr_reachability;
  set_unreachable(sssep->end_reachable);  /* So far. */
  if (kind == ssk_while || kind == ssk_do || kind == ssk_for ||
#if MICROSOFT_EXTENSIONS_ALLOWED
      kind == ssk_for_each ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      kind == ssk_range_based_for) {
    /* The bodies of loops are reachable in that the bottom can branch to
       the top. */
    set_reachable(curr_reachability);
  } else if (kind == ssk_switch) {
    /* The body of a switch is not reachable until a case or default label
       appears.  However, set_unreachable is not called until after the
       condition declaration, if any, is scanned. */
  } else if (kind == ssk_compound) {
    /* Represent this compound statement by adding a block entry to the
       control_flow_descr_list. */
    cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_block);
    if (!C_mode()) {
      /* Set the lifetime in the control flow entry. */
      cfdp->variant.block.object_lifetime = olp;
      if (depth_stmt_stack > 0) {
        /* Special case processing for C++ mode only. */
        a_scope_ptr  scope = scope_stack[depth_scope_stack].il_scope;
        if (sssep->depth_of_assoc_scope == NO_SCOPE_DEPTH) {
          /* Cfront dependent statement. */
        } else if (scope != NULL && scope->kind == (a_scope_kind)sck_block &&
                   scope->variant.assoc_handler != NULL) {
          /* This block represents the compound statement immediately within a
             catch clause. */
          cfdp->variant.block.is_catch_block = TRUE;
          cfdp->variant.block.is_within_goto_protected_block = TRUE;
          sssep->is_catch_clause = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (sssep->kind == (a_struct_stmt_kind)ssk_compound &&
                   sssep[-1].kind == (a_struct_stmt_kind)ssk_try_block &&
                   sssep[-1].parsing_finally_clause) {
          /* This block represents the compound statement immediately within
             the C++/CLI finally clause of a try block. */
          cfdp->variant.block.is_finally_block = TRUE;
          cfdp->variant.block.is_within_goto_protected_block = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else if (sssep[-1].kind == (a_struct_stmt_kind)ssk_try_block) {
          /* This block represents the compound statement immediately within a
             try block statement. */
          cfdp->variant.block.is_try_block = TRUE;
          cfdp->variant.block.is_within_goto_protected_block = TRUE;
        } else if (sssep[-1].kind == (a_struct_stmt_kind)ssk_if &&
                   (sssep[-1].statement->kind ==
                                   (a_statement_kind)stmk_if_consteval ||
                    sssep[-1].statement->kind ==
                                   (a_statement_kind)stmk_if_not_consteval)) {
          /* This block represents a compound statement immediately within an
             "if consteval" or "if not "consteval" statement. */
          cfdp->variant.block.is_within_goto_protected_block = TRUE;
          cfdp->variant.block.is_if_consteval_branch = TRUE;
        } else if (sssep[-1].kind == (a_struct_stmt_kind)ssk_constexpr_if) {
          /* This block represents the compound statement immediately within a
             constexpr if. */
          cfdp->variant.block.is_constexpr_if = TRUE;
          cfdp->variant.block.is_within_goto_protected_block = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Check for a Microsoft __try block in both C and C++ mode.  The control
       flow entries are marked as for ordinary C++ try blocks, and the
       diagnostic on branching into one will also be the same.   Note that
       the guarded statement and the cleanup statement are both treated as
       "try blocks" for the purposes of this checking. */
    if (depth_stmt_stack > 0 &&
        sssep[-1].kind == (a_struct_stmt_kind)ssk_microsoft_try) {
      /* This block represents the compound statement immediately within a
         try block statement. */
      cfdp->variant.block.is_try_block = TRUE;
      cfdp->variant.block.is_within_goto_protected_block = TRUE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (is_statement_expr) {
      /* Set flags for a GNU statement expression, ({ ... }). */
      cfdp->variant.block.is_statement_expr = TRUE;
      cfdp->variant.block.is_within_goto_protected_block = TRUE;
    }  /* if */
    add_to_control_flow_descr_list(cfdp);
  }  /* if */
  db_exit();
}  /* push_stmt_stack_full */


/*
Interface to push_stmt_stack_full for the usual case.
*/
#define push_stmt_stack(kind, sp, olp) \
  push_stmt_stack_full((kind), (sp), (olp), /*is_statement_expr=*/FALSE)


static void end_stmt_sequence(a_struct_stmt_stack_entry_ptr sssep)
/*
End a statement sequence under a structured statement, i.e., clear
the flags used by add_statement to add statements at the end of a
sequence.
*/
{
  sssep->extra_block        = NULL;
  sssep->last_dep_statement = NULL;
}  /* end_stmt_sequence */


static void start_stmt_clause(a_struct_stmt_stack_entry_ptr sssep)
/*
Start a new clause of a structured statement.  sssep points to the
struct_stmt_stack entry for the structured statement (since it need not
be the topmost one).  A call of this routine implies that the current
position in the program can be branched to from the statement that
begins the indicated structured statement.
*/
{
  /* The start of a clause is reachable if the start of the structured
     statement is reachable. */
  curr_reachability = sssep->start_reachable;
}  /* start_stmt_clause */


static void term_stmt_clause(a_struct_stmt_stack_entry_ptr sssep)
/*
end the current clause of a structured statement.  sssep points to the
struct_stmt_stack entry for the structured statement.  A call of this
routine implies that the current position in the program branches
to the end of the indicated structured statement.
*/
{
  /* If the end of the clause is reachable, then the end of the whole
     structured statement is reachable. */
  merge_reachability(&curr_reachability, &sssep->end_reachable);
  end_stmt_sequence(sssep);
}  /* term_stmt_clause */


static a_boolean is_true_constant_expr(an_expr_node_ptr expr)
/*
Return TRUE if the indicated expression has a constant value that is true.
The safe answer, if the truth cannot be discovered, is FALSE.
*/
{
  a_boolean is_true_constant;

  expr = skip_parens(expr);
  while (is_operation_node(expr)) {
    if ( node_operator_is(expr, eok_bool_cast)) {
      expr = skip_parens(expr->variant.operation.operands);
    } else if (node_operator_is(expr, eok_comma)) {
      expr = skip_parens(expr->variant.operation.operands->next);
#if BUILTIN_FUNCTIONS_ENABLED
    } else if (is_call_node(expr)) {
      a_routine_ptr  rp = routine_from_function_expr(
                                            expr->variant.operation.operands);
      if (rp != NULL && is_gnu_builtin_function(rp)) {
        a_builtin_function_kind  bfk = rp->variant.builtin_function_kind;
        if (bfk == (a_builtin_function_kind)bfk_expect ||
            bfk == (a_builtin_function_kind)bfk_expect_with_probability) {
          expr = skip_parens(expr->variant.operation.operands->next);
        } else {
          break;
        }  /* if */
      } else {
        break;
      }  /* if */
#endif  /* BUILTIN_FUNCTIONS_ENABLED */
    } else {
      break;
    }  /* if */
  }  /* while */
  is_true_constant = (is_constant_node(expr) &&
                      constant_bool_value_known_at_compile_time(
                                                       node_constant(expr)) &&
                      !is_false_constant(node_constant(expr)));
  return is_true_constant;
}  /* is_true_constant_expr */


static a_boolean is_infinite_loop(a_statement_ptr stmt)
/*
Return TRUE if the indicated statement is an infinite loop.  The safe answer,
if the truth cannot be discovered, is FALSE.
*/
{
  a_boolean        is_inf_loop = FALSE;
  an_expr_node_ptr expr;

  /* The test for an enhanced-for (i.e., a for-each or range-based-for) loop
     varies depending on the type of enhanced-for loop that has been detected.
     For array iteration, we're assured that the loop is finite; for other
     types various expressions could be examined, but the expressions all
     involve calls of some sort and since they haven't been inlined yet, this
     code is too simplistic to determine whether the call would always return
     true.  Therefore, assume enhanced-for loops are never infinite. */
  if (stmt->kind == (a_statement_kind)stmk_while ||
      stmt->kind == (a_statement_kind)stmk_end_test_while ||
#if UPC_EXTENSIONS_ALLOWED
      stmt->kind == (a_statement_kind)stmk_upc_forall ||
#endif /* UPC_EXTENSIONS_ALLOWED */
      stmt->kind == (a_statement_kind)stmk_for) {
    expr = stmt->expr;
    /* In the "for" loop, the expression can be NULL and that implies an
       infinite loop. */
    if (expr == NULL) {
      is_inf_loop = TRUE;
    } else if (is_true_constant_expr(expr)) {
      /* Loop expression is a non-zero constant: it's an infinite loop. */
      is_inf_loop = TRUE;
    }  /* if */
  }  /* if */
  return(is_inf_loop);
}  /* is_infinite_loop */


static void terminate_curr_block_object_lifetime(
                                      a_struct_stmt_stack_entry_ptr  sssep)
/*
sssep represents a compound statement that either has completed or has an
invalidated object lifetime because of the appearance of a label statement.
Do final processing on the object lifetime -- that is, if it is going to be
retained in the IL, bind it to an IL entity.
*/
{
  an_object_lifetime_ptr  olp;

  /* Set olp to point to the object lifetime that is currently active for
     the block associated with *sssep. */
  olp = sssep->curr_block_object_lifetime;
  if (olp != NULL && olp->kind == (an_object_lifetime_kind)olk_block &&
      olp->entity.ptr == NULL && !is_useless_object_lifetime(olp)) {
    /* olp is still unbound but it has a destructions list.  It will be
       retained in the IL, so it needs to be bound to an IL entry -- either a
       scope or a block. */
#if CHECKING
    a_statement_ptr  sp = sssep->statement;

    check_assertion_str2(olp == curr_object_lifetime,
                         "terminate_curr_block_object_lifetime:",
                         "not at top of lifetime stack");
    check_assertion_str2(sp->kind == (a_statement_kind)stmk_block,
                         "terminate_curr_block_object_lifetime:",
                         "expected a block statement");
    check_assertion_str2(!block_stmt_is_cfront_dependent_stmt(sp),
                         "terminate_curr_block_object_lifetime:",
                         "cfront dependent statement not expected");
#endif /* CHECKING */
    /* There is a scope associated with this block; bind the lifetime
       directly to it. */
    (void)ensure_il_scope_exists(&scope_stack[depth_scope_stack]);
    check_assertion_str2(olp->entity.ptr != NULL,
                         "terminate_curr_block_object_lifetime:",
                         "scope stack out of sync with struct stmt stack");
  }  /* if */
}  /* terminate_curr_block_object_lifetime */


static void reset_curr_block_object_lifetime(a_statement_ptr sp)
/*
If the current structured statement stack entry represents a compound
statement in which a label has appeared that "invalidates" the object
lifetime of the current block, do final processing on the invalidated object
lifetime and create a new one (of kind olk_block_after_label) to run for the
rest of this compound statement.  This handles cases in which the label
appears in the same block or in a nested block.  For example:

    {                           // start of outer block
      <olk_block>
      {                         // start of inner block
        <olk_block>
    L:                          // "invalidates" both olk_block lifetimes
        <olk_block_after_label>
      }                         // resume outer block
      <olk_block_after_label>
    }

This routine is called both at label statements and immediately after a
structured statement terminates (i.e., in the context of the block just
resumed).  sp indicates the statement that starts the new object lifetime
(e.g., an stmk_label statement).
*/
{
  a_struct_stmt_stack_entry_ptr  sssep = &struct_stmt_stack[depth_stmt_stack];
  
  if (sssep->kind == (a_struct_stmt_kind)ssk_compound &&
      sssep->label_invalidates_curr_block_object_lifetime) {
    /* Do any final processing required before eclipsing the previously
       active object lifetime with a new one. */
    terminate_curr_block_object_lifetime(sssep);
    /* Push the object lifetime and set the struct-stmt-stack entry to point
       to it. */
    push_object_lifetime((an_il_entry_kind)iek_statement, (char *)sp,
                         (an_object_lifetime_kind)olk_block_after_label);
    sssep->curr_block_object_lifetime = curr_object_lifetime;
    sssep->label_invalidates_curr_block_object_lifetime = FALSE;
  }  /* if */
}  /* reset_curr_block_object_lifetime */


static void pop_stmt_stack(void)
/*
Pop the top entry off the structured statement stack, recording that
a structured statement has ended.
*/
{
  a_struct_stmt_stack_entry_ptr          sssep;
  a_struct_stmt_kind                     kind;
  a_statement_ptr                        sp;
  a_label_ptr                            break_label;
  a_control_flow_descr_ptr               break_statements;
  
  db_enter(4, "pop_stmt_stack");
  sssep = &struct_stmt_stack[depth_stmt_stack];
  kind = sssep->kind;
  sp = sssep->statement;
  /* Close the final clause of the statement. */
  term_stmt_clause(sssep);
  /* Determine whether or not the code following the statement is reachable,
     and set curr_reachability appropriately. */
  if (kind == ssk_while || kind == ssk_for || kind == ssk_do ||
#if MICROSOFT_EXTENSIONS_ALLOWED
      kind == ssk_for_each ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      kind == ssk_range_based_for) {
    /* A loop. */
    if (is_infinite_loop(sp)) {
      /* An infinite loop.  The code after the loop is not reachable. */
      set_unreachable(curr_reachability);
    } else if (kind == ssk_while || kind == ssk_for ||
#if MICROSOFT_EXTENSIONS_ALLOWED
               kind == ssk_for_each ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
               kind == ssk_range_based_for) {
      /* A top-test loop.  The code after the loop is reachable if the current
         location is reachable or if the start of the loop is reachable. */
      merge_reachability(&sssep->start_reachable, &curr_reachability);
    } else {
      /* A bottom-test loop.  The code after the loop is reachable if the
         current location is reachable. */
    }  /* if */
    if (sssep->fallthrough_statement != NULL) {
      /* A fallthrough statement was the last item in the loop. */
      pos_diagnostic(clang_mode ? es_error :
                                  strict_ansi_discretionary_severity,
                     ec_fallthrough_must_precede_switch_case,
                     &sssep->fallthrough_statement->position);
    }  /* if */
  } else {
    /* Non-loop statement. */
    if (kind == ssk_switch) {
      if (sssep->statement->variant.switch_stmt.extra_info->default_case
                                                                    == NULL) {
        /* Switch statement without a default.  If the initial statement
           can be reached, the end can be reached. */
        merge_reachability(&sssep->start_reachable, &sssep->end_reachable);
      } else {
        /* Switch statement with a default.  If the end of the default clause
           is reachable (curr_reachability), then the end of the switch can
           be reached. */
        merge_reachability(&curr_reachability, &sssep->end_reachable);
      }  /* if */
      if (sssep->fallthrough_statement != NULL) {
        /* A fallthrough statement was the last item in the switch. */
        pos_diagnostic(clang_mode ? es_error :
                                    strict_ansi_discretionary_severity,
                       ec_fallthrough_must_precede_switch_case,
                       &sssep->fallthrough_statement->position);
      }  /* if */
    } else if (kind == ssk_if && sp->variant.if_stmt.else_statement == NULL &&
               (sp->expr == NULL || !is_true_constant_expr(sp->expr))) {
      /* If without an else, except "if (1) ...".  If the initial statement
         can be reached, the end can be reached. */
      merge_reachability(&sssep->start_reachable, &sssep->end_reachable);
    } else if (kind == ssk_constexpr_if &&
               sp->variant.constexpr_if->else_statement == NULL &&
               !is_true_constant_expr(sp->expr)) {
      /* If without an else, except "if constexpr (1) ...".  If the initial
         statement can be reached, the end can be reached. */
      merge_reachability(&sssep->start_reachable, &sssep->end_reachable);
    }  /* if */
    /* The code after the statement can be reached if the end of the statement
       can be reached. */
    curr_reachability = sssep->end_reachable;
  }  /* if */
  if (depth_stmt_stack > 0) {
    /* If the statement just exited is a non-block, propagate the
       any_exec_statement_seen flag upwards. */
    if (kind != ssk_compound || block_stmt_is_cfront_dependent_stmt(sp)) {
      sssep[-1].any_exec_statement_seen = sssep->any_exec_statement_seen;
    }  /* if */
    /* Propagate the contains_user_label and contains_active_switch_case flags
       upwards if appropriate. */
    sssep[-1].contains_user_label |= sssep->contains_user_label;
    if (kind != ssk_switch) {
      sssep[-1].contains_active_switch_case |=
                                           sssep->contains_active_switch_case;
    }  /* if */
  }  /* if */
  if (kind == ssk_compound) {
    /* When this compound statement was pushed onto the statement stack, a
       block entry was added to the control_flow_descr_list.  Now add an
       end-of-block entry to close the block off. */
    add_to_control_flow_descr_list(
       alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_end_of_block));
    /* This is the end of a compound statement.  Be sure the object lifetime
       is properly bound to an IL entry. */
    if (sssep->curr_block_object_lifetime != curr_object_lifetime) {
      check_assertion_str2(sssep->curr_block_object_lifetime->kind ==
                                (an_object_lifetime_kind)olk_block_after_label,
                           "pop_stmt_stack:",
                           "bad kind for curr_block_object_lifetime");
      sssep->curr_block_object_lifetime = curr_object_lifetime;
    }  /* if */
    terminate_curr_block_object_lifetime(sssep);
    if (depth_stmt_stack > 0) {
      /* If a fallthrough statement was last in this block, propagate that
         information upwards. */
      sssep[-1].fallthrough_statement = sssep->fallthrough_statement;
    }  /* if */
  }  /* if */
  break_label = sssep->break_label;
  break_statements = sssep->break_statements;
#if DEBUG
  if (db_flag_is_set("dump_control_flow")) {
    db_ssse_with_indentation(kind, "popping ");
  }  /* if */
#endif /* DEBUG */
  /* Pop the stack. */
  depth_stmt_stack--;
  /* If the break label for this statement was referenced, generate 
     its definition now.  This must be done after depth_stmt_stack is
     decremented so that the label will appear outside the structured
     statement.  It must also be done after curr_reachability has been
     adjusted. */
  if (break_label != NULL) {
    a_reachability_summary  saved_reachability;
    /* Save the reachability because a label is assumed reachable by default
       when calling define_label, but for switch break labels the current
       reachability is correct. */
    saved_reachability = curr_reachability;
    check_assertion(depth_stmt_stack > -1);
    define_implicit_label(break_label, break_statements);
    if (break_label->switch_break_label) {
      curr_reachability = saved_reachability;
    }  /* if */
  }  /* if */
  db_exit();
}  /* pop_stmt_stack */


static void asm_statement(void)
/*
Scan a C++ asm statement.  Its form is

	asm ( "string" ) ;

This is accepted as an extension in non-strict, non-Microsoft C modes.
In Microsoft C modes, the variant using "_asm" or "__asm" is accepted instead.
In strict C mode, the variant using "__asm" is accepted.
*/
{
  a_statement_ptr    sp;
  a_source_position  asm_pos;
  an_asm_entry_ptr   asm_entry;
  a_boolean          asm_decl_allowed;
  db_enter(3, "asm_statement");

  check_for_unreachable_code();
  /* Later versions of MSVC don't allow asm declarations in lambda
     expressions. */
  asm_decl_allowed = !microsoft_mode || ms_version_is(<1916) ||
                     !in_lambda_body();
  asm_pos = pos_curr_token;
  /* Note: process_curr_construct_pragmas is intentionally not called.  Also,
     asm_declaration is called before adding a statement entry, to avoid
     attaching any prefix attributes (which are not valid here). */
  asm_entry =
      asm_declaration(asm_decl_allowed, /*is_asm_statement=*/TRUE,
                      &struct_stmt_stack[depth_stmt_stack].prefix_attributes);
  /* Allocate the statement. */
  sp = add_statement_at_stmt_pos(stmk_asm, &asm_pos,
                                 /*compiler_generated=*/FALSE);
  stmt_update_source_sequence_list(sp);
  sp->variant.asm_entry = asm_entry;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sp->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  db_exit();
}  /* asm_statement */


static a_struct_stmt_stack_entry_ptr find_enclosing_struct_stmt(
                                                     a_boolean find_switch,
                                                     a_boolean find_loop)
/*
Search the structured statement stack from the current entry outward,
looking for a switch statement (if find_switch is TRUE) or a loop
statement (while, do, or for; if find_loop is TRUE).  Return a pointer
to the first structured statement stack entry found, or NULL if none 
was found.
*/
{
  a_struct_stmt_stack_entry_ptr sssep;
  a_struct_stmt_kind            kind;

  sssep = &struct_stmt_stack[depth_stmt_stack];
  /* Note that the loop never looks at entry [0], since that is for
     the compound statement that defines the function. */
  while (sssep != &struct_stmt_stack[0]) {
    kind = sssep->kind;
    if (find_switch && kind == ssk_switch) {
      goto found;
    } else if (find_loop &&
               (kind == ssk_while || kind == ssk_do || kind == ssk_for ||
                kind == ssk_range_based_for
#if MICROSOFT_EXTENSIONS_ALLOWED
                || kind == ssk_for_each
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
              )) {
      goto found;
    }  /* if */
    /* Keeping looking at entries in the structured statement stack. */
    sssep--;
  }  /* while */
  /* No structured statement matching the criteria was found. */
  sssep = NULL;
found:
  return(sssep);
}  /* find_enclosing_struct_stmt */


a_boolean in_switch_statement(void)
/*
Returns TRUE if there is an enclosing "switch" statement at the current
location in the statement stack.
*/
{
  return find_enclosing_struct_stmt(/*find_switch=*/TRUE,
                                    /*find_loop=*/FALSE) != NULL;
}  /* in_switch_statement */


static a_statement_ptr start_block_statement(
                                  a_boolean              generated_statement,
                                  a_boolean              is_statement_expr,
                                  an_object_lifetime_ptr function_try_lifetime)
/*
Do processing to begin a block or compound statement.  Return a pointer
to the block statement.  generated_statement is TRUE if the block is
generated, e.g., to surround a dependent statement in C++ or C99.
is_statement_expr is TRUE if the statement is the top of a GNU
statement expression, ({ ... }).  If function_try_lifetime is
non-NULL, it points to an object lifetime preallocated for the
block under the "try" in a function try block.
*/
{
  a_struct_stmt_kind      kind;
  a_statement_ptr         block_stmt;

  if (is_statement_expr) {
    /* A GNU statement expression.  Do not link the statement into
       the current statement on the statement stack. */
    block_stmt = alloc_statement(stmk_block, generated_statement);
    block_stmt->variant.block.extra_info->is_statement_expression = TRUE;
    block_stmt->position = pos_curr_token;
  } else {
    /* Allocate a block statement and add it to the statements list. */
    block_stmt = add_statement_at_stmt_pos(stmk_block,
                                           generated_statement ?
                                                       &null_source_position :
                                                       &pos_curr_token,
                                           generated_statement);
  }  /* if */
  stmt_update_source_sequence_list(block_stmt);
  if (!generated_statement) {
    /* This is a block statement introduced by a left brace (which should be
       the next token). */
    /* Process any pragmas that are meant to bind to the block statement as
       a whole. */
    if (is_statement_expr) {
      /* If there are any pragmas on the list, they are for the statement that
         contains this GNU statement expression and will be processed later. */
    } else {
      process_curr_construct_pragmas((a_symbol_ptr)NULL, block_stmt);
    }  /* if */
  } else {
    /* This is a dependent statement with no surrounding braces.  Since the
       block statement is generated by the compiler, clear the position
       field. */
    block_stmt->position = null_source_position;
    /* Any pragmas that are current will bind to the statement itself, not
       to the generated block, so don't process them yet. */
  }  /* if */
  if (generated_statement && cfront_2_1_mode) {
    /* This is a dependent statement in cfront 2.1 mode, which is special
       in that no scope is created for it, but it nevertheless has an
       associated object lifetime: anything constructed within the
       statement must also be destroyed therein. */
    push_object_lifetime(iek_block,
                         (char *)(block_stmt->variant.block.extra_info),
                         (an_object_lifetime_kind)olk_block);
  } else {
    /* Push an associated scope.  This does not allocate the IL scope yet. */
    push_block_scope_with_lifetime(function_try_lifetime);
    if (is_statement_expr) {
      /* The previous call sets scope_stack_top().il_memory_region to the
         region of the enclosing (local) scope.  However, statement expressions
         can appear in local classes where the enclosing (class) scope is
         associated with file scope memory, but the memory region for the
         statement expression (already reflected in curr_il_region_number)
         is that of the enclosing function. */
      scope_stack_top().il_memory_region = curr_il_region_number;
    }  /* if */
    if (depth_stmt_stack >= 0) {
      /* Set appropriate flags in the scope stack entry. */
      kind = struct_stmt_stack[depth_stmt_stack].kind;
      if (kind == ssk_while || kind == ssk_do || kind == ssk_for ||
          kind == ssk_range_based_for
#if MICROSOFT_EXTENSIONS_ALLOWED
          || kind == ssk_for_each
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                 ) {
        scope_stack[decl_scope_level].is_loop_scope = TRUE;
      } else if (kind == ssk_try_block) {
        scope_stack[decl_scope_level].is_try_block = TRUE;
        scope_stack[decl_scope_level].within_try_block = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Push an entry on the structured statement stack. */
  push_stmt_stack_full(ssk_compound, block_stmt, curr_object_lifetime,
                       is_statement_expr);
  return block_stmt;
}  /* start_block_statement */


static void finish_block_statement(a_statement_ptr block_stmt)
/*
Do processing to finish a block or compound statement.  block_stmt points to
the block statement.
*/
{
  a_block_ptr block = block_stmt->variant.block.extra_info;
  a_boolean   is_statement_expression = block->is_statement_expression;

  /* Remember whether or not the end of the block is reachable.  This
     is helpful in IL lowering. */
  block->end_of_block_reachable = curr_reachability.reachable;
#if VLA_DEALLOCATIONS_IN_IL
  if (vla_enabled && vla_deallocations_in_il &&
      curr_reachability.reachable) {
    /* Put out a vla-dealloc statement for each declaration of a VLA variable
       in the current block. */
    add_vla_dealloc_stmts_for_block(end_of_control_flow_descr_list);
  }  /* if */
#endif /* VLA_DEALLOCATIONS_IN_IL */
  /* Pop the statement stack. */
  pop_stmt_stack();
  if (block_stmt_is_cfront_dependent_stmt(block_stmt)) {
    /* cfront mode dependent statement. */
    (void)pop_object_lifetime();
  } else {
    /* Store the IL scope pointer in the block.  This is usually NULL for
       blocks with no declarations. */
    a_scope_ptr              scope_ptr;
    a_scope_stack_entry_ptr  ssep = &scope_stack[decl_scope_level];
    an_object_lifetime_ptr   curr_scope_olp, prev_scope_olp = NULL;
    if (is_statement_expression) {
      curr_scope_olp = ssep->curr_scope_object_lifetime;
      prev_scope_olp = ssep->saved_curr_object_lifetime;
      if (curr_scope_olp != NULL &&
          curr_scope_olp->block_lifetime_with_label_or_goto) {
        /* For statement expressions containing a label or goto, we want to be
           sure the scope is represented in the IL so the block object lifetime
           can be bound to it. */
        (void)ensure_il_scope_exists(ssep);
      }  /* if */
    }  /* if */
    scope_ptr = ssep->il_scope;
    if (scope_ptr != NULL) {
      block->assoc_scope = scope_ptr;
      scope_ptr->assoc_block = block_stmt;
    }  /* if */
    /* Pop the name scope. */
    pop_scope();
    if (prev_scope_olp != NULL) {
      curr_object_lifetime = prev_scope_olp;
    }  /* if */
  }  /* if */
  /* If a label appeared in the context of the block that was just
     terminated, it may be appropriate to push a new object lifetime for
     the scope being resumed.  This is not the case for GNU statement
     expressions since we cannot jump into their associated block. */
  if (depth_stmt_stack >= 0) {
    if (!is_statement_expression) {
      reset_curr_block_object_lifetime(block_stmt);
    }  /* if */
  }  /* if */
}  /* finish_block_statement */


static void dependent_statement(void)
/*
Scan the dependent statement of an if, switch, while, do-while, for,
"for each", or range-based-for statement.  In C++ and C99, such a dependent
statement implicitly defines a local scope.
*/
{
  a_boolean         block_added;
  a_statement_ptr   block = NULL;

  db_enter(3, "dependent_statement");
  /* In C++ and C99, add a block (and potential scope).  Do not do so, however,
     if a block will be created anyway. */
  if ((C_mode() && !c99_mode) || curr_token == tok_lbrace) {
    block_added = FALSE;
  } else {
    /* Normal case (in C++ and C99): add a block and potential scope.
       In cfront mode, the block is added but not the scope. */
    block = start_block_statement(/*generated_statement=*/TRUE,
                                  /*is_statement_expr=*/FALSE,
                                  (an_object_lifetime_ptr)NULL);
    block_added = TRUE;
  }  /* if */
  /* Now process the dependent statement itself. */
  statement(/*is_dependent_statement=*/TRUE,
            /*marked_as_gnu_extension=*/FALSE);
  if (block_added) {
    finish_block_statement(block);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Add a source sequence entry marking the end of the compiler-generated
       block that was added to surround the dependent statement. */
    add_end_of_construct_source_sequence_entry((char *)block, iek_statement);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  db_exit();
}  /* dependent_statement */


static void record_condition_initializations(an_il_entity_list_entry  *entry,
                                             a_statement_ptr          sp)
/*
The given (possibly NULL) list of entities are being declared as part of the
given statement, which is associated with a declaration in a condition (see
scan_structured_control_value).  For each of those entities that is a variable
with a dynamic initialization, record a cfdk_init control flow description that
records that initialization (so attempts to jump over them will elicit an
error).
*/
{
  for  (; entry != NULL; entry = entry->next) {
    if (entry->entity.kind == iek_variable) {
      a_variable_ptr  vp = (a_variable_ptr)entry->entity.ptr;
      if (vp->init_kind == (an_init_kind)initk_dynamic) {
        a_control_flow_descr_ptr  cfdp = alloc_control_flow_descr(
                                         (a_control_flow_descr_kind)cfdk_init);
        cfdp->variant.init.statement = sp;
        cfdp->variant.init.variable = vp;
        cfdp->variant.init.in_statement_expression =
                                                 inside_statement_expression();
        add_to_control_flow_descr_list(cfdp);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* record_condition_initializations */


STATIC_THREAD a_boolean
		already_diagnosed_selection_initializer;
			/* Flag indicating whether a non-standard selection
			   initializer has already been diagnosed. */


static void scan_structured_control_value(a_statement_ptr    sp,
                                          an_init_component  *cached_expr)
/*
Scan a C++ control value (a boolean control value or a switch value) for the
given statement that is more than just an expression.  In C++, the following
additional forms are possible (shown with "if" for simplicity):

    if (<declaration>) ...
    if (<declaration>; <expression>) ... 
    if (<expression>; <declaration>) ... 
    if (<expression>; <expression>) ... 

The latter three forms are a C++17 feature (enabled when the global variable
selection_initializers_enabled is TRUE) that only applies to "if" and "switch"
statements.  For the latter two forms, the first expression has already been
scanned into *cached_expr.

Scan the remainder of the construct.  This includes creating an sck_condition
scope and an enk_condition node (the node is attached to sp).
*/
{
  an_expr_node_ptr               node, value_expr;
  a_variable_ptr                 vp;
  a_scope_ptr                    scope;
  a_boolean                      initializer_scanned = FALSE;
  a_boolean                      is_switch_expr =
                                  (sp->kind == (a_statement_kind)stmk_switch);
  a_control_flow_descr_ptr       cfdp;
  a_decl_parse_state             dps;
  a_struct_stmt_stack_entry_ptr  sssep = &struct_stmt_stack_top();
  an_il_entity_list_entry_ptr    entity_list = NULL;

  /* Push the new scope, and bind the if, switch, for, or while statement to
     it. */
  scope = push_scope((a_scope_kind)sck_condition, NO_SCOPE_NUMBER,
                     (a_type_ptr)NULL, (a_routine_ptr)NULL);
  scope->variant.assoc_statement = sp;
  /* Add a control flow entry to represent the condition block. */
  cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_block);
  cfdp->source_pos = pos_curr_token;
  cfdp->variant.block.object_lifetime = curr_object_lifetime;
  add_to_control_flow_descr_list(cfdp);
  /* Allocate an expression node indicating that this is a condition
     declaration. */
  node = alloc_expr_node((an_expr_node_kind)enk_condition);
  node->variant.condition->scope = scope;
  sp->expr = node;
  if (cached_expr != NULL) {
    a_statement_ptr  expr_sp = alloc_statement(stmk_expr,
                                               /*compiler_generated=*/FALSE);
    expr_sp->position = *init_component_pos(cached_expr);
    expr_sp->expr = scan_void_expression(/*repeated_in_loop=*/FALSE,
                                         /*marked_as_gnu_extension=*/FALSE,
                                         /*is_statement_expr=*/FALSE,
                                         (a_dynamic_init**)NULL, cached_expr);
    node->variant.condition->initialization = expr_sp;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (!source_sequence_entries_disallowed) {
      stmt_update_source_sequence_list(expr_sp);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    initializer_scanned = TRUE;
  } else if (curr_token == tok_semicolon && selection_initializers_enabled) {
    /* An "empty initializer" statement. */
    initializer_scanned = TRUE;
  } else {
    /* Scan the upcoming declaration.  In C++17, this may be an initialization
       statement (terminated by a semicolon).  Otherwise, it is a condition
       declaration which should be a single variable with an initializer. */
    struct_stmt_stack_top().record_declared_entities = TRUE;
    struct_stmt_stack_top().p_declared_entities = &entity_list;
    init_decl_parse_state(&dps);
    dps.keep_terminating_token = TRUE;
    scan_nonmember_declaration(&dps, (a_source_range*)NULL);
    /* Re-load sssep since the call to scan_nonmember_declaration may have
       caused the statement stack to be reallocated. */
    sssep = &struct_stmt_stack_top();
    if (curr_token == tok_semicolon && selection_initializers_enabled &&
        (sp->kind == (a_statement_kind)stmk_if ||
         sp->kind == (a_statement_kind)stmk_constexpr_if ||
         is_switch_expr) &&
        cached_expr == NULL) {
      /* The declaration we just scanned is a C++17-style initializer for a
         selection statement. */
      a_statement_ptr  decl_sp = alloc_statement(stmk_decl,
                                                 /*compiler_generated=*/FALSE);
      decl_sp->position = dps.start_pos;
      node->variant.condition->initialization = decl_sp;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (!source_sequence_entries_disallowed) {
        record_sse_for_decl_statement(decl_sp);
        /* Add a source sequence entry marking the end of the declaration.
           This marker is necessary in case what immediately follows in the
           source sequence list is an entry for a condition declaration.
           E.g., without the marker, there would be no source sequence entry
           distinction between 
             if (int i = 0; int j = 3);
           and
             if (int i = 0, j = 3; j);
         */
        add_end_of_construct_source_sequence_entry((char*)decl_sp,
                                                   iek_statement);
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      decl_sp->variant.decl.entities = *sssep->p_declared_entities;
      end_potential_decl_statement();
      initializer_scanned = TRUE;
      if (dps.specifiers_type == NULL) {
        /* If dps.specifiers_type is NULL, the declaration we just scanned
           was not a "simple-declaration" (i.e., a declaration consisting of
           some optional attributes, followed by decl-specifiers, and
           optionally followed by a declarator) nor an alias-declaration. */
        pos_error(ec_invalid_init_statement, &dps.start_pos);
      }  /* if */
    }  /* if */
    record_condition_initializations(entity_list, sp);
    struct_stmt_stack_top().record_declared_entities = FALSE;
    struct_stmt_stack_top().p_declared_entities = NULL;
  }  /* if */
  if (initializer_scanned) {
    /* A C++17-style initializer was scanned (expression or declaration).
       The current token is a semicolon: Skip past it and determine if what
       follows is an ordinary expression or a condition declaration. */
    a_disambig_flag_set  flags = DFS_REAL_DECLARATOR_ALLOWED |
                                 DFS_IS_CONDITION;
    check_assertion(curr_token == tok_semicolon);
    if (!cpp17_mode && gpp_mode) {
      if (!already_diagnosed_selection_initializer && !in_system_header()) {
        pos_warning(ec_selection_initializer_nonstandard, &pos_curr_token);
        already_diagnosed_selection_initializer = TRUE;
      }  /* if */
    }  /* if */
    (void)get_token();
    start_potential_decl_statement(&entity_list);
    if (is_decl_not_expr(flags)) {
      init_decl_parse_state(&dps);
      dps.keep_terminating_token = TRUE;
      scan_nonmember_declaration(&dps, (a_source_range*)NULL);
      record_condition_initializations(entity_list, sp);
      end_potential_decl_statement();
    } else {
      end_potential_decl_statement();
      /* Scan the controlling expression. */
      if (is_switch_expr) {
        value_expr = scan_integer_expression(is_switch_expr,
                                             (an_init_component*)NULL);
      } else {
        value_expr = scan_boolean_controlling_expression(
                                                    (an_init_component*)NULL);
      }  /* if */
      goto done;
    }  /* if */
  }  /* if */
  vp = check_condition_declaration(&dps);
  if (vp->init_kind == (an_init_kind)initk_dynamic) {
    node->variant.condition->dynamic_init = vp->initializer.dynamic;
  }  /* if */
  /* The condition is an expression that represents the value of the
     initialized variable, converted if necessary. */
  value_expr = make_condition_value_expression(vp, is_switch_expr);
done:
  /* Copy the type of the variable expression into the condition node (since
     all expression nodes need to have a type). */
  node->variant.condition->expr = value_expr;
  node->type = value_expr->type;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* Record the source range for the condition declaration. */
  node->expr_range.start = cfdp->source_pos;
  node->expr_range.end = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* scan_structured_control_value */


static void finish_condition_block(void)
/*
Do processing required upon completion of a condition "block".
*/
{
  db_enter(3, "finish_condition_block");
  /* Terminate the control flow block that was started when the condition
     block was started. */
  add_to_control_flow_descr_list(
       alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_end_of_block));
  /* Pop the sck_condition scope. */
  pop_scope();
  db_exit();
}  /* finish_condition_block */


static a_scope_ptr start_fabricated_block_scope_for_enhanced_for(
                                     a_scope_pointers_block_ptr pointers_block)
/*
Start a new block scope and return it.  pointers_block is the pointers block to
be used on the creation of the scope, needed so that scope can be reactivated
later (may be NULL).
*/
{
  a_scope_ptr               scope;
  a_control_flow_descr_ptr  cfdp;

  /* Push the new scope. */
  push_block_scope(pointers_block);
  scope = ensure_il_scope_exists(&scope_stack_top());
  /* Add a control flow entry to represent the new scope. */
  cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_block);
  cfdp->source_pos = pos_curr_token;
  cfdp->variant.block.object_lifetime = curr_object_lifetime;
  add_to_control_flow_descr_list(cfdp);
  return scope;
}  /* start_fabricated_block_scope_for_enhanced_for */


static void finish_block_scope_for_enhanced_for(void)
/*
Do processing required when done with a block scope.
*/
{
  /* Terminate the control flow block that was previously started. */
  add_to_control_flow_descr_list(
       alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_end_of_block));
  /* Pop the block scope. */
  pop_block_scope(/*is_final_pop=*/TRUE);
}  /* finish_block_scope_for_enhanced_for */


static a_boolean alias_decl_next(void)
/*
Return TRUE if the current token sequence starts with "using <id> =".
*/
{
  a_boolean  result = FALSE;

  if (curr_token == tok_using) {
    a_tiny_scanning_token_cache  cache(/*reusable=*/FALSE);

    cache_curr_token(cache.ptr());
    (void)get_token();
    if (curr_token == tok_identifier) {
      cache_curr_token(cache.ptr());
      (void)get_token();
      if (curr_token == tok_assign) {
        result = TRUE;
      }  /* if */
    }  /* if */
    rescan_cached_tokens(cache.ptr());
  }  /* if */
  return result;
}  /* alias_decl_next */


static void scan_condition(a_statement_ptr  sp,
                           a_boolean        *p_is_condition_decl)
/*
Scan a "condition", which is either an expression or in C++ a condition
declaration; if the latter, set *is_condition_decl to TRUE.  This routine
is called for if, switch, for, and while statements, but not for do-while
statements since they are not allowed to have condition declarations even
in C++.
*/
{
  a_disambig_flag_set          flags = DFS_REAL_DECLARATOR_ALLOWED |
                                       DFS_IS_CONDITION;
  an_il_entity_list_entry_ptr  entity_list;
  a_boolean                    is_condition_decl, potential_decl_stmt = FALSE;

  if (sp->kind == stmk_for) {
    flags |= DFS_CONDITION_IS_FOR_STMT;
  } else if (selection_initializers_enabled &&
             (sp->kind == stmk_if ||
              sp->kind == stmk_constexpr_if ||
              sp->kind == stmk_switch)) {
    start_potential_decl_statement(&entity_list);
    potential_decl_stmt = TRUE;
  }  /* if */
  if (!C_mode() &&
      (alias_decl_next() || is_decl_not_expr(flags) ||
       (curr_token == tok_semicolon && potential_decl_stmt))) {
    /* A condition declaration.  Start a scope for the variable declared in
       the condition and scan the declaration. */
    is_condition_decl = TRUE;
    if (curr_token == tok_using && !cpp23_mode) {
      pos_diagnostic(strict_ansi_mode ? strict_ansi_discretionary_severity
                                      : es_warning,
                     ec_nonstandard_alias_declaration_context,
                     &pos_curr_token);
    }  /* if */
    scan_structured_control_value(sp, (an_init_component*)NULL);
  } else {
    is_condition_decl = FALSE;
    if (potential_decl_stmt) {
      end_potential_decl_statement();
    }  /* if */
  }  /* if */
  if (is_condition_decl) {
    /* Nothing more to scan. */
  } else {
    an_init_component_ptr  cache = NULL;
    if (potential_decl_stmt && !is_condition_decl) {
      /* An expression is next, but it may be followed by a condition
         declaration.  Cache the expression and check for a semicolon that
         follows it. */
      cache = cache_expression(sp->kind == stmk_constexpr_if);
      if (curr_token == tok_semicolon) {
        scan_structured_control_value(sp, cache);
        is_condition_decl = TRUE;
      }  /* if */
    }  /* if */
    if (is_condition_decl) {
      /* Nothing more to scan. */
    } else if (sp->kind == (a_statement_kind)stmk_switch) {
      /* Scan the controlling expression and check to see that it is
         integral. */
      sp->expr = scan_integer_expression(/*is_switch_expr=*/TRUE, cache);
    } else {
      /* Scan the controlling expression and check to see that it is scalar. */
      sp->expr = scan_boolean_controlling_expression(cache);
    }  /* if */
  }  /* if */
  if (is_condition_decl) {
    *p_is_condition_decl = TRUE;
  }  /* if */
}  /* scan_condition */


static void push_statement_scope(void)
/*
In C99, iteration and selection statements are surrounded by an implicit
scope.  This routine is called at the beginning of such statements,
and pushes a generated block statement.
*/
{
  (void)start_block_statement(/*generated_statement=*/TRUE,
                              /*is_statement_expr=*/FALSE,
                              (an_object_lifetime_ptr)NULL);
  /* Move any pragmas for the statement (previously selected in
     "statement") to the new level in the scope stack. */
  scope_stack[depth_scope_stack].curr_construct_pragmas =
                       scope_stack[depth_scope_stack-1].curr_construct_pragmas;
  /* Propagate the position of the iteration or selection statement that is
     being wrapped in an implicit block to the new stack entry. */
  struct_stmt_stack_top().p_start_pos =
                                     (&struct_stmt_stack_top()-1)->p_start_pos;
  scope_stack[depth_scope_stack-1].curr_construct_pragmas = NULL;
}  /* push_statement_scope */


static void pop_statement_scope(void)
/*
In C99, iteration and selection statements are surrounded by an implicit
scope.  This routine is called at the end of such statements, and pops
the generated block statement pushed by push_statement_scope.
*/
{
  a_statement_ptr block_stmt = struct_stmt_stack[depth_stmt_stack].statement;

  finish_block_statement(block_stmt);
}  /* pop_statement_scope */


static void set_in_discarded_statement_flag(a_boolean	value)
/*
Update the in_discarded_statement flag in both the statement stack and the
scope stack based on value.  The flag is only cleared if the scope stack
value at the start of the evaluation of the constexpr if was FALSE.
*/
{
  a_struct_stmt_stack_entry_ptr	sssep;

  sssep = &struct_stmt_stack[depth_stmt_stack];
  if (!sssep->scope_stack_in_discarded_statement_state) {
    sssep->in_discarded_statement = value;
    scope_stack_top().in_discarded_statement = value;
  }  /* if */
}  /* set_in_discarded_statement_flag */


static void if_statement(void)
/*
Scan an "if" statement (with or without else) and add it to the current
statement sequence.  This function also handles a number of C++ variants:
"if constexpr (...) ...", "if consteval ...", and "if not consteval ..."
(where "not" can also be "!", since the two are equivalent tokens).
The syntax is:

        if constexpr    ( condition ) statement
                    opt
        if constexpr    ( condition ) statement else statement
                    opt
        if !    consteval statement
            opt
        if !    consteval statement else statement
            opt
*/
{
  a_statement_ptr            sp;
  a_boolean                  is_condition_decl = FALSE;
  a_statement_kind           kind;
  a_struct_stmt_kind         ssk_kind;
  a_boolean                  is_constexpr_if = FALSE, is_if_consteval = FALSE,
                             skip_discarded = FALSE;
  a_constexpr_if_ptr         cip = NULL;
  a_constexpr_if_cache_info  local_cici,
                             *cicip_to_create = NULL, *cicip_to_use = NULL;
  a_token_sequence_number    start_tsn;
  a_source_position          expr_pos;
  a_reachability_summary     saved_reachability = curr_reachability;

  db_enter(3, "if_statement");

  check_for_unreachable_code();
  /* Push a scope in C99 mode. */
  if (c99_mode) push_statement_scope();
  /* Check for a C++17 "if constexpr" or a C++23 "if consteval"/"if not
     consteval" statement (the latter is accepted as an extension in C++20). */
  if (constexpr_if_enabled || if_consteval_enabled || cpp20_mode) {
    a_token_kind  next_tok = next_token();
    if (constexpr_if_enabled && next_tok == tok_constexpr) {
      kind = (a_statement_kind)stmk_constexpr_if;
      ssk_kind = ssk_constexpr_if;
      is_constexpr_if = TRUE;
    } else if ((if_consteval_enabled || cpp20_mode) &&
               next_tok == tok_consteval) {
      if (!if_consteval_enabled) {
        /* Some compilers accept "if consteval" with a warning in C++20
           mode. */
        pos_diagnostic(strict_ansi_mode ? strict_ansi_error_severity
                                        : es_warning,
                       ec_if_consteval_nonstandard, &pos_curr_token);
      }  /* if */
      kind = stmk_if_consteval;
      ssk_kind = ssk_if;
      is_if_consteval = TRUE;
    } else if ((if_consteval_enabled || cpp20_mode) &&
               next_tok == tok_not) {
      a_token_kind  next_next_tok;
      if (next_two_tokens(tok_not, &next_next_tok) == tok_not &&
          next_next_tok == tok_consteval) {
        if (!if_consteval_enabled) {
          /* Some compilers accept "if consteval" with a warning in C++20
             mode. */
          pos_diagnostic(strict_ansi_mode ? strict_ansi_error_severity
                                          : es_warning,
                         ec_if_consteval_nonstandard, &pos_curr_token);
        }  /* if */
        kind = stmk_if_not_consteval;
        ssk_kind = ssk_if;
        is_if_consteval = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!is_constexpr_if && !is_if_consteval) {
    kind = stmk_if;
    ssk_kind = ssk_if;
  }  /* if */
  /* Allocate the statement. */
  sp = add_statement(kind, /*compiler_generated=*/FALSE);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_kind, sp, (an_object_lifetime_ptr)NULL);
  /* Ignore the initial "if". */
#if CHECKING
  if (curr_token != tok_if) internal_error("if_statement: expected if");
#endif /* CHECKING */
  start_tsn = curr_token_sequence_number;
  (void)get_token();
  if (is_constexpr_if) {
    a_struct_stmt_stack_entry_ptr sssep;
    report_gnu_cpp17_extension_if_needed(&pos_curr_token,
                                         ec_if_constexpr_is_cpp17);
    (void)get_token();
    /* Save the current scope stack discarded statement state so it can
       be restored later. */
    sssep = &struct_stmt_stack[depth_stmt_stack];
    sssep->scope_stack_in_discarded_statement_state =
                                      scope_stack_top().in_discarded_statement;
    cip = sp->variant.constexpr_if;
  } else if (is_if_consteval) {
    expr_pos = pos_curr_token;
    if (curr_token == tok_not) (void)get_token();
    (void)get_token();
  }  /* if */
  if (!is_if_consteval) {
    /* Check for and skip the opening parenthesis. */
    (void)required_token(tok_lparen, ec_exp_lparen);
    add_stop_token(tok_rparen);
    expr_pos = pos_curr_token;
    /* Scan the condition, which in C++ may be a condition declaration, and in
       C++17 may include a leading initialization statement. */
    scan_condition(sp, &is_condition_decl);
    if (is_constexpr_if) {
      /* A constexpr if.  Attempt to fold the condition.  In the case of a
         condition declaration, we can just fold the underlying (generated)
         expression: The interpreter will load any constexpr variables as
         needed.  Similarly, a C++17-style initialization statement needs no
         special treatment: The interpreter will load constant-valued variables
         as needed. */
      an_expr_node_ptr               condition_expr;
      a_struct_stmt_stack_entry_ptr  sssep;
      a_boolean                      value_known, expr_is_true = FALSE,
                                     in_real_instantiation = FALSE;
      a_constant_ptr                 folded_con = local_constant();
      a_diag_list                    diag_list;
      if (is_condition_decl) {
        condition_expr = sp->expr->variant.condition->expr;
      } else {
        condition_expr = sp->expr;
      }  /* if */
      clear_diag_list(&diag_list);
      if (interpret_expr(condition_expr, /*is_constexpr_evaluated=*/TRUE,
                         /*force_prvalue=*/TRUE, folded_con, &diag_list)) {
        value_known = TRUE;
        if (!is_error_constant(folded_con) && !is_false_constant(folded_con)) {
          expr_is_true = TRUE;
        }  /* if */
      } else {
        value_known = FALSE;
        if (!is_template_dependent_context() &&
            expr_error_should_be_issued()) {
          a_diagnostic_ptr  dp;
          dp = pos_start_error(ec_expr_not_constant, &expr_pos);
          add_more_info_list(dp, &diag_list);
          end_diagnostic(dp);
        }  /* if */
      }  /* if */
      discard_more_info_list(&diag_list);
      release_local_constant(&folded_con);
      sssep = &struct_stmt_stack[depth_stmt_stack];
      if (is_template_dependent_context()) {
        sssep->dependent_constexpr_if = !value_known;
        in_real_instantiation = is_nested_in_real_instantiation();
        /* If we are in a (non-prototype) instantiation of an enclosing
           templated entity, the discarded substatement is not instantiated
           (see N4659 [stmt.if]/2). */
        skip_discarded = value_known && in_real_instantiation;
        if (is_nonspecialized_prototype_instantiation_context() &&
            !in_real_instantiation) {
          /* Clear the local entry used to record the cache positions for
             constexpr ifs in the prototype instantiation.  A copy will be made
             when this is added to the hash table. */
          local_cici = a_constexpr_if_cache_info();
          local_cici.token_cache = get_token_cache_being_scanned();
          cicip_to_create = &local_cici;
        }  /* if */
      } else {
        check_assertion_or_expect_error(value_known);
        if (is_real_instantiation_context()) {
          /* The discarded substatement is not instantiated. */
          skip_discarded = TRUE;
        }  /* if */
      }  /* if */
      if (skip_discarded) {
        /* Look for template cache information saved during function template
           prototype instantiation.  Save information about the token cache
           currently being scanned. */
        cicip_to_use = check_constexpr_if_cache_hash_table(start_tsn);
        check_assertion(cicip_to_use == NULL ||
                        (cicip_to_use->end_start_tsn >
                                                  curr_token_sequence_number));
      }  /* if */
      cip->value_known = value_known;
      cip->value = expr_is_true;
      /* If the condition is non-dependent, set the discarded flag to the
         appropriate value for the "then" statement. */
      if (!sssep->dependent_constexpr_if) {
        set_in_discarded_statement_flag(!cip->value);
      }  /* if */
    }  /* if */
    /* Check for and skip the closing parenthesis. */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
  }  /* if */
  /* Scan the "then" statement.  If it is an empty statement not followed by
     an "else" clause, issue a remark. */
  if (curr_token == tok_semicolon && next_token() != tok_else) {
    pos_remark(ec_empty_then_statement, &error_position);
  }  /* if */
  if (scope_stack_top().in_discarded_statement && skip_discarded) {
    /* For a discarded statement that should not be instantiated, we can just
       discard the tokens.  Get rid of any pragmas that would bind to this
       statement if it were not being discarded. */
    (void)select_curr_construct_pragmas(/*add_to_list=*/FALSE);
    discard_curr_construct_pragmas();
    if (cicip_to_use != NULL &&
        skip_to_token_sequence_number(cicip_to_use->token_cache.ptr(),
                                      cicip_to_use->else_start_tsn !=
                                                       NO_TOKEN_SEQUENCE_NUMBER
                                              ? cicip_to_use->else_start_tsn
                                              : cicip_to_use->end_start_tsn)) {
      /* We were able to skip directly to the "else" or final token of a
         constexpr if. */
    } else {
      flush_if_or_else_statement();
    }  /* if */
    empty_statement(/*compiler_generated=*/TRUE);
  } else {
    a_boolean  saved_in_consteval_context =
                                       scope_stack_top().in_consteval_context;
    add_stop_token(tok_else);
    if (is_if_consteval) {
      if (scope_stack_top().in_consteval_context) {
        pos_warning(ec_already_in_consteval_context, &expr_pos);
      } else {
        a_routine_ptr  rp = current_routine_entry();
        if (!rp->is_constexpr) {
          pos_warning(ec_if_consteval_in_nonconstexpr_function, &expr_pos);
        }  /* if */
      }  /* if */
      scope_stack_top().in_consteval_context =
                                  kind == (a_statement_kind)stmk_if_consteval;
      if (curr_token != tok_lbrace) {
        pos_error(ec_if_consteval_requires_braced_dependent_statement,
                  &pos_curr_token);
      }  /* if */
    }  /* if */
    dependent_statement();
    scope_stack_top().in_consteval_context = saved_in_consteval_context;
    if (is_constexpr_if && !scope_stack_top().in_discarded_statement) {
      /* The reachability of the constexpr if should be the reachability of
         the non-discarded branch (the "if" branch in this case). */
      saved_reachability = curr_reachability;
    }  /* if */
    remove_stop_token(tok_else);
  }  /* if */
  /* Scan "else" and another statement if they appear. */
  if (curr_token == tok_else) {
    a_struct_stmt_stack_entry_ptr sssep;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (is_constexpr_if) {
      cip->else_position = pos_curr_token;
    } else {
      sp->variant.if_stmt.else_position = pos_curr_token;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (cicip_to_create != NULL) {
      /* Record the cached token handle of the start of the "else"
         clause (for a constexpr if). */
      cicip_to_create->else_start_tsn = curr_token_sequence_number;
    }  /* if */
    (void)get_token();
    if (is_if_consteval) {
      if (curr_token != tok_lbrace) {
        pos_error(ec_if_consteval_requires_braced_dependent_statement,
                  &pos_curr_token);
      }  /* if */
    } else if (curr_token == tok_semicolon) {
      /* Issue a remark if the "else" statement is an empty statement. */
      pos_remark(ec_empty_else_statement, &error_position);
    }  /* if */
    /* We don't try to reuse the sssep value from above in case the stack
       was reallocated. */
    sssep = &struct_stmt_stack[depth_stmt_stack];
    term_stmt_clause(sssep);
    sssep->in_else_of_if = TRUE;
    if (is_constexpr_if && !sssep->dependent_constexpr_if) {
      /* Update the discarded flag, but keep the current value (of FALSE)
         if the value was dependent. */
      set_in_discarded_statement_flag(cip->value);
    }  /* if */
    if (scope_stack_top().in_discarded_statement && skip_discarded) {
      /* For a discarded statement that should not be instantiated, we can just
         discard the tokens.  Get rid of any pragmas that would bind to this
         statement if it were not being discarded. */
      (void)select_curr_construct_pragmas(/*add_to_list=*/FALSE);
      discard_curr_construct_pragmas();
      if (cicip_to_use != NULL &&
          skip_to_token_sequence_number(cicip_to_use->token_cache.ptr(),
                                        cicip_to_use->end_start_tsn)) {
        /* We were able to skip directly to the final token of a constexpr
           if. */
      } else {
        flush_if_or_else_statement();
      }  /* if */
      empty_statement(/*compiler_generated=*/TRUE);
    } else {
      a_boolean  saved_in_consteval_context =
                                       scope_stack_top().in_consteval_context;
      scope_stack_top().in_consteval_context =
                              kind == (a_statement_kind)stmk_if_not_consteval;
      start_stmt_clause(sssep);
      dependent_statement();
      scope_stack_top().in_consteval_context = saved_in_consteval_context;
      if (is_constexpr_if && !scope_stack_top().in_discarded_statement) {
        /* The reachability of the constexpr if should be the reachability of
           the non-discarded branch (the "else" branch in this case). */
        saved_reachability = curr_reachability;
      }  /* if */
    }  /* if */
    /* There should always be a non-NULL else-statement pointer. */
    check_assertion_str((is_constexpr_if
                                 ? cip->else_statement
                                 : sp->variant.if_stmt.else_statement) != NULL,
                        "if_statement: else-stmt pointer is NULL");
  }  /* if */
  if (cicip_to_create != NULL) {
    /* Record the cached token handle of the end of the statement (for
       a constexpr if) */
    if (curr_token == tok_end_of_source) {
      /* Don't create the cached entry in certain error cases. */
      expect_error();
    } else if (!curr_token_pragmas->is_empty()) {
      /* The caching mechanism cannot be used if immediately followed by
         a pragma. */
    } else {
      cicip_to_create->end_start_tsn = curr_token_sequence_number;
      add_to_constexpr_if_cache_hash_table(cicip_to_create, start_tsn);
    }  /* if */
  }  /* if */
  /* End the condition block, if necessary. */
  if (is_condition_decl) finish_condition_block();
  if (is_constexpr_if) {
    scope_stack_top().in_discarded_statement =
               struct_stmt_stack[depth_stmt_stack].
                                      scope_stack_in_discarded_statement_state;
  }  /* if */
  /* Pop the structured statement stack. */
  pop_stmt_stack();
  if (is_constexpr_if && !is_template_dependent_context()) {
    /* After an "if constexpr" statement, the reachability is the same as the
       reachability at the end of the non-discarded branch. */
    curr_reachability = saved_reachability;
  }  /* if */
  /* If a label appeared in the context of the statement that was just
     terminated, it may be appropriate to push a new object lifetime for
     the scope being resumed. */
  reset_curr_block_object_lifetime(sp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sp->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Pop a scope in C99 mode. */
  if (c99_mode) pop_statement_scope();

  db_exit();
}  /* if_statement */


static void switch_statement(void)
/*
Scan a "switch" statement and add it to the current statement sequence.
The syntax is:

3.6.4  selection-statement:
		switch ( expression ) statement

See also 3.6.4.2.
*/
{
  a_statement_ptr           sp;
  a_control_flow_descr_ptr  cfdp;
  a_boolean                 is_condition_decl = FALSE;

  db_enter(3, "switch_statement");

  check_for_unreachable_code();
  /* Push a scope in C99 mode. */
  if (c99_mode) push_statement_scope();
  /* Allocate the statement. */
  sp = add_statement(stmk_switch, /*compiler_generated=*/FALSE);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_switch, sp, (an_object_lifetime_ptr)NULL);
  /* Ignore the initial "switch". */
  check_assertion_str(curr_token == tok_switch,
                      "switch_statement: expected switch");
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the "condition", which in C++ may be a condition declaration. */
  scan_condition(sp, &is_condition_decl);
  if (!is_error_node(sp->expr)) {
    /* Issue a remark if the selector is constant. */
    if (is_constant_node(skip_parens(sp->expr))) {
      pos_remark(ec_switch_selector_expr_is_constant, &error_position);
    }  /* if */
  }  /* if */
  /* Add a switch block entry to the control_flow_descr_list.  The
     corresponding end-of-entry is added at the end of this routine.  This
     is done even though a switch statement usually involves a compound
     statement, which could also serve as the switch block.  It's done
     this way to handle the unusual case as well, e.g.,
       switch (i) if (i > 0) ++i; else { int j = i; i += j; case 0:; }
     Also, set the source position of "switch" in the entry that's
     created. */
  cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_block);
  cfdp->source_pos = sp->position;
  cfdp->variant.block.is_switch_block = TRUE;
  add_to_control_flow_descr_list(cfdp);
  /* Save the selector expression type for checking of the case label
     values. */
  struct_stmt_stack[depth_stmt_stack].type = sp->expr->type;
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* The body of a switch is not reachable until a case or default label
     appears.  Note that set_unreachable should not be called until after
     the condition declaration, if any, is scanned. */
  set_unreachable(curr_reachability);
  /* Scan the dependent statement. */
  dependent_statement();
  /* If any switch case was template-dependent, discard the sorted list since
     its semantics are marginal. */
  if (struct_stmt_stack[depth_stmt_stack].switch_has_dependent_case) {
    a_switch_case_entry_ptr  scep = sp->variant.switch_stmt.extra_info->cases;
    for (; scep != NULL; scep = scep->next) {
      scep->next_on_sorted_list = NULL;
    }  /* for */
    sp->variant.switch_stmt.extra_info->sorted_cases = NULL;
  }  /* if */
  add_to_control_flow_descr_list(
      alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_end_of_block));
  /* End the condition block, if necessary. */
  if (is_condition_decl) finish_condition_block();
  /* Pop the structured statement stack. */
  pop_stmt_stack();
  /* If a label appeared in the context of the statement that was just
     terminated, it may be appropriate to push a new object lifetime for
     the scope being resumed. */
  reset_curr_block_object_lifetime(sp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sp->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Pop a scope in C99 mode. */
  if (c99_mode) pop_statement_scope();

  db_exit();
}  /* switch_statement */


static void warn_if_loop_has_no_labels(a_source_position  *stmt_pos)
/*
This routine is called at the end of a loop construct if the caller has
determined that the beginning of the loop is not sequentially reachable from
the prior statement.  This routine issues a "not reachable" warning at the
given position if the loop doesn't contains a (user declared) label definition
or active switch case (which means the loop cannot be reached at all).
*/
{
  if (!struct_stmt_stack[depth_stmt_stack].contains_user_label &&
      !struct_stmt_stack[depth_stmt_stack].contains_active_switch_case) {
    pos_warning(ec_loop_not_reachable, stmt_pos);
    curr_reachability.suppress_unreachable_warning = TRUE;
  }  /* if */
}  /* warn_if_loop_has_no_labels */


static void while_statement(void)
/*
Scan a "while" statement and add it to the current statement sequence.
The syntax is:

3.6.5  iteration-statement:
		while ( expression ) statement

See also 3.6.5.1.
*/
{
  a_statement_ptr    sp;
  a_boolean          is_condition_decl = FALSE, assume_loop_reachable;
  a_source_position  stmt_pos;

  db_enter(3, "while_statement");

  stmt_pos = pos_curr_token;
  assume_loop_reachable = curr_reachability.reachable ||
                          curr_reachability.suppress_unreachable_warning;
  /* Push a scope in C99 mode. */
  if (c99_mode) push_statement_scope();
  /* Allocate the statement. */
  sp = add_statement(stmk_while, /*compiler_generated=*/FALSE);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_while, sp, (an_object_lifetime_ptr)NULL);
  /* Ignore the initial "while". */
#if CHECKING
  if (curr_token != tok_while) {
    internal_error("while_statement: expected while");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the condition, which in C++ may be a condition declaration. */
  scan_condition(sp, &is_condition_decl);
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Scan the dependent statement. */
  dependent_statement();
  if (!assume_loop_reachable) warn_if_loop_has_no_labels(&stmt_pos);
  /* Define the "continue" label, if it is needed. */
  define_continue_label();
  /* End the condition block, if necessary. */
  if (is_condition_decl) finish_condition_block();
  /* Pop the structured statement stack. */
  pop_stmt_stack();
  /* If a label appeared in the context of the statement that was just
     terminated, it may be appropriate to push a new object lifetime for
     the scope being resumed. */
  reset_curr_block_object_lifetime(sp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sp->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Pop a scope in C99 mode. */
  if (c99_mode) pop_statement_scope();
  db_exit();
}  /* while_statement */


static void do_statement(void)
/*
Scan a "do" statement and add it to the current statement sequence.
The syntax is:

3.6.5  iteration-statement:
		do statement while ( expression )

See also 3.6.5.2.
*/
{
  a_statement_ptr    sp;
  a_boolean          assume_loop_reachable;
  a_source_position  stmt_pos;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_position  saved_while_position;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_enter(3, "do_statement");

  stmt_pos = pos_curr_token;
  assume_loop_reachable = curr_reachability.reachable ||
                          curr_reachability.suppress_unreachable_warning;
  /* Push a scope in C99 mode. */
  if (c99_mode) push_statement_scope();
  /* Allocate the statement. */
  sp = add_statement(stmk_end_test_while, /*compiler_generated=*/FALSE);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_do, sp, (an_object_lifetime_ptr)NULL);
  /* Ignore the initial "do". */
#if CHECKING
  if (curr_token != tok_do) internal_error("do_statement: expected do");
#endif /* CHECKING */
  (void)get_token();
  /* Scan the dependent statement. */
  add_stop_token(tok_while);
  dependent_statement();
  if (!assume_loop_reachable) warn_if_loop_has_no_labels(&stmt_pos);
  /* Define the "continue" label, if it is needed. */
  define_continue_label();
  /* Check for and skip the keyword "while". */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  saved_while_position = pos_curr_token;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  (void)required_token(tok_while, ec_exp_while);
  remove_stop_token(tok_while);
  add_stop_token(tok_semicolon);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Add an end-of-construct source sequence entry to indicate the location of
     the "while" clause.  This allows pragmas and such to be represented when
     they occur between the dependent statement and the "while" keyword. */
  add_end_of_construct_source_sequence_entry((char *)sp, iek_statement);
  if (!source_sequence_entries_disallowed) {
    /* Retrieve the end-of-construct entry just created and modify its
       position to point to the "while" keyword (it currently points to the
       "(" following the "while"). */
    a_source_sequence_entry_ptr     ssep;
    a_src_seq_end_of_construct_ptr  sseocp;
    ssep = scope_stack[depth_scope_stack].end_of_source_sequence_list;
    check_assertion(ssep->entity.kind == iek_src_seq_end_of_construct);
    sseocp = (a_src_seq_end_of_construct_ptr)ssep->entity.ptr;
    sseocp->position = saved_while_position;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the controlling expression, and check to see that it is scalar. */
  sp->expr = scan_boolean_controlling_expression((an_init_component*)NULL);
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (curr_token == tok_semicolon) {
    curr_construct_end_position = end_pos_curr_token;
  }  /* if */
  sp->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and skip the semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);
  /* Pop the structured statement stack. */
  pop_stmt_stack();
  /* If a label appeared in the context of the statement that was just
     terminated, it may be appropriate to push a new object lifetime for
     the scope being resumed. */
  reset_curr_block_object_lifetime(sp);
  /* Pop a scope in C99 mode. */
  if (c99_mode) pop_statement_scope();

  db_exit();
}  /* do_statement */


static void start_of_try_block(a_statement_ptr  sp)
/*
Do initialization for a try-block statement or a function-try-block.  The
current token should be "try", which is consumed.
*/
{
  db_enter(3, "start_of_try_block");
  check_assertion(curr_token == tok_try);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_try_block, sp, (an_object_lifetime_ptr)NULL);
  if (!C_mode()) {
    /* Push an object lifetime. */
    push_object_lifetime(iek_try_supplement, (char *)sp->variant.try_block,
                         (an_object_lifetime_kind)olk_try_block);
  }  /* if */
  current_routine_entry()->contains_try_block = TRUE;
  if (!exceptions_enabled) {
    /* Support for exceptions is suppressed for this compilation. */
    pos_error(ec_no_exception_support, &pos_curr_token);
  } else {
    /* Exceptions are outside the "Embedded C++" subset. */
    feature_is_not_part_of_embedded_cplusplus_subset(
                                        &pos_curr_token,
                                        ec_exceptions_in_embedded_cplusplus);
    statement_not_allowed_inside_statement_expression(&pos_curr_token);
    if (microsoft_mode && warn_on_try_statement) {
      /* Emulate the warning that MSVC++ provides when no /EH command line
         option is specified. */
      pos_warning(ec_exception_handler_used, &pos_curr_token);
      (void)set_severity_for_error_number((int)ec_exception_handler_used,
                                          es_once, /*make_default=*/FALSE);
    }  /* if */
  }  /* if */
  /* Bypass "try". */
  (void)get_token();
  db_exit();
}  /* start_of_try_block */


static void try_block_statement(a_statement_ptr  sp,
                                a_boolean        explicit_return_type)
/*
Scan a C++ try-block statement.  Its form is:

  try compound-statement handler-seq

where handler-seq is a sequence of one or more handlers of the form

  catch ( exception-declaration ) compound-statement

In C++/CLI mode, the following forms are also accepted (see ECMA-372 A.16):

   try compound-statement finally-clause
   try compound-statement handler-seq finally-clause

where finally-clause is of the form:

   finally compound-statement

This function is also called to scan a function try block, in which case the
"try" keyword will already have been consumed and other initialization done.
sp points to an stmk_try_block statement when a function try block is being
scanned but is NULL for an ordinary try-block.  explicit_return_type is TRUE
only for function try blocks and only when the associated function was
declared with an explicit return type.
*/
{
  a_source_position  catch_pos;
  a_boolean          is_function_try_block;
  a_boolean          catch_exists;

  db_enter(3, "try_block_statement");
  /* The statement will already have been created for function try blocks. */
  is_function_try_block = sp != NULL;
  /* Allocate the statement. */
  if (!is_function_try_block) {
    check_for_unreachable_code();
    sp = add_statement(stmk_try_block, /*compiler_generated=*/FALSE);
    stmt_update_source_sequence_list(sp);
    start_of_try_block(sp);
  }  /* if */
  /* "catch" is not put into the stop tokens set on purpose because the
     guarded statement is always a compound statement.  It wouldn't do
     any good and could cause looping on errors. */
  /* Scan the compound statement, and save a pointer to it in the try-block
     statement. */
  sp->variant.try_block->statement =
                          compound_statement(/*at_function_level=*/FALSE,
                                             explicit_return_type,
                                             /*is_catch_clause=*/FALSE,
                                             /*is_statement_expr=*/FALSE);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sp->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* The next token should be a "catch" introducing the first handler. */
  /* Save the current token position as catch_pos before checking whether
     it is in fact tok_catch, since the function that checks also advances
     past it. */
  catch_pos = pos_curr_token;
  /* If C++/CLI mode is not enabled, catch is required here.  Otherwise,
     it can be omitted provided there is a "finally". */
  if (!cli_or_cx_enabled) {
    catch_exists = required_token(tok_catch, ec_missing_handler);
  } else {
    catch_exists = curr_token == tok_catch;
    if (catch_exists) (void)get_token();
  }  /* if */
  if (catch_exists) {
    /* Loop through the (1 or more) handler declarations, adding each to
       the linked list of handlers pointed to by sp. */
    do {
      term_stmt_clause(&struct_stmt_stack[depth_stmt_stack]);
      start_stmt_clause(&struct_stmt_stack[depth_stmt_stack]);
      handler_declaration(sp, &catch_pos, is_function_try_block);
      /* Again, save the current token position as catch_pos before
         checking. */
      catch_pos = pos_curr_token;
    } while (loop_token(tok_catch));
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled) {
    if (curr_token == tok_finally ||
        (curr_token == tok_identifier &&
         curr_token_is_identifier_string("finally") &&
         next_token() == tok_lbrace)) {
      /* Scan the "finally" clause allowed in C++/CLI (__finally is also
         permitted by MS). */
      (void)get_token();
      term_stmt_clause(&struct_stmt_stack[depth_stmt_stack]);
      start_stmt_clause(&struct_stmt_stack[depth_stmt_stack]);
      struct_stmt_stack[depth_stmt_stack].parsing_finally_clause = TRUE;
      sp->variant.try_block->finally_statement =
        compound_statement(/*at_function_level=*/FALSE,
                           explicit_return_type,
                           /*is_catch_clause=*/FALSE,
                           /*is_statement_expr=*/FALSE);
      
      sp->variant.try_block->finally_statement->parent = sp;
      struct_stmt_stack[depth_stmt_stack].parsing_finally_clause = FALSE;
    } else if (!catch_exists) {
      /* Neither "catch" nor "finally" was specified. Use required_token to
         issue the diagnostic and advance the token stream as above. */
      (void)required_token(tok_catch, ec_missing_finally);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (!C_mode()) (void)pop_object_lifetime();
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* try_block_statement */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void microsoft_try_statement(void)
/*
Scan the Microsoft structured exception handling try-finally or try-except
statement.  Its form is

  __try compound-statement __finally compound-statement
  __try compound_statement __except ( expression) compound_statement

*/
{
  a_statement_ptr sp, block;

  db_enter(3, "microsoft_try_statement");
  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement(stmk_microsoft_try, /*compiler_generated=*/FALSE);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_microsoft_try, sp, (an_object_lifetime_ptr)NULL);
  statement_not_allowed_inside_statement_expression(&pos_curr_token);
#if CHECKING
  if (curr_token != tok_microsoft_try) {
    internal_error("microsoft_try_statement: expected __try");
  }  /* if */
#endif /* CHECKING */
  /* Bypass "__try". */
  (void)get_token();
  /* "__except" and "__finally" are not put into the stop tokens set on
     purpose because the guarded statement is always a compound statement.
     It wouldn't do any good and could cause looping on errors. */
  /* Scan the compound statement, and save a pointer to it in the try
     statement. */
  sp->variant.microsoft_try->guarded_statement = block =
                          compound_statement(/*at_function_level=*/FALSE,
                                             /*explicit_return_type=*/FALSE,
                                             /*is_catch_clause=*/FALSE,
                                             /*is_statement_expr=*/FALSE);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sp->end_position = curr_construct_end_position;
  /* The current token should be "__except" or "__finally": */
  sp->variant.microsoft_try->except_or_finally_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (curr_token == tok_except) {
    /* __except ( expression ) form. */
    (void)get_token();
    /* Check for and skip the opening parenthesis. */
    (void)required_token(tok_lparen, ec_exp_lparen);
    add_stop_token(tok_rparen);
    /* Scan the expression and check to see that it is integral. */
    sp->variant.microsoft_try->except_expr =
                             scan_integer_expression(/*is_switch_expr=*/FALSE,
                                                     (an_init_component*)NULL);
    /* Check for and skip the closing parenthesis. */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
  } else if (cli_or_cx_enabled &&
             (curr_token == tok_identifier &&
              curr_token_is_identifier_string("finally") &&
              next_token() == tok_lbrace)) {
    /* In C++/CLI mode, __try/finally is also accepted. */
    (void)get_token();
  } else {
    /* __finally form. */
    (void)required_token(tok_finally, ec_exp_except_or_finally);
  }  /* if */
  /* Wrap up the block statement and pop the name scope.  (This was not done
     in compound_statement because the __except expression has to be scanned
     within the name scope belonging to the guarded statement.) */
  finish_block_statement(block);
  /* Define the "continue" label, if it is needed.  This is the target of
     __leave statements. */
  define_continue_label();
  /* Scan the cleanup statement. */
  term_stmt_clause(&struct_stmt_stack[depth_stmt_stack]);
  start_stmt_clause(&struct_stmt_stack[depth_stmt_stack]);
  struct_stmt_stack[depth_stmt_stack].
                                  in_cleanup_statement_of_microsoft_try = TRUE;
  sp->variant.microsoft_try->cleanup_statement =
                          compound_statement(/*at_function_level=*/FALSE,
                                             /*explicit_return_type=*/FALSE,
                                             /*is_catch_clause=*/FALSE,
                                             /*is_statement_expr=*/FALSE);
  /* Pop the structured statement stack. */
  pop_stmt_stack();

  db_exit();
}  /* microsoft_try_statement */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void transfer_coroutine_lifetime(an_object_lifetime_ptr lifetime)
/*
Given the new lifetime object for a coroutine that is having its body wrapped
in a try block, transfer the appropriate bits from the old lifetime and fixup
the relationships appropriately.
*/
{
  an_object_lifetime_ptr new_lifetime = lifetime->child_lifetime;
  an_object_lifetime_ptr orig_child_lifetime = new_lifetime->next;
  an_object_lifetime_ptr olp;
  a_dynamic_init_ptr     dip;

  check_assertion(lifetime->next == NULL);
  check_assertion(lifetime->parent_destruction_sublist == NULL);
  if (exceptions_enabled) {
    /* When exceptions are enabled, the try block also needs to be skipped. */
    check_assertion(new_lifetime->kind == olk_try_block);
    new_lifetime = new_lifetime->child_lifetime;
  }  /* if */
  /* Transfer the original child lifetime. */
  for (olp = orig_child_lifetime; olp != NULL; olp = olp->next) {
    olp->parent_lifetime = new_lifetime;
  }  /* for */
  new_lifetime->child_lifetime = orig_child_lifetime;
  lifetime->child_lifetime->next = NULL;
  lifetime->child_lifetime->parent_destruction_sublist = NULL;
  /* Transfer destructions to the new lifetime, adjusting lifetime references
     in dynamic init entries. */
  for (dip = lifetime->destructions;
       dip != NULL;
       dip = dip->next_in_destruction_list) {
    dip->lifetime = new_lifetime;
  }  /* for */
  new_lifetime->destructions = lifetime->destructions;
  lifetime->destructions = NULL;
  new_lifetime->has_block_after_label_child_lifetime =
                                lifetime->has_block_after_label_child_lifetime;
  lifetime->has_block_after_label_child_lifetime = FALSE;
  new_lifetime->has_implicit_child = lifetime->has_implicit_child;
  lifetime->has_implicit_child = FALSE;
}  /* transfer_coroutine_lifetime */


static a_statement_ptr create_coroutine_handler_block(
                                                 a_coroutine_descr_ptr cr_desc)
/*
Create and fill in the contents of the catch block for a coroutine's generated
try/catch block.  Return the created block.
*/
{
  a_statement_ptr block = alloc_statement(stmk_block,
                                          /*compiler_generated=*/TRUE);
  a_statement_ptr *handler_stmt = &block->variant.block.statements;
  a_statement_ptr stmt;

  block->variant.block.extra_info->assoc_scope = scope_stack_top().il_scope;
  /* Create the if (!initial-await-resume-called) throw; statement. */
  stmt = alloc_statement(stmk_if, /*compiler_generated=*/TRUE);
  *handler_stmt = stmt;
  handler_stmt = &stmt->next;
  stmt->parent = block;
  stmt->expr = alloc_expr_node((an_expr_node_kind)enk_operation);
  set_node_operator(stmt->expr, eok_not, boolean_result_type(),
                    /*is_lvalue=*/FALSE,
                    var_rvalue_expr(cr_desc->init_await_resume));
  stmt->variant.if_stmt.then_statement =
                       alloc_statement(stmk_expr, /*compiler_generated=*/TRUE);
  stmt = stmt->variant.if_stmt.then_statement;
  stmt->expr = alloc_expr_node((an_expr_node_kind)enk_throw);
  stmt->expr->type = void_type();
  stmt->expr->variant.throw_info = NULL;
  stmt->expr->result_is_not_used = TRUE;
  /* Create the unhandled exception call. */
  if (cr_desc->unhandled_exception_call != NULL) {
    stmt = alloc_statement(stmk_expr, /*compiler_generated=*/TRUE);
    *handler_stmt = stmt;
    handler_stmt = &stmt->next;
    stmt->expr = cr_desc->unhandled_exception_call;
    stmt->parent = block;
  }  /* if */
  return block;
}  /* create_coroutine_handler_block */


a_statement_ptr wrap_coroutine_body_in_try_block(
                                            a_routine_ptr         coroutine,
                                            a_statement_ptr       func_body,
                                            a_coroutine_descr_ptr cr_desc,
                                            an_expr_node_ptr      init_suspend)
/*
Given a function body for a given coroutine, wrap that function body in a
try/catch block and add the initial suspend call contained in init_suspend.
Return the statement for the try/catch.
*/
{
  a_statement_ptr try_catch_stmt;
  a_handler_ptr   handler;
  a_scope_ptr     sp = scope_for_routine(coroutine);

  /* Move the function body into the block of the try. */
  try_catch_stmt = alloc_statement(stmk_block, /*compiler_generated=*/TRUE);
  func_body->parent = try_catch_stmt;
  if (init_suspend != NULL) {
    a_statement_ptr stmt = alloc_statement(stmk_expr,
                                           /*compiler_generated=*/TRUE);
    stmt->expr = init_suspend;
    stmt->parent = try_catch_stmt;
    stmt->next = func_body;
    func_body = stmt;
  }  /* if */
  try_catch_stmt->variant.block.statements = func_body;
  func_body = try_catch_stmt;
  if (exceptions_enabled) {
    try_catch_stmt = alloc_statement(stmk_try_block,
                                     /*compiler_generated=*/TRUE);
    try_catch_stmt->variant.try_block->statement = func_body;
    func_body->parent = try_catch_stmt;
    /* Prepare try block scope */
    push_object_lifetime(iek_try_supplement,
                         (char*)try_catch_stmt->variant.try_block,
                         (an_object_lifetime_kind)olk_try_block);
  } else {
    try_catch_stmt = func_body;
  }  /* if */
  push_object_lifetime(iek_block, (char*)func_body->variant.block.extra_info,
                       olk_block);
  transfer_coroutine_lifetime(sp->lifetime);
  (void)pop_object_lifetime();
  if (exceptions_enabled) {
    /* Create the handler for the try. */
    (void)push_scope((a_scope_kind)sck_block, NO_SCOPE_NUMBER,
                     /*assoc_type=*/NULL, /*assoc_routine=*/NULL);
    try_catch_stmt->variant.try_block->handlers = handler = alloc_handler();
    set_block_scope_handler(handler);
    handler->statement = create_coroutine_handler_block(cr_desc);
    handler->statement->parent = try_catch_stmt;
    pop_scope();
    coroutine->contains_try_block = TRUE;
    (void)pop_object_lifetime();
  }  /* if */
  return try_catch_stmt;
}  /* wrap_coroutine_body_in_try_block */


static void empty_statement(a_boolean compiler_generated)
/*
Do processing appropriate to an empty statement -- typically, just a
semicolon.  However, this routine is also called for some error cases as
well.  The statement's compiler_generated flag will be set as indicated by the
compiler_generated argument.
*/
{
  a_statement_ptr  esp;

  db_enter(3, "empty_statement");
  if (curr_token == tok_semicolon) {
    /* Issue diagnostics on pragmas that are trying to bind to the empty
       statement. */
    cannot_bind_to_curr_construct();
  } else {
    /* Must be an error case.  Discard any pragmas that are bound to the
       current statement. */
    discard_curr_construct_pragmas();
  }  /* if */
  esp = add_statement(stmk_empty, compiler_generated);
  stmt_update_source_sequence_list(esp);
  /* Advance past the semicolon. */
  if (curr_token == tok_semicolon) {
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = end_pos_curr_token;
    esp->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    (void)get_token();
  }  /* if */
  if (esp->is_fallthrough_statement) {
    struct_stmt_stack_top().fallthrough_statement = esp;
  }  /* if */
  db_exit();
}  /* empty_statement */


static void add_goto_to_continue_label(
                                      a_struct_stmt_stack_entry_ptr sssep,
                                      a_boolean                     is_leave,
                                      a_statement_ptr               *goto_stmt)
/*
Generate a goto to the "continue" label for the indicated structured
statement.  Generate the label if it has not been generated yet.
sssep == NULL to indicate an error.  is_leave is TRUE to indicate a
__leave instead of a continue.  Return a pointer to the goto statement
in *goto_stmt.
*/
{
  a_label_ptr              dest_label;
  a_statement_ptr          sp;
  a_control_flow_descr_ptr cfdp;

  if (sssep == NULL) {
    /* Error.  Since no continue statement is actually added to the IL,
       treat this as an empty statement. */
    empty_statement(/*compiler_generated=*/TRUE);
    sp = NULL;
  } else {
    dest_label = sssep->continue_label;
    if (dest_label == NULL) {
      /* The continue label has not previously been used, so generate it. */
      dest_label = sssep->continue_label = alloc_temp_label();
      if (is_leave) {
#if MICROSOFT_EXTENSIONS_ALLOWED
        dest_label->leave_label = TRUE;
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
        unexpected_condition();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        dest_label->continue_label = TRUE;
      }  /* if */
    }  /* if */
    /* Allocate the goto statement. */
    sp = add_statement(stmk_goto, /*compiler_generated=*/TRUE);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    stmt_update_source_sequence_list(sp);
    /* Put the destination label into the goto. */
    sp->variant.label.ptr = dest_label;
    if (!C_mode()) {
      /* Set the object lifetime.  It is a provisional setting and may be
         changed based on the lifetime of the continue label itself. */
      sp->variant.label.lifetime =
                         innermost_block_object_lifetime(curr_object_lifetime);
    }  /* if */
    if (!C_mode() || vla_enabled) {
      /* Allocate and fill in a goto entry.  This is done in C++ mode
         because it's needed for object lifetime management and in C mode when
         VLA support is enabled to insert vla-dealloc statements before
         forward gotos. */
      cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_goto);
      cfdp->source_pos = pos_curr_token;
      cfdp->variant.goto_statement.ptr = sp;
      add_to_control_flow_descr_list(cfdp);
      cfdp->variant.goto_statement.prev_goto = sssep->continue_statements;
      sssep->continue_statements = cfdp;
    }  /* if */
    /* Do processing required for any pragmas that are bound to the current
       statement. */
    process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  }  /* if */
  *goto_stmt = sp;
}  /* add_goto_to_continue_label */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void leave_statement(void)
/*
Scan a Microsoft "__leave" statement and add it to the current statement
sequence.  Such a statement is used to leave a try-finally.
The syntax is:

  __leave ;

*/
{
  a_struct_stmt_stack_entry_ptr sssep;
  a_statement_ptr               goto_stmt;

  db_enter(3, "leave_statement");
  check_for_unreachable_code();
  /* Find an enclosing "try". */
  sssep = &struct_stmt_stack[depth_stmt_stack];
  /* Note that the loop never looks at entry [0], since that is for
     the compound statement that defines the function. */
  while (sssep != &struct_stmt_stack[0]) {
    if (sssep->kind == ssk_microsoft_try &&
        !sssep->in_cleanup_statement_of_microsoft_try) goto found;
    /* Keeping looking at entries in the structured statement stack. */
    sssep--;
  }  /* while */
  /* No structured statement matching the criteria was found. */
  pos_error(ec_leave_must_be_in_try, &error_position);
  sssep = NULL;
found:
  /* Add a "goto" to the continue label for the __try. */
  add_goto_to_continue_label(sssep, /*is_leave=*/TRUE, &goto_stmt);
  /* Ignore the initial "__leave". */
#if CHECKING
  if (curr_token != tok_leave) {
    internal_error("leave_statement: expected __leave");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (curr_token == tok_semicolon) {
    curr_construct_end_position = end_pos_curr_token;
  }  /* if */
  if (goto_stmt != NULL) {
    goto_stmt->end_position = curr_construct_end_position;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  db_exit();
}  /* leave_statement */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void expression_statement(a_boolean  marked_as_gnu_extension)
/*
Scan an expression statement.  If marked_as_gnu_extension is TRUE,
the statement was preceded by the GNU C __extension__ keyword.
*/
{
  a_statement_ptr     sp;
  an_expr_node_ptr    expr;
  a_boolean           is_statement_expr =
                                    struct_stmt_stack_top().is_statement_expr;
  a_dynamic_init_ptr  dip;

  sp = add_statement(stmk_expr, /*compiler_generated=*/FALSE);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Scan the expression. */
  expr = scan_void_expression(/*repeated_in_loop=*/FALSE,
                              marked_as_gnu_extension, is_statement_expr,
                              &dip, (an_init_component*)NULL);
  if (dip != NULL) {
    /* The result of a GNU statement expression that requires nontrivial
       initialization or destruction semantics. */
    check_assertion(is_statement_expr && expr == NULL);
    set_statement_kind(sp, stmk_stmt_expr_result);
    sp->variant.stmt_expr_result.dynamic_init = dip;
  } else if (is_statement_expr && at_end_of_statement_expression()) {
    /* The result of a GNU statement expression: Turn the statement into an
       stmk_stmt_expr_result entry for easy identification. */
    set_statement_kind(sp, stmk_stmt_expr_result);
  }  /* if */
  if (expr != NULL) {
    sp->expr = expr;
    /* If the expression is a throw expression or calls a function that
       is known not to return, the code following is unreachable. */
    check_reachability_following_expression(expr);
  } else {
    /* If expr is NULL, this is the last statement in the enclosing statement
       expression and reachability analysis is not needed at this point. */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (curr_token == tok_semicolon) {
    curr_construct_end_position = end_pos_curr_token;
  }  /* if */
  sp->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* expression_statement */


static void start_for_init_block(a_statement_ptr            sp,
                                 a_scope_pointers_block_ptr pointers_block)
/*
Start a new scope for a for-init declaration (C++ only).  A scope stack
pointers block can be specified for cases where an iterator scope is pushed and
needs to be reactivated.  pointers_block can be NULL.
*/
{
  a_control_flow_descr_ptr  cfdp;

  db_enter(3, "start_for_init_block");
  /* Push a block scope to represent the name scope in which a for-init
     declaration appears and record the IL scope in the for-loop supplement. */
  sp->variant.for_loop.extra_info->for_init_scope =
                                           push_for_init_scope(pointers_block);
  /* Add a control flow entry to represent the for-init block. */
  cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_block);
  cfdp->source_pos = pos_curr_token;
  cfdp->variant.block.object_lifetime = curr_object_lifetime;
  add_to_control_flow_descr_list(cfdp);
  db_exit();
}  /* start_for_init_block */


static void finish_for_init_block(void)
/*
Terminate the for-init block scope.
*/
{
  db_enter(3, "finish_for_init_block");
  /* Terminate the control flow block that was started when the for-init
     block was started. */
  add_to_control_flow_descr_list(
       alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_end_of_block));
  /* Pop the for-init block scope. */
  pop_scope();
  db_exit();
}  /* finish_for_init_block */


static void for_init_statement(a_scope_pointers_block_ptr pointers_block)
/*
Scan the initializing expression or, in C++ or C99, declaration of a for
statement, or in C++20 for a range-based for statement with an optional
init-statement.  A scope stack pointers block can be specified for cases where
an iterator scope is pushed and needs to be reactivated.  pointers_block
can be NULL.
*/
{
  a_struct_stmt_stack_entry_ptr  sssep = &struct_stmt_stack_top();
  an_il_entity_list_entry_ptr    entity_list;

  db_enter(3, "for_init_statement");
  /* Let add_statement know this is a for_init so that the statement is
     attached in the right place. */
  sssep->for_init = TRUE;
  if (!C_mode()) {
    start_potential_decl_statement(&entity_list);
  }  /* if */
  if ((!C_mode() &&
       (alias_decl_next() ||
        is_decl_not_expr(DFS_REAL_DECLARATOR_ALLOWED))) ||
      ((c99_mode ||
        (C_mode() && microsoft_mode && microsoft_version >= 1800)) &&
       is_decl_start(IDS_EXPR_CONTEXT | IDS_REAL_DECLARATOR_ALLOWED))) {
    /* Scan a declaration (C++ or C99). */
    /* In C99, a scope is pushed around all iteration and selection
       statements, so it is not necessary to push another scope here. */
    if (!C_mode()) {
      /* C++. */
      if (curr_token == tok_using && !cpp23_mode) {
        pos_warning(ec_nonstandard_alias_declaration_context, &pos_curr_token);
      }  /* if */
      /* Unless the old-style scoping is required, push a block scope to
         contain the for-init declaration.  (Old-style scoping means the
         declaration occurs in the scope to which the for-statement itself
         belongs, whereas the standard (6.5.3 [stmt.for], para 3) requires,
         in effect, that the for-init declaration have its own scope, nested
         within the containing scope.) */
      if (!use_nonstandard_for_init_scope) {
        start_for_init_block(sssep->statement, pointers_block);
      }  /* if */
    }  /* if */
    decl_statement(/*marked_as_gnu_extension=*/FALSE,
                   /*p_okay_in_constexpr_body=*/NULL);
  } else {
    /* Scan an expression.  It may be omitted. */
    if (curr_token != tok_semicolon) expression_statement(
                                           /*marked_as_gnu_extension=*/FALSE);
    (void)required_token(tok_semicolon, ec_exp_semicolon);
  }  /* if */
  if (!C_mode()) {
    end_potential_decl_statement();
  }  /* if */
  /* Restore the for_init flag to its default value. */
  sssep = &struct_stmt_stack[depth_stmt_stack];
  sssep->for_init = FALSE;
  /* Clear the fields that will have been updated if the for-init required
     more than one stmk_init statement, e.g.:
       for (int i = 0, j = 10; j > i; --j, ++i) { }    */
  end_stmt_sequence(sssep);
  db_exit();
}  /* for_init_statement */


STATIC_THREAD a_boolean
		already_diagnosed_init_in_range_for;
			/* Flag indicating whether a non-standard init
			   statement in a range-based for statement has already
			   been diagnosed. */


static void for_statement(void)
/*
Scan a "for" or range-based-for statement and add it to the current statement
sequence.  The syntax is:

3.6.5  iteration-statement:
    for ( expr    ; expr    ; expr    ) statement
              opt       opt       opt

See also 3.6.5.3.

In C++ the first expression is replaced by for-init-statement, which is
either an expression statement or a declaration statement.

The range-based-for syntax ([stmt.ranged]) is:

  for ( init-statement   for-range-declaration : for-range-initializer )
                      opt
    statement

In UPC mode, the "upc_forall" construct is also accepted.  It looks much
like the standard "for" statement, except for the fourth expression.
    for ( expr    ; expr    ; expr    ; affinity    ) statement
              opt       opt       opt           opt

The affinity can be an expression or the keyword "continue".
*/
{
  a_statement_ptr            sp;
  a_boolean                  saved_flag, assume_loop_reachable;
  a_boolean                  is_condition_decl = FALSE;
  a_boolean                  processing_upc_forall = FALSE;
  a_boolean                  is_range_based_for = FALSE;
  a_boolean                  need_c99_stmt_scope = FALSE;
#if UPC_EXTENSIONS_ALLOWED
  an_expr_node_ptr           affinity_expr = NULL;
  a_statement_ptr            saved_innermost_forall_loop = NULL;
#endif /* UPC_EXTENSIONS_ALLOWED */
  a_source_position          stmt_pos, range_pos, await_pos;
  a_token_sequence_number    expr_tok_seq_number;
  a_range_based_for_loop_ptr rbflp = NULL;
  a_scope_pointers_block     iterator_pointers_block, rbf_pointers_block;
  a_boolean                  use_await = FALSE;
  a_label_ptr                break_label = NULL;

  db_enter(3, "for_statement");

  stmt_pos = pos_curr_token;
  assume_loop_reachable = curr_reachability.reachable ||
                          curr_reachability.suppress_unreachable_warning;
  /* In C99, the statement itself has an associated scope.  Microsoft C also
     implements this starting with version 18.00. */
  need_c99_stmt_scope = c99_mode || (C_mode() && microsoft_mode &&
                                     microsoft_version >= 1800);
  if (need_c99_stmt_scope) push_statement_scope();
  /* Allocate the for statement. */
#if UPC_EXTENSIONS_ALLOWED
  if (curr_token == tok_upc_forall) {
    /* This is a UPC forall statement. */
    processing_upc_forall = TRUE;
    sp = add_statement(stmk_upc_forall, /*compiler_generated=*/FALSE);
  } else
#endif /* UPC_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    /* We don't know yet whether we have a "plain old" for loop or a range-
       based for loop and they have different statement kinds.  Assume it's
       a "plain old" for statement for now and fix it later if we find that
       it's a range-based for. */
    sp = add_statement(stmk_for, /*compiler_generated=*/FALSE);
  }  /* if */
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_for, sp, (an_object_lifetime_ptr)NULL);
  check_assertion_str(processing_upc_forall || curr_token == tok_for,
                      "for_statement: expected for");
  /* Consume the "for" token. */
  (void)get_token();
  if (curr_token == tok_coroutine_await) {
    use_await = TRUE;
    await_pos = pos_curr_token;
    (void)get_token();
  }  /* if */
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  if (C_mode() || find_for_loop_separator() == tok_semicolon) {
    /* This code is for "plain old" for statements (i.e., C, pre-C++11, UPC C)
       as well as C++20 range-based for statements that can have an optional
       init-statement.  Scan an initializing expression or declaration if it is
       present.  It will be added to the correct place in the stmk_for entry.
       (Note: find_for_loop_separator uses the grammar disambiguation code,
       which currently cannot be used in C mode.) */
    add_stop_token(tok_semicolon);
    for_init_statement(&iterator_pointers_block);
    remove_stop_token(tok_semicolon);
  }  /* if */
  if (!C_mode() && find_for_loop_separator() == tok_colon) {
    /* Now that it is known that we're scanning a range-based for statement,
       we need to go back and fix up the current statement and statement stack
       to reflect this. */
    a_for_loop_ptr  flip = sp->variant.for_loop.extra_info;
    if (!range_based_for_enabled) {
      an_error_severity  sev = clang_mode ? es_warning : es_error;
      pos_diagnostic(sev, ec_range_based_for_nonstandard, &pos_curr_token);
    }  /* if */
    is_range_based_for = TRUE;
    struct_stmt_stack[depth_stmt_stack].kind = ssk_range_based_for;
    scope_stack_top().is_for_init_block = FALSE;
    set_statement_kind(sp, (a_statement_kind)stmk_range_based_for);
    rbflp = sp->variant.range_based_for_loop.extra_info;
    /* Copy any initialized items from flip to rbflp (flip will not be part
       of the IL). */
    rbflp->initialization = flip->initialization;
    rbflp->range_based_for_scope = flip->for_init_scope;
    rbflp->use_await = use_await;
    if (rbflp->initialization != NULL) {
      /* We scanned an initialization statement. */
      a_source_position pos;
      /* If the initialization statement has been turned into a block, use
         the position information from the first statement in the block. */
      if (rbflp->initialization->kind == (a_statement_kind)stmk_block &&
          rbflp->initialization->position.seq == 0) {
        pos = rbflp->initialization->variant.block.statements->position;
      } else {
        pos = rbflp->initialization->position;
      }  /* if */
      if (!init_statement_allowed_in_range_based_for) {
        pos_error(ec_init_stmt_in_range_for_nonstandard, &pos);
      } else if (gpp_mode && !cpp20_mode) {
        /* GNU 9.0 and later allow an init-statement in pre-C++20 modes with a
           warning. */
        if (!already_diagnosed_init_in_range_for && !in_system_header()) {
          pos_warning(ec_init_stmt_in_range_for_nonstandard, &pos);
          already_diagnosed_init_in_range_for = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (use_await) {
    /* co_await may only be applied to a range-based for. */
    pos_error(ec_co_await_on_non_range_based_for, &await_pos);
  }  /* if */
  if (is_range_based_for) {
    a_control_flow_descr_ptr  cfdp;
    /* A range-based-for has two scopes, both of which are pushed in
       preparation for scanning the for-range-declaration (if an init-statement
       was present, one of the scopes has already been pushed). */
    a_decl_parse_state dps;
    check_assertion(rbflp != NULL);
    if (rbflp->range_based_for_scope == NULL) {
      /* No optional initialization statement was present, so no scope has
         been created for this statement yet; create the outermost scope. */
      rbflp->range_based_for_scope =
                             start_fabricated_block_scope_for_enhanced_for(
                                                     &iterator_pointers_block);
    }  /* if */
    /* Push the inner scope before scanning the declaration. */
    rbflp->iterator_scope = start_fabricated_block_scope_for_enhanced_for(
                                                          &rbf_pointers_block);
    /* Add a control flow entry to represent the range-based-for block. */
    cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_block);
    cfdp->source_pos = pos_curr_token;
    cfdp->variant.block.object_lifetime = curr_object_lifetime;
    add_to_control_flow_descr_list(cfdp);
    add_stop_token(tok_colon);
    /* Scan the for-range-declaration (in the iterator_scope). */
    for_range_declaration(&dps);
    if (dps.sym != NULL && symbol_is(dps.sym, sk_variable)) {
      rbflp->iterator = dps.sym->variant.variable.ptr;
      if (rbflp->iterator != NULL) {
        rbflp->iterator->is_enhanced_for_iterator = TRUE;
      }  /* if */
    }  /* if */
    /* Pop scope to get back to the scope where the expression needs to
       be scanned.  The scope will be re-activated after the expression
       is scanned. */
    pop_block_scope(/*is_final_pop=*/FALSE);
    (void)required_token(tok_colon, ec_exp_colon);
    remove_stop_token(tok_colon);
    /* Scan the expression or braced-init-list. */
    expr_tok_seq_number = curr_token_sequence_number;
    scan_range_based_for_expression(sp, &range_pos);
    /* Perform the semantic checks and build the IL. */
    check_range_based_for_statement(sp,
                                    &range_pos,
                                    expr_tok_seq_number,
                                    &rbf_pointers_block);
    /* Return to the iterator scope for the dependent statement. */
    push_block_reactivation_scope(rbflp->iterator_scope,
                                  &rbf_pointers_block);
    if (dps.is_struct_binding_decl) {
      define_struct_bindings(&dps);
    }  /* if */
  } else {
    /* A plain-old-for loop (or a UPC forall). */
    add_stop_token(tok_semicolon);
    if (curr_token == tok_semicolon) {
      /* Controlling expression was omitted. */
    } else {
      /* Scan the condition, which in C++ may be a condition declaration. */
      scan_condition(sp, &is_condition_decl);
    }  /* if */
    (void)required_token(tok_semicolon, ec_exp_semicolon);
    if (!processing_upc_forall) {
      remove_stop_token(tok_semicolon);
    }  /* if */
    /* Scan the incrementing expression if it is present. */
    /* coverity[dead_error_condition] */
    if (curr_token != tok_rparen &&
        !(processing_upc_forall && curr_token == tok_semicolon)) {
      a_reachability_summary saved_reachability;
      saved_reachability = curr_reachability;
      set_reachable(curr_reachability);
      /* Be sure that no used-before-set warnings are issued in scanning
         the increment expression -- after all, a variable it references could
         be set within the body of the loop. */
      saved_flag = suppress_used_before_set_warnings;
      suppress_used_before_set_warnings = TRUE;
      sp->variant.for_loop.extra_info->increment =
                        scan_void_expression(/*repeated_in_loop=*/TRUE,
                                             /*marked_as_gnu_extension=*/FALSE,
                                             /*is_statement_expr=*/FALSE,
                                             (a_dynamic_init_ptr*)NULL,
                                             (an_init_component*)NULL);
      /* Restore the global variable. */
      suppress_used_before_set_warnings = saved_flag;
      curr_reachability = saved_reachability;
    }  /* if */
#if UPC_EXTENSIONS_ALLOWED
    /* Process the affinity expression. */
    if (processing_upc_forall) {
      /* Go past the required semicolon. */
      (void)required_token(tok_semicolon, ec_exp_semicolon);
      remove_stop_token(tok_semicolon);
      if (curr_token == tok_rparen) {
        /* Affinity expression was omitted. */
      } else if (curr_token == tok_continue) {
        /* Skip the "continue" and treat as an omitted affinity expression. */
        (void)get_token();
      } else {
        /* Scan the affinity expression. */
        affinity_expr = scan_upc_forall_affinity();
      }  /* if */
    }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  }  /* if */
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
#if UPC_EXTENSIONS_ALLOWED
  if (processing_upc_forall) {
    if (affinity_forall_loop != NULL && affinity_expr != NULL) {
      /* Ignore the affinity expression since we are inside another forall
         loop, and it will never be needed. */
      pos_remark(ec_nested_upc_forall, &error_position);
      affinity_expr = NULL;
    }  /* if */
    if (affinity_expr != NULL) {
      /* Save the affinity expression for later reference. */
      sp->variant.for_loop.extra_info->affinity = affinity_expr;
      affinity_forall_loop = sp;
    }  /* if */
    /* When scanning the dependent statement, save and restore the pointer to
       the innermost forall loop, replacing it with a pointer to the current
       statement so we can check for attempts to branch into or out of the
       loop. */
    saved_innermost_forall_loop = innermost_forall_loop;
    innermost_forall_loop = sp;
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  /* Scan the dependent statement. */
  dependent_statement();
  if (!assume_loop_reachable) warn_if_loop_has_no_labels(&stmt_pos);
#if UPC_EXTENSIONS_ALLOWED
  /* Restore the innermost forall loop tracking. */
  if (processing_upc_forall) {
    innermost_forall_loop = saved_innermost_forall_loop;
    if (affinity_expr != NULL) {
      affinity_forall_loop = NULL;
    }  /* if */
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  /* Define the "continue" label, if it is needed. */
  define_continue_label();
  /* End the condition block, if necessary. */
  if (is_condition_decl) finish_condition_block();
  /* If there is a break label, create its associated "definition" (statement)
     at this point, to ensure that the object lifetime associated with the
     label is the "for" loop scope and not the init-statement scope (which
     will be cleaned up after the "break" is executed).  Something like this:
       struct D { D(); ~D(); operator bool(); };
       void g() {
         for (D d0; D d1;) {
           D d2;
           break;
         }
       }
     is essentially equivalent to:
       { D d0;
         for (; D d1;) {
           D d2;
           goto break_label;  // Destroys d2 and d1, but not d0.
         }
         break_label:;
         // Cleanup of d0 happens here.
       }
  */
  { a_struct_stmt_stack_entry_ptr  sssep = &struct_stmt_stack_top();
    break_label = sssep->break_label;
    if (break_label != NULL) {
      a_control_flow_descr_ptr  break_statements = sssep->break_statements;
      define_implicit_label(break_label, break_statements,
                            /*add_to_stmt_list=*/FALSE);
      sssep->break_label = NULL;
    }  /* if */
  }
  if (is_range_based_for) {
    /* End the control flow block. */
    add_to_control_flow_descr_list(
       alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_end_of_block));
    /* Pop the scopes that have been pushed. */
    finish_block_scope_for_enhanced_for();
    finish_block_scope_for_enhanced_for();
  } else {
    /* If the for-loop supplement contains a non-NULL scope pointer, it means
       a block scope was pushed for a for-init declaration. */
    if (sp->variant.for_loop.extra_info->for_init_scope != NULL) {
      /* Terminate the for-init scope. */
      finish_for_init_block();
    }  /* if */
  }  /* if */
  /* Pop the structured statement stack. */
  pop_stmt_stack();
  if (break_label != NULL) {
    set_reachable(curr_reachability);
    add_statement_list(break_label->exec_stmt, /*reachable=*/TRUE);
  }  /* if */
  /* If a label appeared in the context of the statement that was just
     terminated, it may be appropriate to push a new object lifetime for
     the scope being resumed. */
  reset_curr_block_object_lifetime(sp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sp->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Pop a scope in C99 mode. */
  if (need_c99_stmt_scope) pop_statement_scope();
  if (microsoft_mode && !is_range_based_for) {
    /* Microsoft compilers allow declarations in loop scopes to conflict
       with associated condition-scope and for-init-scope declarations when
       a "for" (but not a "for each") loop previously appeared in the loop
       scope.  For example:
         for (int i = 0; int c = i<10; ++i) {
           for (; false;);
           int i, c;  // Accepted in Microsoft mode.
         }
       The for-init-scope and condition-scope scope stack entries must be
       marked accordingly.
    */
    a_scope_stack_entry_ptr  ssep = &scope_stack_top();
    if (ssep->is_loop_scope) {
      ssep -= 1;
      if (ssep->kind == (a_scope_kind)sck_condition) {
        ssep->is_dissociated_from_loop_scope = TRUE;
        ssep -= 1;
      }  /* if */
      if (ssep->is_for_init_block) {
        ssep->is_dissociated_from_loop_scope = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* for_statement */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void for_each_statement(void)
/*
Scan a "for each" statement and add it to the current statement sequence.
Syntax (ECMA-372 section 16.2.1):

  for each (type-specifier-seq declarator in assignment-expression) statement

Where "in" is a context-sensitive keyword.
*/
{
  a_statement_ptr         sp;
  a_for_each_loop_ptr     felp;
  a_boolean               assume_loop_reachable;
  a_source_position       stmt_pos, collection_pos;
  a_token_sequence_number collection_expr_tok_seq_number;
  a_scope_pointers_block  pointers_block;
  a_symbol_header_ptr     sym_hdr;
  an_operand              prev_decl_iterator;

  db_enter(3, "for_each_statement");

  stmt_pos = pos_curr_token;
  assume_loop_reachable = curr_reachability.reachable ||
                          curr_reachability.suppress_unreachable_warning;
  /* Allocate the "for each" statement. */
  sp = add_statement(stmk_for_each, /*compiler_generated=*/FALSE);
  felp = sp->variant.for_each_loop.extra_info;
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Push an entry on the structured statement stack. */
  push_stmt_stack(ssk_for_each, sp, (an_object_lifetime_ptr)NULL);
  /* Ignore the initial "for each". */
  check_assertion_str(curr_token == tok_for_each,
                      "for_each_statement: expected for each");
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Push the outer scope. */
  felp->for_each_scope = start_fabricated_block_scope_for_enhanced_for(
                                             (a_scope_pointers_block_ptr)NULL);
  /* Push the iterator scope. */
  felp->iterator_scope = start_fabricated_block_scope_for_enhanced_for(
                                                              &pointers_block);
  if (curr_token == tok_identifier &&
      next_token_full((a_token_sequence_number *)NULL, &sym_hdr) ==
                                                              tok_identifier &&
      symbol_header_is_for_identifier_string(sym_hdr, "in")) {
    /* This for-each uses a previously-declared variable as the iterator
       instead of declaring a new one.  VC10 seems to allow this (at least
       without weird spurious errors) only for a simple identifier. */
    scan_previously_decl_iterator_name(felp, &prev_decl_iterator);
  } else {
    /* Normal case.  Scan the iterator declaration. */
    for_each_iterator_declaration(sp);
  }  /* if */
  /* Pop the iterator scope so the collection expression can be scanned outside
     of it.  The scope will be re-pushed below. */
  pop_block_scope(/*is_final_pop=*/FALSE);
  (void)check_context_sensitive_keyword(tok_in, "in");
  (void)required_token(tok_in, ec_exp_in);
  /* Scan the collection expression. */
  collection_expr_tok_seq_number = curr_token_sequence_number;
  scan_for_each_expression(sp, &collection_pos);
  /* Determine the pattern of the for-each statement and generate the IL
     for all the loop-control pieces. */
  check_for_each_statement(sp,
                           &prev_decl_iterator,
                           &collection_pos,
                           collection_expr_tok_seq_number,
                           &pointers_block);
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Push the iterator scope (created previously). */
  check_assertion(felp->for_each_scope == scope_stack_top().il_scope);
  check_assertion(felp->iterator_scope != NULL);
  push_block_reactivation_scope(felp->iterator_scope, &pointers_block);
  /* Scan the dependent statement. */
  dependent_statement();
  if (!assume_loop_reachable) warn_if_loop_has_no_labels(&stmt_pos);
  /* Define the "continue" label, if it is needed. */
  define_continue_label();
  /* Pop the iterator scope.  We want the continue label to transfer to
     any destruction required for the iterator variable. */
  finish_block_scope_for_enhanced_for();
  /* Pop the for-each scope. */
  finish_block_scope_for_enhanced_for();
  /* Pop the structured statement stack. */
  pop_stmt_stack();
  /* If a label appeared in the context of the statement that was just
     terminated, it may be appropriate to push a new object lifetime for
     the scope being resumed. */
  reset_curr_block_object_lifetime(sp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sp->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  db_exit();
}  /* for_each_statement */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void report_goto_past_init(a_control_flow_descr_ptr  start_cfdp,
                                  a_control_flow_descr_ptr  end_cfdp,
                                  a_source_position         *error_pos,
                                  a_diagnostic_ptr          *prev_dp,
                                  an_error_severity         *prev_severity)
/*
This routine moves from entry start_cfdp to entry end_cfdp on the
control_flow_descr_list looking for init entries, which point to stmk_init
statements and represent initializing declarations.  For any that are found,
issue a diagnostic complaining about skipping over an initialization.
If *prev_severity is es_none, no diagnostic is in the process of being
generated.  If it not es_none, *prev_dp is the diagnostic that is being
generated.
*/
{
  a_control_flow_descr_ptr  cfdp;
  a_variable_ptr            vp;

  db_enter(4, "report_goto_past_init");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "start_cfdp = ");
    db_cfd(start_cfdp);
    fprintf(f_debug, "end_cfdp = ");
    db_cfd(end_cfdp);
  }  /* if */
#endif /* DEBUG */
  if (end_cfdp->parent != start_cfdp->parent) {
    report_goto_past_init(start_cfdp, end_cfdp->parent->prev, error_pos,
                          prev_dp, prev_severity);
    start_cfdp = end_cfdp->parent->next;
  }  /* if */
  cfdp = start_cfdp;
  /*lint --e{850} cfdp modified in loop */
  for (cfdp = start_cfdp; ; cfdp = cfdp->next) {
#if DEBUG
#if CHECKING
    if (cfdp->parent != end_cfdp->parent ||
        cfdp->id_number > end_cfdp->id_number) {
      if (debug_level > 0) {
        fprintf(f_debug, "cfdp = ");
        db_cfd_and_parents(cfdp);
        fprintf(f_debug, "end_cfdp = ");
        db_cfd_and_parents(end_cfdp);
      }  /* if */
      internal_error("report_goto_past_init: start > end or parent mismatch");
    }  /* if */
#endif /* CHECKING */
#endif /* DEBUG */
    if (cfdp->kind == (a_control_flow_descr_kind)cfdk_init) {
      an_error_severity  severity = es_none;
      a_type_ptr         tp;
      a_statement_ptr    sp;

      sp = cfdp->variant.init.statement;
      vp = cfdp->variant.init.variable;
      if (vp != NULL && !cfdp->variant.init.is_vla_variable) {
        /* We only issue a diagnostic for jumping over an initialization of
           an automatic variable (see [stmt.decl], para 3). */
        if (!var_has_static_or_thread_storage_duration(vp)) {
          if (C_mode()) {
            /* Just a warning in C mode. */
            severity = es_warning;
          } else {
            /* C++ mode. */
            tp = vp->type;
            if (is_array_type(tp)) tp = underlying_array_element_type(tp);
            tp = skip_typerefs(tp);
            if (sp == NULL) {
              /* cfdp stands for a trivial non-POD initialization.  No
                 statement is needed for such initializations, but a branch
                 over the initialization still must be diagnosed. */
              severity = strict_ansi_mode ? strict_ansi_error_severity
                                          : es_warning;
            } else if ((gnu_mode && !(clang_mode && ms_compat)) ||
                       (ms_extensions && !ms_permissive)) {
              /* An error in GNU mode and clang mode as well (except in
                 Microsoft compatibility mode).  Microsoft issues an error
                 except in "permissive" mode. */
              severity = es_error;
            } else if (is_class_struct_union_type(tp) && !ms_extensions) {
              severity = es_error;
            } else if (strict_ansi_mode) {
              severity = strict_ansi_error_severity;
            } else {
              severity = es_warning;
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        if (sp->kind == (a_statement_kind)stmk_vla_decl) {
          if (!sp->variant.vla.is_typedef_decl) {
            vp = sp->variant.vla.variant.variable;
          }  /* if */
        } else {
          /* Must be a stmk_set_vla_size statement. */
          check_assertion(sp->kind == (a_statement_kind)stmk_set_vla_size);
        }  /* if */
        severity = es_error;
      }  /* if */
      if (severity != es_none) {
        if (severity != *prev_severity) {
          if (*prev_severity != es_none) end_diagnostic(*prev_dp);
          /* This is the first initializing declaration seen.  Issue the
             header diagnostic. */
          *prev_dp = pos_start_diagnostic(severity,
                                          ec_branch_past_initialization,
                                          error_pos);
          *prev_severity = severity;
        }  /* if */
        if (vp != NULL) {
          /* Issue the diagnostic addendum that identifies this particular
             variable. */
          if (vp->is_anonymous_parent_object) {
            add_diag_info_with_pos_insert(*prev_dp,
                                          ec_anon_union_at_decl_position,
                                          &vp->source_corresp.decl_position);
          } else {
            sym_add_diag_info(*prev_dp,
                              (sp != NULL &&
                               sp->kind == (a_statement_kind)stmk_vla_decl) ?
                                 ec_vla_name_at_decl_position :
                                 ec_name_at_decl_position,
                              (a_symbol_ptr)vp->source_corresp.assoc_info);
          }  /* if */
        } else {
          /* Diagnostic addendum that identifies the VLA declaration. */
          add_diag_info_with_pos_insert(*prev_dp, ec_vla_at_decl_pos,
                                        &sp->position);
        }  /* if */
      }  /* if */
    }  /* if */
    if (cfdp == end_cfdp) break;
    if (cfdp->kind == (a_control_flow_descr_kind)cfdk_block) {
      cfdp = cfdp->variant.block.end_of_block;
#if DEBUG
      if (debug_level >= 4) {
        fprintf(f_debug, "jumped over block -- cfdp = ");
        db_cfd(cfdp);
      }  /* if */
#endif /* DEBUG */
      if (cfdp == end_cfdp) break;
    }  /* if */
  }  /* for */
  db_exit();
}  /* report_goto_past_init */


static void check_goto_and_label(a_control_flow_descr_ptr  label_cfdp,
                                 a_control_flow_descr_ptr  goto_cfdp,
                                 a_boolean                 is_forward)
/*
If is_forward is TRUE, a goto was previously recorded in goto_cfdp and now
that the label it referenced has been encountered (represented by label_cfdp)
we can determine whether the goto entailed jumping over an initializing
declaration.  If is_forward is FALSE, a label was encountered previously.
we are now at the goto, and the same determination has to be made.  This
routine sets up the terms for scanning the control_flow_descr_list to
diagnose the condition.
*/
{
  a_control_flow_descr_ptr  cfdp, start_cfdp, common_parent;
  an_error_severity         severity;
  an_object_lifetime_ptr    label_olp, *goto_olp_addr;

  db_enter(4, "check_goto_and_label");
  if (is_forward && goto_cfdp->variant.goto_statement.prev_goto != NULL) {
    /* All forward gotos to a given label are linked together by the
       prev_goto field of the control-flow-descr entries.  Follow the list
       up to process them in the order they appear in the program. */
    check_goto_and_label(label_cfdp,
                         goto_cfdp->variant.goto_statement.prev_goto,
                         /*is_forward=*/TRUE);
  }  /* if */
  start_cfdp = NULL;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "checking %s jump from:  ",
            is_forward ? "forward" : "backwards");
    db_cfd_and_parents(goto_cfdp);
    fprintf(f_debug, "...and jumping to:  ");
    db_cfd_and_parents(label_cfdp);
  }  /* if */
#endif /* DEBUG */
#if UPC_EXTENSIONS_ALLOWED
  if (label_cfdp->enclosing_forall != goto_cfdp->enclosing_forall) {
    /* goto and label are either in different forall statements or one is in
       a forall and the other is not.  (The UPC specification indicates that
       this leads to undefined behavior when executed.)   */
    pos_warning(ec_exit_forall, &goto_cfdp->source_pos);
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  if (check_for_branch_into_goto_protected_block(label_cfdp, goto_cfdp)) {
    /* Ignore the jump-over-initialization errors -- this is an illegal
       branch into a catch clause or try block.  (The diagnostic has
       already been issued.) */
  } else if (label_cfdp->parent == goto_cfdp->parent) {
    /* Label and goto are in the same block:

           goto L;             // forward goto
               : FFFFF         
               : FFFFF
               : FFFFF
           L:
               :
           goto L;             // backwards goto

       In this case the region marked "FFFFF" needs to be searched for
       forward gotos, but backwards gotos are always allowed. */
    if (is_forward) {
      /* Start looking for initializing declarations at the point immediately
         following the goto statement. */
      start_cfdp = goto_cfdp->next;
    }  /* if */
  } else if (is_on_cfd_parent_list(goto_cfdp->parent, label_cfdp)) {
    /* A goto from an outer block to a label in a nested block:

           goto L;             // forward goto
               : FFFFF
           {     FFFFF         // start of inner block
               : FFFFF BBBBB
               : FFFFF BBBBB
           L:
               :
           }                   // end of inner block
           goto L;             // backwards goto

       The region marked "FFFFF" is searched for forward gotos, and the
       region marked "BBBBB" is searched for backward gotos. */
    if (is_forward) {
      /* Start looking for initializing declarations at the point immediately
         following the goto statement. */
      start_cfdp = goto_cfdp->next;
    } else {
      /* Start looking for initializing declarations at the top of the
         outermost block that both contains the label and is contained by
         the block to which the goto belongs. */
      cfdp = label_cfdp->parent;
      while (cfdp->parent != goto_cfdp->parent) {
        cfdp = cfdp->parent;
      }  /* while */
      start_cfdp = cfdp->next;
    }  /* if */
  } else if (is_on_cfd_parent_list(label_cfdp->parent, goto_cfdp)) {
    /* A goto from an inner block to a label in an outer block:

           {                     // start of inner block
             goto L;             // forward goto
           }                     // end of inner block
               : FFFFF
               : FFFFF
               : FFFFF
           L:
               :
           {                     // start of inner block
             goto L;             // backwards goto
           }                     // end of inner block

       The region marked "FFFFF" is searched for forward gotos, but
       backwards gotos are always allowed. */
    if (is_forward) {
      /* Start looking for initializing declarations at the point immediately
         following the outermost block that both contains the goto and is
         contained by the block to which the label belongs. */
      cfdp = goto_cfdp->parent;
      while (cfdp->parent != label_cfdp->parent) {
        cfdp = cfdp->parent;
      }  /* while */
      start_cfdp = cfdp->variant.block.end_of_block->next;
    }  /* if */
  } else {
    /* goto from an inner block to a label in an inner block.

           {                     // start of inner block
             goto L;             // forward goto
           }                     // end of inner block
               : FFFFF
           {     FFFFF           // start of inner block
               : FFFFF BBBBB
               : FFFFF BBBBB
           L:
               :
           }                     // end of inner block

           {                     // start of inner block
             goto L;             // forward goto
           }                     // end of inner block

       The region marked "FFFFF" is searched for forward gotos, and the
       region marked "BBBBB" is searched for backward gotos. */
    /* Find the block that is the "common parent" -- the innermost block
       containing both the goto and the label. */
    common_parent = label_cfdp->parent;
    while (!is_on_cfd_parent_list(common_parent, goto_cfdp->parent)) {
      common_parent = common_parent->parent;
    }  /* while */
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, " common parent = ");
      db_cfd(common_parent);
    }  /* if */
#endif /* DEBUG */
    /* For forward gotos, start looking for initializing declarations at the
       point immediately following the outermost block that both contains the
       goto and is immediately contained by the common parent.  For backwards
       gotos, start looking at the top of the outermost block that both
       contains the label and is immediately contained by the common parent. */
    cfdp = is_forward ? goto_cfdp->parent : label_cfdp->parent;
    check_assertion(cfdp != common_parent);
    while (cfdp->parent != common_parent) {
      cfdp = cfdp->parent;
    }  /* while */
    start_cfdp = is_forward ?
                   cfdp->variant.block.end_of_block->next : cfdp->next;
  }  /* if */
  if (start_cfdp == NULL) {
    /* No checking is required. */
  } else {
    /* If the above algorithm indicates starting at a block that contains
       the label, enter that block and start at its first statement.
       (Otherwise the search will try to skip the block.) */
    a_diagnostic_ptr dp = NULL;
    while (start_cfdp->kind == (a_control_flow_descr_kind)cfdk_block &&
           is_on_cfd_parent_list(start_cfdp, label_cfdp)) {
      start_cfdp = start_cfdp->next;
    }  /* if */
    /* Now do the search for an initializing declaration.  On the path
       between the starting entry, as determined above, and the entry for the
       label.  Check the error severity to see if a diagnostic was issued, in
       which case terminate the multi-line message. */
    severity = es_none;
    report_goto_past_init(start_cfdp, label_cfdp,
                          &goto_cfdp->source_pos, &dp, &severity);
    if (severity != es_none) end_diagnostic(dp);
  }  /* if */
  if (!C_mode()) {
    /* Find the common object lifetime containing both the goto statement and
       the label, and update the goto statement's lifetime field to point to
       it. */
    label_olp = label_cfdp->variant.label_statement->variant.label.lifetime;
    goto_olp_addr = &goto_cfdp->variant.goto_statement.ptr->
                                                      variant.label.lifetime;
    *goto_olp_addr = common_object_lifetime(label_olp, *goto_olp_addr);
  }  /* if */
#if VLA_DEALLOCATIONS_IN_IL
  if (vla_enabled && vla_deallocations_in_il) {
    /* Put out vla-dealloc statements on the goto, if necessary. */
    add_vla_dealloc_stmts_for_goto(goto_cfdp, label_cfdp);
  }  /* if */
#endif /* VLA_DEALLOCATIONS_IN_IL */
  db_exit();
}  /* check_goto_and_label */


static void check_for_jump_over_initialization(a_statement_ptr    sp,
                                               a_source_position  *pos)
/*
sp is either a label statement or a goto statement.  If this is a goto
statement and the label it references has not yet been seen (i.e., if it
is a "forward goto"), record some information about it for later use in
detecting jumps over initializing declarations.  If this is a "backward goto"
statement, issue a diagnostic if it jumps over any initializing declarations.
If this is a label statement, check the associated forward gotos to see if
any of them jumped over initializing declarations.  Diagnostics are put out
at the point of the goto statement, even for forward gotos, where the
condition is not recognized till the label statement is reached.
*/
{
  a_symbol_ptr              label_sym;
  a_control_flow_descr_ptr  label_cfdp, goto_cfdp;

  db_enter(3, "check_for_jump_over_initialization");
  check_assertion (sp->kind == (a_statement_kind)stmk_label ||
                   sp->kind == (a_statement_kind)stmk_goto);
  label_sym = (a_symbol_ptr)sp->variant.label.ptr->source_corresp.assoc_info;
  if (sp->kind == (a_statement_kind)stmk_label) {
    /* This is the definition of the label. */
    goto_cfdp = label_sym->variant.label.assoc_control_flow_descr;
    label_cfdp =
            alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_label);
    label_cfdp->variant.label_statement = sp;
    label_cfdp->source_pos = *pos;
#if UPC_EXTENSIONS_ALLOWED
    /* Keep track of any enclosing forall loop to make sure no exits
       or entries of forall loops are attempted. */
    label_cfdp->enclosing_forall = innermost_forall_loop;
#endif /* UPC_EXTENSIONS_ALLOWED */
    add_to_control_flow_descr_list(label_cfdp);
    label_sym->variant.label.assoc_control_flow_descr = label_cfdp;
    if (goto_cfdp != NULL) {
      /* There was at least one forward goto referencing this label.  For
         each check whether it jumped over any initializing declarations. */
      check_goto_and_label(label_cfdp, goto_cfdp, /*is_forward=*/TRUE);
    }  /* if */
  } else {
    /* This is a goto to the label. */
    /* Allocate and fill in a goto entry. */
    goto_cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_goto);
    goto_cfdp->source_pos = *pos;
    goto_cfdp->variant.goto_statement.ptr = sp;
#if UPC_EXTENSIONS_ALLOWED
    /* Keep track of any enclosing forall loop so we can diagnose attempts
       to jump into or out of such loops. */
    goto_cfdp->enclosing_forall = innermost_forall_loop;
#endif /* UPC_EXTENSIONS_ALLOWED */
    add_to_control_flow_descr_list(goto_cfdp);
    if (label_sym->defined) {
      /* This is a backwards goto -- i.e., it references a label that has
         already been defined.  Check whether it jumps over any initializing
         declarations.  Note that the goto entry has been added to the
         control_flow_descr_list; once the checking has been done it is
         taken off again, since only forward gotos need to remain on the
         list (and then only till the label is seen). */
      label_cfdp = label_sym->variant.label.assoc_control_flow_descr;
      check_goto_and_label(label_cfdp, goto_cfdp, /*is_forward=*/FALSE);
    } else {
      /* This is a forward goto -- i.e., it references a label that has not
         yet been defined.  Record information about it so that, when the
         label definition is reached, a check can made whether it involves
         jumping over any initializing declarations. */
      goto_cfdp->variant.goto_statement.prev_goto =
                        label_sym->variant.label.assoc_control_flow_descr;
      label_sym->variant.label.assoc_control_flow_descr = goto_cfdp;
    }  /* if */
  }  /* if */
  db_exit();
}  /* check_for_jump_over_initialization */


static void goto_statement(void)
/*
Scan a "goto" statement and add it to the current statement sequence.
The syntax is:

3.6.6  jump-statement:
		goto identifier ;

See also 3.6.6.1.
*/
#if GNU_EXTENSIONS_ALLOWED
/*
GNU allows a syntax similar to Fortran's assigned goto:

	jump_statement:
		goto * expr ;
*/
#endif /* GNU_EXTENSIONS_ALLOWED */
{
  a_statement_ptr    sp;
  a_source_position  goto_pos;
  a_statement_kind   stmk;

  db_enter(3, "goto_statement");
  check_for_unreachable_code();
  stmk = (a_statement_kind)stmk_goto;
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode && next_token() == tok_star) {
    stmk = (a_statement_kind)stmk_assigned_goto;
    if (strict_ansi_mode) {
      diagnostic(strict_ansi_error_severity, ec_nonstd_assigned_goto);
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Allocate the statement. */
  sp = add_statement(stmk, /*compiler_generated=*/FALSE);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  goto_pos = pos_curr_token;
  /* Ignore the initial "goto". */
#if CHECKING
  if (curr_token != tok_goto) internal_error("goto_statement: expected goto");
#endif /* CHECKING */
  (void)get_token();
  add_stop_token(tok_semicolon);
#if GNU_EXTENSIONS_ALLOWED
  if (stmk == (a_statement_kind)stmk_assigned_goto) {
    /* Discard the star. */
#if CHECKING
    if (curr_token != tok_star) internal_error("goto_statement: expected '*'");
#endif /* CHECKING */
    (void)get_token();
    /* Scan the expression following, which must have type (void *).
       const void * is also acceptable. */
    sp->expr = scan_typed_expression(make_pointer_type(void_type()),
                                     make_pointer_type(
                                       make_qualified_type(void_type(),
                                                           TQ_CONST)),
				     ec_assigned_goto_requires_void_ptr);
  } else
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    /* Scan the label identifier. */
    sp->variant.label.ptr = scan_label(/*is_definition=*/FALSE,
                                       /*is_declaration=*/FALSE);
    if (!C_mode()) {
      /* Set the object lifetime.  It is a provisional setting and may
	 be changed based on the lifetime of the label definition. */
      sp->variant.label.lifetime =
                        innermost_block_object_lifetime(curr_object_lifetime);
    }  /* if */
    /* If this is a forward reference to a label, record information about
       the goto to allow diagnosis of jump-over-initialization errors.  If
       it is backward reference, do the checking immediately. */
    check_for_jump_over_initialization(sp, &goto_pos);
  }  /* else */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (curr_token == tok_semicolon) {
    curr_construct_end_position = end_pos_curr_token;
  }  /* if */
  sp->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);
  db_exit();
}  /* goto_statement */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean has_nested_finally_clause(a_struct_stmt_stack_entry_ptr sssep)
/*
Return TRUE if a C++/CLI finally clause is nested in the structured statement
represented by *sssep.
*/
{
  a_boolean result = FALSE;

  check_assertion(cli_or_cx_enabled);
  while (sssep != &struct_stmt_stack[depth_stmt_stack]) {
    if (sssep->parsing_finally_clause) {
      result = TRUE;
      break;
    }  /* if */
    sssep++;
  }  /* while */
  return result;
}  /* has_nested_finally_clause */


a_boolean inside_finally_clause()
/*
Return TRUE if the top structured statement is nested in a C++/CLI finally
clause.
*/
{
  int       depth;
  a_boolean result = FALSE;

  check_assertion(cli_or_cx_enabled);
  /* Note that we look at entry [0] because we have to consider function
     try-blocks. */
  for (depth = depth_stmt_stack; depth >= 0; depth--) {
    if (struct_stmt_stack[depth].parsing_finally_clause) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* inside_finally_clause */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void continue_statement(void)
/*
Scan a "continue" statement and add it to the current statement sequence.
The syntax is:

3.6.6  jump-statement:
		continue ;

See also 3.6.6.2.
*/
{
  a_struct_stmt_stack_entry_ptr sssep;
  a_statement_ptr               goto_stmt;

  db_enter(3, "continue_statement");
  check_for_unreachable_code();
  /* See if we are within a loop body (while, do, or for) by looking at the
     entries in the structured statement stack. */
  sssep = find_enclosing_struct_stmt(/*find_switch=*/FALSE,
                                     /*find_loop=*/TRUE);
  if (sssep == NULL) {
    /* No appropriate structured statement was found. */
    pos_error(ec_continue_must_be_in_loop, &error_position);
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cli_or_cx_enabled && has_nested_finally_clause(sssep)) {
    /* A continue statement cannot be inside a C++/CLI finally clause. */
    pos_error(ec_continue_cannot_be_in_finally_block, &error_position);
    sssep = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  /* Add a "goto" to the continue label. */
  add_goto_to_continue_label(sssep, /*is_leave=*/FALSE, &goto_stmt);
  /* Ignore the initial "continue". */
#if CHECKING
  if (curr_token != tok_continue) {
    internal_error("continue_statement: expected continue");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (curr_token == tok_semicolon) {
    curr_construct_end_position = end_pos_curr_token;
  }  /* if */
  if (goto_stmt != NULL) {
    goto_stmt->end_position = curr_construct_end_position;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  db_exit();
}  /* continue_statement */


static void add_goto_for_break(a_struct_stmt_stack_entry_ptr sssep,
                               a_source_position             *pos,
                               ARG_UNUSED a_source_position  *end_pos)
/*
Add a goto to implement a break statement.  sssep points to the structured
statement stack entry for the statement being exited.  *pos and *end_pos
give the starting and ending positions of the break statement.
*/
{
  a_statement_ptr               sp;
  a_label_ptr                   dest_label;
  a_control_flow_descr_ptr      cfdp;

  dest_label = sssep->break_label;
  if (dest_label == NULL) {
    /* The break label has not previously been used, so generate it. */
    dest_label = sssep->break_label = alloc_temp_label();
    dest_label->break_label = TRUE;
    if (sssep->kind == (a_struct_stmt_kind)ssk_switch) {
      dest_label->switch_break_label = TRUE;
    }  /* if */
  }  /* if */
  /* Allocate the goto statement. */
  sp = add_statement_at_stmt_pos(stmk_goto, pos, /*compiler_generated=*/TRUE);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sp->end_position = *end_pos;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  stmt_update_source_sequence_list(sp);
  /* Put the destination label into the goto. */
  sp->variant.label.ptr = dest_label;
  if (!C_mode()) {
    /* Set the object lifetime.  It is a provisional setting and may be
       changed based on the lifetime of the break label itself. */
    sp->variant.label.lifetime =
                        innermost_block_object_lifetime(curr_object_lifetime);
  }  /* if */
  if (!C_mode() || vla_enabled) {
    /* Allocate and fill in a goto entry.  This is done in C++ mode
       because it's needed for object lifetime management and in C mode
       when VLA support is enabled to insert vla-dealloc statements before
       forward gotos. */
    cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_goto);
    cfdp->source_pos = pos_curr_token;
    cfdp->variant.goto_statement.ptr = sp;
    add_to_control_flow_descr_list(cfdp);
    cfdp->variant.goto_statement.prev_goto = sssep->break_statements;
    sssep->break_statements = cfdp;
  }  /* if */
}  /* add_goto_for_break */

#if UPC_EXTENSIONS_ALLOWED

static void check_for_leaving_upc_forall(a_struct_stmt_stack_entry_ptr  sssep)
/*
We are parsing a break statement.  Issue a warning if it is a reachable break
out of a upc_forall statement.  (The UPC specification indicates that this
leads to undefined behavior when executed.)  sssep is the statement to which
the break applies.
*/
{
  if (upc_mode && curr_reachability.reachable_considering_hints &&
      sssep->statement != NULL &&
      sssep->statement->kind == (a_statement_kind)stmk_upc_forall) {
    pos_warning(ec_exit_forall, &error_position);
  }  /* if */
}  /* check_for_leaving_upc_forall */

#else /* !UPC_EXTENSIONS_ALLOWED */

#define check_for_leaving_upc_forall(sssep)  /* Nothing */

#endif /* UPC_EXTENSIONS_ALLOWED */

static void break_statement(void)
/*
Scan a "break" statement and add it to the current statement sequence.
The syntax is:

3.6.6  jump-statement:
		break ;

See also 3.6.6.3.
*/
{
  a_struct_stmt_stack_entry_ptr sssep;
  a_source_position             start_position, end_position;

  db_enter(3, "break_statement");
  start_position = pos_curr_token;
  check_for_unreachable_code();
  /* See if we are within a loop body (while, do, or for) or a switch
     by looking at the entries in the structured statement stack. */
  sssep = find_enclosing_struct_stmt(/*find_switch=*/TRUE,
                                     /*find_loop=*/TRUE);
  /* Binding a pragma to a break statement is disallowed.  This is partly
     a consequence of how break statements are implemented -- usually no
     explicit goto is added to the IL (so there's nothing to actually connect
     the IL pragma entry to). */
  cannot_bind_to_curr_construct();
  if (sssep == NULL) {
    /* No appropriate structured statement was found. */
    pos_error(ec_break_must_be_in_loop_or_switch, &error_position);
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cli_or_cx_enabled && has_nested_finally_clause(sssep)) {
    /* A break statement cannot be inside a C++/CLI finally clause. */
    pos_error(ec_break_cannot_be_in_finally_block, &error_position);
    sssep = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    check_for_leaving_upc_forall(sssep);
    if (sssep->kind == (a_struct_stmt_kind)ssk_switch &&
        sssep->statement->variant.switch_stmt.extra_info->cases != NULL) {
      /* If this break appears after a case label and is reachable, then we
         can presume there is a way out of the switch. */
      merge_reachability(&curr_reachability, &sssep->end_reachable);
    }  /* if */
  }  /* if */
  /* Advance over the "break". */
#if CHECKING
  if (curr_token != tok_break) {
    internal_error("break_statement: expected break");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (sssep != NULL) {
    /* Add a goto to implement the break. */
    add_goto_for_break(sssep, &start_position, &end_position);
  }  /* if */
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  db_exit();
}  /* break_statement */


static void check_void_return_okay(a_boolean         is_implicit_return,
				   an_expr_node_ptr  *return_expr)
/*
A void return (one with no expression) is being used to exit the current
routine.  If is_implicit_return is TRUE, the return was generated as
a consequence of falling off the end of a function; otherwise, the
program contained an explicit return statement with no return value
expression.  Check that a void return is okay as a way of exiting the
current routine, and also set *return_expr to point to an expression
if a return value is implied, or NULL if not.

If the return is from "main", and main returns "int", a return value
of 0 is created.  That is the defined behavior in C++ and C99 when control
reaches the end of the main routine.  That behavior is also used in C89,
in which such a return is undefined.
*/
{
  a_routine_ptr        rout = current_routine_entry();
  a_type_ptr           rout_type, tp;
  a_boolean            issue_no_value_returned_diag = FALSE;
  an_error_severity    no_returned_value_severity = es_none;
  a_scope_stack_entry  *ssep= &scope_stack[depth_innermost_function_scope];

  *return_expr = NULL;
  /* Disable return value optimization in a function that contains a void
     return statement. */
  ssep->return_value_optimization_possible = FALSE;
  ssep->il_scope->variant.routine.return_value_variable = NULL;
  rout_type = skip_typerefs(rout->type);
  check_assertion(rout_type->kind == (a_type_kind)tk_routine);
  if (rout->special_kind == (a_special_function_kind)sfk_constructor ||
#if MICROSOFT_EXTENSIONS_ALLOWED
      rout->special_kind == (a_special_function_kind)sfk_static_constructor ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      rout->special_kind == (a_special_function_kind)sfk_destructor) {
    /* Constructors and destructors have no return value. */
  } else if (rout->is_coroutine) {
    a_coroutine_descr_ptr cdp = get_coroutine_descr(rout);
    check_assertion(is_implicit_return);
    if (!cdp->error_descr && !cdp->has_return_void &&
        !is_template_dependent_type(cdp->promise->type)) {
      /* We're missing an explicit co_return statement and don't have a
         return_void to implicitly call. */
      a_symbol_ptr function_name_symbol = symbol_for(rout);
      pos_syty_diagnostic(strict_ansi_mode ?
                                           strict_ansi_discretionary_severity :
                                           es_warning,
                          ec_implicit_co_return_with_no_return_void,
                          &function_name_symbol->decl_position,
                          function_name_symbol, cdp->promise->type);
    }  /* if */
  } else {
    /* Get the routine return type. */
    if (rout->has_deducible_return_type && !rout->has_deduced_return_type) {
      deduce_return_type_from_void_operand(
                                   rout,
                                   /*keep_placeholder=*/!rout->is_lambda_body,
                                   &error_position);
    }  /* if */
    tp = rout_type->variant.routine.return_type;
    if (is_void_type(tp) || is_template_param_type(tp) || is_error_type(tp)) {
      /* A void return in a void function is okay.  Unknown template-dependent
         return type must be assumed to be okay.  Similarly, when recovering
         from errors we assume the intended type would have been acceptable. */
    } else {
      /* A return without an expression in a non-void function.  Unless a
         special case applies, this case deserves a diagnostic. */
      issue_no_value_returned_diag = TRUE;
      no_returned_value_severity = es_warning;
      /* Check for a return from main. */
      if (rout == il_header.main_routine &&
          is_integral_type(tp) &&
          f_skip_typerefs(tp)->variant.integer.int_kind ==
                                                     (an_integer_kind)ik_int) {
        /* main returning "int", so make it return 0. */
        a_constant_ptr zero = local_constant();
        make_zero_of_proper_type(tp, zero);
        *return_expr = alloc_node_for_constant(zero);
        /* Falling off the end of "main" is a special case that merits
           reduced diagnostics.  An explicit return from main (i.e.,
           "main () {return;}") doesn't get special consideration. */
        if (is_implicit_return) {
          if (!C_mode() || c99_mode) {
            /* In C++ and C99, falling off the end of main is fully
               standard. */
            issue_no_value_returned_diag = FALSE;
          } else {
            /* In pre-C99 C, falling off the end of main merits a remark. */
            no_returned_value_severity = es_remark;
          }  /* if */
        } else if (strict_ansi_mode && (!C_mode() || c99_mode)) {
          /* An explicit "return;" elicits an error in strict C++ and C99
             modes. */
          no_returned_value_severity = strict_ansi_discretionary_severity;
        }  /* if */
        release_local_constant(&zero);
      } else if (rout->is_constexpr && !relaxed_constexpr_allowed()) {
        /* A C++11 constexpr function must return a value (strictly speaking,
           this is undefined behavior, but an error seems warranted) and must
           contain exactly one return statement.  With C++14-style "relaxed"
           constexpr functions, we might return conditionally (because the
           function doesn't consist solely of a return statement) with the
           caller making sure the condition for returning is always satisfied.
           GCC doesn't issue an error for the C++11 template case until it is
           actually instantiated. */
        no_returned_value_severity = es_discretionary_error;
        if (gpp_mode && !clang_mode && rout->is_prototype_instantiation) {
          no_returned_value_severity = es_warning;
        }  /* if */
        if (!ssep->constexpr_ruled_out && is_implicit_return &&
            !special_kind_is(rout, sfk_constructor) &&
            !ssep->has_at_least_one_return) {
          /* For a C++11 function, put out a specialized diagnostic for a
             missing return statement. */
          pos_diagnostic(no_returned_value_severity,
                         ec_invalid_constexpr_body, &pos_curr_token);
          if (is_effective_error(ec_invalid_constexpr_body,
                                 no_returned_value_severity,
                                 &error_position)) {
            ssep->constexpr_ruled_out = TRUE;
          }  /* if */
          /* Do not issue an additional diagnostic. */
          no_returned_value_severity = es_none;
        }  /* if */
      } else if (strict_ansi_mode && !C_mode() && !is_implicit_return) {
          /* In strict C++ mode, the severity may be an error. */
        no_returned_value_severity = strict_ansi_discretionary_severity;
      } else if (c99_mode && !is_implicit_return) {
        /* In C99 mode a non-void (non-main) function must return a value.
           Just give a warning if we're also in Microsoft or GNU mode. */
        no_returned_value_severity = (gcc_mode || microsoft_mode) ?
                                           es_warning : es_discretionary_error;
      } else {
        /* Not "main". */
        /* See if the diagnostic level should be adjusted for other reasons. */
        if (C_mode()) {
          /* C: Issue a remark instead of a warning if the declaration of the
             function did not have an explicit type specifier (omitting the
             specifier implies "int", but may have been intended to mean "void"
             in old-style C). */
          if (!struct_stmt_stack[0].rout_type_explicitly_specified) {
            no_returned_value_severity = es_remark;
          }  /* if */
        }  /* if */
      }  /* if */
      /* Output diagnostic about no value returned from non-void function
         if necessary. */
      if (issue_no_value_returned_diag) {
        if ((int)no_returned_value_severity <= (int)es_warning &&
            is_implicit_return &&
            !curr_reachability.reachable_considering_hints) {
          /* Suppress a non-error diagnostic if this is an implicit return and
             the user told us this code is not reachable. */
        } else {
          /* Get pointer to the symbol for the function name. */
          a_symbol_ptr     rout_sym = symbol_for(rout);
          a_symbol_locator locator;
          check_assertion_str(
                        rout_sym != NULL,
                        "check_void_return_okay: unexpected NULL assoc_info");
          if (rout_sym->is_error) {
            make_locator_for_symbol(rout_sym, &locator);
          }  /* if */
          if (!(rout_sym->is_error && looks_like_ctor_or_dtor(&locator))) {
            an_error_code  err_code =
               is_implicit_return ? ec_implicit_return_from_non_void_function :
                                    ec_no_value_returned_in_non_void_function;
            sym_diagnostic(no_returned_value_severity, err_code, rout_sym);
            if (rout->is_constexpr &&
                !special_kind_is(current_routine_entry(), sfk_constructor) &&
                is_effective_error(err_code, no_returned_value_severity,
                                   &error_position)) {
              /* Can't be a constexpr function. */
              ssep->constexpr_ruled_out = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_void_return_okay */


static void return_statement(void)
/*
Scan a "return" statement and add it to the current statement sequence.
The syntax is:

     jump-statement:
          return expression    ;
                           opt
          return brace-init-list ;  // C++11 only.

In C++20, this also handles co_return statements.
*/
{
  a_statement_ptr    sp;
#if VLA_DEALLOCATIONS_IN_IL
  a_statement_ptr    vla_dealloc_stmts = NULL;
#endif /* VLA_DEALLOCATIONS_IN_IL */
  an_expr_node_ptr   return_expr = NULL;
  a_dynamic_init_ptr dip = NULL;
  a_routine_ptr      rout;
  a_type_ptr         rout_type, return_type;
  a_boolean          microsoft_C_mode_void_return = FALSE, expr_present,
                       return_stmt_allowed = TRUE;
  a_source_position  return_pos;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
                     src_seq_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  an_arg_list_elem_ptr
                     alep = NULL;

  db_enter(3, "return_statement");
  check_for_unreachable_code();
  /* Save the position of the beginning of the return statement. */
  return_pos = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if VLA_DEALLOCATIONS_IN_IL
  if (vla_enabled && vla_deallocations_in_il &&
      curr_reachability.reachable) {
    /* Put out a vla-dealloc statement for each declaration of a VLA variable
       in the currently active blocks of the function. */
    vla_dealloc_stmts = collect_vla_dealloc_stmts_for_function(
                                              end_of_control_flow_descr_list);
  }  /* if */
#endif /* VLA_DEALLOCATIONS_IN_IL */
  /* Get a pointer to the current routine entry, and its return type. */
  if (innermost_function_scope == NULL) {
    add_stop_token(tok_semicolon);
    syntax_error(ec_bad_return);
    goto done;
  }  /* if */
  rout = current_routine_entry();
  rout_type = skip_typerefs(rout->type);
  return_type = rout_type->variant.routine.return_type;
  if (rout->is_coroutine) {
    if (curr_token == tok_return) {
      pos_error(ec_return_in_coroutine, &pos_curr_token);
    }  /* if */
  } else if (curr_token == tok_coroutine_return) {
    if (scope_stack[depth_innermost_function_scope].has_at_least_one_return){
      pos_error(ec_invalid_co_return, &pos_curr_token);
    } else {
      /* Ensure this function is marked as a coroutine. */
      (void)get_coroutine_descr(rout);
    }  /* if */
  }  /* if */
  /* Skip the return or co_return token. */
  (void)get_token();
  add_stop_token(tok_semicolon);
  /* See if there is an expression after "return". */
  expr_present = (curr_token != tok_semicolon);
  if (special_kind_is(rout, sfk_constructor) &&
      depth_stmt_stack > 0 && 
      struct_stmt_stack[0].kind == ssk_try_block &&
      struct_stmt_stack[1].is_catch_clause) {
    /* This is a return statement inside a handler of a function try block
       of a constructor. */
    pos_error(ec_return_from_ctor_function_try_block_handler, &return_pos);
    discard_curr_construct_pragmas();
    return_type = error_type();
    return_stmt_allowed = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cli_or_cx_enabled && inside_finally_clause()) {
    /* This is a return statement inside of a finally block. */
    pos_error(ec_return_from_finally, &return_pos);
    discard_curr_construct_pragmas();
    return_type = error_type();
    return_stmt_allowed = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    /* See if the optional expression is present. */
    if (!expr_present) {
      /* The expression is missing. */
      if (rout->is_coroutine) {
        /* A co-routine statement.  It may eventually be transformed into a
           "return_void()" call.  Set alep to NULL to indicate that no
           arguments should be passed in such a call. */
        alep = NULL;
      } else {
        check_void_return_okay(/*is_implicit_return=*/FALSE, &return_expr);
      }  /* if */
    } else {
      /* The expression is present. */
      a_boolean  return_type_checked = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cppcx_enabled && special_kind_is(rout, sfk_constructor)) {
         /* While processing vccorlib.h, constructors are allowed to return
            a pointer to their class type or a handle to their class type for
            ref classes, though this not required.  scan_return_expression
            performs an additional check for this case. */
        a_type_ptr  parent_type = parent_class_of(rout);
        if (class_symbol_supp(symbol_for(parent_type))->from_vccorlib) {
          return_type_checked = TRUE;
        }  /* if */
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (return_type_checked) {
        /* We already handled this case above. */
      } else if (special_kind_is(rout, sfk_constructor) ||
#if MICROSOFT_EXTENSIONS_ALLOWED
                 special_kind_is(rout, sfk_static_constructor) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                 special_kind_is(rout, sfk_destructor)) {
        /* Constructors and destructors may not return a value (ARM 6.6.3). */
        pos_error(ec_value_returned_in_constructor, &error_position);
        return_type = error_type();
      } else if (is_void_type(return_type) ||
                 is_template_param_type(return_type)) {
        /* A void function may return a void expression in C++, but not in C.
           Microsoft C allows an expression of any type.  cfront 2.1 allows
           a void expression.  cfront 3.0 does not allow any expression. */
        if (C_mode()) {
          if (ms_extensions || gcc_mode) {
            /* In Microsoft and GNU C modes a return statement in a void
               function may have the form "return expr;".  For this case the
               return statement is allocated later so that the expression can
               be put out first as a freestanding expression statement.  This
               feature is standard in C++, and the rewrite in that case is
               handled by IL lowering. */
            check_assertion(is_void_type(return_type));
            if (!gcc_mode) {
              /* In GNU C mode a warning is issued only if the return
                 expression doesn't have void type (done in
                 scan_return_expression). */
              pos_warning(ec_value_returned_in_void_function, &error_position);
            }  /* if */
            microsoft_C_mode_void_return = TRUE;
          } else {
            /* Other C modes.  An expression is not allowed. */
            pos_error(ec_value_returned_in_void_function, &error_position);
            return_type = error_type();
          }  /* if */
        } else {
          /* C++ modes. */
          if ((cfront_3_0_mode ||
               (microsoft_mode && microsoft_version <= 1200)) &&
              is_void_type(return_type)) {
            /* cfront 3.0 does not allow an expression if the function has
               a void return type. */
            /* Neither does Microsoft C++ mode for MSVC++ 6.0 and earlier. */
            pos_error(ec_value_returned_in_void_function, &error_position);
            return_type = error_type();
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (return_stmt_allowed) {
    src_seq_entry = add_empty_source_sequence_entry();
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (expr_present) {
    /* Scan the return expression and convert it to the function type. */
    return_expr = scan_return_expression(return_type,
                                         ec_bad_return_value_type,
                                         &dip, &alep);
  }  /* if */
  /* The return type might have been deduced.  Reload it. */
  return_type = rout_type->variant.routine.return_type;
#if VLA_DEALLOCATIONS_IN_IL
  if (vla_dealloc_stmts != NULL) {
    /* Insert the deallocation statements, but the return expression must be
       evaluated first if it is not invariant. */
    check_assertion(C_mode() && dip == NULL);
    if (return_expr != NULL &&
        !is_invariant_expr(return_expr, /*vars_can_change=*/TRUE,
                           /*treat_as_potential_prvalue=*/FALSE)) {
      /* Evaluate the return expression in a temporary and return that
         temporary. */
      a_statement_ptr  eval = add_statement(stmk_expr,
                                            /*compiler_generated=*/FALSE);
      if (is_void_type(return_type)) {
        /* No temporary is needed: just evaluate the expression. */
        eval->expr = return_expr;
        return_expr = NULL;
        /* If this a C-mode "void return" case, we no longer need to create an
           extra statement expression to evaluate the void expression. */
        microsoft_C_mode_void_return = FALSE;
      } else {
        a_variable_ptr    tmp_var =
                              alloc_temporary_variable(return_type,
                                                       /*force_static=*/FALSE);
        an_expr_node_ptr  lhs = var_lvalue_expr(tmp_var);
        eval->expr = make_assignment_expr(
                          lhs, which_binary_operator(tok_assign, return_type),
                          return_expr);
        eval->expr->result_is_not_used = TRUE;
        return_expr = var_rvalue_expr(tmp_var);
      }  /* if */
    }  /* if */
    add_statement_list(vla_dealloc_stmts, curr_reachability.reachable);
  }  /* if */
#endif /* VLA_DEALLOCATIONS_IN_IL */
  if (!return_stmt_allowed) {
    sp = NULL;
  } else if (microsoft_C_mode_void_return) {
    /* Microsoft or GNU C mode.  A warning was already issued above. Put out
       the expression statement holding the return expression.  The return
       statement will come a little later. */
    sp = add_statement(stmk_expr, /*compiler_generated=*/FALSE);
  } else {
    /* Allocate the return statement. */
    a_statement_kind  kind = (a_statement_kind)stmk_return;
    if (rout->is_coroutine) {
      kind = (a_statement_kind)stmk_coroutine_return;
    }  /* if */
    sp = add_statement_at_stmt_pos(kind, &return_pos,
                                   /*compiler_generated=*/FALSE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    update_source_sequence_list((char*)sp, iek_statement, src_seq_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  if (sp != NULL) {
    /* Do processing required for any pragmas that are bound to the current
       statement. */
    process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
    /* Put the expression into the statement. */
    sp->expr = return_expr;
    if (!microsoft_C_mode_void_return) {
      sp->variant.return_dynamic_init = dip;
    } else {
      /* The Microsoft/GNU C compatibility case: "return expr" in a void
         function.  The statement already put out is an expression statement.
         Follow it now by a return statement with a null expression. */
      /* coverity[returned_pointer] -- sp unused in some configurations. */
      sp = add_statement_at_stmt_pos(stmk_return, &return_pos,
                                     /*compiler_generated=*/FALSE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      update_source_sequence_list((char*)sp, iek_statement, src_seq_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
    if (!relaxed_constexpr_allowed() &&
        current_routine_entry()->is_constexpr &&
        !special_kind_is(current_routine_entry(), sfk_constructor)) {
      /* A C++11 constexpr function must have exactly one return.  (That
         restriction is lifted in C++14.) */
      if (scope_stack[depth_innermost_function_scope].has_at_least_one_return){
        /* There has already been at least one return in this constexpr
           function; give an error and disqualify the routine from being
           constexpr. */
        scope_stack[depth_innermost_function_scope].constexpr_ruled_out = TRUE;
        pos_error(ec_invalid_constexpr_body, &return_pos);
      } else {
        if (return_expr == NULL && dip == NULL) {
          /* A void return in a C++11-style constexpr function is not valid.
             For a template instance this makes the function non-constexpr;
             in other cases, an error should have been issued already. */
          scope_stack[depth_innermost_function_scope].constexpr_ruled_out =
                                                                         TRUE;
        }  /* if */
      }  /* if */
    } else if (rout->is_coroutine) {
      sp->expr = make_coroutine_result_expression(alep, /*is_yield=*/FALSE,
                                                  sp);
    }  /* if */
    if (sp->kind == (a_statement_kind)stmk_return) {
      scope_stack[depth_innermost_function_scope].has_at_least_one_return =
                                                                          TRUE;
    }  /* if */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (curr_token == tok_semicolon) {
    curr_construct_end_position = end_pos_curr_token;
  }  /* if */
  if (sp != NULL) {
    sp->end_position = curr_construct_end_position;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
done:
  remove_stop_token(tok_semicolon);
  db_exit();
}  /* return_statement */


a_boolean in_catch_clause(void)
/*
Return TRUE if we are currently inside a catch clause.
*/
{
  a_boolean  result = FALSE;
  int        depth = depth_stmt_stack;

  for (; depth > 0; --depth) {
    if (struct_stmt_stack[depth].is_catch_clause) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* in_catch_clause */

#if GNU_EXTENSIONS_ALLOWED

static a_boolean conflicting_switch_case_ranges(a_switch_case_entry_ptr  scep1,
                                                a_switch_case_entry_ptr  scep2)
/*
Return whether the two given switch case ranges overlap.  The ranges may be
single elements (e.g., "case 3:" instead of "case 3 ... 4:"), but at least one
range boundary must be an integer constant (i.e., not an error constant and not
a template-dependent constant).
E.g., if I and J are template-dependent constants and x and y are integer
constants, [x, I] and [J, y] will conflict if x == y.
*/
{
  /* Compare two ranges.  [a, b] and [c, d] don't conflict only if b < c
     or a > d.  In common cases, the ranges degenerate to single elements. */
  a_constant_ptr  a = scep1->case_value, b = scep1->range_end,
                  c = scep2->case_value, d = scep2->range_end;
  if (b == NULL || b->kind != (a_constant_repr_kind)ck_integer) {
    b = a;
  } else if (a->kind != (a_constant_repr_kind)ck_integer) {
    a = b;
  }  /* if */
  check_assertion(a->kind == (a_constant_repr_kind)ck_integer);
  if (d == NULL || d->kind != (a_constant_repr_kind)ck_integer) {
    d = c;
  } else if (c->kind != (a_constant_repr_kind)ck_integer) {
    c = d;
  }  /* if */
  check_assertion(c->kind == (a_constant_repr_kind)ck_integer);
  return !(cmp_integer_constants(b, c) < 0 || cmp_integer_constants(a, d) > 0);
}  /* conflicting_switch_case_ranges */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void record_switch_case_entry(a_switch_case_entry_ptr        scep,
                                     a_struct_stmt_stack_entry_ptr  sssep)
/*
Record the given switch case entry in the structures pointed to by the
associated switch statement (described by sssep).  At the very least, this
entails adding the entry to the list of all cases associated with that switch.
It may also involve recording the entry on a sorted list of cases, or recording
the "default case" in its own IL slot.
Finally, this routine also updates the control flow data structures as needed.
*/
{
  a_switch_stmt_descr_ptr  ssdp =
                              sssep->statement->variant.switch_stmt.extra_info;
  /* Append the entry to the clause's source order list. */
  if (ssdp->cases == NULL) {
    /* First (perhaps only) case in this switch. */
    ssdp->cases = scep;
  } else {
    /* Append at the end of the list. */
    sssep->last_switch_case_entry->next = scep;
  }  /* if */
  sssep->last_switch_case_entry = scep;
  /* For the default case, record some additional information in *scep. */
  if (scep->case_value == NULL) {
    /* The "default" case. */
    if (ssdp->default_case != NULL) {
      pos_error(ec_default_label_appears_more_than_once, &scep->position);
    } else {
      ssdp->default_case = scep;
    }  /* if */
  } else {
    /* If a switch case range has a nondependent start or end value, we can
       compare it to other values to report a duplicate.  E.g., "case 3:" and
       "case I ... 3:" conflict, whereas "case 3:" and "case I ... 4:" may not
       conflict (assuming I is a template parameter).  Note that the sorted
       list will be discarded if it contains any dependent cases.  (Error cases
       are treated like template-dependent cases here.) */
    a_constant_ptr  value = scep->case_value, max_value = value;
    a_boolean       dependent_start =
                               value->kind != (a_constant_repr_kind)ck_integer;
    a_boolean       fully_dependent = dependent_start;
    a_boolean       dependent_end = FALSE;
#if GNU_EXTENSIONS_ALLOWED
    if (scep->range_end != NULL) {
      if (scep->range_end->kind != (a_constant_repr_kind)ck_integer) {
        dependent_end = TRUE;
      } else {
        fully_dependent = FALSE;
        max_value = scep->range_end;
        /* If the range start is dependent, but the range end is not, use the
           latter for comparisons. */
        if (dependent_start) value = scep->range_end;
      }  /* if */
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    sssep->switch_has_dependent_case = dependent_start || dependent_end;
    if (!fully_dependent) {
      /* The remainder of this function maintains the sorted_cases list. */
      if (ssdp->sorted_cases == NULL) {
        /* The first element on the sorted list. */
        ssdp->sorted_cases = scep;
        sssep->switch_max_case_value = max_value;
        sssep->last_switch_case_on_sorted_list = scep;
      } else if (cmp_integer_constants(value,
                                       sssep->switch_max_case_value) > 0) {
        /* The largest entry seen so far in this switch: append it at the end
           of the sorted_cases list. */
        sssep->last_switch_case_on_sorted_list->next_on_sorted_list = scep;
        sssep->last_switch_case_on_sorted_list = scep;
        sssep->switch_max_case_value = max_value;
      } else {
        /* Insert the entry at the right location by searching the sorted
           list.  Also check for (and diagnose) conflicts. */
        a_switch_case_entry_ptr  *ptr = &ssdp->sorted_cases;
        /* Skip over smaller value cases (if any). */
        while (*ptr != NULL) {
          a_constant_ptr  prev_value = (*ptr)->case_value;
          int             cmp_result;
#if GNU_EXTENSIONS_ALLOWED
          if ((*ptr)->range_end != NULL && (*ptr)->case_value->kind !=
                                           (a_constant_repr_kind)ck_integer) {
            /* The first element of the range cannot be compared.  Assume the
               "best case" scenario that the range is actually a singleton.
               E.g., for a range I .. 10 with I a template parameter, assume I
               will be 10, which is the least likely to produce a conflict. */
            prev_value = (*ptr)->range_end;
          }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
          check_assertion(prev_value->kind ==
                                            (a_constant_repr_kind)ck_integer);
          cmp_result = cmp_integer_constants(value, prev_value);
          if (cmp_result == 0
#if GNU_EXTENSIONS_ALLOWED
              || (gnu_mode && conflicting_switch_case_ranges(scep, *ptr))
#endif /* GNU_EXTENSIONS_ALLOWED */
                                                                      ) {
            pos2_diagnostic(es_error, ec_case_label_conflict, &scep->position,
                            &(*ptr)->position);
          }  /* if */
          if (cmp_result < 0) {
            /* The current position in the list is a larger case than the new
               switch case, and since the ones further down the list are larger
               still, we can end the search here. */
            break;
          }  /* if */
          ptr = &(*ptr)->next_on_sorted_list;
        }  /* while */
        scep->next_on_sorted_list = *ptr;
        *ptr = scep;
        if (scep->next_on_sorted_list == NULL) {
          /* The newly added entry is the last one on the sorted list. */
          sssep->last_switch_case_on_sorted_list = scep;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* Record the potential branch target (this allows us to e.g. warn about
     bypassed initializations). */
  add_to_control_flow_descr_list(
        alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_case_label));
  if (long_lifetime_temps && !C_mode()) {
    /* When long_lifetime_temps are enabled, treat the switch case statement
       as a label and create a block-after-label object lifetime.  Temporaries
       will be destroyed as necessary when the change of object lifetimes
       is detected. */
    a_struct_stmt_stack_entry_ptr top_sssep =
                                          &struct_stmt_stack[depth_stmt_stack];
    push_object_lifetime(iek_statement, (char *)scep->stmt,
                         (an_object_lifetime_kind)olk_block_after_label);
    if (top_sssep->kind == (a_struct_stmt_kind)ssk_compound) {
      /* If the current structured statement is a compound statement,
         update its curr_block_object_lifetime; */
      top_sssep->curr_block_object_lifetime = curr_object_lifetime;
    }  /* if */
    if (sssep == top_sssep ||
        sssep->statement->variant.switch_stmt.body_statement ==
         top_sssep->statement) {
      /* Update the object lifetime in the ssk_switch entry. */
      sssep->curr_block_object_lifetime = curr_object_lifetime;
    }  /* if */
  }  /* if */
}  /* record_switch_case_entry */


static void case_label(void)
/*
Scan a case label definition.  The syntax is:

3.6.1  labeled_statement:
		case constant-expression : statement

GNU also allows the "case range" form:
	case constant-lower-bound ... constant-upper-bound : statement
*/
{
  a_struct_stmt_stack_entry_ptr sssep;
  a_type_ptr                    switch_type;
  a_constant_ptr                constant_ptr;
  a_constant_ptr                range_end = NULL;
  a_source_position             case_position, constant_position;
  a_boolean                     save_reachability =
                                                   curr_reachability.reachable;
  int                           switch_depth;

  db_enter(4, "case_label");

  struct_stmt_stack[depth_stmt_stack].contains_active_switch_case = TRUE;
  add_stop_token(tok_colon);
  /* See if we are within a switch body by looking at the entries in
     the structured statement stack. */
  sssep = find_enclosing_struct_stmt(/*find_switch=*/TRUE,
                                     /*find_loop=*/FALSE);
  if (sssep != NULL) {
    /* Assume the case is reachable if the switch is reachable. */
    merge_reachability(&sssep->start_reachable, &curr_reachability);
    switch_type = sssep->type;
    switch_depth = (int)(sssep - struct_stmt_stack);
  } else {
    /* We are not inside a switch statement. */
    pos_error(ec_case_label_must_be_in_switch, &error_position);
    set_reachable(curr_reachability);
    switch_type = error_type();
    switch_depth = 0;
  }  /* if */
  /* Ignore the initial "case". */
  check_assertion_str(curr_token == tok_case, "case_label: expected case");
  case_position = pos_curr_token;
  (void)get_token();
  constant_position = pos_curr_token;
  constant_ptr = scan_case_label_constant(switch_type);
  if (gnu_mode && curr_token == tok_ellipsis) {
    /* This is a GNU C case range. E.g.: case 'a' ... 'z': */
    /* Skip the ellipsis. */
    (void)get_token();
    range_end = scan_case_label_constant(switch_type);
    /* Check that *range_end > *constant_ptr. */
    if (range_end != NULL &&
        constant_ptr != NULL &&
        constant_ptr->kind == (a_constant_repr_kind)ck_integer &&
        range_end->kind == (a_constant_repr_kind)ck_integer &&
        cmp_integer_constants(constant_ptr, range_end) > 0) {
      pos_error(ec_invalid_case_range, &error_position);
      range_end = NULL;
    }  /* if */
  }  /* if */
  if (switch_depth != 0 && constant_ptr != NULL) {
    a_statement_ptr          sp;
    a_switch_case_entry_ptr  scep = alloc_switch_case_entry();
    /* Reload sssep because the statement stack may have been reallocated. */
    sssep = &struct_stmt_stack[switch_depth];
    sp = add_statement_at_stmt_pos(stmk_switch_case, &case_position,
                                   /*compiler_generated=*/FALSE);
    stmt_update_source_sequence_list(sp);
    sp->variant.switch_case.switch_statement = sssep->statement;
    sp->variant.switch_case.extra_info = scep;
    scep->stmt = sp;
    scep->case_value = constant_ptr;
#if GNU_EXTENSIONS_ALLOWED
    scep->range_end = range_end;
#endif /* GNU_EXTENSIONS_ALLOWED */
    scep->position = constant_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    scep->end_position = curr_construct_end_position;
    scep->colon_position = pos_curr_token;
    sp->end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    scep->reachable_by_fall_through = save_reachability;
    record_switch_case_entry(scep, sssep);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and ignore the final colon. */
  (void)required_token(tok_colon, ec_exp_colon);
  remove_stop_token(tok_colon);
  db_exit();
}  /* case_label */


static void default_label(void)
/*
Scan a default case label definition.  The syntax is:

3.6.1  labeled_statement
		default : statement

*/
{
  a_struct_stmt_stack_entry_ptr sssep;
  a_source_position             label_position;

  db_enter(4, "default_label");

  label_position = pos_curr_token;
  /* Ignore the initial "default". */
#if CHECKING
  if (curr_token != tok_default) {
    internal_error("default_label: expected default");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* See if we are within a switch body by looking at the entries in
     the structured statement stack. */
  sssep = find_enclosing_struct_stmt(/*find_switch=*/TRUE,
                                     /*find_loop=*/FALSE);
  if (sssep != NULL) {
    a_statement_ptr          sp;
    a_switch_case_entry_ptr  scep = alloc_switch_case_entry();
    sp = add_statement_at_stmt_pos(stmk_switch_case, &label_position,
                                   /*compiler_generated=*/FALSE);
    sp->variant.switch_case.switch_statement = sssep->statement;
    sp->variant.switch_case.extra_info = scep;
    scep->stmt = sp;
    scep->position = label_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    scep->colon_position = pos_curr_token;
    sp->end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    scep->reachable_by_fall_through = curr_reachability.reachable;
    record_switch_case_entry(scep, sssep);
    merge_reachability(&sssep->start_reachable, &curr_reachability);
  }  else {
    /* We are not inside a switch statement. */
    pos_error(ec_default_label_must_be_in_switch, &label_position);
    set_reachable(curr_reachability);
  }  /* if */
  /* Check for and ignore the final colon. */
  (void)required_token(tok_colon, ec_exp_colon);
  db_exit();
}  /* default_label */


static void label_definition(void)
/*
Scan and process a label definition.  (The current token is a label name and
it is followed by a colon.)
*/
{
  a_label_ptr                    label;
  a_struct_stmt_stack_entry_ptr  sssep = &struct_stmt_stack[depth_stmt_stack];
  an_attribute_ptr               attributes = sssep->prefix_attributes;
  a_source_position              label_pos = pos_curr_token;

  sssep->prefix_attributes = NULL;
  sssep->contains_user_label = TRUE;
  /* Scan the label identifier, and enter it into the symbol table if
     needed. */
  label = scan_label(/*is_definition=*/TRUE, /*is_declaration=*/FALSE);
  /* See if the label has already been defined. */
  if (label->exec_stmt != NULL) {
    issue_redef_diag(&label_pos, symbol_for(label));
    set_reachable(curr_reachability);
  } else {
    /* The label has not previously been declared, so put out the
       definition. */
    define_label(label);
    stmt_update_source_sequence_list(label->exec_stmt);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = end_pos_curr_token;
    label->exec_stmt->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (!C_mode()) {
      /* Record the innermost object lifetime that this label is part of.  If
         that lifetime turns out to be "useless," the label lifetime will be
         updated later.  See fixup_curr_block_labels_and_gotos. */
      label->exec_stmt->variant.label.lifetime = curr_object_lifetime;
    }  /* if */
    /* If there have been forward gotos referencing this label, check whether
       any have jumped over initializing declarations. */
    check_for_jump_over_initialization(label->exec_stmt,
                                       &label->source_corresp.decl_position);
    check_assertion(depth_innermost_function_scope > 0);
    scope_stack[depth_innermost_function_scope].last_label_decl_seq =
                                                  symbol_for(label)->decl_seq;
    if (!C_mode()) {
      /* Flag all enclosing blocks to "invalidate" their currently active
         object lifetimes. */
      for (; sssep >= struct_stmt_stack; --sssep) {
        if (sssep->kind == (a_struct_stmt_kind)ssk_compound) {
          sssep->label_invalidates_curr_block_object_lifetime = TRUE;
          if (sssep->is_catch_clause) {
            /* Don't propagate the invalidation flag out of a catch clause. */
            break;
          }  /* if */
        } else if (sssep->kind == (a_struct_stmt_kind)ssk_try_block) {
          /* Don't propagate the invalidation flag out of a try block. */
          break;
        }  /* if */
      }  /* for */
      /* Create an object lifetime to run from this point to the end of
         the current scope.  It's needed to handle backwards gotos to
         the current label. */
      reset_curr_block_object_lifetime(label->exec_stmt);
      if (!cpp23_mode && relaxed_constexpr_allowed()) {
        /* Prior to C++23, labels were not allowed in C++14 constexpr functions
           (they disqualified a lambda from being considered constexpr). */
        a_routine_ptr  rp = innermost_function_scope->variant.routine.ptr;
        if (rp->is_declared_constexpr || rp->is_consteval) {
          pos_error(ec_label_in_constexpr_function,
                    &label->source_corresp.decl_position);
          /* Avoid additional diagnostics by marking the label as
             referenced.  */
          symbol_for(label)->referenced = TRUE;
        } else {
          scope_stack[depth_innermost_function_scope].constexpr_ruled_out =
                                                                          TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  check_assertion_str(curr_token == tok_colon, "statement: expected colon");
  (void)get_token();
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_attributes_enabled && curr_token == tok_attribute) {
    *f_last_attribute_link(&attributes) = scan_gnu_attribute_groups(al_label);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (attributes != NULL) {
    attach_attributes(attributes, (char*)label, iek_label);
  }  /* if */
}  /* label_definition */

#if UPC_EXTENSIONS_ALLOWED

static void upc_barrier_style_statement(void)
/*
Parse the barrier-style UPC statements upc_notify, upc_wait, and upc_barrier.
Each has the form
    <keyword> <integer-expr> ;
*/
{
  a_statement_ptr               sp;
  a_statement_kind              kind = (a_statement_kind)stmk_last;

  switch (curr_token) {
    case tok_upc_notify:
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug, "UPC notify statement\n");
      }  /* if */
#endif /* DEBUG */
      kind = (a_statement_kind)stmk_upc_notify;
      break;
    case tok_upc_wait:
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug, "UPC wait statement\n");
      }  /* if */
#endif /* DEBUG */
      kind = (a_statement_kind)stmk_upc_wait;
      break;
    case tok_upc_barrier:
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug, "UPC barrier statement\n");
      }  /* if */
#endif /* DEBUG */
      kind = (a_statement_kind)stmk_upc_barrier;
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement(kind, /*compiler_generated=*/FALSE);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Skip the initial token */
  (void)get_token();
  add_stop_token(tok_semicolon);
  sp->expr = NULL;
  if (curr_token != tok_semicolon) {
    /* Scan the notification condition expression. */
    sp->expr = scan_integer_expression(/*is_switch_expr=*/FALSE,
                                       (an_init_component*)NULL);
  }  /* if */
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);
}  /* upc_barrier_style_statement */


static void upc_fence_statement(void)
/*
Parse a statement of the form
	upc_fence ;
*/
{
  a_statement_ptr sp;

  check_for_unreachable_code();
  /* Allocate the statement. */
  sp = add_statement(stmk_upc_fence, /*compiler_generated=*/FALSE);
  stmt_update_source_sequence_list(sp);
  /* Do processing required for any pragmas that are bound to the current
     statement. */
  process_curr_construct_pragmas((a_symbol_ptr)NULL, sp);
  /* Skip the "upc_fence" token. */
  check_assertion(curr_token == tok_upc_fence);
  (void)get_token();
  /* Check for and ignore the final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
}  /* upc_fence_statement */

#endif /* UPC_EXTENSIONS_ALLOWED */

static void statement(a_boolean is_dependent_statement,
                      a_boolean marked_as_gnu_extension)
/*
Scan a statement.  Add it to the current statement sequence.
is_dependent_statement is TRUE if the statement is a dependent
statement (of an "if", etc.).  If marked_as_gnu_extension is TRUE,
this statement was preceded by the GNU keyword __extension__.
*/
{
  a_boolean          prev_was_label = FALSE;
  a_boolean          get_another_statement;
  a_boolean          can_appear_in_constexpr_body = relaxed_constexpr_enabled;
  an_error_severity  in_constexpr_body_sev = es_error;
  a_source_position  start_pos;
  an_il_entity_list_entry_ptr
                     entity_list;
  a_routine_ptr      current_rp = innermost_function_scope != NULL ?
                                                current_routine_entry() : NULL;

  db_enter(3, "statement");

rescan_statement:
  if (constexpr_enabled && struct_stmt_stack_top().inside_statement_expr) {
    /* Although only a return-statement is allowed in the body of a C++11
       constexpr function, GCC and Clang accept other statements within a
       statement expression that appears in that return-statement. */
    can_appear_in_constexpr_body = TRUE;
  }  /* if */
  if (struct_stmt_stack_top().p_start_pos == NULL) {
    /* Record the start of the statement for add_statement: This takes into
       account leading attributes or GNU __extension__ keywords (which are
       consumed before calling add_statement). */
    start_pos = pos_curr_token;
    struct_stmt_stack_top().p_start_pos = &start_pos;
  }  /* if */
  if (std_attribute_tokens_next() || curr_token == tok_alignas ||
      curr_token == tok_attribute ||
      (curr_token == tok_declspec && ms_declspec_attributes_enabled)) {
    /* Scan leading attributes. */
    struct_stmt_stack_top().prefix_attributes = scan_attributes(al_prefix);
  }  /* if */
  get_another_statement = FALSE;
  /* Move cached #pragma declarations (if any) to the current scope stack
     entry so they can be examined and acted upon in subsequent processing.
     If we have already scanned a label, any pragmas between the label and
     the statement may be added to the existing list.  Otherwise, the
     list is expected to have been cleared. */
  if (select_curr_construct_pragmas(/*add_to_list=*/prev_was_label)) {
    /* If a lint-style "notreached" comment was detected, suppress the
       warning on unreachable code. */
    check_lint_notreached_state();
  }  /* if */
  struct_stmt_stack_top().fallthrough_statement = NULL;
  switch(curr_token) {
    case tok_semicolon:
      /* Empty statement (part of expression-statement, 3.6.3). */
      empty_statement(/*compiler_generated=*/FALSE);
      can_appear_in_constexpr_body = TRUE;
      break;
    case tok_lbrace:
      /* Compound statement (3.6.2). */
      (void)compound_statement(/*at_function_level=*/FALSE,
                               /*explicit_return_type=*/FALSE,
                               /*is_catch_clause=*/FALSE,
                               /*is_statement_expr=*/FALSE);
      if (!strict_ansi_mode) can_appear_in_constexpr_body = TRUE;
      break;
    case tok_if:
      /* If statement. */
      if_statement();
      break;
    case tok_switch:
      /* Switch statement. */
      switch_statement();
      break;
    case tok_while:
      /* While statement. */
      while_statement();
      break;
    case tok_do:
      /* do .. while statement. */
      do_statement();
      break;
#if UPC_EXTENSIONS_ALLOWED
    case tok_upc_forall:
    /* The upc_forall statement is similar to the standard for statement. */
      check_assertion(!constexpr_enabled);
      FALLTHROUGH
#endif /* UPC_EXTENSIONS_ALLOWED */
    case tok_for:
      /* For statement and range-based-for ([stmt.ranged]). */
      for_statement();
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_for_each:
      /* "for each" statement (ECMA-372 section 16.2.1). */
      for_each_statement();
      can_appear_in_constexpr_body = FALSE;
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_goto:
      /* Goto statement. */
      goto_statement();
      if (!cpp23_mode) can_appear_in_constexpr_body = FALSE;
      break;
    case tok_continue:
      /* Continue statement. */
      continue_statement();
      break;
    case tok_break:
      /* Break statement. */
      break_statement();
      break;
    case tok_return:
    case tok_coroutine_return:
      /* Return or co-return statement. */
      return_statement();
      if (current_rp != NULL && current_rp->is_constexpr &&
          !special_kind_is(current_rp, sfk_constructor)) {
        /* No return statements are allowed in constexpr constructors. */
        can_appear_in_constexpr_body = TRUE;
      }  /* if */
      break;
    case tok_asm:
    case tok_microsoft_asm:
      /* Asm "declaration" or Microsoft mode asm block. */
      asm_statement();
      can_appear_in_constexpr_body = cpp20_mode;
      if (!can_appear_in_constexpr_body && cpp14_mode &&
          gpp_version_is(>= 100000)) {
        /* Newer versions of GCC only warn about asm declarations in constexpr
           functions when in C++14 or C++17 mode. */
        in_constexpr_body_sev = es_warning;
      }  /* if */
      break;
    case tok_try:
      /* C++ try block. */
      try_block_statement((a_statement_ptr)NULL,
                          /*explicit_return_type=*/FALSE);
      can_appear_in_constexpr_body = constexpr_try_enabled;
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_microsoft_try:
      /* Microsoft try-finally or try-except statement. */
      microsoft_try_statement();
      can_appear_in_constexpr_body = FALSE;
      break;
    case tok_leave:
      /* Microsoft __leave. */
      leave_statement();
      can_appear_in_constexpr_body = FALSE;
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_case:
      /* Case label (3.6.1). */
      case_label();
      prev_was_label = TRUE;
      get_another_statement = TRUE;
      break;
    case tok_default:
#if MICROSOFT_EXTENSIONS_ALLOWED
default_label_case:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Default label. */
      default_label();
      prev_was_label = TRUE;
      get_another_statement = TRUE;
      break;
    case tok_identifier:
      /* Identifier.  Make sure that the identifier is a simple identifier
         (i.e., not a C++ qualified name, template-id or operator name).
         A simple identifier is probably the start of an expression-statement,
         but first we must check to see if it is a label definition
         by looking to see if the next token is a colon. */
      if (!locator_for_curr_id.is_qualified_name &&
          !locator_for_curr_id.is_template_id &&
          !locator_for_curr_id.is_conversion_name &&
          !locator_for_curr_id.is_operator_name &&
          !is_error_locator(locator_for_curr_id)) {
        a_token_kind  next_tok = next_token();
        if (next_tok == tok_colon) {
          /* This is a label definition.  In Microsoft mode, it might be the
             "default" label of a switch statement. */
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (microsoft_mode && microsoft_version >= 1400 &&
              check_context_sensitive_keyword(tok_default, "default")) {
            goto default_label_case;
          } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          /* Do not insert code here. */
          {
            label_definition();
            prev_was_label = TRUE;
            get_another_statement = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* if */
      /* Other cases are expression statements. */
      goto expr_statement;
    case tok_rbrace:
      /* Right brace where the start of a statement was expected. */
      if (prev_was_label) {
        if (cpp23_mode || c23_mode) {
          /* A change has been made in C23 and C++23 to allow a label
             as the last item in a compound statement. */
        } else {
          /* When a label definition precedes a "}", let it by as an
             extension, with a warning (at least) in all modes. */
          if (strict_ansi_mode) {
            diagnostic(strict_ansi_error_severity, ec_exp_statement);
          } else {
            pos_warning(ec_exp_statement, &error_position);
          }  /* if */
        }  /* if */
        /* Issue a diagnostic on trying to bind a pragma to the current
           statement. */
        cannot_bind_to_curr_construct();
        break;
      }  /* if */
      FALLTHROUGH
    case tok_else:
    case tok_catch:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_except:
    case tok_finally:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Tokens that figure in statements but do not begin them; issue an
         "expected a statement" error. */
      /* If you add tokens to this list, and they could possibly be in the
         stop tokens set (e.g., tok_else), add them also to the code in
         compound_statement that removes these things from the stop tokens
         set when entering a compound statement.  Otherwise, it would be
         possible to loop on an error. */
      /* Flush to the next statement. */
      add_stop_token(tok_semicolon);
      add_stop_token(tok_rbrace);
      syntax_error(ec_exp_statement);
      remove_stop_token(tok_semicolon);
      remove_stop_token(tok_rbrace);
      empty_statement(/*compiler_generated=*/TRUE);
      break;
#if UPC_EXTENSIONS_ALLOWED
    /* UPC-only constructs. */
    case tok_upc_notify:
    case tok_upc_wait:
    case tok_upc_barrier:
      check_assertion(!constexpr_enabled);
      upc_barrier_style_statement();
      break;
    case tok_upc_fence:
      check_assertion(!constexpr_enabled);
      upc_fence_statement();
      break;
#endif /* UPC_EXTENSIONS_ALLOWED */
    default:
expr_statement:
      /* An expression statement or a declaration.  Declarations can be
         interspersed among executable statements in C++ and C99,
         but only the C++ case is handled here; in C99, a declaration
         is not a statement but rather something that can appear
         intermixed with statements within a compound-statement.
         See compound_statement. */
      if (!marked_as_gnu_extension && curr_token == tok_extension) {
        marked_as_gnu_extension = TRUE;
        (void)get_token();
      }  /* if */
      if (!C_mode()) {
        start_potential_decl_statement(&entity_list);
      }  /* if */
      if (!can_appear_in_constexpr_body && relaxed_constexpr_allowed()) {
        /* This can be true in Clang-C++11-mode system headers. */
        in_constexpr_body_sev = es_warning;
      }  /* if */
      if (!C_mode() &&
#if MICROSOFT_EXTENSIONS_ALLOWED
          /* In C++/CLI, a construct like int:: begins an expression. */
          !(cli_or_cx_enabled && is_type_keyword(curr_token) &&
            next_token() == tok_colon_colon) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          (curr_token == tok_using || curr_token == tok_namespace ||
           is_decl_not_expr(DFS_REAL_DECLARATOR_ALLOWED))) {
        a_struct_stmt_stack_entry_ptr
                   sssep = &struct_stmt_stack[depth_stmt_stack];
        /* Scan a declaration (C++ only). */
        if (any_cfront_mode() && is_dependent_statement) {
          /* In cfront mode, a dependent statement is not allowed to be a
             declaration. */
          pos_error(ec_dependent_stmt_is_declaration, &error_position);
        }  /* if */
        if (sssep->prefix_attributes != NULL) {
          /* Make previously-scanned attributes available to declaration
             processing. */
          unscan_attributes(sssep->prefix_attributes);
          sssep->prefix_attributes = NULL;
        }  /* if */
        decl_statement(marked_as_gnu_extension, &can_appear_in_constexpr_body);
        if (!can_appear_in_constexpr_body && relaxed_constexpr_allowed()) {
          /* This can be true in Clang-C++11-mode system headers. */
          in_constexpr_body_sev = es_warning;
        }  /* if */
      } else if (C_mode() &&
                 (is_dependent_statement || prev_was_label) &&
                 is_decl_start(IDS_EXPR_CONTEXT |
                               IDS_REAL_DECLARATOR_ALLOWED)) {
        /* In C mode, do a special test to give a better error message
           when a declaration is used as a dependent statement, e.g.,
             if (i) int j;
           or for a labeled declaration, e.g.,
             lab: int j;
        */
        if (is_dependent_statement) {
          pos_error(ec_dependent_stmt_is_declaration, &error_position);
        } else if (c23_mode) {
          /* A labeled declaration is allowed starting with C23. */
        } else if (c99_mode) {
          /* A labeled declaration is not allowed in C99 mode (the syntax
             doesn't allow it), but we allow it in default mode. */
          diagnostic(strict_ansi_mode ? strict_ansi_discretionary_severity :
                                        es_warning,
                     ec_labeled_declaration);
        } else {
          /* A labeled declaration in pre-C99 C. */
          pos_error(ec_labeled_declaration, &error_position);
        }  /* if */
        a_struct_stmt_stack_entry_ptr
                   sssep = &struct_stmt_stack[depth_stmt_stack];
        if (sssep->prefix_attributes != NULL) {
          /* Make previously-scanned attributes available to declaration
             processing. */
          unscan_attributes(sssep->prefix_attributes);
          sssep->prefix_attributes = NULL;
        }  /* if */
        decl_statement(marked_as_gnu_extension,
                       /*p_okay_in_constexpr_body=*/NULL);
      } else {
        /* An expression-statement. */
        add_stop_token(tok_semicolon);
        check_for_unreachable_code();
        expression_statement(marked_as_gnu_extension);
        (void)required_token(tok_semicolon, ec_exp_semicolon);
        remove_stop_token(tok_semicolon);
      }  /* if */
      if (!C_mode()) {
        end_potential_decl_statement();
      }  /* if */
      break;
  }  /* switch */
  if (current_rp != NULL && current_rp->is_constexpr &&
      !can_appear_in_constexpr_body) {
    if (current_rp->is_declared_constexpr || current_rp->is_consteval) {
      /* Report an error if the statement is not one that is allowed in a
         constexpr function or constexpr constructor. */
      if (current_rp->is_prototype_instantiation &&
          in_constexpr_body_sev == es_error && !strict_ansi_mode) {
        /* In uninstantiated templates common practice is to either not
           diagnose this (GCC, MSVC) or to just issue a warning (Clang).  We
           therefore only issue a warning in nonstrict modes. */
        in_constexpr_body_sev = es_warning;
      } else if ((gpp_version_is(>=120000) || clangcpp_version_is(>=30300)) &&
                 current_rp->is_template_function) {
        in_constexpr_body_sev = es_warning;
      }  /* if */
      pos_diagnostic(in_constexpr_body_sev,
                     special_kind_is(current_rp, sfk_constructor) ?
                                ec_invalid_statement_in_constexpr_constructor :
                                ec_invalid_statement_in_constexpr_function,
                     &start_pos);
    }  /* if */
    scope_stack[depth_innermost_function_scope].constexpr_ruled_out = TRUE;
  }  /* if */
  /* Loop if we just got a label and not an actual statement. */
  if (get_another_statement) {
    marked_as_gnu_extension = FALSE;
    goto rescan_statement;
  }  /* if */

  db_exit();
}  /* statement */


static void local_label_declaration(void)
/*
Scan a GNU local label declaration of the form:
	__label__ l1, l2, ..., ln;
Local labels can appear only at the beginning of a block (that check has been
performed by the caller).
*/
{
  check_assertion(gnu_mode && curr_token == tok_identifier);
  (void)get_token();
  (void)ensure_il_scope_exists(&scope_stack[decl_scope_level]);
  add_stop_token(tok_semicolon);
  do {
    if (curr_token != tok_identifier) {
      syntax_error(ec_exp_identifier);
      break;
    } else {
      /* Scan the label declaration.  An error will be issued if this is a
         duplicate declaration. */
      (void)scan_label(/*is_definition=*/FALSE, /*is_declaration=*/TRUE);
    }  /* if */
  } while (loop_token(tok_comma));
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  remove_stop_token(tok_semicolon);
}  /* local_label_declaration */


a_statement_ptr compound_statement_full(a_boolean   at_function_level,
                                        a_boolean   explicit_return_type,
                                        a_boolean   is_catch_clause,
                                        a_boolean   is_statement_expr,
                                        a_boolean   marked_as_gnu_extension,
                                        a_type_ptr  *p_result_type)
/*
Scan a compound-statement.  The syntax is

3.6.2  compound-statement
		{ declaration-list    statement-list   }
                                  opt               opt
3.6.2  statement-list
		statement
		statement-list statement

Return a pointer to the statement created.

at_function_level is TRUE if this compound-statement is the body of a
function (rather than an enclosed block).  In that case, the closing "}"
is not swallowed by this routine.  This is unusual, but desirable in
that it gets any error messages (like those for unresolved labels) to 
come out on the closing "}".  If is_catch_clause is TRUE this is being called
to scan the body of an exception handler.  The scope stack has already been
pushed, but otherwise this is handled like an ordinary block (except that
branching into it is disallowed).  If is_statement_expr is TRUE, this
compound statement is the statement in a GNU statement expression (of the
form "({ ... })") and the result type of that compound statement is returned
through *p_result_type.  If marked_as_gnu_extension is TRUE, this statement
is being parsed within the context of the __extension__ keyword.
*/
{
  a_statement_ptr            block;
  a_boolean                  any_statements = FALSE;
  a_token_set_array_element  old_else_stop_token_value;
  a_boolean                  is_function_try_block = FALSE;
  an_object_lifetime_ptr     function_try_lifetime = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean                  saved_sses_disallowed =
                                           source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_source_position          lbrace_pos;

  db_enter (3, "compound_statement");

  /* We expect something on the statement stack unless we are starting a
     new function or this is a statement expression. */
  check_assertion(at_function_level ||
                  depth_stmt_stack >= 0 ||
                  is_statement_expr);
  /* Allocate the statement block. */
  if (at_function_level) {
    /* Block for a function. */
    set_reachable(curr_reachability);
    control_flow_descr_list = end_of_control_flow_descr_list = NULL;
    block = alloc_statement(stmk_block, /*compiler_generated=*/FALSE);
    block->variant.block.extra_info->end_of_block_reachable = FALSE;
    block->position = pos_curr_token;
    stmt_update_source_sequence_list(block);
    /* Clear statement stack just to be careful. */
    depth_stmt_stack = -1;
    /* Push an entry on the structured statement stack. */
    push_stmt_stack(ssk_compound, block, curr_object_lifetime);
  } else if (is_catch_clause) {
    block = alloc_statement(stmk_block, /*compiler_generated=*/FALSE);
    block->position = pos_curr_token;
    stmt_update_source_sequence_list(block);
    /* Issue diagnostics on pragmas that are trying to bind to the catch
       clause. */
    cannot_bind_to_curr_construct();
    /* Push an entry on the structured statement stack. */
    push_stmt_stack(ssk_compound, block,
                    innermost_block_object_lifetime(curr_object_lifetime));
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (depth_stmt_stack >= 0 &&
             struct_stmt_stack[depth_stmt_stack].kind ==
                                          (a_struct_stmt_kind)ssk_try_block &&
             struct_stmt_stack[depth_stmt_stack].parsing_finally_clause) {
    /* When scanning the C++/CLI "finally" block, start_block_statement is
       avoided because its call to add_statement would append the new block
       statement into the wrong part of the try supplement.  This case is
       similar to the is_catch_clause case above, except the scope stack was
       not previously pushed. */
    (void)push_scope((a_scope_kind)sck_block, NO_SCOPE_NUMBER,
                     (a_type_ptr)NULL, (a_routine_ptr)NULL);
    block = alloc_statement(stmk_block, /*compiler_generated=*/FALSE);
    block->position = pos_curr_token;
    stmt_update_source_sequence_list(block);
    /* Issue diagnostics on pragmas that are trying to bind to the finally
       clause. */
    cannot_bind_to_curr_construct();
    /* Push an entry on the structured statement stack. */
    push_stmt_stack(ssk_compound, block,
                    innermost_block_object_lifetime(curr_object_lifetime));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    /* Block nested within a function.  Link it onto the current statement
       sequence. */
    if (is_statement_expr) {
      if (depth_stmt_stack < 0) {
        /* A statement expression in a ctor-initializer is reachable. */
        set_reachable(curr_reachability);
      }  /* if */
    } else if (depth_stmt_stack == 0 && struct_stmt_stack[0].kind ==
                                          (a_struct_stmt_kind)ssk_try_block) {
      /* Since the top-level entry on the structured statement stack is a
         try block (rather than a block), this must be a function try block.
         In most ways this has to be treated just like an ordinary top-level
         block of a function. */
      a_routine_ptr  rp = current_routine_entry();
      is_function_try_block = TRUE;
      if (special_kind_is(rp, sfk_constructor) ||
          special_kind_is(rp, sfk_destructor)) {
        /* A function-try-block in a constructor or destructor.  An object
           lifetime was previously pushed to capture any destructions in
           the members or base classes.  Remove it from the object lifetime
           stack (but don't use pop_object_lifetime because that decides
           whether or not it's useless, and we want to wait until the
           end of the block to decide that). */
        function_try_lifetime = curr_object_lifetime;
        check_assertion(function_try_lifetime->kind ==
                                           (an_object_lifetime_kind)olk_block);
        curr_object_lifetime = function_try_lifetime->parent_lifetime;
        check_assertion(curr_object_lifetime != NULL &&
                        curr_object_lifetime->kind ==
                                       (an_object_lifetime_kind)olk_try_block);
      }  /* if */
    }  /* if */
    /* Note that there is no check for unreachable code.  It's probably too
       draconian to warn about an unreachable open brace if (say) there
       is a label right afterwards. */
    block = start_block_statement(/*generated_statement=*/FALSE,
                                  is_statement_expr,
                                  function_try_lifetime);
  }  /* if */
  /* Record in the statement stack entry whether the routine was declared
     with an explicit return type. */
  if (explicit_return_type) {
    check_assertion(depth_stmt_stack >= 0);
    struct_stmt_stack[depth_stmt_stack].rout_type_explicitly_specified = TRUE;
  }  /* if */
  if (at_function_level && C_mode() && vla_enabled) {
    /* Generate an stmk_set_vla_size for each vla_dimension appearing in
       function scope.  At this point, the list will include only
       declarations that appeared in the function prototype.  (All other
       cases are handled in array_declarator when the VLA is parsed.) */
    a_vla_dimension_ptr  vdp;
    check_assertion(decl_scope_level == depth_innermost_function_scope);
    vdp = scope_stack[decl_scope_level].il_scope->vla_dimensions;
    for (; vdp != NULL; vdp = vdp->next) {
      set_vla_size_statement(vdp, &pos_curr_token);
    }  /* for */
  }  /* if */
  /* Clear the entry for "else" in the stop tokens set.  Without this,
     an else encountered where a statement is expected could cause an
     error recovery loop. */
  old_else_stop_token_value =
                      curr_stop_token_stack_entry->stop_tokens[(int)tok_else];
  curr_stop_token_stack_entry->stop_tokens[(int)tok_else] = 0;
  /* Skip over the opening brace.  Note that this is NOT an internal error
     check; when a compound statement is the body of a function, it's
     required. */
  add_stop_token(tok_rbrace);
  lbrace_pos = (curr_token == tok_lbrace) ? pos_curr_token
                                          : null_source_position;
  (void)required_token(tok_lbrace, ec_exp_lbrace);
  /* This is the only place within a compound statement where C99 and C++11
     predefined pragmas are permitted. */
  if (c99_mode || cpp11_mode || fixed_point_enabled) check_for_stdc_pragmas();
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode) check_for_upc_pragmas(block);
#endif /* UPC_EXTENSIONS_ALLOWED */
  /* It is also the only place where a GNU local label can be declared. */
  while (gnu_mode && curr_token == tok_identifier &&
         strcmp("__label__",
                locator_for_curr_id.symbol_header->identifier) == 0) {
    local_label_declaration();
  }  /* while */

  /* Scan the sequence of statements.  (Note that we may end up here during
     preprocessing error recovery if a C++11 lambda or GNU statement
     expression appears in a #if directive; hence, the test for
     tok_newline.) */
  scope_stack_top().is_compound_statement_block = TRUE;
  while ((curr_token != tok_rbrace && curr_token != tok_end_of_source &&
          curr_token != tok_newline) || scope_stack_top().injections != NULL) {
    if (!C_mode()) {
      /* In C++ mode, where declarations can be interspersed with
         executable statements, statement() handles declarations, too. */
      a_boolean  injected_stmt = FALSE;
      if (scope_stack_top().injections != NULL) {
        /* If there are pending injections at this level, inject the tokens
           for the next one now. */
        an_il_entity_list_entry  *ielep = scope_stack_top().injections;
        a_token_sequence         *tsp = (a_token_sequence*)ielep->entity.ptr;
        rescan_persistent_reusable_cache((a_token_cache*)tsp->token_cache);
        scope_stack_top().injections = ielep->next;
        injected_stmt = TRUE;
      }  /* if */
      statement(/*is_dependent_statement=*/FALSE, marked_as_gnu_extension);
      if (injected_stmt) {
        /* The statement we just saw was injected.  The next token should be
           the cache terminator. */
        if (curr_token != tok_end_of_source) {
          pos_error(ec_extraneous_injected_statement_tokens, &pos_curr_token);
        }  /* if */
        flush_past_token_cache_terminator();
      }  /* if */
    } else {
      /* In C mode the declarations are expected to appear first.  Note that
         label statements may look like the start of a declaration, so we
         have to check for ident followed by ":".  In C99 and GNU C modes,
         declarations may be interspersed with statements. */
      a_source_position  gnu_extension_pos;
      if (curr_token == tok_extension) {
        /* Record the presence of the __extension__ keyword and ensure that
           its position will be used as the starting position of the
           statement. */
        marked_as_gnu_extension = TRUE;
        gnu_extension_pos = pos_curr_token;
        struct_stmt_stack_top().p_start_pos = &gnu_extension_pos;
        (void)get_token();
      }  /* if */
      if (curr_token == tok_attribute || std_attribute_tokens_next()) {
        /* Scan leading attributes.  Generally these are associated with
           a declaration, but the "fallthrough" attribute is associated with
           a statement and its presence would cause is_decl_start to
           disambiguate incorrectly.  These may be "unscanned" below. */
        struct_stmt_stack_top().prefix_attributes = scan_attributes(al_prefix);
      }  /* if */
      if ((curr_token != tok_identifier || next_token() != tok_colon) &&
          is_decl_start(IDS_EXPR_CONTEXT |
                        IDS_REAL_DECLARATOR_ALLOWED)) {
        /* Scan a declaration.  In C89, but not in C99, these must all be at
           the beginning of the block.   GCC has always been permissive about
           this (even in its non-C99 mode).  Newer versions of the Microsoft
           compiler also implement the C99 rule. */
        if (!allow_decl_after_stmt && any_statements) {
          pos_error(ec_declaration_after_statements, &error_position);
          /* Special error-recovery trick: this tries to deal with mismatched
             braces, in the case where a "}" is missing and thus there appears
             to be an extra "{".  If we are at function level, and the next
             thing appears to be a declaration rather than a statement,
             and it's not indented, assume a "}" and exit the compound
             statement. */
          if (at_function_level && pos_curr_token.column == 1) break;
        }  /* if */
        (void)select_curr_construct_pragmas(/*add_to_list=*/FALSE);
        if (struct_stmt_stack_top().prefix_attributes != NULL) {
          /* Make previously scanned attributes available to declaration
             processing. */
          unscan_attributes(struct_stmt_stack_top().prefix_attributes);
          struct_stmt_stack_top().prefix_attributes = NULL;
        }  /* if */
        decl_statement(marked_as_gnu_extension,
                       /*p_okay_in_constexpr_body=*/NULL);
      } else {
        /* Scan a statement. */
        any_statements = TRUE;
        statement(/*is_dependent_statement=*/FALSE, marked_as_gnu_extension);
      }  /* if */
    }  /* if */
  }  /* while */

  /* Move cached #pragma declarations (if any) to the current scope stack
     entry.  This is needed for lint "notreached" comments, and also, if this
     is the top level block of the function, so that they can be examined
     and acted upon in processing the implicit return. */
  if (select_curr_construct_pragmas(/*add_to_list=*/FALSE)) {
    /* Check for a lint-style "notreached" comment -- it will affect
       diagnostics in check_void_return_okay. */
    check_lint_notreached_state();
    /* Issue diagnostics on pragmas that are trying to bind to the closing
       right brace of a compound statement or to an implicit return. */
    cannot_bind_to_curr_construct();
  }  /* if */
  if (curr_reachability.reachable) {
    a_boolean        implicit_return = FALSE;
    a_boolean        implicit_rethrow = FALSE;
    a_statement_ptr  sp;

    if (at_function_level || is_function_try_block) {
      /* Falling off the end of a function in reachable code. */
      implicit_return = TRUE;
    } else if (is_catch_clause && depth_stmt_stack == 1 && 
               struct_stmt_stack[0].kind == ssk_try_block) {
      /* Handler for a function try block. */
      /* Falling off the end of a handler of a function try block for a
         constructor or destructor produces an implicit rethrow; for other
         functions it produces an implicit return (15.3 paragraph 16).
         Microsoft Visual C++ 7.0 implements function try blocks, but
         not this aspect of it.  The 7.1 compiler does this correctly. */
      a_routine_ptr  rp = current_routine_entry();
      if (!(microsoft_bugs && microsoft_version <= 1300) &&
          (special_kind_is(rp, sfk_constructor) ||
           special_kind_is(rp, sfk_destructor))) {
        implicit_rethrow = TRUE;
      } else {
        implicit_return = TRUE;
      }  /* if */
    }  /* if */
    if (implicit_return) {
      /* Check that a void return (one returning no value) is compatible
         with the current function (i.e., the current function should also
         have type void), and add a return with no expression. */
      an_expr_node_ptr return_expr;
      a_routine_ptr    rp = current_routine_entry();
#if VLA_DEALLOCATIONS_IN_IL
      if (vla_enabled && vla_deallocations_in_il) {
        /* Put out a vla-dealloc statement for each declaration of a VLA
           variable in the currently active blocks of the function. */
        check_assertion(end_of_control_flow_descr_list != NULL);
        a_statement_ptr  vla_dealloc_stmts =
                            collect_vla_dealloc_stmts_for_function(
                                              end_of_control_flow_descr_list);
        if (vla_dealloc_stmts != NULL) {
          add_statement_list(vla_dealloc_stmts, curr_reachability.reachable);
        }  /* if */
      }  /* if */
#endif /* VLA_DEALLOCATIONS_IN_IL */
      /* Make sure that a void return is acceptable here.  If this is the main
         routine, generate an implicit return value, if possible. */
      check_void_return_okay(/*is_implicit_return=*/TRUE, &return_expr);
      /* The statement is not allocated earlier because we don't want it to
         affect the reachability information.  The source position on the
         statement is null to indicate that it is compiler generated. */
      if (rp->is_coroutine) {
        a_coroutine_descr_ptr cdp = get_coroutine_descr(rp);
        if (cdp->has_return_void) {
          sp = add_statement_at_stmt_pos(stmk_coroutine_return,
                                         &null_source_position,
                                         /*compiler_generated=*/TRUE);
          sp->expr = make_coroutine_result_expression(/*alep=*/NULL,
                                                      /*is_yield=*/FALSE,
                                                      sp);
        }  /* if */
      } else {
        sp = add_statement_at_stmt_pos(stmk_return, &null_source_position,
                                       /*compiler_generated=*/TRUE);
        /* Insert an implied return value if there is one. */
        sp->expr = return_expr;
      }  /* if */
    } else if (implicit_rethrow) {
      /* Generate a rethrow. */
      sp = add_statement_at_stmt_pos(stmk_expr, &null_source_position,
                                     /*compiler_generated=*/TRUE);
      sp->expr = alloc_expr_node((an_expr_node_kind)enk_throw);
      /* There is no throw expression (i.e., this is a rethrow), so discard
         the throw supplement. */
      sp->expr->variant.throw_info = NULL;
      sp->expr->type = void_type();
      set_expr_result_not_used(sp->expr);
      set_unreachable(curr_reachability);
    }  /* if */
  }  /* if */
  /* Process pragmas associated with the closing brace before the current
     scope is popped.  (Note: process_curr_token_pragmas must be called after
     calling select_curr_construct_pragmas and before calling pop_scope.) */
  process_curr_token_pragmas();
  if (is_statement_expr) {
    check_assertion(p_result_type != NULL);
    *p_result_type = struct_stmt_stack_top().type;
  }  /* if */
  if (at_function_level) {
    /* Pop the statement stack. */
    pop_stmt_stack();
    /* Clear statement stack just to be careful. */
    depth_stmt_stack = -1;
  } else {
    /* Block/compound statement rather than function. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Don't pop the name scope here if this is the end of the guarded
       statement of a Microsoft __try statement.  It will be done after the
       __except expression, if any, is processed. */
    if (depth_stmt_stack < 1 ||
        struct_stmt_stack[depth_stmt_stack-1].kind != ssk_microsoft_try ||
        struct_stmt_stack[depth_stmt_stack-1].
                                  in_cleanup_statement_of_microsoft_try)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here */
    {
      finish_block_statement(block);
    }  /* if */
  }  /* if */

  /* Restore the entry for "else" in the stop tokens set (see comment
     above). */
  curr_stop_token_stack_entry->stop_tokens[(int)tok_else] =
                                                    old_else_stop_token_value;
  /* Remember the sequence number of the current token, which is expected
     to be the closing brace. */
  block->variant.block.extra_info->final_position = pos_curr_token;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Add a source sequence entry marking the end of the block. */
  /* With GNU statement expressions it is possible that source sequence entries
     were temporarily disallowed, but pushing and popping the scope stack may
     have lost the flag that indicates that.  Restore it now. */
  source_sequence_entries_disallowed = saved_sses_disallowed;
  add_end_of_construct_source_sequence_entry((char *)block, iek_statement);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  block->end_position = end_pos_curr_token;
  curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (at_function_level) {
    /* The "}" is left for the caller (scan_function_body) to handle. */
  } else {
    /* Check for the closing "}". */
    (void)required_token(tok_rbrace, ec_exp_rbrace, ec_matching_lbrace,
                         &lbrace_pos);
  }  /* if */
  remove_stop_token(tok_rbrace);
#if DEBUG
  if (debug_level >= 3 ||
      (at_function_level && db_flag_is_set("dump_stmts"))) {
    int  how_deep = 3;
    fputs("terminating compound statement for ", f_debug);
    if (at_function_level) {
      db_scope(scope_stack[depth_scope_stack].il_scope);
      fputs("\n", f_debug);
      /* If debug_level is less than 3, then this display is triggered by
         the "dump_stmts" flag; do a full display of the statements in the
         function. */
      if (debug_level < 3) how_deep = 100;
    }  /* if */
    db_statement_list(block, /*indent=*/0, "", how_deep);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return block;
}  /* compound_statement_full */


void start_of_function_try_block(void)
/*
Do initialization required for scanning the body of a function try block.
Note that this is done independently of function_try_block, since
ctor-initializers may have to be processed (in scan_function_body); that's
done before function_try_block is called, but the object-lifetime for the
function try block has to have been established first.
*/
{
  a_statement_ptr           sp;
  a_control_flow_descr_ptr  cfdp;
  a_routine_ptr             rp = current_routine_entry();
  a_source_position         pos;

  db_enter(3, "start_of_function_try_block");
  pos = pos_curr_token;
  /* Some of this is identical to initializations done for function blocks
     in compound_statement. */
  set_reachable(curr_reachability);
  control_flow_descr_list = end_of_control_flow_descr_list = NULL;
  /* Clear statement stack just to be careful. */
  depth_stmt_stack = -1;
  /* The function try block (including its catch clauses) is contained
     within a block entry in the control_flow_descr_list. */
  cfdp = alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_block);
  /* Set the lifetime in the control flow entry. */
  cfdp->variant.block.object_lifetime = curr_object_lifetime;
  add_to_control_flow_descr_list(cfdp);
  sp = alloc_statement(stmk_try_block, /*compiler_generated=*/FALSE);
  sp->variant.try_block->is_function_try_block = TRUE;
  /* Record position information: */
  sp->position = pos_curr_token;
  stmt_update_source_sequence_list(sp);
  /* Do additional initialization generic to scanning a try statement. */
  start_of_try_block(sp);
  if (rp->is_constexpr && !constexpr_try_enabled) {
    pos_error(special_kind_is(rp, sfk_constructor) ?
                            ec_constexpr_constructor_with_function_try_block :
                            ec_constexpr_function_with_function_try_block,
              &pos);
  }  /* if */
  if (special_kind_is(rp, sfk_constructor) ||
      special_kind_is(rp, sfk_destructor)) {
    /* For a constructor or destructor, push a block object lifetime inside
       the try-block lifetime to capture any destructions in the
       ctor-initializer list.  Those get done on exit from the main statement,
       before any catch clause is entered. */
    push_object_lifetime((an_il_entry_kind)iek_none,
                         (char *)NULL,
                         (an_object_lifetime_kind)olk_block);
  }  /* if */
  db_exit();
}  /* start_of_function_try_block */


a_statement_ptr function_try_block(a_boolean  explicit_return_type)

/*
Scan a function try block.  The "try" token will already have been consumed,
ctor-initializers will have been scanned, and the current token should be
the left brace.  Moreover, initialization for scanning the function body will
have been done.  This routine (along with try_block_statement which it calls)
is in effect a wrapper around compound_statement.
*/
{
  a_statement_ptr  sp;

  db_enter(3, "function_try_block");
  check_assertion(depth_stmt_stack == 0 &&
                  struct_stmt_stack[0].kind ==
                                 (a_struct_stmt_kind)ssk_try_block);
  sp = struct_stmt_stack[depth_stmt_stack].statement;
  try_block_statement(sp, explicit_return_type);
  /* Terminate the control flow block for the function try block. */
  add_to_control_flow_descr_list(
     alloc_control_flow_descr((a_control_flow_descr_kind)cfdk_end_of_block));
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("dump_stmts")) {
    int  how_deep = 3;
    fputs("terminating compound statement for ", f_debug);
    db_scope(scope_stack[depth_scope_stack].il_scope);
    fputs("\n", f_debug);
    /* If debug_level is less than 3, then this display is triggered by
       the "dump_stmts" flag; do a full display of the statements in the
       function. */
    if (debug_level < 3) how_deep = 100;
    db_statement_list(sp, /*indent=*/0, "", how_deep);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return sp;
}  /* function_try_block */


void wrapup_control_flow_processing(a_scope_ptr  scope_ptr)
/*
scope_ptr is identifies the function scope that is just now being popped.
The corresponding compound statement has just been processed -- complete the
processing pertaining to its control flow list.  This routine is called after
compound statement is complete (i.e., from pop_scope) because the object
lifetime for the function has to be popped from the object lifetime stack
before the fixup can be done for pointers in goto and label statements.
*/
{
  a_control_flow_descr_ptr  cfdp;

  if (control_flow_descr_list != NULL) {
    if (!C_mode()) {
      if (scope_ptr->lifetime == NULL) {
        /* The function scope lifetime was eliminated, so be sure it is
           not pointed to by any goto or label statements. */
        /* The labels are still on the control_flow_descr_list. */
        for (cfdp = control_flow_descr_list; cfdp != NULL; cfdp = cfdp->next) {
          if (cfdp->kind == (a_control_flow_descr_kind)cfdk_label) {
            cfdp->variant.label_statement->variant.label.lifetime = NULL;
          } else if (cfdp->kind == (a_control_flow_descr_kind)cfdk_goto) {
            cfdp->variant.goto_statement.ptr->variant.label.lifetime = NULL;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    remove_list_of_control_flow_descrs(control_flow_descr_list,
                                       end_of_control_flow_descr_list);
    control_flow_descr_list = end_of_control_flow_descr_list = NULL;
  }  /* if */
}  /* wrapup_control_flow_processing */

#if DEBUG

unsigned long show_statements_space_used(void)
/*
Display and return the amount of space used for various statements tables.
*/
{
  unsigned long num, size, total, grand_total = 0;

  db_space_used_header("Statements table use:");
  db_space_used_general("struct stmt stack",
                        (unsigned long)size_struct_stmt_stack_container,
                        a_struct_stmt_stack_entry);
  db_space_used_lost("control flow descrs", avail_control_flow_descrs,
                     num_control_flow_descrs_allocated,
                     a_control_flow_descr);

  db_space_used_total();

  return (grand_total);
}  /* show_statements_space_used */

#endif /* DEBUG */

void statements_one_time_init(void)
/*
One-time initialization for statements.c static variables.
*/
{
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_control_flow_descrs),
#if DEBUG
      pch_saved_var_array_elem(num_control_flow_descrs_allocated),
#endif /* if DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  register_trans_unit_variable(control_flow_descr_list);
  register_trans_unit_variable(end_of_control_flow_descr_list);
  register_trans_unit_variable(struct_stmt_stack);
  register_trans_unit_variable(depth_stmt_stack);
  register_trans_unit_variable(struct_stmt_stack_container);
  register_trans_unit_variable(size_struct_stmt_stack_container);
  register_trans_unit_variable(curr_reachability);
#if UPC_EXTENSIONS_ALLOWED
  register_trans_unit_variable(affinity_forall_loop);
  register_trans_unit_variable(innermost_forall_loop);
#endif /* UPC_EXTENSIONS_ALLOWED */
  register_trans_unit_variable(already_diagnosed_selection_initializer);
  register_trans_unit_variable(already_diagnosed_init_in_range_for);
}  /* statements_one_time_init */


void statements_trans_unit_init(void)
/*
Initialize static variables related to statement processing.  These must
be repeated for every (primary or secondary) translation unit.
*/
{
  control_flow_descr_list = NULL;
  end_of_control_flow_descr_list = NULL;
  /* Initialize some global variables declared in statements.h. */
  struct_stmt_stack = NULL;
  depth_stmt_stack = -1;
  /* Initialize static variables. */
  struct_stmt_stack_container = NULL;
  size_struct_stmt_stack_container = 0;
#if UPC_EXTENSIONS_ALLOWED
  affinity_forall_loop = NULL;
  innermost_forall_loop = NULL;
#endif /* UPC_EXTENSIONS_ALLOWED */
  already_diagnosed_selection_initializer = FALSE;
  already_diagnosed_init_in_range_for = FALSE;
}  /* statements_trans_unit_init */


void statements_init(void)
/*
Initialize static variables related to statement processing.  This is done as
a subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  avail_control_flow_descrs = NULL;
#if DEBUG
  num_control_flow_descrs_allocated = 0;
  cfd_id_number = 0;
#endif /* DEBUG */
}  /* statements_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

