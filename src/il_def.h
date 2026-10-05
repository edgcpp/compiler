/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

il_def.h -- Definition of the intermediate language.

*/

/* Avoid including these declarations more than once. */
#ifndef IL_DEF_H
#define IL_DEF_H 1

/*
NOTE:  If you modify definitions here, be sure to modify walk_entry.h,
il.c, lower_il.c, and il_display.c accordingly.  This is crucial in cases
where a pointer is added, and important in other cases.  If you add a
new entry kind, changes may be required in il_file.h and il_walk.c also.
See the documentation on adding an IL entry, at the end of the IL chapter
of the internal documentation, for details.
*/

/*
No #ifndef IL_DEF_H is needed here; this file is included by il.h,
and protected by the ifndef there.
*/

#ifndef TARG_DEF_H
#include "targ_def.h"
#endif /* ifndef TARG_DEF_H */
#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* ifndef LANG_FEAT_H */
#ifndef MEM_TABLES_H
#include "mem_tables.h"
#endif /* ifndef MEM_TABLES_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Specify the version stamp of the IL being generated.
*/
#define IL_VERSION_NUMBER "7.0"


/* Pointers to the main tables in the intermediate language. */
typedef struct a_source_file *a_source_file_ptr;
typedef struct a_constant    *a_constant_ptr;
typedef struct a_template_arg *a_template_arg_ptr;
typedef struct a_type        *a_type_ptr;
typedef struct a_variable    *a_variable_ptr;
typedef struct a_base_class  *a_base_class_ptr;
typedef struct a_field       *a_field_ptr;
typedef struct a_routine     *a_routine_ptr;
typedef struct a_label       *a_label_ptr;
typedef struct an_expr_node  *an_expr_node_ptr;
typedef struct a_statement   *a_statement_ptr;
typedef struct a_handler     *a_handler_ptr;
typedef struct a_try_supplement *a_try_supplement_ptr;
typedef struct an_object_lifetime *an_object_lifetime_ptr;
typedef struct a_namespace   *a_namespace_ptr;
typedef struct a_scope       *a_scope_ptr;
typedef struct a_routine_fixup
                             a_routine_fixup_dummy_typedef;
typedef struct a_lambda      *a_lambda_ptr;
typedef struct a_lambda_capture
                             *a_lambda_capture_ptr;
#if MINIMAL_INLINING
typedef struct a_variable_remapping_for_inlining
			     a_variable_remapping_for_inlining_dummy_typedef;
#endif /* MINIMAL_INLINING */
typedef struct a_template_decl *a_template_decl_ptr;
typedef struct a_template *a_template_ptr;
typedef struct an_ms_attribute *an_ms_attribute_ptr;
#if MICROSOFT_EXTENSIONS_ALLOWED
typedef struct a_generic_constraint *a_generic_constraint_ptr;
typedef struct a_generic_constraint_clause *a_generic_constraint_clause_ptr;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
typedef struct a_destructible_entity_descr
                             a_destructible_entity_descr_dummy_typedef;
typedef struct an_init_pos_descr
                             an_init_pos_descr_dummy_typedef;
#endif /* DO_IL_LOWERING */
typedef struct a_new_delete_supplement
                              *a_new_delete_supplement_ptr;
#if MICROSOFT_EXTENSIONS_ALLOWED
typedef struct a_gcnew_supplement
                              *a_gcnew_supplement_ptr;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
typedef struct a_gnu_routine_supplement
                              *a_gnu_routine_supplement_ptr;
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
typedef struct a_module       *a_module_ptr;
typedef struct a_module_interface
                              *a_module_interface_ptr;
typedef struct a_module_entity
                              *a_module_entity_ptr;
typedef struct a_scoped_expression
                              *a_scoped_expression_ptr;
typedef struct a_data_member_spec
                              *a_data_member_spec_ptr;

/* Opaque type definition for an_arg_operand (used in the expression
   processing routines, but a pointer to it appears in a front-end only
   field in the IL; its structure is not known here). */
typedef struct an_arg_operand *an_arg_operand_ptr;
/* Opaque type definition for an_expr_rescan_info_entry (used in the
   expression processing routines, but a pointer to it appears in some
   front-end only fields in the IL; its structure is not known here).
   It is defined in exprutil.h. */
typedef struct an_expr_rescan_info_entry *an_expr_rescan_info_entry_ptr;
/* Opaque type definition for a string or literal kind (used in the lexical
   processing routines, but part of the ck_string variant of a_constant for
   convenience); its values are not known here. */
typedef int a_string_or_char_literal_kind;
#if BACK_END_IS_CP_GEN_BE && TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
/* Opaque type definition for a_type_scan_record_ptr, which is used by the
   C++-generating back end during scans of template argument types to avoid
   unbounded loops and recursion. */
typedef struct a_type_scan_record *a_type_scan_record_ptr;
#endif /* BACK_END_IS_CP_GEN_BE && ... */
#if MICROSOFT_EXTENSIONS_ALLOWED
/* Deal with a forward reference: */
typedef struct a_property_or_event_descr *a_property_or_event_descr_ptr;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Data structures related to source position and correspondence:
*/
typedef struct a_source_file {
  /* Information about a source file used by the compilation.  A tree of
     these represents the full history of source file compilation and
     inclusion, and is also used to turn a sequence number into a file name
     and line number pair.  Also used to indicate remapping due to #line
     directives: an entry appears for each line sequence remapped in
     that way, with first_line_number set appropriately. */
  a_const_char	*file_name;
			/* The display form of the file name.  May be the
			   same address as full_name (but NOT a pointer to
			   the tail of the full_name string).  Null-terminated.
			   This is the form of the name to be used in error
			   messages and the like. */
  a_const_char	*full_name;
			/* The actual file name of the file.  Null-terminated.
			   NULL for entries generated by #line directives. */
  a_const_char	*name_as_written;
			/* The name as specified on the command line or in a
			   #include or #using directive, including a path (if
			   any).  Null-terminated.  Note that it is not
			   necessarily the same as full_name with the directory
			   path stripped off -- for instance, full_name may be
                           "path/xxx/abc.h" when ``#include "xxx/abc.h"''
			   appears in the source. */
  a_seq_number	first_seq_number,
		last_seq_number;
			/* The range of sequence numbers included in this
			   file and include files within it.  The first
			   is larger than the last if the file has zero lines.
			   A value of MAX_SEQ_NUMBER for the last indicates
			   that the final number is not yet known. */
  a_line_number first_line_number;
			/* The line number (in the file) associated with
			   first_seq_number.  Has the value 1 for entries
			   not generated by #line directives. */
  a_source_file_ptr
		first_child_file,
		last_child_file;
			/* The list of include files referenced from this
			   source file, in order.  Both pointers are NULL
			   if the present file references no include files.
			   The list is linked on the "next" link. */
  a_source_file_ptr
		next;
			/* The next include file referenced by the parent
			   of this file, or NULL if there is no next file. */
  a_module_ptr	assoc_module;
			/* If this is a module source file, the associated
			   module.  NULL if there is no associated module. */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  a_bit_field	related_file_implicit_include_done:1;
			/* For a header file this is TRUE if an attempt has
			   been made to implicitly include the source file
			   (e.g., .c file) that corresponds to this header
			   file.  This is set to TRUE even if the attempt
			   failed (e.g., the file does not exist). */
  a_bit_field	is_implicit_include:1;
			/* TRUE if this file was implicitly included to provide
			   a template definition.  FALSE for assemblies. */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  a_bit_field	is_include_file:1;
			/* TRUE if this is a file that was included
			   (explicitly or implicitly).  FALSE for the
			   primary source file of this compilation and for
			   source file entries associated with any primary
			   source files from which precompiled header
			   information has been restored. */
  a_bit_field	included_by_system_include:1;
			/* TRUE if this is a file that was included using
			   the #include <file.h> or #using <file.dll> notation.
			   FALSE for files included with the #include "file.h"
			   or #using "file.dll" notation and entries not
			   associated with include files. */
  a_bit_field	included_by_preinclude:1;
			/* TRUE if this is a file that was included using the
			   --preinclude or --preusing command-line option. */
  a_bit_field	preinclude_macros_only:1;
			/* TRUE if this is a preincluded file that was
			   included by the preinclude_macros option, and
			   from which only macro definitions should be
			   considered.  FALSE for assemblies. */
  a_bit_field	from_system_include_dir:1;
			/* TRUE if this source file was found in an include
			   directory marked as a "system" include directory.
			   Warnings are suppressed when processing system
			   include directories.  This is also set for a file
			   included using an absolute path name by a file
			   that has its from_system_include_dir flag set.
			   Always FALSE for assemblies. */
  a_bit_field	top_level_file:1;
			/* TRUE if this file is a top-level file, i.e., it
			   wasn't included by another file.  TRUE for the
			   primary source file and for secondary files
			   read when processing exported templates.  Also TRUE
			   for the primary source file associated with any
			   precompiled header files that are being used.  FALSE
			   for files brought in by template implicit
			   inclusion. */
  a_bit_field	top_level_file_from_pch:1;
			/* When top_level_file is TRUE, this field is TRUE
			   for a file that is a primary source file
			   associated with a precompiled header file that is
			   being used. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_assembly_file:1;
			/* TRUE if the source file is a C++/CLI assembly.
			   Assembly source files don't use the "normal" input
			   mechanism; the sequence number information includes
			   a single sequence number (i.e., first_seq_number ==
			   last_seq_number) and this sequence number is used
			   for all tokens scanned from the file. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} a_source_file;

/*
Entry used to build a lookup table that translates a sequence number into a
source file pointer.  An array of pointers to these entries is constructed
in sequence number order.  A binary search is used to find a given sequence
number in the table.  The table contains entries that reflect #line directives,
so this table can only be used for lookups that are not seeking an actual
physical line of the input file.
*/
typedef struct a_seq_number_lookup_entry *a_seq_number_lookup_entry_ptr;
typedef struct a_seq_number_lookup_entry {
  a_seq_number_lookup_entry_ptr
		next;
			/* Pointer to the next entry in the list.  This is
			   not used for searching, but is used to write the
			   list to the IL file, and to reconstruct the lookup
			   table after the IL is read. */
  a_seq_number	first;
			/* The first sequence number represented by this
			   entry. */ 
  a_seq_number	last;
			/* The last sequence number represented by this
			   entry. */
  a_line_number	line_number;
			/* The line number that corresponds with the first
			   sequence number specified by this entry. */ 
  a_source_file_ptr
		source_file;
			/* The source file containing this sequence number. */ 
#if EXPENSIVE_CHECKING && DEBUG
  a_bit_field	is_marked_for_recycle:1;
			/* TRUE if this sequence number lookup entry is
			   expected to be recycled. */
#endif /* EXPENSIVE_CHECKING && DEBUG */
} a_seq_number_lookup_entry;


/*
Type of an integer index into a table of assemblies being imported from.
*/
typedef unsigned short an_assembly_index;

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Type of integer "tokens" for entities stored in metadata files.
*/
typedef unsigned int a_cpp_cli_token;

/*
Type of an integer index indicating a scope within an assembly.
*/
typedef unsigned short a_scope_index;

/*
Type of an integer index that uniquely identifies a particular scope in
an assembly.  The scope index is stored in the least significant 16 bits,
and the assembly index in the 16 bits above that.
*/
typedef unsigned int an_assembly_scope_index;

/*
Obtain the scope index from the assembly scope index.
*/
#define scope_index_from_assembly_scope_index(assembly_scope_index) \
  ((a_scope_index)(assembly_scope_index & 0xFFFF))

/*
Obtain the assembly index from the assembly scope index.
*/
#define assembly_index_from_assembly_scope_index(assembly_scope_index) \
  ((an_assembly_index)(((assembly_scope_index) >> 16) & 0xFFFF))

/*
Create an assembly scope index from an assembly index and a scope index.
*/
/*lint -emacro(835,make_assembly_scope_index)*/
#define make_assembly_scope_index(assembly_index, scope_index)               \
  ((an_assembly_scope_index)(((scope_index) & 0xFFFF) |                      \
                             (((assembly_index) & 0xFFFF) << 16)))


/*
Entry used to represent a CLI metadata file.  This is used for metadata
files made available by actual #using directives that appear in the source
as well as implicit directives (e.g., for mscorlib) and for preusings from
the command-line.
*/
typedef struct a_cli_metadata_file *a_cli_metadata_file_ptr;
typedef struct a_cli_metadata_file {
  a_const_char  *name_as_written;
			/* The name as specified on the command line or in a
			   #using directive, including a path (if any).
			   Null-terminated.  Note that it is not necessarily
			   the same as full_name with the directory path
			   stripped off -- for instance, full_name may be
			   "path/xxx/abc.h" when '#using "xxx/abc.h"'
			   appears in the source. */
  a_const_char  *full_name;
			/* The actual file name of the file.
			   Null-terminated. */
  a_cli_metadata_file_ptr
		next;
			/* The next entry in a list of entries, or NULL for
			   the last entry. */
  a_source_position
		position;
			/* The position of the start of the directive. */
  an_assembly_index
		assembly_index;
			/* The index of the assembly given by the metadata
			   reader. */
  a_source_file_ptr
		assembly_file;
			/* The source file entry associated with this
			   assembly. */
  a_source_position
		inserted_position;
			/* The sequence number to be used for every token
			   scanned from this assembly (so the sequence
			   number can be mapped back to this assembly). */
  a_bit_field   as_friend:1;
			/* TRUE if the #using that named this file included
			   the as_friend keyword, making all types from
			   that assembly visible. */
  a_bit_field   referenced_by_preusing:1;
			/* TRUE if the file was named in a preusing option. */
  a_bit_field   referenced_by_system_using:1;
			/* TRUE if the file was named with the
			   "#using <file.h>" notation and for an implicit
			    #using of mscorlib.  FALSE for files named
			   with the '#using "file.h"' notation or referenced
			   with the --preusing command-line option. */
} a_cli_metadata_file;

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* Type of a scope nesting depth.  This is the depth within the scope_stack. */
/* Defined here (instead of the more obvious symbol_tbl.h) to avoid mutual
   dependency problems. */
typedef int	a_scope_depth;

#define NO_SCOPE_DEPTH (-1)
#define DEPTH_OF_FILE_SCOPE 0


/*
Data structures related to variables:
*/
/*
Possible storage classes for variables and functions.

Note that this represents the C concept of storage class.  In C++, the keyword
"static" is also used to indicate static members of classes.  That kind of
"static" is reflected in things other than the storage class, e.g., the
this_class field for routines.

If you add new storage classes, be sure to update db_storage_class_names.
*/
enum a_storage_class : a_byte {
  sc_unspecified,       /* No explicit storage class was given.  This implies
                           an external definition.  Note that an unspecified
                           storage class in the source program will be
                           mapped to something else (extern or auto) when
                           that is possible, and the sc_unspecified value
                           only remains for external definitions.  (This
                           must be the first enumerator, because we rely
                           on memzero producing this value.) */
  sc_extern,            /* External.  This implies a reference to something
                           defined in another compilation unit. */
  sc_static,            /* Static. */
  sc_auto,              /* Local, stack-based.  Includes parameters. */
  sc_typedef,           /* Not ever used in variables or functions, but
                           in this enumeration for convenience when scanning
                           declarations. */
  sc_register,          /* Register, a special case of local.  Includes
                           parameters declared "register". */
  sc_asm,               /* An asm function.  Only used if ASM_FUNCTION_ALLOWED
                           is TRUE. */
  sc_last
};


/*
An enumeration of C++ operator kinds to identify user-defined overloaded
operators; they apply only to functions with a special function kind of
sfk_operator.
*/
enum an_opname_kind : a_byte {
  onk_none,
  onk_new,               /* "new" */    onk_delete,            /* "delete" */
  onk_array_new,         /* "new[]" */	onk_array_delete,      /* "delete[]" */
  onk_plus,              /* "+" */      onk_minus,             /* "-" */
  onk_star,              /* "*" */      onk_divide,            /* "/" */
  onk_remainder,         /* "%" */      onk_excl_or,           /* "^" */
  onk_ampersand,         /* "&" */      onk_or,                /* "|" */
  onk_compl,             /* "~" */      onk_not,               /* "!" */
  onk_assign,            /* "=" */      onk_lt,                /* "<" */
  onk_gt,                /* ">" */      onk_plus_assign,       /* "+=" */
  onk_minus_assign,      /* "-=" */     onk_times_assign,      /* "*=" */
  onk_divide_assign,     /* "/=" */     onk_remainder_assign,  /* "%=" */
  onk_excl_or_assign,    /* "^=" */     onk_and_assign,        /* "&=" */
  onk_or_assign,         /* "|=" */     onk_shift_left,        /* "<<" */
  onk_shift_right,       /* ">>" */     onk_shift_right_assign,/* ">>=" */
  onk_shift_left_assign, /* "<<=" */    onk_eq,                /* "==" */
  onk_ne,                /* "!=" */     onk_le,                /* "<=" */
  onk_ge,                /* ">=" */     onk_spaceship,         /* <=> */
  onk_and_and,           /* "&&" */     onk_or_or,             /* "||" */
  onk_plus_plus,         /* "++" */     onk_minus_minus,       /* "--" */
  onk_comma,             /* "," */      onk_arrow_star,        /* "->*" */
  onk_arrow,             /* "->" */     onk_function_call,     /* "()" */
  onk_subscript,         /* "[]" */
  onk_question,          /* "?" -- only used in front end. */
  onk_gnu_min,           /* "<?" */     onk_gnu_max,           /* ">?" */
  onk_await,             /* co_await */ onk_last
};

#define is_new_operator(op)                                         \
  ((op) == onk_new || (op) == onk_array_new)

#define is_delete_operator(op)                                      \
  ((op) == onk_delete || (op) == onk_array_delete)


enum an_access_specifier : a_byte {
  /* C++ access control:  "public", "private", or "protected" for class
     members or "public" (meaning no access control) for other entities. */
  as_public,            /* No access restrictions. */
  as_protected,         /* Class member name can be referenced by functions
                           that are members of its own class or of classes
                           derived from its class or by friends of its
                           class or friends of classes derived from it. */
  as_private,           /* Class member name can be referenced only by
                           member functions and by friends of its class. */
  as_inaccessible	/* Entity cannot be accessed.  (This value is used
			   in the symbol table but not in the IL.) */
};

#define is_more_accessible(access1, access2) ((int)(access1) < (int)(access2))

/*
C++/CLI assembly visibility kinds.
*/
enum an_assembly_visibility : a_byte {
  av_none,		/* No assembly visibility applicable. */
  av_public,		/* Assembly member publicly visible. */
  av_private		/* Assembly member not publicly visible. */
};


/*
Kind of name linkage (e.g., external name visibility).  Note that "name
linkage" applies to names (in some implementations it controls whether the
external name for an entity will be mangled) and also applies to function types
(e.g., implying a calling convention in some implementations).

If you add linkage kinds, be sure to update name_linkage_kind_names and
NUM_BITS_FOR_NAME_LINKAGE; you may also need to customize
routine_linkages_are_compatible and routine_linkages_are_identical (in types.c)
and macros is_custom_name_linkage_kind_for_rout_type and
is_name_linkage_kind_subject_to_name_mangling.
*/
enum a_name_linkage_kind : a_byte {
  nlk_none,		/* No linkage, as for a local variable. */
  nlk_internal,		/* Internal linkage, as for a file-scope static. */
  nlk_cplusplus_external,
			/* C++ external linkage, as for an extern in C++.
			   Implies name mangling if that technique is used. */
  nlk_external,		/* External linkage, as for an external routine
                           (i.e., "extern C"). */
  nlk_last_standard = nlk_external,
#ifdef CUSTOM_NAME_LINKAGE_KINDS
  /* An implementation can add additional linkage kinds by defining this
     macro. */
  CUSTOM_NAME_LINKAGE_KINDS
#endif /* ifdef CUSTOM_NAME_LINKAGE_KINDS */
  nlk_last
};

/* If CUSTOM_NAME_LINKAGE_KINDS is defined, a few other macros should be
   defined as well. */
#ifdef CUSTOM_NAME_LINKAGE_KINDS
#ifndef NUM_BITS_FOR_NAME_LINKAGE
 #error -- NUM_BITS_FOR_NAME_LINKAGE must be defined when \
           CUSTOM_NAME_LINKAGE_KINDS is defined
#endif /* ifndef NUM_BITS_FOR_NAME_LINKAGE */
#ifndef is_custom_name_linkage_kind_for_rout_type
 #error -- is_custom_name_linkage_kind_for_rout_type must be defined when \
           CUSTOM_NAME_LINKAGE_KINDS is defined
#endif /* is_custom_name_linkage_kind_for_rout_type */
#endif /* ifdef CUSTOM_NAME_LINKAGE_KINDS */

/* Number of bits required to hold a name linkage.  nlk_last need not be
   accounted for. */
#ifndef NUM_BITS_FOR_NAME_LINKAGE
#define NUM_BITS_FOR_NAME_LINKAGE 3
#endif /* ifndef NUM_BITS_FOR_NAME_LINKAGE */

/* Name linkage kinds are applied both to names and to routine types, yet
   only certain of them are appropriate for routine types.  For example, the
   name of a static function may have nlk_internal linkage, but its type would
   have nlk_external (extern "C") or nlk_cplusplus_external (extern "C++")
   linkage.  Linkage kinds that are added by a given implementation may or
   may not apply to routine types. */
#ifdef is_custom_name_linkage_kind_for_rout_type
#define or_is_custom_name_linkage_kind_for_rout_type(nlk)               \
    || is_custom_name_linkage_kind_for_rout_type(nlk)
#else /* ifndef is_custom_name_linkage_kind_for_rout_type */
#define or_is_custom_name_linkage_kind_for_rout_type(nlk) /* Nothing */
#endif /* ifdef is_custom_name_linkage_kind_for_rout_type */

#define is_name_linkage_kind_for_rout_type(nlk)                         \
  (nlk == (a_name_linkage_kind)nlk_external ||                          \
   nlk == (a_name_linkage_kind)nlk_cplusplus_external                   \
   or_is_custom_name_linkage_kind_for_rout_type(nlk))

#if NEED_NAME_MANGLING
/* Macro that determines whether name mangling should be done for
   an entity with a given linkage kind.  By default, name mangling
   is done for everything except extern "C". */
#ifndef is_name_linkage_kind_subject_to_name_mangling
#define is_name_linkage_kind_subject_to_name_mangling(nlk)              \
  ((nlk) != (a_name_linkage_kind)nlk_external)
#endif /* ifndef is_name_linkage_kind_subject_to_name_mangling */
#endif /* NEED_NAME_MANGLING */

/*
Names of linkage kinds.  These are used to recognize the string in a
linkage specification (extern "xxx") and for debug output.
*/
EXTERN_CONSTINIT_ARRAY(a_const_char*, name_linkage_kind_names, nlk_last + 1)
#if VAR_INITIALIZERS
= {
  "no",			/* nlk_none */
  "internal",		/* nlk_internal */
  "C++",		/* nlk_cplusplus_external */
  "C",			/* nlk_external */
#ifdef CUSTOM_NAME_LINKAGE_KIND_NAMES
  /* An implementation can add additional linkage kind names by defining this
     macro. */
  CUSTOM_NAME_LINKAGE_KIND_NAMES
#endif /* ifdef CUSTOM_NAME_LINKAGE_KIND_NAMES */
  "last"		/* nlk_last */
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(name_linkage_kind_names)

/*
List of all IL entry kinds.

If you change this, also change:
  - il_entry_kind_names (in this file)
  - sizeof_il_entry (in this file)
  - type_to_il_entry_kind (in il.h)
*/
enum an_il_entry_kind : a_byte {
  iek_none,		/* Skip zero value; it's used as a marker. */
  iek_source_file,	/* a_source_file */
  iek_constant,		/* a_constant */
  iek_param_type,	/* a_param_type */
  iek_routine_type_supplement,
			/* a_routine_type_supplement */
  iek_based_type_list_member,
			/* a_based_type_list_member */
  iek_type,		/* a_type */
  iek_variable,		/* a_variable */
  iek_field,		/* a_field */
  iek_exception_specification,
			/* an_exception_specification */
  iek_exception_specification_type,
			/* an_exception_specification_type */
  iek_routine,		/* a_routine */
  iek_label,		/* a_label */
  iek_expr_node,	/* an_expr_node */
  iek_for_loop,         /* a_for_loop */
  iek_range_based_for_loop,
                        /* a_range_based_for_loop */
#if MICROSOFT_EXTENSIONS_ALLOWED
  iek_for_each_loop,    /* a_for_each_loop */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  iek_switch_case_entry,
                        /* a_switch_case_entry */
  iek_switch_stmt_descr,
                        /* a_switch_stmt_descr */
  iek_handler,          /* a_handler */
  iek_try_supplement,	/* a_try_supplement */
#if MICROSOFT_EXTENSIONS_ALLOWED
  iek_microsoft_try_supplement,
			/* a_microsoft_try_supplement */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  iek_block,		/* a_block */
  iek_statement,	/* a_statement */
  iek_object_lifetime,	/* an_object_lifetime */
  iek_scope,		/* a_scope */
  iek_id_name,          /* String giving the name of an identifier. */
  iek_string_text,	/* Text of a string literal. */
  iek_other_text,	/* Text of a file name or similar information. */
#if C99_IL_EXTENSIONS_SUPPORTED
  iek_internal_complex_value,
			/* an_internal_complex_value */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  iek_namespace,	/* a_namespace */
  iek_using_decl,	/* a_using_decl */
  iek_dynamic_init,	/* a_dynamic_init */
  iek_local_static_variable_init,
			/* a_local_static_variable_init */
  iek_vla_dimension,    /* a_vla_dimension */
#if DO_IL_LOWERING && IA64_ABI
  iek_vcall_offset_entry,
			/* a_vcall_offset_entry */
#endif /* DO_IL_LOWERING && IA64_ABI */
#if MICROSOFT_EXTENSIONS_ALLOWED
  iek_partial_class_body,
			/* a_partial_class_body */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  iek_overriding_virtual_function,
			/* an_overriding_virtual_function */
  iek_derivation_step,  /* a_derivation_step */
  iek_base_class_derivation,
			/* a_base_class_derivation */
  iek_base_class,	/* a_base_class */
  iek_class_list_entry, /* a_class_list_entry */
  iek_routine_list_entry,
                        /* a_routine_list_entry */
  iek_variable_list_entry,
                        /* a_variable_list_entry */
  iek_constant_list_entry,
                        /* a_constant_list_entry */
  iek_class_type_supplement,
			/* a_class_type_supplement */
  iek_template_param_type_supplement,
			/* a_template_param_type_supplement */
  iek_constructor_init, /* a_constructor_init */
  iek_asm_entry,        /* an_asm_entry */
#if GNU_EXTENSIONS_ALLOWED
  iek_asm_operand,      /* an_asm_operand */
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  iek_asm_operand_constraint,
                        /* an_asm_operand_constraint */
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  iek_named_register_list,
                        /* a_named_register_list */
  iek_label_list,       /* a_label_list */
#endif /* GNU_EXTENSIONS_ALLOWED */
  iek_template_arg,     /* a_template_arg */
  iek_new_delete_supplement,
			/* a_new_delete_supplement */
#if MICROSOFT_EXTENSIONS_ALLOWED
  iek_gcnew_supplement,	/* a_gcnew_supplement */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  iek_throw_supplement,	/* a_throw_supplement */
  iek_condition_supplement,
			/* a_condition_supplement */
#if !ABI_CHANGES_FOR_RTTI
  iek_accessible_base_class,
			/* an_accessible_base_class */
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  iek_eh_prologue_supplement,
			/* an_eh_prologue_supplement */
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  iek_source_sequence_entry,
			/* a_source_sequence_entry */
  iek_src_seq_secondary_decl,
			/* a_src_seq_secondary_decl */
  iek_src_seq_end_of_construct,
			/* a_src_seq_end_of_construct */
  iek_src_seq_sublist,	/* a_src_seq_sublist */
  iek_instantiation_directive,
			/* an_instantiation_directive */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  iek_scope_orphaned_list_header,
			/* a_scope_orphaned_list_header */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_HIDDEN_NAMES_IN_IL
  iek_hidden_name,	/* a_hidden_name */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  iek_pragma,		/* a_pragma */
  iek_template,		/* a_template */
#if RECORD_MACROS_IN_IL
  iek_macro,		/* a_macro */
#endif /* RECORD_MACROS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
  iek_per_instantiation_needed_flags_entry,
			/* a_per_instantiation_needed_flags_entry */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  iek_element_position,	/* an_element_position */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  iek_decl_position_supplement,
			/* a_decl_position_supplement */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  iek_template_decl,	/* a_template_decl */
  iek_requires_clause,	/* a_requires_clause */
  iek_template_parameter,
			/* a_template_parameter */
  iek_name_reference,	/* a_name_reference */
  iek_name_qualifier,	/* a_name_qualifier */
#if MICROSOFT_EXTENSIONS_ALLOWED
  iek_ms_attribute,	/* an_ms_attribute */
  iek_ms_attribute_arg,	/* an_ms_attribute_arg */
  iek_custom_ms_attribute_arg,
			/* an_ms_attribute_arg */
  iek_property_index_type,
			/* a_property_index_type */
  iek_property_or_event_descr,
			/* a_property_or_event_descr */
  iek_generic_constraint_clause,
			/* a_generic_constraint_clause */
  iek_generic_constraint,
			/* a_generic_constraint */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  iek_seq_number_lookup_entry,
			/* a_seq_number_lookup_entry */
#if RECORD_MACRO_INVOCATIONS
  iek_macro_invocation_record_block,
			/* a_macro_invocation_record_block */
#endif /* RECORD_MACRO_INVOCATIONS */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  iek_ms_if_exists,	/* an_ms_if_exists */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  iek_local_expr_node_ref,
			/* a_local_expr_node_ref */
  iek_static_assertion,
			/* a_static_assertion */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if GENERATE_LINKAGE_SPEC_BLOCKS
  iek_linkage_spec_block,
			/* a_linkage_spec_block */
#endif /* GENERATE_LINKAGE_SPEC_BLOCKS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  iek_local_scope_ref,	/* a_local_scope_ref */
  iek_il_entity_list_entry,
			/* an_il_entity_list_entry */
  iek_lambda,		/* a_lambda */
  iek_lambda_capture,	/* a_lambda_capture */
  iek_attribute,	/* an_attribute */
  iek_attribute_arg,	/* an_attribute_arg */
  iek_attribute_group,	/* an_attribute_group */
  iek_typeref_type_supplement,
			/* a_typeref_type_supplement */
  iek_integer_type_supplement,
			/* an_integer_type_supplement */
#if MICROSOFT_EXTENSIONS_ALLOWED
  iek_cli_metadata_file,
			/* a_cli_metadata_file */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
  iek_gnu_routine_supplement,
                        /* a_gnu_routine_supplement */
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
  iek_coroutine_descr,	/* a_coroutine_descr */
  iek_variable_template_info,
#if MICROSOFT_EXTENSIONS_ALLOWED
  iek_event_interface,  /* an_event_interface */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  iek_subobject_path,	/* a_subobject_path */
  iek_constexpr_if,	/* a_constexpr_if */
  iek_module,		/* a_module */
  iek_module_import_decl,
			/* a_module_import_decl */
  iek_token_sequence, 	/* a_token_sequence */
  iek_token_sequence_entry,
			/* a_token_sequence_entry */
  iek_scoped_expression,/* a_scoped_expression */
  iek_data_member_spec,	/* a_data_member_spec */
  iek_last		/* Marks the end of the list. */
};

/* Macro to test whether or not an entry kind is a string kind. */
#define is_string_entry_kind(entry_kind) \
  ((entry_kind) == iek_id_name || (entry_kind) == iek_string_text || \
   (entry_kind) == iek_other_text)

#if NEED_IL_DISPLAY || DEBUG
/*
Display names for IL entry kinds.
*/
EXTERN_CONSTINIT_ARRAY(a_const_char*, il_entry_kind_names, iek_last + 1)
#if VAR_INITIALIZERS
= {
/* iek_none */				"none",
/* iek_source_file */			"source-file",
/* iek_constant */			"constant",
/* iek_param_type */			"param-type",
/* iek_routine_type_supplement */	"routine-type-supplement",
/* iek_based_type_list_member */	"based-type-list-member",
/* iek_type */				"type",
/* iek_variable */			"variable",
/* iek_field */				"field",
/* iek_exception_specification */	"exception-specification",
/* iek_exception_specification_type */	"exception-specification-type",
/* iek_routine */			"routine",
/* iek_label */				"label",
/* iek_expr_node */			"expr-node",
/* iek_for_loop */			"for-loop",
/* iek_range_based_for_loop */		"range-based-for-loop",
#if MICROSOFT_EXTENSIONS_ALLOWED
/* iek_for_each_loop */			"for-each-loop",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
/* iek_switch_case_entry */		"switch-case-entry",
/* iek_switch_stmt_descr */		"switch-stmt-descr",
/* iek_handler */			"handler",
/* iek_try_supplement */		"try-supplement",
#if MICROSOFT_EXTENSIONS_ALLOWED
/* iek_microsoft_try_supplement */	"microsoft-try-supplement",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
/* iek_block */				"block",
/* iek_statement */			"statement",
/* iek_object_lifetime */		"object-lifetime",
/* iek_scope */				"scope",
/* iek_id_name */			"id-name",
/* iek_string_text */			"string-text",
/* iek_other_text */			"other-text",
#if C99_IL_EXTENSIONS_SUPPORTED
/* iek_internal_complex_value */	"internal-complex-value",
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
/* iek_namespace */			"namespace",
/* iek_using_decl */			"using-decl",
/* iek_dynamic_init */			"dynamic-init",
/* iek_local_static_variable_init */	"local-static-variable-init",
/* iek_vla_dimension */			"vla-dimension",
#if DO_IL_LOWERING && IA64_ABI
/* iek_vcall_offset_entry */		"vcall-offset-entry",
#endif /* DO_IL_LOWERING && IA64_ABI */
#if MICROSOFT_EXTENSIONS_ALLOWED
/* iek_partial_class_body */		"partial-class-body",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
/* iek_overriding_virtual_function */	"overriding-virtual-function",
/* iek_derivation_step */		"derivation-step",
/* iek_base_class_derivation */		"base-class-derivation",
/* iek_base_class */			"base-class",
/* iek_class_list_entry */		"class-list-entry",
/* iek_routine_list_entry */		"routine-list-entry",
/* iek_variable_list_entry */		"variable-list-entry",
/* iek_constant_list_entry */		"constant-list-entry",
/* iek_class_type_supplement */		"class-type-supplement",
/* iek_template_param_type_supplement */"template_param_type_supplement",
/* iek_constructor_init */		"constructor-init",
/* iek_asm_entry */			"asm-entry",
#if GNU_EXTENSIONS_ALLOWED
/* iek_asm_operand */                   "asm-operand",
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
/* iek_asm_operand_constraint */        "asm-operand-constraint",
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
/* iek_named_register_list */           "named-register-list",
/* iek_label_list */                    "label-list",
#endif /* GNU_EXTENSIONS_ALLOWED */
/* iek_template_arg */			"template-arg",
/* iek_new_delete_supplement */		"new-delete-supplement",
#if MICROSOFT_EXTENSIONS_ALLOWED
/* iek_gcnew_supplement */	"gcnew-supplement",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
/* iek_throw_supplement */		"throw-supplement",
/* iek_condition_supplement */		"condition-supplement",
#if !ABI_CHANGES_FOR_RTTI
/* iek_accessible_base_class */		"accessible-base-class",
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
/* iek_eh_prologue_supplement */	"eh-prologue-supplement",
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
/* iek_source_sequence_entry */		"source-sequence-entry",
/* iek_src_seq_secondary_decl */	"src-seq-secondary-decl",
/* iek_src_seq_end_of_construct */	"src-seq-end-of-construct",
/* iek_src_seq_sublist */		"src-seq-sublist",
/* iek_instantiation_directive */	"instantiation-directive",
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
/* iek_scope_orphaned_list_header */	"scope-orphaned-list-header",
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_HIDDEN_NAMES_IN_IL
/* iek_hidden_name */			"hidden-name",
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
/* iek_pragma */			"pragma",
/* iek_template */			"template",
#if RECORD_MACROS_IN_IL
/* iek_macro */				"macro",
#endif /* RECORD_MACROS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
/* iek_per_instantiation_needed_flags_entry */
					"per-instantiation-needed-flags-entry",
#endif /* ONE_INSTANTIATION_PER_OBJECT */
/* iek_element_position */		"element-position",
#if EXTRA_SOURCE_POSITIONS_IN_IL
/* iek_decl_position_supplement */	"decl-position-supplement",
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
/* iek_template_decl */			"template-decl",
/* iek_requires_clause */		"requires-clause",
/* iek_template_parameter */		"template-parameter",
/* iek_name_reference */		"name-reference",
/* iek_name_qualifier */		"name-qualifier",
#if MICROSOFT_EXTENSIONS_ALLOWED
/* iek_ms_attribute */			"ms-attribute",
/* iek_ms_attribute_arg */		"ms-attribute-arg",
/* iek_custom_ms_attribute_arg */	"custom-ms-attribute-arg",
/* iek_property_index_type */		"property-index-type",
/* iek_property_or_event_descr */	"property-or-event-descr",
/* iek_generic_constraint_clause */	"generic-constraint-clause",
/* iek_generic_constraint */		"generic-constraint",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
/* iek_seq_number_lookup_entry */	"seq-number-lookup-entry",
#if RECORD_MACRO_INVOCATIONS
/* iek_macro_invocation_record_block */ "macro-invocation-record-block",
#endif /* RECORD_MACRO_INVOCATIONS */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
/* iek_ms_if_exists */			"ms-if-exists",
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
/* iek_local_expr_node_ref */		"local-expr-node-ref",
/* iek_static_assertion */		"static-assertion",
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if GENERATE_LINKAGE_SPEC_BLOCKS
/* iek_linkage_spec_block */		"linkage-spec-block",
#endif /* GENERATE_LINKAGE_SPEC_BLOCKS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
/* iek_local_scope_ref */		"local-scope-ref",
/* iek_il_entity_list_entry */		"il-entity-list-entry",
/* iek_lambda */			"lambda",
/* iek_lambda_capture */		"lambda-capture",
/* iek_attribute */			"attribute",
/* iek_attribute_arg */			"attribute-arg",
/* iek_attribute_group */		"attribute-group",
/* iek_typeref_type_supplement */	"typeref-type-supplement",
/* iek_integer_type_supplement */	"integer-type-supplement",
#if MICROSOFT_EXTENSIONS_ALLOWED
/* iek_cli_metadata_file */		"CLI metadata file",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
/* iek_gnu_routine_supplement */        "gnu-routine-supplement",
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
/* iek_coroutine_descr */		"coroutine-descr",
/* iek_variable_template_info */        "variable-template-info",
#if MICROSOFT_EXTENSIONS_ALLOWED
/* iek_event_interface */               "event-interface",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
/* iek_subobject_path */		"subobject-path",
/* iek_constexpr_if */			"constexpr-if",
/* iek_module */			"module",
/* iek_module_import_decl */		"mod-import-decl",
/* iek_token_sequence */		"token-sequence",
/* iek_token_sequence_entry */		"token-sequence-entry",
/* iek_scoped_expression */		"scoped-expression",
/* iek_data_member_spec */		"data-member-spec",
/* iek_last */				"last"
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(il_entry_kind_names)
#endif /* NEED_IL_DISPLAY || DEBUG */

/*
Token kinds.

The front end conceptually has two categories of tokens: "simple tokens" and
"complex tokens".  "Simple tokens" are tokens that have a single form.
"Complex tokens" are tokens that have (practically) innumerable distinct forms
(like identifiers, numbers, etc.) or "pseudo tokens" that cannot be spelled
(like an end-of-source token or an IFC entity ref token).

"Complex tokens" should be listed before the tok_last_complex_token constant.

If this enumeration is changed, be sure to change token_names (below) and
opname_kind_for_token (lexical.h).

In addition, the token handling code for modules must be updated.

For "simple tokens", it's sufficient to use an EDG "textual" IFC token; this
requires adding the token kind to the ifc_ebts_complex case of
token_to_basic_token_kind (ifc_modules_write.c).  If the name entered into
token_names (below) for the new token is not unique, additionally, a case in
ifc_token_name_of (returning a unique name) must also be added (as otherwise
the deserialization of the IFC encoding to the front end token kind is
ambiguous).

For "complex tokens" (as described above), the gen-ifc-map tool should be used
to add new IFC node representation of the token.  Once the "IFC Map"
(i.e., ifc_map.h) has been updated via the tool, the writing
(an_ifc_il_map::enter_token_cache in ifc_modules_write.c) and reading
(cache_edg_token_cache in ifc_modules_read.c) code must be updated to make use
of the new IFC node.
*/
enum a_token_kind : unsigned short {
  /* Complex tokens: */
  tok_error                 /* Error token. */,
  tok_identifier,
  tok_float_constant,
  tok_first_literal_token_kind = tok_float_constant,
  tok_fixed_point_constant,
  tok_int_constant,
  tok_char_constant,
  tok_gen_constant,         /* A token representing a general constant.  These
                               cannot always be expressed using ordinary source
                               code, but generated code (from modules or
                               injection) may produce such tokens. */
  tok_string_literal,
  tok_ud_literal,
  tok_last_literal_token_kind = tok_ud_literal,
  tok_end_of_source,
  tok_newline,
  tok_header_name,
  tok_pp_number,
  tok_digit_sequence,
  tok_cpp_quote,
  tok_ptr_to_member 	    /* C++ only */,
  tok_removed_expr	    /* Placeholder for a removed default argument
                               or exception specification. */,
  tok_removed_template_body /* Placeholder for a removed template body. */,
#if MICROSOFT_EXTENSIONS_ALLOWED
  tok_cli_typeid,           /* Represents C++/CLI X::typeid construct. */  
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  tok_decltype_construct,   /* Used to represent a decltype(expr) construct
                               that has been coalesced. */
  tok_pending_ifc_expr,     /* Generated when reading an IFC file to indicate
                               that there exists an expression that has not yet
                               been processed. */
  tok_ifc_entity_ref,       /* Generated when reading an IFC file to represent
                               a reference to another IFC entity.  Only appears
                               in tokens from token caches (the cache's
                               associated index information should be
                               translated to type an_ifc_expr_index). */
  tok_ifc_decl_ref,         /* This is a special variant of the
                               tok_ifc_entity_ref that represents
                               an_ifc_decl_index rather than
                               an_ifc_expr_index.  */
  tok_ifc_type_ref,         /* This is used to represent a resolved type
                               (similar to a tok_decltype_construct).  However,
                               the associated type is not converted from
                               an_ifc_type_index into an IL type until the
                               token is used during parsing. */
  tok_ifc_param_ref,        /* Generated when reading an IFC file to represent
                               an enk_param_ref representing a reference to a
                               parameter that is not yet available. */
  tok_ifc_decl,             /* Generated when reading an IFC file to record the
                               IFC index of a class member declaration in the
                               token stream.  This token currently always
                               follows the class member declaration (including
                               the definition, if applicable). */
  tok_unresolved_ud_literal,/* Token used during lexing of a user-defined
                               literal to represent the incomplete token state
                               during calls to find_literal_operator.  This
                               allows cache_curr_token to properly persist the
                               associated incomplete curr_token state if tokens
                               need to be parsed during the call to
                               find_literal_operator (e.g., because modules are
                               loading new user-defined literal operator
                               declarations from token caches).  This token
                               should not appear unless a call to get_token is
                               on the call stack; other appearances should be
                               considered highly suspicious (and a probable
                               bug). */
  tok_unimplemented         /* Token used to indicate keywords that are not
                               yet implemented. */,
  tok_last_complex_token = tok_unimplemented,
  /* Operators (sizeof, new, typeid, etc. appear with keywords): */
  tok_lbracket              /* [ */,
  tok_rbracket              /* ] */,
  tok_lparen                /* ( */,
  tok_rparen                /* ) */,
  tok_period                /* . */,
  tok_arrow                 /* -> */,
  tok_plus_plus             /* ++ */,
  tok_minus_minus           /* -- */,
  tok_ampersand             /* & */,
  tok_star                  /* * */,
  tok_plus                  /* + */,
  tok_minus                 /* - */,
  tok_compl                 /* ~ */,
  tok_not                   /* ! */,
  tok_divide                /* / */,
  tok_remainder             /* % */,
  tok_shift_left            /* << */,
  tok_shift_right           /* >> */,
  tok_lt                    /* < */,
  tok_gt                    /* > */,
  tok_le                    /* <= */,
  tok_ge                    /* >= */,
  tok_eq                    /* == */,
  tok_ne                    /* != */,
  tok_spaceship             /* <=> */,
  tok_caret_caret           /* ^^ */,
  tok_excl_or               /* ^ */,
  tok_or                    /* | */,
  tok_and_and               /* && */,
  tok_or_or                 /* || */,
  tok_quest_mark            /* ? */,
  tok_colon                 /* : */,
  tok_assign                /* = */,
  tok_times_assign          /* *= */,
  tok_divide_assign         /* /= */,
  tok_remainder_assign      /* %= */,
  tok_plus_assign           /* += */,
  tok_minus_assign          /* -= */,
  tok_shift_left_assign     /* <<= */,
  tok_shift_right_assign    /* >>= */,
  tok_and_assign            /* &= */,
  tok_excl_or_assign        /* ^= */,
  tok_or_assign             /* |= */,
  tok_comma                 /* , */,
  tok_sharp                 /* # */,
  tok_paste                 /* ## */,
  /* The min and max operators are only recognized in GNU C++ mode. */
  tok_gnu_min               /* <? */,
  tok_gnu_max               /* >? */,
  /* Punctuators that are not also operators: */
  tok_lbrace                /* { */,
  tok_rbrace                /* } */,
  tok_lsplice               /* [: */,
  tok_rsplice               /* :] */,
  tok_backslash             /* \ */,
  tok_semicolon             /* ; */,
  tok_ellipsis              /* ... */,
  /* Keywords: */
  tok_auto,
  tok_break,
  tok_case,
  tok_char,
  tok_const,
  tok_continue,
  tok_default,
  tok_do,
  tok_double,
  tok_else,
  tok_enum,
  tok_extern,
  tok_float,
  tok_for,
  tok_goto,
  tok_if,
  tok_int,
  tok_bit_precise_int,
  tok_long,
  tok_register,
  tok_return,
  tok_short,
  tok_signed,
  tok_sizeof,
  tok_static,
  tok_struct,
  tok_switch,
  tok_typedef,
  tok_union,
  tok_unsigned,
  tok_void,
  tok_volatile,
  tok_while,
  /* Specific to C99 mode. */
  tok_c99_generic,
  tok_c99_genericfx,
  /* Extensions.  __ALIGNOF__ (and __alignof__, __alignof, or __builtin_alignof
     in some modes) is similar to sizeof, but slightly different from the C++11
     alignof (tok_alignof).  __INTADDR__ is used to scan an integer address
     expression for offsetof): */
  tok_ext_alignof,
  tok_intaddr,
  /* Used when <stdarg.h> is treated as a builtin. */
  tok_va_start, tok_va_arg, tok_va_end, tok_va_copy,
  tok_builtin_offsetof,
  tok_restrict,
  tok_gnu_restrict,
  /* C99 types: _Bool, _Complex and _Imaginary. */
  tok_c99_bool,
  tok_c99_complex,
  tok_c99_imaginary,
  /* Token for __I__, for the C99 imaginary number "i" (i*i == -1). */
  tok_imaginary_unit,
  /* Token for __NAN__, for a Not-a-Number constant (C99 and other modes). */
  tok_nan,
  /* Token for __INFINITY__, for an Infinity constant (C99 and other modes). */
  tok_infinity,
  /* C++11 types: char16_t and char32_t. */
  tok_char16_t,
  tok_char32_t,
  /* C++20 type: char8_t. */
  tok_char8_t,
  /* Tokens for fixed-point type support ("_Fract", "_Accum", and "_Sat"). */
  tok_fract,
  tok_accum,
  tok_sat,
  tok_declspec,
#if MICROSOFT_EXTENSIONS_ALLOWED
  tok_abstract,
  tok_sealed,
  tok_cdecl,
  tok_fastcall,
  tok_stdcall,
  tok_thiscall,
  tok_vectorcall,
  tok_clrcall,
  tok_microsoft_inline,
  tok_forceinline,
  tok_unaligned,
  tok_microsoft_try,
  tok_finally,
  tok_leave,
  tok_except,
  tok_int8,
  tok_int16,
  tok_int32,
  tok_int64,
  tok_based,
  tok_uuidof,
  tok_assume,
  tok_charize,
  tok_if_exists,
  tok_if_not_exists,
  tok_end_of_if_exists,		/* Generated token used by front end. */
  tok_super,
  tok_noop,
  tok_interface,
  tok_event,
  tok_microsoft_ptr32,
  tok_microsoft_ptr64,
  tok_microsoft_sptr,
  tok_microsoft_uptr,
  tok_microsoft_w64,
  tok_microsoft_Lprefix,
  tok_microsoft_lprefix,
  tok_microsoft_Uprefix,
  tok_microsoft_uprefix,
  tok_microsoft_identifier,
  tok_uuid,
  tok_in,
  tok_gcnew,
  tok_safe_cast,
  tok_implements,
  tok_unresolved_type,
  /* Keywords with embedded white space.  All except tok_for_each are only
     in C++/CLI.  These must be in the contiguous range defined by
     tok_first_whitespace_token through tok_last_whitespace_token, as the
     token kinds are used to index the whitespace_keywords array. */
  tok_for_each,
  tok_first_whitespace_token = tok_for_each,
  tok_ref_class,
  tok_ref_struct,
  tok_value_class,
  tok_value_struct,
  tok_enum_class,
  tok_enum_struct,
  tok_interface_class,
  tok_interface_struct,
  tok_ref_new,
  tok_partial_ref_class,
  tok_partial_ref_struct,
  /* Tokens for the first words of whitespace tokens (never returned by
     get_token()). */
  tok_prefix_ref,
  tok_prefix_value,
  tok_prefix_interface,
  /* Used only for the spelling when a line-start modification is needed
     because the scan for the second word moved to a new source line. */
  tok_prefix_for,
  tok_prefix_enum,
  tok_prefix_partial,
  tok_last_whitespace_token = tok_prefix_partial,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  tok_microsoft_asm,
  /* Special constants for various versions of the name of the current
     function, e.g., __func__ from C99, __FUNCTION__ from GNU and Microsoft. */
  tok_func_name,		/* __func__ */
  tok_function_name,		/* __FUNCTION__ */
  tok_pretty_function_name,	/* __PRETTY_FUNCTION__ */
  tok_decorated_function_name,	/* Microsoft __FUNCDNAME__ */
#if NEAR_AND_FAR_ALLOWED
  tok_near,
  tok_far,
#endif /* NEAR_AND_FAR_ALLOWED */
  tok_attribute,
#if GNU_EXTENSIONS_ALLOWED
  tok_builtin_types_compatible,
  tok_gnu_real,
  tok_gnu_imag,
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* C++ tokens not in C (ARM, 2.4): */
  tok_colon_colon       /* :: */,
  tok_period_star       /* .* */,
  tok_arrow_star        /* ->* */,
  tok_asm,
  tok_catch,
  tok_class,
  tok_delete,
  tok_friend,
  tok_inline,
  tok_new,
  tok_operator,
  tok_private,
  tok_protected,
  tok_public,
  tok_template,
  tok_this,
  tok_throw,
  tok_try,
  tok_virtual,
  /* C++ tokens not in the ARM: */
  tok_wchar_t,
  tok_const_cast,
  tok_dynamic_cast,
  tok_explicit,
  tok_cpp98_export,     /* Used for deprecated "export" templates. */
  tok_export,           /* Used for the "export" keyword. */
  tok_export_keyword,   /* Used for export-keyword in the language. */
  tok_import,           /* Used for import-keyword in the language. */
  tok_module,           /* Used for module-keyword in the language. */
  tok_mutable,
  tok_namespace,
  tok_reinterpret_cast,
  tok_static_cast,
  tok_typeid,
  tok_using,
  tok_bool,
  tok_false,
  tok_true,
  tok_typename,
  tok_static_assert,
  tok_decltype,
  /* Recognized in GNU C and C++ modes only. */
  tok_auto_type,
  tok_extension,
  tok_null,
  /* Recognized in GNU C and C23 modes only. */
  tok_typeof,
  /* Recognized in C23 mode only. */
  tok_typeof_unqual,
  /* Recognized in cfront compatibility mode only. */
  tok_overload,
#if SUN_EXTENSIONS_ALLOWED
  /* Recognized in Sun C++ mode only. */
  tok_global_link_scope,
  tok_symbolic_link_scope,
  tok_hidden_link_scope,
#endif /* SUN_EXTENSIONS_ALLOWED */
  tok_thread,
  tok_thread_local,
  tok_c11_thread_local,
#if UPC_EXTENSIONS_ALLOWED
  /* Recognized in UPC mode only. */
  tok_upc_strict,
  tok_upc_relaxed,
  tok_upc_shared,
  tok_upc_forall,
  tok_upc_barrier,
  tok_upc_notify,
  tok_upc_wait,
  tok_upc_fence,
  tok_upc_threads,
  tok_upc_mythread,
  tok_upc_blocksizeof,
  tok_upc_localsizeof,
  tok_upc_elemsizeof,
#endif /* UPC_EXTENSIONS_ALLOWED */
  tok_has_assign,
  tok_has_copy,
  tok_has_nothrow_assign,
  tok_has_nothrow_constructor,
  tok_has_nothrow_copy,
  tok_has_trivial_assign,
  tok_has_trivial_constructor,
  tok_has_trivial_copy,
  tok_has_trivial_destructor,
  tok_has_user_destructor,
  tok_has_virtual_destructor,
  tok_is_abstract,
  tok_is_base_of,
  tok_is_class,
  tok_is_convertible_to,
  tok_is_convertible,
  tok_is_nothrow_convertible,
  tok_is_empty,
  tok_is_enum,
  tok_is_scoped_enum,
  tok_is_pod,
  tok_is_polymorphic,
  tok_is_union,
  tok_is_trivial,
  tok_is_standard_layout,
  tok_is_trivially_copyable,
  tok_is_literal_type,
  tok_has_trivial_move_constructor,
  tok_has_trivial_move_assign,
  tok_has_nothrow_move_assign,
  tok_is_invocable,
  tok_is_nothrow_invocable,
  tok_is_constructible,
  tok_is_nothrow_constructible,
  tok_is_trivially_constructible,
  tok_is_destructible,
  tok_is_nothrow_destructible,
  tok_is_trivially_destructible,
  tok_is_nothrow_assignable,
  tok_is_trivially_assignable,
  tok_is_valid_winrt_type,
  tok_underlying_type,
#if MICROSOFT_EXTENSIONS_ALLOWED
  tok_has_finalizer,
  tok_is_delegate,
  tok_is_interface_class,
  tok_is_ref_array,
  tok_is_ref_class,
  tok_is_sealed,
  tok_is_simple_value_class,
  tok_is_value_class,
  tok_is_win_class,
  tok_is_win_interface,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  tok_nullptr,
#if MICROSOFT_EXTENSIONS_ALLOWED
  tok_native_nullptr,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  tok_internal_alias_decl,
#if INT128_EXTENSIONS_ALLOWED
  tok_int128,
#endif /* INT128_EXTENSIONS_ALLOWED */
  tok_override,
  tok_final,
  tok_is_final,
  tok_noexcept,
  tok_constexpr,
  tok_consteval,
  tok_constinit,
  tok_alignof,
  tok_alignas,
#if GNU_EXTENSIONS_ALLOWED
  /* g++ variadic type operators. */
  tok_bases,
  tok_direct_bases,
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_VECTOR_TYPES_ALLOWED
  tok_builtin_shuffle,
  tok_builtin_shufflevector,
  tok_builtin_convertvector,
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  tok_noreturn,
  tok_builtin_complex,
  tok_c11_generic,
  tok_c11_atomic,
  tok_nullable,
  tok_nonnull,
  tok_null_unspecified,
  tok_coroutine_yield,
  tok_coroutine_return,
  tok_coroutine_await,
  tok_is_assignable,
#if MICROSOFT_EXTENSIONS_ALLOWED
  tok_is_trivially_copy_assignable,
  tok_is_assignable_no_precondition_check,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  tok_builtin_addressof,
  tok_edg_internal_type,
  tok_edg_vector_type,
  tok_edg_neon_vector_type,
  tok_edg_neon_polyvector_type,
  tok_edg_scalable_vector_type,
  tok_edg_size_type,
  tok_edg_ptrdiff_type,
  tok_edg_bool_type,
  tok_edg_wchar_type,
  tok_edg_throw,
  tok_edg_internal_opnd,
  tok_clang_version,
  tok_datasizeof,
  tok_has_unique_object_representations,
  tok_is_aggregate,
  tok_integer_pack,
  tok_reference_binds_to_temporary,
  tok_reference_constructs_from_temporary,
  tok_reference_converts_from_temporary,
  tok_is_same,
  tok_is_same_as,
  tok_is_function,
  tok_requires,
  tok_concept,
  tok_builtin_has_attribute,
  tok_builtin_bit_cast,
  tok_is_layout_compatible,
  tok_is_pointer_interconvertible_base_of,
  tok_is_pointer_interconvertible_with_class,
  tok_builtin_is_pointer_interconvertible_with_class,
  tok_is_corresponding_member,
  tok_builtin_is_corresponding_member,
  tok_edg_is_deducible,
  tok_is_array,
  tok_array_rank,
  tok_array_extent,
  tok_is_arithmetic,
  tok_is_complete_type,
  tok_is_compound,
  tok_is_const,
  tok_is_floating_point,
  tok_is_fundamental,
  tok_is_integral,
  tok_is_lvalue_reference,
  tok_is_member_function_pointer,
  tok_is_member_object_pointer,
  tok_is_member_pointer,
  tok_is_object,
  tok_is_pointer,
  tok_is_reference,
  tok_is_rvalue_reference,
  tok_is_scalar,
  tok_is_signed,
  tok_is_unsigned,
  tok_is_void,
  tok_is_volatile,
  tok_float32,
  tok_float32x,
  tok_float64,
  tok_float64x,
  tok_float128,
  tok_is_bounded_array,
  tok_is_unbounded_array,
  tok_is_referenceable,
  tok_add_lvalue_reference,
  tok_add_pointer,
  tok_add_rvalue_reference,
  tok_decay,
  tok_make_signed,
  tok_make_unsigned,
  tok_remove_all_extents,
  tok_remove_const,
  tok_remove_cv,
  tok_remove_cvref,
  tok_remove_extent,
  tok_remove_pointer,
  tok_remove_reference,
  tok_remove_reference_t,
  tok_remove_restrict,
  tok_remove_volatile,
  tok_is_trivially_equality_comparable,
  tok_nullptr_t,
  tok_is_trivially_relocatable,
  tok_is_bitwise_cloneable,
  tok_builtin_is_virtual_base_of,
  tok_builtin_is_implicit_lifetime,
  tok_builtin_lt_synthesizes_from_spaceship,
  tok_builtin_gt_synthesizes_from_spaceship,
  tok_builtin_le_synthesizes_from_spaceship,
  tok_builtin_ge_synthesizes_from_spaceship,
  tok_builtin_is_structural,
  /* Placeholder for last position in enumeration. */
  tok_last
};

/*
Table of names corresponding to token kinds.
*/
EXTERN_CONSTINIT_ARRAY(a_const_char*, token_names, tok_last + 1)
#if VAR_INITIALIZERS
= {"error", "identifier", "float constant", "fixed-point constant",
   "int constant", "char constant", "generated constant",
   "string literal", "user-defined literal",
   "end of source", "newline", "header name", "pp number", "digit sequence",
   "cpp quote", "ptr to member", "removed expr", "removed template body",
#if MICROSOFT_EXTENSIONS_ALLOWED
   "cli typeid",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   "decltype construct", "pending IFC expression",
   "IFC entity ref", "IFC decl ref", "IFC type ref", "IFC param ref",
   "IFC decl", "unresolved user-defined literal", "unimplemented",
   "[", "]", "(", ")", ".", "->", "++", "--", "&", "*", "+", "-",
   "~", "!", "/", "%", "<<", ">>", "<", ">", "<=", ">=", "==", "!=", "<=>",
   "^^", "^", "|", "&&", "||", "?", ":", "=", "*=", "/=", "%=",
   "+=", "-=", "<<=", ">>=", "&=", "^=", "|=", ",", "#", "##", "<?", ">?",
   "{", "}", "[:", ":]", "\\",  ";", "...",
   "auto", "break", "case", "char", "const",
   "continue", "default", "do", "double", "else", "enum", "extern",
   "float", "for", "goto", "if", "int", "_BitInt", "long", "register",
   "return", "short", "signed", "sizeof", "static", "struct",
   "switch", "typedef", "union", "unsigned", "void", "volatile",
   "while", "__generic", "__genericfx", "__ALIGNOF__", "__INTADDR__",
   "va_start", "va_arg", "va_end", "va_copy",
   "__builtin_offsetof",
   "restrict", "__restrict",
   "_Bool", "_Complex", "_Imaginary", "__I__", "__NAN__", "__INFINITY__",
   "char16_t", "char32_t", "char8_t",
   "_Fract", "_Accum", "_Sat", "__declspec",
#if MICROSOFT_EXTENSIONS_ALLOWED
   "abstract", "sealed",
   "__cdecl", "__fastcall", "__stdcall", "__thiscall", "__vectorcall",
   "__clrcall", "__inline", "__forceinline",
   "__unaligned", "__try", "__finally", "__leave", "__except",
   "__int8", "__int16", "__int32", "__int64", "__based",
   "__uuidof", "__assume", "#@", "__if_exists", "__if_not_exists",
   "end of __if_exists",
   "__super",
   "__noop", "__interface", "__event",
   "__ptr32", "__ptr64", "__sptr", "__uptr", "__w64",
   "__LPREFIX", "__lPREFIX", "__UPREFIX", "__uPREFIX",
   "__identifier", "uuid", "in", "gcnew", "safe_cast",
   "__implements", "__unresolved_type",
   "for each", "ref class", "ref struct", "value class", "value struct",
   "enum class", "enum struct", "interface class", "interface struct",
   "ref new", "partial ref class", "partial ref struct",
   "ref", "value", "interface", "for", "enum", "partial",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   "__asm",
   "__func__",
   "__FUNCTION__",
   "__PRETTY_FUNCTION__",
   "__FUNCDNAME__",
#if NEAR_AND_FAR_ALLOWED
    "__near", "__far",
#endif /* NEAR_AND_FAR_ALLOWED */
   "__attribute__",
#if GNU_EXTENSIONS_ALLOWED
   "__builtin_types_compatible_p",
   "__real", "__imag",
#endif /* GNU_EXTENSIONS_ALLOWED */
   "::", ".*", "->*", "asm", "catch", "class", "delete", "friend",
   "inline", "new", "operator", "private", "protected", "public",
   "template", "this", "throw", "try", "virtual", "wchar_t",
   "const_cast", "dynamic_cast", "explicit",
   "export", "export", "export", "import", "module",
   "mutable", "namespace", "reinterpret_cast", "static_cast", "typeid",
   "using", "bool", "false", "true", "typename", "static_assert", "decltype",
   "__auto_type", "__extension__", "__null", "typeof", "typeof_unqual",
   "overload",
#if SUN_EXTENSIONS_ALLOWED
   "__global", "__symbolic", "__hidden",
#endif /* SUN_EXTENSIONS_ALLOWED */
   "__thread",
   "thread_local",
   "_Thread_local",
#if UPC_EXTENSIONS_ALLOWED
   "strict", "relaxed", "shared", "upc_forall", "upc_barrier", "upc_notify",
   "upc_wait", "upc_fence", "THREADS", "MYTHREAD", "upc_blocksizeof",
   "upc_localsizeof", "upc_elemsizeof",
#endif /* UPC_EXTENSIONS_ALLOWED */
   "__has_assign",
   "__has_copy",
   "__has_nothrow_assign",
   "__has_nothrow_constructor",
   "__has_nothrow_copy",
   "__has_trivial_assign",
   "__has_trivial_constructor",
   "__has_trivial_copy",
   "__has_trivial_destructor",
   "__has_user_destructor",
   "__has_virtual_destructor",
   "__is_abstract",
   "__is_base_of",
   "__is_class",
   "__is_convertible_to",
   "__is_convertible",
   "__is_nothrow_convertible",
   "__is_empty",
   "__is_enum",
   "__is_scoped_enum",
   "__is_pod",
   "__is_polymorphic",
   "__is_union",
   "__is_trivial",
   "__is_standard_layout",
   "__is_trivially_copyable",
   "__is_literal_type",
   "__has_trivial_move_constructor",
   "__has_trivial_move_assign",
   "__has_nothrow_move_assign",
   "__is_invocable",
   "__is_nothrow_invocable",
   "__is_constructible",
   "__is_nothrow_constructible",
   "__is_trivially_constructible",
   "__is_destructible",
   "__is_nothrow_destructible",
   "__is_trivially_destructible",
   "__is_nothrow_assignable",
   "__is_trivially_assignable",
   "__is_valid_winrt_type",
   "__underlying_type",
#if MICROSOFT_EXTENSIONS_ALLOWED
   "__has_finalizer",
   "__is_delegate",
   "__is_interface_class",
   "__is_ref_array",
   "__is_ref_class",
   "__is_sealed",
   "__is_simple_value_class",
   "__is_value_class",
   "__is_win_class",
   "__is_win_interface",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   "nullptr",
#if MICROSOFT_EXTENSIONS_ALLOWED
   "__nullptr",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   "__internal_alias_decl",
#if INT128_EXTENSIONS_ALLOWED
   "__int128",
#endif /* INT128_EXTENSIONS_ALLOWED */
   "override", "final", "__is_final",
   "noexcept",
   "constexpr",
   "consteval",
   "constinit",
   "alignof",
   "alignas",
#if GNU_EXTENSIONS_ALLOWED
   "__bases",
   "__direct_bases",
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_VECTOR_TYPES_ALLOWED
   "__builtin_shuffle",
   "__builtin_shufflevector",
   "__builtin_convertvector",
#endif /* GNU_VECTOR_TYPES_ALLOWED */
   "_Noreturn",
   "__builtin_complex",
   "_Generic",
   "_Atomic",
   "_Nullable",
   "_Nonnull",
   "_Null_unspecified",
   "co_yield", "co_return", "co_await",
   "__is_assignable",
#if MICROSOFT_EXTENSIONS_ALLOWED
   "__is_trivially_copy_assignable",
   "__is_assignable_no_precondition_check",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   "__builtin_addressof",
   "__edg_type__",
   "__edg_vector_type__",
   "__edg_neon_vector_type__",
   "__edg_neon_polyvector_type__",
   "__edg_scalable_vector_type__",
   "__edg_size_type__",
   "__edg_ptrdiff_type__",
   "__edg_bool_type__",
   "__edg_wchar_type__",
   "__edg_throw__",
   "__edg_opnd__",
   "clang version",
   "__datasizeof",
   "__has_unique_object_representations",
   "__is_aggregate",
   "__integer_pack",
   "__reference_binds_to_temporary",
   "__reference_constructs_from_temporary",
   "__reference_converts_from_temporary",
   "__is_same", "__is_same_as",
   "__is_function",
   "requires", "concept",
   "__builtin_has_attribute",
   "__builtin_bit_cast",
   "__is_layout_compatible",
   "__is_pointer_interconvertible_base_of",
   "__is_pointer_interconvertible_with_class",
   "__builtin_is_pointer_interconvertible_with_class",
   "__is_corresponding_member",
   "__builtin_is_corresponding_member",
   "__edg_is_deducible",
   "__is_array",
   "__array_rank",
   "__array_extent",
   "__is_arithmetic",
   "__is_complete_type",
   "__is_compound",
   "__is_const",
   "__is_floating_point",
   "__is_fundamental",
   "__is_integral",
   "__is_lvalue_reference",
   "__is_member_function_pointer",
   "__is_member_object_pointer",
   "__is_member_pointer",
   "__is_object",
   "__is_pointer",
   "__is_reference",
   "__is_rvalue_reference",
   "__is_scalar",
   "__is_signed",
   "__is_unsigned",
   "__is_void",
   "__is_volatile",
   "_Float32",
   "_Float32x",
   "_Float64",
   "_Float64x",
   "_Float128",
   "__is_bounded_array",
   "__is_unbounded_array",
   "__is_referenceable",
   "__add_lvalue_reference",
   "__add_pointer",
   "__add_rvalue_reference",
   "__decay",
   "__make_signed",
   "__make_unsigned",
   "__remove_all_extents",
   "__remove_const",
   "__remove_cv",
   "__remove_cvref",
   "__remove_extent",
   "__remove_pointer",
   "__remove_reference",
   "__remove_reference_t",
   "__remove_restrict",
   "__remove_volatile",
   "__is_trivially_equality_comparable",
   "nullptr_t",
   "__is_trivially_relocatable",
   "__is_bitwise_cloneable",
   "__builtin_is_virtual_base_of",
   "__builtin_is_implicit_lifetime",
   "__builtin_lt_synthesizes_from_spaceship",
   "__builtin_gt_synthesizes_from_spaceship",
   "__builtin_le_synthesizes_from_spaceship",
   "__builtin_ge_synthesizes_from_spaceship",
   "__builtin_is_structural",
   "last" /* used to check that initialization is right. */
  }
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(token_names)

/*
A range of source text, starting at one source position and ending at another.
*/
typedef struct a_source_range {
  a_source_position
		start;
			/* Starting source position of a range of text. */
  a_source_position
		end;
			/* Ending source position of a range of text. */
} a_source_range;

#if EXTRA_SOURCE_POSITIONS_IN_IL || !NULL_POINTER_IS_ZERO

EXTERN_THREAD a_source_range
		null_source_range
#if VAR_INITIALIZERS
                                  = {{0, SP_COL_UNKNOWN
#if FULLY_RESOLVED_MACRO_POSITIONS
                                      , 0, SP_COL_UNKNOWN
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if RECORD_MACRO_INVOCATIONS
                                      , NO_PARENT_MACRO_INVOCATION
#endif /* RECORD_MACRO_INVOCATIONS */
                                     }, {0, SP_COL_UNKNOWN
#if FULLY_RESOLVED_MACRO_POSITIONS
                                         , 0, SP_COL_UNKNOWN
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if RECORD_MACRO_INVOCATIONS
                                         , NO_PARENT_MACRO_INVOCATION
#endif /* RECORD_MACRO_INVOCATIONS */
                                     }}
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* NULL source range, for initialization. */

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL || !NULL_POINTER_IS_ZERO */


enum an_element_position_kind : a_byte {
  epk_error = 0,		/* Error representation. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  epk_specialization_header,	/* "template" keyword in "template<> ...". */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  epk_noreturn,			/* C11 "_Noreturn" position. */
  epk_last
};


/*
An enumeration of vector kinds.
*/
enum a_vector_kind : a_byte {
  vk_gnu,			/* GNU vector type. */
  vk_ext,			/* vector has the ext_vector_type attribute. */
  vk_neon,			/* ARM NEON vector type. */
  vk_neon_poly,			/* ARM NEON polyvector type. */
  vk_neon_builtin,		/* ARM NEON vector type used by some GNU
				   builtins. */
  vk_last
};


/* Structure to record a single position of an element of a construct
   (e.g., the "template" keyword in an explicit template specialization). */
typedef struct an_element_position *an_element_position_ptr;
typedef struct an_element_position {
  an_element_position_ptr
		next;
			/* Pointer to the next element position entry for the
			   construct associated with this entry (or NULL, if
			   there is no additional position entry). */
  a_source_position
		position;
			/* The element's source position. */
  an_element_position_kind
		kind;
			/* The element's kind. */
} an_element_position;

#if EXTRA_SOURCE_POSITIONS_IN_IL

/* Additional source position information relating to the declaration of the
   associated IL entry. */
typedef struct a_decl_position_supplement *a_decl_position_supplement_ptr;
typedef struct a_decl_position_supplement {
  a_source_range
		identifier_range;
			/* If the associated IL entry has an explicitly
			   declared name, the source positions corresponding
			   to the start and end of the sequence of tokens that
			   represent the identifier, as explicitly spelled in
			   the source program (e.g., possibly including
			   qualifiers and a template argument list, if they
			   were specified explicitly).  May be
			   null_source_range. */
  a_source_range
		specifiers_range;
			/* If the declaration of the associated IL entry
			   involves declaration-specifiers, the source
			   positions corresponding to the start and end of the
			   declaration-specifiers of the declaration.  (When
			   the associated entity is an class or enum type,
			   this field corresponds to enum-specifier and
			   class-specifier in the grammar.)  May be
			   null_source_range. */
  union {
    a_source_range
		declarator_range;
			/* If the declaration of the associated IL entry
			   involves a declarator, the source positions
			   corresponding to the start and end of the
			   declarator.  May be null_source_range. */
    a_source_range
		enum_value_range;
			/* When the associated IL entry is an enumerator, the
			   source positions corresponding to the start and
			   end positions of the value expression.  May be
			   null_source_range (when there is no explicitly
			   specified value). */
    a_source_range
		namespace_definition_range;
			/* When the associated IL entry is a namespace, the
			   source positions of the opening and closing
			   braces (or the "=" token and the terminating
			   semicolon if the entry represents a namespace
			   alias). */
  } variant;
  an_element_position_ptr
		extra_positions;
			/* A list of additional positions recorded for a
			   declaration. */
} a_decl_position_supplement;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

/*
The type "pointer-to-name-reference" is used in secondary source sequence
entries.  The complete a_name_reference type is defined later.
*/
typedef struct a_name_reference *a_name_reference_ptr;

/*
The type "pointer-to-attribute" is used in secondary source sequence entries.
The complete an_attribute type is defined later.
*/
typedef struct an_attribute *an_attribute_ptr;

/*
The type of the integer values used to represent the priority of dynamic
initialization using GNU attributes.
*/
typedef unsigned short a_gnu_init_priority;

#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED

/*
ELF visibility kinds (for the GNU "visibility" attribute).
*/
enum an_ELF_visibility_kind : a_byte {
  evk_unspecified,
  evk_hidden,
  evk_protected,
  evk_internal,
  evk_default
};

#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED */

/*
The type "pointer-to-source-sequence-entry" is defined even if the
underlying type is not, since interfaces will use it.
*/
typedef struct a_source_sequence_entry *a_source_sequence_entry_ptr;

/*
A structure containing a kind and a generic pointer to some entity.  Before
it can be used, the pointer must be cast (based on the kind) to a pointer to
a specific entity.
*/
typedef struct a_tagged_pointer {
  an_il_entry_kind
		kind;
			/* The kind of entry. */
  char		*ptr;
			/* A generic pointer to the entry. */
} a_tagged_pointer;


typedef struct an_il_entity_list_entry *an_il_entity_list_entry_ptr;
typedef struct an_il_entity_list_entry {
  /* An entry used to represent an element of a list of (tagged) pointers to
     arbitrary IL entries.  If all the entries are known to be routines, 
     a_routine_list_entry nodes can be used instead.  Similarly, if all entries
     are known to be class types, a_class_list_entry may be more
     appropriate. */
  an_il_entity_list_entry_ptr
                next;
			/* Next in a linked list of entries. */
  a_tagged_pointer
		entity;
			/* Tagged pointer to an IL entity. */
} an_il_entity_list_entry;


/*
Data structure describing a static_assert construct.  Only pointed to from
source sequence entries and stmk_decl statements (via their entities list).
*/
typedef struct a_static_assertion *a_static_assertion_ptr;
typedef struct a_static_assertion {
  a_constant_ptr
		condition;
			/* A constant representing the condition asserted to
			   be true. */
  a_constant_ptr
		string_literal;
			/* A constant representing the string literal to be
			   emitted if the assertion fails.  NULL in the case
			   of a terse static_assert (i.e., one with only a
			   single argument). */
  a_source_position
		position;
			/* The source position of the start of the
			   construct. */
} a_static_assertion;

#if GENERATE_SOURCE_SEQUENCE_LISTS

/*
A entry on a list that represents the order in which declarations,
statements, macros, pragmas, and comments appear within the source program.
There is a list for the file scope and a list for each function scope.
Each entry on the list points to the entity represented, and when that 
entity is a declared entity or a statement, it has a pointer back to 
its source sequence entry.
*/
typedef struct a_source_sequence_entry {
  a_source_sequence_entry_ptr
		next;
			/* Next entry in a doubly linked list; NULL for
			   the last entry on the list. */
  a_source_sequence_entry_ptr
		prev;
			/* Previous entry in a doubly linked list; NULL for
			   the first entry on the list. */
  a_tagged_pointer
		entity;
			/* A struct containing a tag and a generic pointer
			   to the entity (statement, variable, comment,
			   etc.) with which this source sequence entry is
			   associated. */
} a_source_sequence_entry;


/*
A "source sequence secondary declaration entry" is pointed to from the
source sequence list to identify a declaration that is not a "primary"
declaration.  In most cases, definitions are "primary" declarations and
non-defining declarations are "secondary."  There are a few exceptions:
(a) namespace extensions are "definitions," but they are associated with
a secondary source sequence entry; (b) repeated typedefs are "definitions"
(by convention), but they are also associated with a secondary source
sequence entry; (c) uninstantiated member functions of class templates
are not "definitions" in a sense, but they are sometimes associated with
an ordinary source sequence entry (in configurations where source sequence
entries are generated for template instantiations).
*/
typedef struct a_src_seq_secondary_decl *a_src_seq_secondary_decl_ptr;
typedef struct a_src_seq_secondary_decl {
  a_source_position
		decl_position;
			/* Source position of the declaration.  (The source
			   position of the entity itself records where the
			   primary declaration appeared.) */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_decl_position_supplement_ptr
		decl_pos_info;
			/* Points to a block containing additional source
			   position information about the declaration.
			   May be NULL. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_tagged_pointer
		entity;
			/* Entry identifying the kind of entity (type,
			   function, static data member, etc.) for which this
			   is the secondary declaration. */
  a_type_ptr	declared_type;
			/* The type of the entity, as specified in the
			   declaration referred to by this entry; typically,
			   it is the same as the type of the variable or
			   routine to which this entry corresponds, but it
			   needn't be.  It appears on secondary declarations
			   for enum types (where it specifies the base type as
			   written), friends, and typedefs, but NULL for
			   secondary declarations of class, struct, and union
			   types. */
  a_name_reference_ptr
		name_reference;
			/* The form of the declarator used in the declaration
			   referred to by this entry. */
  an_attribute_ptr
		attributes;	
			/* The attributes list specified on this declaration.
			   (Each attribute appertaining to a secondary
			   declaration is recorded twice: Once here and once
			   in the source correspondence entry for the declared
			   entity.  I.e., the entries here are copies of those
			   recorded in "entity" for this declaration.) */
  a_storage_class
		declared_storage_class;
			/* The storage class that explicitly appears in the
			   source for this declaration; sc_unspecified if the
			   declaration has no explicit storage class. */
  a_bit_field	autonomous_tag_decl:1;
			/* If entity refers to a type entry representing a
			   class, struct, union, or enum, this flag is TRUE if
			   the declaration it corresponds to is not part
			   of the declaration of another entity -- i.e., the
			   case of a vacuous declaration:
			     class A;             // autonomous 2ndary decl
			     class A { int i; };  // autonomous primary decl
			     class B { int i; };  // autonomous primary decl
			     class B;             // autonomous 2ndary decl
			   The source sequence entries for the first and
			   fourth of these class declarations will have the
			   flag set. */
  a_bit_field	embedded_source_sequence_entries:1;
			/* TRUE if the declaration represented by this entry
			   embeds another construct with associated source
			   sequence entries.  For example:
			     extern int x[sizeof(struct S { int i; })];
			   In this example, the source sequence entries for the
			   non-autonomous struct S are considered to be
			   "embedded".  In such cases, the embedded entries are
			   followed by an a_src_seq_end_of_construct for the
			   entity associated with this secondary source
			   sequence entry. */
  a_bit_field	friend_decl:1;
			/* TRUE when the declaration is a friend declaration;
			   "entity" will refer to a routine or class.  (In
			   some modes, it may refer to other types as well.) */
  a_bit_field	declared_in_func_prototype:1;
			/* TRUE when the scope of this declaration is a
			   function prototype scope -- e.g.,
			     void f(struct A *);
			   when this is the first declaration of A.  Used in
			   both C and C++, though the interpretation of such
			   declarations differs between the two languages. */
  a_bit_field	specialized_with_new_syntax:1;
			/* TRUE if the "template<>" syntax was used to declare
			   a specialization. */
  a_bit_field	first_declaration:1;
			/* TRUE if the declaration is the initial appearance
			   of an entity in the translation unit; defined only
			   for entries referring to class or enum types, to
			   routines, and to variables. */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_bit_field	is_partial_instantiation:1;
			/* TRUE if this entry represents the partial
			   instantiation of a template. */
  a_bit_field	compiler_generated_forward_decl:1;
			/* TRUE if this entry represents a forward declaration
			   generated by the compiler (for possible use in a
			   template argument of a specialization inserted to
			   represent a template instantiation). */
  a_bit_field	originally_nonautonomous_definition:1;
			/* TRUE if this entry was created to represent the
			   partial instantiation of a nested class definition
			   that was originally non-autonomous.  The partial
			   instantiation must be treated as autonomous, because
			   non-autonomous declarations that are not definitions
			   are injected in the surrounding namespace scope.
			   E.g.:
			     template<class T> struct S { struct N {} *p; };
			     S<int> s;
			   The source sequence entries for S<int> correspond
			   to a specialization
			     template<> struct S<int> { struct N; N *p; };
			   and not
			     template<> struct S<int> { struct N *p; };
			   since N would be ::N instead of S<int>::N with the
			   latter.  This flag is set for N in this case so
			   that a later full instantiation can identify where
			   the source sequence entries for the S<int>::N
			   definition should be inserted. */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  a_bit_field	marked_as_gnu_extension:1;
			/* TRUE if the corresponding declaration was preceded
			   by the GNU keyword __extension__. */
  a_bit_field	is_decl_after_first_in_comma_list:1;
			/* This declaration appeared in a comma-separated
			   declarator list and was not the first in that list.
			   E.g., "j" in "extern int i, j;".  */
  a_bit_field	explicit_storage_class:1;
			/* This declaration included an explicit storage class.
			   Used by the C++-generating back end to avoid
			   rendering "int f(), n;" as "extern int f(), n;"
			   (or vice versa). */
  a_bit_field	is_alias:1;
			/* TRUE if this declaration is for a typedef declared
			   using the alias syntax; e.g., "using T = int;". */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_event_interface:1;
			/* TRUE if this declaration is for an "__event
			   __interface". */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} a_src_seq_secondary_decl;


/*
A source sequence end of construct entry is pointed to from a source sequence
entry to mark the end of a class or enum definition, the end of a block, the
end of a for-init declaration, etc.  The kind of construct is determined by
examining the tagged pointer.
*/
typedef struct a_src_seq_end_of_construct *a_src_seq_end_of_construct_ptr;
typedef struct a_src_seq_end_of_construct {
  a_source_position
		position;
			/* Normally, the source position of the tok_rbrace
			   or tok_rparen that marks the end of the construct;
			   for for-init declarations, the source position of
			   the token following the for-init statement. */
  a_tagged_pointer
		entity;
			/* Entry identifying the entity (a class or enum type
			   or a block or decl statement) for which
			   this is the terminating token. */
} a_src_seq_end_of_construct;


/*
Header for a sublist of file-scope source sequence entries in the midst of
a function-scope source sequence list.

  IL scope ---------->sublist------------------------->sublist...
  entry for            entry--->src-seq<-->src-seq...   entry--->src-seq...
  function               ^                                ^
        |                |                                |
        --->src-seq<-->src-seq<-->src-seq<-->src-seq<-->src-seq...
                       (parent)                         (parent)

The sublist entry is pointed to by a source sequence entry from the
function scope list (the "sublist-parent").  It in turn points to source
sequence entries (allocated in the file scope memory region) that may be
thought of as successors of the sublist-parent; similarly, the successor of
the sublist's final source sequence entry would be sublist-parent->next.
(The function-scope source sequence list is logically a single list but is
actually discontinuous, with sublist branches, because entities allocated
in the file-scope memory region cannot have pointers into a function-scope
memory region.  Similarly, there is no pointer back from the sublist
header to its parent since the former is allocated in file-scope memory and
the latter resides in function-scope memory.)  There can be any number of
sublists in a given function's source sequence list; the headers are linked
together in a list pointed to from the function's scope entry.
*/
typedef struct a_src_seq_sublist *a_src_seq_sublist_ptr;
typedef struct a_src_seq_sublist {
  a_src_seq_sublist_ptr
		next;
			/* Pointer to the next sublist of file-scope source
			   sequence entries in the current function scope;
			   NULL for the end of the list. */
  a_source_sequence_entry_ptr
		source_sequence_list;
			/* A doubly-linked list of source sequence entries
			   representing contiguous occurrences of file-scope
			   entities (typically, declarations of entities
			   that are allocated in the file-scope memory region)
			   within the current function scope.  Never NULL. */
  a_source_sequence_entry_ptr
		last_source_sequence_entry;
			/* Last in the linked list of source sequence entries
			   that are pointed to by this entry. */
} a_src_seq_sublist;


/*
Entry describing a template instantiation directive.
*/
typedef struct an_instantiation_directive *an_instantiation_directive_ptr;
typedef struct an_instantiation_directive {
  a_source_position
		position;
			/* Source position of the "template" keyword in the
			   instantiation directive. */
  a_tagged_pointer
		entity;
			/* Entry identifying the entity (a class, function,
			   or static data member) specified in the template
			   instantiation directive. */
  a_byte_boolean
		do_not_instantiate;
			/* TRUE if the instantiation directive was used to
			   indicate that the entity named should not be
			   instantiated.  This is used in modes in which the
			   "template" keyword in an instantiation directive
			   may be prefixed with "extern" to indicate that the
			   instantiation of an entity should be suppressed. */
  an_attribute_ptr
		attributes;
			/* Attributes specified explicitly in the instantiation
			   directive (as opposed to the attributes specified
			   on the template being instantiated). */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_type_ptr	declared_type;
			/* The type as it actually appears in the template
			   instantiation directive. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_decl_position_supplement_ptr
		decl_pos_info;
			/* Points to a block containing additional source
			   position information about the instantiation
			   directive.  May be NULL. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} an_instantiation_directive;

#if GENERATE_LINKAGE_SPEC_BLOCKS

/*
Data structure describing a braced linkage specifier.  Only pointed to from
source sequence entries.
*/
typedef struct a_linkage_spec_block *a_linkage_spec_block_ptr;
typedef struct a_linkage_spec_block {
  a_constant_ptr
		name_string;
			/* A constant representing the string-literal in the
			   construct. */
  ENUM_TYPE_FOR_BIT_FIELD(a_name_linkage_kind)
		name_linkage:NUM_BITS_FOR_NAME_LINKAGE;
			/* The name linkage associated with this construct. */
  a_source_position
		position;
			/* The source position of the start of the
			   construct. */
  a_source_position
		end_position;
			/* The source position of the end of the construct
			   (i.e., the position of the closing brace). */
} a_linkage_spec_block;

#endif /* GENERATE_LINKAGE_SPEC_BLOCKS */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES

/*
Entry describing a Microsoft __if_exists (or __if_not_exists) block.
An entry is created at the start and end of the block.  Note that
although the Microsoft documentation describes the contents of an
__if_exists block as a statement, the Microsoft compiler actually
permits any fragment of a statement or declaration to be specified in
the block.  Our emulation of __if_exists in the front end permits this
usage too.  However, when source sequence entries are created for
__if_exists blocks, the starting and ending markers are recorded in
the source sequence list, which means that the C++-generating back end
can only correctly recreate an __if_exists from IL in cases where the
the __if_exists block contains a complete statement or declaration.
At this time, __if_exists entries are only created for uses that appear
in class definitions.  Most other uses (at least in the Microsoft headers)
do not conform to the rules described above.  If an __if_exists appears in
a class in a way that violates the rules above, an error is issued.

__if_exists entries are only created for __if_exists blocks in which
the tested entity is a dependent name.  When the __if_exists is not
dependent, it is simply evaluated during the prototype instantiation
(and any actual instantiations).
*/
typedef struct an_ms_if_exists *an_ms_if_exists_ptr;
typedef struct an_ms_if_exists {
  an_ms_if_exists_ptr
		next;
                        /* Pointer to the next entry in a given scope.
                           NULL if this the last attribute in the scope. */
  a_tagged_pointer
		entity;
			/* The entity whose existence is being tested.  Note
			   that a test of a dependent name such as T::X will
			   result in the creation of a nonreal member X of T,
			   so an entity will be exist for this entry to point
			   to in such cases.  This pointer will be NULL in the
			   entry for the end of the block.  This is also NULL
			   when the is_this flag is TRUE. */
  a_source_position
		position;
			/* The position of the start of the __if_exists
			   block, or the position of the closing brace if
			   this entry marks the end of the block. */
  a_name_reference_ptr
		name_reference;
			/* The form of the identifier used.  NULL when
			   is_this is TRUE. */
  a_byte_boolean
		is_if_exists;
			/* TRUE if this an __if_exists, FALSE if it is
			   an __if_not_exists.  This field is only set
			   for the entry that records the start of the
			   __if_exists. */
  a_byte_boolean
		pending;
			/* TRUE if the closing brace of the block has not yet
			   been encountered.  This is used by the front end,
			   and is only used for entries for the start of a
			   block. */
  a_byte_boolean
		is_this;
			/* TRUE if the "this" keyword appeared instead of
			   an identifier. */
} an_ms_if_exists;

#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */


typedef union a_parent_class_or_namespace *a_parent_class_or_namespace_ptr;
typedef union a_parent_class_or_namespace {
  /* This structure is used to indicate class or namespace membership and
     is incorporated into a_source_correspondence, a_symbol, and
     a_symbol_locator, each of which has an is_class_member flag.  When
     is_class_member is TRUE, the class_type pointer may be assumed to be
     non-NULL.  When it is FALSE, the entity may or may not be a direct
     namespace member, depending on whether namespace_ptr is non-NULL.
     a_name_qualifier also uses it, but the selector field is called
     is_class there. */
  /* When is_class_member is TRUE: */
  a_type_ptr	class_type;
			/* Pointer to the class of which this entry is a
			   member; in C a pointer to the struct/union in
			   which the field was defined. */
  /* When is_class_member is FALSE. */
  a_namespace_ptr
		namespace_ptr;
			/* If the entry is an immediate member of a namespace,
			   a pointer to the latter (C++ only); otherwise,
			   NULL. */
} a_parent_class_or_namespace;

#if ONE_INSTANTIATION_PER_OBJECT

/*
Entry used to represent a segment of the bit vector of "needed" flags
for individual instantiations.  A list of these represents the entire bit
vector.  Each instantiation is assigned a bit number in the vector
(see the field instantiation_needed_bit_number; actually, it's two bits,
with the second used for the definition_needed flag for classes) and all
entities referenced from that instantiation will have the associated bit
of the bit vector set to 1.
*/
#define BYTES_PER_INSTANTIATION_NEEDED_FLAG_ENTRY 12
typedef struct a_per_instantiation_needed_flags_entry
              *a_per_instantiation_needed_flags_entry_ptr;
typedef struct a_per_instantiation_needed_flags_entry {
  a_per_instantiation_needed_flags_entry_ptr
		next;
			/* Pointer to the next entry on the list, or NULL
			   if this is the last entry. */
  a_byte	bytes[BYTES_PER_INSTANTIATION_NEEDED_FLAG_ENTRY];
			/* Part of the bit vector.  The least-significant
			   bit of bytes[0] is the first bit; the most-
			   significant bit of bytes[0] is the CHAR_BIT-th bit;
			   the least-significant bit of bytes[1] is the
			   CHAR_BIT+1-th bit; etc. */
} a_per_instantiation_needed_flags_entry;

#endif /* ONE_INSTANTIATION_PER_OBJECT */


/*
Entry used to represent the qualifier portion of a qualified name.
*/
typedef struct a_name_qualifier *a_name_qualifier_ptr;
typedef struct a_name_qualifier {
  a_name_qualifier_ptr
		next;	/* Pointer to the next name qualifier entry for
			   a given "qualifier" value.  This is used to
			   find a previously allocated entry that matches
			   a given form of reference.  Used only in the
			   front end. */
  a_parent_class_or_namespace
		qualifier;
			/* Pointer to the class or namespace pointer, if
			   any.  If the qualifier was specified using a
			   typedef or template parameter name the class
			   pointer can actually point to a type entry for
			   a typedef or cv-qualified typeref.  In Microsoft
			   mode it may also point to an enum type. */
  a_name_qualifier_ptr
		previous_qualifier;
			/* Pointer to the previous portion of the qualifier,
			   if any, i.e., the parent qualifier.  NULL if
			   this is the first/topmost qualifier. */
  a_const_char	*name;
			/* The string that specifies the identifier used for
			   this qualifier.  This is usually the same as the
			   name specified by "qualifier", but when the
			   qualifier was specified by a template parameter,
			   the qualifier will be the type of the template
			   argument, while "name" will be the name of the
			   template parameter. */
  a_bit_field	is_class:1;
			/* TRUE if the qualifier is a class, FALSE if it
			   is a namespace. */
} a_name_qualifier;


/*
An enumeration of C++ special function kinds.  These may be user written
or compiler generated functions for which special rules may apply.  (See
ARM chapter 12.)
*/
enum a_special_function_kind : a_byte {
  sfk_none,		/* Not a special function. */
  sfk_constructor,	/* A constructor. */
  sfk_destructor,	/* A destructor. */
  sfk_conversion,	/* A conversion operator function. */
  sfk_udl_operator,	/* A literal operator function. */
  sfk_operator,		/* Any other operator function. */
  sfk_lambda_entry_point,
			/* A static member representing an alternative entry
			   point for the invocation of a lambda with no capture
			   fields.  (A pointer to this entry point is returned
			   by the conversion function declared in such a
			   lambda.) */
  sfk_deduction_guide,
			/* A routine entry representing a C++17 deduction
			   guide. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  sfk_static_constructor,
			/* A C++/CLI static constructor. */
  sfk_finalizer,	/* A C++/CLI finalizer. */
  sfk_idisposable_dispose,
			/* A compiler-generated implementation of the
			   IDisposable::Dispose() member. */
  sfk_dispose_bool,	/* A compiler-generated Dispose(bool) member. */
  sfk_object_finalize,	/* A compiler-generated overrider for the
			   Object::Finalize() member. */
  sfk_property_get,	/* A "get" accessor function of a C++/CLI property. */
  sfk_first_accessor = sfk_property_get,
  sfk_property_set,	/* A "set" accessor function of a C++/CLI property. */
  sfk_event_add,	/* An "add" accessor function of a C++/CLI event. */
  sfk_event_remove,	/* A "remove" accessor function of a C++/CLI event. */
  sfk_event_raise,	/* A "raise" accessor function of a C++/CLI event. */
  sfk_last_accessor = sfk_event_raise,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if BUILTIN_FUNCTIONS_ENABLED
  sfk_gnu_sync_concrete_function,
			/* The concrete version of a GNU __sync_... or
			   __atomic_... builtin function, except the
			   __atomic_..._n functions (see next).  Used only
			   in enk_routine expression nodes, never as the
			   special_kind in an a_routine entry. */
  sfk_gnu_atomic_nongeneric_function,
			/* Like sfk_gnu_sync_concrete_function except
			   representing a GNU __atomic_..._n function
			   (__atomic_load_n, etc.).  These are treated
			   separately to allow the original source form to
			   be accurately determined. */
  sfk_gnu_atomic_generic_function,
			/* Represents a generic GNU __atomic_... function.
			   Generic functions have an initial size_t argument
			   added by the front end. */
  sfk_builtin_operator_new,
			/* Represents __builtin_operator_new (which has been
			   replaced by operator new). */
  sfk_builtin_operator_delete,
			/* Represents __builtin_operator_delete (which has been
			   replaced by operator delete). */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
  sfk_last		/* Must be last. */
};


/*
Entry used to represent the form of name used to refer to an entity.
*/
typedef struct a_name_reference {
  a_name_reference_ptr
		next;	/* Pointer to the next name reference entry for a
			   given entity.  This is used to find a previously
			   allocated entry that matches a given form of
			   reference. */
  a_name_qualifier_ptr
		qualifier;
			/* Points to a description of the class or namespace
			   qualifier portion of the name.  NULL if there is
			   no such qualifier. */
  union {
    /* When special_kind is sfk_none: */
    a_type_ptr	destructor_type;
			/* If the name refers to a destructor name in which
			   the name after the "~" is not the same as the class
			   name, this points to the type of the identifier
			   after the "~".  This also points to the type when
			   a type keyword is used.  NULL otherwise. */
#if MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING
    /* When special_kind is sfk_property_get, sfk_property_set, or
       sfk_event_raise. */
    a_property_or_event_descr_ptr
		property_or_event_descr;
			/* If this entry is for an enk_routine node that is
			   part of a call to a Microsoft property accessor
			   (C++/CLI or __declspec) that was rewritten from a
			   reference to a property field, this points to the
			   descriptor for that property.  It is otherwise
			   NULL. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && !DO_IL_LOWERING */
  } variant;
  long		num_template_arguments;
			/* If is_template_id is TRUE, this is the number of
			   template arguments used in the last component of
			   the name; -1L otherwise. */
  a_template_arg_ptr
		orig_template_arg_list;
			/* Points to the template argument list as originally
			   written. */
  a_special_function_kind
		special_kind;
			/* If this entry is for an enk_routine node that is
			   part of a call to a Microsoft property accessor
			   (C++/CLI or __declspec) that was rewritten from a
			   reference to a property field, this is set to either
			   sfk_property_set or sfk_property_get to reflect the
			   kind of access; it is sfk_gnu_sync_concrete_function
			   if the node designates the concrete version of a GNU
			   __sync_...  or __atomic_... builtin function;
			   otherwise, it is sfk_none. */
  a_bit_field	is_global_qualified_name:1;
			/* TRUE if the name begins with a unary "::"
			   (e.g., ::y or ::A::x). */
  a_bit_field	is_template_id:1;
			/* TRUE if the name is a template-id
			   (i.e., template-name < template-arg-list >).
			   This applies to the last component in the name. */
  a_bit_field	is_super_qualified:1;
			/* TRUE if the name is prefixed by the Microsoft
			   __super keyword (e.g., __super::x). */
  a_bit_field	is_decltype_qualified:1;
			/* TRUE if a qualified name began with a decltype
			   specifier. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_bit_field	used_in_primary_declarator:1;
			/* TRUE if the primary declaration specified the
			   name in this form. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_bit_field	from_prototype_instantiation:1;
			/* TRUE if this name reference entry was created
			   in a prototype instantiation context.  Such
			   entries can have qualifiers that refer to
			   prototype instantiations and so cannot be written
			   to an IL file unless prototype instantiations are
			   included in the IL. */
} a_name_reference;

EXTERN_THREAD a_name_reference null_name_reference
#if VAR_INITIALIZERS
= {NULL} /*lint !e785*/
#endif /* VAR_INITIALIZERS */
;

typedef struct an_attribute_group *an_attribute_group_ptr;
typedef struct an_attribute_group {
  /* Structure to represent an attribute group.  E.g., [[noreturn]] or
     [[noreturn, final]] in C++11, or __attribute((noreturn)) in GNU modes.
     Note that attribute groups do not appear on lists, nor are they pointed
     to directly by entities to which they apply.  Instead, the entities
     point to the attributes contained by the group, and those attributes
     point to a group.  This is expected to be the most convenient approach
     for most applications. */
  a_source_position
		position;
			/* The source position of the first token of the
			   attribute group construct (e.g., the first '['
			   for the standard attribute syntax or the
			   '__attribute' token for the GNU syntax). */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		end_position;
			/* The end position of the last token of the attribute
			   group construct (e.g., the last ']' in the standard
			   attribute syntax). */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} an_attribute_group;


enum an_attribute_arg_kind : a_byte {
  /* An attribute argument entry can represent several kinds of arguments. */
  aak_empty,		/* If an attribute has an empty argument list, that
			   list is represented by a single aak_empty entry.
			   (E.g., __attribute(( nonnull() )) has an aak_empty
			   argument, but __attribute((nonnull)) has no
			   attribute arguments at all.)  An aak_empty entry
			   is also appended after a sequence of aak_raw_token
			   entries. */
  aak_raw_token,	/* Raw tokens are used for unrecognized attributes in
			   particular, and can include commas.  E.g., (z, =)
			   could be represented with three raw tokens: "z",
			   "," and "=".  A sequence of raw tokens is terminated
			   by an aak_empty entry. */
  aak_token,		/* A single token argument.  E.g., (z, =) could be
			   represented with two tokens: "z" and "=". */
  aak_constant,		/* A constant argument. */
  aak_type,		/* A type argument. */
  aak_expression,	/* An expression argument. */
  aak_last
};


typedef struct an_attribute_arg *an_attribute_arg_ptr;
typedef struct an_attribute_arg {
  /* Structure to represent the arguments of an attribute.  Each argument
     corresponds to one entry, but an entry is also produced for an empty
     argument list ("()" as opposed to ""). */
  an_attribute_arg_ptr
		next;
			/* Next in a linked list of attribute arguments. */
  a_token_kind
		token_kind;
			/* For aak_token or aak_raw_token entries, the token
			   kind that was scanned.  Otherwise, tok_last. */
  an_attribute_arg_kind
		kind;
			/* The kind of argument this represents. */
  a_bit_field	is_pack_expansion:1;
			/* TRUE if the argument is a variadic template pack
			   expansion, i.e., it's followed by "...". */
  a_bit_field	local_expr_ref:1;
			/* TRUE if the expression normally associated with
			   an aak_expression is stored in a function scope
			   memory region while the attribute is stored in the
			   file scope memory region.  In that case,
			   variant.expr will be NULL and the expression
			   can be found using expr_node_from_attribute_arg. */
  struct a_pack_expansion_descr
		*pack_expansion_descr;
			/* If non-NULL, the attribute value is a pack
			   expansion, and this points to the expansion
			   description. */
  a_source_position
		position;
			/* The source position of the argument. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		end_position;
			/* The position of the end of the argument. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  union {
    /* When kind == aak_empty: no variant fields. */
    /* When kind == aak_token or aak_raw_token: */
    a_const_char
		*token; /* The character sequence forming the argument
			   token. */
    /* When kind == aak_constant: */
    a_constant_ptr
		constant;
			/* The argument constant. */
    /* When kind == aak_type: */
    a_type_ptr
		type;	/* The argument type. */
    /* When kind == aak_expression and local_expr_ref is FALSE: */
    an_expr_node_ptr
                expr;	/* The argument expression (when the expression is in
			   the file scope).  Use expr_node_from_attribute_arg
			   to access the expression (to be agnostic to the
			   value of local_expr_ref). */
    /* When kind == aak_expression and local_expr_ref is TRUE: */
    a_scoped_expression_ptr
                sexpr;	/* A dummy IL entry with an initial
			   a_source_correspondence field that can be used as a
			   key to retrieve the local-expr-node reference to the
			   expression because the expression resides in a
			   function memory region.  Always use
			   expr_node_from_attribute_arg to retrieve the value
			   of the expression. */
  } variant;
} an_attribute_arg;


enum an_attribute_family : a_byte {
  /* Attributes can be specified using different syntactical constructs.  Each
     construct kind corresponds to a "family" of attributes. */
  af_internal,		/* To annotate IL properties that do not come from an
			   attribute-like construct.  E.g., on a template this
			   might reflect the effect of a #pragma directive. */
  af_std,		/* An attribute specified using the standard C++11
			   or C23 syntax [[ ... ]]. */
  af_gnu,		/* An attribute specified using the GNU __attribute
			   syntax.  (The GNU syntax is emulated by other
			   compilers, including Sun's.) */
  af_ms_declspec,	/* An attribute specified using the Microsoft
			   __declspec construct. */
  af_alignas,		/* The C++11 attribute-like construct "alignas". */
  af_has_attribute,	/* An attribute synthesized for the purposes of the
			   __has_cpp_attribute and __has_c_attribute macro
			   operators (where the attribute family is not
			   known by the context). */
  af_last
};


/*
An enumeration describing where (syntactically) in a construct an attribute
was encountered.
*/
enum an_attribute_location : a_byte {
  al_implicit,		/* The attribute did not appear explicitly in the
			   source. */
  al_prefix,		/* The attribute is the first element of a declaration
			   (including an empty declaration), using-directive,
			   label, or statement. */
  al_tag_name,		/* The attribute appears after "enum", "struct",
			   "union", or "class" but before the definition of
			   that associated entity. */
  al_post_tag_definition,
			/* The attribute appears after a class definition
			   (possible only with GNU attributes). */
  al_base_specifier,	/* The attribute appears in a base class specifier or
			   in an explicit enum base type specifier. */
  al_specifier,		/* The attribute is part of the declaration
			   specifiers. */
  al_declarator_id,	/* The attribute is directly associated with the
			   declarator-id. */
  al_post_ptr_or_ref,	/* The attribute immediately follows a pointer,
			   reference, or pointer-to-member declarator
			   operator.  (Standard attributes only.) */
  al_post_array,	/* The attribute immediately follows an array
			   declarator.  (Standard attributes only.) */
  al_post_func,		/* The attribute immediately follows a function
			   declarator.  (Standard attributes only.) */
  al_postfix,		/* The attribute follows the top-level declarator.
			   (GNU attributes only; al_postfix attributes have
			   the same effect as al_declarator_id attributes.) */
  al_predeclarator,
			/* The attribute appeared as the first construct of a
			   nested declarator (GNU attributes only).  E.g.:
			      int (__attribute((cdecl)) **pf)();
			   Such an attribute applies to the type "underneath"
			   the declarator (type "int()" in this example). */
  al_id_equivalent,	/* The attribute appeared in an unusual place, but
			   is treated as if it were a declarator-id attribute.
			   (GNU attributes only.)  E.g.,
			       int* __attribute((weak)) f() { return 0; }
			   In this example, the attribute couldn't appear as
			   a postfix attribute (because GCC doesn't allow
			   postfix attributes on function definitions).  In
			   cases with multiple declarators, a prefix attribute
			   would not be appropriate either. */
  al_trailing_return,	/* The attribute is the first element of a trailing
			   return type. */
  al_post_initializer,	/* The attribute follows a parenthesized initializer
			   (allowed in some GNU C++ modes only). */
  al_namespace,		/* The attribute appears on a namespace definition
			   (for standard attributes). */
  al_gnu_namespace,	/* The attribute appears on a namespace definition
			   (for GNU attributes). */
  al_label,		/* The attribute follows a label name in a label
			   declaration. */
  al_explicit,		/* This value is never recorded in attribute entries,
			   but it is passed into some routines to indicate
			   that an operation should apply to all non-implicit
			   attributes. */
  al_enumerator,	/* The attribute follows an enumerator identifier. */
  al_id_equivalent_as_postfix,
			/* This value is never recorded in attribute entries,
			   but it is used by the C++-generating back end to
			   match al_id_equivalent attributes when they are
			   being generated in the postfix position. */
  al_builtin_has_attribute,
			/* The attribute in a call to the GCC
			   __builtin_has_attribute builtin. */
  al_module,		/* The attribute follows a module-name in a module-
			   import-declaration.  (Standard attributes only.) */
  al_post_using_declarator,
			/* The attribute follows a using-declarator.  This is
			   non-standard and is used only for the Clang
			   using_if_exists attribute currently. */
  al_lambda_expression,	/* The attribute appears in a lambda-expression prior
			   to the lambda-declarator (valid in C++23 and recent
			   GNU and Clang emulation modes).  These attributes
			   appertain to the operator call function or operator
			   template. */
  al_last
};


/* When adding attributes here, also update disp_attribute. */
enum an_attribute_kind : a_byte {
  ak_unrecognized,	/* For unrecognized attributes. */
  ak_empty_attr,	/* A pseudo-attribute marking the presence of an empty
			   attribute.  Usually this appears in entirely empty
			   groups (like [[]] in C++11), but in GNU modes, a
			   group can contain multiple empty attributes (e.g.,
			   __attribute((,,,)) ). */
  ak_attr_using_prefix, /* A pseudo-attribute marking the presence of a
                           "using" prefix that specifies the attribute
                           namespace to be used as the implicit namespace for
                           each attribute that following in the list (std). */

  /* Standard attributes (some of which also have GNU and/or Microsoft
     variants). */
  ak_align,		/* "align" (std, ms) or "aligned" (gnu).  Also used
			   for the C++11 alignas construct. */
  ak_assume,		/* "assume" (std, C++23). */
  ak_base_check,	/* "base_check" (std). */
  ak_carries_dependency,
			/* "carries_dependency" (std). */
  ak_deprecated,	/* "deprecated" (std, gnu, ms). */
  ak_final,		/* "final" (std). */
  ak_hiding,		/* "hiding" (std). */
  ak_known_semantics,	/* "known_semantics" (std) "msvc" Microsoft
			   mode only. */
  ak_noreturn,		/* "noreturn" (std, gnu, ms) or "volatile" (gnu). */
  ak_override,		/* "override" (std). */
  ak_nodiscard,		/* "nodiscard" (std). */
  ak_noop_dtor, 	/* "noop_dtor" (std) "msvc" Microsoft mode only. */
  ak_maybe_unused,	/* "maybe_unused" (std). */
  ak_fallthrough,	/* "fallthrough" (std). */
  ak_likely,		/* "likely" (std). */
  ak_unlikely,		/* "unlikely" (std). */
  ak_no_unique_address,	/* "no_unique_address" (std). */
  ak_indeterminate,	/* "indeterminate" (std). */

  /* Nonstandard attributes that do not require specific configuration
     flags. */
  ak_enable_if,		/* "enable_if" (clang). */
  ak_overloadable,	/* "overloadable" (clang). */
  ak_pass_object_size,	/* "pass_object_size" (clang). */
  ak_diagnose_if,	/* "diagnose_if" (clang). */
  ak_unavailable,	/* "unavailable" (gnu, clang). */

#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  /* Nonstandard attributes available in both GNU and Microsoft
     configurations. */
#if GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  ak_naked,		/* "naked" (gnu, ms). */
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
  ak_noinline,		/* "noinline" (gnu, ms). */
  ak_nothrow,		/* "nothrow" (gnu, ms). */
  ak_pure,		/* "pure" (gnu, ms). */
  ak_section,		/* "section" (gnu) or "allocate" (ms). */
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */

#if GNU_EXTENSIONS_ALLOWED
  /* GNU-only attributes. */
  ak_alias,		/* "alias" (gnu). */
  ak_alloc_size,        /* "alloc_size" (gnu). */
  ak_always_inline,	/* "always_inline" (gnu). */
  ak_artificial,        /* "artificial" (gnu). */
#if GNU_X86_ATTRIBUTES_ALLOWED
  ak_cdecl,		/* "cdecl" (gnu). */
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
  ak_cleanup,		/* "cleanup" (gnu). */
  ak_cold,		/* "cold" (gnu). */
  ak_common,		/* "common" (gnu). */
  ak_const,		/* "const" (gnu). */
  ak_constructor,	/* "constructor" (gnu). */
  ak_destructor,	/* "destructor" (gnu). */
  ak_error,		/* "error" (gnu). */
#if GNU_VECTOR_TYPES_ALLOWED
  ak_ext_vector_type,	/* "ext_vector_type" (clang). */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  ak_externally_visible,
			/* "externally_visible" (gnu). */
#if GNU_X86_ATTRIBUTES_ALLOWED
  ak_fastcall,		/* "fastcall" (gnu). */
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
  ak_flatten,		/* "flatten" (gnu). */
  ak_format,		/* "format" (gnu). */
  ak_format_arg,	/* "format_arg" (gnu). */
  ak_gnu_inline,	/* "gnu_inline" (gnu). */
  ak_hot,		/* "hot" (gnu). */
  ak_ifunc,		/* "ifunc" (gnu). */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  ak_init_priority,	/* "init_priority" (gnu). */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  ak_internal_linkage,  /* "internal_linkage" (clang). */
  ak_malloc,		/* "malloc" (gnu). */
  ak_may_alias,		/* "may_alias" (gnu). */
  ak_mode,		/* "mode" (gnu). */
  ak_no_instrument_function,
			/* "no_instrument_function" (gnu). */
  ak_no_check_memory_usage,
			/* "no_check_memory_usage" (gnu). */
  ak_nocommon,		/* "nocommon" (gnu). */
  ak_nonnull,		/* "nonnull" (gnu). */
  ak_noplt,		/* "noplt" (gnu). */
  ak_packed,		/* "packed" (gnu). */
  ak_sentinel,		/* "sentinel" (gnu). */
#if GNU_X86_ATTRIBUTES_ALLOWED
  ak_stdcall,		/* "stdcall" (gnu). */
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
  ak_strong,		/* "strong" (gnu). */
  ak_target,		/* "target" (gnu) function multiversioning. */
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  ak_tls_model,		/* "tls_model" (gnu). */
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
  ak_transparent_union,	/* "transparent_union" (gnu). */
  ak_unused,		/* "unused" (gnu). */
  ak_used,		/* "used" (gnu). */
#if GNU_VECTOR_TYPES_ALLOWED
  ak_vector_size,	/* "vector_size" (gnu). */
  ak_neon_vector_type,	/* "neon_vector_type" (clang). */
  ak_neon_polyvector_type,
			/* "neon_polyvector_type" (clang). */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  ak_visibility,	/* "visibility" (gnu). */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  ak_warn_unused_result,
			/* "warn_unused_result" (gnu). */
  ak_warning,		/* "warning" (gnu). */
  ak_weak,		/* "weak" (gnu). */
  ak_weakref,		/* "weakref" (gnu). */
  ak_abi_tag,		/* "abi_tag" (gnu). */
  ak_no_specializations,/* "no_specializations" (clang). */
#endif /* GNU_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Microsoft-__declspec-only attributes. */
  ak_appdomain,		/* "appdomain" (ms). */
  ak_assembly_info,	/* "assembly_info" (ms). */
  ak_dllexport,		/* "dllexport" (ms). */
  ak_dllimport,		/* "dllimport" (ms). */
  ak_edg_interior_ptr_alias,
			/* "__edg_interior_ptr_alias" (ms). */
  ak_edg_pin_ptr_alias,	/* "__edg_pin_ptr_alias" (ms). */
  ak_empty_bases,	/* "empty_bases" (ms). */
  ak_guard,		/* "guard" (ms). */
  ak_hybrid_patchable,	/* "hybrid_patchable" (ms). */
  ak_implementation_key,
			/* "implementation_key" (ms). */
  ak_intrin_type,	/* "intrin_type" (ms). */
  ak_jitintrinsic,	/* "jitintrinsic" (ms). */
  ak_no_init_all,	/* "no_init_all" (ms). */
  ak_noalias,		/* "noalias" (ms). */
  ak_non_user_code,	/* "non_user_code" (ms). */
  ak_novtable,		/* "novtable" (ms). */
  ak_process,		/* "process" (ms). */
  ak_property,		/* "property" (ms). */
  ak_restrict,		/* "restrict" (ms). */
  ak_safebuffers,	/* "safebuffers" (ms). */
  ak_selectany,		/* "selectany" (ms). */
  ak_spectre,		/* "spectre" (ms). */
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  ak_thread,		/* "thread" (ms). */
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
  ak_uuid,		/* "uuid" (ms). */
  ak_layout_as_external,/* "layout_as_external" (ms). */
  ak_no_empty_identity_interface,
			/* "no_empty_identity_interface" (ms). */
  ak_no_ftm,		/* "no_ftm" (ms). */
  ak_no_refcount,	/* "no_refcount" (ms). */
  ak_no_release_return,	/* "no_release_return" (ms). */
  ak_no_weakreferencesource,
			/* "no_weakreferencesource" (ms). */
  ak_one_phase_constructed,
			/* "one_phase_constructed" (ms). */
  ak_allocator,		/* "allocator" (ms). */
  ak_no_sanitize_address,
			/* "no_sanitize_address" (ms). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if INCLUDE_EDG_TEST_ATTRIBUTES
  /* Attributes used for testing by EDG. */
  ak_edg_e1,		/* "edg::e1" (always triggers an error). */
  ak_edg_n1,		/* "edg::n1" (must appear in namespace scope). */
#endif /* INCLUDE_EDG_TEST_ATTRIBUTES */

  ak_availability,	/* "availability" */
  ak_using_if_exists,	/* "using_if_exists" */
  ak_exclude_from_explicit_instantiation,
			/* "exclude_from_explicit_instantiation" (clang). */

  /* Other attributes. */
  ak_annotation,	/* An attribute-like user-defined value that can be
			   examined with reflection. */
  ak_conditional_explicit,
			/* An internal attribute representing a C++20
			   "explicit(<bool-expression>)" construct. */
  ak_pragma_pack_state,	/* A pseudo-attribute used to record the current
			   value of "#pragma pack(n)" directives in some
			   cases. */
  ak_last
};


/*
Data structure describing an "attribute" as it appeared in the source.
Currently "attributes" include the following general annotation constructs:
  - standard [[ ... ]] attributes, and
  - GNU-style __attribute((...)) attributes.
In many cases, such annotations also affect an IL entry directly (e.g., the
"alignment" field of a type or variable in case of an alignment attribute),
but in other cases this data structure is the only record of the construct.
*/
typedef struct an_attribute {
  an_attribute_ptr
		next;
			/* Next in a linked list of attributes. */
  an_attribute_kind
		kind;
			/* The specific of attribute that was encountered. */
  an_attribute_family
		family;	/* The kind of construct that was used to express the
			   attribute in the source. */
  an_attribute_location
		syntactic_location;
			/* The syntactic location of the attribute. */
  a_bit_field	on_primary_declaration:1;
			/* The attribute appeared on the primary declaration of
			   an entity.  (Some attributes on a definition take
			   precedence over the same attribute applied to
			   another declaration of the same entity.)  For base
			   class entries, these are the attributes that were
			   specified on the base class specifier. */
  a_bit_field	transforms_type_specifier:1;
			/* TRUE if this attribute appertains to a type
			   specifier and produces a new type as a result.
			   Currently, this is only TRUE for GNU mode and
			   vector_size attributes. */
  a_bit_field	applied_to_declared_type:1;
			/* A front-end-only flag indicating that this attribute
			   has already been applied to the type declared by the
			   declaration in which it appeared, and must therefore
			   not be applied again when it is attached to the
			   entity being declared.  attach_attributes clears the
			   flag when it honors it.  Currently, the flag is only
			   set for GNU calling convention attributes on typedef
			   declarations (see
			   apply_calling_convention_attributes). */
  a_bit_field	must_be_preserved_in_trans_unit_copy:1;
			/* A front-end-only flag indicating that this attribute
			   should be preserved in the merged entity produced
			   by the trans_copy process.  FALSE means it is a
			   duplicate of something in another translation unit
			   and will be discarded. */
  a_bit_field	is_pack_expansion:1;
			/* TRUE if the attribute is a variadic template pack
			   expansion, i.e., it's followed by "...". */
  a_bit_field	is_std_gcc_attribute:1;
			/* TRUE if the attribute is a "[[gnu::...]]" standard
			   attribute.  Its family is af_std, but it is treated
			   as though it were an af_gnu attribute. */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	is_implicit_abi_tag_attribute:1;
			/* TRUE if the attribute is an "implicit" abi_tag
			   attribute.  Such attributes have been added during
			   the mangling process (and do not appear in the
			   source). */
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_bit_field	namespace_from_using:1;
			/* TRUE if the namespace name of the attribute was
			   obtained from a "using" prefix.  Only TRUE if
			   namespace_name is non-NULL. */
  a_bit_field	is_invalid_namespace:1;
			/* TRUE if the namespace name of the attribute is
			   invalid (used to prevent subsequent diagnostics). */
  a_const_char	*name;	/* The attribute name as it appeared in the source.
			   E.g. "aligned" for __attribute((aligned(8))). */
  a_const_char	*namespace_name;
			/* The attribute namespace name as it appeared in the
			   source.  E.g., "XYZ" in [[ XYZ::fast ]].  NULL if
			   no namespace name appeared (in particular, NULL
			   when family is not af_std).  If
			   namespace_from_using is TRUE, the namespace name
			   is implicitly specified through a "using" prefix. */
  an_attribute_arg_ptr
		arguments;
			/* The argument list of this attribute (NULL if there
			   are no arguments).  In the case of an annotation,
			   (kind == ak_annotation) the "argument" is of kind
			   aak_constant and represents the annotation value. */
  an_attribute_group_ptr
		group;
			/* The attribute group this attribute belongs to.
			   (NULL for attribute families that don't have a
			   notion of grouping.) */
  void		*assoc_info;
			/* Additional attribute-specific information (NULL if
			   there is none).  The value recorded here by the
			   front end (if any) is not for use by a back end.
			   However, back ends can make use of this field for
			   their own purposes. */
  a_source_position
		position;
			/* The position of the attribute. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		end_position;
			/* The position of the end of the attribute. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  struct a_pack_expansion_descr
		*pack_expansion_descr;
			/* If non-NULL, the attribute is a pack expansion,
			   and this points to the expansion description. */
} an_attribute;


typedef struct a_source_correspondence *a_source_correspondence_ptr;
typedef struct a_source_correspondence {
  /* Structure placed within several IL constructs to tie the IL construct
     instance back to a corresponding source construct instance. */
  char		*assoc_info;
			/* Pointer to associated information.  In the front
			   end, points to the associated front end symbol,
			   or NULL if there is no associated symbol.  Must
			   be cast to the proper pointer type for use. */
  a_const_char  *name;
			/* Pointer to null-terminated name, or NULL if
			   there is no corresponding source entity. */
#if NEED_NAME_MANGLING
  a_const_char	*unmangled_name_or_mangled_encoding;
			/* If name_has_been_mangled is TRUE, points to the
			   original (or fabricated if
			   unnamed_entity_given_fabricated_name is TRUE) name
			   before mangling (which might be NULL, if the entity
			   was unnamed).  Otherwise (i.e.,
			   name_has_been_mangled is FALSE), it can point to a
			   mangled encoding (currently used only for types in
			   the Cfront ABI) and is NULL otherwise. */
#endif /* NEED_NAME_MANGLING */
  struct a_trans_unit_corresp
		*trans_unit_corresp;
			/* If this entity has external linkage, this points
			   to an entry that represents the set of things
			   that this entry is linked to.  All entries that
			   refer to the same entity (because of linkage)
			   point to the same trans_unit_corresp entry.
			   Used for multiple translation unit checking, so
			   an entity used only in the primary translation
			   unit will have a NULL trans_unit_corresp pointer.
			   Externally-linked entities in secondary translation
			   units will have a non-NULL trans_unit_corresp
			   pointer.  This points to a front end data structure
			   and is for front end use only. */
  a_scope_ptr	parent_scope;
			/* The scope in which the current entity was declared.
			   If is_class_member is TRUE, this points to a scope
			   of kind sck_class_struct_union.  NULL if the current
			   entity is stored in file scope memory and the parent
			   scope is a function or block scope (to avoid memory
			   region constraint violations).  Also NULL for
			   entries (e.g., certain types and constants) not tied
			   to a specific declaration, and for template entries
			   that are members of prototype instantiations when
			   those prototype instantiations are not recorded in
			   the IL.  For some template parameters, points to an
			   sck_template_declaration scope (for prototype
			   instantiations). */
  a_routine_ptr
		enclosing_routine;
			/* If the current entity is a member of a function or
			   block scope, this points to the entry representing
			   the enclosing routine.  Otherwise, NULL. */
  a_module_entity_ptr
		module_entity;
			/* If the entity associated with this source
			   correspondence entry was imported from a module,
			   this points to the corresponding module entity.
			   (Note: multiple source correspondences can point to
			   the same module entity.  The module entity is
			   determined via the current module entity on the
			   module entity stack when this source correspondence
			   was created). */
  a_source_position
		decl_position;
			/* The source position at which this entity is
			   declared.  Unknown if sequence number in it
			   is 0.  Can be valid even if there is no associated
			   source name, to indicate the place where the
			   entity appeared without being named. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_decl_position_supplement_ptr
		decl_pos_info;
			/* Points to a block containing additional source
			   position information about the declaration.
			   May be NULL. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_name_reference_ptr
		name_references;
			/* Points to a list of the various forms of reference
			   used to name this entity.  This is used by the
			   front end to find a previously allocated entry so
			   that it can be reused.  This field should not be
			   used by back ends as it is sometimes cleared
			   (when using multiple translation units, for
			   example). */
  ENUM_TYPE_FOR_BIT_FIELD(an_access_specifier)
		access:2;
			/* The access control specified at the point of
			   declaration.	 Restricted access may be indicated
			   for class members only; all other entities are
			   "public" by default.	 In C mode, always "public". */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ENUM_TYPE_FOR_BIT_FIELD(an_access_specifier)
		assembly_access:2;
			/* Access outside of the parent assembly.  (C++/CLI
			   only.) */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	referenced:1;
			/* TRUE if the item is referenced in the
			   intermediate language.  This is always TRUE
			   for definitions of externally-visible entities,
			   since they may be referenced from other
			   translation units.  Also set for auto variables
			   that are dynamically initialized.  Also differs
			   from the flag in the symbol entry in that more
			   than one symbol can point to the same IL entry. */
#if MAINTAIN_NEEDED_FLAGS
  a_bit_field	needed:1;
			/* TRUE to indicate that an entity is referenced in
			   such a way that it is "really needed" -- that is,
			   it is referenced by something that is itself
			   "needed".  An entity can end up marked as
			   "referenced" but not "needed" if, for example, it
			   is only referenced by a function that is never
			   called.  This flag is intended as an aid to
			   optimization -- if it is FALSE, the entity is a
			   candidate to be optimized away. */
#endif /* MAINTAIN_NEEDED_FLAGS */
  ENUM_TYPE_FOR_BIT_FIELD(a_name_linkage_kind)
		name_linkage:NUM_BITS_FOR_NAME_LINKAGE;
			/* Kind of linkage for the name, e.g., is it
			   externally visible. */
  a_bit_field	has_associated_pragma:1;
			/* TRUE if an entry of type a_pragma has been created
			   and bound to this entity.  The pragma entry, which
			   will contain a pointer to this entity, is found by
			   calling find_assoc_pragma. */
  a_bit_field	is_local_to_function:1;
			/* TRUE if a function scope intervenes in the scope
			   stack between the scope to which the entity belongs
			   and the file scope.	In general, entities declared
			   in function and block scopes and within local
			   classes have the flag set to TRUE, and objects
			   declared at file scope and within nonlocal classes
			   have it set to FALSE. */
  a_bit_field	parent_via_local_scope_ref:1;
			/* TRUE if the parent scope is recorded in an entry of
			   type a_local_scope_ref (because of memory region
			   constraints).  This implies that parent_scope is
			   NULL and enclosing_routine is non-NULL. */
  a_bit_field	is_class_member:1;
			/* TRUE if the entry represents a C++ class member;
			   also TRUE for fields in C.  (Note: it is set for
			   anonymous union members even when their names are
			   promoted to a non-class scope.)  */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	has_associated_attribute:1;
			/* TRUE if a Microsoft attribute entry that applies to
			   this entity has been created. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEED_NAME_MANGLING
  a_bit_field	name_has_been_mangled:1;
			/* TRUE if the name of the entity has been changed
			   to the "mangled" form of the name (C++). */
  a_bit_field	mangled_name_cannot_be_included_in_other_name:1;
			/* TRUE if the name has been mangled in such a way that
			   the mangled form cannot be used as part of another
			   mangled name.  This happens for compressed and
			   truncated names.  When this is TRUE,
			   final_name_mangling_pending will be FALSE. */
  a_bit_field	final_name_mangling_pending:1;
			/* TRUE if part of the name mangling has been done,
			   but the final name mangling, which may or may not
			   change the name, has not been done yet.  Final name
			   mangling might do compression or truncation.  This
			   field will always be FALSE in configurations where
			   final name mangling is not needed. */
  a_bit_field	unnamed_entity_given_fabricated_name:1;
			/* TRUE if a fabricated name has been assigned to this
			   otherwise unnamed entity.  During mangling, the 
			   fabricated name is used (if this field is TRUE), but
			   other parts of the compiler use the unmangled name
			   (NULL). */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	entity_marked:1;
			/* General-purpose flag used during the computation
			   of implicit "abi_tag"s.  Nominally FALSE.  Could
			   be used for other purposes. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#endif /* NEED_NAME_MANGLING */
#if BACK_END_IS_CP_GEN_BE
  a_bit_field	qualification_needed:1;
			/* A qualified name should be used when referring
			   to this entity in the generated code.  This flag
			   is generally set and cleared within the
			   C++-generating back end while processing the
			   hidden name information associated with the
			   various scopes, except in certain cases with
			   PROTOTYPE_INSTANTIATIONS_IN_IL; in those cases,
			   the hiding is not reflected in the hidden name
			   table and this flag is set during the front end
			   processing to reflect the presence or absence of
			   a qualifier in the corresponding source
			   reference. */
  a_bit_field	partially_hidden_by_microsoft_injected_class_name:1;
			/* Used in Microsoft mode only, for injected class
			   names.  They require qualification unless used
			   to the left of "::".  Set/used only within the
			   C++-generating back end. */
  a_bit_field	visible_as_unqualified_name:1;
			/* This name is currently visible as an unqualified
			   name, even if its class or namespace parent is
			   not active.  This is used for injected class names
			   and block extern declarations.  Set/used only within
			   the C++-generating back end. */
#endif /* BACK_END_IS_CP_GEN_BE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_bit_field	is_decl_after_first_in_comma_list:1;
			/* The primary declaration of this entity appeared in
			   a comma-separated declarator list and was not the
			   first in that list.  E.g., "j" in "int i, j;".
			   For secondary declarations, see the similar flag in
			   a_src_seq_secondary_decl.  This is only maintained
			   if this entity has an associated source sequence
			   entry, or if it is associated with a local variable
			   or field declaration. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  a_bit_field	static_used_by_instantiation:1;
			/* TRUE if this entity is a static variable or function
			   that is referenced from an instantiation and
			   therefore needs to be made external (unless the
			   duplicate_static_in_instantiation_slices flag is
			   set).  Can be set (and should be ignored) on
			   an entity that is not static, if it was static
			   at some point and was made external (e.g., by
			   lowering). */
#if DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES
  a_bit_field	duplicate_static_in_instantiation_slices:1;
			/* TRUE if this is a special internal entity that
			   should be duplicated in instantiation slices
			   (rather than externalized) when referenced from an
			   instantiation. */
#endif /* DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if MAINTAIN_NEEDED_FLAGS
  a_bit_field	okay_to_walk_subtree_of_local_entity:1;
			/* TRUE if it is okay to walk the subtree of this
			   entity (a local class or local variable) in "needed"
			   flag or keep_in_il processing.  It's not okay to
			   walk the subtree if it can still change, i.e., while
			   the containing function is still being processed. */
#endif /* MAINTAIN_NEEDED_FLAGS */
  a_bit_field	copied_from_secondary_trans_unit:1;
			/* TRUE if this entity was copied from the IL of a
			   secondary translation unit into the primary
			   translation unit IL.  That might mean that its
			   name conflicts with the name of another entity
			   in the IL. */
  a_bit_field	same_name_as_external_entity_in_secondary_trans_unit:1;
			/* TRUE if this is an entity in the primary translation
			   unit IL that doesn't have external linkage but that
			   has the same name as an entity with external linkage
			   from a secondary translation unit with external
			   linkage. */
  a_bit_field	member_of_unknown_base:1;
			/* When a name is looked up in a class with
			   a dependent base class and is not found in the
			   derived class or in a nondependent base, the name
			   is assumed to be a member of the nonreal base class.
			   This flag is TRUE for the entities created to
			   represent such nonreal class members.  Such names
			   must be used with care because they may actually
			   come from one of several dependent bases (the
			   front end assigns a member to the first dependent
			   base) or the name could come from a base class
			   of the dependent base. */
  a_bit_field	qualified_unknown_base_member:1;
			/* If member_of_unknown_base is TRUE, this flag
			   reflects whether the reference to the member was
			   qualified or unqualified.  This is important to
			   distinguish between "this->f()", which might be a
			   virtual call, and "this->S::f()", which is always
			   non-virtual.  If both qualified and unqualified
			   references to the same member are used, there will
			   be distinct constants for the two forms.  (Note:
			   this flag will always be FALSE for a type used as
			   a qualifier in a qualified name, due to the way
			   the front end handles coalescing of identifiers,
			   but the distinction is unimportant in such
			   cases.) */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	member_of_unknown_super:1;
			/* When a reference to the Microsoft __super keyword
			   is made in a class template with dependent base
			   classes the entity cannot be looked up during the
			   prototype instantiation.  This flag is TRUE for
			   entities created to represent members of an
			   unknown super class. */
  a_bit_field	microsoft_identifier_used:1;
			/* TRUE if the name was specified using a
			   Microsoft __identifier operator. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_bit_field	marked_as_gnu_extension:1;
			/* TRUE if the primary declaration was preceded by the
			   GNU keyword __extension__.  (For other declarations
			   a similar flag is present in the corresponding
			   secondary source sequence entry.) */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_bit_field   is_deprecated_or_unavailable:1;
			/* TRUE if this entity was marked as deprecated
			   or unavailable (using an attribute). */
  a_bit_field	externalized:1;
			/* TRUE if this is a variable or routine that was
			   originally static and has been made external, e.g.,
			   so that it can be referenced from multiple
			   instantiation slices. */
#if IA64_ABI
  a_bit_field	on_mangling_substitution_list:1;
			/* TRUE if this is an entity that's currently on a
			   list of available mangling substitutions. */
#endif /* IA64_ABI */
  a_bit_field	maybe_unused:1;
			/* TRUE if the "maybe_unused" standard attribute or the
			   "unused" GCC attribute appertains to this entity. */
#if RECORD_SCOPE_DEPTH_IN_IL
  a_scope_depth	scope_depth;
			/* Scope nesting depth of this entity. */
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		source_sequence_entry;
			/* Pointer to a source sequence entry that represents
			   the place this entity appears within the
			   translation unit relative to other declarations as
			   well as statements, comments, etc.  When an entity
			   has more than one declaration, this pointer
			   identifies its definition or, if there is no
			   definition in the current translation unit, the
			   first declaration that is not a block-extern
			   or (in C mode) implicit function declaration. This
			   pointer is NULL when there are no declarations of
			   a file-scope routine or variable except within
			   function bodies. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  a_per_instantiation_needed_flags_entry_ptr
		per_instantiation_needed_flags;
			/* A list of entries defining a bit vector.
			   Bit N of the vector indicates whether the entity of
			   which this is the source correspondence field is
			   needed in the instantiation assigned number N.
			   Bits are numbered from 1.  NULL if not needed. */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  an_attribute_ptr
		attributes;
			/* The set of attributes applicable to this entity. */
} a_source_correspondence;


/*
Data structures related to constants:
*/
enum a_constant_repr_kind : a_byte {
  /* In a constant entry, there are several possible representations
     for a constant: */
  ck_error,             /* Error. */
  ck_integer,           /* Integers. */
                        /* char and enum types are handled as integers: see
                           the integer variant of a_type.  Addresses formed
                           by casting an integer constant to a pointer type
                           are also represented by ck_integer, and so are
                           nullptr_t constants (value zero, of tk_nullptr
                           type).  Also used to represent values of the ARM
                           __mfp8 (see tk_mfp8) type. */
#if FIXED_POINT_ALLOWED
  ck_fixed_point,       /* Fixed-point types. */
#endif /* FIXED_POINT_ALLOWED */
  ck_string,            /* Character strings, as well as optimizable #embed
                           expansions. */
  ck_float,             /* All sizes of float. */
#if C99_IL_EXTENSIONS_SUPPORTED
  ck_complex,           /* All sizes of C99's _Complex types. */
  ck_imaginary,         /* All sizes of C99's _Imaginary types. */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  ck_address,           /* Address/pointer. */
  ck_ptr_to_member,	/* C++ pointer-to-member (data or function). */
#if GNU_EXTENSIONS_ALLOWED
  ck_label_difference,	/* The difference between two "label addresses"; a
			   GCC feature. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
  ck_stack_offset,	/* Stack offset constant for a variable.  Used in
			   exception handling tables generated by IL lowering
			   in some modes. */
#endif /* DO_IL_LOWERING && ... */
  ck_dynamic_init,	/* Dynamic initialization.  Indicates the location of
			   a non-constant part of an aggregate initialization,
			   one that requires code.  Only used in C++ and in C
			   modes that allow nonconstant initializers in
			   aggregates. */
  ck_aggregate,         /* For list of constants in initialization. */
  ck_init_repeat,       /* Used to specify a repeated initialization constant
                           in an array. */
  ck_template_param,	/* Nontype parameter in a class template declaration
			   (C++ front end only, except when prototype
			   instantiations are passed to a back end). */
  ck_designator,        /* Used to change the "current object" in an
                           aggregate initializer (C99, some C++ modes). */
#if UPC_EXTENSIONS_ALLOWED
  ck_upc_threads,       /* A multiple of the UPC pseudo-constant THREADS. */
  ck_upc_mythread,      /* The UPC pseudo-constant MYTHREAD. */
#endif /* UPC_EXTENSIONS_ALLOWED */
  ck_void,		/* In C++14, "void" is a literal type, and folding
			   can produces "values" of that type.  This constant
			   kind represents such cases. */
  ck_reflection,	/* A reflection value. */
  ck_last
};


enum an_address_base_kind : a_byte {
  /* When a constant is an address, there are several types of things that can
     be pointed to. */
  abk_routine,          /* Pointer to a function. */
  abk_variable,         /* Pointer to a variable. */
  abk_constant,		/* Pointer to a constant. */
  abk_temporary,	/* Pointer to a temporary initialized to a constant. */
  abk_uuidof,		/* Pointer to _GUID structure for Microsoft __uuidof
			   operation. */
  abk_typeid,		/* Pointer to a std::type_info structure.  Used in
			   Microsoft mode when typeid appears in a template
			   argument list and for C++11 address constants
			   designating type_info objects. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  abk_cli_typeid,	/* Handle to a C++/CLI System::Type object.  Used for
			   C++/CLI T::typeid constants. */
  abk_cli_array,	/* Handle to a C++/CLI System::Array object.  Used for
			   C++/CLI "gcnew" expressions in custom attribute
			   arguments where the array is treated as a
			   constant. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  abk_label,            /* Pointer to a label.  This is used for the
			   GNU address-of-label extension. */
  abk_param_ref,        /* Address of a parameter (only used during constant-
			   evaluation). */
  abk_last
};


#if C99_IL_EXTENSIONS_SUPPORTED
typedef struct an_internal_complex_value *an_internal_complex_value_ptr;
typedef struct an_internal_complex_value {
  /* Internal representation for a complex value. */
  an_internal_float_value
                real,
                imag;   /* Real and imaginary parts of the value. */ 
} an_internal_complex_value;

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

/*
Data structure describing an explicitly declared namespace.  An unnamed
namespace is one in which the source_corresp.name field is a NULL pointer.
*/
typedef struct a_namespace {
  /* The source_corresp field must be first. */
  a_source_correspondence
		source_corresp;
			/* Information on the source entity that corresponds
			   to this entity. */
  a_namespace_ptr
		next;
			/* Next in a linked list of namespace declarations;
			   NULL for the last on the list. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_type_ptr	proxy_class;
			/* In Microsoft mode, in certain template dependent
			   contexts a construct like "decltype(N::x)" is
			   allowed when x has not yet been declared in
			   namespace N.  In such cases, a proxy class is
			   created for N and the member is placed in that
			   proxy class.  This field points to the associated
			   proxy class if one has been created.  NULL
			   otherwise. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_hash_value
		hash_value;
			/* A hash value computed for this namespace, or
			   zero if a hash has not yet been computed. */
  a_bit_field	is_namespace_alias:1;
			/* TRUE when the name is a namespace alias. */
  a_bit_field	is_inline:1;
			/* TRUE if the namespace was declared as an inline
			   namespace. */
  a_bit_field	has_internal_linkage:1;
			/* TRUE if the namespace is unnamed or declared within
			   an unnamed namespace (directly or indirectly). */
  a_bit_field	named_in_strong_using:1;
			/* TRUE if the namespace was named in a g++ strong
			   using directive. */
  a_bit_field	is_std:1;
			/* TRUE if this is namespace std. */
#if BACK_END_IS_CP_GEN_BE
  a_bit_field	shadowed_by_class:1;
			/* TRUE if this namespace is hidden in some scope
			   by a class.  This allows cp_gen_be to put out
			   a namespace alias that can be used in qualifiers
			   to work around a g++ bug. */
#endif /* BACK_END_IS_CP_GEN_BE */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field   has_gnu_abi_tag_attribute:1;
			/* TRUE if this is an inline namespace declared with a
			   GNU "abi_tag" attribute. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  union {
    /* When is_namespace_alias == FALSE: */
    a_scope_ptr	assoc_scope;
			/* Pointer to the scope entry corresponding to this
			   namespace; should never be NULL. */
    /* When is_namespace_alias == TRUE: */
    a_namespace_ptr
		assoc_namespace;
			/* Pointer to the namespace entry (which may itself
			   be an alias) for which this entry is an alias;
			   should never be NULL. */
  } variant;
} a_namespace;


/*
Data structure describing a using-declaration or a using-directive.  A class
member using-declaration has the form "using A::y" (where A is the name of a
base class of the class in which the declaration appears).  A nonmember
using-declaration has the form "using N::y" (where N is a namespace name) or
"using ::y".  A using-directive is of the form "using namespace N", where N
is a namespace name.  Entries are also created to represent C++11 inline
namespaces.  Inline namespaces are the mechanism used to standardize the
g++ feature originally known as strong using-directives.
*/
typedef struct a_using_decl *a_using_decl_ptr;
typedef struct a_using_decl {
  a_using_decl_ptr
		next;
			/* Next in a linked list of using-decl entries for the
			   current scope; NULL for the last on the list. */
  a_source_position
		position;
			/* Source position of the start of the
			   using-declaration or using-directive. */
  a_tagged_pointer
		entity;
			/* Entry identifying the entity specified in the
			   using-declaration or using-directive; when a
			   using-declaration specifies an overload set, each
			   function or function template is recorded
			   individually. */
  an_attribute_ptr
		attributes;
			/* A list of attributes (NULL if none) specified on
			   this using-declaration or using-directive. */
  a_bit_field	is_using_directive:1;
			/* TRUE if this is a using-directive and FALSE if it
			   is a using-declaration. */
  a_bit_field	is_class_member:1;
			/* When is_using_directive is FALSE, this flag is TRUE
			   if this using-declaration refers to a class member
			   and FALSE if it refers to a non-class member.
			   This usually corresponds with whether or not the
			   using-declaration appeared as a class member, but
			   in Microsoft bugs mode a nonmember using-declaration
			   can refer to a type that is a class member. */
  a_bit_field	is_inheriting_ctor:1;
			/* TRUE if this represents a using-declaration for
			   inheriting constructors.  If so, entity points to
			   the class type whose constructors are to be
			   inherited, is_class_member is TRUE, and
			   is_using_directive is FALSE. */
  a_bit_field	hidden:1;
			/* For class member using-declarations only, TRUE if
			   a base class member brought into a derived class
			   by a using-declaration is subsequently hidden by a
			   declaration in the derived class. */
  a_bit_field	compiler_generated:1;
			/* TRUE for a using-directive that did not actually
			   appear in the source.  This is the case for the
			   implicit using-directive created when using
			   unnamed namespaces and is also TRUE for the
			   using-directive created to simulate a Microsoft
			   bug (in Microsoft bugs mode). */
  a_bit_field	inline_namespace:1;
			/* TRUE to represent an entry created for an inline
			   namespace or a GNU strong using-directive.  A
			   namespace named in a strong-using directive is
			   treated as if the namespace were declared inline.
			   An entry created to represent an inline namespace
			   differs from a normal using-directive in the
			   following ways:
			   1. Entities declared in inline namespaces can be
			      defined as if they were members of the namespace
			      containing the inline namespace.
			   2. Templates from the inline namespace can be
			      specialized or instantiated as if they were
			      members of the namespace containing the
			      inline namespace.
			   3. For argument dependent lookup, if an associated
			      namespace is an inline namespace, its enclosing
			      namespace is also an associated namespace.  If
			      an associated namespace directly contains an
			      inline namespace, it is included in the set of
			      associated namespaces.
			   4. In a qualified lookup, namespaces in strong
			      using-directives are examined even if the
			      namespace named in the qualified name contains
			      the named member. */
  a_bit_field	strong:1;
			/* TRUE if this was made an inline namespace through
			   use of the g++ strong attribute. */
  a_bit_field	is_pack_expansion:1;
			/* TRUE if this using-declaration is of the form
			     using Q::N...;
			   Only valid when is_class_member is TRUE (a C++17
			   feature). */ 
  a_bit_field	is_representative:1;
			/* TRUE for a using-declaration entry that is the
			   "representative" for potentially multiple entries
			   created for a using-declaration (because an overload
			   set is referred to, or for a "using enum").  This
			   is useful for the C++-generating back end. */
  a_bit_field	is_using_enum:1;
			/* TRUE if this entry was created for a C++20
			   "using enum" declaration.  One entry is created
			   for each enumerator.  The initial entry for the
			   "using enum" has is_representative set. */
  a_bit_field	is_enumerator:1;
			/* TRUE if this entry was created for a C++20
			   using-declaration that refers to an enumerator.
			   This could be either the "using enum" case for which
			   is_using_enum is also set, or a using-declaration
			   in which the name refers to an enumerator. */
  an_access_specifier
                access;
			/* For class member using-declarations only, the
			   adjusted access for the indicated base class
			   member. */
  a_parent_class_or_namespace
		qualifier;
			/* For using-declarations only, the class, namespace,
			   or enum type that was actually specified in the
			   qualified name that appeared in the source code
			   (which is not necessarily the same as the parent
			   of that which is referred to by entity.ptr).  For
			   class member using-declarations or when
			   is_enumerator is TRUE, use the class_type variant;
			   otherwise, use the namespace_ptr variant, which
			   will be NULL when the global qualifier ("::") was
			   specified. */
  unsigned long	decl_sequence_number;
			/* For using-directives, the declaration sequence
			   number of the location of the using-directive.  This
			   is for front-end use for lookups with template
			   instantiations.  Using-directives that appear
			   after the definition of the template are
			   ignored.  This field is set only for namespace
			   scope using-directives.  For block scope
			   using-directives, it is set to the special value
			   FIRST_DECL_SEQUENCE_NUMBER. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		source_sequence_entry;
			/* Pointer to the source sequence entry that represents
			   the place this using-declaration or using-directive
			   appears within the current scope relative to other
			   declarations, statements, etc.  It may be NULL
			   if the entry refers to a member of an overload set,
			   since only one member of set (namely, the head of
			   the list linked by the next_in_overload_set pointer)
			   actually points to (and is pointed to by) the
			   source-sequence entry for the declaration. */
  a_using_decl_ptr
		next_in_set;
			/* Pointer to the next in a linked list of using-decl
			   entries created for a single using-declaration
			   that names an overload set or from a single
			   using-enum-declaration.  NULL in other cases and
			   for the last entry in the set. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
} a_using_decl;


/*
Data structure a_dynamic_init describes a dynamic initialization of a simple
(non-aggregate) variable, an aggregate variable (class or array), or a
component of an aggregate variable (field or array element).  Dynamic-init
entries are pointed to directly from variables being initialized, from
ck_dynamic_init constant entries (when a component of a variable requires
non-constant initialization), and from stmk_init statements (which mark the
point in an execution stream at which the initialization takes place).
A dynamic init entry of kind dik_none is created for an object that does not
actually require initialization but does have a destructor that must be
called when its lifetime terminates.
*/
enum a_dynamic_init_kind : a_byte {
  dik_none,		/* No dynamic initialization. */
  dik_zero,		/* Initialization to zero, defined to be the same
			   as default initialization of a static object.
			   This is also used to represent trivial constructor
			   invocations that have been folded to an aggregate
			   constant (for the backing expression). */
  dik_constant,		/* Initial value of a simple object is a constant. */
  dik_expression,	/* Initial value of a simple object is an
			   expression. */
  dik_class_result_via_ctor,
			/* Initial value of a simple object is established by
			   a call of a routine that returns a class object
			   via a constructor, or by the evaluation of a GNU
			   statement expression that results in a class object
			   produced via a constructor. */
  dik_call_returning_class_via_cctor = dik_class_result_via_ctor,
			/* Synonym for dik_class_result_via_ctor (for backward
			   compatibility purposes). */
  dik_constructor,	/* Initial value of a simple object is established by
			   a constructor call.  C++ only. */
  dik_nonconstant_aggregate,
			/* Initial value of a nonconstant aggregate object
			   (array or class) is represented by a list of
			   constant entries (some of which will refer to
			   nonconstants).  C++/C99/GNU C only; not used
			   in C89. */
  dik_bitwise_copy,	/* Initial value is established by a bitwise copy --
			   used, for example, for member-wise copy inside a
			   copy constructor, when the field or base class
			   to be copied lacks a copy constructor.  C++ only. */
  dik_lambda		/* Initial value of a lambda object.  C++ only. */
};


typedef struct a_dynamic_init *a_dynamic_init_ptr;
typedef struct a_dynamic_init {
  a_dynamic_init_ptr
		next;	/* For file-scope dynamic initializations, pointer to
			   the next dynamic initialization in the file scope
			   in source order.  NULL otherwise (i.e., not used
			   in all other cases). */
  a_variable_ptr
		variable;
			/* If this dynamic-init entry initializes a whole
			   variable, this points to the variable.  NULL
			   otherwise (e.g., when pointed to from a
			   ck_dynamic_init constant to indicate initialization
			   of one member of an aggregate, or when initializing
			   a temporary in an expression). */
  a_routine_ptr destructor;
			/* If non-NULL, the destructor routine to be invoked
			   when this object ceases to exist; if NULL, no
			   destructor call is required.  Also used, when
			   is_freeing_of_storage_on_exception is TRUE, to
			   point to a delete routine to free storage for a
			   new-allocation cleanup.  If a destructible
			   temporary appears in a potentially-evaluated
			   but not evaluated expression, the destructor will
			   be non-NULL but the destruction will not be on an
			   object lifetime list and the lifetime field will
			   be NULL. */
  an_object_lifetime_ptr
		lifetime;
			/* The object lifetime associated with the object
			   being initialized here.  NULL if the initialization
			   has no associated destruction (and therefore always
			   NULL in C).  NULL for initializations under a "new"
			   operator, because the lifetime is under user
			   control for those. */
  a_dynamic_init_ptr
		next_in_destruction_list;
			/* If both destructor and lifetime are non-NULL
			   (that is, if this entry requires an automatic
			   destruction when the object lifetime terminates),
			   a pointer to a dynamic init entry representing the
			   next destruction to be done after this one, or
			   NULL if this is the last to be done for the
			   associated object lifetime.  Note: since
			   destructions happen in an order opposite to that
			   of initializations, the entry pointed to
			   corresponds to the previous destructible dynamic
			   initialization in the given object lifetime. */
  an_object_lifetime_ptr
		init_expr_lifetime;
			/* If non-NULL, defines the object lifetime for the
			   "full expression" that is the initializer (even
			   if syntactically it's not an expression, e.g., it's
			   a parenthesized list of constructor arguments).
			   Temporaries within the expression are given this
			   lifetime.  Note that this is not the lifetime for
			   the entity being initialized (that's given by the
			   field "lifetime", above). */
  a_dynamic_init_kind
		kind;	/* Kind of dynamic initialization (constant,
			   expression, constructor, aggregate). */
  a_bit_field   static_temp:1;
			/* If TRUE, the temporary (enk_temp_init) or closure
			   variable (enk_lambda) must be static.  This means
			   the storage duration is required to be static.
			   A value of FALSE, however, does not mean the storage
			   duration is forced to be automatic.  For example,
			   temp inits in the file scope that don't need to be
			   static will have this flag FALSE even though static
			   may be the only possible storage duration if the
			   temporary is realized in the file scope.  Also,
			   storage duration is different than object lifetime;
			   see the "lifetime" field also. */
  a_bit_field	follows_an_exec_statement:1;
			/* TRUE if this initialization is pointed to from
			   an stmk_init and the stmk_init appears after
			   some executable statements in its block, or if
			   the initialization must otherwise be done where
			   it appears rather than on the declaration of the
			   variable.  One would think that this belongs in
			   the stmk_init, but putting it here makes it
			   accessible from both the stmk_init and the
			   variable being initialized. */
  a_bit_field	inside_conditional_expression:1;
                        /* This initialization is inside a conditional part of
                           an expression (e.g., under a "?" operator). */
  a_bit_field	unordered:1;
			/* TRUE if this entry represents an automatic
			   end-of-lifetime destruction and is unordered
			   relative to another entry on the destructions list.
			   For instance, in the expression (A(i) + A(j)),
			   where A names a class with a destructor, the
			   operands are unordered in the IL, so the
			   destruction list cannot predetermine which should
			   be destroyed first; the dynamic init entries for
			   both operands will have the flag set. */
  a_bit_field	has_temporary_lifetime:1;
			/* TRUE if the entity initialized is a temporary with
			   the normal object lifetime for a temporary (e.g.,
			   it's not a temporary whose lifetime has been
			   extended by virtue of being bound to a
			   reference). */
  a_bit_field	is_constructor_init:1;
			/* TRUE if this entry is pointed to from a
			   constructor_init entry in a constructor or
			   destructor. */
  a_bit_field	is_freeing_of_storage_on_exception:1;
			/* TRUE if this entry indicates (as a destruction)
			   a call of a delete routine to free the storage
			   allocated in a new if an exception is thrown before
			   the storage is initialized. */
  a_bit_field	is_array_freeing:1;
			/* When is_freeing_of_storage_on_exception is TRUE,
			   this is TRUE if the "new" operation is an array
			   new. */
  a_bit_field	destruction_is_for_partially_constructed_aggregate:1;
			/* TRUE if destructor is non-NULL, exceptions_enabled
			   is TRUE, and this entry is associated with a member
			   of an aggregate whose member-by-member construction
			   might be interrupted by an exception before the
			   entire aggregate has been initialized. */
#if DO_IL_LOWERING
  a_bit_field	is_guard_var_for_local_static_var_init:1;
			/* TRUE if this entry represents the conditional flag
			   variable that guards a local static variable
			   initialization.  The flag variable must be cleared
			   to zero if an exception is thrown before the
			   initialization is completed.  The entry has
			   a NULL destructor pointer. */
#endif /* DO_IL_LOWERING */
  a_bit_field	overlaps_temps_in_inner_lifetime:1;
			/* TRUE if the entity is initialized during a nested
			   object lifetime, and overlaps with destructible
			   temporaries in the inner lifetime.  If this flag is
			   set, the destruction for this entity should not
			   be considered to be on the cleanup list until
			   the entity has actually been initialized.  This flag
			   is not set for variables with static storage
			   duration.  It is set when there are partial-
			   aggregate cleanups in an inner lifetime, even if
			   there are no "real" temporaries. */
#if DO_IL_LOWERING
  a_bit_field	included_in_slice:1;
			/* Used to mark destructions associated with the
			   initializations included in a file-scope
			   initialization routine for a given instantiation
			   slice. */
#endif /* DO_IL_LOWERING */
  a_bit_field	is_explicit_cast:1;
			/* If TRUE, the source construct that generated
			   this initialization is an explicit cast. */
  a_bit_field	is_compound_literal:1;
			/* If TRUE, the source construct that generated
			   this initialization is a compound literal. */
  a_bit_field	is_braced_initializer:1;
			/* If TRUE, the source construct that generated this
			   initialization is a brace-enclosed initializer. */
  a_bit_field	is_partially_initialized:1;
			/* TRUE if the initialized entity is an array or class
			   aggregate not completely initialized by the
			   associated aggregate constant.  This can also
			   indicate that trailing elements not covered by an
			   aggregate initializer need to be zeroed prior to
			   being initialized by a generated default constructor
			   (because of the value-initialization rules).  The
			   use of designated initializers during initialization
			   of array and class aggregates circumvents the normal
			   detection of partially initialized aggregates: This
			   field is therefore also set to TRUE if any member of
			   an array or class aggregate is initialized using a
			   designator.  In some of these cases, the flag will
			   be set to FALSE during lowering if the constant is
			   found to fully initialize the aggregate. */
  a_bit_field	is_result_for_class_rvalue_question_mark:1;
			/* If TRUE, this entity is the temporary that is
			   the result of a "?" operator that returns a
			   class rvalue in C++. */
  a_bit_field	class_rvalue_initialized_through_master_entry:1;
			/* If TRUE, is_result_for_class_rvalue_question_mark
			   or is_result_for_comma_operator will also be TRUE,
			   and an optimization has been done to avoid the final
			   copy of the result of the "?" or "," operation.  The
			   kind is dik_expression and the expression pointed to
			   by variant.expression is evaluated to effect the
			   initialization of this temporary, but the value of
			   the expression is not stored into the temporary
			   (i.e., its associated node has the
			   result_is_not_used flag set to TRUE).  Note that
			   this case is eliminated by IL lowering and therefore
			   will never be seen in lowered code. */
  a_bit_field	is_result_for_comma_operator:1;
			/* If TRUE, this entity is the temporary that is the
			   result of a "," operator. */
  a_bit_field	is_reused_value:1;
			/* TRUE if this initialization's value is reused
			   elsewhere in the current expression via an
			   enk_reuse_value node. */
#if DO_IL_LOWERING
  a_bit_field	is_vla_deallocation:1;
			/* TRUE if this entry represents (on the destruction
			   list) the deallocation of a variable-length array.
			   The "variable" field indicates the VLA variable.
			   This entry is generated by IL lowering, and will
			   not appear in unlowered IL, nor will it remain in
			   the IL after lowering unless object lifetime
			   information is retained (e.g., to do a high-quality
			   exception handling implementation).  Never set
			   if VLA_DEALLOCATION_REQUIRED is FALSE, and never
			   set in C mode.  If the variable requires
			   destruction of its elements, that is represented
			   in a separate entry. */
#if GENERATE_EH_TABLES
  a_bit_field	is_freeing_of_exception_object:1;
			/* TRUE if this entry represents (on the destruction
			   list) the deallocation of the exception object
			   allocated in the runtime.  Only used internally
			   within lowering. */
#endif /* GENERATE_EH_TABLES */
#endif /* DO_IL_LOWERING */
  a_bit_field	is_creation_of_initializer_list_object:1;
			/* TRUE if this is a dynamic init that calls a
			   constructor of std::initializer_list<X> to create
			   an initializer list object from an array of
			   values of type X, provided in a temporary passed
			   as the first argument of the constructor call. */
  a_bit_field	is_array_for_initializer_list_object:1;
			/* TRUE if this is the creation of the array under
			   an std::initializer_list object. */
  a_bit_field	is_top_temporary_for_constexpr_reference_param:1;
			/* Set for the top temporary in an expression passed
			   as the argument for a reference parameter of a
			   constexpr function.  Short-term use in the front end
			   only. */
#if BACK_END_IS_CP_GEN_BE
  a_bit_field	suppress_init_list_arg_braces:1;
			/* TRUE for a dynamic init for which
			   is_creation_of_initializer_list_object is TRUE
			   to indicate that braces around a ck_aggregate
			   argument to the initializer_list constructor
			   should be suppressed because the context is
			   already brace-enclosed.  Set/used only within
			   the C++-generating back end. */
  a_bit_field	suppress_template_arguments_for_cast:1;
			/* TRUE for a dynamic init representing a functional-
			   notation cast where the type was specified using a
			   template name without template arguments (i.e.,
			   relying on the C++17 "class template argument
			   deduction" feature).  In some cases the deduced
			   arguments cannot be expressed explicitly (e.g.,
			   closure types). */
#endif /* BACK_END_IS_CP_GEN_BE */
  union {
    /* When kind == dik_none or dik_zero: no variant fields. */
    /* When kind == dik_constant, dik_nonconstant_aggregate, or dik_lambda: */
    struct {
      a_constant_ptr
		ptr;	/* The constant initial value.  Always an unshared
			   constant.  When non_constant (see below) is TRUE
			   (dik_nonconstant_aggregate (used only in C++,
			   C99, and GNU C) or some cases of dik_lambda) this
			   points to a ck_aggregate constant entry for which
			   one or more of the entries on its linked list are
			   ck_dynamic_init constants. */
      a_lambda_ptr
		lambda;	/* When kind == dik_lambda, the lambda that the
			   constant is an initializer for, NULL otherwise. */
      a_bit_field
		non_constant:1;
			/* TRUE if ptr points to a ck_aggregate constant entry
			   for which one or more of the entries on its linked
			   list are ck_dynamic_init constants. */
    } constant;
    /* When kind == dik_expression or kind == dik_class_result_via_ctor: */
    an_expr_node_ptr
		expression;
			/* The expression that gives the initial value
			   (dik_expression), or the call or GNU statement
			   expression that returns the initial value via a
			   constructor (dik_class_result_via_ctor). See the
			   note on
			   class_rvalue_initialized_through_master_entry
			   regarding one special case of dik_expression. */
    /* When kind == dik_constructor: */
    /* Used only in C++. */
    struct {
      a_routine_ptr
		ptr;
			/* The constructor to be invoked to initialize this
			   object.  In initializations generated for entities
			   with template-dependent types in prototype
			   instantiations, this can be NULL to indicate that
			   the constructor is not known. */
      an_expr_node_ptr
		args;   /* The actual arguments with which the constructor
			   should be called, not including the argument for
			   the destination, and, when
			   is_copy_constructor_with_implied_source is TRUE,
			   also not including the argument for the source.
			   If is_array_copy is TRUE, the first argument is not
			   the one that the constructor should be called with,
			   but an expression that produces an array to copy:
			   Its elements should be passed to repeated calls of
			   the constructor (possibly with the additional
			   arguments on this list, if any).  NULL if there are
			   no arguments other than the implicit one(s). */
      a_bit_field
		is_copy_constructor_with_implied_source:1;
			/* The constructor is a copy constructor in which the
			   source object to be copied is implied (i.e., its
			   address is computed based on the context).  This
			   flag is set, for example, in initializing a
			   subobject (a base class or field) of an object that
			   is being initialized by a copy constructor; the
			   subobject to be copied is determined based on the
			   object being copied (i.e., the address of the source
			   subobject is computed based on the address of the
			   source object).  This flag is also set for
			   class objects copied from a throw expression to the
			   handler parameter, where the address of the
			   source object is known only at runtime. */
      a_bit_field
		is_implicit_copy_for_copy_initialization:1;
			/* TRUE if this call is the copy constructor call
			   for the implicit (unelided) copy of a
			   copy-initialization. */
      a_bit_field
		value_initialization:1;
			/* TRUE if the object should be value-initialized
			   instead of default-initialized.  In C++/CLI, this
			   bit is also used to represent zeroing out of the
			   allocated memory before invoking the constructor. */
      a_bit_field
		has_sequenced_arguments:1;
			/* TRUE if the call arguments must be evaluated in
			   order from left to right.  This comes up when a
			   C++11 initializer list ends up being the argument
			   list for a constructor. */
      a_bit_field
		is_array_copy:1;
			/* TRUE if this represents the nontrivial copying of
			   an array.  In that case, the array to copy will be
			   the first expression on the args list (it should
			   not be passed to the constructor directly; instead,
			   its elements should be passed to repeated calls of
			   the constructor). */
    } constructor;
    /* When kind == dik_bitwise_copy: */
    struct {
      an_expr_node_ptr
		source;
			/* If non-NULL, the lvalue to be copied.  NULL if the
			   source is implied from context, which is frequently
			   the case.  For example, in a constructor init entry
			   representing a field or base class to be copied, the
			   source is pointed to by the constructor init entry
			   itself.  Another example is the initialization of a
			   handler parameter when handling an exception: In
			   that case the source is only known at run-time. */
    } bitwise_copy;
  } variant;
#if DO_IL_LOWERING
  struct a_destructible_entity_descr
		*destructible_entity_descr;
			/* Used by IL lowering to record information about the
			   entity initialized, in an IL-lowering-specific
			   form.  This is needed later when generating
			   destruction code.  Not used for static variables. */
  struct an_init_pos_descr
		*init_destination;
			/* Description of the initialization destination.
			   Set by IL lowering, only for entries with
			   class_rvalue_initialized_through_master_entry TRUE.
			   Note that this points to a variable allocated on the
			   stack (i.e., not something in the IL). */
  a_new_delete_supplement_ptr
		assoc_new;
			/* When is_freeing_of_storage_on_exception is TRUE,
			   points to the a_new_delete_supplement structure
			   associated with this destruction; NULL otherwise.
			   Can also be NULL for destructions generated during
			   lowering.  Must be non-NULL for destructions
			   associated with a placement new. */
#endif /* DO_IL_LOWERING */
  an_object_lifetime_ptr
		lifetime_of_overlapping_temps;
			/* When overlaps_temps_in_inner_lifetime is TRUE, this
			   identifies the inner lifetime. */
  a_dynamic_init_ptr
		master_entry;
			/* If non-NULL, this entry initializes a temporary
			   associated with the initialization entry pointed to.
			   The master entry handles destruction, etc.  This is
			   used for the optimization of a "?" or "," operator
			   returning a class rvalue.  master_entry->
			   class_rvalue_initialized_through_master_entry will
			   be TRUE in such cases. */
  an_expr_rescan_info_entry_ptr
		rescan_info;
			/* For casts scanned in templates that might
			   be rescanned later to redo semantic analysis,
			   points to extra front-end-only information that
			   is needed for the rescan.  NULL otherwise. */
} a_dynamic_init;


enum a_template_param_constant_kind : a_byte {
  /* When a constant is marked as a template parameter it may have one of
     several kinds (front end only except when PROTOTYPE_INSTANTIATIONS_IN_IL
     is TRUE). */
  tpck_param,		/* The template param constant represents a simple
			   non-type template parameter, e.g., for I in the
			   following:
			     template <int I> class A {
                               int a[I];
                             };
			   This is the most common and obvious case. */
  tpck_expression,	/* The template param constant represents an
			   expression, e.g., for I+1 in the following:
			     template <int I> class A {
			       static char s[I+1];
			     };
			     template <int I> char A<I>::s[I+1] = { 0 }; */
  tpck_member,		/* The template param constant represents a member of
			   a tk_template_param class, e.g., for T::k in the
			   following:
			     template <class T> class A {
			       int a[T::k];
			     };
			   Represents the "value" of the member; see
			   also tpck_address. */
  tpck_unknown_function,
			/* Represents the address of an unknown function
			   called in a template context where overload
			   resolution cannot be done (e.g., because the
			   argument types involve template parameters).
			   "address" really means "an rvalue for the function,"
			   which has unknown type and might therefore be a
			   pointer or a pointer to member. */
  tpck_address,		/* Used, pointing to a tpck_member constant, to
			   indicate the address of the indicated member. */
  tpck_sizeof,		/* The template param constant represents the sizeof
			   operator applied to a type or expression that
			   contains a template parameter type. */
  tpck_datasizeof,	/* Same as tpck_sizeof but for the __datasizeof
			   operator. */
  tpck_alignof,		/* The template param constant represents the
			   __ALIGNOF__ operator applied to a type or
			   expression that contains a template parameter
			   type. */
  tpck_uuidof,		/* The template param constant represents the
			   Microsoft __uuidof operator applied to a type
			   that contains a template parameter type.  It
			   represents the address of the implied structure. */
  tpck_typeid,		/* The template param constant represents the typeid
			   operator applied to a type or expression that
			   contains a template parameter type.  It represents
			   the address of the implied std::type_info
			   structure.  (Only used when typeid is used for a
			   nontype template argument; e.g. "X<&typeid(Y)>".)*/
  tpck_noexcept,	/* The template param constant represents the noexcept
			   operator applied to an expression that
			   contains a template parameter type. */
  tpck_template_ref,	/* The template param constant provides the address of
			   an unknown function template or a representation of
			   a static data member template, and a set of explicit
			   template arguments for that template.  Very similar
			   to either tpck_unknown_function or tpck_member. */
  tpck_integer_pack,	/* The template param constant represents a dependent
			   "__integer_pack(N)..." construct. */
  tpck_destructor,	/* The template param constant represents a destructor
			   of a nonreal class. */
  tpck_dependent_constant,
			/* The template param constant wraps a non-dependent
			   constant to cause it to be treated as template-
			   dependent. */
  tpck_concat_string_literals
			/* The template param constant represents the
			   concatenation of string literal constants, at least
			   one of which is dependent. */
};


typedef uint32_t a_template_param_list_pos;
			/* A ordinal position number within a template
			   parameter or argument list (i.e., a given
			   parameter is the Nth parameter in the list). */

typedef int32_t a_template_nesting_depth;
			/* When templates are nested within other templates,
			   the nesting depth is used to associate a template
			   parameter with a given template declaration level.
			   The first level is 1, the second 2, etc.  Levels
			   <= 0 have special meanings (see below). */

#define NO_NESTING_DEPTH	0
			/* Depth used to indicate that a template parameter has
			   no specified depth.  This is used for template
			   template parameters. */
#define AUTO_TYPE_NESTING_DEPTH	-1
			/* Depth used to indicate that the template parameter
			   really represents an "auto" or "decltype(auto) type
			   specifier. */
#define CLASS_TEMPLATE_PLACEHOLDER_NESTING_DEPTH	-2
			/* Depth used to indicate that the template parameter
			   represents a C++17 class template being used for
			   class template argument deduction. */
#define BIT_PRECISE_INT_NESTING_DEPTH	-3
			/* Depth used to indicate that the constraint union
			   contains the width constant for a dependent
			   _BitInt type. */
#define PLAIN_AUTO_TYPE_POS_NUMBER 1
			/* When a template parameter nesting depth is
			   AUTO_TYPE_NESTING_DEPTH, and its position number is
			   this value, it represents a plain "auto" type
			   specifier. */
#define DECLTYPE_AUTO_POS_NUMBER 2
			/* When a template parameter nesting depth is
			   AUTO_TYPE_NESTING_DEPTH, and its position number is
			   this value, it represents a "decltype(auto)" type
			   specifier. */

#if MICROSOFT_EXTENSIONS_ALLOWED
typedef int32_t a_generic_param_seq_number;
			/* Each generic parameter of a generic class is
			   assigned a sequence number as follows.  Parameters
			   of a namespace scope generic class are numbered 1
			   (for the first parameter) through N (for the last
			   parameter).  Parameters of a nested generic class
			   are similarly numbered, but starting with L+1,
			   where L is the sequence number assigned to the last
			   parameter of the nearest enclosing generic class. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

typedef struct a_template_param_coordinate *a_template_param_coordinate_ptr;
typedef struct a_template_param_coordinate {
  /* Structure used to identify a template parameter from a template
     declaration using its list position and template nesting depth.
     Template parameters with matching coordinates are equivalent. */
  a_template_param_list_pos
		position;
			/* Ordinal value indicating the position of the
			   template parameter in its declaration list (1 is
			   first param declared, 2 is second, etc.). */
  a_template_nesting_depth
		depth;
			/* Ordinal value indicating with which of a set of
			   nested templates a given parameter is
                           associated. */
} a_template_param_coordinate;


enum a_character_kind : a_byte {
  /* String and character literals can involve one of several character kinds
     represented by the following enumerator constants. */
  chk_char,		/* The "normal" string or character literal,
			   expressed without prefix or, before C++20, with
			   prefix "u8" (character type "char"). */
  chk_default = chk_char,
  chk_wchar_t,		/* String or character literals expressed with the
			   prefix "L" (e.g., L'x') (character type wchar_t). */
  chk_char8_t,		/* String or character literals expressed with the
			   prefix "u8" (character type char8_t, in C++20
			   only; in earlier versions of C++, "u8"-prefixed
			   literals implied type "char"). */
  chk_char16_t,		/* String or character literals expressed with the
			   prefix "u" (character type char16_t).  This is an
			   extension specified in ISO/IEC TR 19769. */
  chk_char32_t,		/* String or character literals expressed with the
			   prefix "U" (character type char32_t).  This is an
			   extension specified in ISO/IEC TR 19769. */
  chk_last		/* Must be last. */
};


/* Number of bits required to hold a character code kind.  chk_last need not
   be accounted for. */
#ifndef NUM_BITS_FOR_CHARACTER_KIND
#define NUM_BITS_FOR_CHARACTER_KIND 3
#endif /* ifndef NUM_BITS_FOR_CHARACTER_KIND */
#if NUM_BITS_FOR_CHARACTER_KIND < 3
 #error -- NUM_BITS_FOR_CHARACTER_KIND cannot be less than 3
#endif /* NUM_BITS_FOR_CHARACTER_KIND < 3 */

#if DEBUG
/*
Table of names corresponding to special function kinds, for debug purposes.
*/
EXTERN_CONSTINIT_ARRAY(a_const_char*, db_special_function_kinds, sfk_last + 1)
#if VAR_INITIALIZERS
= {
   "none", "constructor", "destructor", "conversion", "literal operator",
   "operator", "lambda entry point", "deduction guide",
#if MICROSOFT_EXTENSIONS_ALLOWED
   "static constructor", "finalizer",
   "IDisposable::Dispose implementation", "Dispose(bool)",
   "Object::Finalize overrider",
   "property getter", "property setter",
   "event add", "event remove", "event raise",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if BUILTIN_FUNCTIONS_ENABLED
   "gnu sync concrete function",
   "gnu atomic nongeneric function",
   "gnu atomic generic function",
   "__builtin_operator_new",
   "__builtin_operator_delete",
#endif /* BUILTIN_FUNCTIONS_ENABLED */
   "last" /* used to check that initialization is right. */
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(db_special_function_kinds)
#endif /* DEBUG */


typedef struct a_subobject_path  *a_subobject_path_ptr;
typedef struct a_subobject_path {
  /* Description of the subobject path for an address constant.  E.g., if
     "&x.f[3]" is the address of a global variable with array-type field f, it
     would point to a list of two subobject path entries: the first pointing
     to the entry representing field f, and the second holding the element
     offset within that field (+3, in this case).  An entry can indicate one
     of three things: (1) a field selection, (2) a base class selection, or
     (3) a numeric offset.  Every field selection is explicit, including
     selections of anonymous union parent fields.  A sequence of base class
     casts only generates a single base class selection (the entry will point
     to the resulting base class entry within the most derived object).
     The path for an address obtained with a multi-level subscript (e.g.,
     arr[k][l][m]) will include an entry for each subscript. */
  a_subobject_path_ptr
		next;
			/* Pointer the next entry in this path (or NULL if
			   none). */
  a_bit_field	is_offset:1;
			/* TRUE if this element designates an offset, either
			   from the start of an array, or from a field treated
			   as a one-element array. */
  a_bit_field	is_base_class:1;
			/* TRUE if this element designates a base class
			   subobject. */
  a_bit_field	is_converted:1;
			/* TRUE if is_offset is TRUE and an array-to-pointer
			   conversion was applied to the result.  Any
			   subsequent offset operation should add a new
			   subobject path entry. */
  union {
    /* When is_offset == FALSE and is_base_class == FALSE. */
    a_field_ptr
		field;
			/* The selected field entry. */
    /* When is_offset == TRUE. */
    a_targ_ptrdiff_t
		ptr_offset;
			/* The offset applied to the pointer (an element
			   count, not a byte count). */
    /* When is_base_class == TRUE. */
    a_base_class_ptr
		base_class;
			/* The selected base class subobject. */
  } variant;
} a_subobject_path;
		

/*
Numbering for scopes.  Each new scope is given a number.  These
numbers are unique identifiers for each scope, not simply the nesting
level of the scope.  Also, each struct or union has a unique scope
number for its member fields, even though no true scope with that
number is created.  In C++, a class/struct/union has a true scope
associated with it.  These scope numbers are mostly of interest to the
front end.
*/
typedef int32_t a_scope_number;
#define MAX_SCOPE_NUMBER INT32_MAX
#define NO_SCOPE_NUMBER ((a_scope_number)-1)
			/* Scope number used for things without scope. */
#define FILE_SCOPE_NUMBER 0
			/* Scope number for the file scope.  Note that in
			   the front end the variable file_scope_number
			   should be used instead if a secondary translation
			   unit might be involved. */


/*
Token information for token sequences.
*/
typedef struct a_token_sequence_entry *a_token_sequence_entry_ptr;
struct a_token_sequence_entry {
  a_token_sequence_entry
		*next;
			/* Pointer to the next token in the sequence (or
			   NULL if this is the last token in the sequence). */
  a_token_kind	token_kind;
			/* The token kind. */
  a_source_position
		position;
  a_const_char
		*spelling;
			/* For tokens other than constants and interpolators
			   the spelling of the token. */
};


typedef struct a_token_sequence *a_token_sequence_ptr;
struct a_token_sequence {
  a_token_sequence_entry
		*tokens;
			/* A representation of tokens (and pseudo-tokens) for
			   a token sequence appearing in the source, excluding
			   the operands of interpolators.  (NULL for a token
			   sequence resulting from constant-evaluation.) */
  void		*token_cache;
			/* Pointer to a_token_cache.  For front-end use
			   only. */
};

/*
Information identifying a reflection value.  This is used both for the
representation of ck_reflection constants and in the interpreter.
*/
struct a_reflection_value {
  a_tagged_pointer
		entity;
			/* The entity represented by this reflection value. */
  a_scope_number
		local_scope_number;
			/* If the entity referred to by the reflection value
			   is local, this holds the scope number of the nearest
			   enclosing scope of that entity. */
};

#if DO_IL_LOWERING
/*
Define a union type to store either a pointer to a field or a base class.
This is used when lowering an optimized empty class (which could be either
an optimized base class or an optimized empty field).  A pointer to either
the field/base class from which a (ck_aggregate or ck_dynamic_init) constant
derived is stored here for easy access during lowering.  When
a_constant::constant_for_base_class is TRUE, the "base" member is active,
otherwise "field" is.
*/
union a_field_or_base {
  a_field_ptr   field;  /* Points to a field whose is_optimized_empty_class
                           flag is TRUE. */
  a_base_class_ptr
                base;   /* Points to a base class whose
                           is_optimized_empty_base flag is TRUE. */
};

#endif /* DO_IL_LOWERING */

typedef struct a_constant {
  /* Description of a constant.  Also used as an element on an initializer
     list; in such cases, it may indicate something about the initialization
     rather than simply a constant. */
  /* The source_corresp field must be first. */
  a_source_correspondence
                source_corresp;
                        /* Information on any source entity that corresponds
                           to this entity. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		end_position;
			/* For constants representing literals in the
			   source and for initializer list elements other
			   than ck_designators, the ending position of the
			   token or initializer element; otherwise,
			   null_source_position.  (The corresponding
			   starting position is given by
			   source_corresp.decl_position.) */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_constant_ptr
                next;
                        /* Next constant declared in the same scope, or
                           next on whatever list the constant appears on
                           (for example: enumeration constants, constants
                           in an initialization list).  NULL if the constant
                           is not on a list, or is the last on a list. */
  a_type_ptr    type;
                        /* The type of the constant.  Will be compatible
                           with the representation below.  A ck_init_repeat or
                           ck_designator entry has a NULL type pointer. */
  a_type_ptr	orig_type;
			/* If the constant is converted to a different type
			   (using the same representation), this is the
			   constant's original type. */
  an_expr_node_ptr
                expr;
			/* If the constant is not just a literal this points
			   to an expression node representing that constant.
			   Otherwise, NULL.  If memory region constraints do
			   not permit direct pointing, this is NULL and
			   local_expr_ref is TRUE: The backing expression can
			   then be retrieved using find_local_expr_node.
			   (Note that for some implicitly-converted constants,
			   this field is NULL and the conversion's original
			   type is recorded in orig_type instead.)  Note also
			   that this expression is never lowered (even in
			   configurations that perform lowering). */
  an_expr_rescan_info_entry_ptr
		rescan_info;
			/* For constants (particularly for nontype template
			   arguments) scanned in templates that might be
			   rescanned later to redo semantic analysis,
			   points to extra front-end-only information that
			   is needed for the rescan.  NULL otherwise. */
#if DO_IL_LOWERING
  a_variable_ptr
                assoc_var;
                        /* When non-NULL, points to an associated variable
                           that has been assigned by IL lowering.  Used during
                           lowering of pointer-to-member, complex, and string
                           literal constants.  Also used in the C-generating
                           back end, as a "next" pointer to maintain a list of
                           wide string literal constants that are rewritten to
                           refer to a variable.  NULL otherwise. */
#endif /* DO_IL_LOWERING */
  ENUM_TYPE_FOR_BIT_FIELD(a_character_kind)
		character_kind:NUM_BITS_FOR_CHARACTER_KIND;
			/* If this constant represents a character or string
			   literal, this field indicates the character kind
			   (e.g., chk_wchar_t for L"..." strings).  Otherwise,
			   the field is set to chk_default (which equals
			   chk_char). */
  a_bit_field	implicit_cast:1;
                        /* If this is TRUE, then the value indicated by the
                           representation has been cast to the type
                           indicated above and it's not a "natural" fit.
                           Used for integer constants cast to pointer types
                           and one pointer type cast to another.  Also used
                           for the representation of nullptr (a zero-valued
                           integer of a nullptr type), even though there is
                           no casting involved.  Note that, despite the
                           name, this cast is not necessarily implicit in
                           the source; it might be an explicit cast. */
  a_bit_field	explicit_cast_applied:1;
			/* TRUE when implicit_cast is TRUE and some part of
			   the type change is explicit in the source code.
			   Also TRUE for functional notation casts applied to
			   braced lists ("T{}"). */
  a_bit_field	is_reinterpret_cast:1;
			/* If this is TRUE, implicit_cast will also be
			   TRUE, and the cast was a reinterpret_cast in
			   the source code.  Only TRUE in C++. */
  a_bit_field	is_reinterpret_like_cast:1;
			/* If this is TRUE, this cast has "reinterpret_cast"
			   semantics.  Currently set, e.g., when folding an
			   explicit cast from a pointer type to an integral
			   type or tightening an exception specification on a
			   function pointer.  Can be TRUE in C and C++
			   modes. */
  a_bit_field	non_arithmetic:1;
                        /* This constant should not be considered to be
                           arithmetic; it's probably a bit mask of some kind.
                           Set for hexadecimal and octal constants, and
                           for results of folding constant bit operations.
                           Used to suppress some warnings on implicit type
                           changes. */
  a_bit_field	is_simple_zero:1;
			/* TRUE if the original version of this constant
			   was simply "0".  This is significant for the
			   case of a virtual function pure specifier in C++. */
  a_bit_field	null_pointer_constant_ruled_out:1;
			/* If TRUE, this constant has been subjected to casts
			   or other operations that rule it out as a null
			   pointer constant.  This is unrelated to whether
			   the constant actually has the value zero. */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	null_keyword:1;
			/* If TRUE, this constant was expressed with a
			   special GNU keyword ("__null") in the source.
			   Although it is semantically equivalent to a plain
			   "0", it is meant to be a null pointer constant. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_bit_field	nullptr_keyword:1;
			/* If TRUE, this constant was expressed with the
			   nullptr keyword (in both C++/CLI and non-C++/CLI
			   modes; i.e., this flag reflects only the
			   spelling of the keyword and not whether the type
			   is the managed or standard variant of the
			   nullptr types).  This is used to distinguish
			   direct uses of nullptr from other expressions
			   with a nullptr type. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	native_nullptr_keyword:1;
			/* If TRUE, this constant was expressed with the
			   __nullptr keyword.  This is used to distinguish
			   between nullptr and __nullptr in C++/CLI mode,
			   where the keywords have different types. */
  a_bit_field	ptr_to_mem_constant_construct:1;
			/* If TRUE, this constant is the result of a construct
			   of the form &C::m that produces a pointer-to-member
			   constant. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	explicit_braces_on_aggregate:1;
			/* For a ck_aggregate constant in an initializer,
			   TRUE if the values were surrounded by explicit
			   braces { ... }.  This affects the meaning of
			   some designated initializers. */
  a_bit_field	explicit_parentheses_on_aggregate:1;
			/* For a ck_aggregate constant in an initializer,
			   TRUE if the values were surrounded by explicit
			   parentheses ( ... ).  Possible in C++20 mode. */
  a_bit_field	from_undefined_preproc_id:1;
			/* This constant was generated from a reference to
			   an undefined preprocessing identifier (i.e.,
			   it's zero). */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
  a_bit_field	flexible_array_initializer:1;
			/* For a ck_aggregate or ck_string constant in an
			   initializer, TRUE if the initializer is for a
			   flexible array member. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
  a_bit_field	uses_designated_initializers:1;
			/* For a ck_aggregate constant in an initializer,
			   TRUE if the initializer contains designated
			   initializers (possibly within a nested aggregate
			   constant). */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_literal_field:1;
			/* TRUE if this entry represents a C++/CLI literal
			   field (i.e., a member declared with the context-
			   sensitive keyword "literal"). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	is_pack_expansion:1;
			/* TRUE if (in an aggregate initializer list) this
			   constant represents a variadic template pack
			   expansion.  When that's the case, the correspondence
			   between initializer constants and initialized
			   members can't be maintained. */
#if BACK_END_IS_C_GEN_BE
  a_bit_field	elide_aggregate_braces:1;
			/* TRUE for a ck_aggregate constant that should not
			   be enclosed in braces in the output of the
			   C-generating back end. */
#endif /* BACK_END_IS_C_GEN_BE */
  a_bit_field	is_named_constant_definition:1;
			/* TRUE if this constant is the definition of a
			   named constant (the enumerator in the definition
			   of an enumeration or a non-standard class member
			   constant). */
  a_bit_field	partial_aggr_value:1;
			/* TRUE for a ck_aggregate or ck_string constant whose
			   list of constants or string length does not cover
			   all the elements of the destination type. */
  a_bit_field	is_partially_initialized:1;
			/* Similar to partial_aggr_value but also TRUE if a
			   direct or indirect subaggregate constant has
			   partial_aggr_value set to TRUE, or if this constant
			   involves a designator into a non-union aggregate
			   (possibly in a subaggregate) */
  a_bit_field	implicit_aggr_element:1;
			/* TRUE if this constant was generated implicitly for
			   an aggregate initializer that does not explicitly
			   specify values for all the elements of the
			   destination type. */
  a_bit_field	is_compound_literal:1;
			/* TRUE if this is an aggregate constant resulting
			   from a compound literal construct. */
  a_bit_field	is_result_of_constexpr_call:1;
			/* TRUE if this constant is the result of calling
			   a constexpr function or constexpr constructor.
			   The interesting case is when the constant is
			   a ck_aggregate, but this flag can be set in
			   any kind of constant.  Also set for something
			   like "A()" when expanded to a constant for a
			   trivial default constructor. */
  a_bit_field	is_generic_initializer:1;
			/* TRUE if this is a constant representing an
			   initializer for an entity in a template-dependent
			   context.  In such context, type checking is limited
			   and thus the type of the constant may not match up
			   with the type of the entity being initialized. */
#if DO_IL_LOWERING
  a_bit_field	has_been_prelowered:1;
			/* Flag that is used during lowering to ensure that
			   ck_aggregate constants are pre-lowered only once.
			   TRUE if the ck_aggregate has been pre-lowered. */
  a_bit_field	vptr_has_been_lowered:1;
			/* Flag that is used during lowering to ensure that
			   ck_aggregate constants are only visited one time.
			   TRUE for ck_aggregate constants that have had
			   an initializer for the __vptr field inserted. */
  a_bit_field
		initializes_empty_object:1;
			/* TRUE if the constant initializes an "empty object",
			   i.e., an optimized empty base class or an optimized
			   empty class.  These constants are lowered (because
			   they may contain dynamic initialization) and then
			   removed from the final aggregate constant. */
  a_bit_field
		is_implicit_initialization:1;
			/* TRUE if this constant represents "implicit
			   initialization" for a field in an aggregate, or
			   for a ck_aggregate, if any field of the aggregate
			   initialization is implicitly initialized. */
#endif /* DO_IL_LOWERING */
  a_bit_field	constant_for_base_class:1;
			/* TRUE if this constant (under a ck_aggregate) is
			   the value for a base class subobject. */
  a_bit_field	constant_for_base_class_from_constexpr_folding:1;
			/* TRUE if this constant (under a ck_aggregate) is
			   the value for a base class subobject generated by
			   constexpr folding of a constructor call. */
  a_bit_field	part_of_constexpr_master_expr:1;
			/* TRUE if this constant was created as part of the
			   master copy of an expression to be used later to
			   do constexpr evaluation.  As such, if should never
			   be incorporated directly into "real" IL; a copy
			   should always be made. */
  a_bit_field	local_expr_ref:1;
			/* TRUE if the expression normally associated with
			   a_constant::expr is stored in a function scope
			   memory region while this constant is stored in the
			   file scope memory region.  In that case,
			   a_constant::expr will be NULL and the expression
			   can be found using find_local_expr_node instead. */
  a_bit_field	folded_statement_expression:1;
			/* TRUE if this constant resulted from folding an
			   expression containing a GNU statement expression,
			   or from a constant-folded operation on entries that
			   have this flag set to TRUE. */
  a_bit_field	formed_from_promoted_storage:1;
			/* TRUE if this is a constant that was originally
			   created from an interpreter object with dynamic
			   storage duration but is the target of a
			   ck_address/abk_constant constant.  Note that C++
			   does not currently allow the promotion of
			   interpreter objects with dynamic storage duration to
			   constants that are referred to by
			   ck_address/abk_constant constants; thus, this
			   promotion is currently only available as part of an
			   implementation detail (e.g., for use by the
			   "__builtin_source_location()" builtin). */
  a_constant_repr_kind
                kind;
                        /* The kind of representation for the constant. */
  union {
    /* When kind == ck_error or ck_void, no variant fields. */
#if MICROSOFT_EXTENSIONS_ALLOWED
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
    /* Likewise when kind == ck_upc_mythread. */
#endif /* UPC_EXTENSIONS_ALLOWED */
    /* When kind == ck_integer or ck_upc_threads: */
    an_integer_value
	        integer_value;
                        /* A target integer.  Used for long, int, short,
			   and char, in both signed and unsigned forms,
			   and for enumerated type constants. */
#if UPC_EXTENSIONS_ALLOWED
			/* When kind == ck_upc_threads, integer_value is
			   the multiple of THREADS to be represented. */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if FIXED_POINT_ALLOWED
    /* When kind == ck_fixed_point. */
    a_fixed_point_value
		fixed_point_value;
			/* A fixed-point value in internal form. */
#endif /* FIXED_POINT_ALLOWED */
    /* When kind == ck_string: */
    struct {
      a_targ_size_t
                length;
                        /* Length of the string, in bytes.  Includes the
                           trailing null, if any.  (Be careful: string
                           literals may have more than one null, or none at
                           all -- use the length.) */
      a_const_char
		*value;
                        /* The bytes of the string, in target machine form
                           as a sequence of bytes, in order of ascending
                           memory addresses.  The characters and escape
                           sequences have all been translated into target
                           computer binary values.  Note that two or more
                           string constants may point to the same string
                           text.  When embed_expansion is TRUE, the bytes
                           of the expansion are interpreted as unsigned,
                           regardless of the signedness of plain char. */
#if PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED
      a_const_char
		*embed_directive;
			/* If embed_expansion is TRUE, this points to a
			   null-terminated string representing the tokens
			   of the associated #embed directive.  NULL when
			   embed_expansion is FALSE. */
#endif /* PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED */
#if DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
      unsigned long
		sequence_number;
			/* A sequence number assigned to string literals used
			   within function scopes.  This is used to permit
			   string literals in inline functions to have
			   a uniform address across a program.  It is also
			   used for mangling of string literal names in
			   the IA64 ABI.  Has a value of zero for string
			   literals that require no special processing.  This
			   field is set for functions for which there
			   is the potential of having more than one copy of
			   the function in a program (e.g., extern inline
			   functions and templates in certain
			   configurations). */
#endif /* DO_IL_LOWERING && ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */
      a_string_or_char_literal_kind
		literal_kind;
			/* Captures the encoding prefix of the literal this
			   constant represents, if any, as well as whether
			   it was a raw string literal.  For string
			   constants that are not associated with string
			   literals, including optimizable #embed
			   expansions, has the value SCLK_NOT_A_LITERAL. */
      a_bit_field
		func_name_tok:1;
			/* TRUE if this constant contains the spelling of a
			   function name token (__PRETTY_FUNCTION__, etc.).
			   Used in the prototype instantiation IL to allow
			   a function template definition to be
			   reconstructed in its original form instead of
			   reflecting the generic function name and
			   parameters. */
      a_bit_field
		embed_expansion:1;
			/* TRUE if this constant is the value of an
			   optimizable #embed expansion - i.e., a #embed
			   with either no prefix or suffix parameters or in
			   which the parameters' operands are lists of
			   values that can be merged with the file
			   contents, allowing the expansion to be treated
			   as a single block and not a sequence of integer
			   constant tokens. */
    } string;
    /* When kind == ck_float: */
#if C99_IL_EXTENSIONS_SUPPORTED
    /* Also, when kind == ck_imaginary: */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    an_internal_float_value
                float_value;
                        /* A floating-point value (real or imaginary) in
			   internal form. */
#if C99_IL_EXTENSIONS_SUPPORTED
    /* When kind == ck_complex: */
    an_internal_complex_value_ptr
                complex_value;
                        /* A complex value, represented internally as two
                           floating-point values. */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    /* When kind == ck_address: */
    struct {
      an_address_base_kind
                kind;
      a_bit_field
		one_past_the_end:1;
			/* If TRUE, this address was obtained by going "one
			   position past the end" of an object or subobject. */
      a_bit_field
		is_object_reflection:1;
			/* TRUE when a reflection value (an iek_constant
			   reflection) uses this address constant to denote an
			   object -- as produced by std::meta::reflect_object,
			   object_of, and reflect_constant_array -- rather than
			   to hold a pointer value.  The constant addresses the
			   object; type_of the reflection is the object's type
			   (not a pointer type).  This distinguishes an object
			   reflection from a reflection of a pointer value that
			   happens to hold the same address, so that is_object
			   and reflection equality behave correctly. */
      union {
        /* The entity whose address is the base for this address constant. */
        /* When kind == abk_cli_array, no variant fields. */
        /* When kind == abk_routine: */
        a_routine_ptr
                routine;
        /* When kind == abk_variable: */
        a_variable_ptr
                variable;
        /* When kind == abk_constant or kind == abk_temporary: */
	/* abk_constant produces the address of a constant that already has
	   an inherent existence in memory, e.g., a string.  abk_temporary
	   produces the address of a static temporary that is initialized
	   with the pointed-to constant, for constants that don't necessarily
	   have an existence in memory, e.g., an integer constant.  Each
	   abk_constant that points to the same constant represents the
	   same address; each abk_temporary pointing to the same constant
	   uses the same temporary and therefore represents the same address.
	   The underlying constant may be a shared constant in either case. */
	/* Note that in C++/CLI mode, a ck_address/abk_constant constant
	   of type System::String^, where the addressed constant is a
	   string literal, is used to represent the result of implicitly or
	   explicitly converting a string literal to that type, even though
	   that is a run-time operation (allocating the System::String object
	   on the gc-heap and initializing it).  This is necessary because
	   the Microsoft compiler treats such a construct as a compile-time
	   constant. */
        a_constant_ptr
                constant;
			/* The constant whose address is taken (abk_constant),
			   or whose value is placed in the temporary
			   (abk_temporary). */
        /* When kind == abk_uuidof or abk_typeid or abk_cli_typeid: */
        a_type_ptr
		type;	/* For abk_uuidof, the value of the constant is the
			   address of a structure representing the uuid_string
			   associated with the indicated type (NULL for the
			   address of a structure representing a zero GUID).
			   For abk_typeid, the value of the constant is the
			   address of the std::type_info structure associated
			   with the given type.  For abk_cli_typeid, the value
			   of the constant is a handle to the System::Type
			   object associated with the given type. */
        /* When kind == abk_label: */
	a_label_ptr
		label;
        /* When kind == abk_param_ref: */
        struct {
          unsigned int
		param_num;
			/* The number of the parameter being referenced (the
			   first parameter is number one). */
        } param_ref;
      } variant;
      a_targ_ptrdiff_t
                offset;
                        /* Byte offset from the base address. */
      a_subobject_path_ptr
		subobject_path;
			/* A description of the subobject referred to by
			   the given address.  NULL if the address refers
			   to the complete object or to a function. */
    } address;
    /* When kind == ck_ptr_to_member: */
    struct {
      /* A C++ pointer-to-member (data or function). */
      /* Note that implicit_cast will be TRUE if the constant is a NULL
         pointer to member or if casting_base_class is non-NULL. */
      a_base_class_ptr
		casting_base_class;
			/* If non-NULL, indicates the derived or base class
			   to which the pointer-to-member has been cast.
			   Always NULL for a NULL pointer-to-member
			   constant. */
      a_name_reference_ptr
		name_reference;
			/* The form of the expression that caused the creation
			   of this entry. */
      a_bit_field
		cast_to_base:1;
			/* If TRUE, the base class given by casting_base_class
			   is a base class of the original class.  If FALSE,
			   casting_base_class indicates a derived class (that
			   is, it indicates the base class [of the derived
			   class] that is the original class). */
      a_bit_field
		is_function_ptr:1;
			/* TRUE if the pointer is to a member function,
			   FALSE if to a data member. */
      union {
        /* When is_function_ptr == TRUE: */
        a_routine_ptr
		routine;
			/* The routine for the member function pointed to.
			   NULL for a NULL pointer-to-member. */
        /* When is_function_ptr == FALSE: */
        a_field_ptr
		field;	/* The field for the data member pointed to.
			   NULL for a NULL pointer-to-member. */
      } variant;
    } ptr_to_member;
#if GNU_EXTENSIONS_ALLOWED
    /* When kind == ck_label_difference: */
    struct {
      a_constant_ptr
		from_address;
			/* The label address from which the distance is
			   computed; i.e., &&x in &&y - &&x. */
      a_constant_ptr
		to_address;
			/* The label address from which the distance is
			   computed; i.e., &&y in &&y - &&x. */
    } label_difference;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    /* When kind == ck_stack_offset: */
    struct {
      /* A constant for the stack offset of a local variable.
         Used in exception handling cleanup tables when IL lowering does
         partial lowering of exception handling. */
      a_variable_ptr
		variable;
			/* The variable. */
      a_targ_size_t
		offset;
			/* Offset relative to the variable. */
    } stack_offset;
#endif /* DO_IL_LOWERING && ... */
    /* When kind == ck_dynamic_init: */
    struct {
      a_dynamic_init_ptr
		ptr;    /* A pointer to the dynamic-init entry that describes
			   a required dynamic initialization that appears in
			   the middle of a ck_aggregate constant list used as
			   an initializer.  Obviously, this is not a constant.
			   Only used in C++. */
#if DO_IL_LOWERING
      a_field_or_base
		field_or_base;
			/* When the constant refers to an optimized empty
			   object (in an aggregate), refers to the associated
			   field or base class. */
#endif /* DO_IL_LOWERING */
    } dynamic_init;
    /* When kind == ck_aggregate: */
    /* A ck_aggregate constant is used only in initialization.  As such, it
       is always an unshared constant. */
    struct {
      a_constant_ptr
                first_constant,
                last_constant;
                        /* List of constants in { } in an initialization.
                           Both pointers are NULL if the list is empty. */
      a_bit_field
		has_dynamic_init_component:1;
			/* TRUE if one of the constants on the list is a
			   ck_dynamic_init entry, or a ck_aggregate entry with
			   this flag set to TRUE. */
      a_bit_field
		added_const_for_template_param:1;
			/* TRUE if the type of the constant was made "const"
			   because it represents the use of a template
			   parameter of class type. */
#if DO_IL_LOWERING
      a_field_or_base
		field_or_base;
			/* When the constant refers to an optimized empty
			   object (in an aggregate), refers to the associated
			   field or base class. */
#endif /* DO_IL_LOWERING */
    } aggregate;
    /* When kind == ck_init_repeat: */
    /* A ck_init_repeat constant is used only in initialization.  As such,
       it is always an unshared constant.  Used in C++ to initialize an
       array of class objects with constructor initialization and with
       designators to indicate a repeated value.  In a multidimensional
       array, the repeated constant can be an aggregate initializing the
       array elements at that level or a constant giving the value of the
       leaf elements of the array; in the latter case, the count gives the
       number of leaf elements to be initialized, not the number of array
       elements at the level at which the repeated constant appears.  The
       cases can be distinguished by the type of the repeated constant. */
    struct {
      a_constant_ptr
                constant;
                        /* The constant to be repeated. */
      a_targ_size_t
		count;
			/* The repeat count (greater than zero).  A count of
			   zero is used for new and delete of an array, and
			   means "use the number of elements recorded along
			   with the storage allocation".  A similar case occurs
			   when initializing a variable-length array. */
      a_byte_boolean
		multidimensional_aggr_tail_not_repeated;
			/* This flag is used to indicate that the repeated
			   constant contains at least one ck_aggregate (without
			   braces) whose first constant is a ck_designator
			   and contains more than one constant in the
			   aggregate.  This situation arises from the use
			   of GNU range extended designators in initializers.
			   When this flag is set only the first constant
			   on the list of the aggregate constant is repeated
			   the first (count-1) times.  On the final iteration, 
			   the entire list of constants in the aggregate is
			   repeated.  This is used to mimic the gcc initializer
			   layout in multi-dimensional arrays. */
    } init_repeat;
    /* When kind == ck_template_param (C++ front end only, except when
       prototype instantiations are passed to a back end): */
    struct {
      a_template_param_constant_kind
		kind;
			/* The kind of template param constant. */
      a_bit_field
		is_qualified_name:1;
			/* For tpck_unknown_function, TRUE if a qualified
			   name was used in the source code. */
      a_bit_field
		has_address_of:1;
			/* For tpck_unknown_function, TRUE if this
			   represents a source construct of the form &T::f
			   rather than just T::f.  (Note that for a
			   tpck_template_ref constant, the controlling flag
			   appears in the associated tpck_unknown_function
			   constant, i.e.,
			   variant.template_param.variant.template_ref.con,
			   and not directly in the tpck_template_ref
			   constant.) */
      a_bit_field
		is_pack:1;
			/* TRUE if this is a template parameter pack. */
      a_bit_field
		has_generic_cast_for_nontype_template_param:1;
			/* TRUE if this is a constant that includes an implicit
			   generic cast to model the binding of a template
			   argument to a nontype template parameter with a
			   template-dependent type (it should not be skipped
			   during expression rescanning). */
#if PROTOTYPE_INSTANTIATIONS_IN_IL
      a_bit_field
		local_expr_ref:1;
			/* TRUE if the expression normally associated with
			   variant.expr or variant.templ_sizeof.expr below
			   is stored in a function scope memory region
			   while this constant is stored in the file scope
			   memory region.  In that case, the pointer will
			   be NULL and the expression can be found using
			   find_local_expr_node instead. */
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */
      a_bit_field
		do_not_rescan:1;
			/* TRUE if expression nodes associated with this entry
			   should not be rescanned (because the information
			   needed for such rescanning was not recorded).  For
			   front end use only. */
      union {
	/* When template param constant kind == tpck_param: */
        a_template_param_coordinate
		coordinates;
			/* The parameter list position and template nesting
			   depth of the parameter. */
	/* When template param constant kind == tpck_expression: */
	an_expr_node_ptr
		expr;	/* Expression node representing a constant value in
			   terms of an expression involving one or more
			   ck_template_param constants -- e.g., if "I" is
			   a template param constant (of kind tpck_param),
			   "I+1" is also a template param constant (of kind
			   tpck_expression).  Will be NULL if local_expr_ref
			   is TRUE, in which case the expression can be
			   retrieved using find_local_expr_node. */
        /* When template param constant kind == tpck_member, no variant
           fields. */
        /* When template param constant kind == tpck_unknown_function: */
        struct {
          a_type_ptr
		conversion_type;
			/* If the unknown function represents a conversion
			   function, this is the result type; NULL
			   otherwise. */
#if MICROSOFT_EXTENSIONS_ALLOWED
          a_property_or_event_descr_ptr
		property_or_event_descr;
			/* If the unknown function is a C++/CLI accessor,
			   this identifies the property or event.  NULL
			   otherwise. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          struct a_symbol
		*symbol;
			/* The symbol for the overload set of which the
			   unknown function must be a member.  Can be a
			   simple symbol for the sake of generality.
			   Can be NULL for unknown member functions, as
			   the name can be looked up in the parent class.
			   Used in the front end only; cannot be used
			   in back ends. */
          an_opname_kind
		opname_kind;
			/* If the unknown function represents an overloaded
			   operator function, this is the operator kind. */
#if MICROSOFT_EXTENSIONS_ALLOWED
          a_special_function_kind
		special_kind;
			/* If the unknown function is a C++/CLI accessor,
			   this specifies the kind of accessor; sfk_none
			   otherwise. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } unknown_function;
        /* When template param constant kind == tpck_address or
           tpck_dependent_constant: */
        a_constant_ptr
		constant;
			/* For tpck_address, the member whose address is being
			   taken.  For tpck_dependent_constant, the
			   non-dependent constant being wrapped. */
        /* When template param constant kind == tpck_concat_string_literals: */
        a_constant_ptr
		string_literal_list;
			/* A list of constants representing the string literals
			   being concatenated.  Each entry is a ck_string or a
			   ck_template_param/tpck_dependent_constant wrapping a
			   ck_string. */
        /* When template param constant kind == tpck_sizeof, tpck_datasizeof,
           tpck_alignof, tpck_uuidof, tpck_typeid, or tpck_noexcept: */
        struct {
          a_type_ptr
		type;	/* The type whose sizeof, __ALIGNOF__, __uuidof, or
			   typeid is represented.  NULL for __uuidof(0)
			   and noexcept cases. */
          an_expr_node_ptr
		expr;	/* If the sizeof etc. was applied to an expression,
			   stored in the same memory region as this constant,
			   this points to the expression.  NULL otherwise (in
			   particular, NULL when local_expr_ref is TRUE). */
          a_bit_field
		is_std_alignof:1;
			/* TRUE if this represents the standard C++11 alignof
			   operation. */
        } templ_sizeof;
        /* When template param constant kind == tpck_template_ref: */
        struct {
          a_constant_ptr
		con;
			/* A constant that identifies the (unknown) template,
			   i.e., a tpck_unknown_function constant. */
          a_template_arg_ptr
		arg_list;
			/* The template argument list.  Note that a NULL
			   list is "<>", not the absence of template
			   arguments. */
        } template_ref;
        /* When template param constant kind == tpck_destructor: */
        struct {
          a_type_ptr
		type;	/* The nonreal type whose destructor is represented. */
          a_bit_field
		unqualified:1;
			/* TRUE if the destructor was named without a qualifier
			   (e.g., TRUE in p->~T(), but not in p->~T::T()). */
        } destructor;
        /* When template param constant kind == tpck_integer_pack: */
        a_constant_ptr
		bound;
			/* The dependent bound specified in a construct of the
			   form "__integer_pack(N)...". */
      } variant;
    } template_param;
    /* When kind == ck_designator: */
    /* A ck_designator is only used in initialization, and as such is always
       an unshared constant.  The designated field or element is initialized
       by the constant pointed to by "next". */
    struct {
      a_bit_field
		is_field_designator:1;
			/* TRUE if the designator is for a field. */
      a_bit_field
		is_generic:1;
			/* TRUE if the entity being designated has not been
			   looked up (e.g., in template contexts). */
      a_bit_field
		uses_direct_init_syntax:1;
			/* TRUE if this is a (standard) designator that uses
			   direct-initialization syntax (i.e., a braced
			   initializer without a preceding "=" token). */
      union {
        /* When is_field_designator == TRUE and is_generic == FALSE: */
        a_field_ptr
		field;
                        /* The field indicated by a designator. */
        /* When is_field_designator == FALSE and is_generic == FALSE: */
        a_targ_size_t
		array_element;
                        /* The subscript indicated by the designator. */
        /* When is_field_designator == TRUE and is_generic == TRUE: */
        a_const_char
		*field_name;
			/* The name of the designated field. */
        /* When is_field_designator == FALSE and is_generic == TRUE: */
        a_constant_ptr
		subscript;
			/* A constant representing the subscript of the
			   designated array element.  For a range-designator,
			   this is a list of two constants. */
      } variant;
    } designator;
    /* When kind == ck_reflection: */
    a_reflection_value
		reflection;
			/* The representation of a reflection value (which
			   consists of a tagged pointer and a scope number). */
  } variant;
} a_constant;

/*
Data structures related to types:
*/
enum a_type_kind : a_byte {
  /* Basic kinds of types: */
  tk_error,             /* Error. */
  tk_void,              /* Void -- has no type. */
  tk_integer,           /* All integral types, including enum. */
  tk_enum = tk_integer, /* Synonym for tk_integer. */
#if FIXED_POINT_ALLOWED
  tk_fixed_point,       /* All fixed-point types. */
#endif /* FIXED_POINT_ALLOWED */
  tk_float,             /* All float types. */
#if C99_IL_EXTENSIONS_SUPPORTED
  tk_imaginary,         /* C99 imaginary types. */
  tk_complex,           /* Complex (C99).  Must have the same layout as an
			   array of two reals of the appropriate size. */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  tk_pointer,           /* Pointer type.  Also used for references in C++. */
  tk_routine,           /* Function. */
  tk_array,             /* Array. */
  tk_class,             /* Class. */
  tk_struct,            /* Struct. */
  tk_union,             /* Union. */
  tk_typeref,           /* Use of a typedef, i.e., a type equivalent to
                           another type; also used to add type qualifiers
                           (const or volatile) to a type. */
  tk_ptr_to_member,     /* Pointer-to-member (C++ only). */
  tk_template_param,	/* Type parameter in a (class or function) template
			   declaration (C++ front end only, except when
			   prototype instantiations are passed to a
			   back end). */
#if GNU_VECTOR_TYPES_ALLOWED
  tk_vector,		/* GNU vector types. */
  tk_scalable_vector,	/* Scalable vector types.  Unlike GNU vector types, the
			   vector size of these types is not known at compile
			   time (it is therefore a sizeless type).  As a
			   consequence, these types cannot be used for objects
			   with static or thread-local storage duration, or as
			   data members. */
  tk_scalable_vector_count,
			/* Opaque scalable vector count type.  This is similar
			   to a scalable vector with an element type of bool,
			   but instead of representing a boolean predicate, it
			   represents a counter predicate (which doesn't have a
			   corresponding C++ element type). */
  tk_riscv_vector,	/* RISC-V vector types.  These are similar to scalable
			   vector types but have an additional length
			   multiplier. */
  tk_mfp8,		/* Modal 8-bit floating-point type used by ARM vector
			   extensions.  This is a storage-only type with no
			   built-in arithmetic operations defined. */
  tk_float8e4m3,	/* RISC-V OFP8 E4M3 8-bit floating-point type.  This is
			   a storage-only type with no built-in arithmetic
			   operations defined. */
  tk_float8e5m2,	/* RISC-V OFP8 E5M2 8-bit floating-point type.  This is
			   a storage-only type with no built-in arithmetic
			   operations defined. */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  tk_nullptr,		/* Type of the C++ or C++/CLI nullptr and __nullptr
			   keywords.  There are two nullptr types that have
			   slightly different semantics: the managed
			   nullptr type is the type of the nullptr keyword
			   in C++/CLI, and the standard nullptr type
			   (std::nullptr_t) is the type of the non-C++/CLI
			   nullptr keyword and the __nullptr keyword in
			   Microsoft mode (both C++/CLI and native).  See
			   is_managed_nullptr_type and
			   is_standard_nullptr_type in types.c for
			   details. */
  tk_reflection,	/* Type of the value returned by the reflection
			   operator (prefix ^). */
  tk_unknown		/* Unknown type. */
};


enum an_integer_kind : a_byte {
  /* Enumeration of the possible integer kinds.  Some of these may be the
     same on the target, but they are kept distinct in the front end.
     These must be listed in order of increasing size (or at least
     non-decreasing size), and the kind for each unsigned type must
     immediately follow the kind for the corresponding signed type. */
  /* If you change this, you should also change int_kind_is_signed and
     unsigned_int_kind_of below. */
  ik_char,
                        /* Not used in pcc mode; ik_signed_char or
                           ik_unsigned_char is used instead. */
  ik_signed_char,
  ik_unsigned_char,
  ik_short,
  ik_unsigned_short,
  ik_int,
  ik_unsigned_int,
  ik_long,
  ik_unsigned_long,
#if LONG_LONG_ALLOWED
  ik_long_long,
  ik_unsigned_long_long,
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
  ik_int128,
  ik_unsigned_int128,
#endif /* INT128_EXTENSIONS_ALLOWED */
  ik_bit_precise,
  ik_unsigned_bit_precise,
  ik_last,
  ik_none = ik_last
};


/* Array that indicates, for each integer kind, whether or not it is signed. */
EXTERN_CONSTINIT_ARRAY(EDG_THREAD a_byte_boolean, int_kind_is_signed, ik_last)
#if VAR_INITIALIZERS
= {
  FALSE,	/* ik_char -- updated when signedness of plain char is
		   known. */
  TRUE,		/* ik_signed_char */
  FALSE,	/* ik_unsigned_char */
  TRUE,		/* ik_short */
  FALSE,	/* ik_unsigned_short */
  TRUE,		/* ik_int */
  FALSE,	/* ik_unsigned_int */
  TRUE,		/* ik_long */
  FALSE,	/* ik_unsigned_long */
#if LONG_LONG_ALLOWED
  TRUE,		/* ik_long_long */
  FALSE,	/* ik_unsigned_long_long */
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
  TRUE,		/* ik_int128 */
  FALSE,	/* ik_uint128 */
#endif /* INT128_EXTENSIONS_ALLOWED */
  TRUE,		/* ik_bit_precise */
  FALSE,	/* ik_unsigned_bit_precise */
}
#endif /* VAR_INITIALIZERS*/
EXTERN_CONSTINIT_ARRAY_END(int_kind_is_signed)

/* Array that indicates, for each integer kind, the unsigned integer kind of
   the same size. */
EXTERN_CONSTINIT_ARRAY(an_integer_kind, unsigned_int_kind_of, ik_last)
#if VAR_INITIALIZERS
= {
  ik_unsigned_char,			/* ik_char */
  ik_unsigned_char,			/* ik_signed_char */
  ik_unsigned_char,			/* ik_unsigned_char */
  ik_unsigned_short,			/* ik_short */
  ik_unsigned_short,			/* ik_unsigned_short */
  ik_unsigned_int,			/* ik_int */
  ik_unsigned_int,			/* ik_unsigned_int */
  ik_unsigned_long,			/* ik_long */
  ik_unsigned_long,			/* ik_unsigned_long */
#if LONG_LONG_ALLOWED
  ik_unsigned_long_long,		/* ik_long_long */
  ik_unsigned_long_long,		/* ik_unsigned_long_long */
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
  ik_unsigned_int128,			/* ik_int128 */
  ik_unsigned_int128,			/* ik_unsigned_int128 */
#endif /* INT128_EXTENSIONS_ALLOWED */
  ik_unsigned_bit_precise,		/* ik_bit_precise */
  ik_unsigned_bit_precise,		/* ik_unsigned_bit_precise */
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(unsigned_int_kind_of)

#if FIXED_POINT_ALLOWED

enum a_fixed_point_precision : a_byte {
  /* Enumeration of the fixed-point precisions (listed according to
     increasing size). */
  fpp_short,
  fpp_default,
  fpp_long,
  fpp_last
};


typedef struct a_fixed_point_type_descr {
  /* Description of the characteristics of a fixed-point type. */
  a_fixed_point_precision
		precision;
			/* The precision (short, default, or long) of this
			   fixed-point type. */
  a_bit_field
		is_unsigned:1;
			/* TRUE if this is an unsigned fixed-point type. */
  a_bit_field
		is_fract_type:1;
			/* TRUE if this is a fixed-point type with no integral
			   part (i.e., declared with _Fract). */
  a_bit_field
		saturating:1;
			/* TRUE if this type saturates on overflow.
			   (E.g., a type declaration with _Sat.) */
} a_fixed_point_type_descr;

#endif /* FIXED_POINT_ALLOWED */

enum a_float_kind : a_byte {
  /* Enumeration of the possible float kinds.  In general, floating point
     types that are distinct (for overloading and template specialization)
     should be represented in this list, even if the types share a common
     representation.  Some special considerations apply to certain kinds:

     Some platforms support __float80 and __float128 typedefs to designate
     floating point types.  If those types are distinct from the standard
     types, they are represented using fk_float80 and/or fk_float128
     respectively; otherwise, the standard float kinds are used (e.g., it
     is not uncommon for __float80 and "long double" to designate the same
     type -- if so, fk_long_double is used in both cases).

     The extended floating-point types (described in WG21 document P1467R9)
     must follow the traditional types, as promoted_float_kind relies on
     this ordering.

     If you add floating point types to this enumeration, be sure to update
     exprutil_init() and init_field_alignment_tables() with the appropriate
     information for the new types, as well as the spelling of the types in
     float_kind_name() and the related floating-point suffixes in
     form_float_constant().  See also select_name_from_float_kind()
     (forming the names of related library routines for complex types) when
     adding a new type that is less than fk_first_extended_type.
*/
  fk_float16,
  fk_fp16,              /* __fp16 has the same format as _Float16, but gets
                           different mangling treatment. */
  fk_float,
  fk_float32x,
  fk_double,
  fk_float64x,
  fk_long_double,
  fk_float80,		/* __float80, if distinct. */
  fk_float128,		/* __float128, if distinct. */
  fk_first_extended_type,
  /* The bfloat16 type must be the first extended type, as the
     select_name_from_float_kind() routines depend on it. */
  fk_std_bfloat16 = fk_first_extended_type,
  fk_std_float16,
  fk_std_float32,
  fk_std_float64,
  fk_std_float128,
  fk_last		/* Must be last. */
};


/* Macro to detect an extended floating point type. */
#define is_extended_flt_kind(fk) (fk != (a_float_kind)fk_last && \
                                  fk >= (a_float_kind)fk_first_extended_type)

#if GNU_EXTENSIONS_ALLOWED

/*
Enumeration of type modes, i.e., sizes of types.  Some of these modes
may not be available on some machines.

If you add new type mode kinds, be sure to update type_mode_kind_names.
*/
enum a_type_mode_kind : a_byte {
  tmk_error,          /* An erroneous mode. */
  tmk_first,
  tmk_QI = tmk_first, /* 1-byte integers. */
  tmk_HI,             /* 2-byte integers. */
  tmk_SI,             /* 4-byte integers. */
  tmk_DI,             /* 8-byte integers. */
  tmk_TI,             /* 16-byte integers. */
  tmk_SF,             /* 4-byte floats. */
  tmk_DF,             /* 8-byte floats. */
  tmk_XF,             /* 12-byte floats. */
  tmk_TF,             /* 16-byte floats. */
  tmk_SC,             /* 4-byte complex floats. */
  tmk_DC,             /* 8-byte complex floats. */
  tmk_XC,             /* 12-byte complex floats. */
  tmk_TC,             /* 16-byte complex floats. */
  tmk_none,
  tmk_last = tmk_none
};


/*
Names of machine modes.
*/
EXTERN_CONSTINIT_ARRAY(a_const_char*, type_mode_kind_names, tmk_last + 1)
#if VAR_INITIALIZERS
= {
/* tmk_error */ "error",
/* tmk_QI */    "QI",
/* tmk_HI */    "HI",
/* tmk_SI */    "SI",
/* tmk_DI */    "DI",
/* tmk_TI */    "TI",
/* tmk_SF */    "SF",
/* tmk_DF */    "DF",
/* tmk_XF */    "XF",
/* tmk_TF */    "TF",
/* tmk_SC */    "SC",
/* tmk_DC */    "DC",
/* tmk_XC */    "XC",
/* tmk_TC */    "TC",
/* tmk_last */  "last" /* used to check that initialization is right. */
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(type_mode_kind_names)

#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
/*
Enumeration of input/output constraint categories for GNU extended
asm.  The first block of these is independent of the target processor,
the rest are machine dependent.  Note that some "modifiers" are treated here as
constraints; any modifier that can appear multiple times in a constraint
string is listed here.  Also update asm_operand_constraint_letters when adding
new entries here.
*/
enum an_asm_operand_constraint_kind : a_byte {
  aoc_invalid = 0,
  aoc_end_of_constraint,/* ,: For cases with multiple constraints, indicates
                              the end of the current constraint (other
                              constraints may follow); represented by a comma
                              in the input stream. */
  /* modifiers */
  /* Note that these are parsed, but not acted upon by the front end. */
  aoc_mod_earlyclobber, /* &: modified early, cannot overlap inputs */
  aoc_mod_commutative_ops,
                        /* %: operands are commutative */
  aoc_mod_ignore,       /* #: ignore the rest of this constraint */
  aoc_mod_ignore_char,  /* *: ignore following character when choosing
                              register preferences */
  aoc_mod_disparage_slightly,
                        /* ?: disparage alternative slightly */
  aoc_mod_disparage_severely,
                        /* !: disparage alternative severely */
  /* misc */
  aoc_any,              /* X: unconstrained */
  aoc_general,          /* g: r or i or m */
  aoc_match_0, aoc_match_1, aoc_match_2, aoc_match_3, aoc_match_4,
  aoc_match_5, aoc_match_6, aoc_match_7, aoc_match_8, aoc_match_9,
                        /* 0-9: same as a previous operand */
  /* registers */
  aoc_reg_integer,      /* r: any integer register */
  aoc_reg_float,        /* f: any float register */
  /* memory */
  aoc_mem_any,          /* m: any memory location */
  aoc_mem_load,         /* p: any memory location that is valid for a load/
                              push operation */
  aoc_mem_offset,       /* o: memory location, if (val + sizeof(object))
                           is also acceptable in this context */
  aoc_mem_nonoffset,    /* V: m but not o */
  aoc_mem_autoinc,      /* >: mem, ptr incremented before or after op */
  aoc_mem_autodec,      /* <: mem, ptr decremented before or after op */
  /* immediates */
  aoc_imm_int,          /* i: any integer (including symbolic references) */
  aoc_imm_number,       /* n: any number known to the compiler (no symbols) */
  aoc_imm_symbol,       /* s: any symbolic reference */
  aoc_imm_float,        /* E, F: any floating point constant */
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
  /* registers */
  aoc_reg_a,            /* a: ax */
  aoc_reg_b,            /* b: bx */
  aoc_reg_c,            /* c: cx */
  aoc_reg_d,            /* d: dx */
  aoc_reg_si,           /* s: si */
  aoc_reg_di,           /* d: di */
  aoc_reg_legacy,       /* R: ax bx cx dx si di bp sp (avail. on non-x86-64) */
  aoc_reg_q,            /* q: ax bx cx dx, lower part only (non-x86-64),
                              same as 'r' (x86-64) */
  aoc_reg_Q,            /* Q: ax bx cx dx (non-x86-64), same as 'r' (x86-64) */
  aoc_reg_ad,           /* A: ax dx */
  aoc_reg_float_tos,    /* t: %st(0) */
  aoc_reg_float_second, /* u: %st(1) */
  aoc_reg_sse,          /* x: any SSE register */
  aoc_reg_sse2,         /* Y: any SSE2 register */
  aoc_reg_mmx,          /* y: any MMX register */
  /* immediates */
  aoc_imm_short_shift,  /* I: [0, 32) */
  aoc_imm_long_shift,   /* J: [0, 64) */
  aoc_imm_lea_shift,    /* M: [0, 4) */
  aoc_imm_signed8,      /* K: [-128, 127] */
  aoc_imm_unsigned8,    /* N: [0, 255] */
  aoc_imm_and_zext,     /* L: {0xFF, 0xFFFF} */
  aoc_imm_80387,        /* G: any 80387 standard constant */
  aoc_imm_sse,          /* H: any SSE standard constant */
  aoc_imm_sext32,       /* e: any 32-bit quantity sign extended to 64 bits */
  aoc_imm_zext32,       /* Z: any 32-bit quantity zero extended to 64 bits */
  aoc_cc,               /* @: condition code */
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
  aoc_last
};

/*
Names of operand constraints.  Used by il_display.c.
*/
EXTERN_CONSTINIT_ARRAY(char, asm_operand_constraint_letters, aoc_last + 1)
#if VAR_INITIALIZERS
= {
  /* aoc_invalid */             '@',
  /* aoc_end_of_constraint */   ',',
  /* aoc_mod_earlyclobber */    '&',
  /* aoc_mod_commutative_ops */ '%',
  /* aoc_mod_ignore */          '#',
  /* aoc_mod_ignore_char */     '*',
  /* aoc_mod_disparage_slightly */ '?',
  /* aoc_mod_disparage_severely */ '!',
  /* aoc_any */                 'X',
  /* aoc_general */             'g',
  /* aoc_match_0 */             '0',
  /* aoc_match_1 */             '1',
  /* aoc_match_2 */             '2',
  /* aoc_match_3 */             '3',
  /* aoc_match_4 */             '4',
  /* aoc_match_5 */             '5',
  /* aoc_match_6 */             '6',
  /* aoc_match_7 */             '7',
  /* aoc_match_8 */             '8',
  /* aoc_match_9 */             '9',
  /* aoc_reg_integer */         'r',
  /* aoc_reg_float */           'f',
  /* aoc_mem_any */             'm',
  /* aoc_mem_load */            'p',
  /* aoc_mem_offset */          'o',
  /* aoc_mem_nonoffset */       'V',
  /* aoc_mem_autoinc */         '>',
  /* aoc_mem_autodec */         '<',
  /* aoc_imm_int */             'i',
  /* aoc_imm_number */          'n',
  /* aoc_imm_symbol */          's',
  /* aoc_imm_float */           'F',
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
  /* aoc_reg_a */               'a',
  /* aoc_reg_b */               'b',
  /* aoc_reg_c */               'c',
  /* aoc_reg_d */               'd',
  /* aoc_reg_si */              'S',
  /* aoc_reg_di */              'D',
  /* aoc_reg_legacy */          'R',
  /* aoc_reg_q */               'q',
  /* aoc_reg_Q */               'Q',
  /* aoc_reg_ad */              'A',
  /* aoc_reg_float_tos */       't',
  /* aoc_reg_float_second */    'u',
  /* aoc_reg_sse */             'x',
  /* aoc_reg_sse2 */            'Y',
  /* aoc_reg_mmx */             'y',
  /* aoc_imm_short_shift */     'I',
  /* aoc_imm_long_shift */      'J',
  /* aoc_imm_lea_shift */       'M',
  /* aoc_imm_signed8 */         'K',
  /* aoc_imm_unsigned8 */       'N',
  /* aoc_imm_and_zext */        'L',
  /* aoc_imm_80387 */           'G',
  /* aoc_imm_sse */             'H',
  /* aoc_imm_sext32 */          'e',
  /* aoc_imm_zext32 */          'Z',
  /* aoc_cc */                  '@',  /* Must be followed by "cc" and the
                                         contents of the cond_code string. */
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
  /* aoc_last */                '~'
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(asm_operand_constraint_letters)

/* An operand constraint for the GNU extended assembly syntax. */
typedef struct an_asm_operand_constraint *an_asm_operand_constraint_ptr;
typedef struct an_asm_operand_constraint {
  an_asm_operand_constraint_kind
  		kind;	/* The kind of constraint associated with this
			   operand. */
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
  a_const_char	*cond_code;
			/* When kind == aoc_cc, points to a string with the
			   specific condition code (e.g., "nz" for "=@ccnz").*/
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
  an_asm_operand_constraint_ptr
  		next;	/* The next constraint that applies to this
			   operand, or NULL if this is the last
			   constraint. */
} an_asm_operand_constraint;

/*
Modifiers to asm operand strings.  These are all machine independent.
Note that these are bitmasks, and that aom_input + aom_output == aom_modify.
Note also that some "modifiers" are treated internally as "constraints"
(see an_asm_operand_constraint_kind).  Specifically, those modifiers
that can appear multiple times in a single constraint string, e.g., for
multiple alternative constraints, are treated as constraints.
*/
enum an_asm_operand_modifier : a_byte {
  aom_invalid           = 0x00, /* error */
  aom_input             = 0x01, /* no mod: input operand */
  aom_output            = 0x02, /* =: output operand */
  aom_modify            = 0x03  /* +: read-mod-write operand */
};

#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */

/*
Enumeration of registers and their names (all machine-specific).

If you add new named registers, be sure to update named_register_names.
*/
enum a_named_register : a_byte {
  anr_invalid = 0,
  anr_memory,                         /* memory */
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
  anr_a,   anr_b,   anr_c,   anr_d,   /* eax, ebx, ecx, edx */
  anr_si,  anr_di,  anr_bp,  anr_sp,  /* esi, edi, ebp, esp */
  anr_r8,  anr_r9,  anr_r10, anr_r11, /* x86-64 extra integer registers */
  anr_r12, anr_r13, anr_r14, anr_r15,
  anr_st,  anr_st1, anr_st2, anr_st3, /* 80387 floating point stack */
  anr_st4, anr_st5, anr_st6, anr_st7,
  anr_mm0, anr_mm1, anr_mm2, anr_mm3, /* MMX registers */
  anr_mm4, anr_mm5, anr_mm6, anr_mm7,
  anr_f0,  anr_f1,  anr_f2,  anr_f3,  /* SSE/SSE2 registers */
  anr_f4,  anr_f5,  anr_f6,  anr_f7,
  anr_f8,  anr_f9,  anr_f10, anr_f11, /* x86-64 extra SSE registers */
  anr_f12, anr_f13, anr_f14, anr_f15,
  anr_flags, anr_fpsr, anr_dirflag,   /* control registers */
  anr_16, anr_17, anr_18, anr_19,
  anr_20,                             /* used to represent numeric register
                                         names that do not map on actual
                                         registers. */
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
#if ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
  anr_unrecognized,		      /* used to represent an unrecognized
                                         register. */
#endif /* ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
  anr_last
};


/*
Names of named registers.  Note that the user is allowed to
give additional variants, see extasm.c.
*/
EXTERN_CONSTINIT_ARRAY(a_const_char*, named_register_names, anr_last + 1)
#if VAR_INITIALIZERS
= {
  /* anr_invalid */ "invalid",
  /* anr_memory */  "memory",
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
  /* anr_a */       "ax",
  /* anr_b */       "bx",
  /* anr_c */       "cx",
  /* anr_d */       "dx",
  /* anr_si */      "si",
  /* anr_di */      "di",
  /* anr_bp */      "bp",
  /* anr_sp */      "sp",
  /* anr_r8 */      "r8",
  /* anr_r9 */      "r9",
  /* anr_r10 */     "r10",
  /* anr_r11 */     "r11",
  /* anr_r12 */     "r12",
  /* anr_r13 */     "r13",
  /* anr_r14 */     "r14",
  /* anr_r15 */     "r15",
  /* anr_st */      "st",
  /* anr_st1 */     "st(1)",
  /* anr_st2 */     "st(2)",
  /* anr_st3 */     "st(3)",
  /* anr_st4 */     "st(4)",
  /* anr_st5 */     "st(5)",
  /* anr_st6 */     "st(6)",
  /* anr_st7 */     "st(7)",
  /* anr_mm0 */     "mm0",
  /* anr_mm1 */     "mm1",
  /* anr_mm2 */     "mm2",
  /* anr_mm3 */     "mm3",
  /* anr_mm4 */     "mm4",
  /* anr_mm5 */     "mm5",
  /* anr_mm6 */     "mm6",
  /* anr_mm7 */     "mm7",
  /* anr_f0 */      "xmm0",
  /* anr_f1 */      "xmm1",
  /* anr_f2 */      "xmm2",
  /* anr_f3 */      "xmm3",
  /* anr_f4 */      "xmm4",
  /* anr_f5 */      "xmm5",
  /* anr_f6 */      "xmm6",
  /* anr_f7 */      "xmm7",
  /* anr_f8 */      "xmm8",
  /* anr_f9 */      "xmm9",
  /* anr_f10 */     "xmm10",
  /* anr_f11 */     "xmm11",
  /* anr_f12 */     "xmm12",
  /* anr_f13 */     "xmm13",
  /* anr_f14 */     "xmm14",
  /* anr_f15 */     "xmm15",
  /* anr_flags */   "flags",
  /* anr_fpsr */    "fpsr",
  /* anr_dirflag */ "dirflag",
  /* anr_16 */      "16",
  /* anr_17 */      "17",
  /* anr_18 */      "18",
  /* anr_19 */      "19",
  /* anr_20 */      "20",
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
#if ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
  /* anr_unrecognized */
                     "unrecognized",
#endif /* ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
  /* anr_last */    "last"
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(named_register_names)

typedef struct an_asm_operand *an_asm_operand_ptr;
typedef struct an_asm_operand {
  an_asm_operand_ptr
                next;   /* Next entry on the list, or NULL if last. */
  char
		*name;	/* The symbolic name indicated for this operand (using
			   the "[ <identifier> ]" syntax), or NULL if none was
			   given. */
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  a_byte_boolean
		is_output_operand;
			/* TRUE if this entry is for an output operand
			   description.  Otherwise, this represents an input
			   operand description. */
  a_const_char	*constraints_string;
			/* The constraint string as it appeared in the
			   source. */
#else /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  an_asm_operand_constraint_ptr
                constraints;     
                        /* Constraints on where the operand may appear
                           in order to make it a valid assembly instruction.
                           Note that "multiple alternative constraints" are
                           supported, that is, a single constraint string can
                           represent multiple constraints each separated by
                           a comma in the constraint string.  These constraints
                           are separated by aoc_end_of_constraint entries
                           in the constraints list.  The number of constraints
                           is given by number_of_constraints in the
                           an_asm_entry that points to this. */
  an_asm_operand_modifier
                modifiers;      
                        /* Modifiers to the constraint. */
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  a_source_position
                position;       
                        /* Source position of this operand. */
  an_expr_node_ptr
                expression;     
			/* The expression constituting the operand. */
} an_asm_operand;

typedef struct a_named_register_list *a_named_register_list_ptr;
typedef struct a_named_register_list {
  a_named_register_list_ptr
                next;   /* Next entry on the list, or NULL if last. */
  a_named_register
                reg;    /* The register itself. */
} a_named_register_list;

typedef struct a_label_list *a_label_list_ptr;
typedef struct a_label_list {
  a_label_list_ptr
                next;   /* Next entry on the list, or NULL if last. */
  a_label_ptr   label;  /* The label itself. */
} a_label_list;
#endif /* GNU_EXTENSIONS_ALLOWED */

/*
A bit set whose values represent the presence of one or more type qualifiers
(const, volatile, along with others that an implementation might choose to
support, such as restrict).
*/
typedef unsigned int a_type_qualifier_set;

/*
Enumeration of type qualifiers that are accepted.  The enumeration values
are used to create bit masks that are used to represent the qualifiers.
*/
enum a_type_qualifier {
  tqt_const,		/* Const qualifier. */
  tqt_volatile,		/* Volatile qualifier. */
  tqt_restrict,		/* Restrict qualifier. */
  tqt_c11_atomic,	/* C11 _Atomic qualifier. */
  tqt_nullable,		/* Clang _Nullable qualifier. */
  tqt_nonnull,		/* Clang _Nonnull qualifier. */
  tqt_null_unspecified,	/* Clang _Null_unspecified qualifier. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  tqt_unaligned,	/* Microsoft __unaligned qualifier. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  tqt_near,		/* near */
  tqt_far,		/* far */
#endif /* NEAR_AND_FAR_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  tqt_upc_shared,	/* UPC shared */
  tqt_upc_strict,	/* UPC strict */
  tqt_upc_relaxed,	/* UPC relaxed */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
  tqt_lsb_named_address_space,
			/* Least significant bit of named address space
			   representation. */
  tqt_msb_named_address_space =
	(int)tqt_lsb_named_address_space + NUM_BITS_FOR_NAMED_ADDRESS_SPACE -1,
			/* Most significant bit of named address space
			   representation. */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
  tqt_last		/* Must be last. */
};

/*
Definitions of the bits in bit sets of type a_type_qualifier_set.
*/
#define TQ_NONE		((a_type_qualifier_set)0x0)
			/* No type qualifiers. */
#define TQ_CONST	((a_type_qualifier_set)(1 << (int)tqt_const))
			/* This bit is set to represent const. */
#define TQ_VOLATILE	((a_type_qualifier_set)(1 << (int)tqt_volatile))
			/* This bit is set to represent volatile. */
#define TQ_RESTRICT	((a_type_qualifier_set)(1 << (int)tqt_restrict))
			/* This bit is set to represent restrict. */
#define TQ_C11_ATOMIC	((a_type_qualifier_set)(1 << (int)tqt_c11_atomic))
			/* This bit is set to represent _Atomic. */
#define TQ_NULLABLE	((a_type_qualifier_set)(1 << (int)tqt_nullable))
			/* This bit is set to represent _Nullable. */
#define TQ_NONNULL	((a_type_qualifier_set)(1 << (int)tqt_nonnull))
			/* This bit is set to represent _Nonnull. */
#define TQ_NULL_UNSPECIFIED                                                  \
			((a_type_qualifier_set)                              \
			                  (1 << (int)tqt_null_unspecified))
			/* This bit is set to represent _Null_unspecified. */
#define TQ_NULLABILITY	(TQ_NULLABLE | TQ_NONNULL | TQ_NULL_UNSPECIFIED)
			/* Convenience constant to mask nullability bits. */
#if MICROSOFT_EXTENSIONS_ALLOWED
#define TQ_UNALIGNED	((a_type_qualifier_set)(1 << (int)tqt_unaligned))
			/* This bit is set to represent __unaligned. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
#define TQ_NEAR		((a_type_qualifier_set)(1 << (int)tqt_near))
			/* This bit is set to represent near. */
#define TQ_FAR		((a_type_qualifier_set)(1 << (int)tqt_far))
			/* This bit is set to represent far. */
#endif /* NEAR_AND_FAR_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
#define TQ_UPC_SHARED	((a_type_qualifier_set)(1 << (int)tqt_upc_shared))
			/* This bit is set to represent UPC shared. */
#define TQ_UPC_STRICT	((a_type_qualifier_set)(1 << (int)tqt_upc_strict))
			/* This bit is set to represent UPC strict. */
#define TQ_UPC_RELAXED	((a_type_qualifier_set)(1 << (int)tqt_upc_relaxed))
			/* This bit is set to represent UPC relaxed. */
#endif /* UPC_EXTENSIONS_ALLOWED */

#if NAMED_ADDRESS_SPACES_ALLOWED
/*
Macros to set and retrieve the named address space id in a type qualifier
bit set.
*/
/*lint -emacro(572,named_address_space_from_qualifier_set)*/
#define named_address_space_from_qualifier_set(tqs)                          \
  (((tqs) >> (int)tqt_lsb_named_address_space) &                             \
   (((a_type_qualifier_set)1 << NUM_BITS_FOR_NAMED_ADDRESS_SPACE) - 1))

#define set_named_address_space_in_qualifier_set(tqs, nas_id)                \
  ((tqs) |= ((a_type_qualifier_set)(nas_id) <<                               \
                                          (int)tqt_lsb_named_address_space))

#endif /* NAMED_ADDRESS_SPACES_ALLOWED */

/*
The last type qualifier tag value is used as the number of bits required
to represent a type qualifier set.
*/
#define NUM_BITS_FOR_TYPE_QUALIFIER_SET ((int)tqt_last)

/*
Copy the qualifiers in the from parameter to the qualifiers stored in the to
parameter.

This macro is used to avoid compiler warnings when copying qualifiers into a
bitfield (with a bitwidth of NUM_BITS_FOR_TYPE_QUALIFIER_SET).
*/
#define copy_qualifiers(from, to)                                            \
    copy_to_bitfield(from, to, NUM_BITS_FOR_TYPE_QUALIFIER_SET)

#if UPC_EXTENSIONS_ALLOWED

/*
For a "#pragma UPC ...", indicates the specific kind of UPC predefined
pragma that is being used.
*/
enum a_upc_pragma_kind : a_byte {
  upc_pk_access,
  upc_pk_coherence
};


/* Tag values indicating the specific UPC access setting. */
enum a_upc_access_method : a_byte {
  upc_access_unspecified,
  upc_access_strict,
  upc_access_relaxed
};

/*
Define a macro for the number of bits for upc access.
*/
#define NUM_BITS_FOR_UPC_ACCESS 2

/* Tag values indicating UPC coherence stack operations. */
enum a_upc_coherence_stack_operation : a_byte {
  upc_coherence_stack_noop,
  upc_coherence_stack_save,
  upc_coherence_stack_restore
};

#endif /* UPC_EXTENSIONS_ALLOWED */

/* Entry used on parameter type lists for functions. */
typedef struct a_param_type *a_param_type_ptr;
typedef struct a_param_type {
  a_param_type_ptr
                next;
                        /* Pointer to the next parameter type, or NULL if
                           this is the last one. */
  a_type_ptr    type;
                        /* Type of the parameter.  In C++, any top-level
			   type qualifiers that were present in the source
			   have been removed -- see the field "qualifiers"
			   below. */
  a_type_ptr    declared_type;
			/* The type before any transformations (like
			   array-to-pointer decay) were applied.  (NULL for
			   compiler-generated parameters.) */
  a_const_char  *name;
			/* Pointer to null-terminated name, or NULL if none
			   was declared. */
  a_bit_field	has_name_conflict:1;
			/* TRUE if the name was inconsistent between
			   declarations.  Note that an unnamed parameter is
			   not inconsistent with a named one. */
  a_bit_field	passed_via_copy_constructor:1;
			/* If TRUE, the parameter has a type that requires
			   a copy constructor to be called.  For a parameter
			   of type T, the actual argument will be the address
			   of a temporary of type T, into which the argument
			   value has been copied.  Also set for parameter
			   types that allow by-value copy construction if
			   the type has a destructor. */
  a_bit_field	has_default_arg:1;
             		/* TRUE if a default argument has been declared for
			   this parameter.  Because of delayed token scanning
			   of default arguments for member functions, this
			   flag may be set even though default_arg_expr
                           remains NULL. */
  a_bit_field	default_arg_appeared_in_class_definition:1;
			/* TRUE if has_default_arg is TRUE, and the default
			   argument appeared on the in-class declaration of a
			   member function. */
  a_bit_field	has_unevaluated_template_default:1;
			/* Default arguments of template functions and
			   member functions of class templates are evaluated
			   (and semantically checked) only if the default
		           value is needed.  This flag is TRUE if the default
			   value is present, but has not yet been evaluated. */
  a_bit_field	default_being_instantiated:1;
			/* TRUE if the default argument is in the process of
			   being instantiated. */
  a_bit_field	type_involves_deduced_template_param:1;
			/* TRUE if the type entry associated with the
			   parameter involves a template parameter in a
			   context in which a template argument value can
			   be deduced. */
  a_bit_field	type_involves_template_param:1;
			/* TRUE if the type entry associated with the
			   parameter involves a template parameter in any
			   context. */
  a_bit_field	is_parameter_pack:1;
			/* TRUE if this entry represents a C++11 function
			   parameter pack of a variadic template.  This is
			   set for the prototype instantiation of variadic
			   templates.  It is also set in a case like the
			   following:
			     template<class ... Ts> struct S {
			       template<class F> auto m(F f, Ts... p)
			                                 ->decltype(f(p...));
			     };

			   When the outer template (S) is instantiated, the
			   first parameter pack entry for p (if any) is marked
			   with this flag set to TRUE, but also with the flag
			   is_pack_element set to TRUE.  That allows "p..." in
			   the return type to be recognized as a valid pack
			   expansion. */
  a_bit_field	is_pack_element:1;
			/* TRUE for parameters of an actual instantiation of
			   a variadic template for those parameters that are
			   associated with a parameter pack of the original
			   variadic template. */
  a_bit_field	was_nontrailing_pack:1;
			/* TRUE if this is a pack element produced from a
			   nontrailing parameter pack (which can happen with
			   explicit template arguments). */
  a_bit_field	is_auto_param:1;
			/* TRUE if the parameter is declared with an "auto"
			   type specifier. */
  a_bit_field	qualifiers:NUM_BITS_FOR_TYPE_QUALIFIER_SET;
			/* Top-level type qualifiers that have been removed
			   from the parameter type; always TQ_NONE except in
			   C++ mode when remove_qualifiers_from_param_types
			   is TRUE.  If the routine type to which this
			   param-type entry belongs is associated with a
			   defined function, then this field reflects how
			   the function was defined. */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	is_transparent:1;
			/* For a parameter of union type, TRUE if the
			   union is transparent. */
  a_bit_field	nonnull:1;
			/* TRUE if this represents a parameter of pointer type
			   that must be passed a non-NULL argument. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_bit_field	duplicate_name:1;
			/* TRUE if the name of this parameter is the same as
			   that of an earlier parameter, which is allowed in
			   some GNU modes and for variadic parameters. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_cli_param_array:1;
			/* TRUE if this parameter is a C++/CLI "parameter
			   array" (declared with a leading ellipsis). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	move_ctor_or_assign_parameter:1;
			/* TRUE if this is the first parameter of a move
			   constructor or a move assignment operator. */
  a_bit_field	copy_or_move_ctor_parameter:1;
			/* TRUE if this is the first parameter of a copy or
			   move constructor. */
  a_bit_field	is_requires_expr_param:1;
			/* TRUE if this is a parameter for a requires-
			   expression. */
  a_bit_field   is_explicit_this:1;
			/* TRUE if this is an explicit object parameter
			   for a member function. */
  uint32_t	param_num;
			/* The ordinal position of the parameter (1, 2, ...).
			   In the instantiation of a variadic template, this
			   is the position of the corresponding parameter from
			   the original template.  In other words, there can
			   be missing or repeated values in the parameter list
			   of the instantiation of a variadic template.
			   Additional parameters generated by lowering (e.g.,
			   for implementing the "this" pointer) are given a
			   zero position. */
  an_expr_node_ptr
		default_arg_expr;
			/* Expression node representing the default value
			   to be used as the actual argument on a function
			   call when the actual argument corresponding to
			   this parameter is omitted (C++ only).  This can
			   be NULL if the default argument value has not
			   yet been evaluated, or for a template default
			   argument value whose value was never needed. */
  a_param_type_ptr
		orig_param_type_for_unevaluated_default_arg_expr;
			/* For a parameter type entry that has or had
			   has_unevaluated_template_default TRUE, this points
			   to the param type entry that originally had the
			   fixup entry attached to it, which might be the
			   current entry.  Front end only. */
  an_il_entity_list_entry_ptr
		entities_defined_in_default_arg;
			/* A list of entities defined in the default argument
			   associated with this parameter.  Currently, this
			   list only has C++11 closure types. */
  an_attribute_ptr
		attributes;
			/* The set of attributes applicable to this
			   parameter. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_ms_attribute_ptr
		ms_attributes;
			/* Linked list of Microsoft attribute entries that
			   apply to this parameter. */ 
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_decl_position_supplement_ptr
		decl_pos_info;
			/* Points to a block containing additional source
			   position information about the parameter
			   declaration.  May be NULL. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  struct a_pack_expansion_descr
		*pack_expansion_descr;
			/* If non-NULL, this parameter is a pack expansion
			   (it contains a "..." next to the parameter name
			   in the declarator), and this points to the
			   expansion description. */
} a_param_type;


/*
For a "#pragma STDC ...", indicates the specific kind of C99 predefined
pragma that is being used.
*/
enum a_stdc_pragma_kind : a_byte {
  stdc_pk_none,
  stdc_pk_fp_contract,
  stdc_pk_fenv_access,
  stdc_pk_cx_limited_range,
#if FIXED_POINT_ALLOWED
  stdc_pk_fx_full_precision,
  stdc_pk_fx_fract_overflow,
  stdc_pk_fx_accum_overflow,
#endif /* FIXED_POINT_ALLOWED */
  stdc_pk_last
};


/* Number of bits required to hold a STDC pragma value. */
#if FIXED_POINT_ALLOWED
#define NUM_BITS_FOR_STDC_PRAGMA_VALUE 3
#else /* !FIXED_POINT_ALLOWED */
#define NUM_BITS_FOR_STDC_PRAGMA_VALUE 2
#endif /* FIXED_POINT_ALLOWED */

/*
For a "#pragma STDC ...", indicates the value specified by the pragma.
*/
enum a_stdc_pragma_value : a_byte {
  stdc_pv_none,
  stdc_pv_off,
  stdc_pv_on,
#if FIXED_POINT_ALLOWED
  stdc_pv_sat,		/* Represents the "SAT" option for the "Embedded C"
			   FX_FRACT_OVERFLOW and FX_ACCUM_OVERFLOW variants
			   of "#pragma STDC ...". */
#endif /* FIXED_POINT_ALLOWED */
  stdc_pv_default
};


#if GNU_EXTENSIONS_ALLOWED
/*
For a "#pragma GCC ...", indicates the specific kind of GCC predefined pragma
that is being used.
*/
enum a_gcc_pragma_kind : a_byte {
  gcc_pk_none,			/* Used for unrecognized GCC pragmas. */
  gcc_pk_system_header,		/* #pragma GCC system_header */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  gcc_pk_visibility_push,	/* #pragma GCC push(...) */
  gcc_pk_visibility_pop,	/* #pragma GCC pop */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  gcc_pk_target,                /* #pragma GCC target(...) */
  gcc_pk_push_options,          /* #pragma GCC push_options */
  gcc_pk_pop_options,           /* #pragma GCC pop_options */
  gcc_pk_reset_options,         /* #pragma GCC reset_options */
  gcc_pk_last
};


/*
Structure describing a "#pragma GCC ..." construct.
*/
typedef struct a_gcc_pragma_descr {
  a_gcc_pragma_kind
		kind;	/* For the GNU GCC predefined pragmas, indicates
			   the specific pragma being used. */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  union {
    /* When kind == gcc_pk_visibility_pop, no variant fields. */
    /* When kind == gcc_pk_visibility_push. */
    an_ELF_visibility_kind
		visibility;
			/* The visibility specified by
			     #pragma GCC visibility push(...)
			*/
  } variant;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
} a_gcc_pragma_descr;

#endif /* GNU_EXTENSIONS_ALLOWED */

/* The pragma kinds representing the specific pragmas that are recognized
   by the implementation.  Some may refer to pragmas for which entries of
   type a_pragma are added to the IL for processing by the back end, but
   some may be for front-end processing only. */
enum a_pragma_kind : a_byte {
  pk_none,
  pk_printf_args,	/* Next function declaration has a printf-style format
			   string that should be checked against the arguments
			   in the call; front-end only. */
  pk_scanf_args,	/* Next function declaration has a scanf-style format
			   string that should be checked against the arguments
			   in the call; front-end only. */
  pk_lint_argsused,	/* Lint "argsused" comment; not strictly a pragma but
			   processed similarly; front-end only. */
  pk_lint_varargs_count,/* Lint "varargs" comment; not strictly a pragma but
			   processed similarly; front-end only. */
  pk_lint_notreached,	/* Lint "not reached" comment; not strictly a pragma
			   but processed similarly; front-end only. */
  pk_instantiate,	/* Instantiation of the specified template entity
			   is required; front-end only. */
  pk_do_not_instantiate,/* Instantiation of the specified template entity
			   should not be done in the current translation
			   unit; front-end only. */
  pk_can_instantiate,	/* Instantiation of the specified template entity
			   may be done in the current translation unit if
			   needed; front-end only. */
  pk_inline_template,	/* An explicit instantiation directive prefixed by
			   the inline keyword.  Used in g++ mode to cause the
			   vtable to be emitted.  Used in the front end only.
			   There is not actually an inline template pragma.
			   This is used because the instantiation_directive
			   required a pragma kind to indicate the action to
			   be performed. */
  pk_pack,		/* Establishes maximum alignment of nonstatic data
			   members of subsequent classes, structs, and
			   unions. */
#if IDENT_DIRECTIVE_AND_PRAGMA
  pk_ident_pragma,	/* Used for #pragma ident; all tokens are collected
			   in a single string which is passed to the back end
			   (in pragma_text). */
  pk_ident_directive,	/* Used for #ident; specifies a single string which
			   is passed on to the back end (in ident_string). */
  pk_ident = pk_ident_directive,
			/* For compatibility with older versions. */
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if PRAGMA_WEAK_ALLOWED
  pk_weak,		/* Specifies "weak binding" for C_mode() name.  The
			   name is passed on to the back end. */
#endif /* PRAGMA_WEAK_ALLOWED */
  pk_once,              /* Indicates that a header file should only be
			   included once, even if referenced more than once. */
  pk_hdrstop,           /* End of sequence of includes to be represented as
			   a precompiled header.  Ignored except during
			   PCH prefix scanning. */
  pk_no_pch,            /* Suppresses generation of PCH file.
			   Ignored except during PCH prefix scanning. */
  pk_define_type_info,  /* The following class definition provides the
			   definition of the type_info type returned
			   by typeid. */
  pk_stdc,		/* Used for the C99 and C++11 predefined pragmas (i.e.,
			   FP_CONTRACT, FENV_ACCESS, and CX_LIMITED_RANGE).
			   Also used for the "Embedded C" (TR 18037) fixed-
			   point pragmas. */
#if UPC_EXTENSIONS_ALLOWED
  pk_upc,               /* UPC-specific pragma, controlling the default
                           access method for shared data. */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
  pk_redefine_extname,	/* Solaris-specific pragma that allows external
			   (mangled) names to be remapped. */
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if SUN_EXTENSIONS_ALLOWED
  pk_enable_ldscope,
  pk_disable_ldscope,
#endif /* SUN_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  pk_gcc_immediate,     /* GCC pragmas (handled immediately). */
  pk_gcc_next_token,    /* GCC pragmas (handled as next token). */
  pk_gcc_next_statement,/* GCC pragmas (bound to the next statement). */
#if GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED
  pk_gnu_riscv,         /* GCC RISC-V intrinsics. */
  pk_clang_riscv,       /* Clang RISC-V intrinsics. */
#endif /* GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED */
#endif /* GNU_EXTENSIONS_ALLOWED */
  pk_diag_suppress,
  pk_diag_remark,
  pk_diag_warning,
  pk_diag_error,
  pk_diag_once,
  pk_diag_default,	/* Pragmas to control the issuing of diagnostics. */
  pk_diagnostic,	/* Pragma to push/pop diagnostic state. */
#if INCLUDE_EDG_TEST_PRAGMAS
  /* For testing purposes. */
  pk_test_next_statement,
  pk_test_next_decl,
  pk_test_immediate,
  pk_test_immediate_text,
  pk_test_immediate_pp_text,
  pk_test_other,
  pk_test_bind_next_pass,
#endif /* INCLUDE_EDG_TEST_PRAGMAS */
#if ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING
  /* Used for internal testing. */
  pk_checking_pragma,
#endif /* ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING */
#if DEBUG
  pk_db_opt,		/* Used to specify a debugging option string. */
  pk_db_name,		/* Used to specify a debug entity name. */
#endif /* DEBUG */
#if NEED_IL_DISPLAY
  pk_il_display,        /* To display the IL of an IL entity. */
#endif /* NEED_IL_DISPLAY */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  pk_if_exists,		/* Used in the implementation of the Microsoft
			   __if_exists feature. */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  pk_push_macro,
  pk_pop_macro,
#if MICROSOFT_EXTENSIONS_ALLOWED
  pk_start_map_region,
  pk_stop_map_region,
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
  pk_setlocale,
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
  pk_comment,
  pk_conform,
  pk_include_alias,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
  pk_unrecognized,	/* This pragma kind is used for pragmas that are
			   not recognized by the front end but are to be
			   recorded as a character string and passed to
			   the back end.  These will typically be emitted
			   by the C or C++ generating back end.  See the
			   comments in pragma_init for information about
			   the type of pragma that is created for an
			   unrecognized pragma. */
#endif /* INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL */

  /* The preceding pragma kinds are required for the default
     implementation of the EDG front end.  If additional pragma kinds
     are supplied for a given implementation, be sure to update
     pragma_ids, a_pragma (if variant fields are required), and
     alloc_pragma (which initializes the variant part of a_pragma),
     and add an entry to the pragma_kind_descriptions list using one
     of the add_..._pragma_kind_description routines. */

  pk_last		/* Must be last. */
};


EXTERN_CONSTINIT_ARRAY(a_const_char*, pragma_ids, pk_last + 1)
#if VAR_INITIALIZERS
= {
/* pk_none */			"none",
/* pk_printf_args */		"__printf_args",
/* pk_scanf_args */		"__scanf_args",
/* pk_lint_argsused */		"ARGSUSED",
/* pk_lint_varargs_count */     "VARARGS",
/* pk_lint_notreached */	"NOTREACHED",
/* pk_instantiate */		"instantiate",
/* pk_do_not_instantiate */	"do_not_instantiate",
/* pk_can_instantiate */	"can_instantiate",
/* pk_inline_template */	"inline_template",
/* pk_pack */			"pack",
#if IDENT_DIRECTIVE_AND_PRAGMA
/* pk_ident_pragma */		"ident",  /* Used for #pragma ident. */
/* pk_ident_directive */	"",       /* Used for #ident. */
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if PRAGMA_WEAK_ALLOWED
/* pk_weak */			"weak",
#endif /* PRAGMA_WEAK_ALLOWED */
/* pk_once */                   "once",
/* pk_hdrstop */                "hdrstop",
/* pk_no_pch */                 "no_pch",
/* pk_define_type_info */       "define_type_info",
/* pk_stdc */                   "STDC",
#if UPC_EXTENSIONS_ALLOWED
/* pk_upc */                    "upc",
#endif /* UPC_EXTENSIONS_ALLOWED */
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
/* pk_redefine_extname */       "redefine_extname",
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if SUN_EXTENSIONS_ALLOWED
/* pk_enable_ldscope */         "enable_ldscope",
/* pk_disable_ldscope */        "disable_ldscope",
#endif /* SUN_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
/* pk_gcc_immediate */		"GCC",
/* pk_gcc_next_token */		"GCC",
/* pk_gcc_next_statement */	"GCC",
#if GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED
/* pk_gnu_riscv */		"riscv",
/* pk_clang_riscv */		"clang",
#endif /* GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED */
#endif /* GNU_EXTENSIONS_ALLOWED */
/* pk_diag_suppress */		"diag_suppress",
/* pk_diag_remark */		"diag_remark",
/* pk_diag_warning */		"diag_warning",
/* pk_diag_error */		"diag_error",
/* pk_diag_once */		"diag_once",
/* pk_diag_default */		"diag_default",
/* pk_diagnostic */		"diagnostic",
#if INCLUDE_EDG_TEST_PRAGMAS
/* For testing purposes. */
/* pk_test_next_statement */	"test_next_statement",
/* pk_test_next_decl */		"test_next_decl",
/* pk_test_immediate */		"test_immediate",
/* pk_test_immediate_text */	"test_immediate_text",
/* pk_test_immediate_pp_text */	"test_immediate_pp_text",
/* pk_test_other */		"test_other",
/* pk_test_bind_next_pass */	"test_bind_next_pass",
#endif /* INCLUDE_EDG_TEST_PRAGMAS */
#if ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING
/* pk_checking_pragma */        "checking_pragma",
#endif /* ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING */
#if DEBUG
/* pk_db_opt */			"db_opt",
/* pk_db_name */		"db_name",
#endif /* DEBUG */
#if NEED_IL_DISPLAY
/* pk_il_display */		"il_display",
#endif /* NEED_IL_DISPLAY */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
/* pk_if_exists */		"__if_exists",
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
/* pk_push_macro */		"push_macro",
/* pk_pop_macro */		"pop_macro",
#if MICROSOFT_EXTENSIONS_ALLOWED
/* pk_start_map_region */	"start_map_region",
/* pk_stop_map_region */	"stop_map_region",
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
/* pk_setlocale */		"setlocale",
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
/* pk_comment */		"comment",
/* pk_conform */		"conform",
/* pk_include_alias */		"include_alias",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
/* pk_unrecognized */		"unrecognized",
#endif /* INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL */
/* pk_last */			"last"
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(pragma_ids)


#if MICROSOFT_EXTENSIONS_ALLOWED
enum a_microsoft_pragma_comment_type : a_byte {
  /* Code for comment types in a Microsoft #pragma comment. */
  mpct_compiler,
  mpct_exestr,
  mpct_lib,
  mpct_linker,
  mpct_user,
  /* Must be last: */
  mpct_last
};


enum a_microsoft_pragma_conform_kind : a_byte {
  /* Code for conformance switch in a Microsoft "#pragma conform(...)".
     Currently only "forScope" is a valid switch. */
  mpck_forScope
};


EXTERN_CONSTINIT_ARRAY(a_const_char*, microsoft_pragma_comment_ids,
                       mpct_last + 1)
#if VAR_INITIALIZERS
= {
/* mpct_compiler */	"compiler",
/* mpct_exestr */	"exestr",
/* mpct_lib */		"lib",
/* mpct_linker */	"linker",
/* mpct_user */		"user",
/* mpct_last */		"last"
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(microsoft_pragma_comment_ids)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


/* A pragma entry represents a pragma declaration that either has general
   effect (over an entire translation unit or over the current scope) or is
   bound to one or more entities (declarations or statements) in the current
   scope.  The entities are in the IL because they represent state that is
   passed to the back-end. */
typedef struct a_pragma *a_pragma_ptr;
typedef struct a_pragma {
  a_pragma_ptr	next;
			/* Next in a linked list of pragma entries
			   declared in the current scope. */
  a_pragma_kind	kind;
			/* The kind of pragma. */
  a_byte_boolean
		ignore_in_back_end;
			/* TRUE if this pragma may be ignored by the back
			   end if it is not recognized; the flag will be set
			   (for example) on pragmas that are in the IL but
			   are for use by other (earlier) phases of the
			   compilation. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_byte_boolean
		is_microsoft_pragma_operator;
			/* TRUE if the pragma was specified using a Microsoft
			   __pragma operator. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_tagged_pointer
		entity;
			/* A struct containing a tag and a generic pointer to
			   the entity (statement, variable, function, etc.) to
			   which this pragma is bound; a given pragma entry
			   is bound to only one such entity.  If the entity's
			   ptr field is NULL, this pragma has general effect,
			   either globally (if it is on the pragma list for
			   the file scope) or locally (if it is on the pragma
			   list for a nonfile scope). */
  a_source_position
		position;
			/* Source position of the pragma in the declaration
			   of this pragma.  Points to the beginning of the
			   #pragma directive. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		source_sequence_entry;
			/* Pointer to source sequence entry that represents
			   the place this pragma appears within the current
			   file or function scope relative to other
			   declarations, statements, comments, etc. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_const_char	*pragma_text;
			/* For pragmas that are passed through to the
			   back end as an uninterpreted character string,
			   this points to the null terminated string.  The
			   string begins with the token immediately following
			   the #pragma keyword. */
  union {
    /* When kind == pk_none or refers to a "front-end-only" pragma, no variant
       fields. */
#if IDENT_DIRECTIVE_AND_PRAGMA
    /* When kind == pk_ident_pragma there is no variant and the string
       (including the "ident " prefix) is in pragma_text. */
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
    /* When kind == pk_stdc: */
    struct {
      a_stdc_pragma_kind
		kind;	/* For the C99 STDC predefined pragmas, indicates
			   the specific pragma being used. */
      a_stdc_pragma_value
		value;	/* Specifies whether the attribute is being turned
			   on, off, or reset to the default value. */
    } stdc;
#if UPC_EXTENSIONS_ALLOWED
    /* When kind == pk_upc: */
    struct {
      a_upc_pragma_kind
		kind;	/* For the UPC predefined pragmas, indicates
			   the specific pragma being used. */
      union {
        a_upc_access_method
		access_method;
			/* Indicate which UPC access method ("strict" or
			   "relaxed") was specified. */
        a_upc_coherence_stack_operation
		operation;
			/* Indicate which UPC coherence stack operation
			   ("save" or "restore") was specified. */
      } value;
    } upc;
#endif /* UPC_EXTENSIONS_ALLOWED */
#if IDENT_DIRECTIVE_AND_PRAGMA
    /* When kind == pk_ident_directive: */
    a_constant_ptr
		ident_string;
			/* The string for the #ident. */
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if BACK_END_IS_CP_GEN_BE
    /* When kind == pk_pack: */
    a_targ_alignment
		alignment;
			/* The alignment specified by the pack pragma. */
#endif /* BACK_END_IS_CP_GEN_BE */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* When kind == pk_comment: */
    struct {
      a_microsoft_pragma_comment_type
		kind;	/* The comment type. */
      a_constant_ptr
		str;	/* The comment string, NULL if none was supplied. */
    } comment;
    /* When kind == pk_conform: */
    struct {
      a_microsoft_pragma_conform_kind
		kind;	/* The conformance switch specified (currently, only
			   "forScope" is available). */
      a_bit_field
		on:1;	/* TRUE if the pragma explicitly enables standard
			   for-init behavior. */
      a_bit_field
		off:1;	/* TRUE if the pragma explicitly disables standard
			   for-init behavior. */
      a_bit_field
		show:1;	/* TRUE if this is a conform "show" pragma. */
      a_bit_field
		push:1;	/* TRUE if this is a conform "push" pragma. */
      a_bit_field
		pop:1;	/* TRUE if this is a conform "pop" pragma. */
      a_const_char
		*identifier;
			/* If an identifier was specified in the conform
			   pragma this points to a null-terminated string
			   representing that identifier.  NULL otherwise. */
    } conform;
    /* When kind == pk_include_alias: */
    struct {
      a_const_char
		*long_file_name;
			/* The file name that is to be aliased to another
			   name.  This contains the raw characters of the
			   header name token. */
      a_const_char
		*short_file_name;
			/* The file name to be used in place of
			   long_file_name.  This contains the file name
			   after conversions such as possible conversion to
			   UTF-8. */
    } include_alias;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    /* When kind == pk_gcc: */
    a_gcc_pragma_descr
		gcc;
			/* Description of the GCC pragma. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  } variant;
} a_pragma;


/* Type used to hold a lint varargs argument count: */
typedef short a_lint_varargs_count;
#define LINT_VARARGS_COUNT_MAX SHRT_MAX
/* Value used to indicate that there is no lint varargs count: */
#define NOT_LINT_VARARGS (-1)


/* Types an_exception_specification and an_exception_specification_type are
   used in C++ only. */
/* an_exception_specification_type is an entry that represents a type that
   appears on a list of types specified in an exception specification.  For
   instance,
     void f() throw (int,float);
   yields an exception-specification-type entry for int and another for
   float. */
typedef struct an_exception_specification_type
                                        *an_exception_specification_type_ptr;
typedef struct an_exception_specification_type {
  an_exception_specification_type_ptr
		next;
			/* Pointer to the next in the linked list of
			   exception specification type entries, or NULL for
			   the last entry on the list. */
  a_type_ptr	type;
			/* A pointer to the type of the exception. */
  a_byte_boolean
		redundant;
			/* TRUE when a previous entry on the list has the same
			   type. */
  a_byte_boolean
		is_pack_expansion;
			/* TRUE if the type is a variadic template pack
			   expansion, i.e., it's followed by "...". */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		source_position;
			/* Position of the start of this type. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} an_exception_specification_type;


/* an_exception_specification is an entry that describes an exception
   specification on a function declaration. */
typedef struct an_exception_specification *an_exception_specification_ptr;
typedef struct an_exception_specification {
  a_bit_field	is_noexcept:1;
			/* TRUE if the exception specification is a C++11-
			   style noexcept form. */
  a_bit_field	indeterminate:1;
			/* TRUE if the exception specification has not been
			   determined yet.  (Only possible with generated
			   special member functions.) */
  a_bit_field	throw_any:1;
			/* TRUE if "noexcept(<false-constant>)" or the
			   Microsoft extension "throw (...)" was encountered.
			   Also TRUE if a noexcept-specifier has a template-
			   dependent argument.  It indicates that any exception
			   may be thrown. */
  a_bit_field	compiler_generated:1;
			/* TRUE for exception specifications that did not
			   appear in the source code. */
  a_bit_field	from_attribute:1;
			/* TRUE if this entry is the result of an attribute
			   (specifically, "__declspec(nothrow)"). */
  a_bit_field	arg_cached:1;
			/* TRUE while the parenthesized argument tokens of the
			   exception specification are cached for later
			   rescanning.  In the case of members of class
			   templates, this rescanning may never occur if the
			   member is never used. */
  a_bit_field	copy_from_prototype:1;
			/* TRUE for the exception specification of a
			   subordinate member template that still must be
			   copied (with substitutions) from the prototype
			   template. */
  union {
    /* When arg_cached is TRUE. */
    struct a_token_cache
    		*token_cache;
			/* Opaque pointer to a token cache containing the
			   argument tokens of the exception specifier (for
			   later rescanning).  This is for front-end use
			   only. */
    /* When copy_from_prototype is TRUE. */
    a_routine_ptr
		routine;
			/* Pointer to the subordinate prototype instantiation
			   for which the exception specification must be
			   copied. */
    /* When is_noexcept is FALSE (and arg_cached and copy_from_prototype are
       FALSE). */
    an_exception_specification_type_ptr
		exception_specification_type_list;
			/* Pointer to the linked list of exception
			   specification type entries giving the types of
			   exceptions a given function will throw, e.g.,
			     void f() throw (int,char);
                           or NULL if no exceptions will be thrown, e.g.,
			     void f() throw ();              */
    /* When is_noexcept is TRUE (and arg_cached and copy_from_prototype are
       FALSE). */
    a_constant_ptr
		noexcept_arg;
			/* Representation of the constant-expression specified
			   as an argument for the "noexcept" specification, or
			   NULL if no argument was specified.  For template
			   instantiations this is the constant as specified
			   in the template declaration; it can be a
			   ck_template_param entry even though the routine
			   type is nondependent. */
  } variant;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		source_range;
			/* Source range of the declaration of this exception
			   specification -- from the source position of "throw"
			   or "noexcept" to that of the closing parenthesis.
			   If the specification is for a routine synthesized by
			   the front end, the start and ending positions are
			   both equal to that of the synthesized routine. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} an_exception_specification;

/*
This type exists even if the Microsoft keywords are not allowed, to permit
routines that deal with types to have a predictable number of parameters (they
can return a calling convention via a parameter even though it is never used).

If you add new calling conventions, be sure to update calling_convention_names.
*/
enum a_calling_convention : a_byte {
/* Microsoft-specific calling convention specifiers. */
  cc_default,		/* Default (unspecified) calling convention, which
			   is the same as/compatible with one of the others. */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
  cc_cdecl,		/* __cdecl calling convention. */
  cc_fastcall,		/* __fastcall calling convention. */
  cc_stdcall,		/* __stdcall calling convention. */
  cc_thiscall,		/* __thiscall calling convention. */
  cc_vectorcall,	/* __vectorcall calling convention. */
  cc_clrcall,		/* __clrcall calling convention. */
  cc_last		/* Must be last. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
};


#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
/* Display names for calling conventions. */
EXTERN_CONSTINIT_ARRAY(a_const_char*, calling_convention_names, cc_last)
#if VAR_INITIALIZERS
= { "<default>", "__cdecl", "__fastcall", "__stdcall", "__thiscall",
    "__vectorcall", "__clrcall" }
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(calling_convention_names)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */

/*
Enumeration of declaration modifiers that are accepted.  The enumeration values
are used to create bit masks that are used to represent the modifiers.

If you add new decl modifiers, be sure to update decl_modifier_names.
*/
enum a_decl_modifier : a_byte {
#if MICROSOFT_EXTENSIONS_ALLOWED
  dmt_dllimport,
  dmt_dllexport,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  dmt_thread,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_... */
#if MICROSOFT_EXTENSIONS_ALLOWED
  dmt_microsoft_inline,
  dmt_forceinline,
  dmt_selectany,
  dmt_novtable,
  dmt_noalias,
  dmt_restrict,
  dmt_safebuffers,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
  /* The order of the following link scope values (increasing strictness) is
     important. */
  dmt_global_link_scope,
  dmt_symbolic_link_scope,
  dmt_hidden_link_scope,
#endif /* SUN_EXTENSIONS_ALLOWED */
  dmt_last
};


#if DECL_MODIFIERS_IN_USE
EXTERN_CONSTINIT_ARRAY(a_const_char*, decl_modifier_names, dmt_last + 1)
#if VAR_INITIALIZERS
= {
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* dmt_dllimport */		"dllimport",
  /* dmt_dllexport */		"dllexport",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  /* dmt_thread */		"thread",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_... */
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* dmt_microsoft_inline */	"__inline",
  /* dmt_forceinline */		"__forceinline",
  /* dmt_selectany */		"selectany",
  /* dmt_novtable */		"novtable",
  /* dmt_noalias */		"noalias",
  /* dmt_restrict */		"restrict",
  /* dmt_safebuffers */		"safebuffers",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
  /* dmt_global_link_scope */	"__global",
  /* dmt_symbolic_link_scope */	"__symbolic",
  /* dmt_hidden_link_scope, */	"__hidden",
#endif /* SUN_EXTENSIONS_ALLOWED */
  /* dmt_last */		"last"
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(decl_modifier_names)
#endif /* DECL_MODIFIERS_IN_USE */

/*
A bit set whose values are used to supply additional declarative information
about variables and routines.
*/
typedef unsigned int a_decl_modifier_set;
#define DM_NONE		((a_decl_modifier_set)0x0)
			/* No decl modifiers. */
#if MICROSOFT_EXTENSIONS_ALLOWED
#define DM_DLLIMPORT	((a_decl_modifier_set)(1 << dmt_dllimport))
			/* TRUE if the declaration includes the
			   Microsoft __declspec(dllimport) specifier. */
#define DM_DLLEXPORT	((a_decl_modifier_set)(1 << dmt_dllexport))
			/* TRUE if the declaration includes the
			   Microsoft __declspec(dllexport) specifier. */
#define DM_DLLFLAGS	((a_decl_modifier_set)(DM_DLLIMPORT | DM_DLLEXPORT))
			/* Convenience macro to select DLL-related flags. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
#define DM_THREAD	((a_decl_modifier_set)(1 << dmt_thread))
			/* TRUE if the declaration includes the __thread or
			   __declspec(thread) specifier. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_... */
#if MICROSOFT_EXTENSIONS_ALLOWED
#define DM_MICROSOFT_INLINE						\
			((a_decl_modifier_set)(1 << dmt_microsoft_inline))
			/* TRUE if the declaration includes the
			   Microsoft __inline specifier. */
#define DM_FORCEINLINE	((a_decl_modifier_set)(1 << dmt_forceinline))
			/* TRUE if the declaration includes the
			   Microsoft __forceinline specifier. */
#define DM_SELECTANY	((a_decl_modifier_set)(1 << dmt_selectany))
			/* TRUE if the declaration includes the Microsoft
			   __declspec(selectany) specifier. */
#define DM_NOVTABLE	((a_decl_modifier_set)(1 << dmt_novtable))
			/* TRUE if the declaration includes the Microsoft
			   __declspec(novtable) specifier. */
#define DM_NOALIAS	((a_decl_modifier_set)(1 << dmt_noalias))
			/* TRUE if the declaration includes the Microsoft
			   __declspec(noalias) specifier. */
#define DM_RESTRICT	((a_decl_modifier_set)(1 << dmt_restrict))
			/* TRUE if the declaration includes the Microsoft
			   __declspec(restrict) specifier. */
#define DM_SAFEBUFFERS	((a_decl_modifier_set)(1 << dmt_safebuffers))
			/* TRUE if the declaration includes the Microsoft
			   __declspec(safebuffers) specifier. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
#define DM_GLOBAL_LINK_SCOPE \
			((a_decl_modifier_set)(1 << dmt_global_link_scope))
			/* TRUE if the declaration includes the Sun __global
			   specifier. */
#define DM_SYMBOLIC_LINK_SCOPE \
			((a_decl_modifier_set)(1 << dmt_symbolic_link_scope))
			/* TRUE if the declaration includes the Sun __symbolic
			   specifier. */
#define DM_HIDDEN_LINK_SCOPE \
			((a_decl_modifier_set)(1 << dmt_hidden_link_scope))
			/* TRUE if the declaration includes the Sun __hidden
			   specifier. */
#define DM_ANY_SUN_LINK_SCOPE	((a_decl_modifier_set)     \
				(DM_GLOBAL_LINK_SCOPE |    \
				 DM_SYMBOLIC_LINK_SCOPE |  \
				 DM_HIDDEN_LINK_SCOPE))
			/* TRUE if the entity was declared with any Sun link
			   scope specifier. */
#endif /* SUN_EXTENSIONS_ALLOWED */

/*
An enumeration describing the ref-qualifier of a member function type.
Ref-qualifiers are a C++11 feature indicating how the *this parameter should
be bound to lvalues and rvalues.  For example:

  struct S {
    void f() &;   // x.f() is only valid when x is an lvalue.
    void g() &&;  // x.g() requires x to be an rvalue.
    void h();     // In x.h() x can be an lvalue or an rvalue.
  };
*/
enum a_ref_qualifier_kind : a_byte {
  rqk_default,
  rqk_lvalue,
  rqk_rvalue
};


/* Entry containing additional information about a routine type
(segregated to keep down the size of a_type). */
typedef struct a_routine_type_supplement *a_routine_type_supplement_ptr;
typedef struct a_routine_type_supplement {
  a_param_type_ptr
                param_type_list;
			/* List of parameter types.  If prototyped is TRUE,
			   this is a list of the prototyped parameter types.
			   If prototyped is FALSE, the function has an
			   old-style parameter list: if assoc_routine is
			   non-NULL (meaning that the function definition
			   has been scanned), this points to the unpromoted
			   old-style parameter types; otherwise, it is NULL
			   (i.e., there is no information on parameter
			   types). */
#if DO_IL_LOWERING
			/* When IL lowering is configured to turn all functions
			   into old-style functions for cfront compatibility,
			   the param_type_list is not cleared from what it
			   was in the prototyped form, the idea being to
			   preserve as much information as possible.  It can,
			   however, create what appears to be an old-style
			   function with no definition that has param_type_list
			   non-NULL. */
#endif /* DO_IL_LOWERING */
  a_routine_ptr assoc_routine;
                        /* If this type is the type for a function that has
			   been defined (has a body) or if it is the type of
			   a special member with an "indeterminate" exception
			   specification, this points to the associated
			   function.  Otherwise, it is NULL. */
  a_bit_field	has_ellipsis:1;
                        /* TRUE if there is an ellipsis ("...") at the end of
                           the prototyped parameter list, indicating a
                           variable number of arguments. */
  a_bit_field	prototyped:1;
                        /* TRUE if the function interface is a prototyped
                           interface, FALSE if it is old-style. */
  a_bit_field	old_style_params_scanned:1;
			/* For functions with old-style parameter declarations,
			   TRUE if the parameter list has been scanned.
			   This allows one to tell when param_type_list is
			   NULL because there are no parameters and when it
			   is NULL because the parameters have not been
			   scanned yet.  Also useful in recognizing a function
			   declared with a prototype and defined with an
			   old-style definition.  Note that when IL lowering
			   is used and MAKE_ALL_FUNCTIONS_UNPROTOTYPED is
			   TRUE, there will be functions with prototyped FALSE
			   and old_style_params_scanned also FALSE. */
  a_bit_field	trailing_return_type:1;
			/* TRUE for function declarators specifying a trailing
			   return type (a C++11 feature).  E.g. "f()->int".
			   The composite type based on two routine types has
			   this flag TRUE if either of the two original types
			   has this flag set to TRUE. */
  a_bit_field	lint_argsused_flag:1;
                        /* TRUE if this function declaration is subject
                           to a lint-style "argsused" flag, indicating that
                           warnings on unreferenced parameters should not
                           be issued. */
  a_bit_field	value_returned_by_cctor:1;
			/* If TRUE, the caller provides a place for the return
			   value (by passing its address as a parameter), and
			   the called routine must place its result in that
			   location.  This is used only for functions that
			   return C++ class types, for cases where the
			   class type returned requires a copy constructor. */
#if DO_IL_LOWERING
  a_bit_field	value_returned_as_parameter:1;
			/* If TRUE, the routine is modified to accept an
			   additional parameter that is used in place of the
			   return value.  The caller places an address of
			   a class or struct in the new argument, and the
			   called function places the result in that location.
			   Currently used only when value_returned_by_cctor
			   is TRUE, but could be used in cases where
			   large structs are being returned. */
  a_bit_field	return_value_parameter_follows_this:1;
			/* In cases where value_returned_as_parameter
			   is TRUE, this flag controls whether the newly added
			   parameter comes after the 'this' parameter (TRUE)
			   or before (FALSE) in cases where the routine is a
			   member function.  This is typically determined
			   by the ABI being used, with IA-64 requiring the flag
			   to be FALSE, and the Cfront-like ABI requiring
			   a setting of TRUE. */
#endif /* DO_IL_LOWERING */
  a_bit_field	assoc_routine_is_ctor:1;
			/* TRUE if associated with a constructor, even if the
			   assoc_routine pointer has not yet been supplied.
			   Also TRUE for deduction guides. */
  a_bit_field	assoc_routine_is_dtor:1;
			/* TRUE if associated with a destructor, even if the
			   assoc_routine pointer has not yet been supplied. */
  a_bit_field	assoc_routine_is_lambda_body:1;
			/* TRUE if associated with a lambda call operator,
			   even if the assoc_routine pointer has not yet been
			   supplied. */
  a_bit_field	suppress_diagnostic_on_incomplete_return_type:1;
			/* TRUE if, upon calling the function or taking its
			   address, a diagnostic has been put out because the
			   return type is incomplete; when this flag is set,
			   diagnostics will not be issued on subsequent uses
			   (though diagnostics on function definitions are not
			   affected).  (Intended for front-end use only.) */
  ENUM_TYPE_FOR_BIT_FIELD(a_name_linkage_kind)
		routine_name_linkage:NUM_BITS_FOR_NAME_LINKAGE;
			/* The default name linkage at the point the function
			   type was declared.  The front end makes this
			   information available to the back end in case, for
			   example, different linkages imply different calling
			   conventions.  (Note: this value does not necessarily
			   correspond to the name linkage with which an
			   associated function was declared -- e.g.,
			     extern "C" typedef void FT();
			     FT f;
			     static FT g;
			   The name linkage associated with the routine type
			   to which FT points is nlk_external, but those for
			   functions f and g are nlk_cplusplus_external and
			   nlk_internal, respectively.) */
  a_bit_field	routine_name_linkage_is_explicit:1;
			/* TRUE when the routine_name_linkage is set based
			   on an explicit linkage specifier in the source. */
  a_bit_field	qualifiers:NUM_BITS_FOR_TYPE_QUALIFIER_SET;
			/* Used for nonstatic member functions: the cv-
			   qualification of the function type (e.g., the
			   "const" in "void f(int) const").  Can contain
			   qualifiers even when this_class (declared below) is
			   NULL in the case of a function typedef. */
  a_bit_field	this_qualifiers:NUM_BITS_FOR_TYPE_QUALIFIER_SET;
			/* Used for nonstatic member functions: The type
			   qualifiers that apply to the "this" pointer itself
			   (unlike "qualifiers" which describe type qualifiers
			   applicable to the object pointed to by "this").
			   In the unmodified front end, only the TQ_RESTRICT
			   qualifier is recorded here (for restrict-qualified
			   member functions). */
  ENUM_TYPE_FOR_BIT_FIELD(a_ref_qualifier_kind)
		ref_qualifiers:2;
			/* Used for nonstatic member functions: The
			   ref-qualification of the member function type.
			   (See a_ref_qualifier_kind above for details.) */
  a_bit_field	does_not_return:1;
			/* TRUE if this is the type of function that is known
			   not to return normally (it can still "return" via an
			   exception).  Usually, the type was declared with the
			   GNU attribute "noreturn" or "volatile". */
  a_bit_field	has_enable_if_attribute:1;
			/* TRUE if the type was declared with the "enable_if"
			   clang attribute. */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	result_should_be_used:1;
			/* TRUE if the type was declared with the attribute
			   "warn_unused_result". */ 
  a_bit_field	is_const:1;
			/* TRUE if the type was declared with the "const"
			   attribute.  Note that this flag is not set on the
			   type of a const member function (unless the "const"
			   attribute is also specified). */
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_bit_field	is_variadic_instance:1;
			/* TRUE for types of template instances generated from
			   variadic function templates. */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
  a_bit_field	explicit_calling_convention:1;
			/* TRUE is a calling convention was specified
			   explicitly. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
  a_bit_field	had_been_implicitly_const:1;
			/* TRUE if the (non-static member function) type had
			   been implicitly considered "const" in C++11 mode
			   but is no longer in C++14.  Member functions with
			   this flag set will have a different mangled name in
			   C++11 and C++14 (unless
			   mangle_had_been_implicitly_const is TRUE). */
  a_bit_field	is_conditionally_explicit:1;
			/* TRUE if the type is associated with a routine
			   declared with the "explicit(<boolean-expression>)"
			   construct (a C++20 feature).  (This is part of the
			   type because it is substituted as part of template
			   argument deduction, which works with types rather
			   than routines.) */
  a_bit_field	has_this_param:1;
			/* TRUE if this is the type of a nonstatic member
			   function.  (Usually, this is equivalent to
			   this_class != NULL, but this_class is sometimes
			   temporarily set to NULL.) */
  a_lint_varargs_count
	         lint_varargs_count;
                        /* If not equal to NOT_LINT_VARARGS (-1), this
                           function declaration is subject to a lint-style
                           "varargs" comment.  The count is the argument to
                           the "varargs", indicating the number of fixed
                           arguments (or 0 if the argument is omitted). */
  a_pragma_kind	arg_pragma;
                        /* Indicates whether or not a #pragma implying
                           special argument-type checking (e.g., for printf)
                           applies to this function type. */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
  a_calling_convention
		calling_convention;
			/* Calling convention for this routine type (e.g.,
			   __cdecl, __fastcall). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  int		fmt_arg;
			/* When arg_pragma is pk_printf_args or
			   pk_scanf_args, the argument that will
			   contain the format string, or zero if the
			   format string is the last argument before
			   the ellipsis.  When arg_pragma is not
			   pk_printf_args or pk_scanf_args, the
			   argument that will contain the fmt_string
			   for a routine marked with the "format_arg"
			   attribute. */
  int		format_first_subst_arg;
			/* When the GNU "format" attribute is applied
			   (resulting in arg_pragma being pk_printf_args or
			   pk_scanf_args), contains the value of the third
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
  a_type_ptr	this_class;
			/* For nonstatic member functions this is a pointer
			   to the (unqualified, untypedefed) class type of
			   which they are a member (i.e., the class of
			   "*this").  For any other routine type this is
			   NULL. */
  a_scope_ptr   prototype_scope;
                        /* Almost always NULL.  In rare cases, points to
                           a scope entry that contains things declared
                           in the function prototype scope (like struct
                           tags, enum constants, etc.).  The parameters
                           themselves are never included in this scope.
                           This can be used for prototypes in both
                           function declarations and function definitions.
                           In the latter case, the declarations here are
                           really part of the function scope; they are
                           separated like this because the function prototype
                           information is required outside of the routine,
                           in order to call it.  Thus, the two scopes are
                           really considered to be one compound scope.
                           Also used for types and tags declared in an
                           old-style parameter list, because those types
                           are likewise needed outside the routine in order
                           to check type compatibility.  Always NULL in C++
                           unless RECORD_HIDDEN_NAMES_IN_IL is TRUE, in which
                           case the prototype scope will be used solely for
                           the hidden name list. */
  an_exception_specification_ptr
		exception_specification;
			/* In C++ only, pointer to an entry describing the
			   exception specification declared for this routine.
			   NULL when any exception may be thrown, e.g.,
			     void f();   // No exception specification declared
			   Also NULL in C mode or if exceptions are disabled
			   for this compilation; also NULL if the type is not
			   bound to a particular routine. */
} a_routine_type_supplement;


/*
A template argument may be a type, nontype, or template argument.  This
enumeration is used to specify which variant of the template argument
entry is being used.
*/
enum a_templ_arg_kind : a_byte {
  tak_type,
  tak_nontype,
  tak_template,
  tak_start_of_pack_expansion
			/* tak_start_of_pack_expansion marks the beginning
			   of a (possibly empty) sequence of template arguments
			   provided for a parameter pack. */
};


typedef struct a_template_arg {
  /* Representation of an actual argument of an instance of a template class
     or template function.  A list of these is used to represent the actual
     argument list for such an instance. */
  a_template_arg_ptr
                next;   /* Next in a linked list of template arguments. */
  a_templ_arg_kind
		kind;
			/* Specifies whether this is a type, nontype,
			   or template template argument. */
  struct a_pack_expansion_descr
		*pack_expansion_descr;
			/* If non-NULL, this argument is a pack expansion,
			   and this points to the expansion description. */
  a_bit_field	is_array_bound_of_unknown_type:1;
			/* TRUE if the template argument is a deduced array
			   bound whose type is not yet known. */
  a_bit_field	explicitly_specified:1;
			/* TRUE if the argument was explicitly specified.
			   When a reference is being processed, this flag
			   is set only for those arguments that were
			   explicitly specified for that reference.  For a
			   template argument list associated with an
			   instance of the template, this flag is set if
			   any reference to the instance explicitly
			   specified the argument. */
  a_bit_field	template_template_param_checked:1;
			/* TRUE for template template arguments if the template
			   parameter list of the argument template has already
			   been compared with that of the parameter
                           template.  Used only in the front end. */
  a_bit_field	is_pack_element:1;
			/* TRUE if this is an argument for which the
			   associated parameter is a template parameter pack.
			   Before a group of such an arguments is seen, a
			   tak_start_of_pack_expansion placeholder argument
			   will have been encountered.  Note that the set of
			   elements associated with a pack expansion may be
			   empty. */
  a_bit_field	is_pack:1;
			/* If this is an element of a nonreal instantiation
			   argument list, this is TRUE if the associated
			   template parameter is a pack. */
  a_bit_field	has_pack_ellipsis:1;
			/* This is similar to is_pack, but is used by the
			   C++-generating back end to emit an ellipsis in
			   some alias-in-template-declaration cases where
			   setting is_pack is not desired because it has
			   other implications in the front end. */
  a_bit_field	is_integer_pack:1;
			/* TRUE for a dependent nontype template argument of
   			   the form "__integer_pack(expr)...". */
  a_bit_field	type_is_injected_class_name:1;
			/* TRUE for a type argument if the type was specified
			   using the injected class name.  This is only
			   set when scanning "unknown" template argument
			   lists where the corresponding parameter is not
			   known.  In such cases the injected class name
			   could end up being used as a template template
			   argument. */
  a_bit_field	is_provisional_value:1;
			/* TRUE if the argument value was deduced from an
			   array bound and should only be used if it cannot
			   be deduced elsewhere. */
  a_bit_field	is_error:1;
			/* TRUE if this template argument was created as part
			   of the representation of an error condition (but
			   not if the template argument simply refers to an
			   error type, error constant, or error template).
			   Used in particular when an error template argument
			   is created for a reference to a missing pack
			   element, and such a reference should cause
			   substitution to fail. */
  a_bit_field	param_is_auto:1;
			/* TRUE if this is a nontype template argument for
			   a parameter whose type is given by auto.  Set
			   and used only by the C++-generating back end. */
  a_bit_field	param_is_decltype_auto:1;
			/* TRUE if this is a nontype template argument for
			   a parameter whose type is given by
			   decltype(auto).  Set and used only by the
			   C++-generating back end. */
#if BACK_END_IS_CP_GEN_BE
  a_bit_field	access_being_checked:1;
			/* Used by the C++-generating back end to prevent
			   unbounded recursion when checking the
			   accessibility of a template argument. */
#endif /* BACK_END_IS_CP_GEN_BE */
  union {
    /* When kind == tak_type. */
    a_type_ptr  type;   /* The type supplied as the argument.  This type can
			   be NULL in a template argument list for a nonreal
			   class in certain cases in Microsoft mode. */
    /* When kind == tak_nontype and is_array_bound_of_unknown_type == FALSE. */
    a_constant_ptr
                constant;
			/* The constant supplied as the argument.  Note that
			   when arg_operand (below) is non-NULL, this field
			   may be NULL.  If both are set, this field points
			   to the version that stays in the IL, which has less
			   information than the arg_operand form.  Also
			   NULL for an unspecified template argument at
			   the end of a list that begins with explicit template
			   arguments. */
    /* When kind == tak_nontype and is_array_bound_of_unknown_type == TRUE. */
    a_targ_size_t
		integer_value;
			/* The integer value deduced from an array bound.
                           This value is only used during type deduction.
                           At the end of type deduction, the type of the
                           parameter being deduced is known and this value
                           is converted into a normal constant parameter.
                           Contains zero if no value has been deduced yet. */
    /* When kind == tak_template */
    struct {
      a_template_ptr
		ptr;
			/* The template supplied as the argument. */
      a_template_ptr
		substituted_param_template;
			/* If the template template parameter for which this
			   is an argument has a template parameter with a
			   dependent type, this points to the rescanned
			   template parameter.  This comes up in cases like
			   "template <class T, template <T t> struct X> ...".
			   The rescanned version of the parameter list must
			   be used when scanning template argument lists of
			   the template template parameter.  Used only in
			   the front end. */
    } templ;
    /* There is no variant when kind == tak_start_of_pack_expansion. */
  } variant;
  an_arg_operand_ptr
		arg_operand;
			/* When an explicit function template argument list
			   is scanned, the corresponding parameter type is
			   not yet known.  Consequently, the argument cannot
			   be converted to its eventual type, nor can a member
			   of an overload set be selected.  Instead, the
			   argument must be retained in a form that permits
			   such operations to be performed later when the
			   parameter type (or potential parameter type) is
			   known.  The argument is represented by the
			   type "an_arg_operand", which is used within the
			   expression processing routines.  NULL for cases
			   other than the above, and meaningful only within
			   the front end proper.  Used only in the same
			   cases as the "constant" field above, i.e., for
			   nontype parameters. */
#if BACK_END_IS_CP_GEN_BE
  a_template_arg_ptr
		parent_arg;
			/* The template argument within which this template
			   argument appears, if any.  Set by
			   form_template_args and used by the
			   C++-generating back end to prevent unbounded
			   recursion when substituting non-real typedefs
			   for their underlying types. */
#endif /* BACK_END_IS_CP_GEN_BE */
} a_template_arg;


/* Data structures related to C++ classes (type entries of kind tk_class,
   tk_struct, and tk_union). */

typedef struct an_overriding_virtual_function
                                         *an_overriding_virtual_function_ptr;
typedef struct an_overriding_virtual_function {
  /* Representation for a virtual function that is declared in a derived
     class and that overrides a virtual function declared in a base class
     (see ARM 10.2).  The declaration that is overridden is referred to as
     the "primary" virtual function.  This data structure is associated
     with the base class entry identifying the class of the primary
     declaration, and provides information required for constructing a
     virtual function table for the base class. */
  an_overriding_virtual_function_ptr
		next;	/* Next in a linked list of overriding virtual
			   function entries. */
  a_routine_ptr overriding_function;
			/* A pointer to the routine entry for the function
			   that overrides the primary virtual function. */
  a_routine_ptr primary_function;
			/* A pointer to the virtual function that is
			   overridden. */
  a_base_class_ptr
		base_class;
			/* A pointer to the base class entry, on the
			   base_classes list of the current derived class,
			   identifying the class of which the overriding
			   function is a member; when it is a member of the
			   current derived class, then this field is NULL. */
  a_base_class_ptr
		return_adjustment_base_class;
			/* When the return types of the overriding and
			   overridden functions are pointer or reference to D
			   and B, respectively (where class D is derived from
			   class B), a pointer to a base class entry for B on
			   the base_classes list of D.  The entry represents
			   the adjustment required on the return.  The field
			   is NULL when no such adjustment is required (i.e.,
			   when the return types of the overriding and
			   overridden functions are identical). */
} an_overriding_virtual_function;


typedef struct a_derivation_step *a_derivation_step_ptr;
typedef struct a_derivation_step {
  /* Description of one step in the derivation of a projection symbol from a
     fundamental class member.  A list of these gives a segment of the reverse
     history of the derivation, in order from the most derived class to the
     fundamental class. */
  a_derivation_step_ptr
                next;
			/* The next step in the derivation path.  If next is
			   NULL, this is the end of the list.  Note that a
			   derivation step may be shared amongst different base
			   class derivations, in which case the last step of
			   the derivation path is denoted by path_tail in
			   a_base_class_derivation. */
  a_derivation_step_ptr
                prev;
			/* Previous entry in a doubly linked list; NULL for
			   the first entry on the list. */
  a_base_class_ptr
                base_class;
			/* A pointer to the base class entry representing
			   the class to which the current object should
			   be cast in traversing the derivation path. */
} a_derivation_step;


typedef struct a_base_class_derivation *a_base_class_derivation_ptr;
typedef struct a_base_class_derivation {
  /* Entry identifying the unique derivation of a nonvirtual base class or
     one of the alternative derivations of a virtual base class.  A derivation
     path is a sequence of steps *from* the most derived class (the first
     entry is a direct base class) *to* the base class whose derivation is
     being described; in other words, it represents the steps of a cast from
     the derived class to the base class.  For instance:
                                                                       A
        class A { };                                                   |
        class B : public A { };                                        B
        class C : public B { };                                        |
                                                                       C
     In the context of C the derivation of direct base class B has one step
     (==>B) and that of indirect base class A has two steps (==>B==>A).  A
     virtual base class may have several paths.  For instance:
                                                                       A
        class A { };                                                 / |
        class B : virtual public A { };                             B  |
        class C : public B, virtual public A { };                    \ |
                                                                       C
     In the context of C virtual base class A has two derivations: as a
     direct base class (==>A) and as an indirect base class (==>B==>A).  An
     indirect base class that is itself a base class of a virtual base class
     has multiple derivations as well, but only the segment of the derivation
     from the intervening virtual base class is explicitly represented in the
     IL.  For instance, modify the preceding example as follows:       X
                                                                       |
        class X { };                                                   A
        class A : public X { };                                      / |
        class B : virtual public A { };                             B  |
        class C : public B, virtual public A { };                    \ |
                                                                       C 
     The derivation path for X is represented as ==>A==>X, but since A is a
     virtual base class with two derivations, this is tantamount to ==>A==>X
     (where A is a direct base class of C) and ==>B==>A==>X (where A is an
     indirect base class of C).  The two paths for X are inferred by
     supplementing the derivation entry for X, which points to the path
     ==>A==>X, with the two derivation entries for A, the direct one and the
     indirect one.  Since there may be several virtual base classes in a
     derivation, the number of paths to be inferred may multiply. */
  a_base_class_derivation_ptr
		next;
			/* Next in a linked list of virtual derivation entries
			   representing the various derivations specified for
			   a given virtual base class; always NULL if the
			   associated base class is nonvirtual, and NULL for
			   the last entry in the list when it is virtual. */
  a_derivation_step_ptr
		path;
			/* Pointer to (all or part of) the path from the
			   derived class to the associated base class.  If
			   direct is TRUE, the derivation consists of a single
			   step which points to the associated base class.  If
			   direct is FALSE, it consists of two or more steps,
			   starting with a step entry pointing to a virtual or
			   nonvirtual direct base class or a virtual indirect
			   base class, followed by zero or more steps pointing
			   to nonvirtual indirect base classes, and terminated
			   by a step that points to the associated virtual
			   base class.  Note that when the path starts with a
			   virtual indirect base class, the part of the path
			   from the derived class to that indirect base class
			   has been elided. */
  a_derivation_step_ptr
		path_tail;
			/* Pointer to the end of the path from the derived
			   class to the associated base class. */
  a_bit_field	direct:1;
			/* TRUE if the associated base class is a direct
			   base class as a result of this derivation. */
  a_bit_field	preferred:1;
			/* TRUE if this derivation is "preferred" because it
			   affords better access from the derived class to the
			   base class; when two or more derivations give equal
			   access, the path with no virtual base classes is
			   preferred over one that has a virtual base class,
			   and a direct derivation is preferred over an
			   indirect derivation. */
  an_access_specifier
		access; /* The kind of derivation (public, protected, or
			   private) specified for the final step of the
			   derivation pointed to by path.  This is tantamount
			   to the access specified from the associated base
			   class to the class directly derived from it. */
} a_base_class_derivation;

#if DO_IL_LOWERING
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
/*
Index into the array of virtual function table pointers used to adjust
virtual function tables during construction and destruction in the presence
of overridden functions in virtual base classes.
*/
typedef unsigned short a_construction_vtbl_array_index;
/* a_construction_vtbl is declared in lower_il.h and is opaque here. */
typedef struct a_construction_vtbl *a_construction_vtbl_ptr;
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#endif /* DO_IL_LOWERING */

typedef unsigned short a_base_class_sequence_number;
			/* The type used for the direct base class sequence
			   number of a base class entry. */

typedef struct a_base_class {
  /* An entry describing a base class from which a class is directly or
     indirectly derived. */
  a_base_class_ptr
                next;
			/* Next in linked list of base class entries (in
			   postorder traversal order). */
  a_base_class_ptr
                next_direct;
			/* Next in linked list of direct base class entries (in
			   declaration order). */
#if IA64_ABI
  a_base_class_ptr
		next_preorder;
			/* The next base class, in a preorder traversal of the
			   base classes.  (The "next" pointer is the next
			   class in a postorder traversal.) */
  a_base_class_ptr
  		primary_base_class;
			/* The primary base class for this class, i.e., the
			   most derived class with which this subobject shares
			   virtual function info.  Note that this may be NULL,
			   even if the primary_base_class for type is
			   non-NULL; that indicates that a virtual primary
			   base has been allocated as part of some other
			   base. */
#endif /* !IA64_ABI */
  an_attribute_ptr
		attributes;
			/* Attributes applicable to this base class. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_ms_attribute_ptr
		ms_attributes;
			/* Linked list of Microsoft attribute entries that
			   apply to this base class.   (Currently, this is
			   possible in C++/CX mode only, and in that case the
			   "base class" is an "interface class".) */ 
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_type_ptr    type;
			/* Pointer to the tk_class or tk_struct type entry
			   representing a base class of the current derived
			   class.  (Unions may not be used as base classes.) */
  a_type_ptr	orig_type;
			/* Pointer to the type specified in the base specifier
			   list, which might be the same as type or might be a
			   typedef.  (NULL for indirect base classes because
			   different derivation paths can specify the type in
			   different ways.) */
  a_type_ptr	derived_class;
			/* The class derived (directly or indirectly) from
			   this base class on whose base_classes list it
			   appears. */
  struct a_trans_unit_corresp
		*trans_unit_corresp;
			/* When compiling multiple translation units, points
			   to an entry used to describe entries that refer
			   to the same entity.  See the trans_unit_corresp
			   field of the source correspondence entry for more
			   information.  This points to a front end data
			   structure and is for front end use only. */
  a_source_position
		decl_position;
			/* For a direct base class, the source position of
			   its declaration.  Otherwise, the source position
			   of a direct base class derived from it. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		base_specifier_range;
			/* For a direct base class, the source positions
			   corresponding to the start and end of the base
			   specifier construct (i.e., possibly including the
			   access specifier and "virtual"). */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_bit_field	direct:1;
			/* TRUE if this is a direct base class of
			   derived_class in any of its derivations. */
  a_bit_field	is_virtual:1;
			/* TRUE if this is a virtual base class (whether
			   directly or indirectly inherited). */
  a_bit_field	ambiguous:1;
			/* TRUE if a direct cast from derived_class to this
			   base class would be ambiguous because it appears
			   more than once in the derivation. */
  a_bit_field	shares_virtual_function_info:1;
			/* TRUE if a class derived from this base class, either
			   derived_class itself or an intermediate base class
			   (one on the derivation path of this base class),
			   shares its virtual function info with this base
			   class.  This flag denotes sharing from the point
			   of view of the base class in reference to a class
			   derived from it; virtual_function_info_base_class,
			   a pointer in a_class_type_supplement, denotes the
			   sharing from the opposite point of view.  Note
			   that this flag reflects the Cfront-like ABI
			   view of the world, and will be set only if the
			   class declares virtual functions itself.  In the
			   IA-64 ABI, a class that declares no virtual
			   functions but inherits some can have a primary
			   base class (primary_base_class non-NULL) but
			   that base class will have
			   shares_virtual_function_info set to FALSE. */
  a_bit_field	ignore_during_dependent_lookup:1;
			/* TRUE if this base class should not be considered
			   when looking up dependent names.  This is the case
			   for base classes of instances of a class template
			   where the base class name was specified as a
			   template-dependent name. */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  a_bit_field	complete_subobject:1;
			/* TRUE if direct is TRUE and the subobject is
			   "complete" (i.e., may contain data sections for
			   virtual base classes).  By default subobjects for
			   direct base classes do not include virtual base
			   class data sections -- this flag is needed only when
			   strict class-layout compatibility with USL's
			   cfront is required. */
  a_bit_field	pointer_offset_is_set:1;
			/* TRUE if the pointer_offset field has been set
			   in the course of prior processing.  This flag is
			   required to distinguish an initial 0 from an
			   assigned 0.  (This flag is required only in cfront
			   layout compatibility mode because the multipass
			   scheme used to emulate cfront's ordering algorithm
			   involves visiting a base class more than once.) */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  a_bit_field   is_optimized_empty_base:1;
			/* For the classic EDG ABI, TRUE if and only if this
			   is a direct empty base that has been optimized
			   (i.e., allocated at the same offset as another
			   subobject).  For the IA-64 ABI, TRUE for every
			   direct empty base. */
#if IA64_ABI
  a_bit_field	offset_is_set:1;
                        /* TRUE for a base after its offset has been set. */
#endif /* IA64_ABI */
  a_bit_field	is_pack_expansion:1;
			/* TRUE if this base class is a variadic template
			   pack expansion, i.e., it's followed by "...". */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_implicit_direct_base:1;
			/* TRUE if this base class is a direct base class
			   added implicitly to certain C++/CLI managed class
			   types (e.g., System::ValueType is usually added
			   implicitly to value class types). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	has_public_derivation:1;
			/* TRUE if there is a derivation of derived_class from
			   this base class with public access for all steps.
			   In this case, no further access checking needs to be
			   done. */
  a_base_class_sequence_number
		direct_base_number;
			/* For a direct base class, the sequence number of
			   this base class entry.  The first base class is
			   number 1.  Zero for indirect base classes. */
  a_targ_size_t	offset;
			/* The byte offset from the start of the current
			   derived class to the data section of this base
			   class. */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  a_base_class_ptr
		data_section_base_class;
			/* If is_virtual is TRUE and the data section for this
			   virtual base class is embedded in the data section
			   reserved for another base class, a pointer to the
			   latter (which may be direct or indirect, and may
			   have virtual steps in its derivation); NULL when
			   the data section for the virtual base class is
			   reserved independently (i.e., in derived_class).
			   Only needed when layout compatibility with USL's
			   cfront is required. */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#if !IA64_ABI
  a_targ_size_t	pointer_offset;
			/* If is_virtual is TRUE, the byte offset from the
			   start of derived_class to a pointer to the data
			   section of this virtual base class; otherwise
			   undefined. */
  a_base_class_ptr
		pointer_base_class;
			/* If is_virtual is TRUE, a pointer to a another base
			   class (direct or indirect, but without virtual
			   steps in its derivation) of derived_class, the
			   data section of which contains the pointer to the
			   data section for this virtual base class; NULL if
			   the pointer to the data section for this virtual
			   base class resides in derived_class itself (which
			   is usually the case for direct virtual base classes
			   and sometimes the case for indirect virtual base
			   classes).  This field is defined in conjunction with
			   pointer_offset: when pointer_base_class is NULL,
			   pointer_offset specifies the offset of a pointer
			   field in derived_class itself; when
			   pointer_base_class is non-NULL, pointer_offset
			   specifies the offset (within derived_class) of a
			   pointer field in the base class pointed to. */
#endif /* !IA64_ABI */
  a_base_class_derivation_ptr
		derivation;
			/* If is_virtual is FALSE, pointer to a single entry
			   describing the derivation of derived_class from
			   this base class; if is_virtual is TRUE, pointer to
			   a linked list of entries describing one or more
			   alternative derivations.  NULL for a dummy base
			   class invented for a projection of a member of
			   a nonreal class into another class, e.g., via
			   a using-declaration. */
  union {
    /* If is_pack_expansion = FALSE: */
    an_overriding_virtual_function_ptr
		overriding_virtual_functions;
			/* Pointer to a linked list of entries representing
			   functions declared in derived classes that
                           override virtual functions declared in the
                           current base class.  These entries are sorted by
			   virtual function number of the routine pointed
			   to by the primary_function field. */
    /* If is_pack_expansion = TRUE: */
    struct a_pack_expansion_descr
		*pack_expansion_descr;
			/* The pack expansion description for this base
			   specifier.  (Front end only.) */
  } variant;
#if DO_IL_LOWERING
#if !IA64_ABI
  a_variable_ptr
		virtual_function_table_var;
			/* When IL lowering is done, this points to the
			   variable that contains the virtual function table
			   for this base class/derived class combination.
			   NULL until allocated and NULL if not needed. */
#else /* IA64_ABI */
  a_virtual_table_index
                virtual_function_table_offset;
                        /* The index in the derived class virtual table group
                           where the virtual table for this base begins, or
                           -1 if this base has no virtual table. */
#endif /* IA64_ABI */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  a_construction_vtbl_array_index
		index_in_construction_vtbl_array;
			/* Non-zero if this base class's override list includes
			   at least one overriding virtual function for which
			   the derivation between the class of the overriding
			   function and the class of the primary function
			   (i.e., the class indicated by this base class entry)
			   contains a virtual step.  When non-zero, special
			   handling of virtual function tables is required
			   during construction and destruction.  The entry
			   at the indicated element (-1) of the construction
			   vtbl array gives the address of the virtual
			   function table to be used for the base class vtbl
			   pointer during the body of the derived class
			   constructor when constructing a complete object
			   of the derived class type. */
#if !IA64_ABI
  /* When is_virtual is FALSE: */
#endif /* !IA64_ABI */
  a_construction_vtbl_array_index
		base_subarray_index_in_construction_vtbl_array;
			/* Non-zero if the constructor or destructor for
			   the base class must be passed an array of vtbl
			   pointers to be used during construction or
			   destruction to deal with overridden virtual
			   functions in virtual base classes.  Gives
			   the number of the element (-1) of the construction
			   vtbl array for the whole current class at which
			   the subarray that is to be passed to the base class
			   constructor or destructor begins. */
#if !IA64_ABI
  /* When is_virtual is TRUE: */
  a_construction_vtbl_ptr
		base_construction_vtbls;
			/* If non-NULL, this virtual base class contains
			   overridden virtual functions in virtual base
			   classes and requires special versions of virtual
			   function tables when a constructor or destructor
			   is called for a subobject.  This points to the
			   first on a list of entries describing (in order)
			   the elements of an array of vtbl addresses to be
			   passed to the base class constructor or destructor
			   when constructing or destroying this base class
			   in a complete object. */
#endif /* !IA64_ABI */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if IA64_ABI
  a_virtual_table_index
  		vbase_offset_index;
			/* If is_virtual is TRUE, the index, counting from 
			   the address point in the derived class primary
			   virtual table to the location containing the
			   base class's virtual base offset.  If
			   is_virtual is FALSE, this field is unused. */
#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING */
} a_base_class;


typedef struct a_class_list_entry *a_class_list_entry_ptr;
typedef struct a_class_list_entry {
  /* An entry used to represent a member of an arbitrary set of classes,
     used to list the classes befriended by a given class and the classes
     befriending a given class or routine. */
  a_class_list_entry_ptr
                next;
			/* Next in a linked list of class list entries. */
  a_type_ptr    class_type;
			/* The tk_class, tk_struct, or tk_union type entry. */
} a_class_list_entry;


typedef struct a_routine_list_entry *a_routine_list_entry_ptr;
typedef struct a_routine_list_entry {
  /* An entry used to represent a member of an arbitrary set of routines,
     used to list the routines declared as friends of a given class. */
  a_routine_list_entry_ptr
                next;
			/* Next in a linked list of routine list entries. */
  a_routine_ptr routine;
			/* Pointer to the routine entry. */
} a_routine_list_entry;


typedef struct a_variable_list_entry *a_variable_list_entry_ptr;
typedef struct a_variable_list_entry {
  /* An entry used to represent a member of an arbitrary set of variables. */
  a_variable_list_entry_ptr
                next;	/* Next in a linked list of variable list entries. */
  a_variable_ptr variable;
			/* Pointer to the variable entry. */
} a_variable_list_entry;


typedef struct a_constant_list_entry *a_constant_list_entry_ptr;
typedef struct a_constant_list_entry {
  /* An entry used to represent a member of an arbitrary set of constants. */
  a_constant_list_entry_ptr
                next;	/* Next in a linked list of constant list entries. */
  a_constant_ptr
                constant;
			/* Pointer to the constant entry. */
} a_constant_list_entry;


enum an_anonymous_union_kind : a_byte {
  auk_none,		/* Not an anonymous union. */
  auk_variable,		/* Anonymous union is associated with a variable. */
  auk_field		/* Anonymous union is associated with a field. */
};


#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Enumeration describing the set of inheritance kinds that can be specified for
a class, corresponding to different pointer-to-member representations.  The
order is significant: single < multiple < virtual.

If you add new inheritance kinds, be sure to update inheritance_kind_names.
*/
enum an_inheritance_kind : a_byte {
  ihk_none,		/* No inheritance kind specified. */
  ihk_single,		/* Single inheritance specified. */
  ihk_multiple,		/* Multiple inheritance specified. */
  ihk_virtual,		/* Virtual inheritance specified. */
  ihk_incomplete,	/* The inheritance kind was needed without knowing
			   the base classes yet (i.e., before the base class
			   specifiers have been seen). */
  ihk_last = ihk_incomplete
};


/*
Names of inheritance kinds, used for diagnostics.
*/
EXTERN_CONSTINIT_ARRAY(a_const_char*, inheritance_kind_names, ihk_last + 1)
#if VAR_INITIALIZERS
= {
/* ihk_none */		"none",
/* ihk_single */	"__single_inheritance",
/* ihk_multiple */	"__multiple_inheritance",
/* ihk_virtual */	"__virtual_inheritance",
/* ihk_incomplete */	"incomplete"
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(inheritance_kind_names)


/*
Class type kinds to distinguish the various kinds of C++/CLI class types.
*/
enum a_cli_class_type_kind : a_byte {
  cctk_standard,	/* Standard classes, structs, and unions. */
  cctk_value,		/* C++/CLI value classes and structs. */
  cctk_ref,		/* C++/CLI ref classes and structs. */
  cctk_interface,	/* C++/CLI interface classes and structs. */
  cctk_unresolved	/* An unresolved C++/CLI type: This occurs when a type
			   used in an assembly being imported comes from
			   another assembly that has not been imported. */
};


typedef struct a_property_index_type *a_property_index_type_ptr;
typedef struct a_property_index_type {
  /* Description of a "property index type" for a C++/CLI indexed property.
     E.g., for 
       ref struct S { property int p[int, char] { ... } };
     two entries are generated to record the property index types "int" and
     "char". */
  a_property_index_type_ptr
		next;
			/* Pointer to the next index type entry (or NULL if
			   there is none). */
  a_type_ptr	type;
			/* The type declared for the index. */
  a_source_position
		position;
			/* The position of the index type. */
} a_property_index_type;


enum a_property_or_event_kind : a_byte {
  /* Kinds of properties and events. */
  pek_declspec_property,
  pek_cli_property,
  pek_cli_event
};


typedef struct a_property_or_event_descr {
  /* Description of a Microsoft property or event member.  Microsoft compilers
     support two kinds of property constructs.  One kind is obtained by
     modifying an ordinary field declaration with a __declspec(property(...))
     attribute: The modified field will point to an entry of type
     a_property_or_event_descr (recording the names of the names of the "get"
     and "put" functions).  The other kind is obtained using C++/CLI syntax
     involving a context-sensitive keyword "property".  For example:
         ref struct S {
           property int p1 { int get(); };
           static property char p2 { void set(char); };
         };
     The property's a_field or a_variable entry (the latter is used for static
     properties) will point to an entry of type a_property_or_event_descr that
     in turn points to the accessor functions (get and/or set).  These accessor
     functions (which have special_kind sfk_property_get or sfk_property_set)
     also point to the associated a_property_or_event_descr.
     There is only one kind of event syntax and it is valid only in C++/CLI
     mode.  Its syntax is similar to that of C++/CLI property constructs but
     involves the context-sensitive keyword "event".  For example:
         ref struct S {
           delegate bool A(void*);
           event A^ actions {
             void add(A^);
             void remove(A^);
             bool raise(void*);
           }
         };
     The accessor functions are "add", "remove", and (optionally) "raise" in
     this case; each with its own special_kind value.
   */
  a_property_or_event_kind
		kind;
			/* Indication of whether this is a C++/CLI event, a
			   C++/CLI property, or a property declared using a
			   __declspec(property(...)) attribute. */
  a_bit_field	is_trivial:1;
			/* TRUE if this is a C++/CLI property or event declared
			   without explicit accessor functions.  Such "trivial"
			   properties and events have associated storage
			   represented by the associated field or static data
			   member. */
  a_bit_field	is_default_indexed:1;
			/* TRUE if this entry is for a default-indexed
			   property (C++/CLI syntax only). */
  a_bit_field	is_virtual:1;
			/* TRUE if this is a C++/CLI property or event declared
			   with the "virtual" specifier. */
  a_bit_field	is_static:1;
			/* TRUE if this is a C++/CLI property or event declared
			   with the "static" specifier. */
  a_property_index_type_ptr
		indices;
			/* Non-NULL only for an indexed property.  Points to a
			   list of entries describing the types of the property
			   indices.  (C++/CLI syntax only.) */
  union {
    /* When is_static is FALSE: */
    a_field_ptr
		field;	/* Field associated with an event or property. */
    /* When is_static is TRUE: */
    a_variable_ptr
		variable;
			/* Static data member associated with a C++/CLI event
			   or property. */
  } variant;
  union {
    /* When kind == pek_declspec_property: */
    a_const_char
		*name;	/* Name (null-terminated) specified by a Microsoft
			   __declspec(property(get=...)) attribute.  NULL if
			   the "get" name was not specified.  */
    /* When kind == pek_cli_property: */
    a_routine_ptr
		ptr;	/* Accessor "get" routine. */
  } get_routine;
  union {
    /* When kind == pek_declspec_property: */
    a_const_char
		*name;	/* Name (null-terminated) specified by a Microsoft
			   __declspec(property(put=...)) attribute.  NULL if
			   the "put" name was not specified.  */
    /* When kind == pek_cli_property: */
    a_routine_ptr
		ptr;	/* Accessor "set" routine. */
  } set_routine;
  a_routine_ptr
		add_routine;
			/* Accessor "add" routine for an event; NULL if this
			   entry is for a property. */
  a_routine_ptr
		remove_routine;
			/* Accessor "remove" routine for an event; NULL if this
			   entry is for a property. */
  a_routine_ptr
		raise_routine;
			/* Accessor "raise" routine for an event; NULL if this
			   entry is for a property. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		property_or_event_position;
			/* The position of the "property" or "event"
			   keyword. */
  a_source_range
		indices_range;
			/* The source position range delimited by the "["
			   and "]" tokens of the property indices, or
			   null_source_range if there are no indices. */
  a_source_range
		definition_range;
			/* The source position range delimited by the "{"
			   and "}" tokens enclosing the property accessor
			   declarations, or, in the case of a trivial
			   property, the range consisting solely of the ";"
			   token. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} a_property_or_event_descr;

/*
Values for the rewritten_property_reference_kind field, indicating
the original kind of operator rewritten as a property reference.
*/
enum a_rewritten_property_reference_kind : a_byte {
  rprk_none,
  rprk_compound_assignment,
			/* Compound assignment, e.g., a.p += 1. */
  rprk_pre_incr_decr,	/* Pre-increment or -decrement, e.g., ++a.p. */
  rprk_post_incr_decr,	/* Post-increment or -decrement, e.g., a.p++. */
  rprk_comma_discard_first,
			/* Comma node used as part of a rewrite.  The first
			   operand is generated (i.e., not part of the
			   source). */
  rprk_comma_discard_second
			/* Comma node used as part of a rewrite.  The second
			   operand is generated (i.e., not part of the
			   source). */
};

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if DO_IL_LOWERING && IA64_ABI

typedef struct a_vcall_offset_entry *a_vcall_offset_entry_ptr;
typedef struct a_vcall_offset_entry {
  /* An entry associating a virtual function with a "virtual call offset".
     Used in IL lowering as a work structure while developing the
     virtual function tables. */
  a_vcall_offset_entry_ptr
		next;	/* Next in a linked list of virtual call offset
			   entries. */
  a_routine_ptr	routine;   
			/* A pointer to the routine, which will always have
			   the is_virtual flag set. */
  a_base_class_ptr
		base_class;
			/* The base class from which this routine comes, or
			   NULL if this routine comes from the most derived
			   class. */
  a_virtual_table_index
		vcall_offset_index;
			/* The index into the virtual table where the virtual
			   call offset will be located.	 This entry gives the
			   offset from the virtual base to the overriding
			   class. */
  a_byte_boolean
		is_primary;
			/* True if this vcall offset entry will appear in a
			   vtable for a primary base of the most derived
			   class, rather than in the vtable of the most
			   derived class. */
} a_vcall_offset_entry;

#endif /* DO_IL_LOWERING && IA64_ABI */

#if MICROSOFT_EXTENSIONS_ALLOWED

/*
Entry describing a single C++/CX partial class body.
*/
typedef struct a_partial_class_body  *a_partial_class_body_ptr;
typedef struct a_partial_class_body {
  a_partial_class_body_ptr
		next;
			/* Pointer to the next partial body in this linked
			   list. */
  a_source_position
		start_position;
			/* The start position of this partial class body
			   (either the ':' or '{' token). */
  a_source_position
		end_position;
			/* The end position of this partial class body
			   (the '}' token). */
  struct a_token_cache
		*body_cache;
			/* The tokens comprising the body of this
			   partial declaration (between the '{' and '}'
			   tokens). */
  struct a_token_cache
		*base_cache;
			/* The tokens comprising the base list of this
			   partial declaration (between the ':' and '{'
			   tokens).  NULL if a base list is not present. */
} a_partial_class_body;


/* Type of a set of Microsoft attribute targets. */
typedef unsigned int an_ms_attribute_target;

/*
Values that identify a target for a Microsoft attribute.
*/
#define msat_invalid          ((an_ms_attribute_target)0x00000000)
			/* Not a valid target. */
#define msat_none             ((an_ms_attribute_target)0x00000001)
			/* A target was not or cannot be explicitly
			   specified. */
#define msat_assembly         ((an_ms_attribute_target)0x00000002)
			/* Applies to an assembly as a whole. */
#define msat_module           ((an_ms_attribute_target)0x00000004)
			/* Applies to a module as a whole. */
#define msat_class            ((an_ms_attribute_target)0x00000008)
			/* Applies to a class. */
#define msat_struct           ((an_ms_attribute_target)0x00000010)
			/* Applies to a struct. */
#define msat_union            ((an_ms_attribute_target)0x00000020)
			/* Applies to a union. */
#define msat_enum             ((an_ms_attribute_target)0x00000040)
			/* Applies to an enum. */
#define msat_constructor      ((an_ms_attribute_target)0x00000080)
			/* Applies to a constructor. */
#define msat_method           ((an_ms_attribute_target)0x00000100)
			/* Applies to a member function.  When specified on an
			   attribute of a property, applies to the accessor
			   functions. */
#define msat_property         ((an_ms_attribute_target)0x00000200)
			/* Applies to a property. */
#define msat_field            ((an_ms_attribute_target)0x00000400)
			/* Applies to a field. */
#define msat_event            ((an_ms_attribute_target)0x00000800)
			/* Applies to an event. */
#define msat_interface        ((an_ms_attribute_target)0x00001000)
			/* Applies to an interface. */
#define msat_parameter        ((an_ms_attribute_target)0x00002000)
			/* Applies to a parameter. */
#define msat_delegate         ((an_ms_attribute_target)0x00004000)
			/* Applies to a delegate. */
#define msat_returnvalue      ((an_ms_attribute_target)0x00008000)
			/* Applies to a method's return value, not the
			   method. */
#define msat_genericparameter ((an_ms_attribute_target)0x00010000)
			/* Applies to a generic parameter. */
#define msat_typedef          ((an_ms_attribute_target)0x00020000)
			/* Applies to a typedef. */
#define msat_variable         ((an_ms_attribute_target)0x00040000)
			/* Applies to a variable. */
#define msat_routine          ((an_ms_attribute_target)0x00080000)
			/* Applies to a nonmember function. */
#define msat_interfaceimpl    ((an_ms_attribute_target)0x00100000)
			/* Applies to an implementation of an interface. */
#define msat_any              ((an_ms_attribute_target)0x001FFFFF)
			/* Applies to any target. */

/* Entry describing additional information for custom Microsoft attributes. */
typedef struct an_ms_attribute_usage *an_ms_attribute_usage_ptr;
typedef struct an_ms_attribute_usage {
  an_ms_attribute_target
		valid_on;
			/* Identifies the kinds of entities to which the
			   attribute may apply. */
  a_bit_field	allow_multiple:1;
			/* TRUE if the attribute can be applied to the same
			   entity multiple times. */
  a_bit_field	inherited:1;
			/* TRUE if the attribute can be inherited by derived
			   classes and overriding members. */
} an_ms_attribute_usage;

/* Entry describing an "__event __interface". */
typedef struct an_event_interface *an_event_interface_ptr;
typedef struct an_event_interface {
  an_event_interface_ptr
                next;   /* A pointer to the next entry. */
  a_type_ptr    interface_type;
                        /* A pointer to the type of the interface. */
  a_source_position
                pos;    /* Position of the "__event" keyword. */
} an_event_interface;

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* Entry containing additional information about a class type (tk_class,
   tk_struct, or tk_union).  The list of nonstatic data members (i.e.,
   "fields") is kept in the type entry. */
typedef struct a_class_type_supplement *a_class_type_supplement_ptr;
typedef struct a_class_type_supplement {
  a_base_class_ptr
                base_classes;
                        /* A linked list of entries describing all the base
                           classes, both directly and indirectly inherited,
			   that are included within this class. */
  a_base_class_ptr
                direct_base_classes;
                        /* A linked list of the direct base classes in
                           declaration order. */
#if IA64_ABI
  a_base_class_ptr
		preorder_base_classes;
			/* A linked list with the same entries as are on the
			   base_classes list, but as found in a preorder
			   traversal of the inheritance hierarchy, rather than
			   a postorder traversal. */
  a_base_class_ptr
		primary_base_class;
			/* The primary base class for this class, i.e., the
			   most derived class with which this class shares
			   virtual function info.  NULL if none.  Differs from
			   virtual_function_info_base_class in that the latter
			   points to the least-derived class that shares
			   virtual function info. */
#endif /* IA64_ABI */
  a_targ_size_t size_without_virtual_base_classes;
                        /* The size in bytes of the class, excluding the
                           virtual base classes from which it derives. */
  a_targ_alignment
		alignment_without_virtual_base_classes;
                        /* The alignment required for this class, when the
                           virtual base classes from which it derives are
                           omitted. */
  a_virtual_function_number
		highest_virtual_function_number;
			/* The highest virtual function number assigned to
			   any virtual member function actually declared in
			   (not just inherited by) the current class.  However,
			   if virtual_function_info_base_class is non-NULL,
			   it may be that no virtual function declared in this
			   class actually was assigned the number; in that
			   case it is the highest virtual function number of
			   the shared virtual functions, including those
			   declared in the base class.  It follows that a
			   class could have *no* directly declared virtual
			   functions yet have a value other than
			   VIRTUAL_FUNCTION_NUMBER_NONE in this field.
			   (Incidentally, this number also corresponds to
			   the size of a virtual function table.) */
#if DO_IL_LOWERING && IA64_ABI
  a_virtual_table_index
		next_negative_virtual_table_index;
			/* The largest unused virtual table index in the
			   backwards-growing part of the virtual table.	 If
			   there is no virtual table, this field will still
			   have a negative value, but that value is unused. */
  a_virtual_table_index
		first_vcall_offset_index;
			/* The (negative) index to the first vcall offset in
			   this class's virtual table.	Zero until set. */
  a_vcall_offset_entry_ptr
		vcall_offsets;
			/* The association between virtual routines in this
			   class and its direct and indirect non-virtual
			   bases.  When this class is used as a virtual base,
			   the offset from the virtual base to the subobject
			   containing the overrider can be found at the
			   location indicated on this list. */
#endif /* DO_IL_LOWERING && IA64_ABI */
  a_targ_size_t	virtual_function_info_offset;
			/* The offset within the class object to a field
			   containing information about the virtual functions
			   declared for this class.  (Typically this would
			   be the offset to a pointer to a virtual function
			   table.)  If virtual_function_info_base_class is
			   non-NULL, this is still the offset within the
			   current class.  If any_virtual_functions in the
			   associated type entry is FALSE, this field is
			   undefined. */
  a_base_class_ptr
		virtual_function_info_base_class;
			/* If virtual function info is shared between the
			   current class and one of its base classes (e.g.,
			   if the current class uses the virtual function
			   table pointer of its base class), this is the
			   base class involved in the sharing.  It is not
			   necessarily a direct base class.  This field is
			   NULL if the virtual function info is not shared. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_const_char	*uuid_string;
			/* Pointer to a character string representing the
			   argument of a uuid decl-modifier. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DECL_MODIFIERS_IN_USE
  a_decl_modifier_set
		decl_modifiers;
			/* Additional declaration information representing
			   Microsoft-style __declspec modifiers that are
			   applied to the class as a whole.  Also used for
			   Sun-style link scope specifiers. */
#endif /* DECL_MODIFIERS_IN_USE */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_type_kind	orig_type_kind;
			/* Type kind indicating the tag used when this type
			   was first declared in the current translation unit.
			   It may differ from the type kind in the type entry,
			   which always reflects the tag used in the defining
			   declaration.  This information is used for
			   Microsoft-style name mangling. */
  an_inheritance_kind
		inheritance_kind;
			/* Inheritance kind (single, multiple, virtual) that
			   was specified for the class (either explicitly or
			   as a result of a pointer-to-member declaration that
			   involves the class).  May be used to control the
			   implementation of pointer-to-member objects for
			   Microsoft ABI compatibility.  An inheritance kind
			   of ihk_none means no specific inheritance kind
			   has been set. */
  a_bit_field	inheritance_kind_is_explicit:1;
			/* TRUE if the inheritance_kind field was set as the
			   result of an explicit specification on the class
			   declaration. */
  a_bit_field	has_direct_property_or_event:1;
			/* TRUE if this class contains a direct (i.e., not
			   inherited) C++/CLI property or event. */
  ENUM_TYPE_FOR_BIT_FIELD(an_assembly_visibility)
		declared_assembly_visibility:2;
			/* Visibility of this type at the assembly level as
			   explicitly declared in the source (av_none if no
			   visibility was explicitly specified).  
			   (C++/CLI only.) */
  ENUM_TYPE_FOR_BIT_FIELD(an_assembly_visibility)
		assembly_visibility:2;
			/* Effective visibility of this type at the assembly
			   level.  (C++/CLI only.) */
  ENUM_TYPE_FOR_BIT_FIELD(a_cli_class_type_kind)
		cli_class_type_kind:3;
			/* The class type kind of this class.  In non-C++/CLI
			   modes, it is always cctk_standard.  In C++/CLI mode,
			   other kinds of classes (e.g., "ref classes") are
			   possible: See a_cli_class_type_kind. */
  a_bit_field	is_hide_by_sig:1;
			/* TRUE if lookup in this class should follow the
			   C++/CLI "hidebysig" rules (which is normally the
			   case for managed class types). */
  a_bit_field	is_cli_array:1;
			/* TRUE if this represents a C++/CLI array type. */
  a_bit_field	is_cli_attribute:1;
			/* TRUE if this represents a C++/CLI attribute type. */
  a_bit_field	is_cppcx_write_only_array:1;
			/* TRUE if this represents a C++/CX write-only array
			   type. */
  a_bit_field	is_cppcx_box:1;
			/* TRUE if this represents a C++/CX Platform::Box<T>
			   type. */
  a_bit_field	is_partial:1;
			/* TRUE if this is a partial class. */
  a_bit_field	has_coclass_attribute:1;
			/* TRUE if the "coclass" Microsoft attribute has been
			   applied to this class. */
  a_bit_field	has_explicitly_aligned_subobject:1;
			/* TRUE if one of the (possibly indirect) subobjects
			   has a type that is explicitly aligned.  This affects
			   class layout in Microsoft mode. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  ENUM_TYPE_FOR_BIT_FIELD(an_ELF_visibility_kind)
		ELF_visibility:3;
			/* The visibility of the class members in the generated
			   ELF object code. */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  a_bit_field	qualifiers:NUM_BITS_FOR_TYPE_QUALIFIER_SET;
			/* Qualifiers that apply to the class as a whole,
			   as in "class __far A {}". */
#endif /* NEAR_AND_FAR_ALLOWED */
#if BACK_END_IS_CP_GEN_BE
  ENUM_TYPE_FOR_BIT_FIELD(a_name_linkage_kind)
		surrounding_name_linkage_state:NUM_BITS_FOR_NAME_LINKAGE;
			/* Name linkage in effect when this class was defined.
			   Used by the C++-generating back end to reconstruct
			   name linkage blocks when appropriate. */
#endif /* BACK_END_IS_CP_GEN_BE */
#if DO_IL_LOWERING
  a_bit_field  compiler_generated:1;
			/* TRUE if this class is compiler-generated.
			   Specifically, this is TRUE for the "types
			   as subobjects" generated during IL lowering. */
  a_bit_field  has_subobject_type:1;
			/* TRUE if this class has been pre-lowered and a
			   subobject type has been generated for this type
			   (for cases where the type is used as a subobject).
			   The subobject type is given by subobject_partner. */
#endif /* DO_IL_LOWERING */
#if RECORD_HIDDEN_NAMES_IN_IL
  a_bit_field	hidden_names_processed:1;
			/* Used only in the front end: TRUE if the names in
			   this class have been examined for hiding and should
			   not be processed again.  This is needed because
			   classes are traversed during hidden name processing
			   both in inheritance order and while processing the
			   namespaces in which they are defined. */
  a_bit_field	base_class_hiding_in_progress:1;
			/* Used only in the front end: TRUE if this class
			   is currently being processed for hiding by
			   inherited names.  This is needed to prevent
			   infinite recursion while processing class
			   templates that are based on each other
			   (presumably the recursion is limited during
			   instantiation by an explicit specialization). */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  a_bit_field	is_lambda_closure_class:1;
			/* TRUE if the class is the closure class generated as
			   the representation of a lambda. */
  a_bit_field	is_generic_lambda_closure_class:1;
			/* TRUE if the class is the closure class generated as
			   the representation of a generic lambda. */
  a_bit_field	has_lambda_conversion_function:1;
			/* TRUE for a closure class for which a lambda
			   conversion function exists. */
  a_bit_field	is_initializer_list:1;
			/* TRUE if the class is an instance of the C++11
			   template std::initializer_list. */
  a_bit_field	has_initializer_list_ctor:1;
			/* TRUE if the class has an initializer_list
			   constructor. */
  a_bit_field	has_anonymous_union_member:1;
			/* TRUE if the class contains an anonymous union
			   member (which makes it a union-like class in C++11
			   parlance). */
  a_bit_field	defined_in_variable_initializer:1;
			/* TRUE if the class is a closure class defined
			   directly in the initializer for a static data member
			   or variable (closure classes nested in such closure
			   classes do not necessarily have this flag set to
			   TRUE). */
  a_bit_field	defined_in_field_initializer:1;
			/* TRUE if the class is a closure class defined
			   directly in the initializer for a field (closure
			   classes nested in such closure classes do not
			   necessarily have this flag set to TRUE). */
  a_bit_field	named_in_inline_template_directive:1;
			/* TRUE if the class was named in a GNU
			   "inline template" directive, which is used to
			   cause a vtable to be emitted in a given translation
			   unit. */
  a_bit_field   is_va_list_tag:1;
			/* TRUE if this class is the __va_list_tag or __va_list
			   class used to implement __builtin_va_list on some
			   systems.  This class is given special treatment
			   during name lookup (where it is exempt from
			   argument-dependent name lookup) and in the
			   C-generating back end. */
  a_bit_field	defined_in_parent_class:1;
			/* TRUE for nested classes defined in their parent
			   class. */
  a_bit_field	has_nodiscard_attribute:1;
			/* TRUE if the class has the "nodiscard" standard
			   attribute applied to it. */
  a_bit_field	has_field_initializer:1;
			/* TRUE if the associated class type has a field with
			   a default member initializer. */
  a_bit_field	removed_from_il:1;
			/* TRUE if the associated class type entry has been
			   removed from the IL because it was unneeded. */
  a_bit_field	contains_error:1;
			/* TRUE if the class contains an error type.  If FALSE
			   and contains_error_cached is TRUE, then the class is
			   known not to contain an error type. */
  a_bit_field	contains_error_cached:1;
			/* TRUE if the value of contains_error is fully
			   determined. */
  a_bit_field	contains_local_type:1;
			/* TRUE if the class contains a local type.  If FALSE
			   and contains_local_type_cached is TRUE, then the
			   class is known not to contain a local type. */
  a_bit_field	contains_local_type_cached:1;
			/* TRUE if the value of contains_local_type is fully
			   determined. */
  a_bit_field	contains_unnamed_namespace_type:1;
			/* TRUE if the class contains an unnamed namespace
			   type.  If FALSE and
			   contains_unnamed_namespace_type_cached is TRUE, then
			   the class is known not to contain an unnamed
			   namespace type. */
  a_bit_field	contains_unnamed_namespace_type_cached:1;
			/* TRUE if the value of contains_unnamed_namespace_type
			   is fully determined. */
  a_bit_field	does_not_contain_parentless_lambda_in_default_argument:1;
			/* TRUE if the class is known not to contain a lambda
			   type that is defined in a default argument and does
			   not have its parent pointer set. */
  a_bit_field	does_not_contain_deprecated_or_unavailable_type:1;
			/* TRUE if the class is known not to contain a
			   deprecated or unavailable type. */
  an_anonymous_union_kind
		anonymous_union_kind;
			/* Indication of whether this class is an anonymous
			   union, and if so whether it is a field of some
			   other class or a variable. */
  a_field_ptr	anonymous_union_field;
			/* If anonymous_union_kind == auk_field, pointer to
			   the unnamed field entry whose type is the anonymous
			   union (in some modes possibly cv-qualified);
			   otherwise NULL. */
  a_class_list_entry_ptr
                befriending_classes;
                        /* A linked list of entries identifying classes that
                           have declared the current class a friend (i.e.,
                           classes that have "befriended" the current class).
                           Note that the representation is backwards
                           compared to the source language: in the source
                           the befriended class (or routine) is declared in
                           the befriending class; here the befriending class
                           is recorded in the befriended class (or routine). */
  an_il_entity_list_entry_ptr
		friends;
			/* A linked list of entries identifying entities that
			   were explicitly declared as friends of the current
			   class (i.e., routines, classes, and templates that
			   the current class has befriended). */
#if MAINTAIN_CLASS_MEMBER_LIST
  an_il_entity_list_entry_ptr
		member_declarations;
			/* A linked list of entries identifying the
			   declarations that appeared in the body of the
			   current class, in the order in which they appeared.
			   NULL if the class has not been defined (or if its
			   body is empty).  An entry can refer to a field, a
			   routine, a variable (i.e., a static data member), a
			   type, a template, a using-declaration, or a static
			   assertion.  A friend declaration appearing in the
			   body is included even though the entity it declares
			   is not a member of the class.  Compiler-generated
			   members (e.g., an implicitly-declared constructor)
			   are not included.  An entity declared more than
			   once in the body (e.g., a nested class that is
			   declared and later defined) appears multiple times
			   in the list. */
#endif /* MAINTAIN_CLASS_MEMBER_LIST */
  a_scope_ptr	assoc_scope;
			/* The scope for the class type.  In the scope entry,
			   "routines" gives a linked list of routine entries
			   representing the member functions of the class.
			   All member functions (static and nonstatic, virtual
			   and nonvirtual, inline and ordinary) are included
 			   in this list.  However, friend functions are
			   not included.  "variables" gives a linked list of
			   variable entries representing the static data
			   members of the class (though these members may be
			   moved to the file scope during lowering).  "types"
			   gives a linked list of type entries representing
			   local types defined within the scope of the class,
			   including nested classes.  This pointer is NULL when
			   the class has been declared but not defined. */
  a_template_ptr
		assoc_template;
			/* For instantiated entities, this points to the
			   template from which they were generated; otherwise,
			   this is NULL. */
  a_template_arg_ptr
		template_arg_list;
			/* For classes that are instantiations of a class
			   template, a list of entries describing the "actual
			   arguments" on which the instantiation is based.
			   If the class is an instantiation of a partial
			   specialization, this argument list corresponds with
			   the template parameter list of the primary template.
			   This pointer is NULL for ordinary classes that are
			   not generated from a template. */
  a_template_arg_ptr
		partial_spec_template_arg_list;
			/* For classes that are instantiations of partial
			   specializations of a class template, a list of
			   entries describing the arguments on which the
			   instantiation is based, with respect to the
			   template parameter list of the partial
			   specialization.  This is NULL for ordinary classes
			   and for classes generated from the primary template
			   (i.e., not from a partial specialization). */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_partial_class_body_ptr
		partial_class_bodies;
			/* if is_partial is TRUE, a list of partial class
			   bodies that comprise this partial class type. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  a_routine_ptr	assoc_operator_new_routine;
			/* The operator new() routine to be used for the class.
			   NULL until a new is done or a constructor is
			   defined.  Needed only if the constructor sometimes
			   does the new. */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
  a_routine_ptr	assoc_operator_delete_routine;
			/* The operator delete() routine to be used for the
			   class.  NULL until a delete is done or a destructor
			   is defined.  Needed only if the destructor sometimes
			   does the delete. */
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if DO_IL_LOWERING
  a_variable_ptr
		virtual_function_table_var;
			/* When IL lowering is done, this points to the
			   variable that contains the virtual function table
			   for this class when it is the most derived class.
			   NULL until allocated and NULL if not needed. */
#if IA64_ABI
  a_variable_ptr
		virtual_table_table_var;
			/* When IL lowering is done, this points to the
			   variable that contains the virtual table table
			   for this class.  NULL until allocated and NULL if
			   not needed. */
#endif /* IA64_ABI */
  a_type_ptr	subobject_partner;
			/* NULL until the type has been pre-lowered.  In cases
			   where a separate subobject type is needed, this
			   points to that type (and has_subobject_type is set
			   to TRUE).  If a separate subobject type is not
			   needed, this points to the class type itself.  Also,
			   for compiler-generated subobject types themselves,
			   points back to the original class type. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_variable_ptr
		uuid_variable;
			/* When IL lowering is done and field uuid_string is
			   non-NULL, this points to a variable of type _GUID
			   that is initialized to reflect the value of the
			   string. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
  a_type_ptr	promoted_local_types;
			/* List of types local to member functions promoted
			   out of those member functions.  Eventually, these
			   will be promoted into the file scope, but they're
			   moved here first to keep them grouped with the
			   other types from this class. */
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  a_construction_vtbl_ptr
		construction_vtbls;
			/* If non-NULL, this class contains overridden virtual 
			   functions in virtual base classes and requires
			   special versions of virtual function tables when
			   a constructor or destructor is called for a
			   subobject.  This points to the first on a list of
			   entries describing (in order) the elements of
			   an array of vtbl addresses to be used when
			   constructing or destroying a complete object
			   of this class type. */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#endif /* DO_IL_LOWERING */
  int32_t	min_template_arguments;
			/* The minimum number of template arguments used to
			   refer to this instance in the source (using
			   default arguments); -1 for non-template classes
			   and for template classes in which all template
			   arguments were always explicitly specified. */
  union {
    /* When defined_in_variable_initializer and
       defined_in_field_initializer are both FALSE: */
    a_routine_ptr
		routine;
			/* If this entry is for a closure type defined directly
			   in a default argument, this points to the entry
			   for the routine that has that default argument. */
    /* When defined_in_variable_initializer is TRUE: */
    a_variable_ptr
		variable;
			/* If this entry is for a closure type defined directly
			   in the initializer of a static data member or
			   variable (i.e., defined_in_variable_initializer is
			   TRUE), this points to the entry representing that
			   variable. */
    /* When defined_in_field_initializer is TRUE: */
    a_field_ptr
		field;
			/* If this entry is for a closure type defined directly
			   in the initializer of a field, this points to that
			   field. */
  } lambda_parent;
  a_hash_value
		hash_value;
			/* A hash value computed for this class type, or
			   zero if a hash has not yet been computed. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_type_ptr
		corresponding_basic_type;
			/* If this class is a fundamental C++/CLI type, the
			   corresponding basic C++ type (otherwise NULL). */
  an_assembly_scope_index
		assembly_scope_index;
			/* The index of the assembly and scope in which this
			   construct was defined, or zero if the construct is
			   not from an assembly. */
  a_cpp_cli_token
		metadata_type_def_token;
			/* If this construct was defined in an assembly, the
			   typedef-token for this construct within the
			   assembly in which it was defined. */
  a_routine_ptr base_dispose_bool_routine;
			/* If this is a C++/CLI ref class that is extending a
			   base class dispose pattern, this is the base class
			   Dispose(bool) routine (which this class'
			   Dispose(bool) member should invoke); NULL
			   otherwise. */
  a_routine_ptr base_idisposable_dispose_routine;
  a_routine_ptr base_object_finalize_routine;
			/* If this is a C++/CLI ref class that is introducing
			   the dispose pattern, these are the base class
			   Dispose() and Finalize() routines.  NULL if this
			   class doesn't introduce the dispose pattern or if
			   the corresponding functions don't exist.  The
			   Dispose(bool) implementation in this class invokes
			   these routines. */
  a_type_ptr	invocation_type;
			/* If this entry is for a delegate class, the function
			   type with which the delegate was declared.  This is
			   a type that doesn't include a "this" parameter.
			   NULL if this entry isn't for a delegate class. */
  an_event_interface_ptr
		event_interfaces;
			/* A pointer to all "__event __interface" types
			   in this class.  NULL if there are none. */
  an_ms_attribute_usage
		attribute_usage;
			/* If is_cli_attribute is TRUE, identifies the kinds
			   of entities to which this attribute may apply.
			   If the valid_on member is msat_invalid, it has yet
			   to be initialized by
			   set_attribute_usage_for_attribute_type. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_type_ptr
		proxy_of_type;
			/* For nonreal classes that directly represent a
			   proxy class, this points back to the template
			   parameter or decltype type for which the proxy
			   class was created; otherwise, NULL. */
} a_class_type_supplement;

enum a_template_param_type_kind : a_byte {
  /* When a type is marked as a template parameter it may have one of several
     kinds (C++ front end only). */
  tptk_param,		/* The template param type represents a simple
			   template parameter, e.g., for T in the following:
			     template <class T> class A {
                               T x;
                             };
			   This is the most common and obvious case. */
  tptk_member,		/* The template param type represents a member of
			   a tk_template_param class, e.g., for T::X in the
			   following:
			     template <class T> class A {
			       typename T::X x;
			     };
			   (where, during prototype instantiation, X is
			   assumed to be a member of T and a type). */
  tptk_unknown,		/* The template param type represents the unknown
			   type of a non-type member of a template parameter
			   class, e.g., the type of T::k, and the type of
                           the constant "1" in the following:
			     template <class T> class A {
			       int a[T::k];
			       typename T::X<1> b;
			     };
			   (where, during prototype instantiation, k is
			   assumed to be a member of T and a constant).

			   Also used as the type of an expression in a
			   prototype instantiation context that involves
			   template parameter values, and whose real type
			   cannot be known. */
  tptk_bit_precise_int	/* The template param type represents a dependent
			   _BitInt type, whose width is represented by
			   constraint.bit_width_constant. */
};


/*
Entry containing additional information about a template parameter type
(tk_template_param).
*/
typedef struct a_template_param_type_supplement
                                         *a_template_param_type_supplement_ptr;
typedef struct a_template_param_type_supplement {
  a_type_ptr	class_type;
			/* The "proxy" class type associated with a given
                           template parameter.  This becomes useful in name
			   lookup if a template parameter is used in a way
			   requiring it to be a class with members --
			   e.g.,
			     template <class T> void f(T::X);
			   Here we know T must represent a class type
			   with a member type X, and the X can be
			   entered in a scope associated with T.  In such
			   contexts the class pointed to by class_type is
			   used in place of the type that points to the
			   template parameter.  The class-qualified
			   lookup is done using class_type.  The first
			   time a name is looked up in class_type it
			   will be entered as a member that can be
			   found by subsequent lookups.  The pointer is
			   NULL if no class use has been encountered.  A
			   proxy class is also created for C++/CLI
			   generics to represent the type specified by
			   the constraints. */
  a_type_ptr	orig_nested_type;
			/* If this is a template parameter created to
			   represent the corresponding nonreal type for
			   a nested type of a class template, this points
			   to the original nested type;  NULL otherwise. */
  struct a_symbol
		*template_symbol;
			/* For templates (including C++/CLI generics) points
			   to the sk_class_template or sk_function_template
			   symbol of which this is a type parameter.  NULL
			   otherwise.  Used in the front end only; cannot
			   be used in back ends. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_generic_constraint_ptr
		generic_constraints;
			/* For C++/CLI generics, this points to the list of
			   constraints specified, and can be NULL. */
  a_generic_param_seq_number
		generic_param_seq_number;
			/* If this entry represents a parameter of a generic
			   class, this is the "sequence number" (see the
			   definition of a_generic_param_seq_number) of that
			   parameter.  Otherwise, zero. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_template_param_coordinate
		coordinates;
			/* The parameter list position and template nesting
			   depth of the parameter. */
  union {
    /* When coordinates.depth != CLASS_TEMPLATE_PLACEHOLDER_NESTING_DEPTH and
       coordinates.depth != BIT_PRECISE_INT_NESTING_DEPTH: */
    an_expr_node_ptr
		type_constraint;
			/* For a template parameter declared with a type
			   constraint, this points to the enk_concept_id
			   node representing that constraint. */
    /* When coordinates.depth == CLASS_TEMPLATE_PLACEHOLDER_NESTING_DEPTH: */
    struct a_symbol
		*class_template_symbol;
			/* For a tptk_param type used to represent a
			   placeholder for C++17 class template argument
			   deduction, this points to the class template
			   that was specified.  NULL otherwise.  Used in
			   the front end only; cannot be used in back ends. */
    /* When coordinates.depth == BIT_PRECISE_INT_NESTING_DEPTH: */
    a_constant_ptr
		bit_width_constant;
			/* For a tptk_bit_precise_int type, the constant
			   specifying the _BitInt width. */
  } constraint;
} a_template_param_type_supplement;


/* Type used for the internal representation of UPC block sizes. */
typedef long a_upc_block_size;
#define UPC_BLOCK_SIZE_NONE ((EDG_QUAL a_upc_block_size)(-1))

#if UPC_EXTENSIONS_ALLOWED

/* Coded values for UPC block size specifications. */
#define UPC_BLOCK_SIZE_INDEFINITE ((a_upc_block_size)(0))
#define UPC_BLOCK_SIZE_BLOCK ((a_upc_block_size)(-2))

#endif /* UPC_EXTENSIONS_ALLOWED */


/*
Entry containing additional information about a typeref type.
*/
typedef struct a_typeref_type_supplement *a_typeref_type_supplement_ptr;
typedef struct a_typeref_type_supplement {
  a_template_arg_ptr
		template_arg_list;
			/* For types that are instantiations of a template
			   alias, this points to the template argument list
			   on which the instantiation is based.  NULL for
			   ordinary types that are not generated from an
			   alias template. */
  a_template_arg_ptr
		orig_template_arg_list;
			/* For types that are instantiations of a template
			   alias, this points to the template argument list
			   before any nonreal typerefs have been removed.
			   This argument list is used for substitution as
			   a substitution failure in one of the arguments
			   should result in a substitution failure on the
			   type, even if the argument is not used in the
			   eventual type. */
#if DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
  a_name_qualifier_ptr
		name_qualifier;
			/* For types written with a nested name specifier
			   (i.e., the trk_name_qualifier kind), this points to
			   the name qualifier.  It is NULL for a global
			   namespace qualifier (in which case
			   is_global_qualified_name is also TRUE). */
#endif /* DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */
  a_template_ptr
		assoc_template;
			/* For instantiated entities, this points to the
			   template from which they were generated; otherwise,
			   this is NULL. */
  an_expr_node_ptr
		expr;	/* The expression argument for a decltype or typeof
			   construct (i.e., the trk_is_decltype or
			   trk_is_typeof_with_expression kinds) and for the
			   constraint associated with a deduced return type
			   (i.e., when the kind is trk_is_deduced_auto or
			   trk_is_deduced_decltype_auto).  It is NULL for
			   kinds for which the operand is a type.  It is
			   also NULL when the expression is local to a
			   function, in which case the expression must be
			   retrieved using find_local_expr_node.  The
			   function decltype_arg can be used to fetch the
			   expression (if there is one) in all cases. */
#if UPC_EXTENSIONS_ALLOWED
  a_upc_block_size
		upc_block_size;
			/* Block size for UPC shared data types. */
#endif /* UPC_EXTENSIONS_ALLOWED */
  a_type_ptr	proxy_class;
			/* If this is the typeref for a dependent decltype
			   this points to the corresponding "proxy" class.
                           This is needed if the decltype is used in a
			   context in which a class type is required
			   (e.g., decltype(...)::X).  This is also used for
			   a typedef that refers to a dependent decltype.
			   NULL for other kinds of typerefs or if no class
			   use has been encountered. */
  a_type_ptr	operator_type_arg;
			/* The type that originally appeared as the argument
			   to the operator. */
  int32_t	min_template_arguments;
			/* The minimum number of template arguments used to
			   refer to this instance in the source (using
			   default arguments); -1 for non-template types
			   and for template aliases in which all template
			   arguments were always explicitly specified. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		type_id_range;
			/* For typeref entries for aliases, the source range
			   of the type to which an alias refers. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} a_typeref_type_supplement;


/*
Entry containing additional information about an integral type.
*/
typedef struct an_integer_type_supplement *an_integer_type_supplement_ptr;
typedef struct an_integer_type_supplement {
  a_bit_field	enumerator_list_seen:1;
			/* TRUE for enumeration types whose enumerator list has
			   been seen. */
  a_bit_field	enumerator_list_complete:1;
			/* TRUE for enumeration types whose enumerator list has
			   been seen in its entirety, i.e., once the closing
			   brace of the enum-specifier has been processed.
			   Unlike the "incomplete" flag of the type, this
			   remains FALSE while the enumerators are being
			   scanned and for an opaque enumeration declaration,
			   both of which produce a complete type when the
			   underlying type is fixed. */
  a_bit_field	has_nodiscard_attribute:1;
			/* TRUE for an enumeration type has the "nodiscard"
			   standard attribute applied to it. */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	underlying_type_should_use_unsigned:1;
			/* TRUE in GNU C++ mode if the type trait helper
			   __underlying_type should produce the unsigned
			   counterpart of the actual underlying type. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ENUM_TYPE_FOR_BIT_FIELD(an_assembly_visibility)
		declared_assembly_visibility:2;
			/* Visibility of this type at the assembly level as
			   explicitly declared in the source (av_none if no
			   visibility was explicitly specified).  (Enumeration
			   types in C++/CLI mode only.) */
  ENUM_TYPE_FOR_BIT_FIELD(an_assembly_visibility)
		assembly_visibility:2;
			/* Effective visibility of this type at the assembly
			   level  (Enumeration types in C++/CLI mode only.) */
  an_assembly_scope_index
		assembly_scope_index;
			/* The index of the assembly and scope in which this
			   construct was defined, or zero if the construct is
			   not from an assembly. */
  a_cpp_cli_token
		metadata_type_def_token;
			/* If this construct was defined in an assembly, the
			   typedef-token for this construct within the
			   assembly in which it was defined. */
  char
		*uuid_string;
			/* Pointer to a character string representing the
			   argument of a uuid decl-modifier (enums only). */
  a_type_ptr	boxed_type;
			/* For C++/CLI enumeration types, the corresponding
			   ref class type representing the boxed version of
			   the enumeration type. */
#if DO_IL_LOWERING
  a_variable_ptr
		uuid_variable;
			/* When IL lowering is done and field uuid_string is
			   non-NULL, this points to a variable of type _GUID
			   that is initialized to reflect the value of the
			   string. */
#endif /* DO_IL_LOWERING */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* When variant.integer.enum_type is TRUE: */
  a_type_ptr
		base_type;
			/* For enumeration types, the type explicitly set as
			   the underlying type (if any).  Otherwise NULL.
			   If non-NULL, has_explicit_enum_base will be TRUE.
			   (In C++/CLI mode this can be a value class type
			   that maps on an integral type.) */
  /* When variant.integer.int_kind is ik_bit_precise or
     ik_unsigned_bit_precise: */
  a_targ_size_t
		bit_width;
			/* The declared width of a bit-precise integer type. */
  a_source_position
		base_type_position;
			/* If base_type is non-NULL, the source position at
			   which the underlying type was explicitly
			   specified. */
  a_template_ptr
		assoc_template;
			/* For member opaque enumerations of class templates
			   this points to the template from which they
			   were generated; otherwise, this is NULL. */
} an_integer_type_supplement;


/*
Entry pointed to by the based_types field of a_type entries.  A list
of these entries gives pointers to types based on the type entry, e.g.,
pointer-to type entry.
*/
enum a_based_type_kind : a_byte {
  /* Indication of the relationship between the based type and the base
     type. */
  btk_qualified,	/* A (const, volatile, const-volatile, etc.) qualified
			   version of the type. */
  btk_rvalue_reference,	/* Rvalue reference to the type. */
  btk_reference,	/* Ordinary ("lvalue") reference to the type. */
  btk_ptr_to_member,	/* Pointer to member type (C++ only). */
  btk_unqualified_array_type,
			/* Means that the "based type" is an array type to
			   which a qualifier was applied to produce the
			   "base type".  The qualifier went to the element
			   type, so a new array type (= the base type)
			   resulted.  (For example, qualifying (int)[3] with
			   const creates (const int)[3], and the original is
			   recorded as a based type of the new type.) */
#if MICROSOFT_EXTENSIONS_ALLOWED
  btk_handle,		/* C++/CLI handle. */
  btk_tracking_ref,	/* C++/CLI tracking reference. */
  btk_interior_ptr,	/* C++/CLI interior_ptr. */
  btk_pin_ptr,		/* C++/CLI pin_ptr. */
  btk_cppcx_box,	/* C++/CX boxed value type; Platform::Box<T>. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  btk_pointer,		/* Pointer to the type. */
  btk_no_noexcept_exception_spec
			/* A function type without its noexcept exception spec
			   (C++ only). */
};


typedef struct a_based_type_list_member *a_based_type_list_member_ptr;
typedef struct a_based_type_list_member {
  a_based_type_list_member_ptr
		next;	/* Next entry on the list, or NULL if last. */
  a_type_ptr	based_type;
			/* The based type. */
  a_based_type_kind
		kind;	/* The relationship between the based type and the
			   base type. */
  a_byte_boolean
		front_end_only;
			/* TRUE if the current entry and the associated based
			   type are used by the front end only; when this flag
			   is set, the entry is removed from the based-type
			   list after front-end processing is completed. */
} a_based_type_list_member;


/* 
A bit set whose values represent the presence of one or more pointer modifiers
(such as "__ptr32" or "__uptr").
*/
typedef a_byte a_pointer_modifier_set;

/*
Enumeration of pointer modifiers that are accepted.  The enumeration values
are used to create bit masks that are used to represent the various modifiers.
Note that -- unlike type qualifiers -- pointer modifiers are not dropped by
calls to skip_typerefs.
*/
enum a_pointer_modifier {
#if MICROSOFT_EXTENSIONS_ALLOWED
  pmt_ptr32,		/* __ptr32 modifier. */
  pmt_ptr64,		/* __ptr64 modifier. */
  pmt_sptr,		/* __sptr modifier ("signed pointer"). */
  pmt_uptr,		/* __uptr modifier ("unsigned pointer"). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  pmt_last
};

/*
Definitions of the bits in bit sets of type a_pointer_modifier_set.
*/
#define PM_NONE		((EDG_QUAL a_pointer_modifier_set)0x0)
			/* No pointer modifiers. */
#if MICROSOFT_EXTENSIONS_ALLOWED
#define PM_PTR32	((a_pointer_modifier_set)(1 << (int)pmt_ptr32))
			/* This bit is set to represent __ptr32. */
#define PM_PTR64	((a_pointer_modifier_set)(1 << (int)pmt_ptr64))
			/* This bit is set to represent __ptr64. */
#define PM_SPTR		((a_pointer_modifier_set)(1 << (int)pmt_sptr))
			/* This bit is set to represent __sptr. */
#define PM_UPTR		((a_pointer_modifier_set)(1 << (int)pmt_uptr))
			/* This bit is set to represent __uptr. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Some typerefs are added during compilation to indicate that a type has been
modified in some way.  This enumeration lists the various kinds of uses of
typerefs.  Note that this information was previously represented by bit
fields but has been moved to an enumeration to save space.  As a result,
these values are now mutually exclusive (whereas before both the is_typeof
and is_typeof_with_type_operand bits could be TRUE).  Some of the names
have been modified to help illustrate that.

If you add new typeref kinds, update typeref_is_type_operator in types.h
and gen_type_operator in cp_gen_be.c.
*/
enum a_typeref_kind : a_byte {
  trk_none,             /* The typeref has no special meaning (e.g., used for
                           an ordinary typedef). */
  trk_is_decltype,      /* The type was created by a decltype(<expr>)
                           operator. */
  trk_is_deduced_decltype_auto,
                        /* The type resulted from deducing a "decltype(auto)"
                           specifier. */
  trk_is_deduced_auto,  /* The type resulted from deducing an "auto" type
                           specifier. */
  trk_is_deduced_class, /* The type resulted from deducing class template
                           arguments (a C++17 feature). */
  trk_is_underlying_type,
                        /* The type was created by an __underlying_type
                           operator (a Microsoft extension). */
  trk_is_typeof_with_expression,
                        /* The type was created by a typeof operator applied
                           to an expression, i.e., typeof(expr). */
  trk_is_typeof_with_type_operand,
                        /* The type was created by a typeof operator applied to
                           a type, i.e., typeof(type-name). */
  trk_for_type_attributes,
                        /* The underlying type has type-transforming attributes
                           applied to it and this entry's attributes field (in
                           source_corresp) describes those attributes. */
  trk_is_alias,         /* A type entry representing an alias but not an
                           instance of an alias template. */
  trk_is_template_alias,
                        /* A type created for instantiations of alias
                           templates, including the prototype instantiation. */
  trk_is_splice,        /* A type entry representing an type splice. */
  trk_bases,            /* TRUE for a typeref entry for a g++ __bases
                           operator. */
  trk_direct_bases,     /* TRUE for a typeref entry for a g++ __direct_bases
                           operator. */
  trk_add_lvalue_reference,
  trk_add_pointer,
  trk_add_rvalue_reference,
  trk_decay,
  trk_make_signed,
  trk_make_unsigned,
  trk_remove_all_extents,
  trk_remove_const,
  trk_remove_cv,
  trk_remove_cvref,
  trk_remove_extent,
  trk_remove_pointer,
  trk_remove_reference_t,
  trk_remove_restrict,
  trk_remove_volatile,
                        /* These are Clang "type-returning type traits" that
                           take a single type "argument" and "return" a
                           suitably-modified type. */
  trk_remove_reference, /* GCC 13.1.0 "type-returning type trait". */
  trk_template_arg_list,
                        /* A typeref representing the originally-written
                           template argument list. */
  trk_name_qualifier,   /* A typeref representing the nested name specifier of
                           the type as written. */
  trk_pack_index,       /* A typeref representing a C++26 pack-index-specifier
                           (T...[N]). */
};

/*
Utility to interrogate the value of type->variant.typeref.kind for a
particular a_typeref_kind.
*/
#if EXPENSIVE_CHECKING
#define is_typeref_kind(type, typeref_kind) \
  (check_assertion((type)->kind == tk_typeref), \
   (type)->variant.typeref.kind == (typeref_kind))
#else /* !EXPENSIVE_CHECKING */
#define is_typeref_kind(type, typeref_kind) \
  ((type)->variant.typeref.kind == (typeref_kind))
#endif /* EXPENSIVE_CHECKING */

typedef struct a_type {
  /* Description of a type. */
  /* The source_corresp field must be first. */
  a_source_correspondence
                source_corresp;
                        /* Information on any source entity that corresponds
                           to this entity. */
  a_type_ptr    next;
                        /* Pointer to the next type declared in the same
                           scope, NULL if this type is the last in the
                           scope. */
  a_based_type_list_member_ptr
		based_types;
			/* Pointer to a list of entries that point to types
			   based on this one, e.g., pointer-to-this-one;
			   used to find those types for reuse.  NULL if
			   the list is empty.  Note that this is used as
			   an optimization, to save space.  There is no
			   guarantee that all based types are on this list,
			   though most of them are. */
  a_targ_size_t	size;
                        /* sizeof() for this type, or 0 if the type is
                           incomplete.  Also 0 for typeref references, even
                           if the referenced type is not incomplete.
                           In GNU C mode, zero-length arrays and empty structs
                           and unions can have size 0 and be complete at the
                           same time (the bound_is_zero or is_empty_class
                           flags must be set in those cases). */
  a_targ_alignment
                alignment;
                        /* Alignment required for this type.  This is the
                           number by which the object address must be
                           divisible.  1 if not applicable. */
  a_type_kind   kind;
                        /* The kind of type. */
  a_bit_field	incomplete:1;
			/* TRUE if the given type has been declared without
			   having been defined or if the type is void.  (The
			   flag is initially set to TRUE, and then cleared
			   when the size of the type is computed.)  For
			   typerefs, the flag should be checked in the
			   underlying type entry. */
  a_bit_field	used_in_exception_or_rtti:1;
			/* TRUE if this type appeared as (1) the type of an
			   exception-declaration of a handler, (2) the type
			   of a throw expression, (3) an
			   exception-specification, (4) the type of a typeid
			   operand, or (5) the source expression type or
			   destination type in a runtime dynamic_cast. */
  a_bit_field	declared_in_function_prototype:1;
			/* TRUE if this is a local type declared or defined
			   within a function prototype scope (C mode only). */
  a_bit_field	is_tag_redefinition:1;
			/* TRUE for the type formed by a second or later
			   definition of a tag that C23 allows to be declared
			   more than once in a scope.  The tag denotes the
			   type formed by the first definition, which this one
			   is required to match, so a back end that emits a
			   definition for each type it is given should skip
			   this one. */
  a_bit_field	is_instantiation_dependent:1;
			/* TRUE if this type is instantiation dependent.  If
			   FALSE and is_instantiation_dependent_cached is TRUE,
			   then the type is known not to be instantiation
			   dependent. */
  a_bit_field	is_instantiation_dependent_cached:1;
			/* TRUE if the value of is_instantiation_dependent is
			   fully determined. */
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  a_bit_field	use_cfront_transitional_nested_type_name_mangling:1;
                        /* TRUE if this type should be treated as a
                           non-nested type for purposes such as name
                           mangling.  This is used for compatibility
                           with cfront 2.1 which promotes nested types
                           to the file scope unless the name is already
                           used as a type name at the file scope. */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if BACK_END_IS_C_GEN_BE
  a_bit_field	prototype_scope_types_if_any_promoted:1;
			/* On function types, TRUE if the type has been
			   examined for prototype scopes, and the types
			   in those scopes promoted out to the file scope. */
  a_bit_field	typedef_pending:1;
			/* For a typedef, a TRUE value indicates that the
			   definition has been deferred from its position in
			   the type list and will be emitted later; this is
			   used to make the typedef "invisible," i.e., to
			   force use of the underlying type instead of the
			   typedef name in the generated code.  For a
			   class/struct/union type, a TRUE value indicates
			   that there are one or more deferred typedefs
			   whose definition should be emitted after this
			   type's definition. */
  a_bit_field	generated_as_empty_struct:1;
			/* Initially FALSE; set to TRUE for an empty
			   class/struct/union if the C-generating back end
			   generates the corresponding struct as empty.  See
			   USE_EMPTY_STRUCT_IN_GENERATED_C for details. */
#endif /* BACK_END_IS_C_GEN_BE */
  a_bit_field	has_been_defined:1;
			/* Used in the C- and C++-generating back ends for
			   class/struct/union types: FALSE until the
			   definition has been emitted, TRUE thereafter. */
  a_bit_field	typedef_definition_has_been_put_out:1;
			/* TRUE if this type is a typedef and its definition
			   has been put out.  Used only within the C- and
			   C++-generating back ends. */
#if BACK_END_IS_CP_GEN_BE
  a_bit_field	has_been_declared:1;
			/* Initially FALSE and set to TRUE when the type has
			   been declared or defined.  This is used to ensure
			   that the first reference to a tagged type is
			   generated as an elaborated-type-specifier, even
			   when the skip_embedded_declarations mechanism for
			   non-autonomous types does not apply. */
  a_bit_field	definition_delayed:1;
			/* Used to indicate the definition of this (tag) type
			   is required and should be put out at the first
			   opportunity.  Used only within the C++-generating
			   back end. */
  a_bit_field	elaborated_type_specifier_needed:1;
			/* An elaborated type specifier (e.g., "class X")
			   is needed when referring to this type in the
			   current scope.  Set and cleared only in the
			   C++-generating back end. */
  a_bit_field	elab_type_spec_needed_in_some_scope:1;
			/* TRUE if this is a tag type that is hidden by a
			   non-type entity in at least one scope in the
			   current translation unit. */
  a_bit_field	replace_by_generated_typedef:1;
			/* TRUE if references to this type should be replaced
			   by references to a generated typedef.  This is used
			   in the C++-generating end, to deal with a Microsoft
			   bug. */
  a_bit_field	typedef_for_vacuous_dtor_call_put_out:1;
			/* TRUE if a temporary typedef for this type has been
			   generated for use in a vacuous destructor call. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	emit_microsoft_class_decl_modifiers:1;
			/* TRUE if the Microsoft declaration modifiers
			   (__declspec(...), etc.) should be inserted into a
			   class declaration.  Set and used only within the
			   C++-generating back end. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_bit_field	explicit_specialization_suppressed:1;
			/* Set to TRUE by the C++-generating back end if
			   the explicit specialization corresponding to
			   this class template instance was suppressed
			   because it would have been invalid. */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  a_bit_field	suppress_operator:1;
			/* A type operator following a left parenthesis in
			   a function declaration or C-style cast can be
			   misparsed as an expression instead of a type.
			   This flag is set and cleared, only in the
			   C++-generating back end itself, to mark such
			   uses of type operators, indicating that the
			   underlying type should be put out instead of
			   the type operator. */
  a_bit_field	force_typename_kwd:1;
			/* If TRUE, the name of this type must be preceded
			   with the "typename" keyword to avoid a g++ bug,
			   even though it is not dependent.  Set and used
			   only in the C++-generating back end. */
#endif /* BACK_END_IS_CP_GEN_BE */
  a_bit_field	alignment_set_explicitly:1;
			/* TRUE if this type differs from the type it
			   refers to because its alignment has been
			   explicitly set, via an attribute. */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	variables_are_implicitly_referenced:1;
			/* TRUE if no warnings about unused variables
			   should be emitted for variables that have
			   this type. */
  a_bit_field	may_alias:1;
			/* TRUE if this is a type resulting from the GNU
			   attribute "may_alias". */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	has_microsoft_w64_specifier:1;
			/* TRUE if this is a type that is the same as some
			   other type, but declared using the Microsoft __w64
			   specifier.  Implicit conversions from such types to
			   equal-sized integral types without the __w64
			   specifier are diagnosed with a remark to help
			   identify potential 64-bit portability issues. */
  a_bit_field	is_microsoft_intrinsic:1;
			/* TRUE if this is a class type declared with the
			   __declspec(intrin_type) specifier. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_bit_field	autonomous_primary_tag_decl:1;
			/* TRUE if this type entry represents a class, struct,
			   union, or enum and its primary source sequence
			   entry refers to a declaration that is not part
			   of the declaration of another entity.  For instance,
			     class A { int i; };    // An "autonomous" decl
                             class B { int i; } b;  // Not "autonomous"
			   The flag would be set TRUE for A but not for B
			   since the latter's definition is part of the
			   declaration of variable b.  Also TRUE for
			   anonymous unions.  Also TRUE for all classes
			   produced by a template instantiation, no matter
			   what triggers the instantiation. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_bit_field	is_builtin_va_list:1;
			/* TRUE if this type is the va_list type declared by
			   <stdarg.h> or <cstdarg>, when that's treated as
			   built-in.  In GNU mode (C or C++), if
			   GCC_BUILTIN_VARARGS is TRUE, the type
			   marked is __builtin_va_list, not va_list. */
  a_bit_field	is_builtin_va_list_from_cstdarg:1;
			/* TRUE if this is the va_list type declared by
			   <cstdarg>, when that's treated as built-in. */
#ifdef GUARD_MACRO_FOR_VA_LIST
  a_bit_field	va_list_guard_macro_was_defined:1;
			/* TRUE if the macro named by GUARD_MACRO_FOR_VA_LIST
			   was defined at the point where <stdarg.h> was
			   included. */
#endif /* ifdef GUARD_MACRO_FOR_VA_LIST */
#ifdef GUARD_MACRO2_FOR_VA_LIST
  a_bit_field	va_list_guard_macro2_was_defined:1;
			/* TRUE if the macro named by GUARD_MACRO2_FOR_VA_LIST
			   was defined at the point where <stdarg.h> was
			   included. */
#endif /* ifdef GUARD_MACRO2_FOR_VA_LIST */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	has_gnu_abi_tag_attribute:1;
			/* TRUE if this class or enum type was declared with
			   the GNU "abi_tag" attribute. */
  a_bit_field	in_gnu_abi_tag_namespace:1;
			/* TRUE if this type has some parent that is an inline
			   namespace with a GNU "abi_tag" attribute. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_bit_field	definition_pending:1;
			/* TRUE if the definition of this class is being
			   processed by get_definition_of_class. */
#if DO_IL_LOWERING
#if ENSURE_LOWERED_TYPE_LIST_ORDERING
  a_bit_field	process_for_ordering:1;
			/* Set during the processing that fixes ordering
			   problems in the file scope types list, to indicate
			   that the type should be processed (because it is
			   on the file scope list). */
  a_bit_field	type_processed_for_ordering:1;
			/* Set during the processing that fixes ordering
			   problems in the file scope types list, to indicate
			   that the type has already been placed in the new
			   ordering.  (For types not on the file scope types
			   list it is set when the type has been traversed
			   for the purpose of determining the type ordering.)
			   */
  a_bit_field	type_processed_as_complete_for_ordering:1;
			/* Similar to previous, but marks whether the type has
			   been processed as a complete type.  For class and
			   enum types the two flags are identical, but for
			   typedefs type_processed_for_ordering may be TRUE
			   while this flag is still FALSE. */
#endif /* ENSURE_LOWERED_TYPE_LIST_ORDERING */
#if LOWER_VARIABLE_LENGTH_ARRAYS
  a_bit_field	visited_for_vla_lowering:1;
			/* Flag to optimize the traversal of types to lower
			   VLA-based types.  If TRUE, the type was visited
			   already and no additional traversal is needed. */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
  a_variable_ptr
		typeinfo_var;
			/* When non-NULL, points to a variable (generated by
			   IL lowering) that contains the typeinfo information
			   for this type.  Only needed for types used in
			   exceptions. */
#endif /* DO_IL_LOWERING */
  union {
    /* When kind == tk_nullptr, tk_reflection, tk_error, tk_unknown, or
       tk_void, no variant fields. */
    /* When kind == tk_integer: */
    struct {
      an_integer_kind
                int_kind;
                        /* Which kind of integer type. */
      a_bit_field
                explicitly_signed:1;
                        /* TRUE if the type specifiers for this type included
                           "signed" explicitly.  Needed for bit fields, where
                           "signed int" and "int" may not mean the same
                           thing; used for ik_short, ik_long, and ik_long_long
			   as well as for ik_int. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_bit_field
		microsoft_sized_int_type:1;
			/* TRUE if this is a Microsoft __intN type that should
			   be treated as a distinct built-in type (rather than
			   a typedef for another integer type). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      a_bit_field
		has_explicit_enum_base:1;
			/* TRUE if this is an enumeration type with an explicit
			   specifier for the underlying type.  (If TRUE, the
			   optional field base_type will be non-NULL.) */
      a_bit_field
                enum_type:1;
                        /* TRUE if this type is an enumerated type (the type 
                           of the tag, not the constants, in C). */
      a_bit_field
		is_scoped_enum:1;
			/* TRUE if this a scoped enum type (enum_type is also
			   TRUE in that case). */
#if GNU_EXTENSIONS_ALLOWED
      a_bit_field
      		packed:1;
			/* TRUE if this type is an enumerated type,
			   and its size may be smaller than "int",
			   even if enum_types_can_be_smaller_than_int
			   is FALSE.  Unused if this type is not an
			   enumerated type. */
#endif /* GNU_EXTENSIONS_ALLOWED */
      a_bit_field
		wchar_t_type:1;
			/* TRUE if this type is wchar_t in C++ when wchar_t
                           is a distinct type.  (This can also be TRUE in
			   Microsoft C mode for the type produced by the
			   __wchar_t keyword.) */
      a_bit_field
		char8_t_type:1;
			/* TRUE if this type is char8_t in C++ when
			   char8_t_enabled is TRUE. */
      a_bit_field
		char16_t_type:1;
			/* TRUE if this type is char16_t in C++ when char16_t
                           is a distinct type. */
      a_bit_field
		char32_t_type:1;
			/* TRUE if this type is char32_t in C++ when char32_t
                           is a distinct type. */
      a_bit_field
		bool_type:1;
			/* TRUE if this type is bool in C++ or _Bool in C99. */
      a_bit_field
		originally_unnamed:1;
			/* TRUE for enum types declared without a tag; in
			   C++ may be TRUE even when the source-corresp name
			   pointer is non-NULL, since a name may be acquired
			   from a typedef name. */
      a_bit_field
		is_template_enum:1;
			/* TRUE for enum types declared in instantiated
			   template classes.  This is TRUE even if the
			   instance has been explicitly specialized. */
      a_bit_field
		is_prototype_instantiation:1;
			/* TRUE when this enum is a nonreal type that
		 	   is the prototype instantiation. */
      a_bit_field
		is_nonreal:1;
			/* TRUE for enum types declared in a prototype
			   instantiation. */
      a_bit_field
		is_specialized:1;
			/* TRUE for enum instances that were explicitly
			   specialized. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_bit_field
		is_ms_instantiated_nonreal_enum:1;
			/* TRUE if the enum is a nonreal enum that was actually
			   instantiated like a real enum.  This is done for
			   certain nonreal enums used as members of base
			   classes in Microsoft mode. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
      ENUM_TYPE_FOR_BIT_FIELD(an_ELF_visibility_kind)
		ELF_visibility:3;
			/* The visibility of the enum type. */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
      union {
        /* When enum_type is TRUE, but is_scoped_enum is FALSE: */
        a_constant_ptr
		constant_list;
			/* The list of constants that defines the enumeration.
			   NULL if the enumeration has not yet been defined.
			   In C++, may be NULL even after definition, since
			   empty enumerations are allowed. */
        /* When enum_type and is_scoped_enum are both TRUE: */
        a_scope_ptr
		assoc_scope;
			/* The scope holding the enumerator constants. */
        /* When enum_type is FALSE: */
        a_type_ptr
		affiliated_type;
			/* If non-NULL, points to the type entry for an
			   enum type (which has enum_type == TRUE), indicating
			   that the present type is an integral type that
			   came from the indicated enumerated type.  Used
			   to suppress conversion warnings.  Always NULL
			   in C++. */
      } enum_info;
      an_integer_type_supplement_ptr
		extra_info;
                        /* Supplementary information, in a separate block
                           to keep down the size of a_type. */
    } integer;
#if FIXED_POINT_ALLOWED
    /* When kind == tk_fixed_point: */
    a_fixed_point_type_descr
		fixed_point;
			/* The characteristics (precision, overflow
			   behavior, ...) of this fixed-point type. */
#endif /* FIXED_POINT_ALLOWED */
    /* When kind == tk_float: */
#if C99_IL_EXTENSIONS_SUPPORTED
    /* Also, when kind == tk_imaginary: */
    /* Also, when kind == tk_complex: */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    a_float_kind
                float_kind;
                        /* Which size of float. */
    /* When kind == tk_pointer: */
    struct {
      a_type_ptr
                type;
                        /* Type pointed to by this pointer type. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_variable_ptr
		base_variable;
			/* Pointer to the variable that is the "base" when
			   the current pointer type is really a "based
			   pointer"; the variable must itself be of pointer
			   type.  Used only when microsoft_mode is TRUE. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      a_bit_field
		is_reference:1;
			/* If TRUE, this type is a C++ reference type.  (This
			   includes both ordinary ("lvalue") references, and
			   C++11 rvalue references.) */
      a_bit_field
		is_rvalue_reference:1;
			/* If TRUE, this type is a C++11 rvalue reference
			   type. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_bit_field
		is_handle:1;
			/* If TRUE, this type is a C++/CLI handle (is_reference
			   FALSE) or tracking reference (is_reference TRUE)
			   type. */
      a_bit_field
		is_interior_ptr:1;
			/* If TRUE, this type is a C++/CLI interior_ptr<T>,
			   which can (but need not) point to a subobject
			   within an object on the managed heap.  It is
			   otherwise treated as a normal pointer except in
			   a few contexts. */
      a_bit_field
		is_pin_ptr:1;
			/* If TRUE, this type is a C++/CLI pin_ptr<T>, which,
			   when it points to a subobject on the managed heap,
			   keeps the garbage collector from moving the object.
			   It is otherwise treated as a normal pointer except
			   in a few contexts. */
      a_pointer_modifier_set
		modifiers;
			/* Bit set with bits to indicate the presence of one
			   or more pointer modifiers (e.g., "__ptr32"). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } pointer;
    /* When kind == tk_routine: */
    struct {
      a_type_ptr
                return_type;
                        /* Return type of the function.  Might be an
			   incomplete struct or union type if function isn't
			   called. */
      a_routine_type_supplement_ptr
                extra_info;
                        /* Supplementary information, in a separate block
                           to keep down the size of a_type. */
#if DO_IL_LOWERING
      a_type_ptr
                unlowered_type;
                        /* In certain cases (namely when a pointer to this
                           function type has the used_in_exception_or_rtti bit
                           set), a copy of the un-lowered type is saved here
                           so that it can be properly mangled (a lowered
                           function type may not give the proper mangling).
                           This is used to get around a race condition with
                           typeid constants.  This type does not appear on
                           a type list and should not be used by a back end. */
#endif /* DO_IL_LOWERING */
    } routine;
    /* When kind == tk_array: */
    struct {
      a_type_ptr
                element_type;
                        /* Type of the elements of the array type. */
      a_bit_field
		qualifiers:NUM_BITS_FOR_TYPE_QUALIFIER_SET;
			/* Bit set with bits set to indicate the presence
			   of one or more type qualifiers in an array
			   declarator (const, volatile, or other(s) as defined
			   by the implementation).  This is a C99 feature.*/
      a_bit_field
		is_template_dependent_size_array:1;
			/* TRUE only in C++ and if the array size is constant
			   and depends on a template parameter.  If this flag
			   is TRUE, the flag is_variable_size_array must be
			   FALSE, and the variant "element_count_constant"
			   can be accessed. */
      a_bit_field
		is_variable_size_array:1;
			/* TRUE if the array size depends on the evaluation
			   of an expression at run time (for a new with a
			   nonconstant first bound or for a variable length
			   array).  Except for VLAs, this field will never
  			   be TRUE in the IL passed to the back end. */
      a_bit_field
		is_vla:1;
			/* TRUE if this array is a "variable length array",
			   one whose dimension is computed at run time.  This
			   field may be TRUE in the IL passed to the back end.
			   Only used in modes that allow VLAs. */
      a_bit_field
		constant_bound_expr_in_local_expr_node_ref:1;
			/* TRUE if the expression for a constant bound
			   contains a reference to a local variable.
			   Because such expressions cannot appear in
			   file-scope memory, the expression is represented
			   as an a_local_expr_node_ref in the function
			   scope and the associated expr field will be
			   NULL.  This applies to bound_constant.expr. */
      a_bit_field
		dep_constant_bound_expr_in_local_expr_node_ref:1;
			/* Like constant_bound_expr_in_local_expr_node_ref,
			   but for the expressions inside the
			   element_count_constant ck_template_param
			   constant, i.e., the expr and templ_sizeof.expr
			   fields of element_count_constant->variant
			   .template_param.variant. */
      a_bit_field
		has_assoc_vla_dimension:1;
			/* TRUE if the variable length array has an associated
			   vla_dimension entry.	 FALSE for cases like [*].
			   (Only set when is_vla is TRUE.)  */
      a_bit_field
		bound_is_zero:1;
			/* TRUE if this array actually has a zero bound.  This
			   is used to distinguish zero-length array types ([0])
			   from array types with unspecified bounds ([]). */
      a_bit_field
		is_static:1;
			/* TRUE if this array is tagged with the C99 keyword
			   static, which indicates for a parameter that the
			   argument passed must have at least as many members
			   as the array size. */
#if UPC_EXTENSIONS_ALLOWED
      a_bit_field
		is_threads_dimension:1;
			/* TRUE if this dimension is a THREADS dimension of
			   a UPC shared array. */
#endif /* UPC_EXTENSIONS_ALLOWED */
      union {
        /* When is_variable_size_array and is_template_dependent_size_array
           are FALSE: */
        a_targ_size_t
                number_of_elements;
                        /* Number of elements in the array.  0 indicates
                           the [] incomplete-type case. */
        /* When is_variable_size_array is TRUE: */
	an_expr_node_ptr
		element_count_expr;
			/* An expression representing the number of elements
			   in the array.  Used only in front-end processing,
			   and only in C++ mode.  Always NULL if is_vla is
			   TRUE. */
        /* When is_template_dependent_size_array is TRUE: */
        a_constant_ptr
		element_count_constant;
			/* A constant giving the template-dependent number
			   of elements in the array, or NULL to indicate that
			   the number of elements is template-dependent but
			   we know nothing about its value. */
      } variant;
      a_constant_ptr
		bound_constant;
			/* A constant representing the number of elements in
			   the array.  For template-dependent dimensions, this
			   holds the same value as the variant
			   element_count_constant above. */
    } array;
    /* When kind == tk_class, tk_struct, or tk_union: */
    struct {
      a_field_ptr
                field_list;
                        /* The list of non-static data members (i.e., fields)
                           of the class, struct, or union. */
      a_class_type_supplement_ptr
                extra_info;
                        /* Supplementary information, in a separate block
                           to keep down the size of a_type. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_bit_field
		is_interface:1;
			/* TRUE if this is a struct type declared with the
			   Microsoft keyword __interface.  Member functions of
			   such types are implicitly pure virtual.  (Implies
			   kind == tk_struct, but a number of restrictions not
			   applicable to structs are imposed.) */
      a_bit_field
		is_interface_like:1;
			/* TRUE for certain class types that are accepted as
			   __interface types in Microsoft mode.  This includes
			   the IUnknown and IDispatch types, as well as certain
			   classes (directly or indirectly) derived from any of
			   those two types. */
      a_bit_field
		is_delegate_class:1;
			/* TRUE for a ref class created by a C++/CLI delegate
			   definition. */
      a_bit_field
		is_generic_definition:1;
			/* TRUE if this is the class type that resulted from
			   the initial scanning of a C++/CLI generic class.
			   This is similar to a prototype instantiation of
			   a template except that generics do not make use
			   of dependent types. */
     a_bit_field
		is_generic_instance:1;
			/* TRUE if this is an instantiation of a C++/CLI
			   generic class or a nested class within a generic. */
     a_bit_field
		is_open_constructed_type:1;
			/* TRUE for generic instances for which one or more
			   of the generic arguments is an open constructed
			   type.  Also TRUE for template classes instantiated
			   on generic type parameters. */
     a_bit_field
		is_generic_constraint:1;
			/* TRUE if this is the type created to represent the
			   type specified by the constraints of a C++/CLI
			   generic type parameter. */
     a_bit_field
		is_hybrid_constraint:1;
			/* TRUE if is_generic_constraint is TRUE and the
			   associated constraints permit both a ref class and
			   a value class (i.e., the constraint type is treated
			   as a kind of hybrid value/ref class). */
     a_bit_field
		any_interface_constraints:1;
			/* TRUE if is_generic_constraint is TRUE and the
			   associated constraint list contains any type
			   constraints that refer to interfaces. */
     a_bit_field
		unconstrained:1;
			/* TRUE if is_generic_constraint is TRUE and the
			   no constraints were specified for in the 
			   associated constraint list. */
      a_bit_field
		sealed:1;
			/* TRUE if this class was defined with the Microsoft-
			   mode context-sensitive keyword "sealed". */
#if BACK_END_IS_CP_GEN_BE
      a_bit_field
		defined_with_abstract_class_modifier:1;
			/* TRUE if this class was defined with the context-
			   sensitive keyword "abstract" (this also implies
			   that the "abstract" flag is set). */
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      a_bit_field
		final:1;
			/* TRUE if this class was defined with the attribute
                           or context-sensitive keyword "final" (C++11) or the
			   Microsoft-mode context-sensitive keyword "sealed".
			   Such a class type cannot be used as a base class. */
      a_bit_field
                any_const_member:1;
                        /* TRUE if any member of the class, struct, or union
                           is const-qualified. */
      a_bit_field
		any_volatile_member:1;
			/* TRUE if the type of any non-static data member is
			   a volatile-qualified type or, recursively, a class
			   type with a volatile-qualified member. */
      a_bit_field
		any_mutable_member:1;
			/* TRUE if any member field of the class, struct, or
			   union is declared "mutable" (C++ only). */
      a_bit_field
                any_virtual_base_classes:1;
                        /* TRUE if the class, struct, or union is derived from
			   one or more virtual base classes, either directly
			   or indirectly (C++ only). */
      a_bit_field
		abstract:1;
			/* If TRUE, as a result of having one or more pure
			   virtual member functions, this is an "abstract"
			   class and is subject to certain restrictions
			   (C++ only, ARM 10.3).  Also TRUE for Microsoft
			   interface classes and for classes defined with the
			   context-sensitive keyword "abstract" (a Microsoft
			   extension). */
      a_bit_field
		any_virtual_functions:1;
			/* TRUE if one or more member functions declared in
			   the class, struct, or union is virtual (C++ only).
			   (Inherited virtual functions that are not
			   redeclared in the current class do not affect
			   this flag.) */
      a_bit_field
		any_pure_virtual_functions:1;
			/* TRUE if one or more member functions declared in
			   the class, struct, or union is a pure virtual
                           function (C++ only).  Inherited pure virtual
			   functions do not affect this flag, and so not every
			   abstract class has this flag set TRUE.  (Microsoft
			   mode interface slots do not affect this flag
			   either.) */
      a_bit_field
		any_virtual_functions_including_in_base_classes:1;
			/* TRUE if one or more member functions declared in
			   the class, struct, or union or its base classes
			   is a virtual function (C++ only). */
      a_bit_field
		nested_class_defined_outside_of_parent:1;
			/* TRUE if the class is a nested class defined outside
			   its parent class. */
      a_bit_field
		originally_unnamed:1;
			/* TRUE if the class was declared without a tag; in
			   C++ may be TRUE even when the source-corresp name
			   pointer is non-NULL, since a name may be acquired
			   from a typedef name (ARM 7.1.3). */
      a_bit_field
		is_nonstd_anonymous_union_type:1;
			/* TRUE if this is the type of an anonymous-union-like
			   construct (an unnamed class/struct/union type, but
			   not one represented by a typedef name, whose
			   subfields are to be visible as though they were
			   fields of the enclosing class).  Despite the name,
			   this flag is also TRUE for C11-style anonymous
			   unions and anonymous structures (which are now
			   "standard", but this flag predates C11). */
      a_bit_field
		is_template_class:1;
			/* TRUE if the class is an instance of a class template
			   or a class nested within a class template.  This
			   value is TRUE even if the instance has been
			   explicitly specialized. */
      a_bit_field
		is_nonreal_class:1;
			/* TRUE if the class is an instantiation of a class
			   template based on template arguments that include
			   one or more template parameters.  For instance,
			   for the class template declared by
			      template <class T, int I> class vec;
			   the prototype instantiation vec<T,I> is a "nonreal"
			   class, but so is vec<T,3>, where T represents a
			   template parameter, e.g., in the declaration:
			      template <class T> void f(vec<T,3> *vp) { ... }
                           In addition, classes that are nested within
			   nonreal classes are marked as nonreal. */
      a_bit_field
		is_ms_instantiated_nonreal_class:1;
			/* TRUE if the class is a nonreal class that was
			   actually instantiated like a real class.  This is
			   done for certain nonreal classes used as
			   base classes in Microsoft mode. */
      a_bit_field
		is_prototype_instantiation:1;
			/* TRUE when this class is a nonreal class that
		 	   is the prototype instantiation.  Also TRUE for
			   classes nested within the prototype
			   instantiation. */
      a_bit_field
		is_specialized:1;
			/* TRUE for class template instances for which the
			   definition is supplied independently of the class
			   template with which it is associated.  This flag
			   may be set as a result of a specialization
			   declaration (either an old-style declaration or
			   one using the template<> syntax), or if the class
			   was specified in a do-not-instantiate pragma. */
      a_bit_field
		specialized_with_old_syntax:1;
			/* TRUE if is_specialized is TRUE but the class was
			   not explicitly declared with the template<>
			   syntax. */
      a_bit_field
		is_in_class_specialization:1;
			/* TRUE if this is a specialized template instance
			   and the specialization was declared within the
			   enclosing class.  Also true for classes nested
			   within an in-class specialization */
      a_bit_field
		explicitly_instantiated:1;
			/* TRUE if this class was referenced in an explicit
			   instantiation directive (either the standard form
			   or pragma). */
      a_bit_field
		do_not_instantiate:1;
			/* TRUE if this class template will not be
			   instantiated because of a do-not-instantiate
			   directive (i.e., "extern template" or "#pragma
			   do_not_instantiate"). */
      a_bit_field
		proxy_class:1;
			/* TRUE if this is a proxy class associated with a
			   template parameter or a dependent decltype.  This
			   is also TRUE for a C++/CLI constraint type for a
			   generic parameter. */
#if MAINTAIN_NEEDED_FLAGS
      a_bit_field
		definition_needed:1;
			/* TRUE if this class is "needed" (see the flag by
			   that name in the source_corresp field), but not
			   merely as a declaration -- a definition of the
			   class is needed in the current translation unit. */
      a_bit_field
		keep_definition_in_il:1;
			/* TRUE if this class's definition should be kept in
			   the IL tree (i.e., should not be discarded before
			   the IL is passed to the back end).  It is for
			   front-end use only. */
#endif /* MAINTAIN_NEEDED_FLAGS */
      a_bit_field
		is_empty_class:1;
			/* TRUE if this class has no nonstatic data members,
			   virtual functions, virtual base classes or bases
			   (direct or indirect) with such things.  (In C mode,
			   that reduces to structs and unions with no fields.)
			   Computed in do_class_layout.  In GNU C mode, this
			   is also TRUE for zero-sized classes. */
      a_bit_field
		no_proper_data:1;
			/* TRUE if this class has no non-inherited data members
			   (excluding unnamed bit fields) and no virtual
			   functions or virtual base classes. */
      a_bit_field
		has_zero_init_component:1;
			/* TRUE if an object of this type has no nontrivial
			   default constructor, or if a call to that
			   constructor is insufficient to value-initialize
			   the object (i.e., a part of it must be zero-
			   initialized). */
      a_bit_field
		has_pointer_component:1;
			/* TRUE if a subobject of this type is a pointer or
			   reference. */
      a_bit_field
		contains_flexible_array_member:1;
			/* TRUE if this is a class or struct type and the last
			   field is an incomplete array type or a class type
			   that has this flag set.  In Microsoft mode, this can
			   also be TRUE for union types. */
#if GNU_EXTENSIONS_ALLOWED
      a_bit_field
      		is_transparent:1;
			/* TRUE if this is a union type that is
			   "transparent".  If a parameter has
			   transparent union type, then it is OK to
			   pass an argument whose type is one of the
			   union members. */
      a_bit_field
      		is_packed:1;
			/* TRUE if this class type was declared with the GNU C
			   "packed" attribute. */
      a_bit_field
		has_internal_linkage_attribute:1;
			/* TRUE if this class type was declared with the Clang
			   "internal_linkage" attribute.  This attribute
			   affects all members of the class. */
#endif /* GNU_EXTENSIONS_ALLOWED */
      a_bit_field
		has_operator_ampersand:1;
			/* TRUE if this class type has an operator&() member
			   function. */
      a_bit_field
		virtual_functions_marked_as_required:1;
			/* TRUE if the virtual functions of the class have
			   been marked as required by
			   require_definitions_of_virtual_functions_in_class;
			   this is used to avoid doing it again. */
      a_bit_field
		copy_assignment_decl_suppressed:1;
			/* TRUE if the class would have had an implicitly-
			   declared copy assignment operator but its
			   declaration was suppressed because the class has
			   a nonstatic data member of reference or
			   const-qualified type or a base or nonstatic
			   member with an ambiguous or inaccessible copy
			   assignment operator.  (Such suppression is
			   nonstandard and occurs only in Microsoft
			   mode.) */
      a_bit_field
		copy_ctor_decl_suppressed:1;
			/* TRUE if the class would have had an implicitly-
			   declared copy constructor but its declaration
			   was suppressed because the class has a nonstatic
			   data member or base class with an ambiguous or
			   inaccessible copy constructor.  (Such
			   suppression is nonstandard and occurs only in
			   Microsoft mode.) */
      a_bit_field
		default_ctor_decl_suppressed:1;
			/* TRUE if the class would have had an implicitly-
			   declared default constructor but its declaration
			   was suppressed because its generation would have
			   triggered an error.  (Such suppression is
			   nonstandard and occurs only in Microsoft mode.) */
      a_bit_field
		dtor_decl_suppressed:1;
			/* TRUE if the class would have had an implicitly-
			   declared destructor but its declaration was
			   suppressed because the class has a nonstatic
			   data member or base class with an inaccessible
			   destructor.  (Such suppression is nonstandard
			   and occurs only in Microsoft mode.) */
      a_bit_field
		inc_class_used_in_array_type:1;
			/* Initially FALSE, set to TRUE if this class is
			   used as the element type of an array while the
			   class is still incomplete, which is permitted in
			   C++.  It is not reset if the class is later
			   defined.  Used by the C-generating back end to
			   avoid using an incomplete struct type as an
			   array element type in the generated code, which
			   is an error in C. */
      a_targ_alignment
		max_member_alignment;
			/* If nonzero, the maximum alignment of any nonstatic
			   data member of this class, struct, or union (even
			   if the member's type indicates a greater alignment).
			   Its value is based on command-line option
			   "--pack_alignment", unless that has been overridden
			   by "#pragma pack". (A zero value means that each
			   nonstatic data member's alignment is based solely
			   on its type.) */
#if BACK_END_IS_CP_GEN_BE
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      a_type_scan_record_ptr
		scan_record;
			/* When the C++-generating back end scans types to
			   determine whether a generated explicit
			   specialization would be invalid because of an
			   unnameable template argument, it creates a scan
			   record pointing to the type entry for each type
			   traversed during the scan, to facilitate
			   prevention of unbounded loops and recursion.
			   This pointer designates the corresponding scan
			   record if this type is being or has already been
			   processed during that scan and thus should be
			   skipped; otherwise, it is NULL. */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
      a_bit_field
		do_not_suppress_templ_arg:1;
			/* If TRUE, the C++-generating back end will put
			   out the template argument list of the prototype
			   instantiation of a class template, even if it
			   would otherwise be suppressed.  This flag is set
			   and cleared only in the C++-generating back end
			   and is used to indicate when the
			   injected-class-name is referenced using the
			   qualified name of the class template and not the
			   bare template name. */
#endif /* BACK_END_IS_CP_GEN_BE */
    } class_struct_union;
    /* When kind == tk_typeref: */
    struct {
      a_type_ptr
                type;
                        /* Type referenced. */
      a_typeref_type_supplement_ptr
		extra_info;
			/* Pointer to a supplement containing additional
			   information about this typeref. */
#if DO_IL_LOWERING
      a_type_ptr
		orig_type;
			/* When this typeref represents a type
			   (specifically, a pointer to member or nullptr
			   type) that has been lowered to something, this
			   points to a copy of the original type.  NULL
			   otherwise.  For internal use in IL lowering
			   only. */
#endif /* DO_IL_LOWERING */
      a_typeref_kind
		kind;   /* The kind of typeref. */
      a_bit_field
		qualifiers:NUM_BITS_FOR_TYPE_QUALIFIER_SET;
			/* Bit set with bits set to indicate the presence
			   of one or more type qualifiers (const, volatile,
			   or other(s) as defined by the implementation). */
      a_bit_field
		predeclared:1;
			/* TRUE for predeclared typedefs. */
#if NEAR_AND_FAR_ALLOWED
      a_bit_field
		explicit_memory_attribute_made_implicit:1;
			/* TRUE if an explicit memory attribute (e.g., near)
			   was omitted from this typeref because it is the
			   default.  Used only when near and far are
			   enabled (e.g., Microsoft 16-bit mode). */
#endif /* NEAR_AND_FAR_ALLOWED */
      a_bit_field
		has_variably_modified_type:1;
			/* The type referred to is a variably modified type,
			   i.e., is or contains a VLA type. */
#if LOWER_VARIABLE_LENGTH_ARRAYS
      a_bit_field
		is_lowered_variably_modified_type:1;
			/* The type referred to is a lowered variably modified
			   type. */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#if BACK_END_IS_CP_GEN_BE
      ENUM_TYPE_FOR_BIT_FIELD(a_name_linkage_kind)
		surrounding_name_linkage_state:NUM_BITS_FOR_NAME_LINKAGE;
			/* Name linkage in effect when this typedef
			   appeared. */
      a_bit_field
		is_renamed_builtin:1;
			/* Handling certain built-in alias templates requires
			   renaming them.  For example, __builtin_common_type
			   is renamed to __builtin_common_type_alias (and a
			   placeholder class template __builtin_common_type is
			   created alongside of it).  This flag indicates that
			   this is an instance of such an alias template, to
			   help the C++-generating back end to render the
			   original name. */
#endif /* BACK_END_IS_CP_GEN_BE */
      a_bit_field
		decltype_expr_not_parenthesized:1;
			/* This is a decltype entry and its argument
			   expression is not parenthesized.  TRUE only if
			   (a) there are no parentheses around the decltype
			   argument expression and (b) that lack of parentheses
			   is significant (the meaning would be different if
			   parentheses were present; that is the case only for
			   id-expressions and member access operators).
			   So, for example, TRUE for "decltype(x.y)" and
			   FALSE for "decltype((x.y))". */
      a_bit_field
		is_dependent_type_operator:1;
			/* TRUE if the type was created by decltype,
			   __underlying_type, or typeof, and it's dependent
			   (including cases where there are dependent
			   subexpressions but the final result has a
			   non-dependent type). */
      a_bit_field
		is_nonreal:1;
			/* TRUE if the result of an alias instantiation is
			   a dependent type.  It is not TRUE if the alias
			   instantiation has a dependent argument that is
			   not used in the resulting type (see is_dependent
			   below). */
      a_bit_field
		is_dependent:1;
			/* TRUE if the type is an instantiation of a template
			   alias based on template arguments that include
			   one or more template parameters.
			   In addition, types from alias templates that
			   are nested within nonreal classes are marked as
			   nonreal. */
      a_bit_field
		is_prototype_instantiation:1;
			/* TRUE when this type is a nonreal type that
		 	   is a prototype instantiation. */
#if C99_IL_EXTENSIONS_SUPPORTED && LOWER_COMPLEX
      a_bit_field
		is_lowered_complex_type:1;
			/* TRUE if this typeref represents a lowered complex
			   type. */
#endif /* C99_IL_EXTENSIONS_SUPPORTED && LOWER_COMPLEX */
      a_bit_field
		embedded_source_sequence_entries:1;
			/* TRUE if the declaration represented by this
			   entry embeds another construct with associated
			   source sequence entries.  For example:
			     typedef int i[sizeof(struct S { int j; })];
			   In this example, the source sequence entries for
			   the non-autonomous struct S are considered to be
			   "embedded".  In such cases, the embedded entries
			   are followed by an a_src_seq_end_of_construct
			   for the typeref entry. */
      a_bit_field
		added_to_record_name:1;
			/* TRUE if this typeref was added to record the
			   name used to name a class member. */
      a_bit_field
		has_typename_prefix:1;
			/* TRUE if this typeref represents a splice with an
			   explicit "typename" keyword. */
      a_bit_field
		is_global_qualified_name:1;
			/* TRUE if this typeref represents a type written using
			   a global namespace qualifier. */
      a_bit_field
		is_intrinsic_member:1;
			/* TRUE if this typeref is the synthesized entry for
			   an intrinsically-resolved type member of a class
			   template (like std::remove_cv<T>::type; see
			   templ_type_member_intrinsics_enabled). */
    } typeref;
    /* When kind == tk_ptr_to_member: */
    struct {
      a_type_ptr
		class_of_which_a_member;
			/* Type of the class to which the member pointed to
			   belongs.  This cannot be a typeref (but the front
			   end occasionally temporarily makes this point to a
			   typeref until "type" is set). */
      a_type_ptr
		orig_class_of_which_a_member;
			/* Type of the class to which the member pointed to
			   belongs, as specified; this might be the same as
			   class_of_which_a_member or it might be a typeref. */
      a_type_ptr
		type;
			/* Type of the member pointed to. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_pointer_modifier_set
		modifiers;
			/* Bit set with bits to indicate the presence of one
			   or more pointer modifiers.  Microsoft compilers
			   only accept the "__ptr32" and "__ptr64" modifiers.
			   Furthermore, they only appear to affect the size of
			   pointer-to-member-functions.  However, even when
			   the size is not affected, the type is considered
			   distinct. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } ptr_to_member;
    /* When kind == tk_template_param (C++ front end only, except when
       prototype instantiations are passed to a back end): */
    struct {
      a_template_param_type_kind
		kind;
			/* The kind of template param type. */
      a_bit_field
		is_pack:1;
			/* TRUE if this is a template parameter pack.  This
			   is set for tpck_param types that represent
			   template parameter pack declarations. */
      a_bit_field
		is_generic_param:1;
			/* TRUE if this is a C++/CLI generic type parameter. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_bit_field
		being_checked:1;
			/* For a naked type parameter constraint, this is used
			   by the front end to detect recursive naked type
			   constraints. */
      a_bit_field
		is_generic_function_param:1;
			/* TRUE if this is a generic type parameter for a
			   C++/CLI generic function. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      a_bit_field
		is_auto_param:1;
			/* TRUE if this is a template parameter introduced by
			   a C++14 "auto" lambda parameter, or a C++17
			   "auto" or "decltype(auto)" nontype template
			   parameter. */
      a_bit_field
		is_decltype_auto:1;
			/* TRUE if this is a C++17 "decltype(auto)" nontype
			   template parameter. */
      a_bit_field
		originally_class_template_param:1;
			/* TRUE if this is a template parameter of an
			   implicit deduction guide that is based on a
			   template parameter from the enclosing class
			   template of the associated constructor. */
      a_bit_field
		is_unsigned_bit_precise_int:1;
			/* TRUE if this is a tptk_bit_precise_int type
			   representing unsigned _BitInt.  FALSE represents
			   signed _BitInt. */
      a_template_param_type_supplement_ptr
		extra_info;
			/* Pointer to a supplement containing additional
			   information about this template parameter type, */
    } template_param;
#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
    /* When kind is tk_vector. */
    struct {
      a_type_ptr
		element_type;
			/* Type of the vector elements. */
      a_constant_ptr
		size_constant;
			/* A constant representing the size expressed through
			   the vector_size or (when is_ext_vector_type is TRUE)
			   the ext_vector_type attribute.  NULL if the size was
			   not explicitly specified in the source (e.g., if the
			   type was formed with the "mode" attribute or if the
			   type was generated by the front end for a builtin
			   function). */
      a_bit_field
		is_boolean_vector:1;
			/* TRUE if this vector is the result of an operation
			   that produces a vector of boolean values in GNU
			   mode.  Such vectors permit more implicit
			   conversions.  This property is volatile; e.g., it
			   is not carried through decltype or deduction. */
      a_vector_kind
		kind;	/* The kind of the vector. */
    } vector;
    /* When kind is tk_scalable_vector: */
    struct {
      a_type_ptr
		element_type;
			/* Type of the vector elements. */
      uint8_t	tuple_elements;
			/* Number of tuple elements. */
    } scalable_vector;
    /* When kind is tk_riscv_vector: */
    struct {
      a_type_ptr
		element_type;
			/* Type of the vector elements. */
      int8_t	length_multiplier;
			/* Length multiplier.  A negative value represents a
			   fractional multiplier. */
      uint8_t	tuple_elements;
			/* Number of tuple elements. */
    } riscv_vector;
#endif /* GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED */
  } variant;
} a_type;


#if DEBUG
/*
Table of storage class names, for debug purposes.
*/
EXTERN_CONSTINIT_ARRAY(a_const_char*, db_storage_class_names, sc_last + 1)
#if VAR_INITIALIZERS
= { "unspecified", "extern", "static", "auto", "typedef", "register", "asm",
    "last" /* used to check that initialization is right. */
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(db_storage_class_names)
#endif /* DEBUG */

enum an_init_kind : a_byte {
  /* Kinds of initialization of a variable: */
  initk_none,		/* No initialization. */
  initk_static,		/* Static initialization to a constant. */
  initk_dynamic,	/* Dynamic initialization (code is required). */
  initk_zero,		/* Initialization to zero (static or dynamic).
			   Also serves to distinguish a tentative definition
			   from a real definition. */
  initk_function_local,	/* Either dynamic or aggregate-constant initialization
			   of a local static variable.  The variable itself
			   does not point at the initializer; rather the
			   initialization is represented by a local static
			   variable init entry. */
  initk_binding,	/* For the bindings in a structured binding, the
			   lvalue expression they stand for.  (This is not
			   an "initialization" in the traditional sense.) */
};


typedef union an_initializer *an_initializer_ptr;
typedef union an_initializer {
  /* Entry embedded in a variable or local-static-variable-init entry to
     indicate the initializer that is required.  Its variants are discriminated
     by the initialization kind specified in the containing entry. */
  /* When the initialization kind is initk_none, initk_zero, or
     initk_function_local, there are no variant fields. */
  /* When the initialization kind is initk_static: */
  a_constant_ptr
                constant;
			/* Constant initial value for static initialization.
			   May be a ck_aggregate constant, but only one that
			   is truly constant, i.e., one that does not contain
			   ck_dynamic_init constants.  Only used for static
			   variables.  The constant is unshared. */
  /* When the initialization kind is initk_dynamic: */
  a_dynamic_init_ptr
		dynamic;
			/* Pointer to an entry describing the dynamic
			   initialization required.  In the unusual case in
			   which no dynamic initialization is required
			   (variable receives default initialization or can
			   be statically initialized) but a destructor must
			   be called when the variable's lifetime terminates,
			   a dynamic init entry will also be supplied. */
  /* When the initialization kind is initk_binding: */
  an_expr_node_ptr
		bound_expr;
			/* The expression a binding is bound to (for bindings
			   that are not ordinary reference variables).  */
#if DO_IL_LOWERING
			/* This expression is left un-lowered and is copied
			   and lowered when used (to avoid memory region
			   issues). */
#endif /* DO_IL_LOWERING */
} an_initializer;


typedef struct a_local_static_variable_init *a_local_static_variable_init_ptr;
typedef struct a_local_static_variable_init {
  /* Description of the initialization of a local static variable.  The
     variable itself cannot point at its initializer, since the latter will
     be in the function scope memory region, and so this construct is used
     to represent the initialization.  These entries are always allocated in
     the function scope memory region and appear on a list pointed to by a
     function or block scope. */
  a_local_static_variable_init_ptr
		next;
			/* Pointer to the next in a linked list of entries
			   identifying local static variable initializations
			   in the current (function or block) scope. */
  a_variable_ptr
		variable;
			/* Pointer to an initialized local static variable
			   whose init_kind is initk_function_local. */
  an_init_kind	init_kind;
			/* Kind of initialization, if any.  Only initk_static
			   and initk_dynamic will occur. */
  an_initializer
		initializer;
			/* Union discriminated by init_kind and indicating the
			   initializer. */
  an_object_lifetime_ptr
		lifetime;
			/* An object lifetime that surrounds the initialization
			   of the variable.  Useful because it defines the
			   range within which the initialization of the
			   variable has to be undone and set up to be done
			   again if an exception is thrown.  NULL if not
			   needed (e.g., when exceptions are not enabled). */
} a_local_static_variable_init;


typedef struct a_vla_dimension *a_vla_dimension_ptr;
typedef struct a_vla_dimension {
  /* Description of the number of elements in a variable length array.  The
     VLA type itself cannot point at its dimension expression, since the
     latter will be in the function scope memory region, and so this
     construct is used to represent the array dimension.  These entries are
     always allocated in the function scope memory region and appear on a
     list pointed to by the function scope.  When making a copy of a VLA
     type a new a_vla_dimension entry must be created for it.  Since we
     cannot in general evaluate the dimension expression multiple times,
     a_vla_dimension entries for copies of VLA types point back to the
     original a_vla_dimension instead. */
  a_vla_dimension_ptr
		next;
			/* Pointer to the next in a linked list of entries
			   identifying VLA dimension expressions in the
			   current function. */
  a_type_ptr
		type;
			/* Pointer to a tk_array type entry for which the
			   is_variable_size_array and has_assoc_vla_dimension
			   flags are TRUE. */
  an_expr_node_ptr
		dimension_expr;
			/* An expression representing the number of elements
			   in the array.  NULL if this entry is for a
			   (compiler-generated) copy of a VLA type. */
  a_vla_dimension_ptr
		original_dimension;
			/* If this is an entry for a (compiler-generated) copy
			   of a VLA type, this field points to the entry
			   associated with the original VLA type.  Otherwise,
			   it is NULL. */
  a_byte_boolean
		in_prototype_scope;
			/* TRUE if the dimension expression is used in a
			   prototype scope, i.e., in a parameter type. */
  a_byte_boolean
		has_size_statement;
			/* TRUE if an stmk_set_vla_size statement pointing to
			   this entry was generated.  The dimension expression
			   is evaluated where that statement appears, so at
			   most one may be generated for an entry. */
  a_source_position
		position;
			/* Source position of the VLA expression. */
#if DO_IL_LOWERING
#if LOWER_VARIABLE_LENGTH_ARRAYS
  a_variable_ptr
		total_number_of_elements;
			/* A variable (produced by lowering of statements of
			   kind stmk_set_vla_size) holding the total number of
			   elements in this VLA type.  For example, if type is
			   int[n][4][m], then the variable would hold the
			   result of computing n*4*m at the appropriate time.
			   Valid only in the front end.  NULL until the
			   associated stmk_set_vla_size statement has been
			   lowered. */
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
  a_variable_ptr
		dimension_variable;
			/* A variable initialized with the original expression
			   recorded in dimension_expr.  It is used by the
			   C-generating back end to avoid duplicating side-
			   effects in VLA bounds.  For example, if a VLA is
			   originally of type "int[++k]" and the C-generating
			   back end must express this type several times, it
			   will do so as "int[n]" where "n" is the variable
			   recorded in this field (and initialized with "++k"
			   at the appropriate time).  NULL for entries not
			   pointed to by a stmk_set_vla_size statement. */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#endif /* DO_IL_LOWERING */
} a_vla_dimension;


/*
Entry containing additional information about variables that are
template-based (variable template instances and static data members of
class templates).
*/
typedef struct a_variable_template_info *a_variable_template_info_ptr;
typedef struct a_variable_template_info {
  a_template_arg_ptr
		template_arg_list;
			/* For variables that are instantiations of a variable
			   template, a list of entries describing the "actual
			   arguments" on which the instantiation is based.
			   If the variable is an instantiation of a partial
			   specialization, this argument list corresponds with
			   the template parameter list of the primary template.
			   This is NULL for static data members of class
			   templates. */
  a_template_arg_ptr
		partial_spec_template_arg_list;
			/* For variables that are instantiations of partial
			   specializations of a variable template, a list of
			   entries describing the arguments on which the
			   instantiation is based, with respect to the
			   template parameter list of the partial
			   specialization.  This is NULL for for variables
			   generated from the primary template, and for
			   static data members of class templates. */
  a_template_ptr
		assoc_template;
			/* The template on which the variable is based. */
} a_variable_template_info;


typedef struct a_variable {
  /* Description of a variable, including formal parameters of functions. */
  /* The source_corresp field must be first. */
  a_source_correspondence
                source_corresp;
                        /* Information on any source entity that corresponds
                           to this entity. */
  a_variable_ptr
                next;
                        /* Pointer to the next variable declared in the same
                           scope, NULL if this variable is the last in the
                           scope. */
  a_type_ptr    type;
			/* Type of the variable. */
  union {
    /* When is_struct_binding and is_struct_binding_container are FALSE: */
    a_param_type_ptr
		assoc_param_type;
			/* If is_parameter is TRUE and is_this_parameter is
			   FALSE, points to the associated a_param_type entry.
			   NULL otherwise. */
#if DO_IL_LOWERING
			/* Left NULL for implicit parameters added by IL
			   lowering. */
#endif /* DO_IL_LOWERING */
    /* When is_struct_binding is TRUE: */
    a_variable_ptr
		container;
			/* The unnamed variable to which this binding
			   refers. */
    /* When is_struct_binding_container is TRUE: */
    an_il_entity_list_entry_ptr
		bindings;
			/* A list of entries pointing to the a_variable entries
			   representing the associated bindings. */
  } variant;
  a_storage_class
                storage_class;
                        /* Storage class.  The storage class is not necessarily
			   what was written in the source program; it is
			   standardized to show the effective storage class
			   rather than the keyword that appeared. */
			/* Note that the C concept of "storage class" is used
			   also in C++.  Other C++ uses of a storage class
			   are not reflected in this field.  "static" on a
			   class member, for example, is a storage class
			   syntactically, but has a different effect, which
			   is represented elsewhere (e.g., in the use of
			   a field for a nonstatic data member and a variable
			   for a static data member). */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_storage_class
		declared_storage_class;
			/* The storage class that explicitly appears in the
			   source when the variable is defined; sc_unspecified
			   if the definition has no explicit storage class
			   (including a variable representing a static data
			   member) or if there is no definition in the current
			   translation unit (if there are only C "tentative
			   definitions", the first is treated as a definition
			   in this context).  For a declaration that is not a
			   definition, the declared storage class is recorded
			   in the a_src_seq_secondary_decl entry associated
			   with that declaration. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DECL_MODIFIERS_IN_USE
  a_decl_modifier_set
		decl_modifiers;
			/* Additional declaration information supplied by
			   nonstandard language features such as the
			   Microsoft storage-class-like __declspec
			   modifiers. */
#endif /* DECL_MODIFIERS_IN_USE */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED ||          \
    NAMED_REGISTERS_ALLOWED
  union {
    a_const_char
		*name;
			/* If non-NULL, and asm_name_is_valid is TRUE
			   this is the name to be used as an assembly
			   language level symbol for this variable. */
#if GNU_EXTENSIONS_ALLOWED
    a_named_register
		reg;
			/* If both has_named_register_storage_class and
                           asm_name_is_valid are FALSE, the register
			   to which this variable should be assigned. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
    a_named_register_id
		id;
			/* If has_named_register_storage_class is TRUE, the
			   id of the register in which this variable is
			   stored. */
#endif /* NAMED_REGISTERS_ALLOWED */
  } asm_name_or_reg;
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED || ... */
  a_targ_alignment
  		alignment;
			/* The explicit alignment specified for the
			   variable, or zero if there was no explicit
			   alignment. */
#if GNU_EXTENSIONS_ALLOWED
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  a_gnu_init_priority
		init_priority;
			/* The initialization priority specified by the GNU
			   attribute "init_priority."  This value should lie
			   between 101 and 65535 inclusive, or should be zero
			   if the attribute was not specified. */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  a_routine_ptr
		cleanup_routine;
			/* The cleanup routine that should be called when the
			   variable (which must be an automatic variable) goes
			   out of scope.  (Currently, the front end does not
			   make that call explicit in any way.) */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  ENUM_TYPE_FOR_BIT_FIELD(an_ELF_visibility_kind)
		ELF_visibility:3;
			/* The visibility of the variable in the generated
			   ELF object code. */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  a_bit_field   is_weak:1;
			/* TRUE if this variable was declared with the
			   weak or weakref attribute. */
  a_bit_field	is_weakref:1;
			/* TRUE if this variable was declared with the 
			   weakref attribute.*/
  a_bit_field	is_gnu_alias:1;
			/* TRUE if this variable was declared with the
			   alias attribute. */
  a_bit_field   has_gnu_used_attribute:1;
			/* TRUE if this variable was declared with the
			   GNU "used" attribute. */
  a_bit_field   has_gnu_abi_tag_attribute:1;
			/* TRUE if this variable was explicitly declared with a
			   GNU "abi_tag" attribute, or has implicit "abi_tag"
			   attributes. */
  a_bit_field   is_not_common:1;
			/* TRUE if this variable was marked with the GNU
			   "nocommon" attribute, which indicates is should not
			   be placed in "COMMON" (or an equivalent) storage
			   (even if it has a "tentative definition").
			   Note that GCC appears to ignore the attribute in
			   many cases (e.g., in C++): This flag is TRUE even
			   in those cases -- it only indicates that the
			   attribute appeared. */
  a_bit_field	is_common:1;
			/* TRUE if this variable was marked with the GNU
			   "common" attribute. */
  a_bit_field	has_internal_linkage_attribute:1;
			/* TRUE if this variable was marked with the Clang
			   "internal_linkage" attribute. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
  a_bit_field   asm_name_is_valid:1;
			/* TRUE if the name field of asm_name_or_reg
			   is valid; FALSE if the reg field is valid. */
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if NAMED_REGISTERS_ALLOWED
  a_bit_field   has_named_register_storage_class:1;
			/* TRUE if the id field of asm_name_or_reg is
			   valid, which indicates that the variable was
			   declared with an Embedded C (TR 18037) named
			   register storage class. */
#endif /* NAMED_REGISTERS_ALLOWED */
  a_bit_field
		used:1;
			/* TRUE if the variable was directly used or had
			   its address taken.  Volatile variables are always
			   considered "used" in this way. */
  a_bit_field	address_taken:1;
                        /* TRUE if the address of this variable has been
                           taken somewhere. */
  a_bit_field	is_parameter:1;
                        /* TRUE if this is a parameter of a function. */
  a_bit_field	is_struct_binding:1;
			/* TRUE if this is a binding variable in a structured
			   binding declaration.  Such variables appear on the
			   "bindings" list of the associated container
			   variable, but not on the "entities" list of the
			   associated stmk_decl entry. */
  a_bit_field	is_struct_binding_container:1;
			/* TRUE if this is the underlying variable referred to
			   by the structured binding variables. */
  a_bit_field	referenced_non_locally:1;
			/* TRUE if the variable is a local static variable
			   that is referenced from outside of its function
			   (e.g., from a member function of a local class).
			   TRUE only in C++. */
  a_bit_field	modified_within_try_block:1;
			/* TRUE if the variable is a local variable that is
			   modified within a try block and declared in a scope
			   containing that try block.  This flag enables a
			   back end to treat such variables as requiring
			   immediate store after a modification.  C++ only. */
  a_bit_field	is_template_variable:1;
			/* TRUE if this is an instance of a variable
			   template or a static data member that is a
			   member of a class generated from a template,
			   including both the case where the static data member
			   is generated from the template and the case where a
			   specialization of the static data member is
			   provided by the user.  FALSE for all other cases,
			   including a static data member of a class that
			   is a specialization of a template class. */
  a_bit_field	is_prototype_instantiation:1;
			/* TRUE if this variable represents the prototype
			   instantiation of a variable template or a static
			   data member of a class template.  Also TRUE for a
			   variable declared during the prototype
			   instantiation of a function template, unless that
			   variable has linkage and a nondependent type. */
  a_bit_field	is_nonreal:1;
			/* TRUE if this is a variable template instance that
			   resulted from an instantiation using a dependent
			   argument list. */
  a_bit_field	is_specialized:1;
			/* TRUE when is_template_variable is TRUE
			   but the definition is supplied independently of
			   the template with which it is associated.  This
			   flag may be set as a result of a specialization
			   declaration (either an old-style declaration or
			   one using the template<> syntax), or if the static
			   data member was specified in a do-not-instantiate
			   pragma. */
  a_bit_field	specialized_with_old_syntax:1;
			/* TRUE if is_specialized is TRUE but the static
			   data member was not explicitly declared with the
			   template<> syntax. */
  a_bit_field	explicit_instantiation:1;
			/* TRUE if an instantiation has been explicitly
			   requested using an explicit instantiation directive
			   or an instantiation pragma. */
  a_bit_field	class_explicitly_instantiated:1;
			/* TRUE if the instantiation request specified the
			   class (meaning that all its members should be
			   instantiated). */
  a_bit_field	explicit_do_not_instantiate:1;
			/* TRUE if instantiation has been explicitly 
			   suppressed by an "extern template" directive or
			   a do_not_instantiate pragma. */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  a_bit_field	can_be_instantiated:1;
			/* TRUE if this is a variable template instance or
			   template static data member that could be
			   instantiated by this compilation.  FALSE if
			   is_template_variable is FALSE.  This flag is
			   provided in the IL so that a back end can pass the
			   information along to a link-time automatic
			   instantiation mechanism.  The flag is only set
			   very late in the compilation process and should
			   not be relied upon for any other purpose. */
  a_bit_field	do_not_instantiate:1;
			/* TRUE if a do_not_instantiate pragma or "extern
			   template" directive was present for this variable
			   template instance or template static data member.
			   FALSE if is_template_variable is FALSE.  This flag
			   is provided in the IL so that a back end can pass
			   the information along to a link-time automatic
			   instantiation mechanism.  The flag is only set very
			   late in the compilation process and should not be
			   relied upon for any other purpose. */
  a_bit_field	instance_required:1;
			/* TRUE for a variable template instance or static
			   data member of a template class for which a
			   definition (either template generated or a specific
			   definition) must be supplied in this compilation
			   unit or in another compilation unit with which this
			   unit will be linked.  Implies that the static data
			   member is referenced in this compilation.  FALSE if
			   is_template_variable is FALSE.  This flag is
			   provided in the IL so that a back end can pass the
			   information along to a link-time automatic
			   instantiation mechanism.  The flag is only set very
			   late in the compilation process and should not be
			   relied upon for any other purpose. */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  a_bit_field	param_value_has_been_changed:1;
			/* TRUE if is_parameter or is_handler_param is TRUE
			   and the variable is assigned to or has had its
			   address taken at least once within the body of the
			   routine or handler. */
#if MINIMAL_INLINING
  a_bit_field	param_used_as_lvalue:1;
			/* TRUE if is_parameter or is_handler_param is TRUE
			   and the variable is used as an lvalue at least once
			   in the lowered code for the routine or handler. */
#endif /* MINIMAL_INLINING */
  a_bit_field	param_used_more_than_once:1;
			/* TRUE if is_parameter or is_handler_param is TRUE
			   and the variable is used more than once within
			   the body of the routine or handler. */
  a_bit_field	is_handler_param:1;
			/* TRUE if the variable is a handler parameter (C++
			   only). */
  a_bit_field	is_this_parameter:1;
			/* TRUE if the variable represents a "this" parameter
			   (C++ only). */
  a_bit_field	is_anonymous_parent_object:1;
			/* TRUE if type is the type of an anonymous union --
			   this variable is the "parent object" of which the
			   anonymous union members are subobjects.  For
			   example, given
			     union { int i, j; };
			   the IL to represent the source construct "i" is
			   "<anonymous-parent-object>.i". */
  a_bit_field	is_member_constant:1;
			/* TRUE if the variable represents a static data member
			   for which an initializer was specified at its
			   declaration within the class definition.  In modes
			   that delay parsing of the initializers of static
			   data member instantiations (e.g., GNU C++ mode), the
			   flag is set to TRUE only after that instantiation is
			   done.  (It is referred to as a "member constant" in
			   part because it can be used in constant expressions
			   elsewhere in the class definition.) */
  a_bit_field	is_constexpr:1;
			/* TRUE if this is a static data member declared with
			   the "constexpr" specifier, or if this is another
			   kind of variable defined with that specifier. */
  a_bit_field	declared_constinit:1;
			/* TRUE if this entity was declared with the C++20
			   "constinit" keyword. */
  a_bit_field	is_inline:1;
			/* TRUE if this is an inline variable (C++17).  This
			   may be explicitly set or implicitly set (because
			   a static data member is constexpr). */
  a_bit_field	on_inline_variable_list:1;
			/* TRUE if this variable has been added to the inline
			   variable list. */
  a_bit_field	suppress_inline_definition:1;
			/* This flag is TRUE when is_inline is TRUE, and when
			   it is also the case that this inline variable's
			   definition should not be emitted by the back end
			   because INSTANTIATE_INLINE_VARIABLES is TRUE (i.e.,
			   when inline variables are instantiated using a
			   mechanism similar to the template instantiation
			   mechanism). */
#if INSTANTIATE_INLINE_VARIABLES
  a_bit_field	inline_instance_required:1;
			/* TRUE for an inline variable if the variable was
			   referenced in a way that requires a definition of
			   the inline variable somewhere in the complete
			   program. */
#endif /* INSTANTIATE_INLINE_VARIABLES */
  a_bit_field	superseded_external:1;
			/* TRUE (in SVR4 C mode only) if the current variable
			   was created to represent a block extern declaration
			   whose type is incompatible with that of another
			   file-scope variable with the same name, where the
			   latter is treated as the "official" variable. */
  a_bit_field	has_variably_modified_type:1;
			/* The type of the variable is a variably modified
			   type, i.e., is or contains a VLA type.  Any variable
			   for which this flag is set will also be specified
			   in a stmk_vla_decl statement, which indicates where
			   in the execution stream the declaration fits. */
  a_bit_field	is_vla:1;
			/* The variable is a variable length array, i.e., its
			   type is a VLA type.  This flag is TRUE only if
			   has_variably_modified_type is also TRUE.  The
			   associated stmk_vla_decl statement (and, in C mode,
			   one or more enk_vla_dealloc nodes), indicates where
			   in the execution stream its memory is to be
			   allocated (or deallocated). */
#if DO_IL_LOWERING
  a_bit_field	initialization_rewritten_as_assignment:1;
			/* TRUE if IL lowering has rewritten some part of
			   the initialization for this variable as assignment
			   statements or the like. */
#if MINIMAL_INLINING
  a_bit_field	is_temp_for_unmodified_inlined_param:1;
			/* TRUE if this variable is a temporary introduced by
			   inlining as the remapping for a parameter that
			   was not modified in the body of the function. */
  a_bit_field	is_temp_for_constructor_this_inlined_param:1;
			/* TRUE if this variable is a temporary introduced by
			   inlining as the remapping for the "this" parameter
			   of a constructor. */
#endif /* MINIMAL_INLINING */
  a_bit_field	promoted_local_static_init:1;
			/* TRUE if this variable is a local static variable
			   with an attached a_local_static_initialization
			   entry that has been promoted out of its
			   function. */
  a_bit_field	promoted_local_static:1;
			/* TRUE if this variable is a local static variable
			   that has been promoted out of its function. */
  a_bit_field	is_optional_vtable:1;
			/* TRUE if this variable is a virtual function table
			   and it is "optional" -- the vtable heuristic does
			   not require it to be put out.  Means "even though
			   I'm an external definition, I don't need to be put
			   out unless referenced."  Also used for typeinfo
			   and typeinfo string variables. */
  a_bit_field	vtable_defined:1;
			/* TRUE if the virtual function table for this vtable
			   variable has been defined. */
  a_bit_field	lowering_generated:1;
			/* TRUE if this variable was created during the
			   lowering process. */
#endif /* DO_IL_LOWERING */
  a_bit_field	is_compound_literal:1;
			/* TRUE if this variable was generated by the front
			   end to hold a static compound literal. */
  a_bit_field	has_explicit_initializer:1;
			/* TRUE if this variable has an explicit
			   initializer. */
  a_bit_field	has_parenthesized_initializer:1;
			/* TRUE if this variable is initialized with a
			   parenthesized initializer; FALSE indicates an
			   "="-form initializer, a braced initializer, an
			   implicit initializer, or no initializer at all. */
  a_bit_field	has_direct_braced_initializer:1;
			/* TRUE if this variable is initialized with a braced
			   initializer immediately following the declarator
			   (i.e., something like "int x{1};" but not
			   "int x = {1};"). */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
  a_bit_field	has_flexible_array_initializer:1;
			/* TRUE if the variable has a type with a flexible
			   array member and the variable is initialized with
			   an aggregate initializer that includes values for
			   the flexible array member.  This may require a
			   back end to allocate more storage for the variable
			   than what is indicated by its type's size. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
  a_bit_field	uninitialized:1;
			/* TRUE if the variable is defined without an
			   initializer and default initialization has no
			   effect. */
  a_bit_field	declared_with_auto_type_specifier:1;
			/* TRUE if the variable's declaration contains the
			   type specifier (not the storage class specifier)
			   "auto". */
  a_bit_field	declared_with_decltype_auto:1;
			/* TRUE if the variable declaration contains the
			   decltype(auto) specifier. */
  a_bit_field	declared_with_class_template_placeholder:1;
			/* TRUE if the variable declaration contains a class
			   template placeholder. */
#if BACK_END_IS_CP_GEN_BE
  a_bit_field	declaration_has_been_put_out:1;
			/* Used in the C++-generating back end to control the
			   storage class specifier used with a redeclaration,
			   to work around a bug in Sun C++ compilers. */
  a_bit_field	definition_has_been_put_out:1;
			/* Used in the C++-generating back end to control
			   the storage class specifier used with a C
			   non-definition declaration, to work around a
			   Microsoft bug. */
#endif /* BACK_END_IS_CP_GEN_BE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_bit_field	embedded_source_sequence_entries:1;
			/* TRUE if the definition of this variable embeds
			   another construct with associated source sequence
			   entries.  For example:
			     int x[sizeof(struct S { int i; })];
			   or
			     void *p = (struct S { int i; }*)0;
			   In these examples, the source sequence entries for
			   the non-autonomous struct S are considered to be
			   "embedded".  In such cases, the embedded entries are
			   followed by an a_src_seq_end_of_construct for this
			   variable. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_bit_field	declared_using_type_without_linkage:1;
			/* In C++, TRUE for variables with linkage (but not
			   extern "C" linkage) that were declared using
			   types without linkage. */
  a_bit_field	is_pack:1;
			/* TRUE for a structured binding pack or for the
			   parameter variable for a function parameter pack of
			   a variadic template. */
  a_bit_field	is_pack_element:1;
			/* TRUE for parameters of an actual instantiation of
			   a variadic template for those parameters that are
			   associated with a parameter pack of the original
			   variadic template. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_initonly:1;
			/* TRUE if the "initonly" context-sensitive keyword
			   appeared on the declaration of this static data
			   member (C++/CLI only).  (Always FALSE for entries
			   that do not represent a static data member.) */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	is_enhanced_for_iterator:1;
			/* TRUE for the iterator variable of a for-each
			   loop (in Microsoft or C++/CLI modes) or a
			   range-based-for (in C++11 modes). */
  a_bit_field	initializer_in_class:1;
			/* TRUE for static data members with in-class
			   initializers. */
  a_bit_field	constant_valued:1;
			/* TRUE for variables of a const type initialized with
			   a constant expression so that uses of the variable's
			   value are permitted in constant-expressions.  Also
			   TRUE for variables in prototype instantiations that
			   might end up with those properties after template
			   instantiation.  Only set in C++. */
  a_bit_field	is_immutable:1;
			/* TRUE if the variable is of a const type that does
			   not contain a mutable subobject. */
  a_bit_field	is_thread_local:1;
			/* TRUE for variables declared with the "thread_local"
			   (or "_Thread_local in C mode) storage class (i.e.,
			   variable has thread storage duration).  Not used for
			   variables declared with "__thread" (see DM_THREAD).
			   It is best to use the is_effective_thread_local
			   macro (see below) rather than accessing this flag
			   directly. */
  a_bit_field
		extends_lifetime:1;
			/* TRUE if this is a reference variable bound to a
			   temporary causing that temporary to have its
			   lifetime extended. */
  a_bit_field
		is_template_param_object:1;
			/* TRUE if this is a C++20 "template parameter object";
			   i.e., a constexpr variable backing a template
			   argument of class type. */
  a_bit_field
		compiler_generated:1;
			/* TRUE if this is a compiler-generated variable. */
  a_bit_field	is_in_class_specialization:1;
			/* TRUE if this is a specialized template instance
			   and the specialization was declared within the
			   enclosing class. */
  an_init_kind	init_kind;
			/* Kind of initialization, if any.
			   When init_kind == initk_function_local (local
			   static variables only), the initializer is
			   indicated by a local-static-variable-init entry
			   on a linked list for the current function or block
			   scope. */
  an_initializer
		initializer;
			/* Union discriminated by init_kind and indicating the
			   initializer. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		initializer_range;
			/* When an initializer appears explicitly in the
			   source, the source positions corresponding to the
			   start and end of the top-level initializer
			   construct (i.e., including "=" or "(" and ")").
			   Otherwise, null_source_range. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  an_il_entity_list_entry_ptr
		entities_defined_in_initializer;
			/* A list of entities defined in the initializer
			   associated with this variable, if this is a static
			   data member.  Currently, this list only has C++11
			   closure types. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_property_or_event_descr_ptr
		property_or_event_descr;
			/* Pointer to the description of the associated event
			   or property (only non-NULL for static C++/CLI events
			   and properties). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_variable_template_info_ptr
		template_info;
			/* For instances of variable templates and static
			   data members of class templates, this points to
			   additional information.  NULL otherwise. */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  a_const_char	*section;
			/* If non-NULL, the GNU "section" or Microsoft "segment
			   name" in which this variable should be placed
			   (specified by __attribute((section(...))) and
			   __declspec(allocate(...)), respectively). */
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  a_variable_ptr
		aliased_variable;
			/* If non-NULL, the variable for which this variable
			   is an alias.  (Used for attributes "alias" and
			   "weakref".) */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  a_const_char	*comdat_group;
			/* The COMDAT group into which this variable
			   should be placed, or NULL if this entity
			   should not be placed into a COMDAT group.
			   Non-NULL only for variable definitions, never for
			   (e.g.) external references. */
  a_variable_ptr
		vla_element_count_variable;
			/* A variable holding the count of a VLA's elements
			   (NULL if this is not a VLA variable).  The run-time
			   support for VLAs lowered in C++ mode requires this
			   value (stored at run time). */
#endif /* DO_IL_LOWERING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_type_ptr	declared_type;
			/* The type as it actually appears in the declaration
			   of the variable at the point of its definition;
			   NULL if there is no defining declaration.  When
			   is_parameter is TRUE, the type is what actually
			   appeared in the parameter declaration -- e.g.,
			   before an array decays to a pointer.  (The only
			   exception is a parameter variable of a function
			   template instantiation, where the type declared
			   in the template may involve a template
			   parameter.) */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ONE_INSTANTIATION_PER_OBJECT
  unsigned long	instantiation_needed_bit_number;
			/* When a separate "needed" flag is maintained for
			   each instantiation, this is the "needed" bit number
			   associated with this (static data member) variable.
			   0 if there is no associated bit. */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if MINIMAL_INLINING
  struct a_variable_remapping_for_inlining
		*remapping_for_inlining;
			/* If non-NULL, points to information about remapping
			   that currently applies to this variable for copies
			   done for inlining.  Front end only. */
#endif /* MINIMAL_INLINING */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS || \
    USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
  union {
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
    /* When is_thread_local is FALSE: */
    a_routine_ptr
		dynamic_init_routine;
			/* If non-NULL, a pointer to the initialization
			   routine for any dynamic initialization required to
			   initialize this variable. */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
    /* When is_thread_local is TRUE: */
    struct {
      a_routine_ptr
                init_routine;
			/* If non-NULL, a pointer to the initialization routine
			   (or more likely an alias for routine that does the
			   actual dynamic initialization) for this thread_local
			   variable. */
      a_routine_ptr
                wrapper;
			/* If non-NULL, a pointer to the wrapper routine to
			   call for dynamic initialization of this
			   thread_local variable. */
    } thread;
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
  } init_routine;
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS || USE_LAZY_... */
#if MAINTAIN_NEEDED_FLAGS && !GENERATE_EH_TABLES
  a_variable_ptr
                eff_class_typeinfo_var;
                        /* If non-NULL, a pointer to the typeinfo variable
                           for the pointed-to class type.  Only set for
                           typeinfo variables of pointer-to-class type. */
#endif /* MAINTAIN_NEEDED_FLAGS && !GENERATE_EH_TABLES */
} a_variable;

/*
Macro used to test whether a variable should be treated as thread_local.  In
configurations where IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS is FALSE, the
is_thread_local flag will be TRUE in the IL, but is_effective_thread_local will
always return FALSE.
*/
#if IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS
#define is_effective_thread_local(var) ((var)->is_thread_local)
#else /* !IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS */
#define is_effective_thread_local(var) (FALSE)
#endif /* IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS */

/*
Data structures related to fields (members) of structs and unions:
*/
typedef unsigned char an_offset_bit_remainder;
			/* To represent the excess (relative to the byte
			   offset) in the offset of a bit field.  The value
			   will be >= 0 and < targ_char_bit. */

#if RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL
typedef unsigned char a_bit_field_container_offset;
			/* To represent the byte offset of a bit field in its
			   container when targ_microsoft_bit_field_allocation 
			   is TRUE. */
#endif /* RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL */

typedef struct a_field {
  /* Description of a field (member of a class, struct, or union), including
     unnamed bit fields. */
  /* The source_corresp field must be first. */
  a_source_correspondence
                source_corresp;
                        /* Information on any source entity that corresponds
                           to this entity.  Note that more than one field
                           entry can point to the same symbol if a struct
                           or union type is copied to file scope. */
  a_field_ptr   next;
                        /* Pointer to the next field declared in the same
                           struct/union, NULL if this field is the last in the
                           struct/union. */
  a_type_ptr    type;
                        /* Type of the field.  For bit fields, this is the
                           base type. */
  a_targ_size_t	offset;
			/* Offset of this field from the start of the struct
			   (in bytes).  Zero for members of unions.  If the
			   field is a bit field, this value is the byte-offset
			   component of the actual offset. */
  an_offset_bit_remainder
		offset_bit_remainder;
			/* If the field is a bit field, this is the offset of
			   the start of the bit field within the byte specified
			   by offset.  Always zero if it is not a bit field;
			   otherwise always >= 0 and < targ_char_bit.  In
			   other words, the offset of a bit field within the
			   struct is the combination of offset (its byte-offset
			   component) and offset_bit_remainder. */
  a_byte	bit_size;
			/* Size of this field (in bits).  Only non-zero for
			   bit-fields; for the others, the size is gotten from
			   the type. */
#if RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL
  a_bit_field_container_offset
		offset_in_container;
			/* When targ_microsoft_bit_field_allocation is TRUE and
			   this is a bit field, the number of whole bytes that
			   the bit field is offset from its container's origin.
			   Otherwise zero. */
#endif /* RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL */
  a_targ_alignment
  		alignment;
			/* The explicit alignment specified for the
			   field, or zero if there was no explicit
			   alignment. */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	is_packed:1;
			/* TRUE if the field was declared with the GNU "packed"
			   attribute. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if IA64_ABI
  a_bit_field	offset_is_set:1;
			/* TRUE if the offset for this field has been set. */
#endif /* IA64_ABI */
  a_bit_field	is_bit_field:1;
			/* TRUE if the field represents a bit field. */
  a_bit_field	bit_field_is_signed:1;
			/* TRUE if the field is a signed bit field. */
  a_bit_field	is_anonymous_parent_object:1;
			/* TRUE if type is the type of an anonymous union --
			   this field is the "parent object" of which the
			   anonymous union members are subobjects.  For
			   example, given
			     class A { union { int i, j; }; } x;
			   the IL to represent the source construct "x.i" is
			   "x.<anonymous-parent-object>.i". */
  a_bit_field	is_mutable:1;
			/* TRUE if the "mutable" specifier appeared on the
			   declaration of this nonstatic data member (C++
			   only). */
  a_bit_field	compiler_generated:1;
			/* TRUE for fields that are created by the compiler
			   and have not been declared in the source,
			   e.g., the virtual function table pointer. */
  a_bit_field	is_init_capture:1;
			/* TRUE if this is a field of a closure type that was
			   generated for a C++14-style init-capture. */
  a_bit_field	is_captured_this:1;
			/* TRUE if this is a field of a closure type that was
			   generated to capture a "this" parameter. */
  a_bit_field	is_captured_pack_element:1;
			/* TRUE if this is a field of a closure type that was
			   generated to capture a variadic function template's
			   parameter pack element or an init-capture that
			   is a pack.  In such cases, there may be multiple
			   fields with the same name. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_initonly:1;
			/* TRUE if the "initonly" context-sensitive keyword
			   appeared on the declaration of this nonstatic data
			   member (C++/CLI only). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	vla_treated_as_zero_length_array:1;
			/* TRUE if the field was declared as a variable-length
			   array (which is normally invalid for field types),
			   but treated as a zero-length array by the front
			   end. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  a_bit_field	is_lowered_base_class:1;
			/* TRUE if this field was added by IL lowering to
			   represent a base class subobject. */
  a_bit_field	class_subobject_with_tail_padding:1;
			/* TRUE if this field was added by IL lowering to
			   represent a base class subobject or if the field
			   has a class type and the [[no_unique_address]]
			   attribute, and if the base or field class type
			   has tail padding in which members following the
			   subobject may be allocated. */
#endif /* DO_IL_LOWERING */
  a_bit_field	has_initializer:1;
			/* TRUE if a C++11-style initializer was specified for
			   this field. */
  a_bit_field	init_is_ctor_dependent:1;
			/* TRUE if the default member initializer associated
			   with this field requires a constructor-specific copy
			   and cannot be used generally (e.g., an initializer
			   containing a call to
			   std::source_location::current()). */
  a_bit_field	has_direct_braced_initializer:1;
			/* TRUE if a direct braced initializer was specified
			   for this field. */
  a_bit_field	has_nonconstant_initializer:1;
			/* TRUE if a C++11-style initializer was specified for
			   this field, and the initializer is known not to be
			   a constant expression.  Note that this means
			   "includes something that rules out a constant
			   expression", and not "the initializer dynamic-init
			   is not a constant"; an initializer like f() could be
			   not-yet-foldable when the initializer is scanned,
			   but foldable to a constant later when a constexpr
			   constructor is called. */
  a_bit_field	bit_size_constant_expr_in_local_expr_node_ref:1;
			/* TRUE if the expression for the bit-field width
			   contains a reference to a local variable.
			   Because such expressions cannot appear in
			   file-scope memory, the expression is represented
			   as an a_local_expr_node_ref in the function
			   scope and the associated expr field will be
			   NULL.  This applies to bit_size_constant->expr. */
  a_bit_field	has_no_unique_address_attribute:1;
			/* TRUE if the field has the [[no_unique_address]]
			   attribute applied to it. */
  a_bit_field	is_optimized_empty_class:1;
			/* TRUE if the field is an empty class that has been
			   "optimized" such that it potentially shares an
			   address with another field in the class.  Similar to
			   is_optimized_empty_base, except that it applies to a
			   field and not a base class.  Such fields do not
			   exist in the lowered struct, so lowering needs to
			   rewrite any expressions that may refer to them.
			   Only TRUE if has_no_unique_address_attribute is also
			   TRUE. */
  a_dynamic_init_ptr
		initializer;
			/* The initializer specified on the field (initializers
			   on nonstatic data members are a C++11 feature).
			   NULL if there is no such initializer.  Note that,
			   in configurations that do lowering, this initializer
			   is not lowered.  Rather, when the field is used in
			   a context where initialization is required, the
			   unlowered initializer is copied to the appropriate
			   location and lowered at that point.  See
			   copy_non_static_data_member_initializers_if_
                                                                  necessary. */
  an_il_entity_list_entry_ptr
		entities_defined_in_initializer;
			/* A list of entities defined in the initializer
			   associated with this field.  Currently, this list
			   only has C++11 closure types. */
  a_constant_ptr
		bit_size_constant;
			/* An IL constant representing the size of the bit
			   field.  (NULL if this is not a bit field.) */ 
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_property_or_event_descr_ptr
		property_or_event_descr;
			/* Non-NULL only if this field represents a Microsoft
			   property or event.  In the case of a property, it
			   may have been declared using an attribute (i.e.,
			   __declspec(property(...))) or using C++/CLI
			   syntax. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  unsigned long	declared_bit_size;
			/* If is_bit_field is TRUE, the declared size of the
			   bit field, which may be longer than the value given
			   by bit_size (in C++ only).  For example, given 
			     int i : 2043; 
			   this field will contain 2043, and bit_size will
			   be the number of bits in an int, which is the
			   maximum permitted size for an int bit field. */
#if BACK_END_IS_C_GEN_BE
  a_type_ptr	bit_field_alignment_type;
			/* For a bit field where the declared_bit_size is
			   larger than bit_size, the integer type whose
			   alignment is to be applied to the bit field, if
			   that concept applies in the ABI (e.g., the IA-64
			   ABI). NULL otherwise, and NULL until the containing
			   class type has been laid out. */
#endif /* BACK_END_IS_C_GEN_BE */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		initializer_range;
			/* When the field has an in-class initializer, the
			   source positions corresponding to the start and end
			   of that initializer (i.e., including "=" or "{" and
			   "}").  Otherwise, null_source_range. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} a_field;


#if BUILTIN_FUNCTIONS_ENABLED

/*
An enumeration of the different builtin function categories.

If you add a new target-specific function category, also update builtin_tables
in builtin_defs.h.
*/
enum a_builtin_function_category : a_byte {
  bfc_none,		/* No builtin function table. */
  bfc_common,		/* Builtin function that is common for all
			   architectures. */
  bfc_arm,		/* ARM specific builtin function. */
  bfc_arm_32,		/* 32-bit ARM specific builtin function. */
  bfc_arm_32_mve,	/* 32-bit ARM specific builtin function (arm_mve.h). */
  bfc_arm_64,		/* 64-bit ARM specific builtin function. */
  bfc_arm_64_acle,	/* 64-bit ARM specific builtin function
			   (arm_acle.h). */
  bfc_arm_64_neon,	/* 64-bit ARM specific builtin function
			   (arm_neon.h). */
  bfc_arm_64_neon_sve_bridge,
			/* 64-bit ARM specific builtin function
			   (arm_neon_sve_bridge.h). */
  bfc_arm_64_sme,	/* 64-bit ARM specific builtin function (arm_sme.h). */
  bfc_arm_64_sve,	/* 64-bit ARM specific builtin function (arm_sve.h). */
  bfc_riscv,		/* RISC-V specific builtin function. */
  bfc_riscv_vector,	/* RISC-V specific builtin function
			   (riscv_vector.h). */
  bfc_riscv_andes_vector,
			/* RISC-V Andes vector builtin function
			   (andes_vector.h). */
  bfc_riscv_sifive_vector,
			/* RISC-V SiFive vector builtin function
			   (sifive_vector.h). */
  bfc_riscv_32,		/* 32-bit RISC-V specific builtin function. */
  bfc_riscv_32_vector,	/* 32-bit RISC-V specific builtin function
			   (riscv_vector.h). */
  bfc_riscv_32_andes_vector,
			/* 32-bit RISC-V Andes vector builtin function
			   (andes_vector.h). */
  bfc_riscv_32_sifive_vector,
			/* 32-bit RISC-V SiFive vector builtin function
			   (sifive_vector.h). */
  bfc_riscv_64,		/* 64-bit RISC-V specific builtin function. */
  bfc_riscv_64_vector,	/* 64-bit RISC-V specific builtin function
			   (riscv_vector.h). */
  bfc_riscv_64_andes_vector,
			/* 64-bit RISC-V Andes vector builtin function
			   (andes_vector.h). */
  bfc_riscv_64_sifive_vector,
			/* 64-bit RISC-V SiFive vector builtin function
			   (sifive_vector.h). */
  bfc_x86,		/* x86 specific builtin function. */
  bfc_x86_32,		/* 32-bit x86 specific builtin function. */
  bfc_x86_64,		/* 64-bit x86 specific builtin function. */
  bfc_keyword,		/* A keyword treated as a builtin function. */
  bfc_user,		/* User-supplied builtin function. */
  bfc_last
};

/* Type used to store an enumeration value used to identify the kind of
   builtin function.  Must be large enough to accommodate enum values from both
   a_builtin_function_kind_tag and a_builtin_user_function_kind.  */
typedef unsigned short a_builtin_function_kind;

/* Type used for an index into a builtin function table.  Defined as
   "unsigned int" (due to the size of some of the vector builtin function
   tables). */
typedef unsigned int a_builtin_function_index;

#endif /* BUILTIN_FUNCTIONS_ENABLED */

/*
An enumeration of the different kinds of constructor and destructor entry
points.  These alternate entry points are generated by IL lowering for
the IA64 ABI.  See also is_inheriting_ctor as that flag also dictates different
variations of constructors.
*/
enum a_ctor_or_dtor_kind : a_byte {
  cdk_none,		/* A constructor or destructor as originally created
			   by lowering. */
#if IA64_ABI
  cdk_complete,		/* A version of a constructor or destructor for a
			   complete object. */
  cdk_subobject,	/* A version of a constructor or destructor for a
			   subobject. */
  cdk_deleting,		/* A version of a destructor that destroys a
			   complete object and then deletes the storage
			   associated with the object. */
  cdk_delegation,	/* A version of a delegating constructor that is
			   invoked by both complete object and subobject
			   versions.  This is an EDG extension -- it is not
			   part of the IA-64 ABI.  Also used for a destructor
			   that invokes either the complete or subobject
			   destructor at run-time depending on the value
			   of the VTT parameter. */
#endif /* IA64_ABI */
  cdk_last
};


typedef struct a_requires_clause *a_requires_clause_ptr;
typedef struct a_requires_clause {
  an_expr_node_ptr
		constraint;
			/* The expression describing the constraint. */
  a_source_position
		requires_pos;
			/* The position of the "requires" keyword. */
} a_requires_clause;


/*
Data structures related to routines:
*/
typedef struct a_routine {
  /* Description of a routine.  Note that this is pointed to from a scope
     block, and the local variables (etc.) are declared there. */
  /* The source_corresp field must be first. */
  a_source_correspondence
                source_corresp;
                        /* Information on any source entity that corresponds
                           to this entity. */
  a_routine_ptr next;
                        /* Pointer to the next routine declared in the same
                           scope, NULL if this routine is the last in the
                           scope. */
  a_type_ptr    type;
                        /* Type of this routine.  Points to a type entry with
                           kind == tk_routine (or to a typeref that refers to
                           such a type), which gives the return type and
                           parameter information. */
  a_function_def_number
                function_def_number;
			/* If not NULL_function_def_number, this indicates the
			   function definition descriptor that is used to
			   determine the scope containing local declarations
			   and executable statements of the function.  This
			   is non-NULL only if the routine has a body.  If
			   this field is non-NULL and the "defined" flag is
			   FALSE during front-end processing, it means that
			   scanning the function body has begun but is not
			   yet complete.  See also the note about discarded
			   function bodies under the "defined" flag.  See
			   also prototype_scope under
			   a_routine_type_supplement. */
  a_memory_region_number
                memory_region;
                        /* If not NULL_region_number, this indicates the
                           memory region containing the function definition.
                           This is non-NULL only if the routine has a body. */
  a_hash_value
                hash_value;
                        /* A hash value computed for this routine, or
                           zero if a hash has not yet been computed. */
  a_storage_class
                storage_class;
                        /* Storage class.  The storage class is not necessarily
			   what was written in the source program; it is
			   standardized to show the effective storage class
			   rather than the keyword that appeared.
			   Note that the C concept of "storage class" is used
			   also in C++.  Other C++ uses of a storage class
			   are not reflected in this field.  "static" on a
			   class member, for example, is a storage class
			   syntactically, but has a different effect, which
			   is represented elsewhere (e.g., in the
			   this_class field for a routine type). */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_storage_class
		declared_storage_class;
			/* The storage class as it appeared on the definition;
			   sc_unspecified if there is no definition.  (For
			   non-defining declarations, the storage class is
			   recorded in the
			   corresponding a_src_seq_secondary_decl entry.) */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_special_function_kind
		special_kind;
			/* An enumerator indicating the special member function
			   kind of which this routine is an instance (e.g.,
			   constructor, destructor); sfk_none when it is an
			   ordinary member function or not a member function
			   at all. */
  union {
    /* When special_kind == sfk_udl_operator, no variant fields. */
    /* When special_kind == sfk_operator. */
    an_opname_kind
		opname_kind;
			/* An enumerator indicating the kind of operator when
			   the special function kind is sfk_operator; onk_none
			   otherwise. */
#if BUILTIN_FUNCTIONS_ENABLED
    /* When special_kind == sfk_none. */
    a_builtin_function_kind
                builtin_function_kind;
			/* An enumerator indicating the kind of GNU-style
			   builtin function; bfk_none for an ordinary
			   function. */ 
#endif /* BUILTIN_FUNCTIONS_ENABLED */
#if IA64_ABI && DO_IL_LOWERING
    /* When special_kind == sfk_constructor or sfk_destructor. */
    struct {
      a_routine_list_entry_ptr
		alternate_entry_points;
			/* For a constructor or destructor with entry points,
			   a list of the entry points.  Can be non-NULL only in
			   the primary routine, i.e., the one with
			   primary_ctor_or_dtor == NULL.  Only valid within
			   the front end. */
      sizeof_t	base_name_offset;
			/* Once the name has been mangled, the offset into
			   the mangled name for this constructor/destructor
			   of the "C"/"D" that indicates that this entity is a
			   constructor/destructor.  (This value is used when
			   calculating the mangled names for alternate entry
			   points.) */
    } ctor_dtor;
#endif /* IA64_ABI && DO_IL_LOWERING */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* When special_kind == sfk_property_get, sfk_property_set, sfk_event_add,
       sfk_event_remove, or sfk_event_raise. */
    a_property_or_event_descr_ptr
		property_or_event_descr;
			/* Pointer to the description of the associated
			   property or event. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* When special_kind == sfk_lambda_entry_point. */
    a_routine_ptr
		lambda_call_operator;
			/* The lambda call operator for which this entry is
			   an alternate entry point. */
    /* When special_kind == sfk_deduction_guide. */
    a_template_ptr
		class_template;
			/* The class template for which this is a deduction
			   guide. */
  } variant;
  a_bit_field	address_taken:1;
			/* TRUE if the address of this routine has been
			   taken somewhere. */
  a_bit_field	is_virtual:1;
			/* TRUE for virtual member functions (i.e., member
			   functions declared with a "virtual" specifier or
			   member functions that are virtual because they
			   match a virtual member function in a base class).
			   (C++ only.) */
  a_bit_field	overrides_base_member:1;
			/* TRUE for virtual member functions that are known to
			   override at least one virtual function in a base
			   class.  (To find the overridden functions, see
			   overriding_virtual_functions in a_base_class.) */
  a_bit_field	pure_virtual:1;
			/* TRUE for virtual member functions declared with a
			   "pure" specifier (C++ only).  TRUE only if
			   is_virtual is also TRUE. */
  a_bit_field	final:1;
			/* TRUE for a virtual member function that cannot be
			   overridden in a derived class.  (Declared using the
			   context-sensitive keyword "final" or "sealed", or
			   using the attribute "final".) */
  a_bit_field	override:1;
			/* TRUE for a virtual member function that was
			   declared with the function-modifier "override". */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	abstract:1;
			/* TRUE for a virtual member function that was
			   declared with the function-modifier "abstract" (in
			   that case pure_virtual is TRUE too). */
  a_bit_field	sealed:1;
			/* TRUE for a virtual member function that was
			   declared with the function-modifier "sealed" (in
			   that case final is TRUE too). */
  a_bit_field	new_member:1;
			/* TRUE for a member function that was declared with
			   the function-modifier "new". */
  a_bit_field	interface_slot:1;
			/* TRUE for member functions generated to represent a
			   compiler-generated "slot" in a Microsoft interface
			   class.  For example:
			      __interface B { virtual void f() = 0; };
			      __interface C1: B {};
			      __interface C2: B {};
			      struct D: C1, C2 {
				void C1::f();  // Creates a "slot" in C1.
			        void C2::f();  // Creates a "slot" in C2.
			      };
			   */
  a_bit_field	definition_cannot_be_generated:1;
			/* TRUE for compiler-generated special members whose
			   definition cannot be generated because the
			   corresponding special member in a subobject is
			   not callable (e.g., because it is inaccessible).
			   Set only in some Microsoft modes to determine
			   whether the definition of such members should be
			   forced for dllexported classes. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	covariant_return_virtual_override:1;
			/* TRUE if is_virtual is TRUE and this routine is an
			   overriding virtual function with a covariant
			   return type (C++ only). */
  a_bit_field	is_inline:1;
			/* TRUE for functions that were specified in the
			   source as candidates for inlining (either by the
			   "inline" keyword or definition within a C++ class
			   definition).  This flag is intended as a hint to
			   the compiler and does not mean that inlining is
			   required.  Also TRUE when the Microsoft-specific
			   specifiers __inline or __forceinline were used (in
			   which case the variant is also recorded in the
			   decl_modifiers field).  If the routine is a
			   template instance, this flag is sometimes not
			   set until the routine is either instantiated or
			   explicitly specialized.  Consequently, to
			   determine if a routine would be inline if
			   instantiated, the is_inline_template_function and
			   rout_is_inline_template_function can be used. */
  a_bit_field	is_declared_constexpr:1;
			/* TRUE for functions that were declared with the
			   C++11 "constexpr" specifier. */
  a_bit_field	is_constexpr:1;
			/* TRUE for "constexpr" functions.  For non-template
			   user-declared functions this usually equals the
			   is_declared_constexpr flag, but for template
			   functions it may be cleared if an instantiation
			   turns out not to meet the "constexpr" constraints.
			   It can also be TRUE for generated default
			   constructors (for which is_declared_constexpr is
			   FALSE).  "consteval" functions also have this flag
			   set since they are also "constexpr" functions. */
  a_bit_field	is_consteval:1;
			/* TRUE for functions that were declared with the
			   C++20 "consteval" specifier. */
  a_bit_field	is_constexpr_intrinsic:1;
			/* TRUE for certain standard library functions that the
			   front end knows how to evaluate independently from
			   the actual definition in the library (e.g.,
			   "std::is_constant_evaluated"). */
  a_bit_field	compiler_generated:1;
			/* TRUE for functions that are created by the compiler
			   and have not been declared in the source, e.g.,
			   default constructors in C++ or previously-unknown
			   identifiers appearing in a call-expression in C89
			   mode.  If a valid declaration is found in the
			   source -- as could for example be true of "operator
			   delete" -- the flag will be cleared; hence the bit
			   is not necessarily TRUE for "intrinsic" routines. */
  a_bit_field	defined:1;
			/* TRUE once the definition of the function has been
			   completed.  (While the function body is being
			   scanned, "defined" remains FALSE.)  Note that
			   for some functions (e.g., trivial default
			   constructors), the body is removed immediately after
			   it has been processed, so defined is TRUE when
			   memory_region == NULL_region_number. */
  a_bit_field	called:1;
			/* TRUE if this routine is directly called.
			   For virtual functions in C++, this indicates that
			   the routine was named in a call, although maybe
			   an overriding routine might be called instead. */
  a_bit_field	is_explicit_constructor:1;
			/* TRUE if this routine is a constructor or deduction
			   guide (i.e., its special_kind is sfk_constructor or
			   sfk_deduction_guide) and the "explicit" keyword
			   appeared in its declaration.  C++ only.  See also
			   the flag is_conditionally_explicit in routine type
			   supplements. */
  a_bit_field	is_explicit_conversion_function:1;
			/* TRUE if this routine is a conversion function and
			   the "explicit" keyword appeared in its declaration.
			   See also the flag is_conditionally_explicit in
			   routine type supplements. */
  a_bit_field	is_trivial_default_constructor:1;
			/* TRUE if this routine is a trivial default
			   constructor (implicitly generated or defaulted).
			   Such a constructor has no effect, and hence calls
			   to it can be elided.  C++ only. */
  a_bit_field	is_trivial_copy_function:1;
			/* TRUE if this routine is a trivial copy or move
			   constructor or a trivial copy or move assignment
			   operator.  The operation performed by such a
			   routine is a bitwise copy.  C++ only. */
  a_bit_field	is_trivial_destructor:1;
			/* TRUE if this routine is a trivial destructor
			   (implicitly generated or defaulted).  Such a 
			   constructor has no effect, and hence calls to it
			   can be elided.  C++ only. */
  a_bit_field	is_initializer_list_ctor:1;
			/* TRUE if this routine is an initializer list
			   constructor. */
  a_bit_field	is_delegating_ctor:1;
			/* TRUE if this routine is a delegating constructor
			   (which can only be known if the constructor
			   definition has been seen). */
  a_bit_field	is_inheriting_ctor:1;
			/* TRUE if this routine is an inheriting
			   constructor. */
  a_bit_field	inherits_virtually:1;
			/* TRUE if is_inheriting_ctor is TRUE and the inherited
			   constructor comes from a virtual base class of the
			   class that owns this constructor. */
  a_bit_field	is_deduction_guide_from_inheriting_ctor:1;
			/* TRUE if this routine is a deduction guide generated
			   from an inheriting constructor. */
#if ASSIGNMENT_TO_THIS_ALLOWED
  a_bit_field	assignment_to_this_done:1;
			/* TRUE if an assignment to "this" (an anachronism)
			   was done in this function.  C++ member functions
			   only. */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  a_bit_field	is_template_function:1;
			/* TRUE for instances and specializations of function
			   templates, instances and specializations of member
			   function templates, and instances and
			   specializations of member functions -- except
			   generated special members -- of generated template
			   class instances.  FALSE for all other functions,
			   including a function that is a member (but not a
			   member template) of a class that is a specialization
			   of a template class. */
  a_bit_field	is_specialized:1;
			/* TRUE when is_template_function is TRUE but the
			   function definition is supplied independently of
			   the template with which it is associated.  This
			   flag may be set as a result of a specialization
			   declaration (either an old-style declaration or
			   one using the template<> syntax), or if the
			   function was specified in a do-not-instantiate
			   pragma. */
  a_bit_field	specialized_with_old_syntax:1;
			/* TRUE if is_specialized is TRUE but the function
			   was not explicitly declared with the template<>
			   syntax. */
  a_bit_field	is_prototype_instantiation:1;
			/* TRUE if this routine represents the prototype
			   instantiation of a function template or a member
			   function of a class template.  It is also TRUE
			   for other kinds of dependent function declarations,
			   such as member functions of a local class that
			   is defined in a dependent context, and block extern
			   functions that are declared with dependent
			   types. */
  a_bit_field	never_throws:1;
			/* TRUE for routines declared with the attribute
			   "nothrow" or the C++11-style "noexcept" construct;
			   this is an assertion by the programmer that the
			   routine will not throw an exception (the front end
			   does not check that assertion). */
  a_bit_field	is_in_class_specialization:1;
			/* TRUE if this is a specialized template instance
			   and the specialization was declared within the
			   enclosing class. */
  a_bit_field	explicit_instantiation:1;
			/* TRUE if an instantiation has been explicitly
			   requested using an explicit instantiation directive
			   or an instantiation pragma. */
  a_bit_field	class_explicitly_instantiated:1;
			/* TRUE if the instantiation request specified the
			   class (meaning that all its members should be
			   instantiated). */
  a_bit_field	explicit_do_not_instantiate:1;
			/* TRUE if instantiation has been explicitly 
			   suppressed by an "extern template" directive or
			   a do_not_instantiate pragma. */
  a_bit_field	has_nodiscard_attribute:1;
			/* TRUE if the routine has the "nodiscard" standard
			   attribute applied to it. */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	never_inline:1;
			/* TRUE for routines declared with the "noinline"
			   attribute; this indicates that a code generator
			   should never attempt to inline calls to this
			   routine. */
  a_bit_field	is_pure:1;
			/* TRUE if this routine was declared with the
			   pure attribute. */
#if GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_naked:1;
			/* TRUE if this routine was declared with the "naked"
			   attribute (indicating that a code generator should
			   not generate a prologue or epilogue for this
			   routine). */
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	declared_only_as_friend:1;
			/* TRUE if this routine has only been declared as a
			   friend.  In that case, Microsoft compilers will
			   not treat this as a specialization of any
			   template. */
  a_bit_field	explicit_extern_inline:1;
			/* TRUE if the routine was explicitly declared with
			   both the "extern" and "inline" specifiers.  In
			   Microsoft C++ mode, this forces the definition to
			   be spilled. */
  a_bit_field	direct_linkage_specifier_on_nondef_decl:1;
			/* TRUE if any non-definition declaration of the
			   routine has a direct (i.e., non-brace form)
			   linkage specification.  In Microsoft C++ mode,
			   such a declaration forces the definition of an
			   inline function to be spilled, similar to the
			   effect of the preceding flag.  (The presence,
			   absence, and form of a linkage specification on
			   the definition has no effect.) */
  a_bit_field	is_reverse_conversion_function:1;
			/* TRUE for a C++/CLI static conversion operator that
			   converts from the argument to the enclosing class
			   type. */
  a_bit_field 	is_generic_definition:1;
			/* TRUE if this is the routine that resulted from
			   the initial scanning of a C++/CLI generic function.
			   This is similar to a prototype instantiation of
			   a template except that generics do not make use
			   of dependent types. */
  a_bit_field	is_generic_instance:1;
			/* TRUE if this is an instantiation of a C++/CLI
			   generic function. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  ENUM_TYPE_FOR_BIT_FIELD(an_ELF_visibility_kind)
		ELF_visibility:3;
			/* The visibility of the routine in the generated
			   ELF object code. */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  a_bit_field	is_initialization_routine:1;
			/* TRUE if this routine was declared with the
			   constructor attribute. */
  a_bit_field	is_finalization_routine:1;
			/* TRUE if this routine was declared with the
			   destructor attribute. */
  a_bit_field	is_weak:1;
			/* TRUE if this routine was declared with the
			   weak or weakref attribute. */
  a_bit_field	is_weakref:1;
			/* TRUE if this routine was declared with the
			   weakref attribute. */
  a_bit_field	is_gnu_alias:1;
			/* TRUE if this routine was declared with the
			   alias attribute. */
  a_bit_field	is_ifunc:1;
			/* TRUE if this routine was declared with the
			   ifunc attribute.  When TRUE,
			   gnu_extra_info->aliased_routine points to the
			   resolver function. */
#if LOWER_IFUNC
			/* is_ifunc (and gnu_extra_info->aliased_routine) stay
			   set even when the routine has been lowered. */
#endif /* LOWER_IFUNC */
  a_bit_field	has_gnu_used_attribute:1;
			/* TRUE if this routine was declared with the
			   GNU "used" attribute. */
  a_bit_field	has_gnu_abi_tag_attribute:1;
			/* TRUE if this routine was declared with an explicit
			   GNU "abi_tag" attribute, or has implicit "abi_tag"
			   attributes. */
  a_bit_field	in_gnu_abi_tag_namespace:1;
			/* TRUE if this routine has a parent inline namespace
			   with a GNU "abi_tag" attribute. */
  a_bit_field	implicit_abi_tags_added:1;
			/* TRUE if the processing to determine implicit
			   "abi_tag" attributes has been performed for this
			   routine. */
  a_bit_field	allocates_memory:1;
			/* TRUE if this routine was declared with the
			   malloc attribute.  Such a routine should
			   return a pointer to newly allocated
			   storage. */
  a_bit_field	no_instrument_function:1;
			/* TRUE if a code generator should not instrument the
			   routine for execution profiling. */
  a_bit_field	no_check_memory_usage:1;
			/* TRUE if a code generator should not instrument the
			   routine for checking memory access. */
  a_bit_field	always_inline:1;
			/* TRUE for routines declared with the GNU attribute
			   "always_inline"; this indicates that a code
			   generator should attempt to inline calls to this
			   routine even at the lowest optimization levels. */
  a_bit_field	gnu_c89_inline:1;
			/* TRUE if this is an "inline" routine declared with
			   the GNU attribute "gnu_inline". */
  a_bit_field	implicit_alias:1;
			/* TRUE if this routine is implicitly an alias for
			   another routine (indicated by
			   gnu_extra_info->aliased_routine).
			   (E.g., a "strlen" declaration may be implicitly
			   treated as an alias for "__builtin_strlen".) */
  a_bit_field	has_internal_linkage_attribute:1;
			/* TRUE if this routine was marked with the Clang
			   "internal_linkage" attribute. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  a_bit_field	can_be_instantiated:1;
			/* TRUE if this is a template function
			   that could be instantiated by this compilation.
			   FALSE if is_template_function is FALSE.
			   This flag is provided in the IL so that a
			   back end can pass the information along to
			   a link-time automatic instantiation mechanism.
			   The flag is only set very late in the compilation
			   process and should not be relied upon for any
			   other purpose. */
  a_bit_field	do_not_instantiate:1;
			/* TRUE if a do_not_instantiate pragma was present
			   for this template function.
			   FALSE if is_template_function is FALSE.
			   This flag is provided in the IL so that a
			   back end can pass the information along to
			   a link-time automatic instantiation mechanism.
			   The flag is only set very late in the compilation
			   process and should not be relied upon for any
			   other purpose. */
  a_bit_field	instance_required:1;
			/* TRUE for a template function or member function of
			   a template class for which a definition (either
			   template generated or a specific definition)
			   must be supplied in this compilation unit or in
			   another compilation unit with which this unit
			   will be linked.  Implies that the function is
			   referenced in this compilation.  FALSE if
			   is_template_function is FALSE.
			   This flag is provided in the IL so that a
			   back end can pass the information along to
			   a link-time automatic instantiation mechanism.
			   The flag is only set very late in the compilation
			   process and should not be relied upon for any
			   other purpose. */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  a_bit_field	contains_try_block:1;
			/* TRUE if the routine has a definition that contains
			   at least one "try" block.  This may affect
			   optimization relating to local variables of the
			   routine. */
  a_bit_field	contains_local_class_type:1;
			/* TRUE if the routine has a definition that contains
			   a local class, struct, or union declaration. */
  a_bit_field	superseded_external:1;
			/* TRUE (in some C modes only) if the current routine
			   was created to represent a block extern declaration
			   or implicit declaration whose type is incompatible
			   with that of file-scope routine with the same name.
			   The latter is treated as the "official" routine. */
  a_bit_field	defined_in_friend_decl:1;
			/* TRUE when the routine definition appears in a
			   friend declaration.  When this flag is set, a
			   source sequence entry pointing to this routine
			   will correspond to a friend declaration. */
  a_bit_field	defined_outside_of_parent:1;
			/* TRUE for a routine that is defined in a scope other
			   than the scope to which it really belongs -- i.e.,
			   a class member function defined outside the class
			   definition or a namespace member defined outside
			   the namespace definition.  It does not apply to a
			   friend declaration that supplies a definition. */
#if DO_IL_LOWERING && MINIMAL_INLINING
  a_bit_field	inlinable:1;
			/* TRUE if this routine can be inlined.  Starts out as
			   TRUE if is_inline is TRUE, then turned off if an
			   attempt to inline the routine discovers something
			   it cannot handle.  Also turned off temporarily
			   if inlining of this routine is temporarily
			   suppressed, e.g., because it's currently being
			   inlined. */
#endif /* DO_IL_LOWERING && MINIMAL_INLINING */
#if MAINTAIN_NEEDED_FLAGS
  a_bit_field	definition_needed:1;
			/* TRUE if this routine is "needed" (see the flag by
			   that name in the source_corresp field), but not
			   merely as a declaration -- a definition of the
			   routine is needed in the current translation
			   unit. */
  a_bit_field	keep_definition_in_il:1;
			/* TRUE if this routine's definition should be kept in
			   the IL tree (i.e., should not be discarded before
			   the IL is passed to the back end).  It is for
			   front-end use only. */
#endif /* MAINTAIN_NEEDED_FLAGS */
  a_bit_field	expl_template_arg_list_used:1;
			/* TRUE if an explicit template argument list was ever
			   used in naming this (template) function. */
#if BACK_END_IS_CP_GEN_BE
  ENUM_TYPE_FOR_BIT_FIELD(a_name_linkage_kind)
		surrounding_name_linkage_state:NUM_BITS_FOR_NAME_LINKAGE;
			/* Name linkage in effect when this routine was
			   defined.  Used by the C++-generating back end to
			   reconstruct name linkage blocks when appropriate
			   (e.g., extern "C" { static int f() { ... } }). */
  a_bit_field	definition_C_name_linkage_specified:1;
			/* TRUE if C name linkage for the definition was
			   specified explicitly (i.e., in a direct linkage
			   specifier or via a containing linkage block), as
			   opposed to being inherited from a preceding
			   declaration. */
  a_bit_field	definition_has_direct_linkage_specifier:1;
			/* TRUE if the definition itself has a linkage
			   specifier (extern "C" void f() { }) rather than
			   simply inheriting it from a preceding declaration
			   or from the surrounding linkage block. */
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_bit_field	has_been_defined:1;
			/* TRUE if the definition for this function has been
			   emitted.  This is used to work around a Microsoft
			   bug that does not allow an explicit specialization
			   for a conversion function template to be declared
			   but not defined. */
  a_bit_field	evaluated_in_interpreter:1;
			/* TRUE if a call to this function completed in the
			   interpreter.  This is useful to decide whether it
			   is safe to declare as "constexpr" an implicit
			   instance rendered as an explicit specialization. */
  a_bit_field	suppress_explicit_specialization:1;
			/* TRUE if this is an instance of a function
			   template and a generated explicit specialization
			   would be invalid for some reason.  Set by both
			   the front end and the C++-generating back end,
			   as required. */
  a_bit_field	need_for_template_args_determined:1;
			/* Some compilers have bugs that require explicit
			   template argument lists to be put out for some
			   explicit specializations to be matched with the
			   correct template; however, there are also cases in
			   which explicit template arguments cannot appear.
			   The function args_needed_for_compiler_bugs does the
			   requisite analysis.  A TRUE value of this flag
			   indicates that the template_args_required flag below
			   reflects the result of that analysis and the
			   function need not be called again. */
  a_bit_field	template_args_required:1;
			/* TRUE if explicit template arguments should be
			   included when putting out a declaration for a
			   generated explicit specialization for this
			   routine.  This flag is only valid when
			   need_for_template_args_determined is TRUE. */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* BACK_END_IS_CP_GEN_BE */
  a_bit_field	definition_for_inlining_only:1;
			/* TRUE for an inline function that has been defined
			   in a way that indicates that the definition is only 
			   to be used for inlining purposes; i.e., a back end
			   should not emit a stand-alone definition of the
			   function.  It is set in C99 mode for so-called
			   "inline definitions" (a definition of an inline
			   function never declared with "extern" in the same
			   translation unit), in GNU C modes for inline
			   functions defined with the "extern" keyword, in
			   GNU C++ modes for inline functions declared with
			   the gnu_inline attribute, and in Microsoft mode
			   for inline functions defined with the dllimport
			   attribute.  See also the suppress_inline_body flag
			   below. */
#if INSTANTIATE_EXTERN_INLINE
  a_bit_field	inline_instance_required:1;
			/* TRUE for an inline function if the function was
			   referenced in a way that requires a definition of
			   the body of the inline function somewhere in the
			   complete program.  This flag is set for all
			   routines because a routine can be declared inline
			   after it has been called. */
#endif /* INSTANTIATE_EXTERN_INLINE */
  a_bit_field	suppress_inline_body:1;
			/* This flag is TRUE when definition_for_inlining_only
			   (see above) is TRUE, but also when the front end
			   has determined for reasons not directly apparent in
			   the source that this inline function's definition 
			   should not be emitted by the back end, particularly
			   when INSTANTIATE_EXTERN_INLINE is TRUE (i.e.,
			   when inline functions are instantiated using a
			   mechanism similar to the template instantiation
			   mechanism). */
  a_bit_field	on_inline_function_list:1;
			/* TRUE if this routine has been added to the inline
			   function list. */
  a_bit_field	need_out_of_line_copy:1;
			/* TRUE if an out-of-line copy of this inline routine
			   is needed, e.g., because its address was taken, or
			   it was named in an explicit instantiation
			   directive. */
  ENUM_TYPE_FOR_BIT_FIELD(a_stdc_pragma_value)
		fp_contract:NUM_BITS_FOR_STDC_PRAGMA_VALUE;
			/* In C99 mode, the setting of the fp_contract mode
			   at the point that this routine was defined. */
  ENUM_TYPE_FOR_BIT_FIELD(a_stdc_pragma_value)
		fenv_access:NUM_BITS_FOR_STDC_PRAGMA_VALUE;
			/* In C99 mode, the setting of the fenv_access mode
			   at the point that this routine was defined. */
  ENUM_TYPE_FOR_BIT_FIELD(a_stdc_pragma_value)
		cx_limited_range:NUM_BITS_FOR_STDC_PRAGMA_VALUE;
			/* In C99 mode, the setting of the cx_limited_range
			   mode at the point that this routine was defined. */
#if FIXED_POINT_ALLOWED
  ENUM_TYPE_FOR_BIT_FIELD(a_stdc_pragma_value)
		fx_full_precision:NUM_BITS_FOR_STDC_PRAGMA_VALUE;
			/* The setting of the fx_full_precision state at the
			   the point that this routine was defined. */
  ENUM_TYPE_FOR_BIT_FIELD(a_stdc_pragma_value)
		fx_fract_overflow:NUM_BITS_FOR_STDC_PRAGMA_VALUE;
			/* The setting of the fx_fract_overflow state at the
			   the point that this routine was defined. */
  ENUM_TYPE_FOR_BIT_FIELD(a_stdc_pragma_value)
		fx_accum_overflow:NUM_BITS_FOR_STDC_PRAGMA_VALUE;
			/* The setting of the fx_accum_overflow state at the
			   the point that this routine was defined. */
#endif /* FIXED_POINT_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  a_bit_field	upc_access_method:NUM_BITS_FOR_UPC_ACCESS;
			/* In UPC mode, the UPC access method set at the
			   point this routine was defined. */
#endif /* UPC_EXTENSIONS_ALLOWED */
  a_bit_field	contains_statement_expression:1;
			/* TRUE if this routine's body contains one or more
			   statement expressions, i.e., ({...}), a GNU
			   extension. */
#if IA64_ABI
  a_bit_field	inline_in_class_definition:1;
			/* TRUE if this routine is a member of a class and was
			   declared inline (explicitly or implicitly) in
			   the class definition. */
#endif /* IA64_ABI */
#if DO_IL_LOWERING && IA64_ABI
  a_bit_field	use_comdat:1;
			/* TRUE if this routine should be placed in a COMDAT
			   group.  The group used should be the same as the
			   mangled name of the routine.	 TRUE only for
			   routines with definitions, never for (e.g.)
			   external references. */
  ENUM_TYPE_FOR_BIT_FIELD(a_ctor_or_dtor_kind)
		ctor_dtor_kind:3;
			/* The kind of constructor or destructor.  cdk_none
			   for other kinds of routines.  All constructors and
			   destructors are given a kind other than cdk_none.
			   Constructor and destructor routines created by
			   the front end proper with kind cdk_none are changed
			   to an appropriate kind during lowering, and entry
			   points added by lowering are created with the right
			   kind. */
  a_bit_field	is_alias_entry:1;
			/* TRUE if this routine is an entry point that is an
			   alias for the primary routine pointed to by
			   primary_ctor_or_dtor.  In other words, the
			   code for this entry point does nothing more than
			   call the primary routine passing the same
			   parameters. */
#endif /* DO_IL_LOWERING && IA64_ABI */
#if DO_IL_LOWERING
  a_bit_field	lowering_delayed_on_nested_function:1;
			/* TRUE if the lowering for this routine is to be
			   delayed because lowering of a nested function was
			   delayed. */
  a_bit_field   has_no_effect:1;
                        /* TRUE if this routine (a constructor or destructor)
                           is known to have no effect and therefore calls to it
                           can be eliminated during lowering.  Set to FALSE
                           initially (safe value) and only set to TRUE if the
                           lowered routine has been inspected and found to
                           have no effect. */
#endif /* DO_IL_LOWERING */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
  a_bit_field	statics_have_been_promoted:1;
			/* TRUE if, in C++, statics have already been promoted
			   from the scope associated with this routine. */
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
  a_bit_field	is_lambda_body:1;
			/* TRUE if this is the operator() member function
			   generated for the body of a lambda. */
  a_bit_field	declared_using_type_without_linkage:1;
			/* In C++, TRUE for routines with linkage (but not
			   extern "C" linkage) that were declared using
			   types without linkage. */
  a_bit_field	is_defaulted:1;
			/* In C++, TRUE if this is a special member function
			   or a comparison function declared with the
			   "= default" syntax.  For class members, if
			   defined_outside_of_parent is FALSE, the "= default"
			   appeared on the in-class declaration and on the
			   out-of-class definition otherwise.  For friend
			   comparison operators, the "= default" appeared on
			   the in-class definition if defined_in_friend_decl
			   is TRUE. */
  a_bit_field	is_deleted:1;
			/* In C++, TRUE if this is a function declared with
			   the "= delete" syntax.  Also TRUE for functions
			   that are explicitly defaulted but implicitly
			   defined as deleted and for compiler-generated
			   functions that should behave as if they had been
			   declared with the "= delete" syntax. */
  a_bit_field	contains_local_static_variable:1;
			/* TRUE if the function body contains at least one
			   local static variable. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_bit_field	embedded_source_sequence_entries:1;
			/* TRUE if the definition of this routine embeds in its
			   declarator another construct with associated source
			   sequence entries.  For example:
			     int (*f())[sizeof(struct { int x; })] { ... }
			   In this example, the source sequence entry for the
			   struct definition is considered "embedded".  In
			   such cases, the embedded entries are followed by an
			   a_src_seq_end_of_construct for this routine.  A
			   similar flag exists in a_src_seq_secondary_decl for
			   declarations of functions that are not
			   definitions. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_bit_field	considered_decider_function_at_some_point:1;
			/* TRUE if at some point in the compilation this
			   function was considered the decider ("key")
			   function of a class for the virtual function table
			   generation decision.  This is intended for front-end
			   use only, to catch cases where a function was the
			   decider and then becomes not the decider because of
			   an out-of-class inline definition. */
  a_bit_field	is_raw_literal_operator:1;
			/* TRUE if this routine is a raw literal operator,
			   i.e., a literal operator with one parameter of
			   type const char*, and FALSE otherwise. */
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
  a_bit_field	is_tls_init_alias:1;
                        /* TRUE if this routine is an alias for the
                           thread_local initialization routine for the
                           translation unit. */
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
  a_bit_field	is_tls_init_routine:1;
                        /* TRUE if this routine is used to initialize
                           thread_local variables.  Typically there is
                           at most one such routine per translation
                           unit, but that's not true when
                           ONE_INSTANTIATION_PER_OBJECT is TRUE (in which case
                           there may be one per slice). */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  a_bit_field   has_ctor_priority:1;
                        /* TRUE if the GNU "constructor" attribute has been
                           used to assign a numeric priority to the routine.
                           The gnu_extra_info->ctor_priority field contains the
                           priority.  FALSE if the attribute was not specified,
                           or if the attribute was specified without an
                           argument. */
  a_bit_field   has_dtor_priority:1;
                        /* TRUE if the GNU "destructor" attribute has been
                           used to assign a numeric priority to the routine.
                           The gnu_extra_info->dtor_priority field contains the
                           priority.  FALSE if the attribute was not specified,
                           or if the attribute was specified without an
                           argument. */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  a_bit_field	has_deducible_return_type:1;
			/* TRUE if the return type of this function contains
			   "auto" or "decltype(auto)" (this excludes the "auto"
			   specifier followed by a matching trailing return
			   type). */
  a_bit_field	has_deduced_return_type:1;
			/* TRUE if has_deducible_return_type is TRUE and the
			   actual return type has been deduced. */
  a_bit_field	contains_generic_lambda:1;
			/* TRUE if the routine contains a generic lambda
			   (directly or in another lambda or local class). */
  a_bit_field	is_coroutine:1;
			/* TRUE if the definition of this function is
			   resumable (i.e., it is a coroutine).   Additional
			   information is recorded in the first statement of
			   the function's top-level compound statement (a
			   stmk_coroutine entry). */
  a_bit_field	is_top_level_in_mem_region:1;
			/* TRUE if this is the top-level function in a
			   memory region.  The memory region can be
			   freed when the processing of this routine is
			   finished. */
  a_bit_field	friend_defined_in_instantiation:1;
			/* TRUE if this routine was defined in a friend
			   declaration in an instantiated class.  This
			   is primarily intended to identify functions
			   defined in class templates, but will also be
			   TRUE for friends of local classes where the classes
			   are defined in some kind of instantiation
			   context. */
  a_bit_field	is_ineligible:1;
			/* TRUE for constrained ordinary member functions of
			   class templates when the constraint is not
			   satisfied.  This flag is set on-demand and should
			   therefore always be queried through the function
			   is_ineligible. */
  a_bit_field	has_pass_object_size_attr:1;
			/* TRUE if any parameter was declared with the Clang
			   pass_object_size attribute. */
  a_bit_field	from_injected_tokens:1;
			/* TRUE if the function was declared via injected
			   tokens. */
#if DECL_MODIFIERS_IN_USE
  a_decl_modifier_set
		decl_modifiers;
			/* Additional declaration information supplied by
			   nonstandard language features such as the
			   Microsoft storage-class-like __declspec
			   modifiers. */
#endif /* DECL_MODIFIERS_IN_USE */
  a_requires_clause_ptr
		trailing_requires_clause;
			/* If this is a constrained templated function, the
			   associated trailing requires clause if any (it is
			   always the original parameterized constraint, not
			   the substituted one).  Otherwise, NULL. */
  union {
    /* When is_constexpr_intrinsic is FALSE: */
    a_virtual_function_number
		virtual_function;
			/* When is_virtual is TRUE and is_consteval is FALSE,
			   the number assigned to this function; it is unique
 			   among the non-consteval virtual functions of a given
			   class.  When is_virtual is FALSE or is_consteval is
			   TRUE, this field is undefined. */
    /* When is_constexpr_intrinsic is TRUE: */
    int32_t	constexpr_intrinsic;
			/* A small integer identifying an intrinsic known to
			   the constexpr interpreter. */
  } number;
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_il_entity_list_entry_ptr
		overridden_functions;
			/* For selectively overriding virtual functions (a
			   Microsoft extension), this points to the base
			   class member functions being overridden.
			   Otherwise, NULL.  In a non-managed class, this
			   reflects the single base class member function
			   named by the declarator of the derived class
			   member function.  In a managed class, this
			   reflects the list of base class member functions
			   following the declarator of the derived class
			   member function.  Usually, this list points to
			   a_routine entries, but, for prototype
			   instantiations, an overridden function may be
			   represented by a ck_template_param/tpck_member
			   constant. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  union {
    /* When is_inheriting_ctor == FALSE. */
    a_class_list_entry_ptr
                befriending_classes;
			/* A linked list of entries identifying classes that
			   have declared the current routine a friend (i.e.,
			   classes that have "befriended" the current routine).
			   Note that the representation is backwards compared
			   to the source language: in the source the
			   befriended routine is declared in the befriending
			   class; this list records the befriending class
			   in the befriended routine.  When
			   defined_in_friend_decl is TRUE, the first entry is
			   the class in which the friend definition appeared.*/
    /* When is_inheriting_ctor == TRUE. */
    a_routine_ptr
		inherited_routine;
			/* For an inheriting constructor, this points to the
			   routine that was inherited.  Otherwise, it is
			   NULL. */
  } friends_or_originator;
  a_template_arg_ptr
		template_arg_list;
			/* For routines that are instantiations of a function
			   template, a list of entries describing the actual
			   arguments on which the instantiation is based.
			   It is present only for instances of function
			   templates and member function templates (i.e.,
			   this pointer is NULL for member functions of
			   class templates and other nontemplate functions). */
  a_template_ptr
		assoc_template;
			/* For instantiated entities, this points to the
			   the template from which they were generated;
			   otherwise, this is NULL. */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
  a_gnu_routine_supplement_ptr
		gnu_extra_info;
			/* Supplementary GNU-specific information, in a
			   separate block to save space for non-GNU cases (as
			   well as GNU cases where all of the fields have their
			   default values).  Allocated only when needed. */
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_type_ptr	declared_type;
			/* The type as it actually appears in the declaration
			   of the routine at the point of its definition; NULL
			   if there is no defining declaration. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  a_routine_ptr	overriding_function_for_wrapper,
		overridden_function_for_wrapper;
			/* If non-NULL, this routine is a wrapper that
			   implements a version of the overriding virtual
			   function that works in place of the overridden
			   function.  Wrappers are used in two situations:
			   to implement IA-64 ABI thunks (where an adjustment
			   is needed to the "this" pointer), and when the
			   return types are covariant (in which case the
			   wrapper calls the overriding routine, then does a
			   derived-to-base adjustment on the returned pointer
			   value to get a result with the right type for the
			   overridden function). */
#if IA64_ABI
  a_targ_ptrdiff_t
		delta;	/* The offset that must be added to the "this" pointer
			   on entry to the function, before any virtual base
			   adjustments, or zero if none. */
  a_virtual_table_index
		vcall_index;
			/* The virtual table entry containing the offset that
			   should be added to the "this" pointer after delta
			   has been added, or zero if none. */
  a_targ_ptrdiff_t
		return_delta;
			/* The offset that should be added to the returned
			   pointer or reference, after converting via the
			   vbase index, or zero if no additional offset is
			   required. */
  a_virtual_table_index
		vbase_index;
			/* The virtual table entry containing the offset that
			   should be added to the returned value to reach the
			   virtual base, or zero if no virtual base conversion
			   is required. */
#endif /* IA64_ABI */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if DO_IL_LOWERING && IA64_ABI
  a_routine_ptr	primary_ctor_or_dtor;
			/* In an entry for a constructor or destructor
			   alternate entry point, this points to the
			   primary constructor or destructor routine.
			   NULL for a primary constructor or destructor
			   routine. */
#endif /* DO_IL_LOWERING && IA64_ABI */
#if ONE_INSTANTIATION_PER_OBJECT
  unsigned long	instantiation_needed_bit_number;
			/* When a separate "needed" flag is maintained for
			   each instantiation, this is the "needed" bit number
			   associated with this function.  0 if there is no
			   associated bit. */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  struct a_routine_fixup
		*routine_fixup;
			/* Used to process the bodies of friend
			   functions defined in class templates only
			   when they are referenced.  Points to the fixup
			   entry for the friend function definition.  This
			   field is for front-end use only. */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED && DO_IL_LOWERING
  a_gnu_init_priority
		init_priority;
			/* Used for initialization routines generated by IL
			   lowering for collections of variables with a
			   specific init_priority value.  This indicates
			   the priority.  Zero otherwise. */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED && DO_IL_LOWERING */
  a_using_decl_ptr
		generating_using_decl;
			/* If this is an inheriting constructor, the
			   using-declaration that generated it. */
} a_routine;

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED

#if USE_X86_FUNCTION_MULTIVERSIONING
/*
GNU multiversion target set; this is a bitset where the bit positions
correspond to a_multiversion_arch_kind enumeration values.
*/
typedef uint32_t a_mv_target_bitset;
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */

/*
A logical extension of a_routine for GNU-specific fields that are rarely used.
This extension is allocated only when necessary (i.e., if one or more of the
fields does not have its default value).  GNU-specific bitfields are left in
a_routine.
*/
typedef struct a_gnu_routine_supplement {
  a_const_char	*section;
			/* If non-NULL, the section in which this
			   routine should be placed. */
  a_routine_ptr	aliased_routine;
			/* If non-NULL, the routine for which this routine
			   is an alias.  (Used for attributes "alias" and
			   "weakref".  Also used for certain routines --
			   such as strlen -- that are implicitly aliased to
			   their __builtin_... counterpart; implicit_alias
			   is TRUE in such cases.)  Also used for the
			   ifunc attribute (in which case is_ifunc is TRUE).
			   In that case, the function signatures are different
			   (as the resolver routine returns a pointer to
			   the type returned by the ifunc routine). */
#if LOWER_IFUNC
  a_variable_ptr
		resolver_var;
			/* A variable that "caches" the result of calling the
			   ifunc resolver routine so that subsequent
			   calls don't need to invoke the resolver. */
#endif /* LOWER_IFUNC */
  a_routine_ptr	inline_partner;
			/* If a function has both a definition "for inlining
			   only" (flag definition_for_inlining_only) and a
			   definition of out-of-line calls, then the routine
			   entries corresponding to those definitions point to
			   each other via this pointer.  Otherwise, NULL. */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  a_gnu_init_priority
		ctor_priority;
			/* The priority (if any) specified by the GNU attribute
			   "constructor" (if any).  Valid only when
			   has_ctor_priority is TRUE. */
  a_gnu_init_priority
		dtor_priority;
			/* The priority (if any) specified by the GNU attribute
			   "destructor" (if any).  Valid only when
			   has_dtor_priority is TRUE. */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  a_const_char	*asm_name;
			/* If non-NULL, the name to be used as an assembly
			   language level symbol for this routine. */
#if GNU_FUNCTION_MULTIVERSIONING
  a_bit_field   is_representative:1;
                        /* TRUE if the routine is a multiversion function and
                           is the representative for all versions.  This
                           version of the routine is recorded in the symbol
                           table.  representative.targeted_versions
                           contains a list of routines (each with
                           is_target_specific_version set to TRUE) that are
                           target-specific. */
  a_bit_field   is_target_specific_version:1;
                        /* TRUE if the routine is a multiversion function
                           for a target-specific architecture (as specified by
                           the "target" attribute).  Not entered into the
                           symbol table. */
#if USE_X86_FUNCTION_MULTIVERSIONING
  a_bit_field   mv_resolver_required:1;
                        /* TRUE if it has been determined that a resolver
                           routine is necessary.  In some cases (e.g.,
                           only one target routine), a resolver routine isn't
                           needed.  Set only on the representative routine. */
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
  union {
    /* For GNU function multiversioning. */
    /* When is_representative is TRUE: */
    struct {
      a_routine_list_entry_ptr
                targeted_versions;
                        /* List of multiversion functions with a "target"
                           attribute.  These routines are not in the symbol
                           table; the is_representative routine acts as the
                           surrogate for the entire set of routines. */
#if USE_X86_FUNCTION_MULTIVERSIONING
                        /* The list is maintained in dispatch priority order
                           (with the exception that the default routine appears
                           first). */
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
    } representative;
    /* When is_target_specific_version is TRUE: */
    struct {
      a_routine_ptr
                representative;
                        /* A pointer to the representative routine. */
#if USE_X86_FUNCTION_MULTIVERSIONING
      a_mv_target_bitset
                target_bitset;
                        /* A bitmask of CPU and instruction set architectures
                           that have been applied (through the "target"
                           attribute) to this routine. */
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
    } targeted_version;
  } mv_info;
#endif /* GNU_FUNCTION_MULTIVERSIONING */
} a_gnu_routine_supplement;

/*
As the "gnu_extra_info" (aka a_gnu_routine_supplement) field of a_routine is
allocated on an as-needed basis, the following macros are intended to hide some
of the mechanics of allocating the supplement when using fields in
a_gnu_routine_supplement.  Each macro takes an a_routine_ptr argument.

ensure_gnu_routine_supp can be used when assigning a value to a field in the
"gnu_extra_info" supplement and it is not known if the supplement has been
previously allocated (the supplement is then allocated in this case).
For example:

  ensure_gnu_routine_supp(routine)->asm_name = "foo";

gnu_routine_supp can be used when accessing a field in the supplement and,
based on external information, it is assumed that the "gnu_extra_info" field
has previously been allocated.  For example, if routine->has_ctor_priority is
TRUE, then the following expression can be used to return the value of
ctor_priority (and an assertion will be generated -- when CHECKING is TRUE --
if the "gnu_extra_info" field has not been allocated):

  priority = gnu_routine_supp(routine)->ctor_priority;

has_gnu_routine_supp is used to test whether or not the field has been
allocated.

gnu_routine_supp_or_null behaves the same as gnu_routine_supp when CHECKING is
FALSE.
*/
#define ensure_gnu_routine_supp(rp) \
  (((rp)->gnu_extra_info) == NULL ? alloc_gnu_supplement_for_routine(rp) : \
                                    (rp)->gnu_extra_info)
#define has_gnu_routine_supp(rp) ((rp)->gnu_extra_info != NULL)
#if CHECKING
/*lint -emacro(664,gnu_routine_supp)*/
#define gnu_routine_supp(rp) \
  (check_assertion(has_gnu_routine_supp(rp)), (rp)->gnu_extra_info)
#else /* !CHECKING */
#define gnu_routine_supp(rp) ((rp)->gnu_extra_info)
#endif /* CHECKING */
#define gnu_routine_supp_or_null(rp) ((rp)->gnu_extra_info)

#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */

typedef struct an_asm_entry *an_asm_entry_ptr;
typedef struct an_asm_entry {
  /* Description of an asm declaration. */
  /* The source_corresp field must be first. */
  a_source_correspondence
		source_corresp;
			/* Information on any source entity that corresponds
			   to this entity. */
  an_asm_entry_ptr
		next;
			/* Pointer to the next asm entry declared in the same
			   scope, NULL if this asm entry is the last in the
			   scope. */
  a_constant_ptr
		asm_string;
			/* Constant containing a string representing an asm
			   definition argument (an uninterpreted line of
			   assembly language).  In Microsoft mode, this can
			   also contain a sequence of lines enclosed in
			   braces. */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	gnu_asm_form:1;
			/* The asm declaration used the extended GNU syntax
			   in which the asm string is followed by a colon.
			   This matters even when the operands and clobbers
			   lists are empty because the meaning of the asm
			   string may be subtly different if this flag is
			   TRUE. */
  a_bit_field	is_volatile:1;
			/* asm is volatile (not to be reordered) either
			   because it has the "volatile" keyword or because
			   it has no output operands. */
  a_bit_field	has_volatile_keyword:1;
			/* asm is marked volatile because the "volatile"
			   keyword appeared in the source. */
  a_bit_field	has_inline_keyword:1;
			/* The "inline" (or "__inline") keyword appeared in the
			   source. */
  a_bit_field	is_asm_goto:1;
			/* TRUE if this is an "asm goto". */
  an_asm_operand_ptr
		operands;
			/* List of asm operands.  Output operands
			   appear before input operands on this list. */
  a_named_register_list_ptr
                clobbers;
                        /* List of registers clobbered. */
  a_label_list_ptr
                labels;
                        /* List of labels (for "asm goto"). */
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  a_targ_size_t
		number_of_constraints;
			/* The number of constraints in each of the input
			   and output operand constraints.  Nominally one,
			   but may be more when multiple alternative
			   constraints (separated by commas) are used. */
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
#endif /* GNU_EXTENSIONS_ALLOWED */
} an_asm_entry;


/*
Data structures related to labels:
*/
typedef struct a_label {
  /* Definition of a label on an executable statement.  The stmk_label
     instruction points to here. */
  /* A compiler-generated label has a NULL name pointer in source_corresp. */
  /* The source_corresp field must be first. */
  a_source_correspondence
                source_corresp;
                        /* Information on any source entity that corresponds
                           to this entity. */
  a_label_ptr   next;
                        /* Pointer to the next label declared in the same
                           scope, NULL if this label is the last in the
                           scope. */
  a_bit_field	reachable_by_fall_through:1;
			/* TRUE if this label can be reached by falling
			   through to it from the code immediately
			   preceding. */
  a_bit_field	break_label:1;
			/* TRUE if this is a compiler-generated label that
			   is the target of a "break" statement. */
  a_bit_field   switch_break_label:1;
			/* TRUE if this is a compiler-generated label that
			   is the target of a switch "break" statement.  Set
			   in addition to break_label above. */
  a_bit_field	continue_label:1;
			/* TRUE if this is a compiler-generated label that
			   is the target of a "continue" statement. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	leave_label:1;
			/* TRUE if this is a compiler-generated label that
			   is the target of a "__leave" statement. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	address_taken:1;
			/* TRUE if this label had its address taken
			   (GNU-extended C). */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field	locally_declared:1;
			/* TRUE if this label was declared in a GNU C
			   __label__ declaration. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_bit_field	is_likely:1;
			/* TRUE if this label has the [[likely]] attribute
			   applied to it.  The front end does not take any
			   action based on this value. */
  a_bit_field	is_unlikely:1;
			/* TRUE if this label has the [[unlikely]] attribute
			   applied to it.  The front end does not take any
			   action based on this value. */
  a_statement_ptr
                exec_stmt;
                        /* Pointer to the stmk_label statement that defines
                           this statement.  During front end processing,
                           NULL until the definition is found. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  unsigned long	num_microsoft_trys_inside_of;
			/* Number of Microsoft try-finally or try-except
			   statements that the label is inside of. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} a_label;

#if DO_IL_LOWERING && GENERATE_EH_TABLES
typedef unsigned long a_cleanup_region_number;
			/* Number for a destructible region, used for
			   exception handling cleanup. */
#endif /* DO_IL_LOWERING && GENERATE_EH_TABLES */

/*
Data structures related to expressions:
*/
/*
Originally, this enum was declared with the underlying type of a_byte in order
to control storage size, however, declaring it as a bit field (and then
constraining the number of bits to 8) results in a better layout with some
compilers.

If you add new expression kinds, be sure to update expr_node_kind_names and
i_copy_expr_tree.
*/
#define NUM_BITS_FOR_EXPR_NODE_KIND 8
enum an_expr_node_kind : a_bit_field {
  enk_error,            /* Error. */
  enk_operation,        /* An operator and n operands; see
                           an_expr_operator_kind. */
  enk_constant,         /* A constant value. */
  enk_variable,         /* A variable. */
  enk_field,            /* Used in an eok_dot_field, eok_points_to_field, etc.
                           operation to indicate the field. */
  enk_temp_init,	/* Initialization of a temporary within an expression.
			   Mostly for C++ but used in C for C99 compound
			   literals. */
  enk_lambda,		/* C++ lambda expression (very similar to
			   enk_temp_init). */
  enk_new_delete,	/* C++ "new" or "delete". */
#if MICROSOFT_EXTENSIONS_ALLOWED
  enk_gcnew,		/* C++/CLI "gcnew". */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  enk_throw,		/* C++ throw expression. */
  enk_condition,	/* C++ condition -- a variable declaration with
			   initializer that appears as the condition of an
			   if-, switch-, while- or for-statement.  Appears
			   only at the top of an expression. */
  enk_object_lifetime,	/* Used to introduce an object lifetime that surrounds
			   a single expression, to restrict the lifetime
			   of temporaries created in the expression.  The
			   front end proper always puts these only at the top
			   of expressions, but IL lowering sometimes inserts
			   code above those nodes, so after IL lowering this
			   node is not necessarily the top node in the
			   expression tree.  C++ only. */
  enk_typeid,		/* C++ typeid expression.  When constexpr is enabled,
			   the non-polymorphic, non-dependent case is
			   represented by a ck_address/abk_typeid entry
			   instead. */
  enk_sizeof,
  enk_runtime_sizeof = enk_sizeof,  /* Old name. */
			/* A sizeof expression.  Usually those are folded to
			   constants at compile time, but this operator
			   still appears in backing expressions, for sizeofs
			   applied to dependent types or expressions in
			   prototype instantiations, and for sizeofs that
			   are actually variable (e.g., sizeof a
			   variable-length array). */
  enk_sizeof_pack,	/* sizeof...(T), the size of a variadic template
			   parameter pack. */
  enk_alignof,		/* An alignof expression.  Similar to sizeof, but
			   returns the alignment of a type or expression.
			   Like enk_sizeof, appears in backing expressions
			   and applied to dependent types or expressions in
			   prototype instantiations. */
  enk_datasizeof,	/* Same as sizeof, except the result doesn't include
			   tail padding.  This is a Clang extension. */
  enk_address_of_ellipsis,
			/* Used to represent nonstandard construct "&..."
			   (when ALLOW_ADDRESS_OF_ELLIPSIS is TRUE, to support
			   stdarg.h macro va_start). */
  enk_statement,	/* GNU statement expression, ({...}). */
  enk_reuse_value,	/* Reuse a value computed elsewhere in the current
			   expression tree.  Used for cases where a single
			   expression is used twice but evaluated only once.
			   In standard C++, this is only used for certain
			   calls to a std::initializer_list constructor.
			   All other uses are for extensions such as GCC's
			   two-operand "?:" operator.  Eliminated by IL
			   lowering. */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  enk_lowered_eh_construct,
			/* Used to represent a partially-lowered exception
			   handling construct. */
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  enk_result_of_overriding_function,
			/* Used in the body of a routine that is an entry
			   to be called in a covariant return type situation,
			   to represent the result of the call of the
			   overriding virtual function.  That result is cast
			   to the proper return type for the overridden
			   function, to give the value to be returned by
			   the entry. */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
  enk_routine,          /* A routine (function). */
#if VLA_DEALLOCATIONS_IN_IL
  enk_vla_dealloc,      /* Used to indicate when a variable-length array
                           should be deallocated (in C++, this may require
                           running destructors). */
#endif /* VLA_DEALLOCATIONS_IN_IL */
  enk_type_operand,	/* Used to represent types in certain expression
			   constructs.  Also used to represent the "default:"
			   case in a C11 _Generic construct. */
  enk_builtin_operation,
			/* Used to represent a variety of builtin
			   operations. */
  enk_param_ref,
			/* Used to represent a reference to a parameter in
			   an expression that participates in the signature
			   of a function type.  For example, in the function
			   declaration "auto f(X a)->decltype(*a)" the use
			   of "a" in the decltype construct is represented
			   with an enk_param_ref node.  Also used to represent
			   "this" in some contexts that don't have a "this"
			   variable. */
  enk_braced_init_list,	/* A C++11 brace-enclosed initializer list. */
  enk_c11_generic,	/* Used to represent a C11 _Generic expression
			   selection. */
#if BUILTIN_FUNCTIONS_ENABLED
  enk_builtin_choose_expr,
			/* Used to represent the GNU __builtin_choose_expr
			   construct. */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
  enk_yield,		/* A "co_yield" expression. */
  enk_await,		/* A "co_await" expression. */
  enk_fold,		/* The generic representation of a C++17 fold
			   expression.  (Concrete instantiations are
			   represented using a chain of enk_operation
			   nodes.) */
  enk_initializer,	/* When a dynamic_initializer is folded to a constant,
			   this represents the original initializer in the
			   backing expression. */
  enk_concept_id,	/* A concept-id expression. */
  enk_requires,		/* A requires-expression. */
  enk_compound_req,	/* A compound-requirement. */
  enk_nested_req,	/* A nested-requirement. */
  enk_const_eval_deferred,
			/* A deferred constant evaluation node. */
  enk_template_name,	/* Used to represent a template name in builtin
			   operation expressions. */
  enk_token_sequence,	/* A token sequence (a reflection feature). */
  enk_reclaimed,	/* Used to represent a node that's been reclaimed and
			   is part of the avail_fs_nodes list. */
  enk_pack_index,	/* A C++26 pack index expression. */
  enk_last
};

#if NEED_IL_DISPLAY || DEBUG
/*
Display names for expression node kinds.
*/
EXTERN_CONSTINIT_ARRAY(a_const_char*, expr_node_kind_names, enk_last + 1)
#if VAR_INITIALIZERS
= {
/* enk_error */				"error",
/* enk_operation */			"operation",
/* enk_constant	*/			"constant",
/* enk_variable	*/			"variable",
/* enk_field */				"field",
/* enk_temp_init */			"temp_init",
/* enk_lambda */			"lambda",
/* enk_new_delete */			"new_delete",
#if MICROSOFT_EXTENSIONS_ALLOWED
/* enk_gcnew */				"gcnew",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
/* enk_throw */				"throw",
/* enk_condition */			"condition",
/* enk_object_lifetime */		"object_lifetime",
/* enk_typeid */			"typeid",
/* enk_sizeof */			"sizeof",
/* enk_sizeof_pack */			"sizeof_pack",
/* enk_alignof */			"alignof",
/* enk_datasizeof */			"datasizeof",
/* enk_address_of_ellipsis */		"address_of_ellipsis",
/* enk_statement */			"statement",
/* enk_reuse_value */			"reuse_value",
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
/* enk_lowered_eh_construct */		"lowered_eh_construct",
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
/* enk_result_of_overriding_function */	"result_of_overriding_function",
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
/* enk_routine */			"routine",
#if VLA_DEALLOCATIONS_IN_IL
/* enk_vla_dealloc */			"vla_dealloc",
#endif  /* VLA_DEALLOCATIONS_IN_IL */
/* enk_type_operand */			"type_operand",
/* enk_builtin_operation */		"builtin_operation",
/* enk_param_ref */			"param_ref",
/* enk_braced_init_list	*/		"braced_inint_list",
/* enk_c11_generic */			"c11_generic",
#if BUILTIN_FUNCTIONS_ENABLED
/* enk_builtin_choose_expr */		"builtin_choose_expr",
#endif /* BUILTIN_FUNCTIONS_ENABLED */
/* enk_yield */				"yield",
/* enk_await */				"await",
/* enk_fold */				"fold",
/* enk_initializer */			"initializer",
/* enk_concept_id */			"concept_id",
/* enk_requires	*/			"requires",
/* enk_compound_req */			"compound_req",
/* enk_nested_req */			"nested_req",
/* enk_const_eval_deferred */		"const_eval_deferred",
/* enk_template_name */			"template_name",
/* enk_token_sequence */		"token_sequence",
/* enk_reclaimed */			"reclaimed",
/* enk_pack_index */			"pack_index",
/* enk_last */				"last"
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(expr_node_kind_names)
#endif /* NEED_IL_DISPLAY || DEBUG */

#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
/* Modifier for enk_lowered_eh_construct nodes, indicating the kind of
   node. */
enum a_lowered_eh_construct_kind : a_byte {
  leck_caught_object_address,
			/* Address of the object caught at the current active
			   catch clause. */
  leck_thrown_object_address,
			/* Address to which the thrown object should be
			   copied. */
  leck_cleanup_state,	/* Set the cleanup state. */
  leck_unreachable_cleanup_state,
			/* Set the cleanup state in unreachable code.  Used
			   when INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE
			   is TRUE. */
  leck_function_prologue,
			/* Prologue for function. */
  leck_function_epilogue,
			/* Epilogue for function. */
  leck_catch_epilogue,	/* Epilogue for catch clause. */
  leck_try_epilogue,	/* Epilogue for try block. */
  leck_exception_caught,
			/* Point after entry/copy of catch, where exception
			   has actually been caught. */
  leck_exception_started,
			/* Point before throw where the throw expression is
			   considered fully evaluated, but the copy constructor
			   to copy the object has not yet been called.  Marks
			   the point after which the exception is considered
			   started. */
#if !GENERATE_EH_TABLES
  leck_initialization_completed,
			/* Point at which an initialization that has
			   overlaps_temps_in_inner_lifetime set to TRUE has
			   been completed. */
#endif /* !GENERATE_EH_TABLES */
  leck_internal_try	/* Internal "try" block, used to get cleanup code
			   executed if an exception is thrown while executing
			   an expression. */
};

#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */

/*
When the expression node kind is "enk_operation", these are the possible
operators.

If you add operators to this list, be sure to update:
     il_def.h (this file):
         db_operator_names
     il_display.c:
         disp_expr_operator_name
     cp_gen_be.c:
         generated_precedence
         gen_expr
     il.c:
         lvalue_rvalue_test
         operation_type_kind
         operation_has_side_effects (if the operator has side effects)
         operator_is_foldable
     expr.c:
         operator_token_for_expr_rescan
     exprutil.c:
         operator_for_opname_kind
     folding.c:
         fold_expr
     interpret.c:
         do_constexpr_expression
     lower_name.c:
         mangled_expr_operator_name
   If the operator returns an lvalue (is_lvalue is TRUE), see also
     il.c:
         is_rvalueable_node
         node_does_fetch
         lvalue_rvalue_test
     exprutil.c:
         conv_prvalue_expr_to_lvalue
         conv_glvalue_expr_to_prvalue (if the operator is not "rvalueable"
           according to is_rvalueable_node)
   If the operator is an addressing operator, see also
     folding.c:
         constant_glvalue_address_full
         constant_prvalue_pointer_full
     il_walk.c:
         traverse_addressing_subtree
*/
enum an_expr_operator_kind : a_byte {
  /* The following have 1 operand: */
  eok_address_of,	/* Address-of operator ("&"). */
  eok_reference_to,	/* Turns a glvalue into a reference, i.e., the
			   reference equivalent of eok_address_of.  Can also be
			   applied to a class prvalue, where it produces a
			   reference to the class object in memory. */
  eok_handle_to,	/* C++/CLI unary "%" operator, which returns a handle
			   to its operand.  The operand must have a ref class
			   or interface class type, and can be an lvalue or
			   an rvalue.  See also eok_handle_to_box. */
  eok_indirect,		/* Pointer de-reference operator ("*"). */
  eok_ref_indirect,	/* Implicit indirection through a reference to get an
			   lvalue, i.e., the reference equivalent of
			   eok_indirect. */
  eok_cast,		/* Type cast.  The type of the expression indicates
			   the type to cast to.	 The type can be void.
			   Also note that C++ reinterpret_casts to pointer-to-
			   class types and pointer-to-member types are
			   represented as eok_cast operations, with the
			   is_reinterpret_cast flag set.  In C++, this
			   is also the operator used for casts involving
			   template-dependent types (in prototype
			   instantiations). */
  eok_lvalue_cast,	/* Used to represent a nonstandard feature present
			   in some older C modes and in Microsoft and GNU C++
			   modes, in which an lvalue operand can be cast to a
			   new type and the result is still an lvalue.
			   The new type (indicated by the type of the
			   expression) is always similar to the operand type
			   (e.g., they could be integral types with the same
			   size but different signedness).  The cast creates
			   an lvalue that refers to the same underlying object
			   but with a slightly different type. */
  eok_ref_cast,		/* Similar to eok_lvalue_cast, but used to represent
			   some explicit casts to reference types.  The operand
			   is an lvalue, and the result is an lvalue for the
			   same object but with a different type (indicated
			   by the type of the expression).  For a cast (T &)x
			   the node type is T, not T&.  Cannot handle base or
			   derived class adjustments.  Unlike eok_lvalue_cast,
			   this operation is rvalueable (it can include an
			   implicit lvalue-to-rvalue conversion).  Also used
			   for casts to reference types involving template-
			   dependent types (in prototype instantiations).
			   eok_ref_cast is rewritten as eok_lvalue_adjust
			   during lowering. */
  eok_lvalue_adjust,	/* Similar to eok_lvalue_cast, but used for implicit
			   lvalue type adjustments related to standard language
			   features, for example when adjusting cv-qualifiers
			   to bind a reference.  The operand is an lvalue,
			   and the result is an lvalue for the same object but
			   with a different type (indicated by the type of the
			   expression).  Typically used to adjust
			   cv-qualifiers, but can also change the underlying
			   object type in significant ways (e.g., from one
			   class type to another).  Cannot handle base or
			   derived class adjustments.  Unlike eok_lvalue_cast,
			   this operation is rvalueable (it can include an
			   implicit lvalue-to-rvalue conversion).  Can also be
			   used on xvalues; the result is an xvalue.  Used in
			   lowering to rewrite eok_ref_cast (so an explicit
			   reference cast will become an eok_lvalue_adjust
			   in configurations that perform lowering). */
  eok_class_rvalue_adjust,
			/* Used to adjust the cv-qualifiers on a class rvalue.
			   The operand is a prvalue with class type.  The
			   result is the same class prvalue with its type
			   changed to the type of the rvalue-adjust expression.
			   Used only in C++.  Usually compiler-generated, but
			   can be the result of an actual cast in C++17 when
			   the temporary materialization conversion is not
			   applied.  Eliminated by IL lowering if
			   LOWER_CLASS_RVALUE_ADJUST is TRUE. */
  eok_box,		/* C++/CLI boxing operation.  The operand is an rvalue
			   of a value type (other than a pointer) and the
			   result is a handle to the box allocated to contain
			   that value. */
  eok_handle_to_box,	/* Exactly the same semantics as eok_box, but used to
			   indicate the case where the source form uses the
			   unary "%" operator.  Never compiler-generated,
			   and unlike eok_box never considered a cast. */
  eok_unbox,		/* C++/CLI unboxing operation.  The operand is a
			   handle to a value class or boxed enum and the
			   result is an lvalue for the unboxed value.  This
			   does not copy the value; it returns a gc-lvalue
			   for the value within the boxed object allocated
			   on the managed heap.  The result type may be
			   a derived class of the type underlying the
			   handle.  If is_lvalue is FALSE, the operation
			   fetches an rvalue from the box (i.e., there's a
			   built-in bitwise copy), but the operation is
			   not rvalueable; the fetch/copy is an inherent
			   part of the operation.  Does a runtime check
			   in some cases. */
  eok_unbox_lvalue,	/* C++/CLI unboxing operation.  The operand is
			   a gc-lvalue for a value class or boxed enum and
			   the result is an lvalue for the unboxed value.
			   Always compiler-generated, e.g., on top of an
			   eok_indirect applied to a handle to a value class.
			   Like eok_unbox, is_lvalue FALSE indicates a
			   fetch/copy, but the operation is not rvalueable.
			   The result type is always the same as the operand
			   type, but a runtime check is still done. */
  eok_base_class_cast,	/* C++ cast of a class to a direct base class.  The
			   type of the expression indicates the type to cast
			   to.  The operand can be a class lvalue, a class
			   rvalue, an rvalue pointer to class, or an rvalue
			   C++/CLI handle.  The result is of the same kind
			   (lvalue, rvalue, pointer, or handle).  Also used for
			   reference casts that convert to a base class. */
  eok_derived_class_cast,
			/* C++ cast of a class to a direct derived class.  The
			   type of the expression indicates the type to cast
			   to.  The operand can be a class lvalue, a class
			   rvalue, an rvalue pointer to class, or an rvalue
			   C++/CLI handle.  The result is of the same kind
			   (lvalue, rvalue, pointer, or handle).  Also used
			   for reference casts that convert to a derived
			   class. */
  eok_pm_base_class_cast,
			/* C++ cast of a pointer to a member of a class to
			   a pointer to a member of a direct base class.
			   The type of the expression indicates the type to
			   cast to. */
  eok_pm_derived_class_cast,
			/* C++ cast of a pointer to a member of a class to
			   a pointer to a member of a direct derived class.
			   The type of the expression indicates the type to
			   cast to. */
  eok_dynamic_cast,	/* C++ dynamic_cast operation on pointers or C++/CLI
			   handles. */
  eok_ref_dynamic_cast,	/* C++ dynamic_cast operation on references.  The
			   operand and the result are glvalues.  For a
			   dynamic_cast<T &>(x), the node type is T, not
			   T&. */
  eok_bool_cast,	/* C++ and C99 cast to bool.  Operand can be
			   arithmetic, enum, pointer, pointer-to-member, or
			   C++/CLI handle, and result is the equivalent of
			   "operand != 0". */
  eok_array_to_pointer,
			/* Array to pointer decay: converts an array lvalue or
			   rvalue to a pointer to its first element. */
  eok_dot_vacuous_destructor_call,
			/* Call of a "destructor" for a class or simple type
			   that does not have one, e.g., x.int::~int().
			   The result is void.  The first operand is the
			   expression designating the object to "destroy".
			   If a user-defined type name was used, an additional
			   enk_routine operand may follow: It will have a NULL
			   routine pointer, but points to an associated name
			   reference entry. */
  eok_points_to_vacuous_destructor_call,
			/* Similar to eok_dot_vacuous_destructor_call, but
			   for the "->" case, e.g., p->int::~int(). */
#if MICROSOFT_EXTENSIONS_ALLOWED
  eok_assume,		/* Microsoft __assume(expr).  Note that the
			   operand is not evaluated in the traditional
			   sense of the word. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  eok_noexcept,		/* C++11 noexcept operator.  The operand is not
			   evaluated; the result is a constant that indicates
			   whether the expression can throw an exception
			   (false) or not (true).  Only used as a backing
			   expression or in a template-dependent constant.
			   The operand can be an lvalue or an rvalue. */
  eok_parens,		/* Parentheses.  See PARENS_IN_IL. */
  eok_negate,           /* Arithmetic negation. */
  eok_unary_plus,	/* Unary "+" (arithmetic or pointer). */
  eok_complement,       /* Integer bitwise complement ("~" operator). */
  eok_not,              /* Logical complement ("!" operator).  Operand is
                           standardized to integer/boolean in some
                           configurations. */
  eok_vector_not,       /* GNU vector logical complement ("!" operator).
                           The operand and result are GNU vectors.  Used in
                           C++ mode only.  Result is a vector of signed
                           integral type. */
  eok_vector_fill,      /* GNU vector "fill" operation.  Takes one scalar
                           operand and returns a GNU vector whose value has the
                           scalar's value in every element of the vector. */
#if C99_IL_EXTENSIONS_SUPPORTED
  eok_xconj,            /* Complex conjugation ("~") operator. */
  eok_real_part,        /* Produce the real part of a complex number.  The
			   operand is an lvalue or rvalue of complex type.
			   (This is the GNU "__real" operator.) */
  eok_imag_part,        /* Produce the imaginary part of a complex number.  The
			   operand is an lvalue or rvalue of complex type.
			   (This is the GNU "__imag" operator.) */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  eok_post_incr,        /* Post increment. */
  eok_post_decr,        /* Post decrement. */
  eok_pre_incr,         /* Pre increment. */
  eok_pre_decr,         /* Pre decrement. */
  /* The following have 2 operands: */
  eok_add,		/* Addition.  Not used for pointer arithmetic (see
			   eok_padd), nor for mixed real/imaginary addition
			   (see eok_fjadd and eok_jfadd). */
  eok_subtract,		/* Subtraction.  Not used for pointer arithmetic (see
			   eok_psubtract and eok_pdiff), nor for mixed real/
			   imaginary subtraction (see eok_fjsubtract and
			   eok_jfsubtract). */
  eok_multiply,		/* Multiplication.  Not used to represent the
			   multiplication of two _Imaginary values in C99
			   mode (see eok_jmultiply). */
  eok_divide,		/* Division.  Not used to represent the division of a
			   real value by an _Imaginary value in C99 mode (see
			   eok_jdivide). */
  eok_remainder,        /* "%" operator. */
#if C99_IL_EXTENSIONS_SUPPORTED
  eok_jmultiply,        /* Multiplication of two imaginary values.  The result
			   is real (i.e., non-imaginary). */
  eok_jdivide,          /* Division of real by imaginary gives an
                           imaginary result with a sign change. */
  eok_fjadd,            /* Real + imaginary, produces complex. */
  eok_jfadd,            /* Imaginary + real, produces complex. */
  eok_fjsubtract,       /* Real - imaginary, produces complex. */
  eok_jfsubtract,       /* Imaginary - real, produces complex. */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  eok_padd,		/* Pointer addition.  One operand is a pointer, the
			   other an integer, in either order.  Note that the
			   integer can be of any integral type; the integral
			   promotions are not done. */
  eok_psubtract,        /* Pointer subtraction.  First operand is always the
                           pointer, second always the integer.  Note that the
                           integer can be of any integral type; the integral
                           promotions are not done. */
  eok_pdiff,            /* Pointer difference.  Difference between two
                           pointers, returns an integer (ptrdiff_t). */
  eok_shiftl,           /* Left shift ("<<" operator).  Also used for
			   vectors. */
  eok_shiftr,           /* Right shift (">>" operator).  Also used for
			   vectors. */
  eok_and,              /* Bitwise and ("&" operator). */
  eok_or,               /* Bitwise or ("|" operator). */
  eok_xor,              /* Exclusive or ("^" operator). */
  eok_eq,               /* Equality ("=="). */
  eok_ne,               /* Inequality ("!="). */
  eok_gt,               /* Greater than (">"). */
  eok_lt,               /* Less than ("<"). */
  eok_ge,               /* Greater than or equal (">="). */
  eok_le,               /* Less than or equal ("<="). */
  eok_spaceship,        /* Three-way comparison ("<=>"). */
  eok_vector_eq,        /* GNU vector equality ("==").  Result is a vector of
                           signed integral element type. */
  eok_vector_ne,        /* GNU vector inequality ("!=").  Result is a vector of
                           signed integral element type. */
  eok_vector_gt,        /* GNU vector greater than (">").  Result is a vector
                           of signed integral element type. */
  eok_vector_lt,        /* GNU vector less than ("<").  Result is a vector of
                           signed integral element type. */
  eok_vector_ge,        /* GNU vector greater than or equal (">=").  Result is
                           a vector of signed integral element type. */
  eok_vector_le,        /* GNU vector less than or equal ("<=").  Result is a
                           vector of signed integral element type. */
  eok_gnu_min,          /* Minimum operator ("<?", a GNU C++ extension).
			   Operands and result may be lvalues or rvalues. */
  eok_gnu_max,          /* Maximum operator (">?", a GNU C++ extension).
			   Operands and result may be lvalues or rvalues. */
  eok_assign,           /* Assignment.  For struct assignments (i.e., when the
			   associated operation type kind is tk_struct), the
			   meaning wrt. tail padding depends on the IL kind.
			   In unlowered C++ IL, it is the operation performed
			   by the generated bitwise operator=, which is defined
			   to copy only the data of the class, and not any tail
			   padding (in other words, it has to be usable to copy
			   a subobject base class; for an empty base class, it
			   should copy nothing).  In lowered C++ IL or in C,
			   this means just a normal C struct copy, which copies
			   sizeof(struct) bytes. */
  eok_add_assign,       /* Add assign operator ("+="). */
  eok_subtract_assign,  /* Subtract assign operator ("-="). */
  eok_multiply_assign,  /* Multiply assign operator ("*="). */
  eok_divide_assign,    /* Divide assign operator ("/="). */
  eok_remainder_assign, /* Remainder assign operator ("%="). */
  eok_shiftl_assign,    /* Left shift assign operator ("<<=").  The first
			   operand may have integral or fixed-point type; the
			   second operand always has integral type. */
  eok_shiftr_assign,    /* Right shift assign operator (">>=").  The first
			   operand may have integral or fixed-point type; the
			   second operand always has integral type. */
  eok_and_assign,       /* Bitwise and assign operator ("&="). */
  eok_or_assign,        /* Bitwise or assign operator ("|="). */
  eok_xor_assign,       /* Exclusive or assign operator ("^="). */
  eok_padd_assign,      /* Pointer add assign operator ("+=").  In unlowered
			   IL, one strange case is bool += pointer. */
  eok_psubtract_assign, /* Pointer subtract assign operator ("-="). */
  eok_bassign,		/* Block assignment.  Only used in C++ after IL
			   lowering, for copy constructors etc.  The
			   destination (first operand) is an lvalue; the
			   source (second operand) is either an lvalue or
			   an array rvalue. The operation is equivalent to
			   a memcpy.  The result is void.  The size of the
			   source operand should be used as the size of the
			   block copy; the source and destination operand
			   types typically have the same size, but may not
			   (e.g., in the case of a variably-sized array
			   destination where the size is unknown or when
			   partially initializing an array). */
  eok_land,		/* Logical intersection ("&&" operator).  Operands are
			   standardized to integer/boolean in some
			   configurations. */
  eok_lor,		/* Logical union ("||" operator).  Operands are
			   standardized to integer/boolean in some
			   configurations. */
  eok_vector_land,	/* GNU vector logical "and" ("&&" operator).
			   At least one operand is a GNU vector; the other may
			   be a scalar or a vector.  C++ only.  Result is a
			   vector of signed integral type. */
  eok_vector_lor,	/* GNU vector logical "or" ("||" operator).
			   At least one operand is a GNU vector; the other may
			   be a scalar or a vector.  C++ only.  Result is a
			   vector of signed integral type. */
  eok_comma,            /* The comma operator (","). */
  eok_subscript,	/* Subscripting operation.  The operands are the
			   pointer to the first element of the array and the
			   integral subscript value, in either order. */
  eok_vector_subscript,	/* GNU vector subscripting operation.  The first
			   operand is the GNU vector, and the second is the
			   integral subscript value. */
  eok_dot_field,	/* Selection of a nonstatic data member of a class,
			   source form x.y.  The first operand is an lvalue
			   or rvalue of class type.  The second operand is
			   an enk_field. */
  eok_points_to_field,	/* Selection of a nonstatic data member of a class,
			   source form p->y.  The first operand is an rvalue
			   pointer to class or C++/CLI handle to class.
			   The second operand is an enk_field. */
  eok_pm_field,		/* Selection of a nonstatic data member of a class
			   using a pointer to member, source form x.*pm.
			   The first operand is an lvalue or rvalue of
			   class type.  The second operand is an rvalue of
			   pointer-to-data-member type. */
  eok_pm_points_to_field,
			/* Selection of a nonstatic data member of a class
			   using a pointer to member, source form x->*pm.
			   The first operand is an rvalue pointer to class.
			   The second operand is an rvalue of pointer-to-data-
			   member type. */
  eok_dot_pm_func_ptr,	/* For the pointer-to-member-function selection x.*pm,
			   returns the address of the function.  The first
			   operand is an lvalue or rvalue of class type.  The
			   second operand is an rvalue of pointer-to-member-
			   function type.  Used for a nonstandard g++
			   feature. */
  eok_points_to_pm_func_ptr,
			/* For the pointer-to-member-function selection x->*y,
			   returns the address of the function.  The first
			   operand is an rvalue pointer to class.  The
			   second operand is an rvalue of pointer-to-member-
			   function type.  Used for a nonstandard g++
			   feature. */
  eok_dot_static,	/* Selection of a static member of a class, source
			   form x.y.  The first operand is an lvalue
			   or rvalue of class type, which is evaluated
			   and then discarded.  The second operand is an
			   enk_variable identifying a static data member,
			   an enk_routine identifying a static member
			   function, or an enk_constant identifying a member
			   constant (e.g., an enumerator).  In Microsoft mode,
			   the second operand may also be a nonstatic member
			   function (to represent, e.g., "sizeof(&x.f)", which
			   is accepted in Microsoft mode). */
  eok_points_to_static,	/* Selection of a static member of a class, source
			   form p->y.  The first operand is an rvalue
			   pointer to class or C++/CLI handle to class, which
			   is evaluated and then discarded.  The second operand
			   is an enk_variable identifying a static data member,
			   an enk_routine identifying a static member function,
			   or an enk_constant identifying a member constant
			   (e.g., an enumerator).  In Microsoft mode, the
			   second operand may also be a nonstatic member
			   function (to represent, e.g., "sizeof(&p->f)", which
			   is accepted in Microsoft mode).  */
  eok_virtual_function_ptr,
			/* Produce a normal function pointer for a C++ virtual
			   member function.  This is (only) used to implement
			   a C++ anachronism.  The first operand is the address
			   of a virtual function (NOT a pointer-to-member);
			   the second is the selector object (standardized,
			   if necessary, to a pointer to class).  The result
			   is a pointer to the selected function. */
  /* The following have 3 operands: */
  eok_question,		/* Conditional expression ("?" operator).  The first
			   operand is standardized to integer/boolean in some
			   configurations.  Also used for the GNU two-operand
			   form, when is_gnu_two_operand_question_mark is TRUE
			   (but three operands are still provided in that
			   case). */
  eok_vector_question,	/* GNU vector conditional expression ("?" operator).
			   All operands are vectors and have the same number
			   of vector elements.  The first operand has
			   vector elements of integer type.  C++ only. */
  /* The following have n operands: */
  eok_call,             /* A call of a non-member function or static member
			   function.  Also any call in C.  The first operand
			   identifies the routine (as a routine address) and
			   the rest are its arguments.  Note that the operand
			   specifying the routine can be an expression
			   (e.g., for a call through a pointer), or
			   eok_dot_static/eok_points_to_static for a static
			   member function call. */
  eok_dot_member_call,	/* A call of a non-static member function with the
			   source form x.f(args).  The first operand
			   identifies the member function (as a routine
			   address, not a pointer-to-member); the second
			   operand is the selector object (class lvalue
			   or class rvalue); the remaining operands are the
			   arguments.  The is_virtual_call flag indicates
			   whether the call is virtual. */
  eok_points_to_member_call,
			/* A call of a non-static member function with the
			   source form p->f(args).  The first operand
			   identifies the member function (as a routine
			   address, not a pointer-to-member); the second
			   operand is an rvalue pointer to class or C++/CLI
			   handle to class that identifies the selector object;
			   the remaining operands are the arguments.  The
			   is_virtual_call flag indicates whether the call
			   is virtual. */
  eok_dot_pm_call,	/* A call of a function identified by a pointer
			   to member, with the source form (x.*pmf)(args).
			   The first operand is the pointer to member
			   function; the second is the selector object
			   (class lvalue or class rvalue); the remaining
			   operands are the arguments.  Note that the selector
			   object is always evaluated first (even though
			   eval_left_to_right is set).  Also, arguments are
			   always evaluated after the first two operands. */
  eok_points_to_pm_call,
			/* A call of a function identified by a pointer
			   to member, with the source form (p->*pmf)(args).
			   The first operand is the pointer to member
			   function; the second is an rvalue pointer to
			   class that identifies the selector object;
			   the remaining operands are the arguments.  Note that
			   the selector object is always evaluated first (even
			   though eval_left_to_right is set).  Also, arguments
			   are always evaluated after the first two operands.*/
  eok_cli_subscript,	/* C++/CLI array subscripting operation.  The first
			   operand is a handle to a CLI array object (a ref
			   class type), and following arguments are the
			   subscripts.  The result is an lvalue for the
			   array element. */
  /* Operators used when the <stdarg.h> macros are treated as builtins: */
  eok_va_start,		/* va_start macro reference.  First operand is an
			   lvalue variable of type va_list, second is
			   (usually) an lvalue for the last parameter
			   before the "..."  of the function.  The second
			   operand will be an rvalue if va_list is an array
			   type (Or, in g++ mode, if the parameter has a
			   reference type). */
  eok_va_arg,		/* va_arg macro reference.  First operand is an lvalue
			   variable of type va_list (or an rvalue as described
			   under eok_va_start).  Second argument of macro
			   is represented by the result type of the expression
			   node.  The result can be an lvalue or an rvalue. */
  eok_va_end,		/* va_end macro reference.  First operand is an lvalue
			   variable of type va_list (or an rvalue as described
			   under eok_va_start). */
  eok_va_copy,		/* va_copy macro reference.  Both operands are
			   lvalue variables of type va_list (or rvalues as
			   described under eok_va_start). */
  eok_va_start_single_operand,
			/* Same as eok_va_start, but without the second
			   operand.  This is typically used to implement the
			   <varargs.h> variant of va_start (as opposed to the
			   variant from <stdarg.h>). */
  /* Operators appearing only in prototype instantiations: */
  eok_lvalue,		/* Indicates that the operand (marked as an rvalue,
			   but really something with unknown lvalueness) is
			   to be used as if it were an lvalue.  The eok_lvalue
			   node itself is marked as an lvalue. */
  eok_await,		/* The coroutine "co_await" operator applied to a
			   dependent operand.  (In non-dependent contexts,
			   an "enk_await" node is created instead.) */
  eok_yield,		/* The coroutine "co_yield" operator applied to a
			   dependent operand.  (In non-dependent contexts,
			   an "enk_yield" node is created instead.) */
  eok_splice,		/* An expression splice from a reflection value.
			   This can appear in prototype instantiations but
			   also in backing expressions for constants.  It
			   takes one expression/constant operand. */
  /* Special operators: */
  eok_error,            /* This is a special operator used in the cases when
                           the operator cannot be determined.  This operator
                           will never be found in an expression node, as it is
                           used only to determine when an expression tree
                           cannot be built because of errors in the
                           operands. */
  eok_last              /* Marks the end of the list. */
};


/*
When the expression node kind is "enk_builtin_operation", these are the
possible operations.

Note that the value of enumerators in this list is used as part of the mangled
name encoding for builtin operations, so the items on this list should not be
reordered; if they are, it will cause an ABI incompatibility (in the names of
templates).  For this same reason, we avoid making any of these enumerators
configuration-dependent (e.g., the GNU-specific bok_types_compatible is part of
the list even when GNU_EXTENSIONS_ALLOWED is FALSE).

Additionally note that in some cases the difference between "builtin
operations" and "builtin functions" can be hazy.  If a builtin takes a type
argument, it must be a builtin operation, otherwise a builtin function is often
a better fit.

If you add an operation to this list, be sure to update
builtin_operation_names.
*/
enum a_builtin_operation_kind : a_byte {
  bok_offsetof,		/* Builtin offsetof (currently only available in some
			   GNU modes).  Two operands: A type and a field. */
  bok_has_assign,	/* __has_assign.  One operand: A type. */
  bok_has_copy,		/* __has_copy.  One operand: A type. */
  bok_has_nothrow_assign,
			/* __has_nothrow_assign.  One operand: A type. */
  bok_has_nothrow_constructor,
			/* __has_nothrow_constructor.  One operand: A type. */
  bok_has_nothrow_copy,	/* __has_nothrow_copy.  One operand: A type. */
  bok_has_trivial_assign,
			/* __has_trivial_assign.  One operand: A type. */
  bok_has_trivial_constructor,
			/* __has_trivial_constructor.  One operand: A type. */
  bok_has_trivial_copy,	/* __has_trivial_copy.  One operand: A type. */
  bok_has_trivial_destructor,
			/* __has_trivial_destructor.  One operand: A type. */
  bok_has_user_destructor,
			/* __has_user_destructor.  One operand: A type. */
  bok_has_virtual_destructor,
			/* __has_virtual_destructor.  One operand: A type. */
  bok_is_abstract,	/* __is_abstract.  One operand: A type. */
  bok_is_base_of,	/* __is_base_of.  Two operands, both types. */
  bok_is_class,		/* __is_class.  One operand: A type. */
  bok_is_convertible_to,
			/* __is_convertible_to.  Two operands, both types
                           (clang). */
  bok_is_empty,		/* __is_empty.  One operand: A type. */
  bok_is_enum,		/* __is_enum.  One operand: A type. */
  bok_is_pod,		/* __is_pod.  One operand: A type. */
  bok_is_polymorphic,	/* __is_polymorphic.  One operand: A type. */
  bok_is_union,		/* __is_union.  One operand: A type. */
  bok_types_compatible,	/* GNU __builtin_types_compatible.  Two operands, both
			   types. */
  bok_intaddr,		/* EDG extension __INTADDR__ (used for offsetof).
			   Effectively, a cast from address constant to
			   size_t. */
  bok_is_trivial,	/* __is_trivial. One operand: A type. */
  bok_is_standard_layout,
			/* __is_standard_layout. One operand: A type. */
  bok_is_trivially_copyable,
			/* __is_trivially_copyable.  One operand: A type. */
  bok_is_literal_type,	/* __is_literal_type.  One operand: A type. */
  bok_has_trivial_move_constructor,
			/* __has_trivial_move_constructor.  One operand:
			   A type. */
  bok_has_trivial_move_assign,
			/* __has_trivial_move_assign.  One operand: A type. */
  bok_has_nothrow_move_assign,
			/* __has_nothrow_move_assign.  One operand: A type. */
  bok_is_constructible,
			/* __is_constructible.  One or more operands,
			   all types. */
  bok_is_nothrow_constructible,
			/* __is_nothrow_constructible.  One or more operands,
			   all types. */
  bok_has_finalizer,	/* __has_finalizer.  One operand: A type. */
  bok_is_delegate,	/* __is_delegate.  One operand: A type. */
  bok_is_interface_class,
			/* __is_interface_class.  One operand: A type. */
  bok_is_ref_array,	/* __is_ref_array.  One operand: A type. */
  bok_is_ref_class,	/* __is_ref_class.  One operand: A type. */
  bok_is_sealed,	/* __is_sealed.  One operand: A type. */
  bok_is_simple_value_class,
			/* __is_simple_value_class.  One operand: A type. */
  bok_is_value_class,	/* __is_value_class.  One operand: A type. */
  bok_is_final,		/* __is_final.  One operand: A type. */
  bok_is_trivially_constructible,
			/* __is_trivially_constructible.  One or more operands,
			   all types. */
  bok_is_destructible,	/* __is_destructible.  One type operand. */
  bok_is_nothrow_destructible,
			/* __is_nothrow_destructible.  One type operand. */
  bok_is_trivially_destructible,
			/* __is_trivially_destructible.  One type operand. */
  bok_is_nothrow_assignable,
			/* __is_nothrow_assignable.  Two type operands. */
  bok_is_trivially_assignable,
			/* __is_trivially_assignable.  Two type operands. */
  bok_builtin_shuffle,
			/* GNU's __builtin_shuffle operator.  Two or three GNU
			   vector operands. */
  bok_builtin_complex,	/* __builtin_complex.  Two real floating-point
			   operands of identical type. */
  bok_is_valid_winrt_type,
			/* __is_valid_winrt_type.  One type operand. */
  bok_is_win_class,	/* __is_win_class.  One type operand. */
  bok_is_win_interface,	/* __is_win_interface.  One type operand. */
  bok_builtin_shufflevector,
			/* Clang's __builtin_shufflevector operator.  Two
			   vector operands followed by a list of integers. */
  bok_builtin_convertvector,
			/* Clang's __builtin_convertvector operator.  A vector
			   operand followed by a type operand. */
  bok_is_assignable,    /* Microsoft's __is_assignable.  Two type operands. */
  bok_is_trivially_copy_assignable,
			/* Microsoft's __is_trivially_copy_assignable.  One
			   type operand. */
  bok_is_assignable_no_precondition_check,
			/* Microsoft's __is_assignable_no_precondition_check.
			   Two type operands (treated the same as
			   __is_assignable). */
  bok_builtin_addressof,/* __builtin_addressof.  One lvalue operand. */
  bok_has_unique_object_representations,
			/* __has_unique_object_representations.  One type
			   operand. */
  bok_is_aggregate,	/* __is_aggregate.  One type operand. */
  bok_reference_binds_to_temporary,
			/* Clang's __reference_binds_to_temporary.  Two type
			   operands. */
  bok_is_same,          /* __is_same (Clang).  Two type operands. */
  bok_is_same_as,       /* __is_same_as (GCC).  Two type operands. */
  bok_is_function,      /* __is_function.  One type operand. */
  bok_builtin_has_attribute,
                        /* __builtin_has_attribute.  The first operand is a
                           type-or-expression (e.g., similar to sizeof).  The
                           second operand is an attribute (with optional
                           arguments). */
  bok_builtin_bit_cast, /* __builtin_bit_cast.  First operand is a type and the
                           second is an object. */
  bok_is_layout_compatible,
			/* Two type operands. */
  bok_is_pointer_interconvertible_base_of,
			/* Two type operands. */
  bok_is_pointer_interconvertible_with_class,
                        /* A class type operand and a pointer-to-member
                           argument. (Visual Studio) */
  bok_builtin_is_pointer_interconvertible_with_class,
                        /* One pointer-to-member argument. (GCC) */
  bok_is_corresponding_member,
                        /* Two class type operands and two pointer-to-member
                           arguments. (Visual Studio) */
  bok_builtin_is_corresponding_member,
                        /* Two pointer-to-member arguments. (GCC) */
  bok_edg_is_deducible, /* A class template operand and a type argument. */
  bok_is_array,		/* __is_array. One type operand. */
  bok_array_rank,       /* __array_rank (GNU, Clang).  One type operand
                           (returns size_t). */
  bok_array_extent,     /* __array_extent (Clang).  One type operand and one
                           int (returns size_t). */
  bok_is_arithmetic,    /* __is_arithmetic (Clang).  One type operand. */
  bok_is_complete_type, /* __is_complete_type (Clang).  One type operand. */
  bok_is_compound,      /* __is_compound (Clang).  One type operand. */
  bok_is_const,         /* __is_const (Clang).  One type operand. */
  bok_is_floating_point,/* __is_floating_point (Clang).  One type operand. */
  bok_is_fundamental,   /* __is_fundamental (Clang).  One type operand. */
  bok_is_integral,      /* __is_integral (Clang).  One type operand. */
  bok_is_lvalue_reference,
                        /* __is_lvalue_reference (Clang).  One type operand. */
  bok_is_member_function_pointer,
                        /* __is_member_function_pointer (Clang).  One type
                           operand. */
  bok_is_member_object_pointer,
                        /* __is_member_object_pointer (Clang).  One type
                           operand. */
  bok_is_member_pointer,/* __is_member_pointer (Clang).  One type operand. */
  bok_is_object,        /* __is_object (Clang).  One type operand. */
  bok_is_pointer,       /* __is_pointer (GNU, Clang).  One type operand. */
  bok_is_reference,     /* __is_reference (Clang).  One type operand. */
  bok_is_rvalue_reference,
                        /* __is_rvalue_reference (Clang).  One type operand. */
  bok_is_scalar,        /* __is_scalar (Clang).  One type operand. */
  bok_is_signed,        /* __is_signed (Clang).  One type operand. */
  bok_is_unsigned,      /* __is_unsigned (Clang).  One type operand. */
  bok_is_void,          /* __is_void (Clang).  One type operand. */
  bok_is_volatile,      /* __is_volatile (GNU, Clang).  One type operand. */
  bok_is_bounded_array, /* __is_bounded_array (Clang).  One type operand. */
  bok_is_unbounded_array,
                        /* __is_unbounded_array (GNU, Clang).  One type
                           operand. */
  bok_is_referenceable, /* __is_referenceable (Clang).  One type operand. */
  bok_is_nothrow_convertible,
			/* __is_nothrow_convertible.  Two operands, both
			   types. */
  bok_reference_constructs_from_temporary,
			/* __reference_constructs_from_temporary (GNU and
			   Clang). Two type operands. */
  bok_reference_converts_from_temporary,
			/* GCC's __reference_converts_from_temporary.  Two type
			   operands. */
  bok_is_convertible,
			/* __is_convertible.  Two operands, both types (GNU).*/
  bok_is_trivially_equality_comparable,
			/* __is_trivially_equality_comparable (Clang).  One
			   type operand. */
  bok_is_scoped_enum,	/* __is_scoped_enum.  One operand: A type. */
  bok_is_trivially_relocatable,
			/* __is_trivially_relocatable (Clang).  One type
			   operand. */
  bok_is_invocable,
			/* __is_invocable.  One or more operands, all types. */
  bok_is_nothrow_invocable,
			/* __is_nothrow_invocable.  One or more operands, all
			   types. */
  bok_is_bitwise_cloneable,
			/* __is_bitwise_cloneable (Clang).  One type
			   operand. */
  bok_builtin_is_virtual_base_of,
			/* __builtin_is_virtual_base_of.  Two operands, both
			   types. */
  bok_builtin_is_implicit_lifetime,
			/* __builtin_is_implicit_lifetime.  One type
			   operand. */
  bok_builtin_lt_synthesizes_from_spaceship,
			/* __builtin_lt_synthesizes_from_spaceship.  Two
			   operands, both types. */
  bok_builtin_gt_synthesizes_from_spaceship,
			/* __builtin_gt_synthesizes_from_spaceship.  Two
			   operands, both types. */
  bok_builtin_le_synthesizes_from_spaceship,
			/* __builtin_le_synthesizes_from_spaceship.  Two
			   operands, both types. */
  bok_builtin_ge_synthesizes_from_spaceship,
			/* __builtin_ge_synthesizes_from_spaceship.  Two
			   operands, both types. */
  bok_builtin_is_structural,
			/* __builtin_is_structural.  One type operand. */
  bok_last              /* Marks the end of the list. */
};


#if !ABI_CHANGES_FOR_RTTI
/* This became unnecessary when the language definition was changed
   to allow more static checking of access on throws. */
/* Entry in a linked list identifying the accessible base classes of the
   class of a thrown object; C++ only. */
typedef struct an_accessible_base_class *an_accessible_base_class_ptr;
typedef struct an_accessible_base_class {
  an_accessible_base_class_ptr
		next;
			/* Next in the linked list; NULL for the last entry
			   in the list. */
  a_base_class_ptr
		base_class;
			/* Pointer to an accessible base class of the class
			   of the thrown object. */
} an_accessible_base_class;
#endif /* !ABI_CHANGES_FOR_RTTI */


/* Description of a C++ "throw" operation. */
typedef struct a_throw_supplement *a_throw_supplement_ptr;
typedef struct a_throw_supplement {
  a_type_ptr	type;
			/* The type of the object being thrown. */
  a_dynamic_init_ptr
		dynamic_init;
			/* Pointer to the dynamic initialization entry that
			   specifies what is done to pass the throw object
			   to the handler. */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  an_expr_node_ptr
		expr;
			/* When IL lowering does partial lowering on a throw,
			   this points to the lowered code that implements
			   the dynamic initialization.  expr can be NULL in
			   cases where no initialization is required (i.e.,
			   zero-initialization of an empty class). */
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if !ABI_CHANGES_FOR_RTTI
  an_accessible_base_class_ptr
		accessible_base_classes;
			/* If type is a class with base classes, a pointer
			   to a linked list of entries identifying those
			   base classes that are accessible at the point of
			   the throw; NULL otherwise. */
#endif /* !ABI_CHANGES_FOR_RTTI */
  a_routine_ptr	destructor;
			/* Destructor to be called by the runtime to destroy
			   the thrown object.  NULL if not needed. */
} a_throw_supplement;


/* Description of a C++ condition in a selection or iteration statement. */
typedef struct a_condition_supplement *a_condition_supplement_ptr;
typedef struct a_condition_supplement {
  a_scope_ptr	scope;
			/* The scope established for the declaration of the
			   variable that is initialized in the condition.
			   The scope for the dependent statement of the
			   then- or else- clause of an if-statement or for
			   the dependent statement of the switch-, while-,
			   or for-statement is nested inside it.  With loop
			   statements the scope is terminated and reentered
			   with each iteration. */
  a_dynamic_init_ptr
		dynamic_init;
			/* The dynamic init entry representing the
			   initialization of the variable declared in the
			   condition.  With loop statements the dynamic
			   initialization takes place with each iteration. */
  an_expr_node_ptr
		expr;
			/* The value to be tested in the selection or
			   iteration statement, i.e., the (possibly converted)
			   variable declared in the condition. */
  a_statement_ptr
		initialization;
			/* The optional initialization statement in a C++17
			   selection statement.  E.g., in
			       if (init(); x == 0) ...
			   this represents the "init();" statement.  Note that
			   this is always NULL for the condition in a "for"
			   loop (where the "initialization" is recorded in
			   "a_for_loop" entries) or "while" loop (where it is
			   not permitted). */
} a_condition_supplement;


/* Description of a C++ "new" or "delete" operation. */
typedef struct a_new_delete_supplement {
  a_bit_field	is_new:1;
			/* TRUE for new, FALSE for delete. */
  a_bit_field	placement_new:1;
			/* TRUE for a "placement" new. */
  a_bit_field	aligned_version:1;
			/* TRUE if an alignment argument is present. */
  a_bit_field	array_delete:1;
			/* TRUE if this is an array delete. */
  a_bit_field	global_new_or_delete:1;
			/* TRUE if the "::" scope qualifier was used. */
  a_bit_field	has_new_initializer:1;
			/* For a new, TRUE if the operator has an explicit
			   initializer. So, for example, TRUE for new int(0)
			   and new int(), but FALSE for new int. */
  a_bit_field	new_initializer_is_brace_enclosed:1;
			/* When has_new_initializer is TRUE, this is TRUE if
			   the new-initializer is enclosed in braces, e.g.,
			   new int{1}. */
  a_bit_field	new_initializer_is_paren_aggr_init:1;
			/* This is TRUE if the operator has a parenthesized
			   aggregate initializer. */
  a_bit_field	deducible_type:1;
			/* For a new in a prototype instantiation, TRUE if
			   the type to be allocated was specified by way of
			   placeholder type (e.g., "auto") and could not be
			   resolved at that time because the initializer is
			   dependent. */
  a_bit_field	parenthesized_type_id:1;
			/* TRUE for a new-expression with a parenthesized
			   type-id (as opposed to a non-parenthesized
			   new-type-id). */
  a_type_ptr	type;
			/* The type of the object being allocated for new;
			   the type pointed to by the object pointer for
			   delete. */
  a_routine_ptr	routine;
			/* Routine to call to do allocation (new) or
			   deallocation (delete).  Can be NULL for a
			   template-dependent "new" in a prototype
			   instantiation and for delete of a C++/CLI
			   handle.  Non-NULL when a global sized or any
			   aligned deallocation routine is to be used. */
#if NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
			/* NULL if the new or delete is for an array whose
			   elements are a class type with a constructor or
			   destructor, and the new or delete has been folded
			   into the runtime routine. */
#endif /* NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE */
#if NEW_CAN_BE_FOLDED_INTO_CTOR || DELETE_CAN_BE_FOLDED_INTO_DTOR
			/* NULL if the new or delete has been folded into a
			   constructor or destructor call.  If NULL,
			   dynamic_init will be non-NULL. */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR || DELETE_CAN_BE_FOLDED_INTO_DTOR */
  an_expr_node_ptr
		arg;	/* For new, the argument list for the "new" call,
			   without the first (size_t) argument.  arg is
			   set even when routine == NULL.  A back end must
			   provide the first argument (number of bytes to
			   allocate), which, in the array case, may depend
			   on number_of_elements (see below).  For delete,
			   the pointer to the object to be deleted. */
  a_dynamic_init_ptr
		dynamic_init;
			/* If non-NULL, points to a dynamic initialization
			   entry that indicates the initialization (new) or
			   destruction (delete) to be done. */
  a_dynamic_init_ptr
		freeing_of_storage_on_exception;
			/* If non-NULL (for a "new" when exceptions are
			   enabled), points to a dynamic initialization entry
			   that describes the delete call to be done to free
			   the storage if an exception is thrown before the
			   storage is initialized.  NULL if no deletion is
			   needed. */
  an_expr_node_ptr
		number_of_elements;
			/* When is_new is TRUE and type specifies an array
			   type, number_of_elements is NULL if the array size
			   is known at compile time; otherwise it contains an
			   expression for the run-time number of elements.
			   Note that the type of this expression is not
			   constrained (i.e., it's whatever was in the
			   source code) and may be signed or unsigned. */
} a_new_delete_supplement;


#if MICROSOFT_EXTENSIONS_ALLOWED
/* Description of a C++/CLI "gcnew" operation. */
typedef struct a_gcnew_supplement {
  a_bit_field	has_new_initializer:1;
			/* TRUE if the operator has an explicit initializer.
			   So, for example, TRUE for gcnew int(0) and
			   gcnew int(), but FALSE for gcnew int. */
  a_bit_field	is_cli_array:1;
			/* TRUE if this gcnew expression is a gcnew
			   initializing a CLI array type. */
  a_type_ptr	type;
			/* The type of the object being allocated by gcnew. */
  an_expr_node_ptr
		cli_array_dimension_lengths;
			/* If is_cli_array is TRUE, a list of expressions
			   describing the lengths of each dimension of a
			   CLI array.  If has_new_initializer is TRUE, the
			   expression list was scanned from source in the
			   array's new-init.  If has_new_initializer is FALSE,
			   the expression list contains constant integers
			   representing the length of each dimension of the
			   array inferred from the array-init.  For prototype
			   instantiations, cli_array_dimension_lengths may be
			   NULL if a new-init is not present.  In C++/CX
			   mode, if is_cli_array is TRUE and dynamic_init is a
			   dik_constructor entry, cli_array_dimension_lengths
			   is NULL because the dynamic_init entry describes
			   the initialization.  This may also be NULL in error
			   cases. */
  a_dynamic_init_ptr
		dynamic_init;
			/* If non-NULL, points to a dynamic initialization
			   entry that indicates the initialization to be done.
			   If is_cli_array is TRUE, and dynamic_init is NULL,
			   an array initializer was not present.  Otherwise, if
			   is_cli_array is TRUE and dynamic_init is non-NULL,
			   this points to a dik_nonconstant_aggregate that
			   describes the array-init.  Unlike native arrays, the
			   aggregate for a CLI array does not specify the
			   initialization for elements that do not have
			   initializers in the source; the elements (either
			   value types or handles) are assumed to all be
			   initialized to zero.  In C++/CX mode, this may
			   point to a dik_constructor entry when is_cli_array
			   is TRUE.  In that case the dik_constructor entry
			   describes the initialization and
			   cli_array_dimension_lengths is NULL. */
} a_gcnew_supplement;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
/* Supplement for an expression node of kind enk_lowered_eh_construct,
   indicating the partially-lowered form of a function prologue for
   exception handling. */
typedef struct an_eh_prologue_supplement *an_eh_prologue_supplement_ptr;
typedef struct an_eh_prologue_supplement {
  a_routine_ptr	routine;
			/* The routine whose supplement this is.  Useful when
			   doing inlining. */
#if GENERATE_EH_TABLES
  a_variable_ptr
		region_table,
		array_table;
			/* The variables for the region table and array table
			   arrays (generated by IL lowering). */
#endif /* GENERATE_EH_TABLES */
} an_eh_prologue_supplement;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */


enum a_local_expr_node_ref_kind : a_byte {
  lerk_none,		/* Used for initialization only. */
#if PROTOTYPE_INSTANTIATIONS_IN_IL
  lerk_generic_sizeof,	/* A template-dependent expression used as an argument
			   for a sizeof, alignof, or uuidof construct. */
  lerk_tpl_param_expr,	/* The expression in a
			   ck_template_param/tpck_expression constant. */
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */
  lerk_array_bound,	/* The expression for an array bound that, despite
			   being constant, refers to a local variable.  This
			   can occur for initialized const variables and for
			   non-const variables that appear in unselected
			   branches of folded constant expressions. */
  lerk_dep_array_bound,	/* Like lerk_array_bound, but for the expressions
			   under a dependent constant giving the bound, rather
			   than the "expr" field of the constant. */
  lerk_decltype,	/* An expression used as an argument for a decltype,
			   splice, or GNU typeof construct. */
  lerk_bit_field_width,	/* The expression for the width of a bit-field that
			   refers to a local variable. */
  lerk_constant_expr,	/* The backing expression of a constant entry. */
  lerk_scoped_expr,	/* A "scoped expression" (currently used only for
                           attribute arguments). */
};


typedef struct a_local_expr_node_ref *a_local_expr_node_ref_ptr;
typedef struct a_local_expr_node_ref {
  /* Entities in file-scope memory region cannot directly refer to function-
     local entities.  To work around that constraint for function-local
     expression nodes, an implicit referencing mechanism is used.  The
     implicit references are represented by a_local_expr_node_ref entries
     stored in the function's memory region: Each entry points to both the
     referenced expression and to the entity in file-scope memory that
     implicitly refers to that expression.  The list of a_local_expr_node_ref
     entries can then be searched whenever the reference must be resolved for
     a given entity in file-scope memory.  (This technique is similar to that
     enabled by a_local_static_variable_init entries.) */
  a_local_expr_node_ref_ptr
		next;
			/* Pointer to the next reference in the current
			   (function or block) scope. */
  an_expr_node_ptr
		expr;
			/* Pointer to the referenced expression. */
  a_local_expr_node_ref_kind
		kind;
			/* Identifies the nature of the construct that
			   causes the reference. */
  a_tagged_pointer
		referrer;
			/* Tag and generic pointer identifying the entity (in
			   file scope memory region) implicitly referring to
			   expr. */
} a_local_expr_node_ref;

/*
A structure for representing reattempt conditions for a deferred constant
evaluation.
*/
struct a_const_eval_reattempt_state {
  a_bit_field   default_arg:1;
			/* TRUE if the deferred evaluation should be
			   reattempted in the context of a default argument
			   transform. */
  a_bit_field   default_mem_init:1;
			/* TRUE if the deferred evaluation should be
			   reattempted in the context of a default member
			   initializer transform. */
};  /* a_const_eval_reattempt_state */

typedef struct an_expr_node {
  /* A single expression node. */
  a_type_ptr    type;
                        /* The type of the expression. */
  a_type_ptr    orig_lvalue_type;
			/* If the expression has been converted from a
			   glvalue to a prvalue simply by clearing the
			   is_lvalue or is_xvalue flag, this records the type
			   the glvalue had (which may have cv-qualifiers that
			   were dropped in the prvalue type).  Also set for
			   casts to reference type (is_reference_cast is TRUE),
			   to indicate the underlying type of the cast.  Also
			   set on enk_constant nodes that are the result of a
			   glvalue-to-prvalue conversion.  NULL otherwise.
			   Note that this field will typically be NULL for
			   expressions that were created during the lowering
			   process (an exception is made for temporary
			   variables for so-called "troublesome aggregate
			   constants" where a const qualification has been
			   added). */
  an_expr_node_ptr
                next;
                        /* When this node is part of a list of operands, this
                           field is used to link them together; otherwise it is
                           NULL. */
  ENUM_TYPE_FOR_BIT_FIELD(an_expr_node_kind)
		kind:NUM_BITS_FOR_EXPR_NODE_KIND;
			/* Identifies what kind of node this is.  This field
			   determines which member of the union to use. */
  a_bit_field	is_lvalue:1;
			/* TRUE if the expression is an lvalue.  FALSE if the
			   expression is something else, e.g., an rvalue or an
			   error node.  C function designators have this
			   field TRUE even though a function designator is
			   not an "lvalue" according to the C standard.
			   Note that the value here indicates how the node
			   is being used, i.e., it includes any implicit
			   lvalue-to-rvalue or function-to-pointer conversion
			   (but not array-to-pointer conversion, which is
			   handled by the eok_array_to_pointer operator). */
  a_bit_field	is_xvalue:1;
			/* TRUE if the expression is a C++11 xvalue, meaning
			   a value created by an rvalue reference cast or
			   rvalue reference return from a function.  Never
			   TRUE at the same time as is_lvalue.  An xvalue
			   is treated as the specifier for an object in
			   storage, like an lvalue is, as opposed to a value,
			   like a prvalue is.  Like is_lvalue, this field
			   can be cleared to indicate that an rvalueable
			   field has been converted to a prvalue. */
  a_bit_field	result_is_not_used:1;
			/* TRUE if the result of evaluating this node is
			   discarded.  The most common case is a void
			   expression.  However, other subtler cases are
			   possible too.  For example, "b ? X() : x", with x
			   of class type X, produces an enk_temp_init node
			   pointing to the eok_question operation, but the
			   result of that operation is discarded because the
			   temporary is initialized by the dependent
			   enk_temp_init nodes that are the operands of the
			   eok_question or eok_comma operator (see also the
			   class_rvalue_initialized_through_master_entry flag
			   in a_dynamic_init). */
  a_bit_field	is_initialization_guard:1;
			/* TRUE if this node is a "?" that guards a first-time
			   test on an initialization.  When generating
			   thread-safe code, the "?" and the first assignment
			   within it should be rendered as an atomic
			   test-and-set. */
  a_bit_field	generated_default_arg:1;
			/* TRUE if this node is a copy of a default argument
			   expression pointed to by a param-type entry; one
			   such copy is associated with each call that uses
			   the default argument. */
  a_bit_field	marked_as_gnu_extension:1;
			/* TRUE if the expression was preceded by the GNU
			   keyword __extension__. */
  a_bit_field	is_static_cast:1;
			/* TRUE if this node represents a static_cast in
			   the source.  Set only on enk_temp_init nodes and
			   on eok_cast, eok_XXX_cast, eok_lvalue_adjust,
			   and eok_class_rvalue_adjust enk_operation nodes;
			   will be FALSE everywhere else. */
  a_bit_field
		is_functional_notation_cast:1;
			/* TRUE if this node represents a cast using functional
			   notation in the source (e.g., "double(x)" or
			   "double{x}"). */
  a_bit_field
		is_brace_notation_cast:1;
			/* TRUE if this node represents a cast using brace
			   notation in the source (e.g., "double{x}"). */
  a_bit_field	is_objectless_nonstatic_data_mem_ref:1;
			/* TRUE if this node represents a reference to a
			   nonstatic data member without an object, as is
			   permitted in unevaluated operands. */
  a_bit_field	is_pack_expansion:1;
			/* TRUE if this is a variadic template pack expansion,
			   i.e., an expression followed by "...".  Note that
			   a pack expansion is marked only once, and at the
			   level that actually appears in a list, which might
			   mean the flag is set on an implicit conversion node
			   that was added on top of the node that appears
			   explicitly in the source, or the flag is set in the
			   constant entry that appears in the initializer list
			   for an aggregate (there's an is_pack_expansion flag
			   also in a_constant). */
  a_bit_field	is_shallow_copy:1;
			/* TRUE if this is a shallow copy created by copy_node
			   (and thus shares pointers with the original
			   expression). */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_safe_cast:1;
			/* TRUE when the operation is a C++/CLI safe_cast
			   in the source.  Similar to is_static_cast. */
  a_bit_field	element_of_cli_param_array_arg:1;
			/* TRUE when this is an argument matching a
			   C++/CLI parameter array. */
  a_bit_field
		is_cli_typeid:1;
			/* TRUE for a typeid entry that comes from a C++/CLI
			   typeid, of the form T::typeid.  In that case,
			   variant.typeid_info.type_with_opt_expr only provides
			   the type T and no operand expression.  Note that
			   cv-qualifiers on T are not removed (we want to keep
			   any typedefs) so a back end should remove them
			   before selecting the appropriate System::Type
			   entry. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
  a_bit_field	is_non_normalized_boolean_controlling_expr:1;
			/* TRUE when this expression is a boolean controlling
			   expression that has not been normalized during the
			   lowering process (presumably because
			   LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS
			   is FALSE). */
#endif /* DO_IL_LOWERING */
#if BACK_END_IS_CP_GEN_BE
  a_bit_field	keep_as_cast_for_cp_gen_be:1;
			/* TRUE to indicate that a cast should be put out
			   for this node in the code generated by the
			   C++-generating back end in spite of the fact
			   that it would normally not appear.  In addition
			   to cast nodes, this flag is also used in the
			   C++-generating back end on a call operation to a
			   conversion function to indicate that the call
			   should be generated as a cast rather than
			   suppressed altogether, as well as on a reference
			   indirection or enk_temp_init when the type or
			   lvalue-to-rvalue conversion must be made
			   explicit, such as for the second or third
			   operand of a folded conditional operation. */
  a_bit_field	needed_in_cp_gen_be:1;
			/* TRUE for the backing expression of a non-type
			   template argument when it must be emitted in
			   place of the constant value.  Set to FALSE after
			   the initial use, so that subsequent references
			   to the template instance will not run into
			   issues with access, visibility, or the need for
			   excessive qualification for names appearing in
			   the expression. */
#endif /* BACK_END_IS_CP_GEN_BE */
  a_bit_field	is_parenthesized:1;
			/* TRUE if the expression was parenthesized in the
			   source code.  In the case of a parenthesized bound
			   function (e.g., "(p->f)()") it is the node 
			   representing the function ("f" in the example) that
			   has this flag set, not the selector expression. */
  a_bit_field
		type_definition_needed:1;
			/* A flag indicating that the type definition must be
			   kept in the IL for the type indicated in the
			   enk_type_operand case. */
  a_bit_field
		volatile_fetch:1;
			/* TRUE if this node represents a fetch from volatile
			   storage. */
  a_bit_field	do_not_interpret:1;
			/* TRUE if the interpreter should not attempt to
			   evaluate this node. */
  a_bit_field
		compiler_generated:1;
			/* TRUE if the node is compiler-generated rather than
			   explicitly present in the source program.  Used,
			   e.g., for casts.  Not used for some nodes where
			   the broader context implies that the node is
			   compiler generated (e.g., in the synthesized
			   definitions of special member functions). */
  a_bit_field
		is_type_constraint:1;
			/* TRUE for an enk_concept_id node that represents a
			   type constraint (i.e., its first template argument
			   is implicit). */ 
  a_bit_field	was_lvalue_temp_initializer:1;
			/* TRUE if this was an initializer expression for an
			   lvalue enk_temp_init node and the latter was dropped
			   again to implement glvalue-to-prvalue conversion. */
  a_source_position
		position;
			/* When kind == enk_operation, the position at which
			   the operator appears in the source.  Otherwise, the
			   starting position of the corresponding construct, or
			   null_source_position if there is no such
			   construct. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		expr_range;
			/* When the node corresponds to an explicit sequence
			   of tokens in the source, the source positions of
			   start and end of the expression.  Otherwise, the
			   source positions where the expression would appear
			   if it were explicit.  May be null_source_range. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  union {
    /* When kind == enk_error or enk_address_of_ellipsis, no variant fields. */
    /* When kind == enk_operation: */
    struct {
      an_expr_operator_kind
                kind;
                        /* What kind of operation it is. */
      a_type_kind
		type_kind;
			/* The kind of type the operation acts on.  E.g., when
			   comparing two complex values it is tk_complex, when
			   adding an integer to a pointer it is tk_pointer, and
			   when adding two vectors of integers the result is
			   tk_vector.  This may differ from the "kind" of the
			   result type, and the "kind" of an operand type.
			   For operations that don't act on a specific type
			   (e.g., eok_call) it is tk_unknown.  For template-
			   dependent operations, it is often tk_template_param
			   (but it may be tk_unknown).  For operations on
			   imaginary values, it is tk_imaginary only if the
			   operation is materially different from the
			   corresponding real floating-point operation (e.g.,
			   imag-imag and imag*real are tk_float, but imag*imag
			   is tk_imaginary).  For operations on class types
			   this field is always tk_struct (not tk_class or
			   tk_union). */
      a_bit_field
		returns_lvalue_instead_of_usual_rvalue:1;
			/* TRUE if the operation is an assignment (simple or
			   compound), prefix ++/--, or "?" or "," operator
			   that returns an lvalue in C++ where the C operation
			   would return an rvalue.  FALSE otherwise, including
			   for other operations and for these operations when
			   they do return rvalues.  Generally TRUE only in C++,
			   but can be TRUE in gcc mode when an rvalue is
			   reverted to an lvalue (IL lowering eliminates that
			   later by rewriting it in rvalue form).  When this
			   field is TRUE, is_lvalue will also be TRUE.
			   In C++11, can also be TRUE for a "?" or ","
			   operator that returns an xvalue; is_xvalue will
			   also be TRUE (and is_lvalue FALSE). */
      a_bit_field
		is_reinterpret_cast:1;
			/* TRUE when the operation was a reinterpret_cast
			   in the source. */
      a_bit_field
		is_reinterpret_like_cast:1;
			/* TRUE when the operation has reinterpret_cast
			   semantics (but perhaps did not appear as such in
			   the source code). */
      a_bit_field
		is_const_cast:1;
			/* TRUE when the operation was a const_cast in the
			   source. */
      a_bit_field
		is_reference_cast:1;
			/* TRUE when the operation is a cast to a reference
			   type in the source.  This applies to several
			   different cast operations that can be used as
			   part of a reference cast (e.g.,
			   eok_base_class_cast).  When a single source cast
			   is represented as several IL cast operators, this
			   flag is set in the topmost. */
      a_bit_field
		is_rvalue_reference_cast:1;
			/* TRUE when the operation is a cast to an rvalue
			   reference type in the source.  Will be TRUE only
			   when is_reference_cast is also TRUE, and only in
			   eok_ref_cast and eok_ref_dynamic_cast nodes. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_bit_field
		is_tracking_reference_cast:1;
			/* TRUE when the operation is a cast to a C++/CLI
			   tracking reference type in the source.  Will be
			   TRUE only when is_reference_cast is also TRUE. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      a_bit_field
		implicit_in_member_naming:1;
			/* TRUE for a base class cast that is implicit in
			   the name used in referring to a class member. */
      a_bit_field
		implicit_step_of_explicit_cast:1;
			/* TRUE for a cast to a base or derived class that
			   was implicitly generated as part of realizing
			   an explicit cast to a related class.  Used
			   also for pointer-to-member casts. */
      a_bit_field
		is_conversion_call:1;
			/* TRUE for a call that does an explicit or implicit
			   conversion, e.g., a conversion function call
			   generated for a cast. */
      a_bit_field
		arg_dependent_lookup_suppressed_on_call:1;
			/* TRUE for a call on which argument-dependent
			   lookup was a possibility but was suppressed because
			   the function name was not followed by a left
			   parenthesis.  Note that there are other things
			   that will suppress argument-dependent lookup (e.g.,
			   using a qualified name) but those are not reflected
			   in this flag. */
      a_bit_field
		call_with_qualified_function_name:1;
			/* TRUE for a call on which argument-dependent
			   lookup was suppressed because the name of the
			   function was qualified. */
#if BACK_END_IS_CP_GEN_BE
      a_bit_field
		only_found_through_arg_dependent_lookup:1;
			/* TRUE if this was an unqualified call and the called
			   function was only found through argument-dependent
			   lookup (and not through ordinary lookup). */
      a_bit_field
		called_through_address_of_overload_set:1;
			/* TRUE for a call of the form "(&func)(args...)"
			   where func is an overload set. */
#endif /* BACK_END_IS_CP_GEN_BE */
      a_bit_field
                call_uses_operator_syntax:1;
			/* TRUE for a call expression that results from
			   operator syntax rather than function-call syntax
			   (e.g., "a+b" as opposed to "operator+(a,b)").  The
			   C++-generating back end must maintain this form when
			   the operator is found via argument-dependent lookup
			   and the call occurs in a context in which a
			   member operator might be found by ordinary
			   lookup of the name and thus suppress ADL.  Also
			   TRUE for implicit calls of the Invoke function of
			   a C++/CLI delegate and for implicit calls of C++11
			   literal operators (resulting from user-defined
			   literals). */
#if GNU_EXTENSIONS_ALLOWED
      a_bit_field
		is_gnu_two_operand_question_mark:1;
			/* TRUE for an eok_question operator that came from
			   the GNU two-operand form, e.g., x ?: y.  A
			   synthesized second operand is present in the
			   operand list. */
#endif /* GNU_EXTENSIONS_ALLOWED */
      a_bit_field
		pointer_operand_is_second:1;
			/* TRUE for eok_subscript or eok_padd in the case where
			   the pointer operand is the second one. */
      a_bit_field
		is_virtual_call:1;
			/* On the member call operators eok_dot_member_call
			   and eok_points_to_member_call, indicates that the
			   call uses virtual semantics.  Note specifically
			   that a call to a virtual function with this flag
			   FALSE is not a virtual call (perhaps because the
			   function was named with a qualified name). */
#if MICROSOFT_EXTENSIONS_ALLOWED
      ENUM_TYPE_FOR_BIT_FIELD(a_rewritten_property_reference_kind)
		rewritten_property_reference_kind:3;
			/* If this is the "put" call in a rewritten
			   Microsoft property reference involving a
			   compound assignment or other operator where both
			   a "get" accessor and a "put" accessor are called
			   as part of the expansion, indicates the kind of
			   operator.  The same applies to an eok_assign
			   operator or a call to an overloaded assignment
			   operator in a compound assignment operation that
			   was decomposed to the corresponding simple
			   operations using operator synthesis.  Finally,
			   this is also set in the top-level
			   compiler-generated eok_comma node that holds
			   temporary initialization for one of these
			   operations.  Otherwise, rprk_none. */
      a_bit_field
		requires_runtime_cast_check:1;
			/* TRUE for a C++/CLI cast operation that requires a
			   runtime check (e.g., it's part of a safe_cast).
			   Can appear on eok_cast (for handles),
			   eok_derived_class_cast, and eok_ref_cast (for
			   casts to tracking references). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if BACK_END_IS_C_GEN_BE
      a_bit_field
		has_deferred_ampersand:1;
			/* TRUE for certain eok_dot_field nodes that are
			   operands of an eok_lvalue_adjust or
			   eok_address_of node and whose first operand is
			   not an lvalue.  Such nodes may be generated by
			   the C-generating back end as a comma expression,
			   assigning to a variable and then referencing the
			   member of the variable.  Because a comma
			   expression cannot be the operand of "&", which
			   is part of the ordinary expansion of an
			   eok_lvalue_adjust or eok_address_of, the "&"
			   must be deferred and applied to the second
			   operand of the comma expression instead. */
#endif /* BACK_END_IS_C_GEN_BE */
      a_bit_field
		eval_left_to_right:1;
			/* TRUE if the operands must be evaluated in the order
			   in which they appear on the operands list, except
			   that for eok_dot_pm_call and eok_points_to_pm_call
			   the selector object/pointer (second operand) is
			   evaluated before the pointer-to-member (first
			   operand). */
      a_bit_field
		eval_right_to_left:1;
			/* TRUE if the operands must be evaluated in the
			   reverse order in which they appear on the operands
			   list. */
      a_bit_field
		is_consteval_call:1;
			/* TRUE if this is a call to a "consteval" function.
			   For use by the front end only. */
#if BACK_END_IS_CP_GEN_BE
      a_bit_field
		suppress_top_level_parens:1;
			/* TRUE if no parentheses should be placed around
			   this expression, even if they otherwise would be
			   emitted. */
#endif /* BACK_END_IS_CP_GEN_BE */
      an_expr_node_ptr  
                operands;
                        /* The list of operands. */
    } operation;

    /* When kind == enk_constant: */
    struct {
      a_constant_ptr
                ptr;
                        /* A pointer to the constant.  This may be a shared
			   constant. */
      a_name_reference_ptr
		name_reference;
			/* If non-NULL, points to information about the
			   form of reference to a name that this expression
			   node refers to. */
    } constant;
    /* When kind == enk_variable: */
    struct {
      a_variable_ptr
                ptr;
                        /* A pointer to the variable. */
      a_name_reference_ptr
		name_reference;
			/* If non-NULL, points to information about the
			   form of reference to a name that this expression
			   node refers to. */
    } variable;
    /* When kind == enk_routine: */
    struct {
      a_routine_ptr
                ptr;	/* A pointer to the routine.  May be NULL for nodes
			   used to representing vacuous destructors.  (The
			   name_reference field records the form of the
			   destructor.) */
      a_name_reference_ptr
		name_reference;
			/* If non-NULL, points to information about the
			   form of reference to a name that this expression
			   node refers to. */
    } routine;
    /* When kind == enk_field: */
    struct {
      a_field_ptr
		ptr;
			/* A pointer to the field.  Used as an operand to an
			   eok_dot_field or eok_points_to_field operation. */
      a_name_reference_ptr
		name_reference;
			/* If non-NULL, points to information about the
			   form of reference to a name that this expression
			   node refers to. */
    } field;
    /* When kind == enk_temp_init or enk_lambda: */
    /* Mostly C++-only, but also used for C99 compound literals. */
    /* The result of this operator is a temporary.  It's an lvalue if
       is_lvalue is TRUE, an rvalue otherwise. */
    struct {
      a_dynamic_init_ptr
		dynamic_init;
			/* Dynamic initialization entry that does the
			   initialization for the temporary.  For a lambda,
			   this is the aggregate initialization that copies
			   the captured variables to the fields of the closure
			   class.  Also includes the destruction of the closure
			   object if necessary.  Still present if no
			   initialization is needed (indicates dik_none in
			   that case). */
      union {
        /* When kind == enk_temp_init: */
        a_type_ptr
		type;	/* In some cases where the type of a cast as it
			   appeared in the source is not reflected in the
			   type of this node, this represents the former.
			   For example:
			      using A = double[];
			      (A{1, 2});
			   Here the type of the initializer provides a
			   dimension for the array type, causing the typedef
			   representation to be dropped in the node type.
			   NULL in most cases. */
        /* When kind == enk_lambda: */
        a_lambda_ptr
		lambda;	/* Pointer to an entry that describes the associated
			   lambda. */
      } source;
    } init;
    /* When kind == enk_new_delete: */
    a_new_delete_supplement_ptr
		new_delete;
			/* Pointer to an entry that describes the new or
			   delete operation (C++ only). */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* When kind == enk_gcnew: */
    a_gcnew_supplement_ptr
		gcnew_info;
			/* Pointer to an entry that describes the gcnew
			   operation (C++/CLI only). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* When kind == enk_throw: */
    a_throw_supplement_ptr
		throw_info;
			/* Information about the object being thrown in a
			   throw expression; NULL when no object is
			   specified (i.e., a "rethrow" of the current
			   throw object). */
    /* When kind == enk_condition (C++ only): */
    a_condition_supplement_ptr
		condition;
			/* Information describing a C++ condition in a
			   selection or iteration statement. */
    /* When kind == enk_object_lifetime: */
    struct {
      an_expr_node_ptr
		expr;	/* The full expression with which the object lifetime
			   is associated.  This expression is standardized
			   to integer/boolean when it appears in the context
			   of a boolean controlling expression in some
			   configurations. */
      an_object_lifetime_ptr
		ptr;	/* The object lifetime itself. */
    } object_lifetime;
    /* When kind == enk_typeid (C++ only): */
    struct {
      an_expr_node_ptr
		type_with_opt_expr;
			/* A list with one or two expression nodes.  The first
			   is always an enk_type_operand node that records
			   either the type specified in the corresponding
			   typeid(...) expression, or the type of the specified
			   expression operand.  (In either case, top-level type
			   qualifiers are removed.)  If an expression operand
			   was specified, a second expression node is present
			   describing that expression. */
      a_bit_field
		is_dynamic:1;
			/* TRUE if the result value depends on the dynamic
			   type of the operand expression. */
    } typeid_info;
    /* When kind == enk_sizeof, kind == enk_alignof, or
       kind == enk_datasizeof: */
    struct {
      a_byte_boolean
		is_type;
			/* TRUE if the sizeof is sizeof(type); FALSE for
			   sizeof expression.  Likewise for alignof and
			   __datasizeof. */
      a_byte_boolean
		is_std_alignof;
			/* TRUE if this node is for the standard C++11 alignof
			   feature. */
      union {
        /* When is_type == TRUE: */
        a_type_ptr
		type;	/* The type whose size/alignment is needed. */
        /* When is_type == FALSE: */
        an_expr_node_ptr
		expr;	/* The expression whose size/alignment is needed. */
      } variant;
    } sizeof_info;
    /* When kind == enk_sizeof_pack: */
    struct {
      a_byte_boolean
		is_type;
			/* TRUE for sizeof...(T) where T is a type template
			   parameter pack. */
      a_byte_boolean
		is_template_template;
			/* TRUE if the operand is a template template
			   parameter pack. */
      union {
        /* When is_type is TRUE and is_template_template is FALSE: */
        a_type_ptr
		type;	/* The argument of sizeof...(T), in type form. */
        /* When is_type is FALSE and is_template_template is FALSE: */
        an_expr_node_ptr
		expr;	/* The argument of sizeof...(x), in expression form. */
        /* When is_template_template is TRUE: */
        a_template_ptr
		templ;	/* The argument of sizeof...(TT), in template form. */
      } variant;
    } sizeof_pack;
    /* When kind == enk_statement: */
    a_statement_ptr
		statement;
			/* Enclosed compound statement. */
    /* When kind == enk_reuse_value: */
    a_dynamic_init_ptr
		reused_value_init;
			/* The dynamic initialization that creates the value
			   to be reused. */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    /* When kind == enk_lowered_eh_construct: */
    struct {
      a_lowered_eh_construct_kind
		kind;	/* Kind of construct. */
      union {
        /* When kind == leck_caught_object_address: */
        a_handler_ptr
		caught_object_handler;
        /* When kind == leck_thrown_object_address, no variant fields. */
        /* When kind == leck_cleanup_state or
                        leck_unreachable_cleanup_state: */
#if GENERATE_EH_TABLES
        a_cleanup_region_number
		cleanup_region_number;
			/* Region number at which to start cleanup. */
#else /* !GENERATE_EH_TABLES */
        a_dynamic_init_ptr
		cleanup_ptr;
			/* Destruction at which to start cleanup. */
#endif /* GENERATE_EH_TABLES */
        /* When kind == leck_function_prologue: */
        an_eh_prologue_supplement_ptr
		prologue_info;
        /* When kind == leck_function_epilogue: */
        a_routine_ptr
		epilogue_routine;
        /* When kind == leck_catch_epilogue: */
        a_handler_ptr
		epilogue_handler;
        /* When kind == leck_try_epilogue: */
        a_try_supplement_ptr
		epilogue_try_block;
        /* When kind == leck_exception_caught, no variant fields. */
        /* When kind == leck_exception_started, no variant fields. */
#if !GENERATE_EH_TABLES
        /* When kind == leck_initialization_completed: */
        a_dynamic_init_ptr
		dynamic_init;
			/* The initialization that is now completed. */
#endif /* !GENERATE_EH_TABLES */
        /* When kind == leck_internal_try: */
        /* Note that an leck_internal_try expression has a void type,
           i.e., it does not pass through the value of the try_expr. */
        an_expr_node_ptr
		try_and_catch_expr;
			/* A list of two nodes.  The first node is the
			   expression to evaluate.  The second node is the
			   expression to execute if an exception is thrown
			   while evaluating the first node. */
      } variant;
    } lowered_eh;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    /* When enk_result_of_overriding_function, no variant fields. */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if VLA_DEALLOCATIONS_IN_IL
    /* When kind == enk_vla_dealloc: */
    a_variable_ptr
		vla_variable;
		       /* Pointer to a VLA object (a variable with a
			  variable length array type), for which memory was
			  previously allocated (stmk_vla_decl) and is
			  deallocated at this point. */
#endif /* VLA_DEALLOCATIONS_IN_IL */
    /* When kind == enk_type_operand: */
    struct {
      a_type_ptr
		type;	/* The type represented by the operand.  NULL for an
			   entry representing the "default:" case of a C11
			   _Generic construct. */
      a_name_reference_ptr
		name_reference;
			/* If non-NULL, points to information about the
			   form of reference to a name that this expression
			   node refers to. */
    } type_operand;
    /* When kind == enk_builtin_operation: */
    struct {
      a_builtin_operation_kind
		kind;	/* The specific operation being represented. */
      an_expr_node_ptr
		operands;
			/* The list of operands. */
    } builtin_operation;
    /* When kind == enk_param_ref: */
    struct {
      unsigned int
		param_num;
			/* The number of the parameter being referenced (the
			   first parameter is number one).  Zero means the
			   "this" parameter (levels_up is always zero in
			   that case). */
      unsigned int
		levels_up;
			/* The number L of parameter lists enclosing the
			   reference to the parameter up to (and including)
			   the one containing the declaration of the
			   referenced parameter.  Zero if the reference is
			   from outside the parameter list that declares the
			   referenced parameter (this includes what is
			   probably the most common case: A trailing return
			   type referring to a parameter of the associated
			   function type).  Examples:
			     typedef struct {} T;
			     void f(T p, decltype(p));                // L = 1
			     void g(T p, decltype(p) (*)());          // L = 1
			     void h(T p, auto (*)()->decltype(p));    // L = 1
			     void i(T p, auto (*)(T q)->decltype(q)); // L = 0
			     void j(T p, auto (*)(decltype(p))->T);   // L = 2
			     void k(T p, int (*(*)(T p))[sizeof(p)]); // L = 1
			   */
    } param_ref;
    /* When kind == enk_braced_init_list: */
    an_expr_node_ptr
		braced_init_list;
			/* A list of expressions (possibly empty) that appear
			   inside a C++11 brace-enclosed initializer list.
			   This kind of node appears only in template prototype
			   instantiations, because in other contexts
			   initializer lists are always resolved to something
			   else (e.g., a constructor call). */
    /* When kind == enk_c11_generic: */
    struct {
      an_expr_node_ptr
		operands;
			/* The list of operands in the _Generic(...) construct
			   in order of appearance.  Types are represented by
			   enk_type_operand nodes, and the "default:" case is
			   represented by an enk_type_operand that has a NULL
			   pointer for its variant.type_operand.type field. */
      an_expr_node_ptr
		result;
			/* The selected expression.  This points to a node in
			   the operands list. */
    } c11_generic;
#if BUILTIN_FUNCTIONS_ENABLED
    /* When kind == enk_builtin_choose_expr: */
    struct {
      an_expr_node_ptr
		operands;
			/* A list of three operands that appeared in the
			   __builtin_choose_expr construct. */
      a_bit_field
		choose_first:1;
			/* TRUE if the first operand has a "true" value,
			   indicating that the overall construct should
			   evaluate the second operand; otherwise, the third
			   operand should be evaluated. */
    } builtin_choose_expr;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
    /* When kind == enk_await or enk_yield: */
    struct {
      an_expr_node_ptr
		operand;
			/* The operand of the co_await operator.  (A call to a
			   yield_value member of the promise type in the case
			   of a "co_yield" operation.) */
      an_expr_node_ptr
		ready_resume_suspend;
			/* A list of three expressions representing the calls
			   to await_ready, await_resume, and await_suspend
			   needed to implement the "co_await" operation. */
    } await_info;
    /* When kind == enk_fold: */
    struct {
      an_expr_node_ptr
		operands;
			/* Usually one or two operands, depending on whether
			   this represents a unary or binary fold.  Partial
			   substitutions, however, can lead to additional
			   operands. */
      a_token_kind
                operator_token;
                        /* The operator token in the fold. */
      a_bit_field
		left_associative:1;
			/* TRUE if this is a "left (associative) fold". */
    } fold;
    /* When kind == enk_initializer: */
    struct {
      a_dynamic_init_ptr
		dyn_init;
			/* The dynamic initializer that was folded. */
    } initializer;
    /* When kind == enk_concept_id: */
    struct {
      a_template_ptr
		concept_template;
			/* The concept template this concept-id refers to. */
      a_template_arg_ptr
		args;
			/* The template arguments passed to the concept. */
    } concept_id;
    /* When kind == enk_template_name: */
    a_template_ptr
		template_name;
			/* The template this template-name refers to. */
    /* When kind == enk_token_sequence: */
    struct {
      an_expr_node
		*interpolations;
			/* The expressions appearing in interpolators (in
			   lexical order). */
      a_token_sequence
		*tokens;
			/* A token sequence resulting from a reflection
			   operation like "^^{ int \[str, n]; }", excluding
			   the interpolated expressions. */
    } token_sequence;
    /* When kind == enk_requires: */
    struct {
      an_expr_node_ptr
		requirements;
			/* The list of requirements.  Type-requirements are
			   represented with enk_type_operand nodes.  Compound
			   and nested requirements are represented with
			   enk_compound_req and enk_nested_req nodes,
			   respectively. */
      a_param_type_ptr
		parameters;
			/* The optional list of parameters. */
    } requires_expr;
    /* When kind == enk_compound_req: */
    struct {
      an_expr_node_ptr
		expr_and_constraint;
			/* A list of one or two nodes.  The first node is a
			   general expression.  The optional second node is an
			   enk_concept_id node representing a type
			   constraint. */
      a_bit_field
		is_noexcept:1;
			/* TRUE if this compound requirement requires that the
			   first node in exor_and_constraint is non-throwing
			   (after substitution of template arguments). */ 
    } compound_req;
    /* When kind == enk_nested_req: */
    struct {
      an_expr_node_ptr
		constraint;
    } nested_req;
    /* When kind == enk_const_eval_deferred: */
    struct {
      an_expr_node_ptr
		wrapped;
			/* The expression to be re-evaluated. */
      a_const_eval_reattempt_state
		reattempt_state;
			/* The associated reattempt conditions. */
    } const_eval_deferred;
    /* When kind == enk_pack_index: */
    struct {
      an_expr_node_ptr
		expr;	/* The pack expression. */
      an_expr_node_ptr
		index_expr;
			/* The index constant-expression. */
    } pack_index;
  } variant;
  union {
    an_expr_rescan_info_entry_ptr
		rescan_info;
			/* For expressions scanned in templates that might
			   be rescanned later to redo semantic analysis,
			   points to extra front-end-only information that
			   is needed for the rescan.  NULL otherwise. */
    an_expr_node_ptr
		next_avail;
			/* For file-scope expressions that have been
			   reclaimed, the next node on the available list. */
  } extra;
} an_expr_node;

/*
Data structures related to statements:
*/
enum a_statement_kind : a_byte {
  /* Kinds of statements. */
  stmk_expr,		/* Evaluate expression, throw away its value. */
  stmk_if,		/* if-then-else. */
  stmk_constexpr_if,	/* C++17 constexpr if-then-else. */
  stmk_if_consteval,	/* C++23 "if consteval" statement. */
  stmk_if_not_consteval,
			/* C++23 "if not consteval" (or "if !consteval")
			   statement. */
  stmk_while,		/* Loop, test at top. */
  stmk_goto,		/* Goto. */
  stmk_label,		/* Code label. */
  stmk_return,		/* Return (not for a coroutine). */
  stmk_coroutine,	/* Coroutine information. */
  stmk_coroutine_return,
			/* Return (for a coroutine). */
  stmk_block,		/* A list of statements, possibly one with its
			   own declarations and scope. */
  stmk_end_test_while,	/* Loop, test at bottom. */
  stmk_for,		/* For loop. */
  stmk_range_based_for,	/* Range-based-for loop. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  stmk_for_each,	/* For each loop. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  stmk_switch_case,	/* A "case ... :" or "default:" construct. */
  stmk_switch,		/* Switch. */
  stmk_init,		/* Do a dynamic initialization. */
  stmk_asm,		/* "asm" statement (or declaration) or the body of
			   an asm function. */
#if ASM_FUNCTION_ALLOWED
  stmk_asm_func_body,	/* Body of an asm function. */
#endif /* ASM_FUNCTION_ALLOWED */
  stmk_try_block,	/* Try block (C++ only). */
#if MICROSOFT_EXTENSIONS_ALLOWED
  stmk_microsoft_try,	/* Microsoft try-finally or try-except. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  stmk_decl,		/* A declaration statement. */
  stmk_set_vla_size,	/* Set the size of a VLA type. */
  stmk_vla_decl,	/* Declaration of a variable or typedef with
			   variably modified type.  If the variable is a VLA,
			   allocate storage for it. */
#if UPC_EXTENSIONS_ALLOWED
  stmk_upc_notify,	/* Notify statement (split barrier start) */
  stmk_upc_wait,	/* Wait statement (split barrier end) */
  stmk_upc_barrier,	/* Barrier statement (full barrier) */
  stmk_upc_fence,	/* Fence statement */
  stmk_upc_forall,	/* Forall statement */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  stmk_assigned_goto,	/* Assigned GOTO. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  stmk_empty,		/* Empty ("null") statement. (";" in C/C++) */
  stmk_stmt_expr_result,
			/* A statement in a GNU statement expression producing
			   the result value of that expression.  Always the
			   last statement of its block. */
  stmk_last
};


/* Extra information about a statement of kind stmk_block (block statement). */
typedef struct a_block *a_block_ptr;
typedef struct a_block {
  a_source_position
		final_position;
                        /* Source position of the end of the block, for
			   symbolic debug purposes. */
  a_scope_ptr   assoc_scope;
                        /* Pointer to the associated scope, or NULL if there
                           is no associated scope.  This is only used for
                           blocks that have local declarations.  Specifically,
                           it is not used for the scope associated with a
                           function (i.e., the block for the compound 
                           statement that is the body of a function has
                           assoc_scope == NULL). */
  an_object_lifetime_ptr
		lifetime;
			/* The object lifetime associated with this block,
			   or NULL if there isn't one.  Used only when
			   assoc_scope is NULL but nevertheless there is
			   an object lifetime region associated with this
			   block (e.g., for cfront dependent statements). */
  a_bit_field	end_of_block_reachable:1;
			/* TRUE if the end of the block is reachable.  The
			   safe setting is TRUE. */
  a_bit_field	is_statement_expression:1;
			/* TRUE if this block is the outer block created for
			   a GNU statement expression. */
  a_bit_field	implicit_scope_not_allowed:1;
			/* TRUE if this block was added by the front end
			   merely to allow grouping some statements together
			   and a scope should not be added to it (e.g., to
			   contain generated temporaries). */
#if UPC_EXTENSIONS_ALLOWED
  a_upc_access_method
		upc_access_method;
			/* Indicates the default access method for shared
			   variables within this block (can be modified with
			   a UPC pragma). */
#endif /* UPC_EXTENSIONS_ALLOWED */
} a_block;

/* 
Information about a for loop, pointed to from an stmk_for statement.  Note
that the test expression and dependent statement are not mentioned in this
construct.  They are pointed to by the expr and variant.for_loop.statement
fields of the statement entry.
*/
typedef struct a_for_loop *a_for_loop_ptr;
typedef struct a_for_loop {
  a_statement_ptr
	        initialization;
			/* Pointer to a statement that represents the
			   loop initialization (in C, the initialization
			   expression is wrapped in an stmk_expr statement);
			   NULL if there is none. */
  an_expr_node_ptr
		increment;
			/* Pointer to an expression to be executed at the end
			   of each iteration of the loop; NULL if there is
			   none. */
  a_scope_ptr	for_init_scope;
			/* Pointer to the sck_block scope created for name(s)
			   declared in the for-init statement.  NULL in C++
			   mode if the for-init statement is not a declaration
			   or if use_nonstandard_for_init_scope is TRUE;
			   always NULL in C mode. */
#if UPC_EXTENSIONS_ALLOWED
  an_expr_node_ptr
		affinity;
			/* Pointer to an expression to be tested for affinity
			   before executing each iteration of the loop. */
#endif /* UPC_EXTENSIONS_ALLOWED */
} a_for_loop;


/*
Information about a range-based-for statement ([stmt.ranged]), pointed to from
a stmk_range_based_for statement.  The range-based-for statement takes the
form:

  for ( init-statement   for-range-declaration : for-range-initializer )
                      opt
    statement

which is implemented as:

  {  // range_based_for_scope:
    init-statement
    auto && __range = for-range-initializer;
    auto __begin = begin-expr;
    auto __end = end-expr;
    for ( ;
	  __begin != __end;
	  ++__begin ) {  // iterator_scope:
      for-range-declaration = *__begin;
      statement
    }
  }

The expressions for begin-expr and end-expr depend on the type of "expression",
and can have the following forms (see the standard for specifics):

  type      begin-expr        end-expr
  ----      ----------        --------
  array     __range           __range + bound
  class     __range.begin()   __range.end()
  other     begin(__range)    end(__range)

Note that the (dynamically) initialized variables don't have associated
stmk_init statements because there is no block yet for those statements.
*/
typedef struct a_range_based_for_loop *a_range_based_for_loop_ptr;
typedef struct a_range_based_for_loop {
  a_statement_ptr
                initialization;
                        /* Pointer to an init-statement if the range-based
                           for has one (C++20 and later), NULL otherwise. */
  a_variable_ptr
                iterator;
                        /* Pointer to the iteration variable declared in
                           for-range-declaration.  Dynamically initialized
                           to *__begin.  NULL if there was an error. */
  a_variable_ptr
                range;
                        /* Pointer to the __range temporary variable above.
                           Initialized to (expression). */
  a_scope_ptr   range_based_for_scope;
                        /* An sck_block scope added to surround the
                           range-based-for statement.  It corresponds to the
                           outermost set of braces shown above, and the
                           __range, __begin, and __end variables are declared
                           in this scope.  Their initializations are also
                           evaluated in this scope. */
  a_scope_ptr   iterator_scope;
                        /* An sck_block scope that corresponds to the
                           body of the rewritten loop.  It contains the
                           for-range-declaration as well as the dependent
                           statement of the loop. */
  a_variable_ptr
                begin;
                        /* Pointer to the variable representing the temporary
                           variable __begin above.  Dynamically initialized to
                           one of: __range, __range.begin(), or begin(__range)
                           as appropriate.  NULL if there was an error. */
  a_variable_ptr
                end;
                        /* Pointer to the variable representing the temporary
                           variable __end above.  Dynamically initialized to
                           one of: __range + __bound, __range.end(), or
                           end(__range) as appropriate.  NULL if there was an
                           error. */
  an_expr_node_ptr
                ne_call_expr;
                        /* Expression for the "__begin != __end" test. */
  an_expr_node_ptr
                incr_call_expr;
                        /* Expression for the "++__begin" increment. */
  a_bit_field	use_await:1;
			/* TRUE in the case of a "for await (...)"
			   statement. */
} a_range_based_for_loop;

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Kind of pattern for a collection type in a for-each statement. 
*/
enum a_for_each_pattern_kind : a_byte {
  sfepk_none,
  sfepk_stl_pattern,    /* The collection type conforms to the STL pattern. */
  sfepk_cli_pattern,    /* The collection type conforms to the C++/CLI 
                           pattern. */
  sfepk_cli_array_pattern,
                        /* The collection type is a CLI array type (a special
                           case of the CLI collection pattern). */
  sfepk_array_pattern   /* The collection type is an array. */
};


/*
Information about a "for each" statement, pointed to from a stmk_for_each
statement.  The type of the collection used in the for-each statement conforms
to one of four patterns: the C++/CLI collection pattern, the STL pattern,
the CLI array pattern, or the (native) array pattern.  Depending on
the kind of pattern, there are several possible rewritings of the
for-each statement

  for each (T t in c) <statement>

In the following, the kind of reference for cref will be either a tracking
reference, an lvalue reference, or an rvalue reference, depending on the
type and value category of c.  If c has a handle type, cref will also
be a handle, and the references to it will use "->" instead of ".".
C, E, and I are determined from their initializing expressions.

  // Case A, C++/CLI collection pattern, GetEnumerator returns a handle
  { C %cref = c;
    E^ e = cref.GetEnumerator();
    while (e->MoveNext()) {
      T t = safe_cast<T>(e->Current);
      <statement>
    }
  }

  // Case B, C++/CLI collection pattern, GetEnumerator does not return a handle
  { C %cref = c;
    E e = cref.GetEnumerator();
    while (e.MoveNext()) {
      T t = safe_cast<T>(e.Current);
      <statement>
    }
  }

  // Case C, STL pattern
  { C &cref = c;
    I cend = cref.end();
    I i = cref.begin();
    for (; i != cend; ++i) {
      T t = static_cast<T>(*i);
      <statement>
    }
  }

  // Case D, array pattern
  { C &cref = c;
    I *cend = &cref[0]+c_num_elements;
    I *i = cref;
    for (; i != cend; ++i) {
      T t = static_cast<T>(*i);
      <statement>
    }
  }

  // Case E, CLI array pattern
  { C ^cref = c;
    int upper0 = cref->GetUpperBound(0);
    int upper1 = cref->GetUpperBound(1);
    // etc. for remaining bounds
    int i0 = cref->GetLowerBound(0);
    int i1 = cref->GetLowerBound(1);
    // etc. for remaining bounds
    for (; i0 <= upper0; i0++) {
      for (; i1 <= upper1; i1++) {
        // etc. for remaining bounds
        T t = safe_cast<T>(cref[i0, i1, ...]);
        <statement>
      }
    }
  }

Note that the (dynamically) initialized variables don't have associated
stmk_init statements because there is no block yet for those statements.
*/
typedef struct a_for_each_loop *a_for_each_loop_ptr;
typedef struct a_for_each_loop {
  a_byte_boolean
		uses_prev_decl_iterator;
			/* If TRUE, the loop uses a previously-declared
			   variable (or variable-like entity) as its iterator
			   variable, instead of declaring a new one. */
  union {
    /* When uses_prev_decl_iterator is FALSE: */
    a_variable_ptr
		variable;
			/* Pointer to the iteration variable ("t" above),
			   a variable newly-declared in the iterator scope.
			   Dynamically initialized to the appropriate value:
			   -- For sfepk_cli_pattern and "e" a handle type,
			       safe_cast<T>(e->Current)
			   -- For sfepk_cli_pattern and "e" not a handle type,
			        safe_cast<T>(e.Current)
			   -- For sfepk_cli_array_pattern,
			        safe_cast<T>(cref[i0, i1, ...])
			   -- For sfepk_stl_pattern or sfepk_array_pattern,
			        static_cast<T>(*i)
			   This field is NULL in some error cases.
			*/
    /* When uses_prev_decl_iterator is TRUE: */
    struct {
      a_variable_ptr
		variable;
      a_field_ptr
		field;
			/* Exactly one of "variable" and "field" is non-NULL,
			   indicating the previously-declared variable or
			   member of the current class (static data member
			   or field) to be used as the iterator variable.
			   The variable or field specified can be a
			   property. */
      an_expr_node_ptr
		assign_expr;
			/* An expression that assigns the iterator a value
			   at the top of each iteration of the loop.  The
			   effect is the same as the initializer for the
			   iterator variable shown above. */
    } prev_decl;
  } iterator;
  a_variable_ptr
		collection_expr_ref;
			/* The cref variable shown above.  Its initial value
			   is the collection expression c, essentially
			   unaltered.  Having the collection expression
			   attached to a reference in this way allows it to
			   be reused in other expressions and also makes it
			   available more directly for source analysis
			   applications. */
  a_scope_ptr	for_each_scope;
			/* An sck_block scope added to surround the for-each
			   statement.  It corresponds to the outermost set
			   of braces shown above, and the cref, e, i, and cend
			   variables shown above are declared in that scope.
			   (Also the extra variables in the CLI array case.)
			   The collection expression is evaluated in this
			   scope. */
  a_scope_ptr	iterator_scope;
			/* An sck_block scope added immediately inside the
			   loop, in which the iterator variable is declared.
			   The dependent statement of the loop is enclosed by
			   this scope.  When a variable from the surrounding
			   context is used as the iterator variable (see
			   uses_prev_decl_iterator), the scope is still
			   present but the iterator variable is not declared
			   there. */
  a_variable_ptr
		temporary_variable;
			/* Pointer to the variable representing the temporary
			   variable "e" when kind is sfepk_cli_pattern or "i"
			   when kind is sfepk_stl_pattern or
			   sfepk_array_pattern.  Dynamically initialized to
			   the appropriate value:
			   -- For sfepk_cli_pattern,
			        c.GetEnumerator()
			   -- For sfepk_stl_pattern,
			        c.begin()
			   -- For sfepk_array_pattern,
			        &c[0] (or equivalent for multi-dim array)
			   NULL for sfepk_cli_array_pattern (the variable is
			   not needed; loop_vars takes its place).  NULL if
			   there was an error. */
  a_for_each_pattern_kind
		kind;   /* Kind of pattern to which the collection type
			   conforms. */
  union {
    /* When kind is sfepk_stl_pattern or sfepk_array_pattern: */
    struct {
      a_variable_ptr
		end_variable;
			/* The "cend" variable, used to hold the final value
			   to be tested against. */
      an_expr_node_ptr
		ne_call_expr;
			/* Expression for the "i != cend" test. */
      an_expr_node_ptr
		incr_call_expr;
			/* Expression for the "++i" increment. */
    } stl_array_pattern;
    /* When kind is sfepk_cli_pattern: */
    struct {
      an_expr_node_ptr
		movenext_call_expression;
			/* Expression for the "MoveNext" call to be tested. */
    } cli_pattern;
    /* When kind is sfepk_cli_array_pattern: */
    struct {
      a_variable_ptr
		upper_bound_vars;
			/* Upper bound variables (upper0, upper1, etc. above),
			   linked by the "next" field.  Note that this list
			   continues into the loop variables, so you have to
			   use loop_vars to check for the end of the list, or
			   count variables.  In the for-each scope.  Each
			   variable is initialized to cref.GetUpperBound(n). */
      a_variable_ptr
		loop_vars;
			/* Loop variables (i0, i1, etc. above), linked by the
			   "next" field.  In the for-each scope.  Each variable
			   is initialized to cref.GetLowerBound(n). */
    } cli_array_pattern;
  } variant;
} a_for_each_loop;

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

typedef struct a_switch_case_entry *a_switch_case_entry_ptr;
typedef struct a_switch_case_entry {
  /* Description of one "case" of a switch statement.  This could be a
    "default:" case, a "case <value>:" case, or a GNU "case <start> ... <end>:"
    case. */
  a_statement_ptr
		stmt;	/* The stmk_switch_case statement pointing to this
			   entry. */
  a_constant_ptr
		case_value;
			/* The integer value associated with this case.  (Or
			   the value of the start of the range if this is a
			   GNU case range.)  NULL for a "default:" case. */
#if GNU_EXTENSIONS_ALLOWED
  a_constant_ptr
		range_end;
			/* If this is a GNU case range, the value of the end
			   of the range; otherwise, NULL. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_switch_case_entry_ptr
		next;
			/* Pointer to the next switch case in source order,
			   or NULL if this is the last case. */
  a_switch_case_entry_ptr
		next_on_sorted_list;
			/* Pointer to the next switch case in numeric order,
			   or NULL if this is the last case.  Also NULL if
			   some of the cases are template dependent. */
  a_source_position
		position;
			/* The position of the case value.  For a default
			   case, the position of the "default" keyword is
			   recorded. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		end_position;
			/* The position of the end of the case value.  If this
			   entry represents a GNU case range, then this is the
			   end position of the range_end value.  For the
			   default case, this is the null position. */
  a_source_position
		colon_position;
			/* The position of the colon token. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_bit_field	reachable_by_fall_through:1;
			/* TRUE if this switch case can be reached by falling
			   through to it from the code immediately
			   preceding. */
} a_switch_case_entry;


/* 
Information about a switch statement, pointed to from an stmk_switch statement.
*/
typedef struct a_switch_stmt_descr *a_switch_stmt_descr_ptr;
typedef struct a_switch_stmt_descr {
  a_switch_case_entry_ptr
		cases;	/* A list of all cases (including "default:") in the
			   order they appeared in the source. */
  a_switch_case_entry_ptr
		default_case;
			/* A pointer to the default case.  (NULL if there is
			   no default case.) */
  a_switch_case_entry_ptr
		sorted_cases;
			/* A list of all non-default cases sorted in increasing
			   order of the "case value".  NULL if some of the
			   cases are template dependent. */
} a_switch_stmt_descr;

/* Information about a handler (or catch-clause) defined within a try block. */
typedef struct a_handler {
  a_handler_ptr	next;
			/* Pointer to the next in the linked list of handlers
			   defined for a given try block; NULL for the last
			   in the list. */
  a_source_position
		catch_position;
                        /* Source position of the catch clause, for symbolic
			   debug purposes. */
  a_variable_ptr
		parameter;
			/* Pointer to a variable entry representing the object
			   to be initialized by the throw object when the
			   the handler is invoked.  It may or may not be
			   named.  It is NULL when the exception declaration
			   is an ellipsis. */
  a_statement_ptr
		statement;
			/* Pointer to an stmk_block statement representing the
			   compound statement that makes up the body of the
			   handler. */
  a_dynamic_init_ptr
		dynamic_init;
			/* Pointer to a dynamic initialization entry that
			   describes the initialization of the parameter (i.e.,
			   initialization via a specific copy constructor or
			   by bitwise copy); it also identifies the destructor
			   to be used, if any.  NULL when the exception
			   declaration is an ellipsis. */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  a_variable_ptr
		typeinfo_var;
			/* Points to a typeinfo variable for the underlying
			   type being caught in this clause (except for the
			   ellipsis case when it is NULL). */
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
} a_handler;

/* Description of an exception-handling "try" statement and the associated
   "catch" clauses. */
typedef struct a_try_supplement {
  a_byte_boolean
		is_function_try_block;
			/* TRUE if this is a function-try-block. */
  a_statement_ptr
		statement;
			/* The dependent statement, i.e., the block that
			   follows the "try" keyword. */
  a_handler_ptr	handlers;
			/* A linked list of entries describing the handlers
			   (or catch-clauses) defined in the try block. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_statement_ptr
                finally_statement;
                        /* The "finally" clause of a try block when compiling
                           C++/CLI code.  NULL if no finally block was
                           specified. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  an_object_lifetime_ptr
		lifetime;
			/* An object lifetime enclosing the try block and
			   catch clauses.  There are no user-declared objects
			   that have this lifetime, but there may be runtime
			   objects with the lifetime. */
} a_try_supplement;

#if MICROSOFT_EXTENSIONS_ALLOWED
/* Supplement for a Microsoft try-finally or try-except statement. */
typedef struct a_microsoft_try_supplement *a_microsoft_try_supplement_ptr;
typedef struct a_microsoft_try_supplement {
  a_statement_ptr
		guarded_statement;
			/* The statement protected by the __try. */
  an_expr_node_ptr
		except_expr;
			/* If this is a try-except, this is the expression
			   tested by the __except.  Otherwise (for a
			   try-finally), NULL. */
  a_statement_ptr
		cleanup_statement;
			/* The statement to be executed on cleanup. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		except_or_finally_position;
			/* Position of the __except or __finally token. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} a_microsoft_try_supplement;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Description of a coroutine definition (pointed to by an stmk_coroutine
statement).
*/
typedef struct a_coroutine_descr *a_coroutine_descr_ptr;
typedef struct a_coroutine_descr {
  a_type_ptr
		traits;
			/* The std::experimental::resumable_traits instance
			   associated with this coroutine. */
  a_variable_ptr
		handle;
			/* A placeholder variable representing the handle
			   for the coroutine invocation. */
  a_variable_ptr
		promise;
			/* A placeholder variable representing the promise
			   for the coroutine invocation. */
  a_variable_ptr
		init_await_resume;
			/* A placeholder variable representing the
			   "initial-await-resume-called" variable for the
			   coroutine invocation. */
  a_variable_ptr
		this_param_copy;
			/* A copy of the implicit "this" parameter of the
			   coroutine, if present.  NULL otherwise.  See
			   a_symbol::variant::routine::this_param_variable. */
  a_variable_ptr
		parameter_copies;
			/* A copy of the coroutine's parameters.  See
			   a_symbol::variant::routine::parameters. */
  a_label_ptr
		final_suspend_label;
			/* A label to use for the generated final_suspend
			   label. */
  an_expr_node_ptr
		initial_suspend_call;
			/* An expression containing the call to
			   promise.initial_suspend(). */
  an_expr_node_ptr
		final_suspend_call;
			/* An expression containing the call to
			   promise.final_suspend(). */
  an_expr_node_ptr
		unhandled_exception_call;
			/* An expression containing the call to
			   promise.unhandled_exception(). */
  an_expr_node_ptr
		get_return_object_call;
			/* An expression containing the call to
			   promise.get_return_object(). */
  an_expr_node_ptr
		alloc_failure_gro_call;
			/* An expression containing the call to the static
			   promise type member function
			   get_return_object_on_allocation_failure(). */
  a_routine_ptr	new_routine;
			/* A pointer to the "new" routine that should be used
			   for allocating the coroutine state. */
  a_routine_ptr	delete_routine;
			/* A pointer to the "delete" routine that should be
			   used for deallocating the coroutine state. */
  a_source_position
		position;
			/* The position of the construct (co_yield or co_await)
			   that triggered the creation of this entry. */
  a_bit_field	error_descr:1;
			/* TRUE if an error occurred in the processing of the
 			   coroutine, such that additional processing is likely
			   to produce more errors and should be inhibited. */
  a_bit_field	has_return_void:1;
			/* TRUE if the promise type has a member function
			   return_void. */
  a_bit_field	body_generated:1;
			/* TRUE if the coroutine body has been generated for
			   the coroutine, FALSE otherwise. */
} a_coroutine_descr;

/*
Description of a C++17 "if constexpr" statement.
*/
typedef struct a_constexpr_if *a_constexpr_if_ptr;
typedef struct a_constexpr_if {
  a_statement_ptr
                then_statement,
                else_statement;
			/* The statements to be evaluated if the expression
			   is true or false.  In a prototype instantiation,
			   and in a non-template function, then_statement
			   is always non-NULL and else_statement is non-NULL
			   if an "else" is present.  In an instantiation of a
			   template function, the then statement will be
			   replaced with an empty statement if the condition
			   is false, and if the else is present, it will be
			   replaced by an empty statement if the condition
			   is true. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		else_position;
                        /* The position of the "else" keyword (if any). */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_bit_field	value_known:1;
			/* TRUE if the expression resulted in a known (i.e.,
			   non-dependent) constant value. */
  a_bit_field	value:1;
			/* When value_known is TRUE, this is TRUE if the
			   result value is TRUE. */
} a_constexpr_if;


typedef struct a_statement {
  /* Definition of an executable statement. */
  a_source_position
		position;
			/* Source position from which this statement came.
			   0 if no direct correspondence. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		end_position;
			/* Source position of the end of this statement.
			   0 if no direct correspondence. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_statement_ptr
		next;
			/* Next statement in execution sequence in the same
			   statement sequence, or NULL if this is the last
			   statement in the sequence. */
  a_statement_ptr
		parent;
			/* The structured statement entry that this one is a
			   direct subordinate of.  NULL for the top-level
			   stmk_block or stmk_try_block entry of a function.
			   Also NULL for the compound statement of a GNU
			   statement expression. */
  an_attribute_ptr
		attributes;
			/* Attributes applicable to this statement. */
  a_statement_kind
                kind;
                        /* The kind of statement. */
  a_bit_field	has_associated_pragma:1;
			/* TRUE if an entry of type a_pragma has been created
			   and bound to this statement.  The pragma entry,
			   which will contain a pointer to this statement, is
			   found by calling find_assoc_pragma. */
  a_bit_field	is_initialization_guard:1;
			/* TRUE if this statement is an "if" that guards
			   a first-time test on an initialization.  When
			   generating thread-safe code, the "if" and the
			   first initialization within it should be rendered
			   as an atomic test-and-set. */
  a_bit_field  compiler_generated:1;
			/* TRUE for a statement that has been added by the
			   front end. */
#if DO_IL_LOWERING
  a_bit_field  lowering_generated:1;
			/* TRUE if this statement has been added during the
			   lowering process. */
  a_bit_field  is_lowering_boilerplate:1;
			/* TRUE if this statement has been added during the
			   lowering of a constructor or destructor and is
			   considered "boilerplate", i.e., the statement has
			   no bearing as to whether the constructor or
			   destructor has an actual effect (it's present
			   in all constructors or destructors). */
#endif /* DO_IL_LOWERING */
  a_bit_field  is_fallthrough_statement:1;
                        /* TRUE if this is a null statement (i.e., stmk_empty)
                           that has the [[fallthrough]] attribute applied to
                           it.  Note that this is not currently used by the
                           front end to suppress any diagnostics. */
  a_bit_field  is_likely:1;
                        /* TRUE if this statement has the [[likely]] attribute
                           applied to it.  No action is taken by the front end
                           based on this attribute. */
  a_bit_field  is_unlikely:1;
                        /* TRUE if this statement has the [[unlikely]]
                           attribute applied to it.  No action is taken by the
                           front end based on this attribute. */
  an_expr_node_ptr
                expr;
                        /* The primary expression, if applicable
                           (but NULL otherwise):
                             The expression to evaluate for stmk_expr.
                             The return value (or NULL) for stmk_return.
                             The expression to test for stmk_if.
                             The expression to test for stmk_constexpr_if.
                             The expression to test for stmk_while.
                             The expression to test for stmk_end_test_while.
                             The expression to test (or NULL) for stmk_for.
                           Note that the "expression to test" in each of the
                           above four cases is always standardized to an
                           integer/boolean expression.
                             The switch expression for stmk_switch.
                             The selector expression for stmk_assigned_goto,
			     if GNU extensions are allowed. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		source_sequence_entry;
			/* Pointer to source sequence entry that represents
			   the place this statement appears within the current
			   function scope relative to other statements as well
			   as declarations, comments, etc. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  union {
    /* When kind == stmk_expr, stmk_coroutine_return or stmk_empty, no variant
       fields. */
#if GNU_EXTENSIONS_ALLOWED
    /* Likewise for stmk_assigned_goto in C/C++ IL. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
    /* Likewise when kind == stmk_upc_notify, stmk_upc_wait, stmk_upc_barrier,
       or stmk_upc_fence. */
#endif /* UPC_EXTENSIONS_ALLOWED */
    /* When kind == stmk_if, stmk_if_consteval, or stmk_if_not_consteval: */
    struct {
      a_statement_ptr
                then_statement,
                else_statement;
                        /* The statements to go to if the expression is true or
                           false.  then_statement is always non-NULL.  If there
                           is no else clause, else_statement is NULL.  These
                           point to a single statement, which will be a block
                           statement if there are several dependent statements.
                           */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      a_source_position
		else_position;
                        /* The position of the "else" keyword (if any). */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    } if_stmt;
    /* When kind == stmk_constexpr_if: */
    a_constexpr_if_ptr
		constexpr_if;
			/* Pointer to an entry describing a C++17
			   "if constexpr" statement. */
    /* When kind == stmk_while: */
    /* When kind == stmk_end_test_while: */
    a_statement_ptr
                loop_statement;
                        /* The statement that is the body of the loop.
                           Points to a single statement, which will be a block
                           statement if there are several dependent
                           statements. */
    /* When kind == stmk_for: */
#if UPC_EXTENSIONS_ALLOWED
    /* When kind == stmk_upc_forall: */
#endif /* UPC_EXTENSIONS_ALLOWED */
    struct {
	a_statement_ptr
		statement;
			/* Pointer to the statement that is the body of the
			   loop (commonly but not necessarily an stmk_block)
			   that is to be executed on each iteration of the
			   loop; NULL if there is none. */
        a_for_loop_ptr
		extra_info;
			/* Information about the loop control constructs
			   (excluding the test expression, which is pointed
			   to from the expr field).  A separate entry is used
			   to keep the size of a_statement down. */
    } for_loop;
    /* When kind == stmk_range_based_for: */
    struct {
      a_statement_ptr
                statement;
                        /* Pointer to the statement that is the body of the
                           loop (commonly but not necessarily an stmk_block)
                           that is to be executed on each iteration of the
                           loop; NULL if there is none. */
      a_range_based_for_loop_ptr
                extra_info;
                        /* Information about the loop control constructs
                           used in the range-based-for.  The expr field in
                           the statement is unused. */
    } range_based_for_loop;
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* When kind == stmk_for_each: */
    struct {
      a_statement_ptr
                statement;
                        /* Pointer to the statement that is the body of the
                           loop (commonly but not necessarily an stmk_block)
                           that is to be executed on each iteration of the
                           loop; NULL if there is none. */
      a_for_each_loop_ptr
                extra_info;
                        /* Information about the loop control constructs
                           (excluding the collection expression which is
                           pointed to from the expr field). */
    } for_each_loop;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* When kind == stmk_switch_case: */
    struct {
      a_statement_ptr
		switch_statement;
			/* The stmk_switch statement associated with this
			   case.*/
      a_switch_case_entry_ptr
		extra_info;
			/* Information about this particular case. */
    } switch_case;
    /* When kind == stmk_switch: */
    struct {
      a_statement_ptr
		body_statement;
			/* The statement (called "substatement" in the C and
			   C++ standards) controlled by the switch.  This is
			   almost always a compound statement that itself
			   contains stmk_switch_case statements. */
      a_switch_stmt_descr_ptr
		extra_info;
			/* Information about this switch statement (including
			   fields describing the associated stmk_switch_case
			   statements). */
    } switch_stmt;
    /* When kind == stmk_goto or stmk_label: */
    struct {
      a_label_ptr
		ptr;	/* The label itself. */
      an_object_lifetime_ptr
		lifetime;
			/* For stmk_goto, the innermost object lifetime that
			   contains both the goto and the label.  Any lifetimes
			   inside of this that the goto is part of are left
			   by the goto, and cleanup must be done for them.
			   For stmk_label, the innermost object lifetime
			   which the label is part of.  NULL if there are
			   no object lifetimes involved, e.g., in C. */
    } label;
    /* When kind == stmk_return: */
    a_dynamic_init_ptr
		return_dynamic_init;
			/* For a routine that returns a value by calling
			   a copy constructor (C++ only), this points to a
			   dynamic initialization entry that initializes the
			   return value.  NULL otherwise.  When this is
			   non-NULL, expr is NULL. */
    /* When kind == stmk_coroutine: */
    struct {
      a_coroutine_descr_ptr
		descr;
			/* A pointer to an entry describing various key
			   entities and operations in the coroutine. */
    } coroutine;
    /* When kind == stmk_block: */
    struct {
      a_statement_ptr
                statements;
                        /* The list of statements contained within the
                           block. */
      a_block_ptr
                extra_info;
                        /* Extra information about the block, stored in
                           a separate entry to keep the size of a_statement
                           down. */
    } block;
    /* When kind == stmk_init: */
    a_dynamic_init_ptr
		dynamic_init;
			/* The description of the dynamic initialization to be
			   performed. */
    /* When kind == stmk_asm: */
    an_asm_entry_ptr
                asm_entry;
                        /* Constant giving the string that is the argument
                           of the "asm" statement, i.e., an assembly-language
                           line. */
#if ASM_FUNCTION_ALLOWED
    /* When kind == stmk_asm_func_body: */
    a_const_char
		*asm_func_body;
			/* Null-terminated string representing the body of
			   an asm function (zero or more uninterpreted lines
			   of assembly language).  Note: this is a literal
			   copy from the source program, with all whitespace,
			   no translation of unprintable characters, etc.,
			   except that it does not include comments unless
			   INCLUDE_COMMENTS_IN_ASM_FUNCTION_BODY is TRUE. */
#endif /* ASM_FUNCTION_ALLOWED */
    /* When kind == stmk_try_block: */
    a_try_supplement_ptr
		try_block;
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* When kind == stmk_microsoft_try: */
    a_microsoft_try_supplement_ptr
		microsoft_try;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* When kind == stmk_decl: */
    struct {
      an_il_entity_list_entry_ptr
		entities;
			/* A list of tagged pointers to the entities declared
			   by this statement.  In the case of a structured
			   binding declaration, this includes the unnamed
			   container variable but not the associated bindings
			   (the bindings are on a separate list pointed to by
			   the container variable entry). */
      a_bit_field
		has_static_or_thread_variable:1;
			/* TRUE if one of the entities being declared is a
			   static or thread-local variable. */
    } decl;
    /* When kind == stmk_set_vla_size: */
    a_vla_dimension_ptr
                vla_dimension;
                        /* The VLA dimension whose number of elements is
                           fixed at this point. */
    /* When kind == stmk_vla_decl: */
    struct {
      a_byte_boolean
		is_typedef_decl;
			/* TRUE for a typedef declaration (the type refers
			   to a variably modified type) or FALSE for a
			   variable declaration (either the variable is a
			   VLA, in which case storage will be allocated at
			   the point represented by this statement, or else
			   it has a variably modified type). */
      union {
        /* When is_typedef_decl is TRUE: */
        a_type_ptr
		typedef_type;
			/* Pointer to a typedef type that refers (directly or
			   indirectly) to a variably modified type. */
        /* When is_typedef_decl is FALSE: */
        a_variable_ptr
                variable;
                        /* Pointer to variable having a variably modified
			   type. If the variable has VLA type, memory for it
                           is allocated at this point. */
      } variant;
    } vla;
    /* When kind == stmk_stmt_expr_result: */
    struct {
      a_dynamic_init_ptr
		dynamic_init;
			/* For a GNU statement expression that produces a value
			   by calling a copy constructor, this points to a
			   dynamic initialization entry that initializes the
			   result value (dik_class_result_via_ctor or
			   dik_constructor).  NULL otherwise.  This is non-NULL
			   (C++ only) when expr is NULL, and vice versa. */
    } stmt_expr_result;
  } variant;
} a_statement;


/* Data structure a_constructor_init, used for C++ only, describes the
   explicit and default initialization to be applied when a constructor is
   called.  This information will reflect constructor initializers that the
   user has supplied with constructor definitions, as well as all default
   constructors that are to be invoked.  In the case of C++11-style delegating
   constructors, only one entry representing the delegation is recorded. */
/* A list of these is also used on destructors to indicate destructor
   calls that must be made for base classes and members. */
enum a_constructor_init_kind : a_byte {
  /* The order of the following constants matters: It reflects the order in
     which class subobjects are initialized (virtual base classes are
     initialized before nonvirtual direct base classes, and all base classes
     are initialized before nonstatic data members). */
  cik_virtual_base_class,
			/* Object to be initialized is a virtual base class. */
  cik_direct_base_class,
			/* Object to be initialized is a nonvirtual direct
			   base class. */
  cik_field,		/* Object to be initialized is a field. */
  cik_delegation	/* Initialization is delegated to another
			   constructor. */
};


typedef struct a_constructor_init *a_constructor_init_ptr;
typedef struct a_constructor_init {
  a_constructor_init_ptr
		next;	/* Pointer to the next constructor initialization
			   entry in the same scope. */
  a_constructor_init_kind
		kind;	/* Kind of constructor initialization, based on the
			   object being initialized (virtual base class,
			   nonvirtual direct base class, or nonstatic data
			   member). */
  a_bit_field	compiler_generated:1;
			/* TRUE for a constructor initializer generated by the
			   compiler rather than representing an explicit
			   initialization. */
  a_bit_field	is_pack_expansion:1;
			/* TRUE if this mem-initializer is a variadic template
			   pack expansion, i.e., it's followed by "...". */
  a_bit_field	is_braced:1;
			/* TRUE if this mem-initializer uses the C++11 braced
			   notation rather than the classic parenthesized
			   form. */
  a_bit_field	use_field_initializer:1;
			/* TRUE if the field initializer should be used to
			   initialize this field (TRUE only when kind is
			   cik_field).  initializer is set to NULL by the
			   front end in that case, but may later be set
			   to point to a copy of the field initializer in
			   lowering configurations. */
  union {
    /* When kind is cik_virtual_base_class or cik_direct_base_class: */
    a_base_class_ptr
		base_class;
			/* The base class to be initialized. */
    /* When kind is cik_field: */
    a_field_ptr field;	/* The field (nonstatic data member) to be
			   initialized. */
    /* When kind is cik_delegation: No variant member. */
  } variant;
  a_dynamic_init_ptr
		initializer;
			/* The initial value to be assigned to the object
			   being initialized, represented by a dynamic
			   initialization entry.  NULL for a field with
			   its own initializer. */
  an_expr_node_ptr
		source_expr;
			/* When copying an explicitly specified array (which
			   is currently only possible in GNU C++ mode), this
			   points to the expression that produces that array.
			   Otherwise, NULL. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		ctor_init_range;
			/* When the mem-initializer is explicit in the source,
			   the source positions corresponding to the opening
			   "(" and closing ")".  May be null_source_range. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_type_ptr    orig_type;
                        /* When non-NULL, is the type that was used in the
                           source to refer to the base class or delegating
                           constructor.  NULL for compiler-generated cases
                           as well as field initializers.  Used to recreate
                           the original source form. */
} a_constructor_init;


#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED

/*
Entry used to hold pointers to lists of orphaned IL type and variable
entries.  These are local types and variables for function and block
scopes, which are allocated in the file scope memory region but pointed
to from the function or block scope a_scope entry.  When the function
scope memory region has been processed and removed from memory, the
types and variables in the file scope memory region are "orphaned" --
their parents have been deleted, and the orphans are not otherwise
attached to the file scope memory region.  The list of orphaned lists
allows one to find the orphans when processing the file scope memory
region.
*/
typedef struct a_scope_orphaned_list_header *a_scope_orphaned_list_header_ptr;
typedef struct a_scope_orphaned_list_header {
  a_scope_orphaned_list_header_ptr
		next;
			/* Pointer to the next header on the list. */
  a_routine_ptr	assoc_routine;
			/* The function that the scope is part of. */
  a_scope_number
		scope_number;
			/* The scope number for the scope. */
  a_type_ptr	orphaned_types;
			/* Pointer to the orphaned file scope IL type entry
			   list for a function scope. */
  a_variable_ptr
		orphaned_variables;
			/* Pointer to the orphaned file scope IL variable
			   entry list for a function scope.  These variables
			   will be local static variables of the function. */
  a_namespace_ptr
		orphaned_namespaces;
			/* Pointer to the orphaned namespace alias list for
			   a function scope.  (Namespace entries are always
			   allocated in the file scope memory region.) */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_src_seq_sublist_ptr
		orphaned_src_seq_sublists;
			/* Pointer to the orphaned file scope IL source
			   sequence sublist list for a function scope. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
} a_scope_orphaned_list_header;

#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

#if RECORD_HIDDEN_NAMES_IN_IL
/*
An entry identifying an entity whose name is hidden by another declaration
but where the hiding can be defeated either by using a qualified name and/or
by using an elaborated type specifier.  This is used in C++ only.  The
combination of !qualification_needed and !elaborated_type_specifier_needed
is used for injected class names.
*/
typedef struct a_hidden_name *a_hidden_name_ptr;
typedef struct a_hidden_name {
  a_hidden_name_ptr
		next;
			/* Next in a linked list of hidden-name entries; NULL
			   for the last on the list. */
  a_tagged_pointer
		entity;
			/* The entity that is hidden by another use of the
			   same name. */
  a_bit_field	qualification_needed:1;
			/* TRUE if entity is a member of a namespace or class
			   scope that is hidden by a declaration in a enclosed
			   scope but for which the hiding can be defeated by
			   using a qualified name (i.e., by prepending "::"
			   or "<class>::" or "<namespace>::". */
  a_bit_field	elaborated_type_specifier_needed:1;
			/* TRUE if entity identifies a tagged type but its
			   name is redeclared by a nontype declaration in the
			   current scope, so that the hiding can be defeated
			   by using an elaborated type specifier. */
  a_bit_field	partially_hidden_by_microsoft_injected_class_name:1;
			/* Used in Microsoft mode only, for injected class
			   names.  They require qualification unless used
			   to the left of "::". */
  a_bit_field	is_class_member:1;
			/* TRUE if entity is a member of a class.  Used when
			   cloning inherited hidden name lists to ensure
			   that block- or namespace-scope names are not
			   cloned. */
  a_bit_field	hidden_by_simulated_injected_class_name:1;
			/* Used in Microsoft mode only.  Microsoft compilers
			   (through at least version 7.1) do not inject the
			   name of an instance of a class template.  In order
			   to facilitate generating code for non-Microsoft
			   dialects, we simulate an injected class name in
			   such cases, and this flag is set for entities
			   hidden by such simulated injected names. The
			   C++-generating back end can then choose whether
			   to honor or ignore the hiding, depending on the
			   target for which code is being generated. */
  a_bit_field	hidden_by_class_name:1;
			/* TRUE if the hiding entity is the
			   injected-class-name of the class associated with
			   this scope.  In particular, this will be FALSE
			   for entities hidden by the injected-class-names
			   of base classes of this scope's class. */
  a_bit_field	hidden_by_template_parameter:1;
			/* TRUE if the hiding entity is a template
			   parameter. */
} a_hidden_name;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */

#if MICROSOFT_EXTENSIONS_ALLOWED

/*
Generic constraint kinds.
*/
enum a_generic_constraint_kind : a_byte {
  gck_none,		/* Used to specify an unknown or invalid kind. */
  gck_type,		/* Used for class and interface constraints. */
  gck_naked_type_param,	/* Used for naked type parameter constraints. */
  gck_ref_class,	/* Used for ref class and ref struct constraints. */
  gck_value_class,	/* Used for value class and value struct
			   constraints. */
  gck_gcnew,		/* Used for gcnew constraints. */
  gck_fail		/* A constraint that is never met.  This is used
			   to handle constraints that use unresolved
			   types. */
};


/*
Entry used to represent a constraint item of a constraint clause.
*/
typedef struct a_generic_constraint {
  a_generic_constraint_kind
		kind;
			/* The kind of constraint represented. */
  a_bit_field	implicit_constraint:1;
			/* TRUE if this constraint did not appear explicitly
			   in the source code (e.g., when a constraint for an
			   overriding virtual function is "inherited" from an
			   overridden function). */
  a_generic_constraint_ptr
		next;
			/* The next entry in a list of constraint items, or
			   NULL for the last entry. */
  a_type_ptr	type;
			/* When kind is gck_type or gck_naked_type_param, this
			   points to the type specified. */
  struct a_token_cache
		*type_cache;
			/* When kind is gtk_type and the type is one that must
			   be rescanned after the complete set of constraints
			   has been scanned, this points to a token cache
			   containing the tokens to be rescanned.  This is
			   for front end use only. */
  a_source_position
		position;
			/* The starting position of the constraint item. */
} a_generic_constraint;

/*
Entry used to represent a generic constraint clause.

The generic constraint clause is the portion of the declaration
highlighted below.

  generic <typename T> where T : ref class, gcnew() ref class A {};
                       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The latter parts of the clause (ref class, gcnew()) are the constraint
items.  A list of those is pointed to by this entry.
*/
typedef struct a_generic_constraint_clause {
  a_generic_constraint_clause_ptr
		next;
			/* The next entry in a list of constraint clauses, or
			   NULL for the last entry. */
  a_type_ptr	type;
			/* The type of the generic parameter named in the
			   constraint clause. */
  a_source_position
		type_position;
			/* The source position of the type that was
			   specified. */
  a_generic_constraint_ptr
		constraints;
			/* The list of constraints items specified in this
			   constraint clause. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		where_position;
			/* The source position of the "where" identifier. */
  a_source_position
		colon_position;
			/* The source position of the ":". */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} a_generic_constraint_clause;

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* Kind of template parameter. */
enum a_template_parameter_kind : a_byte {
  tpk_error,
  tpk_type,
  tpk_nontype,
  tpk_template
};


typedef struct a_template_parameter *a_template_parameter_ptr;
typedef struct a_template_parameter {
  /* Description of a template parameter (type, nontype or template).  A list
     of such items can be assembled through the "next" pointers and should
     normally be headed by a_template_decl entry. */
  /* The source_corresp field must be first. */
  a_source_correspondence
		source_corresp;
			/* Information on the source entity that corresponds
			   to this entity. */
  a_template_parameter_ptr
		next;
			/* Next parameter in this template declaration. */
  a_template_parameter_kind
		kind;
			/* The kind of parameter: type, nontype or template. */
  a_bit_field	is_pack:1;
			/* TRUE if this is a template parameter pack. */
  a_bit_field	is_abbreviated:1;
			/* TRUE if this parameter was created for an "auto"
			   function parameter. */
  union {
    /* When kind == tpk_type: */
    struct {
      a_type_ptr
		ptr;
			/* The placeholder type representing the parameter. */
      a_type_ptr
		default_arg_type;
			/* The prototype instantiation of the default argument
			   (or NULL if none) for this type parameter. */
    } type;
    /* When kind == tpk_nontype: */
    struct {
      a_constant_ptr
		constant;
			/* The placeholder constant representing the
			    parameter. */
      a_constant_ptr
		default_arg_constant;
			/* The prototype instantiation of the default argument
			   (or NULL if none) for this nontype parameter. */
    } nontype;
    /* When kind == tpk_template: */
    struct {
      a_template_ptr
		class_template;
			/* The placeholder template representing the
			    parameter. */
      a_template_ptr
		default_arg_template;
			/* The default argument template (or NULL if none) for
			   this template parameter. */
    } templ;
  } variant;
} a_template_parameter;


typedef struct a_template_decl {
  /* The description of the "header" of a template or C++/CLI generic
     declaration.  The template entity (a_template) points to an entry of
     this type, and the nesting structure (for nested templates) is
     maintained through a parent pointer.
         template <class T> void f(T x) { ... }
                            ^^^^^^^^^^^^^^^^^^^ ----- a_routine entry info
         ^^^^^^^^^^^^^^^^^^ ------------------------- a_template_decl info
  */
  a_template_decl_ptr
		parent;
			/* The enclosing template information, or NULL if
			   this is not a nested template. */
  a_template_parameter_ptr
		param_list;
			/* The list of template parameters for this template
			   entity (not including enclosing parameters). */
  union {
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* When is_generic is TRUE: */
    a_generic_constraint_clause_ptr
		where_clauses;
			/* For C++/CLI generics, this points to the list of
			   constraints specified, and can be NULL. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* When is_generic is FALSE (or undefined): */
    a_requires_clause_ptr
		requires_clause;
			/* The C++20-style requires-clause.  NULL if there is
			   none. */
  } constraint;
  a_scope_ptr	scope;
			/* The template declaration scope containing the
			   template parameter declarations.  NULL for an
			   entry that represents an empty template parameter
			   list of a specialization (e.g., "template <>"). */
  a_source_position
		template_pos;
			/* The position of the "template" keyword. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_generic:1;
			/* TRUE if this is for a C++/CLI "generic". */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} a_template_decl;


/*
The kind of template that is recorded in the IL template representation
(C++ only).
*/
enum a_template_kind : a_byte {
  templk_none,		/* Undefined. */
  templk_class,		/* Class or alias template. */
  templk_function,	/* Function template. */
  templk_variable,	/* Variable template. */
  templk_member_function,
			/* Member function of class template. */
  templk_static_data_member,
			/* Static data member of class template. */
  templk_member_class,
			/* A class nested within a class template. */
  templk_member_enum,
			/* Enumeration member template. */
  templk_template_template_param,
			/* The template associated with a template template
			   parameter. */
  templk_concept
			/* A concept template. */
};


/*
An entry representing the occurrence of a template (or a template template
parameter) declaration in the source.  It can contain the text of such a
declaration (the front end maintains comparable information as a token cache).
These entries are pointed to by source sequence entries for templates.  
Note that there can be multiple a_template entries for a single template;
each declaration or definition produces one entry.  (C++ only.)
*/
typedef struct a_template {
  /* The source_corresp field must be first. */
  a_source_correspondence
                source_corresp;
                        /* Information on the source entity that corresponds
                           to this entity. */
  a_template_ptr
		next;
			/* Next in a linked list of template declarations for
			   the current scope; NULL for the last on the list. */
  a_template_kind
		kind;
			/* The kind of template represented. */
  a_bit_field
		is_exported:1;
			/* TRUE if the template was declared as exported,
			   either because the declaration included the
			   export keyword, or because it is a member of
			   a class declared export.  This is set only on
			   the canonical entry. */
  a_bit_field
		ignore_export:1;
			/* TRUE for templates that have is_exported TRUE
			   but that are static or are declared using
			   types that make it impossible for them to be
			   referenced outside of the translation unit (e.g.,
			   types from an unnamed namespace). */
  a_bit_field
		is_pack:1;
			/* TRUE for a template template arguments if it is
			   a template parameter pack. */
  a_bit_field
		is_friend_template:1;
			/* TRUE for a template declared as a friend
			   template. */
#if BACK_END_IS_CP_GEN_BE
  a_targ_alignment
		final_alignment;
			/* The packing alignment at the end of the template
			   definition.  #pragma pack directives inside the
			   template definition appear in the generated code
			   but do not have associated a_pragma IL entries,
			   so the C++-generating back end cannot track
			   their effect directly.  This field, set during
			   prototype instantiation, allows it to re-sync
			   after inserting the definition into the output.
			   The value is offset by 1 so that the default
			   value of 0 is available to indicate that no
			   #pragma pack directive was seen. */
  int32_t	min_template_arguments;
			/* The number of parameters in this declaration
			   before the first default argument.  This will
			   be updated in the canonical template to reflect
			   the most recent declaration put out for the
			   template.  -1 indicates that this declaration
			   had no default arguments. */
#endif /* BACK_END_IS_CP_GEN_BE */
  uint32_t	cache_checksum;
			/* A checksum of the definition cache used to compare
			   definitions from different translation units. */
  a_template_param_coordinate
		coordinates;
			/* For a class template associated with a template
			   template parameter, provides the list position and
			   nesting depth of the parameter. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		export_position;
			/* The position of the export keyword or
			   null_source_position if no export keyword is
			   present. */
  a_source_range
		definition_range;
			/* When the template is a class template, the source
			   positions of the class definition, if present
			   (i.e., from "{" to "}").  When the template is a
			   function template, the source positions of the
			   body, if present (i.e., from "{" to "}".  When
			   the template is a static data member template,
			   the source positions of the top-level initializer
			   construct (i.e., including "=" or "(" and ")").
			   May be null_source_range. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  struct a_template_symbol_supplement
		*template_info;
			/* Pointer to front end information about the template.
			   This is used only for "nonreal" templates and for
			   template template parameters, and is used to
			   determine if two such templates are equivalent. */
  a_template_decl_ptr
		template_decl;
			/* A description of the template declaration header
			   as used in this particular declaration.  (E.g.,
			   template parameter names could differ from one
			   declaration to the next.)  Always NULL for
			   nonstandard friend template declarations of the
			   form "friend class X;" (a Microsoft extension),
			   since there is no template declaration header in
			   that case. */
  /* Information about the prototype instantiation of this template: */
  union {
    /* When kind == templk_template_template_param, no variant fields. */
    /* When kind == templk_function or templk_member_function: */
    a_routine_ptr
		routine;
			/* A pointer to the prototype instantiation of the
			   function or member function template. */
    /* When kind == templk_class, templk_member_class or templk_member_enum: */
    a_type_ptr	type;
			/* A pointer to the prototype instantiation of the
			   class/alias template, member class, or member enum
			   template. */
    /* When kind == templk_static_data_member or templk_variable: */
    a_variable_ptr
		variable;
			/* A pointer to the prototype instantiation of the
			   static data member definition of a class template
			   or a variable template. */
    /* When kind == templk_concept: */
    an_expr_node_ptr
		constraint;
			/* A pointer to the boolean expression describing the
			   constraint imposed by the concept. */
  } prototype_instantiation;
  a_template_ptr
		canonical_template;
			/* A pointer to the a_template entry associated with
			   the representative declaration of this template.
			   (Currently, this is the first declaration.) */
  a_template_ptr
		definition_template;
			/* If this is the canonical a_template entry, this
			   field points the a_template entry associated with
			   the definition of this template (NULL if no
			   definition appears in this translation unit).
			   NULL for non-canonical entries. */
  a_template_ptr
		prototype_template;
			/* If this is a member template of a class template
			   instance, this points to the template for
			   the original member template declaration in the
			   prototype instantiation. */
#if RECORD_TEMPLATE_STRINGS
  a_const_char	*text;
			/* A null-terminated string representing the text of
			   the template declaration, starting with the keyword
			   "template".  This pointer is NULL for an entry
			   representing a nonstandard friend template of the
			   form "friend class X;" (a Microsoft extension).
			   For generated function templates (such as the call
			   operator of a generic lambda) only the declarator
			   and body of the template is represented. */
#endif /* RECORD_TEMPLATE_STRINGS */
} a_template;

#if RECORD_MACROS_IN_IL

/*
An entry containing the text of a macro, used to represent a #define.
Also used for #undef.
*/
typedef struct a_macro *a_macro_ptr;
typedef struct a_macro {
  /* The source_corresp field must be first. */
  a_source_correspondence
                source_corresp;
                        /* Information on the source entity that corresponds
                           to this entity. */
  a_macro_ptr	next;
			/* Next in a linked list of macro declarations; NULL
			   for the last on the list. */
  a_byte_boolean
		is_undef;
			/* TRUE for #undef, FALSE for #define. */
  a_byte_boolean
		is_command_line_definition;
			/* TRUE if this entry is for a macro defined on the
			   command line. */
  a_byte_boolean
		is_predefined;
			/* TRUE if this entry is for a predefined macro
			   (e.g., __DATE__). */
  a_byte_boolean
		object_like;
			/* TRUE if this is a simple macro that does not take
			   arguments, FALSE for a function-style macro. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		replacement_text_range;
			/* The beginning and ending source positions of the
			   replacement text.  Will be null_source_range for
			   predefined and command-line macros. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_const_char	*text;
			/* A null-terminated string representing the text of
			   the macro declaration, starting with the keyword
			   "#define" or "#undef".  Predefined macros whose
			   replacement text is constructed for each invocation,
			   such as __FILE__ and __LINE__, are identified by an
			   empty (zero-length) text field. */
} a_macro;

#endif /* RECORD_MACROS_IN_IL */

#if RECORD_MACRO_INVOCATIONS

/*
An entry representing a single invocation of a single macro, for use in the
macro invocation tree.
*/
typedef struct a_macro_invocation_record *a_macro_invocation_record_ptr;
typedef struct a_macro_invocation_record {
  a_macro_invocation_record_index
		parent_macro_index;
			/* If greater than NO_PARENT_MACRO_INVOCATION,
			   gives the index in the macro invocation tree of
			   the macro invocation record for the macro
			   expansion in which this macro invocation
			   occurred.  If equal to
			   NO_PARENT_MACRO_INVOCATION, this macro
			   invocation occurred directly in program text.
			   All other (i.e., negative) values indicate that
			   this macro invocation record does not denote an
			   actual macro invocation but is simply a
			   placeholder specifying the number of levels of
			   nesting that are to be popped in the transition
			   to the next record.  (A single-level pop is
			   implicit.) */
  a_macro_ptr	assoc_macro;
			/* The macro whose expansion this invocation record
			   represents.  Will be NULL for negative values of
			   parent_macro_index. */
  a_simple_source_position
		start;	/* The original location of the macro name for this
			   invocation (i.e., if this invocation occurs in
			   the expansion of another, this position will be in
			   either a macro definition line or a macro
			   argument). */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_simple_source_position
		end;	/* The original location of the last character of the
			   macro invocation -- i.e., the last character of the
			   name for an object-like macro or the position of
			   the closing parenthesis for a function-like
			   macro. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if RECORD_MACRO_ARGS
  a_const_char	*arguments;
			/* For an invocation of a function-like macro, a
			   null-terminated string containing the arguments
			   (with canonicalized white space separating the
			   tokens); NULL for an object-like macro. */
#endif /* RECORD_MACRO_ARGS */
} a_macro_invocation_record;

/*
A block of macro invocation records, part of the macro invocation tree.
Because representing variable-length data in the IL is awkward, the macro
invocation records are grouped into blocks and the blocks arranged into a
binary tree.  This represents a compromise between ease of representation and
efficiency.  (During front-end processing, the blocks are kept in a doubly-
linked list and are reorganized into a binary tree at the end of processing,
when the total number of records is known, before assigning the root of the
tree to il_header.root_macro_invocation_record_block.)
*/
#define MACRO_INVOCATION_RECORDS_PER_BLOCK 128
			/* Number of macro invocation records in
			   a_macro_invocation_record_block. */
typedef struct a_macro_invocation_record_block 
                                          *a_macro_invocation_record_block_ptr;
typedef struct a_macro_invocation_record_block {
  a_macro_invocation_record_index
		first_record_in_block;
			/* The index represented by the first macro invocation
			   record in this block.  That is, the array of macro
			   invocation records represents the range of indices
			   from first_record_in_block up through
			   first_record_in_block +
			   MACRO_INVOCATION_RECORDS_PER_BLOCK-1. */
  a_macro_invocation_record_block_ptr
		left_subtree;
			/* Pointer to a binary tree containing all the macro
			   invocation records whose indices are less than that
			   of the first record in this block (NULL if
			   first_record_in_block is 0). */
  a_macro_invocation_record_block_ptr
		right_subtree;
			/* Pointer to a binary tree containing all the macro
			   invocation records whose indices are greater than
			   that of the last record in this block (NULL if this
			   block contains the last invocation record in this
			   subtree). */
  a_macro_invocation_record_block_ptr
		prev;
			/* Pointer to the previous block in the doubly-linked
			   list, or NULL for the first block in the list. */
  a_macro_invocation_record_block_ptr
		next;
			/* Pointer to the next block in the doubly-linked
			   list, or NULL for the last block in the list. */
  a_macro_invocation_record
		records[MACRO_INVOCATION_RECORDS_PER_BLOCK];
			/* The macro invocation records for this block. */
} a_macro_invocation_record_block;

/*
A macro to set mirp to point to the macro invocation record for a specified
index, given a pointer to the root block in the binary tree (i.e.,
il_header.root_macro_invocation_record_block).
*/
#define set_macro_inv_record_ptr_to_index(root, index, mirp)                  \
  { a_macro_invocation_record_block_ptr this_block = (root);                  \
    a_macro_invocation_record_index     idx = (index);                        \
    while (this_block != NULL &&                                              \
           !(idx >= this_block->first_record_in_block &&                      \
             idx < this_block->first_record_in_block +                        \
                                       MACRO_INVOCATION_RECORDS_PER_BLOCK)) { \
      this_block = (idx < this_block->first_record_in_block) ?                \
                        this_block->left_subtree : this_block->right_subtree; \
    }  /* while */                                                            \
    (mirp) = (this_block != NULL) ?                                           \
      this_block->records + (idx - this_block->first_record_in_block) : NULL; \
  }
#endif /* RECORD_MACRO_INVOCATIONS */

enum an_object_lifetime_kind : a_byte {
  olk_global_static,	/* Lifetime of file-scope global variables. */
  olk_block,		/* Lifetime of block-scope automatic entities (plus,
			   when long_lifetime_temps is TRUE, certain
			   expression temporaries). */
  olk_block_after_label,/* Continuation of olk_block, when the block is
			   interrupted by a label.  For example:
			     {
			       <olk_block>
			     L:
			       <olk_block_after_label>
			     }      
			   Also used when beginning a new object lifetime
			   after each switch case statement (only when
			   long_lifetime_temps is TRUE).  For example:
			     switch (x)
			     {
			       <olk_block>
			     case 1:
			       <olk_block_after_label>
			     default:
			       <olk_block_after_label>
			     }      */
  olk_function_static,	/* Lifetime of function-local static variables. */
  olk_expr_temporary,	/* Lifetime of expression temporaries. */
  olk_try_block		/* Lifetime of a try block. */
};

/* Return TRUE if the given object lifetime kind indicates a static
   lifetime. */
#define is_static_object_lifetime_kind(kind) \
  ((kind) == (an_object_lifetime_kind)olk_global_static || \
   (kind) == (an_object_lifetime_kind)olk_function_static)

typedef struct an_object_lifetime {
  /* Represents the lifetime of an object (temporary or variable), which
     might be the same as a scope, or some subregion of a scope. */
  /* Not used for objects allocated via "new," since their lifetimes
     are under user control.  Also not used for objects that don't
     require destruction. */
  /* The bindings between lifetimes and IL entries (indicated by entity.kind)
     are:
	olk_global_static
		<==> iek_scope (sck_file only)
	olk_block
		<==> iek_scope (sck_function, sck_block, or sck_condition)
		<==> iek_block (used for cfront-mode dependent statements,
		     which have no scope entry)
		<==> iek_local_static_variable_init (used to wrap the
		     initialization of a local static variable)
	olk_block_after_label (one-way bindings -- the IL entities have no
			       pointers back to the lifetime.)
		 ==> iek_statement (stmk_label, stmk_switch_case
			            or a structured statement)
	olk_function_static
		<==> iek_scope (sck_function only)
			(Note: an entry for a function scope may bind to two
			lifetimes, one for the topmost block of the function
			and this one, for local static variables.)
	olk_expr_temporary
		<==> iek_expr_node (enk_object_lifetime, used for temporaries
		     that last the lifetime of the full expression)
		<==> iek_dynamic_init (used for temporaries created in
		     constructor-call dynamic initializations that initialize
		     variables).
		<==> iek_block (as a result rewriting done for dynamic init
		     entries during IL lowering)
	olk_try_block
		<==> iek_try_supplement
		<==> iek_block (as a result of rewriting a try block into
		     a block statement during IL lowering)
  */
  an_object_lifetime_kind
		kind;
			/* The kind of lifetime this object lifetime entry
			   represents. */
  a_bit_field	has_block_after_label_child_lifetime:1;
			/* TRUE if this entry is of kind olk_block or
			   olk_block_after_label and has a child lifetime of
			   kind olk_block_after_label. */
  a_bit_field	has_implicit_child:1;
			/* TRUE if this entry is the global static object
			   lifetime and it has children in function scopes
			   that aren't directly attached to it because of
			   memory region issues. */
  a_bit_field	block_lifetime_with_label_or_goto:1;
			/* TRUE if this entry is an olk_block entry for a block
			   that contains labels or unresolved gotos, including
			   such statements generated by "break;" or "continue;"
			   statements.  For front end use only. */
  a_tagged_pointer
		entity;	/* Entity with which this object lifetime is
			   associated.  See list of possible kinds above. */
  a_dynamic_init_ptr
		destructions;
			/* A linked list of dynamic init entries (using
			   the next_in_destruction_list pointer) identifying
			   the destructions that are to be done when this
			   object lifetime terminates; the order of the list
			   is the order in which destructors should be called
			   -- the first on the list is the last created. */
  an_object_lifetime_ptr
		parent_lifetime;
			/* When kind is olk_block_after_label, the predecessor
			   in a chain of lifetimes (terminating with olk_block)
			   that describes the entire block scope.  NULL when
			   kind is olk_global_static or olk_function_static.
			   Otherwise, the object lifetime that is the nearest
			   enclosing lifetime around this one.  For a GNU
			   statement expression's olk_block lifetime, the
			   parent lifetime can be an olk_expr_temporary
			   lifetime. */
  a_dynamic_init_ptr
		parent_destruction_sublist;
			/* Pointer to an entry in the parent lifetime's
			   destructions list; it corresponds to where this
			   object lifetime appears.  NULL if kind is
			   olk_block_after_label or when this lifetime is not
			   on the child list of another lifetime. */
  an_object_lifetime_ptr
		child_lifetime;
			/* If this object lifetime has object lifetimes under
			   it, this is the first on a list linked by the
			   "next" field.  NULL otherwise, including when kind
			   is olk_global_static or olk_function_static.  Note:
			   an olk_block_after_label lifetime is pointed to as
			   child by its predecessor within the chain
			   representing a given block. */
  an_object_lifetime_ptr
		next;
			/* The next object lifetime on a list of sibling
			   lifetimes, or NULL if there are no more siblings.
			   Linked in reverse order of creation, so the
			   lifetimes created last are first on the list. */
} an_object_lifetime;

#if MICROSOFT_EXTENSIONS_ALLOWED

/*
Value that identifies a kind of Microsoft attribute.
*/
enum an_ms_attribute_kind : a_byte {
  msak_none,		/* Must be first. */
  msak_unrecognized,	/* Used to represent unrecognized attributes. */
  msak_misc,		/* Used for predefined attributes that don't require
			   special processing. */
  msak_uuid,		/* The [uuid(...)] attribute. */
  msak_custom,		/* Used to represent custom attributes. */
#if INCLUDE_EDG_TEST_ATTRIBUTES
  msak_edg_test,
#endif /* INCLUDE_EDG_TEST_ATTRIBUTES */
  msak_coclass,		/* The [coclass] attribute. */
  msak_no_injected_text,/* The [no_injected_text] attribute. */
  msak_last		/* Must be last. */
};


/*
Value that identifies the kind of argument value accepted for a given
Microsoft attribute argument.
*/
enum an_ms_attribute_arg_kind : a_byte {
  msaak_none,		/* No argument kind has been specified yet. */
  msaak_integer,	/* An integer constant. */
  msaak_boolean,	/* A boolean constant. */
  msaak_string,		/* A character string. */
  msaak_uuid,		/* A UUID string. */
  msaak_enumeration,	/* A member of an enumerated set of values. */
  msaak_other		/* Some other kind of entity.  Saved as the string
			   version of a set of tokens. */
};


/*
Entry used to describe an argument of a given Microsoft attribute.
*/
typedef struct an_ms_attribute_arg *an_ms_attribute_arg_ptr;
typedef struct an_ms_attribute_arg {
  an_ms_attribute_arg_ptr
		next;
			/* Pointer to the next argument in the list, or NULL
			   for the last argument. */
  a_const_char	*param_name;
			/* The name of the associated parameter. */
  union {
    /* When kind is msaak_integer. */
    long	integer_value;
			/* The integer value specified. */
    /* When kind is msaak_boolean. */
    a_boolean	bool_value;
			/* The boolean value specified. */
    /* When kind is msaak_other. */
    a_const_char
		*other_string;
			/* A null-terminated string representing the tokens
			   of the argument. */
    /* When kind is msaak_string. */
    a_constant_ptr
		string_constant;
			/* A constant containing the string value.  This can
			   be a normal or wide character constant.  The
			   constant can contain any string that can be
			   expressed as a string literal (including embedded
			   null characters). */
    /* When kind is msaak_uuid. */
    a_const_char
		*uuid_string;
			/* String representation of the uuid value. */
    /* When kind is msaak_enumeration. */
    int		enum_value;
			/* The position in the array of acceptable values
			   of the specified value. */
  } variant;
  an_ms_attribute_arg_kind
		kind;	/* Kind of argument (string, integer, etc.). */
} an_ms_attribute_arg;

/*
Entry used to describe a named argument for a custom Microsoft attribute.
*/
typedef struct a_custom_ms_attribute_arg *a_custom_ms_attribute_arg_ptr;
typedef struct a_custom_ms_attribute_arg {
  a_custom_ms_attribute_arg_ptr
		next;
			/* Pointer to the next argument in the list, or NULL
			   for the last argument. */
  a_field_ptr	field;
			/* The field associated with the named argument. */
  an_expr_node_ptr
		expression;
			/* The initialization expression. */
} a_custom_ms_attribute_arg;

/*
Entry used to describe a use of a given attribute.
*/
typedef struct an_ms_attribute {
  an_ms_attribute_ptr
		next;
			/* Pointer to the next attribute in a given scope.
			   NULL if this the last attribute in the scope. */
  an_ms_attribute_ptr
		next_in_block;
			/* Pointer to the next attribute in an attribute
			   block, or NULL if there are no more attributes in
			   the block.  An attribute block is a group of
			   attributes specified in the same set of brackets
			   (e.g., "[coclass, aggregatable(Always)]"). */
  a_tagged_pointer
		entity;
			/* Information about the entity to which the
			   attribute applies.  If there is no associated
			   entity, entity.ptr will be NULL. */
  a_bit_field
		is_attribute_attribute:1;
			/* TRUE if this a C++/CLI AttributeUsage custom
			   attribute that was written as an "attribute"
			   attribute. */
  union {
    /* When kind != msak_custom: */
    struct {
      struct an_ms_attribute_kind_descr
		*kind_descr;
			/* Pointer to an entry that describes the attribute
			   being used.  Used in the front end only; cannot be
			   used in back ends. */
      a_const_char
		*name;
			/* The name of the attribute. */
      a_const_char
		*string;
			/* A textual representation of the attribute.  This is
			   a null-terminated string.  Note that this represents
			   a single attribute and not an attribute block (see
			   next_in_block), so the string does not contain the
			   opening or closing brackets.  If the attribute
			   includes a target (e.g., "[struct: ... ]"), that
			   target is not included in the string either.  */
      an_ms_attribute_arg_ptr
		arg_list;
			/* The arguments, if any, specified for this
			   attribute. */
    } info;
    /* When kind == msak_custom: */
    struct {
      a_type_ptr
		type;
			/* The custom attribute class type. */
      a_routine_ptr
		constructor;
			/* The constructor for the custom attribute. */
      an_expr_node_ptr
		args;   /* The arguments with which the custom attribute
			   constructor should be called. */
      a_custom_ms_attribute_arg_ptr
		named_args;
			/* The named arguments specified for this
			   custom attribute. */
    } custom_info;
  } variant;
  a_source_position
		position;
			/* Source position of the attribute name. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		source_sequence_entry;
			/* Pointer to source sequence entry that represents
			   the place this attribute appears within the current
			   file or function scope relative to other
			   declarations, statements, comments, etc. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  an_ms_attribute_kind
		kind;	/* The kind of attribute used. */
  an_ms_attribute_target
		target;
			/* Indicates the entity to which the attribute
			   applies. */
} an_ms_attribute;

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Entry used to represent a C++11 lambda.  Such an entry is pointed to by an
enk_lambda node (and allocated in the same memory region as that node).
*/
typedef struct a_lambda {
  a_lambda_capture_ptr
		capture_list;
			/* The list of captured local variables (possibly
			   including the enclosing "this" pointer).  This
			   list initially contains any explicitly specified
			   captured entities and may later have implicitly
			   captured entities added.  May be NULL. */
  a_type_ptr	closure_class;
			/* This field points to the class type that is used
			   to represent the result of the lambda.  The class
			   has an operator() member function that contains
			   the lambda body.  The class may also have nonstatic
			   data members used to access the captured entities
			   as well as certain special member functions. */
  a_routine_ptr
		lambda_routine;
			/* Pointer to the routine entry for the operator()
			   member function of closure_class.  This can be
			   used to access information about the lambda,
			   such as the parameter list, the function body,
			   and their associated source positions.  For a
			   generic lambda, this points to the prototype
			   instantiation of the call operator, except that
			   this pointer is cleared if prototype instantiations
			   are not recorded in the IL. */
  a_bit_field
		is_generic:1;
			/* TRUE if this is a C++14-style generic lambda (i.e.,
			   a lambda with at least one "auto" parameter; as a
			   consequence, has_parameter_decl must be TRUE if this
			   flag is TRUE). */
  a_bit_field
		is_mutable:1;
			/* TRUE if the mutable keyword was specified. */
  a_bit_field
		constexpr_specified:1;
			/* TRUE if the constexpr keyword was specified. */
  a_bit_field
		consteval_specified:1;
			/* TRUE if the consteval keyword was specified. */
  a_bit_field
		has_capture_default:1;
			/* TRUE if an explicit capture default was
			   specified. */
  a_bit_field
		default_is_by_reference:1;
			/* When has_capture_default is TRUE, this is TRUE
			   if the default is by reference ("&") or FALSE if
			   the default is by value ("="). */
  a_bit_field
		explicit_return_type:1;
			/* TRUE if the return type of the lambda was specified
			   explicitly. */
  a_bit_field	has_parameter_decl:1;
			/* TRUE if a (possibly empty) parameter list for the
			   lambda appeared explicitly in the input.  (If no
			   parameter list appeared, the effect is equivalent
			   to an empty parameter list.) */
  a_bit_field	has_template_param_list:1;
			/* TRUE if the lambda has an explicit template
			   parameter list.  In cases where this is TRUE, any
			   lambda attributes are present in the token cache of
			   the template. */
  a_source_position
		start_position;
			/* Position of the "[" that begins the lambda. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		capture_end_position;
			/* Position of the "]" that ends the lambda capture. */
  a_source_position
		mutable_position;
			/* If the is_mutable flag is TRUE, this is the
			   position of the mutable keyword; otherwise,
			   null_source_position.  Additional source position
			   information can be accessed via the lambda_routine
			   pointer. */
  a_source_position
		constexpr_position;
			/* If the constexpr_specified or consteval_specified
			   flag is TRUE, this is the position of the
			   corresponding keyword; otherwise,
			   null_source_position. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} a_lambda;


/*
Entry used to represent a local variable, reference, or this parameter
that is part of the capture list (either explicitly or implicitly) of a lambda.
This kind of entry is also used to represent a C++14-style "init-capture";
i.e., a field with an associated initializer for a closure type.
*/
typedef struct a_lambda_capture {
  a_lambda_capture_ptr
		next;	/* Pointer to the next entry on the capture list, or
			   NULL for the last entry. */
  union {
    /* When is_init_capture is FALSE and is_indirect_init_capture is FALSE. */
    a_variable_ptr
		variable;
			/* Pointer to the variable entry for the local variable
			   or "this" pointer to be captured.  This is also
			   set for variables captured indirectly through an
			   enclosing lambda's capture, but in that case the
			   field capture_info.source_closure_field (below) will
			   indicate which field of the enclosing lambda should
			   be used instead. */
    /* When is_init_capture is FALSE and is_indirect_init_capture is TRUE. */
    a_field_ptr	init_capture_field;
			/* Pointer to the field for the init-capture. */
    /* When is_init_capture is TRUE. */
    a_dynamic_init_ptr
		initializer;
			/* The initializer specified on the init-capture. */
  } captured;
  union {
    /* When is_init_capture is FALSE and field_pending is FALSE: */
    a_field_ptr	source_closure_field;
			/* If the variable being captured is reachable only
			   because it's captured by an intervening lambda,
			   this gives the field of the closure class that
			   should be the source of the current capture.
			   (If the field has not been created yet,
			   source_capture is recorded instead.) */
    /* When is_init_capture is FALSE and field_pending is TRUE: */
    a_lambda_capture_ptr
		source_capture;
			/* If the variable being considered for capture is
			   reachable only because it's captured by an
			   intervening lambda, and no closure_field has been
			   created for that enclosing capture, this points to
			   the enclosing capture description instead.  (For
			   use by the front end only.) */
    /* When is_init_capture is TRUE: */
    struct a_decl_parse_state
    		*init_capture_dps;
			/* Opaque pointer to a structure tracking the
			   declaration of the closure field corresponding to an
			   init-capture.  Only valid within the front end. */
  } capture_info;
  a_field_ptr	closure_field;
			/* Pointer to the nonstatic data member of the closure
			   class that is used to access the captured variable
			   within the lambda.  This is NULL until the variable
			   is actually used within the lambda. */
  a_bit_field
		is_init_capture:1;
			/* TRUE if this entry represents a C++14-style
			   init-capture (which isn't really a capture at all).
			   If this is TRUE, is_indirect_init_capture and
			   is_implicit must be FALSE, and source_closure_field
			   must be NULL. */
  a_bit_field
		is_indirect_init_capture:1;
			/* TRUE if this entry represents an indirect capture of
			   a C++14-style init-capture. */
  a_bit_field
		is_param_ref_capture:1;
			/* TRUE if this represents the capture of a "this"
			   pointer in a context that doesn't have an associated
			   "this" variable (specifically, a field initializer).
			   For example:
			     template<typename T> struct Func {
			       template<typename F> Func(F);
			     };
			     struct S {
			       int i;
			       Func<int()> f = [=]{ return i; };
			     };
			   Here, the initializer for f has no "this" variable;
			   instead, references to "this" are represented by an
			   enk_param_ref node. */
  a_bit_field
		capture_by_reference:1;
			/* TRUE if this entity is being captured by
			   reference, FALSE if by value.  This flag may be
			   set based on the capture default or if the
			   default is explicitly overridden for this
			   capture.  The meaning is slightly different when
			   captured.variable is "this": TRUE indicates that
			   the pointer value of "this" is captured, FALSE
			   means that the object to which "this" points
			   (i.e., "*this") is captured. */
  a_bit_field
		is_implicit:1;
			/* TRUE if this entity was implicitly added to the
			   capture list, FALSE if it was explicitly named
			   in the capture list. */
  a_bit_field
		is_pack_expansion:1;
			/* TRUE if this capture is either a variadic template
			   pack expansion (i.e., it is followed by "..." and
			   is_init_capture is FALSE), or a pack expansion in
			   an init-capture (i.e., it is preceded by "..."
			   and is_init_capture is TRUE). */
  a_bit_field	
		is_pack_element:1;
			/* TRUE if this is an instantiation of an init-capture
			   of a pack. */
  a_bit_field
		direct_init:1;
			/* TRUE if init-capture is TRUE and the initializer
			   was a direct braced or parenthesized initializer in
			   the source. */
  a_bit_field
		parenthesized_init:1;
			/* TRUE if init-capture is TRUE and the initializer
			   was a parenthesized initializer in the source. */
  a_bit_field
		field_pending:1;
			/* TRUE if an implicit capture has been created because
			   a variable was referenced, but the associated field
			   hasn't been created yet (because it's possible that
			   the variable is constant-valued and only used as a
			   prvalue; so no true capture was needed).  TRUE only
			   in the front end. */
  a_bit_field
		const_capture:1;
			/* TRUE if the capture is through a non-mutable lambda
			   expression (either the current lambda expression or
			   an enclosing one).  Always FALSE if is_init_capture
			   is TRUE. */
  a_source_position
		position;
			/* The source position of the name of the captured
			   variable or "this" keyword.  For an explicit
			   capture, this is the position in the capture
			   list.  For an implicit capture, it is the position
			   of the first use of the variable in the lambda. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		end_position;
			/* The source position of the end of the name of the
			   captured variable or "this" keyword.
			   null_source_position if is_implicit is TRUE. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} a_lambda_capture;


typedef struct a_local_scope_ref *a_local_scope_ref_ptr;
typedef struct a_local_scope_ref {
  /* Entities in file-scope memory region cannot directly refer to function-
     local entities.  To work around that constraint for function-local
     scopes, an implicit referencing mechanism is used.  The implicit
     references are represented by a_local_scope_ref entries stored in the
     function's memory region: Each entry points to both the referenced
     scope and to the entity in file-scope memory that implicitly refers to
     that scope.  The list of a_local_scope_ref entries (pointed to from the
     a_scope entry of the function in which the referenced entity appears)
     can then be searched whenever the reference must be resolved for a
     given entity in file-scope memory.  (This technique is similar to that
     enabled by a_local_static_variable_init entries.) */
  a_local_scope_ref_ptr
		next;
			/* Pointer to the next reference in the current
			   function. */
  a_scope_ptr
		scope;
			/* Pointer to the referenced scope. */
  a_tagged_pointer
		referrer;
			/* The entity (in file scope memory region) implicitly
			   referring to scope. */
} a_local_scope_ref;


enum a_scope_kind : a_byte {
  /* Kinds of scopes. */
  sck_file,		/* File scope. */
  sck_func_prototype,   /* Function prototype scope, used also during function
			   declarators that are part of a function definition
			   (since we don't know at that point whether or not a
			   body will follow).  A function prototype scope is
			   also pushed for C++20 requires-expressions. */
  sck_block,		/* Block scope, for blocks other than the topmost
			   in a function. */
  sck_namespace,	/* In C++, a scope representing a namespace.  (An
			   IL scope of kind sck_namespace may be pointed to
			   by scope stack entries either of the same kind --
			   for an "original-namespace-definition" -- or of
			   kind sck_namespace_extension.)  */
  sck_namespace_extension,
			/* In C++, a scope representing either an "extension-
			   namespace-definition" or an implicit extension
			   of the namespace when processing the definitions
			   of namespace members in enclosing scopes (because
		           of the name-injection rules for friend and
			   block-extern declarations).  Only used in the
		           front end.  (When a scope stack entry
			   has this kind, the IL scope entry it points to will
			   be of kind sck_namespace.) */

  sck_namespace_reactivation,
			/* In C++, reactivation of a namespace scope, making
			   the namespace members visible without qualification.
			   This is used, for example, when processing the
			   the declarations of friend functions from a
			   namespace. Only used in the front end.   (When a
			   scope stack entry has this kind, the IL scope entry
			   it points to will be of kind sck_namespace.) */
  sck_class_struct_union,
			/* In C, pseudo-scope for fields of a struct or
			   union (and only used in the front end); in C++,
			   real scope for members of a class/struct/union. */
  sck_class_reactivation,
			/* In C++, reactivation of a class scope, making
			   the class members visible without qualification.
			   This is used, for example, when processing a
			   member function definition.  Only used in the
			   front end. */
  sck_template_declaration,
                        /* Template parameter declaration scope, used while
                           scanning the parameter list and declaration of a
                           class or function template (C++ only).  Used only
                           in the front end except when prototype
			   instantiations are included in the IL. */
  sck_template_instantiation,
                        /* Used during the instantiation of class and function
                           templates to make the template arguments visible
                           (C++ only).  Used only in the front end. */
  sck_instantiation_context,
			/* Used during the instantiation of templates to
			   mark the position on the scope stack at which
			   the instantiation context begins.  When a template
			   instantiation scope is pushed, additional context
			   scopes are required to establish the appropriate
			   class and/or namespaces that must be visible
			   during the instantiation.  Used only in the front
			   end. */
  sck_module_decl_import,
			/* Used during module importing immediately before
			   the declaration to be imported. */
  sck_module_isolated,	/* Used during module importing when reconstructing IL
			   that should be isolated from broader context (e.g.,
			   while directly constructing an IL type or template
			   argument without any scope information for
			   context). */
  sck_pragma,
			/* Used while processing certain #pragma directives
			   to affect the visibility of other scopes.  Used
			   only in the front end. */
  sck_function_access,
			/* Used to perform access checking on function
			   declarations.  Used only in the front end. */
  sck_condition,
			/* Used to represent the scope of a C++ condition
			   that is an initialized declaration for an if,
			   switch, for, "for each", or while statement. */
  sck_enum,
			/* The scope associated with a C++11 scoped enum
			   type. */
  sck_function,		/* Function scope. */
  sck_none		/* No scope kind or scope kind not known. */
};


typedef struct a_scope {
  /* Definition of a name scope.  There is one of these for the file
     level, one for each function, etc. */
  /* Scope entries are a key part of the scheme for keeping the
     intermediate language divided up into separate memory regions.
     There is a region for the file scope and a region (containing both
     declarative and executable information) for each top-level function that
     has a body.  Having such regions is useful in that it allows removal of
     all the intermediate language associated with a function as a unit
     when it is no longer needed, and allows writing out and reading
     back in of individual routines.  When the executable information
     for a routine is in memory, all the declarative information for
     all the enclosing scopes will be in memory too.
     Pointers that reference "down" into different memory regions
     (i.e., the pointer from a routine entry to the associated scope)
     are represented as memory region numbers that can be converted into real
     pointers.  Pointers "up" (e.g., the pointer from the scope to the
     associated routine) are just normal pointers, since the referenced
     information will always be in memory. */
  a_scope_ptr
                next;
                        /* Pointer to next scope on the same level, which
                           must be in the same memory region. */
  a_scope_ptr
		prev;
			/* Pointer to the previous scope on the same
			   level. */
  a_scope_ptr
		parent;
			/* Pointer to the parent scope.  NULL when kind ==
			   sck_file.  Also NULL if pointing to the parent
			   would cause a memory region problem, i.e., when
			   a scope for a function-local entity is in the
			   file-scope memory region and its parent is in a
			   function-scope memory region.  In that case, an
			   entry of type a_local_scope_ref is allocated to
			   point to the parent.  Function prototype scopes
			   are an exception: parent is NULL for those (with
			   no a_local_scope_ref alternative), except when
			   nested in another function prototype scope. */
  a_scope_number
		number;	/* Scope number (unique identifier) for this scope. */
  a_scope_kind	kind;
			/* Kind of scope (file, function, block, function
			   prototype, etc.). */
  a_bit_field
		function_body_processing_finished:1;
			/* Front-end only, for sck_function scopes: set to
			   TRUE once the function body processing is
			   finished.  That includes IL lowering if
			   appropriate.  FALSE otherwise. */
  a_bit_field
		do_not_free_memory_region:1;
			/* For sck_function scopes, TRUE if some construct
			   refers to the memory region of this routine, so
			   the memory region cannot be freed. */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  a_bit_field
		scope_orphaned_list_header_generated:1;
			/* TRUE if a scope orphaned list header entry has
			   been generated for this scope.  This is done right
			   after IL lowering, if any. */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  a_bit_field
		is_constexpr_routine:1;
			/* TRUE for a constexpr function or constructor
			   which is valid for constexpr expansion. */
  a_bit_field
		is_stmt_expr_block:1;
			/* TRUE for the top-level block scope of a statement
			   expression. */
  a_bit_field
		is_placeholder_scope:1;
			/* TRUE if this scope was created as a placeholder to
			   be filled in at a future point.  FALSE if this is
			   either not a placeholder scope, or a placeholder
			   scope that has since been filled in. */
  a_bit_field
		needed_walk_done:1;
			/* TRUE if the "needed flag IL walk" has visited this
			   entry.  Only used for block scopes. */
  union {
    /* When kind == sck_file, no variant fields. */
    /* When kind == sck_template_declaration, no variant fields. */
    /* When kind == sck_block (also see assoc_block below): */
    a_handler_ptr
		assoc_handler;
			/* When the scope is associated with an exception
			   handler, a pointer to the handler entry; otherwise
			   NULL. */
    /* When kind == sck_func_prototype, sck_class_struct_union,
       sck_class_reactivation, or sck_enum: */
    a_type_ptr	assoc_type;
			/* The function type whose prototype scope this is,
			   or the class/struct/union type. */
    /* When kind == sck_condition (C++ only): */
    a_statement_ptr
		assoc_statement;
			/* Pointer to the associated if, switch, while, for,
			   or "for each" statement in which the condition
			   declaration appears. */
    /* When kind == sck_namespace (C++ only): */
    a_namespace_ptr
		assoc_namespace;
			/* Pointer to the namespace entry associated with this
			   scope. */
    /* When kind == sck_function: */
    struct {
      a_routine_ptr
		ptr;
                        /* Pointer to the routine associated with this
			   scope. */
      a_variable_ptr
                parameters;
                        /* List of parameters of the associated routine,
                           in declaration order.  NULL if no parameters. */
      a_constructor_init_ptr
		constructor_inits;
			/* List of constructor initializer entries; non-NULL
			   for scopes associated with C++ constructors and
			   destructors only.  The list identifies all
			   subobjects and nonstatic data members of the
			   object being initialized by the constructor,
			   or being destroyed by the destructor, arranged
			   in the order in which the initialization or
			   destruction should be performed (ARM 12.6.2). */
      an_object_lifetime_ptr
		lifetime_of_local_static_vars;
			/* If non-NULL, points to an object lifetime for
			   the local static variables declared within the
			   routine, whether in this scope or a block scope
			   contained within it. */
      a_variable_ptr
                this_param_variable;
			/* If the scope is for a C++ nonstatic member
			   function, this field points to the implicit "this"
			   parameter.  It is NULL in all other cases. */
      a_variable_ptr
		return_value_variable;
			/* If non-NULL, named return value optimization (NRVO)
			   is possible in this routine.  That is, the routine
			   returns a class value via a copy constructor, and
			   all return statements return a single nonstatic
			   local variable, namely the variable pointed to by
			   this field.  Note that the variable is also on the
			   local variables list of this scope. */
    } routine;
  } variant;
  a_statement_ptr
                assoc_block;
			/* Non-NULL if this scope has an associated block
			   of statements.  NULL if none (including implicitly
			   generated sck_block scopes containing for-init
			   declarations).  Used only when kind == sck_function
			   or sck_block.  The statement pointed to is
			   usually an stmk_block statement; however, in C++
			   mode when kind == sck_function, it can also be an
			   stmk_try_block statement, to indicate a
			   function-try-block. */
#if ASM_FUNCTION_ALLOWED
			/* Also used to point to the stmk_asm_func_body
			   statement that represents the uninterpreted body
			   of an asm function.  Used in this way only when
			   kind == sck_function and the associated routine
			   has a storage class of sc_asm. */
#endif /* ASM_FUNCTION_ALLOWED */
  an_object_lifetime_ptr
		lifetime;
			/* Object lifetime that is equivalent to the full
			   scope lifetime.  NULL if the scope contains no
			   objects that require destruction (and therefore
			   always NULL in C mode). */
  a_constant_ptr
                constants;
                        /* List of named constants of this scope, NULL if
                           none. */
  a_type_ptr    types;  /* List of local types of this scope, NULL if
                           none. */
  a_variable_ptr
                variables;
                        /* List of local variables of this scope, NULL
                           if none.  In a function or block scope, this is the
                           list of variables with static or thread storage
                           duration; in a scope for a class, this is the list
                           of static data members.  All variables on this list
                           will be allocated in the file scope memory region.
                           */
  a_variable_ptr
		nonstatic_variables;
			/* List of local nonstatic variables in a function or
			   block scope.  Always NULL at file scope.  Variables
			   on this list will be allocated in the function
			   scope's memory region. */
  a_label_ptr   labels; /* List of local labels of this scope, NULL
                           if none.  Only used at the function scope level
                           and for locally declared labels in GNU C mode
			   (NULL otherwise). */
  a_routine_ptr routines;
                        /* List of local routines of this scope, NULL
                           if none.  Includes both routines with definitions
                           and those that are just declarations of interfaces
                           to external routines.  In a scope for a class,
			   points to a list of the member functions for the
			   class (both static and non-static). */
  an_asm_entry_ptr
		asm_entries;
			/* List of asm entries representing asm declarations
			   that appear in the current scope, NULL if none.
			   (Note: asm entries associated with asm statements
			   do not show up on this list, and so the pointer is
			   always NULL for function and block scopes.  Neither
			   are asm functions included in the list.) */
  a_scope_ptr   scopes;	/* List of local scopes under this scope.  Used for
			   block scopes inside function and block scopes,
			   and prototype scopes inside prototype scopes.
			   NULL if none or not applicable (e.g., at the
			   file scope).  Note that block scopes inside
			   block scopes will appear on the scopes list
			   for those block scopes, not at the function scope
			   level.  Condition scopes can also appear. */
  a_namespace_ptr
		namespaces;
			/* List of namespaces and namespace-aliases defined
			   within the current scope (C++ only).  Will point
			   only to namespace-alias entries in sck_function
			   and sck_block scopes, to either in sck_file and
			   sck_namespace scopes; NULL otherwise. */
  a_using_decl_ptr
		using_declarations,
		using_directives;
			/* List of using-declarations or using-directives
			   appearing within the current scope (C++ only).  If
			   this is an sck_file, sck_namespace, sck_function,
			   or sck_block scope, both lists may be non-NULL.
			   If it is an sck_class_struct_union scope, the
			   using-directives list will always be NULL.
			   Cleared to NULL by IL lowering. */
  a_dynamic_init_ptr
		dynamic_inits;
			/* List of dynamic initializations to be done in the
			   scope, in the order they should be done (C++ only).
                           Used only at file scope or namespace scope; in a
			   function or block scope, where initializations may
			   occur anywhere, stmk_init statements are used to
			   indicate the points within the code where each
			   initialization should be done. */
  a_local_static_variable_init_ptr
		local_static_variable_inits;
			/* List of local static variable initializations in
			   function or block scope; NULL at file or namespace
			   scope.  Only dynamic and aggregate-constant
			   initializations are represented.  The order of
			   entries on the list is not meaningful. */
  a_vla_dimension_ptr
		vla_dimensions;
			/* List of dimension expressions for VLAs declared
			   within a given function -- sck_function scopes;
			   The order of entries on the list is not
			   significant. */
  a_local_expr_node_ref_ptr
		expr_node_refs;
			/* List of references to expressions within this scope
			   (only non-NULL for certain function scopes).  The
			   order of entries on the list is not significant.
			   The entries represent implicit references from the
			   file scope memory region. */
  a_local_scope_ref_ptr
		scope_refs;
			/* List of references to scopes within this scope
			   (only non-NULL for certain function scopes).  The
			   order of entries on the list is not significant.
			   The entries represent implicit references from the
			   file scope memory region. */
  a_pragma_ptr	pragmas;
			/* A linked list of pragma entries.  They may be
			   bound to specific declarations or statements or
			   they may be unbound, meaning they have general
			   effect over this scope. */
  a_scope_depth depth_in_scope_stack;
			/* Used during front end processing, the depth in
			   the scope stack of the entry corresponding to this
			   IL scope entry; NO_SCOPE_DEPTH once it has been
			   popped off the scope stack.   If the scope is on the
			   stack more than once, this contains the depth of
			   the first entry pushed (this only occurs for
			   namespace and namespace extension scopes).
			   Reactivating a class or namespace scope does
			   not affect this value. */
  struct a_symbol
		*symbols;
			/* Used during front end processing.  Points to the
			   list of symbols declared in the scope.  This is
			   used for name lookup purposes when local scopes
			   are reactivated for generic lambda instantiation. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		source_sequence_list;
			/* For file and function scopes, a doubly-linked list
			   of source sequence entries representing all
			   declarations, statements, macros, pragmas, and
			   comments that appear within the textual extent of
			   the scope. */
  a_src_seq_sublist_ptr
		src_seq_sublist_list;
			/* For function scopes, a linked list of source
			   sequence sublist headers, representing those
			   portions of the function-scope source sequence list
			   comprised of entries belonging to the file-scope
			   memory region. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL
  a_hidden_name_ptr
		hidden_names;
			/* For file, function, and block scopes, a linked
			   list of hidden-name entries, designating entities
			   that can be made available in the current scope
			   only if an elaborated type specifier and/or global
			   qualification (a preceding "::") is used.  Only
			   used in C++. */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  a_template_ptr
		templates;
			/* Linked list of template entries. Only used
			   in C++. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_ms_attribute_ptr
		ms_attributes;
			/* Linked list of Microsoft attribute entries. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  an_ms_if_exists_ptr
		ms_if_exists;
			/* Linked list of Microsoft __if_exists entries. */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
} a_scope;


/*
An enumeration to identify the different types of modules.
*/
enum a_module_kind : a_byte {
  mk_none,		/* An unknown module. */
  mk_header_unit,	/* A module header unit. */
  mk_unit,		/* A module unit. */
  mk_unit_partition	/* A partition of a module unit. */
};

/*
An enumeration to identify the different types of supported module files.
*/
enum a_module_file_kind : a_byte {
  mfk_unknown,		/* An unknown module file kind. */
  mfk_edg_ifc,		/* An EDG IFC module. */
  mfk_ms_ifc		/* A Microsoft IFC module. */
};

/*
Information about a named module, named module partition, or imported header
unit.
*/
typedef struct a_module {
  a_module_kind	kind;	/* The kind of module unit. */
  /* FIXME: Remove this cached information from the IL.  We should be able to
     collect it in one read (currently every import requires several file
     reads). */
  a_const_char	*resolved_file;
			/* The (cached) full path name to the module file. */
  a_module_file_kind
		file_kind;
			/* The (cached) file kind of the module file at the
			   path stored by resolved_file. */
  a_bit_field	contains_unsupported_constructs:1;
			/* TRUE if this module contains one or more unsupported
			   binary module interface constructs (i.e., the module
			   made use of a feature of its binary module format
			   that EDG knows about but does not yet support). */
  union {
    /* When kind == mk_none, no variant fields. */
    /* When kind == mk_header_unit: */
    struct {
      a_bit_field
		is_sys_include:1;
			/* This is TRUE if the header import used system header
			   import syntax (e.g., import <foo.h>), and FALSE if
			   it used user header import syntax (e.g., import
			   "foo.h").  This field is meaningless when kind !=
			   mk_header. */
      a_bit_field
		suppress_macro_export:1;
			/* This is FALSE if macros exposed by the header unit
			   should be exported.  Normally a header unit will
			   export macros, but not when the header unit is being
			   transitively imported via another non-header-unit
			   module. */
      a_const_char
		*name;
			/* The header unit name as spelled. */
      a_const_char
		*resolved_header;
			/* The resolved header path. */
    } header_unit;
    /* When kind == mk_unit: */
    struct {
      a_const_char
		*name;	/* The name of the module. */
    } unit;
    /* When kind == mk_unit_partition: */
    struct {
      a_bit_field
		is_internal:1;
			/* This is TRUE if this partition is an internal module
			   partition; otherwise, FALSE. */
      a_const_char
		*name;	/* The name of the module partition (e.g., "B" in
			   "A:B"). */
      a_module_ptr
		unit;	/* The module unit of which this is a partition. */
    } unit_partition;
  } variant;
} a_module;


/*
Entry to represent a module-import-declaration.  Pointed to by the
imported_modules field of il_header.
*/
typedef struct a_module_import_decl *a_module_import_decl_ptr;
typedef struct a_module_import_decl {
  a_module_import_decl_ptr
		next;	/* Next module-import declaration in this translation
			   unit. */
  a_source_position
		position;
			/* Position of the beginning of the declaration. */
  a_source_position
		module_name_position;
			/* Position of the beginning of the module-name. */
  an_attribute_ptr
		attributes;
			/* A list of attributes (NULL if none) specified on
			   this module-import-declaration. */
  a_module_ptr	module_info;
			/* The module referenced by this declaration. */
  a_bit_field	impl_unit_importing_self:1;
			/* This import declaration is an implementation unit
			   importing its own interface unit. */
} a_module_import_decl;


/*
Entry currently used only for expressions in attribute arguments where the
expression is in a function scope.
*/
struct a_scoped_expression {
  a_source_correspondence
                source_corresp;
                        /* A source correspondence. */
  an_expr_node_ptr
                expr;   /* An expression. */
};

/*
Description of a prospective data member, produced by std::meta::
data_member_spec and consumed by std::meta::define_aggregate.  A reflection of
kind iek_data_member_spec designates one of these.  Fields with a zero/NULL
value are "unset" and take their default meaning (an absent name is
synthesized by define_aggregate; a zero alignment means the natural alignment;
a zero bit width means the member is not a bit field).
*/
struct a_data_member_spec {
  a_type_ptr	type;	/* Type of the prospective data member. */
  char		*name;	/* Its name, or NULL if none was specified. */
  a_targ_alignment
		alignment;
			/* Requested alignment, or 0 for the default. */
  a_targ_size_t	bit_width;
			/* Bit-field width, or 0 if not a bit field. */
  a_boolean	no_unique_address;
			/* TRUE if [[no_unique_address]] was requested. */
  an_attribute_ptr
		annotations;
			/* List of ak_annotation attributes to apply to the
			   member, or NULL if none were requested. */
};

/*
Header for the entire intermediate language tree.  Note that the pointers
here are into the file scope memory region.

If you change the structure of this header, be sure to change the IL walk
routines (specifically, remap_il_header_pointers and walk_file_scope_il).
If you add any pointers, be sure to update the precompiled header
processing routines that fix up the IL header after restoring
a precompiled header file.
*/
enum a_source_language {
  /* Code for source language. */
  sl_Cplusplus,
  sl_C
};

typedef struct an_il_header {
  a_source_file_ptr
		primary_source_file;
			/* The description of the primary source file,
			   and linkage to include file information.  In
			   the front end, when there are secondary translation
			   units, this is a list of the top-level files,
			   one for each translation unit. */
  a_scope_ptr	primary_scope;
			/* The file scope, and from there all the subscopes. */
  an_il_entity_list_entry_ptr
		file_scope_statements;
			/* A list of file-scope statements that are not linked
			   into a normal file-scope statement list, but that
			   are reachable from other file-scope IL entries
			   (e.g., the top-level compound statement of a GNU
			   statement expression appearing in a file-scope
			   initializer). */
  a_routine_ptr	main_routine;
			/* If "main" is defined in this compilation, this
			   points to its routine entry.  Otherwise, it
			   is NULL. */
  a_const_char	*compiler_version;
			/* A string that identifies the compiler version. */
  a_const_char	*time_of_compilation;
			/* A string that identifies the time of compilation. */
  a_byte_boolean
		plain_chars_are_signed;
			/* TRUE if the plain char type is signed. */
  a_scope_ptr	*region_scope_entry;
			/* Pointer to an array of scope pointers.
			   region_scope_entry[i] points to the scope entry
			   for memory region i, or is NULL if the region's
			   intermediate language is not currently in memory.
			   Entry [0] is not used.  Note that this is a
			   strange data structure because the pointers can
			   point down into other memory regions; obviously,
			   this table is handled specially by memory management
			   and in writing and reading the IL.  For function-
			   scope memory regions this may point to a list of
			   scopes, the first of which is the top-level function
			   scope (additional entries represent function
			   definitions lexically nested in the top-level
			   function scope). */
  a_function_def_descr
		*function_def_table;
			/* Pointers to an array of entries describing top-level
			   function definitions.  A top-level function is any
			   function that is neither a lambda defined in a
			   function scope, nor the instantiation of a generic
			   lambda defined in a function scope.  Entry [0] is
			   not used.  When an IL file is being used, the
			   function definition entries are not available until
			   the memory region has been read. */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  a_scope_orphaned_list_header_ptr
		scope_orphaned_list_headers;
			/* Pointer to a list of entries that point to lists
			   of "orphaned" file scope IL entries -- entries
			   whose parents are in a function scope memory
			   region. */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  a_source_language
                source_language;
                        /* Code for the language in which the source program
                           is written. */
  uint32_t
		std_version;
			/* A number of the form YYYYmm indicating the version
			   of the language standard (for C or C++) in effect.
			   For C++, this corresponds to the value of the
			   __cplusplus macro and for C to the __STDC_VERSION__
			   macro (except for C89/C90 where it is 199000 since
			   that standard has no corresponding macro).  For
			   standards in development YYYY represents the year
			   in which the standard is expected to be ratified
			   (e.g., it might be 2022 for an anticipated C++22
			   mode) and mm is 00. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_cli_metadata_file_ptr
		cli_metadata_files;
			/* A list of all metadata files made available to the
			   compilation.  These may have been made available by
			   an explicit #using (as in a #using that
			   appears in the source), or an implicit #using (as
			   can be the case with mscorlib, a metadata file named
			   with the preusing option, or referenced indirectly
			   as part of a multi-file assembly). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_byte_boolean
		pcc_compatibility_mode;
			/* TRUE if the source program was compiled as old-style
			   (pcc-compatible) C. */
  a_byte_boolean
		enum_type_is_integral;
			/* Records whether enum types are considered to be
			   integral; normally, TRUE in C mode and FALSE in
			   C++ mode. */
  a_targ_alignment
		default_max_member_alignment;
			/* If nonzero, the maximum alignment of any nonstatic
			   data member of a class, struct, or union, unless a
			   "#pragma pack" overrides it.  Its value is based
			   on command-line option "--pack_alignment".  (A zero
			   value means that a member's alignment is based
			   solely on its type.) */
#if RECORD_MACROS_IN_IL
  a_macro_ptr	macros;
			/* Pointer to a list of entries containing the text
			   of all macros declared in the translation unit. */
#endif /* RECORD_MACROS_IN_IL */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_byte_boolean
		microsoft_mode;
			/* TRUE if Microsoft extensions are accepted;
			   corresponds to global variable microsoft_mode. */
  a_byte_boolean
		cppcli_enabled;
			/* TRUE if C++/CLI extensions are accepted;
			   corresponds to global variable cppcli_enabled. */
  a_byte_boolean
		cppcx_enabled;
			/* TRUE if C++/CX extensions are accepted;
			   corresponds to global variable cppcx_enabled. */
  unsigned long
		microsoft_version;
			/* When microsoft_mode is TRUE, the version of the
			   Microsoft compiler with which compatibility is
			   desired; corresponds to the global variable
			   microsoft_version. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  a_byte_boolean
		default_upc_strict_access;
			/* TRUE if the default UPC access mode for shared
			   objects is "strict".  This default can be
			   overridden by the upc pragma and by explicit
			   reference qualifiers ("strict" and "relaxed"). */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  a_byte_boolean
		gcc_mode;
			/* TRUE if the source program was compiled in
			   GNU C mode. */
  a_byte_boolean
		gpp_mode;
			/* TRUE if the source program was compiled in
			   GNU C++ mode. */
  a_byte_boolean
		clang_mode;
			/* TRUE if the source program was compiled in the
			   clang variant of GNU mode (i.e., will never be
			   TRUE unless either gcc_mode or gpp_mode is
			   TRUE). */
  unsigned long
		gnu_version;
			/* When gcc_mode or gpp_mode is TRUE, the version of
			   the GNU compiler with which compatibility is
			   desired; corresponds to the global variable
			   gnu_version. */
  unsigned long	clang_version;
			/* When clang_mode is TRUE, the version of clang
			   with which compatibility is desired; corresponds
			   to the global variable clang_version. */
  a_byte_boolean
		short_enums;
			/* TRUE if all enumeration types should be considered
			   to be "packed". */
  a_byte_boolean
		default_nocommon;
			/* TRUE if by default tentatively defined variables
			   should be treated like zero-initialized variables
			   (and hence not be placed in "COMMON" storage).
			   This default behavior may be overridden with the
			   GNU "common" attribute. */
  a_byte_boolean
		gnu_c89_inlining;
			/* TRUE in GNU C modes if the older GNU C semantics
			   apply to the inline keyword. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  a_byte_boolean
		near_and_far_are_enabled;
			/* TRUE if near and far memory attributes are
			   enabled (e.g., when Microsoft 16-bit extensions
			   are to be accepted). */
  a_byte_boolean
		far_data_pointers;
			/* TRUE if near_and_far_enabled is TRUE and the
			   default size and alignment of data pointers
			   are targ_sizeof_far_pointer and
			   targ_alignof_far_pointer. */
  a_byte_boolean
		far_code_pointers;
			/* TRUE if near_and_far_enabled is TRUE and the
			   default size and alignment of data pointers
			   are targ_sizeof_far_pointer and
			   targ_alignof_far_pointer. */
#endif /* NEAR_AND_FAR_ALLOWED */
  a_byte_boolean
		UCN_identifiers_used;
			/* TRUE if an identifier containing a universal
			   character name was used anywhere within the
			   translation unit.  When such names are used,
			   and IL lowering is being done, each name must
			   be inspected when special processing is done
			   for the mangling of names containing UCNs.
			   Also set when multibyte characters appear in
			   identifiers and
			   IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS is FALSE,
			   because an encoding similar to UCNs is used for
			   them in that case. */
  a_byte_boolean
		vla_used;
			/* TRUE if a variable-length array type was used
			   anywhere in the translation unit. */
  a_byte_boolean
		any_templates_seen;
			/* TRUE if a template was seen in the translation
			   unit. */
  a_byte_boolean
		prototype_instantiations_in_il;
			/* TRUE if the IL contains some prototype
			   instantiations.  When this is TRUE, consumers
			   of the IL must be prepared to handle prototype
			   instantiations, nonreal types, etc. */
  a_byte_boolean
		il_has_all_prototype_instantiations;
			/* TRUE if both class and nonclass prototype
			   instantiations were recorded in the IL.  In that
			   case, templates can be regenerated from the IL.
			   However, if only class templates prototype
			   instantiations were recorded, all templates should
			   be regenerated from strings, since in-class member
			   definitions would not have their prototype
			   instantiation recorded. */
  a_byte_boolean
		il_has_C_semantics;
			/* TRUE if the IL has C language semantics.  The
			   IL has C semantics if the source program was
			   compiled in C mode, or if the source program was
			   compiled in C++ mode and then lowered. */
#if ONE_INSTANTIATION_PER_OBJECT
  a_const_char	*instantiation_dir_name;
			/* When each instantiation is placed in its own object
			   file, this specifies the directory in which the
			   files should be created.  NULL if the
			   one-instantiation-per-object option is not being
			   used. */
  unsigned long	number_of_external_nonclass_template_entities;
			/* The number of externally linked template functions
			   and template static data members in this
			   translation unit.  This value is only maintained
			   when one instantiation per object mode is used. */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  a_type_ptr	nontag_types_used_in_exception_or_rtti;
			/* Pointer to a list of types that were used in an
			   exception handling or RTTI construct and aren't
			   otherwise on a types list. */
  a_seq_number_lookup_entry_ptr
		seq_number_lookup_entries;
			/* Pointer to the start of a list of sequence number
			   lookup entries that are used to build the sequence
			   number lookup table. */ 
  unsigned long	num_seq_number_lookup_entries;
			/* The number of sequence number lookup entries in
			   use. */ 
#if MACRO_INVOCATION_TREE_IN_IL
  a_macro_invocation_record_index
		num_macro_invocation_records;
			/* The number of macro invocation records in the
			   macro invocation tree.  This number includes
			   placeholder records that represent multi-level
			   stack pops, so it will typically be larger than
			   the actual number of macro invocations that were
			   performed.  Also, the zeroth macro invocation
			   record does not reflect an actual macro
			   invocation but corresponds to the
			   NO_PARENT_MACRO_INVOCATION index.  Nonetheless,
			   a translation unit with no macro invocations
			   will be indicated by the value 0 for
			   num_macro_invocation_records (and not 1). */
  unsigned long	max_macro_invocation_depth;
			/* The number of levels in the deepest part of the
			   macro invocation tree. */
  a_macro_invocation_record_block_ptr
		root_macro_invocation_record_block;
			/* Pointer to the root block in the binary tree of
			   macro invocation record blocks. */
#endif /* MACRO_INVOCATION_TREE_IN_IL */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  a_routine_list_entry_ptr
		file_scope_dynamic_init_routines;
			/* If not NULL, a pointer to a list of routine
			   entries that specify which routines to call,
			   in the order they appear on the list, to correctly
			   initialize variables in the file scope that need
			   dynamic initialization (if any).  Routines on the
			   list are ordered by "needed" bit number and GNU
			   init_priority in applicable configurations. */
#if !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
  a_routine_list_entry_ptr
		thread_local_dynamic_init_routines;
			/* If not NULL, a pointer to a list of routine
			   entries that specify which routines to call,
			   in the order they appear on the list, to perform
			   required dynamic initialization for thread_local
			   variables in the file scope. */
#endif /* !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
  int32_t	target_configuration_index;
			/* Specifies the target configuration that was used
			   to create the IL.  If the value is NO_TARGET_CONFIG,
			   no --target option was given and no default
			   configuration was specified, otherwise the number
			   is an index into the target_configurations array. */
  a_module_import_decl_ptr
		imported_modules;
			/* A list of module import declarations. */
} an_il_header;

EXTERN_THREAD an_il_header il_header;


#if NEAR_AND_FAR_ALLOWED
#define near_and_far_enabled() (il_header.near_and_far_are_enabled)
#define or_near_and_far_enabled() || near_and_far_enabled()
#else /* !NEAR_AND_FAR_ALLOWED */
#define near_and_far_enabled() /*lint --e(506)*/FALSE
#define or_near_and_far_enabled() /* Nothing */
#endif /* NEAR_AND_FAR_ALLOWED */

#if DEBUG
/* Table of debug names for expression operators. */
EXTERN_CONSTINIT_ARRAY(a_const_char*, db_operator_names, eok_last + 1)
#if VAR_INITIALIZERS
= {"&", "ref-&", "%", "*", "ref-*",
   "cast", "lvalue cast", "ref cast", "lvalue adjust", "class rvalue adjust",
   "box", "%-box", "unbox", "unbox-l",
   "base class cast", "derived class cast",
   "pm base class cast", "pm derived class cast",
   "dynamic cast", "ref dynamic cast", "bool cast",
   "array-decay",
   ". vacuous dtor", "-> vacuous dtor",
#if MICROSOFT_EXTENSIONS_ALLOWED
   "__assume",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   "noexcept",
   "()",
   "-", "+", "~", "!", "vec!",
   "vec{}",
#if C99_IL_EXTENSIONS_SUPPORTED
   "x~", "__real", "__imag",
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
   "post ++", "post --", "pre ++", "pre --",
   "+", "-", "*", "/", "%",
#if C99_IL_EXTENSIONS_SUPPORTED
   "j*", "j/", "fj+", "jf+", "fj-", "jf-",
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
   "p+", "p-", "pd",
   "<<", ">>", "&", "|", "^",
   "==", "!=", ">", "<", ">=", "<=", "<=>",
   "vec==", "vec!=", "vec>", "vec<", "vec>=", "vec<=",
   "<?", ">?",
   "=",
   "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^=",
   "p+=", "p-=",
   "b=",
   "&&", "||", "vec&&", "vec||",
   ",",
   "[]", "vec[]",
   ".", "->", ".*", "->*",
   ".* func ptr",
   "->* func ptr",
   ".static", "->static",
   "virt func ptr",
   "?", "vec?",
   "call",
   ". member call",
   "-> member call",
   ".* pm call",
   "->* pm call",
   "cli[]", 
   "va_start", "va_arg", "va_end", "va_copy", "va_start (single op)",
   "lvalue",
   "co_await", "co_yield",
   "[: :]",
   "error", "last"
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(db_operator_names)
#endif /* DEBUG */

/*
Table of names of various builtin operations.
*/
EXTERN_CONSTINIT_ARRAY(a_const_char*, builtin_operation_names, bok_last + 1)
#if VAR_INITIALIZERS
= {
  "__builtin_offsetof",
  "__has_assign",
  "__has_copy",
  "__has_nothrow_assign",
  "__has_nothrow_constructor",
  "__has_nothrow_copy",
  "__has_trivial_assign",
  "__has_trivial_constructor",
  "__has_trivial_copy",
  "__has_trivial_destructor",
  "__has_user_destructor",
  "__has_virtual_destructor",
  "__is_abstract",
  "__is_base_of",
  "__is_class",
  "__is_convertible_to",
  "__is_empty",
  "__is_enum",
  "__is_pod",
  "__is_polymorphic",
  "__is_union",
  "__builtin_types_compatible_p",
  "__INTADDR__",
  "__is_trivial",
  "__is_standard_layout",
  "__is_trivially_copyable",
  "__is_literal_type",
  "__has_trivial_move_constructor",
  "__has_trivial_move_assign",
  "__has_nothrow_move_assign",
  "__is_constructible",
  "__is_nothrow_constructible",
  "__has_finalizer",
  "__is_delegate",
  "__is_interface_class",
  "__is_ref_array",
  "__is_ref_class",
  "__is_sealed",
  "__is_simple_value_class",
  "__is_value_class",
  "__is_final",
  "__is_trivially_constructible",
  "__is_destructible",
  "__is_nothrow_destructible",
  "__is_trivially_destructible",
  "__is_nothrow_assignable",
  "__is_trivially_assignable",
  "__builtin_shuffle",
  "__builtin_complex",
  "__is_valid_winrt_type",
  "__is_win_class",
  "__is_win_interface",
  "__builtin_shufflevector",
  "__builtin_convertvector",
  "__is_assignable",
  "__is_trivially_copy_assignable",
  "__is_assignable_no_precondition_check",
  "__builtin_addressof",
  "__has_unique_object_representations",
  "__is_aggregate",
  "__reference_binds_to_temporary",
  "__is_same",
  "__is_same_as",
  "__is_function",
  "__builtin_has_attribute",
  "__builtin_bit_cast",
  "__is_layout_compatible",
  "__is_pointer_interconvertible_base_of",
  "__is_pointer_interconvertible_with_class",
  "__builtin_is_pointer_interconvertible_with_class",
  "__is_corresponding_member",
  "__builtin_is_corresponding_member",
  "__edg_is_deducible",
  "__is_array",
  "__array_rank",
  "__array_extent",
  "__is_arithmetic",
  "__is_complete_type",
  "__is_compound",
  "__is_const",
  "__is_floating_point",
  "__is_fundamental",
  "__is_integral",
  "__is_lvalue_reference",
  "__is_member_function_pointer",
  "__is_member_object_pointer",
  "__is_member_pointer",
  "__is_object",
  "__is_pointer",
  "__is_reference",
  "__is_rvalue_reference",
  "__is_scalar",
  "__is_signed",
  "__is_unsigned",
  "__is_void",
  "__is_volatile",
  "__is_bounded_array",
  "__is_unbounded_array",
  "__is_referenceable",
  "__is_nothrow_convertible",
  "__reference_constructs_from_temporary",
  "__reference_converts_from_temporary",
  "__is_convertible",
  "__is_trivially_equality_comparable",
  "__is_scoped_enum",
  "__is_trivially_relocatable",
  "__is_invocable",
  "__is_nothrow_invocable",
  "__is_bitwise_cloneable",
  "__builtin_is_virtual_base_of",
  "__builtin_is_implicit_lifetime",
  "__builtin_lt_synthesizes_from_spaceship",
  "__builtin_gt_synthesizes_from_spaceship",
  "__builtin_le_synthesizes_from_spaceship",
  "__builtin_ge_synthesizes_from_spaceship",
  "__builtin_is_structural",
  "last"
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(builtin_operation_names)

/* Array giving, for each IL entry kind, the size of the entry in bytes.
   For string type entries, 1.  This must match the order of the
   enumeration an_il_entry_kind. */
EXTERN_CONSTINIT_ARRAY(sizeof_t, sizeof_il_entry, iek_last)
#if VAR_INITIALIZERS
= {
  0 /* iek_none */,
  sizeof(a_source_file),
  sizeof(a_constant),
  sizeof(a_param_type),
  sizeof(a_routine_type_supplement),
  sizeof(a_based_type_list_member),
  sizeof(a_type),
  sizeof(a_variable),
  sizeof(a_field),
  sizeof(an_exception_specification),
  sizeof(an_exception_specification_type),
  sizeof(a_routine),
  sizeof(a_label),
  sizeof(an_expr_node),
  sizeof(a_for_loop),
  sizeof(a_range_based_for_loop),
#if MICROSOFT_EXTENSIONS_ALLOWED
  sizeof(a_for_each_loop),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  sizeof(a_switch_case_entry),
  sizeof(a_switch_stmt_descr),
  sizeof(a_handler),
  sizeof(a_try_supplement),
#if MICROSOFT_EXTENSIONS_ALLOWED
  sizeof(a_microsoft_try_supplement),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  sizeof(a_block),
  sizeof(a_statement),
  sizeof(an_object_lifetime),
  sizeof(a_scope),
  1 /* iek_id_name */,
  1 /* iek_string_text */,
  1 /* iek_other_text */,
#if C99_IL_EXTENSIONS_SUPPORTED
  sizeof(an_internal_complex_value),
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  sizeof(a_namespace),
  sizeof(a_using_decl),
  sizeof(a_dynamic_init),
  sizeof(a_local_static_variable_init),
  sizeof(a_vla_dimension),
#if DO_IL_LOWERING && IA64_ABI
  sizeof(a_vcall_offset_entry),
#endif /* DO_IL_LOWERING && IA64_ABI */
#if MICROSOFT_EXTENSIONS_ALLOWED
  sizeof(a_partial_class_body),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  sizeof(an_overriding_virtual_function),
  sizeof(a_derivation_step),
  sizeof(a_base_class_derivation),
  sizeof(a_base_class),
  sizeof(a_class_list_entry),
  sizeof(a_routine_list_entry),
  sizeof(a_variable_list_entry),
  sizeof(a_constant_list_entry),
  sizeof(a_class_type_supplement),
  sizeof(a_template_param_type_supplement),
  sizeof(a_constructor_init),
  sizeof(an_asm_entry),
#if GNU_EXTENSIONS_ALLOWED
  sizeof(an_asm_operand),
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  sizeof(an_asm_operand_constraint),
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  sizeof(a_named_register_list),
  sizeof(a_label_list),
#endif /* GNU_EXTENSIONS_ALLOWED */
  sizeof(a_template_arg),
  sizeof(a_new_delete_supplement),
#if MICROSOFT_EXTENSIONS_ALLOWED
  sizeof(a_gcnew_supplement),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  sizeof(a_throw_supplement),
  sizeof(a_condition_supplement),
#if !ABI_CHANGES_FOR_RTTI
  sizeof(an_accessible_base_class),
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  sizeof(an_eh_prologue_supplement),
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sizeof(a_source_sequence_entry),
  sizeof(a_src_seq_secondary_decl),
  sizeof(a_src_seq_end_of_construct),
  sizeof(a_src_seq_sublist),
  sizeof(an_instantiation_directive),
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  sizeof(a_scope_orphaned_list_header),
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_HIDDEN_NAMES_IN_IL
  sizeof(a_hidden_name),
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  sizeof(a_pragma),
  sizeof(a_template),
#if RECORD_MACROS_IN_IL
  sizeof(a_macro),
#endif /* RECORD_MACROS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
  sizeof(a_per_instantiation_needed_flags_entry),
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  sizeof(an_element_position),
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sizeof(a_decl_position_supplement),
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  sizeof(a_template_decl),
  sizeof(a_requires_clause),
  sizeof(a_template_parameter),
  sizeof(a_name_reference),
  sizeof(a_name_qualifier),
#if MICROSOFT_EXTENSIONS_ALLOWED
  sizeof(an_ms_attribute),
  sizeof(an_ms_attribute_arg),
  sizeof(a_custom_ms_attribute_arg),
  sizeof(a_property_index_type),
  sizeof(a_property_or_event_descr),
  sizeof(a_generic_constraint_clause),
  sizeof(a_generic_constraint),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  sizeof(a_seq_number_lookup_entry),
#if RECORD_MACRO_INVOCATIONS
  sizeof(a_macro_invocation_record_block),
#endif /* RECORD_MACRO_INVOCATIONS */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  sizeof(an_ms_if_exists),
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  sizeof(a_local_expr_node_ref),
  sizeof(a_static_assertion),
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if GENERATE_LINKAGE_SPEC_BLOCKS
  sizeof(a_linkage_spec_block),
#endif /* GENERATE_LINKAGE_SPEC_BLOCKS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  sizeof(a_local_scope_ref),
  sizeof(an_il_entity_list_entry),
  sizeof(a_lambda),
  sizeof(a_lambda_capture),
  sizeof(an_attribute),
  sizeof(an_attribute_arg),
  sizeof(an_attribute_group),
  sizeof(a_typeref_type_supplement),
  sizeof(an_integer_type_supplement),
#if MICROSOFT_EXTENSIONS_ALLOWED
  sizeof(a_cli_metadata_file),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
  sizeof(a_gnu_routine_supplement),
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
  sizeof(a_coroutine_descr),
  sizeof(a_variable_template_info),
#if MICROSOFT_EXTENSIONS_ALLOWED
  sizeof(an_event_interface),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  sizeof(a_subobject_path),
  sizeof(a_constexpr_if),
  sizeof(a_module),
  sizeof(a_module_import_decl),
  sizeof(a_token_sequence),
  sizeof(a_token_sequence_entry),
  sizeof(a_scoped_expression),
  sizeof(a_data_member_spec)
}
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(sizeof_il_entry)

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef IL_DEF_H */

