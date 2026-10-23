/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

macro.h -- Declarations relating to macro.c (having to do with macro
           definition and expansion routines).

*/

/* Avoid including these declarations more than once: */
#ifndef MACRO_H
#define MACRO_H 1

#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Data structure used to build a list of local variables that point into
the curr_source_line data structure.  Such variables need to be updated if
one of the primary dynamically-allocated buffers is reallocated.
*/
typedef struct a_pointer_registration *a_pointer_registration_ptr;
typedef struct a_pointer_registration {
  /* A linked list of these entries identifies the local pointer variables
     to be updated.   These entries are themselves stack variables. */
  a_pointer_registration_ptr
		next;
			/* Next entry on the list, or NULL if this is the
			   last entry. */
  char		**ptr_variable;
			/* Pointer to the pointer variable (which has type
			   char *). */
} a_pointer_registration;

EXTERN_THREAD a_pointer_registration_ptr
		registered_pointers;
			/* List of registered pointers. */
/*
Macro to add a local pointer variable to the list of registered pointers.
ptr_var is the pointer variable, and ptr_registration is a_pointer_registration
dedicated to the variable.  The pointer variable is set to NULL to ensure
that it has a value that can be examined henceforth (therefore, it
shouldn't be initialized in its declaration).
*/
/*lint -emacro(733,register_pointer_variable)*/
/*lint -emacro(789,register_pointer_variable)*/
#define register_pointer_variable(ptr_var, ptr_registration)          \
{ ptr_registration.next = registered_pointers;                        \
  ptr_registration.ptr_variable = (char **)&(ptr_var);                \
  registered_pointers = &ptr_registration;                            \
  (ptr_var) = NULL;                                                   \
}  /* register_pointer_variable */

EXTERN_THREAD unsigned long
		macro_depth;
			/* Current number of levels of nesting of macro
			   invocations.  Zero if no macro calls are being
			   processed currently. */

EXTERN_THREAD a_boolean
		in_macro_arg_list;
			/* TRUE when reading the tokens of a macro argument
			   list, FALSE at all other times. */

EXTERN_THREAD a_symbol_ptr
	       	line_macro_symbol,
		file_macro_symbol,
		defined_macro_symbol;
			/* Pointers to the symbol entries for the special
			   macros "__LINE__", "__FILE__", and "defined". */
EXTERN_THREAD int
		num_macro_invocations_in_process;
			/* Number of macro invocations currently being
			   processed.  This is used to suppress PCH creation
			   if a macro is in the process of being expanded. */

EXTERN_THREAD a_symbol_ptr
	       	base_file_macro_symbol;
			/* Pointer to the symbol entry for the special
			   GNU macro __BASE_FILE__. */

EXTERN_THREAD a_symbol_ptr
	       	file_name_macro_symbol;
			/* Pointer to the symbol entry for the special
			   GNU/clang macro __FILE_NAME__. */

EXTERN_THREAD a_boolean
		scanning_macro_name;
			/* TRUE if the token about to be scanned is the
			   macro name in a #define directive. */

EXTERN_THREAD a_boolean
		scanning_module_macro;
			/* TRUE if the macro about to be scanned comes from a
			   module (e.g., from a header unit). */

EXTERN_THREAD a_boolean
		defined_op_not_permitted;
			/* When TRUE, the "defined" preprocessor operator is
			   not allowed in the current context. */

#if MACRO_INVOCATION_TREE_IN_IL
extern void copy_macro_invocation_tree_to_il(void);
#endif /* MACRO_INVOCATION_TREE_IN_IL */
#if RECORD_MACRO_INVOCATIONS
extern a_macro_invocation_record_ptr macro_invocation_record_at_index(
                                        a_macro_invocation_record_index index);
#endif /* RECORD_MACRO_INVOCATIONS */

#if FULLY_RESOLVED_MACRO_POSITIONS
extern void init_macro_text_map(sizeof_t             num_entries,
                                a_macro_text_map_ptr mtmp,
                                a_boolean            resizable);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

/* Find a macro symbol on a list of symbols. */
extern a_symbol_ptr find_defined_macro(a_symbol_header_ptr sym_hdr);

/* Adjust addresses in the curr_source_line structure after something
   has been realloc'd. */
extern a_boolean adjust_curr_source_line_structure_after_realloc(
                                       a_const_char *old_ptr,
                                       a_const_char *old_after_end_ptr,
                                       a_const_char *new_ptr,
                                       a_boolean    adjust_source_line_modifs);

/* Adjust the running deletion counts in the macro_buffer and the source
   line modification associated with line_loc, if applicable. */
extern void adjust_deletion_counts(a_const_char            *line_loc,
                                   sizeof_t                deletion_len);

/* Expand a macro invocation. */
extern a_token_kind macro_invocation(a_symbol_ptr  macro_symbol,
                                     a_boolean     *rescan);

/* Make a_constant with a given integer value, for a preprocessing 
   expression. */
extern a_token_kind make_pp_int_constant(long value);

/* Process a #define directive. */
extern a_symbol_ptr proc_define(void);

/* Process an #assert directive. */
extern void proc_assert(void);

/* Process an #unassert directive. */
extern void proc_unassert(void);

/* Enter a predefined #assert predicate. */
extern void enter_assert_predicate(a_const_char *value,
                                   a_const_char *name);

/* Scan a reference to an #assert predicate */
extern void scan_assert_predicate_reference(a_boolean *rescan);

extern a_symbol_ptr enter_predef_macro(a_const_char *repl_text,
			               a_const_char *macro_name,
                                       a_boolean    cannot_be_redefined,
                                       a_boolean    ref_suppresses_pch_file);

extern void fixup_predefined_macros(char  curr_date_time[26]);

extern void set_predef_macro_mode(a_predef_macro_mode	mode,
				  a_boolean		value);

extern void init_predefined_macros(char  curr_date_time[26]);

extern void clear_macro_def(a_macro_def_ptr mdp);

extern void gen_pp_output_for_macro_definitions(void);

extern a_boolean is_valid_identifier(a_const_char     *id_start,
                                     sizeof_t         id_len,
                                     a_symbol_ptr     *assoc_symbol,
                                     a_symbol_locator *locator);

extern void choose_raw_or_expanded_arg(void);

#if DEBUG
/* GCOVR_EXCL_START */
/* Show and return the amount of space used by macro entries. */
extern unsigned long show_macro_space_used(void);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

extern void macro_one_time_init(void);

extern void macro_trans_unit_init(void);

extern void macro_init(void);

#if MAKE_FRONT_END_CALLABLE
extern void macro_cleanup(void);
#endif /* MAKE_FRONT_END_CALLABLE */

/* When variadic macros are enabled, the identifier __VA_ARGS__ can only
   appear in the replacement lists of variadic macros, and similarly for
   __VA_OPT__ when va_opt_enabled is TRUE.  The following check appears in
   a few places, including lexical analysis of identifiers. */
#define check_for_reserved_VA_id(len, buf)                              \
  if (variadic_macros_allowed &&                                        \
      len == sizeof("__VA_ARGS__")-1 &&                                 \
      strncmp(buf, "__VA_ARGS__", sizeof("__VA_ARGS__")-1) == 0) {      \
    pos_error(ec_VA_ARGS_not_allowed, &error_position);                 \
  } else if (va_opt_enabled &&                                          \
             len == sizeof("__VA_OPT__")-1 &&                           \
             strncmp(buf, "__VA_OPT__", sizeof("__VA_OPT__")-1) == 0) { \
    pos_error(ec_VA_OPT_not_allowed, &error_position);                  \
  }  /* if */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* MACRO_H */

