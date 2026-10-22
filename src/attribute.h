/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

attribute.h -- Declarations related to attribute.c (having to do with 
               attributes, a GCC C extension).

*/

/* Avoid including these declarations more than once: */
#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if DEBUG
/* GCOVR_EXCL_START */
extern void db_attribute(an_attribute_ptr  ap);

extern void db_attribute_list(an_attribute_ptr  ap);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

#if REDEFINE_EXTNAME_PRAGMA_ENABLED
extern void redefine_extname_pragma(a_pending_pragma_ptr  ppp);
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
extern void process_alias_fixup_list(a_boolean  early_attr_resolution);

extern unsigned long show_attribute_space_used(void);
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */

#if GNU_EXTENSIONS_ALLOWED

extern 
a_type_ptr get_type_with_mode(a_type_ptr        type,
                              a_type_mode_kind  mode,
                              a_source_position *pos);

extern void record_asm_name_for_lookup(a_symbol_ptr  sym);

#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
extern void push_ELF_visibility(an_ELF_visibility_kind  evk,
                                a_boolean               namespace_attribute);

extern void pop_ELF_visibility(a_boolean  namespace_attribute);

extern an_ELF_visibility_kind ELF_visibility_from_string(
                                                 a_const_char *visibility_str);

extern void update_for_default_ELF_visibility(
                                     an_ELF_visibility_kind  *visibility,
                                     a_boolean               is_class_member);
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */

extern a_boolean check_transparent_union(a_type_ptr        tp,
                                         a_source_position *pos);

#endif /* GNU_EXTENSIONS_ALLOWED */

/*
Return TRUE if the upcoming tokens introduce standard attributes.
*/
#define std_attribute_tokens_next()                                          \
  (curr_token == tok_lbracket && std_attributes_enabled &&                   \
   next_token() == tok_lbracket)

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Return TRUE if the upcoming tokens introduce Microsoft attributes.
*/
#define microsoft_attribute_tokens_next()                                    \
  (ms_extensions && curr_token == tok_lbracket &&                           \
   !std_attribute_tokens_next())

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Return TRUE if the given attribute is of the form [[...]] or alignas(...).
*/
#define is_std_attribute(ap)                                                 \
  ((ap)->family == af_std || (ap)->family == af_alignas)

/*
Return TRUE if the given attribute is unrecognized or an empty attribute.
Such attributes cannot be "applied" to any entities.
*/
#define is_unapplicable_attr(ap)                                             \
  ((ap)->kind == ak_unrecognized || (ap)->kind == ak_empty_attr)

/*
Return TRUE if the given attribute is unrecognized.
*/
#define is_unrecognized_attr(ap)                                             \
  ((ap)->kind == ak_unrecognized)

/*
Reclassify the given attribute as "unrecognized".
*/
#define make_attr_unrecognized(ap)                                           \
  { (ap)->kind = ak_unrecognized; }

/*
Return TRUE if the given attribute is an af_gnu attribute or an af_std
attribute in the "gnu" namespace.
*/
#define is_gcc_attribute(ap)                                                 \
  ((ap)->family == af_gnu || (ap)->is_std_gcc_attribute)

extern void reset_attr_family_seen(an_attribute_ptr ap);

extern a_boolean is_valid_attribute_identifier(a_token_kind  tok);

extern an_attribute_ptr scan_attribute(an_attribute_family  af,
                                       an_attribute_ptr     using_ns_ap);

extern an_attribute_ptr scan_attributes(an_attribute_location  loc);

extern void scan_and_discard_attributes(an_error_severity sev,
                                        an_error_code     err_code);

extern an_attribute_ptr scan_gnu_attribute_groups(an_attribute_location  loc);

#if CHECKING
extern a_boolean unscanned_attributes_pending(void);
#endif /* CHECKING */

extern void unscan_attributes(an_attribute_ptr  attributes);

EXTERN_THREAD a_token_sequence_number
		last_token_number_of_attributes;
			/* After scanning a group of attributes, this variable
			   holds the token sequence number of the last token of
			   that attribute group (until the next attribute
			   group is scanned). */

EXTERN_THREAD a_source_position
		end_position_of_attributes;
			/* After scanning a group of attributes, this variable
			   holds the position of the last token of that
			   attribute group (until the next attribute group is
			   scanned). */

#if GNU_EXTENSIONS_ALLOWED
EXTERN_THREAD a_boolean
                gnu_abi_tag_attribute_seen;
                        /* TRUE if an abi_tag attribute has been seen in the
                           source (triggers additional mangling work). */
#endif /* GNU_EXTENSIONS_ALLOWED */

#if !IA64_ABI
EXTERN_THREAD a_boolean
                no_unique_address_attribute_seen;
                        /* TRUE if a no_unique_address attribute has been seen
                           in the source (triggers additional layout work). */
#endif /* !IA64_ABI */

EXTERN_THREAD an_attribute_ptr
		unscanned_attributes;
			/* A pointer to previously-scanned attributes that
			   should be returned from the next call to
			   scan_attributes instead.  NULL indicates that no
			   unscanned attributes are present. */

extern void skip_over_attributes(void);

extern void report_bad_attribute_target(an_error_severity  sev,
                                        an_attribute_ptr   ap);

extern an_attribute_ptr* get_attribute_link(char              *entity,
                                            an_il_entry_kind  entity_kind);

extern void attach_attributes(an_attribute_ptr  attributes,
                              char              *entity,
                              an_il_entry_kind  entity_kind);

extern void transform_type_with_gnu_attributes(a_type_ptr        *p_type,
                                               an_attribute_ptr  attributes,
                                               void              *assoc_info);

#if GNU_X86_ATTRIBUTES_ALLOWED
extern void apply_calling_convention_attributes(
                                             a_type_ptr        *p_type,
                                             an_attribute_ptr  attributes,
                                             void              *assoc_info);
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */

extern a_type_ptr make_typeref_with_attributes(a_type_ptr        tp,
                                               an_attribute_ptr  attributes);

extern void attach_type_attributes(a_type_ptr        *p_type,
                                   an_attribute_ptr  attributes,
                                   void              *assoc_info);

extern void extract_type_transforming_attributes(
                                               an_attribute_ptr  *p_attributes,
                                               an_attribute_ptr  *p_extracted);

extern a_boolean attribute_is_template_dependent(an_attribute_ptr ap);

extern a_boolean equivalent_attributes(an_attribute_ptr  ap1,
                                       an_attribute_ptr  ap2,
                                       a_boolean         ignore_family);

extern an_attribute_ptr *f_last_attribute_link(an_attribute_ptr  *attributes);

/*
A macro to access the last link of an attributes list.  The argument ap must
be a pointer to an attribute pointer.  If ap is non-NULL and *ap points to a
list of one or more attributes, return the address of the last "next" pointer
of that list.  Otherwise, return ap itself.
*/
#define last_attribute_link(ap)                                              \
  (/*lint --e(506)*/ ((ap) == NULL || *(ap) == NULL) ?                       \
                                         (ap) : f_last_attribute_link(ap))

/*
Utility to copy an attribute.  Both "from" and "to" have type an_attribute_ptr.
Note that this is a "shallow" copy (i.e., any attribute arguments will be
shared between the original attribute and the copy that is returned).  See
also copy_of_attributes_list and copy_of_attributes_with_substitution.
*/
#define copy_attribute(from, to)                                             \
  { (to) = alloc_attribute();                                                \
    *(to) = *(from);                                                         \
    (to)->next = NULL;                                                       \
  }  /* copy_attribute */

extern an_attribute_ptr copy_of_attributes_list(an_attribute_ptr  attributes);

extern an_attribute_ptr copy_of_attributes_with_substitution(
                                an_attribute_ptr      attributes,
                                a_boolean             primary_only,
                                a_symbol_ptr          template_sym,
                                a_template_param_ptr  t_params,
                                a_template_arg_ptr    t_args,
                                a_type_ptr            parent_class,
                                a_boolean             is_partial_instantiation,
                                a_boolean             *p_error);

/*
Return TRUE if the given attribute may produce a new type entry when applied
to an existing type entry.
*/
#define is_type_transforming_attribute(ap)  ((ap)->transforms_type_specifier)

/*
Return TRUE if the given attribute was applied directly to a class type or
enum type.
*/
#define is_tag_attribute(ap)                                                 \
  ((ap)->syntactic_location == al_tag_name ||                                \
   (ap)->syntactic_location == al_post_tag_definition)


extern void mark_primary_decl_attributes(an_attribute_ptr  attributes);

extern an_attribute_ptr composite_attributes(an_attribute_ptr  ap1,
                                             an_attribute_ptr  ap2);

extern an_attribute_ptr get_param_variable_attr_copies(a_param_type_ptr  ptp);

typedef unsigned int an_attr_corresp_flag_set;
/*
Various flag values that determine how attributes on corresponding entities in
distinct translation units should be matched up.
*/
#define ACF_NO_FLAGS  0x0
#define ACF_STRICT_MATCH  0x0
	/* If an attribute appears on an entity in one translation unit, an
	   equivalent attribute must appear on any corresponding entity in
	   another translation unit.  (This is the default.) */
#define ACF_STRICT_MATCH_OR_VOID  0x1
	/* If an attribute appears on an entity in one translation unit, a
	   corresponding entity must either carry an equivalent attribute or
	   not carry that attribute kind at all.  (For attributes with no
	   arguments this is equivalent to ACF_MATCH_OPTIONAL below.) */
#define ACF_MATCH_OPTIONAL  0x2
	/* An attribute appearing on an entity in one translation unit puts
	   no requirement on attributes for a corresponding entity in another
	   translation unit.  (However, specialized correspondence code can
	   impose additional requirements.) */
#define ACF_CUSTOM_MATCH  0x3
	/* Attributes are matched using a specific callback function of type
	   an_attr_corresp_checking_fn (see below). */
#define ACF_MATCH_MASK 0x3
	/* Mask value to extract the "attribute matching mode". */
#define ACF_ALWAYS_TRANS_COPY 0x4
	/* When merging multiple translation units, always copy this kind of
	   attribute, even if an exact match was found. */

/*
Type of a function to check whether two attributes match on two corresponding
attributes (in different translation units).
*/
typedef a_boolean an_attr_corresp_checking_fn(char              *entity1,
                                              char              *entity2,
                                              an_il_entry_kind  entity_kind,
                                              an_attribute_ptr  ap1,
                                              an_attribute_ptr  ap2);

/*
A macro notation for the case where no special function should be called to
check for corresponding attributes across translation units (this is the
common case).
*/
#define NO_CHECKING_FN ((an_attr_corresp_checking_fn*)NULL)

extern void get_attr_corresp_checking_info(
                                     an_attribute_ptr             ap,
                                     an_il_entry_kind             target_kind,
                                     an_attr_corresp_flag_set     *p_flags,
                                     an_attr_corresp_checking_fn  **p_fn);

#if GNU_EXTENSIONS_ALLOWED
extern a_type_ptr copy_gnu_type_properties(a_type_ptr  dst,
                                           a_type_ptr  src);
#endif /* GNU_EXTENSIONS_ALLOWED */

extern a_hash_value hash_attribute_kind(a_void_ptr  key);

extern a_boolean compare_for_attr_corresp_checking_map(a_void_ptr  entry,
                                                       a_void_ptr  key);

extern a_boolean compare_for_attr_name_map(a_void_ptr  entry,
                                           a_void_ptr  key);

#if GNU_EXTENSIONS_ALLOWED
extern a_boolean compare_for_asm_name_map(a_void_ptr  entry,
                                          a_void_ptr  key);

extern void add_implicit_abi_tag_attribute(a_source_correspondence *scp,
                                           an_il_entry_kind        entity_kind,
                                           a_constant_ptr          con);
#endif /* GNU_EXTENSIONS_ALLOWED */

#if GNU_FUNCTION_MULTIVERSIONING
extern a_boolean process_multiversion_function(
                                             an_attribute_ptr ap,
                                             a_scope_depth    scope_depth,
                                             a_routine_ptr    representative,
                                             a_routine_ptr    *target,
                                             a_boolean        *found_existing);
#endif /* GNU_FUNCTION_MULTIVERSIONING */

extern an_attribute_ptr make_module_attribute(a_const_char        *name,
                                              an_attribute_family family,
                                              an_attribute_ptr    next);

/*
Opaque pointer to the result of looking up an attribute name.
*/
typedef struct an_attr_name_map_entry *an_attr_name_map_entry_ptr;

extern a_boolean attribute_is_supported(a_const_char        *name,
                                        a_const_char        *namespace_name,
                                        an_attribute_family family);

extern void apply_attributes_to_prototype_instantiation(
                               an_attribute_ptr                 new_list,
                               a_template_symbol_supplement_ptr tssp,
                               a_source_position                *def_pos,
                               a_boolean                        is_definition);

extern an_attribute_ptr attribute_string_literal_arg(
                                            an_attribute_kind            kind,
                                            a_source_correspondence_ptr  scp);

extern a_const_char *attribute_string_for_kind(
                                              an_attribute_kind           kind,
                                              a_source_correspondence_ptr scp);

extern
a_host_large_integer validate_ext_vector_size(a_constant         *size_con,
                                              a_type_ptr         elem_type,
                                              a_source_position  *diag_pos,
                                              a_boolean          *p_err);

extern void attribute_one_time_init(void);

extern void attribute_trans_unit_init(void);

extern void attribute_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ATTRIBUTE_H */

