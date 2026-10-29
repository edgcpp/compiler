/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

statements.h -- Declarations relating to statements.c (having to do with
                scanning of statements).

*/

/* Avoid including these declarations more than once: */
#ifndef STATEMENTS_H
#define STATEMENTS_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef PRAGMA_H
#include "pragma.h"
#endif /* ifndef PRAGMA_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Indication of whether or not code is reachable from the code immediately
preceding.
*/
typedef struct a_reachability_summary {
  a_boolean	reachable;
			/* Code is reachable, as determined by the front
			   end. */
  a_boolean	reachable_considering_hints;
			/* Code is reachable, as determined by the front
			   end and modified by user hints in the code. */
  a_boolean	suppress_unreachable_warning;
			/* In an unreachable code section, suppress the
			   warning about unreachable code (because it has
			   already been issued, or because of a lint-style
			   comment). */
} a_reachability_summary;


typedef struct a_control_flow_descr *a_control_flow_descr_ptr;
/*
a_control_flow_descr is an entry used in tracking gotos, labels, and
initializing declarations in order to diagnose errors in transferring
control past an initialization.
*/
enum a_control_flow_descr_kind : a_byte {
  cfdk_block,		/* Start of a block. */
  cfdk_init,		/* Refers to an stmk_init, stmk_set_vla_size, or
                           stmk_vla_decl statement. */
  cfdk_goto,		/* Refers to an stmk_goto statement. */
  cfdk_label,		/* Refers to an stmk_label statement. */
  cfdk_case_label,	/* Case label in switch statement. */
  cfdk_end_of_block	/* End of a block. */
};


typedef struct a_control_flow_descr {
  a_control_flow_descr_ptr
		next;
			/* Pointer to the next entry in a linked list; NULL
			   for the last entry on the list. */
  a_control_flow_descr_ptr
		prev;
			/* Pointer to the preceding entry in a linked list;
			   NULL for the first entry on the list. */
  a_control_flow_descr_ptr
		parent;
			/* Pointer to an entry representing the parent
			   block of the given entry.  NULL only for the
			   cfdk_block and cfdk_end_of_block entries associated
			   the routine scope; all other entries on a list
			   have parents. */
  a_source_position
		source_pos;
			/* Source position of the goto statement. */
  a_control_flow_descr_kind
		kind;
			/* The kind of entry. */
#if DEBUG
  unsigned long id_number;
			/* Unique identifying number for this entry. */
#endif /* DEBUG */
#if UPC_EXTENSIONS_ALLOWED
  a_statement_ptr
		enclosing_forall;
			/* Used to track and match up enclosing forall
			   statements for gotos and labels.  Gotos to labels in
			   different forall statements, or into or out of
			   forall statements, are not allowed.  */
#endif /* UPC_EXTENSIONS_ALLOWED */
  union {
    /* When kind == cfdk_case_label: no variant fields */
    /* When kind == cfdk_block: */
    struct {
      a_control_flow_descr_ptr
		end_of_block;
			/* An entry representing the start of a block has a
			   pointer to the entry representing the end of the
			   same block. */
      a_control_flow_descr_ptr
		last_case_label;
			/* When is_switch_block or is_switch_subblock is TRUE,
			   a pointer to the last case label in the current
			   block or a subblock of the current block.  NULL
			   when the block is not contained within a switch
			   statement or contains no case labels. */
      an_object_lifetime_ptr
		object_lifetime;
			/* Pointer to the object lifetime, if any, pushed for
			   the current block. */
      unsigned long
		goto_count;
			/* Number of goto statements in the current block and
			   blocks contained within the current block.  This
			   counter is decremented as goto entries are
			   removed from the list. */
      a_bit_field
		any_labels:1;
			/* TRUE if the current block contains any label
			   statements or any blocks with label statements. */
      a_bit_field
		any_vla_variables:1;
			/* TRUE if the current block contains any vla-decl
			   statements that represent the declaration of a
			   VLA variable (i.e., not a typedef or variable
			   declaration involving a variably modified
			   type). */
      a_bit_field
		is_switch_block:1;
			/* TRUE if the current block represents the body
			   of a switch statement. */
      a_bit_field
		is_switch_subblock:1;
			/* TRUE if is_switch is TRUE for a block in which the
			   current block is enclosed. */
      a_bit_field
		exposed_init_in_switch:1;
			/* If is_switch is TRUE for this block or for a block
			   in which the current block is enclosed, there
			   has been at least one initializing declaration that
			   is "exposed" -- that is, that may give rise to a
			   jump-over-initialization diagnostic if it is
			   followed by a case label before the current block is
			   closed.  Once the block is closed or a case label
			   appears, the flag is cleared. */
      a_bit_field
		is_catch_block:1;
			/* TRUE if this is the top level block of a catch
			   clause. */
      a_bit_field
		is_try_block:1;
			/* TRUE if this is the top level block of a try
			   statement. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_bit_field
		is_finally_block:1;
			/* TRUE if this is the top level block of a C++/CLI
			   finally clause. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      a_bit_field
		is_statement_expr:1;
			/* TRUE if this is the top level block of a GNU
			   statement expression, i.e., ({ ... }). */
      a_bit_field
		is_within_goto_protected_block:1;
			/* TRUE if this block is or is contained within
			   a block for which transfers of control into the
			   block are prohibited. */
      a_bit_field
		is_constexpr_if:1;
			/* TRUE if this block is the top level block of
			   the "then" or "else" of a C++17 constexpr if
			   statement. */
      a_bit_field
		is_if_consteval_branch:1;
			/* TRUE if this block is the top level block of a
			   dependent statement in an "if consteval" or an
			   "if not consteval" statement. */
    } block;
    /* When kind == cfdk_init: */
    struct {
      a_statement_ptr
		statement;
			/* A pointer to an stmk_init, stmk_set_vla_size, or
			   stmk_vla_decl statement.  In addition, in Microsoft
			   C mode it can be an stmk_block statement, which is
			   what's left over if a dynamic initialization is
			   lowered in place.  Finally, it may also be NULL if
			   the entry represents a trivial initialization of a
			   non-POD variable (which requires no actual work,
			   but which must be diagnosed if branched over). */
      a_variable_ptr
		variable;
			/* A pointer to the variable that is initialized.
			   NULL when the statement pointer refers to an
			   stmk_set_vla_size statement. */
      a_bit_field
		is_vla_variable:1;
			/* TRUE if the statement is an stmk_vla_decl
			   statement that represents the declaration of a
			   VLA variable, i.e., a variable that will require
			   allocation at runtime. */
      a_bit_field
		in_statement_expression:1;
			/* TRUE if the initialization appeared inside a GNU
			   statement expression. */
    } init;
    /* When kind == cfdk_goto: */
    struct {
      a_statement_ptr
		ptr;
			/* A pointer to an stmk_goto statement. */
      a_control_flow_descr_ptr
		prev_goto;
			/* A pointer to a cfdk_goto entry that points to a
			   goto statement referring to the same label; NULL
			   for the first goto statement for a given label in
			   the program.  This pointer produces a chain that
			   can be walked to visit all forward gotos referring
			   to a given label. */
    } goto_statement;
    /* When kind == cfdk_label: */
    a_statement_ptr
		label_statement;
			/* A pointer to an stmk_label statement. */
    /* When kind == cfdk_end_of_block: */
    a_control_flow_descr_ptr
		start_of_block;
			/* An entry representing the end of a block has a
			   pointer to the entry representing the start of the
			   same block. */
  } variant;
} a_control_flow_descr;


/*
Stack indicating nesting of structured statements.  There is an entry
on this stack for each current structured statement.  A structured statement
is one that can contain other statements.
*/
enum a_struct_stmt_kind {
  /* Types of structured statements. */
  ssk_compound,		/* Compound statement, i.e., { ... }. */
  ssk_if,		/* if (...) statement. */
  ssk_constexpr_if,	/* if constexpr (...) statement. */
  ssk_switch,		/* switch statement. */
  ssk_while,		/* while (...) {} statement. */
  ssk_do,		/* do {} while (...); statement. */
  ssk_for,		/* for (...; ...; ...) {} statement. */
  ssk_range_based_for,  /* for (... : ...) {} statement. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ssk_for_each,		/* for each (...) {} statement. */
  ssk_microsoft_try,	/* Microsoft try-except or try-finally. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ssk_try_block		/* try compound-stmt handler-seq statement. */
};

typedef struct a_struct_stmt_stack_entry *a_struct_stmt_stack_entry_ptr;
typedef struct a_struct_stmt_stack_entry {
  /* An entry on the structured statement stack, describing one
     current structured statement. */
  a_struct_stmt_kind
		kind;	/* Kind of structured statement. */
  a_bit_field	in_else_of_if:1;
			/* TRUE when kind == ssk_if or ssk_constexpr_if
			   and we are in the "else" clause. */
  a_bit_field	dependent_constexpr_if:1;
			/* TRUE when kind == ssk_constexpr_if, when the
			   condition expression is dependent (so both
			   branches of the if need to be processed). */
  a_bit_field	in_discarded_statement:1;
			/* TRUE when kind == ssk_constexpr_if if we are in
			   the discarded branch of the if. */
  a_bit_field	scope_stack_in_discarded_statement_state:1;
			/* For kind == ssk_constexpr_if, this is the value
			   of the scope stack in_discarded_statement flag
			   at the beginning of processing the constexpr if. */
  a_bit_field	for_init:1;
			/* TRUE if the structured statement is a for loop
			   or range-based for loop (it is not known yet which
			   -- ssk_for is on the stack) and the statement
			   currently being processed is an init-statement;
			   FALSE otherwise. */
  a_bit_field	is_catch_clause:1;
			/* TRUE if kind == ssk_compound and this structured
			   statement represents the top level block of a
			   catch clause. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	parsing_finally_clause:1;
			/* If kind == ssk_try_block, TRUE if we are parsing
			   a C++/CLI finally clause.  This flag is set before
			   the call to compound_statement and cleared
			   afterward. */
  a_bit_field	in_cleanup_statement_of_microsoft_try:1;
			/* TRUE if currently inside the cleanup statement of
			   a Microsoft try-finally or try-except. */
  a_bit_field	in_handler_parameter_declaration:1;
			/* TRUE if currently inside the declaration of the
			   handler parameter. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	rout_type_explicitly_specified:1;
			/* TRUE if the current routine was declared with an
			   explicit return type.  This flag is set in the
			   top level statement stack entry only. */
  a_bit_field	any_exec_statement_seen:1;
			/* Within compound statements (blocks), TRUE if any
			   executable statement (not declaration) has been
			   seen. */
  a_bit_field	label_invalidates_curr_block_object_lifetime:1;
			/* TRUE if kind == ssk_compound and the object
			   lifetime pointed to by this entry has been
			   invalidated by a label in an inner block. This
			   flag will be cleared again once the required fixup
			   has been done and the curr_block_object_lifetime
			   pointer has been reset. */
  a_bit_field	is_statement_expr:1;
			/* TRUE if the statement is a GNU statement
			   expression, ({ ... }). */
  a_bit_field	inside_statement_expr:1;
			/* TRUE if the statement is or is inside of a
			   GNU statement expression. */
  a_bit_field	switch_has_dependent_case:1;
			/* Set only in entries with kind == ssk_switch.
			   Indicates that at least one case contains a
			   template-dependent constant. */
  a_bit_field	in_switch_condition:1;
			/* Set only in entries with kind == ssk_switch.  TRUE
			   while the condition of the switch statement is
			   being scanned.  A break statement or a case label in
			   the condition (in a GNU statement expression) does
			   not apply to that switch statement. */
  a_bit_field	contains_user_label:1;
			/* TRUE if the structured statement contains a user-
			   declared label (used to avoid spurious reachability
			   warnings). */
  a_bit_field	contains_active_switch_case:1;
			/* TRUE if the structured statement contains a switch
			   case label for a switch statement that is still on
			   the statement stack (used to avoid spurious
			   reachability warnings). */
  a_bit_field	record_declared_entities:1;
			/* TRUE while declared entities should be recorded in
			   the declared_entities list. */
  a_statement_ptr
		statement;
			/* The associated IL statement.  Indirectly,
			   also gives the pointer to the first dependent
			   statement of the structured statement. */
  an_attribute_ptr
		prefix_attributes;
			/* A pointer to the attributes scanned (NULL if none)
			   at the beginning of the current statement. */
  a_constant_ptr
		switch_max_case_value;
			/* If non-NULL, points to the constant with the
			   maximum value so far in a switch statement case
			   label. */
  a_switch_case_entry_ptr
		last_switch_case_entry;
			/* Points to the last switch case entry (if any)
			   pointed to by the a_switch_stmt_descr associated
			   with the current (switch) statement. */
  a_switch_case_entry_ptr
		last_switch_case_on_sorted_list;
			/* Points to the last entry on the "sorted_cases" list
			   pointed to by the a_switch_stmt_descr associated
			   with the current (switch) statement. */
  a_statement_ptr
		extra_block;
			/* If non-NULL, points to an stmk_block statement
			   added under the primary statement for this
			   structured statement in order to allow attaching
			   more than one statement under a statement that
			   allows only one. */
  a_statement_ptr
		last_dep_statement;
			/* Points to the last dependent statement under
			   the structured statement (or under extra_block,
			   if that is non-NULL).  NULL if there are
			   no dependent statements. */
  a_label_ptr	break_label;
			/* Label to be branched to for a break out of this
			   statement.  NULL until needed. */
  a_control_flow_descr_ptr
		break_statements;
			/* Pointer to a linked list of control flow entries
			   identifying the break statements (if any) in this
			   structured statement. */
  a_label_ptr	continue_label;
			/* Label to be branched to for a continue of this loop
			   statement.  NULL until needed. */
  a_control_flow_descr_ptr
		continue_statements;
			/* Pointer to a linked list of control flow entries
			   identifying the continue statements (if any) in this
			   structured statement. */
  a_type_ptr	type;
			/* A type associated with the statement.  For switch
			   statements (ssk_switch), this is the type of the
			   switch selector expression (int or long).  For the
			   compound statement of a GNU statement expression,
			   this is the result type.  Currently NULL for all
			   other cases. */
  a_reachability_summary
		start_reachable;
			/* Indicates whether or not the start of the structured
			   statement is reachable. */
  a_reachability_summary
		end_reachable;
			/* Indicates whether or not the end of the structured
			   statement is reachable. */
  an_object_lifetime_ptr
		curr_block_object_lifetime;
			/* If kind == ssk_compound, a pointer to the currently
			   active object lifetime directly associated with
			   this block (if any).  A lifetime is pushed when a
			   block starts, but a label in the midst of the
			   block "invalidates" the lifetime and a new one is
			   pushed to replace it; this pointer then points to
			   the new one.  Also set on entries with
			   kind == ssk_switch, to track the lifetime in
			   the top block. */
  an_il_entity_list_entry_ptr
		*p_declared_entities;
			/* If non-NULL, we are recording entities declared by
			   the current statement (normally, a declaration
			   statement, although we might record an entity
			   declared in an expression statement while performing
			   expression vs. declaration disambiguation).
			   *p_declared_entities points to the list of recorded
			   entities (or NULL if there are none).  Note that
			   this field cannot directly point the list of
			   recorded entities, because a pointer to the head of
			   the list may be maintained elsewhere (see the field
			   p_postfix_entities in a_decl_parse_state) and the
			   statement stack may be reallocated elsewhere during
			   declaration processing (e.g., during a template
			   instantiation). */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		last_sse_before_expr_decl_disambiguation;
			/* When disambiguating between a declaration and an
			   expression, source sequence entries may be created
			   (e.g., for a template argument like X<struct S>
			   where S is declared for the first time).  Any such
			   entries will be moved to after the entry for the
			   statement (which will be created after
			   disambiguation has determined the statement
			   kind). */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_scope_depth depth_of_assoc_scope;
			/* If kind == ssk_compound and a scope stack entry
			   was pushed in conjunction with this structured
			   statement stack entry, the depth of the former in
			   the scope stack; NO_SCOPE_DEPTH otherwise. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  unsigned long	num_microsoft_trys_inside_of;
			/* Number of Microsoft try-finally or try-except
			   statements currently on the stack. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_source_position
		*p_start_pos;
			/* If non-NULL, this points to the starting position
			   of the current dependent statement.  This could be
			   the position of a GNU __extension__ keyword or of a
			   prefix attribute.  Determines the statement position
			   used by add_statement (if NULL, pos_curr_token is
			   used). */
  a_statement_ptr
		fallthrough_statement;
			/* When non-NULL, points to a fallthrough statement
			   that is still "in force" for the current level in
			   the statement stack (even though it may point to
			   a statement at a deeper level).  Used to give a
			   diagnostic when a fallthrough statement is the last
			   statement in a block/switch statement.  The field
			   is used as a flag as well (but the source position
			   is needed if a diagnostic is to be given). */
} a_struct_stmt_stack_entry;

EXTERN_THREAD a_struct_stmt_stack_entry_ptr
		struct_stmt_stack;
			/* The currently active structured statement stack
			   itself.  The current entry is [depth_stmt_stack].
			   Entry [0] is for the main block of the current
			   function, if we are currently inside a function.
			   Note that in C++ there can be more than one such
			   stack, though only one is active at a time.  The
			   struct_stmt_stack array is actually a subarray of
			   struct_stmt_stack_container. */

EXTERN_THREAD int
		depth_stmt_stack;
			/* Index of the current entry in struct_stmt_stack.
			   -1 if the stack is empty. */

#define struct_stmt_stack_top()  (struct_stmt_stack[depth_stmt_stack])

#if GNU_EXTENSIONS_ALLOWED

extern a_boolean in_gnu_stmt_expression();

#endif /* GNU_EXTENSIONS_ALLOWED */

extern a_statement_ptr add_statement_at_stmt_pos(
                                         a_statement_kind  kind,
                                         a_source_position *stmt_pos,
                                         a_boolean         compiler_generated);

extern void update_init_statement_control_flow(a_statement_ptr  sp);

extern void record_trivial_init_control_flow(a_variable_ptr  var);

extern void set_vla_size_statement(a_vla_dimension_ptr  vdp,
                                   a_source_position    *pos);

extern void generate_vla_size_statements_for_type(a_type_ptr         tp,
                                                  a_source_position  *pos);

extern a_statement_ptr compound_statement_full(
                                          a_boolean   at_function_level,
                                          a_boolean   explicit_return_type,
                                          a_boolean   is_catch_clause,
                                          a_boolean   is_statement_expr,
                                          a_boolean   marked_as_gnu_extension,
                                          a_type_ptr  *p_result_type);

#define compound_statement(at_function_level, explicit_return_type,          \
                           is_catch_clause, is_statement_expr)               \
  (compound_statement_full(at_function_level, explicit_return_type,          \
                           is_catch_clause, is_statement_expr,               \
                           /*marked_as_gnu_extension=*/FALSE,                \
                           /*p_result_type=*/(a_type_ptr*)NULL))

extern void start_of_function_try_block(void);

extern a_statement_ptr function_try_block(a_boolean  explicit_return_type);

extern a_statement_ptr wrap_coroutine_body_in_try_block(
                                           a_routine_ptr         coroutine,
                                           a_statement_ptr       func_body,
                                           a_coroutine_descr_ptr cr_desc,
                                           an_expr_node_ptr      init_suspend);

extern void wrapup_control_flow_processing(a_scope_ptr  scope_ptr);

extern void warn_if_code_is_unreachable(an_error_code      error_code,
                                        a_source_position  *err_pos);

/* Structure for saving the current state of the structured statement stack
   so that it can be reinitialized to handle a nested function and then
   restored to continue processing the current function. */
typedef struct a_struct_stmt_stack_state {
  a_ptrdiff	container_pos;
			/* Saved position of the current struct_stmt_stack
			   within struct_statement_stack_container (a static
			   variable of statements.c). */
  int		depth_stmt_stack;
			/* Saved value of global variable depth_stmt_stack. */
  a_reachability_summary
		code_reachability;
			/* Saved value of code_reachability (a static variable
			   of statements.c). */
  a_control_flow_descr_ptr
		control_flow_list;
			/* Saved pointer to head of control flow list. */
  a_control_flow_descr_ptr
		end_of_control_flow_list;
			/* Saved pointer to tail of control flow list. */
} a_struct_stmt_stack_state;

namespace detail {

/*
The following specializations provide Is_trivially_copyable and
Is_trivially_destructible support for a_struct_stmt_stack_state.
*/

template<>
struct Is_trivially_copyable_edg_impl<a_struct_stmt_stack_state> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<a_struct_stmt_stack_state> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

}  /* namespace detail */

extern void new_struct_stmt_stack(a_struct_stmt_stack_state *saved_state);
extern void restore_struct_stmt_stack(a_struct_stmt_stack_state *saved_state);

extern a_boolean at_end_of_statement_expression(void);

extern a_boolean inside_statement_expression(void);

extern a_boolean in_catch_clause(void);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean inside_finally_clause(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean in_switch_statement(void);

extern void record_entity_in_decl_stmt_if_needed(a_symbol_ptr  sym);

extern void statements_one_time_init(void);

extern void statements_trans_unit_init(void);

extern void statements_init(void);

#if DEBUG
/* Show and return the amount of memory used by statements entries. */
extern unsigned long show_statements_space_used(void);
#endif /* DEBUG*/

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef STATEMENTS_H */

