/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

overload.h -- Declarations related to expression overload resolution.

*/

/* Avoid including these declarations more than once: */
#ifndef OVERLOAD_H
#define OVERLOAD_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */
#ifndef EXPRUTIL_H
#include "exprutil.h"
#endif /* ifndef EXPRUTIL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/* Deal with forward reference: */
typedef struct a_candidate_function *a_candidate_function_ptr;

/*
Description of a complete conversion (user-defined part plus standard
conversion part).  Really, the interesting information about that
conversion which must be kept around, which is not really a full
description.  Can describe no conversion.  Also used to describe a bitwise
copy of a class in C or C++, which is not really a conversion.
If this entry is used in describing a conversion that is part of a
reference initialization, the conversion is the one done on the initial
value to bring it to the underlying type of the reference, not a
conversion to the reference type itself.
*/
typedef struct a_conv_descr *a_conv_descr_ptr;
typedef struct a_conv_descr {
  a_routine_ptr	routine;
			/* The conversion routine entry.  NULL if
			   class_identity_or_bitwise_copy or
			   unknown_dependent_conversion is TRUE or if there
			   is no user-defined part of the conversion. */
  a_symbol_ptr	routine_symbol;
			/* Non-NULL only for conversion functions, in which
			   case it is the symbol for the routine, possibly
			   a projection symbol.  Needed to check access on
			   conversion function calls. */
  a_bit_field	class_identity_or_bitwise_copy:1;
			/* If TRUE, the "conversion" for a class is a bitwise
			   copy (possibly from a derived class to a base
			   class).  If this is viewed as a conversion instead
			   of a copy, it looks like an identity conversion
			   or a derived-to-base conversion, with possible
			   cv-qualifier adjustment.  However: this flag is
			   set only for cases where bitwise copy is the
			   appropriate semantics, and is never used for
			   general type adjustments on class objects (see
			   class_object_adjustment_required).  routine is
			   always NULL when this flag is set. */
  a_bit_field	should_elide_ctor:1;
			/* If TRUE, the constructor call for the conversion
			   can be elided. */
  a_bit_field	result_is_a_glvalue:1;
			/* If TRUE, the function returns a reference and the
			   reference should be left as a glvalue rather than
			   converted to a prvalue.  If FALSE, the result is
			   a prvalue (either originally or after a
			   glvalue-->prvalue conversion).  Note that this
			   is meaningful even when the entry indicates no
			   conversion. */
  a_bit_field	unusable:1;
			/* If TRUE the conversion is unusable, e.g., it is
			   ambiguous. */
  a_bit_field	class_object_adjustment_required:1;
			/* If TRUE, a class object requires a type adjustment
			   of cv-qualifiers or (when std.cast_base_class is
			   non-NULL) to a base class.  The existing class
			   object is treated as having the new type; no copy
			   is made.  This adjustment may be needed on the
			   original source (routine == NULL) or on the result
			   of a conversion routine call (routine != NULL).
			   The adjustment is similar to a standard conversion,
			   but it isn't considered to be one: the standard
			   views it as part of reference binding. */
  a_bit_field	conversion_for_direct_reference_binding:1;
			/* If TRUE, the conversion indicated is one that
			   converts an initializer using a conversion function
			   that returns a reference in order to produce an
			   lvalue that a reference can be directly bound to. */
  a_bit_field	copy_initialization_done_as_direct:1;
			/* If TRUE, the conversion is a copy initialization
			   that, following the rules in [dcl.init] of the
			   C++ standard, is done as if it were a direct
			   initialization. */
  a_bit_field	user_conversion_for_class_copy_must_be_determined:1;
			/* If TRUE, this is a class value being passed to
			   a parameter of the same type, or a base class type.
			   This is seen as a standard conversion of sorts
			   in overload resolution, but the constructor
			   to be called must be determined once it's known
			   that this conversion will be used. */
  a_bit_field	unknown_dependent_conversion:1;
			/* If TRUE, the conversion is from or to a template
			   dependent type in a prototype instantiation, and
			   therefore we cannot know the right constructor
			   or conversion function to call. */
  a_bit_field	is_explicit_cast:1;
			/* If TRUE, the conversion is being done as the
			   result of an explicit cast. */
  a_bit_field	is_base_init:1;
			/* If TRUE, the conversion is being done as the result
			   of a ctor-initializer for a base subobject. */
  a_std_conv_descr
		std;	/* The standard conversion part of the conversion. */
} a_conv_descr;

#define clear_conv_descr(p_conv_descr)                           \
  (memzero((char*)(p_conv_descr), sizeof(a_conv_descr)))


/*
Macro that tests for a conversion description with a null user-defined
part.
*/
#define is_null_user_conv_descr(user_conversion)                      \
  ((user_conversion)->routine == NULL &&                              \
   !(user_conversion)->class_identity_or_bitwise_copy &&              \
   !(user_conversion)->unknown_dependent_conversion)

/*
Macro that returns TRUE if a pointer to a conversion is usable (the
pointer is non-NULL, and the conversion is not unusable).
*/
#define conv_usable(conversion)                             \
  ((conversion) != NULL && !(conversion)->unusable && \
   !(conversion)->user_conversion_for_class_copy_must_be_determined)



/*
Bit flags used to indicate the kinds of built-in types allowed
for try_to_convert_class_operand_to_builtin_type and
conversion_from_class_possible.
*/
#define BTK_INTEGRAL 0x1
			/* Any integral type other than bool (includes enum in
			   C but not in C++). */
#define BTK_FLOATING 0x2
			/* Any floating type. */
#define BTK_POINTER 0x4
			/* Any pointer. */
#define BTK_POINTER_TO_OBJECT 0x8
			/* Any pointer to object type.  Note that the C++
			   standard allows incomplete object types. */
#define BTK_POINTER_TO_FUNCTION 0x10
			/* Any pointer to function. */
#define BTK_PTR_TO_MEMBER 0x20
			/* Any pointer to member. */
#define BTK_BOOL 0x40
			/* bool (C++).  Note that a conversion to BTK_BOOL
			   alone is the C++11 "contextually converted to bool"
			   conversion, which allows explicit conversion
			   functions. */
#define BTK_ENUM 0x80
			/* Enumeration types in C++ (in C, they're
			   integral). */
#define BTK_UNSCOPED_ENUM 0x100
			/* Unscoped enumeration types in C++. */
#define BTK_SCOPED_ENUM 0x200
			/* Scoped enumeration types in C++. */
#define BTK_PTRDIFF_T 0x400
			/* ptrdiff_t. */
#define BTK_SIZE_T 0x800
			/* size_t. */
#if MICROSOFT_EXTENSIONS_ALLOWED
#define BTK_HANDLE 0x1000
			/* Any C++/CLI handle type. */
#define BTK_HANDLE_TO_CLI_ARRAY 0x2000
			/* Handle to a CLI array type. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#define BTK_NULLPTR_T 0x4000
			/* decltype(nullptr), aka. std::nullptr_t. */
#define BTK_NONE 0
typedef int a_builtin_type_kind_set;

extern a_const_char *name_for_builtin_type_kind(
                                        a_builtin_type_kind_set builtin_types);

/*
Data structure used by set_up_overload_set_traversal et al. to control the
traversal of an overload set to produce a sequence of symbols to be
tried in overload resolution.
*/
typedef struct an_overload_set_traversal_block {
  a_symbol_ptr	current_symbol;
			/* The symbol currently being considered. */
  a_symbol_list_entry_ptr
		current_symbol_list_entry;
			/* When a symbol list is being traversed, this points
			   to the current entry on the symbol list and
			   current_symbol and is_overloaded_function_list
			   are not used. */
  a_byte_boolean
		is_overloaded_function_list;
			/* TRUE if the list being traversed is the list
			   under an sk_overloaded_function symbol. */
  a_candidate_function_ptr
		*candidate_functions;
			/* Pointer to the list of candidate functions being
			   built up.  NULL if there is no list of candidate
			   functions. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_candidate_function_ptr
		candidate_functions_on_prev_iteration;
			/* The value of *candidate_functions on the previous
			   iteration, used to note whether anything has been
			   added to the candidate set (assuming additions are
			   made at the front of the list).  Meaningful only
			   if candidate_functions is non-NULL. */
  a_hide_by_sig_list_entry_ptr
		hide_by_sig_list;
			/* For a C++/CLI hide-by-sig name, the list of
			   symbols to be considered, in order.  NULL
			   otherwise. */
  a_byte_boolean
		skip_inaccessible_functions;
			/* TRUE if inaccessible functions on the list should
			   be skipped.  Used in some C++/CLI contexts. */
  a_byte_boolean
		any_inaccessible_function_skipped;
			/* Set to TRUE if any inaccessible function was
			   skipped. */
  a_byte_boolean
		curr_sym_viable;
			/* For internal use.  Indicates that the symbol
			   returned on the previous iteration was found to
			   be viable.  Normally set by checking
			   candidate_functions_on_prev_iteration, but can
			   also be set for some internal purposes. */
  a_byte_boolean
		returned_sym_is_inaccessible;
			/* The symbol returned on the previous iteration
			   was inaccessible, and was returned anyway in
			   order to look for inaccessible symbols that are
			   viable, for better diagnostics. */
  a_symbol_ptr	*inaccessible_match;
			/* In C++/CLI mode, if non-NULL, used to return the
			   symbol for a candidate function that would have
			   been viable except that it is inaccessible, when
			   hide-by-sig lookup applies.  Only the first such
			   symbol is returned.  If this field is NULL, such
			   functions are not looked for. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} an_overload_set_traversal_block;

/*
Argument match levels for overloaded function call resolution; See ARM 13.2.
*/
enum an_arg_match_level {
  aml_exact,		/* Exact match or trivial conversions. */
  aml_promotion,	/* Match with promotions. */
  aml_std_conversion,	/* Match with standard conversions. */
  aml_boxing_conversion,/* Match with C++/CLI boxing. */
  aml_user_conversion,	/* Match with user-defined conversions. */
  aml_ellipsis,		/* Match with ellipsis. */
  aml_error,		/* Match with error type (not in ARM). */
  aml_none		/* No match.  Must be last (highest value). */
};

/*
Entry describing how well an actual argument to a function call matches
the corresponding formal parameter.
*/
typedef struct an_arg_match_summary *an_arg_match_summary_ptr;
typedef struct an_arg_match_summary {
  an_arg_match_summary_ptr
		next;	/* Pointer to entry for following argument, or NULL
			   if this is the last argument. */
  an_arg_match_level
		match_level;
			/* Match level -- see ARM 13.2. */
  a_byte_boolean
		anachronism_used;
			/* TRUE if the match was possible only because an
			   anachronism was used.  This acts like a very
			   weak match level, worse than all others (so,
			   effectively, match_level means nothing).  Also,
			   a candidate function with an anachronism match
			   will not be considered unless there are no other
			   viable candidates without anachronism matches.
			   An entry with this flag set should never change
			   the outcome of overload resolution relative to
			   the standard unless no function would have applied
			   under the standard rules, so this allows
			   anachronisms to interact well with
			   standard-conforming code. */
  a_byte_boolean
		tiebreaker_anachronism_used;
			/* TRUE if the match was possible only because of an
			   anachronism, but the anachronism is a very weak
			   tie-breaker; it will only allow choosing one
			   candidate over another if they are otherwise
			   considered identical.  match_level, therefore,
			   still matters, and nonstandard results are
			   possible.  This is used for a number of
			   Microsoft bug emulations. */
  a_byte_boolean
		const_anachronism;
			/* In cfront compatibility mode, TRUE to indicate
			   that the match was possible only because of the
			   anachronism that allows a non-const function to
			   be called for a const object.  If this is set, the
			   anachronism_used flag will also be set. */
  a_byte_boolean
		is_match_for_this_param;
			/* TRUE if this entry describes the match for the
			   "this" parameter. */
  a_byte_boolean
		arg_is_constant;
			/* TRUE if the corresponding argument is a constant.
			   This is used for a Microsoft-mode test. */
  a_byte_boolean
		lvalue_to_rvalue_conversion_used;
			/* TRUE if the argument was converted from an lvalue
			   to an rvalue.  This follows the C++ standard
			   definition, which includes function --> pointer
			   and array --> pointer. */
  a_byte_boolean
		on_conv_allow_any_cv_qual_on_ptr;
			/* TRUE if in looking for the user-defined conversion
			   we wanted a conversion to a pointer type but were
			   willing to accept any cv-qualification on the
			   underlying type. */
  a_byte_boolean
		function_lvalue_bound_to_rvalue_ref;
			/* TRUE if a function lvalue is bound to an rvalue
			   reference (which is allowed in standard C++, but a
			   lesser match than the same lvalue bound to an lvalue
			   reference). */
  a_ref_qualifier_kind
		ref_qualifier;
			/* When is_match_for_this_param is TRUE, this gives
			   the ref-qualifier value ("&", "&&", or none)
			   for the called function. */
  uint32_t	param_num;
			/* The parameter number of the parameter matched
			   against the argument.  Zero for the "this"
			   parameter. */
  a_type_ptr	param_type;
			/* The type of the parameter.  Used in looking
			   for conversion subsequences involving addition
			   of type qualifiers at the end of a conversion.
			   NULL if not applicable (e.g., for an ellipsis). */
  a_type_ptr	guide_type;
			/* For operands of builtin operators, the type
			   passed as a guiding type when trying to do the
			   conversion from a class type.  This generally
			   comes from the type of the other operand, and
			   controls template conversion operator
			   applicability. */
  a_conv_descr	conversion;
			/* Description of the conversion to be done (really,
			   information we wanted to remember about the
			   conversion to be done).  Contains a user-defined
			   conversion part and a standard conversion part.
			   If match_level is aml_user_conversion, this
			   describes a user-defined conversion.
			   Note that in one case involving cfront
			   compatibility, no user-defined conversion is
			   indicated even through match_level is
			   aml_user_conversion.  Also, when passing a
			   class that requires a copy constructor by value,
			   the user-defined conversion will indicate the
			   copy constructor even though the match level
			   is aml_std_conversion or aml_exact. */
  a_symbol_ptr	template_symbol;
			/* In the case that the argument is a template function
			   or a pointer or pointer-to-member thereto, this
			   points to the template.  NULL otherwise. */
} an_arg_match_summary;


/*
Entry describing a function that is a candidate instance of an overloaded
function.  This entry is used in resolving overloaded function calls.
*/
typedef struct a_candidate_function {
  a_candidate_function_ptr
		next;	/* Next entry on the list of candidates, or NULL
			   if this is the last entry. */
  a_symbol_ptr	function_symbol;
			/* Pointer to the symbol for the function.  NULL if
			   the "function" is a built-in operator or for a
			   surrogate function case.  This can be a projection
			   symbol, but once overload resolution succeeds it
			   may point to a specific template instance even
			   when the template was found through a projection
			   symbol. */
  a_symbol_ptr	best_proj_function_symbol;
			/* If overload resolution succeeds and causes
			   function_symbol to point to a specific instance of
			   a template, this points to the symbol that was
			   found: Either a function template symbol or a
			   projection symbol for such a template.  Otherwise,
			   this ends up equal to function_symbol.  (This
			   matters when performing access checking, which
			   should be based on the symbol that was found and
			   not the specific instance symbol.) */
  a_symbol_ptr	overloaded_function_symbol;
			/* The overloaded function symbol we started from
			   that contains function_symbol.  Can be a
			   projection symbol, or a non-overloaded symbol.
			   Can be NULL if not applicable (e.g., when
			   function_symbol is a conversion function). */
  a_byte_boolean
		is_function_template;
			/* TRUE if function_symbol is a function template. */
  a_byte_boolean
		expl_template_arg_list_used;
			/* TRUE if an explicit template argument list was
			   used (e.g., f<int>). */
  a_template_arg_ptr
		template_arg_list;
			/* If is_function_template is TRUE, this points to
			   the list of template arguments.  The list is
			   complete, through some combination of explicit
			   specification and deduction. */
  a_const_char	*operand_type_pattern;
			/* For a built-in operator, the operand type pattern
			   string (see operand_type_pattern_for_operator).
			   Specifically, the appropriate one- or two-character
			   segment of the operand pattern string.  NULL
			   if not a built-in operator. */
  a_symbol_ptr	surrogate_function_conv_sym;
			/* Non-NULL for a surrogate function case
			   (function_symbol will be NULL).  Points to the
			   symbol for the conversion function that will
			   yield a pointer to the surrogate function to be
			   called. */
  a_conv_descr	conversion;
			/* If is_user_conversion is TRUE. description of the
			   conversion being done, including the user-defined
			   part. */
  a_type_ptr	specific_type;
			/* For a built-in operator with an operand pattern
			   including corresponding types (e.g., pointer types),
			   this indicates the specific type.  For a function
			   template candidate, this indicates the deduced type
			   (if available). */
  an_arg_match_summary_ptr
		arg_matches;
			/* List of entries describing how well each actual
			   argument matches this function's corresponding
			   formal parameter.  If there was a selector object
			   (for the "this" parameter), it appears first. */
  /* Fields used by select_best_candidate_functions: */
  an_arg_match_summary_ptr
		current_arg_match;
			/* The argument match entry currently being considered
			   by select_best_candidate_functions. */
  a_candidate_function_ptr
		next_in_arg_best_match_set;
			/* If non-NULL, points to the next candidate function
			   that's in the set of best matches for the argument
			   currently being examined. */
  an_opname_kind
		opname_kind;
			/* If this represents a built-in operator, the operator
			   kind. */
  a_bit_field	supplemental_comparison_candidate:1;
			/* If TRUE, this is an `operator<=>` or `operator==`
			   candidate added to implement the C++20 rules
			   for a relational or equality operator (see N4810
			   [over.match.oper]/3, bullet (3.4)). */
  a_bit_field	supplemental_reversed_candidate:1;
			/* If TRUE, this is an `operator<=>` or `operator==`
			   candidate added to implement the C++20 rules
			   for a comparison operator, where the order of the
			   operands are reversed (see N4810 (see N4810
			   [over.match.oper]/3, bullet (3.4)). */
  a_bit_field	uses_microsoft_explicit_anachronism:1;
			/* If TRUE, this function is an explicit constructor
			   that should not have been seen but was considered
			   viable because of a Microsoft bug. */
  a_bit_field	init_list_ctor_case:1;
			/* If TRUE, this candidate was matched by using a
			   braced-init-list as a single argument instead of
			   using the elements of the list as the arguments,
			   when matching an initializer-list constructor in
			   a special way (see [over.match.list] in the C++11
			   standard). */
  a_bit_field	gpp_init_list_ctor_param_case:1;
			/* TRUE if this is a viable constructor candidate with
			   a single std::initializer_list parameter in GNU
			   C++ mode.  GCC appears to discard candidates that
			   do not have this property in some cases. */
  a_bit_field	is_inheriting_ctor:1;
			/* If TRUE, this candidate is an inheriting
			   constructor. */
  a_bit_field	is_user_conversion:1;
			/* TRUE if this function is a user-defined conversion
			   being examined to resolve an implicit conversion.
			   The field "conversion" is meaningful in that case.
			   This will have the same setting in all candidate
			   function entries being considered as a set. */
  a_bit_field	user_conv_with_matching_func_ref_kinds:1;
			/* TRUE if is_user_conversion is TRUE and the
			   conversion is for a function reference binding where
			   the destination reference matches the reference kind
			   to which the conversion function converts. */
  a_bit_field	in_best_match_set:1;
			/* TRUE if the function is in the set of best-matching
			   functions. */
  a_bit_field	in_best_match_set_for_some_argument:1;
			/* TRUE if the function is in the set of best-matching
			   functions for some argument. */
  a_bit_field	in_best_match_set_for_curr_argument:1;
			/* TRUE if the function is in the set of best-matching
			   functions for the current argument. */
#if BACK_END_IS_CP_GEN_BE
  a_bit_field	found_through_adl:1;
			/* TRUE if the function was found through argument-
			   dependent lookup */
#endif /* BACK_END_IS_CP_GEN_BE */
} a_candidate_function;

#define clear_candidate_function(p_cand)                           \
  (memzero((char*)(p_cand), sizeof(a_candidate_function)))


EXTERN_THREAD a_candidate_function_ptr
		avail_candidate_functions;
			/* List of candidate function entries that have been
			   freed and are available for reuse. */
EXTERN_THREAD an_arg_match_summary_ptr
		avail_arg_match_summaries;
			/* List of argument match summary entries that have
			   been freed and are available for reuse. */

/*
A type describing an entry in the overload resolution stack.
*/
struct an_ovl_resolution_descr {
  inline an_ovl_resolution_descr();
  inline ~an_ovl_resolution_descr()
    { if (this->noted_candidates != NULL) delete_fe(&this->noted_candidates); }
  a_bit_field   emit_note_diagnostics:1;
                        /* TRUE if we're in the "note processing" pass.
                           Currently only set in the top-most overload (i.e.,
                           the bottom of the stack). */
  a_bit_field   in_comparison_rewrite:1;
                        /* TRUE if we are completing a "comparison rewrite"
                           (which inhibits recursive rewrites). */
  a_bit_field   constraint_failure:1;
                        /* TRUE if we encountered a constraint failure while
                           recording notes (reset once the associated
                           candidate is rejected). */
  a_symbol_ptr  curr_candidate = NULL;
                        /* The candidate being considered at this level of
                           the current overload resolution stack. */
  a_template_arg_ptr
                curr_template_args = NULL;
                        /* When checking for constraints when curr_candidate
                           is a function template, the template arguments to
                           be applied to that candidate. */
  a_diag_list   notes = { NULL, NULL };
                        /* Notes attached to the principal diagnostic
                           adding details about various failures. */
  a_diagnostic_ptr
                curr_diagnostic = NULL;
                        /* A pointer into notes (or NULL) to indicate where
                           bottom-up notes should be spliced in. */
private:
  Ptr_set<a_symbol_ptr>
                *noted_candidates = NULL;
                        /* The set of candidates that have notes associated
                           with them.  (Used to avoid duplicate notes.) */
  friend a_boolean candidate_already_noted(a_symbol_ptr  sym);
}  /* an_ovl_resolution_descr */;


inline an_ovl_resolution_descr::an_ovl_resolution_descr()
/*
Constructor for an_ovl_resolution_descr.
*/
  : emit_note_diagnostics(FALSE)
  , in_comparison_rewrite(FALSE)
  , constraint_failure(FALSE)
  , notes{ NULL, NULL }
{
}  /* an_ovl_resolution_descr::an_ovl_resolution_descr */

namespace detail {

using an_ovl_res_stack_backing_array =
                Small_dyn_array<an_ovl_resolution_descr, 25>;
                        /* The array type that is the underlying implementation
                           type for an_ovl_res_stack. */

}  /* namespace detail */

using an_ovl_res_descr_ptr =
		Array_ptr<detail::an_ovl_res_stack_backing_array>;
			/* A pointer type used to reference elements within the
			   overload resolution stack (even if the backing
			   storage is reallocated). */

using a_const_ovl_res_descr_ptr =
		Array_ptr<const detail::an_ovl_res_stack_backing_array>;
			/* A pointer type used to reference const elements
			   within the overload resolution stack (even if the
			   backing storage is reallocated). */

/*
An overload resolution stack.  Uses Dyn_array to maintain a "stack" of entries
for each overload level.
*/
struct an_ovl_res_stack {
  an_ovl_res_stack() = default;
  an_ovl_res_stack(const an_ovl_res_stack&) = delete;
  inline an_ovl_res_stack(an_ovl_res_stack&&);

  void push()
    { this->underlying_array.push_back(an_ovl_resolution_descr{}); }
  void pop()
    { this->underlying_array.pop_back(); }

  inline an_ovl_res_descr_ptr top();
  inline a_const_ovl_res_descr_ptr top() const;
  inline an_ovl_res_descr_ptr bottom();
  inline a_const_ovl_res_descr_ptr bottom() const;

  a_boolean is_empty() const
    { return this->underlying_array.length() == 0; }
  size_t overload_level() const
    { return this->underlying_array.length(); }
  a_boolean has_multiple_levels() const
    { return this->underlying_array.length() > 1; }
  a_boolean emit_note_diagnostics() const
    { return !this->is_empty() && this->bottom()->emit_note_diagnostics; }
  a_boolean note_reporting_pass_needed();
private:
  detail::an_ovl_res_stack_backing_array
                underlying_array;
                        /* The array providing the storage backing this
                           overload resolution stack. */
};  /* an_ovl_res_stack */


inline an_ovl_res_stack::an_ovl_res_stack(an_ovl_res_stack &&other)
/*
Move constructor.
*/
: underlying_array(move_from(&(other.underlying_array)))
{
}  /* an_ovl_res_stack::an_ovl_res_stack */


inline an_ovl_res_descr_ptr an_ovl_res_stack::top()
/*
Return a pointer to the top overload resolution descriptor on this stack.
*/
{
  check_assertion(!this->is_empty());
  return an_ovl_res_descr_ptr(&this->underlying_array,
                              this->underlying_array.length() - 1);
}  /* an_ovl_res_stack::top */


inline a_const_ovl_res_descr_ptr an_ovl_res_stack::top() const
/*
Return a pointer to a const view of the top overload resolution descriptor on
this stack.
*/
{
  check_assertion(!this->is_empty());
  return a_const_ovl_res_descr_ptr(&this->underlying_array,
                                   this->underlying_array.length() - 1);
}  /* an_ovl_res_stack::top */


inline an_ovl_res_descr_ptr an_ovl_res_stack::bottom()
/*
Return a pointer to the bottom overload resolution descriptor on this stack.
*/
{
  check_assertion(!this->is_empty());
  return an_ovl_res_descr_ptr(&this->underlying_array, 0);
}  /* an_ovl_res_stack::bottom */


inline a_const_ovl_res_descr_ptr an_ovl_res_stack::bottom() const
/*
Return a pointer to a const view of the bottom overload resolution descriptor
on this stack.
*/
{
  check_assertion(!this->is_empty());
  return a_const_ovl_res_descr_ptr(&this->underlying_array, 0);
}  /* an_ovl_res_stack::bottom */


inline a_boolean an_ovl_res_stack::note_reporting_pass_needed(void)
/*
Returns TRUE if a second, error reporting pass, is needed at this level
of overload.
*/
{
  a_boolean result = FALSE;

  if (!expr_error_should_be_issued()) {
    /* If we're not producing diagnostics, no pass is needed. */
  } else if (this->has_multiple_levels()) {
    /* Re-processing of overloads is only triggered at the top-most level. */
  } else if (this->underlying_array.front_elem().emit_note_diagnostics) {
    /* Just finished emitting errors, so we're done. */
  } else {
    /* An error processing pass is needed. */
    this->underlying_array.front_elem().emit_note_diagnostics = TRUE;
    result = TRUE;
  }  /* if */
  return result;
}  /* note_reporting_pass_needed */


extern an_ovl_res_stack* ovl_res_stack();

inline an_ovl_res_descr_ptr ovl_res_descr()
/*
Return the current overload resolution description or NULL if there is none.
*/
{
  an_ovl_res_descr_ptr  result;
  an_ovl_res_stack      *stack = ovl_res_stack();

  if (stack != NULL && !stack->is_empty()) {
    result = stack->top();
  }  /* if */
  return result;
}  /* ovl_res_descr */

extern void push_new_ovl_res_stack();

extern void pop_cur_ovl_res_stack();

inline a_diag_list* current_ovl_res_notes()
/*
Return the notes associated with a diagnostic at the current overload
resolution level.
*/
{
  return &(ovl_res_stack()->top()->notes);
}  /* current_ovl_res_notes */


inline a_boolean candidate_already_noted(a_symbol_ptr  sym)
/*
Return whether in the current level of overload resolution, the given
symbol already has a note associated with it.  If the result is FALSE,
record the candidate as now having an associated note (the expectation
is that the caller is about to record that note).
*/
{
  an_ovl_res_descr_ptr  descr = ovl_res_stack()->top();
  Ptr_set<a_symbol_ptr> *sym_set = descr->noted_candidates;
  a_boolean             result;

  if (sym_set == NULL) {
    /* The set of candidates is created on-demand. */
    result = FALSE;
    sym_set = new_fe<Ptr_set<a_symbol_ptr>>(3u);
    descr->noted_candidates = sym_set;
  } else {
    result = sym_set->contains(sym);
  }  /* if */
  if (!result) {
    sym_set->add(sym);
  }  /* if */
  return result;
}  /* candidate_already_noted */


#if DEBUG
/* GCOVR_EXCL_START */
/*
Counts of entries allocated, for debugging purposes.
*/
EXTERN_THREAD unsigned long
		num_candidate_functions_allocated;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */


/*
Enumeration indicating the state on return from a scan of a printf format
string.
*/
enum a_printf_scan_state {
  pss_new_specifier,	/* Look for new specifier next time. */
  pss_after_field_width,/* Start after field width next time. */
  pss_after_precision	/* Start after precision next time. */
};


/*
Block of information used to check correspondence of a sequence of call
arguments against a sequence of function parameters.
*/
typedef struct an_arg_check_block {
  a_routine_ptr	routine;
			/* The routine being called, if known.  NULL otherwise,
			   e.g., for a call through a function pointer. */
  a_param_type_ptr
		curr_param_type;
			/* The current parameter type entry, if there is one;
			   NULL otherwise. */
  a_byte_boolean
		unknown_dependent_function;
			/* TRUE if we don't know the routine type because
			   we're in a prototype instantiation and the
			   function to be called is given by a
			   template-dependent expression. */
  a_byte_boolean
		args_will_be_discarded;
			/* TRUE if the arguments will be discarded, e.g.,
			   because the function operand has an error.
			   More precisely, we won't be calling a function
			   with these arguments, but we may assemble them into
			   an argument list for error recovery purposes. */
  a_byte_boolean
		have_param_info;
			/* TRUE if we have information on the remaining
			   parameters.  Can be FALSE because
			     (a) The called function has a bad type;
			     (b) The called function has an old-style
			         parameter list and no body (hence no
			         parameter declarations);
			     (c) We're in the ellipsis section of a prototyped
			         function call (including a printf/scanf-type
			         routine);
			     (d) We're in the varargs section of an old-style
			         function call;
			     (e) We're scanning extra arguments after issuing
			         an error about there being too many arguments;
			         or
			     (f) We're scanning the arguments for an
			         overloaded function call or a
			         template-dependent call. */
  a_byte_boolean
		prototyped;
			/* TRUE if the function is prototyped. */
  a_byte_boolean
		has_ellipsis;
			/* TRUE if the function has an ellipsis. */
  a_byte_boolean
		pack_encountered;
			/* TRUE if in scanning parameters and arguments we
			   have encountered either a parameter pack parameter
			   or a pack expansion argument.  In either case, we
			   can no longer maintain the correspondence between
			   parameters and arguments. */
  a_byte_boolean
		discard_further_arguments;
			/* TRUE if additional arguments should be discarded
			   because of certain errors. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_byte_boolean
		passing_cli_param_array_element;
			/* TRUE if the function has a C++/CLI parameter array
			   at the end and we are in the part of the argument
			   list where arguments correspond to elements in the
			   parameter array. */
  a_type_ptr	cli_param_array_element_type;
			/* When passing_cli_param_array_element is TRUE, the
			   type of the elements of the parameter array, i.e.,
			   the effective parameter type. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_pragma_kind	arg_list_kind;
			/* The kind of any pragma that applies to the
			   parameter list. */
  int		varargs_count;
			/* The argument count for the varargs lint comment. */
  int		arg_ctr;
			/* The current argument number. */
  an_expr_node_ptr
		argument_head;
			/* The head of the list of argument expressions
			   collected so far. */
  an_expr_node_ptr
		argument_tail;
			/* The tail of the list of argument expressions
			   collected so far. */
#if GNU_EXTENSIONS_ALLOWED
  int		fmt_arg;
			/* If arg_list_kind is pk_printf_args or
			   pk_scanf_args, the argument number
			   containing the format string, or zero if
			   the format string is the last argument
			   before the ellipsis. */
  int		format_first_subst_arg;
			/* If arg_list_kind is pk_printf_args or
			   pk_scanf_args, contains the value of the third
			   argument to "format" (i.e., the argument position
			   of the start of the actual arguments being used
			   to fill out the format string).  A value of zero
			   indicates that the arguments start immediately
			   after an ellipsis (typically the case when a
			   pragma has been used instead of the "format"
			   attribute). */
  int		sentinel_pos;
			/* Argument position (counted backward from the last
			   argument, which is number one) of a sentinel: The
			   argument at that position must be a constant null
			   pointer (zero cast to a pointer type).  Zero
			   indicates no sentinel position is defined.  A non-
			   zero value is only possible when has_ellipsis is
			   TRUE. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  an_arg_list_elem_ptr
		printf_scanf_args;
			/* A pointer to a list of operands passed as the
			   ellipsis arguments for a printf/scanf-like
			   function. */
  a_const_char	*fmt_string;
			/* When checking a printf- or scanf-like function,
			   points to the format string.  NULL otherwise. */
  a_source_position
		closing_paren_position;
			/* Source position of the closing parenthesis of
			   the call. */
} an_arg_check_block;


extern a_symbol_ptr set_up_overload_set_traversal(
                          a_symbol_ptr                    sym,
                          a_candidate_function_ptr        *candidate_functions,
                          a_symbol_ptr                    *inaccessible_match,
                          an_overload_set_traversal_block *ostblock);

#define set_up_overload_set_traversal_simple(sym, ostblock) \
  (set_up_overload_set_traversal((sym), \
                                 (a_candidate_function **)NULL, \
                                 (a_symbol **)NULL, \
                                 (ostblock)))

extern a_symbol_ptr next_symbol_in_overload_set(
                                    an_overload_set_traversal_block *ostblock);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean hide_by_sig_lookup_applies(a_symbol_ptr sym);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void display_object_type(a_type_ptr       object_type,
                                a_diagnostic_ptr dp);

extern void clear_arg_match_summary(an_arg_match_summary_ptr amsp);

extern void free_arg_match_summary_list(an_arg_match_summary_ptr amsp);

extern a_type_ptr operand_complete_object_type(an_operand *operand,
                                               a_boolean  call_case);

extern void force_complete_type_if_a_variable(an_operand *operand);

extern void issue_warning_from_arg_match_summary(
                                            an_arg_match_summary_ptr amsp,
                                            a_source_position        *err_pos);

extern a_symbol_ptr find_addr_of_overloaded_function_match(
                                a_symbol_ptr       ovl_sym,
                                a_boolean          is_template_id,
                                a_template_arg_ptr template_arg_list,
                                a_boolean          source_is_lvalue,
                                a_type_ptr         dest_type,
                                a_boolean          is_cast,
                                a_boolean          is_static_cast,
                                an_arg_match_level *match_level,
                                a_std_conv_descr   *std_conv,
                                a_boolean          *reinterpret_semantics,
                                a_boolean          *unknown_dependent_function,
                                a_boolean          *ambiguous);

extern void choose_function_and_make_address_constant(
                                             a_symbol_ptr   sym,
                                             a_boolean      is_template_id,
                                             a_template_arg *template_arg_list,
                                             a_type_ptr     guide_type,
                                             a_constant_ptr constant,
                                             a_boolean      *err);

extern void selector_match_with_this_param(
                               an_operand           *bound_function_selector,
                               a_routine_ptr        rout,
                               a_type_ptr           routine_type,
                               a_type_ptr           this_param_type,
                               an_arg_match_summary *arg_summary);

extern a_type_ptr object_parameter_type(a_type_ptr   routine_type,
                                        a_symbol_ptr proj_function_symbol,
                                        a_boolean    is_conv_func);

extern
a_boolean is_template_dependent_indefinite_function(an_operand *operand);

extern a_boolean operand_is_dependent(an_operand *operand);

extern a_boolean arg_list_is_type_dependent(an_arg_list_elem_ptr arg_list);

extern a_boolean arg_list_is_dependent(an_arg_list_elem_ptr arg_list);

extern a_boolean is_skipped_decltype_context(void);

extern a_symbol_ptr select_overloaded_function(
                        a_symbol_ptr             overloaded_function_symbol,
                        a_boolean                is_template_id,
                        a_template_arg_ptr       *template_arg_list,
                        a_boolean                have_selector,
                        an_operand               *bound_function_selector,
                        an_arg_list_elem_ptr     arg_list,
                        an_arg_list_elem_ptr     init_list_ctor_arg_list,
                        a_conv_context_set       conv_context,
                        a_boolean                do_arg_dep_lookup,
                        a_boolean                use_pure_arg_dep_lookup,
                        a_boolean                use_std_for_arg_dep_lookup,
                        an_overload_context      ovl_context,
                        a_source_position        *call_position,
                        a_token_sequence_number  paren_tok_seq_number,
                        a_boolean                *single_function,
                        a_boolean                *init_list_ctor_case,
                        a_boolean                *unknown_dependent_function,
                        a_boolean                *found_through_adl,
                        a_symbol_ptr             *surrogate_function_conv_sym,
                        an_arg_match_summary_ptr *arg_match_list);

a_boolean overloaded_function_match_possible(
                               a_symbol_ptr         overloaded_function_symbol,
                               an_overload_context  ovl_context,
                               a_boolean            is_template_id,
                               a_template_arg_ptr   template_arg_list,
                               an_arg_list_elem_ptr arg_list,
                               a_boolean            have_selector,
                               an_operand           *bound_function_selector);

extern void temp_init_from_operand(an_operand *operand,
                                   a_boolean  result_is_lvalue);

extern 
a_dynamic_init_ptr find_top_temporary(an_expr_node_ptr node,
                                      a_boolean        create_class_temp);

extern
void overloaded_function_catch_up(a_symbol_ptr      function_symbol,
                                  a_symbol_ptr      overloaded_function_symbol,
                                  an_operand        *orig_function_operand,
                                  a_source_position *call_position,
                                  a_boolean         elided_reference,
                                  a_boolean         compiler_generated,
                                  a_boolean         result_is_lvalue,
                                  a_boolean         address_taken,
                                  an_operand        *operand,
                                  a_boolean         *access_error_reported,
                                  a_boolean         operator_notation = FALSE);

extern a_boolean is_dependent_static_selection(an_expr_node_ptr sel_expr);

extern void combine_unneeded_selector_with_operand(
                                           an_operand *bound_function_selector,
                                           a_boolean  is_arrow_operator,
                                           an_operand *operand);

extern void cast_pointer_for_field_selection(
                               an_operand        *operand_1,
                               a_boolean         is_arrow_operator,
                               a_symbol_ptr      member_sym,
                               a_symbol_ptr      projection_member_sym,
                               a_boolean         access_control_error_reported,
                               a_boolean         do_protected_member_check,
                               a_source_position *member_pos);

extern a_boolean variable_this_exists_full(a_variable_ptr    *this_var,
                                           a_type_ptr        *this_type,
                                           a_boolean         allow_lambda_this,
                                           a_source_position *used_pos);

extern a_boolean variable_this_exists(a_variable_ptr *this_var,
                                      a_type_ptr     *this_type);

extern a_boolean this_exists_for_member_access(a_symbol_ptr member_sym,
                                               a_boolean    allow_lambda_this);

extern a_variable_ptr this_variable_for_func_scope(a_scope_ptr  scope);


inline a_variable_ptr this_variable_for_lambda_closure(
                                                  a_scope_depth  depth_lambda)
/*
depth_lambda is the scope depth of a lambda body.  Return a pointer to the
"this" variable for the corresponding lambda closure class, which is used among
other things to access the fields that contain the captures of local variables.
*/
{
  check_assertion(depth_lambda != NO_SCOPE_DEPTH);
  return this_variable_for_func_scope(scope_stack[depth_lambda].il_scope);
}  /* this_variable_for_lambda_closure */


extern an_expr_node_ptr this_expr_node_for_lambda_closure(
                                                 a_scope_depth  depth_lambda);

extern void make_abstract_this_operand(an_operand         *opnd,
                                       a_type_ptr         this_type,
                                       a_source_position  *pos,
                                       a_boolean          compiler_generated);

extern an_expr_node_ptr make_selection_for_captured_variable(
                                              a_lambda_capture *lambda_capture,
                                              a_scope_depth    depth_lambda,
                                              a_boolean        is_lvalue);

extern void make_this_variable_operand(a_variable_ptr    this_var,
                                       a_type_ptr        this_type,
                                       a_boolean         is_implicit,
                                       a_source_position *position,
                                       a_source_position *end_position,
                                       an_operand        *result);

extern a_boolean make_this_pointer_operand(
                               a_symbol_ptr      member_sym,
                               a_symbol_ptr      projection_member_sym,
                               a_source_position *member_pos,
                               a_boolean         access_control_error_reported,
                               an_operand        *result);

extern a_boolean is_this_parameter_operand(an_operand     *operand,
                                           a_variable_ptr *p_this_var);

extern void start_call_argument_processing(a_type_ptr         function_type,
                                           a_routine_ptr      routine,
                                           an_arg_check_block *arg_block);

extern void process_call_argument_list(an_arg_list_elem_ptr arg_list,
                                       an_arg_check_block   *arg_block);

extern void change_refs_on_selector(a_type_ptr routine_type,
                                    an_operand *bound_function_selector);

extern void adjust_overloaded_function_call_arguments(
                           a_symbol_ptr             function_symbol,
                           a_boolean                unknown_dependent_function,
                           a_type_ptr               routine_type,
                           a_boolean                have_selector,
                           an_operand               *bound_function_selector,
                           an_arg_list_elem_ptr     arg_list,
                           an_arg_match_summary_ptr arg_match_list,
                           an_expr_node_ptr         *arg_expr_list);

extern a_boolean select_and_prepare_to_call_overloaded_function(
                           a_symbol_ptr            overloaded_function_symbol,
                           a_boolean               is_template_id,
                           a_template_arg_ptr      *template_arg_list,
                           a_boolean               have_selector,
                           an_operand              *bound_function_selector,
                           an_arg_list_elem_ptr    *arg_list,
                           a_boolean               do_arg_dep_lookup,
                           a_boolean               use_pure_arg_dep_lookup,
                           a_boolean               use_std_for_arg_dep_lookup,
                           a_boolean               try_surrogate_functions,
                           a_boolean               is_property,
                           a_boolean               compiler_generated,
                           an_overload_context     ovl_context,
                           an_operand              *orig_function_operand,
                           a_source_position       *call_position,
                           a_token_sequence_number paren_tok_seq_number,
                           a_source_position       *closing_paren_position,
                           a_boolean               *found_through_adl,
                           an_operand              *function_operand,
                           an_expr_node_ptr        *arg_expr_list);

extern
a_boolean type_is_in_builtin_type_set(a_type_ptr              type,
                                      a_builtin_type_kind_set builtin_types);

extern void adjust_class_object_type(an_operand       *operand,
                                     a_type_ptr       dest_type,
                                     a_base_class_ptr bcp);

extern a_boolean conversion_from_class_possible(
                          an_operand               *source_operand,
                          a_type_ptr               dest_type,
                          a_builtin_type_kind_set  builtin_types_allowed,
                          a_boolean                need_lvalue_result,
                          a_boolean                is_copy_initialization,
                          a_boolean                orig_is_copy_initialization,
                          a_type_ptr               ref_binding_type,
                          a_boolean                is_direct_binding,
                          a_conv_context_set       conv_context,
                          a_conv_descr             *conversion,
                          a_boolean                *ambiguous,
                          a_candidate_function_ptr *ambiguity_list);

extern void try_to_convert_class_operand_to_builtin_type(
                                 an_operand              *operand,
                                 a_type_ptr              specific_type,
                                 a_builtin_type_kind_set builtin_types_allowed,
                                 a_conv_context_set      conv_context,
                                 a_boolean               *processed);

extern void make_generic_operation_operand(
                               an_opname_kind          kind,
                               a_boolean               unary_operator,
                               an_operand              *operand_1,
                               an_operand              *operand_2,
                               an_operand              *result,
                               a_source_position       *operator_position,
                               a_token_sequence_number operator_tok_seq_number,
                               a_source_position       *operator_position_2);

extern void f_check_for_operator_overloading(
                             an_opname_kind            kind,
                             a_boolean                 unary_operator,
                             a_boolean                 must_be_member_function,
                             a_boolean                 try_conversions,
                             a_boolean                 has_predef_meaning,
                             an_operand                *operand_1,
                             an_operand                *operand_2,
                             a_source_position         *operator_position,
                             a_token_sequence_number   operator_tok_seq_number,
                             a_nondependent_call_depth call_depth,
                             a_source_position         *operator_position_2,
                             an_operand                *result,
                             a_boolean                 *p_none_viable,
                             a_candidate_function_ptr  rewritten_candidate,
                             a_boolean                 *processed);

#define check_for_operator_overloading(                                       \
                       kind, unary_operator, must_be_member_function,         \
                       try_conversions, has_predef_meaning, operand_1,        \
                       operand_2, operator_position, operator_tok_seq_number, \
                       call_depth, operator_position_2, result, processed)    \
  f_check_for_operator_overloading(                                           \
                       kind, unary_operator, must_be_member_function,         \
                       try_conversions, has_predef_meaning, operand_1,        \
                       operand_2, operator_position, operator_tok_seq_number, \
                       call_depth, operator_position_2, result,               \
                       (a_boolean*)NULL, (a_candidate_function_ptr)NULL,      \
                       processed)
    

extern a_boolean conversion_to_class_possible(
                          an_operand               *source_operand,
                          an_arg_list_elem_ptr     alep,
                          a_type_ptr               dest_type,
                          a_boolean                try_bitwise_copy,
                          a_boolean                is_copy_initialization,
                          a_boolean                orig_is_copy_initialization,
                          a_type_ptr               ref_binding_type,
                          a_boolean                is_direct_binding,
                          a_conv_context_set       conv_context,
                          a_conv_descr             *conversion,
                          a_conv_descr             *ctor_arg_conversion,
                          a_boolean                *ambiguous,
                          a_candidate_function_ptr *ambiguity_list);

extern void bind_member_function_operand_to_selector(
                                         an_operand *bound_function_selector,
                                         a_boolean  selector_is_object_pointer,
                                         an_operand *function_operand);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean cli_handle_user_defined_conversion_possible(
                          an_operand               *source_operand,
                          a_type_ptr               dest_type,
                          a_builtin_type_kind_set  builtin_types_allowed,
                          a_boolean                need_lvalue_result,
                          a_boolean                is_copy_initialization,
                          a_boolean                orig_is_copy_initialization,
                          a_type_ptr               ref_binding_type,
                          a_boolean                is_direct_binding,
                          a_conv_context_set       conv_context,
                          a_conv_descr             *conversion,
                          a_boolean                *ambiguous,
                          a_candidate_function_ptr *ambiguity_list);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean user_defined_conversion_possible(
                                an_operand         *source_operand,
                                a_type_ptr         dest_type,
                                a_boolean          need_lvalue_result,
                                a_boolean          is_copy_initialization,
                                a_boolean          orig_is_copy_initialization,
                                a_type_ptr         ref_binding_type,
                                a_boolean          is_direct_binding,
                                a_conv_context_set conv_context,
                                a_conv_descr       *conversion,
                                a_conv_descr       *ctor_arg_conversion,
                                a_boolean          *failed);

extern void user_convert_operand(an_operand   *operand,
                                 a_type_ptr   dest_type,
                                 a_conv_descr *conversion,
                                 a_conv_descr *ctor_arg_conversion,
                                 a_boolean    force_copy_to_temp);

extern void handle_elided_copy_constructor(a_type_ptr        source_type,
                                           a_routine_ptr     elided_cctor,
                                           a_source_position *err_pos);

extern a_boolean operand_is_temp_init_full(an_operand       *operand,
                                           an_expr_node_ptr *temp_init_node);

/* Interface to operand_is_temp_init_full for the usual case where
   the second argument is NULL. */
#define operand_is_temp_init(operand) \
  (operand_is_temp_init_full((operand), (an_expr_node **)NULL))


extern a_boolean is_temp_init_usable_in_optimization(
                                   an_operand         *source_operand,
                                   a_boolean          suppress_dtor,
                                   an_expr_node_ptr   *p_temp_init_node,
                                   a_dynamic_init_ptr *p_dip);

extern void prep_elision_initializer_operand(
                                  an_operand         *source_operand,
                                  a_type_ptr         dest_type,
                                  a_boolean          fill_in_dtor,
                                  a_conv_context_set conv_context,
                                  an_error_code      err_code,
                                  a_boolean          *elision_done,
                                  a_dynamic_init_ptr *dip);

extern void temp_init_from_operand_full(an_operand *operand,
                                        a_type_ptr temp_type,
                                        a_boolean  result_is_lvalue);

extern a_boolean conversion_for_direct_reference_binding_possible(
                                     an_operand               *source_operand,
                                     a_type_ptr               dest_type,
                                     a_conv_context_set       conv_context,
                                     a_boolean                question_conv,
                                     a_conv_descr             *conversion,
                                     a_boolean                *ambiguous,
                                     a_candidate_function_ptr *ambiguity_list);

extern a_boolean current_mode_requires_early_rvalue_ref_lvalue_test(void);

extern a_boolean direct_reference_binding_possible(
                                 an_operand         *source_operand,
                                 a_type_ptr         source_type,
                                 a_type_ptr         dest_type,
                                 a_boolean          is_cast,
                                 a_conv_context_set conv_context,
                                 a_boolean          *ref_to_const,
                                 a_boolean          *ref_to_const_volatile,
                                 a_boolean          *binding_to_rvalue_allowed,
                                 a_boolean          *dropping_qualifiers,
                                 a_boolean          *p_template_case,
                                 a_symbol_ptr       *function_symbol);
extern
void prep_reference_initializer_operand(an_operand         *source_operand,
                                        a_type_ptr         dest_type,
                                        a_conv_descr       *conversion,
                                        a_boolean          leave_as_object,
                                        a_conv_context_set conv_context,
                                        an_error_code      incompatible_err);

#if !STANDALONE_UTILITY_PROGRAM
extern void value_initialization(a_type_ptr            dest_type,
                                 a_boolean             copy_init_context,
                                 a_boolean             generate_il,
                                 a_source_position     *pos,
                                 a_routine_ptr         *ctor_called,
                                 a_boolean             *is_constant,
                                 a_dynamic_init_ptr    *p_dip,
                                 a_constant_ptr        *p_constant,
                                 an_init_state         *is,
                                 a_boolean             *error_detected);

extern
void unbundle_init_component_list_expressions(an_init_component_ptr list);

extern void unbundle_init_component_expressions(an_init_component_ptr icp);

extern void keep_worst_match(an_arg_match_summary *new_arg_match,
                             an_arg_match_summary *worst_arg_match);

extern
void prep_list_initializer(an_init_component_ptr icp,
                           a_type_ptr            dest_type,
                           a_boolean             is_direct_init,
                           a_boolean             check_narrowing,
                           a_boolean             warning_on_narrowing,
                           a_conv_context_set    conv_context,
                           a_boolean             fill_in_dtor,
                           a_boolean             force_temp,
                           a_boolean             make_lvalue_temp,
                           an_operand            *result,
                           an_init_state         *is,
                           an_arg_match_summary  *arg_match);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern
void prep_initializer_operand(an_operand         *source_operand,
                              a_type_ptr         dest_type,
                              a_boolean          *is_transparent,
                              a_conv_descr       *conversion,
                              a_boolean          is_copy_initialization,
                              a_conv_context_set conv_context,
                              an_error_code      incompatible_err);

extern void prep_arg_passed_via_copy_constructor(an_operand    *source_operand,
                                                 a_type_ptr    param_type,
                                                 a_conv_descr  *conversion,
                                                 an_error_code err_code);

extern void prep_argument_operand(an_operand       *source_operand,
                                  a_param_type_ptr formal_param,
                                  a_conv_descr     *conversion,
                                  an_error_code    err_code);

extern void prep_argument(an_arg_list_elem_ptr alep,
                          a_param_type_ptr     formal_param,
                          a_conv_descr         *conversion,
                          an_error_code        err_code,
                          an_operand           *result);

extern void prep_assignment_operand(an_operand        *source_operand,
                                    a_type_ptr        dest_type,
                                    an_error_code     incompatible_err,
                                    a_source_position *err_pos);

#if GNU_EXTENSIONS_ALLOWED

extern a_field_ptr transparent_union_conversion_possible(
                                                    an_operand *source_operand,
                                                    a_type_ptr union_type);

extern void prep_transparent_union_conversion_operand(
                                                 a_type_ptr  dest_type,
                                                 a_field_ptr field,
                                                 an_operand  *source_operand);

#endif /* GNU_EXTENSIONS_ALLOWED */


extern
a_boolean nontype_templ_arg_of_class_type_matches(an_operand  *operand,
                                                  a_type_ptr  param_type,
                                                  a_constant  *class_con);

extern a_boolean nontype_template_arg_conversion_possible(
                                                        an_operand *operand,
                                                        a_type_ptr param_type);

extern a_boolean conditional_operator_conversion_possible(
                                                   an_operand   *op1,
                                                   an_operand   *op2,
                                                   a_conv_descr *conv,
                                                   a_boolean    *ambiguous);

extern a_symbol_ptr select_overloaded_default_constructor(
                                        a_type_ptr        class_type,
                                        a_boolean         include_templates,
                                        a_boolean         declarative_context,
                                        a_boolean         no_explicit,
                                        a_source_position *pos,
                                        a_boolean         *ambiguous,
                                        a_symbol_ptr      *inaccessible_match);

extern a_symbol_ptr select_overloaded_copy_constructor(
                                   a_type_ptr            class_type,
                                   a_type_qualifier_set  source_cv_qualifiers,
                                   a_boolean             source_is_rvalue,
                                   a_boolean             ignore_explicit_ctors,
                                   a_source_position     *pos,
                                   a_boolean             *ambiguous,
                                   a_boolean             *uncallable,
                                   a_symbol_ptr          *inaccessible_match,
                                   a_boolean             *class_bitwise_copy);

extern a_symbol_ptr select_overloaded_assignment_operator(
                           a_type_ptr            class_type,
                           a_type_qualifier_set  source_cv_qualifiers,
                           a_boolean             source_is_rvalue,
                           a_type_qualifier_set  dest_cv_qualifiers,
                           a_source_position     *pos,
                           a_boolean             *ambiguous,
                           a_boolean             *undecidable_because_of_error,
                           a_symbol_ptr          *inaccessible_match,
                           a_boolean             *bitwise_assign);

extern a_boolean deduce_class_template_args(
                                     a_type_ptr        placeholder_type,
                                     a_boolean         is_direct_init,
                                     a_boolean         parenthesized_init,
                                     a_boolean         keep_placeholder,
                                     an_arg_list_elem  *initializer_alep,
                                     a_source_position *source_pos,
                                     a_type_ptr        *deduced_placeholder,
                                     a_boolean         *still_dependent);

extern a_boolean deduce_auto_type(a_type_ptr        orig_type,
                                  a_type_ptr        auto_type,
                                  a_boolean         keep_placeholder,
                                  an_operand        *initializer_operand,
                                  an_arg_list_elem  *initializer_alep,
                                  a_source_position *source_pos,
                                  a_type_ptr        *type_after_deduction,
                                  a_type_ptr        *deduced_auto_type,
                                  a_boolean         *still_dependent);

extern void overload_init(void);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern an_expr_node_ptr convert_arg_list_to_expr_list(
                                          an_arg_list_elem_ptr arg_list,
                                          an_expr_node_ptr     *expr_tail);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if DEBUG
/* GCOVR_EXCL_START */
extern void db_substitution_stack(void);

extern void db_viability_stats(void);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef OVERLOAD_H */


