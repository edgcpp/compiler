/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

pragma.c -- Routines to support #pragma directives

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "layout.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Macro used to get a pointer to the active pointer to the current construct
pragma list.  The list pointer is stored in the scope stack entry.
This macro returns a pointer to the head of the list.  By updating this
pointer the caller may alter the list pointed to by the current pointer.
*/
#define curr_list_of_curr_construct_pragmas()		         	      \
  (&scope_stack[depth_scope_stack].curr_construct_pragmas)

#if DEBUG

void db_pragma_list(a_pragma_ptr pp)
/*
Display a list of pragmas for debugging purposes.
*/
{
  a_source_correspondence *scp;  

  for (; pp != NULL; pp = pp->next) {
    fprintf(f_debug, "  Entity kind: %s, ",
                     il_entry_kind_names[(int)pp->entity.kind]);
    fprintf(f_debug, "entity ptr: %p", (a_void_ptr)pp->entity.ptr);
    if (pp->entity.ptr != NULL) {
      scp = source_corresp_for_il_entry(pp->entity.ptr,
                                        (an_il_entry_kind)pp->entity.kind);
      if (scp != NULL) {
        fprintf(f_debug, " (");
        db_name(scp);
        fprintf(f_debug, ")");
      }  /* if */
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* for */
}  /* db_pragma_list */


void db_scope_pragmas(a_scope_ptr scope)
/*
Display the pragma list for a scope for debugging purposes.
*/
{
  fprintf(f_debug, "Pragma list for ");
  db_scope(scope);
  fprintf(f_debug, ":\n");
  db_pragma_list(scope->pragmas);
}  /* db_scope_pragmas */

#endif /* DEBUG */

static a_pragma_kind_description_ptr add_pragma_kind_description
                      (a_pragma_kind 	     kind,
		       a_pragma_binding_kind binding_kind,
		       a_function_number     processing_function_index,
		       a_boolean	     is_pseudo_pragma,
		       a_boolean	     may_bind_to_decl,
		       a_boolean	     may_bind_to_stmt,
		       a_boolean	     global,
		       a_boolean	     automatically_include_in_il,
		       a_boolean	     record_pragma_text,
		       a_boolean	     p_expand_macros,
		       a_boolean	     processing_C_code,
		       a_boolean	     p_fetch_pp_tokens,
		       a_boolean	     ignore_in_back_end,
		       a_boolean	     il_info_is_complete,
		       a_boolean	     allowed_in_pragma_operator,
		       a_boolean	     read_string_as_header_name,
		       an_error_severity     error_severity)
/*
Allocate a pragma description entry, initialize its fields, and add it
to a linked list of pragma descriptions.  is_pseudo_pragma is used for
things like lint comments that are treated like pragmas by the front end
but cannot be referenced by name in a pragma directive.
processing_function_index is a function pointer index that indicates
which function to call to process this pragma.
*/
{
  a_pragma_kind_description_ptr	pkdp;

  /* Make sure this pragma kind is not already on the list. */
  check_assertion_str(pragma_description_for_pragma_kind[kind] == NULL,
                      "add_pragma_kind_description: duplicate pragma kind");
  /* A pbk_next_construct pragma must bind to a declaration and/or
     statement. */
  check_assertion_str2(binding_kind != pbk_next_construct ||
                       (may_bind_to_decl || may_bind_to_stmt),
                       "add_pragma_kind_description:",
		       "bad next_construct binding");
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  /* The back end must be capable of handling any pragmas that are included
     in the IL, and which it is expected to process (i.e., not ignore).
     The C and C++ generating back ends can only handle pragmas that
     are represented as character strings, or those for which the IL
     contains all the information needed for the back end to re-emit the
     pragma (il_info_is_complete), not those represented as token
     caches.  Note: If the C or C++ generating back end is modified to
     do special processing for other kinds of pragmas, this checking code
     will need to be modified accordingly. */
  check_assertion_str2(!automatically_include_in_il ||
                       (record_pragma_text || ignore_in_back_end ||
                        il_info_is_complete),
                       "add_pragma_kind_description:",
		       "pragma flags not valid when using C/C++ gen. BE");
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  /* When fetching pp-tokens, processing_C_code must be FALSE. */
  check_assertion_str2(!p_fetch_pp_tokens || !processing_C_code,
                       "add_pragma_kind_description:",
		       "flags not valid when fetching pp-tokens");
  /* Preprocessing immediate pragmas must have the fetch_pp_tokens flag set. */
  check_assertion_str2(binding_kind != pbk_preproc_immediate ||
                       p_fetch_pp_tokens, "add_pragma_kind_description:",
                       "preproc_immediate pragmas must use fetch_pp_tokens");
  /* Allocate a new entry. */
  pkdp = (a_pragma_kind_description_ptr)
				alloc_fe(sizeof(a_pragma_kind_description));
#if DEBUG
  num_pragma_descriptions_allocated++;
#endif /* DEBUG */
  pkdp->kind = kind;
  pkdp->binding_kind = binding_kind;
  pkdp->processing_function_index = processing_function_index;
  pkdp->may_bind_to_decl = may_bind_to_decl;
  pkdp->may_bind_to_stmt = may_bind_to_stmt;
  pkdp->global = global;
  pkdp->automatically_include_in_il = automatically_include_in_il;
  pkdp->record_pragma_text = record_pragma_text;
  pkdp->expand_macros = p_expand_macros;
  pkdp->processing_C_code = processing_C_code;
  pkdp->fetch_pp_tokens = p_fetch_pp_tokens;
  pkdp->ignore_in_back_end = ignore_in_back_end;
  pkdp->is_pseudo_pragma = is_pseudo_pragma;
  pkdp->il_info_is_complete = il_info_is_complete;
  pkdp->allowed_in_pragma_operator = allowed_in_pragma_operator;
  pkdp->read_string_as_header_name = read_string_as_header_name;
  pkdp->error_severity = error_severity;
  if (is_pseudo_pragma) {
    /* This is a pseudo-pragma (such as a lint comment) that cannot
       be referenced by name.  Don't add it to the linked list. */
    pkdp->next = NULL;
  } else {
    pkdp->next = pragma_kind_descriptions;
    pragma_kind_descriptions = pkdp;
  }  /* if */
  /* Save the pragma description in an array indexed by pragma kind. */
  pragma_description_for_pragma_kind[(int)kind] = pkdp;
  return pkdp;
}  /* add_pragma_kind_description */


static a_pragma_kind_description_ptr add_next_construct_pragma_kind_description
                      (a_pragma_kind 	     kind,
		       a_function_number     processing_function_index,
		       a_boolean	     is_pseudo_pragma,
		       a_boolean	     may_bind_to_decl,
		       a_boolean	     may_bind_to_stmt,
		       a_boolean	     automatically_include_in_il,
		       a_boolean	     record_pragma_text,
		       a_boolean	     p_expand_macros,
		       a_boolean	     processing_C_code,
		       a_boolean	     p_fetch_pp_tokens,
		       a_boolean	     ignore_in_back_end,
		       a_boolean	     il_info_is_complete,
		       a_boolean	     read_string_as_header_name,
		       an_error_severity     error_severity)
/*
This is an interface to the general add_pragma_kind_description that is
used for creating pbk_next_construct pragmas.
*/
{
  /* A pbk_next_construct pragma must bind to a declaration and/or
     statement. */
  check_assertion_str2(may_bind_to_decl || may_bind_to_stmt,
                       "add_next_construct_pragma_kind_description:",
                       "bad next_construct binding");
  return add_pragma_kind_description
           (kind, pbk_next_construct, processing_function_index,
            is_pseudo_pragma, may_bind_to_decl, may_bind_to_stmt,
	    /*global=*/FALSE, automatically_include_in_il,
            record_pragma_text, p_expand_macros, processing_C_code,
            p_fetch_pp_tokens, ignore_in_back_end, il_info_is_complete,
            /*allowed_in_pragma_operator=*/TRUE,
            read_string_as_header_name, error_severity);
}  /* add_next_construct_pragma_kind_description */


static a_pragma_kind_description_ptr add_next_token_pragma_kind_description
                      (a_pragma_kind 	     kind,
		       a_function_number     processing_function_index,
		       a_boolean	     is_pseudo_pragma,
		       a_boolean	     global,
		       a_boolean	     automatically_include_in_il,
		       a_boolean	     record_pragma_text,
		       a_boolean	     p_expand_macros,
		       a_boolean	     processing_C_code,
		       a_boolean	     p_fetch_pp_tokens,
		       a_boolean	     ignore_in_back_end,
		       a_boolean	     il_info_is_complete,
		       a_boolean	     read_string_as_header_name,
		       an_error_severity     error_severity)
/*
This is an interface to the general add_pragma_kind_description that is
used for creating pbk_next_token pragmas.
*/
{
  return add_pragma_kind_description
           (kind, pbk_next_token, processing_function_index,
	    is_pseudo_pragma, /*may_bind_to_decl=*/FALSE,
            /*may_bind_to_expr=*/FALSE, global, automatically_include_in_il,
            record_pragma_text, p_expand_macros, processing_C_code,
            p_fetch_pp_tokens, ignore_in_back_end, il_info_is_complete,
            /*allowed_in_pragma_operator=*/TRUE,
            read_string_as_header_name, error_severity);
}  /* add_next_token_pragma_kind_description */


static a_pragma_kind_description_ptr add_immediate_pragma_kind_description
                      (a_pragma_kind 	     kind,
		       a_function_number     processing_function_index,
		       a_boolean	     global,
		       a_boolean	     automatically_include_in_il,
		       a_boolean	     record_pragma_text,
		       a_boolean	     p_expand_macros,
		       a_boolean	     processing_C_code,
		       a_boolean	     p_fetch_pp_tokens,
		       a_boolean	     ignore_in_back_end,
		       a_boolean	     il_info_is_complete,
		       a_boolean	     read_string_as_header_name,
		       an_error_severity     error_severity)
/*
This is an interface to the general add_pragma_kind_description that is
used for creating pbk_immediate pragmas.
*/
{
  return add_pragma_kind_description
           (kind, pbk_immediate, processing_function_index,
	    /*is_pseudo_pragma=*/FALSE, /*may_bind_to_decl=*/FALSE,
            /*may_bind_to_expr=*/FALSE, global, automatically_include_in_il,
            record_pragma_text, p_expand_macros, processing_C_code,
            p_fetch_pp_tokens, ignore_in_back_end, il_info_is_complete,
            /*allowed_in_pragma_operator=*/TRUE,
            read_string_as_header_name, error_severity);
}  /* add_immediate_pragma_kind_description */

#if INCLUDE_EDG_TEST_PRAGMAS

static a_pragma_kind_description_ptr add_other_pragma_kind_description
                      (a_pragma_kind 	     kind,
		       a_function_number     processing_function_index,
		       a_boolean	     is_pseudo_pragma,
		       a_boolean	     global,
		       a_boolean	     automatically_include_in_il,
		       a_boolean	     record_pragma_text,
		       a_boolean	     p_expand_macros,
		       a_boolean	     processing_C_code,
		       a_boolean	     p_fetch_pp_tokens,
		       a_boolean	     ignore_in_back_end,
		       a_boolean	     il_info_is_complete,
		       a_boolean	     read_string_as_header_name,
		       an_error_severity     error_severity)
/*
This is an interface to the general add_pragma_kind_description that is
used for creating pbk_other pragmas.
*/
{
  return add_pragma_kind_description
           (kind, pbk_other, processing_function_index,
	    is_pseudo_pragma, /*may_bind_to_decl=*/FALSE,
            /*may_bind_to_expr=*/FALSE, global, automatically_include_in_il,
            record_pragma_text, p_expand_macros, processing_C_code,
            p_fetch_pp_tokens, ignore_in_back_end, il_info_is_complete,
            /*allowed_in_pragma_operator=*/TRUE,
            read_string_as_header_name, error_severity);
}  /* add_other_pragma_kind_description */

#endif /* INCLUDE_EDG_TEST_PRAGMAS */

static
a_pragma_kind_description_ptr add_preproc_immediate_pragma_kind_description(
                       a_pragma_kind 	          kind,
                       a_function_number   	  processing_function_index,
		       a_boolean		  record_pragma_text,
		       a_boolean		  il_info_is_complete,
                       a_boolean                  automatically_include_in_il,
		       a_boolean		  ignore_in_back_end,
		       a_boolean		  allowed_in_pragma_operator,
		       a_boolean		  read_string_as_header_name)
/*
This is an interface to the general add_pragma_kind_description that is
used for creating pbk_preproc_immediate pragmas.
*/
{
  return add_pragma_kind_description
           (kind, pbk_preproc_immediate, processing_function_index,
            /*is_pseudo_pragma=*/FALSE, /*may_bind_to_decl=*/FALSE,
            /*may_bind_to_expr=*/FALSE, /*global=*/FALSE,
            automatically_include_in_il, record_pragma_text,
            /*expand_macros=*/FALSE, /*processing_C_code=*/FALSE,
            /*fetch_pp_tokens=*/TRUE, ignore_in_back_end,
            il_info_is_complete, allowed_in_pragma_operator,
            read_string_as_header_name,
            /*error_severity=*/es_none);
}  /* add_preproc_immediate_pragma_kind_description */


a_pending_pragma::a_pending_pragma(a_pragma_kind_description_ptr pkdp)
/*
Construct a pending pragma with the given pragma kind description.
*/
  : descr_ptr(pkdp), id_position(null_source_position),
    pragma_position(null_source_position),
#if GENERATE_SOURCE_SEQUENCE_LISTS
    source_sequence_entry(NULL),
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    is_microsoft_pragma_operator(FALSE), is_function_style_pragma(FALSE),
    has_been_processed(FALSE), pragma_text(NULL), il_pragma_entry(NULL)
{
  /* Initialize any pragma-specific information. */
  switch (pkdp->kind) {
    case pk_lint_varargs_count:
      this->variant.lint_varargs_count = 0;
      break;
#if GNU_EXTENSIONS_ALLOWED
    case pk_gcc_immediate:
    case pk_gcc_next_token:
    case pk_gcc_next_statement:
      clear_gcc_pragma_descr(&this->variant.gcc);
      break;
#if GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED
    case pk_gnu_riscv:
    case pk_clang_riscv:
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if INCLUDE_EDG_TEST_PRAGMAS
    case pk_test_next_statement:
    case pk_test_next_decl:
    case pk_test_immediate:
    case pk_test_immediate_text:
    case pk_test_immediate_pp_text:
    case pk_test_other:
    case pk_test_bind_next_pass:
      break;
#endif /* INCLUDE_EDG_TEST_PRAGMAS */
#if ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING
    case pk_checking_pragma:
      break;
#endif /* ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING */
#if DEBUG
    case pk_db_opt:
    case pk_db_name:
      break;
#endif /* DEBUG */
#if NEED_IL_DISPLAY
    case pk_il_display:
      break;
#endif /* NEED_IL_DISPLAY */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
    case pk_if_exists:
      break;
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
#if INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
    case pk_unrecognized:
      /* No special initialization is required. */
      break;
#endif /* INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL */
    /* Pragma kinds that have no information in the variant section. */
    case pk_printf_args:
    case pk_scanf_args:
    case pk_lint_argsused:
    case pk_lint_notreached:
    case pk_instantiate:
    case pk_do_not_instantiate:
    case pk_can_instantiate:
    case pk_inline_template:
    case pk_diag_suppress:
    case pk_diag_remark:
    case pk_diag_warning:
    case pk_diag_error:
    case pk_diag_once:
    case pk_diag_default:
    case pk_diagnostic:
    case pk_pack:
#if IDENT_DIRECTIVE_AND_PRAGMA
    case pk_ident_pragma:
    case pk_ident_directive:
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if PRAGMA_WEAK_ALLOWED
    case pk_weak:
#endif /* PRAGMA_WEAK_ALLOWED */
    case pk_define_type_info:
    case pk_stdc:
#if UPC_EXTENSIONS_ALLOWED
    case pk_upc:
#endif /* UPC_EXTENSIONS_ALLOWED */
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
    case pk_redefine_extname:
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if SUN_EXTENSIONS_ALLOWED
    case pk_enable_ldscope:
    case pk_disable_ldscope:
#endif /* SUN_EXTENSIONS_ALLOWED */
    case pk_once:
    case pk_hdrstop:
    case pk_no_pch:
    case pk_push_macro:
    case pk_pop_macro:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case pk_start_map_region:
    case pk_stop_map_region:
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
    case pk_setlocale:
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
    case pk_comment:
    case pk_conform:
    case pk_include_alias:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      break;
    default:
      unexpected_condition_str2("alloc_pending_pragma:", "bad pragma kind");
      break;
  }  /* switch */
}  /* a_pending_pragma::a_pending_pragma */

namespace detail {

void copy_construct_pragma_list(a_pending_pragma_list       *dest,
                                const a_pending_pragma_list &old_list)
/*
Make an exact copy of a list of pending pragma entries constructed at the given
destination address.

This function exists to work around a_pending_pragma being incomplete in
lexical.h.  Modern compilers allow that problem to be resolved by specializing
the copy constructor of Shared_obj<a_pending_pragma>; however, legacy compilers
have bugs that prevent that solution from working.
*/
{
  new (dest) a_pending_pragma_list(old_list);
}  /* copy_construct_pragma_list */


void destroy_pending_pragma_list(a_pending_pragma_list *pplp)
/*
Destroy the list of pending pragmas at the given pointer.

This function exists to work around a_pending_pragma being incomplete in
lexical.h.  Modern compilers allow that problem to be resolved by specializing
the destructor of Shared_obj<a_pending_pragma>; however, legacy compilers have
bugs that prevent that solution from working.
*/
{
  (*pplp).~Dyn_array<a_shared_pending_pragma>();
}  /* destroy_pending_pragma_list */

}  /* namespace detail */

void copy_fresh_pragmas_into(a_pending_pragma_list       *dest,
                             const a_pending_pragma_list &old_list)
/*
Copy the given old list of pending pragma entries into the given destination
list.  While pragma entries are copied the has_been_processed_flag is reset
(for immediate pragmas).

This routine is used, for example, when rescanning tokens from a reusable
cache.  When a token with associated pragma entries is rescanned, the pragma
entries must be copied because the original entries will remain attached to the
token in the reusable cache and must not be affected by operations performed on
the copies associated with the token being processed.
*/
{
  for (const a_shared_pending_pragma &spp : old_list) {
    /* Create a deep copy of the pending pragma. */
    a_shared_pending_pragma new_pp(*spp);

    if (new_pp->descr_ptr->binding_kind == pbk_immediate) {
      /* Immediate pragmas must be reprocessed when rescanned from a token
         cache.  See pragma.h for more information. */
      new_pp->has_been_processed = FALSE;
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    new_pp->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    dest->push_back(new_pp);
  }  /* for */
}  /* copy_fresh_pragmas_into */


void add_to_curr_token_pragma_list(const a_shared_pending_pragma &spp)
/*
Add a pragma to the list of pragmas associated with the current token.
*/
{
  curr_token_pragmas->push_back(spp);
  /* Indicate that the special case code at the beginning of get_token
     is needed to do current token pragma processing. */
  any_initial_get_token_tests_needed = TRUE;
}  /* add_to_curr_token_pragma_list */


void add_to_curr_token_pragma_list(const a_pending_pragma_list &list)
/*
Add the given pragmas to the list of pragmas associated with the current token.
*/
{
  for (const a_shared_pending_pragma &spp : list) {
    curr_token_pragmas->push_back(spp);
  }  /* for */
}  /* add_to_curr_token_pragma_list */


a_shared_pending_pragma add_curr_token_pseudo_pragma(a_pragma_kind     kind,
                                                     a_source_position *pos)
/*
This routine is used to create pragma entries for things like lint comments
that are treated as "pseudo pragmas" by the front end.  A pending pragma is
created and added to the current token pragma list.  The pragma entry is
returned to the caller so that the pragma-specific information can be updated,
if necessary.
*/
{
  a_pragma_kind_description_ptr pkdp =
                                 pragma_description_for_pragma_kind[(int)kind];
  a_shared_pending_pragma       spp = shared_obj<a_pending_pragma>(pkdp);

  /* Ensure the pragma description is a pseudo-pragma description. */
  check_assertion(pkdp->is_pseudo_pragma);
  /* We don't have two positions for pseudo pragmas.  Use the same
     position for both the ID and the start of the directive. */
  spp->id_position = *pos;
  spp->pragma_position = *pos;
  add_to_curr_token_pragma_list(spp);
  return spp;
}  /* add_curr_token_pseudo_pragma */


#if GENERATE_SOURCE_SEQUENCE_LISTS

static a_source_sequence_entry_ptr add_empty_src_seq_entry_for_pragma(
					a_pending_pragma_ptr	ppp)
/*
Create an empty source sequence entry for the pragma specified by ppp
and return a pointer to the newly created entry.
*/
{
  a_memory_region_number	region_to_switch_back_to;
  a_scope_depth			scope_depth_to_switch_to;
  a_source_sequence_entry_ptr	ssep;

  /* If we are inside a function scope, allocate the source sequence entry
     in the function scope so that it will match the IL pragma entry to
     which it gets bound later on.  Global pbk_other pragmas are always
     allocated in the file scope. */
  scope_depth_to_switch_to = scope_stack[depth_scope_stack].
                                               depth_innermost_function_scope;
  if (scope_depth_to_switch_to == NO_SCOPE_DEPTH ||
      (ppp->descr_ptr->binding_kind == (a_pragma_binding_kind)pbk_other &&
       ppp->descr_ptr->global)) {
    scope_depth_to_switch_to = DEPTH_OF_FILE_SCOPE;
  }  /* if */
  switch_to_scope_region(scope_depth_to_switch_to,
                         &region_to_switch_back_to);
  ssep = add_empty_source_sequence_entry();
  switch_back_to_original_region(region_to_switch_back_to);
  return ssep;
}  /* add_empty_src_seq_entry_for_pragma */


static void add_source_sequence_entry_to_curr_token_pragmas(
                                            a_pragma_binding_kind binding_kind)
/*
Loop through the current token pragma list and create an empty source
sequence entry for any pending pragma entry that does not already have
one and whose binding kind matches binding_kind (or all entries if binding_kind
is pbk_none).  This routine is called by routines which may end up removing
entries from the current token pragma list.  The source sequence
entries must be created before any entries are removed to preserve
the original source ordering information.

The empty source sequence entry will be changed to an iek_pragma entry and
completed when the corresponding IL pragma entry is created (or removed
if it turns out that no IL pragma entry is created).
*/
{
  db_enter(4, "add_source_sequence_entry_to_curr_token_pragmas");
  if (!source_sequence_entries_disallowed &&
      ((!is_nonspecialized_instantiation_context() &&
        depth_template_declaration_scope == NO_SCOPE_DEPTH) ||
       is_prototype_instantiation_context())) {
    for (a_shared_pending_pragma spp : *curr_token_pragmas) {
      if (spp->source_sequence_entry == NULL &&
          (binding_kind == pbk_none ||
           binding_kind == spp->descr_ptr->binding_kind)) {
        spp->source_sequence_entry =
                                 add_empty_src_seq_entry_for_pragma(spp.ptr());
        if (spp->il_pragma_entry != NULL) {
          /* This pragma was already processed.  Associate the source sequence
             entry with it. */
          a_source_sequence_entry_ptr  prev_ssep = 
                                  spp->il_pragma_entry->source_sequence_entry;
          update_source_sequence_list((char*)spp->il_pragma_entry,
                                      (an_il_entry_kind)iek_pragma,
                                      spp->source_sequence_entry);
          if (prev_ssep != NULL) {
            remove_src_seq_entry(prev_ssep);
          }  /* if */
          spp->source_sequence_entry = NULL;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
}  /* add_source_sequence_entry_to_curr_token_pragmas */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

a_boolean select_curr_construct_pragmas(a_boolean	add_to_list)
/*
This routine scans the current token pragma list for any pbk_next_construct
pragmas.  If the binding kind matches the flags passed by the caller,
the pragma is copied to the curr_construct_pragmas list.
If add_to_list is TRUE, any new entries are added to the end of
the list.  If it is FALSE, the existing list must be empty.
If binding kind does not match the flags passed by the caller an error
is issued.  After any next construct pragmas have been removed from
the current token pragma list, process_curr_token_pragmas is called to
take the appropriate actions for the remaining pragmas.  If there are
any pragmas on the curr_construct_pragmas (either ones that were
already on the list, or new ones added by this call) return TRUE;
otherwise return FALSE.
*/
{
  a_pending_pragma_list *curr_construct_list = NULL;

  db_enter(4, "select_curr_construct_pragmas");
  if (scope_stack_top().in_disambiguation) {
    /* The current construct is only being prescanned: Don't select pending
       pragmas for the resulting (throw-away) IL.  They will be selected
       instead during the subsequent actual scan. */
    goto done;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Create source sequence entries for any pragmas that don't yet have
     them. */
  add_source_sequence_entry_to_curr_token_pragmas(pbk_next_construct);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  curr_construct_list = *curr_list_of_curr_construct_pragmas();
  if (!add_to_list) {
    if (curr_construct_list != NULL && !curr_construct_list->is_empty() &&
        is_at_least_one_error()) {
      /* There should be no items remaining on the list.  If any errors
         occurred, the list items may be a result of the errors.  Discard
         the items on the list.  If no errors have been issued, an internal
         error will be issued below. */
      curr_construct_list->clear();
    }  /* if */
    check_assertion_str2((curr_construct_list == NULL ||
                          curr_construct_list->is_empty()),
                         "select_curr_construct_pragmas:",
                         "previous list not empty");
  }  /* if */
  if (curr_construct_list == NULL) {
    curr_construct_list = new_fe<a_pending_pragma_list>();
  }  /* if */
  { a_pending_pragma_list tmp_list(*curr_token_pragmas);

    curr_token_pragmas->clear();
    for (a_shared_pending_pragma &spp : tmp_list) {
      a_pragma_kind_description_ptr
                            pkdp = spp->descr_ptr;
      a_pragma_binding_kind binding_kind = pkdp->binding_kind;

      if (binding_kind == pbk_next_construct) {
        /* Add the entry to the end of the list of pragmas for the current
           declaration or statement. */
        curr_construct_list->push_back(spp);
      } else {
        /* The entry remains on the current token's pragma list */
        curr_token_pragmas->push_back(spp);
      }  /* if */
    }  /* for */
  }
#if ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING
  if (curr_construct_list->is_empty() && !no_checking_pragmas &&
      !no_very_expensive_checking) {
    curr_construct_list->push_back(shared_obj<a_pending_pragma>(
                 pragma_description_for_pragma_kind[(int)pk_checking_pragma]));
#if GENERATE_SOURCE_SEQUENCE_LISTS
    curr_construct_list->back_elem()->source_sequence_entry =
                                             add_empty_source_sequence_entry();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
#endif /* ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING */
  *curr_list_of_curr_construct_pragmas() = curr_construct_list;
  /* Call process_curr_token_pragmas to handle other pragma kinds.  This
     ensures that any immediate pragmas will be processed before any
     next construct pragmas found at the same point. */
  if (!curr_token_pragmas->is_empty()) process_curr_token_pragmas();
done:
  db_exit();
  /* Return TRUE if there are any entries of the list. */
  return curr_construct_list != NULL && !curr_construct_list->is_empty();
}  /* select_curr_construct_pragmas */


void add_pragma_to_il(a_pending_pragma_ptr  ppp,
                      an_il_entry_kind      entity_kind,
                      char                  *entity_ptr,
                      a_boolean             is_global)
/*
ppp points to the front-end representation of a pragma.  When the pragma
binding kind is pbk_next, entity_ptr is a pointer to the IL entry of the
specified entity_kind with which the pragma is associated; otherwise,
entity_ptr is NULL.  is_global, which is used only when entity_ptr is NULL,
indicates whether the unbound entity is global or local in scope.

This routine (1) allocates the IL pragma entry and initializes it, (2)
binds it to the entity it's associated with, if any, and sets the
has_associated_pragma flag in the latter, (3) adds it to the appropriate
scope pragma list, (4) updates the source sequence entry if there is one,
and (5) returns a pointer to the IL pragma entry to the caller, in case
there is additional processing to be done.
*/
{
  db_enter(5, "add_pragma_to_il");
  /* This function can only be called when the scope stack has at least one
     element. */
  check_assertion(depth_scope_stack != NO_SCOPE_DEPTH);
  if (scope_stack_top().in_nonreal_instantiation) {
    /* Pragmas are never added to the IL inside a nonreal instantiation. */
  } else if (in_constexpr_if_discarded_statement()) {
    /* Pragmas from C++17 constexpr if discarded statements are not added
       to the IL. */
  } else {
    a_pragma_ptr pp;

    /* Determine the memory region in which the IL pragma entry should be
       allocated and the scope_depth of the scope entry to which it should
       be attached. */
    if (entity_ptr == NULL) {
      a_scope_depth scope_depth = depth_scope_stack;
      /* The pragma is not associated with any entity.  If it is a global
         pragma, associate it with the file scope; otherwise, it belongs to
         the local context. */
      if (is_global) {
        scope_depth = DEPTH_OF_FILE_SCOPE;
      } else {
        /* Make sure that this scope is one to which a pragma can be
           attached.  If the current scope has no associated IL scope,
           find the nearest enclosing scope with an associated IL scope. */
        a_scope_stack_entry_ptr	ssep = scope_stack_entry_for(scope_depth);
        a_boolean		done = FALSE;
        /*lint -e{440}*/
        for (; !done; ssep = previous_scope_of(ssep)) {
          check_assertion(ssep != NULL);
          switch (ssep->kind) {
            case sck_class_struct_union:
              /* A pragma can only be added to a class scope in C++ mode. */
              if (C_mode()) break;
              FALLTHROUGH
            /* Scopes for which a pragma entry may be added to the IL. */
            case sck_enum:
            case sck_file:
            case sck_block:
            case sck_namespace:
            case sck_namespace_extension:
            case sck_function:
              done = TRUE;
              /* Convert the scope stack entry back to a scope depth. */
              scope_depth = scope_depth_of(ssep);
              break;
            /* Scopes for which a pragma entry may not be added to the IL.
               Function prototypes do, in a way, have an associated IL scope,
               but pragmas are not expected to be bound to function prototype
               scopes.  Similarly, template declaration scopes have IL scopes
               when prototype instantiations are included in the IL, but
               cannot have pragmas bound to them. */
            case sck_func_prototype:
            case sck_condition:
            case sck_function_access:
            case sck_pragma:
            case sck_template_instantiation:
            case sck_template_declaration:
            case sck_class_reactivation:
            case sck_namespace_reactivation:
              break;
            default:
              unexpected_condition_str("add_pragma_to_il: bad scope kind");
          }  /* switch */
        }  /* for */
      }  /* if */
      /* Ensure there's an IL scope to attach the pragma to. */
      ensure_il_scope_exists(&scope_stack[scope_depth]);
      /* Construct an IL pragma and attach it to the scope. */
      pp = add_non_entity_pragma_to_list(
                                     ppp->descr_ptr,
                                     ppp->pragma_position,
                                     ppp->pragma_text,
#if MICROSOFT_EXTENSIONS_ALLOWED
                                     ppp->is_microsoft_pragma_operator,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                     scope_depth);
    } else {
      /* Construct an IL pragma and associate it with the entity. */
      pp = add_entity_pragma_to_list(
                                     ppp->descr_ptr,
                                     ppp->pragma_position,
                                     ppp->pragma_text,
#if MICROSOFT_EXTENSIONS_ALLOWED
                                     ppp->is_microsoft_pragma_operator,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                     entity_ptr,
                                     entity_kind);
#if EXPENSIVE_CHECKING
      /* Verify that the constructed IL pragma can be found. */
      if (!no_find_pragma_validation && !no_very_expensive_checking) {
        a_pragma_ptr                npp = NULL;
        a_boolean                   tu_pushed = FALSE;
        a_scope_ptr                 scope_for_function_local = NULL;
        a_source_correspondence_ptr scp =
                                       source_corresp_for_il_entry(entity_ptr,
                                                                  entity_kind);
        if (scp != NULL) {
          a_symbol_ptr sym = (a_symbol_ptr)scp->assoc_info;

          tu_pushed = push_translation_unit_if_needed(sym);
        }  /* if */

        a_scope_depth func_scope_depth = get_depth_innermost_function_scope();
        if (func_scope_depth != NO_SCOPE_DEPTH) {
          scope_for_function_local = scope_stack[func_scope_depth].il_scope;
        } else if (is_template_declaration_context()) {
          check_assertion(depth_template_declaration_scope != NO_SCOPE_DEPTH);
          scope_for_function_local =
                        scope_stack[depth_template_declaration_scope].il_scope;
        }  /* if */
        do {
          npp = find_assoc_pragma(entity_ptr, entity_kind,
                                  scope_for_function_local,
                                  npp);
        } while (pp != npp && npp->next != NULL);
        check_assertion(pp == npp);
        if (tu_pushed) {
          pop_translation_unit_stack();
        }  /* if */
      }  /* if */
#endif /* EXPENSIVE_CHECKING */
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    update_source_sequence_list((char *)pp, iek_pragma,
                                ppp->source_sequence_entry);
    /* The source sequence entry is now attached to the IL pragma entry.
       Clear the copy of the source_sequence_entry pointer in the pending
       pragma entry because it is now obsolete. */
    ppp->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    ppp->il_pragma_entry = pp;
  }  /* if */
  db_exit();
}  /* add_pragma_to_il */


void create_il_entry_for_pragma(a_pending_pragma_ptr ppp,
                                a_symbol_ptr         sym,
                                a_statement_ptr      sp)
/*
Create an IL pragma entry for a pending pragma entry.  If either
sym or sp is non-NULL, bind the pragma IL entry to the IL entry
indicated by sym or sp.  For pbk_next_construct pragmas, a non-NULL sym
or sp pointer must be supplied.  The IL entry is then added to the IL.
*/
{
  char              		 *entity;
  an_il_entry_kind	 	 entity_kind;
  a_boolean	         	 is_global = FALSE;
  a_pragma_kind_description_ptr	 pkdp;

  db_enter(5, "create_il_entry_for_pragma");
  pkdp = ppp->descr_ptr;
#if CHECKING
  /* Next construct pragmas must be bound to an IL entry.  Other binding
     kinds may optionally be bound to an IL entry. */
  if (pkdp->binding_kind == pbk_next_construct) {
    check_assertion_str2((sym == NULL) != (sp == NULL),
                         "create_il_entry_for_pragma:",
                         "invalid next_construct call");
  }  /* if */
#endif /* CHECKING */
  /* Don't create IL entries during token caching. */
  if (!caching_tokens) {
    if (sym != NULL) {
      entity = il_entry_for_symbol(sym, &entity_kind);
    } else if (sp != NULL) {
      entity = (char *)sp;
      entity_kind = (an_il_entry_kind)iek_statement;
    } else {
      entity = NULL;
      entity_kind = (an_il_entry_kind)iek_none;
      is_global = pkdp->global;
    }  /* if */
    add_pragma_to_il(ppp, entity_kind, entity, is_global);
  }  /* if */
  db_exit();
}  /* create_il_entry_for_pragma */


void process_curr_token_pragmas(void)
/*
Called by get_token to process pragmas that were found before the
token that is about to become the previous token.

Any pragmas that bind to the next statement/declaration should have
already been removed from the list (assuming that the pragmas were
legally placed).  Diagnostics are issued for any such pragmas that
remain on the list.

pbk_other pragmas are moved to the pragma list of the current scope stack
entry.

pbk_immediate pragmas are processed here.
*/
{
  a_pragma_kind_description_ptr	pkdp;
  a_next_token_pragma_function_ptr
                                ntfp;
  an_immediate_pragma_function_ptr
                                ipfp;

  db_enter(5, "process_curr_token_pragmas");
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Create source sequence entries for any pragmas that don't yet have
     them. */
  add_source_sequence_entry_to_curr_token_pragmas(pbk_none);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  a_pending_pragma_list tmp_list(*curr_token_pragmas);
  curr_token_pragmas->clear();
  for (a_shared_pending_pragma &spp : tmp_list) {
    pkdp = spp->descr_ptr;
    if (spp->has_been_processed) {
      /* This pragma has already been processed (but remains on the list to
         preserve pragma information within the token cache). */
      continue;
    }  /* if */
    spp->has_been_processed = TRUE;
    switch (pkdp->binding_kind) {
      case pbk_next_construct:
        if (pkdp->error_severity != es_none) {
          an_error_code	error_code;
          /* Select the appropriate error based on the kinds of constructs
             that this pragma may bind to. */
          if (pkdp->may_bind_to_decl && pkdp->may_bind_to_stmt) {
            error_code = ec_pragma_must_precede_decl_or_stmt;
          } else if (pkdp->may_bind_to_decl) {
            error_code = ec_pragma_must_precede_declaration;
          } else {
            error_code = ec_pragma_must_precede_statement;
          }  /* if */
          if (pkdp->error_severity != es_none) {
            pos_diagnostic(pkdp->error_severity, error_code,
                           &spp->id_position);
          }  /* if */
        }  /* if */
        break;
      case pbk_next_token:
        /* Next token pragmas are processed when the token they precede is
           discarded. */
        if (pkdp->automatically_include_in_il) {
          /* Create an IL entry for pragmas that should automatically be
             included in the IL. */
          create_il_entry_for_pragma(spp.ptr(), (a_symbol_ptr)NULL,
                                     (a_statement_ptr)NULL);
        }  /* if */
        ntfp = (a_next_token_pragma_function_ptr)index_to_function_pointer(
                                              pkdp->processing_function_index);
        if (ntfp != NULL) {
          (*ntfp)(spp.ptr());
        }  /* if */
        break;
      case pbk_immediate:
        if (pkdp->automatically_include_in_il) {
          /* Create an IL entry for pragmas that should automatically be
             included in the IL. */
          create_il_entry_for_pragma(spp.ptr(), (a_symbol_ptr)NULL,
                                     (a_statement_ptr)NULL);
        }  /* if */
        ipfp = (an_immediate_pragma_function_ptr)index_to_function_pointer(
                                            pkdp->processing_function_index);
        if (ipfp != NULL) {
          (*ipfp)(spp.ptr());
        }  /* if */
        break;
      case pbk_other:
        {
          /* Add this pragma to the pending pragmas list of the current
             scope. */
          a_scope_stack_entry_ptr ssep = &scope_stack[depth_scope_stack];

          if (ssep->pending_pragmas == NULL) {
            ssep->pending_pragmas = new_fe<a_pending_pragma_list>();
          }  /* if */
          ssep->pending_pragmas->push_back(spp);
        }
        break;
      default:
        unexpected_condition_str(
                               "process_curr_token_pragmas: bad binding kind");
        break;
    }  /* switch */
  }  /* for */
  db_exit();
}  /* process_curr_token_pragmas */


void process_immediate_pragmas(void)
/*
Go through the current token pragma list, process any immediate pragmas
on the list.  The pragmas remain on the list, but are marked has having
been processed.  They are kept on the list in case they are associated
with a token that is to be cached.
*/
{
  a_pending_pragma_list         *saved_curr_token_pragmas;
  a_pragma_kind_description_ptr pkdp;
  an_immediate_pragma_function_ptr
                                ipfp;

#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Create source sequence entries for any pragmas that don't yet have
     them. */
  if (!caching_tokens) {
    add_source_sequence_entry_to_curr_token_pragmas(pbk_immediate);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Clear curr_token_pragmas so that enclosing pragmas won't be considered
     part of this pragma. */
  saved_curr_token_pragmas = curr_token_pragmas;
  curr_token_pragmas = new_fe<a_pending_pragma_list>();
  for (a_shared_pending_pragma &spp : *saved_curr_token_pragmas) {
    pkdp = spp->descr_ptr;
    if (pkdp->binding_kind == pbk_immediate) {
      if (!spp->has_been_processed) {
        /* Unless this token is going into a reusable token cache, mark this
           pragma as having been processed so that it won't be applied again by
           process_curr_token_pragmas. */
        spp->has_been_processed = TRUE;
        if (pkdp->automatically_include_in_il ||
            is_template_declaration_context()) {
          /* Create an IL entry for pragmas that should automatically be
             included in the IL. */
          create_il_entry_for_pragma(spp.ptr(), (a_symbol_ptr)NULL,
                                     (a_statement_ptr)NULL);
        }  /* if */
        ipfp = (an_immediate_pragma_function_ptr)index_to_function_pointer(
                                              pkdp->processing_function_index);
        if (ipfp != NULL) {
          (*ipfp)(spp.ptr());
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  check_assertion(curr_token_pragmas->is_empty());
  delete_fe(&curr_token_pragmas);
  curr_token_pragmas = saved_curr_token_pragmas;
}  /* process_immediate_pragmas */


void end_of_scope_pragma_processing(const a_pending_pragma_list &ppl)
/*
This routine is called by pop_scope to process any pbk_other pragmas
that remain on the pending pragma list of the current scope.  pending_pragmas
is the pending pragma list to be processed.

Go through the list and issue diagnostics that indicate that this
pragma is not valid in this location.
*/
{
  db_enter(4, "end_of_scope_pragma_processing");
  for (const a_shared_pending_pragma &spp : ppl) {
    a_pragma_kind_description_ptr pkdp = spp->descr_ptr;

    if (pkdp->error_severity != es_none) {
      pos_diagnostic(pkdp->error_severity, ec_pragma_may_not_be_used_here,
                     &spp->id_position);
    }  /* if */
  }  /* for */
  db_exit();
}  /* end_of_scope_pragma_processing */


a_pending_pragma_list extract_specific_pragmas(
                                              a_pragma_kind    kind,
                                              a_symbol_ptr     sym,
                                              a_statement_ptr  sp,
                                              a_boolean        curr_scope_only)
/*
Return a list of pending-pragma entries of the specified pragma kind.  If the
pragma binds to the current declaration or statement and the pragma's
automatically_include_in_il flag is TRUE, then the current construct must be
specified: either sym, if this is a declaration, or sp, if it's a statement,
but not both, must be non-NULL.  If automatically_include_in_il is TRUE, the IL
entry is created before the associated pending-pragma entry is returned.  If
more than one pending pragma entry of the required kind is found, they are
returned in a linked list.  This is possible, since the entries returned are
first removed from the lists they currently reside on.

The curr_scope_only flag limits the search to pragmas in the current scope
instead of looking through all of the active scope stack entries.
*/
{
  a_pending_pragma_list          **scope_list_addr;
  a_pragma_kind_description_ptr  pkdp;
  a_pending_pragma_list          new_list;
  a_boolean                      is_bound_to_curr_construct;
  a_scope_stack_entry_ptr        ssep;

  db_enter(4, "extract_specific_pragmas");
  /* Get the pragma description entry for the specified kind. */
  pkdp = pragma_description_for_pragma_kind[(int)kind];
  if (pkdp->binding_kind == (a_pragma_binding_kind)pbk_next_construct) {
    /* Set up to search for a pragma bound to the current declaration or
       statement. */
    is_bound_to_curr_construct = TRUE;
    ssep = &scope_stack[depth_scope_stack];
    scope_list_addr = curr_list_of_curr_construct_pragmas();
  } else {
    /* Set up to search for a pbk_other pragma. */
    is_bound_to_curr_construct = FALSE;
    ssep = &scope_stack[depth_scope_stack];
    scope_list_addr = &ssep->pending_pragmas;
  }  /* if */
  /* The outer loop examines one or more scope stack entries.  If it's a
     bind-to-next pragma, only the current scope stack list is checked.
     Otherwise, if it's a global pragma, only the file scope list is checked;
     otherwise, all the scope stack lists, from the current scope stack out
     to the file scope, are checked in turn. */
  for (;;) {
    if (*scope_list_addr != NULL && !(*scope_list_addr)->is_empty()) {
      a_pending_pragma_list scope_list_copy(**scope_list_addr);

      (*scope_list_addr)->clear();
      /* Check the appropriate list of pending-pragma entries. */
      for (a_shared_pending_pragma &spp : scope_list_copy) {
        if (spp->descr_ptr == pkdp) {
          /* It's the right kind: remove it from the scope stack list. */
          new_list.push_back(spp);
          /* If an IL pragma should be generated for it, do that now. */
          if (pkdp->automatically_include_in_il) {
            create_il_entry_for_pragma(spp.ptr(), sym, sp);
          }  /* if */
        } else {
          /* It's not the right kind: keep it on the scope stack list. */
          (*scope_list_addr)->push_back(spp);
        }  /* if */
      }  /* for */
    }  /* if */
    /* The appropriate list for the scope has been examined.  Move on the
       containing scope if appropriate; otherwise, terminate the loop. */
    if (is_bound_to_curr_construct || curr_scope_only) {
      /* Only one iteration of the loop for bind-to-next pragmas. */
      break;
    } else if (ssep == &scope_stack[DEPTH_OF_FILE_SCOPE]) {
      /* Nothing else on the scope stack. */
      break;
    } else {
      if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
        /* If this is a template instantiation scope then skip directly from
           here to the file scope. */
        ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
      } else {
        /* Advance on through the scope stack. */
        --ssep;
      }  /* if */
      scope_list_addr = &ssep->pending_pragmas;
    }  /* if */
  }  /* for */
  db_exit();
  return new_list;
}  /* extract_specific_pragmas */


void process_curr_construct_pragmas(a_symbol_ptr     sym,
                                    a_statement_ptr  sp)
/*
Go through the list of pragmas that are to be bound to the current
declaration or statement and perform any actions required to process
the pragmas.
*/
{
  db_enter(4, "process_curr_construct_pragmas");
  check_assertion_str((sym == NULL) == (sp != NULL),
                      "process_pragmas_bound...: invalid arguments");
  if (scope_stack_top().in_disambiguation) {
    /* The current construct has only been prescanned: Don't apply pending
       pragmas to the resulting (throw-away) IL.  They will be applied again
       during the actual scan. */
    goto done;
  }  /* if */
  /* Go though the pragmas that are meant to apply to the current
     declaration or statement. */
  if (*curr_list_of_curr_construct_pragmas() != NULL &&
      !(*curr_list_of_curr_construct_pragmas())->is_empty()) {
    a_pending_pragma_list tmp_list(**curr_list_of_curr_construct_pragmas());

    /* Clear the list now so that pragmas can be added to this list as
       a consequence of processing the list of pragmas. */
    delete_fe(curr_list_of_curr_construct_pragmas());
    for (a_shared_pending_pragma &spp : tmp_list) {
      a_next_construct_pragma_function_ptr ncpfp;
      a_boolean                            err = FALSE;
      a_pragma_kind_description_ptr	   pkdp = spp->descr_ptr;

      /* Make sure that the binding information in the pragma description
         is consistent with the argument list.  Issue diagnostics for
         any pragmas that cannot bind to the current construct. */
      if ((pkdp->may_bind_to_decl && sym != NULL) ||
          (pkdp->may_bind_to_stmt && sp != NULL)) {
        /* Pragma kind matches arguments. */
      } else {
        /* The pragma binding does not match the kind of construct being
           processed.  Issue a diagnostic. */
        an_error_code	error_code;
        err = TRUE;
        if (pkdp->error_severity != es_none) {
          if (pkdp->may_bind_to_decl) {
            error_code = ec_pragma_must_precede_declaration;
          } else {
            check_assertion(pkdp->may_bind_to_stmt);
            error_code = ec_pragma_must_precede_statement;
          }  /* if */
          pos_diagnostic(pkdp->error_severity, error_code, &spp->id_position);
        }  /* if */
      }  /* if */
      if (!err) {
        ncpfp = (a_next_construct_pragma_function_ptr)
                                                     index_to_function_pointer(
                                              pkdp->processing_function_index);
        if (pkdp->automatically_include_in_il) {
          /* Create an IL entry for pragmas that should automatically be
             included in the IL. */
          create_il_entry_for_pragma(spp.ptr(), sym, sp);
        }  /* if */
        if (ncpfp != NULL) {
          /* Call the pragma processing function associated with this
             pragma. */
          (*ncpfp)(spp.ptr(), sym, sp);
        }  /* if */
      }  /* if */
    }  /* for */
  }
done:
  db_exit();
}  /* process_curr_construct_pragmas */


void cannot_bind_to_curr_construct(void)
/*
While processing a construct the caller has determined that it is
not possible to bind pragmas to the construct.  This routine
issues diagnostics that indicate the pragma could not be bound and
clears the curr_construct_pragma list.
*/
{

  db_enter(4, "cannot_bind_to_curr_construct");
  if (*curr_list_of_curr_construct_pragmas() != NULL &&
      !(*curr_list_of_curr_construct_pragmas())->is_empty()) {
    a_pending_pragma_list tmp_list(**curr_list_of_curr_construct_pragmas());
    delete_fe(curr_list_of_curr_construct_pragmas());
    for (a_shared_pending_pragma &spp : tmp_list) {
      a_pragma_kind_description_ptr pkdp = spp->descr_ptr;
      if (pkdp->error_severity != es_none) {
        pos_diagnostic(pkdp->error_severity, ec_pragma_may_not_be_used_here,
                       &spp->id_position);
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
}  /* cannot_bind_to_curr_construct */


void discard_curr_construct_pragmas(void)
/*
This routine is called when an error is encountered while processing
a construct, and as a result of that error it is not possible to
do the binding of any current construct pragmas.  The list of
current construct pragmas is simply cleared.
*/
{
  db_enter(4, "discard_curr_construct_pragmas");
  delete_fe(curr_list_of_curr_construct_pragmas());
  db_exit();
}  /* discard_curr_construct_pragmas */


a_pending_pragma_list* extract_curr_construct_pragmas()
/*
This routine gets the pointer to the list of current construct
pragmas, goes through the list and clears any removes any source
sequence entries that may exist, clears the current construct pragma
entry on the scope stack, and returns the pragma list to the caller.
This is used for saving the current construct pragma list so that
the pragmas may be applied to each instance of a template.
*/
{
  a_pending_pragma_list *result = NULL;
  a_pending_pragma_list **scope_list_addr =
                                         curr_list_of_curr_construct_pragmas();

  if (*scope_list_addr != NULL && !(*scope_list_addr)->is_empty()) {
    result = *scope_list_addr;
    *scope_list_addr = NULL;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (result != NULL) {
    for (a_shared_pending_pragma &spp : *result) {
      if (spp->source_sequence_entry != NULL) {
        /* If this source sequence entry was never bound to another IL entry,
           remove it from the source sequence list. */
        check_assertion_str2((spp->source_sequence_entry->entity.kind ==
                                                                     iek_none),
                             "extract_curr_construct_pragmas:",
                             "source sequence entry already in use");
        remove_from_src_seq_list(spp->source_sequence_entry);
        spp->source_sequence_entry = NULL;
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  return result;
}  /* extract_curr_construct_pragmas */


void reactivate_curr_construct_pragmas(a_pending_pragma_list *pplp)
/*
Restore a list of pragmas as the current token pragmas.
*/
{
  a_pending_pragma_list  **scope_list_addr;

  db_enter(4, "reactivate_curr_construct_pragmas");
  scope_list_addr = curr_list_of_curr_construct_pragmas();
  check_assertion_str2(*scope_list_addr == NULL ||
                       (*scope_list_addr)->is_empty(),
                       "reactivate_curr_construct_pragmas:",
                       "pragma list not already empty");
  /* Make a copy of the list of pragmas associated with this template and
     set this scope's current construct list to point to the new copy. */
  if (pplp != NULL && !pplp->is_empty()) {
    *scope_list_addr = new_fe<a_pending_pragma_list>(pplp->length());
    copy_fresh_pragmas_into(*scope_list_addr, *pplp);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* The source sequence entries were cleared when the current construct
       pragmas were extracted.  Create new source sequence entries now. */
    for (a_shared_pending_pragma &spp : **scope_list_addr) {
      spp->source_sequence_entry =
                                 add_empty_src_seq_entry_for_pragma(spp.ptr());
    }  /* for */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  } else {
    *scope_list_addr = NULL;
  }  /* if */
  db_exit();
}  /* reactivate_curr_construct_pragmas */


void process_pragmas_at_end_of_source(void)
/*
Do any pragma processing that is required when the end of the source file
has been reached.
*/
{
  db_enter(4, "process_pragmas_at_end_of_source");
  /* Process any current token pragmas that appeared after all other
     tokens of the source program. */
  process_curr_token_pragmas();
  db_exit();
}  /* process_pragmas_at_end_of_source */

#if DEBUG

void db_opt_pragma(a_pending_pragma_ptr	ppp)
/*
The routine called when a db_opt pragma is encountered.  Process the
pragma argument as if it were a debug option specified on the command-line.
*/
{
  if (!db_active) {
    /* In order for the db_opt pragma to be used a debug option must have
       been specified on the command-line.  This is needed because
       db_active cannot be set TRUE in the middle of a compilation. */
    pos_error(ec_db_option_required_on_cmd_line, &ppp->pragma_position);
  } else {
    char	*debug_arg = ppp->pragma_text;
    /* Skip past the debug pragma name. */
    debug_arg = strchr(debug_arg, ' ');
    if (debug_arg != NULL) {
      char	*arg_copy;
      /* Skip past the blank. */
      debug_arg++;
      /* Make a copy of the argument. */
      arg_copy = alloc_general((sizeof_t)(strlen(debug_arg) + 1));
      (void)strcpy(arg_copy, debug_arg);
      (void)proc_debug_option(arg_copy);
    }  /* if */
  }  /* if */
}  /* db_opt_pragma */

void db_name_pragma(a_pending_pragma_ptr	ppp)
/*
The routine called when a db_name pragma is encountered.  Process the
pragma argument as if it were a debug option specified on the command-line.
*/
{
  if (!db_active) {
    /* In order for the db_name pragma to be used a debug option must have
       been specified on the command-line.  This is needed because
       db_active cannot be set TRUE in the middle of a compilation. */
    pos_error(ec_db_option_required_on_cmd_line, &ppp->pragma_position);
  } else {
    char	*debug_arg = ppp->pragma_text;
    /* Skip past the debug pragma name. */
    debug_arg = strchr(debug_arg, ' ');
    if (debug_arg != NULL) {
      char	*arg_copy;
      /* Skip past the blank. */
      debug_arg++;
      /* Make a copy of the argument. */
      arg_copy = alloc_general((sizeof_t)(strlen(debug_arg) + 1));
      (void)strcpy(arg_copy, debug_arg);
      (void)proc_debug_name_option(arg_copy);
    }  /* if */
  }  /* if */
}  /* db_name_pragma */

#endif /* DEBUG */

#if INCLUDE_EDG_TEST_PRAGMAS
void test_immediate_pragma(a_pending_pragma_ptr ppp)
/*
Routine called by the "#pragma test_immediate", a pragma included
by EDG for testing purposes.
*/
{
  a_symbol_ptr       sym = NULL;
  a_boolean          err = FALSE;

  begin_rescan_of_pragma_tokens(ppp);
  if (is_generalized_identifier_start(GID_NO_OPTIONS)) {
    sym = coalesce_and_lookup_generalized_identifier(GID_NO_OPTIONS,
						     ilm_normal, &err);
  }  /* if */
  /* Flush to the end of the pragma. */
  while (curr_token != tok_newline && curr_token != tok_end_of_source) {
    (void)get_token();
  }  /* while */
  wrapup_rescan_of_pragma_tokens(err);
  if (sym != NULL) {
    create_il_entry_for_pragma(ppp, sym, (a_statement_ptr)NULL);
  }  /* if */
}  /* test_immediate_pragma */


void test_next_construct_pragma(ARG_UNUSED a_pending_pragma_ptr ppp,
                                ARG_UNUSED a_symbol_ptr         sym_ptr,
                                ARG_UNUSED a_statement_ptr      stmt_ptr)
/*
Routine called by the test_next_decl and test_next_statement pragmas that
are included by EDG for testing purposes.
*/
{
#if DEBUG
  if (db_flag_is_set("test_pragmas")) {
    fprintf(f_debug, "In test_next_construct pragma\n");
    db_sym(sym_ptr);
    db_statement(stmt_ptr);
  }  /* if */
#endif /* DEBUG */
}  /* test_next_construct_pragma */
#endif /* INCLUDE_EDG_TEST_PRAGMAS */

void pragma_one_time_init(void)
/*
Do one-time initialization of variables related to pragma processing.
(Variables that need to be reinitialized with each new translation unit
are handled in pragma_init.)
*/
{
  /* Save variables from pragma.h and pragma.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(curr_token_pragmas),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* pragma_one_time_init */


void pragma_init(void)
/*
Initialize the pragma description table.
*/
{
  int       i;
  a_boolean ignore_diag_pragma_in_be =
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
    /* The GNU, clang, Microsoft, and Sun compilers do not recognize the
       EDG-specific diagnostic pragmas, so they should not be emitted in
       generated code intended for those compilers.  Other targets may or
       may not have processing for these pragmas, so it's best to include
       them. */
    (gcc_is_generated_code_target || clang_is_generated_code_target ||
     microsoft_dialect_is_generated_code_target ||
     sun_is_generated_code_target);
#else /* !(BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) */
    /* We don't know whether the back end will have processing for the
       diagnostic pragmas, so it's safest not to flag them as being
       ignored. */
    FALSE;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  db_enter(3, "pragma_init");
  /* Clear the array used to get a pragma description pointer based on a
     pragma kind. */
  for (i = (int)pk_none; i < (int)pk_last; ++i) {
    pragma_description_for_pragma_kind[i] =
					 (a_pragma_kind_description_ptr)NULL;
  }  /* for */
  pragma_kind_descriptions = NULL;
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_printf_args,
		 fn_for_function(record_arg_pragma),
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_scanf_args,
	         fn_for_function(record_arg_pragma),
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_lint_argsused,
		 (a_function_number)fn_null,
		 /*is_pseudo_pragma=*/TRUE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_none);
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_lint_varargs_count,
		 (a_function_number)fn_null,
		 /*is_pseudo_pragma=*/TRUE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_none);
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_lint_notreached,
		 (a_function_number)fn_null,
		 /*is_pseudo_pragma=*/TRUE,
		 /*may_bind_to_decl=*/FALSE,
		 /*may_bind_to_stmt=*/TRUE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_none);
  if (!C_mode()) {
    (void)add_next_token_pragma_kind_description
 		((a_pragma_kind)pk_instantiate,
	         fn_for_function(instantiation_pragma),
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
    (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_do_not_instantiate,
		 fn_for_function(instantiation_pragma),
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
    (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_can_instantiate,
		 fn_for_function(instantiation_pragma),
		 /*is_pseudo_pragma=*/FALSE,
		 /*global=*/FALSE,
		 /*automatically_include_in_il=*/FALSE,
		 /*record_pragma_text=*/FALSE,
		 /*expand_macros=*/TRUE,
		 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
		 es_error);
  }  /* if */
  if (pragma_pack_enabled) {
    (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_pack,
                 fn_for_function(pack_pragma),
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/BACK_END_IS_CP_GEN_BE,
                 /*record_pragma_text=*/BACK_END_IS_CP_GEN_BE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/FALSE,
                 /*il_info_is_complete=*/FALSE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
  }  /* if */
#if IDENT_DIRECTIVE_AND_PRAGMA
  /* For "#pragma ident": */
  (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_ident_pragma,
                 fn_for_function(ident_pragma),
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/TRUE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/TRUE,
                 /*ignore_in_back_end=*/FALSE,
                 /*il_info_is_complete=*/FALSE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
  /* For "#ident": */
  (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_ident_directive,
                 fn_for_function(ident_directive),
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/TRUE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/FALSE,
                 /*il_info_is_complete=*/FALSE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if PRAGMA_WEAK_ALLOWED
  (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_weak,
                 fn_for_function(weak_pragma),
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
#endif /* PRAGMA_WEAK_ALLOWED */
  (void)add_preproc_immediate_pragma_kind_description
                ((a_pragma_kind)pk_once,
                 fn_for_function(once_pragma),
                 /*record_pragma_text=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*allowed_in_pragma_operator=*/TRUE,
		 /*read_string_as_header_name=*/FALSE);
  (void)add_preproc_immediate_pragma_kind_description
                ((a_pragma_kind)pk_hdrstop,
                 fn_for_function(hdrstop_or_no_pch_pragma),
                 /*record_pragma_text=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*allowed_in_pragma_operator=*/FALSE,
		 /*read_string_as_header_name=*/FALSE);
  (void)add_preproc_immediate_pragma_kind_description
                ((a_pragma_kind)pk_no_pch,
                 fn_for_function(hdrstop_or_no_pch_pragma),
                 /*record_pragma_text=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*allowed_in_pragma_operator=*/FALSE,
		 /*read_string_as_header_name=*/FALSE);
  if (!C_mode()) {
    (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_define_type_info,
		 fn_for_function(define_type_info_pragma),
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/FALSE,
                 /*automatically_include_in_il=*/BACK_END_IS_CP_GEN_BE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/!BACK_END_IS_CP_GEN_BE, /*lint !e506*/
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  }  /* if */
  if (c99_mode || cpp11_mode || fixed_point_enabled) {
    (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_stdc,
                 fn_for_function(stdc_pragma),
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/TRUE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  }  /* if */
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode) {
    (void)add_next_token_pragma_kind_description(
                                         (a_pragma_kind)pk_upc,
                                         fn_for_function(upc_pragma),
                                         /*is_pseudo_pragma=*/FALSE,
                                         /*global=*/TRUE,
                                         /*automatically_include_in_il=*/FALSE,
                                         /*record_pragma_text=*/FALSE,
                                         /*expand_macros=*/FALSE,
                                         /*processing_C_code=*/FALSE,
			                 /*fetch_pp_tokens=*/FALSE,
                                         /*ignore_in_back_end=*/FALSE,
                                         /*il_info_is_complete=*/TRUE,
                                         /*read_string_as_header_name=*/FALSE,
                                         es_error);
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
  (void)add_next_token_pragma_kind_description(
                                         (a_pragma_kind)pk_redefine_extname,
                                         fn_for_function(
                                                      redefine_extname_pragma),
                                         /*is_pseudo_pragma=*/FALSE,
                                         /*global=*/TRUE,
                                         /*automatically_include_in_il=*/FALSE,
                                         /*record_pragma_text=*/FALSE,
                                         /*expand_macros=*/FALSE,
                                         /*processing_C_code=*/FALSE,
			                 /*fetch_pp_tokens=*/FALSE,
                                         /*ignore_in_back_end=*/FALSE,
                                         /*il_info_is_complete=*/TRUE,
                                         /*read_string_as_header_name=*/FALSE,
                                         es_error);
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if SUN_EXTENSIONS_ALLOWED
  if (sun_linker_scope_allowed) {
    (void)add_preproc_immediate_pragma_kind_description(
                                        (a_pragma_kind)pk_enable_ldscope,
                                        fn_for_function(ldscope_pragma),
		                        /*record_pragma_text=*/TRUE,
                                        /*il_info_is_complete=*/TRUE,
                                        /*automatically_include_in_il=*/TRUE,
					/*ignore_in_back_end=*/FALSE,
					/*allowed_in_pragma_operator=*/TRUE,
                                        /*read_string_as_header_name=*/FALSE);
    (void)add_preproc_immediate_pragma_kind_description(
                                        (a_pragma_kind)pk_disable_ldscope,
                                        fn_for_function(ldscope_pragma),
		                        /*record_pragma_text=*/TRUE,
                                        /*il_info_is_complete=*/TRUE,
                                        /*automatically_include_in_il=*/TRUE,
					/*ignore_in_back_end=*/FALSE,
					/*allowed_in_pragma_operator=*/TRUE,
                                        /*read_string_as_header_name=*/FALSE);
  }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode && gnu_version >= 40200) {
    /* "GCC" pragmas are handled by pk_gcc_next_statement, pk_gcc_next_token,
       or pk_gcc_immediate depending on the specific variety of "GCC" pragma.
       Currently "GCC unroll", "GCC ivdep", and "GCC novector" pragmas
       (which apply to the loop that follows them) are pk_gcc_next_statement,
       "GCC diagnostic" pragmas are pk_gcc_next_token, and all others are
       pk_gcc_immediate.  Note that the order of these next three calls is
       important (look_up_pragma_id relies on it). */
    (void)add_next_construct_pragma_kind_description
                 ((a_pragma_kind)pk_gcc_next_statement,
                 fn_for_function(gcc_loop_pragma),
                 /*is_pseudo_pragma=*/FALSE,
                 /*may_bind_to_decl=*/FALSE,
                 /*may_bind_to_stmt=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/FALSE,
                 /*il_info_is_complete=*/TRUE,
                 /*read_string_as_header_name=*/FALSE,
                 es_warning);
    (void)add_next_token_pragma_kind_description
                 ((a_pragma_kind)pk_gcc_next_token,
                 fn_for_function(gcc_pragma),
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/FALSE,
                 /*il_info_is_complete=*/TRUE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
    (void)add_immediate_pragma_kind_description
                 ((a_pragma_kind)pk_gcc_immediate,
                 fn_for_function(gcc_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/FALSE,
                 /*il_info_is_complete=*/TRUE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
  }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED
  if (gnu_version_is(any_version)) {
    (void)add_immediate_pragma_kind_description
                 (pk_gnu_riscv,
                 fn_for_function(gnu_riscv_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/FALSE,
                 /*il_info_is_complete=*/TRUE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
  }  /* if */
  if (clang_version_is(any_version)) {
    (void)add_immediate_pragma_kind_description
                 (pk_clang_riscv,
                 fn_for_function(clang_riscv_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/FALSE,
                 /*il_info_is_complete=*/TRUE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
  }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED && BUILTIN_FUNCTIONS_ENABLED */
#endif /* GNU_EXTENSIONS_ALLOWED */
  (void)add_immediate_pragma_kind_description
                ((a_pragma_kind)pk_diag_suppress,
                 fn_for_function(diag_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/ignore_diag_pragma_in_be,
                 /*il_info_is_complete=*/FALSE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
                ((a_pragma_kind)pk_diag_remark,
                 fn_for_function(diag_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/ignore_diag_pragma_in_be,
                 /*il_info_is_complete=*/FALSE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
                ((a_pragma_kind)pk_diag_warning,
                 fn_for_function(diag_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/ignore_diag_pragma_in_be,
                 /*il_info_is_complete=*/FALSE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
                ((a_pragma_kind)pk_diag_error,
                 fn_for_function(diag_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/ignore_diag_pragma_in_be,
                 /*il_info_is_complete=*/FALSE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
                ((a_pragma_kind)pk_diag_once,
                 fn_for_function(diag_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/ignore_diag_pragma_in_be,
                 /*il_info_is_complete=*/FALSE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
                ((a_pragma_kind)pk_diag_default,
                 fn_for_function(diag_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/ignore_diag_pragma_in_be,
                 /*il_info_is_complete=*/FALSE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
                ((a_pragma_kind)pk_diagnostic,
                 fn_for_function(diagnostic_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/ignore_diag_pragma_in_be,
                 /*il_info_is_complete=*/FALSE,
                 /*read_string_as_header_name=*/FALSE,
                 es_error);
#if INCLUDE_EDG_TEST_PRAGMAS
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_test_next_decl,
		 fn_for_function(test_next_construct_pragma),
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_next_construct_pragma_kind_description
 		((a_pragma_kind)pk_test_next_statement,
		 fn_for_function(test_next_construct_pragma),
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/FALSE,
		 /*may_bind_to_stmt=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_test_immediate,
                 fn_for_function(test_immediate_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_test_immediate_text,
                 (a_function_number)fn_null,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_test_immediate_pp_text,
                 (a_function_number)fn_null,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/TRUE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_other_pragma_kind_description
		((a_pragma_kind)pk_test_other,
                 (a_function_number)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
  (void)add_next_construct_pragma_kind_description
 		((a_pragma_kind)pk_test_bind_next_pass,
                 (a_function_number)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
#endif /* INCLUDE_EDG_TEST_PRAGMAS */
#if ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_checking_pragma,
                 (a_function_number)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_none);
#endif /* ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING */
#if DEBUG
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_db_opt,
                 fn_for_function(db_opt_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_db_name,
                 fn_for_function(db_name_pragma),
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
#endif /* DEBUG */
#if NEED_IL_DISPLAY
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_il_display,
		 fn_for_function(pragma_il_display),
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/TRUE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
#endif /* NEED_IL_DISPLAY */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  if (ms_extensions) {
    (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_if_exists,
                 fn_for_function(if_exists_pragma),
		 /*is_pseudo_pragma=*/TRUE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  }  /* if */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  if (ms_extensions || clang_mode || (gnu_mode && gnu_version >= 40403)) {
    (void)add_preproc_immediate_pragma_kind_description
		((a_pragma_kind)pk_push_macro,
                 fn_for_function(push_macro_pragma),
                 /*record_pragma_text=*/FALSE,
		 /*il_info_is_complete=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
		 /*ignore_in_back_end=*/TRUE,
		 /*allowed_in_pragma_operator=*/TRUE,
		 /*read_string_as_header_name=*/FALSE);
    (void)add_preproc_immediate_pragma_kind_description
		((a_pragma_kind)pk_pop_macro,
                 fn_for_function(pop_macro_pragma),
                 /*record_pragma_text=*/FALSE,
		 /*il_info_is_complete=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
		 /*ignore_in_back_end=*/TRUE,
		 /*allowed_in_pragma_operator=*/TRUE,
		 /*read_string_as_header_name=*/FALSE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions) {
    (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_start_map_region,
                 fn_for_function(microsoft_start_map_region_pragma),
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/TRUE,
                 es_warning);
    (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_stop_map_region,
                 fn_for_function(microsoft_stop_map_region_pragma),
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
    (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_setlocale,
                 fn_for_function(setlocale_pragma),
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/TRUE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
    /* In the following call, processing_C_code is set to TRUE so that
       concatenation of string literals will occur in the optional second
       argument. */
    (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_comment,
                 fn_for_function(microsoft_comment_pragma),
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/FALSE,
                 /*il_info_is_complete=*/TRUE,
                 /*read_string_as_header_name=*/FALSE,
                 es_warning);
    (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_conform,
                 fn_for_function(microsoft_conform_pragma),
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/FALSE,
                 /*il_info_is_complete=*/TRUE,
                 /*read_string_as_header_name=*/FALSE,
                 es_warning);
    (void)add_preproc_immediate_pragma_kind_description
                ((a_pragma_kind)pk_include_alias,
                 fn_for_function(microsoft_include_alias_pragma),
                 /*record_pragma_text=*/FALSE,
		 /*il_info_is_complete=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
		 /*ignore_in_back_end=*/TRUE,
		 /*allowed_in_pragma_operator=*/TRUE,
		 /*read_string_as_header_name=*/TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
  /* When unrecognized pragmas are being included in the IL, we need a
     pragma description that can be used for the unrecognized pragmas.
     The pragma description for pk_unrecognized is the one that will
     be used.

     This definition may be modified (except as noted below), but some
     description must be provided. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* When source sequence lists are being generated, unrecognized pragmas
     are treated as next-token pragmas.  The source sequence information can
     be used to output the pragmas in the right location (when the C++
     generating back end is used, for example).   Next-token pragmas are
     preferable to next-construct pragmas for representing unrecognized
     pragmas because next-construct pragmas are only valid in certain
     contexts.  The tokens of the pragma string will be macro-expanded when
     the C++-generating back end is used, because macro definitions are put
     out at the end of the generated code and thus cannot affect the
     #pragma directive, which is generated in its correct location. */
  (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_unrecognized,
                 (a_function_number)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
		 /*global=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,  /* Do not change. */
                 /*record_pragma_text=*/TRUE,         /* Do not change. */
                 /*expand_macros=*/BACK_END_IS_CP_GEN_BE, /* Do not change. */
                 /*processing_C_code=*/FALSE, /* Do not change. */
                 /*fetch_pp_tokens=*/TRUE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
  /* When source sequence lists are not being generated, unrecognized pragmas
     are treated as next-construct pragmas.  This allows the C generating
     back end to output the pragma along with the entity to which it is
     bound but also means that unrecognized pragmas may only appear in
     contexts in which next-construct pragmas are allowed (i.e., immediately
     before a declaration or statement).  Furthermore, if the associated
     entity is omitted (because it is unused) the pragma will also be
     omitted.  If you want the pragma to come out even if the associated
     entity is omitted, you can change the pragma to an "immediate" pragma.
     This will, however, have the side-effect that the pragma will no longer
     come out adjacent to the associated declaration.  All of the immediate
     pragmas in a given scope will come out together. */
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_unrecognized,
                 (a_function_number)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,  /* Do not change. */
                 /*record_pragma_text=*/TRUE,         /* Do not change. */
                 /*expand_macros=*/FALSE,		/* Do not change. */
                 /*processing_C_code=*/FALSE, /* Do not change. */
                 /*fetch_pp_tokens=*/TRUE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL */
  db_exit();
}  /* pragma_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

