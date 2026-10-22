/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

symbol_ref.c - Routines to manage references to symbols.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "class_decl.h"
#include "symbol_ref.h"
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "statements.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if DEBUG
/* GCOVR_EXCL_START */

void db_symbol_ref_kind(a_symbol_reference_kind  kind)
/*
Output the flags set in kind to the debug output in a human-readable way.
*/
{
  if (kind & SRK_DECLARATION) {
    fprintf(f_debug, "declaration ");
  }  /* if */
  if (kind & SRK_DEFINITION) {
    fprintf(f_debug, "definition ");
  }  /* if */
  if (kind & SRK_REFERENCE) {
    fprintf(f_debug, "reference ");
  }  /* if */
  if (kind & SRK_USE) {
    fprintf(f_debug, "use ");
  }  /* if */
  if (kind & SRK_MODIFICATION) {
    fprintf(f_debug, "modification ");
  }  /* if */
  if (kind & SRK_ADDRESS_TAKEN) {
    fprintf(f_debug, "address-taken ");
  }  /* if */
  if (kind & SRK_ERROR) {
    fprintf(f_debug, "error ");
  }  /* if */
  if (kind & SRK_FRIEND) {
    fprintf(f_debug, "friend ");
  }  /* if */
  if (kind & SRK_TENTATIVE_DEF) {
    fprintf(f_debug, "tentative-def ");
  }  /* if */
  if (kind & SRK_IMPLICIT_TEMPLATE_ARG) {
    fprintf(f_debug, "implicit-template-arg ");
  }  /* if */
  if (kind & SRK_INITIALIZATION) {
    fprintf(f_debug, "initialization ");
  }  /* if */
  if (kind & SRK_CONST_ADDRESS_TAKEN) {
    fprintf(f_debug, "const-address-taken ");
  }  /* if */
  if (kind & SRK_PROTO_INST_REF) {
    fprintf(f_debug, "proto-inst-ref ");
  }  /* if */
  if (kind & SRK_DEFAULT_ARG_EXPR) {
    fprintf(f_debug, "default-arg-expr ");
  }  /* if */
  if (kind & SRK_TEMPLATE_INSTANTIATION) {
    fprintf(f_debug, "template-instantiation ");
  }  /* if */
  if (kind & SRK_CONST_VALUE_USE) {
    fprintf(f_debug, "const-value-use ");
  }  /* if */
}  /* db_symbol_ref_kind */

/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

STATIC_THREAD an_il_to_str_output_control_block
		octl;
			/* Output control block used to interface to the
			   il_to_str routines. */

STATIC_THREAD a_boolean
		output_control_block_has_been_set_up;
			/* Flag that indicates whether initialization has
			   already been done on the output control block. */

static void write_string_to_xref_file(
                   a_const_char                                     *str,
                   ARG_UNUSED an_il_to_str_output_control_block_ptr local_octl)
/*
Write str to the xref file.  The address of this routine is passed to
the il_to_str routines for generating formatted names.
*/
{
  (void)fputs(str, f_xref_info);
}  /* write_string_to_xref_file */


static void write_xref_entry(a_symbol_reference_kind srk_flags,
                             a_symbol_ptr            sym_ptr,
                             a_source_position       *source_position)
/*
Write information to f_xref_info describing a reference of kind "kind"
to symbol "sym_ptr" at source position "source_position".  This routine
should only be called if cross-reference information is being generated
(i.e., f_xref_info != NULL).
*/
{
  char              code = ' ';
  a_const_char      *file_name, *full_name;
  a_line_number     line_number;
  a_boolean         at_end_of_source;

#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Don't record cross-references in generated code. */
  if (in_generated_code()) goto done;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (!output_control_block_has_been_set_up) {
    /* Set octl so that it can be passed into the il_to_str routines to tell
       them how to write a string to the xref file. */
    clear_il_to_str_output_control_block(&octl);
    octl.output_str = write_string_to_xref_file;
    octl.keep_template_typedefs = FALSE;
    octl.render_auto_deduction_typerefs = TRUE;
    output_control_block_has_been_set_up = TRUE;
  }  /* if */
  if (sym_ptr->kind == (a_symbol_kind)sk_extern_variable ||
             sym_ptr->kind == (a_symbol_kind)sk_extern_routine) {
    /* Ignore extern variable and routine symbols.  They are really just
       shadow symbols for the real ones. */
  } else if (is_unnamed_tag_symbol(sym_ptr)) {
    /* Ignore symbols for unnamed classes and enums. */
  } else if (source_position->seq == 0) {
    /* This symbol is not associated with any particular source position. */
  } else {
    /* The record written to the file is a text line that looks like

       symbol-id name X file-name line-number column-number

       The separator character between the fields is a horizontal tab.
       where X is "d" for declaration,
                  "D" for definition,
                  "t" for partial instantiation,
                  "T" for full instantiation,
                  "M" for modification,
                  "A" for address taken,
                  "U" for use,
                  "C" for changed (i.e., used and modified in one
                      operation, such as an increment operation),
                  "R" for generic reference, or
                  "E" for error.
       The symbol-id is a unique number for the symbol, generated by 
       casting the symbol pointer to unsigned long.
    */
    if (srk_flags & (SRK_DECLARATION | SRK_TEMPLATE_INSTANTIATION)) {
      if (srk_flags & SRK_DEFINITION) {
        code = (srk_flags & SRK_TEMPLATE_INSTANTIATION) ? 'T' : 'D';
      } else {
        code = (srk_flags & SRK_TEMPLATE_INSTANTIATION) ? 't' : 'd';
      }  /* if */
    } else if (srk_flags & SRK_REFERENCE) {
      if (srk_flags & SRK_USE) {
        if (srk_flags & SRK_MODIFICATION) {
          code = 'C';
        } else {
          code = 'U';
        }  /* if */
      } else if (srk_flags & SRK_MODIFICATION) {
        code = 'M';
      } else if (srk_flags & SRK_ADDRESS_TAKEN) {
        code = 'A';
      } else if (srk_flags & SRK_ERROR) {
        code = 'E';
      } else {
        code = 'R';
      }  /* if */
#if CHECKING
    } else {
      internal_error("write_xref_entry: bad reference kind");
#endif /* CHECKING */
    }  /* if */
    /* Convert the source position to file name/line number. */
    (void)conv_seq_to_file_and_line(source_position->seq, &file_name,
                                    &full_name, &line_number,
                                    &at_end_of_source);
    fprintf(f_xref_info, "%p\t", (void *)sym_ptr);
    /* Write the symbol name, complete with class qualifier, etc., to the
       xref file. */
    form_symbol_name(sym_ptr, &octl);
    fprintf(f_xref_info, "\t%c\t%s\t%lu\t%d\n",
                         code,
                         format_file_name(file_name),
                         (unsigned long)line_number,
                         source_position->column);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
done:;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* write_xref_entry */


void mark_variable_value_set(a_symbol_ptr  sym)
/*
Set the "value_has_been_set" flag of the variable symbol pointed to by sym.
*/
{
  check_assertion(sym->kind == (a_symbol_kind)sk_variable);
  if (sym->value_has_been_set) {
    /* Variable has already been set. */
    a_variable_ptr  vp = sym->variant.variable.ptr;
    if (vp->is_parameter || vp->is_handler_param) {
      /* Since parameters are by definition initialized (by the actual
         argument), any subsequent modification is a change to the initial
         value.  Knowing this can be useful for inlining. */
      vp->param_value_has_been_changed = TRUE;
    }  /* if */
  } else {
    sym->value_has_been_set = TRUE;
  }  /* if */
}  /* mark_variable_value_set */


static void mark_static_data_member_value_set(a_symbol_ptr  sym)
/*
Set the "value_has_been_set" flag of the static data member symbol pointed to
by sym.
*/
{
  check_assertion(sym->kind == (a_symbol_kind)sk_static_data_member);
  sym->value_has_been_set = TRUE;
}  /* mark_static_data_member_value_set */

#if RECORD_HIDDEN_NAMES_IN_IL

static a_boolean symbols_are_equivalent(a_symbol_ptr  sym1,
                                        a_symbol_ptr  sym2)
/*
Return TRUE if sym1 and sym2 point to the same IL entries.
*/
{
  an_il_entry_kind  kind;
  a_boolean         equiv = (sym1 == sym2);

  if (!equiv) {
    sym1 = fundamental_symbol_of(sym1);
    sym2 = fundamental_symbol_of(sym2);
    equiv = sym1 == sym2;
    if (!equiv) {
      if (sym1->kind == sym2->kind) {
        char *entry1 = il_entry_for_symbol_null_okay(sym1, &kind);
        char *entry2 = il_entry_for_symbol_null_okay(sym2, &kind);
        if (entry1 == entry2 && entry1 != NULL) equiv = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return equiv;
}  /* symbols_are_equivalent */


static a_hidden_name_ptr make_new_hidden_name(a_scope_ptr  sp)
/*
Allocate a new hidden name entry and link it in the list for the given scope.
*/
{
  a_hidden_name_ptr       hnp;
  a_scope_depth           scope_depth;
  a_memory_region_number  region_to_switch_back_to;
   /* Get the scope depth from which to determine the appropriate
     memory region in which to allocate the hidden-name entry. */
  if (in_file_scope(sp)) {
    scope_depth = DEPTH_OF_FILE_SCOPE;
  } else {
    scope_depth = sp->depth_in_scope_stack;
    check_assertion(scope_depth != NO_SCOPE_DEPTH);
  }  /* if */
  switch_to_scope_region(scope_depth, &region_to_switch_back_to);
  hnp = alloc_hidden_name();
  switch_back_to_original_region(region_to_switch_back_to);
  /* Add it to the start of the hidden_names list for the current
     scope. */
  hnp->next = sp->hidden_names;
  sp->hidden_names = hnp;
  return hnp;
}  /* make_new_hidden_name */


static void record_defeatable_name_hiding_for_single_entity(
                              a_symbol_ptr  hidden_sym,
                              a_boolean     tag_hidden_by_nontag,
                              a_boolean     hidden_class_or_namespace_member,
                              a_boolean     simulated_hiding,
                              a_scope_ptr   sp,
                              a_symbol_ptr  hidden_by)
/*
hidden_sym is a symbol for a single entity that is hidden by another
declaration of the same name -- but the hiding can be "defeated" by using an
elaborated type specifier or global qualification (preceding "::") when
referring to the hidden name.  Create the hidden-name entity to represent
this case and add it to the list for the current scope.  If hidden_by is
non-NULL, it points to the symbol that does the hiding.  simulated_hiding is
TRUE when the hiding symbol is the simulated injected name of an instance of
a class template in Microsoft mode.
(This routine is meant to be called from record_defeatable_name_hiding only.)
*/
{
  a_hidden_name_ptr        hnp;
  char                     *entity;
  an_il_entry_kind         kind;

  /* First find the entity associated with the symbol. */
  entity = il_entry_for_symbol(hidden_sym, &kind);
  /* If there is already a hidden name entry for this entity in this
     scope, reuse it. */
  for (hnp = sp->hidden_names; hnp != NULL; hnp = hnp->next) {
    if (hnp->entity.ptr == entity) break;
  }  /* for */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4 || db_flag_is_set("dump_hidden")) {
    if (hnp == NULL ||
        ((tag_hidden_by_nontag &&
          !hnp->elaborated_type_specifier_needed) ||
         (hidden_class_or_namespace_member &&
          !hnp->qualification_needed))) {
      a_source_correspondence  *scp =
                             source_corresp_for_il_entry(entity, kind);
      fputs("    in ", f_debug);
      db_scope(sp);
      fputs(": use", f_debug);
      if (hidden_class_or_namespace_member) {
        fputs(" qualifier", f_debug);
        if (tag_hidden_by_nontag) fputs(" and", f_debug);
      }  /* if */
      if (tag_hidden_by_nontag) fputs(" class-key", f_debug);
      fprintf(f_debug, " for %s\"",
              hidden_sym->decl_scope == file_scope_number ?
                                               "global " : "");
      if (kind == (an_il_entry_kind)iek_type) {
        db_abbreviated_type((a_type_ptr)entity);
      } else {
        if (scp != NULL) {
          db_name_full(scp, kind);
        } else {
          fprintf(f_debug, "\?\?\?");
        }  /* if */
      }  /* if */
      fprintf(f_debug, "\"%s", hnp == NULL ? "" : " [modif]");
      fprintf(f_debug, "\n");
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  if (hnp == NULL) {
    /* No existing entry.  Allocate a new one. */
    hnp = make_new_hidden_name(sp);
    hnp->entity.ptr = entity;
    hnp->entity.kind = kind;
    hnp->is_class_member = hidden_sym->is_class_member;
    if (hidden_by != NULL) {
      if (hidden_by->kind == (a_symbol_kind)sk_type &&
          hidden_by->variant.type.is_injected_class_name &&
          sp->kind == (a_scope_kind)sck_class_struct_union &&
          sp->variant.assoc_type == hidden_by->variant.type.ptr) {
        hnp->hidden_by_class_name = TRUE;
      } else if (hidden_by->kind == (a_symbol_kind)sk_constant &&
                 hidden_by->variant.constant->kind ==
                                     (a_constant_repr_kind)ck_template_param &&
                 hidden_by->variant.constant->variant.template_param.kind
                               == (a_template_param_constant_kind)tpck_param) {
        hnp->hidden_by_template_parameter = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Set the appropriate flag. */
  if (tag_hidden_by_nontag) {
    check_assertion(kind == (an_il_entry_kind)iek_type);
    hnp->elaborated_type_specifier_needed = TRUE;
#if BACK_END_IS_CP_GEN_BE
    a_type_ptr(entity)->elab_type_spec_needed_in_some_scope = TRUE;
#endif /* BACK_END_IS_CP_GEN_BE */
  }  /* if */
  if (hidden_class_or_namespace_member) {
    a_symbol_ptr  fund_hiding_sym = (hidden_by == NULL) ?
                                       NULL :
                                       fundamental_symbol_of(hidden_by),
                  fund_hidden_sym = fundamental_symbol_of(hidden_sym);
    if (microsoft_mode &&
        fund_hiding_sym != NULL &&
        fund_hiding_sym->kind == (a_symbol_kind)sk_type &&
        fund_hiding_sym->variant.type.is_injected_class_name) {
      /* In Microsoft compilers before version 7.0 an injected class name is
         only visible through qualified lookup.  Furthermore, they do not
         accept (redundant) qualification with a class whose closing brace
         has not yet been seen.  Setting the following flag notifies
         consumers of this special "partial hiding" case.  (Note: this flag
         is set even for microsoft_version >= 1300, where the problem does
         not occur, to enable the C++-generating back end to generate
         correct code for earlier compilers.  The flag will be ignored there
         if msvc_target_version_number is >= 1300.) */
      hnp->partially_hidden_by_microsoft_injected_class_name = TRUE;
    }  /* if */
    check_assertion(in_file_scope(entity));
    if (hnp->elaborated_type_specifier_needed &&
        fund_hiding_sym != NULL && !is_type_symbol(fund_hiding_sym)) {
      /* We do not need qualification if the hidden symbol is a tag symbol
         and the hiding symbol is not a type, as the
         elaborated-type-specifier will be sufficient.  Furthermore, we
         must not add unneeded qualification if the target compiler is
         Visual Studio 2019, as it has a bug that results in spurious
         errors in such cases. */
    } else if (fund_hiding_sym == NULL ||
               !(is_type_symbol(fund_hidden_sym) &&
                 fund_hiding_sym->kind == (a_symbol_kind)sk_type &&
                 fund_hiding_sym->variant.type.is_injected_class_name) ||
               !f_identical_types(type_symbol_type(fund_hidden_sym),
                                  type_symbol_type(fund_hiding_sym),
                                  ITF_NO_FLAGS)) {
      /* It is possible that an injected class name hides another type
         symbol that refers to the same IL entity.  In that case, no
         qualification is needed. */
      hnp->qualification_needed = TRUE;
    }  /* if */
    hnp->hidden_by_simulated_injected_class_name = simulated_hiding;
  }  /* if */
#if BACK_END_IS_CP_GEN_BE
  if (kind == iek_namespace && hidden_by != NULL &&
      is_class_struct_union_symbol(hidden_by)) {
    /* Some versions of g++ issue spurious errors when a namespace name is
       used as a qualifier in a scope in which a class name is visible.
       Mark the namespace so cp_gen_be can declare and use a namespace
       alias for qualification. */
    ((a_namespace_ptr)entity)->shadowed_by_class = TRUE;
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
}  /* record_defeatable_name_hiding_for_single_entity */


static void record_defeatable_name_hiding(
                              a_symbol_ptr  hidden_sym,
                              a_boolean     tag_hidden_by_nontag,
                              a_boolean     hidden_class_or_namespace_member,
                              a_boolean     simulated_hiding,
                              a_scope_ptr   sp,
                              a_symbol_ptr  hidden_by)
/*
hidden_sym is a symbol for an entity that is hidden by another declaration
of the same name -- but the hiding can be "defeated" by using an
elaborated type specifier or global qualification (preceding "::") when
referring to the hidden name.  In addition, names that are ambiguous or
inaccessible are treated as "hidden" to force use of a qualified name when
referring to the entity.  Create the hidden-name entity to represent
this case and add it to the list for the current scope.  If hidden_by is
non-NULL, it points to the symbol that does the hiding; when hidden_sym
and hidden_by refer to the same IL entry, no hidden-name entry is produced.
simulated_hiding is TRUE when the hiding symbol is the simulated injected
name of an instance of a class template in Microsoft mode.
*/
{
  a_symbol_ptr             sym;
  a_template_instance_ptr  tip;
  a_boolean                is_hiding = TRUE;

  check_assertion(sp != NULL);
  if (hidden_by != NULL && symbols_are_equivalent(hidden_sym, hidden_by)) {
    /* A symbol does not hide itself. */
    is_hiding = FALSE;
  } else if (hidden_sym->kind == (a_symbol_kind)sk_member_function &&
             hidden_by != NULL &&
             hidden_by->kind == (a_symbol_kind)sk_overloaded_function) {
    for (sym = hidden_by->variant.overloaded_function.symbols;
         sym != NULL && is_hiding; sym = sym->next) {
      if (symbols_are_equivalent(hidden_sym, sym)) {
        /* A symbol is not hidden by a using-declaration that includes the
           symbol in the resulting overload set. */
        is_hiding = FALSE;
      }  /* if */
    }  /* for */
  }  /* if */
  if (is_hiding) {
    switch (hidden_sym->kind) {
      case sk_label:
      case sk_keyword:
      case sk_macro:
      case sk_undefined:
      case sk_extern_variable:
      case sk_extern_routine:
        /* Not in the same name space. */
        break;
      case sk_overloaded_function:
        /* Enter members of an overload set separately. */
        for (sym = hidden_sym->variant.overloaded_function.symbols;
             sym != NULL;
             sym = sym->next) {
          is_hiding = TRUE;
          if (hidden_by != NULL &&
              hidden_by->kind == (a_symbol_kind)sk_overloaded_function) {
            a_symbol_ptr one_hiding_sym;
            for (one_hiding_sym =
                                hidden_by->variant.overloaded_function.symbols;
                 one_hiding_sym != NULL && is_hiding;
                 one_hiding_sym = one_hiding_sym->next) {
              if (symbols_are_equivalent(sym, one_hiding_sym)) {
                /* A member function is not hidden if it is part of an
                   overload set resulting from a using-declaration. */
                is_hiding = FALSE;
              }  /* if */
            }  /* for */
          }  /* if */
          if (is_hiding) {
            record_defeatable_name_hiding(sym, tag_hidden_by_nontag,
                                          hidden_class_or_namespace_member,
                                          simulated_hiding, sp, hidden_by);
          }  /* if */
        }  /* for */
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case sk_property_set:
        /* Enter members of a property set separately. */
        sym = hidden_sym->variant.property_info->properties;
        for (; sym != NULL; sym = sym->next) {
          record_defeatable_name_hiding(sym, tag_hidden_by_nontag,
                                        hidden_class_or_namespace_member,
                                        simulated_hiding, sp, hidden_by);
        }  /* for */
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case sk_projection:
      case sk_namespace_projection:
        /* Enter the fundamental symbol of a class or namespace
           projection. */
        sym = fundamental_symbol_of(hidden_sym);
        /* If the fundamental symbol belongs to the same namespace as the
           symbol it's hidden by (something that can happen with synthesized
           namespace projections), then the hiding cannot be defeated by a
           qualifier.  Check for that case. */
        if (hidden_class_or_namespace_member &&
            depth_innermost_function_scope == NO_SCOPE_DEPTH &&
            hidden_by != NULL && !hidden_by->is_class_member &&
            !sym->is_class_member &&
            sym_parent_namespace_or_null(sym) ==
                                    sym_parent_namespace_or_null(hidden_by)) {
          /* Set hidden_class_or_namespace_member to FALSE. */
          hidden_class_or_namespace_member = FALSE;
          /* If they're both FALSE don't create a hidden name table entry. */
          if (!tag_hidden_by_nontag) break;
        }  /* if */
        record_defeatable_name_hiding(sym, tag_hidden_by_nontag,
                                      hidden_class_or_namespace_member,
                                      simulated_hiding, sp, hidden_by);
        break;
      case sk_class_template:
        if (hidden_by != NULL && symbol_is(hidden_by, sk_type) &&
            hidden_by->variant.type.is_injected_class_name &&
            hidden_sym->variant.template_info->variant.class_template.
                                             prototype_instantiation != NULL &&
            hidden_sym->variant.template_info->variant.class_template.
                    prototype_instantiation->variant.class_struct_union.type ==
                                                 hidden_by->variant.type.ptr &&
            (cpp11_mode || gpp_mode || microsoft_mode)) {
          /* A class template is not hidden by its injected-class-name in
             C++11 and in the emulated compilers. */
        } else {
          /* Unlike function templates, only the template itself is added
             to the hidden name list; when cp_gen_be encounters an instance
             of a class template, it checks whether the template is hidden
             and processes the instance accordingly.  This avoids an O(N^2)
             performance problem when there are many instances of a class
             template (perhaps resulting from a translation unit that
             contains explicit instantiations), where each instance's
             injected class name would hide every other instance of that
             template. */
          record_defeatable_name_hiding_for_single_entity(
                                              hidden_sym, tag_hidden_by_nontag,
                                              hidden_class_or_namespace_member,
                                              simulated_hiding, sp, hidden_by);
          /* However, we do need to record that partial specializations are
             hidden, as they are treated as separate templates whose
             instances do not refer back to the primary template. */
          for (sym =
                    hidden_sym->variant.template_info->partial_specializations;
               sym != NULL; sym = sym->next) {
            record_defeatable_name_hiding_for_single_entity(
                                              sym, tag_hidden_by_nontag,
                                              hidden_class_or_namespace_member,
                                              simulated_hiding, sp, hidden_by);
          }  /* for */
        }  /* if */
        break;
      case sk_function_template:
        /* Enter each instance of a function template. */
        for (tip = hidden_sym->variant.template_info->
                                  variant.function.instantiations;
             tip != NULL;
             tip = tip->next) {
          if (tip->is_guiding_decl) {
            /* Ignore guiding declarations -- they will have been picked up
               elsewhere. */
          } else {
            record_defeatable_name_hiding(tip->instance_sym,
                                          tag_hidden_by_nontag,
                                          hidden_class_or_namespace_member,
                                          simulated_hiding, sp, hidden_by);
          }  /* if */
        }  /* for */
        /* Process the template itself. */
        record_defeatable_name_hiding_for_single_entity(
                                             hidden_sym, tag_hidden_by_nontag,
                                             hidden_class_or_namespace_member,
                                             simulated_hiding, sp, hidden_by);
        break;
      default:
        record_defeatable_name_hiding_for_single_entity(
                                             hidden_sym, tag_hidden_by_nontag,
                                             hidden_class_or_namespace_member,
                                             simulated_hiding, sp, hidden_by);
        break;
    }  /* switch */
  }  /* if */
}  /* record_defeatable_name_hiding */


static void check_name_unhiding(a_symbol_ptr sym_ptr,
                                a_scope_ptr sp)
/*
Injected class names and block extern declarations enable unqualified access
to entities that might have been previously hidden.  If sym_ptr is a symbol
for such an entity, we create a new hidden name entry for scope sp with all
flags cleared.  Note that this happens even if no hiding had occurred.
*/
{
  a_hidden_name_ptr  hnp = NULL;
  an_il_entry_kind   entity_kind;

  if (is_injected_class_symbol(sym_ptr)) {
    if (microsoft_mode) {
      /* In Microsoft mode, injected class names are only accessible as
         qualified names. */
#if BACK_END_IS_CP_GEN_BE
    } else if (gcc_or_clang_is_generated_code_target && 
               is_injected_template_symbol(sym_ptr)) {
       /* When generating code for g++, the injected class name cannot be used 
          as a template, so don't cancel the hiding. */
#endif /* BACK_END_IS_CP_GEN_BE */
    } else {
      /* An injected class name is accessible without qualification (except in
         Microsoft mode and some cases when generating code for g++).  Creating
         a hidden name entry will ensure that any qualification forced by
         prior entries is canceled.  Make sure that the injected class name
         is not hidden by another member. */
      a_symbol_locator  locator;
      clear_locator(&locator, &sym_ptr->decl_position);
      locator.symbol_header = sym_ptr->header;
      check_assertion(sp->kind == (a_scope_kind)sck_class_struct_union);
      (void)class_qualified_id_lookup(&locator, sp->variant.assoc_type,
                                      IDL_HIDDEN_NAME_LOOKUP |
                                      IDL_DIRECT_CLASS_MEMBERS_ONLY);
      /* If the lookup produced the injected symbol, it is not hidden by
         another member. */
      if (locator.specific_symbol != NULL &&
          fundamental_symbol_of(locator.specific_symbol) == sym_ptr) {
        hnp = make_new_hidden_name(sp);
        hnp->entity.ptr = il_entry_for_symbol(sym_ptr, &entity_kind);
        hnp->entity.kind = entity_kind;
      }  /* if */
    }  /* if */
  } else if (sp->kind == (a_scope_kind)sck_function ||
             sp->kind == (a_scope_kind)sck_block) {
    /* A block extern declaration makes the associated entity accessible
       without qualification.  A new hidden name entry will override any
       previous entry that might have imposed qualification. */
    if (sym_ptr->kind == (a_symbol_kind)sk_variable) {
      a_variable_ptr  var = sym_ptr->variant.variable.ptr;
      if (var->storage_class == (a_storage_class)sc_extern ||
          var->storage_class == (a_storage_class)sc_unspecified) {
        hnp = make_new_hidden_name(sp);
        hnp->entity.ptr = il_entry_for_symbol(sym_ptr, &entity_kind);
        hnp->entity.kind = entity_kind;
      }  /* if */
    } else if (sym_ptr->kind == (a_symbol_kind)sk_routine) {
      a_routine_ptr  routine = sym_ptr->variant.routine.ptr;
      if (routine->storage_class == (a_storage_class)sc_extern ||
          routine->storage_class == (a_storage_class)sc_unspecified) {
        hnp = make_new_hidden_name(sp);
        hnp->entity.ptr = il_entry_for_symbol(sym_ptr, &entity_kind);
        hnp->entity.kind = entity_kind;
      }  /* if */
    } else if (sym_ptr->kind == (a_symbol_kind)sk_overloaded_function) {
      /* Each member of the overload set must be separately "unhidden". */
      a_symbol_ptr  overload_item =
                                 sym_ptr->variant.overloaded_function.symbols;
      for (; overload_item != NULL; overload_item = overload_item->next) {
        check_name_unhiding(overload_item, sp);
      }  /* for */
    }  /* if */
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if ((debug_level >= 4 || db_flag_is_set("dump_hidden")) && hnp != NULL) {
    a_source_correspondence  *scp =
               source_corresp_for_il_entry(hnp->entity.ptr, entity_kind);
    fputs("    in ", f_debug);
    db_scope(sp);
    fprintf(f_debug, ": \"");
    if (entity_kind == (an_il_entry_kind)iek_type) {
      db_abbreviated_type((a_type_ptr)hnp->entity.ptr);
    } else {
      if (scp != NULL) {
        db_name_full(scp, entity_kind);
      } else {
        fprintf(f_debug, "\?\?\?");
      }  /* if */
    }  /* if */
    fprintf(f_debug, "\" can be used as an unqualified name\n");
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
}  /* check_name_unhiding */


static void check_defeatable_base_inaccessibility(
                                              a_type_ptr        class_type,
                                              a_base_class_ptr  bcp)
/*
If a base class bcp is inaccessible (privately but not directly inherited),
the C++ generating back-end cannot access it using an unqualified name in the
class scope of class_type.  Therefore, we treat the base type as hidden in the
scope of the class class_type and mark it as needing qualified access.
This allows the following example to work:
   struct A { static int i; };
   struct B: private A {};
   struct C: B { void f(); };
   void C::f() { ::A::i = 42; }; <-- Needs to remain "::A::i": just "i" or
                                     "A::i" fails.
*/
{
  /* First check if we have access to public members of this base class.
     If so, there is no problem and no need for extra work. */
  if (!is_accessible_base_class(bcp)) {
    /* This base is inaccessible because of private inheritance. */
    a_symbol_ptr            hidden_sym;
    a_scope_ptr             scope;
    a_scope_depth           scope_depth;
    a_hidden_name_ptr       hnp;
    a_memory_region_number  region_to_switch_back_to;
    an_il_entry_kind        kind;

    scope =  class_type->variant.class_struct_union.extra_info->assoc_scope;
    if (in_file_scope(scope)) {
      scope_depth = DEPTH_OF_FILE_SCOPE;
    } else {
      scope_depth = scope->depth_in_scope_stack;
      check_assertion(scope_depth != NO_SCOPE_DEPTH);
    }  /* if */
    switch_to_scope_region(scope_depth, &region_to_switch_back_to);
    hnp = alloc_hidden_name();
    switch_back_to_original_region(region_to_switch_back_to);
    hidden_sym = (a_symbol_ptr)bcp->type->source_corresp.assoc_info;
    hnp->entity.ptr = il_entry_for_symbol(hidden_sym, &kind);
    hnp->entity.kind = kind;
    hnp->qualification_needed = TRUE;
    /* Add it to the start of the hidden_names list for the current scope. */
    hnp->next = scope->hidden_names;
    scope->hidden_names = hnp;
  }  /* if */
}  /* check_defeatable_base_inaccessibility */


static void record_defeatable_hiding_if_not_same(
                                            a_symbol_ptr sym_ptr,
                                            a_scope_ptr  sp,
                                            a_boolean    sym_is_injected_class,
                                            a_boolean    simulated_hiding)
/*
Look up an inherited symbol in the current scope and record the hiding, if any.
If sym_is_injected_class is TRUE, sym_ptr represents an injected class name
(either the real injected class name or, for template instances in Microsoft
bugs mode, a simulation thereof for purposes of the hidden name table only).
In this case a symbol found in the surrounding context might represent the same
class type and should not be hidden.  simulated_hiding is TRUE if the hiding is
due to the simulated injected name of a template instance in Microsoft mode.
*/
{
  a_symbol_ptr     old_sym_ptr;
  a_symbol_locator locator;

  /* Perform a lookup. */
  clear_locator(&locator, &sym_ptr->decl_position);
  locator.symbol_header = sym_ptr->header;
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4 || db_flag_is_set("dump_hidden")) {
    fputs("    ...doing lookup (inherited names)\n", f_debug);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  (void)normal_id_lookup(&locator, IDL_HIDDEN_NAME_LOOKUP);
  old_sym_ptr = locator.specific_symbol;
  /* If something was found, see if it is hidden by sym_ptr. */
  if (old_sym_ptr != NULL) {
    a_symbol_ptr  tag_sym = NULL;
    if (sym_is_injected_class &&
        (is_tag_symbol(old_sym_ptr) ||
         is_class_template_symbol(old_sym_ptr))) {
      tag_sym =
           (a_symbol_ptr)(skip_typerefs(type_symbol_type(sym_ptr))->
                                              source_corresp.assoc_info);
    }  /* if */
    if (tag_sym != NULL && symbols_are_equivalent(old_sym_ptr, tag_sym)) {
      /* sym_ptr does not hide old_sym_ptr -- they represent the same
         declaration. */
    } else if (old_sym_ptr->decl_scope == file_scope_number ||
               sym_is_class_or_namespace_member(old_sym_ptr)) {
      /* The name hiding can be defeated by using a qualifier. */
      record_defeatable_name_hiding(
                              old_sym_ptr,
                              /*tag_hidden_by_nontag=*/FALSE,
                              /*hidden_class_or_namespace_member=*/TRUE,
                              simulated_hiding, sp, sym_ptr);
    }  /* if */
  }  /* if */
}  /* record_defeatable_hiding_if_not_same */


static void clone_inherited_hidden_members(a_type_ptr derived_class,
                                           a_type_ptr base_class)
/*
Create copies in the derived_class hidden name list of all the hidden names
in the base class list that refer to members.  Any inherited members that
are hidden in a base class are either hidden or ambiguous in the derived
class, too, and thus must be flagged as requiring qualification.
*/
{
  a_hidden_name_ptr base_hnp;
  a_scope_ptr       derived_scope =
             derived_class->variant.class_struct_union.extra_info->assoc_scope;
  a_scope_ptr       base_scope =
                base_class->variant.class_struct_union.extra_info->assoc_scope;

  if (base_scope != NULL &&
      !base_class->variant.class_struct_union.extra_info->
                                                      hidden_names_processed &&
      !in_secondary_trans_unit(base_scope)) {
    /* Get the base class scope's hidden names before trying to clone them
       (unless the scope belongs to a secondary translation unit; it's an
       error to try to put hidden names into such scopes). */
    a_scope_depth		init_depth = depth_scope_stack;
    a_scope_depth		saved_previous_scope;
    a_push_scope_options_set	ps_options = PS_NO_OPTIONS;
    if (base_class->variant.class_struct_union.is_prototype_instantiation) {
      ps_options |= PS_PROTOTYPE_INSTANTIATION;
    } else if (base_class->variant.class_struct_union.is_nonreal_class) {
      /* A nonreal class, which includes local classes of prototype
         instantiations. */
      ps_options |= PS_NONREAL_INSTANTIATION;
    }  /* if */
    push_class_and_template_reactivation_scope_full(
                              base_class, /*is_specialization=*/FALSE,
                              /*reactivate_template_params=*/FALSE,
                              /*extend_namespace=*/FALSE,
                              /*force_new_entry_for_namespace=*/TRUE,
                              ps_options);
    /* Skip scopes that were previously pushed for hidden name processing. */
    saved_previous_scope = scope_stack[init_depth+1].previous_scope;
    scope_stack[init_depth+1].previous_scope = DEPTH_OF_FILE_SCOPE;
    check_name_hiding_for_scope(base_scope);
    scope_stack[init_depth+1].previous_scope = saved_previous_scope;
    pop_class_reactivation_scope();
  }  /* if */
  for (base_hnp = (base_scope != NULL) ? base_scope->hidden_names : NULL;
       base_hnp != NULL;
       base_hnp = base_hnp->next) {
    if (base_hnp->is_class_member) {
      /* It should be innocuous to copy non-member hidden symbols, too:
         either the derived class is in the same scope as, or an inner scope
         of, the scope surrounding the base class, or the base class is a
         class or namespace member (else it could not be named).  In the
         first case, the hidden non-member names will also be hidden (by the
         base class members) in the derived class.  In the second case, those
         names would have to be qualified anyway, so flagging them incorrectly
         as hidden won't matter.

         However, the hidden name list is just that -- a list -- so lookup is
         by linear search.  It seems prudent for performance reasons to
         reduce the size of the list.  This test for class membership will
         still include members of the containing class for a nested base
         class.  However, it seemed a reasonable tradeoff to avoid the
         overhead of checking whether the hidden class member is actually a
         member of a base class of the current class.  That check would be
         an expense for every hidden member, while the performance hit from
         a longer list will probably almost never matter.  If this decision
         proves incorrect, a further test can easily be added. */
      a_hidden_name_ptr derived_hnp;
      for (derived_hnp = derived_scope->hidden_names;
           (derived_hnp != NULL &&
            derived_hnp->entity.ptr != base_hnp->entity.ptr);
           derived_hnp = derived_hnp->next) {}
      if (derived_hnp == NULL) {
        /* The hidden entity is not already in the derived class list:
           create a new entry and copy the base class values to it. */

        a_hidden_name_ptr new_hnp = make_new_hidden_name(derived_scope);
        a_hidden_name_ptr saved_next = new_hnp->next;
        *new_hnp = *base_hnp;
        new_hnp->next = saved_next;
      }  /* if */
    }  /* if */
  }  /* for */
}  /* clone_inherited_hidden_members */


static void check_hiding_by_inherited_names(a_type_ptr  class_type,
                                            a_scope_ptr sp,
                                            a_boolean   top_level)
/*
Perform hidden name checking on the members of each of the base classes of
class_type.  This routine is called recursively, in which case top_level
is FALSE.  When top_level is FALSE, i.e., if class_type is not the most
derived type (i.e., if its IL scope is not the indicated IL scope), do
hidden name checking on its own members, too.
*/
{
  a_base_class_ptr  bcp = base_classes_of(class_type);
  a_symbol_ptr      sym_ptr;

#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4 || db_flag_is_set("dump_hidden")) {
    /* If this is the top-level call, identify class_type. */
    if (bcp != NULL && top_level) {
      fputs("Hidden name check for inherited names of ", f_debug);
      db_type_name(class_type);
      fputc('\n', f_debug);
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* Loop through each of the base classes of class type and perform hidden
     name checking on the inherited members of direct (and, if top-level,
     virtual) base classes. */
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct || (top_level && bcp->is_virtual)) {
      a_type_ptr base_class = bcp->type;
      if (base_class->variant.class_struct_union.is_nonreal_class &&
          base_class->variant.class_struct_union.is_template_class) {
        a_type_ptr proto_inst = base_class->
                        variant.class_struct_union.extra_info->assoc_template->
                                                  prototype_instantiation.type;
        if (proto_inst != NULL && proto_inst != class_type) {
          /* Use the prototype instantiation instead of the nonreal class
             for hiding analysis.  The check that the prototype
             instantiation is not this class is necessary to handle cases
             where a class template names an instance of that template as a
             base class, which would otherwise lead to an infinite
             recursion. */
          base_class = proto_inst;
        }  /* if */
      }  /* if */
      if (!base_class->variant.class_struct_union.extra_info->
                                               base_class_hiding_in_progress) {
        /* Recursively check for hiding by the members of base_class.  The
           check for base_class_hiding_in_progress is needed to prevent
           infinite recursion when checking the prototype instantiations
           of templates like

               template<typename T> struct Base;
               template<typename T> struct Derived : Base<T> { };
               template<typename T> struct Base : Derived<T> { };

           where presumably the recursion during instantiation will be
           broken by an explicit specialization of one of the templates. */
        class_type->variant.class_struct_union.extra_info->
                                          base_class_hiding_in_progress = TRUE;
        check_hiding_by_inherited_names(base_class, sp, /*top_level=*/FALSE);
        class_type->variant.class_struct_union.extra_info->
                                         base_class_hiding_in_progress = FALSE;
      }  /* if */
      if (top_level) {
        /* Copy the base class's hidden member list into this class's list:
           if a member symbol is hidden in the base class, it's hidden here,
           too. */
        clone_inherited_hidden_members(class_type, base_class);
      }  /* if */
    }  /* if */
    if (!bcp->direct && !bcp->type->source_corresp.is_local_to_function) {
      /* The base class may be inaccessible or ambiguous by inheritance, but
         it may be possible to refer to it through qualified access (if it is
         not a local class). */
      check_defeatable_base_inaccessibility(class_type, bcp);
    }  /* if */
#if BACK_END_IS_CP_GEN_BE
    if (gcc_or_clang_is_generated_code_target &&
        bcp->type->source_corresp.is_class_member &&
        find_base_class_of(class_type,
                           parent_class_of(bcp->type)) != NULL) {
      /* g++ incorrectly rejects unqualified references to a base class that
         is nested inside another base class, so mark this as hidden. */
      record_defeatable_name_hiding(symbol_for(bcp->type),
                                    /*tag_hidden_by_nontag=*/FALSE,
                                    /*hidden_class_or_namespace_member=*/TRUE,
                                    /*simulated_hiding=*/FALSE,
                                    sp, /*hidden_by=*/(a_symbol_ptr)NULL);
    }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
  }  /* for */
  if (!top_level) {
    sym_ptr = symbol_supplement_for_class(class_type)->symbols;
    for (; sym_ptr != NULL; sym_ptr = sym_ptr->next_in_scope) {
      /* Skip constructors. */
      if (is_constructor_symbol(sym_ptr)) continue;
#if DEBUG
/* GCOVR_EXCL_START */
      if (debug_level >= 4 || db_flag_is_set("dump_hidden")) {
        if (sym_ptr->decl_position.seq) {
          fputs("  Hidden name check: ", f_debug);
          fprintf(f_debug, "<%s> ", symbol_kind_names[(int)sym_ptr->kind]);
          db_symbol_name(sym_ptr);
          fputc('\n', f_debug);
        }  /* if */
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      /* Unhide the injected class names since they can now be accessed
         using an unqualified name even if that was not so in the base. */
      check_name_unhiding(sym_ptr, sp);
      /* Look up the name in the current scope and record the hiding. */
      record_defeatable_hiding_if_not_same(sym_ptr,
                                           sp,
                                           is_injected_class_symbol(sym_ptr),
                                           /*simulated_hiding=*/FALSE);
      if (sp->kind == (a_scope_kind)sck_class_struct_union) {
        /* Check for ambiguous and inaccessible inherited symbols. */
        a_symbol_locator      locator;
        a_derivation_step_ptr path = NULL;
        an_access_specifier   access;
        a_boolean             ambiguous;
        a_boolean             any_using_decl;
        a_boolean             unambiguous_injected_template;

        clear_locator(&locator, &sym_ptr->decl_position);
        locator.symbol_header = sym_ptr->header;
        if (find_progenitor_symbol(sp->variant.assoc_type, &locator,
                                   IDL_NO_OPTIONS,
                                   /*look_in_dependent_bases=*/TRUE,
                                   !treat_as_cli_class_for_lookup(class_type),
                                   &path, &access, &ambiguous,
                                   &any_using_decl,
                                   &unambiguous_injected_template) != NULL) {
          a_boolean found_using_decl = FALSE;
          if (access == (an_access_specifier)as_inaccessible) {
            /* Check to see if there's a using-declaration in this scope
               that would make the inherited access irrelevant. */
            a_using_decl_ptr udp;
            for (udp = sp->using_declarations;
                 udp != NULL && !found_using_decl;
                 udp = udp->next) {
              a_source_correspondence *sdp;
              if (udp->entity.kind == iek_base_class) {
                a_base_class_ptr udp_bcp = (a_base_class_ptr)udp->entity.ptr;
                sdp = &udp_bcp->type->source_corresp;
              } else {
                sdp = source_corresp_for_il_entry(udp->entity.ptr,
                                                  (an_il_entry_kind)udp->
                                                                  entity.kind);
              }  /* if */
              check_assertion(sdp != NULL);
              if (((a_symbol_ptr)sdp->assoc_info)->header == sym_ptr->header &&
                  udp->qualifier.class_type == class_type) {
                found_using_decl = TRUE;
              }  /* if */
            }  /* for */
          }  /* if */
          if (access == as_inaccessible) {
            /* Check to see if class_type names the current class as a
               friend. */
            for (an_il_entity_list_entry_ptr frp =
                                          class_type_supp(class_type)->friends;
                 frp != NULL; frp = frp->next) {
              if (frp->entity.kind == iek_type &&
                  a_type_ptr(frp->entity.ptr) == sp->variant.assoc_type) {
                /* The current class is a friend, so the symbol is
                   accessible. */
                access = as_public;
                break;
              }  /* if */
            }  /* for */
          }  /* if */
          if (ambiguous || (access == as_inaccessible && !found_using_decl)) {
            /* This symbol is either ambiguous or inaccessible in the class
               whose scope we are processing -- mark it as hidden to force
               references to it in this context to be generated as
               qualified. */
            record_defeatable_name_hiding(
                              sym_ptr, /*tag_hidden_by_nontag=*/FALSE,
                              /*hidden_class_or_namespace_member=*/TRUE,
                              /*simulated_hiding=*/FALSE, sp,
                              (a_symbol_ptr)NULL);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    if (microsoft_bugs && microsoft_version < 1400 &&
        class_type->variant.class_struct_union.extra_info->
                                                   template_arg_list != NULL) {
      /* Microsoft compilers prior to version 8.0 do not inject the name of a
         template instance, so this class does not contain an injected class
         name.  However, to enable the C++-generating back end to generate
         correctly-qualified code for dialects that do inject the template
         name, we need to simulate an injected class name in this case for
         hidden name processing. */
      sym_ptr = (a_symbol_ptr)class_type->source_corresp.assoc_info;
      record_defeatable_hiding_if_not_same(sym_ptr,
                                           sp,
                                           /*sym_is_injected_class=*/TRUE,
                                           /*simulated_hiding=*/TRUE);
    }  /* if */
  }  /* if */
}  /* check_hiding_by_inherited_names */


static void check_name_hiding_of_tag_by_nontag(a_symbol_ptr  sym_ptr,
                                               a_scope_ptr   sp)
/*
sym_ptr is a non-tag name declared in scope sp.  Determine whether there
are any tag declarations in an enclosing scope that are hidden by sym_ptr
and that can be rendered unhidden by being referred to with an elaborated
type specifier when put out by the C++-generating back end.
*/
{
  a_symbol_locator         locator;
  a_symbol_ptr             old_sym_ptr;
  a_boolean                tag_hidden_by_nontag;
  a_boolean                hidden_class_or_namespace_member;

  clear_locator(&locator, &sym_ptr->decl_position);
  locator.symbol_header = sym_ptr->header;
  /* Determine what declarations are hidden by the current declaration. */

  /* First check for hiding that can be resolved with an elaborated type
     specifier -- e.g., when the current declaration is a non-tag name that
     hides a tag name in the same or an enclosing scope and when the current
     declaration is a tag name that is hidden by a non-tag name in the
     current scope. */

  /* The current declaration is of something other than a tag name.
     If it hides a tag in the same scope or an enclosing scope, an
     elaborated type specifier can render the tag visible.  For example:
       class x;
       int x = 1;                   // class x is hidden at file scope
       class y;
       void f() {
         int y = 1;                 // class y is hidden in function f
       }
     An elaborated type specifier can be used to "defeat" the hiding. */
  /* Find the innermost tag declaration with the same name. */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4 || db_flag_is_set("dump_hidden")) {
    fputs("    ...doing tag lookup (tag-nontag)\n", f_debug);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  old_sym_ptr = normal_id_lookup(&locator, IDL_HIDDEN_NAME_LOOKUP |
                                           IDL_MUST_BE_TAG);
  if (old_sym_ptr != NULL) {
    /* old_sym_ptr is a tag with the same name as sym_ptr and in the
       same or a containing scope. */
    a_type_ptr  sym_type;
    if (sym_ptr->kind == (a_symbol_kind)sk_type &&
        typeref_is_typedef(sym_ptr->variant.type.ptr) &&
        (sym_type = skip_typerefs(sym_ptr->variant.type.ptr),
         same_entities(sym_type,
                       old_sym_ptr->variant.class_struct_union.type))) {
      /* sym_ptr is a typedef that refers the type represented by
         old_sym_ptr -- something like "typedef struct S { ... } S;"
         There's no need to generate hidden-name info for this common
         construct. */
    } else if (old_sym_ptr->decl_scope != sym_ptr->decl_scope &&
               (sym_is_class_or_namespace_member(old_sym_ptr) ||
                old_sym_ptr->decl_scope == file_scope_number)) {
      /* No need to defeat the name hiding with an elaborated type
         specifier -- the tag name will be qualified, either by its
         parent class or namespace or by a leading "::".  That's
         handled later. */
    } else {
      /* Either the two declarations are in the same scope or else
         old_sym_ptr cannot be qualified -- e.g., its containing scope is
         local to a function:
           void f() {
             struct S { ... };
             { int S; ... }     // S is hidden in the block scope
           }
      */
      tag_hidden_by_nontag = TRUE;
      hidden_class_or_namespace_member = FALSE;
      record_defeatable_name_hiding(old_sym_ptr, tag_hidden_by_nontag,
                                    hidden_class_or_namespace_member,
                                    /*simulated_hiding=*/FALSE, sp, sym_ptr);
    }  /* if */
  }  /* if */
}  /* check_name_hiding_of_tag_by_nontag */


static void check_name_hiding_of_qualifiable_name(a_symbol_ptr  sym_ptr,
                                                  a_scope_ptr   sp)
/*
sym_ptr is a name declared in scope sp.  Determine whether there are any
declarations in an enclosing scope that are hidden by sym_ptr and that can
be rendered unhidden by being qualified or elaborated when put out by the
C++-generating back end.
*/
{
  a_symbol_locator         locator;
  a_symbol_ptr             old_sym_ptr, tag_sym;
  a_boolean                tag_hidden_by_nontag;
  a_boolean                hidden_class_or_namespace_member;

  clear_locator(&locator, &sym_ptr->decl_position);
  locator.symbol_header = sym_ptr->header;
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4 || db_flag_is_set("dump_hidden")) {
    fputs("    ...doing skip-curr-scope lookup (qualifiable)\n", f_debug);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  old_sym_ptr = normal_id_lookup(&locator, IDL_HIDDEN_NAME_LOOKUP |
                                           IDL_SKIP_CURR_SCOPE);
  if (old_sym_ptr != NULL && is_injected_class_symbol(sym_ptr) &&
      (is_tag_symbol(old_sym_ptr) || is_class_template_symbol(old_sym_ptr))) {
    a_type_ptr    tp = skip_typerefs(type_symbol_type(sym_ptr));
    a_scope_depth init_depth = depth_scope_stack;
    a_boolean     redo_lookup = FALSE;
    a_boolean     pushed_class_scope = FALSE;

    tag_sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
    if (tag_sym != NULL && symbols_are_equivalent(old_sym_ptr, tag_sym)) {
      /* The lookup found the class corresponding to the injected-class-name.
         (This usually happens because a class is defined outside its parent
         class or namespace.)  If this class has a parent, we need to push
         that scope and redo the lookup to see if there is a hidden
         qualifiable symbol further out. */
      if (tp->source_corresp.is_class_member) {
        push_class_reactivation_scope(parent_class_of(tp),
                                      /*extend_namespace=*/FALSE);
        redo_lookup = TRUE;
        pushed_class_scope = TRUE;
      } else if (is_namespace_member(tp)) {
        push_namespace_extension_scope(parent_namespace_of(tp));
        redo_lookup = TRUE;
        pushed_class_scope = FALSE;
      }  /* if */
      if (redo_lookup) {
        /* The class was nested, so there are further scopes to search.
           Skip over the scopes already pushed for hidden name processing so
           we don't find the same symbol again. */
        a_scope_depth saved_previous_scope;
        saved_previous_scope = scope_stack[init_depth+1].previous_scope;
        scope_stack[init_depth+1].previous_scope = DEPTH_OF_FILE_SCOPE;
        clear_specific_symbol(locator);
        old_sym_ptr = normal_id_lookup(&locator, IDL_HIDDEN_NAME_LOOKUP |
                                                 IDL_SKIP_CURR_SCOPE);
        scope_stack[init_depth+1].previous_scope = saved_previous_scope;
        if (pushed_class_scope) {
          pop_class_reactivation_scope();
        } else {
          pop_namespace_extension_scope();
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (old_sym_ptr == NULL) {
    /* There is no symbol that may potentially be hidden. */
  } else if (old_sym_ptr->is_nonreal_member &&
             !(gpp_mode && sym_ptr->kind == (a_symbol_kind)sk_type &&
               sym_ptr->variant.type.ptr->kind == (a_type_kind)tk_typeref &&
               typeref_is_using_decl_alias(sym_ptr->variant.type.ptr))) {
    /* Ignore members of proxy and nonreal classes: they don't correspond to
       actual declarations and therefore cannot be hidden.  The exception
       handles a case like:
         template<typename> struct A {
           struct B { };
           struct C {
             using B = typename A::B;
           };
         };
       In g++ mode, C::B must be recorded as hiding A::B, lest the
       C++-generating back end write that as
         using B = B;
       which, although valid, g++ complains about. */
  } else {
    tag_sym = NULL;
    if (is_injected_class_symbol(sym_ptr) &&
        (is_tag_symbol(old_sym_ptr) ||
         is_class_template_symbol(old_sym_ptr))) {
      tag_sym =
         (a_symbol_ptr)(skip_typerefs(type_symbol_type(sym_ptr))->
                                            source_corresp.assoc_info);
    }  /* if */
    if (tag_sym != NULL && symbols_are_equivalent(old_sym_ptr, tag_sym)) {
      /* sym_ptr does not hide old_sym_ptr -- they represent the same
         declaration. */
    } else if (old_sym_ptr->decl_scope == sym_ptr->decl_scope &&
               !old_sym_ptr->synthesized_namespace_projection) {
      /* Despite the skip-curr-scope lookup, the two symbols were declared in
         the same scope; this can happen as a result of using-declarations.
         Qualification will not help.  Synthesized namespace projection
         symbols are excluded from this test because their decl_scope is not
         meaningful in this context. */
    } else if (old_sym_ptr->is_class_member && sym_ptr->is_template_param) {
      /* A class member is not hidden by a template parameter of the same
         name, e.g.,
           template<typename T> void S<T>::f() {
             ++T;
           }
         is well-formed if T is a data member of S. */
    } else if (old_sym_ptr->decl_scope == file_scope_number ||
               sym_is_class_or_namespace_member(old_sym_ptr) ||
               old_sym_ptr->synthesized_namespace_projection) {
      /* A qualifiable name. */
      tag_hidden_by_nontag = FALSE;
      hidden_class_or_namespace_member = TRUE;
      if (is_tag_symbol(old_sym_ptr) && !is_tag_symbol(sym_ptr) &&
          sym_is_namespace_member(old_sym_ptr) &&
          !has_name_before_mangling(sym_parent_namespace(old_sym_ptr))) {
        /* The symbol is a tag symbol and a member of the unnamed
           namespace; if the current scope is nested within the unnamed
           namespace, that symbol cannot be referred to using a qualified
           name, but an elaborated-type-specifier can be used.  Check to
           see if the unnamed namespace is a parent of this scope. */
        a_scope_ptr unnamed_ns =
                        old_sym_ptr->parent.namespace_ptr->variant.assoc_scope;
        a_scope_ptr sp2;
        for (sp2 = sp; sp2 != NULL && sp2 != unnamed_ns; sp2 = sp2->parent) {}
        if (sp2 != NULL) {
          /* Use an elaborated-type-specifier. */
          tag_hidden_by_nontag = TRUE;
        }  /* if */
      }  /* if */
      record_defeatable_name_hiding(old_sym_ptr, tag_hidden_by_nontag,
                                    hidden_class_or_namespace_member,
                                    /*simulated_hiding=*/FALSE, sp, sym_ptr);
    }  /* if */
    /* Do a similar check for tag names in containing scopes. */
    if (sym_ptr->header->any_tag_decl &&
        !is_tag_symbol(old_sym_ptr) &&
        !is_class_template_symbol(old_sym_ptr)) {
      clear_specific_symbol(locator);
#if DEBUG
/* GCOVR_EXCL_START */
      if (debug_level >= 4 || db_flag_is_set("dump_hidden")) {
        fputs("    ...doing skip-curr-scope tag lookup (qualifiable)\n",
              f_debug);
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      old_sym_ptr = normal_id_lookup(&locator, IDL_HIDDEN_NAME_LOOKUP |
                                               IDL_MUST_BE_TAG |
                                               IDL_SKIP_CURR_SCOPE);
      if (old_sym_ptr != NULL) {
        tag_sym = NULL;
        if (is_injected_class_symbol(sym_ptr)) {
          tag_sym = (a_symbol_ptr)(skip_typerefs(type_symbol_type(sym_ptr))->
                                                    source_corresp.assoc_info);
        }  /* if */
        if ((old_sym_ptr == sym_ptr) ||
            (old_sym_ptr->decl_scope == sym_ptr->decl_scope)) {
          /* This can happen when the scope to which sym_ptr belongs is
             an unnamed namespace. */
        } else if (tag_sym != NULL &&
                   symbols_are_equivalent(old_sym_ptr, tag_sym)) {
          /* sym_ptr does not hide old_sym_ptr -- they represent the same
             declaration. */
        } else {
          if (old_sym_ptr->decl_scope == file_scope_number ||
              sym_is_class_or_namespace_member(old_sym_ptr)) {
            /* old_sym_ptr can point to a type or a class template at this
               point. */
            tag_hidden_by_nontag = is_class_struct_union_symbol(old_sym_ptr);
            if (tag_hidden_by_nontag) {
              /* Check if elaboration is sufficient.  It may not be if the
                 current scope contains a tag of the same name. */
              a_symbol_ptr  local_tag;
              clear_specific_symbol(locator);
              local_tag = normal_id_lookup(&locator, IDL_HIDDEN_NAME_LOOKUP |
                                                     IDL_MUST_BE_TAG);
              hidden_class_or_namespace_member = (local_tag != old_sym_ptr);
            } else {
              /* For class templates, qualification is always required. */
              hidden_class_or_namespace_member = TRUE;
            }  /* if */
            record_defeatable_name_hiding(old_sym_ptr,
                                          tag_hidden_by_nontag,
                                          hidden_class_or_namespace_member,
                                          /*simulated_hiding=*/FALSE, sp,
                                          sym_ptr);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_name_hiding_of_qualifiable_name */


void check_name_hiding_by_parameter(a_symbol_locator *param_locator)
/*
Determine whether the function parameter described by param_locator hides any
declarations in enclosing scopes that can be referred to in succeeding
parameter declarations using an elaborated-type-specifier or a qualified-id.
If so, put corresponding entries on the hidden name list in the current
(function prototype) scope (creating the scope entry if it does not already
exist).
*/
{
  a_symbol_locator         locator;
  a_symbol_ptr             old_sym_ptr;
  a_boolean                tag_hidden_by_nontag;
  a_boolean                hidden_class_or_namespace_member;

  check_assertion(param_locator->symbol_header != NULL);
  if (param_locator->symbol_header->any_tag_decl ||
      param_locator->symbol_header->any_decl_in_file_or_namespace_scope ||
      param_locator->symbol_header->inactive_symbols != NULL) {
    /* There are symbols that might be hidden but able to be named using an
       elaborated-type-specifier or qualified-id. */
    clear_locator(&locator, &param_locator->source_position);
    locator.symbol_header = param_locator->symbol_header;
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 4 || db_flag_is_set("dump_hidden")) {
      fputs("    ...doing skip-curr-scope lookup for parameter\n", f_debug);
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    old_sym_ptr = normal_id_lookup(&locator, IDL_HIDDEN_NAME_LOOKUP |
                                             IDL_SKIP_CURR_SCOPE |
                                             IDL_DO_NOT_ADD_TO_NONREAL_CLASS);
    if (old_sym_ptr == NULL) {
      /* There are no hidden symbols. */
    } else if (old_sym_ptr->is_nonreal_member) {
      /* Ignore members of proxy and nonreal classes: they don't correspond
         to actual declarations and therefore cannot be hidden. */
    } else {
      if (is_class_struct_union_symbol(old_sym_ptr)) {
        /* The entity can be named using an elaborated-type-specifier. */
        tag_hidden_by_nontag = TRUE;
        hidden_class_or_namespace_member = FALSE;
      } else if (old_sym_ptr->decl_scope == file_scope_number ||
                 sym_is_class_or_namespace_member(old_sym_ptr) ||
                 old_sym_ptr->synthesized_namespace_projection) {
        /* The entity can be named using a qualified-id. */
        tag_hidden_by_nontag = FALSE;
        hidden_class_or_namespace_member = TRUE;
      } else {
        /* The entity cannot be named. */
        tag_hidden_by_nontag = FALSE;
        hidden_class_or_namespace_member = FALSE;
      }  /* if */
      if (tag_hidden_by_nontag || hidden_class_or_namespace_member) {
        /* The entity can be named: record the hiding. */
        a_scope_ptr sp;
        check_assertion(scope_stack[depth_scope_stack].kind ==
                                             (a_scope_kind)sck_func_prototype);
        sp = ensure_il_scope_exists(&scope_stack[depth_scope_stack]);
        record_defeatable_name_hiding(old_sym_ptr, tag_hidden_by_nontag,
                                      hidden_class_or_namespace_member,
                                      /*simulated_hiding=*/FALSE, sp,
                                      (a_symbol_ptr)NULL);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_name_hiding_by_parameter */


static void resolve_using_directive_ambiguity(a_symbol_ptr  sym_ptr,
                                              a_scope_ptr   sp)
/*
Determine whether references to the declaration indicated by sym_ptr should
be displayed by the C++-generating back end with a qualifier in order to
resolve ambiguities caused by a using-directive.  For example:
  namespace N { void f(); }
  using namespace N;
  void f();
  void g() { ::f(); }    // don't put out unqualified "f" -- qualifier is
                         // needed to resolve the ambiguity.
*/
{
  a_symbol_locator         locator;
  a_symbol_ptr             old_sym_ptr;
  a_boolean                tag_hidden_by_nontag;
  a_boolean                hidden_class_or_namespace_member;
                                              
  /* Do a lookup and see if a synthesized namespace projection is
     found.  If it is, we can assume that sym_ptr is part of the mix
     (though it may not actually be returned unless a function overload
     set is produced), and if so qualifying sym_ptr will disambiguate
     the reference. */
  clear_locator(&locator, &sym_ptr->decl_position);
  locator.symbol_header = sym_ptr->header;
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4 || db_flag_is_set("dump_hidden")) {
    fputs("    ...doing lookup (using-directive)\n", f_debug);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  (void)normal_id_lookup(&locator, IDL_HIDDEN_NAME_LOOKUP);
  old_sym_ptr = locator.specific_symbol;
  if (old_sym_ptr != NULL &&
      old_sym_ptr->synthesized_namespace_projection) {
    if (old_sym_ptr->ambiguous ||
        old_sym_ptr->kind == (a_symbol_kind)sk_overloaded_function) {
      if (is_tag_symbol(sym_ptr) &&
          old_sym_ptr->kind == (a_symbol_kind)sk_overloaded_function) {
        /* We can use an elaborated-type-specifier to refer to the entity
           without requiring qualification. */
        tag_hidden_by_nontag = TRUE;
        hidden_class_or_namespace_member = FALSE;
      } else {
        /* Use qualification to disambiguate from the namespace member(s). */
        hidden_class_or_namespace_member = TRUE;
        tag_hidden_by_nontag = FALSE;
      }  /* if */
      record_defeatable_name_hiding(sym_ptr, tag_hidden_by_nontag,
                                    hidden_class_or_namespace_member,
                                    /*simulated_hiding=*/FALSE, sp,
                                    (a_symbol_ptr)NULL);
    }  /* if */
  }  /* if */
}  /* resolve_using_directive_ambiguity */


static void check_for_defeatable_name_hiding(
                                 a_symbol_ptr                  sym_ptr,
                                 a_scope_ptr                   sp,
                                 an_active_using_directive_ptr using_directive)
/*
Check whether any declarations are hidden by the declaration associated
with sym_ptr, and if appropriate enter hidden_name_table entries in the
indicated scope.  If using_directive is non-NULL, sym_ptr is visible in the
specified scope via the specified using_directive.
*/
{
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4 || db_flag_is_set("dump_hidden")) {
    if (sym_ptr->decl_position.seq) {
      fputs("Hidden name check: ", f_debug);
      fprintf(f_debug, "<%s> ", symbol_kind_names[(int)sym_ptr->kind]);
      db_symbol_name(sym_ptr);
      fputc('\n', f_debug);
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  if (!is_tag_symbol(sym_ptr) && !is_injected_class_symbol(sym_ptr)) {
    /* See if the current symbol hides a tag symbol. */
    if (sym_ptr->header->any_tag_decl) {
      check_name_hiding_of_tag_by_nontag(sym_ptr, sp);
    }  /* if */
  }  /* if */
  if (sym_ptr->header->any_decl_in_file_or_namespace_scope ||
      sym_ptr->header->inactive_symbols != NULL) {
    /* There are qualifiable symbols that share this name. */
    if (sp->kind != (a_scope_kind)sck_file &&
        (sp->kind != (a_scope_kind)sck_namespace ||
         sp->variant.assoc_namespace->source_corresp.name != NULL ||
         is_namespace_member(sp->variant.assoc_namespace))) {
      /* A declaration in the current scope may hide a declaration from the
         file scope or a namespace scope.  If so, the hidden name may be
         rendered visible by qualification.  Conversely, a few situations may
         "unhide" such a name. */
      if (using_directive == NULL) {
        /* Names can be "unhidden" for injected class names and for block
           extern declarations.  Neither of these applies when the hiding
           symbol is the result of a using-directive. */
        check_name_unhiding(sym_ptr, sp);
      }  /* if */
      check_name_hiding_of_qualifiable_name(sym_ptr, sp);
    }  /* if */
  }  /* if */
#if DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
  if (record_form_of_name_reference && using_directive != NULL &&
      is_tag_symbol(sym_ptr)) {
    /* Check for the case where a non-tag name in the namespace of the
       using-directive hides the name of a tag type in the target
       namespace, e.g.,
         namespace A { }
         namespace N { struct A; }
         using namespace N;   // struct N::A hidden by namespace A
         struct ::A *p;
       Here the elaborated type specifier is needed because ::A designates
       the namespace and not N::A.  (This would ordinarily be handled by
       name qualification, "N::A", but when a trk_name_qualifier overrides
       the required qualification, an elaborated-type-specifier is
       required.) */
    for (a_symbol_ptr old_sym_ptr = sym_ptr->header->inactive_symbols;
         old_sym_ptr != NULL; old_sym_ptr = old_sym_ptr->next) {
      a_boolean use_elab_type_spec = FALSE;
      if (!is_tag_symbol(old_sym_ptr)) {
        if (old_sym_ptr->decl_scope == sp->number) {
          /* The non-tag entity is declared in the same scope as the tag
             entity. */
          use_elab_type_spec = TRUE;
        } else {
          /* Check if both the tag and the non-tag are made visible by
             parallel using-directives. */
          for (an_active_using_directive_ptr audp =
                        scope_stack[depth_scope_stack].active_using_directives;
               !use_elab_type_spec && audp != NULL; audp = audp->next) {
            if (audp != using_directive &&
                audp->scope_depth_at_which_using_directive_applies ==
                  using_directive->
                                scope_depth_at_which_using_directive_applies) {
              for (a_symbol_ptr member_sym = audp->namespace_supplement->
                                                        pointers_block.symbols;
                   !use_elab_type_spec && member_sym != NULL;
                   member_sym = member_sym->next_in_scope) {
                if (member_sym == old_sym_ptr) {
                  use_elab_type_spec = TRUE;
                }  /* if */
              }  /* for */
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
      if (use_elab_type_spec) {
        /* The hiding can be defeated using an elaborated-type-specifier. */
        record_defeatable_name_hiding(
                                    sym_ptr, /*tag_hidden_by_nontag=*/TRUE,
                                    /*hidden_class_or_namespace_member=*/FALSE,
                                    /*simulated_hiding=*/FALSE, sp,
                                    old_sym_ptr);
        break;
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */
}  /* check_for_defeatable_name_hiding */


static void check_name_hiding_by_template_parameters(a_scope_ptr  sp)
/*
If the given scope is a prototype instantiation, check whether some of the
template parameters may hide other entities.  If so, record that in the hidden
name table.
*/
{
  a_template_param_ptr  param = NULL;

  if (sp->kind == (a_scope_kind)sck_class_struct_union) {
    a_type_ptr  type = sp->variant.assoc_type;
    if (is_immediate_class_type(type) &&
        type->variant.class_struct_union.is_prototype_instantiation &&
        !type->variant.class_struct_union.is_in_class_specialization) {
      a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(type);
      check_assertion(cssp != NULL && cssp->template_info != NULL);
      param = cssp->template_info->cache->decl_info->parameters;
    }  /* if */
  } else if (sp->kind == (a_scope_kind)sck_function) {
    a_routine_ptr  routine = sp->variant.routine.ptr;
    if (routine->is_prototype_instantiation &&
        !routine->source_corresp.is_local_to_function &&
        !routine->compiler_generated &&
        !routine->is_lambda_body) {
      /* Note that member functions of local classes of prototype
         instantiations are also marked as prototype instantiations
         (but they don't have template parameters to worry about).
         The same goes for compiler-generated members (including lambda
         conversion functions) and for non-local lambda bodies (these can
         occur in a prototype instantiation in contexts such as static data
         member initializers). */
      a_symbol_ptr  sym = (a_symbol_ptr)routine->source_corresp.assoc_info;
      a_template_instance_ptr
                    instance = sym->variant.routine.instance_ptr;
      if (instance != NULL && instance->template_info != NULL) {
        param = instance->template_info->cache->decl_info->parameters;
      }  /* if */
    }  /* if */
  }  /* if */
  for (; param != NULL; param = param->next) {
    a_symbol_ptr  param_sym = param->param_symbol;
    if (param_sym->is_error) {
      /* Ignore invalid parameters. */
    } else if (*param_sym->header->identifier == '<') {
      /* This is an unnamed parameter with header identifier "<unnamed>"
         or "<auto-N>" (where N is a decimal number). */
      check_assertion(
                 strncmp(param_sym->header->identifier, "<unnamed>", 9) == 0 ||
                 strncmp(param_sym->header->identifier, "<auto-", 6) == 0);
    } else {
      check_for_defeatable_name_hiding(param_sym, sp,
                                       /*using_directive=*/NULL);
    }  /* if */
  }  /* for */
}  /* check_name_hiding_by_template_parameters */


static a_boolean symbol_is_candidate_for_hiding(a_symbol_ptr sym)
/*
Returns TRUE if the specified symbol is capable of hiding symbols in the same
or surrounding scopes.
*/
{
  a_boolean is_candidate = FALSE;

  if (sym->kind == (a_symbol_kind)sk_extern_routine ||
      sym->kind == (a_symbol_kind)sk_extern_variable) {
    /* Ignore extern symbols. */
  } else if (sym->is_error) {
    /* Ignore error symbols. */
  } else if (sym->kind == (a_symbol_kind)sk_macro) {
    /* Ignore macros. */
  } else if (!sym->decl_position.seq) {
    /* Ignore compiler-generated symbols for predeclared entities. */
  } else if (is_unnamed_tag_symbol(sym)) {
    /* No name hiding by unnamed symbols. */
  } else if (sym->is_invisible) {
    /* A friend declaration doesn't affect lookup until it's actually
       declared in the scope to which it belongs -- so if it's invisible,
       it can't hide other declarations. */
  } else if (is_template_class_symbol(sym)) {
    /* The template itself belongs to the same scope -- one check for a
       given name is sufficient. */
  } else if (sym->kind == (a_symbol_kind)sk_projection) {
    /* Ignore inherited names -- they are handled separately. */
  } else {
    /* Only the overload symbol should be on the scope list. */
    check_assertion(!sym->overload_set_member);
    /* Falling through to here means the symbol should be checked to see
       if this declaration hides another declaration. */
    is_candidate = TRUE;
  }  /* if */
  return is_candidate;
}  /* symbol_is_candidate_for_hiding */
                       
void check_name_hiding_for_scope(a_scope_ptr  sp)
/*
Check for symbols declared in the specified scope that hide names declared
in the same scope or an enclosing scope and, when appropriate, create hidden
name table entries to describe the hiding when it can be defeated in the
C++-generating back end by using a qualifier or elaborated type specifier.
In C++ mode this routine is called recursively for namespace scopes and
class scopes.  It is called directly from pop_scope for function and block
scopes and for the file scope.
*/
{
  a_scope_stack_entry_ptr       ssep;
  a_namespace_ptr               nsp;
  a_type_ptr                    tp;
  a_symbol_ptr                  sym_list;
  a_symbol_ptr                  sym;
  an_active_using_directive_ptr audp;
  a_scope_depth                 saved_depth_of_initial_lookup_scope;

  db_enter(3, "check_name_hiding_for_scope");
  if (sp == NULL ||
      (sp->kind == (a_scope_kind)sck_class_struct_union &&
       sp->variant.assoc_type->variant.class_struct_union.extra_info->
                                                     hidden_names_processed)) {
    /* Nothing to do. */
  } else {
    if (sp->kind == (a_scope_kind)sck_class_struct_union) {
      /* Immediately mark the class as processed, even though we haven't done
         anything yet, to avoid infinite recursion.  (This can occur when an
         explicit specialization of a nested class template has the containing
         class as a base, for instance.) */
      sp->variant.assoc_type->variant.class_struct_union.extra_info->
                                                 hidden_names_processed = TRUE;
    }  /* if */
    /* Check certain nested scopes first. */
    if (!C_mode()) {
      /* If this is the file scope or a namespace scope, there may be nested
         namespaces.  Do checking for the nested scope before proceeding to
         the symbols that were declared in the current scope. */
      for (nsp = sp->namespaces; nsp != NULL; nsp = nsp->next) {
        if (!nsp->is_namespace_alias) {
          push_namespace_extension_scope(nsp);
          check_name_hiding_for_scope(nsp->variant.assoc_scope);
          pop_namespace_extension_scope();
        }  /* if */
      }  /* for */
      /* Similarly, do checking for class scopes defined within the current
         scope before proceeding to the symbols that were declared in the
         current scope. */
      for (tp = sp->types; tp != NULL; tp = tp->next) {
        if (is_immediate_class_type(tp)) {
          a_scope_ptr  scope_ptr = class_type_supp(tp)->assoc_scope;
          /* Exclude classes with no body and classes with no associated symbol
             (e.g., generated by lowering). */
          if (scope_ptr != NULL && symbol_for(tp) != NULL) {
            push_class_reactivation_scope(tp, /*extend_namespace=*/FALSE);
            check_name_hiding_for_scope(scope_ptr);
            pop_class_reactivation_scope();
            check_hiding_by_inherited_names(tp, scope_ptr,
                                            /*top_level=*/TRUE);
          }  /* if */
        }  /* if */
      }  /* for */
      /* There may be using-directives in this scope.  If so, process the
         symbols in the referenced namespaces for hiding before processing
         the symbols in this scope. */
      saved_depth_of_initial_lookup_scope = depth_of_initial_lookup_scope;
      for (audp = scope_stack[depth_scope_stack].active_using_directives;
           audp != NULL; audp = audp->next) {
        /* First, process the symbols in the namespace named in the
           using-directive to allow them to hide symbols in scopes surrounding
           the scope at which the using-directive applies. */
        depth_of_initial_lookup_scope =
                            audp->scope_depth_at_which_using_directive_applies;
        for (sym = audp->namespace_supplement->pointers_block.symbols;
             sym != NULL; sym = sym->next_in_scope) {
          if (symbol_is_candidate_for_hiding(sym)) {
            check_for_defeatable_name_hiding(sym, sp, audp);
          }  /* if */
        }  /* for */
        /* Next, process the symbols in the namespace at which the
           using-directive applies to flag them for ambiguity with symbols
           from the namespace in the using-directive. */
        ssep = &scope_stack[depth_of_initial_lookup_scope];
        for (sym = assoc_pointers_block_of(ssep)->symbols; sym != NULL;
             sym = sym->next_in_scope) {
          if (symbol_is_candidate_for_hiding(sym)) {
            resolve_using_directive_ambiguity(sym, sp);
          }  /* if */
        }  /* for */
      }  /* for */
      depth_of_initial_lookup_scope = saved_depth_of_initial_lookup_scope;
    }  /* if */
    /* Find the list of symbols declared in the current scope. */
    switch (sp->kind) {
      case sck_function:
        check_name_hiding_by_template_parameters(sp);
        FALLTHROUGH
      case sck_file:
      case sck_block:
        ssep = &scope_stack[depth_scope_stack];
        sym_list = assoc_pointers_block_of(ssep)->symbols;
        break;
      case sck_namespace:
        nsp = sp->variant.assoc_namespace;
        sym_list = symbol_supplement_for_namespace(nsp)->
                                             pointers_block.symbols;
        break;
      case sck_class_struct_union:
        check_name_hiding_by_template_parameters(sp);
        tp = sp->variant.assoc_type;
        sym_list = symbol_supplement_for_class(tp)->symbols;
        if (microsoft_bugs && microsoft_version < 1400 &&
            tp->variant.class_struct_union.extra_info->
                                                   template_arg_list != NULL) {
          /* Microsoft compilers prior to version 8.0 do not inject the name
             of a template instance, so this class does not contain an
             injected class name.  However, to enable the C++-generating
             back end to generate correctly-qualified code for dialects that
             do inject the template name, we need to simulate an injected
             class name in this case for hidden name processing. */
          record_defeatable_hiding_if_not_same((a_symbol_ptr)tp->
                                                     source_corresp.assoc_info,
                                               sp,
                                               /*sym_is_injected_class=*/TRUE,
                                               /*simulated_hiding=*/TRUE);
        }  /* if */
        break;
      default:
#if CHECKING
        unexpected_condition();
#else /* !CHECKING */
        sym_list = NULL;
#endif /* CHECKING */
    }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
    if (debug_level >= 4 || db_flag_is_set("dump_hidden")) {
      if (sym_list != NULL) {
        fputs("Checking hidden names declared in ", f_debug);
        db_scope(sp);
        fputs("\n", f_debug);
      }  /* if */
    }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    /* Traverse the symbols declared in the current scope. */
    for (sym = sym_list; sym != NULL; sym = sym->next_in_scope) {
      if (symbol_is_candidate_for_hiding(sym)) {
        check_for_defeatable_name_hiding(sym, sp, /*using_directive=*/NULL);
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
}  /* check_name_hiding_for_scope */
  
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS

static void sym_update_source_sequence_list(
                                  a_symbol_ptr                 sym,
                                  a_source_position            *pos,
                                  a_boolean                    is_primary_decl,
                                  a_source_sequence_entry_ptr  old_ssep)
/*
Allocate a source sequence entry for the IL entry to which sym refers and
add it to the list for the appropriate scope.  If is_primary_decl is TRUE
this is the primary declaration of the IL entity, and any existing source
sequence entry already bound to the IL entity should be demoted to
secondary status.
*/
{
  char                          *il_entry_ptr;
  an_il_entry_kind              kind;
  a_src_seq_secondary_decl_ptr  sssdp;
  a_boolean                     force_alloc_in_filescope;
  a_memory_region_number        region_to_switch_back_to;

  if (!source_sequence_entries_disallowed) {
    /* We are in a context in which source sequence entries are being
       generated. */
    if ((il_entry_ptr = il_entry_for_symbol_null_okay(sym, &kind)) != NULL) {
      if (pos->seq == 0 ||
          (kind == iek_routine &&
           ((a_routine_ptr)il_entry_ptr)->compiler_generated)) {
        /* Don't put out source sequence information on compiler generated
           functions. */
      } else {
        if (!is_primary_decl) {
          /* This is not a primary declaration, so put out an entry for a
             secondary declaration. */
          /* Switch to the file scope memory region to allocate the new entry
             if necessary. */
          if (curr_il_region_number != file_scope_region_number &&
              in_file_scope(il_entry_ptr)) {
            force_alloc_in_filescope = TRUE;
            switch_to_file_scope_region(&region_to_switch_back_to);
          } else {
            force_alloc_in_filescope = FALSE;
          }  /* if */
          sssdp = alloc_src_seq_secondary_decl();
          if (force_alloc_in_filescope) {
            switch_back_to_original_region(region_to_switch_back_to);
          }  /* if */
          /* Set the fields. */
          sssdp->decl_position = *pos;
          sssdp->entity.kind = kind;
          sssdp->entity.ptr = il_entry_ptr;
          /* Change the parameter values accordingly. */
          kind = iek_src_seq_secondary_decl;
          il_entry_ptr = (char *)sssdp;
          if (scope_stack[depth_scope_stack].kind ==
                                       (a_scope_kind)sck_func_prototype) {
            sssdp->declared_in_func_prototype = TRUE;
          }  /* if */
        }  /* if */
        f_update_source_sequence_list(il_entry_ptr, kind, old_ssep);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* sym_update_source_sequence_list */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

void record_symbol_declaration(
                       a_symbol_reference_kind                srk_flags,
                       a_symbol_ptr                           sym_ptr,
                       a_source_position                      *source_position,
                       ARG_UNUSED a_source_sequence_entry_ptr ssep)
/*
Record information about how the given symbol is declared.  The srk_flags
parameter describes the specificity of the declaration (e.g., that it is a
definition, a friend declaration, etc.).  The source position will be recorded
in the symbol as its "decl_position" (overwriting what's there already, if
this is a definition); the same will be done for the decl_position field of
the IL entry.  A cross-reference entry will be put out.  If source-sequence
entries are being generated, ssep may point to an "empty" entry already
created for this entity; otherwise, it is NULL.
*/
{
  a_boolean                is_definition = (srk_flags & SRK_DEFINITION) != 0;
  a_boolean                is_tentative_def =
                                         (srk_flags & SRK_TENTATIVE_DEF) != 0;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean                is_primary_decl = FALSE;
  a_boolean                set_first_decl_flag = FALSE;
  a_boolean                update_src_seq_list = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_source_correspondence  *scptr = NULL;

  if (is_definition) {
    a_boolean  is_def_with_prior_init;
    is_def_with_prior_init =
           !(srk_flags & SRK_INITIALIZATION) &&
           symbol_is(sym_ptr, sk_static_data_member) &&
           sym_ptr->variant.static_data_member.variable->initializer_in_class;
    if (sym_ptr->defined) {
      /* This is a redefinition -- allowed for C variables at file scope, for
         macros, and for C++ typedefs. */
      if (C_mode() && sym_ptr->kind == (a_symbol_kind)sk_variable &&
          !is_tentative_def) {
        /* This must be an initializing definition following a tentative
           definition. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        is_primary_decl = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      } else {
        /* A definition is ordinarily the primary declaration, but if it was
           previously defined and this is just a redefinition, this should be
           recorded as a secondary declaration. */
        is_definition = FALSE;
      }  /* if */
    } else {
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (is_def_with_prior_init) {
        /* A static data member definition outside a class is not a primary
           declaration if the in-class declaration included an initializer. */
      } else {
        is_primary_decl = !is_tentative_def;
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      sym_ptr->defined = TRUE;
    }  /* if */
    if (is_definition && !is_def_with_prior_init) {
      /* The source correspondence of an IL entry generally records the
         position of the (possibly tentative) definition.  Two exceptions
         are: (1) some redefinitions (for which the is_definition flag will
         have been cleared above), and (2) out-of-class static data member
         definitions associated with an in-class initializer. */
      sym_ptr->decl_position = *source_position;
      scptr = source_corresp_entry_for_symbol(sym_ptr);
      if (scptr != NULL) {
        if (is_template_symbol(sym_ptr)) {
          /* The decl_position field in the IL template entry is not
             updated. */
        } else {
          scptr->decl_position = *source_position;
        }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
        if (scptr->decl_pos_info == NULL) {
          scptr->decl_pos_info =
                       alloc_decl_position_supplement(in_file_scope(scptr));
        } else if (is_template_symbol(sym_ptr)) {
          /* The decl-position-supplement in the IL template entry should not
             be modified. */
        } else {
          clear_decl_position_supplement(scptr->decl_pos_info);
        }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }  /* if */
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  } else if (srk_flags & SRK_INITIALIZATION) {
    /* In-class static data members can have an initializer without being
       defined.  That initialized declaration is the primary declaration. */
    is_primary_decl = !is_tentative_def;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  /* Set declaration sequence numbers based on the first declaration of the
     name, regardless of whether it is the primary declaration or not. */
  if (sym_ptr->decl_seq == 0) set_decl_sequence_number(sym_ptr);
  /* Update the cross reference file if it exists. */
  if (f_xref_info != NULL) {
    /* If writing cross-reference information, write an entry for this
       declaration. */
    write_xref_entry(srk_flags, sym_ptr, source_position);
  }  /* if */
#if MAINTAIN_CLASS_MEMBER_LIST
  if ((srk_flags & SRK_TEMPLATE_INSTANTIATION) == 0) {
    /* Record the declaration if it appears in the body of a class.  A
       template instantiation is not recorded even though it can be generated
       while the body of the class that encloses it is being processed:  It
       does not correspond to a declaration written in that body. */
    record_class_member_symbol_declaration(sym_ptr, source_position);
  }  /* if */
#endif /* MAINTAIN_CLASS_MEMBER_LIST */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Issue a source sequence entry -- unless this is a label definition.  (A
     label is always defined by an stmk_label statement, for which a source
     sequence entry will be put out; putting out both would be redundant.)
     Also, sk_parameter symbols are not yet bound to a variable, so there's
     no way to put out a source sequence entry yet. */
  if ((sym_ptr->kind != (a_symbol_kind)sk_label || !is_definition) &&
      sym_ptr->kind != (a_symbol_kind)sk_parameter) {
    /* Determine whether the field definition_is_first_decl should be set
       for a class symbol.  Note that it may be changed later, if a source
       sequence entry is generated to represent a forward declaration of
       the class. */
    if (is_definition && is_class_struct_union_symbol(sym_ptr) &&
        scptr != NULL && scptr->source_sequence_entry == NULL) {
      set_first_decl_flag = TRUE;
    }  /* if */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    if (sym_ptr->kind == (a_symbol_kind)sk_member_function ||
        sym_ptr->kind == (a_symbol_kind)sk_routine) {
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
      if (is_definition && !C_mode() &&
          scope_is(&scope_stack_top(), sck_class_struct_union)) {
        a_type_ptr     class_type = scope_stack_top().assoc_type;
        a_routine_ptr  rp = sym_ptr->variant.routine.ptr;
        if (!scope_stack_top().inside_local_class &&
            !class_type->variant.class_struct_union.is_nonreal_class &&
            !rp->is_defaulted && !rp->is_deleted &&
            !special_kind_is(rp, sfk_deduction_guide) &&
#if MICROSOFT_EXTENSIONS_ALLOWED
            !(microsoft_mode &&
              microsoft_routine_def_is_unmovable(
                                         rp->overridden_functions != NULL)) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          class_type_can_be_named_in_namespace_scope(class_type)) {
          /* This is a member or friend function definition inside the
             definition of a nonlocal class that is not a prototype
             instantiation.  When template instantiations are put out in the
             source sequence list, it is necessary to move the member or friend
             definition outside the class definition (i.e., just after it).
             That means a secondary source sequence entry should be put out
             here.  (This cannot be done with defaulted functions, since
             out-of-class and in-class definitions are not equivalent.
             Similarly, deleted functions must always be defined inside the
             class definition.) */
          is_primary_decl = FALSE;
        }  /* if */
      }  /* if */
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    } else if (is_class_struct_union_symbol(sym_ptr) && !C_mode()) {
      if (is_definition) {
        if (scptr != NULL && scptr->source_sequence_entry == NULL) {
          /* This is the initial declaration of this class.  If appropriate,
             put out a secondary declaration entry immediately before the
             definition entry to deal with forward reference problems of
             instantiations that are inserted in front of the class; if
             there is no insertion, the entry will be removed eventually. */
          a_boolean                     add_secondary_decl;
          a_type_ptr                    class_type = type_symbol_type(sym_ptr);
          an_il_entry_kind              kind;
          a_class_type_supplement_ptr   parent_ctsp;
          a_src_seq_secondary_decl_ptr  sssdp;

          if (scptr->is_local_to_function ||
              class_type->variant.class_struct_union.originally_unnamed) {
            /* Can't be done for local functions.  It cannot be done for
               classes that got their name through a typedef either because
               the following is not legal: struct X; typedef struct {} X; */
            add_secondary_decl = FALSE;
          } else if (!scptr->is_class_member) {
            /* Okay for all classes that aren't nested. */
            add_secondary_decl = TRUE;
          } else {
            /* It's okay for nested classes only if it's a deferred
               definition (i.e., the definition appears at file or
               namespace scope). */
            parent_ctsp = class_type_supp(scp_parent_class(scptr));
            add_secondary_decl = (parent_ctsp->assoc_scope->
                                     depth_in_scope_stack == NO_SCOPE_DEPTH);
          }  /* if */
          if (add_secondary_decl) {
            kind = (an_il_entry_kind)iek_type;
            sssdp = make_source_sequence_secondary_decl((char *)class_type,
                                                        kind, class_type);
            sssdp->autonomous_tag_decl = TRUE;
            sssdp->decl_position = *source_position;
            sssdp->compiler_generated_forward_decl = TRUE;
            sssdp->first_declaration = TRUE;
            set_first_decl_flag = FALSE;
            if (class_type->variant.class_struct_union.is_template_class) {
#if BACK_END_IS_CP_GEN_BE
              sssdp->specialized_with_new_syntax =
                            !old_specializations_for_generated_instances;
#else /* !BACK_END_IS_CP_GEN_BE */
              sssdp->specialized_with_new_syntax = TRUE;
#endif /* BACK_END_IS_CP_GEN_BE */
            }  /* if */
            kind = (an_il_entry_kind)iek_src_seq_secondary_decl;
            add_to_source_sequence_list((char *)sssdp, kind);
          }  /* if */
        }  /* if */
      } else if (is_nonspecialized_instantiation_context()) {
        /* Not a definition.  If this is a nested class declaration inside
           a class template instantiation, only put out the declaration if
           it's the first.  This is to deal with the following case:
             template <class T> class A {
               class N;
               class N { ... };
             };
             A<int> x;
           where the instantiation of A<int> would be represented in the
           generated C++ as a specialization:
             template<> class A<int> {
               class N;
               class N;                // declaration substitutes for
             };                        //    inline definition
             class A<int>::N { ... }   // if needed
           The problem is that a second declaration of class N is disallowed
           (9.2 paragraph 1 of the C++ standard).  The solution is to suppress
           any declaration of a nested class other than the first (except for
           friend declarations referring to the nested class). */
        a_scope_stack_entry_ptr scope_stack_ptr =
                                         &scope_stack[depth_scope_stack];
        if (!scope_stack_ptr->in_prototype_instantiation &&
            scope_stack_ptr->kind == (a_scope_kind)sck_class_struct_union &&
            sym_ptr->is_class_member &&
            sym_parent_class(sym_ptr) == scope_stack_ptr->assoc_type) {
          scptr = source_corresp_entry_for_symbol(sym_ptr);
          if (scptr != NULL && scptr->source_sequence_entry != NULL &&
              !(srk_flags & SRK_FRIEND)) {
            update_src_seq_list = FALSE;
          }  /* if */
        }  /* if */
      }  /* if */
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    if (srk_flags & SRK_TEMPLATE_INSTANTIATION) {
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      if (srk_flags & SRK_DEFINITION) {
        /* This is a configuration where source sequence entries must be
           emitted for full template instantiations. */
      } else
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
      /* Do not insert code here. */
      {
        update_src_seq_list = FALSE;
      }  /* if */
    }  /* if */
    if (update_src_seq_list) {      /*lint !e774*/
      if (is_definition) {
        /* If this is a primary declaration (or a tentative definition that
           is the first definition of the variable), erase the previous
           source sequence entry bound to this entity (if any).  Do not do
           this if source sequence entries are currently disallowed (since
           the pointer wouldn't be updated by sym_update_source_sequence_list
           in that case). */
        if (scptr != NULL && !source_sequence_entries_disallowed) {
          scptr->source_sequence_entry = NULL;
        }  /* if */
        if (set_first_decl_flag) {
          class_symbol_supp(sym_ptr)->definition_is_first_decl = TRUE;
        }  /* if */
      }  /* if */
      sym_update_source_sequence_list(sym_ptr, source_position,
                                      is_primary_decl, ssep);
    }  /* if */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* record_symbol_declaration */


void check_use_of_deprecated_or_unavailable_entity(
                                              a_source_correspondence_ptr scp,
                                              a_source_position           *pos)
/*
The entity represented by the given source correspondence is referenced
at the given position.  Issue a warning if the entity was declared with
the "deprecated" attribute and an error if it was declared with the
"unavailable" attribute.
*/
{
  if (scp->is_deprecated_or_unavailable &&
      !in_deprecated_or_unavailable_definition()) {
    a_symbol_ptr      sym = (a_symbol_ptr)scp->assoc_info;
    an_error_severity sev;
    an_attribute_ptr  ap, dap = NULL;
    an_error_code     diag_str, diag_no_str, diag_more;
    check_assertion(sym != NULL);
    for (ap = scp->attributes; ap != NULL; ap = ap->next) {
      if (ap->kind == ak_unavailable) {
        dap = ap;
        break;
      } else if (ap->kind == ak_deprecated && dap == NULL) {
        dap = ap;
        /* Continue searching in case we find an ak_unavailable attribute
           (which triggers an error instead of a warning). */
      }  /* if */
    }  /* for */
    check_assertion(dap != NULL);
    if (dap->kind == ak_unavailable) {
      sev = es_error;
      diag_str = ec_unavailable_entity_with_custom_message;
      diag_no_str = ec_unavailable_entity;
      diag_more = ec_unavailable_attr;
    } else {
      sev = es_warning;
      diag_str = ec_deprecated_entity_with_custom_message;
      diag_no_str = ec_deprecated_entity;
      diag_more = ec_deprecated_attr;
    }  /* if */
    a_const_char      *str = attribute_string_for_kind(dap->kind, scp);
    a_diagnostic_ptr  dp;
    a_diag_list       diag_list;
    if (str != NULL) {
      dp = pos_stsy_start_diagnostic(sev, diag_str, pos, str, sym);
    } else {
      dp = pos_sy_start_diagnostic(sev, diag_no_str, pos, sym);
    }  /* if */
    if (cmp_source_positions(dap->position, null_source_position) != 0) {
      clear_diag_list(&diag_list);
      more_info_diagnostic(diag_more, &dap->position, &diag_list);
      add_more_info_list(dp, &diag_list);
    }  /* if */
    end_diagnostic(dp);
  }  /* if */
}  /* check_use_of_deprecated_or_unavailable_entity */


a_boolean check_use_of_deleted_function(a_symbol_ptr      rout_sym,
                                        a_boolean         elided_ref,
                                        a_source_position *pos)
/*
A reference is being made to the indicated function.  It is not necessarily a
call: it might be in unevaluated code, it might be taking the address of the
function, it might be to an elided copy constructor, etc.  If the function is
declared as deleted ("= delete") return FALSE, and, if pos is non-NULL, issue
an error at the position *pos.  elided_ref is TRUE if the reference is to an
elided copy constructor.
*/
{
  a_routine_ptr rout;
  a_boolean     err = FALSE;

  check_assertion(is_simple_function_symbol(rout_sym));
  rout = rout_sym->variant.routine.ptr;
  if (rout->is_deleted) {
    an_error_severity sev = es_error;
    an_error_code     err_code;
    if (elided_ref && !clang_mode && !gpp_mode) {
      sev = strict_ansi_discretionary_severity;
    }  /* if */
    if (special_kind_is(rout, sfk_constructor) &&
        (is_default_constructor(rout, /*is_declarative_context=*/FALSE) ||
         rout->is_inheriting_ctor)) {
      /* Use a specific message for a default constructor.  This is clearer
         when the class is unnamed, as for a lambda. */
      if (rout->is_inheriting_ctor) {
        err_code = ec_deleted_inh_def_constructor;
      } else {
        err_code = ec_deleted_default_constructor;
      }  /* if */
      /* If we are not issuing an error, call the "sfinae" version of the
         routine that can potentially ignore overridden severities. */
      if (pos == NULL) {
        err = is_effective_sfinae_error(err_code, sev, &error_position);
      } else {
        err = is_effective_error(err_code, sev, pos);
        pos_ty_diagnostic(sev, err_code, pos, parent_class_of(rout));
      }  /* if */
    } else {
      err_code = elided_ref ? ec_deleted_elided_cctor : ec_deleted_function;
      /* If we are not issuing an error, call the "sfinae" version of the
         routine that can potentially ignore overridden severities. */
      if (pos == NULL) {
        err = is_effective_sfinae_error(err_code, sev, &error_position);
      } else {
        err = is_effective_error(err_code, sev, pos);
        pos_sy_diagnostic(sev, err_code, pos, rout_sym);
      }  /* if */
    }  /* if */
  }  /* if */
  return !err;
}  /* check_use_of_deleted_function */


static void record_first_use_if_template(
				a_symbol_ptr		sym_ptr,
				a_source_position	*source_position)
/*
This routine is called when the referenced flag is not yet set on the symbol.
If sym_ptr is a nonspecialized template function or template static data member
the position of the first reference is recorded.
*/
{
  a_template_instance_ptr		tip = NULL;

  if (is_function_symbol(sym_ptr)) {
    a_routine_ptr rp = sym_ptr->variant.routine.ptr;
    if (rp->is_template_function && !rp->is_specialized) {
      tip = sym_ptr->variant.routine.instance_ptr;
    }  /* if */
  } else if (symbol_is(sym_ptr, sk_static_data_member) ||
             symbol_is(sym_ptr, sk_variable)) {
    a_variable_ptr vp = variable_for_symbol(sym_ptr);
    if (vp->is_template_variable && !vp->is_specialized) {
      tip = template_instance_for_symbol(sym_ptr);
    }  /* if */
  }  /* if */
  /* tip will be set if this is a template function or template
     static data member for which a position should be recorded. */
  if (tip != NULL) {
    tip->pos_of_first_reference = *source_position;
  }  /* if */
}  /* record_first_use_if_template */


void record_symbol_reference_full(a_symbol_reference_kind kind,
                                  a_symbol_ptr            sym_ptr,
                                  a_source_position       *source_position,
                                  a_boolean               update_il_entry,
                                  a_source_correspondence *specific_il_entry)
/*
Record a reference of the indicated kind to the indicated symbol.  Set the
referenced flag in the symbol entry.  If update_il_entry is TRUE, also
set the referenced flag in the associated IL entry, if any.  Mark the
symbol "used" or "set", if appropriate.  sym_ptr should not point to a
projection symbol.  If specific_il_entry is non-NULL, use the associated
IL entry in place of whatever is pointed to by the symbol.
*/
{
  a_source_correspondence *scptr;
  a_symbol_kind           sym_kind = sym_ptr->kind;
 
  check_assertion_str(sym_kind != (a_symbol_kind)sk_projection &&
                      sym_kind != (a_symbol_kind)sk_namespace_projection,
                      "record_symbol_reference_full: projection symbol");
  /* Don't record references during disambiguation.  They will be recorded
     during the normal processing, so recording them here would result in
     duplicate entries. */
  if (scope_stack_top().in_disambiguation) goto done;
  /* If writing cross-reference information, write an entry for this
     declaration. */
  if (f_xref_info != NULL) {
    a_symbol_ptr             sym_for_xref = sym_ptr;
    a_symbol_reference_kind  kind_for_xref = kind;
    a_type_ptr               tp;

    if (sym_kind == (a_symbol_kind)sk_type) {
      if (sym_ptr->is_template_param) {
        /* A reference to a template parameter during template instantiation is
           actually a reference to the template argument that it represents. */
        tp = sym_ptr->variant.type.ptr;
        if (tp->kind == (a_type_kind)tk_template_param) {
          /* We are in the midst of a template definition. */
        } else {
          /* We are in the midst of a template instantiation. */
          for(;;) {
            if (tp->kind == (a_type_kind)tk_typeref) {
              if (symbol_for(tp) == NULL) {
                /* Not a "named typeref" (e.g., a type qualifier or a decltype
                   placeholder): Look at the underlying type. */
                tp = tp->variant.typeref.type;
              } else {
                /* A named typeref: Use that for cross-referencing purposes. */
                sym_for_xref = symbol_for(tp);
                kind_for_xref |= SRK_IMPLICIT_TEMPLATE_ARG;
                break;
              }  /* if */
            } else if (is_array_type(tp)) {
              tp = array_element_type(tp);
            } else if (is_any_ptr_or_ref_type(tp)) {
              tp = type_pointed_to(tp);
            } else {
              break;
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
    } else if (is_class_symbol(sym_ptr)) {
      tp = type_symbol_type(sym_ptr);
      if (is_immediate_class_type(tp) && !sym_ptr->is_class_member) {
        a_class_symbol_supplement_ptr  cssp;
        cssp = sym_ptr->variant.class_struct_union.extra_info;
        if (tp->variant.class_struct_union.is_prototype_instantiation) {
          /* A reference to a top-level prototype instantiation (i.e., not a
             nested class) is put out as a reference to the template itself. */
          sym_for_xref = cssp->class_template;
          check_assertion(sym_for_xref != NULL);
        }  /* if */
      }  /* if */
    }  /* if */
    write_xref_entry(kind_for_xref, sym_for_xref, source_position);
  }  /* if */
  if (!sym_ptr->referenced) {
    /* If this is a template function or static data member, record the
       position of the first reference. */
    record_first_use_if_template(sym_ptr, source_position);
  }  /* if */
  /* Set the referenced flag in the symbol. */
  sym_ptr->referenced = TRUE;
  if (specific_il_entry != NULL) {
    scptr = specific_il_entry;
  } else {
    scptr = source_corresp_entry_for_symbol(sym_ptr);
  }  /* if */
  if (update_il_entry && scptr != NULL) {
    /* Set the referenced flag in the associated intermediate language entry,
       if there is one.  Note that more than one symbol can point to the same
       IL entry.  Use the fact that all the IL tables begin with
       a_source_correspondence. */
    /* If the symbol is for a virtual function, do not set the IL referenced
       flag; a reference to the symbol is not necessarily a reference to the
       corresponding IL entry.  When it is, the flag is set explicitly
       elsewhere. */
    if (sym_kind == (a_symbol_kind)sk_member_function &&
        sym_ptr->variant.routine.ptr->is_virtual) {
      /* Do not set IL referenced flag. */
    } else {
      if (sym_kind == (a_symbol_kind)sk_static_data_member &&
          /* Avoid references that aren't "uses" according to the standard,
             e.g., a reference to a constant-valued static data member that
             is immediately converted to a constant is not a "use" that
             should cause instantiation of the static data member. */
          !(kind & SRK_CONST_VALUE_USE) &&
          /* Don't instantiate things in prototype instantiations.  They
             get instantiated if referenced out of a real instantiation. */
          !(kind & SRK_PROTO_INST_REF) &&
          /* Don't instantiate things in default arguments.  They get
             instantiated if the default argument is actually used. */
          !(kind & SRK_DEFAULT_ARG_EXPR) &&
          !(gpp_mode && !clang_mode &&
            !(kind & (SRK_ADDRESS_TAKEN | SRK_USE | SRK_MODIFICATION)))) {
        /* If we are marking a template static data member as referenced, also
           set its instantiation required flag. */
        a_variable_ptr  vp = sym_ptr->variant.static_data_member.variable;
        if (gpp_mode && vp->initializer_in_class &&
            vp->init_kind == initk_none) {
          /* In GNU and Clang C++ modes, in-class static data member
             initializers may be instantiated on demand.  Ensure that the
             initializer is now processed. */
          ensure_inclass_static_member_constant_initializer_is_scanned(vp);
        }  /* if */
        set_instance_required(sym_ptr, TRUE, SIR_DEFER_INLINE);
        vp->used = TRUE;
      }  /* if */
      scptr->referenced = TRUE;
    }  /* if */
  }  /* if */
  if (update_il_entry && (kind & SRK_ADDRESS_TAKEN)) {
    /* Set the address_taken flag in variables and routines.  Also set the
       address_taken flag when referencing a static data member or a field in
       an anonymous union. */
    if (sym_kind == (a_symbol_kind)sk_variable) {
      set_variable_address_taken(sym_ptr->variant.variable.ptr);
    } else if (sym_kind == (a_symbol_kind)sk_static_data_member) {
      set_variable_address_taken(sym_ptr->variant.static_data_member.variable);
    } else if (sym_kind == (a_symbol_kind)sk_routine ||
               sym_kind == (a_symbol_kind)sk_member_function) {
      sym_ptr->variant.routine.ptr->address_taken = TRUE;
    } else if (sym_kind == (a_symbol_kind)sk_field) {
      /* Walk up the list of potentially nested anonymous unions to find an
         anonymous union parent variable, if any. Only in C++. */
      a_symbol_ptr sym_apo = sym_ptr->variant.field.anonymous_parent_object;
      while (sym_apo != NULL) {
        if (sym_apo->kind == (a_symbol_kind)sk_variable) {
          set_variable_address_taken(sym_apo->variant.variable.ptr);
          break;
        } else if (sym_apo->kind != (a_symbol_kind)sk_field) {
          break;
        }  /* if */
        sym_apo = sym_apo->variant.field.anonymous_parent_object;
      }  /* while */
    }  /* if */
  }  /* if */
  if (sym_kind == (a_symbol_kind)sk_variable) {
    /* If this reference is a use or, by taking the variable's address, a
       potential use, mark the variable has having been used.  Note that for
       an error reference, the variable is marked as being both set
       and modified, to keep diagnostics from being issued down the road; nor
       are diagnostics issued here. */
    a_scope_stack_entry_ptr  ssep;
    a_variable_ptr           vp = sym_ptr->variant.variable.ptr;
    if (kind & (SRK_ALL_VARIABLE_USES | SRK_ERROR)) {
      if (vp->used) {
        /* This is not the first use. */
        if (vp->is_parameter || vp->is_handler_param) {
          /* Mark the parameter as multiply used (information that may be
             useful for inlining). */
          vp->param_used_more_than_once = TRUE;
        }  /* if */
      } else {
        /* This is the first use of the variable. */
        if (!(kind & SRK_USE) || suppress_used_before_set_warnings ||
            sym_ptr->value_has_been_set) {
          /* No diagnostic. */
        } else if (!vp->source_corresp.is_local_to_function) {
          /* A block-extern variable or a namespace-scope variable with
             internal linkage -- no diagnostic. */
        } else if (vp->is_struct_binding) {
          /* A structured binding "variable".  This was generated by the front
             end and should always have an initializer (unless an error
             occurred or it's a template dependent type). */
        } else {
          /* Variable's value has not been set yet.  Issue a warning, if
             appropriate. */
          a_boolean suppress_warning = FALSE;

          if (!C_mode()) {
            /* In C++ don't put out a warning if the variable is of class
               type (or array of class type) and the class has no fields. */
            a_type_ptr  tp = vp->type;
            if (is_array_type(tp)) tp = underlying_array_element_type(tp);
            if (is_class_struct_union_type(tp) &&
                !symbol_supplement_for_class(tp)->any_nonstatic_data_members) {
              /* Variable is of empty-class type. */
              suppress_warning = TRUE;
            }  /* if */
          }  /* if */
          if (!suppress_warning) {
            /* To determine whether to suppress the warning, examine the scope
               stack for labels and uncompleted loops that might enable the
               program to set the variable in code that has not yet been seen
               and then to branch back to the current code.  In other words,
               only issue a warning if we're sure the variable cannot have
               been set. */
            /*lint --e{446} ssep modified in loop */
            for (ssep = &scope_stack[decl_scope_level]; ; --ssep) {
              if (ssep == &scope_stack[0]) {
                expect_error();
                suppress_warning = TRUE;
                break;
              }  /* if */
              if (ssep->kind == (a_scope_kind)sck_function) {
                /* A reference to a local variable from within a quasi-nested
                   function definition should have been reported as an error
                   and recorded as an srk_error reference.  Such a reference
                   is allowed for certain uses of static variables, and for
                   some local variables that might be subject to capture in
                   a lambda. */
                check_assertion(ssep->number == sym_ptr->decl_scope ||
                                vp->storage_class ==
                                                 (a_storage_class)sc_static ||
                                (scptr != NULL &&
                                 !scptr->is_local_to_function) ||
                                is_lambda_body_scope(ssep));
                /* We are at the outermost scope of the function.  Check for
                   a label. */
                if (!is_lambda_body_scope(ssep) ||
                    ssep->number == sym_ptr->decl_scope) {
                  goto check_label_decl_seq;
                }  /* if */
              } else if (ssep->number == sym_ptr->decl_scope) {
                /* We are at the scope in which the variable was declared.
                   Jump out to the function scope and look for a label. */
                ssep = &scope_stack[depth_innermost_function_scope];
check_label_decl_seq:
                /* If the variable was declared before the label, suppress the
                   warning.  If it was declared after the label, the warning
                   is appropriate.  For example:
                     void f() {
                       int i;
                         :
                     L:
                       int j;
                       ++i;          // No warning -- i may be set later.
                       ++j;          // Warning -- j cannot have been set yet.
                           :
                     }
                */
                if (ssep->last_label_decl_seq > sym_ptr->decl_seq) {
                  /* Variable was declared before the label was defined. */
                  suppress_warning = TRUE;
                }  /* if */
                break;
              } else if (ssep->is_loop_scope) {
                /* The variable was declared in a scope outside the loop
                   scope, so suppress the warning.  If it were declared within
                   the loop, the warning would still be okay.  For example:
                     void f() {
                       int i;
                         :
                       for (;;) {
                         int j;
                         ++i;        // No warning -- i may be set later.
                         ++j;        // Warning -- j cannot have been set yet.
                           :
                       }
                     }
                */
                suppress_warning = TRUE;
                break;
              }  /* if */
            }  /* for */
          }  /* if */
#if GNU_EXTENSIONS_ALLOWED
          if (!suppress_warning && var_is_gnu_named_register(vp)) {
            suppress_warning = TRUE;
          }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
          if (!suppress_warning) {
            pos_sy_warning(ec_used_before_set, source_position, sym_ptr);
          }  /* if */
        }  /* if */
        vp->used = TRUE;
      }  /* if */
    }  /* if */
    /* If this reference involves a modification or, by taking the variable's
       address, a potential modification, mark the variable as having its
       value set. */
    if (kind & (SRK_ALL_VARIABLE_MODIFICATIONS | SRK_ERROR)) {
      mark_variable_value_set(sym_ptr);
      if (exceptions_enabled) {
        /* If the modification takes place inside a try block and the
           variable was declared in a (function-local) scope that contains
           the try block, then it may have to be treated as quasi-volatile
           -- it may need to be stored immediately in case an exception is
           thrown. */
        if (!vp->source_corresp.is_local_to_function) {
          /* Don't worry about file-scope variables. */
        } else if (vp->modified_within_try_block) {
          /* Flag is already set. */
        } else {
          ssep = &scope_stack[decl_scope_level];
          if (ssep->within_try_block) {
            /* This modification is inside a try block. */
            for (;;) {
              if (ssep->number == sym_ptr->decl_scope) {
                /* Symbol was declared inside the try block.  It's only those
                   declared outside the try block we're interested in. */
                break;
              } else if (ssep->is_try_block) {
                /* We've reached the try block's scope without finding the
                   scope in which the variable was declared.  Set the flag
                   and break out of the loop. */
                vp->modified_within_try_block = TRUE;
                break;
              }  /* if */
              /* Advance up the scope stack. */
              --ssep;
              check_assertion_str2(ssep->within_try_block,
                                   "record_symbol_reference_full:",
                                   "within_try_block not set properly");
            }  /* for */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (kind & (SRK_ALL_REFERENCES | SRK_ERROR)) {
      if (vp->is_inline ||
          (vp->is_template_variable && !vp->is_specialized &&
           !vp->is_nonreal)) {
        /* This is called for inline variables to make sure that we know a
           definition of the inline instance may be needed. */
        set_instance_required(sym_ptr, TRUE, SIR_DEFER_INLINE);
        if (vp->template_info != NULL) {
          /* Mark the template and its prototype instantiation as referenced as
             well. */
          a_template_ptr tplp = vp->template_info->assoc_template;
          a_variable_ptr proto_tplp = tplp->prototype_instantiation.variable;
          symbol_for(tplp)->referenced = TRUE;
          if (update_il_entry) {
            tplp->source_corresp.referenced = TRUE;
          }  /* if */
          if (proto_tplp != NULL) {
            symbol_for(proto_tplp)->referenced = TRUE;
            if (update_il_entry) {
              proto_tplp->source_corresp.referenced = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (sym_kind == (a_symbol_kind)sk_static_data_member) {
    /* If appropriate, record that the static data member's value has been
       set (or "potentially set", if the address has been taken). */
    if (kind & (SRK_ALL_VARIABLE_MODIFICATIONS | SRK_ERROR)) {
      mark_static_data_member_value_set(sym_ptr);
    }  /* if */
  }  /* if */
  if ((gnu_mode || ms_extensions || cpp14_mode || c23_mode) &&
      scptr != NULL &&
      !(sym_kind == (a_symbol_kind)sk_type || is_tag_symbol_kind(sym_kind))) {
    check_use_of_deprecated_or_unavailable_entity(scptr, source_position);
  }  /* if */
  if (is_simple_function_symbol(sym_ptr)) {
    (void)check_use_of_deleted_function(sym_ptr, /*elided_ref=*/FALSE,
                                        source_position);
    if (sym_ptr->variant.routine.pending_mapped_exc_spec) {
      /* The routine has an exception specification that has not been parsed
         yet, and the associated context has been recorded in a map (see
         resolve_pending_mapped_exc_spec). */
      instantiate_exception_spec_if_needed(sym_ptr);
    }  /* if */
  }  /* if */
done:;
}  /* record_symbol_reference_full */


void record_symbol_reference(a_symbol_reference_kind kind,
                             a_symbol_ptr            sym_ptr,
                             a_source_position       *source_position,
                             a_boolean               update_il_entry)
/*
Interface routine for record_symbol_reference_full for the usual case
where specific_il_entry is NULL.
*/
{
  record_symbol_reference_full(kind, sym_ptr, source_position,
                               update_il_entry,
                               (a_source_correspondence *)NULL);
}  /* record_symbol_reference */


void reference_to_invalid_name(a_symbol_locator *locator)
/*
A name was referred to in a declaration but was invalid -- either it was
undefined or it was not being used correctly.  Put out cross reference
information on the reference, if required.
*/
{
  a_symbol_locator  loc;

  if (f_xref_info != NULL) {
    loc = *locator;
    if (loc.specific_symbol == NULL && !is_error_locator(loc)) {
      make_specific_symbol_error_locator(&loc);
    }  /* if */
    write_xref_entry(SRK_ERROR | SRK_REFERENCE, loc.specific_symbol,
                     &loc.source_position);
  }  /* if */
}  /* reference_to_invalid_name */


void record_param_id_list_declarations(a_func_info_block_ptr func_info)
/*
The function associated with func_info has been declared but not defined.
The symbols associated with the parameter declarations should be recorded for
cross referencing and any associated source-sequence entries should be removed
from the list.
*/
{
  a_param_id_ptr  pid = func_info->param_id_list;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr  ss_list = func_info->prototype_scope_ss_list;
  a_source_sequence_entry_ptr  ssep, next_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  /* Update xref information on each symbol. */
  for (; pid != NULL; pid = pid->next) {
    if (pid->symbol != NULL) {
      mark_declared(pid->symbol, &pid->symbol->decl_position);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (pid->source_sequence_entry != NULL) {
      check_assertion(ss_entry_kind(pid->source_sequence_entry) ==
                                             (an_il_entry_kind)iek_none);
      if (ss_list != NULL) {
        ssep = ss_list;
        for (; ssep != pid->source_sequence_entry; ssep = ssep->next) {
          check_assertion(ssep != NULL);
        }  /* for */
        if (ssep == ss_list) {
          ss_list = ssep->next;
        } else {
          ssep->prev->next = ssep->next;
        }  /* if */
        if (ssep->next != NULL) ssep->next->prev = ssep->prev;
        recycle_src_seq_entry(ssep);
      }  /* if */
      pid->source_sequence_entry = NULL;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* for */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  for (ssep = ss_list; ssep != NULL; ssep = next_ssep) {
    next_ssep = ssep->next;
    if (ssep->prev != NULL) ssep->prev->next = next_ssep;
    if (ssep->next != NULL) ssep->next->prev = ssep->prev;
    ssep->next = ssep->prev = NULL;
    add_source_sequence_entry_to_list(ssep);
  }  /* for */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* record_param_id_list_declarations */


void record_using_decl(a_symbol_ptr                sym,
                       a_source_position           *pos,
                       ARG_UNUSED a_using_decl_ptr udp,
                       ARG_UNUSED a_using_decl_ptr prev_udp)
/*
udp points to a using-decl entry created to represent an using declaration
specifying fundamental symbol sym.  If appropriate, update the cross reference
and source sequence output.  prev_udp, if non-NULL, refers to the previously
created using-decl in either a using-enum-declaration or a function overload
set.  *pos is the source position of the identifier in the using-declaration.
*/
{
  /* Update the cross reference file if it exists. */
  if (f_xref_info != NULL) {
    /* If writing cross-reference information, write an entry for this
       declaration. */
    write_xref_entry(SRK_DECLARATION, sym, pos);
  }  /* if */
#if MAINTAIN_CLASS_MEMBER_LIST
  if (prev_udp == NULL) {
    /* Record the declaration if it appears in the body of a class.  A
       using-declaration that names an overload set (or, for "using enum", an
       enumeration) produces several using-decl entries, but they represent a
       single declaration, so only the first one is recorded. */
    record_class_member_declaration((char*)udp, iek_using_decl);
  }  /* if */
#endif /* MAINTAIN_CLASS_MEMBER_LIST */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (!source_sequence_entries_disallowed) {
    if (prev_udp == NULL) {
      add_to_source_sequence_list((char *)udp,
                                  (an_il_entry_kind)iek_using_decl);
    } else {
      prev_udp->next_in_set = udp;
    }  /* if */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* record_using_decl */


static a_boolean is_cfront_base_class_destructor_access_bug(
                                                a_symbol_ptr   sym,
                                                a_routine_ptr  rp,
                                                a_type_ptr     class_of_object)
/*
Cfront has a bug in which a private destructor in a base class can be
called when the derived class really should not have access to it.
This function, which should only be called in cfront mode, detects
the condition in which the access error should be suppressed.
*/
{
  a_boolean  result = FALSE;
  if (rp->special_kind == (a_special_function_kind)sfk_destructor &&
      !same_entities(sym_parent_class(sym), class_of_object) &&
      class_of_object != NULL) {
    result = TRUE;
  }  /* if */
  return result;
}  /* is_cfront_base_class_destructor_access_bug */


void reference_to_implicitly_invoked_function
                                (a_symbol_ptr       sym,
                                 a_source_position  *pos,
                                 a_type_ptr         class_of_object,
                                 a_boolean          honor_virtual,
                                 a_boolean          evaluated,
                                 a_boolean          instantiate,
                                 a_boolean          check_access,
                                 a_boolean          elided_reference,
                                 a_boolean          *error_detected)
/*
sym is points to a symbol for a special member function that is invoked
implicitly -- e.g., a copy constructor that is called when a class
object is passed by value or an assignment operator that is called when
another assignment operator function is being created.  Check that the
special member function is accessible (if check_access is TRUE) and
mark the routine entry referenced.  sym can be a projection symbol.
*pos gives the source position of the reference.  class_of_object
points to the type of the object for which the function is being
called, which is not always the same as the class of which the
function is a member.  This is used to check protected member access
which only applies to objects of a derived class.  class_of_object may
be NULL if protected member access checking is not needed.  Also, if
the routine is compiler generated, it may still need to be defined,
since the definition may have been put off until an actual reference
occurred (e.g., ARM 12.8).  This function deals with implicitly called
constructors, destructors, assignment operators, and conversion
functions.  If honor_virtual is TRUE, and the function is virtual, the
reference is considered to be a virtual call; that means the access
control checking is done, but the IL entry is not marked as
referenced.  If evaluated is FALSE, the reference is within an
unevaluated expression; again, access control checking is done, but
the IL entry is not marked as referenced.  If instantiate is TRUE and
the function is a template function, it should be instantiated.
If elided_reference is TRUE, the reference has been elided.
If error_detected is non-NULL, return *error_detected set to TRUE if
there was an error, and do not issue any diagnostics (including
warnings).
*/
{
  a_symbol_ptr  base_sym = fundamental_symbol_of(sym);
  a_routine_ptr rp = base_sym->variant.routine.ptr;

  check_assertion(rp->special_kind ==
                               (a_special_function_kind)sfk_constructor ||
                  rp->special_kind ==
                               (a_special_function_kind)sfk_destructor ||
                  rp->special_kind ==
                               (a_special_function_kind)sfk_conversion ||
                  (rp->special_kind == (a_special_function_kind)sfk_operator &&
                   rp->variant.opname_kind == 
                               (an_opname_kind)onk_assign));
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode && depth_stmt_stack >= 0 &&
      struct_stmt_stack[depth_stmt_stack].in_handler_parameter_declaration &&
      is_constructor_symbol(base_sym)) {
    /* Don't check access on constructors while processing handler parameters
       in Microsoft mode. */
    check_access = FALSE;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (check_access) {
    /* Check for accessibility. */
    if (!have_access_to_symbol(sym)) {
      an_error_severity  severity = es_discretionary_error;
      /* Normally an error, but in cfront mode there is a special case
         involving a private base class destructor where we issue a warning. */
      if (any_cfront_mode() &&
          is_cfront_base_class_destructor_access_bug(sym, rp,
                                                     class_of_object)) {
        severity = es_warning;
      }  /* if */
      /* Record an access error.  If error_detected is non-NULL, the diagnostic
         will be suppressed. */
      record_access_error(sym, (a_symbol_ptr)NULL, (a_type_ptr)NULL, pos,
                          (a_symbol_locator*)NULL, severity,
                          ec_inaccessible_special_function,
                          error_detected);
    } else if (class_of_object != NULL) {
      /* Protected members of a base class can only be accessed through an
         object of a derived class.  Again, if error_detected is non-NULL,
         any diagnostic will be suppressed.  If sym refers to an inheriting
         constructor, use the underlying constructor symbol for the test. */
      sym = originator_symbol_of(sym);
      (void)check_protected_member_access(sym, sym, pos, class_of_object,
                                          error_detected);
    }  /* if */
  }  /* if */
  /* Update the symbol and the cross-reference listing.  If error_detected
     is non-NULL, we assume that we're just trying to find out if the
     reference is valid, so do not record it. */
  if (error_detected == NULL) {
    record_symbol_reference((SRK_REFERENCE | SRK_IMPLICIT), base_sym, pos,
                            /*update_il_entry=*/FALSE);
  } else if (rp->is_deleted) {
    /* Access checking didn't check for deleted functions. */
    *error_detected = TRUE;
  }  /* if */
  if (scope_stack_top().in_field_initializer &&
      special_kind_is(rp, sfk_constructor) &&
      (rp->compiler_generated || rp->is_defaulted) &&
      is_default_constructor(rp, /*is_declarative_context=*/FALSE)) {
    /* A reference to a defaulted default constructor in a field initializer.
       If the constructor is for a class whose field initializer is being
       parsed, issue an error. */
    a_scope_stack_entry_ptr  ssep = &scope_stack_top();
    a_type_ptr               parent_class = parent_class_of(rp);
    /* Look through the scopes associated with the field initializer to see
       if any corresponds to the class type of the defaulted constructor. */
    do {
      if ((scope_is(ssep, sck_class_reactivation) ||
           scope_is(ssep, sck_class_struct_union)) &&
          same_entities(ssep->assoc_type, parent_class)) {
        if (error_detected != NULL) {
          *error_detected = TRUE;
        } else {
          pos_ty_error(
                   ec_generated_default_constructor_used_in_field_initializer,
                   pos, parent_class);
        }  /* if */
        /* Generating the definition of the default constructor is not possible
           since it would be self-referential.  Treat the call as not evaluated
           to avoid problems below. */
        evaluated = FALSE;
        break;
      }  /* if */
      ssep = &scope_stack[ssep->previous_scope];
    } while (ssep->in_field_initializer);
  }  /* if */
  if (!evaluated || error_detected != NULL) {
    /* Unevaluated expression.  Do not set the IL referenced flag. */
  } else if (rp->is_virtual && honor_virtual) {
    /* Virtual function call.  Do not set the IL referenced flag because the
       call might actually be of an overriding function. */
  } else {
    /* Non-virtual call. */
    mark_routine_referenced_full(rp, instantiate, elided_reference);
  }  /* if */
}  /* reference_to_implicitly_invoked_function */


a_boolean reference_to_trivial_default_constructor(
                                            a_type_ptr         class_type,
                                            a_type_ptr         access_class,
                                            a_source_position  *pos,
                                            a_boolean          check_access,
                                            a_boolean          *error_detected)
/*
If class_type has an associated trivial default constructor, record a
reference to it -- checking its accessibility (if check_access is TRUE),
updating the cross-reference listing if appropriate, and assuring that
it is defined, which is done (even though the function is not actually
called) in case the definition has side effects.  If class_type does
have a trivial default constructor representation return TRUE.
If error_detected is non-NULL, return *error_detected set to TRUE if
there was an error, and do not issue any diagnostics (including warnings).
The access check is done using access_class as the class in which the
reference is done.
*/
{
  a_symbol_ptr   ctor_sym;

  check_assertion(is_class_struct_union_type(class_type));
  ctor_sym = symbol_supplement_for_class(class_type)->
                                            trivial_default_constructor;
  if (ctor_sym != NULL) {
    reference_to_implicitly_invoked_function(ctor_sym, pos, access_class,
                                             /*honor_virtual=*/FALSE,
                                             /*evaluated=*/TRUE,
                                             /*instantiate=*/TRUE,
                                             check_access,
                                             /*elided_reference=*/FALSE,
                                             error_detected);
  }  /* if */
  return (ctor_sym != NULL);
}  /* reference_to_trivial_default_constructor */


void reference_to_trivial_copy_constructor(a_type_ptr        class_type,
                                           a_type_ptr        access_class,
                                           a_source_position *pos,
                                           a_boolean         check_access,
                                           a_boolean         elided_reference,
                                           a_boolean         *error_detected)
/*
Record a reference to the trivial copy constructor of class_type
at position pos.  Check accessibility (if check_access is TRUE), and
record a cross-reference entry if appropriate.  A trivial copy
constructor is usually compiler generated and public, so no access
check is needed.  However, with defaulted and deleted functions, it is
possible to have a user-declared defaulted trivial copy constructor
that is nonpublic.  If elided_reference is TRUE, the reference to the
copy constructor has been elided.  If error_detected is non-NULL, return
*error_detected set to TRUE if there was an error, and do not issue
any diagnostics (including warnings).  The access check is done using
access_class as the class in which the reference is done.
*/
{
  a_class_symbol_supplement_ptr cssp;

  class_type = skip_typerefs(class_type);
  cssp = symbol_supplement_for_class(class_type);
  if (cssp->constructor != NULL) {
    a_boolean    overloaded_ctors = FALSE;
    a_symbol_ptr sym = cssp->constructor;
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      overloaded_ctors = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    }  /* if */
    for (; sym != NULL; sym = (overloaded_ctors ? sym->next : NULL)) {
      if (is_simple_function_symbol(sym) &&
          sym->variant.routine.ptr->is_trivial_copy_function) {
        /* Found a trivial copy constructor. */
        reference_to_implicitly_invoked_function(sym, pos, access_class,
                                                 /*honor_virtual=*/FALSE,
                                                 /*evaluated=*/FALSE,
                                                 /*instantiate=*/FALSE,
                                                 check_access,
                                                 elided_reference,
                                                 error_detected);
        break;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* reference_to_trivial_copy_constructor */


void symbol_ref_one_time_init(void)
/*
One-time initialization for symbol_ref.c static variables.
*/
{
  output_control_block_has_been_set_up = FALSE;
}  /* symbol_ref_one_time_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE


