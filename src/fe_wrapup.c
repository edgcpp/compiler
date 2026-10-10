/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

fe_wrapup.c - End of front end processing.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include "fe_wrapup.h"
#include "class_decl.h"
#include "interpret.h"
/*lint -esym(766, macro.h)*/
#include "macro.h"
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if DEBUG
#include "overload.h"
#endif /* DEBUG */
#include "templates.h"
#include "trans_corresp.h"
#include "trans_copy.h"
#include "pch.h"
#if DO_IL_LOWERING
#include "lower_il.h"
#endif /* DO_IL_LOWERING */
#if MANGLE_ALL_NAMES
#include "lower_name.h"
#endif /* MANGLE_ALL_NAMES */
#if DEBUG
#include "exprutil.h"
#include "preproc.h"
#include "statements.h"
#endif /* DEBUG */
#if MAINTAIN_NEEDED_FLAGS || DO_IL_LOWERING
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS || DO_IL_LOWERING */
#if BACK_END_IS_CP_GEN_BE
#include "cp_gen_be.h"
#endif /* BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_LLVM_GEN_BE
#include "llvm_gen_be.h"
#endif /* BACK_END_IS_LLVM_GEN_BE */
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ms_metadata.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEED_IL_DISPLAY
#include "il_display.h"
#endif /* NEED_IL_DISPLAY */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if DEBUG
static void show_space_used(void)
/*
Show the amount of memory allocated.
*/
{
  unsigned long total_space = 0;

  /* Show space use in various categories. */
  total_space += show_symbol_space_used();
  total_space += show_macro_space_used();
  total_space += show_error_space_used();
  total_space += show_lexical_space_used();
  total_space += show_decl_space_used();
  total_space += show_expr_space_used();
  total_space += show_il_space_used();
  total_space += show_statements_space_used();
  total_space += show_preproc_space_used();
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
  total_space += show_attribute_space_used();
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if DO_IL_LOWERING
  total_space += show_lowering_space_used();
#endif /* DO_IL_LOWERING */

  show_mem_manage_space_used(total_space);
}  /* show_space_used */
#endif /* DEBUG */


void translation_unit_wrapup(void)
/*
Do any processing that is required at the end of a translation unit
(primary or secondary).  This is called after all the source code
for the translation unit has been read, but before any templates
are instantiated.
*/
{
  db_enter(1, "translation_unit_wrapup");
#if CHECKING
  /* Check that the stop token array elements all made it back to zero.
     (Every add_stop_token is supposed to have a corresponding
     remove_stop_token.)  Note that there is also a check in db_exit,
     which can be used to pin down problems that are initially
     spotted here. */
  check_all_stop_token_entries_are_reset(
                                   curr_stop_token_stack_entry->stop_tokens);
#endif /* CHECKING */
  if (!do_preprocessing_only && any_cfront_mode()) {
    /* Determine whether any classes defined in this file require external
       linkage, and if so do the appropriate fixup.  No such fixup is
       required in non-cfront mode, since the initial linkage settings
       are already external, when appropriate. */
    check_class_linkage();
  }  /* if */
  /* Check for suppressed errors in imported modules. */
  modules_check_for_suppressed_errors();
  /* Pop and repush the file scope.  This is done to move the symbols from
     the active list to the inactive list. */
  pop_scope();
  push_file_scope(/*is_reactivation=*/TRUE);
  /* Establish any IL correspondences (for primary translation units this
     doesn't involve actual work, but sets some state flags). */
  if (!do_preprocessing_only) {
    set_trans_unit_correspondences();
  }  /* if */
#if MODULE_ID_NEEDED
  /* Make sure the module id is generated for this translation unit if it
     hasn't been generated already.  If this is the first time make_module_id
     has been called for this translation unit, an inferior module id
     will be generated (i.e., no suitable variable or routine definition
     was found). */
  (void)make_module_id((char *)NULL);
#if !STANDALONE_UTILITY_PROGRAM && DO_IL_LOWERING
  /* There may be functions whose lowering has previously been delayed
     because a suitable module id had not yet been created until now.  If so,
     lower those functions now. */
  lower_functions_waiting_for_module_id();
#endif /* !STANDALONE_UTILITY_PROGRAM && DO_IL_LOWERING */
#endif /* MODULE_ID_NEEDED */
  if (!C_mode() && !is_primary_translation_unit && !do_preprocessing_only) {
    /* Check for the presence of a master instance established in a prior
       translation unit. */
    set_master_instance_information();
  }  /* if */
  if (is_primary_translation_unit && !do_preprocessing_only) {
    check_create_pch_file_created();
  }  /* if */
  db_exit();
}  /* translation_unit_wrapup */

#if DO_IL_LOWERING

static void externalize_entity_for_exported_templates(
                                                  a_source_correspondence *scp,
                                                  an_il_entry_kind        kind)
/*
Externalize the entity with the indicated source correspondence and kind.
It's a static entity that may be referenced from exported templates.
*/
{
  a_boolean                is_variable = (kind == iek_variable);
  a_variable_ptr           var;
  a_routine_ptr            rout = NULL;
  a_trans_unit_corresp_ptr tucp;

#if DEBUG
  if (db_has_traced_name(scp, kind)) {
    fprintf(f_debug, "Externalizing for exported templates:\n");
    db_entity_info((char *)scp, kind);
  }  /* if */
#endif /* DEBUG */
  externalize_source_correspondence(scp, is_variable);
  if (is_variable) {
    var = (a_variable_ptr)scp;
    var->storage_class = (a_storage_class)sc_unspecified;
  } else {
    check_assertion(kind == iek_routine);
    rout = (a_routine_ptr)scp;
    rout->storage_class = (a_storage_class)sc_unspecified;
  }  /* if */
  /* Add a trans_unit_corresp entry (an entity with external linkage should
     have one). */
  tucp = alloc_trans_unit_corresp();
  tucp->kind = kind;
  tucp->canonical = (char *)scp;
  if (!in_secondary_trans_unit(scp)) tucp->primary = (char *)scp;
  scp->trans_unit_corresp = tucp;
  if (!is_variable) {
    /* A static function that has been externalized. */
    a_boolean is_template = (rout->is_template_function &&
                             !rout->is_specialized);
    if (!rout->is_inline && !is_template) {
      /* A simple non-inline static function.  Gets put out when its file
         is compiled as a primary file, i.e., not "instantiatable". */
    } else {
      /* For others, someone will decide where the definition is put out:
         either the prelinker or the extern inline lowering mechanism. */
      if (!rout->is_inline && is_template) {
        /* A non-inline static template.  This is already on the instantiation
           lists. */
      } else {
        /* Other cases are treated as if they were extern inline. */
        check_assertion(rout->is_inline);
        if (instantiate_extern_inline && !rout->is_consteval &&
            !rout->on_inline_function_list) {
          /* Add the function to the inline functions list, which is an
             instantiation list for non-templates. */
          add_to_inline_function_list(rout);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if MAINTAIN_NEEDED_FLAGS
  mark_as_needed((char *)scp, kind);
#endif /* MAINTAIN_NEEDED_FLAGS */
}  /* externalize_entity_for_exported_templates */


static void externalize_statics_for_exported_templates(a_scope_ptr scope);


static void externalize_type_list_statics_for_exported_templates(
                                                          a_type_ptr type_list)
/*
Externalize all statics under types on the indicated type list as potentially
referenced by exported templates.
*/
{
  a_type_ptr type;

  for (type = type_list; type != NULL; type = type->next) {
    if (is_immediate_class_type(type) && !ignore_type_in_back_end(type)) {
      a_scope_ptr scope =
                      type->variant.class_struct_union.extra_info->assoc_scope;
      if (!scope_is_null_or_placeholder(scope)) {
        externalize_statics_for_exported_templates(scope);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* externalize_type_list_statics_for_exported_templates */


static void externalize_statics_for_exported_templates(a_scope_ptr scope)
/*
Externalize all statics in the indicated scope and its subscopes as potentially
referenced by exported templates.  The scope is the file scope, a namespace
scope, or a class scope.
*/
{
  a_routine_ptr   rout;
  a_variable_ptr  var;
  a_namespace_ptr nsp;

  check_assertion(scope->kind == (a_scope_kind)sck_file ||
                  scope->kind == (a_scope_kind)sck_namespace ||
                  scope->kind == (a_scope_kind)sck_class_struct_union);
  externalize_type_list_statics_for_exported_templates(scope->types);
  for (rout = scope->routines; rout != NULL; rout = rout->next) {
    if (!ignore_routine_in_back_end(rout) &&
        routine_should_be_externalized_for_exported_templates(rout)) {
      externalize_entity_for_exported_templates(&rout->source_corresp,
                                                iek_routine);
    }  /* if */
  }  /* for */
  for (var = scope->variables; var != NULL; var = var->next) {
    if (!ignore_variable_in_back_end(var) &&
        variable_should_be_externalized_for_exported_templates(var)) {
      externalize_entity_for_exported_templates(&var->source_corresp,
                                                iek_variable);
    }  /* if */
  }  /* for */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      externalize_statics_for_exported_templates(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  if (scope->kind == (a_scope_kind)sck_file) {
    /* Process all local types. */
    process_local_types(scope,
                        externalize_type_list_statics_for_exported_templates);
  }  /* if */
}  /* externalize_statics_for_exported_templates */

#endif /* DO_IL_LOWERING */

static void reset_template_parent_info(a_scope_ptr	il_scope)
/*
When prototype instantiations are not included in the IL, template entries
whose parent class is a prototype instantiation must have their parent
information cleared, because the parent prototype instantiation is not
in the IL.
*/
{
  a_template_ptr	templ;

  for (templ = il_scope->templates; templ != NULL; templ = templ->next) {
    if (templ->source_corresp.is_class_member &&
        has_nonreal_parent_type(&templ->source_corresp)) {
      templ->source_corresp.parent_scope = NULL;
      templ->source_corresp.is_class_member = FALSE;
    }  /* if */
  }  /* for */
}  /* reset_template_parent_info */


static void file_scope_il_wrapup_part_1(void)
/*
Do the processing required to complete the file scope IL.  This is
called both for secondary translation units (is_primary_translation_unit
is FALSE) and for primary translation units (is_primary_translation_unit
is TRUE).  "Part 1" deals with processing and diagnostics on what's actually
in the translation unit, e.g., checks for unreferenced symbols.
When the primary translation unit is processed here, code from any
secondary translation units will not have been copied over already.
Code should be executed here instead of translation_unit_wrapup if
it needs to be executed after all templates have been instantiated.
*/
{
  a_scope_ptr	il_scope;

  il_scope = curr_translation_unit->primary_scope;

  /* Do the initial modules wrapup to shutdown loading of new entities from a
     module. */
  modules_trans_unit_wrapup_part_1();
  /* Do any lexical cleanup that may be needed for this translation unit. */
  lexical_trans_unit_wrapup();
  if (is_primary_translation_unit && !do_preprocessing_only) {
    if (any_cfront_mode()) {
      /* Repeat the class linkage check that was first done during translation
         unit wrapup.  This is done again to catch any classes that may have
         been added during the instantiation process. */
      check_class_linkage();
    }  /* if */
  }  /* if */

  /* Do the wrapup_scope processing on file and namespace scopes. */
  wrapup_scope(il_scope, (a_scope_kind)sck_file,
               &curr_translation_unit->file_scope_pointers_block,
               /*is_namespace_wrapup=*/TRUE, /*is_local_reactivation=*/FALSE,
               PS_NO_OPTIONS);
  wrapup_namespace_scopes(il_scope);

  if (!C_mode()) {
    /* Go through the fixup list for based-type entries and remove entities
       as required.  This must happen before the call to mark_secondary_-
       trans_unit_IL_entities_used_from_primary_as_needed to ensure that
       based type list entries in the primary translation unit pointing to
       types in secondary translation units are removed. */
    do_based_type_fixup();
  }  /* if */
  /* Do any modules cleanup that may be needed for this translation unit. */
  modules_trans_unit_wrapup_part_2();
#if MANGLE_ALL_NAMES
  if (name_mangling_needed()) {
    /* Do name mangling for all entities.  This has to be done before
       the names for statics referenced from templates are externalized.
       If we're using a PCH file (either creating one or using a previously
       created file), the names available at the time of the PCH file creation
       have already been mangled (and won't be mangled again). */
    do_all_name_mangling(/*mangling_pre_pass=*/FALSE);
#if DO_IL_LOWERING
    if (any_exported_templates()) {
      /* In a compilation with exported templates, all statics are potentially
         referenced from templates.  Externalize them. */
      externalize_statics_for_exported_templates(
                                         curr_translation_unit->primary_scope);
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
#endif /* MANGLE_ALL_NAMES */
  if (!C_mode()) {
    if (!prototype_instantiations_in_il) {
      /* Clear the parent information for any template entries that are
         members of prototype instantiations. */
      reset_template_parent_info(il_scope);
    }  /* if */
  }  /* if */

  /* Release any persistent storage held by the C++14 constexpr interpreter. */
  clean_up_interpreter();
}  /* file_scope_il_wrapup_part_1 */


static void file_scope_il_wrapup_needed_flag_processing(void)
/*
Do the needed-flag processing for the current translation unit
(primary or secondary).  The current translation unit is swept to
mark external entities and the things they reference as "needed".
*/
{
  if (!is_at_least_one_error()) {
#if MAINTAIN_NEEDED_FLAGS
    /* Set the "needed" flag in defined variables with external linkage --
       both in the file scope and in each of the namespace scopes. */
    set_needed_flags_at_end_of_file_scope(
                                         curr_translation_unit->primary_scope);
#endif /* MAINTAIN_NEEDED_FLAGS */
#if ONE_INSTANTIATION_PER_OBJECT
#if DO_IL_LOWERING
    if (one_instantiation_per_object && is_primary_translation_unit &&
        il_lowering_needed()) {
      /* Any statics referenced from instantiation slices in
         one-instantiation-per-object mode must be made external so that
         they can be referenced from the instantiation object files. */
      /* Note: this needs to be done after the call of
         set_needed_flags_at_end_of_file_scope, so that statics referenced
         from instantiations are marked before we have to decide whether
         they need to be externalized. */
#if MAINTAIN_NEEDED_FLAGS
      end_of_file_scope_needed_flags_phase = TRUE;
#endif /* MAINTAIN_NEEDED_FLAGS */
      make_statics_referenced_from_instantiations_external();
#if MAINTAIN_NEEDED_FLAGS
      end_of_file_scope_needed_flags_phase = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
    }  /* if */
#endif /* DO_IL_LOWERING */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  }  /* if */
}  /* file_scope_il_wrapup_needed_flag_processing */


static void file_scope_il_wrapup_keep_in_il_processing(void)
/*
Do the needed-flag keep-in-il processing for the current translation unit
(primary or secondary).
*/
{
#if MAINTAIN_NEEDED_FLAGS
  a_scope_ptr il_scope = curr_translation_unit->primary_scope;

  if (!is_at_least_one_error()) {
    /* Set the "keep_in_il" flag for all file-scope IL entries that must
       be kept to maintain the integrity of the IL. */
    end_of_file_scope_needed_flags_phase = TRUE;
    mark_to_keep_in_il((char *)il_scope, (an_il_entry_kind)iek_scope);
    end_of_file_scope_needed_flags_phase = FALSE;
  }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
}  /* file_scope_il_wrapup_keep_in_il_processing */


static void file_scope_il_wrapup_remove_unneeded_il(void)
/*
Do removal of unneeded IL entities for the current translation unit
(primary or secondary).
*/
{
#if MAINTAIN_NEEDED_FLAGS
  a_scope_ptr il_scope = curr_translation_unit->primary_scope;

  /* Don't bother pruning the IL of unneeded entries if errors were seen. */
  if (is_at_least_one_error()) okay_to_eliminate_unneeded_il_entries = FALSE;
  if (!C_mode() && !is_at_least_one_error()) {
    /* Reset the instantiation_required flag on any entities that aren't
       needed so the prelinker doesn't try to instantiate them. */
    clear_instantiation_required_on_unneeded_entities(il_scope);
  }  /* if */
  if (okay_to_eliminate_unneeded_il_entries) {
    /* Eliminate unneeded function bodies.  Note that the function
       declarations are not removed at this point. */
    eliminate_bodies_of_unneeded_functions();
    /* Now eliminate everything at file and namespace scope that does not
       need to be kept in the IL. */
    eliminate_unneeded_il_entries(il_scope);
  }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
}  /* file_scope_il_wrapup_remove_unneeded_il */


static void file_scope_il_wrapup_part_2(void)
/*
Do more wrapup processing on a translation unit.  A single call of
this routine handles all translation units.  "Part 2" does needed
flag processing and unneeded IL removal for secondary translation units.
*/
{
  a_translation_unit_ptr tup;

#if MAINTAIN_NEEDED_FLAGS
  /* The processing has three parts:
       1)  Needed-flag marking
       2)  Keep-in-il flag marking
       3)  Removal of unneeded entities
     Each one of those parts is done for all secondary translation
     units before the next part is done, because the secondary translation
     unit IL is all intermingled.  You can't remove entities in one
     secondary translation unit until you have accounted for references
     from all other secondary translation units. */
  if (!is_at_least_one_error()) {
    /* Sweep the primary translation unit IL tree and look for any
       pointers to entities in secondary translation units that it uses,
       and mark those entities as needed. */
    mark_secondary_trans_unit_IL_entities_used_from_primary_as_needed();
  }  /* if */
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
    file_scope_il_wrapup_needed_flag_processing();
  }  /* for */
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
    file_scope_il_wrapup_keep_in_il_processing();
  }  /* for */
#endif /* MAINTAIN_NEEDED_FLAGS */
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
#if MAINTAIN_NEEDED_FLAGS
    file_scope_il_wrapup_remove_unneeded_il();
#endif /* MAINTAIN_NEEDED_FLAGS */
  }  /* for */
}  /* file_scope_il_wrapup_part_2 */


static void file_scope_il_wrapup_part_3(void)
/*
Do the final wrapup processing on a translation unit.  This is
called both for secondary translation units (is_primary_translation_unit
is FALSE) and for primary translation units (is_primary_translation_unit
is TRUE).  "Part 3" does IL lowering and needed flag processing for
the primary translation unit, and pops the file scope for both primary
and secondary translation units.  When the primary translation unit is
processed here, code from any secondary translation units will have
already been copied over.
*/
{
#if EXPENSIVE_CHECKING
  symbol_table_trans_unit_validate();
#endif /* EXPENSIVE_CHECKING */
  if (is_primary_translation_unit) {
#if DO_IL_LOWERING
    /* Lower the file scope. */
    lower_file_scope();
#endif /* DO_IL_LOWERING */
  }  /* if */

  /* Clear out the shareable constants table for the file scope. */
  empty_shareable_constants_table();

  if (is_primary_translation_unit && !C_mode()) {
    /* Pop the file scope object lifetime.  This must be done after IL
       lowering. */
    check_assertion(curr_object_lifetime ==
                    scope_stack[depth_scope_stack].curr_scope_object_lifetime);
    (void)pop_object_lifetime();
#if DO_IL_LOWERING
    if (il_lowering_needed()) {
      /* If we're not supposed to pass object lifetime information to the back
         end, unlink all object lifetimes from the IL tree.  This has to
         be done after the file scope object lifetime has been popped. */
      clean_up_all_object_lifetimes(curr_translation_unit->primary_scope);
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
  /* Pop the file scope. */
  pop_scope();
  if (is_primary_translation_unit) {
    /* Do needed-flag processing for the primary translation unit.
       The needed-flag processing for secondary translation units
       was done in part 2. */
    file_scope_il_wrapup_needed_flag_processing();
    file_scope_il_wrapup_keep_in_il_processing();
#if MANGLE_ALL_NAMES
    if (name_mangling_needed()) {
      /* Do final name mangling, which can make names that can no longer
         be embedded in other names, and therefore must be done very late.
         In particular, it must be done after
         make_statics_referenced_from_instantiations_external and
         before the removal of unneeded IL entities (because
         parent classes and functions need to be around still). */
      do_final_name_mangling();
    }  /* if */
#endif /* MANGLE_ALL_NAMES */
    /* Do removal of unneeded IL entities for the primary translation
       unit.  That was done for secondary translation units in part 2. */
    file_scope_il_wrapup_remove_unneeded_il();
#if ENSURE_LOWERED_TYPE_LIST_ORDERING
    if (il_lowering_needed() && perform_type_list_ordering) {
      /* Ensure that the file-scope types list is ordered for correct C-code
         generation. */
      fix_type_list_ordering_problems();
    }  /* if */
#endif /* ENSURE_LOWERED_TYPE_LIST_ORDERING */
    /* Check for memory regions that were not written out but now should
       be.  Among other things, this deals with functions that have
       keep_definition_in_il set but not definition_needed, and inline
       functions. */
    check_for_done_with_all_function_memory_regions();
#if AUTOMATIC_TEMPLATE_INSTANTIATION
    if (!C_mode()) {
      if (!is_at_least_one_error()) {
        /* Set the IL flags used to pass automatic instantiation information to
           the link-time instantiation processor.  The timing of this call is
           important.  It must follow the call to
           eliminate_unneeded_il_entries, which may clear the
           instantiation_required flag in the associated template instance
           entry.  And it must precede the call to
           check_for_done_with_memory_region, since it modifies IL entries and
           (if DO_IL_LOWERING is TRUE) may allocate variables that are added to
           the IL. */
        update_auto_instantiation_flags();
        /* Do the similar processing for inline functions and/or variables,
           when instantiating those inline entities similarly to templates. */
        update_inline_entity_flags();
      }  /* if */
      /* Do any special processing needed to wrapup the automatic instantiation
         process at the end of the compilation. */
      wrapup_auto_instantiation_information();
    }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if DO_IL_LOWERING
    if (il_lowering_needed()) {
      /* Clear class/namespace membership information and the
         is_local_to_function flag on entities promoted out of classes,
         namespaces, and functions.  This must be done very late, after
         the generation of instantiation flags, because after parents are
         cleared it becomes impossible to generate mangled names. */
      clear_parent_information();
    }  /* if */
#endif /* DO_IL_LOWERING */
#if CHECKING
    /* Ensure that unexpected situations did not occur without at least one
       error being issued (otherwise, abort compilation).  This is done
       here because it's the last point where the file-scope memory region
       is still available, which is needed to be able to translate the saved
       sequence number back into a file name/line number. */
    check_expected_errors();
#endif /* CHECKING */
    check_for_done_with_memory_region(file_scope_region_number);
  }  /* if */
#if CPPCLI_ENABLING_POSSIBLE
  if (cli_or_cx_enabled) {
    ms_metadata_trans_unit_wrapup();
  }  /* if */
#endif /* CPPCLI_ENABLING_POSSIBLE */
}  /* file_scope_il_wrapup_part_3 */


static void finish_processing_for_function_bodies(void)
/*
Call finish_function_body_processing for all functions not yet processed.
This mainly handles functions copied from secondary translation units, but
there may be some functions in the primary IL for which lowering was delayed.
Functions whose lowering was delayed due to lack of a module id have already
been lowered (by lower_functions_waiting_for_module_id).
*/
{
  if (function_body_processing_delayed_on_some_func_in_primary_il) {
    /* Do inline functions in a first pass to have a better chance of inlining
       calls to them. */
    a_boolean inline_pass = TRUE; 
    for (;;) {
      a_function_def_number n;
      for (n = 1;
           n <= highest_used_function_def_number;
           n++) {
        finish_function_processing_for_function_def(n, inline_pass);
      }  /* for */
      if (!inline_pass) break;
      inline_pass = FALSE;
    }  /* for */
    function_body_processing_delayed_on_some_func_in_primary_il = FALSE;
  }  /* if */
}  /* finish_processing_for_function_bodies */


static void wrap_up_file_scopes(void)
/*
Complete the file scope of each of the translation units.
*/
{
  a_translation_unit_ptr	tup;

  /* Do the initial wrapup processing for each of the secondary and
     primary translation units.  This includes all of the processing except
     for copying IL from secondary translation units, and popping of the
     file scope.  */
  tup = translation_units->next;
  for (; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
    file_scope_il_wrapup_part_1();
  }  /* for */
  /* Switch back to the primary translation unit. */
  switch_translation_unit(translation_units);
  /* Process the primary translation unit. */
  file_scope_il_wrapup_part_1();
  /* Do more wrapup processing for each of the secondary and primary
     translation units.  This call processes all the translation units. */
  file_scope_il_wrapup_part_2();
  /* Do the final wrapup processing for each of the secondary and primary
     translation units. */
  tup = translation_units->next;
  for (; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
    file_scope_il_wrapup_part_3();
  }  /* for */
  /* Copy IL from the secondary translation units to the primary IL.
     This must be done before the lowering of the primary IL.
     In trans_unit_test mode, we don't check for duplicate definitions,
     so we can't do the copy. */
  if (!is_at_least_one_error() && !trans_unit_test_mode &&
#if DO_IL_LOWERING
      !suppress_il_lowering &&
#endif /* DO_IL_LOWERING */
      translation_units->next != NULL) {
    copy_secondary_trans_unit_IL_to_primary();
    /* Some function bodies may have been copied to the primary IL, so
       check for any needed lowering. */
    function_body_processing_delayed_on_some_func_in_primary_il = TRUE;
  }  /* if */
  /* Switch back to the primary translation unit. */
  switch_translation_unit(translation_units);
  /* Finish processing on function bodies moved to the primary IL,
     including IL lowering if appropriate.  Do this also on any
     function bodies in the primary IL whose lowering was delayed.
     Lowering is delayed on some instantiations in the primary
     translation unit when there are exported templates so that we
     can rewrite any references to secondary translation unit entities
     before the lowering is done. */
  finish_processing_for_function_bodies();
  /* Remove the definitions of any functions instantiated only to determine
     their return types or static data members instantiated only to
     determine their size. */
  if (instantiation_mode != tim_all) {
    remove_unneeded_instantiations();
  }  /* if */
  /* Process the primary translation unit. */
  file_scope_il_wrapup_part_3();
  /* Free the secondary IL file-scope memory regions. */
  if (translation_units->next != NULL) {
    a_memory_region_number n;
    for (n = FILE_SCOPE_REGION_NUMBER + 1;
         n <= highest_used_region_number;
         n++) {
      if (mem_region_table[n] != NULL &&
          il_header.region_scope_entry[n]->kind == (a_scope_kind)sck_file) {
          free_memory_region(n);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* wrap_up_file_scopes */


void fe_wrapup(void)
/*
Do any processing required at the end of execution of the front end,
and before the back end (if any) is executed.
*/
{

  db_enter(1, "fe_wrapup");

  /* Switch back to the primary translation unit. */
  switch_translation_unit(translation_units);
  /* Make sure that we have switched back to processing the primary
     translation unit. */
  check_assertion_str2(is_primary_translation_unit,
                       "fe_wrapup:", "bad translation unit in fe_wrapup");

  if (!C_mode()) {
    /* For each translation unit, generate any instantiations that are
       needed, and determine which inline functions require definitions. */
    template_and_inline_entity_wrapup();
    if (collect_top_templates) {
      show_top_templates(top_templates_count);
    }  /* if */
  }  /* if */
#if CHECKING && DEBUG
  check_all_init_component_entries_freed();
#endif /* CHECKING && DEBUG */

  if (list_macro_definitions) {
    /* Write definition lines for all macros. */
    gen_pp_output_for_macro_definitions();
  }  /* if */
    
#if MACRO_INVOCATION_TREE_IN_IL
  /* Transform the macro invocation tree from its front-end (list) form into
     the IL (binary tree) form and set the related fields in il_header. */
  copy_macro_invocation_tree_to_il();
#endif /* MACRO_INVOCATION_TREE_IN_IL */
#if DEBUG
  if (db_flag_is_set("source_file_for_seq_info")) {
    /* Display some debug information about the source file to sequence
       number translation process. */
    db_source_file_for_seq_info();
  }  /* if */
#endif /* DEBUG */

  /* Lower the file scope, remove unneeded entities, etc. */
  wrap_up_file_scopes();

  in_front_end = FALSE;
  curr_translation_unit = NULL;

#if NEED_IL_DISPLAY
  if (il_display && !is_at_least_one_error()) {
    /* Display the IL. */
    do_il_display((char *)NULL);
  }  /* if */
#endif /* NEED_IL_DISPLAY */

#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* Finish writing the IL file, if there is one. */
  finish_il_file();
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

  /* Close the preprocessing output file, if needed. */
  close_output_file_with_error_handling(&f_pp_output, ec_preprocessing_output);

  /* Close the raw listing file if one is being generated. */
  close_output_file_with_error_handling(&f_raw_listing, ec_raw_listing);

  /* Close the cross-reference file if one is being generated. */
  close_output_file_with_error_handling(&f_xref_info, ec_cross_reference);

#if DEBUG
  if (display_space_used || debug_level > 0 || db_flag_is_set("space_used")) {
    /* Print total memory used. */
    show_space_used();
  }  /* if */
  if (db_flag_is_set("scope_stack")) {
    db_scope_stack_stats();
  }  /* if */
  if (db_flag_is_set("viability")) {
    db_viability_stats();
  }  /* if */
#endif /* DEBUG */

#if CHECKING
  /* Make sure all local constants were released. */
  check_local_constant_use();
#endif /* CHECKING */

  /* Don't keep checking the stop token stack in db_enter/db_exit because
     the storage goes away when the front end memory region is freed. */
  curr_stop_token_stack_entry = NULL;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  if (skip_il_read) {
    /* Leave the memory regions intact (mostly for debugging purposes). */
  } else {
    /* Free all memory regions.  Anything left in any of the IL memory regions
       should be discarded as it will be reread by module write out or the back
       end. */
    free_all_memory_regions();
  }  /* if */
#else /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* Free front-end-only storage. */
  free_memory_region(FRONT_END_REGION_NUMBER);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

  /* Clear the file index list maintained by the error routines (it was
     allocated in front-end storage). */
  clear_file_index_list();

  db_exit();
}  /* fe_wrapup */


void fe_wrapup_part_2(void)
/*
Do any processing required at the end of execution of the front end,
and after the back end (if any) is executed.
*/
{
#if CPPCLI_ENABLING_POSSIBLE
  if (cli_or_cx_enabled) ms_metadata_cleanup();
#endif /* CPPCLI_ENABLING_POSSIBLE */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* Close the IL output file, be it a temporary or actual file. */
  close_il_output_file();
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* Write a signoff message (with count of errors) if necessary. */
  write_signoff();
  /* Free the front end memory region, file-scope IL and all function scope
     IL.  This is necessary if an IL file is not written, or if some regions
     were kept because of inlining, but it's a good idea in all cases.
     Some of the regions may have already been freed.  That is okay because
     freeing a region a second time does nothing. */
  free_all_memory_regions();
}  /* fe_wrapup_part_2 */

#if MAKE_FRONT_END_CALLABLE

void fe_cleanup(void)
/*
Clean up any resources used by this compilation.  This routine may be called
at the end of a normal compilation, or may be called at any point during
a compilation that was abnormally terminated for some reason.  This routine
calls routines to close any files that may be open, and frees all of the
memory used by the compilation.
*/
{
  cmd_line_cleanup();
  macro_cleanup();
  templates_cleanup();
#if BACK_END_IS_CP_GEN_BE
  cp_gen_be_cleanup();
#endif /* BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_LLVM_GEN_BE
  llvm_gen_be_cleanup();
#endif /* BACK_END_IS_LLVM_GEN_BE */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  close_il_output_file();
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  error_cleanup();
  lexical_cleanup();
  modules_cleanup();
#if CPPCLI_ENABLING_POSSIBLE
  if (cli_or_cx_enabled) ms_metadata_cleanup();
#endif /* CPPCLI_ENABLING_POSSIBLE */
  mem_manage_wrapup();
  /* It's important that this is called after mem_manage_wrapup.

     Older versions of the Linux kernel require that the file backing a mmap
     operation remain open.  Since the precompile header implementation loads a
     variety of memory mappings from f_pch_input, f_pch_input must NOT be
     closed prior to memory management wrapup (or else odd segfaults will
     occur on older systems). */
  pch_late_cleanup();
  /* This should remain the final cleanup operation. */
  error_late_cleanup();
}  /* fe_cleanup */

#endif /* MAKE_FRONT_END_CALLABLE */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

