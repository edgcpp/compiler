/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

disambig.c -- Disambiguation of C++ declarations and expressions.

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
#include "disambig.h"
#include "decls.h"
#include "expr.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

typedef struct a_disambig_state *a_disambig_state_ptr;
typedef struct a_disambig_state {
  a_type_ptr	decl_class_type;
			/* In certain modes, this is set to the class type
			   of the declarator that is found. */
  a_boolean	check_if_is_decl;
			/* TRUE if this is the normal C++ declaration vs.
			   expression disambiguation, FALSE if this is
			   some other kind of prescan. */
  a_boolean	may_be_decl;
			/* TRUE while the statement being scanned could still
			   be a declaration. */
  a_boolean	terminate;
			/* TRUE if the disambiguation should be stopped at
			   this point. */
  a_boolean	set_decl_class_type;
			/* TRUE if the decl_class_type field should be set,
			   and scanning stopped once the declarator is
                           found. */
  a_boolean	friend_encountered;
			/* TRUE if a tok_friend token was found among the
			   decl-specifiers. */
  a_boolean	variadic_prototype_instantiation;
			/* TRUE if we are in a template dependent context of
			   a variadic template. */
  a_boolean	cache_tokens;
			/* TRUE if the tokens fetched for disambiguation
			   should be cached. */
  a_boolean	saved_in_disambiguation;
			/* The value of the scope stack in_disambiguation
			   flag at the start of disambiguation. */
  a_boolean	saved_source_sequence_entries_disallowed;
			/* The value of the scope stack
			   source_sequence_entries_disallowed flag at the
			   start of disambiguation. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_boolean	find_static_specifier_only;
			/* TRUE if we are only scanning for the presence of a
			   "static" specifier. */
  a_boolean	static_specifier_seen;
			/* TRUE if the "static" keyword was seen as a
			   declaration specifier. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_boolean	suppress_packs;
			/* TRUE if the pack processing should be suppressed
			   in the current context. */
  a_token_sequence_number
		first_tsn;
			/* The value of curr_token_sequence_number at the
			   start of disambiguation. */
  a_pack_expansion_stack_entry_ptr
		pack_expansion_stack_entry;
			/* If variadic_prototype_instantiation is TRUE,
			   this is the pack suppression entry pushed. */
} a_disambig_state;


static void init_disambig_state(a_disambig_state_ptr	dsp,
				a_boolean		check_if_is_decl,
				a_boolean		suppress_packs,
				a_boolean		cache_tokens)
/*
Initialize a disambiguation state block.  check_if_is_decl is TRUE if
this is the normal C++ declaration/expression disambiguation and FALSE
if this is being called for some other kind of prescan.  If suppress_packs
is TRUE and we are in the prototype instantiation of a variadic template,
push a pack expansion suppression.  If cache_tokens is TRUE, a token
cache of the tokens fetched for disambiguation should be created.
*/
{
  dsp->check_if_is_decl = check_if_is_decl;
  dsp->decl_class_type = NULL;
  dsp->may_be_decl = TRUE;
  dsp->terminate = FALSE;
  dsp->set_decl_class_type = FALSE;
  dsp->friend_encountered = FALSE;
  /* dsp->variadic_prototype_instantiation set below. */
  dsp->cache_tokens = cache_tokens;
  /* dsp->saved_in_disambiguation set below. */
  /* dsp->saved_source_sequence_entries_disallowed set below. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  dsp->find_static_specifier_only = FALSE;
  dsp->static_specifier_seen = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  dsp->suppress_packs = suppress_packs;
  dsp->first_tsn = curr_token_sequence_number;
  /* dsp->pack_expansion_stack_entry set below. */
  if (cache_tokens) {
    begin_caching_fetched_tokens(/*include_curr_token=*/TRUE);
  }  /* if */
  /* Indicate that we are in a prescan context. */
  begin_prescan_context(suppress_packs, &dsp->variadic_prototype_instantiation,
                        &dsp->pack_expansion_stack_entry,
                        &dsp->saved_in_disambiguation,
                        &dsp->saved_source_sequence_entries_disallowed);
}  /* init_disambig_state */


static void wrapup_disambig_state(a_disambig_state_ptr dsp)
/*
Perform any operations that must be done to clean up after disambiguation.
*/
{
  /* If we are accumulating cached tokens, extract the tokens and rescan
     them.  The test of dsp->first_tsn is used to avoid doing this when the
     only token put in the cache was the current token at the start of
     caching.  An exception is made when the current token is tok_end_of_source
     because the last_tsn_in_cache may not have been updated in that case. */
  if (dsp->cache_tokens) {
    end_caching_fetched_tokens();
    if (curr_lexical_state_stack_entry->last_tsn_in_cache != dsp->first_tsn ||
        curr_token == tok_end_of_source) {
      a_scanning_token_cache cache;

      /* Get the tokens that were fetched by this routine from the cache
         that has been accumulated and rescan them. */
      copy_tokens_from_cache(curr_lexical_state_cache(), dsp->first_tsn,
                             last_token_sequence_number_of_token,
                             /*include_last_token=*/TRUE, cache.ptr());
      rescan_cached_tokens(
                       cache.ptr(),
                       /*discard_curr_token=*/curr_token != tok_end_of_source);
    }   /* if */
  }  /* if */
  /* Mark the end of the prescan context. */
  end_prescan_context(dsp->variadic_prototype_instantiation,
                      dsp->pack_expansion_stack_entry,
                      dsp->saved_in_disambiguation,
                      dsp->saved_source_sequence_entries_disallowed);
}  /* wrapup_disambig_state */


/*
Macro that is TRUE if the current token (which must be an identifier or
the "::" at the start of a qualified name) is a type name.
*/
#define prescan_curr_id_is_type_name(flags)				\
  (curr_type_symbol(/*is_new_type_name=*/FALSE, /*in_prescan=*/TRUE,	\
                    /*in_type_check=*/FALSE,				\
                    ((flags) & DFS_IMPLICIT_TYPENAME_CONTEXT) != 0 &&	\
                      relaxed_typename_enabled,				\
                    /*is_sizeof_context=*/FALSE,			\
                    concepts_enabled) != NULL)

/*
Macros to test bits in a disambiguation flag set.
*/
#define real_declarator_allowed(flags)					\
  (((flags) & DFS_REAL_DECLARATOR_ALLOWED) != 0)

#define abstract_declarator_allowed(flags)				\
  (((flags) & DFS_ABSTRACT_DECLARATOR_ALLOWED) != 0)

#define declarator_only_check(flags)				\
  (((flags) & ~(DFS_REAL_DECLARATOR_ALLOWED |				\
                DFS_ABSTRACT_DECLARATOR_ALLOWED)) == 0)

#define single_type_required(flags)					\
  (((flags) & DFS_SINGLE_TYPE_REQUIRED) != 0)

#define is_condition(flags)						\
  (((flags) & DFS_IS_CONDITION) != 0)

#define condition_is_for_stmt(flags)					\
  (((flags) & DFS_CONDITION_IS_FOR_STMT) != 0)

#define is_cast(flags)							\
  (((flags) & DFS_IS_CAST) != 0)

#define is_template_decl(flags)					\
  (((flags) & DFS_IS_TEMPLATE_DECL) != 0)

#define is_template_argument(flags)					\
  (((flags) & DFS_IS_TEMPLATE_ARGUMENT) != 0)


/*
Macro that is TRUE if the disambiguation process should stop at this point.
*/
#define terminate_disambiguation(state)					\
  (!(state)->may_be_decl || ((state)->terminate))


/*
Macro that returns GID_USE_PROTOTYPE_NOT_NONREAL if is_template_decl is TRUE
and if gid_flags does not include GID_IS_TYPENAME.
*/
/*lint -emacro(835 506,gid_flags_for_template)*/
#define gid_flags_for_template(flags, gid_flags)			\
  (is_template_decl(flags) && (((gid_flags) & GID_IS_TYPENAME) == 0) ?  \
                               GID_USE_PROTOTYPE_NOT_NONREAL : GID_NO_OPTIONS)



static void cache_tokens_until(a_token_kind		stop_token,
			       a_boolean		coalesce)
/*
Wrapper for cache_token_stream that saves and restores the stop tokens
array.  Cache tokens until the specified token is found.  If coalesce is
TRUE, coalesce any identifiers.
*/
{
  a_token_set_array  stop_token_array;
  a_cts_flag_set     options = CTS_IS_EXPRESSION;

  if (coalesce) {
    options |= CTS_COALESCE_IDS;
  }  /* if */
  clear_token_set_array(stop_token_array);
  incr_token_set_array_element(stop_token_array, stop_token);
  /* Also stop on a right brace or semicolon to keep from caching too far
     in error cases. */
  incr_token_set_array_element(stop_token_array, tok_rbrace);
  incr_token_set_array_element(stop_token_array, tok_semicolon);
  cache_token_stream_full((a_token_cache_ptr)NULL, stop_token_array, options);
}  /* cache_tokens_until */


static void prescan_initializer(void)
/*
Cache the tokens that comprise an initializer of the form
"= initializer-clause".
*/
{
  a_token_set_array  stop_token_array;

  clear_token_set_array(stop_token_array);
  incr_token_set_array_element(stop_token_array, tok_comma);
  incr_token_set_array_element(stop_token_array, tok_semicolon);
  incr_token_set_array_element(stop_token_array, tok_rparen);
  cache_token_stream_coalesce_identifiers((a_token_cache_ptr)NULL,
                                          stop_token_array);
}  /* prescan_initializer */


static void prescan_init_list(void)
/*
Cache the tokens that comprise a brace enclosed initializer list.
*/
{
  (void)cache_token_stream_until_matching_token((a_token_cache_ptr)NULL,
                                                CTS_NO_OPTIONS);
  if (curr_token == tok_rbrace) (void)get_token();
}  /* prescan_init_list */


static void f_get_token_and_coalesce_if_identifier(
				a_disambig_flag_set		flags,
				an_identifier_options_set	gid_flags)
/*
Get a token and, if it is a tok_identifier, call
is_generalized_identifier_start to coalesce it in case it is the
beginning of something like a qualified name.  This is used by the
disambiguation routines to ensure that any identifiers that are
scanned are coalesced prior to analysis.
*/
{
  (void)get_token();
  (void)is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL |
                                        GID_IS_EXPR_CONTEXT |
                                        gid_flags_for_template(flags,
                                                               gid_flags) |
                                        gid_flags);
}  /* f_get_token_and_coalesce_if_identifier */


/*
Macro that calls f_get_token_and_coalesce_if_identifier and provides
the general identifier option.
*/
#define get_token_and_coalesce_if_identifier(flags)			\
  f_get_token_and_coalesce_if_identifier((flags), GID_NO_OPTIONS)

#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED

static void prescan_until_closing_paren(a_disambig_flag_set   flags)
/*
Get tokens, and optionally cache them, until we encounter an unmatched
right parenthesis (that matches a left parenthesis that we have already
scanned).
*/
{
  int	paren_count = 0;

  for (;;) {
    get_token_and_coalesce_if_identifier(flags);
    if (curr_token == tok_rparen) {
      /* A right parenthesis.  Break out if this is a zero level
         parenthesis. */
      if (paren_count == 0) break;
      paren_count--;
    } else if (curr_token == tok_lparen) {
      paren_count++;   
    } else if (curr_token == tok_semicolon ||
               curr_token == tok_end_of_source ||
               curr_token == tok_lbrace) {
      break;
    }  /* if */
  }  /* for */
}  /* prescan_until_closing_paren */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED GNU_EXTENSIONS_ALLOWED */


#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED

static void prescan_extended_decl_modifiers(a_disambig_flag_set   flags)
/*
For Microsoft compatibility, prescan the following Microsoft modifiers:

	__declspec ( extended-decl-modifier-seq )
	__near
	__far
	__single_inheritance
	__multiple_inheritance
	__virtual_inheritance

or for near/far support prescan "near" or "far".   When this routine is
called, the current token must be the initial keyword.
*/
{
  for (;;) {
#if NEAR_AND_FAR_ALLOWED
    if (is_near_or_far()) {
      get_token_and_coalesce_if_identifier(flags);
      continue;
#if !MICROSOFT_EXTENSIONS_ALLOWED
    } else {
      /* When (other) Microsoft extensions are not allowed, exit the
         loop when the token is not a near/far keyword. */
      break;
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (curr_token == tok_declspec) {
      /* Bypass the __declspec token. */
      (void)get_token();
      if (curr_token == tok_lparen) {
        /* The syntax within the parentheses of the __declspec specifier is
           not tested by this routine, except that the parentheses are expected
           to be properly nested (the same number of opening and closing
           parentheses), and to improve error recovery, it is not expected to
           include a semicolon, left brace, or end-of-source token. */
        prescan_until_closing_paren(flags);
        if (curr_token == tok_rparen) {
          get_token_and_coalesce_if_identifier(flags);
        }  /* if */
      }  /* if */
    } else {
      /* If this is a class declaration and the next token is an identifier,
         it is probably the class name.  But it might also be the "inheritance
         kind" (i.e., __single_inheritance, __multiple_inheritance, or
         __virtual_inheritance).  Single-underscore versions of the keywords
         are also allowed. */
      a_boolean	done = TRUE;
      if (curr_token == tok_identifier &&
          locator_for_curr_id.symbol_header != NULL) {
        a_const_char *name = locator_for_curr_id.symbol_header->identifier;
        if (*(name++) == '_') {
          if (*name == '_') name++;
          /* Check the name without its leading single or double underscore. */
          if (strcmp(name, "single_inheritance") == 0 ||
              strcmp(name, "multiple_inheritance") == 0 ||
              strcmp(name, "virtual_inheritance") == 0) {
            get_token_and_coalesce_if_identifier(flags);
            done = FALSE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (done) break;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* for */
}  /* prescan_extended_decl_modifiers */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */

static void prescan_std_attribute(a_disambig_flag_set   flags)
/*
Prescan a C++11 or C23 attribute list of the form:

  [[ attribute-list [opt] ]]

When this routine is called, the first left bracket is the current token.
This routine does not actually enforce the syntax of the elements of the
attribute list.  It simply requires that the brackets be properly nested
(the same number of left and right brackets).
*/
{
  check_assertion(curr_token == tok_lbracket);
  /* Bypass the first left parenthesis. */
  (void)get_token();
  if (curr_token == tok_lbracket) {
    int  bracket_count = 0;
    /* Bypass the second left bracket. */
    (void)get_token();
    /* Look for the closing bracket of the attribute. */
    for (;;) {
      (void)get_token();
      if (curr_token == tok_rbracket) {
        /* A right bracket.  Break out if this is a zero-level bracket. */
        if (bracket_count == 0) break;
        bracket_count--;
      } else if (curr_token == tok_lbracket) {
        bracket_count++;   
      } else if (curr_token == tok_end_of_source) {
        break;
      }  /* if */
    }  /* for */
    /* We should now be at the closing "]]" of the attribute. */
    if (curr_token == tok_rbracket) {
      get_token_and_coalesce_if_identifier(flags);
      if (curr_token == tok_rbracket) {
        get_token_and_coalesce_if_identifier(flags);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* prescan_std_attribute */

#if GNU_EXTENSIONS_ALLOWED

static void prescan_gnu_attribute(a_disambig_flag_set   flags)
/*
Prescan a GNU attribute list of the form:

  __attribute__ (( attribute-list [opt] ))

When this routine is called, the "__attribute__" token is the current token.
This routine does not actually enforce the syntax of the elements of the
attribute list.  It simply requires that the parentheses be properly nested
(the same number of left and right parentheses).
*/
{
  check_assertion(curr_token == tok_attribute);
  /* Bypass the attribute token. */
  (void)get_token();
  if (curr_token == tok_lparen) {
    /* Bypass the first left parenthesis. */
    (void)get_token();
    if (curr_token == tok_lparen) {
      /* Bypass the second left parenthesis. */
      (void)get_token();
      /* Look for the closing parenthesis of the attribute. */
      prescan_until_closing_paren(flags);
      /* We should now be at the closing "))" of the attribute. */
      if (curr_token == tok_rparen) {
        get_token_and_coalesce_if_identifier(flags);
      }  /* if */
    }  /* if */
    if (curr_token == tok_rparen) {
      get_token_and_coalesce_if_identifier(flags);
    }  /* if */
  }  /* if */
}  /* prescan_gnu_attribute */

#endif /* GNU_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void prescan_based_modifier(a_disambig_flag_set 	      flags)
/*
Prescan the Microsoft __based modifier:

	__based ( identifier )

When this routine is called, the current token must be the __based
keyword.
*/
{
  check_assertion_str2(curr_token == tok_based,
                       "prescan_based_modifier:",
                       "curr_token not tok_based");
  /* Bypass the __based token. */
  (void)get_token();
  if (curr_token == tok_lparen) {
    /* Get the next token, which should be an identifier. */
    get_token_and_coalesce_if_identifier(flags);
    if (curr_token == tok_identifier) {
      /* Get the next token, which should be a right paren. */
      get_token_and_coalesce_if_identifier(flags);
      if (curr_token == tok_rparen) {
        /* Bypass the right paren. */
        get_token_and_coalesce_if_identifier(flags);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* prescan_based_modifier */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void prescan_any_prefix_bracketed_attributes(
                                                  a_disambig_flag_set   flags)
/*
This routine is called at the start of a declaration (possibly a parameter
declaration).  If the current token is a left bracket introducing Microsoft or
C++11 attributes (i.e., not a lambda), scan over them.
*/
{
  while (curr_token == tok_lbracket && !C_mode()) {
    a_boolean  attr_next = std_attribute_tokens_next();
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (!attr_next && ms_extensions && !is_lambda()) {
      attr_next = TRUE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* If this is not the start of an attribute, exit the loop. */
    if (!attr_next) break;
    /* This appears to be a left bracket introducing Microsoft or C++11
       attribute. */
    /* Advance past the left bracket. */
    (void)get_token();
    /* Now scan up to the matching right bracket. */
    cache_tokens_until(tok_rbracket, /*coalesce=*/FALSE);
    /* Advance past the right bracket. */
    get_token_and_coalesce_if_identifier(flags);
  }  /* while */
}  /* prescan_any_prefix_bracketed_attributes */


static a_boolean is_token_allowed_after_typeof(a_token_kind token)
/*
Return TRUE if token can follow a typeof of the form "typeof(expression)".
*/
{
  a_boolean	result = FALSE;

  switch (token) {
    case tok_plus_plus:
    case tok_minus_minus:
    case tok_lbracket:
    case tok_period:
    case tok_arrow:
    case tok_lparen:
      result = TRUE;
      break;
    default:
      break;
  }  /* switch */
  return result;
}  /* is_token_allowed_after_typeof */


static void prescan_type_operator(a_disambig_state_ptr       state,
				  a_disambig_flag_set        flags)
/*
Scan past (and cache) a decltype, alignas, __underlying_type, typeof,
__edg_type__, __edg_vector_type__, __edg_neon_vector_type__,
__edg_neon_polyvector_type__, or __edg_scalable_vector_type__ specifier.
(alignas isn't strictly a type operator, but it is syntactically similar.)
*/
{
  a_boolean	is_typeof = curr_token == tok_typeof;
  /* Bypass the leading token (decltype, alignas, __underlying_type, etc.). */
  (void)get_token();
  if (curr_token == tok_lparen) {
    /* Advance past the left paren. */
    get_token_and_coalesce_if_identifier(flags);
    if (is_typeof && gpp_mode && gnu_version >= 30400 &&
        !is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                          DFS_REAL_DECLARATOR_ALLOWED)) {
      /* This is a g++ typeof of the form "typeof (expression)".  Check for
         a continuation of the expression after the ")". */
      cache_tokens_until(tok_rparen, /*coalesce=*/TRUE);
      if (is_token_allowed_after_typeof(next_token())) {
        /* There are more tokens that are part of the expression.  We can't
           prescan an arbitrary expression, so cut off the disambiguation
           here and conclude that this is a declaration. */
        state->may_be_decl = TRUE;
        state->terminate = TRUE;
        /* We can't terminate when looking for the declarator class type. */
        check_assertion(!state->set_decl_class_type);
      }  /* if */
    } else {
      /* The type operator argument is a type name. */
      cache_tokens_until(tok_rparen, /*coalesce=*/TRUE);
    }  /* if */
  }  /* if */
}  /* prescan_type_operator */


static a_boolean is_ctor_dtor_or_finalizer(void)
/*
Return TRUE if the current locator is for a constructor, destructor, or
finalizer definition.  The locator must refer to a qualified name.  
*/
{
  a_boolean	result = FALSE;
  a_boolean	err;

  if (locator_for_curr_id.is_qualified_name) {
    if (locator_for_curr_id.is_destructor_name) {
      /* This is a destructor name. */
      result = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cli_or_cx_enabled && locator_for_curr_id.is_finalizer_name) {
      /* This is a finalizer name. */
      result = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Do not insert code here. */
    } else {
      /* To see if this is a constructor, look up the identifier using
         a tentative type lookup (so that no error will be issued if the
         name is not found).  Then look at the resulting symbol. */
      a_symbol_ptr	sym;
      (void)coalesce_and_lookup_qualified_name(GID_TEMPLATE_ARGS_OPTIONAL,
                                               ilm_tentative_type,
                                               &err);
      sym = locator_for_curr_id.specific_symbol;
      if (sym != NULL) {
        if (is_constructor_symbol(sym)) {
          result = TRUE;
        } else if (gpp_mode && is_injected_class_symbol(sym) &&
                   locator_for_curr_id.is_qualified_name &&
                   locator_for_curr_id.is_class_member &&
                   same_entities(qualifier_class_type(locator_for_curr_id),
                                 sym_parent_class(sym))) {
          /* Qualified lookup is done differently in g++ mode with respect
             to constructors vs. injected class names.  Consider this to be
             a constructor if we get the injected class name back. */
          result = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_ctor_dtor_or_finalizer */


static void prescan_decl_specifiers(a_disambig_state_ptr       state,
                                    a_disambig_flag_set        flags)
/*
Scan and cache the tokens that comprise a list of decl_specifiers.
*/
{
  a_boolean     is_decl_specifier_token = TRUE;
  a_boolean     any_decl_specifiers = FALSE;
  a_boolean     type_specifier_seen = FALSE;
  a_boolean     is_ctor_dtor_or_finalizer_name = FALSE;
  a_boolean     is_typename = FALSE;
  a_symbol_ptr  sym;

  /* Disambiguation code should never be called in C mode.  (Otherwise, we
     would have to add things like tok_c99_bool to the cases below.) */
  check_assertion(!C_mode());
  for (;;) {
    a_boolean	next_token_fetched = FALSE;
    switch (curr_token) {
      /* "auto" is sometimes a storage class and sometimes a type specifier. */
      case tok_auto:
        if (auto_type_specifier_enabled) type_specifier_seen = TRUE;
        break;
      case tok_c11_atomic:
        get_token_and_coalesce_if_identifier(flags);
        if (curr_token == tok_lparen) {
          get_token_and_coalesce_if_identifier(flags);
          cache_tokens_until(tok_rparen, /*coalesce=*/TRUE);
          type_specifier_seen = TRUE;
        } else {
          next_token_fetched = TRUE;
        }  /* if */
        break;
      /* Storage class specifiers. */
      case tok_static:
#if MICROSOFT_EXTENSIONS_ALLOWED
        state->static_specifier_seen = TRUE;
        if (state->find_static_specifier_only) goto done;
        FALLTHROUGH
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case tok_register:
      case tok_extern:
      case tok_mutable:
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
      /* GNU/Sun __thread storage class. */
      case tok_thread:
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
      case tok_thread_local:
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* The Microsoft __inline and __forceinline keywords are treated as
         storage classes. */
      case tok_microsoft_inline:
      case tok_forceinline:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
      /* Sun linkage scope specifiers are treated as storage classes. */
      case tok_global_link_scope:
      case tok_symbolic_link_scope:
      case tok_hidden_link_scope:
#endif /* SUN_EXTENSIONS_ALLOWED */
      /* Function specifiers. */
      case tok_inline:
      case tok_virtual:
      case tok_explicit:
      /* Other specifiers. */
      case tok_constexpr:
      case tok_consteval:
      case tok_constinit:
        break;
      /* Friend and typedef. */
      case tok_friend:
        state->friend_encountered = TRUE;
        break;
      case tok_typedef:
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_declspec:
        prescan_extended_decl_modifiers(flags);
        next_token_fetched = TRUE;
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
      case tok_attribute:
        /* A GNU __attribute__. */
        prescan_gnu_attribute(flags);
        next_token_fetched = TRUE;
        break;
#endif /* GNU_EXTENSIONS_ALLOWED */
      /* Type specifier - identifier that may be a simple type name.
         If we haven't yet seen a type specifier, then this identifier,
         if it is a type, is the type specifier.  Otherwise, this is
         probably the start of the declarator, so we stop scanning
         decl-specifiers. */
      case tok_identifier:
        if (locator_for_curr_id.symbol_header != NULL &&
            locator_for_curr_id.symbol_header->has_intrinsic_name &&
            check_type_transform_name() != tok_error) {
          goto type_operator_case;
        }  /* if */
        sym = locator_for_curr_id.specific_symbol;
        /* If we are scanning a template declaration, see if this is a
           constructor or destructor declaration.  This is not done in other
           cases because the other disambiguation contexts are not in a
           namespace scope, and constructor and destructor declarations are
           not permitted. */
        is_ctor_dtor_or_finalizer_name = is_template_decl(flags) &&
                                         is_ctor_dtor_or_finalizer();
        if (!type_specifier_seen &&
            !is_ctor_dtor_or_finalizer_name &&
            (prescan_curr_id_is_type_name(flags) ||
             (sym != NULL && sym->kind == (a_symbol_kind)sk_class_template))) {
          /* A class template will probably result in a "missing template
             argument list" error later.  Consider it as a type name for now
             though. */
          type_specifier_seen = TRUE;
        } else {
          is_decl_specifier_token = FALSE;
        }  /* if */
        if (sym != NULL && !is_template_class_symbol(sym)) {
          /* Clear the locator field so that the lookup done by
             curr_id_is_type_name will not be used when the statement
             is actually parsed later. */
          clear_specific_symbol(locator_for_curr_id);
        }  /* if */
        if (pack_indexing_allowed && is_decl_specifier_token &&
            type_specifier_seen && !locator_for_curr_id.is_qualified_name &&
            next_token() == tok_ellipsis) {
          /* A potential C++26 type pack-index-specifier (T...[N]) during the
             decl-specifier prescan.  Cache the whole construct so the
             following "[" is not mistaken for the start of an abstract
             declarator.  Only type_specifier_seen cases need this: outside a
             decl-specifier-seq "T..." is a pack expansion of an id-expression,
             not a type, so the "[" is handled by the normal expression
             disambiguation path. */
          get_token_and_coalesce_if_identifier(flags);
          next_token_fetched = TRUE;
          if (curr_token == tok_ellipsis && next_token() == tok_lbracket) {
            get_token_and_coalesce_if_identifier(flags);
            if (curr_token == tok_lbracket) {
              get_token_and_coalesce_if_identifier(flags);
              cache_tokens_until(tok_rbracket, /*coalesce=*/TRUE);
              if (curr_token == tok_rbracket) {
                get_token_and_coalesce_if_identifier(flags);
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
        break;
      /* Type specifier - other simple type name tokens. */
      case tok_char:
      case tok_short:
      case tok_int:
      case tok_long:
      case tok_signed:
      case tok_unsigned:
      case tok_float:
      case tok_double:
      case tok_void:
      case tok_bool:
      case tok_wchar_t:
      case tok_char8_t:
      case tok_char16_t:
      case tok_char32_t:
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* Microsoft type specifiers. */
      case tok_int8:
      case tok_int16:
      case tok_int32:
      case tok_int64:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
      case tok_int128:
#endif /* INT128_EXTENSIONS_ALLOWED */
      case tok_float32:
      case tok_float32x:
      case tok_float64:
      case tok_float64x:
      case tok_float128:
      case tok_nullptr_t:
      case tok_edg_size_type:
      case tok_edg_ptrdiff_type:
      case tok_edg_bool_type:
      case tok_edg_wchar_type:
        type_specifier_seen = TRUE;
        break;
      case tok_bit_precise_int:
        type_specifier_seen = TRUE;
        if (next_token() == tok_lparen) {
          get_token_and_coalesce_if_identifier(flags);
          if (curr_token == tok_lparen) {
            get_token_and_coalesce_if_identifier(flags);
            cache_tokens_until(tok_rparen, /*coalesce=*/TRUE);
            if (curr_token == tok_rparen) {
              get_token_and_coalesce_if_identifier(flags);
            }  /* if */
          }  /* if */
          next_token_fetched = TRUE;
        }  /* if */
        break;
      /* Type qualifier. */
      case tok_const:
      case tok_volatile:
      case tok_restrict:
      case tok_gnu_restrict:
      case tok_nullable:
      case tok_nonnull:
      case tok_null_unspecified:
#if NEAR_AND_FAR_ALLOWED
      case tok_near:
      case tok_far:
#endif /* NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* Microsoft type qualifiers. */
      case tok_unaligned:
      case tok_microsoft_w64:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        break;
      case tok_typename:
        if (next_token() == tok_lsplice && reflection_enabled) {
          /* Presumably "typename [: <expr> :]" or
             "typename [: <expr> :]::id". */
          is_decl_specifier_token = TRUE;
          type_specifier_seen = TRUE;
          /* Bypass "typename". */
          (void)get_token();
          if (!cache_token_stream_until_matching_token((a_token_cache_ptr)NULL,
                                                       CTS_NO_OPTIONS)) {
            /* We found a matching ":]". */
            if (next_token() == tok_colon_colon) {
              /* The splice is a name-qualifier.  Move to the identifier that
                 should follow. */
              (void)get_token();
              get_token_and_coalesce_if_identifier(flags);
            }  /* if */
          } else {
            /* No matching ":]" was found.  Abort the prescan. */
            state->terminate = TRUE;
          }  /* if */
          break;
        }  /* if */
        goto elaborated_type_case;
      case tok_enum:
        /* If "enum" is followed by "class" or "struct", treat the combination
           as if it were just "enum" for disambiguation purposes. */
        { a_token_kind  next_tok = next_token();
          if (next_tok == tok_class || next_tok == tok_struct) {
            (void)get_token();
            curr_token = tok_enum;
          }  /* if */
        }
        FALLTHROUGH
      case tok_class:
      case tok_struct:
      case tok_union:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_interface:
      case tok_value_struct:
      case tok_value_class:
      case tok_ref_struct:
      case tok_ref_class:
      case tok_interface_struct:
      case tok_interface_class:
      case tok_enum_class:
      case tok_enum_struct:
      case tok_partial_ref_struct:
      case tok_partial_ref_class:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
elaborated_type_case:
        /* This could be an elaborated type specifier or the start of
           a enum or class specifier.  The prescanning routines can't
           handle enum and class specifiers, but there should be no need
           for them to do so because types can't be defined in parameter
           lists, and other contexts that are involved in disambiguation.
           We assume this is an elaborated type specifier */
        is_typename = curr_token == tok_typename;
        /* The Microsoft and Sun compilers allow the typename specifier to be
           repeated.  Note that use_implicit_typename() is not used in this
           case. */
        do {
          f_get_token_and_coalesce_if_identifier(
                       flags, curr_token == tok_typename ? GID_IS_TYPENAME
                                                         : GID_NO_OPTIONS);
        } while ((sun_mode || microsoft_bugs) && curr_token == tok_typename);
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
        if (ms_extensions or_near_and_far_enabled()) {
          /* Check for near/far and a Microsoft decl modifier, such as
             __single_inheritance. */
          prescan_extended_decl_modifiers(flags);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
        if (is_typename && microsoft_mode && curr_token != tok_identifier) {
          /* Microsoft allows things like "typename void ...". */
        } else if (type_specifier_seen) {
          /* We've already seen a type specifier, this is probably an
             error. */
          is_decl_specifier_token = FALSE;
        } else {
          /* The token after the class/struct/union/enum keyword must be
             an identifier. */
          if (curr_token != tok_identifier) {
            is_decl_specifier_token = FALSE;
          } else {
            type_specifier_seen = TRUE;
          }  /* if */
        }  /* if */
        if ((flags & DFS_POSSIBLE_ENUM_BASE) != 0) {
          a_token_kind	next_tok = next_token();
          if (next_tok == tok_lbrace || next_tok == tok_semicolon) {
            /* Something like "enum : typename T::X {..." or
               "enum : typename T::X;".  Treat this as an enum declaration. */
            state->terminate = TRUE;
          }  /* if */
        }  /* if */
        break;
      case tok_ellipsis:
        /* An ellipsis is not really a decl-specifier, but we allow it
           here because the prescanning routines are sometimes used to
           scan what may be a function parameter which could look like
           "int ...". */
        break;
      case tok_decltype:
      case tok_underlying_type:
      case tok_typeof:
      case tok_typeof_unqual:
      case tok_edg_vector_type:
      case tok_edg_neon_vector_type:
      case tok_edg_neon_polyvector_type:
      case tok_edg_scalable_vector_type:
      case tok_edg_internal_type:
      case tok_add_lvalue_reference:
      case tok_add_pointer:
      case tok_add_rvalue_reference:
      case tok_decay:
      case tok_make_signed:
      case tok_make_unsigned:
      case tok_remove_all_extents:
      case tok_remove_const:
      case tok_remove_cv:
      case tok_remove_cvref:
      case tok_remove_extent:
      case tok_remove_pointer:
      case tok_remove_reference:
      case tok_remove_reference_t:
      case tok_remove_restrict:
      case tok_remove_volatile:
type_operator_case:
        is_decl_specifier_token = TRUE;
        type_specifier_seen = TRUE;
        prescan_type_operator(state, flags);
        break;
      case tok_ifc_type_ref:
      case tok_decltype_construct:
        is_decl_specifier_token = TRUE;
        type_specifier_seen = TRUE;
        break;
      case tok_alignas:
        is_decl_specifier_token = TRUE;
        prescan_type_operator(state, flags);
        break;
      case tok_lbracket:
        if (std_attribute_tokens_next()) {
          prescan_std_attribute(flags);
          next_token_fetched = TRUE;
          break;
        }  /* if */
        FALLTHROUGH
      default:
        is_decl_specifier_token = FALSE;
        break;
    }  /* switch */
    /* If this token is not part of a decl-specifier then exit the loop. */
    if (!is_decl_specifier_token) break;
    any_decl_specifiers = TRUE;
    if (!next_token_fetched) {
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (state->find_static_specifier_only && type_specifier_seen &&
          next_token() == tok_identifier) {
        /* We're only interested in the presence of the "static" specifier.
           An identifier is next and it cannot be the type specifier (since
           we've already seen it).  Stop the scanning now since coalescing
           the identifier could trigger a spurious error. */
        goto done;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Cache this token and get the next one. */
      get_token_and_coalesce_if_identifier(flags);
    }  /* if */
  }  /* for */
  if (!any_decl_specifiers && !is_ctor_dtor_or_finalizer_name) {
    /* A declaration must have at least one decl-specifier, or this must be
       a constructor or destructor.  This requirement is waived for namespace
       scope declarations. */
    state->may_be_decl = is_template_decl(flags);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
done:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return;
}  /* prescan_decl_specifiers */


/* Forward declaration. */
static void prescan_declaration(a_disambig_state_ptr       state,
                                a_disambig_flag_set 	   flags,
			        a_boolean            	   is_top_level);

static void prescan_trailing_return_type(a_disambig_state_ptr  state)
/*
Scan and cache the tokens that make up a trailing return type (the "->"
introducing the return type has been cached already).
*/
{
  prescan_declaration(state, DFS_ABSTRACT_DECLARATOR_ALLOWED,
                      /*is_top_level=*/FALSE);
}  /* prescan_trailing_return_type */


static void prescan_function_declarator
                              (a_disambig_state_ptr        state,
			       a_disambig_flag_set  	   flags)
/*
Scan and cache the tokens that comprise a function declarator.  This routine
is used by the disambiguation routines.  If a construct that cannot be
part of a function declarator is found, may_be_decl is set to FALSE.
*/
{
  /* Scan the function argument list. */
  while (curr_token != tok_rparen) {
    a_pack_expansion_stack_entry_ptr	pesep = NULL;
    a_boolean				any_more = TRUE;
    if (!state->suppress_packs) {
      /* If packs are not being suppressed, begin a potential pack
         expansion context.  Note that otherwise, pesep will be NULL and
         the other pack routines below will consider this a non-pack
         context. */
      any_more = begin_potential_pack_expansion_context(&pesep);
    }  /* if */
    while (any_more) {
      prescan_any_prefix_bracketed_attributes(flags);
      if (curr_token == tok_ellipsis) {
        /* Advance past the ellipsis. */
        get_token_and_coalesce_if_identifier(flags);
      } else {
        /* A parameter declaration.  Scan the declaration. */
        a_disambig_flag_set  param_flags = DFS_ABSTRACT_DECLARATOR_ALLOWED |
                                           DFS_REAL_DECLARATOR_ALLOWED |
                                           DFS_SINGLE_TYPE_REQUIRED;
        if (relaxed_typename_enabled) {
          param_flags |= DFS_IMPLICIT_TYPENAME_CONTEXT;
        }  /* if */
        prescan_declaration(state, param_flags, /*is_top_level=*/FALSE);
        if (terminate_disambiguation(state)) {
          abandon_potential_pack_expansion_context(pesep);
          goto done;
        }  /* if */
      }  /* if */
      if (curr_token == tok_comma) {
        get_token_and_coalesce_if_identifier(flags);
      } else if (curr_token != tok_rparen && curr_token != tok_ellipsis) {
        /* After scanning a parameter declaration we should be at a comma,
           the closing right parenthesis, or an ellipsis that follows an
           argument without an intervening comma.  If not, we conclude that
           this isn't really a declaration. */
       state->may_be_decl = FALSE;
       abandon_potential_pack_expansion_context(pesep);
       goto done;
      }  /* if */
      (void)end_potential_pack_expansion_context(pesep,
                                                 /*is_declarator=*/TRUE);
      any_more = advance_to_next_pack_element(pesep);
    }  /* while */
  }  /* while */
  /* Cache and bypass the right parenthesis. */
  get_token_and_coalesce_if_identifier(flags);
  /* Skip past any cv-qualifiers associated with this function declarator. */
  while (is_type_qualifier() or_is_near_or_far()) {
    get_token_and_coalesce_if_identifier(flags);
  }  /* while */
  if (curr_token == tok_ampersand || curr_token == tok_and_and) {
    /* C++11 ref-qualifiers.  (Prescan them even in non-C++11 modes since they
       cannot be valid either way.  An error or warning will be issued when
       parsing the declarator.) */
    get_token_and_coalesce_if_identifier(flags);
  }  /* if */
  /* Cache the tokens associated with the optional exception specification.
     Note that we don't try to disambiguate a throw expression from a
     throw declaration.  This is not needed when the prescan is to identify
     "auto" parameters (and could trigger errors if the exception
     specification refers back to parameters, which haven't been declared
     yet). */
  if (curr_token == tok_throw || curr_token == tok_edg_throw ||
      curr_token == tok_noexcept) {
    /* Advance past the throw or noexcept keyword. */
    a_boolean  arg_optional = curr_token == tok_noexcept;
    get_token_and_coalesce_if_identifier(flags);
    if (curr_token != tok_lparen) {
      if (!arg_optional) {
        /* A throw specification must follow the throw keyword in a function
           declarator. */
        state->may_be_decl = FALSE;
        goto done;
      }  /* if */
    } else {
      /* Advance past the left parenthesis. */
      get_token_and_coalesce_if_identifier(flags);
      cache_tokens_until(tok_rparen, /*coalesce=*/TRUE);
      if (curr_token == tok_rparen) {
        /* Cache the right parenthesis. */
        get_token_and_coalesce_if_identifier(flags);
      }  /* if */
    }  /* if */
  }  /* if */
  if (trailing_return_types_enabled && curr_token == tok_arrow &&
      (flags & (DFS_ABSTRACT_DECLARATOR_ALLOWED |
                DFS_REAL_DECLARATOR_ALLOWED)) != 0) {
    /* Cache the trailing return type.  This is not needed when the prescan is
       to identify "auto" parameters (and could trigger errors if the trailing
       return type refers back to parameters, which haven't been declared
       yet). */
    (void)get_token();
    prescan_trailing_return_type(state);
  }  /* if */
done:
  return;
}  /* prescan_function_declarator */


static void prescan_declarator(a_disambig_state_ptr state,
			       a_disambig_flag_set  flags,
			       a_boolean            paren_initializer_allowed,
			       a_boolean            is_top_level)
/*
Scan and cache the tokens that comprise a declarator.  This routine
is used by the disambiguation routines.  If a construct that cannot be
part of a declarator is found, may_be_decl is set to FALSE.
*/
{
  a_boolean       paren_initializer_seen = FALSE;
  a_boolean       pointer_operator_seen = FALSE;

  /* Look for one or more instances of a sequence of tokens corresponding
     to ptr-operator.  Syntax:
         * cv-qualifier-list
         & cv-qualifier-list
         && cv-qualifier-list
         complete-class-name :: * cv-qualifier-list
         microsoft-qualifier-list
     In C++/CLI mode, the following are also possible:
         ^ cv-qualifier-list
         % cv-qualifier-list
     Note that neither pointer declarators nor qualifiers are allowed
     in expressions, so their presence means this is a declaration.
     When rvalue references are enabled, "&&" is also allowed. */
  for (;;) {
    if (curr_token == tok_star || curr_token == tok_ampersand ||
        (rvalue_references_enabled && curr_token == tok_and_and)
        or_is_cli_declarator_operator(curr_token)) {
      /* Cache and bypass the "*" or "&" or "&&" (or, in C++/CLI mode, the
         "^" or "%"). */
      get_token_and_coalesce_if_identifier(flags);
      pointer_operator_seen = TRUE;
      if (std_attribute_tokens_next()) {
        /* C++11/C23 permit attributes after the pointer/reference operator. */
        prescan_std_attribute(flags);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (curr_token == tok_based) {
      /* Microsoft __based modifier. */
      prescan_based_modifier(flags);
    } else if (curr_token == tok_microsoft_w64 ||
               curr_token == tok_microsoft_sptr ||
               curr_token == tok_microsoft_uptr ||
               curr_token == tok_microsoft_ptr32 ||
               curr_token == tok_microsoft_ptr64) {
      /* Syntactically, __w64, __ptr32, and __ptr64 are similar to type
         qualifiers, but semantically they don't affect the type (which
         is why they are not included in "is_type_qualifier"). */
      get_token_and_coalesce_if_identifier(flags);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (is_type_qualifier() ||
               curr_token == tok_ptr_to_member
               or_is_microsoft_declarator_keyword(curr_token)
               or_is_near_or_far()) {
      /* Cache and bypass any cv-qualifiers (including near or far in
         certain modes). */
      /* Cache and bypass any pointer to member operators. */
      /* Keywords allowed in declarators in Microsoft mode, e.g., __cdecl. */
      get_token_and_coalesce_if_identifier(flags);
      if (std_attribute_tokens_next()) {
        /* C++11/C23 permit attributes after the pointer/reference operator. */
        prescan_std_attribute(flags);
      }  /* if */
    } else {
      /* No more ptr-operators. */
      break;
    }  /* if */
  }  /* for */
#if GNU_EXTENSIONS_ALLOWED
  if (curr_token == tok_attribute) {
    /* A GNU __attribute__ may appear after the pointer-declarators. */
    prescan_gnu_attribute(flags);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (curr_token == tok_lparen) {
    /* Left parenthesis indicating nested declarator.  For the abstract
       declarator case, this might be a parenthesis indicating a function.
       We can differentiate the two cases because in the case of a nested
       declarator the next token must be a "*", "(", or "[", whereas in the
       function case it is ")", "...", or a declaration specifier. */
    a_boolean	treat_as_expr = FALSE;
    get_token_and_coalesce_if_identifier(flags);
    if (any_cfront_mode() && is_top_level && !is_template_decl(flags)) {
      /* Cfront handles declarations like
             int a(int());
         as the declaration of an object with an initializer of int()
         (which evaluates to zero), where it really should be a
         function taking parameter of type "function () returning int".
         If this is a top level declaration (e.g., not part of a
         parameter list) don't consider typename() to be a declaration
         in cfront mode. 
 
         Also, in a context in which a parameter declaration
         must be distinguished from an argument expression, cfront seems
         always to treat "type-name ( identifier )" as an expression,
         contrary to our reading of the ARM.  For example:
           class A { A(int); };
           A a(int(x));
         Cfront takes "int(x)" to be an argument to the constructor and
         treats "a" as a variable, but the ARM requires "int(x)" to be a
         declaration and therefore "a" must be a function.  (Note that it
         is a param-decl-vs-arg-expr context if both real and abstract
         declarators are allowed.)

         And finally, cfront makes the same kind of mistake when evaluating
         constructs like this:
           class A { A(int); };
           A(x);
         cfront treats this as a constructor call instead of a declaration
         of an object named x.

         This processing is not done when prescanning template declarations
         because we know the thing being scanned is a declaration and not
         an expression. */
      a_token_kind	token_2;
      if (curr_token == tok_identifier &&
          next_two_tokens(tok_rparen, &token_2) == tok_rparen) {
        if (token_2 != tok_lparen) {
          /* Construct like "A(x);", but not "A(x)(...)". */
          treat_as_expr = TRUE;
        }  /* if */
      } else if (curr_token == tok_rparen) {
        /* Construct like "A a(int());". */
        treat_as_expr = TRUE;
      } else if (abstract_declarator_allowed(flags) &&
                 !pointer_operator_seen) {
        if (is_type_specifier() || curr_token == tok_identifier) {
          /* Construct like A a(int(x));". */
          treat_as_expr = TRUE;
        }  /* if */
      }  /* if */
      if (treat_as_expr) {
        state->may_be_decl = FALSE;
        goto done;
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_bugs && microsoft_version < 1300 &&
        is_top_level && !is_template_decl(flags)) {
      /* The Microsoft compiler suffers from some of the same disambiguation
         problems that cfront does.  The 7.0 compiler (version 1300) fixes
         these problems.  See the cfront mode code above for additional
         information. */
      if (curr_token == tok_rparen) {
        /* Construct like "A a(int());". */
        treat_as_expr = TRUE;
      } else if (abstract_declarator_allowed(flags) &&
                 !pointer_operator_seen) {
        if (is_type_specifier() || curr_token == tok_identifier) {
          /* Construct like A(int());"  Note that second_token will be
             tok_error if the first token is not tok_lparen. */
          a_token_kind	token_2;
          (void)next_two_tokens(tok_lparen, &token_2);
          if (token_2 == tok_rparen) {
            treat_as_expr = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (treat_as_expr) {
        state->may_be_decl = FALSE;
        goto done;
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (abstract_declarator_allowed(flags)) {
      if (curr_token == tok_rparen ||
          is_decl_start(IDS_MS_ATTRIB_NOT_ALLOWED |
                        IDS_REAL_DECLARATOR_ALLOWED) ||
          (curr_token == tok_ellipsis && next_token() == tok_rparen)) {
        /* Function declarator rather than a nested declarator. */
        goto function_lparen;
      }  /* if */
    }  /* if */
    /* Get the nested declarator. */
#if GNU_EXTENSIONS_ALLOWED
    if (curr_token == tok_attribute) {
      /* A GNU __attribute__ may appear at the start of a nested declarator. */
      prescan_gnu_attribute(flags);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    if (curr_token == tok_identifier && locator_for_curr_id.is_template_id &&
        gnu_mode && (flags & DFS_IS_TEMPLATE_DECL) == 0) {
      /* A case like: "T(A<int>())" that is not in a template declaration
         (i.e., it's not a specialization of a function or variable template).
         GCC/clang appear to treat this as an expression (though a template-id
         as a typedef in the same location is treated as a declaration). */
      state->may_be_decl = FALSE;
      goto done;
    }  /* if */
    prescan_declarator(state, flags,
                       /*paren_initializer_allowed=*/FALSE,
                       /*is_top_level=*/FALSE);
    if (terminate_disambiguation(state)) goto done;
    /* The nested declarator must be followed by a ")". */
    if (curr_token != tok_rparen) {
      state->may_be_decl = FALSE;
      goto done;
    }  /* if */
    get_token_and_coalesce_if_identifier(flags);
  } else {
    a_boolean	is_name_start;
    /* An ellipsis indicating a parameter pack declaration might be next. */
    if (curr_token == tok_ellipsis && variadic_templates_enabled) {
      get_token_and_coalesce_if_identifier(flags);
    }  /* if */
    /* An identifier is expected next, but is omitted in the 
       abstract declarator.  All tokens that could start an identifier will
       have already been coalesced into a tok_identifier.  Destructor
       declarators are not allowed in this context, so tildes don't need
       to be handled. */
    /* Declarator names cannot contain global qualifiers (e.g., ::i). This
       is allowed for template prescans.  In g++ mode, a nested declarator
       that is a qualified name forces this to be not a declarator name. */
    is_name_start = curr_token == tok_identifier &&
                    (!locator_for_curr_id.is_global_qualified_name ||
                     is_template_decl(flags)) &&
                    !(state->check_if_is_decl && gpp_mode && !is_top_level &&
                      locator_for_curr_id.is_qualified_name);
    if (!real_declarator_allowed(flags) ||
        (abstract_declarator_allowed(flags) && !is_name_start)) {
      /* Identifier is omitted in an abstract declarator. */
    } else if (!is_name_start) {
      /* Real (non-abstract) declarator.  An identifier must be found here. */
      state->may_be_decl = FALSE;
      goto done;
    } else {
      get_token_and_coalesce_if_identifier(flags);
      if (state->set_decl_class_type) {
        /* Return a pointer to the class of which a member (if any) of the
           declarator. */
        state->decl_class_type = qualifier_class_type(locator_for_curr_id);
        /* Clear the may_be_decl flag to suppress further scanning. */
        state->may_be_decl = FALSE;
        goto done;
      }  /* if */
    }  /* if */
  }  /* if */
  if (std_attribute_tokens_next()) {
    /* C++11/C23 permit attributes in the declarator. */
    prescan_std_attribute(flags);
  }  /* if */
  /* The declarator can end at this point, or an array or function
     specification (or a series of them) can follow.  The additional
     specifications, if they appear, are parsed in their order of 
     appearance. */
  while (curr_token == tok_lparen || curr_token == tok_lbracket) {
    if (curr_token == tok_lparen) {
      /* Appears to be a function declarator.  But be sure it's not the
         start of a parenthesized initializer. */
      /* Advance past the left parenthesis. */
      get_token_and_coalesce_if_identifier(flags);
      if (curr_token != tok_rparen && curr_token != tok_ellipsis) {
        /* See if the thing inside the parenthesis looks like an
           initializer. */
        if (paren_initializer_allowed &&
            !is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                              DFS_REAL_DECLARATOR_ALLOWED)) {
          /* Function_declarator should not be called, scan the tokens that
             comprise the parenthesized initializer and exit the loop. */
          paren_initializer_seen = TRUE;
          cache_tokens_until(tok_rparen, /*coalesce=*/TRUE);
          if (curr_token == tok_rparen) {
            /* Cache the right parenthesis. */
            get_token_and_coalesce_if_identifier(flags);
          }  /* if */
          break;
        }  /* if */
      }  /* if */
function_lparen:
      /* For function types as the top type, fetch the extra function info
         as well.  For non-top types, do not. */
      prescan_function_declarator(state, flags);
    } else {
      /* Left bracket, indicating array declarator. */
      check_assertion(curr_token == tok_lbracket);
      /* Advance past the left bracket. */
      get_token_and_coalesce_if_identifier(flags);
      cache_tokens_until(tok_rbracket, /*coalesce=*/TRUE);
      /* Bypass and cache the "]". */
      if (curr_token == tok_rbracket) {
        get_token_and_coalesce_if_identifier(flags);
      }  /* if */
    }  /* if */
  }  /* while */
#if GNU_EXTENSIONS_ALLOWED
  if (curr_token == tok_attribute) {
    /* A GNU __attribute__. */
    prescan_gnu_attribute(flags);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Look for an initializer of the "=" or "{...}" form. */
  if (!paren_initializer_seen &&
      (curr_token == tok_assign ||
       (is_top_level && curr_token == tok_lbrace && list_init_enabled &&
        real_declarator_allowed(flags)))) {
    /* An initializer that begins with an equals sign or a C++11 brace-enclosed
       initializer list.  Cache the tokens that comprise the initializer and
       leave curr_token as the token following the initializer (usually a
       comma or a semicolon). */
    if (curr_token == tok_assign) {
      /* If we reach an "= x" initializer of a top-level declaration, we don't
         need to look past the initializer. */
      if (is_top_level && declarator_only_check(flags)) {
         state->terminate = TRUE;
      } else {
        prescan_initializer();
      }  /* if */
    } else {
      prescan_init_list();
    }  /* if */
  } else {
    /* A condition is required to have an "=" or "{...}" style initialization.
       If the initialization is missing, don't consider this to be
       a condition.  This causes things like "int(i)" to be treated as
       expressions, not conditions. */
    if (is_top_level && is_condition(flags)) state->may_be_decl = FALSE;
  }  /* if */
done:;
}  /* prescan_declarator */


static void prescan_declaration(a_disambig_state_ptr state,
                                a_disambig_flag_set  flags,
			        a_boolean            is_top_level)
/*
Scan a sequence of tokens and cache them for rescanning later.  The purpose
of this prescan is to help determine whether this is a declaration or an
expression.  The caller guarantees that is_decl_start is TRUE for the current
token.

Assuming that we are in the midst of a declaration, we scan ahead to find
evidence to the contrary. 
*/
{
  an_identifier_options_set	gid_flags = GID_TEMPLATE_ARGS_OPTIONAL |
                                            GID_IS_EXPR_CONTEXT;


  db_enter(3, "prescan_declaration");
  if (curr_token == tok_extension) {
    /* Skip over a leading GNU __extension__ keyword. */
    (void)get_token();
  }  /* if */
  if ((flags & DFS_IMPLICIT_TYPENAME_CONTEXT) != 0) {
    gid_flags |= GID_IMPLICIT_TYPENAME_CONTEXT;
  }  /* if */
  /* Coalesce the identifier if this is a tok_identifier. */
  (void)is_generalized_identifier_start(gid_flags |
                                        gid_flags_for_template(
                                                       flags, GID_NO_OPTIONS));
  for (;;) {
    a_disambig_flag_set  decl_spec_flags = flags;
    /* Prescan leading bracket-enclosed attributes (if any). */
    prescan_any_prefix_bracketed_attributes(flags);
    /* Scan the decl specifiers. */
    prescan_decl_specifiers(state, decl_spec_flags);
    if ((flags & DFS_POSSIBLE_ENUM_BASE) != 0) {
      /* If the following token is a left brace or a semicolon, we have seen
         an enum-base.  Otherwise, this is something else.  Either way, we
         have our answer. */
      if (curr_token != tok_lbrace && curr_token != tok_semicolon) {
        state->may_be_decl = FALSE;
      }  /* if */
      state->terminate = TRUE;
    } else if (curr_token == tok_lbrace) {
      /* A set of specifiers followed by a brace is a C++11-style functional-
         notation cast (with an exception for something like
         "enum : typename T::X { ...", which was handled above). */
      state->may_be_decl = FALSE;
    }  /* if */
    if (terminate_disambiguation(state)) goto done;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (state->find_static_specifier_only) goto done;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    for (;;) {
      /* Parenthesized initializers are only allowed in contexts
         in which only real declarators are allowed, but not in
         conditions. */
      a_boolean	paren_initializer_allowed;
      paren_initializer_allowed = !abstract_declarator_allowed(flags) &&
                                  !is_condition(flags);
      prescan_declarator(state, flags,
                         paren_initializer_allowed,
			 is_top_level);
      if (terminate_disambiguation(state)) goto done;
      /* If we are not processing real declarators, or if we are processing
         a condition, don't look for additional declarators. */
      if (abstract_declarator_allowed(flags) ||
          is_condition(flags) ||
          curr_token != tok_comma) break;
      /* Advance past the comma then scan the next declarator. */
      get_token_and_coalesce_if_identifier(flags);
    }  /* for */
    /* Break out of the loop if no additional declaration seems to follow
       or if multiple types are not allowed. */
    if (single_type_required(flags) ||
         (curr_token != tok_comma && curr_token != tok_ellipsis)) {
      break;
    }  /* if */
    /* Advance past the comma then scan the next declaration. */
    if (curr_token != tok_ellipsis) {
      get_token_and_coalesce_if_identifier(flags);
    }  /* if */
  }  /* for */
done:
  db_exit();
}  /* prescan_declaration */


/*
Return TRUE if token could be a unary or binary operator.
*/
#define is_unary_and_binary_operator_token(token)			\
  ((token) == tok_plus ||						\
   (token) == tok_minus ||						\
   (token) == tok_star ||						\
   (token) == tok_ampersand)


static a_boolean is_decl_not_expr_full(a_disambig_flag_set flags)
/*
This routine is called (in C++ only) to distinguish statements and expressions
from declarations -- for example:
  (1) a statement vs. a declaration, e.g.,
         typedef int I;
         I(i);                // declaration (= I i);
         I(i)++;              // cast i to I, then increment
  (2) in an operator new expression, a parenthesized type vs. a placement
      expression, e.g.,
         new (int(1.5)) A     // placement
         new (int(*  ))       // type
  (3) a parenthesized initializer vs. a parameter declaration, e.g.,
         A a(int(1));         // initialize a by calling A::A() with arg 1
         A a(int(i));         // function a takes int arg, returns A
  (4) a statement vs. a declaration in a condition context
         if (A(i) = 1)        // A (probably invalid) condition declaration
         if (A(i) == 1)       // An expression

The ARM discusses disambiguation in sections 6.8 and 8.1.1.  In general, if a
sequence of tokens looks like a declaration, then it is a declaration, even
if it could also be an expression.  The technique used here involves assuming
a declaration and looking ahead as many tokens as necessary to confirm or
disprove the assumption or, in the case of a more persistent ambiguity, to
decide on the basis of tokens following the "declaration".  Tokens are cached
so that they can be rescanned by the caller.

"flags" provides some information about the context, specifically whether,
if it is a declaration, an abstract or real declarator is expected -- or
either.  The DFS_SINGLE_TYPE_REQUIRED flag is used to indicate that the
construct scanned must be a single type and not a comma separated list.
DFS_IS_CONDITION indicates that a condition statement is being scanned.
DFS_CONDITION_IS_FOR_STMT indicates that the condition is in a for
statement.  DFS_IS_CAST indicates that a cast like "(int)x" is being
scanned, in which case the context following the cast is inspected
to distinguish between cases like (A()), which is an expression and
(A())+1 which is a cast.

These flags are combined in the following ways to handle the various
disambiguation contexts:

Context				Abstract	Real		Single
				allowed		allowed		decl. required
----------------------  	--------	-------		--------------
For-init statements		false		true		disregarded
declaration vs. expr stmt.	false		true		disregarded
sizeof, alignof, new, cast	true		false		true
throw specification		true		false		true
parenthesized initializer vs.	true		true		disregarded
	parameter list


In other words, when a real declarator is allowed, but an abstract declarator
is not, the context is a normal declaration that may contain multiple
declarators.

When both real and abstract declarators are allowed, the context is a
parameter list in which multiple declarations (i.e., not declarators)
may be separated by commas.

When abstract declarators are allowed, but real declarators are not, the
context is one type (when single_type_required is TRUE) or multiple
types separated by commas (when single_type_required is FALSE).

*/
{
  a_boolean	      prev_do_not_clear_specific_symbol = FALSE;
  a_disambig_state    state;
  a_boolean	      result = TRUE;
  a_boolean	      is_implicit_template_type;
  a_symbol_ptr	      specific_sym = locator_for_curr_id.specific_symbol;
  a_token_kind	      next_tok;
  a_boolean	      is_start_of_type;

  db_enter(3, "is_decl_not_expr_full");
  if (curr_token == tok_extension) {
    /* The GNU __extension__ keyword can start an expression or a declaration:
       temporarily skip the token, and restart disambiguation from the next
       token. */
    a_tiny_scanning_token_cache cache;

    cache_curr_token(cache.ptr());
    (void)get_token();
    result = is_decl_not_expr(flags);
    rescan_cached_tokens(cache.ptr());
    goto done;
  }  /* if */
  /* Determine whether the current identifier is a synthesized template
     parameter type symbol created in implicit_typename mode.  If so,
     we must do additional checking for casts to determine that the
     syntax really looks like a cast, because we can't be positive that
     the identifier was really intended to be a type. */
  is_implicit_template_type = 
        curr_token == tok_identifier && use_implicit_typename() &&
        specific_sym != NULL && specific_sym->kind == (a_symbol_kind)sk_type &&
        specific_sym->variant.type.ptr->kind ==
                                       (a_type_kind)tk_template_param &&
        specific_sym->variant.type.ptr->variant.template_param.kind ==
                                       (a_template_param_type_kind)tptk_member;
  /* In Microsoft mode, function-style cases like (unsigned int(x)) are
     allowed so we may need to scan past several tokens in order to look
     for the left parenthesis below. */
  next_tok = next_token();
  is_start_of_type = is_type_start_full(/*is_expr_context=*/TRUE,
                                        /*is_prescan=*/TRUE, IDS_NO_OPTIONS);
  if (pack_indexing_allowed && is_start_of_type &&
      curr_token == tok_identifier && next_tok == tok_ellipsis) {
    a_token_kind  pack_index_next_tok =
                                   token_kind_following_pack_index_specifier();
    if (pack_index_next_tok == tok_lparen ||
        (is_template_argument(flags) && pack_index_next_tok == tok_lbrace)) {
      /* The token stream is of the form "T...[N]".  Treat this C++26 type
         pack-index-specifier as if it were the bare type-name "T". */
      next_tok = pack_index_next_tok;
    }  /* if */
  }  /* if */
  if (microsoft_mode && is_start_of_type && curr_token != tok_identifier &&
      next_tok != tok_lparen && next_tok != tok_declspec &&
      next_tok != tok_alignas) {
    a_scanning_token_cache cache;
    a_token_kind           next_2_tok;
    a_boolean              any_tokens_fetched = FALSE;

    (void)next_two_tokens(next_tok, &next_2_tok);
    while ((is_type_keyword(next_tok) ||
            is_type_qualifier_token(next_tok)) && next_2_tok != tok_lparen) {
      cache_curr_token(cache.ptr());
      (void)get_token();
      any_tokens_fetched = TRUE;
      next_tok = next_token();
      (void)next_two_tokens(next_tok, &next_2_tok);
    }  /* while */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* The __based keyword can appear after a type in some contexts. */
    if (next_tok == tok_based) {
      is_start_of_type = TRUE;
    } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    if (any_tokens_fetched) {
      /* If we get here we have a type token followed by a token that may or
         may not be a type.  Use is_type_start_full to determine if it is
         something like a type identifier. */
      is_start_of_type = is_type_start_full(/*is_expr_context=*/TRUE,
                                            /*is_prescan=*/TRUE,
                                            IDS_NO_OPTIONS);
    } else if (next_2_tok != tok_lparen) {
      /* Not a case we need to worry about. */
    } else if (next_tok == tok_identifier) {
      /* Check whether the identifier is a type. */
      cache_curr_token(cache.ptr());
      (void)get_token();
      any_tokens_fetched = TRUE;
      is_start_of_type = is_type_start_full(/*is_expr_context=*/TRUE,
                                            /*is_prescan=*/TRUE,
                                            IDS_NO_OPTIONS);
    }  /* if */
    next_tok = next_2_tok;
    if (any_tokens_fetched) rescan_cached_tokens(cache.ptr());
  }  /* if */
  /* The ambiguous cases all begin a type name followed by a left
     parenthesis.   Check for this case first to quickly discard most
     cases.  If the current token is "typename" we don't have enough
     information at this point to discard the easy cases, so we need to
     do the full processing.
     Note: Casts require special treatment in two cases.  When using
     implicit typename we need to verify that this would be a valid cast
     to make sure we didn't guess incorrectly about this being a type.
     In normal mode, we assume this to be a cast when the type start is
     not followed by a "(". */
  if ((curr_token == tok_auto &&
       !(auto_cast_enabled || gpp_version_is(>=120000))) ||
      curr_token == tok_c11_atomic) {
    /* "auto" is a type specifier, but prior to C++23 it cannot be part of a
       function-style cast; "auto(" is only valid as part of a declarative
       construct involving a trailing return type (e.g., "auto(*)()->int")
       Similarly, "_Atomic(" is the start of a type specifier, not a cast. */
  } else if (curr_token == tok_typename ||
             ((next_tok == tok_lparen ||
               (is_template_argument(flags) && next_tok == tok_lbrace) ||
               (is_cast(flags) && /* See note above */
                is_implicit_template_type)) &&
              is_start_of_type)) {
    if (curr_token == tok_identifier) {
      /* The prescanning process clears the specific symbol field of the
         locator to prevent the prescanning process from biasing
         subsequent lookups.  If the locator is already set, however, then
         the prescanning process should not cause it to be cleared. */
      prev_do_not_clear_specific_symbol =
                            locator_for_curr_id.do_not_clear_specific_symbol;
      if (!is_implicit_template_type) {
        /* Unless this is an implicit template type symbol, ensure that the
           specific symbol for this locator is not cleared.  For implicit
           template type symbols we want it to be cleared because when it
           is looked up again later, it might be determined to be a nontype. */
        locator_for_curr_id.do_not_clear_specific_symbol = TRUE;
      }  /* if */
      clear_specific_symbol(locator_for_curr_id);
    }  /* if */
    /* Initialize the token cache. */
    init_disambig_state(&state, /*check_if_is_decl=*/TRUE,
                        /*suppress_packs=*/TRUE,
                        /*cache_tokens=*/TRUE);
    /* Scan forward as far as required to determine whether this is a
       declaration.  Each token that is encountered is cached away, so
       that they can be restored for the actual scan. */
    prescan_declaration(&state, flags, /*is_top_level=*/TRUE);
    if (terminate_disambiguation(&state)) goto restore_token_sequence;
    /* We are now usually at a point where only a few possible tokens are next.
       (E.g., if this is a declaration we should be at either a comma
       separating two declarators or at the semicolon at the end of the
       declaration.)  An exception occurs with the operand of a reflection
       operator, which might have all kinds of tokens following it (e.g.,
       "constexpr auto r = ^void(*)() | process;").  For the other cases,
       assume that this is really an expression if the tokens that follow
       would be invalid otherwise. */
    if (real_declarator_allowed(flags)) {
      if (abstract_declarator_allowed(flags)) {
        /* We are scanning a parameter list, we should be at the closing
           right parenthesis. */
        if (curr_token != tok_rparen) state.may_be_decl = FALSE;
      } else {
        /* We are scanning a real declaration, we should be at the end of
           the statement now. */
        if (is_condition(flags) && selection_initializers_enabled) {
          /* A declaration in a condition statement where initializers are
             allowed can end with a right parenthesis or a semicolon. */
          if (curr_token != tok_rparen && curr_token != tok_semicolon) {
            state.may_be_decl = FALSE;
          }  /* if */
        } else if (is_condition(flags) && !condition_is_for_stmt(flags)) {
          /* Condition statements (except in for statements) must end
             with a right parenthesis. */
          if (curr_token != tok_rparen) state.may_be_decl = FALSE;
        } else {
          /* All other declarations must end in a semicolon. */
          if (curr_token != tok_semicolon) state.may_be_decl = FALSE;
        }  /* if */
      }  /* if */
    } else if (is_template_argument(flags)) {
      /* A template argument may be followed by a comma or a greater-than. */
      if (curr_token != tok_comma && curr_token != tok_gt &&
          (!scope_stack_top().in_template_arg_list ||
           curr_token != tok_shift_right)) {
        state.may_be_decl = FALSE;
      }  /* if */
    } else if (flags & DFS_IS_REFLECTION_OPND) {
      /* Do not consider tokens that follow. */
    } else {
      /* Otherwise, if we are scanning one or more types.  We should be
         at a right delimiter. */
      if (curr_token != tok_rparen && curr_token != tok_rbrace &&
          curr_token != tok_rbracket) {
        state.may_be_decl = FALSE;
      }  /* if */
      if (state.may_be_decl && is_cast(flags)) {
        /* If we are in a cast context, look at what follows the right
           parenthesis to see if it is something that could follow a cast.
           This is to prevent (A()) from being interpreted as an invalid
           cast. */
        (void)get_token();
        /* If the thing after the right parenthesis is not the start of an
           expression, then this is not a cast -- so indicate that this is not
           a declaration.  Beware of the "()" case, which is not an expression;
           i.e., "(T())()" is not a cast at the top-level.  Also beware of ++
           and --, which could be a prefix operator of a cast expression, or a
           postfix operation on the parenthesized expression.  When implicit
           typename is enabled and the type is an implicit dependent type,
           don't treat this as a cast if the thing after the right parenthesis
           is something that could be either a unary or binary operator (i.e.,
           in such cases, assume it to be the binary operator, not a cast of a
           unary operation).  A compound literal (e.g., "(int){0}") looks like
           a cast followed by a brace (and is treated as such). */
        if ((!is_expr_start_token(curr_token) ||
             (curr_token == tok_lparen ? next_token() == tok_rparen :
              curr_token == tok_plus_plus ?
                                         !is_expr_start_token(next_token()) :
              curr_token == tok_minus_minus ?
                                         !is_expr_start_token(next_token()) :
                                         FALSE) ||
             (is_implicit_template_type &&
              is_unary_and_binary_operator_token(curr_token))) &&
            (!compound_literals_allowed || curr_token != tok_lbrace)) {
          state.may_be_decl = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
restore_token_sequence:
    wrapup_disambig_state(&state);
    if (curr_token == tok_identifier || curr_token == tok_ifc_decl_ref) {
      /* Restore the saved value of the do_not_clear_specific_symbol
         flag. */
      locator_for_curr_id.do_not_clear_specific_symbol =
                                            prev_do_not_clear_specific_symbol;
    }  /* if */
    result = state.may_be_decl;
  }  /* if */
done:
  db_exit();
  return result;
}  /* is_decl_not_expr_full */


a_boolean is_decl_not_expr(a_disambig_flag_set	flags)
/*
Called in various contexts to distinguish expressions from declarations. 
In C this is straightforward -- is_decl_start() provides all the information
needed.  But the added complexity of disambiguation in C++ requires calling a
routine to do lookahead, etc.
*/
{
  an_is_decl_start_options_set	is_decl_start_options;
  a_boolean			result = FALSE;

  /* When processing what might be an enum base, do not treat this as an
     expression context for is_decl_start purposes. */
  is_decl_start_options = ((flags & DFS_POSSIBLE_ENUM_BASE) == 0
                                        ? IDS_EXPR_CONTEXT : IDS_NO_OPTIONS) |
                          ((flags & DFS_IS_SIZEOF) != 0
                                        ? IDS_IS_SIZEOF : IDS_NO_OPTIONS);
  if ((flags & DFS_REAL_DECLARATOR_ALLOWED) != 0) {
    is_decl_start_options |= IDS_REAL_DECLARATOR_ALLOWED;
  }  /* if */
  if (!C_mode()) {
    if (is_decl_start(is_decl_start_options)) {
      result = is_decl_not_expr_full(flags);
    } else {
      /* is_decl_start returns FALSE on "overload" but it should still be
         considered a declaration. */
      result = curr_token == tok_overload;
    }  /* if */
  } else {
    result = is_decl_start(is_decl_start_options);
  }  /* if */
  return result;
}  /* is_decl_not_expr */


a_boolean is_func_declarator_start(void)
/*
Return TRUE if what follows looks like the start of a function declarator.
*/
{
  a_boolean  result;

  if (curr_token != tok_lparen) {
    result = FALSE;
  } else {
    a_disambig_state  state;
    /* Initialize the disambiguation state block. */
    init_disambig_state(&state, /*check_if_is_decl=*/TRUE,
                        /*suppress_packs=*/TRUE,
                        /*cache_tokens=*/TRUE);
    get_token_and_coalesce_if_identifier(DFS_NO_FLAGS);
    prescan_function_declarator(&state, DFS_NO_FLAGS);
    result = state.may_be_decl;
    wrapup_disambig_state(&state);
  }  /* if */
  return result;
}  /* is_func_declarator_start */


a_type_ptr prescan_and_find_declarator(a_boolean     *is_friend_decl)
/*
Scan the declaration that follows "template <...>" and find the
declarator.  Record the class of which a member of the declarator.
Return in *is_friend_decl whether "friend" was among the
decl-specifiers processed before reaching the declarator.  The caller
provides a token cache containing the tokens to be scanned.
Consequently, the cache built by the prescan routines is not needed
and is discarded.  After the scan is done, any tokens remaining in the
cache passed by the caller are flushed.
*/
{
  a_disambig_state    state;

  /* Initialize the disambiguation state block. */
  init_disambig_state(&state, /*check_if_is_decl=*/FALSE,
                      /*suppress_packs=*/TRUE,
                      /*cache_tokens=*/TRUE);
  state.set_decl_class_type = TRUE;
  prescan_declaration(&state,
                      DFS_REAL_DECLARATOR_ALLOWED | DFS_IS_TEMPLATE_DECL,
                     /*is_top_level=*/TRUE);
  *is_friend_decl = state.friend_encountered;
  wrapup_disambig_state(&state);
  return state.decl_class_type;
}  /* prescan_and_find_declarator */


a_token_kind find_for_loop_separator(void)
/*
A helper routine for the scanning of "for" and expansion statement headers
that returns tok_colon or tok_semicolon according to which is found first in
the token stream (skipping over any tok_colon tokens that are paired with
tok_quest_mark).  A top-level tok_rparen (the closing ")" of the header) and
the end of source also stop the scan and are returned as the result.
*/
{
  a_token_set_array	stop_token_array;
  unsigned int		question_count = 0;
  a_token_kind		result;
  a_disambig_state	state;

  /* Initialize the disambiguation state block. */
  init_disambig_state(&state, /*check_if_is_decl=*/FALSE,
                      /*suppress_packs=*/FALSE,
                      /*cache_tokens=*/TRUE);
  clear_token_set_array(stop_token_array);
  incr_token_set_array_element(stop_token_array, tok_quest_mark);
  incr_token_set_array_element(stop_token_array, tok_colon);
  incr_token_set_array_element(stop_token_array, tok_semicolon);
  incr_token_set_array_element(stop_token_array, tok_rparen);
  for (;;) {
    cache_token_stream_full((a_token_cache_ptr)NULL, stop_token_array,
                            CTS_NO_OPTIONS);
    if (curr_token == tok_quest_mark) {
      question_count++;
    } else if (curr_token == tok_colon) {
      if (question_count > 0) {
        question_count--;
      } else {
        result = tok_colon;
        break;
      }  /* if */
    } else if (curr_token == tok_rparen || curr_token == tok_end_of_source) {
      result = curr_token;
      break;
    } else {
      check_assertion(curr_token == tok_semicolon);
      if (question_count == 0) {
        result = tok_semicolon;
        break;
      } else {
        result = tok_error;
        break;
      }  /* if */
    }  /* if */
    (void)get_token();
  }  /* for */
  wrapup_disambig_state(&state);
  return result;
}  /* find_for_loop_separator */

#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED

void prescan_decl_modifiers(void)
/*
Skip over near, far, and any Microsoft extended decl modifiers that may be
present.
*/
{
  prescan_extended_decl_modifiers(DFS_NO_FLAGS);
}  /* prescan_decl_modifiers */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean static_member_next(void)
/*
Return TRUE if the upcoming token sequence looks like a static member
declaration.
*/
{
  a_disambig_state  state;

  /* Initialize the disambiguation state block. */
  init_disambig_state(&state, /*check_if_is_decl=*/FALSE,
                      /*suppress_packs=*/FALSE,
                      /*cache_tokens=*/TRUE);
  state.find_static_specifier_only = TRUE;
  prescan_declaration(&state, DFS_REAL_DECLARATOR_ALLOWED,
                     /*is_top_level=*/TRUE);
  wrapup_disambig_state(&state);
  return state.static_specifier_seen;
}  /* static_member_next */


a_boolean elaborated_cli_typeid_next(void)
/*
Return TRUE if the upcoming tokens are something like "ref class T::typeid"
for a class type T.  Issue an error if the elaboration does not match the
qualifier class type.
The use of an elaborated name here is not valid according to ECMA-372, but
Microsoft compilers accept it nonetheless.
*/
{
  a_boolean  result = FALSE;

  if (is_class_type_keyword(curr_token)) {
    a_source_position  pos_class_key;
    a_token_kind       class_key = curr_token;
    a_disambig_state   state;
    /* Use init_disambig_state as a convenient way to manage background token
       caching. */
    init_disambig_state(&state, /*check_if_is_decl=*/FALSE,
                        /*suppress_packs=*/FALSE,
                        /*cache_tokens=*/TRUE);
    pos_class_key = pos_curr_token;
    /* Advance past the keyword. */
    (void)get_token();
    /* Check whether the next tokens form a T::typeid construct. */
    if ((curr_token == tok_identifier || curr_token == tok_colon_colon) &&
        !is_generalized_identifier_start(GID_CHECK_TAG_NAME_FLAGS) &&
        curr_token == tok_cli_typeid) {
      a_type_ptr  tp = skip_typerefs(locator_for_curr_id.parent.class_type);
      /* Ensure that the T::typeid construct is for a matching class type or
         a template parameter type. */
      if (is_class_struct_union_type(tp)) {
        a_boolean  mismatch = FALSE;
        switch (class_type_supp(tp)->cli_class_type_kind) {
          case cctk_standard:
            mismatch = class_key != tok_class && class_key != tok_struct &&
                       class_key != tok_union && class_key != tok_interface;
            break;
          case cctk_value:
            mismatch = class_key != tok_value_class &&
                       class_key != tok_value_struct;
            break;
          case cctk_ref:
            mismatch = class_key != tok_ref_class &&
                       class_key != tok_ref_struct;
            break;
          case cctk_interface:
            mismatch = class_key != tok_interface_class &&
                       class_key != tok_interface_struct;
            break;
          default:
            unexpected_condition();
        }  /* switch */
        if (mismatch) {
          pos_sy_error(ec_conflicting_cli_class_type_kinds, &pos_class_key,
                       symbol_for(tp));
        }  /* if */
        result = TRUE;
      } else if (is_template_param_type(tp)) {
        result = TRUE;
      }  /* if */
    }  /* if */
    wrapup_disambig_state(&state);
  }  /* if */
  return result;
}  /* elaborated_cli_typeid_next */
        
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

