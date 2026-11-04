/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

expr.h -- Declarations related to expression parsing.

*/

/* Avoid including these declarations more than once: */
#ifndef EXPR_H
#define EXPR_H 1

#if !STANDALONE_UTILITY_PROGRAM
#ifndef DECLS_H
#include "decls.h"
#endif /* ifndef DECLS_H */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
The operators and their precedences are:

Operators			Precedence	Associativity
()				20		N/A	() syntactic grouping
[] () . -> ++ --		19		L	[] subscripting
							() function call
							++ -- postfix
++ -- & * + - ~ ! sizeof	18		R	Prefix operators
cast				17		R
.* ->* (C++ only)		16		L
* / %				15		L
+ -				14		L
<< >>				13		L
<=>                             12		L
< > <= >=			11		L
== !=				10		L
<? >? (GNU min/max)		9		L
&				8		L
^				7		L
|				6		L
&&				5		L
||				4		L
?				3		R
= += -= *= /= %=
&= ^= |= <<= >>=		2		R	Assignment operators
,				1		L

Higher-valued precedence means an operator binds more tightly.
Precedence level 0 is used to bracket a complete expression.
*/
#define LEFT_ASSOC  TRUE
#define RIGHT_ASSOC FALSE
#define PREC_PRIMARY    20
#define PREC_POSTFIX    19
#define PREC_PREFIX     18
#define PREC_CAST       17
#define PREC_PTR_TO_MEMBER 16
#define PREC_MULT_DIV   15
#define PREC_PLUS_MINUS 14
#define PREC_SHIFT      13
#define PREC_SPACESHIP  12
#define PREC_RELATIONAL 11
#define PREC_EQ_NE      10
#define PREC_GNU_MIN_MAX 9
#define PREC_AND         8
#define PREC_EXCL_OR     7
#define PREC_OR          6
#define PREC_AND_AND     5
#define PREC_OR_OR       4
#define PREC_QUEST_MARK  3
#define PREC_ASSIGNMENT  2
#define PREC_COMMA       1
#define PREC_LOWEST      0


/* Flag bits used to indicate scanning options that apply to one level
   of expression scanning.  These are localized options that indicate
   special handling for an expression because of the context. */
#define EOPT_DISALLOW_COMMA_OPERATOR 0x1
			/* The comma operator should not be allowed at the top
			   level.  Certain contexts suppress the comma because
			   it has another meaning there (e.g., argument
			   lists). */
#define EOPT_OPERAND_OF_CAST 0x2
			/* This expression is the immediate operand of a cast.
			   Floating constants are allowed in integral constant
			   expressions when they are the immediate operand
			   of a cast. */
#define EOPT_OPERAND_OF_ADDRESS_OF 0x4
			/* This expression is the operand of a unary "&"
			   operator. */
#define EOPT_TRAPPED_LEFT_PAREN 0x8
			/* The caller of scan_expr scanned over a left
			   parenthesis which it turned out should have begun
			   an expression.  scan_expr pretends that there is
			   a left parenthesis preceding the current token. */
#define EOPT_ALLOW_BOUND_FUNCTION 0x10
			/* A C++ bound function may be returned. */
#define EOPT_PRESERVE_PROPERTY_REF 0x20
			/* A reference to a Microsoft property member can
			   be left in that form, so that it has a chance
			   to be rewritten in the "put" form.  By default,
			   it will be rewritten in the "get" form. */
#define EOPT_PTR_TO_MEMBER_CONTEXT 0x40
			/* The expression is the immediate operand of the
			   unary "&" operator where a pointer-to-member
			   constant would be valid (presumably without
			   intervening parentheses). */
#define EOPT_OPERAND_OF_OFFSETOF 0x80
			/* This expression is in the top-level chain of the
			   second operand of __builtin_offsetof. */
#define EOPT_DELEGATE_INITIALIZER 0x100
			/* This expression is a top-level expression in the
			   initializer for a C++/CLI gcnew of a delegate
			   type. */
#define EOPT_LOGICAL_NOT_OPERAND 0x200
			/* This expression is an operand for a logical "not"
			   operator. */
#define EOPT_FOLD_EXPR_CONTEXT 0x400
			/* This is a parenthesized expression that could
			   possibly be a C++17 fold expression. */
#define EOPT_CALL_RESCAN 0x800
			/* Flag set when calling make_rescan_operand_full from
			   make_call_rescan_operands. */
#define EOPT_CONSTRAINT_EXPR 0x1000
			/* Flag set when parsing a constraint expression (in a
			   "requires" clause or in a concept definition).  This
			   causes logical and/or to be handled specially. */
#define EOPT_REQUIRES_CLAUSE 0x2000
			/* Flag set when parsing the expression for a
			   "requires" clause (which only permit primary
			   expressions and logical and/or operators at the top
			   level).  This flag is cleared while scanning primary
			   sub-expressions. */
#define EOPT_SUBSCRIPT_OP 0x4000
			/* Flag set when parsing the expression for a subscript
			   operator (e.g., x[2] or x[2,3]). */
#define EOPT_REFLECTION_OP 0x8000
			/* This expression is the operand of a unary "^"
			   operator. */
#define EOPT_NO_OPTIONS 0

typedef int a_local_expr_options_set;
typedef struct an_operand *an_operand_ptr;

/*
Enumeration of the kinds of initializer values described by
an_init_component.
*/
enum an_init_component_kind : a_byte {
  ick_expression,	/* An expression. */
  ick_braced,		/* A brace-enclosed list. */
  ick_designator,	/* A designator (for C99-style or GNU-style
			   designated initializers). */
  ick_continued		/* A placeholder component indicating that more
			   elements of a brace-enclosed list should be
			   parsed. */
};


/*
An opaque type to point to state information needed to suspend/resume the
parsing of braced initializer lists.
*/
typedef struct a_braced_list_continuation *a_braced_list_continuation_ptr;


/*
Entry describing a value in an initializer, which is either an expression
or a brace-enclosed list.  In the C++11 standard, the corresponding syntax
term is "initializer-clause".  It does double duty: An expression-list (i.e.,
an argument list) is an initializer-list, which is a list of
initializer-clauses with possible variadic template expansions.
Accordingly, there are two names for this entry: an_init_component
and an_arg_list_elem.  Generally, the expression routines use
an_arg_list_elem and the initialization routines use an_init_component.
A true initializer list can contain designators (and an_init_component
is typically used), whereas an argument list cannot (and an_arg_list_elem
is typically used).
*/
typedef struct an_init_component *an_init_component_ptr;
typedef struct an_init_component {
  an_init_component_ptr
		next;	/* When this entity is on a list, a pointer to the
			   next component on the list.  NULL if this is the
			   last component or if the entry is not on a list. */
  ENUM_TYPE_FOR_BIT_FIELD(an_init_component_kind)
		kind:8;	/* The kind of initializer value (e.g., brace-enclosed
			   list).  (Originally, this field was declared as an
			   ordinary field of type an_init_component_kind, but
			   declaring it as a bit field produces a better layout
			   with some compilers.) */
  a_bit_field	bundled:1;
			/* Set to TRUE if expressions within this component
			   have been "bundled," meaning some things like
			   object lifetimes have been detached from the
			   enclosing context and attached to this entry so
			   they can be pulled out and restored later when the
			   rest of the processing for the expression is
			   done. */
  a_bit_field	detached_ref_entries:1;
			/* Set to TRUE if any ref entries in this (expression)
			   component are not attached to the current
			   expression at present. */
  a_bit_field	contains_designator:1;
			/* Set to TRUE if this is an ick_braced component that
			   directly contains an ick_designator component. */
  a_bit_field	check_narrowing:1;
			/* TRUE if the narrowing conversion checks should
			   be forced when this component is processed.
			   This flag is set, for example, on the elements
			   of a braced-init-list when it is being used
			   as an argument list, since in that context even
			   the simple conversions of the arguments to the
			   parameter types are prohibited from involving
			   narrowing conversions.  The narrowing checks
			   generate errors, not warnings, in these cases. */
  a_bit_field	braced_init_in_parentheses:1;
			/* TRUE if this entity is a braced-init-list that
			   was scanned in a parenthesized initializer that
			   expects a single expression.  [dcl.init]p13 of
			   the C++11 standard disallows a parenthesized
			   initializer of the form ({x}) if the entity being
			   initialized is not a class. */
  a_bit_field	permanently_allocated:1;
			/* TRUE if this entry is permanently allocated and
			   an attempt to free it should be ignored. */
#if CHECKING
  a_bit_field	on_free_list:1;
			/* TRUE if this entry has been freed and is on the
			   available list. */
#endif /* CHECKING */
  a_bit_field	constant_expr_ruled_out:1;
			/* For an ick_expression component, TRUE if the
			   expression does not have the form required of a
			   constant expression in the current mode.  That
			   can be very slightly different from whether the
			   expression actually evaluates to a constant. */
  a_bit_field	consteval_function_designator_seen:1;
			/* TRUE if during the parsing of this component a
			   function designator for a consteval function was
			   seen. */
  a_bit_field	preserved_deduced_pack:1;
			/* TRUE if this entry is a copy of a pack expansion
			   preserved for future deduction rather than an
			   actual component. */
  a_bit_field	direct_init_designator:1;
			/* Set to TRUE if this is an ick_designator component
			   that is immediately followed by "{" (instead of the
			   more traditional "="). */
  a_pack_expansion_descr_ptr
		pack_expansion_descr;
			/* If non-NULL, this entity is a pack expansion
			   (it is followed by "...", as in "T()..."), and this
			   points to the expansion description.  For an
			   ick_expression, the same pointer is also stored in
			   the operand pack_expansion_descr field. */
  union {
    /* When kind == ick_expression: */
    struct {
      struct an_arg_operand
		*arg_op;
			/* The expression.  The an_arg_operand struct is
			   opaque outside of the expression routines. */
      an_object_lifetime_ptr
		lifetime;
			/* When this entry is bundled, non-NULL to preserve
			   an associated lifetime until the point when the
			   expression is handled. */
    } expr;
    /* When kind == ick_braced: */
    struct {
      an_init_component_ptr
		list;
			/* A list of initializer values linked on the "next"
			   field.  NULL if the list is empty. */
      a_source_position
		start_pos,
		end_pos;
			/* The source positions of the opening and closing
			   brace tokens. */
    } braced;
    /* When kind == ick_designator: */
    struct {
      a_symbol_header_ptr
		field_name;
			/* Pointer to the symbol header for a field designator
			   or NULL if this is an array element designator. */
      a_constant_ptr
		element_index;
			/* The constant value specified in an array element
			   designator (the first one in the case of a GNU-style
			   array range designator), or NULL if this is a field
			   designator. */
      a_constant_ptr
		last_element_index;
			/* If this component represents a GNU-style array range
			   designator, the second constant value specified in
			   the range.  Otherwise, NULL for a field designator
			   and the same value as element_index for an array
			   designator. */
      a_source_position
		position;
			/* The source position of the designator. */
      a_field_ptr
		resolved_field;
			/* When the designated field has been resolved,
			   resolved_field will point to the designated field.
			   NULL otherwise. */
    } designator;
    /* When kind == ick_continued: */
    struct {
      a_braced_list_continuation_ptr
		state;
			/* An opaque pointer to state information that must be
			   restored to permit the continued parsing of a braced
			   initializer list. */
    } continuation;
  } variant;
} an_init_component;
typedef an_init_component an_arg_list_elem;
typedef an_arg_list_elem *an_arg_list_elem_ptr;

an_init_component_ptr get_continued_elem(an_init_component_ptr  icp);

void complete_braced_init_list_parsing(an_init_component_ptr  icp_tree);

/*
Macros to manage to the next initialization component.
*/
#define is_last_elem(icp)                                                    \
  ((icp)->next == NULL)

#define is_continuation_elem(icp)                                            \
  ((icp)->kind == (an_init_component_kind)ick_continued)

#define next_elem(icp)                                                       \
  (is_last_elem(icp)                 ? (an_init_component_ptr) NULL :        \
   is_continuation_elem((icp)->next) ? get_continued_elem(icp) :             \
                                       (icp)->next)

#define p_next_elem(icp)                                                     \
  (&(icp)->next)

#define is_single_elem(icp)                                                  \
  ((icp) != NULL && (icp)->next == NULL)

#define split_tail_elems(icp)                                                \
  ((icp)->next = NULL)

#ifdef _lint
/*lint -emacro(664,append_elem)*/
#define append_elem(icp, tail)                                               \
  (check_assertion((icp) != NULL), (icp)->next = tail)
#else /* !defined(_lint) */
#define append_elem(icp, tail)                                               \
  ((icp)->next = tail)
#endif /* defined(_lint) */

/*
Macro to identify initialization components that are expressions.
*/
#define is_expression_component(icp)                                         \
  ((icp)->kind == (an_init_component_kind)ick_expression)

/*
Macro to identify braced initialization components.
*/
#define is_braced_init_component(icp)                                        \
  ((icp)->kind == (an_init_component_kind)ick_braced)

/*
Macro to identify designator components.
*/
#define is_designator_component(icp)                                        \
  ((icp)->kind == (an_init_component_kind)ick_designator)

/*
Return the operand address from an expression init component.
*/
#define operand_of_arg_list_elem(icp) (&(icp)->variant.expr.arg_op->operand)

/*
Entry used to pass information about the context for a rescan to redo
semantic analysis as part of template deduction.  Many of the fields here
are parameters to copy_template_param_expr that we want to pass from
that function into the expression routines and then back again without
having to list each one on every intervening call.
*/
typedef struct a_rescan_control_block {
  an_expr_node_ptr
		expr;
			/* The expression being rescanned.  This is used only
			   when calling the scan_xxx_operator routines, to
			   reduce the number of parameters by one. */
  a_token_kind	operator_token;
			/* The token kind associated with the operator of the
			   expression associated with expr, if there is one.
			   tok_error otherwise.  Also used only when calling
			   the scan_xxx_operator routines. */
  an_expr_node_ptr
		argument_list;
			/* When processing a call, this points to the
			   argument list. */
  a_template_arg_ptr
		template_arg_list;
			/* The template argument list being tried. */
  a_template_param_ptr
		template_param_list;
			/* The parameter list of the template being tried. */
  a_ctws_options_set
		options;
			/* Options for copy_template_param_expr. */
  a_ctws_state_ptr
		ctws_state;
			/* The template argument substitution state. */
  a_byte_boolean
		error_detected;
			/* TRUE if an error was detected in the rescan, which
			   makes the deduction fail. */
} a_rescan_control_block;


/* Floating point type sizes and precisions. */

EXTERN_THREAD a_targ_size_t
                num_mantissa_bits[(int)fk_last + 1];
EXTERN_THREAD a_targ_size_t
                flt_type_size[(int)fk_last + 1];
EXTERN_THREAD int
                min_exponent[(int)fk_last + 1];
EXTERN_THREAD int
                max_exponent[(int)fk_last + 1];

#if !STANDALONE_UTILITY_PROGRAM
extern void prescan_initializer_for_auto_type_deduction(
                                        a_decl_parse_state *dps,
                                        a_boolean          parenthesized_init);

extern void scan_and_discard_init_component(a_decl_parse_state  *dps);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern
an_expr_node_ptr make_lvalue_cast_node(an_expr_node_ptr source_expr,
                                       a_type_ptr       type_cast_to,
                                       a_boolean        compiler_generated);

extern a_token_kind token_for_rel_op(an_opname_kind  opname);

extern void complete_comparison_rewrite(an_opname_kind           opname,
                                        an_expr_node_ptr         call_node,
                                        a_token_sequence_number  tsn,
                                        an_operand_ptr           result,
                                        a_boolean                reversed);

extern void check_defaulted_eq_properties(a_type_ptr     class_tp,
                                          a_routine_ptr  erp);

extern void check_defaulted_secondary_comp(a_type_ptr     class_tp,
                                           a_routine_ptr  nrp);

extern an_expr_node_ptr make_eq_comparison(an_expr_node_ptr  arg1,
                                           an_expr_node_ptr  arg2);

extern
a_variable_ptr make_spaceship_cmp_variable(an_expr_node_ptr  arg1,
                                           an_expr_node_ptr  arg2,
                                           a_type_ptr        tp,
                                           an_expr_node_ptr  *p_ne_expr);

extern void check_defaulted_spaceship_return_type(a_routine_ptr  srp,
                                                  a_type_ptr     class_type);

extern
void make_defaulted_final_spaceship_return(a_type_ptr       func_tp,
                                           a_statement_ptr  return_stmt);

extern
an_expr_node_ptr make_synthesized_rel_op(a_token_kind      op_token,
                                         an_expr_node_ptr  arg1,
                                         an_expr_node_ptr  arg2);

extern void check_closing_paren_after_expr_list(void);

extern void skip_empty_pack_expansions_after_comma(void);

extern
void scan_ctor_arguments(a_symbol_ptr             constructor_sym,
                         a_source_position        *source_pos,
                         a_type_ptr               object_class_type,
                         a_type_ptr               dest_type,
                         a_boolean                fill_in_dtor,
                         a_boolean                elision_allowed,
                         a_boolean                is_custom_ms_attr_arg_list,
                         a_conv_context_set       conv_context,
                         a_rescan_control_block   *rcblock,
                         a_boolean                arg_list_supplied,
                         an_arg_list_elem_ptr     supplied_arg_list,
                         an_arg_list_elem_ptr     init_list_ctor_arg_list,
                         a_boolean                *trivial_ctor,
                         a_boolean                *explicit_ctor,
                         a_boolean                *elision_done,
                         a_boolean                *unboxing_conv,
                         a_boolean                *string_ctor_skip,
                         an_operand_ptr           simple_result,
                         a_dynamic_init_ptr       *p_dip,
                         an_expr_node_ptr         *p_temp_init_node,
                         a_source_position        *closing_paren_position);

extern void scan_dependent_parenthesized_initializer(
                          a_rescan_control_block   *rcblock,
                          a_boolean                arg_list_supplied,
                          an_arg_list_elem_ptr     supplied_arg_list,
                          a_boolean                is_custom_ms_attr_arg_list,
                          an_operand_ptr           single_operand,
                          a_dynamic_init_ptr       *dip);

extern an_arg_list_elem_ptr rescan_expr_list(an_expr_node_ptr       src_list,
                                             a_rescan_control_block *rcblock);

extern void scan_ctor_args_or_paren_aggr_init(
                                      a_type_ptr             dest_type,
                                      a_rescan_control_block *rcblock,
                                      a_boolean              arg_list_supplied,
                                      an_arg_list_elem_ptr   *arg_list,
                                      a_boolean              *aggr_init);

extern a_type_ptr new_delete_base_type_from_operation_type(a_type_ptr type);

extern a_boolean new_or_delete_type_requires_array_handling(
                                                 a_type_ptr type,
                                                 a_boolean  check_constructor);

extern a_boolean is_expr_start_token(a_token_kind tok);

extern a_boolean token_is_function_name_string_literal(a_token_kind token);

extern void set_curr_token_to_function_name_string(
                                       a_boolean                     do_concat,
                                       a_string_or_char_literal_kind lit_kind);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean set_curr_token_to_microsoft_xprefix_operator_string(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_const_char *spelling_for_function_name_token(a_token_kind token);

extern a_boolean operand_is_string_literal(an_operand_ptr operand);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean operand_is_cast_string_literal(an_operand_ptr operand,
                                                a_constant_ptr *string_con);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void record_start_of_lambda_header(a_lambda_ptr lambda);

extern void record_end_of_lambda_header(a_lambda_ptr lambda);

extern void deduce_return_type_from_void_operand(
                                          a_routine_ptr      rp,
                                          a_boolean          keep_placeholder,
                                          a_source_position  *diag_pos);

extern a_boolean arg_matches_auto_template_param(
                                     a_type_ptr             param_type,
                                     a_constant_ptr         constant,
                                     an_arg_operand_ptr     arg_operand,
                                     a_type_ptr             *p_deduced_type,
                                     a_source_position_ptr  position,
                                     a_template_arg_ptr     arg_list = NULL,
                                     a_template_param_ptr   param_list = NULL);

extern
void record_template_arg_operand(a_template_arg_ptr tap,
                                 an_expr_node_ptr   expr);

extern
void transfer_arg_operand_for_template_arg(a_template_arg_ptr tap,
                                           a_template_arg_ptr orig_tap);

extern a_template_arg_ptr
copy_template_arg_list_with_substitution_rebuilding_arg_operands(
			a_symbol_ptr		template_sym,
			a_template_arg_ptr	arg_list_to_copy,
			a_template_param_ptr	param_list_for_copy,
			a_template_arg_ptr	templ_arg_list,
			a_template_param_ptr	templ_param_list,
			a_source_position	*source_pos,
			a_ctws_options_set	options,
			a_boolean		*copy_error,
			a_ctws_state_ptr	ctws_state);

extern an_expr_node_ptr scan_integer_expression(
                                        a_boolean              is_switch_expr,
                                        an_init_component_ptr  cache);

extern an_expr_node_ptr scan_void_expression(
                                  a_boolean           repeated_in_loop,
                                  a_boolean           marked_as_gnu_extension,
                                  a_boolean           is_statement_expr,
                                  a_dynamic_init_ptr  *dip,
                                  an_init_component   *cache);

extern
an_expr_node_ptr scan_typed_expression(a_type_ptr     required_type,
                                       a_type_ptr     alternate_type,
                                       an_error_code  err_code);

extern void scan_bool_constant_expression(a_constant   *constant,
                                          a_diag_list  *diag_list = NULL);

extern void check_range_based_for_statement(
                          a_statement_ptr            statement,
                          a_source_position          *expr_position,
                          a_token_sequence_number    tok_seq_number,
                          a_scope_pointers_block_ptr iterator_pointers_block);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern
void check_for_each_statement(a_statement_ptr            statement,
                              an_operand_ptr             prev_decl_iterator,
                              a_source_position          *expr_position,
                              a_token_sequence_number    tok_seq_number,
                              a_scope_pointers_block_ptr pointers_block);
extern void scan_for_each_expression(a_statement_ptr   statement,
                                     a_source_position *expr_position);
extern void scan_previously_decl_iterator_name(
                                      a_for_each_loop_ptr felp,
                                      an_operand_ptr      prev_decl_iterator);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void scan_range_based_for_expression(a_statement_ptr   statement,
                                            a_source_position *expr_position);

extern void scan_expansion_initializer(a_statement_ptr   statement);
extern void set_expansion_element_binding(
                                  a_variable_ptr   variable,
                                  an_expr_node_ptr initializer_expr,
                                  a_boolean        extend_temporaries = FALSE);
extern
a_boolean call_via_reflections(a_reflection_value             *target_rv,
                               Dyn_array<a_reflection_value>  *arg_rvs,
                               a_source_position              *diag_pos,
                               a_constant                     *result_con);

an_expr_node_ptr build_info_vector_construction(a_type_ptr         vector_type,
                                                a_constant         *begin_con,
                                                a_constant         *end_con,
                                                a_source_position  *diag_pos);

extern void scan_default_arg_expr(a_param_type_ptr ptp,
                                  a_boolean        is_member_or_friend,
                                  a_boolean        for_consteval_function);

extern an_expr_node_ptr prep_default_arg_expr(an_expr_node_ptr expr,
                                              a_param_type_ptr ptp,
                                              a_boolean        evaluated);

extern
a_boolean variable_eligible_for_copy_optimization(a_variable_ptr var,
                                                  a_boolean      return_case,
                                                  a_boolean      move_case);

extern an_expr_node_ptr scan_return_expression(
                                          a_type_ptr            required_type,
                                          an_error_code         err_code,
                                          a_dynamic_init_ptr    *dip,
                                          an_arg_list_elem_ptr  *alep);

extern a_symbol_ptr look_up_named_member_function(a_type_ptr       type,
                                                  a_const_char     *name,
                                                  a_symbol_locator *locator);

extern
void add_await_to_operand(an_operand_ptr          operand,
                          a_source_position       *pos,
                          a_token_sequence_number tok_seq_number,
                          a_boolean               for_yield,
                          a_boolean               generated_suspend_point,
                          a_boolean               initial_suspend_point);

extern an_expr_node_ptr make_coroutine_result_expression(
                                              an_arg_list_elem_ptr  alep,
                                              a_boolean             is_yield,
                                              a_statement_ptr       sp);

extern void scan_pp_expression(a_constant *constant);

extern void scan_integral_constant_expression(a_constant *constant);

extern void scan_fs_integral_constant_expression(a_type_ptr specific_type,
                                                 a_boolean  is_enum,
                                                 a_constant *constant);

extern void scan_constant_dimension_expression(a_constant *constant);

extern void scan_nonconstant_dimension_expression(
                                    a_boolean        is_new_or_delete_bound,
                                    a_boolean        is_top_level_vla_bound,
                                    a_boolean        is_evaluated_sizeof_arg,
                                    a_boolean        *is_constant,
                                    an_expr_node_ptr *expression,
                                    a_constant       *constant);

extern void do_fs_constant_fixup(a_constant_ptr  cp);

extern void extract_constant_from_operand_with_fs_fixup(
                                                     an_operand_ptr operand,
                                                     a_constant     *constant);

extern
void rescan_selector_of_call(a_rescan_control_block *rcblock,
                             an_operand_ptr         function_operand,
                             an_operand_ptr         bound_function_selector);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void insert_temporary_initialization(an_expr_node_ptr temp_init_expr,
                                            an_operand_ptr   result);

extern void process_microsoft_null_pointer_constant_bug(
                                                    an_operand_ptr operand,
                                                    a_type_ptr     dest_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern
void process_simple_assignment(an_operand_ptr          operand_1,
                               an_operand_ptr          operand_2,
                               a_source_position       *operator_position,
                               a_token_sequence_number operator_tok_seq_number,
                               a_boolean               check_for_overloading,
                               an_operand_ptr          result);

#if GNU_EXTENSIONS_ALLOWED
an_expr_node_ptr scan_asm_operand_expression(a_boolean output,
                                             a_boolean input,
                                             a_boolean is_memory_operand);
#endif /* GNU_EXTENSIONS_ALLOWED */

#if BUILTIN_FUNCTIONS_ENABLED
extern a_boolean fold_gnu_call_if_possible(an_operand_ptr   op,
                                           an_expr_node_ptr call);
#endif /* BUILTIN_FUNCTIONS_ENABLED */

#if !STANDALONE_UTILITY_PROGRAM
extern a_dynamic_init_ptr scan_array_mem_initializer(a_constructor_init  *cip,
                                                     an_init_component   *icp);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern an_expr_node_ptr prep_generated_arg_expr(an_expr_node_ptr  expr,
                                                a_param_type_ptr  param,
                                                a_source_position *err_pos);

#if !STANDALONE_UTILITY_PROGRAM
extern void scan_class_initializer_expression(a_decl_parse_state  *dps);

extern a_boolean whole_aggr_class_init_possible(
                                            an_init_component_ptr  icp,
                                            a_type_ptr             dest_type);

extern a_boolean whole_array_init_possible(an_init_component_ptr  icp,
                                           a_type_ptr             dest_type,
                                           a_constant_ptr         *result);

#if GNU_VECTOR_TYPES_ALLOWED
a_boolean whole_vector_init_possible(an_init_component_ptr  icp,
                                     a_type_ptr             dest_type);
#endif /* GNU_VECTOR_TYPES_ALLOWED */

extern
a_boolean is_overloadable_type_operand_full(an_operand_ptr operand,
                                            a_boolean      first_operand,
                                            a_boolean      CFOO_guard);

extern void value_init_variable_or_member(a_type_ptr         type,
                                          an_init_state      *is,
                                          a_source_position  *diag_pos);

extern void scan_class_parenthesized_initializer(
                                   a_type_ptr            class_type,
                                   a_type_ptr            object_class_type,
                                   a_source_position     *source_pos,
                                   a_boolean             fill_in_dtor,
                                   a_boolean             args_supplied,
                                   an_arg_list_elem_ptr  arg_list,
                                   an_init_state         *is);

extern a_dynamic_init_ptr forwarding_initializer_for_inheriting_constructor(
                                                       a_routine_ptr ctor,
                                                       a_routine_ptr inh_ctor);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern void scan_template_argument_constant_expression(
                                     a_type_ptr             param_type,
                                     a_constant             *constant,
                                     a_template_arg_ptr     arg_list = NULL,
                                     a_template_param_ptr   param_list = NULL);

extern an_arg_operand_ptr scan_nontype_template_argument(
                                  a_decl_sequence_number initial_inst_seq_num);

extern a_boolean nontype_template_arg_is_compatible_with_param_type(
                                            an_arg_operand_ptr arg_operand,
                                            a_type_ptr         param_type);

extern void conv_nontype_template_arg_to_param_type(
                                            an_arg_operand_ptr arg_operand,
                                            a_type_ptr         param_type,
                                            a_constant         *constant);

extern a_requires_clause_ptr scan_requires_clause(a_boolean  discard);

extern an_expr_node_ptr scan_concept_expression();

#if !STANDALONE_UTILITY_PROGRAM
extern
an_init_component_ptr get_braced_init_list(a_boolean          is_full_expr,
                                           a_decl_parse_state *dps);

extern an_init_component_ptr scan_full_initializer_expr_as_component(
                                     a_decl_parse_state *dps,
                                     a_boolean          parenthesized,
                                     a_boolean          allow_empty_expansion);

extern
void convert_initializer(an_init_component_ptr icp,
                         a_type_ptr            dest_type,
                         a_boolean             is_var_init,
                         a_boolean             fill_in_dtor,
                         an_init_state         *is);

extern
a_constant_ptr convert_generic_aggr_init_element(an_init_component_ptr icp,
                                                 an_init_state         *is);

typedef struct an_arg_match_summary an_arg_match_summary_dummy_typedef;
extern void record_aggr_init_match(struct an_arg_match_summary *arg_match);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void aggr_init_cli_array_with_alloc(an_init_component_ptr  icp,
                                           a_type_ptr             hatype,
                                           an_init_state          *is,
                                           a_dynamic_init_ptr     *result);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_boolean expr_is_rescannable(an_expr_node_ptr expr);

extern void rescan_expr_with_substitution_internal(
                            an_expr_node_ptr         expr,
                            a_rescan_control_block   *rcblock,
                            a_local_expr_options_set local_options,
                            an_operand_ptr           result,
                            an_operand_ptr           bound_function_selector,
                            a_boolean                top_level_expr);

extern an_expr_node_ptr rescan_expr_with_substitution(
                                             an_expr_node_ptr       expr,
                                             a_type_ptr             guide_type,
                                             a_rescan_control_block *rcblock,
                                             a_constant             *constant);

extern
void rescan_dynamic_init_with_substitution(a_dynamic_init_ptr     dip,
                                           a_rescan_control_block *rcblock,
                                           an_operand_ptr         result);

#if !STANDALONE_UTILITY_PROGRAM
extern void scan_member_constant_initializer_expression(
                                                 a_decl_parse_state *dps,
                                                 a_constant         *constant);
extern
void scan_constant_initializer_expression(a_type_ptr         required_type,
                                          a_decl_parse_state *dps,
                                          a_constant         *constant);

extern void scan_dependent_type_parenthesized_initializer(
                                                  an_init_state     *is,
                                                  an_arg_list_elem  *arg_list);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_constant_ptr scan_case_label_constant(a_type_ptr switch_type);

extern an_expr_node_ptr scan_boolean_controlling_expression(
                                                an_init_component_ptr  cache);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_const_char *scan_uuidof_operand(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void scan_lambda_expression(an_operand  *result);

extern
a_type_ptr scan_decltype_operator(a_rescan_control_block *rcblock,
                                  a_boolean              might_be_id_start);

extern a_tagged_pointer scan_type_or_namespace_splicer(
                                    a_rescan_control_block *rcblock,
                                    a_boolean              might_be_id_start);

extern
a_type_ptr scan_type_splicer(a_rescan_control_block *rcblock);

extern a_type_ptr decltype_of_expr_with_substitution(
                                  a_type_ptr               type,
                                  an_expr_node_ptr         expr,
                                  a_template_arg_ptr       template_arg_list,
                                  a_template_param_ptr     template_param_list,
                                  a_ctws_options_set       options,
                                  a_boolean                *copy_error,
                                  a_ctws_state_ptr         ctws_state);

extern a_type_ptr scan_type_returning_type_trait_operator(void);

extern a_type_ptr scan_pack_index_type_specifier(
                                          a_boolean is_new_type_name,
                                          a_boolean is_implicit_type_context,
                                          a_boolean concept_okay,
                                          a_boolean might_be_id_start);

extern a_type_ptr scan_typeof_operator(a_rescan_control_block *rcblock,
                                       a_decl_pos_block       *decl_pos_block);

#if GNU_EXTENSIONS_ALLOWED 

extern a_type_ptr scan_bases_operator(void);

extern void typedef_initializer(a_symbol_ptr  symbol_ptr);

#endif /* GNU_EXTENSIONS_ALLOWED */

#if UPC_EXTENSIONS_ALLOWED
extern an_expr_node_ptr scan_upc_forall_affinity(void);
#endif /* UPC_EXTENSIONS_ALLOWED */

extern an_expr_node_ptr make_condition_value_expression(
                                                a_variable_ptr var,
                                                a_boolean      is_switch_expr);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_variable_ptr based_variable(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean in_expression_context(void);

extern a_boolean operand_is_instantiation_dependent(an_operand_ptr operand);

extern a_boolean arg_operand_is_instantiation_dependent(
                                               an_arg_operand_ptr arg_operand);

extern a_boolean arg_list_is_instantiation_dependent(
                                                   an_arg_list_elem_ptr  alep);

extern a_boolean arg_operand_involves_error_entity(
                                               an_arg_operand_ptr arg_operand);

extern
a_symbol_ptr find_default_constructor(a_type_ptr        class_type,
                                      a_boolean         include_templates,
                                      a_boolean         declarative_context,
                                      a_boolean         no_explicit,
                                      a_source_position *pos,
                                      a_boolean         *ambiguous,
                                      a_symbol_ptr      *inaccessible_match,
                                      a_boolean         *trivial);

extern
a_symbol_ptr find_copy_constructor(a_type_ptr            class_type,
                                   a_type_qualifier_set  required_qualifiers,
                                   a_boolean             source_is_rvalue,
                                   a_source_position     *pos,
                                   a_boolean             *ambiguous,
                                   a_symbol_ptr          *inaccessible_match,
                                   a_boolean             *class_bitwise_copy);

a_symbol_ptr find_copy_assignment_operator(
                                   a_type_ptr            class_type,
                                   a_type_qualifier_set  source_cv_qualifiers,
                                   a_boolean             source_is_rvalue,
                                   a_type_qualifier_set  dest_cv_qualifiers,
                                   a_source_position     *pos,
                                   a_boolean             *ambiguous,
                                   a_boolean             *bitwise_assign);

extern a_routine_ptr find_assignment_operator_for_memberwise_copy(
                                             a_type_ptr        class_type,
                                             an_expr_node_ptr  source_expr,
                                             an_expr_node_ptr  dest_expr,
                                             a_source_position *dest_decl_pos);

extern void process_unattached_template_argument_list(
                                         a_template_arg_ptr template_arg_list);

extern an_expr_node_ptr make_assignment_expr(
                                      an_expr_node_ptr       lvalue_expr,
                                      an_expr_operator_kind  op,
                                      an_expr_node_ptr       rvalue_expr);

extern an_expr_node_ptr make_builtin_edg_is_deducible_expr(
                                                       a_template_ptr    templ,
                                                       a_type_ptr        type);

#if !STANDALONE_UTILITY_PROGRAM

extern void determine_get_call_for_tuple_like_binding(
                                           a_variable_ptr     container,
                                           a_type_ptr         tp,
                                           a_targ_size_t      elem_idx,
                                           a_source_position  *diag_pos,
                                           an_init_component  **p_icp,
                                           a_boolean          *lvalue_binding);

extern void record_init_for_array_struct_binding(a_decl_parse_state  *dps,
                                                 an_init_component   *icp);

extern void record_struct_binding_expr_for_array_element(
                                                  a_variable_ptr  container,
                                                  a_variable_ptr  binding,
                                                  a_targ_size_t   n);

extern void record_struct_binding_expr_for_field(a_variable_ptr  container,
                                                 a_variable_ptr  binding,
                                                 a_field_ptr     field);

#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_boolean in_lambda_body(void);

extern a_scope_depth scope_depth_for_capture(a_variable_ptr var,
                                             a_scope_depth  starting_depth,
                                             a_lambda_ptr   *lambda);

extern a_boolean check_var_for_lambda_capture(a_variable_ptr  var,
                                              a_boolean       implicit,
                                              a_boolean       by_ref,
                                              an_error_code   *diag);

extern a_boolean current_mode_allows_field_selection_folding(void);

extern
a_boolean compute_is_invocable(a_builtin_operation_kind kind,
                               a_type_ptr               dst_type,
                               an_expr_node_ptr         expr);

extern
a_boolean compute_is_convertible(a_type_ptr               src_type,
                                 a_type_ptr               dst_type,
                                 a_builtin_operation_kind op);

extern
a_boolean compute_is_constructible(a_builtin_operation_kind kind,
                                   a_type_ptr               dst_type,
                                   an_expr_node_ptr         expr);

extern
a_boolean meta_compute_is_constructible(a_builtin_operation_kind kind,
                                        a_type_ptr               type,
                                        a_type_ptr               *arg_types,
                                        int                      n_args);

extern
a_boolean meta_compute_is_invocable(a_builtin_operation_kind kind,
                                    a_type_ptr               type,
                                    a_type_ptr               *arg_types,
                                    int                      n_args);

extern
a_boolean meta_fold_builtin_type_trait(a_builtin_operation_kind kind,
                                       a_type_ptr               *types,
                                       int                      n_types,
                                       a_host_large_integer     *p_result);

extern
a_boolean compute_is_destructible(a_builtin_operation_kind kind,
                                  a_type_ptr               type);

extern
a_boolean compute_is_assignable(a_builtin_operation_kind kind,
                                a_type_ptr               dst_type,
                                a_type_ptr               src_type);

extern
a_boolean compute_reference_binds_to_temporary(
                                            a_type_ptr               ref_type,
                                            a_type_ptr               init_type,
                                            a_builtin_operation_kind op);

#if MICROSOFT_EXTENSIONS_ALLOWED

extern an_expr_node_ptr make_cli_array_length_nodes(
                                               a_host_large_unsigned  rank,
                                               a_host_large_integer   dims[]);

#define UNSPECIFIED_CLI_ARRAY_LENGTH ((a_host_large_integer)0xC0FFEE)
			/* Default value representing an unspecified length
			   for an array dimension.  This is dictated by the
			   ECMA-372 standard (24.6). */

extern a_boolean scan_custom_ms_attribute_arg_list(an_ms_attribute_ptr attr);

extern a_type_ptr underlying_uuidof_type(a_type_ptr uuidof_type,
                                         a_boolean  *template_case,
                                         a_boolean  *err);

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean concept_id_value(an_expr_node_ptr  node,
                                  a_boolean         *fatal);

extern an_expr_node_ptr process_boolean_attribute_expression(
                                                        an_expr_node_ptr expr);

extern an_expr_node_ptr scan_expr_for_attribute(int        precedence,
                                                a_boolean  evaluated,
                                                a_boolean  convert_to_bool);

extern void scan_annotation_value(an_attribute_arg  *aap);

extern
a_type_ptr conditional_result_type(a_type     *tp2,
                                   a_type     *tp3,
                                   a_boolean  add_const_ref);

extern a_boolean rel_op_synthesizes_from_spaceship(a_type          *tp1,
                                                   a_type          *tp2,
                                                   an_opname_kind  rel_op);

extern an_init_component_ptr cache_expression(bool  immediate_context);

typedef struct an_initializer_cache *an_initializer_cache_ptr;
extern void prescan_parenthesized_mem_init_expr(
                                         an_initializer_cache_ptr  init_cache);

extern an_init_component_ptr scan_expr_or_braced_init_list(
                                                a_boolean bundle,
                                                a_boolean always_allow_braced);

extern a_const_char *get_string_for_function_name(a_token_kind token,
                                                  a_boolean    include_quote);

extern a_boolean do_expression_level_string_literal_concatenation(void);

extern void expr_one_time_init(void);

extern void expr_trans_unit_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef EXPR_H */


