/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

ms_attrib.h -- Declarations related to ms_attrib.c (Microsoft attribute
               processing).

*/

/* Avoid including these declarations more than once: */
#ifndef MS_ATTRIB_H
#define MS_ATTRIB_H 1

#if MICROSOFT_EXTENSIONS_ALLOWED

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Entry used to represent a parameter description for a Microsoft attribute.
*/
typedef struct an_ms_attribute_param *an_ms_attribute_param_ptr;
typedef struct an_ms_attribute_param {
  an_ms_attribute_param_ptr
		next;	/* Next entry on a list of parameter entries, or NULL
			   for the last entry. */
  char		*name;	/* Name of the parameter.  Present even for "unnamed"
			   parameters for descriptive purposes in
			   diagnostics. */
  char		**values;
			/* When "kind" is msapk_enumeration, this is an array
			   of the acceptable values.  The last list entry is
			   NULL. */
  an_ms_attribute_arg_kind
		kind;	/* The kind of parameter (string, integer, etc. ). */
  a_byte_boolean
		is_unnamed;
			/* TRUE if the parameter is unnamed. */
} an_ms_attribute_param;


/*
Entry used to represent the definition of a particular kind of Microsoft
attribute.
*/
typedef struct an_ms_attribute_kind_descr *an_ms_attribute_kind_descr_ptr;
typedef struct an_ms_attribute_kind_descr {
  an_ms_attribute_kind
		kind;
			/* The kind of attribute that this represents. */
  an_ms_attribute_target
		target;
			/* Identifies the kind of entity to which this
			   attribute may apply. */
  a_byte_boolean
		initialization_style_arg_allowed;
			/* TRUE if this attribute supports the "attr=1" form
			   of argument list. */
  int		num_params;
			/* The number of parameters the attribute has. */
  sizeof_t	name_length;
			/* The length of the name, not including the null
			   terminator. */
  char		*name;
			/* The name of the attribute.  Null terminated. */
  an_ms_attribute_kind_descr_ptr
		next;	/* Pointer to the next entry in a given hash table
			   bucket of attribute kind description entries. */
  an_ms_attribute_param_ptr
		parameters;
			/* A linked list of attribute parameter entries. */
  an_ms_attribute_param_ptr
		parameters_tail;
			/* Pointer to the last entry on the list of
			   parameters. */
} an_ms_attribute_kind_descr;


extern void skip_microsoft_attribute_tokens(void);

extern
an_ms_attribute_ptr scan_microsoft_attributes(a_boolean	is_param_or_base);

extern void scan_and_append_microsoft_attributes(
                                       an_ms_attribute_ptr  *p_ms_attributes,
                                       a_boolean            is_param_or_base);

extern
void apply_microsoft_attributes(an_ms_attribute_ptr	*attributes,
				char			*entity,
				an_il_entry_kind	kind,
				an_ms_attribute_target	target,
				an_ms_attribute_target	cli_target);

extern
void apply_microsoft_attributes_to_type(an_ms_attribute_ptr *attributes,
                                        a_type_ptr          type);

extern
void apply_microsoft_attributes_to_field(an_ms_attribute_ptr *attributes,
                                         a_field_ptr         field);

extern
void apply_microsoft_attributes_to_variable(an_ms_attribute_ptr *attributes,
                                            a_variable_ptr      variable);

extern
void apply_microsoft_attributes_to_routine(an_ms_attribute_ptr *attributes,
                                           a_routine_ptr       routine);

extern void apply_microsoft_attributes_to_base_class(
                                              an_ms_attribute_ptr *attributes,
                                              a_base_class_ptr    bcp);

extern void verify_standalone_attributes(an_ms_attribute_ptr	*attributes);

extern void dispose_of_unapplied_attributes(an_ms_attribute_ptr	*attributes,
					    an_error_code	error_code);

extern
an_ms_attribute_ptr duplicate_ms_attributes(an_ms_attribute_ptr  orig,
                                            char                 *new_entity);

extern an_ms_attribute_ptr  find_ms_attribute_for_entity(
                                            an_ms_attribute_ptr          msap,
                                            a_source_correspondence_ptr  scp);

extern void ms_attrib_one_time_init(void);

extern void ms_attrib_init(void);

#if DEBUG
/* GCOVR_EXCL_START */
extern void db_microsoft_attribute(an_ms_attribute_ptr	msap);
extern unsigned long db_show_ms_attrib_space_used(unsigned long grand_total);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

EXTERN_THREAD a_boolean
                no_injected_text;
                        /* TRUE if "injected text" (i.e., code added by the
                           compiler) is currently enabled.  Set/reset by the
                           "no_injected_text" Microsoft attribute. */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#endif /* ifndef MS_ATTRIB_H */

