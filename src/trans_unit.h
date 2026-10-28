/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

trans_unit.h -- Declarations related to translation unit management.

*/

/* Avoid including these declarations more than once: */
#ifndef TRANS_UNIT_H
#define TRANS_UNIT_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Type declaration for a pointer to a list of fixups to be applied to based-type
lists.  The definition of the fixup structure is private to il.c.
*/
typedef struct a_based_type_fixup *a_based_type_fixup_ptr;


/*
Structure used to record information about a translation unit.

The front end processes more than one translation unit at once when doing
processing for exported templates.

Note that when simply compiling multiple source files, there is not more
than one translation unit being used.  Instead, the front end is reinitialized
and the subsequent files are processed one at a time, each as a primary
translation unit.
*/
typedef struct a_translation_unit {
  a_translation_unit_ptr
		next;
			/* Pointer to the next entry on a list of translation
			   units, or NULL for the last entry. */
  a_scope_ptr	primary_scope;
			/* The file scope of the translation unit. */
  a_void_ptr	variables_block;
			/* Pointer to the block of memory used to store
			   variables that are saved and restored when
			   switching between translation units. */
  a_scope_pointers_block
		file_scope_pointers_block;
			/* A block of pointers that are logically part of the
			   scope stack entry for the file scope.  This needs
			   to be a separate structure so that the file scope
			   can be reactivated while preserving the pointers
			   to lists of IL entries, symbols, etc. */
  a_source_file_ptr
		source_file;
			/* The source file for the primary source file of the
			   translation unit. */
  an_il_header	il_header;
			/* Copy of il_header for this translation unit.
			   Note that only the translation-unit-specific
			   field are maintained.  See
			   save_translation_unit_state to see the list. */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  a_scope_orphaned_list_header_ptr
		last_scope_orphaned_list_header;
			/* End of the il_header.scope_orphaned_list_headers
			   list; NULL if the list is empty. */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_MACROS_IN_IL
  a_macro_ptr	last_macro;
			/* End of the il_header.macros list; NULL if the
			   list is empty. */
#endif /* RECORD_MACROS_IN_IL */
  a_based_type_fixup_ptr
		based_type_fixup_list;
			/* Head of a linked list of entries identifying
			   types whose based-type lists include entries that
			   must not be removed (either because they refer to
			   type entries that are for front-end use only, or
			   because they refer to types from other translation
			   units). */
  an_exported_template_file_ptr
		exported_template_file;
			/* Points to an entry used to associate a translation
			   unit with a given exported template.  When a file
			   is loaded to define an exported template, the
			   exported template file entry is created first (when
			   the exported template files are read).  But for a
			   translation unit specified on the command line, the
			   exported template file entry is created while
			   the translation unit is being processed. */
#if ORPHAN_PROCESSING_NEEDED
  an_orphaned_il_entry_list
		*orphaned_file_scope_il_entries;
			/* Pointer to the orphaned file scope IL entry array
			   for this translation unit. */
#endif /* ORPHAN_PROCESSING_NEEDED */
  a_byte_boolean
		specified_on_command_line;
			/* TRUE if the translation unit was specified on the
			   command-line (FALSE if it was loaded to define an
			   exported template). */
  a_byte_boolean
		additional_instantiation_wrapup_required;
			/* This flag is set when something is changed in the
			   instantiation state information that requires an
			   additional pass of instantiation wrapup
			   processing. */
#if MODULE_ID_NEEDED
  char		**module_id_ptr;
			/* Pointer to the module-id value for this translation
			   unit. */
#endif /* MODULE_ID_NEEDED */
  a_memory_region_number
		file_scope_region_number;
			/* The memory region number for the file scope of
			   this translation unit. */
#if NEED_NAME_MANGLING
  a_namespace_ptr
		individuated_namespace;
			/* A dummy namespace used during mangling for the
			   individuation of entities. */
#endif /* NEED_NAME_MANGLING */
#if EXPENSIVE_CHECKING && DEBUG
  a_bit_field	is_partially_sequenced:1;
			/* TRUE if sequencing has started on this translation
			   unit.  Used by expensive checks to validate sequence
			   numbers.  Generally speaking, this value only
			   becomes TRUE once (during the first call to
			   record_start_of_source_file).  However, in the case
			   of a lexical reset (for cases like PCH processing),
			   this value is expected to revert to FALSE. */
  a_bit_field	is_fully_sequenced:1;
			/* TRUE if the translation unit has been fully
			   sequenced.  Used by expensive checks to validate
			   sequence numbers.  Generally speaking, this value
			   only becomes TRUE once (when sequencing concludes).
			   However, in the case of implicit includes (for
			   delayed template instantiations) translation units
			   may have their sequences expanded, in which case
			   this value is expected to revert to FALSE.
			   Additionally, similarly to is_partially_sequenced
			   lexical resets revert this value to FALSE. */
#endif /* EXPENSIVE_CHECKING && DEBUG */
} a_translation_unit;


/*
Entry used to maintain a stack of translation units.
*/
typedef struct a_translation_unit_stack_entry
                                           *a_translation_unit_stack_entry_ptr;
typedef struct a_translation_unit_stack_entry {
  a_translation_unit_stack_entry_ptr
		next;
			/* Pointer to the previous stack entry (e.g., the
			   entry that should become the current entry when
			   this one is popped off of the stack. */
  a_translation_unit_ptr
		prev_trans_unit;
			/* Pointer to the previous value of
			   curr_translation_unit that should be restored when
			   the translation unit stack is popped. */
} a_translation_unit_stack_entry;


/*
Entry used to maintain a stack of translation units that should be searched
during instantiation of an exported template for argument dependent lookup.
*/
typedef struct an_export_trans_unit_stack_entry
                                         *an_export_trans_unit_stack_entry_ptr;
typedef struct an_export_trans_unit_stack_entry {
  an_export_trans_unit_stack_entry_ptr
		next;
			/* Pointer to the previous stack entry (e.g., the
			   entry that should become the current entry when
			   this one is popped off of the stack. */
  a_translation_unit_ptr
		trans_unit;
			/* Pointer to the translation unit this export template
			   translation unit stack is adding to argument
			   dependent lookup. */
} an_export_trans_unit_stack_entry;


/*
Entry pointed to by the trans_unit_corresp field of a_source_correspondence
to describe a linkage-based correspondence between entities in different
translation units.  Each entity with linkage will point to one of these
entries, and that entry will also be pointed to by all equivalent entities
in other translation units.
*/
typedef struct a_trans_unit_corresp *a_trans_unit_corresp_ptr;
typedef struct a_trans_unit_corresp {
  char          *canonical;
                        /* The instance of the entity that is considered the
                           canonical one, which means the one with the most
                           information.  Points to a definition if one is
                           available, and to a specialization if one is
                           available.  Always non-NULL.  When the entry is
			   on the list of freed and available entries, this
			   is used as the "next" pointer. */
  char          *primary;
                        /* The instance of the entity in the primary IL, if
                           there is one.  NULL otherwise. */
  unsigned int  count;
                        /* The number of entities pointing to this entry. */
  an_il_entry_kind
                kind;   /* Kind of entity. */
} a_trans_unit_corresp;

extern void trans_unit_early_init(void);

extern void process_translation_unit(
				a_const_char			*file_name,
				a_boolean			is_primary,
				an_exported_template_file_ptr	exported_file);

extern void switch_translation_unit(a_translation_unit_ptr	tup);

extern void trans_unit_one_time_init(void);

extern void trans_unit_init(void);

EXTERN_THREAD a_translation_unit_stack_entry_ptr
		curr_translation_unit_stack_entry;
			/* Pointer to the top of the translation unit stack. */

EXTERN_THREAD an_export_trans_unit_stack_entry_ptr
		curr_export_translation_unit_stack_entry;
			/* Pointer to the top of the export template
			   translation unit stack. */

EXTERN_THREAD a_translation_unit_ptr
		curr_translation_unit;
			/* Pointer to the translation unit entry for the
			   translation unit that is being processed (and
			   whose per-translation unit data structures are
			   currently active). */

EXTERN_THREAD a_boolean
		is_primary_translation_unit;
			/* TRUE when processing the primary translation
			   unit.  FALSE when processing secondary translation
			   units. */

EXTERN_THREAD a_const_char
		*trans_unit_file_name;
			/* Name of the primary source file for the current
			   translation unit. */

EXTERN_THREAD a_module_ptr
		trans_unit_module;
			/* If this ia a non-module translation unit, this
			   points to a special module entry used to identify
			   that context.  NULL for TUs that contain a module
			   declaration. */

EXTERN_THREAD a_boolean
		translation_unit_needed_only_for_exported_templates;
			/* TRUE when processing a secondary translation unit
			   that is needed only for the exported templates
			   it contains. */

EXTERN_THREAD a_translation_unit_ptr
		translation_units;
			/* Pointer to a list of translation units.  The first
			   entry on the list is the primary translation
			   unit. */

extern void push_translation_unit_stack(a_translation_unit_ptr	tup);

extern void pop_translation_unit_stack(void);

extern void
push_export_template_translation_unit_stack(a_translation_unit_ptr tup);

extern void pop_export_template_translation_unit_stack();

extern a_boolean push_primary_translation_unit_if_needed();

extern a_boolean push_translation_unit_if_needed(a_symbol_ptr	sym);

extern void f_register_trans_unit_variable(a_void_ptr	var,
					   sizeof_t	size,
					   sizeof_t	field_offset);

extern a_trans_unit_corresp_ptr alloc_trans_unit_corresp(void);

extern void free_trans_unit_corresp(a_trans_unit_corresp_ptr	tucp);

extern void fix_up_translation_unit(a_translation_unit_ptr       tup);

/*
Macro that returns whether a secondary translation unit has been seen.
*/
#define secondary_translation_unit_seen()          \
  (translation_units->next != NULL)


/*
Macro used to register a variable that is related to a specific translation
unit.  This is used to save and restore the contents of the variable when
switching between translation units.
*/
#define register_trans_unit_variable(var)				\
  (f_register_trans_unit_variable((a_void_ptr)&var, sizeof(var), 0))

/*
Macro used to register an array that is related to a specific translation
unit.  This is used to save and restore the contents of the array when
switching between translation units.
*/
#define register_trans_unit_array(var)				\
  (f_register_trans_unit_variable((a_void_ptr)var, sizeof(var), 0))

/*
This is like register_trans_unit_variable except that the translation
unit data structure contains a field whose name is specified by
trans_unit_field.  That field points to the active version of "var".
That is, while the translation unit is active, the field points to
the "var" and while the translation unit is inactive, it points to
memory in the variables_block of the translation unit entry.
*/
#define register_trans_unit_variable_with_field(var, trans_unit_field)	\
  (f_register_trans_unit_variable(				\
                            (a_void_ptr)&var, sizeof(var),		\
    /*lint --e(413)*/       offsetof(a_translation_unit, trans_unit_field)))

/*
Array version of register_trans_unit_variable_with_field.
*/
#define register_trans_unit_array_with_field(var, trans_unit_field)	\
  (f_register_trans_unit_variable(				\
                            (a_void_ptr)var, sizeof(var),		\
     /*lint --e(413)*/      offsetof(a_translation_unit, trans_unit_field)))


#if DEBUG
/* GCOVR_EXCL_START */
extern unsigned long db_show_trans_unit_space_used(unsigned long grand_total);

extern void db_translation_unit(a_translation_unit_ptr tup);

extern void db_translation_unit_stack(void);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef TRANS_UNIT_H */

