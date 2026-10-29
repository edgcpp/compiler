/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

il_walk.h -- Declarations related to il_walk.c (walking the intermediate
             language tree).

*/

/* Avoid including these declarations more than once. */
#ifndef IL_WALK_H
#define IL_WALK_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/* Type of function called to process each non-string entry.  First arg
   is the (new) pointer to the entry and second is the kind of entry. */
typedef void an_entry_process_function(char *, an_il_entry_kind);
typedef an_entry_process_function *an_entry_process_function_ptr;

#if IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS

/* Type of function called to process each string entry.  First arg
   is the (new) pointer to the entry, second is the kind of entry, and
   third is the string length in bytes. */
typedef void a_string_entry_process_function(char *, an_il_entry_kind, 
                                             sizeof_t);
typedef a_string_entry_process_function *a_string_entry_process_function_ptr;
/* Type of function called to remap an old entry pointer to a new entry
   pointer. */
typedef char *a_remap_function(char *, an_il_entry_kind);
typedef a_remap_function *a_remap_function_ptr;
/* Type of function called to test for termination in the IL walk.
   First parameter is the pointer to the entry and second is the kind
   of entry.  Returned value of TRUE means prune the walk at this entry. */
typedef a_boolean a_walk_termination_test_function(char *, an_il_entry_kind);
typedef a_walk_termination_test_function *a_walk_termination_test_function_ptr;

/*
If this flag is TRUE, the IL walk routines that allow remapping of the
pointers in an entry in isolation (i.e., not as part of an IL tree walk) are
compiled.  Note that the top-level routine remap_pointers_in_il_entry is
compiled regardless of the setting of this flag, because it is used
by the trans_copy.c code.
*/
#if IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT
#define REMAP_ONLY_ROUTINES_NEEDED TRUE
#else /* !(IL_SHOULD_BE_WRITTEN_TO_FILE && ...) */
#define REMAP_ONLY_ROUTINES_NEEDED FALSE
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && ... */

EXTERN_THREAD a_boolean
		flag_value_meaning_visited;
			/* Value to be placed in the il_walk_flag field
			   to indicate that an entry has been visited.
			   The value alternates between FALSE and TRUE. */

#if IL_WALK_NEEDED 

/* Walk the intermediate language tree for the file scope. */
extern void walk_file_scope_il(
            an_entry_process_function_ptr        entry_process_function,
            a_string_entry_process_function_ptr  string_entry_process_function,
            a_remap_function_ptr                 remap_function,
            a_remap_function_ptr                 list_remap_function,
            a_walk_termination_test_function_ptr termination_test_function,
            a_boolean                            clear_fe_pointers);

/* Walk the intermediate language tree for a routine scope. */
extern void walk_routine_scope_il(
            a_memory_region_number               region_number,
            an_entry_process_function_ptr        entry_process_function,
            a_string_entry_process_function_ptr  string_entry_process_function,
            a_remap_function_ptr                 remap_function,
            a_remap_function_ptr                 list_remap_function,
            a_walk_termination_test_function_ptr termination_test_function,
            a_boolean                            clear_fe_pointers);

/* Walk a subtree of the IL. */
extern void walk_il_subtree(
            an_entry_process_function_ptr        entry_process_function,
            a_string_entry_process_function_ptr  string_entry_process_function,
            a_remap_function_ptr                 remap_function,
            a_remap_function_ptr                 list_remap_function,
            a_walk_termination_test_function_ptr termination_test_function,
            a_boolean                            clear_fe_pointers,
            char                                 *ptr,
            an_il_entry_kind                     kind);

#endif /* IL_WALK_NEEDED */

#if MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM
#if ONE_INSTANTIATION_PER_OBJECT
extern void set_per_instantiation_needed_flag(char             *entry_ptr,
                                              an_il_entry_kind entry_kind,
                                              unsigned long    bit_number);
#endif /* ONE_INSTANTIATION_PER_OBJECT */

extern void mark_as_needed(char             *entry_ptr,
                           an_il_entry_kind entry_kind);

extern void mark_as_needed_like(char                    *entry_ptr,
                                an_il_entry_kind        entry_kind,
                                a_source_correspondence *model_scp,
                                a_boolean               set_class_defn_needed);

extern void remark_as_needed(char             *entry_ptr,
                             an_il_entry_kind entry_kind);

extern void mark_to_keep_in_il(char             *entry_ptr,
                               an_il_entry_kind entry_kind);

extern void remark_routine_definition_needed(a_routine_ptr rout);

extern void set_routine_definition_needed(a_routine_ptr rout);

extern void set_class_definition_needed(a_type_ptr type);

extern void set_routine_keep_definition_in_il(a_routine_ptr rout);

extern void set_class_keep_definition_in_il(a_type_ptr type);

extern void walk_subtrees_of_local_entities(a_scope_ptr scope);

EXTERN_THREAD a_boolean
		end_of_file_scope_needed_flags_phase;
			/* TRUE during the phase at the end of the file scope
			   that deals with walking the subtrees of variables
			   and classes to set needed flags. */
#endif /* MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM */

extern
void remap_pointers_in_il_entry(char                 *entry_ptr,
                                an_il_entry_kind     entry_kind,
                                a_remap_function_ptr remap_function,
                                a_remap_function_ptr list_remap_function,
                                a_boolean            clear_fe_pointers);

#if REMAP_ONLY_ROUTINES_NEEDED
extern void remap_il_header_pointers(a_remap_function_ptr remap_function,
                                     a_remap_function_ptr list_remap_function);

extern void remap_first_ptr_of_orphaned_file_scope_entry_array(
                                          a_remap_function_ptr remap_function);
#endif /* REMAP_ONLY_ROUTINES_NEEDED */

#if IL_SHOULD_BE_WRITTEN_TO_FILE
extern void remap_last_ptr_of_orphaned_file_scope_entry_array(
                                          a_remap_function_ptr remap_function);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

extern void il_walk_init(void);

#endif /* IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS */

#if NEED_DECLARATIVE_WALK

extern void walk_declarative_entities_in_scope(
                         a_scope_ptr                   scope,
                         an_entry_process_function_ptr entry_process_function);

#endif /* NEED_DECLARATIVE_WALK */

typedef void a_type_list_processing_routine(a_type_ptr type_list);
typedef a_type_list_processing_routine *a_type_list_processing_routine_ptr;

extern void process_local_types(
                   a_scope_ptr                        scope,
                   a_type_list_processing_routine_ptr list_processing_routine);


/*
Types for expression and statement traversal routines.
*/
typedef struct an_expr_or_stmt_traversal_block
		*an_expr_or_stmt_traversal_block_ptr;
/* Type of function called to process an expression node. */
typedef void a_traversal_expr_process_function(
                                   an_expr_node_ptr                    expr,
                                   an_expr_or_stmt_traversal_block_ptr tblock);
typedef a_traversal_expr_process_function
		*a_traversal_expr_process_function_ptr;
/* Type of function called to process a constant. */
typedef void a_traversal_constant_process_function(
                                  a_constant_ptr                      constant,
                                  an_expr_or_stmt_traversal_block_ptr tblock);
typedef a_traversal_constant_process_function
		*a_traversal_constant_process_function_ptr;
/* Type of function called to process a dynamic initialization. */
typedef void a_traversal_dynamic_init_process_function(
                                   a_dynamic_init_ptr                  dip,
                                   an_expr_or_stmt_traversal_block_ptr tblock);
typedef a_traversal_dynamic_init_process_function
		*a_traversal_dynamic_init_process_function_ptr;
/* Type of function called to process a statement. */
typedef void a_traversal_statement_process_function(
                                 a_statement_ptr                     statement,
                                 an_expr_or_stmt_traversal_block_ptr tblock);
typedef a_traversal_statement_process_function
		*a_traversal_statement_process_function_ptr;
/* Type of function called to process a type. */
typedef void a_traversal_type_process_function(
                                 a_type_ptr                          type,
                                 an_expr_or_stmt_traversal_block_ptr tblock);
typedef a_traversal_type_process_function
		*a_traversal_type_process_function_ptr;

typedef struct a_seq_pt_var_entry a_seq_pt_var_entry_dummy_typedef;

/*
Structure to track a_constant/ck_aggregate entries being traversed.  It is
possible, via ck_address/abk_temporary entries, to have cycles in the structure
of a_constant, and this is used to detect those cycles and avoid runaway
recursion in traverse_constant. */
struct an_aggregate_constant_stack_entry {
  an_aggregate_constant_stack_entry
		*prev;
			/* Previous entry on the stack (or NULL, if none). */
  a_constant	*aggr_constant;
			/* a_constant entry being traversed. */
};


typedef struct an_expr_or_stmt_traversal_block {
  /* If you add fields here, also add them to
     clear_expr_or_stmt_traversal_block. */
  /* For the callback routines, if the pointer is NULL no routine is
     called.  The tree is still traversed below that node. */
  a_traversal_expr_process_function_ptr
		process_expr;
			/* Function called for each expression node, before
			   the subtree. */
  a_traversal_expr_process_function_ptr
		process_post_expr;
			/* Function called for each expression node, after
			   the subtree. */
  a_traversal_constant_process_function_ptr
		process_constant;
			/* Function called for each constant, before the
			   subtree. */
  a_traversal_constant_process_function_ptr
		process_post_constant;
			/* Function called for each constant, after the
			   subtree. */
  a_traversal_dynamic_init_process_function_ptr
		process_dynamic_init;
			/* Function called for each dynamic init, before the
			   subtree */
  a_traversal_dynamic_init_process_function_ptr
		process_post_dynamic_init;
			/* Function called for each dynamic init, after the
			   subtree. */
  a_traversal_statement_process_function_ptr
		process_statement;
			/* Function called for each statement, before the
			   subtree. */
  a_traversal_statement_process_function_ptr
		process_post_statement;
			/* Function called for each statement, after the
			   subtree. */
  a_traversal_type_process_function_ptr
		process_type;
			/* Function called for each type, before the
			   subtree. */
  an_aggregate_constant_stack_entry
		*curr_aggregate;
			/* The current ck_aggregate constant being
			   traversed. */
  a_boolean	terminate;
			/* A called routine can set this to TRUE to
			   terminate the tree walk. */
  a_boolean	suppress_subtree_walk;
			/* A called routine can set this to TRUE to
			   suppress the walk of the subtree of the
			   current entry. */
  a_boolean	result;
			/* A place for called routines to store a boolean
			   result for the overall walk. */
  a_boolean	process_non_dynamic_constants;
			/* If TRUE, constants that are not dynamic (i.e.,
			   that do not potentially include executable code/
			   expressions) are also walked.  Ordinarily, they
			   are not walked because we are primarily looking for
			   expressions and statements. */
  a_boolean	process_expressions_for_constants;
			/* If TRUE, the expressions recorded for constants
			   are traversed.  The user routines are NOT called
			   for any constant with a recorded expression --
			   the model is that the expression replaces the
			   constant in the traversal. */
  a_boolean	process_template_parameter_constants_and_expressions;
			/* If TRUE, constants and expressions that appear in
			   ck_template_parameter constants are also walked. */
  Ptr_set<a_void_ptr>
		*visited_shared_exprs;
			/* If non-NULL, the set of expressions the walk has
			   already followed from the constants that record
			   them, as the two preceding flags ask it to.  Each
			   such expression is walked just once.  This
			   matters because those expressions may be shared,
			   and they are the only means by which a walk leaves
			   the tree it is given, so following them makes the
			   walk cover a graph; repeating shared subgraphs
			   costs time exponential in the depth of that graph.
			   Supply a set only for a walk whose outcome does not
			   depend on how often a node is reached, such as a
			   predicate asking whether the tree holds any node
			   with a given property. */
  unsigned long	shared_expr_budget;
			/* If nonzero, the number of such expressions the walk
			   may still follow.  Each one followed decrements it,
			   and the walk terminates once it runs out.  A walk
			   that has no set can thereby bound what the graph
			   case described above may cost it: A budget left at
			   zero says the walk was cut short, and walking again
			   with a set then costs little more than the
			   abandoned walk did.  Obtaining and releasing a set
			   costs more than most walks do, so a caller that
			   usually covers few of these expressions is better
			   served by spending a budget first.  This is ignored
			   when a set is supplied, as the set already keeps
			   the walk to one visit per expression. */
  a_boolean	follow_addressing_path;
			/* If TRUE, the subtree walk visits only the operands
			   that lead to the ultimate underlying object for
			   an addressing expression.  This can be used to
			   determine some attribute of the underlying object
			   (e.g., is it automatic) given an lvalue or a
			   pointer rvalue for it.  See comment on proper use
			   in traverse_addressing_subtree. */
  a_boolean	follow_class_rvalue_addressing_path;
			/* If follow_addressing_path is TRUE, then if this
			   is TRUE the subtree walk should follow class rvalue
			   objects as part of the addressing walk. */
  a_boolean	has_recursive_aggregate_constant;
			/* TRUE if traverse_constant found a recursive
			   aggregate. */
  a_boolean	skip_expr_process_type;
			/* TRUE if traverse_expr should not call process_type
			   directly on a given expression's type.
			   (process_type might still be called elsewhere,
			   including by process_expr, or by traverse_expr on
			   a type that is not pointed-to directly by the
			   expression passed to traverse_expr.) */
  /* Fields used by examine_expr_for_unordered_temp_inits: */
  a_boolean	set_unordered_on_dynamic_inits;
			/* If TRUE, set the "unordered" flag in dynamic
			   initializations encountered in the traversal. */
  a_boolean	relink_dynamic_inits;
			/* If TRUE, relink dynamic initialization entries in
			   the order they were encountered in the traversal. */
  a_dynamic_init_ptr
		last_relinked_dynamic_init;
			/* When relink_dynamic_inits is TRUE, this points
			   to the last processed dynamic initialization. */
  /* Field used by node_has_side_effects et al.: */
  a_boolean	suppress_warning;
			/* TRUE if a warning about an entity having no
			   side effects should be suppressed. */
  a_boolean	for_unused_variable_warning;
			/* TRUE if the side effect scan is being done for the
			   warning about an unused variable.  Certain things
			   that are nominally side effects but don't really
			   do anything productive are not counted. */
  /* Fields used by compute_checksum_for_expr: */
  unsigned long	checksum;
			/* The running checksum for the expression. */
  /* Fields used by expr_complete_object_type: */
  a_type_ptr	complete_object_type;
			/* The type of the complete object underlying the
			   expression being traversed. */
  a_boolean	call_case;
			/* TRUE if expr_complete_object_type was called
			   to determine the object type for a virtual function
			   call. */
  /* Fields used by is_glvalue_for_auto_object: */
  a_boolean	is_temp;
			/* TRUE if the underlying object is a temporary. */
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
  a_variable_ptr
		orig_params;
			/* A list of parameters for a subobject ctor/dtor. */
  a_variable_ptr
		new_params;
			/* A list of parameters for a complete ctor/dtor
			    (should correspond to orig_params above, with the
			    exception that the VTT param is missing). */
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
  /* Fields used by traverse_types_for_expr: */
  a_type_predicate_function_ptr
		type_predicate_function;
			/* The type predicate function to be passed to
			   traverse_type_tree_full to process the types of
			   expressions. */
  a_type_post_order_function_ptr
		type_post_order_function;
			/* The type post-order traversal function to be passed
			   to traverse_type_tree_full. */
  a_type_tree_traversal_flag_set
		type_tree_traversal_flags;
			/* The type traversal flags to be passed to
			   traverse_type_tree_full when processing the types of
			   expressions. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Fields used by examine_expr_for_initonly_field_selection: */
  a_boolean	skip_valid_lvalue_uses_of_initonly_fields;
			/* This is used to determine if C++/CLI initonly field
			   references are valid.  If this is TRUE, references
			   to nonstatic initonly fields evaluated within their
			   containing class's instance constructor are skipped
			   during the tree walk, as are references to static
			   initonly fields evaluated within their containing
			   class's static constructor. */
  a_boolean	is_static_initonly_field;
			/* TRUE if the C++/CLI initonly field that was found
			   during the tree walk is a static member. */
#endif  /* MICROSOFT_EXTENSIONS_ALLOWED */
  struct a_seq_pt_var_entry
		*seq_pt_var_list;
			/* A list of variables referenced in the expression
			   and the sequence point information associated
			   with each variable. */
  a_boolean	end_of_full_expr;
			/* The check is at the end of the full expression. */
} an_expr_or_stmt_traversal_block;

extern void clear_expr_or_stmt_traversal_block(
                                   an_expr_or_stmt_traversal_block_ptr tblock);

extern void traverse_constant(a_constant_ptr                      constant,
                              an_expr_or_stmt_traversal_block_ptr tblock);

extern a_boolean constant_is_recursive(a_constant  *cp);

extern void traverse_dynamic_init(a_dynamic_init_ptr                  dip,
                                  an_expr_or_stmt_traversal_block_ptr tblock);

extern void traverse_variable_init(
                                   a_variable_ptr                      var,
                                   an_expr_or_stmt_traversal_block_ptr tblock);

extern void traverse_expr_list(an_expr_node_ptr                    expr_list,
                               an_expr_or_stmt_traversal_block_ptr tblock);

extern void traverse_expr(an_expr_node_ptr                    expr,
                          an_expr_or_stmt_traversal_block_ptr tblock);

extern void traverse_statement(a_statement_ptr                     statement,
                               an_expr_or_stmt_traversal_block_ptr tblock);

extern void traverse_statement_list(
                            a_statement_ptr                     statement_list,
                            an_expr_or_stmt_traversal_block_ptr tblock);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef IL_WALK_H */

