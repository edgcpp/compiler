/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

lookup.c - Name lookup routines.

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Although lookup.c is not really a "declaration processing file",
   it turns out that most of the header files it needs are in decl_hdrs.h. */
#include "decl_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


static a_symbol_ptr find_nested_type_symbol(a_symbol_locator *locator)
/*
Find a "semivisible" symbol for a member type that is no longer in scope
(nested class, typedef name, or enumeration).  Such symbols are not found
by the normal lookup procedure but are visible according to the "nested
class anachronism" (ARM 18.3.5) which, it turns out, applies in cfront to
typedefs and enums as well.  They are visible as though they had been
entered in the file, function or block scope that is the containing
nonclass scope (i.e., the declaration scope of the parent class).  Look
for a qualifying symbol on the inactive list for the specified symbol
locator.  In the case of an ambiguity, return NULL.
*/
{
  a_symbol_ptr            sym, nested_type_sym = NULL;
  a_type_ptr              tp;
  a_scope_stack_entry_ptr ssep;
  a_scope_number          effective_scope, effective_scope_of_nested_type;

  db_enter(4, "find_nested_type_symbol");
  if (allow_anachronisms &&
      locator->symbol_header->any_nested_types_on_inactive_list) {
    sym = inactive_symbol_list_from_locator(*locator);
    effective_scope_of_nested_type = NO_SCOPE_NUMBER;
    for (; sym != NULL; sym = sym->next) {
      if (is_tag_symbol(sym) || sym->kind == (a_symbol_kind)sk_type) {
        /* Found a type symbol, but is it nested? */
        if (sym->is_class_member) {
          tp = sym_parent_class(sym);
          /* It is a nested member type.  Pop out to the outermost parent
             class to get the nonclass scope number. */
          while (tp->source_corresp.is_class_member) {
            tp = parent_class_of(tp);
          }  /* while */
          /* Ignore classes that are namespace members. */
          if (is_namespace_member(tp)) continue;
          /* The effective scope is the innermost file, function, or block
             scope in which parent class is declared. */
          effective_scope = symbol_for(tp)->decl_scope;
          if (effective_scope == effective_scope_of_nested_type) {
            /* We have an ambiguity.  We could issue a warning or error, but
               we choose to recognize the nested class anachronism only when
               it is "legally" used -- we don't want a message that says,
               "You're doing something nonstandard and what's more you aren't
               even doing it correctly."  Especially since the user may not
               have been intending to do any such thing.  However, this may
               introduce some differences with Cfront (2.1), which is wedded
               to the nested class anachronism in surprising ways. */
            nested_type_sym = NULL;
            break;
          }  /* if */
          /* If the effective scope is still active on the scope stack sym is
             a match.  Look through the scope stack for scope number. */
          ssep = &scope_stack[depth_scope_stack];
          for (;;) {
            if (nested_type_sym == NULL) {
              if (ssep->number == effective_scope) {
                /* sym's effective scope is still active, so sym is a match. */
                nested_type_sym = sym;
                effective_scope_of_nested_type = effective_scope;
                /* Break out of the inner loop, but keep looking at symbols
                   in case there's an ambiguity. */
                break;
              } else {
                /* Continue through the scope stack till the scope is found,
                   if it's still active. */
              }  /* if */
            } else {
              /* We already have a symbol but ambiguity has been ruled out.
                 If the current symbol also has an effective scope that's
                 that's still active, keep the symbol whose effective scope
                 is closer to the top of the scope stack. */
              if (ssep->number == effective_scope) {
                /* sym's effective scope is still active and is higher. */
                nested_type_sym = sym;
                effective_scope_of_nested_type = effective_scope;
                break;
              } else if (ssep->number == effective_scope_of_nested_type) {
                /* Other symbol's effective scope is higher.  Break out of
                   the inner loop but keep looking at symbols. */
                break;
              }  /* if */
            }  /* if */
            /* End the loop when we reach the bottom of the scope stack. */
            if (ssep == &scope_stack[DEPTH_OF_FILE_SCOPE]) break;
            ssep--;
          }  /* for */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
  return nested_type_sym;
}  /* find_nested_type_symbol */


a_scope_number scope_depth_for_synth_namespace_symbol(void)
/*
Determine the depth at which a synthesized namespace symbol for the
current context should be entered or found.  The scope used is

- the innermost scope containing a using-directive, or

- if no using-directives are active, but we are in a template instantiation,
  the innermost instantiation scope,

- otherwise, the file scope.
*/
{
  a_scope_depth	depth;

  depth = depth_innermost_instantiation_scope;
  if (depth == NO_SCOPE_DEPTH) {
    for (depth = depth_scope_stack;
      depth != DEPTH_OF_FILE_SCOPE; --depth) {
      if (scope_stack[depth].active_using_directives != NULL) break;
    }  /* for */
  }  /* if */
  return depth;
}  /* scope_depth_for_synth_namespace_symbol */


static
a_symbol_ptr find_synthesized_projection_symbol(
                              a_symbol_locator          *locator,
                              an_id_lookup_options_set	options,
                              a_boolean			qualified_lookup,
			      a_namespace_ptr		qualifier_namespace)
/*
Look for a synthesized projection symbol from a previous lookup that
can be reused to capture the results of this lookup.  locator
represents the symbol to be found.

qualified_lookup is TRUE if the symbol being found is the result of
a namespace or file scope qualified lookup.  For qualified lookups
qualifier_namespace points to the namespace in which the lookup is
being done, or is NULL for a file scope lookup.  options specifies
the options being used for the lookup.
*/
{
  a_symbol_ptr		sym = NULL;
  a_symbol_header_ptr	sym_hdr = locator->symbol_header;

  if (is_reusable_using_directive_lookup(options)) {
    /* Only search if the lookup options represent a lookup whose results
       may be reused. */
    a_boolean		must_be_class_or_namespace
                             = (options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0;
    a_boolean		must_be_tag = (options & IDL_MUST_BE_TAG) != 0;
    a_boolean		must_be_class = (options & IDL_MUST_BE_CLASS) != 0;
    a_boolean		must_be_namespace
                                      = (options & IDL_MUST_BE_NAMESPACE) != 0;
    a_boolean		tentative_type_lookup
                                  = (options & IDL_TENTATIVE_TYPE_LOOKUP) != 0;
    a_scope_number	scope_number = NO_SCOPE_NUMBER;
    a_scope_depth	scope_depth;
    a_boolean		instantiation_context_lookup =
                                    (options & IDL_INSTANTIATION_CONTEXT) != 0;
    /* Synthesized namespace symbols are entered into the file scope, except
       for those referenced from template instantiations, which are entered
       in the template instantiation scope. */
    scope_depth = scope_depth_for_synth_namespace_symbol();
    scope_number = scope_stack[scope_depth].number;
    /* Loop through the list of other symbols.  Look for symbols marked
       as synthesized namespace projection symbols. */
    for (sym = sym_hdr->other_symbols; sym != NULL; sym = sym->next) {
      /* Look for symbols whose lookup characteristics match the current
         lookup.  The file_scope_number test is used to exclude file scope
         symbols from other translation units. */
      if (sym->synthesized_namespace_projection &&
          (a_boolean)sym->qualified_lookup == qualified_lookup &&
          /* Note that same_entities must not be used for this test. */
          sym_parent_namespace_or_null(sym) == qualifier_namespace &&
          (qualifier_namespace != NULL ||
           sym->decl_scope == file_scope_number) &&
          (a_boolean)sym->must_be_class_or_namespace_lookup ==
                                                 must_be_class_or_namespace &&
          (a_boolean)sym->instantiation_context_lookup ==
                                              instantiation_context_lookup &&
          (a_boolean)sym->tentative_type_lookup == tentative_type_lookup &&
          (a_boolean)sym->must_be_namespace_lookup == must_be_namespace &&
          (a_boolean)sym->must_be_class_lookup == must_be_class &&
          (a_boolean)sym->must_be_tag_lookup == must_be_tag) {
        /* If this is not a qualified lookup, the decl_scope of the symbol
           must match the current scope. */
        if (qualified_lookup) {
          break;
        } else {
          if (sym->decl_scope == scope_number) break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (sym != NULL) {
   /* Reset the declaration sequence number of the symbol.  The symbol will
      be assigned a new declaration sequence number later. */
   sym->decl_seq = NO_DECL_SEQUENCE_NUMBER;
   if (sym->kind == (a_symbol_kind)sk_namespace_projection &&
        !is_function_or_template_symbol(fundamental_symbol_of(sym))) {
      /* A previous lookup created a synthesized namespace
         projection symbol.  If the fundamental symbol is not a
	 function symbol, clear the fundamental symbol pointed
         to.  This is needed in case something has changed that would
         cause a different symbol to be found. */
      sym->variant.namespace_projection.fundamental_symbol = NULL;
    }  /* if */
  }  /* if */
  return sym;
}  /* find_synthesized_projection_symbol */


static inline void load_lazy_symbols_if_needed(a_scope_ptr      scope,
                                               a_symbol_locator *locator)
/*
If the kind associated with scope is a namespace or file scope, call a
routine to see if there are symbols that should be made visible in scope.
locator points to the symbol locator for the symbol being looked up.  Note
that scope can be NULL.

WARNING: Lazy loading of symbols may cause scope_stack to be re-allocated,
invalidating any saved pointers into the scope stack.  Any such pointers will
need to be reset after a call to this routine.
*/
{
  /* Lazy loading may need to be selectively disabled + re-enabled at times -
     ensure that we only lazy load when lazy loading is enabled. */
  if (lazy_symbols_may_be_visible && scope != NULL &&
      locator->symbol_header->deferred_module_entries != NULL) {
    if (scope_is(scope, sck_file) ||
        scope_is(scope, sck_namespace)) {
#if CHECKING
      a_const_char *orig_curr_source_line = curr_source_line;
      a_const_char *orig_start_of_curr_token = start_of_curr_token;
      a_const_char *orig_end_of_curr_token = end_of_curr_token;
#endif /* CHECKING */

      define_names_from_scope(scope, locator->symbol_header);
#if CHECKING
      /* If these assertions fail, modules code has failed to preserve the
         lexical state of the current token and needs to be corrected to do
         so. */
      check_assertion(orig_curr_source_line == curr_source_line);
      check_assertion(orig_start_of_curr_token == start_of_curr_token);
      check_assertion(orig_end_of_curr_token == end_of_curr_token);
#endif /* CHECKING */
    }  /* if */
  }  /* if */
}  /* load_lazy_symbols_if_needed */

namespace {

/*
An internal class for representing flags for a scope id lookup, and testing
acceptance of symbols.
*/
struct a_scope_id_lookup_options_set {
  inline a_scope_id_lookup_options_set(an_id_lookup_options_set options);

  inline a_boolean accepts(a_name_space_kind required_name_space_kind,
                           a_symbol_ptr      sym,
                           a_symbol_ptr      fund_sym) const;

  const a_boolean must_be_tag;
  const a_boolean projection_allowed;
};  /* a_scope_id_lookup_options_set */

inline a_scope_id_lookup_options_set::a_scope_id_lookup_options_set(
                                              an_id_lookup_options_set options)
  : must_be_tag((options & IDL_MUST_BE_TAG) != 0),
    projection_allowed((options & IDL_PROJ_SYMBOL_ALLOWED) != 0)
/*
Construct a scope id lookup options set from the given id lookup options set.
*/
{
  check_assertion_str2((options & ~(IDL_MUST_BE_TAG |
                                    IDL_MUST_BE_CLASS |
                                    IDL_PROJ_SYMBOL_ALLOWED |
                                    IDL_HIDDEN_NAME_LOOKUP)) == 0,
                       "curr_scope_id_lookup:", "invalid_option");
}  /* a_scope_id_lookup_options_set */


inline a_boolean a_scope_id_lookup_options_set::accepts(
                                    a_name_space_kind required_name_space_kind,
                                    a_symbol_ptr      sym,
                                    a_symbol_ptr      fund_sym) const
/*
Return TRUE if sym and its accompanying fundamental symbol are acceptable
symbols for the given required name space kind and the current lookup options.
*/
{
  a_boolean result = TRUE;

  if (must_be_tag && !is_tag_symbol(fund_sym)) {
    result = FALSE;
  } else if (name_space_for_symbol_kind[(int)sym->kind] !=
                                                    required_name_space_kind) {
    result = FALSE;
  } else if (!projection_allowed && sym->kind ==
                                                (a_symbol_kind)sk_projection) {
    result = FALSE;
  }  /* if */
  return result;
}  /* accepts */

}  /* namespace */

a_symbol_ptr curr_scope_id_lookup(a_symbol_locator         *locator,
                                  an_id_lookup_options_set options)
/*
Lookup, in the current scope, the identifier indicated by *locator and
return a pointer to the symbol found, or NULL if the symbol is not found.
options contains bits indicating special restrictions, i.e., the symbol
must a tag.  Projection symbols are only considered in the lookup if
IDL_PROJ_SYMBOL_ALLOWED is specified in options.
*/
{
  a_symbol_ptr                  sym;
  a_scope_number                scope_number;
  a_scope_id_lookup_options_set scope_lookup_opts(options);
  a_scope_stack_entry_ptr       ssep;
  a_name_space_kind             required_name_space_kind = nsk_other;

  /* In C mode, a "must be tag" lookup only considers symbols in the tag
     name space kind. */
  if (C_mode() && scope_lookup_opts.must_be_tag) {
    required_name_space_kind = nsk_tag;
  }  /* if */
  sym = locator->specific_symbol;
  if (is_error_locator(*locator)) {
    /* The locator is an error locator, so return NULL (i.e., no symbol
       found). */
    sym = NULL;
  } else if (sym != NULL) {
#if CHECKING
    a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
    check_assertion(scope_lookup_opts.accepts(required_name_space_kind, sym,
                                              fund_sym));
#endif /* CHECKING */
    /* The locator is for a specific symbol, so return the symbol for it. */
  } else {
    ssep = scope_stack_entry_for(decl_scope_level);
    load_lazy_symbols_if_needed(ssep->il_scope, locator);
    /* Lazy loading of symbols may cause scope_stack to be re-allocated,
       invalidating ssep. */
    ssep = scope_stack_entry_for(decl_scope_level);
    /* Look for a symbol in the current scope for which the kind matches that
       of the scope level specified by the caller. */
    scope_number = ssep->number;
    sym = symbol_list_from_locator(*locator);
    for (; sym != NULL; sym = sym->next) {
      a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
      if (sym->decl_scope == scope_number &&
          is_symbol_currently_lookup_visible(sym) &&
          scope_lookup_opts.accepts(required_name_space_kind, sym, fund_sym)) {
         /* Found it. */
         break;
      }  /* if */
    }  /* for */
    if (sym == NULL &&
        (ssep->kind == (a_scope_kind)sck_namespace_extension ||
        (ssep->kind == (a_scope_kind)sck_file))) {
      /* If no symbol was found on the active list, and this is a namespace
         extension or a file scope, then look on the inactive list too.  This
         doesn't have to be done for original namespace scopes because their
         symbols will still be on the active list. */
      a_symbol_ptr tag_symbol = NULL;
      for (a_module_ptr mod : curr_lookup_modules()) {
        for (sym = find_symbol_list_in_table(curr_lookup_table(ssep, mod),
                                             locator->symbol_header);
             sym != NULL;
             sym = sym->next_in_lookup_table) {
          a_symbol_ptr fund_sym = fundamental_symbol_of(sym);
          if (scope_lookup_opts.accepts(required_name_space_kind, sym,
                                        fund_sym)) {
            /* Found an acceptable symbol. */
            /* If the symbol is a tag symbol, there's the possibility that
               there is a non-type symbol in the same scope later in the list
               (because the inactive list is not ordered in any way).  Save the
               tag symbol and keep looking.  If nothing else turns up,
               use the tag symbol. */
            if (!is_tag_symbol(fund_sym)) {
              tag_symbol = NULL;
              goto found_sym;
            }  /* if */
            if (tag_symbol != NULL &&
                symbol_is(tag_symbol, sk_namespace_projection)) {
              /* If a using-declaration to a tag symbol is followed by
                 an actual tag symbol, ignore the first one. */
              tag_symbol = NULL;
            }  /* if */
            /* If a tag symbol is followed by a projection to a different tag,
               use the first symbol. */
            check_assertion_or_expect_error(tag_symbol == NULL ||
                                            symbol_is(sym, sk_projection));
            if (tag_symbol == NULL) tag_symbol = sym;
          }  /* if */
        }  /* for */
      }  /* for */
      /* We reached the end of the list.  If there is a tag symbol saved
         within the loop, use it. */
      if (tag_symbol != NULL) sym = tag_symbol;
    }  /* if */
found_sym:
    locator->specific_symbol = sym;
  }  /* if */
  /* If the symbol is a projection symbol, reduce it to the fundamental
     symbol.  The specific_symbol in the locator stays pointing to the
     projection symbol. */
  if (sym != NULL) reduce_projection_symbol_to_fundamental_symbol(sym);
  return sym;
}  /* curr_scope_id_lookup */


#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG

static a_symbol_ptr check_for_cfront_name_lookup_bug(
					 a_type_ptr                class_type,
					 a_symbol_ptr	           sym,
                                         a_symbol_locator          *locator,
					 an_id_lookup_options_set  options)
/*
Cfront 2.1 has a bug that causes a global identifier to be found when
a member of a class or one of its base classes should actually be found.
The following code illustrates an instance in which the bug occurs:

  struct B   {
          void func(const char*);	// Needs to be here
  };

  struct D : public B {
  public:
          D();
          void Init(const char* );
  };

  struct func {
          func( const char* msg);
  };

  D::D(){}

  void D::Init(const char* t)
  {
          new func(t);
  }

For the bad lookup to occur:

1. A member in a base class must have the same name as an identifier
   at the global scope.  Any member kind is OK -- it can be a function,
   static data member, or nonstatic data member.  Member type names don't
   apply because a nested type will be promoted to the global scope by
   cfront which disallows a later declaration of a type with the same name
   at the global scope.

2. The declaration of the global scope name must occur between the declaration
   of the derived class and the declaration of either an out-of-line
   constructor or destructor.  The global scope name must be a type name.

3. No other member function definition -- even one for an unrelated class
   may appear between the destructor and the offending reference.
   This has the effect that the bad lookup applies to only one class at
   any given point in time.

The global variable last_ctor_or_dtor_sym is set by function_declaration
when the body of a constructor or destructor that is defined outside of
the class definition is processed.  This field is cleared when any other
member function is defined.
*/
{
  a_derivation_step_ptr	path = NULL;
  an_access_specifier   access;
  a_boolean             ambiguous, any_using_decl;
  a_symbol_ptr          new_sym = sym;

  /* Before this routine is called we will have already verified that the
     lookup terminated in the class reactivation scope for the same class
     as indicated by last_ctor_or_dtor_sym.  This means that we are
     in a member function definition (or static data member definition) of
     a class for which a constructor or destructor was just defined. */
  if (sym == NULL || sym->kind != (a_symbol_kind)sk_projection) {
    /* If sym is NULL it means that a symbol was found but it was not a
       type when a tentative type lookup was being done.  If the symbol
       is not a projection symbol, then the name was found in the
       derived class.  For the bug to occur, the name must be defined
       in the base class even if the name was redefined in the derived
       class.  Look for the name in a base class. */
    a_boolean  unambiguous_injected_template = FALSE;

    new_sym = find_progenitor_symbol(class_type, locator,
                                     IDL_NO_OPTIONS,
                                     /*look_in_dependent_bases=*/FALSE,
                                     /*look_in_dependent_interfaces=*/FALSE,
                                     &path, &access,
                                     &ambiguous, &any_using_decl,
                                     &unambiguous_injected_template);
  }  /* if */
  if (new_sym != NULL) {
    a_symbol_ptr  fund_sym = fundamental_symbol_of(new_sym);
    if (!(is_type_symbol(fund_sym) &&
          type_symbol_type(fund_sym)->
                       use_cfront_transitional_nested_type_name_mangling)) {
      /* Names that are in essence promoted to file scope are not
         considered. */
      a_symbol_ptr file_scope_sym;
      /* new_sym must now be a projection symbol or progenitor symbol.  Look
         for a symbol with the same name at file scope. */
      check_assertion(class_type != sym_parent_class(fund_sym));
      file_scope_sym = file_scope_id_lookup(il_header.primary_scope,
                                            locator, options);
      if (file_scope_sym != NULL && is_type_symbol(file_scope_sym)) {
        /* A file scope symbol was found.  For the incorrect lookup to be
           done the file scope symbol must have been declared after the
           derived class but before the most recent constructor or
           destructor body. */
        a_symbol_ptr	class_sym;
        class_sym = symbol_for(class_type);
        if ((compare_source_positions(&file_scope_sym->decl_position,
                                      &class_sym->decl_position) > 0) &&
            (compare_source_positions(&file_scope_sym->decl_position,
                                      &last_ctor_or_dtor_sym->
                                                       decl_position) < 0)) {
          sym = file_scope_sym;
          pos_sy2_warning(ec_cfront_name_lookup_bug,
                          &locator->source_position,
                          sym, new_sym);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return (sym);
}  /* check_for_cfront_name_lookup_bug */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */


static a_type_ptr create_proxy_class(a_symbol_ptr		orig_sym,
				     a_source_correspondence	*scp,
				     a_boolean			is_generic)
/*
Create a proxy class for the entity specified by orig_sym and scp.
Creation of the proxy class consists of allocating and initializing the
class and assigning a scope number. The class type is created the first
time that a template parameter is used in a context in which a class type
is required.  This includes use in a qualified name lookup where the
template parameter is used as the class type, and use as a base class.
Proxy classes are also created for C++/CLI type parameters to represent the
type specified by the constraints.  is_generic is TRUE when the proxy class
is being created for a C++/CLI type parameter.  Proxy classes are also used
in Microsoft mode to represent namespace members used in dependent
decltypes in cases where the member has not yet been declared.
*/
{
  a_type_ptr				type;
  a_symbol_ptr				sym;
  a_class_symbol_supplement_ptr		cssp;
  a_type_ptr				proxy_class;

  if (orig_sym == NULL) {
    /* No template parameter symbol.  This is the case when getting the
       proxy class for type_of_unknown_templ_param_nontype. */
    sym = make_unnamed_tag_symbol((a_symbol_kind)sk_class_or_struct_tag,
                                  &null_source_position);
  } else {
    /* Create a symbol for the class.  The symbol will have the same name
       as the template parameter symbol.  mark_declared is not called
       because this symbol is not visible to the user. */
    sym = alloc_symbol((a_symbol_kind)sk_class_or_struct_tag,
                       orig_sym->header,
                       &orig_sym->decl_position);
  }  /* if */
  /* The class will be considered to be at file scope.  If this is changed
     to be some other scope then set_source_corresp_with_scope_depth may
     need to be called because set_source_corresp requires that the
     decl_scope of the symbol still be an active scope. */
  sym->decl_scope = file_scope_number;
  /* Create the type for the class. */
  type = alloc_type((a_type_kind)tk_class);
  if (!is_generic) {
    /* Set the size and alignment so that the type will be considered to be
       complete.  No scope is created until members of the proxy class are
       needed. */
    type->size = 1;
    type->alignment = 1;
    type->incomplete = FALSE;
  } else {
    /* The proxy class associated with a generic parameter behaves much like
       a real class, but it cannot always be completed at the point of the
       generic declaration since its constraints may not be complete at that
       point.  It will be completed by complete_generic_constraint_type.
       See also create_generic_constraint_types. */
  }  /* if */
  set_source_corresp(&(type->source_corresp), sym);
  type->source_corresp.member_of_unknown_base = scp->member_of_unknown_base;
  type->source_corresp.qualified_unknown_base_member =
                                            scp->qualified_unknown_base_member;
  type->variant.class_struct_union.proxy_class = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  type->source_corresp.member_of_unknown_super = scp->member_of_unknown_super;
  type->variant.class_struct_union.is_generic_constraint = is_generic;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  sym->variant.class_struct_union.type = type;
  if (scp->is_class_member) {
    set_class_membership(sym, &type->source_corresp, scp_parent_class(scp));
  }  /* if */
  proxy_class = type;
  /* Set the scope number. */
  cssp = symbol_supplement_for_class(type);
  cssp->member_decl_scope = take_next_scope_number();
  /* Note that while generic definitions are nonreal, the proxy classes for
     constraints are not. */
  type->variant.class_struct_union.is_nonreal_class = !is_generic;
  if (prototype_instantiations_in_il || is_generic) {
    /* When prototype instantiations are included in the IL, add the proxy
       class to the IL.  Also do this for proxy classes created for
       C++/CLI generic constraint types. */
    add_to_types_list(type, DEPTH_OF_FILE_SCOPE);
  }  /* if */
  return proxy_class;
}  /* create_proxy_class */


a_type_ptr proxy_class_for_template_param(a_type_ptr   type)
/*
Return the proxy class associated with the template parameter or dependent
decltype specified by type.  If one does not already exist, one is
created.
*/
{
  a_type_ptr	*proxy_class;
  a_boolean	is_generic = FALSE;

  /* If the original type is a template parameter, get a pointer to the
     template parameter.  In any case, get a pointer to the proxy class
     pointer. */
  if (type->kind == (a_type_kind)tk_template_param) {
    proxy_class = &type->variant.template_param.extra_info->class_type;
    is_generic = type->variant.template_param.is_generic_param;
  } else {
    check_assertion(type->kind == (a_type_kind)tk_typeref);
    proxy_class = &type->variant.typeref.extra_info->proxy_class;
  }  /* if */
  /* If the proxy class does not exist yet, create it now. */
  if (*proxy_class == NULL) {
    /* Assigning through *proxy_class ensures that "type" is also updated to
       point to the newly created class. */
    *proxy_class = create_proxy_class(symbol_for(type), &type->source_corresp,
                                      is_generic);
    /* Allow users of the proxy class to find the associated template
       parameter or decltype type. */
    class_type_supp(*proxy_class)->proxy_of_type = type;
  }  /* if */
  return *proxy_class;
}  /* proxy_class_for_template_param */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_type_ptr proxy_class_for_namespace(a_namespace_ptr	nsp)
/*
Return the proxy class associated with the namespace specified by nsp.
If one does not already exist, one is created.  In certain template
dependent contexts, the Microsoft compiler accepts constructs like
"decltype(N::x)" when x has not yet been declared in namespace N.
To emulate this behavior we create the member in a proxy class associated
with the namespace.
*/
{
  a_type_ptr	proxy_class;

  proxy_class = nsp->proxy_class;
  if (proxy_class == NULL) {
    a_symbol_ptr	orig_sym = symbol_for(nsp);
    proxy_class = create_proxy_class(orig_sym, &nsp->source_corresp,
                                     /*is_generic=*/FALSE);
    nsp->proxy_class = proxy_class;
  }  /* if */
  return proxy_class;
}  /* proxy_class_for_namespace */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_symbol_ptr create_unknown_function_symbol(
				a_symbol_header_ptr	sym_hdr,
				a_type_ptr		parent_class,
				a_namespace_ptr		parent_namespace,
				a_boolean		is_qualified_name,
				a_constant_ptr		*unk_func_constant)
/*
Create a ck_constant entry of kind tpck_unknown_function, and an sk_constant
symbol that points to the constant.  These constants are used during
prototype instantiations to represent functions in dependent calls.
The symbol is created using the symbol header information from sym_hdr
and the parent information specified by parent_class and parent_namespace.
The is_qualified_name field in the constant is set as indicated by
is_qualified_name.  A pointer to the unknown-function constant is returned
in *unk_func_constant.
*/
{
  /* Create a ck_template_param constant.  We don't know the type of the
     constant so we use a special template parameter type that represents
     the type of an unknown constant. */
  a_constant_ptr		constant;
  a_scope_depth			depth = NO_SCOPE_DEPTH;
  a_symbol_ptr			sym;
  a_source_correspondence	*scp;

#if RECORD_SCOPE_DEPTH_IN_IL
  {
    a_source_correspondence	*parent_scp = NULL;
    /* Get the source correspondence entry associated with the parent class
       or namespace, if any. */
    if (parent_class != NULL) {
      parent_scp = &parent_class->source_corresp;
    } else {
      if (parent_namespace != NULL) {
        parent_scp = &parent_namespace->source_corresp;
      }  /* if */
    }  /* if */
    if (parent_scp != NULL) {
      depth = parent_scp->scope_depth;
    }  /* if */
  }
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
  /* Create a symbol for the member.  mark_declared is not called
     because this symbol is not visible to the user. */
  sym = alloc_symbol((a_symbol_kind)sk_constant, sym_hdr,
                     &null_source_position);
  constant = fs_constant((a_constant_repr_kind)ck_template_param);
  set_template_param_constant_kind(
              constant, (a_template_param_constant_kind)tpck_unknown_function);
  sym->variant.constant = constant;
  constant->type = type_of_unknown_templ_param_nontype;
  constant->variant.template_param.is_qualified_name = is_qualified_name;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (sym_hdr->is_cli_operator) {
    /* Not a C++ overloaded operator. */
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    /* If this represents an overloaded operator, set the specific kind. */
    constant->variant.template_param.variant.unknown_function.
                                         opname_kind = sym_hdr->variant.opname;
  }  /* if */
  *unk_func_constant = constant;
  scp = &constant->source_corresp;
  set_source_corresp_with_scope_depth(scp, sym, depth);
  if (parent_class != NULL) {
    set_class_membership(sym, scp, parent_class);
  } else if (parent_namespace != NULL) {
    set_namespace_membership(sym, scp, parent_namespace);
  }  /* if */
  sym->is_unknown_function = TRUE;
  /* Set the decl_scope to the file scope of the current translation unit
     so that we can determine the translation unit with which this
     unknown function symbol is associated. */
  sym->decl_scope = curr_translation_unit->primary_scope->number;
  return sym;
}  /* create_unknown_function_symbol */


a_symbol_ptr find_unknown_function_symbol(
					a_symbol_ptr	orig_sym,
					a_boolean	is_qualified_name)
/*
Find an sk_constant symbol that points to a constant of kind
tpck_unknown_function.  The symbol name must match that of orig_sym
and must have the same parent scope.  The is_qualified_name flag in
the constant must match is_qualified_name.

If a matching symbol cannot be found, one is created.  The list of
previously created symbols is included in the "other symbols" list
of the symbol header.
*/
{
  a_symbol_ptr	sym;

  /* Look for a previously created unknown function symbol. */
  for (sym = orig_sym->header->other_symbols; sym != NULL; sym = sym->next) {
    if (sym->is_unknown_function &&
        sym->variant.constant->variant.template_param.is_qualified_name ==
                                                           is_qualified_name &&
        sym->variant.constant->variant.template_param.variant.
                            unknown_function.symbol->kind == orig_sym->kind &&
        trans_unit_for_symbol(sym) == curr_translation_unit) {
      if (sym->is_class_member == orig_sym->is_class_member) {
        if (sym->is_class_member) {
          if (sym_parent_class(sym) == sym_parent_class(orig_sym)) {
            /* The symbols have the same parent class. */
            break;
          }  /* if */
        } else {
          if (sym_parent_namespace_or_null(sym) ==
                                     sym_parent_namespace_or_null(orig_sym)) {
            /* The symbols have the same parent namespace (including the case
               where both have no namespace). */
            break;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  if (sym == NULL) {
    /* No symbol was found -- create one now. */
    a_symbol_header_ptr		hdr;
    a_type_ptr			parent_class = NULL;
    a_namespace_ptr		parent_namespace = NULL;
    a_constant_ptr		con;
    if (orig_sym->is_class_member) {
      parent_class = sym_parent_class(orig_sym);
    } else {
      parent_namespace = sym_parent_namespace_or_null(orig_sym);
    }  /* if */
    sym = create_unknown_function_symbol(orig_sym->header, parent_class,
                                         parent_namespace, is_qualified_name,
                                         &con);
    check_assertion(is_unknown_function_constant(con));
    /* Save the original symbol (probably an overload set) for use later
       in copy_type_with_substitution. */
    con->variant.template_param.variant.unknown_function.symbol = orig_sym;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cli_or_cx_enabled && symbol_is(orig_sym, sk_member_function)) {
      /* For a C++/CLI property or event accessor, save a pointer to the
         property description. */
      a_routine_ptr rp = orig_sym->variant.routine.ptr;
      if (rout_is_cli_accessor(rp)) {
        con->variant.template_param.variant.unknown_function
                .property_or_event_descr = rp->variant.property_or_event_descr;
        con->variant.template_param.variant.unknown_function.special_kind =
                                                              rp->special_kind;
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    hdr = sym->header;
    /* Link this symbol onto the other symbols list. */
    sym->next = hdr->other_symbols;
    hdr->other_symbols = sym;
    if (orig_sym->kind == (a_symbol_kind)sk_class_template) {
      /* Copy the flags indicating whether the unknown function is a member
         of an unknown base and, if so, whether the name was qualified. */
      a_template_ptr tp = orig_sym->variant.template_info->il_template_entry;
      con->source_corresp.member_of_unknown_base =
                                     tp->source_corresp.member_of_unknown_base;
      con->source_corresp.qualified_unknown_base_member =
                              tp->source_corresp.qualified_unknown_base_member;
    }  /* if */
  }  /* if */
  return sym;
}  /* find_unknown_function_symbol */


/*
Macro that returns the symbol kind for a nonreal member created for the
current lookup options.  The symbol is created as a class template
if a "treat as template ID" lookup is done.  The symbol is created as a
type if the lookup is a "must be class or namespace", "must be tag" or
"typename lookup".  In addition, if "implicit typename" is enabled, we
also force the member to be a type when doing a "tentative type" lookup
(but not if the name being looked up is something that could not be
a typename, like a destructor).

Implicit typename mode is used to compile code that was not written
using "typename".  If the symbol is not considered to be a class
template or a type, then it is created as a constant.

In Microsoft mode, a tentative template lookup for a member of an unknown
base results in the creations of a class template symbol.
*/
/*lint -emacro(641,nonreal_member_symbol_kind)*/
#define nonreal_member_symbol_kind(locator, options)		\
  ((a_symbol_kind)(((options & IDL_TREAT_AS_TEMPLATE_ID) ||	\
    (microsoft_mode && use_implicit_typename() &&		\
     (options & IDL_TENTATIVE_TEMPLATE_LOOKUP) &&		\
     (options & IDL_MEMBER_OF_UNKNOWN_BASE)))			\
    ? sk_class_template						\
    : 								\
      (options & IDL_MUST_BE_CLASS_OR_NAMESPACE ||		\
       options & IDL_MUST_BE_TAG ||				\
       options & IDL_MUST_BE_CLASS ||				\
       options & IDL_IMPLICIT_TYPENAME_CONTEXT ||			\
       options & IDL_TYPENAME_LOOKUP ||				\
       (use_implicit_typename() && (options & IDL_TENTATIVE_TYPE_LOOKUP))) && \
       !(locator)->is_destructor_name &&			\
       !(locator)->is_conversion_name &&			\
       !(locator)->is_operator_name				\
        ? sk_type						\
        :							\
          sk_constant))


static a_boolean acceptable_nonreal_class_member_symbol(
				a_symbol_ptr			sym,
				an_id_lookup_options_set	options,
				a_symbol_locator		*locator)
/*
Return TRUE if the symbol specified by "sym" matches the nonreal symbol
that would be created to represent the symbol described by "locator"
and the lookup options "options".
*/
{
  a_boolean	result = FALSE;

  if (sym->kind == nonreal_member_symbol_kind(locator, options)) {
    /* If the symbol represents a member of an unknown base class, it
       is a match only if the current lookup is also for a member of
       an unknown base. */
    a_source_correspondence	*scp;
    scp = source_corresp_entry_for_symbol(sym);
    if (scp->member_of_unknown_base ==
                               ((options & IDL_MEMBER_OF_UNKNOWN_BASE) != 0)
      if_microsoft_extensions(
        && scp->member_of_unknown_super ==
                             ((options & IDL_MEMBER_OF_UNKNOWN_SUPER) != 0))) {
      if (scp->member_of_unknown_base) {
        /* Qualified and unqualified references to a given member of an
           unknown base are treated as separate symbols, so that the IL can
           distinguish between "this->f()", which might be virtual, and
           "this->S::f()", which is always non-virtual. */
        result = scp->qualified_unknown_base_member ==
                                                    locator->is_qualified_name;
      } else {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* acceptable_nonreal_class_member_symbol */


a_symbol_ptr create_proxy_or_nonreal_class_member_of_kind(
				a_type_ptr			class_type,
				a_symbol_kind			kind,
				an_id_lookup_options_set	options,
				a_symbol_locator		*locator)
/*
Create a proxy or nonreal member with the specified symbol kind.
class_type is the nonreal class in which the member is to be created.
"locator" is used to get the name, source position, and conversion
result type.  options specifies the lookup options being used.

The member that is created is not added to the inactive list by this
routine.
*/
{
  a_class_symbol_supplement_ptr cssp;
  a_scope_depth                 depth = NO_SCOPE_DEPTH;
  a_symbol_ptr                  sym;
  a_source_correspondence       *scp = NULL;

  db_enter(4, "create_proxy_or_nonreal_class_member_of_kind");
  /* Create a symbol for the member.  mark_declared is not called
     because this symbol is not visible to the user. */
  sym = alloc_symbol(kind, locator->symbol_header, &locator->source_position);
  /* Get the scope number from the symbol supplement.  The scope depth
     will be the scope depth of the class plus one. */
  cssp = symbol_supplement_for_class(class_type);
  check_assertion(cssp->member_decl_scope != NO_SCOPE_NUMBER);
  sym->decl_scope = cssp->member_decl_scope;
  sym->is_nonreal_member = TRUE;
#if RECORD_SCOPE_DEPTH_IN_IL
  depth = class_type->source_corresp.scope_depth;
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
  /* Create the type or constant. */
  switch (kind) {
    case sk_type:
    {
      a_type_ptr	type = alloc_type((a_type_kind)tk_template_param);
      type->variant.template_param.kind =
                                    (a_template_param_type_kind)tptk_member;
      set_type_size(type);
      sym->variant.type.ptr = type;
      scp = &type->source_corresp;
      break;
    }
    case sk_constant:
    {
      /* Create a ck_template_param constant.  We don't know the type of the
         constant so we use a special template parameter type that represents
         the type of an unknown constant. */
      a_constant_ptr	constant;
      constant = fs_constant((a_constant_repr_kind)ck_template_param);
      if (locator->is_conversion_name) {
        /* For a conversion operator, create an "unknown function" entry that
           represents the conversion result type. */
        set_template_param_constant_kind(
              constant, (a_template_param_constant_kind)tpck_unknown_function);
        constant->variant.template_param.variant.unknown_function.
                     conversion_type = locator->variant.conversion_result_type;
        sym->is_unknown_function = TRUE;
      } else if (locator->is_operator_name) {
        /* For operators, create an "unknown function" entry that represents
           the kind of operator. */
        set_template_param_constant_kind(
              constant, (a_template_param_constant_kind)tpck_unknown_function);
        if (locator->is_qualified_name) {
          constant->variant.template_param.is_qualified_name = TRUE;
        }  /* if */
        constant->variant.template_param.variant.unknown_function.
                                         opname_kind = locator->variant.opname;
        sym->is_unknown_function = TRUE;
      } else if (locator->is_destructor_name &&
                 locator->variant.destructor_type != NULL) {
        /* Create a tpck_destructor entry. */
        a_type_ptr  dtor_type = locator->variant.destructor_type; 
        if (is_proxy_class(dtor_type)) {
          /* Use the "original" type of a proxy type. */
          dtor_type = class_type_supp(dtor_type)->proxy_of_type;
        }  /* if */
        set_template_param_constant_kind(
                    constant, (a_template_param_constant_kind)tpck_destructor);
        constant->variant.template_param.variant.destructor.type = dtor_type;
        constant->variant.template_param.variant.destructor.unqualified =
                                                   !locator->is_qualified_name;
      } else {
        /* For everything except a conversion function create a generic
           nontype member. */
        set_template_param_constant_kind(
                        constant, (a_template_param_constant_kind)tpck_member);
      }  /* if */
      sym->variant.constant = constant;
      constant->type = type_of_unknown_templ_param_nontype;
      scp = &constant->source_corresp;
      break;
    }
    case sk_static_data_member:
    {
      /* Create a static data member whose type is the unknown nontype
         type. */
      a_variable_ptr	var;
      var = make_variable(type_of_unknown_templ_param_nontype,
                          (a_storage_class)sc_extern, NO_SCOPE_DEPTH);
      sym->variant.static_data_member.variable = var;
      scp = &var->source_corresp;
      break;
    }
    case sk_class_template:
    {
      /* This is a template used in a context like T::A<int>.  A template
         symbol is created for T::A.  Indicate that this template is
         a nonreal class member. */
      a_template_symbol_supplement_ptr	tssp;
      a_template_ptr			templ_ptr;
      tssp = sym->variant.template_info;
      tssp->is_nonreal_member = TRUE;
      tssp->variant.class_template.type_kind = (a_type_kind)tk_class;
      tssp->variant.class_template.access = (an_access_specifier)as_public;
      templ_ptr = alloc_template();
      /* The templates associated with template parameters and nonreal classes
         have a template_info pointer that points back to the front end
         information. */
      templ_ptr->template_info = tssp;
      scp = &templ_ptr->source_corresp;
      templ_ptr->kind = (a_template_kind)templk_class;
      tssp->il_template_entry = templ_ptr;
      if (prototype_instantiations_in_il) {
        /* When prototype instantiations are included in the IL, add the
           template to the IL.  We arbitrarily use the file scope as the
           place to add it. */
        add_to_templates_list(templ_ptr, DEPTH_OF_FILE_SCOPE);
      }  /* if */
      break;
    }
    default:
      unexpected_condition();
  }  /* switch */
  if (scp != NULL) {
    set_source_corresp_with_scope_depth(scp, sym, depth);
    if (options & IDL_MEMBER_OF_UNKNOWN_BASE) {
      scp->member_of_unknown_base = TRUE;
      scp->qualified_unknown_base_member = locator->is_qualified_name;
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    scp->member_of_unknown_super =
                                 (options & IDL_MEMBER_OF_UNKNOWN_SUPER) != 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  if (class_type_supp(class_type)->assoc_scope == NULL) {
    /* Create a scope in which the member can live. */
    add_scope_to_class_type(class_type);
  }  /* if */
  set_class_membership(sym, scp, class_type);
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    fprintf(f_debug, "Created: ");
    db_symbol(sym, "", 0);
    if (scp != NULL) {
      fprintf(f_debug, "Member of unknown base=%s\n",
              scp->member_of_unknown_base ? "true" : "false");
      fprintf(f_debug, "Qualified unknown base member = %s\n",
              scp->qualified_unknown_base_member ? "true" : "false");
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  db_exit();
  return sym;
}  /* create_proxy_or_nonreal_class_member_of_kind */


static a_symbol_ptr create_proxy_or_nonreal_class_member
					(a_type_ptr	          class_type,
					 an_id_lookup_options_set options,
					 a_symbol_locator         *locator)
/*
This routine is called by class_qualified_id_lookup when the name
being looked up is not found in the proxy class associated with a
template parameter type or in a class that is a nonreal instantiation.
We don't know anything about the name that is being looked up except
whether or not it is a type (inferred from the lookup options).  If
the name is a type, we create a member of class_type that is a
tk_template_param; otherwise, we create a member of class_type that is
a ck_template_param.

The member that is created is not added to the inactive list by this
routine.
*/
{
  a_symbol_kind                 kind;
  a_symbol_ptr                  sym;

  db_enter(4, "create_proxy_or_nonreal_class_member");
  /* Determine the symbol kind to be created.  The symbol can be a
     type, constant, or class template, depending on the kind of
     lookup being done. */
  kind = nonreal_member_symbol_kind(locator, options);
  sym = create_proxy_or_nonreal_class_member_of_kind(class_type, kind,
                                                     options, locator);
  db_exit();
  return sym;
}  /* create_proxy_or_nonreal_class_member */


static a_symbol_ptr add_member_to_proxy_or_nonreal_class
					(a_type_ptr	          class_type,
					 an_id_lookup_options_set options,
					 a_symbol_locator         *locator)
/*
Creates a proxy or nonreal class member of class_type.  Adds the newly
created symbol to the inactive list and returns it to the caller.
*/
{
  a_symbol_ptr	sym;

  sym = create_proxy_or_nonreal_class_member(class_type, options, locator);
  /* Add the symbol to the scope list and inactive list. */
  enter_symbol_into_completed_class(sym);
  return sym;
}  /* add_member_to_proxy_or_nonreal_class */


void create_nonreal_version_of_nested_type(a_symbol_ptr	orig_sym)
/*
A nested type of a prototype instantiation exists in two forms: its original
form, and a nonreal version that is used in contexts where the name should be
considered a dependent type.

  template <typename T> struct A {
    class B {};
    B b;  // refers to nonreal B
  };

This routine is given the original symbol and creates the nonreal version.
*/
{
  a_type_ptr				class_type;
  a_symbol_locator			locator;
  a_symbol_ptr				nonreal_sym;
  a_template_param_type_supplement_ptr	tptsp;
  a_type_ptr				templ_param_type;

  check_assertion(orig_sym->is_class_member);
  class_type = sym_parent_class(orig_sym);
  make_locator_for_symbol(orig_sym, &locator);
  nonreal_sym = create_proxy_or_nonreal_class_member_of_kind(
                 class_type, (a_symbol_kind)sk_type, IDL_NO_OPTIONS, &locator);
  orig_sym->corresp_nonreal_or_nested_type = nonreal_sym;
  nonreal_sym->corresp_nonreal_or_nested_type = orig_sym;
  nonreal_sym->is_nonreal_nested_type = TRUE;
  templ_param_type = nonreal_sym->variant.type.ptr;
  tptsp = templ_param_type->variant.template_param.extra_info;
  tptsp->orig_nested_type = type_symbol_type(orig_sym);
  if (prototype_instantiations_in_il) {
    /* When prototype instantiations are included in the IL, add the type. */
    add_to_types_list(templ_param_type, DEPTH_OF_FILE_SCOPE);
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("cnvont")) {
    fprintf(f_debug, "Created nonreal nested type:\n");
    db_symbol(nonreal_sym, "  Nonreal symbol: ", 4);
    db_symbol(orig_sym, "  Original symbol: ", 4);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
}  /* create_nonreal_version_of_nested_type */


a_type_ptr f_orig_nested_type_if_nonreal_nested_type(a_type_ptr	tp)
/*
If tp is a template parameter type that is the nonreal type for a nested
type of a class template return the original nested type, otherwise return
tp.
*/
{
  /* If this is a template parameter that represents a nested class of
     a class template, use the original nested type in its place. */
  a_template_param_type_supplement_ptr	tptsp;
  check_assertion(tp->kind == (a_type_kind)tk_template_param);
  tptsp = tp->variant.template_param.extra_info;
  if (tptsp->orig_nested_type != NULL) {
    tp = tptsp->orig_nested_type;
  }  /* if */
  return tp;
}  /* f_orig_nested_type_if_nonreal_nested_type */


static
a_symbol_ptr enter_sym_for_out_of_scope_routine(a_symbol_ptr     extern_sym,
						a_symbol_locator *locator)
/*
Create a symbol entry in the current scope for the external routine
designated by extern_sym.  Return the symbol pointer to the caller.
This routine is only used in SVR4 C compatibility mode.  It is
called by find_out_of_scope_declaration when a normal lookup fails
and there is an external symbol (created by some other scope) that
should be used to satisfy the lookup.
*/
{
  an_id_linkage_kind     linkage;
  a_type_ptr             old_type;
  a_symbol_ptr           ext_sym;
  a_func_info_block      func_info;
  a_decl_parse_state     dps;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean              saved_sses_disallowed =
                                           source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  /* This routine must only be called in ANSI C mode. */
  check_assertion(C_dialect == C_dialect_ANSI);
  init_decl_parse_state(&dps);
  /* Enter the symbol in the symbol table.  decl_routine expects
     this to be done by the caller for implicitly declared routines. */
  dps.sym = enter_symbol((a_symbol_kind)sk_routine, locator, depth_scope_stack,
                         /*suppress_error=*/FALSE);
  /* Create a local declaration of the external routine.  Use the type from
     the sk_extern_routine symbol. */
  dps.type = extern_sym->variant.extern_symbol_descr->type;
  dps.storage_class = dps.declared_storage_class = (a_storage_class)sc_extern;
  /* Declare the function identifier. */
  clear_func_info(&func_info);
  func_info.is_implicit_declaration = TRUE;
  if (exceptions_enabled) func_info.throw_position = locator->source_position;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Since this isn't a declaration that appears in the source code, do not
     emit a source sequence entry for it. */
  source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  decl_routine(locator, &dps, &func_info, (SRK_DECLARATION | SRK_IMPLICIT),
               &linkage, &old_type, &ext_sym, (a_decl_pos_block_ptr)NULL);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  source_sequence_entries_disallowed = saved_sses_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  done_with_func_info(func_info);
  /* Set the referenced flag on the routine entry.  The implicit declaration
     is also an immediate reference. */
  dps.sym->variant.routine.ptr->source_corresp.referenced = TRUE;
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 3) {
    db_symbol(dps.sym, "", 4);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return dps.sym;
}  /* enter_sym_for_out_of_scope_routine */


static
a_symbol_ptr enter_sym_for_out_of_scope_variable(a_symbol_ptr     extern_sym,
						 a_symbol_locator *locator)
/*
Create a symbol entry in the current scope for the external variable
designated by extern_sym.  Return the symbol pointer to the caller.
This routine is only used in SVR4 C compatibility mode.  It is
called by find_out_of_scope_declaration when a normal lookup fails
and there is an external symbol (created by some other scope) that
should be used to satisfy the lookup.
*/
{
  a_decl_parse_state  dps;
  an_id_linkage_kind  linkage;
  a_symbol_ptr        ext_sym;

  /* This routine must only be called in ANSI C mode. */
  check_assertion(C_dialect == C_dialect_ANSI);
  init_decl_parse_state(&dps);
  /* Create a local declaration of the external variable.  Use the type
     from the sk_extern_variable symbol. */
  dps.type = extern_sym->variant.extern_symbol_descr->type;
  dps.storage_class = (a_storage_class)sc_extern;
  decl_variable(locator, &dps, (SRK_DECLARATION | SRK_IMPLICIT), &linkage,
                &ext_sym, (a_decl_pos_block_ptr)NULL); 
  /* Set the referenced flag on the variable entry.  The implicit declaration
     is also an immediate reference. */
  dps.sym->variant.variable.ptr->source_corresp.referenced = TRUE;
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 3) {
    db_symbol(dps.sym, "", 4);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return dps.sym;
}  /* enter_sym_for_out_of_scope_variable */


static
a_symbol_ptr find_out_of_scope_declaration(a_symbol_locator         *locator,
                                           an_id_lookup_options_set options)
/*
This is an SVR4 compatibility feature.  This routine is used to make external
symbol declarations from other scopes visible in the current scope.
For example:

  int f1(void)
  {
    extern void f();
    extern int i;
  }

  int f2()
  {
    int j;
    f();
    j = i;
  }

In this example, symbols for f and i are entered in function f2.  They
refer to the external entities declared by the declarations in f1.

When a symbol lookup fails, this routine is called to see if an external
symbol exists with the name being looked up.  If so, a new symbol is
entered in the current scope that refers to the external entity.
"options" specifies the lookup options being used.  The external symbol
that is found must meet the criteria specified by the lookup options.
A warning is issued.  A pointer to the new symbol is returned.  If no
such pointer is found, NULL is returned.
*/
{
  a_symbol_locator  new_locator;
  a_symbol_ptr      sym = NULL;;

  sym = find_external_symbol(locator, (a_name_linkage_kind)nlk_external,
                             (a_type_ptr)NULL, (a_requires_clause*)NULL,
                             &new_locator);
  if (sym != NULL && sym->kind == (a_symbol_kind)sk_extern_routine && 
      sym->variant.extern_symbol_descr->
                                  variant.routine.is_implicit_declaration) {
    /* Don't redeclare a previous symbol if it was implicitly
       declared. */
    sym = NULL;
  } else if (sym != NULL && !sym_matches_lookup_options(sym, options)) {
    /* The external symbol does not satisfy the lookup criteria. */
    sym = NULL;
  }  /* if */
  if (sym != NULL) {
    sym_warning(ec_using_out_of_scope_declaration, sym);
    if (sym->kind == (a_symbol_kind)sk_extern_routine) {
      sym = enter_sym_for_out_of_scope_routine(sym, locator);
    } else if (sym->kind == (a_symbol_kind)sk_extern_variable) {
      sym = enter_sym_for_out_of_scope_variable(sym, locator);
    } else {
      unexpected_condition();
    }  /* if */
  }  /* if */
  return sym;
}  /* find_out_of_scope_declaration */


a_boolean symbols_are_lookup_equivalent(
			a_symbol_ptr			sym1,
			a_symbol_ptr			sym2,
			a_boolean			merge_c_funcs,
			an_id_lookup_options_set	options)
/*
Return TRUE if sym1 is the same as sym2 or if sym1 and sym2 point to the same
IL entities.  The latter check is used, for example, to determine whether two
symbols from different namespaces point to the same underlying extern "C"
variable or function.  Two such symbols that appear in the same using-directive
lookup set are considered to represent the same entity, so one of the two
symbols is arbitrarily selected.  sym1 and sym2 must have been reduced to their
fundamental symbols by the caller.  merge_c_funcs is TRUE if extern "C"
functions should be treated as equivalent in GNU and Microsoft C++ modes.
options specifies the options being used for the lookup.
*/
{
  a_boolean	result = FALSE;
  if (sym1 == sym2) {
    /* The symbols are the same. */
    result = TRUE;
  } else if (sym1->kind == sym2->kind) {
    /* The symbols refer to the same kind of entity -- check further. */
    if (sym1->kind == (a_symbol_kind)sk_variable) {
      result = sym1->variant.variable.ptr == sym2->variant.variable.ptr;
    } else if (symbol_is(sym1, sk_namespace) ) {
      /* A namespace is considered equivalent to a namespace alias that refers
         to the same namespace. */
      a_namespace_ptr	ns1 = sym1->variant.namespace_info.ptr;
      a_namespace_ptr	ns2 = sym2->variant.namespace_info.ptr;
      ns1 = skip_namespace_aliases(ns1);
      ns2 = skip_namespace_aliases(ns2);
      result = ns1 == ns2;
    } else if (sym1->kind == (a_symbol_kind)sk_routine) {
      a_routine_ptr	rp1 = sym1->variant.routine.ptr;
      a_routine_ptr	rp2 = sym2->variant.routine.ptr;
      result = rp1 == rp2;
      if (!result && merge_c_funcs &&
          ((gpp_mode && !clang_mode) || microsoft_mode)) {
        /* In g++ mode a second extern "C" routine is not added to the lookup
           set for using-directives if the set already contains an extern "C"
           routine.  Note that the types don't need to be the same. */
        if (rp1->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_external &&
            rp2->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_external) {
          result = TRUE;
        }  /* if */
      }  /* if */
    } else if ((equiv_typedefs_are_lookup_equivalent ||
                (options & IDL_HIDDEN_NAME_LOOKUP) == 0) &&
               is_type_symbol(sym1)) {
      /* If the two symbols refer to the same type they are considered
         equivalent.  For hidden name processing, the symbols are considered
         as not equivalent in some modes. */
      a_type_ptr  tp1 = type_symbol_type(sym1), tp2 = type_symbol_type(sym2);
      result = identical_types(tp1, tp2);
    }  /* if */
  }  /* if */
  return result;
}  /* symbols_are_lookup_equivalent */


a_boolean already_in_lookup_set(a_symbol_ptr			curr_sym,
                                a_symbol_ptr			new_sym,
				a_boolean			merge_c_funcs,
				an_id_lookup_options_set	options)
/*
See if new_sym is already in the lookup set represented by curr_sym.
curr_sym could point to a single namespace projection symbol or
an sk_overloaded_function symbol that points to a set of namespace
projections symbols.  curr_sym is the fundamental symbol to be compared
with the fundamental symbols pointed to by the namespace projection
symbol(s) in curr_sym.  merge_c_funcs is TRUE if extern "C" functions
from different scopes should be considered equivalent (in GNU and
Microsoft modes).  options specifies the options being used for the
lookup.
*/
{
  a_boolean	result = FALSE;

  new_sym = fundamental_symbol_of(new_sym);
  if (curr_sym == NULL) {
    /* No current list -- return FALSE. */
  } else if (curr_sym->kind == (a_symbol_kind)sk_namespace_projection) {
    /* See if the current symbol is a projection symbol that points to
       new_sym or a symbol equivalent to new_sym. */
    a_symbol_ptr	fund_curr_sym = fundamental_symbol_of(curr_sym);
    result = new_sym == fund_curr_sym ||
             symbols_are_lookup_equivalent(new_sym, fund_curr_sym,
                                           merge_c_funcs, options);
  } else if (curr_sym->kind == (a_symbol_kind)sk_overloaded_function) {
    /* Look through the overload set for a fundamental symbol that matches
       new_sym. */
    a_symbol_ptr	sym;
    for (sym = curr_sym->variant.overloaded_function.symbols;
         sym != NULL; sym = sym->next) {
      a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
      if (new_sym == fund_sym ||
          symbols_are_lookup_equivalent(new_sym, fund_sym, merge_c_funcs,
                                        options)) {
        break;
      }  /* if */
    }  /* for */
    if (sym != NULL) result = TRUE;
  } else {
    /* See if the current symbol is a routine symbol that is the same as
       new symbol.  This case is used when the first symbol found is
       a function or template symbol. */
    check_assertion(is_function_or_template_symbol(curr_sym));
    result = curr_sym == new_sym ||
             symbols_are_lookup_equivalent(curr_sym, new_sym, merge_c_funcs,
                                           options);
  }  /* if */
  return result;
}  /* already_in_lookup_set */


static
a_symbol_ptr merge_function_into_lookup_set(
                               a_symbol_ptr		curr_sym,
                               a_symbol_ptr		new_sym,
                               a_symbol_locator		*locator,
                               a_boolean		qualified_lookup,
                               a_namespace_ptr		qualifier_namespace,
                               an_id_lookup_options_set	options)
/*
curr_sym is a pointer to the current lookup set, and may be NULL,
a pointer to a single namespace projection symbol, or an
sk_overloaded_function symbol that points to a number of namespace
projection symbols.  Add the function(s) pointed to by new_sym
creating a new overload set.
qualified_lookup is TRUE if for a namespace or file scope qualified
lookup.  For qualified lookups qualifier_namespace points to the
namespace in which the lookup is being done, or is NULL for a file
scope lookup.  options specifies the options being used for the lookup.
*/
{
  /* The set is currently empty.  If the initial member is a single
     routine, create a namespace projection that points to it.  If
     it is an overload set, make a new overload set containing
     namespace projections that point to its members. */
  if (new_sym->kind != (a_symbol_kind)sk_overloaded_function) {
    /* The new symbol is a simple function or is a function template.
       Make a namespace projection that points to it. */
    if (curr_sym == NULL) {
      curr_sym = enter_synthesized_projection_symbol(new_sym, locator,
                                                     qualified_lookup,
                                                     qualifier_namespace,
                                                     options);
    } else if (curr_sym->kind == (a_symbol_kind)sk_namespace_projection &&
               fundamental_symbol_of(curr_sym) == NULL) {
      /* The current lookup set is a namespace projection symbol whose
         fundamental symbol pointer has been cleared.  Simply set this
         symbol to point to the new symbol. */
      set_namespace_projection_symbol(curr_sym, new_sym, NO_SCOPE_DEPTH);
    } else {
      /* If new_sym is not already in the lookup set, add it. */
      if (!already_in_lookup_set(curr_sym, new_sym, /*merge_c_funcs=*/TRUE,
                                 options)) {
        new_sym = make_namespace_projection_symbol(new_sym,
                                                   &locator->source_position,
                                                   depth_scope_stack);
        curr_sym = add_symbol_to_overload_list(new_sym, curr_sym,
                                               qualified_lookup,
                                               qualifier_namespace);
      }  /* if */
    }  /* if */
  } else {
    /* The new symbol is an overload set. */
    a_symbol_ptr	rout_sym;
    a_symbol_ptr	new_rout_sym;
    a_boolean		curr_sym_was_null = curr_sym == NULL;
    rout_sym = new_sym->variant.overloaded_function.symbols;
    /* Skip any invisible symbols. */
    while (rout_sym != NULL && rout_sym->is_invisible) {
      rout_sym = rout_sym->next;
    }  /* while */
    if (rout_sym == NULL) {
      /* All of the members of the set were invisible. */
    } else if (curr_sym == NULL) {
      /* If the current symbol is NULL, take the first member of the
         overload set and create a projection symbol to it.  Later
         we will add the remaining members and create a new overload set. */
      curr_sym = enter_synthesized_projection_symbol(rout_sym, locator,
                                                     qualified_lookup,
                                                     qualifier_namespace,
                                                     options);
      rout_sym = rout_sym->next;
    } else if (curr_sym->kind == (a_symbol_kind)sk_namespace_projection &&
               fundamental_symbol_of(curr_sym) == NULL) {
      /* The current lookup set is a namespace projection symbol whose
         fundamental symbol pointer has been cleared.  Simply set this
         symbol to point to the first member of the overload set. */
      set_namespace_projection_symbol(curr_sym, rout_sym, NO_SCOPE_DEPTH);
      rout_sym = rout_sym->next;
    }  /* if */
    for (; rout_sym != NULL; rout_sym = rout_sym->next) {
      /* If rout_sym is not already in the lookup set, add it (unless it is an
         invisible symbol, presumably a routine only declared as a friend
         declaration). */
      if (rout_sym->is_invisible) continue;
      if (curr_sym_was_null ||
          !already_in_lookup_set(curr_sym, rout_sym, /*merge_c_funcs=*/TRUE,
                                 options)) {
        new_rout_sym =
                   make_namespace_projection_symbol(rout_sym,
                                                    &locator->source_position,
                                                    depth_scope_stack);
        curr_sym = add_symbol_to_overload_list(new_rout_sym, curr_sym,
                                               qualified_lookup,
                                               qualifier_namespace);
      }  /* if */
    }  /* for */
  }  /* if */
  return curr_sym;
}  /* merge_function_into_lookup_set */


static a_boolean check_for_microsoft_qualifier_using_directive_bug(
				a_symbol_ptr			*curr_sym,
				a_symbol_ptr			fund_curr_sym,
				a_symbol_ptr			new_sym,
				an_id_lookup_options_set	options)
/*
In certain cases, the Microsoft compiler prefers a class name made visible
by a using-directive to a typedef-to-class that is visible in a scope
where using-directives apply.  For example:

  template <class T> struct basic_ios { };
  typedef basic_ios<char> ios;
  namespace std { struct ios { const static int i;}; };
  using namespace std;
  int i = ios::i;

The Microsoft compiler uses std::ios instead of reporting an ambiguity.

This routine checks for this case.  curr_sym could be the typedef symbol
or the class symbol.  Likewise for new_sym.  "options" is the set of
lookup options.  fund_curr_sym is the fundamental symbol associated with
curr_sym.  The special processing is only done when a "must be class or
namespace" lookup is done.
*/
{
  a_boolean	result = FALSE;

  if ((options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0) {
    a_boolean	curr_is_typedef;
    a_boolean	new_is_typedef;
    curr_is_typedef = fund_curr_sym->kind == (a_symbol_kind)sk_type;
    new_is_typedef = new_sym->kind == (a_symbol_kind)sk_type;
    if (!curr_is_typedef && new_is_typedef) {
      /* Use the current symbol. */
      result = TRUE;
    } else if (!new_is_typedef && curr_is_typedef) {
      /* Use the new symbol. */
     result = TRUE;
     set_namespace_projection_symbol(*curr_sym, new_sym, NO_SCOPE_DEPTH);
    }  /* if */
  }  /* if */
  return result;
}  /* check_for_microsoft_qualifier_using_directive_bug */
				


static a_boolean symbols_from_same_scope(a_symbol_ptr curr_sym,
                                         a_symbol_ptr new_sym)
/*
Return TRUE if the two symbols are from the same scope.  curr_sym represents a
lookup set that has been constructed and new_sym is a normal symbol (not a
synthesized projection symbol) that is being considered as an alternative to
curr_sym because of the 1.5 namespace rule for struct names.  When curr_sym
points to a set of overloaded functions, the overload set must be inspected to
see if all of the members of the set are from the same scope.
*/
{
  a_boolean		result = TRUE;
  a_scope_number	curr_scope;

  /* Get the scope associated with curr_sym. */
  if (curr_sym->kind != (a_symbol_kind)sk_overloaded_function) {
    curr_scope = fundamental_symbol_of(curr_sym)->decl_scope;
  } else {
    /* curr_sym is an overload set, see if all of the members of the set have
       the same scope. */
    a_symbol_ptr	overload_sym;
    overload_sym = curr_sym->variant.overloaded_function.symbols;
    curr_scope = fundamental_symbol_of(overload_sym)->decl_scope;
    overload_sym = overload_sym->next;
    for (; overload_sym != NULL; overload_sym = overload_sym->next) {
      if (fundamental_symbol_of(overload_sym)->decl_scope != curr_scope) {
        /* A scope mismatch -- stop the search. */
        result = FALSE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  /* If we produced a single scope above, compare it with the new symbol. */
  if (result) {
    result = curr_scope == fundamental_symbol_of(new_sym)->decl_scope;
  }  /* if */
  return result;
}  /* symbols_from_same_scope */


/* Forward declaration. */
static a_symbol_ptr add_symbol_to_lookup_set(
                               a_symbol_ptr		curr_sym,
                               a_symbol_ptr		new_sym,
                               a_symbol_locator		*locator,
                               a_boolean		qualified_lookup,
                               a_namespace_ptr		qualifier_namespace,
                               an_id_lookup_options_set	options,
                               a_boolean		*any_errors);


static a_boolean check_for_tag_hiding(
			a_symbol_ptr			*curr_sym,
			a_symbol_ptr			fund_curr_sym,
			a_symbol_ptr			new_sym,
			a_symbol_locator		*locator,
			a_namespace_ptr			qualifier_namespace,
			a_boolean			qualified_lookup,
			an_id_lookup_options_set	options,
                        a_boolean			*any_errors)
/*
This routine is used by add_symbol_to_lookup_set to detect the case
in which a tag symbol is hidden by a nontype symbol from the same
scope.

curr_sym and new_sym are the two symbols, either one of which could be
a tag or nontag.

In Sun compatibility mode, the symbols need not be from the same scope.
*/
{
  a_boolean	result = FALSE;

  if (sun_mode || symbols_from_same_scope(fund_curr_sym, new_sym)) {
    a_boolean	new_is_tag = is_tag_symbol(new_sym);
    a_boolean	curr_is_tag = is_tag_symbol(fund_curr_sym);
    if (new_is_tag != curr_is_tag) {
      /* Two symbols from the same scope and only one is a nontag.
         This is okay. */
      a_boolean  must_be_tag = (options & IDL_MUST_BE_TAG) != 0;
      result = TRUE;
      if (gnu_namespace_and_class_in_same_scope) {
        /* Some versions of g++ allow a class and namespace with the same
           name in a given scope.  The class name should be used as the
           result of the lookup. */
        a_boolean	new_is_namespace;
        a_boolean	curr_is_namespace;
        new_is_namespace = !new_is_tag && is_namespace_symbol(new_sym);
        curr_is_namespace = !curr_is_tag && is_namespace_symbol(fund_curr_sym);
        if (new_is_namespace || curr_is_namespace) {
          /* Set must_be_tag so that the non-namespace symbol will be used. */
          must_be_tag = TRUE;
        }  /* if */
      }  /* if */
      if (curr_is_tag != must_be_tag) {
        /* The current symbol is a tag and we prefer a nontag, or the current
           symbol is a nontag and we are looking for a tag.  Update the
           namespace projection symbol to point to the new symbol. */
        check_assertion_str2((*curr_sym)->kind ==
                                   (a_symbol_kind)sk_namespace_projection,
                             "check_for_tag_hiding:",
                             "expected a namespace projection symbol");
        /* Reset the namespace projection to NULL, then recall this routine
           to add the new symbol.  This is done to handle cases where
           new_sym points to an overload set. */
        (*curr_sym)->variant.namespace_projection.fundamental_symbol = NULL;
        *curr_sym = add_symbol_to_lookup_set(
                             *curr_sym, new_sym, locator, qualified_lookup,
                             qualifier_namespace, options, any_errors);
      } else {
        /* The current symbol is a nontag and the new one is a tag.
           Simply ignore the new one. */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* check_for_tag_hiding */

#if DEBUG
/* GCOVR_EXCL_START */

static unsigned long functions_represented_by_symbol(a_symbol_ptr	sym)
/*
Return the number of function symbols represented by "sym".  Actually,
it also returns a value of 1 for non-function symbols too.
*/
{
  unsigned long	result = 0;

  if (sym == NULL) {
  } else if (sym->kind != (a_symbol_kind)sk_overloaded_function) {
    result = 1;
  } else {
      sym = sym->variant.overloaded_function.symbols;
      for (; sym != NULL; sym = sym->next) result++;
  }  /* if */
  return result;
}  /* functions_represented_by_symbol */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

static
a_symbol_ptr add_symbol_to_lookup_set(
                               a_symbol_ptr		curr_sym,
                               a_symbol_ptr		new_sym,
                               a_symbol_locator		*locator,
                               a_boolean		qualified_lookup,
                               a_namespace_ptr		qualifier_namespace,
                               an_id_lookup_options_set	options,
                               a_boolean		*any_errors)
/*
Reconcile the results of a lookup in which more than one symbol is found
in scopes that are considered equivalent.  This occurs when using
directives cause symbols from multiple namespaces (possibly including
the global namespace) to be found as the result of a lookup.

When the first symbol is found, create an sk_namespace_projection
symbol that points to it.  If a second symbol is found, and both
the old and new symbols are functions, create an sk_overloaded_function
symbol that points to two sk_namespace_projection symbols.  Continue
adding new sk_namespace_projections as long as all of the symbols
found are functions.  If, at any point, there are both functions and
nonfunctions, or more than one nonfunction, set the any_errors flag.
qualified_lookup is TRUE for a namespace or file scope qualified
lookup.  For qualified lookups qualifier_namespace points to the
namespace in which the lookup is being done, or is NULL for a file
scope lookup.  options specifies the options being used for the lookup.

In most cases a new or updated curr_sym is returned.  However, if new_sym
contains only invisible symbols, a NULL symbol can be returned.
*/
{
  a_boolean	err = FALSE;

  /* Make sure the lookup set points to the fundamental symbol. */
  new_sym = fundamental_symbol_of(new_sym);
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("lookup_set")) {
    a_decl_sequence_number decl_seq_of_symbol;
    /* Get the declaration sequence number to be used for this symbol.  For
       most symbols it is just the declaration sequence number of the symbol.
       For overloaded function symbols though, it is the declaration sequence
       number of the symbol most recently added to the overload list, which
       is the symbol at the front of the list. */
    decl_seq_of_symbol = new_sym->kind == (a_symbol_kind)sk_overloaded_function
                      ? new_sym->variant.overloaded_function.symbols->decl_seq
                      : new_sym->decl_seq;
    fprintf(f_debug,
            "add_symbol_to_lookup_set: symbols at start - curr=%lu, new=%lu\n",
            functions_represented_by_symbol(curr_sym),
            functions_represented_by_symbol(new_sym));
    fprintf(f_debug, "  decl_seq_of_symbol=%lu\n",
            (unsigned long)decl_seq_of_symbol);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif  /* DEBUG */
  if (curr_sym == NULL) {
    if (is_function_or_template_symbol(new_sym)) {
      curr_sym = merge_function_into_lookup_set((a_symbol_ptr)NULL,
                                                new_sym, locator,
                                                qualified_lookup,
                                                qualifier_namespace,
						options);
    } else {
      curr_sym = enter_synthesized_projection_symbol(new_sym, locator,
                                                     qualified_lookup,
                                                     qualifier_namespace,
                                                     options);
    }  /* if */
  } else if (curr_sym->kind == (a_symbol_kind)sk_namespace_projection &&
             namespace_projection_fundamental_symbol(curr_sym) == NULL) {
    /* curr_sym is a namespace projection symbol that doesn't point to any
       other symbol.  This is the case when a synthesized namespace
       projection symbol from a previous lookup is being used, and the
       previous symbol does not point to a function.  In this case the
       fundamental symbol pointer is cleared because it may have been
       the result of a constrained lookup (e.g., a tag lookup) which
       may not be applicable now.  Set the existing symbol to point to
       the new symbol. */
    if (is_function_or_template_symbol(new_sym)) {
      curr_sym = merge_function_into_lookup_set(curr_sym, new_sym, locator,
                                                qualified_lookup,
                                                qualifier_namespace,
						options);
    } else {
      set_namespace_projection_symbol(curr_sym, new_sym, NO_SCOPE_DEPTH);
    }  /* if */
  } else if (already_in_lookup_set(curr_sym, new_sym, /*merge_c_funcs=*/TRUE,
                                   options)) {
    /* The symbol is already present -- nothing more to do. */
  } else {
    a_symbol_ptr	fund_curr_sym;
    fund_curr_sym = fundamental_symbol_of(curr_sym);
    if (!is_function_or_template_symbol(new_sym) ||
        !is_function_or_template_symbol(fund_curr_sym)) {
      /* There is more than one symbol, and they are not all functions.
         This is an error unless the two symbols are from the same scope
         and one is a tag and the other a nontag.  Set the error flag.
         It will be cleared later if we determine that this case is okay. */
      err = TRUE;
      if (check_for_tag_hiding(&curr_sym, fund_curr_sym, new_sym, locator,
                               qualifier_namespace, qualified_lookup,
                               options, any_errors)) {
        /* curr_sym is set to the appropriate symbol by
           check_for_tag_hiding. */
        err = FALSE;
      } else if ((equiv_typedefs_are_lookup_equivalent ||
                 (options & IDL_HIDDEN_NAME_LOOKUP) == 0) &&
                 is_type_symbol(new_sym) && is_type_symbol(fund_curr_sym) &&
                 identical_types(type_symbol_type(new_sym),
                                 type_symbol_type(fund_curr_sym))) {
        /* The two symbols refer to the same type.  Ignore the new one.
           For hidden name processing, both symbols are included in the
           lookup set in some modes. */
        err = FALSE;
      } else if ((options & IDL_TENTATIVE_TYPE_LOOKUP) != 0 &&
                 is_type_symbol(new_sym) && !is_type_symbol(fund_curr_sym)) {
        /* We are doing a tentative type lookup and the new symbol is
           a type, but the old one is not.  This is an ambiguous case,
           but when possible, we want the symbol returned to point to
           the type symbol.  This improves error recovery in declaration
           contexts. */
       set_namespace_projection_symbol(curr_sym, new_sym, NO_SCOPE_DEPTH);
      } else if (curr_sym->kind == (a_symbol_kind)sk_namespace_projection &&
                 fund_curr_sym->kind == (a_symbol_kind)sk_undefined) {
        /* The current symbol is an sk_undefined symbol.  Use a "real" symbol
           if one is available, for better error recovery. */
        curr_sym->variant.namespace_projection.fundamental_symbol = NULL;
        curr_sym = add_symbol_to_lookup_set(curr_sym, new_sym, locator,
                                            qualified_lookup,
                                            qualifier_namespace, options,
                                            &err);
      } else {
        /* An ambiguous symbol. */
        if (microsoft_bugs && microsoft_version < 1300) {
          /* Check for cases where the Microsoft compiler prefers a given
             symbol.  This was fixed in the 7.0 compiler. */
          if (check_for_microsoft_qualifier_using_directive_bug(
                                 &curr_sym, fund_curr_sym, new_sym, options)) {
            /* curr_sym is set by the call above. */
            err = FALSE;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* Both symbols are functions. */
      curr_sym = merge_function_into_lookup_set(curr_sym, new_sym, locator,
                                                qualified_lookup,
                                                qualifier_namespace,
						options);
    }  /* if */
  }  /* if */
  /* Note that for cases where merge_function_into_lookup_set is called
     above, curr_sym can be NULL if new_sym contained only invisible
     symbols. */
  if (err) {
    *any_errors = TRUE;
    if (curr_sym != NULL) curr_sym->ambiguous = TRUE;
  }  /* if */
  if (curr_sym != NULL && curr_sym->decl_seq == NO_DECL_SEQUENCE_NUMBER) {
    /* Assign a declaration sequence number to the synthesized projection
       symbol if it does not yet have one. */
    set_decl_sequence_number(curr_sym);
  }  /* if */
#if EXPENSIVE_CHECKING
  if (curr_sym != NULL) {
    a_symbol_ptr	fund_curr_sym;
    fund_curr_sym = fundamental_symbol_of(curr_sym);
    /* fund_curr_sym can be NULL in some error cases. */
    if (fund_curr_sym != NULL &&
        fund_curr_sym->kind == (a_symbol_kind)sk_overloaded_function) {
      a_symbol_ptr	overload_sym;
      overload_sym = fund_curr_sym->variant.overloaded_function.symbols;
      for (; overload_sym != NULL; overload_sym = overload_sym->next) {
        check_assertion_str2(fundamental_symbol_of(overload_sym) != NULL,
                             "add_symbol_to_lookup_set:", "NULL fund_sym");
      }  /* for */
    }  /* if */
  }  /* if */
#endif /* EXPENSIVE_CHECKING */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4 || db_flag_is_set("lookup_set")) {
    if (curr_sym != NULL) {
      db_symbol(curr_sym, "add_symbol_to_lookup_set:", 0);
      if (curr_sym->kind == (a_symbol_kind)sk_overloaded_function) {
        a_symbol_ptr	overload_sym;
        overload_sym = curr_sym->variant.overloaded_function.symbols;
        for (; overload_sym != NULL; overload_sym = overload_sym->next) {
          db_symbol(fundamental_symbol_of(overload_sym), "", 4);
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return curr_sym;
}  /* add_symbol_to_lookup_set */


/*
Structure used to pass name lookup state information between routines
used to implement name lookup.
*/
typedef struct a_lookup_state *a_lookup_state_ptr;
typedef struct a_lookup_state {
  a_boolean	must_be_class_or_namespace;
			/* TRUE if the IDL_MUST_BE_CLASS_OR_NAMESPACE
			   option was specified for this lookup. */
  a_boolean	must_be_tag;
			/* TRUE if the IDL_MUST_BE_TAG option
			   was specified for this lookup. */
  a_boolean	must_be_namespace;
			/* TRUE if the IDL_MUST_BE_NAMESPACE option
			   was specified for this lookup. */
  a_boolean	must_be_class;
			/* TRUE if the IDL_MUST_BE_CLASS option
			   was specified for this lookup. */
  a_boolean	tentative_type_lookup;
			/* TRUE if the IDL_TENTATIVE_TYPE_LOOKUP option
			   was specified for this lookup. */
  a_boolean	tentative_template_lookup;
			/* TRUE if the IDL_TENTATIVE_TEMPLATE_LOOKUP option
			   was specified for this lookup. */
  a_boolean	is_linkage_lookup;
			/* TRUE if the IDL_LINKAGE_LOOKUP option
			   was specified for this lookup. */
  a_boolean	treat_as_template_id;
			/* TRUE if the IDL_TREAT_AS_TEMPLATE_ID option
			   was specified for this lookup. */
  a_boolean	is_friend_lookup;
			/* TRUE if the IDL_FRIEND_LOOKUP option
			   was specified for this lookup. */
  a_boolean	hidden_name_lookup;
			/* TRUE if the IDL_HIDDEN_NAME_LOOKUP option was
			   specified for this lookup. */
  a_boolean	do_not_create_proj_sym;
			/* TRUE if the IDL_DO_NOT_CREATE_PROJ_SYM option was
			   specified for this lookup. */
  a_boolean	suppress_decl_seq_check;
			/* TRUE if the IDL_SUPPRESS_DECL_SEQ_CHECK option was
			   specified for this lookup. */
  a_boolean	terminate_lookup;
			/* TRUE if a condition occurred that should cause
			   the lookup to terminate even if a symbol was
			   not found (i.e., a symbol was found but ignored
			   because it did not satisfy the lookup options). */
  a_boolean	skip_curr_scope;
			/* TRUE if the IDL_SKIP_CURR_SCOPE was specified for
			   this lookup. If the current scope is a class
			   scope, any base classes are still be searched
			   even though the derived class is not. */
  a_boolean	skip_class_scopes;
			/* TRUE if the IDL_SKIP_CLASS_SCOPES
			   was specified for this lookup. */
  a_boolean	skip_first_class_reactivation;
			/* TRUE in cfront mode if we have found a scope
			   for a friend function definition and that
			   we should skip the next class reactivation
			   encountered to duplicate a cfront bug. */
  a_boolean	check_for_nonreal_bases;
			/* TRUE if we are within an instantiation scope
			   and we should check whether any of the base
			   classes involved in the lookup are nonreal. */
  a_boolean	any_nonreal_bases;
			/* TRUE when check_for_nonreal_bases is TRUE and
			   a nonreal base class has been found. */
  a_boolean	any_ignored_dependent_bases;
			/* TRUE when using g++ dependent base class lookup
			   when a lookup was done in a class with dependent
			   base classes, and no symbol was found. */
  a_boolean	look_for_projected_symbol;
			/* TRUE for class and class reactivation scopes if
			   the lookup should attempt to find a projection
			   symbol that meets the lookup criteria. */
  a_boolean	look_in_dependent_bases;
			/* TRUE if the lookup can consider dependent base
			   classes generated from class templates. */
  a_boolean	look_in_interfaces;
			/* TRUE if the lookup should consider C++/CLI
			   interface classes. */
  a_boolean	inline_namespace_set_only;
			/* TRUE if the lookup should only consider
			   using-directives that refer to inline namespaces. */
  a_boolean	force_lookup_in_dependent_bases;
			/* TRUE if we are doing a special second lookup pass
			   in g++ mode and should look in dependent base
			   classes.  Also used when creating hidden name
			   lists. */
  a_boolean	add_to_active_list;
			/* TRUE if look_for_projected_symbol is TRUE and
			   the resulting projection symbol (if any) should be
			   added to the active list. */
  a_boolean	inclass_exception_spec;
			/* TRUE if the lookup is taking place in the context
			   of the instantiation of an exception specification
			   appearing in a class definition. */
#if MICROSOFT_EXTENSIONS_ALLOWED || CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  a_boolean	projection_symbol_found;
			/* TRUE if a projection symbol was found, but did
			   not meet the requirements of the lookup. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || ... */
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  a_scope_depth	last_scope_used;
			/* Last scope used to find the symbol. */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
  a_type_ptr	class_with_nonreal_base;
			/* When any_nonreal_bases is TRUE, this points to
			   the class type of the nonreal base class that
			   was found. */
  a_symbol_ptr	insert_sym;
			/* If look_for_projection_symbol and add_to_active_list
			   are TRUE, this symbol points to the location at
			   which the projection symbol should be added to the
			   active list. */
  an_id_lookup_options_set
		options;
			/* The lookup options passed into the lookup
			   routine. */
  a_name_space_kind
		required_name_space_kind;
			/* Indicates whether we are looking in the tag
			   name_space or the "other" namespace.  The tag
			   name_space is used only in C. */  
  a_decl_sequence_number
		decl_seq;
			/* The declaration sequence number of the effective
			   lookup position.  Symbols declared after this
			   position are ignored.  Zero if the declaration
			   sequence number should not be checked. */
  a_decl_sequence_number
		using_dir_decl_seq;
			/* The declaration sequence number of the effective
			   lookup position to be used when doing a
			   using-directive lookup.  Symbols declared after this
			   position are ignored.  This used used when doing
			   g++ lookup emulation. */
   a_boolean	check_decl_seq;
			/* Flag that is TRUE if the decl_seq field should be
			   compared with the corresponding value for each
			   candidate symbol. */
   a_boolean	found_template_param;
			  /* TRUE if the lookup result is a template
			     parameter. */
} a_lookup_state;


/*
A lookup state that has been cleared that can be used to initialize
new lookup state variables.
*/
STATIC_THREAD a_lookup_state
		cleared_lookup_state;


static void init_cleared_lookup_state(void)
/*
Routine that initializes cleared_lookup_state to the correct initial
value.
*/
{
  cleared_lookup_state.must_be_class_or_namespace    = FALSE;
  cleared_lookup_state.must_be_tag                   = FALSE;
  cleared_lookup_state.must_be_namespace             = FALSE;
  cleared_lookup_state.must_be_class                 = FALSE;
  cleared_lookup_state.tentative_type_lookup         = FALSE;
  cleared_lookup_state.tentative_template_lookup     = FALSE;
  cleared_lookup_state.is_linkage_lookup             = FALSE;
  cleared_lookup_state.treat_as_template_id          = FALSE;
  cleared_lookup_state.is_friend_lookup              = FALSE;
  cleared_lookup_state.hidden_name_lookup            = FALSE;
  cleared_lookup_state.do_not_create_proj_sym        = FALSE;
  cleared_lookup_state.suppress_decl_seq_check       = FALSE;
  cleared_lookup_state.terminate_lookup              = FALSE;
  cleared_lookup_state.skip_curr_scope               = FALSE;
  cleared_lookup_state.skip_class_scopes             = FALSE;
  cleared_lookup_state.skip_first_class_reactivation = FALSE;
  cleared_lookup_state.check_for_nonreal_bases       = FALSE;
  cleared_lookup_state.any_nonreal_bases             = FALSE;
  cleared_lookup_state.any_ignored_dependent_bases   = FALSE;
  cleared_lookup_state.look_for_projected_symbol     = FALSE;
  cleared_lookup_state.look_in_dependent_bases       = FALSE;
  cleared_lookup_state.look_in_interfaces            = FALSE;
  cleared_lookup_state.inline_namespace_set_only     = FALSE;
  cleared_lookup_state.force_lookup_in_dependent_bases = FALSE;
  cleared_lookup_state.add_to_active_list            = FALSE;
  cleared_lookup_state.inclass_exception_spec        = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED || CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  cleared_lookup_state.projection_symbol_found       = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || ... */
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  cleared_lookup_state.last_scope_used               = NO_SCOPE_DEPTH;
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
  cleared_lookup_state.class_with_nonreal_base       = NULL;
  cleared_lookup_state.insert_sym                    = NULL;
  cleared_lookup_state.options                       = IDL_NO_OPTIONS;
  cleared_lookup_state.required_name_space_kind      = nsk_other;
  cleared_lookup_state.decl_seq                      = 0;
  cleared_lookup_state.using_dir_decl_seq            = NO_DECL_SEQUENCE_NUMBER;
  cleared_lookup_state.check_decl_seq                = 0;
  cleared_lookup_state.found_template_param          = FALSE;
}  /* init_cleared_lookup_state */

/*
Macro that initializes a lookup state variable.
*/
#define clear_lookup_state(state) ((state) = cleared_lookup_state)


static inline a_boolean is_acceptable_invisible_symbol(
                                               a_symbol_ptr       fund_sym,
                                               a_lookup_state_ptr lookup_state)
/*
Function used by is_acceptable_symbol to determine if an invisible symbol
has an exceptional rule that allows it to be accepted.  Return TRUE if the
symbol should be accepted despite being invisible, FALSE otherwise.
*/
{
  a_boolean result = FALSE;

  /* GCC 5.x makes the function template symbol visible for parsing purposes,
     but it is ignored by overload resolution. */
  if (gpp_version_is(>= 50000) && gnu_version < 60000 && 
      symbol_is_or_contains_function_template(fund_sym)) {
    result = TRUE;
  } else if (lookup_state->is_linkage_lookup) {
    result = TRUE;
  } else if (lookup_state->is_friend_lookup) {
    result = TRUE;
  }  /* if */
  return result;
}  /* is_acceptable_invisible_symbol */


static inline a_boolean is_acceptable_template_alias_symbol(
                                              a_symbol_ptr            fund_sym,
                                              a_scope_stack_entry_ptr ssep)
/*
Function used by is_acceptable_symbol to determine if an alias symbol should be
accepted because it isn't the template associated associated with the innermost
instantiation scope.  Return TRUE if the symbol should be accepted, FALSE
otherwise.
*/
{
  a_boolean result = FALSE;

  /* Check for disqualifying conditions that prove the symbol isn't the
     template associated with the innermost instantiation scope. */
  if (ssep == NULL) {
    result = TRUE;
  } else if (depth_innermost_instantiation_scope == NO_SCOPE_DEPTH) {
    result = TRUE;
  } else if (scope_stack[depth_innermost_instantiation_scope].template_sym
                                                                     == NULL) {
    result = TRUE;
  } else if (scope_stack[depth_innermost_instantiation_scope].template_sym
                                                                 != fund_sym) {
    result = TRUE;
  }  /* if */
  return result;
}  /* is_acceptable_template_alias_symbol */


static inline a_boolean is_acceptable_symbol(
                                        a_symbol_ptr            sym,
                                        a_symbol_ptr            fund_sym,
                                        a_lookup_state_ptr      lookup_state,
                                        a_scope_stack_entry_ptr ssep,
                                        a_boolean               invisible_okay)
/*
Function used by normal_id_lookup and do_using_directive_lookup that tests
whether or not a symbol is acceptable.
*/
{
  a_boolean result = TRUE;

  if (name_space_for_symbol_kind[(int)sym->kind] !=
                                      lookup_state->required_name_space_kind) {
    result = FALSE;
  } else if ((fund_sym->is_invisible || sym->is_invisible) &&
             !invisible_okay &&
             !is_acceptable_invisible_symbol(fund_sym, lookup_state)) {
    result = FALSE;
  } else if (fund_sym->ignore_in_decl_scope &&
             !is_acceptable_template_alias_symbol(fund_sym, ssep)) {
    result = FALSE;
  } else if (lookup_state->must_be_class_or_namespace &&
             /* symbol_may_precede_qualifier checks for a symbol that is a
                class, class template, namespace, or template type
                parameter. */
             !symbol_may_precede_qualifier(fund_sym)) {
    result = FALSE;
  } else if (lookup_state->must_be_tag &&
             !is_tag_or_tag_proxy_symbol(fund_sym,
                                         lookup_state->is_friend_lookup)) {
    result = FALSE;
  } else if (lookup_state->must_be_class &&
             !is_class_or_class_proxy_symbol(fund_sym)) {
    result = FALSE;
  } else if (lookup_state->must_be_namespace &&
             !is_namespace_symbol(fund_sym)) {
    result = FALSE;
  } else if (lookup_state->is_friend_lookup &&
             lookup_state->treat_as_template_id &&
             !symbol_is(fund_sym, sk_class_template) &&
             /* Lookup for template friends is normally a type-only lookup, but
                MSVC only considers class templates. */
             (microsoft_mode || !is_type_symbol(fund_sym))) {
    result = FALSE;
  } else if (lookup_state->is_linkage_lookup &&
             lookup_state->treat_as_template_id &&
             !symbol_is(fund_sym, sk_class_template)) {
    result = FALSE;
  } else if (lookup_state->check_decl_seq &&
             lookup_state->decl_seq != NO_DECL_SEQUENCE_NUMBER &&
             lookup_state->decl_seq < sym->decl_seq) {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_acceptable_symbol */


a_boolean sym_matches_lookup_options(a_symbol_ptr		sym,
				     an_id_lookup_options_set	options)
/*
Return TRUE if the symbol specified by "sym" is acceptable according to
the lookup options specified by "options".  This routine is similar to the
is_acceptable_symbol macro, but is intended to be called from routines that
do not have the appropriate local variables (e.g., lookup_state) set that
are needed to use is_acceptable_symbol.
*/
{
  a_boolean	result;
  a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
 
  result = ((!((options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0) ||
             symbol_may_precede_qualifier(fund_sym)) &&
            (!((options & IDL_MUST_BE_TAG) != 0) ||
             is_tag_or_tag_proxy_symbol(fund_sym,
                                        (options & IDL_FRIEND_LOOKUP) != 0)) &&
            (!((options & IDL_MUST_BE_NAMESPACE) != 0) ||
             is_namespace_symbol(fund_sym)) &&
            (!((options & IDL_MUST_BE_CLASS) != 0) ||
             is_class_or_class_proxy_symbol(fund_sym)));
  return result;
}  /* sym_matches_lookup_options */


static a_boolean is_symbol_visible_for_gpp_using_dir(
			ARG_UNUSED a_scope_stack_entry_ptr	ssep,
			a_symbol_locator			*locator,
			a_lookup_state_ptr			lookup_state,
			a_symbol_ptr				sym_from_scope,
			a_symbol_ptr				new_sym,
			a_namespace_symbol_supplement_ptr	nssp)
/*
When we are doing g++ dependent name lookup the visibility of the symbol
depends on whether or not it is a function, whether the name was specified
using a template-id, and the version of g++ that we are emulating.

Symbols are sometimes visible if a using-directive was visible at the point
of definition of the template.  The using_dir_decl_seq field of the namespace
is used to determine the visibility of using-directives in g++ mode.

Consider this example:

  namespace N1 {
    template<typename T> inline void f(const T&, const T&) { }
  }

  template<typename T> inline void f(T const&, T const&) { }

  template<typename T> inline void g(T const& val1, T const& val2) {
    f<int>(val1, val2);  // call #1
    f(val1, val2);       // call #2
  }

  int v1;
  int v2;

  int main() {
    g(v1, v2);
  }

  using namespace N1;

We match the behavior of the various g++ versions, except as noted below.

Call #1 is:

  - ambiguous in g++ 3.2
  - accepted in g++ 3.3 and above, but we give an ambiguity
  - accepted in g++ 3.4 and above

Call #2 is:

  - ambiguous in g++ 3.2
  - accepted in g++ 3.3, but we give an ambiguity
  - ambiguous in g++ 3.4
  - ambiguous in g++ 4.0
  - accepted in g++ 4.1

Call #2 is accepted in g++ 4.1 because the presence of a symbol found by the
normal lookup suppresses the visibility of certain symbols that would be
found by a using-directive lookup.

Operator names seem to be visible regardless of the location of the
using-directive.

ssep is the scope stack entry at which the using-directives apply that are
being considered for this lookup.  locator identifies the kind of name being
looked up.  new_sym is the symbol being considered.  nssp is the namespace of
new_sym.  sym_from_scope is the symbol found by normal lookup in the scope in
which the using-directives apply.
*/
{
  a_boolean	visible_using_dir;
  a_boolean	result = TRUE;

#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("gpp_lookup")) {
    an_active_using_directive_ptr	audp;
    fprintf(f_debug, "g++ using-dir lookup:\n");
    fprintf(f_debug, "  nssp->using_dir_decl_seq=%lu\n",
            (unsigned long)nssp->using_dir_decl_seq);
    fprintf(f_debug, "  lookup_state->using_dir_decl_seq=%lu\n",
            (unsigned long)lookup_state->using_dir_decl_seq);
    fprintf(f_debug, "  lookup_state->decl_seq=%lu\n",
            (unsigned long)lookup_state->decl_seq);
    for (audp = ssep->using_directives_that_apply_here;
         audp != NULL; audp = audp->next_that_applies_at_depth) {
      if (nssp == audp->namespace_supplement) {
        fprintf(f_debug, "  effective_decl_seq=%lu\n",
                (unsigned long)audp->effective_decl_seq);
      }  /* if */
    }  /* for */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  visible_using_dir = 
            nssp->using_dir_decl_seq <= lookup_state->using_dir_decl_seq ||
            lookup_state->using_dir_decl_seq == NO_DECL_SEQUENCE_NUMBER ||
            locator->is_operator_name;
  if (is_function_or_template_symbol(new_sym) || visible_using_dir) {
    /* For gnu_versions < 40700, non-template symbols are visible but
       templates are not.  The symbol_is_or_contains_template test is
       an approximation of this behavior. */
    if (!visible_using_dir &&
        ((gnu_version >= 40100 &&
          (sym_from_scope != NULL &&
           (gnu_version >= 40700 ||
            symbol_is_or_contains_template(new_sym)))) ||
         (gnu_version >= 30400 && locator->is_template_id))) {
      /* This is a symbol that should be ignored in g++ mode. */
      result = FALSE;
    }  /* if */
  } else {
    /* This is a symbol that should be ignored in g++ mode. */
    result = FALSE;
  }  /* if */
  return result;
}  /* is_symbol_visible_for_gpp_using_dir */


static inline a_boolean is_acceptable_active_symbol(
                                        a_symbol_ptr            sym,
                                        a_symbol_ptr            fund_sym,
                                        a_lookup_state_ptr      lookup_state,
                                        a_scope_stack_entry_ptr ssep,
                                        a_boolean               invisible_okay)
/*
Function used by active_scope_lookup that tests whether or not a symbol on the
active list is acceptable.  Return TRUE if the symbol is in the proper name
space, FALSE otherwise.
*/
{
  a_name_space_kind ns_kind = name_space_for_symbol_kind[(int)sym->kind];
  a_boolean         result = TRUE;

  if (ns_kind != lookup_state->required_name_space_kind) {
    result = FALSE;
  } else if (!is_acceptable_symbol(sym, fund_sym, lookup_state, ssep,
                                   /*invisible_okay=*/FALSE)) {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_acceptable_active_symbol */


static a_symbol_ptr do_using_directive_lookup
                              (a_scope_stack_entry_ptr	ssep,
                               a_symbol_ptr		sym_from_scope,
                               a_symbol_locator		*locator,
                               a_lookup_state_ptr	lookup_state)
/*
Look for a symbol, as described by locator, that is in a namespace whose
scope_depth_at_which_using_directive_applies matches the scope depth of ssep.

sym_from_scope points to a symbol found in ssep by the normal_id_lookup,
and may be NULL.

If no additional symbols are found then the value of sym_from_scope
is returned to the caller.  If any additional symbols are found,
a pointer to a namespace projection symbol that reflects the results
of the lookup is returned to the caller.
*/
{
  a_symbol_ptr		synth_sym = NULL;
  a_symbol_ptr		new_sym, ns_sym;
  a_symbol_ptr		sym = sym_from_scope;
  an_active_using_directive_ptr
			audp;
  a_namespace_symbol_supplement_ptr
			nssp;
  a_namespace_ptr	nsp;
  a_boolean		gpp_namespace_only_mode = FALSE;

  db_enter(4, "do_using_directive_lookup");
  if (microsoft_bugs && microsoft_version < 1400 && sym_from_scope != NULL) {
    /* In Microsoft bugs mode for older MSVC versions, a class template
       symbol found suppresses the using-directive lookup from that
       scope. */
    a_symbol_ptr	fund_sym;
    fund_sym = fundamental_symbol_of(sym_from_scope);
    if (is_class_template_symbol(fund_sym)) {
      sym = sym_from_scope;
      goto done;
    }  /* if */
  }  /* if */
  if (gpp_mode && sym_from_scope != NULL &&
      lookup_state->must_be_class_or_namespace &&
      sym_from_scope->kind == (a_symbol_kind)sk_namespace) {
    /* In g++ mode, if a namespace is found by normal lookup and is followed by
       "::", symbols from using-directives that are not namespaces are
       ignored. */
    gpp_namespace_only_mode = TRUE;
  }  /* if */
  /* Loop through the using-directives that apply at this scope to see if
     any of them contain symbols that match the lookup options. */
  for (audp = ssep->using_directives_that_apply_here;
       audp != NULL; audp = audp->next_that_applies_at_depth) {
    if ((lookup_state->inline_namespace_set_only ||
         lookup_state->is_linkage_lookup) && !audp->entry->inline_namespace) {
      /* Ignore namespaces made visible by a using-directive unless it refers
         to an inline namespace. */
      continue;
    }  /* if */
    /* Search for a symbol in the lookup table for the namespace. */
    nssp = audp->namespace_supplement;
    ns_sym = fundamental_symbol_of(nssp->symbol);
    check_assertion(ns_sym->kind == (a_symbol_kind)sk_namespace);
    nsp = skip_namespace_aliases(ns_sym->variant.namespace_info.ptr);
    load_lazy_symbols_if_needed(nsp->variant.assoc_scope, locator);
    for (a_module_ptr mod : curr_lookup_modules()) {
      for (new_sym = find_symbol_list_in_table(
                  curr_lookup_table(&nssp->pointers_block, mod, sck_namespace),
                  locator->symbol_header);
           new_sym != NULL;
           new_sym = new_sym->next_in_lookup_table) {
        /* Look through the symbols associated with the given namespace.  Note
           that we keep looking even if an ambiguity is detected.  The symbol
           pointed to by the ambiguous synthesized projection symbol may
           differ depending on the lookup options. */
        a_symbol_ptr fund_sym;
        a_boolean    any_errors = FALSE;

        /* Ignore symbols that do not match the lookup requirements. */
        fund_sym = fundamental_symbol_of(new_sym);
        if (!is_acceptable_symbol(new_sym, fund_sym, lookup_state, ssep,
                                  /*invisible_okay=*/FALSE)) {
          continue;
        }  /* if */
        if (gpp_namespace_only_mode && fund_sym->kind != sk_namespace) {
          /* Non-namespace symbols should be ignored.  See the comment about
             gpp_namespace_only_mode for more information. */
          continue;
        }  /* if */
        if (gpp_using_directive_lookup) {
          /* In normal mode, only a namespace test is needed.  When we are
             doing g++ dependent name lookup, a more complex test is needed to
             emulate the g++ using-directive visibility rules. */
          if (!is_symbol_visible_for_gpp_using_dir(ssep, locator, lookup_state,
                                                   sym_from_scope,
                                                   new_sym, nssp)) {
            continue;
          }  /* if */
        }  /* if */
        if (synth_sym == NULL) {
          /* Look for a previous synthesized namespace projection symbol for
             this scope. */
          synth_sym = find_synthesized_projection_symbol(
                                                locator, lookup_state->options,
                                                /*qualified_lookup=*/FALSE,
                                                (a_namespace_ptr)NULL);
          if (sym != NULL) {
            /* The lookup from this scope did find a symbol.  Put it in the
               lookup set. */
            synth_sym = add_symbol_to_lookup_set(synth_sym, sym,
                                                 locator,
                                                 /*qualified_lookup=*/FALSE,
                                                 (a_namespace_ptr)NULL,
                                                 lookup_state->options,
                                                 &any_errors);
          }  /* if */
          sym = synth_sym;
        }  /* if */
        /* Merge the information about this symbol, with that
           of any previous symbol that was found. */
        sym = add_symbol_to_lookup_set(sym, new_sym,
                                       locator,
                                       /*qualified_lookup=*/FALSE,
                                       (a_namespace_ptr)NULL,
                                       lookup_state->options,
                                       &any_errors);
        /* Set synth_sym in case it was not set earlier.  This
           suppresses subsequent attempts to look up synth_sym. */
        synth_sym = sym;
      }  /* for */
    }  /* for */
  }  /* for */
done:
  db_exit();
  return sym;
}  /* do_using_directive_lookup */


static a_symbol_ptr do_normal_using_directive_lookup_if_needed(
				a_scope_stack_entry_ptr	ssep,
				a_symbol_ptr		sym_from_scope,
				a_symbol_locator	*locator,
				a_lookup_state_ptr	lookup_state)
/*
This routine is called during normal (unqualified) lookup to perform
a using-directive lookup if appropriate based on the current scope and
lookup state.  The result of the lookup is returned, or sym_from_scope if
no lookup is done.  See do_using_directive_lookup for a description of
the parameters.
*/
{
  a_symbol_ptr	sym = sym_from_scope;
  a_scope_kind	kind = ssep->kind;

  /* If this is a namespace scope or the file scope, also look for
     any symbols that are visible because of using directives.
     Starting with g++ 4.0, a symbol from a using-directive is only
     considered if the normal lookup did not find a symbol. */
  if ((kind == (a_scope_kind)sck_file ||
       kind == (a_scope_kind)sck_namespace_extension ||
       kind == (a_scope_kind)sck_namespace_reactivation ||
       kind == (a_scope_kind)sck_namespace) &&
      ssep->using_directives_that_apply_here != NULL &&
      (!lookup_state->is_friend_lookup ||
       (friend_class_decl_can_find_using_dir &&
        (!gpp_mode || gnu_version < 40000 || sym_from_scope == NULL)))) {
    sym = do_using_directive_lookup(ssep, sym_from_scope, locator,
                                    lookup_state);
  }  /* if */
  return sym;
}  /* do_normal_using_directive_lookup_if_needed */


static a_boolean should_skip_active_sym(a_symbol_ptr   sym,
                                        a_scope_number scope_number)
/*
Return TRUE if the given symbol should be skipped when looking for active
symbols in the given scope number and additionally, if non-NULL, the current
module; otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  if (sym != NULL) {
    if (sym->decl_scope != scope_number) {
      result = TRUE;
    } else if (!is_symbol_currently_lookup_visible(sym)) {
      /* The symbol is not currently visible to lookup. */
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* should_skip_active_sym */


static
a_symbol_ptr active_scope_lookup(a_scope_kind       kind,
                                 a_scope_depth      scope_depth,
                                 a_symbol_locator   *locator,
                                 a_lookup_state_ptr lookup_state)
/*
This routine is called as part of normal_id_lookup processing to handle
scopes for which the symbols are on the active list.

Note that for namespace extension and reactivation scopes, kind will have
been changed to sck_namespace if the symbols for the namespace are
still on the active list.  As a result, this routine will also be called
for namespace extension and reactivation scopes in such cases.
Note also that this routine will also be called for class reactivations
of classes whose class scope is still on the scope stack (i.e., for classes
that are still in the process of being defined.  scope_depth is the index into
scope_stack for which symbols are being considered.  locator is the symbol
locator for the name being looked up.  lookup_state is used to pass state
information between the various routines that do normal id lookup processing.

WARNING: A scope stack lookup may involve lazy loading of symbols which may
cause scope_stack to be re-allocated, invalidating any saved pointers into the
scope stack.  Any such pointers will need to be reset after a call to this
routine.
*/
{
  a_symbol_ptr            sym = NULL;
  a_symbol_ptr            active_sym;
  a_symbol_ptr            prev_active_sym = NULL;
  a_symbol_ptr            type_tag_symbol = NULL;
  a_boolean               saved_check_decl_seq;
  a_scope_stack_entry_ptr ssep = scope_stack_entry_for(scope_depth);

  /* The decl_seq check should not be done on function parameters in certain
     noexcept contexts. */
  saved_check_decl_seq = lookup_state->check_decl_seq;
  if (lookup_state->inclass_exception_spec &&
      (kind == (a_scope_kind)sck_func_prototype ||
       kind == (a_scope_kind)sck_function)) {
    lookup_state->check_decl_seq = FALSE;
  }  /* if */
  if (lookup_state->skip_curr_scope) {
    /* Skip this scope.  Note that we still look for a projected symbol. */
  } else {
    load_lazy_symbols_if_needed(ssep->il_scope, locator);
    /* Lazy loading of symbols can cause the scope stack to be re-allocated,
       invalidating the current pointer. */
    ssep = scope_stack_entry_for(scope_depth);
    prev_active_sym = NULL;
    active_sym = symbol_list_from_locator(*locator);
    while (should_skip_active_sym(active_sym, ssep->number)) {
      prev_active_sym = active_sym;
      active_sym = active_sym->next;
    }  /* while */
    for (; active_sym != NULL && active_sym->decl_scope == ssep->number;
         prev_active_sym = active_sym, active_sym = active_sym->next) {

      while (should_skip_active_sym(active_sym, ssep->number)) {
        prev_active_sym = active_sym;
        active_sym = active_sym->next;
      }  /* while */
      if (active_sym == NULL) {
        break;
      }  /* if */

      a_symbol_ptr fund_sym = fundamental_symbol_of(active_sym);
      /* Exit the loop if we found a type tag symbol and the new symbol
         to check is not from the same scope. */
      if (type_tag_symbol != NULL &&
          type_tag_symbol->decl_scope != active_sym->decl_scope) {
        break;
      }  /* if */
      if (is_acceptable_active_symbol(active_sym, fund_sym, lookup_state,
                                      ssep, /*invisible_okay=*/FALSE)) {
        /* We found a matching symbol.  If this is a type symbol found
           by a must-be-tag lookup, keep searching for a "real" tag in
           the same scope. */
        if (lookup_state->must_be_tag &&
            fund_sym->kind == (a_symbol_kind)sk_type) {
          /* If a tag symbol is followed by a projection to a different tag,
             use the first symbol. */
          check_assertion_or_expect_error(
                               type_tag_symbol == NULL ||
                               symbol_is(active_sym, sk_projection) ||
                               symbol_is(active_sym, sk_namespace_projection));
          if (type_tag_symbol == NULL) type_tag_symbol = active_sym;
        } else {
          /* Use this symbol. */
          sym = active_sym;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
    /* If a type symbol was found and no other matching tag was present,
       use the type symbol. */
    if (sym == NULL && type_tag_symbol != NULL) sym = type_tag_symbol;
    /* Do a using-directive lookup if needed based on the scope and
       lookup options. */
    sym = do_normal_using_directive_lookup_if_needed(ssep, sym, locator,
                                                     lookup_state);
  }  /* if */
  if (sym == NULL &&
      !(lookup_state->options & IDL_TREAT_AS_TEMPLATE_ID &&
       (lookup_state->is_linkage_lookup || lookup_state->is_friend_lookup)) &&
      (kind == (a_scope_kind)sck_class_struct_union ||
       kind == (a_scope_kind)sck_class_reactivation) && !C_mode()) {
    /* For class scopes, look for a symbol projected (inherited)
       into the class scope if a symbol was not found in the class
       itself.  Don't look in dependent base classes for classes that
       are generated from templates.  In a friend class template declaration
       (covered by the test that includes is_linkage_lookup and
       is_friend_lookup tests), don't look for a projection symbol. */
    lookup_state->look_for_projected_symbol = TRUE;
    lookup_state->look_in_dependent_bases =
                      (depth_innermost_instantiation_scope == NO_SCOPE_DEPTH &&
                       depth_template_declaration_scope == NO_SCOPE_DEPTH) ||
                      lookup_state->force_lookup_in_dependent_bases ||
                      !is_unspecialized_template_class(ssep->assoc_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* If lookup begins in a C++/CLI class, base interface classes are not
       considered. */
    lookup_state->look_in_interfaces =
                              !treat_as_cli_class_for_lookup(ssep->assoc_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    lookup_state->add_to_active_list = TRUE;
    lookup_state->insert_sym = prev_active_sym;
  }  /* if */
  lookup_state->check_decl_seq = saved_check_decl_seq;
  return sym;
}  /* active_scope_lookup */


static
a_symbol_ptr inactive_scope_lookup(a_scope_kind       kind,
                                   a_scope_depth      scope_depth,
                                   a_symbol_locator   *locator,
                                   a_lookup_state_ptr lookup_state)
/*
This routine is called as part of normal_id_lookup processing to handle
scopes for which the symbols are now on the inactive list.  This
includes class reactivations, namespace reactivations (except when the
original namespace is still on the scope stack), and template instantiation
scopes.

Note that for namespace extension and reactivation scopes, kind will have
been changed to sck_namespace if the symbols for the namespace are
still on the active list (this routine will not be called for such
scopes).

scope_depth is the index into scope_stack for which symbols are being
considered.  locator is the symbol locator for the name being looked up.
lookup_state is used to pass state information between the various routines
that do normal id lookup processing.

WARNING: A scope stack lookup may involve lazy loading of symbols which may
cause scope_stack to be re-allocated, invalidating any saved pointers into the
scope stack.  Any such pointers will need to be reset after a call to this
routine.
*/
{
  a_boolean     skip_scope = FALSE;
  a_symbol_ptr  sym = NULL;
  a_boolean     saved_check_decl_seq;
  a_boolean     found_template_param = FALSE;
  a_scope_stack_entry_ptr
                ssep = scope_stack_entry_for(scope_depth);

  saved_check_decl_seq = lookup_state->check_decl_seq;
  if (cfront_2_1_mode &&
      kind == (a_scope_kind)sck_class_reactivation &&
      lookup_state->skip_first_class_reactivation) {
     /* This is used to skip class reactivation scopes when
        processing friend declarations in cfront compatibility
        mode.  Cfront ignores the innermost class reactivation
        scope when processing friend functions. */
    lookup_state->skip_first_class_reactivation = FALSE;
    skip_scope = TRUE;
  }  /* if */
  if (kind == (a_scope_kind)sck_class_reactivation &&
      lookup_state->skip_class_scopes) {
    /* This is a class scope and we are skipping class scopes. */
    skip_scope = TRUE;
  }  /* if */
  if (!skip_scope) {
    if (ssep->reactivated_class_being_defined) {
      /* If the class that is being reactivated is still in the process of
         being defined, look on the active list for the symbols instead of
         looking on the inactive list as is usually the case. */
      sym = active_scope_lookup(kind, scope_depth, locator, lookup_state);
      /* A lookup may involve lazy loading of symbols, which can cause
         scope_stack to be re-allocated, invalidating ssep. */
      ssep = scope_stack_entry_for(scope_depth);
    } else {
      if (lookup_state->skip_curr_scope) {
        /* Skip this scope.  Note that we still look for a projected symbol. */
      } else {
        /* Look on the inactive list for a symbol from this reactivated
           scope. */
        a_module_lookup_array modules;

        if (is_file_or_namespace_scope(ssep)) {
          load_lazy_symbols_if_needed(ssep->il_scope, locator);
          modules = curr_lookup_modules();
        } else {
          /* In the non-namespace scope case, only the NULL module is relevant.
             To prevent extra traversals, instead always use a single NULL
             module pointer value. */
          modules.push_back(NULL);
        }  /* if */
        for (a_module_ptr mod : modules) {
          a_boolean        use_lookup_table;
          a_boolean        use_scope_list = FALSE;
          a_boolean        process_single_symbol = FALSE;
          a_hash_table_ptr lookup_table;

          /* Lazy loading of symbols may cause scope_stack to be re-allocated,
             invalidating ssep. */
          ssep = scope_stack_entry_for(scope_depth);
          lookup_table = curr_lookup_table(ssep, mod);
          use_lookup_table = lookup_table != NULL;
          if (ssep->is_reactivation && is_local_scope_kind(ssep->kind)) {
            use_scope_list = TRUE;
          }  /* if */
          /* If the scope has a lookup table, use it.  Otherwise, use the
             inactive list. */
          if (use_lookup_table) {
            sym = find_symbol_list_in_table(lookup_table,
                                            locator->symbol_header);
          } else if (use_scope_list) {
            /* Get the symbol list from the scope stack entry.  Note that
               if the scope is on the stack more than once, the depth will
               be the first occurrence.  This is important because the
               symbol list for the other entries will not be correct if
               the first occurrence is for the initial use of the scope.
               If this is a reactivation and the primary entry for the
               scope is not on the stack, get the symbol list from the
               scope. */
            a_scope_depth depth;

            check_assertion(ssep->il_scope != NULL);
            depth = ssep->il_scope->depth_in_scope_stack;
            check_assertion(depth != NO_SCOPE_DEPTH);
            if (scope_stack[depth].is_reactivation) {
              sym = ssep->il_scope->symbols;
            } else {
              sym = assoc_pointers_block_of(&scope_stack[depth])->symbols;
            }  /* if */
          } else if (scope_is(ssep, sck_template_instantiation) ||
                     scope_is(ssep, sck_template_declaration)) {
            /* Template instantiation and template declaration scopes don't
               have a lookup table or a scope list that can be used.  Go
               through the template parameter list of the scope. */
            a_template_param_ptr tpp = ssep->template_decl_info->parameters;

            for (; tpp != NULL; tpp = tpp->next) {
              if (tpp->param_symbol->header == locator->symbol_header) {
                sym = tpp->param_symbol;
                process_single_symbol = TRUE;
                /* Suppress the decl_seq check for template parameters. */
                lookup_state->check_decl_seq = FALSE;
                found_template_param = TRUE;
                break;
              }  /* if */
            }  /* for */
          } else {
            sym = inactive_symbol_list_from_locator(*locator);
          }  /* if */

          a_symbol_ptr type_tag_symbol = NULL;
          a_symbol_ptr tag_symbol = NULL;
          a_symbol_ptr namespace_symbol = NULL;
          a_symbol_ptr next_sym;
          for (; sym != NULL; sym = next_sym) {
            /* Determine the next symbol to be used based on which list we used
               to find the symbol. */
            if (use_lookup_table) {
              next_sym = sym->next_in_lookup_table;
            } else if (use_scope_list) {
              next_sym = sym->next_in_scope;
            } else if (process_single_symbol) {
              next_sym = NULL;
            } else {
              next_sym = sym->next;
            }  /* if */
            /* The symbol header test is really only needed when the scope list
               is being used. */
            if (sym->decl_scope == ssep->number &&
                sym->header == locator->symbol_header) {
              a_symbol_ptr fund_sym = fundamental_symbol_of(sym);

              if (is_acceptable_symbol(sym, fund_sym, lookup_state, ssep,
                                       /*invisible_okay=*/FALSE)) {
                /* Found a symbol. */
                /* If this is a template parameter symbol that should not
                   be visible then continue looking for another symbol. */
                if (sym->template_param_not_visible) continue;
                /* When looking for a tag symbol, both tags and typedefs may
                   match the "acceptable" test.  The tag should be preferred
                   over the typedef, so if we find a typedef we must keep
                   looking.  Similarly, when not doing a "must be tag" lookup,
                   both tags and non-tags will match the test.  A non-tag
                   should be preferred over the tag, so if we find a tag we
                   must keep looking. */
                if (!lookup_state->must_be_tag) {
                  /* A normal lookup. */
                  if (is_tag_symbol(fund_sym)) {
                    if (tag_symbol != NULL &&
                        (symbol_is(tag_symbol, sk_projection) ||
                         symbol_is(tag_symbol, sk_namespace_projection))) {
                      /* If a using-declaration to a tag symbol is followed by
                         an actual tag symbol, ignore the first one. */
                      tag_symbol = NULL;
                    }  /* if */
                    /* If a tag symbol is followed by a projection to a
                       different tag, use the first symbol. */
                    check_assertion_or_expect_error(
                                      tag_symbol == NULL ||
                                      symbol_is(sym, sk_projection) ||
                                      symbol_is(sym, sk_namespace_projection));
                    if (tag_symbol == NULL) tag_symbol = sym;
                  } else {
                    if (is_namespace_symbol(sym) &&
                        gnu_namespace_and_class_in_same_scope) {
                      /* Some versions of g++ allow a namespace and class
                         with the same name in a scope.  The class name
                         should be preferred.  If we found the namespace
                         keep looking in case we find a class. */
                      namespace_symbol = sym;
                    } else {
                      /* Take the symbol. */
                      break;
                    }  /* if */
                  }  /* if */
                } else {
                  /* A tag lookup. */
                  if (sym->kind == (a_symbol_kind)sk_type) {
                    if (type_tag_symbol != NULL &&
                        (symbol_is(type_tag_symbol, sk_projection) ||
                         symbol_is(type_tag_symbol, sk_namespace_projection))){
                      /* If a using-declaration to a tag symbol is followed by
                         an actual tag symbol, ignore the first one. */
                      type_tag_symbol = NULL;
                    }  /* if */
                    /* If a tag symbol is followed by a projection to a
                       different tag, use the first symbol. */
                    check_assertion_or_expect_error(
                                      type_tag_symbol == NULL ||
                                      symbol_is(sym, sk_projection) ||
                                      symbol_is(sym, sk_namespace_projection));
                    if (type_tag_symbol == NULL) type_tag_symbol = sym;
                  } else {
                    /* Take the symbol. */
                    break;
                  }  /* if */
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* for */
          if (sym == NULL) {
            /* If a type symbol was found and no other matching tag was
               present, use the type symbol. */
            if (type_tag_symbol != NULL) {
              sym = type_tag_symbol;
            } else if (tag_symbol != NULL) {
              /* If there is a tag symbol saved within the loop, use it. */
              sym = tag_symbol;
            } else if (namespace_symbol != NULL) {
              /* If a namespace symbol was saved, use it. */
              sym = namespace_symbol;
            }  /* if */
          }  /* if */
          /* Do a using-directive lookup if needed based on the scope and
             lookup options. */
          sym = do_normal_using_directive_lookup_if_needed(ssep, sym, locator,
                                                           lookup_state);
          if (sym != NULL) {
            /* Use the symbol. */
            break;
          }  /* if */
        }  /* for */
      }  /* if */
      if (sym == NULL && kind == (a_scope_kind)sck_class_reactivation) {
        /* There is no inactive symbol that is in this class. */
        /* Look for a symbol projected (inherited) into this class.
           Don't look in dependent base classes if the associated scope is
           a generated template class. */
        lookup_state->look_for_projected_symbol = TRUE;
        lookup_state->look_in_dependent_bases =
                      (depth_innermost_instantiation_scope == NO_SCOPE_DEPTH &&
                       depth_template_declaration_scope == NO_SCOPE_DEPTH) ||
                      lookup_state->force_lookup_in_dependent_bases ||
                      !is_unspecialized_template_class(ssep->assoc_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
        /* If lookup begins in a C++/CLI class, base interface classes are not
           considered. */
        lookup_state->look_in_interfaces =
                              !treat_as_cli_class_for_lookup(ssep->assoc_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        lookup_state->add_to_active_list = FALSE;
        lookup_state->insert_sym = NULL;
      }  /* if */
      /* The Microsoft compiler (versions prior to 7.0) ignores inherited
         injected class names in most cases.  The principal case in which
         it is found is at the start of a qualified name.  The name is also
         found for certain lookups (which we approximate using the tentative
         type lookup flag) when the name is the name of the current class
         (but not an injected name from a base class).  Compilers for
         versions >= 7.0 and < 8.0 continue to ignore the injected class name
         of template instances so injected class names are only created for
         explicitly specialized instances. */
      if (sym != NULL && microsoft_bugs &&
          !lookup_state->must_be_class_or_namespace) {
        a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
        if ((!lookup_state->tentative_type_lookup ||
             fund_sym != sym) &&
            is_injected_class_symbol(fundamental_symbol_of(sym))) {
          if (microsoft_version < 1300 ||
             (fund_sym != sym && microsoft_version < 1400 &&
              is_injected_template_symbol(sym))) {
            sym = NULL;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  lookup_state->check_decl_seq = saved_check_decl_seq;
  lookup_state->found_template_param = found_template_param;
  return sym;
}  /* inactive_scope_lookup */


a_symbol_ptr namespace_definition_id_lookup(a_symbol_locator  *locator)
/*
Lookup, in the current scope and the inline namespace set, the identifier
indicated by *locator and return a pointer to the symbol found, or NULL if no
symbol is found.  Only namespace symbols are considered in the inline namespace
set.  If more than one namespace symbol is found, the lookup is ambiguous.
*/
{
  a_symbol_ptr                  sym;
  a_scope_stack_entry_ptr       ssep;

  if (is_error_locator(*locator)) {
    /* The locator is an error locator, so return NULL (i.e., no symbol
       found). */
    sym = NULL;
  } else {
    a_lookup_state  lookup_state;
    a_symbol_ptr    ns_sym = NULL;
    sym = curr_scope_id_lookup(locator, IDL_NO_OPTIONS);
    ssep = scope_stack_entry_for(decl_scope_level);
    /* Continue the search in the inline namespace set, but only consider
       namespace symbols. */
    if (sym != NULL && sym->kind == sk_namespace) {
      /* Only a namespace symbol in the current scope can cause an
         ambiguity. */
      ns_sym = locator->specific_symbol;
    }  /* if */
    clear_lookup_state(lookup_state);
    lookup_state.inline_namespace_set_only = TRUE;
    lookup_state.must_be_namespace = TRUE;
    ns_sym = do_using_directive_lookup(ssep, ns_sym, locator, &lookup_state);
    if (ns_sym != NULL) {
      sym = ns_sym;
      locator->specific_symbol = ns_sym;
      /* If the symbol is a projection symbol, reduce it to the fundamental
         symbol.  The specific_symbol in the locator stays pointing to the
         projection symbol. */
      reduce_projection_symbol_to_fundamental_symbol(sym);
    }  /* if */
  }  /* if */
  return sym;
}  /* namespace_definition_id_lookup */


static
a_symbol_ptr look_for_projected_symbol(a_scope_stack_entry_ptr	ssep,
				       a_symbol_locator		*locator,
				       a_lookup_state_ptr	lookup_state)
/*
This routine is called as part of the normal_id_lookup processing when
a class or class reactivation scope is encountered that does not
satisfy the lookup.  In such cases we need to see if a projection symbol
to a base class symbol could satisfy the lookup.

If a projection symbol is found that does not satisfy the lookup (e.g.,
it is not a type and we are doing a tentative type lookup), a NULL
symbol is returned and terminate_lookup is set to TRUE.  ssep points to
the scope stack entry for the class or class reactivation scope
being processed.  locator is the symbol locator for the name being looked up.
lookup_state is used to pass state information between the various routines
that do normal id lookup processing.
*/
{
  a_symbol_ptr	sym = NULL;

#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled && lookup_state->must_be_class_or_namespace) {
    /* The Microsoft compiler will find the name of a direct interface
       even when not looking inside the interfaces. */
    a_class_type_supplement_ptr	ctsp;
    a_base_class_ptr		bcp;
    ctsp = ssep->assoc_type->variant.class_struct_union.extra_info;
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp->direct && is_cli_interface_type(bcp->type)) {
        a_symbol_ptr	base_sym = symbol_for(bcp->type);
        if (base_sym->header == locator->symbol_header &&
            is_acceptable_symbol(base_sym, base_sym, lookup_state, ssep,
                                 /*invisble_okay=*/FALSE)) {
          sym = base_sym;
          goto end_lookup;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  check_assertion(!C_mode());
  if (find_projected_symbol(ssep->assoc_type, locator,
                            lookup_state->options,
                            lookup_state->look_in_dependent_bases ||
                                                 ssep->treat_as_specialization,
                            lookup_state->look_in_interfaces,
                            lookup_state->tentative_type_lookup,
                            lookup_state->tentative_template_lookup,
                            lookup_state->hidden_name_lookup ||
                            lookup_state->do_not_create_proj_sym ||
                            ssep->assoc_type->variant.class_struct_union.
                                                              is_nonreal_class,
                            lookup_state->add_to_active_list,
                            lookup_state->insert_sym, &sym,
                            /*can_create_nonreal=*/FALSE)) {
    if (sym == NULL) {
      /* A symbol was found in a base class, but it was not returned
         (presumably because must_be_type_name was not satisfied).
         Don't continue looking. */
#if MICROSOFT_EXTENSIONS_ALLOWED || CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
      lookup_state->projection_symbol_found = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || ... */
      lookup_state->terminate_lookup = TRUE;
    } else {
      /* A projection symbol was created (or the fundamental symbol was
         returned, in the case of a hidden name lookup).  It must still
         satisfy the constraints for this lookup. */
      a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
      /* An invisible projection symbol can be returned by g++ mode
         lookups. */
      if (!is_acceptable_symbol(sym, fund_sym, lookup_state, ssep,
                                /*invisible_okay=*/TRUE)) {
        sym = NULL;
      } else if (microsoft_bugs) {
        /* The Microsoft compiler (versions prior to 7.0) ignores inherited
           injected class names in most cases.  The principal case in which
           it is found is at the start of a qualified name.   Compilers for
           versions >= 7.0 and < 8.0 continue to ignore the injected class name
           of template instances, so injected class names are only created for
           explicitly specialized instances.*/
        if (!lookup_state->must_be_class_or_namespace &&
            is_injected_class_symbol(fund_sym)) {
          if (microsoft_version < 1300 ||
              (microsoft_version < 1400 &&
               is_injected_template_symbol(fund_sym))) {
            sym = NULL;
          }  /* if */
        }  /* if */
      } else if (gpp_mode && gnu_version < 40500) {
        /* g++ ignores inherited injected class names from template classes. */
        if (!lookup_state->must_be_class_or_namespace &&
            is_injected_template_symbol(fund_sym)) {
          sym = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    if (lookup_state->check_for_nonreal_bases) {
      /* No projection symbol was found.  If the class has any
         nonreal base classes record this information for possible
         later use.  This is repeated for each enclosing class that has
         nonreal bases so that the outermost such class will be used. */
      if (symbol_supplement_for_class(ssep->assoc_type)->
                                                    any_nonreal_base_classes) {
        lookup_state->class_with_nonreal_base = ssep->assoc_type;
        lookup_state->any_nonreal_bases = TRUE;
      }  /* if */
    }  /* if */
    if (gpp_dependent_name_lookup &&
        !lookup_state->look_in_dependent_bases &&
        symbol_supplement_for_class(ssep->assoc_type)
                                                ->any_dependent_base_classes) {
      /* Record the fact that we looked for (and did not find) a name from
         a dependent base class. */
      lookup_state->any_ignored_dependent_bases = TRUE;
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
end_lookup:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return sym;
}  /* look_for_projected_symbol */


a_type_ptr look_up_qualifier_in_dependent_bases(a_type_ptr  tp)
/*
The given type (call it T) is a nondependent type used as a name qualifier in
GCC mode (in a using-declaration), but GCC treats it as a dependent type
inherited from a dependent base.  The current scope is a class scope.  If the
associated class has dependent bases, return a nonreal type obtained by
looking up the name of T in those nonreal bases.
*/
{
  a_symbol          *sym;
  a_symbol_locator  loc;
  a_type_ptr        result = tp, class_type;

  check_assertion(gpp_version_is(any_version) &&
                  scope_is(&scope_stack_top(), sck_class_struct_union));
  class_type = scope_stack_top().assoc_type;
  if (class_symbol_supp(symbol_for(class_type))->any_dependent_base_classes) {
    make_locator_for_symbol(symbol_for(tp), &loc);
    clear_specific_symbol(loc);
    if (find_projected_symbol(class_type, &loc, IDL_MUST_BE_CLASS,
                              /*look_in_dependent_bases=*/TRUE,
                              /*look_in_interfaces=*/FALSE,
                              /*tentative_type_lookup=*/FALSE,
                              /*tentative_template_lookup=*/FALSE,
                              /*do_not_create_proj_sym=*/FALSE,
                              /*add_to_active_list=*/FALSE,
                              /*insert_sym=*/(a_symbol*)NULL,
                              &sym, /*can_create_nonreal=*/TRUE)) {
      reduce_projection_symbol_to_fundamental_symbol(sym);
      result = type_symbol_type(sym);
    }  /* if */
  }  /* if */
  return result;
}  /* look_up_qualifier_in_dependent_bases */


a_symbol_ptr find_conversion_template_instance(
			a_symbol_locator		*locator,
			a_symbol_list_entry_ptr		conversion_templates,
                        a_boolean                       match_fn_qualifiers,
                        a_type_qualifier_set            fn_qualifiers)
/*
locator is a symbol locator for a conversion function.  conversion_templates
is a list of conversion templates for the class in which the lookup is being
done.  Go through the conversion template list and find any templates that can
supply an appropriate conversion function.  If match_fn_qualifiers is TRUE,
this is a declarative context and the conversion template must have the
indicated qualifiers.  Otherwise, this is an expression context and multiple
matches should be resolved through overload resolution.  Return the symbol for
the matching function or an overloaded function symbol if there are multiple
matches.  If more than one match is found and match_fn_qualifiers is TRUE,
create an ambiguous symbol and return a pointer.  If no match is found, return
NULL.
*/
{
  a_symbol_list_entry_ptr	slep;
  a_type_ptr			result_type =
                                       locator->variant.conversion_result_type;
  a_symbol_ptr			result_sym = NULL;
  a_partial_order_candidate_ptr	candidate_list = NULL;

  if (deduced_return_types_enabled) {
    a_type_ptr  utp = skip_typerefs(result_type);
    while (utp->kind == (a_type_kind)tk_pointer) {
      utp = skip_typerefs(utp->variant.pointer.type);
    }  /* if */
    if (is_auto_type(utp)) {
      goto done;
    }  /* if */
  }  /* if */
  /* Loop though each of the templates. */
  for (slep = conversion_templates; slep != NULL; slep = slep->next) {
    a_symbol_ptr			sym;
    a_symbol_ptr			fund_sym;
    a_template_symbol_supplement_ptr	tssp;
    a_template_arg_ptr			templ_arg_list = NULL;
    a_routine_ptr			rout_ptr;
    a_type_ptr	       			rout_type;
    a_type_ptr				return_type;
    a_template_param_ptr		param_list;
    sym = slep->symbol;
    fund_sym = fundamental_symbol_of(sym);
    tssp = template_supplement_for_symbol(fund_sym);
    rout_ptr = fund_sym->variant.template_info->variant.function.routine;
    rout_type = skip_typerefs(rout_ptr->type);
    if (match_fn_qualifiers &&
        rout_type->variant.routine.extra_info->qualifiers != fn_qualifiers) {
      continue;
    }  /* if */
    return_type = rout_type->variant.routine.return_type;
    param_list = tssp->variant.function.decl_cache->decl_info->parameters;
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_flag_is_set("conversion_lookup")) {
      fprintf(f_debug, "Looking for conversion template match with:\n");
      db_symbol(sym, "", 2);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    /* See if the type specified matches the return type of the conversion
       function. */
    if (matches_template_type(result_type, return_type, &templ_arg_list,
                              param_list, MTT_NO_FLAGS)) {
      /* Do the wrapup processing to make sure that all of the parameters
         have been deduced. */
      if (wrapup_function_template_argument_deduction(
               &templ_arg_list, fund_sym, (a_template_param_ptr)NULL,
               CTWS_IS_OVERLOAD_CANDIDATE, /*param_count=*/0) != NULL) {
        /* We have a match.  Add the matching template to a list of matching
           candidates.  Any poorer matches will be removed by this process.
           The template argument list is saved along with the symbol. */
        add_to_partial_order_candidates_list(&candidate_list, sym,
                                             templ_arg_list);
        /* Set the argument list pointer to NULL so it won't be freed
           below. */
        templ_arg_list = NULL;
      }  /* if */
    }  /* if */
    /* Free the template argument list.  The pointer will have been
       set to NULL above if we need to save this list. */
    if (templ_arg_list != NULL) free_template_arg_list(templ_arg_list);
  }  /* for */
  if (candidate_list != NULL) {
    a_boolean		ambiguous = FALSE;
    a_symbol_ptr	matching_sym = NULL;
    a_template_arg_ptr	matching_arg_list = NULL;
    if (match_fn_qualifiers) {
      /* Select the best matching candidate and its template argument list. */
      select_best_partial_order_candidate(candidate_list, (a_symbol_ptr)NULL,
                                          &matching_sym, &matching_arg_list,
                                          &ambiguous);
      /* Find or create the template instance that matches the type needed.
         Note that the template argument list is freed in the called
         function. */
      result_sym = find_template_function(matching_sym, &matching_arg_list,
                                          (a_boolean)locator->is_template_id,
                                          &locator->source_position);
      free_template_arg_list(matching_arg_list);
      if (ambiguous || matching_sym->ambiguous) {
        a_symbol_ptr  new_sym;
        /* Create a copy of the result_sym and mark that copy as ambiguous. */
        new_sym = alloc_symbol((a_symbol_kind)sk_member_function,
                               result_sym->header, &locator->source_position);
        set_class_membership(new_sym, (a_source_correspondence*)NULL,
                             sym_parent_class(result_sym));
        new_sym->ambiguous = TRUE;
        new_sym->variant.routine.ptr = result_sym->variant.routine.ptr;
        new_sym->variant.routine.instance_ptr =
                                     result_sym->variant.routine.instance_ptr;
        result_sym = new_sym;
      }  /* if */
    } else {
      /* An expression context: Instantiate each candidate and produce an
         ad-hoc overload set if needed to combine those candidates. */
      a_partial_order_candidate_ptr  candidate = candidate_list,
                                     next_candidate;
      a_symbol_ptr                   instance_sym, new_sym, ovld_sym;
      for (; candidate != NULL; candidate = next_candidate) {
        next_candidate = candidate->next;
        candidate->next = NULL;
        instance_sym = find_template_function(
                                           candidate->symbol,
                                           &candidate->template_arg_list,
                                           (a_boolean)locator->is_template_id,
                                           &locator->source_position);
        if (next_candidate == NULL && result_sym == NULL) {
          /* There is only one candidate.  So we can use the symbol
             directly. */
          result_sym = instance_sym;
        } else {
          /* Make a copy of the instance symbol since we may have to
             synthesize an overload set for this case (which would overwrite
             the "next" pointer of the original symbol). */
          new_sym = alloc_symbol((a_symbol_kind)sk_member_function,
                                 instance_sym->header,
                                 &instance_sym->decl_position);
          *new_sym = *instance_sym;
          new_sym->next = NULL;
          new_sym->do_not_reuse = TRUE;
          if (result_sym == NULL) {
            result_sym = new_sym;
          } else {
            if (!symbol_is(result_sym, sk_overloaded_function)) {
              ovld_sym = alloc_symbol((a_symbol_kind)sk_overloaded_function,
                                      new_sym->header,
                                      &locator->source_position);
              ovld_sym->decl_scope = new_sym->decl_scope;
              ovld_sym->decl_seq = new_sym->decl_seq;
              set_class_membership(ovld_sym, (a_source_correspondence*)NULL,
                                   sym_parent_class(result_sym));
              ovld_sym->potentially_overloaded =
                                              new_sym->potentially_overloaded;
              ovld_sym->variant.overloaded_function.symbols = result_sym;
              result_sym = ovld_sym;
            }  /* if */
            new_sym->next = result_sym->variant.overloaded_function.symbols;
            result_sym->variant.overloaded_function.symbols = new_sym;
            result_sym->potentially_overloaded |=
                                              new_sym->potentially_overloaded;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
done:
  return result_sym;
}  /* find_conversion_template_instance */


static a_symbol_ptr create_unknown_conversion_symbol(
					a_symbol_locator	*locator,
					a_type_ptr		class_type)
/*
Create an unknown function symbol that is a member of class_type.  Use the
symbol header information from the locator.  Return the symbol created.
*/
{
  a_symbol_ptr		sym;
  a_type_ptr		conv_result;
  a_constant_ptr        con;

  /* Put the result type of the conversion function into the constant
     created. */
  conv_result = locator->variant.conversion_result_type;
  check_assertion(conv_result != NULL);
  sym = create_unknown_function_symbol(locator->symbol_header,
                                       class_type, (a_namespace_ptr)NULL,
                                       (a_boolean)locator->is_qualified_name,
                                       &con);
  check_assertion(is_unknown_function_constant(con));
  con->variant.template_param.variant.unknown_function.conversion_type =
                                                                   conv_result;
  return sym;
}  /* create_unknown_conversion_symbol */


static a_symbol_ptr look_up_conversion_template_instance(
			a_symbol_locator		*locator,
                        a_type_ptr			class_type)
/*
Find the conversion template instance that matches the type specified
by the conversion type in the locator.  The conversion_template_list
of class_type can be NULL, in which case this routine is still called to
(possibly) do the template dependent context processing.

If either the class_type or the conversion type is template dependent,
return an unknown function symbol that has the result type recorded in
the ck_template_param constant.
*/
{
  a_symbol_ptr			result_sym = NULL;
  a_type_ptr			conv_result =
				      locator->variant.conversion_result_type;
  a_class_symbol_supplement_ptr	cssp;

  cssp = symbol_supplement_for_class(class_type);
  if (is_template_dependent_context() &&
      !type_is_lambda_closure(class_type) &&
      (class_type->variant.class_struct_union.is_nonreal_class ||
       is_template_dependent_type(conv_result))) {
    /* A template context where either the source object type or the
       result type is dependent.  Create an unknown function symbol to
       represent the conversion function. */
    result_sym = create_unknown_conversion_symbol(locator, class_type);
  } else if (cssp->conversion_template_list != NULL) {
    /* A normal (nondependent) context.  Try to find a matching
       template conversion instance. */
    result_sym = find_conversion_template_instance(
                          locator, cssp->conversion_template_list,
                          /*match_fn_qualifiers=*/FALSE, TQ_NONE);
  }  /* if */
  return result_sym;
}  /* look_up_conversion_template_instance */


a_symbol_ptr look_up_conversion_function(a_type_ptr		parent_class,
					 a_type_ptr		conv_type,
					 a_source_position	*source_pos)
/*
Look in parent_class for a conversion function that converts to conv_type.
source_pos is the position to be used in the locator used for the lookup.
*/
{
  a_symbol_locator	locator;
  a_symbol_ptr		new_sym;

  /* Create a symbol locator for the conversion type. */
  make_type_conversion_locator(conv_type, &locator, source_pos);
  /* First look for a nontemplate conversion operator. */
  new_sym = class_qualified_id_lookup(&locator, parent_class,
                                      IDL_DO_NOT_ADD_TO_NONREAL_CLASS);
  if (new_sym == NULL) {
    /* Look for a conversion function template, and possibly create an
       unknown conversion function. */
    new_sym = look_up_conversion_template_instance(&locator, parent_class);
  }  /* if */
  return new_sym;
}  /* look_up_conversion_function */


/* Forward declarations. */
static
a_symbol_ptr scope_stack_lookup(a_symbol_locator    *locator,
                                a_lookup_state_ptr  lookup_state,
				a_scope_depth	    start_depth,
				a_scope_depth       end_depth);

static
a_symbol_ptr instantiation_context_lookup(a_scope_depth      scope_depth,
                                          a_symbol_locator   *locator,
                                          a_lookup_state_ptr lookup_state);


static a_symbol_ptr check_for_microsoft_hidden_template_bug(
				a_symbol_ptr	    orig_sym,
				a_symbol_locator    *locator,
                                a_lookup_state_ptr  lookup_state,
				a_scope_depth	    start_depth,
				a_scope_depth       end_depth)
/*
"orig_sym" is a symbol found in a class reactivation scope.  We are doing
a tentative template lookup.  If "orig_sym" is not a type symbol, continue
looking for a template symbol.  If a template is found, return that symbol.
Otherwise, return the original symbol.

This is done to accept examples like the following, which is accepted by
the Microsoft 7.0 compiler:

  template<class T> struct A { };
  template<class T> struct B {
    class C : public A<T> { };
    typedef C A;
  };
  B<int>::A ba;

WARNING: A scope stack lookup may involve lazy loading of symbols which may
cause scope_stack to be re-allocated, invalidating any saved pointers into the
scope stack.  Any such pointers will need to be reset after a call to this
routine.
*/
{
  a_symbol_ptr	fund_orig_sym;
  a_symbol_ptr	result_sym = orig_sym;

  fund_orig_sym = fundamental_symbol_of(orig_sym);
  if (is_type_symbol(fund_orig_sym) &&
      !is_injected_template_symbol(fund_orig_sym)) {
    /* If the symbol found is a type, look for a class template symbol in an
       enclosing scope.  This is not done for an injected template symbol,
       which could legally be followed by a template argument list. */
    result_sym = scope_stack_lookup(locator, lookup_state, start_depth,
                                    end_depth);
    if (result_sym == NULL ||
        !is_class_template_symbol(result_sym)) result_sym = orig_sym;
  }  /* if */
  return result_sym;
}  /* check_for_microsoft_hidden_template_bug */


static a_boolean clang_pair_swap_hack_criterion(a_symbol_locator  *locator)
/*
Some versions of GCC have a bug for unqualified name lookup in exception
specifications.  Clang emulates that bug specifically for the name "swap"
appearing in the exception specification of a member of std::pair.  Return
TRUE if the current context is std::pair and locator is for "swap".
*/
{
  a_boolean  result = FALSE;

  auto  name_is = [](a_symbol_header_ptr  hdr, a_const_char  *name) {
                    return hdr != NULL && hdr->identifier != NULL &&
                           strcmp(hdr->identifier, name) == 0;
                  };
  if (name_is(locator->symbol_header, "swap")) {
    a_scope_stack_entry  *ssep = &scope_stack_top();
    if (scope_is(ssep, sck_func_prototype) &&
        scope_is(ssep-1, sck_class_reactivation) &&
        scope_is(ssep-2, sck_template_instantiation) &&
        name_is(ssep[-2].template_sym->header, "pair") &&
        symbol_for_namespace_std != NULL &&
        is_member_of_namespace(ssep[-2].template_sym,
                               symbol_for_namespace_std) &&
        seq_is_in_system_header(locator->source_position.seq)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* clang_pair_swap_hack_criterion */


static
a_symbol_ptr scope_stack_lookup(a_symbol_locator    *locator,
                                a_lookup_state_ptr  lookup_state,
				a_scope_depth	    start_depth,
				a_scope_depth       end_depth)
/*
This routine is used by normal_id_lookup to look through a specified
set of scopes and return the result of a normal_id_lookup when only
those scopes are considered.  start_depth is the depth of the innermost
scope to be considered, end_depth is scope at which the lookup should
terminate.  end_depth is not included in the lookup.

locator is the symbol locator for the name being looked up.
lookup_state is used to pass state information between the various routines
that do normal id lookup processing.

WARNING: A scope stack lookup may involve lazy loading of symbols which may
cause scope_stack to be re-allocated, invalidating any saved pointers into the
scope stack.  Any such pointers will need to be reset after a call to this
routine.
*/
{
  a_symbol_ptr			sym = NULL;
  a_scope_depth			curr_depth;
  a_scope_stack_entry_ptr	ssep = NULL;
  a_boolean			curr_scope_skipped = FALSE;
  a_boolean			check_decl_seq_in_exception_spec = FALSE;

  /* Work out from the innermost scope on the stack, and look at each
     scope.  If the scope is a class reactivation or a template
     instantiation, look on the inactive list for a symbol from that
     scope.  Otherwise, search part of the active list to look for an
     active symbol. */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 5 || db_flag_is_set("scope_stack_lookup")) {
    fprintf(f_debug, "Scope stack lookup of %s, initial lookup scope=%d\n",
            locator->symbol_header->identifier, depth_of_initial_lookup_scope);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  if (lookup_state->inclass_exception_spec &&
      (gpp_version_is(<120000) ||
       (clang_mode && clang_pair_swap_hack_criterion(locator)))) {
    /* Early versions of GCC have a bug in their lookup of unqualified names
       appearing in noexcept specifiers.  Clang emulates that bug for a very
       specific case in system headers. */
    check_decl_seq_in_exception_spec = TRUE;
  }  /*if */
  /* Loop through the scope stack until we reach the scope indicated by
     end_depth. */
  for (curr_depth = start_depth; curr_depth > end_depth;
       curr_depth = ssep->previous_scope) {
    a_scope_kind	kind;
    ssep = &scope_stack[curr_depth];
    if (curr_scope_skipped && lookup_state->skip_curr_scope &&
        lookup_state->hidden_name_lookup &&
        ssep->kind == (a_scope_kind)sck_template_instantiation) {
      /* If we skipped the current scope for a hidden name lookup and this
         is a template instantiation scope immediately containing the one
         we skipped, skip it, too. */
      continue;
    }  /* if */
    kind = ssep->kind;
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 5 || db_flag_is_set("scope_stack_lookup")) {
      fprintf(f_debug, "Doing lookup in ");
      db_scope_stack_entry(ssep);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
    /* Record the depth of the scope in which the symbol is being sought. */
    lookup_state->last_scope_used = curr_depth;
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
    /* Skip scopes that are marked to be ignored for normal lookup. */
    if (ssep->ignore_during_normal_lookup) continue;
    /* The "skip_curr_scope" flag is used to skip the initial lookup scope
       when a hidden name lookup is done.  Reset this flag after the first
       iteration of the loop. */
    if (curr_scope_skipped) {
      lookup_state->skip_curr_scope = FALSE;
    } else {
      curr_scope_skipped = TRUE;
    }  /* if */
    /* Clear the flag that indicates whether a projected symbol should be
       sought. */
    lookup_state->look_for_projected_symbol = FALSE;
    /* Lookups outside of a template instantiations scope check the declaration
       sequence number of the symbol found.  This check should be suppressed
       for class scopes.  In g++ mode, only member functions declared before
       a given member function are visible in its exception specification. */
    lookup_state->check_decl_seq = !lookup_state->is_linkage_lookup &&
                       !lookup_state->is_friend_lookup &&
                       !lookup_state->suppress_decl_seq_check &&
                       (check_decl_seq_in_exception_spec ||
                        (kind != (a_scope_kind)sck_class_reactivation &&
                         kind != (a_scope_kind)sck_class_struct_union));
    /* Declaration sequence numbers are not normally checked for class members,
       but need to be checked for alias template instantiations of alias
       templates that are class members. */
    if (!lookup_state->check_decl_seq && is_real_instantiation_context()) {
      a_scope_stack_entry_ptr	instantiation_ssep;
      a_symbol_ptr		instance_sym;
      instantiation_ssep = &scope_stack[depth_innermost_instantiation_scope];
      instance_sym = instantiation_ssep->instance_sym;
      if (instance_sym != NULL && symbol_is(instance_sym, sk_type)) {
        lookup_state->check_decl_seq = TRUE;
      }  /* if */
    }  /* if */
    if (ssep->decl_seq_for_lookup != NO_DECL_SEQUENCE_NUMBER) {
      /* If an explicit declaration sequence number was specified in the scope
         stack entry, use that for lookup in this scope. */
      lookup_state->decl_seq = ssep->decl_seq_for_lookup;
    }  /* if */
    /* Inline namespaces are skipped because names declared there should be
       found via the enclosing namespace in which the members are implicitly
       visible. */
    if (kind == (a_scope_kind)sck_namespace_extension ||
        kind == (a_scope_kind)sck_namespace_reactivation) {
      /* If a namespace extension or reactivation scope is pushed while the
         original namespace is still on the scope stack, the symbols will
         be on the active list and not on the inactive lists.  For
         purposes of name lookup, set the scope kind to sck_namespace if
         the symbols are still on the active list. */
      a_scope_pointers_block_ptr	spbp;
      spbp = assoc_pointers_block_of(ssep);
      if (!spbp->add_symbols_to_inactive_list) {
        kind = (a_scope_kind)sck_namespace;
      }  /* if */
    }  /* if */
    if (kind == (a_scope_kind)sck_class_reactivation ||
        kind == (a_scope_kind)sck_namespace_extension ||
        kind == (a_scope_kind)sck_namespace_reactivation ||
        kind == (a_scope_kind)sck_template_instantiation ||
        (kind == (a_scope_kind)sck_file && ssep->is_reactivation)) {
      if (kind == (a_scope_kind)sck_class_reactivation &&
          (lookup_state->is_linkage_lookup &&
           !lookup_state->treat_as_template_id)) {
        /* Skip class reactivation scopes for linkage lookups, except when
           looking for a template name. */
      } else {
        sym = inactive_scope_lookup(kind, curr_depth, locator, lookup_state);
        /* A scope lookup may involve lazy loading of symbols which can cause
           scope_stack to be re-allocated, invalidating ssep. */
        ssep = scope_stack_entry_for(curr_depth);
        if (sym != NULL && microsoft_bugs && microsoft_version <= 1300 &&
            lookup_state->tentative_template_lookup &&
            !is_class_template_symbol(sym)) {
          /* The Microsoft compiler sometimes finds a hidden template
             symbol. */
          sym = check_for_microsoft_hidden_template_bug(sym, locator,
                                                        lookup_state,
                                                        ssep->previous_scope,
                                                        end_depth);
          /* A scope lookup may involve lazy loading of symbols which can cause
             scope_stack to be re-allocated, invalidating ssep. */
           ssep = scope_stack_entry_for(curr_depth);
        }  /* if */
        if (sym == NULL && kind == (a_scope_kind)sck_template_instantiation) {
          /* For template instantiation scopes, also look on the active list if
             the symbol was not found on the inactive list.  This is done to
             find symbols that are entered into the instantiation scope during
             a prototype instantiation, such as symbols for friend classes
             and friend functions. */
          sym = active_scope_lookup(kind, curr_depth, locator, lookup_state);
          /* A scope lookup may involve lazy loading of symbols which can cause
             scope_stack to be re-allocated, invalidating ssep. */
          ssep = scope_stack_entry_for(curr_depth);
        }  /* if */
      }  /* if */
    } else if (kind == (a_scope_kind)sck_pragma) {
      /* We have found a pragma scope -- ignore symbols in this scope. */
    } else if (kind == (a_scope_kind)sck_class_struct_union &&
               lookup_state->skip_class_scopes) {
      /* This is a class scope and we are skipping class scopes. */
    } else if (ssep->is_reactivation &&
               (is_local_scope_kind(kind) ||
                kind == (a_scope_kind)sck_template_declaration)) {
      /* For a reactivated local scope, the scope's symbol list is used
         by inactive_scope_lookup for this case.  For a reactivated
         template declaration scope, the template parameter list in
         the template_decl_info is used. */
      sym = inactive_scope_lookup(kind, curr_depth, locator, lookup_state);
      /* A scope lookup may involve lazy loading of symbols which can cause
         scope_stack to be re-allocated, invalidating ssep. */
      ssep = scope_stack_entry_for(curr_depth);
    } else {
      /* Not a class reactivation or a template instantiation,
         i.e., normal scope.  Search through any symbols on the front
         of the active list that are from the associated scope, and see
         if any one is the symbol desired. */
      sym = active_scope_lookup(kind, curr_depth, locator, lookup_state);
      /* A scope lookup may involve lazy loading of symbols which can cause
         scope_stack to be re-allocated, invalidating ssep. */
      ssep = scope_stack_entry_for(curr_depth);
      if (sym != NULL && lookup_state->is_linkage_lookup &&
          symbol_is(sym, sk_variable) &&
          sym->variant.variable.ptr->source_corresp.is_local_to_function &&
          is_local_scope_kind(scope_stack_top().kind) &&
          (C_mode() || !strict_ansi_mode)) {
        /* For a linkage lookup in block scope, ignore local variables.  E.g.:
             static int i = 42;
             void g() {
               static int i;
               { extern int i; }  // Finds the file-scope i, not the local i.
             }
           Core issue 426 revised that.  However, it is not commonly
           implemented and therefore we only adhere to it in strict C++
           mode. */
        sym = NULL;
        continue;
      }  /* if */
    }  /* if */
    /* For class and class reactivation scopes, when the symbol is not
       found, see if there is a projection of some symbol into the
       scope. */
    if (lookup_state->look_for_projected_symbol) {
      sym = look_for_projected_symbol(ssep, locator, lookup_state);
      if (lookup_state->terminate_lookup) break;
      /* If we are looking for a conversion function and we still haven't
         found a symbol, look for a conversion template that can match
         the specified type. */
      if (sym == NULL && locator->is_conversion_name) {
        a_type_ptr  class_type = ssep->assoc_type;
        check_assertion(class_type != NULL);
        sym = look_up_conversion_template_instance(locator, class_type);
      }  /* if */
    }  /* if */
    if (sym != NULL) break;
    if (lookup_state->is_linkage_lookup) {
      /* When doing a linkage lookup, stop when we encounter the first
         namespace scope. */
      if (kind == (a_scope_kind)sck_namespace ||
          kind == (a_scope_kind)sck_namespace_extension) break;
    } else if (lookup_state->is_friend_lookup && !locator->is_template_id) {
      /* When doing a friend lookup, stop when we encounter the first
         namespace scope (except for tag names in Microsoft, GNU, and Sun
         modes) or (except in cfront mode) the first function/block scope. */
      if (((kind == (a_scope_kind)sck_namespace ||
            kind == (a_scope_kind)sck_namespace_extension) &&
           !((lookup_state->must_be_tag ||
              lookup_state->treat_as_template_id) &&
             ((gpp_mode && gnu_version < 40000) ||
              sun_mode || ms_extensions))) ||
          (!any_cfront_mode() && (kind == (a_scope_kind)sck_function ||
                                  kind == (a_scope_kind)sck_block))) {
        break;
      }  /* if */
    }  /* if */
    if (cfront_2_1_mode && kind == (a_scope_kind)sck_function) {
      /* In cfront compatibility mode friend functions defined within
         a class ignore the innermost class reactivation scope.
         If this is a friend function, set a flag that will cause
         the innermost class reactivation scope to be ignored.  This
         is a cfront 2.1 problem that appears to have been fixed in
         cfront 3.0. */
      if (!ssep->assoc_routine->source_corresp.is_class_member) {
        /* Not a member function. */
        lookup_state->skip_first_class_reactivation = TRUE;
      }  /* if */
    }  /* if */
    if (kind == (a_scope_kind)sck_template_instantiation &&
        !ssep->nested_instantiation) {
      /* We have reached a template instantiation scope and have not
         yet found the symbol we are looking for.  Do the special
         template lookup that considers symbols from both the
         defining and referencing context. */
      sym = instantiation_context_lookup(curr_depth, locator, lookup_state);
      /* A scope lookup may involve lazy loading of symbols which can cause
         scope_stack to be re-allocated, invalidating ssep. */
      ssep = scope_stack_entry_for(curr_depth);
      break;
    } else if (check_decl_seq_in_exception_spec &&
               (kind == (a_scope_kind)sck_class_reactivation ||
                kind == (a_scope_kind)sck_class_struct_union)) {
      /* GCC limits the visibility of class members when scanning an exception
         specification, which is emulated when check_decl_seq_in_exception_spec
         is TRUE.  However, that limitation appears to apply only to the
         innermost class definition. */
      check_decl_seq_in_exception_spec = FALSE;
    }  /* if */
  }  /* for */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 5 || db_flag_is_set("scope_stack_lookup")) {
    fprintf(f_debug, "Scope stack lookup returning ");
    db_symbol(sym, "", 0);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return sym;
}  /* scope_stack_lookup */


static
a_symbol_ptr merge_instantiation_lookup_symbols(
					a_symbol_ptr		ref_sym,
					a_symbol_ptr		def_sym,
					a_symbol_locator	*locator,
					a_lookup_state_ptr	lookup_state)
/*
Given the symbols that resulted from the two instantiation context lookups,
create a synthesized projection symbol that represents the merged
information from the two symbols.  ref_sym and def_sym are the
symbols from the referencing and defining contexts, respectively.
But if the name is found in the context that is common between them,
it is arbitrarily assigned to one of them.

locator is the symbol locator for the name being looked up.
lookup_state is used to pass state information between the various routines
that do normal id lookup processing.
*/
{
  a_symbol_ptr			sym = NULL;
  an_id_lookup_options_set	options;
  a_boolean			any_errors = FALSE;

  /* Add a bit to the options set that indicates that the synthesized
     namespace projection symbol being created is for a template
     context lookup. */
  options = lookup_state->options | IDL_INSTANTIATION_CONTEXT;
  /* Look for an existing synthesized namespace projection symbol
     from a previous lookup that can be reused. */
  sym = find_synthesized_projection_symbol(locator, options,
                                          /*qualified_lookup=*/FALSE,
                                          (a_namespace_ptr)NULL);
  /* Add the first symbol to an existing lookup set.  Note that
     sym may be NULL at this point. */
  sym = add_symbol_to_lookup_set(sym, ref_sym, locator,
                                 /*qualified_lookup=*/FALSE,
                                 (a_namespace_ptr)NULL, options,
                                 &any_errors);
  /* Add the second symbol to the set.  This is done even if an error was
     returned from the previous lookup. */
  sym = add_symbol_to_lookup_set(sym, def_sym, locator,
                                 /*qualified_lookup=*/FALSE,
                                 (a_namespace_ptr)NULL, options,
                                 &any_errors);
  return sym;
}  /* merge_instantiation_lookup_symbols */


static
a_symbol_ptr instantiation_context_lookup(a_scope_depth      scope_depth,
                                          a_symbol_locator   *locator,
                                          a_lookup_state_ptr lookup_state)
/*
When a normal lookup reaches an instantiation scope and, after considering the
template parameters, still has not found a symbol, the context of the
instantiation must be considered.  The context has two components: the
definition context (where the template was declared or defined) and the
referencing context (the namespace containing the reference that caused the
instantiation).

The symbol may be found in one or both of the contexts.  If the symbol is
found in both contexts, both instances must be functions.

The two lookups are done by calling scope_stack_lookup with the
appropriate set of starting and ending scopes.  The ending scopes
specified will result in all scopes having been considered up to
a "common" scope at which the two lookup paths come back together.
The two lookups could result in zero, one, or two symbols being found.
If no symbols are found, the search resumes from the common scope.
If one symbol is found, the search also begins with the common
scope but any symbol found from the common search is used as the
"second" symbol (i.e., the one for which no symbol was found
in the defining/referencing searches).  If two symbols are found,
either from the defining/referencing search or as a result of
finding one in the defining/referencing search and one in the
common search, the two symbols are merged and result in either
an overload set or an ambiguous symbol.

scope_depth is the index into scope_stack for the template instantiation
scope.  locator is the symbol locator for the name being looked up.
lookup_state is used to pass state information between the various routines
that do normal id lookup processing.

WARNING: A scope stack lookup may involve lazy loading of symbols which may
cause scope_stack to be re-allocated, invalidating any saved pointers into the
scope stack.  Any such pointers will need to be reset after a call to this
routine.
*/
{
  a_scope_stack_entry_ptr ssep = scope_stack_entry_for(scope_depth);
  a_scope_depth           common_depth = ssep->instantiation_common_depth;
  a_scope_depth           def_start = ssep->previous_scope;
  a_scope_depth           ref_start = ssep->instantiation_context_depth;
  a_symbol_ptr            def_sym;
  a_symbol_ptr            ref_sym = NULL;
  a_symbol_ptr            sym = NULL;
  a_boolean               do_not_look_in_common_scopes = FALSE;

#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 5 || db_flag_is_set("instantiation_lookup")) {
    fprintf(f_debug,
            "doing instantiation lookup of %s: def_start=%d, ref_start=%d, ",
            locator->symbol_header->identifier,
            def_start, ref_start);
    fprintf(f_debug, "common=%d\n", common_depth);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  if (gpp_using_directive_lookup) {
    /* f_get_effective_decl_seq is used to force the declaration sequence
       to be fetched even when not doing dependent name processing. */
    lookup_state->using_dir_decl_seq = f_get_effective_decl_seq();
  }  /* if */
  if (do_dependent_name_processing) {
    /* Only consider names visible at the point at which the template was
       defined. */
    lookup_state->decl_seq = get_effective_decl_seq();
  } else if (ssep->force_decl_seq_check) {
    /* f_get_effective_decl_seq is used to force the declaration sequence
       to be fetched even when not doing dependent name processing. */
    lookup_state->decl_seq = f_get_effective_decl_seq();
  }  /* if */
  def_sym = scope_stack_lookup(locator, lookup_state, def_start, common_depth);
  /* A scope lookup may involve lazy loading of symbols which can cause
     scope_stack to be re-allocated, invalidating ssep. */
  ssep = scope_stack_entry_for(scope_depth);
  if (def_sym != NULL && def_sym->is_class_member) {
    /* Do not look for a name in the referencing context if the definition
       context search found a class member. */
    do_not_look_in_common_scopes = TRUE;
  } else {
    if (do_dependent_name_processing || gpp_dependent_name_lookup ||
        !nonstandard_instantiation_lookup_enabled) {
      /* The referencing context should be not considered in dependent lookup
         mode, g++, and when nonstandard instantiation lookup is not
         enabled.  This means the common lookup must be suppressed if
         we've already found a symbol. */
      if (def_sym != NULL) do_not_look_in_common_scopes = TRUE;
    } else {
      /* Only do the referencing context lookup when not doing the
         standard-conforming dependent name processing.  When doing
         dependent name processing only names visible to argument-dependent
         lookup are visible from the instantiation context. */
      if (ref_start > common_depth) {
        /* Only look in the referencing namespace when it is different from
           the common depth. */
        ref_sym = scope_stack_lookup(locator, lookup_state, ref_start,
                                     common_depth);
        /* A scope lookup may involve lazy loading of symbols which can cause
           scope_stack to be re-allocated, invalidating ssep. */
        ssep = scope_stack_entry_for(scope_depth);
      }  /* if */
    }  /* if */
  }  /* if */
  /* If either of these lookups failed to find a symbol, continue the
     lookup starting from the common scope.  When doing a friend lookup
     for a name that is not a template-id, only do the common scope lookup
     if the innermost namespace scope is part of the common lookup.  Don't
     do this lookup if the terminate_lookup flag is set (which means we
     found a symbol earlier, but it didn't satisfy the lookup options). */
  if ((ref_sym == NULL || def_sym == NULL) &&
      !lookup_state->terminate_lookup &&
      (!lookup_state->is_friend_lookup ||
       (lookup_state->must_be_tag &&
        ((gpp_mode && gnu_version < 40000) || sun_mode ||
          (microsoft_mode && ms_permissive))) ||
       depth_innermost_namespace_scope <= common_depth ||
       locator->is_template_id) &&
       !do_not_look_in_common_scopes) {
    /* One of the lookups did not find a symbol.  Do the lookup of
       the common scopes. */
    a_symbol_ptr	common_sym;
    common_sym = scope_stack_lookup(locator, lookup_state, common_depth,
                                    NO_SCOPE_DEPTH);
    /* A scope lookup may involve lazy loading of symbols which can cause
       scope_stack to be re-allocated, invalidating ssep. */
    ssep = scope_stack_entry_for(scope_depth);
    /* Assign the common symbol to whichever of the previous lookups that
       did not produce a symbol.  If both were NULL, arbitrarily use the
       common symbol as the defining context symbol. */
    if (def_sym == NULL) {
      def_sym = common_sym;
    } else {
      ref_sym = common_sym;
    }  /* if */
  }  /* if */
  if (ref_sym != NULL) {
    a_symbol_ptr	fund_ref_sym;
    fund_ref_sym = fundamental_symbol_of(ref_sym);
    if (!is_function_or_template_symbol(fund_ref_sym)) {
      /* Only functions from the referencing context are used.  All other
         names can only come from the definition context.  The WP
         requires only "dependent" functions from the referencing context
         be considered.  The "dependent" lookup portion has not been
         implemented yet. */
      ref_sym = NULL;
    } else if (def_sym != NULL) {
      a_symbol_ptr	fund_def_sym;
      fund_def_sym = fundamental_symbol_of(def_sym);
      if (!is_function_or_template_symbol(fund_def_sym)) {
        /* The name found in the definition context is not a function.
           Because only functions can come from the referencing context,
           we know we'll get an ambiguity if we try to merge a function
           and a nonfunction, so we ignore the name from the referencing
           context. */
        ref_sym = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  if (ref_sym != NULL && def_sym != NULL && ref_sym != def_sym) {
    /* Both symbols are present.  Merge the results. */
    sym = merge_instantiation_lookup_symbols(ref_sym, def_sym, locator,
                                             lookup_state);
  } else if (ref_sym != NULL) {
    /* Only ref_sym is non-NULL (or both symbols are the same). Return that
       value. */
    sym = ref_sym;
  } else if (def_sym != NULL) {
    /* Only def_sym is non-NULL. Return that value. */
    sym = def_sym;
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 5 || db_flag_is_set("instantiation_lookup")) {
    fprintf(f_debug, "instantiation lookup: ");
    if (sym == NULL) {
      fprintf(f_debug, "<NULL>\n");
    } else {
      db_symbol(sym, "", 4);
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  return sym;
}  /* instantiation_context_lookup */


a_symbol_ptr f_nonreal_type_if_nested_prototype_type(a_symbol_ptr	sym)
/*
If a symbol has a corresponding nonreal type, return the symbol for that type,
otherwise return the original symbol.  Only return the nonreal symbol if the
type associated with the original symbol is not on the scope stack.  "sym"
is already known to be a symbol with an associated nonreal type.
*/
{

  a_symbol_ptr	result_sym = sym->corresp_nonreal_or_nested_type;

  if (is_class_struct_union_symbol(sym) &&
      scope_of_class_is_active(sym->variant.class_struct_union.type)) {
    /* If the type is found on the scope stack, return the original symbol. */
    result_sym = sym;
  }  /* if */
  return result_sym;
}  /* f_nonreal_type_if_nested_prototype_type */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_symbol_ptr look_up_property_or_event_accessor(
					a_symbol_locator         *locator,
					a_type_ptr               class_type)
/*
The locator represents a Microsoft property or event accessor function.
These are named in an unusual way.  For example, for a property P in class
C, the accessor is named as C::P::get even though P is not itself a class
type.  If the locator names a valid accessor in class_type, return the symbol
for the accessor; otherwise return NULL.
*/
{
  a_symbol_ptr	result_sym = NULL;

  if (locator->specific_symbol != NULL) {
    /* Retain the specific symbol if there is one. */
    result_sym = locator->specific_symbol;
  } else if (!locator->is_class_member ||
             (!same_entities(locator->parent.class_type, class_type) &&
              find_base_class_of(class_type,
                                 locator->parent.class_type) == NULL)) {
    /* The locator is not for a class member, or does not match the class
       type provided or one of its base classes.  Return NULL (set above). */
  } else if (symbol_is(locator->property_or_event_parent, sk_property_set)) {
    /* Return the symbol for the get or set accessor maintained in the
       property set supplement. */
    if (strcmp(locator->symbol_header->identifier, "get") == 0) {
      result_sym = locator->property_or_event_parent->variant.property_info
                                                    ->get_accessors;
    } else if (strcmp(locator->symbol_header->identifier, "set") == 0) {
      result_sym = locator->property_or_event_parent->variant.property_info
                                                    ->set_accessors;
    } else {
      /* Not a valid accessor name: Return NULL (set above). */
    }  /* if */
  } else {
    a_property_or_event_descr_ptr	pdp;
    if (symbol_is(locator->property_or_event_parent, sk_field)) {
      pdp = locator->property_or_event_parent
                   ->variant.field.ptr
                   ->property_or_event_descr;
    } else {
      pdp = locator->property_or_event_parent
                   ->variant.static_data_member.variable
                   ->property_or_event_descr;
    }  /* if */
    check_assertion(pdp->kind == (a_property_or_event_kind)pek_cli_event);
    /* See if the symbol header we are looking for matches the add,
       remove, or raise routine. */
    if (strcmp(locator->symbol_header->identifier, "add") == 0) {
      result_sym = symbol_for_or_null(pdp->add_routine);
    } else if (strcmp(locator->symbol_header->identifier, "remove") == 0) {
      result_sym = symbol_for_or_null(pdp->remove_routine);
    } else if (strcmp(locator->symbol_header->identifier, "raise") == 0) {
      result_sym = symbol_for_or_null(pdp->raise_routine);
    }  /* if */
  }  /* if */
  return result_sym;
}  /* look_up_property_or_event_accessor */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_symbol_ptr normal_id_lookup(a_symbol_locator         *locator,
                              an_id_lookup_options_set options)
/*
Look up the identifier indicated by *locator and return a pointer to
the symbol found, or NULL if the symbol is not found.  options contains
bits indicating special restrictions, i.e., the symbol must be a class
name or tag.  The symbol is looked up in the normal name space (variables,
functions, classes, types, etc.).  If the symbol found is a projection
symbol, the projection symbol pointer is recorded in the locator and the
fundamental symbol pointer is returned.  This routine is used in both
C and C++.

WARNING: A lookup may involve lazy loading of symbols or template
instantiations which may cause scope_stack to be re-allocated, invalidating any
saved pointers into the scope stack.  Any such pointers will need to be reset
after a call to this routine.
*/
{
  a_symbol_ptr            sym, inactive_symbol_list;
  a_symbol_ptr            active_symbol_list;
  a_scope_stack_entry_ptr ssep;
  a_boolean		  use_slow_lookup = FALSE;

  db_enter(4, "normal_id_lookup");

#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("normal_id_lookup")) {
    fprintf(f_debug, "Normal lookup of %s\n",
            locator->symbol_header->identifier);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  sym = locator->specific_symbol;
  if (sym != NULL) {
    /* The locator is for a specific symbol, so return the symbol for it. */
  } else if (is_error_locator(*locator)) {
    /* The locator is an error locator, so return NULL (i.e., no symbol
       found). */
    sym = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (locator->is_property_or_event_accessor) {
    /* If the locator refers to a property or event, do a special lookup
       for accessor functions. */
    sym = look_up_property_or_event_accessor(locator,
                                             locator->parent.class_type);
    locator->specific_symbol = sym;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    /* Initialize the lookup state information used to pass information
       about this lookup between the various routines used to do the
       lookup. */
    a_lookup_state	lookup_state;
    clear_lookup_state(lookup_state);
    lookup_state.must_be_class_or_namespace =
                               (options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0;
    lookup_state.must_be_tag = (options & IDL_MUST_BE_TAG) != 0;
    lookup_state.must_be_namespace = (options & IDL_MUST_BE_NAMESPACE) != 0;
    lookup_state.must_be_class = (options & IDL_MUST_BE_CLASS) != 0;
    lookup_state.tentative_type_lookup =
                                    (options & IDL_TENTATIVE_TYPE_LOOKUP) != 0;
    lookup_state.tentative_template_lookup =
                                (options & IDL_TENTATIVE_TEMPLATE_LOOKUP) != 0;
    lookup_state.is_linkage_lookup = (options & IDL_LINKAGE_LOOKUP) != 0;
    lookup_state.treat_as_template_id =
                                     (options & IDL_TREAT_AS_TEMPLATE_ID) != 0;
    lookup_state.is_friend_lookup = (options & IDL_FRIEND_LOOKUP) != 0;
    lookup_state.suppress_decl_seq_check =
                                  (options & IDL_SUPPRESS_DECL_SEQ_CHECK) != 0;
    lookup_state.hidden_name_lookup = (options & IDL_HIDDEN_NAME_LOOKUP) != 0;
    lookup_state.force_lookup_in_dependent_bases =
                                               lookup_state.hidden_name_lookup;
    lookup_state.do_not_create_proj_sym =
                                   (options & IDL_DO_NOT_CREATE_PROJ_SYM) != 0;
    lookup_state.skip_curr_scope = (options & IDL_SKIP_CURR_SCOPE) != 0;
    lookup_state.skip_class_scopes = (options & IDL_SKIP_CLASS_SCOPES) != 0;
    if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&
        scope_stack_top().exception_specification &&
        scope_stack_top().decl_parse_state != NULL &&
        scope_stack_top().decl_parse_state->is_inclass_member_function_decl) {
      lookup_state.inclass_exception_spec = TRUE;
    }  /* if */
    /* If any instantiation scopes are active we will need to check for
       the presence of nonreal base classes.  This is done to "pretend"
       that certain names were found in a nonreal base class.  Don't
       do this special processing when doing standard dependent name
       lookup. */
    lookup_state.check_for_nonreal_bases =
                         !do_dependent_name_processing &&
                         !lookup_state.is_friend_lookup &&
			 !lookup_state.is_linkage_lookup &&
                         is_template_dependent_context();
    if (C_mode() && lookup_state.must_be_tag) {
      lookup_state.required_name_space_kind = nsk_tag;
    }  /* if */
    if (lookup_state.inclass_exception_spec &&
        (gpp_version_is(<100000) ||
         (clang_mode && clang_pair_swap_hack_criterion(locator)))) {
      /* exception_spec_decl_seq is used in some g++ modes to limit visibility
         of names used in exception specification to those previously declared
         in a class.  Clang appears to emulate that behavior in system headers
         for the identifier "swap" appearing in the std::pair implementation
         (presumably to be able to consume certain GNU standard library
         headers). */
      lookup_state.decl_seq = scope_stack_top().exception_spec_decl_seq;
    }  /* if */
    lookup_state.options = options;
    /* We must search for the symbol. */
    /* We have two search algorithms: the first is the C algorithm, which
       is fast; it just searches the active list.  The second is the C++
       algorithm, which is slower; it considers each scope on the scope
       stack in turn, and looks for a symbol in that scope. */
    active_symbol_list = symbol_list_from_locator(*locator);
    inactive_symbol_list = inactive_symbol_list_from_locator(*locator);
    /* The slow lookup mechanism is used:

	- in C mode if a pragma scope is active
	- if a template instantiation scope is active
	- if symbols from the inactive list may be visible, which is true
	  if a template instantiation, class reactivation, namespace
	  reactivation, namespace extension, or class scope for a class
	  with base classes is active.  It is also true for scopes containing
          using-directives and for the reactivation of the file scope.
    */
    ssep = &scope_stack[depth_scope_stack];
    if (lazy_symbols_may_be_visible || ssep->slow_lookup_required) {
      /* Certain scopes (e.g., pragma and template instantiation) require
         slow lookups. */    
      use_slow_lookup = TRUE;
    } else if (!C_mode() &&
               (lookup_state.skip_curr_scope ||
                lookup_state.skip_class_scopes ||
                lookup_state.is_linkage_lookup ||
                lookup_state.is_friend_lookup)) {
      /* Certain lookup kinds require a slow lookup in C++ mode. */
      use_slow_lookup = TRUE;
    } else if ((ssep->inactive_symbols_may_be_visible &&
                inactive_symbol_list != NULL) ||
                locator->is_conversion_name) {
      /* A slow lookup is required if we need to search the inactive list
         or if we need to look up a conversion name. */
      use_slow_lookup = TRUE;
    }  /* if */
    if (!use_slow_lookup) {
      /* Fast algorithm: just search the active symbol list. */
       a_symbol_ptr		type_tag_symbol = NULL;
#if DEBUG
/* GCOVR_EXCL_START */
      num_fast_id_lookups++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      /*lint --e{446,850} sym modified in loop (LINTBUG) */
      for (sym = active_symbol_list; sym != NULL; sym = sym->next) {
        /* See if the symbol is acceptable (e.g., it's a class if it
           must be one). */
        a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
        /* Exit the loop if we found a type tag symbol and the new symbol
           to check is not from the same scope. */
        if (type_tag_symbol != NULL &&
            type_tag_symbol->decl_scope != sym->decl_scope) {
          sym = NULL;
          break;
        }  /* if */
        if (is_acceptable_symbol(sym, fund_sym, &lookup_state, ssep,
                                 /*invisible_okay=*/FALSE)) {
          /* We found a matching symbol.  If this is a type symbol found
             by a must-be-tag lookup, keep searching for a "real" tag in
             the same scope. */
          if (lookup_state.must_be_tag &&
              fund_sym->kind == (a_symbol_kind)sk_type) {
            type_tag_symbol = sym;
          } else {
            /* Use this symbol. */
            break;
          }  /* if */
        }  /* if */
      }  /* for */
      /* If a type symbol was found and no other matching tag was present,
         use the type symbol. */
      if (sym == NULL && type_tag_symbol != NULL) sym = type_tag_symbol;
    } else {
      /* There are inactive symbols and they may be visible, so the more
         complicated search is required. */
#if DEBUG
/* GCOVR_EXCL_START */
      num_slow_id_lookups++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      /* In C mode, slow lookups should only occur when the file scope has
         been reactivated. */
      check_assertion(!C_mode() ||
                      scope_stack[DEPTH_OF_FILE_SCOPE].is_reactivation);
      sym = scope_stack_lookup(locator, &lookup_state,
                               depth_of_initial_lookup_scope, NO_SCOPE_DEPTH);
      /* A scope stack lookup may involve lazy loading of symbols which may
         cause scope_stack to be re-allocated, invalidating ssep. */
      ssep = scope_stack_entry_for(depth_scope_stack);
    }  /* if */
    /* In some cases, a second lookup is done in g++ mode if any dependent
       base classes were ignored.  This is done if no symbol was found,
       if a symbol from a using-directive lookup was found, or if we are
       in an expression context.   It is also always done for g++ versions
       before 3.4. */
    if (gpp_dependent_name_lookup &&
        (gnu_version < 30400 ||
         ((sym == NULL || sym->synthesized_namespace_projection) ||
          (options & IDL_IS_EXPR_CONTEXT) != 0)) &&
        lookup_state.any_ignored_dependent_bases) {
      /* In g++ mode, names from dependent base classes are sometimes
         ignored and sometimes not.  A second lookup is done to determine if
         a different result would have been found if dependent bases were
         included.  If the first lookup found no symbol or the symbol
         found was a function symbol, use the symbol from the second lookup
         (which may be the same as the first symbol if there were no names
         found in dependent base classes).  If the first lookup did not
         find a function and the second lookup did find a function, use the
         symbol from the second lookup.  In addition, if both lookups find
         fields, use the result of the second lookup.  This occurs when
         the first lookup finds a field from an enclosing class and the second
         finds one from a base class (gnu_version < 30400).  When gnu_version
         >= 40100 the symbol from the first lookup is always used if a
         symbol was found. */
      a_symbol_ptr	fund_sym;
      fund_sym = sym == NULL ? NULL : fundamental_symbol_of(sym);
      /* For versions 3.4 and newer the second lookup should not be done if
         the first lookup found a type, variable, or constant. */
      if (gnu_version < 30400 || fund_sym == NULL ||
          (!symbol_is(fund_sym, sk_variable) &&
           !symbol_is(fund_sym, sk_type) &&
           !symbol_is(fund_sym, sk_constant) &&
           !is_class_struct_union_symbol(fund_sym))) {
        a_symbol_ptr	new_sym;
        a_symbol_ptr	fund_new_sym;
        lookup_state.force_lookup_in_dependent_bases = TRUE;
        new_sym = scope_stack_lookup(locator, &lookup_state,
                                     depth_of_initial_lookup_scope,
                                     NO_SCOPE_DEPTH);
        /* A scope stack lookup may involve lazy loading of symbols which may
           cause scope_stack to be re-allocated, invalidating ssep. */
        ssep = scope_stack_entry_for(depth_scope_stack);
        lookup_state.force_lookup_in_dependent_bases = FALSE;
        fund_new_sym = new_sym == NULL ? NULL : fundamental_symbol_of(new_sym);
        if (fund_new_sym != NULL && fund_new_sym->is_class_member) {
          a_type_ptr	parent_class = fund_new_sym->parent.class_type;
          if (parent_class->variant.class_struct_union.is_nonreal_class) {
            /* If the second lookup found a member of a nonreal class, ignore
               it.  The special g++ processing should only be done when the
               lookup finds a member of a real base class. */
            new_sym = NULL;
            fund_new_sym = NULL;
          }  /* if */
        }  /* if */
        if (sym != NULL && new_sym != NULL &&
            sym->synthesized_namespace_projection) {
          /* If the original symbol is a synthesized namespace projection
             symbol (i.e., from a using-directive lookup) prefer the new
             symbol. */
          sym = new_sym;
        } else if (sym == NULL ||
                   (gnu_version < 40100 && is_function_symbol(fund_sym))) {
          sym = new_sym;
        } else if (gnu_version < 40600 && fund_sym != NULL &&
                   fund_sym == fund_new_sym) {
          /* We found two different projection symbols that refer to the same
             fundamental symbol.  This can happen if a given class is a
             nondependent base of an enclosing class and a dependent base of
             a nested class. Use the new symbol. */
          sym = new_sym;
        } else if (sym != NULL && gnu_version >= 40100) {
          /* Use the existing sym. */
        } else if (new_sym != NULL) {
          if (is_function_symbol(fund_new_sym) &&
              fund_sym != NULL && !is_template_symbol(fund_sym)) {
            sym = new_sym;
          } else if (gnu_version < 30400 &&
                     fund_new_sym->kind == (a_symbol_kind)sk_field &&
                     sym->kind == (a_symbol_kind)sk_field) {
            sym = new_sym;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    /* If this is a linkage lookup, don't do the nested class anachronism
       lookup, or SVR4 mode lookup. */
    if (!lookup_state.is_linkage_lookup && !lookup_state.terminate_lookup) {
      if (!C_mode()) {
        if (sym == NULL) {
          /* See if the nested class anachronism (ARM 18.3.5) yields a symbol.
             Note that if there is an ambiguity, NULL is returned.  Note
             also that we look for a semivisible nested class only if no
             other symbol is found.  This means a nested class that is
             semivisible at function scope will not hide a name at file
             scope; this is different from how cfront 2.1 works, but it
             means that programs that are legal by the ARM do not fail to
             compile or otherwise behave differently because the anachronism
             was invoked. */
          if (allow_anachronisms) sym = find_nested_type_symbol(locator);
          if (sym != NULL) {
            a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
            if (is_acceptable_symbol(sym, fund_sym, &lookup_state, ssep,
                                     /*invisible_okay=*/FALSE)) {
              locator->is_semivisible_nested_type = TRUE;
            } else {
              sym = NULL;
            }  /* if */
          }  /* if */
        }  /* if */
        if (sym == NULL && lookup_state.any_nonreal_bases &&
            !lookup_state.must_be_tag) {
          /* If no symbol was found and one of the classes searched has
             a nonreal base class then consider the symbol to be a member
             of the class with the nonreal base class.  This will occur when
             a base class depends on a template parameter (such as A<T>)
             or when the base class is a template parameter (such as T).
             In these cases it is impossible to know, at the time that
             prototype instantiation is done, which names will be in the
             classes used in the real instantiations.  Any name is accepted
             as a member of the class.  When class_qualified_id_lookup is
             used to lookup a name in a class with nonreal base classes,
             it will add a projection symbol to one of the nonreal bases
             if the name is not found.  In other words, this call is used
             to create the nonreal member.  any_nonreal_bases will only
             be set if check_for_nonreal_bases was set earlier.  This is not
             done for must_be_tag lookups so that an elaborated type specifier
             will declare a previously undeclared name. */
          sym = class_qualified_id_lookup(
                                 locator, lookup_state.class_with_nonreal_base,
                                 options | IDL_DO_NOT_CREATE_PROJ_SYM |
					   IDL_MEMBER_OF_UNKNOWN_BASE);
        }  /* if */
      }  /* if */
      if (sym == NULL && SVR4_C_mode &&
          (options & IDL_TENTATIVE_TYPE_LOOKUP) == 0 &&
          (options & IDL_IS_LOOKUP_TO_CHECK_FOR_NAME_HIDING) == 0) {
        /* This is an SVR4 compatibility feature.  A symbol declared as
           a block extern in a block that is no longer in scope may be
           referenced later.  Look for an external variable or routine that
           matches the name being looked up.  This is not done during
           tentative type lookups.  The "is_acceptable_symbol" test is done
           by find_out_of_scope_declaration. */
        sym = find_out_of_scope_declaration(locator, options);
      }  /* if */
    }  /* if */
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
    if (cfront_2_1_mode &&
        (sym != NULL || lookup_state.projection_symbol_found) &&
        lookup_state.last_scope_used != NO_SCOPE_DEPTH) {
      a_scope_stack_entry_ptr	proj_ssep;
      /* Special case to emulate a cfront 2.1 bug.  See the comments
         in check_for_cfront_name_lookup_bug for more information.  The
         special case code is only executed when the symbol found is from
	 a class reactivation scope for the same class of a constructor
	 or destructor that was just defined -- so this should not have
	 a significant performance impact.

         We have either found a symbol (sym != NULL) or a projection symbol
         was found that was not a type symbol in a tentative type lookup
         (projection_symbol_found == TRUE).  Call the special routine to
         see if a file scope name exists that satisfies the required
         criteria. */
      proj_ssep = &scope_stack[lookup_state.last_scope_used];
      if (proj_ssep->kind == (a_scope_kind)sck_class_reactivation) {
        a_type_ptr      class_type = proj_ssep->assoc_type;
	if (last_ctor_or_dtor_sym != NULL &&
            sym_parent_class(last_ctor_or_dtor_sym) == class_type) {
	  sym = check_for_cfront_name_lookup_bug(class_type, sym, locator,
						 options);
          /* This looks like we can find an alternate symbol when emulating
	     the cfront bug and then discard it because it is not the
	     correct kind of symbol.  In practice this will never happen
	     because the only symbols that can be rejected are types
	     (either classes or tags) and because of the transitional model
	     of nested type handling, will always be defined before a nested
	     class of the same name can be used. */
          if (sym != NULL) {
            a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
            if (!is_acceptable_symbol(sym, fund_sym, &lookup_state, ssep,
                                      /*invisible_okay=*/FALSE)) {
              sym = NULL;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Perform dual-lookups on unqualified identifiers in C++/CLI mode if
       the identifier was not found.  This allows members of the 'cli'
       namespace to be found without qualification or a using declaration or
       directive (e.g. array => cli::array). */
    if (cli_or_cx_enabled && sym == NULL &&
        !in_code_generated_from_metadata() &&
        !lookup_state.projection_symbol_found &&
        (options & (IDL_IF_EXISTS_LOOKUP | IDL_LINKAGE_LOOKUP)) == 0) {
      a_symbol_header_ptr sym_hdr = locator_for_curr_id.symbol_header;
      if (sym_hdr != NULL) {
        const a_cli_symbol_kind *p_csk_fallback;
        p_csk_fallback = cppcx_enabled ? cppcx_fallback_symbols
                                       : cli_fallback_symbols;
        for (; *p_csk_fallback != csk_none; ++p_csk_fallback) {
          a_symbol_ptr fallback_sym = cli_symbol_from_kind(*p_csk_fallback);
          check_assertion(fallback_sym != NULL);
          if (fallback_sym != NULL && sym_hdr == fallback_sym->header) {
            sym = fallback_sym;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    locator->specific_symbol = sym;
    locator->is_template_param = lookup_state.found_template_param;
  }  /* if */
  if (sym != NULL) {
    /* If the symbol is a projection symbol, reduce it to the fundamental
       symbol.  The specific_symbol in the locator stays pointing to the
       projection symbol. */
    reduce_projection_symbol_to_fundamental_symbol(sym);
    /* If the symbol is a nested type of a prototype instantiation, use
       the associated nonreal type. */
    sym = nonreal_type_if_nested_prototype_type(sym);
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    if (sym != NULL) {
      fprintf(f_debug, "normal_id_lookup: found %s\n",
                       sym->header->identifier);
      if (debug_level >= 5) db_symbol(sym, "", 2);
    } else {
      fprintf(f_debug, "normal_id_lookup: not found\n");
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  db_exit();
  return sym;
}  /* normal_id_lookup */


a_symbol_ptr curr_tag_symbol(a_symbol_locator  *locator,
                             a_symbol_kind     tag_kind,
                             a_boolean         allow_typedef,
                             a_boolean         is_friend_decl)
/*
The current token is an identifier.  If it is a tag of the indicated kind
do ambiguity and access control checking and return a pointer to the tag
symbol.  Otherwise, return NULL.  allow_typedef is TRUE if the identifier
may refer to a typedef.  is_friend_decl is TRUE when the tag appears
in a friend declaration.
*/
{
  a_symbol_ptr              assoc_symbol;
  an_id_lookup_options_set  options = IDL_MUST_BE_TAG;
  a_boolean                 class_template_ignored = FALSE;

  /* Look up the current token.  Note that a qualified name is not allowed. */
  if (is_friend_decl) options |= IDL_FRIEND_LOOKUP;
  assoc_symbol = normal_id_lookup(locator, options);
  if (assoc_symbol != NULL) {
    if (assoc_symbol->kind == (a_symbol_kind)sk_class_template &&
        is_nonspecialized_instantiation_context()) {
      /* If the symbol found is a class template symbol and we are inside an
         instantiation of the class, use the template class symbol associated
         with the current instantiation. */
      (void)current_class_symbol_if_class_template(&assoc_symbol);
    } else if (is_friend_decl &&
               !friend_class_decl_can_find_using_dir &&
               locator->specific_symbol != NULL &&
               locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_namespace_projection) {
      /* Friend lookups should not find using declarations. */
      assoc_symbol = NULL;
      clear_specific_symbol(*locator);
    }  /* if */
  }  /* if */
  /* If the symbol still refers to the class template, ignore it. */
  if (assoc_symbol != NULL &&
      assoc_symbol->kind == (a_symbol_kind)sk_class_template) {
    assoc_symbol = NULL;
    clear_specific_symbol(*locator);
    class_template_ignored = TRUE;
  }  /* if */
  if (assoc_symbol == NULL && (!is_friend_decl || class_template_ignored)) {
    /* If the symbol was not found using a normal lookup above, look again
       using a linkage lookup which will return an invisible symbol. */
    assoc_symbol = normal_id_lookup(locator, options | IDL_LINKAGE_LOOKUP);
    /* If the new symbol refers to the class template, ignore it. */
    if (assoc_symbol != NULL &&
        assoc_symbol->kind == (a_symbol_kind)sk_class_template) {
      assoc_symbol = NULL;
      clear_specific_symbol(*locator);
    }  /* if */
  }  /* if */
  if (assoc_symbol != NULL) {
    a_boolean	is_injected_class_name;
    /* Make sure that the lookup was not ambiguous. */
    check_for_ambiguity(locator);
    is_injected_class_name = is_injected_class_symbol(assoc_symbol);
    if (assoc_symbol->is_template_param) {
      a_type_ptr   	tp;
      a_symbol_ptr	new_sym;
      an_error_severity	severity;
      /* We are within a template instantiation, so the name may map to a
         template parameter.  For example,
            class A { };
            template <class T> class B { class T x; };
            B<A> b;
         The symbol is for a type template parameter (nontypes will not be
         found by an IDL_MUST_BE_TAG lookup).  During prototype instantiation
         the template parameter symbol is simply returned to the caller.
         During a real instantiation the symbol associated with the type
         pointed to by the template parameter is returned. */
      check_assertion_str(assoc_symbol->kind == (a_symbol_kind)sk_type,
                          "curr_tag_symbol: bad symbol kind");
      tp = assoc_symbol->variant.type.ptr;
      /* If the type is a typedef, use the underlying type. */
      tp = skip_typedefs_not_dependent_decltypes(tp);
      new_sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
      /* Issue a diagnostic because this usage is no longer permitted by
         the Working Paper. */
      severity = strict_ansi_error_severity;
      pos_st_diagnostic(severity,
                        ec_template_param_in_elab_type, 
                        &error_position, assoc_symbol->header->identifier);
      if (severity == es_error) {
        /* If an error was issued above.  Return an error locator. */
        set_to_error_locator(*locator);
        assoc_symbol = NULL;
      } else if (tp->kind == (a_type_kind)tk_template_param) {
        /* A template parameter encountered during prototype instantiation.
           Simply return the original symbol. */
      } else if (new_sym != NULL && new_sym->kind == tag_kind) {
        /* Use the template argument to which the template parameter points. */
        assoc_symbol = new_sym;
      } else {
        /* The template argument is the wrong kind of tag. */
        pos_stty_error(ec_tag_kind_incompatible_with_template_parameter,
                       &error_position,
                       name_of_symbol_kind(tag_kind), tp);
        set_to_error_locator(*locator);
        assoc_symbol = NULL;
      }  /* if */
    } else if (assoc_symbol->kind == (a_symbol_kind)sk_type &&
               !is_injected_class_name) {
      a_boolean	typedef_okay = FALSE;
      if (gpp_mode && gnu_version < 30400) {
        a_type_ptr	underlying_type = assoc_symbol->variant.type.ptr;
        a_symbol_ptr	underlying_sym;
        underlying_type = skip_typerefs(underlying_type);
        underlying_sym = symbol_for(underlying_type);
        if ((allow_typedef || assoc_symbol->is_class_member) &&
            is_class_symbol(assoc_symbol)) {
          /* Some versions of g++ allow "class <typedef-name>" when the
             typedef name is a qualified name (determined by the caller) or
             when the typedef name is a class member (determined here).
             Only allow this if the type referred to is a class type. */
          typedef_okay = TRUE;
        } else if (is_enum_symbol(assoc_symbol) &&
                   is_unnamed_tag_symbol(underlying_sym)) {
          /* Some versions of g++ allow "enum <typedef-name>" when the
             typedef refers to an unnamed enumeration. */
          typedef_okay = TRUE;
        }  /* if */
      } else if (microsoft_mode && !assoc_symbol->is_nonreal_nested_type) {
        /* The Microsoft compiler allows "friend class X", where X is a
           typedef, but only when the elaborated type specifier is a friend
           declaration. */
        if (is_friend_decl) {
          typedef_okay = TRUE;
        } else {
          /* If the use was not in a friend declaration, discard the symbol
             that was found. */
          assoc_symbol = NULL;
          clear_specific_symbol(*locator);
        }  /* if */
      }  /* if */
      if (typedef_okay || assoc_symbol == NULL) {
        /* The code above determined that a typedef is allowed in this case,
           or cleared assoc_symbol if the typedef that was found was
           discarded. */
      } else if (assoc_symbol->is_nonreal_nested_type) {
        /* A nested class of a prototype instantiation. */
      } else {
        /* The lookup found a typedef name.  Issue a diagnostic. */
        pos_st_error(ec_typedef_in_elab_type, 
                     &error_position, assoc_symbol->header->identifier);
        set_to_error_locator(*locator);
        assoc_symbol = NULL;
      }  /* if */
    }  /* if */
    if (assoc_symbol == NULL) {
      /* A NULL symbol resulted from an error above. */
    } else if (assoc_symbol->kind == (a_symbol_kind)sk_type &&
               !is_injected_class_name) {
      /* This must be a symbol for a template parameter, and we must be in
         the midst of a prototype instantiation.  Return the symbol that
         was found. */
    } else if (assoc_symbol->kind != tag_kind &&
               ((C_mode() && !(c99_mode || gcc_mode || microsoft_mode)) ||
                any_cfront_mode()) &&
               !is_injected_class_name &&
               assoc_symbol->decl_scope !=
                        scope_stack[decl_scope_level].number) {
      /* In K&R and the C90 standard it is unclear whether using a different
         tag kind (e.g., "struct" instead of "union") creates a new type or
         not; our interpretation has always been that it does.  The C++
         standard and the C99 standard (through DR251), however, make it clear
         that using a different tag kind still finds the original type (and
         results in an error).  For cfront mode, we keep the older
         interpretation too. */
      assoc_symbol = NULL;
    } else {
      if (locator->is_semivisible_nested_type) {
        /* The symbol in the locator is a nested class that is not visible
           according to the ARM lookup rules but is returned in support of the
           nested class anachronism (ARM 18.3.5).  Issue an anachronism
           diagnostic. */
        sym_diagnostic(anachronism_error_severity, ec_nested_class_anachronism,
                       locator->specific_symbol);
      }  /* if */
      /* Do access control checking on the member.  Ambiguity has already
         been checked above. */
      check_ambiguity_and_verify_access(locator);
    }  /* if */
  }  /* if */
  return assoc_symbol;
}  /* curr_tag_symbol */


static void determine_projected_symbol_insert_location(
                                          a_symbol_locator *locator,
                                          a_type_ptr       class_type,
                                          a_boolean        *add_to_active_list,
                                          a_symbol_ptr     *insert_sym)
/*
Determine the insert location (add_to_active_list and insert_sym) required
by find_projected_symbol to insert a projection symbol for the locator
*locator into the class indicated by class_type.
*/
{
  a_symbol_ptr            prev_active_sym, active_sym;
  a_scope_stack_entry_ptr ssep;

  /* Find out whether or not the class is active, and if so, where in
     the active list its entries begin. */
  *add_to_active_list = FALSE;
  *insert_sym = NULL;
  prev_active_sym = NULL;
  active_sym = symbol_list_from_locator(*locator);
  for (ssep = &scope_stack[depth_scope_stack];
       ssep != &scope_stack[DEPTH_OF_FILE_SCOPE];
       ssep--) {
    /* If we've found the class, exit the loop. */
    if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
        same_entities(ssep->assoc_type, class_type)) {
      *add_to_active_list = TRUE;
      *insert_sym = prev_active_sym;
      break;
    }  /* if */
    /* If the stack entry may have associated entries on the active list,
       move past them. */
    if (ssep->kind != (a_scope_kind)sck_class_reactivation) {
      for (;active_sym != NULL && active_sym->decl_scope == ssep->number;
           prev_active_sym = active_sym, active_sym = active_sym->next) {
      }  /* for */
    }  /* if */
  }  /* for */
}  /* determine_projected_symbol_insert_location */


static a_boolean is_definition_of_template_member(a_symbol_ptr sym)
/*
Return TRUE if sym is a class symbol for a class that is the parent of the
current template member that is being defined; FALSE otherwise.
*/
{
  a_boolean			result = FALSE;
  a_scope_stack_entry_ptr	ssep;
  a_symbol_ptr			tmc_sym;

  ssep = &scope_stack[depth_scope_stack];
  for (tmc_sym = ssep->templ_member_class_sym; tmc_sym != NULL; ) {
    if (tmc_sym == sym) {
      result = TRUE;
      break;
    }  /* if */
    if (tmc_sym->is_class_member) {
      /* Get the class symbol. */
      a_type_ptr	parent_type;
      parent_type = sym_parent_class(tmc_sym);
      tmc_sym = (a_symbol_ptr)parent_type->source_corresp.assoc_info;
    } else {
      tmc_sym = NULL;
    }  /* if */
  }  /* for */
  return result;
}  /* is_definition_of_template_member */


static a_symbol_ptr check_for_inheriting_constructor_decl(
				a_symbol_locator	*locator,
				a_type_ptr		type)
/*
Check for an inheriting constructor declaration.  Such a declaration normally
has the form "using X::X".  When X is a class type, the normal lookup rules
find the constructor, but when X is a template parameter or typedef, we need
to handle this construct specially.  If the name following the :: is the same
as the type before the ::, this is considered to be a reference to an
inheriting constructor.  Note that this routine is only called for
using-declaration cases.  The identifier being looked up is described by
locator.  The type of the qualifier is specified by type.

In GNU and Microsoft modes, the form
  using A::X;
is also treated as an inheriting constructor case if A is a typedef for a
class type whose name is X.  For example:

  template<int> struct X;
  template<> struct X<0> {};
  template<int N> struct X: X<N-1> {
    using A = X<N-1>;
    using A::X;  // Treated as an inheriting constructor declaration
  };             // in GNU and Microsoft modes.

(In this particular example, an error will be issued in other modes because of
an attempt to declare a member of the same name as the parent class.)
*/
{
  a_symbol_ptr	sym = NULL;

  if (inheriting_constructors_enabled) {
    a_symbol_ptr	type_sym = symbol_for(type);
    if (type_sym != NULL) {
      if (locator->name_qualifier != NULL &&
          locator->name_qualifier->name != NULL &&
          (strcmp(locator->symbol_header->identifier,
                  locator->name_qualifier->name) == 0 ||
           ((microsoft_mode || (gpp_mode && !clang_mode)) &&
            is_immediate_class_type(type) &&
            strcmp(locator->symbol_header->identifier,
                   type_sym->header->identifier) == 0))) {
        /* If the type is a class type with a constructor symbol, return that
           constructor.  Otherwise, return the type symbol.  Either way, set
           the is_inheriting_ctor flag. */
        if (is_immediate_class_type(type)) {
          a_class_symbol_supplement_ptr	cssp;
          cssp = symbol_supplement_for_class(type);
          sym = cssp->constructor;
        }  /* if */
        if (sym == NULL) {
          sym = type_sym;
          locator->is_inheriting_ctor = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return sym;
}  /* check_for_inheriting_constructor_decl */

namespace {

/*
An internal class for representing flags for a class qualified id lookup, and
testing acceptance of symbols.
*/
struct a_class_qualified_lookup_options_set {
  a_class_qualified_lookup_options_set(an_id_lookup_options_set options)
    : must_be_class_or_namespace(
                              (options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0),
      must_be_tag((options & IDL_MUST_BE_TAG) != 0),
      must_be_class((options & IDL_MUST_BE_CLASS) != 0),
      is_declarator((options & IDL_IS_DECLARATOR) != 0),
      is_do_not_add_to_nonreal(
                             (options & IDL_DO_NOT_ADD_TO_NONREAL_CLASS) != 0),
      is_prototype_not_nonreal((options & IDL_USE_PROTOTYPE_NOT_NONREAL) != 0),
      is_hidden_name_lookup((options & IDL_HIDDEN_NAME_LOOKUP) != 0),
      is_do_not_create_proj_sym((options & IDL_DO_NOT_CREATE_PROJ_SYM) != 0),
      is_friend_lookup((options & IDL_FRIEND_LOOKUP) != 0),
      is_field_selection_operand(
                              (options & IDL_IS_FIELD_SELECTION_OPERAND) != 0),
      is_using_declaration((options & IDL_USING_DECLARATION) != 0),
#if MICROSOFT_EXTENSIONS_ALLOWED
      is_static_decl((options & IDL_IS_STATIC_DECL) != 0),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      is_direct_class_members_only(
                               (options & IDL_DIRECT_CLASS_MEMBERS_ONLY) != 0),
      is_member_of_direct_base((options & IDL_MEMBER_OF_UNKNOWN_BASE) != 0),
      is_expr_context((options & IDL_IS_EXPR_CONTEXT) != 0),
      is_typename_lookup((options & IDL_TYPENAME_LOOKUP) != 0),
      is_tentative_type_lookup((options & IDL_TENTATIVE_TYPE_LOOKUP) != 0),
      is_tentative_template_lookup(
                               (options & IDL_TENTATIVE_TEMPLATE_LOOKUP) != 0),
      are_proxy_members_template_ids((options & IDL_TREAT_AS_TEMPLATE_ID) != 0)
  {}

  inline a_boolean accepts(a_type_ptr   class_type_ptr,
                           a_symbol_ptr sym,
                           a_symbol_ptr fund_sym) const;

  a_boolean is_prototype_instantiation_lookup = FALSE;
  const a_boolean must_be_class_or_namespace;
  const a_boolean must_be_tag;
  const a_boolean must_be_class;
  const a_boolean is_declarator;
  const a_boolean is_do_not_add_to_nonreal;
  const a_boolean is_prototype_not_nonreal;
  const a_boolean is_hidden_name_lookup;
  const a_boolean is_do_not_create_proj_sym;
  const a_boolean is_friend_lookup;
  const a_boolean is_field_selection_operand;
  const a_boolean is_using_declaration;
#if MICROSOFT_EXTENSIONS_ALLOWED
  const a_boolean is_static_decl;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  const a_boolean is_direct_class_members_only;
  const a_boolean is_member_of_direct_base;
  const a_boolean is_expr_context;
  const a_boolean is_typename_lookup;
  const a_boolean is_tentative_type_lookup;
  const a_boolean is_tentative_template_lookup;
  const a_boolean are_proxy_members_template_ids;
private:
  inline a_boolean is_valid_gnu_injected_symbol(a_symbol_ptr fund_sym) const;
  inline a_boolean is_valid_injected_symbol(a_type_ptr   class_type_ptr,
                                            a_symbol_ptr fund_sym) const;
  inline a_boolean is_tag(a_symbol_ptr fund_sym) const;
};  /* a_class_qualified_lookup_options_set */


inline a_boolean a_class_qualified_lookup_options_set::accepts(
                                                   a_type_ptr   class_type_ptr,
                                                   a_symbol_ptr sym,
                                                   a_symbol_ptr fund_sym) const
/*
Return TRUE if sym and its accompanying fundamental symbol are acceptable
symbols in the class specified by class_type_ptr given the current lookup
options.
*/
{
  a_boolean result = TRUE;

  if (!sym->is_class_member) {
    result = FALSE;
  } else if (is_injected_class_symbol(sym) &&
             !this->is_valid_injected_symbol(class_type_ptr, fund_sym)) {
    result = FALSE;
  } else if (sym_parent_class(sym) != class_type_ptr) {
    /* Note that same_entities must not be used for this test. */
    result = FALSE;
  } else if (this->must_be_class_or_namespace &&
             !symbol_may_precede_qualifier(fund_sym)) {
    result = FALSE;
  } else if (this->must_be_class &&
             !is_class_or_class_proxy_symbol(fund_sym)) {
    result = FALSE;
  } else if (this->must_be_tag && !this->is_tag(fund_sym)) {
    result = FALSE;
  } else if (sym->is_invisible && !symbol_is(sym, sk_projection) &&
             !sym->qualified_lookup) {
    /* Ignore invisible symbols except for invisible projection symbols
       and class member symbols marked as visible to qualified lookup. */
    result = FALSE;
  } else if (this->is_typename_lookup && gpp_version_is(any_version) &&
             !is_type_symbol(fund_sym) && !this->is_tag(fund_sym)) {
    /* g++ ignores non-types for typename lookups. */
    result = FALSE;
  }  /* if */
  return result;
}  /* accepts */


inline a_boolean
a_class_qualified_lookup_options_set::is_valid_gnu_injected_symbol(
                                                         a_symbol_ptr fund_sym)
                                                                          const
/*
Return TRUE if the given injected fundamental symbol is an acceptable symbol in
g++ mode, otherwise return FALSE.
*/
{
  a_boolean result = FALSE;

  /* Injected class names are not accepted in g++ mode in prototype
     instantiation contexts to avoid a problem with names like A<T>::A<T>. */
  if (!is_prototype_instantiation_lookup && !is_using_declaration) {
    /* Starting with g++ 3.4, injected class names are returned in fewer
       contexts.  We emulate this by returning them only for tentative type
       lookup, typename lookups, and lookups in expression contexts.

       In g++ 4.5 emulation mode, tentative template/type lookups are excluded
       from returning the injected class name.  In Microsoft mode an injected
       class name is also allowed in typename lookups. */
    if (is_expr_context) {
      result = TRUE;
    } else if (gnu_version < 30400) {
      result = TRUE;
    } else if (gnu_version < 40500 &&
               (is_tentative_type_lookup || is_tentative_template_lookup)) {
      result = TRUE;
    } else if (are_proxy_members_template_ids &&
               is_injected_template_symbol(fund_sym)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_valid_gnu_injected_symbol */


inline a_boolean
a_class_qualified_lookup_options_set::is_valid_injected_symbol(
                                                   a_type_ptr   class_type_ptr,
                                                   a_symbol_ptr fund_sym) const
/*
Return TRUE if the given injected fundamental symbol is an acceptable symbol,
otherwise return FALSE.
*/
{
  a_boolean result = FALSE;

  /* An injected class name symbol is only acceptable when the injected symbol
     does not point to the class in which the lookup is being done; otherwise,
     that symbol is rejected and (typically) the constructor symbol will be
     returned later.  An injected class name is accepted when doing a
     class-or-namespace or tag lookup, because such a lookup could never find
     the constructor.  An injected class name is also accepted in g++ mode
     under some circumstance. */
  if (is_using_declaration && inheriting_constructors_enabled) {
    result = TRUE;
  } else if ((gpp_mode || microsoft_mode) && is_typename_lookup) {
    result = TRUE;
  } else if (gpp_mode && is_valid_gnu_injected_symbol(fund_sym)) {
    result = TRUE;
  } else if (is_field_selection_operand) {
    result = TRUE;
  } else if (must_be_class_or_namespace) {
    result = TRUE;
  } else if (must_be_class) {
    result = TRUE;
  } else if (must_be_tag) {
    result = TRUE;
  } else if (!same_entities(class_type_ptr, fund_sym->variant.type.ptr)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* is_valid_injected_symbol */


inline a_boolean a_class_qualified_lookup_options_set::is_tag(
                                                  a_symbol_ptr fund_sym) const
/*
Return TRUE if the given symbol is a tag or should be considered a tag for the
purposes of emulating a bug, FALSE otherwise.
*/
{
  a_boolean result = FALSE;

  if (is_tag_or_tag_proxy_symbol(fund_sym, is_friend_lookup)) {
    result = TRUE;
  } else if (microsoft_bugs && symbol_is(fund_sym, sk_type)) {
    result = TRUE;
  }
  return result;
}  /* is_tag */

}  /* namespace */

a_symbol_ptr class_qualified_id_lookup(a_symbol_locator         *locator,
                                       a_type_ptr               class_type,
                                       an_id_lookup_options_set options)
/*
Look up the identifier indicated by *locator in the class indicated by
class_type, and return a pointer to the symbol found, or NULL if
the symbol is not found.  options indicates a set of special options,
as a bit set.  For example, if IDL_MUST_BE_CLASS_OR_NAMESPACE is TRUE,
the symbol found must be a class name (or typedef to a class name) or a
namespace.  If the symbol found is a projection symbol, the
projection symbol pointer is recorded in the locator and the fundamental
symbol pointer is returned.  This routine is used in both C and C++ mode.
*/
{
  a_symbol_ptr sym, tag_symbol, class_symbol;
  a_symbol_ptr type_tag_symbol;
  a_class_symbol_supplement_ptr
               cssp;
  a_symbol_ptr insert_sym;
  a_boolean    add_to_active_list;
  a_boolean    is_proxy_or_nonreal_class_lookup = FALSE;
  a_boolean    any_nonreal_base_classes = FALSE;
  a_boolean    dependent_conversion_operator = FALSE;
  a_type_ptr   orig_class_type = class_type;
  a_class_qualified_lookup_options_set
               cls_lookup_opts(options);

  db_enter(4, "class_qualified_id_lookup");
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (locator->is_property_or_event_accessor) {
    /* If the locator refers to a property or event, do a special lookup
       for accessor functions. */
    sym = look_up_property_or_event_accessor(locator, class_type);
    goto end_lookup;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (cls_lookup_opts.is_using_declaration &&
      !cls_lookup_opts.must_be_tag &&
      !cls_lookup_opts.must_be_class_or_namespace) {
    /* Check for a using-declaration of the form "using T::T", which
       is an inheriting constructor declaration. */
    sym = check_for_inheriting_constructor_decl(locator, class_type);
    if (sym != NULL) goto end_lookup;
  }  /* if */
  /* Remove any typedef on the class type. */
  class_type = skip_typerefs_not_dependent_decltypes(class_type);
  if (cls_lookup_opts.is_declarator) {
    /* If this is a template parameter that represents a nested class of
       a class template, use the original nested type in its place. */
    class_type = orig_nested_type_if_nonreal_nested_type(class_type);
  }  /* if */
  if (class_type->kind == (a_type_kind)tk_template_param ||
      class_type->kind == (a_type_kind)tk_typeref) {
    /* We are looking up a name in a template parameter that is being used
       as a class (e.g., T::X, where T is a template parameter) or a
       dependent decltype that is being used as a class.  Each
       template parameter that is used as a class has a "proxy class"
       created for it that contains a list of member names that have
       been looked up in the class.  Any name that is looked up in the
       proxy class will be found -- if it doesn't already exist, a symbol
       entry will be created for it. */
    /* Use the proxy class in place of the template parameter type. */
    if (class_type->kind == (a_type_kind)tk_typeref) {
      /* If the original class type was a typedef to a decltype, use the
         original class type because we need the name for the proxy class. */
      class_type = orig_class_type;
    }  /* if */
    class_type = proxy_class_for_template_param(class_type);
    is_proxy_or_nonreal_class_lookup = TRUE;
  } else {
    /* Determine whether we are looking up a name in a nonreal class
       that is not the prototype instantiation.  Nonreal lookups are
       handled like proxy class lookups; any name looked up is found.
       If the symbol does not exist one will be created.  The
       assoc_scope check is used to exclude the prototype
       instantiation from being considered nonreal for lookup
       purposes. */
    if (class_type->variant.class_struct_union.is_nonreal_class) {
      if (class_type_supp(class_type)->proxy_of_type != NULL) {
        /* A proxy class. */
        is_proxy_or_nonreal_class_lookup = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (class_type->
                            variant.class_struct_union.is_generic_instance) {
        /* A lookup in a C++/CLI generic definition or open constructed
           (i.e., nonreal) generic instance.  This can occur if a generic
           is instantiated on a template-based managed type. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else if (class_type
                    ->variant.class_struct_union.is_prototype_instantiation ||
                 class_type->variant.class_struct_union.
                                           is_ms_instantiated_nonreal_class ||
                 !class_type->variant.class_struct_union.is_template_class) {
        /* A prototype instantiation, or a class defined as part of a
           prototype instantiation (e.g., a local class defined in the
           prototype instantiation of a function template). */
        cls_lookup_opts.is_prototype_instantiation_lookup = TRUE;
      } else if (!class_type->variant.class_struct_union.
                                            is_ms_instantiated_nonreal_class ||
                 cls_lookup_opts.is_member_of_direct_base) {
        /* Another kind of nonreal class.  The proxy/nonreal lookup is not
           done for Microsoft instantiated nonreal classes, but an additional
           lookup may be done for these classes below if the initial lookup
           fails. */
        is_proxy_or_nonreal_class_lookup = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  class_symbol = symbol_for(class_type);
  cssp = class_symbol->variant.class_struct_union.extra_info;
  any_nonreal_base_classes = cssp->any_nonreal_base_classes;
  /* Determine whether the thing being looked up is a template dependent
     conversion operator name in an expression context.  When looking for
     such names, the ordinary lookup is suppressed causing an unknown
     function symbol to be created later. */
  if (locator->is_conversion_name && cls_lookup_opts.is_expr_context) {
    dependent_conversion_operator =
           is_template_dependent_type(locator->variant.conversion_result_type);
  }  /* if */
  sym = locator->specific_symbol;
  if (is_error_locator(*locator) || locator->is_udl_operator_name) {
    /* The locator is an error locator or it designates a user-defined
       literal operator name (which cannot be a class member), so return
       NULL (i.e., no symbol found). */
    sym = NULL;
  } else if (sym != NULL) {
    /* The locator is for a specific symbol, so return the symbol for it. */
    /* Make sure that the symbol we are returning matches the lookup
       criteria.  This check is suppressed if the do_not_clear_specific_symbol
       flag is set, which usually indicates that the symbol is a coalesced
       template reference. */
#if CHECKING
    a_symbol_ptr  fund_sym = fundamental_symbol_of(sym);
    check_assertion(cls_lookup_opts.accepts(class_type, sym, fund_sym) ||
                    locator->do_not_clear_specific_symbol);
#endif /* CHECKING */
  } else {
    /* Search for a symbol in the right scope.  Suppress the normal lookup
       part of the search when looking for a dependent conversion operator in
       an expression context. */
    if (!dependent_conversion_operator) {
      /* Bypass the normal loop when looking for the assignment operator.
         This is an optimization as the inactive list for assignment operators
         is usually long. */
      if (locator->is_operator_name &&
          locator->variant.opname == (an_opname_kind)onk_assign) {
        sym = cssp->assignment_operator;
        /* Ignore the operator= symbol if it does not meet the lookup
           criteria. */
        if (sym != NULL && !cls_lookup_opts.accepts(class_type, sym, sym)) {
          sym = NULL;
        }  /* if */
        if (sym != NULL) {
          goto end_lookup;
        } else {
          goto bypass_normal_search;
        }  /* if */
      }  /* if */
      /* First, search the list of inactive symbols.  These are class
         members for classes that are no longer active.  Or, in C,
         fields of structs/unions. */
      tag_symbol = NULL;
      type_tag_symbol = NULL;
      for (sym = find_symbol_list_in_table(&cssp->pointers_block,
                                           locator->symbol_header);
           sym != NULL;
           sym = sym->next_in_lookup_table) {
        a_symbol_ptr  fund_sym = fundamental_symbol_of(sym);
        if (cls_lookup_opts.accepts(class_type, sym, fund_sym)) {
          /* Found an acceptable symbol. */
          if (is_proxy_or_nonreal_class_lookup &&
              !acceptable_nonreal_class_member_symbol(sym, options, locator)) {
            /* The nonreal class member found does not match the kind required
               by the lookup.  Ignore this symbol. */
          } else if (any_nonreal_base_classes &&
                     sym->kind == (a_symbol_kind)sk_projection &&
                     sym->variant.projection.fund_sym_is_nonreal_member &&
                     !acceptable_nonreal_class_member_symbol(fund_sym, options,
                                                             locator)) {
            /* The symbol is a projection symbol in a derived class that points
               to a nonreal member of a base class.  Ignore this symbol
               if it does not match the kind required by the lookup. */
          } else if (cls_lookup_opts.is_direct_class_members_only &&
                     sym->kind == (a_symbol_kind)sk_projection &&
                     !sym->variant.projection.is_using_decl) {
            /* This is a projection symbol not created by a using-declaration.
               This should be ignored for "direct class members only"
               lookups. */
          } else {
            /* When looking for a tag symbol, both tags and typedefs may
               match the "acceptable" test.  The tag should be preferred
               over the typedef, so if we find a typedef we must keep
               looking.  Similarly, when not doing a "must be tag" lookup,
               both tags and non-tags will match the test.  A non-tag
               should be preferred over the tag, so if we find a tag we
               must keep looking. */
           if (!cls_lookup_opts.must_be_tag) {
              /* A normal lookup. */
              if (is_tag_symbol(fund_sym)) {
                if (tag_symbol != NULL &&
                    symbol_is(tag_symbol, sk_namespace_projection)) {
                  /* If a using-declaration to a tag symbol is followed by
                     an actual tag symbol, ignore the first one. */
                  tag_symbol = NULL;
                }  /* if */
                /* If a tag symbol is followed by a projection to a
                   different tag, use the first symbol. */
                check_assertion_or_expect_error(
                                            tag_symbol == NULL ||
                                            symbol_is(sym, sk_projection) ||
                                            is_injected_class_symbol(sym));
                if (tag_symbol == NULL) tag_symbol = sym;
              } else {
                if (sym->kind == (a_symbol_kind)sk_type) {
                  type_tag_symbol = sym;
                } else {
                  /* Take the symbol. */
                  goto end_lookup;
                }  /* if */
              }  /* if */
            } else {
              /* A tag lookup. */
              if (sym->kind == (a_symbol_kind)sk_type) {
                if (type_tag_symbol != NULL &&
                    symbol_is(type_tag_symbol, sk_namespace_projection)) {
                  /* If a using-declaration to a tag symbol is followed by
                     an actual tag symbol, ignore the first one. */
                  type_tag_symbol = NULL;
                }  /* if */
                /* If a tag symbol is followed by a projection to a
                   different tag, use the first symbol. */
                check_assertion_or_expect_error(
                          tag_symbol == NULL || symbol_is(sym, sk_projection));
                if (tag_symbol == NULL) tag_symbol = sym;
                if (type_tag_symbol == NULL) type_tag_symbol = sym;
              } else {
                /* Take the symbol. */
                goto end_lookup;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
      /* We reached the end of the list.  If there is a tag symbol, or
         type tag symbol saved within the loop, use it. */
      if (type_tag_symbol != NULL) {
        if (microsoft_bugs && cls_lookup_opts.must_be_tag &&
            !cls_lookup_opts.is_direct_class_members_only) {
          /* MSVC++ allows an elaborated-type-specifier with a qualified-id to
             refer to a typedef for a tagged type.  Return the symbol for the
             tagged type.  (If cls_lookup_opts.is_direct_class_members_only is
             TRUE, the typeref should not be skipped since that may result in a
             nonmember type.)  In some cases (such as g++ typename lookup) the
             underlying type might not have a symbol.  In that case use the
             original symbol. */
          sym = (a_symbol_ptr)skip_typerefs(type_tag_symbol->
                                  variant.type.ptr)->source_corresp.assoc_info;
          if (sym == NULL) sym = type_tag_symbol;
        } else {
          sym = type_tag_symbol;
        }  /* if */
        goto end_lookup;
      } else if (tag_symbol != NULL) {
        sym = tag_symbol;
        goto end_lookup;
      }  /* if */
    }  /* if */
bypass_normal_search:
    if (is_proxy_or_nonreal_class_lookup &&
        !cls_lookup_opts.is_do_not_add_to_nonreal) {
      /* When looking up a name in a proxy or nonreal class, the name is
         always found.  If we did not find the name in the search
         above then we must create a symbol now. */
      sym = add_member_to_proxy_or_nonreal_class(class_type, options, locator);
      goto end_lookup;
    }  /* if */
    if (C_dialect == C_dialect_cplusplus) {
      /* Look to see if the name is the name of a constructor or destructor
         for the class.  The symbols for those are not entered in the
         normal symbol table; they're pointed to from the class symbol
         supplement. */
      if (locator->symbol_header == class_symbol->header) {
        /* Looking up the class name within itself.  Return the constructor if
           there is one. */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (cls_lookup_opts.is_static_decl) {
          sym = cssp->static_constructor;
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          sym = cssp->constructor;
        }  /* if */
        if (sym == NULL && !class_type->incomplete &&
            !class_type->variant.class_struct_union.is_nonreal_class) {
          sym = generate_trivial_ctors(class_symbol);
        }  /* if */
        if (sym != NULL) {
          /* Change the locator symbol header to the header for the constructor
             rather than the header for the class.  They have the same name,
             but different headers. */
          locator->symbol_header = sym->header;
          goto end_lookup;
        }  /* if */
      }  /* if */
      if (locator->is_destructor_name) {
        sym = cssp->destructor;
        if (sym == NULL && !class_type->incomplete &&
            !class_type->variant.class_struct_union.is_nonreal_class) {
          sym = generate_trivial_dtor(class_symbol, locator);
        }  /* if */
        if (sym != NULL) {
          if (locator->symbol_header == sym->header) {
            /* This is the destructor. */
          } else {
            /* Something like "X::~Y".  That is not a valid name for the
               destructor. */
            sym = NULL;
          }  /* if */
          goto end_lookup;
        }  /* if */
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cli_or_cx_enabled && cssp->finalizer != NULL &&
          locator->symbol_header == cssp->finalizer->header) {
        /* This is the finalizer. */
        sym = cssp->finalizer;
        goto end_lookup;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (sym == NULL && !cls_lookup_opts.is_direct_class_members_only &&
          locator->is_conversion_name &&
          !cls_lookup_opts.is_using_declaration) {
        /* We still haven't found a symbol, we are looking for a conversion
           function,  and this class has conversion function templates.
           See if any of the templates match the type desired.  This
           lookup is suppressed for member using-declarations because
           it should not be possible for a derived class to name a template
           instance in a using-declaration. */
        sym = look_up_conversion_template_instance(locator, class_type);
        if (sym != NULL) goto end_lookup;
      }  /* if */
      if (!cls_lookup_opts.is_direct_class_members_only &&
          !locator->is_destructor_name) {
        /* The name was not found.  Try looking for a member symbol that can
           be projected into the class.  When doing a "direct class members
           only" lookup, projection symbols are not created and conversion
           template instances are not found.  Destructors are not inherited,
           so don't attempt to look for a destructor name. */
        a_boolean	use_nonreal_in_curr_class = FALSE;
        determine_projected_symbol_insert_location(locator,
                                                   class_type,
                                                   &add_to_active_list,
                                                   &insert_sym);
        if (!cls_lookup_opts.is_do_not_add_to_nonreal &&
            (!cls_lookup_opts.is_declarator ||
             (cls_lookup_opts.is_friend_lookup &&
              (gpp_mode || ms_extensions))) &&
            (cssp->any_nonreal_base_classes ||
             (gpp_mode || ms_extensions)) &&
            cls_lookup_opts.is_prototype_instantiation_lookup) {
          /* If a nonreal member needs to be created, create it as a member of
             the class in which the lookup is being done.  See the comment
             below where create_proxy_of_nonreal_class_member is called for
             more information. */
          use_nonreal_in_curr_class = TRUE;
        }  /* if */
        (void)find_projected_symbol(
                            class_type, locator, options,
                            /*look_in_dependent_bases=*/TRUE,
                            !treat_as_cli_class_for_lookup(class_type),
                            /*tentative_type_lookup=*/FALSE,
                            /*tentative_template_lookup=*/FALSE,
                            cls_lookup_opts.is_hidden_name_lookup ||
                            cls_lookup_opts.is_do_not_create_proj_sym ||
                            cssp->any_nonreal_base_classes,
                            add_to_active_list, insert_sym, &sym,
                            /*can_create_nonreal=*/FALSE);
        if (use_nonreal_in_curr_class && sym != NULL &&
            fundamental_symbol_of(sym)->is_nonreal_member) {
          /* If we found a previously created nonreal member, ignore it if we
             should be using a nonreal member of the class in which the lookup
             is actually being done. */
          sym = NULL;
        }  /* if */
        if (sym == NULL && use_nonreal_in_curr_class) {
          /* This is a lookup of a name like D::x, where D is a
             prototype instantiation or local class of a prototype
             instantiation.  Create a symbol whose parent is the class
             in which the lookup is being done.  In g++ and Microsoft mode
             a friend declaration in a prototype instantiation is allowed
             to refer to a non-existent member of the prototype
             instantiation. */
          check_assertion(cls_lookup_opts.is_prototype_instantiation_lookup);
          sym = create_proxy_or_nonreal_class_member(
                                          class_type,
                                          options | IDL_MEMBER_OF_UNKNOWN_BASE,
                                          locator);
        }  /* if */
      }  /* if */
    }  /* if */
end_lookup:
    if (sym == NULL && microsoft_mode &&
        cls_lookup_opts.is_using_declaration &&
        class_type->variant.class_struct_union.
                                            is_ms_instantiated_nonreal_class) {
      /* The initial lookup failed and this is a Microsoft instantiated nonreal
         class.  Attempt the lookup again as a normal nonreal class. */
      sym = class_qualified_id_lookup(locator, class_type,
                                      options | IDL_MEMBER_OF_UNKNOWN_BASE);
    }  /* if */
    if (sym != NULL) {
      /* Unless the prototype symbol was explicitly requested, check for an
         associated nonreal type symbol. */
      if (!cls_lookup_opts.is_prototype_not_nonreal) {
        a_symbol_ptr	possible_nonreal_sym;
        possible_nonreal_sym = nonreal_type_if_nested_prototype_type(sym);
        if (possible_nonreal_sym != sym) {
          /* Except when the symbol is associated with a class template member
             that is being defined, use the nonreal symbol. */
          if (!is_definition_of_template_member(sym)) {
            sym = possible_nonreal_sym;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    locator->specific_symbol = sym;
  }  /* if */
  if (sym != NULL) {
    /* If the symbol is a projection symbol, reduce it to the fundamental
       symbol.  The specific_symbol in the locator stays pointing to the
       projection symbol. */
    reduce_projection_symbol_to_fundamental_symbol(sym);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcx_enabled && sym == NULL &&
      is_managed_class_type(class_type) &&
      locator->symbol_header != NULL &&
      locator->symbol_header->identifier != NULL &&
      strncmp(locator->symbol_header->identifier,
              "__abi_" , sizeof("__abi_")-1) == 0 &&
      !cls_lookup_opts.is_direct_class_members_only) {
    /* In C++/CX mode, fake up a symbol for __abi_* locators in C++/CX
       types. */
    sym = make_and_enter_abi_member_function_symbol(locator, class_type);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    fprintf(f_debug, "class_qualified_id_lookup: id = %s, %s\n",
                     (locator->symbol_header != NULL) ?
                                           locator->symbol_header->identifier :
                                           "NULL",
                     (sym != NULL) ? "found" : "not found");
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  db_exit();
  return sym;
}  /* class_qualified_id_lookup */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void add_symbol_to_super_set(a_symbol_locator    *locator,
				    a_symbol_ptr	new_sym,
				    a_symbol_ptr	*result_sym,
				    a_type_ptr          class_type,
				    a_base_class_ptr	base_class,
				    a_type_ptr		orig_base_type)
/*
Add "new_sym" to the set of symbols specified by "result_sym".  When
called for the first symbol, result_sym is set to a projection symbol
that points to the symbol.  For subsequent symbols (assuming they
are functions), result_sym is replaced with an overload set symbol
that points to the list of function symbols.  If more than one non-function
symbol is found, the first one is used but is marked as ambiguous.
"base_class" is the base class in which new_sym was found.
*/
{
  a_boolean		is_function;
  a_symbol_ptr		fund_result_sym = NULL;
  a_symbol_ptr		fund_new_sym;

  fund_new_sym = fundamental_symbol_of(new_sym);
  is_function = is_function_or_template_symbol(fund_new_sym);
  /* Check the accessibility of the symbol. */
  if (*result_sym != NULL) {
    fund_result_sym = fundamental_symbol_of(*result_sym);
  }  /* if */
  if (*result_sym != NULL && (*result_sym)->ambiguous) {
    /* We already detected an ambiguity. */
  } else if (*result_sym != NULL && 
             (!is_function_or_template_symbol(fund_result_sym) ||
              !is_function)) {
    /* A combination of function and non-function symbols or two non-function
       symbols.  Mark the result symbol as ambiguous. */
    (*result_sym)->ambiguous = TRUE;
  } else {
    /* Add the symbol to the set.  Start by creating a projection symbol
       that points to new_sym. */
    a_symbol_ptr	new_proj;
    a_boolean		is_using_decl = FALSE;
    if (new_sym->kind == (a_symbol_kind)sk_projection) {
      /* The new symbol is a projection symbol when it comes from a
         using-declaration.  Get the base class information for the member
         named in the using-declaration as a base class of the class in which
         the using-declaration was found. */
      base_class = corresp_base_class(new_sym->variant.projection.extra_info->
                                                        fundamental_base_class,
                                      base_class);
      is_using_decl = TRUE;
    }  /* if */
    new_proj = make_projection_symbol(fund_new_sym, class_type, base_class,
                                     (a_derivation_step_ptr)NULL,
                                     /*ambiguous=*/FALSE);
    /* Note that projection symbols for using-declarations have the
       source position of the using-declaration itself, whereas
       other projection symbols take on the source position of the
       fundamental symbol. */
    new_proj->decl_position = locator->source_position;
    set_decl_sequence_number(new_proj);
    new_proj->is_super_reference = TRUE;
    /* Mark the projection symbol as a using-declaration so that the "this"
       parameter will be considered an exact match for overload resolution.
       This is only done if the symbol found actually was a using-declaration
       or if the symbol was found directly in the base class being searched
       (not in an indirect base class). */
    if (is_using_decl || orig_base_type == fund_new_sym->parent.class_type) {
      new_proj->variant.projection.is_using_decl = TRUE;
    }  /* if */
    if (*result_sym == NULL) {
      /* This is the first symbol in the set. */
      *result_sym = new_proj;
    } else {
      a_symbol_ptr	overload_sym;
      /* Add to an existing symbol. */
      if ((*result_sym)->kind == (a_symbol_kind)sk_projection) {
        /* The current symbol is not an overload set.  Create one now. */
        overload_sym = alloc_symbol((a_symbol_kind)sk_overloaded_function,
                                     (*result_sym)->header,
                                     &((*result_sym)->decl_position));
        overload_sym->decl_scope = (*result_sym)->decl_scope;
        overload_sym->decl_seq = (*result_sym)->decl_seq;
        set_class_membership(overload_sym, (a_source_correspondence *)NULL,
                             sym_parent_class(*result_sym));
        overload_sym->variant.overloaded_function.symbols = *result_sym;
        (*result_sym)->overload_set_member = TRUE;
        *result_sym = overload_sym;
        overload_sym->is_super_reference = TRUE; 
      } else {
        /* The current symbol is an overload set. */
        overload_sym = *result_sym;
      }  /* if */
      /* Add the new symbol to the overload list. */
      new_proj->next = overload_sym->variant.overloaded_function.symbols;
      overload_sym->variant.overloaded_function.symbols = new_proj;
      new_proj->overload_set_member = TRUE;
    }  /* if */
  }  /* if */
}  /* add_symbol_to_super_set */


a_type_ptr get_super_class_type(void)
/*
Determine whether we are within a class or class reactivation scope.  If
so, return the associated class type.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_type_ptr			class_type = NULL;

  for (ssep = scope_stack_entry_for(depth_scope_stack);
       ssep != NULL; ssep = previous_scope_of(ssep)) {
    if (ssep->kind == (a_scope_kind)sck_class_struct_union ||
        ssep->kind == (a_scope_kind)sck_class_reactivation) {
      class_type = ssep->assoc_type;
      break;
    }  /* if */
  }  /* for */
  return class_type;
}  /* get_super_class_type */


static a_symbol_ptr find_super_lookup_symbol(
				a_symbol_ptr			symbol_list,
				a_symbol_header_ptr		sym_header,
				an_id_lookup_options_set	options)
/*
Look through symbol_list for a previously created __super lookup symbol
that matches the lookup criteria specified by options and sym_header.
*/
{
  a_symbol_ptr	sym;

  for (sym = symbol_list; sym != NULL; sym = sym->next) {
    a_boolean		must_be_class_or_namespace
                             = (options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0;
    a_boolean		must_be_tag = (options & IDL_MUST_BE_TAG) != 0;
    a_boolean		must_be_class = (options & IDL_MUST_BE_CLASS) != 0;
    a_boolean		tentative_type_lookup
                                  = (options & IDL_TENTATIVE_TYPE_LOOKUP) != 0;
    if (sym->header == sym_header &&
        (a_boolean)sym->must_be_class_or_namespace_lookup ==
                                               must_be_class_or_namespace &&
        (a_boolean)sym->tentative_type_lookup == tentative_type_lookup &&
        (a_boolean)sym->must_be_class_lookup == must_be_class &&
        (a_boolean)sym->must_be_tag_lookup == must_be_tag) {
       break;
    }  /* if */
  }  /* for */
  return sym;
}  /* find_super_lookup_symbol */


static void save_super_lookup_symbol(
				a_symbol_ptr			sym,
				a_class_symbol_supplement_ptr	cssp,
				an_id_lookup_options_set	options)
/*
Save "sym" on the list of reusable super lookup symbols of "cssp".  "options"
is the set of lookup options used to produce "sym".
*/
{
  /* Set the flags in the symbol to reflect the lookup options used. */
  sym->must_be_class_or_namespace_lookup =
                               (options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0;
  sym->must_be_tag_lookup = (options & IDL_MUST_BE_TAG) != 0;
  sym->tentative_type_lookup = (options & IDL_TENTATIVE_TYPE_LOOKUP) != 0;
  sym->must_be_class_lookup = (options & IDL_MUST_BE_CLASS) != 0;
  /* Add it to the list of super lookups. */
  sym->next = cssp->super_lookup_symbols;
  cssp->super_lookup_symbols = sym;
}  /* save_super_lookup_symbol */


static a_boolean is_base_of_other_base(
				a_base_class_ptr		base_to_find,
				a_class_type_supplement_ptr	ctsp)
/*
Go through the direct base classes of the class specified by ctsp and check
whether the base class specified by bcp is a base class of one of the other
bases.
*/
{
  a_base_class_ptr	bcp;
  a_boolean		result = FALSE;

  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
    if (bcp != base_to_find && bcp->direct) {
       if (find_base_class_of(bcp->type, base_to_find->type)) {
         /* We found a direct base that has base_to_find as a base class. */
         result = TRUE;
       }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* is_base_of_other_base */


a_symbol_ptr super_qualified_id_lookup(
				a_symbol_locator		*locator,
				an_id_lookup_options_set	options)
/*
This routines implements the lookup of a name that follows the
Microsoft __super keyword.
*/
{
  a_type_ptr                    class_type;
  a_symbol_ptr                  result_sym = NULL;
  a_boolean			reused_symbol = FALSE;
  a_class_type_supplement_ptr   ctsp;
  a_class_symbol_supplement_ptr cssp;
  a_base_class_ptr              bcp;
  a_boolean			exclude_interface_members = FALSE;

  /* Get the type of the current class on the scope stack, if any. */
  class_type = get_super_class_type();
  if (class_type != NULL) {
    /* If this is a template, make sure it is instantiated. */
    complete_class_type_is_needed(class_type);
    /* If lookup begins in a C++/CLI class, base interface classes are not
       considered.  Direct interfaces are ignored below.  This flag
       causes indirect base interfaces of base ref classes to be
       ignored. */
    if (cli_or_cx_enabled && treat_as_cli_class_for_lookup(class_type)) {
      options |= IDL_EXCLUDE_BASE_INTERFACE_MEMBERS;
      exclude_interface_members = TRUE;
    }  /* if */
    /* Go through its base classes and look for members that match
       the lookup. */
    ctsp = class_type->variant.class_struct_union.extra_info;
    cssp = symbol_supplement_for_class(class_type);
    if (is_reusable_super_lookup(options)) {
      /* Look for a previously created super lookup symbol that matches the
         lookup options being used. */
      result_sym = find_super_lookup_symbol(cssp->super_lookup_symbols,
                                            locator->symbol_header, options);
    }  /* if */
    if (result_sym != NULL) {
      /* We are reusing a previously created symbol. */
      reused_symbol = TRUE;
    } else if (cssp->any_nonreal_base_classes) {
      /* There are nonreal base classes so we can't guarantee that we will
         get the right result by doing the lookup now. */
      result_sym = create_proxy_or_nonreal_class_member(
                                         class_type,
                                         options | IDL_MEMBER_OF_UNKNOWN_SUPER,
                                         locator);
    } else {
      /* A class with only real base classes. */
      for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
        a_symbol_ptr	base_sym;
        /* Only consider direct base classes.  In C++/CLI mode, ignore
           interfaces if the lookup started in a ref class. */
        if (!bcp->direct ||
            (bcp->is_virtual && is_base_of_other_base(bcp, ctsp)) ||
            (exclude_interface_members &&
             is_cli_interface_type(bcp->type))) {
          continue;
        }  /* if */
        /* Clear the specific symbol so that it does not influence the lookup
           below. */
        clear_specific_symbol(*locator);
        base_sym = class_qualified_id_lookup(locator, bcp->type, options);
        if (base_sym != NULL) {
          a_boolean		is_list;
          a_symbol_ptr		proj_base_sym;
          a_base_class_ptr	fundamental_bcp;
          /* See if the base class symbol is itself a projection symbol. */
          proj_base_sym = locator->specific_symbol;
          if (proj_base_sym->kind == (a_symbol_kind)sk_projection) {
            a_base_class_ptr  disambiguator;
            /* It is a projection symbol.  Get the base class information from
               the projection symbol information. */
            fundamental_bcp = proj_base_sym->variant.projection.extra_info->
                                                       fundamental_base_class;
            /* Find the base class that represents fundamental_bcp as a
               base class of the class specified by class_type. */
            disambiguator = find_disambiguator(bcp, fundamental_bcp);
            fundamental_bcp = corresponding_base_class(fundamental_bcp,
                                                       class_type,
                                                       disambiguator);
          } else {
            fundamental_bcp = bcp;
          }  /* if */
          /* If the base class symbol is an overloaded function, process
             each of the overload set members. */
          is_list = (base_sym->kind == (a_symbol_kind)sk_overloaded_function);
          if (is_list) {
            base_sym = base_sym->variant.overloaded_function.symbols;
          }  /* if */
          for (; base_sym != NULL;
                 base_sym = (is_list ? base_sym->next : NULL)) {
            add_symbol_to_super_set(locator, base_sym, &result_sym,
                                    class_type, fundamental_bcp,
                                    bcp->type);
          }  /* for */
        }  /* if */
      }  /* for */
    }  /* if */
    /* If we ended up with an overload set, set the mixed static/nonstatic
       flag. */
    if (result_sym != NULL &&
        result_sym->kind == (a_symbol_kind)sk_overloaded_function) {
      set_mixed_static_nonstatic_flag(result_sym);
    }  /* if */
    locator->specific_symbol = result_sym;
    if (result_sym != NULL && !reused_symbol &&
        is_reusable_super_lookup(options)) {
      /* Save this symbol for possible reuse later. */
      save_super_lookup_symbol(result_sym, cssp, options);
    }  /* if */
    /* If the symbol is a projection symbol, reduce it to the fundamental
       symbol.  The specific_symbol in the locator stays pointing to the
       projection symbol. */
    if (result_sym != NULL) {
      reduce_projection_symbol_to_fundamental_symbol(result_sym);
    }  /* if */
  }  /* if */
  return result_sym;
}  /* super_qualified_id_lookup */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if CHECKING

static inline a_boolean is_acceptable_enum_symbol(a_symbol_ptr sym,
                                                  a_type_ptr   enum_type)
/*
Return TRUE if the given symbol is a constant with matching enum type, FALSE
otherwise.
*/
{
  a_boolean result = TRUE;

  /* Check for disqualifying conditions that prove the symbol isn't required
     enum symbol. */
  if (sym->kind != (a_symbol_kind)sk_constant) {
    result = FALSE;
  } else if (!same_entities(sym->variant.constant->type, enum_type)) {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_acceptable_enum_symbol */

#endif /* CHECKING */

a_symbol_ptr enum_qualified_id_lookup(a_symbol_locator		*locator,
				      a_type_ptr		enum_type)
/*
Look up the identifier indicated by *locator as an enumerator associated
with the enumeration specified by enum_type.  This is used in Microsoft
mode to permit an enumerator to be used in a qualified name and in
modes in which C++11-style scoped enums are allowed.  The enumerator that
is found is returned, or NULL if no matching symbol is found.
*/
{
  a_symbol_ptr			sym;
  a_constant_ptr		cp = NULL;

  db_enter(4, "enum_qualified_id_lookup");
  /* Remove any typedefs on the enum type. */
  enum_type = skip_typerefs(enum_type);
  sym = locator->specific_symbol;
  /* The enumerators are needed here, so instantiate the definition of the
     enumeration if that has not been done yet. */
  instantiate_template_enum_if_needed(enum_type);
  if (is_error_locator(*locator)) {
    /* The locator is an error locator, so return NULL (i.e., no symbol
       found). */
    sym = NULL;
  } else if (sym != NULL) {
    /* The locator is for a specific symbol, so return the symbol for it. */
    /* Make sure that the symbol we are returning matches the lookup
       criteria.  This check is suppressed if the do_not_clear_specific_symbol
       flag is set, which usually indicates that the symbol is a coalesced
       template reference. */
    check_assertion(is_acceptable_enum_symbol(sym, enum_type) ||
                    locator->do_not_clear_specific_symbol);
  } else {
    /* Look for the constant on the list of constants for the enum type. */
    if (enum_type->variant.integer.is_scoped_enum) {
      a_scope_ptr	sp;
      sp = enum_type->variant.integer.enum_info.assoc_scope;
      if (sp != NULL) {
        cp = sp->constants;
      }  /* if */
    } else {
      cp = enum_type->variant.integer.enum_info.constant_list;
    }  /* if */
    for (; cp != NULL; cp = cp->next) {
      sym = symbol_for(cp);
      check_assertion(sym != NULL);
      if (sym->header == locator->symbol_header) break;
    }  /* for */
    if (cp == NULL) sym = NULL;
    locator->specific_symbol = sym;
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    fprintf(f_debug, "enum_qualified_id_lookup: id = %s, %s\n",
                     locator->symbol_header->identifier,
                     (sym != NULL) ? "found" : "not found");
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  db_exit();
  return sym;
}  /* enum_qualified_id_lookup */


/* Forward declaration. */
static a_symbol_ptr lookup_in_namespace(
		a_symbol_locator		*locator,
		a_namespace_ptr			ns_ptr,
		an_id_lookup_options_set	options,
		a_namespace_ptr			orig_ns_ptr,
		a_symbol_ptr			*synth_sym,
		a_boolean			inline_namespace_lookup,
		a_boolean			*any_errors);


static
a_symbol_ptr qualified_using_directive_lookup(
			a_symbol_locator		*locator,
			a_namespace_ptr			ns_ptr,
			a_scope_ptr			ns_scope,
			an_id_lookup_options_set	options,
			a_namespace_ptr			orig_ns_ptr,
			a_symbol_ptr			*synth_sym,
			a_boolean			*any_errors,
			a_boolean			inline_namespace_only)
/*
Do a qualified lookup of namespaces nominated by using directives
in this scope.  This is used for qualified namespace and file scope
lookups.  For functions, the symbol returned may be an overload set
containing functions from several namespaces.  For nonfunctions,
an ambiguity may need to be diagnosed.

locator describes the symbol being looked up.  ns_scope is the namespace
(or file scope) in which we should look for the symbol.  if ns_scope is
a namespace, ns_ptr points to the namespace.  options are the lookup options
to be used.  orig_ns_ptr is the namespace specified in the qualifier.
*synth_sym points to a synthesized projection symbol that captures the
results of the lookup.  *any_errors is set to TRUE if an ambiguity is
detected.

         D        E
          \      /
           B    C
            \  /
             A

  namespace E { int l; }  
  namespace D { int j, l; void g(double); }
  namespace C { int k;    void g(char); using namespace E; }
  namespace B { int j;    void g(int);  using namespace D; }
  namespace A { int i;    void f();     using namespace B; using namespace C;}

A qualified lookup begins with the namespace specified by the qualifier.
If the name is found there, the lookup stops.  If it is not found there,
the lookup looks in each of the namespaces used in using directives in
that namespace.  Functions from each namespace are merged together
into an overload set.  If more than one name is found, and they are not
all functions, the lookup is ambiguous.

Using the example above, the result of the following lookups are
as follows:

	Lookup		Result
 	------		------
	A::i		A::i
	A::j		B::j
	A::k		C::k
	A::l		ambiguous (in D and E)
	A::f()		A::f()
	A::g(1)		B::g(int)
	A::g('x')	C::g(char)
	A::g(2.0)	ambiguous (B::g or C::g.  D::g is hidden)

inline_namespace_only is TRUE if a symbol was found in a given namespace
and that symbol can overload with symbols from inline namespaces (or
named in strong using-directives in g++ mode).

If no symbol is found in the specified namespace, NULL is returned.
*/
{
  a_using_decl_ptr			udp;
  a_symbol_ptr				sym;
  a_namespace_symbol_supplement_ptr	nssp = NULL;
  a_symbol_ptr				result_sym = NULL;

  /* Get the list of using directives from the scope in which the lookup
     is being done. */
  udp = ns_scope->using_directives;
  if (ns_ptr != NULL) nssp = symbol_supplement_for_namespace(ns_ptr);
  /* Set a flag that indicates that this namespace is being processed so
     that in case of a recursive reference it is not visited again.
     Note that it is still possible for a namespace to be visited twice
     if it appears more than once on the lattice.  From a language
     point of view it should not be visited twice, but our implementation
     will disregard symbols that are already part of the lookup set. */
  if (nssp != NULL) nssp->visited_by_qualified_lookup = TRUE;
  /* Look through each of the using directives in this namespace.  We
     keep going even if an ambiguity is detected because the symbol
     returned may differ depending on the lookup options so we need
     to check each symbol to make sure we return the appropriate one. */
  for (; udp != NULL; udp = udp->next) {
    /* Ignore using-directives that are not for inline namespaces
       if the inline_namespace_only flag was passed in. */
    if (udp->is_using_directive &&
        (!inline_namespace_only || udp->inline_namespace)) {
      a_namespace_symbol_supplement_ptr	next_nssp;
      a_namespace_ptr			assoc_namespace;

      assoc_namespace =
                  skip_namespace_aliases((a_namespace_ptr)udp->entity.ptr);
      next_nssp = symbol_supplement_for_namespace(assoc_namespace);
      /* Skip this namespace if we have already looked in it. */
      if (next_nssp->visited_by_qualified_lookup) continue;
      sym = lookup_in_namespace(locator, assoc_namespace, options,
                                orig_ns_ptr, synth_sym,
                                inline_namespace_only, any_errors);
      if (sym != NULL && sym->synthesized_namespace_projection) {
        *synth_sym = sym;
      } else if (sym != NULL) {
        /* If this lookup found a symbol, add it to the lookup set.
           Don't do this if it is already a synthesized namespace
           projection -- such symbols are already represented in synth_sym. */
        if (*synth_sym == NULL) {
          /* Look for an existing synthesized namespace projection symbol
             from a previous lookup that can be reused. */
          *synth_sym = find_synthesized_projection_symbol
                                    (locator, options,
                                     /*qualified_lookup=*/TRUE, orig_ns_ptr);
        }  /* if */
        /* Add the new symbol to an existing lookup set.  Note that
           *synth_sym may be NULL at this point. */
        *synth_sym = add_symbol_to_lookup_set(*synth_sym, sym, locator,
                                              /*qualified_lookup=*/TRUE,
                                              orig_ns_ptr, options,
                                              any_errors);
      }  /* if */
      result_sym = *synth_sym;
    }  /* if */
  }  /* for */
  /* Clear the flag that indicates this namespace is being processed. */
  if (nssp != NULL) nssp->visited_by_qualified_lookup = FALSE;
  return result_sym;
}  /* qualified_using_directive_lookup */

namespace {

/*
An internal class for representing flags for a namespace lookup, and testing
acceptance of symbols.
*/
struct a_namespace_lookup_options_set {
  a_namespace_lookup_options_set(an_id_lookup_options_set options)
    : must_be_class_or_namespace(
                              (options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0),
      must_be_tag((options & IDL_MUST_BE_TAG) != 0),
      must_be_class((options & IDL_MUST_BE_CLASS) != 0),
      is_linkage_or_friend_lookup(
                    (options & (IDL_LINKAGE_LOOKUP | IDL_FRIEND_LOOKUP)) != 0),
      is_linkage_lookup((options & IDL_LINKAGE_LOOKUP) != 0),
      is_declarator_lookup((options & IDL_IS_DECLARATOR) != 0),
      is_friend_lookup((options & IDL_FRIEND_LOOKUP) != 0),
      direct_namespace_members_only(
                           (options & IDL_DIRECT_NAMESPACE_MEMBERS_ONLY) != 0),
      check_decl_seq((options & IDL_SUPPRESS_DECL_SEQ_CHECK) == 0 &&
                     !is_linkage_or_friend_lookup),
      decl_seq_number(get_effective_decl_seq())
  {}

  inline a_boolean accepts(a_namespace_ptr ns_ptr,
                           a_symbol_ptr    sym,
                           a_symbol_ptr    fund_sym) const;

  const a_boolean must_be_class_or_namespace;
  const a_boolean must_be_tag;
  const a_boolean must_be_class;
  const a_boolean is_linkage_or_friend_lookup;
  const a_boolean is_linkage_lookup;
  const a_boolean is_declarator_lookup;
  const a_boolean is_friend_lookup;
  const a_boolean direct_namespace_members_only;
  const a_boolean check_decl_seq;
  const a_decl_sequence_number
                  decl_seq_number;
};  /* a_namespace_lookup_options_set */


inline a_boolean a_namespace_lookup_options_set::accepts(
                                                      a_namespace_ptr ns_ptr,
                                                      a_symbol_ptr    sym,
                                                      a_symbol_ptr    fund_sym)
                                                                          const
/*
Return TRUE if sym and its accompanying fundamental symbol are acceptable
symbols in the namespace ns_ptr given the current lookup options.
*/
{
  a_boolean result = TRUE;

  if (fund_sym->is_invisible && !is_linkage_or_friend_lookup &&
      !is_declarator_lookup) {
    result = FALSE;
  } else if (sym->is_class_member) {
    result = FALSE;
  } else if (sym_parent_namespace_or_null((sym)) != ns_ptr) {
    /* Note that same_entities must not be used for this test. */
    result = FALSE;
  } else if (must_be_class_or_namespace &&
             !symbol_may_precede_qualifier(fund_sym)) {
    result = FALSE;
  } else if (must_be_class && !is_class_or_class_proxy_symbol(fund_sym)) {
    result = FALSE;
  } else if (must_be_tag &&
             !is_tag_or_tag_proxy_symbol(fund_sym, is_friend_lookup)) {
    result = FALSE;
  } else if (check_decl_seq && !(decl_seq_number == NO_DECL_SEQUENCE_NUMBER ||
                                 decl_seq_number >= (sym)->decl_seq)) {
    result = FALSE;
  }  /* if */
  return result;
}  /* accepts */

}  /* namespace */

static a_symbol_ptr lookup_in_namespace(
		a_symbol_locator		*locator,
		a_namespace_ptr			ns_ptr,
		an_id_lookup_options_set	options,
		a_namespace_ptr			orig_ns_ptr,
		a_symbol_ptr			*synth_sym,
		a_boolean			inline_namespace_lookup,
		a_boolean			*any_errors)
/*
Look up the identifier indicated by *locator in the namespace indicated by
ns_ptr, and return a pointer to the symbol found, or NULL if
the symbol is not found.  ns_ptr must refer to an actual namespace and not
a namespace alias.  options indicates a set of special options,
as a bit set.

This routine is called by namespace_qualified_id_lookup, and then calls
itself recursively to look in the namespaces of any using directives
present in the namespace.  A flag is set in the namespace symbol
supplement when each namespace is searched.  This flag is checked
to make sure that no namespace is searched more than once.

orig_ns_ptr is a pointer to the namespace specified in the call to
namespace_qualified_id_lookup.  inline_namespace_lookup is TRUE if
a using-directive lookup done from this namespace should only consider
inline namespaces.
*/
{
  a_symbol_ptr	sym = NULL;
  a_symbol_ptr	tag_symbol = NULL;
  a_symbol_ptr	type_tag_symbol = NULL;
  a_symbol_ptr	namespace_symbol = NULL;
  a_namespace_lookup_options_set
                ns_lookup_opts(options);
  a_namespace_symbol_supplement_ptr
		nssp;

  db_enter(4, "lookup_in_namespace");
  check_assertion(ns_ptr != NULL);
  /* Get the declaration sequence number to be used for this lookup. */
  nssp = symbol_supplement_for_namespace(ns_ptr);
  load_lazy_symbols_if_needed(ns_ptr->variant.assoc_scope, locator);
  /* Search for a symbol in the lookup table for the namespace. */
  for (a_module_ptr mod : curr_lookup_modules()) {
    for (sym = find_symbol_list_in_table(
                  curr_lookup_table(&nssp->pointers_block, mod, sck_namespace),
                  locator->symbol_header);
         sym != NULL;
         sym = sym->next_in_lookup_table) {
      a_symbol_ptr fund_sym = fundamental_symbol_of(sym);

      if (ns_lookup_opts.accepts(ns_ptr, sym, fund_sym)) {
        /* Found an acceptable symbol. */
        /* When looking for a tag symbol, both tags and typedefs may match the
           "acceptable" test.  The tag should be preferred over the typedef, so
           if we find a typedef we must keep looking.  Similarly, when not
           doing a "must be tag" lookup, both tags and non-tags will match the
           test.  A non-tag should be preferred over the tag, so if we find a
           tag we must keep looking. */
        if (!ns_lookup_opts.must_be_tag) {
          /* A normal lookup. */
          if (is_tag_symbol(fund_sym)) {
            check_assertion_or_expect_error(
                                           tag_symbol == NULL ||
                                           fundamental_symbol_of(tag_symbol) ==
                                                                     fund_sym);
            tag_symbol = sym;
          } else {
            if (is_namespace_symbol(sym) &&
                gnu_namespace_and_class_in_same_scope) {
              /* Some versions of g++ allow a namespace and class with the same
                 name in a scope.  The class name should be preferred.  If we
                 found the namespace keep looking in case we find a class. */
              namespace_symbol = sym;
            } else {
              /* Take the symbol. */
              goto sym_found;
            }  /* if */
          }  /* if */
        } else {
          /* A tag lookup. */
          if (sym->kind == sk_type) {
            check_assertion_or_expect_error(type_tag_symbol == NULL);
            type_tag_symbol = sym;
          } else {
            /* Take the symbol. */
            goto sym_found;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* for */
sym_found:
  if (sym == NULL) {
    /* If a type symbol was found and no other matching tag was present,
       use the type symbol. */
    if (type_tag_symbol != NULL) {
      sym = type_tag_symbol;
    } else if (tag_symbol != NULL) {
      /* If there is a tag symbol saved within the loop, use it. */
      sym = tag_symbol;
    } else if (namespace_symbol != NULL) {
      /* If a namespace symbol was saved, use it. */
      sym = namespace_symbol;
    }  /* if */
  }  /* if */
  if ((!ns_lookup_opts.is_linkage_lookup ||
       (options & IDL_TREAT_AS_TEMPLATE_ID) != 0) &&
      !ns_lookup_opts.direct_namespace_members_only) {
     /* If the symbol was not found in this namespace, look in namespaces
        visible because of an inline namespace.  Skip this process for a
        linkage lookup.  A linkage lookup should only find names that are
        actually defined in a scope.  The template-id exception is made
        because class template declarations use a linkage lookup but should
        do the inline namespace processing. */
    a_symbol_ptr	new_sym;
    new_sym = qualified_using_directive_lookup(
                                  locator, ns_ptr, ns_ptr->variant.assoc_scope,
                                  options, orig_ns_ptr, synth_sym,
                                  any_errors,
                                  /*inline_namespace_only=*/TRUE);
    /* For the inline namespace case, we may have to merge the result of the
       using-directive lookup and the lookup in the current namespace. */
    if (new_sym != NULL) {
      if (sym == NULL) {
        sym = new_sym;
      } else {
        sym = add_symbol_to_lookup_set(new_sym, sym, locator,
                                       /*qualified_lookup=*/TRUE,
                                       orig_ns_ptr, options,
                                       any_errors);
      }  /* if */
    }  /* if */
  }  /* if */
  if (sym == NULL && !ns_lookup_opts.is_linkage_lookup &&
      (!ns_lookup_opts.is_friend_lookup || !clang_mode) &&
      !inline_namespace_lookup &&
      !ns_lookup_opts.direct_namespace_members_only) {
     /* If the symbol was not found in this namespace, look in namespaces
        visible because of using directives.  Skip this process for linkage
        lookups.  A linkage lookup should only find names that are actually
        defined in a scope.  Clang (as of version 19.1) also skips this for
        friend lookups. */
    sym = qualified_using_directive_lookup(
                                  locator, ns_ptr, ns_ptr->variant.assoc_scope,
                                  options, orig_ns_ptr, synth_sym,
                                  any_errors,
                                  /*inline_namespace_only=*/FALSE);
  }  /* if */
  db_exit();
  return sym;
}  /* lookup_in_namespace */


a_symbol_ptr namespace_qualified_id_lookup(a_symbol_locator         *locator,
                                           a_namespace_ptr          ns_ptr,
                                           an_id_lookup_options_set options)
/*
Look up the identifier indicated by *locator in the namespace indicated by
ns_ptr, and return a pointer to the symbol found, or NULL if
the symbol is not found.  ns_ptr must refer to an actual namespace and not
a namespace alias.  options indicates a set of special options,
as a bit set.  For example, if IDL_MUST_BE_CLASS_OR_NAMESPACE is TRUE,
the symbol found must be a class name (or typedef to a class name) or a
namespace.  This routine is used only in C++ mode.
*/
{
  a_symbol_ptr	sym;
  a_symbol_ptr	synth_sym = NULL;
  a_boolean	any_errors = FALSE;

  db_enter(4, "namespace_qualified_id_lookup");
  if ((sym = locator->specific_symbol) != NULL) {
    /* There is an existing specific symbol. */
  } else if (ignore_std_namespace &&
             ns_ptr == symbol_for_namespace_std->variant.namespace_info.ptr) {
    /* When using the g++ compatibility feature that treats "std" as
       a synonym for the global namespace, do the lookup in the file scope. */
    sym = file_scope_id_lookup(il_header.primary_scope, locator, options);
  } else {
    /* Search for a symbol in the right scope. */
    sym = lookup_in_namespace(locator, ns_ptr, options, ns_ptr, &synth_sym,
                              /*inline_namespace_lookup=*/FALSE, &any_errors);
    locator->specific_symbol = sym;
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (sym == NULL && microsoft_mode && ms_permissive) {
    a_scope_stack_entry_ptr	ssep = &scope_stack_top();
    if (ssep->in_decltype_context && scope_is(ssep, sck_func_prototype) &&
        is_template_dependent_context()) {
      /* In certain template dependent contexts, the Microsoft compiler
         accepts constructs like "decltype(N::x)" when x has not yet been
         declared in namespace N.  Create the member in a proxy class
         associated with the namespace we are looking in. */
      a_type_ptr	proxy_class;
      proxy_class = proxy_class_for_namespace(ns_ptr);
      if ((options & IDL_TENTATIVE_TEMPLATE_LOOKUP) != 0) {
        /* The tentative template lookup flag will be TRUE if the name
           was preceded by "template" or followed (in some cases) by "<".
           In such cases, create the nonreal member as a template. */
        options |= IDL_TREAT_AS_TEMPLATE_ID;
      }  /* if */
      sym = create_proxy_or_nonreal_class_member(proxy_class, options,
                                                 locator);
      locator->specific_symbol = sym;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* If the symbol is a projection symbol, reduce it to the fundamental
     symbol.  The specific_symbol in the locator stays pointing to the
     projection symbol. */
  if (sym != NULL) reduce_projection_symbol_to_fundamental_symbol(sym);
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    fprintf(f_debug, "namespace_qualified_id_lookup: id = %s, %s\n",
                     locator->symbol_header->identifier,
                     (sym != NULL) ? "found" : "not found");
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  db_exit();
  return sym;
}  /* namespace_qualified_id_lookup */

namespace {

/*
An internal class for representing flags for a file scope id lookup, and
testing acceptance of symbols.
*/
struct a_file_scope_id_lookup_options_set {
  a_file_scope_id_lookup_options_set(an_id_lookup_options_set options)
    : must_be_class_or_namespace(
                              (options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0),
      must_be_tag((options & IDL_MUST_BE_TAG) != 0),
      must_be_class((options & IDL_MUST_BE_CLASS) != 0),
      is_friend_lookup((options & IDL_FRIEND_LOOKUP) != 0),
      is_linkage_lookup((options & IDL_LINKAGE_LOOKUP) != 0),
      is_direct_namespace_members_only(
                           (options & IDL_DIRECT_NAMESPACE_MEMBERS_ONLY) != 0),
      check_decl_seq((options & IDL_SUPPRESS_DECL_SEQ_CHECK) == 0 &&
                     !is_linkage_lookup && !is_friend_lookup),
      are_proxy_members_template_ids(
                                    (options & IDL_TREAT_AS_TEMPLATE_ID) != 0),
      decl_seq_number(get_effective_decl_seq())
  {}

  inline a_boolean accepts(a_scope_ptr  file_scope_ptr,
                           a_symbol_ptr sym,
                           a_symbol_ptr fund_sym) const;

  const a_boolean must_be_class_or_namespace;
  const a_boolean must_be_tag;
  const a_boolean must_be_class;
  const a_boolean is_friend_lookup;
  const a_boolean is_linkage_lookup;
  const a_boolean is_direct_namespace_members_only;
  const a_boolean check_decl_seq;
  const a_boolean are_proxy_members_template_ids;
  const a_decl_sequence_number
                  decl_seq_number;
};  /* a_file_scope_id_lookup_options_set */


inline a_boolean a_file_scope_id_lookup_options_set::accepts(
                                                a_scope_ptr  file_scope_ptr,
                                                a_symbol_ptr sym,
                                                a_symbol_ptr fund_sym) const
/*
Return TRUE if sym and its accompanying fundamental symbol are acceptable
symbols for the given file scope and the current lookup options.
*/
{
  a_boolean result = TRUE;

  if (fund_sym->is_invisible && !(is_linkage_lookup || is_friend_lookup)) {
    result = FALSE;
  } else if (sym->decl_scope != file_scope_ptr->number) {
    result = FALSE;
  } else if (name_space_for_symbol_kind[(int)sym->kind] != nsk_other) {
    /* The name space test is needed when searching the file scope so that
       macro symbols are not found. */
    result = FALSE;
  } else if (must_be_class_or_namespace &&
             !symbol_may_precede_qualifier(fund_sym)) {
    /* symbol_may_precede_qualifier checks for a symbol that is a class, class
       template, namespace, or template type parameter. */
    result = FALSE;
  } else if (must_be_class && !is_class_or_class_proxy_symbol(fund_sym)) {
    result = FALSE;
  } else if (must_be_tag &&
             !is_tag_or_cplusplus_type_symbol(fund_sym, is_friend_lookup)) {
    result = FALSE;
  } else if (check_decl_seq && !(decl_seq_number == NO_DECL_SEQUENCE_NUMBER ||
                                 decl_seq_number >= (sym)->decl_seq)) {
    result = FALSE;
  }
  return result;
}  /* accepts */

}  /* namespace */

a_symbol_ptr file_scope_id_lookup(
			a_scope_ptr			file_scope_to_use,
			a_symbol_locator		*locator,
			an_id_lookup_options_set	options)
/*
Look up the identifier indicated by *locator in the file scope specified
by file_scope_to_use, and return a pointer to the symbol found, or NULL if
the symbol is not found (there can be multiple file scopes when multiple
translation units are being processed).

options indicates a set of special options, as a bit set.  For example,
if IDL_MUST_BE_CLASS_OR_NAMESPACE is TRUE, the symbol found must be a class
name (or a typedef to a class name) or a namespace.  Only symbols in the
nsk_other name space are considered.  This routine is used for the unary
"::" qualifier and other cases in which it is necessary to determine
whether a given symbol exists in the file scope.  It is used in both
C and C++ (in C it is used for identifier linkage).  If the name is not
found in the file scope, and this is not a linkage lookup, the lookup
will also look in any namespaces used in using directives in the
file scope.
*/
{
  a_symbol_ptr  sym;
  a_symbol_ptr  synth_sym = NULL;
  a_boolean     any_errors = FALSE;
  a_file_scope_id_lookup_options_set
                file_scope_lookup_opts(options);

  db_enter(4, "file_scope_id_lookup");
  if ((sym = locator->specific_symbol) != NULL) {
    /* There is an existing specific symbol. */
  } else {
    /* Search for a symbol in the file scope.  First look on the active
       list. */
    a_symbol_ptr	type_tag_symbol = NULL;
    load_lazy_symbols_if_needed(file_scope_to_use, locator);
    /*lint --e{446,850} sym modified in loop (LINTBUG) */
    for (sym = symbol_list_from_locator(*locator);
         sym != NULL;
         sym = sym->next) {
      /* See if the symbol is acceptable (e.g., it's a class if it
         must be one). */
      a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
      /* Exit the loop if we found a type tag symbol and the new symbol
         to check is not from the same scope. */
      if (type_tag_symbol != NULL &&
          type_tag_symbol->decl_scope != sym->decl_scope) {
        sym = NULL;
        break;
      }  /* if */
      /* Ignore symbols not visible to the current module. */
      if (!is_symbol_currently_lookup_visible(sym)) {
        continue;
      }  /* if */
      if (file_scope_lookup_opts.accepts(file_scope_to_use, sym, fund_sym)) {
        /* We found a matching symbol.  If this is a type symbol found
           by a must-be-tag lookup, keep searching for a "real" tag in
           the same scope. */
        if (file_scope_lookup_opts.must_be_tag &&
            sym->kind == (a_symbol_kind)sk_type) {
          check_assertion_or_expect_error(type_tag_symbol == NULL);
          type_tag_symbol = sym;
        } else {
          /* Use this symbol. */
          break;
        }  /* if */
      }  /* if */
    }  /* for */
    /* If a type symbol was found and no other matching tag was present,
       use the type symbol. */
    if (sym == NULL && type_tag_symbol != NULL) sym = type_tag_symbol;
    /* If no symbol was found, continue the search in the lookup table. */
    if (sym == NULL) {
      a_symbol_ptr   tag_symbol = NULL;
      a_scope_number scope_number = file_scope_to_use->number;
      a_translation_unit_ptr
                     trans_unit = get_trans_unit_for_scope(scope_number);

      type_tag_symbol = NULL;
      for (a_module_ptr mod : curr_lookup_modules()) {
        for (sym = find_symbol_list_in_table(
                                      curr_lookup_table(
                                        &trans_unit->file_scope_pointers_block,
                                        mod,
                                        sck_file),
                                      locator->symbol_header);
             sym != NULL;
             sym = sym->next_in_lookup_table) {
          a_symbol_ptr fund_sym = fundamental_symbol_of(sym);

          if (file_scope_lookup_opts.accepts(file_scope_to_use, sym,
                                             fund_sym)) {
            /* Found an acceptable symbol. */
            /* When looking for a tag symbol, both tags and typedefs may match
               the "acceptable" test.  The tag should be preferred over the
               typedef, so if we find a typedef we must keep looking.
               Similarly, when not doing a "must be tag" lookup, both tags and
               non-tags will match the test.  A non-tag should be preferred
               over the tag, so if we find a tag we must keep looking. */
            if (!file_scope_lookup_opts.must_be_tag) {
              /* A normal lookup. */
              if (is_tag_symbol(fund_sym)) {
                check_assertion_or_expect_error(tag_symbol == NULL);
                tag_symbol = sym;
              } else {
                /* Take the symbol. */
                goto found_sym;
              }  /* if */
            } else {
              /* A tag lookup. */
              if (sym->kind == sk_type) {
                check_assertion_or_expect_error(type_tag_symbol == NULL);
                type_tag_symbol = sym;
              } else {
                /* Take the symbol. */
                goto found_sym;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* for */
found_sym:
      if (sym == NULL) {
        /* If a type symbol was found and no other matching tag was present,
           use the type symbol. */
        if (type_tag_symbol != NULL) {
          sym = type_tag_symbol;
        } else if (tag_symbol != NULL) {
          /* If there is a tag symbol saved within the loop, use it. */
          sym = tag_symbol;
        }  /* if */
      }  /* if */
    }  /* if */
    if ((!file_scope_lookup_opts.is_linkage_lookup ||
         file_scope_lookup_opts.are_proxy_members_template_ids) &&
        !file_scope_lookup_opts.is_direct_namespace_members_only) {
       /* If the symbol was not found in this namespace, look in namespaces
          visible because of an inline namespace.  Skip this process for a
          linkage lookup.  A linkage lookup should only find names that are
          actually defined in a scope.  The template-id exception is made
          because class template declarations use a linkage lookup but should
          do the inline namespace processing. */
      a_symbol_ptr	new_sym;
      new_sym = qualified_using_directive_lookup(
                             locator, (a_namespace_ptr)NULL, file_scope_to_use,
                             options, (a_namespace_ptr)NULL, &synth_sym,
                             &any_errors,
                             /*inline_namespace_only=*/TRUE);
      /* For the inline namespace case, we may have to merge the result of the
         using-directive lookup and the lookup in the current namespace. */
      if (new_sym != NULL) {
        if (sym == NULL) {
          sym = new_sym;
        } else {
          sym = add_symbol_to_lookup_set(new_sym, sym, locator,
                                         /*qualified_lookup=*/TRUE,
                                         (a_namespace_ptr)NULL, options,
                                         &any_errors);
        }  /* if */
      }  /* if */
    }  /* if */
    if (sym == NULL && !file_scope_lookup_opts.is_linkage_lookup &&
        (!file_scope_lookup_opts.is_friend_lookup || !clang_mode) &&
        !file_scope_lookup_opts.is_direct_namespace_members_only) {
      /* If the symbol was not found in this namespace, look in namespaces
         visible because of using directives.  Skip this process for linkage
         lookups.  A linkage lookup should only find names that are actually
         defined in a scope.  Clang (as of version 19.1) also skips this for
         friend lookups. */
      sym = qualified_using_directive_lookup(
                             locator, (a_namespace_ptr)NULL, file_scope_to_use,
                             options, (a_namespace_ptr)NULL, &synth_sym,
                             &any_errors,
                             /*inline_namespace_only=*/FALSE);
    }  /* if */
    locator->specific_symbol = sym;
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    fprintf(f_debug, "file_scope_id_lookup: id = %s, %s\n",
                     locator->symbol_header->identifier,
                     (sym != NULL) ? "found" : "not found");
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* If the symbol is a projection symbol, reduce it to the fundamental
     symbol.  The specific_symbol in the locator stays pointing to the
     projection symbol. */
  if (sym != NULL) reduce_projection_symbol_to_fundamental_symbol(sym);
  db_exit();
  return sym;
}  /* file_scope_id_lookup */


a_symbol_ptr opname_member_function_symbol(an_opname_kind kind,
                                           a_type_ptr     class_type)
/*
Return a pointer to the symbol entry for the operator function for the
operator identified by kind in class class_type, or NULL if there is no such
operator.  The symbol may be a projection symbol (that's desirable, because
a projection symbol is needed to check for ambiguity and access).
*/
{
  a_symbol_ptr        sym = NULL;
  a_symbol_header_ptr symhdr;
  a_symbol_locator    locator;

  /* See if there are any functions for this operator. */
  symhdr = opname_symbol_table[kind];
  if (symhdr != NULL) {
    /* Yes.  Look for one in the desired class. */
    make_opname_locator(kind, &locator, &pos_curr_token);
    if (class_qualified_id_lookup(&locator, class_type,
                                  IDL_DO_NOT_ADD_TO_NONREAL_CLASS) != NULL) {
      /* Get the projection symbol if any. */
      sym = locator.specific_symbol;
    }  /* if */
  }  /* if */
  return sym;
}  /* opname_member_function_symbol */


a_symbol_ptr opname_function_symbol(an_opname_kind kind)
/*
Return a pointer to the symbol entry for the operator function for the
operator identified by kind, or NULL if there is no such operator.
Only non-member functions will be found.  Function templates *will*
be found.
*/
{
  a_symbol_ptr        sym = NULL;
  a_symbol_header_ptr symhdr;

  check_assertion(kind == (an_opname_kind)onk_new ||
                  kind == (an_opname_kind)onk_array_new ||
                  kind == (an_opname_kind)onk_delete ||
                  kind == (an_opname_kind)onk_array_delete);
  /* See if there are any functions for this operator. */
  symhdr = opname_symbol_table[kind];
  if (symhdr != NULL) {
    /* Look for one that's visible and a non-member function.  New and delete
       operators can normally only be declared in global namespace.  However,
       in Microsoft mode and in some GNU modes they can also be declared in
       namespace scope.  In the Microsoft case, the namespace scope
       declarations are found with explicit operator call syntax (which uses
       the usual name lookup routines), but not with new-expression or delete-
       expression syntax (which uses this routine).  In GNU modes with
       gnu_version < 40000 namespaces are considered even for new-expressions
       (and this routine is not called in those cases), but not for delete-
       expressions. */
    /* The file scope symbols start on the active list, but will be on the
       inactive list when the file scope is reactivated for the purpose of
       generating instantiations. */
    sym = (scope_stack[DEPTH_OF_FILE_SCOPE].is_reactivation) ?
                                    symhdr->inactive_symbols : symhdr->symbol;
    for (; sym != NULL; sym = sym->next) {
      /* The file_scope_number test excludes symbols from other translation
         units (and from other namespaces in Microsoft and GNU modes). */
      if (!sym->is_class_member &&
          sym->decl_scope == file_scope_number &&
          (is_function_symbol(sym) ||
           sym->kind == (a_symbol_kind)sk_function_template)) {
        /* A non-member function or function template. */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return sym;
}  /* opname_function_symbol */


a_boolean any_opname_function_symbol(an_opname_kind kind)
/*
Return TRUE if there is at least one declared operator function that
overloads the operator identified by kind, or FALSE if there is no such
function.
*/
{
  a_boolean           result;
  a_symbol_header_ptr symhdr = opname_symbol_table[kind];

  result = (symhdr != NULL &&
            (symhdr->inactive_symbols != NULL ||
             symhdr->symbol != NULL));
  return result;
}  /* any_opname_function_symbol */


void add_to_arg_dependent_lookup_list(a_type_ptr		arg_type,
				      a_type_list_entry_ptr	*type_list)
/*
Add the type specified by arg_type to the list of types to be used for
argument-dependent lookup specified by type_list.  Return an updated
list pointer in type_list.  *type_list should be NULL on the first call.
*/
{
  a_type_ptr		orig_type = NULL;
  a_type_list_entry_ptr	tlep;

  /* Remove any pointer, references, arrays, or qualifiers on the type. */
  for (; orig_type != arg_type ;) {
    orig_type = arg_type;
    switch (arg_type->kind) {
      case tk_pointer:  /* Includes reference too, and C++/CLI handles. */
        arg_type = type_pointed_to(arg_type);
        break;
      case tk_array:
        arg_type = array_element_type(arg_type);
        break;
      case tk_typeref:
        arg_type = arg_type->variant.typeref.type;
        break;
      default:
        break;
    }  /* switch */
    arg_type = skip_typerefs(arg_type);
  }  /* for */
  /* See if the type is already on the list. */
  for (tlep = *type_list; tlep != NULL; tlep = tlep->next) {
    if (same_entities(tlep->type, arg_type)) break;
  }  /* if */
  if (tlep == NULL) {
    /* The type is not on the list -- add it now. */
    tlep = alloc_type_list_entry();
    tlep->type = arg_type;
    /* Add this to the front of the list. */
    tlep->next = *type_list;
    *type_list = tlep;
    /* If the type is a pointer-to-member or function type, add the
       types of which the type is composed to the list. */
    switch (arg_type->kind) {
      case tk_class:
      case tk_struct:
      case tk_union:
        /* A class type.  Add the types of the template type arguments. */
        if (arg_type->variant.class_struct_union.is_template_class) {
          /* Include the types of any template type arguments. */
          a_template_arg_ptr	tap;
          begin_template_arg_list_traversal_simple(
            arg_type->variant.class_struct_union.extra_info->template_arg_list,
            &tap);
          for (; tap != NULL; advance_to_next_template_arg_simple(&tap)) {
            if (is_type_templ_arg(tap)) {
              /* Note that nontype template arguments do not influence
                 argument dependent lookup.  Template template arguments
                 are handled later. */
              add_to_arg_dependent_lookup_list(tap->variant.type, type_list);
            }  /* if */
          }  /* for */
        }  /* if */
        break;
      case tk_ptr_to_member:
        /* A pointer to member type; add the type of the member, and the
           class type. */
        add_to_arg_dependent_lookup_list(pm_member_type(arg_type), type_list);
        add_to_arg_dependent_lookup_list(pm_class_type(arg_type), type_list);
        break;
      case tk_routine:
        /* A function type; add the types of each parameter, and the return
           type. */
        { a_routine_type_supplement_ptr	rtsp;
          a_param_type_ptr			ptp;
          rtsp = arg_type->variant.routine.extra_info;
          for (ptp = rtsp->param_type_list; ptp != NULL; ptp = ptp->next) {
           add_to_arg_dependent_lookup_list(ptp->type, type_list);
          }  /* for */
          add_to_arg_dependent_lookup_list(
                             arg_type->variant.routine.return_type, type_list);
        }
        break;
      default:
        break;
    }  /* switch */
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("add_to_arg_dependent_lookup_list")) {
    fprintf(f_debug, "add_to_arg_dependent_lookup_list:\n");
    for (tlep = *type_list; tlep != NULL; tlep = tlep->next) {
      fprintf(f_debug, "  ");
      db_type_name(tlep->type);
      fprintf(f_debug, "\n");
    }  /* for */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
}  /* add_to_arg_dependent_lookup_list */


static void add_namespace_to_namespace_list(
			a_namespace_ptr			nsp,
			a_namespace_list_entry_ptr	*namespace_list)
/*
Add "nsp" to the list of namespaces specified by "namespace_list".  Note
that "nsp" can be NULL to represent the global namespace.
*/
{
  a_namespace_list_entry_ptr	nlep;

  /* Look for its namespace on the namespace list. */
  for (nlep = *namespace_list; nlep != NULL; nlep = nlep->next) {
    if (nlep->ptr == nsp) break;
  }  /* for */
  if (nlep == NULL) {
    a_scope_pointers_block_ptr	spbp;
    a_scope_ptr			namespace_scope;
    a_namespace_list_entry_ptr	inline_namespace_nlep;
    /* The namespace was not found.  Add it to the list now. */
    nlep = alloc_namespace_list_entry();
    nlep->ptr = nsp;
    nlep->next = *namespace_list;
    *namespace_list = nlep;
    /* If the namespace contains any inline namespaces (or was named in
       any strong using-directives), add inline namespaces (or named
       namespace(s) in the strong using-directive case) to the namespace
       list. */
    namespace_scope = nsp == NULL ? il_header.primary_scope
                                  : nsp->variant.assoc_scope;
    spbp = get_pointers_block_for_scope(namespace_scope);
    for (inline_namespace_nlep = spbp->inline_namespaces;
         inline_namespace_nlep != NULL;
         inline_namespace_nlep = inline_namespace_nlep->next) {
      add_namespace_to_namespace_list(inline_namespace_nlep->ptr,
                                      namespace_list);
    }  /* for */
    if (nsp != NULL && (nsp->is_inline || nsp->named_in_strong_using)) {
      /* If the namespace is inline, add the enclosing namespace to the
         namespace list.  Note that "nsp" will be NULL for global scope
         namespaces. */
      a_namespace_ptr	parent_nsp;
      parent_nsp = parent_namespace_or_null(nsp);
      add_namespace_to_namespace_list(parent_nsp, namespace_list);
    }  /* if */
  }  /* if */
}  /* add_namespace_to_namespace_list */


static void add_namespace_of_type_to_lookup_list(
			a_type_ptr			type,
			a_namespace_list_entry_ptr	*namespace_list)
/*
Add the namespace in which "type" is defined to the namespace_list.
*/
{
  a_namespace_ptr		nsp = NULL;

  /* Determine the parent namespace. */
  if (type->source_corresp.is_local_to_function) {
    /* For a local type, use the namespace of the enclosing routine. */
    a_routine_ptr  rout;
    rout = enclosing_routine_for_local_type(type);
    if (rout->source_corresp.is_class_member) {
      /* If the routine is a class member, get the namespace of its class. */
      type = parent_class_of(rout);
    } else {
      /* Use the enclosing namespace of the routine. */
      nsp = parent_namespace_or_null(rout);
      type = NULL;
    }  /* if */
  }  /* if */
  /* type will be NULL if the namespace was determined above. */
  if (type != NULL) {
    while (type->source_corresp.is_class_member) {
      type = parent_class_of(type);
    }  /* while */
    /* Note that "nsp" will be NULL for global scope types. */
    nsp = parent_namespace_or_null(type);
  }  /* if */
  add_namespace_to_namespace_list(nsp, namespace_list);
}  /* add_namespace_of_type_to_lookup_list */


static void add_class_to_lookup_lists(
			a_type_ptr			class_type,
			a_namespace_list_entry_ptr	*namespace_list,
			a_type_list_entry_ptr		*class_list)
/*
Add the specified class_type to the class_list and add the namespace in
which it is defined to the namespace_list.
*/
{
  a_type_list_entry_ptr	tlep;

  check_assertion(class_type->kind == (a_type_kind)tk_class ||
                  class_type->kind == (a_type_kind)tk_struct ||
                  class_type->kind == (a_type_kind)tk_union);
  /* Look for this type on the class list. */
  for (tlep = *class_list; tlep != NULL; tlep = tlep->next) {
    if (same_entities(tlep->type, class_type)) break;
  }  /* for */
  if (tlep == NULL) {
    /* It is not on the class list.  Add it to both lists now.  If it is on
       the class list, its namespace will already be on the namespace list
       too. */
    tlep = alloc_type_list_entry();
    tlep->type = class_type;
    /* Add this to the front of the list. */
    tlep->next = *class_list;
    *class_list = tlep;
    add_namespace_of_type_to_lookup_list(class_type, namespace_list);
  }  /* if */
}  /* add_class_to_lookup_lists */


static void add_template_template_arg_to_lookup_lists(
			a_template_arg_ptr		tap,
			a_namespace_list_entry_ptr	*namespace_list,
			a_type_list_entry_ptr		*type_list)
/*
Add the namespaces and classes associated with the given template template
argument to namespace_list and type_list, respectively.
*/
{
  a_template_ptr		templ = tap->variant.templ.ptr;
  a_namespace_ptr		nsp;
  a_source_correspondence	*scp;

  /* Add any parent types to the types list. */
  scp = &templ->source_corresp;
  while (scp->is_class_member) {
    a_type_ptr	parent_type;
    parent_type = scp_parent_class(scp);
    add_class_to_lookup_lists(parent_type, namespace_list, type_list);
    scp = &parent_type->source_corresp;
  }  /* while */
  /* Note that "nsp" will be NULL for global scope types. */
  nsp = scp_parent_namespace_or_null(scp);
  add_namespace_to_namespace_list(nsp, namespace_list);
}  /* add_template_template_arg_to_lookup_lists */


static void determine_assoc_namespaces_and_classes_for_type(
			a_type_ptr			type,
			a_namespace_list_entry_ptr	*namespace_list,
			a_type_list_entry_ptr		*class_list)
/*
Determine the associated classes and namespaces of "type".  Add the
associated namespaces and classes to "namespace_list" and "class_list".
*/
{
  a_class_type_supplement_ptr	ctsp;
  a_base_class_ptr		bcp;
  a_boolean			add_parent = FALSE;

  switch (type->kind) {
    case tk_class:
    case tk_struct:
    case tk_union:
      ctsp = type->variant.class_struct_union.extra_info;
      if (!target_is_32_bit_x86_based() && ctsp->is_va_list_tag) {
        /* The __va_list_tag predeclared class doesn't participate in
           this lookup. */
      } else {
        /* The standard specifies different behavior for unions vs. classes.
           Specifically, the class of which a class is a member is not
           an associated class, and a union is not one of its own associated
           classes.  These seem to be errors in the standard, so this
           implementation treats unions and classes equivalently. */
        /* Add the class itself to the lookup list. */
        add_class_to_lookup_lists(type, namespace_list, class_list);
        /* If this is a template, make sure it is instantiated.  In some modes
           this is not always done. */
        if (gpp_mode && gnu_version < 40500) {
          /* Older versions of g++ do not to instantiate the class type in
             this case. */
        } else if (microsoft_mode &&
                   (microsoft_version < 1500 ||
                    ms_does_not_complete_class_for_candidate_decl())) {
          /* Early Microsoft compilers don't instantiate the class type in this
             case either.  Newer versions do, but not always: The contexts
             where instantiation doesn't happen are not entirely clear, but
             they seem to include decltype rescan contexts. */
        } else {
          complete_class_type_is_needed(type);
        }  /* if */
        /* Add its base classes. */
        for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
          add_class_to_lookup_lists(bcp->type, namespace_list, class_list);
        }  /* for */
        /* The enclosing class (if any) and namespace should be included. */
        add_parent = TRUE;
        /* A class type.  Add the types and namespaces associated with
           any template template arguments. */
        if (type->variant.class_struct_union.is_template_class) {
          /* Include the types of any template type arguments. */
          a_template_arg_ptr	tap;
          begin_template_arg_list_traversal_simple(
                type->variant.class_struct_union.extra_info->template_arg_list,
                &tap);
          for (; tap != NULL; advance_to_next_template_arg_simple(&tap)) {
            if (is_template_templ_arg(tap)) {
              add_template_template_arg_to_lookup_lists(tap, namespace_list,
                                                        class_list);
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
      break;
    case tk_integer:
      /* Enums are represented using a tk_integer. */
      if (type->variant.integer.enum_type) {
        /* The enclosing class (if any) and namespace should be included. */
        add_parent = TRUE;
      }  /* if */
      break;
    case tk_reflection:
      if (symbol_for_namespace_std_meta != NULL) {
        add_namespace_to_namespace_list(symbol_for_namespace_std_meta
                                                 ->variant.namespace_info.ptr,
                                        namespace_list);
      }  /* if */
      break;
    default:
      break;
  }  /* switch */
  if (add_parent) {
    /* If this is a member of another class, add the class of which it is
       a members. */
    if (type->source_corresp.is_class_member) {
      add_class_to_lookup_lists(parent_class_of(type),
                                namespace_list, class_list);
    } else {
      /* Add the namespace in which the type was defined to the list. */
      add_namespace_of_type_to_lookup_list(type, namespace_list);
    }  /* if */
  }  /* if */
}  /* determine_assoc_namespaces_and_classes_for_type */


static void find_friend_functions_for_class(
					a_symbol_locator	*locator,
					a_type_ptr		class_type,
					a_symbol_list_entry_ptr	*symbol_list)
/*
Go through the friend functions list of class_type looking for a symbol
that matches the name specified by "locator".  If a match is found, add
the entry to symbol_list.
*/
{
  a_symbol_ptr			sym;
  a_class_symbol_supplement_ptr	cssp;
  a_symbol_list_entry_ptr	slep;

  cssp = symbol_supplement_for_class(class_type);
  check_assertion(cssp != NULL);
  for (sym = cssp->friend_functions; sym != NULL; sym = sym->next) {
    if (sym->header == locator->symbol_header) break;
  }  /* for */
  if (sym != NULL) {
    /* A match was found.  Add this entry to the symbol list.  We don't check
       for an existing entry because it should not be possible for such an
       entry to exist. */
    slep = alloc_symbol_list_entry();
    slep->symbol = sym;
    slep->next = *symbol_list;
    *symbol_list = slep;
  }  /* if */
}  /* find_friend_functions_for_class */


static void find_functions_for_namespace(
					a_symbol_locator	*locator,
					a_namespace_ptr		nsp,
					a_translation_unit_ptr	tup,
					a_symbol_list_entry_ptr	*symbol_list)
/*
Look for a function in the namespace specified by "nsp" whose name is specified
by "locator".  Note that "nsp" will be NULL for the global scope.  "tup"
is the translation unit of the file scope to be used when "nsp" is NULL.
If a match is found, add the entry to symbol_list.
*/
{
  a_symbol_ptr			sym;
  a_symbol_list_entry_ptr	slep;

  /* Clear the specific symbol so that it does not influence the lookup
     below. */
  clear_specific_symbol(*locator);
  /* Look up the symbol in the specified namespace or in the global scope.
     A "direct namespace members only" lookup is used to prevent other
     namespaces from being searched if the specified namespace includes
     using-directives. */
  if (nsp != NULL) {
    sym = namespace_qualified_id_lookup(locator, nsp,
                                        IDL_DIRECT_NAMESPACE_MEMBERS_ONLY |
                                        IDL_SUPPRESS_DECL_SEQ_CHECK);
  } else {
    sym = file_scope_id_lookup(tup->primary_scope, locator,
                               IDL_DIRECT_NAMESPACE_MEMBERS_ONLY |
                               IDL_SUPPRESS_DECL_SEQ_CHECK);
  }  /* if */
  if (sym != NULL) {
    a_symbol_ptr	fund_sym;
    /* A symbol was found in the namespace.  Make sure it is a function
       symbol. */
    fund_sym = fundamental_symbol_of(sym);
    if (is_function_or_template_symbol(fund_sym)) {
      /* A match was found.  Add this entry to the symbol list.  We don't check
         for an existing entry because it should not be possible for such an
         entry to exist. */
      slep = alloc_symbol_list_entry();
      slep->symbol = sym;
      slep->next = *symbol_list;
      *symbol_list = slep;
    }  /* if */
  }  /* if */
}  /* find_functions_for_namespace */


static a_namespace_list_entry_ptr update_namespace_list_for_trans_unit(
			a_namespace_list_entry_ptr	orig_namespace_list,
			a_translation_unit_ptr		tup)
/*
Given a list of namespaces specified by orig_namespace_list, create a new list
that refers to the corresponding namespace in the translation unit specified
by tup.  If the other translation unit does not have a given namespace,
that entry is excluded from the new list.  Return the new list.
*/
{
  a_namespace_list_entry_ptr	new_list = NULL;
  a_namespace_list_entry_ptr	nlep;

  for (nlep = orig_namespace_list; nlep != NULL; nlep = nlep->next) {
    a_namespace_list_entry_ptr	new_nlep;
    a_namespace_ptr		new_nsp = NULL;
    a_boolean			add_to_list = TRUE;
    if (nlep->ptr == NULL) {
      /* A NULL namespace pointer refers to the file scope.  An entry
         for the file scope should be placed on the new list. */
      new_nsp = NULL;
    } else {
      a_symbol_ptr		orig_sym;
      a_symbol_ptr		new_sym = NULL;
      orig_sym = (a_symbol_ptr)nlep->ptr->source_corresp.assoc_info;
      if (is_unnamed_namespace_symbol(orig_sym)) {
        /* find_corresponding_symbol_in_trans_unit does not work for
           unnamed namespace symbols.  If this is an unnamed namespace
           symbol for the desired translation unit, add it to the new
           list. */
        if (trans_unit_for_symbol(orig_sym) == tup) new_sym = orig_sym;
      } else {
        new_sym = find_corresponding_symbol_in_trans_unit(orig_sym, tup);
      }  /* if */
      /* The new symbol will be NULL if there is no corresponding namespace
         in the other translation unit. */
      if (new_sym == NULL) {
        add_to_list = FALSE;
      } else {
        new_nsp = new_sym->variant.namespace_info.ptr;
      }  /* if */
    }  /* if */
    if (add_to_list) {
      new_nlep = alloc_namespace_list_entry();
      new_nlep->ptr = new_nsp;
      /* Add this to the front of the list. */
      new_nlep->next = new_list;
      new_list = new_nlep;
    }  /* if */
  }  /* for */
  return new_list;
}  /* update_namespace_list_for_trans_unit */


static void argument_dependent_lookup_for_trans_unit(
				a_symbol_locator		*locator,
				a_namespace_list_entry_ptr	namespace_list,
				a_type_list_entry_ptr		class_list,
				a_translation_unit_ptr		trans_unit,
				a_symbol_list_entry_ptr		*symbol_list)
/*
Perform C++ argument-dependent lookup of the name specified by
locator in the translation unit specified by trans_unit.  namespace_list
and class_list are the list of associated namespaces and classes to
be used.  The symbols found are added to symbol_list.
*/
{
  a_type_list_entry_ptr		tlep;
  a_namespace_list_entry_ptr	nlep;

  /* Go through the type list and create the symbol list entries for any
     matching friend declarations. */
  for (tlep = class_list; tlep != NULL; tlep = tlep->next) {
    find_friend_functions_for_class(locator, tlep->type, symbol_list);
  }  /* for */
  /* Go through the namespace list and look for matching functions in each
     of the namespaces. */
  for (nlep = namespace_list; nlep != NULL; nlep = nlep->next) {
    find_functions_for_namespace(locator, nlep->ptr, trans_unit, symbol_list);
  }  /* for */
}  /* argument_dependent_lookup_for_trans_unit */


static void exported_template_argument_dependent_lookup(
			a_symbol_locator		*locator,
			a_namespace_list_entry_ptr	orig_namespace_list,
			a_type_list_entry_ptr		orig_class_list,
			a_symbol_list_entry_ptr		*symbol_list)
/*
Perform C++ argument-dependent lookup on the name specified by locator
in each of the translation units in which dependent names can be found.
Add the symbols for the names found to the list specified by symbol_list.
*/
{
  an_export_trans_unit_stack_entry_ptr ettusep;

  /* Loop through the translation units that are on the translation unit
     stack.  This is used for argument dependent lookup that is done
     during the instantiation of exported templates. */
  for (ettusep = curr_export_translation_unit_stack_entry; ettusep != NULL;
       ettusep = ettusep->next) {
    a_translation_unit_ptr      tup = ettusep->trans_unit;
    /* Update the namespace list to refer to the corresponding entities in
       the new translation unit. */
    a_namespace_list_entry_ptr namespace_list =
                update_namespace_list_for_trans_unit(orig_namespace_list, tup);
    /* We don't need to create a transformed class list because the
       class lookup is not translation unit dependent. */
    a_type_list_entry_ptr      class_list = tup == curr_translation_unit ?
                                                        orig_class_list : NULL;

    argument_dependent_lookup_for_trans_unit(locator, namespace_list,
                                             class_list, tup, symbol_list);
    free_list_of_namespace_list_entries(namespace_list);
  }  /* for */
}  /* exported_template_argument_dependent_lookup */


static void remove_namespace_from_list(
			a_namespace_list_entry_ptr	*namespace_list,
			a_namespace_ptr			nsp)
/*
If there is any entry for the namespace nsp in namespace_list, remove it.
Note that nsp can be NULL if the entry for the file scope should be removed
from the list.
*/
{
  a_namespace_list_entry_ptr	nlep;
  a_namespace_list_entry_ptr	prev_nlep = NULL;
  a_namespace_list_entry_ptr	next_nlep;

  for (nlep = *namespace_list; nlep != NULL; nlep = next_nlep) {
    next_nlep = nlep->next;
    if (nlep->ptr == nsp) {
      /* Unlink the entry from the list. */
      if (prev_nlep == NULL) {
        *namespace_list = next_nlep;
      } else {
        prev_nlep->next = next_nlep;
      }  /* if */
      nlep->next = NULL;
      free_list_of_namespace_list_entries(nlep);
    } else {
      prev_nlep = nlep;
    }  /* if */
  }  /* for */
}  /* remove_namespace_from_list */


static void remove_namespaces_used_in_normal_lookup(
			a_namespace_list_entry_ptr	*namespace_list,
			a_symbol_ptr			normal_sym)
/*
This routine is used in Microsoft mode to remove namespaces from namespace_list
that were searched in the normal lookup.  Such namespaces are not searched by
argument-dependent lookup in some Microsoft modes.  normal_sym is the
symbol found by the normal lookup.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_boolean			done = FALSE;
  a_namespace_ptr		normal_sym_namespace;
  a_symbol_ptr			fund_normal_sym;

  fund_normal_sym = fundamental_symbol_of(normal_sym);
  normal_sym_namespace = sym_parent_namespace_or_null(fund_normal_sym);
  for (ssep = scope_stack_entry_for(depth_scope_stack);
       !done && ssep != NULL; ssep = previous_scope_of(ssep)) {
    a_namespace_ptr			nsp;
    an_active_using_directive_ptr	audp;
    if (ssep->kind == (a_scope_kind)sck_namespace ||
        ssep->kind == (a_scope_kind)sck_namespace_extension) {
      /* For namespace scopes, remove the associated namespace from the
         namespace list. */
      nsp = ssep->assoc_namespace;
      remove_namespace_from_list(namespace_list, nsp);
      /* Terminate the loop when we find the scope in which normal_sym
         was found. */
      if (ssep->il_scope != NULL &&
          ssep->il_scope->number == normal_sym->decl_scope) {
        done = TRUE;
      }  /* if */
    } else if (ssep->kind == (a_scope_kind)sck_file) {
      /* For the file scope, pass in a NULL pointer to remove the namespace
         list entry for the file scope.  We don't need to set "done" here
         as the file scope will always be the last one checked. */
      remove_namespace_from_list(namespace_list, (a_namespace_ptr)NULL);
    } else {
      /* For other scopes, stop if the scope matches that of the normal
         symbol (e.g., block extern or using-declarations). */
      if (ssep->number == normal_sym->decl_scope) done = TRUE;
    }  /* if */
    /* Also remove any namespaces named in using-directives that apply
       here.  Note this is done even if the code above terminates the
       loop because a namespace was found. */
    for (audp = ssep->using_directives_that_apply_here;
         audp != NULL; audp = audp->next_that_applies_at_depth) {
      nsp = audp->namespace_supplement->symbol->variant.namespace_info.ptr;
      /* Don't remove a namespace if the using-directive appeared after
         the point where the normal symbol was found. */
      if (normal_sym->decl_seq > audp->effective_decl_seq) {
        remove_namespace_from_list(namespace_list, nsp);
      }  /* if */
      if (nsp == normal_sym_namespace) {
        done = TRUE;
      }  /* if */
    }  /* for */
  }  /* for */
}  /* remove_namespaces_used_in_normal_lookup */


void add_templ_arg_list_to_lookup_lists(
			a_template_arg_ptr		templ_args,
			a_type_list_entry_ptr		*type_list,
			a_namespace_list_entry_ptr	*namespace_list,
			a_type_list_entry_ptr		*class_list)
/*
For every argument in templ_args:
  - if it is a type argument, add that type to type_list, the list of types
    from which associated namespaces and classes will be collected later on,
  - if it is a template template argument, add its associated namespace and
    classes (if any) to namespace_list and class_list, respectively.
*/
{
  a_template_arg_ptr  tap;

  begin_template_arg_list_traversal_simple(templ_args, &tap);
  for (; tap != NULL; advance_to_next_template_arg_simple(&tap)) {
    if (is_type_templ_arg(tap)) {
      add_to_arg_dependent_lookup_list(tap->variant.type, type_list);
    } else if (is_template_templ_arg(tap)) {
      add_template_template_arg_to_lookup_lists(tap, namespace_list,
                                                class_list);
    }  /* if */
  }  /* for */
}  /* add_templ_arg_list_to_lookup_lists */


a_symbol_list_entry_ptr argument_dependent_lookup(
                      a_symbol_ptr			normal_sym,
                      a_symbol_locator			*locator,
                      a_type_list_entry_ptr		*type_list,
                      a_namespace_list_entry_ptr	*p_namespace_list,
                      a_type_list_entry_ptr		*p_class_list,
                      a_boolean				include_std_namespace)
/*
Perform C++ argument-dependent lookup as specified in [basic.lookup.argdep] of
the C++ standard (as of N4868).  normal_sym is the result of a normal lookup of
the function name in the context of the call and may be NULL.  type_list is a
list of argument types to be used to produce a list of associated classes
(p_class_list) and namespaces (p_namespace_list) from which candidate functions
should be considered.  p_class_list and p_namespace_list may be pre-populated
by the caller in some cases (specifically, when an argument to the call is a
template-id naming an overload set, p_class_list and p_namespace_list will list
the classes and namespaces associated with the template argument list).
locator is the symbol locator associated with the name that is being looked up.
When include_std_namespace is TRUE, the std namespace is included in the lookup
as an associated namespace (e.g., for range-based-for).

normal_sym, if not NULL, is included in the symbol list that is returned
and must be the first entry on the list.

This routine builds a list of symbol list entries.  Each entry points to
a sk_routine, sk_overloaded_function, or sk_namespace_projection symbol.
The same function may be pointed to directly and/or indirectly by
any number of these symbols, so the caller is responsible for ignoring
duplicate entries.

The lists pointed to by *type_list, *class_list, and *namespace_list are freed
by this routine, and *type_list, *class_list, and *namespace_list are set to
NULL.
*/
{
  a_symbol_list_entry_ptr	slep;
  a_symbol_list_entry_ptr	symbol_list = NULL;
  a_type_list_entry_ptr		tlep;
  a_type_list_entry_ptr		class_list = *p_class_list;
  a_namespace_list_entry_ptr	namespace_list = *p_namespace_list;
  
  /* Build a list of namespaces and classes to be included in the search. */
  for (tlep = *type_list; tlep != NULL; tlep = tlep->next) {
    determine_assoc_namespaces_and_classes_for_type(
                                     tlep->type, &namespace_list,
                                     &class_list);
  }  /* for */
  if (include_std_namespace) {
    check_assertion(symbol_for_namespace_std != NULL);
    add_namespace_to_namespace_list(
                          symbol_for_namespace_std->variant.namespace_info.ptr,
                          &namespace_list);
  }  /* if */
  if (microsoft_mode && !do_dependent_name_processing &&
      microsoft_version >= 1310 && normal_sym != NULL) {
    /* The Microsoft compiler does not do an ADL lookup in namespaces that
       were searched by the normal lookup.  If dependent name processing
       has been requested in Microsoft mode, don't remove these namespaces
       as names that appear after the point of definition of the template
       will not be visible, so can only be found by ADL. */
    remove_namespaces_used_in_normal_lookup(&namespace_list, normal_sym);
  }  /* if */
  if (in_exported_template_instantiation()) {
    /* We are doing the instantiation of an exported template.  Names
       from other translation units must be considered. */
    exported_template_argument_dependent_lookup(locator, namespace_list,
                                                class_list, &symbol_list);
  } else {
    /* This is either not a template instantiation, or is an instantiation
       that does not involve an exported template.  Consider only the
       current translation unit. */
    argument_dependent_lookup_for_trans_unit(locator, namespace_list,
                                            class_list, curr_translation_unit,
					    &symbol_list);
  }  /* if */
  /* Add the specified normal symbol to the list of symbols found. */
  if (normal_sym != NULL) {
    slep = alloc_symbol_list_entry();
    slep->symbol = normal_sym;
    slep->next = symbol_list;
    symbol_list = slep;
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_flag_is_set("argument_dependent_lookup")) {
    int	i;
    fprintf(f_debug, "argument_dependent_lookup:\n");
    for (i = 0, slep = symbol_list; slep != NULL; slep = slep->next, i++) {
      a_boolean		is_list;
      a_symbol_ptr	sym = slep->symbol;
      fprintf(f_debug, "  symbol list %d:", i);
      db_symbol(sym, "", 2);
      is_list = (sym->kind == (a_symbol_kind)sk_overloaded_function);
      if (is_list) sym = sym->variant.overloaded_function.symbols;
      for (; sym != NULL; sym = (is_list ? sym->next : NULL)) {
        a_symbol_ptr	fund_sym;
        if (is_list) db_symbol(sym, "", 4);
        fund_sym = fundamental_symbol_of(sym);
        if (sym != fund_sym) db_symbol(fund_sym, "  fund_sym:", 4);
        if (secondary_translation_unit_seen()) {
          fprintf(f_debug, "   canonical ptr: %p\n",
                  (a_void_ptr)db_canonical_ptr_for_symbol(fund_sym));
        }  /* if */
        if (is_list) fprintf(f_debug, "\n");
      }  /* for */
    }  /* for */
    for (i = 0, tlep = class_list; tlep != NULL; tlep = tlep->next, i++) {
      fprintf(f_debug, "class list %d: ", i);
      db_type_name(tlep->type);
      fprintf(f_debug, "\n");
    }  /* for */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* Free the lists used to create the symbol list. */
  free_list_of_type_list_entries(*type_list);
  *type_list = NULL;
  free_list_of_namespace_list_entries(namespace_list);
  *p_namespace_list = NULL;
  free_list_of_type_list_entries(class_list);
  *p_class_list = NULL;
  /* Make sure that normal_sym is pointed to by the first entry on the
     returned list. */
  check_assertion(normal_sym == NULL || symbol_list->symbol == normal_sym);
  return symbol_list;
}  /* argument_dependent_lookup */


a_decl_sequence_number f_get_effective_decl_seq(void)
/*
Return the declaration sequence number to be used for lookups in the
current scope.
*/
{
  /* The macro that calls this function is responsible for making sure that
     we are inside a template instantiation scope. */
  /* Get the declaration sequence number that indicates which declarations
     should be visible at the point of definition of the template. */
  a_scope_stack_entry_ptr	ssep;
  a_decl_sequence_number	decl_seq_number;

  ssep = &scope_stack[depth_innermost_instantiation_scope];
  check_assertion(ssep->template_decl_info != NULL);
  if (in_code_from_module()) {
    /* A module is being loaded; disable this check for this entity. */
    decl_seq_number = NO_DECL_SEQUENCE_NUMBER;
  } else if (ssep->is_rescan && ssep->template_sym != NULL) {
    /* When rescanning a template X, only declarations prior to X are
       visible. */
    decl_seq_number = ssep->template_sym->decl_seq-1;
  } else {
    decl_seq_number = ssep->template_decl_info->decl_seq;
  }  /* if */
  if (decl_seq_number != NO_DECL_SEQUENCE_NUMBER && ssep->is_generic_lambda &&
      ssep->assoc_routine != NULL && ssep->assoc_routine->is_lambda_body &&
      !generic_lambda_is_in_specialized_routine(ssep->assoc_routine)) {
    /* The innermost instantiation is the call operator of a generic lambda.
       The call operator's own decl_seq is not always the right one for lookups
       inside the lambda body: in deferred prototype instantiation contexts the
       call operator's decl_seq can be assigned later than the lexically
       enclosing template's, so the visibility filter would treat declarations
       that appear after the lambda as if they were visible.  When the
       enclosing real template instantiation has a smaller decl_seq, use that
       value instead.  Do not apply this adjustment when the lambda is inside
       an explicitly-specialized function. */
    a_scope_stack_entry_ptr  encl = previous_scope_of(ssep);
    while (encl != NULL) {
      if (scope_is(encl, sck_template_instantiation) &&
          !encl->in_nonreal_instantiation) {
        if (encl->template_decl_info != NULL &&
            encl->template_decl_info->decl_seq != NO_DECL_SEQUENCE_NUMBER &&
            encl->template_decl_info->decl_seq < decl_seq_number) {
          decl_seq_number = encl->template_decl_info->decl_seq;
        }  /* if */
        break;
      }  /* if */
      encl = previous_scope_of(encl);
    }  /* while */
  }  /* if */
  return decl_seq_number;
}  /* f_get_effective_decl_seq */


void lookup_one_time_init(void)
/*
Do one-time initialization of variables related to name lookup.
*/
{
  init_cleared_lookup_state();
}  /* lookup_one_time_init */


void lookup_init(void)
/*
Initialize static variables related to name lookup.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
}  /* lookup_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE


