/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

pch.h -- Precompiled header declarations

*/

/* Avoid including these declarations more than once. */
#ifndef PCH_H
#define PCH_H 1

#ifndef PREPROC_H
#include "preproc.h"
#endif /* ifndef PREPROC_H */

#ifndef CMD_LINE_H
#include "cmd_line.h"
#endif /* ifndef CMD_LINE_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Enumeration used to specify the kinds of precompiled header events that
can be recorded.  If this list is updated, be sure to change
pch_event_kind_names below.
*/
enum a_pch_event_kind {
  pchek_none,
			/* The event kind is not yet known, or there is
                           no event. */
  pchek_command_line,
			/* Command line option information. */
  pchek_pp_directive,
			/* A preprocessing directive. */
  pchek_last		/* Must be last. */
};

#if DEBUG
/* GCOVR_EXCL_START */
constexpr a_const_char *pch_event_kind_names[(int)pchek_last+1]
/*
Table of names of PCH event kinds.
*/
= {
  "none",
  "command_line",
  "pp_directive",
  "last"
};
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */


/*
Structure used to record precompiled header events.  The PCH events
record the sequence of command line options, #includes, #defines, etc.
that are found at the beginning of a primary source file.  They are
used to determine whether two source files share a common prefix for
which a precompiled header may be used.
*/
typedef struct a_pch_event *a_pch_event_ptr;
typedef struct a_pch_event {
  a_pch_event_ptr
		next;
			/* Next entry in the list of events. */
  a_pch_event_kind
		kind;
			/* The event kind for this entry. */
  union {
    /* When kind == pchek_pp_directive */
    a_pp_directive_kind
		ppd_kind;
			/* The kind of preprocessing directive. */
    struct {
      an_option_kind
		kind;
			/* The command line option kind. */
      a_byte_boolean
		opt_value;
			/* Specifies whether the option is to be turned on
			   or off. */
    } cl_option;
  } variant;
  char		*value;
			/* An optional character string that provides specific
			   information about the event.  For example, if
			   this event represents a command line option, the
			   value would contain the option code and any
			   arguments. */
  a_source_position
		position;
			/* The source position of this event. */
  a_pch_event_ptr
		last_if_event;
			/* Pointer to the nearest enclosing preprocessing
			   if directive.  Present only for nested if
			   directives.  This field is only used
		           to find the last zero-level directive once the
			   end of the prefix has been found. */
  a_byte_boolean
		match_found;
			/* Flag used while comparing the event list for
			   the current file with a candidate PCH file. */
} a_pch_event;


/*
Structure used to record statically allocated compiler variables that need
to be saved when precompiled headers are written out and restored when they
are read back in.
*/
typedef struct a_pch_saved_variable *a_pch_saved_variable_ptr;
typedef struct a_pch_saved_variable {
  a_void_ptr	var_address;
			/* The address of a variable that is to be saved
			   and/or restored. */
  sizeof_t	var_size;
			/* The size of the variable (that is, the number of
			   bytes that comprise its value). */
  a_byte_boolean
		indirect;
			/* TRUE if var_address should be dereferenced
			   in order to get the true address of the
			   data to be stored. */
#if DEBUG
/* GCOVR_EXCL_START */
  a_const_char	*var_name;
			/* The name of the variable being saved.  Used for
			   debug output purposes. */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
} a_pch_saved_variable;

/*
Macro to generate a string containing a variable name.  Only used when
debug code is enabled and when using a compiler that supports ANSI C
preprocessing.  The actual string generated is ', "var-name"'.
*/
#if DEBUG && USING_ISO_C
#define pch_saved_var_name(var) , #var
#else /* !(DEBUG && USING_ISO_C) */
#define pch_saved_var_name(var)	/* nothing */
#endif /* !(DEBUG && USING_ISO_C) */

/*
Macro used to initialize one element of an array of a_pch_saved_variable.
*/
#define pch_saved_var_array_elem(var)                                   \
  { (a_void_ptr)&var, sizeof(var), FALSE pch_saved_var_name(var) }

/*
Similar to pch_saved_var_array_elem, except used to save the address
of an array.
*/
#define pch_array_saved_var_array_elem(var)                                   \
  { (a_void_ptr)var, sizeof(var), FALSE pch_saved_var_name(var) }

/*
Similar to pch_saved_var_array_elem, except used when the variable
contains the address of the data to be stored.
*/
#define pch_indirect_saved_var_array_elem(var, size)                      \
  { (a_void_ptr)&var, size, TRUE pch_saved_var_name(var) }

/*
Macro used to mark the end of a list of saved variables.
*/
#define pch_saved_var_array_terminating_elem()                          \
  { (a_void_ptr)NULL, (sizeof_t)0, FALSE pch_saved_var_name(NULL) }

extern void register_pch_saved_variables(a_pch_saved_variable array[]);


EXTERN_THREAD a_boolean
		building_pch_prefix;
			/* TRUE when doing the initial scan of the
			   primary source file to build the precompiled
			   header prefix information. */
EXTERN_THREAD a_boolean
	        cannot_do_pch_processing;
			/* TRUE if a condition has occurred that makes it
			   impossible to generate or use precompiled header
			   information for this compilation.  For example,
			   running out of special PCH memory. */

EXTERN_THREAD a_boolean
		cannot_create_pch_file;
			/* TRUE is a condition has occurred that makes
			   the current compilation ineligible to create
			   a precompiled header.  For example, using
			   the predefined macros __DATE__ and __TIME__. */

EXTERN_THREAD a_source_position
		header_stop_source_position;
			/* The line number and column position in the
			   primary source file of the first token of the
			   file that is not part of a preprocessing
			   directive.  This is used by the declaration
			   processing routines to determine when they have
			   reached the implied header stop point. */

EXTERN_THREAD a_boolean
		header_stop_position_pending;
			/* TRUE when the actual compilation of the file
			   has reached the header stop point.  Also TRUE
                           if PCH processing is not being done.  This disables
			   subsequence checking for the header stop
			   condition. */

EXTERN_THREAD a_boolean
		pragma_hdrstop_found;
			/* TRUE is a #pragma hdrstop was encountered
			   during the prefix prescan.  This disables
			   the recognition of subsequent events. */

EXTERN_THREAD a_boolean
		using_a_pch_file;
			/* TRUE if this compilation makes use of a
			   precompiled header file. */

EXTERN_THREAD a_source_position
		pos_of_last_event_from_pch;
			/* Position of the last event in the current source
			   file that will actually be supplied by the PCH
			   being used.  Used to skip past the common
			   prefix before beginning the compilation. */

EXTERN_THREAD a_boolean
		next_event_resumes_compilation;
			/* Used when skipping the common prefix before
			   beginning real compilation when using a PCH.
			   This is TRUE when the next event should be
			   processed normally. */

EXTERN_THREAD a_boolean
		generate_pch_on_return_to_primary_source_file;
			/* This flag indicates that a precompiled header
			   should be generated the next time that the
			   input stack is popped back to the primary
			   source file. */


/*
Macro used to set cannot_do_pch_processing.
*/
#define abandon_pch_processing()					\
  (cannot_do_pch_processing = TRUE)

/*
Macro used to set cannot_create_pch_file.
*/
#define suppress_creation_of_pch()					\
  (cannot_create_pch_file = TRUE)

extern
void add_pch_event(a_pch_event_kind	kind,
		   a_pp_directive_kind	ppd_kind,
		   a_const_char		*value,
		   a_source_position	*position,
		   a_line_number	actual_line);

extern
void add_command_line_pch_event(a_pch_event_kind	kind,
                                an_option_kind		opt_kind,
				a_boolean		opt_value,
				a_const_char		*optarg);

extern void precompiled_header_processing(void);

extern void generate_precompiled_header(void);

extern void check_create_pch_file_created(void);

extern void header_stop_no_longer_pending(void);

extern void process_prefix_pragma_hdrstop(void);

extern void pch_fixup_part_2(void);

extern void pch_one_time_init(void);

extern void pch_init(void);

extern void pch_early_init(void);

#if MAKE_FRONT_END_CALLABLE
extern void pch_late_cleanup(void);
#endif /* MAKE_FRONT_END_CALLABLE */

#if DEBUG
/* GCOVR_EXCL_START */
extern unsigned long db_show_pch_space_used(unsigned long grand_total);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

/*
Macro that returns TRUE if the line number indicated by the current input
stack entry and the column number from the supplied source position
match the header stop position.
*/
#define is_header_stop_position(pos)					\
  (header_stop_position_pending &&					\
   (!curr_ise->is_include_file &&					\
    curr_ise->actual_line == (a_line_number)header_stop_source_position.seq &&\
    (pos).column == header_stop_source_position.column))

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef PCH_H */


