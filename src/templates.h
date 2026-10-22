/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

templates.h -- Declarations relating to templates.c (template support)

*/

/* Avoid including these declarations more than once: */
#ifndef TEMPLATES_H
#define TEMPLATES_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */
#if !STANDALONE_UTILITY_PROGRAM
#ifndef DECLS_H
#include "decls.h"
#endif /* ifndef DECLS_H */
#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Flags used to specify options to the template declaration processing routines.
*/
typedef int a_template_decl_options_set;

#define TDO_NO_OPTIONS		0x0
#define TDO_EXTERN		0x1
			/* TRUE if the "extern" keyword was specified
			   before the "template" keyword. */
#define TDO_INLINE		0x2
			/* TRUE if the "inline" keyword was specified
			   before the "template" keyword. */
#define TDO_GENERIC		0x4
			/* TRUE if this is a C++/CLI generic declaration. */

#if !STANDALONE_UTILITY_PROGRAM
/*
Declaration of the template parameter state entry, which is defined in
templates.c.
*/
typedef struct a_tmpl_param_state *a_tmpl_param_state_ptr;

/*
Structure used to pass information about the current template declaration
between the routines used to implement the processing of template
declarations.
*/
typedef struct a_tmpl_decl_state {
  a_decl_parse_state
		*decl_parse;
			/* General declaration information. */
  a_symbol_ptr
		sym;
			/* A symbol representing the template being
			   declared. */
  a_boolean	is_template_friend;
			/* TRUE if this is a friend declaration. */
  a_boolean	is_member_decl;
			/* TRUE if this declaration appeared in a class
			   scope. */
  a_boolean	is_specialization;
			/* TRUE if the declaration is a specialization.
			   A specialization contains one or more template
			   parameter clauses with empty parameter lists. */
  a_boolean	is_partial_specialization;
			/* TRUE if the declaration is a partial specialization
			   of a class template.  Only set during the initial
			   declaration of a partial specialization. */
  a_boolean	is_full_specialization;
			/* TRUE if the declaration is a full specialization
			   of a template entity.  A full specialization
			   declares a real function or class (i.e., not a
			   template).  In a full specialization all the
			   template parameter clauses contain empty parameter
			   lists (i.e., "template <>"). */
  a_boolean	defines_something;
			/* TRUE if the declaration is a definition. */
  a_boolean	is_deleted;
			/* TRUE for a function template declaration that
			   was defined as "= delete". */
  a_boolean	in_prototype_instantiation;
			/* TRUE if the declaration is being processed as
			   part of the prototype instantiation of an
			   enclosing class template. */
  a_boolean	in_generic_definition;
			/* TRUE if the declaration is being processed as
			   part of the definition of a C++/CLI generic. */
  a_boolean	decl_scope_err;
			/* TRUE if the template declaration is invalid in the
			   current scope. */
  a_boolean	nesting_depth_err;
			/* TRUE if a nesting depth error was detected. */
  a_boolean	export_present;
			/* TRUE if the "export" keyword was used on the
			   declaration. */
  a_boolean	partial_spec_outside_of_class;
			/* TRUE if this is the declaration of a partial
			   specialization that is a member of a class
			   or class template, and the declaration appears
			   outside of the parent class. */
  a_boolean	partial_spec_outside_of_class_template;
			/* TRUE if this is the declaration of a partial
			   specialization that is a member of a class
			   template, and the declaration appears outside of
			   the parent class. */
  a_boolean	out_of_class_instantiation;
			/* TRUE if this is the instantiation of an out-of-class
			   declaration of a partial specialization. */
  a_boolean	is_template_template_param;
			/* TRUE when scanning the template parameter clauses
			   of a template template declaration. */
  a_boolean	is_template_template_param_rescan;
			/* TRUE when rescanning a template template parameter
			   whose declaration depends on other template
			   parameters. */
  a_boolean	is_variadic;
			/* TRUE if any of the template parameters are
			   parameter packs.  Also TRUE in GNU mode when
			   GNU variadic operators, such as __bases, are
			   enabled. */
  a_boolean	has_variadic_template_params;
			/* TRUE if this is an actual variadic template and
			   not simply treated as variadic in GNU mode (see
			   is_variadic above. */
  a_boolean	is_generic;
			/* TRUE if this a C++/CLI generic declaration. */
  a_boolean	is_delegate;
			/* TRUE if this is a C++/CLI delegate. */
  a_boolean	is_lambda;
			/* TRUE if this is a lambda (i.e., a C++14-style
			   generic lambda). */
  a_boolean	generic_constraints_pending;
			/* TRUE if this is a generic generated from metadata
			   declared with an indication that constraints will
			   be specified on a later declaration. */
  a_boolean	friend_depth_known;
			/* TRUE if this is a template friend template class
			   declaration whose nesting depth has been
			   determined via global qualification or by looking
			   up the template. */
  a_boolean	is_alias_redecl;
			/* TRUE if this is a redeclaration of an alias
			   template. */
  a_boolean	is_var_templ_initial_decl;
			/* TRUE if this is the initial declaration of
			   a variable template. */
  a_boolean	is_enum;
			/* TRUE if this is an enum template declaration. */
  a_boolean	caching_tokens;
			/* TRUE if we are currently doing background
			   caching of tokens for this declaration. */
  a_boolean	has_template_param_constraint;
			/* TRUE if one of the template parameters has a type
			   constraint. */
  a_boolean	is_pack_element;
			/* TRUE if a nontype template parameter	is an element
			   of a pack expansion. */
  a_boolean	is_pack_expansion;
			/* TRUE if a nontype template parameter was declared
			   using an enclosing pack expansion. */
  a_source_position
		export_position;
			/* If export_present is TRUE, the position of the
			   export keyword. */
  a_source_position
		other_decl_pos;
			/* For a redeclaration of a function template, this
			   is the declaration position of the symbol
			   that was found. */
  a_token_sequence_number
		starting_token_sequence_number;
			/* The token sequence number of the first token of
			   the template declaration. */
  a_token_sequence_number
		last_token_sequence_number_of_params;
			/* The token sequence number of the last token of
			   the template parameter clauses. */
  an_access_specifier
		access;
			/* When the declaration appears in a class scope,
			   contains the current access. */
  a_template_nesting_depth
		nesting_depth;
			/* Nesting depth of this template declaration (i.e.,
			   the number of enclosing template scopes.  The
			   outermost template declaration has a nesting
			   depth of 1. */
  a_template_nesting_depth
		friend_depth;
			/* If a particular nesting depth should be used for
			   this declaration, friend_depth is set to that
			   depth; otherwise it is set to zero. */
  a_template_nesting_depth
		specialization_levels;
			/* The number of template parameter clauses that
			   do not have a template parameter list (i.e.,
			   "template <>"). */
  a_token_kind	*final_token_ptr;
			/* Pointer to a token kind indicating whether the
			   final token of the declaration is expected to be
			   a semicolon or a right brace. */
  a_template_decl_info_ptr
		decl_info;
			/* Points to the template declaration information
			   associated with the innermost template declaration
			   scope.  Contains NULL for full specializations. */
  a_scope_depth	orig_decl_level;
			/* The scope depth of the scope containing the
			   template declaration. */
  a_scope_depth	effective_decl_level;
			/* The effective declaration scope of the
			   template declaration.  This is normally the
			   to the scope that contains the template
			   declaration, but is the nearest namespace scope
			   for friend declarations. */
  a_scope_depth	err_decl_level;
			/* The scope depth to be used for error recovery
			   purposes when a template is declared in an invalid
			   scope. */
  unsigned long	number_of_template_decl_scopes;
			/* The number of template declaration scopes pushed
			   while processing this template declaration. */
  unsigned long	number_of_template_param_clauses;
			/* The number of template parameter clauses (including
			   ones with empty parameter lists in specialization
			   declarations) in the current template
                           declaration. */
  a_scope_ptr	enclosing_scope;
			/* Points to the scope entry for the scope that
			   contains the template declaration. */
  a_type_ptr	class_declared_in;
			/* When the template definition appears in a class
			   scope, this points to the class type of the
			   enclosing class, otherwise contains NULL. */
  a_shared_token_cache
		param_list_cache;
			/* Token cache containing the template parameter
			   list(s). */
  a_token_sequence_number
		first_decl_cache_tsn;
			/* The token sequence number of the first token that
			   should be included in the decl_token_cache. */
  a_shared_token_cache
		decl_token_cache;
			/* Token cache containing the template declaration
			   (the portion that follows the template parameter
			   list(s)). */
  a_boolean	decl_token_cache_used;
			/* TRUE if the declaration token cache was saved as
			   part of the template that was declared. */
  a_pending_pragma_list
		*pragmas_bound_to_template;
			/* A list of next-construct pragmas that appeared
			   before this template declaration. */
  a_template_ptr
		il_template_entry;
			/* Pointer to the IL template entry created for this
			   template declaration, or NULL if no entry has been
			   created. */
  a_decl_pos_block
		decl_pos_block;
			/* Source range information for the template
			   declaration. */
  a_symbol_ptr	new_alias_symbol;
			/* When is_alias_redecl is TRUE, this points to the
			   symbol created for the redeclaration. */
  a_symbol_ptr	prototype_scope_symbols;
			/* For a function template declaration, points to the
			   list of prototype scope symbols from the
			   func_info_block. */
  a_param_id_ptr
		param_id_list;
			/* For a function template declaration, points to the
			   list of parameter ID entries from the
			   func_info_block. */
  a_symbol_ptr	bad_partial_spec_parent_class_sym;
			/* If the declaration is for an out-of-class
			   partial specialization, and the specialization
			   is invalid because of an incorrect parent class,
			   this points to the symbol of the parent class
			   (to be used for error reporting purposes). */
  a_symbol_ptr	out_of_class_prototype_sym;
			/* For an out-of-class instantiation of a partial
			   specialization, this points to the symbol created
			   for the partial specialization in the primary
			   template. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		definition_range;
			/* Source range information for the template
			   definition (if any). */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_template_decl_ptr
		template_decl;
			/* IL representation of the template parameterization
			   of the entity being declared. */
  a_token_sequence_number
		requires_tsn;
			/* The token sequence number of the requires clause in
			   the template head or NO_TOKEN_SEQUENCE_NUMBER if
			   there is none. */
  a_tmpl_param_state_ptr
		enclosing_param;
			/* If this is the state for a template template
			   parameter, this points to the parameter state of
			   the template template parameter. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  uint32_t	num_parameters;
			/* The number of parameters for the generic being
			   declared. */
  a_generic_param_seq_number
		enclosing_generic_params;
			/* The number of generic parameters declared in
			   generic classes that enclose the current
			   declaration.  See enclosing_generic_parameters
			   and a_generic_param_seq_number for more details. */
  a_cli_class_type_kind
		cli_class_type_kind;
			/* The class type kind of this template.  In
			   non-C++/CLI modes, it is always cctk_standard.
			   In C++/CLI mode, other kinds of classes
			   (e.g., "ref classes") are possible. */
  an_assembly_visibility
		cli_visibility;
			/* The C++/CLI visibility, if any, specified for this
			   declaration, or av_none if none was specified. */
  a_source_position
		cli_visibility_pos;
			/* The source position if a cli_visibility was
			   specified, or null_source_position. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} a_tmpl_decl_state;


/*
TRUE if we are in a prototype instantiation or C++/CLI generic definition.
*/
#define in_prototype_instantiation_or_cli_generic(decl_state)		\
  ((decl_state)->in_prototype_instantiation ||				\
   (decl_state)->in_generic_definition)

#endif /* !STANDALONE_UTILITY_PROGRAM */


/*
Flags used to specify options to equiv_template_arg_lists.
*/
typedef int an_equiv_templ_arg_options_set;

#define ETA_NO_OPTIONS			0x0
#define ETA_ERROR_MATCHES_ANYTHING	0x1
			/* TRUE if an error type or constant will match
			   anything (this is used for compatibility checking
			   instead of equivalence checking). */
#define ETA_IS_NONREAL_MEMBER		0x2
			/* TRUE if the template is a member of a nonreal
			   class and has no template parameter list.  In such
			   cases, a NULL argument list, and argument lists of
			   different lengths are permitted.  */
#define ETA_MS_IGNORE_QUALIFIERS	0x4
			/* TRUE if, in Microsoft bugs mode, top level
			   qualifiers should be ignored when comparing two
			   argument lists. */
#define ETA_IS_PROTOTYPE		0x8
			/* TRUE if the first template argument list is
			   the argument list for a prototype instantiation of
			   a class template or a prototype instantiation of
			   a partial specialization. */
#define ETA_EXACT_MATCH_REQUIRED	0x10
			/* TRUE if the values of the templates arguments must
			   match exactly (e.g., point to the same type or
			   constant).  FALSE if only equivalence is
			   required. */
#define ETA_IS_VARIADIC			0x20
			/* TRUE if the template is variadic (so the number
			   of arguments is not expected to be uniform). */
#define ETA_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED	0x40
			/* TRUE if, when dependent decltypes appear in a type,
			   they must appear in both entities being compared
			   and the expressions must match. */
#define ETA_ALLOW_EQUIV_NESTING_DEPTHS 0x80
			/* By default, when comparing template parameter types,
			   their nesting depth must match exactly.  When this
			   flag is specified, equiv_nesting_depths is used to
			   compare nesting depths instead. */
#define ETA_IS_PARTIAL_SPECIALIZATION_CHECK 0x400
			/* TRUE when comparing a template argument list with a
			   substituted list as part of the partial
			   specialization matching process. */

/*
Flags used to specify options to equiv_template_param_lists.
*/
typedef int an_equiv_templ_param_options_set;

#define ETP_NO_OPTIONS			0x0
#define ETP_BAD_PARAM_TYPE_OKAY		0x1
			/* TRUE if a nontype parameter initially declared with
			   one type may be redeclared later with a different
			   type.  This is used to emulate Microsoft and
			   g++ bugs. */
#define ETP_NESTING_DEPTH_MISMATCH_OKAY	0x2
			/* TRUE if the comparison of nesting depths should be
			   suppressed. */
#define ETP_TEMPLATE_TEMPLATE_PARAM_MATCH 0x4
			/* TRUE if the comparison is being done to determine
			   whether a template matches the parameter list of
			   a template template parameter.  Some flexibility
			   is provided in such cases (e.g., for matching
			   parameter packs). */
#define ETP_DEFAULT_ARGUMENT_MATCH_REQUIRED 0x8
			/* TRUE if default arguments need to match. */
#define ETP_DEPENDENT_PARAMS_DONT_MATCH 0x10
			/* TRUE if dependent parameters should be considered to
			   be different.  (Used in GCC/Clang mode when
			   comparing instantiated member function template
			   declarations.) */

/*
Flags used to specify options to set_instance_required and
update_instantiation_required_flag.
*/
typedef int a_set_instance_required_options_set;
#define SIR_NONE		0x0
#define SIR_DEFER_INLINE	0x1
			/* TRUE if the instantiation of an inline function
			   should be deferred and not done immediately upon
			   the call. */
#define SIR_CLEAR_VALUE		0x2
			/* Normally the value specified on a call is merged
			   with the earlier value.  This flag forces the
			   instance required flag to be cleared. */

#define SIR_INLINE_DEFINITION_NEEDED 0x4
			/* This is used in certain C++-generating back end
			   configurations to indicate that an instantiation
			   is required because the C++-generating back end
			   requires a definition in order for correct code
			   to be generated. */
#define SIR_CONSTANT_CONTEXT 0x8
			/* TRUE if the instantiation is required in a constant
			   constant.  This forces the immediate instantiation
			   of constexpr functions when possible. */

/*
Structure used to represent the information found an in export information
file.
*/
typedef struct an_export_info_file *an_export_info_file_ptr;
typedef struct an_export_info_file {
  a_directory_name_entry_ptr
		dir_name_entry;
			/* The directory name entry for the export template
			   search directory associated with this file. */
  char		*file_name;
			/* The name of the export information file. */
  a_directory_name_entry_ptr
		incl_search_path;
			/* The include search path to be used. */
  a_directory_name_entry_ptr
		end_incl_search_path;
			/* The end of the include search path. */
  a_directory_name_entry_ptr
		sys_incl_search_path;
			/* The system include search path to be used (i.e.,
			   for includes of the form <...>). */
} an_export_info_file;

/*
Structure used to keep track of the class template partial specializations
or function templates that match a given instance.
*/
typedef struct a_partial_order_candidate *a_partial_order_candidate_ptr;
typedef struct a_partial_order_candidate {
  a_partial_order_candidate_ptr
		next;
			/* Next entry in the list. */
  a_symbol_ptr	symbol;
			/* Pointer to the symbol associated with a
			   given partial specialization or function
			   template. */
  a_template_arg_ptr
		template_arg_list;
			/* Template argument list to be used if this partial
			   specialization or function template is used to
			   generate the instance. */
} a_partial_order_candidate;


extern void add_to_partial_order_candidates_list(
			a_partial_order_candidate_ptr	*psc_list,
			a_symbol_ptr			new_sym,
			a_template_arg_ptr		templ_arg_list);

extern void select_best_partial_order_candidate(
                        a_partial_order_candidate_ptr  psc_list,
                        a_symbol_ptr                   instance_sym,
                        a_symbol_ptr                   *best_sym,
                        a_template_arg_ptr             *best_arg_list,
                        a_boolean                      *p_ambiguous,
                        a_boolean                      no_diagnostics = FALSE);

extern
a_template_cache_ptr cache_for_template(a_template_symbol_supplement_ptr tssp);

extern a_template_cache_ptr decl_cache_for_template(a_symbol_ptr  sym);

extern void init_templ_decl_state(a_tmpl_decl_state_ptr	tdsp,
                                  a_decl_parse_state    *dps);

extern a_template_arg_ptr templ_arg_list_for_class(a_type_ptr class_type);

extern
a_template_arg_ptr templ_arg_list_for_variable(a_variable_ptr	var_ptr);

extern void get_substitution_pairs_for_template_class(
                                           a_type_ptr            class_type,
                                           a_template_param_ptr  *p_t_params,
                                           a_template_arg_ptr    *p_t_args);

extern void get_all_class_subst_pairs(a_type_ptr           class_type,
                                      a_subst_pairs_array  *p_array);

extern a_subst_pairs_array get_current_subst_pairs(void);

#if GENERATE_SOURCE_SEQUENCE_LISTS
extern a_src_seq_secondary_decl_ptr
                            secondary_src_seq_for_template(a_template_ptr  tp);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern a_symbol_ptr primary_template_of(a_symbol_ptr sym);

extern a_symbol_ptr template_for_instance(a_symbol_ptr sym);

extern a_boolean not_needed_or_will_be_instantiated(a_symbol_ptr	sym);

extern a_boolean rout_is_inline_template_function(a_routine_ptr rout,
                                                  a_boolean     in_class);

extern a_boolean template_arg_list_is_dependent(
					a_template_arg_ptr	tap);

extern a_boolean template_arg_list_involves_error_entity(
                                        a_template_arg_ptr	tap);

extern an_expr_node_ptr scan_type_constraint(
                                        a_symbol_ptr  concept_templ,
                                        a_boolean     for_requirement = FALSE);


inline a_template_decl_info_ptr templ_decl_info_of(a_symbol_ptr  template_sym)
/*
Return the template declaration information associated with the given template.
*/
{
  a_template_decl_info_ptr  decl_info;
  a_template_symbol_supplement_ptr
                            tssp = template_sym->variant.template_info;

  if (symbol_is(template_sym, sk_function_template)) {
    decl_info = tssp->variant.function.decl_cache->decl_info;
  } else if (symbol_is(template_sym, sk_variable_template)) {
    decl_info = tssp->variant.variable.decl_cache->decl_info;
  } else {
    decl_info = tssp->cache->decl_info;
  }  /* if */
  return decl_info;
}  /* templ_decl_info_of */


inline a_template_decl_ptr templ_decl_of(a_symbol_ptr  template_sym)
/*
Return the template declaration for the given template.
*/
{
  a_template_decl_ptr       tdp;
  a_template_decl_info_ptr  decl_info = templ_decl_info_of(template_sym);

  if (decl_info != NULL) {
    tdp = decl_info->template_decl;
  } else {
    /* Use the IL entry when decl_info has not been set yet. */
    tdp = template_sym->variant.template_info->il_template_entry
                                             ->template_decl;
  }  /* if */
  return tdp;
}  /* templ_decl_of */


inline a_template_param_ptr templ_params_of(a_symbol_ptr  template_sym)
/*
Return the list of template parameters for the given template.
*/
{
  return templ_decl_info_of(template_sym)->parameters;
}  /* templ_params_of */


inline a_template_param_ptr templ_params_of(a_template_ptr template_ptr)
/*
Return the list of template parameters for the given template.
*/
{
  return templ_params_of(symbol_for(template_ptr));
}  /* templ_params_of */


extern
a_boolean template_param_constraint_satisfied(
                                            a_type_ptr            param_type,
                                            a_type_ptr            arg_type,
                                            a_template_arg_ptr    arg_list,
                                            a_template_param_ptr  param_list,
                                            a_type_ptr            parent_type,
                                            a_source_position     *diag_pos);

extern a_boolean check_template_constraints(a_symbol_ptr        template_sym,
                                            a_template_arg_ptr  args,
                                            a_boolean           diagnose);

extern a_symbol_ptr create_template_param_symbol(a_symbol_kind    kind,
                                                 a_symbol_locator *locator,
                                                 a_boolean        is_unnamed,
                                                 a_boolean        is_rescan);

extern a_symbol_kind determine_template_param_kind(a_symbol_ptr  *p_concept);

extern a_template_param_ptr decl_type_template_param(
                                    a_template_param_list_pos param_pos,
                                    a_symbol_locator          *loc,
                                    a_boolean                 is_named,
                                    a_boolean                 is_pack,
                                    an_expr_node_ptr          constraint,
                                    a_tmpl_decl_state         *decl_state,
                                    a_decl_pos_block          *decl_pos_block);

extern a_template_param_ptr make_nontype_template_param(
                                  a_template_nesting_depth  depth,
                                  a_template_param_list_pos position,
                                  a_boolean                 is_unnamed,
                                  a_boolean                 is_pack,
                                  a_boolean                 is_pack_element,
                                  a_boolean                 is_non_initial,
                                  a_boolean                 is_pack_expansion,
                                  a_symbol_locator          *loc,
                                  a_type_ptr                param_type,
                                  a_tmpl_decl_state_ptr     decl_state);

extern a_symbol_ptr create_template_for_template_template_param(
                      a_template_decl_ptr       decl,
                      a_symbol_locator          *locator,
                      a_template_nesting_depth  depth,
                      a_template_param_list_pos position,
                      a_boolean                 is_named,
                      a_boolean                 is_rescan,
                      a_boolean                 is_pack,
                      a_boolean                 is_variadic,
                      a_boolean                 has_variadic_template_params,
                      a_boolean                 has_template_param_constraint);

extern void template_or_specialization_declaration_full(
                                           a_tmpl_decl_state   *decl_state,
                                           a_boolean           is_generic,
                                           a_decl_parse_state  *orig_dps);

extern void explicit_instantiation(
                              a_decl_parse_state_ptr      dps,
                              a_template_decl_options_set options,
                              a_source_position_ptr       directive_start_pos);

extern a_boolean are_template_args_lexically_identical(
                                             a_template_arg_ptr list1,
                                             a_template_arg_ptr list2,
                                             long               num_args = -1);

extern a_symbol_ptr find_template_instantiation(
                                             a_symbol_ptr       template_sym,
                                             a_template_arg_ptr template_args);

extern a_symbol_ptr find_partial_template_specialization(
                                             a_symbol_ptr       template_sym,
                                             a_template_arg_ptr template_args);

extern a_symbol_ptr find_template_class(
			     a_symbol_ptr        class_template_sym,
                             a_template_arg_ptr  *new_list,
			     a_boolean	         any_prototype_allowed,
			     a_symbol_ptr        specific_prototype_allowed,
			     a_boolean		 instantiate_nonreal,
			     a_boolean		 do_not_create,
			     a_boolean		 in_substitution);

extern
a_boolean adjust_templ_arg_list_for_template(a_symbol_ptr          templ,  
                                             a_template_arg_ptr    *arg_list,
                                             a_template_param_ptr  param_list);

extern a_symbol_ptr find_class_template_instance(
                                              a_symbol_ptr        class_templ,
                                              a_template_arg_ptr  *arg_list);

extern a_symbol_ptr find_template_variable(
				a_symbol_ptr		template_sym,
				a_template_arg_ptr	*new_templ_arg_list,
				a_boolean		prototype_allowed,
				a_boolean		is_use,
				a_boolean		diagnose);

extern a_namespace_ptr determine_referencing_namespace(void);

extern void make_template_decl_cache(
				a_tmpl_decl_state_ptr	decl_state,
				a_token_sequence_number	last_tsn,
				a_boolean		include_last_token);

extern void set_template_arg_to_error(a_template_arg_ptr	tap);

extern a_template_arg_ptr create_initial_template_arg_list(
		a_template_param_ptr		templ_param_list,
		a_template_arg_ptr		partial_arg_list,
		a_boolean			is_templ_templ_param_check,
		a_source_position		*source_pos);

extern a_template_arg_ptr get_template_arg_for_coordinates(
		        a_template_param_coordinate_ptr	coordinates,
			a_ctws_options_set		options,
			a_template_arg_ptr		*templ_arg_list,
			a_template_param_ptr		templ_param_list);

extern a_template_param_coordinate_ptr coordinates_of_template_param_symbol(
                                                   a_symbol_ptr sym);

extern a_template_param_coordinate_ptr coordinates_of_template_arg(
						a_template_arg_ptr	tap);

extern a_template_param_coordinate_ptr coordinates_of_template_param(
                                                   a_template_param_ptr tpp);

extern a_type_ptr rescan_template_constant_parameter
                                     (a_symbol_ptr	   template_sym,
                                      a_symbol_ptr	   param_sym,
			              a_template_param_ptr param_ptr,
				      a_template_arg_ptr   arg_list,
                                      a_boolean		   do_default_arg,
                                      a_constant_ptr       *constant);

extern a_template_ptr rescan_template_template_parameter(
				a_symbol_ptr		template_sym,
				a_template_param_ptr	param_ptr,
				a_template_arg_ptr	arg_list);

extern a_type_ptr rescan_template_type_default_arg
                                     (a_symbol_ptr	   template_sym,
			              a_template_param_ptr param_ptr,
				      a_template_arg_ptr   arg_list);

extern a_template_ptr rescan_template_template_default_arg
                                     (a_symbol_ptr	   template_sym,
			              a_template_param_ptr param_ptr,
				      a_template_arg_ptr   arg_list);

extern void delayed_scan_of_template_param_default_arg(
					a_symbol_ptr		template_sym,
					a_template_param_ptr	tpp,
					a_template_param_ptr	param_list);

/*
Bit vector used to pass flags into matches_template_type.
*/
typedef unsigned int an_mtt_flag_set;

#define MTT_NO_FLAGS 0x00
#define MTT_ALLOW_INEXACT_DEDUCTION 0x01
			/* TRUE when a conversion from Derived<T>
			   to Base<T> may be done if needed, and qualifiers
			   under an array type can be added.  Qualifiers
			   on top-level types are handled outside of the
			   deduction process.  When TRUE, this also permits
			   noexcept specifiers to differ (when they are part
			   of the function type). */
#define MTT_UNKNOWN_THIS_CLASS_TYPE 0x02
			/* TRUE if the this class type may not
			   be known yet.  When this flag is set, a
			   NULL this class type is ignored (i.e., no attempt
                           is made to match it with the template type). */
#define MTT_IS_CONVERSION_TEMPLATE 0x04
			/* TRUE if argument deduction is being done in the
			   context of a conversion template return type. */
#define MTT_ALLOW_ADDED_QUALIFIERS 0x08
			/* TRUE the template type can be more cv-qualified
			   than the other type. */
#define MTT_ALLOW_SPECIAL_RVALUE_REF_DEDUCTION 0x10
			/* TRUE if a T&& template type can match up with an
			   X& actual type resulting in an X& deduced type.
			   This is used when deducing arguments from a complete
			   function type. */
#define MTT_REVERSE_BASE_DERIVED_THIS_TEST 0x20
			/* Normally when routines types are being compared,
			   the template type can be a base of the other
			   type.  This flag allows the other type to be a
			   base of the template type. */
#define MTT_ALLOW_STRICTER_NOEXCEPT 0x40
			/* TRUE when the template routine type may have a more
			   restrictive noexcept specifier than the other type.
			   Use for cases like:
                             template<class T> T f() noexcept;
                             int (*fp)() noexcept(false) = &f;
			   Note that MTT_ALLOW_INEXACT_DEDUCTION permits the
			   relaxation in the other direction. */
#define MTT_TEMPL_TEMPL_MATCH 0x80
			/* TRUE when doing partial ordering as part of
			   C++17-style template template argument matching. */
#define MTT_PROVISIONAL_VALUE 0x100
			/* TRUE when doing deduction based on the type of an
			   array bound.  The value is only deduced from the
			   bound if not otherwise deduced. */
#define MTT_PARTIAL_SPEC 0x200
			/* TRUE when doing deduction for the purpose of
			   matching a partial specialization. */
#define MTT_NESTED_TYPE_MATCH 0x400
			/* TRUE when matching a type in a template
			   argument list. */
#define MTT_IS_PACK 0x800
			/* TRUE when matching a type for a parameter pack. */
#define MTT_NO_PACK_DEDUCTION 0x1000
			/* TRUE when packs shouldn't be matched. */

extern a_boolean matches_template_type_with_qualification_conversion(
				a_type_ptr           type,
                                a_type_ptr           templ_type,
                                a_template_arg_ptr   *templ_arg_list,
				a_template_param_ptr templ_param_list,
				an_mtt_flag_set      flags);

extern
a_boolean matches_template_type(a_type_ptr           type,
                                a_type_ptr           templ_type,
                                a_template_arg_ptr   *templ_arg_list,
                                a_template_param_ptr templ_param_list,
				an_mtt_flag_set      flags);

extern
a_boolean tentatively_matches_template_type(
			       a_type_ptr           type,
		  	       a_type_ptr           templ_type,
                               a_template_param_ptr templ_param_list,
                               a_template_arg_ptr   templ_arg_list);

extern
a_boolean matches_template_array_bound(a_targ_size_t        elements,
                                       a_constant_ptr       templ_constant,
                                       a_template_arg_ptr   *templ_arg_list,
                                       a_template_param_ptr templ_param_list,
                                       an_mtt_flag_set      flags);

extern a_type_ptr substitute_template_arguments(
			a_symbol_ptr		templ_sym,
			a_template_arg_ptr	templ_arg_list,
			a_template_arg_ptr	*new_arg_list,
			a_template_param_ptr	templ_param_list,
			a_ctws_options_set	ctws_options);

extern a_type_ptr find_substituted_type(
			a_symbol_ptr				template_sym,
			a_template_symbol_supplement_ptr	tssp,
			a_template_arg_ptr			templ_arg_list,
			a_ctws_options_set			options,
			a_type_ptr				type);

extern a_type_ptr wrapup_function_template_argument_deduction(
				a_template_arg_ptr   *templ_arg_list,
				a_symbol_ptr         rout_templ_sym,
				a_template_param_ptr templ_param_list,
				a_ctws_options_set   ctws_options,
				uint32_t	     param_count);

extern a_symbol_ptr find_template_function(
			a_symbol_ptr		templ_sym,
                        a_template_arg_ptr	*new_list,
			a_boolean		explicit_arg_list_present,
                        a_source_position	*source_pos);

extern a_boolean is_match_for_function_template(
				a_symbol_ptr		templ_sym,
				a_type_ptr		curr_type,
				a_template_arg_ptr	*templ_arg_list,
				a_symbol_ptr		*instance_sym,
				a_template_param_ptr	templ_param_list,
				a_template_arg_ptr	explicit_arg_list,
				a_boolean		is_decl_context,
				a_boolean		ignore_noexcept);

extern a_symbol_ptr matching_template_function(
				a_symbol_ptr        templ_sym,
                                a_type_ptr          curr_type,
				a_template_arg_ptr  explicit_arg_list,
				a_boolean	    explicit_arg_list_present,
				a_boolean	    is_decl_context,
				a_boolean	    ignore_noexcept,
				a_boolean	    in_class_specialization,
				a_boolean	    *is_new_template_instance);

extern
a_boolean has_matching_template_function(a_symbol_ptr       templ_sym,
                                         a_type_ptr         curr_type,
					 a_template_arg_ptr explicit_arg_list,
                                         a_boolean          is_decl_context,
                                         a_boolean          ignore_noexcept);

extern a_type_ptr explicit_arg_list_identifies_specialization(
				a_symbol_ptr		template_sym,
				a_template_arg_ptr	templ_arg_list,
				a_template_arg_ptr	*new_arg_list);

extern
a_boolean has_matching_template_instance(
				a_symbol_ptr		sym,
                                a_type_ptr		type,
				a_template_arg_ptr	explicit_arg_list);

#if !STANDALONE_UTILITY_PROGRAM
extern a_symbol_ptr find_matching_template_instance(
		a_symbol_ptr			sym,
		a_decl_parse_state              *dps,
		a_symbol_locator		*loc,
		a_boolean			in_class_specialization,
		a_boolean			prefer_template,
		a_boolean			check_only,
		a_template_nesting_depth	nesting_depth,
		an_error_severity		severity_if_not_found,
		a_boolean        		*is_new_template_instance);
#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
Flags passed to control the behavior of compare_function_templates (which
computes the partial ordering of two function templates).
*/
#define CFT_NO_FLAGS  ((uint32_t)0x0)
#define CFT_ENTIRE_TYPE  ((uint32_t)0x1)
			/* TRUE if the partial ordering requires comparing the
			   entire type (including the return type). */
#define CFT_TEMPL_TEMPL_PARAM  ((uint32_t)0x2)
			/* TRUE if the ordering is for functions synthesized to
			   implement the C++17 template template parameter
			   matching rules (N4868 [temp.arg.template]/4). */
#define CFT_REVERSE_PARAMS_1  ((uint32_t)0x4)
#define CFT_REVERSE_PARAMS_2  ((uint32_t)0x8)
			/* TRUE if, respectively, the first or second function
			   template is a C++20 comparison candidate that is
			   being matched with reversed arguments. */

extern int compare_function_templates(
			a_symbol_ptr 		templ_sym1,
			a_symbol_ptr		templ_sym2,
			uint32_t		cft_flags,
			uint32_t		param_count);

extern void record_predeclared_template_function(
                                       a_symbol_ptr         templ_sym,
                                       a_symbol_ptr         rout_sym,
                                       a_template_param_ptr templ_param_list,
				       a_template_ptr	    il_template_entry);

extern void set_il_template_entry(
			a_tmpl_decl_state_ptr			decl_state,
			a_symbol_ptr				sym,
			a_template_symbol_supplement_ptr	tssp);

extern void find_member_function_template(
                                    a_symbol_ptr  rout_sym,
                                    a_symbol_ptr  corresp_prototype_tag_sym);

extern void find_variable_member_template(
			a_symbol_ptr		var_sym,
			a_symbol_ptr		corresp_prototype_tag_sym,
			a_token_sequence_number	token_sequence_number);

extern void find_inclass_field_initializer_for_instance(
				a_symbol_ptr	field_sym,
				a_symbol_ptr	corresp_prototype_tag_sym);

extern void find_inclass_sdm_initializer_for_instance(
                                      a_symbol_ptr  sdm_sym,
                                      a_symbol_ptr  corresp_prototype_tag_sym);

extern a_boolean is_transparent_alias_template(a_template_ptr  templ);

extern a_boolean has_default_template_arguments(a_template_ptr templ);

extern void find_enum_member(a_symbol_ptr		alias_sym,
                             a_type_ptr			parent_class,
                             a_token_sequence_number	token_sequence_number);

extern void check_for_uninstantiated_template_class(a_type_ptr  type);

extern a_type_ptr compute_meta_common_type(a_type_ptr  types[],
                                           int         n,
                                           a_boolean   *p_err);

extern a_type_ptr compute_meta_common_reference(a_type_ptr  types[],
                                                int         n,
                                                a_boolean   *p_err);

extern a_type_ptr compute_meta_invoke_result(a_type_ptr  fn_type,
                                             a_type_ptr  arg_types[],
                                             int         n,
                                             a_boolean   *p_err);

extern a_boolean compute_meta_library_predicate(a_const_char  *name,
                                                a_type_ptr    types[],
                                                int           n,
                                                a_boolean     *p_err);

extern
void complete_template_variable_type_is_needed(a_variable_ptr vp);

extern void f_instantiate_template_class(a_type_ptr  type,
                                         a_boolean   *p_error = NULL);

extern void instantiate_template_enum(a_type_ptr		enum_type);

extern void instantiate_template_enum_if_needed(a_type_ptr	enum_type);

extern void init_ctws_state(a_ctws_state_ptr	csp);

extern void free_list_of_variadic_param_info(a_variadic_param_info_ptr vpip);

extern a_template_arg_ptr copy_template_arg_list_with_substitution(
			a_symbol_ptr		template_sym,
			a_template_arg_ptr	arg_list_to_copy,
			a_template_param_ptr	param_list_for_copy,
			a_template_param_ptr	ttp_list_for_copy,
			a_template_arg_ptr	templ_arg_list,
			a_template_param_ptr	templ_param_list,
			a_source_position	*source_pos,
			a_ctws_options_set	options,
			a_boolean		*copy_error,
			a_ctws_state_ptr	ctws_state);

extern a_symbol_ptr copy_parent_type_with_substitution(
			a_symbol_ptr			sym,
			a_type_ptr			parent_type,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_boolean			is_type,
			a_type_ptr			*new_type,
			a_ctws_options_set		options,
			a_boolean			*copy_error,
			a_ctws_state_ptr		ctws_state);

extern a_template_ptr copy_template_with_substitution(
			a_template_ptr			templ,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_ctws_options_set		options,
			a_boolean			*copy_error,
			a_ctws_state_ptr		ctws_state);


extern a_symbol_ptr copy_template_variable_with_substitution(
			a_variable_ptr			var,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_ctws_options_set		options,
			a_boolean			*copy_error,
			a_ctws_state_ptr		ctws_state);

extern a_symbol_ptr copy_template_routine_with_substitution(
			a_routine_ptr			rp,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_ctws_options_set		options,
			a_boolean			*copy_error,
			a_ctws_state_ptr		ctws_state);

extern a_type_ptr copy_type_with_substitution(
			a_type_ptr			type,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_ctws_options_set		options,
			a_boolean			*copy_error,
			a_ctws_state_ptr		ctws_state);

extern
a_type_ptr type_after_substitutions(a_type_ptr                 type,
                                    a_subst_pairs_array const  &subst_pairs,
                                    a_source_position          *source_pos,
                                    a_ctws_options_set         options,
                                    a_boolean                  *copy_error,
                                    a_ctws_state_ptr           ctws_state);

extern
a_param_type_ptr param_types_after_substitutions(
                                    a_param_type_ptr           ptp_list,
                                    a_subst_pairs_array const  &subst_pairs,
                                    a_source_position          *source_pos,
                                    a_ctws_options_set         options,
                                    a_boolean                  *copy_error,
                                    a_ctws_state_ptr           ctws_state);

extern a_template_arg_ptr templ_args_after_substitutions(
                               a_symbol_ptr               template_sym,
                               a_template_arg_ptr         arg_list_to_copy,
                               a_template_param_ptr       param_list_for_copy,
                               a_template_param_ptr       ttp_list_for_copy,
                               a_subst_pairs_array const  &subst_pairs,
                               a_source_position          *source_pos,
                               a_ctws_options_set         options,
                               a_boolean                  *copy_error,
                               a_ctws_state_ptr           ctws_state);

extern a_boolean equiv_template_arg_lists(
				a_template_arg_ptr list1,
				a_template_arg_ptr list2,
				an_equiv_templ_arg_options_set	options);

extern a_boolean equiv_templates_and_arg_lists(
		a_symbol_ptr			template_sym_1,
		a_symbol_ptr			template_sym_2,
		a_source_correspondence_ptr	scp_1,
		a_source_correspondence_ptr	scp_2,
		a_template_arg_ptr		tap_1,
		a_template_arg_ptr		tap_2,
		an_equiv_templ_arg_options_set	eta_options,
		a_boolean			error_matches_anything,
		a_boolean			exact_templ_arg_match_required,
		a_boolean			exact_decltype_exprs_required,
		a_boolean			exact_nesting_depths_required);

extern a_boolean check_template_template_arg_compatibility(
				a_template_ptr		arg_template,
				a_template_ptr		param_template);

extern
a_boolean check_nontype_template_param_type(a_type_ptr         *p_type,
                                            a_boolean          from_auto,
                                            a_source_position  *pos);

extern a_boolean equiv_template_param_lists(
		a_template_param_ptr			old_list,
		a_template_param_ptr			new_list,
		a_boolean				issue_errors,
		an_equiv_templ_param_options_set	options,
		a_source_position			*error_pos,
		an_error_severity			error_severity);

extern a_boolean reconcile_template_param_lists(
			a_template_param_ptr  param_list,
		        a_tmpl_decl_state_ptr decl_state,
			a_symbol_ptr          class_sym,
			a_source_position     *error_pos,
			a_boolean	      default_allowed,
			a_boolean	      checking_parent_params,
			a_boolean	      allow_missing_member_constraint,
			an_error_severity     error_severity);

extern a_template_ptr skip_simple_alias_templates(a_template_ptr  templ);

/*
Flags used to specify options to equiv_templates and
equiv_templates_given_supplement.
*/
typedef int an_equiv_templates_options_set;

#define ET_NO_OPTIONS		0x0
#define ET_EXACT_MATCH_REQUIRED 0x1
			/* TRUE if, when comparing template template
			   parameters and nonreal templates, the template
			   pointers must match, not just the coordinates. */
#define ET_OLD_IS_PACK		0x2
			/* TRUE if the old list parameter is associated
			   with a template parameter that is a pack. */

extern a_boolean equiv_templates(a_template_ptr			templ1,
				 a_template_ptr			templ2,
				 an_equiv_templates_options_set	options);

extern void copy_exc_spec_from_prototype_template(
                                  an_exception_specification_ptr  esp,
                                  a_boolean                       *copy_error);

extern void instantiate_exception_spec_if_needed(a_symbol_ptr  sym);

extern
void proto_instantiate_exception_spec_redecl(a_tmpl_decl_state_ptr  decl_state,
                                             a_symbol_ptr           sym);

extern void instantiate_field_initializer_if_needed(a_field_ptr  field);

extern void prescan_function_template_default_arg_expr(
					a_param_type_ptr  ptp,
					unsigned long	  param_number);

extern void instantiate_default_argument(a_symbol_ptr		rout_sym,
					 a_param_type_ptr	param);

extern void default_arg_prototype_instantiation(
	a_symbol_ptr				template_sym,
	a_def_arg_expr_fixup_ptr		def_arg_list,
	a_symbol_ptr				prototype_scope_symbols,
        a_boolean                               update_declared_type);

extern a_template_arg_ptr create_prototype_arg_list(
			a_symbol_ptr		template_sym,
			a_template_param_ptr	templ_param_list,
			a_boolean		add_pack_descr);

extern a_boolean prototype_instantiation_should_be_done_for_function(
					a_symbol_ptr		template_sym);

extern void function_prototype_instantiation(
			a_symbol_ptr		template_sym);

#if ONE_INSTANTIATION_PER_OBJECT
extern void set_routine_instantiation_needed_bit_number(a_routine_ptr routine);
extern void set_variable_instantiation_needed_bit_number(
                                                      a_variable_ptr variable);
#endif /* ONE_INSTANTIATION_PER_OBJECT */

extern void instantiate_template_variable(a_template_instance_ptr  tip,
                                          a_boolean                is_new,
                                          a_boolean                is_use);

extern void scan_nested_deduction_guide_template(
                                       a_tmpl_decl_state_ptr  decl_state,
                                       a_type_ptr             parent_class,
                                       a_decl_pos_block_ptr   decl_pos_block);

extern a_template_parameter_ptr alloc_template_parameter_for_symbol(
                                               a_template_param_ptr param_sym);

extern a_symbol_ptr template_directive_or_declaration(
			a_token_kind			*final_token,
			a_template_decl_options_set	options,
			a_source_position_ptr		directive_start_pos);

extern a_boolean is_template_friend_decl(void);

void add_befriending_class_to_function_template(
                         a_template_symbol_supplement_ptr  tssp,
                         a_type_ptr                        class_declared_in);

#if !STANDALONE_UTILITY_PROGRAM

extern void prepare_to_reparse_func_template_declarator_with_auto_params(
                                    a_token_sequence_number  reparse_tsn,
                                    a_decl_parse_callback    *reparse_actions,
                                    a_func_info_block        *func_info,
                                    a_symbol_locator         *locator);

extern void reparse_abbr_func_template(a_decl_parse_state  *orig_dps,
			               a_token_kind        *final_token);

extern void scan_lambda_template_param_list(a_tmpl_decl_state   *templ_state,
                                            a_decl_parse_state  *dps);

extern
void set_up_generic_lambda_declarator_scan(a_decl_parse_state  *dps,
                                           a_tmpl_decl_state   *templ_state);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern void update_friend_info_for_specialization(a_type_ptr	class_type);

extern void add_befriending_class_to_class_template(
                          a_template_symbol_supplement_ptr  tssp,
                          a_type_ptr                        class_declared_in);

extern
void set_nested_template_class_symbol_info(a_symbol_ptr  sym,
                                           a_type_kind	 type_kind);

extern
void update_nested_template_class_symbol_info(a_symbol_ptr       sym,
                                              a_type_kind	 type_kind);

extern void finalize_deduced_return_type(a_routine_ptr      rp,
                                         a_source_position  *diag_pos);

extern
void set_instance_required(a_symbol_ptr				sym,
			   a_boolean				value,
			   a_set_instance_required_options_set	options);

extern void process_deferred_instantiation_requests(void);

extern void set_master_instance_for_new_canonical_routine(
					a_routine_ptr	primary_routine,
					a_routine_ptr	secondary_routine);

extern void set_master_instance_for_new_canonical_variable(
					a_variable_ptr	primary_variable,
					a_variable_ptr	secondary_variable);

extern void set_master_instance_information(void);

extern a_template_instance_ptr template_instance_for_symbol(a_symbol_ptr sym);

extern void additional_instantiation_wrapup_processing_needed(void);

extern void template_and_inline_function_processing_for_pch(void);

extern void template_and_inline_entity_wrapup(void);

extern void remove_unneeded_instantiations(void);

extern void record_cache_checksum(
	       a_template_symbol_supplement_ptr	tssp,
	       a_token_cache			*p_template_body_cache);

extern void add_to_inline_function_list(a_routine_ptr	rout_ptr);

extern void add_to_inline_variable_list(a_variable_ptr	var);

extern
a_type_ptr type_if_unknown_conversion_function_symbol(a_symbol_ptr	sym);

extern void check_specialization_scope(a_symbol_ptr	     sym,
				       a_source_position     *pos);

extern void begin_template_arg_list_traversal(
				a_template_param_ptr	templ_param_list,
				a_template_arg_ptr	templ_arg_list,
				a_template_param_ptr	*tpp,
				a_template_arg_ptr	*tap);

extern void advance_to_next_template_arg(
				a_template_param_ptr	*tpp,
				a_template_arg_ptr	*tap);

extern void begin_special_variadic_template_arg_list_traversal(
				a_template_param_ptr	templ_param_list,
				a_template_arg_ptr	templ_arg_list,
				a_template_param_ptr	*tpp,
				a_template_arg_ptr	*tap);

extern void special_variadic_advance_to_next_template_arg(
				a_template_param_ptr	*tpp,
				a_template_arg_ptr	*tap);

extern a_symbol_ptr variable_template_partial_specialization(
				a_symbol_ptr		orig_sym,
				a_tmpl_decl_state_ptr	decl_state,
				a_symbol_locator	*locator);

extern a_symbol_ptr create_variable_template_symbol(
                                  a_tmpl_decl_state_ptr    decl_state,
                                  a_symbol_locator         *locator,
                                  a_symbol_ptr             primary_sym);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean is_start_of_generic_decl(void);
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define is_start_of_generic_decl()  /*lint --e(506)*/FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void make_symbol_header_for_initializer_list(void);

extern void templates_one_time_init(void);

extern void templates_trans_unit_init(void);

extern void templates_init(void);

#if MAKE_FRONT_END_CALLABLE
extern void templates_cleanup(void);
#endif /* MAKE_FRONT_END_CALLABLE */

#if AUTOMATIC_TEMPLATE_INSTANTIATION
extern void wrapup_auto_instantiation_information(void);
extern void update_auto_instantiation_flags(void);
extern void update_inline_entity_flags(void);
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

extern void update_instantiation_flags_for_class(
                                 a_symbol_ptr          sym,
                                 a_pragma_kind         pragma_kind,
                                 a_source_position     *pos,
                                 a_boolean             is_pragma,
                                 a_boolean             top_level,
                                 a_boolean             is_dll_directive);

extern void instantiation_pragma(a_pending_pragma_ptr	ppp);

EXTERN_THREAD a_def_arg_expr_fixup_ptr
		curr_default_args /* = NULL */;
			/* Pointer to the default argument entries for
			   the function template being scanned. */

EXTERN_THREAD a_type_ptr
		type_of_unknown_templ_param_nontype /* = NULL */;
			/* A type used for template parameter nontype values
			   and expressions whose real type cannot be known. */

EXTERN_THREAD unsigned long
		defer_instantiations;
			/* Nonzero if nonclass instantiations should be
			   deferred.  This causes instantiations to be placed
			   on the deferred_instantiations list instead of being
			   processed immediately.  Instantiations are also
			   deferred when pending_class_definitions is
			   nonzero. */

EXTERN_THREAD a_symbol_list_entry_ptr
		exported_templates_list;
			/* List of exported templates whose definitions
			   were provided in this compilation.  This list
			   includes only functions and static data members
			   (i.e., not classes). */

EXTERN_THREAD a_decl_sequence_number
		class_instantiation_sequence_number;
			/* Count of the number of instantiations of
			   class templates that have been performed in the
			   current translation unit. */

#if ENSURE_LOWERED_TYPE_LIST_ORDERING

EXTERN_THREAD a_boolean
		local_type_used_as_template_type_argument;
			/* TRUE if a local type has been used as a template
			   argument in any translation unit of the
			   compilation. */

#endif /* ENSURE_LOWERED_TYPE_LIST_ORDERING */


inline void instantiate_template_class(a_type_ptr  tp,
                                       a_boolean   *p_subst_error = NULL)
/*
tp is a class type.  If it is incomplete, see if it is a template class in
need of instantiation and, if so, instantiate it.  If p_subst_error is non-NULL
and an error occurs during partial specialization selection, *p_subst_error is
set to TRUE.
*/
{
  if (is_incomplete_type(tp) && is_class_struct_union_type(tp)) {
    f_instantiate_template_class(tp, p_subst_error);
  }  /* if */
}  /* instantiate_template_class */

/*
Return TRUE if there have been any exported templates defined in the current
translation unit.
*/
#define any_exported_templates()					\
  (exported_templates_list != NULL)


extern a_boolean is_nontemplate_routine_from_exported_trans_unit(
						a_routine_ptr rout_ptr);

#if MICROSOFT_EXTENSIONS_ALLOWED

extern
a_type_ptr generic_param_if_generic_definition_argument(a_type_ptr	type);

extern a_boolean equivalent_generic_constraints_for_param_lists(
                              a_template_param_ptr      list1,
                              a_template_param_ptr      list2,
                              a_boolean                 issue_error,
                              a_boolean                 ignore_empty_gclist2,
                              a_generic_constraint_ptr  *p_mismatch_in_list1);

extern a_boolean type_satisfies_constraints_of_generic_def_arg_type(
                                                     a_type_ptr arg_type,
                                                     a_type_ptr gda_type);

void verify_generic_arg_list_satisfies_constraints(
				a_symbol_ptr		generic_sym,
				a_template_arg_ptr	generic_arg_list,
				a_source_position_ptr	list_start_pos);

extern void scan_cli_generic_class_definition_from_assembly_import(void);

extern a_symbol_ptr make_cli_array_type(a_type_ptr             element_type,
                                        a_host_large_unsigned  rank);

/*
Entry used to represent a C++/CLI generic constraint check that was
deferred to be performed later.
*/
typedef struct a_deferred_constraint_check {
  a_deferred_constraint_check_ptr
		next;
			/* The next entry on a list of entries, or NULL for the
			   last entry. */
  a_symbol_ptr
		generic_symbol;
			/* The symbol for the class template symbol that
			   represents the generic for which the generic
			   arguments are being checked. */
  a_template_arg_ptr
		generic_arg_list;
			/* The generic arguments that are to be checked
			   against the constraints of generic_symbol. */
  a_source_position
		error_position;
			/* The position to be used for any diagnostics
			   issued for constraint violations. */
} a_deferred_constraint_check;

extern void perform_deferred_constraint_checks(a_scope_depth	scope_depth);

/*
Set the flag that specifies that constraint errors should be deferred and
checked later.
*/
#define begin_deferral_of_constraint_checks()				\
{									\
  if (cli_or_cx_enabled) {						\
    scope_stack[depth_scope_stack].defer_constraint_checks = TRUE;	\
  }  /* if */								\
}

/*
Clear the flag that specifies that constraint checks should be deferred,
and perform any checks that were deferred.  scope_depth specifies the
scope depth at the point that the deferral was started and the original
constraint checks were done.
*/
#define end_deferral_of_constraint_checks(scope_depth)			\
{									\
  if (cli_or_cx_enabled) {						\
    check_assertion(scope_stack[(scope_depth)].defer_constraint_checks);\
    scope_stack[(scope_depth)].defer_constraint_checks = FALSE;		\
    if (scope_stack[(scope_depth)].deferred_constraint_checks != NULL) {\
      /* Only make this call if there are entries on the list. */	\
      perform_deferred_constraint_checks(scope_depth);			\
    }  /* if */								\
  }  /* if */								\
}

/*
Throw away any deferred constraint entries.
*/
#define discard_deferred_constraint_checks()				\
{									\
  if (scope_stack[depth_scope_stack].deferred_constraint_checks != NULL) { \
    /* Only make this call if there are entries on the list. */	       	\
    f_discard_deferred_constraint_checks();				\
  }  /* if */								\
}

#if DEBUG
/* GCOVR_EXCL_START */
extern
void db_generic_constraint_kind(a_generic_constraint_kind	kind);

extern
void db_generic_constraint(a_generic_constraint_ptr	gcp);

extern
void db_generic_constraint_list(a_generic_constraint_ptr	gcp,
                                int				indent);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

#else /* !MICROSOFT_EXTENSIONS_ALLOWED */

/*
Stub version for use when Microsoft extensions are not enabled.
*/
#define generic_param_if_generic_definition_argument(tp) (tp)

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean check_internal_template_instantiation(
                                        a_symbol_ptr       template_sym,
                                        a_template_arg_ptr template_arg_list,
                                        a_source_position  *arg1_pos,
                                        a_source_position  *arg2_pos,
                                        a_source_position  *arg3_pos);

extern a_boolean is_instance_of_class_template(
				a_type_ptr		instance_type,
				a_symbol_ptr		template_sym,
				a_template_arg_ptr	*templ_arg_list);

extern a_boolean is_or_derived_from_instance_of_class_template(
                                           a_type_ptr         instance_type,
                                           a_symbol_ptr       template_sym,
                                           a_template_arg_ptr *templ_arg_list);

extern a_template_param_ptr copy_template_param_list(
                                                   a_template_param_ptr  tpl);

extern a_template_param_ptr copy_template_param_list_with_new_depth(
                                a_template_param_ptr        templ_params,
                                a_template_nesting_depth    new_nesting_depth);

extern void init_tmpl_decl_state_for_generated_member_template(
                                                a_tmpl_decl_state_ptr  state,
                                                a_decl_parse_state     *dps);

extern void complete_generated_member_template(
                                            a_tmpl_decl_state_ptr  decl_state,
                                            a_func_info_block      *func_info,
                                            a_symbol_ptr           sym);

extern void update_implicit_deduction_guides(a_symbol_ptr  ct_sym);

extern a_symbol_ptr create_transformed_deduction_guide_for_alias_template(
                                                      a_symbol_ptr  alias_sym,
                                                      a_symbol_ptr  guide_sym);

extern void create_deduction_guides_for_template_template_param(
                                             a_symbol_ptr  tttp_sym,
                                             a_symbol_ptr  arg_sym,
                                             a_symbol_ptr  *result_guide_list);

extern a_symbol_ptr bound_template_template_argument(
                                         a_symbol_ptr  tttp_sym,
                                         a_boolean     *is_deducible_template);

extern a_symbol_ptr make_aggregate_deduction_candidate(
                                                     a_symbol_ptr      ct_sym,
                                                     a_param_type_ptr  params);

extern a_boolean is_template_deducible_from(a_template_ptr  tmpl,
                                            a_type_ptr      type);

#if DEBUG
/* GCOVR_EXCL_START */
extern unsigned long db_show_template_space_used(unsigned long grand_total);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

extern a_hash_value hash_instantiation(a_void_ptr	key);

extern a_boolean compare_instantiation(a_void_ptr	entry,
                                       a_void_ptr	key);

extern a_hash_value hash_substitution(a_symbol_ptr        template_sym,
                                      a_template_arg_ptr  templ_args);

extern a_boolean compare_substituted_type_list_entry(a_void_ptr	entry,
						     a_void_ptr	key);

extern
a_boolean too_many_pending_instantiations(a_symbol_ptr      template_sym,
                                          a_symbol_ptr      instance_sym,
                                          a_source_position *pos);

extern void increment_pending_instantiations(a_symbol_ptr  template_sym);

extern void decrement_pending_instantiations(a_symbol_ptr  template_sym);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE
#endif /* TEMPLATES_H */

