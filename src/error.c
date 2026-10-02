/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

error.c -- Error reporting routines.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include "err_data.h"
#include "decls.h"
#include "templates.h"
#if !STANDALONE_UTILITY_PROGRAM
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#include "pch.h"
#if RECORD_MACRO_INVOCATIONS
#include "macro.h"
#endif /* RECORD_MACRO_INVOCATIONS */
#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Static variable set when a catastrophe occurs, to catch catastrophe loops.
*/
STATIC_THREAD a_boolean
                catastrophe_has_occurred;

/*
Constants, structures and static variables used to format diagnostic
messages.
*/

#define NORMAL_DIAG_INDENT 0	/* The number of spaces to be indented prior
				   to conventional single message
				   diagnostics. */
#define SOURCE_INDENT 2		/* The number of spaces by which source lines
				   are indented. */
#define INDENT_AMOUNT 10	/* Number of additional spaces at the start of
				   continuation lines. */
#define LIST_DIAG_INDENT 12	/* The number of spaces to be indented prior
				   to each additional message that is part of
				   a multiple message diagnostic, i.e. an
				   error diagnostic followed by a list of
				   entities.  For appearances, this value
				   should be greater than INDENT_AMOUNT. */
#define MACRO_CONTEXT_INDENT 1	/* The number of spaces to indent macro
				   context lines.  For best appearances, this
				   value should be greater than
				   NORMAL_DIAG_INDENT and less than
				   SOURCE_INDENT. */

STATIC_THREAD int
		diagnostic_indent;
			/* Typically all diagnostic messages will
			   begin in the first column of a line and
			   subsequent continuation lines would be
			   indented.  With diagnostics involving
			   multiple messages or entity names, this
			   static variable will be adjusted for the
			   start of each additional message.  See
			   NORMAL_DIAG_INDENT and LIST_DIAG_INDENT
			   above. */
STATIC_THREAD a_memory_region_number
		diag_memory_region;
			/* The memory region to be used for allocating
			   diagnostics.  Normally this is front end
			   memory, but for command-line errors and
			   catastrophic errors, which can occur before
			   IL memory can be allocated, general memory is
			   used.  This is indicated by setting this to
			   NO_MEMORY_REGION_NUMBER. */

				   
/*
Kinds of diagnostic fill-ins that can be represented by a_diag_fill_in.
*/
enum a_diag_fill_in_kind {
  dfk_number,
			/* An integer value. */
  dfk_unsigned_number,
			/* An unsigned integer value. */
  dfk_position,
			/* A source position. */
  dfk_string,
			/* A null-terminated ASCII string. */
  dfk_symbol,
			/* A symbol name (and possibly type). */
  dfk_type,
			/* A type name. */
  dfk_template_args,
			/* A template argument list. */
  dfk_reflection,
			/* A reflection. */
  dfk_module,
			/* A module. */
			/*lint -esym(749,*a_diag_fill_in_kind::dfk_last)*/
  dfk_last		/* Must be last. */
};


/*
Structure used to represent an entity to be used as a fill-in
in a diagnostic message.
*/
typedef struct a_diag_fill_in *a_diag_fill_in_ptr;
typedef struct a_diag_fill_in {
  a_diag_fill_in_kind
		kind;
			/* The kind of entity to be included in the
			   diagnostic. */
#if CHECKING
  a_byte_boolean
		fill_in_used;
			/* Set to TRUE when the fill-in is inserted into
			   the message text.  Used to detect unused
			   fill-ins. */
#endif /* CHECKING */
  a_diag_fill_in_ptr
		next;
			/* Pointer to the next fill-in entry for this
			   message, or NULL if there are no more fill-ins. */
  union {
    /* When kind == dfk_number. */
    a_signed_diag_number
		number;
			/* The numeric value to be inserted. */
    /* When kind == dfk_unsigned_number. */
    an_unsigned_diag_number
		unsigned_number;
			/* The numeric value to be inserted. */
    /* When kind == dfk_position. */
    a_source_position
		position;
			/* The source position to be inserted. */
    /* When kind == dfk_string. */
    struct {
      a_const_char
		*start;	/* The start of the string to be inserted. */
      size_t	len;	/* The length of the string to be inserted. */
    } string;
    /* When kind == dfk_symbol. */
    struct {
      a_symbol_ptr
		ptr;
				/* Pointer to the symbol to be included in the
				   diagnostic. */
      a_scope_depth
		scope_depth;
				/* A scope depth associated with the symbol,
				   or NO_SCOPE_DEPTH.  This is only used
				   for certain context fill-ins. */
      a_byte_boolean
		full_type;	/* True if the symbol should be expanded
				   into an object (type and name). */
      a_byte_boolean
		name_only;	/* True if only the symbol name is needed. */
      a_byte_boolean
		force_function_params;
				/* True if parameters of a function should be
				   listed even if it is not overloaded. */
      a_byte_boolean
		force_template_name_output;
				/* True if the "A<T> [with T=int]" style
 				   of output should be used for classes
				   when distinct template signatures are
				   being used. */
      a_byte_boolean
		decl_pos;	/* True if the declaration position is
				   to be generated. */
      a_byte_boolean
		template_args;	/* True if the template arguments are to be
				   displayed; the scope is specified by the
				   scope_depth field.  Used for
				   sk_function_template only. */
      a_byte_boolean
		trans_unit;	/* True if the translation unit should be
				   displayed under certain circumstances.
				   In the primary translation unit, it is
				   displayed for secondary translation units.
				   In secondary translation units, it is
				   displayed for all translation units. */
    } symbol;
    /* When kind == dfk_type. */
    a_type_ptr
		type;
			/* A pointer to the type entry to be included
			   in the diagnostic. */
    /* When kind == dfk_template_args. */
    a_template_arg_ptr
		template_args;
			/* A pointer to the template argument list to be
			   included in the diagnostic. */
    /* When kind == dfk_reflection. */
    a_reflection_value
		reflection;
			/* The reflection value to be included in the
			   diagnostic. */
    /* When kind == dfk_module. */
    a_module_ptr
		mod;	/* The module to be included in the diagnostic. */
  } variant;
} a_diag_fill_in;


/*
Category codes for the various parts of the processing for multi-line
diagnostics:
*/
enum a_diagnostic_kind {
  dck_primary,			/* A top-level normal diagnostic, also used
				   for "more information" messages, which
				   are treated mostly like primary
				   messages. */
  dck_sub_message,		/* Additional message in a multi-message
				   diagnostic.  This message will typically
				   be indented relative to its associated
				   primary message.  Source file information
				   is not displayed. */
  dck_context,			/* A diagnostic message that specifies
				   error context information. */
  dck_macro_context		/* Similar to dck_context, but for macro
				   context stack trace information. */
};


/*
Structure to hold information resulting from conversion of a source
position to file/line information.
*/
typedef struct a_source_info_for_pos *a_source_info_for_pos_ptr;
typedef struct a_source_info_for_pos {
  a_boolean	at_end_of_source;
			/* TRUE if the location is after the end of the
			   source file. */
  a_source_file_ptr
		source_file;
			/* The source file, or NULL if none is available. */
  a_const_char	*file_name;
			/* The name of the source file, or NULL if
			   none is available. */
  a_line_number	line_number;
			/* The line number within the compilation unit. */
  a_unicode_source_kind
		unicode_source_kind;
			/* The Unicode source kind of the file from which
			   the source line originated. */
} a_source_info_for_pos;


/*
Structure used to build a diagnostic to be issued.  A diagnostic consists of
a primary entry and an optional list of sub-messages used for lists of
information associated with the primary entry.  Primary entries and sub-
messages may have associated fill-in entries.
*/
typedef struct a_diagnostic {
  a_diagnostic_kind
		kind;
			/* Indicates whether this is a primary diagnostic,
			   sub-message, context, etc. */
  a_diagnostic_ptr
		next;
			/* Pointer to the next sub-message if the current
			   entry is for a sub-message.  NULL for primary
			   entries and for the last diagnostic of a
			   sub-message. */
  a_diagnostic_ptr
		primary_diag;
			/* For a subordinate diagnostic or context, this
			   points back to the primary diagnostic.  NULL
			   otherwise. */
  a_diag_list
		sub_msgs;
			/* A list of subordinate diagnostics used for
			   multi-message diagnostics (e.g., for a list
			   of candidate functions on an ambiguous call).
			   The list may be empty. */
  a_diag_list
		context;
			/* A list of context diagnostics.  The list may be
			   empty. */
  a_diag_list
		macro_context;
			/* A list of macro context diagnostics.  The list
			   may be empty. */
  a_diag_list
		more_info;
			/* Pointer to a list of additional information
			   diagnostics to be issued to provide more detail
			   about the cause of the primary message.  The list
			   may be empty. */
  a_translation_unit_ptr
		translation_unit;
			/* Pointer to the translation unit with which this
			   error is associated, or NULL if the diagnostic
			   is not associated with a translation unit
			   (e.g., for a command-line error). */
  a_source_position
		position;
			/* Source location associated with this diagnostic.
			   This is the position passed in on the call of the
			   diagnostic routine. */
  a_source_info_for_pos
		source_info;
			/* File, line, etc. for position. */
  a_source_position
		diag_header_pos;
			/* The position to be displayed in the diagnostic
			   header.  This can be different than "position"
			   when macro positions are being reported. */
  a_source_info_for_pos
		diag_header_source_info;
			/* File, line, etc. for diag_header_pos. */
  an_error_code	error_code;
			/* The error code associated with the diagnostic that
			   being issued. */
  an_error_severity
		severity;
			/* The severity of the diagnostic being issued, or
			   es_none for sub-messages. */
  a_diag_fill_in_ptr
		fill_in_head;
			/* Pointer to a list of diagnostic fill-ins to be
			   inserted into the diagnostic message.  NULL if
			   there are no fill-ins. */
  a_diag_fill_in_ptr
		fill_in_tail;
			/* Pointer to the last element of a list of fill-ins.
			   NULL if there are no fill-ins. */
} a_diagnostic;


void a_diag_list::append(a_diagnostic_ptr  dp)
/*
Append the given diagnostic entry to *this.
*/
{
  if (this->head == NULL) {
    this->head = dp;
  } else {
    this->tail->next = dp;
  }  /* if */
  this->tail = dp;
}  /* a_diag_list::append */


/*
Describe a label fill-in entry.  Label fill-in entries are used to specify
at run time which of two different strings (as specified by error codes)
should be filled in based upon the value of a variable.  Useful in cases
where an error message should differ depending on a particular mode.
*/
typedef struct a_label_fill_in_entry {
  a_const_char  *label;         /* The name of the "label" that is used in
                                   error message text (i.e., %[label]).  This
                                   string is used only for matching purposes
                                   and doesn't necessarily have to appear
                                   in the substituted string. */
  a_boolean     *test;          /* A pointer to a boolean variable whose
                                   value at run time is used to decide which
                                   error code below is substituted. */
  an_error_code true_value,
                false_value;    /* Error codes representing strings to be used
                                   in the "TRUE" and "FALSE" cases.  Note that
                                   these error codes should not themselves
                                   contain any fill-ins. */
} a_label_fill_in_entry;

/* Define the label fill-ins: */
STATIC_THREAD a_label_fill_in_entry label_fill_ins[] = {
  { "managed",       &use_cppcli_fill_ins, ec_managed,      ec_cppcx },
  { "C++/CLI",       &use_cppcli_fill_ins, ec_cppcli,       ec_cppcx_mapping },
  { "default",       &use_cppcli_fill_ins, ec_default,      ec_cli_mapping },
  { "cli::array",    &use_cppcli_fill_ins, ec_cli_array,    ec_platform_array},
  { "C++/CLI array", &use_cppcli_fill_ins, ec_cppcli_array, ec_cppcx_array },
  { "System",        &use_cppcli_fill_ins, ec_system,       ec_platform },
  { "gcnew",         &use_cppcli_fill_ins, ec_gcnew,        ec_ref_new },
  { NULL,            (a_boolean*)NULL,     ec_no_error,     ec_no_error },
};


static a_label_fill_in_entry *get_label_fill_in_entry(a_const_char *label,
                                                      sizeof_t      length)
/*
Return a pointer to the label fill-in entry that matches the specified
label, with the specified length.  An assertion failure occurs if the
label fill-in entry is not found.
*/
{
  a_label_fill_in_entry *lfie;

  for (lfie = label_fill_ins; lfie->label != NULL; lfie++) {
    if (strncmp(lfie->label, label, length) == 0) break;
  }  /* for */
#if CHECKING
  if (lfie->label == NULL) {
#if DEBUG
    char *label_copy = alloc_general((sizeof_t)length+1);
    strncpy(label_copy, label, length);
    label_copy[length] = '\0';
    fprintf(f_debug, "missing fill-in label: %s\n", label_copy);
#endif /* DEBUG */
    unexpected_condition_str(
                            "get_label_fill_in_entry: no label fill-in found");
  }  /* if */
#endif /* CHECKING */
  return lfie;
}  /* get_label_fill_in_entry */

#if EXPENSIVE_CHECKING && !STANDALONE_UTILITY_PROGRAM

static void verify_label_fill_in_entries(void)
/*
Check the label fill-ins of each error message to ensure that the label
fill-in entries are valid.
*/
{
  int          error_code;
  a_const_char *ptr, *end_label;

  for (error_code = 0; error_code < (int)ec_last; error_code++) {
    ptr = error_text((an_error_code)error_code);
    while (ptr != NULL) {
      ptr = mbc_strchr(ptr, '%');
      if (ptr == NULL || *ptr == '\0') {
        break;
      } else {
        ptr++;
        if (*ptr == '\0') {
          break;
        } else if (*ptr == '[') {
          end_label = mbc_strchr(ptr+1, ']');
          check_assertion(end_label != NULL);
          (void)get_label_fill_in_entry(ptr + 1,
                                        (sizeof_t)(end_label - ptr - 1));
          ptr = end_label+1;
        }  /* if */
      }  /* if */
    }  /* while */
  }  /* for */
}  /* verify_label_fill_in_entries */

#endif /* EXPENSIVE_CHECKING && !STANDALONE_UTILITY_PROGRAM */

#if DEBUG
/*
Counts of tables allocated, to track total use of memory.
*/
STATIC_THREAD uint32_t	num_diagnostics_allocated,
		num_diag_fill_ins_allocated;
#endif /* DEBUG */

STATIC_THREAD a_diagnostic_ptr
		avail_diagnostics;
			/* List of diagnostic entries freed and available
			   for reuse. */

STATIC_THREAD a_diag_fill_in_ptr
		avail_diag_fill_ins;
			/* List of diagnostic fill-in entries freed and
			   available for reuse. */

STATIC_THREAD a_text_buffer_ptr
		msg_buffer;
			/* Buffer used to construct an error message. */

STATIC_THREAD a_text_buffer_ptr
		prefix_buffer;
			/* Buffer used to construct the diagnostic prefix
			   information (file, severity, etc). */

STATIC_THREAD a_text_buffer_ptr
		write_diagnostic_buffer;
			/* A text buffer used by write_diagnostic to
			   accumulate the entire contents of a
			   diagnostic message (so it can be written
			   in one atomic operation).  The text buffer
			   is not null-terminated during the
			   construction of the diagnostic. */

STATIC_THREAD int
		diagnostic_line_length;
			/* The maximum length of an error output line.
			   Use to wrap diagnostics. */

/*
Information related to issuing diagnostics (e.g., severity, etc.) for each
error code.
*/
struct an_error_code_entry {
  an_error_severity
                default_severity;
                                /* Default severity for this error code.
                                   Contains values set from the
                                   command-line. */
  /* The members below are re-initialized for each compilation.  (Note: when
     adding a new field, code in error_init must be updated to make that so).
     */
  an_error_severity
                current_severity;
                                /* The current severity associated with an
                                   error code.  Contains values set from the
                                   command-line or by pragmas. */
  a_bit_field   once:1;         /* TRUE if a non-error diagnostic should be
                                   issued only once. */
  a_bit_field   diagnostic_issued:1;
                                /* TRUE if a diagnostic has ever been issued
                                   for this error code. */
  a_bit_field   severity_changed_by_pragma:1;
                                /* TRUE if the severity of this error code has
                                   ever been modified by a pragma during the
                                   current compilation. */
};

STATIC_THREAD an_error_code_entry
                error_codes[(int)ec_last + 1];
                                /* An array of consisting of information
                                   related to how each error code is handled.
                                   */
STATIC_THREAD a_boolean
                error_codes_initialized;
                                /* A flag used to avoid re-initialization of
                                   error_codes when compiling the first
                                   translation unit. */

STATIC_THREAD an_il_to_str_output_control_block
		octl;	/* Output control block for interface to il_to_str
			   routines. */


#if !STANDALONE_UTILITY_PROGRAM
/*
Variables pertaining to a line of source that must be reread for output in
a diagnostic.  Most diagnostics are issued for the current logical source
line.  Occasionally a diagnostic will refer to a source line that is not in
the current logical source line (a line read earlier).  The buffer pointed
to by error_source_line will hold such a source line that has been reread.
*/
STATIC_THREAD char
		*error_source_line;
			/* Characters of the source line being reread for
			   diagnostic generation, ended by both a newline and
			   a null.  Space is dynamically allocated, and its
			   upper bound is given by
			   after_end_of_error_source_line. */
#define ERROR_SOURCE_LINE_INITIAL_ALLOCATION 200
#define ERROR_SOURCE_LINE_INCREMENTAL_ALLOCATION 1000
			/* Initial and incremental allocation sizes for
			   error_source_line.  The initial allocation should be
			   such that almost all cases can be accepted (so that
			   the realloc is hardly ever needed). */
STATIC_THREAD char
		*after_end_of_error_source_line;
			/* Address past the last element of error_source_line,
			   as an aid to checking for overflow, etc.  A variable
			   because error_source_line line can be reallocated
			   larger if needed. */

/*
Data structures and variables used to index into source files to locate
a needed source line for a diagnostic.  For each source file, an index
table is created and updated as the file is read.
*/
#define INITIAL_PHYSICAL_LINE_COUNT_INCREMENT 100
				/* Constant value specifying the starting
				   interval at which physical line positions
				   will be recorded in the error_file_index
				   entry. */
#define NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES 10
				/* Number of physical line indexes maintained
		       		   for each source file. */

typedef struct an_error_file_index *an_error_file_index_ptr;
typedef struct an_error_file_index {
  a_source_file_ptr
		source_file;	/* Pointer to the IL source file entry
				   associated with this physical line index
				   table. */
  an_error_file_index_ptr
		previous;	/* Pointer to the previous an_error_file_index
				   entry in the doubly linked list. */
  an_error_file_index_ptr
		next;		/* Pointer to the next an_error_file_index
				   entry in the doubly linked list. */
  unsigned short
		next_index_entry;
				/* Index of the next available entry in the
				   line_number and file_position arrays. */
  a_line_number line_number[NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES];
				/* Physical line number of the file that
				   begins at the associated file position. */
  long		file_position[NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES];
				/* File position that is the beginning of
				   the associated physical line and used 
				   to fseek() into the file. */
  long		physical_line_count_increment;
				/* Value specifying the interval at which
				   physical line positions will be recorded
				   in the error_file_index entry.  Initially
				   set to INITIAL_PHYSICAL_LINE_COUNT_INCREMENT
				   but may be updated later if the file is
				   large. */
} an_error_file_index;

STATIC_THREAD an_error_file_index_ptr
		head_of_file_index_list;
				/* Pointer to the beginning of the list of
				   an_error_file_index entries.  Initialized
				   to NULL by error_init(). */
STATIC_THREAD an_error_file_index_ptr
		tail_of_file_index_list;
				/* Pointer to the tail of the list of
				   an_error_file_index entries.  Initialized
				   to NULL by error_init(). */

/*
Type used to track diagnostic (e.g., pk_diag*) pragmas.  Such pragmas are
"immediate" and for each one that is encountered (except pk_diag_once),
an element of this type is entered into the pragma_diag_list linked list.
Elements in that list are kept sorted by source location (i.e.,
src/column).  Note that a_source_location is not used here because it
potentially contains additional fields and there is a desire to keep this
structure small.
*/
struct a_pragma_diag_elem {
  inline a_pragma_diag_elem(a_pragma_kind     kind,
                            a_source_position *pos);
  inline a_pragma_diag_elem(a_pragma_kind     kind,
                            a_source_position *pos,
                            int               error_number);
  struct a_pragma_diag_elem
                *next;  /* Next entry on the list. */
  struct a_pragma_diag_elem
                *prev;  /* Previous entry on the list. */
  a_simple_source_position
                spos;   /* Source position of the pragma. */
  a_pragma_kind kind;   /* The pk_diag* pragma kind. */
  a_bit_field   is_pop:1;
                        /* TRUE if this corresponds to a "diagnostic pop". */
  union {
    /* When kind == pk_diagnostic: */
    a_pragma_diag_elem
                *corresponding_push;
                        /* When is_pop is TRUE, this entry is a pointer to
                           the corresponding "push".  For "pop" entries that
                           have no corresponding "push" entry, NULL is used. */
    /* When kind != pk_diagnostic: */
    int         error_number;
                        /* The error number specified by the pragma. */
  } variant;
};

namespace detail {

template<>
struct Is_trivially_copyable_edg_impl<a_pragma_diag_elem> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<a_pragma_diag_elem> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

}  /* namespace detail */

inline a_pragma_diag_elem::a_pragma_diag_elem(a_pragma_kind     _kind,
                                              a_source_position *pos)
/*
Constructor for pk_diagnostic entries.
*/
  : kind(_kind)
  , is_pop(FALSE)
{
  check_assertion(kind == pk_diagnostic);
  this->next = NULL;
  this->prev = NULL;
  this->spos.seq = pos->seq;
  this->spos.column = pos->column;
  this->variant.corresponding_push = NULL;
}  /* a_pragma_diag_elem::a_pragma_diag_elem */


inline a_pragma_diag_elem::a_pragma_diag_elem(a_pragma_kind     _kind,
                                              a_source_position *pos,
                                              int               error_number)
/*
Constructor for pk_diag_* (and pk_none) entries.
*/
  : kind(_kind)
  , is_pop(FALSE)
{
  check_assertion(kind != pk_diagnostic);
  this->next = NULL;
  this->prev = NULL;
  this->spos.seq = pos->seq;
  this->spos.column = pos->column;
  this->variant.error_number = error_number;
}  /* a_pragma_diag_elem::a_pragma_diag_elem */


static inline a_boolean operator==(const a_pragma_diag_elem &e1,
                                   const a_pragma_diag_elem &e2)
/*
Return TRUE if e1 and e2 have the same values for all applicable non-static
data members.
*/
{
  return (e1.kind == e2.kind &&
          e1.spos.seq == e2.spos.seq &&
          e1.spos.column == e2.spos.column &&
          (e1.kind == pk_diagnostic ?
           (!e1.is_pop ||
            e1.variant.corresponding_push == e2.variant.corresponding_push) :
           e1.variant.error_number == e2.variant.error_number));
}  /* operator== */


STATIC_THREAD a_pragma_diag_elem
                *pragma_diag_list, *pragma_diag_tail;
                        /* Head and tail pointers to a linked list of
                           a_pragma_diag_elem entries that represent all of the
                           "diagnostic" pragmas in the compilation.  The list
                           is ordered by source location (which generally
                           occurs naturally except for the case of _Pragmas in
                           instantiations which must be inserted at the proper
                           location). */
STATIC_THREAD Dyn_array<a_pragma_diag_elem*>
                *pragma_push_stack;
                        /* A stack that keeps track of "diagnostic push"
                           pragmas so that they can later be matched with their
                           corresponding "diagnostic pop". */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
Structure used to maintain a record of diagnostic messages that have been
issued during prototype instantiations.  This is used to suppress
diagnostics during actual instantiations if the diagnostic was issued
during the prototype instantiation.
*/
typedef struct a_recorded_diagnostic *a_recorded_diagnostic_ptr;
typedef struct a_recorded_diagnostic {
  a_recorded_diagnostic_ptr
		next;
			/* Pointer to the next entry in the current bucket.
			   NULL for the last entry in the bucket. */
  an_error_code	error_code;
			/* The error code of the diagnostic. */
  an_error_severity
		severity;
			/* The severity of the message. */
  a_source_position
		error_pos;
			/* The position associated with the message. */
  a_scope_number
		scope_of_prev_check;
			/* The scope number at the point of the last
			   suppressed diagnostic. */
  unsigned int	number_of_times_suppressed;
			/* Number of times the message was suppressed in
			   scope_of_prev_check. */
} a_recorded_diagnostic;


#define RECORDED_DIAG_TABLE_SIZE 983
			/* The number of buckets in the recorded diagnostic
			   table.  This number should be prime. */

STATIC_THREAD a_recorded_diagnostic_ptr
		recorded_diagnostic_table[RECORDED_DIAG_TABLE_SIZE];
			/* The top level array used for the hash table used
			   to find previously issued diagnostics. */


static inline void annotate_diagnostic(a_text_buffer_ptr            buffer,
                                       a_diagnostic_annotation_kind kind)
/*
Emit, in buffer, the "escape" sequence (DIAG_ANNOTATION_INDICATOR) followed
by the kind of annotation, but only if annotations are enabled.
*/
{
  if (annotate_diagnostics) {
    add_char_to_text_buffer(buffer, DIAG_ANNOTATION_INDICATOR);
    add_char_to_text_buffer(buffer, (char)kind);
  }  /* if */
}  /* annotate_diagnostic */


static void set_sgr_string(a_diagnostic_annotation_kind kind,
                           a_const_char                 *string)
/*
Find (in sgr_string_for_colored_diagnostics) the string if it exists and save a
pointer to it (and its length) in sgr_map[kind].  If the string doesn't exist,
set the pointer to NULL and the length to 0.
*/
{
  a_const_char *ptr = strstr(sgr_string_for_colored_diagnostics, string);
  sgr_map[(int)kind].ptr = NULL;
  sgr_map[(int)kind].length = 0;
  if (ptr != NULL) {
    size_t len = strlen(string);
    if (ptr[len] == '=') {
      ptr += len+1;
      a_const_char *end_ptr = strchr(ptr, ':');
      len = (end_ptr == NULL) ? strlen(ptr) : (size_t)(end_ptr - ptr);
      sgr_map[(int)kind].ptr = ptr;
      sgr_map[(int)kind].length = len;
      /* Make sure only valid characters are in the SGR string. */
      for (a_const_char *p = ptr; p < ptr+len ; p++) {
        if (*p != ';' && !isdigit((unsigned char)*p)) {
          sgr_map[(int)kind].ptr = NULL;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* set_sgr_string */


void init_colorization(void)
/*
Perform initialization to enable colorization of diagnostics.
*/
{
  check_assertion(colorize_diagnostics);
  if (getenv("NOCOLOR") != NULL) {
    /* Do not colorize if the NOCOLOR environment variable is set. */
    colorize_diagnostics = FALSE;
  } else if (!terminal_is_color_capable()) {
    /* Only colorize if errors are being sent to a terminal. */
    colorize_diagnostics = FALSE;
  }  /* if */
  if (colorize_diagnostics) {
    if ((sgr_string_for_colored_diagnostics = getenv("EDG_COLORS")) != NULL) {
      /* Use EDG_COLORS environment variable if set. */
    } else if ((sgr_string_for_colored_diagnostics = getenv("GCC_COLORS")) !=
                                                                        NULL) {
      /* Use select portions of GCC_COLORS environment variable if set. */
    } else {
      /* Use default settings. */
      sgr_string_for_colored_diagnostics = DEFAULT_EDG_COLORS;
    }  /* if */
    /* Find relevant substrings in the sgr_string, if any. */
    set_sgr_string(da_error, "error");
    set_sgr_string(da_warning, "warning");
    set_sgr_string(da_note, "note");
    set_sgr_string(da_locus, "locus");
    set_sgr_string(da_quote, "quote");
    set_sgr_string(da_range1, "range1");
  }  /* if */
  annotate_diagnostics = colorize_diagnostics;
}  /* init_colorization */


static void add_colorization_characters(a_diagnostic_annotation_kind kind)
/*
The diagnostic currently being written (in write_diagnostic_buffer) has
the specified annotation kind at this point in the buffer.  Add the appropriate
SGR code (if there is one) to the buffer.
*/
{
  check_assertion(kind < da_last);
  an_sgr_string *sgrp = &sgr_map[(int)kind];
  if (kind == da_reset || sgrp->ptr != NULL) {
    add_char_to_text_buffer(write_diagnostic_buffer, '\033');
    add_char_to_text_buffer(write_diagnostic_buffer, '[');
    if (kind == da_reset) {
      add_char_to_text_buffer(write_diagnostic_buffer, '0');
    } else {
      add_string_with_length_to_text_buffer(write_diagnostic_buffer,
                                            sgrp->ptr, sgrp->length);
    }  /* if */
    add_char_to_text_buffer(write_diagnostic_buffer, 'm');
  }  /* if */
}  /* add_colorization_characters */


static char *diag_copy_string(a_const_char           *string)
/*
Make a copy of the specified string to diag_memory_region and return
a pointer to the copy.  When STANDALONE_UTILITY_PROGRAM is TRUE, general
memory is always used.
*/
{
#if STANDALONE_UTILITY_PROGRAM
  sizeof_t	length;
  char		*new_string;

  length = strlen(string);
  new_string = alloc_general(length+1);
  (void)strcpy(new_string, string);
  return new_string;
#else /* !STANDALONE_UTILITY_PROGRAM */
  return copy_string_to_region(diag_memory_region, string);
#endif /* STANDALONE_UTILITY_PROGRAM */
}  /* diag_copy_string */


a_const_char *error_text(an_error_code error_code)
/*
Return a pointer to the error text for the message identified by the given
error code.  Note that if this routine is modified to get the text from
some other source (e.g., a file) it should be copied into memory in such
a way that it will not be invalidated by subsequent calls of this routine.
This routine is called numerous times for each diagnostic, so it is important
that it be fast.  If a file is used, all of the messages should probably
be read into memory so that an array of strings can still be used here.
*/
{
  check_assertion_str2((int)error_code < (int)ec_last,
                       "error_text: ", "invalid error code");
  return ((a_const_char *)message_text[(int)error_code]);
}  /* error_text */


static void clear_source_info_for_pos(a_source_info_for_pos_ptr	sifpp)
/*
Initialize the fields of the source-info-for-pos entry sifpp.
*/
{
  sifpp->at_end_of_source = FALSE;
  sifpp->source_file = NULL;
  sifpp->file_name = NULL;
  sifpp->line_number = 0;
  sifpp->unicode_source_kind = usk_none;
}  /* clear_source_info_for_pos */


static a_diagnostic_ptr alloc_diagnostic(void)
/*
Allocate and initialize a diagnostic entry.  Reuse an entry from a list
of freed entries if possible.
*/
{
  a_diagnostic_ptr	dp;

  /* Don't attempt reuse if the allocation is from general memory. */
  if (avail_diagnostics != NULL &&
      diag_memory_region != NO_MEMORY_REGION_NUMBER) {
    /* Reuse an existing entry. */
    dp = avail_diagnostics;
    avail_diagnostics = avail_diagnostics->next;
  } else {
    /* Allocate a new entry. */
    dp = alloc_general_or_in_region_of_type(diag_memory_region, a_diagnostic);
#if DEBUG
   num_diagnostics_allocated++;
#endif /* DEBUG */
  }  /* if */
  dp->kind = dck_primary;
  dp->next    = NULL;
  dp->primary_diag = NULL;
  clear_diag_list(&dp->sub_msgs);
  clear_diag_list(&dp->context);
  clear_diag_list(&dp->macro_context);
  clear_diag_list(&dp->more_info);
  dp->translation_unit = NULL;
  dp->position = null_source_position;
  clear_source_info_for_pos(&dp->source_info);
  dp->diag_header_pos = null_source_position;
  clear_source_info_for_pos(&dp->diag_header_source_info);
  dp->error_code = ec_no_error;
  dp->severity = es_none;
  dp->fill_in_head = NULL;
  dp->fill_in_tail = NULL;
  return dp;
}  /* alloc_diagnostic */


/* Forward declaration. */
static void free_diagnostic(a_diagnostic_ptr dp);


static void free_diag_list_elements(a_diag_list_ptr	dlp)
/*
Free the list of diagnostics pointed to by the diagnostic list entry dip.
*/
{
  a_diagnostic_ptr	dp;
  a_diagnostic_ptr	next_dp;

  for (dp = dlp->head; dp != NULL; dp = next_dp) {
    next_dp = dp->next;
    free_diagnostic(dp);
  }  /* for */
}  /* free_diag_list_elements */


static void free_diagnostic(a_diagnostic_ptr dp)
/*
Add the diagnostic entry specified by "dp" to the list of diagnostic entries
that are available for reuse.
*/
{
  /* Don't put entries from general memory on the available lists. */
  if (diag_memory_region != NO_MEMORY_REGION_NUMBER) {
    /* If there are sub-lists, free them now. */
    free_diag_list_elements(&dp->sub_msgs);
    free_diag_list_elements(&dp->context);
    free_diag_list_elements(&dp->macro_context);
    free_diag_list_elements(&dp->more_info);
    /* If the diagnostic has fill-ins, add them to the available list. */
    if (dp->fill_in_head != NULL) {
      dp->fill_in_tail->next = avail_diag_fill_ins;
      avail_diag_fill_ins = dp->fill_in_head;
    }  /* if */
    dp->next = avail_diagnostics;
    avail_diagnostics = dp;
  }  /* if */
}  /* free_diagnostic */


static a_diag_fill_in_ptr alloc_diag_fill_in(a_diag_fill_in_kind kind)
/*
Allocate and initialize a diagnostic fill-in entry.  The kind of entry
is specified by "kind".
*/
{
  a_diag_fill_in_ptr	dfip;

  /* Don't attempt reuse if the allocation is from general memory. */
  if (avail_diag_fill_ins != NULL &&
      diag_memory_region != NO_MEMORY_REGION_NUMBER) {
    /* Reuse an existing entry. */
    dfip = avail_diag_fill_ins;
    avail_diag_fill_ins = avail_diag_fill_ins->next;
  } else {
    /* Allocate a new entry. */
    dfip = alloc_general_or_in_region_of_type(diag_memory_region,
                                              a_diag_fill_in);
#if DEBUG
   num_diag_fill_ins_allocated++;
#endif /* DEBUG */
  }  /* if */
  dfip->next = NULL;
  dfip->kind = kind;
#if CHECKING
  dfip->fill_in_used = FALSE;
#endif /* CHECKING */
  switch (kind) {
    case dfk_number:
      dfip->variant.number = 0;
      break;
    case dfk_unsigned_number:
      dfip->variant.unsigned_number = 0;
      break;
    case dfk_position:
      dfip->variant.position = null_source_position;
      break;
    case dfk_string:
      dfip->variant.string.start = NULL;
      dfip->variant.string.len = 0;
      break;
    case dfk_symbol:
      dfip->variant.symbol.ptr = NULL;
      dfip->variant.symbol.scope_depth = NO_SCOPE_DEPTH;
      dfip->variant.symbol.full_type = FALSE;
      dfip->variant.symbol.name_only = FALSE;
      dfip->variant.symbol.force_function_params = FALSE;
      dfip->variant.symbol.force_template_name_output = FALSE;
      dfip->variant.symbol.decl_pos = FALSE;
      dfip->variant.symbol.template_args = FALSE;
      dfip->variant.symbol.trans_unit = FALSE;
      break;
    case dfk_type:
      dfip->variant.type = NULL;
      break;
    case dfk_template_args:
      dfip->variant.template_args = NULL;
      break;
    case dfk_reflection:
      dfip->variant.reflection.entity.kind = iek_none;
      dfip->variant.reflection.entity.ptr = NULL;
      break;
    case dfk_module:
      dfip->variant.mod = NULL;
      break;
    case dfk_last:
      unexpected_condition();
    default_is_unexpected();
  }  /* switch */
  return dfip;
}  /* alloc_diag_fill_in */


static void set_up_output_control_block(void)
/*
Set up the output control block octl so that the il_to_str routines can be
called.
*/
{
  /* Allocate the diagnostic buffer if this is our first time. */
  if (msg_buffer == NULL) {
    msg_buffer = alloc_text_buffer(1024);
    prefix_buffer = alloc_text_buffer(128);
  }  /* if */
  reset_text_buffer(msg_buffer);
  reset_text_buffer(prefix_buffer);
  clear_il_to_str_output_control_block(&octl);
  octl.output_str = put_str_into_text_buffer;
  octl.text_buffer = msg_buffer;
  octl.for_diagnostics = TRUE;
  octl.gen_pcc_code = (C_dialect == C_dialect_pcc);
  octl.max_template_arg_depth = MAX_ERROR_TEMPLATE_ARG_DEPTH;
  /* For diagnostics in C99 mode we want to see "_Bool" rather "bool" or the
     type underlying _Bool. */
  octl.render_c99_bool = c99_mode;
  octl.keep_template_typedefs = display_template_typedefs_in_diagnostics;
}  /* set_up_output_control_block */


static void form_type_summary(a_diag_fill_in_ptr	dfip)
/*
Format a string that represents the type pointed to by dfip into the message
buffer.
*/
{
#if STANDALONE_UTILITY_PROGRAM
  add_string_to_text_buffer(msg_buffer, "\"");
  form_type(dfip->variant.type, &octl);
  add_string_to_text_buffer(msg_buffer, "\"");
#else /* !STANDALONE_UTILITY_PROGRAM */
  Value_saver<a_byte_boolean>
                keep_template_typedefs_saved(&octl.keep_template_typedefs);

  octl.keep_template_typedefs = TRUE;
  add_string_to_text_buffer(msg_buffer, "\"");

  auto type_str_fn = [dfip]() { form_type(dfip->variant.type, &octl); };
  a_text_buffer_view type_str = capture_buffer_append(msg_buffer, type_str_fn);
  add_string_to_text_buffer(msg_buffer, "\"");
  /* If the type contains an aliased type, include a string showing the aliased
     type, e.g.:

       "a_type" (aka "unsigned int")
               ^^^^^^^^^^^^^^^^^^^^^

     If the output is incomplete, do not even try to strip aliases.  The
     additional output is more likely to be unhelpful adding even more noise to
     the output.  Additionally, in the case of deep template argument lists,
     this prevents excessive time spent inside of is_or_contains_typedef_type.
  */
  if (!octl.incomplete_output &&
      is_or_contains_typedef_type(dfip->variant.type)) {
    Value_saver<a_byte_boolean>
                suppress_typedef_names_saved(&octl.suppress_typedef_names);
    sizeof_t    original_start = msg_buffer->size;

    octl.suppress_typedef_names = TRUE;
    octl.keep_template_typedefs = FALSE;
    add_string_to_text_buffer(msg_buffer, " (aka \"");

    auto aka_type_str_fn = [dfip]() { form_type(dfip->variant.type, &octl); };
    a_text_buffer_view aka_type_str = capture_buffer_append(msg_buffer,
                                                            aka_type_str_fn);
    add_string_to_text_buffer(msg_buffer, "\")");
    if (type_str == aka_type_str) {
      /* In some cases the front end encounters a typedef type that's
         invisible in the printed type, e.g.:

           typedef void (*func)(int*);
           template <func f> struct my_struct {};

           // If my_struct<&some_function> appears in a diagnostic, the above
           // function type results in a non-visible typedef usage being
           // detected by is_or_contains_typedef_type.
           my_struct<&some_function> *a1;

           // The front end associates "x" with the struct, so if this appears
           // in a diagnostic it will appear with a redundant name.
           typedef struct { } x;

         this results in the front end printing out a redundant "aka".  These
         cases are non-trivial to detect via traversal and aggressively culling
         them can result in valid cases not being handled correctly.  Thus, we
         perform this post-processing check for the redundancy in the
         "rendered" type name and (when the names match) revert the msg_buffer
         to its prior state.

         Similarly note, when a typedef is in a namespace that inhibits typedef
         dealiasing, is_or_contains_typedef_type will still return TRUE (to
         keep the said function straight forward).  form_type handles the
         actual process of ignoring the normal rules for ignoring dealiasing.
         Thus, if the produced "aka" type name is equivalent to the original
         type name, this code handles the associated cleanup.
      */
      truncate_text_buffer_to(msg_buffer, original_start);
    }  /* if */
  }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
}  /* form_type_summary */


static void form_template_arg_list(a_diag_fill_in_ptr	dfip)
/*
Format a string that represents the template argument list pointed to by
dfip into the message buffer.
*/
{
  a_template_arg_ptr  tap;
  a_boolean           first = TRUE;

  add_string_to_text_buffer(msg_buffer, "\"<");
  for (tap = dfip->variant.template_args; tap != NULL; tap = tap->next) {
    /* Don't emit anything for a start-of-pack entry. */
    if (is_start_of_pack_expansion_templ_arg(tap)) continue;
    /* Add a "," separator after the first argument. */
    if (first) {
      first = FALSE;
    } else {
      add_string_to_text_buffer(msg_buffer, ", ");
    }  /* if */
    form_a_template_arg(tap, &octl);
  }  /* for */
  add_string_to_text_buffer(msg_buffer, ">\"");
}  /* form_template_arg_list */

#if !STANDALONE_UTILITY_PROGRAM

static void form_source_position(a_source_position   *pos,
                                 a_diagnostic_ptr    dp,
			         a_const_char	     *prefix_string,
			         a_const_char	     *suffix_string,
                                 a_const_char	     *end_of_source_string)
/*
Add the source position pos to msg_buffer.  The generated format is
one of:

        <prefix_string>at line xxx of "file name"<suffix string>
        <prefix_string>in "file name"<suffix string>

depending on whether or not the line number is zero (e.g., for assemblies).

If the file is stdin or the file name is identical to that of the error
position of the diagnostic message being composed, the file name is not
emitted as part of this declaration position.  dp is the diagnostic
being formed and is used to eliminate redundant file names in a diagnostic.
*/
{
  a_const_char          *file_name, *full_name, *diag_file_name;
  a_line_number         line_number;
  a_boolean             at_end_of_source;
  a_diagnostic_ptr      primary_dp;
  a_source_position_ptr error_pos;
  a_source_file_ptr     sfp;

  primary_dp = dp->primary_diag != NULL ? dp->primary_diag : dp;
  error_pos = &primary_dp->diag_header_pos;
  diag_file_name = "";
  if (error_pos->seq != 0) {
    /* Have a valid diagnostic source position. */
    (void)conv_seq_to_file_and_line(error_pos->seq, &diag_file_name,
                                    &full_name, &line_number,
                                    &at_end_of_source);
    if (at_end_of_source) diag_file_name = "";
  }  /* if */
  if (pos->seq != 0) {
    /* Have a valid source position. */
    sfp = conv_seq_to_file_and_line(pos->seq, &file_name, &full_name,
                                    &line_number, &at_end_of_source);
    annotate_diagnostic(msg_buffer, da_range1);
    if (at_end_of_source) {
      add_string_to_text_buffer(msg_buffer, end_of_source_string);
    } else {
      a_boolean file_name_needed = strcmp(file_name, diag_file_name) != 0 &&
                                   strcmp(file_name, FILE_NAME_FOR_STDIN) != 0;
      add_string_to_text_buffer(msg_buffer, prefix_string);
      if (line_number == SP_LINE_UNKNOWN) {
        /* No line number (e.g., C++/CLI assemblies). */
        if (file_name_needed) {
          f_add_string_to_text_buffer(msg_buffer, error_text(ec_in));
        }  /* if */
      } else {
        Small_string<20> num_buffer(line_number);

        /* Emit the line number. */
        f_add_string_to_text_buffer(msg_buffer, error_text(ec_at_line));
        add_string_to_text_buffer(msg_buffer, num_buffer.as_temp_characters());
      }  /* if */
      /* Add the file name if needed. */
      if (file_name_needed) {
        char *formatted_file_name;
        if (line_number != SP_LINE_UNKNOWN) {
          f_add_string_to_text_buffer(msg_buffer, error_text(ec_of));
        }  /* if */
        if (sfp != NULL) {
          formatted_file_name =
                    format_source_file_name(sfp, /*use_name_as_written=*/FALSE,
                                            /*quote_file_name=*/TRUE);
        } else {
          add_char_to_text_buffer(msg_buffer, '"');
          formatted_file_name = format_file_name(file_name);
          add_char_to_text_buffer(msg_buffer, '"');
        }  /* if */
        add_string_to_text_buffer(msg_buffer, formatted_file_name);
      }  /* if */
      add_string_to_text_buffer(msg_buffer, suffix_string);
    }  /* if */
    annotate_diagnostic(msg_buffer, da_reset);
  }  /* if */
}  /* form_source_position */


static void form_function_template_param_list(a_symbol_ptr sym)
/*
Display the parameter list of the function template specified by sym.
*/
{
  check_assertion(sym->kind == sk_function_template);
  a_template_symbol_supplement_ptr tssp = sym->variant.template_info;

  /* Only display the parameter list if the function template makes use
     of template parameters that are not part of the function signature. */
  if (tssp->variant.function.template_param_not_in_function_type) {
    a_template_decl_info_ptr decl_info = tssp->variant.function.decl_cache->
                                                                     decl_info;
    a_template_param_ptr     tpp = decl_info->parameters;

    if (tpp != NULL) {
      add_string_to_text_buffer(msg_buffer, "<");
      for (; tpp != NULL; tpp = tpp->next) {
        add_string_to_text_buffer(msg_buffer,
                                  tpp->param_symbol->header->identifier);
        if (tpp->is_pack) add_string_to_text_buffer(msg_buffer, "...");
        if (tpp->next != NULL) add_string_to_text_buffer(msg_buffer, ",");
      }  /* for */
      add_string_to_text_buffer(msg_buffer, ">");
    }  /* if */
  }  /* if */
}  /* form_function_template_param_list */


static a_symbol_ptr prototype_symbol_for_class(a_type_ptr class_type)
/*
If class_type is an instance of a class template or nested class of a class
template, but not a specialization, return a pointer to the prototype
instantiation of the associated template.
*/
{
  a_symbol_ptr			result_sym = NULL;
  a_class_symbol_supplement_ptr	cssp;

  if (class_type->variant.class_struct_union.is_template_class &&
      !class_type->variant.class_struct_union.is_specialized) {
    cssp = symbol_supplement_for_class(class_type);
    /* For instances of a class template, display the symbol for
       the prototype instantiation.  This may not be set yet for
       an incomplete template class.  If it is not set, get the
       prototype instantiation from the class template. */
    result_sym = cssp->corresp_prototype_sym;
    if (result_sym == NULL) {
      check_assertion(cssp->class_template != NULL);
      result_sym = prototype_template_of(cssp->class_template)->
                                variant.template_info->
                                variant.class_template.prototype_instantiation;
    }  /* if */
  }  /* if */
  return result_sym;
}  /* prototype_symbol_for_class */


static void form_template_arg_info(a_symbol_ptr			sym,
				   a_symbol_ptr			template_sym,
				   a_boolean			*p_any_args)
/*
sym is an entity that was to be displayed in an error fill-in.  If
template_sym is non-NULL (when called at the top level) it points to a
template of which sym is an instance, and which was actually displayed
instead of sym.  This routine displays the value of any template
parameters of the entity referred to by sym and its parent classes.
If template_sym is NULL, sym is a nontemplate entity, but may be a
member (such as a nonstatic data member) of a template class.  This
routine displays the value of any template parameters referred to by
the parent classes of sym.  *p_any_args is set to TRUE if an argument
has been displayed.  It is a NULL pointer when called at the outermost
level.
*/
{
  a_boolean				sym_is_specialized = FALSE;
  a_template_arg_ptr			tap = NULL;
  a_template_param_ptr			tpp;
  a_template_decl_info_ptr		decl_info = NULL;
  a_boolean				*any_args;
  a_boolean				any_args_value;

  if (p_any_args == NULL) {
    /* This is a top-level call.  Set any_args to point to a local variable
       that will contain the status. */
    any_args = &any_args_value;
    any_args_value = FALSE;
  } else {
    /* This is not a top-level call, use the pointer passed by the caller. */
    any_args = p_any_args;
  }  /* if */
  if (template_sym != NULL) {
    /* Get the template argument list and the is_specialized flag for the
       entity. */
    a_template_symbol_supplement_ptr	tssp;
    tssp = template_supplement_for_symbol(template_sym);
    switch (sym->kind) {
      case sk_class_or_struct_tag:
      case sk_union_tag:
        {
          a_type_ptr tp = sym->variant.class_struct_union.type;

          sym_is_specialized = tp->variant.class_struct_union.is_specialized;
          tap = templ_arg_list_for_class(tp);
          decl_info = tssp->cache->decl_info;
        }
        break;
      case sk_routine:
      case sk_member_function:
        {
          a_routine_ptr	rp = sym->variant.routine.ptr;
          if (!rp->is_prototype_instantiation) {
            tap = rp->template_arg_list;
            decl_info = tssp->variant.function.decl_cache->decl_info;
          }  /* if */
        }
        break;
      case sk_static_data_member:
      case sk_variable:
        {
          a_variable_ptr vp = variable_for_symbol(sym);

          decl_info = tssp->variant.variable.decl_cache->decl_info;
          tap = vp->template_info->template_arg_list;
        }
        break;
      default:
        unexpected_condition_str2("form_template_arg_info:",
                                  "unexpected symbol kind");
        break;
    }  /* switch */
  }  /* if */
  if (sym_is_specialized) {
    /* There is no information to be displayed for fully specialized
       instances. */
  } else {
    /* Display the template argument information for the parent classes,
       then display the template arguments for this entity. */
    if (sym->is_class_member) {
      a_symbol_ptr		parent_sym;
      a_symbol_ptr		parent_template_sym;
      parent_sym = symbol_for(sym_parent_class(sym));
      if (template_sym != NULL) {
        check_assertion(template_sym->is_class_member);
        parent_template_sym = symbol_for(sym_parent_class(template_sym));
      } else {
        /* No template symbol was provided by the caller.  If the parent class
           is a template instance, use the prototype instantiation as the
           template symbol. */
        parent_template_sym =
                            prototype_symbol_for_class(sym_parent_class(sym));
      }  /* if */
      /* Only display the parent information if the parent class of the
         template is a prototype instantiation.  This suppresses the
         template argument information for the levels at which the
         template has been specialized.  This is also suppressed if the
         parent_sym is a prototype instantiation, to avoid output like
         "[with T=T]" */
      if (parent_template_sym != NULL &&
          is_prototype_instantiation_or_cli_generic(parent_template_sym) &&
          !is_prototype_instantiation_symbol(parent_sym)) {
        form_template_arg_info(parent_sym, parent_template_sym, any_args);
      }  /* if */
    }  /* if */
    if (tap != NULL) {
      /* Display the argument list for this entity.  Don't use the standard
         template argument traversal routines because they hide the existence
         of parameter packs and we want parameter packs to be displayed in
         the diagnostic. */
      check_assertion(decl_info != NULL);
      tpp = decl_info->parameters;
      for (; tpp != NULL; tpp = tpp->next) {
        /* Display "parameter=value". */
        if (!*any_args) {
          /* This is the first argument displayed -- add the introduction
             string to the message. */
          add_string_to_text_buffer(msg_buffer, " [");
          f_add_string_to_text_buffer(msg_buffer, error_text(ec_with));
          *any_args = TRUE;
        } else {
          /* This is not the first argument -- add "," separator. */
          add_string_to_text_buffer(msg_buffer, ", ");
        }  /* if */
        add_string_to_text_buffer(msg_buffer,
                                  tpp->param_symbol->header->identifier);
        add_string_to_text_buffer(msg_buffer, "=");
        check_assertion(tap != NULL);
        if (is_start_of_pack_expansion_templ_arg(tap)) {
          a_boolean   first_pack_arg = TRUE;
          /* This template parameter maps to zero or more template arguments
             in a parameter pack. */
          add_string_to_text_buffer(msg_buffer, "<");
          for (tap = tap->next;
               (tap != NULL &&
                !is_start_of_pack_expansion_templ_arg(tap) &&
                tap->is_pack_element);
               tap = tap->next) {
            /* Add a "," separator after the first argument. */
            if (first_pack_arg) {
              first_pack_arg = FALSE;
            } else {
              add_string_to_text_buffer(msg_buffer, ", ");
            }  /* if */
            form_a_template_arg(tap, &octl);
          }  /* for */
          add_string_to_text_buffer(msg_buffer, ">");
        } else {
          form_a_template_arg(tap, &octl);
          tap = tap->next;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  if (p_any_args == NULL && *any_args) {
    /* One or more arguments were displayed.  Terminate the list. */
    add_string_to_text_buffer(msg_buffer, "]");
  }  /* if */
}  /* form_template_arg_info */


static void form_symbol_name_for_error(a_symbol_ptr		sym)
/*
Display the name of the symbol specified by sym.  If sym is a member
of a template class, or a class nested within a template class, display
the prototype instantiation name instead of the normal parent class name.
The template argument values will then be added to the diagnostic later.
*/
{
  a_symbol_ptr	prototype_sym = NULL;
  a_type_ptr	parent_class = NULL;

  if (distinct_template_signatures && sym->is_class_member) {
     /* If the symbol is a member of a class, get the parent class and
        determine whether it is a template instance. */
     parent_class = sym_parent_class(sym);
     prototype_sym = prototype_symbol_for_class(parent_class);
  }  /* if */
  if (prototype_sym != NULL) {
    form_symbol_name(prototype_sym, &octl);
    add_string_to_text_buffer(msg_buffer, "::");
    form_optionally_qualified_symbol_name(sym, &octl,
                                          /*suppress_qualifier=*/TRUE);
  } else {
    form_symbol_name(sym, &octl);
  }  /* if */
}  /* form_symbol_name_for_error */


static void form_symbol_summary(a_diagnostic_ptr	dp,
				a_diag_fill_in_ptr	dfip)
/*
Format the symbol described by the fill-in entry dfip for the diagnostic
specified by dp.
*/
{
  a_type_ptr	type = NULL;
  a_routine_ptr	routine = NULL;	
  a_symbol_ptr  fund_sym;	/* Pointer to the fundamental symbol of
				   argument sym if it exists.  Otherwise,
				   the value will be that of sym. */
  an_error_code			entity_kind;
  a_boolean			force_function_params = FALSE;
  a_boolean			force_return_type = FALSE;
  a_boolean			return_type_needed = TRUE;
  a_boolean			is_declaration_like = FALSE;
  a_symbol_ptr			corresp_template_sym = NULL;
  a_template_instance_ptr	tip = NULL;
  a_symbol_ptr			sym_to_display = NULL;
  a_boolean			saved_keep_template_typedefs;
  a_symbol_ptr			sym = dfip->variant.symbol.ptr;
  a_boolean  			saved_render_auto_deduction_typerefs =
				          octl.render_auto_deduction_typerefs;

  /* Determine the fundamental symbol of this symbol. */
  fund_sym = fundamental_symbol_of(sym);
  switch (fund_sym->kind) {
    case sk_keyword:
      /* The name of a keyword is extracted from the token_names array, and
         is handled differently from other symbols. */
      if (! dfip->variant.symbol.name_only) {
        f_add_string_to_text_buffer(msg_buffer, error_text(ec_keyword));
        add_string_to_text_buffer(msg_buffer, " ");
      }  /* if */
      add_string_to_text_buffer(msg_buffer, "\"");
      /* Use the name in the header. */
      add_string_to_text_buffer(msg_buffer, sym->header->identifier);
      break;
    case sk_macro:
      entity_kind = ec_macro;
      goto symbol_name;
    case sk_label:
      entity_kind = ec_label;
      goto symbol_name;
    case sk_type:
      if (fund_sym->variant.type.ptr->kind == (a_type_kind)tk_template_param) {
        entity_kind = ec_template_parameter;
      } else {
        entity_kind = ec_type;
      }  /* if */
      goto symbol_name;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      { if (C_dialect == C_dialect_cplusplus &&
            is_prototype_instantiation_symbol(fund_sym)) {
          /* This is a symbol for a prototype instantiation of a class
             template.  It is preferable to display "class template X<T>"
             instead of "class X<T>", so fall through to code for
             sk_class_template. */
        } else {
          if (fund_sym->kind == (a_symbol_kind)sk_union_tag) {
            entity_kind = ec_union;
          } else if (C_mode()) {
            entity_kind = ec_struct;
#if MICROSOFT_EXTENSIONS_ALLOWED
          } else if (fund_sym->variant.class_struct_union.type
                             ->variant.class_struct_union.is_interface) {
            entity_kind = ec_microsoft_interface;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          } else {
            entity_kind = ec_class;
          }  /* if */
          if (distinct_template_signatures &&
              dfip->variant.symbol.force_template_name_output) {
            /* If the class is a template instance, get the corresponding
               prototype symbol for display purposes. */
            corresp_template_sym = prototype_symbol_for_class(
                                    fund_sym->variant.class_struct_union.type);
          }  /* if */
          goto symbol_name;
        }  /* if */
      }
      FALLTHROUGH
    case sk_class_template:
      if (sym->is_template_param) {
        entity_kind = ec_template_template_parameter;
      } else if (sym->kind == (a_symbol_kind)sk_class_template &&
                 sym->variant.template_info->is_nonreal_member) {
        entity_kind = ec_template;
      } else if (sym->kind == (a_symbol_kind)sk_class_template &&
                 sym->variant.template_info->
                                    variant.class_template.is_alias_template) {
        entity_kind = ec_alias_template;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (sym->kind == (a_symbol_kind)sk_class_template &&
                 sym->variant.template_info->is_generic) {
        entity_kind = ec_generic_class;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        entity_kind = ec_class_template;
      }  /* if */
      goto symbol_name;
    case sk_enum_tag:
      entity_kind = ec_enum;
      goto symbol_name;
    case sk_parameter:
      entity_kind = ec_parameter;
      type = fund_sym->variant.param_id->type;
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_variable:
      type = fund_sym->variant.variable.ptr->type;
      if (fund_sym->variant.variable.ptr->is_parameter) {
        entity_kind = ec_parameter;
      } else if (fund_sym->variant.variable.ptr->is_handler_param) {
        entity_kind = ec_handler_parameter;
      } else if (fund_sym->variant.variable.ptr->is_struct_binding) {
        entity_kind = ec_struct_binding;
        type = NULL;
      } else {
        tip = fund_sym->variant.variable.instance_ptr;
        if (tip != NULL && distinct_template_signatures &&
            dfip->variant.symbol.force_template_name_output) {
          /* When a template variable is displayed, it is done by displaying
             the variable from the prototype instantiation and the template
             arguments used for each template parameter list.  Get the template
             symbol to be displayed. */
          a_variable_ptr  vp = variable_for_symbol(fund_sym);
          a_template_ptr  var_templ = vp->template_info->assoc_template;
          corresp_template_sym = symbol_for(var_templ);
          corresp_template_sym = prototype_template_of(corresp_template_sym);
          vp = variable_for_symbol(corresp_template_sym);
          type = vp->type;
        }  /* if */
        entity_kind = ec_variable;
      }  /* if */
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_extern_variable:
      type = fund_sym->variant.extern_symbol_descr->type;
      entity_kind = ec_variable;
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_constant:
      type = fund_sym->variant.constant->type;
      if (type->kind == (a_type_kind)tk_template_param &&
          type->variant.template_param.kind ==
                   (a_template_param_type_kind)tptk_unknown) {
        /* If the constant is a proxy or nonreal class member then use an
           entity kind of "nontype" to indicate that this is a generic
           nontype entity and not actually a constant. */
        entity_kind = ec_nontype;
      } else {
        entity_kind = ec_constant;
      }  /* if */
      goto symbol_name;
    case sk_routine:
    case sk_member_function:
      tip = fund_sym->variant.routine.instance_ptr;
      if (tip != NULL && distinct_template_signatures) {
        /* When a template function, or member function of a template class
           is displayed, it is done by displaying the template itself
           (or the member function of the prototype instantiation) and the
           template arguments used for each template parameter list.  Get
           the template symbol to be displayed. */
        /* Get the type of the template itself. */
        a_symbol_ptr				template_sym;
        a_template_symbol_supplement_ptr	tssp;
        template_sym = fund_sym->variant.routine.instance_ptr->template_sym;
        if (is_template_symbol(template_sym)) {
          template_sym = prototype_template_of(template_sym);
        }  /* if */
        tssp = template_supplement_for_symbol(template_sym);
        corresp_template_sym = template_sym;
        if (template_sym->kind == (a_symbol_kind)sk_function_template) {
          type = skip_typerefs(tssp->variant.function.routine->type);
        } else {
          type = routine_symbol_type(template_sym);
        }  /* if */
      } else {
        type = routine_symbol_type(fund_sym);
      }  /* if */
      routine = fund_sym->variant.routine.ptr;
      if (special_kind_is(routine, sfk_deduction_guide)) {
        entity_kind = ec_deduction_guide;
      } else {
        entity_kind = ec_function;
      }  /* if */
      is_declaration_like = TRUE;
      if (fund_sym->variant.routine.ptr->is_lambda_body) {
        /* For lambdas, display the closure type name instead, which is of
           the form "lambda [](params)->some_type" and therefore already
           reflects the function's signature. */
        sym_to_display = symbol_for(fund_sym->parent.class_type);
        routine = NULL;
        type = NULL;
      }  /* if */
      goto symbol_name;
    case sk_extern_routine:
      type = fund_sym->variant.extern_symbol_descr->type;
      routine = fund_sym->variant.extern_symbol_descr->variant.routine.ptr;
      entity_kind = ec_function;
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_overloaded_function:
      entity_kind = ec_overloaded_function;
      /* There is no specific type information available; this entity cannot
         be expressed as a declaration. */
      goto symbol_name;
    case sk_static_data_member:
      tip = fund_sym->variant.static_data_member.instance_ptr;
      type = fund_sym->variant.static_data_member.variable->type;
      entity_kind = ec_member;
      is_declaration_like = TRUE;
      if (tip != NULL && distinct_template_signatures) {
        /* When a static data member of a template class is displayed, it
           is done by displaying the static data member of the prototype
           instantiation and the template arguments used for each template
           parameter list.  Get the template symbol to be displayed. */
        corresp_template_sym = tip->template_sym;
      }  /* if */
      goto symbol_name;
    case sk_field:
      type = fund_sym->variant.field.ptr->type;
      if (C_dialect == C_dialect_cplusplus) {
        entity_kind = ec_member;
        is_declaration_like = TRUE;
      } else {
        entity_kind = ec_field;
      }  /* if */
      goto symbol_name;
    case sk_namespace:
      entity_kind = ec_namespace;
      goto symbol_name;
#if NAMED_REGISTERS_ALLOWED
    case sk_named_register:
      entity_kind = ec_named_register;
      goto symbol_name;
#endif /* NAMED_REGISTERS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
    case sk_named_address_space:
      entity_kind = ec_named_address_space;
      goto symbol_name;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case sk_property_set:
      if (fund_sym->variant.property_info->properties != NULL &&
          fund_sym->variant.property_info->properties->next != NULL) {
        entity_kind = ec_property_set;
      } else {
        entity_kind = ec_property;
      }  /* if */
      goto symbol_name;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case sk_undefined:
      entity_kind = ec_no_error;
      goto symbol_name;
    case sk_variable_template:
      entity_kind = ec_variable_template;
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_concept_template:
      entity_kind = ec_concept_template;
      is_declaration_like = TRUE;
      goto symbol_name;
    case sk_function_template:
      entity_kind = ec_function_template;
      routine = fund_sym->variant.template_info->variant.function.routine;
      type = skip_typerefs(routine->type);
      /* Function templates can differ only by return type, so include the
         return type when also displaying the parameter types. */
      force_return_type = dfip->variant.symbol.force_function_params;
      if (routine->is_lambda_body) {
        /* For lambdas, display the closure type name instead, which is of
           the form "lambda [](params)->some_type" and therefore already
           reflects the function's signature. */
        sym_to_display = symbol_for(fund_sym->parent.class_type);
        routine = NULL;
        type = NULL;
      }  /* if */
symbol_name:
      /* Add the entity kind if not specified as name only or full type for
         a declaration-like entity. */
      if (type == NULL) is_declaration_like = FALSE;
      if (! dfip->variant.symbol.name_only &&
          ! (dfip->variant.symbol.full_type && is_declaration_like) ) {
        if (entity_kind != ec_no_error) {
          f_add_string_to_text_buffer(msg_buffer, error_text(entity_kind));
          add_string_to_text_buffer(msg_buffer, " ");
        }  /* if */
      }  /* if */
      /* Add color if so configured. */
      annotate_diagnostic(msg_buffer, da_quote);
      /* Add the beginning double quote. */
      add_string_to_text_buffer(msg_buffer, "\"");
      /* Check for special kinds of routines. */
      if (routine != NULL) {
        if (is_constructor_symbol(fund_sym) ||
#if MICROSOFT_EXTENSIONS_ALLOWED
            is_static_constructor_symbol(fund_sym) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            is_destructor_symbol(fund_sym) ||
            routine->special_kind == (a_special_function_kind)sfk_conversion) {
          /* The return type is not listed for constructors, destructors, and
             conversion functions. */
          return_type_needed = FALSE;
        }  /* if */
      }  /* if */
      /* Determine the symbol to be displayed.  For class members and
         ambiguous symbols always use the original symbol.  Otherwise, use
         the fundamental symbol. */
      if (sym_to_display == NULL) {
        a_boolean	use_orig_sym;
        use_orig_sym = sym->is_class_member || sym->ambiguous;
        if (corresp_template_sym == NULL) {
          sym_to_display = use_orig_sym ? sym : fund_sym;
        } else {
          sym_to_display = corresp_template_sym;
        }  /* if */
      }
      /* When outputting a type based on a template definition, don't remove
         member typedefs. */
      saved_keep_template_typedefs = octl.keep_template_typedefs;
      octl.keep_template_typedefs = corresp_template_sym != NULL;
      if (routine != NULL) {
        if (dfip->variant.symbol.force_function_params) {
          /* "%np" was specified for this fill-in. */
          force_function_params = TRUE;
        } else if (fund_sym->kind != (a_symbol_kind)sk_function_template &&
                   routine->template_arg_list != NULL) {
          /* Always put out the parameter list for functions that are
             instances of function templates. */
          force_function_params = TRUE;
        } else if (special_kind_is(routine, sfk_deduction_guide)) {
          /* Always put out the param list for deduction guides. */
          force_function_params = TRUE;
        } else if (sym_to_display->overload_set_member) {
          /* Always put out the param list for overloaded functions. */
          force_function_params = TRUE;
        } else if (sym_to_display == corresp_template_sym &&
                   fund_sym->overload_set_member) {
          /* If a function is member of an instance of a template class, it
             may be that the template symbol is not part of an overload
             set and the member of the instantiated class is (e.g.,
             constructors and assignment operators, since the implicitly
             declared forms may bring an overload set into existence).  Treat
             this case as overloaded, too. */
          force_function_params = TRUE;
        }  /* if */
      }  /* if */
      /* Put out the first part of the type if needed, but not for
         constructors, destructors, and conversion functions (the return type
         is not listed for those). */
      if (type != NULL &&
          (dfip->variant.symbol.full_type || force_return_type) &&
          (routine == NULL || return_type_needed)) {
        form_type_first_part_simple(type,
                                    /*under_lhs_declarator=*/FALSE,
                                    /*need_trailing_space=*/TRUE,
                                    &octl);
      }  /* if */
      /* Put out the name, including the class qualifier if any.
         If a template symbol is being displayed, use the normal
         form_symbol_name routine.  If a template symbol is not
         being displayed, use a special routine that displays the
         corresponding prototype template in place of the actual
         parent class. */
      if (corresp_template_sym == NULL) {
        form_symbol_name_for_error(sym_to_display);
      } else {
        form_symbol_name(sym_to_display, &octl);
      }  /* if */
      /* Put out the second part of the type if needed.  Don't put it
         out in name-only mode.  Do put it out in full-type mode, or
         if function parameters should be listed. */
      if (type != NULL &&
          !dfip->variant.symbol.name_only &&
          (dfip->variant.symbol.full_type || force_function_params) ) {
        if (sym_to_display->kind == (a_symbol_kind)sk_function_template &&
            distinct_template_signatures) {
          /* If the symbol being displayed is a function template, display
             the template's parameter list in the form of an explicit
             function template parameter list. */
          form_function_template_param_list(sym_to_display);
        }  /* if */
        if ((routine != NULL && !return_type_needed) ||
            (!dfip->variant.symbol.full_type && !force_return_type)) {
          /* For constructors, destructors, and conversion functions,
             put out the function type but not the return type. */
          form_function_declarator(type, &octl);
        } else {
          /* Normal case -- put out the complete second part of the type. */
          form_type_second_part_simple(type, /*under_lhs_declarator=*/FALSE,
                                       &octl);
        }  /* if */
      }  /* if */
      /* Restore the typedef removal state before outputting the template
         argument values. */
      octl.keep_template_typedefs = saved_keep_template_typedefs;
      if (distinct_template_signatures) {
        /* Display the template argument information, if any. */
        a_symbol_ptr	templ_arg_sym;
        /* If a template name was displayed above, use the fundamental
           symbol to produce the appropriate template arguments.  Otherwise,
           use the symbol whose name was output above. */
        if (corresp_template_sym != NULL) {
          templ_arg_sym = originator_symbol_of(fund_sym);
        } else {
          templ_arg_sym = sym_to_display;
        }  /* if */
        form_template_arg_info(templ_arg_sym, corresp_template_sym, 
                               (a_boolean*)NULL);
      }  /* if */
      break;
    case sk_projection:
    case sk_namespace_projection:
      /* Cannot have a projection of a projection symbol.  This is an
         error. */
      unexpected_condition_str2("form_symbol_summary:",
                                "projection of projection kind");
      break;
    default:
      unexpected_condition_str("form_symbol_summary: unsupported symbol kind");
  }  /* switch */
  /* Add the closing double quote mark. */
  add_string_to_text_buffer(msg_buffer, "\"");
  annotate_diagnostic(msg_buffer, da_reset);
  /* If the name is based on template arguments, add a message to that
     effect. */
  if (dfip->variant.symbol.template_args) {
    a_scope_stack_entry_ptr  ssep;
    a_template_arg_ptr       tap;
    a_scope_depth            depth;
    depth = dfip->variant.symbol.scope_depth;
    check_assertion(depth != NO_SCOPE_DEPTH);
    check_assertion(sym->kind == (a_symbol_kind)sk_function_template ||
                    sym->kind == (a_symbol_kind)sk_class_template ||
                    sym->kind == (a_symbol_kind)sk_concept_template ||
                    sym->kind == (a_symbol_kind)sk_variable_template);
    ssep = &scope_stack[depth];
    begin_template_arg_list_traversal_simple(ssep->template_arg_list, &tap);
    if (tap != NULL) {
      add_string_to_text_buffer(msg_buffer, " ");
      advance_to_next_template_arg_simple(&tap);
      if (tap != NULL) {
        f_add_string_to_text_buffer(
                       msg_buffer, error_text(ec_based_on_template_arguments));
      } else {
        f_add_string_to_text_buffer(
                        msg_buffer, error_text(ec_based_on_template_argument));
      }  /* if */
      add_string_to_text_buffer(msg_buffer, " ");
      form_template_args(ssep->template_arg_list, /*tpp=*/NULL, &octl);
    }  /* if */
  }  /* if */
  /* Add the declaration position as requested. */
  if (dfip->variant.symbol.decl_pos) {
    if (routine != NULL && routine->compiler_generated &&
        !(routine->is_lambda_body && sym->decl_position.seq != 0)) {
      /* For compiler-generated routines referred to by name in diagnostics a
         declaration position is usually not helpful (e.g., for a generated
         constructor it ends up being the position of the class).  Lambda
         expressions, however, look sufficiently like the operator() they
         generate that the expression position can be reported as the position
         at which the corresponding operator() is declared. */
      f_add_string_to_text_buffer(msg_buffer,
                                  error_text(ec_declared_implicitly));
    } else {
      form_source_position(&sym->decl_position, dp,
                           error_text(ec_declared_prefix), ")",
                           error_text(ec_at_end_of_source));
    }  /* if */
  }  /* if */
  /* Add the translation unit associated with the symbol. */
  if (dfip->variant.symbol.trans_unit) {
    a_boolean			add_trans_unit = FALSE;
    a_translation_unit_ptr	tup = NULL;
    if (sym->decl_scope == NO_SCOPE_NUMBER) {
      /* No translation unit is available for a symbol with no scope. */
    } else {
      tup = trans_unit_for_symbol(sym);
      if (is_primary_translation_unit) {
        /* In primary translation units,  only include the translation unit
           for symbols from secondary translation units. */
        add_trans_unit = tup != curr_translation_unit;
      } else {
        /* In secondary translation units, include the translation unit for
           all symbols. */
        add_trans_unit = TRUE;
      }  /* if */
    }  /* if */
    if (add_trans_unit) {
      /* Add the translation unit to the message. */
      char *formatted_file_name;
      add_string_to_text_buffer(msg_buffer, " (");
      /* This message code includes the explanatory text (e.g.,
         "from translation unit"). */
      f_add_string_to_text_buffer(msg_buffer, error_text(ec_from_trans_unit));
      formatted_file_name =
                         format_source_file_name(tup->source_file,
                                                 /*use_name_as_written=*/FALSE,
                                                 /*quote_file_name=*/TRUE);
      add_string_to_text_buffer(msg_buffer, formatted_file_name);
      add_string_to_text_buffer(msg_buffer, ")");
    }  /* if */
  }  /* if */
  octl.render_auto_deduction_typerefs = saved_render_auto_deduction_typerefs;
}  /* form_symbol_summary */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static a_boolean message_has_fill_in(an_error_code error_code)
/*
Return TRUE if the message text for the indicated error code has at least
one error fill-in.
*/
{
  a_const_char *p;

  p = mbc_strchr(error_text(error_code), '%');
  /* Ignore "%%"; it's not a real fill-in. */
  while (p != NULL && p[1] == '%') p = mbc_strchr(p+2, '%');
  return (p != NULL);
}  /* message_has_fill_in */

#if !STANDALONE_UTILITY_PROGRAM

void clear_file_index_list(void)
/*
Clear the file index list.
*/
{
  head_of_file_index_list = NULL;
  tail_of_file_index_list = NULL;
}  /* clear_file_index_list */

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if !STANDALONE_UTILITY_PROGRAM

a_line_number initialize_file_index(a_source_file_ptr src_file)
/*
Create and initialize an_error_file_index entry for the source file IL
entry specified by src_file.  The newly created an_error_file_index is placed
at the head of the list pointed to by the static variable 
head_of_file_index_list.  Return the physical line number at which the first
index entry should be made.
*/
{
  an_error_file_index_ptr new_file;

  /* Allocate the error file index entry in the front end memory region. */
  new_file = (an_error_file_index_ptr)alloc_fe(sizeof(an_error_file_index));
  new_file->source_file = src_file;
  new_file->next_index_entry = 0;
  new_file->physical_line_count_increment =
					 INITIAL_PHYSICAL_LINE_COUNT_INCREMENT;
  /* Add the new entry at the head of the list. */
  new_file->previous = NULL;
  if ((new_file->next = head_of_file_index_list) == NULL) {
    /* This is for the primary source file. */
    tail_of_file_index_list = new_file;
  } else {
    /* Update the backward link. */
    head_of_file_index_list->previous = new_file;
  }  /* if */
  head_of_file_index_list = new_file;
  /* Return the physical line number that the first index entry should be
     made. */
  return INITIAL_PHYSICAL_LINE_COUNT_INCREMENT;
}  /* initialize_file_index */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

a_line_number update_file_index(a_source_file_ptr src_file,
                                a_line_number     physical_line,
                                long              file_pos)
/*
Add the file index specified by the physical_line and corresponding file
position (file_pos) to the an_error_file_index entry for the file represented
by src_file.  The physical line number that the next index entry should be
made is returned.
*/
{
  an_error_file_index_ptr curr_file;
  size_t                  idx, mid_idx;
  unsigned long           spacing;

  /* Typically the current file being read will be at the head of the list
     of an_error_file_index entries. */
  if ((curr_file = head_of_file_index_list)->source_file != src_file) {
    /* Since files are added to the beginning of this list as they are opened
       and the current file is not at the head of the list, the files
       included by the current file (precede the current file on the list)
       are no longer open.  Move these entries in front of the current file
       to the end of the list.  It is better to have any performance cost of
       file lookup associated with diagnostic generation, if needed. */
    for(curr_file = curr_file->next;
        curr_file != NULL;
        curr_file = curr_file->next) {
      /* Check if this is the entry needed. */
      if (curr_file->source_file == src_file) break;
    }  /* for */
#if CHECKING
    if (curr_file == NULL) {
#if DEBUG
      if (debug_level > 0) {
        (void)fprintf(f_debug,
                      "Missing file index entry for source file \"%s\"\n", 
                      src_file->full_name);
      }  /* if */
#endif /* DEBUG */
      internal_error("update_file_index: missing file index entry");
    }  /* if */
#endif /* CHECKING */
    /* Move the current file to the top of the list. */
    tail_of_file_index_list->next = head_of_file_index_list;
    head_of_file_index_list->previous = tail_of_file_index_list;
    /* Now have a circular list; cut where needed. */
    (tail_of_file_index_list = curr_file->previous)->next = NULL;
    (head_of_file_index_list = curr_file)->previous = NULL;
  }  /* if */
  /* Make the new entry. */
  idx = curr_file->next_index_entry;
  if (idx < NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES) {
    /* Add the file index information into the next available table entry. */
    curr_file->line_number[idx] = physical_line;
    curr_file->file_position[idx] = file_pos;
    curr_file->next_index_entry++;
  } else {
    /* The index table is full.  Reorganize the table by compressing the
       first half of the table to cover a wider range of lines.  The line
       number gaps will be some integer multiple of
       INITIAL_PHYSICAL_LINE_COUNT_INCREMENT. The value of
       curr_file->physical_line_count_increment is incremented by
       INITIAL_PHYSICAL_LINE_COUNT_INCREMENT each time the table is
       filled. */
    mid_idx = NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES / 2;
    spacing = curr_file->line_number[mid_idx] / (a_line_number)mid_idx;
    /* Eliminate the first entry in the top half of the table that is less
       than the value should be at the desired interval. */
    for (idx = 0; idx < mid_idx; idx++ ) {
      if (curr_file->line_number[idx] < ((idx + 1) * spacing)) {
        /* Eliminate this entry simply by breaking the loop. */
        break;
      }  /* if */
    }  /* for */
    /* Now shift all remaining entries in the table. */
    for (/* start with the index to be eliminated */;
         idx < NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES - 1;
         idx++ ) {
      curr_file->line_number[idx] = curr_file->line_number[idx + 1];
      curr_file->file_position[idx] = curr_file->file_position[idx + 1];
    }  /* for */
    /* Add the new entry at the end of the table. */
    curr_file->line_number[NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES - 1] =
                                                               physical_line;
    curr_file->file_position[NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES - 1] =
                                                              file_pos;
    /* Increments the physical line count increment value so that the
       additional entries that are added to the list will be spaced further
       apart. */
    curr_file->physical_line_count_increment +=
				 INITIAL_PHYSICAL_LINE_COUNT_INCREMENT;
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Updated error file index entries:\n");
    for (idx = 0;
         idx < NUMBER_OF_ERROR_FILE_INDEX_TABLE_ENTRIES; ++idx) {
      fprintf(f_debug, "entry %lu=%5lu\n", (unsigned long)idx,
              (unsigned long)curr_file->line_number[idx]);
    }  /* for */
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  /* Return the physical line number at which the next entry should be made. */
  return physical_line +
                       (a_line_number)curr_file->physical_line_count_increment;
}  /* update_file_index */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

static void optimum_file_start_position(a_source_file_ptr src_file,
                                        a_line_number     physical_line,
                                        long              *seek_position,
                                        a_line_number     *starting_line)
/*
Given the source file specified by the IL source file entry pointer
src_file and the desired physical line number in that file, determine the
best position in the file to begin reading source lines.  The worst case
is from the beginning of the file, but if we have built a source line
index for the file as we were reading it, there may be a position in the
file which is closer to the desired line.
*/
{
  an_error_file_index_ptr curr_file;
  int                     idx;

  /* Locate the file index entry for the IL file entry specified by
      src_file. */
  for (curr_file = head_of_file_index_list;
       curr_file != NULL;
       curr_file = curr_file->next) {
    if (curr_file->source_file == src_file) break;
  }  /* for */
#if CHECKING
  if (curr_file == NULL) {
#if DEBUG
    if (debug_level > 0) {
      (void)fprintf(f_debug,
                    "Missing file index entry for source file \"%s\"\n", 
                    src_file->full_name);
    }  /* if */
#endif /* DEBUG */
    internal_error("optimum_file_start_position: missing file index entry");
  }  /* if */
#endif /* CHECKING */

  /* Find the index of the first entry greater than the specified physical
     line. */
  for (idx = 0; idx < curr_file->next_index_entry; idx++) {
    if (curr_file->line_number[idx] > physical_line )  break;
  }  /* for */
  if (idx == 0) {
    /* Desired position is earlier than any known position; start at the
       beginning of the file. */
    *seek_position = 0L;
    *starting_line = 1;
  } else {
    /* Return the last encountered "good" file position. */
    *seek_position = curr_file->file_position[idx - 1];
    *starting_line = curr_file->line_number[idx - 1];
  }  /* if */
}  /* optimum_file_start_position */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

STATIC_THREAD FILE
		*f_err_src_file;
			/* File variable used to fetch the source line from
			   the source file. */

STATIC_THREAD a_boolean
		can_locate_source_line_info_cached;
			/* TRUE if the cached... values below can be used
			   to fetch the information about a previous call to
			   can_locate_source_line. */

STATIC_THREAD a_seq_number
		cached_seq_number;
			/* The last sequence number passed into
			   can_locate_source_line. */

STATIC_THREAD a_unicode_source_kind
		cached_unicode_source_kind;
			/* The unicode_source_kind returned by the last call
			   to can_locate_source_line. */

STATIC_THREAD a_boolean
		cached_can_locate_source_line;
			/* The result returned by the last call
			   to can_locate_source_line. */

static a_boolean can_locate_source_line(
                                    a_seq_number          seq_number,
                                    a_unicode_source_kind *unicode_source_kind)
/*
Determine the actual file which contains the specified sequence number.  If
possible read the desired source line into the buffer pointed to by
error_source_line for later use by diagnostic output functions.
*unicode_source_kind is set to indicate the kind of Unicode encoding
form for the file, or usk_none if the file is not Unicode.
*/
{
  a_source_file_ptr src_file;
  a_line_number     physical_line, starting_line, skip_lines;
  long              seek_position;
  a_boolean         at_end_of_source;
  a_boolean         src_line_found = FALSE;
  int               ch;
  char              *loc_in_line;
  char              *after_end_of_error_source_line_minus_2;
#if UNICODE_SOURCE_SUPPORTED
  a_getc_source_state
                    source_state;
#endif /* UNICODE_SOURCE_SUPPORTED */

  /* This routine is called twice for most diagnostic cases, so we cache
     the most recent call to avoid to cost of reading the file again.
     In some cases (where macro_positions_in_diagnostics is TRUE) this
     optimization may not be possible.  In such cases, if the source line
     for the diagnostic is a macro definition, the two calls will refer
     to different sequence numbers and the optimization will not apply. */
  if (can_locate_source_line_info_cached && seq_number == cached_seq_number) {
    src_line_found = cached_can_locate_source_line;
    if (src_line_found) {
      *unicode_source_kind = cached_unicode_source_kind;
    }  /* if */
    goto return_point;
  }  /* if */
  /* Ignore the old cached values. */
  can_locate_source_line_info_cached = FALSE;
  *unicode_source_kind = usk_none;
  conv_seq_to_physical_file_and_line(seq_number, &src_file, &physical_line,
                                     &at_end_of_source);
  if (physical_line == 0 ||
      src_file->assoc_module != NULL ||
      at_end_of_source ||
      strcmp(src_file->full_name, FILE_NAME_FOR_STDIN) == 0 ||
#if MICROSOFT_EXTENSIONS_ALLOWED
      src_file->is_assembly_file ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      head_of_file_index_list == NULL) {
    /* Either the file position is strange or unknown, we are at the end of
       the primary source file, the input is from stdin, the input is
       from a module or C++/CLI assembly file, or there is no file index
       information (for example, because we are currently in the back end).
       The original source line cannot be recovered.  Note that in the case
       of a header module, it may be possible to find the original source
       (but it's also possible that the source file has been changed more
       recently than the module file itself).  This may be revisited in the
       future. */
    goto return_point;
  } else {
    /* Determine the optimum starting position in the file to read the desired
       source line. */
    optimum_file_start_position(src_file, physical_line, &seek_position,
                                &starting_line);
    /* Attempt to read the desired source line.  The source file should be
       readable unless it was deleted recently.  Fail softly if any problems
       arise. */
    if ((f_err_src_file = reopen_source_file(src_file->full_name,
                                             unicode_source_kind)) != NULL) {
      if (seek_position != 0) {
        if (fseek(f_err_src_file, seek_position, SEEK_SET) != 0) {
          /* The seek failed; fail softly and assume the source line is
             not readable. */
          goto close_file;
        }  /* if */
      }  /* if */
#if UNICODE_SOURCE_SUPPORTED
      clear_getc_source_state(&source_state, *unicode_source_kind);
#endif /* UNICODE_SOURCE_SUPPORTED */
      /* Skip over lines in the file to the position of the desired line. */
      for (skip_lines = physical_line - starting_line;
           skip_lines > 0;
           skip_lines--) {
        while ((ch = getc_source(f_err_src_file, source_state)) != '\n') {
          /* If the file has been changed under us, fail softly and assume
             the source line is not readable. */
          if (ch == EOF) goto close_file;
        }  /* while */
      }  /* for */
      /* Now positioned to read the actual source line desired.  Check if the
         error_source_line_buffer has been allocated.  This check may seem
         wasteful here, but it will only be done when a source line other
         than the current source line is needed, typically on a warning.
         The same error_source_line buffer will be used over multiple
         compilations. */
      if (error_source_line == NULL) {
        error_source_line = alloc_resizable_buffer(
                                  ERROR_SOURCE_LINE_INITIAL_ALLOCATION + 1);
        after_end_of_error_source_line = error_source_line +
                                  ERROR_SOURCE_LINE_INITIAL_ALLOCATION;
      }  /* if */
      loc_in_line = error_source_line;
      after_end_of_error_source_line_minus_2 = after_end_of_error_source_line -
                                               2;
      while ((ch = getc_source(f_err_src_file, source_state)) != '\n' &&
             ch != EOF) {
        if (loc_in_line == after_end_of_error_source_line_minus_2) {
          /* The buffer is not large enough for the current line. */
          sizeof_t  curr_length, old_size, new_size;
          char      *new_error_source_line;

          curr_length = (sizeof_t)(loc_in_line - error_source_line);
          old_size = (sizeof_t)(after_end_of_error_source_line -
                                error_source_line);
          /* Increase the size of the error_source_line buffer. */
          new_size = old_size + ERROR_SOURCE_LINE_INCREMENTAL_ALLOCATION;
          /* As with the curr_source_line, add one more byte than required,
             so that a pointer past the end will not have the same address
             as a pointer to the next object. */
          new_error_source_line = realloc_buffer(error_source_line,
                                                  (sizeof_t)(old_size + 1),
                                                  (sizeof_t)(new_size + 1));
          /* Adjust the pointers to the old error_source_line */
          error_source_line = new_error_source_line;
          after_end_of_error_source_line = error_source_line + new_size;
          loc_in_line = error_source_line + curr_length;
          after_end_of_error_source_line_minus_2 =
                                     after_end_of_error_source_line - 2;
        }  /* if */
        /* Change a null character to a space (which is also what is done
           when the line is read initially). */
        if (ch == '\0') ch = ' ';
        /* Add the character to the buffer. */
        *loc_in_line++ = (char)ch;
      }  /* while */
      /* Add a trailing newline and null. */
      *loc_in_line++ = '\n';
      *loc_in_line = '\0';
      src_line_found = TRUE;

close_file:
      (void)fclose(f_err_src_file);
      f_err_src_file = NULL;
    }  /* if */
  }  /* if */
    
return_point:
  if (!can_locate_source_line_info_cached) {
    /* We are not using previously-cached information.  Save the new
       information so that it can (potentially) be used by a subsequent
       call. */
    can_locate_source_line_info_cached = TRUE;
    cached_seq_number = seq_number;
    cached_can_locate_source_line = src_line_found;
    if (src_line_found) {
      cached_unicode_source_kind = *unicode_source_kind;
    }   /* if */
  }  /* if */
  return src_line_found;
}  /* can_locate_source_line */


static void output_msg_buffer(void)
/*
Add the contents of msg_buffer to the write_diagnostic_buffer and
reset msg_buffer.
*/
{
  add_to_text_buffer(write_diagnostic_buffer, msg_buffer->buffer,
                     msg_buffer->size);
  add_char_to_text_buffer(write_diagnostic_buffer, '\n');
  reset_text_buffer(msg_buffer);
}  /* output_msg_buffer */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
Put out_char into a local (a_text_buffer) buffer for later writing.  This is
done to avoid lots of costly system calls in the usual case that f_error
points to stderr, and stderr is unbuffered, as well as to emit an entire
diagnostic in a single system call.  This minimizes the chances of diagnostic
messages being intermixed when multiple compilations are performed in
parallel.
*/

#define putcb(out_char, buffer)                                       \
  add_char_to_text_buffer((buffer), (out_char))

/*
Shorthand for cases where an output character (out_char) is being written
to the msg_buffer.
*/

#define putcwdb(out_char)                                             \
  putcb((out_char), msg_buffer)

#if !STANDALONE_UTILITY_PROGRAM

/*
Macro to write source line characters in the first pass, and spaces over
and the caret on the second pass.  Exits to "end_of_loop" upon finding the
column for the caret in the second pass.
*/
/*lint -emacro(506,put_char)*/
#define put_char(out_char)                                            \
{ if (pass_for_caret && curr_column >= source_pos->column) {          \
    goto end_of_loop;                                                 \
  } else {                                                            \
    if ((out_char != '\r') &&                                         \
        (/*lint -e(506,845)*/ !pass_for_caret || (out_char) == '\t')) {  \
      putcwdb(out_char);                                              \
    } else {                                                          \
      putcwdb(' ');                                                   \
    }  /* if */                                                       \
    curr_column++;                                                    \
  }  /* if */                                                         \
}  /* put_char */


/*
Put out a character at *loc_in_line to the error output buffer, and
advance loc_in_line.  On the pass_for_caret, outputs a blank instead of
the character.  Handles multibyte characters appropriately.  ch is
the character to be used for *loc_in_line; it is different from what
is stored there when an ATTENTION_MARKER is being replaced by the
original character at that position.
*/
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED

#if UNICODE_SOURCE_SUPPORTED
#define and_unicode_source_kind_ne_usk_none(ukind) \
  && ((ukind) != usk_none)
#else /* !UNICODE_SOURCE_SUPPORTED */
#define and_unicode_source_kind_ne_usk_none(ukind) /* Nothing */
#endif /* UNICODE_SOURCE_SUPPORTED */

#define put_char_from_line(ch, ukind) \
{ put_char(ch); \
  if (multibyte_chars_in_source_enabled \
      and_unicode_source_kind_ne_usk_none(ukind)) { \
    int  numch; \
    char orig_ch = *loc_in_line; \
    *(char *)loc_in_line = (ch);                       \
    numch = mbc_length_simple(loc_in_line) - 1; \
    *(char *)loc_in_line = orig_ch;                    \
    while (numch-- > 0) { \
      loc_in_line++; \
      if (!pass_for_caret) { \
         putcwdb(*loc_in_line); \
      } \
    }  /* while */ \
  }  /* if */ \
  loc_in_line++; \
}  /* put_char_from_line */
#else /* !MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
#define put_char_from_line(ch, ukind) \
{ put_char(ch); \
  loc_in_line++; \
}  /* put_char_from_line */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */


static void write_orig_source_line(a_source_position *source_pos)
/*
Write out the source line associated with the source position source_pos,
and place a caret under the proper column.  The position must be within
the current logical source line.  If the column position is zero, write
a blank line instead of the caret line.  All diagnostics generated by this
routine (or routines it calls) are placed in msg_buffer for later output
as appropriate.
*/
{
  a_seq_number            seq;
  an_orig_line_modif_ptr  line_olmp, olmp, olmp_next;
  a_const_char            *line_start, *loc_in_line;
  a_column_number         curr_column;
  int                     pass_for_caret;
  char                    ch;
  a_source_line_modif_ptr slmp;
  int                     i;

  /* Start by finding the right line.  The logical source line originally
     came from one or more physical lines ended by "\"s (line splices).
     Find the location of the start of text for the physical line
     containing the desired source position. */
  line_start = curr_source_line;
  seq = curr_seq_number;
  line_olmp = orig_line_modif_list;
  while (seq != source_pos->seq) {
    /* Need to advance to the next physical line.  Find the next line splice
       modification in the list of modifications. */
    for (;; line_olmp = line_olmp->next) {
#if CHECKING
      if (line_olmp == NULL) {
        internal_error("write_orig_source_line: could not find line");
      }  /* if */
#endif /* CHECKING */
      if (line_olmp->kind == olm_line_splice ||
          line_olmp->kind == olm_multiline_string_splice) break;
    }  /* for */
    seq++;
    line_start = line_olmp->line_loc;
    /* Skip the inserted \n of a multiline-string splice. */
    if (line_olmp->kind == olm_multiline_string_splice) line_start += 2;
    line_olmp = line_olmp->next;
  }  /* while */
  /* Found the proper physical line. */
  /* Take two passes -- the first to write the source line, the second to
     write the caret.  Because of the presence of tabs in the source line,
     etc. it is hard to figure out where to place the caret without
     running through the data structure again. */
  for (pass_for_caret = 0; pass_for_caret <= 1; pass_for_caret++) {
    /* Indent both the source line and the caret line.  This is done so
       that programs (like emacs) that read the error output will ignore
       these lines. */
    for (i = 0; i < SOURCE_INDENT; ++i) {
      putcwdb(' ');
    }  /* for */
    /* Perform any additional indentation needed (based on the category
       kind) */
    for (i = 0; i < diagnostic_indent; i++) {
      putcwdb(' ');
    }  /* for */
    /* On the caret pass, if the column number is zero (unknown), skip
       writing the spaces and caret and go right to the newline. */
    if (!pass_for_caret || source_pos->column != SP_COL_UNKNOWN) {
      loc_in_line = line_start;
      curr_column = 1;
      for (olmp = line_olmp; /*Exited by goto*/; olmp = olmp->next) {
        /* Print the characters of the current piece of curr_source_line, up
           to the next modification.  This is done a character at a time
           so that tabs can be processed specially and so that on the
           second pass the spaces/caret can be output. */
        while (olmp == NULL || loc_in_line != olmp->line_loc) {
          ch = *loc_in_line;
          if (ch == LE_ESCAPE) {
overwritten_escape:
            if (loc_in_line[1] == LE_END_OF_EMBED_PREFIX ||
                loc_in_line[1] == LE_END_OF_EMBED) {
              /* This lexical escape marks the end of a prefix, suffix, or
                 if_empty parameter in a #embed directive.  The LE_ESCAPE
                 replaces the ')' and the original character overwritten
                 by the second byte of the escape is stored in the embed
                 control block. */
              put_char_from_line(')', curr_file_unicode_source_kind);
              ch = orig_char_from_embed_directive(*loc_in_line);
              if (ch == LE_ESCAPE) {
                goto overwritten_escape;
              }  /* if */
            } else {
              /* LE_NULL is handled below (it has an associated
                 modification entry).  No other lexical escapes besides
                 LE_NEWLINE should be encountered. */
              /* Exit on the newline at the end of the source line.  (If there
                 wasn't one there originally, one has been added.) */
              check_assertion_str(
                                 loc_in_line[1] == LE_NEWLINE,
                                 "write_orig_source_line: bad lexical escape");
              goto end_of_loop;
            }  /* if */
          }  /* if */
          /* This character of the source line may have been replaced by
             an attention character to indicate that some sort of source
             line modification starts here.  If so, go to the modification
             entry and get the original source line character. */
          if (ch == ATTENTION_MARKER) {
            slmp = nested_source_line_modif(loc_in_line);
            ch = (char)slmp->orig_char;
          }  /* if */
          /* Put out the character, which is possibly a multibyte
             character, and advance loc_in_line to after the character. */
          put_char_from_line(ch, curr_file_unicode_source_kind);
        }  /* while */
        /* Dump the characters for the modification. */
        switch ((int)olmp->kind) {
          case olm_trigraph:
            put_char('?');
            put_char('?');
            put_char((char)olmp->variant.orig_char);
            loc_in_line++;
            /* If the trigraph is "? ? /", which turns into "\", and it's at
	       the end of a line, the "\" will indicate a line splice.  In
	       that case, the "\" for the line splice should not be put out.
	       (Note that the added space in this comment is to avoid
	       complaints about trigraphs while compiling this code.) */
            olmp_next = olmp->next;
            if (olmp_next != NULL && olmp_next->kind == olm_line_splice &&
                olmp_next->line_loc == olmp->line_loc) {
              /* Exit the loop because the line splice marks the end of
                 the physical line. */
              goto end_of_loop;
            }  /* if */
            break;
          case olm_line_splice:
            put_char('\\');
            FALLTHROUGH
          case olm_multiline_string_splice:
            /* Exit the loop since this line splice marks the end of the
               physical line. */
            goto end_of_loop;
          case olm_null:
            /* Null (zero) character in source line. */
            put_char(' ');
            loc_in_line += LE_ESCAPE_LEN;
            break;
          case olm_splice_whitespace:
            /* A whitespace character following the '\' of a line splice.
               It is not represented in the line. */
            break;
          default:
            unexpected_condition_str2("write_orig_source_line:",
                                      "bad orig_modif_list entry");
        }  /* switch */
      }  /* for */
end_of_loop:
      /* For the pass that writes the caret, write the caret at this point. */
      if (pass_for_caret) {
        putcwdb('^');
      }  /* if */
    }  /* if */
    output_msg_buffer();
    /* After the first pass (writing the source), go on to the second pass
       (writing the caret). */
  }  /* for */
}  /* write_orig_source_line */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

static void write_error_source_line(
                          a_source_position                *source_pos,
                          ARG_UNUSED a_unicode_source_kind unicode_source_kind)
/*
Write out the source line associated with the source position source_pos,
and place a caret under the proper column.  The position has been determined
earlier to be in other than the current logical source line and the line
has been reread into the buffer pointed to by the static variable
error_source_line.  If the column position is zero, write a blank line
instead of the caret line.  unicode_source_kind indicates the kind of Unicode
encoding form for the file, or usk_none if the file is not Unicode.
All diagnostics generated by this routine (or routines it calls) are placed in
msg_buffer for later output as appropriate.
*/
{
  char            *loc_in_line;
  char            ch;
  int             pass_for_caret;
  a_column_number curr_column;
  int             i;

  /* Take two passes -- the first to write the source line, the second to
     write the caret.  Because of the presence of tabs in the source line,
     etc. it is hard to figure out where to place the caret without
     running through the characters again. */
  for (pass_for_caret = 0; pass_for_caret <= 1; pass_for_caret++) {
    /* Indent both the source line and the caret line.  This is done so
       that programs (like emacs) that read the error output will ignore
       these lines. */
    for (i = 0; i < SOURCE_INDENT; ++i) {
      putcwdb(' ');
    }  /* for */
    /* Perform any additional indentation needed (based on the category
       kind). */
    for (i = 0; i < diagnostic_indent; i++) {
      putcwdb(' ');
    }  /* for */
    /* On the caret pass, if the column number is zero (unknown), skip
       writing the spaces and caret and go right to the newline. */
    if (!pass_for_caret || source_pos->column != SP_COL_UNKNOWN) {
      loc_in_line = error_source_line;
      curr_column = 1;
      /* Process each individual character until the newline is found. */
      for (;;) {
        /* Exit on the newline or carriage return/newline at the end of the
           source line. */
        if ((ch = *loc_in_line) == '\n' ||
            (ch == '\r' && loc_in_line[1] == '\n')) goto end_of_loop;
        put_char_from_line(ch, unicode_source_kind);
      }  /* for */

end_of_loop:
      /* For the pass that writes the caret, write the caret at this point. */
      if (pass_for_caret) {
        putcwdb('^');
      }  /* if */
    }  /* if */
    output_msg_buffer();
    /* After the first pass (writing the source), go on to the second pass
       (writing the caret). */
  }  /* for */
}  /* write_error_source_line */


static a_boolean source_position_is_in_module_file(a_source_position* pos)
/*
Determine whether the given source position is in a module file (and therefore
no source line is available).  Return TRUE if so, FALSE otherwise.
*/
{
  a_boolean         result = FALSE;
  a_source_file_ptr src_file;
  a_line_number     line_no;
  a_boolean         at_end_of_source;

  /* Do not use physical lines; modules don't consistently have sensible
     physical lines since they're loaded lazily.  Additionally, physical lines
     are both slower, and may result in the translation unit source file being
     returned rather than the implicitly created module source file. */
  src_file = source_file_for_seq(pos->seq, &line_no, &at_end_of_source,
                                 /*physical_line=*/FALSE);
  if (src_file != NULL && src_file->assoc_module != NULL) {
    result = TRUE;
  }  /* if */
  return result;
}  /* source_position_is_in_module_file */


static void write_source_line(a_source_position		*position,
			      a_source_info_for_pos_ptr	sifpp)
/*
Write the source line specified by position and sifpp.  If the position
designates a position on the current source line, output that line.  Otherwise,
see if we can get the source line from a file, and if so, output that line.
*/
{
  a_boolean	write_source;
  a_boolean	use_orig_line = TRUE;

  if (position->seq == 0 || sifpp->at_end_of_source ||
      source_position_is_in_module_file(position)) {
    /* There is no position, the position is in a module file, or it is at the
       end-of-file. */
    write_source = FALSE;
  } else {
    write_source = !brief_diagnostics;
    if (write_source && position->seq < curr_seq_number) {
      /* Not in current source line -- see if we can print it. */
      write_source = can_locate_source_line(position->seq,
                                            &sifpp->unicode_source_kind);
      use_orig_line = FALSE;
    }  /* if */
  }  /* if */
  if (write_source) {
    if (use_orig_line) {
      write_orig_source_line(position);
    } else {
      write_error_source_line(position, sifpp->unicode_source_kind);
    }  /* if */
  }  /* if */
}  /* write_source_line */

#else /* STANDALONE_UTILITY_PROGRAM */

#define write_source_line(position, sifpp) /* nothing */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static void add_position_prefix(a_source_info_for_pos_ptr sifpp,
                                a_column_number           column_number)
/*
Add the source position using the information in sifpp (filename and line
number -- when non-zero) to msg_buffer.  If column_number is not
SP_COL_UNKNOWN, the column number is also added.
*/
{
  a_const_char    *error_text_string;
  a_line_number   line_number = sifpp->line_number;
  a_number_buffer num_buffer;

  annotate_diagnostic(prefix_buffer, da_locus);
  /* Print the file and line number, with a column number if it is not
     SP_COL_UNKNOWN. */
  /* If the line is from stdin, do not display the file name. */
  if (strcmp(sifpp->file_name, FILE_NAME_FOR_STDIN) == 0) {
    error_text_string = error_text(ec_Line);
    add_string_to_text_buffer(prefix_buffer, error_text_string);
    add_string_to_text_buffer(prefix_buffer, " ");
    num_buffer.reset_to(line_number);
    add_string_to_text_buffer(prefix_buffer, num_buffer.as_temp_characters());
  } else {
    if (sifpp->source_file != NULL) {
      a_const_char *file_name;
      file_name = format_source_file_name(sifpp->source_file,
                                          /*use_name_as_written=*/FALSE,
                                          /*quote_file_name=*/TRUE);
      add_string_to_text_buffer(prefix_buffer, file_name);
    } else {
      add_char_to_text_buffer(prefix_buffer, '"');
      /* Don't convert '\' to '\\' in error message output.  The
         name should be displayed as written by the user.  This also
         prevents doubling of directory separators on Windows. */
      write_file_name_to_text_buffer(sifpp->file_name, prefix_buffer,
                                     /*process_escapes=*/FALSE,
                                     /*escape_nonprintable_chars=*/FALSE);
      add_char_to_text_buffer(prefix_buffer, '"');
    }  /* if */
    if (line_number != SP_LINE_UNKNOWN) {
      error_text_string = error_text(ec_line);
      add_string_to_text_buffer(prefix_buffer, ", ");
      add_string_to_text_buffer(prefix_buffer, error_text_string);
      add_string_to_text_buffer(prefix_buffer, " ");
      num_buffer.reset_to(line_number);
      add_string_to_text_buffer(prefix_buffer,
                                num_buffer.as_temp_characters());
    }  /* if */
  }  /* if */
  if (column_number != SP_COL_UNKNOWN) {
    error_text_string = error_text(ec_col);
    add_string_to_text_buffer(prefix_buffer, " (");
    add_string_to_text_buffer(prefix_buffer, error_text_string);
    add_string_to_text_buffer(prefix_buffer, " ");
    num_buffer.reset_to(column_number);
    add_string_to_text_buffer(prefix_buffer, num_buffer.as_temp_characters());
    add_string_to_text_buffer(prefix_buffer, ")");
  }  /* if */
  annotate_diagnostic(prefix_buffer, da_reset);
}  /* add_position_prefix */


static an_error_severity determine_reported_severity(a_diagnostic_ptr dp)
/*
Determine the appropriate severity string for the given diagnostic pointer.
*/
{
  an_error_severity reported_severity;
  reported_severity = dp->severity;
  if ((int)reported_severity < (int)es_error &&
      (int)reported_severity >= (int)error_promotion_threshold) {
    reported_severity = es_discretionary_error;
  }  /* if */
  return reported_severity;
}  /* determine_reported_severity */


static an_error_code determine_severity_code(
                                  an_error_severity reported_severity,
                                  a_boolean         capitalize_severity,
                                  a_boolean         local_display_error_number)
/*
Determine the appropriate severity error code for the given diagnostic pointer
factoring in the capitalization requested.  Additionally, consider whether or
not the error number should be displayed for this diagnostic.
*/
{
  an_error_code severity_code;

  switch (reported_severity) {
    case es_more_info:
      severity_code = capitalize_severity ? ec_More_Info : ec_more_info;
      break;
    case es_remark:
      severity_code = capitalize_severity ? ec_Remark : ec_remark;
      break;
    case es_warning:
      severity_code = capitalize_severity ? ec_Warning : ec_warning;
      break;
    case es_command_line_warning:
      severity_code = capitalize_severity ? ec_Command_line_warning
                                          : ec_command_line_warning;
      break;
    case es_error:
    case es_discretionary_error:
      if (local_display_error_number ||
          ERROR_SEVERITY_EXPLICIT_IN_ERROR_MESSAGES) { /*lint !e506 !e774*/
        severity_code = capitalize_severity ? ec_Error : ec_error;
      } else {
        severity_code = ec_no_error;
      }  /* if */
      break;
    case es_catastrophe:
      severity_code = capitalize_severity ? ec_Catastrophic_error
                                          : ec_catastrophic_error;
      break;
    case es_command_line_error:
      severity_code = capitalize_severity ? ec_Command_line_error
                                          : ec_command_line_error;
      break;
    case es_internal_error:
      severity_code = capitalize_severity ? ec_Internal_error
                                          : ec_internal_error;
      break;
    case es_none:
    default:
      severity_code = ec_error;
      unexpected_condition_str("determine_severity_code: bad severity");
  }  /* switch */
  return severity_code;
}  /* determine_severity_code */


static a_diagnostic_annotation_kind determine_diagnostic_annotation_kind(
                                           an_error_severity reported_severity)
/*
Determine the appropriate diagnostic annotation kind for the given reported
severity.
*/
{
  a_diagnostic_annotation_kind annotation_kind;

  switch (reported_severity) {
    case es_more_info:
      annotation_kind = da_note;
      break;
    case es_remark:
      annotation_kind = da_note;
      break;
    case es_warning:
    case es_command_line_warning:
      annotation_kind = da_warning;
      break;
    case es_error:
    case es_discretionary_error:
    case es_catastrophe:
    case es_command_line_error:
    case es_internal_error:
      annotation_kind = da_error;
      break;
    case es_none:
    default:
      unexpected_condition_str(
                         "determine_diagnostic_annotation_kind: bad severity");
  }  /* switch */
  return annotation_kind;
}  /* determine_diagnostic_annotation_kind */


static void update_diagnostic_counter(
                                    an_error_severity        reported_severity,
                                    a_diagnostic_counter_ptr counter)
/*
Update the given counter based on the reported severity.
*/
{
  switch (reported_severity) {
    case es_more_info:
      break;
    case es_remark:
      ++(counter->remarks);
      break;
    case es_warning:
    case es_command_line_warning:
      ++(counter->warnings);
      break;
    case es_error:
    case es_discretionary_error:
      ++(counter->errors);
      break;
    case es_catastrophe:
    case es_command_line_error:
    case es_internal_error:
      ++(counter->catastrophes);
      break;
    case es_none:
    default:
      unexpected_condition_str("update_diagnostic_counter: bad severity");
  }  /* switch */
}  /* update_diagnostic_counter */


static void add_primary_prefix(a_diagnostic_ptr	dp)
/*
Write the source position (file name and line number) and severity to the
prefix_buffer text buffer.  Determine if the actual source line is
available, either in the current source line or able to be reread from one of
the source files.   If the actual source line is not available, the column
number is added into the output.
*/
{
  a_const_char                  *error_text_string;
  a_boolean                     capitalize_severity;
  a_boolean                     column_needed;
  a_boolean                     local_display_error_number;
  an_error_code                 severity_code;
  an_error_severity             reported_severity;
  a_source_info_for_pos_ptr     sifpp = &dp->diag_header_source_info;
  a_source_position             *pos = &dp->diag_header_pos;

#if STANDALONE_UTILITY_PROGRAM
  local_display_error_number = FALSE;
#else /* !STANDALONE_UTILITY_PROGRAM */
  /* Determine whether the error number should be displayed for this
     diagnostic.  Internal errors don't have error numbers.  If the
     caller passes the value ec_no_error, the error number display is
     suppressed. */
  local_display_error_number = display_error_number &&
                               dp->error_code != ec_no_error;
#endif /* STANDALONE_UTILITY_PROGRAM */
  capitalize_severity = FALSE;
  /* Determine the source position (file, line number). */
#if !STANDALONE_UTILITY_PROGRAM
  if (processing_predefined_macro) {
    /* The error concerns a line in the predefined macro file. */
    error_text_string = error_text(ec_predef_macro_file);
    add_string_to_text_buffer(prefix_buffer, error_text_string);
    add_string_to_text_buffer(prefix_buffer, ": ");
  } else
#endif /* !STANDALONE_UTILITY_PROGRAM */
  if (pos->seq == 0) {
    /* Error position is in the command line or in initialization. */
    /* No position indication is written. */
    capitalize_severity = TRUE;
  } else {
    if (sifpp->at_end_of_source) {
      /* After end of source. */
      error_text_string = error_text(ec_at_end_of_source2);
      add_string_to_text_buffer(prefix_buffer, error_text_string);
      add_string_to_text_buffer(prefix_buffer, ": ");
    } else {
      /* Normal line in file, not end of file. */
#if STANDALONE_UTILITY_PROGRAM
      /* In program-form C-generating back end, source lines are 
         never displayed. */
      column_needed = FALSE;
#else /* !STANDALONE_UTILITY_PROGRAM */
      column_needed = brief_diagnostics &&
		      /*lint -e(506)*/COLUMN_NUMBER_IN_BRIEF_DIAGNOSTICS;
      /* If the line is the current one, print it and a caret indicating
         the position. */
      if (pos->seq >= curr_seq_number) {
        /* The sequence number falls within the sequence numbers for the
           current logical source line (it can't be past the current
           line). */
      } else {
        /* The sequence number is not in the current logical source line.
           Try to relocate the source line in the known source files. */
        if (!brief_diagnostics &&
            can_locate_source_line(pos->seq,
                                   &sifpp->unicode_source_kind)) {
          /* The source line has been read into the error_source_line
             buffer. */
        } else {
          /* The source line could not be reread.  Print column if it is
             nonzero. */
          column_needed = (pos->column != 0);
        }  /* if */
      }  /* if */
#endif /* STANDALONE_UTILITY_PROGRAM */
      /* Print the file and line number, with a column number if the
         position could not be indicated via a caret pointing to the
         source of the current line. */
      add_position_prefix(sifpp, column_needed ? pos->column
                                               : SP_COL_UNKNOWN);
      add_string_to_text_buffer(prefix_buffer, ": ");
    }  /* if */
  }  /* if */
  reported_severity = determine_reported_severity(dp);
  severity_code = determine_severity_code(reported_severity,
                                          capitalize_severity,
                                          local_display_error_number);
  if (severity_code != ec_no_error) {
    a_diagnostic_annotation_kind annotation_kind;

    error_text_string = error_text(severity_code);
    annotation_kind = determine_diagnostic_annotation_kind(reported_severity);
    annotate_diagnostic(prefix_buffer, annotation_kind);
    add_string_to_text_buffer(prefix_buffer, error_text_string);
    annotate_diagnostic(prefix_buffer, da_reset);
  }  /* if */
  /* The error number may optionally be displayed based on a command
     line option. */
  if (local_display_error_number) {
    /* Display the error message number.  Append a -D suffix if the
       severity may be changed. */
    a_boolean       is_discretionary;
    a_number_buffer num_buffer((size_t)dp->error_code);

    is_discretionary = ((int)dp->severity <= (int)es_discretionary_error);
    error_text_string = error_text(
                               is_discretionary ? ec_discretionary_suffix
                                                : ec_non_discretionary_suffix);
    add_string_to_text_buffer(prefix_buffer, " #");
    add_string_to_text_buffer(prefix_buffer, num_buffer.as_temp_characters());
    add_string_to_text_buffer(prefix_buffer, error_text_string);
  }  /* if */
  add_string_to_text_buffer(prefix_buffer, ": ");
}  /* add_primary_prefix */

#if !STANDALONE_UTILITY_PROGRAM

static void write_diag_to_raw_listing(a_diagnostic_ptr	dp)
/*
If raw-listing information has been requested, the diagnostic message
is also output to the raw-listing file in coded form, for later
incorporation into the listing.  The coded form output line has the form:

  S "file-name" line-number column-number message-text

where "S" is R for remark, W for warning, E for error, and C for
catastrophe, command-line error, or internal error.  If the diagnostic
message is an additional message, the coded severity is in lower case.
*/
{
  char			severity_char;
  a_diagnostic_ptr	primary_dp;

  /* Use the primary diagnostic to determine the severity. */
  primary_dp = dp->primary_diag != NULL ? dp->primary_diag : dp;
  /* Start with the severity code character. */
  switch (primary_dp->severity) {
    case es_remark:
      severity_char = 'R';
      break;
    case es_warning:
    case es_command_line_warning:
      severity_char = 'W';
      break;
    case es_discretionary_error:
    case es_error:
      severity_char = 'E';
      break;
    case es_catastrophe:
    case es_command_line_error:
    case es_internal_error:
      severity_char = 'C';
      break;
    case es_none:
    default:
      severity_char = '?';
      unexpected_condition_str("write_diag_to_raw_listing: bad severity");
  }  /* switch */
  if (dp->primary_diag != NULL) {
    /* For things that are not top-level diagnostics, the severity is
       put out in lower case. */
    severity_char = (char)tolower((int)severity_char);
  }  /* if */
  (void)putc(severity_char, f_raw_listing);
  (void)fputc(' ', f_raw_listing);
  /* Determine the source position (file, line number). */
  if (dp->position.seq == 0) {
    /* Error position is in the command line or in initialization. */
    fputs("\"\" 0 0 ", f_raw_listing);
  } else {
    /* Normal line in file, or end of source.  Note that
       conv_seq_to_file_and_line has returned the position of the
       last line of the primary source file for the end-of-source case. */
    fprintf(f_raw_listing, "\"%s\" %lu %d ",
            format_file_name(primary_dp->source_info.file_name),
            (unsigned long)primary_dp->source_info.line_number,
            dp->position.column);
  }  /* if */
  /* For an internal error, the coded-form message indicates only that the
     error is catastrophic, so we add text to indicate that it is an
     internal error. */
  if (primary_dp->severity == es_internal_error) {
    fputs("(internal error) ", f_raw_listing);
  }  /* if */
  /* Put out the error message text. */
  fputs(msg_buffer->buffer, f_raw_listing);
  (void)fputc('\n', f_raw_listing);
}  /* write_diag_to_raw_listing */

/* Forward declarations. */
static a_boolean in_secondary_translation_unit(a_source_position *pos);

#endif /* !STANDALONE_UTILITY_PROGRAM */

static void general_diagnostic(
			an_error_severity	error_severity,
			an_error_code		error_code,
			a_source_position	*error_pos,
			a_const_char		*string1,
			a_const_char		*string2,
			a_symbol_ptr		symbol1,
			a_symbol_ptr		symbol2,
			a_type_ptr		type1,
			a_type_ptr		type2,
			a_template_arg_ptr	templ_args1,
			a_template_arg_ptr	templ_args2,
			a_source_position	*other_pos,
			a_diag_list_ptr		diag_list);


NORETURN void insufficient_address_space()
/*
The program requires additional address space that it cannot acquire.  Write
a diagnostic message and exit.
*/
{
  catastrophe(ec_insufficient_address_space);
}  /* insufficient_address_space */

#if CHECKING

STATIC_THREAD a_boolean
                internal_error_loop;
			/* Set to TRUE once an internal error has been
			   detected.  Used to detect a loop in internal
			   error processing. */

NORETURN void internal_error(a_const_char *error_message)
/*
An internal error has occurred.  Write the given message and abort.
*/
{
  /* Make sure that if one internal error leads to another, we abort
     the compilation instead of looping. */
  if (internal_error_loop) {
    fprintf(f_error, "%s: %s\n", error_text(ec_internal_error_loop),
            error_message);
    term_compilation(es_internal_error);
  }  /* if */
  internal_error_loop = TRUE;
  general_diagnostic(es_internal_error, ec_internal_error_fill_in,
                     &error_position,
                     error_message, (a_const_char*)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
  /* Avoid spurious warning.  The function above does not return. */
  exit_compilation(es_internal_error);
}  /* internal_error */


NORETURN void assertion_failed(a_const_char *filename,
                               int          line_number,
                               a_const_char *function,
                               a_const_char *string1,
                               a_const_char *string2)
/*
An assertion has failed.  Abort the compilation.
*/
{
  a_text_buffer_ptr buffer;
  a_number_buffer   line_number_buffer;

  /* Strip the directory name of the file (if any).  Use the text buffer
     facilities to ensure there are no buffer overruns. */
  filename = start_of_file_name(filename);
  if (suppress_assertion_line_number) {
    line_number_buffer.append("<suppressed>");
  } else {
    line_number_buffer.append(line_number);
  }  /* if */
  buffer = alloc_text_buffer(1024);
  add_string_to_text_buffer(buffer, "assertion failed");
  if (string1 == NULL) {
    add_string_to_text_buffer(buffer, " at: \"");
    add_string_to_text_buffer(buffer, filename);
    add_string_to_text_buffer(buffer, "\"");
  } else {
    a_const_char *separator;
    add_string_to_text_buffer(buffer, ": ");
    add_string_to_text_buffer(buffer, string1);
    if (string2 == NULL || strlen(string2) == 0) {
      separator = "";
      if (string2 == NULL) string2 = "";
    } else {
      separator = " ";
    }  /* if */
    add_string_to_text_buffer(buffer, separator);
    add_string_to_text_buffer(buffer, string2);
    add_string_to_text_buffer(buffer, " (");
    add_string_to_text_buffer(buffer, filename);
  }  /* if */
  add_string_to_text_buffer(buffer, ", line ");
  add_string_to_text_buffer(buffer, line_number_buffer.as_temp_characters());
  add_string_to_text_buffer(buffer, " in ");
  add_string_to_text_buffer(buffer, function);
  if (string1 != NULL) {
    add_char_to_text_buffer(buffer, ')');
  }  /* if */
  add_char_to_text_buffer(buffer, '\n');
  add_char_to_text_buffer(buffer, '\0');
  internal_error(buffer->buffer);
}  /* assertion_failed */


/*
Structure to record a pending assertion.  If necessary, the recorded entities
will be passed to assertion_failed at a later time.
*/
STATIC_THREAD struct {
  a_const_char  *filename;
  int          line_number;
  a_const_char *function;
  a_const_char *string1;
  a_const_char *string2;
} expected_error_record;

  
void record_expected_error(a_const_char *filename,
                           int          line_number,
                           a_const_char *function,
                           a_const_char *string1,
                           a_const_char *string2)
/*
Record a pending assertion.  This routine may be called in a situation that
is expected to be the result of processing invalid source code but where a
diagnostic has not yet been issued.  The location (and associated message)
of that situation is recorded and then later checked by check_expected_errors.
Only the first instance of such a situation is recorded; subsequent calls
have no effect.
*/
{
  check_assertion(filename != NULL);
  if (expected_error_record.filename == NULL) {
    /* No expected error has been recorded yet. */
    expected_error_record.filename = filename;
    expected_error_record.line_number = line_number;
    expected_error_record.function = function;
    expected_error_record.string1 = string1;
    expected_error_record.string2 = string2;
  }  /* if */
}  /* record_expected_error */


void check_expected_errors(void)
/*
If expected_error was called, check that errors have been issued.  Otherwise,
abort the compilation with the information recorded in the first call to
expected_error.
*/
{
  if (expected_error_record.filename != NULL && !is_at_least_one_error()) {
    assertion_failed(expected_error_record.filename,
                     expected_error_record.line_number,
                     expected_error_record.function,
                     expected_error_record.string1,
                     expected_error_record.string2);
  }  /* if */
}  /* check_expected_errors */

#endif /* CHECKING */

#if !STANDALONE_UTILITY_PROGRAM

INLINE static a_pragma_diag_elem *pragma_diag_list_lower_bound(
                                                 const a_pragma_diag_elem *pos)
/*
Return a pointer to the element in pragma_diag_list with the greatest source
location that is also less than or equal to the source location in the
argument.  Note that due to the nature of this list, it is typically the
case that an exact match will not be found -- that's because the entries on
the list represent source locations of pragmas and we're searching for
a source location of an error (that is typically not on a #pragma line).
*/
{
  a_pragma_diag_elem *ptr, *result = NULL;

  if (pragma_diag_tail != NULL && pragma_diag_tail->spos < pos->spos) {
    /* It's often the case that the position is past all of the entries, so
       check for that. */
    result = pragma_diag_tail;
  } else {
    for (ptr = pragma_diag_list; ptr != NULL; ptr = ptr->next) {
      if (ptr->spos > pos->spos) {
        /* This entry in the list is past the given source position (so the
           last entry is the one we want). */
        break;
      }  /* if */
      result = ptr;
    }  /* for */
  }  /* if */
  return result;
}  /* pragma_diag_list_lower_bound */


static an_error_severity get_severity_from_pragma(
                                                 a_pragma_diag_elem *ptr,
                                                 an_error_code      error_code)
/*
Determine the error severity represented by the given pragma diag element using
the given error code's default severity when the pragma represents the default
severity.
*/
{
  an_error_severity severity;

  switch (ptr->kind) {
  case pk_diag_suppress:
    severity = es_none;
    break;
  case pk_diag_remark:
    severity = es_remark;
    break;
  case pk_diag_warning:
    severity = es_warning;
    break;
  case pk_diag_error:
    severity = es_discretionary_error;
    break;
  case pk_diag_default:
    severity = error_codes[(int)error_code].default_severity;
    break;
  default:
    unexpected_condition();
  }  /* switch */
  return severity;
}

#endif /* !STANDALONE_UTILITY_PROGRAM */

static void check_for_overridden_severity(an_error_code     error_code,
                                          an_error_severity *severity,
                                          a_source_position *pos)
/*
Determine whether this error code should have its severity
overridden by a value specified on the command line.  Diagnostics
may have their severity increased or decreased using this mechanism,
but diagnostics with a severity greater than es_discretionary_error
may not have their severity altered.  The source position is used to
determine what "diagnostic pragmas" are in effect at the given location
in the source.
*/
{
  if ((int)*severity <= (int)es_discretionary_error) {
    /* Default the new severity to the current severity for the error code. */
    an_error_severity new_severity =
                                 error_codes[(int)error_code].current_severity;

#if !STANDALONE_UTILITY_PROGRAM
    /* Do a quick check to see if the severity of this error code has ever been
       changed by a pragma (avoids a longer search process in most cases). */
    if (error_codes[(int)error_code].severity_changed_by_pragma) {
      /* If any "diagnostic pragmas" have been used, map the source location
         of the error to the appropriate location in the pragma_diag_list
         array, then search backwards to see if any prior pragma in the source
         has an effect on this diagnostic severity. */
      a_pragma_diag_elem *ptr = NULL;
      if (pos->seq == 0) {
        /* If the source position is null, it's likely that the position is
           that of a compiler-generated construct, so instead use a source
           position that is likely to be near code that is currently being
           compiled. */
        pos = &error_position;
      }  /* if */
      /* Find the appropriate location in pragma_diag_list. */
      a_pragma_diag_elem dummy = {pk_none, pos, 0};
      ptr = pragma_diag_list_lower_bound(&dummy);
      if (ptr != NULL) {
        /* Search backwards (i.e., towards the beginning of the list) to see
           if there is any pk_diag* pragma that refers to the error code at
           hand.  If we find a "diagnostic pop", skip to the corresponding
           "diagnostic push". */
        while (TRUE) {
          if (ptr->kind == pk_diagnostic) {
            if (ptr->is_pop) {
              if (ptr->variant.corresponding_push != NULL) {
                /* Skip to the corresponding "push" for this "pop". */
                ptr = ptr->variant.corresponding_push;
                continue;
              } else {
                /* There is no corresponding "push" for this "pop" (a warning
                   has previously been given).  Just ignore it and proceed
                   towards the top of the list. */
              }  /* if */
            }  /* if */
          } else if (ptr->variant.error_number == (int)error_code) {
            /* Update the severity according to this pragma. */
            new_severity = get_severity_from_pragma(ptr, error_code);
            break;
          }  /* if */
          if (ptr == pragma_diag_list) {
            /* At the beginning. */
            break;
          }  /* if */
          ptr = ptr->prev;
          check_assertion(ptr != NULL);
        }  /* while */
      }  /* if */
    }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    if (new_severity != es_default) *severity = new_severity;
  }  /* if */
}  /* check_for_overridden_severity */


static a_boolean check_severity(a_diagnostic_ptr	dp)
/*
Determine whether this message should have its severity overridden by a
value specified on the command line.  Compare the resulting error severity
with the threshold setting to see if this diagnostic should be issued.
*/
{
  an_error_severity	    error_threshold_to_use = es_none;

  error_threshold_to_use = error_threshold;
  if ((int)dp->severity >= (int)error_threshold) {
    /* Check whether we are inside a "system" include file in which
       warnings should be suppressed.  This test is only done if the message
       would be issued based on the current threshold. */
    if (seq_is_in_system_header(dp->position.seq)) {
      error_threshold_to_use = es_error;
#if !STANDALONE_UTILITY_PROGRAM
    } else if (curr_cmd_line_or_predef_macro_def != NULL) {
      /* We are processing a command-line or predefined macro definition.
         Warnings detected during this process are ignored. */
      error_threshold_to_use = es_discretionary_error;
#endif /* !STANDALONE_UTILITY_PROGRAM */
    }  /* if */
  }  /* if */
  check_assertion((int)error_threshold_to_use != (int)es_none);
  /* Return FALSE if the current severity is below the threshold. */
  return ((int)dp->severity >= (int)error_threshold_to_use);
}  /* check_severity */


a_boolean is_effective_error(an_error_code	error_code,
                             an_error_severity	severity,
                             a_source_position	*pos)
/*
Determine the severity at which a diagnostic specified by error_code, severity,
and pos would be issued.  Return TRUE if it would be issued as an error.
*/
{
  a_boolean	result;

  check_for_overridden_severity(error_code, &severity, pos);
  if ((int)severity >= (int)es_error) {
    result = TRUE;
  } else if (severity == es_discretionary_error &&
             !seq_is_in_system_header(pos->seq /*lint !e413*/)) {
    result = TRUE;
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_effective_error */


a_boolean is_effective_sfinae_error(an_error_code                 error_code,
                                    an_error_severity             severity,
                                    ARG_UNUSED a_source_position  *pos)
/*
This routine is similar to is_effective_error but is called when a SFINAE error
is detected.  If diag_override_does_not_affect_sfinae is FALSE, it is almost
equivalent to a call to is_effective_error, except that system headers do not
affect the effective severity.  But if it is TRUE then the determination is
based on the severity passed in (ignoring any overridden severities).  Return
TRUE if the diagnostic would be treated as an error.
*/
{
  a_boolean	result;

  if (!diag_override_does_not_affect_sfinae) {
    check_for_overridden_severity(error_code, &severity, pos);
  }  /* if */
  if ((int)severity >= (int)es_discretionary_error) {
    result = TRUE;
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_effective_sfinae_error */


a_boolean is_effective_diagnostic(an_error_code     error_code,
                                  an_error_severity severity,
                                  a_source_position *pos)
/*
Returns TRUE if a diagnostic with the specified error_code would be emitted
at the current error threshold, taking into account any overridden
severities.
*/
{
  a_boolean	result;

  check_for_overridden_severity(error_code, &severity, pos);
  if ((int)severity >= (int)es_error) {
    result = TRUE;
  } else if (severity == es_discretionary_error &&
             !seq_is_in_system_header(pos->seq /*lint !e413*/)) {
    result = TRUE;
  } else {
    result = (int)severity >= (int)error_threshold;
  }  /* if */
  return result;
}  /* is_effective_diagnostic */


#if !STANDALONE_UTILITY_PROGRAM

static a_source_position *context_position_for_instantiation(
				a_symbol_ptr		sym,
				a_source_position	*scope_stack_pos)
/*
Determine the position to be used as the point of instantiation of a
template for diagnostic purposes.  This is normally the scope_stack_pos,
but if that position represents the end-of-source position, then use the
position of the first reference of the instance specified by "sym".
*/
{
  a_const_char	*file_name;
  a_const_char	*full_name;
  a_line_number	line_number;
  a_boolean	at_end_of_source;

  a_source_position	*result_pos = scope_stack_pos;

  (void)conv_seq_to_file_and_line(scope_stack_pos->seq, &file_name, &full_name,
                                  &line_number, &at_end_of_source);
  if (at_end_of_source) {
    a_template_instance_ptr	tip = NULL;
    /* Get the template instance (if any) associated with this symbol. */
    if (sym->kind == (a_symbol_kind)sk_static_data_member) {
      tip = sym->variant.static_data_member.instance_ptr;
    } else if (is_function_symbol(sym)) {
      tip = sym->variant.routine.instance_ptr;
    }  /* if */
    if (tip != NULL) {
      /* If the symbol has a position of first reference, use that. */
      if (tip->pos_of_first_reference.seq != 0) {
        result_pos = &tip->pos_of_first_reference;
      }  /* if */
    }  /* if */
  }  /* if */
  return result_pos;
}  /* context_position_for_instantiation */


static a_boolean include_in_context_output(
			 a_scope_stack_entry_ptr ssep,
			 a_symbol_ptr	         *context_sym,
			 an_error_code		 *context_error_code,
			 a_source_position	 *context_source_pos,
			 a_boolean               add_detected_prefix)
/*
Return TRUE if this scope stack entry has context information that should
be processed, otherwise return FALSE.  When TRUE is returned *context_sym
is set to point to a symbol that provides the context information,
*context_error_code is set to the appropriate error code, and
context_source_pos (if not NULL) is set to the position to be reported.  When
add_detected_prefix is TRUE, the error code returned will refer to
a message that includes the text (e.g., "detected during ") that is
used when only a single line of context information is being supplied.
When multiple context lines are being displayed, the "detected during"
message appears by itself on a separate line.
*/
{
  a_boolean		result = FALSE;
  a_symbol_ptr		sym = NULL;
  an_error_code		error_code = ec_no_error;
  a_source_position	*pos = NULL;

  if (ssep->exclude_from_context_output) {
    /* Don't include this scope in the context output. */
  } else if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
    /* Template instantiations (except for prototype instantiations)
       need additional context information. */
    sym = ssep->instance_sym;
    /* If the instance symbol is NULL use the template symbol instead. */
    if (ssep->in_prototype_instantiation ||
        ssep->in_generic_definition) {
      /* Prototype instantiations are excluded from the context output. */
    } else if (sym == NULL) {
      sym = ssep->template_sym;
      if (sym->kind == (a_symbol_kind)sk_function_template) {
        error_code = add_detected_prefix ? 
                          ec_det_during_template_function_declaration_context :
                          ec_template_function_declaration_context;
      } else if (sym->kind == (a_symbol_kind)sk_class_template ||
                 sym->kind == (a_symbol_kind)sk_concept_template ||
                 sym->kind == (a_symbol_kind)sk_variable_template) {
        error_code = add_detected_prefix ?
                           ec_det_during_template_class_argument_list_context :
                           ec_template_class_argument_list_context;
      } else {
        unexpected_condition();
      }  /* if */
      result = TRUE;
    } else {
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
        result = TRUE;
        error_code = add_detected_prefix ?
                      ec_det_during_implicit_static_data_member_definition :
                      ec_implicit_static_data_member_definition;
      } else {
        error_code = add_detected_prefix ?
                               ec_det_during_template_instantiation_context :
                               ec_template_instantiation_context;
        result = TRUE;
      }  /* if */
      /* Determine the position to be used as the location of this
         instantiation. */
      pos = context_position_for_instantiation(sym, &ssep->source_position);
    }  /* if */
  } else if (ssep->kind == (a_scope_kind)sck_function) {
    /* Compiler generated functions need additional information. */
    if (ssep->assoc_routine->compiler_generated) {
      sym = (a_symbol_ptr)ssep->assoc_routine->source_corresp.assoc_info;
      if (symbol_is(sym, sk_member_function) &&
          func_sym_routine(sym)->is_inheriting_ctor &&
          func_sym_routine(sym)->assoc_template != NULL &&
          !func_sym_routine(sym)->is_template_function) {
        /* Inheriting constructors have unusual template information that
           does not accommodate the usual display of instantiation contexts,
           and information regarding their implicit instantiation is not
           likely to be helpful in any event.  Skip this entry. */
      } else {
        result = TRUE;
        error_code = add_detected_prefix ?
                            ec_det_during_compiler_generated_function_context :
                            ec_compiler_generated_function_context;
      }  /* if */
    }  /* if */
  }  /* if */
  if (result) {
    *context_sym = sym;
    *context_error_code = error_code;
    /* If some other position was determined above, use that.  Otherwise,
       use the position from the scope stack entry. */
    if (context_source_pos != NULL) {
      *context_source_pos = pos != NULL ? *pos : ssep->source_position;
    }  /* if */
  }  /* if */
#if CHECKING
  if (result && sym == NULL) {
    internal_error("include_in_context_output: no sym for context info");
  }  /* if */
#endif /* CHECKING */
  return result;
}  /* include_in_context_output */


static int bucket_for_diag(an_error_code		error_code,
			   an_error_severity	severity,
			   a_source_position	*error_pos)
/*
Compute the hash table bucket to be used for this diagnostic.
*/
{
  unsigned long	value;
  int		bucket;

  value = (unsigned long)error_code;
  value *= ((unsigned long)severity + 1);
  value *= ((unsigned long)error_pos->seq + 1);
  value *= ((unsigned long)error_pos->column + 1);
  bucket = (int)(value % RECORDED_DIAG_TABLE_SIZE);
  return bucket;
}  /* bucket_for_diag */


void record_prototype_diagnostic(
				an_error_code		error_code,
				an_error_severity	severity,
				a_source_position	*error_pos)
/*
This diagnostic is being issued for a prototype instantiation.
Make a record of the diagnostic so that we can find it later to
suppress duplicate diagnostics.  This is also used to record information
about diagnostics issued during disambiguation.
*/
{
  int				bucket;
  a_recorded_diagnostic_ptr	rdp;

  bucket = bucket_for_diag(error_code, severity, error_pos);
  rdp = (a_recorded_diagnostic_ptr)alloc_fe(sizeof(a_recorded_diagnostic));
  rdp->error_code = error_code;
  rdp->severity = severity;
  rdp->error_pos = *error_pos;
  rdp->next = recorded_diagnostic_table[bucket];
  rdp->scope_of_prev_check = NO_SCOPE_NUMBER;
  rdp->number_of_times_suppressed = 0;
  recorded_diagnostic_table[bucket] = rdp;
}  /* record_prototype_diagnostic */


a_boolean find_prototype_diagnostic(
				an_error_code		error_code,
				an_error_severity	severity,
				a_source_position	*error_pos)
/*
This diagnostic is being issued for a real instantiation.  Check
whether a matching diagnostic was issued during a prototype instantiation.
Return TRUE if one is found.
*/
{
  a_boolean			found = FALSE;
  int				bucket;
  a_recorded_diagnostic_ptr	rdp;

  bucket = bucket_for_diag(error_code, severity, error_pos);
  rdp = recorded_diagnostic_table[bucket];
  for (; rdp != NULL; rdp = rdp->next) {
    if (rdp->error_code == error_code &&
        rdp->severity == severity &&
        rdp->error_pos.seq == error_pos->seq &&
        rdp->error_pos.column == error_pos->column) {
      a_scope_stack_entry_ptr  ssep = &scope_stack_top();
      a_scope_number           curr_scope = ssep->number;
      found = TRUE;
      /* Check whether a given diagnostic is suppressed a large number of
         times from the same scope.  This is used to prevent an infinite
         loop if there is an error recovery problem.  If the same diagnostic
         is issued many times, discontinue the suppression so that the error
         limit will be reached.  Suppress this test for template instantiation
         scopes because their number gets reused so the references may not
         be from the same instantiation. */
      if (rdp->scope_of_prev_check == curr_scope &&
          !scope_is(ssep, sck_template_instantiation)) {
        if (++(rdp->number_of_times_suppressed) > error_limit) found = FALSE;
      } else {
        /* A different scope.  Reset the count. */
        rdp->scope_of_prev_check = curr_scope;
        rdp->number_of_times_suppressed = 0;
      }  /* if */
      break;
    }  /* if */
  }  /* for */
  return found;
}  /* find_prototype_diagnostic */


static a_boolean diagnostic_already_issued_for_prototype(
						a_diagnostic_ptr	dp)
/*
This routine is used to prevent duplication of diagnostics in templates and
other contexts that may be parsed multiple times (e.g., for disambiguation).
When a diagnostic is issued during a prototype instantiation a record is kept
based on the error code, severity, and position.  If a matching diagnostic is
issued during a real instantiation, it is suppressed.

Return TRUE if the diagnostic should be suppressed.
*/
{
  a_boolean		suppress_diagnostic = FALSE;

  if (depth_scope_stack == NO_SCOPE_DEPTH) {
    /* The scope stack is empty, don't check further (probably a
       command-line error. */
  } else if (find_prototype_diagnostic(dp->error_code, dp->severity,
                                       &dp->position)) {
    suppress_diagnostic = TRUE;
  } else if (is_template_dependent_context() ||
#if MICROSOFT_EXTENSIONS_ALLOWED
             scope_stack_top().in_generic_definition ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
             scope_stack_top().in_tentative_decl ||
             scope_stack_top().in_disambiguation) {
    record_prototype_diagnostic(dp->error_code, dp->severity, &dp->position);
  }  /* if */
  return suppress_diagnostic;
}  /* diagnostic_already_issued_for_prototype */


static a_boolean diagnostic_already_issued_for_diag_once(
						a_diagnostic_ptr	dp)
/*
This routine records the fact that a given diagnostic has been issued and
also checks whether this occurrence of the diagnostic should be suppressed
because of the use of the "once" diagnostic control.  Return TRUE if the
diagnostic should be suppressed.
*/
{
  a_boolean		result = FALSE;

  if ((int)dp->severity <= (int)es_discretionary_error &&
      error_codes[(int)dp->error_code].once) {
    result = error_codes[(int)dp->error_code].diagnostic_issued;
  }  /* if */
  error_codes[(int)dp->error_code].diagnostic_issued = TRUE;
  return result;
}  /* diagnostic_already_issued_for_diag_once */

#if GNU_EXTENSIONS_ALLOWED

void f_report_gnu_cpp11_extensions_if_needed(a_source_position  *pos,
                                             an_error_code      error_code)
/*
If the given position is not in a system header file, and the given
diagnostic has not been issued yet, issue that diagnostic now at the given
position.  (This function should only be called through the macro
report_gnu_cpp11_extension_if_needed.)
*/
{
  if (!error_codes[(int)error_code].diagnostic_issued &&
      is_effective_diagnostic(error_code, es_warning, pos) &&
      !cmd_line_option_inhibits_gnu_cpp11_extension_warning(error_code) &&
      !seq_is_in_system_header(pos->seq)) {
    pos_warning(error_code, pos);
  }  /* if */
}  /* f_report_gnu_cpp11_extensions_if_needed */

#endif /* GNU_EXTENSIONS_ALLOWED */
#endif /* !STANDALONE_UTILITY_PROGRAM */

static void form_reflection(a_diag_fill_in_ptr	dfip)
/*
Format a string that represents the reflection value dfip into the message
buffer.
*/
{
  a_reflection_value       rv = dfip->variant.reflection;
#if !STANDALONE_UTILITY_PROGRAM
  a_source_correspondence  *scp = source_corresp_for_il_entry(rv.entity.ptr,
                                                              rv.entity.kind);

  if (scp != NULL && scp->assoc_info != NULL) {
    form_symbol_name_for_error((a_symbol*)scp->assoc_info);
  } else
#endif /* !STANDALONE_UTILITY_PROGRAM */
  /* Do not insert code here. */
  {
    form_reflection(rv, &octl);
  }  /* if */
}  /* form_reflection */


static void form_module(a_diag_fill_in_ptr	dfip)
/*
Format a string that represents the module value dfip into the message buffer.
*/
{
  a_module_ptr mod = dfip->variant.mod;

  /* FIXME: We probably want to add error codes to aid in translation. */
  switch (mod->kind) {
    case mk_none:
      add_string_to_text_buffer(msg_buffer, "unknown module");
      break;
    case mk_header_unit:
      { a_boolean     is_sys_include = mod->variant.header_unit.is_sys_include;
        a_string_view header_name = header_unit_name_of(mod);

        add_string_to_text_buffer(msg_buffer, "header unit ");
        annotate_diagnostic(msg_buffer, da_quote);
        if (is_sys_include) {
          add_char_to_text_buffer(msg_buffer, '<');
        } else {
          add_char_to_text_buffer(msg_buffer, '"');
        }  /* if */
        add_string_to_text_buffer(msg_buffer, header_name.start());
        if (is_sys_include) {
          add_char_to_text_buffer(msg_buffer, '>');
        } else {
          add_char_to_text_buffer(msg_buffer, '"');
        }  /* if */
        annotate_diagnostic(msg_buffer, da_reset);
      }
      break;
    case mk_unit:
    case mk_unit_partition:
      { a_string mod_name = module_full_name_of(mod);

        add_string_to_text_buffer(msg_buffer, "module ");
        annotate_diagnostic(msg_buffer, da_quote);
        add_char_to_text_buffer(msg_buffer, '"');
        add_string_to_text_buffer(msg_buffer, mod_name.as_temp_characters());
        add_char_to_text_buffer(msg_buffer, '"');
        annotate_diagnostic(msg_buffer, da_reset);
      }
      break;
    default_is_unexpected();
  }  /* switch */
}  /* form_module */


static void process_fill_in(a_diagnostic_ptr	dp,
			    char		fill_in_char,
			    char		*options,
			    int			fill_in_seq)
/*
Create a fill-in for the diagnostic specified by "dp".  fill_in_char
is the character that followed the "%" in the message text.  options
provides any additional option characters that may have been specified.
fill_in_seq specifies the fill-in number that was specified, or 1 if there
was no fill-in number.  For example, if the fill in was "%s2q", fill_in_char
is "s", options is "q", and fill_in_seq is "2".  The options string is
null-terminated.
*/
{
  a_boolean		add_quotes = FALSE;
  a_diag_fill_in_kind	kind = dfk_number;
  a_diag_fill_in_ptr	dfip;

  /* Determine the fill-in kind associated with this fill-in character. */
  switch (fill_in_char) {
    case 'd': kind = dfk_number;          break;
    case 'u': kind = dfk_unsigned_number; break;
    case 'n': kind = dfk_symbol;          break;
    case 'p': kind = dfk_position;        break;
    case 's': kind = dfk_string;          break;
    case 't': kind = dfk_type;            break;
    case 'T': kind = dfk_template_args;   break;
    case 'r': kind = dfk_reflection;      break;
    case 'm': kind = dfk_module;          break;
    default:
      unexpected_condition_str2("process_fill_in:", "bad fill-in kind");
  }  /* switch */
  /* Find the fill-in associated with the specified fill-in kind and
     sequence number. */
  for (dfip = dp->fill_in_head; dfip != NULL; dfip = dfip->next) {
    if (dfip->kind == kind) {
      /* We've found an entry of the right kind.  See if it is the one
         specified by the sequence number. */
      fill_in_seq--;
      if (fill_in_seq == 0) break;
    }  /* if */
  }  /* for */
#if CHECKING
  if (dfip == NULL) {
    /* When this condition is reached, typically the diagnostic was constructed
       without a fill-in the error message text is expecting (or alternatively
       there is a type mismatch between the fill-in and the string attempting
       to use it -- e.g., use of a "%d" fill-in for an unsigned integer). */
    a_string err_msg("placeholder (",
                     a_string_view(&fill_in_char, 1), fill_in_seq,
                     ") in error string: \"",
                     error_text(dp->error_code),
                     "\" did not match any of the provided fill-ins");

    unexpected_condition_str2("process_fill_in:",
                              err_msg.as_temp_characters());
  }  /* if */
#endif /* CHECKING */
#if CHECKING
  dfip->fill_in_used = TRUE;
#endif /* CHECKING */
  /* Evaluate the option string. */
  switch (kind) {
    case dfk_symbol:
      while (*options != '\0') {
        char	opt = *options;
        /* Check for formatting options. */
        if (opt == 'f') {
          /* Display complete type and object name. */
          dfip->variant.symbol.full_type = TRUE;
        } else if (opt == 'o') {
          /* Display only the entity name. */
          dfip->variant.symbol.name_only = TRUE;
        } else if (opt == 'p') {
          /* Display function parameters with the name. */
          dfip->variant.symbol.force_function_params = TRUE;
        } else if (opt == 't') {
          /* Use the special template formatting for classes.  This
             also implies the 'f' option. */
          dfip->variant.symbol.force_template_name_output = TRUE;
          dfip->variant.symbol.full_type = TRUE;
        } else if (opt == 'a') {
          /* Display the entity name along with associated template
             arguments. */
          dfip->variant.symbol.name_only = TRUE;
          dfip->variant.symbol.template_args = TRUE;
        } else if (opt == 'd') {
          /* Display the declaration position following the entity name. */
          dfip->variant.symbol.decl_pos = TRUE;
        } else if (opt == 'T') {
          /* Display the translation unit under certain conditions. */
          dfip->variant.symbol.trans_unit = TRUE;
        }  /* if */
        options++;
      }  /* while */
      break;
    case dfk_string:
      /* A string can have the "q" (surround with quotes). */
      while (*options != '\0') {
        switch (*options) {
          case 'q': add_quotes = TRUE; break;
	  default:
            unexpected_condition_str("process_fill_in: bad option");
        }  /* switch */
        options++;
      }  /* while */
      break;
    default:
      /* For the cases not handled above, no options are expected. */
      if (*options != '\0') {
        unexpected_condition_str("process_fill_in: bad option");
      }  /* if */
      break;
  }  /* switch */
  /* Do the actual generation of the fill-in text. */
  if (add_quotes) {
    add_char_to_text_buffer(msg_buffer, '"');
    annotate_diagnostic(msg_buffer, da_quote);
  }  /* if */
  switch (kind) {
    case dfk_number:
      /* A numeric fill-in.  Add it to the message buffer. */
      { a_number_buffer number(dfip->variant.number);

        add_string_to_text_buffer(msg_buffer, number.as_temp_characters());
      }
      break;
    case dfk_unsigned_number:
      /* An unsigned numeric fill-in.  Add it to the message buffer. */
      { a_number_buffer number(dfip->variant.unsigned_number);

        add_string_to_text_buffer(msg_buffer, number.as_temp_characters());
      }
      break;
#if !STANDALONE_UTILITY_PROGRAM
    case dfk_position:
      /* A position fill-in.  Add it to the message buffer. */
      form_source_position(&dfip->variant.position, dp, "", "", "");
      break;
    case dfk_symbol:
      /* A symbol fill-in.  Add it to the message buffer. */
      form_symbol_summary(dp, dfip);
      break;
#endif /* !STANDALONE_UTILITY_PROGRAM */
    case dfk_string:
      /* A string fill-in.  Add it to the message buffer. */
      add_string_with_length_to_text_buffer(msg_buffer,
                                            dfip->variant.string.start,
                                            dfip->variant.string.len);
      break;
    case dfk_type:
      /* A type fill-in. */
      form_type_summary(dfip);
      break;
    case dfk_template_args:
      /* A template argument list fill-in. */
      form_template_arg_list(dfip);
      break;
    case dfk_reflection:
      /* A reflection fill-in. */
      form_reflection(dfip);
      break;
    case dfk_module:
      /* A module fill-in. */
      form_module(dfip);
      break;
    default:
      break;
  }  /* switch */
  if (add_quotes) {
    annotate_diagnostic(msg_buffer, da_reset);
    add_char_to_text_buffer(msg_buffer, '"');
  }  /* if */
}  /* process_fill_in */


static void format_output_line(sizeof_t first_indent,
                               sizeof_t continuation_indent)
/*
Write the specified diagnostic constructed in prefix_buffer to the
write_diagnostic_buffer.  Wrap the output unless do_not_wrap_diagnostics
is TRUE.  first_indent is the number of characters to indent the first
line of the message, continuation_indent is the number of characters to
indent subsequent lines when the output wraps to more than one line.
*/
{
  a_const_char	*curr_char;
  a_const_char	*segment_start;
  sizeof_t	usable_line_length;
  sizeof_t	indent = first_indent;
  sizeof_t	length = prefix_buffer->size - 1;
  sizeof_t	i;
  a_boolean	wrapping = !do_not_wrap_diagnostics && !brief_diagnostics;

  curr_char = prefix_buffer->buffer;
  if (annotate_diagnostics) {
    /* Diagnostics are annotated to identify various pieces so they can
       be highlighted to make reading diagnostics easier.  Currently,
       "colorizing" is the only consumer of these annotations, but
       customers can add their own means of highlighting and would need
       to add such code here.  If using colorized diagnostics, replace the
       annotations in the diagnostic error message with escape sequences to
       do the colorization.  For wrapping purposes, don't count the
       characters used as annotations or the SGR strings used to perform
       the actual colorization.  If not doing colorization, the
       annotations are removed. */
    a_const_char *last_space = NULL;
    sizeof_t     last_space_index = 0;
    sizeof_t     chars_left = 0;
    a_boolean    new_line = TRUE;
    a_diagnostic_annotation_kind
                 save_last_annotation = da_reset, last_annotation = da_reset;
    /* Assumes the error message is null terminated. */
    check_assertion(prefix_buffer->buffer[prefix_buffer->size-1] == '\0');
    /*lint --e{850} */
    for (; *curr_char != '\0'; curr_char++) {
      while (*curr_char == DIAG_ANNOTATION_INDICATOR) {
        if (colorize_diagnostics) {
          /* Add the appropriate colorization SGR characters and keep track
             of what annotation was last used. */
          last_annotation = (a_diagnostic_annotation_kind)curr_char[1];
          add_colorization_characters(last_annotation);
        }  /* if */
        /* Skip over the two byte annotation. */
        curr_char += 2;
      }  /* while */
      if (*curr_char == '\0') break;
      if (new_line) {
        /* Starting a new line; put out indentation and reset counter. */
        if (last_annotation != da_reset) {
          /* We're in the middle of some colorization; temporarily turn it off
             so the indentation is not colorized. */
          add_colorization_characters(da_reset);
        }  /* if */
        for (i = 0; i < indent; ++i) {
          add_char_to_text_buffer(write_diagnostic_buffer, ' ');
        }  /* for */
        if (last_annotation != da_reset) {
          add_colorization_characters(last_annotation);
        }  /* if */
        last_space = NULL;
        check_assertion(diagnostic_line_length > (int)indent);
        chars_left = (sizeof_t)diagnostic_line_length - indent;
        new_line = FALSE;
      }  /* if */
      /* Tentatively add this character to the buffer (it may be "removed"
         later for wrapping purposes). */
      add_char_to_text_buffer(write_diagnostic_buffer, *curr_char);
      if (wrapping) {
        if (--chars_left == 0) {
          /* Time to wrap.  Wrap at a space if possible. */
          new_line = TRUE;
          if (*curr_char == ' ') {
            /* Perfect.  Suppress the trailing space. */
            write_diagnostic_buffer->size--;
          } else {
            /* Process any annotations (since they don't take up any width)
               before looking at the "next" character. */
            while (curr_char[1] == DIAG_ANNOTATION_INDICATOR) {
              if (colorize_diagnostics) {
                /* Add the appropriate colorization SGR characters and keep
                   track of what annotation was last used. */
                last_annotation = (a_diagnostic_annotation_kind)curr_char[2];
                add_colorization_characters(last_annotation);
              }  /* if */
              /* Skip over the two byte annotation. */
              curr_char += 2;
            }  /* while */
            if (curr_char[1] == '\0') {
              /* End of the buffer. */
              new_line = FALSE;
            } else if (curr_char[1] == ' ') {
              /* Next character is a space, wrap there. */
              curr_char++;
            } else if (last_space != NULL) {
              /* Wrap at the last space we saw. */
              curr_char = last_space;
              write_diagnostic_buffer->size = last_space_index - 1;
              last_annotation = save_last_annotation;
            } else {
              /* Wraps in the middle of text. */
            }  /* if */
          }  /* if */
          if (new_line) {
            add_char_to_text_buffer(write_diagnostic_buffer, '\n');
          }  /* if */
          /* Emit the proper indentation and re-set for new line. */
          indent = continuation_indent;
        } else if (*curr_char == ' ') {
          /* Save the location of the last space we've seen. */
          last_space = curr_char;
          last_space_index = write_diagnostic_buffer->size;
          save_last_annotation = last_annotation;
        }  /* if */
      }  /* if */
    }  /* for */
    add_char_to_text_buffer(write_diagnostic_buffer, '\n');
  } else {
    segment_start = curr_char;
    for (;;) {
      /* Compute the number of characters that will fit on a line taking into
         account any indentation that is required. */
      usable_line_length = (sizeof_t)diagnostic_line_length - indent;
      /* Put out the required indentation. */
      for (i = 0; i < indent; ++i) {
        add_char_to_text_buffer(write_diagnostic_buffer, ' ');
      }  /* for */
      if (length > usable_line_length && wrapping) {
        /* Output as much of the string as will fit on a line.  Wrap
           at a blank, if possible; otherwise, just wrap at the end of the
           line. */
        sizeof_t segment_length = usable_line_length;

        curr_char = segment_start + segment_length - 1;
        /* If the character after the end is a blank, wrap on that one. */
        if (curr_char[1] == ' ') curr_char++;
        /* Look backward from the end of the line to find a blank. */
        while (curr_char > segment_start && *curr_char != ' ') curr_char--;
        /* If we found a blank, compute the length of the string up to the
           character before the blank. */
        if (*curr_char == ' ') {
          segment_length = (sizeof_t)(curr_char - segment_start);
        }  /* if */
        add_to_text_buffer(write_diagnostic_buffer, segment_start,
                           segment_length);
        add_char_to_text_buffer(write_diagnostic_buffer, '\n');
        /* Skip over the blank before starting the next segment. */
        if (*curr_char == ' ') segment_length++;
        length -= segment_length;
        segment_start += segment_length;
      } else {
        add_string_to_text_buffer(write_diagnostic_buffer, segment_start);
        add_char_to_text_buffer(write_diagnostic_buffer, '\n');
        break;
      }  /* if */
      /* Set the indentation to be used for subsequent lines. */
      indent = continuation_indent;
    }  /* for */
  }  /* if */
}  /* format_output_line */


static void display_message(a_diagnostic_ptr	dp)
/*
Output the message contained in the message buffer, and to the raw
listing file (if needed).
*/
{
  sizeof_t first_indent;
  sizeof_t continuation_indent;

  /* Determine the indentation to be used for this message. */
  if (dp->kind == dck_primary) {
    first_indent = NORMAL_DIAG_INDENT;
    continuation_indent = INDENT_AMOUNT;
  } else if (dp->kind == dck_context) {
    /* The first context line uses the normal indent, subsequent ones use
       the list indent. */
    if (dp == dp->primary_diag->context.head) {
      first_indent = INDENT_AMOUNT;
      continuation_indent = first_indent + INDENT_AMOUNT;
    } else {
      first_indent = LIST_DIAG_INDENT;
      continuation_indent = first_indent + INDENT_AMOUNT;
    }  /* if */
  } else if (dp->kind == dck_macro_context) {
    first_indent = MACRO_CONTEXT_INDENT;
    continuation_indent = first_indent + INDENT_AMOUNT;
  } else {
    first_indent = LIST_DIAG_INDENT;
    continuation_indent = first_indent + INDENT_AMOUNT;
  }  /* if */
  /* Append the message line to the prefix buffer for output.  The
     message buffer is kept separately so it can be used, if needed,
     for output to the raw listing file. */
  add_to_text_buffer(prefix_buffer, msg_buffer->buffer, msg_buffer->size);
  add_char_to_text_buffer(msg_buffer, '\0');
  add_char_to_text_buffer(prefix_buffer, '\0');
  format_output_line(first_indent, continuation_indent);
#if !STANDALONE_UTILITY_PROGRAM
  if (f_raw_listing != NULL && dp->kind != dck_macro_context) {
    write_diag_to_raw_listing(dp);
  }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
  reset_text_buffer(msg_buffer);
}  /* display_message */

#if FULLY_RESOLVED_MACRO_POSITIONS

static void write_source_line_for_macro(a_diagnostic_ptr	dp)
/*
Write the source line of the macro invocation, if needed.
*/
{
  if (macro_positions_in_diagnostics) {
    if (dp->diag_header_pos.seq != dp->position.seq) {
      write_source_line(&dp->position, &dp->source_info);
    }  /* if */
  }  /* if */
}  /* write_source_line_for_macro */

#else /* !FULLY_RESOLVED_MACRO_POSITIONS */

#define write_source_line_for_macro(dp)  /* Nothing */

#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

static void write_message_to_buffer(a_diagnostic_ptr dp)
/*
Write the textual message for a given diagnostic entry pointer and its fill-ins
to the msg_buffer.
*/
{
  constexpr int max_options = 30;
  a_const_char  *curr_char;
  a_const_char  *msg_ptr;
  char          options[max_options];

  msg_ptr = error_text(dp->error_code);
  curr_char = msg_ptr;
  for (;;) {
    a_const_char	*segment_start;
    a_const_char	*segment_end;
    int			fill_in_seq;
    char		fill_in_char;
    int			opt_pos;
    a_boolean		is_double_percent = FALSE;
    segment_start = curr_char;
    /* Find the end of this segment.  This is the next fill-in, or the end
       of the message. */
    while (*curr_char != '\0' && *curr_char != '%') {
      curr_char++;
    }  /* while */
    if (*curr_char == '%' && curr_char[1] == '%') {
      is_double_percent = TRUE;
      curr_char++;
    }  /* if */
    /* The character before the one we found marks the end of the segment. */
    segment_end = curr_char - 1;
    if (segment_end >= segment_start) {
      /* The text segment is non-empty.  Add these characters to the
         message buffer. */
      sizeof_t length = (sizeof_t)(segment_end - segment_start + 1);
      add_to_text_buffer(msg_buffer, segment_start, length);
    }  /* if */
    /* We have reached the end of the string -- exit the loop. */
    if (*curr_char == '\0') break;
    /* We should be processing a fill-in. */
    check_assertion(*curr_char == '%');
    curr_char++;
    if (is_double_percent) {
      /* The message contains a "%%" -- discard the second "%". */
    } else if (*curr_char == '[') {
      /* A label fill in of the form "%[label_name]".  Replace the label
         fill-in with the appropriate value. */
      a_const_char		*label_end;
      a_const_char		*label_text;
      a_label_fill_in_entry	*lfie;
      label_end = mbc_strchr(curr_char, ']');
      check_assertion(label_end != NULL);
      lfie = get_label_fill_in_entry(curr_char + 1,
                                     (sizeof_t)(label_end - curr_char - 1));
      label_text = error_text(*(lfie->test) ? lfie->true_value
                                            : lfie->false_value);
      add_string_to_text_buffer(msg_buffer, label_text);
      /* Move to the character after the ']'. */
      curr_char = label_end + 1;
    } else {
      fill_in_char = *curr_char++;
      /* Scan any option characters that may follow.  These may be letters or
         a digit that specifies which of several fill-ins of a given type is
         to be used (e.g., %s2 for the second string fill-in).  Note that
         isdigit, etc. are not used here because they could return TRUE
         if the fill-in is followed by a multibyte character. */
      opt_pos = 0;
      fill_in_seq = 1;
      for (;;) {
        if (*curr_char >= '0' && *curr_char <= '9') {
          fill_in_seq = *curr_char - '0';
        } else if ((*curr_char >= 'a' && *curr_char < 'z') ||
                   (*curr_char >= 'A' && *curr_char < 'Z')) {
          options[opt_pos] = *curr_char;
          opt_pos++;
          check_assertion_str2(opt_pos < max_options,
                               "construct_text_message:",
                               "too many option characters");
        } else {
          break;
        }  /* if */
        curr_char++;
      }  /* for */
      /* Terminate the string of options. */
      options[opt_pos] = '\0';
      process_fill_in(dp, fill_in_char, options, fill_in_seq);
    }  /* if */
  }  /* for */
}  /* write_message_to_buffer */


static void construct_text_message(a_diagnostic_ptr dp)
/*
Convert a diagnostic entry (dp) and its fill-ins into a text string.  Write
out that string as a diagnostic, and also write out the associated source
line with an indication of the diagnostic position.  If there are
sub-messages, output those messages as well.  Output any context messages
that might be required.
*/
{
  reset_text_buffer(prefix_buffer);
  if (dp->kind == dck_primary) {
    /* Display the file name, position, etc. */
    add_primary_prefix(dp);
  }  /* if */
  /* Get the error message text. */
  write_message_to_buffer(dp);
#if CHECKING
  /* Make sure all of the fill-ins were used. */
  { a_diag_fill_in_ptr dfip;
    for (dfip = dp->fill_in_head; dfip != NULL; dfip = dfip->next) {
      if (dfip->fill_in_used) {
        continue;
      }  /* if */

      a_string err_msg("not all fill-ins used for error string: \"",
                       error_text(dp->error_code), "\"");

      unexpected_condition_str2("construct_text_message:",
                                err_msg.as_temp_characters());
    }  /* for */
  }
#endif /* CHECKING */
  /* Display the formatted message. */
  display_message(dp);
  /* If there are sub-messages, process them now. */
  if (dp->kind == dck_primary) {
    a_diagnostic_ptr	sub_dp;
    for (sub_dp = dp->sub_msgs.head; sub_dp != NULL; sub_dp = sub_dp->next) {
      construct_text_message(sub_dp);
    }  /* for */
  }  /* if */
  if (dp->kind == dck_primary && !brief_diagnostics) {
    /* If this is a primary diagnostic, and we are able to fetch the source
       line, display it now. */
    write_source_line(&dp->diag_header_pos, &dp->diag_header_source_info);
  }  /* if */
  /* If there are context messages, process them now. */
  if (dp->kind == dck_primary && dp->severity != es_more_info) {
    a_diagnostic_ptr	sub_dp;
    /* Output macro context diagnostics. */
    for (sub_dp = dp->macro_context.head; sub_dp != NULL;
         sub_dp = sub_dp->next) {
      construct_text_message(sub_dp);
    }  /* for */
    if (dp->kind == dck_primary && !brief_diagnostics) {
      /* Display the macro invocation source line, if needed. */
      write_source_line_for_macro(dp);
    }  /* if */
    /* If there are more-info, process them now. */
    if (dp->kind == dck_primary) {
      a_diagnostic_ptr	mi_dp;
      for (mi_dp = dp->more_info.head; mi_dp != NULL; mi_dp = mi_dp->next) {
        /* Unlike other messages, the primary diagnostic is not known when
           the more_info messages are created. */
        mi_dp->primary_diag = dp;
        construct_text_message(mi_dp);
      }  /* for */
    }  /* if */
    /* Output instantiation context messages. */
    for (sub_dp = dp->context.head; sub_dp != NULL; sub_dp = sub_dp->next) {
      construct_text_message(sub_dp);
    }  /* for */
    if (!brief_diagnostics) {
      /* Put out an extra space line after the error, for clarity.  The
         space is suppressed if a context message is to follow since the
         space should follow the context. */
      add_char_to_text_buffer(write_diagnostic_buffer, '\n');
    }  /* if */
    add_char_to_text_buffer(write_diagnostic_buffer, '\0');
    fputs(write_diagnostic_buffer->buffer, f_error);
    (void)fflush(f_error);
  }  /* if */
}  /* construct_text_message */

#if !STANDALONE_UTILITY_PROGRAM

static void write_sarif_rule_id(a_diagnostic_ptr dp)
/*
Write a SARIF "result object"."ruleId property" for the given diagnostic
pointer to the write_diagnositic_buffer.
*/
{
  a_number_buffer num_buffer((size_t)dp->error_code);

  add_string_to_text_buffer(write_diagnostic_buffer, "\"EC");
  add_string_to_text_buffer(write_diagnostic_buffer,
                            num_buffer.as_temp_characters());
  add_string_to_text_buffer(write_diagnostic_buffer, "\"");
}  /* write_sarif_rule_id */


static void write_sarif_level(a_diagnostic_ptr dp)
/*
Write a SARIF "result object"."level property" for the given diagnostic pointer
to the write_diagnositic_buffer.
*/
{
  an_error_severity reported_severity = determine_reported_severity(dp);

  switch (reported_severity) {
    case es_remark:
      add_string_to_text_buffer(write_diagnostic_buffer, "\"note\"");
      break;
    case es_command_line_warning:
    case es_warning:
      add_string_to_text_buffer(write_diagnostic_buffer, "\"warning\"");
      break;
    case es_catastrophe:
    case es_command_line_error:
    case es_discretionary_error:
    case es_error:
    case es_internal_error:
      add_string_to_text_buffer(write_diagnostic_buffer, "\"error\"");
      break;
    case es_default:
    case es_more_info:
    case es_none:
    case es_once:
      unexpected_condition_str("write_sarif_level: unexpected severity");
    default_is_unexpected();
  }  /* switch */
}  /* write_sarif_level */


static void add_json_escaped_string_to_text_buffer(a_const_char *str)
/*
Add the given null-terminated string to the write_diagnostic_buffer with
escaping on characters that would cause the string to terminate prematurely.
Note this function does not add the surrounding double quotes for the string,
this is the caller's responsibility.
*/
{
  for (a_const_char *a_char = str; *a_char != '\0'; ++a_char) {
    /* Add an escape \ if necessary. */
    switch (*a_char) {
      case '"':
      case '\\':
        add_char_to_text_buffer(write_diagnostic_buffer, '\\');
        break;
      default:
        break;
    }  /* switch */
    /* Copy the character the to write_diagnostic_buffer. */
    add_char_to_text_buffer(write_diagnostic_buffer, *a_char);
  }  /* for */
}  /* add_json_escaped_string_to_text_buffer */


static void write_sarif_message(a_diagnostic_ptr dp)
/*
Write a SARIF "message object" for the given diagnostic pointer to the
write_diagnositic_buffer.  Note that the msg_buffer will be updated and then
reset in this process, thus it's important that the msg_buffer is not already
in use.
*/
{
  add_string_to_text_buffer(write_diagnostic_buffer, "{\"text\":\"");
  /* Form the message in the msg_buffer. */
  write_message_to_buffer(dp);
  add_char_to_text_buffer(msg_buffer, '\0');
  /* Copy the message buffer into the write_diagnostic_buffer with the required
     JSON string escapes. */
  add_json_escaped_string_to_text_buffer(msg_buffer->buffer);
  reset_text_buffer(msg_buffer);
  add_string_to_text_buffer(write_diagnostic_buffer, "\"}");
}  /* write_sarif_message */


static void write_sarif_artifact_location(a_const_char  *file_name)
/*
Write a SARIF "artifactLocation object" for the given file name to the
write_diagnositic_buffer.
*/
{
  add_string_to_text_buffer(write_diagnostic_buffer, "{\"uri\":\"file://");
  /* SARIF file names should always be resolved paths (i.e., should not contain
     relative path markers like ".."). */
  file_name = normalize_file_name(file_name);
  add_string_to_text_buffer(write_diagnostic_buffer, file_name);
  add_string_to_text_buffer(write_diagnostic_buffer, "\"}");
}  /* write_sarif_artifact_location */


static void write_sarif_region(a_line_number   line_number,
                               a_column_number column_number)
/*
Write a SARIF "physicalLocation object"."region property" for the given line
and column numbers to the write_diagnositic_buffer.
*/
{
  a_number_buffer num_buffer(line_number);

  add_string_to_text_buffer(write_diagnostic_buffer, "{\"startLine\":");
  add_string_to_text_buffer(write_diagnostic_buffer,
                            num_buffer.as_temp_characters());
  /* SARIF does not have a concept of "no column"; if no column information is
     present, omit the startColumn (though notably, this is interpreted in the
     specification as equivalent to a startColumn value of 1). */
  if (column_number != 0) {
    num_buffer.reset_to(column_number);

    add_string_to_text_buffer(write_diagnostic_buffer, ",\"startColumn\":");
    add_string_to_text_buffer(write_diagnostic_buffer,
                              num_buffer.as_temp_characters());
  }  /* if */
  add_char_to_text_buffer(write_diagnostic_buffer, '}');
}  /* write_sarif_region */


static void write_sarif_physical_location(a_source_position_ptr error_pos)
/*
Write a SARIF "location object"."physicalLocation property" for the given error
position to the write_diagnositic_buffer.
*/
{
  a_const_char  *file_name;
  a_const_char  *full_name;
  a_line_number line_number;
  a_boolean     at_end_of_source;

  (void)conv_seq_to_file_and_line(error_pos->seq, &file_name,
                                  &full_name, &line_number,
                                  &at_end_of_source);
  add_string_to_text_buffer(write_diagnostic_buffer,
                            "{\"artifactLocation\":");
  write_sarif_artifact_location(file_name);
  add_string_to_text_buffer(write_diagnostic_buffer, ",\"region\":");
  write_sarif_region(line_number, error_pos->column);
  add_char_to_text_buffer(write_diagnostic_buffer, '}');
}  /* write_sarif_physical_location */


static void write_sarif_locations(a_source_position_ptr error_pos)
/*
Write a SARIF "result object"."locations property" for the given source
position pointer to the write_diagnositic_buffer.
*/
{
  add_string_to_text_buffer(write_diagnostic_buffer,
                            "[{\"physicalLocation\":");
  write_sarif_physical_location(error_pos);
  add_string_to_text_buffer(write_diagnostic_buffer, "}]");
}  /* write_sarif_locations */


static void write_sarif_related_location(a_diagnostic_ptr dp)
/*
Write a SARIF "result object"."relatedLocations property" for the given
diagnostic pointer to the write_diagnositic_buffer.
*/
{
  add_string_to_text_buffer(write_diagnostic_buffer, "{\"message\":");
  write_sarif_message(dp);
  if (dp->diag_header_pos.seq != 0) {
    add_string_to_text_buffer(write_diagnostic_buffer,
                              ",\"physicalLocation\":");
    write_sarif_physical_location(&dp->diag_header_pos);
  }  /* if */
  add_char_to_text_buffer(write_diagnostic_buffer, '}');
}  /* write_sarif_related_location */


static void construct_sarif_result(a_diagnostic_ptr dp)
/*
Write a SARIF "result object" (and a preceding comma if this is not the first
result) for the given diagnostic pointer to the write_diagnositic_buffer.  Note
that the msg_buffer will be updated and then reset in this process, thus it's
important that the msg_buffer is not already in use.
*/
{
  /* If this isn't the first SARIF "result object" that's been emitted, add a
     comma to start the new result. */
  if (diagnostic_counters.total.all_error_types() > 1) {
    add_char_to_text_buffer(write_diagnostic_buffer, ',');
  }  /* if */
  add_char_to_text_buffer(write_diagnostic_buffer, '{');
  /* Construct the ruleId property. */
  add_string_to_text_buffer(write_diagnostic_buffer, "\"ruleId\":");
  write_sarif_rule_id(dp);
  /* Construct the level property. */
  add_string_to_text_buffer(write_diagnostic_buffer, ",\"level\":");
  write_sarif_level(dp);
  /* Construct the message property. */
  add_string_to_text_buffer(write_diagnostic_buffer, ",\"message\":");
  write_sarif_message(dp);
  /* Construct the locations property. */
  if (dp->diag_header_pos.seq != 0) {
    add_string_to_text_buffer(write_diagnostic_buffer, ",\"locations\":");
    write_sarif_locations(&dp->diag_header_pos);
  }  /* if */

  /* Construct the relatedLocations property. */
  a_diagnostic_ptr mi_dp = dp->more_info.head;
  if (mi_dp != NULL) {
    a_boolean first = TRUE;

    add_string_to_text_buffer(write_diagnostic_buffer,
                              ",\"relatedLocations\":[");
    for (; mi_dp != NULL; mi_dp = mi_dp->next) {
      /* Unlike other messages, the primary diagnostic is not known when the
         more_info messages are created. */
      mi_dp->primary_diag = dp;
      write_sarif_related_location(mi_dp);
      if (first) {
        first = FALSE;
      } else {
        add_char_to_text_buffer(write_diagnostic_buffer, ',');
      }  /* if */
    }  /* for */
    add_char_to_text_buffer(write_diagnostic_buffer, ']');
  }  /* if */
  /* Finish writing the result and flush the buffer. */
  add_char_to_text_buffer(write_diagnostic_buffer, '}');
  add_char_to_text_buffer(write_diagnostic_buffer, '\0');
  fputs(write_diagnostic_buffer->buffer, f_error);
  (void)fflush(f_error);
}  /* construct_sarif_result */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static void construct_message(a_diagnostic_ptr dp)
/*
Convert and emit the given diagnostic entry (dp) as textual output with respect
to the current output mode.
*/
{
#if STANDALONE_UTILITY_PROGRAM
  construct_text_message(dp);
#else /* !STANDALONE_UTILITY_PROGRAM */
  switch (output_mode) {
    case om_text:
      construct_text_message(dp);
      break;
    case om_sarif:
      construct_sarif_result(dp);
      break;
    default_is_unexpected();
  }  /* switch */
#endif /* STANDALONE_UTILITY_PROGRAM */
}  /* construct_message */


static a_boolean is_catastrophic_error_severity(an_error_severity severity)
/*
Return TRUE if the given error severity would result in a catastrophic error;
otherwise, return FALSE.
*/
{
  a_boolean result;

  switch (severity) {
    case es_catastrophe:
    case es_command_line_error:
    case es_internal_error:
      result = TRUE;
      break;
    default:
      result = FALSE;
  }  /* switch */
  return result;
}  /* is_catastrophic_error_severity */


static void end_of_diagnostic_actions(a_diagnostic_ptr	dp)
/*
Do any actions that should be performed as a consequence of issuing the
diagnostic specified by dp.  For example, some error severities cause
the program to be terminated, as does reaching the error limit.
*/
{
  unsigned long total_all_errors = diagnostic_counters.total.all_error_types();

#if IL_SHOULD_BE_WRITTEN_TO_FILE && !STANDALONE_UTILITY_PROGRAM
  /* If there are any errors, suppress generation of the intermediate
     language file. */
  if (total_all_errors > 0) cancel_il_file();
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && !STANDALONE_UTILITY_PROGRAM */
  /* Terminate the compilation for the more serious severities. */
  if (is_catastrophic_error_severity(dp->severity)) {
    /* Force out the last line of the raw listing file. */
#if !STANDALONE_UTILITY_PROGRAM
    finish_raw_listing_file();
#endif /* !STANDALONE_UTILITY_PROGRAM */
    term_compilation(dp->severity);
  }  /* if */
  /* Terminate the compilation if the error limit has been reached.  Note
     that remarks and warnings are never counted. */
  if (total_all_errors >= error_limit) {
#if !USING_DRIVER
    fprintf(f_error, "%s\n", error_text(ec_error_limit_reached));
#endif /* !USING_DRIVER */
#if !STANDALONE_UTILITY_PROGRAM
    if (f_raw_listing != NULL) {
      fprintf(f_raw_listing, "C \"\" 0 0 error limit reached\n");
    }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    /* Force out the last line of the raw listing file. */
#if !STANDALONE_UTILITY_PROGRAM
    finish_raw_listing_file();
#endif /* !STANDALONE_UTILITY_PROGRAM */
    term_compilation(es_catastrophe);
  }  /* if */
}  /* end_of_diagnostic_actions */


static a_diagnostic_ptr create_diagnostic_entry(
			a_diagnostic_ptr	primary_diagnostic,
			a_diagnostic_kind	kind,
			an_error_code		error_code,
			a_source_position	*position,
			an_error_severity	severity)
/*
Create a diagnostic entry and initialize it with the specified kind, error
code, and for primary diagnostics, severity, and source position.  If this
is not a primary diagnostic link this diagnostic to the appropriate
sub-list of the specified primary diagnostic.  Return a pointer to the
newly created entry.
*/
{
  a_diagnostic_ptr	dp;
  a_line_number		line_number;
  a_boolean		at_end_of_source;
  a_source_file_ptr	sfp;
  a_const_char		*file_name;
  a_const_char		*full_name;

  dp = alloc_diagnostic();
  dp->kind = kind;
  dp->error_code = error_code;
  if (kind == dck_primary) {
    a_source_info_for_pos_ptr	sifpp = &dp->source_info;
    check_assertion_str2(position != NULL, "create_diagnostic_entry:",
                         "position is NULL");
    /* See whether this error should have its severity overridden. */
    check_for_overridden_severity(error_code, &severity, position);
    dp->severity = severity;
    dp->translation_unit = curr_translation_unit;
    /* Convert the sequence number into a compilation unit and line number. */
    sfp = conv_seq_to_file_and_line(position->seq, &file_name, &full_name,
                                    &line_number, &at_end_of_source);
    sifpp->source_file = sfp;
    sifpp->file_name = file_name;
    sifpp->line_number = line_number;
    sifpp->at_end_of_source = at_end_of_source;
    dp->position = *position;
    dp->diag_header_pos = *position;
    dp->diag_header_source_info = *sifpp;
#if FULLY_RESOLVED_MACRO_POSITIONS
    if (position->orig_seq != 0 &&
        (macro_positions_in_diagnostics ||
         position->orig_seq >= position->seq)) {
      /* Use the original position of the text for the first part of the
          message (i.e., if the text is in a macro expansion, the position will
          indicate the macro definition or macro argument from which the text
          was copied).  We use the normal position if the original position is
          in a command-line or predefined macro (orig_seq == 0), and if
          macro_positions_in_diagnostics is FALSE, we only use the original
          position if it is in a macro argument (a reference to a macro
          definition will necessarily have orig_seq < seq). */
      dp->diag_header_pos.seq = position->orig_seq;
      dp->diag_header_pos.column = position->orig_column;
      sfp = conv_seq_to_file_and_line(position->orig_seq, &file_name,
                                      &full_name, &line_number,
                                      &at_end_of_source);
      sifpp = &dp->diag_header_source_info;
      sifpp->source_file = sfp;
      sifpp->file_name = file_name;
      sifpp->line_number = line_number;
      sifpp->at_end_of_source = at_end_of_source;
    }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  } else {
    check_assertion(primary_diagnostic != NULL);
    /* Copy the position from the primary diagnostic. */
    dp->position = primary_diagnostic->position;
    dp->primary_diag = primary_diagnostic;
  }  /* if */
  if (primary_diagnostic != NULL) {
    a_diag_list_ptr	dlp = NULL;
    switch (kind) {
      case dck_sub_message:   dlp = &primary_diagnostic->sub_msgs;      break;
      case dck_context:       dlp = &primary_diagnostic->context;       break;
      case dck_macro_context: dlp = &primary_diagnostic->macro_context; break;
      default:
        unexpected_condition();
        break;
    }  /* switch */
    /* Link this entry as a sub-list of the specified primary diagnostic. */
    if (dlp->head == NULL) {
      dlp->head = dp;
    }  /* if */
    if (dlp->tail != NULL) {
      dlp->tail->next = dp;
    }  /* if */
    dlp->tail = dp;
  }  /* if */
  return dp;
}  /* create_diagnostic_entry */

namespace detail {

a_diagnostic_ptr create_primary_diagnostic(an_error_code     error_code,
                                           a_source_position *position,
                                           an_error_severity severity)
/*
Interface to create_diagnostic_entry that does not require a primary
diagnostic to be provided.
*/
{
#if !STANDALONE_UTILITY_PROGRAM
  if (severity == es_command_line_error ||
      severity == es_command_line_warning ||
      severity == es_internal_error ||
      severity == es_catastrophe ||
      !in_front_end) {
    /* For these error severities it may not be possible to allocate
       memory in IL memory yet, so allocate it in general memory.  Also, use
       general memory when not executing as part of the front end. */
    diag_memory_region = NO_MEMORY_REGION_NUMBER;
  } else {
    diag_memory_region = FRONT_END_REGION_NUMBER;
  }  /* if */
#else /* STANDALONE_UTILITY_PROGRAM */
  /* Always use general memory. */
  diag_memory_region = NO_MEMORY_REGION_NUMBER;
#endif /* !STANDALONE_UTILITY_PROGRAM */
  return create_diagnostic_entry((a_diagnostic_ptr)NULL, dck_primary,
                                 error_code, position, severity);
}  /* create_primary_diagnostic */


a_diagnostic_ptr create_sub_message(a_diagnostic_ptr primary_dp,
                                    an_error_code    error_code)
/*
Create a sub-message for the diagnostic pointed to by "primary_dp".
*/
{
  return create_diagnostic_entry(primary_dp, dck_sub_message, error_code,
                                 &null_source_position, es_none);
}  /* create_sub_message */


void append_to_diag_list(a_diag_list_ptr  diag_list,
                         a_diagnostic_ptr new_diag)
/*
Append the given new diagnostic to the given diagnostic list.
*/
{
  if (diag_list->head == NULL) {
    diag_list->head = new_diag;
  }  /* if */
  if (diag_list->tail != NULL) {
    diag_list->tail->next = new_diag;
  }  /* if */
  diag_list->tail = new_diag;
}  /* append_to_diag_list */

}  /* namespace detail */

using namespace detail;

static inline void add_fill_in_to_diagnostic(a_diagnostic_ptr   diag_ptr,
                                             a_diag_fill_in_ptr fill_in_ptr)
/*
Add the fill-in specified by fill_in_ptr to the diagnostic specified by
diag_ptr.
*/
{
  if (diag_ptr->fill_in_head == NULL) {
    diag_ptr->fill_in_head = fill_in_ptr;
  }  /* if */
  if (diag_ptr->fill_in_tail != NULL) {
    diag_ptr->fill_in_tail->next = fill_in_ptr;
  }  /* if */
  diag_ptr->fill_in_tail = fill_in_ptr;
}  /* add_fill_in_to_diagnostic */

#if !STANDALONE_UTILITY_PROGRAM

static inline void add_number_fill_in(a_diagnostic_ptr     diag_ptr,
                                      a_signed_diag_number number)
/*
Add a number fill-in entry for number to the diagnostic specified by diag_ptr.
*/
{
  a_diag_fill_in_ptr dfip = alloc_diag_fill_in(dfk_number);

  dfip->variant.number = number;
  add_fill_in_to_diagnostic(diag_ptr, dfip);
}  /* add_number_fill_in */


static inline void add_unsigned_number_fill_in(
                                              a_diagnostic_ptr        diag_ptr,
                                              an_unsigned_diag_number number)
/*
Add an unsigned number fill-in entry for number to the diagnostic specified by
diag_ptr.
*/
{
  a_diag_fill_in_ptr dfip = alloc_diag_fill_in(dfk_unsigned_number);

  dfip->variant.unsigned_number = number;
  add_fill_in_to_diagnostic(diag_ptr, dfip);
}  /* add_unsigned_number_fill_in */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static inline void add_position_fill_in(a_diagnostic_ptr  diag_ptr,
                                        a_source_position *pos)
/*
Add a position fill-in entry for pos to the diagnostic specified by diag_ptr.
*/
{
  a_diag_fill_in_ptr dfip = alloc_diag_fill_in(dfk_position);

  dfip->variant.position = *pos;
  add_fill_in_to_diagnostic(diag_ptr, dfip);
}  /* add_position_fill_in */


static inline void add_string_fill_in(a_diagnostic_ptr diag_ptr,
                                      a_const_char     *str_start,
                                      size_t           str_len)
/*
Add a string fill-in entry for the given string (starting at str_start and
containing str_len characters -- excluding any null terminator character) to
the diagnostic specified by diag_ptr.
*/
{
  a_diag_fill_in_ptr dfip = alloc_diag_fill_in(dfk_string);

  dfip->variant.string.start = str_start;
  dfip->variant.string.len = str_len;
  add_fill_in_to_diagnostic(diag_ptr, dfip);
}  /* add_string_fill_in */


static inline void add_string_fill_in(a_diagnostic_ptr diag_ptr,
                                      a_const_char     *string)
/*
Add a string fill-in entry for string to the diagnostic specified by diag_ptr.
*/
{
  add_string_fill_in(diag_ptr, string, strlen(string));
}  /* add_string_fill_in */


static inline void add_symbol_fill_in_with_depth(a_diagnostic_ptr diag_ptr,
                                                 a_symbol_ptr     symbol,
                                                 a_scope_depth    scope_depth)
/*
Add a symbol fill-in entry for symbol to the diagnostic specified by diag_ptr.
scope_depth is the scope stack depth associated with the symbol, or
NO_SCOPE_DEPTH.
*/
{
  a_diag_fill_in_ptr dfip = alloc_diag_fill_in(dfk_symbol);

  dfip->variant.symbol.ptr = symbol;
  dfip->variant.symbol.scope_depth = scope_depth;
  add_fill_in_to_diagnostic(diag_ptr, dfip);
}  /* add_symbol_fill_in_with_depth */


static inline void add_symbol_fill_in(a_diagnostic_ptr diag_ptr,
                                      a_symbol_ptr     symbol)
/*
Add a symbol fill-in entry for "symbol" to the diagnostic specified by
diag_ptr.
*/
{
  add_symbol_fill_in_with_depth(diag_ptr, symbol, NO_SCOPE_DEPTH);
}  /* add_symbol_fill_in */


static inline void add_template_arg_list_fill_in(a_diagnostic_ptr   diag_ptr,
                                                 a_template_arg_ptr templ_args)
/*
Add a declaration fill-in entry for "templ_args" to the diagnostic specified by
diag_ptr.
*/
{
  a_diag_fill_in_ptr dfip = alloc_diag_fill_in(dfk_template_args);

  dfip->variant.template_args = templ_args;
  add_fill_in_to_diagnostic(diag_ptr, dfip);
}  /* add_template_arg_list_fill_in */


static inline void add_reflection_fill_in(a_diagnostic_ptr    diag_ptr,
                                          a_reflection_value  rv)
/*
Add a fill-in entry for "rv" to the diagnostic specified by diag_ptr.
*/
{
  a_diag_fill_in_ptr dfip = alloc_diag_fill_in(dfk_reflection);

  dfip->variant.reflection = rv;
  add_fill_in_to_diagnostic(diag_ptr, dfip);
}  /* add_reflection_fill_in */


static inline void add_type_fill_in(a_diagnostic_ptr diag_ptr,
                                    a_type_ptr       type)
/*
Add a declaration fill-in entry for "type" to the diagnostic specified by
diag_ptr.
*/
{
  a_diag_fill_in_ptr dfip = alloc_diag_fill_in(dfk_type);

  dfip->variant.type = type;
  add_fill_in_to_diagnostic(diag_ptr, dfip);
}  /* add_type_fill_in */

#if !STANDALONE_UTILITY_PROGRAM

static void general_context_diagnostic(
			a_diagnostic_ptr	primary_dp,
			a_diagnostic_kind	kind,
			an_error_code		error_code,
			a_const_char		*string1,
			a_const_char		*string2,
			a_symbol_ptr		symbol,
			a_scope_depth		scope_depth,
			a_source_position	*other_pos)
/*
General interface to the diagnostic routines.  Issue a diagnostic with
the specified severity, kind, error code, and error position.   If any of the
other parameters is non-NULL, add a fill-in of the given kind.  For
a symbol fill-in, scope_depth is an option associated scope depth, or
NO_SCOPE_DEPTH.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_diagnostic_entry(primary_dp, kind,
                               error_code, (a_source_position*)NULL,
                               es_none);
  if (string1 != NULL) add_string_fill_in(dp, string1);
  if (string2 != NULL) add_string_fill_in(dp, string2);
  if (symbol != NULL) add_symbol_fill_in_with_depth(dp, symbol, scope_depth);
  if (other_pos != NULL) add_position_fill_in(dp, other_pos);
}  /* general_context_diagnostic */


static void add_str_context_diag(a_diagnostic_ptr	primary_dp,
				 a_diagnostic_kind	kind,
				 an_error_code		error_code,
				 a_const_char		*string)
/*
Add a context diagnostic with a string fill-in to primary_dp.
*/
{
  general_context_diagnostic(primary_dp, kind, error_code,
                             string,
                             (a_const_char*)NULL,
                             (a_symbol_ptr)NULL,
                             NO_SCOPE_DEPTH,
                             (a_source_position*)NULL);
}  /* add_str_context_diag */


#if RECORD_MACRO_INVOCATIONS

static void add_str_pos_context_diag(a_diagnostic_ptr	primary_dp,
				     a_diagnostic_kind	kind,
				     an_error_code	error_code,
				     a_const_char	*string,
				     a_source_position	*other_pos)
/*
Add a context diagnostic with a string and position fill-in to primary_dp.
*/
{
  general_context_diagnostic(primary_dp, kind, error_code,
                             string,
                             (a_const_char*)NULL,
                             (a_symbol_ptr)NULL,
                             NO_SCOPE_DEPTH,
                             other_pos);
}  /* add_str_pos_context_diag */

#endif /* RECORD_MACRO_INVOCATIONS */

static void add_symbol_pos_context_diag(a_diagnostic_ptr	primary_dp,
					a_diagnostic_kind	kind,
					an_error_code		error_code,
					a_symbol_ptr		symbol,
					a_scope_depth		scope_depth,
					a_source_position	*position)
/*
Add a context diagnostic with a symbol and position fill-in to primary_dp.
scope_depth is a scope depth associated with the symbol or NO_SCOPE_DEPTH.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_diagnostic_entry(primary_dp, kind,
                               error_code, (a_source_position*)NULL,
                               es_none);
  add_symbol_fill_in_with_depth(dp, symbol, scope_depth);
  add_position_fill_in(dp, position);
}  /* add_symbol_pos_context_diag */


static void add_number_context_diag(a_diagnostic_ptr	primary_dp,
				    a_diagnostic_kind	kind,
				    an_error_code	error_code,
				    int32_t		number)
/*
Add a context diagnostic with a numeric fill-in to primary_dp.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_diagnostic_entry(primary_dp, kind,
                               error_code, (a_source_position*)NULL,
                               es_none);
  add_number_fill_in(dp, number);
}  /* add_number_context_diag */


static void add_context_diag(a_diagnostic_ptr	primary_dp,
			     a_diagnostic_kind	kind,
			     an_error_code	error_code)
/*
Add a context diagnostic primary_dp.
*/
{
  general_context_diagnostic(primary_dp, kind, error_code,
                             (a_const_char*)NULL,
                             (a_const_char*)NULL,
                             (a_symbol_ptr)NULL,
                             NO_SCOPE_DEPTH,
                             (a_source_position*)NULL);
}  /* add_context_diag */


static void display_trans_unit_context(
			a_diagnostic_ptr	dp,
			a_boolean		add_detected_prefix)
/*
This routine is called when the diagnostic that is being issued is for
a translation unit other than the primary one.  Generate a context message
that indicates the translation unit that is associated with the message.

When add_detected_prefix is TRUE, the error code returned will refer to
a message that includes the text (e.g., "detected during ") that is
used when only a single line of context information is being supplied.
When multiple context lines are being displayed, the "detected during"
message appears by itself on a separate line.
*/
{
  an_error_code		context_error_code;
  a_source_file_ptr	sfp;
  a_const_char		*file_name_copy;

  /* If only one line of context is being issued, then it is
     considered the "primary" context line.  Otherwise a header
     was issued above and the context lines are handled as list
     elements. */
  if (add_detected_prefix) {
    context_error_code = ec_det_during_compilation_of_secondary_trans_unit;
  } else {
    context_error_code = ec_compilation_of_secondary_trans_unit_context;
  }  /* if */
  /* Get the primary source file associated with this position. */
  sfp = primary_source_file_for_seq(dp->position.seq);
  /* format_source_file_name returns a pointer to a buffer that is reused.
     Make a copy of the string. */
  file_name_copy = format_source_file_name(sfp, /*use_name_as_written=*/FALSE,
                                           /*quote_file_name=*/FALSE);
  file_name_copy = diag_copy_string(file_name_copy);
  add_str_context_diag(dp, dck_context, context_error_code, file_name_copy);
}  /* display_trans_unit_context */


static a_boolean in_secondary_translation_unit(a_source_position *pos)
/*
Determine whether "pos" represents a position in a secondary translation
unit.  If so, set a global variable to indicate the source file that is
associated with the translation unit that it is in.
*/
{
  a_boolean	result;

  if (!in_front_end) {
    /* The translation unit data structure cannot be used after the front
       end has completed. */
    result = FALSE;
  } else if (translation_units == NULL ||
             translation_units->next == NULL) {
    /* Optimize the case where there is only one translation unit. */
    result = FALSE;
  } else {
    a_source_file_ptr	sfp;
    /* Get the primary source file associated with this position. */
    sfp = primary_source_file_for_seq(pos->seq);
    /* It is a secondary translation unit if it is not the first entry
       on the list. */
    result = sfp != NULL &&
             sfp != translation_units->source_file;
  }  /* if */
  return result;
}  /* in_secondary_translation_unit */


static void add_instantiation_context(a_diagnostic_ptr	dp)
/*
If there is instantiation context information for the diagnostic,
create the diagnostic entries for the context.
*/
{
  /* Certain conditions, such as errors that occur while instantiating
     template classes and functions, require additional context information
     to be supplied after the message is printed.  The processing is done
     in two phases.  First, we determine whether any context information
     is required.  This is needed because we handle the case of a single
     context line differently than multiple lines. */
  int		num_of_contexts = 0;
  a_symbol_ptr	sym;
  an_error_code	context_error_code;
  a_boolean	pos_is_in_secondary_trans_unit;
  a_scope_depth	sd;

  /* Check whether we need to supply additional context information. */
  for (sd = depth_scope_stack; sd > DEPTH_OF_FILE_SCOPE; --sd) {
    if (include_in_context_output(&scope_stack[sd], &sym,
                                  &context_error_code,
                                  (a_source_position*)NULL,
                                  /*add_detected_prefix=*/FALSE)) {
      num_of_contexts++;
    }  /* if */
  }  /* for */
  /* If we are in a secondary translation unit, we need a context line
     to specify the translation unit name. */
  pos_is_in_secondary_trans_unit =
                                  in_secondary_translation_unit(&dp->position);
  if (pos_is_in_secondary_trans_unit) num_of_contexts++;
  /* Loop through the scope stack and output context information. */
  if (num_of_contexts > 0) {
    int32_t	contexts_to_include;
    int32_t	contexts_processed = 0;
    int32_t	contexts_skipped = 0;
    a_boolean	limit_context;
    /* Check whether we should limit the number of context lines emitted.
       Ignore the limit if we are just above it. */
    limit_context = context_limit > 0 &&
                    num_of_contexts > (context_limit + 1);
    contexts_to_include = context_limit / 2;
    if (num_of_contexts != 1) {
      /* If there is more than one line of context we output an
         initial header line. */
      add_context_diag(dp, dck_context, ec_template_detected_during_header);
    }  /* if */
    for (sd = depth_scope_stack; sd > DEPTH_OF_FILE_SCOPE; --sd) {
      a_scope_stack_entry_ptr	ssep = &scope_stack[sd];
      STATIC_THREAD a_source_position	context_source_pos;
      if (!include_in_context_output(ssep, &sym,
                                     &context_error_code,
                                     &context_source_pos,
                                     /*add_detected_prefix=*/
                                     num_of_contexts == 1)) continue;
      contexts_processed++;
      /* If we are limiting the number of context lines, see if this is
         an entry that should be excluded. */
      if (limit_context &&
          contexts_processed > contexts_to_include &&
          contexts_processed <= (num_of_contexts - contexts_to_include)) {
        contexts_skipped++;
        continue;
      }  /* if */
      /* When resuming the display of contexts, indicate the number of
         entries not shown. */
      if (contexts_skipped > 0) {
        add_number_context_diag(dp, dck_context, ec_context_lines_skipped,
                                contexts_skipped);
        contexts_skipped = 0;
      }  /* if */
      add_symbol_pos_context_diag(dp, dck_context, context_error_code, sym, sd,
                                  &context_source_pos);
    }  /* for */
    if (pos_is_in_secondary_trans_unit) {
      display_trans_unit_context(dp,
                                 /*add_detected_prefix=*/num_of_contexts == 1);
    }  /* if */
  }  /* if */
}  /* add_instantiation_context */

#if RECORD_MACRO_INVOCATIONS

static void add_macro_context(a_diagnostic_ptr	dp)
/*
If we are recording macro invocations and/or fully resolved macro
positions, create the context diagnostic information for those.
*/
{
  a_source_position		*error_pos = &dp->position;
  a_macro_invocation_record_ptr	mirp = NULL;

  if (error_pos->macro_context != NO_PARENT_MACRO_INVOCATION &&
      macro_positions_in_diagnostics) {
    /* Print a trace of the macro invocation stack in effect at
       error_pos.  Note that the last (bottommost) stack frame is
       omitted in this trace because it will refer to the same position
       as the normal position in error_pos, which will be printed
       below before the source line. */
    int			stack_depth = 0;
    int			i;
    a_boolean		frames_omitted_msg_printed = FALSE;

    for (mirp = macro_invocation_record_at_index(error_pos->macro_context);
         mirp != NULL &&
         mirp->parent_macro_index != NO_PARENT_MACRO_INVOCATION;
         mirp = macro_invocation_record_at_index(mirp->parent_macro_index)) {
      ++stack_depth;
    }  /* for */
    mirp = macro_invocation_record_at_index(error_pos->macro_context);
    for (i = 0; i <= stack_depth; ++i) {
      if (i < 5 || i >= stack_depth - 4) {
        a_const_char		*macro_name;
        a_source_position	full_pos;
        if (mirp->assoc_macro != NULL) {
          macro_name = mirp->assoc_macro->source_corresp.name;
        } else {
          macro_name = error_text(ec_name_of_unknown_macro);
        }  /* if */
        copy_simple_position_to_full_position(mirp->start, full_pos);
        add_str_pos_context_diag(dp, dck_macro_context,
                                 ec_in_expansion_of_macro, macro_name,
                                 &full_pos);
      } else if (!frames_omitted_msg_printed) {
        add_number_context_diag(dp, dck_macro_context,
                                ec_macro_context_lines_skipped,
                                stack_depth - 9);
        frames_omitted_msg_printed = TRUE;
      }  /* if */
      mirp = macro_invocation_record_at_index(mirp->parent_macro_index);
    }  /* for */
  }  /* if */
}  /* add_macro_context */

#else /* !RECORD_MACRO_INVOCATIONS */

#define add_macro_context(dp) /* Nothing */

#endif /* RECORD_MACRO_INVOCATIONS */

static void add_error_context(a_diagnostic_ptr	dp)
/*
If there is context information for the diagnostic (instantiation
information, etc.), create the diagnostic entries for the context.
*/
{
  add_macro_context(dp);
  add_instantiation_context(dp);
}  /* add_error_context */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static void wrap_up_diagnostic(a_diagnostic_ptr	dp)
/*
This routine is called when a diagnostic has been completely constructed.
The message is formatted into text strings and is output.
*/
{
  a_boolean diag_should_be_issued;

#if !STANDALONE_UTILITY_PROGRAM
  /* For safety, make sure we don't use any information cached by
     can_locate_source_line. */
  can_locate_source_line_info_cached = FALSE;
#endif /* !STANDALONE_UTILITY_PROGRAM */
  diag_should_be_issued = check_severity(dp);
#if !STANDALONE_UTILITY_PROGRAM
  if (in_front_end) {
    if (diag_should_be_issued) {
      /* Determine whether this diagnostic should not be issued because
         of the use of the "once" diagnostic control. */
      diag_should_be_issued = !diagnostic_already_issued_for_diag_once(dp);
      if (diag_should_be_issued) {
        /* Suppress the diagnostic if it has already been issued during the
           prototype instantiation. */
        diag_should_be_issued = !diagnostic_already_issued_for_prototype(dp);
      }  /* if */
      if (!diag_should_be_issued) {
        an_error_severity reported_severity = determine_reported_severity(dp);

        /* Update the repeated diagnostics counter. */
        update_diagnostic_counter(reported_severity,
                                  &diagnostic_counters.repeated);
      }  /* if */
    }  /* if */
    if (diag_should_be_issued) {
      /* Check to see if this error occurred while scanning a command-line
         macro definition.  If so, ignore the original error and issue a
         general error indicating that the macro definition is invalid. */
      if (curr_cmd_line_or_predef_macro_def != NULL &&
          !processing_predefined_macro &&
          dp->error_code != ec_bad_cmd_line_macro) {
        str_command_line_error(ec_bad_cmd_line_macro,
                               curr_cmd_line_or_predef_macro_def);
        /* This is unreachable, str_command_line_error will terminate
           compilation. */
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
  if (diag_should_be_issued) {
    an_error_severity reported_severity = determine_reported_severity(dp);

#if !STANDALONE_UTILITY_PROGRAM
    if (in_front_end) {
      /* If there's an instantiation currently on the scope stack, grab the
         most recent instantiation's associated template.  Then mark the
         template as having an invalid active instantiation so that further
         (recursive) instantiations of the template are suspended until the
         problematic instantiation is popped off the scope stack. */
      if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&
          reported_severity >= es_discretionary_error) {
        a_scope_stack_entry_ptr ssep =
                             &scope_stack[depth_innermost_instantiation_scope];
        a_symbol_ptr            templ_sym = ssep->template_sym;

        if (templ_sym != NULL) {
          a_template_symbol_supplement_ptr supp =
                                              templ_sym->variant.template_info;

          if (supp->invalid_active_instantiation == NULL) {
            supp->invalid_active_instantiation = ssep->instance_sym;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    /* Update the total diagnostics counter. */
    update_diagnostic_counter(reported_severity, &diagnostic_counters.total);
    /* When diagnostics are suppressed, suppress all non-catastrophic error
       types.  Catastrophic error types are excluded from suppression as they
       require early termination. */
    if (globally_suppress_diagnostics &&
        !is_catastrophic_error_severity(reported_severity)) {
      diag_should_be_issued = FALSE;
      /* Update the suppressed diagnostics counter. */
      update_diagnostic_counter(reported_severity,
                                &diagnostic_counters.suppressed);
    }  /* if */
    /* Check for a local diagnostic counter.  If said diagnostic counter was
       set, update it now. */
    if (diagnostic_counters.local != NULL) {
      update_diagnostic_counter(reported_severity,
                                diagnostic_counters.local);
    }  /* if */
  }  /* if */
  if (diag_should_be_issued) {
    /* Set up for use of the il_to_str routines. */
    set_up_output_control_block();
    /* Allocate the diagnostic buffer if this is our first time. */
    if (write_diagnostic_buffer == NULL) {
      write_diagnostic_buffer = alloc_text_buffer(1024);
    }  /* if */
    /* Start at the beginning of the buffer. */
    reset_text_buffer(write_diagnostic_buffer);
    if (dp->severity == es_catastrophe) {
      /* Make sure that if one catastrophic error leads to another, we abort
         the compilation instead of looping. */
      if (catastrophe_has_occurred) {
        fprintf(f_error, "%s\n", error_text(ec_catastrophic_error_loop));
        term_compilation(es_catastrophe);
      }  /* if */
      catastrophe_has_occurred = TRUE;
    }  /* if */
#if !STANDALONE_UTILITY_PROGRAM
    /* Generate information about the context in which the error occurred. */
    if (dp->severity != es_internal_error &&
        (dp->severity != es_catastrophe ||
         display_error_context_on_catastrophe)) {
      add_error_context(dp);
    }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    /* Start at the beginning of the buffer. */
    reset_text_buffer(msg_buffer);
    construct_message(dp);
  }  /* if */
  /* Check if the compilation should be terminated as a result of this
     diagnostic. */
  end_of_diagnostic_actions(dp);
  /* Free the diagnostic entry. */
  free_diagnostic(dp);
}  /* wrap_up_diagnostic */


/* Forward declaration. */
NORETURN static void error_code_errno_catastrophe(
                                      an_error_code error_code,
                                      an_error_code error_code2,
                                      int           errno_value);


static void file_open_error_full(an_error_severity	severity,
				 an_error_code		file_kind,
                                 a_const_char		*file_name,
				 an_open_file_result	*open_result,
				 a_source_position	*error_pos)
/*
Write an error message about opening the file named file_name.  file_kind is
an error code for a message that describes the kind of file being opened.
open_result is the entry that describes the kind of failure, which was
returned by the file open routine.
*/
{
  a_diagnostic_ptr		dp;
  a_const_char			*reason = NULL;
  an_open_file_result_set	flags = open_result->flags;
  an_error_code			error_code;
  a_source_position		local_error_pos = *error_pos;

  /* Determine if an open failure reason should be displayed, and if so
     what it should be. */
  if ((flags & OFR_NOT_FOUND) != 0) {
    /* Do not display a reason if the file was not found. */
  } else if ((flags & OFR_CANNOT_OPEN) != 0) {
    /* Use strerror to determine the reason based on the errno value. */
    reason = strerror(open_result->errno_value);
  } else if ((flags & OFR_NOT_REGULAR) != 0) {
    reason = error_text(ec_not_regular);
  } else if ((flags & OFR_IS_DIRECTORY) != 0) {
    reason = error_text(ec_is_directory);
  } else if ((flags & OFR_BAD_NAME) != 0) {
    reason = error_text(ec_illegal_file_name);
  }  /* if */
  error_code = reason == NULL ? ec_cannot_open_file
                              : ec_cannot_open_file_reason;
  /* If the error is to be issued as a command-line error, provide an
     appropriate source position. */
  if (severity == (an_error_severity)es_command_line_error) {
    set_position_to(local_error_pos, 0, SP_COL_CMD_LINE);
  }  /* if */
  dp = create_primary_diagnostic(error_code, &local_error_pos, severity);
  add_string_fill_in(dp, error_text(file_kind));
  add_string_fill_in(dp,
                     new_copy_of_string(format_file_name(file_name),
                                        General_allocator<char>()));
  if (reason != NULL) {
    add_string_fill_in(dp, reason);
  }  /* if */
  wrap_up_diagnostic(dp);
}  /* file_open_error_full */


void file_open_error(an_error_severity		severity,
		     an_error_code		file_kind,
                     a_const_char		*file_name,
		     an_open_file_result	*open_result)
/*
Write an error message about opening the file named file_name.  file_kind is
an error code for a message that describes the kind of file being opened.
open_result is the entry returned by the file open routine that describes
the kind of failure.
*/
{
  file_open_error_full(severity, file_kind, file_name, open_result,
                       &error_position);
}  /* file_open_error */


NORETURN void output_file_open_error(a_boolean         bad_name,
                                     an_error_code     file_kind,
                                     a_const_char      *file_name,
                                     an_error_severity severity)
/*
Write an error message about opening the output file named file_name,
and terminate the compilation.  This routine is used in contexts in which
a file open result is not available.   file_kind is an error code for a
message that describes the kind of file being opened.  Normally a "cannot open
file" error is issued, but if bad_name is TRUE an "illegal file name"
error is issued instead.
*/
{
  an_open_file_result	open_result;

  clear_open_file_result(&open_result);
  if (bad_name) open_result.flags |= OFR_BAD_NAME;
  /* If the error is to be issued as a command-line error, provide an
     appropriate source position. */
  if (severity == (an_error_severity)es_command_line_error) {
    set_position_to(error_position, 0, SP_COL_CMD_LINE);
  }  /* if */
  file_open_error(severity, file_kind, file_name, &open_result);
  /* Avoid spurious warning.  The function above does not return. */
  exit_compilation(es_internal_error);
}  /* output_file_open_error */


NORETURN void file_write_error(an_error_code	file_kind,
                               int		errno_value)
/*
Issue an error that a write to the file specified by file_kind failed.
errno_value provides information about the cause of the failure.
*/
{
  if (errno_value == 0) {
    pos_st_catastrophe(ec_file_write_error, &error_position,
                       error_text(file_kind));
  } else {
    error_code_errno_catastrophe(ec_file_write_error_errno,
                                 file_kind, errno_value);
  }  /* if */
}  /* file_write_error */


static a_boolean open_error_should_be_issued(
					an_open_file_flag_set	open_flags,
					an_open_file_result	*open_result,
					an_error_severity	*severity)
/*
The open of a file failed with open_result.  Based on open_flags, determine
whether an error should be issued or a NULL file pointer should be returned
to the caller.  Return TRUE if an error should be issued.  Set severity
to the error severity of the error to be issued.
*/
{
  a_boolean	issue_error = FALSE;

  if ((open_result->flags & OFR_NOT_FOUND) != 0 &&
      (open_flags & OFF_OKAY_IF_NOT_FOUND) == 0) {
    issue_error = TRUE;
  } else if ((open_result->flags & OFR_CANNOT_OPEN) != 0 &&
             (open_flags & OFF_OKAY_IF_CANNOT_OPEN) == 0) {
    issue_error = TRUE;
  } else if ((open_result->flags & OFR_NOT_REGULAR) != 0 &&
             (open_flags & OFF_OKAY_IF_NOT_REGULAR) == 0) {
    issue_error = TRUE;
  } else if ((open_result->flags & OFR_IS_DIRECTORY) != 0 &&
             (open_flags & OFF_OKAY_IF_DIRECTORY) == 0) {
    issue_error = TRUE;
  } else if ((open_result->flags & OFR_BAD_NAME) != 0) {
    issue_error = TRUE;
#if DEBUG
  } else if ((open_flags & OFF_FORCE_ERROR) != 0) {
    /* If this debug flag is specified, force an error to be issued. */
    issue_error = TRUE;
#endif /* DEBUG */
  }  /* if */
  *severity = (open_flags & OFF_COMMAND_LINE) != 0 ? es_command_line_error
                                                   : es_catastrophe;
  return issue_error;
}  /* open_error_should_be_issued */


FILE *open_source_file_with_error_handling(
				a_const_char		*file_name,
				an_open_file_flag_set	open_flags,
				an_open_file_result	*open_result,
				a_unicode_source_kind	*unicode_source_kind)
/*
Open the given file as a source input file, and return a pointer to the
file, or NULL if the file cannot be opened (and no error is issued).
open_flags indicates the cases in which NULL should be returned and
the cases in which an error should be issued.  *unicode_source_kind is set
to indicate the Unicode encoding form for the file, or usk_none if the file
is not Unicode.
*/
{
  FILE			*file;
  an_error_severity	severity;

  file = open_source_file(file_name, open_result, unicode_source_kind);
#if DEBUG
  /* Pretend the open failed if OFF_FORCE_ERROR is specified. */
  if ((open_flags & OFF_FORCE_ERROR) != 0) {
    if (file != NULL) (void)fclose(file);
    file = NULL;
  }  /* if */
#endif /* DEBUG */
  if (file == NULL &&
      open_error_should_be_issued(open_flags, open_result, &severity)) {
    /* Note that file_open_error does not return when called from here. */
    file_open_error(severity, ec_source, file_name, open_result);
  }  /* if */
  return file;
}  /* open_source_file_with_error_handling */


void close_output_file_with_error_handling(FILE			**f_output,
					   an_error_code	file_kind)
/*
Check for errors in writing an output file and close it.  Issue a diagnostic
if an error was detected while the file was being closed.  The file variable
passed by the caller is cleared.  file_kind is an error code for a message
that describes the kind of file being closed.
*/
{
  if (*f_output != NULL) {
    int		errno_value;
    /* Make a copy of the file variable and clear the copy from the caller. */
    FILE	*f_temp = *f_output;
    *f_output = NULL;
    if (close_output_file(f_temp, &errno_value)) {
      file_write_error(file_kind, errno_value);
    }  /* if */
  }  /* if */
}  /* close_output_file_with_error_handling */


FILE *fopen_with_error(a_const_char		*file_name,
		       a_const_char		*mode,
		       an_open_file_flag_set	open_flags,
		       an_error_code		file_kind)
/*
Open the given file_name using mode as the open mode.  Return the file
pointer, or NULL if the file cannot be opened (and no error is issued).
open_flags indicates the cases in which NULL should be returned and
the cases in which an error should be issued.  file_kind is the error
code for the description of the file to be used if an error is issued.
*/
{
  FILE			*file;
  an_open_file_result	open_result;
  an_error_severity	severity;

  file = fopen_with_result(file_name, mode, &open_result);
#if DEBUG
  /* Pretend the open failed if OFF_FORCE_ERROR is specified. */
  if ((open_flags & OFF_FORCE_ERROR) != 0) {
    if (file != NULL) (void)fclose(file);
    file = NULL;
  }  /* if */
#endif /* DEBUG */
  if (file == NULL &&
      open_error_should_be_issued(open_flags, &open_result, &severity)) {
    /* Note that file_open_error does not return when called from here. */
    file_open_error(severity, file_kind, file_name, &open_result);
  }  /* if */
  return file;
}  /* fopen_with_error */


FILE *open_output_file_with_error_handling(
					a_const_char		*file_name,
					a_boolean		binary_file,
					a_boolean		update_mode,
					an_open_file_flag_set	open_flags,
					an_error_code		file_kind)
/*
Open the given file_name as an output file.  binary_file is TRUE if
the file should be opened as a binary file instead of a text file.
update_mode is TRUE if the file should be opened in update mode so it
can be read as well as written.  Return the file pointer, or NULL if
the file cannot be opened (and no error is issued).  open_flags
indicates the cases in which NULL should be returned and the cases
in which an error should be issued.  file_kind is the error code for
the description of the file to be used if an error is issued.
*/
{
  FILE			*file;
  an_open_file_result	open_result;
  an_error_severity	severity;

  file = open_output_file(file_name, binary_file, update_mode, &open_result);
#if DEBUG
  /* Pretend the open failed if OFF_FORCE_ERROR is specified. */
  if ((open_flags & OFF_FORCE_ERROR) != 0) {
    if (file != NULL) (void)fclose(file);
    file = NULL;
  }  /* if */
#endif /* DEBUG */
  if (file == NULL &&
      open_error_should_be_issued(open_flags, &open_result, &severity)) {
    /* Note that file_open_error does not return when called from here. */
    file_open_error(severity, file_kind, file_name, &open_result);
  }  /* if */
  return file;
}  /* open_output_file_with_error_handling */


FILE *open_input_file_with_error_handling(
				a_const_char		*file_name,
				a_boolean		binary_file,
				an_open_file_flag_set	open_flags,
				an_error_code		file_kind)
/*
Open the given file_name as an input file.  Return the file pointer, or
NULL if the file cannot be opened (and no error is issued).  binary_file
is TRUE if the file should be opened as a binary file instead of a
text file.  open_flags indicates the cases in which NULL should be
returned and the cases in which an error should be issued.  file_kind
is the error code for the description of the file to be used if an
error is issued.
*/
{
  FILE			*file;
  an_open_file_result	open_result;
  an_error_severity	severity;

  file = open_input_file(file_name, binary_file, &open_result);
#if DEBUG
  /* Pretend the open failed if OFF_FORCE_ERROR is specified. */
  if ((open_flags & OFF_FORCE_ERROR) != 0) {
    if (file != NULL) (void)fclose(file);
    file = NULL;
  }  /* if */
#endif /* DEBUG */
  if (file == NULL &&
      open_error_should_be_issued(open_flags, &open_result, &severity)) {
    /* Note that file_open_error does not return when called from here. */
    file_open_error(severity, file_kind, file_name, &open_result);
  }  /* if */
  return file;
}  /* open_input_file_with_error_handling */


static an_error_code convert_error_tag_to_error_code(a_const_char *tag,
                                                     a_boolean    *err)
/*
Given an error tag string, convert it to an error code.  *err is set to
TRUE if the error tag is invalid.
*/
{
  an_error_code  error_code = ec_no_error;
  a_string_view	 tag_view(tag);
  auto           get_tag = [](size_t idx) {
                             return a_string_view(error_tags[idx].tag);
                           };
  ptrdiff_t      idx = bin_search(NUMBER_OF_ERROR_TAGS, tag_view, get_tag);

  if (idx != -1) {
    error_code = error_tags[idx].code;
    *err = FALSE;
  } else {
    *err = TRUE;
  }  /* if */
  return error_code;
}  /* convert_error_tag_to_error_code */


a_boolean set_severity_for_error_tag(a_const_char      *tag,
                                     an_error_severity severity,
                                     a_boolean         make_default)
/*
Given an error tag string, this routine looks up the error tag and updates the
table used to override the error severity of diagnostic messages.  If the tag
cannot be found return TRUE, otherwise return FALSE.  make_default is TRUE when
this is called for a value set on the command line or as part of the initial
front end configuration.  This causes both the current_severity and
default_severity fields of the error_codes array to be updated.  For other
calls, only the current_severity field is updated.  If the severity is
"es_default" the severity from the default_severity field is used to reset the
value in the current_severity field.
*/
{
  a_boolean     err;
  an_error_code	error_code = convert_error_tag_to_error_code(tag, &err);

  if (!err) {
    (void)set_severity_for_error_number((int)error_code,
                                        severity, make_default);
  }  /* if */
  return err;
}  /* set_severity_for_error_tag */


a_boolean set_severity_for_error_number(int               error_number,
                                        an_error_severity severity,
                                        a_boolean         make_default)
/*
Given an error number, this routine updates the table used to override the
error severity of diagnostic messages.  If the error number is out of range
return TRUE, otherwise return FALSE.  make_default is TRUE when this is called
for a value set on the command line or as part of the initial front end
configuration.  This causes both the current_severity and default_severity
fields of the error_codes array to be updated.  For other calls, only the
current_severity field is updated.  If the severity is "es_default" the
severity from the default_severity field is used to reset the value in the
current_severity field.
*/
{
  a_boolean			err;

  err = (error_number <= (int)ec_no_error || error_number >= (int)ec_last);
  if (!err) {
    if (severity == es_default) {
      /* Restore the severity from the default table. */
      error_codes[error_number].current_severity =
                                 error_codes[error_number].default_severity;
    } else if (severity == es_once) {
      /* Set the flag indicating that a given non-error diagnostic
         should be issued only once. */
      error_codes[error_number].once = TRUE;
    } else {
      error_codes[error_number].current_severity = severity;
      if (make_default) {
        error_codes[error_number].default_severity = severity;
      }  /* if */
    }  /* if */
  }  /* if */
  return err;
}  /* set_severity_for_error_number */


static void general_diagnostic(
			an_error_severity	error_severity,
			an_error_code		error_code,
			a_source_position	*error_pos,
			a_const_char		*string1,
			a_const_char		*string2,
			a_symbol_ptr		symbol1,
			a_symbol_ptr		symbol2,
			a_type_ptr		type1,
			a_type_ptr		type2,
			a_template_arg_ptr	templ_args1,
			a_template_arg_ptr	templ_args2,
			a_source_position	*other_pos,
			a_diag_list_ptr		dlp)
/*
General interface to the diagnostic routines.  Build a diagnostic with
the specified severity, error code, and error position.   If any of the
other parameters is non-NULL, add a fill-in of the given kind.  If dlp
is NULL, issue the diagnostic.  If it is not NULL, add the diagnostic
to the specified list.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, error_severity);
  if (string1 != NULL) add_string_fill_in(dp, string1);
  if (string2 != NULL) add_string_fill_in(dp, string2);
  if (symbol1 != NULL) add_symbol_fill_in(dp, symbol1);
  if (symbol2 != NULL) add_symbol_fill_in(dp, symbol2);
  if (type1 != NULL) add_type_fill_in(dp, type1);
  if (type2 != NULL) add_type_fill_in(dp, type2);
  if (templ_args1 != NULL) add_template_arg_list_fill_in(dp, templ_args1);
  if (templ_args2 != NULL) add_template_arg_list_fill_in(dp, templ_args2);
  if (other_pos != NULL) add_position_fill_in(dp, other_pos);
  if (dlp == NULL) {
    wrap_up_diagnostic(dp);
  } else {
    detail::append_to_diag_list(dlp, dp);
  }  /* if */
}  /* general_diagnostic */

#if !STANDALONE_UTILITY_PROGRAM

void str_command_line_warning(an_error_code error_code,
                              a_const_char  *concat_string)
/*
Write a command-line error message concatenated with concat_string, and
terminate the compilation.
*/
{
  set_position_to(error_position, 0, SP_COL_CMD_LINE);
  general_diagnostic(es_command_line_warning, error_code, &error_position,
                     concat_string, (a_const_char*)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
}  /* str_command_line_warning */

#endif /* !STANDALONE_UTILITY_PROGRAM */

NORETURN void str_command_line_error(an_error_code error_code,
                                     a_const_char  *concat_string)
/*
Write a command-line error message concatenated with concat_string, and
terminate the compilation.
*/
{
  set_position_to(error_position, 0, SP_COL_CMD_LINE);
  general_diagnostic(es_command_line_error, error_code, &error_position,
                     concat_string, (a_const_char*)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
  /* Avoid spurious warning.  The function above does not return. */
  exit_compilation(es_internal_error);
}  /* str_command_line_error */


NORETURN void command_line_error(an_error_code error_code)
/*
Write a command-line error message, and terminate the compilation.
*/
{
  str_command_line_error(error_code, (a_const_char*)NULL);
}  /* command_line_error */


void pos_st_diagnostic(an_error_severity error_severity,
                       an_error_code     error_code,
                       a_source_position *error_pos,
                       a_const_char      *error_string)
/*
Report the indicated diagnostic message (with the indicated fill-in string)
at the indicated position.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     error_string, (a_const_char*)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
}  /* pos_st_diagnostic */


void pos_diagnostic(an_error_severity  error_severity,
                    an_error_code      error_code,
                    a_source_position  *error_pos)
/*
Report the indicated diagnostic at the indicated position.
*/
{
  pos_st_diagnostic(error_severity, error_code, error_pos, (char *)NULL);
}  /* pos_diagnostic */


void diagnostic(an_error_severity    error_severity,
                an_error_code        error_code)
/*
Report the indicated diagnostic at the position indicated by error_position.
*/
{
  pos_st_diagnostic(error_severity, error_code, &error_position, (char *)NULL);
}  /* diagnostic */


void pos_ty_diagnostic(an_error_severity  error_severity,
                       an_error_code      error_code,
                       a_source_position  *error_pos,
                       a_type_ptr         type)
/*
Report the indicated diagnostic (with the indicated type) at the
indicated position.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     type, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
}  /* pos_ty_diagnostic */


void pos_ty2_diagnostic(an_error_severity  error_severity,
                        an_error_code      error_code,
                        a_source_position  *error_pos,
                        a_type_ptr         type1,
                        a_type_ptr         type2)
/*
Report the indicated diagnostic (with the two indicated types) at the
indicated position.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     type1, type2,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
}  /* pos_ty2_diagnostic */

#if !STANDALONE_UTILITY_PROGRAM

void st_num_diagnostic(an_error_severity error_severity,
                       an_error_code     error_code,
                       a_const_char      *error_string,
                       int32_t           num)
/*
Report the indicated diagnostic (with the indicated string and number fill-ins)
as a positionless diagnostic.
*/
{
  a_diagnostic_ptr dp;

  dp = create_primary_diagnostic(error_code,
                                 /*error_pos=*/&null_source_position,
                                 error_severity);
  add_string_fill_in(dp, error_string);
  add_number_fill_in(dp, num);
  wrap_up_diagnostic(dp);
}  /* st_num_diagnostic */


void pos_sy_diagnostic(an_error_severity  error_severity,
                       an_error_code      error_code,
                       a_source_position  *error_pos,
                       a_symbol_ptr       symbol)
/*
Report the indicated diagnostic (with the indicated symbol) at the
indicated position.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     symbol, (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
}  /* pos_sy_diagnostic */


void pos2_diagnostic(an_error_severity  error_severity,
                     an_error_code      error_code,
                     a_source_position  *error_pos,
                     a_source_position  *other_pos)
/*
Report the indicated diagnostic at the indicated position.  A second position
is also provided.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     other_pos, (a_diag_list_ptr)NULL);
}  /* pos2_diagnostic */


void pos2_st_diagnostic(an_error_severity  error_severity,
                        an_error_code      error_code,
                        a_source_position  *error_pos,
                        a_source_position  *other_pos,
                        a_const_char       *error_string)
/*
Report the indicated diagnostic (with the indicated string fill-in) at the
indicated position.  A second position is also provided.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     error_string, (a_const_char*)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     other_pos, (a_diag_list_ptr)NULL);
}  /* pos2_st_diagnostic */


void pos2_sy_diagnostic(an_error_severity  error_severity,
                        an_error_code      error_code,
                        a_source_position  *error_pos,
                        a_source_position  *other_pos,
                        a_symbol_ptr       symbol)
/*
Report the indicated diagnostic (with the indicated symbol) at the
indicated position, a second position is also provided.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     symbol, (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     other_pos, (a_diag_list_ptr)NULL);
}  /* pos2_sy_diagnostic */

#if MICROSOFT_EXTENSIONS_ALLOWED

void pos2_ty_diagnostic(an_error_severity  error_severity,
                        an_error_code      error_code,
                        a_source_position  *error_pos,
                        a_source_position  *other_pos,
                        a_type_ptr         type)
/*
Report the indicated diagnostic (with the indicated type) at the
indicated position, a second position is also provided.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     (a_const_char*)NULL,
                     (a_const_char*)NULL,
                     (a_symbol_ptr)NULL,
                     (a_symbol_ptr)NULL,
                     type,
                     (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     other_pos, (a_diag_list_ptr)NULL);
}  /* pos2_ty_diagnostic */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void pos_sy_ty2_diagnostic(an_error_severity  error_severity,
                           an_error_code      error_code,
                           a_source_position  *error_pos,
                           a_symbol_ptr       symbol,
                           a_type_ptr         type1,
                           a_type_ptr         type2)
/*
Report the indicated diagnostic (with the indicated symbol and types) at the
indicated position.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     (a_const_char*)NULL,
                     (a_const_char*)NULL,
                     symbol,
                     (a_symbol_ptr)NULL,
                     type1,
                     type2,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
}  /* pos_sy_ty2_diagnostic */


void pos_sy2_diagnostic(an_error_severity  error_severity,
                        an_error_code      error_code,
                        a_source_position  *error_pos,
                        a_symbol_ptr       symbol1,
                        a_symbol_ptr       symbol2)
/*
Report the indicated diagnostic (with the indicated symbols) at the
indicated position.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     (a_const_char*)NULL,
                     (a_const_char*)NULL,
                     symbol1,
                     symbol2,
                     (a_type_ptr)NULL,
                     (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
}  /* pos_sy2_diagnostic */


void sym_diagnostic(an_error_severity  error_severity,
                    an_error_code      error_code,
                    a_symbol_ptr       symbol)
/*
Report the indicated diagnostic (with the indicated symbol) at the position
indicated by error_position.
*/
{
  pos_sy_diagnostic(error_severity, error_code, &error_position, symbol);
}  /* sym_diagnostic */


void pos_syty_diagnostic(an_error_severity  error_severity,
                         an_error_code      error_code,
                         a_source_position  *error_pos,
                         a_symbol_ptr       symbol,
                         a_type_ptr         type)
/*
Report the indicated diagnostic (with the indicated symbol and type) at the
indicated position.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     (a_const_char*)NULL,
                     (a_const_char*)NULL,
                     symbol,
                     (a_symbol_ptr)NULL,
                     type,
                     (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
}  /* pos_syty_diagnostic */


void pos_stsy_diagnostic(an_error_severity  error_severity,
                         an_error_code      error_code,
                         a_source_position  *error_pos,
                         a_const_char       *error_string,
                         a_symbol_ptr       symbol)
/*
Report the indicated diagnostic (with the indicated fill-in string and symbol)
at the indicated position.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     error_string,
                     (a_const_char*)NULL,
                     symbol,
                     (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL,
                     (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
}  /* pos_stsy_diagnostic */


a_diagnostic_ptr pos_st2_unum_start_error(
                                        an_error_code           error_code,
                                        a_source_position       *error_pos,
                                        a_const_char            *error_string1,
                                        a_const_char            *error_string2,
                                        an_unsigned_diag_number num)
/*
Begin a multiple message error with the specified error code, source position,
string fill-ins, and unsigned number.  Return a pointer to the diagnostic entry
that will be passed in for the subsequent messages.
*/
{
  a_diagnostic_ptr dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_error);
  add_string_fill_in(dp, error_string1);
  add_string_fill_in(dp, error_string2);
  add_unsigned_number_fill_in(dp, num);
  return dp;
}  /* pos_st2_unum_start_error */


void pos_st_num2_diagnostic(an_error_severity error_severity,
                            an_error_code     error_code,
                            a_source_position *error_pos,
                            a_const_char      *error_string,
                            int32_t           num1,
                            int32_t           num2)
/*
Report the indicated diagnostic (with the indicated fill-in string and numbers)
at the indicated position.
*/
{
  a_diagnostic_ptr dp;

  dp = create_primary_diagnostic(error_code, error_pos, error_severity);
  add_string_fill_in(dp, error_string);
  add_number_fill_in(dp, num1);
  add_number_fill_in(dp, num2);
  wrap_up_diagnostic(dp);
}  /* pos_st_num2_diagnostic */


void pos_num2_diagnostic(an_error_severity error_severity,
                         an_error_code     error_code,
                         a_source_position *error_pos,
                         int32_t           num1,
                         int32_t           num2)
/*
Report the indicated diagnostic (with the indicated number fill-ins) at the
indicated position.
*/
{
  a_diagnostic_ptr dp;

  dp = create_primary_diagnostic(error_code, error_pos, error_severity);
  add_number_fill_in(dp, num1);
  add_number_fill_in(dp, num2);
  wrap_up_diagnostic(dp);
}  /* pos_num2_diagnostic */


void st_num_add_diag_info(a_diagnostic_ptr primary_dp,
                          an_error_code    error_code,
                          a_const_char     *error_string,
                          int32_t          num)
/*
Add the specified diagnostic message (with the indicated fill-in string and
number) to primary_dp.
*/
{
  a_diagnostic_ptr dp;

  dp = create_sub_message(primary_dp, error_code);
  add_string_fill_in(dp, error_string);
  add_number_fill_in(dp, num);
}  /* st_num_add_diag_info */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static
void pos_stty_diagnostic(an_error_severity  error_severity,
                         an_error_code      error_code,
                         a_source_position  *error_pos,
                         a_const_char       *error_string,
                         a_type_ptr         type)
/*
Report the indicated diagnostic (with the indicated fill-in string and type)
at the indicated position.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     error_string,
                     (a_const_char*)NULL,
                     (a_symbol_ptr)NULL,
                     (a_symbol_ptr)NULL,
                     type,
                     (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
}  /* pos_stty_diagnostic */


void pos_st2_diagnostic(an_error_severity error_severity,
                        an_error_code     error_code,
                        a_source_position *error_pos,
                        a_const_char      *error_string1,
                        a_const_char      *error_string2)
/*
Report the indicated diagnostic (with the indicated fill-in strings) at the
position indicated by error_position.
*/
{
  general_diagnostic(error_severity, error_code, error_pos,
                     error_string1,
                     error_string2,
                     (a_symbol_ptr)NULL,
                     (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL,
                     (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, (a_diag_list_ptr)NULL);
}  /* pos_st2_diagnostic */


void pos_opt_ty2_diagnostic(an_error_severity sev,
                            an_error_code     error_code,
                            a_source_position *error_pos,
                            a_type_ptr        type1,
                            a_type_ptr        type2)
/*
Report the indicated diagnostic (with the two indicated types) at the
indicated position with the given severity.  If the error message has no
fill-ins, do not put the types in the message.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, sev);
  /* See if the error message contains a fill-in for a type.  If so,
     put out the types. */
  if (message_has_fill_in(error_code)) {
    add_type_fill_in(dp, type1);
    add_type_fill_in(dp, type2);
  }  /* if */
  wrap_up_diagnostic(dp);
}  /* pos_opt_ty2_diagnostic */


void pos_st_remark(an_error_code     error_code,
                   a_source_position *error_pos,
                   a_const_char      *error_string)
/*
Report the indicated remark (with the indicated fill-in string) at the
indicated position.
*/
{
  pos_st_diagnostic(es_remark, error_code, error_pos, error_string);
}  /* pos_st_remark */


void pos_remark(an_error_code     error_code,
                a_source_position *error_pos)
/*
Report the indicated remark at the indicated position.
*/
{
  pos_st_remark(error_code, error_pos, (char *)NULL);
}  /* pos_remark */


void str_remark(an_error_code error_code,
                a_const_char  *error_string)
/*
Report the indicated remark (with the indicated fill-in string) at the
position indicated by error_position.
*/
{
  pos_st_remark(error_code, &error_position, error_string);
}  /* str_remark */


void pos_ty_remark(an_error_code     error_code,
                   a_source_position *error_pos,
                   a_type_ptr        type)
/*
Report the indicated remark (with the indicated type) at the
indicated position.
*/
{
  pos_ty_diagnostic(es_remark, error_code, error_pos, type);
}  /* pos_ty_remark */


#if 0
/* These routines are not currently used by the compiler. */

void pos_ty2_remark(an_error_code     error_code,
                    a_source_position *error_pos,
                    a_type_ptr        type1,
                    a_type_ptr        type2)
/*
Report the indicated remark (with the two indicated types) at the
indicated position.
*/
{
  pos_ty2_diagnostic(es_remark, error_code, error_pos, type1, type2);
}  /* pos_ty2_remark */


void type_remark(an_error_code error_code,
                 a_type_ptr    type)
/*
Report the indicated remark (with the indicated type) at the position
indicated by error_position.
*/
{
  pos_ty_remark(error_code, &error_position, type);
}  /* type_remark */

#endif /* 0 */

#if !STANDALONE_UTILITY_PROGRAM

void pos_sy_remark(an_error_code     error_code,
                   a_source_position *error_pos,
                   a_symbol_ptr      symbol)
/*
Report the indicated remark (with the indicated symbol) at the
indicated position.
*/
{
  pos_sy_diagnostic(es_remark, error_code, error_pos, symbol);
}  /* pos_sy_remark */


void sym_remark(an_error_code error_code,
                a_symbol_ptr  symbol)
/*
Report the indicated remark (with the indicated symbol) at the position
indicated by error_position.
*/
{
  pos_sy_remark(error_code, &error_position, symbol);
}  /* sym_remark */


void pos_stsy_remark(an_error_code     error_code,
                     a_source_position *error_pos,
                     a_const_char      *error_string,
                     a_symbol_ptr      symbol)
/*
Report the indicated remark (with the indicated fill-in string and symbol)
at the indicated position.
*/
{
  pos_stsy_diagnostic(es_remark, error_code, error_pos, error_string, symbol);
}  /* pos_stsy_remark */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void pos_st_warning(an_error_code     error_code,
                    a_source_position *error_pos,
                    a_const_char      *error_string)
/*
Report the indicated warning (with the indicated fill-in string) at the
indicated position.
*/
{
  pos_st_diagnostic(es_warning, error_code, error_pos, error_string);
}  /* pos_st_warning */


void pos_warning(an_error_code     error_code,
                 a_source_position *error_pos)
/*
Report the indicated warning at the indicated position.
*/
{
  pos_st_warning(error_code, error_pos, (char *)NULL);
}  /* pos_warning */


void str_warning(an_error_code error_code,
                 a_const_char  *error_string)
/*
Report the indicated warning (with the indicated fill-in string) at the
position indicated by error_position.
*/
{
  pos_st_warning(error_code, &error_position, error_string);
}  /* str_warning */

#if MICROSOFT_EXTENSIONS_ALLOWED

void pos_st2_warning(an_error_code     error_code,
                     a_source_position *error_pos,
                     a_const_char      *error_string1,
                     a_const_char      *error_string2)
/*
Report the indicated warning (with the indicated fill-in strings) at the
position indicated by error_position.
*/
{
  pos_st2_diagnostic(es_warning, error_code, error_pos, error_string1,
                     error_string2);
}  /* pos_st2_warning */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void pos_ty_warning(an_error_code     error_code,
                    a_source_position *error_pos,
                    a_type_ptr        type)
/*
Report the indicated warning (with the indicated type) at the
indicated position.
*/
{
  pos_ty_diagnostic(es_warning, error_code, error_pos, type);
}  /* pos_ty_warning */

void pos_ty2_warning(an_error_code     error_code,
                     a_source_position *error_pos,
                     a_type_ptr        type1,
                     a_type_ptr        type2)
/*
Report the indicated warning (with the two indicated types) at the
indicated position.
*/
{
  pos_ty2_diagnostic(es_warning, error_code, error_pos, type1, type2);
}  /* pos_ty2_warning */


void pos_opt_ty2_warning(an_error_code     error_code,
                         a_source_position *error_pos,
                         a_type_ptr        type1,
                         a_type_ptr        type2)
/*
Report the indicated warning (with the two indicated types) at the
indicated position.  If the error message has no fill-ins, do not
put the types in the message.
*/
{
  pos_opt_ty2_diagnostic(es_warning, error_code, error_pos, type1, type2);
}  /* pos_opt_ty2_warning */


void type_warning(an_error_code error_code,
                  a_type_ptr    type)
/*
Report the indicated warning (with the indicated type) at the position
indicated by error_position.
*/
{
  pos_ty_warning(error_code, &error_position, type);
}  /* type_warning */

#if !STANDALONE_UTILITY_PROGRAM

void pos_syty_warning(an_error_code     error_code,
                      a_source_position *error_pos,
                      a_symbol_ptr      symbol,
                      a_type_ptr        type)
/*
Report the indicated warning (with the indicated symbol and type) at the
indicated position.
*/
{
  pos_syty_diagnostic(es_warning, error_code, error_pos, symbol, type);
}  /* pos_syty_warning */


void pos_sy_warning(an_error_code     error_code,
                    a_source_position *error_pos,
                    a_symbol_ptr      symbol)
/*
Report the indicated warning (with the indicated symbol) at the
indicated position.
*/
{
  pos_sy_diagnostic(es_warning, error_code, error_pos, symbol);
}  /* pos_sy_warning */


void sym_warning(an_error_code error_code,
                 a_symbol_ptr  symbol)
/*
Report the indicated warning (with the indicated symbol) at the position
indicated by error_position.
*/
{
  pos_sy_warning(error_code, &error_position, symbol);
}  /* sym_warning */


void pos_stsy_warning(an_error_code     error_code,
                      a_source_position *error_pos,
                      a_const_char      *error_string,
                      a_symbol_ptr      symbol)
/*
Report the indicated warning (with the indicated fill-in string and symbol)
at the indicated position.
*/
{
  pos_stsy_diagnostic(es_warning, error_code, error_pos, error_string, symbol);
}  /* pos_stsy_warning */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void pos_stty_warning(an_error_code     error_code,
                      a_source_position *error_pos,
                      a_const_char      *error_string,
                      a_type_ptr        type)
/*
Report the indicated warning (with the indicated fill-in string and type) at
the indicated position.
*/
{
  pos_stty_diagnostic(es_warning, error_code, error_pos, error_string, type);
}  /* pos_stty_warning */


void pos_st_error(an_error_code     error_code,
                  a_source_position *error_pos,
                  a_const_char      *error_string)
/*
Report the indicated error (with the indicated fill-in string) at the
indicated position.
*/
{
  pos_st_diagnostic(es_error, error_code, error_pos, error_string);
}  /* pos_st_error */


void pos_st2_error(an_error_code     error_code,
                   a_source_position *error_pos,
                   a_const_char      *error_string1,
                   a_const_char      *error_string2)
/*
Report the indicated error (with the indicated fill-in strings) at the
indicated position.
*/
{
  pos_st2_diagnostic(es_error, error_code, error_pos, error_string1,
                     error_string2);
}  /* pos_st2_error */


void pos_stty_error(an_error_code     error_code,
                    a_source_position *error_pos,
                    a_const_char      *error_string,
                    a_type_ptr        type)
/*
Report the indicated error (with the indicated fill-in string and type) at the
indicated position.
*/
{
  pos_stty_diagnostic(es_error, error_code, error_pos, error_string, type);
}  /* pos_stty_error */


void pos_error(an_error_code     error_code,
               a_source_position *error_pos)
/*
Report the indicated error at the indicated position.
*/
{
  pos_st_error(error_code, error_pos, (char *)NULL);
}  /* pos_error */


void str_error(an_error_code error_code,
               a_const_char  *error_string)
/*
Report the indicated error (with the indicated fill-in string) at the
position indicated by error_position.
*/
{
  pos_st_error(error_code, &error_position, error_string);
}  /* str_error */


void pos_ty_error(an_error_code     error_code,
                  a_source_position *error_pos,
                  a_type_ptr        type)
/*
Report the indicated error (with the indicated type) at the
indicated position.
*/
{
  pos_ty_diagnostic(es_error, error_code, error_pos, type);
}  /* pos_ty_error */


void pos_ty2_error(an_error_code     error_code,
                   a_source_position *error_pos,
                   a_type_ptr        type1,
                   a_type_ptr        type2)
/*
Report the indicated error (with the two indicated types) at the
indicated position.
*/
{
  pos_ty2_diagnostic(es_error, error_code, error_pos, type1, type2);
}  /* pos_ty2_error */


void pos_ty_str_error(an_error_code     error_code,
                      a_source_position *error_pos,
                      a_type_ptr        type,
                      a_const_char      *error_string)
/*
Report the indicated error (with the indicated type and string value) at the
indicated position.
*/
{
  pos_stty_diagnostic(es_error, error_code, error_pos, error_string, type);
}  /* pos_ty_str_error */

#if MICROSOFT_EXTENSIONS_ALLOWED

void pos_ty3_error(an_error_code     error_code,
                   a_source_position *error_pos,
                   a_type_ptr        type1,
                   a_type_ptr        type2,
                   a_type_ptr        type3)
/*
Report the indicated error (with the three indicated types) at the
indicated position.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_error);
  add_type_fill_in(dp, type1);
  add_type_fill_in(dp, type2);
  add_type_fill_in(dp, type3);
  wrap_up_diagnostic(dp);
}  /* pos_ty3_error */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void pos_opt_ty2_error(an_error_code     error_code,
                       a_source_position *error_pos,
                       a_type_ptr        type1,
                       a_type_ptr        type2)
/*
Report the indicated error (with the two indicated types) at the
indicated position.  If the error message has no fill-ins, do not
put the types in the message.
*/
{
  pos_opt_ty2_diagnostic(es_error, error_code, error_pos, type1, type2);
}  /* pos_opt_ty2_error */


void type_error(an_error_code error_code,
                a_type_ptr    type)
/*
Report the indicated error (with the indicated type) at the position
indicated by error_position.
*/
{
  pos_ty_error(error_code, &error_position, type);
}  /* type_error */

#if !STANDALONE_UTILITY_PROGRAM

void pos_stsy_error(an_error_code     error_code,
                    a_source_position *error_pos,
                    a_const_char      *error_string,
                    a_symbol_ptr      symbol)
/*
Report the indicated error (with the indicated fill-in string) at the
indicated position.
*/
{
  pos_stsy_diagnostic(es_error, error_code, error_pos, error_string, symbol);
}  /* pos_stsy_error */


void pos_sy_error(an_error_code     error_code,
                  a_source_position *error_pos,
                  a_symbol_ptr      symbol)
/*
Report the indicated error (with the indicated symbol) at the
indicated position.
*/
{
  pos_sy_diagnostic(es_error, error_code, error_pos, symbol);
}  /* pos_sy_error */


void pos_sy2_error(an_error_code     error_code,
                   a_source_position *error_pos,
                   struct a_symbol   *symbol1,
                   struct a_symbol   *symbol2)
/*
Report the indicated error (with the indicated symbols) at the
indicated position.
*/
{
  pos_sy2_diagnostic(es_error, error_code, error_pos, symbol1, symbol2);
}  /* pos_sy2_error */


void pos_syty_error(an_error_code     error_code,
                    a_source_position *error_pos,
                    a_symbol_ptr      symbol,
                    a_type_ptr        type)
/*
Report the indicated error (with the indicated symbol and type) at the
indicated position.
*/
{
  pos_syty_diagnostic(es_error, error_code, error_pos, symbol, type);
}  /* pos_syty_error */


void sym_error(an_error_code error_code,
               a_symbol_ptr  symbol)
/*
Report the indicated error (with the indicated symbol) at the position
indicated by error_position.
*/
{
  pos_sy_error(error_code, &error_position, symbol);
}  /* sym_error */

#endif /* !STANDALONE_UTILITY_PROGRAM */

#if !STANDALONE_UTILITY_PROGRAM
void syntax_error(an_error_code error_code)
/*
Report the indicated error at the position indicated by error_position,
then get and throw away tokens until a token is read that is in the set
of stop tokens.  This routine is called to report and recover from syntax
errors.
*/
{
  /* Report the error. */
  pos_error(error_code, &error_position);

  /* Flush tokens until something in the stop token set turns up. */
  flush_tokens();
}  /* syntax_error */
#endif /* !STANDALONE_UTILITY_PROGRAM */

#if DO_IL_LOWERING

/*lint -esym(759,pos_ty_catastrophe)*/
/*lint -esym(765,pos_ty_catastrophe)*/
NORETURN void pos_ty_catastrophe(an_error_code     error_code,
                                 a_source_position *error_pos,
                                 a_type            *type)
/*
Report the indicated catastrophic error (with the indicated fill-in type)
at the indicated position, and then terminate the compilation.
*/
{
  pos_ty_diagnostic(es_catastrophe, error_code, error_pos, type);
  /* Avoid spurious warning.  The function above does not return. */
  exit_compilation(es_internal_error);
}  /* pos_ty_catastrophe */

#endif /* DO_IL_LOWERING */

/*lint -esym(759,pos_st_catastrophe)*/
/*lint -esym(765,pos_st_catastrophe)*/
NORETURN void pos_st_catastrophe(an_error_code     error_code,
                                 a_source_position *error_pos,
                                 a_const_char      *error_string)
/*
Report the indicated catastrophic error (with the indicated fill-in string)
at the indicated position, and then terminate the compilation.
*/
{
  pos_st_diagnostic(es_catastrophe, error_code, error_pos, error_string);
  /* Avoid spurious warning.  The function above does not return. */
  exit_compilation(es_internal_error);
}  /* pos_st_catastrophe */


NORETURN void str_catastrophe(an_error_code error_code,
                              a_const_char  *error_string)
/*
Report the indicated catastrophe (with the indicated fill-in string) at the
position indicated by error_position, and then terminate the compilation.
*/
{
  pos_st_catastrophe(error_code, &error_position, error_string);
}  /* str_catastrophe */


NORETURN void pos_str2_catastrophe(an_error_code     error_code,
                                   a_const_char      *error_string1,
                                   a_const_char      *error_string2,
                                   a_source_position *error_pos)
/*
Report the indicated catastrophe (with the indicated fill-in strings) at the
indicated error_position, and then terminate the compilation.
*/
{
  pos_st2_diagnostic(es_catastrophe, error_code, error_pos, error_string1,
                     error_string2);
  /* Avoid spurious warning.  The function above does not return. */
  exit_compilation(es_internal_error);
}  /* pos_str2_catastrophe */

#if EDG_WIN32
#if CPPCLI_ENABLING_POSSIBLE
#if !STANDALONE_UTILITY_PROGRAM

NORETURN void win32_catastrophe(an_ms_dword   error_code,
                                a_const_char  *error_string)
/*
When a WIN32 API fails, issue a diagnostic that describes the failure.
*/
{
  a_const_char	*com_string;
  
  /* Make a copy of the string as the value returned is in a text buffer. */
  com_string = win32_error_to_str(error_code);
  com_string = diag_copy_string(com_string);
  pos_str2_catastrophe(ec_win32_api_error, error_string, com_string,
                       &error_position);
  /* Avoid spurious warning.  The function above does not return. */
  exit_compilation(es_internal_error);
}  /* win32_catastrophe */


NORETURN void hresult_catastrophe(a_const_char *error_string)
/*
When a random COM API (or other API that hopefully uses ISetErrorInfo) fails,
this produces a diagnostic that describes the failure.
*/
{
  a_const_char	*com_string;

  /* Make a copy of the string as the value returned is in a text buffer. */
  com_string = com_error_to_str();
  com_string = diag_copy_string(com_string);
  pos_str2_catastrophe(ec_win32_api_error, error_string, com_string,
                       &error_position);
  /* Avoid spurious warning.  The function above does not return. */
  exit_compilation(es_internal_error);
}  /* hresult_catastrophe */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* CPPCLI_ENABLING_POSSIBLE */
#endif /* EDG_WIN32 */

NORETURN void str_errno_catastrophe(an_error_code error_code,
                                    a_const_char  *error_string,
                                    int           errno_value)
/*
Report the indicated catastrophe with the fill-in string error_string and
with errno converted to a string fill-in at the position indicated by
error_position, and then terminate the compilation.
*/
{
  char *errno_string;

  /* Make a copy of the strerror value to guard against the unlikely
     case of another call to strerror happening somewhere. */
  errno_string = strerror(errno_value);
  errno_string = diag_copy_string(errno_string);
  pos_str2_catastrophe(error_code, error_string,
                       errno_string, &error_position);
}  /* str_errno_catastrophe */


NORETURN static void error_code_errno_catastrophe(
                                      an_error_code error_code,
                                      an_error_code error_code2,
                                      int           errno_value)
/*
Report the indicated catastrophe with the indicated fill-in strings
and with errno converted to a string fill-in at the position indicated by
error_position, and then terminate the compilation.
*/
{
  char *errno_string;

  /* Make a copy of the strerror value to guard against the unlikely
     case of another call to strerror happening somewhere. */
  errno_string = strerror(errno_value);
  errno_string = diag_copy_string(errno_string);
  pos_str2_catastrophe(error_code, error_text(error_code2),
                       errno_string, &error_position);
}  /* error_code_errno_catastrophe */


NORETURN void catastrophe(an_error_code error_code)
/*
Report the indicated catastrophe at the position indicated by error_position,
and then terminate the compilation.
*/
{
  pos_st_catastrophe(error_code, &error_position, (char *)NULL);
}  /* catastrophe */


/* The following routines are used to construct multiple message
   diagnostics with various fill-ins. */

a_diagnostic_ptr pos_start_diagnostic(an_error_severity  error_severity,
                                      an_error_code      error_code,
                                      a_source_position  *error_pos)
/*
Begin a multiple message diagnostic with the specified severity, error code,
and source position.  Return a pointer to the diagnostic entry that
will be passed in for the subsequent messages.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, error_severity);
  return dp;
}  /* pos_start_diagnostic */


a_diagnostic_ptr pos_ty_start_diagnostic(an_error_severity  error_severity,
                                         an_error_code      error_code,
                                         a_source_position *error_pos,
                                         struct a_type     *type)
/*
Begin a multiple message diagnostic with the specified severity, error code,
source position, and type fill-in.  Return a pointer to the diagnostic entry
that will be passed in for the subsequent messages.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, error_severity);
  add_type_fill_in(dp, type);
  return dp;
}  /* pos_ty_start_diagnostic */


a_diagnostic_ptr pos_start_error(an_error_code     error_code,
                                 a_source_position *error_pos)
/*
Begin a multiple message error with the specified error code and source
position.  Return a pointer to the diagnostic entry that will be passed
in for the subsequent messages.
*/
{
  return pos_start_diagnostic(es_error, error_code, error_pos);
}  /* pos_start_error */


a_diagnostic_ptr pos_st_start_error(an_error_code     error_code,
                                    a_source_position *error_pos,
                                    a_const_char      *error_string)
/*
Begin a multiple message error with the specified error code, source
position and string fill-in.  Return a pointer to the diagnostic entry
that will be passed in for the subsequent messages.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_error);
  add_string_fill_in(dp, error_string);
  return dp;
}  /* pos_st_start_error */


a_diagnostic_ptr pos_ty_start_error(an_error_code     error_code,
                                    a_source_position *error_pos,
                                    a_type_ptr        type)
/*
Begin a multiple message error with the specified error code, source
position and type fill-in.  Return a pointer to the diagnostic entry
that will be passed in for the subsequent messages.
*/
{
  return pos_ty_start_diagnostic(es_error, error_code, error_pos, type);
}  /* pos_ty_start_error */


a_diagnostic_ptr pos_ty2_start_error(an_error_code     error_code,
                                     a_source_position *error_pos,
                                     a_type_ptr        type1,
                                     a_type_ptr        type2)
/*
Begin a multiple message error with the specified error code, source
position, and 2 types as fill-ins.  Return a pointer to the diagnostic entry
that will be passed in for the subsequent messages.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_error);
  add_type_fill_in(dp, type1);
  add_type_fill_in(dp, type2);
  return dp;
}  /* pos_ty2_start_error */


void ty_add_diag_info(a_diagnostic_ptr primary_dp,
                      an_error_code    error_code,
                      a_type_ptr       type)

/*
Add the specified diagnostic message with the type substitution to primary_dp.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_sub_message(primary_dp, error_code);
  add_type_fill_in(dp, type);
}  /* ty_add_diag_info */


void str_add_diag_info(a_diagnostic_ptr primary_dp,
                       an_error_code    error_code,
                       a_const_char     *error_string)
/*
Add the specified diagnostic message with the string substitution to
primary_dp.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_sub_message(primary_dp, error_code);
  add_string_fill_in(dp, error_string);
}  /* str_add_diag_info */


void copy_str_add_diag_info(a_diagnostic_ptr primary_dp,
                            an_error_code    error_code,
                            a_const_char     *error_string)
/*
Add the specified diagnostic message with the string substitution to
primary_dp.  Make a copy of the string and use the copy for substitution.
*/
{
  a_diagnostic_ptr	dp;
  a_const_char		*string_copy;

  string_copy = diag_copy_string(error_string);
  dp = create_sub_message(primary_dp, error_code);
  add_string_fill_in(dp, string_copy);
}  /* copy_str_add_diag_info */


void add_diag_info(a_diagnostic_ptr primary_dp,
                   an_error_code    error_code)
/*
Add the specified diagnostic message to the primary_dp.
*/
{
  (void)create_sub_message(primary_dp, error_code);
}  /* add_diag_info */

#if !STANDALONE_UTILITY_PROGRAM

void add_diag_info_with_pos_insert(a_diagnostic_ptr	primary_dp,
				   an_error_code	error_code,
				   a_source_position	*pos)
/*
Add the specified diagnostic message to primary_dp.  pos refers to a source
position that will be embedded in the message text using the "%p" convention.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_sub_message(primary_dp, error_code);
  add_position_fill_in(dp, pos);
}  /* add_diag_info_with_pos_insert */


a_diagnostic_ptr pos_sy_start_diagnostic(an_error_severity  error_severity,
                                         an_error_code      error_code,
                                         a_source_position  *error_pos,
                                         a_symbol_ptr       symbol)
/*
Begin a multiple message diagnostic with the specified severity, error code,
source position, and symbol fill-in.  Return a pointer to the diagnostic
entry that will be passed in for the subsequent messages.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, error_severity);
  add_symbol_fill_in(dp, symbol);
  return dp;
}  /* pos_sy_start_diagnostic */


a_diagnostic_ptr pos_stsy_start_diagnostic(an_error_severity  error_severity,
                                           an_error_code      error_code,
                                           a_source_position  *error_pos,
                                           a_const_char       *error_string,
                                           struct a_symbol    *symbol)
/*
Begin a multiple message diagnostic with the specified severity, error code,
source position, string, and symbol fill-in.  Return a pointer to the
diagnostic entry that will be passed in for the subsequent messages.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, error_severity);
  add_string_fill_in(dp, error_string);
  add_symbol_fill_in(dp, symbol);
  return dp;
}  /* pos_stsy_start_diagnostic */


a_diagnostic_ptr pos_sy_start_error(an_error_code     error_code,
                                    a_source_position *error_pos,
                                    a_symbol_ptr      symbol)
/*
Begin a multiple message error with the specified error code, source
position and symbol fill-in.  Return a pointer to the diagnostic entry that
will be passed in for the subsequent messages.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_error);
  add_symbol_fill_in(dp, symbol);
  return dp;
}  /* pos_sy_start_error */


a_diagnostic_ptr pos_stsy_start_error(an_error_code     error_code,
                                      a_source_position *error_pos,
                                      a_const_char      *error_string,
                                      a_symbol_ptr      symbol)
/*
Begin a multiple message error with the specified error code, source
position and symbol fill-in.  Return a pointer to the diagnostic entry that
will be passed in for the subsequent messages.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_error);
  add_string_fill_in(dp, error_string);
  add_symbol_fill_in(dp, symbol);
  return dp;
}  /* pos_stsy_start_error */


void pos_sy2_warning(an_error_code     error_code,
                     a_source_position *error_pos,
                     struct a_symbol   *symbol1,
                     struct a_symbol   *symbol2)
/*
Report the indicated warning (with the indicated symbols) at the
indicated position.
*/
{
  pos_sy2_diagnostic(es_warning, error_code, error_pos, symbol1, symbol2);
}  /* pos_sy2_warning */


void sym_add_diag_info(a_diagnostic_ptr primary_dp,
                       an_error_code    error_code,
                       a_symbol_ptr     symbol)
/*
Add the specified diagnostic message with the symbol substitution to
primary_dp.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_sub_message(primary_dp, error_code);
  add_symbol_fill_in(dp, symbol);
}  /* sym_add_diag_info */


void pos_sy_add_diag_info(a_diagnostic_ptr      primary_dp,
                          an_error_code         error_code,
                          a_source_position_ptr pos,
                          a_symbol_ptr          symbol)
/*
Add the specified diagnostic message with the indicated position and symbol
substitution to primary_dp.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_sub_message(primary_dp, error_code);
  add_position_fill_in(dp, pos);
  add_symbol_fill_in(dp, symbol);
}  /* pos_sy_add_diag_info */


void add_more_info_list(a_diagnostic_ptr	dp,
			a_diag_list_ptr		dlp)
/*
Add the list of "more information" diagnostics specified by dlp to the
primary diagnostic dp.
*/
{
  dp->more_info = *dlp;
  /* Clear the entries in the list passed in. */
  clear_diag_list(dlp);
}  /* add_more_info_list */


void splice_diag_list(a_diag_list_ptr   src,
                      a_diag_list_ptr   dst,
                      a_diagnostic_ptr  insert_after)
/*
Splice the diagnostic list src into the list dst, either after the diagnostic
pointed to by insert_after (if non-NULL), or at the front of the list if
insert_after is NULL.
*/
{
  if (insert_after != NULL) {
    if (insert_after == dst->tail) {
      dst->tail = src->tail;
    } else {
      check_assertion(src->tail != NULL);
      src->tail->next = insert_after->next;
    }  /* if */
    insert_after->next = src->head;
  } else {
    check_assertion(src->tail != NULL);
    src->tail->next = dst->head;
    dst->head = src->head;
  }  /* if */
}  /* splice_diag_list */


void discard_more_info_list(a_diag_list_ptr	dlp)
/*
Discard the list of "more information" diagnostics specified by dlp.  The
list is freed and the list passed in is reset.
*/
{
  free_diag_list_elements(dlp);
  /* Clear the entries in the list passed in. */
  clear_diag_list(dlp);
}  /* discard_more_info_list */


void more_info_diagnostic(an_error_code     error_code,
                          a_source_position *error_pos,
                          a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.
*/
{
  general_diagnostic(es_more_info, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, diag_list);
}  /* more_info_diagnostic */


void more_info_type_diagnostic(an_error_code     error_code,
                               a_source_position *error_pos,
                               struct a_type     *tp,
                               a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given type is used to replace
a %t placeholder in the diagnostic string.
*/
{
  general_diagnostic(es_more_info, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     tp, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, diag_list);
}  /* more_info_type_diagnostic */


void more_info_type2_diagnostic(an_error_code     error_code,
                                a_source_position *error_pos,
                                struct a_type     *tp1,
                                struct a_type     *tp2,
                                a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given types are used to replace
%t1 and %t2 placeholders in the diagnostic string.
*/
{
  general_diagnostic(es_more_info, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     tp1, tp2,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, diag_list);
}  /* more_info_type2_diagnostic */


void more_info_sym_diagnostic(an_error_code     error_code,
                              a_source_position *error_pos,
                              a_symbol_ptr      sym,
                              a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given symbol is used to replace
a %n placeholder in the diagnostic string.
*/
{
  general_diagnostic(es_more_info, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     sym, (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, diag_list);
}  /* more_info_sym_diagnostic */


void more_info_sym_type_diagnostic(an_error_code     error_code,
                                   a_source_position *error_pos,
                                   a_symbol_ptr      sym,
                                   a_type_ptr        type,
                                   a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given symbol and type are used to
replace %n and %t placeholders in the diagnostic string.
*/
{
  general_diagnostic(es_more_info, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     sym, (a_symbol_ptr)NULL,
                     type, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, diag_list);
}  /* more_info_sym_type_diagnostic */


void more_info_sym2_diagnostic(an_error_code     error_code,
                              a_source_position *error_pos,
                              a_symbol_ptr      sym1,
                              a_symbol_ptr      sym2,
                              a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given symbols are used to replace
%n1 and %n2 placeholders in the diagnostic string.
*/
{
  general_diagnostic(es_more_info, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     sym1, sym2,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     (a_template_arg_ptr)NULL, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, diag_list);
}  /* more_info_sym2_diagnostic */


void more_info_num_diagnostic(an_error_code     error_code,
                              a_source_position *error_pos,
                              int32_t           num,
                              a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given number is used for the %d
fill-in.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_more_info);
  add_number_fill_in(dp, num);
  check_assertion(diag_list != NULL);
  /* Add the diagnostic to the given list. */
  diag_list->append(dp);
}  /* more_info_num_diagnostic */


void more_info_num2_diagnostic(an_error_code     error_code,
                               a_source_position *error_pos,
                               int32_t           num1,
                               int32_t           num2,
                               a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given numbers are used for fill-ins
%d1 and %d2.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_more_info);
  add_number_fill_in(dp, num1);
  add_number_fill_in(dp, num2);
  check_assertion(diag_list != NULL);
  /* Add the diagnostic to the given list. */
  diag_list->append(dp);
}  /* more_info_num2_diagnostic */


void more_info_st_diagnostic(an_error_code     error_code,
                             a_source_position *error_pos,
                             a_const_char      *fill_in_str,
                             a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given string is used for the %s
fill-in.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_more_info);
  add_string_fill_in(dp, fill_in_str);
  check_assertion(diag_list != NULL);
  /* Add the diagnostic to the given list. */
  diag_list->append(dp);
}  /* more_info_st_diagnostic */


void more_info_st3_diagnostic(an_error_code     error_code,
                              a_source_position *error_pos,
                              a_const_char      *str1,
                              a_const_char      *str2,
                              a_const_char      *str3,
                              a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given strings are used for fill-ins.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_more_info);
  add_string_fill_in(dp, str1);
  add_string_fill_in(dp, str2);
  add_string_fill_in(dp, str3);
  check_assertion(diag_list != NULL);
  /* Add the diagnostic to the given list. */
  diag_list->append(dp);
}  /* more_info_st3_diagnostic */


void more_info_sym_num_diagnostic(an_error_code     error_code,
                                  a_source_position *error_pos,
                                  struct a_symbol   *sym,
                                  int32_t           num,
                                  a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given symbol is used to replace the
%n fill-in and the given number is used for the %d fill-in.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_more_info);
  add_symbol_fill_in(dp, sym);
  add_number_fill_in(dp, num);
  check_assertion(diag_list != NULL);
  /* Add the diagnostic to the given list. */
  diag_list->append(dp);
}  /* more_info_sym_num_diagnostic */


void more_info_sym_num_ty_diagnostic(an_error_code     error_code,
                                     a_source_position *error_pos,
                                     a_symbol_ptr      sym,
                                     int32_t           num,
                                     a_type_ptr        tp,
                                     a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given symbol, integer value, and
type are used for fill-ins.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_more_info);
  add_symbol_fill_in(dp, sym);
  add_number_fill_in(dp, num);
  add_type_fill_in(dp, tp);
  check_assertion(diag_list != NULL);
  /* Add the diagnostic to the given list. */
  diag_list->append(dp);
}  /* more_info_sym_num_ty_diagnostic */


void more_info_st_num_diagnostic(an_error_code     error_code,
                                 a_source_position *error_pos,
                                 a_const_char      *fill_in_str,
                                 int32_t           num,
                                 a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given string is used to replace the
%s fill-in and the given number is used for the %d fill-in.
*/
{
  a_diagnostic_ptr	dp;

  dp = create_primary_diagnostic(error_code, error_pos, es_more_info);
  add_string_fill_in(dp, fill_in_str);
  add_number_fill_in(dp, num);
  check_assertion(diag_list != NULL);
  /* Add the diagnostic to the given list. */
  diag_list->append(dp);
}  /* more_info_st_num_diagnostic */


void more_info_tap_diagnostic(an_error_code     error_code,
                              a_source_position *error_pos,
                              a_template_arg    *tap,
                              a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given template argument list is used
to replace a %T placeholder in the diagnostic string.
*/
{
  general_diagnostic(es_more_info, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     tap, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, diag_list);
}  /* more_info_tap_diagnostic */


void more_info_sym_tap_diagnostic(an_error_code     error_code,
                                  a_source_position *error_pos,
                                  a_symbol_ptr      sym,
                                  a_template_arg    *tap,
                                  a_diag_list_ptr   diag_list)
/*
Add the indicated diagnostic with the associated position to the list of
diagnostics pointed to by diag_list.  The given symbol and template argument
list are used to replace a %n and %T placeholders, respectively, in the
diagnostic string.
*/
{
  general_diagnostic(es_more_info, error_code, error_pos,
                     (a_const_char*)NULL, (a_const_char*)NULL,
                     sym, (a_symbol_ptr)NULL,
                     (a_type_ptr)NULL, (a_type_ptr)NULL,
                     tap, (a_template_arg_ptr)NULL,
                     (a_source_position*)NULL, diag_list);
}  /* more_info_sym_tap_diagnostic */


void pch_message(an_error_code error_code,
		 a_const_char  *fill_in_str)
/*
Display a message of the form:

source_file: creating precompiled header file "file".

This is used to display the messages that indicate a precompiled header is
being created or used.  The text from the error message file must supply
two string fill-ins for the source file name and PCH file name.
*/
{
  a_const_char *text;

  if (!suppress_pch_messages) {
    text = error_text(error_code);
    fprintf(f_error, text, primary_source_file_name, fill_in_str);
    fprintf(f_error, "\n");
  }  /* if */
}  /* pch_message */


void embedded_cplusplus_noncompliance_diagnostic(a_source_position  *error_pos,
                                                 an_error_code      error_code)
/*
Issue a discretionary error for use of a feature that does not belong to the
"Embedded C++" subset.  *error_pos is the source position with which the
diagnostic is associated; error_code indicates the message to be issued.
*/
{
  an_error_severity  severity;

  /* Implementations may elect to hard-code a reduction in the severity of
     of the diagnostics.  Otherwise, users can control it from the command
     line (e.g., --diag_warning=ec_not_part_of_embedded_cplusplus). */
  severity = es_discretionary_error;
  pos_diagnostic(severity, error_code, error_pos);
}  /* embedded_cplusplus_noncompliance_diagnostic */


static a_pragma_diag_elem *insert_into_pragma_diag_list(
                                               const a_pragma_diag_elem &entry)
/*
Allocate and insert an element (with the values given by entry) into the proper
place in the pragma_diag_list (which is kept sorted by source location).  No
action is taken if an element with the same source position is already on the
list.  Return a pointer to the newly-allocated element in the array (or NULL if
it was already there).
*/
{
  a_pragma_diag_elem *elem = alloc_fe_of_type(a_pragma_diag_elem);

  *elem = entry;
  if (pragma_diag_list == NULL) {
    /* Empty list. */
    pragma_diag_list = elem;
    pragma_diag_tail = elem;
    check_assertion(elem->next == NULL && elem->prev == NULL);
  } else if (pragma_diag_tail->spos < elem->spos) {
    /* Typical case: new element goes at the end of the list. */
    elem->prev = pragma_diag_tail;
    pragma_diag_tail->next = elem;
    pragma_diag_tail = elem;
    check_assertion(elem->next == NULL);
  } else {
    a_pragma_diag_elem *found;
    found = pragma_diag_list_lower_bound(elem);
    if (found == NULL) {
      /* Didn't find anything; insert as the first entry. */
      elem->next = pragma_diag_list;
      pragma_diag_list = elem;
      check_assertion(elem->prev == NULL);
    } else {
      if (*elem == *found) {
        /* Nothing to do; this entry is already on the list. */
        free_fe(elem);
        elem = NULL;
      } else {
        /* Insert item after the one we found. */
        elem->prev = found;
        elem->next = found->next;
        found->next = elem;
        if (pragma_diag_tail == found) {
          pragma_diag_tail = elem;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return elem;
}  /* insert_into_pragma_diag_list */


void diag_pragma(a_pending_pragma_ptr	ppp)
/*
The routine called when a "diag_xxx" pragma is encountered.  The form of
the pragma is

	#pragma diag_xxx [=] arg, arg, arg

where "arg" is either an error number or an error tag.  Note that these
pragmas are parsed multiple times during instantiations, but are only acted
upon when not in an instantiation (except for function-style pragmas such
as _Pragma that are not parsed except during instantiations).
*/
{
  a_pragma_kind_description_ptr	pkdp = ppp->descr_ptr;
  a_pragma_kind			kind = pkdp->kind;
  a_boolean			error_in_pragma = FALSE;
  a_source_position		pos;

  begin_rescan_of_pragma_tokens(ppp);
  /* Bypass the optional "=". */
  if (curr_token == tok_assign) (void)get_token();
  do {
    a_boolean			err = FALSE;
    a_host_large_integer	error_number = 0;
    pos = pos_curr_token;
    if (curr_token == tok_int_constant) {
      /* The argument is an integer, which is expected to be an error
         number. */
      error_number = value_of_integer_constant(&const_for_curr_token, &err);
      if (!err) {
        /* Make sure the integer represents a valid error code. */
        err = error_number <= (int)ec_no_error || error_number >= (int)ec_last;
      }  /* if */
      if (err) {
        pos_warning(ec_invalid_error_number, &pos_curr_token);
      }  /* if */
    } else if (curr_token == tok_identifier) {
      /* The argument is an identifier, which is expected to name an error
         tag. */
      a_const_char *error_tag = locator_for_curr_id.symbol_header->identifier;
      /* The routine will return TRUE if the tag is invalid. */
      error_number = (a_host_large_integer)
                              convert_error_tag_to_error_code(error_tag, &err);
      if (err) {
        pos_warning(ec_invalid_error_tag, &pos_curr_token);
      }  /* if */
    } else {
      /* Not an error number or an error tag. */
      err = TRUE;
      pos_warning(ec_exp_error_argument, &pos_curr_token);
    }  /* if */
    /* Advance past the argument.  Do not consume tok_end_of_source here.
       Fetching that terminator already popped the pragma token cache, so
       another get_token would read the source that follows the pragma and
       could queue later pragmas while this one is still being processed. */
    if (curr_token != tok_end_of_source) (void)get_token();
    if (curr_token != tok_comma && curr_token != tok_end_of_source) {
      pos_warning(ec_exp_comma, &pos_curr_token);
      error_in_pragma = TRUE;
    }  /* if */
    if (!err) {
      /* Note that the severity for this error code has been modified in some
         fashion. */
      error_codes[(int)error_number].severity_changed_by_pragma = TRUE;
      if (kind == pk_diag_once) {
        /* diag_once pragmas are not queued on the list and are instead
           acted upon immediately. */
        err = set_severity_for_error_number((int)error_number, es_once,
                                            /*make_default=*/FALSE);
        check_assertion(!err);
      } else {
        /* Add the #pragma to the list.  #pragmas are generally encountered
           only once, but in the case of deferred class fixups, may be
           encountered more than once.  In that case this entry is
           effectively discarded (as it's already on the list). */
        (void)insert_into_pragma_diag_list({kind, &pos, (int)error_number});
      }  /* if */
    }  /* if */
  } while (loop_token(tok_comma));
  /* Stop rescanning tokens from the pragma token cache. */
  wrapup_rescan_of_pragma_tokens(error_in_pragma);
}  /* diag_pragma */


void diagnostic_pragma(a_pending_pragma_ptr	ppp)
/*
The routine called when a "diagnostic push/pop" pragma is encountered.  The
form of the pragma is

	#pragma diagnostic push|pop

The effect is to push the severities of all diagnostics, then later pop the
previously pushed severities.  Note that these pragmas are parsed multiple
times during instantiations, but are only acted upon when not in an
instantiation (except for function-style pragmas such as _Pragma that are not
parsed except during instantiations).
*/
{
  a_boolean    err = FALSE, is_push = FALSE;

  begin_rescan_of_pragma_tokens(ppp);
  if (curr_token == tok_identifier) {
    /* There should be a single "push" or "pop" argument. */
    a_const_char *str = locator_for_curr_id.symbol_header->identifier;
    if (strcmp(str, "push") == 0) {
      is_push = TRUE;
    } else if (strcmp(str, "pop") == 0) {
      is_push = FALSE;
    } else {
      pos_warning(ec_exp_push_pop, &pos_curr_token);
      err = TRUE;
    }  /* if */
    /* Bypass the token just processed. */
    (void)get_token();
  } else {
    /* Missing argument. */
    pos_warning(ec_exp_push_pop, &pos_curr_token);
    err = TRUE;
  }  /* if */
  if (!err) {
    /* Add the #pragma to the list.  #pragmas are generally encountered
       only once, but in the case of deferred class fixups, may be
       encountered more than once.  In that case this entry will not be
       inserted (and ptr will be NULL). */
    a_pragma_diag_elem *ptr =
                insert_into_pragma_diag_list({pk_diagnostic, &pos_curr_token});

    if (ptr != NULL) {
      if (is_push) {
        /* Push the entry onto a stack so it can be associated with a later
           "pop". */
        check_assertion(pragma_diag_list != NULL);
        pragma_push_stack->push_back(ptr);
      } else {
        /* Link to associated "push". */
        ptr->is_pop = TRUE;
        if (!pragma_push_stack->is_empty()) {
          a_pragma_diag_elem *last_push = pragma_push_stack->back_elem();
          check_assertion(last_push->kind == pk_diagnostic &&
                          !last_push->is_pop);
          ptr->variant.corresponding_push = last_push;
          pragma_push_stack->pop_back();
        } else {
          /* There was no "push" associated with this "pop". */
          ptr->variant.corresponding_push = NULL;
          pos_warning(ec_no_corresponding_push, &pos_curr_token);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* Flush to the end of the line. */
  while (curr_token != tok_newline && curr_token != tok_end_of_source) {
    (void)get_token();
  }  /* while */
  /* Stop rescanning tokens from the pragma token cache. */
  wrapup_rescan_of_pragma_tokens(err);
}  /* diagnostic_pragma */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void end_diagnostic(a_diagnostic_ptr	dp)
/*
Complete the multiple message (dp) diagnostic currently being processed.
*/
{
  wrap_up_diagnostic(dp);
}  /* end_diagnostic */


a_diagnostic_ptr start_command_line_error(an_error_code  error_code,
			                  a_const_char   *error_string)
/*
Begin a multiple message command line error.  error_string is a string
to be used as fill-in and can be NULL if there is no fill-in.  Return a
pointer to the diagnostic entry that will be passed in for the subsequent
messages.
*/
{
  a_diagnostic_ptr	dp;

  set_position_to(error_position, 0, SP_COL_CMD_LINE);
  dp = create_primary_diagnostic(error_code, &error_position,
                                 es_command_line_error);
  if (error_string != NULL) {
    /* Make a copy of the string fill-in. */
    error_string = diag_copy_string(error_string);
    add_string_fill_in(dp, error_string);
  }  /* if */
  return dp;
}  /* start_command_line_error */


NORETURN void end_command_line_error(a_diagnostic_ptr dp)
/*
Complete the multiple message (dp) command line error currently being
processed.
*/
{
  wrap_up_diagnostic(dp);
  /* Avoid spurious warning.  The function above does not return. */
  exit_compilation(es_internal_error);
}  /* end_command_line_error */


static void close_f_error_if_open()
/*
Close the error output FILE* (f_error) if it's open (and not a standard output
stream).
*/
{
#if MAKE_FRONT_END_CALLABLE
  if (f_error != stderr && f_error != stdout) {
    close_file_if_open(&f_error);
  }  /* if */
#endif /* MAKE_FRONT_END_CALLABLE */
}  /* close_f_error_if_open */


static void reset_f_error()
/*
Reset the error output FILE* (f_error).  In DEBUG configurations, additionally
reset the associated debug output FILE* (f_debug).
*/
{
  close_f_error_if_open();
  f_error = default_error_output_file();
#if DEBUG
  f_debug = f_error;
#endif /* DEBUG */
}  /* reset_f_error */


void error_early_init(void)
/*
Do initialization that needs to be done very early, specifically before command
line processing is done.
*/
{
#if CHECKING
  internal_error_loop = FALSE;
#endif /* CHECKING */
  /* The initialization of f_error is also done in cfe.c, but is reset here so
     in case the front end is reinitialized outside of cfe_main. */
  reset_f_error();
  diag_memory_region = FRONT_END_REGION_NUMBER;
  diagnostic_line_length = MAX_ERROR_OUTPUT_LINE_LENGTH;
  msg_buffer = NULL;
  prefix_buffer = NULL;
  write_diagnostic_buffer = NULL;
  catastrophe_has_occurred = FALSE;
  error_threshold = es_warning;
  error_promotion_threshold = es_error;
  error_limit = 100;
  context_limit = DEFAULT_CONTEXT_LIMIT;
  strict_ansi_error_severity = es_warning;
  strict_ansi_discretionary_severity = es_warning;
  /* These are initialized here and also during per-compilation
     initialization. */
  diagnostic_counters = {};
  globally_suppress_diagnostics = FALSE;
  anachronism_error_severity
#if DEFAULT_ALLOW_ANACHRONISMS
                             = es_warning;
#else /* DEFAULT_ALLOW_ANACHRONISMS */
                             = es_error;
#endif /* DEFAULT_ALLOW_ANACHRONISMS */
  brief_diagnostics = DEFAULT_BRIEF_DIAGNOSTICS;
  do_not_wrap_diagnostics = FALSE;
  display_error_context_on_catastrophe =
                                  DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE;
  display_template_typedefs_in_diagnostics =
                              DEFAULT_DISPLAY_TEMPLATE_TYPEDEFS_IN_DIAGNOSTICS;
#if FULLY_RESOLVED_MACRO_POSITIONS
  macro_positions_in_diagnostics = DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  /* Zeroing this array causes it to be set to default values (e.g.,
     es_default). */
  memzero((a_void_ptr)error_codes, sizeof(error_codes));
  error_codes_initialized = TRUE;
#if !STANDALONE_UTILITY_PROGRAM
  error_source_line = NULL;
  after_end_of_error_source_line = NULL;
  f_err_src_file = NULL;
#endif /* !STANDALONE_UTILITY_PROGRAM */
  /* Annotation (and thus effectively colorization) is disabled here (and
     re-enabled in cmd_line.c) so early diagnostics work properly. */
  annotate_diagnostics = FALSE;
  colorize_diagnostics = DEFAULT_ENABLE_COLORIZED_DIAGNOSTICS;
  sgr_string_for_colored_diagnostics = NULL;
  memzero((a_void_ptr)sgr_map, sizeof(sgr_map));
}  /* error_early_init */


void error_one_time_init(void)
/*
Do one-time initialization of variables related to the error routines.
(Variables that need to be reinitialized with each new translation unit
are handled in error_init.)
*/
{
#if !STANDALONE_UTILITY_PROGRAM
  /* Save variables from error.h and error.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(head_of_file_index_list),
      pch_saved_var_array_elem(tail_of_file_index_list),
      pch_saved_var_array_elem(avail_diagnostics),
      pch_saved_var_array_elem(avail_diag_fill_ins),
      pch_saved_var_array_elem(error_position),
      pch_array_saved_var_array_elem(error_codes),
      pch_saved_var_array_elem(pragma_diag_list),
      pch_saved_var_array_elem(pragma_diag_tail),
      pch_saved_var_array_elem(pragma_push_stack),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
#if EXPENSIVE_CHECKING
  verify_label_fill_in_entries();
#endif /* EXPENSIVE_CHECKING */
#endif /* !STANDALONE_UTILITY_PROGRAM */
}  /* error_one_time_init */


void error_trans_unit_init(void)
/*
Initialize variables that are specific to a given translation unit.
*/
{
#if !STANDALONE_UTILITY_PROGRAM
  pragma_diag_list = NULL;
  pragma_diag_tail = NULL;
  pragma_push_stack = alloc_fe_of_type(Dyn_array<a_pragma_diag_elem*>);
  construct(pragma_push_stack, /*cap=*/16u);
#endif /* !STANDALONE_UTILITY_PROGRAM */
}  /* error_trans_unit_init */


void error_init(void)
/*
Perform any initializations necessary for error.c functions at the beginning
of each compilation.
*/
{
  diagnostic_counters = {};
  globally_suppress_diagnostics = FALSE;
  avail_diagnostics = NULL;
  avail_diag_fill_ins = NULL;
  diagnostic_indent = 0;
#if DEBUG
  num_diagnostics_allocated = 0;
  num_diag_fill_ins_allocated = 0;
#endif /* DEBUG */
  memzero((char *)recorded_diagnostic_table,
          sizeof(recorded_diagnostic_table));
  /* The error_codes array has been initialized in early_error_init, so there
     is no need to re-initialize it for the first compilation unit. */
  if (!error_codes_initialized) {
    for (an_error_code_entry *ptr = error_codes;
         ptr < &error_codes[(int)ec_last];
         ptr++) {
      /* Re-initialize all fields except default_severity. */
      ptr->current_severity = es_default;
      ptr->once = FALSE;
      ptr->diagnostic_issued = FALSE;
      ptr->severity_changed_by_pragma = FALSE;
    }  /* for */
  }  /* if */
  error_codes_initialized = FALSE;
#if !STANDALONE_UTILITY_PROGRAM
  clear_file_index_list();
  can_locate_source_line_info_cached = FALSE;
  cached_seq_number = 0;
  cached_unicode_source_kind = usk_none;
  cached_can_locate_source_line = FALSE;
#endif /* !STANDALONE_UTILITY_PROGRAM */
#if CHECKING
  expected_error_record.filename = NULL;
  expected_error_record.line_number = 0;
  expected_error_record.string1 = NULL;
  expected_error_record.string2 = NULL;
#endif /* CHECKING */
}  /* error_init */

#if DEBUG

unsigned long show_error_space_used(void)
/*
Display and return the amount of memory used for error processing
structures for space tracking purposes.
*/
{
  unsigned long num, size, total, grand_total = 0;

  db_space_used_header("Error table use:");
  db_space_used_lost("diagnostic", avail_diagnostics,
                     num_diagnostics_allocated, a_diagnostic);
  db_space_used_lost("diag fill-in", avail_diag_fill_ins,
                     num_diag_fill_ins_allocated, a_diag_fill_in);
  db_space_used_total();
  return grand_total;
}  /* show_error_space_used */

#endif /* DEBUG */

namespace detail {

#if !STANDALONE_UTILITY_PROGRAM

void Fill_in<long long>::add(a_diagnostic_ptr diag,
                             long long        value)
/*
Add a number fill-in entry for the given signed integer (value) to the
diagnostic specified by diag.
*/
{
  add_number_fill_in(diag, value);
}  /* Fill_in<long long>::add */


void Fill_in<unsigned long long>::add(a_diagnostic_ptr   diag,
                                      unsigned long long value)
/*
Add an unsigned number fill-in entry for the given unsigned integer (value) to
the diagnostic specified by diag.
*/
{
  add_unsigned_number_fill_in(diag, value);
}  /* Fill_in<unsigned long long>::add */

#endif /* !STANDALONE_UTILITY_PROGRAM */

void Fill_in<const a_source_position*>::add(a_diagnostic_ptr        diag,
                                            const a_source_position *value)
/*
Add a position fill-in entry for the given source position (value) to the
diagnostic specified by diag.
*/
{
  add_position_fill_in(diag, const_cast<a_source_position*>(value));
}  /* Fill_in<a_source_position*>::add */


void Fill_in<a_const_char*>::add(a_diagnostic_ptr diag,
                                 a_const_char     *value)
/*
Add a string fill-in entry for the given string (value) to the diagnostic
specified by diag.
*/
{
  add_string_fill_in(diag, value);
}  /* Fill_in<a_const_char*>::add */


void Fill_in<a_module*>::add(a_diagnostic_ptr diag,
                             a_module         *value)
/*
Add a string fill-in entry for the given module (value) to the diagnostic
specified by diag.
*/
{
  a_diag_fill_in_ptr dfip = alloc_diag_fill_in(dfk_module);

  dfip->variant.mod = value;
  add_fill_in_to_diagnostic(diag, dfip);
}  /* Fill_in<a_const_char*>::add */


void Fill_in<a_string_view>::add(a_diagnostic_ptr diag,
                                 a_string_view    value)
/*
Add a string fill-in entry for the given string (value) to the diagnostic
specified by diag.
*/
{
  add_string_fill_in(diag, value.start(), value.length());
}  /* Fill_in<a_const_char*>::add */


void Fill_in<a_symbol*>::add(a_diagnostic_ptr diag,
                             a_symbol         *value)
/*
Add a symbol fill-in entry for the given symbol (value) to the diagnostic
specified by diag.
*/
{
  add_symbol_fill_in(diag, value);
}  /* Fill_in<a_symbol*>::add */


void Fill_in<a_type*>::add(a_diagnostic_ptr diag,
                           a_type           *value)
/*
Add a declaration fill-in entry for the given type (value) to the diagnostic
specified by diag.
*/
{
  add_type_fill_in(diag, value);
}  /* Fill_in<a_type*>::add */


void Fill_in<a_reflection_value>::add(a_diagnostic_ptr    diag,
                                      a_reflection_value  rv)
/*
Add a reflection fill-in entry for the given reflection value to the
diagnostic specified by diag.
*/
{
  add_reflection_fill_in(diag, rv);
}  /* Fill_in<a_reflection*>::add */

}  /* namespace detail */

#if !STANDALONE_UTILITY_PROGRAM

static a_source_position *find_definition_position(an_il_entry_kind kind,
                                                   char             *ptr)
/*
Find and return a pointer to the source position of the definition of the IL
entity with the given kind and address.  If no definition position is known,
instead return NULL.
*/
{
  /* If this assertion fails, the caller gave a NULL pointer for an entity kind
     other than iek_none and needs to be corrected. */
  check_assertion(kind == iek_none || ptr != NULL);
  a_source_position *result = NULL;

  switch (kind) {
    case iek_label:
    case iek_routine:
    case iek_type:
    case iek_variable:
      { /* These entities have their decl_position updated to reflect the
           position of the definition. */
        a_source_correspondence *scp = source_corresp_for_il_entry(ptr, kind);

        result = &scp->decl_position;
      }
      break;
    case iek_template:
      { /* The front end records all template declarations, thus the definition
           template entity needs to be used to get the position of the
           defining declaration. */
        a_template *templ = (a_template*)ptr;

        check_assertion(templ->canonical_template != NULL);
        templ = templ->canonical_template;
        if (templ->definition_template != NULL) {
          templ = templ->definition_template;
          result = &templ->source_corresp.decl_position;
        }  /* if */
      }
      break;
    default:
      /* Unknown definition position. */
      break;
  }  /* switch */
  /* Catch any cases where the source position is a copy of the null position,
     and instead return NULL to prevent a confusing diagnostic. */
  if (result != NULL &&
      (cmp_source_positions(*result, null_source_position) == 0)) {
      result = NULL;
  }  /* if */
  return result;
}  /* find_definition_position */


void issue_redef_diag(a_source_position *new_pos,
                      a_symbol          *prev_decl_sym,
    /* Defaulted: */  an_error_severity severity)
/*
Given the (new) position of the redefinition, the previous declaration symbol,
and an error severity, emit a redefinition diagnostic.
*/
{
  an_il_entry_kind  kind;
  char              *il_ptr = il_entry_for_symbol_null_okay(prev_decl_sym,
                                                            &kind);
  a_source_position *prev_def_pos = find_definition_position(kind, il_ptr);

  if (prev_def_pos != NULL) {
    pos_diagnostic(severity, ec_already_defined_with_pos, new_pos,
                   prev_decl_sym, prev_def_pos);
  } else {
    pos_diagnostic(severity, ec_already_defined, new_pos, prev_decl_sym);
  }  /* if */
}  /* issue_redef_diag */


an_error_code incomplete_type_error_code(a_type *type)
/*
Given an incorrect use of the given incomplete type, return the appropriate
error code.  Currently this function only distinguishes between the C++/CLI
managed nullptr type and other incomplete types.
*/
{
  an_error_code result = ec_incomplete_type_not_allowed;

#if MICROSOFT_EXTENSIONS_ALLOWED
  if (is_managed_nullptr_type(type)) {
    result = ec_managed_nullptr_not_allowed;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return result;
}  /* incomplete_type_error_code */


void issue_incomplete_type_diag(an_error_code     error_code,
                                a_source_position *pos,
                                a_type            *type,
                                an_error_severity severity)
/*
Emit a diagnostic given the incomplete type error code to diagnose, the
position where the incomplete type is used, the type that's incomplete, and the
severity of the diagnostic.
*/
{
  switch (error_code) {
    case ec_managed_nullptr_not_allowed:
      pos_diagnostic(severity, ec_managed_nullptr_not_allowed, pos);
      break;
    case ec_incomplete_type_not_allowed:
    case ec_ptr_or_ref_to_incomplete_type:
      pos_diagnostic(severity, error_code, pos, type);
      break;
    default:
      /* If this is reached, the caller has added support for a new error code
         case that needs to be handled here. */
      unexpected_condition();
  }  /* switch */
}  /* issue_incomplete_type_diag */

#if MAKE_FRONT_END_CALLABLE

void error_cleanup(void)
/*
This routine is called at the end of compilation, or if compilation is
terminated prematurely for some reason.  It performs any cleanup operations
required.  In particular, it closes any files that may have been open at
the point at which the compilation was terminated.
*/
{
  close_file_if_open(&f_err_src_file);
  /* Reset f_error so that an internal error during initialization will
     be directed to stderr, not wherever the previous compilation directed
     error output. */
  reset_f_error();
}  /* error_cleanup */


void error_late_cleanup()
/*
Perform final cleanup and shut down error output.
*/
{
  close_f_error_if_open();
}  /* error_late_cleanup */

#endif /* MAKE_FRONT_END_CALLABLE */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

