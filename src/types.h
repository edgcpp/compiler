/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

types.h -- Declarations related to types.c (having to do with types).

*/

/* Avoid including these declarations more than once: */
#ifndef TYPES_H
#define TYPES_H 1

#include "error.h"
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */
#ifndef EXPR_H
#include "expr.h"
#endif /* ifndef EXPR_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

EXTERN_THREAD a_boolean
		enum_type_is_integral;
			/* TRUE if an enum type is considered an integral
			   type.  Typically TRUE in C mode and FALSE in C++
			   mode. */

EXPAND a_type_ptr skip_typerefs(a_type_ptr type_ptr)
/*
Strip any typeref entries off the given type to get to the real type, and
return a pointer to that.  Note that the typeref may have some type
qualifiers (const, volatile), and they will be dropped here.  Therefore,
this routine should not be used when checking type qualifiers.
*/
{
  while (type_is(type_ptr, tk_typeref)) {
    type_ptr = type_ptr->variant.typeref.type;
#if EXPENSIVE_CHECKING
    check_assertion_str(type_ptr != NULL,
                        "skip_typerefs: NULL referenced type");
#endif /* EXPENSIVE_CHECKING */
  }  /* while */
  return type_ptr;
}  /* skip_typerefs */


INLINE a_float_kind float16_kind(void)
/*
Return the floating-point kind used for the type name "_Float16" in the
current mode:  GNU versions 13 and later and Clang versions 15 and later
use the standard "_Float16" type, and other modes use the GNU extension
type.
*/
{
  return (gnu_version_is(>= 130000) || clang_version_is(>= 150000)) ?
             fk_std_float16 : fk_float16;
}  /* float16_kind */


INLINE a_boolean is_lexical_typeref(a_type_ptr tp)
/*
Return TRUE if tp is a trk_template_arg_list or trk_name_qualifier typeref,
FALSE otherwise.
*/
{
  a_boolean result = FALSE;

  if (type_is(tp, tk_typeref) &&
      (is_typeref_kind(tp, trk_template_arg_list) ||
       is_typeref_kind(tp, trk_name_qualifier))) {
#if DEFAULT_RECORD_FORM_OF_NAME_REFERENCE && !STANDALONE_UTILITY_PROGRAM
    /* These should only be created when recording name references. */
    check_assertion(record_form_of_name_reference);
#endif /* DEFAULT_RECORD_FORM_OF_NAME_REFERENCE && ... */
    result = TRUE;
  }  /* if */
  return result;
}  /* is_lexical_typeref */


INLINE a_type_ptr skip_lexical_typerefs(a_type_ptr type_ptr)
/*
Strip any typeref entries that store information about the lexical form of the
type as written in the source code (i.e., trk_template_arg_list and
trk_name_qualifier).
*/
{
#if DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
  while (is_lexical_typeref(type_ptr)) {
    type_ptr = type_ptr->variant.typeref.type;
#if EXPENSIVE_CHECKING
    check_assertion_str(type_ptr != NULL,
                        "skip_lexical_typerefs: NULL referenced type");
#endif /* EXPENSIVE_CHECKING */
  }  /* while */
#endif /* DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */
  return type_ptr;
}  /* skip_lexical_typerefs */


/*
This macro is equivalent to skip_typerefs.  It exists for compatibility
purposes because skip_typerefs previously was a macro making use of
f_skip_typerefs, and f_skip_typerefs is sometimes called directly.
*/
#define f_skip_typerefs skip_typerefs

extern a_type_ptr skip_typedefs(a_type_ptr type_ptr);
extern a_type_ptr skip_typerefs_not_typedefs(a_type_ptr type_ptr);
extern a_type_ptr skip_typerefs_not_dependent_decltypes(a_type_ptr type_ptr);
extern a_type_ptr skip_typerefs_not_parameterized_decltypes(
                                                         a_type_ptr type_ptr);
extern a_type_ptr skip_typedefs_not_dependent_decltypes(a_type_ptr type_ptr);
extern a_type_ptr skip_typerefs_not_typedefs_or_type_operators(
                                                         a_type_ptr type_ptr);
extern a_type_ptr skip_nontemplate_typerefs(a_type_ptr type_ptr);

EXPAND a_boolean is_error_type(a_type_ptr tp)
/*
Return TRUE if the given type is an error type.
*/
{
  return type_is(skip_typerefs(tp), tk_error);
}  /* is_error_type */

/*
m_is_error_type was a function-like macro in older versions of the front end.
It is now defined as a synonym of is_error_type for compatibility purposes.
*/
#define m_is_error_type is_error_type

EXPAND a_boolean is_immediate_error_type(a_type_ptr tp)
/*
Return TRUE if a type is a direct error type (i.e., not a typeref on
top of such a type).
*/
{
  return type_is(tp, tk_error);
}  /* is_immediate_error_type */

extern a_boolean is_function_type(a_type_ptr tp);
extern a_boolean is_pointer_to_function_type(a_type_ptr tp);
extern a_boolean is_incomplete_type(a_type_ptr tp);
extern a_boolean is_incomplete_array_type(a_type_ptr tp);
extern a_boolean is_sizeless_type(a_type_ptr tp);
extern a_boolean is_flexible_array_type(a_type_ptr tp);
extern a_boolean class_type_has_body(a_type_ptr tp);
#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean is_empty_class_type(a_type_ptr  type);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern a_boolean class_type_has_variant_member(a_type_ptr tp);
extern a_boolean is_object_type(a_type_ptr tp);
extern a_boolean is_complete_object_type(a_type_ptr tp);
extern a_boolean is_void_type(a_type_ptr tp);
extern a_boolean is_reflection_type(a_type_ptr tp);
extern a_boolean is_nullptr_type(a_type_ptr tp);
extern a_boolean is_managed_nullptr_type(a_type_ptr tp);
extern a_boolean is_standard_nullptr_type(a_type_ptr tp);
extern a_boolean is_void_star_type(a_type_ptr tp);
extern a_boolean is_pointer_to_void_type(a_type_ptr tp);
extern a_boolean is_integral_type(a_type_ptr tp);
extern a_boolean is_signed_integral_type(a_type_ptr tp);
#define int_type_is_signed(tp)                                               \
  (int_kind_is_signed[(int)tp->variant.integer.int_kind])
extern a_boolean is_standard_integer_type(a_type_ptr tp);
extern a_boolean is_enum_type(a_type_ptr tp);
extern a_boolean is_scoped_enum_type(a_type_ptr tp);
extern a_boolean is_unscoped_enum_type(a_type_ptr tp);
extern a_boolean is_integral_or_enum_type(a_type_ptr tp);
extern a_boolean is_integral_or_unscoped_enum_type(a_type_ptr tp);
extern a_boolean is_bool_type(a_type_ptr tp);
extern a_boolean enum_has_bool_underlying_type(a_type_ptr tp);
extern a_boolean is_character_type(a_type_ptr tp);
#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean is_general_character_type(a_type_ptr tp);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern a_boolean is_plain_char_type(a_type_ptr tp);
#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean is_narrow_or_wide_character_type(a_type_ptr tp);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if FIXED_POINT_ALLOWED
extern a_boolean is_fixed_point_type(a_type_ptr tp);
#endif /* FIXED_POINT_ALLOWED */
extern a_boolean is_floating_type(a_type_ptr tp);
#if C99_IL_EXTENSIONS_SUPPORTED
extern a_boolean is_real_floating_type(a_type_ptr tp);
extern a_boolean is_nonreal_floating_type(a_type_ptr tp);
extern a_boolean is_imaginary_type(a_type_ptr tp);
extern a_boolean is_complex_type(a_type_ptr tp);

/* Macro to test whether a type kind is a floating point type kind. */
#define type_kind_is_simple_float_like(tkind)                                \
  ((tkind) == tk_float || (tkind) == tk_imaginary)
#define type_kind_is_float_like(tkind)                                       \
  ((tkind) == tk_float || (tkind) == tk_imaginary || (tkind) == tk_complex)
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
#define is_real_floating_type(tp) is_floating_type(tp)
#define type_kind_is_simple_float_like(tkind)                                \
  ((tkind) == tk_float)
#define type_kind_is_float_like(tkind) ((tkind) == (a_type_kind)tk_float)
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#define type_is_simple_float_like(tp)                                        \
  type_kind_is_simple_float_like((tp)->kind)
#define type_is_float_like(tp)  type_kind_is_float_like((tp)->kind)
#if GNU_VECTOR_TYPES_ALLOWED
extern a_boolean is_vector_type(a_type_ptr tp);
#define is_immediate_vector_type(tp)  type_is(tp, tk_vector)
extern a_boolean is_scalable_type(a_type_ptr tp);
extern a_boolean is_opaque_type(a_type_ptr tp);
extern a_boolean is_valid_neon_vector_element_type(a_type_ptr tp);
extern a_boolean is_valid_neon_polyvector_element_type(a_type_ptr tp);
#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean vector_type_is_template_dependent(a_type_ptr  tp);
#endif /* !STANDALONE_UTILITY_PROGRAM */
#else /* !GNU_VECTOR_TYPES_ALLOWED */
#define is_vector_type(tp)  (/*lint --e(506)*/FALSE)
#define is_immediate_vector_type(tp)  (/*lint --e(506)*/FALSE)
#define is_scalable_type(tp)  (/*lint --e(506)*/FALSE)
#define is_opaque_type(tp)  (/*lint --e(506)*/FALSE)
#endif /* GNU_VECTOR_TYPES_ALLOWED */
extern a_boolean is_arithmetic_or_enum_type(a_type_ptr tp);
extern a_boolean is_arithmetic_or_unscoped_enum_type(a_type_ptr tp);
extern a_boolean is_arithmetic_type(a_type_ptr tp);
extern a_boolean is_pointer_type(a_type_ptr tp);
#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean is_pointer_type_to_type(a_type_ptr tp,
                                         a_type_ptr pointee_tp);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern a_boolean is_plain_pointer_type(a_type_ptr tp);
extern a_boolean is_pointer_to_object_type(a_type_ptr tp);
extern a_boolean is_pointer_or_handle_type(a_type_ptr tp);
extern a_boolean types_are_both_pointers_or_both_handles(a_type_ptr tp1, 
                                                         a_type_ptr tp2);
extern a_boolean is_reference_type(a_type_ptr tp);
extern a_boolean is_lvalue_reference_type(a_type_ptr tp);
extern a_boolean is_any_lvalue_reference_type(a_type_ptr tp);
extern a_boolean is_rvalue_reference_type(a_type_ptr tp);
extern a_boolean has_pointer_component(a_type_ptr  tp);
extern a_boolean is_qualified_function_type(a_type_ptr	tp);
#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean rvalue_ref_can_be_bound_to_function_lvalue(void);
extern a_boolean is_reference_that_can_bind_to_rvalue(a_type_ptr type);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern a_boolean may_be_lvalue_ref_to_const_type(a_type_ptr  tp,
                                                 a_type_ptr  *p_utp);
extern a_boolean types_are_references_of_the_same_kind(a_type_ptr tp1, 
                                                       a_type_ptr tp2);
extern a_boolean is_ptr_or_ref_type(a_type_ptr tp);
extern a_boolean is_any_reference_type(a_type_ptr tp);
extern a_boolean is_any_ptr_or_ref_type(a_type_ptr tp);
#define is_pointer_or_handle(tp) ((tp)->kind == (a_type_kind)tk_pointer && \
                                  !(tp)->variant.pointer.is_reference)
#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean is_cli_generic_param_type(a_type_ptr tp);
extern a_boolean is_template_not_cli_generic_param_type(a_type_ptr tp);
extern a_boolean is_handle_type(a_type_ptr tp);
extern a_boolean is_handle_type_not_value_generic(a_type_ptr tp);
extern a_boolean is_handle_type_not_generic_constraint(a_type_ptr tp);
extern a_boolean is_tracking_reference_type(a_type_ptr tp);
extern a_boolean is_handle_or_tracking_ref_type(a_type_ptr tp);
extern a_boolean is_interior_ptr_type(a_type_ptr tp);
extern a_boolean is_pin_ptr_type(a_type_ptr tp);
extern a_boolean is_handle_to_cli_array_type(a_type_ptr tp);
extern a_boolean is_cli_array_type(a_type_ptr tp);
extern a_boolean is_handle_to_nonconst_cppcx_plain_array_type(a_type_ptr tp);
extern a_boolean is_cli_value_type(a_type_ptr tp);
extern a_boolean is_boxable_type(a_type_ptr tp);
extern a_type_ptr boxed_type_for(a_type_ptr unboxed_type);
extern a_boolean is_cli_nullable_type(a_type_ptr tp);
extern a_type_ptr cli_array_element_type(a_type_ptr tp);
extern a_constant_ptr cli_array_rank_constant(a_type_ptr tp);
extern a_host_large_unsigned cli_array_rank(a_type_ptr tp,
                                            a_boolean  *unknown);
extern a_boolean is_ref_class_type(a_type_ptr tp);
extern a_boolean is_value_class_type(a_type_ptr tp);
extern a_boolean is_simple_value_class_type(a_type_ptr tp);
extern a_boolean is_standard_class_type(a_type_ptr tp);
extern a_boolean is_managed_class_type(a_type_ptr tp);
extern a_boolean is_nonreal_template_template_param_instance(a_type_ptr tp);
extern a_boolean is_cli_interface_type(a_type_ptr tp);
extern a_boolean is_cli_ref_or_interface_class_type(a_type_ptr tp);
extern a_boolean is_cli_generic_definition_argument_type(a_type_ptr type);
extern a_boolean cli_type_has_public_default_constructor(a_type_ptr tp);
/* This is called is_handle_ptr because there is a field called
   is_handle in il_def.h and old preprocessors have problems with
   that. */
#define is_handle_ptr(tp) (is_pointer_or_handle(tp) &&                \
                           (tp)->variant.pointer.is_handle)
#define cli_class_type_kind_is(tp, cctk)                                     \
  (class_type_supp(tp)->cli_class_type_kind == (a_cli_class_type_kind)(cctk))
#define is_immediate_standard_class_type(tp)                                 \
  (is_immediate_class_type(tp) &&                                            \
   cli_class_type_kind_is((tp), cctk_standard))
#define is_immediate_managed_class_type(tp)                                  \
  (is_immediate_class_type(tp) &&                                            \
   !cli_class_type_kind_is((tp), cctk_standard))
#define is_immediate_cli_ref_class_type(tp)                                  \
  (is_immediate_class_type(tp) &&                                            \
   cli_class_type_kind_is((tp), cctk_ref))
#define is_immediate_cli_interface_type(tp)                                  \
  (is_immediate_class_type(tp) &&                                            \
   cli_class_type_kind_is((tp), cctk_interface))
#define is_immediate_delegate_type(tp)                                       \
  (is_immediate_class_type(tp) &&                                            \
   (tp)->variant.class_struct_union.is_delegate_class)
extern a_boolean is_delegate_type(a_type_ptr tp);
extern a_routine_ptr delegate_invocation_function(a_type_ptr delegate_type);
extern a_boolean is_delegate_invocation_function(a_routine_ptr rp);
extern a_type_ptr delegate_invocation_type(a_type_ptr delegate_type);
#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean f_is_cli_type_of_kind(a_type_ptr        tp,
                                       a_cli_symbol_kind csk);
#define is_cli_type_of_kind(tp, csk)                                         \
  (f_is_cli_type_of_kind((tp), (a_cli_symbol_kind)(csk)))
#define is_cli_system_object_type(tp)                                        \
  (is_cli_type_of_kind((tp), csk_system_object))
#define is_cli_system_string_type(tp)                                        \
  (is_cli_type_of_kind((tp), csk_system_string))
#define is_cli_system_type_type(tp)                                          \
  (is_cli_type_of_kind((tp), csk_system_type))
extern a_boolean is_cli_attribute_type(a_type_ptr tp);
extern a_boolean is_valid_cli_attribute_parameter_type(a_type_ptr tp);
extern a_boolean class_is_instance_of_generic_from_metadata(
                                                      a_type_ptr  class_type);
/* Macro that produces TRUE if the given class type entry is a class loaded
   from metadata, an instance of a generic class loaded from metadata, or a
   nested class thereof. */
#define class_is_from_metadata(tp)                                           \
  (class_type_supp((tp))->assembly_scope_index != 0 ||                       \
   (tp->variant.class_struct_union.is_generic_instance &&                    \
    class_is_instance_of_generic_from_metadata((tp))))
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern a_boolean is_cli_open_constructed_type(a_type_ptr  tp);
extern a_boolean is_class_or_handle_to_class_type(a_type_ptr  tp);
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,is_immediate_managed_class_type)*/
#define is_immediate_managed_class_type(tp) FALSE
#define is_immediate_standard_class_type(tp)                                 \
  is_immediate_class_type(tp)
/*lint -emacro(506,is_value_class_type)*/
#define is_value_class_type(tp) FALSE
#define is_class_or_handle_to_class_type(tp)  is_class_struct_union_type(tp)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
extern a_boolean is_scalar_type(a_type_ptr tp);
extern a_boolean is_simple_scalar_type(a_type_ptr tp);
#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean is_trivially_copyable_type(a_type_ptr tp);
extern a_boolean is_trivially_copy_constructible_type(a_type_ptr tp);
extern a_boolean is_const_default_constructible(a_type_ptr  tp);
extern a_boolean is_trivial_class(a_type_ptr  tp);
extern a_boolean is_pod_class(a_type_ptr  tp);
extern a_boolean is_literal_type(a_type_ptr tp);
extern a_boolean is_structural_type(a_type_ptr tp);
extern a_boolean could_be_literal_type(a_type_ptr tp);
extern a_boolean is_implicit_lifetime_class(a_type_ptr tp);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern a_boolean is_array_type(a_type_ptr tp);
extern a_boolean is_vla_type(a_type_ptr tp);
extern a_boolean is_char_array_type(a_type_ptr tp);
#if !STANDALONE_UTILITY_PROGRAM 
extern a_targ_size_t array_rank(a_type_ptr tp);
extern a_targ_size_t array_extent(a_type_ptr             tp,
                                  a_host_large_unsigned  dim);
extern a_boolean is_wchar_t_array_type(a_type_ptr tp);
extern a_boolean is_char8_t_array_type(a_type_ptr tp);
extern a_boolean is_char16_t_array_type(a_type_ptr tp);
extern a_boolean is_char32_t_array_type(a_type_ptr tp);
extern a_boolean is_string_type(a_type_ptr tp);
extern a_boolean may_be_string_type(a_type_ptr tp);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern a_boolean is_ptrdiff_t_type(a_type_ptr tp);
extern a_boolean is_size_t_type(a_type_ptr tp);
extern a_boolean is_class_struct_union_type(a_type_ptr tp);
extern a_boolean is_class_struct_type(a_type_ptr tp);
extern a_boolean is_real_class_type(a_type_ptr  tp);
extern a_boolean is_union_type(a_type_ptr tp);
#if GNU_EXTENSIONS_ALLOWED
extern a_boolean is_transparent_union_type(a_type_ptr  tp);
#endif /* GNU_EXTENSIONS_ALLOWED */
extern a_boolean is_aggregate_or_union_type(a_type_ptr tp);
extern a_boolean is_aggregate_type(a_type_ptr tp);
extern a_boolean is_lambda_closure_type(a_type_ptr tp);
extern a_boolean is_std_initializer_list_type(a_type_ptr tp);
extern a_boolean is_std_class(a_type_ptr   tp,
                              a_const_char *name);
extern a_boolean is_std_nothrow_type(a_type_ptr tp);
extern a_boolean is_std_destroying_delete_t(a_type_ptr tp);
extern a_boolean is_ptr_to_member_type(a_type_ptr tp);
extern a_boolean is_abstract_class_type(a_type_ptr tp);
extern a_boolean is_template_param_type(a_type_ptr tp);
extern a_boolean is_template_param_or_proxy_type(a_type_ptr tp);
extern a_boolean is_template_param_type_or_ref_thereto(a_type_ptr tp);
extern a_boolean is_unknown_template_param_type(a_type_ptr tp);
extern a_boolean is_template_alias_type(a_type_ptr tp);
extern a_boolean is_template_class_type(a_type_ptr tp);
extern a_boolean is_polymorphic_class_type(a_type_ptr tp);
extern a_boolean is_auto_type(a_type_ptr tp);
extern a_boolean is_decltype_auto_type(a_type_ptr tp);
extern a_boolean is_auto_template_param_type(a_type_ptr tp);
extern a_boolean is_decltype_auto_template_param_type(a_type_ptr tp);
extern a_boolean is_class_template_placeholder_type(a_type_ptr tp);
#if !STANDALONE_UTILITY_PROGRAM
extern a_type_ptr apply_type_transforming_intrinsic(
                           a_type_ptr             tp,
                           a_typeref_kind         kind,
                           a_source_position_ptr  position,
                           a_boolean              diagnostic_should_be_issued);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern a_boolean is_or_has_volatile_qualified_type(a_type_ptr tp);
extern a_boolean is_referenceable_type(a_type_ptr tp);
extern a_boolean is_trivially_equality_comparable_type(a_type_ptr tp);

extern a_type_ptr array_element_type(a_type_ptr array_type);
extern a_type_ptr underlying_array_element_type(a_type_ptr array_type);
extern a_type_ptr skip_array_types(a_type_ptr tp);
extern a_targ_size_t num_array_elements(a_type_ptr array_type);
extern a_boolean constant_fully_initializes_type(a_constant_ptr con,
                                                 a_type_ptr     type);
#if GNU_VECTOR_TYPES_ALLOWED
extern a_targ_size_t num_vector_elements(a_type_ptr vector_type);
#endif /* GNU_VECTOR_TYPES_ALLOWED */
extern a_type_ptr find_bottom_of_type(a_type_ptr type);
extern a_type_ptr type_pointed_to(a_type_ptr pointer_type);
extern a_type_ptr skip_pointer_types(a_type_ptr tp);

inline a_type_ptr skip_reference_type(a_type_ptr  tp)
/*
If tp is a reference type (or alias thereof), return the underlying referenced
type.  Otherwise, return tp.
*/
{
  if (is_any_reference_type(tp)) tp = type_pointed_to(tp);
  return tp;
}  /* skip_reference_type */

#if !STANDALONE_UTILITY_PROGRAM
extern a_type_ptr decay_type(a_type_ptr  tp);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_type_ptr pm_member_type(a_type_ptr pm_type);
extern a_type_ptr pm_class_type(a_type_ptr pm_type);
extern a_type_ptr pm_orig_class_type(a_type_ptr pm_type);
extern a_type_ptr f_underlying_type_of_derived_type(
                                              a_type_ptr  type,
                                              a_boolean   *p_is_derived_type);
#define underlying_type_of_derived_type(tp)                       \
  f_underlying_type_of_derived_type((tp), (a_boolean*)NULL)

extern a_boolean check_for_vla_in_pointer_to_member(a_type_ptr         type,
                                                    a_source_position  *pos);

extern a_type_ptr type_specifier_of_type(a_type_ptr type);


EXPAND a_routine_type_supplement_ptr rout_type_supp(a_type_ptr type)
/*
Given a routine type, return the corresponding routine type supplement pointer.
*/
{
  check_assertion(type_is(type, tk_routine));
  return type->variant.routine.extra_info;
}  /* rout_type_supp */


inline a_boolean is_fundamental_type(a_type_ptr  type)
/*
Return TRUE if a type is a fundamental type.
*/
{
  return is_void_type(type) || is_arithmetic_type(type) ||
         is_nullptr_type(type);
}  /* is_fundamental_type */


/*
Return TRUE if a type is a direct class type (i.e., not a typeref on
top of a class type).
*/
#define is_immediate_class_type(type)                                 \
  (type_is((type), tk_class) || type_is((type), tk_struct) ||         \
   type_is((type), tk_union))

/*
Return TRUE if a type is a direct non-union class type.
*/
#define is_class_or_struct(type)                                      \
  (type_is((type), tk_class) || type_is((type), tk_struct))


EXPAND a_class_type_supplement_ptr& class_type_supp(a_type_ptr tp)
/*
Return a pointer to the associated class type supplement.
*/
{
#if EXPENSIVE_CHECKING
  check_assertion(is_immediate_class_type(tp));
#endif /* EXPENSIVE_CHECKING */
  return tp->variant.class_struct_union.extra_info;
}  /* class_type_supp */


inline a_type_ptr skip_proxy_class(a_type_ptr  type)
/*
If type is a proxy class, return the associated original type.  Otherwise,
return type.
*/
{
  if (type_is(type, tk_class) &&
      type->variant.class_struct_union.proxy_class) {
    type = class_type_supp(type)->proxy_of_type;
    check_assertion(type != NULL);
  }  /* if */
  return type;
}  /* skip_proxy_class */

/*
Return the list of fields of the given class, struct, or union type.
*/
#define fields_of(tp)                                                 \
  ((tp)->variant.class_struct_union.field_list)

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Return TRUE if tp is a partial class.
*/
#define is_partial_class(tp)                                          \
  (is_class_or_struct((tp)) && class_type_supp((tp))->is_partial)

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


EXPAND an_integer_type_supplement_ptr integer_type_supp(a_type_ptr tp)
/*
Return a pointer to the associated integer type supplement.
*/
{
  check_assertion(type_is(tp, tk_integer));
  return tp->variant.integer.extra_info;
}  /* integer_type_supp */


#define is_bit_precise_kind(kind)                                    \
  ((kind) == ik_bit_precise || (kind) == ik_unsigned_bit_precise)


EXPAND a_boolean is_bit_precise_integer_type(a_type_ptr tp)
/*
Return TRUE if tp is a bit-precise integer type.
*/
{
  tp = skip_typerefs(tp);
  return (type_is(tp, tk_integer) &&
          is_bit_precise_kind(tp->variant.integer.int_kind));
}  /* is_bit_precise_integer_type */


EXPAND a_targ_size_t bit_precise_integer_width(a_type_ptr tp)
/*
Return the width of a bit-precise integer type.
*/
{
  tp = skip_typerefs(tp);
  check_assertion(type_is(tp, tk_integer) &&
                  is_bit_precise_kind(tp->variant.integer.int_kind));
  return integer_type_supp(tp)->bit_width;
}  /* bit_precise_integer_width */


EXPAND a_boolean bit_precise_integer_is_unsigned(a_type_ptr tp)
/*
Return TRUE if tp is an unsigned bit-precise integer type.
*/
{
  tp = skip_typerefs(tp);
  check_assertion(type_is(tp, tk_integer) &&
                  is_bit_precise_kind(tp->variant.integer.int_kind));
  return tp->variant.integer.int_kind == ik_unsigned_bit_precise;
}  /* bit_precise_integer_is_unsigned */


/*
Return TRUE if a type is a direct enum type (i.e., not a typeref on top of
an enum type).
*/
#define is_immediate_enum_type(type)                                  \
  ((type)->kind == (a_type_kind)tk_integer &&                         \
   (type)->variant.integer.enum_type)

/*
Return TRUE if a type is a (direct) scoped enum type.  This macro assumes that
the given type is a tk_integer type.
*/
#define integer_type_is_scoped_enum(type)                             \
  ((type)->variant.integer.is_scoped_enum)

#define is_tag_type(type)                                             \
  (is_immediate_class_type((type)) || is_immediate_enum_type((type)))


inline a_boolean is_unknown_type(a_type_ptr tp)
/*
Return TRUE if the given type is an unknown type.
*/
{
  return type_is(skip_typerefs(tp), tk_unknown);
}  /* is_unknown_type */


/*
Return TRUE or FALSE about the qualifiers of a tk_typeref type.
*/
#define typeref_is_qualified(tp)                                      \
 ((tp)->variant.typeref.qualifiers != TQ_NONE)
#define typeref_is_const_qualified(tp)                                \
 (((tp)->variant.typeref.qualifiers & TQ_CONST) != 0)
#define typeref_is_volatile_qualified(tp)                             \
 (((tp)->variant.typeref.qualifiers & TQ_VOLATILE) != 0)
#define typeref_is_restrict_qualified(tp)                             \
 (((tp)->variant.typeref.qualifiers & TQ_RESTRICT) != 0)

/*
Macro that takes an array type and returns TRUE if its bound is specified but
unknown (e.g., a template parameter or a run-time quantity).
*/
#define has_unknown_specified_bound(array_type)                       \
  ((array_type)->variant.array.is_variable_size_array ||              \
   (array_type)->variant.array.is_template_dependent_size_array)

a_boolean has_any_unknown_specified_bound(a_type_ptr  array_type);

a_boolean has_any_zero_bound(a_type_ptr  array_type);

inline a_boolean array_type_has_no_bound(a_type_ptr  tp)
/*
Return TRUE if the given tk_array type represents an array type formed with an
empty array declarator (i.e., no specified bound; e.g., "int[]").
*/
{
  return !tp->variant.array.bound_is_zero &&
         !has_unknown_specified_bound(tp) &&
         tp->variant.array.variant.number_of_elements == 0;
}  /* array_type_has_no_bound */



/*
Macro that returns TRUE if a type is a template class type that has
not been specialized.
*/
#define is_unspecialized_template_class(tp)				\
  (is_immediate_class_type(tp) &&					\
   (tp)->variant.class_struct_union.is_template_class &&		\
   !(tp)->variant.class_struct_union.is_specialized)

/*
Return a pointer to the associated typeref type supplement.
*/
#define typeref_supp(tp)                                                   \
  ((tp)->variant.typeref.extra_info)


EXPAND a_boolean typeref_is_typedef(a_type_ptr  tp)
/*
Return TRUE if a tk_typeref type represents a typedef-name.

Note typedef-names include both type aliases formed via the typedef specifier
and those formed via alias-declaration syntax.

Additionally, note that this is not identical to !typeref_is_qualified --
though typeref_is_qualified and typeref_is_typedef can never be true at the
same time -- since there are cases in which typerefs are produced that are
empty, with neither name nor qualifier.
*/
{
  return tp->source_corresp.name != NULL;
}  /* typeref_is_typedef */


inline a_boolean typeref_is_using_decl_alias(a_type_ptr  tp)
/*
Return TRUE if a tk_typeref type represents an alias-declaration (including an
instance of an alias template).
*/
{
  return tp->variant.typeref.kind == trk_is_alias ||
         tp->variant.typeref.kind == trk_is_template_alias;
}  /* typeref_is_using_decl_alias */


inline a_boolean typeref_is_type_operator(
                                         a_type_ptr tp,
                                         a_boolean  include_intrinsics = FALSE)
/*
Return TRUE if tp (which must be a tk_typeref type) represents a type
operator (decltype, GNU __bases, etc.) or, if include_intrinsics is TRUE, a
clang/GNU type-returning type builtin.
*/
{
  a_boolean result;

  check_assertion(tp->kind == tk_typeref);
  if (include_intrinsics) {
    result = !is_typeref_kind(tp, trk_none) &&
             !is_typeref_kind(tp, trk_is_deduced_decltype_auto) &&
             !is_typeref_kind(tp, trk_is_deduced_auto) &&
             !is_typeref_kind(tp, trk_is_deduced_class) &&
             !is_typeref_kind(tp, trk_for_type_attributes) &&
             !is_typeref_kind(tp, trk_is_alias) &&
             !is_typeref_kind(tp, trk_is_template_alias) &&
             !is_typeref_kind(tp, trk_template_arg_list) &&
             !is_typeref_kind(tp, trk_name_qualifier);
  } else {
    result = is_typeref_kind((tp), trk_is_decltype) ||
             is_typeref_kind((tp), trk_is_splice) ||
             is_typeref_kind((tp), trk_bases) ||
             is_typeref_kind((tp), trk_direct_bases) ||
             is_typeref_kind((tp), trk_is_typeof_with_expression) ||
             is_typeref_kind((tp), trk_is_typeof_with_type_operand) ||
             is_typeref_kind((tp), trk_pack_index);
  }  /* if */
  return result;
}  /* typeref_is_type_operator */


inline a_type_ptr skip_non_naming_typerefs(a_type_ptr tp)
/*
Skip over any typerefs that do not affect the naming of the underlying type
(i.e., stop when a non-typeref type or a typedef, type operator,
alternative template argument list, or name qualifier is found) and return
the resulting type.
*/
{
  while (type_is(tp, tk_typeref) &&
         !(typeref_is_typedef(tp) ||
           typeref_is_type_operator(tp) ||
           is_typeref_kind(tp, trk_template_arg_list) ||
           is_typeref_kind(tp, trk_name_qualifier))) {
    tp = tp->variant.typeref.type;
  }  /* while */
  return tp;
}  /* skip_non_naming_typerefs */


inline a_boolean typeref_is_type_transforming_intrinsic(a_type_ptr  tp)
/*
Return TRUE if tp (which must be a tk_typeref type) represents a
type-transforming intrinsic.
*/
{
  return typeref_is_type_operator(tp, /*include_intrinsics=*/TRUE) &&
         !typeref_is_type_operator(tp);
}  /* typeref_is_type_transforming_intrinsic */


inline a_boolean type_is_dependent_type_transforming_intrinsic(a_type_ptr  tp)
/*
Return TRUE if tp represents a dependent type-transforming intrinsic.
*/
{
  return type_is(tp, tk_typeref) &&
         tp->variant.typeref.is_dependent_type_operator &&
         typeref_is_type_transforming_intrinsic(tp);
}  /* type_is_dependent_type_transforming_intrinsic */


EXPAND a_boolean type_is_typedef(a_type_ptr  tp)
/*
Return TRUE if the given type represents a typedef-name.

Note typedef-names include both type aliases formed via the typedef specifier
and those formed via alias-declaration syntax.
*/
{
  return type_is(tp, tk_typeref) && typeref_is_typedef(tp);
}  /* type_is_typedef */


extern a_boolean is_possibly_qualified_typedef(a_type_ptr  tp);

#define type_is_overaligned_for_new(tp)                                      \
  (overaligned_allocation_enabled &&                                         \
   alignment_of_type(tp) > targ_default_new_alignment)                       \

/*
Return the alignment of the given type.  Normally, a skip_typeref must
be performed to make sure we get correct alignment, but if the alignment
was set explicitly using an attribute on a typedef, the skip_typeref
could be erroneous (GNU and Microsoft modes only).  Additionally, in
clang mode a C11 atomic qualified class type can have stronger alignment
requirements.
*/
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
extern a_targ_alignment f_alignment_of_type(a_type_ptr  tp);

#define alignment_of_type(tp)                                         \
  ((tp)->alignment_set_explicitly ? (tp)->alignment :                 \
   (tp)->kind != (a_type_kind)tk_typeref ? (tp)->alignment :          \
                                           f_alignment_of_type((tp)))

/*
Return the size of a type.  Internally the size of a tk_void or
tk_routine type is 0, but in GCC emulation mode the size of a void or
function type is 1.  Additionally, in Clang mode an atomic-qualified
type can have additional padding.
*/
#if GNU_EXTENSIONS_ALLOWED
extern a_targ_size_t f_size_of_type(a_type_ptr  tp);

#define size_of_non_typeref_type(tp)                                  \
  ((gcc_mode &&                                                       \
    ((tp)->kind == tk_void || (tp)->kind == tk_routine)) ?            \
      (a_targ_size_t)1 : (tp)->size)
#define size_of_type(tp)                                              \
  ((tp)->kind != tk_typeref ? size_of_non_typeref_type(tp) :          \
     f_size_of_type(tp))
#else /* !GNU_EXTENSIONS_ALLOWED */
#define size_of_non_typeref_type(tp) ((tp)->size)
#define size_of_type(tp)             (skip_typerefs(tp)->size)
#endif /* GNU_EXTENSIONS_ALLOWED */

extern a_boolean type_contains_explicit_alignment(a_type_ptr  tp);
#else /* !(GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED) */
#define alignment_of_type(tp)  (skip_typerefs(tp)->alignment)
#define size_of_type(tp)       (skip_typerefs(tp)->size)
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */

extern a_targ_size_t data_size_of_type(a_type_ptr  tp);

extern a_type_qualifier_set f_get_type_qualifiers(a_type_ptr  tp,
                                                  a_boolean   top_level);

#define get_type_qualifiers(tp)                                       \
  (((tp)->kind == (a_type_kind)tk_typeref ||                          \
    (tp)->kind == (a_type_kind)tk_array) ?                            \
      (f_get_type_qualifiers((tp), /*top_level=*/C_mode())) :         \
      (a_type_qualifier_set)TQ_NONE)

#define get_top_level_type_qualifiers(tp)                             \
  ((tp)->kind == (a_type_kind)tk_typeref ?                            \
      (f_get_type_qualifiers((tp), /*top_level=*/TRUE)) :             \
      (a_type_qualifier_set)TQ_NONE)

/*
Check for type qualifiers.  In C++ this includes looking for qualifiers
on the underlying element type of an array.
*/
#define is_qualified_type(tp)                                         \
  (get_type_qualifiers(tp) != TQ_NONE)
#define is_const_qualified_type(tp)                                   \
  ((get_type_qualifiers(tp) & TQ_CONST) != 0)
#define is_volatile_qualified_type(tp)                                \
  ((get_type_qualifiers(tp) & TQ_VOLATILE) != 0)
#define is_c11_atomic_qualified_type(tp)                              \
  ((get_type_qualifiers(tp) & TQ_C11_ATOMIC) != 0)


#if NAMED_ADDRESS_SPACES_ALLOWED
/*
Return TRUE if the given type is qualified with a named address space (this
includes array types whose element type is so qualified).
*/
#define type_qualified_with_named_address_space(tp)                   \
  (named_address_spaces_enabled &&                                    \
   named_address_space_from_qualifier_set(                            \
             f_get_type_qualifiers(tp, /*top_level=*/FALSE)) != 0)
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */

/*
Check for "top-level" type qualifiers -- i.e., don't look at the element
type if tp is an array.
*/
#define is_top_level_qualified_type(tp)                               \
  (get_top_level_type_qualifiers(tp) != TQ_NONE)

/*
Macro defining qualifiers that don't affect type equivalence: From the front
end's perspective a type so-qualified is identical to its underlying type.
These qualifiers may mean something to a back end, however: They should
therefore be stripped from template argument types.

An example is the Clang _Nullable qualifier.  Given a template X, X<int*> and
X<int *_Nullable> denote the same specialization.  Recording the _Nullable
attribute in the template argument would therefore be misleading.
*/
#define TRANSPARENT_QUALIFIERS TQ_NULLABILITY

/*
Return TRUE if type qualifiers that actually modify the underlying type are
identical.  (In particular, this ignores the transparent qualifiers.)
*/
#define matching_type_qualifier_sets(tqs1, tqs2)                     \
  (((tqs1) & ~TRANSPARENT_QUALIFIERS) == ((tqs2) & ~TRANSPARENT_QUALIFIERS))

/*
Return TRUE if the type qualifiers on two types match.  Typedefs and
the underlying types are ignored.  On an array type it is the element
type that is checked for qualifiers.  (When UPC extensions are supported,
UPC block sizes must match too.)
*/
#if UPC_EXTENSIONS_ALLOWED
#define type_qualifiers_match(tp1, tp2)                               \
  (matching_type_qualifier_sets(get_type_qualifiers(tp1),             \
                                get_type_qualifiers(tp2)) &&          \
   get_upc_block_size(tp1) == get_upc_block_size(tp2))
#else /* !UPC_EXTENSIONS_ALLOWED */
#define type_qualifiers_match(tp1, tp2)                               \
  matching_type_qualifier_sets(get_type_qualifiers(tp1),              \
                               get_type_qualifiers(tp2))
#endif /* UPC_EXTENSIONS_ALLOWED */

/*
Mask off qualifiers that do not map on a single bit (e.g., named address space
qualifiers).
*/
#if NAMED_ADDRESS_SPACES_ALLOWED
#define simple_qualifiers(qualifiers)                                        \
  ((qualifiers) &                                                            \
   ~((((a_type_qualifier_set)1 << NUM_BITS_FOR_NAMED_ADDRESS_SPACE) - 1)     \
                                     << (int)tqt_lsb_named_address_space))
extern a_boolean first_address_space_encloses_second(a_type_qualifier_set  q1,
                                                     a_type_qualifier_set  q2);
#define or_disjunct_named_address_spaces(q1, q2)                             \
|| (named_address_space_from_qualifier_set((q1)) !=                          \
                            named_address_space_from_qualifier_set((q2)) &&  \
   !first_address_space_encloses_second((q1), (q2)))
extern a_type_ptr type_without_named_address_space_qualifiers(a_type_ptr  tp);
#else /* !NAMED_ADDRESS_SPACES_ALLOWED */
#define simple_qualifiers(qualifiers)  (qualifiers)
#define or_disjunct_named_address_spaces(q1, q2)  /* Nothing */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */

/*
Return TRUE if tp1_qualifiers does not have some type qualifier that
tp2_qualifiers has.
*/
#if NEAR_AND_FAR_ALLOWED
/* The "near" qualifier is backwards in that it's okay to remove it
   (producing "far") but not okay to add it, so flip it in the test. */
#define any_qualifier_in_set_missing(tp1_qualifiers, tp2_qualifiers)  \
  ((~(simple_qualifiers((tp1_qualifiers)) ^ TQ_NEAR)                  \
    & (simple_qualifiers((tp2_qualifiers)) ^ TQ_NEAR)) != 0           \
   or_disjunct_named_address_spaces((tp1_qualifiers), (tp2_qualifiers)))
#else /* !NEAR_AND_FAR_ALLOWED */
#define any_qualifier_in_set_missing(tp1_qualifiers, tp2_qualifiers)  \
  ((~simple_qualifiers((tp1_qualifiers))                              \
    & simple_qualifiers((tp2_qualifiers))) != 0                       \
   or_disjunct_named_address_spaces((tp1_qualifiers), (tp2_qualifiers)))
#endif /* NEAR_AND_FAR_ALLOWED */

/*
Return TRUE if tp1 does not have some type qualifiers that tp2 has.  Note
that this macro does not check that the underlying types are compatible.
*/
#define any_qualifier_missing(tp1, tp2)                               \
  (((tp2)->kind == (a_type_kind)tk_typeref ||                         \
    (tp2)->kind == (a_type_kind)tk_array) ?                           \
          f_any_qualifier_missing(tp1, tp2) : FALSE)

extern a_boolean f_any_qualifier_missing(a_type_ptr  tp1,
                                         a_type_ptr  tp2);
extern a_boolean is_qualified_version_of_array_typedef(
                                                a_type_ptr type,
                                                a_type_ptr *unqual_array_type);
#if NEAR_AND_FAR_ALLOWED
extern a_boolean is_far_type(a_type_ptr tp);
extern a_type_qualifier_set get_original_type_qualifiers(a_type_ptr type);
#endif /* NEAR_AND_FAR_ALLOWED */


extern a_boolean class_has_mutable_member(a_type_ptr  tp);

extern a_boolean f_type_has_default_constructor(a_type_ptr  tp,
                                                a_boolean   user_provided_only,
                                                a_boolean   nontrivial_only);

/*
Return TRUE if tp is a class type (or array thereof) with a user-provided
default constructor.
*/
#define type_has_user_provided_default_constructor(tp)               \
  f_type_has_default_constructor(tp, /*user_provided_only=*/TRUE,    \
                                 /*nontrivial_only=*/FALSE)
/*
Return TRUE if tp is a class type (or array thereof) with a user-declared
default constructor or a nontrivial implicitly declared default constructor.
*/
#define type_has_nontrivial_default_constructor(tp)                  \
  f_type_has_default_constructor(tp, /*user_declared_only=*/FALSE,   \
                                 /*nontrivial_only=*/TRUE)

extern a_boolean type_has_nontrivial_destructor(a_type_ptr  tp);

extern a_boolean is_on_any_derivation_of(a_base_class_ptr  bcp,
                                         a_base_class_ptr  ref_bcp);
extern void map_corresponding_base_class(a_base_class_ptr  base_class,
                                         a_type_ptr        new_class,
                                         a_base_class_ptr  disambiguator,
                                         a_base_class_ptr  new_base_class);
extern a_base_class_ptr corresponding_base_class(
                                            a_base_class_ptr  base_class,
                                            a_type_ptr        new_class,
                                            a_base_class_ptr  disambiguator);

extern a_base_class_ptr corresp_base_class(
                                    a_base_class_ptr base_class,
                                    a_base_class_ptr old_class_as_base_of_new);

extern a_base_class_ptr find_base_class_of_full(
                                         a_type_ptr derived_class,
                                         a_type_ptr base_class,
                                         a_boolean  instantiate_if_necessary);
extern a_base_class_ptr find_base_class_of(a_type_ptr derived_class,
                                           a_type_ptr base_class);
extern a_base_class_ptr find_direct_base_class_of(a_type_ptr  derived_class,
                                                  a_type_ptr  base_class_type);
extern a_boolean is_same_class_or_base_class_thereof(a_type_ptr class_1,
                                                     a_type_ptr class_2);
extern a_boolean same_or_related_class_types(a_type_ptr type_1,
                                             a_type_ptr type_2);
extern a_boolean any_nonpublic_steps_in_derivation(a_base_class_ptr bcp);
extern a_boolean f_related_class_pointers(a_type_ptr       type_1,
                                          a_type_ptr       type_2,
                                          a_boolean        *baseward_cast,
                                          a_base_class_ptr *bcp);
/*
Return TRUE if type_1 and type_2 are related class pointers.  If they
are, set *baseward_cast if type_1 --> type_2 is a baseward cast, and
set *bcp to point to the base class entry that shows the relationship.
*/
#define related_class_pointers(type_1, type_2, baseward_cast, bcp)    \
  (C_dialect == C_dialect_cplusplus &&                                \
   is_pointer_type(type_1) && is_pointer_type(type_2) &&              \
   f_related_class_pointers(type_1, type_2, baseward_cast, bcp))
/*
Return TRUE if type_1 and type_2 are related class pointers or related
C++/CLI handles.  If they are, set *baseward_cast if type_1 --> type_2
is a baseward cast, and set *bcp to point to the base class entry that
shows the relationship.
*/
#define related_class_pointers_or_handles(type_1, type_2, baseward_cast, bcp) \
  (!C_mode() && \
   types_are_both_pointers_or_both_handles(type_1, type_2)  && \
   f_related_class_pointers(type_1, type_2, baseward_cast, bcp))

extern a_boolean f_rel_member_pointers(a_type_ptr       type_1,
                                       a_type_ptr       type_2,
                                       a_boolean        *baseward_cast,
                                       a_base_class_ptr *bcp);
/*
Return TRUE if type_1 and type_2 are related pointers to members.  If they
are, set *baseward_cast if type_1 --> type_2 is a baseward cast, and
set *bcp to point to the base class entry that shows the relationship.
*/
#define related_member_pointers(type_1, type_2, baseward_cast, bcp)   \
  (is_ptr_to_member_type(type_1) && is_ptr_to_member_type(type_2) &&  \
   f_rel_member_pointers(type_1, type_2, baseward_cast, bcp))

extern a_boolean type_masks_handler_param_type(a_type_ptr  type_1,
                                               a_type_ptr  type_2);
extern a_boolean set_array_type_size(a_type_ptr  array_type,
                                     a_boolean   suppress_error);
extern a_boolean any_multiple_inheritance(a_type_ptr  class_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
extern an_inheritance_kind implied_inheritance_kind(a_type_ptr  class_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
extern void set_type_size(a_type_ptr type_ptr);
extern a_type_ptr type_after_integral_promotion(a_type_ptr type);
extern a_type_ptr default_argument_promotion(a_type_ptr old_type);
extern a_type_ptr type_after_array_to_pointer_transformation(a_type_ptr type);
extern a_boolean is_narrowing_conversion(a_type_ptr    source_type,
                                         a_constant    *source_constant,
                                         a_type_ptr    dest_type,
                                         a_boolean     check_enum_target,
                                         an_error_code *err_code);
extern a_type_ptr expr_complete_object_type(an_expr_node_ptr node,
                                            a_boolean        call_case);
extern a_type_ptr pointer_con_complete_object_type(a_constant_ptr constant);
extern
a_type_ptr pointer_expr_complete_object_type(an_expr_node_ptr node,
                                             a_boolean        call_case);
extern a_type_ptr add_right_pointer_type_to_this(a_type_ptr type,
                                                 a_type_ptr class_type);

/*
Bit vector used to pass flags into f_identical_types.
*/
typedef unsigned int an_itf_flag_set;

#define ITF_NO_FLAGS 0x0u

#define ITF_IL_IDENTICAL 0x01u
			/* If il_identical is TRUE, check only that the
			   types are identical from the point of view
			   of the IL.  Basically, two types are
			   IL-identical if no cast is needed to assign
			   a value of one type to an entity of the
			   other type. */
#define ITF_UNKNOWN_THIS_CLASS_TYPE 0x02u
			/* TRUE if the this class type may not
			   be known yet.  When this flag is set, a
			   NULL "this" class type is ignored. */
#define ITF_SEEK_CORRESP 0x04u
			/* The given types are expected to be compatible and
			   if they are, the first type (and its components)
			   should have its correspondence pointer point to
			   the corresponding component of the second type
			   (only applies to enum and struct/union types). */
#define ITF_IGNORE_NESTING_DEPTH 0x08u
			/* TRUE if the nesting depths of template parameters
			   should be ignored for purposes of this
			   comparison. */
#define ITF_EXACT_NESTING_DEPTHS_REQUIRED 0x10u
			/* TRUE if the nesting depths of template parameters
			   must match exactly.  In that case, constraints on
			   template parameters must match too. */
#define ITF_IGNORE_TOP_LEVEL_QUALIFIERS 0x20u
			/* TRUE if top-level qualifiers do not have to
			   match.  (In the case of arrays in C++, the top-level
			   qualifiers are those on the element type.) */
#define ITF_EXACT_EQUIVALENCE 0x40u
			/* TRUE if the compared types should be fully
			   equivalent.  In particular, when comparing template
			   parameters of tptk_param kind, the type pointers
			   must match, not just the coordinates.  Also,
			   embedded constants and expressions must be compared
			   with CC_EXACT_EQUIVALENCE. */
#define ITF_CHECKING_DEDUCTION_RESULT 0x80u
			/* We are comparing two types to make sure deduction
			   worked right and we didn't get a function type where
			   we expected a non-function, or vice-versa. */
#define ITF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED 0x100u
			/* TRUE if, when dependent decltypes appear in the
			   type trees, they must appear in both types and the
			   expressions must match. */
#define ITF_CONTEXTUAL_GENERIC_PARAMETERS 0x200u
			/* TRUE if the comparison of generic parameters should
			   take into account the relevant context of the
			   parameters.  Specifically: (a) for generic functions
			   the parameter depth is ignored, and (b) for
			   generic classes the sequence number (field
			   generic_param_seq_number of a template parameter
			   type supplement) is compared instead of the
			   template parameter coordinates. */
#define ITF_IGNORE_MS_CALLING_CONVENTION 0x400u
			/* Ignore Microsoft style calling convention (like
			   "cdecl") specifications. */
#define ITF_EXACT_DOES_NOT_RETURN_MATCH_REQUIRED 0x800u
			/* TRUE if the does_not_return field must match
			   when comparing function types. */
#define ITF_CHECK_DEDUCED_PLACEHOLDER_MATCH 0x1000u
			/* TRUE if an undeduced "auto"/"decltype(auto)"
			   placeholder should be considered identical to a
			   tk_typeref entry indicating a deduced type for such
			   a placeholder. */
#define ITF_IGNORE_TOP_LEVEL_NOEXCEPT 0x2000u
			/* TRUE if a top-level exception specifier should be
			   ignored while comparing the types. */
#define ITF_PLACEHOLDER_CONSTRAINT_MATCH_REQUIRED 0x4000u
			/* TRUE if constraints on placeholder types must
			   match. */
#define ITF_LAST ITF_PLACEHOLDER_CONSTRAINT_MATCH_REQUIRED
			/* Last bit in the bit vector that is in use. */

#define identical_types(t1, t2) \
  ((t1) == (t2) || f_identical_types((t1), (t2), ITF_NO_FLAGS))
#define identical_types_full(t1, t2, options) \
  ((t1) == (t2) || f_identical_types((t1), (t2), (options)))
#define il_identical_types(t1, t2) \
  ((t1) == (t2) || f_identical_types((t1), (t2), ITF_IL_IDENTICAL))
#define identical_types_ignoring_qualifiers(t1, t2) \
  ((t1) == (t2) || f_identical_types((t1), (t2), \
                                     ITF_IGNORE_TOP_LEVEL_QUALIFIERS))

#if STANDALONE_UTILITY_PROGRAM
/* Compare types using a subset of the processing in f_identical_types to
   minimize the size of standalone back ends and utilities. */
extern a_boolean f_standalone_identical_types(a_type_ptr type_1,
                                              a_type_ptr type_2);
#define standalone_identical_types(t1, t2) \
  ((t1) == (t2) || f_standalone_identical_types((t1), (t2)))
#else /* !STANDALONE_UTILITY_PROGRAM */
/* Use the full processing of f_identical_types to compare types. */
#define standalone_identical_types(t1, t2) \
  identical_types((t1), (t2))
#endif /* STANDALONE_UTILITY_PROGRAM */

/* Compare one level of two array types. */
extern a_boolean f_identical_types(a_type_ptr      type_1,
                                   a_type_ptr      type_2,
                                   an_itf_flag_set flags);
extern a_boolean cast_identical_types(a_type_ptr type_1,
                                      a_type_ptr type_2);
extern a_type_ptr param_type_restoring_orig_templ_array(a_param_type_ptr ptp);
extern a_boolean integral_types_the_same_except_for_signedness(
                                                            a_type_ptr type_1,
                                                            a_type_ptr type_2);
extern a_boolean interchangeable_types(a_type_ptr type_1,
                                       a_type_ptr type_2);
extern a_boolean this_param_types_correspond(a_type_ptr rout_type_1,
                                             a_type_ptr rout_type_2,
                                             a_boolean  check_as_conversion,
                                             a_boolean  check_as_operands);
extern
a_boolean member_types_correspond(a_type_ptr dest_type,
                                  a_type_ptr source_type,
                                  a_boolean  source_is_function,
                                  a_boolean  allow_qualifier_or_eh_mismatch,
                                  a_boolean  *qualifiers_added);

extern a_boolean types_are_layout_compatible(a_type_ptr  tp1,
                                             a_type_ptr  tp2);

extern a_targ_size_t common_initial_sequence_limit(a_type_ptr  tp1,
                                                   a_type_ptr  tp2);

/*
Description of differences found while comparing types.
*/
typedef struct a_type_difference_descr *a_type_difference_descr_ptr;
typedef struct a_type_difference_descr {
  a_type_list_entry_ptr
		incompatible_calling_conventions;
			/* A list containing an even number of types, which,
			   taken two-by-two, describe routine types with
			   incompatible calling conventions during a type
			   comparison that permits incompatible conventions. */
} a_type_difference_descr;


/*
Bit flags for calls of f_types_are_compatible et al.
*/
#define TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING 0x1u
			/* An error type is considered compatible with
			   anything. */
#define TCF_IGNORE_TYPE_QUALIFIERS 0x2u
			/* Ignore type qualifiers at the first level.  In C++,
			   this includes qualifiers on array element types. */
#define TCF_REDECLARATION 0x4u
			/* This is a top-level compatibility check for a
			   redeclaration.  It's important in C++ because it's
			   the only context in which known- and unknown-bound
			   array types are "compatible" (WP 3.5). */
#define TCF_IGNORE_CALLING_CONVENTIONS 0x8u
			/* Ignore the calling conventions implied by name
			   linkage specified on top-level function types. */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
			/* Also ignore Microsoft style calling convention
			   specifications. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
#define TCF_IMPLICIT_CONVERSION 0x10u
			/* The conversion appears in the context of an
			   implicit conversion, which (in C++) may affect how
			   how routine linkage compatibility is determined. */
#define TCF_DONT_IGNORE_PARAM_TYPE_QUALIFIERS 0x20u
			/* Parameter types should be regarded as incompatible
			   when their top-level type qualifiers differ, even
			   when remove_qualifiers_from_param_types is TRUE.
			   This flag is used in Microsoft-bugs mode only,
			   to deal with a bug in checking for overriding
			   virtual functions. */
#define TCF_IGNORE_PTR_TO_MEMBER_CLASS_TYPE 0x40u
			/* Two pointer-to-member types are deemed compatible
			   as long as the member-types match -- no check
			   should be done for the class-types.  This flag is
			   used in Microsoft-bugs mode only, to deal with a
			   bug in redeclaration of static data members. */
#define TCF_IGNORE_THIS_CLASS_TYPE 0x80u
			/* Two function types are deemed compatible even if
			   the this class types do not match. */
#define TCF_SEEK_CORRESP 0x100u
			/* The given types are expected to be compatible and
			   if they are, the first type (and its components)
			   should have its correspondence pointer point to
			   the corresponding component of the second type
			   (only applies to enum and struct/union types). */
#define TCF_NO_DEFAULT_ARG_PROMOTIONS 0x200u
			/* The second type has an unprototyped parameter list
			   (i.e., from an old-style function definition).
			   Compare the parameter types without the usual
			   default promotions.  (Used in GNU C mode.) */
#define TCF_IGNORE_RETURN_TYPE_QUALIFIERS 0x400u
			/* Ignore qualifiers when testing the return type of
			   a top-level function type.  (Used in GNU C mode.) */
#define TCF_CHECKING_DEDUCTION_RESULT 0x800u
			/* We are comparing two types to make sure deduction
			   worked right and we didn't get a function type where
			   we expected a non-function, or vice-versa.
			   See verify_routine_type_matches_template. */
#define TCF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED 0x1000u
			/* TRUE if, when dependent decltypes appear in the
			   type trees, they must appear in both types and the
			   expressions must match. */
#define TCF_CONTEXTUAL_GENERIC_PARAMETERS 0x2000u
			/* TRUE if the comparison of generic parameters should
			   take into account the relevant context of the
			   parameters.  Specifically: (a) for generic functions
			   the parameter depth is ignored, and (b) for
			   generic classes the sequence number (field
			   generic_param_seq_number of a template parameter
			   type supplement) is compared instead of the
			   template parameter coordinates. */
#define TCF_RECORD_DIRECT_CALLING_CONVENTION_DIFFS 0x4000u
			/* Record incompatible Microsoft-style calling
			   conventions (like __cdecl, or __clrcall) in the
			   "diffs" parameter (if non-NULL) and do not let them
			   affect the overall type compatibility outcome.
			   Only "direct" conventions (i.e., not under a
			   typedef) are tracked. */
#define TCF_ALLOW_BASE_DERIVED_THIS_MATCH 0x8000u
			/* TRUE if the this class type of the second type
			   can be a base class of the first.  This is used
			   to allow a base/derived mismatch in template
			   function matching. */
#define TCF_IGNORE_NESTING_DEPTH 0x10000u
			/* TRUE if the nesting depths of template parameters
			   should be ignored for purposes of this
			   comparison. */
#define TCF_CHECK_DEDUCED_PLACEHOLDER_MATCH 0x20000u
			/* TRUE if an undeduced "auto"/"decltype(auto)"
			   placeholder should be considered compatible with
			   a tk_typeref entry indicating a deduced type for
			   such a placeholder. */
#define TCF_CHECK_ENABLE_IF_ATTRIBUTES 0x40000u
			/* TRUE if the Clang enable_if attributes should be
			   compared. */
#define TCF_USE_CPP_QUALIFIER_RULES 0x80000u
			/* TRUE if even in C mode the C++ rules for
			   TCF_IGNORE_TYPE_QUALIFIERS should be applied. */
#define TCF_IGNORE_TOP_LEVEL_NOEXCEPT 0x100000u
			/* TRUE if a top-level exception specifier should be
			   ignored while comparing the types. */
#define TCF_MEMBER_REDECL_CHECK 0x200000u
			/* TRUE when comparing routine types to check for
			   member redeclaration conflicts. */
#define TCF_STRICT_EXCEPTION_SPEC 0x400000u
			/* TRUE if exception specifications on routine types
			   should match exactly (when exception specifications
			   are part of routine types).  Requires that the flag
			   ICF_IMPLICIT_CONVERSION also be TRUE. */
#define TCF_PLACEHOLDER_CONSTRAINT_MATCH_REQUIRED 0x800000u
			/* TRUE if constraints on placeholder types must
			   match. */
#define TCF_DISTINCT_DEPENDENT_TYPES 0x1000000u
			/* TRUE if dependent types should always be considered
			   to be distinct.  (Used in GCC/Clang mode when
			   comparing instantiated member function template
			   declarations.) */
#define TCF_LAST TCF_DISTINCT_DEPENDENT_TYPES
			/* Last bit in the bit vector that is in use. */
#define TCF_NO_FLAGS 0x0u
typedef unsigned a_type_compat_flags_set;

extern a_boolean compatible_enable_if_attributes(a_type_ptr  rtp1,
                                                 a_type_ptr  rtp2);

/*
How strictly c_tagged_types_match compares the types of corresponding members
of two C struct or union types.
*/
enum a_tagged_type_match_kind : a_byte {
  ttmk_redeclaration,	/* Corresponding members must have the same types,
			   as C23 6.7.3.3 requires of two declarations that
			   declare the same tagged type. */
  ttmk_compatibility	/* Corresponding members need only have compatible
			   types, as C23 6.2.7 requires of two compatible
			   tagged types. */
};
typedef enum a_tagged_type_match_kind  a_tagged_type_match_kind;

extern a_boolean c_tagged_types_match(a_type_ptr                type_1,
                                      a_type_ptr                type_2,
                                      a_tagged_type_match_kind  match_kind);

extern a_boolean f_types_are_compatible_full(
                                          a_type_ptr                   type_1,
                                          a_type_ptr                   type_2,
                                          a_type_compat_flags_set      flags,
                                          a_type_difference_descr_ptr  diffs);

extern a_boolean param_types_are_compatible_full(
                                      a_type_ptr                   rout_type1,
                                      a_type_ptr                   rout_type2,
                                      a_type_compat_flags_set      flags,
                                      a_type_difference_descr_ptr  diffs);

#define param_types_are_compatible(rtp1, rtp2, flags)                        \
  (param_types_are_compatible_full((rtp1), (rtp2), (flags),                  \
                                   (a_type_difference_descr_ptr)NULL))

#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
extern a_boolean calling_conventions_are_compatible(a_type_ptr type1,
                                                    a_type_ptr type2);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
extern a_boolean f_types_are_compatible(a_type_ptr              type_1,
                                        a_type_ptr              type_2,
                                        a_type_compat_flags_set flags);
/*
Several macros to be used in calling f_types_are_compatible, since they short
circuit some of the processing in common cases.
*/
/* Use types_are_compatible when an error type should be treated as compatible
   with any type. */
#define types_are_compatible(t1, t2)                                  \
	 ((t1) == (t2) ||                                             \
          f_types_are_compatible((t1), (t2),                          \
                                 TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING))
/* Use types_are_redecl_compatible for special handling in C++ of known- and
   unknown-bound arrays; otherwise it's the same as types_are_compatible. */
#define types_are_redecl_compatible(t1, t2)                           \
	 ((t1) == (t2) ||                                             \
          f_types_are_compatible((t1), (t2),                          \
                                 TCF_REDECLARATION |                  \
                                 TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING))
/* Use types_are_strictly_compatible when an error type is incompatible with
   any type, including an error type. */
#define types_are_strictly_compatible(t1, t2, flags)                  \
         ((t1) == (t2) ? !is_error_type(t1) :                         \
            f_types_are_compatible((t1), (t2), flags))
/* Use types_are_compatible_ignoring_qualifiers to check compatibility while
   ignoring first-level qualifiers. */
#define types_are_compatible_ignoring_qualifiers(t1, t2)              \
  ((t1) == (t2) ||                                                    \
   f_types_are_compatible((t1), (t2),                                 \
                          TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING |   \
                          TCF_IGNORE_TYPE_QUALIFIERS))
/* Use routine_types_are_redecl_compatible to check types of routines, ignoring
   top-level calling convention modifiers.  This is intended for redeclaration
   checking, as in "are these declaring the same function?" */
/*lint -emacro(835,routine_types_are_redecl_compatible)*/
#define routine_types_are_redecl_compatible(t1, t2, extra_flags)          \
         ((t1) == (t2) ||                                                 \
          f_types_are_compatible((t1), (t2),                              \
                                 TCF_IGNORE_CALLING_CONVENTIONS |         \
                                 TCF_IGNORE_TOP_LEVEL_NOEXCEPT |          \
                                 TCF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED | \
                                 TCF_CHECK_ENABLE_IF_ATTRIBUTES |         \
                                 (extra_flags)))

#define types_are_compatible_for_impl_conversion(t1, t2)              \
  ((t1) == (t2) ||                                                    \
   f_types_are_compatible((t1), (t2),                                 \
                          TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING |   \
                          TCF_IGNORE_TYPE_QUALIFIERS |                \
                          TCF_IMPLICIT_CONVERSION))

extern a_boolean check_gpp_template_redecl_match(a_type_ptr	type_1,
						 a_type_ptr	type_2);

extern a_boolean equiv_class_types(a_type_ptr type_1,
                                   a_type_ptr type_2,
                                   a_boolean  error_matches_anything,
                                   a_boolean  exact_templ_arg_match_required,
                                   a_boolean  contextual_generic_parameters,
                                   a_boolean  exact_decltype_exprs_required,
                                   a_boolean  exact_nesting_depths_required);


extern a_boolean is_address_of_string_constant(a_constant *constant);

extern a_boolean same_type_with_added_qualifiers(
                                     a_type_ptr source_type,
                                     a_type_ptr dest_type,
                                     a_boolean  ignore_qualifiers,
                                     a_boolean  *p_qualifiers_added);

extern
a_boolean exception_spec_conversion_possible(a_type_ptr source_type,
                                             a_type_ptr dest_type);

extern a_boolean qualification_conversion_possible_full(
                                        a_type_ptr    source_type,
                                        a_type_ptr    dest_type,
                                        a_boolean     *p_qualifiers_added,
                                        a_boolean     ignore_underlying_type,
                                        a_boolean     qual_pattern_only,
                                        an_error_code *warning_suggested,
                                        a_type_ptr    *underlying_source_type,
                                        a_type_ptr    *underlying_dest_type);

extern
a_boolean qualification_conversion_possible(
                                         a_type_ptr    source_type,
                                         a_type_ptr    dest_type,
                                         a_boolean     *p_qualifiers_added,
                                         an_error_code *warning_suggested,
                                         a_boolean     ignore_underlying_type);

extern
a_boolean cast_removes_qualifiers(a_type_ptr    source_type,
                                  a_type_ptr    dest_type,
                                  an_error_code *warning_suggested);

extern a_boolean types_are_similar(a_type_ptr  tp1,
                                   a_type_ptr  tp2);

extern a_boolean are_reference_related(a_type_ptr type_1,
                                       a_type_ptr type_2);

extern a_boolean are_reference_compatible(a_type_ptr type_1,
                                          a_type_ptr type_2);

extern a_boolean types_are_interpreter_compatible(a_type_ptr  tp1,
                                                  a_type_ptr  tp2);

/*
Description of a standard conversion (implicit or explicit), or at least
of information relating to such a conversion that's non-trivial to compute.
*/
typedef struct a_std_conv_descr *a_std_conv_descr_ptr;
typedef struct a_std_conv_descr {
  a_base_class_ptr
		cast_base_class;
			/* If the standard conversion is a related-class cast,
			   this is the base class entry for it.  Otherwise,
			   NULL. */
  an_error_code	warning_suggested;
			/* If not ec_no_error, the code for a warning to be
			   issued if this conversion is done. */
  a_bit_field	reversed_cast:1;
			/* If TRUE, cast_base_class describes the
			   reverse of the cast performed.  Used for
			   conversions of pointers to members to
			   pointers to members of derived classes. */
  a_bit_field	type_qualifiers_added:1;
			/* TRUE if type qualifiers were added under a pointer,
			   pointer-to-member, reference, or C++/CLI handle.
			   Also TRUE when converting from a pointer to a
			   noexcept function to a pointer to a matching type
			   without "noexcept".  Serves as a tie-breaker in
			   overload resolution. */
  a_bit_field	secondary_type_qualifiers_added:1;
			/* TRUE if type qualifiers were added on a conversion
			   under a reference, e.g., when a reference to a
			   pointer type is bound to a pointer to a slightly
			   different type.  type_qualifiers_added in
			   that case refers to the qualifiers directly under
			   the reference, and this field refers to the
			   qualifiers added under the pointer type. */
  a_bit_field	null_pointer_constant:1;
			/* TRUE if the conversion involves converting an
			   integral null pointer constant to a pointer.  (Note
			   that this does not apply to conversion of a
			   std::nullptr_t value.) */
  a_bit_field	pointer_normalization_needed:1;
			/* TRUE if the conversion involves converting an
			   integral null pointer constant to a pointer or
			   nullptr type or converting a pointer type to
			   "void *".  (Note that this does not apply to
			   conversion of a nullptr type to a pointer or
			   pointer-to-member type.)  Also covers the C++/CLI
			   conversion of a handle to interface to a handle to
			   System::Object^. */
  a_bit_field	nontrivial_conversion:1;
			/* TRUE if the conversion is, in the terms of
			   overload resolution (ARM 13.2), more than just
			   a sequence of trivial conversions. */
  a_bit_field	promotion:1;
			/* TRUE if the conversion is a promotion, e.g.,
			   short --> int.  Only set in C++ mode. */
  a_bit_field	fixed_enum_promotion:1;
			/* TRUE if the conversion is a promotion from an enum
			   type with a fixed underlying type to that underlying
			   type in a mode where such promotions are preferred
			   over other promotions. */
  a_bit_field	ptr_or_pm_to_bool:1;
			/* TRUE if this conversion is from a pointer type,
			   pointer to member type, or nullptr type to
			   bool. */
  a_bit_field	boxing_conversion:1;
			/* TRUE if this conversion is a C++/CLI boxing
			   conversion, i.e., from a value type to a handle
			   to the value type. */
  a_bit_field	exception_spec_incompatibility:1;
			/* TRUE if the conversion involves converting to
			   a function type with a more restrictive exception
			   specification, which is disallowed in
			   initializations and assignments. */
  a_bit_field	conv_of_string_literal_to_ptr_to_nonconst:1;
			/* TRUE if the conversion is the deprecated conversion
			   of a string literal to "char *", or a wide string
			   literal to "wchar_t *". */
  a_bit_field	is_mild_warning:1;
			/* If TRUE, the warning indicated by warning_suggested
			   is mild, more an observation than a conformance
			   issue. */
  a_bit_field	cli_array_covariance_conversion:1;
			/* TRUE if this conversion is a C++/CLI array
			   covariance conversion, i.e., from an array of handle
			   types to a array of handle types where a conversion
			   exists for the underlying element types and the
			   arrays have the same rank. */
  a_bit_field	gpp_conv_of_real_to_complex:1;
			/* TRUE if this conversion is from an integer or
			   real floating type to a complex type, in
			   g++ mode. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	conv_of_string_literal_to_cli_string:1;
			/* TRUE if the conversion involves the conversion of a
			   string literal to a C++/CLI System::String^ (it does
			   not imply that the destination type is
			   System::String^; there could follow an additional
			   conversion to another handle type). */
  a_bit_field	param_array_conversion:1;
			/* TRUE if this is a conversion to the element type of
			   a C++/CLI parameter array. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	conv_to_std_initializer_list:1;
			/* TRUE if this is a conversion of a braced-init-list
			   to an std::initializer_list<X> object.  See C++11
			   [over.ics.rank]p3 last bullet. */
  a_bit_field	conv_to_array:1;
			/* TRUE if this is a conversion of a braced-init-list
			   to an array object.  See C++14 [over.ics.rank]p3
			   last bullet, second sub-bullet. */
  a_bit_field	flt_identical_representations:1;
			/* TRUE if at least one of the types is an extended
			   floating point type and the two types have the
			   same representation; FALSE in all other
			   cases. */
  a_targ_size_t	num_elements_initialized;
			/* If conv_to_array is TRUE, contains the number of
			   elements initialized by the braced-init-list. */
} a_std_conv_descr;

#define clear_std_conv_descr(p_std_conv)                           \
  (memzero((char*)(p_std_conv), sizeof(a_std_conv_descr)))


extern a_boolean is_nothrow_spec(an_exception_specification_ptr  esp);
extern a_boolean is_nothrow_type(a_type_ptr  type);
extern a_boolean is_non_throwing_routine(a_routine_ptr rp);
extern a_boolean exception_spec_is_less_restrictive(
                                         an_exception_specification_ptr  esp1,
                                         an_exception_specification_ptr  esp2);
extern a_boolean type_has_less_restrictive_exception_spec(a_type_ptr  type1,
                                                          a_type_ptr  type2);
extern a_boolean same_exception_spec(a_type_ptr type_1, a_type_ptr type_2);

#if MICROSOFT_EXTENSIONS_ALLOWED

extern
a_boolean is_prohibited_interior_ptr_conversion(a_type_ptr source_type,
                                                a_type_ptr dest_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
extern a_boolean impl_pointer_conversion(
                         a_type_ptr           source_type,
                         a_boolean            source_is_constant,
                         a_boolean            source_is_string_literal,
                         a_boolean            source_is_function,
                         a_constant           *source_constant,
                         a_type_ptr           dest_type,
                         a_boolean            allow_qualifier_or_eh_mismatch,
                         a_boolean            suppress_extensions,
                         an_error_code        default_warning_code,
                         a_std_conv_descr_ptr std_conv,
                         a_conv_context_set   conv_context = CCO_DEFAULT);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern
a_boolean literal_type_convertible_to_cli_string(a_type_ptr type_ptr);
extern
a_boolean cli_string_literal_conversion_possible(
                                              a_type_ptr           source_type,
                                              a_type_ptr           dest_type,
                                              a_std_conv_descr_ptr std_conv);
extern
a_boolean cli_array_covariance_conversion_possible(
                                              a_type_ptr           source_type,
                                              a_type_ptr           dest_type,
                                              a_std_conv_descr_ptr std_conv);
extern a_boolean impl_handle_conversion(
                         a_type_ptr           source_type,
                         a_type_ptr           dest_type,
                         a_boolean            allow_qualifier_or_eh_mismatch,
                         a_std_conv_descr_ptr std_conv);
extern a_boolean boxing_conversion_possible(a_type_ptr           source_type,
                                            a_type_ptr           dest_type,
                                            a_std_conv_descr_ptr std_conv);
extern
a_boolean unboxing_conversion_possible(a_type_ptr           source_type,
                                       a_type_ptr           dest_type,
                                       a_std_conv_descr_ptr std_conv);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
extern a_boolean impl_ptr_to_member_conversion(
                         a_type_ptr           source_type,
                         a_boolean            source_is_constant,
                         a_boolean            source_is_function,
                         a_constant           *source_constant,
                         a_type_ptr           dest_type,
                         a_boolean            allow_qualifier_or_eh_mismatch,
                         a_std_conv_descr_ptr std_conv);
extern a_boolean impl_conversion_possible(
                          a_type_ptr           source_type,
                          a_boolean            source_is_constant,
                          a_boolean            source_is_string_literal,
                          a_boolean            source_is_function,
                          a_boolean            is_copy_initialization,
                          a_constant           *source_constant,
                          a_type_ptr           dest_type,
                          a_boolean            singleton_braced_init,
                          a_boolean            allow_qualifier_or_eh_mismatch,
                          a_boolean            suppress_extensions,
                          an_error_code        default_warning_code,
                          a_std_conv_descr_ptr std_conv,
                          a_conv_context_set   conv_context = CCO_DEFAULT);
extern a_boolean impl_converted_constant_expr_conversion_possible(
                                           a_type_ptr       source_type,
                                           a_boolean        source_is_constant,
                                           a_constant       *source_constant,
                                           a_type_ptr       dest_type,
                                           an_error_code    *err_code);
extern a_boolean conversion_allowed_for_nontype_template_argument(
                                           a_std_conv_descr *conversion,
                                           a_type_ptr       source_type,
                                           a_boolean        source_is_constant,
                                           a_constant       *source_constant,
                                           a_type_ptr       dest_type,
                                           an_error_code    *err_code);
extern a_boolean static_cast_conversion_possible(
                                 a_type_ptr    source_type,
                                 a_boolean     source_is_constant,
                                 a_boolean     source_is_string_literal,
                                 a_boolean     source_is_function,
                                 a_constant    *source_constant,
                                 a_type_ptr    dest_type,
                                 a_boolean     allow_qualifier_or_eh_mismatch,
                                 an_error_code default_warning_code,
                                 an_error_code *warning_suggested);
extern a_boolean reinterpret_cast_conversion_possible(
                                             a_type_ptr    source_type,
                                             a_type_ptr    dest_type,
                                             an_error_code *warning_suggested);
extern a_boolean expl_conversion_possible(
                                        a_type_ptr    source_type,
                                        a_boolean     source_is_constant,
                                        a_boolean     source_is_string_literal,
                                        a_boolean     source_is_function,
                                        a_constant    *source_constant,
                                        a_type_ptr    dest_type,
                                        a_boolean     *reinterpret_cast_needed,
                                        an_error_code default_warning_code,
                                        an_error_code *warning_suggested);

#if GENERATE_SOURCE_SEQUENCE_LISTS
extern void disentangle_default_args(a_type_ptr  rtp1,
                                     a_type_ptr  rtp2);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern a_type_ptr make_cv_combined_type_if_possible(a_type_ptr  tp1,
                                                    a_type_ptr  tp2);

extern a_type_ptr multilevel_composite_pointer_type(a_type_ptr type_1,
                                                    a_type_ptr type_2);

extern a_type_ptr composite_type(a_type_ptr type_1,
                                 a_type_ptr type_2);

extern a_boolean overload_distinguishable(a_symbol_ptr        old_sym_ptr,
                                          a_type_ptr          new_type,
                                          a_decl_parse_state  *dps,
                                          an_error_code       *err_code);
extern a_boolean is_or_contains_error_type(a_type_ptr  type_ptr);
extern a_boolean is_or_contains_typedef_type(a_type_ptr  type_ptr);
extern a_boolean is_or_contains_local_type(a_type_ptr  type_ptr);
extern a_boolean is_or_contains_unnamed_namespace_type(a_type_ptr  type_ptr);
extern a_boolean is_or_contains_type_with_no_name_linkage(
                                                       a_type_ptr  type_ptr);
extern a_boolean is_or_contains_trans_unit_specific_type(a_type_ptr  type_ptr);
#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean function_type_has_clrcall_component(a_type_ptr  type_ptr);
extern a_boolean is_or_contains_cli_generic_param(a_type_ptr  type_ptr);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean is_invalid_parameter_type(a_type_ptr  param_type);
extern a_boolean is_invalid_template_arg_type(a_type_ptr  type_ptr,
                                              a_boolean   *is_unnamed,
                                              a_boolean   *is_local,
                                              a_boolean   *is_vla,
                                              a_boolean   *is_generic);
extern a_boolean is_template_dependent_type(a_type_ptr  type_ptr);
extern a_boolean is_template_dependent_type_or_cli_generic_param(
                                                         a_type_ptr  type_ptr);
extern a_boolean is_instantiation_dependent_type(a_type_ptr  type_ptr);
extern a_boolean is_instantiation_dependent_type_or_cli_generic_param(
							a_type_ptr  type_ptr);
extern a_boolean is_or_contains_template_param(a_type_ptr  type_ptr);
extern void set_parameter_list_template_param_flags(a_type_ptr  rout_type);
extern
a_boolean is_or_contains_specific_template_param(a_type_ptr  type_ptr,
                                                 a_type_ptr  tparam_type,
                                                 a_boolean   deduced_only,
                                                 a_boolean   exclude_parents);
extern a_boolean type_contains_specific_template_template_param(
					a_type_ptr	type_ptr,
					a_template_ptr	tparam_template,
					a_boolean	deduced_only,
					a_boolean	exclude_parents);
extern a_boolean type_contains_specific_template_param_constant(
					a_type_ptr	tp,
					a_constant_ptr	cp,
					a_boolean	deduced_only,
					a_boolean	exclude_parents);
extern a_boolean could_be_dependent_class_type(a_type_ptr tp);

/*
Alias for could_be_dependent_class_type, representing another view of the
same test: template parameter type or nonreal class type.
*/
#define is_template_param_or_nonreal_class_type(tp) \
  could_be_dependent_class_type(tp)

#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean is_overloadable_type(a_type_ptr type);
extern a_boolean is_overloadable_handle_type(a_type_ptr type);
extern a_boolean is_potential_conv_function_source(a_type_ptr type);
extern a_boolean is_overloadable_first_operand_type(a_type_ptr type);
#endif /* !STANDALONE_UTILITY_PROGRAM */

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
extern a_boolean is_or_contains_member_of_uncompleted_class(a_type_ptr  tp);
extern a_boolean template_args_involve_specific_class_type(
                                             a_template_arg_ptr  tap,
                                             a_type_ptr          class_type,
                                             a_boolean           members_only);
extern a_boolean type_involves_specific_class_type(a_type_ptr  tp,
                                                   a_type_ptr  class_type,
                                                   a_boolean   members_only);
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern void set_force_external_linkage_flag(a_type_ptr  type_ptr);
extern void force_definition_of_typeinfo_for(a_type_ptr type);
extern void set_used_in_exception_or_rtti_flag(a_type_ptr  type_ptr);
extern a_boolean is_or_contains_vla_type_with_unspecified_bound(a_type_ptr tp);
extern a_boolean is_variably_modified_type(a_type_ptr  tp);
extern a_boolean is_nonlocal_variably_modified_type(a_type_ptr  tp);
extern a_boolean type_has_side_effects(a_type_ptr  tp);
#if DO_IL_LOWERING
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
extern an_ELF_visibility_kind ELF_visibility_of_type(a_type_ptr  type);
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
extern void lower_vla_dimensions_in_type(a_type_ptr  tp);
#endif /* DO_IL_LOWERING */
extern a_boolean is_directly_variably_modified_type(a_type_ptr  tp);
extern a_type_ptr strip_routine_default_args(a_type_ptr  type);
extern a_type_ptr strip_qualifiers_from_param_types(a_type_ptr  type);
extern a_type_ptr strip_local_and_nonreal_typedefs(a_type_ptr  type,
                                                   a_boolean   local_only);
extern a_type_ptr remove_assoc_vla_dimensions(a_type_ptr  type);

#if !STANDALONE_UTILITY_PROGRAM
extern void diagnose_use_of_deprecated_or_unavailable_type(
                                                      a_type_ptr         type,
                                                      a_source_position  *pos);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_boolean routine_linkages_are_compatible(
                                           a_name_linkage_kind  nlk1,
                                           a_name_linkage_kind  nlk2,
                                           a_boolean            is_impl_conv);

/*
Return TRUE if a routine type is the type of a nonstatic member function.
The type must be known to be a routine type (not, for example, an error type),
but it may have typerefs on top of it.
*/
#define routine_type_is_nonstatic_member_function(routine_type)       \
 (rout_type_supp(skip_typerefs(routine_type))->this_class != NULL)

extern a_type_ptr f_implicit_this_param_type_of(a_type_ptr  routine_type);

/*
Synthesize the type of the implicit "this" parameter of a routine if the
underlying class exists (nonstatic members and pointer-to-members); otherwise
NULL.
*/
#define implicit_this_param_type_of(rout_type)                         \
  (routine_type_is_nonstatic_member_function(rout_type) ?              \
                              f_implicit_this_param_type_of(rout_type) : NULL)

/*
Extract a pointer to a base classes list for a class type.  This macro
may be called only for class, struct, and union types and only in C++ mode.
*/
#define base_classes_of(tp) \
  ((tp)->variant.class_struct_union.extra_info->base_classes)

/*
Extract a pointer to a direct base classes list for a class type.  This macro
may be called only for class, struct, and union types and only in C++ mode.
*/
#define direct_base_classes_of(tp) \
  ((tp)->variant.class_struct_union.extra_info->direct_base_classes)

#if IA64_ABI

/*
Extract a pointer to a base classes list for a class type, as with
"base_classes_of", but given in the order found by a preorder traversal
of the class hierarchy.
*/
#define preorder_base_classes_of(tp) \
  ((tp)->variant.class_struct_union.extra_info->preorder_base_classes)

#endif /* IA64_ABI */

/*
Extract the pointer to the template that generated a class type.
*/
#define assoc_template_of(tp) \
  ((tp)->variant.class_struct_union.extra_info->assoc_template)

/*
Return TRUE if "tp" is the prototype instantiation of a class template
that was declared as exported.
*/
#define class_is_exported(tp)						\
  (tp->variant.class_struct_union.is_prototype_instantiation &&		\
   tp->variant.class_struct_union.extra_info->assoc_template->is_exported)

/*
Macro that extracts the underlying enum type from an integral type,
or NULL if there is no underlying enum type.  Used for enum
compatibility checking.  The type must be an integral type, and
not even a typeref on top of an integral type.
*/
#define underlying_enum_type(tp)                                      \
  ((tp)->variant.integer.enum_type ?                                  \
          (tp) :                                                      \
          (tp)->variant.integer.enum_info.affiliated_type)


/* Bit vector used to pass flags into traverse_type_tree.  Each bit
   represents a flag. */
typedef int a_type_tree_traversal_flag_set;
/* Constants defining bits in the input bit vector used in calls to
   traverse_type_tree. */
#define TTT_NO_INPUT_FLAGS 0x0
#define TTT_RETURN_TYPE 0x1
			/* When the type being traversed is a function type,
			   apply the predicate check to the return type. */
#define TTT_PARAM_TYPES 0x2
			/* When the type being traversed is a function type,
			   apply the predicate check to the parameter types. */
#define TTT_THIS_PARAM_TYPE 0x4
			/* When the type being traversed is a function type,
			   apply the predicate check to the implicit "this"
			   parameter type. */
#define TTT_TEMPLATE_ARGS 0x8
			/* When the type being traversed is a class type,
			   apply the predicate check to its template args
                           (if it is a template class). */
#define TTT_SKIP_TYPEREFS 0x10
			/* Skip over typerefs before applying the predicate
			   check to a given type. */
#define TTT_SKIP_TYPEDEFS 0x20
			/* Skip over typedefs before applying the predicate
			   check to a given type. */
#define TTT_EXCEPTION_SPECS 0x40
			/* When the type being traversed is a function type,
			   apply the predicate check to the exception
			   specification list. */
#define TTT_STOP_AT_TYPEDEFS 0x80
			/* When the type encountered is a typedef, stop
			   the traversal. */
#define TTT_DEDUCED_CONTEXTS_ONLY 0x100
			/* When the type is traversed, only consider contexts
			   in which a template argument value can be deduced.
			   This ignores template parameters used in
			   the parent classes of a type (e.g., ignore
			   the T in A<T>::B) and nontype template
			   parameters used in expression contexts. */
#define TTT_PARENT_CLASSES 0x200
			/* When the type being traversed is a class member,
			   also traverse its parent type. */
#define TTT_DECLTYPE_AND_TYPEOF_EXPRS 0x400
			/* If a decltype or typeof typeref is found, traverse
			   the expression under the decltype or typeof. */
#define TTT_CLI_GENERIC_PARAMETERS 0x800
			/* If the type is a C++/CLI constraint type, traverse
			   the associated generic parameter. */
#define TTT_NONREAL_TEMPLATE_ARGS 0x1000
			/* When the type being traversed is a class type,
			   apply the predicate check to its template args,
			   but only if the type is a nonreal type. */
#define TTT_TYPE_OF_NONTYPE_ARG 0x2000
			/* TRUE if the type of nontype template arguments
			   should be traversed.  This should only be used in
			   conjunction with TTT_DEDUCED_CONTEXTS_ONLY, by
			   calling add_implicit_ttt_flags. */
#define TTT_SCAN_ALIAS_TEMPLATE_ARGS 0x4000
			/* TRUE if the template arguments of an alias
			   template or alias template specialization, as
			   well as the template arguments of the parents of
			   a member template or alias template, should be
			   scanned (if requested by the relevant flags),
			   even if TTT_STOP_AT_TYPEDEFS is TRUE. */


/* Type of service function called by traverse_type_tree to return TRUE or
   FALSE status regarding a given type in a type tree. */
typedef a_boolean a_type_predicate_function(a_type_ptr tp, a_boolean *flag);
typedef a_type_predicate_function *a_type_predicate_function_ptr;

/* Type of post-order service function called by traverse_type_tree_full for a
   type with its traversal result. */
typedef void a_type_post_order_function(a_type_ptr tp, a_boolean result);
typedef a_type_post_order_function *a_type_post_order_function_ptr;

a_boolean traverse_type_tree_full(a_type_ptr                      type_ptr,
                                  a_type_predicate_function_ptr   func,
                                  a_type_post_order_function_ptr  pofunc,
                                  a_type_tree_traversal_flag_set  flags);
a_boolean traverse_type_tree(a_type_ptr                     type_ptr,
                             a_type_predicate_function_ptr  func,
                             a_type_tree_traversal_flag_set flags);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_const_char *uuid_string_of_type(a_type_ptr  type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void set_declspec_align(a_type_ptr         type,
                               a_targ_alignment   alignment,
                               a_source_position  *pos);

#if UPC_EXTENSIONS_ALLOWED
extern a_upc_block_size f_get_upc_block_size(a_type_ptr  tp,
                                             a_boolean   top_level);

extern a_boolean is_underlying_shared_qualified_type(a_type_ptr tp);
extern a_boolean is_underlying_threads_dimensioned_array_type(a_type_ptr tp);
extern void fixup_upc_block_size (a_type_ptr array_type);
extern a_boolean is_shared_void_star_type(a_type_ptr tp);
extern a_targ_size_t upc_local_type_size(a_type_ptr tp);

#define typeref_is_shared_qualified(tp)                                 \
 (((tp)->variant.typeref.qualifiers & TQ_UPC_SHARED) != 0)

#define is_ptr_to_shared_type(tp)                                       \
 (is_pointer_type(tp) &&                                                \
  is_underlying_shared_qualified_type(type_pointed_to(tp)))

#define is_shared_qualified_type(tp)                                    \
  ((get_type_qualifiers(tp) & TQ_UPC_SHARED) != 0)

#define get_upc_block_size(tp)                                          \
  (((tp)->kind == (a_type_kind)tk_typeref ||                            \
    (tp)->kind == (a_type_kind)tk_array) ?                              \
      (f_get_upc_block_size((tp), /*top_level=*/C_mode())) :            \
      UPC_BLOCK_SIZE_NONE)

#define get_underlying_upc_block_size(tp)                               \
  (((tp)->kind == (a_type_kind)tk_typeref ||                            \
    (tp)->kind == (a_type_kind)tk_array) ?                              \
      (f_get_upc_block_size((tp), /*top_level=*/FALSE)) :               \
      UPC_BLOCK_SIZE_NONE)

#endif /* UPC_EXTENSIONS_ALLOWED */

/*
Return TRUE if type is a prototype instantiation.  type is required to be
a class type.
*/
#define is_prototype_instantiation_type(type)				\
  ((type)->variant.class_struct_union.is_prototype_instantiation)

/*
Return TRUE if type is a C++/CLI generic definition.  type is required to be
a class type.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_cli_generic_definition_type(type)				\
  ((type)->variant.class_struct_union.is_generic_definition)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,is_cli_generic_definition_type)*/
#define is_cli_generic_definition_type(type) FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Return TRUE if type is an instance of a C++/CLI generic that is an
open constructed type.  type is required to be a class type.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_cli_open_constructed_instance(type)				\
  ((type)->variant.class_struct_union.is_open_constructed_type)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,is_cli_open_constructed_instance)*/
#define is_cli_open_constructed_instance(type) FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Return TRUE if type is an instantiation of a C++/CLI generic.  type is
required to be a class type.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_cli_generic_instance_type(type)				\
  ((type)->variant.class_struct_union.is_generic_instance)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,is_cli_generic_instance_type)*/
#define is_cli_generic_instance_type(type) FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Return TRUE if type is the type used to represent a C++/CLI generic
constraint.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_cli_generic_constraint(type)				\
  (is_immediate_class_type(type) &&				\
   (type)->variant.class_struct_union.is_generic_constraint)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,is_cli_generic_constraint)*/
#define is_cli_generic_constraint(type) FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Return TRUE if type is a prototype instantiation or a C++/CLI generic
definition.  type is required to be a class type.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_prototype_instantiation_or_cli_generic_type(type)		\
  (is_prototype_instantiation_type(type) ||				\
   is_cli_generic_definition_type(type))
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define is_prototype_instantiation_or_cli_generic_type(type)		\
  (is_prototype_instantiation_type(type))
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean in_definition_of_class(a_type_ptr  tp);

extern a_boolean virtual_base_class_is_indirect(a_base_class_ptr vbcp,
                                                a_type_ptr       class_type);

#define function_type_params(rtp)                                       \
  (rout_type_supp(rtp)->param_type_list)

extern void types_early_init(void);

extern int32_t *min_template_arguments_for_type(a_type_ptr   tp,
                                                a_symbol_ptr template_sym);

#if MICROSOFT_EXTENSIONS_ALLOWED

extern a_type_ptr system_type_from_fundamental_type(a_type_ptr tp);

extern a_type_ptr fundamental_type_from_system_type(a_type_ptr tp);

extern a_type_ptr map_cli_system_type_to_fundamental_type(a_type_ptr tp);

extern a_boolean is_value_class_or_fundamental_type(a_type_ptr tp);

extern a_boolean is_cli_enum_type(a_type_ptr tp);

extern a_boolean is_cli_param_array_routine_type(a_type_ptr tp);

extern void error_if_cppcx_public_global_type(
                                             a_type_ptr            tp,
                                             a_source_position_ptr error_pos);


#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean compatible_ms_bit_field_container_types(a_type_ptr tp1,
                                                         a_type_ptr tp2);

extern a_targ_alignment check_explicit_enum_alignment(
                                              a_type_ptr       type,
                                              a_targ_alignment base_alignment);

inline a_boolean has_explicit_this_parameter(a_type_ptr  rtp)
/*
Return TRUE if the given routine type represents the type of a function with
an explicit "this" parameter.
*/
{
  a_param_type_ptr  ptp;

  check_assertion(type_is(rtp, tk_routine));
  ptp = function_type_params(rtp);
  return ptp != NULL && ptp->is_explicit_this;
}  /* has_explicit_this_parameter */

inline a_param_type_ptr first_nonobject_param(a_type_ptr rtp)
/*
Return the first non-object parameter of the given routine type, or NULL
if there is none.  When the function has an explicit "this" parameter,
that parameter is skipped.
*/
{
  a_param_type_ptr ptp;

  check_assertion(type_is(rtp, tk_routine));
  ptp = function_type_params(rtp);
  if (ptp != NULL && ptp->is_explicit_this) ptp = ptp->next;
  return ptp;
}  /* first_nonobject_param */

extern void types_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef TYPES_H */

