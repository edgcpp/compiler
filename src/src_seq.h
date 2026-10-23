/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

src_seq.h -- Declarations for support for source sequence list management

*/

/* Avoid including these declarations more than once. */
#ifndef SRC_SEQ_H
#define SRC_SEQ_H

#if GENERATE_SOURCE_SEQUENCE_LISTS

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Macro to extract the kind from a source sequence entry or secondary
declaration entry.
*/
#define ss_entry_kind(ssep) ((an_il_entry_kind)(ssep)->entity.kind)

/*
Macro to extract the pointer from a source sequence entry or secondary
declaration entry.  It is cast to the indicated pointer type.
*/
#define ss_entry_ptr(ssep, type) ((type)(ssep)->entity.ptr)

/*
ssep points to an iek_src_seq_sublist source sequence entry.  Such an entry
resides on the function-scope source sequence list but points to a header
for a sublist of file-scope source sequence entries.  Fetch and return a
pointer to the sublist header.
*/
#define assoc_sublist_of(ssep) ss_entry_ptr((ssep), a_src_seq_sublist_ptr)

#if DEBUG
/* GCOVR_EXCL_START */
extern void db_source_sequence_entry(a_source_sequence_entry_ptr  ssep);
extern void db_ss_list(a_source_sequence_entry_ptr  ssep);
extern void db_ss_list_for_scope_depth(a_scope_depth  depth);
extern void db_ss_list_for_scope(a_scope_ptr  sp);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

extern void fixup_function_scope_source_sequence_list(a_scope_ptr  sp);

#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
void move_sses_out_of_class_if_otherwise_invalid(a_type_ptr      class_type,
                                                 a_template_ptr  templ_entry);
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */

extern void add_source_sequence_entry_to_list(
                                       a_source_sequence_entry_ptr new_ssep);

extern void f_update_source_sequence_list(char                    *entity_ptr,
                                          an_il_entry_kind        kind,
                                          a_source_sequence_entry *old_ssep);

/* Macro interface to f_update_source_sequence_list that enforces the
   precondition that source sequence entries be allowed. */
#define update_source_sequence_list(entity_ptr, kind, old_ssep)          \
{ if (!source_sequence_entries_disallowed) {                             \
    f_update_source_sequence_list((entity_ptr), (kind), (old_ssep));     \
  }  /* if */                                                            \
}  /* update_source_sequence_list */

/* Macro interface to f_update_source_sequence_list when a new entry is to
   be added to the list. */
#define add_to_source_sequence_list(entity_ptr, kind)                    \
{ if (!source_sequence_entries_disallowed) {                             \
    f_update_source_sequence_list((entity_ptr), (kind),                  \
                                  (a_source_sequence_entry_ptr)NULL);    \
  }  /* if */                                                            \
}  /* add_to_source_sequence_list */


extern a_source_sequence_entry_ptr add_empty_source_sequence_entry(void);

extern void add_end_of_construct_source_sequence_entry(char             *ptr,
                                                       an_il_entry_kind kind);

extern a_source_sequence_entry_ptr matching_end_of_construct(
                                           a_source_sequence_entry_ptr  head);

extern void insert_src_seq_list(a_source_sequence_entry_ptr  head,
                                a_source_sequence_entry_ptr  tail,
                                a_scope_depth                scope_depth,
                                a_source_sequence_entry_ptr  insert_point);

extern a_src_seq_secondary_decl_ptr make_source_sequence_secondary_decl(
                                            char               *ptr,
                                            an_il_entry_kind   kind,
                                            a_type_ptr         declared_type);

extern void move_src_seq_entry(a_source_sequence_entry_ptr  ssep,
                               a_scope_depth                source_depth,
                               a_source_sequence_entry_ptr  insert_point,
                               a_scope_depth                target_depth);

#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS

extern void reset_ss_list_instantiation_insert_point(void);

extern a_scope_depth scope_depth_for_class_ss_list(a_type_ptr  class_type);

extern void add_source_sequence_entry_for_partial_instantiation(
                                            char               *ptr,
                                            an_il_entry_kind   kind,
                                            a_type_ptr         declared_type);

extern void update_classes_in_ss_list(a_scope_stack_entry_ptr  src_ssep,
                                      a_scope_stack_entry_ptr  dst_ssep);

extern void insert_instantiation_src_seq_list(
                                    a_scope_stack_entry_ptr  scope_stack_ptr);

#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS

extern void check_for_and_remove_redundant_secondary_decl_ss_entry(
                                                       a_type_ptr class_type);

#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */

extern void f_remove_from_src_seq_list(a_source_sequence_entry_ptr ssep,
                                       a_scope_depth               depth);

#define remove_from_src_seq_list(ssep)                                 \
  f_remove_from_src_seq_list((ssep), depth_scope_stack)

extern void remove_src_seq_entry(a_source_sequence_entry_ptr  ssep);

#if GENERATE_SOURCE_SEQUENCE_LISTS

extern void remove_src_seq_list(a_source_sequence_entry_ptr  head,
                                a_source_sequence_entry_ptr  tail);

extern void clear_src_seq_list_segment(a_source_sequence_entry_ptr  ss_start,
                                       a_source_sequence_entry_ptr  ss_end);

extern void prune_src_seq_list(a_source_sequence_entry_ptr  *head,
                               a_source_sequence_entry_ptr  *tail);

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern a_source_sequence_entry_ptr last_matching_source_sequence_entry(
                                                               char *entity);

/* Bit vector used to pass flags into update_src_seq_secondary_decl.   Each
   Each bit represents a flag. */
typedef unsigned int an_sssd_flag_set;

/* Constants defining bits in the bit vector used in calls to
   set_src_seq_secondary_decl_fields. */
#define SSSD_NO_FLAGS ((an_sssd_flag_set)0x0)
#define SSSD_AUTONOMOUS_TAG_DECL ((an_sssd_flag_set)0x1)
			/* If this bit is set, set autonomous_tag_decl in the
			   secondary-decl entry. */
#define SSSD_FRIEND_DECL ((an_sssd_flag_set)0x2)
			/* If this bit is set, set friend_decl in the
			   secondary-decl entry. */
#define SSSD_DECLARED_IN_FUNC_PROTOTYPE ((an_sssd_flag_set)0x4)
			/* If this bit is set, set declared_in_func_prototype
			   in the secondary-decl entry. */
#define SSSD_SPECIALIZED_WITH_NEW_SYNTAX ((an_sssd_flag_set)0x08)
			/* If this bit is set, set specialized_with_new_syntax
			   in the secondary-decl entry. */
#define SSSD_FIRST_DECLARATION ((an_sssd_flag_set)0x10)
			/* If this bit is set, set first_declaration in the
			   secondary-decl entry. */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#define SSSD_ORIGINALLY_NONAUTONOMOUS_DEFINITION ((an_sssd_flag_set)0x20)
			/* If this bit is set, set
			   originally_nonautonomous_definition in the
			   secondary-decl entry. */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#define SSSD_MARKED_AS_GNU_EXTENSION ((an_sssd_flag_set)0x40)
			/* If this bit it set, set marked_as_gnu_extension in
			   the secondary-decl entry. */

extern a_src_seq_secondary_decl_ptr set_src_seq_secondary_decl_fields(
                                           char                  *il_entry_ptr,
                                           a_type_ptr            declared_type,
                                           a_name_reference_ptr  name_ref,
                                           an_sssd_flag_set      flags);

extern a_src_seq_secondary_decl_ptr update_src_seq_secondary_decl(
                                        char                  *il_entry_ptr,
                                        a_type_ptr            declared_type,
                                        a_name_reference_ptr  name_ref,
                                        an_sssd_flag_set      flags,
                                        a_decl_pos_block_ptr  decl_pos_block);

extern a_type_ptr type_from_src_seq_declaration(
                                             a_source_sequence_entry_ptr ssep);

extern
void eliminate_variable_definition_source_sequence_entry(a_variable_ptr  vp);

extern void turn_routine_primary_sse_into_secondary_sse(a_routine_ptr  rp);

extern void eliminate_function_body_source_sequence_entries(a_scope_ptr sp);

#if MAINTAIN_NEEDED_FLAGS

extern void eliminate_class_body_source_sequence_entries(a_type_ptr tp);

extern void eliminate_unneeded_source_sequence_entries(a_scope_ptr sp);

#endif /* MAINTAIN_NEEDED_FLAGS */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#endif /* ifndef SRC_SEQ_H */

