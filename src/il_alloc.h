/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

il_alloc.h -- Declarations related to allocation of intermediate language
              entries.

*/

/* Avoid including these declarations more than once. */
#ifndef IL_ALLOC_H
#define IL_ALLOC_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
List of released local constants for reuse by local_constant.
*/
EXTERN_THREAD a_constant_ptr
                available_local_constants;

/*
List of released file-scope expression nodes for reuse by alloc_expr_node.
*/
EXTERN_THREAD an_expr_node_ptr
                avail_fs_nodes;

/*
Scratch variable for use by macro versions of some local constant routines
in some configurations.
*/
EXTERN_THREAD a_constant_ptr
                temp_for_local_constant;

/* Most IL allocation facilities are not needed in a standalone utility
   program. */
#if !STANDALONE_UTILITY_PROGRAM

extern char *alloc_il(sizeof_t size);

extern char *alloc_primary_file_scope_il(sizeof_t size);

#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
a_scope_orphaned_list_header_ptr alloc_scope_orphaned_list_header(
                                                 a_routine_ptr   assoc_routine,
                                                 a_scope_number  scope_number);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

extern a_source_file_ptr alloc_source_file(void);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_cli_metadata_file_ptr alloc_cli_metadata_file(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if ONE_INSTANTIATION_PER_OBJECT
extern a_per_instantiation_needed_flags_entry_ptr
           alloc_per_instantiation_needed_flags_entry(a_boolean at_file_scope);
#endif /* ONE_INSTANTIATION_PER_OBJECT */

extern a_subobject_path_ptr alloc_subobject_path(void);

extern void set_template_param_constant_kind(
                                      a_constant                     *cp,
                                      a_template_param_constant_kind kind);

extern void set_constant_kind(a_constant           *cp,
                              a_constant_repr_kind kind);

extern void clear_constant(a_constant           *cp,
                           a_constant_repr_kind kind);

extern a_constant_ptr alloc_constant(a_constant_repr_kind kind);

extern a_constant_ptr fs_constant(a_constant_repr_kind kind);

extern a_constant_ptr local_constant(void);

extern void release_local_constant(a_constant_ptr *cpp);

#if CHECKING
extern void check_local_constant_use(void);
#endif /* CHECKING */

extern a_constant_ptr move_local_constant_to_il(a_constant_ptr *cp);

extern a_param_type_ptr alloc_param_type(a_type_ptr type);

extern void free_param_type_list(a_param_type_ptr  ptp);

extern a_derivation_step_ptr alloc_derivation_step(void);

extern a_base_class_derivation_ptr alloc_base_class_derivation(void);

#if DO_IL_LOWERING && IA64_ABI
extern a_vcall_offset_entry_ptr alloc_vcall_offset_entry(void);
#endif /* DO_IL_LOWERING && IA64_ABI */

extern an_overriding_virtual_function_ptr
                                       alloc_overriding_virtual_function(void);

extern a_template_arg_ptr alloc_template_arg(a_templ_arg_kind	kind);

extern void free_template_arg_list(a_template_arg_ptr  tap);


template<>
INLINE void delete_fe(a_template_arg **tap)
/*
This is a specialization of delete_fe that frees template arguments back to the
special template argument list recycling logic.
*/
{
  if (*tap != NULL) {
    free_template_arg_list(*tap);
    *tap = NULL;
  }  /* if */
}  /* delete_fe */


using an_owned_template_arg_list = Owning_ptr<a_template_arg>;
                        /* The type used for a template argument list that
                           should be freed when no longer in used. */

extern a_base_class_ptr alloc_base_class(void);

extern a_class_list_entry_ptr alloc_list_entry_for_class_full(
                                                 a_source_correspondence *scp);

extern a_class_list_entry_ptr alloc_list_entry_for_class(void);

extern a_routine_list_entry_ptr alloc_list_entry_for_routine(void);

extern a_variable_list_entry_ptr alloc_list_entry_for_variable(void);

#if NEED_NAME_MANGLING
extern a_constant_list_entry_ptr alloc_list_entry_for_constant(void);

extern void free_list_of_constant_list_entries(a_constant_list_entry_ptr list);
#endif /* NEED_NAME_MANGLING */

extern a_based_type_list_member_ptr alloc_based_type_list_member(
                                               a_based_type_kind  kind,
                                               a_type_ptr         base_type);

extern void clear_class_type_definition_fields(a_type_ptr  class_type);

extern void clear_class_type_supplement(a_class_type_supplement_ptr  ctsp);

extern void set_type_kind(a_type_ptr  pte,
                          a_type_kind kind);

extern void clear_type_cached_flags(a_type_ptr  pte);

extern void clear_type(a_type_ptr  pte,
                       a_type_kind kind);

extern a_type_ptr alloc_type(a_type_kind kind);

extern void set_dynamic_init_kind(a_dynamic_init_ptr  dip,
                                  a_dynamic_init_kind kind);

extern a_dynamic_init_ptr alloc_dynamic_init(a_dynamic_init_kind kind);

extern a_local_static_variable_init_ptr alloc_local_static_variable_init(void);

extern a_vla_dimension_ptr alloc_vla_dimension(void);

extern void clear_variable(a_variable_ptr vp);

extern a_variable_template_info_ptr alloc_variable_template_info(void);

extern a_variable_ptr alloc_variable(a_storage_class  storage_class);

extern a_field_ptr alloc_field(void);

extern an_exception_specification_ptr alloc_exception_specification(void);

extern an_exception_specification_type_ptr
                                  alloc_exception_specification_type(void);

extern void set_routine_special_kind(a_routine_ptr           rp,
                                     a_special_function_kind special_kind);

extern a_routine_ptr alloc_routine(void);

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
extern a_gnu_routine_supplement_ptr alloc_gnu_supplement_for_routine(
                                                             a_routine_ptr rp);
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */

extern an_asm_entry_ptr alloc_asm_entry(void);

#if ASM_SUPPORT_NEEDED
extern char *alloc_asm_function_body(sizeof_t  len);
#endif /* ASM_SUPPORT_NEEDED */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern an_ms_attribute_ptr alloc_ms_attribute(an_ms_attribute_kind kind);

extern
an_ms_attribute_arg_ptr alloc_ms_attribute_arg(an_ms_attribute_arg_kind	kind);

extern a_custom_ms_attribute_arg_ptr alloc_custom_ms_attribute_arg(void);

extern a_property_index_type_ptr alloc_property_index_type(void);

extern a_property_or_event_descr_ptr alloc_property_or_event_descr(
                                              a_property_or_event_kind  kind);

extern a_generic_constraint_ptr alloc_generic_constraint(void);

extern
void clear_generic_constraint_clause(a_generic_constraint_clause_ptr gccp);

extern a_generic_constraint_clause_ptr alloc_generic_constraint_clause(void);

extern an_event_interface_ptr alloc_event_interface(void);

extern a_partial_class_body_ptr alloc_partial_class_body(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
extern an_ms_if_exists_ptr alloc_ms_if_exists(void);
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

extern a_lambda_ptr alloc_lambda(void);

extern a_lambda_capture_ptr alloc_lambda_capture(void);

extern a_seq_number_lookup_entry_ptr alloc_seq_number_lookup_entry(void);

#if GNU_EXTENSIONS_ALLOWED
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
extern an_asm_operand_constraint_ptr alloc_asm_operand_constraint(
                                            an_asm_operand_constraint_kind ck);
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */

extern an_asm_operand_ptr alloc_asm_operand(void);

extern a_named_register_list_ptr alloc_named_register_list(void);

extern a_label_list_ptr alloc_label_list(void);
#endif /* GNU_EXTENSIONS_ALLOWED */

extern a_label_ptr alloc_label(void);

extern a_token_sequence_entry* alloc_token_sequence_entry(void);

extern a_token_sequence* alloc_token_sequence(void);

extern void set_expr_node_kind(an_expr_node_ptr  node,
                               an_expr_node_kind kind);

extern void clear_expr_node(an_expr_node_ptr  node,
                            an_expr_node_kind kind);

extern a_local_expr_node_ref_ptr alloc_local_expr_node_ref(void);

extern an_expr_node_ptr alloc_expr_node(an_expr_node_kind node_kind);

extern an_expr_node_ptr fs_alloc_expr_node(an_expr_node_kind node_kind);

inline void mark_fs_node_reclaimed(an_expr_node_ptr node)
/*
It's been determined by the caller that the given node can be made
available for reuse.  Update the avail_fs_nodes list to include the given
node.
*/
{
  node->kind = enk_reclaimed;
  node->extra.next_avail = avail_fs_nodes;
  avail_fs_nodes = node;
}  /* mark_fs_node_reclaimed */


#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
extern void set_lowered_eh_construct_node_kind(
                                             an_expr_node_ptr node,
                                             a_lowered_eh_construct_kind kind);

extern an_expr_node_ptr alloc_lowered_eh_construct_node(
                                             a_lowered_eh_construct_kind kind);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void set_for_each_loop_kind(a_for_each_loop_ptr     felp,
                                   a_for_each_pattern_kind kind);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_switch_case_entry_ptr alloc_switch_case_entry(void);

#if !ABI_CHANGES_FOR_RTTI
extern an_accessible_base_class_ptr alloc_accessible_base_class(
                                                         a_base_class_ptr bcp);
#endif /* !ABI_CHANGES_FOR_RTTI */

extern a_handler_ptr alloc_handler(void);

extern a_coroutine_descr_ptr alloc_coroutine_descr(void);

extern void set_statement_kind(a_statement_ptr  sp,
                               a_statement_kind kind);

extern a_statement_ptr alloc_statement(a_statement_kind stmt_kind,
                                       a_boolean        compiler_generated);

extern a_constructor_init_ptr alloc_ctor_init(a_constructor_init_kind  kind);

#if GNU_EXTENSIONS_ALLOWED
void clear_gcc_pragma_descr(a_gcc_pragma_descr  *gpd);
#endif /* GNU_EXTENSIONS_ALLOWED */

extern a_pragma_ptr alloc_pragma(a_pragma_kind kind);

extern an_object_lifetime_ptr alloc_object_lifetime(
                                               an_object_lifetime_kind  kind);

extern void set_scope_kind(a_scope_ptr    sp,
                           a_scope_kind   kind,
                           a_routine_ptr  assoc_routine);

extern void  clear_namespace(a_namespace_ptr nsp,
                             a_boolean       is_alias);

extern a_namespace_ptr alloc_namespace(a_boolean  is_alias);

extern a_using_decl_ptr alloc_using_decl(void);

extern a_scope_ptr alloc_scope(a_scope_kind   kind,
                               a_scope_number number,
                               a_routine_ptr  assoc_routine);

extern a_scope_ptr alloc_placeholder_scope(a_scope_kind  kind,
                                           a_routine_ptr assoc_routine);

extern a_local_scope_ref_ptr alloc_local_scope_ref(void);

extern a_static_assertion_ptr alloc_static_assertion(void);

#if GENERATE_SOURCE_SEQUENCE_LISTS

extern a_source_sequence_entry_ptr alloc_source_sequence_entry(void);

extern void recycle_src_seq_entry(a_source_sequence_entry_ptr  ssep);

extern a_src_seq_secondary_decl_ptr alloc_src_seq_secondary_decl(void);

extern a_src_seq_end_of_construct_ptr alloc_src_seq_end_of_construct(void);

extern a_src_seq_sublist_ptr alloc_src_seq_sublist(void);

extern an_instantiation_directive_ptr alloc_instantiation_directive(void);


template<>
INLINE void delete_fe(a_source_sequence_entry **elem_ptr)
/*
This is a specialization of delete_fe that frees a source sequence entry back
to the special source sequence entry recycling logic.
*/
{
  if (*elem_ptr != NULL) {
    recycle_src_seq_entry(*elem_ptr);
    *elem_ptr = NULL;
  }  /* if */
}  /* delete_fe */


#if GENERATE_LINKAGE_SPEC_BLOCKS
extern a_linkage_spec_block_ptr alloc_linkage_spec_block(void);
#endif /* GENERATE_LINKAGE_SPEC_BLOCKS */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if RECORD_HIDDEN_NAMES_IN_IL
extern a_hidden_name_ptr alloc_hidden_name(void);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */

extern a_template_parameter_ptr alloc_template_parameter(void);

extern a_requires_clause_ptr alloc_requires_clause(void);

extern a_template_decl_ptr alloc_template_decl(void);

extern a_template_ptr alloc_template(void);

#if RECORD_MACROS_IN_IL
extern a_macro_ptr alloc_macro(void);
#endif /* RECORD_MACROS_IN_IL */

#if RECORD_MACRO_INVOCATIONS
extern a_macro_invocation_record_block_ptr alloc_macro_invocation_record_block(
                                                                         void);
#endif /* RECORD_MACRO_INVOCATIONS */

extern an_element_position_ptr alloc_element_position(void);

#if EXTRA_SOURCE_POSITIONS_IN_IL

extern void clear_decl_position_supplement(a_decl_position_supplement *dpsp);

extern a_decl_position_supplement_ptr alloc_decl_position_supplement
                                                  (a_boolean  at_file_scope);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

extern a_name_qualifier_ptr alloc_name_qualifier(void);
extern a_name_reference_ptr alloc_name_reference(void);
extern void clear_name_reference(a_name_reference_ptr	nrp);

#if !STANDALONE_UTILITY_PROGRAM

extern char *copy_string_to_region(a_memory_region_number region,
                                   a_const_char           *string);

extern char *copy_string_of_length_to_region(
				      a_memory_region_number region,
				      a_const_char           *string,
				      sizeof_t		     length);

extern char *alloc_text_of_string_literal(sizeof_t size);

#endif /* !STANDALONE_UTILITY_PROGRAM */

extern an_il_entity_list_entry_ptr alloc_il_entity_list_entry_with(
                                                 a_source_correspondence *scp);

extern an_il_entity_list_entry_ptr alloc_il_entity_list_entry(void);

extern an_attribute_ptr alloc_attribute(void);

extern an_attribute_arg_ptr alloc_attribute_arg(void);

extern an_attribute_group_ptr alloc_attribute_group(void);

extern a_module_ptr alloc_module(a_module_kind kind);

extern a_module_import_decl_ptr alloc_module_import_decl(void);

extern a_scoped_expression_ptr alloc_scoped_expression(void);

#if DEBUG
unsigned long show_il_alloc_space_used(unsigned long grand_total);
#endif /* DEBUG */

extern void il_alloc_one_time_init(void);

extern void compute_il_prefix_size(void);

extern void il_alloc_trans_unit_init(void);

extern void il_alloc_init(void);

#ifdef TRACE_ALLOC
void trace_alloc_check(void *ptr);
#endif /* TRACE_ALLOC */

#if !STANDALONE_UTILITY_PROGRAM && DEBUG
void f_tally_alloc(an_il_entry_kind kind);
#define tally_alloc(type) f_tally_alloc(type_to_il_entry_kind<type>())
#endif /* !STANDALONE_UTILITY_PROGRAM && DEBUG */

/*
Macro that allocates an entry for the specified type in the file scope
memory region.
*/
#if !STANDALONE_UTILITY_PROGRAM && DEBUG
#define alloc_il_of_type(type) \
  (tally_alloc(type), (type*)alloc_il(sizeof(type)))
#else /* !(!STANDALONE_UTILITY_PROGRAM && DEBUG) */
#define alloc_il_of_type(type) \
  ((type*)alloc_il(sizeof(type)))
#endif /* !STANDALONE_UTILITY_PROGRAM && DEBUG */

/*
Macro that allocates an entry for the specified type in the current
memory region.
*/
#if !STANDALONE_UTILITY_PROGRAM && DEBUG
#define alloc_cil_of_type(type) \
  (tally_alloc(type), (type*)alloc_cil(sizeof(type)))
#else /* !(!STANDALONE_UTILITY_PROGRAM && DEBUG) */
#define alloc_cil_of_type(type) \
  ((type*)alloc_cil(sizeof(type)))
#endif /* !STANDALONE_UTILITY_PROGRAM && DEBUG */

#else /* STANDALONE_UTILITY_PROGRAM */

/* Provide stubs for local_constant and release_local_constant for use in
   standalone utility programs. */

#define local_constant()                                         \
  ((available_local_constants == NULL) ?                         \
   (a_constant_ptr)malloc(sizeof(a_constant)) :                  \
   (temp_for_local_constant = available_local_constants,         \
    available_local_constants = available_local_constants->next, \
    temp_for_local_constant))

#define release_local_constant(cp)          \
  ((*cp)->next = available_local_constants, \
   available_local_constants = *cp,         \
   *cp = NULL)

/* A standalone C++-generating back end uses clear_expr_node. */
#if BACK_END_IS_CP_GEN_BE || BACK_END_IS_GCC_GEN_BE

extern void clear_expr_node(an_expr_node_ptr  node,
                            an_expr_node_kind kind);

#endif /* BACK_END_IS_CP_GEN_BE || BACK_END_IS_GCC_GEN_BE */
#endif /* !STANDALONE_UTILITY_PROGRAM */

#define clear_tagged_ptr(tagged_ptr)                                        \
  (((tagged_ptr).kind = iek_none), ((tagged_ptr).ptr = NULL))

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef IL_ALLOC_H */

