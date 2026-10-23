/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

pragma.h -- Declarations related to the #pragma directives

*/

/* Avoid including these declarations more than once. */
#ifndef PRAGMA_H
#define PRAGMA_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* Note: a_pending_pragma_ptr is defined in symbol_tbl.h. */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
The pragma binding kinds indicate the ways in which a pragma may relate
to the surrounding constructs.  The binding kinds differ in how they
are handled in cached vs. non-cached contexts.  This table summarizes
the processing of the various pragma binding kinds:

			Processed		Processed
Kind			when encountered	when rescanned
----------		----------------	--------------
immediate		yes			yes
next_token		no			yes
preproc_immediate	yes			no
next_construct		no			yes
other			not applicable		not applicable

"Processed when encountered" means that the pragma is processed even if
it is encountered in a context that is being cached to be scanned later.
"Processed when rescanned" means that the pragma is processed when the
token with which it is associated is rescanned from a cache.
*/
enum a_pragma_binding_kind {
  pbk_none,
		/* Used by some routines to indicate that no binding kind
		   was specified. */
  pbk_next_construct,
		/* Binds to the next top level construct (declaration or
		   statement).  When the pragma appears in a cached context,
		   it is not processed at that point but is processed each time
		   the cache is scanned. */
  pbk_next_token,
		/* Processed when cleared from the curr_token pragma list.
		   When the pragma appears in a cached context, it is not
		   processed at that point but is processed each time
		   the cache is scanned. */
  pbk_immediate,
		/* Processed after the pragma directive is scanned.  When
		   the pragma appears in a cached context, it is processed
		   when it is encountered and again each time the cache is
		   scanned. */
  pbk_other,
		/* Processed by special code added to handle a given pragma. */
  pbk_preproc_immediate,
                /* Processed when encountered as a preprocessing directive.
		   When the pragma appears in a cached context, it is processed
		   when it is encountered and not at all when the cache is
		   scanned. */
  pbk_last
		/* Must be last. */
};


/*
Typedefs used to declare pointers to pragma processing functions.

For pbk_next_construct pragmas, either sym_ptr or stmt_ptr will be
defined.  The one that is not defined will be NULL.  For all other
binding kinds, both sym_ptr and stmt_ptr will be NULL.
*/
typedef void a_next_construct_pragma_function
					(a_pending_pragma_ptr ppp,
					 struct a_symbol      *sym_ptr,
					 a_statement_ptr      stmt_ptr);
typedef a_next_construct_pragma_function *a_next_construct_pragma_function_ptr;

typedef void a_next_token_pragma_function(a_pending_pragma_ptr ppp);
typedef a_next_token_pragma_function *a_next_token_pragma_function_ptr;

typedef void an_immediate_pragma_function(a_pending_pragma_ptr ppp);
typedef an_immediate_pragma_function *an_immediate_pragma_function_ptr;

typedef void an_other_pragma_function(a_pending_pragma_ptr ppp);
typedef an_other_pragma_function *an_other_pragma_function_ptr;

typedef void a_preproc_immediate_pragma_function(a_pending_pragma_ptr ppp);
typedef a_preproc_immediate_pragma_function
                                    *a_preproc_immediate_pragma_function_ptr;

/*
For each pragma that is defined, there exists an a_pragma_kind_description
record that indicates how that pragma is to be handled by the front
end.
*/
typedef struct a_pragma_kind_description *a_pragma_kind_description_ptr;
typedef struct a_pragma_kind_description {
  a_pragma_kind_description_ptr
	        next;
			/* Pointer to the next element in the list of
			   pragma descriptions. */
  a_pragma_kind	kind;
			/* The IL pragma kind code. */
  a_pragma_binding_kind
		binding_kind;
			/* The binding kind indicates when and how the pragma
			   should be scanned by the front end. */
  a_function_number
		processing_function_index;
			/* An index that indicates which function should be
			   called to process this particular pragma.  The
			   resulting function pointer needs to be cast to
			   the appropriate type depending on the kind:
			      pbk_next_construct
			          -> a_next_construct_pragma_function_ptr
			      pbk_next_token
			          -> a_next_token_pragma_function_ptr
			      pbk_immediate
			          -> an_immediate_pragma_function_ptr
			      pbk_other
			          -> an_other_pragma_function_ptr
			      pbk_preproc_immediate
			          -> a_preproc_immediate_pragma_function_ptr */
  a_bit_field	may_bind_to_decl:1;
			/* For pbk_next_construct pragmas, TRUE if this
			   pragma can bind to a declaration. */
  a_bit_field	may_bind_to_stmt:1;
			/* For pbk_next_construct pragmas, TRUE if this
			   pragma can bind to a statement. */
  a_bit_field	global:1;
			/* This flag is used to determine the IL scope to be
			   used when automatically_include_in_il is TRUE
			   and when an IL entry is created for a pragma by
			   explicitly calling create_il_entry_for_pragma.
			   See the description below. */
  a_bit_field	automatically_include_in_il:1;
			/* This flag is TRUE if the front end should
			   automatically generate an IL entry for this
			   pragma kind.  When this flag is TRUE, the front
			   end will create an IL entry before the
			   processing function (if any) is called.
			   For pbk_next_construct pragmas, the pragma is
			   entered in the same IL scope as the entity to
			   which it is bound.  For pbk_immediate,
			   pbk_next_token and pbk_other pragmas the pragma is
			   entered in the file scope (when global is TRUE) or
			   in the current IL scope (when global is FALSE).
			   If this flag is not set, the pragma will
			   not be automatically included in the IL by
			   the front end but can still be made part of
			   the IL by user written code to explicitly
			   link the pragma into the IL. */
  a_bit_field	record_pragma_text:1;
			/* TRUE if the text of this pragma should be saved as
			   a null terminated string.  The string created
			   begins with the identifier following the #pragma
			   keyword.  The character string representation
			   may be used in source-to-source transformation
			   applications to pass pragmas to the generated
			   output, and may also be used for pragmas which are
			   more easily processed through the use of a
			   character string instead of a token cache.  The
			   token representation of the pragma is also saved
			   for use in the front end (except for pragmas
			   scanned in fetch_pp_tokens mode). */
  a_bit_field	expand_macros:1;
                        /* Specifies whether macros should be expanded when
			   recording the pragma. */
  a_bit_field	processing_C_code:1;
			/* Used for pragmas that are being saved as a token
			   cache.  Indicates that the tokens should be
			   interpreted as C/C++ code.  Keywords should be
			   recognized, and adjacent string literals
			   concatenated together. */
  a_bit_field	fetch_pp_tokens:1;
			/* TRUE if the tokens for this pragma should be
			   fetched as pp-tokens.  When this flag is TRUE,
			   processing_C_code must be FALSE.  In this mode
			   the spacing of the pragma invocation (i.e., the
			   presence or absence of white space) is preserved,
			   but white space and comments are standardized to a
			   single space. */
  a_bit_field	ignore_in_back_end:1;
			/* TRUE if this pragma may be ignored if it is
			   not recognized by the back end.  This allows the
			   back end to diagnose any pragmas that are in the
			   IL that it does not recognize, but ignore pragmas
			   that are in the IL but are intended to be processed
			   by other (earlier) phases of the compilation. */
  a_bit_field	is_pseudo_pragma:1;
                        /* TRUE if this pragma kind represents a pseudo
			   pragma (something that is not specified in the
			   source code as a pragma, but that is treated as
			   a pragma).  Lint comment pragmas are examples of
			   pseudo pragmas. */
  a_bit_field	il_info_is_complete:1;
			/* TRUE if a back end (including the C and C++
			   generating back ends) can get the information
			   needed to process or re-emit the pragma solely
			   from the pragma IL entry without the need to
			   refer to the saved copy of the pragma.  This is
			   needed for pragmas saved as token caches, which
			   otherwise are not permitted to be passed in the
			   IL. */
  a_bit_field	allowed_in_pragma_operator:1;
			/* TRUE for pbk_preproc_immediate pragmas that can
			   be used in _Pragma (or Microsoft __pragma)
			   operators.  FALSE if only the #pragma form is
			   allowed, and for other pragma binding kinds. */
  a_bit_field	read_string_as_header_name:1;
			/* TRUE if strings in the pragma text should be
			   read as header names rather than as ordinary
			   character strings, suppressing recognition of what
			   would otherwise appear to be escape sequences. */
  an_error_severity
		error_severity;
			/* For pbk_other pragmas, the severity of the
			   diagnostic to be issued if the pragma is
			   never scanned.  For pbk_next_construct
			   pragmas, the severity of diagnostic
			   to be issued if the pragma is encountered
			   in an improper location.
			   May be es_none if no diagnostic is to be
                           issued. */
} a_pragma_kind_description;


/*
Pending pragma entries describe pragmas that have been encountered
in the source and recorded in token caches, but have not yet been
processed by the front-end proper.
*/
/* a_pending_pragma_ptr declared earlier. */
typedef struct a_pending_pragma {
  a_pending_pragma(a_pragma_kind_description_ptr pkdp);
  INLINE a_pending_pragma(const a_pending_pragma &other) = default;

  a_pragma_kind_description_ptr
		descr_ptr;
			/* Pointer to the structure that describes the
			   particular kind of pragma being processed. */
  a_shared_token_cache
		token_cache;
			/* The tokens that comprise the body of the pragma.
			   The first token in the cache is the token
			   following the identifier(s) used to determine the
			   pragma kind.  The cache is terminated by a
			   tok_newline followed by a tok_end_of_source. */
  a_source_position
		id_position;
			/* Source position of the identifier that indicates
			   the kind of pragma being processed.  For a
			   C99 _Pragma, it points to the pragma string
			   literal. */
  a_source_position
		pragma_position;
			/* Source position of the start of the #pragma
			   directive. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		source_sequence_entry;
			/* Pointer to source sequence entry that represents
			   the place this pragma appears within the current
			   file or function scope relative to other
			   declarations, statements, comments, etc. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_bit_field	is_microsoft_pragma_operator:1;
			/* TRUE if the pragma was specified using a Microsoft
			   __pragma operator. */
  a_bit_field	is_function_style_pragma:1;
			/* TRUE if the pragma was specified as a function-style
			   pragma (i.e., _Pragma or __pragma, but not #pragma).
			   */
  a_bit_field	has_been_processed:1;
			/* TRUE if this pragma has already been processed.
			   This is used for immediate pragmas that are
			   processed but must be kept on the current token
			   pragmas list in case the token is cached. */
  char		*pragma_text;
			/* For pragmas that are passed through to the
			   back end as an uninterpreted character string,
			   this points to the null terminated string.  The
			   string begins with the token immediately following
			   the #pragma keyword.  The string is allocated in
			   the file scope IL memory region, so this pointer
			   may be copied directly to the IL entry created
			   for this pragma (if any).  The string does not
			   need to be moved.  For pbk_preproc_immediate
			   pragmas, the pragma_text is only present if the
			   automatically_include_in_il flag is TRUE. */
  a_pragma_ptr	il_pragma_entry;
			/* A pointer to the IL pragma entry associated with
			   this pending pragma, if any. */

  /* Pragma-specific information.  This union contains other information
     about the pragma and may be used to preserve information about the
     pragma between the time the tokens are scanned and some later time
     in which the information may be used.  Note that the pragma entry
     for a pragma defined within the body of a template is copied each
     time the template is instantiated, so it is not possible to pass
     information between different instantiations of the template by
     using this union. */
  union {
    /* When descr_ptr->kind == pk_lint_varargs_count */
    a_lint_varargs_count
		lint_varargs_count;
#if GNU_EXTENSIONS_ALLOWED
    /* When descr_ptr->kind == pk_gcc */
    a_gcc_pragma_descr
		gcc;
#endif /* GNU_EXTENSIONS_ALLOWED */
  } variant;
} a_pending_pragma;


EXTERN_THREAD a_pragma_kind_description_ptr
                pragma_kind_descriptions;
			/* Pointer to a linked list of pragma descriptions. */

EXTERN_THREAD a_pending_pragma_list
		*curr_token_pragmas;
			/* A list of pending pragma entries for any
			   pragmas the immediately preceded the current
			   token. */

EXTERN_THREAD a_pragma_kind_description_ptr
		pragma_description_for_pragma_kind[(int)pk_last + 1];
			/* An array that can be used to get a pointer to
			   a pragma description given a pragma kind.  Note that
			   the entry in the array will only contain a value
			   if the pragma has been added to the list of
			   active pragma descriptions through an
			   add_pragma_description call. */

#if DEBUG
/* GCOVR_EXCL_START */

extern void db_pragma_list(a_pragma_ptr pp);

extern void db_scope_pragmas(a_scope_ptr scope);

extern void db_opt_pragma(a_pending_pragma_ptr	ppp);

extern void db_name_pragma(a_pending_pragma_ptr	ppp);

/*
Counts of tables allocated, to track total use of memory.  These are
initialized and the results reported in lexical.c.
*/
EXTERN_THREAD unsigned long
		num_pragmas_allocated,
		num_pragma_descriptions_allocated;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

#if INCLUDE_EDG_TEST_PRAGMAS
extern void test_immediate_pragma(a_pending_pragma_ptr ppp);
extern void test_next_construct_pragma(a_pending_pragma_ptr  ppp,
				       a_symbol_ptr          sym_ptr,
				       a_statement_ptr	     stmt_ptr);
#endif /* INCLUDE_EDG_TEST_PRAGMAS */

namespace detail {

extern void copy_construct_pragma_list(a_pending_pragma_list       *dest,
                                       const a_pending_pragma_list &old_list);
extern void destroy_pending_pragma_list(a_pending_pragma_list *pplp);

}  /* namespace detail */

extern void copy_fresh_pragmas_into(a_pending_pragma_list       *dest,
                                    const a_pending_pragma_list &old_list);

extern void add_to_curr_token_pragma_list(const a_shared_pending_pragma &spp);

extern void add_to_curr_token_pragma_list(const a_pending_pragma_list &list);

extern a_boolean select_curr_construct_pragmas(a_boolean  add_to_list);

extern void add_pragma_to_il(a_pending_pragma_ptr  ppp,
                             an_il_entry_kind      entity_kind,
                             char                  *entity_ptr,
                             a_boolean             is_global);

extern
a_shared_pending_pragma add_curr_token_pseudo_pragma(a_pragma_kind      kind,
                                                     a_source_position *pos);

extern void create_il_entry_for_pragma(a_pending_pragma_ptr ppp,
                                       a_symbol_ptr         sym,
                                       a_statement_ptr      sp);

extern void process_immediate_pragmas(void);

extern void process_curr_token_pragmas(void);

extern void end_of_scope_pragma_processing(const a_pending_pragma_list &ppl);

extern void cannot_bind_to_curr_construct(void);

extern void discard_curr_construct_pragmas(void);

extern a_pending_pragma_list* extract_curr_construct_pragmas();

extern
void reactivate_curr_construct_pragmas(a_pending_pragma_list *pplp);

extern a_pending_pragma_list extract_specific_pragmas(
                                              a_pragma_kind   kind,
                                              a_symbol_ptr    sym,
                                              a_statement_ptr sp,
                                              a_boolean       curr_scope_only);

extern void process_curr_construct_pragmas(a_symbol_ptr     sym,
                                           a_statement_ptr  sp);

extern void process_pragmas_at_end_of_source(void);

extern void pragma_one_time_init(void);

extern void pragma_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef PRAGMA_H */

