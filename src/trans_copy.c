/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

trans_copy.c -- Copy IL from secondary translation units to the
                primary translation unit.

*/

#include "basic_hdrs.h"

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include "trans_copy.h"
#include "trans_corresp.h"
#include "il_walk.h"
#include "scope_stk.h"
#include "templates.h"
#if DO_IL_LOWERING
#include "lower_il.h"
#endif /* DO_IL_LOWERING */

#if !SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
/* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED is needed if IL walking
   is used, so this shouldn't be an extra requirement. */
 #error -- trans_copy.c requires scope orphaned list processing
#endif /* !SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Flag that is TRUE if we are in the setup phase for trans_copy.c.
*/
STATIC_THREAD a_boolean
                in_trans_copy_setup;

/*
Flag that is TRUE if we are in the phase that rewrites primary IL
references to secondary IL addresses.
*/
STATIC_THREAD a_boolean
                in_primary_il_reference_rewrite;


/*
Return TRUE if the given entry has the flag set that indicates that
it needs to be copied.  The il_walk_flag is used for this purpose.
*/
#define entry_needs_copy_flag_is_set(ptr) \
  (il_entry_prefix_of(ptr).il_walk_flag)

/*
Set the flag that indicates that an entry needs to be copied.
*/
#define set_entry_needs_copy_flag(ptr) \
  (il_entry_prefix_of(ptr).il_walk_flag = TRUE)

/*
Reset the flag that indicates that an entry needs to be copied.
*/
#define reset_entry_needs_copy_flag(ptr) \
  (il_entry_prefix_of(ptr).il_walk_flag = FALSE)

/*
Macro interface to f_mark_to_merge, which allows it to be called for
entries of various kinds.
*/
#define mark_to_merge(ptr, kind) f_mark_to_merge((char *)(ptr), (kind))

/*
Return TRUE if the given entry is to be merged with its counterpart
in the primary IL (this tests a flag, which must have been set previously).
See f_mark_to_merge for more information about merging.
*/
#define entry_to_be_merged(ptr) \
  (il_entry_prefix_of(ptr).il_lowering_flag)


static void copy_entry(char             *ptr,
                       an_il_entry_kind kind);
static void copy_string_entry(char             *ptr,
                              an_il_entry_kind kind,
                              sizeof_t         length);
static a_boolean copy_termination_test(char             *ptr,
                                       an_il_entry_kind kind);
static void copy_address_setup(
                             char             *ptr,
                             an_il_entry_kind kind,
                             a_boolean        known_will_process_in_curr_walk);
static
void rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary(void);


static char *f_transitive_copy_address_of(char *ptr)
/*
Return the copy address for the indicated entry.  In the case where the
entry is to be merged, and the copy address points to the intermediate
copy, get the ultimate primary IL copy address from the copy.  The
copy address must be set (i.e., the return value is always non-NULL).

For more information about the copy address, see copy_address_setup.
For more information about the merge process itself, see f_mark_to_merge.
*/
{
  ptr = checked_trans_unit_copy_address_of(ptr);
  check_assertion(ptr != NULL);
  if (in_secondary_trans_unit(ptr)) {
    /* In a merged entry, the copy address points to the space for a
       copy in the secondary translation unit, and that in turn points to
       the address in the primary IL. */
    ptr = checked_trans_unit_copy_address_of(ptr);
    check_assertion(ptr != NULL);
  }  /* if */
  check_assertion(!in_secondary_trans_unit(ptr));
  return ptr;
}  /* f_transitive_copy_address_of */


/*
Macro that provides a convenient interface to f_transitive_copy_address_of.
*/
#define transitive_copy_address_of(ptr) \
  f_transitive_copy_address_of((char *)(ptr))


static char *primary_il_entry_of(char             *ptr,
                                 an_il_entry_kind kind)
/*
Return the address in the primary IL that the entry at "ptr" of kind "kind"
corresponds to.  ptr must either be an address in the primary IL (in
which case it is returned) or must be an address of an entry in a secondary
translation unit, in which case the copy address of the entry will be set
if it is not already set.
*/
{
  if (in_secondary_trans_unit(ptr) && in_file_scope(ptr)) {
    copy_address_setup(ptr, kind, /*known_will_process_in_curr_walk=*/FALSE);
    ptr = transitive_copy_address_of(ptr);
  }  /* if */
  return ptr;
}  /* primary_il_entry_of */


static void copy_address_setup(
                              char             *ptr,
                              an_il_entry_kind kind,
                              a_boolean        known_will_process_in_curr_walk)
/*
Called during the IL walk that copies IL from the secondary translation unit to
the primary translation unit to set up the copy address pointer of the entry
pointed to by ptr, of kind "kind".

The copy address is normally a direct pointer to the primary (canonicalized)
address where the given entry will be copied or remapped to (in the primary
IL).  When merging there is an intermediary address used (see f_mark_to_merge
for more information about the merge).  In this latter case,
transitive_copy_address_of always reaches the former case transitively through
the intermediary address.

If there is no corresponding entry in the primary IL (and this is not a string
entry -- string entries are handled by copy_string_entry itself) this function
will allocate a new entry in the primary IL and use that address as the copy
address.

known_will_process_in_curr_walk is TRUE if it is known that the entry has been
or will be processed (and not merely have its address remapped) in the current
IL walk.  If it is not known that an entry will be processed and
copy_address_setup is invoked as part of copy_from_secondary_to_primary_IL,
copy_address_setup will ensure the entry and its IL subtree are copied as
needed.
*/
{
  if (ptr == NULL) {
    /* Ignore NULL pointers. */
  } else if (!in_file_scope(ptr)) {
    /* This entry is in a function scope memory region, so do nothing. */
  } else if (!in_secondary_trans_unit(ptr)) {
    /* This entry is in the primary file IL, so do nothing. */
    /* Add it as an orphan in case this reference from a secondary translation
       unit is the only one to it. */
    f_possibly_add_orphaned_file_scope_il_entry(ptr, kind, translation_units);
  } else if (trans_unit_copy_address_of(ptr) != NULL) {
    /* A copy address has already been assigned to this entry. */
  } else {
    a_trans_unit_corresp_ptr tucp = NULL;
    /* See whether the entry corresponds to something in another
       translation unit. */
    if (kind == iek_base_class) {
      tucp = ((a_base_class_ptr)ptr)->trans_unit_corresp;
    } else {
      /* See whether the entry has a source correspondence field, which
         may provide correspondence information. */
      a_source_correspondence_ptr scp = source_corresp_for_il_entry(ptr, kind);
      if (scp != NULL) tucp = scp->trans_unit_corresp;
    }  /* if */
    if (tucp != NULL && tucp->canonical != ptr) {
      /* This entry is in a correspondence set but it's not the canonical
         entry.  Set the copy address on the canonical entry and then use
         the same address here. */
      trans_unit_copy_address_of(ptr) =
                                    primary_il_entry_of(tucp->canonical, kind);
    } else if (tucp != NULL && tucp->primary != NULL) {
      /* This entry corresponds to an entry in the primary IL, so
         set the copy address to that entry. */
      trans_unit_copy_address_of(ptr) = tucp->primary;
    } else {
      /* The entry needs to be copied to the primary IL.  Allocate space
         for it in the primary file scope, and set the copy address pointer
         to point to that space.  The entry is copied into that space a
         little later. */
      /* String entries are allocated in copy_string_entry because the
         length is known there. */
      if (!is_string_entry_kind(kind)) {
        char *copy = alloc_primary_file_scope_il(sizeof_il_entry[(int)kind]);
        trans_unit_copy_address_of(ptr) = copy;
        /* Set the flag to request copying. */
        set_entry_needs_copy_flag(ptr);
#if DEBUG
/* GCOVR_EXCL_START */
        if (db_trace("trans_copy", ptr, kind)) {
          fprintf(f_debug, "assigned addr for copy to primary at %p:\n",
                  (a_void_ptr)copy);
          db_entity_info(ptr, kind);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        if (!known_will_process_in_curr_walk) {
          /* We don't know for sure that the entry will be processed in the
             current IL walk.  It might be from another secondary translation
             unit, or it might be an unrecorded orphan.  Do the copy now
             to make sure it gets done. */
          /* Don't do this copy now if we're still in
             prepare_for_trans_unit_copy and not yet in the copy phase. */
          if (!in_trans_copy_setup) {
            walk_il_subtree(copy_entry, copy_string_entry,
                            (a_remap_function_ptr)NULL,
                            (a_remap_function_ptr)NULL,
                            copy_termination_test,
                            /*clear_fe_pointers=*/FALSE,
                            ptr, kind);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* copy_address_setup */


static a_boolean copy_termination_test(char             *ptr,
                                       an_il_entry_kind kind)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit.  Returns TRUE if the walk should be
pruned at the entry pointed to by ptr, of kind "kind".
*/
{
  a_boolean prune;

  /* Make sure the copy address pointer, if any, is set. */
  copy_address_setup(ptr, kind, /*known_will_process_in_curr_walk=*/TRUE);
  if (!in_secondary_trans_unit(ptr)) {
    /* This entry is in the primary file IL, so stop and don't process
       it. */
    prune = TRUE;
  } else if (!in_file_scope(ptr)) {
    /* This entry is in a function scope memory region of a secondary
       translation unit.  Use the secondary_trans_unit flag as
       a "visited" flag; it needs to get cleared anyway.  Using
       il_walk_flag itself is a bad idea because flipping it would
       cause the entries in the function scope memory region to
       have a value different from that in other function scope
       memory regions that weren't moved from secondary translation
       units. */
    il_entry_prefix_of(ptr).secondary_trans_unit = FALSE;
    prune = FALSE;
  } else {
    /* This entry is in the file scope memory region of a secondary translation
       unit. */
    if (!entry_needs_copy_flag_is_set(ptr)) {
      /* This entry does not need any (more) processing. */
      prune = TRUE;
    } else {
      /* This entry still needs to be processed (i.e., copied and remapped). */
      reset_entry_needs_copy_flag(ptr);
      prune = FALSE;
    }  /* if */
  }  /* if */
  return prune;
}  /* copy_termination_test */


static char *remap_secondary_ptr_to_primary_full(
                              char             *ptr,
                              an_il_entry_kind kind,
                              a_boolean        is_list_pointer)

/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit, to remap a pointer to something
in a secondary translation unit ("ptr", of kind "kind") to the
corresponding entry in the primary file IL.  If is_list_pointer is TRUE,
the pointer is a list pointer, i.e., a "next" pointer or a start-of-list
pointer.
*/
{
  char *corresp;

top_of_routine:
  if (ptr == NULL) {
    /* Leave a NULL pointer unchanged. */
    corresp = NULL;
  } else if (!in_secondary_trans_unit(ptr)) {
    /* Leave a primary IL pointer unchanged. */
    corresp = ptr;
  } else if (!in_file_scope(ptr)) {
    /* Leave a function scope pointer unchanged. */
    corresp = ptr;
  } else {
    if (is_list_pointer) {
      /* For a "next" pointer or start-of-list pointer, adjust the value to
         skip any merged entries on the list.  This makes a copied list
         that contains only copied entries, not merged ones. */
      if (entry_to_be_merged(ptr)) {
        switch (kind) {
          case iek_type:
            ptr = (char *)((a_type_ptr)ptr)->next;
            break;
          case iek_variable:
            ptr = (char *)((a_variable_ptr)ptr)->next;
            break;
          case iek_routine:
            ptr = (char *)((a_routine_ptr)ptr)->next;
            break;
          case iek_namespace:
            ptr = (char *)((a_namespace_ptr)ptr)->next;
            break;
          case iek_template:
            ptr = (char *)((a_template_ptr)ptr)->next;
            break;
          case iek_scope:
            ptr = (char *)((a_scope_ptr)ptr)->next;
            break;
          case iek_object_lifetime:
            ptr = (char *)((an_object_lifetime_ptr)ptr)->next;
            break;
          case iek_field:
            /* Merged fields come up when attributes on those fields need
               to be merged. */
            ptr = (char *)((a_field_ptr)ptr)->next;
            break;
          default:
            unexpected_condition_str(
                      "remap_secondary_ptr_to_primary_full: bad merged entry");
        }  /* switch */
        goto top_of_routine;
      }  /* if */
    }  /* if */
    /* If the pointer wasn't encountered previously, make sure its
       copy address pointer is set.  This happens for "next" pointers. */
    /* In the phase that rewrites secondary IL references in the primary IL,
       we can't be sure we didn't enter in the middle of a list, or the
       start of a list where the parent wasn't processed, so turn
       off the list-pointer optimization to force "next" pointers to be
       followed immediately. */
    copy_address_setup(ptr, kind,
                       is_list_pointer && !in_primary_il_reference_rewrite);
    /* Fetch the copy address assigned by copy_address_setup. */
    corresp = transitive_copy_address_of(ptr);
  }  /* if */
  return corresp;
}  /* remap_secondary_ptr_to_primary_full */


static char *remap_secondary_ptr_to_primary(char             *ptr,
                                            an_il_entry_kind kind)
/*
Remap a pointer to something in a secondary translation unit to the
corresponding entry in the primary file IL.
*/
{
  char *corresp = remap_secondary_ptr_to_primary_full(
                                    ptr, kind,
                                    /*is_list_pointer=*/FALSE);
  return corresp;
}  /* remap_secondary_ptr_to_primary */


static char *remap_secondary_list_ptr_to_primary(char             *ptr,
                                                 an_il_entry_kind kind)
/*
Remap a pointer to something in a secondary translation unit to the
corresponding entry in the primary file IL.  This version is called
for list pointers, i.e., "next" pointers and start-of-list pointers.
*/
{
  char *corresp = remap_secondary_ptr_to_primary_full(
                                     ptr, kind,
                                     /*is_list_pointer=*/TRUE);
  return corresp;
}  /* remap_secondary_list_ptr_to_primary */


static void copy_string_entry(char                        *ptr,
                              ARG_UNUSED an_il_entry_kind kind,
                              sizeof_t                    length)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit, to copy the string IL entry at ptr
(of kind "kind", and length "length") to the primary file IL, setting its
copy address pointer to point to the copy.
*/
{
  /* Ignore strings that are already in the primary file IL (such as
     name strings from the symbol header). */
  if (in_secondary_trans_unit(ptr)) {
    if (!in_file_scope(ptr)) {
      /* A string in a function scope memory region just gets reassigned
         to the primary IL. */
      il_entry_prefix_of(ptr).secondary_trans_unit = FALSE;
    } else {
      char *copy = alloc_primary_file_scope_il(length);
      checked_trans_unit_copy_address_of(ptr) = copy;
      (void)memcpy(copy, ptr, size_t_arg(length));
    }  /* if */
  }  /* if */
}  /* copy_string_entry */


static void copy_entry(char             *ptr,
                       an_il_entry_kind kind)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit, to copy the IL entry at ptr
(of kind "kind") to the space indicated by its copy address pointer,
and remap the pointers in the copy.
*/
{
  a_source_correspondence *scp = NULL;
  char                    *copy;

  if (!in_file_scope(ptr)) {
    /* Process an entry in a function scope memory region.  Remap
       the pointers but don't copy. */
    remap_pointers_in_il_entry(ptr, kind,
                               remap_secondary_ptr_to_primary,
                               remap_secondary_list_ptr_to_primary,
                               /*clear_fe_pointers=*/FALSE);
    copy = ptr;
    scp = source_corresp_for_il_entry(copy, kind);
  } else {
    a_trans_unit_corresp_ptr tucp = NULL;
    copy = checked_trans_unit_copy_address_of(ptr);
    check_assertion_str(copy != NULL, "copy_entry: NULL copy address pointer");
    /* Copy the entry to its corresponding space and remap the pointers
       in the copy. */
    (void)memcpy(copy, ptr, size_t_arg(sizeof_il_entry[(int)kind]));
    remap_pointers_in_il_entry(copy, kind,
                               remap_secondary_ptr_to_primary,
                               remap_secondary_list_ptr_to_primary,
                               /*clear_fe_pointers=*/FALSE);
    if (kind == iek_base_class) {
      tucp = ((a_base_class_ptr)ptr)->trans_unit_corresp;
    } else {
      scp = source_corresp_for_il_entry(copy, kind);
      if (scp != NULL) {
        tucp = scp->trans_unit_corresp;
        scp->copied_from_secondary_trans_unit = TRUE;
      }  /* if */
    }  /* if */
    if (tucp != NULL && !in_secondary_trans_unit(copy)) {
      /* This entry is the canonical one, so update the canonical pointer
         to point to the copy in the primary IL.  For the "merge" case,
         the overwrite_primary_xxx routine updates the canonical pointer. */
      check_assertion(tucp->canonical == ptr);
      tucp->canonical = copy;
    }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_trace("trans_copy", ptr, kind)) {
      fprintf(f_debug, "copying from secondary to %p:\n", (a_void_ptr)copy);
      db_entity_info(ptr, kind);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
#if MAINTAIN_NEEDED_FLAGS
  /* Clear the needed and keep_in_il flags in the copy (or original,
     for an entry in a function scope memory region), so that they can be
     recomputed in the context of the primary IL. */
  il_entry_prefix_of(copy).keep_in_il = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
  if (scp != NULL) {
#if MAINTAIN_NEEDED_FLAGS
    reset_needed_flag(scp);
#endif /* MAINTAIN_NEEDED_FLAGS */
    if (kind == iek_type) {
#if MAINTAIN_NEEDED_FLAGS
      a_type_ptr type = (a_type_ptr)copy;
      if (is_immediate_class_type(type)) {
        type->variant.class_struct_union.definition_needed = FALSE;
        type->variant.class_struct_union.keep_definition_in_il = FALSE;
      }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
    } else if (kind == iek_routine) {
      a_routine_ptr rout = (a_routine_ptr)copy;
#if MAINTAIN_NEEDED_FLAGS
      rout->definition_needed = FALSE;
      rout->keep_definition_in_il = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
      rout->on_inline_function_list = FALSE;
    }  /* if */
  } else if (kind == iek_scope) {
    a_scope_ptr scope = (a_scope_ptr)copy;
    scope->scope_orphaned_list_header_generated = FALSE;
#if DO_IL_LOWERING
  } else if (kind == iek_class_type_supplement) {
    a_class_type_supplement_ptr ctsp = (a_class_type_supplement_ptr)copy;
    /* subobject_partner can be non-NULL if prelowering of the class type
       has been done.  If so, clear the pointer on copy. */
    ctsp->subobject_partner = NULL;
    ctsp->has_subobject_type = FALSE;
#endif /* DO_IL_LOWERING */
  }  /* if */
}  /* copy_entry */


static void copy_from_secondary_to_primary_IL(void)
/*
Copy everything from the current secondary translation unit IL to the
primary translation unit IL.
*/
{
  db_enter(1, "copy_from_secondary_to_primary_il");
  walk_file_scope_il(copy_entry,
                     copy_string_entry,
                     (a_remap_function_ptr)NULL,
                     (a_remap_function_ptr)NULL,
                     copy_termination_test,
                     /*clear_fe_pointers=*/FALSE);
  db_exit();
}  /* copy_from_secondary_to_primary_IL */


/*
Fix the indicated "last" pointer in a pointers block for a namespace
by remapping it to the corresponding primary IL address.  In the
case where the last entry was not copied, recompute the last pointer
by running through the list from the start.
*/
#define fix_last_pointer(last_ptr, first_ptr, ptr_type, kind) \
{ if ((last_ptr) != NULL) { \
    if (!entry_to_be_merged(last_ptr)) { \
      (last_ptr) = (ptr_type)primary_il_entry_of((char *)(last_ptr), (kind)); \
    } else { \
      ptr_type ptr = (first_ptr); \
      if (ptr != NULL) { \
        while (ptr->next != NULL) ptr = ptr->next; \
      }  /* if */ \
      (last_ptr) = ptr; \
    }  /* if */ \
  }  /* if */ \
}  /* fix_last_pointer */


static void update_namespace_pointers_block(a_scope_ptr scope)
/*
scope is the secondary translation unit scope for a namespace that has been
copied to the primary IL rather than merged.  Update its pointers block (which
should be shared with the primary IL copy) so that its last-pointers point
to the copied entries in the primary IL.  The pointers block is a
front-end-only data structure, not one in the IL.
*/
{
  a_scope_pointers_block *pointers_block = get_pointers_block_for_scope(scope);

  check_assertion(pointers_block != NULL);
  scope = (a_scope_ptr)primary_il_entry_of((char *)scope, iek_scope);
  check_assertion(pointers_block == get_pointers_block_for_scope(scope));
  fix_last_pointer(pointers_block->last_constant, scope->constants,
                   a_constant_ptr, iek_constant);
  fix_last_pointer(pointers_block->last_type, scope->types,
                   a_type_ptr, iek_type);
  fix_last_pointer(pointers_block->last_variable, scope->variables,
                   a_variable_ptr, iek_variable);
  fix_last_pointer(pointers_block->last_routine, scope->routines,
                   a_routine_ptr, iek_routine);
  fix_last_pointer(pointers_block->last_asm_entry, scope->asm_entries,
                   an_asm_entry_ptr, iek_asm_entry);
  fix_last_pointer(pointers_block->last_dynamic_init, scope->dynamic_inits,
                   a_dynamic_init_ptr, iek_dynamic_init);
  fix_last_pointer(pointers_block->last_namespace, scope->namespaces,
                   a_namespace_ptr, iek_namespace);
  fix_last_pointer(pointers_block->last_using_directive,
                   scope->using_directives,
                   a_using_decl_ptr, iek_using_decl);
  fix_last_pointer(pointers_block->last_using_declaration,
                   scope->using_declarations,
                   a_using_decl_ptr, iek_using_decl);
  fix_last_pointer(pointers_block->last_pragma, scope->pragmas,
                   a_pragma_ptr, iek_pragma);
  fix_last_pointer(pointers_block->last_template, scope->templates,
                   a_template_ptr, iek_template);
#if MICROSOFT_EXTENSIONS_ALLOWED
  fix_last_pointer(pointers_block->last_ms_attribute, scope->ms_attributes,
                   an_ms_attribute_ptr, iek_ms_attribute);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* update_namespace_pointers_block */


static void update_namespace_last_pointers(a_scope_ptr scope)
/*
scope is a file scope or namespace scope for a secondary translation unit
that has just been processed in a copy to the primary IL.  For each namespace
at or under the indicated scope, update the "last" pointers in its pointers
block now that all the copies have been done.  
*/
{
  if (scope->kind == (a_scope_kind)sck_namespace) {
    if (!entry_to_be_merged(scope)) {
      update_namespace_pointers_block(scope);
    }  /* if */
  }  /* if */
  { a_namespace_ptr nsp;
    for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
      if (!nsp->is_namespace_alias) {
        update_namespace_last_pointers(nsp->variant.assoc_scope);
      }  /* if */
    }  /* for */
  }
}  /* update_namespace_last_pointers */


static void move_routine_body_to_primary(a_routine_ptr routine)
/*
Move the body of the indicated routine to the primary IL by walking
it and remapping pointers.
*/
{
  a_scope_ptr scope;

  check_assertion(in_secondary_trans_unit(routine));
  scope = scope_for_routine(routine);
  /* Note that the "copy" routines called here simply remap pointers
     when given an entry in a function scope memory region. */
  walk_routine_scope_il(mem_region_for_routine(routine),
                        copy_entry,
                        copy_string_entry,
                        (a_remap_function_ptr)NULL,
                        (a_remap_function_ptr)NULL,
                        copy_termination_test,
                        /*clear_fe_pointers=*/FALSE);
  /* Reset function_body_processing_finished in all the function bodies in this
     memory region to ensure that they will eventually be lowered.  (See also
     finish_processing_for_function_bodies.) */
  do {
    scope->function_body_processing_finished = FALSE;
    scope = scope->next;
  } while (scope != NULL);
}  /* move_routine_body_to_primary */


static void copy_function_bodies_from_secondary_to_primary_IL(
                                                            a_scope_ptr scope);


static void copy_type_list_function_bodies_from_secondary_to_primary_IL(
                                                          a_type_ptr type_list)
/*
Copy the bodies of any functions that are members of types on the indicated
list to the primary translation unit IL.
*/
{
  a_type_ptr type;

  for (type = type_list; type != NULL; type = type->next) {
    if (is_immediate_class_type(type)) {
      a_scope_ptr class_scope =
                      type->variant.class_struct_union.extra_info->assoc_scope;
      if (!scope_is_null_or_placeholder(class_scope)) {
        copy_function_bodies_from_secondary_to_primary_IL(class_scope);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* copy_type_list_function_bodies_from_secondary_to_primary_IL */


static void copy_function_bodies_from_secondary_to_primary_IL(
                                                             a_scope_ptr scope)
/*
Copy the bodies of any functions in the indicated scope (a file,
namespace, class, function, or block scope in a secondary translation unit)
to the primary translation unit IL.
*/
{
  a_routine_ptr                    routine;
  a_namespace_ptr                  nsp;
  a_scope_ptr                      sub_scope;
  a_scope_orphaned_list_header_ptr solhp;

  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    if (routine->function_def_number != NULL_function_def_number) {
      /* Move the routine body to the primary IL. */
      /* Local types are handled by visiting the orphan lists later. */
      move_routine_body_to_primary(routine);
    }  /* if */
  }  /* for */
  if (!C_mode()) {
    copy_type_list_function_bodies_from_secondary_to_primary_IL(scope->types);
  }  /* if */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      copy_function_bodies_from_secondary_to_primary_IL(
                                                     nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  for (sub_scope = scope->scopes;
       sub_scope != NULL;
       sub_scope = sub_scope->next) {
    copy_function_bodies_from_secondary_to_primary_IL(sub_scope);
  }  /* for */
  if (!C_mode() && scope->kind == (a_scope_kind)sck_file) {
    /* Visit orphan lists to get member functions of local types. */
    /* Note that using the orphan lists is better than going from the
       scopes of functions as they are hit, because when unneeded entities
       are not removed there are cases where a function's body is deleted
       because it need not be copied and yet a member function of a local
       class of that removed function survives in the IL. */
    for (solhp = il_header.scope_orphaned_list_headers;
         solhp != NULL;
         solhp = solhp->next) {
      copy_type_list_function_bodies_from_secondary_to_primary_IL(
                                                        solhp->orphaned_types);
    }  /* for */
  }  /* if */
}  /* copy_function_bodies_from_secondary_to_primary_IL */


static void establish_as_canonical(a_source_correspondence *scp)
/*
If the entity with the indicated source correspondence has a trans-unit
correspondence, make it the canonical entry of the correspondence set.
This is used when an entry is copied to the primary IL or overwrites
the primary IL entry, to establish the copy as the canonical entry.
*/
{
  a_trans_unit_corresp_ptr tucp = scp->trans_unit_corresp;

  if (tucp != NULL) {
    tucp->canonical = (char *)scp;
  }  /* if */
}  /* establish_as_canonical */


void switch_canonical_for_deleted_definition(a_source_correspondence *scp)
/*
The definition of the entity with the indicated source correspondence has
been deleted.  If the entity is the canonical entry of a correspondence
set, and there is a primary IL entry that's now just as good, switch the
canonical entry to the primary IL entry.  Note that the entity passed
in must not be a specialization (in that case, the secondary IL copy
remains better than the primary IL copy).
*/
{
  a_trans_unit_corresp_ptr tucp = scp->trans_unit_corresp;

  if (tucp != NULL && tucp->canonical == (char *)scp) {
    /* This entity is the canonical entry. */
    if (tucp->primary != NULL) {
      tucp->canonical = tucp->primary;
    }  /* if */
  }  /* if */
}  /* switch_canonical_for_deleted_definition */


static void clear_body_for_routine(a_routine_ptr routine)
/*
Eliminate the body of the indicated routine.
*/
{
  a_scope_ptr routine_scope = scope_for_routine(routine);
  clear_function_body(routine_scope);
  if (!routine->is_specialized) {
    switch_canonical_for_deleted_definition(&routine->source_corresp);
  }  /* if */
}  /* clear_body_for_routine */


/*
Return TRUE if an entry should be copied (and not merged) to the primary IL.
The entry must be of a kind that has a source correspondence field.
The entry is copied if either (a) it has no correspondence, e.g., because
it has no linkage, or (b) it has a correspondence and it is the canonical
entry of that correspondence set (i.e., it's the one that should be copied),
and there is no primary IL entry that the entry must overwrite.
*/
#define entry_should_be_copied(ptr) \
  (trans_unit_corresp_of(ptr) == NULL || \
   (trans_unit_corresp_of(ptr)->canonical == (char *)(ptr) && \
    trans_unit_corresp_of(ptr)->primary == NULL))


/*
Return TRUE if an entry should overwrite a corresponding IL entry in
the primary IL.  Overwriting is one kind of "merge".
*/
#define entry_should_overwrite_primary_entry(ptr) \
  (trans_unit_corresp_of(ptr) != NULL && \
   trans_unit_corresp_of(ptr)->canonical == (char *)(ptr) && \
   trans_unit_corresp_of(ptr)->primary != NULL)


/*
Return TRUE if the indicated entry should be kept in the IL, either
so that it can be copied or so that it can be merged.  Note that for
classes and routines there may be other reasons for keeping the entity.
*/
#define entry_should_be_kept(ptr) \
  (entry_should_be_copied(ptr) || entry_should_overwrite_primary_entry(ptr))


static void process_variable_if_unneeded_non_template(a_variable_ptr variable)
/*
If we're processing the current secondary translation unit only to
get exported templates, and the given variable is not a generated template,
do any necessary processing, e.g., externalizing it if it is static.
*/
{
  if (translation_unit_needed_only_for_exported_templates) {
    if (!variable->is_template_variable ||
        variable->is_specialized) {
      /* The variable is not a generated template. */
      clear_variable_definition(variable);
#if DO_IL_LOWERING
      if (il_lowering_needed() &&
          variable->storage_class == (a_storage_class)sc_static) {
        /* A static variable referenced from a template is changed to an
           external declaration and copied over. */
        externalize_source_correspondence(&variable->source_corresp,
                                          /*is_variable=*/TRUE);
        variable->storage_class = (a_storage_class)sc_extern;
      }  /* if */
#endif /* DO_IL_LOWERING */
    }  /* if */
  }  /* if */
}  /* process_variable_if_unneeded_non_template */


static void process_routine_if_unneeded_non_template(a_routine_ptr routine)
/*
If we're processing the current secondary translation unit only to
get exported templates, and the given routine is not a generated template,
do any necessary processing, e.g., externalizing it if it is static.
*/
{
  if (is_nontemplate_routine_from_exported_trans_unit(routine)) {
    /* The definition should have been eliminated at pop_scope
       time (the definition will be put out when the file is compiled
       as a primary file) unless the routine is inline. */
    check_assertion(routine->function_def_number == NULL_function_def_number ||
                    routine->is_inline);
#if DO_IL_LOWERING
    if (il_lowering_needed() &&
        routine->storage_class == (a_storage_class)sc_static) {
      /* A static function referenced from a template is changed to an
         external declaration and copied over. */
      externalize_source_correspondence(&routine->source_corresp,
                                        /*is_variable=*/FALSE);
      if (routine->function_def_number == NULL_function_def_number) {
        routine->storage_class = (a_storage_class)sc_extern;
      } else {
        /* A static inline function becomes extern inline. */
        check_assertion(routine->is_inline);
        routine->storage_class = (a_storage_class)sc_unspecified;
      }  /* if */
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
}  /* process_routine_if_unneeded_non_template */

#if CHECKING

static int corresp_ranking(char             *ptr,
                           an_il_entry_kind kind)
/*
Return a correspondence ranking for the indicated entry, of kind "kind".
The entry with the highest value should be the canonical entry.
*/
{
  int rank = 0;

  switch (kind) {
    case iek_type:
      { a_type_ptr type = (a_type_ptr)ptr;
        if (is_immediate_class_type(type)) {
          rank = class_type_has_body(type);
          if (type->variant.class_struct_union.is_specialized) rank += 2;
        } else if (is_immediate_enum_type(type)) {
          rank = !is_incomplete_type(type);
        } else {
          rank = 0;
        }  /* if */
      }
      break;
    case iek_variable:
      { a_variable_ptr var = (a_variable_ptr)ptr;
        rank = (var->storage_class == (a_storage_class)sc_unspecified);
        if (var->is_specialized) rank += 2;
      }
      break;
    case iek_routine:
      { a_routine_ptr rout = (a_routine_ptr)ptr;
        rank = (rout->function_def_number != NULL_function_def_number);
        if (rout->is_specialized) rank += 2;
      }
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return rank;
}  /* corresp_ranking */


static void check_correspondences(a_source_correspondence *scp,
                                  an_il_entry_kind        kind)
/*
Check that the correspondences, if any, established for the entity whose
source correspondence field is scp and whose kind is "kind" are consistent.
*/
{
  a_trans_unit_corresp_ptr tucp = scp->trans_unit_corresp;

  if (tucp == NULL) {
    /* An entry without a correspondence should not have external linkage. */
    if (scp->name != NULL &&
        (scp->name_linkage == (a_name_linkage_kind)nlk_external ||
         scp->name_linkage == (a_name_linkage_kind)nlk_cplusplus_external)) {
#if DEBUG
/* GCOVR_EXCL_START */
      db_entity_info((char *)scp, kind);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      unexpected_condition_str(
                    "entity with external linkage does not have corresp info");
    }  /* if */
  } else {
    if (tucp->canonical == tucp->primary) {
      if (corresp_ranking(tucp->primary, kind) <
          corresp_ranking((char*)scp, kind)) {
        /* The primary IL entry is the canonical one, but the current entry
           is better.  That means the correspondence information is wrong. */
#if DEBUG
/* GCOVR_EXCL_START */
        db_entity_info(tucp->primary, kind);
        db_entity_info((char *)scp, kind);
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        unexpected_condition_str("primary entry should not be canonical");
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_correspondences */

#else /* !CHECKING */
#define check_correspondences(scp, kind) /* Nothing */
#endif /* CHECKING */


static void f_mark_to_merge(char *ptr, an_il_entry_kind kind)
/*
Mark the given entry (of kind "kind") as one that must be merged with its
counterpart in the primary IL.

The merge process occurs in effectively two phases.  The merge process starts
with the first phase when copy_from_secondary_to_primary_IL is called.  During
this phase IL will be copied from the secondary translation unit to a "remapped
copy."  The remapped copy is a temporary copy of the IL entry with all pointers
remapped to IL entries in the primary IL memory region.  The remapped copy
itself is allocated in the secondary translation unit's IL memory region and is
thus dropped along with the secondary translation unit's IL following the
merge.

The merge operation is then completed in its secondary phase, during the final
(manual) walk of the translation unit, facilitated by finish_trans_unit_copy.
For entries that were marked for merging the copy address returned points to
the remapped copy.  The primary IL entry's address is then obtained from the
copy address of the remapped copy.  As all pointers at this point reachable
from the remapped copy refer to entries in the primary IL, information can be
safely and freely moved from the remapped copy to the primary IL entry (thus
facilitating the merge).

Note: copy_address_setup will be called to set the copy address if it is not
set already, so if a special copy address is required it should be established
before this routine is called.
*/
{
  check_assertion_str(in_file_scope(ptr) && in_secondary_trans_unit(ptr),
                      "f_mark_to_merge: bad input pointer");
  /* If the flag is set already, do nothing. */
  if (!il_entry_prefix_of(ptr).il_lowering_flag) {
    /* Strings should never be marked for merging (as there's nothing to
       "merge").  String entries are allocated in copy_string_entry (because
       the length is known there). */
    check_assertion(!is_string_entry_kind(kind));
    /* Set a flag to request merging.  The IL lowering flag is borrowed
       for this process because IL lowering is not done on secondary
       translation units. */
    il_entry_prefix_of(ptr).il_lowering_flag = TRUE;
    /* Get the copy address set if it is not set already. */
    copy_address_setup(ptr, kind, /*known_will_process_in_curr_walk=*/TRUE);

    /* The copy address of ptr has been configured to a location in the primary
       IL. */
    char *primary = trans_unit_copy_address_of(ptr);
    check_assertion_str(primary != NULL,
                        "f_mark_to_merge: copy address is not set");
    check_assertion_str(!in_secondary_trans_unit(primary),
                        "f_mark_to_merge: copy address is in sec trans unit");

    /* Allocate space for a copy in the secondary translation unit IL
       so we will have a version with all the pointers remapped appropriately.
       The original entry points to the copy, which points (via its
       copy address pointer) to the primary IL. */
    char *copy = alloc_il(sizeof_il_entry[(int)kind]);
    trans_unit_copy_address_of(ptr) = copy;
    trans_unit_copy_address_of(copy) = primary;
    /* Set the flag to request copying. */
    set_entry_needs_copy_flag(ptr);
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_trace("trans_copy", ptr, kind)) {
      fprintf(f_debug, "assigned addr for copy in secondary at %p:\n",
              (a_void_ptr)copy);
      db_entity_info(ptr, kind);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
}  /* f_mark_to_merge */


static a_boolean strip_non_merged_attributes(a_source_correspondence *scp)
/*
Remove from the list of attributes attached to scp any that are not
marked to be merged.  This is so we don't have to copy the non-merged
attributes only to discard them later.  Return TRUE if the list
contained at least one attribute to be merged.
*/
{
  a_boolean        requires_merge = FALSE;
  an_attribute_ptr ap, ap_next, end_of_list = NULL;

  check_assertion(trans_unit_corresp_of((a_type_ptr)scp)->canonical !=
                                                                  (char *)scp);
  for (ap = scp->attributes, scp->attributes = NULL;
       ap != NULL;
       ap = ap_next) {
    ap_next = ap->next;
    if (ap->must_be_preserved_in_trans_unit_copy) {
      requires_merge = TRUE;
      if (end_of_list == NULL) {
        scp->attributes = ap;
      } else {
        end_of_list->next = ap;
      }  /* if */
      end_of_list = ap;
      ap->next = NULL;
    }  /* if */
  }  /* for */
  return requires_merge;
}  /* strip_non_merged_attributes */


template<typename an_Entry_type>
static inline a_boolean entry_requires_merge_because_of_attributes(
                                                          an_Entry_type *entry)
/*
If the indicated entry has attributes that must be merged into the canonical
entry as part of the copy/merge process, return TRUE.

The entry passed in must not be one that will be copied instead of merged
(i.e., entry_should_be_copied must be FALSE for it), nor may it be one that
will overwrite a primary IL entry (i.e., entry_should_overwrite_primary_entry
must be FALSE for it).  The combination of those two means the entry is not the
canonical entry (all of whose attributes would be preserved), and is instead an
entry that would otherwise be discarded.
*/
{
  check_assertion(!entry_should_be_copied(entry));
  check_assertion(!entry_should_overwrite_primary_entry(entry));
  a_boolean                   result = FALSE;
  a_source_correspondence_ptr scp = &entry->source_corresp;

  /* Only entries with correspondences require this special processing,
     because only they can be merged. */
  if (scp->attributes != NULL && trans_unit_corresp_of(entry) != NULL) {
    /* Go through the list of attributes and keep only those marked as
       requiring a merge.  Note whether we found any. */
    if (strip_non_merged_attributes(scp)) result = TRUE;
  }  /* if */
  return result;
}  /* entry_requires_merge_because_of_attributes */


template<typename an_Entry_type>
static inline a_boolean entry_requires_merge_because_of_name_references(
                                                          an_Entry_type *entry)
/*
If the indicated entry has name references that must be merged into the
canonical entry as part of the copy/merge process, return TRUE.

The entry passed in must not be one that will be copied instead of merged
(i.e., entry_should_be_copied must be FALSE for it), nor may it be one that
will overwrite a primary IL entry (i.e., entry_should_overwrite_primary_entry
must be FALSE for it).  The combination of those two means the entry is not the
canonical entry (all of whose name references would be preserved), and is
instead an entry that would otherwise be discarded.
*/
{
  check_assertion(!entry_should_be_copied(entry));
  check_assertion(!entry_should_overwrite_primary_entry(entry));
  a_boolean                   result = FALSE;
  a_source_correspondence_ptr scp = &entry->source_corresp;

  /* Only entries with correspondences require this special processing,
     because only they can be merged. */
  if (scp->name_references != NULL && trans_unit_corresp_of(entry) != NULL) {
    /* All name references are always kept. */
    result = TRUE;
  }  /*if */
  return result;
}  /* entry_requires_merge_because_of_name_references */


template<typename an_Entry_type>
static inline a_boolean entry_requires_merge_because_of_details(
                                                          an_Entry_type *entry)
/*
Return TRUE if the given entity requires merging because of some details,
e.g., it has attributes that must be merged into the composite entity.
*/
{
  a_boolean result = FALSE;

  if (entry_requires_merge_because_of_attributes(entry)) {
    result = TRUE;
  } else if (entry_requires_merge_because_of_name_references(entry)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* entry_requires_merge_because_of_details */


static inline void
merge_attributes(a_source_correspondence *expiring_scp,
                 a_source_correspondence *surviving_scp)
/*
Transfer from the expiring_scp to the surviving_scp any attributes that
must be merged in.
*/
{
  an_attribute_ptr ap, last_ap, ap_next;

  ap = expiring_scp->attributes;
  if (ap != NULL) {
    if (in_secondary_trans_unit(ap)) {
      /* When transfer_xxx_details is called for a case where a non-canonical
         secondary entry is being deleted because it duplicates information
         in the primary entry, no attributes should be ones that need to
         be copied (if there were any, we should have concluded that the
         entry needed to be merged). */
#if CHECKING
      for (; ap != NULL; ap = ap->next) {
        check_assertion(!ap->must_be_preserved_in_trans_unit_copy);
      }  /* for */
#endif /* CHECKING */
    } else {
      expiring_scp->attributes = NULL;
      /* Move the list of attributes headed by ap onto the list of attributes
         attached to surviving_scp.  Copy only the ones marked to be merged. */
      last_ap = surviving_scp->attributes;
      if (last_ap != NULL) {
        /* Find the last attribute on surviving_scp so we can add after it. */
        while (last_ap->next != NULL) last_ap = last_ap->next;
      }  /* if */
      for (; ap != NULL; ap = ap_next) {
        ap_next = ap->next;
        if (ap->must_be_preserved_in_trans_unit_copy) {
          /* Keep this attribute on the merged list. */
          if (last_ap == NULL) {
            surviving_scp->attributes = ap;
          } else {
            last_ap->next = ap;
          }  /* if */
          last_ap = ap;
          ap->next = NULL;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* merge_attributes */


static inline void
merge_name_reference_lists(a_source_correspondence *expiring_scp,
                           a_source_correspondence *surviving_scp)
/*
Merge the name reference list from the expiring_scp into the surviving_scp.
*/
{
  a_name_reference_ptr nrp, last_nrp, nrp_next;

  nrp = expiring_scp->name_references;
  if (nrp != NULL) {
    expiring_scp->name_references = NULL;
    /* Move the list of name references headed by nrp onto the list of name
       references attached to surviving_scp. */
    last_nrp = surviving_scp->name_references;
    if (last_nrp != NULL) {
      /* Find the last name reference on surviving_scp so we can add after
         it. */
      while (last_nrp->next != NULL) last_nrp = last_nrp->next;
    }  /* if */
    for (; nrp != NULL; nrp = nrp_next) {
      nrp_next = nrp->next;
      /* Keep this name reference on the merged list. */
      if (last_nrp == NULL) {
        surviving_scp->name_references = nrp;
      } else {
        last_nrp->next = nrp;
      }  /* if */
      last_nrp = nrp;
      nrp->next = NULL;
    }  /* for */
  }  /* if */
}  /* merge_name_reference_lists */


static void merge_entity_details(a_source_correspondence *expiring_scp,
                                 a_source_correspondence *surviving_scp)
/*
Transfer from the expiring_scp to the surviving_scp any details that
must be merged in.  For example, merge the lists of attributes, if any.
*/
{
  /* If this assertion fails, a merge is being performed as part of setup. */
  check_assertion(!in_trans_copy_setup);
  merge_attributes(expiring_scp, surviving_scp);
  merge_name_reference_lists(expiring_scp, surviving_scp);
}  /* merge_entity_details */


static void transfer_type_details(a_type_ptr type,
                                  a_type_ptr corresp_type)
/*
The type identified by "type" is about to be eliminated.
If there is any useful minor information in that type, e.g.,
about use in exceptions, merge it into corresp_type, which is
not being eliminated.
*/
{
  if (type->used_in_exception_or_rtti) {
    corresp_type->used_in_exception_or_rtti = TRUE;
  }  /* if */
  if (is_immediate_class_type(type)) {
    a_class_type_supplement_ptr ctsp, corresp_ctsp;
    ctsp = type->variant.class_struct_union.extra_info;
    check_assertion(is_immediate_class_type(corresp_type));
    corresp_ctsp = corresp_type->variant.class_struct_union.extra_info;
#if NEW_CAN_BE_FOLDED_INTO_CTOR
    if (corresp_ctsp->assoc_operator_new_routine == NULL &&
        ctsp->assoc_operator_new_routine != NULL) {
      corresp_ctsp->assoc_operator_new_routine =
                            (a_routine_ptr)primary_il_entry_of(
                                      (char *)ctsp->assoc_operator_new_routine,
                                      iek_routine);
    }  /* if */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
    if (corresp_ctsp->assoc_operator_delete_routine == NULL &&
        ctsp->assoc_operator_delete_routine) {
      corresp_ctsp->assoc_operator_delete_routine =
                         (a_routine_ptr)primary_il_entry_of(
                                   (char *)ctsp->assoc_operator_delete_routine,
                                   iek_routine);
    }  /* if */
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (corresp_ctsp->uuid_string == NULL && ctsp->uuid_string != NULL) {
      /* Preserve the UUID string associated with a class if it was only
         specified on the entry about to be eliminated. */
      corresp_ctsp->uuid_string = ctsp->uuid_string;
    }  /* if */
    if ((ctsp->decl_modifiers & DM_DLLEXPORT) != 0) {
      /* __declspec(dllexport) takes precedence over
         __declspec(dllimport).  */
      corresp_ctsp->decl_modifiers |= DM_DLLEXPORT;
      corresp_ctsp->decl_modifiers &= ~DM_DLLIMPORT;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (is_immediate_enum_type(type)) {
    if (integer_type_supp(corresp_type)->uuid_string == NULL &&
        integer_type_supp(type)->uuid_string != NULL) {
      /* Preserve the UUID string associated with an enum type if it was only
         specified on the entry about to be eliminated. */
      integer_type_supp(corresp_type)->uuid_string =
                                         integer_type_supp(type)->uuid_string;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
}  /* transfer_type_details */


static void transfer_variable_flags(a_variable_ptr variable,
                                    a_variable_ptr corresp_variable)
/*
The variable identified by "variable" is about to be eliminated.
If there is any useful information in flags in that variable, e.g.,
about attributes, merge it into corresp_variable, which is not being
eliminated.
*/
{
  corresp_variable->address_taken |= variable->address_taken;
  if (variable->alignment > corresp_variable->alignment) {
    corresp_variable->alignment = variable->alignment;
  }  /* if */
}  /* transfer_variable_flags */


static void transfer_routine_flags(a_routine_ptr routine,
                                   a_routine_ptr corresp_routine)
/*
The routine identified by "routine" is about to be eliminated.
If there is any useful information in flags in that routine, e.g.,
about inline attributes, merge it into corresp_routine, which is
not being eliminated.
*/
{
  a_type_ptr                    rout_type, corresp_rout_type;
  a_routine_type_supplement_ptr rtsp, corresp_rtsp;
  a_param_type_ptr              param, corresp_param;

  /* Transfer the passed_via_copy_constructor flag in parameters.
     It may not get set if the routine is not called. */
  rout_type = skip_typerefs(routine->type);
  corresp_rout_type = skip_typerefs(corresp_routine->type);
  rtsp = rout_type->variant.routine.extra_info;
  corresp_rtsp = corresp_rout_type->variant.routine.extra_info;
  param = rtsp->param_type_list;
  corresp_param = corresp_rtsp->param_type_list;
  for (; param != NULL && corresp_param != NULL;
       param = param->next, corresp_param = corresp_param->next) {
    if (param->passed_via_copy_constructor) {
      corresp_param->passed_via_copy_constructor = TRUE;
    }  /* if */
  }  /* for */
  if (rtsp->value_returned_by_cctor) {
    corresp_rtsp->value_returned_by_cctor = TRUE;
  }  /* if */
  /* Any closures defined in the default arguments for the routine should
     be disconnected from that routine. */
  for (param = rtsp->param_type_list; param != NULL; param = param->next) {
    an_il_entity_list_entry_ptr   entry;
    for (entry = param->entities_defined_in_default_arg;
         entry != NULL;
         entry = entry->next) {
      if (entry->entity.kind == iek_type) {
        a_type_ptr  tp = (a_type_ptr)entry->entity.ptr;
        if (type_is_lambda_closure(tp)) {
          class_symbol_supp(symbol_for(tp))
                    ->lambda_immediately_inside_default_arg_expression = FALSE;
#if NEED_NAME_MANGLING
          class_type_supp(tp)->lambda_parent.routine = NULL;
#endif /* NEED_NAME_MANGLING */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* for */
  corresp_routine->address_taken |= routine->address_taken;
  corresp_routine->called        |= routine->called;
  check_assertion((param == NULL && corresp_param == NULL) ||
                  !rtsp->prototyped || !corresp_rtsp->prototyped);
  check_assertion(routine->is_inline == corresp_routine->is_inline ||
                  /* In C mode, the inline specifier need not match. */
                  C_mode() ||
                  /* In C++, there are a lot of cases where a declaration
                     can be not inline while the definition is inline.
                     For templates, the inline flag is not set until
                     the function is fully instantiated. */
                  routine->function_def_number == NULL_function_def_number ||
                  corresp_routine->function_def_number ==
                                                     NULL_function_def_number);
#if INSTANTIATE_EXTERN_INLINE
  if (instantiate_extern_inline) {
    corresp_routine->inline_instance_required |=
                                             routine->inline_instance_required;
  }  /* if */
#endif /* INSTANTIATE_EXTERN_INLINE */
  if (rtsp->does_not_return && !corresp_rtsp->does_not_return) {
    /* corresp_routine must be marked as "noreturn", but beware of routines
       declared with a typedef type. */
    ensure_underlying_function_type_is_modifiable(&corresp_routine->type,
                                                  &corresp_rout_type);
    corresp_rtsp = corresp_rout_type->variant.routine.extra_info;
    corresp_rtsp->does_not_return = TRUE;
  }  /* if */
  if (routine->never_throws) corresp_routine->never_throws = TRUE;
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  if (routine->never_inline) corresp_routine->never_inline = TRUE;
#if GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  if (routine->is_naked) corresp_routine->is_naked = TRUE;
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
#if DECL_MODIFIERS_IN_USE && MICROSOFT_EXTENSIONS_ALLOWED
  /* Merge the __forceinline specifier (it should be retained if it appeared
     on any of the declarations). */
  corresp_routine->decl_modifiers |= routine->decl_modifiers & DM_FORCEINLINE;
#endif /* DECL_MODIFIERS_IN_USE && MICROSOFT_EXTENSIONS_ALLOWED */
  /* Note that suppress_inline_body is meaningful only when the routine
     has a body, and the interesting value -- the one that sticks --
     is FALSE. */
  if (routine->function_def_number != NULL_function_def_number &&
      corresp_routine->function_def_number != NULL_function_def_number) {
    corresp_routine->suppress_inline_body &= routine->suppress_inline_body;
  }  /* if */
}  /* transfer_routine_flags */


static a_boolean prepare_for_trans_unit_copy(
                                      a_scope_ptr scope,
                                      a_boolean   *any_removed_function_bodies)
/*
Scan the indicated scope (a file, namespace, or class scope in a secondary
translation unit) and its subscopes and set up for copying the scope
to the primary translation unit IL.  *any_removed_function_bodies is
set to TRUE if the body of a routine is eliminated.  Returns TRUE if the
entity associated with the scope should be kept on the caller's list
(because something in it needs to be copied or merged later).
The entity lists of the scope are pruned so that only entities
that must be copied to or merged into the primary IL are left on
the lists, and entities that must be merged are marked with the merge
flag.  This routine is called with the current translation unit set
to the secondary translation unit.
*/
{
  a_boolean          keep_on_parent_list = FALSE;
  a_boolean          check_member_merges = FALSE;
  a_boolean          any_members_to_process = FALSE;
  a_type_ptr         type, prev_type;
  a_variable_ptr     variable, prev_variable;
  a_dynamic_init_ptr dyn_init, prev_dyn_init;
  a_routine_ptr      routine, prev_routine;
  a_template_ptr     templ, prev_templ;
  a_namespace_ptr    nsp, prev_nsp;
  a_pragma_ptr       pragma, prev_pragma;
  a_type_ptr         class_type;
  a_boolean          keep_on_list, keep_body;
  a_scope_pointers_block
                     *pointers_block;

  /* Set the correspondence for this scope, if any. */
  if (scope->kind == (a_scope_kind)sck_file) {
    /* The file scope in a secondary translation unit corresponds to
       the file scope in the primary translation unit, and gets merged
       into it. */
    a_scope_ptr corresp_scope = translation_units->primary_scope;
    check_assertion(il_entry_prefix_of(scope).il_walk_flag == 0);
    checked_trans_unit_copy_address_of(scope) = (char *)corresp_scope;
    mark_to_merge(scope, iek_scope);
    if (scope->lifetime != NULL && corresp_scope->lifetime != NULL) {
      /* The object lifetime of the file scope corresponds with the
         object lifetime of the corresponding scope, and gets merged into
         it. */
      checked_trans_unit_copy_address_of(scope->lifetime) =
                                               (char *)corresp_scope->lifetime;
      mark_to_merge(scope->lifetime, iek_object_lifetime);
    }  /* if */
    keep_on_parent_list = TRUE;
    check_member_merges = TRUE;
    pointers_block = get_pointers_block_for_scope(scope);
    check_assertion(pointers_block != NULL);
  } else if (scope->kind == (a_scope_kind)sck_namespace) {
    /* For a namespace scope, go to the a_namespace entry to find out what
       correspondence there is, if any. */
    nsp = scope->variant.assoc_namespace;
    if (entry_should_be_copied(nsp)) {
      /* The namespace doesn't exist in the primary IL, and just gets
         copied over.  We don't need to check its members. */
      keep_on_parent_list = TRUE;
      check_member_merges = FALSE;
    } else {
      /* The namespace scope gets merged into the corresponding scope. */
      a_namespace_ptr corresp_nsp= (a_namespace_ptr)canonical_il_entry_of(nsp);
      checked_trans_unit_copy_address_of(scope) =
                  primary_il_entry_of((char *)corresp_nsp->variant.assoc_scope,
                                      iek_scope);
      /* The namespace needs to be kept only if some of its members need
         to be processed.  Assume there are none, and correct that assumption
         as we look at the members. */
      keep_on_parent_list = FALSE;
      check_member_merges = TRUE;
    }  /* if */
    pointers_block = get_pointers_block_for_scope(scope);
    check_assertion(pointers_block != NULL);
  } else {
    /* A class scope. */
    check_assertion(scope->kind == (a_scope_kind)sck_class_struct_union);
    class_type = scope->variant.assoc_type;
    if (entry_should_be_copied(class_type)) {
      /* The class doesn't exist in the primary IL, and just gets copied
         over.  We don't need to check its members. */
      keep_on_parent_list = TRUE;
      check_member_merges = FALSE;
    } else if (entry_should_overwrite_primary_entry(class_type)) {
      /* The class overwrites the corresponding class in the primary IL.
         This happens, for example, when the class in the primary IL has
         a declaration and the class in the secondary IL has a definition.
         The definition will be copied over, but we don't need to check
         the members. */
      keep_on_parent_list = TRUE;
      check_member_merges = FALSE;
      mark_to_merge(class_type, iek_type);
    } else {
      /* The class gets merged into the corresponding class. */
      a_type_ptr corresp_class = (a_type_ptr)canonical_il_entry_of(class_type);
      check_assertion(corresp_class != class_type);
      if (class_type_has_body(corresp_class)) {
        /* Both instances of the class have definitions, so their scopes
           correspond. */
        a_scope_ptr corresp_class_scope = corresp_class->variant.
                                    class_struct_union.extra_info->assoc_scope;
        checked_trans_unit_copy_address_of(scope) =
                       (char *)primary_il_entry_of((char *)corresp_class_scope,
                                                   iek_scope);
      }  /* if */
      /* The class here needs to be kept only if some of its members need
         to be processed.  Assume there are none, and correct that assumption
         as we look at the members. */
      keep_on_parent_list = FALSE;
      check_member_merges = TRUE;
      if (entry_requires_merge_because_of_details(class_type)) {
        /* The class type has some details that must be merged into the
           canonical entry. */
        keep_on_parent_list = TRUE;
        mark_to_merge(class_type, iek_type);
      }  /* if */
      { a_field_ptr fp;
        a_boolean   has_field_with_attr_to_merge =
                                      symbol_supplement_for_class(class_type)->
                                                  has_field_with_attr_to_merge;
        for (fp = class_type->variant.class_struct_union.field_list;
             fp != NULL;
             fp = fp->next) {
          a_field_ptr corresp_field = (a_field_ptr)canonical_il_entry_of(fp);
          if (corresp_field->initializer == NULL && fp->initializer != NULL) {
            /* Mark the field to be merged so the initializer can be properly
               merged. */
            any_members_to_process = TRUE;
            mark_to_merge(fp, iek_field);
            continue;
          } else if (fp->initializer != NULL) {
            /* Eliminate any object lifetime associated with the initializer
               (which is in the global scope, but shouldn't be merged into the
               primary global scope). */
            a_dynamic_init     *field_init = fp->initializer;
            an_object_lifetime *init_lifetime = field_init->init_expr_lifetime;

            if (init_lifetime != NULL) {
              detach_from_object_lifetime_tree(init_lifetime);
            }  /* if */
          }  /* if */
          if (has_field_with_attr_to_merge) {
            /* The class has at least one field that has an attribute that must
               be merged.  Walk through all fields and strip the attributes
               except for those that must be merged. */
            if (strip_non_merged_attributes(&fp->source_corresp)) {
              any_members_to_process = TRUE;
              mark_to_merge(fp, iek_field);
            }  /* if */
          }  /* if */
        }  /* for */
      }
    }  /* if */
    pointers_block = NULL;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Source sequence entries aren't generated in secondary translation
     units. */
  check_assertion(scope->source_sequence_list == NULL &&
                  scope->src_seq_sublist_list == NULL);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL
  /* The hidden name table is not generated in secondary translation units.
     If that is changed, keep two things in mind:
       1)  With instantiated exported templates, it's going to be hard to
           define what the hidden name table should contain for the
           synthesized lookup context in the instantiation.
       2)  The hidden name table from the scope here would have to be
           merged into the destination scope, with duplicates eliminated. */
  check_assertion(scope->hidden_names == NULL);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  /* Visit all types. */
  prev_type = NULL;
  for (type = scope->types; type != NULL; type = type->next) {
#if DEBUG
/* GCOVR_EXCL_START */
    a_type_ptr trace_type = type;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    check_correspondences(&type->source_corresp, iek_type);
    keep_on_list = TRUE;
    if (is_immediate_class_type(type) &&
        type->variant.class_struct_union.extra_info != NULL &&
        type->variant.class_struct_union.extra_info->assoc_scope != NULL) {
      /* A class with a scope.  Do a recursive call to process it. */
      a_scope_ptr class_scope =
                      type->variant.class_struct_union.extra_info->assoc_scope;
      check_assertion(!class_scope->is_placeholder_scope);
      keep_on_list = prepare_for_trans_unit_copy(class_scope,
                                                 any_removed_function_bodies);
    } else if (entry_should_be_copied(type)) {
      /* The type doesn't exist in the primary IL, and just gets copied
         over. */
      keep_on_list = TRUE;
    } else if (entry_should_overwrite_primary_entry(type)) {
      /* The type overwrites the corresponding type in the primary IL.
         This happens, for example, when the type is an enum with a definition
         in the secondary translation unit but only a declaration in the
         primary IL. */
      check_assertion(check_member_merges);
      keep_on_list = TRUE;
      mark_to_merge(type, iek_type);
    } else if (check_member_merges &&
               entry_requires_merge_because_of_details(type)) {
      /* The type has some details that must be merged into the
         canonical entry. */
      keep_on_list = TRUE;
      mark_to_merge(type, iek_type);
    } else {
      /* The type is a duplicate of one elsewhere and should be discarded. */
      keep_on_list = FALSE;
    }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_trace("trans_copy", trace_type, iek_type)) {
      fprintf(f_debug, "prepare_for_trans_unit_copy, ");
      fprintf(f_debug, "%skeeping on list, ", keep_on_list ? "" : "not ");
      fprintf(f_debug, "%smerging:\n",
                       entry_to_be_merged(type) ? "" : "not ");
      db_entity_info((char *)type, iek_type);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    if (keep_on_list) {
      prev_type = type;
      any_members_to_process = TRUE;
      if (!C_mode() && is_immediate_class_type(type) &&
          type->variant.class_struct_union.extra_info != NULL) {
        /* Clear befriending lists so they are not copied.  They will be
           rebuilt later. */
        type->variant.class_struct_union.extra_info->befriending_classes= NULL;
      }  /* if */
    } else {
      /* Remove this entry from the list. */
      if (prev_type == NULL) {
        scope->types = type->next;
      } else {
        prev_type->next = type->next;
      }  /* if */
      /* Transfer any minor information that would otherwise be lost. */
      { a_type_ptr corresp_type = (a_type_ptr)canonical_il_entry_of(type);
        transfer_type_details(type, corresp_type);
      }
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_type = prev_type;
  /* Visit all static variables (non-static variables come up only
     in function and block scopes, which don't come here). */
  prev_variable = NULL;
  for (variable = scope->variables;
       variable != NULL;
       variable = variable->next) {
    if (trans_unit_corresp_of(variable) == NULL &&
        variable->is_nonreal && !variable->is_prototype_instantiation) {
      /* Nonreal variables can be introduced during template instantiations
         in a secondary translation unit and thus will not have been
         processed for correspondences earlier.  Since each nonreal
         nonprototype instance of a templated variable is considered to be
         a distinct entry not corresponding to any other entry, we can
         avoid checking the correspondence of such nonreal variables and
         the likely assertion failure that would result. */
    } else {
      check_correspondences(&variable->source_corresp, iek_variable);
    }  /* if */
    keep_on_list = TRUE;
    /* If we're supposed to copy only generated templates, other variables
       are made external (if necessary) and their definitions are
       dropped (the definition will be put out when the file
       is compiled as a primary file). */
    process_variable_if_unneeded_non_template(variable);
    if (entry_should_be_copied(variable)) {
      /* The variable doesn't exist in the primary IL, and just gets copied
         over. */
      keep_on_list = TRUE;
    } else if (entry_should_overwrite_primary_entry(variable)) {
      /* The variable overwrites the corresponding variable in the primary IL.
         This happens, for example, when the variable has a definition
         in the secondary translation unit but only a declaration in the
         primary IL. */
      check_assertion(check_member_merges);
      keep_on_list = TRUE;
      mark_to_merge(variable, iek_variable);
    } else if (check_member_merges &&
               entry_requires_merge_because_of_details(variable)) {
      /* The variable has some details that must be merged into the
         canonical entry. */
      keep_on_list = TRUE;
      mark_to_merge(variable, iek_variable);
    } else {
      /* The variable is a duplicate of one elsewhere and should be
         discarded. */
      keep_on_list = FALSE;
    }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_trace("trans_copy", variable, iek_variable)) {
      fprintf(f_debug, "prepare_for_trans_unit_copy, ");
      fprintf(f_debug, "%skeeping on list, ", keep_on_list ? "" : "not ");
      fprintf(f_debug, "%smerging:\n",
                       entry_to_be_merged(variable) ? "" : "not ");
      db_entity_info((char *)variable, iek_variable);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    if (keep_on_list) {
      prev_variable = variable;
      any_members_to_process = TRUE;
    } else {
      /* Remove this entry from the list. */
      if (prev_variable == NULL) {
        scope->variables = variable->next;
      } else {
        prev_variable->next = variable->next;
      }  /* if */
      /* Transfer any information that should be preserved in the canonical
         entry. */
      { a_variable_ptr corresp_variable =
                               (a_variable_ptr)canonical_il_entry_of(variable);
        transfer_variable_flags(variable, corresp_variable);
      }
      if (variable->storage_class == (a_storage_class)sc_unspecified) {
        /* Delete the definition of this variable. */
        clear_variable_definition(variable);
      }  /* if */
#if MAINTAIN_NEEDED_FLAGS
      /* The variable will not be copied over, so eliminate any
         default argument object lifetimes so they will not be copied
         over. */
      eliminate_variable_default_arg_object_lifetimes(variable);
#endif /* MAINTAIN_NEEDED_FLAGS */
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_variable = prev_variable;
  /* Visit all dynamic initializations. */
  prev_dyn_init = NULL;
  for (dyn_init = scope->dynamic_inits;
       dyn_init != NULL;
       dyn_init = dyn_init->next) {
    variable = dyn_init->variable;
    /* Remove an entry if the corresponding variable is no longer
       initialized.  See clear_variable_definition. */
    if (variable->init_kind == (an_init_kind)initk_none) {
      if (prev_dyn_init == NULL) {
        scope->dynamic_inits = dyn_init->next;
      } else {
        prev_dyn_init->next = dyn_init->next;
      }  /* if */
    } else {
      /* Keep this entry on the list. */
      prev_dyn_init = dyn_init;
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) {
    pointers_block->last_dynamic_init = prev_dyn_init;
  }  /* if */
  /* Visit all routines. */
  prev_routine = NULL;
  for (routine = scope->routines;
       routine != NULL;
       routine = routine->next) {
    check_correspondences(&routine->source_corresp, iek_routine);
    keep_on_list = TRUE;
    keep_body = TRUE;
    /* If we're supposed to copy only generated templates, other routines
       are made external (if necessary) and their definitions are
       dropped. */
    process_routine_if_unneeded_non_template(routine);
    if (entry_should_be_copied(routine)) {
      /* The routine doesn't exist in the primary IL, and just gets copied
         over. */
      keep_on_list = TRUE;
    } else if (entry_should_overwrite_primary_entry(routine)) {
      /* The routine overwrites the corresponding routine in the primary IL.
         This happens, for example, when the routine has a definition
         in the secondary translation unit but only a declaration in the
         primary IL. */
      check_assertion(check_member_merges);
      keep_on_list = TRUE;
      mark_to_merge(routine, iek_routine);
    } else if (check_member_merges &&
               entry_requires_merge_because_of_details(routine)) {
      /* The routine has some details that must be merged into the
         canonical entry.  We delete the body but keep the routine entry
         so we can do the merge from the copy of it. */
      keep_on_list = TRUE;
      keep_body = FALSE;
      mark_to_merge(routine, iek_routine);
    } else {
      /* The routine is a duplicate of one elsewhere and should be
         discarded. */
      keep_on_list = FALSE;
    }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
    if (db_trace("trans_copy", routine, iek_routine)) {
      fprintf(f_debug, "prepare_for_trans_unit_copy, ");
      fprintf(f_debug, "%skeeping on list, ", keep_on_list ? "" : "not ");
      fprintf(f_debug, "%smerging:\n",
                       entry_to_be_merged(routine) ? "" : "not ");
      db_entity_info((char *)routine, iek_routine);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    if (keep_on_list) {
      prev_routine = routine;
      any_members_to_process = TRUE;
      /* Clear befriending lists so they are not copied.  They will be
         rebuilt later. */
      if (!routine->is_inheriting_ctor) {
        routine->friends_or_originator.befriending_classes = NULL;
      }  /* if */
    } else {
      /* Remove this entry from the list. */
      keep_body = FALSE;
      if (prev_routine == NULL) {
        scope->routines = routine->next;
      } else {
        prev_routine->next = routine->next;
      }  /* if */
      /* Preserve some information regarding inline functions. */
      { a_routine_ptr corresp_routine =
                                 (a_routine_ptr)canonical_il_entry_of(routine);
        transfer_routine_flags(routine, corresp_routine);
      }
    }  /* if */
    if (!keep_body) {
      if (routine->function_def_number != NULL_function_def_number) {
        /* Delete the body of this routine. */
        clear_body_for_routine(routine);
        *any_removed_function_bodies = TRUE;
      }  /* if */
#if MAINTAIN_NEEDED_FLAGS
      /* The routine will not be copied over, so eliminate any
         default argument object lifetimes so they will not be copied
         over. */
      eliminate_routine_default_arg_object_lifetimes(routine);
#endif /* MAINTAIN_NEEDED_FLAGS */
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_routine = prev_routine;
  /* Visit all templates. */
  prev_templ = NULL;
  for (templ = scope->templates;
       templ != NULL;
       templ = templ->next) {
    if (entry_should_be_copied(templ)) {
      /* The template doesn't exist in the primary IL, and just gets copied
         over. */
      keep_on_list = TRUE;
    } else if (entry_should_overwrite_primary_entry(templ)) {
      /* The template overwrites the corresponding template in the primary IL.
         This happens, for example, when the template has a definition
         in the secondary translation unit but only a declaration in the
         primary IL. */
      check_assertion(check_member_merges);
      /* Eliminate the link to the primary IL copy in this case, to
         keep both entries. */
      trans_unit_corresp_of(templ)->primary = NULL;
      keep_on_list = TRUE;
    } else {
      /* The template is a duplicate of one elsewhere and should be
         discarded. */
      keep_on_list = FALSE;
    }  /* if */
    if (keep_on_list) {
      prev_templ = templ;
      any_members_to_process = TRUE;
    } else {
      /* Remove this entry from the list. */
      if (prev_templ == NULL) {
        scope->templates = templ->next;
      } else {
        prev_templ->next = templ->next;
      }  /* if */
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_template = prev_templ;
  /* Visit all namespaces. */
  prev_nsp = NULL;
  for (nsp = scope->namespaces;
       nsp != NULL;
       nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      /* Do a recursive call to process the namespace. */
      keep_on_list = prepare_for_trans_unit_copy(nsp->variant.assoc_scope,
                                                 any_removed_function_bodies);
    } else {
      /* A namespace alias.  Keep it only if it needs to be copied to the
         primary IL. */
      keep_on_list = entry_should_be_copied(nsp);
      check_assertion(!entry_should_overwrite_primary_entry(nsp));
    }  /* if */
    if (keep_on_list) {
      prev_nsp = nsp;
      any_members_to_process = TRUE;
    } else {
      /* Remove this entry from the list. */
      if (prev_nsp == NULL) {
        scope->namespaces = nsp->next;
      } else {
        prev_nsp->next = nsp->next;
      }  /* if */
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_namespace = prev_nsp;
  /* Visit all pragmas. */
  prev_pragma = NULL;
  for (pragma = scope->pragmas;
       pragma != NULL;
       pragma = pragma->next) {
    a_source_correspondence_ptr scp;
    /* Keep the pragma if it has an associated entity that will be kept. */
    keep_on_list = FALSE;
    if (pragma->entity.ptr != NULL &&
        (scp = source_corresp_for_il_entry(
                             pragma->entity.ptr,
                             (an_il_entry_kind)pragma->entity.kind)) != NULL &&
        /* "a_constant_ptr" here is arbitrary. */
        entry_should_be_kept((a_constant_ptr)scp)) {
      keep_on_list = TRUE;
    }  /* if */
    if (keep_on_list) {
      prev_pragma = pragma;
      any_members_to_process = TRUE;
    } else {
      /* Remove this entry from the list. */
      if (prev_pragma == NULL) {
        scope->pragmas = pragma->next;
      } else {
        prev_pragma->next = pragma->next;
      }  /* if */
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_pragma = prev_pragma;
#if MICROSOFT_EXTENSIONS_ALLOWED
  { an_ms_attribute_ptr ms_attr, prev_ms_attr;
    /* Visit all Microsoft attributes. */
    prev_ms_attr = NULL;
    for (ms_attr = scope->ms_attributes;
         ms_attr != NULL;
         ms_attr = ms_attr->next) {
      a_source_correspondence_ptr scp;
      /* Keep the attribute if it has an associated entity that will
         be kept. */
      keep_on_list = FALSE;
      if (ms_attr->entity.ptr != NULL &&
          (scp = source_corresp_for_il_entry(
                            ms_attr->entity.ptr,
                            (an_il_entry_kind)ms_attr->entity.kind)) != NULL &&
          /* "a_constant_ptr" here is arbitrary. */
          entry_should_be_kept((a_constant_ptr)scp)) {
        keep_on_list = TRUE;
      }  /* if */
      if (keep_on_list) {
        prev_ms_attr = ms_attr;
        any_members_to_process = TRUE;
      } else {
        /* Remove this entry from the list. */
        if (prev_ms_attr == NULL) {
          scope->ms_attributes = ms_attr->next;
        } else {
          prev_ms_attr->next = ms_attr->next;
        }  /* if */
      }  /* if */
    }  /* for */
    if (pointers_block != NULL) {
      pointers_block->last_ms_attribute = prev_ms_attr;
    }  /* if */
  }
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (scope->kind == (a_scope_kind)sck_file) {
    if (*any_removed_function_bodies) {
      /* Remove scope orphaned list entries for eliminated functions. */
#if MAINTAIN_NEEDED_FLAGS
      if (okay_to_eliminate_unneeded_il_entries) {
        eliminate_unneeded_scope_orphaned_list_entries();
      }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
    }  /* if */
    /* Mark the entries on the il_header nontag_types_used_in_exception_or_rtti
       list that shouldn't be copied.  Every type must stay on the list
       in the secondary IL, because the fact that the type exists in the
       primary IL does not guarantee that the type is on the nontag_...
       list in the primary IL.  The ones that don't need to be copied are
       marked for merging. */
    for (type = il_header.nontag_types_used_in_exception_or_rtti;
         type != NULL;
         type = type->next) {
      check_assertion(in_secondary_trans_unit(type) &&
                      type->used_in_exception_or_rtti);
      if (!entry_should_be_copied(type)) {
        check_assertion(!entry_should_overwrite_primary_entry(type));
        mark_to_merge(type, iek_type);
      }  /* if */
    }  /* for */
  }  /* if */
  if (check_member_merges && any_members_to_process) {
    /* There are some members of this scope that need processing, so we have
       to keep the scope's associated entity on the list to be able to
       perform the merges. */
    keep_on_parent_list = TRUE;
    mark_to_merge(scope, iek_scope);
    if (scope->kind == (a_scope_kind)sck_namespace) {
      nsp = scope->variant.assoc_namespace;
      mark_to_merge(nsp, iek_namespace);
    } else if (scope->kind == (a_scope_kind)sck_class_struct_union) {
      class_type = scope->variant.assoc_type;
      mark_to_merge(class_type, iek_type);
    }  /* if */
  }  /* if */
  return keep_on_parent_list;
}  /* prepare_for_trans_unit_copy */


static void merge_object_lifetimes(a_scope_ptr scope,
                                   a_scope_ptr primary_scope)
/*
Merge the object lifetimes, if any, attached to scope and primary_scope.
Those are corresponding scopes (from a secondary translation unit and
the primary translation unit, respectively) that are being merged.
*/
{
  an_object_lifetime_ptr lifetime, primary_lifetime;

  if (scope->lifetime != NULL) {
    check_assertion(scope->kind == (a_scope_kind)sck_file);
    /* Use the copy of the scope lifetime, which has remapped pointers. */
    lifetime = (an_object_lifetime_ptr)checked_trans_unit_copy_address_of(
                                                              scope->lifetime);
    primary_lifetime = primary_scope->lifetime;
    if (primary_lifetime == NULL) {
      /* There is no object lifetime in the primary scope, so the
         lifetime is copied not merged. */
      primary_scope->lifetime = lifetime;
    } else {
      an_object_lifetime_ptr child, last_child;
      a_dynamic_init_ptr     last_destr;
      primary_lifetime->has_implicit_child |= lifetime->has_implicit_child;
      /* Copy the children of "lifetime" to the end of the list of children
         of "primary lifetime". */
      last_child = primary_lifetime->child_lifetime;
      if (last_child == NULL) {
        primary_lifetime->child_lifetime = lifetime->child_lifetime;
      } else {
        while (last_child->next != NULL) last_child = last_child->next;
        last_child->next = lifetime->child_lifetime;
      }  /* if */
      /* Copy the destructions list of "lifetime" to the front of the
         destructions list of "primary_lifetime" (the list is in order
         of destruction -- backwards -- so new entries go at the front
         of the list). */
      last_destr = lifetime->destructions;
      if (last_destr != NULL) {
        /* Find the end of the list. */
        while (last_destr->next_in_destruction_list != NULL) {
          last_destr = last_destr->next_in_destruction_list;
        }  /* while */
        last_destr->next_in_destruction_list = primary_lifetime->destructions;
        primary_lifetime->destructions = lifetime->destructions;
        /* Adjust the parent_destruction_sublist pointer in children:
           a NULL pointer, meaning end of the list, becomes the first
           destruction on the primary_lifetime list. */
        for (child = lifetime->child_lifetime;
             child != NULL;
             child = child->next) {
          if (child->parent_destruction_sublist == NULL) {
            child->parent_destruction_sublist = primary_lifetime->destructions;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* merge_object_lifetimes */


/*
Macros that do saves/restores needed for each overwrite_primary_xxx
routine, used when an IL entry in the secondary translation unit
IL is copied on top of an existing entry in the primary IL.
*/
/*lint -e823*/
#if MAINTAIN_NEEDED_FLAGS
#define save_needed_flag_for_overwrite(primary_entry) \
  a_boolean saved_needed = (primary_entry)->source_corresp.needed;
#define restore_needed_flag_for_overwrite(primary_entry) \
  (primary_entry)->source_corresp.needed = saved_needed;
#else /* !MAINTAIN_NEEDED_FLAGS */
#define save_needed_flag_for_overwrite(primary_entry) /* Nothing */
#define restore_needed_flag_for_overwrite(primary_entry) /* Nothing */
#endif /* MAINTAIN_NEEDED_FLAGS */
#if ONE_INSTANTIATION_PER_OBJECT
#define save_per_instantiation_needed_flags_for_overwrite(primary_entry) \
  a_per_instantiation_needed_flags_entry_ptr saved_pi_needed = \
        (primary_entry)->source_corresp.per_instantiation_needed_flags;
#define restore_per_instantiation_needed_flags_for_overwrite(primary_entry) \
  (primary_entry)->source_corresp.per_instantiation_needed_flags = \
                                                       saved_pi_needed;
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define save_per_instantiation_needed_flags_for_overwrite(primary_entry) \
  /* Nothing */
#define restore_per_instantiation_needed_flags_for_overwrite(primary_entry) \
  /* Nothing */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#define do_saves_for_overwrite(primary_entry, entry_ptr_type) \
  entry_ptr_type saved_next = (primary_entry)->next; \
  save_needed_flag_for_overwrite(primary_entry) \
  save_per_instantiation_needed_flags_for_overwrite(primary_entry)
#define do_restores_for_overwrite(primary_entry, entry) \
  (primary_entry)->next = saved_next; \
  restore_needed_flag_for_overwrite(primary_entry) \
  restore_per_instantiation_needed_flags_for_overwrite(primary_entry)
/*lint +e823*/


static void overwrite_primary_type(a_type_ptr type,
                                   a_type_ptr primary_type)
/*
Overwrite the type primary_type (in the primary IL) with type (in
the secondary translation unit IL).
*/
{
  a_boolean                   is_class = is_immediate_class_type(type);
  a_class_list_entry_ptr      saved_befriending_classes = NULL;
#if MAINTAIN_NEEDED_FLAGS
  a_boolean                   saved_definition_needed = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
  a_class_type_supplement_ptr primary_ctsp;
  a_symbol_ptr                sym =
                               (a_symbol_ptr)(type->source_corresp.assoc_info);
  do_saves_for_overwrite(primary_type, a_type_ptr);
  if (is_class) {
    primary_ctsp = class_type_supp(primary_type);
    saved_befriending_classes = primary_ctsp->befriending_classes;
#if MAINTAIN_NEEDED_FLAGS
    saved_definition_needed =
                    primary_type->variant.class_struct_union.definition_needed;
#endif /* MAINTAIN_NEEDED_FLAGS */
  }  /* if */
  transfer_type_details(primary_type, type);
  *primary_type = *type;
  do_restores_for_overwrite(primary_type, type);
  if (is_class) {
    primary_ctsp = class_type_supp(primary_type);
    primary_ctsp->befriending_classes = saved_befriending_classes;
#if MAINTAIN_NEEDED_FLAGS
    primary_type->variant.class_struct_union.definition_needed =
                                                       saved_definition_needed;
#endif /* MAINTAIN_NEEDED_FLAGS */
  }  /* if */
  establish_as_canonical(&primary_type->source_corresp);
  if (sym != NULL) {
    /* Make the symbol (in a secondary translation unit) point to the
       copy of the type in the primary IL. */
    switch (sym->kind) {
      case sk_type:
        sym->variant.type.ptr = primary_type;
        break;
      case sk_enum_tag:
        sym->variant.enumeration.type = primary_type;
        break;
      case sk_class_or_struct_tag:
      case sk_union_tag:
        sym->variant.class_struct_union.type = primary_type;
        break;
      default:
        unexpected_condition_str("overwrite_primary_type: bad symbol kind");
    }  /* switch */
  }  /* if */
}  /* overwrite_primary_type */


static void overwrite_primary_variable(a_variable_ptr var,
                                       a_variable_ptr primary_var)
/*
Overwrite the variable primary_var (in the primary IL) with var (in
the secondary translation unit IL).
*/
{
#if ONE_INSTANTIATION_PER_OBJECT
  unsigned long saved_instantiation_needed_bit_number =
                                  primary_var->instantiation_needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  a_boolean     saved_address_taken = primary_var->address_taken;
  a_symbol_ptr  sym = (a_symbol_ptr)(var->source_corresp.assoc_info);
  do_saves_for_overwrite(primary_var, a_variable_ptr);
  *primary_var = *var;
  do_restores_for_overwrite(primary_var, var);
#if ONE_INSTANTIATION_PER_OBJECT
  primary_var->instantiation_needed_bit_number =
                                         saved_instantiation_needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (saved_address_taken) {
    primary_var->address_taken = TRUE;
  }  /* if */
  establish_as_canonical(&primary_var->source_corresp);
  if (sym != NULL) {
    /* Make the symbol (in a secondary translation unit) point to the
       copy of the variable in the primary IL. */
    switch (sym->kind) {
      case sk_variable:
        sym->variant.variable.ptr = primary_var;
        break;
      case sk_static_data_member:
        sym->variant.static_data_member.variable = primary_var;
        break;
      default:
        unexpected_condition_str(
                                "overwrite_primary_variable: bad symbol kind");
    }  /* switch */
  }  /* if */
}  /* overwrite_primary_variable */


static void overwrite_primary_routine(a_routine_ptr rout,
                                      a_routine_ptr primary_rout)
/*
Overwrite the routine primary_rout (in the primary IL) with rout (in
the secondary translation unit IL).
*/
{
#if ONE_INSTANTIATION_PER_OBJECT
  unsigned long saved_instantiation_needed_bit_number =
                                 primary_rout->instantiation_needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  a_class_list_entry_ptr saved_befriending_classes =
                                        rout_befriending_classes(primary_rout);
  a_routine_ptr          saved_inh_ctor_orig =
                                         inh_ctor_inherited_ctor(primary_rout);
  a_boolean saved_on_inline_function_list =
                                         primary_rout->on_inline_function_list;
#if MAINTAIN_NEEDED_FLAGS
  a_boolean saved_definition_needed = primary_rout->definition_needed;
#endif /* MAINTAIN_NEEDED_FLAGS */
#if IA64_ABI && DO_IL_LOWERING
  a_routine_list_entry_ptr saved_alternate_entry_points = NULL;
  a_ctor_or_dtor_kind saved_ctor_dtor_kind = (a_ctor_or_dtor_kind)cdk_none;
#endif /* IA64_ABI && DO_IL_LOWERING */
  a_symbol_ptr sym = (a_symbol_ptr)(rout->source_corresp.assoc_info);
  do_saves_for_overwrite(primary_rout, a_routine_ptr);
#if IA64_ABI && DO_IL_LOWERING
  if (primary_rout->special_kind == sfk_constructor ||
      primary_rout->special_kind == sfk_destructor) {
    saved_alternate_entry_points =
                        primary_rout->variant.ctor_dtor.alternate_entry_points;
    saved_ctor_dtor_kind =
                  enum_cast<a_ctor_or_dtor_kind>(primary_rout->ctor_dtor_kind);
  }  /* if */
#endif /* IA64_ABI && DO_IL_LOWERING */
  transfer_routine_flags(primary_rout, rout);
  *primary_rout = *rout;
  do_restores_for_overwrite(primary_rout, rout);
#if ONE_INSTANTIATION_PER_OBJECT
  primary_rout->instantiation_needed_bit_number =
                                         saved_instantiation_needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (primary_rout->is_inheriting_ctor) {
    primary_rout->friends_or_originator.inherited_routine= saved_inh_ctor_orig;
  } else {
    primary_rout->friends_or_originator.befriending_classes =
                                                     saved_befriending_classes;
  }  /* if */
  primary_rout->on_inline_function_list = saved_on_inline_function_list;
#if MAINTAIN_NEEDED_FLAGS
  primary_rout->definition_needed = saved_definition_needed;
#endif /* MAINTAIN_NEEDED_FLAGS */
#if IA64_ABI && DO_IL_LOWERING
  if (primary_rout->special_kind == (a_special_function_kind)sfk_constructor ||
      primary_rout->special_kind == (a_special_function_kind)sfk_destructor) {
    primary_rout->variant.ctor_dtor.alternate_entry_points =
                                                  saved_alternate_entry_points;
    primary_rout->ctor_dtor_kind = saved_ctor_dtor_kind;
  }  /* if */
#endif /* IA64_ABI && DO_IL_LOWERING */
  establish_as_canonical(&primary_rout->source_corresp);
  if (sym != NULL) {
    /* Make the symbol (in a secondary translation unit) point to the
       copy of the routine in the primary IL. */
    switch (sym->kind) {
      case sk_routine:
      case sk_member_function:
        sym->variant.routine.ptr = primary_rout;
        break;
      default:
        unexpected_condition_str("overwrite_primary_routine: bad symbol kind");
    }  /* switch */
  }  /* if */
}  /* overwrite_primary_routine */


static void finish_trans_unit_copy(a_scope_ptr scope)
/*
scope is a file, namespace, or class scope from the secondary file IL.  Do
processing required after the IL walk has copied IL entries from the secondary
scope to the primary file IL.  This includes putting copied entries on primary
IL lists and merging entries into the corresponding primary IL entries (see
f_mark_to_merge for more information about merging).  This routine is called
with the current translation unit set to the primary translation unit.
*/
{
  a_scope_ptr            primary_scope;
  a_scope_pointers_block *pointers_block;
  a_boolean              is_class_scope =
                         (scope->kind == (a_scope_kind)sck_class_struct_union);
  a_boolean              scope_being_merged = entry_to_be_merged(scope);
  a_boolean              add_to_list, move_to_end;

  check_assertion(is_primary_translation_unit);
  /* Find the corresponding scope. */
  primary_scope = (a_scope_ptr)transitive_copy_address_of(scope);
  /* Get the pointers block for the primary IL scope. */
  pointers_block = get_pointers_block_for_scope(primary_scope);
  check_assertion(pointers_block != NULL || is_class_scope);
  if (scope->types != NULL) {
    a_type_ptr type, last_type;
    /* Merge the types in the scope into the primary IL scope. */
    /* Get a pointer to the last type in the primary scope. */
    if (pointers_block != NULL) {
      last_type = pointers_block->last_type;
    } else {
      /* Actual last type will be determined below if/when needed. */
      last_type = NULL;
    }  /* if */
    for (type = scope->types; type != NULL; type = type->next) {
      a_type_ptr corresp_type =
                          (a_type_ptr)checked_trans_unit_copy_address_of(type);
      if (is_immediate_class_type(type) &&
          type->variant.class_struct_union.extra_info != NULL &&
          type->variant.class_struct_union.extra_info->assoc_scope != NULL) {
        /* A class with a scope.  Do a recursive call to process it. */
        a_scope_ptr class_scope = type->variant.class_struct_union.
                                                       extra_info->assoc_scope;
        check_assertion(!class_scope->is_placeholder_scope);
        finish_trans_unit_copy(class_scope);
      }  /* if */
      if (!entry_to_be_merged(type)) {
        /* The entry was copied. */
        add_to_list = scope_being_merged;
      } else {
        /* The entry gets merged into the corresponding type. */
        a_type_ptr primary_type =
                  (a_type_ptr)checked_trans_unit_copy_address_of(corresp_type);
#if DEBUG
/* GCOVR_EXCL_START */
        if (db_trace("trans_copy", corresp_type, iek_type)) {
          fprintf(f_debug,
                  "finish_trans_unit_copy, merging into %p after copy:\n",
                  (void *)primary_type);
          db_entity_info((char *)corresp_type, iek_type);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        add_to_list = FALSE;
        if (!entry_should_overwrite_primary_entry(type)) {
          /* No overwriting is needed, so we're done.  This happens,
             for example, when the only reason the class is marked to be
             merged is that some of its members need to be merged. */
          transfer_type_details(corresp_type, primary_type);
          merge_entity_details(&corresp_type->source_corresp,
                               &primary_type->source_corresp);
        } else {
          /* Copy this type and its definition, overwriting the
             existing primary type.  Move the primary IL type
             to the end of the types list so that it appears on
             the list at the point where the definition appears.
             Class members are not moved to the end of the list.
             Also do not move to the end of the list when the
             type being moved is a declaration (that can
             happen when a definition in a secondary translation
             unit is chosen as the canonical entry, and then its
             definition is not needed anywhere and is removed by
             the unneeded-entity removal processing). */
          move_to_end = (!is_class_scope &&
                         (is_immediate_class_type(corresp_type)
                                 ? class_type_has_body(corresp_type) &&
                                   !corresp_type->variant.class_struct_union.
                                                               is_nonreal_class
                                 : !is_incomplete_type(corresp_type)));
          if (move_to_end) {
            move_to_end_of_types_list(primary_type, NO_SCOPE_DEPTH);
            last_type = primary_type;
            check_assertion(pointers_block != NULL &&
                            pointers_block->last_type == last_type);
          }  /* if */
          overwrite_primary_type(corresp_type, primary_type);
          corresp_type = primary_type;
        }  /* if */
      }  /* if */
      if (add_to_list) {
        /* Add the type to the end of the list. */
#if DEBUG
/* GCOVR_EXCL_START */
        if (db_trace("trans_copy", corresp_type, iek_type)) {
          fprintf(f_debug,
                  "finish_trans_unit_copy, adding to list after copy:\n");
          db_entity_info((char *)corresp_type, iek_type);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        if (is_class_scope && last_type == NULL) {
          /* Determine the last type the first time it is needed. */
          last_type = primary_scope->types;
          if (last_type != NULL) {
            while (last_type->next != NULL) last_type = last_type->next;
          }  /* if */
        }  /* if */
        /* Add the type to the end of the list. */
        if (last_type == NULL) {
          primary_scope->types = corresp_type;
        } else {
          last_type->next = corresp_type;
        }  /* if */
        corresp_type->next = NULL;
        last_type = corresp_type;
        if (pointers_block != NULL) pointers_block->last_type = last_type;
      }  /* if */
    }  /* for */
  }  /* if */
  if (is_class_scope) {
    a_type_ptr class_type = scope->variant.assoc_type;

    /* Go through the fields and merge initializers and attributes. */
    a_field_ptr field;
    for (field = class_type->variant.class_struct_union.field_list;
         field != NULL;
         field = field->next) {
      if (entry_to_be_merged(field)) {
        a_field_ptr corresp_field =
                        (a_field_ptr)checked_trans_unit_copy_address_of(field);
        a_field_ptr primary_field =
                (a_field_ptr)checked_trans_unit_copy_address_of(corresp_field);
        if (primary_field->initializer == NULL &&
            corresp_field->initializer != NULL) {
          primary_field->initializer = corresp_field->initializer;
        }  /* if */
        merge_entity_details(&corresp_field->source_corresp,
                             &primary_field->source_corresp);
      }  /* if */
    }  /* for */
  }  /* if */
  if (scope->variables != NULL) {
    a_variable_ptr variable, last_variable;
    /* Merge the variables in the scope into the primary IL scope. */
    /* Get a pointer to the last variable in the primary scope. */
    if (pointers_block != NULL) {
      last_variable = pointers_block->last_variable;
    } else {
      /* Actual last variable will be determined below if/when needed. */
      last_variable = NULL;
    }  /* if */
    for (variable = scope->variables;
         variable != NULL;
         variable = variable->next) {
      a_variable_ptr corresp_variable =
                  (a_variable_ptr)checked_trans_unit_copy_address_of(variable);
      if (!entry_to_be_merged(variable)) {
        /* The entry was copied. */
        add_to_list = scope_being_merged;
      } else {
        /* The entry gets merged into the corresponding variable. */
        a_variable_ptr primary_variable =
                     (a_variable_ptr)checked_trans_unit_copy_address_of(
                                                             corresp_variable);
#if DEBUG
/* GCOVR_EXCL_START */
        if (db_trace("trans_copy", corresp_variable, iek_variable)) {
          fprintf(f_debug,
                  "finish_trans_unit_copy, merging into %p after copy:\n",
                  (void *)primary_variable);
          db_entity_info((char *)corresp_variable, iek_variable);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        add_to_list = FALSE;
        if (!entry_should_overwrite_primary_entry(variable)) {
          /* No overwriting is needed, so we're done. */
          transfer_variable_flags(corresp_variable, primary_variable);
          merge_entity_details(&corresp_variable->source_corresp,
                               &primary_variable->source_corresp);
        } else {
          /* Copy this variable and its definition, overwriting the
             existing primary variable.  Move the primary IL variable
             to the end of the variables list so that it appears on
             the list at the point where the definition appears.
             Class members are not moved to the end of the list.
             Also do not move if a specialization declaration replaces
             an unspecialized variable (with or without a definition). */
          move_to_end = (!is_class_scope &&
                         corresp_variable->storage_class ==
                                              (a_storage_class)sc_unspecified);
          if (move_to_end) {
            remove_from_variables_list(primary_variable, NO_SCOPE_DEPTH);
            /* coverity[var_deref_op] */
            last_variable = pointers_block->last_variable;
            add_to_list = TRUE;
          }  /* if */
          if (primary_variable->storage_class ==
                                             (a_storage_class)sc_unspecified) {
            /* Eliminate the definition of the primary variable (this happens
               when the secondary has a specialization and the primary does
               not). */
            clear_variable_definition(primary_variable);
          }  /* if */
#if MAINTAIN_NEEDED_FLAGS
          /* Eliminate any default argument object lifetimes associated with
             the entry that is about to be overwritten. */
          eliminate_variable_default_arg_object_lifetimes(primary_variable);
#endif /* MAINTAIN_NEEDED_FLAGS */
          overwrite_primary_variable(corresp_variable, primary_variable);
          corresp_variable = primary_variable;
        }  /* if */
      }  /* if */
      if (add_to_list) {
        /* Add the variable to the end of the list. */
        if (is_class_scope && last_variable == NULL) {
          /* Determine the last variable the first time it is needed. */
          last_variable = primary_scope->variables;
          if (last_variable != NULL) {
            while (last_variable->next != NULL) {
              last_variable = last_variable->next;
            }  /* while */
          }  /* if */
        }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
        if (db_trace("trans_copy", corresp_variable, iek_variable)) {
          fprintf(f_debug,
                  "finish_trans_unit_copy, adding to list after copy:\n");
          db_entity_info((char *)corresp_variable, iek_variable);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        /* Add the variable to the end of the list. */
        if (last_variable == NULL) {
          primary_scope->variables = corresp_variable;
        } else {
          last_variable->next = corresp_variable;
        }  /* if */
        corresp_variable->next = NULL;
        last_variable = corresp_variable;
        if (pointers_block != NULL) {
          pointers_block->last_variable = last_variable;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (scope->dynamic_inits != NULL && scope_being_merged) {
    /* Add the dynamic initializations of "scope" to the end of the
       dynamic initializations list of "primary scope". */
    a_dynamic_init_ptr copied_inits =
          (a_dynamic_init_ptr)transitive_copy_address_of(scope->dynamic_inits);
    a_dynamic_init_ptr last_dyn_init = primary_scope->dynamic_inits;
    if (last_dyn_init == NULL) {
      primary_scope->dynamic_inits = copied_inits;
    } else {
      /* coverity[var_deref_op] */
      last_dyn_init = pointers_block->last_dynamic_init;
      last_dyn_init->next = copied_inits;
    }  /* if */
    last_dyn_init = copied_inits;
    while (last_dyn_init->next != NULL) {
      last_dyn_init = last_dyn_init->next;
    }  /* while */
    check_assertion(pointers_block != NULL);  /* No dynamic init list
                                                 in classes. */
    pointers_block->last_dynamic_init = last_dyn_init;
  }  /* if */
  if (scope->routines != NULL) {
    a_routine_ptr routine, last_routine;
    /* Merge the routines in the scope into the primary IL scope. */
    /* Get a pointer to the last routine in the primary scope. */
    if (pointers_block != NULL) {
      last_routine = pointers_block->last_routine;
    } else {
      /* Actual last routine will be determined below if/when needed. */
      last_routine = NULL;
    }  /* if */
    for (routine = scope->routines;
         routine != NULL;
         routine = routine->next) {
      a_routine_ptr corresp_routine =
                    (a_routine_ptr)checked_trans_unit_copy_address_of(routine);
      if (!entry_to_be_merged(routine)) {
        /* The entry was copied. */
        add_to_list = scope_being_merged;
      } else {
        /* The entry gets merged into the corresponding routine. */
        a_routine_ptr primary_routine =
                   (a_routine_ptr)checked_trans_unit_copy_address_of(
                                                              corresp_routine);
#if DEBUG
/* GCOVR_EXCL_START */
        if (db_trace("trans_copy", corresp_routine, iek_routine)) {
          fprintf(f_debug,
                  "finish_trans_unit_copy, merging into %p after copy:\n",
                  (void *)primary_routine);
          db_entity_info((char *)corresp_routine, iek_routine);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        add_to_list = FALSE;
        if (!entry_should_overwrite_primary_entry(routine)) {
          /* No overwriting is needed, so we're done. */
          transfer_routine_flags(corresp_routine, primary_routine);
          merge_entity_details(&corresp_routine->source_corresp,
                               &primary_routine->source_corresp);
        } else {
          /* Copy this routine and its definition, overwriting the
             existing primary routine.  Move the primary IL routine
             to the end of the routines list so that it appears on
             the list at the point where the definition appears.
             Class members are not moved to the end of the list.
             Also do not move if a specialization declaration replaces
             an unspecialized routine (with or without a definition). */
          move_to_end = (!is_class_scope &&
                         corresp_routine->function_def_number !=
                                                     NULL_function_def_number);
          if (move_to_end) {
            remove_from_routines_list(primary_routine, NO_SCOPE_DEPTH);
            last_routine = pointers_block->last_routine;
            add_to_list = TRUE;
          }  /* if */
          if (primary_routine->function_def_number !=
                                                    NULL_function_def_number) {
            /* Eliminate the body of the primary routine (this happens when
               the secondary has a specialization and the primary does not). */
            clear_body_for_routine(primary_routine);
          }  /* if */
#if MAINTAIN_NEEDED_FLAGS
          /* Eliminate any default argument object lifetimes associated with
             the entry that is about to be overwritten. */
          eliminate_routine_default_arg_object_lifetimes(primary_routine);
#endif /* MAINTAIN_NEEDED_FLAGS */
          overwrite_primary_routine(corresp_routine, primary_routine);
          corresp_routine = primary_routine;
        }  /* if */
      }  /* if */
      if (add_to_list) {
        /* Add the routine to the end of the list. */
        if (is_class_scope && last_routine == NULL) {
          /* Determine the last routine the first time it is needed. */
          last_routine = primary_scope->routines;
          if (last_routine != NULL) {
            while (last_routine->next != NULL) {
              last_routine = last_routine->next;
            }  /* while */
          }  /* if */
        }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
        if (db_trace("trans_copy", corresp_routine, iek_routine)) {
          fprintf(f_debug,
                  "finish_trans_unit_copy, adding to list after copy:\n");
          db_entity_info((char *)corresp_routine, iek_routine);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        /* Add the routine to the end of the list. */
        if (last_routine == NULL) {
          primary_scope->routines = corresp_routine;
        } else {
          last_routine->next = corresp_routine;
        }  /* if */
        corresp_routine->next = NULL;
        last_routine = corresp_routine;
        if (pointers_block != NULL) {
          pointers_block->last_routine = last_routine;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (scope->templates != NULL) {
    a_template_ptr templ, last_templ;
    /* Merge the templates in the scope into the primary IL scope. */
    /* Get a pointer to the last template in the primary scope. */
    if (pointers_block != NULL) {
      last_templ = pointers_block->last_template;
    } else {
      /* Actual last template will be determined below if/when needed. */
      last_templ = NULL;
    }  /* if */
    for (templ = scope->templates; templ != NULL; templ = templ->next) {
      a_template_ptr corresp_templ =
                     (a_template_ptr)checked_trans_unit_copy_address_of(templ);
      check_assertion(!entry_to_be_merged(templ));
      if (scope_being_merged) {
        /* Add the template to the end of the list. */
        if (is_class_scope && last_templ == NULL) {
          /* Determine the last template the first time it is needed. */
          last_templ = primary_scope->templates;
          if (last_templ != NULL) {
            while (last_templ->next != NULL) last_templ = last_templ->next;
          }  /* if */
        }  /* if */
        if (last_templ == NULL) {
          primary_scope->templates = corresp_templ;
        } else {
          last_templ->next = corresp_templ;
        }  /* if */
        corresp_templ->next = NULL;
        last_templ = corresp_templ;
        if (pointers_block != NULL) pointers_block->last_template = last_templ;
      }  /* if */
    }  /* for */
  }  /* if */
  if (scope->namespaces != NULL) {
    a_namespace_ptr nsp, last_nsp;
    /* Merge the namespaces in the scope into the primary IL scope. */
    check_assertion(pointers_block != NULL);  /* No namespaces in classes. */
    last_nsp = pointers_block->last_namespace;
    for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
      a_namespace_ptr corresp_nsp =
                      (a_namespace_ptr)checked_trans_unit_copy_address_of(nsp);
      if (!entry_to_be_merged(nsp) && scope_being_merged) {
        /* Add the namespace to the end of the list. */
        if (last_nsp == NULL) {
          primary_scope->namespaces = corresp_nsp;
        } else {
          last_nsp->next = corresp_nsp;
        }  /* if */
        corresp_nsp->next = NULL;
        last_nsp = corresp_nsp;
        pointers_block->last_namespace = last_nsp;
      }  /* if */
      if (!nsp->is_namespace_alias) {
        finish_trans_unit_copy(nsp->variant.assoc_scope);
      }  /* if */
    }  /* for */
  }  /* if */
  if (scope->pragmas != NULL && scope_being_merged) {
    a_pragma_ptr pragma, last_pragma;
    /* Merge the pragmas in the scope into the primary IL scope. */
    /* Get a pointer to the last pragma in the primary scope. */
    if (pointers_block != NULL) {
      last_pragma = pointers_block->last_pragma;
    } else {
      /* Actual last pragma will be determined below if/when needed. */
      last_pragma = NULL;
    }  /* if */
    for (pragma = scope->pragmas; pragma != NULL; pragma = pragma->next) {
      a_pragma_ptr corresp_pragma =
                      (a_pragma_ptr)checked_trans_unit_copy_address_of(pragma);
      if (is_class_scope && last_pragma == NULL) {
        /* Determine the last pragma the first time it is needed. */
        last_pragma = primary_scope->pragmas;
        if (last_pragma != NULL) {
          while (last_pragma->next != NULL) last_pragma = last_pragma->next;
        }  /* if */
      }  /* if */
      /* Add the pragma to the end of the list. */
      if (last_pragma == NULL) {
        primary_scope->pragmas = corresp_pragma;
      } else {
        last_pragma->next = corresp_pragma;
      }  /* if */
      corresp_pragma->next = NULL;
      last_pragma = corresp_pragma;
      if (pointers_block != NULL) pointers_block->last_pragma = last_pragma;
    }  /* for */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (scope->ms_attributes != NULL && scope_being_merged) {
    an_ms_attribute_ptr ms_attr, last_ms_attr;
    /* Merge the Microsoft attributes in the scope into the primary IL
       scope. */
    /* Get a pointer to the last attribute in the primary scope. */
    if (pointers_block != NULL) {
      last_ms_attr = pointers_block->last_ms_attribute;
    } else {
      /* Actual last attribute will be determined below if/when needed. */
      last_ms_attr = NULL;
    }  /* if */
    for (ms_attr = scope->ms_attributes;
         ms_attr != NULL;
         ms_attr = ms_attr->next) {
      an_ms_attribute_ptr corresp_ms_attr =
              (an_ms_attribute_ptr)checked_trans_unit_copy_address_of(ms_attr);
      if (is_class_scope && last_ms_attr == NULL) {
        /* Determine the last attribute the first time it is needed. */
        last_ms_attr = primary_scope->ms_attributes;
        if (last_ms_attr != NULL) {
          while (last_ms_attr->next != NULL) last_ms_attr = last_ms_attr->next;
        }  /* if */
      }  /* if */
      /* Add the attribute to the end of the list. */
      if (last_ms_attr == NULL) {
        primary_scope->ms_attributes = corresp_ms_attr;
      } else {
        last_ms_attr->next = corresp_ms_attr;
      }  /* if */
      corresp_ms_attr->next = NULL;
      last_ms_attr = corresp_ms_attr;
      if (pointers_block != NULL) {
        pointers_block->last_ms_attribute = last_ms_attr;
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (scope->asm_entries != NULL && scope_being_merged) {
    an_asm_entry_ptr asm_entry, last_asm_entry;
    /* Merge the asm entries in the scope into the primary IL scope. */
    /* Get a pointer to the last asm entry in the primary scope. */
    if (pointers_block != NULL) {
      last_asm_entry = pointers_block->last_asm_entry;
    } else {
      /* Actual last asm entry will be determined below if/when needed. */
      last_asm_entry = NULL;
    }  /* if */
    for (asm_entry = scope->asm_entries;
         asm_entry != NULL;
         asm_entry = asm_entry->next) {
      an_asm_entry_ptr corresp_asm_entry =
               (an_asm_entry_ptr)checked_trans_unit_copy_address_of(asm_entry);
      if (is_class_scope && last_asm_entry == NULL) {
        /* Determine the last asm entry the first time it is needed. */
        last_asm_entry = primary_scope->asm_entries;
        if (last_asm_entry != NULL) {
          while (last_asm_entry->next != NULL) {
            last_asm_entry = last_asm_entry->next;
          }  /* while */
        }  /* if */
      }  /* if */
      /* Add the asm entry to the end of the list. */
      if (last_asm_entry == NULL) {
        primary_scope->asm_entries = corresp_asm_entry;
      } else {
        last_asm_entry->next = corresp_asm_entry;
      }  /* if */
      corresp_asm_entry->next = NULL;
      last_asm_entry = corresp_asm_entry;
      if (pointers_block != NULL) {
        pointers_block->last_asm_entry = last_asm_entry;
      }  /* if */
    }  /* for */
  }  /* if */
  if (scope_being_merged) {
    /* Merge the object lifetime from "scope" into that from
       "primary_scope". */
    merge_object_lifetimes(scope, primary_scope);
  }  /* if */
}  /* finish_trans_unit_copy */


static void merge_il_headers(a_translation_unit_ptr tup)
/*
Do merging of the il_header of the indicated secondary translation unit
into the primary translation unit il_header.
*/
{
  check_assertion(is_primary_translation_unit);
  if (tup->il_header.main_routine != NULL) {
    /* "main" is defined in the secondary translation unit.  Indicate
       that it is now defined in the primary translation unit. */
    a_routine_ptr primary_main =
        (a_routine_ptr)primary_il_entry_of((char *)tup->il_header.main_routine,
                                           iek_routine);
    check_assertion(il_header.main_routine == NULL ||
                    il_header.main_routine == primary_main);
    il_header.main_routine = primary_main;
  }  /* if */
  /* Merge some of the IL header flags. */
  if (tup->il_header.UCN_identifiers_used) {
    il_header.UCN_identifiers_used = TRUE;
  }  /* if */
  if (tup->il_header.vla_used) il_header.vla_used = TRUE;
  if (tup->il_header.any_templates_seen) il_header.any_templates_seen = TRUE;
  /* Add copied types from the nontag_types_used_in_exception_or_rtti list
     to the corresponding primary IL list. */
  { a_type_ptr eh_type, last_primary_eh_type;
    /* Find the last entry on the primary IL list. */
    last_primary_eh_type = il_header.nontag_types_used_in_exception_or_rtti;
    if (last_primary_eh_type != NULL) {
      while (last_primary_eh_type->next != NULL) {
        last_primary_eh_type = last_primary_eh_type->next;
      }  /* while */
    }  /* if */
    for (eh_type = tup->il_header.nontag_types_used_in_exception_or_rtti;
         eh_type != NULL;
         eh_type = eh_type->next) {
      a_boolean add_to_list;
      a_type_ptr corresp_eh_type =
                       (a_type_ptr)checked_trans_unit_copy_address_of(eh_type);
      if (!entry_to_be_merged(eh_type)) {
        /* Entries that were really copied get added to the list. */
        add_to_list = TRUE;
      } else {
        /* Entities for which an instance already existed in the primary IL
           are added to the list only if they are not already on the list. */
        a_type_ptr primary_eh_type = 
                   (a_type_ptr)checked_trans_unit_copy_address_of(
                                                              corresp_eh_type);
        add_to_list = (primary_eh_type->next == NULL &&
                       primary_eh_type != last_primary_eh_type);
        corresp_eh_type = primary_eh_type;
      }  /* if */
      if (add_to_list) {
        if (last_primary_eh_type == NULL) {
          il_header.nontag_types_used_in_exception_or_rtti = corresp_eh_type;
        } else {
          last_primary_eh_type->next = corresp_eh_type;
        }  /* if */
        last_primary_eh_type = corresp_eh_type;
        corresp_eh_type->next = NULL;
        corresp_eh_type->used_in_exception_or_rtti = TRUE;
      }  /* if */
    }  /* for */
  }
}  /* merge_il_headers */


static void ensure_routine_is_on_inline_list(ARG_UNUSED a_routine_ptr routine)
/*
The indicated routine has been copied or merged from the secondary
translation unit IL to the primary IL.  routine points to the copy in the
secondary translation unit.  Update the "instantiation" lists for extern
inline functions, if appropriate.
*/
{
  /* This routine runs while switched to the primary translation unit. */
  check_assertion(is_primary_translation_unit &&
                  in_secondary_trans_unit(routine));
#if INSTANTIATE_EXTERN_INLINE
  if (instantiate_extern_inline) {
    a_routine_ptr primary_routine =
                            (a_routine_ptr)transitive_copy_address_of(routine);
    if (treat_as_extern_inline(primary_routine) && !routine->is_consteval &&
        !routine->is_prototype_instantiation) {
      if (primary_routine->on_inline_function_list) {
        /* There is already a list entry for the routine in the primary IL. */
#if DEBUG
/* GCOVR_EXCL_START */
        if (db_trace("trans_copy", primary_routine, iek_routine)) {
          fprintf(f_debug,
                  "ensure_routine_is_on_inline_list: already on list:\n");
          db_entity_info((char *)primary_routine, iek_routine);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      } else {
        /* Add an entry for the routine. */
        add_to_inline_function_list(primary_routine);
#if DEBUG
/* GCOVR_EXCL_START */
        if (db_trace("trans_copy", primary_routine, iek_routine)) {
          fprintf(f_debug, "ensure_routine_is_on_inline_list: adding:\n");
          db_entity_info((char *)primary_routine, iek_routine);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if MAINTAIN_NEEDED_FLAGS
        /* If the suppress_inline_body flag indicates that this function
           is assigned to this compilation and that was noted in a secondary
           translation unit, mark the function as needed in the primary
           translation unit. */
        if (!primary_routine->suppress_inline_body) {
          mark_as_needed((char *)primary_routine, iek_routine);
        }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* INSTANTIATE_EXTERN_INLINE */
}  /* ensure_routine_is_on_inline_list */


static void finish_scope_moved_entity_processing(a_scope_ptr scope);


static void finish_type_list_moved_entity_processing(a_type_ptr type_list)
/*
Finish processing in the indicated type list and its subscopes for any
entities that were moved or merged from the secondary translation unit IL
to the primary IL.
*/
{
  a_type_ptr type;

  if (!C_mode()) {
    /* Look for class types and process their member functions. */
    for (type = type_list; type != NULL; type = type->next) {
      if (is_immediate_class_type(type)) {
        a_scope_ptr class_scope =
                      type->variant.class_struct_union.extra_info->assoc_scope;
        if (class_scope != NULL) {
          check_assertion(!class_scope->is_placeholder_scope);
          finish_scope_moved_entity_processing(class_scope);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* finish_type_list_moved_entity_processing */


static void finish_scope_moved_entity_processing(a_scope_ptr scope)
/*
Finish processing in the indicated scope and its subscopes for any
entities that were moved or merged from the secondary translation unit IL
to the primary IL.  The scope passed in is from the secondary translation
unit.
*/
{
  a_routine_ptr   routine;
  a_namespace_ptr nsp;
  a_scope_ptr     sub_scope;

  check_assertion(in_secondary_trans_unit(scope));
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      finish_scope_moved_entity_processing(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  finish_type_list_moved_entity_processing(scope->types);
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    ensure_routine_is_on_inline_list(routine);
  }  /* for */
  for (sub_scope = scope->scopes;
       sub_scope != NULL;
       sub_scope = sub_scope->next) {
    finish_scope_moved_entity_processing(sub_scope);
  }  /* for */
#if ONE_INSTANTIATION_PER_OBJECT || MAINTAIN_NEEDED_FLAGS
  { a_variable_ptr  variable;
    for (variable = scope->variables;
         variable != NULL;
         variable = variable->next) {
      a_variable_ptr primary_variable;

      if (in_secondary_trans_unit(variable)) {
        primary_variable= (a_variable_ptr)transitive_copy_address_of(variable);
      } else {
        check_assertion(variable->source_corresp.is_local_to_function);
        primary_variable = variable;
      }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
      if (one_instantiation_per_object &&
          scope->kind == (a_scope_kind)sck_class_struct_union) {
        /* Assign one-instantiation-per-object needed bit numbers to
           static data members. */
        set_variable_instantiation_needed_bit_number(primary_variable);
      }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if MAINTAIN_NEEDED_FLAGS
      if (primary_variable->storage_class == (a_storage_class)sc_unspecified ||
          primary_variable->init_kind == (an_init_kind)initk_dynamic) {
        /* Mark an externally-defined variable or one with initialization
           side effects as "needed". */
        mark_as_needed((char *)primary_variable,
                       (an_il_entry_kind)iek_variable);
      }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
    }  /* for */
  }
#endif /* ONE_INSTANTIATION_PER_OBJECT || MAINTAIN_NEEDED_FLAGS */
}  /* finish_scope_moved_entity_processing */


static void finish_moved_entity_processing(a_translation_unit_ptr tup)
/*
Finish processing in the indicated secondary translation unit for any
entities that were moved or merged from the secondary translation unit IL
to the primary IL.  The processing done here differs from that done in
finish_trans_unit_copy in that all entities, even those that are members
of non-merged scopes (e.g., local classes) are processed here.  If some
processing needs to be done on every entity moved or merged from the
secondary IL, it must be done here.
*/
{
  a_scope_orphaned_list_header_ptr solhp;

  /* Process the file scope and its subscopes. */
  finish_scope_moved_entity_processing(tup->primary_scope);
  /* Visit orphan lists to get local types. */
  for (solhp = tup->il_header.scope_orphaned_list_headers;
       solhp != NULL;
       solhp = solhp->next) {
    finish_type_list_moved_entity_processing(solhp->orphaned_types);
  }  /* for */
}  /* finish_moved_entity_processing */


static void finish_scope_orphaned_list_processing(
                                    a_scope_orphaned_list_header_ptr solh_list)
/*
Final processing on scope orphaned list headers.  For functions that
were actually copied over, a new version of the orphaned list header
entries will be generated on the other side (after lowering).  For functions
whose bodies were deleted, however, there may be dangling types etc.
in the scope orphaned list headers in the secondary translation unit.
The entries have been copied over, but they're not linked into the IL
tree.  Link them in now if appropriate.  solh_list points to the list
of scope orphaned list headers from the secondary translation unit.
The current translation unit is the primary translation unit.
*/
{
  a_scope_orphaned_list_header_ptr solhp;

  check_assertion(solh_list == NULL || in_secondary_trans_unit(solh_list));
  check_assertion(is_primary_translation_unit);
  for (solhp = solh_list; solhp != NULL; solhp = solhp->next) {
    if (solhp->assoc_routine->function_def_number ==
                                                    NULL_function_def_number) {
      /* The routine associated with this entry was deleted, so link the
         copy of this entry into the scope orphaned headers list in the
         primary IL. */
      a_scope_orphaned_list_header_ptr corresp_solhp =
                               (a_scope_orphaned_list_header_ptr)
                                     checked_trans_unit_copy_address_of(solhp);
      if (il_header.scope_orphaned_list_headers == NULL) {
        il_header.scope_orphaned_list_headers = corresp_solhp;
      } else {
        curr_translation_unit->last_scope_orphaned_list_header->next =
                                                                 corresp_solhp;
      }  /* if */
      corresp_solhp->next = NULL;
      curr_translation_unit->last_scope_orphaned_list_header = corresp_solhp;
    }  /* if */
  }  /* for */
}  /* finish_scope_orphaned_list_processing */


static void rebuild_scope_befriending_lists(a_scope_ptr scope);


static void rebuild_type_list_befriending_lists(a_type_ptr type_list)
/*
Rebuild the befriending lists for classes on the indicated list of types.
*/
{
  a_type_ptr type;

  for (type = type_list; type != NULL; type = type->next) {
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      a_class_list_entry_ptr      befriending_clep;
      an_il_entity_list_entry_ptr ielep = ctsp->friends;
      for (; ielep != NULL; ielep = ielep->next) {
        if (ielep->entity.kind == iek_type) {
          a_type_ptr friend_class = (a_type_ptr)ielep->entity.ptr;

          /* The only expected types are class types. */
          check_assertion(is_class_struct_union_type(friend_class));

          a_class_type_supplement_ptr friend_ctsp =
                           friend_class->variant.class_struct_union.extra_info;
          befriending_clep = alloc_list_entry_for_class();
          befriending_clep->class_type = type;
          befriending_clep->next = friend_ctsp->befriending_classes;
          friend_ctsp->befriending_classes = befriending_clep;
        } else if (ielep->entity.kind == iek_routine) {
          a_routine_ptr friend_routine = (a_routine_ptr)ielep->entity.ptr;
          check_assertion(!friend_routine->is_inheriting_ctor);
          befriending_clep = alloc_list_entry_for_class();
          befriending_clep->class_type = type;
          befriending_clep->next =
                     friend_routine->friends_or_originator.befriending_classes;
          friend_routine->friends_or_originator.befriending_classes =
                                                              befriending_clep;
        } else if (ielep->entity.kind == iek_template) {
          a_template_ptr friend_template = (a_template_ptr)ielep->entity.ptr;
          a_template_symbol_supplement_ptr
                         friend_tssp =
                             template_supplement_for_template(friend_template);
          befriending_clep = alloc_list_entry_for_class();
          befriending_clep->class_type = type;
          befriending_clep->next = friend_tssp->befriending_classes;
          friend_tssp->befriending_classes = befriending_clep;
        } else {
          unexpected_condition_str2("rebuild_type_list_befriending_lists:",
                                    "unexpected entity kind");
        }  /* if */
      }  /* for */
      if (ctsp->assoc_scope != NULL) {
        rebuild_scope_befriending_lists(ctsp->assoc_scope);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* rebuild_type_list_befriending_lists */


static void rebuild_scope_befriending_lists(a_scope_ptr scope)
/*
Visit the classes and routines in the indicated scope and its subscopes,
and rebuild the befriending lists.  They have previously been cleared.
*/
{
  a_namespace_ptr nsp;

  rebuild_type_list_befriending_lists(scope->types);
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      rebuild_scope_befriending_lists(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  if (scope->kind == (a_scope_kind)sck_file) {
    /* Visit orphan lists to get local types. */
    a_scope_orphaned_list_header_ptr solhp;
    for (solhp = il_header.scope_orphaned_list_headers;
         solhp != NULL;
         solhp = solhp->next) {
      rebuild_type_list_befriending_lists(solhp->orphaned_types);
    }  /* for */
  }  /* if */
}  /* rebuild_scope_befriending_lists */


void copy_secondary_trans_unit_IL_to_primary(void)
/*
Copy IL from any secondary translation units to the primary translation
unit IL.  If needed flag processing is configured in, unneeded entities
have already been removed from the secondary translation unit IL and
therefore will not be copied.
*/
{
  a_translation_unit_ptr tup;
  a_scope_ptr            top_scope;

  db_enter(1, "copy_secondary_trans_unit_IL_to_primary");
  check_assertion(!is_at_least_one_error() && !trans_unit_test_mode);
#if CHECKING
  /* This code doesn't handle source sequence lists, so the result won't
     work with the C++-generating back end. */
  { a_boolean okay = /*lint -e(506)*/!BACK_END_IS_CP_GEN_BE;
    check_assertion(okay);
  }
#endif /* CHECKING */
  check_assertion(initial_value_for_il_lowering_flag == FALSE);/*lint !e527*/
  in_trans_copy_setup = TRUE;
  in_primary_il_reference_rewrite = FALSE;
  /* Loop over each translation unit, preparing for the copy.  This
     decides which entities should be copied, which should be merged,
     and which are duplicates that can be dropped. */
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
    a_boolean   any_removed_function_bodies = FALSE;
    switch_translation_unit(tup);
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 1) {
      fprintf(f_debug, "Preparing copy from sec trans unit %s:\n",
              curr_translation_unit->source_file->name_as_written);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    top_scope = il_header.primary_scope;
    check_assertion(!il_entry_prefix_of(top_scope).il_lowering_flag);
    (void)prepare_for_trans_unit_copy(top_scope, &any_removed_function_bodies);
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 1) {
      fprintf(f_debug,
              "Done preparing copy from sec trans unit %s\n",
              curr_translation_unit->source_file->name_as_written);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* for */
  in_trans_copy_setup = FALSE;
  /* Loop over each translation unit, copying the IL. */
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 1) {
      fprintf(f_debug, "Copying from sec trans unit %s:\n",
              curr_translation_unit->source_file->name_as_written);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    top_scope = il_header.primary_scope;
    copy_from_secondary_to_primary_IL();
    /* Update "last" pointers in any namespaces copied. */
    update_namespace_last_pointers(top_scope);
    /* Also remap pointers in bodies of functions. */
    copy_function_bodies_from_secondary_to_primary_IL(top_scope);
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 1) {
      fprintf(f_debug,
              "Done copying from sec trans unit %s\n",
              curr_translation_unit->source_file->name_as_written);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* for */
  switch_translation_unit(translation_units);
  /* Loop over each translation unit, linking copied entities into the
     primary IL.  Note that this part runs while switched to the
     primary translation unit. */
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 1) {
      fprintf(f_debug, "Wrapping up copy from sec trans unit %s:\n",
                       curr_translation_unit->source_file->name_as_written);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    top_scope = tup->primary_scope;
    finish_trans_unit_copy(top_scope);
    merge_il_headers(tup);
    finish_moved_entity_processing(tup);
    finish_scope_orphaned_list_processing(
                                   tup->il_header.scope_orphaned_list_headers);
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 1) {
      fprintf(f_debug, "Done wrapping up copy from sec trans unit %s:\n",
                       curr_translation_unit->source_file->name_as_written);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* for */
  if (!C_mode()) {
    /* Sweep the primary translation unit IL tree and look for any
       pointers to entities in secondary translation units that it uses,
       and rewrite the pointers as the corresponding primary IL entities. */
    in_primary_il_reference_rewrite = TRUE;
    rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary();
    in_primary_il_reference_rewrite = FALSE;
    /* Rebuild the befriending lists. */
    rebuild_scope_befriending_lists(il_header.primary_scope);
  }  /* if */
  /* Update the tables of fundamental types kept for the primary translation
     unit. */
  if (secondary_translation_unit_seen()) {
    record_fundamental_types_copied_from_secondary_IL();
  }  /* if */
  db_exit();
}  /* copy_secondary_trans_unit_IL_to_primary */


static a_boolean mem_region_is_primary_func_scope(
                                                 a_memory_region_number number)
/*
Return TRUE if the indicated memory region is a function scope memory
region of the primary IL.
*/
{
  a_boolean result = FALSE;

  if (mem_region_table[number] == NULL) {
    /* This memory has already been freed. */
  } else {
    a_scope_ptr sp = il_header.region_scope_entry[number];
    if (!in_secondary_trans_unit(sp) &&
        sp->kind != (a_scope_kind)sck_file) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* mem_region_is_primary_func_scope */

#if MAINTAIN_NEEDED_FLAGS

static void mark_secondary_il_entry_as_needed(char             *ptr,
                                              an_il_entry_kind kind)
/*
Mark the indicated entry of the indicated kind as needed.  It is
an entry in the secondary translation unit IL and it is referenced
from a primary IL entry.
*/
{
  mark_as_needed(ptr, kind);
  /* If the entity is a class type with a definition, mark its definition
     as needed as well.  We don't actually know whether it is needed,
     so we have to assume it is.  Likewise for routines (often
     mark_as_needed will have taken care of that, but not in
     every case). */
  if (kind == iek_type) {
    a_type_ptr type = (a_type_ptr)ptr;
    if (is_immediate_class_type(type) &&
        class_type_has_body(type)) {
      set_class_keep_definition_in_il(type);
      set_class_definition_needed(type);
    }  /* if */
  } else if (kind == iek_routine) {
    a_routine_ptr routine = (a_routine_ptr)ptr;
    set_routine_keep_definition_in_il(routine);
    set_routine_definition_needed(routine);
  }  /* if */
}  /* mark_secondary_il_entry_as_needed */


static char *remap_secondary_pointer_for_mark(char             *ptr,
                                              an_il_entry_kind kind)
/*
Called as part of the IL walk for
mark_secondary_trans_unit_IL_entities_used_from_primary_as_needed.
Doesn't actually do any pointer remapping, but does mark entries
in secondary translation units as needed.  That is done here instead
of the termination-test routine because even pointers in remap_ptr
calls are passed to the remap routine.
*/
{
  if (ptr == NULL) {
    /* Ignore NULL pointers. */
  } else if (in_secondary_trans_unit(ptr)) {
    /* An entity in the secondary IL -- mark it as needed. */
    mark_secondary_il_entry_as_needed(ptr, kind);
  } else {
    /* An entity in the primary IL. */
    /* If the entry has linkage and its canonical entry is in a secondary
       translation unit, mark it as needed. */
    a_source_correspondence_ptr scp = source_corresp_for_il_entry(ptr, kind);
    if (scp != NULL && scp->trans_unit_corresp != NULL) {
      char *canonical = scp->trans_unit_corresp->canonical;
      if (in_secondary_trans_unit(canonical)) {
        mark_secondary_il_entry_as_needed(canonical, kind);
      }  /* if */
    }  /* if */
  }  /* if */
  return ptr;
}  /* remap_secondary_pointer_for_mark */


static a_boolean mark_secondary_termination_test(
                                              char                        *ptr,
                                              ARG_UNUSED an_il_entry_kind kind)
/*
Called during the IL walk for
mark_secondary_trans_unit_IL_entities_used_from_primary_as_needed
to do the termination test.
*/
{
  a_boolean prune;

  if (in_secondary_trans_unit(ptr)) {
    prune = TRUE;
  } else {
    an_il_entry_prefix_ptr prefix = &il_entry_prefix_of(ptr);

    if (prefix->il_walk_flag == flag_value_meaning_visited) {
      /* This entry has already been visited on this walk. */
      prune = TRUE;
    } else {
      /* This entry has not been visited previously. */
      prefix->il_walk_flag = flag_value_meaning_visited;
      prune = FALSE;
    }  /* if */
  }  /* if */
  return prune;
}  /* mark_secondary_termination_test */

#endif /* MAINTAIN_NEEDED_FLAGS */

void mark_secondary_trans_unit_IL_entities_used_from_primary_as_needed(void)
/*
Walk through the primary translation unit IL tree, looking for pointers
to entities in secondary translation unit IL.  Mark such secondary IL
entities as needed, so that they will be copied to the primary IL later.
(This is done before the copying of secondary translation unit IL to the
primary IL.)
*/
{
  db_enter(1,
          "mark_secondary_trans_unit_IL_entities_used_from_primary_as_needed");
#if MAINTAIN_NEEDED_FLAGS
  if (secondary_translation_unit_seen()) {
    a_memory_region_number n;
    a_boolean              first_pass = TRUE;
    /* Do two passes so that the il_walk_flag returns to its original value. */
    for (;;) {
      a_remap_function_ptr remap_func = NULL;
      if (first_pass) remap_func = remap_secondary_pointer_for_mark;
      walk_file_scope_il((an_entry_process_function_ptr)NULL,
                         (a_string_entry_process_function_ptr)NULL,
                         remap_func,
                         remap_func,
                         mark_secondary_termination_test,
                         /*clear_fe_pointers=*/FALSE);
      /* Loop through the memory regions looking for functions in the
         primary IL, and process them too. */
      for (n = FILE_SCOPE_REGION_NUMBER + 1;
           n <= highest_used_region_number;
           n++) {
        if (mem_region_is_primary_func_scope(n)) {
          walk_routine_scope_il(n,
                                (an_entry_process_function_ptr)NULL,
                                (a_string_entry_process_function_ptr)NULL,
                                remap_func,
                                remap_func,
                                mark_secondary_termination_test,
                                /*clear_fe_pointers=*/FALSE);
        }  /* if */
      }  /* for */
      if (!first_pass) break;
      first_pass = FALSE;
    }  /* for */
  }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
  db_exit();
}  /* mark_secondary_trans_unit_IL_entities_used_from_primary_as_needed */


static char *remap_secondary_pointer_for_rewrite(char             *old_ptr,
                                                 an_il_entry_kind kind)
/*
Called as part of the IL walk for 
rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary to
do the pointer remapping.  Remaps pointers to entities in secondary
translation units to pointers to the corresponding entities in the
primary IL.
*/
{
  char *new_ptr = old_ptr;

  if (old_ptr == NULL) {
    /* Leave a NULL pointer alone. */
  } else if (in_secondary_trans_unit(old_ptr)) {
    check_assertion_str(in_file_scope(old_ptr),
                     "remap_secondary_pointer_for_rewrite: not in file scope");
    if (trans_unit_copy_address_of(old_ptr) != NULL) {
      /* The entry already has a copy address assigned. */
      new_ptr = transitive_copy_address_of(old_ptr);
    } else {
      /* No copy address established.  For entities with linkage,
         we can get an address from the associated canonical entry.
         Other things we can copy.  However, we can't copy things
         that go on lists, because we won't get a chance to link
         them on the lists.  For example, the type "pointer to int"
         need not have a copy address here, but A<int> must. */
      /* Basically, primary_il_entry_of can do the copy, but we
         do some extra checking here to make sure that we're not
         copying things that should have been copied already. */
#if CHECKING
      /* Check whether the entity is okay. */
      { a_boolean                err = FALSE;
        a_trans_unit_corresp_ptr tucp;
        /* For entities with linkage, an address should have been
           assigned to the correspondence set, but it might not
           have been established in this entry.  This entry shouldn't
           be the canonical entry -- if it were, its address should
           have been assigned already. */
        if (source_corresp_for_il_entry(old_ptr, kind) != NULL &&
            (tucp = trans_unit_corresp_of_unknown_entry(old_ptr),
             tucp != NULL)) {
          check_assertion_str(tucp->canonical != old_ptr &&
                              (!in_secondary_trans_unit(tucp->canonical) ||
                               checked_trans_unit_copy_address_of(
                                                    tucp->canonical) != NULL),
      "remap_secondary_pointer_for_rewrite: canonical copy addr not assigned");
        } else {
          /* Entity does not have a linkage correspondence. */
          switch (kind) {
            case iek_constant:
              { a_constant_ptr con = (a_constant_ptr)old_ptr;
                if (has_name(con)) err = TRUE;
              }
              break;
            case iek_type:
              { a_type_ptr type = (a_type_ptr)old_ptr;
                if (has_name(type) ||
                    is_immediate_class_type(type) ||
                    (type->kind == (a_type_kind)tk_enum &&
                     type->variant.integer.enum_type)) err = TRUE;
              }
              break;
            case iek_expr_node:
            case iek_template_arg:
            case iek_name_reference:
            case iek_name_qualifier:
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
            case iek_gnu_routine_supplement:
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
              break;
            default:
              err = TRUE;
          }  /* switch */
          if (err) {
#if CHECKING && DEBUG
            fprintf(f_debug, "entry kind = %s\n", il_entry_kind_names[kind]);
#endif /* CHECKING && DEBUG */
            unexpected_condition_str(
     "remap_secondary_pointer_for_rewrite: missing primary IL correspondence");
          }  /* if */
        }  /* if */
      }
#endif /* CHECKING */
      new_ptr = primary_il_entry_of(old_ptr, kind);
    }  /* if */
  }  /* if */
  return new_ptr;
}  /* remap_secondary_pointer_for_rewrite */


static a_boolean rewrite_secondary_termination_test(
                                              char                        *ptr,
                                              ARG_UNUSED an_il_entry_kind kind)
/*
Called during the IL walk for
rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary to
do the termination test.
*/
{
  an_il_entry_prefix_ptr prefix = &il_entry_prefix_of(ptr);
  a_boolean              prune;

  /* There shouldn't be any secondary translation unit pointers left at
     this point -- the remap routine eliminates them. */
  check_assertion_str(!in_secondary_trans_unit(ptr),
         "rewrite_secondary_termination_test: remaining secondary IL pointer");
  if (prefix->il_walk_flag == flag_value_meaning_visited) {
    /* This entry has already been visited on this walk. */
    prune = TRUE;
  } else {
    prefix->il_walk_flag = flag_value_meaning_visited;
    prune = FALSE;
    /* Clear befriending lists, which will be rebuilt later. */
    if (kind == iek_class_type_supplement) {
      a_class_type_supplement_ptr ctsp = (a_class_type_supplement_ptr)ptr;
      ctsp->befriending_classes = NULL;
    } else if (kind == iek_routine) {
      a_routine_ptr routine = (a_routine_ptr)ptr;
      if (!routine->is_inheriting_ctor) {
        routine->friends_or_originator.befriending_classes = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  return prune;
}  /* rewrite_secondary_termination_test */


static
void rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary(void)
/*
Walk through the primary translation unit IL tree, looking for pointers
to entities in secondary translation unit IL.  Rewrite such pointers
as pointers to the corresponding primary IL entities.  This is done after
the copying of secondary translation unit IL to the primary IL, and
before lowering and needed flag marking of the primary IL.
*/
{
  a_boolean              first_pass = TRUE;
  a_memory_region_number n;

  db_enter(1,
           "rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary");
  check_assertion(is_primary_translation_unit);
  /* Do two passes so that the il_walk_flag returns to its original value. */
  for (;;) {
    a_remap_function_ptr remap_func = NULL;
    if (first_pass) remap_func = remap_secondary_pointer_for_rewrite;
    walk_file_scope_il((an_entry_process_function_ptr)NULL,
                       (a_string_entry_process_function_ptr)NULL,
                       remap_func,
                       remap_func,
                       rewrite_secondary_termination_test,
                       /*clear_fe_pointers=*/FALSE);
    /* Loop through the memory regions looking for functions in the
       primary IL, and process them too.  Note that copying has been
       done already, so we're really ruling out file-scope regions from
       secondary translation units and function regions (if any) that
       didn't get copied over. */
    for (n = FILE_SCOPE_REGION_NUMBER + 1;
         n <= highest_used_region_number;
         n++) {
      if (mem_region_is_primary_func_scope(n)) {
        walk_routine_scope_il(n,
                              (an_entry_process_function_ptr)NULL,
                              (a_string_entry_process_function_ptr)NULL,
                              remap_func,
                              remap_func,
                              rewrite_secondary_termination_test,
                              /*clear_fe_pointers=*/FALSE);
      }  /* if */
    }  /* for */
    if (!first_pass) break;
    first_pass = FALSE;
  }  /* for */
  db_exit();
}  /* rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary */


void trans_copy_one_time_init(void)
/*
One-time initialization for trans_copy.c static variables.
*/
{
  in_trans_copy_setup = FALSE;
  in_primary_il_reference_rewrite = FALSE;
}  /* trans_copy_one_time_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

