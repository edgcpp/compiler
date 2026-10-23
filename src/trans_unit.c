/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

trans_unit.c -- Translation unit management routines.

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

#include "fe_init.h"
#include "fe_wrapup.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Structure used to keep track of variables that are specific to a
given translation unit.

Variables are registered during one-time initialization.  When a
translation unit is started, space is allocated for all of the
registered variables.  When switching from one translation unit to
another, the registered variables are saved into the memory block
for that old translation unit and the values associated with the
new translation unit are copied into the registered variables.
*/
typedef struct a_variable_registration *a_variable_registration_ptr;
typedef struct a_variable_registration {
  a_variable_registration_ptr
		next;
			/* Pointer to the next entry on a list of
			   registered variables, or NULL for the last entry. */
  a_void_ptr	ptr;
			/* Pointer to the global variable to be saved and
			   restored. */
  sizeof_t	size;
			/* Size of the variable. */
  sizeof_t	offset;
			/* The location in the variables block at which this
			   variable is stored. */
  sizeof_t	field_offset;
			/* The offset within the translation unit data
			   structure of a field that contains a pointer to the
			   current version of the data being used for a given
			   translation unit.  That field points either to the
			   global variable or to a location within the
			   variables_block of the translation unit.  Contains
			   zero if there is no translation unit field. */
} a_variable_registration;


STATIC_THREAD a_translation_unit_stack_entry_ptr
		avail_translation_unit_stack_entries;
			/* List of translation unit stack entries that have
			   been freed and are available for reuse. */

STATIC_THREAD an_export_trans_unit_stack_entry_ptr
		avail_export_template_translation_unit_stack_entries;
			/* List of export template translation unit stack
			   entries that have been freed and are available for
			   reuse. */

STATIC_THREAD a_trans_unit_corresp_ptr
		avail_trans_unit_corresps;
			/* List of translation unit correspondence entries
			   that have been freed and are available for reuse. */

STATIC_THREAD a_variable_registration_ptr
		trans_unit_variables;
			/* Pointer to a list of variable registrations for
			   variables that are local to a given translation
			   unit. */

STATIC_THREAD a_variable_registration_ptr
		trans_unit_variables_tail;
			/* Pointer to the last entry on the list of variables
			   that are local to a given translation unit. */

STATIC_THREAD sizeof_t
		trans_unit_var_block_size;
			/* Size of the memory block used to store variables
			   that are specific to a given translation unit. */

STATIC_THREAD a_translation_unit_ptr
		translation_units_tail;
			/* Pointer to the end of the list of translation
			   units. */

#if CHECKING
STATIC_THREAD a_boolean
		any_translation_units_allocated;
			/* Set to TRUE once the first translation unit
			   entry has been allocated.  Variable registrations
			   are not permitted after this point. */

STATIC_THREAD a_boolean
		any_exported_template_files_loaded;
			/* Set to TRUE once the first translation unit is
			   loaded for the purpose of defining an exported
			   template. */
#endif /* CHECKING */

#if DEBUG
/* GCOVR_EXCL_START */
STATIC_THREAD unsigned long
		num_translation_unit_stack_entries_allocated,
		num_export_trans_unit_stack_entries_allocated,
		num_translation_units_allocated,
		num_trans_unit_corresps_allocated,
		num_variable_registrations_allocated;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */


a_trans_unit_corresp_ptr alloc_trans_unit_corresp(void)
/*
Allocate a translation unit correspondence entry, initialize its fields,
and return a pointer to it.
*/
{
  a_trans_unit_corresp_ptr tucp;

  if (avail_trans_unit_corresps != NULL) {
    tucp = avail_trans_unit_corresps;
    /* The canonical pointer is used as the next pointer. */
    avail_trans_unit_corresps = (a_trans_unit_corresp_ptr)tucp->canonical;
  } else {
    tucp = alloc_fe_of_type(a_trans_unit_corresp);
#if DEBUG
/* GCOVR_EXCL_START */
    num_trans_unit_corresps_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  tucp->canonical = NULL;
  tucp->primary = NULL;
  tucp->count = 0;
  tucp->kind = iek_none;
  return tucp;
}  /* alloc_trans_unit_corresp */


void free_trans_unit_corresp(a_trans_unit_corresp_ptr	tucp)
/*
If the correspondence entry is only referred to by one entity, "free" the entry
by returning it to the list of available translation unit correspondences.
Otherwise, decrement the count of entities referring to the entry.
*/
{
  if (tucp->count == 1) {
    /* The canonical pointer is used as the next pointer. */
    tucp->canonical = (char *)avail_trans_unit_corresps;
    avail_trans_unit_corresps = tucp;
  } else {
    check_assertion(tucp->count > 1);
    --tucp->count;
  }  /* if */
}  /* free_trans_unit_corresp */


static
a_translation_unit_stack_entry_ptr alloc_translation_unit_stack_entry(void)
/*
Allocate a translation unit stack entry, initialize its fields, and return
a pointer to the entry created.
*/
{
  a_translation_unit_stack_entry_ptr	tusep;

  if (avail_translation_unit_stack_entries != NULL) {
    /* Reuse an existing entry. */
    tusep = avail_translation_unit_stack_entries;
    avail_translation_unit_stack_entries = tusep->next;
  } else {
    /* Allocate a new entry. */
    tusep = alloc_general_of_type(a_translation_unit_stack_entry);
#if DEBUG
/* GCOVR_EXCL_START */
    num_translation_unit_stack_entries_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  tusep->next = NULL;
  tusep->prev_trans_unit = NULL;
  return tusep;
}  /* alloc_translation_unit_stack_entry */


static an_export_trans_unit_stack_entry_ptr
alloc_export_template_translation_unit_stack_entry(void)
/*
Allocate an export template translation unit stack entry, initialize its
fields, and return a pointer to the entry created.
*/
{
  an_export_trans_unit_stack_entry_ptr tusep;

  if (avail_export_template_translation_unit_stack_entries != NULL) {
    /* Reuse an existing entry. */
    tusep = avail_export_template_translation_unit_stack_entries;
    avail_export_template_translation_unit_stack_entries = tusep->next;
  } else {
    /* Allocate a new entry. */
    tusep = alloc_general_of_type(an_export_trans_unit_stack_entry);
#if DEBUG
/* GCOVR_EXCL_START */
    num_export_trans_unit_stack_entries_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  }  /* if */
  tusep->next = NULL;
  tusep->trans_unit = NULL;
  return tusep;
}  /* alloc_export_template_translation_unit_stack_entry */


static a_variable_registration_ptr alloc_variable_registration(void)
/*
Allocate a variable registration entry, initialize its fields, and return
a pointer to the entry created.
*/
{
  a_variable_registration_ptr	vrp;

  vrp = alloc_general_of_type(a_variable_registration);
#if DEBUG
/* GCOVR_EXCL_START */
  num_variable_registrations_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  vrp->next = NULL;
  vrp->ptr = NULL;
  vrp->size = 0;
  vrp->offset = 0;
  vrp->field_offset = 0;
  return vrp;
}  /* alloc_variable_registration */


void f_register_trans_unit_variable(a_void_ptr	var,
				    sizeof_t	size,
				    sizeof_t	field_offset)
/*
Register a variable that is specific to a given translation unit.
*/
{
  a_variable_registration_ptr	vrp;

  check_assertion_str2(!any_translation_units_allocated,
                       "f_register_trans_unit_variable:",
                       "registration too late");
  check_assertion_str2(var != NULL,
                       "f_register_trans_unit_variable:",
                       "NULL variable pointer");
#if EXPENSIVE_CHECKING
  {
    /* Make sure this variable is not already registered. */
    for (vrp = trans_unit_variables; vrp != NULL; vrp = vrp->next) {
      check_assertion_str2(vrp->ptr != var, "f_register_trans_unit_variable:",
                           "duplicate registration");
    }  /* for */
  }
  check_assertion_str2(size <= 2048, "f_register_trans_unit_variable:",
                       "entity registered is too large");
#endif /* EXPENSIVE_CHECKING */
  vrp = alloc_variable_registration();
  vrp->ptr = var;
  vrp->size = size;
  vrp->offset = trans_unit_var_block_size;
  vrp->field_offset = field_offset;
  if (trans_unit_variables == NULL) trans_unit_variables = vrp;
  if (trans_unit_variables_tail != NULL) {
    trans_unit_variables_tail->next = vrp;
  }  /* if */
  trans_unit_variables_tail = vrp;
  /* Round the size up to the nearest increment of HOST_ALIGNMENT_REQUIRED. */
  do_host_alignment(&size);
  /* Increment the size of the block required to store the translation unit
     variables. */
  trans_unit_var_block_size += size;
}  /* f_register_trans_unit_variable */

#if EXPENSIVE_CHECKING

static void check_using_directive_scope_info(a_scope_ptr	scope)
/*
Go through the namespace lists of the file and namespace scopes and make
sure that the depth at which using directive applies field has been
cleared.
*/
{
  a_namespace_ptr	nsp;

  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    /* Ignore namespace alias entries. */
    if (nsp->is_namespace_alias) continue;
    check_using_directive_scope_info(nsp->variant.assoc_scope);
  }  /* for */
}  /* check_using_directive_scope_info */

#endif /* EXPENSIVE_CHECKING */

static void clear_scope_stack_related_information(void)
/*
This routine is used when saving the translation unit state.  It clears the
depth_in_scope_stack field of any scopes on the scope stack.
*/
{
  a_scope_stack_entry_ptr  ssep;

  /* Clear the information about using-directives in scopes on the scope
     stack. */
  set_active_using_list_scope_depths(depth_scope_stack,
                                     /*set_value=*/FALSE,
                                     NO_DECL_SEQUENCE_NUMBER);
  for (ssep = &scope_stack[depth_scope_stack]; ssep != NULL;
       ssep = ssep->kind == (a_scope_kind)sck_file ? NULL : ssep - 1) {
    a_scope_ptr	scope = ssep->il_scope;
    if (scope != NULL) {
      scope->depth_in_scope_stack = NO_SCOPE_DEPTH;
    }  /* if */
  }  /* for */
#if EXPENSIVE_CHECKING
  /* Make sure that the using-directive state information was cleared
     properly. */
  check_using_directive_scope_info(il_header.primary_scope);
#endif /* EXPENSIVE_CHECKING */
}  /* clear_scope_stack_related_information */


static void set_scope_stack_related_information(void)
/*
This routine is used when restoring the translation unit state.  It sets the
depth_in_scope_stack field of any scopes on the scope stack that have
associated IL scopes.
*/
{
  /* Refresh the scope stack so it's up to date with the current IL state. */
  refresh_scope_stack();
  /* Reset the active using list flags to the values. */
  set_active_using_list_scope_depths(depth_scope_stack,
                                     /*set_value=*/TRUE,
                                     get_effective_decl_seq());
}  /* set_scope_stack_related_information */


static void save_translation_unit_state(a_translation_unit_ptr	tup)
/*
Copy the variables for a given translation unit to the variables block
pointed to by the translation unit entry.
*/
{
  a_variable_registration_ptr	vrp;
  a_void_ptr			var_block;

  var_block = tup->variables_block;
  for (vrp = trans_unit_variables; vrp != NULL; vrp = vrp->next) {
    a_void_ptr	src;
    a_void_ptr	dest;
    src = vrp->ptr;
    dest = (a_void_ptr)(((char*)var_block) + vrp->offset);
    memcpy(dest, src, size_t_arg(vrp->size));
    /* If there is an associated translation unit field, set it to point
       to the copy in the variables block. */
    if (vrp->field_offset != 0) {
      a_void_ptr	*field;
      field = (a_void_ptr*)((char *)tup + vrp->field_offset);
      *field = (a_void_ptr)dest;
    }  /* if */
  }  /* for */
  /* Save several per-translation-unit fields of il_header. */
  tup->il_header.main_routine = il_header.main_routine;
#if RECORD_MACROS_IN_IL
  tup->il_header.macros = il_header.macros;
#endif /* RECORD_MACROS_IN_IL */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  tup->il_header.scope_orphaned_list_headers =
                                         il_header.scope_orphaned_list_headers;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  tup->il_header.nontag_types_used_in_exception_or_rtti =
                              il_header.nontag_types_used_in_exception_or_rtti;
  /* Reset the depth_in_scope stack field of any scopes on the scope stack. */
  if (depth_scope_stack != NO_SCOPE_DEPTH) {
    clear_scope_stack_related_information();
  }  /* if */
}  /* save_translation_unit_state */


static void restore_translation_unit_state(a_translation_unit_ptr	tup)
/*
Copy the variables for a given translation unit from the variables block
pointed to by the translation unit entry.
*/
{
  a_variable_registration_ptr	vrp;
  a_void_ptr			var_block;

  var_block = tup->variables_block;
  for (vrp = trans_unit_variables; vrp != NULL; vrp = vrp->next) {
    a_void_ptr	src;
    a_void_ptr	dest;
    dest = vrp->ptr;
    src = (a_void_ptr)(((char*)var_block) + vrp->offset);
    memcpy(dest, src, size_t_arg(vrp->size));
    /* If there is an associated translation unit field, set it to point
       to the global variable. */
    if (vrp->field_offset != 0) {
      a_void_ptr	*field;
      field = (a_void_ptr*)((char *)tup + vrp->field_offset);
      *field = (a_void_ptr)dest;
    }  /* if */
  }  /* for */
  /* Restore several per-translation-unit fields of il_header. */
  il_header.primary_scope = tup->primary_scope;
  il_header.main_routine = tup->il_header.main_routine;
#if RECORD_MACROS_IN_IL
  il_header.macros = tup->il_header.macros;
#endif /* RECORD_MACROS_IN_IL */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  il_header.scope_orphaned_list_headers =
                                    tup->il_header.scope_orphaned_list_headers;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  il_header.nontag_types_used_in_exception_or_rtti =
                         tup->il_header.nontag_types_used_in_exception_or_rtti;
#if CPPCLI_ENABLING_POSSIBLE
  if (cli_or_cx_enabled) {
    a_cli_metadata_file_ptr cmfp;
    a_boolean               is_duplicate;

    /* Reset the metadata reader for the next translation unit. */
    ms_metadata_trans_unit_wrapup();
    ms_metadata_trans_unit_init(trans_unit_file_name);
    /* Restore the metadata files.  Since the metadata reader doesn't use
       the memory region mechanism, it cannot be saved using the normal PCH
       mechanism.  Instead, we will re-import the metadata files.  However,
       since the top level declarations are in the symbol table already, we
       can skip that step. */
    cmfp = il_header.cli_metadata_files;
    while (cmfp != NULL) {
      a_cpp_cli_import_flag_set import_flags = default_cpp_cli_import_flags;
      if (cmfp->as_friend) {
        import_flags |= (a_cpp_cli_import_flag_set)cpp_cli_as_friend_assembly;
      }  /* if */
      if (wchar_t_is_keyword) {
        import_flags |= (a_cpp_cli_import_flag_set)cpp_cli_wchar_t_is_keyword;
      }  /* if */
      /* Re-register the assemblies that we have imported.  It is important
         that we import the assemblies in the same order so that they will
         maintain the same assembly index. */
      if (import_metadata_file(cmfp->full_name, import_flags,
                               &is_duplicate) != cmfp->assembly_index) {
        unexpected_condition();
      }  /* if */
      cmfp = cmfp->next;
    }  /* while */
  }  /* if */
#endif /* CPPCLI_ENABLING_POSSIBLE */
  /* Restore the depth_in_scope stack field of any scopes on the scope
     stack. */
  if (depth_scope_stack != NO_SCOPE_DEPTH) {
    set_scope_stack_related_information();
  }  /* if */
}  /* restore_translation_unit_state */


void fix_up_translation_unit(a_translation_unit_ptr       tup)
/*
The specified translation unit (which is the only translation unit at this
point), has just been restored from a PCH file; fix any invalid pointers
that it may contain (due to ASLR on many operating systems).  Most fields
in tup point directly to memory that resides in memory regions and need
no fixing; however there are some fields in a_translation_unit (namely
orphaned_file_scope_il_entries and module_id_ptr) that point to global
variables (or into a variable block).  Since the addresses of global variables
may have changed since the PCH file was written, these fields need to be
re-initialized to the new addresses of the global variables.  Such fields
are identified by variable registration entries with non-NULL field_offset
pointers; use these variable registration entries to reset the tup fields to
the proper global variable addresses.
*/
{
  a_variable_registration_ptr   vrp;

  check_assertion(!secondary_translation_unit_seen());
  for (vrp = trans_unit_variables; vrp != NULL; vrp = vrp->next) {
    /* If there is an associated translation unit field, set it to point
       to the global variable. */
    if (vrp->field_offset != 0) {
      a_void_ptr        *field;
      field = (a_void_ptr*)((char *)tup + vrp->field_offset);
      *field = (a_void_ptr)vrp->ptr;
    }  /* if */
  }  /* for */
}  /* fix_up_translation_unit */

#if DEBUG
/* GCOVR_EXCL_START */

void db_translation_unit(a_translation_unit_ptr	tup)
/*
Display a translation unit, for debugging purposes.
*/
{
  fprintf(f_debug, "Translation unit %s\n", tup->source_file->file_name);
}  /* db_translation_unit */


void db_translation_unit_stack(void)
/*
Display the translation unit stack, for debugging purposes.
*/
{
  a_translation_unit_stack_entry_ptr tusep = curr_translation_unit_stack_entry;
  a_translation_unit_ptr             tup = curr_translation_unit;
  int                                count = 0;

  fprintf(f_debug, "Translation unit stack:\n");
  do {
    fprintf(f_debug, "  %d: %s\n", count, tup->source_file->file_name);
    count++;
    if (tusep == NULL) {
      break;
    }  /* if */
    tup = tusep->prev_trans_unit;
    tusep = tusep->next;
  } while (tup != NULL);
}  /* db_translation_unit_stack */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

void switch_translation_unit(a_translation_unit_ptr	tup)
/*
Make the translation unit specified by "tup" the current translation unit.
*/
{
  check_assertion(curr_translation_unit != NULL);
  if (tup != curr_translation_unit) {
    /* Only switch if the current translation unit is not the one desired. */
    save_translation_unit_state(curr_translation_unit);
    curr_translation_unit = tup;
    restore_translation_unit_state(tup);
  }  /* if */
}  /* switch_translation_unit */


void push_translation_unit_stack(a_translation_unit_ptr	tup)
/*
Add an entry for "tup" to the top of the translation unit stack, and make
it the current translation unit.
*/
{
  a_translation_unit_stack_entry_ptr	tusep;

  /* The given translation unit must not be NULL. */
  check_assertion(tup != NULL);
  tusep = alloc_translation_unit_stack_entry();
  tusep->next = curr_translation_unit_stack_entry;
  tusep->prev_trans_unit = curr_translation_unit;
  switch_translation_unit(tup);
  curr_translation_unit_stack_entry = tusep;
}  /* push_translation_unit_stack */


void pop_translation_unit_stack(void)
/*
Remove the top entry from the translation unit stack and make the
new top entry the current translation unit.
*/
{
  a_translation_unit_stack_entry_ptr	tusep;

  tusep = curr_translation_unit_stack_entry;
  /* If this assertion fails, there's no translation unit on the stack. */
  check_assertion(tusep != NULL);
  /* Unlink this entry from the stack. */
  curr_translation_unit_stack_entry = tusep->next;
  /* Restore the previous translation unit. */
  switch_translation_unit(tusep->prev_trans_unit);
  /* Add the old entry to the list of available stack entries. */
  tusep->next = avail_translation_unit_stack_entries;
  avail_translation_unit_stack_entries = tusep;
}  /* pop_translation_unit_stack */


void push_export_template_translation_unit_stack(a_translation_unit_ptr tup)
/*
Add an entry for "tup" to the top of the export template translation unit
stack.
*/
{
  an_export_trans_unit_stack_entry_ptr ettusep;

  /* The given translation unit must not be NULL. */
  check_assertion(tup != NULL);
  ettusep = alloc_export_template_translation_unit_stack_entry();
  ettusep->next = curr_export_translation_unit_stack_entry;
  ettusep->trans_unit = tup;
  curr_export_translation_unit_stack_entry = ettusep;
  /* Update the translation unit stack itself. */
  push_translation_unit_stack(tup);
}  /* push_export_template_translation_unit_stack */


void pop_export_template_translation_unit_stack()
/*
Remove the top entry from the export template translation unit stack.
*/
{
  an_export_trans_unit_stack_entry_ptr ettusep;

  ettusep = curr_export_translation_unit_stack_entry;
  /* If this assertion fails the export template translation stack is empty
     or out of sync with the current translation unit state. */
  check_assertion((ettusep != NULL) &&
                  (curr_translation_unit == ettusep->trans_unit));
  /* Update the translation unit stack itself. */
  pop_translation_unit_stack();
  /* Unlink this entry from the stack. */
  curr_export_translation_unit_stack_entry = ettusep->next;
  /* Add the old entry to the list of available stack entries. */
  ettusep->next = avail_export_template_translation_unit_stack_entries;
  avail_export_template_translation_unit_stack_entries = ettusep;
}  /* pop_export_template_translation_unit_stack */


static a_boolean push_translation_unit_if_needed(a_translation_unit_ptr tup)
/*
Push the given translation unit to the translation unit stack if it is not the
current translation unit.  Return TRUE if the translation unit was added to
the stack; otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  if (tup != curr_translation_unit) {
    result = TRUE;
    push_translation_unit_stack(tup);
  }  /* if */
  return result;
}  /* push_translation_unit_if_needed */


a_boolean push_primary_translation_unit_if_needed()
/*
Push the primary translation unit if not already
*/
{
  /* Note that translation_units always starts with the primary translation
     unit. */
  return push_translation_unit_if_needed(translation_units);
}  /* push_primary_translation_unit_if_needed */


a_boolean push_translation_unit_if_needed(a_symbol_ptr	sym)
/*
If "sym" was declared in a translation unit other than the current one, push
its translation unit onto the translation unit stack.  If the translation unit
the symbol was declared in cannot be determined, fall back to the primary
translation unit.

Return TRUE if a translation unit was pushed, FALSE if not.
*/
{
  a_boolean result = FALSE;

  if (symbol_has_trans_unit_ptr(sym)) {
    a_translation_unit_ptr tup = trans_unit_for_symbol(sym);

    result = push_translation_unit_if_needed(tup);
  } else {
    /* Assume the primary translation unit is the safest place to perform any
       operations in lieu of better information. */
    result = push_primary_translation_unit_if_needed();
  }  /* if */
  return result;
}  /* push_translation_unit_if_needed */


static a_translation_unit_ptr alloc_translation_unit(void)
/*
Allocate a translation unit entry, initialize its fields, and return
a pointer to the entry created.
*/
{
  a_translation_unit_ptr	tup;
  a_variable_registration_ptr	vrp;

#if CHECKING
  any_translation_units_allocated = TRUE;
#endif /* CHECKING */
  tup = alloc_fe_of_type(a_translation_unit);
#if DEBUG
/* GCOVR_EXCL_START */
  num_translation_units_allocated++;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  tup->next = NULL;
  /* Allocate the variable block for this translation unit. */
  tup->variables_block = alloc_fe(trans_unit_var_block_size);
  tup->primary_scope = NULL;
  clear_scope_pointers_block(&tup->file_scope_pointers_block);
  tup->source_file = NULL;
  memzero((char *)&tup->il_header, sizeof(an_il_header));
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
   tup->last_scope_orphaned_list_header = NULL;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_MACROS_IN_IL
  tup->last_macro = NULL;
#endif /* RECORD_MACROS_IN_IL */
  tup->based_type_fixup_list = NULL;
  tup->exported_template_file = NULL;
  tup->specified_on_command_line = FALSE;
  tup->additional_instantiation_wrapup_required = TRUE;
  tup->file_scope_region_number = NULL_region_number;
#if NEED_NAME_MANGLING
  tup->individuated_namespace = NULL;
#endif /* NEED_NAME_MANGLING */
#if EXPENSIVE_CHECKING && DEBUG
  tup->is_partially_sequenced = FALSE;
  tup->is_fully_sequenced = FALSE;
#endif /* EXPENSIVE_CHECKING && DEBUG */
  /* Translation unit fields that are maintained by the mechanism that
     saves and restores translation unit variables.  They point to whichever
     copy of the information is currently active (either the global variable
     or the copy in the variables block of the translation unit entry).
     These are initialized with a pointer to the global variable. */
  for (vrp = trans_unit_variables; vrp != NULL; vrp = vrp->next) {
    /* If there is an associated translation unit field, set it to point
       to the global variable. */
    if (vrp->field_offset != 0) {
      a_void_ptr	dest;
      a_void_ptr	*field;
      dest = vrp->ptr;
      field = (a_void_ptr*)((char *)tup + vrp->field_offset);
      *field = (a_void_ptr)dest;
    }  /* if */
  }  /* for */
  return tup;
}  /* alloc_translation_unit */


void process_translation_unit(a_const_char			*file_name,
			      a_boolean				is_primary,
			      an_exported_template_file_ptr	exported_file)
/*
This routine processes a translation unit (a source file and any
files included by that source file).  file_name is the name of the
primary source file of the translation unit.  is_primary is TRUE if the
translation unit is the primary translation unit.   If the translation
unit is being processed for the purpose of defining exported templates,
exported_file describes the file to be processed.

There is usually one translation unit per compilation.  When the
COMPILE_MULTIPLE_SOURCE_FILES flag is TRUE, the front end can
perform multiple compilations, each of which will typically contain
one translation unit (but may contain more).  There is more than one
translation unit per compilation when making use of exported templates.
When using exported templates, the translation units containing the
definitions of the exported templates are processed as secondary
translation units.

When COMPILE_MULTIPLE_TRANSLATION_UNITS is TRUE, multiple source files
can be specified on the command-line and the source files are each
treated as separate translation units of a single compilation.
*/
{
  a_translation_unit_ptr	trans_unit;

#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 1 || db_flag_is_set("trans_unit")) {
    fprintf(f_debug, "Processing translation unit %s\n", file_name);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif  /* DEBUG */
#if CHECKING
  if (!is_primary && exported_file == NULL) {
    /* We can't load a normal secondary translation unit after an exported
       file has been loaded because the command-line macro definition
       information and include search paths will not be correct. */
    check_assertion(!any_exported_template_files_loaded);
  }  /* if */
  if (exported_file != NULL) any_exported_template_files_loaded = TRUE;
#endif /* CHECKING */
  if (curr_translation_unit != NULL) {
    /* Save the currently active set of translation unit specific variables. */
    save_translation_unit_state(curr_translation_unit);
  }  /* if */
  /* Set a current position indicating we are in initialization.  This
     actually does something for a secondary translation unit. */
  set_position_to(pos_curr_token, 0, SP_COL_UNKNOWN);
  set_err_pos_to_curr_token();
  /* Initialize the front end. */
  is_primary_translation_unit = is_primary;
  translation_unit_needed_only_for_exported_templates = exported_file != NULL;
  trans_unit_file_name = file_name;
  compute_il_prefix_size();
  if (is_primary_translation_unit) fe_init_part_1();
  trans_unit = alloc_translation_unit();
  trans_unit->exported_template_file = exported_file;
  /* If the exported_file passed in is NULL, this is a translation unit
     specified on the command line.  Note that the exported_template_file
     in the translation unit may get set later for files specified on the
     command line. */
  trans_unit->specified_on_command_line = exported_file == NULL;
  /* Add this translation unit to the list of translation units. */
  if (translation_units == NULL) {
    translation_units = trans_unit;
    /* The primary translation unit must be first. */
    check_assertion(is_primary_translation_unit);
  }  /* if */
  curr_translation_unit = trans_unit;
  /* Push this translation unit onto the translation unit stack. */
  push_translation_unit_stack(trans_unit);
  if (translation_units_tail != NULL) {
    translation_units_tail->next = trans_unit;
  }  /* if */
  translation_units_tail = trans_unit;
  if (exported_file != NULL) {
    /* Set the include search path and macro define/undefines to be used for
       this exported template file.  For secondary translation units loaded
       from the command-line, these variables retain the values used for the
       primary translation unit. */
    char	*dir_name;
    defs_from_cmd_line = exported_file->define_list;
    incl_search_path = exported_file->incl_search_path;
    sys_incl_search_path = exported_file->sys_incl_search_path;
    end_incl_search_path = exported_file->end_incl_search_path;
    /* Save the translation unit associated with this exported template. */
    exported_file->translation_unit = trans_unit;
    /* For exported translation units, update the include search path to
       reflect the directory of the file being used. */
    dir_name = gs_directory_of(file_name);
    dir_name_of_primary_source_file = dir_name;
    add_to_front_of_include_search_path(dir_name, &incl_search_path,
                                        &end_incl_search_path);
  }  /* if */
  fe_translation_unit_init();
#if MODULE_ID_NEEDED
  if (exported_file != NULL) {
    /* When loading a file for the purpose of defining exported templates,
       the module ID must be restored to the value used when the file was
       originally compiled. */
    set_module_id(exported_file->module_id);
  }  /* if */
#endif /* MODULE_ID_NEEDED */
  if (do_preprocessing_only) {
    /* Compiler is to operate like cpp, and do just preprocessing. */
    fe_init_part_2();
    cpp_driver();
  } else {
    /* Compiler is to do preprocessing and compilation. */
    if (precompiled_header_processing_required &&
        !cannot_do_pch_processing) {
      fe_init_for_pch_prefix_scan();
      precompiled_header_processing();
    }  /* if */
    fe_init_part_2();
    translation_unit();
  }  /* if */
  translation_unit_wrapup();
  /* Remove this entry from the translation unit stack. */
  pop_translation_unit_stack();
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 1 || db_flag_is_set("trans_unit")) {
    fprintf(f_debug, "Done processing translation unit %s\n", file_name);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif  /* DEBUG */
#if COMPILE_MULTIPLE_TRANSLATION_UNITS
  if (is_primary) {
    /* Process any secondary translation units specified on the
       command line. */
    proc_secondary_translation_units();
  }  /* if */
#endif /* COMPILE_MULTIPLE_TRANSLATION_UNITS */
  /* No db_exit because the start and end of this routine are not in
     the same translation unit and that fouls up the stop tokens check. */
}  /* process_translation_unit */

#if DEBUG
/* GCOVR_EXCL_START */

unsigned long db_show_trans_unit_space_used(unsigned long grand_total)
/*
Show space used by the trans_unit routines.  This is called by
the symbol table space used routine.  The space used by the trans_unit
routines is reported as part of the symbol table memory used.
*/
{
  unsigned long	num;
  unsigned long	size;
  unsigned long	total;

  db_space_used("trans. unit corresps",
                num_trans_unit_corresps_allocated,
                a_trans_unit_corresp);
  db_space_used_general("translation units",
                        num_translation_units_allocated,
                        a_translation_unit);
  db_space_used_general("trans. unit stack entry",
                        num_translation_unit_stack_entries_allocated,
                        a_translation_unit_stack_entry);
  db_space_used_general("export templ trans. unit stack entry",
                        num_export_trans_unit_stack_entries_allocated,
                        an_export_trans_unit_stack_entry);
  db_space_used_general("variable registration",
                        num_variable_registrations_allocated,
                        a_variable_registration);
  return grand_total;
}  /* db_show_trans_unit_space_used */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */


void trans_unit_one_time_init(void)
/*
One-time initialization for trans_unit variables.
*/
{
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(curr_translation_unit),
      pch_saved_var_array_elem(translation_units),
      pch_saved_var_array_elem(translation_units_tail),
#if DEBUG
/* GCOVR_EXCL_START */
      pch_saved_var_array_elem(num_trans_unit_corresps_allocated),
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  register_trans_unit_variable(is_primary_translation_unit);
  register_trans_unit_variable(trans_unit_file_name);
  register_trans_unit_variable(
                          translation_unit_needed_only_for_exported_templates);
}  /* trans_unit_one_time_init */


void trans_unit_init(void)
/*
The per-compilation unit initialization routine for variables related to
translation unit processing.
*/
{
  curr_translation_unit = NULL;
  translation_units = NULL;
  translation_units_tail = NULL;
  translation_unit_needed_only_for_exported_templates = FALSE;
  curr_translation_unit_stack_entry = NULL;
  curr_export_translation_unit_stack_entry = NULL;
}  /* trans_unit_init */


void trans_unit_early_init(void)
/*
One time initialization that must occur early in the execution of the
front end.  This must occur before the one-time initialization routines
of the front end are called.
*/
{
  trans_unit_variables = NULL;
  trans_unit_variables_tail = NULL;
  trans_unit_var_block_size = 0;
  is_primary_translation_unit = FALSE;
  trans_unit_file_name = NULL;
  trans_unit_module = NULL;
  avail_translation_unit_stack_entries = NULL;
  avail_export_template_translation_unit_stack_entries = NULL;
  avail_trans_unit_corresps = NULL;
#if DEBUG
/* GCOVR_EXCL_START */
  num_translation_unit_stack_entries_allocated = 0;
  num_export_trans_unit_stack_entries_allocated = 0;
  num_translation_units_allocated = 0;
  num_variable_registrations_allocated = 0;
  num_trans_unit_corresps_allocated = 0;
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if CHECKING
  any_translation_units_allocated = FALSE;
  any_exported_template_files_loaded = FALSE;
#endif /* CHECKING */
}  /* trans_unit_early_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

