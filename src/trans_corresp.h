/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

trans_corresp.h -- Declarations related to matching entities across
                   translation units.

*/

/* Avoid including these declarations more than once: */
#ifndef TRANS_CORRESP_H
#define TRANS_CORRESP_H 1

#define TRACK_TU_CORRESP \
    (EXPORT_ENABLING_POSSIBLE || COMPILE_MULTIPLE_TRANSLATION_UNITS)

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

EXTERN_THREAD a_boolean
		correspondence_checking_underway;
			/* TRUE if the correspondence checking code is
			   currently executing. */
EXTERN_THREAD a_boolean
		correspondence_checking_done;
			/* TRUE if the correspondence checking code has been
			   completed for the current translation unit. */


/* Return TRUE if the indicated entry has a correspondence set (it
   has a correspondence pointer, the pointer is set, and it doesn't
   point to itself). */
#define has_correspondence(ptr)                                               \
  (trans_unit_corresp_of_unknown_entry(ptr) != NULL &&                        \
   (trans_unit_corresp_of_unknown_entry(ptr)->canonical != (char*)(ptr)  ||   \
    trans_unit_corresp_of_unknown_entry(ptr)->primary == (char*)ptr))


extern a_boolean f_same_name(char  *entity1,
                             char  *entity2);

#define same_name(ptr1, ptr2)                                       \
  f_same_name((char*)(ptr1), (char*)(ptr2))

#if TRACK_TU_CORRESP
/*
Routine to record builtin type correspondences.
*/
extern void record_builtin_type(a_type_ptr  type);
#else /* !TRACK_TU_CORRESP */
#define record_builtin_type(tp)  /* Nothing */
#endif /* TRACK_TU_CORRESP */


/*
Routines to retrieve the primary builtin types.
*/
extern a_type_ptr primary_int_type(an_integer_kind  kind);

extern a_type_ptr primary_signed_int_type(an_integer_kind  kind);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_type_ptr primary_microsoft_sized_int_type(an_integer_kind  kind);

extern a_type_ptr primary_microsoft_sized_signed_int_type(
                                                       an_integer_kind  kind);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_type_ptr primary_wchar_t_type(void);

extern a_type_ptr primary_char8_t_type(void);

extern a_type_ptr primary_char16_t_type(void);

extern a_type_ptr primary_char32_t_type(void);

extern a_type_ptr primary_managed_nullptr_type(void);

extern a_type_ptr primary_standard_nullptr_type(void);

extern a_type_ptr primary_float_type(a_float_kind  kind);

#if C99_IL_EXTENSIONS_SUPPORTED
extern a_type_ptr primary_bool_type(void);

extern a_type_ptr primary_imaginary_type(a_float_kind  kind);

extern a_type_ptr primary_complex_type(a_float_kind  kind);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

#if FIXED_POINT_ALLOWED
extern a_type_ptr primary_fixed_point_type(a_fixed_point_type_descr descr);
#endif /* FIXED_POINT_ALLOWED */

/*
Return TRUE if we need to compare the canonical entries in order to determine
if two pointers refer to the same entity.  This test is never needed in
standalone utility programs.
*/
#if !STANDALONE_UTILITY_PROGRAM && TRACK_TU_CORRESP
#define canonical_test_needed(ptr1, ptr2)				\
  (secondary_translation_unit_seen() &&					\
    (ptr1) != NULL && (ptr2) != NULL)
#else /* STANDALONE_UTILITY_PROGRAM || !TRACK_TU_CORRESP */
#define canonical_test_needed(ptr1, ptr2) (FALSE)
#endif /* !STANDALONE_UTILITY_PROGRAM && TRACK_TU_CORRESP */

/*
The following routine is used by the macros corresponding_* to determine
whether two entities in different translation units correspond.  This may
entail actually searching for the correspondence.
*/
extern a_boolean corresponding_entries(char              *entity1,
                                       char              *entity2,
                                       an_il_entry_kind  kind);

/*
The same_*_entities macros determine whether the two given entities are in fact
the same, even though they might have been declared in different translation
units (resulting in distinct IL entries).
*/

#define corresponding_namespaces(ptr1, ptr2)                               \
  ((ptr1) == (ptr2) ||                                                     \
   (canonical_test_needed(ptr1, ptr2) &&                                   \
    corresponding_entries((char*)ptr1, (char*)ptr2, iek_namespace)))

#define corresponding_fields(ptr1, ptr2)                                   \
  ((ptr1) == (ptr2) ||                                                     \
   (canonical_test_needed(ptr1, ptr2) &&                                   \
    corresponding_entries((char*)ptr1, (char*)ptr2, iek_field)))

#define corresponding_routines(ptr1, ptr2)                                 \
  ((ptr1) == (ptr2) ||                                                     \
   (canonical_test_needed(ptr1, ptr2) &&                                   \
    corresponding_entries((char*)ptr1, (char*)ptr2, iek_routine)))

#define corresponding_variables(ptr1, ptr2)                                \
  ((ptr1) == (ptr2) ||                                                     \
   (canonical_test_needed(ptr1, ptr2) &&                                   \
    corresponding_entries((char*)ptr1, (char*)ptr2, iek_variable)))

#define corresponding_types(ptr1, ptr2)                                    \
  ((ptr1) == (ptr2) ||                                                     \
   (canonical_test_needed(ptr1, ptr2) &&                                   \
    corresponding_entries((char*)ptr1, (char*)ptr2, iek_type)))

#define corresponding_templates(ptr1, ptr2)                                \
  ((ptr1) == (ptr2) ||                                                     \
   (canonical_test_needed(ptr1, ptr2) &&                                   \
    corresponding_entries((char*)ptr1, (char*)ptr2, iek_template)))


/*
Macro that returns the trans_unit_corresp for an IL entry that has a source
correspondence.
*/
#if EXPENSIVE_CHECKING
/*lint -emacro(664,trans_unit_corresp_of)*/
#define trans_unit_corresp_of(ptr) 			                \
  (*(check_assertion(in_front_end), &(ptr)->source_corresp.trans_unit_corresp))
#else /* !EXPENSIVE_CHECKING */
#define trans_unit_corresp_of(ptr)					\
  ((ptr)->source_corresp.trans_unit_corresp)
#endif /* EXPENSIVE_CHECKING */

/*
Macro like trans_unit_corresp_of, but that can operate on a char* pointer
or a direct source correspondence pointer.
*/
#if EXPENSIVE_CHECKING
/*lint -emacro(664,trans_unit_corresp_of_unknown_entry)*/
#define trans_unit_corresp_of_unknown_entry(ptr)			  \
  (*(check_assertion(in_front_end),                                       \
     &((a_source_correspondence*)(ptr))->trans_unit_corresp))
#else /* !EXPENSIVE_CHECKING */
#define trans_unit_corresp_of_unknown_entry(ptr)			  \
  (((a_source_correspondence*)(ptr))->trans_unit_corresp)
#endif /* EXPENSIVE_CHECKING */

/*
Macro that returns the canonical IL entry pointer for an IL entry that
has a source correspondence.  If the entry has no correspondence pointer,
the given IL entry is returned.
*/
#define canonical_il_entry_of(ptr)				            \
  (trans_unit_corresp_of_unknown_entry(ptr) != NULL		            \
                    ? trans_unit_corresp_of_unknown_entry(ptr)->canonical   \
                    : (char*)ptr)

/*
Compare two translation unit correspondence pointers.  They match if they
are equal and non-NULL.
*/
#define same_trans_unit_corresps(ptr1, ptr2)				\
  ((ptr1) == (ptr2) && (ptr1) != NULL)

/*
Return TRUE if two IL entries (that have source correspondence entries)
refer to the same IL entity.  If the pointers differ, check the
translation unit correspondence pointers.  Unlike the "corresponding_*"
macros above, same_entities assumes that any correspondences between
the objects pointed to by ptr1 and ptr2 have already been set (if in
doubt whether that assumption is valid, it is always safe to use one
of the "corresponding_*" macros).
*/
/*lint -emacro(666,same_entities)*/
#define same_entities(ptr1, ptr2)                                        \
  ((ptr1) == (ptr2) ||                                                   \
   ((ptr1) != NULL && (ptr2) != NULL && in_front_end &&                  \
    same_trans_unit_corresps(trans_unit_corresp_of(ptr1),                \
                             trans_unit_corresp_of(ptr2))))

/*
This is an interface to the function version of same_entities.  This is
a macro too so that the source correspondence pointer can be used as
the function argument so that any IL entity with a source correspondence
can be used as an argument.  A cast is used instead of &(ptrn)->source_corresp
because the pointers are allowed to be NULL.
*/
#define f_same_entities(ptr1, ptr2)					\
  (ff_same_entities((a_source_correspondence *)(ptr1),			\
                    (a_source_correspondence *)(ptr2)))


extern a_boolean ff_same_entities(a_source_correspondence	*ptr1,
				  a_source_correspondence	*ptr2);
/*
Return TRUE if two base classes refer to the same IL entry.  If the
pointers differ, check the translation unit correspondence pointers.
*/
#define same_base_classes(ptr1, ptr2)					\
  ((ptr1) == (ptr2) ||							\
   same_trans_unit_corresps((ptr1)->trans_unit_corresp,			\
                            (ptr2)->trans_unit_corresp))


extern a_boolean seek_type_corresp(a_type_ptr  type_1,
                                   a_type_ptr  type_2);

extern a_symbol_ptr find_corresponding_symbol_in_trans_unit(
					a_symbol_ptr		sym_to_find,
					a_translation_unit_ptr	tup);

extern a_symbol_ptr find_corresponding_class_instance_in_trans_unit(
				a_symbol_ptr		sym_to_find,
				a_translation_unit_ptr	tup);

extern void set_trans_unit_correspondences(void);

extern void set_correspondence_of_unvisited_entries(a_scope_ptr  scope);

#if TRACK_TU_CORRESP
extern void record_instantiation(a_symbol_ptr                      inst,
                                 a_template_symbol_supplement_ptr  tssp);

extern void record_default_arg_instantiation(a_routine_ptr     rp1,
                                             a_param_type_ptr  ptp1);

extern void establish_class_instantiation_corresp(a_type_ptr  type);

extern void establish_function_instantiation_corresp(a_routine_ptr  routine);

extern void establish_variable_instantiation_corresp(a_variable_ptr  var);

extern void establish_enum_instantiation_corresp(a_type_ptr  enum_type);

extern void establish_block_extern_function_correspondence(
                                                      a_routine_ptr  routine);

extern void establish_block_extern_variable_correspondence(
                                                      a_variable_ptr  var);

extern void establish_friend_type_correspondence(a_type_ptr  type);
#else /* !TRACK_TU_CORRESP */
#define record_instantiation(inst, tssp)  /* Nothing */

#define record_default_arg_instantiation(rp1, ptp1)  /* Nothing */

#define establish_class_instantiation_corresp(type)  /* Nothing */

#define establish_function_instantiation_corresp(routine)  /* Nothing */

#define establish_variable_instantiation_corresp(var)  /* Nothing */

#define establish_enum_instantiation_corresp(enum_type)  /* Nothing */

#define establish_block_extern_function_correspondence(routine)  /* Nothing */

#define establish_block_extern_variable_correspondence(var)  /* Nothing */

#define establish_friend_type_correspondence(type)  /* Nothing */
#endif /* TRACK_TU_CORRESP */

extern void corresp_one_time_init(void);

extern void corresp_trans_unit_init(void);

extern void corresp_init(void);

extern a_routine_ptr canonical_routine_entry_of(a_routine_ptr  routine);

extern a_variable_ptr canonical_variable_entry_of(a_variable_ptr  var);

extern a_type_ptr canonical_type_entry_of(a_type_ptr type);

extern a_template_ptr canonical_template_entry_of(a_template_ptr templ);

#if DEBUG
/* GCOVR_EXCL_START */
extern void* db_corresp(void *ptr);

extern void db_sym_list(a_symbol_list_entry_ptr entries);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef TRANS_CORRESP_H */

