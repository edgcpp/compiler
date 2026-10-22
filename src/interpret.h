/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

interpret.h -- Interface to IL interpreter for constexpr functions

*/
/* Avoid including these declarations more than once: */
#ifndef INTERPRET_H
#define INTERPRET_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

a_subobject_path_ptr* last_subobject_path_link(a_constant_ptr  con);

void decay_subobject_path(a_constant_ptr  con);

a_subobject_path_ptr get_trailing_subobject_path_entry(
                                               a_constant_ptr  con,
                                               a_boolean       is_offset,
                                               a_boolean       is_base_class);

a_boolean interpret_clang_enable_if_opnd(
                                       an_expr_node_ptr              expr,
                                       Dyn_array<a_constant*> const  &params,
                                       a_source_position             *pos,
                                       a_boolean                     *p_value);

a_boolean is_core_constant_expr(an_expr_node_ptr  expr,
                                a_diag_list_ptr   diag_list);

a_boolean interpret_bool_assertion(an_expr_node_ptr  expr,
                                   a_constant_ptr    result_con,
                                   a_diag_list_ptr   diag_list);

a_boolean address_con_is_unknown_object(a_constant_ptr  con);

a_boolean interpret_expr(an_expr_node_ptr  expr,
                         a_boolean         is_constant_evaluated,
                         a_boolean         force_prvalue,
                         a_constant_ptr    result_con,
                         a_diag_list_ptr   diag_list);

a_routine_ptr get_constexpr_callee(an_expr_node_ptr  call_expr,
                                   a_diag_list_ptr   diag_list);

a_boolean interpret_constexpr_call(an_expr_node_ptr  call_expr,
                                   a_boolean         is_constant_evaluated,
                                   a_constant_ptr    result_con,
                                   a_diag_list_ptr   diag_list);

a_boolean interpret_dynamic_init_full(
                                  a_dynamic_init_ptr  dip,
                                  a_source_position   *pos,
                                  a_type_ptr          result_type,
                                  a_boolean           is_constant_evaluated,
                                  a_constant_ptr      result_con,
                                  a_diag_list_ptr     diag_list,
                                  a_boolean           allow_reinterpret_cast);

#define interpret_dynamic_init(dip, pos, result_type, is_constant_evaluated, \
                               result_con, diag_list)                        \
  (interpret_dynamic_init_full(dip, pos, result_type, is_constant_evaluated, \
                               result_con, diag_list, FALSE))

a_boolean interpret_constexpr_ctor(a_dynamic_init_ptr  dip,
                                   a_boolean           is_constant_evaluated,
                                   a_source_position   *pos,
                                   a_constant_ptr      result_con,
                                   a_diag_list_ptr     diag_list);

/*
The following macro (NS_scope_constexpr_intrinsics) describes functions in
namespace std and std::meta that the front end recognizes and attempts to
evaluate intrinsically.  The macro takes a macro M that should be replaced
by a macro of the form:

  #define MACRO(ns, name, signature)

Different parts of the front end invoke NS_scope_constexpr_intrinsics to
  1) define (here, in interpret.h) enumerator constants identifying the
     intrinsics by number
  2) define a table (in symbol_tbl.c) used to (a) mark associated symbol
     headers for efficient identification, and (b) build a Ptr_map to
     associate a "signature" to match function declarations with
  3) produce switch cases (in interpret.c) to handle evaluation dispatch

The "signature" is a string literal used as the third operand for the macro M.
This literal places some constraints on template arguments (optional), function
parameters (optional), and the return type (mandatory).  For example, if the
front end runs into a function std::construct_at, it will (a) recognize that
the symbol header for "construct_at" is marked as an intrinsic name, (b) look
up that header in the Ptr_map mentioned in (2) above (finding the string
literal "<T,>(*.,)*."), and (c) compare the declaration against the string as
follows:
  - "<T,>" indicates that a leading type argument is required (and the trailing
     comma indicates that additional unconstrained arguments are permitted)
  - "(*.,)" indicates that one or more parameter type are expected (again, the
     trailing comma indicates optional additional parameters); "*" represents
     "pointer to" and "." represents "any type"
  - The second "*." represents the return type ("pointer to any type")
The "type codes" currently recognized are:
  - ".": any type
  - "b": bool
  - "r": std::meta::info
  - "v": void
  - "C": a "char" type (plain, signed, or unsigned)
  - "G": any of the character types (i.e., also wchar_t and the charN_t types)
  - "I": an integral type
  - "Sv": std::string_view
  - "Sz": std::size_t
  - "Su": std::u8string_view
  - "So": std::strong_ordering
  - "Vr": std::vector<std::meta::info>
  - "L": std::source_location
  - "A": std::meta::access_context
  - "Mo": std::meta::member_offset
  - "*": pointer to the type kind described in the following code
  - "&": reference to the type kind described in the following code

If a new intrinsic is added with name intrin in the namespace identified with
ns (currently ns must be std or std_meta), a function do_constexpr_ns_intrin
must be defined in interpret.c to implement its evaluation.
*/
#define NS_scope_constexpr_intrinsics(M) \
  M(std, is_constant_evaluated, "()b") \
  M(std, construct_at, "<T,>(*.,)*.") \
  M(std, __report_constexpr_value, "(I)v|(*C)v|(*C,I)v") \
  M(std, is_string_literal, "(*G)b") \
  M(std_meta, identifier_of, "(r)Sv") \
  M(std_meta, u8identifier_of, "(r)Su") \
  M(std_meta, display_string_of, "(r)Sv") \
  M(std_meta, u8display_string_of, "(r)Su") \
  M(std_meta, source_location_of, "(r)L") \
  M(std_meta, members_of, "(r,A)Vr") \
  M(std_meta, current, "()A") \
  M(std_meta, current_function, "()r") \
  M(std_meta, current_class, "()r") \
  M(std_meta, current_namespace, "()r") \
  M(std_meta, static_data_members_of, "(r,A)Vr") \
  M(std_meta, nonstatic_data_members_of, "(r,A)Vr") \
  M(std_meta, bases_of, "(r,A)Vr") \
  M(std_meta, subobjects_of, "(r,A)Vr") \
  M(std_meta, enumerators_of, "(r)Vr") \
  M(std_meta, parameters_of, "(r)Vr") \
  M(std_meta, current_parameters, "()Vr") \
  M(std_meta, template_arguments_of, "(r)Vr") \
  M(std_meta, annotations_of, "(r)Vr") \
  M(std_meta, annotations_of_with_type, "(r,r)Vr") \
  M(std_meta, substitute, "<T>(r,&.)r") \
  M(std_meta, can_substitute, "<T>(r,&.)b") \
  M(std_meta, is_constructible_type, "<T>(r,&.)b") \
  M(std_meta, is_trivially_constructible_type, "<T>(r,&.)b") \
  M(std_meta, is_nothrow_constructible_type, "<T>(r,&.)b") \
  M(std_meta, is_invocable_type, "<T>(r,&.)b") \
  M(std_meta, is_invocable_r_type, "<T>(r,r,&.)b") \
  M(std_meta, is_nothrow_invocable_type, "<T>(r,&.)b") \
  M(std_meta, is_nothrow_invocable_r_type, "<T>(r,r,&.)b") \
  M(std_meta, common_type, "<T>(&.)r") \
  M(std_meta, common_reference, "<T>(&.)r") \
  M(std_meta, invoke_result, "<T>(r,&.)r") \
  M(std_meta, is_same_type, "(r,r)b") \
  M(std_meta, remove_cvref, "(r)r") \
  M(std_meta, reflect_constant, "<T>(.)r") \
  M(std_meta, reflect_object, "<T>(&.)r") \
  M(std_meta, reflect_function, "<T>(&.)r") \
  M(std_meta, reflect_constant_array, "<T>(&.)r") \
  M(std_meta, reflect_constant_string, "<T>(&.)r") \
  M(std_meta, extract, "<T>(r).") \
  M(std_meta, object_of, "(r)r") \
  M(std_meta, constant_of, "(r)r") \
  M(std_meta, is_token_sequence, "(r)b") \
  M(std_meta, is_empty_token_sequence, "(r)b") \
  M(std_meta, is_annotation, "(r)b") \
  M(std_meta, is_public, "(r)b") \
  M(std_meta, is_protected, "(r)b") \
  M(std_meta, is_private, "(r)b") \
  M(std_meta, is_accessible, "(r,A)b") \
  M(std_meta, has_inaccessible_nonstatic_data_members, "(r,A)b") \
  M(std_meta, has_inaccessible_bases, "(r,A)b") \
  M(std_meta, has_inaccessible_subobjects, "(r,A)b") \
  M(std_meta, is_virtual, "(r)b") \
  M(std_meta, is_pure_virtual, "(r)b") \
  M(std_meta, is_override, "(r)b") \
  M(std_meta, is_final, "(r)b") \
  M(std_meta, is_deleted, "(r)b") \
  M(std_meta, is_defaulted, "(r)b") \
  M(std_meta, is_user_provided, "(r)b") \
  M(std_meta, is_user_declared, "(r)b") \
  M(std_meta, is_explicit, "(r)b") \
  M(std_meta, is_noexcept, "(r)b") \
  M(std_meta, is_bit_field, "(r)b") \
  M(std_meta, is_enumerator, "(r)b") \
  M(std_meta, is_const, "(r)b") \
  M(std_meta, is_volatile, "(r)b") \
  M(std_meta, is_mutable_member, "(r)b") \
  M(std_meta, is_lvalue_reference_qualified, "(r)b") \
  M(std_meta, is_rvalue_reference_qualified, "(r)b") \
  M(std_meta, has_static_storage_duration, "(r)b") \
  M(std_meta, has_thread_storage_duration, "(r)b") \
  M(std_meta, has_automatic_storage_duration, "(r)b") \
  M(std_meta, has_internal_linkage, "(r)b") \
  M(std_meta, has_module_linkage, "(r)b") \
  M(std_meta, has_external_linkage, "(r)b") \
  M(std_meta, has_c_language_linkage, "(r)b") \
  M(std_meta, has_linkage, "(r)b") \
  M(std_meta, is_complete_type, "(r)b") \
  M(std_meta, is_enumerable_type, "(r)b") \
  M(std_meta, is_variable, "(r)b") \
  M(std_meta, is_type, "(r)b") \
  M(std_meta, is_namespace, "(r)b") \
  M(std_meta, is_type_alias, "(r)b") \
  M(std_meta, is_namespace_alias, "(r)b") \
  M(std_meta, is_function, "(r)b") \
  M(std_meta, is_conversion_function, "(r)b") \
  M(std_meta, is_operator_function, "(r)b") \
  M(std_meta, is_literal_operator, "(r)b") \
  M(std_meta, is_special_member_function, "(r)b") \
  M(std_meta, is_constructor, "(r)b") \
  M(std_meta, is_default_constructor, "(r)b") \
  M(std_meta, is_copy_constructor, "(r)b") \
  M(std_meta, is_move_constructor, "(r)b") \
  M(std_meta, is_assignment, "(r)b") \
  M(std_meta, is_copy_assignment, "(r)b") \
  M(std_meta, is_move_assignment, "(r)b") \
  M(std_meta, is_destructor, "(r)b") \
  M(std_meta, is_function_parameter, "(r)b") \
  M(std_meta, is_explicit_object_parameter, "(r)b") \
  M(std_meta, has_default_argument, "(r)b") \
  M(std_meta, is_vararg_function, "(r)b") \
  M(std_meta, is_template, "(r)b") \
  M(std_meta, is_function_template, "(r)b") \
  M(std_meta, is_variable_template, "(r)b") \
  M(std_meta, is_class_template, "(r)b") \
  M(std_meta, is_alias_template, "(r)b") \
  M(std_meta, is_conversion_function_template, "(r)b") \
  M(std_meta, is_operator_function_template, "(r)b") \
  M(std_meta, is_literal_operator_template, "(r)b") \
  M(std_meta, is_constructor_template, "(r)b") \
  M(std_meta, is_concept, "(r)b") \
  M(std_meta, is_value, "(r)b") \
  M(std_meta, is_object, "(r)b") \
  M(std_meta, is_structured_binding, "(r)b") \
  M(std_meta, is_class_member, "(r)b") \
  M(std_meta, is_namespace_member, "(r)b") \
  M(std_meta, is_nonstatic_data_member, "(r)b") \
  M(std_meta, is_static_member, "(r)b") \
  M(std_meta, is_base, "(r)b") \
  M(std_meta, has_default_member_initializer, "(r)b") \
  M(std_meta, has_parent, "(r)b") \
  M(std_meta, has_template_arguments, "(r)b") \
  M(std_meta, has_identifier, "(r)b") \
  M(std_meta, dealias, "(r)r") \
  M(std_meta, template_of, "(r)r") \
  M(std_meta, type_of, "(r)r") \
  M(std_meta, variable_of, "(r)r") \
  M(std_meta, return_type_of, "(r)r") \
  M(std_meta, parent_of, "(r)r") \
  M(std_meta, size_of, "(r)Sz") \
  M(std_meta, offset_of, "(r)Mo") \
  M(std_meta, bit_size_of, "(r)Sz") \
  M(std_meta, alignment_of, "(r)Sz") \
  M(std_meta, reflect_invoke, "<T>(r,&.)r") \
  M(std_meta, __report_tokens, "(r)v") \
  M(std_meta, queue_injection, "(r,r)v") \
  M(std_meta, namespace_inject, "(r,r)v") \
  M(std_meta, nearest_token_queuing_context, "()r") \
  M(std_meta, nearest_class_or_namespace, "()r") \
  M(std_meta, nearest_namespace, "()r") \
  M(std_meta, tuple_size, "(r)Sz") \
  M(std_meta, tuple_element, "(Sz,r)r") \
  M(std_meta, is_void_type, "(r)b") \
  M(std_meta, is_null_pointer_type, "(r)b") \
  M(std_meta, is_integral_type, "(r)b") \
  M(std_meta, is_floating_point_type, "(r)b") \
  M(std_meta, is_array_type, "(r)b") \
  M(std_meta, is_pointer_type, "(r)b") \
  M(std_meta, is_lvalue_reference_type, "(r)b") \
  M(std_meta, is_rvalue_reference_type, "(r)b") \
  M(std_meta, is_member_object_pointer_type, "(r)b") \
  M(std_meta, is_member_function_pointer_type, "(r)b") \
  M(std_meta, is_enum_type, "(r)b") \
  M(std_meta, is_union_type, "(r)b") \
  M(std_meta, is_class_type, "(r)b") \
  M(std_meta, is_function_type, "(r)b") \
  M(std_meta, is_reflection_type, "(r)b") \
  M(std_meta, is_reference_type, "(r)b") \
  M(std_meta, is_arithmetic_type, "(r)b") \
  M(std_meta, is_fundamental_type, "(r)b") \
  M(std_meta, is_object_type, "(r)b") \
  M(std_meta, is_scalar_type, "(r)b") \
  M(std_meta, is_compound_type, "(r)b") \
  M(std_meta, is_member_pointer_type, "(r)b") \
  M(std_meta, is_const_type, "(r)b") \
  M(std_meta, is_volatile_type, "(r)b") \
  M(std_meta, is_trivially_copyable_type, "(r)b") \
  M(std_meta, is_standard_layout_type, "(r)b") \
  M(std_meta, is_empty_type, "(r)b") \
  M(std_meta, is_polymorphic_type, "(r)b") \
  M(std_meta, is_abstract_type, "(r)b") \
  M(std_meta, is_final_type, "(r)b") \
  M(std_meta, is_aggregate_type, "(r)b") \
  M(std_meta, is_signed_type, "(r)b") \
  M(std_meta, is_unsigned_type, "(r)b") \
  M(std_meta, is_bounded_array_type, "(r)b") \
  M(std_meta, is_unbounded_array_type, "(r)b") \
  M(std_meta, is_scoped_enum_type, "(r)b") \
  M(std_meta, remove_const, "(r)r") \
  M(std_meta, remove_volatile, "(r)r") \
  M(std_meta, remove_cv, "(r)r") \
  M(std_meta, add_const, "(r)r") \
  M(std_meta, add_volatile, "(r)r") \
  M(std_meta, add_cv, "(r)r") \
  M(std_meta, remove_reference, "(r)r") \
  M(std_meta, add_lvalue_reference, "(r)r") \
  M(std_meta, add_rvalue_reference, "(r)r") \
  M(std_meta, make_signed, "(r)r") \
  M(std_meta, make_unsigned, "(r)r") \
  M(std_meta, remove_extent, "(r)r") \
  M(std_meta, remove_all_extents, "(r)r") \
  M(std_meta, remove_pointer, "(r)r") \
  M(std_meta, add_pointer, "(r)r") \
  M(std_meta, decay, "(r)r") \
  M(std_meta, underlying_type, "(r)r") \
  M(std_meta, is_structural_type, "(r)b") \
  M(std_meta, is_default_constructible_type, "(r)b") \
  M(std_meta, is_copy_constructible_type, "(r)b") \
  M(std_meta, is_move_constructible_type, "(r)b") \
  M(std_meta, is_assignable_type, "(r,r)b") \
  M(std_meta, is_copy_assignable_type, "(r)b") \
  M(std_meta, is_move_assignable_type, "(r)b") \
  M(std_meta, is_destructible_type, "(r)b") \
  M(std_meta, is_trivially_default_constructible_type, "(r)b") \
  M(std_meta, is_trivially_copy_constructible_type, "(r)b") \
  M(std_meta, is_trivially_move_constructible_type, "(r)b") \
  M(std_meta, is_trivially_assignable_type, "(r,r)b") \
  M(std_meta, is_trivially_copy_assignable_type, "(r)b") \
  M(std_meta, is_trivially_move_assignable_type, "(r)b") \
  M(std_meta, is_trivially_destructible_type, "(r)b") \
  M(std_meta, is_nothrow_default_constructible_type, "(r)b") \
  M(std_meta, is_nothrow_copy_constructible_type, "(r)b") \
  M(std_meta, is_nothrow_move_constructible_type, "(r)b") \
  M(std_meta, is_nothrow_assignable_type, "(r,r)b") \
  M(std_meta, is_nothrow_copy_assignable_type, "(r)b") \
  M(std_meta, is_nothrow_move_assignable_type, "(r)b") \
  M(std_meta, is_nothrow_destructible_type, "(r)b") \
  M(std_meta, is_implicit_lifetime_type, "(r)b") \
  M(std_meta, has_virtual_destructor, "(r)b") \
  M(std_meta, has_unique_object_representations, "(r)b") \
  M(std_meta, reference_constructs_from_temporary, "(r,r)b") \
  M(std_meta, reference_converts_from_temporary, "(r,r)b") \
  M(std_meta, rank, "(r)Sz") \
  M(std_meta, extent, "(r,I)Sz") \
  M(std_meta, is_base_of_type, "(r,r)b") \
  M(std_meta, is_virtual_base_of_type, "(r,r)b") \
  M(std_meta, is_convertible_type, "(r,r)b") \
  M(std_meta, is_nothrow_convertible_type, "(r,r)b") \
  M(std_meta, is_layout_compatible_type, "(r,r)b") \
  M(std_meta, type_order, "(r,r)So") \
  M(std_meta, is_swappable_type, "(r)b") \
  M(std_meta, is_nothrow_swappable_type, "(r)b") \
  M(std_meta, is_swappable_with_type, "(r,r)b") \
  M(std_meta, is_nothrow_swappable_with_type, "(r,r)b") \
  M(std_meta, is_pointer_interconvertible_base_of_type, "(r,r)b") \
  M(std_meta, unwrap_reference, "(r)r") \
  M(std_meta, unwrap_ref_decay, "(r)r") \
  M(std_meta, variant_size, "(r)Sz") \
  M(std_meta, variant_alternative, "(Sz,r)r") \
  M(std_meta, operator_of, "(r).") \
  M(std_meta, symbol_of, "(.)Sv") \
  M(std_meta, u8symbol_of, "(.)Su") \
  M(std_meta, data_member_spec, "(r,.)r") \
  M(std_meta, is_data_member_spec, "(r)b") \
  M(std_meta, define_aggregate, "<T>(r,&.)r") \
  /* End of NS_scope_constexpr_intrinsics. */


enum a_constexpr_intrinsic {
  cit_error,
  cit_std_allocator_allocate,
  cit_std_allocator_deallocate,
#define CIT_name(ns, name, signature)  cit_##ns##_##name,
  NS_scope_constexpr_intrinsics(CIT_name)
#undef CIT_name
  cit_last
};

void register_constexpr_intrinsic(a_constexpr_intrinsic  tag,
                                  a_routine_ptr          rp);

a_boolean constexpr_diag_tag_is_valid(a_const_char   *tag,
                                      a_targ_size_t  tag_len);


#if DEBUG
/* GCOVR_EXCL_START */
uintptr_t db_hash_ptr(void  *ptr);

a_host_large_integer db_int_val(a_byte  *val_bytes);

void db_complete_object(a_byte  *addr);

void db_call_stack(void  *ips);

a_byte* db_stack_storage(void  *ptr,
                         void  *ips);

void db_data_map(void  *map_ptr);

void db_live_set(void  *interpreter_state);

unsigned long db_show_interpret_fe_space_used(unsigned long  grand_total);

#if TRACK_INTERPRETER_ALLOCATIONS
unsigned long db_object_alloc_num(a_byte  *ptr);
#endif /* TRACK_INTERPRETER_ALLOCATIONS */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

void clean_up_interpreter(void);

void interpret_trans_unit_init(void);

void interpret_init(void);

void interpret_one_time_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef INTERPRET_H */


