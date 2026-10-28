/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

pch.c -- Precompiled header processing.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include <errno.h>

/* Additional header files. */
#include "pch.h"
#include "decls.h"
#include "statements.h"
#include "macro.h"
#include "templates.h"
#if MANGLE_ALL_NAMES
#if DO_IL_LOWERING
#include "lower_il.h"
#endif /* DO_IL_LOWERING */
#include "lower_name.h"
#endif /* MANGLE_ALL_NAMES */
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ms_metadata.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


#define PCH_ID_STRING_LENGTH 128
			/* Maximum length of the PCH id string. */

STATIC_THREAD char
		pch_id_string[PCH_ID_STRING_LENGTH];
			/* Buffer used to store the PCH id string. */

STATIC_THREAD sizeof_t
		pch_id_string_length;
			/* The actual length of the PCH id string (including
			   the trailing null character). */

STATIC_THREAD a_pch_event_ptr
		pch_event_list_head;
			/* List of precompiled header events for the
			   current primary input file. */

STATIC_THREAD a_pch_event_ptr
		pch_event_list_tail;
			/* Pointer to the end of the list of precompiled
                           header events for the current primary input file. */

STATIC_THREAD a_pch_event_ptr
		pch_cmd_line_event_list_head;
			/* List of precompiled header events associated with
			   the command line. */

STATIC_THREAD a_pch_event_ptr
		pch_cmd_line_event_list_tail;
			/* Pointer to the end of the list of precompiled
                           header events associated with the command line. */

STATIC_THREAD a_const_char
		*pch_file_name;
			/* Name of the precompiled header file being written
			   or read. */

STATIC_THREAD FILE
		*f_pch_input;
			/* File from which the precompiled header information
			   is being read. */

STATIC_THREAD FILE
		*f_pch_output;
			/* File to which the precompiled header information
			   is being written. */


#define MAX_NUMBER_OF_SAVED_VARIABLE_LISTS 64
			/* The number of saved variable lists that can be
			   used.  One saved variable list entry will be used
			   for each compiled source file containing variables
			   to be saved. */

STATIC_THREAD a_pch_saved_variable_ptr
		saved_variable_array_list[MAX_NUMBER_OF_SAVED_VARIABLE_LISTS];
			/* Array of pointers to arrays of saved variable
			   lists.  Each element points to an array of
			   saved variable entries. */

STATIC_THREAD int
		num_of_saved_variable_lists;
			/* Number of entries in the saved variable array list
			   that have been used. */

STATIC_THREAD an_error_code
		mismatch_reason;
			/* An error code that specifies why a given
			   precompiled header file could not be used. */

/*lint -esym(728,*il_header_from_pch)*/
STATIC_THREAD an_il_header
		il_header_from_pch;
			/* Copy of the IL header from the compilation that
			   generated the PCH file. */

STATIC_THREAD a_seq_number
		saved_curr_seq_number;
			/* Saved value of curr_seq_number, used to fix up
			   the source file sequence number information. */

STATIC_THREAD a_mem_alloc_history_ptr
		new_alloc_history;
			/* The memory allocation history information
			   read from the precompiled header file. */

STATIC_THREAD a_mem_alloc_history_number
		new_alloc_history_entries;
			/* Number of entries in new_alloc_history. */

STATIC_THREAD a_text_buffer_ptr
		file_name_text_buffer;
			/* A buffer used to construct PCH file names. */

STATIC_THREAD a_boolean
		precompiled_header_file_created;
			/* TRUE if write_precompiled_header_file completed
			   successfully during this compilation. */

/*
Macro to write a value to the PCH output file.
*/
#define pch_write_value(value)						\
  (void)fwrite((a_stdio_arg)&(value), sizeof(value), 1, f_pch_output)


NORETURN static void bad_pch_file(void)
/*
Called when a read operation on a PCH file fails.  Issue a catastrophic
error.
*/
{
  /* The error position is reset to prevent the error from being issued
     with respect to a particular source line.  This is important because
     the state information used to file source lines, etc. may be in an
     indeterminate state. */
  error_position = null_source_position;
  catastrophe(ec_bad_pch_file);
}  /* bad_pch_file */


NORETURN static void pch_write_error(void)
/*
Called when a write operation on a PCH file fails.  Issue a catastrophic
error.
*/
{
  error_position = null_source_position;
  file_write_error(ec_pch, errno);
}  /* pch_write_error */


/*
Macro to read a value from the PCH input file.  If the value read originates
from a global variable that is different from the variable being read into,
prefer pch_read_size_checked_value; this ensures the sizes do not diverge over
time.
*/
#define pch_read_value(value)						\
  if (fread((a_stdio_arg)&(value), sizeof((value)), 1, f_pch_input) != 1) { \
    bad_pch_file();							\
  }  /* if */


/*
Macro to read a value from the PCH input file into a variable that differs from
the source variable (e.g., if the value of "foo" is written and read into "bar"
value is the variable bar and src_variable is the variable foo).
*/
#define pch_read_size_checked_value(value, src_variable)                      \
  pch_read_value(value);                                                      \
  static_assert((sizeof(value) == sizeof(src_variable)),                      \
                "the sizes of " #value " and " #src_variable " must match");


/*
Macro to perform an fread with an error check.
*/
#define fread_with_check(value, length, file)				\
  if (fread((a_stdio_arg)(value), size_t_arg((length)), 1, (file)) != 1) { \
    bad_pch_file();							\
  }  /* if */

/*
Macro to perform an fread and return an error status.  Return TRUE if the
read succeeded.
*/
#define fread_with_status(value, length, file)				\
  (fread((a_stdio_arg)(value), size_t_arg((length)), 1, (file)) == 1)

/*
Macro to perform an fwrite with an error check.
*/
#define fwrite_with_check(value, length, file)				\
  if (fwrite((a_stdio_arg)(value), size_t_arg((length)), 1, (file)) != 1) { \
    pch_write_error();							\
  }  /* if */

/*
Macro to do an fseek on the output file with an error check.
*/
#define fseek_with_check(file, pos, mode)			\
  if (fseek((file), (long)(pos), (mode)) != 0) {			\
    pch_write_error();							\
  }  /* if */


#if DEBUG
STATIC_THREAD unsigned long
		num_pch_events_allocated;
#endif /* DEBUG */


static void prepare_to_write_precompiled_header_file(void)
/*
We're about to write a precompiled header file.  Make any needed updates to
the data structures that will be written out.
*/
{
#if MANGLE_ALL_NAMES
  if (name_mangling_needed()) {
    /* Do name mangling for all entities.  Typically this isn't done until
       lowering of the file scope, but having mangled names in the PCH file
       greatly reduces the time spent during compilation of files that use
       the PCH file.  Entities whose mangled names depend on a module id (e.g.,
       unnamed namespaces) aren't given mangled names in this pass; they will
       receive mangled names during the usual name mangling phase. */
    do_all_name_mangling(/*mangling_pre_pass=*/TRUE);
  }  /* if */
#endif /* MANGLE_ALL_NAMES */
  if (instantiate_before_pch_creation) {
    /* Generate any instantiations that should be included in the precompiled
       header. */
    template_and_inline_function_processing_for_pch();
  }  /* if */
  /* To avoid any surprises, we ensure that the routines list is up-to-date by
     performing all scheduled moves prior to writing the precompiled header
     file. */
  perform_scheduled_routine_moves();
  /* Ensure that any pending deferred access checks have been performed. */
  end_deferral_of_access_checks();
}  /* prepare_to_write_precompiled_header_file */


#if CHECKING
/* Enumeration of sections of the PCH file.  This is used to make sure that
   the file is positioned at the correct location before a section of the
   file is read. */
enum a_pch_file_section {
  pfs_cmd_line_events,
  pfs_other_events,
  pfs_include_file_info,
  pfs_mem_alloc_info,
  pfs_saved_variables,
  pfs_memory_regions,
  pfs_last		/* Must be last. */
};

#if DEBUG
static a_const_char
		*file_section_names[(int)pfs_last + 1] =
{
  "cmd_line_events",
  "other_events",
  "include_file_info",
  "mem_alloc_info",
  "saved_variables",
  "memory_regions",
  "last"
};
#endif /* DEBUG */


static void write_file_section_id(a_pch_file_section section)
/*
Write the file section ID to the PCH file.
*/
{
  pch_write_value(section);
}  /* write_file_section_id */


static void check_file_section_id(a_pch_file_section section)
/*
Read a file section ID from the PCH file and compare it with the expected
value passed by the caller.
*/
{
  a_pch_file_section	section_in_file;

  /* coverity[+taint_source: arg-0] */ /* coverity[string_null_argument] */
  pch_read_value(section_in_file);
#if DEBUG
  if (section_in_file != section) {
    fprintf(f_debug, "Incorrect file section ID: expected %d, got %d\n",
            (int)section, (int)section_in_file);
    fprintf(f_debug, "  (expected name: %s, got name: %s\n",
            file_section_names[(int)section],
            file_section_names[(int)section_in_file]);
  }  /* if */
#endif /* DEBUG */
  check_assertion_str2(section_in_file == section,
                       "check_file_section_id:",
                       "incorrect file section encountered");
}  /* check_file_section_id */
#else /* !CHECKING */
/*
When not generating checking code, these functions are replaced with NULL
macros.
*/
#define write_file_section_id(value) /* Nothing. */
#define check_file_section_id(value) /* Nothing. */
#endif /* CHECKING */

			
#define PCH_BUFFER_INITIAL_ALLOCATION 2048
#define PCH_BUFFER_INCREMENTAL_ALLOCATION 1024
			/* Initial and incremental allocation sizes for
			   pch_buffer.  The initial allocation
			   should be such that almost all cases can be
			   accepted (so that the realloc is hardly ever
			   needed). */

/*
Dynamically allocated buffer used to contain string that are used
during precompiled header prefix comparisons.
*/
STATIC_THREAD char
		*pch_buffer = NULL;
			/* Not allocated on a per-file basis. */

STATIC_THREAD sizeof_t
		size_pch_buffer;
			/* Current size of pch_buffer. */


static void expand_pch_buffer(sizeof_t size_needed)
/*
Expand the pch_buffer by reallocating it, so that its total
size is at least size_needed.  Called by ensure_pch_buffer_space.
*/
{
  sizeof_t new_size;

  new_size = size_pch_buffer +
             PCH_BUFFER_INCREMENTAL_ALLOCATION;
  if (new_size < size_needed) new_size  = size_needed;
  pch_buffer = realloc_buffer(pch_buffer, size_pch_buffer, new_size);
  size_pch_buffer = new_size;
}  /* expand_pch_buffer */


/*
Ensure that pch_buffer has at least size_needed bytes in it.
If not, expand pch_buffer by reallocating it.
*/
#define ensure_pch_buffer_space(size_needed)                 \
{ if (size_pch_buffer < size_needed) {                       \
    expand_pch_buffer((sizeof_t)(size_needed));              \
  }  /* if */                                                          \
}  /* ensure_pch_buffer_space */


#define FILE_NAME_BUFFER_INCREMENTAL_ALLOCATION 1024
			/* Initial and incremental allocation sizes for
			   file_name_buffer.  The initial allocation
			   should be such that almost all cases can be
			   accepted (so that the realloc is hardly ever
			   needed). */

typedef struct a_file_name_buffer *a_file_name_buffer_ptr;
typedef struct a_file_name_buffer {
  char		*name;
			/* Pointer to a buffer containing the file name. */
  sizeof_t	size;
			/* Allocated size of the buffer pointed to by
			   file_name. */
} a_file_name_buffer;

STATIC_THREAD a_file_name_buffer
		file_name_buffer;
			/* Buffer use to temporarily record file names. */


static void expand_file_name_buffer(a_file_name_buffer_ptr fnbp,
                                    sizeof_t		   size_needed)
/*
Expand the file_name_buffer by reallocating it, so that its total
size is at least size_needed.  Called by ensure_file_name_buffer_space.
*/
{
  sizeof_t new_size;

  new_size = fnbp->size + FILE_NAME_BUFFER_INCREMENTAL_ALLOCATION;
  if (new_size < size_needed) new_size  = size_needed;
  fnbp->name = realloc_buffer(fnbp->name, fnbp->size, new_size);
  fnbp->size = new_size;
}  /* expand_file_name_buffer */


/*
Ensure that file_name_buffer has at least size_needed bytes in it.
If not, expand file_name_buffer by reallocating it.
*/
#define ensure_file_name_buffer_space(fnb, size_needed)                 \
{ if ((fnb).size < (size_needed)) {                    			  \
    expand_file_name_buffer(&(fnb), (sizeof_t)(size_needed));              \
  }  /* if */                                                          \
}  /* ensure_file_name_buffer_space */


static a_const_char *build_pch_file_name(a_const_char *file_name)
/*
Concatenate the PCH directory with the specified file name.  Uses a
local file name buffer for storage.  Return a pointer to the name.  If no
directory name is being used, a pointer to the original name is returned.
*/
{
  a_const_char *result;

  if (pch_dir_name == NULL || is_absolute_file_name(file_name)) {
    result = file_name;
  } else {
    if (file_name_text_buffer == NULL) {
      /* Allocate this buffer the first time it is needed. */
      file_name_text_buffer = alloc_text_buffer(256);
    }  /* if */
    (void)combine_dir_and_file_name(pch_dir_name, file_name,
                                    file_name_text_buffer);
    result = file_name_text_buffer->buffer;
  }  /* if */
  return result;
}  /* build_pch_file_name */


static void initialize_pch_id_string(void)
/*
Create the string that is used to identify a flag as a precompiled header
associated with this compiler version.
*/
{
  Small_string<PCH_ID_STRING_LENGTH>
                temp_pch_str("EDG C/C++ version ",VERSION_NUMBER,
                             " (", build_stamp, ")\n");

  check_assertion_str2((size_t_arg(temp_pch_str.length()) <
                        size_t_arg(PCH_ID_STRING_LENGTH)),
                       "initialize_pch_id_string:",
                       "PCH ID string too long");
  temp_pch_str.write_to_buffer(pch_id_string, PCH_ID_STRING_LENGTH);
  pch_id_string_length = temp_pch_str.length() + 1;
}  /* initialize_pch_id_string */


static a_pch_event_ptr alloc_pch_event(a_pch_event_kind kind)
/*
Allocate and initialize a precompiled header event record.
*/
{
  a_pch_event_ptr pep;

  /* Allocate a new entry. */
  pep = (a_pch_event_ptr)alloc_general(sizeof(a_pch_event));
#if DEBUG
  num_pch_events_allocated++;
#endif /* DEBUG */
  pep->next = NULL;
  pep->kind = kind;
  switch (kind) {
    case pchek_command_line:
      pep->variant.cl_option.kind = optk_none;
      pep->variant.cl_option.opt_value = FALSE;
      break;
    case pchek_pp_directive:
      pep->variant.ppd_kind = ppd_not_valid;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  pep->value = NULL;
  pep->position = null_source_position;
  pep->match_found = FALSE;
  return pep;
}  /* alloc_pch_event */


void add_pch_event(a_pch_event_kind	kind,
		   a_pp_directive_kind	ppd_kind,
		   a_const_char		*value,
		   a_source_position	*position,
		   a_line_number	actual_line)
/*
Add a precompiled header event record to the list of events for the current
file.
*/
{
  a_pch_event_ptr	pep;

  db_enter(4, "add_pch_event");
  pep = alloc_pch_event(kind);
  if (kind == pchek_pp_directive) {
    pep->variant.ppd_kind = ppd_kind;
  }  /* if */
  if (value != NULL) {
    /* Copy the value string. */
    pep->value = (char *)alloc_general((sizeof_t)(strlen(value) + 1));
    /* coverity[deref_ptr_in_call] */  /* Coverity bug. */
    (void)strcpy(pep->value, value);
  }  /* if */
  pep->position = *position;
  /* Replace the sequence number with the actual file line number. */
  pep->position.seq = actual_line;
  /* Add this entry to the list. */
  if (pch_event_list_head == NULL) pch_event_list_head = pep;
  if (pch_event_list_tail != NULL) pch_event_list_tail->next = pep;
  pch_event_list_tail = pep;
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("pch_event")) {
    fprintf(f_debug, "Added PCH event: %s, value=%s, line %lu, col %d\n",
            pch_event_kind_names[(int)pep->kind],
            pep->value == NULL ? "(NULL)" : pep->value,
            (unsigned long)pep->position.seq, pep->position.column);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* add_pch_event */


void add_command_line_pch_event(a_pch_event_kind	kind,
                                an_option_kind		opt_kind,
				a_boolean		opt_value,
				a_const_char		*opt_arg)
/*
Add a precompiled header event to the list of events associated with
the command line.
*/
{
  a_pch_event_ptr	pep;

  db_enter(4, "add_command_line_pch_event");
  check_assertion_str2(kind == pchek_command_line,
                       "add_command_line_pch_event:",
                       "invalid PCH event kind");
  pep = alloc_pch_event(kind);
  pep->variant.cl_option.kind = opt_kind;
  pep->variant.cl_option.opt_value = opt_value;
  if (opt_arg != NULL) {
    /* Command line events are reused for multiple source files so
       the value string must be allocated in general memory. */
    pep->value = (char *)alloc_general((sizeof_t)(strlen(opt_arg) + 1));
    /* coverity[deref_ptr_in_call] */  /* Coverity bug. */
    (void)strcpy(pep->value, opt_arg);
  }  /* if */
  /* Add this entry to the list. */
  if (pch_cmd_line_event_list_head == NULL) {
    pch_cmd_line_event_list_head = pep;
  }  /* if */
  if (pch_cmd_line_event_list_tail != NULL) {
    pch_cmd_line_event_list_tail->next = pep;
  }  /* if */
  pch_cmd_line_event_list_tail = pep;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Added PCH event: %s, value=%s\n",
            pch_event_kind_names[(int)pep->kind],
            pep->value == NULL ? "(NULL)" : pep->value);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* add_command_line_pch_event */


#if DEBUG
static void db_pch_event(a_pch_event_ptr pep)
/*
Display a PCH event for debugging purposes.
*/
{
  fprintf(f_debug, "Event kind: %s", pch_event_kind_names[(int)pep->kind]);
  switch (pep->kind) {
    case pchek_command_line:
      fprintf(f_debug, ", option kind: %d", (int)pep->variant.cl_option.kind);
      fprintf(f_debug, ", option value: %s",
              pep->variant.cl_option.opt_value ? "TRUE" : "FALSE");
      break;
    case pchek_pp_directive:
      fprintf(f_debug, ", ppd_kind: %s",
              pp_directive_kind_names[(int)pep->variant.ppd_kind]);
      break;
    default:
      unexpected_condition();
  }  /* switch */
  fprintf(f_debug, ", value: %s", pep->value == NULL ? "(NULL)" : pep->value);
  fprintf(f_debug, ", seq: %lu, column: %lu\n",
          (unsigned long)pep->position.seq,
          (unsigned long)pep->position.column);
}  /* db_pch_event */
#endif /* DEBUG */


static void find_last_event_to_use(void)
/*
Once the end of the file prefix has been found, this routine determines the
last usable event in the list.  The last usable event is the last
zero-level (i.e., not within a preprocessing if directive) event that
precedes the header stop position.
*/
{
  a_pch_event_ptr	last_if_event = NULL;
  a_pch_event_ptr	pep = pch_event_list_head;
  a_pch_event_ptr	last_event_to_use = NULL;
  int			nesting_level = 0;

  for (; pep != NULL; pep = pep->next) {
    if (pep->kind == pchek_pp_directive) {
      a_pp_directive_kind	ppd_kind = pep->variant.ppd_kind;
      if (ppd_kind == ppd_if ||
          ppd_kind == ppd_ifdef ||
          ppd_kind == ppd_ifndef) {
        /* This is an if directive, save a pointer to the last if event
           and increment the if nesting level. */
        pep->last_if_event = last_if_event;
        last_if_event = pep;
        nesting_level++;
      } else if (ppd_kind == ppd_endif) {
        /* On an endif, decrement the if nesting level.  If there is
           an if/endif mismatch, don't try to do any further PCH
           processing. */
        if (nesting_level == 0) {
          /* A nesting error occurred.  Don't do any PCH processing.
             An error will be diagnosed when the file is compiled. */
          abandon_pch_processing();
          break;
        } else {
          nesting_level--;
          if (last_if_event != NULL) {
            last_if_event = last_if_event->last_if_event;
          }  /* if */
        }  /* if */
      }  /* if */
      if (nesting_level == 0 && 
          (ppd_kind == ppd_include ||
#if MICROSOFT_EXTENSIONS_ALLOWED
           ppd_kind == ppd_using ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
           ppd_kind == ppd_define ||
           ppd_kind == ppd_pragma ||
           ppd_kind == ppd_endif)) {
        /* Update the pointer to the last zero level event.  When we've
           scanned the whole event list, this will point to the last event
           to be included in the prefix. */
        last_event_to_use = pep;
      }  /* if */
    }  /* if */
  }  /* for */
  if (last_event_to_use != NULL) {
    /* Discard any events that follow the new last one. */
    last_event_to_use->next = NULL;
    pch_event_list_tail = last_event_to_use;
    header_stop_source_position = last_event_to_use->position;
  } else {
    /* We did not find an eligible event.  Suppress any PCH processing. */
    abandon_pch_processing();
  }  /* if */
}  /* find_last_event_to_use */


static void build_prefix_information(void)
/*
Do an initial scan of the primary source file to build the file prefix
information.
*/
{
  a_source_position	saved_pos_curr_token;
  a_source_position	saved_error_position;
  a_boolean		save_fetch_pp_tokens = fetch_pp_tokens;

  saved_pos_curr_token = pos_curr_token;
  saved_error_position = error_position;
  /* Set the flag that indicates that we are building the file prefix
     information.  This affects the way in which preprocessing directives
     are handled and the way end-of-file is processed. */
  building_pch_prefix = TRUE;
  fetch_pp_tokens = TRUE;
  /* If any preinclude files were specified, create an event for them. */
  if (preinclude_file_list != NULL || macro_preinclude_file_list) {
    create_preinclude_pch_event();
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (preusing_file_list != NULL) {
    create_preinclude_pch_event();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  /* Simply do a get_token call.  This will return the first token
     of the file that is not a comment or a preprocessing directive.
     Because the prefix information includes only preprocessing directives,
     they will have all been seen by the time the first token is returned.
     The actual work to build the prefix information is done by special
     processing in preproc.c that is enabled when building_pch_prefix is
     TRUE. */
  (void)get_token();
  if (curr_ise != NULL) {
    /* If we didn't reach the end of the source file, pop the input stack
       and close the primary input file. */
    pop_input_stack();
  }  /* if */
  /* Go through the event list and find the last eligible event. */
  find_last_event_to_use();
  /* Reset the state information maintained by the lexical routines. */
  lexical_reset();
  /* Update curr_char_loc to point to the end of the current line.  This
     will force the next token to begin on a new line. */
  building_pch_prefix = FALSE;
  pos_curr_token = saved_pos_curr_token;
  error_position = saved_error_position;
  fetch_pp_tokens = save_fetch_pp_tokens;
}  /* build_prefix_information */


void process_prefix_pragma_hdrstop(void)
/*
Do processing needed when a pragma hdrstop is found while doing the
prefix scan.
*/
{
  pragma_hdrstop_found = TRUE;
}  /* process_prefix_pragma_hdrstop */


static void open_pch_output_file(void)
/*
Create or truncate the precompiled header file.
*/
{
  a_const_char *file_name;

  if (create_precompiled_header) {
    file_name = pch_output_file_name;
  } else {
    file_name = derived_name(primary_source_file_name, PCH_FILE_SUFFIX);
  }  /* if */
  pch_file_name = build_pch_file_name(file_name);
  if (is_regular_file(pch_file_name)) {
    /* Delete the file before writing it.  This way, if someone already
       has the file open for reading, we won't be overwriting the
       same file that they are reading. */
    delete_file(pch_file_name);
  }  /* if */
  f_pch_output = open_output_file_with_error_handling(
                    pch_file_name, /*binary_file=*/TRUE, /*update_mode=*/FALSE,
                    OFF_NO_OPTIONS, ec_precompiled_header);
}  /* open_pch_output_file */


static a_boolean open_pch_input_file(a_const_char *file_name)
/*
Open the PCH input file.  Return TRUE if the file could be opened.
If the file cannot be opened, and the name was explicitly specified by the
user, then issue an error.
*/
{
  an_open_file_flag_set	open_flags = OFF_NO_OPTIONS;

  if (automatic_pch_processing) {
    /* Silently ignore errors if the input file was not explicitly
       specified. */
    open_flags = OFF_OKAY_IF_NOT_FOUND | OFF_OKAY_IF_CANNOT_OPEN |
                 OFF_OKAY_IF_NOT_REGULAR | OFF_OKAY_IF_DIRECTORY;
  }  /* if */
  f_pch_input = open_input_file_with_error_handling(file_name,
                                                    /*binary_file=*/TRUE,
                                                    open_flags,
                                                    ec_precompiled_header);
  return f_pch_input != NULL;
}  /* open_pch_input_file */


static void remove_pch_input_file(void)
/*
The current input file is not usable for some reason, so remove it.
This may be done because the include files used by the PCH have
changed.
*/
{
  db_enter(3, "remove_pch_input_file");
#if DEBUG
  if (debug_level >= 3) {
    fprintf(f_debug, "Removing PCH file: %s\n", pch_input_file_name);
  }  /* if */
#endif /* DEBUG */
  /* First close the file. */
  if (f_pch_input != NULL) (void)fclose(f_pch_input);
  f_pch_input = NULL;
  delete_file(pch_input_file_name);
  db_exit();
}  /* remove_pch_input_file */


static void remove_assoc_pch_file_if_not_being_used(void)
/*
See if the PCH file being used (if any) is associated with the
file currently being compiled.  If not, remove the associated file.
*/
{
  a_const_char	*assoc_pch_file_name;
  a_boolean	remove_file = FALSE;

  db_enter(3, "remove_assoc_pch_file_if_not_being_used");
  /* Append the PCH file prefix to the primary source file base name. */
  assoc_pch_file_name = derived_name(primary_source_file_name,
                                     PCH_FILE_SUFFIX);
  /* Add the PCH directory name. */
  assoc_pch_file_name = build_pch_file_name(assoc_pch_file_name);
  if (!is_regular_file(assoc_pch_file_name)) {
    /* The file does not exist -- nothing to do. */
  } else if (!using_a_pch_file) {
     /* We're not using a PCH file, remove the old one. */
     remove_file = TRUE;
  } else if (compare_file_names(assoc_pch_file_name,
                                pch_input_file_name) != 0) {
    /* The PCH file in use is not associated with this file -- remove the
       associated file. */
    remove_file = TRUE;
  }  /* if */
  if (remove_file) {
#if DEBUG
    if (debug_level >= 3) {
      fprintf(f_debug, "Removing PCH file: %s\n", assoc_pch_file_name);
    }  /* if */
#endif /* DEBUG */
    delete_file(assoc_pch_file_name);
  }  /* if */
  db_exit();
}  /* remove_assoc_pch_file_if_not_being_used */


static void pch_write_string(a_const_char *str)
/*
Write a null terminated character string to the PCH output file.  The
string is written as a length followed by the characters of the string.
Both the length and the actual string include the null terminator.
*/
{
  sizeof_t	length;
  if (str != NULL) {
    length = strlen(str) + 1;
    pch_write_value(length);
    fwrite_with_check(str, length, f_pch_output);
  } else {
    /* The string pointer is null.  Represent this as a zero length
       string. */
    length = 0;
    pch_write_value(length);
  }  /* if */
}  /* pch_write_string */


static char *pch_read_string(void)
/*
Read a string from the PCH input file.  In the
file, the string consists of a length followed by the actual
characters.  A pointer to the pch_buffer containing the string
is returned.
*/
{
  sizeof_t	length;
  /* coverity[+taint_source: arg-0] */
  pch_read_value(length);
  ensure_pch_buffer_space(length);
  if (length == 0) {
    /* The original pointer was NULL.  Return a NULL string. */
    pch_buffer[0] = '\0';
  } else {
    /* Read the string.  The length includes the null terminator. */
    fread_with_check(pch_buffer, length, f_pch_input);
  }  /* if */
  return pch_buffer;
}  /* pch_read_string */


static void write_pch_events(a_pch_event_ptr list)
/*
Write a list of precompiled header events to the PCH output file.
*/
{
  a_pch_event_ptr	pep;
  a_pch_event_kind	dummy_pchek;

  for (pep = list; pep != NULL; pep = pep->next) {
    check_assertion(pep->kind != pchek_none);
    pch_write_value(pep->kind);
    switch (pep->kind) {
      case pchek_command_line:
        pch_write_value(pep->variant.cl_option.kind);
        pch_write_value(pep->variant.cl_option.opt_value);
        break;
      case pchek_pp_directive:
        pch_write_value(pep->variant.ppd_kind);
        break;
      default:
        unexpected_condition();
    }  /* switch */
    pch_write_string(pep->value);
    pch_write_value(pep->position);
  }  /* for */
  /* An event kind of "none" terminates the list. */
  dummy_pchek = pchek_none;
  pch_write_value(dummy_pchek);
}  /* write_pch_events */


static a_boolean read_pch_event(a_pch_event_ptr pep)
/*
Read a single PCH event from the PCH input file.  Note that the
string pointer returned points into the pch_buffer (i.e., it exists
only for a short time).  Return TRUE if an event is successfully
read.  Return FALSE when the pchek_none marker at the end of the list
is encountered.
*/
{
  a_boolean		result = FALSE;

  pch_read_value(pep->kind);
  if (pep->kind == pchek_none) {
    /* This is the end of the list. */
  } else {
    result = TRUE;
    switch (pep->kind) {
      case pchek_command_line:
        pch_read_value(pep->variant.cl_option.kind);
        pch_read_value(pep->variant.cl_option.opt_value);
        break;
      case pchek_pp_directive:
        pch_read_value(pep->variant.ppd_kind);
        break;
      default:
        unexpected_condition();
    }  /* switch */
    pep->value = pch_read_string();
    pch_read_value(pep->position);
  }  /* if */
  return result;
}  /* read_pch_event */


static a_boolean equivalent_pch_events(a_pch_event_ptr pep1,
                                       a_pch_event_ptr pep2)
/*
Return TRUE if two PCH events are equivalent.
*/
{
  a_boolean	result = FALSE;
  a_boolean	is_include = FALSE;

  if (pep1->kind == pep2->kind) {
    switch (pep1->kind) {
      case pchek_command_line:
        if (pep1->variant.cl_option.kind == pep2->variant.cl_option.kind) {
          result = pep1->variant.cl_option.opt_value ==
                   pep2->variant.cl_option.opt_value;
        }  /* if */
        break;
      case pchek_pp_directive:
        result = pep1->variant.ppd_kind == pep2->variant.ppd_kind;
        is_include = pep1->variant.ppd_kind == ppd_include;
        break;
      default:
        unexpected_condition();
    }  /* switch */
    /* If the events are equal so far, compare the value strings. */
    if (result) {
      if ((pep1->value == NULL || *(pep1->value) == '\0') &&
          (pep2->value == NULL || *(pep2->value) == '\0')) {
        /* Both value strings are empty so are equivalent.  Leave result
           set to TRUE. */
      } else if (pep1->value == NULL || pep2->value == NULL) {
        /* Only one string is NULL.  The values are not equal. */
        result = FALSE;
      } else {
        if (!is_include) {
          result = strcmp(pep1->value, pep2->value) == 0;
        } else {
          /* For include directives, compare the values as file names. */
          /* First make sure the include kind matches (i.e., <...> vs.
             "...".  This is not done in Microsoft bugs mode. */
          result = microsoft_bugs || pep1->value[0] == pep2->value[0];
          if (result) {
            /* Now compare the actual file names. */
            result = f_compare_file_names(pep1->value, pep2->value,
                                          /*ignore_delimiters=*/TRUE,
                                          /*is_partial_file_name=*/TRUE,
                                          /*in_general_memory=*/FALSE) == 0;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Comparing PCH event: ");
    db_pch_event(pep1);
    fprintf(f_debug, "  with PCH event: ");
    db_pch_event(pep2);
    fprintf(f_debug, "  Equivalent: %s\n", result ? "TRUE" : "FALSE");
  }  /* if */
#endif /* DEBUG */
  return result;
}  /* equivalent_pch_events */


static void write_list_of_file_timestamps(a_source_file_ptr sfp)
/*
Go through a list of source file entries and write the file name and
timestamp to the PCH output file.  Do a recursive call to process any
child files encountered.
*/
{
  db_enter(5, "write_list_of_file_timestamps");
  for (; sfp != NULL; sfp = sfp->next) {
    time_t	mod_time;
    /* Only do this for include files, not for the primary source file
       or for the source file entry associated with a primary source
       file from which precompiled header information has been restored.
       Ignore entries with a NULL full_name.  These are mostly created by
       #line directives (that should suppress PCH creation), but they can be
       created by other things like the GCC system_header pragma. */
    if (sfp->is_include_file && sfp->full_name != NULL) {
      (void)get_file_modification_time(sfp->full_name, &mod_time);
      pch_write_string(sfp->full_name);
      pch_write_value(mod_time);
#if DEBUG
      if (debug_level >= 5) {
        fprintf(f_debug, "Writing file timestamp for %s, time is %ld\n",
                sfp->full_name, (long)mod_time);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
    if (sfp->first_child_file != NULL) {
      write_list_of_file_timestamps(sfp->first_child_file);
    }  /* if */
  }  /* for */
  db_exit();
}  /* write_list_of_file_timestamps */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void write_list_of_metadata_file_timestamps(
                                                  a_cli_metadata_file_ptr cmfp)
/*
Go through a list of cli metadata file entries and write the file name and
timestamp to the PCH output file.
*/
{
  db_enter(5, "write_list_of_metadata_file_timestamps");
  for (; cmfp != NULL; cmfp = cmfp->next) {
    time_t	mod_time;

    (void)get_file_modification_time(cmfp->full_name, &mod_time);
    pch_write_string(cmfp->full_name);
    pch_write_value(mod_time);
#if DEBUG
    if (debug_level >= 5) {
      fprintf(f_debug, "Writing file timestamp for %s, time is %ld\n",
              cmfp->full_name, (long)mod_time);
    }  /* if */
#endif /* DEBUG */
  }  /* for */
  db_exit();
}  /* write_list_of_metadata_file_timestamps */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void write_list_of_module_file_timestamps(void)
/*
Go through a list of module file entries and write the file name and timestamp
to the PCH output file.
*/
{
  a_module_import_decl_ptr midp;

  db_enter(5, "write_list_of_module_file_timestamps");
  for (midp = il_header.imported_modules; midp != NULL; midp = midp->next) {
    time_t mod_time;

    (void)get_file_modification_time(midp->module_info->resolved_file,
                                     &mod_time);
    pch_write_string(midp->module_info->resolved_file);
    pch_write_value(mod_time);
#if DEBUG
    if (debug_level >= 5) {
      fprintf(f_debug, "Writing file timestamp for %s, time is %ld\n",
              midp->module_info->resolved_file, (long)mod_time);
    }  /* if */
#endif /* DEBUG */
  }  /* for */
  db_exit();
}  /* write_list_of_module_file_timestamps */


static void write_file_timestamps(void)
/*
Write a list of file names and their associated modification times.  Any
external file that the front end reads during the compilation is a candidate
for inclusion on this list (e.g., include files, module files, assembly files).
In order for this PCH to be used later, none of the files may have changed.
*/
{
  write_list_of_file_timestamps(il_header.primary_source_file);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled) {
    write_list_of_metadata_file_timestamps(il_header.cli_metadata_files);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  write_list_of_module_file_timestamps();
  /* Write a NULL string to mark the end of the list. */
  pch_write_string((char *)NULL);
}  /* write_file_timestamps */


static void write_saved_variables(void)
/*
Save the contents of the variables as specified by the saved
variable lists.
*/
{
  int				i;
  a_pch_saved_variable_ptr	psvp;

  db_enter(4, "write_saved_variables");
  for (i = 0; i < num_of_saved_variable_lists; ++i) {
    for (psvp = saved_variable_array_list[i];
         psvp->var_address != NULL;
         psvp++) {
      a_void_ptr	address = psvp->var_address;
      /* If the indirect flag is set, get the address stored at the
         specified address. */
      if (psvp->indirect) address = *(a_void_ptr*)address;
#if DEBUG
      if (debug_level >= 5) {
        fprintf(f_debug, "Saving %5lu bytes at %p, variable %s %s\n",
                (unsigned long)psvp->var_size, address,
                psvp->var_name == NULL
                  ? "(name not available)"
                  : psvp->var_name,
                psvp->indirect ? "(indirect)" : "");
      }  /* if */
#endif /* DEBUG */
      fwrite_with_check(address, psvp->var_size, f_pch_output);
    }  /* for */
  }  /* for */
  db_exit();
}  /* write_saved_variables */


static void read_saved_variables(void)
/*
Save the contents of the variables as specified by the saved
variable lists.
*/
{
  int				i;
  a_pch_saved_variable_ptr	psvp;

  check_file_section_id(pfs_saved_variables);
  for (i = 0; i < num_of_saved_variable_lists; ++i) {
    for (psvp = saved_variable_array_list[i];
         psvp->var_address != NULL;
         psvp++) {
      a_void_ptr	address = psvp->var_address;
      /* If the indirect flag is set, get the address stored at the
         specified address. */
      if (psvp->indirect) address = *(a_void_ptr*)address;
#if DEBUG
      if (debug_level >= 5) {
        fprintf(f_debug, "Restoring %5lu bytes at %p, variable %s %s\n",
                (unsigned long)psvp->var_size, address,
                psvp->var_name == NULL
                  ? "(name not available)"
                  : psvp->var_name,
                psvp->indirect ? "(indirect)" : "");
      }  /* if */
#endif /* DEBUG */
      fread_with_check(address, psvp->var_size, f_pch_input);
    }  /* for */
  }  /* for */
}  /* read_saved_variables */


static void write_mem_alloc_history(void)
/*
Write the memory allocation history information to the PCH output
file.
*/
{
  db_enter(4, "write_mem_alloc_history");
  pch_write_value(size_of_mem_alloc_history);
  pch_write_value(mem_alloc_history_entries_used);
  /* If this assertion fails, the count of memory allocation histories
     used is negative, which should never happen. */
  check_assertion(mem_alloc_history_entries_used >= 0);
  fwrite_with_check(mem_alloc_history,
                    sizeof(a_mem_alloc_history) *
                                        (size_t)mem_alloc_history_entries_used,
                    f_pch_output);
  db_exit();
}  /* write_mem_alloc_history */


static a_boolean read_mem_alloc_history(void)
/*
Read the memory allocation history information from the PCH input file and
restore the memory allocation state.  First, make sure that any memory 
already allocated by the current process matches the corresponding
entries read from the file.  If so, duplicate the remaining entries in the
allocation list so that we know we have reserved the memory needed to
restore the memory regions.
*/
{
  a_boolean                  successful = TRUE;
  a_mem_alloc_history_number new_size;
  a_mem_alloc_history_number n;
  sizeof_t                   bytes_in_new_alloc_history;

  db_enter(4, "read_mem_alloc_history");
  check_file_section_id(pfs_mem_alloc_info);
  /* Read the memory allocation history information.  Read it into
     a separate area so that it can be compared with the existing
     information. */
  pch_read_size_checked_value(new_size, size_of_mem_alloc_history);
  /* coverity[+taint_source: arg-0] */
  pch_read_size_checked_value(new_alloc_history_entries,
                              mem_alloc_history_entries_used);
  bytes_in_new_alloc_history = size_t_arg(new_alloc_history_entries) *
                                                 sizeof(a_mem_alloc_history);
  new_alloc_history = (a_mem_alloc_history_ptr)alloc_general
                             (bytes_in_new_alloc_history);
  fread_with_check(new_alloc_history,
                   bytes_in_new_alloc_history,
                   f_pch_input);
  /* Make sure the entries for the current compilation match the initial
     entries read from the file. */
  for (n = 0; n < mem_alloc_history_entries_used; ++n) {
    if (!equivalent_mem_alloc_history(mem_alloc_history[n],
                                      new_alloc_history[n])) {
      successful = FALSE;
      break;
    }  /* if */
  }  /* for */
#if !USE_MMAP_FOR_MEMORY_REGIONS
  if (successful) {
    /* The memory allocations that have been done so far are compatible
       with those done in the original compilation.  Perform the
       remaining allocations needed to read in the memory regions. */
    for (; n < new_alloc_history_entries; ++n) {
      /* Allocate the needed block. */
      (void)alloc_new_mem_block(new_alloc_history[n].size);
      if (!equivalent_mem_alloc_history(mem_alloc_history[n],
                                        new_alloc_history[n])) {
        successful = FALSE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  if (!successful) {
    mismatch_reason = ec_memory_mismatch;
    if (automatic_pch_processing && verbose_pch_messages) {
      pos_st_warning(mismatch_reason, &null_source_position,
                     format_file_name(pch_input_file_name));
    }  /* if */
  }  /* if */
  db_exit();
  return successful;
}  /* read_mem_alloc_history */


#if USE_MMAP_FOR_MEMORY_REGIONS
static void write_memory_used_for_memory_regions(void)
/*
Write, to the PCH output file, the contents of the memory that has been
allocated for memory region purposes.  This routine is used when the
memory region information will be accessed using mmap by the consumer of
the PCH file.  The memory is written this way, instead of as individual
memory regions, to avoid the need to write each region at a file offset
that is a multiple of the host page size.
*/
{
  int		i;

  for (i = 0; i < num_of_mem_alloc_history_entries; ++i) {
    a_mem_alloc_history_ptr	mahp = &mem_alloc_history[i];
    (void)seek_to_page_alignment(f_pch_output);
    fwrite_with_check(mahp->addr, mahp->size, f_pch_output);
  }  /* for */
}  /* write_memory_used_for_memory_regions */

#else /* !USE_MMAP_FOR_MEMORY_REGIONS */

static void write_a_memory_region(a_memory_region_number number)
/*
Write the blocks comprising a single memory region to the PCH output
file.  The header information is written out as part of the memory
block.  In order to use the precompiled header, we first guarantee that
the memory blocks used for memory region storage have been allocated
in exactly the same manner as that in which they were created.
*/
{
  a_mem_block_header_ptr	mbhp = mem_region_table[number];
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Writing memory region %d\n", number);
  }  /* if */
#endif /* DEBUG */
  while (mbhp != NULL) {
    sizeof_t	size;
    sizeof_t	region_size;
    size = mbhp->next_avail_in_block - (char *)mbhp;
    region_size = mbhp->after_end_of_block - (char *)mbhp;
    pch_write_value(size);
    pch_write_value(region_size);
    pch_write_value(mbhp);
    fwrite_with_check(mbhp, size, f_pch_output);
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Writing %lu bytes from %p\n", (unsigned long)size,
              mbhp);
    }  /* if */
#endif /* DEBUG */
    mbhp = mbhp->next;
  }  /* while */
}  /* write_a_memory_region */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */


#if USE_MMAP_FOR_MEMORY_REGIONS

static void read_memory_used_for_memory_regions(void)
/*
Read, from the PCH input file, the contents of the memory that has been
allocated for memory region purposes.  This routine is used when the
memory region information will be accessed using mmap by the consumer of
the PCH file.
*/
{
  size_t              i;
  sizeof_t            offset;
  a_mapped_input_file mapped_file;

  /* Open the file in a way that it can be used for file mapping purposes. */
  mapped_file = open_mapped_input_file(pch_input_file_name, f_pch_input);
  /* The memory regions that have already been allocated should be
     unmapped so that the address space is available to be remapped. */
  free_mapped_mem_blocks();
  /* Get the current input file position. */
  offset = (sizeof_t)ftell(f_pch_input);
  for (i = 0; i < size_t_arg(new_alloc_history_entries); ++i) {
    a_mem_alloc_history_ptr	mahp = &new_alloc_history[i];
    offset = do_page_alignment(offset);
    /* coverity[leaked_storage] */
    (void)map_input_file_to_region(mapped_file, /*read_only=*/FALSE, offset,
                                   mahp->size, mahp->addr,
                                   pch_input_file_name);
    offset += mahp->size;
    /* Create a memory allocation history entry for this block. */
    record_mapped_mem_block(mahp->addr, mahp->size);
#if DEBUG
    if (debug_level >= 5) {
      fprintf(f_debug, "Mapped bytes from %p for %lu bytes from PCH\n",
              mahp->addr, (unsigned long)mahp->size);
    }  /* if */
#endif /* DEBUG */
  }  /* for */
  close_mapped_input_file(mapped_file);
}  /* read_memory_used_for_memory_regions */

#else /* !USE_MMAP_FOR_MEMORY_REGIONS */

static void read_a_memory_region(a_memory_region_number number)
/*
Read the blocks comprising a single memory region from the PCH input
file.  See write_a_memory_region for more information.
*/
{
  a_mem_block_header_ptr	mbhp = mem_region_table[number];

#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Reading memory region %d\n", number);
  }  /* if */
#endif /* DEBUG */
  for (;;) {
    sizeof_t	size;
    sizeof_t	region_size;
    pch_read_value(size);
    pch_read_value(region_size);
    pch_read_value(mbhp);
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Reading %lu bytes into %p\n", (unsigned long)size,
              mbhp);
    }  /* if */
#endif /* DEBUG */
    fread_with_check((a_stdio_arg)mbhp, size, f_pch_input);
    /* See if this is the last block in the memory region. */
    if (mbhp->next == NULL) break;
  }  /* for */
}  /* read_a_memory_region */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */


static void write_memory_regions(void)
/*
Write the memory region information to the PCH output file.  This includes
header information about the memory regions such as the mem_region_table.
*/
{
  size_t mem_regions_used = 0;
  size_t function_defs_used = 0;
  db_enter(4, "write_memory_regions");
  /* Write a copy of the IL header. */
  pch_write_value(il_header);
  if (highest_used_region_number != NULL_region_number) {
    mem_regions_used = (size_t)highest_used_region_number + 1;
  }  /* if */
  /* Write the memory region table and the region_scope_entry table from
     the IL header.  Note that index_for_il_file is not written. */
  pch_write_value(highest_used_region_number);
  fwrite_with_check(mem_region_table,
                    sizeof(a_mem_block_header_ptr) * mem_regions_used,
                    f_pch_output);
  fwrite_with_check(il_header.region_scope_entry,
                    sizeof(a_mem_block_header_ptr) * mem_regions_used,
                    f_pch_output);
  if (highest_used_function_def_number != NULL_function_def_number) {
    function_defs_used = (size_t)highest_used_function_def_number + 1;
  }  /* if */
  pch_write_value(highest_used_function_def_number);
  if (function_defs_used > 1) {
    fwrite_with_check(il_header.function_def_table,
                      sizeof(a_function_def_descr) * function_defs_used,
                      f_pch_output);
  }  /* if */
#if DEBUG
  /* Write the allocated_in_region information. */
  fwrite_with_check(allocated_in_region,
                    sizeof(unsigned long) * mem_regions_used,
                    f_pch_output);
#endif /* DEBUG */
#if USE_MMAP_FOR_MEMORY_REGIONS
  /* When using memory mapping, instead of writing the memory regions one
     at a time, we write the entire memory blocks that contain the
     memory regions.  File sections that are to be accessed later via
     mmap must be written at file offsets that are multiples of the host
     page size.  Writing out individual memory regions in this way
     wastes too much space for alignment. */
  write_memory_used_for_memory_regions();
#else /* USE_MMAP_FOR_MEMORY_REGIONS */
  /* Write the actual memory regions to the PCH file. */
  {
    a_memory_region_number	n;
    for (n = 0; n < mem_regions_used; ++n) {
      write_a_memory_region(n);
    }  /* for */
  }
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  db_exit();
}  /* write_memory_regions */


static void read_memory_regions(void)
/*
Read the memory region information from the PCH output file.  This includes
header information about the memory regions such as the mem_region_table.
*/
{
  size_t mem_regions_used = 0;
  size_t function_defs_used = 0;

  db_enter(4, "read_memory_regions");
  check_file_section_id(pfs_memory_regions);
  /* Read the copy of the IL header. */
  pch_read_size_checked_value(il_header_from_pch, il_header);
  /* Read the memory region table and the region_scope_entry table from
     the IL header.  Note that index_for_il_file is not written. */
  /* coverity[+taint_source: arg-0] */
  pch_read_value(highest_used_region_number);
  /* Make sure that the tables allocated to store the memory region
     information are large enough. */
  ensure_mem_region_table_space(highest_used_region_number);
  if (highest_used_region_number != NULL_region_number) {
    mem_regions_used = (size_t)highest_used_region_number + 1;
  }  /* if */
  fread_with_check(mem_region_table,
                  sizeof(a_mem_block_header_ptr) * mem_regions_used,
                  f_pch_input);
  fread_with_check(il_header.region_scope_entry,
                   sizeof(a_scope_ptr) * mem_regions_used,
                   f_pch_input);
  pch_read_value(highest_used_function_def_number);
  if (highest_used_function_def_number != NULL_function_def_number) {
    function_defs_used = (size_t)highest_used_function_def_number + 1;
  }  /* if */
  if (function_defs_used > 1) {
    /* Allocate the IL header function definition table, if needed. */
    ensure_function_def_table_space(highest_used_function_def_number);
    fread_with_check(il_header.function_def_table,
                      sizeof(a_function_def_descr) * function_defs_used,
                      f_pch_input);
  }  /* if */
#if DEBUG
  /* Read the allocated_in_region information. */
  fread_with_check(allocated_in_region,
                   sizeof(unsigned long) * mem_regions_used,
                   f_pch_input);
#endif /* DEBUG */
#if USE_MMAP_FOR_MEMORY_REGIONS
  /* Read the blocks of memory used for memory region storage.  See
     write_memory_regions for more information. */
  read_memory_used_for_memory_regions();
#else /* USE_MMAP_FOR_MEMORY_REGIONS */
  /* Read the actual memory regions from the PCH file. */
  {
    a_memory_region_number	n;
    for (n = 0; n < mem_regions_used; ++n) {
      if (mem_region_table[n] != NULL) {
        /* A NULL memory region table entry will not have any entries written
           to the file.  Don't try to read the memory region in this case. */
        read_a_memory_region(n);
      }  /* if */
    }  /* for */
  }
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  db_exit();
}  /* read_memory_regions */


static void write_precompiled_header_file(void)
/*
Create a precompiled header file for the compilation up to the
current point.
*/
{
  a_boolean	is_complete = FALSE;
  long		flag_position;
  open_pch_output_file();
  pch_message(ec_creating_pch, format_file_name(pch_file_name));
#if DEBUG
  if (debug_level >= 3) {
    a_pch_event_ptr	pep;
    fprintf(f_debug, "Events to be recorded in %s:\n", pch_file_name);
    for (pep = pch_event_list_head; pep != NULL; pep = pep->next) {
      db_pch_event(pep);
    }  /* for */
  }  /* if */
#endif /* DEBUG */
  /* Write the string that identifies this file as a precompiled header
     file. */
  fwrite_with_check(pch_id_string, pch_id_string_length, f_pch_output);
  /* Write a FALSE to the file, this will later be changed to TRUE after
     the file has been completely written.  This is done to prevent a
     partially written PCH file from being used. */
  flag_position = ftell(f_pch_output);
  pch_write_value(is_complete);
  /* Current directory name. */
  pch_write_string(get_working_directory());
  /* The directory name associated with the primary source file. */
  pch_write_string(directory_of(primary_source_file_name));
  /* Write the event list that will be used for PCH file matching. */
  write_file_section_id(pfs_cmd_line_events);
  write_pch_events(pch_cmd_line_event_list_head);
  write_file_section_id(pfs_other_events);
  write_pch_events(pch_event_list_head);
  /* Write dependency checking information. */
  /* Include file names and timestamps. */
  write_file_section_id(pfs_include_file_info);
  write_file_timestamps();
  /* Write the memory allocation history information. */
  write_file_section_id(pfs_mem_alloc_info);
  write_mem_alloc_history();
  /* Write the compilation state to be restored. */
  write_file_section_id(pfs_saved_variables);
  write_saved_variables();
  /* Write the memory region information. */
  write_file_section_id(pfs_memory_regions);
  write_memory_regions();
  /* Write the flag that indicates that the PCH file is now complete. */
  fseek_with_check(f_pch_output, flag_position, SEEK_SET);
  is_complete = TRUE;
  pch_write_value(is_complete);
  (void)fclose(f_pch_output);
  f_pch_output = NULL;
  precompiled_header_file_created = TRUE;
}  /* write_precompiled_header_file */


#if DEBUG
static void db_cannot_generate_reason(a_const_char *str)
/*
Display a debugging message explaining why a precompiled header file
can't be generated.
*/
{
  if (debug_level >= 2) {
    fprintf(f_debug, "Cannot generate precompiled header: %s\n", str);
  }  /* if */
}  /* db_cannot_generate_reason */
#else /* !DEBUG */
#define db_cannot_generate_reason(str) /* Nothing */
#endif /* !DEBUG */


static inline a_boolean check_can_generate_pch()
/*
Check to see if generating a PCH is both possible and reasonable.  Return TRUE
if a PCH should be generated; otherwise, return FALSE.
*/
{
  a_boolean result = TRUE;

  if (cannot_create_pch_file) {
    /* Some condition was encountered that makes creation of a precompiled
       header impossible. */
    db_cannot_generate_reason("cannot_create_pch_file is set");
#if !USE_MMAP_FOR_MEMORY_REGIONS
    if (exhausted_preallocated_memory) {
      /* Compute the amount of preallocated memory needed in K (1024) byte
         units. */
      sizeof_t size_needed = (HOST_ALLOCATION_INCREMENT *
                              total_mem_blocks_allocated);

      size_needed = (size_needed / 1024) + 1;

      /* Convert the size needed to a string. */
      a_number_buffer size_string(size_needed, "K");
      str_warning(ec_not_enough_preallocated_memory,
                  size_string.as_temp_characters());
    } else if (large_mem_block_needed) {
      pos_warning(ec_program_entity_too_large_for_pch,
                     &large_mem_block_error_pos);
    }  /* if */
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
    result = FALSE;
  } else if (!next_token_is_top_level_decl_start) {
    /* We are in the middle of a construct.  We must be between top level
       declarations in order to generate a precompiled header. */
    db_cannot_generate_reason("not between top level declarations");
    result = FALSE;
  } else if (num_macro_invocations_in_process != 0) {
    /* We are in the middle of processing a macro invocation.  Don't generate
       a PCH here. */
    db_cannot_generate_reason("macro invocation in process");
    result = FALSE;
  } else if (depth_scope_stack != DEPTH_OF_FILE_SCOPE) {
    /* Don't save the header files if we are not currently at file scope. */
    db_cannot_generate_reason("not at file scope");
    result = FALSE;
  } else if (macro_depth != 0 || pp_if_stack_depth != -1) {
    /* Nor if we are in the midst of a macro definition or a #if construct. */
    db_cannot_generate_reason("in a macro or #if");
    result = FALSE;
  } else if (is_at_least_one_error()) {
    /* Nor if there have been errors. */
    db_cannot_generate_reason("there have been errors");
    result = FALSE;
  } else if (scope_stack[DEPTH_OF_FILE_SCOPE].name_linkage_is_explicit) {
    /* Nor if we are in the middle of a linkage specifier block. */
    db_cannot_generate_reason("in a linkage block");
    result = FALSE;
  } else {
    /* The state justifies creating a precompiled header. */
    check_assertion(curr_il_region_number == FILE_SCOPE_REGION_NUMBER);
    check_assertion(depth_stmt_stack == -1);
    /* Be sure that the overhead in generating a precompiled header is
       justified "quantitatively". */
    if (decl_seq_counter < PCH_DECL_SEQ_THRESHOLD) {
      /* There haven't been enough declarations to justify writing out and
         restoring the header information. */
      db_cannot_generate_reason("too few declarations");
      result = FALSE;
    } else if (il_header.primary_source_file->first_child_file == NULL) {
      /* Don't generate a PCH if there were no included files. */
      db_cannot_generate_reason("no included files");
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* check_can_generate_pch */


void generate_precompiled_header(void)
/*
Processing has reached the "header stop" point.  Check for conditions that
would prevent generation of a precompiled header file, and if none exists,
write out the precompiled header file.
*/
{
  db_enter(2, "generate_precompiled_header");
  check_assertion(header_stop_position_pending);
  if (check_can_generate_pch()) {
    /* Allow for any work needed to prepare data structures that will be
       recorded in the precompiled header file. */
    prepare_to_write_precompiled_header_file();
    /* The next token state should not have been modified by PCH
       preparation.  This is a sign of an underlying issue with the parsing
       state not being properly preserved by invoked functions.  */
    check_assertion(next_token_is_top_level_decl_start);
    /* Perform an additional check following the data structure updates.  This
       additional check is performed in case something happened during PCH
       preparation that now disqualifies the PCH from being created (e.g., an
       error could have occurred during one or more instantiations when
       prepare_to_write_precompiled_header_file was called). */
    if (check_can_generate_pch()) {
      /* Okay -- go ahead and do it. */
      write_precompiled_header_file();
    }  /* if */
  }  /* if */
  db_exit();
}  /* generate_precompiled_header */


void check_create_pch_file_created(void)
/*
If the --create_pch option was specified but no precompiled header file was
written, issue a warning.  The warning is suppressed if any errors were
issued, since a PCH file would not be created in that case anyway.
*/
{
  if (create_precompiled_header && !precompiled_header_file_created) {
    if (!suppress_pch_messages &&
        diagnostic_counters.total.all_error_types() == 0) {
      str_warning(ec_create_pch_file_not_created,
                  format_file_name(pch_output_file_name));
    }  /* if */
  }  /* if */
}  /* check_create_pch_file_created */


void header_stop_no_longer_pending(void)
/*
This routine is called when we are no longer generating information that
may potentially be part of a precompiled header.  We may have just generated
a precompiled header file, or we may have determined that generating one is
not possible.  This routine goes back of the memory regions that have already
been completed and calls check_for_done_with_memory_region so that the IL
file can be written (if needed) and the memory freed (if appropriate).
*/
{
  a_memory_region_number	n;

  db_enter(3, "header_stop_no_longer_pending");
  header_stop_position_pending = FALSE;
  /* Loop through the memory regions.  Skip the front end and file scope
     memory regions. */
  for (n = FILE_SCOPE_REGION_NUMBER + 1;
       n <= highest_used_region_number; ++n) {
    if (mem_region_table[n] == NULL) {
      /* This memory region has already been freed. */
    } else {
      a_scope_ptr		sp;
      sp = il_header.region_scope_entry[n];
      if (sp->depth_in_scope_stack != NO_SCOPE_DEPTH) {
        /* The scope is still active and will be processed when it is
           popped off of the scope stack. */
      } else {
        /* The scope is no longer active. */
        check_for_done_with_memory_region(n);
      }  /* if */
    }  /* if */
  }  /* for */
  free_unused_pch_memory();
  db_exit();
}  /* header_stop_no_longer_pending */


static a_boolean id_string_matches(void)
/*
Make sure that the ID string in the candidate PCH file matches the current
version.  This routine also makes sure that the is_complete flag in the
header has been set indicating that the PCH file was successfully
written.
*/
{
  a_boolean	match = FALSE;
  a_boolean	is_complete = FALSE;

  /* We don't use fread_with_check here because we want to handle
     read errors more gracefully.  After all, we don't yet know
     that is actually a PCH written by this compiler. */
  ensure_pch_buffer_space(pch_id_string_length + 1);
  if (!fread_with_status(pch_buffer, size_t_arg(pch_id_string_length),
                         f_pch_input)) {
    /* The read failed -- the file must contain something unexpected. */
  } else {
    /* The read succeeded, ensure the read string is null terminated, then
       check to see if the ID string matches. */
    pch_buffer[pch_id_string_length] = '\0';
    if (strncmp(pch_buffer, pch_id_string, pch_id_string_length) == 0) {
      match = TRUE;
    }  /* if */
  }  /* if */
  if (!match) {
    mismatch_reason = ec_invalid_pch_file;
  }  /* if */
  if (!fread_with_status(&is_complete, sizeof(is_complete), f_pch_input)) {
    /* The read failed - the file must not be complete. */
    is_complete = FALSE;
  }  /* if */
  if (!is_complete) {
    /* Either a read error or the file is still being written, either way
       the file is incomplete and should not be used. */
    mismatch_reason = ec_pch_file_incomplete;
  }  /* if */
  return match && is_complete;
}  /* id_string_matches */


static a_boolean curr_dir_matches(void)
/*
Return TRUE if the next string in the candidate PCH file matches
the current directory.
*/
{
  char		*ptr;
  a_boolean	result;
  ptr = pch_read_string();
  result = strcmp(ptr, get_working_directory()) == 0;
  if (!result) {
    mismatch_reason = ec_pch_curr_directory_changed;
  }  /* if */
  ptr = pch_read_string();
  if (result) {
    /* If the current directory matches, check the directory associated
       with the primary source file. */
    result = compare_dir_names(ptr,
                               directory_of(primary_source_file_name),
                               /*is_partial_file_name=*/FALSE) == 0;
    if (!result) {
      /* A mismatch of the primary source file is diagnosed as a command
	 line option mismatch. */
      mismatch_reason = ec_pch_cmd_line_option_mismatch;
    }  /* if */
  }  /* if */
  return result;
}  /* curr_dir_matches */


static a_boolean files_have_not_changed(void)
/*
Read the file timestamp information from the PCH input file and make the
modification times match the current values for the files.
*/
{
  a_boolean	match = TRUE;

  check_file_section_id(pfs_include_file_info);
  for (;;) {
    char	*file_name;
    time_t	time_from_file;
    time_t	curr_time;
    /* Read the file name. */
    file_name = pch_read_string();
    /* A null string marks the end of the list. */
    if (*file_name == '\0') break;
    /* Read the modification time. */
    pch_read_value(time_from_file);
    if (!get_file_modification_time(file_name, &curr_time) ||
        time_from_file != curr_time) {
      /* Either the file does not exist or the modification time has changed.
         Note that we require the times to be identical, so even if the include
         file seems to be older than the last one we still consider it to be
         a change. */
      mismatch_reason = ec_pch_header_files_have_changed;
      match = FALSE;
      break;
    }  /* if */
  }  /* for */
  if (!match && automatic_pch_processing) {
    /* At least one of the files has a different modification time.  Remove the
       PCH file. */
    remove_pch_input_file();
  }  /* if */
  return match;
}  /* files_have_not_changed */


static a_boolean cmd_line_events_match(void)
/*
Compare the command line event list of the current file with the
command line events in the candidate PCH file.  Return TRUE if they
all match.
*/
{
  a_pch_event_ptr	pep;
  a_pch_event		event;
  a_boolean		match = TRUE;

  db_enter(4, "cmd_line_events_match");
  check_file_section_id(pfs_cmd_line_events);
  /* Loop through the command line events until one that does not match
     is found. */
  for (pep = pch_cmd_line_event_list_head; pep != NULL; pep = pep->next) {
    /* coverity[tainted_data] */   /* coverity[+taint_source: arg-0] */
    if (!read_pch_event(&event) || !equivalent_pch_events(pep, &event)) {
      /* Either the event list of the file has ended, or the events are
         not equivalent. */
      match = FALSE;
      break;
    }  /* if */
  }  /* for */
  /* There should be no more events in the PCH input file.  If there are
     any, then there is a mismatch. */
  if (match && read_pch_event(&event)) {
    match = FALSE;
  }  /* if */
  if (!match) {
    mismatch_reason = ec_pch_cmd_line_option_mismatch;
  }  /* if */
  db_exit();
  return match;
}  /* cmd_line_events_match */


static a_pch_event_ptr compare_event_lists(void)
/*
Compare the event list of the current file with the list in the candidate
pch file.  Return a pointer to the last matching event.
*/
{
  a_pch_event_ptr	pep;
  a_pch_event_ptr	last_matching_event = NULL;
  a_pch_event_ptr	pos_in_event_list;
  a_pch_event		event;
  a_boolean		match;
  
  db_enter(4, "compare_event_lists");
  check_file_section_id(pfs_other_events);
  /* Clear the match_found flags in the event list for the current file. */
  for (pep = pch_event_list_head; pep != NULL; pep = pep->next) {
    pep->match_found = FALSE;
  }  /* for */
  pos_in_event_list = pch_event_list_head;
  /* Read each of the events from the candidate file until a mismatch is
     found. */
  match = TRUE;
  /* coverity[+taint_source: arg-0] */
  while (read_pch_event(&event)) {
    a_boolean	is_define = FALSE;
    a_boolean	event_matches = FALSE;
#if DEBUG
    if (debug_level >= 4) {
      /* coverity[tainted_data] */
      db_pch_event(&event);
      if (pos_in_event_list == NULL) {
        fprintf(f_debug, "Candidate event list longer than current file\n");
      }  /* if */
    }  /* if */
#endif /* DEBUG */
    if (pos_in_event_list == NULL) {
      /* We've reached the end of the event list for this file but there
         are still more events in the candidate file.  Terminate the
         scan of this candidate file. */
      match = FALSE;
      break;
    }  /* if */
    switch (event.kind) {
      case pchek_command_line:
        unexpected_condition();
        break;
      case pchek_pp_directive:
        is_define = event.variant.ppd_kind == ppd_define;
        break;
      default:
        unexpected_condition();
    }  /* switch */
    if (is_define) {
      /* Look for a matching #define in a sequence of defines. */
      pep = pos_in_event_list;
      while (pep != NULL && pep->kind == pchek_pp_directive &&
             pep->variant.ppd_kind == ppd_define) {
        if (pep->match_found) {
          /* A match has already been found for this define.  It can't
             be used again. */
        } else {
          if (equivalent_pch_events(pep, &event)) {
            /* This event matches on of the defines.  Set the flag that
               indicates that this match has been used and exit the loop. */
            pep->match_found = TRUE;
            event_matches = TRUE;
            break;
          }  /* if */
        }  /* if */
        pep = pep->next;
      }  /* while */
    } else {
      /* This is not a #define, this event must match exactly.  Before
         checking, see if we need to skip over one or more #define events
         that have already been matched. */
      pep = pos_in_event_list;
      while (pep != NULL && pep->kind == pchek_pp_directive &&
             pep->variant.ppd_kind == ppd_define && pep->match_found) {
        pep = pep->next;
      }  /* while */
      /* The events must match exactly. */
      if (pep != NULL && equivalent_pch_events(pep, &event)) {
        event_matches = TRUE;
        last_matching_event = pep;
        pos_in_event_list = pep->next;
      }  /* if */
    }  /* if */
    if (!event_matches) {
      /* If this event doesn't match, then terminate the scan of this
         candidate file. */
      match = FALSE;
      break;
    }  /* if */
  }  /* while */
  /* Skip past any matched #define entries that were not followed
     by some other directive. */
  pep = pos_in_event_list;
  while (pep != NULL && pep->kind == pchek_pp_directive &&
         pep->variant.ppd_kind == ppd_define && pep->match_found) {
    last_matching_event = pep;
    pep = pep->next;
  }  /* while */
  /* If the candidate file doesn't match, return a NULL to the caller. */
  if (!match) {
    last_matching_event = NULL;
    mismatch_reason = ec_pch_file_prefix_mismatch;
  }  /* if */
  db_exit();
  return last_matching_event;
}  /* compare_event_lists */


static a_pch_event_ptr pch_is_applicable(void)
/*
Compares the precompiled header referred to by the f_pch_input
pointer with the current event list to see if it matches.  If
it so, it checks the include file timestamps to see if the files
are up to date.  If the PCH can be used, a pointer to the last
matching event is returned.
*/
{
  a_pch_event_ptr	last_matching_event = NULL;

  db_enter(3, "pch_is_applicable");
  mismatch_reason = ec_no_error;
  if (id_string_matches() && curr_dir_matches()) {
    /* This is a valid precompiled header -- see if the event lists match. */
    if (cmd_line_events_match()) {
      last_matching_event = compare_event_lists();
      if (last_matching_event != NULL) {
        if (files_have_not_changed()) {
          /* The include files have not changed.  We can use this PCH. */
        } else {
          /* This PCH cannot be used because the include files have changed. */
          last_matching_event = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return last_matching_event;
}  /* pch_is_applicable */


static a_boolean find_applicable_pch(void)
/*
Compare the prefix information for this file with the prefix
information for the other precompiled headers in the current
directory.  Return TRUE if an applicable PCH was found.
*/
{
  a_boolean		first;
  a_boolean		first_from_dir;
  a_const_char		*file_name;
  a_source_position	best_result_so_far;
  a_boolean		is_applicable;
  a_boolean		result = FALSE;
  a_boolean		skip_remaining_entries = FALSE;

  db_enter(3, "find_applicable_pch");
#if DEBUG
  if (debug_level >= 4) {
    a_pch_event_ptr	pep;
    fprintf(f_debug, "Event list of this file:\n");
    for (pep = pch_event_list_head; pep != NULL; pep = pep->next) {
      db_pch_event(pep);
    }  /* for */
  }  /* if */
#endif /* DEBUG */
  best_result_so_far = null_source_position;
  for (first = first_from_dir = TRUE;;first = FALSE) {
    a_pch_event_ptr	last_matching_event;
    if (first) {
      /* The first time through the loop, look for a PCH file associated with
         the primary source file.  If this one matches, use it and don't
         search for a better match in the directory. */
      file_name = derived_name(primary_source_file_name, PCH_FILE_SUFFIX);
    } else {
      /* On subsequent iterations of the loop, get a file name from the
         PCH directory.  first_from_dir indicates that this is the first
         call of this routine (i.e., the directory must be opened). */
      file_name = get_file_name_from_dir(first_from_dir, pch_dir_name,
                                         PCH_FILE_SUFFIX,
                                         get_working_directory());
      first_from_dir = FALSE;
    }  /* if */
    /* A NULL pointer indicates there are no more matching file names. */
    if (file_name == NULL) break;
    /* If we've found an optimal PCH file, don't bother looking at more
       entries.  We still read the directory entries so that the
       directory will be closed after all entries have been read. */
    if (skip_remaining_entries) continue;
    /* Append the PCH directory name to the file name. */
    file_name = build_pch_file_name(file_name);
    /* The open routine will also make sure that it is a regular file. */
    if (!open_pch_input_file(file_name)) continue;
    pch_input_file_name = file_name;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Checking %s for applicability\n", file_name);
    }  /* if */
#endif /* DEBUG */
    /* See if this PCH file can be used. */
    last_matching_event = pch_is_applicable();
    if (f_pch_input != NULL) {
      (void)fclose(f_pch_input);
      f_pch_input = NULL;
    }  /* if */
    is_applicable = last_matching_event != NULL;
    if (is_applicable) result = TRUE;
#if DEBUG
    if (db_flag_is_set("pch")) {
      fprintf(f_debug, "PCH file %s, applicable: %s",
              file_name, is_applicable ? "TRUE" : "FALSE");
      if (is_applicable) {
        fprintf(f_debug, ", seq: %lu, column: %lu\n",
                (unsigned long)last_matching_event->position.seq,
                (unsigned long)last_matching_event->position.column);
      } else {
        fprintf(f_debug, "\n");
        if (db_active) {
          pos_st_warning(mismatch_reason, &null_source_position,
                         format_file_name(file_name));
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* DEBUG */
    if (is_applicable) {
      /* See if this precompiled header matches an event later in the source
         file than the previous best. */
      if (cmp_source_positions(last_matching_event->position,
                               best_result_so_far) >= 0) {
        sizeof_t	file_name_length = strlen(file_name);
        best_result_so_far = last_matching_event->position;
        /* Make sure that the file name buffer is large enough to hold
           the new file name. */
        ensure_file_name_buffer_space(file_name_buffer, file_name_length+1);
        (void)strcpy(file_name_buffer.name, file_name);
        /* If this file provides all possible events, don't bother
           inspecting any other files.  Also skip other files if this is
           the PCH file associated with the primary source file. */
        skip_remaining_entries = last_matching_event == pch_event_list_tail;
        /* Exit the loop if the first entry (the one associated with the
           primary source file and not read from the directory) matches. */
        if (first) break;
      }  /* if */
    } else {
      if (verbose_pch_messages) {
        /* Issue a warning that the precompiled header cannot be used. */
        pos_st_warning(mismatch_reason, &null_source_position,
                       format_file_name(file_name));
      }  /* if */
    }  /* if */
  }  /* for */
  if (result) {
    /* Save a copy of the precompiled header file name to be used. */
    pch_input_file_name = 
           (char *)alloc_general((sizeof_t)strlen(file_name_buffer.name) + 1);
    (void)strcpy((char *)pch_input_file_name, file_name_buffer.name);
  }  /* if */
  db_exit();
  return result;
}  /* find_applicable_pch */


static void pch_fixup_part_1(void)
/*
This routine is called after restoring the memory region information
from the PCH file.  It updates the IL header (which is not restored
from the PCH file) to reflect the information loaded from the file.
*/
{
  a_source_file_ptr	orig_sfp;

  /* Reset any necessary memory management state. */
  mem_manage_reset();
  il_reset();
  orig_sfp = il_header_from_pch.primary_source_file;
  /* Make the source file pointer for the file that created the
     precompiled header file the first child file of the new source
     file.  This allows diagnostics that reference lines that come
     from the precompiled header to work properly. */
  il_header.primary_source_file = orig_sfp;
  il_header.primary_scope = il_header_from_pch.primary_scope;
  il_header.file_scope_statements = il_header_from_pch.file_scope_statements;
  il_header.main_routine = il_header_from_pch.main_routine;
  il_header.seq_number_lookup_entries =
                                  il_header_from_pch.seq_number_lookup_entries;
  il_header.num_seq_number_lookup_entries =
                              il_header_from_pch.num_seq_number_lookup_entries;
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  il_header.scope_orphaned_list_headers =
                              il_header_from_pch.scope_orphaned_list_headers;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_MACROS_IN_IL
  il_header.macros = il_header_from_pch.macros;
#endif /* RECORD_MACROS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
  il_header.number_of_external_nonclass_template_entities =
              il_header_from_pch.number_of_external_nonclass_template_entities;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
  il_header.file_scope_dynamic_init_routines =
                           il_header_from_pch.file_scope_dynamic_init_routines;
#if !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
  il_header.thread_local_dynamic_init_routines =
                         il_header_from_pch.thread_local_dynamic_init_routines;
#endif /* !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
  il_header.imported_modules = il_header_from_pch.imported_modules;
  /* Rebuild the trans_unit_for_scope table.  This is done by calling
     take_next_scope_number the appropriate number of times. */
  {
    a_scope_number	number_of_scopes = next_scope_number;
    for (next_scope_number = 0; next_scope_number < number_of_scopes;) {
      (void)take_next_scope_number();
    }  /* for */
  }
  /* There should only be one entry on the stack. */
  check_assertion(curr_translation_unit_stack_entry->next == NULL);
  /* Fix up any fields in the translation unit that point to globals. */
  fix_up_translation_unit(curr_translation_unit);
  /* Clear the stop tokens array that was restored. */
  clear_stop_tokens();
  /* Reset the directory name list.  It points to a list of entries in
     general storage, but the entries point to strings in IL memory.
     dir_name_list_general does not need to be reset because the entries
     and strings are in general memory. */
  dir_name_list_il = NULL;
  /* Reset lexical information to make sure it does not refer to the PCH. */
  lexical_pch_reset();
  /* Reconstruct any data structures that must be rebuilt from the IL
     that was just read. */
  rebuild_structures_on_il_read();
  /* Re-open module files that might have been open. */
  modules_pch_reset();
}  /* pch_fixup_part_1 */


void pch_fixup_part_2(void)
/*
This routine is called when we have reached the point in the current source
file at which we make the transition from information supplied by the PCH
to information generated as a result of this compilation.  This routine
updates the source file pointer in the IL header to reflect the information
loaded from the PCH file.  This needs to be done in both part 1 and 2
of the fixup because a new source file entry for the primary source file
is created when the primary source file is reopened between the two fixups.

*/
{
  a_source_file_ptr	sfp;
  a_source_file_ptr	orig_sfp;

  db_enter(3, "pch_fixup_part_2");
  building_pch_prefix = FALSE;
  next_event_resumes_compilation = FALSE;
  sfp = il_header.primary_source_file;
  orig_sfp = il_header_from_pch.primary_source_file;
  /* Make the source file pointer for the file that created the
     precompiled header file the first child file of the new source
     file.  This allows diagnostics that reference lines that come
     from the precompiled header to work properly. */
  sfp->first_child_file = orig_sfp;
  sfp->last_child_file = orig_sfp;
  /* Mark the source file entry as being the top level file from a PCH
     that is being used. */
  orig_sfp->top_level_file_from_pch = TRUE;
  sfp->first_seq_number = 1;
  /* Set the ending sequence number for what was the primary source
     file when the precompiled header was generated. */
  orig_sfp->last_seq_number = saved_curr_seq_number;
#if CPPCLI_ENABLING_POSSIBLE
  /* Restore the metadata files.  Since the metadata reader doesn't use 
     the memory region mechanism, it cannot be saved using the normal PCH 
     mechanism.  Instead, we will re-import the metadata files.  However, 
     since the top level declarations are in the symbol table already, we
     can skip that step. */
  il_header.cli_metadata_files = il_header_from_pch.cli_metadata_files;
  if (il_header.cli_metadata_files) {
    a_cli_metadata_file_ptr cmfp;
    a_boolean               is_duplicate;

    cmfp = il_header.cli_metadata_files;
    while (cmfp != NULL) {
      a_cpp_cli_import_flag_set import_flags = default_cpp_cli_import_flags;
      if (cmfp->as_friend) {
        import_flags |= (a_cpp_cli_import_flag_set)cpp_cli_as_friend_assembly;
      }  /* if */
      if (wchar_t_is_keyword) {
        import_flags |= (a_cpp_cli_import_flag_set)cpp_cli_wchar_t_is_keyword;
      }  /* if */
      /* Re-register the assemblies that we have imported.  It is important 
         that we import the assemblies in the same order so that they will
         maintain the same assembly index. */
      if (import_metadata_file(cmfp->full_name, import_flags,
                               &is_duplicate) != cmfp->assembly_index) {
        unexpected_condition();
      }  /* if */
      cmfp = cmfp->next;
    }  /* while */
  }  /* if */
#endif /* CPPCLI_ENABLING_POSSIBLE */
  db_exit();
}  /* pch_fixup_part_2 */


static void restore_precompiled_header_information(void)
/*
Reload the compiler state information so that a precompiled header file
may be used.
*/
{
  a_boolean			can_use_pch = TRUE;
  a_pch_event_ptr		last_event_from_pch;

  db_enter(2, "restore_precompiled_header_information");
  if (!automatic_pch_processing) {
    sizeof_t	size;
    char	*name_with_dir;
    /* In non-automatic mode, the input file name will not yet have
       had the PCH directory name added.  Do it now. */
    pch_input_file_name = build_pch_file_name(pch_input_file_name);
    /* Make a copy of the name in general memory. */
    size = strlen(pch_input_file_name) + 1;
    name_with_dir = strcpy((char *)alloc_general(size), pch_input_file_name);
    pch_input_file_name = name_with_dir;
  }  /* if */
  if (open_pch_input_file(pch_input_file_name)) {
    /* Make sure the PCH can still be used.  Also make sure that
       the memory configuration needed by the PCH is compatible with
       what we can allocate. */
    last_event_from_pch = pch_is_applicable();
    if (last_event_from_pch != NULL && read_mem_alloc_history()) {
      /* Everything is OK. */
      pos_of_last_event_from_pch = last_event_from_pch->position;
    } else {
      /* The file is not applicable for some reason. */
      can_use_pch = FALSE;
      if (!automatic_pch_processing) {
        /* Issue a warning that the precompiled header cannot be used. */
        pos_st_warning(mismatch_reason, &null_source_position,
                       format_file_name(pch_input_file_name));
      } else {
        /* Something must have changed since was last read the PCH file.
           Silently suppress use of the PCH file. */
      }  /* if */
    }  /* if */
  }  /* if */
  if (can_use_pch) {
    a_stop_token_stack_entry_ptr saved_curr_stop_token_stack_entry;
    pch_message(ec_using_pch, format_file_name(pch_input_file_name));
    using_a_pch_file = TRUE;
    read_saved_variables();
    /* Clear curr_stop_token_stack_entry so db_enter/db_exit won't use it. */
    saved_curr_stop_token_stack_entry = curr_stop_token_stack_entry;
    curr_stop_token_stack_entry = NULL;
    read_memory_regions();
    if (new_alloc_history != NULL) {
      /* Free the new allocation history information. */
      free_general((a_void_ptr)new_alloc_history,
                   (size_t_arg(new_alloc_history_entries) *
                                                 sizeof(a_mem_alloc_history)));
    }  /* if */
    /* Save the sequence number as of this point.  seq_number_last_read is
       used rather than curr_seq_number because in some cases curr_seq_number
       may not have been updated to reflect the latest seq_number read. */
    saved_curr_seq_number = seq_number_last_read;
    /* Reset the stop token stack value to the one saved above. */
    curr_stop_token_stack_entry = saved_curr_stop_token_stack_entry;
    /* Update the IL header to reflect the information in the PCH file. */
    pch_fixup_part_1();
  }  /* if */
  db_exit();
}  /* restore_precompiled_header_information */


void precompiled_header_processing(void)
/*
This is the main routine responsible for precompiled header processing.
It is called after the primary source file has been opened but before any
tokens have been fetched.  First, the file "prefix" is scanned.  The prefix
is the sequence of preprocessing directives that precede the first declaration
of the file.  When determining whether an existing precompiled header may
be used to replace part of this compilation, the prefix will be compared
with the prefix of candidate precompiled header files to see if they
are applicable.  If this compilation is to generate a precompiled header
file, the prefix information will be saved in the file so that it may
be used as part of the applicability check in subsequent compilations.
*/
{
  a_boolean	applicable_pch_found = FALSE;

  db_enter(2, "precompiled_header_processing");
  /* We have not encountered a condition that would prevent us from
     using a precompiled header. */
  build_prefix_information();
  if (cannot_do_pch_processing) {
    /* Something happened that makes it impossible to generate or
       use a precompiled header. */
  } else {
    if (automatic_pch_processing) {
      applicable_pch_found = find_applicable_pch();
    }  /* if */
    if (use_precompiled_header ||
        (automatic_pch_processing && applicable_pch_found)) {
      restore_precompiled_header_information();
    }  /* if */
    if (automatic_pch_processing) {
      /* If we are not using the PCH file associated with this source file
         (if one exists), then remove it.  It must be obsolete for some
         reason. */
      remove_assoc_pch_file_if_not_being_used();
    }  /* if */
    /* See if we can create a precompiled header file. */
    if (automatic_pch_processing || create_precompiled_header) {
      if (cmp_source_positions(header_stop_source_position,
                               null_source_position) != 0) {
        /* Only generate one if a header stop position was found. */
        if (!using_a_pch_file ||
            (cmp_source_positions(header_stop_source_position,
                                  pos_of_last_event_from_pch) > 0)) {
          /* If we are also using a precompiled header file, make sure that
             the new header stop position is beyond what is being obtained from
             the PCH input file. */
          header_stop_position_pending = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (!header_stop_position_pending) {
      /* We are not attempting to generate a precompiled header.
         Call header_stop_no_longer_pending to process any memory regions
         that may have been restored from a PCH (if one is being used).
         If a PCH is not being used, this will free any specially allocated
         PCH memory. */
      header_stop_no_longer_pending();
    }  /* if */
  }  /* if */
  db_exit();
}  /* precompiled_header_processing */


void register_pch_saved_variables(a_pch_saved_variable array[])
/*
Save the address of a list of variables to be saved when a PCH
file is created and restored when a PCH file is used.
*/
{
  check_assertion_str2
            (num_of_saved_variable_lists < MAX_NUMBER_OF_SAVED_VARIABLE_LISTS,
             "register_pch_saved_variables:",
             "too many saved variable lists");
  saved_variable_array_list[num_of_saved_variable_lists++] = array;
}  /* register_pch_saved_variables */


#if DEBUG
unsigned long db_show_pch_space_used(unsigned long grand_total)
/*
Show space used by the PCH routines.  This is called by
the symbol table space used routine.  The space used by the PCH
routines is reported as part of the symbol table memory used.
*/
{
  unsigned long num;
  unsigned long size;
  unsigned long total;

  db_space_used("PCH events", num_pch_events_allocated, a_pch_event);
  return grand_total;
}  /* db_show_pch_space_used */
#endif /* DEBUG */


void pch_init(void)
/*
Initialize variables used by the precompiled header routines.
*/
{
  db_enter(4, "pch_init");
#if DEBUG
  check_assertion(strcmp(pch_event_kind_names[(int)pchek_last], "last") == 0);
#endif /* DEBUG */
  initialize_pch_id_string();
  cannot_do_pch_processing = FALSE;
  pch_event_list_head = NULL;
  pch_event_list_tail = NULL;
  pch_file_name = NULL;
  f_pch_input = NULL;
  f_pch_output = NULL;
  building_pch_prefix = FALSE;
  header_stop_source_position = null_source_position;
  header_stop_position_pending = FALSE;
  next_event_resumes_compilation = FALSE;
  generate_pch_on_return_to_primary_source_file = FALSE;
  pragma_hdrstop_found = FALSE;
  pos_of_last_event_from_pch = null_source_position;
  using_a_pch_file = FALSE;
  precompiled_header_file_created = FALSE;
  file_name_text_buffer = NULL;
  new_alloc_history_entries = 0;
  new_alloc_history = NULL;
#if DEBUG
  num_pch_events_allocated = 0;
#endif /* DEBUG */
  /* Check for conditions that make it impossible to do precompiled header
     processing. */
  if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) == 0) {
    /* PCH processing must be able to restart the scan of the primary
       source file.  This can't be done with standard input, so we have
       to suppress PCH processing. */
    abandon_pch_processing();
  }  /* if */

  db_exit();
}  /* pch_init */


void pch_one_time_init(void)
/*
Do one-time initialization of variables related to PCH processing.
*/
{
  pch_buffer = (char *)alloc_resizable_buffer(PCH_BUFFER_INITIAL_ALLOCATION);
  size_pch_buffer = PCH_BUFFER_INITIAL_ALLOCATION;
  /* Do initial allocation of the file name buffer. */
  file_name_buffer.name = NULL;
  file_name_buffer.size = 0;
  ensure_file_name_buffer_space(file_name_buffer, 1);
}  /* pch_one_time_init */


void pch_early_init(void)
/*
One time initialization that must take place early on in the front end.
This is done before command line processing.
*/
{
  cannot_create_pch_file = FALSE;
  num_of_saved_variable_lists = 0;
  pch_cmd_line_event_list_head = NULL;
  pch_cmd_line_event_list_tail = NULL;
}  /* pch_early_init */

#if MAKE_FRONT_END_CALLABLE

void pch_late_cleanup(void)
/*
This routine is called at the end of compilation, or if compilation is
terminated prematurely for some reason.  It performs any cleanup operations
required.  In particular, it closes any files that may have been open at
the point at which the compilation was terminated.
*/
{
  close_file_if_open(&f_pch_input);
  close_file_if_open(&f_pch_output);
}  /* pch_late_cleanup */

#endif /* MAKE_FRONT_END_CALLABLE */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

