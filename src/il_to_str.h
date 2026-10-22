/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

il_to_str.h -- Declarations related to il_to_str.c (produce an external
               string-form representation for various IL entries).

*/

/* Avoid including these declarations more than once. */
#ifndef IL_TO_STR_H
#define IL_TO_STR_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Block describing how to do output from within the il_to_str routines.
*/
typedef void an_output_str_function(
                                   a_const_char                          *str,
                                   an_il_to_str_output_control_block_ptr octl);
typedef an_output_str_function *an_output_str_function_ptr;
typedef void an_output_name_function(char *entry, an_il_entry_kind kind);
typedef an_output_name_function *an_output_name_function_ptr;
typedef void an_output_class_qualifier_function(
                                            a_type_ptr type,
                                            a_boolean  for_ptr_to_data_member);
typedef an_output_class_qualifier_function
                                       *an_output_class_qualifier_function_ptr;
typedef void an_output_enum_qualifier_function(a_type_ptr type);
typedef an_output_enum_qualifier_function
                                        *an_output_enum_qualifier_function_ptr;
typedef void an_output_func_declarator_function(a_type_ptr type);
typedef an_output_func_declarator_function
                                       *an_output_func_declarator_function_ptr;
typedef void an_output_expression_function(an_expr_node_ptr expr,
                                           a_boolean        suppress_parens);
typedef an_output_expression_function *an_output_expression_function_ptr;
typedef a_boolean an_output_name_reference_function(
                          a_name_reference_ptr     name_ref,
                          a_source_correspondence* scp,
                          an_il_entry_kind         kind,
                          a_boolean                is_declaration,
                          a_boolean                suppress_declarator_parens);
typedef an_output_name_reference_function
                                        *an_output_name_reference_function_ptr;
typedef void an_output_temp_name_function(char *entry);
typedef an_output_temp_name_function *an_output_temp_name_function_ptr;
typedef a_boolean a_typedef_visibility_test_function(
                                                    a_type_ptr type,
                                                    a_type_ptr *resolved_type);
typedef a_typedef_visibility_test_function
                                       *a_typedef_visibility_test_function_ptr;
typedef void an_output_attributes_function(
                           an_attribute_ptr       attributes,
                           an_attribute_location  syntactic_location,
                           a_boolean              primary_only);
typedef an_output_attributes_function *an_output_attributes_function_ptr;
typedef a_boolean an_expression_test_function(an_expr_node_ptr expr);
typedef an_expression_test_function *an_expression_test_function_ptr;
typedef an_expr_node_ptr an_expression_modifier(an_expr_node_ptr expr);
typedef an_expression_modifier *an_expression_modifier_ptr;

#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE

/*
Entry used in a stack to track function prototypes being rendered.  This stack
is used to identify which parameter an enk_param_ref node refers to and
whether the associated function is a prototype instantiation.
*/
typedef struct a_func_prototype_stack_entry *a_func_prototype_stack_entry_ptr;
typedef struct a_func_prototype_stack_entry {
  a_func_prototype_stack_entry_ptr
		next;	/* Pointer to the entry for the enclosing function
			   prototype scope (or NULL if this entry if for the
			   outermost function prototype scope). */
  a_param_type_ptr
		params;
			/* The parameters associated with this function
			   prototype scope. */
  a_boolean	outside_parameter_list;
			/* TRUE if we have already rendered the list of
			   parameters for this function prototype scope. */
} a_func_prototype_stack_entry;
  
extern void push_function_prototype(
                                 a_func_prototype_stack_entry_ptr       fpsep,
                                 an_il_to_str_output_control_block_ptr  octl);

extern void pop_function_prototype(
                                 an_il_to_str_output_control_block_ptr  octl);

extern a_param_type_ptr get_param_for_param_ref(
                                     an_expr_node_ptr                  expr,
                                     a_func_prototype_stack_entry_ptr  fpsep);

#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_C_GEN_BE
extern void form_param_ref(an_expr_node_ptr                       expr,
                           an_il_to_str_output_control_block_ptr  octl);
#endif /* BACK_END_IS_C_GEN_BE */

/* If you add a field here, add it also to
   clear_il_to_str_output_control_block. */
typedef struct an_il_to_str_output_control_block {
  an_output_str_function_ptr
	output_str;	/* Function to output a null-terminated string, where
			   the string consists of one of more complete
			   tokens. */
  an_output_str_function_ptr
	output_partial_token_str;
			/* Function to output a null-terminated string, where
			   the string may be only part of a token.  If NULL,
			   the output_str routine is used (implying that
			   for the kind of output being done token boundaries
			   don't matter). */
  void  *text_buffer;
			/* A type-erased pointer to the buffer output_str
			   should write to.  Note the expected type of
			   output_str must match the type of the type-erased
			   buffer. */
  an_output_name_function_ptr
	output_name;
			/* Function to output the name of an entity.  NULL
			   if a default routine should be used. */
  an_output_name_function_ptr
	output_template_name;
			/* Function to output the name of a template.  NULL
			   if a default routine should be used. */
  an_output_class_qualifier_function_ptr
	output_class_qualifier;
			/* Function to output a class qualifier, e.g.,
			   "A::B::".  NULL if a default routine should be
			   used. */
  an_output_enum_qualifier_function_ptr
	output_enum_qualifier;
			/* Function to output an enum qualifier, e.g., "E::".
			   NULL if a default routine should be used. */
  an_output_temp_name_function_ptr
	output_temp_name;
			/* Function to output a compiler-generated name based
			   on the address passed in.  Used by the C-generating
			   back end.  NULL if not needed. */
  an_output_func_declarator_function_ptr
	output_func_declarator;
			/* Function to output a function declarator from
			   a function type.  NULL if a default routine
			   should be used. */
  an_output_expression_function_ptr
	output_expression;
			/* Function to output expressions in various contexts
			   (e.g., VLA declarators).  NULL if a default routine
			   should be used (it puts out non-compilable code). */
  an_output_name_reference_function_ptr
	output_name_reference;
			/* Function to output a name as described by a given
			   name reference (which describes the qualification
			   of the name).  NULL if name reference information
			   should be ignored.  Returns TRUE if the name was
			   output using the supplied name_reference pointer
			   and FALSE otherwise (if the name_reference pointer
			   is NULL, for instance). */
  an_output_attributes_function_ptr
	output_attributes;
			/* Function to output attributes.  NULL if no
			   attributes should be output. */
  a_typedef_visibility_test_function_ptr
	is_typedef_invisible;
			/* Function that indicates whether a given typedef
			   name should be considered visible.  If the typedef
			   is invisible, it is skipped by the output routines
			   and the underlying type is put out instead.
			   Certain visibility tests (e.g., the one related to
			   suppress_typedefs, below) are always done.  This
			   pointer is non-NULL if additional tests are
			   needed. */
  an_expression_test_function_ptr
	has_unprotected_gt_or_comma_operation;
			/* Function that tests whether a ">" or "," will
			   appear outside parentheses in the text put out
			   for a given expression.  This is used to test a
			   non-type template argument to see if it needs to
			   be enclosed in parentheses. */
  an_expression_test_function_ptr
	expr_is_unusable;
			/* Function that tests whether an expression can
			   appear in the current context.  If the call
			   returns TRUE, the resulting type of a type
			   operator will be put out instead of the type
			   operator itself and the value of a constant will
			   be used instead of its backing expression. */
  an_expression_modifier_ptr
	skip_implicit_steps;
			/* Function that steps over any expression nodes
			   that will not affect the output code, such as
			   compiler-generated cast nodes. */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  a_func_prototype_stack_entry_ptr
	func_prototype_stack;
			/* If non-NULL, a pointer to the top of a stack of
			   function prototype scopes currently being rendered.
			   This is used to identify the parameter referred to
			   by an enk_param_ref node. */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
  a_template_arg_ptr
	curr_template_arg;
			/* If non-NULL, the current template argument that
			   is being put out.  Set by form_template_args and
			   used by the C++-generating back end to prevent
			   unbounded recursion when substituting non-real
			   typedefs for their underlying types. */
#endif /* BACK_END_IS_CP_GEN_BE */
  uint16_t
	max_template_arg_depth;
			/* When forming a template argument list, do not
			   process template arguments at a depth greater than
			   this value.  As an example a value of 1 would print
			   "A<B<int, C<int>>>" as "A<B<[etc...]>>".  A value of
			   0 (the default) disables this feature. */
  uint16_t
	curr_template_arg_depth;
			/* This represents the current depth into a template
			   argument list.  This value should be 0 at the start
			   of the output and increase by one every time a
			   template argument list print is started. */
  a_byte_boolean
	incomplete_output;
			/* This flag is an observable flag that is set if the
			   buffer contains output that is incomplete (e.g.,
			   because the max_template_arg_depth was hit and the
			   output was simplified to prevent performance
			   issues). */
  a_byte_boolean
	gen_compilable_code;
			/* TRUE if the generated string is intended to be
			   compiled later; FALSE means it's intended to
			   be read by humans.  One reason for this flag:
			   compilable code has to avoid bugs of the target
			   compilers even if it makes the output less
			   readable. */
  a_byte_boolean
	gen_pcc_code;	/* TRUE if the generated string should be old-style
			   pcc/K&R C.  Used by the C-generating back end. */
  a_byte_boolean
	suppress_typedefs;
			/* Skip over typedefs so that they don't show up in
			   output (used to generate the string for the GNU C++
			   __PRETTY_FUNCTION__ feature, which is also accepted
			   in default mode). */
  a_byte_boolean
	suppress_local_typedefs;
			/* Suppress function-local typedefs in type output,
			   i.e., skip over them and don't show them in the
			   output. */
  a_byte_boolean
	render_c99_bool;
			/* TRUE if the C99 boolean type should be rendered as
			   "_Bool".  Otherwise, the underlying type is used. */
  a_byte_boolean
	c_generating_back_end;
			/* TRUE if the output is being done for the
			   C-generating back end. */
  a_byte_boolean
	cpp_generating_back_end;
			/* TRUE if the output is being done for the
			   C++-generating back end. */
  a_byte_boolean
	for_diagnostics;
			/* TRUE if the output is being done for diagnostic
			   text. */
  a_byte_boolean
	reflection_display_form;
			/* TRUE if a reflection should be rendered the way
			   std::meta::display_string_of describes the entity it
			   designates: without the prefix naming the kind of
			   entity and with the full signature of a function
			   rather than just its name. */
#if DEBUG
/* GCOVR_EXCL_START */
  a_byte_boolean
	debug_output;	/* TRUE if the generated string is part of debug
			   output. */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  a_byte_boolean
	force_qualified_name;
			/* Set to TRUE by the il_to_str routines when calling
			   the output_name routine, to indicate that the
			   output must be a qualified name even if the output
			   routine thinks it could go out as an unqualified
			   name because of the context (used, e.g., for
			   pointers-to-members). */
  a_byte_boolean
	gen_vla_array_as_asterisk_bound_array;
			/* TRUE if the bounds for variable-length arrays should
			   be put out as "[*]".  This is needed to suppress
			   VLAs in function declarations. */
  a_byte_boolean
	gen_raw_tab_in_literals;
			/* TRUE if a tab character should be emitted as an
			   actual tab character rather than as '\t'. */
  a_byte_boolean
	keep_template_typedefs;
			/* TRUE if typedefs from class templates should not be
			   replaced with their underlying type and
			   non-dependent alias template instances should not be
			   replaced with their underlying type.  Note
			   suppress_typedef_names takes precedence over this
			   option. */
  a_byte_boolean
	suppress_line_breaking;
			/* Suppress any processing that breaks long output
			   lines into smaller pieces.  Turned on, for example,
			   while outputting a pragma. */
  a_byte_boolean
	suppress_cast_on_short_integral_const;
			/* Suppress the cast to a shorter-than-int integral
			   type that is normally output for integer
			   constants. */
  a_byte_boolean
	suppress_name_in_template_cast_enum_const;
			/* Suppress the name of a tpck_expression constant that
			   represents an enumerator cast to a specific type and
			   put out its value instead. */
  a_byte_boolean
	render_auto_deduction_typerefs;
			/* TRUE if typerefs representing deduced "auto" and
			   "decltype(auto)" types should be rendered as they
			   appeared in the source. */
#if GNU_VECTOR_TYPES_ALLOWED
  a_byte_boolean
	suppress_cast_on_vector_const;
			/* Suppress the "cast" on a vector constant (which is
			   really a compound literal). */
  a_byte_boolean
	defer_vector_attribute;
			/* When forming a vector type, do not put out the
			   "vector_size" attribute but just the element
			   type.  This is needed because versions 4.1 and
			   following of the GNU compilers report alignment
			   errors for large vector sizes if the vector size
			   is specified before the typedef name and the
			   corresponding alignment attribute appears in the
			   normal location for attributes following the
			   typedef name. */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  a_byte_boolean
	suppress_template_args;
			/* Suppress the output of template arguments in
			   type names. */
  a_byte_boolean
	suppress_typedef_names;
			/* Suppress the output of typedef-names in type names,
			   instead outputting the true type names.  Note this
			   option takes precedence over
			   keep_template_typedefs. */
  a_byte_boolean
	suppress_ptr_to_data_member_parens;
			/* If TRUE, do not parenthesize the declarator of a
			   type that is a pointer to data member.  The
			   parentheses are normally put out to prevent the
			   member type from being interpreted as part of a
			   globally-qualified name designating the class
			   type, e.g., "T (::X::*)", but the parentheses
			   are not permitted (or needed) in a
			   conversion-type-id and must be suppressed in
			   that context. */
  a_byte_boolean
	suppress_compiler_generated_parameters;
			/* If TRUE, form_function_declarator will put out
			   only those parameters that were present in the
			   source; parameters added by lowering, such as
			   "this" and pointers to return values, will be
			   omitted. */
  a_byte_boolean
	processing_nontype_template_argument;
			/* Set to TRUE by the il_to_str routines when
			   calling the output_name routine to indicate that
			   the name appears in the context of a nontype
			   template argument. */
  a_byte_boolean
	part_of_ud_literal;
			/* When TRUE, a constant being put out is the
			   literal portion of a user-defined literal, so
			   nothing can be put out following the constant. */
  a_byte_boolean
	pending_right_paren;
			/* Set to TRUE by the il_to_str routines when a left
			   parenthesis is put out but the corresponding
			   right parenthesis cannot be (e.g., because
			   part_of_ud_literal is TRUE).  The caller is
			   responsible for checking this flag, putting out
			   the right parenthesis at the appropriate point,
			   and clearing the flag. */
  a_byte_boolean
	suppress_expr_in_nontype_arg;
			/* When TRUE, a nontype template argument will be
			   put out as its value rather than as the backing
			   expression whose evaluation yielded that
			   value. */
  a_byte_boolean
	name_is_dependent_conversion_type_id;
			/* When TRUE, the name being processed is a
			   dependent conversion-type-id in which the usual
			   qualification of the operator name was
			   suppressed.  This information is used by the
			   C++-generating back end to control qualification
			   of the conversion-type-id. */
  a_byte_boolean
	use_microsoft_format;
			/* When TRUE, emulate the formatting of MSVC's
			   __FUNCSIG__ construct (i.e., using elaborated
			   type specifiers for tag types and "(void)"
			   instead of "()" for empty parameter lists). */
  a_byte_boolean
	type_context;	/* TRUE if we are currently putting out a type.
			   Used, e.g., by the C++-generating back end to
			   suppress the "this" keyword of an
			   explicit-object function parameter that would be
			   put out in a declaration context. */
} an_il_to_str_output_control_block;

/*
Options for form_type_first_part/form_type_second_part, in bit set form.
*/
typedef int a_form_type_options_set;
#define FTO_NO_OPTIONS 0
#define FTO_SUPPRESS_CONST 0x1
			/* Suppress top-level "const" on the type. */
#define FTO_SUPPRESS_SPECIFIERS 0x2
			/* Suppress the type specifiers of the type (put out
			   only the declarator). */


extern void clear_il_to_str_output_control_block(
                                   an_il_to_str_output_control_block_ptr octl);

extern void form_a_template_arg(a_template_arg_ptr                    tap,
                                an_il_to_str_output_control_block_ptr octl);

extern void form_template_args(a_template_arg_ptr                    tap,
                               a_template_parameter_ptr              tpp,
                               an_il_to_str_output_control_block_ptr octl);

extern void form_class_or_namespace_qualifier(
                         a_boolean                             is_class_member,
                         a_parent_class_or_namespace           parent,
                         an_il_to_str_output_control_block_ptr octl);

#if MICROSOFT_EXTENSIONS_ALLOWED
void form_property_or_event_name_as_qualifier_if_needed(
                              a_source_correspondence               *scp,
                              an_il_entry_kind                      entry_kind,
                              an_il_to_str_output_control_block_ptr octl);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void form_name(a_source_correspondence               *scp,
                      an_il_entry_kind                      kind,
                      an_il_to_str_output_control_block_ptr octl);

extern
void form_unqualified_name(a_source_correspondence               *scp,
                           an_il_entry_kind                      entry_kind,
                           an_il_to_str_output_control_block_ptr octl);

extern a_const_char *int_kind_name_full(an_integer_kind kind,
                                  a_boolean       for_generated_code);

extern a_const_char *int_kind_name(an_integer_kind kind);

extern a_const_char *int_type_name(a_type_ptr type);

extern a_const_char *float_kind_name(a_float_kind kind,
                                     a_boolean    use_C_form);

extern a_const_char *type_transforming_intrinsic_name(a_typeref_kind kind);

extern void form_type_qualifier(
                     a_type_qualifier_set                  qualifiers,
                     a_upc_block_size                      upc_block_size,
                     a_boolean                             need_trailing_space,
                     an_il_to_str_output_control_block_ptr octl);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_const_char *cli_managed_class_tag_keyword(a_type_ptr type);

extern void form_pointer_modifiers(
                             a_pointer_modifier_set                 modifiers,
                             an_il_to_str_output_control_block_ptr  octl);

extern void form_calling_convention(
                     a_calling_convention                  calling_convention,
                     an_il_to_str_output_control_block_ptr octl);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED
extern a_type_ptr complex_type_needs_modification(
                                                a_type_ptr   orig_type,
                                                a_float_kind *orig_float_kind);
#endif /* GNU_EXTENSIONS_ALLOWED && C99_IL_EXTENSIONS_SUPPORTED */

extern void form_type_first_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    a_boolean                             need_trailing_space,
                    a_type_qualifier_set                  added_qualifiers,
                    a_form_type_options_set               options,
                    an_il_to_str_output_control_block_ptr octl);

/* Macro used to call form_type_first_part for the simple cases. */
#define form_type_first_part_simple(type, lhs_decl, tr_space, octl)   \
  form_type_first_part(type, lhs_decl, tr_space, TQ_NONE, FTO_NO_OPTIONS, octl)

extern void form_function_declarator(
                              a_type_ptr                            type,
                              an_il_to_str_output_control_block_ptr octl);

extern void form_type_second_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    a_form_type_options_set               options,
                    an_il_to_str_output_control_block_ptr octl);

/* Macro used to call form_type_second_part for the simple cases. */
#define form_type_second_part_simple(type, lhs_decl, octl)            \
  form_type_second_part(type, lhs_decl, FTO_NO_OPTIONS, octl)

extern void form_type(a_type_ptr                            type,
                      an_il_to_str_output_control_block_ptr octl);

extern void form_integer_constant(
                           a_constant_ptr                        constant,
                           a_boolean                             suppress_cast,
                           a_boolean                             need_parens,
                           an_il_to_str_output_control_block_ptr octl);

extern int form_char(char                                  ch,
                     an_il_to_str_output_control_block_ptr octl);

extern void form_pm_constant(
                      a_constant_ptr                        constant,
                      a_boolean                             minimal_casts,
                      a_boolean                             need_parens,
                      an_il_to_str_output_control_block_ptr octl);

/* Return TRUE if the given constant has associated information about the
   expression it came from and should be put out in expression form.  Don't
   be fooled by named constants, which also have a non-NULL expression
   pointer if they were given an explicit value in their definitions, but
   shouldn't be put out in expression form.  However, a non-definition
   named constant that has a backing expression is used to preserve the
   form of name reference, so the expression should be used in that
   case. */
#define constant_should_be_put_out_as_expr(constant)                      \
  (((constant)->expr != NULL || constant->local_expr_ref) &&              \
   !(constant)->is_named_constant_definition)

extern void form_uuidof_reference(a_constant_ptr                        con,
                                  an_il_to_str_output_control_block_ptr octl);

extern void form_typeid_reference(a_constant_ptr                        con,
                                  an_il_to_str_output_control_block_ptr octl);

extern void form_unknown_lvalue_constant(
                             a_constant_ptr                        constant,
                             an_il_to_str_output_control_block_ptr octl);

extern void form_reflection(a_reflection_value                     rv,
                            an_il_to_str_output_control_block_ptr  octl);

extern void form_constant(a_constant_ptr                        constant,
                          a_boolean                             need_parens,
                          an_il_to_str_output_control_block_ptr octl);

extern void form_lvalue_address_constant(
                          a_constant_ptr                        constant,
                          a_boolean                             need_parens,
                          an_il_to_str_output_control_block_ptr octl);


#if BACK_END_IS_CP_GEN_BE

/*
Opaque declaration of a_saved_template_param_mapping_ptr to allow
checkpointing mappings for parameters of nested templates and generic
lambdas.
*/
struct a_saved_template_param_mapping;
typedef a_saved_template_param_mapping *a_saved_template_param_mapping_ptr;

extern a_saved_template_param_mapping_ptr save_template_param_mappings(void);

extern
void restore_template_param_mappings(a_saved_template_param_mapping_ptr);

extern void remap_template_param(a_template_param_coordinate_ptr  coord,
                                 a_source_correspondence_ptr      scp);

extern void unmap_template_param(a_template_param_coordinate_ptr  coord);

#endif /* BACK_END_IS_CP_GEN_BE */

extern an_expr_node_ptr decltype_arg(a_type_ptr  type);

#if PROTOTYPE_INSTANTIATIONS_IN_IL
extern a_source_correspondence_ptr source_corresp_for_template_param(
                                        a_template_param_coordinate_ptr coord);
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */

extern a_boolean form_alignas_attributes(
                      an_attribute_ptr                      ap,
                      a_boolean                             need_leading_space,
                      an_il_to_str_output_control_block_ptr octl);

#if GNU_EXTENSIONS_ALLOWED
#if GNU_VECTOR_TYPES_ALLOWED
extern Small_string<16> get_name_for_riscv_vector_type(
                                                    a_const_char  *name_prefix,
                                                    a_type_ptr    vector_type);
#endif /* GNU_VECTOR_TYPES_ALLOWED */

#if BACK_END_IS_C_GEN_BE
extern a_boolean form_type_attributes(
                   a_type_ptr                             type,
                   a_boolean                              need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl);

#if GNU_VECTOR_TYPES_ALLOWED
extern void form_vector_type_attribute(
                     a_type_ptr                            type,
                     a_boolean                             *need_leading_space,
                     an_il_to_str_output_control_block_ptr octl);
#endif /* GNU_VECTOR_TYPES_ALLOWED */

extern a_boolean form_variable_attributes(
                   a_variable_ptr                         var,
                   a_boolean                              need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl);

extern a_boolean form_field_attributes(
                   a_field_ptr                            field,
                   a_boolean                              need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl);

extern a_boolean form_routine_attributes(
                   a_routine_ptr                          rout,
                   a_boolean                              need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl);

extern a_boolean form_label_attributes(
                   a_label_ptr                            label,
                   a_boolean                              need_leading_space,
                   an_il_to_str_output_control_block_ptr  octl);
#endif /* BACK_END_IS_C_GEN_BE */

#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
extern void form_asm_name(a_const_char                           *asm_name,
                          an_il_to_str_output_control_block_ptr  octl);

extern void form_var_reg_name(a_named_register                       reg,
                              an_il_to_str_output_control_block_ptr  octl);

#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
#endif /* GNU_EXTENSIONS_ALLOWED */

#if SUN_EXTENSIONS_ALLOWED
#if (BACK_END_IS_C_GEN_BE && C_GEN_BE_GENERATES_ANSI_C) || \
    BACK_END_IS_CP_GEN_BE
extern void form_sun_link_scope_specifiers(
                                 a_decl_modifier_set                    flags,
                                 an_il_to_str_output_control_block_ptr  octl);
#endif /* (BACK_END_IS_C_GEN_BE && C_GEN_BE_GENERATES_ANSI_C) || ... */
#endif /* SUN_EXTENSIONS_ALLOWED */

#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE

using a_temp_var_name_buffer = Small_string<50>;
                        /* The type used for the name of a temporary variable
                           created for use in the C or C++-generating back
                           ends. */

extern a_temp_var_name_buffer form_temporary_name_for_back_end(void *ptr);

extern void il_to_str_back_end_file_init();

#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */

#if DEBUG
/* GCOVR_EXCL_START */
void db_abbr_expr(an_expr_node_ptr                       expr,
                  an_il_to_str_output_control_block_ptr  octl);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

extern void il_to_str_one_time_init(void);
extern void il_to_str_init(void);

EXTERN_THREAD a_boolean
		gen_edg_special_types;
			/* If TRUE, use the __edg_vector_type__ keyword to
			   render vector types (instead of a GNU attribute)
			   and __edg_fp16__ rather than its underlying
			   type.  Used when generating the builtin table. */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef IL_TO_STR_H */


