/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

preproc.h -- Declarations related to preproc.c (having to do with
             preprocessing directives).

*/

/* Avoid including these declarations more than once: */
#ifndef PREPROC_H
#define PREPROC_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

#ifndef PRAGMA_H
#include "pragma.h"
#endif /* ifndef PRAGMA_H */

#ifndef LEXICAL_H
#include "lexical.h"
#endif /* ifndef LEXICAL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
If this list is updated, be sure to change pp_directive_kind_names below.
Note that the "import", "export", and "module" preprocessor directives are
handled outside of this framework (see is_module_pp_directive).
*/
enum a_pp_directive_kind {
  /* Enumeration of preprocessing directives. */
  ppd_if, ppd_ifdef, ppd_ifndef, ppd_elif, ppd_else, ppd_elifdef,
  ppd_elifndef, ppd_endif, ppd_include, ppd_define, ppd_undef,
  ppd_line, ppd_error, ppd_pragma, ppd_null, ppd_linedef,
#if IDENT_DIRECTIVE_AND_PRAGMA
  ppd_ident,
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if ALIAS_DIRECTIVE
  ppd_alias,
#endif /* ALIAS_DIRECTIVE */
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
  ppd_assert, ppd_unassert,
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
  ppd_import,         /* #import Microsoft extension */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ppd_using,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ppd_include_next,
  ppd_warning,
  ppd_embed,
  ppd_not_valid
};

#if DEBUG
/*
Table of names of preprocessing directives, used as event kinds for PCH
processing.  This is not the definition of the preprocessing directive
keywords (see identify_dir_keyword).
*/
EXTERN_CONSTINIT_ARRAY(a_const_char*, pp_directive_kind_names,
                       ppd_not_valid + 1)
#if VAR_INITIALIZERS
= { "if",
    "ifdef",
    "ifndef",
    "elif",
    "else",
    "elifdef",
    "elifndef",
    "endif",
    "include",
    "define",
    "undef",
    "line",
    "error",
    "pragma",
    "null",
    "linedef",
#if IDENT_DIRECTIVE_AND_PRAGMA
    "ident",
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if ALIAS_DIRECTIVE
    "alias",
#endif /* ALIAS_DIRECTIVE */
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
    "assert",
    "unassert",
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
    "import",
#if MICROSOFT_EXTENSIONS_ALLOWED
    "using",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    "include_next",
    "warning",
    "embed",
    "not_valid"
  }
#endif /* VAR_INITIALIZERS */
EXTERN_CONSTINIT_ARRAY_END(pp_directive_kind_names)
#endif /* DEBUG */


/*
Preprocessor state variables.  These all have valid values at all times
(not just when other variables would indicate that it is sensible for them
to have values), except as explicitly noted.
*/
EXTERN_THREAD a_boolean
		fetch_pp_tokens;
			/* TRUE if preprocessing tokens (pp-tokens) should be
			   fetched instead of normal tokens.  Also implies
			   that constants should not be converted (and
			   numeric ones should be returned as pp-number),
			   adjacent strings should not be concatenated,
			   keywords should not be recognized, and errors
			   should not be issued for malformed tokens. */
EXTERN_THREAD a_boolean
		expand_macros;
			/* TRUE if preprocessor macros should be expanded. */
EXTERN_THREAD a_boolean
		in_preprocessing_directive;
			/* TRUE if we are currently somewhere between the
			   opening "#" and the closing newline of a
			   preprocessing directive.  This controls the
			   interpretation of white space, makes newline
			   a token, disables recognition of keywords,
			   and enables "#" and "##" as tokens. */

EXTERN_THREAD a_boolean
		suppress_keyword_recognition;
			/* TRUE if keywords should not be recognized even
			   when not in a preprocessing directive.  This is
			   used to suppress keyword recognition in Microsoft
			   attribute processing. */

EXTERN_THREAD a_boolean
		caching_pragma_tokens;
			/* TRUE is we are in a pragma that is being recorded
			   as a token cache.  This disables some of
			   the special processing that is normally done when
		           is_preprocessing_directive is TRUE; specifically,
			   identifiers are looked up and the tokens
			   "#" and "##" are disabled.  Keywords are only
			   recognized if processing_C_code_in_pragma is
			   TRUE. */

#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN_THREAD a_boolean
		in_microsoft_attribute;
			/* TRUE if we are scanning the tokens of a Microsoft
			   attribute. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

EXTERN_THREAD a_boolean
                recognize_keywords_in_pragma;
                        /* TRUE if we are saving the tokens of a pragma in
			   a token cache and keywords should be recognized. */

EXTERN_THREAD a_boolean
		do_string_literal_concatenation;
			/* TRUE if adjacent string literal tokens should be
			   concatenated.  Considered only if fetch_pp_tokens
			   is FALSE. */
EXTERN_THREAD a_boolean
		in_pp_if_expression;
			/* TRUE if we are currently inside the expression of
			   a #if.  When TRUE, integer constants get an
			   implied "L" suffix, undefined identifiers
			   are taken to be 0L, and the "defined" operator
			   is enabled. */
EXTERN_THREAD a_boolean
		exp_header_name;
			/* TRUE means that a header name (name on a #include)
			   of the form "..." is expected next, and tells
			   get_token to scan accordingly.  exp_header_name
			   is valid even when in_preprocessing_directive is
			   FALSE (it's always FALSE in that case).  */
EXTERN_THREAD a_boolean
		exp_system_header_name;
			/* TRUE means that a header name (name on a #include)
			   of the form <...> is expected next, and tells
			   get_token to scan accordingly.
			   exp_system_header_name is valid even when
			   in_preprocessing_directive is FALSE (it's always
			   FALSE in that case).  */
EXTERN_THREAD a_boolean
		exp_digit_sequence;
			/* TRUE means that a digit-sequence
			   is expected next, and tells get_token to scan
			   accordingly.  exp_digit_sequence is valid even
			   when in_preprocessing_directive is FALSE (it's
			   always FALSE in that case).  */
EXTERN_THREAD a_boolean
		do_not_put_curr_line_in_pp_output;
			/* When TRUE, the current line should not be
			   put out as preprocessing output (probably because
			   it's a directive, or skipped over by a #if).
			   Meaningful when generate_pp_output is TRUE.
			   Note that when this flag is TRUE, the line is
			   entirely deleted, rather than put out as a blank
			   line.  Also TRUE when there is no current line,
			   as at the start and end of the source. */
EXTERN_THREAD a_boolean
		pass_pp_directive_to_output;
			/* When TRUE, the current preprocessing directive
			   should be passed unchanged to preprocessing output,
			   so that some later processor (like a compiler)
			   can handle it. */
EXTERN_THREAD a_seq_number
		next_seq_in_pp_output;
			/* Indicates the sequence number associated with
			   the next line to be put out in preprocessing output.
			   This is used to control the output of
			   line-identifying directives.  Meaningful when
			   generate_pp_output is TRUE. */
EXTERN_THREAD a_boolean
		prev_pp_output_line_was_complete;
			/* TRUE if the previous line of preprocessing
			   output ended with a newline, i.e., it was not a
			   partial line caused by macro expansion or a
			   multi-line preprocessing directive being passed
			   unchanged to output. */
EXTERN_THREAD a_boolean
		currently_in_pp_if_skip;
			/* If TRUE, we are currently skipping lines
			   because of an #if or the like. */
EXTERN_THREAD a_boolean
		some_error_in_curr_directive;
			/* TRUE if some error has been detected in the
			   current preprocessing directive.  This means
			   specifically some syntax error that might
			   prevent complete successful scanning of the
			   directive, not something like the directive
			   appearing out of sequence. */
EXTERN_THREAD long
		pp_if_stack_depth;
			/* Stack of currently active #if, #ifdef, and
			   #ifndef directives.  pp_if_stack_depth
			   is the index of the currently active entry.
			   pp_if_stack_depth == -1 for an empty stack. */
EXTERN_THREAD long
		base_pp_if_stack_depth;
			/* The value of pp_if_stack_depth at entry to
			   the current file; important because in ANSI C,
			   each #if must be closed within the file in
			   which it was opened.  In pcc mode, always -1. */

EXTERN_THREAD sizeof_t
		size_pp_dir_string_buffer;
			/* Current allocated size of
                           pp_dir_string_buffer.  Not per-file.
                           See preproc.c for the definition of
			   pp_dir_string_buffer. */

EXTERN_THREAD a_boolean
		actual_include_was_suppressed;
			/* TRUE if an #include operation did not result in
			   the actual inclusion of a file.  This could occur
			   if the include was simulated (as is sometimes done
			   for stdarg.h, etc.), or because the include was
			   suppressed because the file had already been
			   included. */

#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN_THREAD a_boolean
		processing_vccorlib_header;
			/* TRUE if currently processing vccorlib.h as part of
			   C++/CX initialization. */

EXTERN_THREAD an_assembly_index
		curr_assembly_index;
			/* When scanning imported metadata this is the index
			   for the assembly being processed; zero otherwise. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if GNU_EXTENSIONS_ALLOWED
typedef struct a_gcc_pragma_options_entry *a_gcc_pragma_options_entry_ptr;
EXTERN_THREAD a_gcc_pragma_options_entry_ptr
		gcc_pragma_options_stack;
			/* Pointer to the top of the GCC pragma options
			   stack.  If NULL, there's no stack entry (which
			   implies that the "current" stack entry has default
			   values). */
#endif /* GNU_EXTENSIONS_ALLOWED */

extern a_const_char *copy_header_name(a_boolean process_escapes);

/* Scan a preprocessing directive. */
extern void pp_directive(void);
/* Verify that all #ifs are closed at end of source. */
extern void verify_that_all_pp_ifs_were_closed(void);
/* Driver for mode where compiler just does preprocessing, like cpp. */
extern void cpp_driver(void);

extern void create_preinclude_pch_event(void);

extern void pch_prefix_processing_for_preinclude(void);

extern void process_macro_preincludes(void);

#if IDENT_DIRECTIVE_AND_PRAGMA
extern void ident_pragma(a_pending_pragma_ptr ppp);

extern void ident_directive(a_pending_pragma_ptr ppp);
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */

extern a_pragma_kind_description_ptr look_up_pragma_id(
					a_source_position	*id_position);

extern
void record_pragma(a_pragma_kind_description_ptr pkdp,
		   a_source_position		 *start_of_dir_position,
		   a_source_position		 *id_position,
		   a_boolean			 is_microsoft_pragma_operator,
		   a_boolean			 is_function_style_pragma);

extern void stdc_pragma(a_pending_pragma_ptr	ppp);

extern void check_for_stdc_pragmas(void);

#if GNU_EXTENSIONS_ALLOWED
extern void gcc_pragma(a_pending_pragma_ptr  ppp);

#if GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED
extern void gnu_riscv_pragma(a_pending_pragma_ptr  ppp);
extern void clang_riscv_pragma(a_pending_pragma_ptr  ppp);
#endif /* GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED */

extern void attach_target_pragma_attribute(an_attribute_ptr *list);
#endif /* GNU_EXTENSIONS_ALLOWED */

#if UPC_EXTENSIONS_ALLOWED
extern void check_for_upc_pragmas(a_statement_ptr  sp);

extern void upc_pragma(a_pending_pragma_ptr  ppp);
#endif /* UPC_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void microsoft_start_map_region_pragma(a_pending_pragma_ptr  ppp);

extern void microsoft_stop_map_region_pragma(a_pending_pragma_ptr  ppp);

extern void microsoft_comment_pragma(a_pending_pragma_ptr  ppp);

extern void microsoft_conform_pragma(a_pending_pragma_ptr  ppp);

extern void microsoft_include_alias_pragma(a_pending_pragma_ptr ppp);

extern void process_preusings(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void once_pragma(a_pending_pragma_ptr ppp);

extern void hdrstop_or_no_pch_pragma(a_pending_pragma_ptr ppp);

extern a_boolean get_header_name(void);

extern a_const_char *check_for_include_alias(void);

extern a_const_char *extract_header_name(a_boolean process_escapes,
                                         sizeof_t  *result_length);

extern a_boolean parse_embed(a_boolean is_directive);

extern void preproc_one_time_init(void);

extern void preproc_trans_unit_init(void);

extern void preproc_init(void);

#if DEBUG
/* Show and return the amount of memory used by preprocessing structures. */
extern unsigned long show_preproc_space_used(void);
#endif /* DEBUG*/

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_hash_value hash_include_alias(a_void_ptr	key);

extern a_boolean compare_include_alias(a_void_ptr	entry,
                                       a_void_ptr	key);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef PREPROC_H */

