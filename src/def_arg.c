/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

def_arg.c -- Processing of default arguments

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

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/* Previously allocated fixup entries available for reuse. */
STATIC_THREAD a_def_arg_expr_fixup_ptr
                avail_def_arg_expr_fixup;


#if DEBUG
/* GCOVR_EXCL_START */
/*
Counter to track use of memory.
*/
STATIC_THREAD unsigned long
		num_def_arg_expr_fixups_allocated;

unsigned long db_show_def_arg_expr_fixups_used(unsigned long grand_total)
/*
Display the amount of space used for allocation of default argument fixup
entries, for debugging purposes.  Also return the total amount.
*/
{
  unsigned long  num, size, total;

  db_space_used_lost("def arg expr fixups", avail_def_arg_expr_fixup,
                     num_def_arg_expr_fixups_allocated, a_def_arg_expr_fixup);
  return grand_total;
}  /* db_show_def_arg_expr_fixups_used */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

static a_def_arg_expr_fixup_ptr alloc_def_arg_expr_fixup(void)
/*
Allocate (or take from the available-list) a default arg expression fixup
entry and initialize it.
*/
{
  a_def_arg_expr_fixup_ptr  daefp;
  
  if (avail_def_arg_expr_fixup != NULL) {
    /* Reuse a previously allocated entity. */
    daefp = avail_def_arg_expr_fixup;
    avail_def_arg_expr_fixup = daefp->next;
  } else {
    /* Allocate memory for a new entity. */
    daefp = (a_def_arg_expr_fixup_ptr)alloc_fe(sizeof(a_def_arg_expr_fixup));
#if DEBUG
/* GCOVR_EXCL_START */
    num_def_arg_expr_fixups_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  /* Clear the entity. */
  daefp->next = NULL;
  daefp->param_type = NULL;
  new (&daefp->cache) a_template_cache();
  clear_template_cache(&daefp->cache);
  daefp->param_number = 0;

  return daefp;
}  /* alloc_def_arg_expr_fixup */


a_def_arg_expr_fixup_ptr copy_def_arg_expr_fixup_list(
				a_def_arg_expr_fixup_ptr	orig_list)
/*
Make a copy of the default argument fixup list specified by orig_list.
Return a pointer to the new list.
*/
{
  a_def_arg_expr_fixup_ptr	new_list = NULL;
  a_def_arg_expr_fixup_ptr	new_tail = NULL;
  a_def_arg_expr_fixup_ptr	daefp;
  a_def_arg_expr_fixup_ptr	new_daefp;

  for (daefp = orig_list; daefp != NULL; daefp = daefp->next) {
    new_daefp = alloc_def_arg_expr_fixup();
    *new_daefp = *daefp;
    new_daefp->next = NULL;
    if (new_list == NULL) new_list = new_daefp;
    if (new_tail != NULL) new_tail->next = new_daefp;
    new_tail = new_daefp;
  }  /* for */
  return new_list;
}  /* copy_def_arg_expr_fixup_list */


void free_def_arg_expr_fixup(a_def_arg_expr_fixup_ptr  daefp)
/*
Return a default arg expr fixup entry, and any others chained to it, to the
available-list.
*/
{
  if (daefp != NULL) {
    daefp->cache.~a_template_cache();
    free_def_arg_expr_fixup(daefp->next);
    daefp->next = avail_def_arg_expr_fixup;
    avail_def_arg_expr_fixup = daefp;
  }  /* if */
}  /* free_def_arg_expr_fixup */


void prescan_default_argument(a_token_cache_ptr  token_cache,
                              a_boolean          is_template_param,
                              a_boolean          is_function_template,
                              a_boolean          is_friend_decl,
            /* Defaulted: */  a_boolean          is_expression)
/*
Place the tokens for a default argument - an expression if is_expression is
TRUE (the default value) - into a token cache, to await actual processing
at a later point.  is_template_param is TRUE for the default argument for a
template parameter.  is_function_template is TRUE for the default argument
for a function template function parameter.  is_friend_decl is TRUE for a
default argument in a friend declaration; it must be FALSE when either
is_function_template or is_template_param are FALSE.
*/
{
  a_token_set_array        stop_tokens;
  a_token_sequence_number  first_tsn;
  a_token_sequence_number  last_tsn;
  a_scope_stack_entry_ptr  ssep;
  a_source_position        start_pos;
  a_cts_flag_set           cts_options;
  a_decl_parse_state       *dps;

  db_enter(3, "prescan_default_argument");
  /* Initialize a local stop token set. */
  clear_token_set_array(stop_tokens);
  /* Save the token sequence number of the first token to be cached. */
  first_tsn = curr_token_sequence_number;
  /* In the normal case we will scan the default argument and encounter a
     comma or right parenthesis.  If both of these are omitted, terminate
     the token stream when some likely delimiter is reached. */
  incr_token_set_array_element(stop_tokens, tok_comma);
  if (!is_template_param && !is_variadic_template_context()) {
    incr_token_set_array_element(stop_tokens, tok_ellipsis);
  }  /* if */
  incr_token_set_array_element(stop_tokens, tok_rparen);
  incr_token_set_array_element(stop_tokens, tok_semicolon);
  /* When scanning a template default argument add ">" to the stop tokens. */
  if (is_template_param) {
    incr_token_set_array_element(stop_tokens, tok_gt);
  }  /* if */
  /* The background caching mechanism is used.  Once the end of the default
     argument is found, the original (non-coalesced) tokens are extracted
     from the background cache.  The caller should have enabled the
     caching, but it is also done here to handle certain error cases. */
  start_pos = pos_curr_token;
  begin_caching_fetched_tokens(/*include_curr_token=*/TRUE);
  cts_options = CTS_COALESCE_IDS;
  if (is_template_param) {
    cts_options |= CTS_STOP_ON_STATEMENT_END;
  }  /* if */
  if (is_expression) {
    cts_options |= CTS_IS_EXPRESSION;
  }  /* if */
  cache_token_stream_full((a_token_cache_ptr)NULL, stop_tokens, cts_options);
  end_caching_fetched_tokens();
  if (is_template_param &&
      (curr_token == tok_semicolon || curr_token == tok_rbrace)) {
    /* We ended up at an unexpected place because of mismatched
       delimiters in the default argument.  A diagnostic is issued elsewhere
       for function default argument cases. */
    pos_error(ec_invalid_default_arg, &start_pos);
  }  /* if */
  /* Save the token sequence number of the last token of the default
     argument. */
  last_tsn = curr_token_sequence_number;
  /* Normally the last token of the cache (usually a comma, ")", or the ">"
     that ends the template parameter list) is not included.  But if we are
     rescanning the default argument from a cache and hit the
     tok_end_of_source terminator (which will not be in the cache),
     we want to include the last token that is in the cache. */
  copy_tokens_from_cache(curr_lexical_state_cache(), first_tsn, last_tsn,
                         /*include_last_token=*/
                                               curr_token == tok_end_of_source,
                         token_cache);
  ssep = &scope_stack[depth_scope_stack];
  dps = ssep->decl_parse_state;
  if (!is_template_param &&
      ((ssep->in_prototype_instantiation && !is_friend_decl) ||
       (is_function_template &&
        depth_innermost_instantiation_scope == NO_SCOPE_DEPTH)) &&
      !(dps != NULL && dps->variant.auto_params != NULL &&
        !dps->is_abbr_func_template)) {
    /* This is a function default argument within a prototype instantiation
       or in a template declaration.  Save the token numbers associated with
       this default argument so that it can be removed from the cache later.
       Do not do this if we just performed the first scan through a function
       declarator and encountered "auto" parameters: Instead we will allocate
       the template cache segment on the second parse (when
       dps->is_abbr_func_template will be TRUE). */
    a_template_cache_segment_ptr  tcsp;
    /* Unless there is no default, the cache actually contains the token after
       the last one of the default argument, but that token should not be used
       as the last token of the cache segment. */
    if (last_tsn != first_tsn) last_tsn--;
    tcsp = get_template_cache_segment(
                   (a_symbol_ptr)NULL, (a_template_symbol_supplement_ptr)NULL,
                   first_tsn, last_tsn);
    tcsp->is_default_arg = TRUE;
    /* Check for the case where the cache is empty. */
    tcsp->expression_missing = token_cache->is_empty();
  }  /* if */
  /* Note that the terminating token (comma, rparen, etc.) is not added to
     the cache. */
  terminate_token_cache(token_cache);
  db_exit();
}  /* prescan_default_argument */


void prescan_default_function_arg_expr(
			a_param_type_ptr		ptp,
		        a_def_arg_expr_fixup_ptr	*list,
			a_boolean			is_function_template,
			a_boolean			is_friend_decl,
			unsigned long			param_number)
/*
Place the tokens for a default argument expression into a token cache, to
await actual processing at a later point.  Link the default argument
entry onto the list provided by the caller.  param_number specifies the
position of the parameter in the parameter list.  If list or ptp is NULL,
scan the default argument expression but discard the token cache.
*/
{
  a_def_arg_expr_fixup_ptr new_daefp, daefp;
  a_scanning_token_cache   token_cache(/*is_reusable=*/TRUE);

  db_enter(3, "prescan_default_function_arg_expr");
  /* Scan the default argument expression. */
  prescan_default_argument(token_cache.ptr(), /*is_template_param=*/FALSE,
                           is_function_template, is_friend_decl);
  if (list != NULL && ptp != NULL) {
    /* Allocate a default arg expr fixup entry. */
    new_daefp = alloc_def_arg_expr_fixup();
    new_daefp->param_type = ptp;
    new_daefp->cache.tokens = shared_obj<a_token_cache>(*token_cache);
    new_daefp->param_number = param_number;
    /* Add the entry to the end of the list of default arg expr fixup entries
       for the current routine fixup. */
    if (*list == NULL) {
      *list = new_daefp;
    } else {
      daefp = *list;
      while (daefp->next != NULL) daefp = daefp->next;
      daefp->next = new_daefp;
    }  /* if */
    if (is_function_template) {
      /* Indicate that this default argument is a template default argument
         whose expression has not yet been evaluated. */
      ptp->has_unevaluated_template_default = TRUE;
      ptp->orig_param_type_for_unevaluated_default_arg_expr = ptp;
    }  /* if */
  }  /* if */
  db_exit();
}  /* prescan_default_function_arg_expr */


void delayed_scan_of_default_arg_expr(a_param_type_ptr        param_type_entry,
                                      ARG_UNUSED a_symbol_ptr rout_sym,
                                      a_boolean               check_for_errors)
/*
Do the delayed scan of the default argument expression for a parameter.  The
cache has just been reactivated, so curr_token should represent the first
token in the cache.  If check_for_errors is TRUE, then before doing the scan
check that default expressions have been declared for all successor arguments.

The error checks are suppressed when this routine is called for a
function template because the checks are done elsewhere (and cannot
be done here because the default arguments for a given function template
may be spread between several declarations).

param_type_entry describes the parameter of the given routine (rout_sym) with
which the default argument is associated.
*/
{
  a_param_type_ptr  ptp;
  a_boolean         err = FALSE, for_consteval_func = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_boolean         discard_default_arg = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_enter(3, "delayed_scan_of_default_arg_expr");
  if (param_type_entry->default_arg_expr != NULL &&
      !is_error_node(param_type_entry->default_arg_expr)) {
    pos_error(ec_default_arg_already_defined, &pos_curr_token);
  }  /* if */
  if (check_for_errors) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Verify that this is not a function with a C++/CLI param array. */
    if (cppcli_enabled && is_cli_param_array_routine_symbol(rout_sym)) {
      /* Default arguments are not allowed with C++/CLI parameter arrays. */
      pos_error(ec_default_arg_used_in_param_array_function, &pos_curr_token);
      err = TRUE;
      discard_default_arg = TRUE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Make a pass over all the param type entries that follow the current one.
       It is an error if there are any without a default argument. */
   for (ptp = param_type_entry->next; ptp != NULL; ptp = ptp->next) {
     if (!ptp->has_default_arg) {
        /* Issue an error on the first successor in the parameter list that
           does not have a default argument. */
        if (!err) {
          pos_error(ec_default_arg_not_at_end, &pos_curr_token);
          err = TRUE;
        }  /* if */
        ptp->has_default_arg = TRUE;
        ptp->default_arg_appeared_in_class_definition =
                    param_type_entry->default_arg_appeared_in_class_definition;
        ptp->default_arg_expr = error_node();
      }  /* if */
    }  /* for */
  }  /* if */
  /* We scan the expression whether an error was detected or not. */
  if (is_simple_function_symbol(rout_sym) &&
      func_sym_routine(rout_sym)->is_consteval) {
    for_consteval_func = TRUE;
  }  /* if */
  scan_default_arg_expr(param_type_entry, /*is_member_or_friend=*/TRUE,
                        for_consteval_func);
  set_parent_entity_for_closure_types(
          param_type_entry->entities_defined_in_default_arg,
          is_simple_function_symbol(rout_sym) ? rout_sym : (a_symbol_ptr)NULL,
          param_type_entry->default_arg_appeared_in_class_definition);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (discard_default_arg) {
    /* Treat the parameter as not having a default argument for error recovery
       purposes. */
    check_assertion(err);
    param_type_entry->has_default_arg = FALSE;
    param_type_entry->default_arg_appeared_in_class_definition = FALSE;
    param_type_entry->default_arg_expr = NULL;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* In the normal case the current token should be end_of_source,
     which was inserted to mark the end of the cached token
     stream. */
  if (curr_token != tok_end_of_source) {
    pos_error(ec_unexpected_end_of_default_arg, &pos_curr_token);
    /* If necessary, keep flushing until end-of-source is found. */
    while (curr_token != tok_end_of_source) (void)get_token();
  }  /* if */
  /* Advance past the end-of-source token, which was added in
     the prescan routine. */
  (void)get_token();
  db_exit();
}  /* delayed_scan_of_default_arg_expr */


static void check_for_valid_end_of_template_def_arg(void)
/*
We have just scanned the default for a template argument.  We
should now be at the tok_end_of_source that terminates the cache.
*/
{
  /* In the normal case the current token should be end_of_source,
     which was inserted to mark the end of the cached token
     stream. */
  if (curr_token != tok_end_of_source) {
    pos_error(ec_exp_comma, &pos_curr_token);
    /* If necessary, keep flushing until end-of-source is found. */
    while (curr_token != tok_end_of_source) (void)get_token();
  }  /* if */
  /* Advance past the end-of-source token, which was added in
     the prescan routine. */
  (void)get_token();
}  /* check_for_valid_end_of_template_def_arg */


void delayed_scan_of_template_default_arg_expr(a_type_ptr	type,
					       a_constant_ptr   constant)
/*
Do the delayed scan of the default argument expression for a template
parameter.  The cache has just been reactivated, so curr_token should
represent the first token in the cache.
*/
{
  db_enter(3, "delayed_scan_of_template_default_arg_expr");
  scan_template_argument_constant_expression(type, constant);
  check_for_valid_end_of_template_def_arg();
  db_exit();
}  /* delayed_scan_of_template_default_arg_expr */


a_type_ptr delayed_scan_of_template_default_type_arg(void)
/*
Do the delayed scan of the default argument expression for a template
type parameter.  The cache has just been reactivated, so curr_token should
represent the first token in the cache.  Return a pointer to the type
that was scanned.
*/
{
  a_type_ptr	tp = NULL;

  db_enter(3, "delayed_scan_of_template_default_type_arg");
  tp = scan_template_type_argument((a_boolean*)NULL, /*is_default_arg=*/TRUE);
  check_for_valid_end_of_template_def_arg();
  db_exit();
  return tp;
}  /* delayed_scan_of_template_default_type_arg */


a_template_ptr delayed_scan_of_template_default_template_arg(
				a_template_ptr		param_template,
				a_boolean		dependent_default,
				a_source_position	*err_pos)
/*
Do the delayed scan of the default argument expression for a template
template parameter.  The cache has just been reactivated, so curr_token should
represent the first token in the cache.  Return a pointer to the template
that was scanned.  dependent_default is TRUE if the template parameter
list of the template parameter involves other template parameters, in
which case checking the template parameter types of the default argument
must be deferred until it is used.
*/
{
  a_template_ptr	templ;

  db_enter(3, "delayed_scan_of_template_default_template_arg");
  templ = scan_template_template_argument(param_template, err_pos,
                                          /*is_default=*/TRUE,
                                          dependent_default);
  check_for_valid_end_of_template_def_arg();
  db_exit();
  return templ;
}  /* delayed_scan_of_template_default_template_arg */


void def_arg_one_time_init(void)
/*
One-time initialization for def_arg.c static variables.
*/
{
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_def_arg_expr_fixup),
#if DEBUG
/* GCOVR_EXCL_START */
      pch_saved_var_array_elem(num_def_arg_expr_fixups_allocated),
/* GCOVR_EXCL_STOP */
#endif /* if DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* def_arg_one_time_init */


void def_arg_init(void)
/*
Initializations for class declaration processing.
*/
{
  /* Initialize the list of freed delayed-scan-fixup entries. */
  avail_def_arg_expr_fixup = NULL;
#if DEBUG
/* GCOVR_EXCL_START */
  num_def_arg_expr_fixups_allocated = 0;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return;
}  /* def_arg_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

