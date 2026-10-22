/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

lower_name.h -- Declarations related to lower_name.c (name mangling for
                IL lowering).

*/

/* Avoid including these declarations more than once: */
#ifndef LOWER_NAME_H
#define LOWER_NAME_H 1

/* Only include this code if it is needed: */
/* NEED_NAME_MANGLING is always TRUE if DO_IL_LOWERING is TRUE. */
#if NEED_NAME_MANGLING

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/* Define a macro that tells whether or not name mangling is needed. */
#if DO_IL_LOWERING
#define name_mangling_needed() (il_lowering_needed())
#else /* !DO_IL_LOWERING */
#define name_mangling_needed() (!C_mode())
#endif /* DO_IL_LOWERING */

#if !IA64_ABI

/*
Entry used to record the position of a compressible string in a mangled name.
Used in compressing the mangled name.
*/
typedef struct a_compressible_string_pos *a_compressible_string_pos_ptr;
typedef struct a_compressible_string_pos {
  a_compressible_string_pos_ptr
		next;
			/* Next entry in the same bucket of the hash table. */
  sizeof_t	str_pos;
			/* The index of the compressible string in the
			   original mangled name. */
} a_compressible_string_pos;

EXTERN_THREAD a_compressible_string_pos_ptr
		avail_compressible_string_pos;
			/* List of compressible string position entries freed
			   and available for reuse. */

#if DEBUG
/* GCOVR_EXCL_START */
/*
Count of entries allocated, for debugging purposes.
*/
EXTERN_THREAD unsigned long
		num_compressible_string_pos_allocated;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

#endif /* !IA64_ABI */

#if TEMPLATE_LOOKUP_NEEDED || MICROSOFT_EXTENSIONS_ALLOWED || MODULE_ID_NEEDED
extern char *get_mangled_function_name_full(
                                     a_routine_ptr routine,
                                     a_boolean     force_primary_name,
                                     a_boolean     externalize_if_necessary);
extern char *get_mangled_function_name(a_routine_ptr routine);
#endif /* TEMPLATE_LOOKUP_NEEDED || MICROSOFT_EXTENSIONS_ALLOWED ||
          MODULE_ID_NEEDED */

#if TEMPLATE_LOOKUP_NEEDED || MODULE_ID_NEEDED
extern a_const_char *get_mangled_variable_name(a_variable_ptr variable);
#endif /* TEMPLATE_LOOKUP_NEEDED || MODULE_ID_NEEDED */

extern a_boolean variable_name_mangling_needed(a_variable_ptr variable);

extern void do_scope_other_name_mangling(a_scope_ptr scope);

extern void externalize_mangled_name(a_source_correspondence  *scp,
                                     a_boolean                is_variable);

extern char *mangled_vtbl_name(a_type_ptr       class_type,
                               a_base_class_ptr bcp,
                               a_base_class_ptr ctor_bcp);

extern char *mangled_class_name(a_type_ptr type);

#if !ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES

extern void mangle_subobject_class_name(a_type_ptr class_type,
                                        a_type_ptr subobject_type);

#endif /* !ONLY_MANGLE_TYPES_NEEDED_FOR_EXTERNAL_NAMES */

extern char *mangled_typeinfo_name(a_type_ptr type);

#if !IA64_ABI
extern char *mangled_id_object_name(a_type_ptr type);
#else /* IA64_ABI */
extern char *mangled_typeinfo_string_name(a_type_ptr type);

extern char *mangled_virtual_table_table_name(a_type_ptr type);

extern char *mangled_typeinfo_string(a_type_ptr type);
#endif /* !IA64_ABI */

#if DO_IL_LOWERING
extern void mangle_promoted_entity_name(a_source_correspondence *scp,
                                        an_il_entry_kind        kind,
                                        a_boolean               final,
                                        a_routine_ptr           routine,
                                        a_scope_ptr             scope);
#endif /* DO_IL_LOWERING */

#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
extern void mangle_wrapper_name(a_routine_ptr entry_routine);
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if IA64_ABI
extern void set_ctor_dtor_mangled_name_kind(a_routine_ptr routine);

extern void mangle_alternate_entry_point_name(a_routine_ptr routine,
                                              a_routine_ptr prim_routine);
#endif /* IA64_ABI */

#if USE_X86_FUNCTION_MULTIVERSIONING && IA64_ABI && DO_IL_LOWERING
extern char *mangled_resolver_name(a_routine_ptr representative);
#endif /* USE_X86_FUNCTION_MULTIVERSIONING && IA64_ABI && DO_IL_LOWERING */

extern void mangle_function_name(a_routine_ptr routine,
                                 a_boolean     suppress_parent_encoding);

extern void do_type_name_mangling(void);

extern void do_all_name_mangling(a_boolean mangling_pre_pass);

extern void do_final_name_mangling(void);

extern void lower_name_one_time_init(void);

extern void lower_name_init(void);

extern a_boolean routine_contains_an_individuated_entity(
                                                        a_routine_ptr routine);

extern a_boolean exception_specification_contains_an_individuated_entity(
                                                      a_type_ptr routine_type);

extern char *make_prefixed_object_name(a_const_char            *prefix,
                                       a_source_correspondence *scp,
                                       an_il_entry_kind        kind);
/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* NEED_NAME_MANGLING */

#endif /* ifndef LOWER_NAME_H */

