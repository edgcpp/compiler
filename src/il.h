/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

il.h -- Declarations related to the intermediate language.

*/

/* Avoid including these declarations more than once. */
#ifndef IL_H
#define IL_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

#include "il_def.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

namespace detail {

/*
The following specializations provide Is_trivially_copyable and
Is_trivially_destructible support for a_tagged_pointer and a_token_kind.  These
are declared here to keep il_def.h free from C++ specializations.
*/

template<>
struct Is_trivially_copyable_edg_impl<a_tagged_pointer> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_copyable_edg_impl<a_token_kind> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<a_tagged_pointer> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<a_token_kind> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

}  /* namespace detail */

inline a_boolean operator==(a_tagged_pointer ptr1,
                            a_tagged_pointer ptr2)
/*
Return TRUE if the tagged pointer represented by ptr1 is the same as the tagged
pointer represented by ptr2.  If both tagged pointers contain NULL pointers and
equivalent tags then the tagged pointers are considered the same.
*/
{
  a_boolean result = TRUE;

  if (ptr1.kind != ptr2.kind) {
    result = FALSE;
  } else if (ptr1.ptr != ptr2.ptr) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(a_tagged_pointer ptr1,
                            a_tagged_pointer ptr2)
/*
Return TRUE if the tagged pointer represented by ptr1 is not the same as the
tagged pointer represented by ptr2.  If both tagged pointers contain NULL
pointers and equivalent tags then the tagged pointers are considered the same.
*/
{
  return !(ptr1 == ptr2);
}  /* operator!= */


inline uintptr_t hash_ptr(a_tagged_pointer ptr)
/*
Compute a hash for the given C-string.  The hash must be appropriate for
Ptr_map.
*/
{
  return (uintptr_t)ptr.ptr;
}  /* hash_ptr */


inline a_tagged_pointer canonicalize_tagged_ptr(an_il_entry_kind kind,
                                                char             *ptr)
/*
Given the kind and pointer value to form a tagged pointer, return a tagged
pointer that points to the canonical IL entity.
*/
{
  a_tagged_pointer result;

  result.kind = kind;
  switch (kind) {
    case iek_template:
      { a_template_ptr templ = (a_template_ptr)ptr;

        result.ptr = (char*)templ->canonical_template;
      }
      break;
    default:
      result.ptr = ptr;
      break;
  }  /* switch */
  return result;
}  /* canonicalize_tagged_ptr */


inline a_tagged_pointer canonicalize_tagged_ptr(a_tagged_pointer tag)
/*
Given a tagged pointer, return an equivalent tagged pointer that points to the
canonical IL entity.
*/
{
  return canonicalize_tagged_ptr(tag.kind, tag.ptr);
}  /* canonicalize_tagged_ptr */


/*
This function returns the corresponding IL entry kind for the IL entry type
specializing it.

The map_il_type_to_kind macro is used to set up the specializations serving as
implementation details.  There should be a macro invocation for each IL entry
type and its corresponding IL entry kind.
*/
template<typename an_IL_type>
constexpr an_il_entry_kind type_to_il_entry_kind()
#if HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES
                                                   = delete;
#else /* !HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES */
  { return iek_none; }
#endif /* HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES */

#define map_il_type_to_kind(type, kind)                                       \
  template<>                                                                  \
  constexpr an_il_entry_kind type_to_il_entry_kind<type>() { return kind; }

map_il_type_to_kind(a_source_file, iek_source_file)
map_il_type_to_kind(a_constant, iek_constant)
map_il_type_to_kind(a_param_type, iek_param_type)
map_il_type_to_kind(a_routine_type_supplement, iek_routine_type_supplement)
map_il_type_to_kind(a_based_type_list_member, iek_based_type_list_member)
map_il_type_to_kind(a_type, iek_type)
map_il_type_to_kind(a_variable, iek_variable)
map_il_type_to_kind(a_field, iek_field)
map_il_type_to_kind(an_exception_specification, iek_exception_specification)
map_il_type_to_kind(an_exception_specification_type,
                    iek_exception_specification_type)
map_il_type_to_kind(a_routine, iek_routine)
map_il_type_to_kind(a_label, iek_label)
map_il_type_to_kind(an_expr_node, iek_expr_node)
map_il_type_to_kind(a_for_loop, iek_for_loop)
map_il_type_to_kind(a_range_based_for_loop, iek_range_based_for_loop)
#if MICROSOFT_EXTENSIONS_ALLOWED
map_il_type_to_kind(a_for_each_loop, iek_for_each_loop)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
map_il_type_to_kind(a_switch_case_entry, iek_switch_case_entry)
map_il_type_to_kind(a_switch_stmt_descr, iek_switch_stmt_descr)
map_il_type_to_kind(a_handler, iek_handler)
map_il_type_to_kind(a_try_supplement, iek_try_supplement)
#if MICROSOFT_EXTENSIONS_ALLOWED
map_il_type_to_kind(a_microsoft_try_supplement, iek_microsoft_try_supplement)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
map_il_type_to_kind(a_block, iek_block)
map_il_type_to_kind(a_statement, iek_statement)
map_il_type_to_kind(an_object_lifetime, iek_object_lifetime)
map_il_type_to_kind(a_scope, iek_scope)
#if C99_IL_EXTENSIONS_SUPPORTED
map_il_type_to_kind(an_internal_complex_value, iek_internal_complex_value)
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
map_il_type_to_kind(a_namespace, iek_namespace)
map_il_type_to_kind(a_using_decl, iek_using_decl)
map_il_type_to_kind(a_dynamic_init, iek_dynamic_init)
map_il_type_to_kind(a_local_static_variable_init,
                    iek_local_static_variable_init)
map_il_type_to_kind(a_vla_dimension, iek_vla_dimension)
#if DO_IL_LOWERING && IA64_ABI
map_il_type_to_kind(a_vcall_offset_entry , iek_vcall_offset_entry)
#endif /* DO_IL_LOWERING && IA64_ABI */
#if MICROSOFT_EXTENSIONS_ALLOWED
map_il_type_to_kind(a_partial_class_body, iek_partial_class_body)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
map_il_type_to_kind(an_overriding_virtual_function,
                    iek_overriding_virtual_function)
map_il_type_to_kind(a_derivation_step, iek_derivation_step)
map_il_type_to_kind(a_base_class_derivation, iek_base_class_derivation)
map_il_type_to_kind(a_base_class, iek_base_class)
map_il_type_to_kind(a_class_list_entry, iek_class_list_entry)
map_il_type_to_kind(a_routine_list_entry, iek_routine_list_entry)
map_il_type_to_kind(a_variable_list_entry, iek_variable_list_entry)
map_il_type_to_kind(a_constant_list_entry, iek_constant_list_entry)
map_il_type_to_kind(a_class_type_supplement, iek_class_type_supplement)
map_il_type_to_kind(a_template_param_type_supplement,
                    iek_template_param_type_supplement)
map_il_type_to_kind(a_constructor_init, iek_constructor_init)
map_il_type_to_kind(an_asm_entry, iek_asm_entry)
#if GNU_EXTENSIONS_ALLOWED
map_il_type_to_kind(an_asm_operand, iek_asm_operand)
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
map_il_type_to_kind(an_asm_operand_constraint, iek_asm_operand_constraint)
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
map_il_type_to_kind(a_named_register_list, iek_named_register_list)
map_il_type_to_kind(a_label_list, iek_label_list)
#endif /* GNU_EXTENSIONS_ALLOWED */
map_il_type_to_kind(a_template_arg, iek_template_arg)
map_il_type_to_kind(a_new_delete_supplement, iek_new_delete_supplement)
#if MICROSOFT_EXTENSIONS_ALLOWED
map_il_type_to_kind(a_gcnew_supplement, iek_gcnew_supplement)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
map_il_type_to_kind(a_throw_supplement, iek_throw_supplement)
map_il_type_to_kind(a_condition_supplement, iek_condition_supplement)
#if !ABI_CHANGES_FOR_RTTI
map_il_type_to_kind(an_accessible_base_class, iek_accessible_base_class)
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
map_il_type_to_kind(an_eh_prologue_supplement, iek_eh_prologue_supplement)
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
map_il_type_to_kind(a_source_sequence_entry, iek_source_sequence_entry)
map_il_type_to_kind(a_src_seq_secondary_decl, iek_src_seq_secondary_decl)
map_il_type_to_kind(a_src_seq_end_of_construct, iek_src_seq_end_of_construct)
map_il_type_to_kind(a_src_seq_sublist, iek_src_seq_sublist)
map_il_type_to_kind(an_instantiation_directive, iek_instantiation_directive)
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
map_il_type_to_kind(a_scope_orphaned_list_header,
                    iek_scope_orphaned_list_header)
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_HIDDEN_NAMES_IN_IL
map_il_type_to_kind(a_hidden_name, iek_hidden_name)
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
map_il_type_to_kind(a_pragma, iek_pragma)
map_il_type_to_kind(a_template, iek_template)
#if RECORD_MACROS_IN_IL
map_il_type_to_kind(a_macro, iek_macro)
#endif /* RECORD_MACROS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
map_il_type_to_kind(a_per_instantiation_needed_flags_entry,
                    iek_per_instantiation_needed_flags_entry)
#endif /* ONE_INSTANTIATION_PER_OBJECT */
map_il_type_to_kind(an_element_position, iek_element_position)
#if EXTRA_SOURCE_POSITIONS_IN_IL
map_il_type_to_kind(a_decl_position_supplement, iek_decl_position_supplement)
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
map_il_type_to_kind(a_template_decl, iek_template_decl)
map_il_type_to_kind(a_requires_clause, iek_requires_clause)
map_il_type_to_kind(a_template_parameter, iek_template_parameter)
map_il_type_to_kind(a_name_reference, iek_name_reference)
map_il_type_to_kind(a_name_qualifier, iek_name_qualifier)
#if MICROSOFT_EXTENSIONS_ALLOWED
map_il_type_to_kind(an_ms_attribute, iek_ms_attribute)
map_il_type_to_kind(an_ms_attribute_arg, iek_ms_attribute_arg)
map_il_type_to_kind(a_custom_ms_attribute_arg, iek_custom_ms_attribute_arg)
map_il_type_to_kind(a_property_index_type, iek_property_index_type)
map_il_type_to_kind(a_property_or_event_descr, iek_property_or_event_descr)
map_il_type_to_kind(a_generic_constraint_clause, iek_generic_constraint_clause)
map_il_type_to_kind(a_generic_constraint, iek_generic_constraint)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
map_il_type_to_kind(a_seq_number_lookup_entry, iek_seq_number_lookup_entry)
#if RECORD_MACRO_INVOCATIONS
map_il_type_to_kind(a_macro_invocation_record_block,
                    iek_macro_invocation_record_block)
#endif /* RECORD_MACRO_INVOCATIONS */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
map_il_type_to_kind(an_ms_if_exists, iek_ms_if_exists)
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
map_il_type_to_kind(a_local_expr_node_ref, iek_local_expr_node_ref)
map_il_type_to_kind(a_static_assertion, iek_static_assertion)
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if GENERATE_LINKAGE_SPEC_BLOCKS
map_il_type_to_kind(a_linkage_spec_block, iek_linkage_spec_block)
#endif /* GENERATE_LINKAGE_SPEC_BLOCKS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
map_il_type_to_kind(a_local_scope_ref, iek_local_scope_ref)
map_il_type_to_kind(an_il_entity_list_entry, iek_il_entity_list_entry)
map_il_type_to_kind(a_lambda, iek_lambda)
map_il_type_to_kind(a_lambda_capture, iek_lambda_capture)
map_il_type_to_kind(an_attribute, iek_attribute)
map_il_type_to_kind(an_attribute_arg, iek_attribute_arg)
map_il_type_to_kind(an_attribute_group, iek_attribute_group)
map_il_type_to_kind(a_typeref_type_supplement, iek_typeref_type_supplement)
map_il_type_to_kind(an_integer_type_supplement, iek_integer_type_supplement)
#if MICROSOFT_EXTENSIONS_ALLOWED
map_il_type_to_kind(a_cli_metadata_file, iek_cli_metadata_file)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
map_il_type_to_kind(a_gnu_routine_supplement, iek_gnu_routine_supplement)
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
map_il_type_to_kind(a_coroutine_descr, iek_coroutine_descr)
map_il_type_to_kind(a_variable_template_info, iek_variable_template_info)
#if MICROSOFT_EXTENSIONS_ALLOWED
map_il_type_to_kind(an_event_interface, iek_event_interface)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
map_il_type_to_kind(a_subobject_path, iek_subobject_path)
map_il_type_to_kind(a_constexpr_if, iek_constexpr_if)
map_il_type_to_kind(a_module, iek_module)
map_il_type_to_kind(a_module_import_decl, iek_module_import_decl)
map_il_type_to_kind(a_token_sequence, iek_token_sequence)
map_il_type_to_kind(a_token_sequence_entry, iek_token_sequence_entry)
map_il_type_to_kind(a_scoped_expression, iek_scoped_expression)
map_il_type_to_kind(a_data_member_spec, iek_data_member_spec)


template<typename an_IL_type>
inline a_tagged_pointer make_tagged_ptr(an_IL_type *val)
/*
Convert the given IL entity pointer into a tagged pointer.
*/
{
  return canonicalize_tagged_ptr(type_to_il_entry_kind<an_IL_type>(),
                                 (char*)val);
}  /* make_tagged_ptr */


/* Current memory region number for IL information. */
EXTERN_THREAD a_memory_region_number
		curr_il_region_number;

/* A dummy name for placeholders. */
EXTERN_THREAD a_const_char
		*routine_move_placeholder_name
#if VAR_INITIALIZERS
                         = "<routine move placeholder>"
#endif /* VAR_INITIALIZERS */
                                                       ;

#if ORPHAN_PROCESSING_NEEDED
/*
It is necessary to maintain a list of IL entries that are allocated in
the file scope memory region but accessed from the function scope
region.  These lists are walked during IL file writing and reading
and when displaying the IL to ensure that all IL entries are
visited.  Note, the first_entry and last_entry point to the first
byte of the IL entry.  The address of the next entry in the linked list
precedes the IL entry.
*/
typedef struct an_orphaned_il_entry_list {
  char *first_entry;	/* Pointer to the first IL entry of a specific
			   kind in a linked list. */
  char *last_entry;	/* Pointer to the last IL entry of a specific
			   kind in a linked list. */
} an_orphaned_il_entry_list;

EXTERN_THREAD an_orphaned_il_entry_list
		orphaned_file_scope_il_entries[(int)iek_last];
			/* Array of orphaned IL entry lists containing
			   individual orphaned file scope IL entries. */
#endif /* ORPHAN_PROCESSING_NEEDED */

/*
If IL lowering is to be done, IL entry prefixes have a flag that indicates
whether or not IL lowering has visited them yet.  This is the initial
value for that flag when it is cleared.
*/
/* Not conditional because it's also used by trans_copy.c. */
EXTERN_THREAD a_boolean
		initial_value_for_il_lowering_flag;

/*
The default "routine name linkage" is the value to which the
routine_name_linkage field of a routine type supplement is initialized.
When a value other than the language default is required, the caller of
alloc_type will make the correction.
*/
EXTERN_THREAD a_name_linkage_kind
		default_routine_name_linkage;

/* The various type_info types. */
enum a_type_info_kind {
  tik_user,             /* The user-visible std::type_info type.  This
			   type must be first. */
  /* tik_implementation is the type used by the runtime to implement
     type_info, which must start with the type_info fields, but may
     have additional information following that. */
#if IA64_ABI
  /* Additional derived classes of type_info defined by the IA-64 ABI: */
  tik_implementation = tik_user,
			/* An alias for the user type.	In some
			   places (like the exception_type_spec), the
			   front end creates pointers to the
			   "implementation" type_info type, and
			   providing this alias makes it unnecessary to
			   conditionalize that code. */
  tik_fundamental,	/* Void, integral, floating, decltype(nullptr)
			   types. */
  tik_enum,		/* Enumeration types. */
  tik_array,		/* Array types. */
  tik_function,		/* Function types. */
  tik_class,		/* Class types without inheritance. */
  tik_si_class,		/* Class types with single, public,
			   non-virtual inheritance. */
  tik_vmi_class,	/* Other class types. */
  tik_pbase,		/* Base class for pointers and
			   pointers-to-members. */
  tik_pointer,		/* Pointer types. */
  tik_ptr_to_member,	/* Pointer-to-member types. */
#else /* !IA64_ABI */
  tik_implementation,	/* Implementation type. */
#endif /* !IA64_ABI */
  tik_last
};


/* Names of type_info types. */
EXTERN_CONSTINIT_ARRAY(a_const_char*, type_info_names, tik_last + 1)
#if VAR_INITIALIZERS
= { 
  "type_info",			/* tik_user */
#if IA64_ABI 
  "__fundamental_type_info",	/* tik_fundamental */
  "__enum_type_info",		/* tik_enum */
  "__array_type_info",		/* tik_array */
  "__function_type_info",	/* tik_function */
  "__class_type_info",		/* tik_class */
  "__si_class_type_info",	/* tik_si_class */
  "__vmi_class_type_info",	/* tik_vmi_class */
  "__pbase_type_info",		/* tik_pbase */
  "__pointer_type_info",	/* tik_pointer */
  "__pointer_to_member_type_info", /* tik_ptr_to_member */
#else /* !IA64_ABI */
  NULL,				/* tik_implementation */
#endif /* !IA64_ABI */
  NULL				/* tik_last */
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(type_info_names)

/* Pointer types for types defined in il_to_str.h. */
typedef struct an_il_to_str_output_control_block
                                        *an_il_to_str_output_control_block_ptr;

EXTERN_THREAD a_type_ptr
		*internal_type_array;
			/* Pointer to an array of types usable by internal
			   expression code. */

EXTERN_THREAD a_host_large_unsigned
		n_internal_types;
			/* Number of elements pointed to by
			   internal_type_array. */

EXTERN_THREAD a_type_ptr
		type_of_type_info;
			/* Points to the definition of the type_info type
			   returned by typeid.	In some configurations, this
			   type is identified by a #pragma define_type_info
			   that immediately precedes the class definition of
			   type_info. */
EXTERN_THREAD a_type_ptr
		types_of_type_info[(int)tik_last + 1];
			/* The user-visible type_info types.  The element
			   with index tik_user has the same value as
			   type_of_type_info. */
EXTERN_THREAD a_type_ptr
		type_of_align_val_t;
			/* Points to the definition of std::align_val_t. */

#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN_THREAD a_type_ptr
		type_of_guid;
			/* Points to the definition of the _GUID struct. */

EXTERN_THREAD a_boolean
		in_microsoft_implementation_key_mapping_region;
			/* Indicates whether the source being parsed is inside
			   a region of code delimited by "#pragma
			   start_map_region" and "#pragma stop_map_region". */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

EXTERN_THREAD a_boolean
		okay_to_eliminate_unneeded_il_entries;
			/* When TRUE unneeded entities may be pruned from the
			   IL tree; otherwise, pruning is suppressed even if
			   entities are determined to be unneeded. Always
			   FALSE when MAINTAIN_NEEDED_FLAGS is FALSE.
			   Otherwise, controlled by command line option
			   --[no_]remove_unneeded_entities; also FALSE if
			   templates appear in the source program and
			   template instantiation is not under the control
			   of the front end (e.g., when the C++-generating
			   back end is used).  */

EXTERN_THREAD a_stdc_pragma_value
		curr_fp_contract_state;
			/* Used in C99 mode to reflect the current setting
			   of the fp_contract state, which is set using the
			   STDC FP_CONTRACT pragma. */

EXTERN_THREAD a_stdc_pragma_value
		curr_fenv_access_state;
			/* Used in C99 and C++11 modes to reflect the
			   current setting of the fenv_access state, which
			   is set using the STDC FENV_ACCESS pragma. */

EXTERN_THREAD a_stdc_pragma_value
		curr_cx_limited_range_state;
			/* Used in C99 mode to reflect the current setting
			   of the cx_limited_range state, which is set using
			   the STDC CX_LIMITED_RANGE pragma. */

#if FIXED_POINT_ALLOWED

EXTERN_THREAD a_stdc_pragma_value
		curr_fx_full_precision_state;
			/* Used to reflect the current setting of the
			   fx_full_precision state, which is set using the
			   STDC FX_FULL_PRECISION pragma. */

EXTERN_THREAD a_stdc_pragma_value
		curr_fx_fract_overflow_state;
			/* Used to reflect the current setting of the
			   fx_fract_overflow state, which is set using the
			   STDC FX_FRACT_OVERFLOW pragma. */

EXTERN_THREAD a_stdc_pragma_value
		curr_fx_accum_overflow_state;
			/* Used to reflect the current setting of the
			   fx_accum_overflow state, which is set using the
			   STDC FX_ACCUM_OVERFLOW pragma. */

#endif /* FIXED_POINT_ALLOWED */

#if UPC_EXTENSIONS_ALLOWED

EXTERN_THREAD a_upc_access_method
		curr_upc_access_method;
			/* Used in UPC mode to reflect the last setting of
			   the UPC access mode through the UPC pragma. */

#endif /* UPC_EXTENSIONS_ALLOWED */

#if ONE_INSTANTIATION_PER_OBJECT

EXTERN_THREAD unsigned long
		needed_flag_bit_number;
			/* If non-zero, indicates that instead of the normal
			   "needed" flag in the source correspondence entry,
			   the so-numbered bit in the
			   per_instantiation_needed_flags bit vector is to
			   be tested and set by the "needed" flag
			   processing. */

/* Structure used to hold state between calls of
next_set_instantiation_needed_flag. */
typedef struct an_instantiation_needed_flags_scan_state
                                 *an_instantiation_needed_flags_scan_state_ptr;
typedef struct an_instantiation_needed_flags_scan_state {
  a_per_instantiation_needed_flags_entry_ptr
		curr_segment;
			/* Current segment of the bit vector.  NULL after
			   falling off the end. */
  unsigned long	first_bit_this_segment;
			/* Number of the first bit in the current segment. */
  int		byte_number;
			/* Current byte number in the current segment. */
  int		bit_number;
			/* Current bit number in the current byte.  -1 if we
			   haven't started the current byte yet. */
} an_instantiation_needed_flags_scan_state;

extern void clear_instantiation_needed_flags_scan_state(
                          an_instantiation_needed_flags_scan_state_ptr infssp,
                          a_source_correspondence                      *scp);
extern unsigned long next_set_instantiation_needed_flag(
                          an_instantiation_needed_flags_scan_state_ptr infssp);

extern unsigned long assign_instantiation_needed_bit_number(void);

#endif /* ONE_INSTANTIATION_PER_OBJECT */

#if PARENS_IN_IL
extern an_expr_node_ptr f_skip_parens(an_expr_node_ptr expr);
#endif /* PARENS_IN_IL */

/*
Strip parentheses off an expression and return the underlying expression.
*/
#if PARENS_IN_IL
#define skip_parens(expr) f_skip_parens(expr)
#else /* !PARENS_IN_IL */
#define skip_parens(expr) (expr)
#endif /* PARENS_IN_IL */

/*
Macro to access the parent_scope field of an IL entry.
*/
#define parent_scope_of(ptr)                                                \
  (((a_source_correspondence*)(ptr))->parent_scope)

/*
Macro that returns whether a parent scope was recorded for the given IL entry.
(This includes cases where the parent scope is recorded indirectly via an
entry of type a_local_scope_ref.)
*/
#define has_parent_scope(ptr)                                               \
  ((ptr)->source_corresp.parent_scope != NULL ||                            \
   (ptr)->source_corresp.parent_via_local_scope_ref)

extern a_scope_ptr f_get_parent_scope_of(a_source_correspondence_ptr  scp);

extern a_scope_ptr get_assoc_scope_of_il_entry(char             *entity,
                                               an_il_entry_kind kind);

/*
Macro to get the parent scope of an IL entry.  This differs from the macro
"parent_scope_of" (see above) in that it works even for entities in file scope
memory whose parent scope is in function scope memory.
*/
#define get_parent_scope_of(ptr)                                            \
  (((a_source_correspondence*)(ptr))->parent_via_local_scope_ref ?          \
                     f_get_parent_scope_of((a_source_correspondence*)(ptr)) \
                   : parent_scope_of(ptr))

/*
Macro that returns TRUE if an IL entry represents a scoped enumerator.
*/
#define scp_is_enum_member(scp)                                             \
  ((scp)->parent_scope != NULL &&                                           \
   (scp)->parent_scope->kind == (a_scope_kind)sck_enum)

/*
Macros that return TRUE if an IL entry represents a namespace member.
*/
#define scp_is_namespace_member(scp)                                        \
  ((scp)->parent_scope != NULL &&                                           \
   (scp)->parent_scope->kind == (a_scope_kind)sck_namespace)

#define is_namespace_member(ptr)                                            \
  (scp_is_namespace_member(&(ptr)->source_corresp))

/*
Macros that return TRUE if an IL entry represents a class or namespace member.
*/
#define scp_is_class_or_namespace_member(scp)                               \
  ((scp)->is_class_member || scp_is_namespace_member((scp)))

#define is_class_or_namespace_member(ptr)                                   \
  (scp_is_class_or_namespace_member(&(ptr)->source_corresp))

/*
Return the memory region for a given routine entry, or NULL_region_number
if the routine has no definition.
*/
#define mem_region_for_routine(rout)					\
  ((rout)->memory_region)

/*
Macro that returns the parent type of a scoped enumerator. 
*/
#if EXPENSIVE_CHECKING
/*lint -emacro(664,scp_parent_scoped_enum_type)*/
#define scp_parent_scoped_enum_type(scp)                                    \
  (check_assertion(scp_is_enum_member(scp)),                                \
   (scp)->parent_scope->variant.assoc_type)
#else /* !EXPENSIVE_CHECKING */
#define scp_parent_scoped_enum_type(scp)                                    \
  ((scp)->parent_scope->variant.assoc_type)
#endif /* EXPENSIVE_CHECKING */

/*
Macros that return the parent namespace of a namespace member. 
*/
#if EXPENSIVE_CHECKING
/*lint -emacro(664,scp_parent_namespace)*/
#define scp_parent_namespace(scp)                                           \
  (check_assertion(scp_is_namespace_member(scp)),                           \
   (scp)->parent_scope->variant.assoc_namespace)
#else /* !EXPENSIVE_CHECKING */
#define scp_parent_namespace(scp)                                           \
  ((scp)->parent_scope->variant.assoc_namespace)
#endif /* EXPENSIVE_CHECKING */

#define parent_namespace_of(ptr)                                            \
  (scp_parent_namespace(&(ptr)->source_corresp))

/*
Macros that return the parent namespace for a namespace member, and NULL for
entities that are neither namespace members nor class members.  (This macro
should not be used for class members.)
*/
#if EXPENSIVE_CHECKING && !defined(_lint)
#define scp_parent_namespace_or_null(scp)                                   \
  (check_assertion(!(scp)->is_class_member),                                \
   (scp_is_namespace_member((scp)) ? scp_parent_namespace((scp))            \
                                   : (a_namespace_ptr)NULL))
#else /* !(EXPENSIVE_CHECKING && !defined(_lint)) */
#define scp_parent_namespace_or_null(scp)                                   \
  (scp_is_namespace_member((scp)) ? scp_parent_namespace((scp))             \
                                  : (a_namespace_ptr)NULL)
#endif /* EXPENSIVE_CHECKING && !defined(_lint) */

#define parent_namespace_or_null(ptr)                                       \
  (scp_parent_namespace_or_null(&(ptr)->source_corresp))

/*
Macros that return the parent class of a class member. 
*/
#if defined(_lint)
/* When linting, duplicate the macro argument to catch side-effects that would
   be duplicated in the EXPENSIVE_CHECKING version, but don't call
   check_assertion since that results in spurious lint errors when the macro
   is used in a macro that itself duplicates its argument. */
/*lint -emacro(505 664,scp_parent_class)*/
#define scp_parent_class(scp)                                               \
  ((void)(scp)->is_class_member,                                            \
   (scp)->parent_scope->variant.assoc_type)
#else /* !defined(_lint) */
#if EXPENSIVE_CHECKING
#define scp_parent_class(scp)                                               \
  (check_assertion((scp)->is_class_member &&                                \
                   (scp)->parent_scope != NULL &&                           \
                   (scp)->parent_scope->kind ==                             \
                                    (a_scope_kind)sck_class_struct_union),  \
   (scp)->parent_scope->variant.assoc_type)
#else /* !EXPENSIVE_CHECKING */
#define scp_parent_class(scp)                                               \
  ((scp)->parent_scope->variant.assoc_type)
#endif /* EXPENSIVE_CHECKING */
#endif /* defined(_lint) */


#define parent_class_of(ptr)                                                \
  (scp_parent_class(&(ptr)->source_corresp))

/*
Macro that returns the parent class if the given pointer points to a class
member, and NULL otherwise.
*/
#define parent_class_or_null(ptr)                                           \
  ((ptr)->source_corresp.is_class_member ? parent_class_of(ptr)             \
                                         : (a_type_ptr)NULL)


inline a_boolean scp_is_module_imported(a_source_correspondence  *scp)
/*
Return TRUE if the given source correspondence is for an entity imported from
a module.
*/
{
  a_boolean  result;

  if (!scp->is_class_member) {
    result = scp->module_entity != NULL;
  } else {
    a_type_ptr  parent_class = scp_parent_class(scp);
    while (parent_class->source_corresp.is_class_member) {
      parent_class = parent_class_of(parent_class);
    }  /* if */
    result = parent_class->source_corresp.module_entity != NULL;
  }  /* if */
  return result;
}  /* scp_is_module_imported */

/*
Macro to determine whether an entity was imported from a module.
*/
#define is_module_imported(entry)                                            \
  (scp_is_module_imported(&(entry)->source_corresp))


extern a_string_view module_name_of(a_module_ptr mod);

extern a_string_view module_partition_name_of(a_module_ptr mod);

extern a_string module_full_name_of(a_module_ptr mod);

extern a_string_view header_unit_name_of(a_module_ptr mod);

extern a_boolean is_routine_definition_exported_inline(a_routine_ptr rp);

/*
Return TRUE if a dynamic_initializer is of a given kind.
*/
#define dyn_init_is(dip, dik)                                               \
  ((dip)->kind == (a_dynamic_init_kind)(dik))

/*
Return TRUE if a constant is of a given kind.
*/
#define constant_is(con, con_kind)                                          \
  ((con)->kind == (a_constant_repr_kind)(con_kind))

/*
Return TRUE if a ck_address entry is of the given kind.
*/
#define address_base_is(con, abk_kind)                                      \
  ((con)->variant.address.kind == (an_address_base_kind)(abk_kind))

/*
Return TRUE if a ck_template_param entry is of the given kind.
*/
#define tpck_is(con, tpck_kind)                                             \
  ((con)->variant.template_param.kind ==                                    \
                               (a_template_param_constant_kind)(tpck_kind))

/*
Return TRUE if cp is a ck_template_param/tpck_unknown_function constant.
*/
#define is_unknown_function_constant(cp) \
  ((cp)->kind == (a_constant_repr_kind)ck_template_param && \
   (cp)->variant.template_param.kind == \
        (a_template_param_constant_kind)tpck_unknown_function)

/*
Return TRUE if tp has the specified type kind.
*/
#define type_is(tp, type_kind)						\
  ((tp)->kind == (a_type_kind)(type_kind))

/*
Return TRUE if tp is a template type parameter pack.
*/
#define type_is_pack(tp)						\
  ((tp)->kind == (a_type_kind)tk_template_param &&			\
   (tp)->variant.template_param.is_pack)

/*
Return TRUE if tp (which is a tk_template_param type) has the specified
template parameter type kind.
*/
#define tptk_is(tp, k)							\
  ((tp)->variant.template_param.kind == (a_template_param_type_kind)k)

/*
Return TRUE if tp is a tk_template_param/tptk_param type that designates an
actual type template parameter (and not one of the special cases indicated
by certain values of its depth coordinate).
*/
#define type_is_actual_template_parameter(tp)                      \
  (type_is(tp, tk_template_param) &&                               \
   tptk_is(tp, tptk_param) &&                                      \
   (tp)->variant.template_param.extra_info->coordinates.depth > 0)

/*
Return TRUE if cp is a template nontype parameter pack.
*/
#define constant_is_pack(cp)						\
  ((cp)->kind == (a_constant_repr_kind)ck_template_param &&		\
   (cp)->variant.template_param.is_pack)

extern a_boolean type_is_nonreal(a_type_ptr	type);

extern a_boolean entity_is_nonreal(a_source_correspondence_ptr scp,
                                   an_il_entry_kind            kind);


inline a_boolean entity_is_nonreal(a_tagged_pointer entity)
/*
This is a convenience overload for entity_is_nonreal that uses a_tagged_pointer
instead of a source correspondence pointer and IL entry kind.  See the
documentation of
entity_is_nonreal(a_source_correspondence_ptr, an_il_entry_kind)
for more information.
*/
{
  return entity_is_nonreal((a_source_correspondence_ptr)entity.ptr,
                           entity.kind);
}  /* entity_is_nonreal */

#if DO_IL_LOWERING
/*
When doing IL lowering, prototype instantiations are ignored during the
lowering, mangling, and back end passes (since they aren't used).
*/
#define ignore_type_in_back_end(type) type_is_nonreal(type)

/*
Return TRUE if rout should be ignored by IL lowering and code generating
back ends.  This is TRUE for dependent template entities, deduction guides,
and C++20 consteval functions.
*/
#define ignore_routine_in_back_end(rout)				\
  ((rout)->is_prototype_instantiation ||                                \
   special_kind_is((rout), sfk_deduction_guide) ||                      \
   (rout)->is_consteval)

/*
Return TRUE if var should be ignored by IL lowering and code generating
back ends.  This is TRUE for dependent template entities as well as variables
used for structured bindings.
*/
#define ignore_variable_in_back_end(var)				\
  ((var)->is_prototype_instantiation || (var)->is_nonreal ||            \
   (var)->init_kind == ((an_init_kind)initk_binding))

/*
Return TRUE if constant should be ignored by IL lowering and code generating
back ends.  This is TRUE for dependent template entities.
*/
#define ignore_constant_in_back_end(constant) \
  ((constant)->kind == (a_constant_repr_kind)ck_template_param)
#else /* !DO_IL_LOWERING */
/*
In cases where we're not doing lowering, process prototype instantiations
(may be useful in cases where mangled names are desired).
*/
#define ignore_type_in_back_end(type) FALSE
#define ignore_routine_in_back_end(rout) FALSE
#define ignore_variable_in_back_end(var) FALSE
#define ignore_constant_in_back_end(constant) FALSE
#endif /* DO_IL_LOWERING */

extern a_routine_ptr lambda_body_for_closure(a_type_ptr	type);

extern a_namespace_ptr namespace_enclosing_class(a_type_ptr  tp);

extern a_routine_ptr routine_and_node_from_function_expr(
                                                       an_expr_node_ptr expr,
                                                       an_expr_node_ptr *node);

/*
Convenience macro to call routine_and_node_from_function_expr when the call
node is not needed.
*/
#define routine_from_function_expr(expr) \
  (routine_and_node_from_function_expr(expr, (an_expr_node_ptr*)NULL))
/*
Macro to test a routine entry's special_kind field.
*/
#define special_kind_is(rp, sfk)                                            \
  ((rp)->special_kind == (a_special_function_kind)(sfk))

inline a_boolean is_implicitly_declared_c_func(a_routine_ptr  rp)
/*
Return TRUE if rp is declared in C mode as the result of "calling" an unknown
identifier.  For example:

  int main() { return unknown(); }  // "unknown" is implicitly declared in C.
*/
{
  a_boolean  result = rp->compiler_generated &&
                      special_kind_is(rp, sfk_none) && C_mode();

#if BUILTIN_FUNCTIONS_ENABLED
  if (result &&
      rp->variant.builtin_function_kind != (a_builtin_function_kind)0) {
    result = FALSE;
  }  /* if */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
  return result;
}  /* is_implicitly_declared_c_func */

/*
Functions and macros to test whether a routine is a GNU built-in function.
*/
#if BUILTIN_FUNCTIONS_ENABLED

EXPAND a_boolean is_gnu_builtin_function(a_routine_ptr  rp)
/*
Return TRUE if and only if the given routine represents a GNU-style built-in
function.
*/
{
  return special_kind_is(rp, sfk_none) &&
         rp->variant.builtin_function_kind != (a_builtin_function_kind)0;
}  /* is_gnu_builtin_function */


EXPAND a_boolean is_specific_builtin(a_routine_ptr            rp,
                                     a_builtin_function_kind  bfk)
/*
Return TRUE if and only if the given routine represents a GNU-style built-in
function of the given kind.
*/
{
  return special_kind_is(rp, sfk_none) &&
         rp->variant.builtin_function_kind == (a_builtin_function_kind)bfk;
}  /* is_specific_builtin */


inline a_boolean is_call_to_builtin_function(an_expr_node_ptr        expr,
                                             a_builtin_function_kind bfk)
/*
Return TRUE if the expression is a call to a builtin function (when bfk
is bfk_none) or a call to a specific builtin function (otherwise).  Note that
this code occurs early in the compilation process of the front end so 0 is
used in place of bfk_none below (and convenience macros are also unavailable).
*/
{
  a_boolean result = FALSE;

  if (expr->kind == (an_expr_node_kind)enk_operation &&
      expr->variant.operation.kind == (an_expr_operator_kind)eok_call) {
    a_routine_ptr rp =
                  routine_from_function_expr(expr->variant.operation.operands);
    if (rp != NULL) {
      result = (bfk == (a_builtin_function_kind)0) ?
                                                  is_gnu_builtin_function(rp) :
                                                  is_specific_builtin(rp, bfk);
    }  /* if */
  }  /* if */
  return result;
}  /* is_call_to_builtin_function */


#define rout_is_specific_builtin(rp, bfk)                                   \
   is_specific_builtin(rp,(a_builtin_function_kind)(bfk))
#else /* !BUILTIN_FUNCTIONS_ENABLED */
#define is_gnu_builtin_function(rp) FALSE
/*lint -emacro(506,rout_is_specific_builtin)*/
#define rout_is_specific_builtin(rp, bfk) FALSE
/*lint -emacro(506,is_call_to_builtin_function)*/
#define is_call_to_builtin_function(rp, bfk) FALSE
#endif /* BUILTIN_FUNCTIONS_ENABLED */

inline a_boolean is_any_reattempt_permitted(
                                  a_const_eval_reattempt_state reattempt_state)
/*
If the given reattempt state indicates that there's one or more conditions
where a reattempt should be made, return TRUE; otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  if (reattempt_state.default_arg) {
    result = TRUE;
  } else if (reattempt_state.default_mem_init) {
    result = TRUE;
  }  /* if */
  return result;
}  /* is_any_reattempt_permitted */


inline void merge_reattempt_state(a_const_eval_reattempt_state *dest_state,
                                  a_const_eval_reattempt_state new_state)
/*
Merge the new reattempt state into the destination state.
*/
{
  if (new_state.default_arg) {
    dest_state->default_arg = TRUE;
  }  /* if */
  if (new_state.default_mem_init) {
    dest_state->default_mem_init = TRUE;
  }  /* if */
}  /* merge_reattempt_state */

/*
Macro to test an operator routine entry's opname_kind field.
*/
#define opname_kind_is(rp, onk)                                             \
  ((rp)->variant.opname_kind == (onk))

/*
Macros to test for relational operators.
*/
#define opname_is_rel_op(opkind)                                            \
  ((opkind) == onk_lt || (opkind) == onk_le ||                              \
   (opkind) == onk_gt || (opkind) == onk_ge)

#define opname_kind_is_rel_op(rp)                                           \
  (opname_is_rel_op((rp)->variant.opname_kind))

/*
Macro to test for equality operators.
*/
#define opname_is_eq_op(opkind)                                             \
  ((opkind) == onk_eq || (opkind) == onk_ne)

/*
Macro to test for comparison operators.
*/
#define opname_is_comparison(opkind)                                        \
  (opname_is_eq_op(opkind) ||                                               \
   opname_is_rel_op(opkind) ||                                              \
   (opkind) == onk_spaceship)

/*
Macro to test for call-like operators: onk_function_call and, in C++23,
onk_subscript.
*/
#define opname_is_call_like(opkind)                                          \
  ((opkind) == onk_function_call ||                                          \
   (multi_subscript_enabled && (opkind) == onk_subscript))

/*
Macro that returns TRUE if a routine has been defined.  The value is
TRUE from the beginning of scanning of the function body (not just
after the closing brace), and is also TRUE for functions with
compiler-generated bodies.  The value remains TRUE if the body of
the function is discarded, as for example with trivial default
constructors.  The test of routine_fixup is used so that a
function defined in a friend declaration in a template will be
considered defined even if the definition has not been fixed-up yet.
*/
#define routine_has_been_defined(rout) \
  ((rout)->defined || \
   (rout)->function_def_number != NULL_function_def_number ||	\
   (rout)->routine_fixup != NULL)


/* Macro to fetch the value of the needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define needed_flag_is_set(scp) \
  (needed_flag_bit_number == 0 ? (scp)->needed : \
                                 instantiation_needed_flag_is_set(scp,0))
extern a_boolean instantiation_needed_flag_is_set(
                                           a_source_correspondence *scp,
                                           int                     bit_offset);
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define needed_flag_is_set(scp) ((scp)->needed)
#endif /* ONE_INSTANTIATION_PER_OBJECT */

/*
Produce TRUE if a given routine is a real template instance (not an explicit
specialization nor a prototype instantiation).
*/
#define rout_is_real_template_instance(rp)                              \
  (((rp)->is_template_function &&                                       \
    !(rp)->is_specialized &&                                            \
    !(rp)->is_prototype_instantiation) ||                               \
   (rp)->friend_defined_in_instantiation)

/*
Produce TRUE if a given routine is an instance (a template function that is not
an explicit specialization).
*/
#define rout_is_template_instance(rp)                                   \
  ((rp)->is_template_function && !(rp)->is_specialized)


/*
Produce TRUE if the given member function is a template instance.  This macro
also works for generated members.
*/
#define is_unspecialized_template_member_function(rp)                        \
  ((rp)->compiler_generated ?                                                \
                   (is_unspecialized_template_class(parent_class_of(rp))) :  \
                   ((rp)->is_template_function && !(rp)->is_specialized))


/* Test a routine to see whether it is inline.  When it is a template
   instance, this may require looking at the template because the
   is_inline flag is not recorded until the function is fully instantiated. */
#if STANDALONE_UTILITY_PROGRAM
#define rout_is_inline(rout)						\
  ((rout)->is_inline)
#else /* !STANDALONE_UTILITY_PROGRAM */
extern a_boolean intf_rout_is_inline_template_function(a_routine_ptr rout);
#define rout_is_inline(rout)						\
  ((rout)->is_inline ||							\
   ((rout)->is_template_function &&					\
    intf_rout_is_inline_template_function(rout)))
#endif /* STANDALONE_UTILITY_PROGRAM */

/* Helper macro for macros treat_as_static_inline and treat_as_extern_inline
   (see below). */
#if LOWER_EXTERN_INLINE && !IA64_ABI
#if MICROSOFT_EXTENSIONS_ALLOWED
#define and_not_dllexport(rout)  && !((rout)->decl_modifiers & DM_DLLEXPORT)
#else /* MICROSOFT_EXTENSIONS_ALLOWED */
#define and_not_dllexport(rout)  /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* LOWER_EXTERN_INLINE && !IA64_ABI */

/* Macro to determine whether a routine is to be treated as a static inline
   function.  This includes "extern inline" functions that are lowered to
   static functions. */
/* In IA-64 mode, the lowered functions are still external and they
   go out in COMDAT sections. */
/* Note that these macros work only in C++ mode. */
#if LOWER_EXTERN_INLINE && !IA64_ABI
/* When lowering "extern inline" all inline functions are treated as static.
   Those that really are static stay static (actually, they may get
   externalized if there are exported templates, or because of
   one-instantiation-per-object mode, then lowered to static again), and
   extern inline functions get lowered to static.  An exception
   is made for function definitions marked with dllexport: They must be
   spilled with extern linkage. */
#define treat_as_static_inline(rout)                                    \
  (rout_is_inline(rout)                                                 \
   and_not_dllexport(rout))
#else /* !(LOWER_EXTERN_INLINE && !IA64_ABI) */
/* When not lowering "extern inline" only those declared static are treated
   as static. */
#define treat_as_static_inline(rout)					\
  (rout_is_inline(rout) &&						\
   (!extern_inline_allowed ||						\
    ((rout)->storage_class == (a_storage_class)sc_static)))
#endif /* LOWER_EXTERN_INLINE && !IA64_ABI */

/*
Return TRUE if the routine should be treated as an extern inline function.
*/
#if LOWER_EXTERN_INLINE && !IA64_ABI
#define treat_as_extern_inline(rout)                                    \
  ((rout)->is_inline &&                                                 \
   (rout)->storage_class == (a_storage_class)sc_unspecified             \
   and_not_dllexport(rout))
#else /* !LOWER_EXTERN_INLINE && !IA64_ABI */
#define treat_as_extern_inline(rout)                                    \
  ((rout)->is_inline &&                                                 \
   (rout)->storage_class == (a_storage_class)sc_unspecified)
#endif /* LOWER_EXTERN_INLINE && !IA64_ABI */

#if !STANDALONE_UTILITY_PROGRAM

/* Macro to set the needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define set_needed_flag(scp) \
  (needed_flag_bit_number == 0 ? ((scp)->needed = TRUE) : \
                                 (set_instantiation_needed_flag(scp,0,1), 0))
extern void set_instantiation_needed_flag(a_source_correspondence *scp,
                                          int                     bit_offset,
                                          int                     new_value);
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define set_needed_flag(scp) ((scp)->needed = TRUE)
#endif /* ONE_INSTANTIATION_PER_OBJECT */

/* Macro to reset the needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define reset_needed_flag(scp) \
    ((scp)->needed = FALSE, \
     (scp)->per_instantiation_needed_flags = NULL)
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define reset_needed_flag(scp) ((scp)->needed = FALSE)
#endif /* ONE_INSTANTIATION_PER_OBJECT */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Macro to fetch the value of the class definition_needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define class_definition_needed_flag_is_set(tp) \
  (needed_flag_bit_number == 0 ? \
                   (tp)->variant.class_struct_union.definition_needed : \
                   instantiation_needed_flag_is_set(&(tp)->source_corresp,1))
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define class_definition_needed_flag_is_set(tp) \
                  ((tp)->variant.class_struct_union.definition_needed)
#endif /* ONE_INSTANTIATION_PER_OBJECT */

#if !STANDALONE_UTILITY_PROGRAM
/* Macro to set the class definition_needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define set_class_definition_needed_flag(tp) \
  (needed_flag_bit_number == 0 ? \
                ((tp)->variant.class_struct_union.definition_needed = TRUE) : \
                (set_instantiation_needed_flag(&(tp)->source_corresp,1,1), 0))
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define set_class_definition_needed_flag(tp) \
                ((tp)->variant.class_struct_union.definition_needed = TRUE)
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Macro to fetch the value of the routine definition_needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define routine_definition_needed_flag_is_set(rp) \
  (needed_flag_bit_number == 0 ? \
                   (rp)->definition_needed : \
                   instantiation_needed_flag_is_set(&(rp)->source_corresp,1))
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define routine_definition_needed_flag_is_set(rp) \
                  ((rp)->definition_needed)
#endif /* ONE_INSTANTIATION_PER_OBJECT */

#if !STANDALONE_UTILITY_PROGRAM
/* Macro to set the routine definition_needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define set_routine_definition_needed_flag(rp) \
  (needed_flag_bit_number == 0 ? \
                ((rp)->definition_needed = TRUE) : \
                (set_instantiation_needed_flag(&(rp)->source_corresp,1,1), 0))
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define set_routine_definition_needed_flag(rp) \
                ((rp)->definition_needed = TRUE)
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#endif /* !STANDALONE_UTILITY_PROGRAM */


/*
Macro that casts a pointer to an unsigned long.  Such a conversion may be
lossy in some configurations, and in most cases where this macro is used
(e.g., when creating hash values), that may be okay.
*/
#define possible_lossy_cast_from_pointer(x) ((unsigned long)(size_t)(x))

/*
Macro that casts a pointer to an uintptr_t.  Such a conversion should not be
lossy in any configuration (but the underlying data type may change from
one configuration to another).
*/
#define cast_from_pointer(x) ((uintptr_t)(size_t)(x))

/*
Macro that generates a unique uintptr_t identifier from an IL pointer.
This is useful for generating names for unnamed symbols, for cross-reference
information, and for debug prints.  On most machines, the unique identifier
can simply be the pointer converted to unsigned long.  If that won't work,
a function can be substituted that does something else.
*/
#define unique_id_for_il_pointer(ptr) (cast_from_pointer((ptr)))


extern void set_inline_flag(a_routine_ptr  rp,
                            a_boolean      flag);

/*
A collection of source positions passed around during declaration processing.
*/
typedef struct a_decl_pos_block *a_decl_pos_block_ptr;
typedef struct a_decl_pos_block {
  a_source_position
		decl_pos;
			/* Source position of the identifier. */
  a_source_position
		storage_class_pos;
			/* Source position of storage-class, if any. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		identifier_range;
			/* Start and end positions of coalesced identifier. */
  a_source_range
		specifiers_range;
			/* Start and end positions of decl-specifiers. */
  a_source_range
		declarator_range;
			/* Start and end positions of declarator. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_source_range
		var_init_range;
			/* Start and end positions of initializer.  The end
			   position is only recorded when
			   EXTRA_SOURCE_POSITIONS_IN_IL is TRUE. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  an_element_position_ptr
		extra_positions;
			/* A list of positions for various elements of a
			   declaration that aren't recorded directly in the
			   corresponding IL entry. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} a_decl_pos_block;

#if NULL_POINTER_IS_ZERO
#define clear_decl_pos_block(dpbp)                                           \
  (memzero((char*)(dpbp), sizeof(a_decl_pos_block)))
#else /* !NULL_POINTER_IS_ZERO */
extern void clear_decl_pos_block(a_decl_pos_block_ptr  decl_pos_block);
#endif /* NULL_POINTER_IS_ZERO */

extern void f_add_element_position(an_element_position_kind  kind,
                                   a_source_position         *pos,
                                   an_element_position_ptr   *p_epp);

#define add_element_position(kind, pos, p_epp)                             \
  (f_add_element_position((an_element_position_kind)(kind), (pos), (p_epp)))

#if EXTRA_SOURCE_POSITIONS_IN_IL

extern a_decl_position_supplement_ptr make_decl_pos_supplement(
                                        a_boolean             at_file_scope,
                                        a_decl_pos_block_ptr  decl_pos_block);

extern void update_decl_pos_info(a_source_correspondence  *scp,
                                 a_decl_pos_block_ptr     decl_pos_block);

extern void prepend_element_positions(an_element_position_ptr  new_epp,
                                      an_element_position_ptr  *p_epp);

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

extern int compare_source_positions(const a_source_position  *pos1,
                                    const a_source_position  *pos2);

/*
Dynamically-allocated and expandable buffer used for short-lived text.
"short-lived" means text that is needed during a bit of processing during
which no parsing or lexical advance is done (no get_token calls, no
macro expansions, etc.).
*/
EXTERN_THREAD char
		*temp_text_buffer;
			/* The buffer itself.  Not allocated on a per-file
			   basis. */
EXTERN_THREAD sizeof_t
		size_temp_text_buffer;
			/* The size of temp_text_buffer, as currently
			   allocated. */
EXTERN_THREAD sizeof_t
		pos_in_temp_text_buffer;
			/* The number of characters actually in
			   temp_text_buffer currently. */
EXTERN_THREAD a_boolean
		temp_text_buffer_managed;
			/* TRUE if temp_text_buffer is managed by an instance
			   of a_temp_text_buffer_swap; otherwise, FALSE. */
/* See il.c for TEMP_TEXT_BUFFER_INCREMENTAL_ALLOCATION. */

/*
This structure is used to create a new temporary text buffer.  The previous
temporary text buffer is restored upon destruction.
*/
struct a_temp_text_buffer_swap {
  inline a_temp_text_buffer_swap();
  inline ~a_temp_text_buffer_swap();
private:
  char          *prev_temp_text_buffer;
                        /* The previous temp_text_buffer value. */
  sizeof_t      prev_size_temp_text_buffer;
                        /* The previous size_temp_text_buffer value. */
  sizeof_t      prev_pos_in_temp_text_buffer;
                        /* The previous temp_text_buffer value. */
  a_boolean     prev_temp_text_buffer_managed;
                        /* The previous temp_text_buffer_managed value. */
};  /* a_temp_text_buffer_swap */


a_temp_text_buffer_swap::a_temp_text_buffer_swap()
/*
Begin a new temp text buffer state independent of any current state.
*/
  : prev_temp_text_buffer(temp_text_buffer),
    prev_size_temp_text_buffer(size_temp_text_buffer),
    prev_pos_in_temp_text_buffer(pos_in_temp_text_buffer),
    prev_temp_text_buffer_managed(temp_text_buffer_managed)
{
  temp_text_buffer = NULL;
  size_temp_text_buffer = 0;
  pos_in_temp_text_buffer = 0;
  temp_text_buffer_managed = TRUE;
}  /* a_temp_text_buffer_swap::a_temp_text_buffer_swap */


a_temp_text_buffer_swap::~a_temp_text_buffer_swap()
/*
End the current temp text buffer state restoring the previous state.
*/
{
  if (temp_text_buffer != NULL) {
    /* If this assertion fails, either temp_text_buffer or
       size_temp_text_buffer were modified and are no longer consistent with
       each other.  The code that caused this inconsistency should be
       corrected. */
    check_assertion(size_temp_text_buffer > 0);
    free_fe(temp_text_buffer, size_temp_text_buffer);
  }  /* if */
  temp_text_buffer_managed = this->prev_temp_text_buffer_managed;
  pos_in_temp_text_buffer = this->prev_pos_in_temp_text_buffer;
  size_temp_text_buffer = this->prev_size_temp_text_buffer;
  temp_text_buffer = this->prev_temp_text_buffer;
}  /* a_temp_text_buffer_swap::~a_temp_text_buffer_swap */


extern void expand_temp_text_buffer(sizeof_t size_needed);

/*
Ensure that temp_text_buffer has at least size_needed bytes in it.  If not,
expand temp_text_buffer by reallocating it.
*/
#define ensure_temp_text_buffer_space(size_needed)                     \
{ if (size_temp_text_buffer < (size_needed)) {                         \
    expand_temp_text_buffer((sizeof_t)(size_needed));                  \
  }  /* if */                                                          \
}  /* ensure_temp_text_buffer_space */

extern void put_str_to_temp_text_buffer(a_const_char *str);

extern void put_uint_to_temp_text_buffer(unsigned long long value);

extern void put_str_to_temp_text_buffer_octl(
                               a_const_char                          *str,
                               an_il_to_str_output_control_block_ptr octl);

extern
void put_str_into_text_buffer(a_const_char                          *str,
                              an_il_to_str_output_control_block_ptr octl);

extern void put_ch_to_temp_text_buffer(char ch);

extern void set_error_constant(a_constant *cp);

extern a_constant_ptr alloc_error_constant(void);

extern a_dynamic_init_ptr make_error_constant_dynamic_init(void);

extern void set_routine_address_constant(a_routine_ptr routine,
                                         a_constant    *con,
                                         a_boolean     set_address_taken_flag);

extern void set_variable_address_taken(a_variable_ptr variable);

extern void set_variable_address_constant(
                                        a_variable_ptr variable,
                                        a_constant     *con,
                                        a_boolean      set_address_taken_flag);
#if DO_IL_LOWERING
extern void set_variable_address_constant_preserving_implicit_cast(
                                        a_variable_ptr variable,
                                        a_constant     *con,
                                        a_boolean      set_address_taken_flag);
#endif /* DO_IL_LOWERING */

extern void set_constant_address_constant(a_constant_ptr constant,
                                          a_constant     *con);

extern void set_temporary_address_constant(a_constant_ptr constant,
                                           a_constant    *con);

extern a_boolean is_template_param_cast_constant(a_constant_ptr  con,
                                                 a_constant_ptr  *p_base_con,
                                                 a_boolean       *is_explicit);

#if GNU_EXTENSIONS_ALLOWED
extern void set_label_address_constant(a_label_ptr label,
                                       a_constant  *con);

/*
Macro to determine whether the argument constant is a label address.
*/
#define constant_is_address_of_label(cp)                                     \
  ((cp)->kind == (a_constant_repr_kind)ck_address &&                         \
   (cp)->variant.address.kind == (an_address_base_kind)abk_label)

/*
Macro to determine whether the argument variable was mapped on a specific
register using the GNU "asm(...)" extension.
*/
/*lint -emacro(506,var_is_gnu_named_register)*/
#define var_is_gnu_named_register(var)                                     \
  (!var_has_named_register_storage_class((var)) &&                         \
   !(var)->asm_name_is_valid)
#endif /* GNU_EXTENSIONS_ALLOWED */

extern void set_ptr_to_member_function_constant(a_routine_ptr routine,
                                                a_constant    *con);

extern void set_ptr_to_data_member_constant(a_field_ptr field,
                                            a_constant  *con);

extern void set_arg_transfer_method_flag(a_param_type_ptr   ptp,
                                         a_source_position  *err_pos);

extern a_param_type_ptr make_param_type(a_type_ptr         tp,
                                        a_source_position  *decl_pos);
extern void update_param_top_level_qualifiers(a_param_type_ptr ptp);
extern a_type_ptr make_routine_type(a_type_ptr        return_type,
                                    a_type_ptr        param1_type = NULL,
                                    a_type_ptr        param2_type = NULL,
                                    a_type_ptr        param3_type = NULL,
                                    a_type_ptr        param4_type = NULL,
                                    a_type_ptr        param5_type = NULL,
                                    a_type_ptr        param6_type = NULL,
                                    a_type_ptr        param7_type = NULL);

extern a_type_ptr add_param_type(a_type_ptr  rout_type,
                                 a_type_ptr  param_type);

#if GNU_VECTOR_TYPES_ALLOWED
extern a_type_ptr make_vector_type(a_type_ptr     element_type,
                                   a_targ_size_t  n_elements,
                                   a_vector_kind  kind = vk_gnu);
extern a_type_ptr make_scalable_vector_type(a_type_ptr  element_type,
                                            uint8_t     n_tuple_elements);
extern a_type_ptr scalable_vector_count_type(void);
extern a_type_ptr make_riscv_vector_type(a_type_ptr  element_type,
                                         int8_t      length_multiplier,
                                         uint8_t     tuple_elements);
extern a_type_ptr modal_8bit_floating_point_type(void);
extern a_type_ptr float8e4m3_type(void);
extern a_type_ptr float8e5m2_type(void);

extern void eliminate_boolean_vector(a_type_ptr  *p_type);
#endif /* GNU_VECTOR_TYPES_ALLOWED */

#if NAMED_REGISTERS_ALLOWED
extern void record_named_register_storage_class(
                                             a_variable_ptr       var,
                                             a_named_register_id  register_id,
                                             a_boolean            is_redecl,
                                             a_source_position    *pos);

/*
Macro to determine if the given variable was declared with a named-register
storage class (an Embedded C extension).  The macro can be used in
configurations that don't allow named-register storage classes (in that case
the macro expands to FALSE).
*/
#define var_has_named_register_storage_class(var)                            \
  ((var)->has_named_register_storage_class)
#else /* !NAMED_REGISTERS_ALLOWED */
/*lint -emacro(506,var_has_named_register_storage_class)*/
#define var_has_named_register_storage_class(var)  FALSE
#endif /* NAMED_REGISTERS_ALLOWED */

extern a_boolean may_be_added_to_types_list(a_type_ptr     type_ptr,
                                            a_scope_depth  decl_level);

extern void set_parent_scope_for_type(a_type_ptr     type_ptr,
                                      a_scope_depth  scope_level);

extern void add_lambda_closure_to_types_list(a_type_ptr     type_ptr,
                                             a_scope_depth  scope_level);

extern void add_to_types_list(a_type_ptr     type_ptr,
                              a_scope_depth  scope_level);

extern void move_to_end_of_types_list(a_type_ptr     type_ptr,
                                      a_scope_depth  scope_level);

extern void do_based_type_fixup(void);

extern void record_fundamental_types_copied_from_secondary_IL(void);

extern a_const_char* get_type_name(a_type_ptr tp);

extern a_type_ptr integer_type(an_integer_kind kind);

extern a_type_ptr bit_precise_integer_type(a_targ_size_t bit_width,
                                           a_boolean     is_unsigned,
                                           a_boolean     explicitly_signed);
extern a_type_ptr dependent_bit_precise_integer_type(
                                           a_constant_ptr bit_width_constant,
                                           a_boolean      is_unsigned);

extern a_type_ptr signed_integer_type(an_integer_kind kind);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_type_ptr microsoft_sized_integer_type(an_integer_kind kind);

extern a_type_ptr microsoft_sized_signed_integer_type(an_integer_kind kind);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_type_ptr other_signedness_integer_type(an_integer_kind ikind);

extern a_type_ptr wchar_t_type(void);

extern a_type_ptr char8_t_type(void);

extern a_type_ptr char16_t_type(void);

extern a_type_ptr char32_t_type(void);

extern a_type_ptr eff_wchar_t_type(void);

extern a_type_ptr eff_char8_t_type(void);

extern a_type_ptr eff_char16_t_type(void);

extern a_type_ptr eff_char32_t_type(void);

extern a_type_ptr bool_type(void);

#if FIXED_POINT_ALLOWED
extern a_boolean fixed_point_type_used_in_primary_IL(
                                 a_fixed_point_type_descr descr);
extern a_type_ptr fixed_point_type(a_fixed_point_type_descr descr);
extern a_fixed_point_type_descr make_fixed_point_type_descr(
                                 a_fixed_point_precision  precision,
                                 a_boolean                is_unsigned,
                                 a_boolean                is_fract,
                                 a_boolean                saturating);
#endif /* FIXED_POINT_ALLOWED */

extern a_type_ptr float_type(a_float_kind kind);

#if C99_IL_EXTENSIONS_SUPPORTED
#if DO_IL_LOWERING
extern a_boolean complex_type_used_in_primary_IL(a_float_kind kind);
#endif /* DO_IL_LOWERING */

extern a_type_ptr complex_type(a_float_kind kind);

#if LOWER_COMPLEX
extern a_boolean imaginary_type_used_in_primary_IL(a_float_kind kind);
#endif /* LOWER_COMPLEX */

extern a_type_ptr imaginary_type(a_float_kind kind);

extern void set_complex_constant(a_float_kind float_kind,
                                 a_const_char *real,
                                 a_const_char *imag,
                                 a_constant   *con);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

extern a_type_ptr string_literal_type(a_character_kind  kind,
                                      a_targ_size_t     num_chars);

extern a_type_ptr string_type(a_targ_size_t  num_chars);

#define wide_string_type(num_chars)                                        \
  string_literal_type((a_character_kind)chk_wchar_t, (num_chars))

extern a_type_ptr error_type(void);

extern a_type_ptr unknown_type(void);

extern a_type_ptr void_type(void);

extern a_type_ptr reflection_type(void);

extern a_type_ptr managed_nullptr_type(void);

extern a_type_ptr standard_nullptr_type(void);

extern a_type_ptr strong_ordering_type(void);

extern a_type_ptr weak_ordering_type(void);

extern a_type_ptr partial_ordering_type(void);

extern a_type_ptr strong_equality_type(void);

extern a_type_ptr weak_equality_type(void);

extern a_boolean check_consistent_string_view_type(a_type_ptr  svtp);

/*
A collection of information required to compatibly support the interpretation
of the GNU libstdc++ standard library's std::source_location::__impl type.
*/
struct a_gnu_source_location_type_info {
  a_type_ptr    impl_type;
                        /* The type representing the GNU libstdc++ standard
                           library's "__impl" type. */
  a_field_ptr   file_field;
                        /* The impl_type's field representing the file name
                           (NULL if impl_type is an error type). */
  a_field_ptr   function_field;
                        /* The impl_type's field representing the function name
                           (NULL if impl_type is an error type). */
  a_field_ptr   line_field;
                        /* The impl_type's field representing the line number
                           (NULL if impl_type is an error type). */
  a_field_ptr   column_field;
                        /* The impl_type's field representing the column number
                           (NULL if impl_type is an error type). */
};  /* a_gnu_source_location_type_info */

extern a_type_ptr gnu_source_location_impl_type(void);

extern a_boolean has_gnu_source_location_impl_type(void);

extern a_gnu_source_location_type_info gnu_source_location_impl(void);

EXTERN_THREAD a_constant_ptr
		strong_ordering_equal,
		strong_ordering_less,
		strong_ordering_greater,
		weak_ordering_equivalent,
		weak_ordering_less,
		weak_ordering_greater,
		partial_ordering_equivalent,
		partial_ordering_less,
		partial_ordering_greater,
		partial_ordering_unordered,
		strong_equality_equal,
		strong_equality_nonequal,
		weak_equality_equivalent,
		weak_equality_nonequivalent;
			/* The constants that initialize the constexpr static
			   members std::strong_ordering::equal, etc. */

EXTERN void initialize_ordering_constants(void);


extern void update_ptr_to_member_type(a_type_ptr  ptr_mem_type,
                                      a_type_ptr  member_type);

extern a_type_ptr make_partial_ptr_to_member_type(a_type_ptr  class_type,
                                                  a_type_ptr  orig_class_type);

extern a_type_ptr ptr_to_member_type_full(
                                       a_type_ptr              member_type,
                                       a_type_ptr              class_type,
                                       a_type_ptr              orig_class_type,
                                       a_pointer_modifier_set  modifiers);

#define ptr_to_member_type(tp, cp, orig_cp)                                  \
  (EDG_QUAL ptr_to_member_type_full((tp), (cp), (orig_cp), PM_NONE))

extern a_type_ptr related_ptr_to_member_type(a_type_ptr member_type,
                                             a_type_ptr class_type);

extern a_type_ptr function_type_without_noexcept_exception_spec(
                                                              a_type_ptr type);

extern a_type_ptr make_pointer_type_full(
                                      a_type_ptr              pointed_to_type,
                                      a_pointer_modifier_set  modifiers);

#define make_pointer_type(tp)                                                \
  (EDG_QUAL make_pointer_type_full((tp), PM_NONE))

extern a_type_ptr make_reference_type(a_type_ptr type_pointed_to);

extern a_type_ptr make_rvalue_reference_type(a_type_ptr  pointed_to_type);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_type_ptr make_handle_type(a_type_ptr type_pointed_to);

extern a_type_ptr make_handle_to_system_string(void);

extern a_type_ptr make_tracking_reference_type(a_type_ptr type_pointed_to);

extern a_type_ptr make_interior_ptr_type(a_type_ptr pointed_to_type);

extern a_type_ptr make_pin_ptr_type(a_type_ptr pointed_to_type);

extern a_type_ptr make_cppcx_box_type(a_type_ptr boxed_type);

extern a_routine_ptr get_idisposable_dispose_routine(void);

extern a_routine_ptr get_object_finalize_routine(void);

extern void f_set_clrcall_convention_if_needed(a_type_ptr  rtp);

#define set_clrcall_convention_if_needed(rtp)                                \
  if (cppcli_enabled) {                                                      \
    f_set_clrcall_convention_if_needed(rtp);                                 \
  }  /* if */

#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
extern a_boolean f_is_member_of_namespace_cli(a_source_correspondence  *scp);

#define is_member_of_namespace_cli(ptr)                                      \
  (f_is_member_of_namespace_cli((a_source_correspondence*)ptr))
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define set_clrcall_convention_if_needed(rtp)  /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern
a_type_ptr make_pointer_type_of_same_kind(a_type_ptr base_type,
                                          a_type_ptr model_pointer_type);

a_type_ptr make_reference_type_of_same_kind(a_type_ptr base_type,
                                            a_type_ptr model_ref_type);

extern a_type_ptr make_reference_to_reference(
                                          a_type_ptr            base_ref_type,
                                          a_boolean             rvalue_ref,
                                          a_boolean             tracking_ref,
                                          a_type_qualifier_set  qualifiers,
                                          a_source_position     *qual_pos,
                                          a_boolean             *is_error);

extern a_type_ptr f_make_qualified_type(a_type_ptr            old_type,
                                        a_type_qualifier_set  qualifier,
                                        a_upc_block_size      upc_block_size);

#define make_qualified_type(old_type, qualifier)    \
  EDG_QUAL f_make_qualified_type(old_type, qualifier, UPC_BLOCK_SIZE_NONE)

/*
Make a version of type that has the same qualifiers as model_type, and return
a pointer to it.  The original qualifiers on type, if any, are ignored.
Note that type and model_type need not be the same (or even similar) types
under the qualifiers.
*/
#define make_identically_qualified_type(type, model_type)             \
  (make_qualified_type(skip_typerefs(type), get_type_qualifiers(model_type)))


extern a_type_ptr type_plus_qualifiers_from_second_type(a_type_ptr type,
                                                        a_type_ptr model_type);

extern a_type_ptr make_unqualified_type(
                              a_type_ptr old_type,
                              a_boolean  unqualify_array_elements = !C_mode());

extern a_type_ptr remove_qualifiers(a_type_ptr           type,
                                    a_type_qualifier_set qualifiers_to_remove);

extern a_type_ptr remove_cvref(a_type_ptr  tp);

extern a_type_ptr prvalue_type(a_type_ptr type);

extern a_type_ptr return_type_of(a_type_ptr routine_type);

extern a_type_ptr il_return_type_of(a_type_ptr routine_type);

extern a_vla_dimension_ptr find_vla_dimension_in_current_function(
                                                       a_type_ptr  array_type);

extern a_vla_dimension_ptr find_vla_dimension(a_type_ptr array_type);

extern void make_local_expr_node_ref(
                                 an_expr_node_ptr            expr,
                                 a_local_expr_node_ref_kind  kind,
                                 char                        *referrer,
                                 a_scope_ptr                 func_scope);

extern an_expr_node_ptr find_local_expr_node_in_scope(
                                     char                        *referrer,
                                     a_local_expr_node_ref_kind  kind,
                                     a_scope_ptr                 target_scope);

extern an_expr_node_ptr find_local_expr_node(char  *referrer,
                                             a_local_expr_node_ref_kind  kind);

extern an_expr_node_ptr expr_node_from_tpck_expression(const a_constant *cp);

extern an_expr_node_ptr expr_node_from_constant(a_constant_ptr cp);

extern an_expr_node_ptr expr_node_from_attribute_arg(an_attribute_arg_ptr aap);

extern void make_local_scope_ref(a_scope_ptr            scope,
                                 char                   *referrer,
                                 an_il_entry_kind       referrer_kind,
                                 a_scope_ptr            func_scope);

extern a_scope_ptr find_local_scope(char  *referrer);

#if PROTOTYPE_INSTANTIATIONS_IN_IL
extern an_expr_node_ptr generic_sizeof_arg_expr(a_constant_ptr  con);
#else /* !PROTOTYPE_INSTANTIATIONS_IN_IL */
#define generic_sizeof_arg_expr(con)                                        \
  ((con)->variant.template_param.variant.templ_sizeof.expr)
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */

extern a_type_ptr make_field_selection_type(a_field_ptr           field,
                                            a_type_qualifier_set  qualifiers);

extern a_type_ptr make_pm_selection_type(a_type_ptr operand_1_type,
                                         a_type_ptr operand_2_type);

extern void skip_common_type_qualifiers(a_type_ptr  *type1,
                                        a_type_ptr  *type2);

/*
Macro that is TRUE if the given operator kind is a simple (i.e., not
compound) assignment.
*/
#define is_simple_assignment(op)                                            \
  ((op) == (an_expr_operator_kind)eok_assign)

/*
Macro that is TRUE for a dynamic initialization that initializes a
variable-length array (VLA).
*/
#define is_dynamic_init_for_vla(dip) \
  ((dip)->variable != NULL && (dip)->variable->is_vla)

extern a_boolean node_is_pointer_with_restrict_semantics(
                                                        an_expr_node_ptr node);

#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean is_rvalueable_node(an_expr_node_ptr node);

extern a_boolean node_includes_glvalue_to_prvalue_conv(an_expr_node_ptr node);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_boolean dynamic_init_has_side_effects(
                                        a_dynamic_init_ptr dip,
                                        a_boolean          for_unused_var,
                                        a_boolean          *suppress_warning);

extern a_boolean expr_list_has_side_effects(
                                           an_expr_node_ptr expr_list,
                                           a_boolean        *suppress_warning);

extern a_boolean node_has_side_effects(an_expr_node_ptr node,
                                       a_boolean        *suppress_warning);

extern a_boolean is_invariant_expr(
                                  an_expr_node_ptr expr,
                                  a_boolean        vars_can_change,
                                  a_boolean        treat_as_potential_prvalue);

extern a_boolean expr_has_reference_to_local_entity(an_expr_node_ptr expr);

extern a_boolean expr_has_reused_value_init(an_expr_node_ptr expr);

extern a_boolean mixed_regions_in_expr_tree(an_expr_node_ptr expr);

extern a_boolean expr_has_reference_to_routine_scope_variable(
                                                        an_expr_node_ptr expr);

extern a_variable_ptr get_routine_scope_variable_found(void);

extern a_boolean expr_might_throw(an_expr_node_ptr expr);

extern a_boolean dynamic_init_might_throw(a_dynamic_init_ptr expr);

extern a_boolean expr_calls_nontrivial_function(an_expr_node_ptr expr);

extern a_boolean expr_calls_nontrivial_ctor(an_expr_node_ptr expr);

extern a_boolean has_statement_expression(an_expr_node_ptr expr);

#if !STANDALONE_UTILITY_PROGRAM
extern void eliminate_statement_expr_pragmas(an_expr_node_ptr  expr);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern void eliminate_statement_expr_src_seq_entries(an_expr_node_ptr  expr);

extern a_boolean expr_is_dep_static_member_of_current_instantiation(
                                                        an_expr_node_ptr expr);

extern a_boolean expr_is_instantiation_dependent(an_expr_node_ptr expr);

extern a_boolean constant_is_instantiation_dependent(a_constant_ptr con);

extern a_boolean expr_contains_error(an_expr_node_ptr expr);

extern a_boolean constant_addresses_local_var(a_constant_ptr  con);

extern a_boolean constant_contains_error(a_constant_ptr con);

extern a_boolean type_returned_by_cctor(a_type_ptr  return_type,
                                        a_boolean   *p_incomplete);

extern void set_routine_calling_method_flag(a_type_ptr         routine_type,
                                            a_source_position  *err_pos);

extern a_boolean routine_has_default_args(a_routine_ptr  rp);

extern a_boolean has_uninstantiated_default_arg(a_type_ptr  rtp);

extern void copy_type_full(a_type_ptr from,
                           a_type_ptr to,
                           a_boolean  copy_default_args);

extern void copy_type(a_type_ptr from,
                      a_type_ptr to);

extern a_type_ptr type_without_deduced_auto_placeholder(a_type_ptr  type);

extern a_type_ptr copy_array_type_replacing_element_type(
                                                     a_type_ptr  old_array,
                                                     a_type_ptr  element_type);

extern a_type_ptr copy_routine_type_with_param_types(
                                               a_type_ptr  from_type,
                                               a_boolean   copy_default_args);

extern void copy_routine_type_default_args(a_type_ptr  from_type,
                                           a_type_ptr  to_type);

extern a_type_ptr routine_type_without_default_args(a_type_ptr orig_type);

extern
a_type_ptr routine_type_without_this_class(a_type_ptr  orig_type,
                                           a_boolean   copy_default_args);

extern
void ensure_underlying_function_type_is_modifiable(a_type_ptr  *p_type,
                                                   a_type_ptr  *func_type);

extern
a_type_ptr routine_type_without_param_type_qualifiers(a_type_ptr  orig_type);


#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
extern a_boolean class_type_can_be_named_in_namespace_scope(a_type_ptr  type);
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern void skip_start_of_pack_placeholders_simple(a_template_arg_ptr *p_tap);

#define begin_template_arg_list_traversal_simple(arg_list, p_tap)            \
  (((*p_tap) = arg_list),                                                    \
   (arg_list != NULL &&                                                      \
    is_start_of_pack_expansion_templ_arg(arg_list)) ?                        \
        skip_start_of_pack_placeholders_simple(p_tap) : (void)0)

#define advance_to_next_template_arg_simple(p_tap)                           \
  (((*p_tap) = (*p_tap)->next),                                              \
   (*p_tap != NULL &&                                                        \
    is_start_of_pack_expansion_templ_arg(*p_tap)) ?                          \
        skip_start_of_pack_placeholders_simple(p_tap) : (void)0)

extern a_template_arg_ptr copy_template_arg_list(a_template_arg_ptr orig_list);

extern a_template_arg_ptr copy_template_type_arg_list_with_deduplication(
                                                 a_template_arg_ptr orig_list);

extern a_boolean is_default_constructor(a_routine_ptr  rout,
                                        a_boolean      is_declarative_context);

extern a_boolean is_copy_constructor_type(
                                 a_type_ptr            routine_type,
                                 a_type_ptr            class_of_which_a_member,
                                 a_type_qualifier_set  *qualifiers,
                                 a_boolean             include_move_ctors,
                                 a_boolean             is_declarative_context);

extern a_boolean is_copy_constructor(
                                 a_routine_ptr         ctor_rout,
                                 a_type_ptr            class_of_which_a_member,
                                 a_type_qualifier_set  *qualifiers,
                                 a_boolean             include_move_ctors,
                                 a_boolean             is_declarative_context);

extern a_boolean copy_ctor_is_move_ctor(a_routine_ptr  rp);

extern a_boolean routine_is_move_constructor(a_routine_ptr  rp);

extern a_boolean is_copy_assignment_operator_type(
                                 a_type_ptr            routine_type,
                                 a_type_ptr            class_type,
                                 a_boolean             move_assign_okay,
                                 a_boolean             *is_ref_arg,
                                 a_type_qualifier_set  *qualifiers,
                                 a_boolean             *is_base_class_match);

extern
a_boolean routine_is_copy_or_move_assign_operator(
                                               a_routine_ptr         rp,
                                               a_type_qualifier_set  *tqs,
                                               a_boolean             *is_move);

extern a_boolean routine_is_move_assignment_operator(a_routine_ptr  rp);

/* 
Macro that is TRUE if move constructors and move assign operators can be
defined with "= default;".
*/
#define move_operations_can_be_defaulted()                                  \
  (generate_move_operations ||                                              \
   (gpp_mode &&                                                             \
    (clang_mode ? clang_version >= 30000 : gnu_version >= 40500)))

/*
Macro that is TRUE for functions with an indeterminate exception specification.
*/
#define has_indeterminate_exception_spec(rp)                                  \
  ((rp)->type->kind == (a_type_kind)tk_routine &&                             \
   (rp)->type->variant.routine.extra_info->exception_specification != NULL && \
   (rp)->type->variant.routine.extra_info->exception_specification            \
                                         ->indeterminate)

#if DO_IL_LOWERING
extern a_boolean special_member_is_user_provided(a_routine_ptr  rp);
#endif /* DO_IL_LOWERING */

extern a_boolean is_main_function(a_routine_ptr  routine);

extern void switch_il_region(a_memory_region_number region_number);

extern void switch_to_file_scope_region(
                             a_memory_region_number *region_to_switch_back_to);

extern void switch_to_scope_region(
                             a_scope_depth          scope_depth,
                             a_memory_region_number *region_to_switch_back_to);

extern void switch_back_to_original_region(
                              a_memory_region_number region_to_switch_back_to);

extern a_scope_ptr new_file_scope(a_scope_number scope_number);

extern a_scope_ptr new_function_scope(a_scope_number           scope_number,
                                      a_routine_ptr            assoc_routine,
                                      a_memory_region_number   memory_region);

extern a_subobject_path_ptr copy_subobject_path(a_subobject_path_ptr  path);

extern void copy_constant(const a_constant *from,
                          a_constant       *to);

extern void copy_template(a_template *from,
                          a_template *to);

extern void add_constant_to_aggregate(a_constant_ptr con,
                                      a_constant_ptr aggr_con,
                                      a_base_class_ptr bcp,
                                      a_field_ptr      fp);

extern a_constant_ptr add_repeat_con(a_constant_ptr  elem_con,
                                     a_targ_size_t   count);

extern void explode_string_initializer(a_constant_ptr con);

extern a_targ_size_t string_constant_length(a_constant_ptr  con);

extern void combine_initializers(a_constant_ptr     first,
                                 a_dynamic_init_ptr first_dip,
                                 a_constant_ptr     second,
                                 a_dynamic_init_ptr second_dip);

extern void combine_initializer_constants(a_constant_ptr first,
                                          a_constant_ptr second);

extern a_constant_ptr alloc_unshared_constant(a_constant *cp);

extern a_constant_ptr alloc_unshared_constant_full(a_constant *cp,
                                                   a_boolean  source_in_il,
                                                   a_boolean  suppress_copy);

extern a_constant_ptr alloc_unshared_constant_in_region(
                                                    a_constant *cp,
                                                    a_boolean  in_file_region);

/*
Options for copy_expr_tree et al.
*/
typedef int an_expr_copy_options_set;
#define CE_NO_OPTIONS 0
#define CE_DOING_INLINING_OF_FUNCTION_CALL 0x1
			/* TRUE if this copy operation is copying an expression
			   for inlining and extra operations like remapping
			   should be done. */
#define CE_TRANSFER_DESTR_ENTITY_DESCR 0x2
			/* TRUE if, when copying a dynamic initialization
			   entry, the pointer to the destructible entity
			   description should be transferred to the copy. */
#define CE_INSIDE_CONDITIONAL_EXPRESSION 0x4
			/* TRUE if the expression is being copied into a
			   context that is under a conditional operator. */
#define CE_UNLINK_SOURCE_DESTRUCTIONS 0x8
			/* TRUE if destructions in the source expression
			   should be unlinked from their object lifetimes. */
#define CE_COPYING_EVALUATED_DEFAULT_ARG_EXPR 0x10
			/* TRUE if this copy operation is copying a default
			   argument expression, i.e., making a real use from
			   the scanned expression.  This is set only for
			   evaluated expressions (specifically, potentially
			   evaluated ones), not unevaluated ones. */
#define CE_COPIED_CONSTANTS_MAY_BE_SHARED 0x20
			/* TRUE if when constants are copied they may be
			   shared.  FALSE means such constants must be
			   unshared. */
#define CE_REPLACE_STRINGS_BY_VARIABLES 0x40
			/* TRUE if string literals with sequence_number != 0
			   should be replaced by variables as they are
			   copied.  More precisely, address constants that
			   point to ck_string constants with sequence_number
			   != 0 are rewritten as the addresses of the
			   generated variables. */
#define CE_COPY_NOT_EVALUATED 0x80
			/* TRUE if the copy is in an unevaluated context. */
#define CE_DEST_CONSTANT_IS_NOT_ALLOC_IN_IL 0x100
			/* TRUE if the destination address provided to
			   copy_constant_full might not be an IL address (e.g.,
			   it's the address of a stack variable). */
#define CE_COPYING_FROM_ONE_FUNC_TO_ANOTHER 0x200
			/* TRUE if the copy is from one function scope
			   memory region into another. */
#define CE_SRC_CONSTANT_IS_NOT_ALLOC_IN_IL 0x400
			/* TRUE if the source address provided to
			   copy_constant_full might not be an IL address (e.g.,
			   it's the address of a stack variable). */
#define CE_COPYING_FOR_CONSTEXPR_MASTER_EXPR 0x800
			/* When TRUE, the copy is being done to create the
			   master copy of an expression to be used later to
			   do constexpr evaluation.  The copy is made so
			   that we still have a pristine copy if the original
			   version is changed by lowering.  Therefore, copy
			   all constants in the source, even in cases where
			   we ordinarily wouldn't. */
#define CE_COPYING_FOR_CONSTEXPR_FOLDING 0x1000
			/* When TRUE, the copy is being done as part of
			   constexpr evaluation folding. */
#define CE_COPYING_FOR_LOCAL_EXPR_NODE_REF 0x2000
			/* When TRUE, the copy is being done to create an
			   expression tree in function-scope memory that
			   can be referenced by an a_local_expr_node_ref
			   entry.  Expression nodes already copied in this
			   operation are reused, so shared backing-expression
			   subtrees stay shared. */
#define CE_PRESERVE_RESCAN_INFO 0x4000
			/* When TRUE, rescan info is preserved in the copy of
			   expression nodes. */
#define CE_COPYING_DMI_DIP 0x8000
			/* When TRUE, the dynamic initialization being copied
			   is for a data member initializer.  In the IL,
			   data member initializers have no lifetime, but
			   should be treated as having a lifetime (because the
			   copy will be placed in one).  In particular, this
			   is required to handle overlapping lifetime
			   destructions. */
#define CE_ALWAYS_COPY_BACKING_EXPRESSIONS 0x10000
			/* When TRUE, always copy backing expressions.  The
			   caller is responsible for ensuring that references
			   to local entities don't leak into the wrong scope
			   memory (e.g., a reference to a local entity in
			   file-scope memory). */
#define CE_COPYING_DEFAULT_MEMBER_INIT 0x20000
			/* TRUE if this copy operation is copying a default
			   member initializer expression, i.e., making a
			   constructor-specific use of the scanned
			   expression. */
#define CE_CONST_EVAL_SUB_EXPRESSION 0x40000
			/* TRUE if this copy operation is copying an operand of
			   an expression in an immediate evaluation context. */

a_constant_ptr copy_constant_full(a_constant_ptr           old_constant,
                                  a_constant_ptr           new_constant,
                                  an_expr_copy_options_set options);

/*
Flags used to specify options to compare_constants.
*/
typedef int a_compare_constants_options_set;

#define CC_NO_OPTIONS		0x0
#define CC_STRICTLY_IDENTICAL	0x1
			/* When this flag is not set the qualifiers are
			   stripped from the constant type before they
			   are compared; otherwise, a "const int 5" and
			   an "int 5" are treated as nonidentical. */
#define CC_EXACT_EQUIVALENCE 0x2
			/* TRUE if the compared constants should be fully
			   equivalent.  In particular, when comparing template
			   parameters of tpck_param kind, the constant pointers
			   must match, not just the coordinates.  Also, rescan
			   information (in underlying expressions) must be
			   equivalent. */
#define CC_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED 0x4
			/* TRUE if, when checking that the types of expressions
			   match, one should check for an exact match of the
			   expressions under any dependent decltypes. */
#define CC_TEMPLATE_TEMPLATE_PARAM 0x8
			/* TRUE if, when comparing template parameters of
			   tpck_param kind, the coordinates are not
			   required to match and certain type mismatches
			   are allowed. */
#define CC_RELAXED_ADDRESS_OF_CONSTANT_COMPARISON 0x10
			/* TRUE if abk_constant entries compare equal if they
			   point to constants that compare equal (instead of
			   pointing to the same constant entry). */
#define CC_CONSTEXPR_NAME_EQUIVALENCES_ALLOWED 0x20
			/* TRUE if constexpr entities that haven't been folded
			   can be considered equivalent so long as they have
			   the same name. */
#define CC_TEMPLATE_ARG 0x40
			/* TRUE when comparing template nontype arguments. */
#define CC_GENERIC 0x80
			/* TRUE if the expressions were built up in a
			   template-dependent context, which makes some
			   attributes (like value category) unreliable. */

extern a_boolean compare_constants(a_constant_ptr                   cp1,
                                   a_constant_ptr                   cp2,
                                   a_compare_constants_options_set  options);

extern a_boolean compare_reflections(a_reflection_value               rv1,
                                     a_reflection_value               rv2,
                                     a_compare_constants_options_set  options);

extern a_constant_ptr copy_unshared_constant(a_constant_ptr old_constant);

extern a_boolean eq_constants(a_constant *cp1,
                              a_constant *cp2);

extern a_boolean expr_tree_contains_template_param_constant(
                                             an_expr_node_ptr  node,
                                             a_constant_ptr    cp);

extern a_source_correspondence_ptr nontype_templ_arg_constant_corresp(
                                                      a_constant_ptr constant);

extern a_boolean nontype_templ_arg_constant_involves_invalid_linkage(
                                                      a_constant_ptr constant);

extern a_boolean has_non_file_scope_ref(a_constant *cp);

extern a_boolean constant_is_shareable(a_constant *cp);

extern a_constant_ptr alloc_shareable_constant(a_constant *cp);

extern a_constant_ptr shareable_fs_string_constant(a_const_char *str);

extern void add_backing_expression_for_named_constant(a_constant *cp);

extern void add_scope_to_class_type(a_type_ptr  type);

/* Forward declare some class types (and their associated pointer types). */
typedef struct a_scope_stack_entry *a_scope_stack_entry_ptr;
typedef struct a_template_param *a_template_param_ptr;

extern a_scope_ptr ensure_il_scope_exists(a_scope_stack_entry_ptr  ssep);

extern void add_to_namespaces_list(a_namespace_ptr  nsp);

extern void add_to_using_declarations_list(a_using_decl_ptr  udp,
					   a_scope_depth     depth);

extern void add_to_using_directives_list(a_using_decl_ptr  udp,
					 a_scope_depth     depth);

extern void add_to_constants_list(a_constant_ptr con_ptr,
                                  a_boolean      at_file_scope);

extern void empty_shareable_constants_table(void);

extern void empty_func_shareable_constants_table(void);

extern void set_integer_constant(a_constant		*cp,
                                 a_host_large_integer	value,
                                 an_integer_kind	kind);

extern void set_unsigned_integer_constant(a_constant		*cp,
                                          a_host_large_unsigned	value,
                                          an_integer_kind	kind);

#if !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
extern void set_integer_constant(a_constant		*cp,
                                 const an_integer_value	&value,
                                 an_integer_kind	kind);

extern void set_unsigned_integer_constant(a_constant			*cp,
                                          const an_integer_value	&value,
                                          an_integer_kind		kind);
#endif /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

/* Macro to retrieve the list of constants associated with an enum type.  The
   location of the list is different depending on whether it's a scoped enum
   or not. */
#define enum_constants(tp)                                                   \
  (!integer_type_supp(tp)->enumerator_list_seen ?                            \
      (a_constant_ptr)NULL :                                                 \
      (integer_type_is_scoped_enum((tp)) ?                                   \
          (tp)->variant.integer.enum_info.assoc_scope->constants :           \
          (tp)->variant.integer.enum_info.constant_list))
     
extern a_boolean is_enum_constant(a_constant_ptr con);

extern a_boolean is_ordinary_string_constant(a_constant_ptr constant);

extern a_boolean is_wide_string_constant(a_constant_ptr constant);

#define is_normal_character_kind(kind)  ((kind) == (a_character_kind)chk_char)

extern void make_zero_of_proper_type(a_type_ptr desired_type,
                                     a_constant *zero_constant);

extern an_expr_node_ptr make_zero_expr(a_type_ptr  tp);

extern void make_one_of_proper_type(a_type_ptr desired_type,
                                    a_constant *one_constant);

extern an_expr_node_ptr make_one_expr(a_type_ptr  tp);

extern void make_bool_constant_value(a_boolean       val,
                                     a_constant_ptr  con);

extern a_boolean make_value_initialized_constant(a_type_ptr type,
                                                 a_constant *con);

extern void make_uuidof_constant(a_type_ptr     uuidof_type,
                                 a_constant_ptr uuidof_con);

extern a_type_ptr typeid_constant_type(a_boolean is_cli_typeid);

extern void make_typeid_constant(a_type_ptr     typeid_type,
                                 a_boolean      is_cli_typeid,
                                 a_constant_ptr typeid_con);

#if MICROSOFT_EXTENSIONS_ALLOWED
void make_cli_array_constant(an_expr_node_ptr gcnew_expr,
                             a_constant_ptr   array_con);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void set_dynamic_init_constant(a_dynamic_init_ptr dip,
                                      a_constant         *constant);

extern void add_to_dynamic_inits_list(a_dynamic_init_ptr dip);

extern a_local_static_variable_init_ptr make_local_static_variable_init(
                                                  a_variable_ptr     var,
                                                  a_scope_ptr        var_scope,
                                                  an_init_kind       init_kind,
                                                  a_constant_ptr     con,
                                                  a_dynamic_init_ptr dip);

extern a_local_static_variable_init_ptr find_local_static_variable_init(
                                                      a_variable_ptr  var,
                                                      a_scope_ptr     scope);

extern a_vla_dimension_ptr make_vla_dimension(
                                       a_type_ptr        array_type,
                                       an_expr_node_ptr  expr_node,
                                       a_boolean         in_prototype_scope,
                                       a_source_position *position);

extern void get_variable_initializer(a_variable_ptr     variable,
                                     a_scope_ptr        var_scope,
                                     an_init_kind       *init_kind,
                                     an_initializer_ptr *initializer);

#if !STANDALONE_UTILITY_PROGRAM
extern a_constant_ptr template_arg_operand_constant(a_template_arg  *tap);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_constant_ptr initializer_constant(a_variable_ptr var);

extern void remove_from_variables_list(a_variable_ptr var_ptr,
                                       a_scope_depth  scope_depth);

extern void add_to_variables_list(a_variable_ptr var_ptr,
                                  a_scope_depth  scope_depth);

extern void add_to_parameters_list(a_variable_ptr param_ptr);

extern a_variable_ptr make_variable(a_type_ptr      type_ptr,
                                    a_storage_class storage_class,
                                    a_scope_depth   scope_depth);

extern a_variable_ptr make_handler_parameter(a_type_ptr  type_ptr);

extern void add_temporary_to_front_of_variables_list(a_variable_ptr temp,
                                                     a_scope_ptr    scope);

extern a_variable_ptr alloc_temporary_variable(a_type_ptr temp_type,
                                               a_boolean  force_static);

/*
Flags used to specify options to next_applicable_field.
*/
typedef int a_next_field_options_set;

#define NF_INITIALIZABLE 0x1
                        /* When this flag is set, only "initializable"
                           fields are returned. */
#define NF_SKIP_PROPERTY_OR_EVENT 0x2
                        /* Do not return any field that is a Microsoft property
                           or event. */
#define NF_SKIP_FIELDS_ADDED_BY_LOWERING 0x4
                        /* Do not return any field that was added by the
                           lowering process. */
#define NF_SKIP_OPTIMIZED_EMPTY_CLASS 0x8
                        /* Do not return any field that is an optimized empty
                           class. */

extern a_field_ptr next_applicable_field(a_field_ptr              field,
                                         a_next_field_options_set options);

/* Utility macro to return only initializable fields. */
#define next_initializable_field(field)                                       \
  (next_applicable_field((field),                                             \
              (a_next_field_options_set)(NF_INITIALIZABLE |                   \
                                         NF_SKIP_PROPERTY_OR_EVENT)))

/* Utility macro to return only initializable fields not added by lowering. */
#define next_proper_initializable_field(field)                                \
  (next_applicable_field((field),                                             \
              (a_next_field_options_set)(NF_INITIALIZABLE |                   \
                                         NF_SKIP_FIELDS_ADDED_BY_LOWERING |   \
                                         NF_SKIP_PROPERTY_OR_EVENT)))

/* Utility macro to return only non-empty initializable fields. */
#define next_non_empty_initializable_field(field)                             \
  (next_applicable_field((field),                                             \
              (a_next_field_options_set)(NF_INITIALIZABLE |                   \
                                         NF_SKIP_OPTIMIZED_EMPTY_CLASS |      \
                                         NF_SKIP_PROPERTY_OR_EVENT)))

/* Utility macro to return only non-empty fields. */
#define next_non_empty_field(field)                                           \
  (next_applicable_field((field),                                             \
              (a_next_field_options_set)(NF_SKIP_OPTIMIZED_EMPTY_CLASS |      \
                                         NF_SKIP_PROPERTY_OR_EVENT)))

/* Utility macro to return only fields not added by lowering. */
#define next_proper_field(field)                                \
  (next_applicable_field((field),                                             \
              (a_next_field_options_set)(NF_SKIP_FIELDS_ADDED_BY_LOWERING |   \
                                         NF_SKIP_PROPERTY_OR_EVENT)))


#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Macros to examine property and event members (and their accessor functions).
*/
#define field_is_property_or_event(fp)  ((fp)->property_or_event_descr != NULL)
#define property_or_event_kind_is(ep, pek)                                   \
   ((ep)->property_or_event_descr->kind == (a_property_or_event_kind)pek)
#define field_is_nontrivial_property_or_event(fp)                            \
  (field_is_property_or_event(fp) &&                                         \
   !(fp)->property_or_event_descr->is_trivial)
#define var_is_property_or_event(vp)  ((vp)->property_or_event_descr != NULL)
#define rout_is_cli_accessor(rp) \
  ((rp)->special_kind >= (int)sfk_first_accessor && \
   (rp)->special_kind <= (int)sfk_last_accessor)
#define rout_is_generic_definition(rp) ((rp)->is_generic_definition)
#define rout_is_generic_instance(rp) ((rp)->is_generic_instance)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/*lint -emacro(506,field_is_property_or_event)*/
#define field_is_property_or_event(fp)  FALSE
/*lint -emacro(506,field_is_nontrivial_property_or_event)*/
#define field_is_nontrivial_property_or_event(fp)  FALSE
/*lint -emacro(506,rout_is_cli_accessor)*/
#define rout_is_cli_accessor(rp)  FALSE
/*lint -emacro(506,rout_is_generic_definition)*/
#define rout_is_generic_definition(rp)  FALSE
/*lint -emacro(506,rout_is_generic_instance)*/
#define rout_is_generic_instance(rp)  FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean is_compound_assignment_operator(an_expr_operator_kind op);

extern a_type_ptr fixed_point_result_type(a_type_ptr  type_1,
                                          a_type_ptr  type_2);

extern a_type_ptr compound_assignment_operation_type(an_expr_node_ptr expr);

extern a_type_kind binary_operation_type_kind(an_expr_operator_kind  op,
                                              a_type_ptr             op1_type,
                                              a_type_ptr             op2_type);

extern a_constant_ptr constant_value_of_dynamic_init(a_dynamic_init_ptr dip);

extern a_dynamic_init_ptr skip_constexpr_init_folding(a_dynamic_init_ptr dip);

extern
a_dynamic_init_ptr effective_dynamic_init_for_initializer_list_object(
                                         a_dynamic_init_ptr dip,
                                         a_type_ptr         *init_entity_type);

extern void perform_scheduled_routine_moves(void);

extern void schedule_move_to_current_end_of_routines_list(a_routine_ptr  rp);

extern void remove_from_routines_list(a_routine_ptr rout_ptr,
                                      a_scope_depth scope_depth);

extern void add_to_routines_list(a_routine_ptr  rout_ptr,
                                 a_scope_depth  scope_level);

extern void add_to_asm_entries_list(an_asm_entry_ptr asm_entry_ptr);

extern void add_to_labels_list(a_label_ptr label_ptr);

extern void copy_statement(a_statement *from,
                           a_statement *to);

extern void change_statement_into_block(a_statement_ptr statement,
                                        a_statement_ptr *orig_statement);

extern void set_expr_result_not_used(an_expr_node_ptr node);

/*
Macro to set a possibly NULL expression as not used.
*/
#define set_possibly_null_expr_result_not_used(node)                          \
  if ((node) != NULL) set_expr_result_not_used(node)

extern void set_node_operator(an_expr_node_ptr      node,
                              an_expr_operator_kind kind,
	   	              a_type_ptr            type,
                              a_boolean             is_lvalue,
		              an_expr_node_ptr      operands);

extern void copy_node_value_category(an_expr_node_ptr from,
                                     an_expr_node_ptr to);

extern an_expr_node_ptr make_operator_node(an_expr_operator_kind kind,
			   	           a_type_ptr            type,
			   	           an_expr_node_ptr      operands);

extern an_expr_node_ptr make_lvalue_operator_node(
                                               an_expr_operator_kind kind,
                                               a_type_ptr            type,
                                               an_expr_node_ptr      operands);

extern an_expr_node_ptr make_comma_node(an_expr_node_ptr expr1,
                                        an_expr_node_ptr expr2);

extern an_expr_node_ptr make_comma_node_if_necessary(an_expr_node_ptr node1,
                                                     an_expr_node_ptr node2);

extern void overwrite_node(an_expr_node_ptr node,
                           an_expr_node_ptr source_node);

extern an_expr_node_ptr error_node(void);

extern an_expr_node_ptr fs_error_node(void);

extern an_expr_node_ptr alloc_node_for_constant(a_constant *constant);

extern an_expr_node_ptr alloc_node_for_allocated_constant(
                                                         a_constant *constant);

extern an_expr_node_ptr node_for_integer_constant(long            value,
                                                  an_integer_kind kind);

extern an_expr_node_ptr node_for_host_large_integer(
					     a_host_large_integer	value,
                                             an_integer_kind		kind);

extern a_boolean is_bad_type_for_template_arg_operand(a_type_ptr type);

extern a_boolean is_cast_operation_node(an_expr_node_ptr expr);

extern a_boolean is_generated_dynamic_init(a_dynamic_init_ptr dip);

extern a_boolean is_error_dynamic_init(a_dynamic_init_ptr dip);

extern a_boolean is_valid_object_for_nontype_arg(a_constant_ptr  con);

extern a_variable_ptr variable_designated_by_object_reflection(
                                                         a_constant_ptr  con);

extern a_boolean is_valid_ptr_or_ptr_to_member_templ_arg_constant(
                                                         a_constant_ptr  con);

extern a_boolean is_valid_class_templ_arg_constant(a_constant_ptr  con);

extern a_boolean is_valid_templ_arg_constant(a_constant_ptr  con);

/*
Bit flags used to indicate information about the context of a conversion
that may allow or suppress certain conversions or diagnostics.
*/
typedef int a_conv_context_set;
#define CCO_DEFAULT ((a_conv_context_set)0x0)
#define CCO_INITIALIZING_VARIABLE ((a_conv_context_set)0x1)
			/* The result of the conversion initializes a
			   variable (or a part of an aggregate variable). */
#define CCO_INITIALIZING_RETURN_VALUE ((a_conv_context_set)0x2)
			/* The result of the conversion initializes the
			   return value of a function. */
#define CCO_NONTYPE_TEMPLATE_ARG ((a_conv_context_set)0x4)
			/* The result of the conversion is the value of
			   a nontype template argument. */
#define CCO_CAST ((a_conv_context_set)0x8)
			/* The conversion is being done by a cast (of any
			   kind). */
#define CCO_FUNC_NOTATION_CAST ((a_conv_context_set)0x10)
			/* The conversion is being done by a functional-
			   notation cast. */
#define CCO_BITWISE_ASSIGNMENT_PARAM ((a_conv_context_set)0x20)
			/* The result of the conversion initializes the
			   notional parameter of a bitwise copy assignment
			   operator.  Also used for non-class assignments. */
#define CCO_MOVE_CTOR_OR_ASSIGN_PARAMETER ((a_conv_context_set)0x40)
			/* The result of the conversion initializes the
			   parameter of a move constructor or move assignment
			   operator. */
#define CCO_MOVE_OPTIMIZATION_ALLOWED ((a_conv_context_set)0x80)
			/* The result of the conversion is potentially
			   subject to the move optimization. */
#define CCO_ANY_CV_QUAL_ON_PTR_ALLOWED ((a_conv_context_set)0x100)
			/* The result of the conversion is a pointer type,
			   and we will accept any cv-qualification on the
			   type underlying the pointer. */
#define CCO_STATIC_LIFETIME ((a_conv_context_set)0x200)
			/* When CCO_INITIALIZING_VARIABLE is TRUE, this
			   is also TRUE if the variable being initialized
			   has static lifetime. */
#define CCO_DIRECT_INITIALIZATION ((a_conv_context_set)0x400)
			/* The conversion is a direct-initialization context,
			   e.g., a parenthesized initializer, new, or
			   cast. */
#define CCO_ALLOW_EXPLICIT_CONV_FUNCTIONS ((a_conv_context_set)0x800)
			/* Explicit conversion functions should be allowed in
			   this context.  This is in addition to other normal
			   reasons why they might be allowed. */
#define CCO_INITIALIZING_FIELD ((a_conv_context_set)0x1000)
			/* The result of the conversion initializes a
			   field of a class (C++11 nonstatic data member
			   initializer, or NSDMI). */
#define CCO_SUPPRESS_USER_CONVERSIONS_IN_OVL_RES ((a_conv_context_set)0x2000)
			/* Suppress user-defined conversions in overload
			   resolution for some specific reason related to
			   the context (i.e., not just because this is
			   copy-initialization). */
#define CCO_NEW_INITIALIZER ((a_conv_context_set)0x4000)
			/* The result of the conversion is the initializer
			   in a "new".  Currently set only for a braced
			   initializer. */
#define CCO_LEAVE_AS_OBJECT ((a_conv_context_set)0x8000)
			/* Used with a reference initialization in
			   prep_list_initializer to request that the result
			   be left as an object (see parameter on
			   prep_reference_initializer_operand).  Implied
			   by CCO_CAST. */
#define CCO_ARG_VIA_COPY_CTOR ((a_conv_context_set)0x10000)
			/* Used when passing an argument operand via copy
			   constructor. */
#define CCO_STMT_EXPR_RESULT ((a_conv_context_set)0x20000)
			/* Used when calling prep_elision_initializer_operand
			   for the result expression of a GNU statement
			   expression. */
#define CCO_TYPE_TRAITS_CHECK ((a_conv_context_set)0x40000)
			/* Used when a conversion is being checked for a
			   type traits helper function (e.g.,
			   __is_convertible). */
#define CCO_CONVERTED_CONSTANT_EXPR ((a_conv_context_set)0x80000)
			/* Used when the conversion context is a "converted
			   constant expression" (a C++11 concept). */
#define CCO_SINGLETON_BRACED_INIT ((a_conv_context_set)0x100000)
			/* Used when the conversion source is a single
			   value enclosed in braces. */
#define CCO_BASE_INIT ((a_conv_context_set)0x200000)
			/* Used when the conversion is for a constructor
			   initializer for a base subobject. */
#define CCO_IGNORE_EXPLICIT_MEMBERS ((a_conv_context_set)0x400000)
			/* When considering constructors or conversion
			   operators ignore those that are "explicit" (possibly
			   after substitution of "explicit( <bool-expr> )"). */
#define CCO_IS_CONSTANT_EVALUATED ((a_conv_context_set)0x800000)
			/* Used to indicate that in this context calls to
			   std::is_constant_evaluated() should produce
			   "true". */
#define CCO_EXPLICIT_CAST ((a_conv_context_set)0x1000000)
			/* Used in combination with CCO_CAST to indicate that
			   a cast appeared explicitly in the source code. */
#define CCO_BUILTIN_OP ((a_conv_context_set)0x2000000)
			/* Used to indicate that this is a conversion for a
			   built-in operator. */
#define CCO_UNWRAPPED_BRACED_LIST ((a_conv_context_set)0x4000000)
			/* Used while looking for a conversion to class type to
			   indicate that the argument list was originally
			   enclosed by braces.  Also TRUE when initializing the
			   elements of a std::initializer_list object. */
#define CCO_CONVERT_INITIALIZER ((a_conv_context_set)0x8000000)
			/* Used to indicate that this is a conversion for an
			   initializer via convert_initializer. */
#define CCO_PREP_ARGUMENT ((a_conv_context_set)0x10000000)
			/* Used to indicate that this is a conversion for an
			   argument that has already been determined to match
			   its parameter. */
#define CCO_FORCE_DEPENDENCE ((a_conv_context_set)0x20000000)
			/* TRUE if overload resolution should treat the call
			   as dependent in any case. */


/*
Flags used to specify options to copy_type_with_substitution.
*/

typedef int a_ctws_options_set;

#define CTWS_NO_OPTIONS			0x0
#define CTWS_IS_PARENT			0x1
			/* TRUE if the type being processed is the parent
			   type of a class member.  This affects the way
			   in which names are looked up during the
			   substitution process. */
#define CTWS_COPY_ARG_OPERAND_INFO	0x2
			/* TRUE if arg_operand information on nontype
			   template arguments should be copied over to the
			   substituted arguments for use in a rescan. */
#define CTWS_NON_CONSTANT_EXPR		0x4
			/* TRUE when copying a non-constant expression,
			   which can come up under a sizeof. */
#define CTWS_IS_PARTIAL_ORDER_CHECK	0x8
			/* TRUE when creating the substituted routine type
			   as part of the partial ordering process. */
#define CTWS_INSIDE_EXPR_RESCAN		0x10
			/* TRUE when we're inside a rescan of an expression.
			   Used to control pushing a template instantiation
			   scope when we first enter an expression rescan,
			   and not again in any nested processing. */
#define CTWS_IS_CALL_CONTEXT		0x20
			/* TRUE when the substitution routines are called
			   from rescan contexts and the entity being
			   substituted is the function name in a call (e.g.,
			   was followed by a "(" in the source). */
#define CTWS_PRESERVE_DEDUCED_PACKS	0x40
			/* TRUE if a deduced parameter pack should be
			   retained in the substituted type if there are no
			   template arguments associated with the pack.
			   This is used during the initial substitution of
			   explicitly supplied template arguments so that
			   the resulting type will still be usable to deduce
			   the remaining pack. */
#define CTWS_NONTYPE_TEMPLATE_ARG	0x80
			/* TRUE when the substitution routines are called
			   from rescan contexts and the entity being
			   substituted is a nontype template argument
			   expression. */
#define CTWS_PARTIAL_ARG_LIST_OKAY	0x100
			/* TRUE when substituting into a template argument
			   list (via copy_template_arg_list_with_substitution)
			   should permit a list that doesn't cover all the
			   corresponding template parameters (because
			   additional arguments will be deduced later). */
#define CTWS_RETURN_TYPE		0x200
			/* TRUE if the type being processed is the return type
			   of a function type. */
#define CTWS_IS_PARTIAL_SPECIALIZATION_CHECK	0x400
			/* TRUE when creating the substituted template
			   argument list as part of the partial specialization
			   matching process. */
#define CTWS_IS_RESCAN_OF_NOEXCEPT_OPERAND	0x800
			/* TRUE when rescanning the operand of a noexcept
			   operator. */
#define CTWS_CAST_OPERAND			0x1000
			/* TRUE when substituting/rescanning the operand of a
			   cast. */
#define CTWS_ADJUST_COORDINATES		0x2000
			/* TRUE when doing substitution to adjust the
			   coordinates of template parameters.  This is used
			   when forming a deduction guide for a constructor
			   template or adjusting the nesting depth of template
			   parameters of a friend function template when
			   instantiating its enclosing class. */
#define CTWS_MAY_BE_RESCANNED		0x4000
			/* TRUE if the result of substituting an expression may
			   itself be subject to substitution ("rescanning")
			   later on. */
#define CTWS_EXPLICIT_CAST_OPERAND	0x8000
			/* TRUE when CTWS_CAST_OPERAND is TRUE and the
			   associated cast is explicit in the source. */
#define CTWS_IN_PARENT_SUBSTITUTION	0x10000
			/* TRUE when substitution is being recursively
			   performed on parent template parameters and
			   arguments (for example, as is done in
			   substitute_constant). */
#define CTWS_IS_OVERLOAD_CANDIDATE	0x20000
			/* TRUE when substitution is being used to produce a
			   function type as an overload resolution candidate.
			   In this case, certain substitutions are not done
			   (e.g., of the exception specification when it is
			   part of the type). */
#define CTWS_ALIAS_DEDUCTION_GUIDE	0x40000
			/* TRUE when doing substitution to create a deduction
			   guide routine type for an alias template. */


/*
Structure used to represent a set of function parameters that resulted from
a pack expansion during the type substitution process.
*/
typedef struct a_variadic_param_info *a_variadic_param_info_ptr;
typedef struct a_variadic_param_info {
  a_variadic_param_info_ptr
		next;
			/* The next element on the list of entries, or NULL
			   for the last entry. */
  a_param_type_ptr
		param_type;
			/* The parameter type entry resulting from the
			   variadic expansion. */
  a_param_type_ptr
		orig_param_type;
			/* The parameter type entry for the parameter pack
			   representing the variadic expansion. */
   int		level;
			/* The value of routine_type_levels when this entry
			   was created. */
} a_variadic_param_info;


/*
Structure used to pass information between the routines that do template
argument substitution (primarily copy_type_with_substitution).
*/
typedef struct a_ctws_state *a_ctws_state_ptr;
typedef struct a_ctws_state {
  a_variadic_param_info_ptr
		variadic_param_info;
			/* A list of parameters created by variadic
			   pack expansions during this substitution. */
  a_variadic_param_info_ptr
		variadic_param_info_tail;
			/* The end of the list of parameters created by
			   variadic pack expansions during this
			   substitution. */
  a_template_param_ptr
		orig_class_templ_params;
			/* During the creation of a deduction guide template,
			   pack expansion descriptors need to be copied and
			   references to the original parameters replaced with
			   references to the new versions.  This is the list
			   of the original template parameters of the
			   enclosing class template. */
  a_template_param_ptr
		orig_ctor_templ_params;
			/* This is like orig_class_templ_params except it
			   is the list of the original constructor template
			   parameters. */
  a_template_param_ptr
		new_templ_params;
			/* This is the list of replacement parameters.  The
			   first N correspond to the N elements of
			   orig_class_templ_params.  The remaining elements
			   correspond to the elements of
			   orig_ctor_templ_params. */
  a_type_ptr	old_this_class;
			/* During deduction guide substitution, a this_class
			   that matches this value will be replaced with
			   new_this_class. */
  a_type_ptr	new_this_class;
			/* During deduction guide substitution, if this is
			   non-NULL, it is used as a replacement for
			   a this_class that matches old_this_class. */
  const Dyn_array<a_template_param_ptr>
		*alias_parameter_pack_mapping;
			/* During deduction guide substitution for alias
			   templates, if this is non-NULL, it is used as a map
			   for parameter packs, indexed by the parameter
			   position in the deduction guide template parameter
			   list, to the new template parameter. */
  Dyn_array<a_boolean>
		*record_used_arguments;
			/* During deduction guide substitution for alias
			   templates, if this is non-NULL, it is used to record
			   the positions of template arguments being used
			   during substitution.  This array is resized and
			   updated by the template substitution routines. */
  int32_t	routine_type_levels;
			/* The level of nesting of routine types. */
  int32_t	parent_levels;
			/* The number of times we have started copying a
			   parent type. */
  a_boolean
		preserve_deduced_packs;
			/* TRUE if a deduced parameter pack should be
			   retained in the substituted type.  This is used
			   during the initial substitution of explicitly
			   supplied template arguments so that the resulting
			   type will still be usable to deduce the remaining
			   pack elements. */
  a_boolean	in_parent_substitution;
			/* TRUE if this is the substitution of an enclosing
			   pack expansion and such expansions should be
			   ignored unless the pack uses only non-enclosing
			   packs. */
  a_boolean	substituted_parameter_pack;
			/* TRUE if a pack argument was substituted.  This is
			   set by the template substitution routines. */
  a_boolean	unexpanded_pack;
			/* TRUE if a pack was not expanded because template
			   parameters were for a different template depth.
			   This is set by the template substitution
			   routines. */
} a_ctws_state;


extern a_constant_ptr strip_implicit_casts_if_template_param_constant(
						a_constant_ptr	constant);

/*
A structure representing one set of substitution pairs (i.e., one list of
template parameters and associated template arguments).
*/
struct a_subst_pairs_descr {
  a_template_param_ptr
		params;
			/* The template parameters of the substitution. */
  a_template_arg_ptr
		args;
			/* The template arguments of the substitution. */
  a_bit_field	args_known_dependent:1;
			/* TRUE if args is known to be
			   instantiation-dependent. */
  a_bit_field	args_known_nondependent:1;
			/* TRUE if args is known not to be
			   instantiation-dependent. */
  a_bit_field	adjust_coordinates:1;
			/* TRUE if CTWS_ADJUST_COORDINATES option
			   should be set for substitution. */
  a_bit_field	alias_deduction_guide:1;
			/* TRUE if CTWS_ALIAS_DEDUCTION_GUIDE option
			   should be set for substitution. */
};

typedef Dyn_array<a_subst_pairs_descr>
		a_subst_pairs_array;
			/* A type used to hold the complete set of substitution
			   pairs for an entity.  The array has multiple entries
			   if it results from a nested template. */


extern a_constant_ptr copy_template_param_con(
                                    a_constant_ptr        con,
                                    a_template_arg_ptr    template_arg_list,
                                    a_template_param_ptr  template_param_list,
                                    a_type_ptr            guide_type,
                                    a_source_position     *source_pos,
                                    a_ctws_options_set    options,
                                    a_boolean             *copy_error,
                                    a_ctws_state_ptr      ctws_state,
                                    a_constant_ptr        constant);


extern an_expr_node_ptr copy_template_param_expr(
                                    an_expr_node_ptr      expr,
                                    a_template_arg_ptr    template_arg_list,
                                    a_template_param_ptr  template_param_list,
                                    a_type_ptr            guide_type,
                                    a_source_position     *source_pos,
                                    a_ctws_options_set    options,
                                    a_boolean             *copy_error,
                                    a_ctws_state_ptr      ctws_state,
                                    a_constant_ptr        constant,
                                    a_constant_ptr        *alloc_con);

extern an_expr_node_ptr copy_expr_with_substitutions(
                                    an_expr_node_ptr      expr,
                                    a_template_arg_ptr    template_arg_list,
                                    a_template_param_ptr  template_param_list,
                                    a_ctws_options_set    options,
                                    a_boolean             *copy_error,
                                    a_ctws_state_ptr      ctws_state);

extern a_type_ptr type_of_decltype_expr_with_substitution(
                                 a_type_ptr               type,
                                 an_expr_node_ptr         expr,
                                 a_template_arg_ptr       template_arg_list,
                                 struct a_template_param  *template_param_list,
                                 a_ctws_options_set       options,
                                 a_boolean                *copy_error,
                                 a_ctws_state_ptr         ctws_state);


typedef struct a_symbol a_symbol_il_h_dummy_typedef;
extern struct a_symbol *
symbol_for_template_param_unknown_entity_con_after_substitution(
                                 a_constant_ptr           con,
                                 a_template_arg_ptr       template_arg_list,
                                 struct a_template_param  *template_param_list,
                                 a_source_position        *source_pos,
                                 a_ctws_state_ptr         ctws_state,
                                 a_ctws_options_set       options);

extern a_constant_ptr copy_template_param_con_with_substitution(
                                 a_constant_ptr           con,
                                 a_template_arg_ptr       template_arg_list,
                                 struct a_template_param  *template_param_list,
                                 a_type_ptr               template_param_type,
                                 a_source_position        *source_pos,
                                 a_ctws_options_set       options,
                                 a_boolean                *copy_error,
                                 a_ctws_state_ptr         ctws_state);

extern void substitute_constant(a_constant_ptr           *p_constant,
                                a_type_ptr               parent_class,
                                struct a_template_param  *t_params,
                                a_template_arg_ptr       t_args,
                                a_ctws_options_set       options,
                                a_ctws_state             *ctws_state,
                                a_source_position        *source_pos,
                                a_boolean                *p_error);

extern void fully_substitute_constant(a_constant_ptr             src_con,
                                      a_subst_pairs_array const  &subst_pairs,
                                      a_constant_ptr             dst_con);

extern void increment_template_dependent_enum_constant(a_constant_ptr  con);

extern a_boolean is_operator_returning_bool(an_expr_operator_kind op);

extern an_expr_node_ptr add_cast(an_expr_node_ptr node,
                                 a_type_ptr       new_type);

extern an_expr_node_ptr add_cast_if_necessary(an_expr_node_ptr node,
                                              a_type_ptr       new_type);

extern an_expr_node_ptr add_cast_to_glvalue(an_expr_node_ptr node,
                                            a_type_ptr       type);

extern an_expr_node_ptr add_cast_to_glvalue_if_necessary(
                                                        an_expr_node_ptr node,
                                                        a_type_ptr       type);

extern an_expr_node_ptr add_rvalue_class_adjust_node(an_expr_node_ptr node,
                                                     a_type_ptr       type);

extern an_expr_node_ptr copy_node(an_expr_node_ptr expr);

extern an_expr_node_ptr copy_list_of_expr_trees(
                                            an_expr_node_ptr         expr_list,
                                            an_expr_copy_options_set options);

extern an_expr_node_ptr copy_expr_tree(an_expr_node_ptr         expr,
                                       an_expr_copy_options_set options);

#if MINIMAL_INLINING
extern an_expr_node_ptr copy_expr_tree_for_inlining(
                                            an_expr_node_ptr expr,
                                            a_boolean        *inlining_failed);
#endif /* MINIMAL_INLINING */

extern a_dynamic_init_ptr copy_dynamic_init(a_dynamic_init_ptr       dip,
                                            an_expr_copy_options_set options);

extern an_expr_node_ptr copy_default_arg_expr(
                               a_routine_ptr    rout_ptr,
                               a_param_type_ptr ptp,
                               a_boolean        inside_conditional_expression,
                               a_boolean        potentially_evaluated,
                               a_boolean        evaluated);

extern an_expr_node_ptr duplicate_default_arg_expr(an_expr_node_ptr expr);

extern an_expr_node_ptr copy_default_arg_expr_list(
                               a_routine_ptr    rout_ptr,
                               a_param_type_ptr ptp,
                               a_boolean        inside_conditional_expression,
                               a_boolean        potentially_evaluated,
                               a_boolean        evaluated);

extern a_boolean is_gc_lvalue_expr(an_expr_node_ptr expr);

extern a_boolean cannot_be_null(an_expr_node_ptr expr);

extern an_expr_node_ptr var_lvalue_expr(a_variable_ptr var);

extern an_expr_node_ptr var_rvalue_expr(a_variable_ptr var);

extern an_expr_node_ptr var_addr_expr(a_variable_ptr var);

extern an_expr_node_ptr function_lvalue_expr(a_routine_ptr rout);

extern an_expr_node_ptr function_rvalue_expr(a_routine_ptr rout);

extern an_expr_node_ptr function_addr_expr(a_routine_ptr rout);

extern an_expr_node_ptr rvalue_expr_for_lvalue(an_expr_node_ptr expr);

extern an_expr_node_ptr xvalue_expr_for_lvalue(an_expr_node_ptr expr);

extern an_expr_node_ptr add_indirection_to_node(an_expr_node_ptr node);

extern an_expr_node_ptr add_ref_indirection_to_node(an_expr_node_ptr node);

extern a_type_ptr type_of_address_of(an_expr_node_ptr node);

extern an_expr_node_ptr add_address_of_to_node(an_expr_node_ptr node);

extern
an_expr_node_ptr glvalue_from_class_prvalue_node(an_expr_node_ptr node,
                                                 a_boolean        is_xvalue);

extern an_expr_node_ptr add_reference_to_to_node(an_expr_node_ptr node);

extern void set_address_taken_for_variable_or_routine_expr(
                                                        an_expr_node_ptr node);

extern an_expr_node_ptr add_object_lifetime_to_expr(
                                             an_expr_node_ptr       expr,
                                             an_object_lifetime_ptr lifetime);

extern an_expr_node_ptr this_param_value_expr(void);

extern an_expr_node_ptr field_lvalue_selection_expr(an_expr_node_ptr node,
                                                    a_field_ptr      field);

extern an_expr_node_ptr field_rvalue_selection_expr(an_expr_node_ptr node,
                                                    a_field_ptr      field);

extern void adjust_anonymous_union_field_selection(an_expr_node_ptr node,
                                                   a_field_ptr      au_field);

extern void adjust_nonstandard_anonymous_object_field_references(
                                                  an_expr_node_ptr node,
                                                  struct a_symbol  *field_sym,
                                                  a_boolean        std_also);

extern an_expr_node_ptr fe_field_lvalue_selection_expr(an_expr_node_ptr node,
                                                       a_field_ptr      field);

extern an_expr_node_ptr base_class_selection_expr(an_expr_node_ptr node,
                                                  a_base_class_ptr bcp);

extern an_expr_node_ptr base_class_rvalue_expr(an_expr_node_ptr node,
                                               a_base_class_ptr bcp);

extern void mark_routine_referenced_full(a_routine_ptr routine,
                                         a_boolean     instantiate,
                                         a_boolean     elided_reference);

extern void mark_routine_referenced(a_routine_ptr routine);

extern void set_routine_defined(a_routine_ptr rout);

extern a_statement_ptr make_assignment_statement(an_expr_node_ptr dest,
                                                 an_expr_node_ptr source);

extern a_statement_ptr make_array_assignment_statement(an_expr_node_ptr dest,
                                                      an_expr_node_ptr source);

extern void set_block_scope_handler(a_handler_ptr  handler);

extern a_statement_ptr alloc_expr_statement(an_expr_node_ptr node);

extern void add_to_templates_list(a_template_ptr  tp,
                                  a_scope_depth   scope_depth);

extern a_boolean has_nonreal_parent_type(a_source_correspondence	*scp);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void add_to_ms_attributes_list(an_ms_attribute_ptr	msap,
                                      a_scope_depth		scope_depth);

extern a_routine_ptr selectively_overridden_function(a_routine_ptr  rp);

extern an_assembly_visibility get_assembly_visibility_of(a_type_ptr  type);

#define class_has_public_assembly_visibility(tp)                             \
  (class_type_supp(tp)->assembly_visibility ==                               \
                                      (an_assembly_visibility)av_public)

#define is_nonpublic_nested_class(tp)                                        \
  (tp->source_corresp.is_class_member &&                                     \
   tp->source_corresp.access != (an_access_specifier)as_public)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
extern void add_to_ms_if_exists_list(an_ms_if_exists_ptr	msiep,
                                     a_scope_depth		scope_depth);
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

#if RECORD_MACROS_IN_IL
extern void add_to_macros_list(a_macro_ptr  mp);
#endif /* RECORD_MACROS_IN_IL */

typedef struct a_pragma_kind_description *a_pragma_kind_description_ptr;

extern a_pragma_ptr add_non_entity_pragma_to_list(
                    a_pragma_kind_description_ptr pragma_descr,
                    a_source_position             pragma_position,
                    char                          *pragma_text,
#if MICROSOFT_EXTENSIONS_ALLOWED
                    a_boolean                     is_microsoft_pragma_operator,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                    a_scope_depth                 pragma_scope_depth);

extern a_pragma_ptr add_entity_pragma_to_list(
                    a_pragma_kind_description_ptr pragma_descr,
                    a_source_position             pragma_position,
                    char                          *pragma_text,
#if MICROSOFT_EXTENSIONS_ALLOWED
                    a_boolean                     is_microsoft_pragma_operator,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                    char                          *entity_ptr,
                    an_il_entry_kind              entity_kind);

extern a_pragma_ptr find_assoc_pragma(char             *entity_ptr,
                                      an_il_entry_kind entity_kind,
                                      a_scope_ptr      func_scope,
                                      a_pragma_ptr     prev_assoc_pragma);

extern a_boolean operator_takes_lvalue_op1(an_expr_operator_kind op);

extern a_boolean operator_takes_lvalue_op2(an_expr_operator_kind op);

EXTERN_THREAD an_object_lifetime_ptr
		curr_object_lifetime;
			/* The top of the currently active object lifetime
			   stack. */

#define lifetime_is(olp, lifetime_kind)                                      \
  ((olp)->kind == (an_object_lifetime_kind)(lifetime_kind))

extern void add_to_destructions_list(a_dynamic_init_ptr      dip,
                                     an_object_lifetime_ptr  olp);

extern void add_to_end_of_destructions_list(
                    a_dynamic_init_ptr      dip,
                    an_object_lifetime_ptr  olp,
                    a_boolean               update_parent_destruction_sublist);

extern void add_to_destructions_list_following(a_dynamic_init_ptr dip,
                                               a_dynamic_init_ptr new_dip);

extern void record_end_of_lifetime_destruction(
                                        a_dynamic_init_ptr  dip,
                                        a_boolean           static_lifetime,
                                        a_boolean           block_lifetime);

extern void record_partial_aggregate_cleanup_destruction(
                                                 a_dynamic_init_ptr dip,
                                                 a_boolean          evaluated);

extern void promote_lifetime_contents_to_curr_object_lifetime(
                                                      an_object_lifetime *olp);

extern void free_object_lifetime(an_object_lifetime_ptr  olp);

extern void bind_object_lifetime(an_object_lifetime_ptr  olp,
                                 an_il_entry_kind        entity_kind,
                                 char                    *entity_ptr);

extern void unbind_object_lifetime(an_object_lifetime_ptr  olp);

extern
void push_or_repush_object_lifetime(an_il_entry_kind         entity_kind,
                                    char                     *entity_ptr,
                                    an_object_lifetime_ptr   olp,
                                    an_object_lifetime_kind  kind,
                                    a_boolean                is_reactivation);

#define push_object_lifetime(entity_kind, entity_ptr, kind)                  \
  (push_or_repush_object_lifetime(entity_kind, entity_ptr,                   \
                                 (an_object_lifetime_ptr)NULL, kind,         \
                                 /*is_reactivation=*/FALSE))

extern a_boolean is_useless_object_lifetime(an_object_lifetime_ptr  olp);

extern void remove_from_destruction_list(a_dynamic_init_ptr  dip);

extern void unlink_expr_destructions(an_expr_node_ptr expr);

extern void mark_object_lifetime_as_useless(an_object_lifetime_ptr  olp);

extern a_boolean pop_object_lifetime_full(a_boolean unbound_okay);

#define pop_object_lifetime()                                                \
  (pop_object_lifetime_full(/*unbound_okay=*/FALSE))

extern an_object_lifetime_ptr innermost_block_object_lifetime(
                                             an_object_lifetime_ptr  olp);

extern void record_start_of_source_file(
				 a_source_file_ptr parent_file,
			         a_seq_number      seq_number,
				 a_line_number     line_number,
			         a_const_char      *file_name,
			         a_const_char      *full_name,
                                 a_const_char      *name_as_written,
			         a_source_file_ptr *new_file,
                                 a_boolean	   is_include_file,
				 a_boolean	   is_system_include,
                                 a_boolean         is_preinclude,
				 a_boolean	   preinclude_macros_only,
				 a_boolean	   is_implicit_include,
				 a_boolean	   from_system_include_dir,
				 a_boolean	   is_assembly_file);

extern void record_inclusion_of_module_source_file(
                                         a_const_char      *file_name,
                                         a_source_position *inserted_position,
                                         a_module_ptr      mod,
                                         uint32_t          max_line_number);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void record_inclusion_of_assembly_source_file(
                                     a_const_char      *file_name,
                                     a_const_char      *full_name,
                                     a_const_char      *name_as_written,
                                     a_source_file_ptr *new_file,
                                     a_boolean         is_system_include,
                                     a_boolean         is_preinclude,
                                     a_source_position *inserted_position);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void record_resumption_of_source_file(a_source_file_ptr	curr_file,
					     a_seq_number	seq_number,
					     a_line_number	line_number);

extern void record_end_of_source_file(a_source_file_ptr curr_file,
			              a_seq_number      seq_number);
extern a_source_file_ptr primary_source_file_for_seq(a_seq_number seq_number);
extern a_boolean is_at_end_of_translation_unit(a_seq_number seq_number);
extern a_source_file_ptr source_file_for_seq(a_seq_number   seq_number,
                                             a_line_number  *line_number,
                                             a_boolean      *at_end_of_source,
                                             a_boolean      physical_line);
extern
a_source_file_ptr conv_seq_to_file_and_line(a_seq_number  seq_number,
                                            a_const_char  **file_name,
                                            a_const_char  **full_name,
                                            a_line_number *line_number,
                                            a_boolean     *at_end_of_source);

extern a_source_file_ptr eff_primary_source_file(void);

extern a_boolean same_source_file(a_source_position_ptr pos_1,
                                  a_source_position_ptr pos_2);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_cli_metadata_file_ptr map_assembly_index_to_cmfp(
                                             an_assembly_index assembly_index);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if !STANDALONE_UTILITY_PROGRAM

extern void conv_seq_to_physical_file_and_line(
                                          a_seq_number      seq_number,
                                          a_source_file_ptr *src_file,
                                          a_line_number     *physical_line,
                                          a_boolean         *at_end_of_source);

extern a_boolean seq_is_in_include_file(a_seq_number seq_number);

extern void break_instance_source_corresp(a_source_correspondence *sc);

extern void break_source_corresp(a_source_correspondence *sc);

extern void break_constant_source_corresp(a_constant_ptr cp);

extern void fix_memory_region_problems_in_copied_constant(a_constant_ptr cp);

#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_boolean seq_is_in_system_header(a_seq_number  seq_number);

#define in_system_header()                                                    \
  seq_is_in_system_header(pos_curr_token.seq)

extern a_source_correspondence *source_corresp_for_il_entry(
                                                 char              *entity_ptr,
                                                 an_il_entry_kind  kind);

extern a_boolean is_defined(char *entity_ptr, an_il_entry_kind kind);


/*
Macro that is TRUE if the node is of the given kind.
*/
#define node_is(node, node_kind)                                        \
  ((node)->kind == (an_expr_node_kind)(node_kind))

/*
Macro that is TRUE if the node is an operation node.
*/
#define is_operation_node(node)	 node_is(node, enk_operation)

/*
Macro that is TRUE if the node is a constant node.
*/
#define is_constant_node(node)  node_is(node, enk_constant)

/*
Macro to get the constant from a constant node.
*/
#define node_constant(node)  ((node)->variant.constant.ptr)

/*
Macro to test for a constant kind in a constant node.
*/
#define node_constant_is(node, con_kind)                                \
  constant_is(node_constant(node), (con_kind))

/*
Macro that is TRUE if the node is a variable node.
*/
#define is_variable_node(node)  node_is(node, enk_variable)

/*
Macro to get the variable from a variable node.
*/
#define node_variable(node)  ((node)->variant.variable.ptr)

/*
Macro that is TRUE if the node is a field node.
*/
#define is_field_node(node)  node_is(node, enk_field)

/*
Macro to get the field from a field node.
*/
#define node_field(node)  ((node)->variant.field.ptr)

/*
Macro that is TRUE if the node is a routine node.
*/
#define is_routine_node(node)  node_is(node, enk_routine)

/*
Macro to get the routine from a routine node.
*/
#define node_routine(node)  ((node)->variant.routine.ptr)

/*
Macro that is TRUE if the node is a type operand node.
*/
#define is_type_node(node)  node_is(node, enk_type_operand)

/*
Macro to get the type from a type operand node.
*/
#define type_operand_type(node)  ((node)->variant.type_operand.type)

/*
Macro that is TRUE if the node is a temporary node (enk_temp_init/enk_lambda).
*/
#define is_temp_node(node)						\
	(node_is(node, enk_temp_init) || node_is(node, enk_lambda))

/*
Macro that is TRUE if the node is an error node.
*/
#define is_error_node(node)  node_is(node, enk_error)

/*
Return TRUE if the node is a glvalue, meaning an lvalue or an xvalue.
*/
#define is_glvalue_node(node)						\
  ((node)->is_lvalue || (node)->is_xvalue)

/*
Return TRUE if the operator in the given node (which must be an operation
node) is "op".
*/
#define node_operator_is(node, op)                                      \
  ((node)->variant.operation.kind == (an_expr_operator_kind)(op))


EXPAND a_boolean node_is_operator(an_expr_node_ptr       node,
                                  an_expr_operator_kind  kind)
/*
Return TRUE if (and only if) the given node is an enk_operator node for the
given operator kind.
*/
{
  return is_operation_node(node) && node_operator_is(node, kind);
}  /* node_is_operator */


/*
Return TRUE if the given node (which must be an operation node) has its
type_kind field set to the given value.
*/
#define node_operator_type_kind_is(node, tkind)                    \
  ((node)->variant.operation.type_kind == (a_type_kind)(tkind))

/*
Return TRUE if "node" is a function call operation.
*/
#define is_call_node(node)                                              \
  (is_operation_node((node)) &&                                         \
   (node_operator_is((node), eok_call) ||                               \
    node_operator_is((node), eok_dot_member_call) ||                    \
    node_operator_is((node), eok_points_to_member_call) ||              \
    node_operator_is((node), eok_dot_pm_call) ||                        \
    node_operator_is((node), eok_points_to_pm_call)))

/*
Return TRUE if "node" is a vacuous destructor call.
*/
#define is_vacuous_dtor_call_node(node)                                 \
  (is_operation_node((node)) &&                                         \
   (node_operator_is((node), eok_dot_vacuous_destructor_call) ||        \
    node_operator_is((node), eok_points_to_vacuous_destructor_call)))

inline a_boolean is_cmp_node(an_expr_node  *node)
/*
Return TRUE if "node" is a comparison operation.
*/
{
  node = skip_parens(node);
  return is_operation_node((node)) &&
         (node_operator_is((node), eok_eq) ||
          node_operator_is((node), eok_ne) ||
          node_operator_is((node), eok_gt) ||
          node_operator_is((node), eok_lt) ||
          node_operator_is((node), eok_ge) ||
          node_operator_is((node), eok_le));
}  /* is_cmp_node */


#if GNU_EXTENSIONS_ALLOWED

/*
Return TRUE if the given operator is a gnu min/max operator (>? or <?).
*/
#define is_gnu_min_max_operator(op) \
 ((op) == (an_expr_operator_kind)eok_gnu_min || \
  (op) == (an_expr_operator_kind)eok_gnu_max)
#endif /* GNU_EXTENSIONS_ALLOWED */

/*
The operands for eok_subscript or eok_padd are a pointer and an
integral subscript which can appear in either order.  This macro returns
the pointer operand of these nodes.
*/
#define subscript_or_padd_pointer_operand(node)                        \
        ((node)->variant.operation.pointer_operand_is_second ?         \
         (node)->variant.operation.operands->next :                    \
         (node)->variant.operation.operands)


inline void extract_reflected_entity(a_reflection_value  *rvp)
/*
If rvp represents an expression node that is a simple reference to an entity
(enk_variable, enk_field, enk_routine, enk_constant), replace rvp by a
reflection for that entity.
*/
{
  if (rvp->entity.kind == iek_expr_node) {
    an_expr_node  *node = (an_expr_node*)rvp->entity.ptr;
    switch (node->kind) {
      case enk_variable:
        rvp->entity.kind = iek_variable;
        rvp->entity.ptr = (char*)node_variable(node);
        rvp->local_scope_number =
                            get_parent_scope_of(node_variable(node))->number;
        break;
      case enk_constant:
        rvp->entity.kind = iek_constant;
        rvp->entity.ptr = (char*)node_constant(node);
        break;
      case enk_field:
        rvp->entity.kind = iek_field;
        rvp->entity.ptr = (char*)node_field(node);
        break;
      case enk_routine:
        rvp->entity.kind = iek_routine;
        rvp->entity.ptr = (char*)node_routine(node);
        break;
      default:
        break;
    }  /* switch */
    if (in_file_scope(rvp->entity.ptr)) {
      rvp->local_scope_number = FILE_SCOPE_NUMBER;
    }  /* if */
  }  /* if */
}  /* extract_reflected_entity */


inline void strip_template_arg(a_reflection_value  *rvp)
/*
If rvp points to a reflection value for a template argument replace it by the
reflection value for the underlying type, constant, or template.
*/
{
  if (rvp->entity.kind == iek_template_arg) {
    a_template_arg  *tap = (a_template_arg*)rvp->entity.ptr;
    switch (tap->kind) {
      case tak_type:
        rvp->entity.kind = iek_type;
        rvp->entity.ptr = (char*)tap->variant.type;
        break;
      case tak_nontype:
        rvp->entity.kind = iek_constant;
        rvp->entity.ptr = (char*)tap->variant.constant;
        break;
      case tak_template:
        rvp->entity.kind = iek_template;
        rvp->entity.ptr = (char*)tap->variant.templ.ptr;
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* if */
}  /* strip_template_arg */


inline a_source_correspondence *source_corresp_for_reflection(
                                                     a_reflection_value  *rvp)
/*
Return the source correspondence for the entity associated with rvp, if any.
*/
{
  a_reflection_value       rv = *rvp;
  a_source_correspondence  *scp;

  strip_template_arg(&rv);
  if (rv.entity.kind == iek_scope) {
    /* A namespace is reflected as its associated scope, so the correspondence
       is taken from that scope's namespace entry rather than from the scope
       itself (which has none).  A namespace alias is reflected as the alias
       entry and therefore doesn't get here. */
    a_scope_ptr  scope = (a_scope_ptr)rv.entity.ptr;
    /* A scope reflection whose scope could not be recovered (a null
       pointer) has no source correspondence. */
    if (scope != NULL &&
        (scope->kind == sck_namespace ||
         scope->kind == sck_namespace_extension)) {
      scp = &scope->variant.assoc_namespace->source_corresp;
    } else {
      scp = NULL;
    }  /* if */
  } else {
    scp = source_corresp_for_il_entry(rv.entity.ptr, rv.entity.kind);
  }  /* if */
  return scp;
}  /* source_corresp_for_reflection */


extern a_boolean is_zero_constant(a_constant *constant);

/*
Macro that returns TRUE if an IL entry has a name.  (Applies only to
those containing source correspondence information.)
*/
#define has_name(entry) ((entry)->source_corresp.name != NULL)

/*
Macro that returns TRUE if a class/struct/union or enum tag type is unnamed
or is marked as being originally unnamed.
*/
#define is_unnamed_or_originally_unnamed_tag(tag_type)                   \
  ((tag_type)->source_corresp.name == NULL ||                            \
   (is_immediate_class_type(tag_type) &&                                 \
    (tag_type)->variant.class_struct_union.originally_unnamed))

/*
Macro that returns TRUE if the type is unnamed.
*/
#define type_is_unnamed(type)                                           \
  (unmangled_name_of(&(type)->source_corresp) == NULL)

/*
Return TRUE if type is a lambda closure class.
*/
#define type_is_lambda_closure(type)					\
  ((type)->kind == (a_type_kind)tk_class &&				\
   class_type_supp(type)->is_lambda_closure_class)

/*
Return TRUE if rout_type is a routine type for a lambda.
*/
#define is_lambda_body_routine_type(rout_type)				\
  ((rout_type)->variant.routine.extra_info->assoc_routine_is_lambda_body)

/*
Return the unmangled name of an entity, given a pointer to its source
correspondence entry (for unnamed types that have been given a fabricated
name during mangling, returns NULL).
*/
#if NEED_NAME_MANGLING
#define unmangled_name_of(scp)                                          \
  ((scp)->unnamed_entity_given_fabricated_name ? (char *)NULL :         \
     (((scp)->name_has_been_mangled ?                                   \
       (scp)->unmangled_name_or_mangled_encoding : (scp)->name)))
#else /* !NEED_NAME_MANGLING */
#define unmangled_name_of(scp) ((scp)->name)
#endif /* NEED_NAME_MANGLING */

/*
Return the unmangled or fabricated (name given to unnamed entities during
mangling) name of an entity.
*/
#if NEED_NAME_MANGLING
#define unmangled_or_fabricated_name_of(scp)                            \
  ((scp)->name_has_been_mangled ?                                       \
   (scp)->unmangled_name_or_mangled_encoding : (scp)->name)
#else /* !NEED_NAME_MANGLING */
#define unmangled_or_fabricated_name_of(scp) (unmangled_name_of((scp)))
#endif /* NEED_NAME_MANGLING */

/*
Macro that returns TRUE if an IL entry has a name before any name mangling
that was done.  This is useful when testing entities like classes and
namespaces that may have been given a generated name during name
mangling.  (Applies only to those entries containing source correspondence
information.)
*/
#define has_name_before_mangling(entry) \
  (unmangled_name_of(&(entry)->source_corresp) != NULL)

extern void clear_local_scope_ref_if_present(a_source_correspondence *scp);

extern void clear_parent(a_source_correspondence *scp);

extern void set_parent_scope(a_source_correspondence *scp,
                             an_il_entry_kind        entry_kind,
                             a_scope_ptr             parent_scope);

/*
Return TRUE if a constant is an error constant.
*/
#define is_error_constant(cp) ((cp)->kind == (a_constant_repr_kind)ck_error)

/*
Macros for determining static and/or thread storage duration.  Internally,
the difference between the static storage duration and thread storage duration
is the setting of is_thread_local (since thread local storage doesn't have an
a_storage_class setting).  Note that storage classes have been canonicalized
during declaration processing.
*/
#define is_static_or_thread_storage_duration_storage_class(storage_class)     \
  ((storage_class) == (a_storage_class)sc_static ||                           \
   (storage_class) == (a_storage_class)sc_extern ||                           \
   (storage_class) == (a_storage_class)sc_unspecified)

#define var_has_static_or_thread_storage_duration(var)                        \
  is_static_or_thread_storage_duration_storage_class((var)->storage_class)

#define var_has_thread_storage_duration(var)                                  \
  ((var)->is_thread_local &&                                                  \
   (var_has_static_or_thread_storage_duration(var)))

#define var_has_static_storage_duration(var)                                  \
  (!(var)->is_thread_local &&                                                 \
   (var_has_static_or_thread_storage_duration(var)))

/*
Macros used to determine the kind of a template argument.
*/
#define is_type_templ_arg(arg) \
  ((arg)->kind == (a_templ_arg_kind)tak_type)
#define is_nontype_templ_arg(arg) \
  ((arg)->kind == (a_templ_arg_kind)tak_nontype)
#define is_template_templ_arg(arg) \
  ((arg)->kind == (a_templ_arg_kind)tak_template)
#define is_start_of_pack_expansion_templ_arg(arg) \
  ((arg)->kind == (a_templ_arg_kind)tak_start_of_pack_expansion)


extern a_boolean con_is_exact_addr_of_variable(
                                           a_constant_ptr con,
                                           a_variable_ptr *var,
                                           a_boolean      array_decay_allowed);

/*
Macro that returns TRUE if a constant entry is the exact address of
a routine.
*/
#define con_is_exact_addr_of_routine(con)                             \
  ((con)->kind == (a_constant_repr_kind)ck_address &&                 \
   (con)->variant.address.kind == (an_address_base_kind)abk_routine &&\
   (con)->variant.address.offset == 0 && !(con)->implicit_cast)

extern
a_type_ptr make_class_template_placeholder(
                                    struct a_symbol   *class_template,
                                    a_source_position *pos,
                                    struct a_symbol   *bound_template = NULL);

extern a_type_ptr make_auto_type(a_source_position *pos,
                                 a_boolean         is_decltype_auto);

extern a_type_ptr add_placeholder_typeref(a_type_ptr  tp,
                                          a_boolean   is_decltype_auto);

/*
Macro that produces TRUE if the given variable is declared with a placeholder
type.
*/
#define var_declared_with_placeholder_type(var)                              \
  ((var)->declared_with_auto_type_specifier ||                               \
   (var)->declared_with_decltype_auto ||                                     \
   (var)->declared_with_class_template_placeholder)

extern a_base_class_derivation_ptr preferred_virtual_derivation_of(
                                                      a_base_class_ptr  bcp);

/*
Macros that return information about base classes that may, for virtual base
classes, be contingent on the derivation selected.
*/
/* If bcp is a virtual base class, return a pointer to the virtual derivation
   entry associated with its preferred path; otherwise return a pointer to
   the derivation entry pointed to from bcp. */
#define preferred_derivation_of(bcp)                                 \
  ((bcp)->is_virtual ? preferred_virtual_derivation_of(bcp) :        \
                       (bcp)->derivation)


/* Return TRUE if bcp is a direct nonvirtual base class or a virtual base
   class whose preferred derivation is direct. */
#define preferred_derivation_is_direct(bcp)                          \
  ((bcp)->direct &&                                                  \
   (!(bcp)->is_virtual || preferred_virtual_derivation_of(bcp)->direct))

/* Return TRUE if bcp is a direct nonvirtual base class or a virtual base
   class whose "first" derivation (i.e., first as found in a depth-first
   left-to-right search of the derivation graph) is direct. */
#define first_derivation_is_direct(bcp)                              \
  ((bcp)->derivation->direct)


/* Return a derivation path to be used for a cast to the indicated base
   class.  For virtual base classes, this is a single step to the virtual
   base class. */
#define cast_derivation_path_of(bcp)                                  \
  ((bcp)->is_virtual ? (bcp)->derivation->path_tail :                 \
                       (bcp)->derivation->path)

/* Return TRUE if the given base class is virtual or if there is a virtual
   step in its derivation. */
#define any_virtual_steps_in_derivation(bcp)                          \
  ((bcp)->is_virtual || (bcp)->derivation->path->base_class->is_virtual)

/* Return TRUE if the given class needs a virtual function table. */
#if !IA64_ABI
#define needs_virtual_function_table(type)                            \
  ((type)->variant.class_struct_union.any_virtual_functions)
#else /* IA64_ABI */
#define needs_virtual_function_table(type)                            \
  ((type)->variant.class_struct_union.any_virtual_functions ||        \
   (type)->variant.class_struct_union.any_virtual_base_classes)
#endif /* !IA64_ABI */

/* Return TRUE if two template nesting depths should be considered
   equivalent.  Depths are equivalent if they are the same, or if either
   of the depths is NO_NESTING_DEPTH. */
#define equiv_nesting_depths(depth1, depth2)				\
  ((depth1) == (depth2) ||						\
   (depth1) == NO_NESTING_DEPTH || (depth2 == NO_NESTING_DEPTH))

typedef struct a_translation_unit a_translation_unit_dummy_typedef;

#if !STANDALONE_UTILITY_PROGRAM
struct a_translation_unit *trans_unit_for_source_corresp(
                                                 a_source_correspondence *scp);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_routine_ptr enclosing_routine_for_local_type_or_null(a_type_ptr type);

extern a_routine_ptr enclosing_routine_for_local_type(a_type_ptr type);

extern a_scope_ptr function_scope_for_local_type(a_type_ptr type);

extern a_scope_ptr scope_for_routine_or_null(a_routine_ptr rout);

extern a_scope_ptr scope_for_routine(a_routine_ptr rout);

#if DEBUG
extern void db_indent(size_t indent);

extern void db_template_arg_list(a_template_arg_ptr tap);

extern void db_template_name(a_template_ptr  tp);

extern void db_type_name(a_type_ptr  tp);

extern void db_based_types(a_type_ptr  tp);

extern void db_name_full(a_source_correspondence *sc,
                         an_il_entry_kind        kind);

extern void db_name(a_source_correspondence *sc);

extern char *db_name_str_full(a_source_correspondence *scp,
                              an_il_entry_kind        kind,
                              a_boolean               include_func_params);

extern char *db_name_str(a_source_correspondence *sc,
                         an_il_entry_kind        kind);

extern void db_scp(void  *entity);

extern void db_entity_info(char             *entry,
                           an_il_entry_kind kind);

extern void db_access_control(an_access_specifier as);

extern void db_field(a_field_ptr fp, int depth);

extern void db_class_list(a_class_list_entry_ptr list);

extern void db_classes_in_list(an_il_entity_list_entry_ptr list);

extern void db_subobject_path(a_subobject_path  *path);

extern void db_constant(a_constant *cp);

extern void db_param_type_list(a_param_type_ptr  ptp,
                               a_boolean         comma_required = FALSE);
extern void db_type(a_type *tp);

extern void db_function_param_list(a_type_ptr  tp);

extern char* db_qualifiers_str(a_type_qualifier_set  qualifiers);

extern void db_abbreviated_type(a_type *tp);

/* Abbreviated version of db_abbreviated type. */
/*lint -esym(755,db_abbr_type)*/
#define db_abbr_type(tp)                                              \
  (db_abbreviated_type(tp), (void)fputc('\n', f_debug))

extern void db_variable(a_variable_ptr var_ptr);

extern void db_expr_node(an_expr_node_ptr node,
                         size_t           level);

extern void db_expression(an_expr_node_ptr node);

extern void db_expr_range(an_expr_node_ptr node);

extern void db_expr_summary(an_expr_node_ptr  node);

extern void db_ctor_init(a_constructor_init_ptr cip,
                         size_t                 level);

extern void db_cip(a_constructor_init_ptr cip);

extern void db_ctor_init_list(a_constructor_init_ptr cip_list,
                              size_t                 level);

extern void db_cip_list(a_constructor_init_ptr cip_list);

extern void db_dynamic_initializer(a_dynamic_init_ptr  dip,
                                   size_t              level);

extern void db_dip(a_dynamic_init_ptr  dip);

extern void db_initializer(a_variable_ptr  var_ptr,
                           size_t          level);

extern void db_statement_kind(a_statement_kind  kind);

extern void db_statement(a_statement_ptr  sp);

extern void db_statement_list(a_statement_ptr  sp,
                              size_t           indent,
                              a_const_char     *str,
                              int              how_deep);

extern void db_statements(a_statement_ptr statement);

extern void db_scope(a_scope_ptr sp);

extern void db_scope_type_list(a_scope_ptr scope,
                               size_t      indent,
                               a_boolean   do_subscopes);

extern void db_type_lists(a_scope_ptr scope,
                          size_t      indent);

extern void db_destruction(a_dynamic_init_ptr  dip);

extern void db_object_lifetime_name(an_object_lifetime_ptr  olp);

extern void db_object_lifetime(an_object_lifetime_ptr  olp);

extern void db_object_lifetime_stack(void);

extern void db_pending_destructions(a_dynamic_init_ptr      dip,
                                    an_object_lifetime_ptr  stop_at);

extern void db_object_lifetime_with_indentation(an_object_lifetime_ptr  olp,
                                                a_const_char            *str);

extern void db_object_lifetime_tree(an_object_lifetime_ptr olp);

extern unsigned long db_show_il_c_fe_space_used(unsigned long grand_total);

extern unsigned long show_il_space_used(void);

extern void db_seq_number_lookup_table(void);

extern void db_source_file_for_seq_info(void);

extern a_line_number db_line_for_seq(a_seq_number seq_number);

extern void db_scheduled_routine_moves(void);

extern void put_str_to_f_debug(a_const_char                          *str,
                               an_il_to_str_output_control_block_ptr octl);

extern void subst_fail_intercept(void);
#endif /* DEBUG */

/*
Macro to set a flag to TRUE to indicate a substitution failure.
In DEBUG configurations, this also calls subst_fail_intercept, to ease tracking
of substitution failures in a debugger.
*/
#if DEBUG
/*lint -emacro(505,subst_fail)*/
#define subst_fail(x) (subst_fail_intercept(), (x) = TRUE) 
#else /* !DEBUG */
#define subst_fail(x) ((x) = TRUE)
#endif /* DEBUG */

#if ORPHAN_PROCESSING_NEEDED
/*
Like add_orphaned_file_scope_il_entry, but do not enter certain kinds
of entries that can never be orphans (e.g., class members).  The
orphan is recorded in the current translation unit.
*/
#define possibly_add_orphaned_file_scope_il_entry(entry_ptr, entry_kind) \
{ if (fs_orphan_pointer_of(entry_ptr) == NULL) { \
    f_possibly_add_orphaned_file_scope_il_entry((entry_ptr), (entry_kind), \
                                                curr_translation_unit); \
  }  /* if */ \
}  /* possibly_add_orphaned_file_scope_il_entry */
extern
void f_possibly_add_orphaned_file_scope_il_entry(
                                          char                      *entry_ptr,
                                          an_il_entry_kind          entry_kind,
                                          struct a_translation_unit *tup);
#endif /* ORPHAN_PROCESSING_NEEDED */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
#if !STANDALONE_UTILITY_PROGRAM
extern void add_scope_orphaned_il_lists(a_scope_ptr scope);
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

extern void eliminate_pragmas_for_file_scope_entities(a_scope_ptr scope);

extern void clear_function_body(a_scope_ptr sp);

extern void detach_from_object_lifetime_tree(an_object_lifetime_ptr olp);

#if MAINTAIN_NEEDED_FLAGS
extern void eliminate_bodies_of_unneeded_functions(void);

#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
extern void eliminate_unneeded_scope_orphaned_list_entries(void);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

extern void eliminate_default_arg_object_lifetimes(a_type_ptr type);

extern void eliminate_routine_default_arg_object_lifetimes(a_routine_ptr rout);

extern void eliminate_variable_default_arg_object_lifetimes(
                                                           a_variable_ptr var);

extern void eliminate_unneeded_il_entries(a_scope_ptr scope);
#endif /* MAINTAIN_NEEDED_FLAGS */

extern void detach_dynamic_init_lifetimes(a_dynamic_init_ptr dip);

extern void attach_dynamic_init_lifetimes(
                                        an_object_lifetime_ptr parent,
                                        a_dynamic_init_ptr     dip,
                                        a_boolean              only_sub_inits);

extern void clear_variable_definition(a_variable_ptr variable);

extern a_routine_ptr vtbl_decider_function_for_class(a_type_ptr class_type,
                                                     a_boolean  *unknown);

extern a_namespace_ptr f_skip_namespace_aliases(a_namespace_ptr nsp);

extern a_boolean is_member_of_unnamed_namespace(a_source_correspondence *scp);

#if DO_IL_LOWERING
extern a_boolean routine_should_be_externalized_for_exported_templates(
                                                           a_routine_ptr rout);
extern a_boolean variable_should_be_externalized_for_exported_templates(
                                                           a_variable_ptr var);
#endif /* DO_IL_LOWERING */

#if DO_IL_LOWERING || NEED_NAME_MANGLING
extern a_boolean routine_might_exist_in_multiple_copies(a_routine_ptr rout);
#endif /* DO_IL_LOWERING || NEED_NAME_MANGLING */

extern a_param_type_ptr get_routine_param_types(a_routine_ptr rp);

/*
Given a namespace pointer, return a pointer to the actual namespace,
skipping any namespace aliases that might be present.
*/
#define skip_namespace_aliases(nsp)					\
  ((nsp)->is_namespace_alias ? f_skip_namespace_aliases(nsp) : (nsp))

extern a_type_ptr init_predeclared_class(a_type_kind  kind,
                                         a_const_char *name);

extern void enter_predeclared_class(a_type_ptr         predeclared_type,
                                    a_scope_depth      scope_depth,
                                    a_source_position  *pos);

extern a_targ_alignment alignment_of_variable(a_variable_ptr  vp);

extern an_attribute_ptr find_attribute(an_attribute_kind  kind,
                                       an_attribute_ptr   attributes);

#define has_attr(kind, list)                                                 \
  (list != NULL && find_attribute(kind, list) != NULL)

#if GNU_FUNCTION_MULTIVERSIONING
extern an_attribute_ptr find_last_target_attribute(
                                                  an_attribute_ptr attributes);
/*
Utility that returns TRUE if the routine is a GNU function multiversion
"representative" function.
*/
#define is_multiversion_representative(routine) \
 (has_gnu_routine_supp(routine) && \
  (routine)->gnu_extra_info->is_representative)

#endif /* GNU_FUNCTION_MULTIVERSIONING */

#define routine_does_not_return(rp)                                          \
  (skip_typerefs(rp->type)->variant.routine.extra_info->does_not_return)

extern a_hash_value hash_constant(a_constant *cp);

extern a_hash_value hash_expr(an_expr_node_ptr expr);

extern a_hash_value hash_template_arg_list(a_template_arg_ptr	tap);

#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED

extern a_boolean routine_has_default_calling_convention(
                                           a_routine_type_supplement_ptr rtsp);

extern a_calling_convention normalized_calling_conv(a_calling_convention  cc);

#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */

extern an_expr_node_ptr unwrap_if_tpck_expression(an_expr_node_ptr  expr);

extern a_boolean compare_expressions(an_expr_node_ptr                node1,
                                     an_expr_node_ptr                node2,
                                     a_compare_constants_options_set options);


EXPAND a_requires_clause_ptr trailing_requires_clause(a_routine_ptr  rp)
/*
Return the trailing requires-clause associated with rp, if any.
*/
{
  return rp->trailing_requires_clause;
}  /* trailing_requires_clause */


extern a_boolean equiv_requires_clauses(a_requires_clause_ptr  rcp1,
                                        a_requires_clause_ptr  rcp2);

extern void rebuild_structures_on_il_read(void);

#if CHECKING
#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean tree_has_correct_lvalueness(an_expr_node_ptr root);

extern void check_operation_node_consistency(an_expr_node_ptr expr);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern void check_result_not_used_flag(an_expr_node_ptr node);
#endif /* CHECKING */

#if ENSURE_LOWERED_TYPE_LIST_ORDERING
extern void fix_type_list_ordering_problems(void);
#endif /* ENSURE_LOWERED_TYPE_LIST_ORDERING */

extern a_targ_alignment field_alignment_for(a_type_ptr  type);

extern void il_reset(void);

extern void il_one_time_init(void);

extern void il_trans_unit_init(void);

extern void il_init(void);


#if UPC_EXTENSIONS_ALLOWED

#define upc_dynamic_threads() (upc_num_threads == 0)

extern a_boolean upc_block_size_too_large(a_host_large_unsigned  block_size);

EXTERN_THREAD a_upc_block_size
		max_upc_block_size
#if VAR_INITIALIZERS
			= MAX_UPC_BLOCK_SIZE
#endif /* VAR_INITIALIZERS */
					    ;
			/* The maximum allowable UPC block size. */
#endif /* UPC_EXTENSIONS_ALLOWED */

#if MODULE_ID_NEEDED

extern void use_variable_or_routine_for_module_id_if_needed(
                                             a_source_correspondence_ptr scp,
                                             an_il_entry_kind            kind);

#endif /* MODULE_ID_NEEDED */

extern void destination_type_for_reference_cast(an_expr_node_ptr  expr,
                                                a_type            *ref_type);

extern a_boolean pm_constant_is_null(a_constant_ptr constant);

#if MAINTAIN_NEEDED_FLAGS
extern void clear_instantiation_required_on_unneeded_entities(
                                                            a_scope_ptr scope);
#endif /* MAINTAIN_NEEDED_FLAGS */

extern an_expr_node_ptr make_dummy_lvalue_expr(a_type_ptr type);

extern an_expr_node_ptr expr_before_type_adjustment(an_expr_node_ptr expr);

/*
Get the name reference associated with a node, if any.
*/
#define name_ref_for_node(node)                                               \
  (is_constant_node(node) ? (node)->variant.constant.name_reference :         \
   is_variable_node(node) ? (node)->variant.variable.name_reference :         \
   is_routine_node(node) ? (node)->variant.routine.name_reference :           \
   is_field_node(node) ? (node)->variant.field.name_reference :               \
   is_type_node(node) ? (node)->variant.type_operand.name_reference :         \
   /* else */ (a_name_reference*)NULL)

#if BACK_END_IS_CP_GEN_BE
/*
If an enk_routine node has an associated name reference, return its
"special_kind" field.  Otherwise, produce sfk_none.
*/
#define special_kind_for_routine_node(node)                                   \
  ((node)->variant.routine.name_reference != NULL ?                           \
       (node)->variant.routine.name_reference->special_kind :                 \
       (a_special_function_kind)sfk_none)

/*
Get the property/event descriptor associated with a routine node, if any.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING
#define property_or_event_for_routine_node(node)                              \
  (((node)->variant.routine.name_reference != NULL &&                         \
    !special_kind_is((node)->variant.routine.name_reference, sfk_none)) ?     \
       (node)->variant.routine.name_reference                                 \
             ->variant.property_or_event_descr :                              \
       (a_property_or_event_descr_ptr)NULL)
#else /* !(MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING) */
#define property_or_event_for_routine_node(node)                              \
  ((a_property_or_event_descr_ptr)NULL)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING */
#endif /* BACK_END_IS_CP_GEN_BE */

extern
a_targ_alignment compute_alignof_value(a_type_ptr         alignof_type,
                                       a_boolean          is_type,
                                       an_expr_node_ptr   expr,
                                       a_source_position  *diag_pos,
                                       a_boolean          *p_is_error,
                                       a_boolean          *p_template_case);

#if IA64_ABI
extern a_targ_size_t compute_dsize(a_type_ptr  class_type);
#endif /* IA64_ABI */

/*
The canonical form of the introductory part of an operator-function-id (i.e.,
"operator").
*/
#define CANONICAL_OPERATOR_FUNCTION_INTRO "operator"
#define LENGTH_CANONICAL_OPERATOR_FUNCTION_INTRO \
  (sizeof(CANONICAL_OPERATOR_FUNCTION_INTRO)-1)

/*
The canonical form of the introductory part of a conversion-function-id (i.e.,
"operator ").
*/
#define CANONICAL_CONVERSION_FUNCTION_INTRO "operator "
#define LENGTH_CANONICAL_CONVERSION_FUNCTION_INTRO \
  (sizeof(CANONICAL_CONVERSION_FUNCTION_INTRO)-1)

/*
The canonical form of the introductory part of a literal-operator-id (i.e.,
the part without the suffix).
*/
#define CANONICAL_LITERAL_OPERATOR_INTRO "operator \"\""
#define LENGTH_CANONICAL_LITERAL_OPERATOR_INTRO \
  (sizeof(CANONICAL_LITERAL_OPERATOR_INTRO)-1)

/*
Return a pointer to the ud-suffix position of a canonical
literal-operator-id (operator ""suffix).
*/
#define ud_suffix_from_literal_operator_id(name) \
  ((name) + LENGTH_CANONICAL_LITERAL_OPERATOR_INTRO)

/*
Utility that returns TRUE if the two ck_string constants have the same value.
*/
#define string_constants_are_the_same(con1, con2)                             \
  ((con1) == (con2) ||                                                        \
   ((con1)->variant.string.length == (con2)->variant.string.length &&         \
    memcmp((con1)->variant.string.value, (con2)->variant.string.value,        \
           size_t_arg((con1)->variant.string.length)) == 0))

/*
Structure used to represent a derivation path.
*/
struct a_derivation_path {
  a_derivation_step_ptr
		head;   /* Pointer to the first entry in the derivation
			   path. */
  a_derivation_step_ptr
		tail;   /* Pointer to the last entry in the derivation
			   path. */
};

/* Bit vector used to pass flags into walk_parents.  Each bit represents a
   flag. */
typedef int a_walk_parents_flag_set;
/* Constants defining bits in the input bit vector used in calls to
   walk_parents. */
#define WP_NO_INPUT_FLAGS 0x0
#define WP_NAMESPACE 0x01
                        /* Invoke the callback routine for each namespace that
                           is a parent of the specified entity. */
#define WP_TYPE 0x02    /* Invoke the callback routine for each type that is
                           a parent of the specified entity. */
#define WP_ROUTINE 0x04 /* Invoke the callback routine for each routine that is
                           a parent of the specified entity. */
#define WP_SELF 0x08    /* Invoke the callback routine for the entity itself.
                           Note that the callback is invoked regardless of the
                           setting of the other WP_* flags. */

typedef struct a_walk_parents_control_block {
  a_boolean   terminate;
                        /* When set to TRUE, forces walk_parents to terminate
                           the walk. */
  void        *ptr;     /* Pointer to walk-specific data passed from caller
                           to callback (not interpreted by walk_parents). */
} a_walk_parents_control_block;

/* Typedef for callback from walk_parents. */
typedef void (*a_walk_parent_callback)(a_source_correspondence      *scp,
                                       an_il_entry_kind             kind,
                                       a_walk_parents_control_block *wpcp);

extern void walk_parents(a_source_correspondence      *scp,
                         an_il_entry_kind             kind,
                         a_walk_parent_callback       callback,
                         a_walk_parents_control_block *wpcb,
                         a_walk_parents_flag_set      options);

extern void eval_order_for_op_kind(an_opname_kind kind,
                                   a_boolean      *eval_left_to_right,
                                   a_boolean      *eval_right_to_left);

extern void eval_order_for_binary_node_kind(
                                    an_expr_operator_kind kind,
                                    a_boolean             *eval_left_to_right,
                                    a_boolean             *eval_right_to_left);

extern an_expr_node_ptr *find_expression_in_initializer(a_constant_ptr con);

/*
Utility to reverse a list only if flag is TRUE.  Note that "list" is modified.
When called a second time it restores the list to its original state.
*/
#define reverse_simple_list_if(flag, list)                                    \
  if ((flag)) {                                                               \
    (list) = reverse_simple_list((list));                                     \
  }  /* if */

EXTERN_THREAD a_boolean
		record_form_of_name_reference;
			/* TRUE if the form of all name references should be
			   recorded in the IL.  When this is FALSE, some
			   name references may still be recorded (e.g.,
			   if needed for ABI purposes). */

namespace detail {

/*
The following specializations provide Is_trivially_copyable and
Is_trivially_destructible support for il.h and il_def.h types.
*/

template<>
struct Is_trivially_copyable_edg_impl<a_reflection_value> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<a_reflection_value> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_copyable_edg_impl<a_subst_pairs_descr> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<a_subst_pairs_descr> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

}  /* namespace detail */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef IL_H */

