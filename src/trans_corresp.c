/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

trans_corresp.c -- Routines related to matching entities across
                   translation units.

*/

#include "basic_hdrs.h"

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
#include "trans_corresp.h"
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */

#undef record_builtin_type
#undef record_instantiation
#undef record_default_arg_instantiation
#undef establish_class_instantiation_corresp
#undef establish_function_instantiation_corresp
#undef establish_variable_instantiation_corresp
#undef establish_enum_instantiation_corresp
#undef establish_block_extern_function_correspondence
#undef establish_block_extern_variable_correspondence
#undef establish_friend_type_correspondence

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/* Forward declarations. */
static void clear_scope_correspondence(a_scope_ptr  scope,
                                       a_boolean    visited);
static void clear_type_correspondence(a_type_ptr  type,
                                      a_boolean   visited);
static void establish_trans_unit_correspondences_for_enum(a_type_ptr  type);
static void establish_trans_unit_correspondences_for_class(a_type_ptr  type);
static void find_template_correspondence(a_template_ptr  templ,
                                         a_boolean       parent_found);
static a_symbol_list_entry_ptr find_class_template_instantiation(
                                      a_template_symbol_supplement_ptr  tssp,
                                      a_symbol_ptr                      inst);
static a_boolean verify_type_correspondence(a_type_ptr  type);
static a_boolean verify_template_correspondence(a_template_ptr  templ);
static void verify_trans_unit_correspondences_for_scope(a_scope_ptr  scope);
static void establish_instantiation_correspondences(
                                               a_template_ptr  templ,
                                               a_template_ptr  corresp_templ);
static void process_pending_instantiations(void);


/* Pointers to canonical built-in types. */
STATIC_THREAD a_type_ptr
                canonical_int_types[(int)ik_last];
STATIC_THREAD a_type_ptr
                canonical_signed_int_types[(int)ik_last];
#if MICROSOFT_EXTENSIONS_ALLOWED
STATIC_THREAD a_type_ptr
                canonical_microsoft_sized_int_types[(int)ik_last];
STATIC_THREAD a_type_ptr
                canonical_microsoft_sized_signed_int_types[(int)ik_last];
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if FIXED_POINT_ALLOWED
STATIC_THREAD a_type_ptr
                canonical_fixed_point_types[(int)fpp_last][/*is_unsigned*/2]
                                           [/*is_fract*/2][/*saturating*/2];
#endif /* FIXED_POINT_ALLOWED */
STATIC_THREAD a_type_ptr
                canonical_float_types[(int)fk_last];
#if C99_IL_EXTENSIONS_SUPPORTED
STATIC_THREAD a_type_ptr
                canonical_complex_types[(int)fk_last];
STATIC_THREAD a_type_ptr
                canonical_imaginary_types[(int)fk_last];
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
STATIC_THREAD a_type_ptr
                canonical_il_void_type;
STATIC_THREAD a_type_ptr
                canonical_il_reflection_type;
STATIC_THREAD a_type_ptr
                canonical_il_wchar_t_type;
STATIC_THREAD a_type_ptr
                canonical_il_char8_t_type;
STATIC_THREAD a_type_ptr
                canonical_il_char16_t_type;
STATIC_THREAD a_type_ptr
                canonical_il_char32_t_type;
STATIC_THREAD a_type_ptr
                canonical_il_bool_type;
STATIC_THREAD a_type_ptr
                canonical_il_standard_nullptr_type;
STATIC_THREAD a_type_ptr
                canonical_il_managed_nullptr_type;

/*
Correspondence checking is inhibited when errors other than correspondence
errors occur (because such errors may have changed the IL in ways that can
trigger spurious correspondence errors).  To keep track of such errors we
count correspondence errors in corresp_errors: total_errors - corresp_errors
then produces the number of noncorrespondence errors.  corresp_errors is a
"per translation unit" variable, which means that correspondence errors in
issued for another translation unit are treated as non-correspondence errors
in the current translation unit.  This approach avoids a cascade of
correspondence errors due to errors in one translation unit.
*/
STATIC_THREAD unsigned long
                corresp_errors;

#define any_noncorresp_errors()                                       \
 ((diagnostic_counters.total.errors - corresp_errors) != 0)

/*
Whenever the canonical entry of a correspondence set changes from an entry
in a translation unit that is already processed to an entry in the current
translation unit, the previous canonical entry must be verified against the
new one.  However, this verification must be delayed until all the
correspondences have been set.  Therefore a list of "previously canonical
entries" is created to keep track of items to verify later on (see function
process_verification_list).
*/
typedef struct a_verification_entry *a_verification_entry_ptr;
typedef struct a_verification_entry {
  a_verification_entry_ptr
		next;
			/* The next item to verify. */
  an_il_entry_kind
		kind;
			/* The kind of entry pointed to by il_entry. */
  char		*il_entry;
			/* A pointer to an IL entry whose correspondence
			   must be verified against the canonical entry. */
} a_verification_entry;

/* Pointer to the head of the list of IL entries to verify. */
STATIC_THREAD a_verification_entry_ptr
		verification_list;

STATIC_THREAD a_verification_entry_ptr
		avail_verification_entries;


static void add_verification_entry(an_il_entry_kind  kind,
                                   char              *il_entry)
/*
Record an IL entry to be verified at the end of verification processing.
*/
{
  a_verification_entry_ptr  entry;

  if (avail_verification_entries != NULL) {
    entry = avail_verification_entries;
    avail_verification_entries = avail_verification_entries->next;
  } else {
    entry = (a_verification_entry_ptr)alloc_fe(sizeof(a_verification_entry));
  }  /* if */
  entry->next = verification_list;
  verification_list = entry;
  entry->kind = kind;
  entry->il_entry = il_entry;
}  /* add_verification_entry */


static void remove_from_verification_list(char  *il_entry)
/*
See if the given entry is on the verification list, and if so clear the
list entry.
*/
{
  a_verification_entry_ptr  entry = verification_list;

  for (; entry != NULL; entry = entry->next) {
    if (entry->il_entry == il_entry) {
      entry->kind = (an_il_entry_kind)iek_none;
      entry->il_entry = NULL;
      break;
    }  /* if */
  }  /* for */
}  /* remove_from_verification_list */


static void free_verification_entry(a_verification_entry_ptr  entry)
/*
Return the given entry to the list of available entries.
*/
{
  entry->next = avail_verification_entries;
  avail_verification_entries = entry->next;
}  /* free_verification_entry */


/*
Finding correspondences for class template instantiations is dependent on
finding the correspondences for the associates templates.  However, the
processing of templates must occur before the corresponding instantiations
to avoid infinite recursion.  Therefore, we build a list of instantiations
to process as we find correspondences for templates.  The list is then
processed later (see process_pending_instantiations).
*/
STATIC_THREAD a_symbol_list_entry_ptr
                instantiations_to_process;

static void add_pending_instantiation(a_symbol_ptr  inst)
/*
Add the given instance symbol to the list of instantiations whose
correspondences must be determined at a later time.
*/
{
  a_symbol_list_entry_ptr  slep = alloc_symbol_list_entry();

  slep->next = instantiations_to_process;
  instantiations_to_process = slep;
  slep->symbol = inst;
}  /* add_pending_instantiation */


static a_boolean type_has_definition(a_type_ptr  type)
/*
Return TRUE if type has a definition.
*/
{
  a_boolean  result;

  type = skip_typerefs(type);
  if (is_immediate_class_type(type)) {
    result = class_type_has_body(type);
  } else if (is_scoped_enum_type(type) ||
             (is_enum_type(type) &&
              type->variant.integer.has_explicit_enum_base)) {
    /* Opaque enumerations are complete types, so check whether there was an
       enumerator list seen. */
    result = !type->incomplete &&
             integer_type_supp(type)->enumerator_list_seen;
  } else {
    result = !is_incomplete_type(type);
  }  /* if */
  return result;
}  /* type_has_definition */


/*
Return TRUE if the symbol associated with the source correspondence of the
given entity is defined.
*/
#define assoc_sym_defined(ptr)                                              \
  (((a_symbol_ptr)((a_source_correspondence_ptr)ptr)->assoc_info)->defined)


/*
Return the symbol list on which an entry corresponding to the given sym may
be expected.
*/
#define corresp_symbol_list(sym)					\
  ((sym)->header->inactive_symbols)


static a_symbol_ptr corresp_extern_symbol_list(a_symbol_ptr  sym)
/*
The given symbol corresponds to a variable or routine declaration in
namespace scope.  Return the list of external symbols that contains
sym.
*/
{
  a_symbol_locator     loc, ext_loc;
  a_name_linkage_kind  name_linkage;
  a_type_ptr           routine_type;
  a_requires_clause    *trcp;

  check_assertion(sym->kind == sk_routine || sym->kind == sk_member_function ||
                  sym->kind == sk_variable);
  /* All of the hard work is done by find_external_symbol; we just have
     to pass it the appropriate linkage and type. */
  if (sym->kind != sk_variable) {
    /* An ordinary function or a member function. */
    a_routine_ptr  routine = sym->variant.routine.ptr;
    name_linkage = enum_cast<a_name_linkage_kind>(
                                         routine->source_corresp.name_linkage);
    routine_type = routine->type;
    trcp = routine->trailing_requires_clause;
  } else {
    /* A variable symbol. */
    a_variable_ptr  variable = sym->variant.variable.ptr;
    name_linkage = enum_cast<a_name_linkage_kind>(
                                        variable->source_corresp.name_linkage);
    routine_type = NULL;
    trcp = NULL;
  }  /* if */
  make_locator_for_symbol(sym, &loc);
  (void)find_external_symbol(&loc, name_linkage, routine_type, trcp, &ext_loc);
  return ext_loc.symbol_header->other_symbols;
}  /* corresp_extern_symbol_list */


#if DEBUG
/* GCOVR_EXCL_START */
/*
The following variable can be set to point to an IL entry from within a 
debugger.  When that IL entry's correspondence is modified, corresp_intercept
is called.
*/
static void *trace_corresp_ptr = NULL;

static void corresp_intercept(void)
/*
This routine's main purpose is to have a breakpoint set on it from a symbolic
debugger.  The routine is called if the correspondence pointer for the address
pointed to by trace_corresp_ptr is modified.
*/
{
  fprintf(f_debug, "Modifying correspondence for node at %p.\n",
          trace_corresp_ptr);
}  /* corresp_intercept */

#define trace_corresp_check(ptr)                                       \
  if ((void*)(ptr) == trace_corresp_ptr) { corresp_intercept(); }

void* db_corresp(void *ptr)
/*
Report correspondence pointer for the given entry and return the address of the
canonical entry (or NULL if none).
*/
{
  void  *result;

  if (trans_unit_corresp_of_unknown_entry(ptr) != NULL) {
    result = (void*)canonical_il_entry_of(ptr);
    fprintf(f_debug, "Correspondence for %p is %p", ptr, result);
  } else {
    /* This entry doesn't belong to a correspondence set yet. */
    result = NULL;
    fprintf(f_debug, "No correspondence for %p", ptr);
  }  /* if */
  return result;
}  /* db_corresp */

/* GCOVR_EXCL_STOP */
#else /* !DEBUG */

#define trace_corresp_check(ptr)  /* Nothing */

#endif /* DEBUG */

#if DEBUG
/* GCOVR_EXCL_START */

void db_sym_list(a_symbol_list_entry_ptr  entries)
/*
Dump a short summary of the symbols in the given list.
*/
{
  a_line_number            line;
  a_const_char             *file_name, *full_name;
  a_boolean                at_end_of_source;
  a_symbol_list_entry_ptr  first = NULL;

  while (entries != NULL) {
    a_symbol_ptr  sym = entries->symbol;
    if (first == NULL) {
      first = entries;
    } else if (entries == first) {
      fprintf(f_debug, "(CIRCULAR)\n");
      break;
    }  /* if */
    db_symbol_name(sym);
    fprintf(f_debug, " (%s)", symbol_kind_names[(int)sym->kind]);
    (void)conv_seq_to_file_and_line(sym->decl_position.seq, &file_name,
                                    &full_name, &line, &at_end_of_source);
    if (line != 0) {
      fprintf(f_debug, " (%s:%lu)\n", file_name, (unsigned long)line);
    } else {
      fprintf(f_debug, " (%s:built-in)\n", file_name);
    }  /* if */
    entries = entries->next;
  }  /* while */
}  /* db_sym_list */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */


static void f_change_canonical_entry(a_trans_unit_corresp_ptr  tcp,
                                     char                      *entity)
/*
Update the canonical entry field of the given correspondence entry to be the
given entity.
*/
{
  check_assertion(entity != NULL);
#if DEBUG
/* GCOVR_EXCL_START */
  if (tcp->kind != (an_il_entry_kind)iek_base_class &&
      (db_trace("trans_corresp", entity, tcp->kind) ||
       (tcp->canonical != NULL &&
        db_trace("trans_corresp", tcp->canonical, tcp->kind)))) {
    if (tcp->canonical != NULL) {
      fprintf(f_debug, "Canonical entity ");
      db_scp(tcp->canonical);
      fprintf(f_debug, " replaced by ");
      db_scp(entity);
      fprintf(f_debug, ".\n");
    } else {
      db_scp(entity);
      fprintf(f_debug, " is canonical.\n");
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  tcp->canonical = entity;
}  /* f_change_canonical_entry */

#define change_canonical_entry(tcp, ptr)                                 \
  f_change_canonical_entry((tcp), (char*)(ptr))


static int canonical_ranking(an_il_entry_kind  kind,
                             char              *entity)
/*
This routine computes a value reflecting the appropriateness for the given
entity to be the canonical entry.  The value is obtained by adding certain
powers of 2 for various binary criteria (some of which apply only to
certain kinds of entities.  The criteria are (in increasing order of
importance):
   (a) Is the entity's parent canonical?
   (b) Is the entity in the primary IL?
   (c) Does the entity have an explicit initializer? (variables only)
       Does the entity have a determined exception specification?
                                                     (routines only)
   (d) Is the entity a template specialization?
       If it is a prototype instantiation, is the associated template
       defined?
   (e) Is the entity defined?
   (f) Is the entity a non-weak definition or (for a type) complete?
The entity with the highest ranking in a correspondence set should be the
canonical entry.  (See also corresp_ranking in trans_copy.c for a reduced
version of this function.)
The given entity should have a source correspondence.
*/
{
  int                          rank = 0;
  a_source_correspondence_ptr  scp = (a_source_correspondence_ptr)entity;

  check_assertion(kind != (an_il_entry_kind)iek_base_class);
  /* Is the parent canonical? */
  if (scp->is_class_member) {
    a_type_ptr parent_class = scp_parent_class(scp);
    if (canonical_il_entry_of(parent_class) == (char*)parent_class) {
      rank = 1;
    }  /* if */
  } else if (scp_is_namespace_member(scp)) {
    a_namespace_ptr parent_ns = scp_parent_namespace(scp);
    if (canonical_il_entry_of(parent_ns) == (char*)parent_ns) {
      rank = 1;
    }  /* if */
  }  /* if */
  /* Is the entity in the primary translation unit? */
  if (!in_secondary_trans_unit(entity)) {
    rank += 2;
  }  /* if */ 
  switch (kind) {
    case iek_constant:
    case iek_field:
    case iek_namespace:
      /* No other criteria apply. */
      break;
    case iek_routine:
      /* Note: assoc_sym_defined not used because when unneeded routines are
         removed the "defined" flag in the symbol is not cleared. */
      { a_routine_ptr  routine = (a_routine_ptr)entity;
        if (!has_indeterminate_exception_spec(routine)) {
          rank += 4;
        }  /* if */
        if (routine->function_def_number != NULL_function_def_number) {
          rank += 16;
#if GNU_EXTENSIONS_ALLOWED
          if (!routine->is_weak) {
            /* A non-weak definition should displace a weak definition. */
            rank += 32;
          }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
          if ((routine->decl_modifiers & DM_DLLIMPORT) == 0) {
            /* A non-dllimport definition should displace a dllimport
               definition (the latter is only possible for inline
               functions). */
            rank += 64;
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
        if (routine->is_specialized ||
            (routine->is_prototype_instantiation &&
             routine->special_kind != sfk_deduction_guide &&
             assoc_sym_defined(routine->assoc_template))) {
          /* For "real" instances, prefer explicit specializations.  For
             prototype instantiations, prefer those associated with a
             template that includes a definition. */
          rank += 8;
        }  /* if */
      }
      break;
    case iek_template:
      if (assoc_sym_defined(entity)) {
        rank += 16;
      }  /* if */
      break;
    case iek_type:
      { a_type_ptr  type = (a_type_ptr)entity;
        if (type_has_definition(type)) {
          if (!type->incomplete) {
            rank += 32;
          }  /* if */
          rank += 16;
        }  /* if */
        if (is_immediate_class_type(type) &&
            type->variant.class_struct_union.is_specialized) {
          rank += 8;
        }  /* if */
      }
      break;
    case iek_variable:
      { a_variable_ptr  var = (a_variable_ptr)entity;
        if (var->storage_class == (a_storage_class)sc_unspecified) {
          if (var->init_kind != (an_init_kind)initk_none) {
            rank += 4;
          }  /* if */
          rank += 16;
#if GNU_EXTENSIONS_ALLOWED
          if (!var->is_weak) {
            /* A non-weak definition should displace a weak definition. */
            rank += 32;
          }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
        }  /* if */
        if (var->is_specialized) {
          rank += 8;
        }  /* if */
      }
      break;
    default:
      unexpected_condition_str("Bad kind for correspondence checking");
  }  /* if */
  return rank;
}  /* canonical_ranking */


static void update_canonical_entry(an_il_entry_kind  kind,
                                   char              *entity)
/*
The given IL entity (of the given kind) may be a more appropriate canonical
entry than the current canonical entry of its correspondence set.  If so,
the canonical entity is changed by this routine.  In general, canonical
entries should be definitions if possible, and among the definitions, one from
the primary translation unit is preferred (see canonical_ranking for the
exact criteria).
*/
{
  if (kind == (an_il_entry_kind)iek_base_class) {
    /* Base class entries belong to correspondence sets, but are canonical
       if and only if their associated derived class is canonical. */
    a_base_class_ptr  bcp = (a_base_class_ptr)entity;
    a_type_ptr        derived = bcp->derived_class;
    if (canonical_il_entry_of(derived) == (char*)derived) {
      change_canonical_entry(bcp->trans_unit_corresp, entity);
    }  /* if */
  } else {
    a_trans_unit_corresp  *tcp = trans_unit_corresp_of_unknown_entry(entity);
    char                  *old_ce;
    check_assertion(tcp != NULL && kind == tcp->kind);
    old_ce = tcp->canonical;
    if (old_ce != entity &&
        canonical_ranking(kind, entity) > canonical_ranking(kind, old_ce)) {
      /* The canonical entity is about to change.  Update any information
         that depends on the canonical entry. */
      switch (kind) {
        case iek_routine:
          {
            a_routine_ptr  routine = (a_routine_ptr)entity,
                           corresp_routine = (a_routine_ptr)old_ce;
            if (routine->is_template_function &&
                corresp_routine->is_template_function &&
                !routine->is_prototype_instantiation &&
                !routine->is_specialized) {
              set_master_instance_for_new_canonical_routine(
                                                     routine, corresp_routine);
            }  /* if */
          }
          break;
        case iek_template:
          { /* Since the canonical template is changing, the associated
               all_instantiations list must be moved too. */
            a_template_ptr
                 corresp_templ = (a_template_ptr)entity,
                 templ = (a_template_ptr)old_ce;
            a_symbol_ptr
                 templ_sym = symbol_for(templ),
                 corresp_sym = symbol_for(corresp_templ);
            a_template_symbol_supplement_ptr
                 tssp = template_supplement_for_symbol(templ_sym),
                 corresp_tssp = template_supplement_for_symbol(corresp_sym);
            if (tssp->all_instantiations != NULL) {
              /* The canonical entry is changing: the list of all
                 instantiations should be reattached to the new canonical
                 entry. */
#if DEBUG
/* GCOVR_EXCL_START */
              if (db_trace("trans_corresp", templ, iek_template) ||
                  db_trace("trans_corresp", corresp_templ, iek_template)) {
                fprintf(f_debug, "all_instantiations transferred because\n");
              }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
              if (corresp_tssp->all_instantiations == NULL) {
                corresp_tssp->all_instantiations = tssp->all_instantiations;
              } else {
                /* corresp_tssp already has entries on its all_instantiations
                   list.  That can occur if an instance of the template was
                   recorded prior to the associated template having a
                   correspondence. */
                a_symbol_list_entry_ptr  *sym_entry =
                                            &corresp_tssp->all_instantiations;
                while (*sym_entry != NULL) sym_entry = &(*sym_entry)->next;
                *sym_entry = tssp->all_instantiations;
              }  /* if */
              tssp->all_instantiations = NULL;
            }  /* if */
          }
          break;
        case iek_variable:
          {
            a_variable_ptr  var = (a_variable_ptr)entity;
            a_boolean       is_prototype_inst = FALSE;

            if (var->source_corresp.is_class_member) {
              is_prototype_inst = parent_class_of(var)
                       ->variant.class_struct_union.is_prototype_instantiation;
            } else {
              is_prototype_inst = var->is_prototype_instantiation;
            }  /* if */
            if (!is_prototype_inst && var->is_template_variable) {
              a_symbol_ptr
                   new_sym = symbol_for(var),
                   old_sym = symbol_for((a_variable_ptr)old_ce);
              a_template_instance_ptr
                   new_tip = new_sym->variant.static_data_member.instance_ptr,
                   old_tip = old_sym->variant.static_data_member.instance_ptr;
              if (new_tip == NULL || old_tip == NULL ||
                  old_tip->master_instance == NULL) {
                /* In a normal (no correspondence errors) case, we should have
                   valid instance pointers.  However, cases can be constructed
                   that are invalid C++ where this is not the case. */
                expect_error();
              } else {
                set_master_instance_for_new_canonical_variable(
                                                 var, (a_variable_ptr)old_ce);
              }  /* if */
            }  /* if */
          }
          break;
        default:
          /* Nothing to be done. */
          break;
      }  /* switch */
      if (in_secondary_trans_unit(old_ce)) {
        /* Make sure that the previously canonical entry is compared against
           whichever entry ends up being the canonical entry of the
           correspondence set. */
        add_verification_entry(kind, old_ce);
      }  /* if */
      change_canonical_entry(tcp, entity);
      if (kind == (an_il_entry_kind)iek_type) {
        a_type_ptr  old_ctp = (a_type_ptr)old_ce;
        if (is_immediate_class_type(old_ctp) && !old_ctp->incomplete) {
          /* We're changing from one class definition to another.  Update
             the member correspondences. */
          establish_trans_unit_correspondences_for_class(old_ctp);
        } else if (is_immediate_enum_type(old_ctp) && !old_ctp->incomplete) {
          /* We're changing from one enum definition to another.  (This is
             possible because many modes permit a nondefining declaration of
             an enum type.)  Update the correspondences of the enumerator
             constants if needed. */
          establish_trans_unit_correspondences_for_enum(old_ctp);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* update_canonical_entry */

#if MAINTAIN_NEEDED_FLAGS

static void transfer_needed_info(an_il_entry_kind          kind,
                                 char                      *entity,
                                 a_trans_unit_corresp_ptr  tcp)
/*
Transfer "needed" information from the given (new) entity to its canonical
entry (if the canonical entry had been set previously, setting "needed" on the
new entry would have set "needed" on the canonical entry, so we're catching up
on what would have been done).  Do not process entities in the primary IL,
because those flags get set correctly only with the final IL after copying.
tcp is the correspondence node associated with entity.
*/
{
  if (in_secondary_trans_unit(entity)) {
    if (tcp->canonical != entity &&
        in_secondary_trans_unit(tcp->canonical)) {
      char *canonical = tcp->canonical;
      if (kind != (an_il_entry_kind)iek_base_class) {
        a_source_correspondence *new_scp= (a_source_correspondence *)entity;
        if (new_scp->needed) {
          mark_as_needed(canonical, kind);
        }  /* if */
      }  /* if */
      if (il_entry_prefix_of(entity).keep_in_il) {
        mark_to_keep_in_il(canonical, kind);
      }  /* if */
      if (kind == (an_il_entry_kind)iek_type) {
        a_type_ptr can_type = (a_type_ptr)canonical;
        a_type_ptr new_type = (a_type_ptr)entity;
        if (is_immediate_class_type(new_type) &&
            is_immediate_class_type(can_type)) {
          if (new_type->variant.class_struct_union.definition_needed) {
            set_class_definition_needed(can_type);
          }  /* if */
          if (new_type->variant.class_struct_union.keep_definition_in_il) {
            set_class_keep_definition_in_il(can_type);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* transfer_needed_info */

#endif /* MAINTAIN_NEEDED_FLAGS */

static void f_set_trans_unit_corresp(an_il_entry_kind  kind,
                                     char              *entity1,
                                     char              *entity2)
/*
Make entity1 point to the same correspondence entry as entity2.  If necessary,
this routine will create such a correspondence entry.
*/
{
  a_trans_unit_corresp_ptr  *tcp1, *tcp2;

  check_assertion_str(entity1 != NULL && entity2 != NULL && entity1 != entity2,
                      "f_set_trans_unit_corresp: bad input");
  trace_corresp_check(entity1);
#if DEBUG
/* GCOVR_EXCL_START */
  if (kind != (an_il_entry_kind)iek_base_class &&
      db_trace("trans_corresp", entity1, kind)) {
    db_scp(entity1);
    fprintf(f_debug, " should correspond to ");
    db_scp(entity2);
    fprintf(f_debug, ".\n");
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  if (kind == (an_il_entry_kind)iek_base_class) {
    /* Base class entries are the only entries with a correspondence pointer
       that is not part of a source correspondence structure. */
    tcp1 = &((a_base_class*)entity1)->trans_unit_corresp;
    tcp2 = &((a_base_class*)entity2)->trans_unit_corresp;
  } else {
    tcp1 = &trans_unit_corresp_of_unknown_entry(entity1);
    tcp2 = &trans_unit_corresp_of_unknown_entry(entity2);
  }  /* if */
  if (*tcp2 == NULL) {
    trace_corresp_check(entity2);
    /* Presumably, no entity corresponding to entity1 has been processed yet.
       Allocate a correspondence entry to start a correspondence set with
       entity2. */
    if (*tcp1 != NULL) {
      /* Reuse the correspondence node of tcp1: it may in some cases have
         a correspondence set already (this happens only when entity2 is a
         new canonical entity). */
      *tcp2 = *tcp1;
      ++(*tcp2)->count;
      update_canonical_entry(kind, entity2);
    } else {
      /* Neither of the two entries had a correspondence node: create one. */
      *tcp2 = alloc_trans_unit_corresp();
      (*tcp2)->kind = kind;
      ++(*tcp2)->count;
      change_canonical_entry(*tcp2, entity2);
    }  /* if */
  } else if (*tcp1 != NULL && *tcp1 != *tcp2) {
    a_boolean is_template = FALSE;
    /* Both entity1 and entity2 have correspondence sets already.  One of
       them should be a singleton and can therefore be freed.  An exception is
       the sharing by two template entries that are in the same translation
       unit.  Correspondence errors can also lead to unexpected sharing, which
       will be diagnosed later during the verification step. */
    if ((*tcp1)->count > 1 && (*tcp2)->count == 1) {
      a_trans_unit_corresp_ptr *tmp = tcp2;
      tcp2 = tcp1;
      tcp1 = tmp;
    }  /* if */
    if (kind == iek_template ||
        (kind == iek_variable &&
         ((a_variable_ptr)entity1)->is_template_variable)) {
      is_template = TRUE;
    }  /* if */
    if (!is_template && (*tcp1)->count > 1) {
      expect_error_str("set_trans_unit_corresp: correspondence busy");
    }  /* if */
    free_trans_unit_corresp(*tcp1);
  }  /* if */
  /* Add entity1 to the correspondence set of entity2. */
  if (*tcp1 != *tcp2) {
    *tcp1 = *tcp2;
    ++(*tcp2)->count;
  }  /* if */
  update_canonical_entry(kind, entity1);
  /* Is either entity coming from a primary translation unit? */
  if (!in_secondary_trans_unit(entity2)) {
    (*tcp2)->primary = entity2;
  } else if (!in_secondary_trans_unit(entity1)) {
    (*tcp2)->primary = entity1;
  }  /* if */
#if MAINTAIN_NEEDED_FLAGS
  transfer_needed_info(kind, entity1, *tcp2);
#endif /* MAINTAIN_NEEDED_FLAGS */
}  /* f_set_trans_unit_corresp */

#define set_trans_unit_corresp(kind, entity1, entity2)                    \
  f_set_trans_unit_corresp((an_il_entry_kind)(kind),                      \
                           (char*)(entity1), (char*)(entity2))


static a_trans_unit_corresp_ptr* detach_trans_unit_corresp(
                                                    an_il_entry_kind  kind,
                                                    char              *entity)
/*
If possible, set the translation unit correspondence pointer in the given
entity to NULL.  Return the address of that pointer.
*/
{
  a_trans_unit_corresp_ptr  *tcp;

  check_assertion(entity != NULL);
  if (kind == (an_il_entry_kind)iek_base_class) {
    /* Base class entries are the only entries with a correspondence pointer
       that is not part of a source correspondence structure. */
    tcp = &((a_base_class*)entity)->trans_unit_corresp;
  } else {
    tcp = &trans_unit_corresp_of_unknown_entry(entity);
  }  /* if */
  if (*tcp != NULL && 
      ((*tcp)->canonical != entity ||
       ((*tcp)->primary != NULL && (*tcp)->canonical != (*tcp)->primary))) {
    /* Detach the given entity from the correspondence entry. */
    if ((*tcp)->canonical == entity) {
      /* Normally, the canonical entry cannot be detached from the
         correspondence set.  In error cases, however, it is possible
         that the canonical entry wasn't a match after all.  This only
         occurs when there is a non-canonical corresponding entry in
         the primary translation unit. */
      check_assertion(is_at_least_one_error());
      change_canonical_entry(*tcp, (*tcp)->primary);
    }  /* if */
    free_trans_unit_corresp(*tcp);
    *tcp = NULL;
  }  /* if */
  return tcp;
}  /* detach_trans_unit_corresp */


static void f_set_no_trans_unit_corresp(an_il_entry_kind  kind,
                                        char              *entity)
/*
Mark the given IL entry as having no correspondence in another translation
unit.  This is done by having the correspondence pointer point to the IL entry
itself.  In contrast, a NULL correspondence pointer indicates that the entry
has not yet been examined for a matching entry in another translation unit.
*/
{
  a_trans_unit_corresp_ptr  *tcp;

  trace_corresp_check(entity);
#if DEBUG
/* GCOVR_EXCL_START */
  if (kind != (an_il_entry_kind)iek_base_class &&
      db_trace("trans_corresp", entity, kind)) {
    db_scp(entity);
    fprintf(f_debug, " has no correspondence.\n");
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  tcp = detach_trans_unit_corresp(kind, entity);
  if (*tcp == NULL) {
    /* Allocate a correspondence node. */
    *tcp = alloc_trans_unit_corresp();
    (*tcp)->kind = kind;
    ++(*tcp)->count;
  } else {
    a_boolean is_template = FALSE;
    /* Reuse the correspondence entry.  (Normally, the entry shouldn't be
       shared.  An exception is the sharing by two template entries that are
       in the same translation unit.  Correspondence errors can also lead
       to unexpected sharing, but may not be diagnosed until the verification
       stage.) */
    if (kind == iek_template ||
        (kind == iek_variable &&
         ((a_variable_ptr)entity)->is_template_variable)) {
      is_template = TRUE;
    }  /* if */
    if (!is_template && (*tcp)->count > 1) {
      expect_error_str("f_set_no_trans_unit_corresp: correspondence busy");
    }  /* if */
  }  /* if */
  change_canonical_entry(*tcp, entity);
  if (!in_secondary_trans_unit(entity)) {
    (*tcp)->primary = entity;
  }  /* if */
}  /* f_set_no_trans_unit_corresp */

#define set_no_trans_unit_corresp(kind, ptr)                                \
  f_set_no_trans_unit_corresp((an_il_entry_kind)kind, (char*)(ptr))


static void f_set_unvisited_trans_unit_corresp(
                                           ARG_UNUSED an_il_entry_kind kind,
                                           char                        *entity)
/*
Detach the given IL entity from a translation unit correspondence entry
and free the correspondence entry.
*/
{
  a_trans_unit_corresp_ptr  tcp = trans_unit_corresp_of_unknown_entry(entity);

  if (tcp != NULL) {
    trace_corresp_check(entity);
    (void)detach_trans_unit_corresp(kind, entity);
    check_assertion(tcp->count == 1 && tcp->kind == kind);
    free_trans_unit_corresp(tcp);
    trans_unit_corresp_of_unknown_entry(entity) = NULL;
  }  /* if */
}  /* f_set_unvisited_trans_unit_corresp */

#define set_unvisited_trans_unit_corresp(kind, ptr)                         \
  f_set_unvisited_trans_unit_corresp((an_il_entry_kind)(kind), (char*)ptr)


/*lint -emacro(506,clear_trans_unit_corresp)*/
#define clear_trans_unit_corresp(kind, ptr, visited)                        \
  ((visited) ? set_no_trans_unit_corresp(kind, ptr)                         \
             : set_unvisited_trans_unit_corresp(kind, ptr))


static void report_corresp_error(char                   *entity1,
                                 a_source_position_ptr  pos2,
                                 an_error_code          same_src_error,
                                 an_error_code          distinct_src_error)
/*
The given IL node has a source correspondence and an associated symbol.  It
conflicts in some way with an entity declared at pos2.  If the two conflicting
IL identities result from the same source construct (e.g., because the same
header file was included in two translation units), use the message associated
with same_src_error; otherwise, use distinct_src_error.
*/
{
  unsigned long  saved_total_errors = diagnostic_counters.total.errors;
  a_symbol_ptr   sym = (a_symbol_ptr)((a_source_correspondence_ptr)entity1)
                                                                  ->assoc_info;
  a_source_position_ptr
                 pos1 = &sym->decl_position;
  a_line_number  line1, line2;
  a_boolean      at_end_of_source;
  a_source_file_ptr
                 src_file1 = source_file_for_seq(
                                pos1->seq, &line1, &at_end_of_source,
                                /*physical_line=*/TRUE),
                 src_file2 = source_file_for_seq(
                                pos2->seq, &line2, &at_end_of_source,
                                /*physical_line=*/TRUE);

  if (src_file1 != NULL && src_file2 != NULL &&
      src_file1->full_name != NULL && src_file2->full_name != NULL &&
      strcmp(src_file1->full_name, src_file2->full_name) == 0) {
    /* The entities correspond to the same source construct, but resulted in
       incompatible IL (perhaps due to preprocessor effects). */
    if (find_prototype_diagnostic(same_src_error, es_error,
                                  &sym->decl_position)) {
      /* This diagnostic was produced already (presumably for a prototype
         instantiation, while this is a real instantiation). */
    } else {
      a_source_file_ptr  prim_file2 = primary_source_file_for_seq(pos2->seq);
      a_const_char       *file_copy;
      file_copy = format_source_file_name(prim_file2,
                                          /*use_name_as_written=*/TRUE,
                                          /*quote_file_name=*/FALSE);
      file_copy = copy_string_to_region(FRONT_END_REGION_NUMBER, file_copy);
      pos_stsy_error(same_src_error, &sym->decl_position,
                     file_copy, sym);
      record_prototype_diagnostic(same_src_error, es_error,
                                  &sym->decl_position);
    }  /* if */
  } else {
    /* The corresponding entities result from distinct source constructs. */
    if (find_prototype_diagnostic(distinct_src_error, es_error,
                                  &sym->decl_position)) {
      /* This diagnostic was produced already (presumably for a prototype
         instantiation, while this is a real instantiation). */
    } else {
      a_diagnostic_ptr dp;
      dp = pos_sy_start_error(distinct_src_error, &sym->decl_position, sym);
      add_diag_info_with_pos_insert(dp, ec_corresp_decl_at, pos2);
      end_diagnostic(dp);
      record_prototype_diagnostic(distinct_src_error, es_error,
                                  &sym->decl_position);
    }  /* if */
  }  /* if */
  corresp_errors += (diagnostic_counters.total.errors - saved_total_errors);
}  /* report_corresp_error */


static void f_report_bad_trans_unit_corresp(char                   *entity1,
                                            a_source_position_ptr  pos2)
/*
The given IL node has a source correspondence and an associated symbol.  It
also has a non-NULL translation unit correspondence, but it points to a node
that does not actually correspond to the given entity.  Therefore, issue a
diagnostic.
*/
{
  report_corresp_error(entity1, pos2,
                       ec_entity_differs_in_other_trans_unit,
                       ec_corresp_decl_incompatible);
}  /* f_report_bad_trans_unit_corresp */

#define report_bad_trans_unit_corresp(entity)                               \
  f_report_bad_trans_unit_corresp(                                          \
    (char*)(entity),                                                        \
    &((a_source_correspondence_ptr)canonical_il_entry_of(entity))           \
      ->decl_position)


static void f_process_bad_trans_unit_corresp(an_il_entry_kind  kind,
                                             char              *entity1,
                                             char              *entity2)
/*
Similar to report_bad_trans_unit_corresp but use the position of entity2
and clear the correspondence of entity1.
*/
{
  f_report_bad_trans_unit_corresp(entity1,
                                  &((a_source_correspondence_ptr)entity2)
                                                              ->decl_position);
  set_no_trans_unit_corresp(kind, entity1);
}  /* f_process_bad_trans_unit_corresp */

#define process_bad_trans_unit_corresp(kind, entity1, entity2)               \
  f_process_bad_trans_unit_corresp((an_il_entry_kind)kind,                   \
                                   (char*)(entity1), (char*)(entity2))


static void f_report_multiple_definitions(char                   *entity1,
                                          a_source_position_ptr  pos2)
/*
The given IL node has a source correspondence and an associated symbol.  It
also has a non-NULL translation unit correspondence.  Issue a diagnostic
reporting that both entries are definitions.
*/
{
  report_corresp_error(entity1, pos2,
                       ec_entity_defined_in_other_trans_unit,
                       ec_entity_defined_twice);
}  /* f_report_multiple_definitions */

#define report_multiple_definitions(entity)                               \
  f_report_multiple_definitions(                                          \
    (char*)(entity),                                                      \
    &((a_source_correspondence_ptr)canonical_il_entry_of(entity))         \
      ->decl_position)


static a_boolean same_parents(a_symbol_ptr  sym1,
                              a_symbol_ptr  sym2)
/*
Return TRUE if and only if the parent (namespace or class) entities of the
given symbols are identical.
*/
{
  a_boolean  result;

  if (sym1->is_class_member != sym2->is_class_member) {
    result = FALSE;
  } else if (sym1->is_class_member) {
    /* Two class members. */
    a_type_ptr  parent1 = sym_parent_class(sym1);
    a_type_ptr  parent2 = sym_parent_class(sym2);
    check_assertion(parent1 != NULL && parent2 != NULL);
    result = corresponding_types(parent1, parent2);
  } else {
    /* Members of namespaces (possibly global scope). */
    an_il_entry_kind             kind;
    a_source_correspondence_ptr  scp1, scp2;
    scp1 = (a_source_correspondence_ptr)il_entry_for_symbol_null_okay(
                                                                  sym1, &kind);
    scp2 = (a_source_correspondence_ptr)il_entry_for_symbol_null_okay(
                                                                  sym2, &kind);
    /* Anonymous union members can have a source correspondence with the flag
       is_class_member set to TRUE while the corresponding flag in the symbol
       is FALSE. */
    if (scp1 != NULL && scp2 != NULL &&
        scp1->is_class_member != scp2->is_class_member) {
      result = FALSE;
    } else if (scp1 != NULL && scp2 != NULL && scp1->is_class_member) {
      a_type_ptr  parent1 = sym_parent_class(sym1);
      a_type_ptr  parent2 = sym_parent_class(sym2);
      check_assertion(parent1 != NULL && parent2 != NULL);
      result = corresponding_types(parent1, parent2);
    } else {
      a_namespace_ptr  parent1 = sym_parent_namespace_or_null(sym1);
      a_namespace_ptr  parent2 = sym_parent_namespace_or_null(sym2);
      /* Block-extern declarations can have a NULL parent pointer in the symbol
         entry even though they declared an entity in a namespace.  Use the IL
         entry in those cases.  (See function add_namespace_parent_pointer.) */
      if (parent1 == NULL && scp1 != NULL) {
        parent1 = scp_parent_namespace_or_null(scp1);
      }  /* if */
      if (parent2 == NULL && scp2 != NULL) {
        parent2 = scp_parent_namespace_or_null(scp2);
      }  /* if */
      if (parent1 != NULL || parent2 != NULL) {
        if (!microsoft_bugs && scp1 != NULL && scp2 != NULL &&
            scp1->name_linkage == (a_name_linkage_kind)nlk_external &&
            scp2->name_linkage == (a_name_linkage_kind)nlk_external) {
          /* extern "C" entities match even if they are declared in different
             namespaces.  (But not in Microsoft bugs mode.) */
          result = TRUE;
        } else {
          result = corresponding_namespaces(parent1, parent2);
        }  /* if */
      } else {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* same_parents */


static a_boolean known_same_parents(a_symbol_ptr  sym1,
                                    a_symbol_ptr  sym2)
/*
Return TRUE if and only if the parent (namespace or class) entities of the
given symbols have the same canonical entry.  (This differs from "same_parents"
in that no attempt is made to establish a canonical entry for the parents.)
*/
{
  a_boolean  result;

  if (sym1->is_class_member != sym2->is_class_member) {
    result = FALSE;
  } else if (sym1->is_class_member) {
    result = (canonical_il_entry_of(sym_parent_class(sym1)) ==
                               canonical_il_entry_of(sym_parent_class(sym2)));
  } else {
    result = (canonical_il_entry_of(sym_parent_namespace_or_null(sym1)) ==
                   canonical_il_entry_of(sym_parent_namespace_or_null(sym2)));
  }  /* if */
  return result;
}  /* known_same_parents */


static a_boolean f_il_entries_have_known_same_parents(
                                            a_source_correspondence_ptr  scp1,
                                            a_source_correspondence_ptr  scp2)
/*
This function performs a similar task for IL entries as known_same_parents
does for symbol entries, but in the case of namespace members it does
establish correspondences if needed.
*/
{
  a_boolean  result;

  if (scp1->is_class_member != scp2->is_class_member) {
    result = FALSE;
  } else if (scp1->is_class_member) {
    result = same_entities(scp_parent_class(scp1),
                           scp_parent_class(scp2));
  } else if (scp1->enclosing_routine != NULL) {
    if (scp2->enclosing_routine == NULL) {
      result = FALSE;
    } else {
      result = corresponding_routines(scp1->enclosing_routine,
                                      scp2->enclosing_routine);
    }  /* if */
  } else {
    a_namespace_ptr ns1 = scp_parent_namespace_or_null(scp1),
                    ns2 = scp_parent_namespace_or_null(scp2);
    if (ns1 == NULL) {
      result = ns2 == NULL;
    } else {
      result = corresponding_namespaces(ns1, ns2);
    }  /* if */
  }  /* if */
  return result;
}  /* f_il_entries_have_known_same_parents */

#define il_entries_have_known_same_parents(ptr1, ptr2)                      \
  f_il_entries_have_known_same_parents((a_source_correspondence_ptr)ptr1,   \
                                       (a_source_correspondence_ptr)ptr2)

static a_boolean may_have_correspondence(a_symbol_ptr sym)
/*
Return TRUE if the given symbol is associated with an entity that may appear
in multiple translation units so that correspondence pointers between them
need to be determined.
*/
{
  a_boolean        result = FALSE;

  switch (sym->kind) {
    case sk_class_or_struct_tag:
    case sk_enum_tag:
    case sk_union_tag:
      if (C_mode()) {
        /* In C mode structs, unions and enums will be matched up if they're
           involved in the declaration of variables or routines with linkage.
           Assume they may have a correspondence at first. */
        result = TRUE;
      } else {
        an_il_entry_kind             kind;
        a_source_correspondence_ptr  scp;
        scp = (a_source_correspondence_ptr)il_entry_for_symbol_null_okay(
                                                                   sym, &kind);
        if (scp != NULL &&
            (scp->name_linkage == nlk_external ||
             scp->name_linkage == nlk_cplusplus_external)) {
          result = TRUE;
        } else {
          result = FALSE;
        }  /* if */
      }
      break;
    case sk_class_template:
    case sk_constant:
    case sk_field:
    case sk_function_template:
    case sk_member_function:
    case sk_namespace:
    case sk_routine:
    case sk_static_data_member:
    case sk_variable:
    case sk_variable_template:
    case sk_concept_template:
      {
        /* These entities can have correspondences if they have external
           linkage. */
        an_il_entry_kind             kind;
        a_source_correspondence_ptr  scp;
        scp = (a_source_correspondence_ptr)il_entry_for_symbol_null_okay(
                                                                   sym, &kind);
        if (scp != NULL &&
            (scp->name_linkage == nlk_external ||
             scp->name_linkage == nlk_cplusplus_external)) {
          result = TRUE;
        } else {
          result = FALSE;
        }  /* if */
      }
      break;
    case sk_extern_routine:
    case sk_extern_variable:
      {
        /* These entities can have correspondences if they have external
           linkage. */
        a_source_correspondence_ptr  scp;
        if (is_function_type(sym->variant.extern_symbol_descr->type)) {
          scp = &sym->variant.extern_symbol_descr
                    ->variant.routine.ptr->source_corresp;
        } else {
          scp = &sym->variant.extern_symbol_descr
                    ->variant.variable->source_corresp;
        }  /* if */
        if ((scp->name_linkage == nlk_external ||
             scp->name_linkage == nlk_cplusplus_external)) {
          result = TRUE;
        } else {
          result = FALSE;
        }  /* if */
      }
      break;
    case sk_keyword:
    case sk_label:
    case sk_macro:
    case sk_projection:
    case sk_namespace_projection:
    case sk_undefined:
      result = FALSE;
      break;
    case sk_type:
      {
        a_type_ptr  type = type_symbol_type(sym);
        result = FALSE;
        if (type->kind == (a_type_kind)tk_typeref &&
            typeref_is_typedef(type)) {
          /* A typedef of an unnamed class or enum has linkage. */
          type = skip_typerefs(type);
          result = (is_immediate_class_type(type) &&
                    type->variant.class_struct_union.originally_unnamed) ||
                   (is_immediate_enum_type(type) &&
                    type->variant.integer.originally_unnamed);
        }  /* if */
      }
      break;
    case sk_overloaded_function:
      result = FALSE;
      {
        /* If any member of the overload set may have a correspondence,
           the whole set should be treated as such. */
        a_symbol_ptr  sub_sym = sym->variant.overloaded_function.symbols;
        for (; sub_sym != NULL; sub_sym = sub_sym->next) {
          if (may_have_correspondence(sub_sym)) {
            result = TRUE;
            break;
          }  /* if */
        }  /* for */
      }
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case sk_property_set:
      result = FALSE;
      { /* If any member of the overload set may have a correspondence,
           the whole set should be treated as such. */
        a_symbol_ptr  sub_sym = sym->variant.property_info->properties;
        for (; sub_sym != NULL; sub_sym = sub_sym->next) {
          if (may_have_correspondence(sub_sym)) {
            result = TRUE;
            break;
          }  /* if */
        }  /* for */
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
    case sk_named_address_space:
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
    default:
      unexpected_condition_str("may_have_correspondence: bad symbol kind");
  }  /* switch */
  return result;
}  /* may_have_correspondence */


static void set_builtin_type_corresp(a_type_ptr  *record,
                                     a_type_ptr  type)
/*
*record holds a record of the canonical builtin type equivalent to the given
type.  If *record is NULL, the type is to become that canonical type.
Otherwise, type's correspondence should be set to *record.
*/
{
  if (*record == NULL) {
    *record = type;
    if (in_secondary_trans_unit(type)) {
      set_no_trans_unit_corresp(iek_type, type);
    }  /* if */
  } else {
    set_trans_unit_corresp(iek_type, type, *record);
  }  /* if */
}  /* set_builtin_type_corresp */


void record_builtin_type(a_type_ptr  type)
/*
This routine is called whenever a basic builtin type is created.  If this is
the first time this particular type is created, the type is recorded in an
array so that it can be found when an equivalent type is created in a future
translation unit.  If the type had already been created, its correspondence
is set to point to the first created type.
*/
{
  switch (type->kind) {
    case tk_void:
      set_builtin_type_corresp(&canonical_il_void_type, type);
      break;
    case tk_reflection:
      set_builtin_type_corresp(&canonical_il_reflection_type, type);
      break;
    case tk_integer:
      if (type->variant.integer.bool_type) {
        set_builtin_type_corresp(&canonical_il_bool_type, type);
      } else if (type->variant.integer.wchar_t_type) {
        set_builtin_type_corresp(&canonical_il_wchar_t_type, type);
      } else if (type->variant.integer.char8_t_type) {
        set_builtin_type_corresp(&canonical_il_char8_t_type, type);
      } else if (type->variant.integer.char16_t_type) {
        set_builtin_type_corresp(&canonical_il_char16_t_type, type);
      } else if (type->variant.integer.char32_t_type) {
        set_builtin_type_corresp(&canonical_il_char32_t_type, type);
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (type->variant.integer.microsoft_sized_int_type) {
        if (type->variant.integer.explicitly_signed) {
          set_builtin_type_corresp(
               &canonical_microsoft_sized_signed_int_types[
                                              type->variant.integer.int_kind],
               type);
        } else {
          set_builtin_type_corresp(
               &canonical_microsoft_sized_int_types[
                                              type->variant.integer.int_kind],
               type);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        if (type->variant.integer.explicitly_signed) {
          set_builtin_type_corresp(
                  &canonical_signed_int_types[type->variant.integer.int_kind],
                  type);
        } else {
          set_builtin_type_corresp(
                  &canonical_int_types[type->variant.integer.int_kind], type);
        }  /* if */
      }  /* if */
      break;
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
      set_builtin_type_corresp(
        &canonical_fixed_point_types[(int)type->variant.fixed_point.precision]
                                    [type->variant.fixed_point.is_unsigned]
                                    [type->variant.fixed_point.is_fract_type]
                                    [type->variant.fixed_point.saturating],
        type);
      break;
#endif /* FIXED_POINT_ALLOWED */
    case tk_float:
      set_builtin_type_corresp(
                      &canonical_float_types[type->variant.float_kind], type);
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
      set_builtin_type_corresp(
                  &canonical_imaginary_types[type->variant.float_kind], type);
      break;
    case tk_complex:
      set_builtin_type_corresp(
                    &canonical_complex_types[type->variant.float_kind], type);
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_nullptr:
      if (type->incomplete) {
        set_builtin_type_corresp(&canonical_il_managed_nullptr_type, type);
      } else {
        set_builtin_type_corresp(&canonical_il_standard_nullptr_type, type);
      }  /* if */
      break;
    default:
      unexpected_condition_str("record_builtin_type: bad type kind");
  }  /* switch */
}  /* record_builtin_type */


a_type_ptr primary_int_type(an_integer_kind  kind)
/*
Return the integer type entry of the given kind used in the primary translation
unit IL, or NULL if the type hasn't been used in the primary IL.  This routine
takes into account the possibility that the trans_copy process (which must have
completed) created such an entry as a result of copying an entry into the
primary IL.
*/
{
  a_type_ptr  result = canonical_int_types[kind];

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_int_type */


a_type_ptr primary_signed_int_type(an_integer_kind  kind)
/*
Return the explicitly signed integer type entry of the given kind used in the
primary translation unit IL, or NULL if the type hasn't been used in the
primary IL.  This routine takes into account the possibility that the
trans_copy process (which must have completed) created such an entry as a
result of copying an entry into the primary IL.
*/
{
  a_type_ptr  result = canonical_signed_int_types[kind];

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_signed_int_type */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_type_ptr primary_microsoft_sized_int_type(an_integer_kind  kind)
/*
Return the sized integer type (__intN) entry of the given kind used in the
primary translation unit IL, or NULL if the type hasn't been used in the
primary IL.  This routine takes into account the possibility that the
trans_copy process (which must have completed) created such an entry as a
result of copying an entry into the primary IL.
*/
{
  a_type_ptr  result = canonical_microsoft_sized_int_types[kind];

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_microsoft_sized_int_type */


a_type_ptr primary_microsoft_sized_signed_int_type(an_integer_kind  kind)
/*
Return the explicitly signed sized integer type (signed __intN) entry of the
given kind used in the primary translation unit IL, or NULL if the type hasn't
been used in the primary IL.  This routine takes into account the possibility
that the trans_copy process (which must have completed) created such an entry
as a result of copying an entry into the primary IL.
*/
{
  a_type_ptr  result = canonical_microsoft_sized_signed_int_types[kind];

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_microsoft_sized_signed_int_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_type_ptr primary_wchar_t_type(void)
/*
Return the wchar_t type entry used in the primary translation unit IL, or NULL
if the type hasn't been used in the primary IL.  This routine takes into
account the possibility that the trans_copy process (which must have completed)
created such an entry as a result of copying an entry into the primary IL.
*/
{
  a_type_ptr  result = canonical_il_wchar_t_type;

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_wchar_t_type */


a_type_ptr primary_char8_t_type(void)
/*
Return the char8_t type entry used in the primary translation unit IL, or NULL
if the type hasn't been used in the primary IL.  This routine takes into
account the possibility that the trans_copy process (which must have completed)
created such an entry as a result of copying an entry into the primary IL.
*/
{
  a_type_ptr  result = canonical_il_char8_t_type;

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_char8_t_type */


a_type_ptr primary_char16_t_type(void)
/*
Return the char16_t type entry used in the primary translation unit IL, or NULL
if the type hasn't been used in the primary IL.  This routine takes into
account the possibility that the trans_copy process (which must have completed)
created such an entry as a result of copying an entry into the primary IL.
*/
{
  a_type_ptr  result = canonical_il_char16_t_type;

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_char16_t_type */


a_type_ptr primary_char32_t_type(void)
/*
Return the char32_t type entry used in the primary translation unit IL, or NULL
if the type hasn't been used in the primary IL.  This routine takes into
account the possibility that the trans_copy process (which must have completed)
created such an entry as a result of copying an entry into the primary IL.
*/
{
  a_type_ptr  result = canonical_il_char32_t_type;

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_char32_t_type */


a_type_ptr primary_managed_nullptr_type(void)
/*
Return the C++/CLI managed type entry used in the primary translation unit
IL, or NULL if the type hasn't been used in the primary IL.  This routine
takes into account the possibility that the trans_copy process (which must
have completed) created such an entry as a result of copying an entry into
the primary IL.
*/
{
  a_type_ptr  result = canonical_il_managed_nullptr_type;

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_managed_nullptr_type */


a_type_ptr primary_standard_nullptr_type(void)
/*
Return the std::nullptr_t type entry used in the primary translation unit
IL, or NULL if the type hasn't been used in the primary IL.  This routine
takes into account the possibility that the trans_copy process (which must
have completed) created such an entry as a result of copying an entry into
the primary IL.
*/
{
  a_type_ptr  result = canonical_il_standard_nullptr_type;

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_standard_nullptr_type */


a_type_ptr primary_float_type(a_float_kind  kind)
/*
Return the float type entry of the given kind used in the primary translation
unit IL, or NULL if the type hasn't been used in the primary IL.  This routine
takes into account the possibility that the trans_copy process (which must have
completed) created such an entry as a result of copying an entry into the
primary IL.
*/
{
  a_type_ptr  result = canonical_float_types[kind];

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_float_type */

#if C99_IL_EXTENSIONS_SUPPORTED

a_type_ptr primary_bool_type(void)
/*
Return the bool type entry used in the primary translation unit IL, or NULL
if the type hasn't been used in the primary IL.  This routine takes into
account the possibility that the trans_copy process (which must have completed)
created such an entry as a result of copying an entry into the primary IL.
*/
{
  a_type_ptr  result = canonical_il_bool_type;

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_bool_type */


a_type_ptr primary_complex_type(a_float_kind  kind)
/*
Return the complex type entry of the given kind used in the primary translation
unit IL, or NULL if the type hasn't been used in the primary IL.  This routine
takes into account the possibility that the trans_copy process (which must have
completed) created such an entry as a result of copying an entry into the
primary IL.
*/
{
  a_type_ptr  result = canonical_complex_types[kind];

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_complex_type */


a_type_ptr primary_imaginary_type(a_float_kind  kind)
/*
Return the imaginary type entry of the given kind used in the primary
translation unit IL, or NULL if the type hasn't been used in the primary IL.
This routine takes into account the possibility that the trans_copy process
(which must have completed) created such an entry as a result of copying an
entry into the primary IL.
*/
{
  a_type_ptr  result = canonical_imaginary_types[kind];

  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_imaginary_type */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if FIXED_POINT_ALLOWED

a_type_ptr primary_fixed_point_type(
                                 a_fixed_point_type_descr descr/*lint !e1746*/)
/*
Return the fixed-point type entry of the given kind used in the primary
translation unit IL, or NULL if the type hasn't been used in the primary IL.
This routine takes into account the possibility that the trans_copy process
(which must have completed) created such an entry as a result of copying an
entry into the primary IL.
*/
{
  a_type_ptr  result = canonical_fixed_point_types[descr.precision]
                                                  [(int)descr.is_unsigned]
                                                  [(int)descr.is_fract_type]
                                                  [(int)descr.saturating];
  if (result != NULL) {
    result = (a_type_ptr)canonical_il_entry_of(result);
    check_assertion(!in_secondary_trans_unit(result));
  }  /* if */
  return result;
}  /* primary_fixed_point_type */

#endif /* FIXED_POINT_ALLOWED */

static a_field_ptr skip_generated_field(a_field_ptr  field)
/*
If the given field was generated by the front end (e.g., a virtual function
table pointer), skip to the next field that was actually declared in the
source.  Fields that represent anonymous union objects are not skipped (even
though in some sense they are "generated").
*/
{
  while (field != NULL && field->compiler_generated &&
         !field->is_anonymous_parent_object) {
    field = field->next;
  }  /* while */
  return field;
}  /* skip_generated_field */


static a_routine_ptr skip_generated_routine(a_routine_ptr  routine)
/*
If the given routine was generated by the front end, skip to the next routine
that was actually declared in the source.  Here a routine generated from a
function or member function template is considered "generated", but a member
of a class template is not.  (The point is that "generated" routines do not
always appear in the same order on the routines list of a class scope.  Hence
explicit member template specializations are also considered to be "generated"
except for in-class specialization.)
*/
{
  while (routine != NULL && (
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
         /* Skip covariant routines generated during lowering and generated
            class template deduction guides. */
         (routine->compiler_generated &&
          (special_kind_is(routine, sfk_deduction_guide) ||
           routine->overridden_function_for_wrapper != NULL)) ||
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if MICROSOFT_EXTENSIONS_ALLOWED
         /* Interface slots are generated for derived classes, and can
            therefore differ from one derivation to another. */
         routine->interface_slot ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
         /* Ordinary members of template classes have a NULL template argument
            list. */ 
          (routine->is_template_function &&
           !routine->is_in_class_specialization &&
           routine->template_arg_list != NULL))) {
    routine = routine->next;
  }  /* while */
  return routine;
}  /* skip_generated_routine */


static an_il_entity_list_entry_ptr skip_generated_friend_routine(
                                            an_il_entity_list_entry_ptr  ielep)
/*
Similar to skip_generated_routine except the routines are listed through
an_il_entity_list_entry_ptr nodes with other entity kinds mixed in.
*/
{
  while (ielep != NULL && (ielep->entity.kind != iek_routine || (
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
         /* Skip covariant routines generated during lowering. */
          (((a_routine_ptr)(ielep->entity.ptr))->compiler_generated &&
           ((a_routine_ptr)(ielep->entity.ptr))->
                                    overridden_function_for_wrapper != NULL) ||
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
         /* Ordinary members of template classes have a NULL template argument
            list. */ 
          (((a_routine_ptr)(ielep->entity.ptr))->is_template_function &&
           (((a_routine_ptr)(ielep->entity.ptr))->template_arg_list !=
            NULL))))) {
    ielep = ielep->next;
  }  /* while */
  return ielep;
}  /* skip_generated_friend_routine */


#if DO_IL_LOWERING
#define class_type_is_compiler_generated(tp)                           \
  (class_type_supp(tp)->compiler_generated)
#else /* !DO_IL_LOWERING */
#define class_type_is_compiler_generated(tp) FALSE /*lint --e(506)*/
#endif /* DO_IL_LOWERING */

/*
Return TRUE if this class is an instance of a template (the template_arg_list
check excludes nontemplate members of template instances) that is not a
prototype instantiation.
*/
#define class_type_is_nonprototype_template_instance(tp)               \
  ((tp)->variant.class_struct_union.is_template_class &&               \
   !(tp)->variant.class_struct_union.is_prototype_instantiation &&     \
   class_type_supp(tp)->template_arg_list != NULL)

/*
Return TRUE for "generated" classes: These can differ from one translation
unit to another.
*/
#define is_generated_class_type(tp)                                    \
  (is_immediate_class_type(tp) &&                                      \
   (class_type_is_compiler_generated(tp) ||                            \
    class_type_supp(tp)->is_lambda_closure_class ||                    \
    class_type_is_nonprototype_template_instance(tp))) 

/*
Return TRUE for "generated" aliases: These can differ from one translation
unit to another.
*/
#define is_generated_alias(tp)                                         \
  ((tp)->kind == (a_type_kind)tk_typeref &&                            \
   is_typeref_kind((tp), trk_is_template_alias) &&                     \
   !(tp)->variant.typeref.is_prototype_instantiation)
/*
Return TRUE for "generated" types: These can differ from one translation
unit to another.
*/
#define is_generated_type(tp)                                          \
  (is_generated_class_type(tp) ||                                      \
   is_generated_alias(tp))

static a_type_ptr skip_generated_type(a_type_ptr  type)
/*
If the given type was generated by the front end, skip to the next type
that was actually declared in the source.  Here a type generated from a
class or member class template is considered "generated", but a member
of a class template is not.  Generated types also include closure types.
(The point is that "generated" types do not always appear in the same
order on the types list of a class scope.)
*/
{
  a_type_ptr  result = type;

  while (result != NULL && is_generated_type(result)) {
    result = result->next;
  }  /* while */
  return result;
}  /* skip_generated_type */


static an_il_entity_list_entry_ptr skip_generated_friend_class(
                                              an_il_entity_list_entry_ptr  cle)
/*
Similar to skip_generated_type, except the types are list through
an_il_entity_list_entry_ptr nodes with other entity kinds mixed in.
*/
{
  an_il_entity_list_entry_ptr  result = cle;

  while (result != NULL &&
         (!(result->entity.kind == iek_type &&
            is_class_struct_union_type((a_type_ptr)result->entity.ptr)) || (
#if DO_IL_LOWERING
         /* Some types are generated as part of prelowering. */
         (is_immediate_class_type((a_type_ptr)result->entity.ptr) &&
          (((a_type_ptr)result->entity.ptr)->variant.class_struct_union.
                                                         extra_info != NULL) &&
          (((a_type_ptr)result->entity.ptr)->variant.class_struct_union.
                                            extra_info->compiler_generated)) ||
#endif /* DO_IL_LOWERING */
          /* Nonprototype instantiations can differ from one translation unit
             to another.  (The check on template_arg_list ensures that we
             only skip actual instantiations as opposed to members of
             instantiations.) */
         (is_immediate_class_type((a_type_ptr)result->entity.ptr) &&
          (((a_type_ptr)result->entity.ptr)->variant.class_struct_union.
                                                          is_template_class) &&
          !((a_type_ptr)result->entity.ptr)
                    ->variant.class_struct_union.is_prototype_instantiation &&
          ((a_type_ptr)result->entity.ptr)->variant.class_struct_union.
                                    extra_info->template_arg_list != NULL)))) {
    result = result->next;
  }  /* while */
  return result;
}  /* skip_generated_friend_class */


static void add_instantiation(a_template_symbol_supplement_ptr  tssp,
                              a_symbol_ptr                      inst)
/*
Add the given instantiation symbol to the list of all instantiations
associated with tssp.  If tssp is associated with a partial specialization,
its corresponding primary template supplement will be used instead.
*/
{
  a_symbol_list_entry_ptr  slep = alloc_symbol_list_entry();
  a_template_ptr           templ = tssp->il_template_entry;

  if (templ != NULL) {
    /* This could be a partial specialization: retrieve the primary template
       to access its all_instantiations list. */
    a_symbol_ptr  templ_sym;
    templ = canonical_template_entry_of(templ);
    templ_sym = (a_symbol_ptr)templ->source_corresp.assoc_info;
    if (is_type_symbol(inst)) {
      templ_sym = primary_template_of(templ_sym);
    }  /* if */
    tssp = templ_sym->variant.template_info;
  }  /* if */
  /* Insert the given instantiation in the list. */
  slep->next = tssp->all_instantiations;
  tssp->all_instantiations = slep;
  slep->symbol = inst;
#if DEBUG
/* GCOVR_EXCL_START */
  if (db_sym_trace("trans_corresp", inst)) {
    a_line_number  line;
    a_const_char   *file_name, *full_name;
    a_boolean      at_end_of_source;
    fprintf(f_debug, "Adding ");
    db_symbol_name(inst);
    fprintf(f_debug, " (%s) to all_instantiations list for ",
            symbol_kind_names[(int)inst->kind]);
    if (tssp->il_template_entry != NULL) {
      a_symbol_ptr   templ_sym = (a_symbol_ptr)tssp->il_template_entry
                                                   ->source_corresp.assoc_info;
      db_symbol_name(templ_sym);
      (void)conv_seq_to_file_and_line(templ_sym->decl_position.seq, &file_name,
                                      &full_name, &line, &at_end_of_source);
      if (line != 0) {
        fprintf(f_debug, " in file %s (line %lu)\n", file_name,
                (unsigned long)line);
      } else {
        fprintf(f_debug, " (built-in; line %lu)\n", (unsigned long)line);
      }  /* if */
    } else {
      fprintf(f_debug, "unknown symbol\n");
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
}  /* add_instantiation */


static void clear_instantiation_correspondences(a_template_ptr  templ,
                                                a_boolean       visited)
/*
Mark all instantiations associated with the given template as having no
correspondences.  If visited is TRUE, also record those instantiations on the
all_instantiations list of the associated template symbol supplement.
*/
{
  a_symbol_ptr  templ_sym = symbol_for(templ);
  a_template_symbol_supplement_ptr
                tssp = template_supplement_for_symbol(templ_sym);
  if (!is_template_symbol(templ_sym) || templ_sym->is_template_param) {
    /* Nontemplate member of class template or template template parameter:
       no instantiations to mark. */
  } else if (is_class_template_symbol(templ_sym)) {
    a_symbol_list_entry_ptr slep;
    a_symbol_ptr            inst;
    a_symbol_ptr            proto;
    a_type_ptr              class_type;
    proto = tssp->variant.class_template.prototype_instantiation;
    /* Process the prototype instantiation first. */
    if (proto != NULL) {
      class_type = type_symbol_type(proto);
      clear_type_correspondence(class_type, visited);
      if (visited) {
        add_instantiation(tssp, proto);
      }  /* if */
    }  /* if */
    for (slep = tssp->variant.class_template.instantiations;
         slep != NULL; slep = slep->next) {
      inst = slep->symbol;
      if (inst != proto) {
        /* Sometimes the prototype instantiation is placed on the
           instantiations list; skip it since it has been processed above. */
        class_type = type_symbol_type(inst);
        clear_type_correspondence(class_type, visited);
        if (visited && find_class_template_instantiation(tssp, inst) == NULL) {
          add_instantiation(tssp, inst);
        }  /* if */
      }  /* if */
    }  /* for */
  } else if (symbol_is(templ_sym, sk_variable_template)) {
    a_symbol_list_entry_ptr slep;
    a_variable_ptr          proto;
    /* Process the prototype instantiation first. */
    proto = tssp->variant.variable.prototype_variable;
    if (proto != NULL) {
      clear_trans_unit_corresp(iek_variable, proto, visited);
      if (visited) {
        add_instantiation(tssp, symbol_for(proto));
      }  /* if */
    }  /* if */
    for (slep = tssp->variant.variable.instantiations;
         slep != NULL; slep = slep->next) {
      a_symbol_ptr    inst = slep->symbol;
      a_variable_ptr  vp;
      if (!symbol_is(inst, sk_variable)) continue;
      vp = inst->variant.variable.ptr;
      if (vp != proto) {
        /* Sometimes the prototype instantiation is placed on the
           instantiations list; skip it since it has been processed above. */
        clear_trans_unit_corresp(iek_variable, vp, visited);
        if (visited) {
          add_instantiation(tssp, inst);
        }  /* if */
      }  /* if */
    }  /* for */
  } else if (symbol_is(templ_sym, sk_concept_template)) {
    /* Concept templates have no associated instantiations.  Nothing to do. */
  } else {
    /* A function template. */
    a_template_instance_ptr  inst = tssp->variant.function.instantiations;
    a_routine_ptr            proto = tssp->variant.function.routine;
    /* Process the prototype instantiation first. */
    if (proto != NULL) {
      clear_trans_unit_corresp(iek_routine, proto, visited);
      if (visited) {
        add_instantiation(tssp, symbol_for(proto));
      }  /* if */
    }  /* if */
    for (; inst != NULL; inst = inst->next) {
      a_routine_ptr   routine = inst->instance_sym->variant.routine.ptr;
      if (routine != proto) {
        clear_trans_unit_corresp(iek_routine, routine, visited);
        if (visited) {
          add_instantiation(tssp, inst->instance_sym);
        }  /* if */
      }  /* if */
    }  /* for */
    /* Handle the prototype instantiation. */
    clear_trans_unit_corresp(iek_routine, tssp->variant.function.routine,
                             visited);
  }  /* if */
}  /* clear_instantiation_correspondences */


static void clear_enum_type_correspondence(a_type_ptr  type,
                                           a_boolean   visited)
/*
Clear the correspondence pointers in the substructure of an enum type.
*/
{
  a_constant_ptr  enumerator = enum_constants(type);

  for (; enumerator != NULL; enumerator = enumerator->next) {
    clear_trans_unit_corresp(iek_constant, enumerator, visited);
  }  /* for */
}  /* clear_enum_type_correspondence */

#define set_no_enum_type_correspondence(type)                       \
  clear_enum_type_correspondence((type), /*visited=*/TRUE)


static void clear_class_type_correspondence(a_type_ptr  type,
                                            a_boolean   visited)
/*
Clear the correspondence pointers in the substructure of a class type.
*/
{
  if (class_type_has_body(type)) {
    /* Traverse fields: (both C and C++) */
    a_field_ptr  field = type->variant.class_struct_union.field_list;
    for (; field != NULL; field = field->next) {
      clear_trans_unit_corresp(iek_field, field, visited);
    }  /* for */
  
    if (!C_mode()) {
      /* Traverse entities only available in C++ mode. */
      a_scope_ptr  scope = type->
                           variant.class_struct_union.extra_info->assoc_scope;
      a_base_class_ptr  base = type->variant.class_struct_union.extra_info
                                                               ->base_classes;
      if (scope != NULL) {
        /* scope is only NULL for some types generated during the lowering
           of function bodies. */
        clear_scope_correspondence(scope, visited);
      }  /* if */
      for (; base != NULL; base = base->next) {
        clear_trans_unit_corresp(iek_base_class, base, visited);
      }  /* for */
    }  /* if */
  }  /* if */
}  /* clear_class_type_correspondence */

#define set_no_class_type_correspondence(type)                       \
  clear_class_type_correspondence((type), /*visited=*/TRUE)


static void clear_type_correspondence(a_type_ptr  type,
                                      a_boolean   visited)
/*
Clear the correspondence of the given type and (if applicable) its
substructure.
*/
{
  /* Mark this type as visited. */
  clear_trans_unit_corresp(iek_type, type, visited);
  /* Also mark inner structure if applicable. */
  if (is_immediate_class_type(type)) {
    clear_class_type_correspondence(type, visited);
  } else if (is_immediate_enum_type(type)) {
    clear_enum_type_correspondence(type, visited);
  }  /* if */
}  /* clear_type_correspondence */


static void mark_canonical_instantiation(a_template_symbol_supplement  *tssp,
                                         a_symbol_ptr                  inst)
/*
Record inst as being a canonical instantiation of the template associated with
tssp.
*/
{
  /* Put the instance on the appropriate all_instantiations list so it can be
     found when looking for cross-translation-unit correspondences. */
  add_instantiation(tssp, inst);
  /* Make sure that the instance has a correspondence that reflects the fact
     that this instance is the canonical entry. */
  if (is_class_struct_union_symbol(inst) ||
      symbol_is(inst, sk_type)) {
    a_type_ptr  type = type_symbol_type(inst);
    if (trans_unit_corresp_of(type) == NULL) {
      clear_type_correspondence(type, /*visited=*/TRUE);
    } else {
      check_assertion(canonical_il_entry_of(type) == (char*)type);
    }  /* if */
  } else if (is_function_symbol(inst)) {
    a_routine_ptr  routine = inst->variant.routine.ptr;
    if (trans_unit_corresp_of(routine) == NULL) {
      set_no_trans_unit_corresp(iek_routine, routine);
    } else {
      check_assertion(canonical_il_entry_of(routine) == (char*)routine);
    }  /* if */
  } else if (is_template_variable_symbol(inst)) {
    a_variable_ptr  var = variable_for_symbol(inst);
    if (trans_unit_corresp_of(var) == NULL) {
      set_no_trans_unit_corresp(iek_variable, var);
    } else {
      check_assertion(canonical_il_entry_of(var) == (char*)var);
    }  /* if */
  }  /* if */
}  /* mark_canonical_instantiation */


static void clear_namespace_correspondence(a_namespace_ptr  nsp,
                                           a_boolean        visited)
/*
Clear the correspondence pointers in the members of a namespace.
*/
{
  if (!nsp->is_namespace_alias) {
    clear_scope_correspondence(nsp->variant.assoc_scope, visited);
  }  /* if */
}  /* clear_namespace_correspondence */

#define set_no_namespace_correspondence(nsp)                       \
  clear_namespace_correspondence((nsp), /*visited=*/TRUE)


static void clear_scope_correspondence(a_scope_ptr  scope,
                                       a_boolean    visited)
/*
Mark the IL entries in the given scope as having no correspondence in other
translation units.  If visited is TRUE, the entries are marked as having been
visited; otherwise, they may yet be set to correspond to another entry.
*/
{
  /* Traverse namespaces: */
  {
    a_namespace_ptr  nsp = scope->namespaces;
    for (; nsp != NULL; nsp = nsp->next) {
      clear_namespace_correspondence(nsp, visited);
      clear_trans_unit_corresp(iek_namespace, nsp, visited);
    }  /* for */
  }

  /* Traverse templates: */
  {
    a_template_ptr  templ = scope->templates;
    for (; templ != NULL; templ = templ->next) {
      clear_trans_unit_corresp(iek_template, templ, visited);
      if (!templ->is_friend_template) {
        /* Avoid traversing friend template entries because that could lead to
           unbounded recursion in some cases. */
        clear_instantiation_correspondences(templ, visited);
      }  /* for */
    }  /* for */
  }

  /* Traverse types: */
  {
    a_type_ptr  type = scope->types;
    for (; type != NULL; type = type->next) {
      clear_type_correspondence(type, visited);
    }  /* for */
  }
  /* Traverse routines: */
  {
    a_routine_ptr  routine = scope->routines;
    for (;routine != NULL; routine = routine->next) {
      clear_trans_unit_corresp(iek_routine, routine, visited);
    }  /* for */
  }

  /* Traverse variables/static data members: */
  {
    a_variable_ptr  variable = scope->variables;
    for (; variable != NULL; variable = variable->next) {
      clear_trans_unit_corresp(iek_variable, variable, visited);
    }  /* for */
  }

  /* Traverse constants: */
  {
    a_constant_ptr  constant = scope->constants;
    for (; constant != NULL; constant = constant->next) {
      clear_trans_unit_corresp(iek_constant, constant, visited);
    }  /* for */
  }
}  /* clear_scope_correspondence */


a_boolean f_same_name(char  *entity1,
                      char  *entity2)
/*
Return whether the given entities have the same name (in most cases, their
associated symbols are listed under the same header).
*/
{
  a_boolean            match;
  a_source_correspondence_ptr
                       scp1 = (a_source_correspondence_ptr)entity1,
                       scp2 = (a_source_correspondence_ptr)entity2;
  a_symbol_ptr         sym1 = (a_symbol_ptr)scp1->assoc_info,
                       sym2 = (a_symbol_ptr)scp2->assoc_info;
  a_symbol_header_ptr  sh1, sh2;

  if (unmangled_or_fabricated_name_of(scp1) ==
      unmangled_or_fabricated_name_of(scp2)) {
    /* Note that in some cases one entity may already have a mangled name,
       while the other does not. */
    match = TRUE;
  } else if (sym1 == NULL || sym2 == NULL ||
             (!sym1->is_class_member && is_unnamed_tag_symbol(sym1)) || 
             (!sym2->is_class_member && is_unnamed_tag_symbol(sym2))) {
    /* A mismatch in which one of the entities is unnamed.  Unnamed classes
       and enums are considered to be matching if they are class members
       since in those cases the ordering of declarations is sufficient to
       ascertain a correspondence. */
    check_assertion(!(sym1 == NULL && sym2 == NULL));
    match = FALSE;
  } else {
    sh1 = sym1->header;
    sh2 = sym2->header;
    match = (sh1 == sh2);
    if (!match) {
      /* This is possible if the associated symbol is not part of the symbol
         table (which is TRUE of template instances). */
      match = (sh1->identifier_length == sh2->identifier_length) &&
              !strncmp(sh1->identifier, sh2->identifier,
                       (size_t)sh1->identifier_length);
    }  /* if */
  }  /* if */
  return match;
}  /* f_same_name */


static a_boolean f_verify_name_correspondence(char  *entity1)
/*
Verify that the given entity and the one pointed to by its translation unit
correspondence pointer have the same name (effectively, that their associated
symbols are listed under the same header).
*/
{
  char       *entity2 = canonical_il_entry_of(entity1);
  a_boolean  match = same_name(entity1, entity2);

  if (!match) {
    /* Only class members have a correspondence pointer set without testing
       whether the names match.  If the names don't match, an error
       contrasting the two member entities would not make much sense.
       Instead, report the error on the parent type.  Note that the IL entries
       for some template members may not indicate that they are class members;
       in those cases, the associated symbol entry should be examined.
       */
    a_source_correspondence_ptr  scp1 = (a_source_correspondence_ptr)entity1;
    a_source_correspondence_ptr  scp2 = (a_source_correspondence_ptr)entity2;
    a_symbol_ptr                 sym1 = (a_symbol_ptr)scp1->assoc_info;
    a_symbol_ptr                 sym2 = (a_symbol_ptr)scp2->assoc_info;
    if (scp1->is_class_member) {
      if (sym1 != NULL && sym2 != NULL &&
          sym1->kind == (a_symbol_kind)sk_member_function &&
          sym2->kind == (a_symbol_kind)sk_member_function &&
          sym1->variant.routine.ptr->special_kind ==
                                    (a_special_function_kind)sfk_conversion &&
          sym2->variant.routine.ptr->special_kind ==
                                    (a_special_function_kind)sfk_conversion) {
        /* Conversion functions don't really have names, so there is no need
           to check the name (a name is created for the entry, but it may
           involve typedef names that were not actually used in the declaration
           of the operator; this would cause a simple name comparison to fail
           even though the declarations were compatible). */
        match = TRUE;
      } else {
        a_type_ptr  parent_to_diagnose = scp_parent_class(scp1);
        if (parent_to_diagnose ==
                       (a_type_ptr)canonical_il_entry_of(parent_to_diagnose) &&
            scp2->is_class_member) {
          parent_to_diagnose = scp_parent_class(scp2);
        }  /* if */
        report_bad_trans_unit_corresp(parent_to_diagnose);
      }  /* if */
    } else {
      /* Normally, this happens only for certain template entries that
         represent members of templates (or friends of templates).  No
         diagnostic is issued here, because one will be issued on the
         prototype instantiation. */
#if CHECKING
      if (!(sym1->is_class_member ||
            sym1->kind == (a_symbol_kind)sk_member_function ||
            (sym1->kind == (a_symbol_kind)sk_routine &&
             rout_befriending_classes(sym1->variant.routine.ptr) != NULL))) {
#if DEBUG
/* GCOVR_EXCL_START */
        db_sym(sym1);
        db_sym(sym2);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        unexpected_condition_str("Expected a class member");
      }  /* if */
#endif /* CHECKING */
      if (sym1->is_class_member &&
          sym1->kind == (a_symbol_kind)sk_member_function &&
          sym1->variant.routine.ptr->is_prototype_instantiation) {
        /* Make sure two distinct entities are involved in the diagnostic. */
        a_type_ptr  parent_to_diagnose = sym_parent_class(sym1);
        if (parent_to_diagnose ==
                       (a_type_ptr)canonical_il_entry_of(parent_to_diagnose) &&
            sym2->is_class_member) {
          parent_to_diagnose = sym_parent_class(sym2);
        }  /* if */
        report_bad_trans_unit_corresp(parent_to_diagnose);
      } else {
        expect_error();
      }  /* if */
    }  /* if */
  }  /* if */
  return match;
}  /* f_verify_name_correspondence */

#define verify_name_correspondence(ptr)                                \
  f_verify_name_correspondence((char*)(ptr))

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean same_str(a_const_char *s1,
                          a_const_char *s2)
/*
Determine whether the given character strings are the same.  Also handle NULL
pointers.
*/
{
  a_boolean  result;

  if (s1 == s2) {
    result = TRUE;
  } else if (s1 == NULL || s2 == NULL) {
    result = FALSE;
  } else {
    result = (strcmp(s1, s2) == 0);
  }  /* if */
  return result;
}  /* same_str */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void set_type_corresp(a_type_ptr  type,
                             a_type_ptr  corresp_type)
/*
Make type (and its inner structure) correspond to corresp_type.  This routine
also deals with the consequences of type becoming the new canonical entry.
*/
{
  if (trans_unit_corresp_of(corresp_type) == NULL &&
      trans_unit_corresp_of(type) != NULL) {
    /* corresp_type is the newer type: swap the arguments. */
    a_type_ptr  tmp = type;
    type = corresp_type;
    corresp_type = tmp;
  }  /* if */
  set_trans_unit_corresp(iek_type, type, corresp_type);
  if (type->kind != corresp_type->kind &&
      (!is_class_or_struct(type) || !is_class_or_struct(corresp_type))) {
    /* This is an error and will be caught later (in the verification process).
       Don't attempt to handle the substructure of the type.  (However, don't
       worry about struct vs. class differences.) */
    if (is_immediate_class_type(type)) {
      clear_class_type_correspondence(type, /*visited=*/TRUE);
    } else if (is_immediate_enum_type(type)) {
      clear_enum_type_correspondence(type, /*visited=*/TRUE);
    }  /* if */
    expect_error();
  } else if (type == (a_type_ptr)canonical_il_entry_of(corresp_type)) {
    /* The canonical IL entry changed to type. */
    if (!type_has_definition(corresp_type)) {
      /* This is the first definition.  The members of type should therefore
         be marked as having no correspondence. */
      if (is_immediate_class_type(type)) {
        clear_class_type_correspondence(type, /*visited=*/TRUE);
      } else if (is_immediate_enum_type(type)) {
        clear_enum_type_correspondence(type, /*visited=*/TRUE);
      }  /* if */
    } else {
      /* Make the members of corresp_type correspond to those of type. */
      if (is_immediate_class_type(corresp_type)) {
        establish_trans_unit_correspondences_for_class(corresp_type);
        if (corresp_type->variant.class_struct_union.is_template_class) {
          /* Make sure this instance will be compared against the canonical
             entry. */
          add_verification_entry(iek_type, (char*)corresp_type);
        }  /* if */
      } else if (is_immediate_enum_type(corresp_type)) {
        establish_trans_unit_correspondences_for_enum(corresp_type);
      }  /* if */
    }  /* if */
  } else {
    /* The canonical IL entry didn't change.  Set the correspondences for
       the members. */
    if (is_immediate_class_type(type)) {
      establish_trans_unit_correspondences_for_class(type);
      if (type->variant.class_struct_union.is_template_class) {
        /* Make sure this instance will be compared against the canonical
           entry. */
        add_verification_entry(iek_type, (char*)type);
      }  /* if */
    } else if (is_immediate_enum_type(type)) {
      establish_trans_unit_correspondences_for_enum(type);
    }  /* if */
  }  /* if */
  if (type->kind == (a_type_kind)tk_typeref && typeref_is_typedef(type)) {
    /* Setting a correspondence for a typedef sometimes also requires
       matching the underlying types. */
    a_type_ptr  tp = skip_typerefs(type);
    a_type_ptr  corresp_tp = skip_typerefs(corresp_type);
    /* In C mode, correspondences are only set for enum and class types
       (and not for typedefs of such types). */
    check_assertion(!C_mode());
    if (is_immediate_class_type(tp) &&
        tp->variant.class_struct_union.originally_unnamed &&
        is_immediate_class_type(corresp_tp) &&
        corresp_tp->variant.class_struct_union.originally_unnamed) {
      /* These are unnamed class types that acquired linkage through a typedef.
         Since the typedefs correspond, these types should too. */
      if (same_name(tp, corresp_tp)) {
        set_type_corresp(tp, corresp_tp);
      }  /* if */
    } else if (is_immediate_enum_type(tp) &&
               tp->variant.integer.originally_unnamed &&
               is_immediate_enum_type(corresp_tp) &&
               corresp_tp->variant.integer.originally_unnamed) {
      /* Same for unnamed enum types. */
      if (same_name(tp, corresp_tp)) {
        set_type_corresp(tp, corresp_tp);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_type_corresp */


static void verify_attr_corresp_one_way(char               *entity1,
                                        char               *entity2,
                                        an_il_entry_kind   entity_kind,
                                        a_source_position  *pos2)
/*
Check every attribute attached to entity1 for correspondence with the
attributes attached to the corresponding entity2 (both entities are of the
indicated kind).  Report conflicts as needed (pos2 is the position to report
if entity2 is missing an attribute).  Also, mark any attributes of entity1
that would have to be added to those of entity2 if entity2 were the canonical
entry.
*/
{
  an_attribute_ptr  attr1 = *get_attribute_link(entity1, entity_kind), ap1;

  if (attr1 != NULL) {
    /* For entity2, set up an array that points to the "principal" attribute
       of each kind (if any) for easy/speedy lookup. */
    an_attribute_ptr  attr2 = *get_attribute_link(entity2, entity_kind), ap2;
    an_attribute_ptr  atable2[(int)ak_last];
    memzero((char*)atable2, sizeof(atable2));
    for (ap2 = attr2; ap2 != NULL; ap2 = ap2->next) {
      if (atable2[(int)ap2->kind] == NULL || ap2->on_primary_declaration) {
        atable2[(int)ap2->kind] = ap2;
      }  /* if */
    }  /* for */
    /* Check each attribute to see if it has a counterpart in the other
       translation unit. */
    for (ap1 = attr1; ap1 != NULL; ap1 = ap1->next) {
      an_attr_corresp_flag_set     acflags, match_mode;
      an_attr_corresp_checking_fn  *checking_fn;
      if (is_unapplicable_attr(ap1)
#if GNU_EXTENSIONS_ALLOWED
          || ap1->is_implicit_abi_tag_attribute
#endif /* GNU_EXTENSIONS_ALLOWED */
                                            ) {
        /* Ignore any attributes that aren't applicable or were added
           implicitly. */
        continue;
      }  /* if */
      get_attr_corresp_checking_info(ap1, entity_kind, &acflags, &checking_fn);
      match_mode = acflags & ACF_MATCH_MASK;
      ap2 = atable2[(int)ap1->kind];
      if (match_mode == ACF_CUSTOM_MATCH) {
        /* Call a custom function to check whether the attributes match.
           (Such a function can be specified in the attr_corresp_table array
           defined in attribute.c.) */
        check_assertion(checking_fn != 0);
        (void)checking_fn(entity1, entity2, entity_kind, ap1, ap2);
      } else if (ap2 == NULL) {
        /* An attribute (ap1) of the first entity doesn't have a counterpart
           in entity2. */
        if (match_mode == ACF_STRICT_MATCH && pos2->seq != 0) {
          /* Strict matching is requested.  (However, when matching built-in
             declarations just retain the source attribute.) */
          a_diagnostic_ptr dp;
          check_assertion(ap1->name != NULL);
          dp = pos_st_start_error(
                            ec_missing_attribute_in_other_translation_unit,
                            &ap1->position, ap1->name);
          add_diag_info_with_pos_insert(dp, ec_corresp_decl_at, pos2);
          end_diagnostic(dp);
        } else {
          /* A valid ("void") match: Mark the attribute as requiring a copy
             to the primary translation unit entity. */
          ap1->must_be_preserved_in_trans_unit_copy = TRUE;
        }  /* if */
      } else if (equivalent_attributes(ap1, ap2, /*ignore_family=*/FALSE)) {
        /* Ordinarily, since the attribute is present in both translation
           units, no copy is required.  Occasionally, however, it might make
           sense to "accumulate" all attributes from all translation units. */
        if (acflags & ACF_ALWAYS_TRANS_COPY) {
          ap1->must_be_preserved_in_trans_unit_copy = TRUE;
        }  /* if */
      } else if ((acflags & ACF_MATCH_MASK) == ACF_MATCH_OPTIONAL) {
        /* The attributes don't match, but that is okay.   Mark the attribute
           as requiring a copy to the primary translation unit entity. */
        ap1->must_be_preserved_in_trans_unit_copy = TRUE;
      } else {
        /* Conflicting attributes: Issue an error. */
        a_diagnostic_ptr dp;
        dp = pos_st_start_error(
                           ec_conflicting_attribute_in_other_translation_unit,
                           &ap1->position, ap1->name);
        add_diag_info_with_pos_insert(dp, ec_corresp_decl_at, &ap2->position);
        end_diagnostic(dp);
        make_attr_unrecognized(ap1);
        make_attr_unrecognized(ap2);
      }  /* if */
      if (ap1->must_be_preserved_in_trans_unit_copy &&
          entity_kind == iek_field) {
        /* To improve the performance of the trans_copy process, we mark
           classes that have a field with an attribute that might have to be
           copied over to the primary translation unit. */
        a_type_ptr  pt = parent_class_of((a_field*)entity1);
        symbol_supplement_for_class(pt)->has_field_with_attr_to_merge = TRUE;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* verify_attr_corresp_one_way */


static void f_verify_attributes_correspondence(
                                            char              *entity1,
                                            char              *entity2,
                                            an_il_entry_kind  entity_kind)
/*
The two given entities (of the given kind) have been found to correspond.  
Issue diagnostics as appropriate if their attached attributes conflict
(including cases where an attribute is present in one translation unit but not
in the other).  This routine also marks attributes if they should be merged to
the canonical entry.  entity1 and entity2 must have a source correspondence
(for other entity kinds, such as a_param_type entries, two calls to
verify_attr_corresp_one_way should be made instead).
*/
{
  a_source_position  *pos1, *pos2;

  pos1 = &((a_source_correspondence_ptr)entity1)->decl_position;
  pos2 = &((a_source_correspondence_ptr)entity2)->decl_position;
  verify_attr_corresp_one_way(entity1, entity2, entity_kind, pos2);
  verify_attr_corresp_one_way(entity2, entity1, entity_kind, pos1);
}  /* f_verify_attributes_correspondence */

#define verify_attributes_correspondence(e1, e2, kind)                       \
  (f_verify_attributes_correspondence((char*)(e1), (char*)(e2),              \
                                      (an_il_entry_kind)kind))

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean equivalent_properties_or_events(
                                          a_property_or_event_descr_ptr  pdp1,
                                          a_property_or_event_descr_ptr  pdp2)
/*
Return whether the given properties/events (or NULL pointers) are equivalent.
*/
{
  a_boolean  result = FALSE;

  if (pdp1 == NULL && pdp2 == NULL) {
    result = TRUE;
  } else if (pdp1 != NULL && pdp2 != NULL) {
    if (pdp1->kind != pdp2->kind) {
      result = FALSE;
    } else {
      switch (pdp1->kind) {
        case pek_declspec_property:
          /* __declspec(property(...)) fields. */
          result = same_str(pdp1->get_routine.name, pdp2->get_routine.name) &&
                   same_str(pdp1->set_routine.name, pdp2->set_routine.name);
          break;
        case pek_cli_property:
          /* C++/CLI property fields. */
          result = same_entities(pdp1->get_routine.ptr,
                                 pdp2->get_routine.ptr) &&
                   same_entities(pdp1->set_routine.ptr,
                                 pdp2->set_routine.ptr);
          break;
        case pek_cli_event:
          /* C++/CLI event fields. */
          result = same_entities(pdp1->add_routine, pdp2->add_routine) &&
                   same_entities(pdp1->remove_routine, pdp2->remove_routine) &&
                   same_entities(pdp1->raise_routine, pdp2->raise_routine);
          break;
        default:
          unexpected_condition();
      }  /* switch */
    }  /* if */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* equivalent_properties_or_events */


static a_boolean equivalent_property_or_event_fields(a_field_ptr  fp1,
                                                     a_field_ptr  fp2)
/*
Return whether fp1 and fp2 are either both not properties/events, or both
equivalent properties/events.
*/
{
  return equivalent_properties_or_events(fp1->property_or_event_descr,
                                         fp2->property_or_event_descr);
}  /* equivalent_property_or_event_fields */


static a_boolean equivalent_property_or_event_vars(a_variable_ptr  var1,
                                                   a_variable_ptr  var2)
/*
Return whether var1 and var2 are either both not properties/events, or both
equivalent properties/events.
*/
{
  return equivalent_properties_or_events(var1->property_or_event_descr,
                                         var2->property_or_event_descr);
}  /* equivalent_property_or_event_vars */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean verify_field_correspondence(a_field_ptr  field)
/*
Check that the recorded translation unit correspondence for the given field
is in fact valid.
*/
{
  a_boolean    match = TRUE;
  if (trans_unit_corresp_of(field) != NULL) {
    a_field_ptr  corresp_field = (a_field_ptr)canonical_il_entry_of(field);
    a_source_correspondence_ptr
                 scp, corresp_scp;

    if (field == corresp_field) {
      /* This is the canonical entry.  If applicable, verify the entry in
         the primary translation unit against this one.  Otherwise, nothing
         needs to be done. */
      a_field_ptr  prim = (a_field_ptr)trans_unit_corresp_of(field)->primary;
      if (prim != NULL && field != prim) {
        field = prim;
      } else {
        goto done;
      }  /* if */
    }  /* if */
    scp = &field->source_corresp,
    corresp_scp = &corresp_field->source_corresp;
  
    match = verify_name_correspondence(field);
    if (match &&
        (!f_types_are_compatible(field->type, corresp_field->type,
                                 TCF_SEEK_CORRESP |
                                 TCF_REDECLARATION |
                                 TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING) ||
         !same_exception_spec(field->type, corresp_field->type) ||
         field->bit_size != corresp_field->bit_size ||
         field->is_bit_field != corresp_field->is_bit_field ||
         field->bit_field_is_signed != corresp_field->bit_field_is_signed ||
         field->is_anonymous_parent_object !=
                                   corresp_field->is_anonymous_parent_object ||
         field->is_mutable != corresp_field->is_mutable ||
#if MICROSOFT_EXTENSIONS_ALLOWED
         !equivalent_property_or_event_fields(field, corresp_field) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
         field->has_initializer != corresp_field->has_initializer ||
         scp->access != corresp_scp->access ||
         scp->name_linkage != corresp_scp->name_linkage)) {
      match = FALSE;
      if (scp->assoc_info != NULL &&
          scp->assoc_info != (char*)get_unnamed_field_symbol()) {
        /* A named field: */
        process_bad_trans_unit_corresp(iek_field, field, corresp_field);
      } else {
        /* An unnamed field has a meaningless associated symbol.  Report
           the error on the associated class instead. */
        report_bad_trans_unit_corresp(scp_parent_class(scp));
        set_no_trans_unit_corresp(iek_field, field);
      }  /* if */
    }  /* if */
    verify_attributes_correspondence(field, corresp_field, iek_field);
#if CHECKING
    if (match &&
        (field->offset != corresp_field->offset ||
         field->offset_bit_remainder != corresp_field->offset_bit_remainder)) {
      /* A mismatch in the offset attributes normally reflects a mismatch in
         another area.  Hence we don't issue an additional error on this. */
      expect_error();
    }  /* if */
#endif /* CHECKING */
  }  /* if */
done:
  return match;
}  /* verify_field_correspondence */


static a_boolean inline_flag_can_differ(a_routine_ptr  rp1,
                                        a_routine_ptr  rp2)
/*
Determine whether two C++ routines can validly have different values for their
is_inline flag.
*/
{
  a_boolean  result = FALSE;

  check_assertion(!C_mode() && rp1->is_inline != rp2->is_inline);
  if (rp1->is_inline) {
    a_routine_ptr  tmp = rp1;
    rp1 = rp2;
    rp2 = tmp;
  }  /* if */
  /* rp1 is not inline whereas rp2 is. */
  if (rp1->is_template_function && !rp1->is_specialized &&
      !(rp1->is_prototype_instantiation && rp1->defined) &&
      rp1->function_def_number == NULL_function_def_number) {
    /* An uninstantiated template function may not have had its is_inline
       flag set yet. */
    result = TRUE;
  } else if (!routine_defined(rp1) && !rp1->called && !rp1->address_taken &&
             !rp1->is_virtual &&
             (routine_defined(rp2) || !rp1->source_corresp.is_class_member)) {
    /* If a routine is undefined and unused, it is OK for it not to have
       been declared inline when other declarations of that routine are
       inline.  (However, if the routine is a class member that hasn't
       been defined in either translation unit, the parent class must have
       been defined differently in those translation units.) */
    result = TRUE;
  }  /* if */
  return result;
}  /* inline_flag_can_differ */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean microsoft_compatible_friend_linkage(a_routine_ptr  rp1,
                                                     a_routine_ptr  rp2)
/*
In Microsoft mode, a routine only declared in a friend declaration should
be treated as having "unknown" linkage instead of the standard "external"
linkage.  Two matching friend function declarations are therefore always
compatible if one was the only declaration of that function in that
translation unit.
*/
{
  a_boolean  result = FALSE;

  if (microsoft_mode &&
      ((rp1->declared_only_as_friend && !rp1->defined_in_friend_decl) ||
       (rp2->declared_only_as_friend && !rp2->defined_in_friend_decl))) {
    result = TRUE;
  }  /* if */
  return result;
}  /* microsoft_compatible_friend_linkage */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean routine_name_linkage_can_differ(a_routine_ptr  rp1,
                                                 a_routine_ptr  rp2)
/*
Determine whether two C++ routines can validly have different name linkages.
This should only happen for inline functions when extern_inline_allowed is
FALSE or (rarely) for friend declarations in Microsoft mode.
*/
{
  a_boolean  result = FALSE;

  if (!extern_inline_allowed) {
    if (rp1->is_inline) {
      a_routine_ptr  tmp = rp1;
      rp1 = rp2;
      rp2 = tmp;
    }  /* if */
    /* rp2 must be inline for the differing name linkages to be valid.
       If rp1 has not yet been "instantiated", it may still have extern "C" or
       "C++" linkage, whereas the instantiated entity has internal linkage. */
    if (rp2->is_inline && rp2->defined && !rp1->defined &&
        rp2->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_internal) {
      result = TRUE;
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (!result) {
    /* If the routine only appeared as a nondefining declaration in one of the
       two translation units, then that declaration is considered compatible
       to the other wrt. linkage and name linkage. */
    result = microsoft_compatible_friend_linkage(rp1, rp2);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return result;
}  /* routine_name_linkage_can_differ */


static a_boolean is_generated_new_or_delete_operator(a_routine_ptr  routine)
/*
Return TRUE if the given routine is a new or delete operator (including
array variants) and marked as compiler-generated.
*/
{
  a_boolean  result = FALSE;

  if (routine->compiler_generated &&
      routine->special_kind == (a_special_function_kind)sfk_operator &&
      (is_new_operator(routine->variant.opname_kind) ||
       is_delete_operator(routine->variant.opname_kind))) {
    result = TRUE;
  }  /* if */
  return result;
}  /* is_generated_new_or_delete_operator */

#if DECL_MODIFIERS_IN_USE

static a_boolean incompatible_routine_decl_modifiers(a_routine_ptr  rp1,
                                                     a_routine_ptr  rp2)
/*
Return TRUE if and only if the two corresponding routines have incompatible
declaration modifiers.
*/
{
  a_boolean            result = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_decl_modifier_set  dm_mask = (DM_DLLFLAGS | DM_FORCEINLINE);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
  /* dllimport and dllexport need not match up.  The only constraint is that
     there should not be two dllexport definitions, but that is covered by
     the more general check for multiple definitions. */
  result = ((rp1->decl_modifiers & ~dm_mask) !=
                                            (rp2->decl_modifiers & ~dm_mask));
  if (!result &&
      (((rp1->decl_modifiers & DM_FORCEINLINE) && rp2->never_inline) ||
       (rp1->never_inline && (rp2->decl_modifiers & DM_FORCEINLINE)))) {
    /* A routine shouldn't be declared both with __forceinline and with
       __declspec(noinline). */
    result = TRUE;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  if (!result && rp1->is_naked != rp2->is_naked) {
    /* Attribute "naked" need not be specified on every declaration, but if it
       appears on one, it must also appear on the definition. */
    if ((rp1->is_naked && rp2->defined) || (rp2->is_naked && rp1->defined)) {
      result = TRUE;
    }  /* if */
  }  /* if */
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
  if (!result && rp1->never_throws != rp2->never_throws) {
    /* The nothrow attribute need not be specified on every declaration, but
       if it appears on one, it must also appear on the definition. */
    if ((rp1->never_throws && rp2->defined) ||
        (rp2->never_throws && rp1->defined)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  if (!result &&
      routine_does_not_return(rp1) != routine_does_not_return(rp2)) {
    /* The noreturn attribute need not be specified on every declaration, but
       if it appears on one, it must also appear on the definition.  (The
       standard C++11 [[noreturn]] attribute has stricter requirements: They
       are verified in verify_attributes_correspondence.) */
    if ((routine_does_not_return(rp1) && rp2->defined) ||
        (routine_does_not_return(rp2) && rp1->defined)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* incompatible_routine_decl_modifiers */

#endif /* DECL_MODIFIERS_IN_USE */

static void verify_corresp_for_entities_list(
                              an_il_entity_list_entry_ptr  ep1,
                              an_il_entity_list_entry_ptr  ep2,
                              char                         *entity1,
                              a_source_position_ptr        pos2,
                              an_error_code                same_src_error,
                              an_error_code                distinct_src_error)
/*
The entities from the first list (ep1) should correspond to those of the
second list (ep2): Verify that it is so, and issue a correspondence error
otherwise.  (The correspondences should normally have been set by a call to
set_corresp_for_entities_list.)  If the error is in the length of the lists,
the diagnostic is issued by calling report_corresp_error with the arguments
entity1, pos2, same_src_error, and distinct_src_error.  (Currently, the lists
only contain closure types.)
*/
{
  for (; ep1 != NULL && ep2 != NULL; ep1 = ep1->next, ep2 = ep2->next) {
    /* Currently we expect only closure types here. */
    a_type_ptr  tp1;
    check_assertion(ep1->entity.kind == iek_type &&
                    ep2->entity.kind == iek_type);
    tp1 = (a_type_ptr)ep1->entity.ptr;
    check_assertion(type_is_lambda_closure(tp1));
    (void)verify_type_correspondence(tp1);
  }  /* for */
  if (ep1 != NULL || ep2 != NULL) {
    /* The number of closure types recorded don't match: Issue an error. */
    report_corresp_error(entity1, pos2, same_src_error, distinct_src_error);
  }  /* if */
}  /* verify_corresp_for_entities_list */


static void verify_corresp_for_default_arg_entities(a_routine_ptr  rp1,
                                                    a_routine_ptr  rp2)
/*
Verify that any entities defined in default arguments of the given routines
do indeed correspond (normally they were made to correspond via a call to
set_corresp_for_routines).  Issue diagnostics as appropriate.
*/
{
  a_type_ptr  rtp1 = rp1->type, rtp2 = rp2->type;
  a_boolean   inline_or_template_case;

  inline_or_template_case =
                     (rp1->is_inline && rp2->is_inline) ||
                     (rp1->is_template_function && rp2->is_template_function);
  if (!inline_or_template_case && !rp1->source_corresp.is_class_member) {
    /* Not a case where default arguments must match. */
    check_assertion(!rp2->source_corresp.is_class_member);
  } else if (rtp1->kind == (a_type_kind)tk_routine &&
             rtp2->kind == (a_type_kind)tk_routine) {
    a_param_type_ptr  ptp1 = rtp1->variant.routine.extra_info->param_type_list;
    a_param_type_ptr  ptp2 = rtp2->variant.routine.extra_info->param_type_list;
    for (; ptp1 != NULL && ptp2 != NULL;
           ptp1 = ptp1->next, ptp2 = ptp2->next) {
      if (!inline_or_template_case &&
          !ptp1->default_arg_appeared_in_class_definition &&
          !ptp2->default_arg_appeared_in_class_definition) {
        /* Any entities defined in the default arguments associated with this
           parameter pair aren't required to correspond. */
      } else {
        verify_corresp_for_entities_list(
                               ptp1->entities_defined_in_default_arg,
                               ptp2->entities_defined_in_default_arg,
                               (char*)rp1, &rp2->source_corresp.decl_position,
                               ec_default_arg_differs_in_other_trans_unit,
                               ec_default_args_incompatible);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* verify_corresp_for_default_arg_entities */


static a_boolean exc_spec_is_always_compatible(a_routine_ptr rp)
/*
Given a routine whose exception specification is being checked for a match with
another (not provided here) routine, return TRUE if the exception specification
should be considered compatible with any exception specification on the other
routine.

Note that the checks in same_exception_spec are not repeated here, as these are
assumed to have already occurred (and presumably that has already returned
FALSE).  A TRUE result from this routine does not indicate that the exception
specifications are the same, rather that they could be the same and further
accuracy is not possible at this stage of compilation.
*/
{
  a_boolean  result = FALSE;
  an_exception_specification_ptr
             esp;

  check_assertion(rp->type->kind == (a_type_kind)tk_routine);
  esp = rp->type->variant.routine.extra_info->exception_specification;
  if (esp != NULL) {
    if (esp->indeterminate) {
      result = TRUE;
    } else if (esp->copy_from_prototype) {
      a_template_instance_ptr
                    tip = symbol_for(rp)->variant.routine.instance_ptr;
      a_symbol_ptr  templ_sym = tip->template_sym,
                    proto_templ_sym = prototype_template_of(templ_sym);
      a_template_symbol_supplement_ptr
                    proto_tssp=template_supplement_for_symbol(proto_templ_sym);
      a_routine_ptr proto_rp = proto_tssp->variant.function.routine;
      a_type_ptr    proto_rtp = skip_typerefs(proto_rp->type);
      a_routine_type_supplement_ptr
                    proto_rtsp = proto_rtp->variant.routine.extra_info;
      an_exception_specification_ptr
                    proto_esp = proto_rtsp->exception_specification;

      check_assertion(proto_esp != NULL);
      if (proto_esp->indeterminate || proto_esp->throw_any) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* exc_spec_is_always_compatible */


static a_boolean compatible_exception_spec(a_routine_ptr  rp1,
                                           a_routine_ptr  rp2)
/*
Return TRUE if the given routines have matching exception specifications, or
if either one has an indeterminate exception specification.
*/
{
  a_boolean  result = same_exception_spec(rp1->type, rp2->type);

  if (!result) {
    if (type_is(rp1->type, tk_routine) && exc_spec_is_always_compatible(rp1)) {
      result = TRUE;
    } else if (type_is(rp2->type, tk_routine) &&
               exc_spec_is_always_compatible(rp2)) {
      result = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_mode) {
      /* MSVC allows both:
            __declspec(nothrow) void f();
            void f();
         and
            declspec(nothrow) void f();
            void f() noexcept;
         The __declspec(nothrow) case has a compiler-generated "noexcept"
         specification. */
      a_type_ptr  rtp1 = skip_typerefs(rp1->type),
                  rtp2 = skip_typerefs(rp2->type);
      an_exception_specification_ptr
                  esp1 = rout_type_supp(rtp1)->exception_specification,
                  esp2 = rout_type_supp(rtp2)->exception_specification;
      if (esp1 != NULL && esp1->is_noexcept && esp1->compiler_generated &&
          find_attribute(ak_nothrow, rp1->source_corresp.attributes) != NULL) {
        result = TRUE;
      } else if (esp2 != NULL && esp2->is_noexcept &&
                 esp2->compiler_generated &&
                 find_attribute(ak_nothrow,
                                rp2->source_corresp.attributes) != NULL) {
        result = TRUE;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
  return result;
}  /* compatible_exception_spec */


static a_boolean routines_are_compatible(a_routine_ptr rp1,
                                         a_routine_ptr rp2)
/*
Return TRUE if the given routines are compatible with each other.
*/
{
  a_boolean                   result = FALSE;
  a_source_correspondence_ptr scp1 = &rp1->source_corresp;
  a_source_correspondence_ptr scp2 = &rp2->source_corresp;

  /* These could all be combined into a single if statement, but for
     readability and debugability they are kept separate. */
  if (!f_types_are_compatible(rp1->type, rp2->type,
                              TCF_SEEK_CORRESP |
                              TCF_REDECLARATION |
                              TCF_IGNORE_TOP_LEVEL_NOEXCEPT |
                              TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING)) {
    /* result = FALSE; */
  } else if (rp1->compiler_generated != rp2->compiler_generated &&
             !is_implicitly_declared_c_func(rp1) &&
             !is_implicitly_declared_c_func(rp2) &&
             !is_generated_new_or_delete_operator(rp1) &&
             !is_generated_new_or_delete_operator(rp2)) {
    /* result = FALSE; */
  } else if (!rp1->compiler_generated && !rp2->compiler_generated &&
             !rp1->is_defaulted && !rp2->is_defaulted &&
             !compatible_exception_spec(rp1, rp2)) {
    /* result = FALSE; */
  } else if (rp1->is_virtual != rp2->is_virtual ||
             rp1->pure_virtual != rp2->pure_virtual) {
    /* result = FALSE; */
  } else if (!rp1->defined_outside_of_parent &&
             !rp2->defined_outside_of_parent &&
             (rp1->is_defaulted != rp2->is_defaulted ||
              rp1->is_deleted != rp2->is_deleted)) {
    /* result = FALSE; */
  } else if (!C_mode() && rp1->is_inline != rp2->is_inline &&
             !inline_flag_can_differ(rp1, rp2)) {
    /* In C mode (C99 & GNU C), the inline flag does not need to match.
       In C++ mode, they usually should match, but some special situations
       do not require a match. */
    /* result = FALSE; */
  } else if (rp1->template_arg_list != NULL &&
             rp1->is_specialized != rp2->is_specialized) {
    /* If both routines are template specialization, the explicit
       template specialization bit should be the same. */
    /* result = FALSE; */
  } else if (rp1->is_explicit_constructor != rp2->is_explicit_constructor) {
    /* result = FALSE; */
#if DECL_MODIFIERS_IN_USE
  } else if (incompatible_routine_decl_modifiers(rp1, rp2)) {
    /* result = FALSE; */
#endif /* DECL_MODIFIERS_IN_USE */
  } else if (rp1->defined && rp2->defined &&
             (rp1->fp_contract != rp2->fp_contract ||
              rp1->fenv_access != rp2->fenv_access ||
              rp1->cx_limited_range != rp2->cx_limited_range
#if FIXED_POINT_ALLOWED
              || rp1->fx_full_precision != rp2->fx_full_precision
              || rp1->fx_fract_overflow != rp2->fx_fract_overflow
              || rp1->fx_accum_overflow != rp2->fx_accum_overflow
#endif /* FIXED_POINT_ALLOWED */
             )) {
    /* result = FALSE; */
  } else if (scp1->access != scp2->access ||
             (scp1->name_linkage != scp2->name_linkage &&
              !routine_name_linkage_can_differ(rp1, rp2))) {
    /* result = FALSE; */
  } else {
    result = TRUE;
  }  /* if */
  return result;
}  /* routines_are_compatible */


static a_boolean verify_routine_correspondence(a_routine_ptr  routine)
/*
Check that the recorded translation unit correspondence for the given routine
is in fact valid.
*/
{
  a_boolean      match = TRUE;

  if (trans_unit_corresp_of(routine) != NULL) {
    a_routine_ptr  corresp_routine =
                                (a_routine_ptr)canonical_il_entry_of(routine);
    a_type_ptr     type, corresp_type;
    if (routine == corresp_routine) {
      /* This is the canonical entry.  If applicable, verify the entry in
         the primary translation unit against this one.  Otherwise, nothing
         needs to be done. */
      a_routine_ptr  prim =
                       (a_routine_ptr)trans_unit_corresp_of(routine)->primary;
      if (prim != NULL && routine != prim) {
        corresp_routine = routine;
        routine = prim;
      } else {
        goto done;
      }  /* if */
    }  /* if */
    match = verify_name_correspondence(routine);
    type = skip_typerefs(routine->type);
    corresp_type = skip_typerefs(corresp_routine->type);
    if (match && !routines_are_compatible(routine, corresp_routine)) {
      match = FALSE;
      process_bad_trans_unit_corresp(iek_routine, routine, corresp_routine);
    }  /* if */
    /* If this is an inline function based on a template, compare the
       template checksums. */
    if (match && routine->is_template_function &&
        (routine->is_inline || corresp_routine->is_inline) &&
        !routine->is_specialized) {
      a_template_ptr	templ;
      a_template_ptr	corresp_templ;
      templ = routine->assoc_template;
      corresp_templ= corresp_routine->assoc_template;
      templ = templ->canonical_template->definition_template;
      corresp_templ = corresp_templ->canonical_template->definition_template;
      if (templ != NULL && corresp_templ != NULL) {
        if (templ->cache_checksum != corresp_templ->cache_checksum &&
            !suppress_inline_corresp_check) {
          /* A difference in definition doesn't break the correspondence
             (i.e., "match" remains TRUE).  This is to avoid unnecessary
             cascading of correspondence errors. */
          process_bad_trans_unit_corresp(iek_routine,
                                         routine, corresp_routine);
        }  /* if */
      }  /* if */
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
    if (match && (has_gnu_routine_supp(routine) ||
                  has_gnu_routine_supp(corresp_routine))) {
      /* At least one of the routines has a GNU routine supplement.  Make sure
         they're both allocated and that they correspond. */
      if (!has_gnu_routine_supp(routine)) {
        (void)ensure_gnu_routine_supp(routine);
      }  /* if */
      if (!has_gnu_routine_supp(corresp_routine)) {
        (void)ensure_gnu_routine_supp(corresp_routine);
      }  /* if */
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
    if (match && !trans_unit_test_mode && !routine->is_inline &&
        (!routine->is_prototype_instantiation ||
         (routine->assoc_template != NULL &&
          routine->assoc_template->is_exported)) &&
#if GNU_EXTENSIONS_ALLOWED
        !(routine->is_weak || corresp_routine->is_weak) &&
#endif /* GNU_EXTENSIONS_ALLOWED */
        routine->defined && corresp_routine->defined) {
      /* Multiple definition. */
      report_multiple_definitions(routine);
    } else if (match) {
      verify_corresp_for_default_arg_entities(routine, corresp_routine);
    }  /* if */
    if (match &&
        type->variant.routine.extra_info->prototyped &&
        corresp_type->variant.routine.extra_info->prototyped) {
      /* Verify that attributes on the routine and on its parameters match
         across translation units. */
      a_param_type_ptr   ptp1 = function_type_params(type),
                         ptp2 = function_type_params(corresp_type);
      a_source_position  *pos1, *pos2;
      verify_attributes_correspondence(routine, corresp_routine, iek_routine);
      pos1 = &routine->source_corresp.decl_position;
      pos2 = &corresp_routine->source_corresp.decl_position;
      for (; ptp1 != NULL; ptp1 = ptp1->next, ptp2 = ptp2->next) {
        check_assertion(ptp2 != NULL);
        verify_attr_corresp_one_way((char*)ptp1, (char*)ptp2, iek_param_type,
                                    pos2);
        verify_attr_corresp_one_way((char*)ptp2, (char*)ptp1, iek_param_type,
                                    pos1);
      }  /* for */
      check_assertion(ptp2 == NULL);
    }  /* if */
  }  /* if */
done:
  return match;
}  /* verify_routine_correspondence */

#if DECL_MODIFIERS_IN_USE

static a_boolean incompatible_variable_decl_modifiers(
                                               ARG_UNUSED a_variable_ptr  var1,
                                               ARG_UNUSED a_variable_ptr  var2)
/*
Return TRUE if and only if the two corresponding variables have incompatible
declaration modifiers.
*/
{
  a_boolean            result = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_decl_modifier_set  dm_mask = (DM_DLLFLAGS | DM_SELECTANY);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
  result = ((var1->decl_modifiers & ~dm_mask) !=
            (var2->decl_modifiers & ~dm_mask));
  /* dllimport, dllexport, and selectany need not match up.  The only
     constraint is that if there are multiple definitions they must be
     marked with selectany, but that is covered by the check for multiple
     definitions. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return result;
}  /* incompatible_variable_decl_modifiers */

#endif /* DECL_MODIFIERS_IN_USE */

static a_boolean equiv_member_constants(a_variable_ptr  var,
                                        a_variable_ptr  corresp_var)
/*
The given variable entries represent member constants.  Return TRUE if their
associated constants can be considered equivalent.
*/
{
  a_compare_constants_options_set  options;
  a_constant_ptr                   cp1 = initializer_constant(var);
  a_constant_ptr                   cp2 = initializer_constant(corresp_var);

  check_assertion(cp1 != NULL && cp2 != NULL);
  if (!strict_ansi_mode) {
    options = CC_RELAXED_ADDRESS_OF_CONSTANT_COMPARISON;
    if (var->is_prototype_instantiation &&
        corresp_var->is_prototype_instantiation) {
      options |= CC_CONSTEXPR_NAME_EQUIVALENCES_ALLOWED;
    }  /* if */
  } else {
    options = CC_NO_OPTIONS;
  }  /* if */
  return compare_constants(cp1, cp2, options);
}  /* equiv_member_constants */


static a_boolean verify_variable_correspondence(a_variable_ptr  var)
/*
Check that the recorded translation unit correspondence for the given variable
is in fact valid.
*/
{
  a_boolean       match = TRUE;

  if (trans_unit_corresp_of(var) != NULL) {
    a_variable_ptr  corresp_var = (a_variable_ptr)canonical_il_entry_of(var);
    a_source_correspondence_ptr
                    scp, corresp_scp;

    if (var == corresp_var) {
      /* This is the canonical entry.  If applicable, verify the current entry
         against the one in the primary translation unit.  Otherwise, nothing
         needs to be done. */
      a_variable_ptr  prim =
                          (a_variable_ptr)trans_unit_corresp_of(var)->primary;
      if (prim != NULL && var != prim) {
        corresp_var = prim;
      } else {
        goto done;
      }  /* if */
    }  /* if */
    scp = &var->source_corresp,
    corresp_scp = &corresp_var->source_corresp;
    match = verify_name_correspondence(var);
    if (match &&
        (!f_types_are_compatible(var->type, corresp_var->type,
                                 TCF_SEEK_CORRESP |
                                 TCF_REDECLARATION |
                                 TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING) ||
         !same_exception_spec(var->type, corresp_var->type) ||
         /* In-class static member initializations must be equivalent.  (In
            some GNU modes, template variables are instantiated lazily and may
            therefore be member constants in one translation unit, but not in
            another one.) */
         (var->is_member_constant == corresp_var->is_member_constant ?
           (var->is_member_constant &&
            !equiv_member_constants(var, corresp_var)) :
           !var->is_template_variable) ||
#if DECL_MODIFIERS_IN_USE
         incompatible_variable_decl_modifiers(var, corresp_var) ||
#endif /* DECL_MODIFIERS_IN_USE */
#if MICROSOFT_EXTENSIONS_ALLOWED
         !equivalent_property_or_event_vars(var, corresp_var) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
         scp->access != corresp_scp->access ||
         scp->name_linkage != corresp_scp->name_linkage)) {
      match = FALSE;
      process_bad_trans_unit_corresp(iek_variable, var, corresp_var);
    }  /* if */
    if (match && !trans_unit_test_mode &&
        var->storage_class == (a_storage_class)sc_unspecified &&
        corresp_var->storage_class == (a_storage_class)sc_unspecified &&
        /* Inline variables and instantiations of variable templates and
           template static data members may appear in more than one translation
           unit. */
        !var->is_inline &&
        (!var->is_template_variable || var->is_specialized) &&
#if GNU_EXTENSIONS_ALLOWED
        !(var->is_weak || corresp_var->is_weak) &&
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
        !((var->decl_modifiers & DM_SELECTANY) &&
          (corresp_var->decl_modifiers & DM_SELECTANY)) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        (!C_mode() ||
         (var->init_kind != (an_init_kind)initk_none &&
          corresp_var->init_kind != (an_init_kind)initk_none))) {
      /* Two nontentative definitions. */
      report_multiple_definitions(var);
    }  /* if */
    verify_attributes_correspondence(var, corresp_var, iek_variable);
  }  /* if */
done:
  return match;
}  /* verify_variable_correspondence */


static a_boolean verify_constant_correspondence(a_constant_ptr  constant)
/*
Check that the recorded translation unit correspondence for the given constant
is in fact valid.
*/
{
  a_boolean       match = TRUE;

  if (trans_unit_corresp_of(constant) != NULL) {
    a_constant_ptr  corresp_constant =
                               (a_constant_ptr)canonical_il_entry_of(constant);
    a_source_correspondence_ptr
                    scp, corresp_scp;

    if (constant == corresp_constant) {
      /* This is the canonical entry.  If applicable, verify the entry in
         the primary translation unit against this one.  Otherwise, nothing
         needs to be done. */
      a_constant_ptr  prim =
                     (a_constant_ptr)trans_unit_corresp_of(constant)->primary;
      if (prim != NULL && constant != prim) {
        corresp_constant = constant;
        constant = prim;
      } else {
        goto done;
      }  /* if */
    }  /* if */
    scp = &constant->source_corresp,
    corresp_scp = &corresp_constant->source_corresp;
    match = verify_name_correspondence(constant);
    if (match &&
        (!identical_types(constant->type, corresp_constant->type) ||
         !same_exception_spec(constant->type, corresp_constant->type) ||
         !eq_constants(constant, corresp_constant) ||
         scp->access != corresp_scp->access ||
         scp->name_linkage != corresp_scp->name_linkage)) {
      match = FALSE;
      process_bad_trans_unit_corresp(iek_constant,
                                     constant, corresp_constant);
    }  /* if */
  }  /* if */
done:
  return match;
}  /* verify_constant_correspondence */


static void check_for_enumerator_conflicts(a_type_ptr  type)
/*
Check whether the enumerators attached to the given enum type conflict with
other entities.
*/
{
  check_assertion(is_immediate_enum_type(type));
  if (!type->source_corresp.is_class_member) {
    a_constant_ptr  enumerator = enum_constants(type);
    for (; enumerator != NULL; enumerator = enumerator->next) {
      a_symbol_ptr            enum_sym = symbol_for(enumerator),
                              sym = corresp_symbol_list(enum_sym);
      a_translation_unit_ptr  trans_unit = trans_unit_for_symbol(enum_sym);
      /* Look through the symbol table for any entities with linkage that may
         conflict with an enumerator. */
      for (; sym != NULL; sym = sym->next) {
        if (sym->decl_scope != NO_SCOPE_NUMBER &&
            trans_unit_for_symbol(sym) != trans_unit &&
            same_parents(sym, enum_sym)) {
          if (may_have_correspondence(sym)) {
            if (sym->kind == (a_symbol_kind)sk_constant &&
                same_name(type, sym->variant.constant->type) &&
                seek_type_corresp(type, sym->variant.constant->type) &&
                same_entities(enumerator, sym->variant.constant)) {
              /* We found a corresponding enumerator in another TU. */
            } else if (is_tag_symbol(sym)) {
              /* Tag names have their own name space. */
            } else {
              a_source_correspondence_ptr
                               scp = source_corresp_entry_for_symbol(sym);
              if (scp != NULL && !in_secondary_trans_unit(scp)) {
                scp->same_name_as_external_entity_in_secondary_trans_unit =
                                                                         TRUE;
              }  /* if */
            }  /* if */
          } else {
            a_source_correspondence_ptr
                               scp = source_corresp_entry_for_symbol(sym);
            if (scp != NULL && !in_secondary_trans_unit(scp)) {
              scp->same_name_as_external_entity_in_secondary_trans_unit = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* if */
}  /* check_for_enumerator_conflicts */


static a_boolean verify_enum_type_correspondence(a_type_ptr  type)
/*
Check that the recorded translation unit correspondence for the given enum
type is in fact valid.
*/
{
  a_boolean       match = verify_name_correspondence(type);
  a_boolean       report_error = FALSE;
  a_type_ptr      corresp_type = (a_type_ptr)canonical_il_entry_of(type);

  if (!match) {
    /* An error was already issued. */
  } else if (!is_immediate_enum_type(corresp_type)) {
    match = FALSE;
    report_error = TRUE;
  } else if (type->variant.integer.is_scoped_enum !=
             corresp_type->variant.integer.is_scoped_enum) {
    /* One is a scoped enum type, and the other is a traditional unscoped
       enum type. */
    match = FALSE;
    report_error = TRUE;
  } else if (!type_has_definition(type) ||
             !type_has_definition(corresp_type)) {
    /* At least one of the two types was declared without being defined.
       In that case, there is no need to verify the list of enumerator
       constants. */
  } else {
    a_constant_ptr  enumerator = enum_constants(type),
                    corresp_enumerator = enum_constants(corresp_type);
    /* Verify one-for-one correspondence of the enumerator constants. */
    for (; enumerator != NULL && corresp_enumerator != NULL;
         enumerator = enumerator->next,
                              corresp_enumerator = corresp_enumerator->next) {
      if (!same_name(enumerator, corresp_enumerator)) {
        /* The error should be issued on the enum type since there is not
           much in common between the enumerators if even their names don't
           match. */
        match = FALSE;
        report_error = TRUE;
        break;
      } else if (!verify_constant_correspondence(enumerator)) {
        match = FALSE;
        break;
      }  /* if */
    }  /* for */
    if ((enumerator != NULL && corresp_enumerator == NULL) ||
        (corresp_enumerator != NULL && enumerator == NULL)) {
      match = FALSE;
      report_error = TRUE;
    }  /* if */
    if (match && 
        (
#if MICROSOFT_EXTENSIONS_ALLOWED
         (integer_type_supp(type)->uuid_string != NULL &&
          integer_type_supp(corresp_type)->uuid_string != NULL &&
          !same_str(integer_type_supp(type)->uuid_string,
                    integer_type_supp(corresp_type)->uuid_string)) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
         type->variant.integer.int_kind !=
                                     corresp_type->variant.integer.int_kind)) {
      match = FALSE;
      report_error = TRUE;
    }  /* if */
  }  /* if */
  if (!match) {
    if (report_error) {
      report_bad_trans_unit_corresp(type);
    }  /* if */
    set_no_enum_type_correspondence(type);
  }  /* if */
  return match;
}  /* verify_enum_type_correspondence */


static a_boolean equiv_base_using_decls(a_using_decl_ptr  ud1,
                                        a_using_decl_ptr  ud2)
/*
Return TRUE if the given using declarations refer to corresponding entities.
*/
{
  a_boolean  result = ud1->is_using_directive == ud2->is_using_directive &&
                      ud1->access == ud2->access &&
                      ud1->is_using_enum == ud2->is_using_enum &&
                      ud1->is_enumerator == ud2->is_enumerator &&
                      ud1->entity.kind == ud2->entity.kind;

  if (!result) {
    /* Nothing more to be tested. */
  } else if (skip_typerefs(ud1->qualifier.class_type)
                ->variant.class_struct_union.is_nonreal_class) {
    /* The using-declaration refers to a dependent base class.  In this case
       it is usually not sufficient to compare the canonical entries. */
    if (!identical_types(ud1->qualifier.class_type,
                         ud2->qualifier.class_type)) {
      /* If the qualifiers are not equivalent, there is no match. */
      result = FALSE;
    } else if (ud1->entity.kind == iek_base_class) {
      /* Using-declarations that represent inherited constructors.  See if
         the inherited constructor class types are the same. */
      check_assertion(ud1->is_inheriting_ctor && ud2->is_inheriting_ctor);
      result = (identical_types(((a_base_class_ptr)ud1->entity.ptr)->type,
                                ((a_base_class_ptr)ud2->entity.ptr)->type));
    } else if (ud1->entity.kind == iek_type ||
               ud1->entity.kind == iek_routine ||
               ud1->entity.kind == iek_variable ||
               ud1->entity.kind == iek_template) {
      a_type_ptr tp1 = NULL, tp2 = NULL;
      if (ud1->entity.kind == iek_type) {
        tp1 = (a_type_ptr)ud1->entity.ptr;
        tp2 = (a_type_ptr)ud2->entity.ptr;
      } else if (ud1->entity.kind == iek_routine) {
        tp1 = ((a_routine_ptr)ud1->entity.ptr)->type;
        tp2 = ((a_routine_ptr)ud2->entity.ptr)->type;
      } else if (ud1->entity.kind == iek_variable) {
        tp1 = ((a_variable_ptr)ud1->entity.ptr)->type;
        tp2 = ((a_variable_ptr)ud2->entity.ptr)->type;
      } else /*ud1->entity.kind == iek_template*/ {
        a_template_ptr templ1, templ2;
        templ1 = (a_template_ptr)ud1->entity.ptr;
        templ2 = (a_template_ptr)ud2->entity.ptr;
        if (templ1->kind != templ2->kind) {
          result = FALSE;
        } else if (templ1->kind == (a_template_kind)templk_class ||
                   templ1->kind == (a_template_kind)templk_member_class ||
                   templ1->kind == (a_template_kind)templk_member_enum) {
          tp1 = templ1->prototype_instantiation.type;
          tp2 = templ2->prototype_instantiation.type;
        } else if (templ1->kind == (a_template_kind)templk_function ||
                   templ1->kind == (a_template_kind)templk_member_function) {
          tp1 = templ1->prototype_instantiation.routine->type;
          tp2 = templ2->prototype_instantiation.routine->type;
        } else if (templ1->kind == (a_template_kind)templk_variable ||
                   templ1->kind == (a_template_kind)templk_static_data_member){
          tp1 = templ1->prototype_instantiation.variable->type;
          tp2 = templ2->prototype_instantiation.variable->type;
        } else {
          /* A using-declaration for a template should be using a type,
             function, or variable. */
          unexpected_condition();
        }
      }  /* if */
      if (result) {
        result = identical_types(tp1, tp2);
      }  /* if */
    } else {
      /* In all other dependent base class cases, the entity is represented
         as a (nonreal) "constant". */
      check_assertion(ud1->entity.kind == iek_constant);
      result = eq_constants((a_constant_ptr)ud1->entity.ptr, 
                            (a_constant_ptr)ud2->entity.ptr);
    }  /* if */
  } else {
    /* Non-dependent case: check that the canonical entries match up.  For
       using declarations that refer to base classes, there is no source
       correspondence data immediately associated with the pointer, so some
       additional work is required. */
    char        *entity1, *entity2;
    a_type_ptr  utp1 = skip_typerefs(ud1->qualifier.class_type),
                utp2 = skip_typerefs(ud2->qualifier.class_type);
    if (ud1->entity.kind == iek_base_class) {
      entity1=canonical_il_entry_of(((a_base_class_ptr)ud1->entity.ptr)->type);
      entity2=canonical_il_entry_of(((a_base_class_ptr)ud2->entity.ptr)->type);
    } else {
      entity1 = canonical_il_entry_of(ud1->entity.ptr);
      entity2 = canonical_il_entry_of(ud2->entity.ptr);
    }  /* if */
    result = canonical_il_entry_of(utp1) == canonical_il_entry_of(utp2) &&
             entity1 == entity2;
  }  /* if */
  return result;
}  /* equiv_base_using_decls */


static a_boolean class_specialization_conflict(a_type_ptr  type_1,
                                               a_type_ptr  type_2)
/*
The class types represented by type_1 and type_2 have different values for
the is_specialized bit.  Return TRUE is this is really a conflict (i.e., if
the nonspecialized type has a definition).
*/
{
  a_boolean  result = FALSE;

  if (!type_1->variant.class_struct_union.is_specialized &&
      class_type_has_body(type_1)) {
    result = type_2->variant.class_struct_union.is_specialized;
  } else if (!type_2->variant.class_struct_union.is_specialized &&
             class_type_has_body(type_2)) {
    result = type_1->variant.class_struct_union.is_specialized;
  }  /* if */
  return result;
}  /* class_specialization_conflict */

#if MICROSOFT_EXTENSIONS_ALLOWED

static an_inheritance_kind inheritance_kind_of(a_type_ptr  type)
/*
Return the inheritance kind for the given class type.
*/
{
  an_inheritance_kind  result = class_type_supp(type)->inheritance_kind;

  if (result == (an_inheritance_kind)ihk_none) {
    result = implied_inheritance_kind(type);
  }  /* if */
  return result;
}  /* inheritance_kind_of */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DECL_MODIFIERS_IN_USE

static a_boolean incompatible_class_decl_modifiers(ARG_UNUSED a_type_ptr  tp1,
                                                   ARG_UNUSED a_type_ptr  tp2)
/*
Return TRUE if and only if the two corresponding class types have incompatible
declaration modifiers.
*/
{
  a_boolean  result = FALSE;

#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    a_class_type_supplement_ptr  sup1 = class_type_supp(tp1),
                                 sup2 = class_type_supp(tp2);
    /* The dllimport and dllexport modifiers need not match up (dllexport
       should take precedence).  The other modifier applying to a class
       (novtable) must only match when the declarations in both translation
       units are also definitions. */
    if ((sup1->decl_modifiers & ~DM_DLLFLAGS) !=
                                      (sup2->decl_modifiers & ~DM_DLLFLAGS)) {
      result = class_type_has_body(tp1) && class_type_has_body(tp2);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return result;
}  /* incompatible_class_decl_modifiers */

#endif /* DECL_MODIFIERS_IN_USE */

static void verify_corresp_for_template_static_data_member_entities(
                                                           a_symbol_ptr  sym1,
                                                           a_symbol_ptr  sym2)
/*
Some entities may be defined in initializers of template static data members
(currently, this is limited to closure types).  For corresponding data members
represented by sym1 and sym2, verify the correspondence of those entities and
issue errors if needed.
*/
{
  if (sym1->defined && sym2->defined) {
    a_variable_ptr  var1 = sym1->variant.static_data_member.variable;
    a_variable_ptr  var2 = sym2->variant.static_data_member.variable;
    verify_corresp_for_entities_list(
                             var1->entities_defined_in_initializer,
                             var2->entities_defined_in_initializer,
                             (char*)var1, &var2->source_corresp.decl_position,
                             ec_initializer_differs_in_other_trans_unit,
                             ec_initializers_incompatible);
  }  /* if */
}  /* verify_corresp_for_template_static_data_member_entities */


static a_boolean verify_class_type_correspondence(a_type_ptr  type)
/*
Check that the recorded translation unit correspondence for the given class
type is in fact valid.
*/
{
  a_boolean   match = verify_name_correspondence(type);
  a_boolean   report_error = FALSE;
  a_boolean   both_defined = TRUE;
  a_type_ptr  corresp_type = (a_type_ptr)canonical_il_entry_of(type);
#define class_info type->variant.class_struct_union
#define corresp_info corresp_type->variant.class_struct_union
  a_class_type_supplement_ptr
              sup = class_info.extra_info,
              corresp_sup = corresp_info.extra_info;

  if (type->variant.class_struct_union.is_template_class) {
    /* We may have conservatively placed this type on the verification list
       in set_type_corresp.  To avoid double verification (and double
       diagnostics), remove it from that list. */
    remove_from_verification_list((char*)type);
  }  /* if */
  if (!match) {
    /* An error was already issued. */
  } else if (!is_immediate_class_type(corresp_type)) {
    match = FALSE;
    report_error = TRUE;
  } else if (!class_type_has_body(type) || is_incomplete_type(type) ||
             !class_type_has_body(corresp_type) ||
             is_incomplete_type(corresp_type)) {
    /* The types are assumed to match in their inner structure since at least
       one is incomplete and therefore has no inner structure to conflict
       with. */
    both_defined = FALSE;
  } else {
    /* Traverse fields: (both C and C++) */
    a_field_ptr  field = skip_generated_field(class_info.field_list);
    a_field_ptr  corresp_field = skip_generated_field(corresp_info.field_list);
    for (; field != NULL && corresp_field != NULL;
         field = skip_generated_field(field->next),
           corresp_field = skip_generated_field(corresp_field->next)) {
      if (!verify_field_correspondence(field)) {
        match = FALSE;
        goto done;
      }  /* if */
    }  /* for */
    if (!(field == NULL && corresp_field == NULL)) {
      report_error = TRUE;
      match = FALSE;
      goto done;
    }  /* if */

    if (!C_mode()) {
      /* Traverse entities only available in C++ mode. */
      a_scope_ptr  scope = sup->assoc_scope;
      a_scope_ptr  corresp_scope = corresp_sup->assoc_scope;
  
      /* Traverse member templates: */
      {
        a_template_ptr  templ = scope->templates;
        a_template_ptr  corresp_templ = corresp_scope->templates;
        for (; templ != NULL && corresp_templ != NULL;
             templ = templ->next, corresp_templ = corresp_templ->next) {
          if (!verify_template_correspondence(templ)) {
            match = FALSE;
            goto done;
          }  /* if */
        }  /* for */
        if (!(templ == NULL && corresp_templ == NULL)) {
          report_error = TRUE;
          match = FALSE;
          goto done;
        }  /* if */
      }
      /* Traverse member types: */
      {
        a_type_ptr  mem_type = skip_generated_type(scope->types);
        a_type_ptr  corresp_mem_type = skip_generated_type(
                                                        corresp_scope->types);
        for (; mem_type != NULL && corresp_mem_type != NULL;
             mem_type = skip_generated_type(mem_type->next),
             corresp_mem_type = skip_generated_type(corresp_mem_type->next)) {
          if (!verify_type_correspondence(mem_type)) {
            match = FALSE;
            goto done;
          }  /* if */
        }  /* for */
        if (!(mem_type == NULL && corresp_mem_type == NULL)) {
          report_error = TRUE;
          match = FALSE;
          goto done;
        }  /* if */
      }
      /* Traverse member routines: */
      {
        a_routine_ptr  routine = skip_generated_routine(scope->routines);
        a_routine_ptr  corresp_routine = skip_generated_routine(
                                                      corresp_scope->routines);
        for (; routine != NULL && corresp_routine != NULL;
             routine = skip_generated_routine(routine->next),
             corresp_routine = skip_generated_routine(corresp_routine->next)) {
          if (!verify_routine_correspondence(routine)) {
            match = FALSE;
            goto done;
          }  /* if */
        }  /* for */
        if (!(routine == NULL && corresp_routine == NULL)) {
          report_error = TRUE;
          match = FALSE;
          goto done;
        }  /* if */
      }
      /* Traverse static data members: */
      {
        a_variable_ptr  variable = scope->variables;
        a_variable_ptr  corresp_variable = corresp_scope->variables;
        for (; variable != NULL && corresp_variable != NULL;
             variable = variable->next,
                                  corresp_variable = corresp_variable->next) {
          if (!verify_variable_correspondence(variable)) {
            match = FALSE;
            goto done;
          } else if (
                type->variant.class_struct_union.is_prototype_instantiation) {
            verify_corresp_for_template_static_data_member_entities(
                          symbol_for(variable), symbol_for(corresp_variable));
          }  /* if */
        }  /* for */
        if (!(variable == NULL && corresp_variable == NULL)) {
          report_error = TRUE;
          match = FALSE;
          goto done;
        }  /* if */
      }
      /* Traverse member constants: */
      {
        a_constant_ptr  constant = scope->constants;
        a_constant_ptr  corresp_constant = corresp_scope->constants;
        for (; constant != NULL && corresp_constant != NULL;
             constant = constant->next,
                                  corresp_constant = corresp_constant->next) {
          if (!verify_constant_correspondence(constant)) {
            match = FALSE;
            goto done;
          }  /* if */
        }  /* for */
        if (!(constant == NULL && corresp_constant == NULL)) {
          report_error = TRUE;
          match = FALSE;
          goto done;
        }  /* if */
      }
      /* Traverse base classes. */
      {
        /* Ensure that the list of base classes is compatible across
           translation units.  If so, we set the correspondence pointers
           for the convenience of later processing stages, but note that
           this is an unusual case in that a correspondence is set for an
           entity without source correspondence. */
        a_base_class_ptr  base = type->variant.class_struct_union.extra_info
                                                                ->base_classes;
        a_base_class_ptr  corresp_base = corresp_type
                                        ->variant.class_struct_union.extra_info
                                        ->base_classes;
        for (; base != NULL && corresp_base != NULL;
             base = base->next, corresp_base = corresp_base->next) {
          if (!base->direct) {
            /* Only consider direct bases.  A mismatch in an indirect base can
               be reported on the class for which it is a direct base. */
          } else if (!identical_types(base->type, corresp_base->type) ||
                     base->is_virtual != corresp_base->is_virtual ||
                     base->derivation->access !=
                                            corresp_base->derivation->access) {
            match = FALSE;
            report_error = TRUE;
            goto done;
          }  /* if */
          /* Set source correspondence: */
          set_trans_unit_corresp(iek_base_class, base, corresp_base);
        }  /* for */
        if (!(base == NULL && corresp_base == NULL)) {
          match = FALSE;
          report_error = TRUE;
          goto done;
        }  /* if */
      }
      /* Traverse member using declarations. */
      {
        /* Member using declaration entries do not have a correspondence
           pointer set.  However, they must match across translation units. */
        a_using_decl_ptr  ud = scope->using_declarations;
        a_using_decl_ptr  corresp_ud = corresp_scope->using_declarations;
        for (; ud != NULL && corresp_ud != NULL;
             ud = ud->next, corresp_ud = corresp_ud->next) {
          if (!equiv_base_using_decls(ud, corresp_ud)) {
            match = FALSE;
            report_error = TRUE;
            goto done;
          }  /* if */
        }  /* for */
        if (!(ud == NULL && corresp_ud == NULL)) {
          match = FALSE;
          report_error = TRUE;
          goto done;
        }  /* if */
        /* Class scopes cannot have using-directives. */
        check_assertion(scope->using_directives == NULL);
      }
      /* Traverse friend function declarations. */
      {
        /* Similar to member using declarations. */
        an_il_entity_list_entry_ptr  rle = skip_generated_friend_routine(
                                                                 sup->friends);
        an_il_entity_list_entry_ptr  corresp_rle =
                                                 skip_generated_friend_routine(
                                                         corresp_sup->friends);
        for (; rle != NULL && corresp_rle != NULL;
             rle = skip_generated_friend_routine(rle->next),
             corresp_rle = skip_generated_friend_routine(corresp_rle->next)) {
          if (canonical_il_entry_of((a_routine_ptr)rle->entity.ptr) !=
               canonical_il_entry_of((a_routine_ptr)corresp_rle->entity.ptr)) {
            match = FALSE;
            report_error = TRUE;
            goto done;
          }  /* if */
        }  /* for */
        if (!(rle == NULL && corresp_rle == NULL)) {
          match = FALSE;
          report_error = TRUE;
          goto done;
        }  /* if */
      }
      /* Traverse friend class declarations. */
      {
        /* Similar to member using declarations. */
        an_il_entity_list_entry_ptr  cle = skip_generated_friend_class(
                                                                 sup->friends);
        an_il_entity_list_entry_ptr  corresp_cle = skip_generated_friend_class(
                                                         corresp_sup->friends);
        for (; cle != NULL && corresp_cle != NULL;
             cle = skip_generated_friend_class(cle->next),
             corresp_cle = skip_generated_friend_class(corresp_cle->next)) {
          if (!corresponding_types((a_type_ptr)cle->entity.ptr,
                                   (a_type_ptr)corresp_cle->entity.ptr)) {
            match = FALSE;
            report_error = TRUE;
            goto done;
          }  /* if */
        }  /* for */
        if (!(cle == NULL && corresp_cle == NULL)) {
          match = FALSE;
          report_error = TRUE;
          goto done;
        }  /* if */
      }
    }  /* if */
  }  /* if */
  if (match) {
    /* Check various properties of the type. */
    if ((both_defined &&
         (class_info.any_const_member != corresp_info.any_const_member ||
          class_info.any_volatile_member != corresp_info.any_volatile_member ||
          class_info.any_mutable_member != corresp_info.any_mutable_member ||
          class_info.any_virtual_base_classes !=
                                       corresp_info.any_virtual_base_classes ||
          class_info.abstract != corresp_info.abstract ||
          class_info.any_virtual_functions !=
                                          corresp_info.any_virtual_functions ||
          class_info.any_pure_virtual_functions !=
                                     corresp_info.any_pure_virtual_functions ||
          class_info.any_virtual_functions_including_in_base_classes !=
                corresp_info.any_virtual_functions_including_in_base_classes ||
          class_info.originally_unnamed != corresp_info.originally_unnamed ||
          class_info.is_nonstd_anonymous_union_type !=
                                 corresp_info.is_nonstd_anonymous_union_type ||
          class_info.contains_flexible_array_member !=
                                 corresp_info.contains_flexible_array_member ||
          class_info.max_member_alignment !=
                                           corresp_info.max_member_alignment ||
          class_info.is_empty_class != corresp_info.is_empty_class ||
          (sup != NULL &&
           (sup->virtual_function_info_offset !=
                                   corresp_sup->virtual_function_info_offset ||
            sup->anonymous_union_kind != corresp_sup->anonymous_union_kind
#if MICROSOFT_EXTENSIONS_ALLOWED
            || inheritance_kind_of(type) !=
                                      inheritance_kind_of(corresp_type)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                                       )))) ||
        class_info.is_template_class != corresp_info.is_template_class ||
        class_info.is_nonreal_class != corresp_info.is_nonreal_class ||
        class_info.is_prototype_instantiation !=
                                     corresp_info.is_prototype_instantiation ||
        (class_info.is_specialized != corresp_info.is_specialized &&
         class_specialization_conflict(type, corresp_type)) ||
        (sup != NULL &&
         (
#if NEAR_AND_FAR_ALLOWED
          sup->qualifiers != corresp_sup->qualifiers ||
#endif /* NEAR_AND_FAR_ALLOWED */
#if DECL_MODIFIERS_IN_USE
          incompatible_class_decl_modifiers(type, corresp_type) ||
#endif /* DECL_MODIFIERS_IN_USE */
#if MICROSOFT_EXTENSIONS_ALLOWED
          (sup->uuid_string != NULL && corresp_sup->uuid_string != NULL &&
           !same_str(sup->uuid_string, corresp_sup->uuid_string)) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          !corresponding_fields(sup->anonymous_union_field,
                                corresp_sup->anonymous_union_field)))) {
      match = FALSE;
      report_error = TRUE;
    }  /* if */
#undef class_info
#undef corresp_info
  }  /* if */
done:
  if (!match) {
    if (report_error) {
      report_bad_trans_unit_corresp(type);
    }  /* if */
  }  /* if */
  return match;
}  /* verify_class_type_correspondence */


static a_boolean verify_type_correspondence(a_type_ptr  type)
/*
Check that the recorded translation unit correspondence for the given type
is in fact valid.
*/
{
  a_boolean     match;
  a_symbol_ptr  type_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
  a_type_ptr    corresp_type;
  a_boolean     both_defined, both_visible;
  a_source_correspondence_ptr
                scp, corresp_scp;
  a_trans_unit_corresp_ptr
                tcp = trans_unit_corresp_of(type);

  if (tcp == NULL) {
    /* This should only happen if an error prevents us from setting
       correspondences on all entities in secondary translation units. */
    expect_error();
    corresp_type = type;
  } else {
    corresp_type = (a_type_ptr)tcp->canonical;
    if (type == corresp_type) {
      /* This is the canonical entry.  If applicable, verify the entry in
         the primary translation unit against this one.  Otherwise, nothing
         needs to be done. */
      a_type_ptr  prim = (a_type_ptr)tcp->primary;
      if (prim != NULL) {
        type = prim;
      }  /* if */
    }  /* if */
  }  /* if */
  check_assertion(corresp_type != NULL);
  scp = &type->source_corresp,
  corresp_scp = &corresp_type->source_corresp;
  both_defined = type_has_definition(type) &&
                 type_has_definition(corresp_type);
  both_visible = !symbol_for(type)->is_invisible &&
                 !symbol_for(corresp_type)->is_invisible;
  if (type == corresp_type) {
    match = TRUE;
    if (tcp != NULL && is_immediate_enum_type(type)) {
      check_for_enumerator_conflicts(type);
    }  /* if */
  } else if (type_sym == NULL) {
    /* This must be a built-in type: It needs no checking. */
    match = TRUE;
  } else {
    /* The usual case: class and enumeration types must have their inner
       structure checked. */
    if (!verify_name_correspondence(type)) {
      match = FALSE;
      set_no_trans_unit_corresp(iek_type, type);
    } else if (is_immediate_class_type(type)) {
      /* corresp_type is also a class type since the type kinds are
         identical. */
      match = verify_class_type_correspondence(type);
    } else if (is_immediate_enum_type(type)) {
      match = verify_enum_type_correspondence(type);
    } else {
      match = identical_types(type, corresp_type) &&
              same_exception_spec(type, corresp_type);
      if (!match) {
        process_bad_trans_unit_corresp(iek_type, type, corresp_type);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Check some other general type properties.  Symbols such as
     std::align_val_t can be pre-defined but invisible, and overwritten by
     actual declarations later.  Don't issue errors when one has been
     overwritten but the other hasn't. */
  if (match && both_visible && type != corresp_type &&
      ((type->kind != corresp_type->kind &&
        /* "class" and "struct" are interchangeable if not both entries are
           definitions. */
        !(!both_defined && is_class_or_struct(type) &&
                           is_class_or_struct(corresp_type))) ||
       (both_defined && (type->size != corresp_type->size ||
                         type->alignment != corresp_type->alignment)) ||
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
       type->use_cfront_transitional_nested_type_name_mangling !=
             corresp_type->use_cfront_transitional_nested_type_name_mangling ||
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
       type->is_builtin_va_list != corresp_type->is_builtin_va_list ||
       scp->access != corresp_scp->access ||
       scp->name_linkage != corresp_scp->name_linkage)) {
    match = FALSE;
    /* These kinds of mismatches were not reported earlier, so report them
       now. */
    report_bad_trans_unit_corresp(type);
  }  /* if */
  if (match) {
    verify_attributes_correspondence(type, corresp_type, iek_type);
  }  /* if */
  return match;
}  /* verify_type_correspondence */


static a_boolean specialized_vs_generic_class_template_conflict(
                                      a_template_symbol_supplement_ptr  tssp1,
                                      a_template_symbol_supplement_ptr  tssp2)
/*
Return TRUE if one of the class templates associated with the given pointers
is a specialization while the other one is generic and has been used.
(Note that only member templates can be specializations.)
*/
{
  a_boolean  result =
             tssp1->is_specific_definition != tssp2->is_specific_definition &&
             ((!tssp1->is_specific_definition &&
               tssp1->variant.class_template.any_full_instantiations) ||
              (!tssp2->is_specific_definition &&
               tssp2->variant.class_template.any_full_instantiations));
  return result;
}  /* specialized_vs_generic_class_template_conflict */


static a_boolean is_exported(a_template_ptr  templ)
/* Return TRUE if the given template was declared as exported in its
translation unit.
*/
{
  return templ->canonical_template != NULL ?
                  templ->canonical_template->is_exported : templ->is_exported;
}  /* is_exported */


static a_boolean verify_template_correspondence(a_template_ptr  templ)
/*
Check that the recorded translation unit correspondence for the given template
is in fact valid.
*/
{
  a_boolean       match = TRUE;
  a_symbol_ptr    templ_sym = symbol_for(templ);

  if (trans_unit_corresp_of(templ) != NULL) {
    a_template_ptr  corresp_templ =
                                 (a_template_ptr)canonical_il_entry_of(templ);
    a_symbol_ptr    corresp_sym;
    a_template_symbol_supplement_ptr
                    tssp = NULL, corresp_tssp = NULL;
    /* Use the canonical template entry for scp since it has the correct
       value for "scp->access". */
    a_source_correspondence_ptr
                    scp, corresp_scp;
    if (templ == corresp_templ) {
      /* This is the canonical entry.  If applicable, verify the entry in
         the primary translation unit against this one.  Otherwise, nothing
         needs to be done. */
      a_template_ptr  prim =
                        (a_template_ptr)trans_unit_corresp_of(templ)->primary;
      if (prim != NULL && templ != prim) {
        corresp_templ = templ;
        templ = prim;
      } else {
        goto done;
      }  /* if */
    }  /* if */
    corresp_sym = symbol_for(corresp_templ);
    scp = &templ->canonical_template->source_corresp,
    corresp_scp = &corresp_templ->source_corresp;
    match = templ->kind == corresp_templ->kind;
    if (match && is_template_symbol(templ_sym)) {
      /* templ_sym could also be an ordinary member function. */
      tssp = templ_sym->variant.template_info;
      corresp_tssp = corresp_sym->variant.template_info;
    }  /* if */
    if (match &&
        (scp->access != corresp_scp->access ||
         scp->name_linkage != corresp_scp->name_linkage ||
         (!is_class_template_symbol(templ_sym) && !is_type_symbol(templ_sym) &&
          is_exported(templ) != is_exported(corresp_templ)) ||
         (tssp != NULL &&
          ((is_class_template_symbol(templ_sym) &&
            tssp->variant.class_template.is_alias_template !=
                    corresp_tssp->variant.class_template.is_alias_template) ||
           !equiv_template_param_lists(templ_params_of(corresp_sym),
                                       templ_params_of(templ_sym),
                                       /*issue_errors=*/FALSE,
                                       ETP_NO_OPTIONS,
                                       &templ_sym->decl_position, es_error) ||
           /* Check if a (member) class template was specialized in one
              translation unit, but generated in the other.  To avoid
              duplicate diagnostics, this is only done for the canonical
              template. */
           (is_class_template_symbol(templ_sym) &&
            templ->canonical_template == templ &&
            specialized_vs_generic_class_template_conflict(tssp,
                                                           corresp_tssp)))))) {
      match = FALSE;
      process_bad_trans_unit_corresp(iek_template, templ, corresp_templ);
    }  /* if */
    if (!match) {
      /* The templates don't seem to match, so don't try to verify the
         instantiations. */
    } else if (is_class_template_symbol(templ_sym)) {
      /* A class or alias template. Verify the instantiations (if any). */
      a_symbol_ptr  proto_sym = prototype_template_of(templ_sym)
                              ->variant.template_info
                              ->variant.class_template.prototype_instantiation,
                    corresp_proto_sym = prototype_template_of(corresp_sym)
                              ->variant.template_info
                              ->variant.class_template.prototype_instantiation;
      a_symbol_list_entry_ptr
                    slep;
      if (symbol_is(proto_sym, sk_type)) {
        /* An alias template. */
        /* First process the prototype instantiation. */
        match = verify_type_correspondence(proto_sym->variant.type.ptr);
        if (match) {
          /* Only check real instantiations if the prototype instantiation
             matched. */
          for (slep = tssp->variant.class_template.instantiations;
               slep != NULL; slep = slep->next) {
            a_type_ptr  inst_type = slep->symbol->variant.type.ptr;
            (void)verify_type_correspondence(inst_type);
          }  /* for */
        }  /* if */
      } else {
        /* An ordinary class template. */
        a_type_ptr  proto = proto_sym->variant.class_struct_union.type,
                    corresp_proto = corresp_proto_sym
                                             ->variant.class_struct_union.type;
        an_equiv_templ_arg_options_set
                    eta_options = ETA_NO_OPTIONS;
        if (tssp->is_variadic || corresp_tssp->is_variadic) {
          eta_options |= ETA_IS_VARIADIC;
        }  /* if */
        /* For partial specializations we must also verify the template
           arguments (attached to the prototype instantiations). */
        if (!equiv_template_arg_lists(
                            class_type_supp(proto)->template_arg_list,
                            class_type_supp(corresp_proto)->template_arg_list,
                            eta_options)) {
          match = FALSE;
          process_bad_trans_unit_corresp(iek_template, templ, corresp_templ);
        } else if (templ->is_friend_template && proto_sym->defined) {
          /* If this is a friend class template, checking its inner structure
             could lead to unbounded recursion since the class template may be
             an enclosing template.  However, since it is defined, there must
             be another template entry for it, and the checking can be done
             for that one. */
        } else {
          /* First process the prototype instantiation. */
          match = verify_type_correspondence(proto);
          if (match) {
            /* Only check real instantiations if the prototype instantiation
               matched. */
            for (slep = tssp->variant.class_template.instantiations;
                 slep != NULL; slep = slep->next) {
              a_symbol_ptr inst = slep->symbol;
              a_type_ptr   inst_type = type_symbol_type(inst);
              (void)verify_type_correspondence(inst_type);
            }  /* for */
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (templ_sym->kind == (a_symbol_kind)sk_function_template) {
      /* A function template.  Verify the instantiations (if any). */
      a_template_instance_ptr  inst = tssp->variant.function.instantiations;
      for (; inst != NULL; inst = inst->next) {
        (void)verify_routine_correspondence(
                                      inst->instance_sym->variant.routine.ptr);
      }  /* for */
      /* Also process prototype instantiation. */
      (void)verify_routine_correspondence(tssp->variant.function.routine);
    }  /* if */
  }  /* if */
done:
  return match;
}  /* verify_template_correspondence */


static a_boolean verify_namespace_correspondence(a_namespace_ptr  nsp)
/*
If the given namespace has a canonical entry in another translation unit,
and assuming it already matches in name, kind and scope attributes, verify
that any other significant attributes also match.  Only namespace aliases
have such an attribute: the aliased namespace.
*/
{
  a_boolean        match = TRUE;
  a_namespace_ptr  corresp_nsp = (a_namespace_ptr)canonical_il_entry_of(nsp);

  if (nsp->is_namespace_alias) {
    a_namespace_ptr  unaliased_nsp = skip_namespace_aliases(nsp);
    a_namespace_ptr  other_nsp = skip_namespace_aliases(corresp_nsp);
    if (canonical_il_entry_of(unaliased_nsp) !=
                                           canonical_il_entry_of(other_nsp)) {
      report_bad_trans_unit_corresp(nsp);
      match = FALSE;
    }  /* if */
  }  /* if */
  if (match) {
    verify_attributes_correspondence(nsp, corresp_nsp, iek_namespace);
  }  /* if */
  return match;
}  /* verify_namespace_correspondence */


static void verify_namespace_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of namespaces of the given scope and verify a
translation unit correspondence pointer for each of them.
*/
{
  a_namespace_ptr  nsp;

  /* Visit all namespaces. */
  for (nsp = scope->namespaces;
       nsp != NULL;
       nsp = nsp->next) {
    if (has_name(nsp)) {
      if (!verify_namespace_correspondence(nsp)) {
        /* Some error occurred---clear the association. */
        set_no_trans_unit_corresp(iek_namespace, nsp);
      } else if (!nsp->is_namespace_alias) {
        /* Verify correspondences for nested namespaces. */
        verify_trans_unit_correspondences_for_scope(nsp->variant.assoc_scope);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* verify_namespace_correspondences_for_scope */


static void verify_type_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of types of the given scope and verify a translation unit
correspondence pointer for each of them.
*/
{
  a_type_ptr  type;

  /* Visit all types. */
  for (type = skip_generated_type(scope->types);
       type != NULL;
       type = skip_generated_type(type->next)) {
    if (trans_unit_corresp_of(type) == NULL) {
      /* Some types (e.g., C-mode types not participating in entities with
         linkage) may not have a correspondence yet.  If no correspondence
         is found later on, the entry will be marked as visited by
         set_correspondence_of_unvisited_entries eventually. */
    } else {
      a_symbol_ptr  type_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
      if (type_sym != NULL && may_have_correspondence(type_sym)) {
        (void)verify_type_correspondence(type);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* verify_type_correspondences_for_scope */


static void verify_routine_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of routines of the given scope and verify a translation
unit correspondence pointer for each of them.
*/
{
  a_routine_ptr  routine;

  /* Visit all routines. */
  for (routine = scope->routines;
       routine != NULL;
       routine = routine->next) {
    if (trans_unit_corresp_of(routine) != NULL &&
        !verify_routine_correspondence(routine)) {
      /* Some error occurred---clear the association. */
      set_no_trans_unit_corresp(iek_routine, routine);
    }  /* if */
  }  /* for */
}  /* verify_routine_correspondences_for_scope */


static void verify_variable_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of variables of the given scope and verify a translation
unit correspondence pointer for each of them.
*/
{
  a_variable_ptr  variable;

  /* Visit all variables. */
  for (variable = scope->variables;
       variable != NULL;
       variable = variable->next) {
    if (trans_unit_corresp_of(variable) != NULL &&
        !verify_variable_correspondence(variable)) {
      /* Some error occurred---clear the association. */
      set_no_trans_unit_corresp(iek_variable, variable);
    }  /* if */
  }  /* for */
}  /* verify_variable_correspondences_for_scope */


static void verify_template_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of templates of the given scope and verify a translation
unit correspondence pointer for each of them.
*/
{
  a_template_ptr  templ;

  /* Visit all templates. */
  for (templ = scope->templates;
       templ != NULL;
       templ = templ->next) {
    if (trans_unit_corresp_of(templ) != NULL &&
        !verify_template_correspondence(templ)) {
      /* Some error occurred---clear the association. */
      set_no_trans_unit_corresp(iek_template, templ);
    }  /* if */
  }  /* for */
}  /* verify_template_correspondences_for_scope */


static void establish_trans_unit_correspondences_for_enum(a_type_ptr  type)
/*
Establish correspondences for the list of constants associated with the
given enum type.
*/
{
  a_type_ptr      corresp_type = (a_type_ptr)canonical_type_entry_of(type);

  if (corresp_type != NULL && corresp_type != type) {
    a_constant_ptr  enumerator = enum_constants(type);
    a_constant_ptr  corresp_enumerator = enum_constants(corresp_type);
    for (; enumerator != NULL; enumerator = enumerator->next) {
      if (corresp_enumerator != NULL) {
        set_trans_unit_corresp(iek_constant, enumerator, corresp_enumerator);
        corresp_enumerator = corresp_enumerator->next;
      } else {
        set_no_trans_unit_corresp(iek_constant, enumerator);
      }  /* if */
    }  /* for */
  }  /* for */
}  /* establish_trans_unit_correspondences_for_enum */


static void set_corresp_for_associated_templates(a_symbol_ptr  sym,
                                                 a_symbol_ptr  corresp_sym)
/*
The given symbols correspond to members of a prototype instantiation.  If
these members are defined outside their class, they may have an associated
template entry.  If so, this function establishes a correspondence between
those template entries.
*/
{
  a_template_symbol_supplement_ptr
                   tssp = template_supplement_for_symbol(sym),
                   corresp_tssp = template_supplement_for_symbol(corresp_sym);

  if (tssp != NULL && corresp_tssp != NULL) {
    a_template_ptr  templ = tssp->il_template_entry;
    a_template_ptr  corresp_templ = corresp_tssp->il_template_entry;
    if (templ != NULL && corresp_templ != NULL &&
        templ->kind == corresp_templ->kind) {
      set_trans_unit_corresp(iek_template, templ, corresp_templ);
    }  /* if */
  }  /* if */
}  /* set_corresp_for_associated_templates */


static void set_no_corresp_for_entities_list(an_il_entity_list_entry_ptr  ep)
/*
Mark all entities on the given list as having no correspondences.  (Currently,
the lists only contain closure types.)
*/
{
  for (; ep != NULL; ep = ep->next) {
    /* Currently we expect only closure types here. */
    a_type_ptr  tp;
    check_assertion(ep->entity.kind == iek_type);
    tp = (a_type_ptr)ep->entity.ptr;
    check_assertion(type_is_lambda_closure(tp));
    clear_type_correspondence(tp, /*visited=*/TRUE);
  }  /* for */
}  /* set_no_corresp_for_entities_list */


static void set_corresp_for_entities_list(an_il_entity_list_entry_ptr  ep1,
                                          an_il_entity_list_entry_ptr  ep2)
/*
Make the entities from the first list (ep1) correspond to those of the
second list (ep2).  (Currently, the lists only contain closure types.)
*/
{
  for (; ep1 != NULL && ep2 != NULL; ep1 = ep1->next, ep2 = ep2->next) {
    /* Currently we expect only closure types here. */
    a_type_ptr  tp1, tp2;
    check_assertion(ep1->entity.kind == iek_type &&
                    ep2->entity.kind == iek_type);
    tp1 = (a_type_ptr)ep1->entity.ptr;
    tp2 = (a_type_ptr)ep2->entity.ptr;
    check_assertion(type_is_lambda_closure(tp1) &&
                    type_is_lambda_closure(tp2));
    set_type_corresp(tp1, tp2);
  }  /* for */
  /* If any items remain on the list, they have no correspondence. */
  set_no_corresp_for_entities_list(ep1);
  set_no_corresp_for_entities_list(ep2);
}  /* set_corresp_for_entities_list */


static void set_corresp_for_routines(a_routine_ptr  rp1,
                                     a_routine_ptr  rp2)
/*
Set the correspondence for routine rp1 to routine rp2.  Some entities may be
defined in default arguments (currently, this is limited to closure types).
If such entities are defined in default arguments appearing in a class
definition, a template, or an inline function, they are also made to
correspond.
*/
{
  a_type_ptr  rtp1 = rp1->type, rtp2 = rp2->type;
  a_boolean   inline_or_template_case;

  set_trans_unit_corresp(iek_routine, rp1, rp2);
  inline_or_template_case =
                     (rp1->is_inline && rp2->is_inline) ||
                     (rp1->is_template_function && rp2->is_template_function);
  if (!inline_or_template_case && !rp1->source_corresp.is_class_member) {
    /* Not a case where default arguments must match. */
    check_assertion(!rp2->source_corresp.is_class_member);
  } else if (rtp1->kind == (a_type_kind)tk_routine &&
             rtp2->kind == (a_type_kind)tk_routine) {
    a_param_type_ptr  ptp1 = rtp1->variant.routine.extra_info->param_type_list;
    a_param_type_ptr  ptp2 = rtp2->variant.routine.extra_info->param_type_list;
    for (; ptp1 != NULL && ptp2 != NULL;
           ptp1 = ptp1->next, ptp2 = ptp2->next) {
      an_il_entity_list_entry_ptr  ep1 = ptp1->entities_defined_in_default_arg;
      an_il_entity_list_entry_ptr  ep2 = ptp2->entities_defined_in_default_arg;
      if (!inline_or_template_case &&
          !ptp1->default_arg_appeared_in_class_definition &&
          !ptp2->default_arg_appeared_in_class_definition) {
        /* Any entities defined in the default arguments associated with this
           parameter pair aren't required to correspond. */
      } else {
        set_corresp_for_entities_list(ep1, ep2);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* set_corresp_for_routines */


void record_default_arg_instantiation(a_routine_ptr     rp1,
                                      a_param_type_ptr  ptp1)
/*
The default argument corresponding to ptp1 for the template function rp1 was
instantiated.  If necessary set the correspondences for any entities defined
by that default argument.
*/
{
  a_routine_ptr     rp2 = canonical_routine_entry_of(rp1);
  a_type_ptr        rtp1 = rp1->type, rtp2 = rp2->type;

  check_assertion(rtp1->kind == (a_type_kind)tk_routine &&
                  rtp2->kind == (a_type_kind)tk_routine);
  if (rp1 == rp2) {
    /* rp1 is canonical: Entities defined by the default argument are
       canonical too. */
    set_no_corresp_for_entities_list(ptp1->entities_defined_in_default_arg);
  } else {
    a_param_type_ptr  ptp2 = rtp2->variant.routine.extra_info->param_type_list;
    a_param_type_ptr  ptp = rtp1->variant.routine.extra_info->param_type_list;
    /* Find the parameter corresponding to ptp1 in rp2. */
    while (ptp != ptp1 && ptp2 != NULL) {
      ptp = ptp->next;
      ptp2 = ptp2->next;
      check_assertion(ptp != NULL);
    }  /* while */
    if (ptp2 == NULL) {
      /* The routine types don't correspond. */
      expect_error();
      set_no_corresp_for_entities_list(ptp1->entities_defined_in_default_arg);
    } else {
      set_corresp_for_entities_list(ptp1->entities_defined_in_default_arg,
                                    ptp2->entities_defined_in_default_arg);
    }  /* if */
  }  /* if */
}  /* record_default_arg_instantiation */


static void set_corresp_for_template_static_data_member_entities(
                                                           a_symbol_ptr  sym1,
                                                           a_symbol_ptr  sym2)
/*
Some entities may be defined in initializers of template static data members
(currently, this is limited to closure types).  For corresponding data members
represented by sym1 and sym2, make those entities correspond.
*/
{
  if (sym1->defined && sym2->defined) {
    a_variable_ptr  var1 = sym1->variant.static_data_member.variable;
    a_variable_ptr  var2 = sym2->variant.static_data_member.variable;
    set_corresp_for_entities_list(var1->entities_defined_in_initializer,
                                  var2->entities_defined_in_initializer);
  }  /* if */
}  /* set_corresp_for_template_static_data_member_entities */


static void establish_trans_unit_correspondences_for_class(a_type_ptr  type)
/*
Set the correspondence pointers in the members of a type.  The members' types
are not checked.
*/
{
  a_type_ptr  corresp_type = (a_type_ptr)canonical_il_entry_of(type);

  check_assertion(type != NULL);
  if (corresp_type == type) {
    /* The canonical entry: nothing to be done. */
  } else if (!is_immediate_class_type(corresp_type)) {
    /* An error: caught elsewhere. */
  } else if (is_incomplete_type(type) || !class_type_has_body(type)) {
    /* No members to traverse.  Note: A class occasionally may be complete
       without having a body (e.g., nonreal class types). */
  } else if (is_incomplete_type(corresp_type) ||
             !class_type_has_body(corresp_type)) {
    /* The corresponding entry has no definition: mark the members as
       visited. */
    set_no_class_type_correspondence(type);
  } else {
    /* Traverse fields: */
    {
      a_field_ptr  field = skip_generated_field(
                                 type->variant.class_struct_union.field_list);
      a_field_ptr  corresp_field = skip_generated_field(corresp_type->
                                       variant.class_struct_union.field_list);
      for (; field != NULL && corresp_field != NULL;
           field = skip_generated_field(field->next),
             corresp_field = skip_generated_field(corresp_field->next)) {
        set_trans_unit_corresp(iek_field, field, corresp_field);
      }  /* for */
    }

    if (!C_mode()) {
      /* Traverse entities only available in C++ mode. */
      a_scope_ptr  scope = type->
                           variant.class_struct_union.extra_info->assoc_scope;
      a_scope_ptr  corresp_scope = corresp_type->
                           variant.class_struct_union.extra_info->assoc_scope;
    
      /* Traverse member templates: */
      {
        a_template_ptr  templ = scope->templates;
        a_template_ptr  corresp_templ = corresp_scope->templates;
        a_boolean       error_issued = FALSE;
        for (; templ != NULL && corresp_templ != NULL;
             templ = templ->next, corresp_templ = corresp_templ->next) {
          if (templ->kind != corresp_templ->kind || error_issued) {
            /* Could only be due to an error. */
            if (!error_issued) {
              report_corresp_error(
                                 (char*)templ,
                                 &corresp_templ->source_corresp.decl_position,
                                 ec_entity_differs_in_other_trans_unit,
                                 ec_corresp_member_template_is_different_kind);
              error_issued = TRUE;
            }  /* if */
            set_no_trans_unit_corresp(iek_template, templ);
            clear_instantiation_correspondences(templ, /*visited=*/TRUE);
          } else {
            an_il_entry_kind  entry_kind = (an_il_entry_kind)iek_template;
#if PROTOTYPE_INSTANTIATIONS_IN_IL
            if (prototype_instantiations_in_il) {
              /* When prototype instantiations are recorded in the IL, the
                 list of templates may also contain prototype instantiations.
                 These need to be treated differently. */
              a_symbol_ptr  templ_sym =
                                (a_symbol_ptr)templ->source_corresp.assoc_info;
              if (templ_sym->kind != (a_symbol_kind)sk_class_template &&
                  templ_sym->kind != (a_symbol_kind)sk_function_template) {
                (void)il_entry_for_symbol(templ_sym, &entry_kind);
              }  /* if */
            }  /* if */
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */
            set_trans_unit_corresp(iek_template, templ, corresp_templ);
            if (entry_kind == iek_template) {
              establish_instantiation_correspondences(templ, corresp_templ);
            }  /* if */
          }  /* if */
        }  /* for */
      }
      /* Traverse member types: */
      {
        a_type_ptr  mem_type = skip_generated_type(scope->types);
        a_type_ptr  corresp_mem_type =
                                    skip_generated_type(corresp_scope->types);
        for (; mem_type != NULL && corresp_mem_type != NULL;
             mem_type = skip_generated_type(mem_type->next),
             corresp_mem_type = skip_generated_type(corresp_mem_type->next)) {
          set_type_corresp(mem_type, corresp_mem_type);
          if (is_immediate_class_type(mem_type)) {
            /* This could be a member of a template class.  If we're dealing
               with a prototype instantiation, this is a good opportunity to
               get to any a_template entry associated with an out-of-class
               definition of the nested class. */
            if (mem_type
                    ->variant.class_struct_union.is_prototype_instantiation) {
              a_symbol_ptr  sym = symbol_for(mem_type);
              a_symbol_ptr  corresp_sym = symbol_for(corresp_mem_type);
              a_template_symbol_supplement_ptr
                            tssp = template_supplement_for_symbol(sym),
                            corresp_tssp =
                                  template_supplement_for_symbol(corresp_sym);
              if (tssp != NULL && corresp_tssp != NULL &&
                  tssp->il_template_entry != NULL &&
                  corresp_tssp->il_template_entry != NULL) {
                set_trans_unit_corresp(iek_template,
                                       tssp->il_template_entry,
                                       corresp_tssp->il_template_entry);
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* for */
      }
      /* Traverse member routines: */
      {
        a_routine_ptr  routine = skip_generated_routine(scope->routines);
        a_routine_ptr  corresp_routine = skip_generated_routine(
                                                      corresp_scope->routines);
        for (; routine != NULL && corresp_routine != NULL;
             routine = skip_generated_routine(routine->next),
             corresp_routine = skip_generated_routine(corresp_routine->next)) {
          if (routine->compiler_generated ==
                                         corresp_routine->compiler_generated &&
              routine->is_template_function ==
                                       corresp_routine->is_template_function) {
            set_corresp_for_routines(routine, corresp_routine);
          } else {
            /* Do not set up a correspondence in this case because it could
               confuse master instance processing (the compiler generated
               case has no instance pointer). */
            report_bad_trans_unit_corresp(type);
            if (trans_unit_corresp_of(routine) == NULL ||
                (a_routine_ptr)canonical_il_entry_of(routine) != routine) {
              set_no_trans_unit_corresp(iek_routine, routine);
            }  /* if */
            if (trans_unit_corresp_of(corresp_routine) == NULL ||
                (a_routine_ptr)canonical_il_entry_of(corresp_routine) !=
                                                             corresp_routine) {
              set_no_trans_unit_corresp(iek_routine, corresp_routine);
            }  /* if */
          }  /* if */
          /* This could be a member of a template class.  If we're dealing
             with a prototype instantiation, this is a good opportunity to
             get to any a_template entry associated with an out-of-class
             definition of the member function. */
          if (routine->is_prototype_instantiation &&
              !routine->is_in_class_specialization &&
              corresp_routine->is_prototype_instantiation &&
              !corresp_routine->is_in_class_specialization) {
            a_symbol_ptr  sym = symbol_for(routine),
                          corresp_sym = symbol_for(corresp_routine);
            set_corresp_for_associated_templates(sym, corresp_sym);
          }  /* if */
        }  /* for */
      }
      /* Traverse static data members: */
      {
        a_variable_ptr  var = scope->variables;
        a_variable_ptr  corresp_var = corresp_scope->variables;
        for (; var != NULL && corresp_var != NULL;
             var = var->next, corresp_var = corresp_var->next) {
          a_symbol_ptr  sym = symbol_for(var);
          a_symbol_ptr  corresp_sym = symbol_for(corresp_var);
          if (sym != NULL && corresp_sym != NULL &&
              (sym->variant.static_data_member.instance_ptr == 0) !=
                 (corresp_sym->variant.static_data_member.instance_ptr == 0)) {
            /* One of the members is template instance, the other not.
               Avoid setting a correspondence in that case. */
            f_report_bad_trans_unit_corresp((char*)var,
                                            &corresp_sym->decl_position);
            if (trans_unit_corresp_of(var) == NULL) {
              set_no_trans_unit_corresp(iek_variable, var);
            }  /* if */
          } else {
            set_trans_unit_corresp(iek_variable, var, corresp_var);
          }  /* if */
          if (type->variant.class_struct_union.is_prototype_instantiation) {
            /* Establish a correspondence between the template entries
               associated with these static data members (if applicable). */
            set_corresp_for_associated_templates(sym, corresp_sym);
            /* Also establish correspondences between any lambdas defined in
               the initializers for this static data member (if any). */
            set_corresp_for_template_static_data_member_entities(sym,
                                                                 corresp_sym);
          }  /* if */
        }  /* for */
      }
      /* Traverse member constants: */
      {
        a_constant_ptr  constant = scope->constants;
        a_constant_ptr  corresp_constant = corresp_scope->constants;
        for (; constant != NULL && corresp_constant != NULL;
             constant = constant->next,
                                  corresp_constant = corresp_constant->next) {
          set_trans_unit_corresp(iek_constant, constant, corresp_constant);
        }  /* for */
      }
      /* Traverse friend functions. */
      {
        /* It is possible for a namespace scope function to only be declared
           in a friend declaration.  If the friend declaration appears in a
           template, it will not appear in the routines list until the class
           is instantiated.  To ensure that it has its correspondence set,
           we intercept such functions here. */
        an_il_entity_list_entry_ptr
           rle = skip_generated_friend_routine(
                                 type->variant.class_struct_union.extra_info
                                     ->friends),
           corresp_rle = skip_generated_friend_routine(
                         corresp_type->variant.class_struct_union.extra_info
                                     ->friends);
        for (; rle != NULL && corresp_rle != NULL;
             rle = skip_generated_friend_routine(rle->next),
             corresp_rle = skip_generated_friend_routine(corresp_rle->next)) {
          /* This may be the only opportunity to set a correspondence. */
          a_routine_ptr  routine = (a_routine_ptr)rle->entity.ptr,
                         corresp_routine =
                                        (a_routine_ptr)corresp_rle->entity.ptr;
          a_symbol_ptr  friend_sym = symbol_for(routine),
                        corresp_friend_sym = symbol_for(corresp_routine);
          if (same_name(routine, corresp_routine) &&
              (trans_unit_corresp_of(routine) == NULL ||
               trans_unit_corresp_of(corresp_routine) == NULL) &&
              ((may_have_correspondence(friend_sym) &&
                may_have_correspondence(corresp_friend_sym)) ||
#if MICROSOFT_EXTENSIONS_ALLOWED
               /* Enable a correspondence between an entry that only occurred
                  because of a nondefining friend declaration, even when the
                  other entry has internal linkage. */
               (microsoft_mode && microsoft_compatible_friend_linkage(
                                                 routine, corresp_routine)) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
               /* Allow correspondences on friend declarations even if they
                  have internal linkage as a consequence of being inline. */
               (routine->is_inline && corresp_routine->is_inline)) &&
              same_parents(friend_sym, corresp_friend_sym) &&
              (param_types_are_compatible(
                                       routine->type, corresp_routine->type,
                                       TCF_REDECLARATION | TCF_SEEK_CORRESP) ||
               /* The function ::main doesn't overload. */
               (is_main_function(routine) &&
                is_main_function(corresp_routine)))) {
            set_corresp_for_routines(routine, corresp_routine);
          }  /* if */
        }  /* for */
      }
      /* Traverse friend classes. */
      {
        /* Just as with friend functions, it is possible that classes are
           only declared in class template instantiations. */
        an_il_entity_list_entry_ptr
           cle = skip_generated_friend_class(
                         type->variant.class_struct_union.extra_info->friends),
           corresp_cle = skip_generated_friend_class(
                 corresp_type->variant.class_struct_union.extra_info->friends);
        for (; cle != NULL && corresp_cle != NULL;
             cle = skip_generated_friend_class(cle->next),
             corresp_cle = skip_generated_friend_class(corresp_cle->next)) {
          a_type_ptr    friend_type = (a_type_ptr)cle->entity.ptr,
                        corresp_friend_type =
                                           (a_type_ptr)corresp_cle->entity.ptr;
          a_symbol_ptr  friend_sym = (a_symbol_ptr)friend_type->
                                                     source_corresp.assoc_info,
                        corresp_friend_sym =
                                            (a_symbol_ptr)corresp_friend_type->
                                                     source_corresp.assoc_info;
          if (same_name(friend_type, corresp_friend_type) &&
              (trans_unit_corresp_of(friend_type) == NULL ||
               trans_unit_corresp_of(corresp_friend_type) == NULL) &&
              same_parents(friend_sym, corresp_friend_sym)) {
            set_type_corresp(friend_type, corresp_friend_type);
          }  /* if */
        }  /* for */
      }
    }  /* if */
  }  /* if */
}  /* establish_trans_unit_correspondences_for_class */


static void set_master_instance_for_new_canonical_class(
					a_type_ptr	primary_class,
					a_type_ptr	secondary_class)
/*
primary_class is a class in the primary IL and is the new canonical
entry.  secondary_class is a class in a secondary translation unit
and was formerly the canonical entry.  Update the template instances for
the variables and routines that are members of primary_class so that they
refer to the master instance entries that the template instances of the
members of secondary_class refer to.
*/
{
  a_scope_ptr  primary_scope = primary_class->
                           variant.class_struct_union.extra_info->assoc_scope;
  a_scope_ptr  secondary_scope = secondary_class->
                           variant.class_struct_union.extra_info->assoc_scope;
    
  check_assertion(primary_class->variant.class_struct_union.is_template_class);
  /* Go through the list of routines and set the master instance for the
     routines of the primary scope class. */
  {
    a_routine_ptr  primary_routine = skip_generated_routine(
                                                      primary_scope->routines);
    a_routine_ptr  secondary_routine = skip_generated_routine(
                                                    secondary_scope->routines);
    for (; primary_routine != NULL && secondary_routine != NULL;
         primary_routine = skip_generated_routine(primary_routine->next),
         secondary_routine = skip_generated_routine(secondary_routine->next)) {
      if (!primary_routine->compiler_generated) {
        set_master_instance_for_new_canonical_routine(primary_routine,
                                                      secondary_routine);
      }  /* if */
    }  /* for */
  }
  /* Go through the list of variables (static data members) and set the
     master instance for the variables of the primary scope class. */
  {
    a_variable_ptr  primary_variable = primary_scope->variables;
    a_variable_ptr  secondary_variable = secondary_scope->variables;
    for (; primary_variable != NULL && secondary_variable != NULL;
         primary_variable = primary_variable->next,
         secondary_variable = secondary_variable->next) {
      set_master_instance_for_new_canonical_variable(primary_variable,
                                                     secondary_variable);
    }  /* for */
  }
}  /* set_master_instance_for_new_canonical_class */


void establish_class_instantiation_corresp(a_type_ptr  type)
/*
Establish correspondences for members of a class template instantiation.
This routine is called when an instantiation is completed; the incomplete
type may already have had its correspondence set (or may be the canonical
entry).  This is much like establish_trans_unit_correspondences_for_class,
but for instantiations in primary translation units, we must start with the
type (if any) in a secondary translation unit whose correspondence is the
given type.
*/
{
  if (trans_unit_corresp_of(type) == NULL) {
    /* Nothing to be done: correspondences are not being processed yet. */
  } else if (any_noncorresp_errors()) {
    /* Correspondence checking is inhibited after errors are encountered. */
  } else {
    a_type_ptr  canon = (a_type_ptr)canonical_il_entry_of(type);
    a_boolean   new_canon = FALSE, match = FALSE;
    check_assertion(!is_incomplete_type(type));
    if (canon == type) {
      /* This is presumably the first class body instantiation. */
      clear_class_type_correspondence(type, /*visited=*/TRUE);
    } else if (!is_immediate_class_type(canon)) {
      expect_error();
    } else {
      a_boolean  canon_defined = !is_incomplete_type(canon);
      if (!canon_defined || !in_secondary_trans_unit(type)) {
        /* The canonical entry is about to change. */
        new_canon = TRUE;
        /* Prefer definitions as canonical entries, and definitions in primary
           translation units in particular. */
        change_canonical_entry(trans_unit_corresp_of(type), (char*)type);
        if (!canon_defined) {
          /* This is apparently the first full instantiation of this type.
             Clear the members' correspondences. */
          clear_class_type_correspondence(type, /*visited=*/TRUE);
        }  /* if */
        /* Work from the noncanonical entry to set the correspondences of
           members. */
        type = canon;
        canon = (a_type_ptr)canonical_il_entry_of(type);
      }  /* if */
      establish_trans_unit_correspondences_for_class(type);
      /* Find correspondences for any instances that might have been
         discovered. */
      process_pending_instantiations();
      if (!is_incomplete_type(canon) && !is_incomplete_type(type)) {
        if (new_canon || correspondence_checking_done) {
          /* Force the verification of the previous canonical entry against the
             new one if (a) the given type is a new canonical type and hence it
             will not be compared against the old canonical entry in a later
             stage, or (b) the normal verification pass has been completed
             already. */
          match = verify_class_type_correspondence(type);
        }  /* if */
        if (new_canon && match) {
          /* Since the canonical entry has changed, extra actions may be
             needed.  */
          if (class_type_supp(type)->assoc_scope != NULL) {
            /* The master instance is found using the canonical entry.  We are
               creating a new canonical entry, so we must make sure its master
               instance pointer is set for the class members. */
            set_master_instance_for_new_canonical_class(
                                (a_type_ptr)canonical_il_entry_of(type), type);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* establish_class_instantiation_corresp */


void establish_function_instantiation_corresp(a_routine_ptr  routine)
/*
This routine is called when the definition of the given routine has been
instantiated.  Such an event may cause routine to become the canonical entry.
(This routine may also be called for nontemplate compiler-generated class
members that are being defined because they're referenced from a template
instantiation.)  "Instantiation" is considered in a broad sense for this
routine; in particular, friend functions defined in class templates are
considered "instantiated" when their bodies have been scanned.
*/
{
  if (trans_unit_corresp_of(routine) == NULL) {
    /* Nothing to be done: correspondences are not being processed yet. */
  } else if (any_noncorresp_errors()) {
    /* Correspondence checking is inhibited after errors are encountered. */
  } else {
    a_routine_ptr  canon = (a_routine_ptr)canonical_il_entry_of(routine);
    if (canon->is_specialized && !routine->is_specialized) {
      /* The canonical entry is specialized, but we're instantiating a matching
         generic version.  (Note that Microsoft/Sun in-class specializations
         can be "instantiated" as the result of the enclosing class template
         being instantiated; this should not result in a correspondence
         error.) */
      f_report_bad_trans_unit_corresp((char*)canon,
                                      &routine->source_corresp.decl_position);
    }  /* if */
    update_canonical_entry(iek_routine, (char*)routine);
  }  /* if */
}  /* establish_function_instantiation_corresp */


void establish_variable_instantiation_corresp(a_variable_ptr  var)
/*
This routine is called when the definition of the given variable (a static
data member) has been instantiated.  Such an event may cause var to become
the canonical entry.
*/
{
  if (trans_unit_corresp_of(var) == NULL) {
    /* Nothing to be done: correspondences are not being processed yet. */
  } else if (any_noncorresp_errors()) {
    /* Correspondence checking is inhibited after errors are encountered. */
  } else {
    a_variable_ptr  canon = (a_variable_ptr)canonical_il_entry_of(var);
    if (canon->is_specialized) {
      /* The canonical entry is specialized, but we're instantiating a
         matching generic version. */
      f_report_bad_trans_unit_corresp((char*)canon,
                                      &var->source_corresp.decl_position);
    }  /* if */
    update_canonical_entry(iek_variable, (char*)var);
  }  /* if */
}  /* establish_variable_instantiation_corresp */


void establish_enum_instantiation_corresp(a_type_ptr  enum_type)
/*
This routine is called when the definition of the given enumeration has been
instantiated.  Such an event may cause enum_type to become the canonical entry.
*/
{
  if (trans_unit_corresp_of(enum_type) == NULL) {
    /* Nothing to be done: correspondences are not being processed yet. */
  } else if (any_noncorresp_errors()) {
    /* Correspondence checking is inhibited after errors are encountered. */
  } else {
    a_type_ptr  canon = (a_type_ptr)canonical_il_entry_of(enum_type);
    if (canon->variant.integer.is_specialized) {
      /* The canonical entry is specialized, but we're instantiating a
         matching generic version. */
      f_report_bad_trans_unit_corresp(
                                     (char*)canon,
                                     &enum_type->source_corresp.decl_position);
    }  /* if */
    update_canonical_entry(iek_type, (char*)enum_type);
    establish_trans_unit_correspondences_for_enum(enum_type);
  }  /* if */
}  /* establish_enum_instantiation_corresp */


static a_boolean make_type_correspond(a_type_ptr  type_1,
                                      a_type_ptr  type_2,
                                      a_boolean   *canonical_changed)
/*
Make type_1 correspond to type_2.  If canonical_changed is non-NULL and type_1
becomes the canonical entry of the correspondence set as a result of this
operation, set *canonical_changed to TRUE.  The caller is responsible for
initializing *canonical_changed.
*/
{
  /* First set the correspondence of the type entry itself. */
  set_trans_unit_corresp(iek_type, type_1, type_2);
  /* Then set the correspondence of the type's substructure. */
  if (canonical_il_entry_of(type_1) == (char*)type_1) {
    /* type_1 may have become the canonical entry, in which case we must
       use type_2 to establish the correspondence of type_1's substructure.
       This cannot be undone. */
    a_type_ptr  tmp = type_1;
    type_1 = type_2;
    type_2 = tmp;
    if (canonical_changed != NULL) {
      *canonical_changed = TRUE;
    }  /* if */
  }  /* if */
  if (is_immediate_class_type(type_1)) {
    establish_trans_unit_correspondences_for_class(type_1);
  } else if (is_immediate_enum_type(type_1)) {
    establish_trans_unit_correspondences_for_enum(type_1);
  } else {
    unexpected_condition();
  }  /* if */
  return verify_type_correspondence(type_1);
}  /* make_type_correspond */


static a_boolean change_c_type_correspondence(a_type_ptr  type_1,
                                              a_type_ptr  type_2)
/*
Change the correspondence of type_1 (and its substructure, if any) so that it
belongs to the same correspondence set as type_2.  Return TRUE if and only if
the two types are in fact compatible.  This routine should only be called in
C mode.
*/
{
  a_boolean                 match = TRUE;
  a_trans_unit_corresp_ptr  tucp1 = trans_unit_corresp_of(type_1);
  a_trans_unit_corresp_ptr  tucp2 = trans_unit_corresp_of(type_2);

  check_assertion(C_mode());
  /* First change the correspondence of the type entry itself. */
  trace_corresp_check(type_1);
  trans_unit_corresp_of(type_1) = tucp2;
  tucp1->canonical = tucp2->canonical;
  /* Then change the correspondence of the substructure. */
  if (type_1->kind != type_2->kind) {
    /* An error: The two types are of a different kind (e.g., and enum vs. a
       class type. */
    match = FALSE;
  } else if (is_immediate_class_type(type_1)) {
    if (class_type_has_body(type_1)) {
      /* In C mode, the substructure of class types consists solely of their
         fields. */
      a_field_ptr  field = type_1->variant.class_struct_union.field_list;
      check_assertion(class_type_has_body(type_2));
      for (; field != NULL; field = field->next) {
        trace_corresp_check(field);
        trans_unit_corresp_of(field) = NULL;
      }  /* for */
      /* We can now safely call establish_trans_unit_correspondences_for_class
         and verify_class_type_correspondence. */
      establish_trans_unit_correspondences_for_class(type_1);
      match = verify_class_type_correspondence(type_1);
    }  /* if */
  } else if (is_immediate_enum_type(type_1)) {
    a_constant_ptr  ecp = enum_constants(type_1);
    for (; ecp != NULL; ecp = ecp->next) {
      trans_unit_corresp_of(ecp) = NULL;
    }  /* for */
    /* We can now safely call establish_trans_unit_correspondences_for_enum
       and verify_enum_type_correspondence. */
    establish_trans_unit_correspondences_for_enum(type_1);
    match = verify_enum_type_correspondence(type_1);
  } else {
    unexpected_condition();
  }  /* if */
  return match;
}  /* change_c_type_correspondence */


static a_boolean make_c_types_correspond(a_type_ptr  type_1,
                                         a_type_ptr  type_2)
/*
C types don't have linkage, but they must "correspond" if they participate in
corresponding declarations of variables or functions with external linkage.
Unfortunately, that means that we may temporarily end up with two (or more)
correspondence entries for entities that should really all correspond.
Consider the following scenario.  Four translation units A (primary), B, C, and
D (loaded and processed in that order), all define a class type X.  Translation
unit A declares a variable v1 of type X.  B and C both declare variable v2 of
type X, and D first declares v1 and then v2.  The processing of A and B does
not cause a correspondence to be recorded for their respective types X.  The
processing of C causes its X and B's X to correspond (because of the matching
variable v1).  When processing v2 in D, D's X is made to correspond to A's X,
but then we find v1 which implies that there should really only be one
correspondence entry for all X.
If type_1 and/or type_2 have no correspondence yet, we can just make the two
correspond.  If they both do have a different correspondence, we're in a
situation like the one described above and a special procedure is needed:
The canonical entry of type_1 is made to correspond to type_2 and its original
correspondence entry is placed on a list that will be traversed later (to free
the entries).  When all translation units have been visited, but before they
are merged, set_correspondence_of_unvisited_entries is called.  In C mode, this
routine now also adjusts any type whose canonical entry does not point to the
same correspondence entry.  Return TRUE if the types are indeed identical;
return FALSE otherwise.
It is important that all C types have their correspondence set through this
routine.
*/
{
  a_boolean  result;
  a_trans_unit_corresp_ptr  tucp1 = trans_unit_corresp_of(type_1);
  a_trans_unit_corresp_ptr  tucp2 = trans_unit_corresp_of(type_2);

  check_assertion(C_mode());
  if (tucp1 == NULL) {
    result = make_type_correspond(type_1, type_2, (a_boolean*)NULL);
  } else if (tucp2 == NULL) {
    result = make_type_correspond(type_2, type_1, (a_boolean*)NULL);
  } else {
    /* Check if type_1 and/or type_2 are part of a correspondence set made
       obsolete by a previous call to this routine.  Such cases are identified
       by the fact that the canonical entry no longer belongs to the
       correspondence set itself. */
    if (tucp1 != trans_unit_corresp_of_unknown_entry(tucp1->canonical)) {
      (void)change_c_type_correspondence(type_1, (a_type_ptr)tucp1->canonical);
      tucp1 = trans_unit_corresp_of(type_1);
    }  /* if */
    if (tucp2 != trans_unit_corresp_of_unknown_entry(tucp2->canonical)) {
      (void)change_c_type_correspondence(type_2, (a_type_ptr)tucp2->canonical);
      tucp2 = trans_unit_corresp_of(type_2);
    }  /* if */
    if (tucp1 == tucp2) {
      /* The types already correspond. */
      result = TRUE;
    } else {
      /* Ensure type_2 already corresponds to the "most canonical"
         correspondence set.  If both sets have equal ranking, compare the
         ranking of type_1 and type_2, and ensure type_2 has no smaller a rank
         than type_1 (this ensures that if type_1 has a definition, type_2 will
         have one too). */
      int rank_set_1 = canonical_ranking(iek_type, tucp1->canonical);
      int rank_set_2 = canonical_ranking(iek_type, tucp2->canonical);
      if (rank_set_2 < rank_set_1 ||
          (rank_set_2 == rank_set_1 &&
           canonical_ranking((an_il_entry_kind)iek_type, (char*)type_2) <
               canonical_ranking((an_il_entry_kind)iek_type, (char*)type_1))) {
        a_type_ptr  tmp = type_2;
        type_2 = type_1;
        type_1 = tmp;
        tucp1 = trans_unit_corresp_of(type_1);
        tucp2 = trans_unit_corresp_of(type_2);
      }  /* if */
      /* Force the correspondence of type_1 to that of type_2.  We cannot use
         the C++ mode routines for this since they don't allow modifying the
         correspondence of an entry that is the canonical entry in a nontrivial
         correspondence set. */
      result = change_c_type_correspondence(type_1, type_2);
      tucp1->canonical = tucp2->canonical;
    }  /* if */
  }  /* if */
  return result;
}  /* make_c_types_correspond */


a_boolean seek_type_corresp(a_type_ptr  type_1,
                            a_type_ptr  type_2)
/*
Check if the given class or enumeration types are in fact the same and, if so,
record all the needed correspondence pointers for type_1 and return TRUE.  
Otherwise, return FALSE.
*/
{
  a_boolean    result;
  a_symbol_ptr sym_1 = (a_symbol_ptr)type_1->source_corresp.assoc_info;
  a_symbol_ptr sym_2 = (a_symbol_ptr)type_2->source_corresp.assoc_info;

  result = (canonical_il_entry_of(type_1) == canonical_il_entry_of(type_2));
  if (result || has_correspondence(type_1)) {
    /* The type is already pointing to a corresponding entry in another
       translation unit.  We only need to check if type_2 is also in the
       set of corresponding entries. */
  } else if (sym_1 != NULL && sym_2 != NULL &&
             trans_unit_for_symbol(sym_1) == trans_unit_for_symbol(sym_2)) {
    /* The two types are in the same translation unit - they cannot be
       corresponding entries. */
  } else if (!same_name(type_1, type_2)) {
    /* If the type names differ, they can certainly not correspond. */
  } else if (!il_entries_have_known_same_parents(type_1, type_2)) {
    /* If the types have different parents, they cannot correspond. */
  } else if (type_1->source_corresp.is_class_member ||
             type_2->source_corresp.is_class_member) {
    /* Member types should already have had their correspondence set if their
       parents are known to correspond.  So these two types cannot
       correspond. */
  } else if (type_is_lambda_closure(type_1) ||
             type_is_lambda_closure(type_2)) {
    /* Nonmember closure types never correspond to other closure types.
       (Member closure types are already handled above along with other
       member types.) */
  } else if (is_at_least_one_error()) {
    /* If correspondence errors already occurred, an attempt to compare
       the structure of type_1 and type_2 may end up being meaningless. */
  } else if (C_mode()) {
    /* The correspondence of C types is more complex than that of other
       entities because the types themselves have no linkage. */
    result = make_c_types_correspond(type_1, type_2);
  } else {
    /* type_1 either hasn't been visited yet, or it was found not to have a
       correspondence.  Even in the latter case it is possible that type_2
       is a corresponding entry because it might not have been considered
       earlier (e.g., because it hadn't been instantiated yet; in C mode,
       this is not possible).  To check for this possibility, we establish
       the correspondence and then verify it.  If verification finds that
       the types do not after all match, the type is restored to its
       previous state wrt. correspondence checking. */
    a_boolean  visited = (trans_unit_corresp_of(type_1) != NULL);
    clear_type_correspondence(type_1, /*visited=*/FALSE);
    result = make_type_correspond(type_1, type_2, &visited);
    if (!result && !visited && !is_at_least_one_error()) {
      /* Undo any correspondences established earlier.  This requires two
         steps: One to detach the entities from each other, and a second
         one to delete the correspondence entry altogether. */
      clear_type_correspondence(type_1, /*visited=*/TRUE);
      clear_type_correspondence(type_1, /*visited=*/FALSE);
    }  /* if */
  }  /* if */
  return result;
}  /* seek_type_corresp */


void set_correspondence_of_unvisited_entries(a_scope_ptr  scope)
/*
Traverse the type list of the given file or namespace scope and set the
correspondence for any type that has not yet been visited.  This routine is
called when the normal process of setting correspondences has been completed:
Some IL entries may not have had their correspondence set because the
correspondence could depend on other declarations (e.g., in C mode, the
correspondence of a struct or enum type is only set when the type is
involved in the declaration of an entity with linkage).
*/
{
  a_type_ptr  type;

  for (type = scope->types; type != NULL; type = type->next) {
    a_trans_unit_corresp_ptr  tucp;
    a_boolean                 is_closure = FALSE;
    /* Skip most "generated" types, but not closure types since those may
       have linkage.  (I.e., do not use skip_generated_type here.) */
    if (is_generated_type(type)) {
      if (is_immediate_class_type(type) &&
          class_type_supp(type)->is_lambda_closure_class) {
        is_closure = TRUE;
      } else {
        continue;
      }  /* if */
    }  /* if */
    tucp = trans_unit_corresp_of(type);
    if (tucp == NULL) {
      a_symbol_ptr  type_sym = symbol_for(type);
      if (!C_mode() &&
          ((type_sym != NULL && may_have_correspondence(type_sym)) ||
            is_closure)) {
        /* Some types may not have a correspondence yet.  (Note that in C types
           do not have linkage and therefore do not need a correspondence
           unless they participate in the type of an entity with linkage.) */
        clear_type_correspondence(type, /*visited=*/TRUE);
      }  /* if */
    } else if (trans_unit_corresp_of_unknown_entry(tucp->canonical) != tucp) {
      (void)change_c_type_correspondence(type, (a_type_ptr)tucp->canonical);
    }  /* if */
  }  /* for */
}  /* set_correspondence_of_unvisited_entries */


static void find_namespace_correspondence(a_namespace_ptr  nsp)
/*
Look for the given namespace in another translation unit and set the
translation unit correspondence pointer if one is found.
*/
{
  a_symbol_ptr     nsp_sym = (a_symbol_ptr)nsp->source_corresp.assoc_info;
  a_namespace_ptr  unaliased_nsp = skip_namespace_aliases(nsp);

  check_assertion(nsp_sym != NULL);
  if (nsp_sym == symbol_for_namespace_std) {
    /* The standard namespace always exists (in C++), but its symbol in the
       primary translation unit is not linked into the symbol table if no
       member of std has been declared yet.  However, we do know that it is
       the first entry on the list of namespaces for the primary file scope. */
    a_namespace_ptr  primary_std_namespace = translation_units
                                                  ->primary_scope->namespaces;
    check_assertion(primary_std_namespace != NULL &&
                    !is_namespace_member(primary_std_namespace) &&
                    strncmp(primary_std_namespace->source_corresp.name,
                            "std", 3) == 0);
    if (nsp == primary_std_namespace) {
      /* A namespace in the primary translation unit is always canonical. */
      set_no_trans_unit_corresp(iek_namespace, primary_std_namespace);
    } else {
      set_trans_unit_corresp(iek_namespace, nsp, primary_std_namespace);
    }  /* if */
#if IA64_ABI
  } else if (nsp_sym == symbol_for_namespace_abi) {
    /* The __cxxabiv1 namespace is treated identically to the std namespace
       above.  When it exists, it is the second namespace on the list of
       namespaces of the primary scope. */
    a_namespace_ptr  primary_abi_namespace = translation_units
                                            ->primary_scope->namespaces->next;
    check_assertion(primary_abi_namespace != NULL &&
                    !is_namespace_member(primary_abi_namespace) &&
                    strncmp(primary_abi_namespace->source_corresp.name,
                            "__cxxabiv1", 10) == 0);
    if (nsp == primary_abi_namespace) {
      /* A namespace in the primary translation unit is always canonical. */
      set_no_trans_unit_corresp(iek_namespace, primary_abi_namespace);
    } else {
      set_trans_unit_corresp(iek_namespace, nsp, primary_abi_namespace);
    }  /* if */
#endif /* IA64_ABI */
  } else if (is_member_of_unnamed_namespace(&unaliased_nsp->source_corresp)) {
    /* A member of an unnamed namespace does not correspond to a similar
       member in another translation unit.  This also applies to aliases of
       member namespaces. */
    set_no_trans_unit_corresp(iek_namespace, nsp);
  } else {
    a_symbol_ptr            sym = corresp_symbol_list(nsp_sym);
    a_translation_unit_ptr  trans_unit = trans_unit_for_symbol(nsp_sym);
    if (trans_unit_corresp_of(nsp) == NULL) {
      /* Mark this namespace as visited to avoid infinite recursion. */
#if DEBUG
/* GCOVR_EXCL_START */
      if (db_trace("trans_corresp", nsp, iek_namespace)) {
        fprintf(f_debug, "Guard: ");
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      set_no_trans_unit_corresp(iek_namespace, nsp);
    }  /* if */
    for (; sym != NULL; sym = sym->next) {
      if (sym->decl_scope != NO_SCOPE_NUMBER &&
          trans_unit_for_symbol(sym) != trans_unit &&
          same_parents(sym, nsp_sym)) {
        /* Two different declarations in the same namespace and with the same
           name: they should probably match up. */
        if (!may_have_correspondence(sym)) {
          a_source_correspondence_ptr  scp =
                                         source_corresp_entry_for_symbol(sym);
          if (scp != NULL && !in_secondary_trans_unit(scp)) {
            /* The entity corresponding to sym doesn't have linkage, but since
               it appears in the primary translation unit it could cause a
               conflict in generated C code when nsp is copied over. */
            scp->same_name_as_external_entity_in_secondary_trans_unit = TRUE;
          }  /* if */
        } else if (has_correspondence(nsp)) {
          /* We've found the correspondence already; only look for
             conflicts. */
        } else if (is_namespace_symbol(sym) &&
                   sym->variant.namespace_info.ptr->is_namespace_alias ==
                                                    nsp->is_namespace_alias) {
          /* Record the correspondence. */
          set_trans_unit_corresp(iek_namespace,
                                 nsp, sym->variant.namespace_info.ptr);
        } else {
          /* An error since the conflicting entity has external linkage. */
          f_report_bad_trans_unit_corresp((char*)nsp, &sym->decl_position);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* find_namespace_correspondence */


static a_boolean type_conflicts_with_tag(a_symbol_ptr  sym)
/*
sym is a symbol for an entity (not necessarily a type) of the same name and in
the same scope as a tag.  Return TRUE if sym represents a type that conflicts
with the tag of the same name.
*/
{
  a_boolean  result = FALSE;

  check_assertion(!C_mode());
  if (is_tag_symbol(sym)) {
    result = TRUE;
  } else if (sym->kind == (a_symbol_kind)sk_type) {
    /* A typedef symbol conflicts with a homonym tag symbol if it has
       linkage. */
    result = may_have_correspondence(sym);
  }  /* if */
  return result;
}  /* type_conflicts_with_tag */


static void find_type_correspondence(a_type_ptr  type,
                                     a_boolean   parent_found)
/*
Look for the given type in another translation unit and set the translation
unit correspondence pointer if one is found.  When parent_found is TRUE,
this procedure should not attempt to seek correspondences for parent
entities.
*/
{
  a_symbol_ptr  type_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
  a_symbol_ptr  sym;
  a_boolean     handled_later = FALSE;

  if (is_immediate_class_type(type) &&
      (type->variant.class_struct_union.originally_unnamed ||
       type->variant.class_struct_union.is_prototype_instantiation)) {
    /* A class that acquired a name through a typedef declaration will be
       handled elsewhere.   Similarly, prototype instantiations are handled
       when the generic template is processed. */
    handled_later = TRUE;
  } else if (is_immediate_enum_type(type) &&
             type->variant.integer.originally_unnamed) {
    /* An enum type that is originally unnamed but acquired a name through a
       typedef declaration will be handled elsewhere (when the typedef
       correspondence is established). */
    handled_later = TRUE;
  } else if (C_mode()) {
    /* In C, types have no correspondence unless they're involved in the
       declaration of entities with linkage. */
    handled_later = TRUE;
  } else if (!has_name(type)) {
    /* Cannot establish a correspondence without a name. */
  } else if (type_sym != NULL && may_have_correspondence(type_sym)) {
    a_boolean  corresp_found = FALSE;
    a_translation_unit_ptr
               trans_unit = trans_unit_for_symbol(type_sym);
    sym = corresp_symbol_list(type_sym);
    for (; sym != NULL; sym = sym->next) {
      /* Don't consider symbols in the same file. */
      if (sym->decl_scope != NO_SCOPE_NUMBER &&
          trans_unit_for_symbol(sym) != trans_unit &&
          (parent_found ? known_same_parents(sym, type_sym)
                        : same_parents(sym, type_sym))) {
        /* Two different declarations in the same namespace and with the same
           name: they should probably match up. */
        if (!may_have_correspondence(sym)) {
          a_source_correspondence_ptr  scp =
                                         source_corresp_entry_for_symbol(sym);
          if (scp != NULL && !in_secondary_trans_unit(scp)) {
            /* The entity corresponding to sym doesn't have linkage, but since
               it appears in the primary translation unit it could cause a
               conflict in generated C code when nsp is copied over. */
            scp->same_name_as_external_entity_in_secondary_trans_unit = TRUE;
          }  /* if */
        } else if (corresp_found) {
          /* We've found the correspondence already; only look for
             conflicts. */
        } else if (sym->kind == type_sym->kind) {
          /* This test will succeed when one symbol represents a "struct" and
             the other a "class".  Strictly speaking, "class" and "struct" are
             interchangeable only if the entries are not both for definitions,
             but that is not enforced here. */
          a_type_ptr  corresp_type = type_symbol_type(sym);
          set_type_corresp(type, corresp_type);
          corresp_found = TRUE;
        } else if (type_sym->is_class_member) {
          /* A conflict, but errors are reported elsewhere for class
             members. */
        } else if ((is_tag_symbol(type_sym) &&
                    !(type_conflicts_with_tag(sym) ||
                      is_namespace_symbol(sym) ||
                      symbol_is_or_contains_template(sym))) ||
                   (is_tag_symbol(sym) &&
                    !type_conflicts_with_tag(type_sym))) {
          /* Tag names have their own name space. */
        } else {
          f_report_bad_trans_unit_corresp((char*)type, &sym->decl_position);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (!handled_later && trans_unit_corresp_of(type) == NULL) {
    clear_type_correspondence(type, /*visited=*/TRUE);
  }  /* if */
}  /* find_type_correspondence */


static a_symbol_list_entry_ptr find_class_template_instantiation(
                                       a_template_symbol_supplement_ptr  tssp,
                                       a_symbol_ptr                      inst)
/*
Search the list of instantiations attached to the given template symbol
supplement for an instantiation that matches inst.
*/
{
  a_type_ptr  type = type_symbol_type(inst);
  a_symbol_list_entry
              guard;
  a_symbol_list_entry_ptr
              result = NULL, sym_entry, *last_ptr, *guard_ptr;

  if (is_type_symbol(inst)) {
    tssp = primary_template_of(symbol_for(tssp->il_template_entry))
                                                      ->variant.template_info;
  }  /* if */
  /* Special measures must be taken to avoid infinite recursion while
     still correctly handling unusual nested instantiations.  E.g., if this
     is X<X<int> > and the symbol entry is for X<X<X<int> > > the argument
     comparison would have to compare X<int> against X<X<int> > which may
     lead back to here to find if X<X<int> > has a correspondence.  By
     placing a "guard" at the end of the list and moving instantiations
     that are already known not to match class_type behind the guard
     (so they can still be found by recursive invocations of this function
     that look for other instantiations) we achieve the desired effect. */
  last_ptr = &tssp->all_instantiations;
  while (*last_ptr != NULL) { last_ptr = &(*last_ptr)->next; }
  guard.next = NULL;
  guard.symbol = inst;
  *last_ptr = &guard;
  while (tssp->all_instantiations != &guard) {
    a_type_ptr                    corresp_type;
    /* Move the current entry to the end of the all_instantiations list. */
    sym_entry = tssp->all_instantiations;
    while (*last_ptr != NULL) { last_ptr = &(*last_ptr)->next; }
    *last_ptr = sym_entry;
    tssp->all_instantiations = sym_entry->next;
    sym_entry->next = NULL;
    /* Get the type information associated with sym_entry. */
    corresp_type = type_symbol_type(sym_entry->symbol);
    if (corresp_type == type) {
      /* Apparently, we're already trying to find a correspondence for this
         entry.  Sym_entry is the guard entry from another search loop
         (for the same instance). */
      break;
    }  /* if */
    if (tssp->variant.class_template.is_alias_template) {
      /* An alias template: Just check that the template arguments match. */
      an_equiv_templ_arg_options_set  eta_options = ETA_NO_OPTIONS;
      if (tssp->is_variadic) {
        eta_options |= ETA_IS_VARIADIC;
      }  /* if */
      if (equiv_template_arg_lists(
                  type->variant.typeref.extra_info->template_arg_list,
                  corresp_type->variant.typeref.extra_info->template_arg_list,
                  eta_options)) {
        result = sym_entry;
        break;
      }  /* if */
    } else {
      /* A class template. */
      a_class_type_supplement_ptr
               ctsp = class_type_supp(type),
               corresp_ctsp = class_type_supp(corresp_type);
      /* Check that the template arguments and possibly the partial
         specialization arguments are equivalent.  The ETA_IS_NONREAL_MEMBER
         option allows differing length for the argument lists.  Do not confuse
         a prototype instantiation with a similar nonreal instantiation. */
      if (type->variant.class_struct_union.is_nonreal_class ==
                  corresp_type->variant.class_struct_union.is_nonreal_class &&
          type->variant.class_struct_union.is_prototype_instantiation ==
             corresp_type
                    ->variant.class_struct_union.is_prototype_instantiation &&
          equiv_template_arg_lists(ctsp->template_arg_list,
                                   corresp_ctsp->template_arg_list,
                                   ETA_IS_NONREAL_MEMBER)) {
        /* Partial specializations should be generated from the same set of
           partial specialization arguments.  However, those arguments are only
           determined when the body of the class template is instantiated. */
        if ((ctsp->partial_spec_template_arg_list == NULL &&
             corresp_ctsp->partial_spec_template_arg_list == NULL) ||
            class_type_has_body(type) || class_type_has_body(corresp_type) ||
            equiv_template_arg_lists(
                                 ctsp->partial_spec_template_arg_list,
                                 corresp_ctsp->partial_spec_template_arg_list,
                                 ETA_IS_NONREAL_MEMBER)) {
          result = sym_entry;
          break;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* while */
  /* Remove the guard and restore the list to its original state
     (possibly with some added new entries): */
  guard_ptr = &tssp->all_instantiations;
  while (*guard_ptr != &guard) { guard_ptr = &(*guard_ptr)->next; }
  if (guard.next == NULL) {
    /* The guard is already in the last position. */
    *guard_ptr = NULL;
  } else {
    /* Move the segment after the guard to the front of the list. */
    while (*last_ptr != NULL) { last_ptr = &(*last_ptr)->next; }
    *guard_ptr = NULL;
    *last_ptr = tssp->all_instantiations;
    tssp->all_instantiations = guard.next;
  }  /* if */
  return result;
}  /* find_class_template_instantiation */


static void record_class_template_instantiation(a_symbol_ptr  inst)
/*
Search for an instantiation that corresponds to inst in a prior translation
unit.  If there is one, record a correspondence pointer; otherwise, add
the instantiation to the list of instantiations in the associated template
symbol supplement.
*/
{
  a_type_ptr      class_type = type_symbol_type(inst);
  a_symbol_ptr    templ_sym;
  a_template_symbol_supplement_ptr
                  tssp,
                  corresp_tssp;
  a_template_ptr  templ,
                  corresp_templ;

  templ_sym = template_symbol_for_class_symbol(inst);
  templ_sym = primary_template_if_template_symbol(templ_sym);
  tssp = template_supplement_for_symbol(templ_sym);
  templ = tssp->il_template_entry;
  if (correspondence_checking_done &&
      templ->source_corresp.is_class_member &&
      trans_unit_corresp_of(templ) == NULL) {
    /* This is a member template whose correspondence has not been established
       yet, but we are in a phase where normal correspondence checking is
       done.  Presumably, the parent class is still in the process of being
       instantiated and hence establish_class_instantiation_corresp has not
       been called yet to determine the correspondences of its members
       (including that of this member template).  Go ahead and establish the
       correspondence of the template now. */
    find_template_correspondence(templ, /*parent_found=*/FALSE);
  }  /* if */
  corresp_templ = canonical_template_entry_of(templ);
  /* Note that the call to canonical_template_entry_of may have resulted in a
     correspondence value being set already. */
  if (trans_unit_corresp_of(class_type) == NULL) {
    a_symbol_list_entry_ptr
                    sym_entry = NULL;
    corresp_tssp = ((a_symbol_ptr)corresp_templ->source_corresp.assoc_info)
                         ->variant.template_info;
    sym_entry = find_class_template_instantiation(corresp_tssp, inst);
    if (sym_entry == NULL) {
      /* The instantiation was not found on the canonical list.  Add it now. */
      mark_canonical_instantiation(corresp_tssp, inst);
    } else {
      /* Record the necessary correspondences. */
      a_type_ptr  corresp_type = type_symbol_type(sym_entry->symbol);
      set_type_corresp(class_type, corresp_type);
    }  /* if */
  }  /* if */
}  /* record_class_template_instantiation */


static void record_alias_template_instantiation(a_symbol_ptr  inst)
/*
Search for an instantiation that corresponds to inst in a prior translation
unit.  If there is one, record a correspondence pointer; otherwise, add
the instantiation to the list of instantiations in the associated template
symbol supplement.
*/
{
  a_type_ptr      alias_type = type_symbol_type(inst);
  a_typeref_type_supplement_ptr
                  ttsp = alias_type->variant.typeref.extra_info;
  a_symbol_ptr    templ_sym;
  a_template_symbol_supplement_ptr
                  tssp,
                  corresp_tssp;
  a_template_ptr  templ,
                  corresp_templ;

  if (alias_type->variant.typeref.is_dependent &&
      !alias_type->variant.typeref.is_prototype_instantiation) {
    /* Do not record correspondences for dependent alias template instances
       because we cannot do so reliably (multiple IL entries may exist for
       what appear to be equivalent dependent instantiations). */
    set_no_trans_unit_corresp(iek_type, alias_type);
    goto done;
  }  /* if */
  templ_sym = symbol_for(ttsp->assoc_template);
  tssp = template_supplement_for_symbol(templ_sym);
  templ = tssp->il_template_entry;
  if (correspondence_checking_done &&
      templ->source_corresp.is_class_member &&
      trans_unit_corresp_of(templ) == NULL) {
    /* This is a member template whose correspondence has not been established
       yet, but we are in a phase where normal correspondence checking is
       done.  Presumably, the parent class is still in the process of being
       instantiated and hence establish_class_instantiation_corresp has not
       been called yet to determine the correspondences of its members
       (including that of this member template).  Go ahead and establish the
       correspondence of the template now. */
    find_template_correspondence(templ, /*parent_found=*/FALSE);
  }  /* if */
  corresp_templ = canonical_template_entry_of(templ);
  /* Note that the call to canonical_template_entry_of may have resulted in a
     correspondence value being set already. */
  if (trans_unit_corresp_of(alias_type) == NULL) {
    a_symbol_list_entry_ptr
                    sym_entry = NULL;
    corresp_tssp = symbol_for(corresp_templ)->variant.template_info;
    sym_entry = find_class_template_instantiation(corresp_tssp, inst);
    if (sym_entry == NULL) {
      /* The instantiation was not found on the canonical list.  Add it now. */
      mark_canonical_instantiation(corresp_tssp, inst);
    } else {
      /* Record the necessary correspondences. */
      a_type_ptr  corresp_type = type_symbol_type(sym_entry->symbol);
      set_type_corresp(alias_type, corresp_type);
    }  /* if */
  }  /* if */
done:;
}  /* record_alias_template_instantiation */


static a_symbol_list_entry_ptr find_function_template_instantiation(
                                       a_template_symbol_supplement_ptr  tssp,
                                       a_symbol_ptr                      inst)
/*
Search the list of instantiations attached to the given template symbol
supplement for an instantiation that matches inst.
*/
{
  a_symbol_list_entry_ptr  sym_entry = tssp->all_instantiations;
  a_routine_ptr            routine = inst->variant.routine.ptr;
  a_template_arg_ptr       templ_args = routine->template_arg_list;

  for (; sym_entry != NULL; sym_entry = sym_entry->next) {
    a_routine_ptr  corresp_routine = sym_entry->symbol->variant.routine.ptr;
    if (identical_types(routine->type, corresp_routine->type) &&
        /* The ETA_IS_NONREAL_MEMBER option allows comparisons between
           template argument lists that are not known to match the same
           template. */
        equiv_template_arg_lists(corresp_routine->template_arg_list,
                                 templ_args, ETA_IS_NONREAL_MEMBER)) {
      break;
    }  /* if */
  }  /* for */
  return sym_entry;
}  /* find_function_template_instantiation */


static void record_function_template_instantiation(a_symbol_ptr  instance_sym)
/*
Search for an instantiation that corresponds to instance_sym in a prior
translation unit.  If there is one, record a correspondence pointer;
otherwise, add the instantiation to the list of instantiations in the
associated template symbol supplement.
*/
{
  a_template_instance_ptr
                  inst = instance_sym->variant.routine.instance_ptr;
  a_template_symbol_supplement_ptr
                  tssp = inst->template_sym->variant.template_info;
  a_template_ptr  templ = tssp->il_template_entry,
                  corresp_templ =
                            (a_template_ptr)canonical_template_entry_of(templ);
  a_template_symbol_supplement_ptr
                  corresp_tssp =
                       ((a_symbol_ptr)corresp_templ->source_corresp.assoc_info)
                         ->variant.template_info;
  a_routine_ptr   routine = instance_sym->variant.routine.ptr;
  a_symbol_list_entry_ptr
                  sym_entry;

  sym_entry = find_function_template_instantiation(corresp_tssp,
                                                   inst->instance_sym);
  if (sym_entry == NULL) {
    /* The instantiation was not found on the canonical list.  Add it now. */
    mark_canonical_instantiation(corresp_tssp, inst->instance_sym);
  } else if (routine != sym_entry->symbol->variant.routine.ptr) {
    a_routine_ptr  old_ce = (a_routine_ptr)canonical_il_entry_of(
                                      sym_entry->symbol->variant.routine.ptr);
    if (routine != old_ce) {
      set_corresp_for_routines(routine, old_ce);
    }  /* if */
  }  /* if */
}  /* record_function_template_instantiation */


static a_symbol_list_entry_ptr find_variable_template_instantiation(
                                       a_template_symbol_supplement_ptr  tssp,
                                       a_symbol_ptr                      inst)
/*
Search the list of instantiations attached to the given template symbol
supplement for an instantiation that matches inst.
*/
{
  a_symbol_list_entry_ptr  sym_entry = tssp->all_instantiations;
  a_variable_ptr           vp = inst->variant.variable.ptr;
  a_template_arg_ptr       templ_args = vp->template_info->template_arg_list;

  for (; sym_entry != NULL; sym_entry = sym_entry->next) {
    a_variable_ptr  corresp_vp = sym_entry->symbol->variant.variable.ptr;
    if (identical_types(vp->type, corresp_vp->type) &&
        /* The ETA_IS_NONREAL_MEMBER option allows comparisons between
           template argument lists that are not known to match the same
           template. */
        equiv_template_arg_lists(corresp_vp->template_info->template_arg_list,
                                 templ_args, ETA_IS_NONREAL_MEMBER) &&
        /* Check partial specialization arguments. */
        (vp->template_info->partial_spec_template_arg_list == NULL ?
           corresp_vp->template_info->partial_spec_template_arg_list == NULL :
           equiv_template_arg_lists(
                     vp->template_info->partial_spec_template_arg_list,
                     corresp_vp->template_info->partial_spec_template_arg_list,
                     ETA_IS_NONREAL_MEMBER))) {
      break;
    }  /* if */
  }  /* for */
  return sym_entry;
}  /* find_variable_template_instantiation */


static void record_variable_template_instantiation(a_symbol_ptr  instance_sym)
/*
Search for an instantiation that corresponds to instance_sym in a prior
translation unit.  If there is one, record a correspondence pointer;
otherwise, add the instantiation to the list of instantiations in the
associated template symbol supplement.
*/
{
  a_template_instance_ptr
                  inst = instance_sym->variant.variable.instance_ptr;
  a_template_symbol_supplement_ptr
                  tssp = inst->template_sym->variant.template_info;
  a_template_ptr  templ = tssp->il_template_entry,
                  corresp_templ =
                            (a_template_ptr)canonical_template_entry_of(templ);
  a_template_symbol_supplement_ptr
                  corresp_tssp =
                       symbol_for(corresp_templ)->variant.template_info;
  a_variable_ptr  vp = instance_sym->variant.variable.ptr;
  a_symbol_list_entry_ptr
                  sym_entry;

  sym_entry = find_variable_template_instantiation(corresp_tssp,
                                                   inst->instance_sym);
  if (sym_entry == NULL) {
    /* The instantiation was not found on the canonical list.  Add it now. */
    mark_canonical_instantiation(corresp_tssp, inst->instance_sym);
  } else if (vp != sym_entry->symbol->variant.variable.ptr) {
    a_variable_ptr  old_ce = (a_variable_ptr)canonical_il_entry_of(
                                      sym_entry->symbol->variant.variable.ptr);
    if (vp != old_ce) {
      set_trans_unit_corresp(iek_variable, vp, old_ce);
    }  /* if */
  }  /* if */
}  /* record_variable_template_instantiation */



void record_instantiation(a_symbol_ptr                      inst,
                          a_template_symbol_supplement_ptr  tssp)
/*
Check if the given instantiation has a corresponding entry in another
translation unit and record the correspondence if so.  If not, add the
instantiation to the list of all instantiations of the corresponding
template.
*/
{
  if (any_noncorresp_errors()) {
    /* Once errors have been detected correspondence checking is no
       longer done so there's no need to maintain the list of all
       instantiations of an entity. */
    goto done;
  } else if (tssp->il_template_entry == NULL) {
    /* This can happen with "placeholder templates" that are not linked into
       the IL (such as prototype instantiations of friend templates).
       These do not have correspondences.
       It can also happen with guiding declarations declared prior to
       the declaration of the template.  Those will have their
       tssp->il_template_entry set later on. */
    if (is_function_symbol(inst)) {
      if (inst->variant.routine.instance_ptr != NULL &&
          inst->variant.routine.instance_ptr->is_guiding_decl) {
        if (is_primary_translation_unit &&
            !secondary_translation_unit_seen()) {
          mark_canonical_instantiation(tssp, inst);
        } else {
          add_pending_instantiation(inst);
        }  /* if */
      }  /* if */
    }  /* if */
    goto done;
  } else if (is_tag_symbol(inst)) {
    a_type_ptr  class_type = type_symbol_type(inst);
    if (class_type->variant.class_struct_union.is_nonreal_class &&
        !class_type->variant.class_struct_union.is_prototype_instantiation) {
      /* Every nonreal nonprototype instance of a template is considered a
         distinct entry that does not correspond to another entry. */
      set_no_trans_unit_corresp(iek_type, (char*)class_type);
      goto done;
    } else if (inst->is_class_member) {
      a_type_ptr  parent_type = sym_parent_class(inst);
      if (class_type->variant.class_struct_union.is_prototype_instantiation &&
          parent_type->variant.class_struct_union.is_template_class &&
          !parent_type->variant.class_struct_union.is_nonreal_class) {
        /* inst represents a prototype instantiation of a member template of a
           real instantiation.  The a_template entry for such member templates
           does not have a parent class pointer in some configurations, which
           makes determining the correspondence difficult at this point.
           However, their correspondence will be set when the enclosing class
           is processed. */
        goto done;
      }  /* if */
    }  /* if */
  } else if (symbol_is(inst, sk_type)) {
    /* An alias template. */
    a_type_ptr  inst_type = inst->variant.type.ptr;
    if (inst_type->variant.typeref.is_nonreal &&
        !inst_type->variant.typeref.is_prototype_instantiation) {
      /* Every nonreal nonprototype instance of a template is considered a
         distinct entry that does not correspond to another entry. */
      set_no_trans_unit_corresp(iek_type, (char*)inst_type);
      goto done;
    }  /* if */
  }  /* if */
  if (is_primary_translation_unit) {
    a_template_ptr  templ;
    if (!secondary_translation_unit_seen()) {
      /* There is no need to look for a matching instantiation in a secondary
         translation unit. */
      mark_canonical_instantiation(tssp, inst);
      goto done;
    }  /* if */
    templ = tssp->il_template_entry;
    /* Note that templ may not have an associated correspondence yet, and we
       may not be able to establish such a correspondence at this time (e.g.,
       if templ represents a member template whose parent's correspondence
       is in the process of being determined). */
    if (canonical_il_entry_of(templ) != (char*)templ->canonical_template) {
      /* The given tssp is not associated with the canonical template entry. */
      templ = (a_template_ptr)canonical_il_entry_of(templ);
      tssp = template_supplement_for_symbol(
                              (a_symbol_ptr)templ->source_corresp.assoc_info);
    }  /* if */
    if (is_class_struct_union_symbol(inst)) {
      a_type_ptr               prim = type_symbol_type(inst);
      a_symbol_list_entry_ptr
                         slep = find_class_template_instantiation(tssp, inst);
      if (slep == NULL) {
        mark_canonical_instantiation(tssp, inst);
      } else if (slep->symbol != inst) {
        /* This template class was presumably first instantiated in a
           secondary translation unit, but now it is instantiated in the
           primary translation unit.  The new instantiation should become
           the canonical correspondence. */
        a_type_ptr  sec = type_symbol_type(slep->symbol);
        check_assertion(in_secondary_trans_unit(sec));
        set_type_corresp(prim, sec);
        /* It is tempting to set slep->symbol = inst at this point, but we
           may need to have a record of sec to set correspondences for its
           members when establish_class_instantiation_corresp is called. */
      }  /* if */
    } else if (is_function_symbol(inst)) {
      a_symbol_list_entry_ptr
                      slep = find_function_template_instantiation(tssp, inst);
      if (slep == NULL) {
        mark_canonical_instantiation(tssp, inst);
      } else {
        /* This template function was presumably first instantiated in a
           secondary translation unit, but now it is instantiated in the
           primary translation unit.  The new instantiation should become
           the canonical correspondence. */
        a_routine_ptr  prim = inst->variant.routine.ptr,
                       sec = slep->symbol->variant.routine.ptr;
        check_assertion(in_secondary_trans_unit(sec));
        set_corresp_for_routines(sec, prim);
        /* The master instance is found using the canonical entry.  We are
           creating a new canonical entry, so we must make sure its master
           instance pointer is set. */
        set_master_instance_for_new_canonical_routine(prim, sec);
      }  /* if */
    }  /* if */
  } else if (correspondence_checking_done) {
    /* This is an instantiation in a secondary translation unit added
       after correspondence checking has been completed, so do catch-up
       correspondence processing. */
    if (is_class_struct_union_symbol(inst)) {
      record_class_template_instantiation(inst);
    } else if (is_function_symbol(inst)) {
      record_function_template_instantiation(inst);
    } else if (symbol_is(inst, sk_type)) {
      record_alias_template_instantiation(inst);
    } else if (symbol_is(inst, sk_variable)) {
      record_variable_template_instantiation(inst);
    }  /* if */
  }  /* if */
done:
  return;
}  /* record_instantiation */


static void process_pending_instantiations(void)
/*
Find correspondences for the pending list of class instantiations to process.
If any instantiations are generated during that process, also find instances
for those.
*/
{
  while (instantiations_to_process != NULL) {
    a_symbol_list_entry_ptr  entries = instantiations_to_process, entry;
    /* Detach the list of pending instantiations; new instantiations may
       be generated while we process the detached list. */
    instantiations_to_process = NULL;
    for (entry = entries; entry != NULL; entry = entry->next) {
      a_symbol_ptr  inst = entry->symbol;
      if (inst == NULL) {
        /* Processed earlier (presumably by a call to
           process_instantiation_if_pending). */
      } else if (is_class_struct_union_symbol(inst)) {
        record_class_template_instantiation(inst);
      } else if (is_function_symbol(inst)) {
        record_function_template_instantiation(inst);
      } else if (symbol_is(inst, sk_type)) {
        record_alias_template_instantiation(inst);
      } else if (symbol_is(inst, sk_variable)) {
        record_variable_template_instantiation(inst);
      }  /* if */
    }  /* if */
    free_list_of_symbol_list_entries(entries);
  }  /* while */
}  /* process_pending_instantiations */


static void process_instantiation_if_pending(a_symbol_ptr  inst)
/*
If the given instantiation is on the list of instantiations whose
correspondence must be found, process it now.
*/
{
  a_symbol_list_entry_ptr  entry = instantiations_to_process;

  for (; entry != NULL; entry = entry->next) {
    if (inst == entry->symbol) {
      if (is_class_struct_union_symbol(inst)) {
        record_class_template_instantiation(inst);
      } else if (is_function_symbol(inst)) {
        record_function_template_instantiation(inst);
      } else if (symbol_is(inst, sk_type)) {
        record_alias_template_instantiation(inst);
      }  /* if */
      /* Clear this instantiation so it doesn't get re-processed by
         process_pending instantiations. */
      entry->symbol = NULL;
    }  /* if */
  }  /* if */
}  /* process_instantiation_if_pending */


static void establish_instantiation_correspondences(
                                                a_template_ptr  templ,
                                                a_template_ptr  corresp_templ)
/*
Find correspondences for every instantiation of the given template (for
non-prototype instantiations, the actual search is delayed until all
templates are processed).

This routine should only be called for templates that have an associated
sk_class_template, sk_variable_template, or sk_function_template symbol.
Other template entries correspond to class members (e.g., a member function of
a class template) and are handled elsewhere.  corresp_templ is the template
whose prototype instantiation should match that of templ (the canonical entry
of templ may be templ itself and therefore unusable).
*/
{
  a_symbol_ptr
         templ_sym = symbol_for(templ),
         corresp_sym = symbol_for(corresp_templ);
  a_template_symbol_supplement_ptr
         tssp = templ_sym->variant.template_info,
         corresp_tssp = corresp_sym->variant.template_info;

  if (templ != tssp->il_template_entry) {
    /* There can be multiple a_template entries for the same template.  Only
       process the instantiations when encountering the a_template entry that
       is recorded in the template symbol supplement. */
  } else if (symbol_is(templ_sym, sk_class_template)) {
    /* Record the instantiations for later processing to avoid infinite
       recursion. */
    a_symbol_ptr  inst, corresp_inst;
    a_type_ptr    inst_type, corresp_inst_type;
    a_boolean     is_alias = tssp->variant.class_template.is_alias_template;
    a_symbol_list_entry_ptr
                  slep;
    for (slep = tssp->variant.class_template.instantiations;
         slep != NULL; slep = slep->next) {
      inst = slep->symbol;
      inst_type = is_alias ? inst->variant.type.ptr :
                             inst->variant.class_struct_union.type;
      if (!has_correspondence(inst_type)) {
        add_pending_instantiation(inst);
      }  /* if */
    }  /* for */
    for (slep = corresp_tssp->variant.class_template.instantiations;
         slep != NULL; slep = slep->next) {
      inst = slep->symbol;
      inst_type = is_alias ? inst->variant.type.ptr :
                             inst->variant.class_struct_union.type;
      if (!has_correspondence(inst_type)) {
        add_pending_instantiation(inst);
      }  /* if */
    }  /* for */
    /* Also process the prototype instantiations. */
    if (tssp->variant.class_template.prototype_instantiation != NULL) {
      inst = tssp->variant.class_template.prototype_instantiation;
      inst_type = is_alias ? inst->variant.type.ptr :
                             inst->variant.class_struct_union.type;
      corresp_inst = corresp_tssp
                             ->variant.class_template.prototype_instantiation;
      /* For instantiations from template template parameters corresp_inst
         will be NULL.  It will also be NULL for nonprototype templates (the
         prototype instantiation is attached to the corresponding prototype
         template). */
      if (corresp_inst != NULL &&
          corresp_templ->canonical_template != templ->canonical_template) {
        corresp_inst_type = is_alias ?
                                corresp_inst->variant.type.ptr :
                                corresp_inst->variant.class_struct_union.type;
        set_type_corresp(inst_type, corresp_inst_type);
      } else {
        clear_type_correspondence(inst_type, /*visited=*/TRUE);
      }  /* if */
    }  /* if */
  } else if (symbol_is(templ_sym, sk_function_template)) {
    /* Record the instantiations for later processing to avoid infinite
       recursion. */
    a_template_instance_ptr  inst = tssp->variant.function.instantiations;
    for (; inst != NULL; inst = inst->next) {
      if (!has_correspondence(inst->instance_sym->variant.routine.ptr)) {
        add_pending_instantiation(inst->instance_sym);
      }  /* if */
    }  /* for */
    inst = corresp_tssp->variant.function.instantiations;
    for (; inst != NULL; inst = inst->next) {
      if (!has_correspondence(inst->instance_sym->variant.routine.ptr)) {
        add_pending_instantiation(inst->instance_sym);
      }  /* if */
    }  /* for */
    /* Also process prototype instantiations. */
    if (corresp_templ->canonical_template != templ->canonical_template) {
      set_corresp_for_routines(tssp->variant.function.routine,
                               corresp_tssp->variant.function.routine);
    } else {
      /* The prototype instantiation in the translation unit of the canonical
         template entry. */
      if (trans_unit_corresp_of(tssp->variant.function.routine) == NULL) {
        set_no_trans_unit_corresp(iek_routine, tssp->variant.function.routine);
      }  /* if */
    }  /* if */
  } else if (symbol_is(templ_sym, sk_variable_template)) {
    /* Record the instantiations for later processing to avoid infinite
       recursion. */
    a_symbol_list_entry_ptr   slep;
    for (slep = tssp->variant.variable.instantiations;
         slep != NULL; slep = slep->next) {
      a_variable_ptr  vp = slep->symbol->variant.variable.ptr;
      if (!has_correspondence(vp)) {
        add_pending_instantiation(slep->symbol);
      }  /* if */
    }  /* for */
    for (slep = corresp_tssp->variant.variable.instantiations;
         slep != NULL; slep = slep->next) {
      a_variable_ptr  vp = slep->symbol->variant.variable.ptr;
      if (!has_correspondence(vp)) {
        add_pending_instantiation(slep->symbol);
      }  /* if */
    }  /* for */
    /* Also process the prototype instantiations. */
    if (tssp->variant.variable.prototype_variable != NULL) {
      a_variable_ptr  proto, corresp_proto;
      proto = tssp->variant.variable.prototype_variable;
      corresp_proto = corresp_tssp->variant.variable.prototype_variable;
      /* For a nonprototype template this could be NULL (the prototype
         instantiation is attached to the corresponding prototype template). */
      if (corresp_proto != NULL &&
          corresp_templ->canonical_template != templ->canonical_template) {
        set_trans_unit_corresp(iek_variable, proto, corresp_proto);
      } else {
        clear_trans_unit_corresp(iek_variable, proto, /*visited=*/TRUE);
      }  /* if */
    }  /* if */
  } else if (symbol_is(templ_sym, sk_concept_template)) {
    /* Concept templates do not have instantiations.  Nothing more to do. */
  } else {
    unexpected_condition();
  }  /* if */
}  /* establish_instantiation_correspondences */


static a_template_ptr find_corresp_class_template(a_template_ptr  templ,
                                                  a_symbol_ptr    sym)
/*
Find a class (or alias) template from another translation unit corresponding
to the given class (or alias) template templ.  However, only consider sym and
its subordinate symbols when looking up a correspondence: if none is found,
return NULL.
*/
{
  a_template_ptr         corresp_templ = NULL;
  a_symbol_ptr           templ_sym = symbol_for(templ);
  a_template_symbol_supplement_ptr
                         tssp = template_supplement_for_symbol(templ_sym),
                         corresp_tssp = template_supplement_for_symbol(sym);
  a_template_decl_ptr    tdp = templ->template_decl;
  a_requires_clause_ptr  rcp = tdp->constraint.requires_clause;

  /* The symbol "sym" always corresponds to a primary symbol. */
  check_assertion(corresp_tssp->primary_template_sym == NULL);
  if (tssp->variant.class_template.is_alias_template !=
                     corresp_tssp->variant.class_template.is_alias_template) {
    /* Class templates cannot match alias templates. */
  } else if (tssp->primary_template_sym != NULL) {
    /* The given template is a partial specialization: look for a partial
       specialization with the same set of parameters and arguments.
       First, however, we must check that they come from corresponding
       primary templates. */
    a_symbol_ptr    prim_templ_sym = tssp->primary_template_sym;
    a_template_ptr  prim_templ =
                           template_supplement_for_symbol(prim_templ_sym)
                                                          ->il_template_entry;
    a_template_ptr  corresp_prim_templ = corresp_tssp->il_template_entry;
    if (corresponding_templates(prim_templ, corresp_prim_templ)) {
      /* The two partial specializations specialize the same primary
         template. */
      for (sym = corresp_tssp->partial_specializations;
           sym != NULL;
           sym = sym->next) {
        a_template_decl_ptr    corresp_tdp;
        a_requires_clause_ptr  corresp_rcp;
        corresp_tssp = template_supplement_for_symbol(sym);
        corresp_tdp = corresp_tssp->il_template_entry->template_decl;
        corresp_rcp = corresp_tdp->constraint.requires_clause;
        if (equiv_template_param_lists(
                                    corresp_tssp->cache->decl_info->parameters,
                                    tssp->cache->decl_info->parameters,
                                    /*issue_errors=*/FALSE,
                                    ETP_NO_OPTIONS,
                                    &templ_sym->decl_position, es_error) &&
            equiv_requires_clauses(corresp_rcp, rcp)) {
          /* The template parameters correspond; now check the arguments: they
             are attached to the prototype instantiation. */
          a_symbol_ptr  proto, corresp_proto;
          proto = tssp->variant.class_template.prototype_instantiation;
          corresp_proto =
                 corresp_tssp->variant.class_template.prototype_instantiation;
          if (equiv_template_arg_lists(
                  class_type_supp(type_symbol_type(proto))->template_arg_list,
                  class_type_supp(type_symbol_type(corresp_proto))
                                                          ->template_arg_list,
                  ETA_NO_OPTIONS)) {
            corresp_templ = corresp_tssp->il_template_entry;
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  } else {
    /* This is a primary template: the template parameters must match. */
    a_template_decl_ptr    corresp_tdp;
    a_requires_clause_ptr  corresp_rcp;
    corresp_tdp = corresp_tssp->il_template_entry->template_decl;
    corresp_rcp = corresp_tdp->constraint.requires_clause;
    if (equiv_template_param_lists(corresp_tssp->cache->decl_info->parameters,
                                   tssp->cache->decl_info->parameters,
                                   /*issue_errors=*/TRUE,
                                   ETP_NO_OPTIONS,
                                   &templ_sym->decl_position, es_error) &&
        equiv_requires_clauses(corresp_rcp, rcp)) {
      corresp_templ = corresp_tssp->il_template_entry;
    }  /* if */
  }  /* if */
  return corresp_templ;
}  /* find_corresp_class_template */


static a_template_ptr find_corresp_var_template(a_template_ptr  templ,
                                                a_symbol_ptr    sym)
/*
Find a variable template from another translation unit corresponding to the
given variable template templ.  However, only consider sym and its subordinate
symbols when looking up a correspondence: if none is found, return NULL.
*/
{
  a_template_ptr         corresp_templ = NULL;
  a_symbol_ptr           templ_sym = symbol_for(templ);
  a_template_symbol_supplement_ptr
                         tssp = template_supplement_for_symbol(templ_sym),
                         corresp_tssp = template_supplement_for_symbol(sym);
  a_template_decl_ptr    tdp = templ->template_decl;
  a_requires_clause_ptr  rcp = tdp->constraint.requires_clause;

  /* The symbol "sym" always corresponds to a primary symbol. */
  check_assertion(corresp_tssp->primary_template_sym == NULL);
  if (tssp->primary_template_sym != NULL) {
    /* The given template is a partial specialization: look for a partial
       specialization with the same set of parameters and arguments.
       First, however, we must check that they come from corresponding
       primary templates. */
    a_symbol_ptr    prim_templ_sym = tssp->primary_template_sym;
    a_template_ptr  prim_templ =
                           template_supplement_for_symbol(prim_templ_sym)
                                                          ->il_template_entry;
    a_template_ptr  corresp_prim_templ = corresp_tssp->il_template_entry;
    if (corresponding_templates(prim_templ, corresp_prim_templ)) {
      /* The two partial specializations specialize the same primary
         template. */
      for (sym = corresp_tssp->partial_specializations;
           sym != NULL;
           sym = sym->next) {
        a_requires_clause_ptr  corresp_rcp;
        corresp_tssp = template_supplement_for_symbol(sym);
        corresp_rcp = corresp_tssp->il_template_entry->template_decl
                                  ->constraint.requires_clause;
        if (equiv_template_param_lists(
                                    corresp_tssp->cache->decl_info->parameters,
                                    tssp->cache->decl_info->parameters,
                                    /*issue_errors=*/FALSE,
                                    ETP_NO_OPTIONS,
                                    &templ_sym->decl_position, es_error) &&
            equiv_requires_clauses(rcp, corresp_rcp)) {
          /* The template parameters correspond; now check the arguments: they
             are attached to the prototype instantiation. */
          a_variable_ptr  proto, corresp_proto;
          proto = tssp->variant.variable.prototype_variable;
          corresp_proto = corresp_tssp->variant.variable.prototype_variable;
          if (equiv_template_arg_lists(
                  proto->template_info->template_arg_list,
                  corresp_proto->template_info->template_arg_list,
                  ETA_NO_OPTIONS)) {
            corresp_templ = corresp_tssp->il_template_entry;
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  } else {
    /* This is a primary template: the template parameters must match. */
    if (equiv_template_param_lists(corresp_tssp->cache->decl_info->parameters,
                                   tssp->cache->decl_info->parameters,
                                   /*issue_errors=*/TRUE,
                                   ETP_NO_OPTIONS,
                                   &templ_sym->decl_position, es_error)) {
      corresp_templ = corresp_tssp->il_template_entry;
    }  /* if */
  }  /* if */
  return corresp_templ;
}  /* find_corresp_var_template */


static a_template_ptr find_corresp_concept_template(a_template_ptr  templ,
                                                    a_symbol_ptr    sym)
/*
Find a concept template from another translation unit corresponding to the
given concept template templ.  However, only consider sym and its subordinate
symbols when looking up a correspondence: if none is found, return NULL.
*/
{
  a_symbol_ptr    templ_sym = symbol_for(templ);
  a_template_symbol_supplement_ptr
                  tssp = template_supplement_for_symbol(templ_sym),
                  corresp_tssp = template_supplement_for_symbol(sym);
  a_template_ptr  corresp_templ = corresp_tssp->il_template_entry;
  a_requires_clause_ptr
                  rcp = templ->template_decl->constraint.requires_clause,
                  corresp_rcp = corresp_templ->template_decl
                                             ->constraint.requires_clause;

  if (!equiv_template_param_lists(corresp_tssp->cache->decl_info->parameters,
                                  tssp->cache->decl_info->parameters,
                                  /*issue_errors=*/TRUE, ETP_NO_OPTIONS,
                                  &templ_sym->decl_position, es_error) ||
      !equiv_requires_clauses(rcp, corresp_rcp)) {
    corresp_templ = NULL;
  }  /* if */
  return corresp_templ;
}  /* find_corresp_concept_template */


static a_template_ptr find_corresp_function_template(a_template_ptr  templ,
                                                     a_symbol_ptr    sym)
/*
Find a function template from another translation unit corresponding to the
given function template templ.  However, only consider sym and its subordinate
symbols when looking up a correspondence: if none is found, return NULL.
*/
{
  a_template_ptr  corresp_templ = NULL;
  a_boolean       is_list = symbol_is(sym, sk_overloaded_function);
  a_symbol_ptr    sub_sym = is_list ? sym->variant.overloaded_function.symbols
                                    : sym;
  a_symbol_ptr    templ_sym = symbol_for(templ);
  a_template_symbol_supplement_ptr
                  tssp = template_supplement_for_symbol(templ_sym),
                  corresp_tssp;
  a_routine_ptr   routine = tssp->variant.function.routine,
                  corresp_routine;
  a_template_decl_ptr
                  tdp = templ->template_decl, corresp_tdp;
  a_requires_clause_ptr
                  rcp = tdp->constraint.requires_clause, corresp_rcp;

  /* If necessary, iterate over every overloaded function template. */
  for (; sub_sym != NULL; sub_sym = is_list ? sub_sym->next : NULL) {
    if (sub_sym->kind != (a_symbol_kind)sk_function_template) continue;
    corresp_tssp = template_supplement_for_symbol(sub_sym);
    corresp_routine = corresp_tssp->variant.function.routine;
    corresp_tdp = corresp_tssp->il_template_entry->template_decl;
    corresp_rcp = corresp_tdp->constraint.requires_clause;
    if (equiv_template_param_lists(templ_params_of(sub_sym),
                                   templ_params_of(templ_sym),
                                   /*issue_errors=*/FALSE,
                                   ETP_NO_OPTIONS,
                                   &templ_sym->decl_position, es_error) &&
        equiv_requires_clauses(corresp_rcp, rcp) &&
        identical_types_full(routine->type, corresp_routine->type,
                             ITF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED) &&
        equiv_requires_clauses(routine->trailing_requires_clause,
                               corresp_routine->trailing_requires_clause) &&
        equiv_template_arg_lists(routine->template_arg_list,
                                 corresp_routine->template_arg_list,
                                 ETA_NO_OPTIONS)) {
      corresp_templ = corresp_tssp->il_template_entry;
      break;
    }  /* if */
  }  /* for */
  return corresp_templ;
}  /* find_corresp_function_template */


static inline
a_boolean is_possible_template_corresp_sym(a_symbol_ptr sym,
                                           a_symbol_ptr templ_sym,
                                           a_boolean    parent_found)
/*
Return TRUE if the given symbol could possibly have a correspondence with the
given template symbol; otherwise, return FALSE.  When parent_found is TRUE,
this procedure should not attempt to seek correspondences for parent entities.
*/
{
  a_boolean              result = TRUE;
  a_translation_unit_ptr trans_unit = trans_unit_for_symbol(templ_sym);

  if (sym->decl_scope == NO_SCOPE_NUMBER) {
    result = FALSE;
  } else if (trans_unit_for_symbol(sym) == trans_unit) {
    result = FALSE;
  } else if (!may_have_correspondence(sym)) {
    result = FALSE;
  } else if (parent_found ? !known_same_parents(sym, templ_sym)
                          : !same_parents(sym, templ_sym)) {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_possible_template_corresp_sym */


static a_boolean is_template_corresp_conflict(a_symbol_ptr    *matching_sym,
                                              a_template_ptr  templ,
                                              a_boolean       parent_found)
/*
Check for any entity that matches with the given template in another TU.  When
parent_found is TRUE, this procedure should not attempt to seek correspondences
for parent entities.  Set the matching entity (if any) to *matching_sym.
Return TRUE if the matching entity is a conflicting entity that should be
diagnosed; otherwise, the entity is the corresponding entity and FALSE is
returned.
*/
{
  a_boolean     result = FALSE;
  a_symbol_ptr  templ_sym = (a_symbol_ptr)templ->source_corresp.assoc_info;

  if (is_deduction_guide_symbol(templ_sym)) {
    a_routine_ptr  routine = il_entry_for_symbol<a_template>(templ_sym)->
                                               prototype_instantiation.routine;
    a_template_ptr class_templ = routine->variant.class_template;
    a_symbol_ptr   class_templ_sym = symbol_for(class_templ);
    a_template_ptr class_templ_corresp = NULL;

    /* Find the corresponding class template. */
    for (a_symbol_ptr sym = corresp_symbol_list(class_templ_sym); sym != NULL;
         sym = sym->next) {
      if (!is_possible_template_corresp_sym(sym, class_templ_sym,
                                            parent_found)) {
        continue;
      }  /* if */
      if (is_class_template_symbol(sym)) {
         class_templ_corresp = find_corresp_class_template(class_templ, sym);
         if (class_templ_corresp != NULL) {
           break;
         }  /* if */
      }  /* if */
    }  /* for */
    /* If a corresponding class template was found, check for a corresponding
       deduction guide. */
    if (class_templ_corresp != NULL) {
      a_template_symbol_supplement_ptr tssp =
                         template_supplement_for_template(class_templ_corresp);

      for (a_symbol_ptr sym = tssp->variant.class_template.deduction_guides;
           sym != NULL; sym = sym->next) {
        a_template_ptr candidate = find_corresp_function_template(templ, sym);

        if (candidate == NULL) {
          /* Continue searching for a match. */
        } else {
          *matching_sym = symbol_for(candidate);
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  } else {
    a_boolean    class_template = is_class_template_symbol(templ_sym);
    a_boolean    var_template = symbol_is(templ_sym, sk_variable_template);
    a_boolean    concept_template = symbol_is(templ_sym, sk_concept_template);
    a_symbol_ptr sym = corresp_symbol_list(templ_sym);

    for (; sym != NULL; sym = sym->next) {
      if (!is_possible_template_corresp_sym(sym, templ_sym, parent_found)) {
        continue;
      }  /* if */
      /* Two different declarations in the same namespace and with the same
         name: they should probably match up. */
      if ((is_template_symbol(sym) &&
           is_class_template_symbol(sym) == class_template &&
           symbol_is(sym, sk_variable_template) == var_template &&
           symbol_is(sym, sk_concept_template) == concept_template) ||
          (symbol_is(sym, sk_overloaded_function) &&
           !class_template && !var_template)) {
        a_template_ptr  candidate;

        if (class_template) {
          candidate = find_corresp_class_template(templ, sym);
        } else if (var_template) {
          candidate = find_corresp_var_template(templ, sym);
        } else if (concept_template) {
          candidate = find_corresp_concept_template(templ, sym);
        } else {
          candidate = find_corresp_function_template(templ, sym);
        }  /* if */
        if (candidate == NULL) {
          /* Continue searching for a match. */
        } else {
          *matching_sym = symbol_for(candidate);
          break;
        }  /* if */
      } else if (!class_template && !var_template && is_function_symbol(sym)) {
        /* A function template can always be overloaded with a non-template
           function: no conflict. */
      } else {
        /* An error if the conflicting entity has external linkage. */
        result = TRUE;
        *matching_sym = sym;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* is_template_corresp_conflict */


static void find_template_correspondence(a_template_ptr  templ,
                                         a_boolean       parent_found)
/*
Look for the given template in another translation unit and set the translation
unit correspondence pointer if one is found.  When parent_found is TRUE,
this procedure should not attempt to seek correspondences for parent
entities.
*/
{
  a_symbol_ptr  templ_sym = (a_symbol_ptr)templ->source_corresp.assoc_info;

  check_assertion(templ_sym != NULL);
  if (!is_template_symbol(templ_sym) || templ_sym->is_template_param ||
      trans_unit_corresp_of(templ) != NULL) {
    /* Template definitions for nontemplate members of class templates should
       not be processed here.  Nor should template template parameters.
       If a correspondence has already been established, nothing needs to
       be done either. */
  } else if (templ->canonical_template != NULL &&
             templ->canonical_template != templ) {
    /* Templates are somewhat unique in that there can be multiple IL
       entries corresponding to multiple declarations of the same template.
       In those cases, all entries belong to the same correspondence set. */
    a_template_ptr  canon =
                        canonical_template_entry_of(templ->canonical_template);
    set_trans_unit_corresp(iek_template, templ, canon);
  } else if (templ_sym->decl_scope == NO_SCOPE_NUMBER) {
    /* Some prototype instantiations are not associated with any scope and
       as a result cannot have a correspondence set.  This is in particular
       the case with dummy IL entries for friend functions. */
  } else if (!may_have_correspondence(templ_sym)) {
    /* Function templates marked "static" do not correspond to their
       homonyms in other translation units. */
  } else {
    a_symbol_ptr  matching_sym = NULL;
    a_boolean     conflict = is_template_corresp_conflict(&matching_sym,
                                                          templ,
                                                          parent_found);
    if (conflict) {
      f_report_bad_trans_unit_corresp((char*)templ,
                                      &matching_sym->decl_position);
    } else if (matching_sym != NULL) {
      /* The matching symbol should be NULL, a template symbol, or it should've
         been considered a conflict.  If this assertion fails, a non-template
         symbol was returned as a matching (but not conflicting) symbol. */
      check_assertion(is_template_symbol(matching_sym));

      /* Record the correspondence. */
      a_template_ptr corresp_templ =
                                 il_entry_for_symbol<a_template>(matching_sym);
      set_trans_unit_corresp(iek_template, templ, corresp_templ);
      establish_instantiation_correspondences(templ, corresp_templ);
    } else {
      /* Mark this template as visited. */
      set_no_trans_unit_corresp(iek_template, templ);
      /* Mark all instantiations as visited and record them for later lookup.
         */
      clear_instantiation_correspondences(templ, /*visited=*/TRUE);
    }  /* if */
  }  /* if */
}  /* find_template_correspondence */


static a_boolean check_gnu_multiversion_routine_corresponds(
                                      ARG_UNUSED a_routine_ptr routine,
                                      ARG_UNUSED a_routine_ptr corresp_routine)
/*
If routine is a GNU multiversion routine, verify that it corresponds with
corresp_routine.  Return TRUE if so, FALSE otherwise.  Note that a
representative routine is considered to correspond with a routine that has no
multiversioning.
*/
{
  a_boolean result = TRUE;
#if GNU_FUNCTION_MULTIVERSIONING
  a_gnu_routine_supplement_ptr grsp, corresp_grsp;
  a_boolean                    routine_is_rep, corresp_routine_is_rep;
#endif /* GNU_FUNCTION_MULTIVERSIONING */

#if GNU_FUNCTION_MULTIVERSIONING
  grsp = gnu_routine_supp_or_null(routine);
  corresp_grsp = gnu_routine_supp_or_null(corresp_routine);
  routine_is_rep = grsp == NULL || !grsp->is_target_specific_version;
  corresp_routine_is_rep = corresp_grsp == NULL ||
                           !corresp_grsp->is_target_specific_version;
  result = routine_is_rep == corresp_routine_is_rep;
  if (result && !routine_is_rep) {
    an_attribute_ptr ap, corresp_ap;
    /* Both routines are target-specific versions.  Check that the targets
       match.  To do this, we need to dive into the target attributes. */
    ap = find_attribute(ak_target, routine->source_corresp.attributes);
    corresp_ap = find_attribute(ak_target,
                                corresp_routine->source_corresp.attributes);
    check_assertion(ap != NULL && ap->arguments != NULL &&
                    ap->arguments->kind ==
                                         (an_attribute_arg_kind)aak_raw_token);
    check_assertion(corresp_ap != NULL && corresp_ap->arguments != NULL &&
                    corresp_ap->arguments->kind ==
                                         (an_attribute_arg_kind)aak_raw_token);
    if (strcmp(ap->arguments->variant.token,
               corresp_ap->arguments->variant.token) != 0) {
      /* The targets do not match. */
      result = FALSE;
    }  /* if */
  }  /* if */
#endif /* GNU_FUNCTION_MULTIVERSIONING */
  return result;
}  /* check_gnu_multiversion_routine_corresponds */


static a_symbol_ptr check_routine_sym_corresponds(a_symbol_ptr   sym,
                                                  a_routine_ptr  routine)
/*
The given symbol represents an ordinary function, a member function or is
an external routine symbol (sk_extern_routine).  If it corresponds to the
given routine entry, return the sk_routine or sk_member_function associated
with sym.  This is called from find_corresponding_routine_on_list.
*/
{
  a_routine_ptr  corresp_routine;
  a_type_ptr     sym_type;
  a_symbol_ptr   corresp_sym = NULL;

  if (sym->kind == (a_symbol_kind)sk_extern_routine) {
    corresp_routine = sym->variant.extern_symbol_descr->variant.routine.ptr;
  } else {
    corresp_routine = sym->variant.routine.ptr;
  }  /* if */
  sym_type = corresp_routine->type;
  if (routine == corresp_routine) {
    /* Skip this symbol. */
  } else if (corresp_routine->is_template_function) {
    /* An ordinary function never corresponds to a template instance. */
  } else if (!check_gnu_multiversion_routine_corresponds(routine,
                                                         corresp_routine)) {
    /* The routines disagree on their target version, so they cannot
       correspond. */
  } else if (routine->special_kind == (a_special_function_kind)sfk_operator &&
             corresp_routine->special_kind ==
                                      (a_special_function_kind)sfk_operator &&
             routine->variant.opname_kind !=
                                       corresp_routine->variant.opname_kind) {
    /* In some Microsoft modes, the implicitly declared new[] and delete[]
       operators are synonyms for their non-array counterparts.  If these
       operators were user-declared in one translation unit and not in the
       other, we may end up here. */
    check_assertion(
                  microsoft_mode && microsoft_version >= 1400 &&
                  (is_new_operator(routine->variant.opname_kind) ||
                   is_delete_operator(routine->variant.opname_kind)) &&
                  (is_new_operator(corresp_routine->variant.opname_kind) ||
                   is_delete_operator(corresp_routine->variant.opname_kind)));
  } else if (corresp_routine->source_corresp.name_linkage !=
                                        routine->source_corresp.name_linkage &&
             !compatible_enable_if_attributes(
                                       skip_typerefs(routine->type),
                                       skip_typerefs(corresp_routine->type))) {
    /* Routines with different name linkage don't correspond if they have
       distinct enable_if attributes. */
  } else if (param_types_are_compatible(routine->type,
                                        sym_type,
                                        TCF_REDECLARATION |
                                        TCF_SEEK_CORRESP) ||
             /* The function ::main doesn't overload. */
             (is_main_function(routine) &&
              is_main_function(corresp_routine))) {
    corresp_sym = (a_symbol_ptr)corresp_routine->source_corresp.assoc_info;
  } else if (routine->source_corresp.name_linkage ==
                          (a_name_linkage_kind)nlk_external &&
             corresp_routine->source_corresp.name_linkage ==
                          (a_name_linkage_kind)nlk_external &&
             !((routine->compiler_generated ||
                corresp_routine->compiler_generated) &&
               !is_implicitly_declared_c_func(routine) &&
               !is_implicitly_declared_c_func(corresp_routine))) {
    /* Two C-linkage functions with corresponding names but different types are
       usually an error.  An exception occurs with certain built-in functions
       that are "instantiated" for various invocation points (e.g.,
       __c11_atomic_load). */
    f_report_bad_trans_unit_corresp((char*)routine,
                                    &sym->decl_position);
  }  /* if */
  return corresp_sym;
}  /* check_routine_sym_corresponds */


static a_symbol_ptr find_corresponding_routine_on_list(
                                                     a_symbol_ptr  routine_sym,
                                                     a_symbol_ptr  syms)
/*
Look for a routine symbol corresponding to routine_sym (but in another
translation unit) on the list of symbols headed by syms.
*/
{
  a_translation_unit_ptr  trans_unit = trans_unit_for_symbol(routine_sym);
  a_routine_ptr           routine = routine_sym->variant.routine.ptr;
  a_symbol_ptr            sym, corresp_sym = NULL;

  for (sym = syms; sym != NULL; sym = sym->next) {
    /* Don't consider symbols in the same file. */
    if (sym->decl_scope != NO_SCOPE_NUMBER &&
        trans_unit_for_symbol(sym) != trans_unit) {
      /* The matching symbol may be part of an overload set. */
      a_boolean  is_list = (sym->kind ==
                                      (a_symbol_kind)sk_overloaded_function);
      a_symbol_ptr  sub_sym = is_list ?
                              sym->variant.overloaded_function.symbols : sym;
      for (; sub_sym != NULL; sub_sym = is_list ? sub_sym->next : NULL) {
        if (!same_parents(sub_sym, routine_sym)) {
          /* Don't consider symbols in noncorresponding scopes. */  
        } else if (!may_have_correspondence(sub_sym)) {
          a_source_correspondence_ptr  scp =
                                   source_corresp_entry_for_symbol(sub_sym);
          if (scp != NULL && !in_secondary_trans_unit(scp)) {
            /* The entity corresponding to sym doesn't have linkage, but
               since it appears in the primary translation unit it could
               cause a conflict in generated C code when the routine is
               copied over. */
            scp->same_name_as_external_entity_in_secondary_trans_unit = TRUE;
          }  /* if */
        } else if (corresp_sym != NULL) {
          /* We've found the correspondence already; only look for conflicts.
             */
        } else {
          /* Two different declarations in the same namespace or class, and
             with the same name: they should probably match up. */
          switch (sub_sym->kind) {
            case sk_routine:
            case sk_member_function:
            case sk_extern_routine:
              corresp_sym = check_routine_sym_corresponds(sub_sym, routine);
              break;
            case sk_function_template:
            case sk_class_or_struct_tag:
            case sk_union_tag:
            case sk_enum_tag:
              /* No conflict. */
              break;
            case sk_type:
              if (sym->variant.type.is_injected_class_name) break;
              FALLTHROUGH
            default:
              f_report_bad_trans_unit_corresp((char*)routine,
                                              &sub_sym->decl_position);
          }  /* switch */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* for */
  return corresp_sym;
}  /* find_corresponding_routine_on_list */


static void find_routine_correspondence(a_routine_ptr  routine)
/*
Look for the given routine in another translation unit and set the
translation unit correspondence pointer if one is found.
*/
{
  a_symbol_ptr  routine_sym = (a_symbol_ptr)routine->source_corresp.assoc_info,
                corresp_sym = NULL;

  check_assertion(routine_sym != NULL);
  if (may_have_correspondence(routine_sym)) {
    corresp_sym = find_corresponding_routine_on_list(
                                routine_sym, corresp_symbol_list(routine_sym));
    if (corresp_sym == NULL) {
      /* No correspondence was found on the normal list.  Try to look for a
         correspondence among the block extern declarations (some of which
         may not have been declared elsewhere). */
      corresp_sym = find_corresponding_routine_on_list(
                         routine_sym, corresp_extern_symbol_list(routine_sym));
    }  /* if */
  }  /* if */
  if (corresp_sym != NULL) {
    /* Record the correspondence. */
    a_routine_ptr  corresp_routine = corresp_sym->variant.routine.ptr;
    set_corresp_for_routines(routine, corresp_routine);
    if (C_mode()) {
      /* In C mode, type correspondences are only set as the result of
         correspondences between variables and routines.  This occurs as
         a side-effect of calls to f_types_are_compatible with the
         TCF_SEEK_CORRESP flag set. */
      (void)f_types_are_compatible(routine->type, corresp_routine->type,
                                   TCF_SEEK_CORRESP |
                                   TCF_REDECLARATION |
                                   TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING);
    }  /* if */
  } else if (trans_unit_corresp_of(routine) == NULL) {
    /* Mark this routine as visited. */
    set_no_trans_unit_corresp(iek_routine, routine);
  }  /* if */
}  /* find_routine_correspondence */


static a_symbol_ptr find_corresponding_variable_on_list(a_symbol_ptr  var_sym,
                                                        a_symbol_ptr  syms)
/*
Look for a variable symbol corresponding to var_sym (but in another translation
unit) on the list of symbols headed by syms.
*/
{
  a_translation_unit_ptr  trans_unit = trans_unit_for_symbol(var_sym);
  a_variable_ptr          var = var_sym->variant.variable.ptr;
  a_symbol_ptr            sym, corresp_var_sym = NULL;

  for (sym = syms; sym != NULL; sym = sym->next) {
    if (sym->decl_scope == NO_SCOPE_NUMBER ||
        trans_unit_for_symbol(sym) == trans_unit ||
        !same_parents(sym, var_sym)) {
      /* Don't consider symbols in the same file or in noncorresponding
         scopes. */
    } else if (!may_have_correspondence(sym)) {
      a_source_correspondence_ptr  scp = source_corresp_entry_for_symbol(sym);
      if (scp != NULL && !in_secondary_trans_unit(scp)) {
        /* The entity corresponding to sym doesn't have linkage, but since
           it appears in the primary translation unit it could cause a
           conflict in generated C code when var is copied over. */
        scp->same_name_as_external_entity_in_secondary_trans_unit = TRUE;
      }  /* if */
    } else {
      /* Two different declarations in the same namespace or class, and
         with the same name: they should probably match up. */
      switch (sym->kind) {
        case sk_extern_variable:
          if (corresp_var_sym == NULL &&
              var != sym->variant.extern_symbol_descr->variant.variable) {
            corresp_var_sym = (a_symbol_ptr)sym->variant.extern_symbol_descr
                                               ->variant.variable
                                               ->source_corresp.assoc_info;
          }  /* if */
          break;
        case sk_variable:
          {
            if (corresp_var_sym == NULL && var != sym->variant.variable.ptr) {
              corresp_var_sym = sym;
            }  /* if */
          }
          break;
        case sk_class_or_struct_tag:
        case sk_union_tag:
        case sk_enum_tag:
          break;
        case sk_type:
          if (sym->variant.type.is_injected_class_name ||
              is_template_param_type_symbol(sym)) break;
          FALLTHROUGH
        default:
          f_report_bad_trans_unit_corresp((char*)var, &sym->decl_position);
      }  /* switch */
    }  /* if */
  }  /* for */
  return corresp_var_sym;
}  /* find_corresponding_variable_on_list */


static void find_variable_correspondence(a_variable_ptr  var)
/*
Look for the given variable in another translation unit and set the
translation unit correspondence pointer if one is found.
*/
{
  a_symbol_ptr  var_sym = (a_symbol_ptr)var->source_corresp.assoc_info;
  a_symbol_ptr  corresp_var_sym = NULL;

  if (has_name(var) &&
      var_sym != NULL && may_have_correspondence(var_sym)) {
    corresp_var_sym = find_corresponding_variable_on_list(
                                        var_sym, corresp_symbol_list(var_sym));
    if (corresp_var_sym == NULL) {
      /* No correspondence was found on the normal list.  Try to look for a
         correspondence among the block extern declarations (some of which
         may not have been declared elsewhere). */
      corresp_var_sym = find_corresponding_variable_on_list(
                                 var_sym, corresp_extern_symbol_list(var_sym));
    }  /* if */
  }  /* if */
  if (corresp_var_sym != NULL) {
    a_variable_ptr  corresp_var = corresp_var_sym->variant.variable.ptr;
    /* Record the correspondence. */
    set_trans_unit_corresp(iek_variable, var, corresp_var);
    /* If the variable has an anonymous type, assume it matches
       that of the corresponding entity. */
    if (!has_correspondence(var->type) &&
        !has_name(var->type) && !has_name(corresp_var->type) &&
        (is_immediate_class_type(var->type) ||
         is_immediate_enum_type(var->type)) &&
        (is_immediate_class_type(corresp_var->type) ||
         is_immediate_enum_type(corresp_var->type))) {
      set_trans_unit_corresp(iek_type, var->type, corresp_var->type);
      if (var->type->kind != corresp_var->type->kind) {
        /* An error: will be caught later. */
      } else if (is_immediate_class_type(var->type)) {
        establish_trans_unit_correspondences_for_class(var->type);
      } else {
        establish_trans_unit_correspondences_for_enum(var->type);
      }  /* if */
    } else if (C_mode()) {
      /* In C mode, type correspondences are only set as the result of
         correspondences between variables and routines.  This occurs as
         a side-effect of calls to f_types_are_compatible with the
         TCF_SEEK_CORRESP flag set. */
      (void)f_types_are_compatible(var->type, corresp_var->type,
                                   TCF_SEEK_CORRESP |
                                   TCF_REDECLARATION |
                                   TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING);
    }  /* if */
  } else {
    /* Mark this variable as visited. */
    set_no_trans_unit_corresp(iek_variable, var);
  }  /* if */
}  /* find_variable_correspondence */


static a_boolean type_is_top_level_prototype_instantiation(a_type_ptr  type)
/*
Return TRUE if and only if the given type is a prototype instantiation of a
true class template (as opposed to a member type of a class template).
*/
{
  return is_immediate_class_type(type) &&
         has_name(type) &&
         type->variant.class_struct_union.is_prototype_instantiation &&
         type->variant.class_struct_union.extra_info
             ->template_arg_list != NULL;
}  /* type_is_top_level_prototype_instantiation */


static a_type_ptr outer_class_without_correspondence(
                                                 a_source_correspondence  *scp)
/*
The given scp corresponds to a class member (and has no correspondence yet).
Find the outermost enclosing class of that entity that doesn't have a
correspondence either.  If all parents have a correspondence already, return
the direct parent class.
*/
{
  a_type_ptr  root;

  check_assertion(scp->is_class_member);
  root = scp_parent_class(scp);
  while (root->source_corresp.is_class_member &&
         !type_is_top_level_prototype_instantiation(root)) {
    a_type_ptr  next_out = parent_class_of(root);
    if (trans_unit_corresp_of(next_out) == NULL) {
      root = next_out;
    } else {
      break;
    }  /* if */
  }  /* while */
  return root;
}  /* outer_class_without_correspondence */


static void determine_correspondence(a_source_correspondence_ptr  scp,
                                     an_il_entry_kind             kind)
/*
The given source correspondence is part of an IL entry of the given kind.
If it has not been done already and correspondence checking is still under
way, determine to which other IL entry this might correspond.
*/
{
  /* If we're in the process of establishing correspondences, this particular
     entry may need to be processed now.  Sometimes, such processing is also
     needed for members of instantiated classes.  Otherwise, it should already
     have been done or no correspondence can be expected. */
  if ((correspondence_checking_underway ||
       (correspondence_checking_done && scp->is_class_member)) &&
      trans_unit_corresp_of_unknown_entry(scp) == NULL) {
    a_type_ptr  root = NULL;
    /* Class members usually have their correspondence set when their parent
       type is processed.  In those cases we look for the outermost parent
       type without a correspondence. */
    if (scp->is_class_member) {
      if (kind == (an_il_entry_kind)iek_type &&
          type_is_top_level_prototype_instantiation((a_type_ptr)scp)) {
        /* Prototype instantiations are not always recorded in the IL.
           Therefore, set root to NULL so that the symbol table will be used
           to find the named member instead. */
        root = NULL;
      } else {
        /* Make sure the parent class has been processed. */
        determine_correspondence(&scp_parent_class(scp)->source_corresp,
                                 (an_il_entry_kind)iek_type);
        if (kind == (an_il_entry_kind)iek_template) {
          /* For member class templates we can end up with ordering problems
             if we assume that setting the parent correspondences will also
             set the member template's correspondence in time.  Therefore,
             find the member template's correspondence through the symbol
             table. */
          root = NULL;
        } else {
          /* Search for the outermost parent class, but stop at a class type
             that has no correspondence or at one that is a prototype
             instantiation of a true class template. */
          root = outer_class_without_correspondence(scp);
        }  /* if */
      }  /* if */
    }  /* if */
    if (root == NULL) {
      /* Not a class member. */
      switch (kind) {
        case iek_namespace:
          find_namespace_correspondence((a_namespace_ptr)scp);
          break;
        case iek_routine:
          {
            a_routine_ptr  routine = (a_routine_ptr)scp;
            if (routine->is_template_function &&
                routine->template_arg_list != NULL) {
              record_function_template_instantiation(
                                                (a_symbol_ptr)scp->assoc_info);
            } else {
              find_routine_correspondence(routine);
            }  /* if */
          }
          break;
        case iek_variable:
          find_variable_correspondence((a_variable_ptr)scp);
          break;
        case iek_type:
          {
            a_type_ptr  type = (a_type_ptr)scp;
            if ((is_immediate_class_type(type) &&
                 type->variant.class_struct_union.is_template_class &&
                 class_type_supp(type)->template_arg_list != NULL) ||
                (type->kind == (a_type_kind)tk_typeref &&
                 is_typeref_kind(type, trk_is_template_alias))) {
              a_symbol_ptr  inst = symbol_for(type);
              /* Flush the pending instantiations list, in case the type we're
                 interested in is on that list. */
              process_instantiation_if_pending(inst);
              if (trans_unit_corresp_of(type) == NULL) {
                if (type->kind == (a_type_kind)tk_typeref) {
                  record_alias_template_instantiation(inst);
                } else {
                  record_class_template_instantiation(inst);
                }  /* if */
              }  /* if */
            } else {
              find_type_correspondence(type, (a_boolean)scp->is_class_member);
            }  /* if */
            if (trans_unit_corresp_of(type) == NULL) {
              /* Unnamed classes sometimes don't have their correspondence
                 cleared by find_type_correspondence.  Do it here. */
              clear_type_correspondence(type, /*visited=*/TRUE);
            }  /* if */
          }
          break;
        case iek_template:
          find_template_correspondence((a_template_ptr)scp,
                                       (a_boolean)scp->is_class_member);
          break;
        default:
          unexpected_condition_str("Unexpected IL entry kind");
      }  /* switch */
    } else if (trans_unit_corresp_of(root) == NULL) {
      /* A member of a class that was not yet visited. */
      if (root->variant.class_struct_union.is_template_class &&
          class_type_supp(root)->template_arg_list != NULL) {
        record_class_template_instantiation(symbol_for(root));
      } else {
        find_type_correspondence(root, /*parent_found=*/FALSE);
      }  /* if */
    } else {
      /* A member of a class that was already visited.  This could be in a
         member template instantiation. */
      if (kind == (an_il_entry_kind)iek_type) {
        a_type_ptr  type = (a_type_ptr)scp;
        if (is_immediate_class_type(type) &&
            type->variant.class_struct_union.is_template_class &&
            class_type_supp(type)->template_arg_list != NULL) {
          /* A member class template instantiation. */
          record_class_template_instantiation(symbol_for(type));
        } else if (type->kind == (a_type_kind)tk_typeref &&
                   is_typeref_kind(type, trk_is_template_alias)) {
          /* A member alias template instantiation. */
          record_alias_template_instantiation(symbol_for(type));
        } else {
          /* A regular member class of a class template instantiation. */
          find_type_correspondence(type, /*parent_found=*/TRUE);
        }  /* if */
      } else if (kind == (an_il_entry_kind)iek_routine) {
        if (((a_routine_ptr)scp)->is_template_function &&
            ((a_routine_ptr)scp)->template_arg_list != NULL) {
          /* A member function template instantiation. */
          record_function_template_instantiation(
                                                (a_symbol_ptr)scp->assoc_info);
        }  /* if */
      }  /* if */
    }
    if (trans_unit_corresp_of_unknown_entry(scp) == NULL) {
      /* If scp is a member of a class instantiation, we may have to look for
         its parent on the pending instantiations list.  Otherwise, we may
         have to look for scp itself. */
      if (root != NULL) {
        a_type_ptr  parent = outer_class_without_correspondence(scp);
        a_symbol_ptr  parent_sym =
                               (a_symbol_ptr)parent->source_corresp.assoc_info;
        if (parent_sym != NULL) {
          process_instantiation_if_pending(parent_sym);
        }  /* if */
      }  /* if */
      if (scp->assoc_info != NULL) {
        process_instantiation_if_pending((a_symbol_ptr)scp->assoc_info);
      }  /* if */
    }  /* if */
    if (trans_unit_corresp_of_unknown_entry(scp) == NULL) {
      /* A failure to find a correspondence error at an outer level prevents
         this entry from having a correspondence also.  Mark it and its
         unvisited ancestors as having no correspondence. */
      set_no_trans_unit_corresp(kind, scp);
      if (scp->is_class_member) {
        a_type_ptr  parent = scp_parent_class(scp);
        while (parent != root &&
               trans_unit_corresp_of_unknown_entry(parent) == NULL) {
          set_no_trans_unit_corresp(iek_type, parent);
          if (!parent->source_corresp.is_class_member) break;
          parent = parent_class_of(parent);
        }  /* while */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* determine_correspondence */


static a_namespace_ptr canonical_namespace_entry_of(a_namespace_ptr  nsp)
/*
Return the canonical entry established for the given namespace entry.
(Should not be called until the namespaces have already been visited for
correspondences with other translation units.)
*/
{
  a_namespace_ptr  result = nsp;

  if (nsp != NULL && secondary_translation_unit_seen()) {
    /* If we're in the process of establishing correspondences, this particular
       entry may need to be processed now.  Otherwise, it should already have
       been done or no correspondence can be expected. */
    determine_correspondence(&nsp->source_corresp, iek_namespace);
    result = (a_namespace_ptr)canonical_il_entry_of(nsp);
  }  /* if */
  return result;
}  /* canonical_namespace_entry_of */


static a_field_ptr canonical_field_entry_of(a_field_ptr  field)
/*
If the given field entry has not yet been examined for a corresponding entry
in another translation unit, do so now.  Then return the established canonical
entry.
*/
{
  a_field_ptr  result = field;

  if (field != NULL && secondary_translation_unit_seen()) {
    /* If we're in the process of establishing correspondences, this particular
       entry may need to be processed now.  Otherwise, it should already have
       been done or no correspondence can be expected. */
    determine_correspondence(&field->source_corresp, iek_field);
    result = (a_field_ptr)canonical_il_entry_of(field);
  }  /* if */
  return result;
}  /* canonical_field_entry_of */


a_routine_ptr canonical_routine_entry_of(a_routine_ptr  routine)
/*
If the given routine entry has not yet been examined for a corresponding entry
in another translation unit, do so now.  Then return the established canonical
entry.
*/
{
  a_routine_ptr  result = routine;

  if (any_noncorresp_errors()) {
    /* Correspondence checking is inhibited once errors occur. */
  } else if (routine != NULL && secondary_translation_unit_seen()) {
    determine_correspondence(&routine->source_corresp, iek_routine);
    result = (a_routine_ptr)canonical_il_entry_of(routine);
  }  /* if */
  return result;
}  /* canonical_routine_entry_of */


a_variable_ptr canonical_variable_entry_of(a_variable_ptr  var)
/*
If the given variable entry has not yet been examined for a corresponding entry
in another translation unit, do so now.  Then return the established canonical
entry.
*/
{
  a_variable_ptr  result = var;

  if (any_noncorresp_errors()) {
    /* Correspondence checking is inhibited once errors occur. */
  } else if (var != NULL && secondary_translation_unit_seen()) {
    determine_correspondence(&var->source_corresp, iek_variable);
    result = (a_variable_ptr)canonical_il_entry_of(var);
  }  /* if */
  return result;
}  /* canonical_variable_entry_of */


a_type_ptr canonical_type_entry_of(a_type_ptr  type)
/*
If the given type entry has not yet been examined for a corresponding entry
in another translation unit, do so now.  Then return the established
canonical entry.
*/
{
  a_type_ptr  result = type;

  if (any_noncorresp_errors()) {
    /* Correspondence checking is inhibited once errors occur. */
  } else if (type != NULL && secondary_translation_unit_seen() &&
      /* Do not attempt to find a match for a type instantiated from a
         template template parameter. */
      !(is_immediate_class_type(type) &&
        type->variant.class_struct_union.extra_info != NULL &&
        assoc_template_of(type) != NULL &&
        assoc_template_of(type)->kind ==
                           (a_template_kind)templk_template_template_param)) {
    determine_correspondence(&type->source_corresp, iek_type);
    result = (a_type_ptr)canonical_il_entry_of(type);
  }  /* if */
  return result;
}  /* canonical_type_entry_of */


a_template_ptr canonical_template_entry_of(a_template_ptr  templ)
/*
If the given template entry has not yet been examined for a corresponding entry
in another translation unit, do so now.  Then return the established
canonical entry.
*/
{
  a_template_ptr  result = templ;

  if (any_noncorresp_errors()) {
    /* Correspondence checking is inhibited once errors occur. */
  } else if (templ != NULL && secondary_translation_unit_seen()) {
    determine_correspondence(&templ->source_corresp, iek_template);
    result = (a_template_ptr)canonical_il_entry_of(templ);
  }  /* if */
  return result;
}  /* canonical_template_entry_of */


static char* get_canonical_entry_of(char              *entity,
                                    an_il_entry_kind  kind)
/*
Find the canonical entry for the given entity.  If the entity is in a
secondary translation unit, this may entail searching for a corresponding
entry.
*/
{
  char  *result = NULL;

  if (in_secondary_trans_unit(entity)) {
    switch (kind) {
      case iek_namespace:
        result = (char*)canonical_namespace_entry_of((a_namespace_ptr)entity);
        break;
      case iek_field:
        result = (char*)canonical_field_entry_of((a_field_ptr)entity);
        break;
      case iek_routine:
        result = (char*)canonical_routine_entry_of((a_routine_ptr)entity);
        break;
      case iek_variable:
        result = (char*)canonical_variable_entry_of((a_variable_ptr)entity);
        break;
      case iek_type:
        result = (char*)canonical_type_entry_of((a_type_ptr)entity);
        break;
      case iek_template:
        result = (char*)canonical_template_entry_of((a_template_ptr)entity);
        break;
      default:
        unexpected_condition();
    }  /* switch */
  } else {
    result = canonical_il_entry_of(entity);
  }  /* if */
  return result;
}  /* get_canonical_entry_of */


a_boolean corresponding_entries(char              *entity1,
                                char              *entity2,
                                an_il_entry_kind  kind)
/*
Check if both given entities (of the given kind) correspond.  If necessary,
determine the correspondences.
*/
{
  a_boolean  result;
  char       *canon1, *canon2;

  /* Be sure to determine the correspondence of an entity in the secondary
     translation unit first.  Correspondence pointers in the primary
     translation unit are only set as a consequence of an entity in a
     secondary translation unit being processed. */
  if (in_secondary_trans_unit(entity1)) {
    canon1 = get_canonical_entry_of(entity1, kind);
    canon2 = get_canonical_entry_of(entity2, kind);
  } else {
    canon2 = get_canonical_entry_of(entity2, kind);
    canon1 = get_canonical_entry_of(entity1, kind);
  }  /* if */
  result = (canon1 == canon2);
  if (!result && kind == (an_il_entry_kind)iek_type) {
    /* Proxy class types sometimes cannot have their correspondence set,
       but they "correspond" nonetheless. */
    a_type_ptr  type1 = (a_type_ptr)canon1, type2 = (a_type_ptr)canon2;
    if (is_immediate_class_type(type1) && is_immediate_class_type(type2)) {
      result = equiv_class_types(type1, type2,
                                 /*error_matches_anything=*/FALSE,
                                 /*exact_templ_arg_match_required=*/FALSE,
                                 /*contextual_generic_parameters=*/FALSE,
                                 /*exact_decltype_exprs_required=*/FALSE,
                                 /*exact_nesting_depths_required=*/FALSE);
    }  /* if */
  }  /* if */
  return result;
}  /* corresponding_entries */


void establish_block_extern_function_correspondence(a_routine_ptr  routine)
/*
The given routine entry was just created as the result of a block extern
declaration.  This could potentially occur during a template instantiation
in which case this might be the only opportunity to establish its
correspondences in other translation units.
*/
{
  if (any_noncorresp_errors()) {
    /* Correspondence checking is inhibited once errors occur. */
  } else if (correspondence_checking_done &&
             trans_unit_corresp_of(routine) == NULL) {
    find_routine_correspondence(routine);
  }  /* if */
}  /* establish_block_extern_function_correspondence */


void establish_block_extern_variable_correspondence(a_variable_ptr  var)
/*
The given variable entry was just created as the result of a block extern
declaration.  This could potentially occur during a template instantiation
in which case this might be the only opportunity to establish its
correspondences in other translation units.
*/
{
  if (any_noncorresp_errors()) {
    /* Correspondence checking is inhibited once errors occur. */
  } else if (correspondence_checking_done &&
             trans_unit_corresp_of(var) == NULL) {
    find_variable_correspondence(var);
  }  /* if */
}  /* establish_block_extern_variable_correspondence */


void establish_friend_type_correspondence(a_type_ptr  type)
/*
The given type entry was just created as the result of a friend declaration.
This could potentially occur during a template instantiation in which case
this might be the only opportunity to establish its correspondences in other
translation units.
*/
{
  if (any_noncorresp_errors()) {
    /* Correspondence checking is inhibited once errors occur. */
  } else if (correspondence_checking_done &&
             trans_unit_corresp_of(type) == NULL) {
    find_type_correspondence(type, /*parent_found=*/FALSE);
  }  /* if */
}  /* establish_friend_type_correspondence */


static void establish_trans_unit_correspondences_for_scope(a_scope_ptr  scope)
/*
Establish correspondences for all the applicable entities in the given
scope.  The process is repeated in nested class and namespace scopes.
*/
{
  /* Visit all namespaces. */
  {
    a_namespace_ptr  nsp;
    for (nsp = scope->namespaces;
         nsp != NULL;
         nsp = nsp->next) {
      if (has_name(nsp)) {
        find_namespace_correspondence(nsp);
        if (!nsp->is_namespace_alias) {
          /* Establish correspondences for nested namespaces. */
          establish_trans_unit_correspondences_for_scope(
                                                    nsp->variant.assoc_scope);
        }  /* if */
      } else {
        set_no_namespace_correspondence(nsp);
        set_no_trans_unit_corresp(iek_namespace, nsp);
      }  /* if */
    }  /* for */
  }

  /* Visit all types. */
  {
    a_type_ptr  type;
    for (type = skip_generated_type(scope->types);
         type != NULL;
         type = skip_generated_type(type->next)) {
      if (is_immediate_class_type(type) &&
          type->variant.class_struct_union.is_template_class &&
          type->variant.class_struct_union.is_specialized) {
        /* Explicit specializations are handled with the template they
           specialize. */
      } else {
        find_type_correspondence(type, /*parent_found=*/FALSE);
      }  /* if */
    }  /* for */
  }

  /* Visit all routines. */
  {
    a_routine_ptr  routine;
    for (routine = skip_generated_routine(scope->routines);
         routine != NULL;
         routine = skip_generated_routine(routine->next)) {
      find_routine_correspondence(routine);
    }  /* for */
  }

  /* Visit all variables. */
  {
    a_variable_ptr  var;
    for (var = scope->variables; var != NULL; var = var->next) {
      if (!var->is_template_variable) {
        find_variable_correspondence(var);
      }  /* if */
    }  /* for */
  }

  /* Visit all templates. */
  {
    a_template_ptr  templ;
    for (templ = scope->templates; templ != NULL; templ = templ->next) {
      find_template_correspondence(templ,
                                   /*parent_found=*/FALSE);
    }  /* for */
  }

  process_pending_instantiations();
}  /* establish_trans_unit_correspondences_for_scope */


static void verify_trans_unit_correspondences_for_scope(a_scope_ptr  scope)
/*
Verify correspondences for all the applicable entities in the given
scope.  The process is repeated in nested scopes.
*/
{
  verify_namespace_correspondences_for_scope(scope);
  verify_type_correspondences_for_scope(scope);
  verify_routine_correspondences_for_scope(scope);
  verify_variable_correspondences_for_scope(scope);
  verify_template_correspondences_for_scope(scope);
}  /* verify_trans_unit_correspondences_for_scope */


static void process_verification_list(void)
/*
Traverse the list of entries from other translation units that need to be
verified against their canonical entry (which is presumably in the current
translation unit).
*/
{
  while (verification_list != NULL) {
    a_verification_entry_ptr  entries = verification_list, entry;
    /* Detach the verification list; new entries may be added as we process
       the detached list (hence the outer while loop). */
    verification_list = NULL;
    while (entries != NULL) {
      entry = entries;
      entries = entries->next;
      switch (entry->kind) {
        case iek_constant:
          (void)verify_constant_correspondence(
                                              (a_constant_ptr)entry->il_entry);
          break;
        case iek_field:
          (void)verify_field_correspondence((a_field_ptr)entry->il_entry);
          break;
        case iek_namespace:
          (void)verify_namespace_correspondence(
                                             (a_namespace_ptr)entry->il_entry);
          break;
        case iek_routine:
          (void)verify_routine_correspondence((a_routine_ptr)entry->il_entry);
          break;
        case iek_template:
          (void)verify_template_correspondence(
                                              (a_template_ptr)entry->il_entry);
          break;
        case iek_type:
          (void)verify_type_correspondence((a_type_ptr)entry->il_entry);
          break;
        case iek_variable:
          (void)verify_variable_correspondence(
                                              (a_variable_ptr)entry->il_entry);
          break;
        case iek_none:
          /* The entry was removed. */
          break;
        default:
          unexpected_condition();
      }  /* switch */
      free_verification_entry(entry);
    }  /* while */
  }  /* while */
}  /* process_verification_list */


void set_trans_unit_correspondences(void)
/*
Establish correspondences between entities with linkage in the current
translation unit and ones in other translation units.  Actual work is only
done in secondary translation units, and only if no non-correspondence errors
have been encountered.  This routine should still be called for the primary
translation unit to correctly set the correspondence_checking_done flag, since
it may be consulted when additional correspondence checking is done for
later instantiations performed in the primary translation unit.
*/
{
  /* Correspondences are not established if errors occurred. */
  if (!is_primary_translation_unit && !any_noncorresp_errors()) {
    a_scope_ptr  file_scope = curr_translation_unit->primary_scope;
    correspondence_checking_underway = TRUE;
    establish_trans_unit_correspondences_for_scope(file_scope);
    verify_trans_unit_correspondences_for_scope(file_scope);
    process_verification_list();
    correspondence_checking_underway = FALSE;
  }  /* if */
  correspondence_checking_done = TRUE;
}  /* set_trans_unit_correspondences */


static a_boolean is_corresponding_sym_in_trans_unit(
			a_trans_unit_corresp_ptr	corresp_ptr,
			a_symbol_ptr			candidate_sym,
			a_translation_unit_ptr		tup)
/*
Return TRUE if candidate_sym is defined in the translation unit specified
by tup and refers to an IL entry whose correspondence pointer is corresp_ptr.
*/
{
  char			*entry;
  an_il_entry_kind	il_kind;
  a_boolean		result = FALSE;

  entry = il_entry_for_symbol_null_okay(candidate_sym, &il_kind);
  if (entry != NULL) {
    result = trans_unit_corresp_of_unknown_entry(entry) == corresp_ptr &&
             symbol_is_from_trans_unit(candidate_sym, tup);
  }  /* if */
  return result;
}  /* is_corresponding_sym_in_trans_unit */


a_symbol_ptr find_corresponding_class_instance_in_trans_unit(
				a_symbol_ptr		sym_to_find,
				a_translation_unit_ptr	tup)
/*
sym_to_find is a template class instance.  Find the corresponding
instance in the translation unit specified by "tup".  Return the
corresponding instance, or NULL if no corresponding instance is found.
*/
{
  a_symbol_ptr				result_sym = NULL;
  a_symbol_ptr				template_sym;
  a_template_symbol_supplement_ptr	tssp;
  a_symbol_ptr				candidate_sym;
  a_trans_unit_corresp_ptr		corresp_ptr;
  char					*entry;
  an_il_entry_kind			il_kind;

  /* Get the correspondence entry associated with sym_to_find. */
  entry = il_entry_for_symbol(sym_to_find, &il_kind);
  corresp_ptr = trans_unit_corresp_of_unknown_entry(entry);
  if (corresp_ptr == NULL) {
    /* Entities in the primary translation unit may not get a correspondence
       entry.  Some error situations also leave entries without
       correspondence entries.  Create one now. */
    check_assertion(!in_secondary_trans_unit(entry) ||
                    is_at_least_one_error());
    set_no_trans_unit_corresp(il_kind, entry);
    corresp_ptr = trans_unit_corresp_of_unknown_entry(entry);
  }  /* if */
  /* Get the corresponding template in the specified translation unit.
     Note that it is possible that there is no such corresponding template. */
  template_sym = template_symbol_for_class_symbol(sym_to_find);
  template_sym = primary_template_of(template_sym);
  template_sym = find_corresponding_symbol_in_trans_unit(template_sym, tup);
  if (template_sym != NULL) {
    tssp = template_supplement_for_symbol(template_sym);
    candidate_sym = tssp->variant.class_template.prototype_instantiation;
    /* First check whether the prototype instantiation is a match. */
    if (candidate_sym != NULL &&
        is_corresponding_sym_in_trans_unit(corresp_ptr,
                                           candidate_sym, tup)) {
      result_sym = candidate_sym;
    } else {
      /* Check whether a partial specialization prototype instantiation is
         a match. */
      a_symbol_ptr	ps_sym;
      for (ps_sym = tssp->partial_specializations;
           ps_sym != NULL; ps_sym = ps_sym->next) {
        candidate_sym = ps_sym->variant.template_info->
                                variant.class_template.prototype_instantiation;
        if (is_corresponding_sym_in_trans_unit(corresp_ptr,
                                               candidate_sym, tup)) {
          result_sym = candidate_sym;
          break;
        }  /* if */
      }  /* for */
      if (result_sym == NULL) {
        /* We still haven't found a match.  Go through the instantiations
           list. */
        a_symbol_list_entry_ptr	 slep;
        a_symbol_ptr             inst_sym;
        for (slep = tssp->variant.class_template.instantiations;
             slep != NULL; slep = slep->next) {
          inst_sym = slep->symbol;
          if (is_corresponding_sym_in_trans_unit(corresp_ptr,
                                                 inst_sym, tup)) {
            result_sym = inst_sym;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    if (result_sym == NULL) {
      /* No symbol was found.  Instantiate the class in the other translation
         unit. */
      a_template_arg_ptr           templ_arg_list;
      a_type_ptr                   class_type;
      a_class_type_supplement_ptr  ctsp;
      class_type = sym_to_find->variant.class_struct_union.type;
      ctsp = class_type_supp(class_type);
      /* This routine cannot create a new prototype instantiation in the other
         translation unit. */
      check_assertion(!class_type->
                        variant.class_struct_union.is_prototype_instantiation);
      /* Make a copy of the template argument list to be used.  Note that
         templ_arg_list_for_class is not used because, for partial
         specializations, we still want to use the primary template argument
         list. */
      templ_arg_list = copy_template_arg_list(ctsp->template_arg_list);
      result_sym = find_template_class(template_sym, &templ_arg_list,
                                       /*any_prototype_allowed=*/FALSE,
                                       (a_symbol_ptr)NULL,
                                       /*instantiate_nonreal=*/FALSE,
                                       /*do_not_create=*/FALSE,
                                       /*in_substitution=*/FALSE);
    }  /* if */
  }  /* if */
  return result_sym;
}  /* find_corresponding_class_instance_in_trans_unit */


static a_symbol_ptr find_corresponding_symbol_on_symbol_list(
			a_symbol_ptr			sym_to_find,
			a_symbol_ptr			symbols,
			a_boolean			is_routine,
			a_type_ptr			parent_class,
			a_namespace_ptr			parent_namespace,
			a_trans_unit_corresp_ptr	corresp_ptr,
			a_translation_unit_ptr		tup)
/*
Look through the list of symbols specified by "symbols" to find one
from the translation unit "tup" that corresponds to "sym_to_find".
"corresp_ptr" is the correspondence pointer of associated with the
symbol to be found.  is_routine is TRUE if sym_to_find is a routine symbol.
"parent_class" and "parent_namespace" are used to indicate the class or
namespace (if any) of the corresponding symbol in the other translation unit.
*/
{
  a_symbol_ptr		result_sym = NULL;
  a_symbol_ptr		sym;

  /* If we did not decide to use a special symbol list above, use the
     inactive list. */
  if (symbols == NULL) symbols = corresp_symbol_list(sym_to_find);
  for (sym = symbols; sym != NULL; sym = sym->next) {
    a_boolean		is_list;
    a_symbol_ptr	sym_to_check;
    /* Check the kind of the symbol to see if it is a potential match. */
    if (is_routine) {
      if (!is_function_or_template_symbol(sym)) continue;
    } else {
      if (sym->kind != sym_to_find->kind) continue;
    }  /* if */
    /* The kind matches, if there is a parent pointer, make sure the parents
       match. */
    if (parent_class != NULL) {
      /* We are looking for a class member.  Make sure it is a member of the
         right class. */
      if (!sym->is_class_member || sym_parent_class(sym) != parent_class) {
        continue;
      }  /* if */
    } else if (parent_namespace != NULL) {
      /* We are looking for a namespace member.  Make sure it is a member of
         the right namespace. */
      if (sym->is_class_member ||
          sym_parent_namespace_or_null(sym) != parent_namespace) {
        continue;
      }  /* if */
    }  else {
      /* We are looking for a symbol that is not a class or namespace
         member. */
      if (sym_is_class_or_namespace_member(sym)) continue;
    }  /* if */
    /* If the symbol is an overload set we must check each member of the
       set. */
    is_list = (sym->kind == (a_symbol_kind)sk_overloaded_function);
    sym_to_check = is_list ? sym->variant.overloaded_function.symbols : sym;
    for (; sym_to_check != NULL;
         sym_to_check = is_list ? sym_to_check->next : NULL) {
      if (is_corresponding_sym_in_trans_unit(corresp_ptr,
                                             sym_to_check, tup)) {
        result_sym = sym_to_check;
        break;
      }  /* if */
    }  /* for */
    /* Exit the loop if we found a match. */
    if (result_sym != NULL) break;
  }  /* for */
  return result_sym;
}  /* find_corresponding_symbol_on_symbol_list */


/* Forward declaration. */
static a_symbol_ptr find_corresponding_symbol(
				a_symbol_ptr		sym_to_find,
				a_translation_unit_ptr	tup);


static void get_symbol_list_for_corresp_symbol(
					a_symbol_ptr		sym_to_find,
					a_translation_unit_ptr	tup,
					a_boolean		is_routine,
					a_symbol_ptr		parent_sym,
					a_symbol_ptr		*symbols,
					a_symbol_list_entry_ptr	*symbol_list)
/*
Return the symbol list on which to look for "sym_to_find", which is a
member of "parent_class".  "parent_sym" is the symbol for "parent_class".
"is_routine" is TRUE if "sym_to_find" is a function or function template
symbol.

If the symbol list is represented by a list of symbols linked on the next
pointer, the list is returned in "symbols".  If it is represented by list
of a_symbol_list_entries, the list is returned in "symbol_list".

If the inactive list from the symbol header is to be used, this routine
does not set either return value.
*/
{
  a_class_symbol_supplement_ptr	cssp;
  a_special_function_kind	special_kind;

  if (is_routine) {
    special_kind = special_function_kind_for_symbol(sym_to_find);
    cssp = parent_sym->variant.class_struct_union.extra_info;
    /* Constructor and destructor routines are not entered into the symbol
       table, so are found using the appropriate symbols from the class
       symbols supplement. */
    switch (special_kind) {
      case sfk_constructor:
        *symbols = cssp->constructor;
        break;
      case sfk_destructor:
        *symbols = cssp->destructor;
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case sfk_static_constructor:
        *symbols = cssp->static_constructor;
        break;
      case sfk_finalizer:
        *symbols = cssp->finalizer;
        break;
      case sfk_idisposable_dispose:
        *symbols = cssp->has_dispose_pattern_idisposable_dispose
                                           ? cssp->idisposable_dispose : NULL;
        break;
      case sfk_dispose_bool:
        *symbols = (cssp->has_dispose_pattern_idisposable_dispose ||
                    cssp->has_dispose_pattern_object_finalize)
                                                  ? cssp->dispose_bool : NULL;
        break;
      case sfk_object_finalize:
        *symbols = cssp->has_dispose_pattern_object_finalize
                                               ? cssp->object_finalize : NULL;
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case sfk_conversion:
        /* There are two lists of conversion operators.  One for templates
           and one for non-templates. */
        if (sym_to_find->kind == (a_symbol_kind)sk_function_template) {
          *symbol_list = cssp->conversion_template_list;
        } else {
          *symbol_list = cssp->conversion_list;
        }  /* if */
        break;
      default:
        break;
    }  /* switch */
  } else if (sym_to_find->kind == (a_symbol_kind)sk_class_template) {
    /* If this is a class template, check whether it is a partial
       specialization.  If so, return the partial specializations list
       from the corresponding primary template. */
    a_template_symbol_supplement_ptr	tssp;
    a_symbol_ptr			primary_sym;
    tssp = sym_to_find->variant.template_info;
    primary_sym = tssp->primary_template_sym;
    if (primary_sym != NULL) {
      primary_sym = find_corresponding_symbol(primary_sym, tup);
      *symbols = primary_sym->variant.template_info->partial_specializations;
    }  /* if */
  }  /* if */
}  /* get_symbol_list_for_corresp_symbol */


static a_symbol_ptr find_corresponding_symbol(
				a_symbol_ptr		sym_to_find,
				a_translation_unit_ptr	tup)
/*
Find the symbol in the translation unit specified by tup that
corresponds to sym_to_find.  Return a pointer to the symbol found, or
NULL if none is found.
*/
{
  a_symbol_ptr		result_sym = NULL;
  a_boolean		is_routine;
  a_symbol_ptr		parent_sym;
  a_type_ptr		parent_class = NULL;
  a_namespace_ptr	parent_namespace = NULL;
  char			*il_entry;
  a_trans_unit_corresp_ptr
			corresp_ptr;
  an_il_entry_kind	il_kind;
  a_symbol_ptr		symbols = NULL;
  a_symbol_list_entry_ptr
			symbol_list = NULL;

  /* When searching for a routine symbol, we may have to inspect overload
     sets. */
  is_routine = is_function_or_template_symbol(sym_to_find);
  /* If this is a class or namespace member, get the corresponding parent. */
  if (sym_to_find->is_class_member) {
    /* Find the corresponding parent class. */
    parent_sym = symbol_for(sym_parent_class(sym_to_find));
    parent_sym = find_corresponding_symbol_in_trans_unit(parent_sym, tup);
    if (parent_sym != NULL) {
      parent_class = parent_sym->variant.class_struct_union.type;
      complete_class_type_is_needed(parent_class);
      get_symbol_list_for_corresp_symbol(sym_to_find, tup,
                                         is_routine, parent_sym,
                                         &symbols, &symbol_list);
    }  /* if */
  } else if (sym_is_namespace_member(sym_to_find)) {
    /* Find the corresponding parent namespace. */
    parent_sym = symbol_for(sym_parent_namespace(sym_to_find));
    parent_sym = find_corresponding_symbol_in_trans_unit(parent_sym, tup);
    if (parent_sym != NULL) {
      parent_namespace = parent_sym->variant.namespace_info.ptr;
    }  /* if */
  }  /* if */
  /* Get the correspondence entry associated with sym_to_find. */
  il_entry = il_entry_for_symbol(sym_to_find, &il_kind);
  corresp_ptr = trans_unit_corresp_of_unknown_entry(il_entry);
  if (corresp_ptr == NULL) {
    /* Entities in the primary translation unit may not get a correspondence
       entry.  Some error situations also leave entries without
       correspondence entries.  Create one now. */
    check_assertion(!in_secondary_trans_unit(il_entry) ||
                    is_at_least_one_error());
    set_no_trans_unit_corresp(il_kind, il_entry);
    corresp_ptr = trans_unit_corresp_of_unknown_entry(il_entry);
  }  /* if */
  if (symbol_list != NULL) {
    /* Find the symbol on a list of symbol list entries. */
    a_symbol_list_entry_ptr	slep;
    for (slep = symbol_list; slep != NULL; slep = slep->next) {
      a_symbol_ptr	sym_to_check = slep->symbol;
      if (is_corresponding_sym_in_trans_unit(corresp_ptr,
                                             sym_to_check, tup)) {
        result_sym = sym_to_check;
        break;
      }  /* if */
    }  /* for */
  } else {
    /* Find the symbol on a list of symbols linked by the next
       pointer of the symbol. */
    result_sym = find_corresponding_symbol_on_symbol_list(
                      sym_to_find, symbols, is_routine, parent_class,
                      parent_namespace, corresp_ptr, tup);
  }  /* if */
  return result_sym;
}  /* find_corresponding_symbol */


a_symbol_ptr find_corresponding_symbol_in_trans_unit(
					a_symbol_ptr		sym_to_find,
					a_translation_unit_ptr	tup)
/*
Find the symbol associated with the translation unit specified by tup
that refers to the entity that corresponds to sym_to_find.
*/
{
  a_symbol_ptr		result_sym = NULL;

  if (is_template_class_symbol(sym_to_find)) {
    /* For a class instance, we need to look through the instantiations
       list. */
    result_sym = find_corresponding_class_instance_in_trans_unit(
                                            sym_to_find, tup);
  } else {
    /* The normal case -- look for the corresponding symbol on the inactive
       list. */
    result_sym = find_corresponding_symbol(sym_to_find, tup);
  }  /* if */
  return result_sym;
}  /* find_corresponding_symbol_in_trans_unit */


a_boolean ff_same_entities(a_source_correspondence	*ptr1,
			   a_source_correspondence	*ptr2)
/*
Function version of same_entities macro.  Return TRUE if two IL entries
point to the same entity or entities that have the same canonical entry.
*/
{
  return ptr1 == ptr2 ||
         (ptr1 != NULL && ptr2 != NULL && in_front_end &&
          /*lint -e666*/
          same_trans_unit_corresps(trans_unit_corresp_of_unknown_entry(ptr1),
                                   trans_unit_corresp_of_unknown_entry(ptr2)));
}  /* ff_same_entities */


void corresp_one_time_init(void)
/*
Do one-time initialization of variables related to correspondence
checking.
*/
{
  /* Register variables that must be saved and restored when switching
     between translation units. */
  register_trans_unit_variable(correspondence_checking_underway);
  register_trans_unit_variable(correspondence_checking_done);
  register_trans_unit_variable(corresp_errors);
}  /* corresp_one_time_init */


void corresp_trans_unit_init(void)
/* 
Initialize things related to correspondence checking that must be
re-initialized for each translation unit.
*/
{
  correspondence_checking_underway = FALSE;
  correspondence_checking_done = FALSE;
  corresp_errors = 0;
}  /* corresp_trans_unit_init */


void corresp_init(void)
/* 
Initialize things related to correspondence checking that must be initialized
for each compilation.
*/
{
  /* Static variables declared in trans_corresp.c.
     We depend on NULL being represented as zero bits here. */
  memzero((char *)canonical_int_types, sizeof(canonical_int_types));
  memzero((char *)canonical_signed_int_types,
          sizeof(canonical_signed_int_types));
#if MICROSOFT_EXTENSIONS_ALLOWED
  memzero((char *)canonical_microsoft_sized_int_types,
          sizeof(canonical_microsoft_sized_int_types));
  memzero((char *)canonical_microsoft_sized_signed_int_types,
          sizeof(canonical_microsoft_sized_signed_int_types));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if FIXED_POINT_ALLOWED
  memzero((char *)canonical_fixed_point_types,
          sizeof(canonical_fixed_point_types));
#endif /* FIXED_POINT_ALLOWED */
  memzero((char *)canonical_float_types, sizeof(canonical_float_types));
#if C99_IL_EXTENSIONS_SUPPORTED
  memzero((char *)canonical_complex_types, sizeof(canonical_complex_types));
  memzero((char *)canonical_imaginary_types,
          sizeof(canonical_imaginary_types));
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  canonical_il_void_type = NULL;
  canonical_il_wchar_t_type = NULL;
  canonical_il_char8_t_type = NULL;
  canonical_il_char16_t_type = NULL;
  canonical_il_char32_t_type = NULL;
  canonical_il_bool_type = NULL;
  canonical_il_standard_nullptr_type = NULL;
  canonical_il_managed_nullptr_type = NULL;
  verification_list = NULL;
  avail_verification_entries = NULL;
  instantiations_to_process = NULL;
}  /* corresp_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

