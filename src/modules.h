/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

modules.h -- Declarations related to module handling.

*/

/* Avoid including these declarations more than once: */
#ifndef MODULES_H
#define MODULES_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if !STANDALONE_UTILITY_PROGRAM

/*
The following structure is associated with every entity in a module file that
has been loaded into the IL (or, if invalid is TRUE, failed to load into the
IL).

The module entity serves as a central point to accumulate module related
information about the module entity across different BMIs.
*/
struct a_module_entity {
  a_module_ptr  module_info;
                        /* The module corresponding to the module-unit this
                           entity is in the purview of; if NULL, the entity is
                           in the purview of the global module. */
  a_symbol_header_ptr
                sym_header;
                        /* The symbol header this entity is named by (if
                           any); otherwise, NULL. */
  a_scope_ptr   scope;  /* The scope in which this entity is defined. */
  a_tagged_pointer
                entity; /* The IL entity that corresponds to the module
                           entity. */
  Small_dyn_array<a_module_entry_locator, 1>
                locators;
                        /* The locations within BMIs where details about this
                           module entity can be recovered.  There is always at
                           least one locator for a module entity.  New locators
                           should be added via
                           update_entity_from_new_locator. */
  uint16_t      primary_locator_idx;
                        /* The index of the primary locator (i.e., the locator
                           with the most detailed information).  In most cases,
                           this corresponds to the locator that contains the
                           definition of the entity (if any). */
  a_bit_field   imminent:1;
                        /* This flag is typically set to TRUE when processing
                           to form or find the associated IL entity starts.
                           This is used to avoid unbounded recursion.

                           The flag is sometimes unset if an entity might need
                           to be reexamined (e.g., this is a namespace that
                           might be expanded by additional module imports).  In
                           that case, the num_locators_processed value is
                           typically used to track what work still needs to be
                           completed. */
  a_bit_field   def_imminent:1;
                        /* This flag is set when the definition of the
                           associated entity has started being loaded. */
  a_bit_field   uses_bound_token:1;
                        /* This flag is set when the IL entity is being
                           declared or defined via a reparse of a broader
                           cache.  This indicates less strict requirements may
                           be placed on recursive definition processing, as the
                           entity could have a severely "broken" token
                           representation or the entity may be invalid but not
                           yet marked as such (as the parse is still
                           ongoing). */
  a_bit_field   invalid:1;
                        /* TRUE if the associated entity cannot be constructed
                           from the module for any reason. */
  a_bit_field   global_module:1;
                        /* TRUE if this is an entity owned by the "global
                           module". */
  a_bit_field   non_exported:1;
                        /* TRUE if this is an entity that's reachable but not
                           visible (and thus was not exported from the imported
                           module). */
  a_module_entity(a_module_ptr module_info_val);
};  /* a_module_entity */

#if DEBUG
/* GCOVR_EXCL_START */

EXTERN_THREAD unsigned long
                num_module_decls_attempted;
                        /* The number of declarations that process_ifc_decl
                           attempted to process. */

EXTERN_THREAD unsigned long
                num_module_decls_failed;
                        /* The number of declarations that process_ifc_decl
                           attempted to process, but marked invalid. */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

inline a_boolean is_module_entity_globally_visible(a_module_entity_ptr mep)
/*
Return TRUE if the given module entity is globally visible to lookup.  For the
symbol to be globally visible to lookup, it must either be part of the global
module or it must have been exported from a module (that has been imported).
Otherwise, return FALSE.

Note that this function does not account for class membership (as class members
do not currently have individual module entities).  To account for class
membership, prefer using is_symbol_globally_visible with the appropriate class
member symbol.
*/
{
  a_boolean result = FALSE;

  if (mep == NULL || mep->global_module) {
    /* This declaration belongs to the global module, so it's definitely
       lookup visible. */
    result = TRUE;
  } else if (!mep->non_exported) {
    /* This declaration was exported from an imported module. */
    result = TRUE;
  }  /* if */
  return result;
}  /* is_module_entity_globally_visible */

#endif /* !STANDALONE_UTILITY_PROGRAM */

inline a_module_ptr skip_module_partitions(a_module_ptr mod)
/*
Return the module unit skipping over any module partition unit.
*/
{
  a_module_ptr result = mod;

  while (result != NULL && result->kind == mk_unit_partition) {
    result = result->variant.unit_partition.unit;
  }  /* while */
  return result;
}  /* skip_module_partitions */

#if !STANDALONE_UTILITY_PROGRAM

inline a_module *lookup_module_for_mep(a_module_entity_ptr mep)
/*
Return a pointer to the module used for lookup of the given module entity
pointer.  Return NULL if there isn't one.
*/
{
  a_module_ptr result;

  if (is_module_entity_globally_visible(mep)) {
    result = NULL;
  } else {
    result = mep->module_info;
  }  /* if */
  return result;
}  /* lookup_module_for_mep */


EXTERN_THREAD a_symbol_ptr
                curr_module_sym;
                        /* If in a module unit, the current module symbol. */

/*
Entry used to maintain a stack of module contexts.  The top of the stack
is the "current" module for declarations.
*/
typedef struct a_module_entity_stack_entry *a_module_entity_stack_entry_ptr;
struct a_module_entity_stack_entry {
  void invalidate()
    { check_assertion(this->mep != NULL); this->mep->invalid = TRUE; }

  a_module_entity_ptr
                mep;    /* The current module entity pointer.  If mep is NULL,
                           this represents a return to the translation unit
                           module or the global module. */
  a_source_position
                saved_error_position;
                        /* The value of error_position prior to this module
                           entity being pushed to the stack. */
};  /* a_module_entity_stack_entry */

namespace detail {

/*
The following specializations provide Is_trivially_copyable and
Is_trivially_destructible support for a_module_entity_stack_entry.
*/

template<>
struct Is_trivially_copyable_edg_impl<a_module_entity_stack_entry> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<a_module_entity_stack_entry> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

}  /* namespace detail */

using a_module_entity_stack = Dyn_array<a_module_entity_stack_entry>;
                        /* The type used to represent the stack of module
                           contexts. */

EXTERN_THREAD a_module_entity_stack
                *module_entity_stack;
                        /* A dynamic array of the active module entities. */

using a_module_entity_depth = ptrdiff_t;
                        /* The type used to represent the module entity scope
                           depth. */

inline a_module_entity_depth module_entity_stack_depth()
/*
Return the depth of the module entity stack.
*/
{
  a_module_entity_depth result = NO_SCOPE_DEPTH;

  if (module_entity_stack != NULL && !module_entity_stack->is_empty()) {
    result = (a_module_entity_depth)(module_entity_stack->length() - 1);
  }  /* if */
  return result;
}  /* module_entity_stack_depth */


extern void push_module_entity_state(a_module_entity_ptr mep);

extern void pop_module_entity_state();


/*
A class used to represent an element on the module entity state stack.  This is
an RAII object that automatically manages the calls to
push/pop_module_entity_state for the module entity given during construction.
*/
struct a_module_entity_stack_state {
  a_module_entity_stack_state(a_module_entity_ptr mep_val)
    { push_module_entity_state(mep_val); }
  ~a_module_entity_stack_state()
    { pop_module_entity_state(); }
};  /* a_module_entity_stack_state */


inline a_module_entity_ptr curr_module_entity()
/*
Return a pointer to the module entity at the top of the module entity stack, or
NULL if there is no current module entity.
*/
{
  a_module_entity_ptr mep = NULL;

  if (module_entity_stack != NULL && module_entity_stack->length() > 0) {
    a_module_entity_stack_entry_ptr mesep = &module_entity_stack->back_elem();

    mep = mesep->mep;
  }  /* if */
  return mep;
}  /* curr_module_entity */


inline a_module_ptr curr_lookup_module()
/*
Return the module that should be used for additional lookup (outside of
the global module and any exported entities from imported modules).
*/
{
  a_module_ptr        result = NULL;
  a_module_entity_ptr mep = curr_module_entity();

  if (mep != NULL && mep->module_info != NULL &&
      mep->module_info->kind != mk_header_unit) {
    result = mep->module_info;
  } else if (trans_unit_module != NULL) {
    result = trans_unit_module;
  }   /* if */
  return result;
}  /* curr_lookup_module */


using a_module_lookup_array = Small_dyn_array<a_module_ptr, 2>;
                        /* An array used to represent the array of modules
                           to be used for lookup. */


inline a_module_lookup_array curr_lookup_modules()
/*
Return all modules that can be pulled from during lookup.  A NULL value
represents the global module and any exported entities from imported modules.
*/
{
  a_module_lookup_array result;
  a_module_ptr          curr_module = curr_lookup_module();

  result.push_back(NULL);
  if (curr_module != NULL) {
    result.push_back(curr_module);
  }  /* if */
  return result;
}  /* curr_lookup_modules */


EXTERN_THREAD a_boolean
                lazy_symbols_may_be_visible;
                        /* TRUE if symbols (and their definitions) may be
                           "lazily loaded" (i.e., because at least one module
                           has been imported). */

inline a_boolean magic_numbers_match(const a_byte magic[4],
                                     const a_byte expected[4])
/*
Return true if the provided magic numbers match their expected magic numbers.
*/
{
  return magic[0] == expected[0] &&
         magic[1] == expected[1] &&
         magic[2] == expected[2] &&
         magic[3] == expected[3];
}  /* magic_numbers_match */

extern a_boolean check_module_has_interface_dependency(
                                           a_symbol_ptr          module_sym,
                                           a_symbol_ptr          interface_sym,
                                           a_source_position_ptr module_pos);

extern a_module_ptr find_or_create_module(a_symbol_ptr module_sym);

extern a_boolean find_module_file(a_module_ptr  mod,
                                  a_module_kind kind);

extern void import_module_file(a_module_import_decl_ptr midp);

extern void define_names_from_scope(a_scope_ptr     scope,
                                    a_symbol_header *sym_hdr);

struct a_module_entity_scope;
struct a_module_template_parameter;

enum a_module_template_parameter_kind {
  mtpk_non_type, /* A non-type template parameter. */
  mtpk_template, /* A template template parameter. */
  mtpk_type      /* A type template parameter. */
};

using a_module_template_parameter_list =
                                        Dyn_array<a_module_template_parameter>;
                        /* A list of template parameters for use by module
                           entity hashing. */

/*
A lightweight representation of a template parameter used for module entity
hashing.
*/
struct a_module_template_parameter {
  a_module_template_parameter_kind
                kind;   /* The kind of template parameter being represented. */
  union {
    /* When kind == mtpk_type, no variant fields. */
    /* When kind == mtpk_non_type: */
    a_type_ptr  type;   /* The type of the non-type template parameter. */
    /* When kind == mtpk_template: */
    a_module_template_parameter_list
                *params_list;
                        /* The inner template parameter list of the template
                           template parameter. */
  } variant;
  a_module_template_parameter() = default;
  a_module_template_parameter(const a_module_template_parameter&) = delete;
  a_module_template_parameter(a_module_template_parameter &&other);
  ~a_module_template_parameter();
  a_module_template_parameter& operator=(a_module_template_parameter &&other);
};  /* a_module_template_parameter */

extern a_boolean operator==(const a_module_template_parameter &a,
                            const a_module_template_parameter &b);

using a_module_func_param_list = Dyn_array<a_type_ptr>;
                        /* A list of function parameter types for use by module
                           entity hashing. */

using a_module_deduct_guide_param_list = Dyn_array<a_type_ptr>;
                        /* A list of deduction guide parameter types for use by
                           module entity hashing. */

extern a_module_entity_scope* get_module_entity_scope(
                                                a_symbol_header_ptr   name,
                                                a_module_entity_scope *parent);

extern a_module_entity_ptr get_module_entity(a_module_ptr          mod,
                                             a_module_entity_scope *scope,
                                             a_symbol_header_ptr   name);

extern a_module_entity_ptr get_alias_module_entity(
                                                  a_module_ptr          mod,
                                                  a_module_entity_scope *scope,
                                                  a_symbol_header_ptr   name);

extern a_module_entity_ptr get_function_module_entity(
                            a_module_ptr                         mod,
                            a_module_entity_scope                *scope,
                            a_symbol_header_ptr                  name,
                            Owning_ptr<a_module_func_param_list> &&func_params,
                            a_boolean                            has_ellipsis);

extern a_module_entity_ptr get_specialized_module_entity(
                                   a_module_ptr               mod,
                                   a_module_entity_scope      *scope,
                                   a_symbol_header_ptr        name,
                                   an_owned_template_arg_list &&template_args);

extern a_module_entity_ptr get_function_template_module_entity(
                a_module_ptr                                 mod,
                a_module_entity_scope                        *scope,
                a_symbol_header_ptr                          name,
                Owning_ptr<a_module_template_parameter_list> &&template_params,
                Owning_ptr<a_module_func_param_list>         &&func_params,
                a_boolean                                    has_ellipsis);

extern a_module_entity_ptr get_specialized_function_module_entity(
                          a_module_ptr                         mod,
                          a_module_entity_scope                *scope,
                          a_symbol_header_ptr                  name,
                          an_owned_template_arg_list           &&template_args,
                          Owning_ptr<a_module_func_param_list> &&func_params,
                          a_boolean                            has_ellipsis);

extern a_module_entity_ptr get_deduction_guide_module_entity(
                a_module_ptr                                 mod,
                a_module_entity_scope                        *scope,
                a_symbol_header_ptr                          name,
                Owning_ptr<a_module_template_parameter_list> &&template_params,
                Owning_ptr<a_module_deduct_guide_param_list> &&param_list);

extern void import_header_module(a_module_import_decl_ptr midp);

extern void import_curr_module();

extern void import_module(a_module_import_decl_ptr midp,
                          a_symbol_ptr             assoc_sym);

extern a_boolean has_variable_initializer_from_module(a_variable_ptr  vp);

extern a_boolean load_variable_initializer_from_module(a_variable_ptr  vp);

extern a_boolean has_routine_definition_from_module(a_routine_ptr  rp);

extern a_boolean load_routine_definition_from_module(a_routine_ptr  rp);

extern a_boolean has_template_definition_from_module(a_template_ptr  templ);

extern a_boolean has_pending_template_definition_from_module(
                                                        a_template_ptr  templ);

extern a_boolean load_template_definition_from_module(a_template_ptr  templ);

extern a_boolean has_pending_template_specializations_from_module(
                                                        a_template_ptr  templ);

extern a_boolean load_template_specializations_from_module(
                                                        a_template_ptr  templ);

extern a_boolean has_type_definition_from_module(a_type_ptr  ty);

extern a_boolean load_type_definition_from_module(a_type_ptr ty);


inline a_boolean is_entity_resolved(a_module_entity_ptr mep)
/*
Given a module entity pointer, return TRUE if the entity is in a resolved state
(i.e., the entity is either resolved to an IL entity or marked invalid);
otherwise, return FALSE.
*/
{
  return mep->imminent && (mep->entity.ptr != NULL || mep->invalid);
}  /* is_entity_resolved */


inline a_boolean is_entity_imminent(a_module_entity_ptr mep)
/*
Given a module entity pointer, return TRUE if the entity is an unresolved
imminent state (i.e., the entity is not yet resolved to an IL entity or marked
invalid); otherwise, return FALSE.
*/
{
  return mep->imminent && !mep->invalid && mep->entity.ptr == NULL;
}  /* is_entity_imminent */


extern a_module_entity_ptr locate_module_entity(a_module_entry_locator loc);

extern a_boolean request_entity(a_module_entity_ptr mep);

extern void load_namespace_elements_from_locator(a_module_entity_ptr    mep,
                                                 a_module_entry_locator loc);

extern void mark_locator_as_primary(a_module_entity_ptr    mep,
                                    a_module_entry_locator loc);

extern void update_entity_from_new_locator(a_module_entity_ptr    mep,
                                           a_module_entry_locator new_loc);

extern Opt<a_source_position> source_position_of(a_module_entity_ptr mep);

/*
An internal token cache wrapper structure that represents additional state for
modules.
*/
struct a_module_token_cache {
  a_module_token_cache(const a_source_position *initial_hint = NULL)
    : underlying_cache(/*reusable=*/TRUE), valid(TRUE),
      position_hint(initial_hint)
    {}
  a_module_token_cache(const a_module_token_cache&) = delete;
  inline ~a_module_token_cache();

  a_boolean is_valid() const
    { return valid; }
  void invalidate()
    { this->valid = FALSE; }

  a_token_cache_ptr as_canonical()
    { return &(this->underlying_cache); }
  const a_token_cache* as_canonical() const
    { return &(this->underlying_cache); }

  a_boolean is_empty() const
    { return this->underlying_cache.is_empty(); }

  void append_token(const a_cached_token &tok)
    { this->underlying_cache.append_token(tok); }
  void append_token(a_cached_token &&tok)
    { this->underlying_cache.append_token(move_from(&tok)); }
  void append_token(const a_shared_token &tok)
    { this->underlying_cache.append_token(tok); }
  void append_token(a_shared_token &&tok)
    { this->underlying_cache.append_token(move_from(&tok)); }

  const a_shared_token& get_first_token()
    { return this->underlying_cache.get_first_token(); }
  const a_shared_token& get_last_token()
    { return this->underlying_cache.get_last_token(); }

  a_token_cache_iterator get_last_token_iter()
    { return this->underlying_cache.get_last_token_iter(); }

  a_token_cache_iterator begin()
    { return this->underlying_cache.begin(); }
  a_token_cache_iterator end()
    { return this->underlying_cache.end(); }

  const a_source_position* get_position_hint() const
    { return this->position_hint; }
  void set_position_hint(const a_source_position *new_position_hint)
    { this->position_hint = new_position_hint; }
  void suggest_source_position(a_source_position_ptr new_position_hint);
private:
  a_token_cache
                underlying_cache;
                        /* The underlying cache to insert tokens into. */
  a_boolean     valid;  /* TRUE if the underlying cache should be parsed after
                           caching; otherwise, FALSE. */
  const a_source_position
                *position_hint;
                        /* Current source position hint (used for source
                           position inference). */
};  /* a_module_token_cache */


inline void a_module_token_cache::suggest_source_position(
                                       a_source_position_ptr new_position_hint)
/*
Set the token position hint (only) if the new token position follows the most
recent token in the cache.
*/
{
  check_assertion(new_position_hint != NULL);
  if (this->is_empty()) {
    this->position_hint = new_position_hint;
  } else {
    const a_source_position *last_pos =
                                 this->get_last_token()->get_source_position();

    if (last_pos->seq < new_position_hint->seq ||
        (last_pos->seq == new_position_hint->seq &&
         last_pos->column < new_position_hint->column)) {
      this->position_hint = new_position_hint;
    }  /* if */
  }  /* if */
}  /* a_module_token_cache::suggest_source_position */


a_module_token_cache::~a_module_token_cache()
/*
Cleanup after the token cache.
*/
{
#if DEBUG
/* GCOVR_EXCL_START */
  if (!this->valid && db_flag_is_set("invalid_token_cache")) {
    fprintf(f_debug, "Discarded invalid module cache:\n");
    db_tokens(&(this->underlying_cache));
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
}  /* a_module_token_cache::~a_module_token_cache() */


using a_module_token_cache_ptr = a_module_token_cache*;

inline const a_source_position*
infer_next_source_position(a_module_token_cache_ptr cache,
                           const a_source_position  *pos = NULL)
/*
Perform source position inference on the given cache.  The inferred position
should only be requested when gathering the source position for a new token.

Position inference resolves the source position to one of the following:

  1. The provided source position (if not NULL).
  2. The current position hint (a_module_token_cache_ptr::get_position_hint).
  3. The previous token's source position.
  4. The null source position.

The current source position hint is cleared after inference as the new token's
source position (i.e., the result of the most recent call to this function for
this cache) should take priority.
*/
{
  if (pos != NULL) {
    goto done;
  }  /* if */
  pos = cache->get_position_hint();
  if (pos != NULL) {
    goto done;
  }  /* if */
  if (cache->is_empty()) {
    pos = &null_source_position;
  } else {
    const a_shared_token &last_tok = cache->get_last_token();

    pos = last_tok->get_source_position();
  }  /* if */
done:
  cache->set_position_hint(NULL);
  return pos;
}  /* infer_next_source_position */


inline a_token_sequence_number enter_module_token_rescan(
                                                a_module_token_cache_ptr cache)
/*
Begin a token rescan of the given module token cache.  The module token cache
will be terminated by this function and should not be pre-terminated.  The
token sequence number for the added terminator token (end of source) will be
returned.  The caller is responsible for calling exit_module_token_rescan after
the rescanned tokens have been used.

If possible, prefer use of the RAII class a_module_entity_rescan which handles
calls to exit_module_token_rescan automatically upon destruction.
*/
{
#if CHECKING
  check_assertion_str(!cache->is_empty(), "the cache cannot be empty");
  check_assertion_str(!cache->get_last_token()->is(tok_end_of_source),
                      "the cache cannot be pre-terminated.");
#endif /* CHECKING */
  terminate_token_cache(cache->as_canonical());

  a_shared_token_cache tok_cache(move_from(cache->as_canonical()));
  const a_shared_token &last_tok = tok_cache->get_last_token();
  push_lexical_state_stack();
  push_stop_token_stack();
  rescan_shared_reusable_cache(tok_cache);
  increment_dependent_scans_for_reusable_cache();
  return last_tok->get_starting_seq_number();
}  /* enter_module_token_rescan */


inline void exit_module_token_rescan(
                             a_const_char            *orig_start_of_curr_token,
                             a_const_char            *orig_end_of_curr_token,
                  ARG_UNUSED a_token_sequence_number expected_end_tsn,
                  ARG_UNUSED a_token_kind            final_token = tok_error)
/*
Restore the token stream state after processing a module token cache.

The given start_of_curr_token value (orig_start_of_curr_token) and
end_of_curr_token value (orig_end_of_curr_token) are the original start and end
of the current token prior to the start of the rescan.

The given expected_end_tsn argument is the expected token sequence number for
the associated terminator (end of source) token.  If upon clearing any
remaining tokens the encountered end of source token's sequence number doesn't
match, the front end will expect an error in CHECKING modes.

The given final token argument is the expected current token at the time this
function is called.  If the final token doesn't match the current token, the
front end will expect an error in CHECKING modes.  If there is no specific
expected token, tok_error can be used to safely skip this check.
*/
{
#if CHECKING
  if (final_token != tok_error && curr_token != final_token) {
    expect_error();
  }  /* if */
#endif /* CHECKING */
  clear_stop_tokens();
  flush_to_end_of_source(/*suppress_warning=*/TRUE);
  decrement_dependent_scans_for_reusable_cache();
  pop_stop_token_stack();
  pop_lexical_state_stack();
  check_assertion(curr_token == tok_end_of_source);
  check_assertion(curr_token_sequence_number == expected_end_tsn ||
                  curr_token_sequence_number == NO_TOKEN_SEQUENCE_NUMBER);
  (void)get_token();
  start_of_curr_token = orig_start_of_curr_token;
  end_of_curr_token = orig_end_of_curr_token;
}  /* exit_module_token_rescan */


/*
A structure for representing an automatically cleaned up module token rescan
operation.  This class should be used to reenter a module token cache for
parsing.
*/
struct a_module_entity_rescan {
  inline a_module_entity_rescan(
                         a_module_token_cache_ptr cache,
                         a_token_kind             *final_token_ptr_val = NULL);
  inline ~a_module_entity_rescan();
private:
  a_boolean     valid;  /* TRUE if the rescan successfully cached one or more
                           tokens; otherwise, FALSE. */
  a_token_kind  *final_token_ptr;
                        /* A pointer to the variable storing the final token
                           value token to be used on deconstruction, or NULL if
                           tok_error should be passed to
                           exit_module_token_rescan. */
#if CHECKING
  a_const_char*
                expected_curr_source_line;
                        /* The value of curr_source_line when the rescan was
                           started and thus, the expected value of
                           curr_source_line when the rescan ends. */
#endif /* CHECKING */
  a_const_char*
                save_start_of_curr_token;
                        /* The value of start_of_curr_token when the rescan was
                           started that will be restored to the current token
                           when the rescan ends. */
  a_const_char*
                save_end_of_curr_token;
                        /* The value of end_of_curr_token when the rescan was
                           started that will be restored to the current token
                           when the rescan ends. */
#if CHECKING
  a_token_sequence_number
                expected_end_tsn;
                        /* The expected ending token sequence number. */
#endif /* CHECKING */
};  /* a_module_entity_rescan */


a_module_entity_rescan::a_module_entity_rescan(
                                 a_module_token_cache_ptr cache,
                                 a_token_kind             *final_token_ptr_val)
/*
Apply the appropriate initialization logic to set up the parser for parsing the
tokens specified in the given cache.  final_token_ptr_val should be a pointer
to the expected token kind upon a correct parse, or NULL if no specific token
is expected.
*/
  : valid(cache->is_valid()), final_token_ptr(final_token_ptr_val),
#if CHECKING
    expected_curr_source_line(curr_source_line),
#endif /* CHECKING */
    save_start_of_curr_token(start_of_curr_token),
    save_end_of_curr_token(end_of_curr_token)
{
  if (this->valid) {
#if CHECKING
    this->expected_end_tsn =
#endif /* CHECKING */
      /* Do not put code here. */
      enter_module_token_rescan(cache);
  } else {
#if CHECKING
    this->expected_end_tsn = NO_TOKEN_SEQUENCE_NUMBER;
#endif /* CHECKING */
  }  /* if */
}  /* a_module_entity_rescan */


a_module_entity_rescan::~a_module_entity_rescan()
/*
Apply the appropriate token cleanup logic to restore the parser state prior to
the module entity rescan.
*/
{
  if (this->valid) {
    a_token_kind            final_token = tok_error;
    a_token_sequence_number end_tsn = NO_TOKEN_SEQUENCE_NUMBER;

    if (this->final_token_ptr != NULL) {
      final_token = *(this->final_token_ptr);
    }  /* if */
#if CHECKING
    end_tsn = this->expected_end_tsn;
#endif /* CHECKING */
#if CHECKING
    /* If this assertion fails, something has altered the current source line
       and needs to be modified to ensure it restores the original
       curr_source_line value. */
    check_assertion(this->expected_curr_source_line == curr_source_line);
#endif /* CHECKING */
    exit_module_token_rescan(this->save_start_of_curr_token,
                             this->save_end_of_curr_token,
                             end_tsn, final_token);
  }
}  /* a_module_entity_rescan::~a_module_entity_rescan */


extern void save_function_definition_for_module_write(
                                            a_routine_ptr              rp,
                                            const a_shared_token_cache &token);

extern a_shared_token_cache get_function_definition_for_module_write(
                                                             a_routine_ptr rp);

extern void save_field_initializer_for_module_write(
                                            a_field_ptr                fp,
                                            const a_shared_token_cache &token);

extern a_shared_token_cache get_field_initializer_for_module_write(
                                                               a_field_ptr fp);

#if DEBUG
/* GCOVR_EXCL_START */

extern void db_tokens(a_module_token_cache_ptr cache);

extern a_string s_db_module(a_module_ptr mod);

extern void db_module(a_module_ptr mod);

extern a_string s_db_module_entry_locator(a_module_entry_locator loc);

extern void db_module_entry_locator(a_module_entry_locator loc);

extern a_string s_basic_db_mep(a_module_entity_ptr mep);

extern void db_mep(a_module_entity_ptr mep);

extern void push_module_context(a_module_ptr mod_ptr);

extern void pop_module_context(void);

extern void db_module_entity(a_module_entity_ptr mep);

extern void db_module_stack();

extern void db_mep_stack();

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

extern void modules_pch_reset();

extern void modules_check_for_suppressed_errors();

extern void modules_early_init();

extern void modules_one_time_init();

extern void require_modules();

extern void modules_trans_unit_init();

extern void modules_trans_unit_wrapup_part_1();

extern void modules_trans_unit_wrapup_part_2();

#if MAKE_FRONT_END_CALLABLE
extern void modules_cleanup();
#endif /* MAKE_FRONT_END_CALLABLE */

extern void modules_write_out();

#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef MODULES_H */

